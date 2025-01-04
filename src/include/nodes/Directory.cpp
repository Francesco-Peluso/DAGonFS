//
// Created by frank on 11/29/24.
//

#include "Directory.hpp"

#include <iostream>
using namespace std;

Directory::Directory() {
    m_children = map<string, fuse_ino_t>();
}

/**
 Returns a child inode number given the name of the child.

 @param name The child file / dir name.
 @return The child inode number if the child is found. -1 otherwise.
 */
fuse_ino_t Directory::ChildINodeNumberWithName(const std::string& name) {
    if (m_children.find(name) == m_children.end()) {
        return -1;
    }

    return m_children[name];
}

/**
 Changes the inode number of the child with the given name.

 @param name The name of the child to update.
 @param ino The new inode number.
 @return The old inode number before the change.
 */
fuse_ino_t Directory::UpdateChild(const std::string& name, fuse_ino_t ino) {
    fuse_ino_t ino_ret = m_children[name];
    m_children[name] = ino;

    // TODO: What about directory sizes? Shouldn't we increase the reported size of our dir?

    return ino_ret;
}

fuse_ino_t Directory::DeleteChild(const std::string& name) {
    fuse_ino_t ino_ret = m_children[name];
    m_children.erase(name);
    return ino_ret;
}

/**
 Check if the directory is empty.

 @return True if the only children are the following directories: "." and ".."
 */
bool Directory::hasChildren() {
    map<std::string, fuse_ino_t>::iterator it;
    for (it = m_children.begin(); it != m_children.end(); it++) {
        if (it->first != "." && it->first != "..")
            return true;
    }
    return false;
}

