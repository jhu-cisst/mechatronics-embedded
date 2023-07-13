# Find Xilinx Vitis installation
#

find_program(VITIS_XSCT NAMES "xsct" DOC "Xilinx Vitis XSCT tool")

get_filename_component(VITIS_BIN ${VITIS_XSCT} PATH)

find_program(VITIS_BOOTGEN NAMES "bootgen" HINTS ${VITIS_BIN} DOC "Xilinx Vitis bootgen tool")

get_filename_component(VITIS_ROOT ${VITIS_BIN} DIRECTORY)

# Check Vitis version. Even though the version is usually part of the path, it is better
# to parse the output from an executable. Although "vitis -version" would work, since we are
# finding xsct, we instead use: xsct -eval "puts [version]"
# This actually reports a major/minor/patch, which are saved in the corresponding CMake variables
# Vitis_VERSION_MAJOR, Vitis_VERSION_MINOR and Vitis_VERSION_PATCH, but for consistency we set
# Vitis_VERSION to include just the major and minor versions (e.g., 2022.2, 2023.1).

set (Vitis_VERSION "")
set (Vitis_VERSION_MAJOR "")
set (Vitis_VERSION_MINOR "")
set (Vitis_VERSION_PATCH "")
if (VITIS_XSCT)
  execute_process (COMMAND ${VITIS_XSCT} -eval "puts [version]"
                   OUTPUT_VARIABLE VitisVersionOutput)

  string (REGEX MATCH "xsct [0-9]+\.[0-9]\.[0-9]" VitisVersionString ${VitisVersionOutput})
  if (VitisVersionString)
    string (SUBSTRING ${VitisVersionString} 5 -1 Vitis_VERSION_FULL)
    if (Vitis_VERSION_FULL)
      string (FIND ${Vitis_VERSION_FULL} "." DOT_POS1)
      string (FIND ${Vitis_VERSION_FULL} "." DOT_POS2 REVERSE)
      if (DOT_POS1 EQUAL 4 AND DOT_POS2 EQUAL 6)
        string (SUBSTRING ${Vitis_VERSION_FULL} 0  4 Vitis_VERSION_MAJOR)
        string (SUBSTRING ${Vitis_VERSION_FULL} 5  1 Vitis_VERSION_MINOR)
        string (SUBSTRING ${Vitis_VERSION_FULL} 7  1 Vitis_VERSION_PATCH)
        set (Vitis_VERSION "${Vitis_VERSION_MAJOR}.${Vitis_VERSION_MINOR}")
        message (STATUS "Found Vitis ${Vitis_VERSION} (${Vitis_VERSION_MAJOR}, ${Vitis_VERSION_MINOR}, ${Vitis_VERSION_PATCH})")
      else ()
        message (WARNING "FindVitis: invalid version string ${Vitis_VERSION_FULL}")
      endif ()
    endif ()
  else ()
    message (WARNING "FindVitis: could not get version string")
  endif ()
endif ()

set (VITIS_USE_FILE "${CMAKE_SOURCE_DIR}/cmake/UseVitis.cmake")

if (VITIS_XSCT AND VITIS_BOOTGEN)
  set (Vitis_FOUND TRUE)
else()
  set (Vitis_FOUND FALSE)
endif()
