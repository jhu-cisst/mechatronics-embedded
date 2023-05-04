
option (PETALINUX_MENU "Show Petalinux config menus" OFF)

# Directory to custom configuration files, config and rootfs_config
set (PETALINUX_CONFIG_DIR  "${CMAKE_CURRENT_SOURCE_DIR}/configs")

# Custom configuration files
set (PETALINUX_CONFIG_FILES "${PETALINUX_CONFIG_DIR}/config"
                            "${PETALINUX_CONFIG_DIR}/rootfs_config")

# Custom device tree
set (PETALINUX_DEVICE_TREE "${CMAKE_CURRENT_SOURCE_DIR}/device-tree/system-user.dtsi")

petalinux_create(PROJ_NAME          "fpgav3"
                 HW_FILE            ${HW_XSA_FILE}
                 CONFIG_MENU        ${PETALINUX_MENU}
                 CONFIG_SRC_DIR     ${PETALINUX_CONFIG_DIR}
                 DEVICE_TREE_FILES  ${PETALINUX_DEVICE_TREE})

set (FPGAV3INIT_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/fpgav3init/files/fpgav3init.c"
                        "${CMAKE_CURRENT_SOURCE_DIR}/fpgav3init/files/fpgav3init.service")

set (FPGAV3INIT_BBAPPEND "${CMAKE_CURRENT_SOURCE_DIR}/fpgav3init/fpgav3init.bbappend")

petalinux_app_create(APP_NAME       "fpgav3init"
                     PROJ_NAME      "fpgav3"
                     APP_SOURCES    ${FPGAV3INIT_SOURCES}
                     APP_BBAPPEND   ${FPGAV3INIT_BBAPPEND})

set (PETALINUX_BUILD_DEPS ${PETALINUX_CONFIG_FILES} ${PETALINUX_DEVICE_TREE} ${FPGAV3INIT_SOURCES} "fpgav3init")

petalinux_build(TARGET_NAME         "fpgav3_build"
                PROJ_NAME           "fpgav3"
                CONFIG_MENU         ${PETALINUX_MENU}
                CONFIG_SRC_DIR      ${PETALINUX_CONFIG_DIR}
                BIT_FILE            ${BIT_FILE}
                DEPENDENCIES        ${PETALINUX_BUILD_DEPS})
