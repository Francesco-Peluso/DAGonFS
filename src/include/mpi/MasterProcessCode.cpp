//
// Created by frank on 12/30/24.
//

#include "MasterProcessCode.hpp"

#include <iostream>
#include <cstring>
#include <unistd.h>

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

	unsigned int numberOfBlocks = fileSize / FILE_SYSTEM_SINGLE_BLOCK_SIZE + (fileSize % FILE_SYSTEM_SINGLE_BLOCK_SIZE > 0);
	unsigned int blockPerProcess = numberOfBlocks / mpi_world_size;
	unsigned int remainingBlocks = numberOfBlocks % mpi_world_size;

	//Data for Scatter
	int *scatterCounts = new int[mpi_world_size];
	int *scatterDispls = new int[mpi_world_size];
	int scatterOffset = 0;
	for (int i=0; i<mpi_world_size; i++) {
		//Calculating elements
		scatterCounts[i] = ( blockPerProcess + (i < remainingBlocks) ) * FILE_SYSTEM_SINGLE_BLOCK_SIZE;

		//Calculating displacements
		scatterDispls[i] = scatterOffset;
		scatterOffset += scatterCounts[i];
	}

	//Data for gather
	PointerPacket *addresses = new PointerPacket[numberOfBlocks];
	int *gatherCounts = new int[mpi_world_size];
	int *gatherDispls = new int[mpi_world_size];
	int gatherOffset = 0;
	for (int i=0; i< mpi_world_size; i++) {
		//Calculating elements
		gatherCounts[i] = ( blockPerProcess + (i < remainingBlocks) ) * sizeof(PointerPacket);

		//Calculating displacements
		gatherDispls[i] = gatherOffset;
		gatherOffset += gatherCounts[i];
	}

	//In this code the rank is always 0 due to the fact that this code it's executed only by the master
	void *localScatBuf = malloc(scatterCounts[rank]);
	MPI_Scatterv(buffer, scatterCounts, scatterDispls, MPI_BYTE, localScatBuf, scatterCounts[rank], MPI_BYTE, 0, MPI_COMM_WORLD);

	unsigned int effectiveBlocks = blockPerProcess + (rank < remainingBlocks);
	PointerPacket *localGathBuf = new PointerPacket[effectiveBlocks];
	for (int i=0; i < effectiveBlocks; i++) {
		void *data_p = malloc(FILE_SYSTEM_SINGLE_BLOCK_SIZE);
		memcpy(data_p, localScatBuf + i*FILE_SYSTEM_SINGLE_BLOCK_SIZE, FILE_SYSTEM_SINGLE_BLOCK_SIZE);
		localGathBuf[i].address = data_p;
	}

	MPI_Gatherv(localGathBuf, gatherCounts[rank], MPI_BYTE, addresses, gatherCounts, gatherDispls, MPI_BYTE, 0, MPI_COMM_WORLD);


	//Saving pointers for later reading
	Blocks *blocks = Blocks::getInstance();
	if (!blocks->blockListExistForInode(inode)) {
		blocks->createEmptyBlockListForInode(inode);
	}
	vector<DataBlock *> &inodeBlockList = blocks->getDataBlockListOfInode(inode);
	int additionBlocks = numberOfBlocks - inodeBlockList.size();
	LOG4CPLUS_INFO(MasterProcessLogger, MasterProcessLogger.getName() << "Current block list size: " << inodeBlockList.size() );
	LOG4CPLUS_INFO(MasterProcessLogger, MasterProcessLogger.getName() << "Number of blocks: " << numberOfBlocks);
	LOG4CPLUS_INFO(MasterProcessLogger, MasterProcessLogger.getName() << "Additional block: " << additionBlocks);
	if (additionBlocks > 0) {
		dataBlockManager->addDataBlocksTo(inodeBlockList, additionBlocks, inode);
		LOG4CPLUS_INFO(MasterProcessLogger, MasterProcessLogger.getName() << "New block list size: " << inodeBlockList.size());
	}

	for (int i=0; i < numberOfBlocks; i++) {
		DataBlock *dataBlock = inodeBlockList[i];
		dataBlock->setData(addresses[i].address);
	}

	delete[] scatterCounts;
	delete[] scatterDispls;
	delete[] gatherCounts;
	delete[] gatherDispls;
	delete[] localScatBuf;
	delete[] localGathBuf;
	delete[] addresses;

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

	unsigned int blockPerProcess = numberOfBlocksForRequest / mpi_world_size;
	unsigned int remainingBlocks = numberOfBlocksForRequest % mpi_world_size;
	unsigned int effectiveBlocks = numberOfBlocksForRequest / mpi_world_size + (rank < remainingBlocks);
	PointerPacket *addressesToScat = new PointerPacket[numberOfBlocksForRequest];
	Blocks *blocks = Blocks::getInstance();
	vector<DataBlock *> &dataBlockList = blocks->getDataBlockListOfInode(inode);
	for (int i=0; i< numberOfBlocksForRequest; i++) {
		addressesToScat[i].address = dataBlockList[i]->getData();
	}
	int *scatterCounts = new int[mpi_world_size];
	int *scatterDispls = new int[mpi_world_size];
	int scatterOffset = 0;
	for (int i=0; i < mpi_world_size; i++) {
		scatterCounts[i] = ( blockPerProcess + (i < remainingBlocks) )* sizeof(PointerPacket);
		scatterDispls[i] = scatterOffset;
		scatterOffset += scatterCounts[i];
	}

	int *gatherCounts = new int[mpi_world_size];
	int *gatherDispls = new int[mpi_world_size];
	int gatherOffset = 0;
	for (int i=0; i < mpi_world_size; i++) {
		gatherCounts[i] = ( blockPerProcess + (i < remainingBlocks) )* FILE_SYSTEM_SINGLE_BLOCK_SIZE;
		gatherDispls[i] = gatherOffset;
		gatherOffset += gatherCounts[i];
	}

	MPI_Scatterv(addressesToScat, scatterCounts, scatterDispls, MPI_BYTE, MPI_IN_PLACE, scatterDispls[rank], MPI_BYTE, 0, MPI_COMM_WORLD);
	void *localGathBuf = malloc(effectiveBlocks*FILE_SYSTEM_SINGLE_BLOCK_SIZE);
	for (int i=0;i < effectiveBlocks; i++) {
		memcpy(localGathBuf+i*FILE_SYSTEM_SINGLE_BLOCK_SIZE,addressesToScat[i].address, FILE_SYSTEM_SINGLE_BLOCK_SIZE);
	}
	MPI_Gatherv(localGathBuf, gatherCounts[rank], MPI_BYTE, readBuff,gatherCounts, gatherDispls, MPI_BYTE, 0, MPI_COMM_WORLD);

	delete[] scatterCounts;
	delete[] scatterDispls;
	delete[] gatherCounts;
	delete[] gatherDispls;
	delete[] localGathBuf;

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
		cout << "Master - mkdir ./DAGonFS_dump failed" << endl;
		return;
	}
	if (mkdir("/tmp/DAGonFS_dump/master", 0777) < 0) {
		cout << "Master - mkdir ./DAGonFS_dump/" << rank << " failed" << endl;
		return;
	}
	if (chdir("/tmp/DAGonFS_dump/master") < 0) {
		cout << "Master - cd ./DAGonFS_dump/master" << rank << " failed" << endl;
		return;
	}
	/*
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
	*/
}
