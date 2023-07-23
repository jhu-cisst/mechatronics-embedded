
############################ Platform #############################

if (BUILD_FPGAV30)
  set (PLATFORM_LINUX      "FpgaV30Linux")
else (BUILD_FPGAV30)
  set (PLATFORM_LINUX      "FpgaV31Linux")
endif (BUILD_FPGAV30)

if (USE_PETALINUX_SYSROOT)

  set (PETALINUX_SDK_FILE "${PETALINUX_SDK}/cmake.sdk")
  set (PETALINUX_SYSROOT  "${PETALINUX_SDK}/sysroots/cortexa9t2hf-neon-xilinx-linux-gnueabi")

  vitis_platform_create(
      PLATFORM_NAME     ${PLATFORM_LINUX}
      HW_FILE           ${HW_XSA_FILE}
      PROC_NAME         "ps7_cortexa9"
      OS_NAME           "linux"
      SYSROOT           ${PETALINUX_SYSROOT}
      DEPENDENCIES      ${BLOCK_DESIGN_NAME} ${PETALINUX_SDK_FILE})

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
      DEPENDENCIES   "fpgav3-sdk")

  get_property(FPGAV3INIT_OUTPUT TARGET ${FPGAV3INIT_TARGET} PROPERTY OUTPUT_NAME)

else ()

  message (STATUS "Skipping fpgav3init Vitis build (USE_PETALINUX_SYSROOT not set)")

endif ()
