//
// Created by frank on 12/30/24.
//

#ifndef MASTERPROCESSCODE_HPP
#define MASTERPROCESSCODE_HPP

#include "DataBlockManager.hpp"
#include "DistributedRead.hpp"
#include "DistributedWrite.hpp"

#include "../utils/log_level.hpp"

class MasterProcessCode: public DistributedWrite, public DistributedRead {
private:
	//Singleton implementation
	static MasterProcessCode* instance;
	MasterProcessCode(int rank, int mpi_world_size);

	int rank;
	int mpi_world_size;

	int *scatterCounts;
	int *scatterDispls;
	int scatterOffset;
	int *gatherCounts;
	int *gatherDispls;
	int gatherOffset;

	DataBlockManager *dataBlockManager;
	log4cplus::Logger MasterProcessLogger;

public:
	double DAGonFSWriteSGElapsedTime;
	double DAGonFSReadSGElapsedTime;;
	double lastWriteTime;
	double lastReadTime;
	static MasterProcessCode* getInstance(int rank, int mpi_world_size);

	~MasterProcessCode() override;
	void DAGonFS_Write(void* buffer, fuse_ino_t inode, size_t fileSize) override;
	void* DAGonFS_Read(fuse_ino_t inode, size_t fileSize, size_t reqSize, off_t offset) override;

	void sendWriteRequest();
	void sendReadRequest();
	void sendTermination();
	void sendChangedir();
	void createFileDump();
};

#endif //MASTERPROCESSCODE_HPP
