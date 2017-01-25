# - Try to find GOOGLEBENCH
#
# Once done this will define
#
#  GOOGLEBENCH_FOUND - system has GOOGLEBENCH
#  GOOGLEBENCH_INCLUDE_DIRS - the GOOGLEBENCH include directory
#  GOOGLEBENCH_LIBRARIES - Link these to use GOOGLEBENCH
#

find_path(
    GOOGLEBENCH_INCLUDE_DIRS
    NAMES benchmark/benchmark.h
    HINTS ${GOOGLEBENCH_INCLUDE_DIRS}
)

find_library(
    GOOGLEBENCH_LIBRARIES
    NAMES benchmark
    HINTS ${GOOGLEBENCH_LIBRARY_DIRS}
)

include(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(
    GOOGLEBENCH
    DEFAULT_MSG
    GOOGLEBENCH_LIBRARIES
    GOOGLEBENCH_INCLUDE_DIRS
)

if(GOOGLEBENCH_FOUND)
    mark_as_advanced(GOOGLEBENCH_LIBRARIES GOOGLEBENCH_INCLUDE_DIRS)
endif()