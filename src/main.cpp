/** @file main.cpp
 *  @copyright 2016 Peter Watkins. All rights reserved.
 */

#ifdef __APPLE__
#include <osxfuse/fuse/fuse_lowlevel.h>
#else
#include <fuse/fuse_lowlevel.h>
#endif
#include <iostream>
#include <vector>
#include <queue>
#include <map>
#include <cstring>

#include "inode.hpp"
#include "fuse_cpp_ramfs.hpp"

using namespace std;

char **copy_args(int argc, const char * argv[]) {
    char **new_argv = new char*[argc];
    for (int i = 0; i < argc; ++i) {
        int len = (int) strlen(argv[i]) + 1;
        new_argv[i] = new char[len];
        strncpy(new_argv[i], argv[i], len);
    }
    return new_argv;
}

void delete_args(int argc, char **argv) {
    for (int i = 0; i < argc; ++i) {
        delete argv[i];
    }
    delete argv;
}

/** The main entry point for the program. A filesystem may be mounted by running this executable directly.
 * @param argc The number of arguments.
 * @param argv A pointer to the first argument.
 * @return The exit code of the program. Zero on success. */
int main(int argc, const char * argv[]) {

    
    int world_size=1, world_rank=0;

    int ret = -1;

    #ifdef USE_MPI
       char **mpiArgv = (char **)argv;
    
       // Initialize MPI
       MPI_Init(&argc, &mpiArgv);

       // Get the number of involved processes
       MPI_Comm_size(MPI_COMM_WORLD, &world_size);

       // Get the number of the current process (world_rank=0 is for the main process)
       MPI_Comm_rank(MPI_COMM_WORLD, &world_rank);
    #endif


    if (world_rank == 0) {


    char **fuse_argv = copy_args(argc, argv);

    struct fuse_args args = FUSE_ARGS_INIT(argc, fuse_argv);
    struct fuse_cmdline_opts opts;
    struct fuse_loop_config config;

    // The core code for our filesystem.
    FuseRamFs core;

    if (fuse_parse_cmdline(&args, &opts) != 0)
        return 1;

    if (opts.show_help) {
        printf("usage: %s [options] <mountpoint>\n\n", argv[0]);
        fuse_cmdline_help();
        fuse_lowlevel_help();
        ret = 0;
    } else if (opts.show_version) {
        printf("FUSE library version %s\n", fuse_pkgversion());
        fuse_lowlevel_version();
        ret = 0;
    }
    else if(opts.mountpoint == NULL) {
        printf("usage: %s [options] <mountpoint>\n", argv[0]);
        printf("       %s --help\n", argv[0]);
        ret = 1;
    }
    else {

        struct fuse_session *se = fuse_session_new(&args, &(core.FuseOps), sizeof(core.FuseOps), NULL);

        if (se) {

            if (fuse_set_signal_handlers(se) == 0) {
                if (fuse_session_mount(se, opts.mountpoint) == 0) {

                    fuse_daemonize(opts.foreground);

                    /* Block until ctrl+c or fusermount -u */
                    if (opts.singlethread) {
                        ret = fuse_session_loop(se);
                    }
                    else {
                        config.clone_fd = opts.clone_fd;
                        config.max_idle_threads = opts.max_idle_threads;
                        ret = fuse_session_loop_mt(se, &config);
                    }

                    fuse_session_unmount(se);
                }
                fuse_remove_signal_handlers(se);
            }
            fuse_session_destroy(se);
        }
    }

    free(opts.mountpoint);
    fuse_opt_free_args(&args);

    }
    else {
    }
    return ret ? 1 : 0;
}
