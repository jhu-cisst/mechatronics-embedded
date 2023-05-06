
# Set CMake minimum version requirement.
# Need 3.15+ for ADDITIONAL_CLEAN_FILES property
# CMake 3.16 improved dependency checking in add_custom_target
cmake_minimum_required(VERSION 3.16)

project(FpgaBoard NONE)

set (CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

find_file (BIT_FILE "FPGA1394V3-BCFG.bit" DOC "FPGA compiled firmware, FPGA1394V3-BCFG.bit")

option (USE_VIVADO "Use Vivado to build block_design (XSA file)" ON)

option (USE_VITIS "Use Vitis to build platforms (standalone and linux)" ON)

if (WIN32)
  set (USE_PETALINUX OFF)
  set (CMAKE_CONFIGURATION_TYPES "Release")   # For Visual Studio
else (WIN32)
  # Can only build Petalinux on Linux
  option (USE_PETALINUX "Build Petalinux image" OFF)
endif (WIN32)

option (BUILD_FPGAV30 "Build for FPGA V3.0 (instead of V3.1)" OFF)
mark_as_advanced (BUILD_FPGAV30)

if (USE_VIVADO)

  find_package(Vivado)

  if (Vivado_FOUND)

    include(${VIVADO_USE_FILE})

    if (BUILD_FPGAV30)
      set (HW_XSA_FILE  "${CMAKE_CURRENT_BINARY_DIR}/block_design/FpgaV30.xsa")
    else (BUILD_FPGAV30)
      set (HW_XSA_FILE  "${CMAKE_CURRENT_BINARY_DIR}/block_design/FpgaV31.xsa")
    endif (BUILD_FPGAV30)

    add_subdirectory(block_design)

  else (Vivado_FOUND)

    message(SEND_ERROR "Xilinx Vivado not found")

  endif (Vivado_FOUND)

else (USE_VIVADO)

  if (BUILD_FPGAV30)
    find_file (HW_XSA_FILE "FpgaV30.xsa" DOC "XSA file produced by Vivado")
  else (BUILD_FPGAV30)
    find_file (HW_XSA_FILE "FpgaV31.xsa" DOC "XSA file produced by Vivado")
  endif (BUILD_FPGAV30)

endif (USE_VIVADO)

if (USE_VITIS)

  find_package(Vitis)

  if (Vitis_FOUND)

    include(${VITIS_USE_FILE})

    add_subdirectory(platform_standalone)

    add_subdirectory(platform_linux)

  else (Vitis_FOUND)

    message(SEND_ERROR "Xilinx Vitis not found")

  endif (Vitis_FOUND)

endif (USE_VITIS)

if (USE_PETALINUX)

  find_package(Petalinux)

  if (Petalinux_FOUND)

    include(${PETALINUX_USE_FILE})

    add_subdirectory(petalinux)

  else (Petalinux_FOUND)

    message(WARNING "Petalinux image will not be built because tools not found")

  endif (Petalinux_FOUND)

endif (USE_PETALINUX)
