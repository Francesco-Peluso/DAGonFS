//
// Created by frank on 12/30/24.
//

#include "NodeProcessCode.hpp"

#include <iostream>
#include <unistd.h>
#include <cstring>

#include <mpi.h>
#include "mpi_data.hpp"

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
}

void NodeProcessCode::DAGonFS_Write(void* buffer, fuse_ino_t inode, size_t fileSize) {
	LOG4CPLUS_TRACE(NodeProcessLogger, NodeProcessLogger.getName() << "Process " << rank << " - Invoked DAGonFS_Write()");
	if (dataBlockPointers.find(inode) == dataBlockPointers.end()) {
		createEmptyBlockListForInode(inode);
	}
	vector<DataBlock *> *inodeBlockList = &dataBlockPointers[inode];

	//Calcolo dimensioni
	unsigned int numberOfBlocks = fileSize / FILE_SYSTEM_SINGLE_BLOCK_SIZE + (fileSize % FILE_SYSTEM_SINGLE_BLOCK_SIZE > 0);
	unsigned int blocksPerProcess = numberOfBlocks / (mpi_world_size - 1);
	void *receivedBlock;
	PointerPacket newAddress;
	for (int i = 0; i < blocksPerProcess; i++) {
		//Ricezione dei dati dalla Scatter del master
		//Ogni volta un puntatore diverso
		receivedBlock = malloc(FILE_SYSTEM_SINGLE_BLOCK_SIZE);
		MPI_Scatter(MPI_IN_PLACE, FILE_SYSTEM_SINGLE_BLOCK_SIZE, MPI_BYTE,
					receivedBlock, FILE_SYSTEM_SINGLE_BLOCK_SIZE, MPI_BYTE,
					0, MPI_COMM_WORLD);
		//LOG4CPLUS_TRACE(NodeProcessLogger, NodeProcessLogger.getName() << "Received block: " << (char *) receivedBlock);

		//Invio puntatori alla Gather del master
		newAddress.address = receivedBlock;
		//LOG4CPLUS_TRACE(NodeProcessLogger, NodeProcessLogger.getName() << "New address: " << newAddress.address);
		MPI_Gather(&newAddress, sizeof(PointerPacket), MPI_BYTE,
					MPI_IN_PLACE, sizeof(PointerPacket), MPI_BYTE,
					0, MPI_COMM_WORLD);

		//Salvataggio puntatore per mille casi
		DataBlock *newDataBlock = new DataBlock(inode);
		newDataBlock->setData(newAddress.address);
		newDataBlock->setRank(rank);
		inodeBlockList->push_back(newDataBlock);
	}


	//Gestione numero blocchi non divisibili per il nunmero di processi
	unsigned int remainingBlocks = numberOfBlocks % (mpi_world_size - 1);
	LOG4CPLUS_DEBUG(NodeProcessLogger, NodeProcessLogger.getName() << "Process " << rank << " - remainingBlocks=" << remainingBlocks);
	if (remainingBlocks > 0 && rank <= remainingBlocks) {
		LOG4CPLUS_TRACE(NodeProcessLogger, NodeProcessLogger.getName() << "Process " << rank << " - Will give store one of the remaining blocks");
		receivedBlock = malloc(FILE_SYSTEM_SINGLE_BLOCK_SIZE);
		MPI_Status status;
		MPI_Recv(receivedBlock, FILE_SYSTEM_SINGLE_BLOCK_SIZE, MPI_BYTE,0,0, MPI_COMM_WORLD, &status);

		newAddress.address = receivedBlock;
		MPI_Send(&newAddress, sizeof(PointerPacket), MPI_BYTE, 0, 0, MPI_COMM_WORLD);

		DataBlock *newDataBlock = new DataBlock(inode);
		newDataBlock->freeBlock();
		newDataBlock->setData(newAddress.address);
		inodeBlockList->push_back(newDataBlock);
	}

}

void* NodeProcessCode::DAGonFS_Read(fuse_ino_t inode, size_t fileSize, size_t reqSize, off_t offset) {
	LOG4CPLUS_TRACE(NodeProcessLogger, NodeProcessLogger.getName() << "Process " << rank << " - Invoked DAGonFS_Read()");
	int numberOfBlocksForRequest;
	if (reqSize > fileSize)
		numberOfBlocksForRequest = fileSize / FILE_SYSTEM_SINGLE_BLOCK_SIZE + (fileSize % FILE_SYSTEM_SINGLE_BLOCK_SIZE > 0);
	else
		numberOfBlocksForRequest = reqSize / FILE_SYSTEM_SINGLE_BLOCK_SIZE + (reqSize % FILE_SYSTEM_SINGLE_BLOCK_SIZE > 0);
	int blocksPerProcess = numberOfBlocksForRequest / (mpi_world_size - 1);
	int remainingBlocks = numberOfBlocksForRequest % (mpi_world_size - 1);
	if (rank <= remainingBlocks) {
		blocksPerProcess++;
	}

	PointerPacket readAddress;
	MPI_Status status;
	LOG4CPLUS_DEBUG(NodeProcessLogger, NodeProcessLogger.getName() << "Process " << rank << " - Will receive " << blocksPerProcess << " blocks");
	for (int i = 0; i < blocksPerProcess; i++) {
		MPI_Recv(&readAddress, sizeof(PointerPacket), MPI_BYTE, 0, 0, MPI_COMM_WORLD, &status);
		//LOG4CPLUS_DEBUG(NodeProcessLogger, NodeProcessLogger.getName() << "Process " << rank << " - Received " << readAddress.address);
		MPI_Send(readAddress.address, FILE_SYSTEM_SINGLE_BLOCK_SIZE, MPI_BYTE, 0, 0, MPI_COMM_WORLD);
	}

	return nullptr;
}

void NodeProcessCode::createEmptyBlockListForInode(fuse_ino_t inode) {
	dataBlockPointers[inode] = vector<DataBlock *>();
}

vector<DataBlock*>& NodeProcessCode::getDataBlockPointers(fuse_ino_t inode) {
	return dataBlockPointers[inode];
}


