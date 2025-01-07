//
// Created by frank on 11/29/24.
//

#ifndef INODE_HPP
#define INODE_HPP

#include "../utils/fuse_headers.hpp"
#include <map>
#include <string>
#include "../utils/log_level.hpp"

using namespace std;

/**
 * @brief The class representing an inode in the file system.
 *
 * This class denote a file system inode with its components: inode number, metadata and
 * pointers to effective data.
 */
class INode {
private:
    //unsigned long m_nlookup;
    //bool markedForDeletion;
    log4cplus::Logger INodeLogger;

public:
    /**
     * @brief The lookup number for the operation of lookup.
     *
     * Keeps the number of reference of this inode.
     */
    unsigned long m_nlookup; //Da mettere private

    /**
     * @brief The flag indicating if the inode has been deleted.
     */
    bool markedForDeletion; //Da mettere private

    /**
     * @brief The inode number of this inode.
     */
    fuse_ino_t inodeNumber;

    /**
     * @brief The directory entry for this inode.
     *
     * This reflects the metadata of this inode.
     */
    fuse_entry_param m_fuseEntryParam; //Directory entry parameters supplied to fuse_reply_entry()

    /**
     * @brief The extended attributes for this inode.
     */
    map<string, pair<void *, size_t> > m_xattr;

    /**
     * @brief Constructor
     */
    INode();

    /**
     * @brief Virtual Destructor for polymorphism.
     *
     * This allows the inheritance for different file types (i.e. regular file, directory, etc.) since them ARE inodes.
     */
    virtual ~INode() = 0;

    /**
     * @brief Return the number of used block for this inode.
     *
     * @return The number of the used block.
     */
    size_t UsedBlocks() { return m_fuseEntryParam.attr.st_blocks; }

    /**
     * @brief Increments the number of references for this inode.
     */
    void Lookup();

    /**
     * @brief Removes the number of references executed by the lookup() file system operation.
     *
     * @param nlookup The number of references to remove for this inode.
     */
    void Forget(unsigned long nlookup);

    /**
     * @brief Check if an inode is completely de-referenced.
     *
     * @return TRUE if the references count reaches 0
     */
    bool Forgotten() { return m_nlookup == 0; }

    /**
     * @brief Check if the hard link number is 0.
     *
     * @return TURE if the inode reaches 0 hard link count.
     */
    bool HasNoLinks() { return m_fuseEntryParam.attr.st_nlink == 0; }

    /**
     * @brief Mark this inode as deleted.
     */
    void markForDeletion() { markedForDeletion = true; }

    /**
     * @brief Mark this inode as non-deleted.
     */
    void unmarkForDeletion() { markedForDeletion = false; }

    /**
     * @brief Check if this inode is deleted.
     * @return TRUE if this inode has the deletion flag to TRUE.
     */
    bool isDeleted() { return markedForDeletion; }

    /**
     * @brief Increment the hard link count for this inode.
     */
    void AddHardLink() { m_fuseEntryParam.attr.st_nlink++; }

    /**
     * @brief Decrementing the hard link count for this inode.
     */
    void RemoveHardLink();

    /**
     * @brief Set the given extended attribute of this inode to the given value.
     *
     * @param name The name of the extended attribute.
     * @param value The value to set of the extended attribute.
     * @param size The size of the value.
     * @param flags The flags for creating or replacing the extended attribute.
     * @param position Offset.
     * @return 0 on success. Other values may be EEXISTS, ENODATA (ENOATTR on __APPLE__), E2BIG
     */
    virtual int SetXAttr(const string &name, const void *value, size_t size, int flags, uint32_t position); //Corrispettivo di SetXAttrAndReply

    /**
     * @brief Return the extended attributes list.
     * 
     * @return The extended attributes list
     */
    virtual map<string, pair<void *, size_t> > &GetXAttr(); //Corrispettivo di GetXAttrAndReply

    /**
     * @brief Remove an extended attribute from this inode given the name.
     * 
     * @param name The name of attribute to remove.
     * @return 0 on success or ENODATA (or ENOATTR on __APPLE__) if the extended attribute to remove is not found.
     */
    virtual int RemoveXAttr(const std::string &name); //Corrispettivo di RemoveXAttrAndReply
};

#endif //INODE_HPP
