/** @file fuse_cpp_ramfs.hpp
 *  @copyright 2016 Peter Watkins. All rights reserved.
 */

//
// Created by frank on 11/29/24.
//

#ifndef FILESYSTEM_HPP
#define FILESYSTEM_HPP

#include "../utils/fuse_headers.hpp"
#include "../nodes/Nodes.hpp"

#include "../blocks/Blocks.hpp"
#include "../mpi/MasterProcessCode.hpp"

#include "../utils/log_level.hpp"

/**
 * @brief The main FUSE code repository.
 *
 * The main FUSE file system that intercepts system calls on the file system.
 * Every operation in the file system is implemented for working exclusively in RAM.
 */
class FileSystem {
private:
    //Attributes
    /**
     * True if the filesystem is reclaiming inodes at the present point in time.
     */
    static bool m_reclaimingINodes;
    /**
    * The constants defining the capabilities and sizes of the filesystem.
    */
    static struct statvfs m_stbuf;
    /**
     * The number of blocks for the RAM file system
     */
    static const fsblkcnt_t kTotalBlocks = ~0;
    /**
     * The total number of inodes
     */
    static const fsfilcnt_t kTotalInodes = ~0;
    /**
     * The file system id
     */
    static const unsigned long kFilesystemId = 0xc13f944870434d8f;
    /**
     * The maximum file name length, currently 1024 characters
     */
    static const size_t kMaxFilenameLength = 1024;
    /**
     * The maximum file path length, currently 4096 characters including null
     */
    static const size_t kMaxPathLength = 4096;
    /**
     * The threshold beyond which the file system starts to reclaim inodes labeled as deleted
     */
    static const size_t kINodeReclamationThreshold = 256;
    /**
     * The maximum buffer size to read a directory
     */
    static const size_t kReadDirBufSize = 384;
    /**
     * The maximum number of directory entries per response
     */
    static const size_t kReadDirEntriesPerResponse = 255;

    /**
     * Reference to  the inodes manager for instantiating and managing inodes
     */
    static Nodes *INodeManager;

    static Blocks *BlocksManager;

    static MasterProcessCode *MasterProcess;

    static log4cplus::Logger FSLogger;

    static int mpiWorldSize;

    //Methods
    /**
     * @brief Show the usage of the program.
     *
     * @param progname The name of the program, it's argv[0] passed by main program.
    */
    void show_usage(const char *progname);

    /**
     * @brief Create a new i-node and insert it into the ram file system.
     *
     * @param type The type of the i-node (e.g. directory, file, symbolic link, etc.).
     * @param mode The permission given to the new i-node.
     * @param nlink The starting number of hard links for the new i-node.
     * @param gid The process group id for the new i-node.
     * @param uid The process user id for the new i-node.
     * @return The i-node number of the new i-node.
     */
    static fuse_ino_t RegisterINode(INodeType type, mode_t mode, nlink_t nlink, gid_t gid, uid_t uid);
public:
    //Attributes
    /**
    * All the supported filesystem operations mapped to object-methods.
    */
    static struct fuse_lowlevel_ops FuseOperations;
	static FILE *timeFile1;
	static double startWriteTime;
	static double endWriteTime;
	static double startReadTime;
	static double endReadTime;
	static FILE *timeFile2;

    //Methods
    FileSystem(int rank, int mpi_world_size);
    ~FileSystem();

    void setMpiWorldSize(int size){ mpiWorldSize = size; }

    static int getMpiWorldSize() { return mpiWorldSize; }

    /**
     * @brief Ram file system Stating method.
     *
     * @param argc Arguments count.
     * @param argv Arguments list.
     * @return Final status of our file system.
     */
    int start(int argc,char *argv[]);

    //  FS operations
    /**
     * @brief The first function called by FUSE API.
     *
     * @param userdata Any user data carried through FUSE calls.
     * @param conn Information on the capabilities of the connection to FUSE.
     */
    static void FuseInit(void *userdata, struct fuse_conn_info *conn);

    /**
     * @brief Gets inode's attributes.
     *
     * @param req The FUSE request.
     * @param ino The inode to git the attributes from.
     * @param fi The file info (information about an open file).
     */
    static void FuseGetAttr(fuse_req_t req, fuse_ino_t ino,struct fuse_file_info *fi);

    /**
     * @brief Looks up an inode given a parent and the name of the inode.
     *
     * @param req The FUSE request.
     * @param parent The parent inode.
     * @param name The name of the child to look up.
     */
    static void FuseLookup(fuse_req_t req, fuse_ino_t parent, const char *name);

    /**
     * @brief Forget the given inode.
     *
     * @param req The FUSE request.
     * @param ino The inode number.
     * @param nlookup The number of the references that the kernel is currently forgetting.
     */
    static void FuseForget(fuse_req_t req, fuse_ino_t ino, unsigned long nlookup);

    /**
     * @brief Sets the given attributes on the given inode.
     *
     * @param req The FUSE request.
     * @param ino The inode.
     * @param attr The incoming attributes.
     * @param to_set A mask of all incoming attributes that should be applied to the inode.
     * @param fi The file info (information about an open file).
     */
    static void FuseSetAttr(fuse_req_t req, fuse_ino_t ino, struct stat *attr, int to_set, struct fuse_file_info *fi);

    /**
     * @brief Read a symbolic link given its inode.
     *
     * @param req The FUSE request.
     * @param ino The inode.
     */
    static void FuseReadLink(fuse_req_t req, fuse_ino_t ino);

    /**
     * @brief Create a generic inode given the parent inode.
     *
     * @param req The FUSE request.
     * @param parent The parent inode.
     * @param name The name of the new node.
     * @param mode The permissions.
     * @param rdev The device number (only for device file).
     */
    static void FuseMknod(fuse_req_t req, fuse_ino_t parent, const char *name, mode_t mode, dev_t rdev);

    /**
     * @brief Create a directory.
     *
     * @param req The FUSE request.
     * @param parent The parent inode.
     * @param name The name of the directory.
     * @param mode The permissions.
     */
    static void FuseMkdir(fuse_req_t req, fuse_ino_t parent, const char *name, mode_t mode);

    /**
     * @brief Unlink a given i-node
     *
     * @param req The FUSE request.
     * @param parent The parent inode.
     * @param name The name of the inode to be unliked.
     */
    static void FuseUnlink(fuse_req_t req, fuse_ino_t parent, const char *name);

    /**
     * @brief Removes a directory
     *
     * @param req The FUSE request.
     * @param parent The parent inode.
     * @param name The name of the directory to be removed.
     */
    static void FuseRmdir(fuse_req_t req, fuse_ino_t parent, const char *name);

    /**
     * @brief Create a symbolic link given the file name and the parent inode.
     *
     * @param req The FUSE request.
     * @param link The path of the file to link.
     * @param parent The parent inode
     * @param name The name of the link
     */
    static void FuseSymlink(fuse_req_t req, const char *link, fuse_ino_t parent, const char *name);

    /**
     * @brief Rename an existing inode
     *
     * @param req The FUSE request.
     * @param parent The parent inode.
     * @param name The name of the existing file.
     * @param newparent The new parent directory of the file.
     * @param newname The new file name.
     * @param flags The specification flag for exchange old file with new file or not (currently not used).
     */
    static void FuseRename(fuse_req_t req, fuse_ino_t parent, const char *name, fuse_ino_t newparent, const char *newname, unsigned int flags);

    /**
     * @brief Create a hard link to a given existing inode.
     *
     * @param req The FUSE request.
     * @param ino The inode.
     * @param newparent The new parent directory of the nw hard link
     * @param newname The new name for hard link
     */
    static void FuseLink(fuse_req_t req, fuse_ino_t ino, fuse_ino_t newparent, const char *newname);

    /**
     * @brief Open an inode
     *
     * @param req The FUSE request.
     * @param ino The inode.
     * @param fi The file info (information about an open file).
     */
    static void FuseOpen(fuse_req_t req, fuse_ino_t ino, struct fuse_file_info *fi);

    /**
     * @brief Flush method called on each opened file
     *
     * @param req The FUSE request.
     * @param ino The inode.
     * @param fi The file info (information about an open file).
     */
    static void FuseFlush(fuse_req_t req, fuse_ino_t ino, struct fuse_file_info *fi);

    /**
     * @brief Release an open file
     *
     * Invoked when there's no other references to the given inode.
     *
     * @param req The FUSE request.
     * @param ino The inode.
     * @param fi The file info (information about an open file).
     */
    static void FuseRelease(fuse_req_t req, fuse_ino_t ino, struct fuse_file_info *fi);

    /**
     * @brief Synchronize file contents
     *
     * @param req The FUSE request.
     * @param ino The inode.
     * @param datasync flag indicating if only data should be flushed.
     * @param fi The file info (information about an open file).
     */
    static void FuseFsync(fuse_req_t req, fuse_ino_t ino, int datasync, struct fuse_file_info *fi);

    /**
     * @brief Opens a directory.
     *
     * @param req The FUSE request.
     * @param ino The directory inode.
     * @param fi The file info (information about an open file).
     */
    static void FuseOpenDir(fuse_req_t req, fuse_ino_t ino, struct fuse_file_info *fi);

    /**
     * @breif Reads a directory.
     *
     * @param req The FUSE request.
     * @param ino The directory inode.
     * @param size The maximum response size.
     * @param off The offset into the list of children.
     * @param fi The file info (information about an open file).
     */
    static void FuseReadDir(fuse_req_t req, fuse_ino_t ino, size_t size, off_t off, struct fuse_file_info *fi);

    /**
     * @brief Release an open directory.
     *
     * Invoked for every opendir() call.
     *
     * @param req The FUSE request.
     * @param ino The inode.
     * @param fi The file info (information about an open file).
     */
    static void FuseReleaseDir(fuse_req_t req, fuse_ino_t ino, struct fuse_file_info *fi);
    /**
     * @brief Synchronize directory contents
     *
     * @param req The FUSE request.
     * @param ino The inode.
     * @param datasync flag indicating if only data should be flushed.
     * @param fi The file info (information about an open file).
     */
    static void FuseFsyncDir(fuse_req_t req, fuse_ino_t ino, int datasync, struct fuse_file_info *fi);

    /**
     * @brief Get the file system information.
     *
     * @param req The FUSE request.
     * @param ino The inode.
     */
    static void FuseStatfs(fuse_req_t req, fuse_ino_t ino);

    #ifdef __APPLE__
    static void FuseSetXAttr(fuse_req_t req, fuse_ino_t ino, const char *name, const char *value, size_t size, int flags, uint32_t position);
    #else
    /**
     * @brief Set the value of the extended attribute identified by its name.
     *
     * @param req The FUSE request.
     * @param ino The inode.
     * @param name The extended attribute's name.
     * @param value The extended attribute's value to be set.
     * @param size The size of the value.
     * @param flags The flags for creating or replacing the extended attribute.
     */
    static void FuseSetXAttr(fuse_req_t req, fuse_ino_t ino, const char *name, const char *value, size_t size, int flags);
    #endif

    #ifdef __APPLE__
    static void FuseGetXAttr(fuse_req_t req, fuse_ino_t ino, const char *name, size_t size, uint32_t position);
    #else
    /**
     * @brief Get the value of the given extended attribute's name.
     *
     * @param req The FUSE request.
     * @param ino The inode.
     * @param name The extended attribute's name.
     * @param size The extended attribute value's size.
     */
    static void FuseGetXAttr(fuse_req_t req, fuse_ino_t ino, const char *name, size_t size);
    #endif

    /**
     * @brief List extended attribute names
     *
     * @param req The FUSE request.
     * @param ino The inode.
     * @param size The maximum size of the list to send.
     */
    static void FuseListXAttr(fuse_req_t req, fuse_ino_t ino, size_t size);

    /**
     * @brief Remove an extended attribute given its name.
     *
     * @param req The FUSE request.
     * @param ino The inode.
     * @param name The name of the extended attribute to remove.
     */
    static void FuseRemoveXAttr(fuse_req_t req, fuse_ino_t ino, const char *name);

    /**
     * @brief Check file access permission on a given inode.
     *
     * @param req The FUSE request.
     * @param ino The inode.
     * @param mask Requested access mode.
     */
    static void FuseAccess(fuse_req_t req, fuse_ino_t ino, int mask);

    /**
     * @brief Create and open a file
     *
     * @param req The FUSE request.
     * @param parent The parent inode.
     * @param name The name of the new file.
     * @param mode The file type and mode with which to create the new file.
     * @param fi The file information (information of an open file).
     */
    static void FuseCreate(fuse_req_t req, fuse_ino_t parent, const char *name, mode_t mode, struct fuse_file_info *fi);

    /**
     * @brief Acquire, modify or release a mutex lock.
     *
     * @param req The FUSE request.
     * @param ino The inode.
     * @param fi The file information (information of an open file).
     * @param lock The locking operations
     */
    static void FuseGetLock(fuse_req_t req, fuse_ino_t ino, struct fuse_file_info *fi, struct flock *lock);

    /**
     * @brief Read data.
     *
     * @param req The FUSE request.
     * @param ino The inode.
     * @param size The number of bytes to read.
     * @param off The offset to read from.
     * @param fi The file information (information of an open file).
     */
    static void FuseRead(fuse_req_t req, fuse_ino_t ino, size_t size, off_t off, struct fuse_file_info *fi);

    /**
     * @brief Write data.
     *
     * @param req The FUSE request.
     * @param ino The inode.
     * @param buf The data to write.
     * @param size The number of bytes to write.
     * @param off The offset to write to.
     * @param fi The file information (information of an open file).
     */
    static void FuseWrite(fuse_req_t req, fuse_ino_t ino, const char *buf, size_t size, off_t off, struct fuse_file_info *fi);

    /**
     * @brief Update the number of used blocks decrementing the number of the free blocks
     *
     * @param blocksAdded
     */
    static void UpdateUsedBlocks(ssize_t blocksAdded) { m_stbuf.f_bfree -= blocksAdded; m_stbuf.f_bavail -= blocksAdded;}

    /**
     * @brief Update the number of used inode decrementing the number  of free inodes
     *
     * @param inodesAdded
     */
    static void UpdateUsedINodes(ssize_t inodesAdded) { m_stbuf.f_ffree -= inodesAdded; m_stbuf.f_favail -= inodesAdded;}
};

#endif //FILESYSTEM_HPP
