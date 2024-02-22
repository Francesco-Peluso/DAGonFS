# Find the FUSE3 includes and library
#
#  FUSE3_INCLUDE_DIR - where to find fuse.h, etc.
#  FUSE3_LIBRARIES   - List of libraries when using FUSE.
#  FUSE3_FOUND       - True if FUSE lib is found.

# check if already in cache, be silent
IF (FUSE3_INCLUDE_DIR)
    SET (FUSE3_FIND_QUIETLY TRUE)
ENDIF (FUSE3_INCLUDE_DIR)

# find includes
FIND_PATH (FUSE3_INCLUDE_DIR 
	NAMES fuse.h
	PATHS /usr/local/include/osxfuse /usr/local/include/fuse3/ /usr/include/fuse3/
	NO_DEFAULT_PATH
)


# find lib
if (APPLE)
    SET(FUSE3_NAMES libosxfuse3.dylib fuse3)
else (APPLE)
    SET(FUSE3_NAMES fuse3)
endif (APPLE)

FIND_LIBRARY(FUSE3_LIBRARIES
        NAMES ${FUSE3_NAMES}
        PATHS /lib64 /lib /usr/lib64 /usr/lib /usr/local/lib64 /usr/local/lib /usr/lib/x86_64-linux-gnu
)

include ("FindPackageHandleStandardArgs")

find_package_handle_standard_args ("FUSE3" DEFAULT_MSG FUSE3_INCLUDE_DIR FUSE3_LIBRARIES)

mark_as_advanced (FUSE3_INCLUDE_DIR FUSE3_LIBRARIES)
