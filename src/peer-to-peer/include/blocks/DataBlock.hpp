//
// Created by frank on 12/30/24.
//

#ifndef DATABLOCK_HPP
#define DATABLOCK_HPP
#include <string>
#include "../utils/fuse_headers.hpp"

#include "data_blocks_info.hpp"
using namespace std;

class DataBlock {
private:
	fuse_ino_t inode;
	void *dataBlockAddress;
	unsigned int usedBytes;
	unsigned int absoluteBytes;
	unsigned int processRank;
	unsigned int progressiveNumber;

public:
	DataBlock();
	DataBlock(fuse_ino_t inode);
	DataBlock(fuse_ino_t inode, void *dataBlockAddress, unsigned int absoluteBytes);
	~DataBlock();

	fuse_ino_t getInode(){ return inode; }

	void *getData(){ return dataBlockAddress; }
	void setData(void *address);

	void setUsedBytes(unsigned int bytes) { usedBytes = bytes; }
	unsigned int getUsedBytes(){ return usedBytes; }
	unsigned int getFreeBytes(){ return FILE_SYSTEM_SINGLE_BLOCK_SIZE - usedBytes; }
	bool isFull() { return usedBytes == FILE_SYSTEM_SINGLE_BLOCK_SIZE; }

	unsigned int getAbsoluteBytes(){ return absoluteBytes; }
	void setAbsoluteBytes(unsigned int bytes){ absoluteBytes = bytes; }

	unsigned int getProgressiveNumber(){ return progressiveNumber; }
	void setProgressiveNumber(unsigned int number){ progressiveNumber = number; }

	unsigned int getRank(){ return processRank; }
	void setRank(unsigned int rank){ processRank = rank; }

	void *allocateBlock();
	void freeBlock();
};



#endif //DATABLOCK_HPP
