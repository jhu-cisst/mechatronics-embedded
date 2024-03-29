
# Set CMake minimum version requirement.
# Need 3.15+ for ADDITIONAL_CLEAN_FILES property
# CMake 3.16 improved dependency checking in add_custom_target
cmake_minimum_required(VERSION 3.16)

project(FpgaBoard NONE)

set (EMBEDDED_VERSION_MAJOR 1)
set (EMBEDDED_VERSION_MINOR 0)
set (EMBEDDED_VERSION_PATCH 1)
set (EMBEDDED_VERSION "${EMBEDDED_VERSION_MAJOR}.${EMBEDDED_VERSION_MINOR}.${EMBEDDED_VERSION_PATCH}")

set (CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
set (CMAKE_PREFIX_PATH ${CMAKE_PREFIX_PATH} "${CMAKE_CURRENT_SOURCE_DIR}/espm_firmware")

if (NOT APPLE)
  option (USE_VIVADO "Use Vivado to build block_design (XSA file)" ON)
  option (USE_VITIS "Use Vitis to build platforms (standalone and linux)" ON)
endif (NOT APPLE)

if (WIN32 OR APPLE)
  set (USE_PETALINUX OFF)
  set (BUILD_PETALINUX_SYSROOT OFF)
  if (WIN32)
    set (CMAKE_CONFIGURATION_TYPES "Release")   # For Visual Studio
  endif ()
else ()
  # Can only build Petalinux on Linux
  option (USE_PETALINUX "Build Petalinux image" OFF)
  option (BUILD_PETALINUX_SYSROOT "Build Petalinux SDK (sysroot)" OFF)
  if (BUILD_PETALINUX_SYSROOT)
    # PETALINUX_SDK is the directory where the SDK will be installed
    set (PETALINUX_SDK     "${CMAKE_BINARY_DIR}/petalinux/sdk")
    # PETALINUX_SYSROOT is where the sysroot will be installed (within the SDK)
    set (PETALINUX_SYSROOT "${PETALINUX_SDK}/sysroots/cortexa9t2hf-neon-xilinx-linux-gnueabi")
    # PETALINUX_SDK_TARGET is the CMake target
    set (PETALINUX_SDK_TARGET "fpgav3-sdk")
  endif ()
endif ()

if (APPLE)
  # Turn on USE_PETALINUX_SYSROOT because that is the only option supported on Mac OSX
  option (USE_PETALINUX_SYSROOT "Use Petalinux sysroot for cross-compiling" ON)
else ()
  option (USE_PETALINUX_SYSROOT "Use Petalinux sysroot for cross-compiling" OFF)
endif ()

if (USE_PETALINUX_SYSROOT AND NOT BUILD_PETALINUX_SYSROOT)
    # If BUILD_PETALINUX_SYSROOT is ON, then PETALINUX_SYSROOT is already set.
    # Otherwise, we set it to a user-specified sysroot, which defaults to the latest release
    set (PETALINUX_SYSROOT_EXTERNAL
         "https://github.com/jhu-cisst/mechatronics-embedded/releases/latest/download/fpgav3-sysroot-cortexa9.zip"
         CACHE STRING "URL or path to external sysroot (from Petalinux)")
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

  option (BUILD_FPGAV30 "Build for FPGA V3.0 (instead of V3.1)" OFF)
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

  find_package (mechatronics-firmware)

  if (mechatronics-firmware_FOUND)

    set (BIT_FILE ${FPGAV3_BCFG_BIT_FILE})

  else ()

    message (STATUS "mechatronics-firmware not found -- manually specify BIT_FILE")
    find_file (BIT_FILE_MANUAL "FPGA1394V3-BCFG.bit" DOC "FPGA compiled firmware, FPGA1394V3-BCFG.bit")
    set (BIT_FILE ${BIT_FILE_MANUAL})

  endif ()

endif ()

set (Xilinx_VERSION "")

if (USE_VIVADO)

  find_package (Vivado)

  if (Vivado_FOUND)

    include(${VIVADO_USE_FILE})
    set (Xilinx_VERSION ${Vivado_VERSION})

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

if (USE_VITIS)

  find_package(Vitis)

  if (Vitis_FOUND)

    if (Vivado_FOUND AND NOT Vitis_VERSION STREQUAL Vivado_VERSION)

      message (SEND_ERROR "Vitis ${Vitis_VERSION} is not compatible with Vivado ${Vivado_VERSION}")

    else ()

      set (Xilinx_VERSION ${Vitis_VERSION})
      include(${VITIS_USE_FILE})

      add_subdirectory (platform_standalone)

      add_subdirectory (platform_linux)

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

      add_subdirectory (petalinux)

    endif ()

  else ()

    message (WARNING "Petalinux image will not be built because tools not found")

  endif ()

endif ()

#
# Write a simple mechatronics-embedded-config.cmake file
#
set (CONFIG_FILE "${CMAKE_CURRENT_BINARY_DIR}/mechatronics-embedded-config.cmake")
file (WRITE  ${CONFIG_FILE} "#\n# CMake configuration file for mechatronics-embedded\n#\n\n")
file (APPEND ${CONFIG_FILE} "set (EMBEDDED_VERSION_MAJOR ${EMBEDDED_VERSION_MAJOR})\n")
file (APPEND ${CONFIG_FILE} "set (EMBEDDED_VERSION_MINOR ${EMBEDDED_VERSION_MINOR})\n")
file (APPEND ${CONFIG_FILE} "set (EMBEDDED_VERSION_PATCH ${EMBEDDED_VERSION_PATCH})\n")
file (APPEND ${CONFIG_FILE} "set (EMBEDDED_VERSION       \"${EMBEDDED_VERSION}\")\n\n")
if (mechatronics-firmware_FOUND)
  file (APPEND ${CONFIG_FILE} "set (FIRMWARE_VERSION ${FIRMWARE_VERSION})\n\n")
endif ()
if (FPGA_VERSION)
  file (APPEND ${CONFIG_FILE} "set (FPGA_VERSION     ${FPGA_VERSION})\n\n")
endif ()
file (APPEND ${CONFIG_FILE} "set (USE_VIVADO       ${USE_VIVADO})\n")
file (APPEND ${CONFIG_FILE} "set (USE_VITIS        ${USE_VITIS})\n")
file (APPEND ${CONFIG_FILE} "set (USE_PETALINUX    ${USE_PETALINUX})\n\n")
if (BUILD_PETALINUX_SYSROOT OR USE_PETALINUX_SYSROOT)
  file (APPEND ${CONFIG_FILE} "set (PETALINUX_SYSROOT      \"${PETALINUX_SYSROOT}\")\n\n")
endif ()
if (Xilinx_VERSION)
  file (APPEND ${CONFIG_FILE} "set (Xilinx_VERSION   ${Xilinx_VERSION})\n")
endif ()

#
# Mechatronics toolchain files for FPGA V3
#
# To cross-compile with Vitis:
#    cmake  -DCMAKE_TOOLCHAIN_FILE=<path-to-toolchain>/toolchain_vitis_fpgav3.cmake  <path-to-source>
#
# To cross-compile with clang:
#    cmake  -DCMAKE_TOOLCHAIN_FILE=<path-to-toolchain>/toolchain_clang_fpgav3.cmake  <path-to-source>

if (Vitis_FOUND AND (BUILD_PETALINUX_SYSROOT OR USE_PETALINUX_SYSROOT))

  # Create a custom toolchain file for cross-compiling
  set (TOOLCHAIN_FILE "${CMAKE_CURRENT_BINARY_DIR}/toolchain_vitis_fpgav3.cmake")
  file (WRITE  ${TOOLCHAIN_FILE} "#\n# Toolchain file for FPGA V3\n#\n")
  file (APPEND ${TOOLCHAIN_FILE} "# Usage: cmake -DCMAKE_TOOLCHAIN_FILE=${CMAKE_CURRENT_BINARY_DIR}/toolchain_vitis_fpgav3.cmake"
                                 "  <path-to-source>\n\n")
  file (APPEND ${TOOLCHAIN_FILE} "set (Sysroot ${PETALINUX_SYSROOT})\n")
  file (APPEND ${TOOLCHAIN_FILE} "set (VITIS_ROOT ${VITIS_ROOT})\n")
  file (APPEND ${TOOLCHAIN_FILE} "\n# Following is patched version from Xilinx repository\n\n")

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
  file (APPEND ${TOOLCHAIN_FILE} "${FILE_CONTENTS}")

endif ()

if (BUILD_PETALINUX_SYSROOT OR USE_PETALINUX_SYSROOT)

  set (GCC_VERSION "12.2.0")   # Xilinx 2023.1

  configure_file ("${CMAKE_CURRENT_SOURCE_DIR}/cmake/toolchain_clang_fpgav3.cmake.in"
                  "${CMAKE_CURRENT_BINARY_DIR}/toolchain_clang_fpgav3.cmake"
                  @ONLY)

endif ()
