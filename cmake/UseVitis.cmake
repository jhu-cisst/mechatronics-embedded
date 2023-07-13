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
# Function: vitis_create
#
# Parameters:
#   - OBJECT_TYPE     APP or LIBRARY (required as first parameter)
#   - TARGET_NAME     CMake target name (optional, defaults to APP_NAME or LIB_NAME)
#   - APP_NAME        Name of app (APP only)
#   - LIB_NAME        Name of library (LIBRARY only)
#   - PLATFORM_NAME   Name of platform (created by vitis_platform_create)
#   - TEMPLATE_NAME   Name of Vitis application template (APP only)
#   - LIB_TYPE        Library type (static or shared) (LIBRARY only)
#   - ADD_SOURCE      Additional source files (optional)
#   - DEL_SOURCE      Source files to delete (optional, APP only)
#   - TARGET_LIBS     Library targets (optional, APP only)
#   - BUILD_CONFIG    Build configuration (optional, default is "Release")
#   - COMPILER_FLAGS  Compiler flags (optional), prepend with '/' to remove
#
# Description:
#   This function creates an app or library and then builds it. For an APP,
#   TEMPLATE_NAME must be specified. For a LIBRARY, LIB_TYPE must be specified.
#   Specifying a leading '/' in COMPILER_FLAGS (e.g., "/FSBL_DEBUG_INFO")
#   causes that compiler option to be removed.
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
#   - LIBRARY_NAMES   Names of libraries to remove
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
    set (BSP_DIR "${CMAKE_CURRENT_BINARY_DIR}/${PLATFORM_NAME}/${PROC_NAME}/${OS_NAME}_domain/bsp")
    if (Vitis_VERSION_MAJOR EQUAL 2022)
      set (LWIP_SRC_BUILD_DIR "${BSP_DIR}/${PROC_NAME}/libsrc/lwip211_v1_8/src/contrib/ports/xilinx/netif")
    else ()
      set (LWIP_SRC_BUILD_DIR "${BSP_DIR}/${PROC_NAME}/libsrc/lwip213_v1_0/src/contrib/ports/xilinx/netif")
    endif ()

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
    if (LWIP_PATCH_SOURCE)
      foreach (src ${LWIP_PATCH_SOURCE})
        get_filename_component(fname ${src} NAME)
        file (APPEND ${TCL_FILE} "puts \"Copying file ${fname}\"\n")
        file (APPEND ${TCL_FILE} "file copy -force -- ${src} ${LWIP_SRC_BUILD_DIR}\n")
      endforeach (src)
    endif ()
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

function (vitis_create OBJECT_TYPE ...)

  # set all keywords and their values to ""
  set (FUNCTION_KEYWORDS
       TARGET_NAME
       APP_NAME
       LIB_NAME
       PLATFORM_NAME
       TEMPLATE_NAME
       LIB_TYPE
       ADD_SOURCE
       DEL_SOURCE
       TARGET_LIBS
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

  if (OBJECT_TYPE STREQUAL APP)

    set (OBJECT_NAME ${APP_NAME})
    set (REQUIRED_KEYWORD TEMPLATE_NAME)

    if (LIB_TYPE)
      message (WARNING "vitis_create APP does not expect LIB_TYPE")
    endif ()

    set (XSCT_CMD "app")

  elseif (OBJECT_TYPE STREQUAL LIBRARY)

    set (OBJECT_NAME ${LIB_NAME})
    set (REQUIRED_KEYWORD LIB_TYPE)

    if (TEMPLATE_NAME)
      message (WARNING "vitis_create LIBRARY does not expect TEMPLATE_NAME")
    endif ()

    if (DEL_SOURCE)
      message (WARNING "vitis_create LIBRARY does not expect DEL_SOURCE")
    endif ()

    if (TARGET_LIBS)
      message (WARNING "vitis_create LIBRARY does not expect TARGET_LIBS")
    endif ()

    set (XSCT_CMD "library")

  else ()

    message (SEND_ERROR "vitis_create: must specify APP or LIBRARY")

  endif ()

  if (OBJECT_NAME AND PLATFORM_NAME AND ${REQUIRED_KEYWORD})

    # If TARGET_NAME not specified, default is OBJECT_NAME
    if (NOT TARGET_NAME)
      set (TARGET_NAME ${OBJECT_NAME})
    endif (NOT TARGET_NAME)

    # If BUILD_CONFIG not specified, default is Release
    if (NOT BUILD_CONFIG)
      set (BUILD_CONFIG "release")
    endif (NOT BUILD_CONFIG)

    file(TO_NATIVE_PATH ${VITIS_XSCT} XSCT_NATIVE)

    #************** First, create the app or library ****************

    # The PRJ file is one of the generated outputs.
    #
    # We make a copy of it, OJBECT_PRJ_COPY, and use that as the custom
    # command output because "make clean" will remove the specified
    # output file. Vitis does not allow to create an app that already
    # exists, but if the PRJ file is missing, Vitis is not able to
    # remove the app or library.
    #
    # The other advantage to using OBJECT_PRJ_COPY is that the file time
    # of OBJECT_PRJ is updated when building the app or library, so we
    # cannot have the build custom command depend on OBJECT_PRJ. Previously,
    # the work-around was to introduce an intermediate custom target,
    # ${TARGET_NAME}_create, that depended on ${OBJECT_PRJ} and then have
    # the build custom command depend on ${TARGET_NAME}_create. Now, we
    # can instead have the build custom command depend on OBJECT_PRJ_COPY.

    set (OBJECT_PRJ      "${CMAKE_CURRENT_BINARY_DIR}/${OBJECT_NAME}/${OBJECT_NAME}.prj")
    set (OBJECT_PRJ_COPY "${CMAKE_CURRENT_BINARY_DIR}/${OBJECT_NAME}/cmake.copy")

    # Create TCL file
    set (TCL_CREATE "${CMAKE_CURRENT_BINARY_DIR}/make-${OBJECT_NAME}-create.tcl")
    file (WRITE  ${TCL_CREATE} "setws ${CMAKE_CURRENT_BINARY_DIR}\n")
    # Set active platform
    file (APPEND ${TCL_CREATE} "platform active ${PLATFORM_NAME}\n")
    # First, remove the app or library if it exists (i.e., if OBJECT_PRJ exists).
    # We could instead copy OBJECT_PRJ to OBJECT_PRJ_COPY, but that behavior would
    # not be consistent with "make clean".
    file (APPEND ${TCL_CREATE} "if { [file exists ${OBJECT_PRJ}] } {\n")
    file (APPEND ${TCL_CREATE} "  if { [catch {${XSCT_CMD} remove ${OBJECT_NAME}} errMsg ]} {\n")
    file (APPEND ${TCL_CREATE} "    puts stderr \"${XSCT_CMD} remove: $errMsg\"\n  }\n}\n")
    # Create app or library, using active platform (and domain); also creates sysproj ${OBJECT_NAME}_system
    if (XSCT_CMD STREQUAL "app")
      file (APPEND ${TCL_CREATE} "app create -name ${OBJECT_NAME} -template {${TEMPLATE_NAME}}\n")
    else ()
      file (APPEND ${TCL_CREATE} "library create -name ${OBJECT_NAME} -type ${LIB_TYPE}\n")
    endif ()
    # Set build configuration (i.e., Release or Debug)
    file (APPEND ${TCL_CREATE} "if { [catch {${XSCT_CMD} config -name ${OBJECT_NAME} build-config ${BUILD_CONFIG}} errMsgCfg ]} {\n")
    file (APPEND ${TCL_CREATE} "  puts stderr \"${XSCT_CMD} build-config: $errMsgCfg\"\n}\n")
    if (XSCT_CMD STREQUAL "library")
      # For some reason, library does not already have path to BSP include dir
      set (BSP_INCLUDE_DIR "export/FpgaV31Standalone/sw/FpgaV31Standalone/standalone_domain/bspinclude/include")
      set (BSP_INCLUDE_DIR "${CMAKE_CURRENT_BINARY_DIR}/${PLATFORM_NAME}/${BSP_INCLUDE_DIR}")
      file (APPEND ${TCL_CREATE} "if { [catch {library config -name ${OBJECT_NAME} -add include-path ${BSP_INCLUDE_DIR}} errMsgFlag ]} {\n")
      file (APPEND ${TCL_CREATE} "  puts stderr \"library config include-path: $errMsgFlag\"\n}\n")
    endif ()
    # Set compiler flags
    foreach (flag ${COMPILER_FLAGS})
      string (SUBSTRING ${flag} 0 1 flag0)
      if (flag0 STREQUAL "/")
        set (REMOVE_FLAG "-remove")
        string (SUBSTRING ${flag} 1 -1 flag)
      else ()
        set (REMOVE_FLAG "")
      endif ()
      file (APPEND ${TCL_CREATE} "if { [catch {${XSCT_CMD} config -name ${OBJECT_NAME} ${REMOVE_FLAG} define-compiler-symbols {${flag}}} errMsgFlag ]} {\n")
      file (APPEND ${TCL_CREATE} "  puts stderr \"${XSCT_CMD} config: $errMsgFlag\"\n}\n")
    endforeach (flag)
    # Set header and linker information for TARGET_LIBRARIES (if any)
    foreach (lib ${TARGET_LIBS})
      # Add library include directory
      get_property(LIB_HEADER_DIR TARGET ${lib} PROPERTY INTERFACE_INCLUDE_DIRECTORIES)
      file (APPEND ${TCL_CREATE} "${XSCT_CMD} config -name ${OBJECT_NAME} -add include-path ${LIB_HEADER_DIR}\n")
      # Add library archive directory
      get_property(LIB_ARCHIVE_DIR TARGET ${lib} PROPERTY ARCHIVE_OUTPUT_DIRECTORY)
      file (APPEND ${TCL_CREATE} "${XSCT_CMD} config -name ${OBJECT_NAME} -add library-search-path ${LIB_ARCHIVE_DIR}\n")
      # Add library name
      get_property(LIB_ARCHIVE_NAME TARGET ${lib} PROPERTY ARCHIVE_OUTPUT_NAME)
      file (APPEND ${TCL_CREATE} "${XSCT_CMD} config -name ${OBJECT_NAME} -add libraries ${LIB_ARCHIVE_NAME}\n")
    endforeach (lib)
    # If app or library creation was successful, copy OBJECT_PRJ to OBJECT_PRJ_COPY
    file (APPEND ${TCL_CREATE} "file copy -force -- ${OBJECT_PRJ} ${OBJECT_PRJ_COPY}\n")

    add_custom_command (OUTPUT ${OBJECT_PRJ_COPY}
                        COMMAND ${XSCT_NATIVE} ${TCL_CREATE}
                        COMMENT "Creating ${XSCT_CMD} ${OBJECT_NAME}"
                        DEPENDS ${PLATFORM_NAME})

    #************** Next, build the app or library ****************

    # Create TCL file
    set (TCL_BUILD "${CMAKE_CURRENT_BINARY_DIR}/make-${TARGET_NAME}-build.tcl")
    file (WRITE  ${TCL_BUILD} "setws ${CMAKE_CURRENT_BINARY_DIR}\n")
    # Set active platform
    file (APPEND ${TCL_BUILD} "platform active ${PLATFORM_NAME}\n")
    # Delete any specified source file
    foreach (src ${DEL_SOURCE})
      file (APPEND ${TCL_BUILD} "file delete ${CMAKE_CURRENT_BINARY_DIR}/${OBJECT_NAME}/src/${src}\n")
    endforeach (src)
    # Add source files, if any
    foreach (src ${ADD_SOURCE})
      file (APPEND ${TCL_BUILD} "importsources -name ${OBJECT_NAME} -path ${src}\n")
    endforeach (src)
    # Compile app or library
    file (APPEND ${TCL_BUILD} "${XSCT_CMD} build -name ${OBJECT_NAME}\n")

    # This is the output (executable or library)
    if (XSCT_CMD STREQUAL "app")
      set (OBJECT_OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/${OBJECT_NAME}/${BUILD_CONFIG}/${OBJECT_NAME}.elf")
    else ()  # "library"
      if (LIB_TYPE STREQUAL "static")
        set (OBJECT_OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/${LIB_NAME}/${BUILD_CONFIG}/lib${OBJECT_NAME}.a")
      else ()
        set (OBJECT_OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/${LIB_NAME}/${BUILD_CONFIG}/lib${OBJECT_NAME}.so")
      endif ()
    endif ()

    add_custom_command (OUTPUT ${OBJECT_OUTPUT}
                        COMMAND ${XSCT_NATIVE} ${TCL_BUILD}
                        COMMENT "Building ${XSCT_CMD} ${OBJECT_NAME}"
                        DEPENDS ${OBJECT_PRJ_COPY} ${ADD_SOURCE} ${TARGET_LIBS})

    add_custom_target(${TARGET_NAME} ALL
                      DEPENDS ${OBJECT_OUTPUT} ${TARGET_LIBS})

    if (XSCT_CMD STREQUAL "app")
      set_property(TARGET ${TARGET_NAME}
                 PROPERTY OUTPUT_NAME ${OBJECT_OUTPUT})
    else ()  # "library"
      set_property(TARGET ${TARGET_NAME}
                          PROPERTY INTERFACE_INCLUDE_DIRECTORIES "${CMAKE_CURRENT_BINARY_DIR}/${OBJECT_NAME}/src")
      # Alternatively, could set LIBRARY_OUTPUT_NAME and LIBRARY_OUTPUT_DIRECTORY
      set_property(TARGET ${TARGET_NAME}
                          PROPERTY ARCHIVE_OUTPUT_NAME ${LIB_NAME})
      set_property(TARGET ${TARGET_NAME}
                          PROPERTY ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/${OBJECT_NAME}/${BUILD_CONFIG}")
    endif ()

  else ()

    message (SEND_ERROR "vitis_create: required parameter missing")

  endif ()

endfunction (vitis_create)

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

  else ()

    message (SEND_ERROR "vitis_boot_create: required parameter missing")

  endif ()

endfunction (vitis_boot_create ...)

function (vitis_clean ...)

  # set all keywords and their values to ""
  set (FUNCTION_KEYWORDS
       PLATFORM_NAMES
       APP_NAMES
       LIBRARY_NAMES)

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
  foreach (lib ${LIBRARY_NAMES})
    file (APPEND ${TCL_FILE} "if { [catch {library remove ${lib}} errMsgLib ]} {\n")
    file (APPEND ${TCL_FILE} "  puts stderr \"library remove: $errMsgLib\"\n}\n")
    file (APPEND ${TCL_FILE} "if { [catch {sysproj remove ${lib}_system} errMsgSys ]} {\n")
    file (APPEND ${TCL_FILE} "  puts stderr \"sysproj remove: $errMsgSys\"\n}\n")
  endforeach (lib)
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

endfunction (vitis_clean)
