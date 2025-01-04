# Find the LOG4CPLUS includes and library
#
#  LOG4CPLUS_INCLUDE_DIR - where to find fuse.h, etc.
#  LOG4CPLUS_LIBRARIES   - List of libraries when using LOG4CPLUS.
#  LOG4CPLUS_FOUND       - True if LOG4CPLUS lib is found.

# check if already in cache, be silent
IF (LOG4CPLUS_INCLUDE_DIR)
    SET (LOG4CPLUS_FIND_QUIETLY TRUE)
ENDIF (LOG4CPLUS_INCLUDE_DIR)

# find includes
FIND_PATH (LOG4CPLUS_INCLUDE_DIR 
	NAMES log4cplus.h
	PATHS /usr/local/include/log4cplus/ /usr/include/log4cplus/
	NO_DEFAULT_PATH
)

SET(LOG4CPLUS_NAMES log4cplus)

FIND_LIBRARY(LOG4CPLUS_LIBRARIES
        NAMES ${LOG4CPLUS_NAMES}
        PATHS /lib64 /lib /usr/lib64 /usr/lib /usr/local/lib64 /usr/local/lib /usr/lib/x86_64-linux-gnu
)

include ("FindPackageHandleStandardArgs")

find_package_handle_standard_args ("LOG4CPLUS" DEFAULT_MSG LOG4CPLUS_INCLUDE_DIR LOG4CPLUS_LIBRARIES)

mark_as_advanced (LOG4CPLUS_INCLUDE_DIR LOG4CPLUS_LIBRARIES)
