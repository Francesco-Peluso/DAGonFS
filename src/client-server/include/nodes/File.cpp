//
// Created by frank on 11/29/24.
//

#include "File.hpp"
#include <cstring>
#include <ctime>
#include <iostream>

#include "Nodes.hpp"
#include "../ramfs/FileSystem.hpp"

using namespace std;

File::File() {
    m_buf = nullptr;
    isWaitingForDistributedWrite = false;
}

File::~File() {
    if (m_buf) free(m_buf);
}
