//
// Created by frank on 12/30/24.
//

#include "DataBlockManager.hpp"
#include <iostream>
#include <cstring>

using namespace log4cplus;

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
	LogLevel ll = DAGONFS_LOG_LEVEL;
	DataBlockManagerLogger.setLogLevel(ll);
}

void DataBlockManager::addDataBlocksTo(vector<DataBlock*>& blockList, int nblocks, fuse_ino_t inode, int startingRank) {
	int startingIndex = blockList.size();
	LOG4CPLUS_INFO(DataBlockManagerLogger, DataBlockManagerLogger.getName() << "Master Process - Starting index for new blocks: " << startingIndex);
	int newSize = blockList.size() + nblocks;
	LOG4CPLUS_INFO(DataBlockManagerLogger, DataBlockManagerLogger.getName() << "Master Process - New block list size: " << newSize);
	int lastRank = blockList.size() == 0 ? 0 : blockList[startingIndex - 1]->getRank();
	LOG4CPLUS_INFO(DataBlockManagerLogger, DataBlockManagerLogger.getName() << "Master Process - Last rank in list " << lastRank);
	int rank_i = lastRank;
	LOG4CPLUS_INFO(DataBlockManagerLogger, DataBlockManagerLogger.getName() << "Master Process - Starting rank " << lastRank);

	for (int i = startingIndex; i < newSize; i++) {
		DataBlock *dataBlock = new DataBlock(inode);
		dataBlock->setRank(rank_i);
		dataBlock->setProgressiveNumber(i);
		dataBlock->setAbsoluteBytes(i*FILE_SYSTEM_SINGLE_BLOCK_SIZE);
		blockList.push_back(dataBlock);

		if (++rank_i == mpi_world_size) rank_i = 0;
	}
}
