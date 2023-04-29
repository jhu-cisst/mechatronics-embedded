# Find Xilinx Vitis installation
#

find_program(VITIS_XSCT NAMES "xsct" DOC "Xilinx Vitis XSCT tool")

get_filename_component(VITIS_BIN ${VITIS_XSCT} PATH)

find_program(VITIS_BOOTGEN NAMES "bootgen" HINTS ${VITIS_BIN} DOC "Xilinx Vitis bootgen tool")

set (VITIS_USE_FILE "${CMAKE_SOURCE_DIR}/cmake/UseVitis.cmake")

if (VITIS_XSCT AND VITIS_BOOTGEN)
  set (Vitis_FOUND TRUE)
else()
  set (Vitis_FOUND FALSE)
endif()
