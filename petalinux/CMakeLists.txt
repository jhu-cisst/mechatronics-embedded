
option (PETALINUX_MENU "Show Petalinux config menus" OFF)

# Directory to custom configuration files: config, rootfs_config and bsp.cfg
set (PETALINUX_CONFIG_DIR  "${CMAKE_CURRENT_SOURCE_DIR}/configs")
if (Petalinux_VERSION STREQUAL "2022.2")
  set (PETALINUX_CONFIG_VER_DIR "${PETALINUX_CONFIG_DIR}/2022.2")
else ()
  if (NOT Petalinux_VERSION STREQUAL "2023.1")
    message (WARNING "Petalinux ${Petalinux_VERSION} not supported -- using config files from 2023.1")
  endif ()
  set (PETALINUX_CONFIG_VER_DIR "${PETALINUX_CONFIG_DIR}/2023.1")
endif ()

# ************************* Petalinux create ******************************

# Custom device tree
set (PETALINUX_DEVICE_TREE "${CMAKE_CURRENT_SOURCE_DIR}/device-tree/system-user.dtsi")

petalinux_create (PROJ_NAME          "fpgav3"
                  HW_FILE            ${HW_XSA_FILE}
                  CONFIG_MENU        ${PETALINUX_MENU}
                  CONFIG_SRC         "${PETALINUX_CONFIG_VER_DIR}/config"
                  BSP_CFG_SRC        "${PETALINUX_CONFIG_DIR}/bsp.cfg"
                  DEVICE_TREE_FILES  ${PETALINUX_DEVICE_TREE})

# ************************* fpgav3lib library *****************************

set (LIBFPGAV3_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/libfpgav3/files/fpgav3_emio.h"
                       "${CMAKE_CURRENT_SOURCE_DIR}/libfpgav3/files/fpgav3_emio.c"
                       "${CMAKE_CURRENT_SOURCE_DIR}/libfpgav3/files/fpgav3_qspi.h"
                       "${CMAKE_CURRENT_SOURCE_DIR}/libfpgav3/files/fpgav3_qspi.c"
                       "${CMAKE_CURRENT_SOURCE_DIR}/libfpgav3/files/Makefile")

set (LIBFPGAV3_BB      "${CMAKE_CURRENT_SOURCE_DIR}/libfpgav3/libfpgav3.bb")

petalinux_app_create (APP_NAME       "libfpgav3"
                      PROJ_NAME      "fpgav3"
                      APP_SOURCES    ${LIBFPGAV3_SOURCES}
                      APP_BB         ${LIBFPGAV3_BB})

# ************************** fpgav3init app *******************************

set (FPGAV3INIT_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/fpgav3init/files/fpgav3init.c"
                        "${CMAKE_CURRENT_SOURCE_DIR}/fpgav3init/files/fpgav3init.service")

set (FPGAV3INIT_BBAPPEND "${CMAKE_CURRENT_SOURCE_DIR}/fpgav3init/fpgav3init.bbappend")

petalinux_app_create (APP_NAME       "fpgav3init"
                      PROJ_NAME      "fpgav3"
                      APP_SOURCES    ${FPGAV3INIT_SOURCES}
                      APP_BB         ${FPGAV3INIT_BBAPPEND})

# ************************** fpgav3sn app ********************************

set (FPGAV3SN_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/fpgav3sn/files/fpgav3sn.c")

set (FPGAV3SN_BBAPPEND "${CMAKE_CURRENT_SOURCE_DIR}/fpgav3sn/fpgav3sn.bbappend")

petalinux_app_create (APP_NAME       "fpgav3sn"
                      PROJ_NAME      "fpgav3"
                      APP_SOURCES    ${FPGAV3SN_SOURCES}
                      APP_BB         ${FPGAV3SN_BBAPPEND})

set (PETALINUX_BUILD_DEPS "libfpgav3"  ${LIBFPGAV3_SOURCES}  ${LIBFPGAV3_BBAPPEND}
                          "fpgav3init" ${FPGAV3INIT_SOURCES} ${FPGAV3INIT_BBAPPEND}
                          "fpgav3sn"   ${FPGAV3SN_SOURCES}   ${FPGAV3SN_BBAPPEND})

# ************************** Petalinux build *******************************

set (PETALINUX_BUILD_TARGET "fpgav3_build")

petalinux_build (TARGET_NAME         ${PETALINUX_BUILD_TARGET}
                 PROJ_NAME           "fpgav3"
                 CONFIG_MENU         ${PETALINUX_MENU}
                 ROOTFS_CONFIG_SRC   "${PETALINUX_CONFIG_VER_DIR}/rootfs_config"
                 BIT_FILE            ${BIT_FILE}
                 DEPENDENCIES        ${PETALINUX_BUILD_DEPS})

# *********************** Petalinux SDK (sysroot) **************************

if (BUILD_PETALINUX_SYSROOT)

  petalinux_build_sdk (PROJ_NAME       "fpgav3"
                       SDK_INSTALL_DIR ${PETALINUX_SDK}
                       DEPENDENCIES    ${PETALINUX_BUILD_TARGET})

endif ()

if (mechatronics-firmware_FOUND AND Vitis_FOUND)

  # Create the MicroSD card image
  set (SD_IMAGE_DIR "${CMAKE_CURRENT_BINARY_DIR}/SD_Image")

  file (MAKE_DIRECTORY ${SD_IMAGE_DIR})
  set (VERSION_FILE "${SD_IMAGE_DIR}/version.txt")
  file (WRITE   ${VERSION_FILE} "mechatronics-embedded ${EMBEDDED_VERSION}\n")
  file (APPEND  ${VERSION_FILE} "mechatronics-firmware ${FIRMWARE_VERSION}\n")
  file (APPEND  ${VERSION_FILE} "xilinx-tools          ${Xilinx_VERSION}\n")
  file (APPEND  ${VERSION_FILE} "fpga-hw               ${FPGA_VERSION}\n")
  file (APPEND  ${VERSION_FILE} "espm-firmware         ${ESPM_FIRMWARE_VERSION}\n")

  set (PETALINUX_IMAGE_DIR  "${CMAKE_CURRENT_BINARY_DIR}/fpgav3/images/linux")
  set (ZYNQ_BOOT_FILE "${CMAKE_BINARY_DIR}/platform_standalone/zynq_boot/BOOT.bin")

  add_custom_target (MicroSDImage ALL
                     COMMAND ${CMAKE_COMMAND} -E copy_if_different
                             "${PETALINUX_IMAGE_DIR}/BOOT.bin"
                             ${SD_IMAGE_DIR}
                     COMMAND ${CMAKE_COMMAND} -E copy_if_different
                             "${PETALINUX_IMAGE_DIR}/boot.scr"
                             ${SD_IMAGE_DIR}
                     COMMAND ${CMAKE_COMMAND} -E copy_if_different
                             "${PETALINUX_IMAGE_DIR}/image.ub"
                             ${SD_IMAGE_DIR}
                     COMMAND ${CMAKE_COMMAND} -E copy_if_different
                             ${FPGAV3_HW_BIT_FILES}
                             ${SD_IMAGE_DIR}
                     COMMAND ${CMAKE_COMMAND} -E copy_if_different
                             ${ZYNQ_BOOT_FILE}
                             "${SD_IMAGE_DIR}/qspi-boot.bin"
                     COMMAND ${CMAKE_COMMAND} -E copy_if_different
                             ${ESPM_FIRMWARE_FILE}
                             ${SD_IMAGE_DIR}
                     COMMENT "Creating MicroSD Image"
                     DEPENDS "fpgav3_build")

else ()

  message (WARNING "Not configured to create MicroSD image")

endif ()
