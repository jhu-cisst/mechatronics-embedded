# Find Xilinx Petalinux installation
#
# Assumes that Petalinux settings.sh has been sourced to set PATH and
# environment variables.
#

if (DEFINED ENV{PETALINUX})

  message (STATUS "Found Petalinux in $ENV{PETALINUX}")

  set (Petalinux_VERSION       $ENV{PETALINUX_VER})
  string (FIND ${Petalinux_VERSION} "." DOT_POS)
  if (DOT_POS EQUAL 4)
    string (SUBSTRING ${Petalinux_VERSION} 0  4 Petalinux_VERSION_MAJOR)
    string (SUBSTRING ${Petalinux_VERSION} 5 -1 Petalinux_VERSION_MINOR)
    message (STATUS "Found Petalinux ${Petalinux_VERSION} (${Petalinux_VERSION_MAJOR}, ${Petalinux_VERSION_MINOR})")
  else ()
    message (WARNING "FindPetalinux: invalid version string ${Petalinux_VERSION}")
  endif ()

  set (PETALINUX_USE_FILE "${CMAKE_SOURCE_DIR}/cmake/UsePetalinux.cmake")
  set (Petalinux_FOUND TRUE)

else (DEFINED ENV{PETALINUX})

  message (WARNING "Petalinux not found: source settings.sh (or csh) from Petalinux installation before starting CMake")
  set (Petalinux_FOUND FALSE)

endif (DEFINED ENV{PETALINUX})
