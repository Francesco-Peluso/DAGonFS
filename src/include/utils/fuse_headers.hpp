//
// Created by frank on 1/2/25.
//

#ifndef MY_FUSE_HEADERS_HPP
#define MY_FUSE_HEADERS_HPP

#ifdef __APPLE__
#include <osxfuse/fuse/fuse_lowlevel.h>
#else
#include <fuse3/fuse_lowlevel.h>
#endif

#endif //MY_FUSE_HEADERS_HPP
