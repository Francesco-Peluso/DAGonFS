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
	unsigned int blocksPerProcess = numberOfBlocks / (mpi_world_size - 1);
	LOG4CPLUS_INFO(MasterProcessLogger, MasterProcessLogger.getName() << "Blocks per process: " << blocksPerProcess);

	//Buffer per Scatter dei dati
	void *dataScatterBuffer = malloc(FILE_SYSTEM_SINGLE_BLOCK_SIZE * mpi_world_size);
	//Buffer per Gather dei puntatori
	PointerPacket *addressesGatherBuffer = (PointerPacket *) malloc(sizeof(PointerPacket)*mpi_world_size);

	//Distribuzione dati
	int progressive_i = 0;
	for (int i=0; i < blocksPerProcess; i++) {
		//Scatter dati
		memcpy(dataScatterBuffer + FILE_SYSTEM_SINGLE_BLOCK_SIZE,
				buffer + i*FILE_SYSTEM_SINGLE_BLOCK_SIZE*(mpi_world_size - 1),
				FILE_SYSTEM_SINGLE_BLOCK_SIZE*(mpi_world_size - 1));
		MPI_Scatter(dataScatterBuffer, FILE_SYSTEM_SINGLE_BLOCK_SIZE, MPI_BYTE,
					MPI_IN_PLACE, FILE_SYSTEM_SINGLE_BLOCK_SIZE, MPI_BYTE,
					0, MPI_COMM_WORLD);

		//Gather puntatori
		MPI_Gather(MPI_IN_PLACE, sizeof(PointerPacket), MPI_BYTE,
					addressesGatherBuffer, sizeof(PointerPacket), MPI_BYTE,
					0, MPI_COMM_WORLD);
		for (int j=1; j < mpi_world_size; j++) {
			PointerPacket singleAddress = addressesGatherBuffer[j];
			LOG4CPLUS_INFO(MasterProcessLogger, MasterProcessLogger.getName() << "Address received from P" << j << ": " << singleAddress.address);
			inodeBlockList[progressive_i++]->setData(singleAddress.address);
		}
	}

	free(dataScatterBuffer);
	free(addressesGatherBuffer);

	//Gestione di blocchi parzialmente riempiti
	unsigned int remainingBlocks = numberOfBlocks % (mpi_world_size - 1);
	LOG4CPLUS_INFO(MasterProcessLogger, MasterProcessLogger.getName() << "Remaining blocks=" << remainingBlocks);
	if (remainingBlocks > 0) {
		void *dataSendBuffer = malloc(FILE_SYSTEM_SINGLE_BLOCK_SIZE);
		PointerPacket receivedAddress;
		unsigned int scatteredBytes = FILE_SYSTEM_SINGLE_BLOCK_SIZE * blocksPerProcess * (mpi_world_size - 1);
		for (int i=0,process_i=1; i < remainingBlocks; i++,process_i++) {
			memcpy(dataSendBuffer, buffer + scatteredBytes, FILE_SYSTEM_SINGLE_BLOCK_SIZE);
			MPI_Send(dataSendBuffer, FILE_SYSTEM_SINGLE_BLOCK_SIZE, MPI_BYTE, process_i, 0, MPI_COMM_WORLD);

			MPI_Status status;
			MPI_Recv(&receivedAddress, sizeof(PointerPacket), MPI_BYTE, process_i, 0, MPI_COMM_WORLD, &status);
			LOG4CPLUS_INFO(MasterProcessLogger, MasterProcessLogger.getName() << "Address received from P" << process_i << ": " << receivedAddress.address);

			inodeBlockList[progressive_i++]->setData(receivedAddress.address);

			scatteredBytes += FILE_SYSTEM_SINGLE_BLOCK_SIZE;
		}

		free(dataSendBuffer);
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
	void *receivedDataBuf = malloc(FILE_SYSTEM_SINGLE_BLOCK_SIZE);
	MPI_Status status;
	PointerPacket readAddress;
	int blockIndex = offset / FILE_SYSTEM_SINGLE_BLOCK_SIZE;
	for (int i = 0; i < numberOfBlocksForRequest; i++, blockIndex++) {
		DataBlock *block = dataBlockList[blockIndex];
		readAddress.address = block->getData();
		LOG4CPLUS_INFO(MasterProcessLogger, MasterProcessLogger.getName() << "Sending to P" << block->getRank() << " the address: " << readAddress.address << "(=readAddress)(block->getData()="<<block->getData());
		MPI_Send(&readAddress, sizeof(PointerPacket), MPI_BYTE, block->getRank(), 0, MPI_COMM_WORLD);
		MPI_Recv(receivedDataBuf, FILE_SYSTEM_SINGLE_BLOCK_SIZE, MPI_BYTE, block->getRank(), 0, MPI_COMM_WORLD, &status);
		//LOG4CPLUS_INFO(MasterProcessLogger, MasterProcessLogger.getName() << "Partial reading from "<<readAddress.address <<": '" << (char *) receivedDataBuf << "'");
		memcpy(readBuff + i*FILE_SYSTEM_SINGLE_BLOCK_SIZE, receivedDataBuf, FILE_SYSTEM_SINGLE_BLOCK_SIZE);
	}

	LOG4CPLUS_TRACE(MasterProcessLogger, MasterProcessLogger.getName() << "DAGonFS_Read() completed!");

	free(receivedDataBuf);

	return readBuff;
}

MasterProcessCode::~MasterProcessCode() {

}

void MasterProcessCode::sendWriteRequest() {
	RequestPacket request;
	request.type = WRITE;
	MPI_Bcast(&request, sizeof(request), MPI_BYTE, 0, MPI_COMM_WORLD);
}

void MasterProcessCode::sendReadRequest() {
	RequestPacket request;
	request.type = READ;
	MPI_Bcast(&request, sizeof(request), MPI_BYTE, 0, MPI_COMM_WORLD);
}

void MasterProcessCode::sendTermination() {
	RequestPacket request;
	request.type = TERMINATE;
	MPI_Bcast(&request, sizeof(request), MPI_BYTE, 0, MPI_COMM_WORLD);
}

void MasterProcessCode::sendChangedir() {
	RequestPacket request;
	request.type = CHANGE_DIR;
	MPI_Bcast(&request, sizeof(request), MPI_BYTE, 0, MPI_COMM_WORLD);
}
