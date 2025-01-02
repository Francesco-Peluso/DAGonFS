#include <iostream>
#include <mpi.h>
#include <sys/stat.h>

#include "include/ramfs/FileSystem.hpp"
#include "include/blocks/Blocks.hpp"
#include "include/mpi/NodeProcessCode.hpp"

//Prova logging
#include <log4cplus/logger.h>
#include <log4cplus/loggingmacros.h>
#include <log4cplus/configurator.h>
#include <log4cplus/initializer.h>

using namespace log4cplus;

using namespace std;

static void show_usage(const char *progname);

int main(int argc, char *argv[]){
    int ret = 0;

    Initializer initializer;
    BasicConfigurator config;
    config.configure();
    Logger logger = Logger::getInstance(LOG4CPLUS_TEXT("main"));


    //RAM FS
    //Setting umask of the process
    mode_t oldUmask = umask(0777);

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
