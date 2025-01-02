//
// Created by frank on 12/30/24.
//

#ifndef DISTRIBUTEDREAD_HPP
#define DISTRIBUTEDREAD_HPP

#include <fuse3/fuse_lowlevel.h>

class DistributedRead {
public:
	virtual ~DistributedRead() {};
	virtual void *DAGonFS_Read(fuse_ino_t inode, size_t fileSize, size_t reqSize, off_t offset) = 0;
};

#endif //DISTRIBUTEDREAD_HPP
