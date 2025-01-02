//
// Created by frank on 11/29/24.
//

#ifndef FILE_HPP
#define FILE_HPP

#include "inodes_data_structures.hpp"

class File final: public INode {
public:
    void * m_buf;
    bool isWaitingForDistributedWrite;

public:
    File();
    ~File();
    void *getData(){return m_buf;};
    bool isWaitingForWriting() { return isWaitingForDistributedWrite; };
    void setWaiting(){isWaitingForDistributedWrite = true;};
    void removeWaiting(){isWaitingForDistributedWrite = false;};
};



#endif //FILE_HPP
