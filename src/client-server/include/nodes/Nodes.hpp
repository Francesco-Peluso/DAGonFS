//
// Created by frank on 11/29/24.
//

#ifndef NODES_HPP
#define NODES_HPP

#include "inodes_data_structures.hpp"

#include <vector>
#include <queue>

#include "../utils/fuse_headers.hpp"
#include "../blocks/data_blocks_info.hpp"

using namespace std;

/**
 * @brief The main inodes manager.
 *
 * Class that keep track of the active and deleted inodes in the system.
 * This singleton class allows to create, deleting and managing inodes and them information.
 */
class Nodes {
private:
    //Singleton
    /**
     * @brief Singleton static reference to this class.
     */
    static Nodes* _instance;

    /**
     * @brief Private Constructor for implementing Singleton.
     */
    Nodes();

    /**
     * @brief All the inode in  the file system.
     */
    vector<INode *> INodes;

    /**
     * @brief The inodes which have been deleted.
     */
    queue<fuse_ino_t> DeletedINodes;

public:
    /**
     * @brief The size of a file system block.
     */
    static const size_t INodeBufBlockSize = FILE_SYSTEM_SINGLE_BLOCK_SIZE;

    //Singleton
    /**
     * @brief The method which give the instance of the singleton.
     *
     * @return The existing instance of the singleton. If there isn't any instance, a new instance is provided.
     */
    static Nodes* getInstance();

    /**
     * @brief Get the inode object pointer of given inode number.
     *
     * @param inodeNumber The inode number.
     * @return The inode object pointer on success, nullptr otherwise.
     */
    INode *getINodeByINodeNumber(fuse_ino_t inodeNumber);

    /**
     * @brief Create a new inode of a given type without setting any attribute.
     *
     * @param type The type of the new inode.
     * @return A new inode object.
     */
    INode *createEmptyINode(INodeType type);

    /**
     * @brief Get the number of registered inode in file system including deleted inodes.
     *
     * @return The number of inode in the file system.
     */
    int getNumberOfINodes(){return INodes.size();}

    /**
     * @brief Get the number of deleted inodes.
     *
     * @return The number of deleted inodes.
     */
    int getNumberOfDeletedINodes(){ return DeletedINodes.size();};

    /**
     * @brief Set a new inode object to a previously used (and then deleted) inode number.
     *
     * @param inodeNumber The inode number of the inode to set.
     * @param inode The new inode.
     */
    void setINodeAt(fuse_ino_t inodeNumber, INode *inode);

    /**
     * @brief Add a new inode object to the inode list.
     *
     * @param newInode The new inode object reference.
     * @return The new inode number for the  inode.
     */
    fuse_ino_t AddINode(INode *newInode);

    //Methods
    /**
     * @brief Initialize an inode object pointer with the given information.
     *
     * @param inode The object pointer of the new inode to initialize.
     * @param ino The inode number of the new inode.
     * @param mode The permissions of the new inode.
     * @param nlink The number of hard links for the new inode.
     * @param gid The group-id of the new inode.
     * @param uid The user-id of the nuw inode.
     */
    void InitializeINode(INode *inode, fuse_ino_t ino, mode_t mode, nlink_t nlink, gid_t gid, uid_t uid);

    /**
     * @brief Increase the number of reference of the given inode.
     *
     * @param inodeNumber The referenced inode.
     */
    void LookupINode(fuse_ino_t inodeNumber);
    
    /**
     * @brief Increase the number of reference of the given inode.
     *
     * @param inode The referenced inode.
     */
    void LookupINode(INode *inode);

    /**
     * @brief Forget a given inode.
     *
     * @param inodeNumber The inode to forget.
     * @param nlookup The number of the references that the kernel is currently forgetting for the given inode.
     */
    void Forget(fuse_ino_t inodeNumber, unsigned long nlookup);

    /**
     * @brief Forget a given inode.
     *
     * @param inode The inode to forget.
     * @param nlookup The number of the references that the kernel is currently forgetting for the given inode.
     */
    void Forget(INode *inode, unsigned long nlookup);

    /**
     * @brief Delete a given inode.
     *
     * @param inodeNumber The inode to delete.
     */
    void DeleteINode(fuse_ino_t inodeNumber);

    /**
     * @brief Reclaim the first inode previously marked as deleted.
     *
     * @return The inode number to reuse.
     */
    fuse_ino_t reclaimINode();

    /**
     * @brief Set the given attributes for the given inode.
     *
     * @param inode The inode.
     * @param attr The attributes to set for the inode.
     * @param to_set The flags indicating which attributes are to set.
     */
    void SetINodeAttributes(INode *inode, struct stat* attr, int to_set);
};

#endif //NODES_HPP
