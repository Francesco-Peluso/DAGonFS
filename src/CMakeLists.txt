option(USE_MPI "Use MPI for distributed memory." OFF)

set(LIBMPI "")

find_package(MPI)
if(MPI_C_FOUND)
    if (USE_MPI)
        message(STATUS "Using MPI for distributed memory parallelism.")
        add_definitions(-DUSE_MPI)
        include_directories(${MPI_INCLUDE_PATH})
        set(LIBMPI ${MPI_C_LIBRARIES})
    endif()
endif()

add_executable(fuse-cpp-ramfs main.cpp directory.cpp inode.cpp symlink.cpp file.cpp util.cpp fuse_cpp_ramfs.cpp special_inode.cpp)
set_property(TARGET fuse-cpp-ramfs PROPERTY CXX_STANDARD 11)
target_compile_definitions(fuse-cpp-ramfs PRIVATE FUSE_USE_VERSION=32 _FILE_OFFSET_BITS=64)
if(APPLE)
  target_link_libraries(fuse-cpp-ramfs osxfuse)
elseif(UNIX) # Linux, BSD etc
  target_link_libraries(fuse-cpp-ramfs ${LIBMPI} fuse3)
endif()
install(TARGETS fuse-cpp-ramfs DESTINATION bin)
