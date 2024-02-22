.. image:: https://travis-ci.org/watkipet/fuse-cpp-ramfs.svg?branch=master
    :target: https://travis-ci.org/watkipet/fuse-cpp-ramfs

======================================================================
mpi-ramfs: An example RAM filesystem using FUSE and MPI written in C++
======================================================================

.. contents::

Quick Run
=========
::

        mkdir
	cd build
	cmake -DUSE_MPI=ON ../src
	make
        mkdir /tmp/mydir
	mpirun -np 1 mpi-ramfs -f /tmp/mydir


Requirements
============
mpi-ramfs builds with CMake version 3.0 or greater.

mpi-ramfs requires the libfuse3-3.2 (or later) 
filesystem-in-userspace library and header files for successful 
compilation.  libfuse is available
at: 
https://github.com/libfuse/libfuse
https://osxfuse.github.io

--
Thanks to Peter Watkins for his remarkable source code (https://github.com/watkipet/fuse-cpp-ramfs).


