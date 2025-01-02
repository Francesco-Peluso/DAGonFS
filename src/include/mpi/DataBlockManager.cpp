//
// Created by frank on 12/30/24.
//

#include "DataBlockManager.hpp"
#include <iostream>
#include <cstring>
#include <log4cplus/loggingmacros.h>

DataBlockManager *DataBlockManager::instance = nullptr;

DataBlockManager *DataBlockManager::getInstance(int mpi_world_size) {
	if (instance == nullptr) {
		instance = new DataBlockManager(mpi_world_size);
	}

	return instance;
}

DataBlockManager::DataBlockManager(int mpi_world_size) {
	this->mpi_world_size = mpi_world_size;
	DataBlockManagerLogger = Logger::getInstance("DataBlockManager.logger - ");
}

void DataBlockManager::addDataBlocksTo(vector<DataBlock*>& blockList, int nblocks, fuse_ino_t inode) {
	int startingIndex = blockList.size();
	LOG4CPLUS_INFO(DataBlockManagerLogger, DataBlockManagerLogger.getName() << "Master Process - Starting index for new blocks: " << startingIndex);
	int newSize = blockList.size() + nblocks;
	LOG4CPLUS_INFO(DataBlockManagerLogger, DataBlockManagerLogger.getName() << "Master Process - New block list size: " << newSize);
	int lastRank = blockList.size() == 0 ? 1 : blockList[startingIndex - 1]->getRank();
	LOG4CPLUS_INFO(DataBlockManagerLogger, DataBlockManagerLogger.getName() << "Master Process - Last rank in list " << lastRank);
	int rank_i = lastRank;
	LOG4CPLUS_INFO(DataBlockManagerLogger, DataBlockManagerLogger.getName() << "Master Process - Starting rank " << lastRank);

	for (int i = startingIndex; i < newSize; i++) {
		DataBlock *dataBlock = new DataBlock(inode);
		dataBlock->setRank(rank_i);
		dataBlock->setProgressiveNumber(i);
		dataBlock->setAbsoluteBytes(i*FILE_SYSTEM_SINGLE_BLOCK_SIZE);
		blockList.push_back(dataBlock);

		rank_i = (rank_i + 1) == mpi_world_size ? 1 : rank_i + 1;
	}
}
