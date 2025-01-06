/** @file fuse_cpp_ramfs.cpp
 *  @copyright 2016 Peter Watkins. All rights reserved.
 */

//
// Created by frank on 11/29/24.
//

#include "FileSystem.hpp"
#include "../utils/ArgumentParser.hpp"

#include <iostream>
#include <cstdio>
#include <cstdlib>
#include <unistd.h>
#include <string>
#include <cstring>
#include <cassert>
#include <thread>

//For logging
#include "../utils/log_level.hpp"

using namespace std;

using namespace log4cplus;

bool FileSystem::m_reclaimingINodes = false;

struct statvfs FileSystem::m_stbuf = {};

struct fuse_lowlevel_ops FileSystem::FuseOperations = {};

Nodes *FileSystem::INodeManager = nullptr;

Blocks *FileSystem::BlocksManager = nullptr;

MasterProcessCode *FileSystem::MasterProcess = nullptr;

Logger FileSystem::FSLogger = Logger::getInstance("FuseFileSystem.logger - ");

int FileSystem::mpiWorldSize = 0;

/**
 * Constructor of our file system in RAM. It initializes all fuse operation to its methods.
 */
FileSystem::FileSystem(int rank, int mpi_world_size) {
    FuseOperations.init        = FileSystem::FuseInit;
    FuseOperations.getattr     = FileSystem::FuseGetAttr;
    FuseOperations.lookup      = FileSystem::FuseLookup;
    FuseOperations.forget      = FileSystem::FuseForget;
    FuseOperations.setattr     = FileSystem::FuseSetAttr;
    FuseOperations.readlink    = FileSystem::FuseReadLink;
    FuseOperations.mknod       = FileSystem::FuseMknod;
    FuseOperations.mkdir       = FileSystem::FuseMkdir;
    FuseOperations.unlink      = FileSystem::FuseUnlink;
    FuseOperations.rmdir       = FileSystem::FuseRmdir;
    FuseOperations.symlink     = FileSystem::FuseSymlink;
    FuseOperations.rename      = FileSystem::FuseRename;
    FuseOperations.link        = FileSystem::FuseLink;
    FuseOperations.open        = FileSystem::FuseOpen;
    FuseOperations.read        = FileSystem::FuseRead;
    FuseOperations.write       = FileSystem::FuseWrite;
    FuseOperations.flush       = FileSystem::FuseFlush;
    FuseOperations.release     = FileSystem::FuseRelease;
    FuseOperations.fsync       = FileSystem::FuseFsync;
    FuseOperations.opendir     = FileSystem::FuseOpenDir;
    FuseOperations.readdir     = FileSystem::FuseReadDir;
    FuseOperations.releasedir  = FileSystem::FuseReleaseDir;
    FuseOperations.fsyncdir    = FileSystem::FuseFsyncDir;
    FuseOperations.statfs      = FileSystem::FuseStatfs;
    FuseOperations.setxattr    = FileSystem::FuseSetXAttr;
    FuseOperations.getxattr    = FileSystem::FuseGetXAttr;
    FuseOperations.listxattr   = FileSystem::FuseListXAttr;
    FuseOperations.removexattr = FileSystem::FuseRemoveXAttr;
    FuseOperations.access      = FileSystem::FuseAccess;
    FuseOperations.create      = FileSystem::FuseCreate;
    FuseOperations.getlk       = FileSystem::FuseGetLock;

    m_stbuf.f_bsize   = Nodes::INodeBufBlockSize;      // File system block size
    m_stbuf.f_frsize  = Nodes::INodeBufBlockSize;      // Fundamental file system block size
    m_stbuf.f_blocks  = kTotalBlocks;          // Blocks on FS in units of f_frsize
    m_stbuf.f_bfree   = kTotalBlocks;          // Free blocks
    m_stbuf.f_bavail  = kTotalBlocks;          // Blocks available to non-root
    m_stbuf.f_files   = kTotalInodes;          // Total inodes
    m_stbuf.f_ffree   = kTotalInodes;          // Free inodes
    m_stbuf.f_favail  = kTotalInodes;          // Free inodes for non-root
    m_stbuf.f_fsid    = kFilesystemId;         // Filesystem ID
    m_stbuf.f_flag    = 0777;                  // Bit mask of values
    m_stbuf.f_namemax = kMaxFilenameLength;    // Max file name length

    INodeManager = Nodes::getInstance();
    BlocksManager = Blocks::getInstance();
    MasterProcess = MasterProcessCode::getInstance(rank, mpi_world_size);

    LogLevel ll = DAGONFS_LOG_LEVEL;
    FSLogger.setLogLevel(ll);
}

FileSystem::~FileSystem() {}

/**
 * Initialize the FUSE API connection and start the loop for the API calls. The parameters
 * are the same that are passed to main program by command line.
 */
int FileSystem::start(int argc,char *argv[]) {
    int ret = 0;
    //LIBFUSE
    //Argument copying for fuse args
    //This operation is a prevention: the program also uses MPI, for this
    //  reason it's better keep the original args untouched
    ArgumentParser parser = ArgumentParser();
    parser.copy_args(argc, argv);
    char **copied_argv_for_fuse = parser.getCopiedArgs();

    fuse_args args_for_fuse = FUSE_ARGS_INIT(argc, copied_argv_for_fuse);
    fuse_cmdline_opts fuse_options;

    //LIBFUSE
    //CLI arguments parsing to fill the options
    if(fuse_parse_cmdline(&args_for_fuse,&fuse_options) != 0){
        show_usage(argv[0]);
    }

    //LIBFUSE
    //Options control
    if(fuse_options.show_help){
        cout << "***** Command Line Help fuse fuse_cmdline_help() *****" << endl;
        fuse_cmdline_help();
        cout << "***** Low Level Help fuse_lowlevel_help() *****" << endl;
        fuse_lowlevel_help();
        return ret;
    }
    else{
        if(fuse_options.show_version){
            cout << "FUSE library version: " << fuse_pkgversion() << endl;
            return ret;
        }
    }

    //LIBFUSE
    //Check if the mountpoint is not null, without it the application must not start
    if(fuse_options.mountpoint == nullptr){
        LOG4CPLUS_ERROR(FSLogger, FSLogger.getName() <<  "options.mountpoint is NULL : " << fuse_options.mountpoint);
        ret = 2;
        return ret;
    }
    LOG4CPLUS_TRACE(FSLogger, FSLogger.getName() << "--> Step 1: options.mountpoint is not NULL: " << fuse_options.mountpoint);

    //LIBFUSE
    //Inizialize the session for low lvel API
    fuse_session *session = fuse_session_new(&args_for_fuse, &(FuseOperations),sizeof(FuseOperations),nullptr);
    if(session == nullptr) {
        LOG4CPLUS_ERROR(FSLogger, FSLogger.getName() <<  "fuse_session_new() failed");
        ret = -1;
        return ret;
    }
    LOG4CPLUS_TRACE(FSLogger, FSLogger.getName() <<"--> Step 2: fuse_Session_new() Success, fuse_session: " << session);

    //LIBFUSE
    //Setting signal handlers
    if(fuse_set_signal_handlers(session) == 0) {
        LOG4CPLUS_ERROR(FSLogger, FSLogger.getName() <<  "--> Step 3: fuse_signal_handlers() OK");

        //LIBFUSE
        //Mounting our file system
        if(fuse_session_mount(session,fuse_options.mountpoint) == 0) {
            LOG4CPLUS_TRACE(FSLogger, FSLogger.getName() << "--> Step 4: fuse_session_mount() Success");

            //LIBFUSE
            //Entering a single-block-event loop
            fuse_daemonize(fuse_options.foreground);
            ret = fuse_session_loop(session);
            LOG4CPLUS_TRACE(FSLogger, FSLogger.getName() << "***** Loop terminated *****");

            //LIBFUSE
            //Unmuont our file system
            fuse_session_unmount(session);
            LOG4CPLUS_TRACE(FSLogger, FSLogger.getName() << "--> Step 5: fuse_session_unmount() Success");
        }

        //LIBFUSE
        //Remove signal handlers previously setted
        fuse_remove_signal_handlers(session);
        LOG4CPLUS_TRACE(FSLogger, FSLogger.getName() << "--> Step 6: fuse_remove_signal_handlers() Success");
    }

    //LIBFUSE
    //Destruction of the session and releasing of the mountpoint
    fuse_session_exit(session);
    fuse_session_destroy(session);
    free(fuse_options.mountpoint);
    fuse_opt_free_args(&args_for_fuse);
    LOG4CPLUS_TRACE(FSLogger, FSLogger.getName() << "--> Step 7: fuse_session_destroy() Success");

    LOG4CPLUS_TRACE(FSLogger, FSLogger.getName() << "--> Step 8: tell other MPI process that the file system has been unmounted");
    MasterProcess->sendTermination();

    return ret;
}

void FileSystem::show_usage(const char *progname){
    printf("usage: %s [options] <mount point>\n\n",progname);
    printf("options\n"
            "       --version \tdisplay version information"
            "       -V\n"
            "       --help \t\tdisplay help information"
            "       --ho"
            "\n");
}

/**
 * Initializes the filesystem. Creates the root directory. The UID and GID are those of the creating process.
 */
void FileSystem::FuseInit(void *userdata, struct fuse_conn_info *conn) {
    LOG4CPLUS_TRACE(FSLogger, FSLogger.getName() <<  "Starting FuseInit()...");

    FileSystem::m_reclaimingINodes = false;

    m_stbuf.f_bfree  = m_stbuf.f_blocks;	//Free blocks
    m_stbuf.f_bavail = m_stbuf.f_blocks;	//Blocks available to non-root
    m_stbuf.f_ffree  = m_stbuf.f_files;     //Free inodes
    m_stbuf.f_favail = m_stbuf.f_files;	    //Free inodes for non-root
    m_stbuf.f_flag   = 0777;		        //Bit mask of values

    //For our root nodes, we'll set gid and uid to the ones the process is using.
    uid_t gid = getgid();
    //TODO: Should I be getting the effective UID instead?
    uid_t uid = getuid();

    //We start out with a special inode and a single directory (the root directory).
    LOG4CPLUS_TRACE(FSLogger, FSLogger.getName() << "Registering SpecialINode for deleting i-node...");
    RegisterINode(SPECIAL_INODE_TYPE_NO_BLOCK, 0777, 0, gid, uid);
    LOG4CPLUS_TRACE(FSLogger, FSLogger.getName() << "SpecialINode registered!");

    //I think that the root directory should have a hardlink count of 3.
    //This is what I believe I've surmised from reading around.
    LOG4CPLUS_TRACE(FSLogger, FSLogger.getName() << "Registering root directory");
    RegisterINode(DIRECTORY, S_IFDIR | 0777, 3, gid, uid);
    LOG4CPLUS_TRACE(FSLogger, FSLogger.getName() << "root directory registered!");

    LOG4CPLUS_TRACE(FSLogger, FSLogger.getName() << "FuseInit() completed!");

    MasterProcess->sendChangedir();
}

/**
 * This method create a new i-node and add it to the list of i-nodes of the file system.
 * The i-node number is generated in this method, and it's retured to the caller.
 */
fuse_ino_t FileSystem::RegisterINode(INodeType type, mode_t mode, nlink_t nlink, gid_t gid, uid_t uid) {
    //Stop reclaiming inodes if there are no more to reclaim.
    if (INodeManager->getNumberOfDeletedINodes() == 0) {
        m_reclaimingINodes = false;
    }

    INode *inode_p = INodeManager->createEmptyINode(type);

    //Either re-use a deleted inode or push one back depending on whether we're reclaiming inodes now or not
    fuse_ino_t ino;
    if (m_reclaimingINodes) {
        ino = INodeManager->reclaimINode();
        INode *del_p = INodeManager->getINodeByINodeNumber(ino);
        FileSystem::UpdateUsedBlocks(-(del_p->UsedBlocks())); //Operazione della struttura dati Blocks
        INodeManager->setINodeAt(ino,inode_p);
        delete del_p;
    } else {
        ino = INodeManager->AddINode(inode_p);
        FileSystem::UpdateUsedINodes(1); //Operazione della struttura dati Blocks
    }

    INodeManager->InitializeINode(inode_p, ino, mode, nlink, gid, uid);

    return ino;
}

void FileSystem::FuseGetAttr(fuse_req_t req, fuse_ino_t ino, struct fuse_file_info *fi){
    LOG4CPLUS_TRACE(FSLogger, FSLogger.getName() <<  "Getting Attributes -> FuseRamFs::FuseGetAttr()");
    //Fail if the inode hasn't been created yet
    if (ino >= INodeManager->getNumberOfINodes()) {
        fuse_reply_err(req, ENOENT);
    }

    //TODO: What do we do if the inode was deleted?
    INode *inode = INodeManager->getINodeByINodeNumber(ino);

    fuse_reply_attr(req, &(inode->m_fuseEntryParam.attr), 1.0);

    LOG4CPLUS_TRACE(FSLogger, FSLogger.getName() << "Getting Attributes -> FuseRamFs::FuseGetAttr() completed!");
}

void FileSystem::FuseLookup(fuse_req_t req, fuse_ino_t parent, const char* name) {
    LOG4CPLUS_TRACE(FSLogger, FSLogger.getName() <<  "Lookup -> FuseRamFs::FuseLookup()");

    if (parent >= INodeManager->getNumberOfINodes()) {
        fuse_reply_err(req, ENOENT);
        return;
    }

    INode *parentInode = INodeManager->getINodeByINodeNumber(parent);
    Directory *dir = dynamic_cast<Directory *>(parentInode);
    if (dir == nullptr) {
        // The parent wasn't a directory. It can't have any children.
        fuse_reply_err(req, ENOENT);
        return;
    }

    fuse_ino_t ino = dir->ChildINodeNumberWithName(string(name));
    if (ino == -1) {
        fuse_reply_err(req, ENOENT);
        return;
    }

    // TODO: What do we do if the inode was deleted?
    INode *inode = INodeManager->getINodeByINodeNumber(ino);
    INodeManager->LookupINode(ino);

    LOG4CPLUS_TRACE(FSLogger, FSLogger.getName() << "\tLookup for: " << ino << "-" << name << " nlookup++");
    fuse_reply_entry(req, &(inode->m_fuseEntryParam));

    LOG4CPLUS_TRACE(FSLogger, FSLogger.getName() << "Lookup -> FuseRamFs::FuseLookup() completed!");
}

/**
 * Check if the i-node is forgotten. If an i-node has no hard links, it's added to the deleted i-node list.
 * Check if the number of deleted i-node is greater than a threshold, the file system active reclaiming mode.
 */
void FileSystem::FuseForget(fuse_req_t req, fuse_ino_t ino, unsigned long nlookup) {
    LOG4CPLUS_TRACE(FSLogger, FSLogger.getName() <<  "Forgetting -> FuseRamFs::FuseForget()");
    //TODO: What if the inode doesn't exist!?!?!?!
    INode *inode_p = INodeManager->getINodeByINodeNumber(ino);

    LOG4CPLUS_TRACE(FSLogger, FSLogger.getName() <<  "\tforget per " << ino << ". nlookup -= " << nlookup);
    inode_p->Forget(nlookup);

    fuse_reply_none(req);

    if (inode_p->Forgotten()){
        LOG4CPLUS_TRACE(FSLogger, FSLogger.getName() <<  "\ti-node: " << inode_p << "forgotten");
        if (inode_p->HasNoLinks()){
            //Add the inode to the deleted list. Anything in this list may be reclaimed.
            INodeManager->DeleteINode(ino);
            if (INodeManager->getNumberOfDeletedINodes() > FileSystem::kINodeReclamationThreshold) {
                m_reclaimingINodes = true;
            }

            //At this point, directory entries will still point to this inode. This is
            //OK since we'll update those entries as soon as someone reads the directory.
            LOG4CPLUS_TRACE(FSLogger, FSLogger.getName() << "\ti-node " << ino << " aggiunto alla delete list");
        }
        else{
            // TODO: Verify that this only happens on unmount. It's OK on unmount but bad elsewhere.
            LOG4CPLUS_TRACE(FSLogger, FSLogger.getName() << "\ti-node " << ino << " è stato forgotten ma non eliminato");
        }
    }

    //Note that there's no reply here. That was done in the steps above this check.
    LOG4CPLUS_TRACE(FSLogger, FSLogger.getName() << "Forgetting -> FuseRamFs::FuseForget() completed!");
}

void FileSystem::FuseSetAttr(fuse_req_t req, fuse_ino_t ino, struct stat* attr, int to_set, struct fuse_file_info* fi) {
    LOG4CPLUS_TRACE(FSLogger, FSLogger.getName() << "Setting -> FuseRamFs::FuseSetAttr()");

    // Fail if the inode hasn't been created yet
    if (ino >= INodeManager->getNumberOfINodes()) {
        fuse_reply_err(req, ENOENT);
    }

    // TODO: What do we do if the inode was deleted?
    INode *inode = INodeManager->getINodeByINodeNumber(ino);

    LOG4CPLUS_TRACE(FSLogger, FSLogger.getName() << "\tsetattr per: " << ino);
    INodeManager->SetINodeAttributes(inode, attr, to_set);
    fuse_reply_attr(req, &(inode->m_fuseEntryParam.attr), 1.0);

    LOG4CPLUS_TRACE(FSLogger, FSLogger.getName() << "Setting -> FuseRamFs::FuseSetAttr() completed!");
}

void FileSystem::FuseReadLink(fuse_req_t req, fuse_ino_t ino) {
    LOG4CPLUS_TRACE(FSLogger, FSLogger.getName() << "Reading Link -> FuseRamFs::FuseReadLink()");

    if (ino >= INodeManager->getNumberOfINodes()) {
        fuse_reply_err(req, ENOENT);
        return;
    }

    INode *inode_p = INodeManager->getINodeByINodeNumber(ino);

    // You can only readlink on a symlink
    SymbolicLink *link_p = dynamic_cast<SymbolicLink *>(inode_p);
    if (link_p == nullptr) {
        fuse_reply_err(req, EPERM);
        return;
    }

    // TODO: Handle permissions.
    //    else if ((fi->flags & 3) != O_RDONLY)
    //        fuse_reply_err(req, EACCES);

    // TODO: Is reply_entry only for directories? What about files?
    LOG4CPLUS_TRACE(FSLogger, FSLogger.getName() <<  "\treadlink per: " << ino);

    fuse_reply_readlink(req, link_p->Link().c_str());

    LOG4CPLUS_TRACE(FSLogger, FSLogger.getName() << "Reading Link -> FuseRamFs::FuseReadLink() completed!");
}

void FileSystem::FuseMknod(fuse_req_t req, fuse_ino_t parent, const char* name, mode_t mode, dev_t rdev) {
    LOG4CPLUS_TRACE(FSLogger, FSLogger.getName() << "Making node -> FuseRamFs::FuseMknod()");

    if (parent >= INodeManager->getNumberOfINodes()) {
        fuse_reply_err(req, ENOENT);
        return;
    }

    INode *parentInode = INodeManager->getINodeByINodeNumber(parent);

    // You can only make something inside a directory
    Directory *parentDir_p = dynamic_cast<Directory *>(parentInode);
    if (parentDir_p == nullptr) {
        fuse_reply_err(req, EISDIR);
        return;
    }

    string tmp_string = string(name);
    if (tmp_string.length() > kMaxFilenameLength) {
        fuse_reply_err(req, ENAMETOOLONG);
        return;
    }

    // TODO: Handle permissions on dirs. You can't just create anything you please!:
    //    else if ((fi->flags & 3) != O_RDONLY)
    //        fuse_reply_err(req, EACCES);

    const struct fuse_ctx* ctx_p = fuse_req_ctx(req);

    INodeType inode_type;
    nlink_t nlink = 0;
    if (S_ISDIR(mode)) {
        //inode_p = new Directory();
        inode_type = DIRECTORY;
        nlink = 2;
        mode = S_IFDIR;

        // Update the number of hardlinks in the parent dir
        parentDir_p->AddHardLink();
    } else if (S_ISREG(mode)) {
        //inode_p = new File();
        inode_type = REGULAR_FILE;
        nlink = 1;
        mode = S_IFREG;
    } else {
        // TODO: Handle
        // S_ISBLK
        // S_ISCHR
        // S_ISDIR
        // S_ISFIFO
        // S_ISREG
        // S_ISLNK
        // S_ISSOCK
        // ...instead of returning this error.
        fuse_reply_err(req, ENOENT);
        return;
    }

    fuse_ino_t ino = RegisterINode(inode_type,mode | 0777, nlink, ctx_p->uid, ctx_p->gid);
    INode *inode_p = INodeManager->getINodeByINodeNumber(ino);

    // TODO: Handle: S_ISCHR S_ISBLK S_ISFIFO S_ISLNK S_ISSOCK S_TYPEISMQ S_TYPEISSEM S_TYPEISSHM
    assert(inode_p != nullptr);

    // Insert the inode into the directory. TODO: What if it already exists?
    parentDir_p->UpdateChild(string(name), ino);

    // TODO: Is reply_entry only for directories? What about files?
    LOG4CPLUS_TRACE(FSLogger, FSLogger.getName() << "\tmknod for " << ino << ". nlookup++");
    inode_p->Lookup();
    fuse_reply_entry(req, &(inode_p->m_fuseEntryParam));

    LOG4CPLUS_TRACE(FSLogger, FSLogger.getName() << "Making node -> FuseRamFs::FuseMknod() completed!");
}

void FileSystem::FuseMkdir(fuse_req_t req, fuse_ino_t parent, const char* name, mode_t mode) {
    LOG4CPLUS_TRACE(FSLogger, FSLogger.getName() << "Making directory -> FuseRamFs::FuseMkdir()");

    if (parent >= INodeManager->getNumberOfINodes()) {
        fuse_reply_err(req, ENOENT);
        return;
    }

    INode *parentInode = INodeManager->getINodeByINodeNumber(parent);

    // You can only make something inside a directory
    Directory *parentDir_p = dynamic_cast<Directory *>(parentInode);
    if (parentDir_p == nullptr) {
        fuse_reply_err(req, EISDIR);
        return;
    }

    // TODO: Handle permissions on dirs. You can't just create anything you please!:
    //    else if ((fi->flags & 3) != O_RDONLY)
    //        fuse_reply_err(req, EACCES);

    fuse_ino_t ino = RegisterINode(DIRECTORY, S_IFDIR | 0777, 2, getgid(), getuid());
    Directory *dir_p = dynamic_cast<Directory *>(INodeManager->getINodeByINodeNumber(ino));

    // Insert the inode into the directory. TODO: What if it already exists?
    parentDir_p->UpdateChild(string(name), ino);

    // Update the number of hardlinks in the parent dir
    parentDir_p->AddHardLink();

    // TODO: Is reply_entry only for directories? What about files?
    LOG4CPLUS_TRACE(FSLogger, FSLogger.getName() << "\tmkdir for " << ino << ". nlookup++");
    dir_p->Lookup();
    fuse_reply_entry(req, &(dir_p->m_fuseEntryParam));

    LOG4CPLUS_TRACE(FSLogger, FSLogger.getName() << "\tAll child of parentDir: " << parentDir_p);
    for (auto child: parentDir_p->Children()) {
        LOG4CPLUS_TRACE(FSLogger, FSLogger.getName() << "\tname='"<<child.first<<"',ino="<< child.second);
    }

    cout << "Making directory -> FuseRamFs::FuseMkdir() completed!" <<endl;
}

/**
 * Unlink a given i-node. If the given i-node reaches 0 hard links, is marked as deleted i-node.
 * The reference in the parent directory it's eliminated anyways.
 */
void FileSystem::FuseUnlink(fuse_req_t req, fuse_ino_t parent, const char* name) {
    LOG4CPLUS_TRACE(FSLogger, FSLogger.getName() << "Unlinking inode-> FuseRamFs::FuseUnlink()");

    LOG4CPLUS_TRACE(FSLogger, FSLogger.getName() << "\tunlink per: " << name << " nella directory padre: " << parent);

    // TODO: Node may also be deleted.
    if (parent >= INodeManager->getNumberOfINodes()) {
        fuse_reply_err(req, ENOENT);
        return;
    }

    INode *parentInode = INodeManager->getINodeByINodeNumber(parent);

    // You can only delete something inside a directory
    Directory *parentDir_p = dynamic_cast<Directory *>(parentInode);
    if (parentDir_p == nullptr) {
        fuse_reply_err(req, EISDIR);
        return;
    }

    // TODO: Handle permissions on dirs. You can't just delete anything you please!:
    //    else if ((fi->flags & 3) != O_RDONLY)
    //        fuse_reply_err(req, EACCES);

    // Return an error if the child doesn't exist.
    fuse_ino_t ino = parentDir_p->ChildINodeNumberWithName(string(name));
    if (ino == -1) {
        fuse_reply_err(req, ENOENT);
        return;
    }

    INode *inode_p = INodeManager->getINodeByINodeNumber(ino);
    // TODO: Any way we can fail here? What if the inode doesn't exist? That probably indicates
    // a problem that happened earlier.

    // Update the number of hardlinks in the target
    inode_p->RemoveHardLink();
    parentDir_p->DeleteChild(string(name));

    // Reply with no error. TODO: Where is ESUCCESS?
    fuse_reply_err(req, 0);

    LOG4CPLUS_TRACE(FSLogger, FSLogger.getName() << "Unlinking inode-> FuseRamFs::FuseUnlink() completed!");
}

/**
 * Decrease to 0 the inode hard links count and delete the directory form the children list of the parent directory.
 */
void FileSystem::FuseRmdir(fuse_req_t req, fuse_ino_t parent, const char *name) {
    LOG4CPLUS_TRACE(FSLogger, FSLogger.getName() << "Removing directory -> FuseRamFs::FuseRmdir");

    if (parent >= INodeManager->getNumberOfINodes()) {
        LOG4CPLUS_TRACE(FSLogger, FSLogger.getName() << "\tparent >= INodes.size()");
        fuse_reply_err(req, ENOENT);
        return;
    }

    INode *parentInode = INodeManager->getINodeByINodeNumber(parent);

    // You can only delete something inside a directory
    Directory *parentDir_p = dynamic_cast<Directory *>(parentInode);
    if (parentDir_p == nullptr) {
        LOG4CPLUS_ERROR(FSLogger, FSLogger.getName() << "\tparentDir_p == nullptr");
        fuse_reply_err(req, EISDIR);
        return;
    }

    // TODO: Handle permissions on dirs. You can't just delete anything you please!:
    //    else if ((fi->flags & 3) != O_RDONLY)
    //        fuse_reply_err(req, EACCES);

    // Return an error if the child doesn't exist.
    fuse_ino_t ino = parentDir_p->ChildINodeNumberWithName(string(name));
    if (ino == -1) {
        LOG4CPLUS_ERROR(FSLogger, FSLogger.getName() << "\tino == -1");
        fuse_reply_err(req, ENOENT);
        return;
    }

    INode *inode_p = INodeManager->getINodeByINodeNumber(ino);
    // TODO: Any way we can fail here? What if the inode doesn't exist? That probably indicates
    // a problem that happened earlier.

    Directory *dir_p = dynamic_cast<Directory *>(inode_p);
    if (dir_p == nullptr) {
        LOG4CPLUS_ERROR(FSLogger, FSLogger.getName() << "\tdir_p == nullptr");
        // Someone tried to rmdir on something that wasn't a directory.
        fuse_reply_err(req, EISDIR);
        return;
    }

    // Remove the directory only if is empty
    if (dir_p->hasChildren()) {
        LOG4CPLUS_ERROR(FSLogger, FSLogger.getName() << "\tDirectory contains children");
        fuse_reply_err(req, EPERM);
        return;
    }

    // Update the number of hardlinks in the parent dir
    parentDir_p->RemoveHardLink();

    // Remove the hard links to this dir so it can be cleaned up later
    // TODO: What if there's a real hardlink to this dir? Hardlinks to dirs allowed?
    while (!dir_p->HasNoLinks()) {
        dir_p->RemoveHardLink();
    }

    parentDir_p->DeleteChild(string(name));

    // Reply with no error. TODO: Where is ESUCCESS?
    fuse_reply_err(req, 0);

    LOG4CPLUS_TRACE(FSLogger, FSLogger.getName() << "Removing directory -> FuseRamFs::FuseRmdir completed!");
}

void FileSystem::FuseSymlink(fuse_req_t req, const char* link, fuse_ino_t parent, const char* name) {
    LOG4CPLUS_TRACE(FSLogger, FSLogger.getName() <<  "Creating symbolic link -> FuseRamFs::FuseSymlink");
    if (parent >= INodeManager->getNumberOfINodes()) {
        fuse_reply_err(req, ENOENT);
        return;
    }

    INode *parent_p = INodeManager->getINodeByINodeNumber(parent);

    // You can only make something inside a directory
    Directory *dir = dynamic_cast<Directory *>(parent_p);
    if (dir == nullptr) {
        fuse_reply_err(req, EISDIR);
        return;
    }

    // TODO: Handle permissions on dirs. You can't just make symlinks anywhere:
    //    else if ((fi->flags & 3) != O_RDONLY)
    //        fuse_reply_err(req, EACCES);

    const struct fuse_ctx* ctx_p = fuse_req_ctx(req);

    fuse_ino_t ino = RegisterINode(SYMBOLIC_LINK, S_IFLNK | 0755, 1, ctx_p->gid, ctx_p->uid);
    SymbolicLink *symLink = dynamic_cast<SymbolicLink *>(INodeManager->getINodeByINodeNumber(ino));
    symLink->setLink(link);
    INode *inode_p = symLink;

    // Insert the inode into the directory. TODO: What if it already exists?
    dir->UpdateChild(string(name), ino);

    // TODO: Is reply_entry only for directories? What about files?
    LOG4CPLUS_TRACE(FSLogger, FSLogger.getName() <<  "\tsymlink for " << ino << ". nlookup++");
    inode_p->Lookup();
    fuse_reply_entry(req,&(inode_p->m_fuseEntryParam));

    LOG4CPLUS_TRACE(FSLogger, FSLogger.getName() << "Creating symbolic link -> FuseRamFs::FuseSymlink completed!");
}

void FileSystem::FuseRename(fuse_req_t req, fuse_ino_t parent, const char* name, fuse_ino_t newparent, const char* newname, unsigned int flags) {
    LOG4CPLUS_TRACE(FSLogger, FSLogger.getName() << "Renaming new inode -> FuseRamFs::FuseRename()");
    // Make sure the parent still exists.
    if (parent >= INodeManager->getNumberOfINodes()) {
        fuse_reply_err(req, ENOENT);
        return;
    }

    INode *parentInode = INodeManager->getINodeByINodeNumber(parent);

    // You can only rename something inside a directory
    Directory *parentDir = dynamic_cast<Directory *>(parentInode);
    if (parentDir == nullptr) {
        fuse_reply_err(req, EISDIR);
        return;
    }

    // TODO: Handle permissions on dirs. You can't just rename anything you please!:
    //    else if ((fi->flags & 3) != O_RDONLY)
    //        fuse_reply_err(req, EACCES);

    // Return an error if the child doesn't exist.
    fuse_ino_t ino = parentDir->ChildINodeNumberWithName(string(name));
    if (ino == -1) {
        fuse_reply_err(req, ENOENT);
        return;
    }

    // Make sure the new parent still exists.
    if (newparent >= INodeManager->getNumberOfINodes()) {
        fuse_reply_err(req, ENOENT);
        return;
    }

    INode *newParentInode = INodeManager->getINodeByINodeNumber(newparent);

    // The new parent must be a directory. TODO: Do we need this check? Will FUSE
    // ever give us a parent that isn't a dir? Test this.
    Directory *newParentDir = dynamic_cast<Directory *>(newParentInode);
    if (newParentDir == nullptr) {
        fuse_reply_err(req, EISDIR);
        return;
    }

    // Look for an existing child with the same name in the new parent
    // directory
    fuse_ino_t existingIno = newParentDir->ChildINodeNumberWithName(string(newname));
    // Type is unsigned so we have to explicitly check for largest value. TODO: Refactor please.
    if (existingIno != -1 && existingIno > 0) {
        // There's already a child with that name. Replace it.
        // TODO: What about directories with the same name?
        INode *existingInode_p = dynamic_cast<Directory *>(INodeManager->getINodeByINodeNumber(existingIno));
        if (existingInode_p != nullptr) {
            parentDir->RemoveHardLink();
            LOG4CPLUS_TRACE(FSLogger, FSLogger.getName() << "\tRemoving hard link to " << existingIno);
            newParentDir->AddHardLink();
            LOG4CPLUS_TRACE(FSLogger, FSLogger.getName() << "\tAdding hard link to " << existingIno);
        }
    }

    // Update (or create) the new name and point it to the inode.
    newParentDir->UpdateChild(string(newname), ino);

    // Mark the old name as unused. TODO: Should we just delete the old name?
    parentDir->DeleteChild(string(name));

    LOG4CPLUS_TRACE(FSLogger, FSLogger.getName() << "\tRename " << name << " in " << parent << " to " << newname << " in " << newparent);
    fuse_reply_err(req, 0);

    LOG4CPLUS_TRACE(FSLogger, FSLogger.getName() << "Renaming new inode -> FuseRamFs::FuseRename() completed!");
}

void FileSystem::FuseLink(fuse_req_t req, fuse_ino_t ino, fuse_ino_t newparent, const char* newname) {
    LOG4CPLUS_TRACE(FSLogger, FSLogger.getName() <<  "Creating hard link -> FuseRamFs::FuseLink");

    // Make sure the new parent still exists.
    if (newparent >= INodeManager->getNumberOfINodes()) {
        fuse_reply_err(req, ENOENT);
        return;
    }

    INode *inode_p = INodeManager->getINodeByINodeNumber(newparent);

    // The new parent must be a directory. TODO: Do we need this check? Will FUSE
    // ever give us a parent that isn't a dir? Test this.
    Directory *newParentDir_p = dynamic_cast<Directory *>(inode_p);
    if (newParentDir_p == nullptr) {
        fuse_reply_err(req, EISDIR);
        return;
    }

    // Make target still exists.
    if (ino >= INodeManager->getNumberOfINodes()) {
        fuse_reply_err(req, ENOENT);
        return;
    }

    inode_p = INodeManager->getINodeByINodeNumber(ino);

    // Look for an existing child with the same name in the new parent
    // directory
    fuse_ino_t existingIno = newParentDir_p->ChildINodeNumberWithName(string(newname));
    // Type is unsigned so we have to explicitly check for largest value. TODO: Refactor please.
    if (existingIno != -1 && existingIno > 0) {
        // There's already a child with that name. Return an error.
        fuse_reply_err(req, EEXIST);
    }

    // Create the new name and point it to the inode.
    newParentDir_p->UpdateChild(string(newname), ino);

    // Update the number of hardlinks in the target
    inode_p->AddHardLink();

    LOG4CPLUS_TRACE(FSLogger, FSLogger.getName() << "\tlink " << newname << " in " << newparent << " to " << ino);
    inode_p->Lookup();
    fuse_reply_entry(req, &(inode_p->m_fuseEntryParam));
    LOG4CPLUS_TRACE(FSLogger, FSLogger.getName() << "Creating hard link -> FuseRamFs::FuseLink completed");
}

void FileSystem::FuseOpen(fuse_req_t req, fuse_ino_t ino, struct fuse_file_info* fi) {
    LOG4CPLUS_TRACE(FSLogger, FSLogger.getName() << "Opening file -> FuseRamFs::FuseOpen");

    // TODO: Node may also be deleted.
    if (ino >= INodeManager->getNumberOfINodes()) {
        fuse_reply_err(req, ENOENT);
        return;
    }

    INode *inode = INodeManager->getINodeByINodeNumber(ino);

    // You can't open a dir with 'open'. Check for this.
    Directory *dir = dynamic_cast<Directory *>(inode);
    if (dir != nullptr) {
        fuse_reply_err(req, EISDIR);
        return;
    }

    File *file_p = dynamic_cast<File *>(inode);
    if (file_p == nullptr) {
        fuse_reply_err(req, EPERM);
        return;
    }

    // TODO: Handle permissions on files:
    //    else if ((fi->flags & 3) != O_RDONLY)
    //        fuse_reply_err(req, EACCES);
    if ( fi->flags & (O_WRONLY | O_TRUNC) ) {
        LOG4CPLUS_DEBUG(FSLogger, FSLogger.getName() << "\tFile opened in write only mode or with O_TRUNC mode, the content must be deleted");
        file_p->m_fuseEntryParam.attr.st_size = 0;
        file_p->m_fuseEntryParam.attr.st_blocks = 0;
    }
    else {
        LOG4CPLUS_DEBUG(FSLogger, FSLogger.getName() << "\tFile opened in read and write or a mode that not erase the file content, the content must be loaded");
        MasterProcess->sendReadRequest();
        file_p->m_buf = MasterProcess->DAGonFS_Read(ino,
                                                    file_p->m_fuseEntryParam.attr.st_size,
                                                    file_p->m_fuseEntryParam.attr.st_size,
                                                    0);
    }

    // TODO: We seem to be able to delete a file and copy it back without a new inode being created. The only evidence is the open call. How do we handle this?

    LOG4CPLUS_TRACE(FSLogger, FSLogger.getName() << "\topen for " << ino << ". with flags " << fi->flags);

    fuse_reply_open(req, fi);

    LOG4CPLUS_TRACE(FSLogger, FSLogger.getName() << "Opening file -> FuseRamFs::FuseOpen completed!");
}

void FileSystem::FuseFlush(fuse_req_t req, fuse_ino_t ino, struct fuse_file_info* fi) {
    LOG4CPLUS_TRACE(FSLogger, FSLogger.getName() << "Flushing file -> FuseRamFs::FuseFlush");

    if (ino >= INodeManager->getNumberOfINodes()) {
        fuse_reply_err(req, ENOENT);
        return;
    }

    // TODO: Handle info in fi.

    cout << "flush for " << ino << endl;

    File *file_p = dynamic_cast<File *>(INodeManager->getINodeByINodeNumber(ino));
    if (file_p != nullptr && file_p->isWaitingForWriting()) {
        MasterProcess->sendWriteRequest();
        MasterProcess->DAGonFS_Write(file_p->m_buf, ino, file_p->m_fuseEntryParam.attr.st_size);
        file_p->removeWaiting();
        if (file_p->m_buf) {
            free(file_p->m_buf);
            file_p->m_buf = nullptr;
        }
    }

    fuse_reply_err(req, 0);

    LOG4CPLUS_TRACE(FSLogger, FSLogger.getName() << "Flushing file -> FuseRamFs::FuseFlush completed!");
}

/**
 * Invoked when there's no other references to the given inode.
 */
void FileSystem::FuseRelease(fuse_req_t req, fuse_ino_t ino, struct fuse_file_info* fi) {
    LOG4CPLUS_TRACE(FSLogger, FSLogger.getName() << "Releasing file -> FuseRamFs::FuseRelease completed!");

    if (ino >= INodeManager->getNumberOfINodes()) {
        fuse_reply_err(req, ENOENT);
        return;
    }

    INode *inode_p = INodeManager->getINodeByINodeNumber(ino);

    // You can't release a dir with 'close'. Check for this.
    Directory *dir = dynamic_cast<Directory *>(inode_p);
    if (dir != nullptr) {
        fuse_reply_err(req, EISDIR);
        return;
    }

    // TODO: Handle permissions on files:
    //    else if ((fi->flags & 3) != O_RDONLY)
    //        fuse_reply_err(req, EACCES);

    LOG4CPLUS_TRACE(FSLogger, FSLogger.getName() <<  "\trelease for " << ino);

    fuse_reply_err(req, 0);

    LOG4CPLUS_TRACE(FSLogger, FSLogger.getName() << "Releasing file -> FuseRamFs::FuseRelease completed!");
}

/**
 * Since the file system is in RAM, it's not necessary synchronize file content on HDD/SSD.
 */
void FileSystem::FuseFsync(fuse_req_t req, fuse_ino_t ino, int datasync, struct fuse_file_info* fi) {
    LOG4CPLUS_TRACE(FSLogger, FSLogger.getName() << "Synchronizing file -> FuseRamFs::Fsync");

    if (ino >= INodeManager->getNumberOfINodes()) {
        fuse_reply_err(req, ENOENT);
        return;
    }

    LOG4CPLUS_TRACE(FSLogger, FSLogger.getName() <<  "\tfysnc for " << ino);
    fuse_reply_err(req, 0);

    LOG4CPLUS_TRACE(FSLogger, FSLogger.getName() << "Synchronizing file -> FuseRamFs::Fsync completed");
}

void FileSystem::FuseOpenDir(fuse_req_t req, fuse_ino_t ino, struct fuse_file_info* fi) {
    LOG4CPLUS_TRACE(FSLogger, FSLogger.getName() << "Opening directory -> FuseRamFs::FuseOpenDir()");

    if (ino >= INodeManager->getNumberOfINodes()) {
        fuse_reply_err(req, ENOENT);
        return;
    }

    INode *inode = INodeManager->getINodeByINodeNumber(ino);

    // You can't open a file with 'opendir'. Check for this.
    File *file = dynamic_cast<File *>(inode);
    if (file != nullptr) {
        fuse_reply_err(req, ENOTDIR);
        return;
    }

    // TODO: Handle permissions on files:
    //    else if ((fi->flags & 3) != O_RDONLY)
    //        fuse_reply_err(req, EACCES);

    LOG4CPLUS_TRACE(FSLogger, FSLogger.getName() << "\topendir for " << ino);
    fuse_reply_open(req, fi);

    LOG4CPLUS_TRACE(FSLogger, FSLogger.getName() << "Opening directory -> FuseRamFs::FuseOpenDir() completed!");
}

/**
 * Since the file system is in RAM, all directory entries will show in the first readdir() API call.
 */
void FileSystem::FuseReadDir(fuse_req_t req, fuse_ino_t ino, size_t size, off_t off, struct fuse_file_info* fi) {
    LOG4CPLUS_TRACE(FSLogger, FSLogger.getName() << "Reading directory -> FuseRamFs::FuseReadDir()");

    (void) fi;

    LOG4CPLUS_TRACE(FSLogger, FSLogger.getName() << "\treaddir for ");

    size_t numInodes = INodeManager->getNumberOfINodes();
    // TODO: Node may also be deleted.
    if (ino >= numInodes) {
        fuse_reply_err(req, ENOENT);
        return;
    }

    INode *inode = INodeManager->getINodeByINodeNumber(ino);
    Directory *dir = dynamic_cast<Directory *>(inode);
    if (dir == nullptr) {
        fuse_reply_err(req, ENOTDIR);
        return;
    }

    map<string, fuse_ino_t>::const_iterator *childIterator = (map<string, fuse_ino_t>::const_iterator *) off;

    if (childIterator != nullptr && *childIterator == dir->Children().end()) {
        LOG4CPLUS_TRACE(FSLogger, FSLogger.getName() << "\tWas going to delete childIterator at " << (void *) childIterator << " pointing to " << (void *) &(**childIterator) << ". Let it leak instead");
        //delete childIterator;
        // This is the case where we've been called after we've sent all the children. End
        // with an empty buffer.
        fuse_reply_buf(req, NULL, 0);
        return;
    }

    // Loop through and put children into a buffer until we have either:
    // a) exceeded the passed in size parameter, or
    // b) filled the maximum number of children per response, or
    // c) come to the end of the directory listing
    //
    // In the case of (a), we won't know if this is the case until we've
    // added a child and exceeded the size. In that case, we need to back up.
    // In the process, we may end up exceeding our buffer size for this
    // resonse. In that case, increase the buffer size and add the child again.
    //
    // We must exercise care not to re-send children because one may have been
    // added in the middle of our map of children while we were sending them.
    // This is why we access the children with an iterator (instead of using
    // some sort of index).

    struct stat stbuf;
    memset(&stbuf, 0, sizeof(stbuf));

    // Pick the lesser of the max response size or our max size.
    size_t bufSize = FileSystem::kReadDirBufSize < size ? FileSystem::kReadDirBufSize : size;
    char *buf = (char *) malloc(bufSize);
    if (buf == NULL) {
        LOG4CPLUS_ERROR(FSLogger, FSLogger.getName() << "*** fatal error: cannot allocate memory");
        cerr << "*** fatal error: cannot allocate memory" << endl;
        abort();
    }

    // We'll assume that off is 0 when we start. This means that
    // childIterator hasn't been newed up yet.
    size_t bytesAdded = 0;
    size_t entriesAdded = 0;
    if (childIterator == nullptr) {
        childIterator = new map<string, fuse_ino_t>::const_iterator(dir->Children().begin());
        LOG4CPLUS_TRACE(FSLogger, FSLogger.getName() << "\twith new iterator at " << (void *) childIterator << " pointing to " << (void *) &(**childIterator));

        // Add . and .. - We'll assume that there's enough space in the buffer
        // to do this.
        stbuf.st_ino = ino;
        bytesAdded += fuse_add_direntry(req,
                                        buf + bytesAdded,
                                        bufSize - bytesAdded,
                                        ".",
                                        &stbuf,
                                        (off_t) childIterator);
        bytesAdded += fuse_add_direntry(req,
                                        buf + bytesAdded,
                                        bufSize - bytesAdded,
                                        "..",
                                        &stbuf,
                                        (off_t) childIterator);
        entriesAdded +=2;
    }

    while (entriesAdded < FileSystem::kReadDirEntriesPerResponse &&
           *childIterator != dir->Children().end()) {
        stbuf.st_ino = (*childIterator)->second;
        size_t oldSize = bytesAdded;
        //Add the directory entry only if the i-node still exists and hasn't been eliminated
        //inode = INodes[stbuf.st_ino];
        //if (!inode->isDeleted()) {
            // TODO: We don't look at sticky bits, etc. Revisit this in the future.
            //        Inode &childInode = Inodes[stbuf.st_ino];
            //        Directory *childDir = dynamic_cast<Directory *>(&childInode);
            //        if (childDir == NULL) {
            //            // This must be a file.
            //            stbuf.st_mode
            //        }
            bytesAdded += fuse_add_direntry(req,
                                        buf + bytesAdded,
                                        bufSize - bytesAdded,
                                        (*childIterator)->first.c_str(),
                                        &stbuf,
                                        (off_t) childIterator);
        //}
        if (bytesAdded > bufSize) {
            // Oops. There wasn't enough space for that last item. Back up and exit.
            --(*childIterator);
            LOG4CPLUS_TRACE(FSLogger, FSLogger.getName() << "\t. backed iterator at " << (void *) childIterator << " to point to " << (void *) &(**childIterator));
            bytesAdded = oldSize;
            break;
        }
        else {
            ++(*childIterator);
            LOG4CPLUS_TRACE(FSLogger, FSLogger.getName() << "\t. advanced iterator at " << (void *) childIterator << " to point to " << (void *) &(**childIterator));
            ++entriesAdded;
        }
    }

    cout << endl;
    fuse_reply_buf(req, buf, bytesAdded);
    if (buf) free(buf);

    LOG4CPLUS_TRACE(FSLogger, FSLogger.getName() << "Reading directory -> FuseRamFs::FuseReadDir() completed!");
}

/**
 * Invoked for every opendir() call.
 */
void FileSystem::FuseReleaseDir(fuse_req_t req, fuse_ino_t ino, struct fuse_file_info* fi) {
    LOG4CPLUS_TRACE(FSLogger, FSLogger.getName() <<  "Closing directory -> FuseRamFs::ReleaseDir");

    if (ino >= INodeManager->getNumberOfINodes()) {
        fuse_reply_err(req, ENOENT);
        return;
    }

    INode *inode = INodeManager->getINodeByINodeNumber(ino);

    // You can't close a file with 'closedir'. Check for this.
    File *file = dynamic_cast<File *>(inode);
    if (file != nullptr) {
        fuse_reply_err(req, ENOTDIR);
        return;
    }

    // TODO: Handle permissions on files:
    //    else if ((fi->flags & 3) != O_RDONLY)
    //        fuse_reply_err(req, EACCES);

    LOG4CPLUS_TRACE(FSLogger, FSLogger.getName() << "\treleasedir for " << ino);
    fuse_reply_err(req, 0);

    LOG4CPLUS_TRACE(FSLogger, FSLogger.getName() << "Closing directory -> FuseRamFs::ReleaseDir completed!");
}

/**
 * Since the file system is in RAM, it's not necessary synchronize directory content on HDD/SSD.
 */
void FileSystem::FuseFsyncDir(fuse_req_t req, fuse_ino_t ino, int datasync, struct fuse_file_info* fi) {
    LOG4CPLUS_TRACE(FSLogger, FSLogger.getName() <<  "Synchronizing directory -> FuseRamFs::FuseFsyncDir");

    if (ino >= INodeManager->getNumberOfINodes()) {
        fuse_reply_err(req, ENOENT);
        return;
    }

    LOG4CPLUS_TRACE(FSLogger, FSLogger.getName() <<  "\tfysncdir for " << ino);
    fuse_reply_err(req, 0);

    LOG4CPLUS_TRACE(FSLogger, FSLogger.getName() <<  "Synchronizing directory -> FuseRamFs::FuseFsyncDir completed");
}

/**
 * Get the file system information by filling the statvfs data structure (it is a member funciont of this class)
 */
void FileSystem::FuseStatfs(fuse_req_t req, fuse_ino_t ino) {
    LOG4CPLUS_TRACE(FSLogger, FSLogger.getName() << "Getting FS information -> FuseRam::FuseStatfs");
    // TODO: Why were we given an inode? What do we do with it?
    //    if (ino >= Inodes.size()) {
    //        fuse_reply_err(req, ENOENT);
    //        return;
    //    }

    LOG4CPLUS_TRACE(FSLogger, FSLogger.getName() <<  "\tstatfs for " << ino);

    fuse_reply_statfs(req, &m_stbuf);

    LOG4CPLUS_TRACE(FSLogger, FSLogger.getName() <<  "Getting FS information -> FuseRamFs::FuseStatfs completed");
}


#ifdef __APPLE__
void FileSystem::FuseSetXAttr(fuse_req_t req, fuse_ino_t ino, const char *name, const char *value, size_t size, int flags, uint32_t position)
#else
void FileSystem::FuseSetXAttr(fuse_req_t req, fuse_ino_t ino, const char *name, const char *value, size_t size, int flags)
#endif
{
    LOG4CPLUS_TRACE(FSLogger, FSLogger.getName() <<  "Setting " << name << "attribute with value " << value <<" -> FuseRamFs::FuseSetXAttr");
    if (ino >= INodeManager->getNumberOfINodes()) {
        fuse_reply_err(req, ENOENT);
        return;
    }

    INode *inode_p = INodeManager->getINodeByINodeNumber(ino);

    #ifndef __APPLE__
    uint32_t position = 0;
    #endif

    LOG4CPLUS_TRACE(FSLogger, FSLogger.getName() <<  "\tsetxattr for " << ino);

    int ret_val = inode_p->SetXAttr(string(name), value, size, flags, position);

    fuse_reply_err(req, ret_val);

    LOG4CPLUS_TRACE(FSLogger, FSLogger.getName() << "Setting " << name << "attribute with value " << value <<" -> FuseRamFs::FuseSetXAttr completed");
}

#ifdef __APPLE__
void FileSystem::FuseGetXAttr(fuse_req_t req, fuse_ino_t ino, const char *name, size_t size, uint32_t position)
#else
void FileSystem::FuseGetXAttr(fuse_req_t req, fuse_ino_t ino, const char *name, size_t size)
#endif
{
    LOG4CPLUS_TRACE(FSLogger, FSLogger.getName() << "Getting " << name << "attribute -> FuseRamFs::FuseSetXAttr");
    if (ino >= INodeManager->getNumberOfINodes()) {
        fuse_reply_err(req, ENOENT);
        return;
    }

    INode *inode_p = INodeManager->getINodeByINodeNumber(ino);

    #ifndef __APPLE__
    uint32_t position = 0;
    #endif

    LOG4CPLUS_TRACE(FSLogger, FSLogger.getName() <<  "\tgetxattr for " << ino);
    map<string, pair<void *, size_t> > xattrs = inode_p->GetXAttr();
    LOG4CPLUS_TRACE(FSLogger, FSLogger.getName() <<  "\tGetting "<< name << "attribute -> INode::GetXAttrAndReply");

    if (xattrs.find(name) == xattrs.end()) {
        LOG4CPLUS_ERROR(FSLogger, FSLogger.getName() <<  "xattr named '"<< name << "' not found");
        #ifdef __APPLE__
        fuse_reply_err(req, ENOATTR);
        #else
        fuse_reply_err(req, ENODATA);
        #endif
        return;
    }

    // The requestor wanted the size. TODO: How does position figure into this?
    if (size == 0) {
        fuse_reply_xattr(req, xattrs[name].second);
        return;
    }

    // TODO: What about overflow with size + position?
    size_t newExtent = size + position;

    // TODO: Is this the case where "the size is to small for the value"?
    if (xattrs[name].second < newExtent) {
        fuse_reply_err(req, ERANGE);
        return;
    }

    LOG4CPLUS_TRACE(FSLogger, FSLogger.getName() << "\tGetting "<< name << "attribute -> INode::GetXAttrAndReply completed!");

    // TODO: It's fine for someone to just read part of a value, right (i.e. size is less than m_xattr[name].second)?
    fuse_reply_buf(req, (char *) xattrs[name].first + position, size);

    LOG4CPLUS_TRACE(FSLogger, FSLogger.getName() <<  "Getting " << name << "attribute -> FuseRamFs::FuseSetXAttr completed");
}

void FileSystem::FuseListXAttr(fuse_req_t req, fuse_ino_t ino, size_t size) {
    LOG4CPLUS_TRACE(FSLogger, FSLogger.getName() << "Listing attributes -> FuseRamFs::FuseListXAttr");

    if (ino >= INodeManager->getNumberOfINodes()) {
        fuse_reply_err(req, ENOENT);
        return;
    }

    INode *inode_p = INodeManager->getINodeByINodeNumber(ino);

    LOG4CPLUS_TRACE(FSLogger, FSLogger.getName() <<  "\tlistxattr for " << ino);
    map<string, pair<void *, size_t> > xattrs = inode_p->GetXAttr();
    LOG4CPLUS_TRACE(FSLogger, FSLogger.getName() << "\tListing attributes -> INode::ListXAttrAndReply");
    size_t listSize = 0;
    for(map<string, pair<void *, size_t> >::iterator it = xattrs.begin(); it != xattrs.end(); it++) {
        listSize += (it->first.size() + 1);
    }

    // The requestor wanted the size
    if (size == 0) {
        fuse_reply_xattr(req, listSize);
    }

    // "If the size is too small for the list, the ERANGE error should be sent"
    if (size < listSize) {
        fuse_reply_err(req, ERANGE);
        return ; //A
    }

    // TODO: Is EIO really the best error to return if we ran out of memory?
    void *buf = malloc(listSize);
    if (buf == NULL) {
        fuse_reply_err(req, EIO);
        return ; //A
    }

    size_t position = 0;
    for(map<string, pair<void *, size_t> >::iterator it = xattrs.begin(); it != xattrs.end(); it++) {
        // Copy the name as well as the null termination character.
        memcpy((char *) buf + position, it->first.c_str(), it->first.size() + 1);
        position += (it->first.size() + 1);
    }

    fuse_reply_buf(req, (char *) buf, position);
    if (buf) free(buf);

    LOG4CPLUS_TRACE(FSLogger, FSLogger.getName() << "\tListing attributes -> INode::ListXAttrAndReply completed!");

    LOG4CPLUS_TRACE(FSLogger, FSLogger.getName() << "Listing attributes -> FuseRamFs::FuseListXAttr completed!");
}

void FileSystem::FuseRemoveXAttr(fuse_req_t req, fuse_ino_t ino, const char* name) {
    LOG4CPLUS_TRACE(FSLogger, FSLogger.getName() << "Removing " << name << "attribute -> FuseRamFs::FuseRemoveXAttr");
    if (ino >= INodeManager->getNumberOfINodes()) {
        fuse_reply_err(req, ENOENT);
        return;
    }

    INode *inode_p = INodeManager->getINodeByINodeNumber(ino);

    LOG4CPLUS_TRACE(FSLogger, FSLogger.getName() << "\tremovexattr for " << ino);
    int ret_val = inode_p->RemoveXAttr(string(name));

    fuse_reply_err(req,ret_val);

    LOG4CPLUS_TRACE(FSLogger, FSLogger.getName() << "Removing " << name << "attribute -> FuseRamFs::FuseRemoveXAttr completed!");
}

void FileSystem::FuseAccess(fuse_req_t req, fuse_ino_t ino, int mask) {
    LOG4CPLUS_TRACE(FSLogger, FSLogger.getName() << "Accessing -> FuseRamFs::FuseAccess");

    if (ino >= INodeManager->getNumberOfINodes()) {
        LOG4CPLUS_ERROR(FSLogger, FSLogger.getName() <<  "\tino >= size");
        fuse_reply_err(req, ENOENT);
        return;
    }

    INode *inode_p = INodeManager->getINodeByINodeNumber(ino);

    if (inode_p->isDeleted()) {
        LOG4CPLUS_ERROR(FSLogger, FSLogger.getName() << "Deleted inode");
        fuse_reply_err(req, ENOENT);
        return;
    }

    const struct fuse_ctx* ctx_p = fuse_req_ctx(req);

    LOG4CPLUS_TRACE(FSLogger, FSLogger.getName() <<  "\taccess for " << ino);

    // If all the user wanted was to know if the file existed, it does.
    if (mask == F_OK) {
        fuse_reply_err(req, 0);
        return;
    }

    // Check other
    if ((inode_p->m_fuseEntryParam.attr.st_mode & mask) == mask) {
        fuse_reply_err(req,0);
        return;
    }
    mask <<= 3;

    // Check group. TODO: What about other groups the user is in?
    if ((inode_p->m_fuseEntryParam.attr.st_mode & mask) == mask) {
        // Go ahead if the user's main group is the same as the file's
        if (ctx_p->gid == inode_p->m_fuseEntryParam.attr.st_gid) {
            fuse_reply_err(req,0);
            return;
        }

        // Now check the user's other groups. TODO: Where is this function?! not on this version of FUSE?
        // int numGroups = fuse_req_getgroups(req, 0, NULL);

    }
    mask <<= 3;

    // Check owner.
    if ((ctx_p->uid == inode_p->m_fuseEntryParam.attr.st_uid) && (inode_p->m_fuseEntryParam.attr.st_mode & mask) == mask) {
        fuse_reply_err(req,0);
        return;
    }

    fuse_reply_err(req, EACCES);

    LOG4CPLUS_TRACE(FSLogger, FSLogger.getName() <<  "Accessing -> FuseRamFs::FuseAccess completed!");
}

void FileSystem::FuseCreate(fuse_req_t req, fuse_ino_t parent, const char* name, mode_t mode, struct fuse_file_info* fi) {
    LOG4CPLUS_TRACE(FSLogger, FSLogger.getName() <<  "Creating " << name << " -> FuseRamFs::FuseCreate");

    if (parent >= INodeManager->getNumberOfINodes()) {
        fuse_reply_err(req, ENOENT);
        return;
    }

    INode *parent_p = INodeManager->getINodeByINodeNumber(parent);
    Directory *parentDir_p = dynamic_cast<Directory *>(parent_p);
    if (parentDir_p == nullptr) {
        // The parent wasn't a directory. It can't have any children.
        fuse_reply_err(req, ENOENT);
        return;
    }

    string tmp_string = string(name);
    if (tmp_string.length() > kMaxFilenameLength) {
        fuse_reply_err(req, ENAMETOOLONG);
        return;
    }

    const struct fuse_ctx* ctx_p = fuse_req_ctx(req);

    // TODO: It looks like, according to the documentation, that this will never be called to
    // make a dir--only a file. Test to make sure this is true.
    fuse_ino_t ino = RegisterINode(REGULAR_FILE, S_IFREG | 0777, 1, ctx_p->gid, ctx_p->uid);
    BlocksManager->createEmptyBlockListForInode(ino);
    INode *inode_p = INodeManager->getINodeByINodeNumber(ino);

    // Insert the inode into the directory. TODO: What if it already exists?
    parentDir_p->UpdateChild(string(name), ino);

    LOG4CPLUS_TRACE(FSLogger, FSLogger.getName() <<  "\tcreate for " << ino << " with name " << name << " in " << parent);
    inode_p->Lookup();
    if ( fi->flags & (O_WRONLY | O_TRUNC) ) {
        LOG4CPLUS_DEBUG(FSLogger, FSLogger.getName() << " File created with O_WRONLY | O_TRUNC");
        File *file_p = dynamic_cast<File *>(inode_p);
        if (file_p != nullptr) {
            file_p->m_fuseEntryParam.attr.st_size = 0;
            file_p->m_fuseEntryParam.attr.st_blocks = 0;
        }
    }
    fuse_reply_create(req, &(inode_p->m_fuseEntryParam), fi);

    LOG4CPLUS_TRACE(FSLogger, FSLogger.getName() << "Creating " << name << " -> FuseRamFs::FuseCreate completed!");

}

void FileSystem::FuseGetLock(fuse_req_t req, fuse_ino_t ino, struct fuse_file_info* fi, struct flock* lock) {
    LOG4CPLUS_TRACE(FSLogger, FSLogger.getName() <<  "Getting the lock -> FuseRamFs::FuseGetLock");

    if (ino >= INodeManager->getNumberOfINodes()) {
        fuse_reply_err(req, ENOENT);
        return;
    }

    INode *inode_p = INodeManager->getINodeByINodeNumber(ino);

    LOG4CPLUS_TRACE(FSLogger, FSLogger.getName() << "\tgetlk for " << ino);
    // TODO: implement locking

    LOG4CPLUS_TRACE(FSLogger, FSLogger.getName() << "Getting the lock -> FuseRamFs::FuseGetLock completed!");
}

/**
 * The data of the inode (when it's a File inode) are stored in its member attribute "m_buf", since
 * the file system is in RAM.
 */
void FileSystem::FuseRead(fuse_req_t req, fuse_ino_t ino, size_t size, off_t off, struct fuse_file_info* fi) {
    LOG4CPLUS_TRACE(FSLogger, FSLogger.getName() <<  "Reading " << ino << " -> FuseRamFs::FuseRead");

    // TODO: Node may also be deleted.
    if (ino >= INodeManager->getNumberOfINodes()) {
        fuse_reply_err(req, ENOENT);
        return;
    }

    INode *inode_p = INodeManager->getINodeByINodeNumber(ino);

    // TODO: Handle info in fi.

    LOG4CPLUS_TRACE(FSLogger, FSLogger.getName() << "\tread for " << size << " at " << off << " from " << ino);

    //Directory
    Directory *dir_p = dynamic_cast<Directory *>(inode_p);
    if (dir_p != nullptr) {
        fuse_reply_err(req, EISDIR);
        return;
    }

    //SpecialINode
    SpecialINode *special_p = dynamic_cast<SpecialINode *>(inode_p);
    if (special_p != nullptr) {
        fuse_reply_err(req, ENOENT);
        return;
    }

    //SymbolicLink
    SymbolicLink *symlink_p = dynamic_cast<SymbolicLink *>(inode_p);
    if (symlink_p != nullptr) {
        fuse_reply_err(req, EISDIR);
        return;
    }

    //File
    File *file_p = dynamic_cast<File *>(inode_p);

    // Don't start the read past our file size
    if (off > file_p->m_fuseEntryParam.attr.st_size) {
        fuse_reply_buf(req, (const char *) file_p->m_buf, 0);
    }

    // Update access time. TODO: This could get very intensive. Some
    // filesystems buffer this with options at mount time. Look into this.
    // TODO: What do we do if this fails? Do we care? Log the event?
    #ifdef __APPLE__
    clock_gettime(CLOCK_REALTIME, &(m_fuseEntryParam.attr.st_atimespec));
    #else
    clock_gettime(CLOCK_REALTIME, &(file_p->m_fuseEntryParam.attr.st_atim));
    #endif

    // Handle reading past the file size as well as inside the size.
    size_t bytesRead = off + size > file_p->m_fuseEntryParam.attr.st_size ? file_p->m_fuseEntryParam.attr.st_size - off : size;

    // TODO: There are all sorts of other replies. What about them?
    fuse_reply_buf(req, (const char *) file_p->m_buf + off, bytesRead);
    LOG4CPLUS_TRACE(FSLogger, FSLogger.getName() <<  "Reading " << ino << " -> FuseRamFs::FuseRead completed!");
}

/**
 * The data will be written in the inode member attribute "m_buf", since the file system is in RAM.
 */
void FileSystem::FuseWrite(fuse_req_t req, fuse_ino_t ino, const char* buf, size_t size, off_t off, struct fuse_file_info* fi) {
    LOG4CPLUS_TRACE(FSLogger, FSLogger.getName() << "Writing " << ino << " -> FuseRamFs::FuseWrite");

    // TODO: Fuse seems to have problems writing with a null (buf) buffer.
    if (buf == nullptr) {
        fuse_reply_err(req, EPERM);
        return;
    }

    // TODO: Node may also be deleted.
    if (ino >= INodeManager->getNumberOfINodes()) {
        fuse_reply_err(req, ENOENT);
        return;
    }

    INode *inode_p = INodeManager->getINodeByINodeNumber(ino);

    Directory *dir_p = dynamic_cast<Directory *>(inode_p);
    if (dir_p != nullptr) {
        fuse_reply_err(req, EISDIR);
        return;
    }
    SymbolicLink *symlink_p = dynamic_cast<SymbolicLink *>(inode_p);
    if (symlink_p != nullptr) {
        fuse_reply_err(req, ENOENT);
        return;
    }
    SpecialINode *special_p = dynamic_cast<SpecialINode *>(inode_p);
    if (special_p != nullptr) {
        fuse_reply_err(req, EISDIR);
        return;
    }

    File *file_p = dynamic_cast<File *>(inode_p);
    if (file_p == nullptr) {
        fuse_reply_err(req, ENOENT);
        return;
    }

    if (!file_p->isWaitingForWriting()) {
        file_p->setWaiting();
    }

    // TODO: Handle info in fi

    LOG4CPLUS_TRACE(FSLogger, FSLogger.getName() << "\tWrite request for " << size << " bytes at " << off << " to " << ino);
    //LOG4CPLUS_TRACE(FSLogger, FSLogger.getName() << "\tcontent: '" << buf << "'");

    //Allocate more memory if we don't have space.
    size_t newSize = off + size;
    size_t originalCapacity = Nodes::INodeBufBlockSize * file_p->m_fuseEntryParam.attr.st_blocks;
    if (newSize > originalCapacity) {
        size_t newBlocks = newSize/Nodes::INodeBufBlockSize + (newSize % Nodes::INodeBufBlockSize != 0);
        void *newBuf = realloc(file_p->m_buf, newBlocks * Nodes::INodeBufBlockSize);
        // If we ran out of memory, let the caller know that no bytes were
        // written.
        if (newBuf == nullptr) {
            fuse_reply_write(req, 0);
        }

        // Update our buffer size
        file_p->m_buf = newBuf;
        FileSystem::UpdateUsedBlocks(newBlocks - file_p->m_fuseEntryParam.attr.st_blocks);
        file_p->m_fuseEntryParam.attr.st_blocks = newBlocks;
    }

    // Write to the buffer. TODO: Check if SRC and DST overlap.
    // We assume that there can be no buffer overflow since realloc has already
    // been called with the new size and offset. If realloc failed, we wouldn't
    // be here.
    memcpy((char *) file_p->m_buf + off, buf, size);
    if (newSize > file_p->m_fuseEntryParam.attr.st_size) {
        file_p->m_fuseEntryParam.attr.st_size = newSize;
    }


    // TODO: What do we do if this fails? Do we care? Log the event?
    #ifdef __APPLE__
    clock_gettime(CLOCK_REALTIME, &(m_fuseEntryParam.attr.st_ctimespec));
    m_fuseEntryParam.attr.st_mtimespec = m_fuseEntryParam.attr.st_ctimespec;
    #else
    clock_gettime(CLOCK_REALTIME, &(file_p->m_fuseEntryParam.attr.st_ctim));
    file_p->m_fuseEntryParam.attr.st_mtim = file_p->m_fuseEntryParam.attr.st_ctim;
    #endif

    fuse_reply_write(req, size);

    LOG4CPLUS_TRACE(FSLogger, FSLogger.getName() << "Writing " << ino << " -> FuseRamFs::FuseWrite completed!");
}
