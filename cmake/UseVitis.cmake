# UseVitis
#
# This package assumes that find_package(Vitis) has been called,
# so that the VITIS_XSCT program has been found.
#

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

endfunction (vitis_platform_create)

function (vitis_app_create_from_template ...)

  # set all keywords and their values to ""
  set (FUNCTION_KEYWORDS
       TARGET_NAME
       APP_NAME
       PLATFORM_NAME
       TEMPLATE_NAME)

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

  file(TO_NATIVE_PATH ${VITIS_XSCT} XSCT_NATIVE)

  # Create TCL file
  set (TCL_FILE "${CMAKE_CURRENT_BINARY_DIR}/make-${APP_NAME}-create.tcl")
  file (WRITE  ${TCL_FILE} "setws ${CMAKE_CURRENT_BINARY_DIR}\n")
  # Set active platform
  file (APPEND ${TCL_FILE} "platform active ${PLATFORM_NAME}\n")
  # Create app, using active platform (and domain); also creates sysproj ${APP_NAME}_system
  file (APPEND ${TCL_FILE} "if { [catch {app create -name ${APP_NAME} -template {${TEMPLATE_NAME}}} errMsg ]} {\n")
  file (APPEND ${TCL_FILE} "  puts stderr \"app create: $errMsg\"\n}\n")

  # The PRJ file is one of the generated outputs
  set (APP_PRJ "${CMAKE_CURRENT_BINARY_DIR}/${APP_NAME}/${APP_NAME}.prj")

  add_custom_command (OUTPUT ${APP_PRJ}
                      COMMAND ${XSCT_NATIVE} ${TCL_FILE}
                      DEPENDS ${PLATFORM_NAME})

  add_custom_target(${TARGET_NAME} ALL
                    DEPENDS ${APP_PRJ})

endfunction (vitis_app_create_from_template)

function (vitis_app_build ...)

  # set all keywords and their values to ""
  set (FUNCTION_KEYWORDS
       TARGET_NAME
       APP_NAME
       PLATFORM_NAME
       ADD_SOURCE
       BUILD_CONFIG
       COMPILER_FLAGS
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

  if (APP_NAME AND PLATFORM_NAME)

    # If TARGET_NAME not specified, default is APP_NAME
    if (NOT TARGET_NAME)
      set (TARGET_NAME ${APP_NAME})
    endif (NOT TARGET_NAME)

    # If BUILD_CONFIG not specified, default is Release
    if (NOT BUILD_CONFIG)
      set (BUILD_CONFIG "Release")
    endif (NOT BUILD_CONFIG)

    file(TO_NATIVE_PATH ${VITIS_XSCT} XSCT_NATIVE)

    # Create TCL file
    set (TCL_FILE "${CMAKE_CURRENT_BINARY_DIR}/make-${TARGET_NAME}-build.tcl")
    file (WRITE  ${TCL_FILE} "setws ${CMAKE_CURRENT_BINARY_DIR}\n")
    # Set active platform
    file (APPEND ${TCL_FILE} "platform active ${PLATFORM_NAME}\n")
    # Add source files, if any
    foreach (src ${ADD_SOURCE})
      file (APPEND ${TCL_FILE} "importsources -name ${APP_NAME} -path ${src}\n")
    endforeach (src)
    # Set build configuration (i.e., Release or Debug)
    file (APPEND ${TCL_FILE} "if { [catch {app config -name ${APP_NAME} build-config ${BUILD_CONFIG}} errMsgCfg ]} {\n")
    file (APPEND ${TCL_FILE} "  puts stderr \"app build-config: $errMsgCfg\"\n}\n")
    # Set compiler flags
    foreach (flag ${COMPILER_FLAGS})
      file (APPEND ${TCL_FILE} "if { [catch {app config -name ${APP_NAME} define-compiler-symbols {${flag}}} errMsgFlag ]} {\n")
      file (APPEND ${TCL_FILE} "  puts stderr \"app config: $errMsgFlag\"\n}\n")
    endforeach (flag)
    # Compile app
    file (APPEND ${TCL_FILE} "if { [catch {app build -name ${APP_NAME}} errMsgBuild]} {\n")
    file (APPEND ${TCL_FILE} "  puts stderr \"app build: $errMsgBuild\"\n}\n")

    # This is the output (executable)
    set (APP_EXEC "${CMAKE_CURRENT_BINARY_DIR}/${APP_NAME}/${BUILD_CONFIG}/${APP_NAME}.elf")

    add_custom_command (OUTPUT ${APP_EXEC}
                        COMMAND ${XSCT_NATIVE} ${TCL_FILE}
                        DEPENDS ${DEPENDENCIES} ${ADD_SOURCE})

    add_custom_target(${TARGET_NAME} ALL
                      DEPENDS ${APP_EXEC})

    set_property(TARGET ${TARGET_NAME}
                 APPEND
                 PROPERTY ADDITIONAL_CLEAN_FILES ${TCL_FILE})

  else (APP_NAME AND PLATFORM_NAME)

    message (SEND_ERROR "vitis_app_build: required parameter missing")

  endif (APP_NAME AND PLATFORM_NAME)

endfunction (vitis_app_build)

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

  file(TO_NATIVE_PATH ${VITIS_BOOTGEN} BOOTGEN_NATIVE)

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
