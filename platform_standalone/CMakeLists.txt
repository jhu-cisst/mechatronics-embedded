
############################ Platform #############################

if (BUILD_FPGAV30)
  set (PLATFORM_STANDALONE "FpgaV30Standalone")
else (BUILD_FPGAV30)
  set (PLATFORM_STANDALONE "FpgaV31Standalone")
endif (BUILD_FPGAV30)

# Vitis 2022.x uses lwip211. Vitis 2023.1 uses lwip213.
if (Vitis_VERSION_MAJOR EQUAL 2022)
  set (LWIP_LIB "lwip211")
else ()
  set (LWIP_LIB "lwip213")
endif ()

# Patch to lwIP library needed for RTL8211F
set (LWIP_PATCH_FILES
     "${CMAKE_CURRENT_SOURCE_DIR}/bsp_lib_src/${LWIP_LIB}/xemacpsif_physpeed.c")

# Libraries to include (xilffs required for fsbl, lwip for echo)
set (BSP_LIBRARIES "xilffs" "xilrsa" ${LWIP_LIB})


vitis_platform_create(
    PLATFORM_NAME     ${PLATFORM_STANDALONE}
    HW_FILE           ${HW_XSA_FILE}
    PROC_NAME         "ps7_cortexa9_0"
    OS_NAME           "standalone"
    LIBRARIES         ${BSP_LIBRARIES}
    LWIP_PATCH_SOURCE ${LWIP_PATCH_FILES}
    DEPENDENCIES      ${BLOCK_DESIGN_NAME})

############################ FPGAV3 Lib #############################

set (FPGAV3LIB_NAME   "fpgav3")
set (FPGAV3LIB_TARGET "${FPGAV3LIB_NAME}lib")

# Additional source files
set (FPGAV3LIB_SOURCE
     "${CMAKE_CURRENT_SOURCE_DIR}/fpgav3_lib_src/fpgav3_emio.h"
     "${CMAKE_CURRENT_SOURCE_DIR}/fpgav3_lib_src/fpgav3_emio.c")

vitis_create (LIBRARY
    TARGET_NAME    ${FPGAV3LIB_TARGET}
    LIB_NAME       ${FPGAV3LIB_NAME}
    LIB_TYPE       "static"
    PLATFORM_NAME  ${PLATFORM_STANDALONE}
    ADD_SOURCE     ${FPGAV3LIB_SOURCE}
    BUILD_CONFIG   "Release")

############################ FSBL App #############################

set (FSBL_NAME   "zynq_fsbl")
set (FSBL_CONFIG "Release")

# Additional source files
set (FSBL_SOURCE
     "${CMAKE_CURRENT_SOURCE_DIR}/fsbl_src/fsbl_hooks.c")

vitis_create (APP
    APP_NAME       ${FSBL_NAME}
    PLATFORM_NAME  ${PLATFORM_STANDALONE}
    TEMPLATE_NAME  "Zynq FSBL"
    ADD_SOURCE     ${FSBL_SOURCE}
    TARGET_LIBS    ${FPGAV3LIB_TARGET}
    BUILD_CONFIG   ${FSBL_CONFIG})

get_property(FSBL_OUTPUT TARGET ${FSBL_NAME} PROPERTY OUTPUT_NAME)

########################## MFG_TEST App ############################

set (MFG_TEST_NAME   "mfg_test")
set (MFG_TEST_CONFIG "Release")

# Additional source files
set (MFG_TEST_SOURCE
     "${CMAKE_CURRENT_SOURCE_DIR}/mfg_test_src/fsbl_hooks.c"
     "${CMAKE_CURRENT_SOURCE_DIR}/mfg_test_src/menu.c"
     "${CMAKE_CURRENT_SOURCE_DIR}/mfg_test_src/testDefines.h"
     "${CMAKE_CURRENT_SOURCE_DIR}/mfg_test_src/memtest.c"
     "${CMAKE_CURRENT_SOURCE_DIR}/mfg_test_src/xqspips_selftest_example.c")

vitis_create (APP
    APP_NAME       ${MFG_TEST_NAME}
    PLATFORM_NAME  ${PLATFORM_STANDALONE}
    TEMPLATE_NAME  "Zynq FSBL"
    ADD_SOURCE     ${MFG_TEST_SOURCE}
    TARGET_LIBS    ${FPGAV3LIB_TARGET}
    BUILD_CONFIG   ${MFG_TEST_CONFIG})

get_property(MFG_TEST_OUTPUT TARGET ${MFG_TEST_NAME} PROPERTY OUTPUT_NAME)

######################## Echo Server App ##########################

set (ECHO_TEST_NAME   "echo_test")
set (ECHO_TEST_CONFIG "Release")

# Additional source files
set (ECHO_TEST_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/echo_test_src/main.c")

vitis_create (APP
    APP_NAME       ${ECHO_TEST_NAME}
    PLATFORM_NAME  ${PLATFORM_STANDALONE}
    TEMPLATE_NAME  "lwIP Echo Server"
    ADD_SOURCE     ${ECHO_TEST_SOURCE}
    BUILD_CONFIG   ${ECHO_TEST_CONFIG})

get_property(ECHO_TEST_OUTPUT TARGET ${ECHO_TEST_NAME} PROPERTY OUTPUT_NAME)

############################## Demo App ################################

set (DEMO_APP_NAME   "demo_app")
set (DEMO_APP_CONFIG "Release")

# Additional source files
set (DEMO_APP_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/demo_app_src/main.c")
set (DEMO_APP_DELETE "helloworld.c")

vitis_create (APP
    APP_NAME       ${DEMO_APP_NAME}
    PLATFORM_NAME  ${PLATFORM_STANDALONE}
    TEMPLATE_NAME  "Hello World"
    ADD_SOURCE     ${DEMO_APP_SOURCE}
    DEL_SOURCE     ${DEMO_APP_DELETE}
    BUILD_CONFIG   ${DEMO_APP_CONFIG})

get_property(DEMO_APP_OUTPUT TARGET ${DEMO_APP_NAME} PROPERTY OUTPUT_NAME)

########################### Boot Images #############################

if (BIT_FILE)

  vitis_boot_create(
      BIF_NAME       "zynq_boot"
      FSBL_FILE      ${FSBL_OUTPUT}
      BIT_FILE       ${BIT_FILE}
      DEPENDENCIES   ${FSBL_NAME})

  vitis_boot_create(
      BIF_NAME       "mfg_test_boot"
      FSBL_FILE      ${MFG_TEST_OUTPUT}
      BIT_FILE       ${BIT_FILE}
      DEPENDENCIES   ${MFG_TEST_NAME})

  set (ECHO_TEST_DEPS ${FSBL_OUTPUT} ${ECHO_TEST_NAME})
  vitis_boot_create(
      BIF_NAME       "echo_test_boot"
      FSBL_FILE      ${FSBL_OUTPUT}
      BIT_FILE       ${BIT_FILE}
      APP_FILE       ${ECHO_TEST_OUTPUT}
      DEPENDENCIES   ${ECHO_TEST_DEPS})

  set (DEMO_APP_DEPS ${FSBL_OUTPUT} ${DEMO_APP_NAME})
  vitis_boot_create(
      BIF_NAME       "demo_app_boot"
      FSBL_FILE      ${FSBL_OUTPUT}
      BIT_FILE       ${BIT_FILE}
      APP_FILE       ${DEMO_APP_OUTPUT}
      DEPENDENCIES   ${DEMO_APP_DEPS})

else (BIT_FILE)

  message (STATUS "BIT file not found, skipping creation of boot image (BOOT.bin)")

endif (BIT_FILE)

set (CLEAN_APP_NAMES ${FSBL_NAME} ${MFG_TEST_NAME} ${ECHO_TEST_NAME})
set (CLEAN_LIBRARY_NAMES ${FSBL_NAME} ${MFG_TEST_NAME} ${ECHO_TEST_NAME})

vitis_clean(
    PLATFORM_NAMES ${PLATFORM_STANDALONE}
    APP_NAMES      ${CLEAN_APP_NAMES}
    LIBRARY_NAMES  ${FPGAV3LIB_NAME})
