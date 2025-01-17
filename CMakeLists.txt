cmake_minimum_required(VERSION 3.0)

set(PROJECT_NAME DAGonFS)
set(EXECUTABLE_NAME "${PROJECT_NAME}.exe")
project(${PROJECT_NAME})

option(USE_MPI "Use MPI for distributed memory." OFF)

list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_LIST_DIR}/CMake)

find_package(FUSE3 REQUIRED)
include_directories(SYSTEM ${FUSE3_INCLUDE_DIR})

find_package(MPI)
if(MPI_C_FOUND)
  if (USE_MPI)
    message(STATUS "Using MPI for distributed memory.")
    add_definitions(-DUSE_MPI)
    include_directories(SYSTEM ${MPI_INCLUDE_PATH})
  endif()
endif()

if(WIN32)
  set(SHAREDIR ".")
  set(DOCDIR "doc")
elseif(APPLE)
  set(SHAREDIR "${CMAKE_INSTALL_PREFIX}/share/${PROJECT_NAME}")
  set(DOCDIR "share/doc/${PROJECT_NAME}")
  set(MANDIR "share/man")
elseif(UNIX) # Linux, BSD etc
  set(SHAREDIR "share/${PROJECT_NAME}")
  set(DOCDIR "share/doc/${PROJECT_NAME}")
  set(MANDIR "share/man")
endif()


add_subdirectory(doc)

#install(DIRECTORY sample_data DESTINATION "${SHAREDIR}")

#log4cplus
#find_package(LOG4CPLUS)
#include_directories(SYSTEM ${LOG4CPLUS_INCLUDE_DIR})
set(EXTERNAL_INSTALL_LOCATION ${CMAKE_BINARY_DIR}/external)
include_directories(SYSTEM ${EXTERNAL_INSTALL_LOCATION}/include)
include(ExternalProject)
link_directories(${EXTERNAL_INSTALL_LOCATION}/lib)
ExternalProject_Add(log4cplus
        #URL https://kumisystems.dl.sourceforge.net/project/log4cplus/log4cplus-stable/2.0.5/log4cplus-2.0.5.tar.gz
        URL https://github.com/log4cplus/log4cplus/releases/download/REL_2_1_1/log4cplus-2.1.1.tar.gz
        TIMEOUT 360
        BUILD_IN_SOURCE 1
        CONFIGURE_COMMAND ./configure --prefix=${EXTERNAL_INSTALL_LOCATION} CFLAGS=-fPIC CPPFLAGS=-I${EXTERNAL_INSTALL_LOCATION}/include/ LDFLAGS=-L${EXTERNAL_INSTALL_LOCATION}/lib/
        BUILD_COMMAND make
        INSTALL_COMMAND make install
)
set(LOG4CPLUS_LIBRARIES ${EXTERNAL_INSTALL_LOCATION}/lib/liblog4cplus.so)
if(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    set(LOG4CPLUS_LIBRARIES ${EXTERNAL_INSTALL_LOCATION}/lib/liblog4cplus.dylib)
endif()

set(DAGONFS_HEADER_FILES
        src/include/ramfs/FileSystem.hpp
        src/include/nodes/Nodes.hpp
        src/include/nodes/INode.hpp
        src/include/nodes/File.hpp
        src/include/nodes/Directory.hpp
        src/include/nodes/inodes_data_structures.hpp
        src/include/nodes/SpecialINode.hpp
        src/include/nodes/SymbolicLink.hpp
        src/include/nodes/INodeTypes.hpp
        src/include/blocks/data_blocks_info.hpp
        src/include/blocks/Blocks.hpp
        src/include/blocks/DataBlock.hpp
        src/include/mpi/DistributedRead.hpp
        src/include/mpi/DistributedWrite.hpp
        src/include/mpi/MasterProcessCode.hpp
        src/include/mpi/NodeProcessCode.hpp
        src/include/mpi/mpi_data.hpp
        src/include/mpi/DataBlockManager.hpp
        src/include/utils/fuse_headers.hpp
        src/include/utils/log_level.hpp
        src/include/utils/ArgumentParser.hpp
)

set(DAGONFS_SOURCE_FILES
        src/include/ramfs/FileSystem.cpp
        src/include/nodes/Nodes.cpp
        src/include/nodes/INode.cpp
        src/include/nodes/File.cpp
        src/include/nodes/Directory.cpp
        src/include/nodes/SpecialINode.cpp
        src/include/nodes/SymbolicLink.cpp
        src/include/blocks/Blocks.cpp
        src/include/blocks/DataBlock.cpp
        src/include/mpi/MasterProcessCode.cpp
        src/include/mpi/NodeProcessCode.cpp
        src/include/mpi/DataBlockManager.cpp
        src/include/utils/ArgumentParser.cpp
)

add_executable(${PROJECT_NAME}
        src/main.cpp
         ${DAGONFS_HEADER_FILES} ${DAGONFS_SOURCE_FILES}
)
set_target_properties(${PROJECT_NAME} PROPERTIES OUTPUT_NAME ${EXECUTABLE_NAME})

target_compile_definitions(${PROJECT_NAME} PRIVATE FUSE_USE_VERSION=32 _FILE_OFFSET_BITS=64)
set_property(TARGET ${PROJECT_NAME} PROPERTY CXX_STANDARD 23)

target_link_libraries(${PROJECT_NAME} ${FUSE3_LIBRARIES} ${MPI_C_LIBRARIES} ${LOG4CPLUS_LIBRARIES})

install(TARGETS ${PROJECT_NAME} DESTINATION bin)
