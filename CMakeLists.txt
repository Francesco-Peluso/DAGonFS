cmake_minimum_required(VERSION 3.0)
project(mpi-ramfs)

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

add_executable(mpi-ramfs
        src/main.cpp
        src/directory.cpp src/directory.hpp
        src/inode.cpp src/inode.hpp
        src/symlink.cpp src/symlink.hpp
        src/file.cpp src/file.hpp
        src/util.cpp src/util.hpp
        src/fuse_cpp_ramfs.cpp src/fuse_cpp_ramfs.hpp
        src/special_inode.cpp src/special_inode.hpp
)

target_compile_definitions(mpi-ramfs PRIVATE FUSE_USE_VERSION=32 _FILE_OFFSET_BITS=64)
set_property(TARGET mpi-ramfs PROPERTY CXX_STANDARD 14)

target_link_libraries(mpi-ramfs ${FUSE3_LIBRARIES} ${MPI_C_LIBRARIES})

install(TARGETS mpi-ramfs DESTINATION bin)
