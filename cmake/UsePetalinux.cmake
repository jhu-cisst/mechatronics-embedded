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
#   information is specified in DEVICE_TREE_FILES. The CONFIG_SRC_DIR contains
#   two files, config and rootfs_config, that are copied to the project-spec/configs
#   directory in the build tree and one file, bsp.cfg, that is copied to the
#   the project-spec/meta-user/recipes-kernel/linux/linux-xlnx directory in the
#   build tree.
#
#   The bsp.cfg file contains custom kernel configuration entries that are not stored
#   in "config" or "rootfs_config", but rather in generated files named "user_<date>.cfg"
#   in the project-spec/meta-user/recipes-kernel/linux/linux-xlnx directory.
#   It is easier, however, to put them in the existing "bsp.cfg" file, which
#   is empty by default.
#
#   To change the configuration, set CONFIG_MENU ON via CMake. This will cause
#   the system to show the configuration menus and update the files in the
#   project-spec/configs and project-spec/meta-user/recipes-kernel/linux/linux-xlnx
#   directories in the build tree.
#   Note that this function only updates the "config" and "bsp.cfg" files;
#   the "rootfs_config" is updated in petalinux_app_create and petalinux_build.
#
##########################################################################################
#
# Function: petalinux_app_create
#
# Parameters:
#   - APP_NAME           Application name (also used as CMake target name)
#   - PROJ_NAME          Project name
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
#
# Important CMake Information
#
# This implementation relies on CMake add_custom_command and add_custom_target, so it is
# important to understand how dependency checking works when using these commands.
#
# We use add_custom_command to produce an output, which is usually one or more files.
# For dependencies (DEPENDS), we can specify files or targets. If a file is specified,
# the command is executed if any of the outputs are older than the file.
# If a target is specified, the custom command will be considered after the target is built
# (i.e., the target dependency controls the order of the build, but may not cause the command
# to be executed). Whether or not the command is executed depends only on the file dependency
# check. Note: although not relevant here, CMake automatically adds file dependencies for
# targets created by add_executable or add_library, but not for add_custom_target.
#
# CMake add_custom_target creates a target that is always executed. Although it is possible
# to specify one or more commands (COMMAND), that capability is not used in this implementation.
# Instead, add_custom_target is just used to control the order of the build process.
# Typically, the dependencies (DEPENDS) are on the output files created by add_custom_command,
# so that these custom commands are executed if needed (based on the dependencies specified
# in add_custom_command). Starting with CMake 3.16, it also adds a target-level dependency
# for any targets in the same directory that create one of the dependent files.
#
# This implementation creates the following targets:
#
#    ${PROJ_NAME}    Petalinux project name, created by petalinux_create. There is only one such
#                    target and it should be the first one built.
#
#    ${APP_NAME}     Application name, created by petalinux_app_create. There may be multiple app
#                    targets as a result of multiple calls to petalinux_app_create. This has a
#                    target-level dependency on ${PROJ_NAME} via one of the dependent custom commands.
#
#    ${TARGET_NAME}  Final target (typically ${PROJ_NAME}_build), created by petalinux_build.
#                    There is only one such target and it has target-level dependencies on ${PROJ_NAME}
#                    and (via DEPENDENCIES arg to petalinux_build) on any ${APP_NAME} targets.
#                    It should be the last target built.
#
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

  # parse input (could instead use cmake_parse_arguments)
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
    else ()
      set (CONFIG_OPTION "--silentconfig")
    endif ()

    get_filename_component (HW_FILE_NAME ${HW_FILE} NAME)

    set (CONFIG_SRC_FILE     "${CONFIG_SRC_DIR}/config")
    set (KERNEL_CFG_SRC      "${CONFIG_SRC_DIR}/bsp.cfg")
    set (DEVICE_TREE_BIN_DIR "${CMAKE_CURRENT_BINARY_DIR}/${PROJ_NAME}/project-spec/meta-user/recipes-bsp/device-tree/files")

    set (CONFIG_ARCHIVE_DIR "${CMAKE_CURRENT_BINARY_DIR}/${PROJ_NAME}-configs")
    set (README_FILE "${CONFIG_ARCHIVE_DIR}/ReadMe.txt")
    file (WRITE  ${README_FILE} "Directory created by CMake for config files (config and rootfs_config):\n")
    file (APPEND ${README_FILE} " - config.default, rootfs_config.default: original versions from petalinux-create\n")
    file (APPEND ${README_FILE} " - config.hw: config file after specifying the hardware description\n")
    file (APPEND ${README_FILE} " - rootfs_config.<appname>: rootfs_config file after creating app <appname>\n")
    file (APPEND ${README_FILE} " - config.cfg: config file after configuring the kernel (also used as CMake build output)\n")
    file (APPEND ${README_FILE} " - rootfs_config.cfg: config file after configuring rootfs (also used as CMake build output)\n")

    set (CONFIG_BIN_DIR   "${CMAKE_CURRENT_BINARY_DIR}/${PROJ_NAME}/project-spec/configs")
    set (CONFIG_BIN_FILE  "${CONFIG_BIN_DIR}/config")
    set (KERNEL_CFG_DIR   "${CMAKE_CURRENT_BINARY_DIR}/${PROJ_NAME}/project-spec/meta-user/recipes-kernel/linux/linux-xlnx")

    # Output of petalinux-create
    set (PETALINUX_CREATE_OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/${PROJ_NAME}/config.project")
    # Output of petalinux-config (hardware)
    set (PETALINUX_CONFIG_HW_OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/${PROJ_NAME}/project-spec/hw-description/system.xsa")

    add_custom_command (
        OUTPUT ${PETALINUX_CREATE_OUTPUT} ${PETALINUX_CONFIG_HW_OUTPUT}
        # Create the project. This does not take very long.
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
        # Copy the config file from the source tree (rootfs_config is copied in petalinux_build)
        COMMAND ${CMAKE_COMMAND}
                ARGS -E copy_if_different
                ${CONFIG_SRC_FILE}
                ${CONFIG_BIN_FILE}
        # Specify the hardware description (XSA) file. This does not take very long and has one config menu
        # (shown if CONFIG_MENU is ON). This menu can also be shown by typing petalinux-config -p ${PROJ_NAME}
        # on the command line.
        COMMAND petalinux-config -p ${PROJ_NAME} --get-hw-description ${HW_FILE} ${CONFIG_OPTION}
        # Archive config file
        COMMAND ${CMAKE_COMMAND}
                ARGS -E copy_if_different
                "${CONFIG_BIN_DIR}/config"
                "${CONFIG_ARCHIVE_DIR}/config.hw"
        COMMENT "Creating Petalinux project ${PROJ_NAME} and configuring hardware from ${HW_FILE_NAME}"
        # Adding dependency on ${CONFIG_SRC_FILE} forces a complete rebuild if the config file is
        # changed, even though in many cases it would not be necessary (i.e., it is only necessary
        # if one of the hw-description entries is updated).
        DEPENDS ${HW_FILE} ${CONFIG_SRC_FILE})

    # Output from configuring kernel
    set (PETALINUX_CONFIG_OUTPUT "${CONFIG_ARCHIVE_DIR}/config.cfg")

    add_custom_command (
        OUTPUT ${PETALINUX_CONFIG_OUTPUT}
        # Copy the bsp.cfg file
        COMMAND ${CMAKE_COMMAND}
                  ARGS -E copy_if_different
                  ${KERNEL_CFG_SRC}
                  ${KERNEL_CFG_DIR}
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
        DEPENDS ${PETALINUX_CONFIG_HW_OUTPUT} ${KERNEL_CFG_SRC} ${DEVICE_TREE_FILES})

    add_custom_target(${PROJ_NAME} ALL
                      COMMENT "Checking Petalinux creation and hardware/kernel configuration"
                      DEPENDS ${PETALINUX_CONFIG_OUTPUT})

  else ()

    message (SEND_ERROR "petalinux_create: required parameter missing")

  endif ()

endfunction (petalinux_create)

################################ petalinux_app_create ####################################

function (petalinux_app_create ...)

  # set all keywords and their values to ""
  set (FUNCTION_KEYWORDS
       APP_NAME
       PROJ_NAME
       APP_TEMPLATE
       APP_SOURCES
       APP_BBAPPEND)

  # reset local variables
  foreach(keyword ${FUNCTION_KEYWORDS})
    set (${keyword} "")
  endforeach(keyword)
  set (APP_TEMPLATE c)

  # parse input (could instead use cmake_parse_arguments)
  foreach (arg ${ARGV})
    list (FIND FUNCTION_KEYWORDS ${arg} ARGUMENT_IS_A_KEYWORD)
    if (${ARGUMENT_IS_A_KEYWORD} GREATER -1)
      set (CURRENT_PARAMETER ${arg})
      set (${CURRENT_PARAMETER} "")
    else (${ARGUMENT_IS_A_KEYWORD} GREATER -1)
      set (${CURRENT_PARAMETER} ${${CURRENT_PARAMETER}} ${arg})
    endif (${ARGUMENT_IS_A_KEYWORD} GREATER -1)
  endforeach (arg)

  if (PROJ_NAME AND APP_NAME AND APP_SOURCES)

    if (APP_TEMPLATE STREQUAL "fpgamanager")
      # For fpgamanager, files are in recipes-firmware and it is assumed that APP_SOURCES specifies just one file (the BIT file),
      # which is copied and renamed to system.bit. This feature ("fpgamanager" template) is not currently used.
      set (APP_BIN           "${CMAKE_CURRENT_BINARY_DIR}/${PROJ_NAME}/project-spec/meta-user/recipes-firmware/${APP_NAME}")
      set (APP_FILES_BIN     "${APP_BIN}/files/system.bit")
    else ()
      # For all other apps, files are in recipes-apps. In this case APP_SOURCES can specify more than one file, though further
      # updates may be necessary to support this (e.g., via .bbappend)
      set (APP_BIN           "${CMAKE_CURRENT_BINARY_DIR}/${PROJ_NAME}/project-spec/meta-user/recipes-apps/${APP_NAME}")
      set (APP_FILES_BIN     "${APP_BIN}/files")
    endif ()

    set (APP_CREATE_OUTPUT "${APP_BIN}/${APP_NAME}.bb")

    set (CONFIG_ARCHIVE_DIR "${CMAKE_CURRENT_BINARY_DIR}/${PROJ_NAME}-configs")
    set (ROOTFS_CONFIG      "${CMAKE_CURRENT_BINARY_DIR}/${PROJ_NAME}/project-spec/configs/rootfs_config")

    if (APP_BBAPPEND)

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
          # Copy the bbappend source file
          COMMAND ${CMAKE_COMMAND}
                  ARGS -E copy_if_different
                  ${APP_BBAPPEND}
                  ${APP_BIN}
          COMMENT "Creating ${APP_NAME}"
          DEPENDS ${PROJ_NAME} ${APP_SOURCES} ${APP_BBAPPEND})

    else ()

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
          DEPENDS ${PROJ_NAME} ${APP_SOURCES})

    endif ()

    add_custom_target(${APP_NAME} ALL
                      COMMENT "Checking creation of ${APP_NAME} app"
                      DEPENDS ${APP_CREATE_OUTPUT})

  else ()

    message (SEND_ERROR "petalinux_app_create: required parameter missing")

  endif ()

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

  # parse input (could instead use cmake_parse_arguments)
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
    else ()
      set (CONFIG_OPTION "--silentconfig")
    endif ()

    set (CONFIG_SRC_FILE  "${CONFIG_SRC_DIR}/rootfs_config")
    set (CONFIG_BIN_DIR   "${CMAKE_CURRENT_BINARY_DIR}/${PROJ_NAME}/project-spec/configs")
    set (CONFIG_BIN_FILE  "${CONFIG_BIN_DIR}/rootfs_config")
    set (CONFIG_ARCHIVE_DIR "${CMAKE_CURRENT_BINARY_DIR}/${PROJ_NAME}-configs")

    set (PETALINUX_IMAGE_DIR  "${CMAKE_CURRENT_BINARY_DIR}/${PROJ_NAME}/images/linux")

    # First, configure rootfs. This is done here, rather than in petalinux_create, because
    # petalinux_app_create also modifies rootfs_config.

    # Output of petalinux-configure
    set (PETALINUX_CONFIG_OUTPUT "${CONFIG_ARCHIVE_DIR}/rootfs_config.cfg")

    add_custom_command (
        OUTPUT ${PETALINUX_CONFIG_OUTPUT}
        # Copy the rootfs_config file from the source tree.
        COMMAND ${CMAKE_COMMAND}
                ARGS -E copy_if_different
                ${CONFIG_SRC_FILE}
                ${CONFIG_BIN_FILE}
        # Configure the rootfs. This does not take a long time and has one config menu
        # (shown if CONFIG_MENU is ON).
        COMMAND petalinux-config -p ${PROJ_NAME} -c rootfs ${CONFIG_OPTION}
        # Save in the archive (this is also the command output)
        COMMAND ${CMAKE_COMMAND}
                ARGS -E copy
                ${CONFIG_BIN_FILE}
                ${PETALINUX_CONFIG_OUTPUT}
        COMMENT "Copying rootfs_config to build tree and configuring rootfs"
        # May not be necessary to have dependency on ${CONFIG_BIN_FILE}, but this might
        # help since petalinux_app_create also modifies rootfs_config.
        DEPENDS ${PROJ_NAME} ${CONFIG_SRC_FILE} ${CONFIG_BIN_FILE})

    # Next, build petalinux.
    # Outputs of petalinux-build
    set (PETALINUX_IMAGE_UB   "${PETALINUX_IMAGE_DIR}/image.ub")
    set (PETALINUX_FSBL_FILE  "${PETALINUX_IMAGE_DIR}/zynq_fsbl.elf")
    set (PETALINUX_UBOOT_FILE "${PETALINUX_IMAGE_DIR}/u-boot.elf")

    add_custom_command (
        OUTPUT ${PETALINUX_IMAGE_UB} ${PETALINUX_FSBL_FILE} ${PETALINUX_UBOOT_FILE}
        COMMAND petalinux-build -p ${PROJ_NAME}
        COMMENT "Building petalinux"
        DEPENDS ${PETALINUX_CONFIG_OUTPUT} ${DEPENDENCIES})

    # Package the boot files

    # Output of petalinux-package
    set (PETALINUX_BOOT_FILE  "${PETALINUX_IMAGE_DIR}/BOOT.bin")

    if (BIT_FILE)

      add_custom_command (
          OUTPUT ${PETALINUX_BOOT_FILE}
          COMMAND petalinux-package -p ${PROJ_NAME} --boot --fsbl ${PETALINUX_FSBL_FILE} --fpga ${BIT_FILE}
                                    --u-boot ${PETALINUX_UBOOT_FILE} --force -o ${PETALINUX_BOOT_FILE}
          COMMENT "Petalinux package (boot image)"
          DEPENDS ${BIT_FILE} ${PETALINUX_IMAGE_UB} ${PETALINUX_FSBL_FILE} ${PETALINUX_UBOOT_FILE})

    else ()

      add_custom_command (
          OUTPUT ${PETALINUX_BOOT_FILE}
          COMMAND petalinux-package -p ${PROJ_NAME} --boot --fsbl ${PETALINUX_FSBL_FILE}
                                    --u-boot ${PETALINUX_UBOOT_FILE} --force -o ${PETALINUX_BOOT_FILE}
          COMMENT "Petalinux package (boot image) without BIT file"
                "${CMAKE_CURRENT_BINARY_DIR}/${PROJ_NAME}/project-spec/configs"
          DEPENDS ${PETALINUX_IMAGE_UB} ${PETALINUX_FSBL_FILE} ${PETALINUX_UBOOT_FILE})

    endif ()

    add_custom_target(${TARGET_NAME} ALL
                      COMMENT "Checking Petalinux rootfs configuration and build"
                      DEPENDS ${PETALINUX_BOOT_FILE})

    # Finally, check if config files in build tree differ from source tree
    # (if there is a difference, consider updating source tree).
    add_custom_command (
        TARGET ${TARGET_NAME} POST_BUILD
        # CMake provides a portable compare_files command:
        #   ${CMAKE_COMMAND} ARGS -E compare_files <file1> <file2>
        # But, since Petalinux only runs on Linux and since we also want
        # to see which lines are different, we just use the "diff" command.
        # The "|| :" is added so that differences are not flagged as errors.
        COMMAND ${CMAKE_COMMAND} -E echo "Checking config"
        COMMAND diff
                "${CONFIG_BIN_DIR}/config"
                "${CONFIG_SRC_DIR}/config"
                || :
        COMMAND ${CMAKE_COMMAND} -E echo "Checking rootfs_config"
        COMMAND diff
                "${CONFIG_BIN_DIR}/rootfs_config"
                "${CONFIG_SRC_DIR}/rootfs_config"
                || :
        COMMENT "Comparing config files in build tree to source tree")

  else ()

    message (SEND_ERROR "petalinux_build: required parameter missing")

  endif ()

endfunction (petalinux_build)
