//
// Created by frank on 12/30/24.
//

#ifndef DISTRIBUTEDWRITE_HPP
#define DISTRIBUTEDWRITE_HPP

#include "../utils/fuse_headers.hpp"

class DistributedWrite {
public:
	virtual ~DistributedWrite() {};
	virtual void DAGonFS_Write(void *buffer, fuse_ino_t inode, size_t fileSize) = 0;
};

#endif //DISTRIBUTEDWRITE_HPP
