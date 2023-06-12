
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

# Custom device tree
set (PETALINUX_DEVICE_TREE "${CMAKE_CURRENT_SOURCE_DIR}/device-tree/system-user.dtsi")

petalinux_create (PROJ_NAME          "fpgav3"
                  HW_FILE            ${HW_XSA_FILE}
                  CONFIG_MENU        ${PETALINUX_MENU}
                  CONFIG_SRC         "${PETALINUX_CONFIG_VER_DIR}/config"
                  BSP_CFG_SRC        "${PETALINUX_CONFIG_DIR}/bsp.cfg"
                  DEVICE_TREE_FILES  ${PETALINUX_DEVICE_TREE})

set (FPGAV3INIT_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/fpgav3init/files/fpgav3init.c"
                        "${CMAKE_CURRENT_SOURCE_DIR}/fpgav3init/files/fpgav3init.service")

set (FPGAV3INIT_BBAPPEND "${CMAKE_CURRENT_SOURCE_DIR}/fpgav3init/fpgav3init.bbappend")

petalinux_app_create (APP_NAME       "fpgav3init"
                      PROJ_NAME      "fpgav3"
                      APP_SOURCES    ${FPGAV3INIT_SOURCES}
                      APP_BBAPPEND   ${FPGAV3INIT_BBAPPEND})

set (FPGAV3SN_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/fpgav3sn/files/fpgav3sn.c")

petalinux_app_create (APP_NAME       "fpgav3sn"
                      PROJ_NAME      "fpgav3"
                      APP_SOURCES    ${FPGAV3SN_SOURCES})

set (PETALINUX_BUILD_DEPS "fpgav3init" ${FPGAV3INIT_SOURCES} ${FPGAV3INIT_BBAPPEND}
                          "fpgav3sn"   ${FPGAV3SN_SOURCES})

petalinux_build (TARGET_NAME         "fpgav3_build"
                 PROJ_NAME           "fpgav3"
                 CONFIG_MENU         ${PETALINUX_MENU}
                 ROOTFS_CONFIG_SRC   "${PETALINUX_CONFIG_VER_DIR}/rootfs_config"
                 BIT_FILE            ${BIT_FILE}
                 DEPENDENCIES        ${PETALINUX_BUILD_DEPS})

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
