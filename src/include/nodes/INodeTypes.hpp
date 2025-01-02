//
// Created by frank on 11/30/24.
//

#ifndef INODETYPES_HPP
#define INODETYPES_HPP

enum INodeType {
    REGULAR_FILE,
    DIRECTORY,
    SYMBOLIC_LINK,
    SPECIAL_INODE_TYPE_NO_BLOCK
};

typedef unsigned int INodeID;

#endif //INODETYPES_HPP
