##########################################################################################
#
# UseVitis
#
# This package assumes that find_package(Vitis) has been called,
# so that the VITIS_XSCT and VITIS_BOOTGEN programs have been found.
# It contains the following functions:
#
##########################################################################################
#
# Function: vitis_platform_create
#
# Parameters:
#   - PLATFORM_NAME      Platform name (also CMake target name)
#   - HW_FILE            Input hardware file (XSA)
#   - PROC_NAME          Processor name (see below)
#   - OS_NAME            Operating system ("standalone" or "linux")
#   - LIBRARIES          BSP libraries to install (optional)
#   - LWIP_PATCH_SOURCE  Patch to lwip library (optional)
#   - DEPENDENCIES       Additional dependencies (optional)
#
# Description:
#   This function creates the platform and domain, based on the specified HW_FILE.
#   Note that for the "standalone" OS, PROC_NAME should be "ps7_cortexa9_0" (or
#   "ps7_cortexa9_1"), whereas for the "linux" OS, it should be ""ps7_cortexa9".
#   The DEPENDENCIES parameter is optional, but if the CMake USE_VIVADO option is
#   ON, it can be set to the Vivado build target (BLOCK_DESIGN_NAME). This is not
#   required, since the dependency on HW_FILE is enough to ensure that
#   BLOCK_DESIGN_NAME (which creates HW_FILE) is built first.
#
##########################################################################################
#
# Function: vitis_app_create
#
# Parameters:
#   - TARGET_NAME     CMake target name (optional, defaults to APP_NAME)
#   - APP_NAME        Name of application
#   - PLATFORM_NAME   Name of platform (created by vitis_platform_create)
#   - TEMPLATE_NAME   Name of Vitis application template
#   - ADD_SOURCE      Additional source files (optional)
#   - DEL_SOURCE      Source files to delete (optional)
#   - BUILD_CONFIG    Build configuration (optional, default is "Release")
#   - COMPILER_FLAGS  Compiler flags (optional)
#
# Description:
#   This function creates an application from the specified template and then
#   builds it.
#
##########################################################################################
#
# Function: vitis_boot_create
#
# Parameters:
#   - BIF_NAME        Name for BIF file used to create boot image (also CMake target name)
#   - FSBL_FILE       Compiled first stage boot loader (required)
#   - BIT_FILE        Compiled FPGA firmware (required)
#   - APP_FILE        Compiled application (optional)
#   - DEPENDENCIES    Additional dependencies (optional)
#
# Description:
#   This function is used to create the boot image, using the Xilinx bootgen utility.
#   Currently, the BIT_FILE is required, but this could become optional.
#
##########################################################################################
#
# Function: vitis_clean
#
# Parameters:
#   - PLATFORM_NAMES  Names of platforms to remove
#   - APP_NAMES       Names of apps to remove
#
# Description:
#   This target uses the Xilinx XSCT tool to clean files in the build tree.
#   As currently implemented, the target name is hard-coded (VitisClean), so
#   this function should only be used once in a CMake project.
#
##########################################################################################

function (vitis_platform_create ...)

  # set all keywords and their values to ""
  set (FUNCTION_KEYWORDS
       PLATFORM_NAME
       HW_FILE
       PROC_NAME
       OS_NAME
       LIBRARIES
       LWIP_PATCH_SOURCE
       DEPENDENCIES)

  # reset local variables
  foreach(keyword ${FUNCTION_KEYWORDS})
    set (${keyword} "")
  endforeach(keyword)

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

  if (PLATFORM_NAME AND HW_FILE AND PROC_NAME AND OS_NAME)

    file(TO_NATIVE_PATH ${VITIS_XSCT} XSCT_NATIVE)

    # Path to lwip source files in build tree
    set (BSP_DIR            "${CMAKE_CURRENT_BINARY_DIR}/${PLATFORM_NAME}/${PROC_NAME}/${OS_NAME}_domain/bsp")
    set (LWIP_SRC_BUILD_DIR "${BSP_DIR}/${PROC_NAME}/libsrc/lwip211_v1_8/src/contrib/ports/xilinx/netif")

    # Create TCL file
    set (TCL_FILE "${CMAKE_CURRENT_BINARY_DIR}/make-${PLATFORM_NAME}.tcl")
    file (WRITE  ${TCL_FILE} "setws ${CMAKE_CURRENT_BINARY_DIR}\n")
    # Create the platform and domain
    file (APPEND ${TCL_FILE} "platform create -name {${PLATFORM_NAME}} -hw {${HW_FILE}} -proc {${PROC_NAME}}\\\n")
    file (APPEND ${TCL_FILE} "  -os {${OS_NAME}} -no-boot-bsp -out {${CMAKE_CURRENT_BINARY_DIR}}\n")
    # Create the file platform.spr
    file (APPEND ${TCL_FILE} "platform write\n")
    # Add specified libraries to BSP
    foreach (lib ${LIBRARIES})
      file (APPEND ${TCL_FILE} "bsp setlib -name ${lib}\n")
    endforeach (lib)
    # Regenerate BSP (not sure if this is needed)
    if (LIBRARIES)
      file (APPEND ${TCL_FILE} "bsp regenerate\n")
    endif (LIBRARIES)
    foreach (src ${LWIP_PATCH_SOURCE})
      get_filename_component(fname ${src} NAME)
      file (APPEND ${TCL_FILE} "puts \"Copying file ${fname}\"\n")
      file (APPEND ${TCL_FILE} "file copy -force -- ${src} ${LWIP_SRC_BUILD_DIR}\n")
    endforeach (src)
    # Generate the platform (this compiles the BSP libraries)
    file (APPEND ${TCL_FILE} "puts \"Generating platform ...\"\n")
    file (APPEND ${TCL_FILE} "platform generate\n")

    set (PLATFORM_OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/${PLATFORM_NAME}/platform.spr")

    add_custom_command (OUTPUT ${PLATFORM_OUTPUT}
                        COMMAND ${XSCT_NATIVE} ${TCL_FILE}
                        DEPENDS ${HW_FILE} ${DEPENDENCIES} ${LWIP_PATCH_SOURCE})

    add_custom_target(${PLATFORM_NAME} ALL
                      DEPENDS ${PLATFORM_OUTPUT})

  else ()

    message (SEND_ERROR "vitis_platform_create: required parameter missing")

  endif ()

endfunction (vitis_platform_create)

function (vitis_app_create ...)

  # set all keywords and their values to ""
  set (FUNCTION_KEYWORDS
       TARGET_NAME
       APP_NAME
       PLATFORM_NAME
       TEMPLATE_NAME
       ADD_SOURCE
       DEL_SOURCE
       BUILD_CONFIG
       COMPILER_FLAGS)

  # reset local variables
  foreach(keyword ${FUNCTION_KEYWORDS})
    set (${keyword} "")
  endforeach(keyword)

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

  if (APP_NAME AND PLATFORM_NAME AND TEMPLATE_NAME)

    # If TARGET_NAME not specified, default is APP_NAME
    if (NOT TARGET_NAME)
      set (TARGET_NAME ${APP_NAME})
    endif (NOT TARGET_NAME)

    # If BUILD_CONFIG not specified, default is Release
    if (NOT BUILD_CONFIG)
      set (BUILD_CONFIG "Release")
    endif (NOT BUILD_CONFIG)

    file(TO_NATIVE_PATH ${VITIS_XSCT} XSCT_NATIVE)

    #************** First, create the app ****************

    # Create TCL file
    set (TCL_CREATE "${CMAKE_CURRENT_BINARY_DIR}/make-${APP_NAME}-create.tcl")
    file (WRITE  ${TCL_CREATE} "setws ${CMAKE_CURRENT_BINARY_DIR}\n")
    # Set active platform
    file (APPEND ${TCL_CREATE} "platform active ${PLATFORM_NAME}\n")
    # Create app, using active platform (and domain); also creates sysproj ${APP_NAME}_system
    file (APPEND ${TCL_CREATE} "if { [catch {app create -name ${APP_NAME} -template {${TEMPLATE_NAME}}} errMsg ]} {\n")
    file (APPEND ${TCL_CREATE} "  puts stderr \"app create: $errMsg\"\n}\n")

    # The PRJ file is one of the generated outputs
    set (APP_PRJ "${CMAKE_CURRENT_BINARY_DIR}/${APP_NAME}/${APP_NAME}.prj")

    add_custom_command (OUTPUT ${APP_PRJ}
                        COMMAND ${XSCT_NATIVE} ${TCL_CREATE}
                        COMMENT "Creating app ${APP_NAME}"
                        DEPENDS ${PLATFORM_NAME})

    add_custom_target("${TARGET_NAME}_create" ALL
                      DEPENDS ${APP_PRJ})

    #************** Next, build the app ****************

    # Create TCL file
    set (TCL_BUILD "${CMAKE_CURRENT_BINARY_DIR}/make-${TARGET_NAME}-build.tcl")
    file (WRITE  ${TCL_BUILD} "setws ${CMAKE_CURRENT_BINARY_DIR}\n")
    # Set active platform
    file (APPEND ${TCL_BUILD} "platform active ${PLATFORM_NAME}\n")
    # Delete any specified source file
    foreach (src ${DEL_SOURCE})
      file (APPEND ${TCL_BUILD} "file delete ${CMAKE_CURRENT_BINARY_DIR}/${APP_NAME}/src/${src}\n")
    endforeach (src)
    # Add source files, if any
    foreach (src ${ADD_SOURCE})
      file (APPEND ${TCL_BUILD} "importsources -name ${APP_NAME} -path ${src}\n")
    endforeach (src)
    # Set build configuration (i.e., Release or Debug)
    file (APPEND ${TCL_BUILD} "if { [catch {app config -name ${APP_NAME} build-config ${BUILD_CONFIG}} errMsgCfg ]} {\n")
    file (APPEND ${TCL_BUILD} "  puts stderr \"app build-config: $errMsgCfg\"\n}\n")
    # Set compiler flags
    foreach (flag ${COMPILER_FLAGS})
      file (APPEND ${TCL_BUILD} "if { [catch {app config -name ${APP_NAME} define-compiler-symbols {${flag}}} errMsgFlag ]} {\n")
      file (APPEND ${TCL_BUILD} "  puts stderr \"app config: $errMsgFlag\"\n}\n")
    endforeach (flag)
    # Compile app
    file (APPEND ${TCL_BUILD} "if { [catch {app build -name ${APP_NAME}} errMsgBuild]} {\n")
    file (APPEND ${TCL_BUILD} "  puts stderr \"app build: $errMsgBuild\"\n}\n")

    # This is the output (executable)
    set (APP_EXEC "${CMAKE_CURRENT_BINARY_DIR}/${APP_NAME}/${BUILD_CONFIG}/${APP_NAME}.elf")

    add_custom_command (OUTPUT ${APP_EXEC}
                        COMMAND ${XSCT_NATIVE} ${TCL_BUILD}
                        COMMENT "Building app ${APP_NAME}"
                        DEPENDS "${TARGET_NAME}_create" ${ADD_SOURCE})

    add_custom_target(${TARGET_NAME} ALL
                      DEPENDS ${APP_EXEC})

    set_property(TARGET ${TARGET_NAME}
                 APPEND
                 PROPERTY ADDITIONAL_CLEAN_FILES ${TCL_CREATE} ${TCL_BUILD})

  else ()

    message (SEND_ERROR "vitis_app_create: required parameter missing")

  endif ()

endfunction (vitis_app_create)

function (vitis_boot_create ...)

  # set all keywords and their values to ""
  set (FUNCTION_KEYWORDS
       BIF_NAME
       FSBL_FILE
       BIT_FILE
       APP_FILE
       DEPENDENCIES)

  # reset local variables
  foreach(keyword ${FUNCTION_KEYWORDS})
    set (${keyword} "")
  endforeach(keyword)

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

  file(TO_NATIVE_PATH ${VITIS_BOOTGEN} BOOTGEN_NATIVE)

  if (BIF_NAME AND FSBL_FILE AND BIT_FILE)

    # Make a directory for the output files
    file (MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/${BIF_NAME}")

    # Create BIF file
    set (BIF_FILE "${CMAKE_CURRENT_BINARY_DIR}/${BIF_NAME}/${BIF_NAME}.bif")
    file (WRITE  ${BIF_FILE} "//arch = zynq; split = false; format = BIN\n")
    file (APPEND ${BIF_FILE} "the_ROM_image:\n{\n")
    file (APPEND ${BIF_FILE} "   [bootloader]${FSBL_FILE}\n")
    file (APPEND ${BIF_FILE} "   ${BIT_FILE}\n")
    if (APP_FILE)
      file (APPEND ${BIF_FILE} "   ${APP_FILE}\n")
    endif (APP_FILE)
    file (APPEND ${BIF_FILE} "}\n")

    # Output file (BOOT.bin)
    set (BOOT_FILE "${CMAKE_CURRENT_BINARY_DIR}/${BIF_NAME}/BOOT.bin")

    add_custom_command (OUTPUT ${BOOT_FILE}
                        COMMAND ${BOOTGEN_NATIVE} -image ${BIF_FILE} -w -o ${BOOT_FILE}
                        DEPENDS ${FSBL_FILE} ${BIT_FILE} ${APP_FILE} ${DEPENDENCIES})

    add_custom_target(${BIF_NAME} ALL
                      DEPENDS ${BOOT_FILE})

    set_property(TARGET ${BIF_NAME}
                 APPEND
                 PROPERTY ADDITIONAL_CLEAN_FILES ${BIF_FILE})

  else ()

    message (SEND_ERROR "vitis_boot_create: required parameter missing")

  endif ()

endfunction (vitis_boot_create ...)

function (vitis_clean ...)

  # set all keywords and their values to ""
  set (FUNCTION_KEYWORDS
       PLATFORM_NAMES
       APP_NAMES)

  # reset local variables
  foreach(keyword ${FUNCTION_KEYWORDS})
    set (${keyword} "")
  endforeach(keyword)

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

  file(TO_NATIVE_PATH ${VITIS_XSCT} XSCT_NATIVE)

  # Create TCL file
  set (TCL_FILE "${CMAKE_CURRENT_BINARY_DIR}/make-clean.tcl")
  file (WRITE  ${TCL_FILE} "setws ${CMAKE_CURRENT_BINARY_DIR}\n")
  foreach (app ${APP_NAMES})
    file (APPEND ${TCL_FILE} "if { [catch {app remove ${app}} errMsgApp ]} {\n")
    file (APPEND ${TCL_FILE} "  puts stderr \"app remove: $errMsgApp\"\n}\n")
    file (APPEND ${TCL_FILE} "if { [catch {sysproj remove ${app}_system} errMsgSys ]} {\n")
    file (APPEND ${TCL_FILE} "  puts stderr \"sysproj remove: $errMsgSys\"\n}\n")
  endforeach (app)
  foreach (platform ${PLATFORM_NAMES})
    file (APPEND ${TCL_FILE} "if { [catch {platform remove ${platform}} errMsgPlat ]} {\n")
    file (APPEND ${TCL_FILE} "  puts stderr \"platform remove: $errMsgPlat\"\n}\n")
  endforeach (platform)

  set_source_files_properties (clean_output PROPERTIES SYMBOLIC true)
  add_custom_command (OUTPUT clean_output
                      COMMAND ${XSCT_NATIVE} ${TCL_FILE})

  add_custom_target(VitisClean ALL
                    DEPENDS clean_output)

  set_target_properties(VitisClean PROPERTIES EXCLUDE_FROM_ALL ON)

  set_property(TARGET VitisClean
               APPEND
               PROPERTY ADDITIONAL_CLEAN_FILES ${TCL_FILE})

endfunction (vitis_clean)
