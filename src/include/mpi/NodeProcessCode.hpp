//
// Created by frank on 12/30/24.
//

#ifndef NODEPROCESSCODE_HPP
#define NODEPROCESSCODE_HPP
#include <map>

#include "DataBlockManager.hpp"
#include "DistributedWrite.hpp"
#include "DistributedRead.hpp"
#include "../utils/my_log_level.hpp"

class NodeProcessCode: public DistributedWrite, public DistributedRead {
private:
	//Singleton implementation
	static NodeProcessCode* instance;
	NodeProcessCode(int rank, int mpi_world_size);

	int rank;
	int mpi_world_size;

	map<fuse_ino_t, vector<DataBlock *> > dataBlockPointers;

	DataBlockManager *dataBlockManager;
	Logger NodeProcessLogger;

public:
	static NodeProcessCode *getInstance(int rank, int mpi_world_size);

	~NodeProcessCode() override;
	void DAGonFS_Write(void* buffer, fuse_ino_t inode, size_t fileSize) override;
	void* DAGonFS_Read(fuse_ino_t inode, size_t fileSize, size_t reqSize, off_t offset) override;

	void createEmptyBlockListForInode(fuse_ino_t inode);
	vector<DataBlock *> &getDataBlockPointers(fuse_ino_t inode);

	void start();
};



#endif //NODEPROCESSCODE_HPP
