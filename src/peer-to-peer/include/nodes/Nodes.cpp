//
// Created by frank on 11/29/24.
//

#include "Nodes.hpp"
#include "inodes_data_structures.hpp"

#include <iostream>
#include <cstring>

#include "../ramfs/FileSystem.hpp"

using namespace std;

Nodes *Nodes::_instance = nullptr;

Nodes::Nodes() {
    INodes = vector<INode *>();
    DeletedINodes = queue<fuse_ino_t>();
}

Nodes* Nodes::getInstance() {
    if (_instance == nullptr) {
        _instance = new Nodes();
    }
    return _instance;
}

INode* Nodes::getINodeByINodeNumber(fuse_ino_t inodeNumber) {
    return INodes[inodeNumber];
}

void Nodes::setINodeAt(fuse_ino_t inodeNummber,INode *inode) {
    INodes[inodeNummber] = inode;
}

fuse_ino_t Nodes::AddINode(INode* newInode) {
    INodes.push_back(newInode);
    fuse_ino_t newInodeNumber = INodes.size() - 1;
    return newInodeNumber;
}

INode* Nodes::createEmptyINode(INodeType type) {
    INode *newINode;

    switch (type) {
    case REGULAR_FILE:
            newINode = new File();
            break;
        case DIRECTORY:
            newINode = new Directory();
            break;
        case SYMBOLIC_LINK:
            newINode = new SymbolicLink();
            break;
        case SPECIAL_INODE_TYPE_NO_BLOCK:
            newINode = new SpecialINode(SPECIAL_INODE_TYPE_NO_BLOCK);
            break;
        default:
            newINode = nullptr;
            break;
    }

    return newINode;
}

void Nodes::InitializeINode(INode *inode, fuse_ino_t ino, mode_t mode, nlink_t nlink, gid_t gid, uid_t uid) {
    // TODO: Still not sure if I should use inode->m_fuseEntryParam = {}
    memset(&inode->m_fuseEntryParam, 0, sizeof(inode->m_fuseEntryParam));
    inode->inodeNumber = ino;

    inode->m_fuseEntryParam.ino = ino;
    inode->m_fuseEntryParam.attr_timeout = 1.0;
    inode->m_fuseEntryParam.entry_timeout = 1.0;
    inode->m_fuseEntryParam.attr.st_mode = mode;
    inode->m_fuseEntryParam.attr.st_gid = gid;
    inode->m_fuseEntryParam.attr.st_uid = uid;

    // Note this found on the Internet regarding nlink on dirs:
    // "For the root directory it is at least three; /, /., and /.. .
    // Make a directory /foo and /foo/.. will have the same inode number as /, incrementing st_nlink.
    //
    // Cheers, Ralph."
    inode->m_fuseEntryParam.attr.st_nlink = nlink;

    inode->m_fuseEntryParam.attr.st_blksize = Nodes::INodeBufBlockSize;

    timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    #ifdef __APPLE__
    inode->m_fuseEntryParam.attr.st_atimespec = ts;
    inode->m_fuseEntryParam.attr.st_ctimespec = ts;
    inode->m_fuseEntryParam.attr.st_birthtimespec = ts;
    inode->m_fuseEntryParam.attr.st_mtimespec = ts;
    #else
    inode->m_fuseEntryParam.attr.st_atim = ts;
    inode->m_fuseEntryParam.attr.st_ctim = ts;
    inode->m_fuseEntryParam.attr.st_mtim = ts;
    #endif
}

/**
 * Overloading.
 */
void Nodes::LookupINode(fuse_ino_t inodeNumber) {
    LookupINode(INodes[inodeNumber]);
}

/**
 * Overloading.
 */
void Nodes::LookupINode(INode* inode) {
    inode->Lookup();
}

/**
 * Overloading.
 */
void Nodes::Forget(fuse_ino_t inodeNumber, unsigned long nlookup) {
    Forget(INodes[inodeNumber],nlookup);
}

/**
 * Overloading.
 */
void Nodes::Forget(INode* inode, unsigned long nlookup) {
    inode->Forget(nlookup);
}

/**
 * The given inode is not effectively deleted: it's only marked as deleted for reclaiming it later.
 */
void Nodes::DeleteINode(fuse_ino_t inodeNumber) {
    DeletedINodes.push(inodeNumber);
}

/**
 * Using a queue will always give the smallest inode number to reuse when reclaiming.
 * When the inode number is reused, it will effectively.
 */
fuse_ino_t Nodes::reclaimINode() {
    fuse_ino_t ino = DeletedINodes.front();
    DeletedINodes.pop();
    return ino;
}

void Nodes::SetINodeAttributes(INode *inode, struct stat* attr, int to_set) {
    if (to_set & FUSE_SET_ATTR_MODE) {
        inode->m_fuseEntryParam.attr.st_mode = attr->st_mode;
    }
    if (to_set & FUSE_SET_ATTR_UID) {
        inode->m_fuseEntryParam.attr.st_uid = attr->st_uid;
    }
    if (to_set & FUSE_SET_ATTR_GID) {
        inode->m_fuseEntryParam.attr.st_gid = attr->st_gid;
    }
    if (to_set & FUSE_SET_ATTR_SIZE) {
        inode->m_fuseEntryParam.attr.st_size = attr->st_size;
    }
    if (to_set & FUSE_SET_ATTR_ATIME) {
        #ifdef __APPLE__
        m_fuseEntryParam.attr.st_atimespec = attr->st_atimespec;
        #else
        inode->m_fuseEntryParam.attr.st_atim = attr->st_atim;
        #endif
    }
    if (to_set & FUSE_SET_ATTR_MTIME) {
        #ifdef __APPLE__
        m_fuseEntryParam.attr.st_mtimespec = attr->st_mtimespec;
        #else
        inode->m_fuseEntryParam.attr.st_mtim = attr->st_mtim;
        #endif
    }
    #ifdef __APPLE__
    if (to_set & FUSE_SET_ATTR_CHGTIME) {
        m_fuseEntryParam.attr.st_ctimespec = attr->st_ctimespec;
    #else
    if (to_set & FUSE_SET_ATTR_CTIME) {
        inode->m_fuseEntryParam.attr.st_ctim = attr->st_ctim;
    #endif
    }

    #ifdef __APPLE__
    if (to_set & FUSE_SET_ATTR_CRTIME) {
        m_fuseEntryParam.attr.st_birthtimespec = attr->st_birthtimespec;
    }
    // TODO: Can't seem to find this one.
    //    if (to_set & FUSE_SET_ATTR_BKUPTIME) {
    //        m_fuseEntryParam.attr.st_ = attr->st_mode;
    //    }
    if (to_set & FUSE_SET_ATTR_FLAGS) {
        m_fuseEntryParam.attr.st_flags = attr->st_flags;
    }
    #endif /* __APPLE__ */

    // TODO: What do we do if this fails? Do we care? Log the event?
    #ifdef __APPLE__
    clock_gettime(CLOCK_REALTIME, &(m_fuseEntryParam.attr.st_ctimespec));
    #else
    clock_gettime(CLOCK_REALTIME, &(inode->m_fuseEntryParam.attr.st_ctim));
    #endif
}
