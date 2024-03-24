
############################ Platform #############################

if (BUILD_FPGAV30)
  set (PLATFORM_LINUX      "FpgaV30Linux")
else (BUILD_FPGAV30)
  set (PLATFORM_LINUX      "FpgaV31Linux")
endif (BUILD_FPGAV30)

set (PLATFORM_LINUX_DEPS ${BLOCK_DESIGN_NAME})

if (USE_PETALINUX_SYSROOT)

  if (BUILD_PETALINUX_SYSROOT)
    set (PLATFORM_LINUX_DEPS ${PLATFORM_LINUX_DEPS}
                             ${PETALINUX_SDK_TARGET})
  endif()

  vitis_platform_create(
      PLATFORM_NAME     ${PLATFORM_LINUX}
      HW_FILE           ${HW_XSA_FILE}
      PROC_NAME         "ps7_cortexa9"
      OS_NAME           "linux"
      SYSROOT           ${PETALINUX_SYSROOT}
      DEPENDENCIES      ${PLATFORM_LINUX_DEPS})

else ()

  vitis_platform_create(
      PLATFORM_NAME     ${PLATFORM_LINUX}
      HW_FILE           ${HW_XSA_FILE}
      PROC_NAME         "ps7_cortexa9"
      OS_NAME           "linux"
      DEPENDENCIES      ${PLATFORM_LINUX_DEPS})

endif()

######################## fpgav3init App ##########################

if (USE_PETALINUX_SYSROOT)

  set (FPGAV3INIT_NAME   "fpgav3init")

  # Set a distinct target name to avoid conflict with petalinux build
  set (FPGAV3INIT_TARGET "fpgav3initonly")

  # Additional source files
  set (FPGAV3INIT_SOURCE "${CMAKE_SOURCE_DIR}/petalinux/fpgav3init/files/fpgav3init.cpp")

  if (BUILD_PETALINUX_SYSROOT)

    vitis_create (APP
        TARGET_NAME    ${FPGAV3INIT_TARGET}
        APP_NAME       ${FPGAV3INIT_NAME}
        PLATFORM_NAME  ${PLATFORM_LINUX}
        TEMPLATE_NAME  "Empty Application (C++)"
        LANGUAGE       "c++"
        ADD_SOURCE     ${FPGAV3INIT_SOURCE}
        LIBRARIES      "fpgav3"
        BUILD_CONFIG   "Release"
        DEPENDENCIES   ${PETALINUX_SDK_TARGET})

    # Probably do not need to build fpgav3init standalone when also building Petalinux
    set_target_properties(${FPGAV3INIT_TARGET} PROPERTIES EXCLUDE_FROM_ALL ON)

  else ()

    vitis_create (APP
        TARGET_NAME    ${FPGAV3INIT_TARGET}
        APP_NAME       ${FPGAV3INIT_NAME}
        PLATFORM_NAME  ${PLATFORM_LINUX}
        TEMPLATE_NAME  "Empty Application (C++)"
        LANGUAGE       "c++"
        ADD_SOURCE     ${FPGAV3INIT_SOURCE}
        LIBRARIES      "fpgav3"
        BUILD_CONFIG   "Release")

  endif ()

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
  set (LIBFPGAV3_SOURCE "${CMAKE_SOURCE_DIR}/petalinux/libfpgav3/files/fpgav3_emio.cpp"
                        "${CMAKE_SOURCE_DIR}/petalinux/libfpgav3/files/fpgav3_emio.h"
                        "${CMAKE_SOURCE_DIR}/petalinux/libfpgav3/files/fpgav3_emio_gpiod.cpp"
                        "${CMAKE_SOURCE_DIR}/petalinux/libfpgav3/files/fpgav3_emio_gpiod.h"
                        "${CMAKE_SOURCE_DIR}/petalinux/libfpgav3/files/fpgav3_emio_mmap.cpp"
                        "${CMAKE_SOURCE_DIR}/petalinux/libfpgav3/files/fpgav3_emio_mmap.h"
                        "${CMAKE_SOURCE_DIR}/petalinux/libfpgav3/files/fpgav3_qspi.cpp"
                        "${CMAKE_SOURCE_DIR}/petalinux/libfpgav3/files/fpgav3_qspi.h")


  if (BUILD_PETALINUX_SYSROOT)

    vitis_create (LIBRARY
        TARGET_NAME    ${LIBFPGAV3_TARGET}
        LIB_NAME       ${LIBFPGAV3_NAME}
        PLATFORM_NAME  ${PLATFORM_LINUX}
        LIB_TYPE       "shared"
        LANGUAGE       "c++"
        ADD_SOURCE     ${LIBFPGAV3_SOURCE}
        LIBRARIES      "gpiod"
        BUILD_CONFIG   "Release"
        DEPENDENCIES   ${PETALINUX_SDK_TARGET})

    # Probably do not need to build libfpgav3 standalone when also building Petalinux
    set_target_properties(${LIBFPGAV3_TARGET} PROPERTIES EXCLUDE_FROM_ALL ON)

  else ()

    vitis_create (LIBRARY
        TARGET_NAME    ${LIBFPGAV3_TARGET}
        LIB_NAME       ${LIBFPGAV3_NAME}
        PLATFORM_NAME  ${PLATFORM_LINUX}
        LIB_TYPE       "shared"
        LANGUAGE       "c++"
        ADD_SOURCE     ${LIBFPGAV3_SOURCE}
        LIBRARIES      "gpiod"
        BUILD_CONFIG   "Release")

  endif ()

  get_property(LIBFPGAV3_OUTPUT TARGET ${LIBFPGAV3_TARGET} PROPERTY OUTPUT_NAME)


else ()

  message (STATUS "Skipping libfpgav3 Vitis build (USE_PETALINUX_SYSROOT not set)")

endif ()
