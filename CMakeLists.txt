
# Set CMake minimum version requirement.
# Need 3.15+ for ADDITIONAL_CLEAN_FILES property
# CMake 3.16 improved dependency checking in add_custom_target
cmake_minimum_required(VERSION 3.16)

project(FpgaBoard NONE)

# The designated version; note that this may be different from the version indicated by git (see below)
set (EMBEDDED_VERSION_MAJOR 1)
set (EMBEDDED_VERSION_MINOR 1)
set (EMBEDDED_VERSION_PATCH 0)
set (EMBEDDED_VERSION "${EMBEDDED_VERSION_MAJOR}.${EMBEDDED_VERSION_MINOR}.${EMBEDDED_VERSION_PATCH}")

set (CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
set (CMAKE_PREFIX_PATH ${CMAKE_PREFIX_PATH} "${CMAKE_CURRENT_SOURCE_DIR}/espm_firmware")

# Git is used by CreateVersions.cmake, which is used to generate fpgav3_version.h and
# mechatronics-embedded-config.cmake, so we find it here.
# Note that ExternalProject.cmake and FetchContent.cmake also find Git.
# If Git is not found, we cannot enable USE_VITIS or USE_PETALINUX.
find_package (Git QUIET)

if (UNIX)
  # On Unix, TOOLCHAIN_ONLY defaults to OFF
  option (TOOLCHAIN_ONLY "Only create toolchain files and download sysroot" OFF)
else ()
  # On other platforms, TOOLCHAIN_ONLY defaults to ON because the most common workflow
  # is to just support cross-compiling
  option (TOOLCHAIN_ONLY "Only create toolchain files and download sysroot" ON)
endif ()

include (CMakeDependentOption)
# Vivado and Vitis not available on APPLE
cmake_dependent_option (USE_VIVADO "Use Vivado to build block_design (XSA file)" ON
                        "NOT APPLE;NOT TOOLCHAIN_ONLY" OFF)
cmake_dependent_option (USE_VITIS "Use Vitis to build platforms (standalone and linux)" ON
                        "NOT APPLE;NOT TOOLCHAIN_ONLY" OFF)
# Petalinux only available on Linux
cmake_dependent_option (USE_PETALINUX "Build Petalinux image" ON
                        "UNIX;NOT TOOLCHAIN_ONLY" OFF)
cmake_dependent_option (BUILD_PETALINUX_SYSROOT "Build Petalinux SDK (sysroot)" ON
                        "UNIX;USE_PETALINUX" OFF)
# USE_PETALINUX_SYSROOT is ON when TOOLCHAIN_ONLY is specified, and appears as a user-visible
# option when not only building the toolchain files and not building the Petalinux sysroot
cmake_dependent_option (USE_PETALINUX_SYSROOT "Use Petalinux sysroot for cross-compiling" OFF
                        "NOT TOOLCHAIN_ONLY;NOT BUILD_PETALINUX_SYSROOT" ON)

if (WIN32)
  set (CMAKE_CONFIGURATION_TYPES "Release")   # For Visual Studio
  set (CMAKE_BUILD_TYPE "Release")            # For Ninja
endif ()

if (BUILD_PETALINUX_SYSROOT)

  # PETALINUX_SDK is the directory where the SDK will be installed
  set (PETALINUX_SDK     "${CMAKE_BINARY_DIR}/petalinux/sdk")
  # PETALINUX_SDK_TARGET is the CMake target
  set (PETALINUX_SDK_TARGET "fpgav3-sdk")

  # PETALINUX_SYSROOT is where the sysroot will be installed (within the SDK)
  set (PETALINUX_SYSROOT "${PETALINUX_SDK}/sysroots/cortexa9t2hf-neon-xilinx-linux-gnueabi")

elseif (USE_PETALINUX_SYSROOT)

  # We set PETALINUX_SYSROOT to a user-specified sysroot, which defaults to the latest release
  set (PETALINUX_SYSROOT_EXTERNAL
       "https://github.com/jhu-cisst/mechatronics-embedded/releases/latest/download/fpgav3-sysroot-cortexa9.zip"
       CACHE STRING "URL or path to external sysroot (from Petalinux)")

  if (IS_DIRECTORY ${PETALINUX_SYSROOT_EXTERNAL})
    # The SYSROOT built by Petalinux does not have read permissions on /usr/bin/sudo, which will
    # cause downstream failures. It is better to use the ZIP file, either from the GitHub Releases,
    # or built locally using Petalinux -- see fpgav3-sysroot-cortexa9.zip in fpgav3-generated directory.
    message (WARNING "SYSROOT directory specified - could fail due to permissions on /usr/bin/sudo -"
                     " recommend using ZIP file on GitHub or in fpgav3-generated directory")
  endif ()

  include (ExternalProject)
  ExternalProject_Add (sysroot_external
                       URL ${PETALINUX_SYSROOT_EXTERNAL}
                       CONFIGURE_COMMAND ""
                       BUILD_COMMAND ""
                       INSTALL_COMMAND "")
  ExternalProject_Get_property(sysroot_external SOURCE_DIR)
  set (PETALINUX_SYSROOT ${SOURCE_DIR})

endif ()

if (USE_VIVADO OR USE_VITIS OR USE_PETALINUX)

  option (BUILD_FPGAV30 "Build for FPGA V3.0 (instead of V3.1+)" OFF)
  mark_as_advanced (BUILD_FPGAV30)

  if (BUILD_FPGAV30)
    set (FPGA_VERSION "3.0")
  else ()
    set (FPGA_VERSION "3.1")
  endif ()

  # espm-firmware should always be found, since it is in this repository
  find_package (espm-firmware)

endif ()

if (USE_VITIS OR USE_PETALINUX)

  # We need git to create fpgav3_version.h, using CreateVersions.cmake.
  if (NOT Git_FOUND)
    message (SEND_ERROR "Git not found (required for USE_VITIS or USE_PETALINUX)")
  endif ()

  # ************************* Create fpgav3 version header file *******************************
  # Create the file fpgav3_version.h (FPGAV3_VERSION_HEADER), which is used in petalinux,
  # platform_linux and platform_standalone, when building the fpgav3 library. Note that
  # this introduces a dependency on the FpgaVersionFile target.

  set (VERSION_INPUT_FILE_SRC  "${CMAKE_CURRENT_SOURCE_DIR}/petalinux/libfpgav3/files/fpgav3_version.h.in")
  set (VERSION_INPUT_FILE_BIN  "${CMAKE_CURRENT_BINARY_DIR}/include/fpgav3_version.h.in")
  set (VERSION_OUTPUT_FILE_NEW "${CMAKE_CURRENT_BINARY_DIR}/include/fpgav3_version.h.new")
  set (FPGAV3_VERSION_HEADER   "${CMAKE_CURRENT_BINARY_DIR}/include/fpgav3_version.h")

  configure_file (${VERSION_INPUT_FILE_SRC} ${VERSION_INPUT_FILE_BIN} @ONLY)

  add_custom_command (OUTPUT ${VERSION_OUTPUT_FILE_NEW}
                      COMMAND ${CMAKE_COMMAND} -DGIT_EXECUTABLE=${GIT_EXECUTABLE}
                                               -DSOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR}
                                               -DTAG_PREFIX="v"
                                               -DIN_FILE=${VERSION_INPUT_FILE_BIN}
                                               -DOUT_FILE=${VERSION_OUTPUT_FILE_NEW}
                                               -P "${CMAKE_MODULE_PATH}/CreateVersions.cmake"
                       DEPENDS ${VERSION_INPUT_FILE_BIN})

  add_custom_target (FpgaVersionFile ALL
                     BYPRODUCTS ${FPGAV3_VERSION_HEADER}
                     COMMAND ${CMAKE_COMMAND} -E copy_if_different ${VERSION_OUTPUT_FILE_NEW}
                                                                   ${FPGAV3_VERSION_HEADER}
                     COMMAND ${CMAKE_COMMAND} -E remove -f ${VERSION_OUTPUT_FILE_NEW}
                     COMMENT "Checking fpgav3_version.h"
                     DEPENDS ${VERSION_OUTPUT_FILE_NEW})

  # *******************************************************************************************

  find_package (mechatronics-firmware QUIET)

  if (mechatronics-firmware_FOUND)

    set (BIT_FILE ${FPGAV3_BCFG_BIT_FILE})
    unset (BIT_FILE_MANUAL CACHE)

  else ()

    find_file (BIT_FILE_MANUAL "FPGA1394V3-BCFG.bit" DOC "FPGA compiled firmware, FPGA1394V3-BCFG.bit")
    set (BIT_FILE ${BIT_FILE_MANUAL})

    if (EXISTS ${BIT_FILE_MANUAL})
      message (STATUS "mechatronics-firmware not found -- using BIT_FILE_MANUAL")
    else ()
      message (SEND_ERROR "mechatronics-firmware not found -- please set BIT_FILE_MANUAL")
    endif ()

  endif ()

endif ()

set (Xilinx_VERSION "")

if (USE_VIVADO)

  find_package (Vivado)

  if (Vivado_FOUND)

    include(${VIVADO_USE_FILE})
    set (Xilinx_VERSION ${Vivado_VERSION})

    unset (HW_XSA_FILE CACHE)
    if (BUILD_FPGAV30)
      set (BLOCK_DESIGN_NAME "FpgaV30BlockDesign")
      set (HW_XSA_FILE  "${CMAKE_CURRENT_BINARY_DIR}/block_design/FpgaV30.xsa")
    else (BUILD_FPGAV30)
      set (BLOCK_DESIGN_NAME "FpgaV31BlockDesign")
      set (HW_XSA_FILE  "${CMAKE_CURRENT_BINARY_DIR}/block_design/FpgaV31.xsa")
    endif (BUILD_FPGAV30)

    add_subdirectory (block_design)

  else ()

    message(SEND_ERROR "Xilinx Vivado not found")

  endif ()

elseif (USE_VITIS OR USE_PETALINUX)

  set (BLOCK_DESIGN_NAME "")
  if (BUILD_FPGAV30)
    find_file (HW_XSA_FILE "FpgaV30.xsa" DOC "XSA file produced by Vivado")
  else (BUILD_FPGAV30)
    find_file (HW_XSA_FILE "FpgaV31.xsa" DOC "XSA file produced by Vivado")
  endif (BUILD_FPGAV30)

endif ()

set (VITIS_FSBL_TARGET "")

if (USE_VITIS)

  find_package(Vitis)

  if (Vitis_FOUND)

    if (Vivado_FOUND AND NOT Vitis_VERSION STREQUAL Vivado_VERSION)

      message (SEND_ERROR "Vitis ${Vitis_VERSION} is not compatible with Vivado ${Vivado_VERSION}")

    else ()

      set (Xilinx_VERSION ${Vitis_VERSION})
      include(${VITIS_USE_FILE})

      set (VITIS_FSBL_TARGET "zynq_fsbl")
      set (VITIS_MFG_TARGET  "mfg_test")

      if (HW_XSA_FILE)
        add_subdirectory (platform_standalone)
        add_subdirectory (platform_linux)
      else ()
        message (WARNING "Cannot build standalone and linux platforms (Find Vivado or specify HW_XSA_FILE)")
      endif ()

    endif ()

  else ()

    message(SEND_ERROR "Xilinx Vitis not found")

  endif ()

endif ()

if (USE_PETALINUX)

  find_package (Petalinux)

  if (Petalinux_FOUND)

    if (Vivado_FOUND AND NOT Petalinux_VERSION STREQUAL Vivado_VERSION)

      message (SEND_ERROR "Petalinux ${Petalinux_VERSION} is not compatible with Vivado ${Vivado_VERSION}")

    elseif (Vitis_FOUND AND NOT Petalinux_VERSION STREQUAL Vitis_VERSION)

      message (SEND_ERROR "Petalinux ${Petalinux_VERSION} is not compatible with Vitis ${Vitis_VERSION}")

    else ()

      set (Xilinx_VERSION ${Petalinux_VERSION})
      include (${PETALINUX_USE_FILE})

      if (HW_XSA_FILE)
        add_subdirectory (petalinux)
      else ()
        message (WARNING "Cannot build petalinux (Find Vivado or specify HW_XSA_FILE)")
      endif ()

    endif ()

  else ()

    message (WARNING "Petalinux image will not be built because tools not found")

  endif ()

endif ()

#
# CMake config file: mechatronics-embedded-config.cmake
#

set (CONFIG_IN_FILE "${CMAKE_CURRENT_BINARY_DIR}/mechatronics-embedded-config.cmake.in")
set (CONFIG_FILE    "${CMAKE_CURRENT_BINARY_DIR}/mechatronics-embedded-config.cmake")

#
# Create an initial mechatronics-embedded-config.cmake.in file, which is then updated
# at build time using configure_file (see CreateVersions.cmake), if the git executable is found.
#

file (WRITE  ${CONFIG_IN_FILE} "#\n# CMake configuration file for mechatronics-embedded\n#\n\n")
file (APPEND ${CONFIG_IN_FILE} "set (EMBEDDED_VERSION         \"${EMBEDDED_VERSION}\")\n\n")
file (APPEND ${CONFIG_IN_FILE} "set (EMBEDDED_VERSION_MAJOR   ${EMBEDDED_VERSION_MAJOR})\n")
file (APPEND ${CONFIG_IN_FILE} "set (EMBEDDED_VERSION_MINOR   ${EMBEDDED_VERSION_MINOR}\n")
file (APPEND ${CONFIG_IN_FILE} "set (EMBEDDED_VERSION_PATCH   ${EMBEDDED_VERSION_PATCH})\n\n")
if (Git_FOUND)
  file (APPEND ${CONFIG_IN_FILE} "set (EMBEDDED_GIT_VERSION     \"@GIT_VERSION@\")\n\n")
  file (APPEND ${CONFIG_IN_FILE} "set (EMBEDDED_GIT_VERSION_MAJOR   @GIT_VERSION_MAJOR@)\n")
  file (APPEND ${CONFIG_IN_FILE} "set (EMBEDDED_GIT_VERSION_MINOR   @GIT_VERSION_MINOR@)\n")
  file (APPEND ${CONFIG_IN_FILE} "set (EMBEDDED_GIT_VERSION_PATCH   @GIT_VERSION_PATCH@)\n\n")
  file (APPEND ${CONFIG_IN_FILE} "set (EMBEDDED_GIT_SHA     @GIT_SHA@)\n")
  file (APPEND ${CONFIG_IN_FILE} "set (EMBEDDED_GIT_COMMITS @GIT_COMMITS@)\n")
  file (APPEND ${CONFIG_IN_FILE} "set (EMBEDDED_GIT_DIRTY   @GIT_DIRTY@)\n\n")
else ()
  message (WARNING "Git version not included in mechatronics-embedded-config.cmake (git executable not found)")
endif ()
if (mechatronics-firmware_FOUND)
  file (APPEND ${CONFIG_IN_FILE} "set (FIRMWARE_VERSION ${FIRMWARE_VERSION})\n\n")
endif ()
if (FPGA_VERSION)
  file (APPEND ${CONFIG_IN_FILE} "set (FPGA_VERSION     ${FPGA_VERSION})\n\n")
endif ()
file (APPEND ${CONFIG_IN_FILE} "set (USE_VIVADO       ${USE_VIVADO})\n")
file (APPEND ${CONFIG_IN_FILE} "set (USE_VITIS        ${USE_VITIS})\n")
file (APPEND ${CONFIG_IN_FILE} "set (USE_PETALINUX    ${USE_PETALINUX})\n\n")
if (BUILD_PETALINUX_SYSROOT OR USE_PETALINUX_SYSROOT)
  file (APPEND ${CONFIG_IN_FILE} "set (PETALINUX_SYSROOT      \"${PETALINUX_SYSROOT}\")\n\n")
endif ()
if (Xilinx_VERSION)
  file (APPEND ${CONFIG_IN_FILE} "set (Xilinx_VERSION   ${Xilinx_VERSION})\n")
endif ()

if (Git_FOUND)
  # If git executable was found, add a custom target to create mechatronics-embedded-config.cmake
  # during build, so that we can call "git describe" (see CreateVersions.cmake).
  add_custom_target (ConfigFile ALL
                     BYPRODUCTS ${CONFIG_FILE}
                     COMMAND ${CMAKE_COMMAND} -DGIT_EXECUTABLE=${GIT_EXECUTABLE}
                                              -DSOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR}
                                              -DTAG_PREFIX="v"
                                              -DIN_FILE=${CONFIG_IN_FILE}
                                              -DOUT_FILE=${CONFIG_FILE}
                                              -DCONFIG_OPTIONS="@ONLY"
                                              -P "${CMAKE_MODULE_PATH}/CreateVersions.cmake"
                     COMMENT "Creating mechatronics-embedded-config.cmake")
else ()
  # If git executable was not found, we can create mechatronics-embedded-config.cmake at
  # configuration time.
  configure_file (${CONFIG_IN_FILE} ${CONFIG_FILE} @ONLY})
endif ()

#
# Mechatronics toolchain files for FPGA V3
#
# To cross-compile with Vitis:
#    cmake  -DCMAKE_TOOLCHAIN_FILE=<path-to-toolchain>/toolchain_vitis_fpgav3.cmake  <path-to-source>
#
# To cross-compile with clang:
#    cmake  -DCMAKE_TOOLCHAIN_FILE=<path-to-toolchain>/toolchain_clang_fpgav3.cmake  <path-to-source>

if (BUILD_PETALINUX_SYSROOT OR USE_PETALINUX_SYSROOT)

  # Root directory for Petalinux source files
  set (CC_PETALINUX_SOURCE_DIR "${CMAKE_SOURCE_DIR}/petalinux")

  #### Create Vitis (gcc) toolchain file, if possible

  # If we are not on APPLE, and have not already tried to find Vitis (e.g., by turning on USE_VITIS),
  # then we quietly try to find it.
  if (NOT APPLE AND NOT USE_VITIS AND NOT Vitis_FOUND)
    find_package(Vitis QUIET)
  endif ()

  if (Vitis_FOUND)
    # Create a custom toolchain file for cross-compiling using Vitis
    set (TOOLCHAIN_VITIS "${CMAKE_CURRENT_BINARY_DIR}/toolchain_vitis_fpgav3.cmake")
    file (WRITE  ${TOOLCHAIN_VITIS} "#\n# Toolchain file for FPGA V3\n#\n")
    file (APPEND ${TOOLCHAIN_VITIS} "# Usage: cmake -DCMAKE_TOOLCHAIN_FILE=${CMAKE_CURRENT_BINARY_DIR}/toolchain_vitis_fpgav3.cmake"
                                 "  <path-to-source>\n\n")
    file (APPEND ${TOOLCHAIN_VITIS} "set (Sysroot ${PETALINUX_SYSROOT})\n")
    file (APPEND ${TOOLCHAIN_VITIS} "set (VITIS_ROOT ${VITIS_ROOT})\n\n")
    if (NOT WIN32)
      file (APPEND ${TOOLCHAIN_VITIS} "# Disable annoying linker warning about parameter passing change in GCC 7.1\n")
      file (APPEND ${TOOLCHAIN_VITIS} "SET (CMAKE_CXX_FLAGS \"\${CMAKE_CXX_FLAGS} -Wno-psabi\")\n\n")
    endif ()
    file (APPEND ${TOOLCHAIN_VITIS} "# Following is patched version from Xilinx repository\n\n")

    # Fetch the Xilinx CMake modules
    include (FetchContent)
    FetchContent_Declare (xilinx_cmake
                          GIT_REPOSITORY https://github.com/Xilinx/cmakeModules/
                          GIT_TAG 7915d8fa4d2869bdbc078b9d266a555b72c2ebb2)
    FetchContent_MakeAvailable (xilinx_cmake)

    # Apply some patches (using CMake string replacement rather than PATCH_COMMAND)
    file (READ ${xilinx_cmake_SOURCE_DIR}/toolchain_vitis_crosscomp_arm.cmake FILE_CONTENTS)
    # Change default Arch to "arm32"
    string (REPLACE "Arch \"arm64\"" "Arch \"arm32\"" FILE_CONTENTS "${FILE_CONTENTS}")
    # Remove call to find_package(Vitis) since we already define VITIS_ROOT
    string (REPLACE "find_package" "# find_package" FILE_CONTENTS "${FILE_CONTENTS}")
    if (WIN32)
      # Add .exe to programs on Windows
      string (REPLACE "-gcc)" "-gcc.exe)" FILE_CONTENTS "${FILE_CONTENTS}")
      string (REPLACE "-g++)" "-g++.exe)" FILE_CONTENTS "${FILE_CONTENTS}")
      # Does not seem necessary to update ld or ar, but we do it anyway
      string (REPLACE "-ld)" "-ld.exe)" FILE_CONTENTS "${FILE_CONTENTS}")
      string (REPLACE "-ar)" "-ar.exe)" FILE_CONTENTS "${FILE_CONTENTS}")
    endif (WIN32)
    file (APPEND ${TOOLCHAIN_VITIS} "${FILE_CONTENTS}")

    # Create a binary directory for cross-compiling (cc_vitis)
    set (CC_VITIS_BINARY_DIR "${CMAKE_CURRENT_BINARY_DIR}/cc_vitis")
    file (MAKE_DIRECTORY ${CC_VITIS_BINARY_DIR})
    message (STATUS "Creating cc_vitis (cross-compile binary directory)")
    # Call CMake, passing in the toolchain file, root path for source files and CMake generator
    execute_process(COMMAND ${CMAKE_COMMAND} -D CMAKE_TOOLCHAIN_FILE:FILEPATH=${TOOLCHAIN_VITIS}
                                             -D PETALINUX_SOURCE_DIR:PATH=${CC_PETALINUX_SOURCE_DIR}
                                             -G "${CMAKE_GENERATOR}"
                                             "${CMAKE_CURRENT_SOURCE_DIR}/cross_compile"
                    WORKING_DIRECTORY ${CC_VITIS_BINARY_DIR}
                    OUTPUT_QUIET ERROR_QUIET)
  elseif (NOT APPLE)
    message (STATUS "Not generating toolchain_vitis_fpgav3.cmake (Vitis not found)")
  endif ()

  #### Create clang toolchain file; note that the toolchain file is responsible for finding clang

  set (TOOLCHAIN_CLANG "${CMAKE_CURRENT_BINARY_DIR}/toolchain_clang_fpgav3.cmake")

  configure_file ("${CMAKE_CURRENT_SOURCE_DIR}/cmake/toolchain_clang_fpgav3.cmake.in"
                  ${TOOLCHAIN_CLANG}
                  @ONLY)

  # Create a binary directory for cross-compiling (cc_lang)
  set (CC_CLANG_BINARY_DIR "${CMAKE_CURRENT_BINARY_DIR}/cc_clang")
  file (MAKE_DIRECTORY ${CC_CLANG_BINARY_DIR})
  message (STATUS "Creating cc_clang (cross-compile binary directory)")
  # Call CMake, passing in the toolchain file, root path for source files and CMake generator
  execute_process(COMMAND ${CMAKE_COMMAND} -D CMAKE_TOOLCHAIN_FILE:FILEPATH=${TOOLCHAIN_CLANG}
                                           -D PETALINUX_SOURCE_DIR:PATH=${CC_PETALINUX_SOURCE_DIR}
                                           -G "${CMAKE_GENERATOR}"
                                           "${CMAKE_CURRENT_SOURCE_DIR}/cross_compile"
                  WORKING_DIRECTORY ${CC_CLANG_BINARY_DIR}
                  OUTPUT_QUIET ERROR_QUIET)

endif ()
