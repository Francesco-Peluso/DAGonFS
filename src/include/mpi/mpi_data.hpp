//
// Created by frank on 12/30/24.
//

#ifndef MPI_DATA_HPP
#define MPI_DATA_HPP

typedef enum {WRITE, READ, CHANGE_DIR, REDUCE_BLOCKS, TERMINATE} RequestType;

typedef struct RequstPacket {
	RequestType type;
} RequestPacket;

typedef struct IORequestPacket {
	fuse_ino_t inode;
	size_t fileSize;
	size_t reqSize;
	off_t offset;
} IORequestPacket;

typedef struct PointerPacket {
	void *address;
} PointerPacket;

#endif //MPI_DATA_HPP
