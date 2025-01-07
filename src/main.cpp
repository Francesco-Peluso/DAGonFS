#include <iostream>
#include <mpi.h>
#include <sys/stat.h>
#include <sstream>

#include "include/ramfs/FileSystem.hpp"
#include "include/blocks/Blocks.hpp"
#include "include/mpi/NodeProcessCode.hpp"

#include "include/utils/log_level.hpp"

using namespace std;

using namespace log4cplus;

//LogLevel DAGONFS_LOG_LEVEL = OFF_LOG_LEVEL;
LogLevel DAGONFS_LOG_LEVEL = ALL_LOG_LEVEL;

static void show_usage(const char *progname);

int main(int argc, char *argv[]){
    int ret = 0;

    if (argc < 1) {
        show_usage(argv[0]);
        return ret;
    }

    if(argc > 2){
      bool logLevelFlag;
      istringstream(argv[2]) >> logLevelFlag;
      if(logLevelFlag)
        DAGONFS_LOG_LEVEL = ALL_LOG_LEVEL;
    }
    
    Initializer initializer;
    BasicConfigurator config;
    config.configure();

    //RAM FS
    //Setting umask of the process
    mode_t oldUmask = umask(0000);

    //MPI
    //Inizialize MPI
    int mpiWorldSize, mpiRank;
    MPI_Init(&argc, &argv);

    //MPI
    //Get number of involved processes
    MPI_Comm_size(MPI_COMM_WORLD, &mpiWorldSize);

    //MPI
    //Get the process' rank -> rank = 0 is the main process
    MPI_Comm_rank(MPI_COMM_WORLD, &mpiRank);

    //MPI
    //The master process will manage the RAM FS
    if(mpiRank == 0) {
        Nodes::getInstance();
        Blocks::getInstance();
        DataBlockManager::getInstance(mpiWorldSize);

        //LIBFUSE
        //Creation of our RAM FS
        FileSystem ramfs = FileSystem(mpiRank,mpiWorldSize);
        ret = ramfs.start(argc, argv);
    }
    //MPI
    //Other process will manage the reading and writing operations
    else {
        NodeProcessCode *node = NodeProcessCode::getInstance(mpiRank, mpiWorldSize);
        node->start();
    }

    cout << "Process rank=" << mpiRank << " will wait other process to terminate" << endl;
    MPI_Barrier(MPI_COMM_WORLD);
    cout << "Process rank=" << mpiRank << " is about to terminate in main.cpp" <<endl;
    MPI_Finalize();

    umask(oldUmask);

    return ret;
}

static void show_usage(const char *progname) {
    cout << "Usage: mpirun -np <number of processes> [--hostfile <hostfile>] " << progname << " [-d][-f] <mountpoint> [1 -log flag]" << endl;
}