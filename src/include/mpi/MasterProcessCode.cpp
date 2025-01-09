//
// Created by frank on 12/30/24.
//

#include "MasterProcessCode.hpp"

#include <iostream>
#include <cstring>

#include <mpi.h>
#include "mpi_data.hpp"

#include "../blocks/Blocks.hpp"

using namespace std;

using namespace log4cplus;

MasterProcessCode *MasterProcessCode::instance = nullptr;

MasterProcessCode *MasterProcessCode::getInstance(int rank, int mpi_world_size) {
	if (instance == nullptr) {
		instance = new MasterProcessCode(rank, mpi_world_size);
	}

	return instance;
}

MasterProcessCode::MasterProcessCode(int rank, int mpi_world_size) {
	this->rank = rank;
	this->mpi_world_size = mpi_world_size;
	dataBlockManager = DataBlockManager::getInstance(mpi_world_size);
	MasterProcessLogger = Logger::getInstance("MasterProcess.logger - ");
	LogLevel ll = DAGONFS_LOG_LEVEL;
	MasterProcessLogger.setLogLevel(ll);
}


void MasterProcessCode::DAGonFS_Write(void* buffer, fuse_ino_t inode, size_t fileSize) {
	LOG4CPLUS_TRACE(MasterProcessLogger, MasterProcessLogger.getName() << "Invoked DAGonFS_Write()");
	IORequestPacket ioRequest;
	ioRequest.inode = inode;
	ioRequest.fileSize = fileSize;
	MPI_Bcast(&ioRequest, sizeof(ioRequest), MPI_BYTE, 0, MPI_COMM_WORLD);

	Blocks *blocks = Blocks::getInstance();
	if (!blocks->blockListExistForInode(inode)) {
		blocks->createEmptyBlockListForInode(inode);
	}
	vector<DataBlock *> &inodeBlockList = blocks->getDataBlockListOfInode(inode);

	unsigned int numberOfBlocks = fileSize / FILE_SYSTEM_SINGLE_BLOCK_SIZE + (fileSize % FILE_SYSTEM_SINGLE_BLOCK_SIZE > 0);
	int additionBlocks = numberOfBlocks - inodeBlockList.size();
	LOG4CPLUS_INFO(MasterProcessLogger, MasterProcessLogger.getName() << "Current block list size: " << inodeBlockList.size() );
	LOG4CPLUS_INFO(MasterProcessLogger, MasterProcessLogger.getName() << "Number of blocks: " << numberOfBlocks);
	LOG4CPLUS_INFO(MasterProcessLogger, MasterProcessLogger.getName() << "Additional block: " << additionBlocks);
	if (additionBlocks > 0) {
		dataBlockManager->addDataBlocksTo(inodeBlockList, additionBlocks, inode);
		LOG4CPLUS_INFO(MasterProcessLogger, MasterProcessLogger.getName() << "New block list size: " << inodeBlockList.size());
	}

	//Calcolo dimensioni
	unsigned int blocksPerProcess = numberOfBlocks / (mpi_world_size);
	LOG4CPLUS_INFO(MasterProcessLogger, MasterProcessLogger.getName() << "Blocks per process: " << blocksPerProcess);

	//Buffer per Scatter dei dati
	void *dataScatterBuffer = malloc(FILE_SYSTEM_SINGLE_BLOCK_SIZE * mpi_world_size);
	//Buffer per Gather dei puntatori
	PointerPacket *addressesGatherBuffer = (PointerPacket *) malloc(sizeof(PointerPacket)*mpi_world_size);

	//Distribuzione dati
	int progressive_i = 0;
	for (int i=0; i < blocksPerProcess; i++) {
		//Buffer da scatterare
		memcpy(dataScatterBuffer,
				buffer + i*FILE_SYSTEM_SINGLE_BLOCK_SIZE*(mpi_world_size),
				FILE_SYSTEM_SINGLE_BLOCK_SIZE*(mpi_world_size));

		void *masterLocalScatBuf = malloc(FILE_SYSTEM_SINGLE_BLOCK_SIZE);
		MPI_Scatter(dataScatterBuffer, FILE_SYSTEM_SINGLE_BLOCK_SIZE, MPI_BYTE,
					masterLocalScatBuf, FILE_SYSTEM_SINGLE_BLOCK_SIZE, MPI_BYTE,
					0, MPI_COMM_WORLD);

		//Gather puntatori

		MPI_Gather(MPI_IN_PLACE, sizeof(PointerPacket), MPI_BYTE,
					addressesGatherBuffer, sizeof(PointerPacket), MPI_BYTE,
					0, MPI_COMM_WORLD);
		addressesGatherBuffer[0].address = masterLocalScatBuf;

		for (int j=0; j < mpi_world_size; j++) {
			PointerPacket singleAddress = addressesGatherBuffer[j];
			//LOG4CPLUS_INFO(MasterProcessLogger, MasterProcessLogger.getName() << "Address received from P" << j << ": " << singleAddress.address);
			inodeBlockList[progressive_i++]->setData(singleAddress.address);
		}
	}

	free(dataScatterBuffer);
	free(addressesGatherBuffer);

	//Gestione dei blocchi avanzati
	unsigned int remainingBlocks = numberOfBlocks % (mpi_world_size);
	LOG4CPLUS_INFO(MasterProcessLogger, MasterProcessLogger.getName() << "Remaining blocks=" << remainingBlocks);
	if (remainingBlocks > 0) {
		//void *dataSendBuffer = malloc(FILE_SYSTEM_SINGLE_BLOCK_SIZE);
		unsigned int sentBytes = FILE_SYSTEM_SINGLE_BLOCK_SIZE * blocksPerProcess * (mpi_world_size);

		void *master = malloc(FILE_SYSTEM_SINGLE_BLOCK_SIZE);
		memcpy(master, buffer + sentBytes, FILE_SYSTEM_SINGLE_BLOCK_SIZE);
		inodeBlockList[progressive_i++]->setData(master);
		sentBytes += FILE_SYSTEM_SINGLE_BLOCK_SIZE;

		for (int process_i=1; process_i < remainingBlocks; process_i++) {
			PointerPacket receivedAddress;
			//memcpy(dataSendBuffer, buffer + scatteredBytes, FILE_SYSTEM_SINGLE_BLOCK_SIZE);
			MPI_Send(buffer + sentBytes, FILE_SYSTEM_SINGLE_BLOCK_SIZE, MPI_BYTE, process_i, 0, MPI_COMM_WORLD);

			MPI_Status status;
			MPI_Recv(&receivedAddress, sizeof(PointerPacket), MPI_BYTE, process_i, 0, MPI_COMM_WORLD, &status);
			//LOG4CPLUS_INFO(MasterProcessLogger, MasterProcessLogger.getName() << "Address received from P" << process_i << ": " << receivedAddress.address);

			inodeBlockList[progressive_i++]->setData(receivedAddress.address);

			sentBytes += FILE_SYSTEM_SINGLE_BLOCK_SIZE;
		}

		//free(dataSendBuffer);
	}

	LOG4CPLUS_TRACE(MasterProcessLogger, MasterProcessLogger.getName() << "DAGonFS_Write() completed!");
}

void *MasterProcessCode::DAGonFS_Read(fuse_ino_t inode, size_t fileSize, size_t reqSize, off_t offset) {
	LOG4CPLUS_TRACE(MasterProcessLogger, MasterProcessLogger.getName() << "Invoked DAGonFS_Read()");
	LOG4CPLUS_TRACE(MasterProcessLogger, MasterProcessLogger.getName() << "\tRead request size="<<reqSize<<", file size="<<fileSize<<", starting offset="<<offset);

	IORequestPacket ioRequest;
	ioRequest.inode = inode;
	ioRequest.fileSize = fileSize;
	ioRequest.reqSize = reqSize;
	ioRequest.offset = offset;
	MPI_Bcast(&ioRequest, sizeof(ioRequest), MPI_BYTE, 0, MPI_COMM_WORLD);

	if (fileSize == 0)
		return nullptr;

	size_t numberOfBlocksForRequest;
	if (reqSize > fileSize)
		numberOfBlocksForRequest = fileSize / FILE_SYSTEM_SINGLE_BLOCK_SIZE + (fileSize % FILE_SYSTEM_SINGLE_BLOCK_SIZE > 0);
	else
		numberOfBlocksForRequest = reqSize / FILE_SYSTEM_SINGLE_BLOCK_SIZE + (reqSize % FILE_SYSTEM_SINGLE_BLOCK_SIZE > 0);

	LOG4CPLUS_DEBUG(MasterProcessLogger, MasterProcessLogger.getName() << "numberOfBlocksForRequest="<<numberOfBlocksForRequest);
	void *readBuff = malloc(numberOfBlocksForRequest * FILE_SYSTEM_SINGLE_BLOCK_SIZE);
	if (readBuff == nullptr) {
		LOG4CPLUS_ERROR(MasterProcessLogger, MasterProcessLogger.getName() << "readBuff points to NULL, abort");
		abort();
	}

	Blocks *blocks = Blocks::getInstance();
	vector<DataBlock *> &dataBlockList = blocks->getDataBlockListOfInode(inode);
	unsigned long int blockPerProcess = numberOfBlocksForRequest / mpi_world_size;
	PointerPacket *readScatAddresses = (PointerPacket *) malloc(sizeof(PointerPacket) * mpi_world_size);
	int progressive_i = 0;
	for (int i=0; i < blockPerProcess; i++) {
		for (int j=0; j < mpi_world_size; j++) {
			readScatAddresses[j].address = dataBlockList[progressive_i++]->getData();
		}
		MPI_Scatter(readScatAddresses, sizeof(PointerPacket), MPI_BYTE,
					MPI_IN_PLACE, sizeof(PointerPacket), MPI_BYTE,
					0, MPI_COMM_WORLD);
		MPI_Gather(readScatAddresses[0].address, FILE_SYSTEM_SINGLE_BLOCK_SIZE, MPI_BYTE,
					readBuff + i*FILE_SYSTEM_SINGLE_BLOCK_SIZE*mpi_world_size, FILE_SYSTEM_SINGLE_BLOCK_SIZE, MPI_BYTE,
					0, MPI_COMM_WORLD);
	}

	free(readScatAddresses);

	unsigned long int remainingBlocks = numberOfBlocksForRequest % mpi_world_size;
	if (remainingBlocks > 0) {
		unsigned long int sentBytes = FILE_SYSTEM_SINGLE_BLOCK_SIZE * blockPerProcess * (mpi_world_size);
		PointerPacket singleAddress;
		for (int i=0; i < remainingBlocks; i++) {
			DataBlock *dataBlock = dataBlockList[progressive_i++];
			singleAddress.address = dataBlock->getData();
			if (dataBlock->getRank() == 0) {
				memcpy(readBuff + sentBytes, singleAddress.address, FILE_SYSTEM_SINGLE_BLOCK_SIZE);
			}
			else {
				MPI_Send(&singleAddress, sizeof(PointerPacket), MPI_BYTE, dataBlock->getRank(), 0, MPI_COMM_WORLD);
				MPI_Status status;
				MPI_Recv(readBuff + sentBytes, FILE_SYSTEM_SINGLE_BLOCK_SIZE, MPI_BYTE, dataBlock->getRank(), 0, MPI_COMM_WORLD, &status);
			}

			sentBytes += FILE_SYSTEM_SINGLE_BLOCK_SIZE;
		}
	}
	/*
	void *receivedDataBuf = malloc(FILE_SYSTEM_SINGLE_BLOCK_SIZE);
	MPI_Status status;
	PointerPacket readAddress;
	int blockIndex = offset / FILE_SYSTEM_SINGLE_BLOCK_SIZE;
	for (int i = 0; i < numberOfBlocksForRequest; i++, blockIndex++) {
		DataBlock *block = dataBlockList[blockIndex];
		readAddress.address = block->getData();
		//LOG4CPLUS_INFO(MasterProcessLogger, MasterProcessLogger.getName() << "Sending to P" << block->getRank() << " the address: " << readAddress.address << "(=readAddress)(block->getData()="<<block->getData());
		MPI_Send(&readAddress, sizeof(PointerPacket), MPI_BYTE, block->getRank(), 0, MPI_COMM_WORLD);
		MPI_Recv(receivedDataBuf, FILE_SYSTEM_SINGLE_BLOCK_SIZE, MPI_BYTE, block->getRank(), 0, MPI_COMM_WORLD, &status);
		//LOG4CPLUS_INFO(MasterProcessLogger, MasterProcessLogger.getName() << "Partial reading from "<<readAddress.address <<": '" << (char *) receivedDataBuf << "'");
		memcpy(readBuff + i*FILE_SYSTEM_SINGLE_BLOCK_SIZE, receivedDataBuf, FILE_SYSTEM_SINGLE_BLOCK_SIZE);
	}

	LOG4CPLUS_TRACE(MasterProcessLogger, MasterProcessLogger.getName() << "DAGonFS_Read() completed!");

	free(receivedDataBuf);

	*/

	return readBuff;
}

MasterProcessCode::~MasterProcessCode() {

}

void MasterProcessCode::sendWriteRequest() {
	RequestPacket request;
	request.type = WRITE;
	MPI_Bcast(&request, sizeof(RequestPacket), MPI_BYTE, 0, MPI_COMM_WORLD);
}

void MasterProcessCode::sendReadRequest() {
	RequestPacket request;
	request.type = READ;
	MPI_Bcast(&request, sizeof(RequestPacket), MPI_BYTE, 0, MPI_COMM_WORLD);
}

void MasterProcessCode::sendTermination() {
	RequestPacket request;
	request.type = TERMINATE;
	MPI_Bcast(&request, sizeof(RequestPacket), MPI_BYTE, 0, MPI_COMM_WORLD);
}

void MasterProcessCode::sendChangedir() {
	RequestPacket request;
	request.type = CHANGE_DIR;
	MPI_Bcast(&request, sizeof(RequestPacket), MPI_BYTE, 0, MPI_COMM_WORLD);
}

void MasterProcessCode::createFileDump() {
	if (mkdir("/tmp/DAGonFS_dump", 0777) < 0) {
		cout << "Master - mkdir /tmp/DAGonFS_dump failed" << endl;
		return;
	}
	if (mkdir("/tmp/DAGonFS_dump/master", 0777) < 0) {
		cout << "Master - mkdir /tmp/DAGonFS_dump/" << rank << " failed" << endl;
		return;
	}
	if (chdir("/tmp/DAGonFS_dump/master") < 0) {
		cout << "Master - mkdir /tmp/DAGonFS_dump/" << rank << " failed" << endl;
		return;
	}

	Blocks *blocks = Blocks::getInstance();

	for (auto &inode: blocks->getAll()) {
		cout << "Master - Creating dump for inode=" << inode.first << endl;
		string file_name_path="./";
		file_name_path+=to_string(inode.first);
		file_name_path+="-";
		for (auto &block: inode.second) {
			if (block->getRank() == 0) {
				string file_name = file_name_path.c_str();
				ostringstream get_the_address;
				get_the_address << block->getData();
				file_name +=  get_the_address.str();

				cout << "Master - Creating file " << file_name << endl;
				FILE *file_tmp = fopen(file_name.c_str(), "w");
				fwrite(block->getData(), 1, FILE_SYSTEM_SINGLE_BLOCK_SIZE, file_tmp);
				fclose(file_tmp);
			}
		}
	}
}
