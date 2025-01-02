//
// Created by frank on 12/30/24.
//

#ifndef DATABLOCKMANAGER_HPP
#define DATABLOCKMANAGER_HPP

#include <vector>
#include <log4cplus/logger.h>

#include "../blocks/DataBlock.hpp"
using namespace std;
using namespace log4cplus;

class DataBlockManager {
private:
	//Singleton implementation
	static DataBlockManager* instance;
	DataBlockManager(int mpi_world_size);

	int mpi_world_size;
	Logger DataBlockManagerLogger;

public:
	static DataBlockManager* getInstance(int mpi_world_size);

	void addDataBlocksTo(vector<DataBlock *> &blockList, int nblocks, fuse_ino_t inode);
};



#endif //DATABLOCKMANAGER_HPP
