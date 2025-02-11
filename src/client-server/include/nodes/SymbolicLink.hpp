//
// Created by frank on 11/29/24.
//

#ifndef SYMBOLICLINK_HPP
#define SYMBOLICLINK_HPP

#include "inodes_data_structures.hpp"

class SymbolicLink final: public INode{
private:
    string m_link;
public:
    SymbolicLink(){}
    SymbolicLink(string link): m_link(link){};
    ~SymbolicLink() override = default;
    const string &Link() { return m_link; }

    void setLink(string link) { m_link = link; }
};



#endif //SYMBOLICLINK_HPP
