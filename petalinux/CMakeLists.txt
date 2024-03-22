
set (PETALINUX_PROJ_NAME "fpgav3")

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
# Custom recipes-core
set (PETALINUX_RECIPES_CORE "${CMAKE_CURRENT_SOURCE_DIR}/recipes-core")

petalinux_create (PROJ_NAME          ${PETALINUX_PROJ_NAME}
                  HW_FILE            ${HW_XSA_FILE}
                  CONFIG_MENU        ${PETALINUX_MENU}
                  CONFIG_SRC         "${PETALINUX_CONFIG_VER_DIR}/config"
                  BSP_CFG_SRC        "${PETALINUX_CONFIG_DIR}/bsp.cfg"
                  DEVICE_TREE_FILES  ${PETALINUX_DEVICE_TREE}
                  RECIPES_CORE_SRC_DIR ${PETALINUX_RECIPES_CORE})

# ************************** fpgav3 library *******************************

set (LIBFPGAV3_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/libfpgav3/files/fpgav3_emio.h"
                       "${CMAKE_CURRENT_SOURCE_DIR}/libfpgav3/files/fpgav3_emio.cpp"
                       "${CMAKE_CURRENT_SOURCE_DIR}/libfpgav3/files/fpgav3_emio_gpiod.h"
                       "${CMAKE_CURRENT_SOURCE_DIR}/libfpgav3/files/fpgav3_emio_gpiod.cpp"
                       "${CMAKE_CURRENT_SOURCE_DIR}/libfpgav3/files/fpgav3_qspi.h"
                       "${CMAKE_CURRENT_SOURCE_DIR}/libfpgav3/files/fpgav3_qspi.cpp"
                       "${CMAKE_CURRENT_SOURCE_DIR}/libfpgav3/files/Makefile")

set (LIBFPGAV3_BB      "${CMAKE_CURRENT_SOURCE_DIR}/libfpgav3/libfpgav3.bb")

petalinux_app_create (APP_NAME       "libfpgav3"
                      PROJ_NAME      ${PETALINUX_PROJ_NAME}
                      APP_SOURCES    ${LIBFPGAV3_SOURCES}
                      APP_TEMPLATE   "c++"
                      APP_BB         ${LIBFPGAV3_BB})

# ************************** fpgav3init app *******************************

set (FPGAV3INIT_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/fpgav3init/files/fpgav3init.cpp"
                        "${CMAKE_CURRENT_SOURCE_DIR}/fpgav3init/files/fpgav3init.service")

set (FPGAV3INIT_BBAPPEND "${CMAKE_CURRENT_SOURCE_DIR}/fpgav3init/fpgav3init.bbappend")

petalinux_app_create (APP_NAME       "fpgav3init"
                      PROJ_NAME      ${PETALINUX_PROJ_NAME}
                      APP_SOURCES    ${FPGAV3INIT_SOURCES}
                      APP_TEMPLATE   "c++"
                      APP_BB         ${FPGAV3INIT_BBAPPEND})

# ************************** fpgav3sn app ********************************

set (FPGAV3SN_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/fpgav3sn/files/fpgav3sn.cpp")

set (FPGAV3SN_BBAPPEND "${CMAKE_CURRENT_SOURCE_DIR}/fpgav3sn/fpgav3sn.bbappend")

petalinux_app_create (APP_NAME       "fpgav3sn"
                      PROJ_NAME      ${PETALINUX_PROJ_NAME}
                      APP_SOURCES    ${FPGAV3SN_SOURCES}
                      APP_TEMPLATE   "c++"
                      APP_BB         ${FPGAV3SN_BBAPPEND})

# ************************** fpgav3block app *******************************

set (FPGAV3BLOCK_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/fpgav3block/files/fpgav3block.cpp")

set (FPGAV3BLOCK_BBAPPEND "${CMAKE_CURRENT_SOURCE_DIR}/fpgav3block/fpgav3block.bbappend")

petalinux_app_create (APP_NAME       "fpgav3block"
                      PROJ_NAME      ${PETALINUX_PROJ_NAME}
                      APP_SOURCES    ${FPGAV3BLOCK_SOURCES}
                      APP_TEMPLATE   "c++"
                      APP_BB         ${FPGAV3BLOCK_BBAPPEND})

# ************************** Petalinux build *******************************

set (PETALINUX_BUILD_DEPS "libfpgav3"  ${LIBFPGAV3_SOURCES}  ${LIBFPGAV3_BB}
                          "fpgav3init" ${FPGAV3INIT_SOURCES} ${FPGAV3INIT_BBAPPEND}
                          "fpgav3sn"   ${FPGAV3SN_SOURCES}   ${FPGAV3SN_BBAPPEND}
                          "fpgav3block" ${FPGAV3BLOCK_SOURCES}   ${FPGAV3BLOCK_BBAPPEND})

set (PETALINUX_BUILD_TARGET "${PETALINUX_PROJ_NAME}_build")

petalinux_build (TARGET_NAME         ${PETALINUX_BUILD_TARGET}
                 PROJ_NAME           ${PETALINUX_PROJ_NAME}
                 CONFIG_MENU         ${PETALINUX_MENU}
                 ROOTFS_CONFIG_SRC   "${PETALINUX_CONFIG_VER_DIR}/rootfs_config"
                 BIT_FILE            ${BIT_FILE}
                 DEPENDENCIES        ${PETALINUX_BUILD_DEPS})

# *************** Petalinux generated output: MicroSD and sysroot (SDK) *********************

# Directory for generated ZIP files (SD-Image and sysroot)
set (PETALINUX_GENERATED_DIR "${CMAKE_CURRENT_BINARY_DIR}/${PETALINUX_PROJ_NAME}-generated")

set (README_GEN "${PETALINUX_GENERATED_DIR}/ReadMe.txt")
file (WRITE  ${README_GEN} "Directory created by CMake for generated files\n")

set (PETALINUX_SDK_DEPENDENCIES ${PETALINUX_BUILD_TARGET})

# ***************************** Petalinux MicroSD zip file **********************************

if (mechatronics-firmware_FOUND AND Vitis_FOUND)

  set (PETALINUX_MICROSD_ZIP_FILE "${PETALINUX_PROJ_NAME}-micro-sd.zip")
  set (PETALINUX_MICROSD_ZIP      "${PETALINUX_GENERATED_DIR}/${PETALINUX_MICROSD_ZIP_FILE}")

  file (APPEND ${README_GEN} "  ${PETALINUX_MICROSD_ZIP_FILE}: MicroSD image\n")

  # Create the MicroSD card image
  set (SD_IMAGE_DIR "${CMAKE_CURRENT_BINARY_DIR}/SD_Image")

  file (MAKE_DIRECTORY ${SD_IMAGE_DIR})

  # Following might be better implemented using configure_file
  set (README_SD  "${SD_IMAGE_DIR}/ReadMe.txt")
  file (WRITE   ${README_SD} "FPGA V3 MicroSD Files:\n\n")
  file (APPEND  ${README_SD} "  BOOT.bin:  First-stage boot loader (FSBL), programs BCFG firmware and loads Linux\n")
  file (APPEND  ${README_SD} "  boot.scr:  Boot script for u-boot, loads Linux\n")
  file (APPEND  ${README_SD} "  image.ub:  Linux kernel image\n")
  file (APPEND  ${README_SD} "  *.bit: Firmware for QLA, DQLA or DRAC\n")
  file (APPEND  ${README_SD} "  qspi-boot.bin:  First-stage boot loader (FSBL) for QSPI, standalone\n")
  file (APPEND  ${README_SD} "  ${ESPM_FIRMWARE_NAME}:  Firmware for ESPM Programmer\n")
  file (APPEND  ${README_SD} "  version.txt:  Specifies file versions used to build this image\n\n")
  file (APPEND  ${README_SD} "The FPGA V3 boot process begins by loading BOOT.bin, which programs the BCFG firmware and loads Linux.\n")
  file (APPEND  ${README_SD} "More precisely, BOOT.bin includes u-boot, which uses boot.scr to load Linux from image.ub.\n")
  file (APPEND  ${README_SD} "Once Linux boots, it runs an application, fpgav3init, that queries the BCFG firmware to determine\n")
  file (APPEND  ${README_SD} "which board (QLA, DQLA or DRAC) is connected and then loads the appropriate firmware (bit file).\n")
  file (APPEND  ${README_SD} "The fpgav3init program performs other initialization tasks, such as copying the FPGA S/N from the\n")
  file (APPEND  ${README_SD} "designated QSPI flash partition to registers in the FPGA firmware. It also programs the first QSPI\n")
  file (APPEND  ${README_SD} "flash partition with qspi-boot.bin, if the file has not already been programmed. The qspi-boot.bin\n")
  file (APPEND  ${README_SD} "provides a first-stage boot loader that programs the BCFG firmware and then loads a simple standalone\n")
  file (APPEND  ${README_SD} "application; it is only used when no MicroSD card is inserted.\n\n")
  file (APPEND  ${README_SD} "The ${ESPM_FIRMWARE_NAME} file is used by the ESPM programmer. It is stored on this MicroSD card so that\n")
  file (APPEND  ${README_SD} "it can be used either with FPGA V3, in which case ${ESPM_FIRMWARE_NAME} is not used, or with the ESPM\n")
  file (APPEND  ${README_SD} "programmer, in which case all other files are not used.\n")

  set (VERSION_FILE "${SD_IMAGE_DIR}/version.txt")
  file (WRITE   ${VERSION_FILE} "mechatronics-embedded ${EMBEDDED_VERSION}\n")
  file (APPEND  ${VERSION_FILE} "mechatronics-firmware ${FIRMWARE_VERSION}\n")
  file (APPEND  ${VERSION_FILE} "xilinx-tools          ${Xilinx_VERSION}\n")
  file (APPEND  ${VERSION_FILE} "fpga-hw               ${FPGA_VERSION}\n")
  file (APPEND  ${VERSION_FILE} "espm-firmware         ${ESPM_FIRMWARE_VERSION}\n")

  set (PETALINUX_IMAGE_DIR  "${CMAKE_CURRENT_BINARY_DIR}/${PETALINUX_PROJ_NAME}/images/linux")
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
                     DEPENDS "${PETALINUX_BUILD_TARGET}")

  set (MICROSD_FILES "${SD_IMAGE_DIR}/BOOT.bin" "${SD_IMAGE_DIR}/boot.scr"
                     "${SD_IMAGE_DIR}/image.ub" "${SD_IMAGE_DIR}/qspi-boot.bin")

  foreach (bit_file ${FPGAV3_HW_BIT_FILES})
    get_filename_component (bit_file_name ${bit_file} NAME)
    set (MICROSD_FILES ${MICROSD_FILES} "${SD_IMAGE_DIR}/${bit_file_name}")
  endforeach (bit_file)

  add_custom_command (OUTPUT ${PETALINUX_MICROSD_ZIP}
                      COMMAND zip -9 -q ${PETALINUX_MICROSD_ZIP} *
                      WORKING_DIRECTORY ${SD_IMAGE_DIR}
                      COMMENT "Creating ${PETALINUX_MICROSD_ZIP_FILE}"
                      DEPENDS ${MICROSD_FILES})

  add_custom_target (MicroSDzip ALL
                     COMMENT "Checking MicroSD Zip"
                     DEPENDS "MicroSDImage" ${PETALINUX_MICROSD_ZIP})

  # The Petalinux SDK does not actually depend on the MicroSD zip file,
  # but we set this dependency to control the build order
  set (PETALINUX_SDK_DEPENDENCIES ${PETALINUX_SDK_DEPENDENCIES} "MicroSDzip")

else ()

  message (WARNING "Not configured to create MicroSD image")

endif ()

# *********************** Petalinux SDK (sysroot) **************************

if (BUILD_PETALINUX_SYSROOT)

  set (PETALINUX_SYSROOT_ZIP_FILE "${PETALINUX_PROJ_NAME}-sysroot-cortexa9.zip")
  set (PETALINUX_SYSROOT_ZIP      "${PETALINUX_GENERATED_DIR}/${PETALINUX_SYSROOT_ZIP_FILE}")
  file (APPEND ${README_GEN} "  ${PETALINUX_SYSROOT_ZIP_FILE}: petalinux-generated sysroot (no debug files)\n")

  petalinux_build_sdk (TARGET_NAME     ${PETALINUX_SDK_TARGET}
                       PROJ_NAME       ${PETALINUX_PROJ_NAME}
                       SDK_INSTALL_DIR ${PETALINUX_SDK}
                       SYSROOT_ZIP     ${PETALINUX_SYSROOT_ZIP}
                       DEPENDENCIES    ${PETALINUX_SDK_DEPENDENCIES})

endif ()
