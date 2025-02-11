//
// Created by frank on 1/15/25.
//

#ifndef DISTRIBUTEDCODE_HPP
#define DISTRIBUTEDCODE_HPP
#include <string>

#include "DataBlockManager.hpp"
#include "mpi_data.hpp"
#include "../utils/fuse_headers.hpp"


class DistributedCode {
private:
	static int mpiRank;
	static int mpiWorldSize;
	static std::string fsPath;
	static std::string unmountScript;

	static DistributedCode *instance;
	DistributedCode(int rank, int worldSize, const char *mountpointPath);

	static DataBlockManager *dataBlockManager;

	static int *scatterCounts;
	static int *scatterDispls;
	static int scatterOffset;
	static int *gatherCounts;
	static int *gatherDispls;
	static int gatherOffset;

	static double DAGonFSWriteSGElapsedTime;
	static double lastWriteTime;
	static double DAGonFSReadSGElapsedTime;
	static double lastReadTime;

public:
	static DistributedCode *getInstance(int rank, int worldSize, const char *mountpointPath);
	void setup();
	static void start();
	static void DAGonFS_Write(int sourceRank, void *buffer, fuse_ino_t inode, size_t fileSize);
	static void* DAGonFS_Read(int sourceRank, fuse_ino_t inode, size_t fileSize, size_t reqSize, off_t offset);
	static void createFile();
	static void deleteFile();
	static void createDir();
	static void deleteDir();
	static void renameHandler();
	static void unmountFileSystem();

	double getDAGonFSWriteSGElapsedTime() { return DAGonFSWriteSGElapsedTime; };
	double getDAGonFSReadSGElapsedTime() { return DAGonFSReadSGElapsedTime; };
	double getLastWriteTime() { return lastWriteTime; };
	double getLastReadTime() { return lastReadTime; };
};



#endif //DISTRIBUTEDCODE_HPP
