#
# CMakeLists.txt for cross-compiling fpgav3 library and apps, using either
# Vitis (gcc) or clang toolchain.
#
# This CMakeLists.txt should not be directly called -- it is called from
# the mechatronics-embedded top-level CMakeLists.txt, which defines the toolchain
# file and the root directory of the source files (PETALINUX_SOURCE_DIR).
# 

cmake_minimum_required(VERSION 3.16)

project(fpgav3apps)

# When cross-compiling, Arch should be set to arm32
if (NOT Arch STREQUAL "arm32")
  message(SEND_ERROR "Cross-compilation not properly initialized (Arch not set to arm32)")
endif ()

set (LIBFPGAV3_SOURCE_DIR "${PETALINUX_SOURCE_DIR}/libfpgav3/files")

# Set include_directories so that most recent header files are available, rather than
# relying on sysroot
include_directories (${LIBFPGAV3_SOURCE_DIR})

# Build the library
set (LIBFPGAV3_SOURCE "${LIBFPGAV3_SOURCE_DIR}/fpgav3_emio.cpp"
                      "${LIBFPGAV3_SOURCE_DIR}/fpgav3_emio.h"
                      "${LIBFPGAV3_SOURCE_DIR}/fpgav3_emio_gpiod.cpp"
                      "${LIBFPGAV3_SOURCE_DIR}/fpgav3_emio_gpiod.h"
                      "${LIBFPGAV3_SOURCE_DIR}/fpgav3_emio_mmap.cpp"
                      "${LIBFPGAV3_SOURCE_DIR}/fpgav3_emio_mmap.h"
                      "${LIBFPGAV3_SOURCE_DIR}/fpgav3_qspi.cpp"
                      "${LIBFPGAV3_SOURCE_DIR}/fpgav3_qspi.h"
                      "${LIBFPGAV3_SOURCE_DIR}/fpgav3_lib.cpp"
                      "${LIBFPGAV3_SOURCE_DIR}/fpgav3_lib.h"
                      ${FPGAV3_VERSION_HEADER})

add_library (fpgav3 SHARED ${LIBFPGAV3_SOURCE})

# Build the apps

set (FPGAV3INIT_SOURCE "${PETALINUX_SOURCE_DIR}/fpgav3init/files/fpgav3init.cpp")
add_executable (fpgav3init ${FPGAV3INIT_SOURCE})
target_link_libraries (fpgav3init "fpgav3" "gpiod")

set (FPGAV3BLOCK_SOURCE "${PETALINUX_SOURCE_DIR}/fpgav3block/files/fpgav3block.cpp")
add_executable (fpgav3block ${FPGAV3BLOCK_SOURCE})
target_link_libraries (fpgav3block "fpgav3" "gpiod")

set (FPGAV3SN_SOURCE "${PETALINUX_SOURCE_DIR}/fpgav3sn/files/fpgav3sn.cpp")
add_executable (fpgav3sn ${FPGAV3SN_SOURCE})
target_link_libraries (fpgav3sn "fpgav3" "gpiod")
