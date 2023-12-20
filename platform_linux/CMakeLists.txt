
############################ Platform #############################

if (BUILD_FPGAV30)
  set (PLATFORM_LINUX      "FpgaV30Linux")
else (BUILD_FPGAV30)
  set (PLATFORM_LINUX      "FpgaV31Linux")
endif (BUILD_FPGAV30)

if (USE_PETALINUX_SYSROOT)

  vitis_platform_create(
      PLATFORM_NAME     ${PLATFORM_LINUX}
      HW_FILE           ${HW_XSA_FILE}
      PROC_NAME         "ps7_cortexa9"
      OS_NAME           "linux"
      SYSROOT           ${PETALINUX_SYSROOT}
      DEPENDENCIES      ${BLOCK_DESIGN_NAME} ${PETALINUX_SDK_TARGET})

else ()

  vitis_platform_create(
      PLATFORM_NAME     ${PLATFORM_LINUX}
      HW_FILE           ${HW_XSA_FILE}
      PROC_NAME         "ps7_cortexa9"
      OS_NAME           "linux"
      DEPENDENCIES      ${BLOCK_DESIGN_NAME})

endif()

######################## fpgav3init App ##########################

if (USE_PETALINUX_SYSROOT)

  set (FPGAV3INIT_NAME   "fpgav3init")

  # Set a distinct target name to avoid conflict with petalinux build
  set (FPGAV3INIT_TARGET "fpgav3initonly")

  # Additional source files
  set (FPGAV3INIT_SOURCE "${CMAKE_SOURCE_DIR}/petalinux/fpgav3init/files/fpgav3init.c")

  vitis_create (APP
      TARGET_NAME    ${FPGAV3INIT_TARGET}
      APP_NAME       ${FPGAV3INIT_NAME}
      PLATFORM_NAME  ${PLATFORM_LINUX}
      TEMPLATE_NAME  "Linux Empty Application"
      ADD_SOURCE     ${FPGAV3INIT_SOURCE}
      LIBRARIES      "fpgav3"
      BUILD_CONFIG   "Release"
      DEPENDENCIES   ${PETALINUX_SDK_TARGET})

  get_property(FPGAV3INIT_OUTPUT TARGET ${FPGAV3INIT_TARGET} PROPERTY OUTPUT_NAME)

else ()

  message (STATUS "Skipping fpgav3init Vitis build (USE_PETALINUX_SYSROOT not set)")

endif ()

###################### libfpgav3 Library ########################

if (USE_PETALINUX_SYSROOT)

  set (LIBFPGAV3_NAME   "fpgav3")

  # Set a distinct target name to avoid conflict with petalinux build
  set (LIBFPGAV3_TARGET "libfpgav3only")

  # Additional source files
  set (LIBFPGAV3_SOURCE "${CMAKE_SOURCE_DIR}/petalinux/libfpgav3/files/fpgav3_emio.c"
                        "${CMAKE_SOURCE_DIR}/petalinux/libfpgav3/files/fpgav3_emio.h"
                        "${CMAKE_SOURCE_DIR}/petalinux/libfpgav3/files/fpgav3_qspi.c"
                        "${CMAKE_SOURCE_DIR}/petalinux/libfpgav3/files/fpgav3_qspi.h")

  vitis_create (LIBRARY
      TARGET_NAME    ${LIBFPGAV3_TARGET}
      LIB_NAME       ${LIBFPGAV3_NAME}
      PLATFORM_NAME  ${PLATFORM_LINUX}
      LIB_TYPE       "shared"
      ADD_SOURCE     ${LIBFPGAV3_SOURCE}
      LIBRARIES      "gpiod"
      BUILD_CONFIG   "Release"
      DEPENDENCIES   ${PETALINUX_SDK_TARGET})

  get_property(LIBFPGAV3_OUTPUT TARGET ${LIBFPGAV3_TARGET} PROPERTY OUTPUT_NAME)

else ()

  message (STATUS "Skipping libfpgav3 Vitis build (USE_PETALINUX_SYSROOT not set)")

endif ()
