//
// Created by frank on 12/30/24.
//

#include "NodeProcessCode.hpp"

#include <iostream>
#include <unistd.h>
#include <dirent.h>
#include <cstring>

#include <mpi.h>
#include "mpi_data.hpp"
#include "../blocks/Blocks.hpp"

using namespace std;

using namespace log4cplus;

NodeProcessCode *NodeProcessCode::instance = nullptr;

NodeProcessCode *NodeProcessCode::getInstance(int rank, int mpi_world_size) {
	if (instance == nullptr) {
		instance = new NodeProcessCode(rank, mpi_world_size);
	}

	return instance;
}

NodeProcessCode::NodeProcessCode(int rank, int mpi_world_size) {
	this->rank = rank;
	this->mpi_world_size = mpi_world_size;
	dataBlockPointers = map<fuse_ino_t, vector<DataBlock *> >();
	dataBlockManager = DataBlockManager::getInstance(mpi_world_size);
	LogLevel ll = DAGONFS_LOG_LEVEL;
	NodeProcessLogger = Logger::getInstance("NodeProcess.logger ");
	NodeProcessLogger.setLogLevel(ll);
}

NodeProcessCode::~NodeProcessCode() {

}

void NodeProcessCode::start() {
	bool running = true;

	while (running) {
		LOG4CPLUS_TRACE(NodeProcessLogger, NodeProcessLogger.getName() << "Process " << rank << " - Waiting for a request..." );
		RequestPacket request;
		IORequestPacket ioRequest;
		MPI_Bcast(&request, sizeof(request), MPI_BYTE, 0, MPI_COMM_WORLD);
		switch (request.type) {
			case WRITE:
				LOG4CPLUS_TRACE(NodeProcessLogger, NodeProcessLogger.getName() << "Process " << rank << " - Recived WRITE request");
				MPI_Bcast(&ioRequest, sizeof(ioRequest), MPI_BYTE, 0, MPI_COMM_WORLD);
				DAGonFS_Write(nullptr,ioRequest.inode,ioRequest.fileSize);
				break;
			case READ:
				LOG4CPLUS_TRACE(NodeProcessLogger, NodeProcessLogger.getName() << "Process " << rank << " - Recived READ request");
				MPI_Bcast(&ioRequest, sizeof(ioRequest), MPI_BYTE, 0, MPI_COMM_WORLD);
				DAGonFS_Read(ioRequest.inode,ioRequest.fileSize, ioRequest.reqSize, ioRequest.offset);
				break;
			case TERMINATE:
				LOG4CPLUS_TRACE(NodeProcessLogger, NodeProcessLogger.getName() << "Process " << rank << " - Recived TERMINATION request");
				running = false;
				break;
			default:
				break;
		}
	}

	createFileDump();
}

void NodeProcessCode::DAGonFS_Write(void* buffer, fuse_ino_t inode, size_t fileSize) {
	LOG4CPLUS_TRACE(NodeProcessLogger, NodeProcessLogger.getName() << "Process " << rank << " - Invoked DAGonFS_Write()");

	int numberOfBlocks = fileSize / FILE_SYSTEM_SINGLE_BLOCK_SIZE + (fileSize % FILE_SYSTEM_SINGLE_BLOCK_SIZE > 0);
	int blockPerProcess = numberOfBlocks / mpi_world_size;
	int remainingBlocks = numberOfBlocks % mpi_world_size;
	int effectiveBlocks = blockPerProcess + (rank < remainingBlocks);

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
	PointerPacket *addresses = new PointerPacket[effectiveBlocks];
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
	MPI_Scatterv(MPI_IN_PLACE, scatterCounts, scatterDispls, MPI_BYTE, localScatBuf, scatterCounts[rank], MPI_BYTE, 0, MPI_COMM_WORLD);
	for (int i=0; i< effectiveBlocks; i++) {
		void *data_p = malloc(FILE_SYSTEM_SINGLE_BLOCK_SIZE);
		memcpy(data_p,localScatBuf+i*FILE_SYSTEM_SINGLE_BLOCK_SIZE,FILE_SYSTEM_SINGLE_BLOCK_SIZE);
		addresses[i] = PointerPacket(data_p);
	}
	MPI_Gatherv(addresses, gatherCounts[rank], MPI_BYTE, MPI_IN_PLACE, gatherCounts, gatherDispls, MPI_BYTE, 0, MPI_COMM_WORLD);

	if (dataBlockPointers.find(inode) == dataBlockPointers.end()) {
		createEmptyBlockListForInode(inode);
	}
	vector<DataBlock *> *inodeBlockList = &dataBlockPointers[inode];
	for (int i=0;i<effectiveBlocks;i++) {
		DataBlock *newDataBlock = new DataBlock(inode);
		newDataBlock->setData(addresses[i].address);
		newDataBlock->setRank(rank);
		inodeBlockList->push_back(newDataBlock);
	}

	delete[] scatterCounts;
	delete[] scatterDispls;
	delete[] gatherCounts;
	delete[] gatherDispls;
	delete[] addresses;

}

void* NodeProcessCode::DAGonFS_Read(fuse_ino_t inode, size_t fileSize, size_t reqSize, off_t offset) {
	LOG4CPLUS_TRACE(NodeProcessLogger, NodeProcessLogger.getName() << "Process " << rank << " - Invoked DAGonFS_Read()");
	if (fileSize == 0)
		return nullptr;

	size_t numberOfBlocksForRequest;
	if (reqSize > fileSize)
		numberOfBlocksForRequest = fileSize / FILE_SYSTEM_SINGLE_BLOCK_SIZE + (fileSize % FILE_SYSTEM_SINGLE_BLOCK_SIZE > 0);
	else
		numberOfBlocksForRequest = reqSize / FILE_SYSTEM_SINGLE_BLOCK_SIZE + (reqSize % FILE_SYSTEM_SINGLE_BLOCK_SIZE > 0);

	int blockPerProcess = numberOfBlocksForRequest / mpi_world_size;
	int remainingBlocks = numberOfBlocksForRequest % mpi_world_size;
	int effectiveBlocks = numberOfBlocksForRequest / mpi_world_size + (rank < remainingBlocks);

	PointerPacket *addressesFromScat = new PointerPacket[effectiveBlocks];
	int *scatterCounts = new int[mpi_world_size];
	int *scatterDispls = new int[mpi_world_size];
	int scatterOffset = 0;
	for (int i=0; i< mpi_world_size; i++) {
		scatterCounts[i] = ( blockPerProcess + (i < remainingBlocks) ) * sizeof(PointerPacket);
		scatterDispls[i] = scatterOffset;
		scatterOffset += scatterCounts[i];
	}

	void *dataToGath = malloc(effectiveBlocks * FILE_SYSTEM_SINGLE_BLOCK_SIZE);
	int *gatherCounts = new int[mpi_world_size];
	int *gatherDispls = new int[mpi_world_size];
	int gatherOffset = 0;
	for (int i=0; i< mpi_world_size; i++) {
		gatherCounts[i] = ( blockPerProcess + (i < remainingBlocks) ) * FILE_SYSTEM_SINGLE_BLOCK_SIZE;
		gatherDispls[i] = gatherOffset;
		gatherOffset += gatherCounts[i];
	}

	MPI_Scatterv(MPI_IN_PLACE, scatterCounts, scatterDispls, MPI_BYTE, addressesFromScat, scatterCounts[rank], MPI_BYTE, 0, MPI_COMM_WORLD);
	for (int i=0; i< effectiveBlocks; i++) {
		memcpy(dataToGath + i*FILE_SYSTEM_SINGLE_BLOCK_SIZE, addressesFromScat[i].address, FILE_SYSTEM_SINGLE_BLOCK_SIZE);
	}
	MPI_Gatherv(dataToGath, gatherCounts[rank], MPI_BYTE, MPI_IN_PLACE, gatherCounts, gatherDispls, MPI_BYTE, 0, MPI_COMM_WORLD);

	delete[] scatterCounts;
	delete[] scatterDispls;
	delete[] gatherCounts;
	delete[] gatherDispls;
	free(dataToGath);

	return nullptr;
}

void NodeProcessCode::createEmptyBlockListForInode(fuse_ino_t inode) {
	dataBlockPointers[inode] = vector<DataBlock *>();
}

vector<DataBlock*>& NodeProcessCode::getDataBlockPointers(fuse_ino_t inode) {
	return dataBlockPointers[inode];
}

void NodeProcessCode::createFileDump() {
	string dir="/tmp/DAGonFS_dump/"+to_string(rank);
	cout << "Process " << rank << " - Creating dump dir" << dir.c_str() << endl;

	DIR *process_dup_dir = opendir(dir.c_str());
	if (process_dup_dir) {
		closedir(process_dup_dir);
	}
	else {
		if (mkdir(dir.c_str(), 0777) < 0) {
			cout << "Process "<<rank<< " - mkdir /tmp/DAGonFS_dump/" << rank << " failed" << endl;
			return;
		}
	}

	if (chdir(dir.c_str()) < 0) {
		cout << "Process "<<rank<< " - cd /tmp/DAGonFS_dump/" << rank << " failed" << endl;
		return;
	}

	for (auto &inode: dataBlockPointers) {
		cout << "Process " << rank << " - Creating dump file for inode=" << inode.first << endl;
		string file_name_path="./";
		file_name_path+=to_string(inode.first);
		file_name_path+="-";
		cout << "Process " << rank << " - Creating file " << file_name_path << endl;
		for (auto &block: inode.second) {
			string file_name = file_name_path;
			cout << "Process " << rank << " - Creating file " << file_name << endl;

			ostringstream get_the_address;
			get_the_address << block->getData();
			file_name +=  get_the_address.str();

			cout << "Process " << rank << " - Creating file " << file_name << endl;

			FILE *file_tmp = fopen(file_name.c_str(), "w");
			fwrite(block->getData(), 1, FILE_SYSTEM_SINGLE_BLOCK_SIZE, file_tmp);
			fclose(file_tmp);

		}
	}

}


