# Find Xilinx Petalinux installation
#
# Assumes that Petalinux settings.sh has been sourced to set PATH and
# environment variables.
#

if (DEFINED ENV{PETALINUX})

  message (STATUS "Found Petalinux in $ENV{PETALINUX}")
  set (PETALINUX_USE_FILE "${CMAKE_SOURCE_DIR}/cmake/UsePetalinux.cmake")
  set (Petalinux_FOUND TRUE)

else (DEFINED ENV{PETALINUX})

  message (WARNING "Petalinux not found: source settings.sh (or csh) from Petalinux installation before starting CMake")
  set (Petalinux_FOUND FALSE)

endif (DEFINED ENV{PETALINUX})
