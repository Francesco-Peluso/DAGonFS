//
// Created by frank on 12/30/24.
//

#include "Blocks.hpp"

#include "data_blocks_info.hpp"

Blocks *Blocks::instance = nullptr;

Blocks *Blocks::getInstance() {
	if (instance == nullptr) {
		instance = new Blocks();
	}

	return instance;
}

Blocks::Blocks() {
	FileSystemDataBlocks = map<fuse_ino_t, vector<DataBlock *> >();
}

Blocks::~Blocks() {

}

void Blocks::createEmptyBlockListForInode(fuse_ino_t inode) {
	if (!FileSystemDataBlocks.contains(inode)) {
		FileSystemDataBlocks[inode] = vector<DataBlock *>();
	}
}

void Blocks::setBlockListForInode(fuse_ino_t inode, vector<DataBlock*> &blockList) {
	FileSystemDataBlocks[inode] = blockList;
}

bool Blocks::blockListExistForInode(fuse_ino_t inode) {
	return FileSystemDataBlocks.contains(inode);
}


vector<DataBlock*>& Blocks::getDataBlockListOfInode(fuse_ino_t inode) {
	return FileSystemDataBlocks[inode];
}

DataBlock* Blocks::addDataBlockToInode(fuse_ino_t inode) {
	DataBlock *newBlock = new DataBlock(inode);

	if (!FileSystemDataBlocks[inode].empty()) {
		DataBlock *lastBlock = FileSystemDataBlocks[inode].back();
		unsigned int newAbsBytes = lastBlock->getAbsoluteBytes() + FILE_SYSTEM_SINGLE_BLOCK_SIZE;
		newBlock->setAbsoluteBytes(newAbsBytes);
	}

	FileSystemDataBlocks[inode].push_back(newBlock);
	return newBlock;
}


void Blocks::addDataBlockToInode(fuse_ino_t inode, DataBlock* dataBlock) {
	FileSystemDataBlocks[inode].push_back(dataBlock);
}

unsigned int Blocks::getNumberOfUsedBlocksOfInode(fuse_ino_t inode) {
	return FileSystemDataBlocks[inode].size();
}

unsigned int Blocks::getTotalBlockBytesOfInode(fuse_ino_t inode) {
	unsigned int totalBytes = FileSystemDataBlocks[inode].size();
	return totalBytes * FILE_SYSTEM_SINGLE_BLOCK_SIZE;
}

bool Blocks::hasNoBlocks(fuse_ino_t inode) {
	return FileSystemDataBlocks[inode].empty();
}


