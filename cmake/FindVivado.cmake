# Find Xilinx Vivado installation
#
# This will automatically find the necessary command line programs if the Xilinx Vivado binary directories are
# on the system PATH. Otherwise, it is necessary to manually specify the path.
#
# Future enhancements could include determining the Xilinx Vivado version number.


find_program(XILINX_VIVADO NAMES "vivado" DOC "Xilinx Vivado tool")

set (VIVADO_USE_FILE "${CMAKE_SOURCE_DIR}/cmake/UseVivado.cmake")

if (XILINX_VIVADO)
  set (Vivado_FOUND TRUE)
else()
  set (Vivado_FOUND FALSE)
endif()
