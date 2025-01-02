//
// Created by frank on 11/29/24.
//

#include "INode.hpp"

#include <iostream>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <cstring>
#include <cerrno>
#include <map>
#include <tuple>
#include <unistd.h>
#include <fcntl.h>

#include <sys/xattr.h>
using namespace std;

INode::INode() {
    m_nlookup = 0;
    markedForDeletion = false;
    m_xattr = map<string, pair<void *, size_t> >();
}

INode::~INode() {}

void INode::Lookup() {
    m_nlookup++;
}

void INode::Forget(unsigned long nlookup) {
    m_nlookup -= nlookup;
}

/**
 * After decrementing, if the number of hard links reaches 0, the inode is considered as deleted.
 */
void INode::RemoveHardLink() {
    m_fuseEntryParam.attr.st_nlink--;
    if (m_fuseEntryParam.attr.st_nlink <= 0) {
        markForDeletion();
    }
}

int INode::SetXAttr(const string& name, const void* value, size_t size, int flags, uint32_t position) {
    cout << "\tSetting " << name << "attribute with value " << value <<" -> FuseRamFs::FuseSetXAttr" << endl;
    if (m_xattr.find(name) == m_xattr.end()) {
        if (flags & XATTR_CREATE) {
            return EEXIST;
        }
    }
    else {
        if (flags & XATTR_REPLACE) {
            #ifdef __APPLE__
            return ENOATTR;
            #else
            return ENODATA;
            #endif
        }
    }

    // TODO: What about overflow with size + position?
    size_t newExtent = size + position;

    // Expand the space for the value if required.
    if (m_xattr[name].second < newExtent) {
        void *newBuf = realloc(m_xattr[name].first, newExtent);
        if (newBuf == NULL) {
            return E2BIG;
        }

        m_xattr[name].first = newBuf;

        // TODO: How does the user truncate the value? I.e., if they want to replace part, they'll send in
        // a position and a small size, right? If they want to make the whole thing shorter, then what?
        m_xattr[name].second = newExtent;
    }

    // Copy the data.
    memcpy((char *) m_xattr[name].first + position, value, size);

    cout << "\tSetting " << name << "attribute with value " << value <<" -> FuseRamFs::FuseSetXAttr completed!" << endl;

    return 0;
}

map<string, pair<void*,size_t>> &INode::GetXAttr() {
    return m_xattr;
}

int INode::RemoveXAttr(const string& name) {
    cout <<"\tRemoving " << name << "attribute -> INode::RemoveXAttrAndReply" << endl;
    map<string, pair<void *, size_t> >::iterator it = m_xattr.find(name);

    if (it == m_xattr.end()) {
        #ifdef __APPLE__
        return ENOATTR;
        #else
        return ENODATA;
        #endif
    }

    m_xattr.erase(it);

    cout <<"\tRemoving " << name << "attribute -> INode::RemoveXAttrAndReply completed!" << endl;

    return 0;
}