//
// Created by frank on 11/29/24.
//

#ifndef DIRECTORY_HPP
#define DIRECTORY_HPP

#include "inodes_data_structures.hpp"

#include <map>
#include <string>

using namespace std;

/**
 * @brief The class representing a directory in the file system.
 *
 * This class denote a directory in the file system with its children: files and/or directory.
 */
class Directory final: public INode {
private:
    /**
     * @brief Children list.
     */
    map<string,fuse_ino_t> m_children;  /** The map uses the name of the child as key and its inode number as a value. */

public:
    Directory();
    ~Directory() override = default;

    /**
     * @brief Get the children of this directory.
     *
     * @return The children list.
     */
    map<string,fuse_ino_t> &Children(){ return m_children; }

    /**
     * 
     * @return 
     */
    int GetChildrenNumber(){ return m_children.size(); }
    fuse_ino_t ChildINodeNumberWithName(const string &name);
    fuse_ino_t UpdateChild(const std::string& name, fuse_ino_t ino);
    fuse_ino_t DeleteChild(const std::string& name);
    bool hasChildren();
};



#endif //DIRECTORY_HPP
