//
// Created by frank on 12/30/24.
//

#include "DataBlock.hpp"

DataBlock::DataBlock() {
	inode = 0;
	dataBlockAddress = nullptr;
	usedBytes = 0;
	absoluteBytes = 0;
	processRank = 0;
	progressiveNumber = 0;
}

DataBlock::DataBlock(fuse_ino_t inode) {
	this->inode = inode;
	dataBlockAddress = nullptr;
	usedBytes = 0;
	absoluteBytes = 0;
	processRank = 0;
	progressiveNumber = 0;
}

DataBlock::DataBlock(fuse_ino_t inode, void *dataBlockAddress, unsigned int absoluteBytes) {
	this->inode = inode;
	this->dataBlockAddress = dataBlockAddress;
	usedBytes = 0;
	this->absoluteBytes = absoluteBytes;
	processRank = 0;
	progressiveNumber = 0;
}

DataBlock::~DataBlock() {
	if (dataBlockAddress) free(dataBlockAddress);
}

void DataBlock::setData(void* address) {
	dataBlockAddress = address;
}

void* DataBlock::allocateBlock() {
	dataBlockAddress = calloc(FILE_SYSTEM_SINGLE_BLOCK_SIZE,1);
	return dataBlockAddress;
}

void DataBlock::freeBlock() {
	if (dataBlockAddress) free(dataBlockAddress);
}

