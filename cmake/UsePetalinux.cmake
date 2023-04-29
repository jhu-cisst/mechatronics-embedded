##########################################################################################
#
# UsePetalinux
#
# This package assumes that find_package(Petalinux) has been called.
# It contains the following functions:
#
##########################################################################################
#
# Function: petalinux_create
#
# Parameters:
#   - PROJ_NAME          Project name (also used as CMake target name)
#   - HW_FILE            Input hardware file (XSA)
#   - CONFIG_MENU        ON --> show config menus and copy back if changed (default is OFF)
#   - CONFIG_SRC_DIR     Source directory for config files
#   - DEVICE_TREE_FILES  List of device tree files
#
# Description:
#   This function assumes that the sources for non-default configuration files
#   are provided in the CONFIG_SRC_DIR directory and that the custom device-tree
#   information is specified in DEVICE_TREE_FILES.
#
#   To change the configuration, set CONFIG_MENU ON via CMake. This will
#   cause the system to show the configuration menus and update the files
#   in the project-spec/configs sub-directory in the build tree, which are
#   then copied to the configs folder in the source tree so that the repository
#   can be updated (if desired).
#
##########################################################################################
#
# Function: petalinux_app_create
#
# Parameters:
#   - APP_NAME           Application name (also used as CMake target name)
#   - PROJ_NAME          Project name
#   - CONFIG_SRC_DIR     Source directory for config files
#   - APP_TEMPLATE       Application template (c, c++, autoconf, fpgamanager, install)
#   - APP_SOURCES        List of source files
#
# Description:
#   This function creates the application and then overwrites it with the specified
#   source files. The APP_TEMPLATE parameter is optional (defaults to c).
#
##########################################################################################
#
# Function: petalinux_build
#
# Parameters:
#   - TARGET_NAME        Target name (for CMake)
#   - PROJ_NAME          Project name
#   - BIT_FILE           BIT file to use for boot image (optional)
#   - DEPENDENCIES       Build dependencies
#
# Description:
#
#  This function builds and packages the petalinux image. The dependency on ${PROJ_NAME}
#  (built by petalinux_create) is already incorporated, so the DEPENDENCIES parameter
#  is used to specify additional dependencies, such as any apps that were created.
#
##########################################################################################

################################ petalinux_create ########################################

function (petalinux_create ...)

  # set all keywords and their values to ""
  set (FUNCTION_KEYWORDS
       PROJ_NAME
       HW_FILE
       CONFIG_MENU
       CONFIG_SRC_DIR
       DEVICE_TREE_FILES)

  # reset local variables
  foreach(keyword ${FUNCTION_KEYWORDS})
    set (${keyword} "")
  endforeach(keyword)
  set (CONFIG_MENU OFF)

  # parse input
  foreach (arg ${ARGV})
    list (FIND FUNCTION_KEYWORDS ${arg} ARGUMENT_IS_A_KEYWORD)
    if (${ARGUMENT_IS_A_KEYWORD} GREATER -1)
      set (CURRENT_PARAMETER ${arg})
      set (${CURRENT_PARAMETER} "")
    else (${ARGUMENT_IS_A_KEYWORD} GREATER -1)
      set (${CURRENT_PARAMETER} ${${CURRENT_PARAMETER}} ${arg})
    endif (${ARGUMENT_IS_A_KEYWORD} GREATER -1)
  endforeach (arg)

  if (PROJ_NAME AND HW_FILE AND CONFIG_SRC_DIR AND DEVICE_TREE_FILES)

    set (CONFIG_SRC_FILES "${CONFIG_SRC_DIR}/config" "${CONFIG_SRC_DIR}/rootfs_config")
    set (CONFIG_BIN_DIR   "${CMAKE_CURRENT_BINARY_DIR}/${PROJ_NAME}/project-spec/configs")
    set (CONFIG_BIN_FILES "${CONFIG_BIN_DIR}/config" "${CONFIG_BIN_DIR}/rootfs_config")

    set (DEVICE_TREE_BIN_DIR "${CMAKE_CURRENT_BINARY_DIR}/${PROJ_NAME}/project-spec/meta-user/recipes-bsp/device-tree/files")

    # Create the project. This does not take very long.
    set (PETALINUX_CREATE_OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/${PROJ_NAME}/config.project")
    add_custom_command (
        OUTPUT ${PETALINUX_CREATE_OUTPUT}
        COMMAND petalinux-create -t project --force --template zynq -n ${PROJ_NAME}
        COMMENT "Creating Petalinux project ${PROJ_NAME}")

    set (PETALINUX_CONFIG_HW_OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/${PROJ_NAME}/project-spec/hw-description/system.xsa")
    set (PETALINUX_CONFIG_OUTPUT    "${CONFIG_BIN_DIR}/config")

    if (CONFIG_MENU)

      add_custom_command (
          OUTPUT ${PETALINUX_CONFIG_HW_OUTPUT}
          # Copy the config files if needed
          COMMAND ${CMAKE_COMMAND}
                  ARGS -E copy_if_different
                  "${CONFIG_SRC_DIR}/config"
                  ${CONFIG_BIN_DIR}
          # Specify the hardware description (XSA) file. This does not take very long and has one config menu.
          COMMAND petalinux-config -p ${PROJ_NAME} --get-hw-description ${HW_FILE}
          # Copy back to the source tree in case config was changed
          COMMAND ${CMAKE_COMMAND}
                  ARGS -E copy_if_different
                  "${CONFIG_BIN_DIR}/config"
                  ${CONFIG_SRC_DIR}
          COMMENT "Configuring hardware from ${HW_FILE}"
          DEPENDS ${HW_FILE} "${CONFIG_SRC_DIR}/config" ${PETALINUX_CREATE_OUTPUT})

      add_custom_command (
          OUTPUT ${PETALINUX_CONFIG_OUTPUT}
          # Copy the config files if needed
          COMMAND ${CMAKE_COMMAND}
                  ARGS -E copy_if_different
                  ${CONFIG_SRC_FILES}
                  ${CONFIG_BIN_DIR}
          # Copy the device-tree file if needed
          COMMAND ${CMAKE_COMMAND}
                  ARGS -E copy_if_different
                  ${DEVICE_TREE_FILES}
                  ${DEVICE_TREE_BIN_DIR}
          # Configure the kernel. This takes a long time and has one config menu.
          COMMAND petalinux-config -p ${PROJ_NAME} -c kernel
          # Configure the rootfs. This does not take a long time and has one config menu.
          COMMAND petalinux-config -p ${PROJ_NAME} -c rootfs
          # Copy back to the source tree in case config was changed
          COMMAND ${CMAKE_COMMAND}
                  ARGS -E copy_if_different
                  ${CONFIG_BIN_FILES}
                  ${CONFIG_SRC_DIR}
          # Touch PETALINUX_CONFIG_HW_OUTPUT (system.xsa) so that petalinux-config is
          # not required to be run again
          COMMAND ${CMAKE_COMMAND}
                  ARGS -E touch_nocreate
                  ${PETALINUX_CONFIG_HW_OUTPUT}
          COMMENT "Petalinux configuration"
          DEPENDS ${PETALINUX_CONFIG_HW_OUTPUT} ${CONFIG_SRC_FILES} ${DEVICE_TREE_FILES})

    else (CONFIG_MENU)

      # No menu, so do not copy files back to source directory

      add_custom_command (
          OUTPUT ${PETALINUX_CONFIG_HW_OUTPUT}
          # Copy the config files if needed
          COMMAND ${CMAKE_COMMAND}
                  ARGS -E copy_if_different
                  "${CONFIG_SRC_DIR}/config"
                  ${CONFIG_BIN_DIR}
          # Specify the hardware description (XSA) file. This does not take very long.
          COMMAND petalinux-config -p ${PROJ_NAME} --get-hw-description ${HW_FILE} --silentconfig
          COMMENT "Configuring hardware from ${HW_FILE}"
          DEPENDS ${HW_FILE} "${CONFIG_SRC_DIR}/config" ${PETALINUX_CREATE_OUTPUT})

      add_custom_command (
          OUTPUT ${PETALINUX_CONFIG_OUTPUT}
          # Copy the config files if needed
          COMMAND ${CMAKE_COMMAND}
                  ARGS -E copy_if_different
                  ${CONFIG_SRC_FILES}
                  ${CONFIG_BIN_DIR}
          # Copy the device-tree file if needed
          COMMAND ${CMAKE_COMMAND}
                  ARGS -E copy_if_different
                  ${DEVICE_TREE_FILES}
                  ${DEVICE_TREE_BIN_DIR}
          # Configure the kernel. This takes a long time.
          COMMAND petalinux-config -p ${PROJ_NAME} -c kernel --silentconfig
          # Configure the rootfs. This does not take a long time.
          COMMAND petalinux-config -p ${PROJ_NAME} -c rootfs --silentconfig
          COMMENT "Petalinux configuration"
          DEPENDS ${PETALINUX_CONFIG_HW_OUTPUT} ${CONFIG_SRC_FILES} ${DEVICE_TREE_FILES})

    endif (CONFIG_MENU)

    add_custom_target(${PROJ_NAME} ALL
                      DEPENDS ${PETALINUX_CONFIG_OUTPUT})

  else (PROJ_NAME AND HW_FILE AND CONFIG_SRC_DIR AND DEVICE_TREE_FILES)

    message (SEND_ERROR "petalinux_create: required parameter missing")

  endif (PROJ_NAME AND HW_FILE AND CONFIG_SRC_DIR AND DEVICE_TREE_FILES)

endfunction (petalinux_create)

################################ petalinux_app_create ####################################

function (petalinux_app_create ...)

  # set all keywords and their values to ""
  set (FUNCTION_KEYWORDS
       APP_NAME
       PROJ_NAME
       CONFIG_SRC_DIR
       APP_TEMPLATE
       APP_SOURCES)

  # reset local variables
  foreach(keyword ${FUNCTION_KEYWORDS})
    set (${keyword} "")
  endforeach(keyword)
  set (APP_TEMPLATE c)

  # parse input
  foreach (arg ${ARGV})
    list (FIND FUNCTION_KEYWORDS ${arg} ARGUMENT_IS_A_KEYWORD)
    if (${ARGUMENT_IS_A_KEYWORD} GREATER -1)
      set (CURRENT_PARAMETER ${arg})
      set (${CURRENT_PARAMETER} "")
    else (${ARGUMENT_IS_A_KEYWORD} GREATER -1)
      set (${CURRENT_PARAMETER} ${${CURRENT_PARAMETER}} ${arg})
    endif (${ARGUMENT_IS_A_KEYWORD} GREATER -1)
  endforeach (arg)

  if (PROJ_NAME AND APP_NAME AND CONFIG_SRC_DIR AND APP_SOURCES)

    if (APP_TEMPLATE STREQUAL "fpgamanager")
      # For fpgamanager, files are in recipes-firmware and it is assumed that APP_SOURCES specifies just one file (the BIT file),
      # which is copied and renamed to system.bit
      set (APP_FILES_BIN     "${CMAKE_CURRENT_BINARY_DIR}/${PROJ_NAME}/project-spec/meta-user/recipes-firmware/${APP_NAME}/files/system.bit")
      set (APP_CREATE_OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/${PROJ_NAME}/project-spec/meta-user/recipes-firmware/${APP_NAME}/${APP_NAME}.bb")
    else (APP_TEMPLATE STREQUAL "fpgamanager")
      # For all other apps, files are in recipes-apps. In this case APP_SOURCES can specify more than one file, though further
      # updates may be necessary to support this (e.g., via .bbappend)
      set (APP_FILES_BIN     "${CMAKE_CURRENT_BINARY_DIR}/${PROJ_NAME}/project-spec/meta-user/recipes-apps/${APP_NAME}/files")
      set (APP_CREATE_OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/${PROJ_NAME}/project-spec/meta-user/recipes-apps/${APP_NAME}/${APP_NAME}.bb")
    endif (APP_TEMPLATE STREQUAL "fpgamanager")

    set (ROOTFS_CONFIG     "${CMAKE_CURRENT_BINARY_DIR}/${PROJ_NAME}/project-spec/configs/rootfs_config")
    set (PETALINUX_CONFIG_HW_OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/${PROJ_NAME}/project-spec/hw-description/system.xsa")

    add_custom_command (
        OUTPUT ${APP_CREATE_OUTPUT}
        COMMAND petalinux-create -p ${PROJ_NAME} -t apps --template ${APP_TEMPLATE} --name ${APP_NAME} --enable --force
        # Enabling the app modifies configs/rootfs_config, so copy that back to the source tree
        COMMAND ${CMAKE_COMMAND}
                ARGS -E copy_if_different
                ${ROOTFS_CONFIG}
                ${CONFIG_SRC_DIR}
        # Copy the source files
        COMMAND ${CMAKE_COMMAND}
                ARGS -E copy_if_different
                ${APP_SOURCES}
                ${APP_FILES_BIN}
        # Touch PETALINUX_CONFIG_HW_OUTPUT (system.xsa) so that petalinux-config is
        # not required to be run again
        COMMAND ${CMAKE_COMMAND}
                ARGS -E touch_nocreate
                ${PETALINUX_CONFIG_HW_OUTPUT}
        COMMENT "Creating ${APP_NAME}"
        DEPENDS ${APP_SOURCES})

    add_custom_target(${APP_NAME} ALL
                      DEPENDS ${APP_CREATE_OUTPUT})

  else (PROJ_NAME AND APP_NAME AND CONFIG_SRC_DIR AND APP_SOURCES)

    message (SEND_ERROR "petalinux_app_create: required parameter missing")

  endif (PROJ_NAME AND APP_NAME AND CONFIG_SRC_DIR AND APP_SOURCES)

endfunction (petalinux_app_create)

################################ petalinux_build #########################################

function (petalinux_build ...)

  # set all keywords and their values to ""
  set (FUNCTION_KEYWORDS
       TARGET_NAME
       PROJ_NAME
       BIT_FILE
       DEPENDENCIES)

  # reset local variables
  foreach(keyword ${FUNCTION_KEYWORDS})
    set (${keyword} "")
  endforeach(keyword)

  # parse input
  foreach (arg ${ARGV})
    list (FIND FUNCTION_KEYWORDS ${arg} ARGUMENT_IS_A_KEYWORD)
    if (${ARGUMENT_IS_A_KEYWORD} GREATER -1)
      set (CURRENT_PARAMETER ${arg})
      set (${CURRENT_PARAMETER} "")
    else (${ARGUMENT_IS_A_KEYWORD} GREATER -1)
      set (${CURRENT_PARAMETER} ${${CURRENT_PARAMETER}} ${arg})
    endif (${ARGUMENT_IS_A_KEYWORD} GREATER -1)
  endforeach (arg)

  if (TARGET_NAME AND PROJ_NAME)

    set (PETALINUX_IMAGE_DIR  "${CMAKE_CURRENT_BINARY_DIR}/${PROJ_NAME}/images/linux")

    # Outputs of petalinux-build
    set (PETALINUX_IMAGE_UB   "${PETALINUX_IMAGE_DIR}/image.ub")
    set (PETALINUX_FSBL_FILE  "${PETALINUX_IMAGE_DIR}/zynq_fsbl.elf")
    set (PETALINUX_UBOOT_FILE "${PETALINUX_IMAGE_DIR}/u-boot.elf")

    add_custom_command (
        OUTPUT ${PETALINUX_IMAGE_UB} ${PETALINUX_FSBL_FILE} ${PETALINUX_UBOOT_FILE}
        COMMAND petalinux-build -p ${PROJ_NAME}
        COMMENT "Building petalinux"
        DEPENDS ${PROJ_NAME} ${DEPENDENCIES})

    # Output of petalinux-package
    set (PETALINUX_BOOT_FILE  "${PETALINUX_IMAGE_DIR}/BOOT.bin")

    if (BIT_FILE)

      add_custom_command (
          OUTPUT ${PETALINUX_BOOT_FILE}
          COMMAND petalinux-package -p ${PROJ_NAME} --boot --fsbl ${PETALINUX_FSBL_FILE} --fpga ${BIT_FILE}
                                    --u-boot ${PETALINUX_UBOOT_FILE} --force -o ${PETALINUX_BOOT_FILE}
          COMMENT "Petalinux package (boot image)"
          DEPENDS ${BIT_FILE} ${PETALINUX_IMAGE_UB} ${PETALINUX_FSBL_FILE} ${PETALINUX_UBOOT_FILE})

    else (BIT_FILE)

      add_custom_command (
          OUTPUT ${PETALINUX_BOOT_FILE}
          COMMAND petalinux-package -p ${PROJ_NAME} --boot --fsbl ${PETALINUX_FSBL_FILE}
                                    --u-boot ${PETALINUX_UBOOT_FILE} --force -o ${PETALINUX_BOOT_FILE}
          COMMENT "Petalinux package (boot image) without BIT file"
                "${CMAKE_CURRENT_BINARY_DIR}/${PROJ_NAME}/project-spec/configs"
          DEPENDS ${PETALINUX_IMAGE_UB} ${PETALINUX_FSBL_FILE} ${PETALINUX_UBOOT_FILE})

    endif (BIT_FILE)

    add_custom_target(${TARGET_NAME} ALL
                      DEPENDS ${PETALINUX_BOOT_FILE})

  else (TARGET_NAME AND PROJ_NAME)

    message (SEND_ERROR "petalinux_build: required parameter missing")

  endif (TARGET_NAME AND PROJ_NAME)

endfunction (petalinux_build)
