# CMake module to create the specified version file
#
# This module is invoked using "cmake -P"; the caller should first define:
#   SOURCE_DIR      path to the git source directory
#   TAG_PREFIX      prefix for version tags (e.g., "Rev" or "v")
#   IN_FILE         input file (full path)
#   OUT_FILE        output file (full path)
#   CONFIG_OPTIONS  configure_file options (e.g., @ONLY)

find_package (Git REQUIRED QUIET)

execute_process (COMMAND ${GIT_EXECUTABLE} describe --tags --match "${TAG_PREFIX}*"
                                           --long --dirty=-d --abbrev=7
                 WORKING_DIRECTORY ${SOURCE_DIR}
                 OUTPUT_VARIABLE GIT_DESCRIBE_STRING)
string (REPLACE "-" ";" GIT_DESCRIBE_LIST ${GIT_DESCRIBE_STRING})
list (LENGTH GIT_DESCRIBE_LIST desc_len)
if (desc_len LESS 3)
  message (FATAL_ERROR "git describe output invalid: ${GIT_DESCRIBE_STRING}")
endif ()
list (GET GIT_DESCRIBE_LIST 0 GIT_VERSION_TAG)
string (LENGTH ${TAG_PREFIX} TAG_PREFIX_LEN)
string (SUBSTRING ${GIT_VERSION_TAG} ${TAG_PREFIX_LEN} -1 GIT_VERSION)
list (GET GIT_DESCRIBE_LIST 1 GIT_COMMITS)
list (GET GIT_DESCRIBE_LIST 2 GIT_SHA_RAW)
string (SUBSTRING ${GIT_SHA_RAW} 1 7 GIT_SHA)
if (desc_len GREATER 3)
  set (GIT_DIRTY 1)
else ()
  set (GIT_DIRTY 0)
endif()

# Following is hard-coded to look for major.minor.patch
string (REPLACE "." ";" GIT_VERSION_LIST ${GIT_VERSION})
list (LENGTH GIT_VERSION_LIST ver_len)
if (ver_len GREATER 0)
  list (GET GIT_VERSION_LIST 0 GIT_VERSION_MAJOR)
  if (ver_len GREATER 1)
    list (GET GIT_VERSION_LIST 1 GIT_VERSION_MINOR)
    if (ver_len GREATER 2)
      list (GET GIT_VERSION_LIST 2 GIT_VERSION_PATCH)
    endif ()
  endif ()
endif ()

if ((GIT_COMMITS GREATER 0) OR (GIT_DIRTY EQUAL 1))
  set (GIT_VERSION "${GIT_VERSION}-${GIT_SHA}")
  if (GIT_DIRTY EQUAL 1)
    set (GIT_VERSION "${GIT_VERSION}*")
  endif ()
endif ()

if (IN_FILE AND OUT_FILE)
  configure_file (${IN_FILE} ${OUT_FILE} ${CONFIG_OPTIONS})
else ()
  message (WARNING "CreateVersions: missing input or output file")
endif ()
