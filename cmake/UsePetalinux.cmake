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
#   - CONFIG_MENU        ON --> show config menus (default is OFF)
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
#   in the project-spec/configs sub-directory in the build tree.
#   Note that this function only updates the "config" file;
#   the "rootfs_config" is updated in petalinux_app_create and petalinux_build.
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
#   - APP_BBAPPEND       Application bbappend file (optional)
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
#   - CONFIG_MENU        ON --> show config menus (default is OFF)
#   - CONFIG_SRC_DIR     Source directory for config files
#   - BIT_FILE           BIT file to use for boot image (optional)
#   - DEPENDENCIES       Build dependencies
#
# Description:
#
#  This function first configures the rootfs, because that needs to be done after any
#  apps are created (via petalinux_app_create).
#  It then builds and packages the petalinux image. The dependency on ${PROJ_NAME}
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

    if (CONFIG_MENU)
      set (CONFIG_OPTION "")
    else (CONFIG_MENU)
      set (CONFIG_OPTION "--silentconfig")
    endif (CONFIG_MENU)

    set (CONFIG_SRC_FILES  "${CONFIG_SRC_DIR}/config" "${CONFIG_SRC_DIR}/rootfs_config")
    set (DEVICE_TREE_BIN_DIR "${CMAKE_CURRENT_BINARY_DIR}/${PROJ_NAME}/project-spec/meta-user/recipes-bsp/device-tree/files")

    set (CONFIG_ARCHIVE_DIR "${CMAKE_CURRENT_BINARY_DIR}/${PROJ_NAME}-configs")
    set (README_FILE "${CONFIG_ARCHIVE_DIR}/ReadMe.txt")
    file (WRITE  ${README_FILE} "Directory created by CMake for config files (config and rootfs_config):\n")
    file (APPEND ${README_FILE} " - config.default, rootfs_config.default: original versions from petalinux-create\n")
    file (APPEND ${README_FILE} " - config.hw: config file after specifying the hardware description\n")
    file (APPEND ${README_FILE} " - config.<appname>: config file after creating app <appname>\n")
    file (APPEND ${README_FILE} " - config.cfg: config file after configuring the kernel (also used as CMake build output)\n")
    file (APPEND ${README_FILE} " - rootfs_config.cfg: config file after configuring rootfs (also used as CMake build output)\n")

    set (CONFIG_BIN_DIR   "${CMAKE_CURRENT_BINARY_DIR}/${PROJ_NAME}/project-spec/configs")
    set (CONFIG_BIN_FILE  "${CONFIG_BIN_DIR}/config")

    # Create the project. This does not take very long.
    set (PETALINUX_CREATE_OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/${PROJ_NAME}/config.project")
    add_custom_command (
        OUTPUT ${PETALINUX_CREATE_OUTPUT}
        COMMAND petalinux-create -t project --force --template zynq -n ${PROJ_NAME}
        # Archive default versions of config and rootfs_config
        COMMAND ${CMAKE_COMMAND}
                ARGS -E copy_if_different
                "${CONFIG_BIN_DIR}/config"
                "${CONFIG_ARCHIVE_DIR}/config.default"
        COMMAND ${CMAKE_COMMAND}
                ARGS -E copy_if_different
                "${CONFIG_BIN_DIR}/rootfs_config"
                "${CONFIG_ARCHIVE_DIR}/rootfs_config.default"
        COMMENT "Creating Petalinux project ${PROJ_NAME}")

    # Outputs of petalinux-config (hardware and kernel)
    set (PETALINUX_CONFIG_HW_OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/${PROJ_NAME}/project-spec/hw-description/system.xsa")

    add_custom_command (
        OUTPUT ${PETALINUX_CONFIG_HW_OUTPUT}
        # Copy the config files if needed
        COMMAND ${CMAKE_COMMAND}
                ARGS -E copy_if_different
                ${CONFIG_SRC_FILES}
                ${CONFIG_BIN_DIR}
        # Specify the hardware description (XSA) file. This does not take very long and has one config menu
        # (shown if CONFIG_MENU is ON).
        COMMAND petalinux-config -p ${PROJ_NAME} --get-hw-description ${HW_FILE} ${CONFIG_OPTION}
        # Archive config file
        COMMAND ${CMAKE_COMMAND}
                ARGS -E copy_if_different
                ${CONFIG_BIN_FILE}
                "${CONFIG_ARCHIVE_DIR}/config.hw"
        COMMENT "Configuring hardware from ${HW_FILE}"
        DEPENDS ${HW_FILE} ${CONFIG_SRC_FILES} ${PETALINUX_CREATE_OUTPUT})

    set (PETALINUX_CONFIG_OUTPUT "${CONFIG_ARCHIVE_DIR}/config.cfg")

    add_custom_command (
        OUTPUT ${PETALINUX_CONFIG_OUTPUT}
        # Copy the config file if needed
        COMMAND ${CMAKE_COMMAND}
                ARGS -E copy_if_different
                ${CONFIG_SRC_FILES}
                ${CONFIG_BIN_DIR}
        # Copy the device-tree file if needed
        COMMAND ${CMAKE_COMMAND}
                ARGS -E copy_if_different
                ${DEVICE_TREE_FILES}
                ${DEVICE_TREE_BIN_DIR}
        # Configure the kernel. This takes a long time and has one config menu
        # (shown if CONFIG_MENU is ON).
        COMMAND petalinux-config -p ${PROJ_NAME} -c kernel ${CONFIG_OPTION}
        # Archive the config file (this is also the command output)
        COMMAND ${CMAKE_COMMAND}
                ARGS -E copy
                ${CONFIG_BIN_FILE}
                ${PETALINUX_CONFIG_OUTPUT}
        COMMENT "Configuring Petalinux kernel"
        DEPENDS ${PETALINUX_CONFIG_HW_OUTPUT} ${CONFIG_SRC_FILES} ${DEVICE_TREE_FILES})

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
       APP_SOURCES
       APP_BBAPPEND)

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
      # which is copied and renamed to system.bit. This feature ("fpgamanager" template) is not currently used.
      set (APP_BIN           "${CMAKE_CURRENT_BINARY_DIR}/${PROJ_NAME}/project-spec/meta-user/recipes-firmware/${APP_NAME}")
      set (APP_FILES_BIN     "${APP_BIN}/files/system.bit")
    else (APP_TEMPLATE STREQUAL "fpgamanager")
      # For all other apps, files are in recipes-apps. In this case APP_SOURCES can specify more than one file, though further
      # updates may be necessary to support this (e.g., via .bbappend)
      set (APP_BIN           "${CMAKE_CURRENT_BINARY_DIR}/${PROJ_NAME}/project-spec/meta-user/recipes-apps/${APP_NAME}")
      set (APP_FILES_BIN     "${APP_BIN}/files")
    endif (APP_TEMPLATE STREQUAL "fpgamanager")
    set (APP_CREATE_OUTPUT "${APP_BIN}/${APP_NAME}.bb")

    set (CONFIG_ARCHIVE_DIR "${CMAKE_CURRENT_BINARY_DIR}/${PROJ_NAME}-configs")
    set (ROOTFS_CONFIG      "${CMAKE_CURRENT_BINARY_DIR}/${PROJ_NAME}/project-spec/configs/rootfs_config")

    add_custom_command (
        OUTPUT ${APP_CREATE_OUTPUT}
        COMMAND petalinux-create -p ${PROJ_NAME} -t apps --template ${APP_TEMPLATE} --name ${APP_NAME} --enable --force
        # Enabling the app modifies configs/rootfs_config, so archive it
        COMMAND ${CMAKE_COMMAND}
                ARGS -E copy_if_different
                ${ROOTFS_CONFIG}
                "${CONFIG_ARCHIVE_DIR}/rootfs_config.${APP_NAME}"
        # Copy the source files
        COMMAND ${CMAKE_COMMAND}
                ARGS -E copy_if_different
                ${APP_SOURCES}
                ${APP_FILES_BIN}
        COMMENT "Creating ${APP_NAME}"
        DEPENDS ${APP_SOURCES})

    if (APP_BBAPPEND)

      set (APP_BBAPPEND_OUTPUT "${APP_BIN}/${APP_BBAPPEND}")
      add_custom_command (
          OUTPUT ${APP_BBAPPEND_OUTPUT}
          # Copy the source files
          COMMAND ${CMAKE_COMMAND}
                  ARGS -E copy_if_different
                  ${APP_BBAPPEND}
                  ${APP_BIN}
          COMMENT "Checking ${APP_BBAPPEND}"
          DEPENDS ${APP_CREATE_OUTPUT} ${APP_BBAPPEND})

      add_custom_target(${APP_NAME} ALL
                        DEPENDS ${APP_BBAPPEND_OUTPUT})

    else (APP_BBAPPEND)

      add_custom_target(${APP_NAME} ALL
                        DEPENDS ${APP_CREATE_OUTPUT})

    endif (APP_BBAPPEND)

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
       CONFIG_MENU
       CONFIG_SRC_DIR
       BIT_FILE
       DEPENDENCIES)

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

  if (TARGET_NAME AND PROJ_NAME AND CONFIG_SRC_DIR)

    if (CONFIG_MENU)
      set (CONFIG_OPTION "")
    else (CONFIG_MENU)
      set (CONFIG_OPTION "--silentconfig")
    endif (CONFIG_MENU)

    set (CONFIG_SRC_FILES "${CONFIG_SRC_DIR}/config" "${CONFIG_SRC_DIR}/rootfs_config")
    set (CONFIG_BIN_DIR   "${CMAKE_CURRENT_BINARY_DIR}/${PROJ_NAME}/project-spec/configs")
    set (CONFIG_BIN_FILE  "${CONFIG_BIN_DIR}/rootfs_config")
    set (CONFIG_ARCHIVE_DIR "${CMAKE_CURRENT_BINARY_DIR}/${PROJ_NAME}-configs")

    set (PETALINUX_IMAGE_DIR  "${CMAKE_CURRENT_BINARY_DIR}/${PROJ_NAME}/images/linux")

    # Output of petalinux-configure
    set (PETALINUX_CONFIG_OUTPUT "${CONFIG_ARCHIVE_DIR}/rootfs_config.cfg")

    # Outputs of petalinux-build
    set (PETALINUX_IMAGE_UB   "${PETALINUX_IMAGE_DIR}/image.ub")
    set (PETALINUX_FSBL_FILE  "${PETALINUX_IMAGE_DIR}/zynq_fsbl.elf")
    set (PETALINUX_UBOOT_FILE "${PETALINUX_IMAGE_DIR}/u-boot.elf")

    # Output of petalinux-package
    set (PETALINUX_BOOT_FILE  "${PETALINUX_IMAGE_DIR}/BOOT.bin")

    # First, configure rootfs. This is done here, rather than in petalinux_create, because
    # petalinux_app_create also modifies rootfs.

    add_custom_command (
        OUTPUT ${PETALINUX_CONFIG_OUTPUT}
        # Copy the config files if needed
        COMMAND ${CMAKE_COMMAND}
                ARGS -E copy_if_different
                ${CONFIG_SRC_FILES}
                ${CONFIG_BIN_DIR}
        # Configure the rootfs. This does not take a long time and has one config menu
        # (shown if CONFIG_MENU is ON).
        COMMAND petalinux-config -p ${PROJ_NAME} -c rootfs ${CONFIG_OPTION}
        # Save in the archive (this is also the command output)
        COMMAND ${CMAKE_COMMAND}
                ARGS -E copy
                ${CONFIG_BIN_FILE}
                ${PETALINUX_CONFIG_OUTPUT}
        COMMENT "Configuring Petalinux rootfs"
        DEPENDS ${PROJ_NAME} ${CONFIG_SRC_FILES} ${DEPENDENCIES})

    # Next, build petalinux.
    add_custom_command (
        OUTPUT ${PETALINUX_IMAGE_UB} ${PETALINUX_FSBL_FILE} ${PETALINUX_UBOOT_FILE}
        COMMAND petalinux-build -p ${PROJ_NAME}
        COMMENT "Building petalinux"
        DEPENDS ${PETALINUX_CONFIG_OUTPUT})

    # Package the boot files
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

    # Finally, check if config files in build tree differ from source tree
    # (if there is a difference, consider updating source tree).
    set_source_files_properties (compare_output PROPERTIES SYMBOLIC true)
    add_custom_command (
        OUTPUT compare_output
        # CMake provides a portable compare_files command:
        #   ${CMAKE_COMMAND} ARGS -E compare_files <file1> <file2>
        # But, since Petalinux only runs on Linux and since we also want
        # to see which lines are different, we just use the "diff" command.
        # The "|| :" is added so that differences are not flagged as errors.
        COMMAND diff
                "${CONFIG_BIN_DIR}/config"
                "${CONFIG_SRC_DIR}/config"
                || :
        COMMAND diff
                "${CONFIG_BIN_DIR}/rootfs_config"
                "${CONFIG_SRC_DIR}/rootfs_config"
                || :
        COMMENT "Comparing config files in build tree to source tree"
        DEPENDS ${PETALINUX_BOOT_FILE})

    add_custom_target(${TARGET_NAME} ALL
                      DEPENDS compare_output)

  else (TARGET_NAME AND PROJ_NAME AND CONFIG_SRC_DIR)

    message (SEND_ERROR "petalinux_build: required parameter missing")

  endif (TARGET_NAME AND PROJ_NAME AND CONFIG_SRC_DIR)

endfunction (petalinux_build)
