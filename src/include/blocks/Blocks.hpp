//
// Created by frank on 12/30/24.
//

#ifndef BLOCKS_HPP
#define BLOCKS_HPP

#include <map>
#include <vector>
#include "../utils/fuse_headers.hpp"

#include "DataBlock.hpp"

using namespace std;

class Blocks {
private:
	//Singleton implementation
	static Blocks *instance;
	Blocks();

	map<fuse_ino_t, vector<DataBlock *> > FileSystemDataBlocks;

public:
	//Singleton implementation
	static Blocks* getInstance();

	~Blocks();

	void createEmptyBlockListForInode(fuse_ino_t inode);
	void setBlockListForInode(fuse_ino_t inode, vector<DataBlock *> &blockList);
	bool blockListExistForInode(fuse_ino_t inode);
	vector<DataBlock *> &getDataBlockListOfInode(fuse_ino_t inode);
	DataBlock *addDataBlockToInode(fuse_ino_t inode);
	void addDataBlockToInode(fuse_ino_t inode, DataBlock *dataBlock);
	unsigned int getNumberOfUsedBlocksOfInode(fuse_ino_t inode);
	unsigned int getTotalBlockBytesOfInode(fuse_ino_t inode);
	bool hasNoBlocks(fuse_ino_t inode);

	map<fuse_ino_t, vector<DataBlock *> >& getAll() {return FileSystemDataBlocks;}
};



#endif //BLOCKS_HPP
