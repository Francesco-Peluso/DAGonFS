//
// Created by frank on 11/29/24.
//

#ifndef SPECIALINODE_HPP
#define SPECIALINODE_HPP

#include "inodes_data_structures.hpp"

class SpecialINode final: public INode {
private:
    //Attributes
    enum INodeType m_type;

public:
    SpecialINode(INodeType type): m_type(type) {};
    ~SpecialINode() override = default;
};



#endif //SPECIALINODE_HPP
