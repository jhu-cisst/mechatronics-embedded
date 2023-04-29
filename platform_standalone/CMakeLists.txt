
############################ Platform #############################

if (BUILD_FPGAV30)
  set (PLATFORM_STANDALONE "FpgaV30Standalone")
else (BUILD_FPGAV30)
  set (PLATFORM_STANDALONE "FpgaV31Standalone")
endif (BUILD_FPGAV30)

# Libraries to include (xilffs required for fsbl, lwip211 for echo)
set (BSP_LIBRARIES "xilffs" "xilrsa" "lwip211")

# Patch to lwIP library needed for RTL8211F
set (LWIP_PATCH_FILES
     "${CMAKE_CURRENT_SOURCE_DIR}/bsp_lib_src/lwip211/xemacpsif_physpeed.c")

vitis_platform_create(
    PLATFORM_NAME     ${PLATFORM_STANDALONE}
    HW_FILE           ${HW_XSA_FILE}
    PROC_NAME         "ps7_cortexa9_0"
    OS_NAME           "standalone"
    LIBRARIES         ${BSP_LIBRARIES}
    LWIP_PATCH_SOURCE ${LWIP_PATCH_FILES}
    DEPENDENCIES      ${BLOCK_DESIGN_NAME})

############################ FSBL App #############################

set (FSBL_NAME   "zynq_fsbl")
set (FSBL_CONFIG "Release")

# Additional source files
set (FSBL_SOURCE
     "${CMAKE_CURRENT_SOURCE_DIR}/fsbl_src/fsbl_hooks.c")

vitis_app_create_from_template(
    TARGET_NAME    "${FSBL_NAME}_create"
    APP_NAME       ${FSBL_NAME}
    PLATFORM_NAME  ${PLATFORM_STANDALONE}
    TEMPLATE_NAME  "Zynq FSBL")

vitis_app_build(
    APP_NAME       ${FSBL_NAME}
    PLATFORM_NAME  ${PLATFORM_STANDALONE}
    ADD_SOURCE     ${FSBL_SOURCE}
    BUILD_CONFIG   ${FSBL_CONFIG}
    COMPILER_FLAGS "FSBL_DEBUG_INFO"
    DEPENDENCIES   "${FSBL_NAME}_create")

set (FSBL_OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/${FSBL_NAME}/${FSBL_CONFIG}/${FSBL_NAME}.elf")

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

vitis_app_create_from_template(
    TARGET_NAME    "${MFG_TEST_NAME}_create"
    APP_NAME       ${MFG_TEST_NAME}
    PLATFORM_NAME  ${PLATFORM_STANDALONE}
    TEMPLATE_NAME  "Zynq FSBL")

vitis_app_build(
    APP_NAME       ${MFG_TEST_NAME}
    PLATFORM_NAME  ${PLATFORM_STANDALONE}
    ADD_SOURCE     ${MFG_TEST_SOURCE}
    BUILD_CONFIG   ${MFG_TEST_CONFIG}
    DEPENDENCIES   "${MFG_TEST_NAME}_create")

set (MFG_TEST_OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/${MFG_TEST_NAME}/${MFG_TEST_CONFIG}/${MFG_TEST_NAME}.elf")

######################## Echo Server App ##########################

set (ECHO_TEST_NAME   "echo_test")
set (ECHO_TEST_CONFIG "Release")

# Additional source files
set (ECHO_TEST_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/echo_test_src/main.c")

vitis_app_create_from_template(
    TARGET_NAME    "${ECHO_TEST_NAME}_create"
    APP_NAME       ${ECHO_TEST_NAME}
    PLATFORM_NAME  ${PLATFORM_STANDALONE}
    TEMPLATE_NAME  "lwIP Echo Server")

vitis_app_build(
    APP_NAME       ${ECHO_TEST_NAME}
    PLATFORM_NAME  ${PLATFORM_STANDALONE}
    ADD_SOURCE     ${ECHO_TEST_SOURCE}
    BUILD_CONFIG   ${ECHO_TEST_CONFIG}
    DEPENDENCIES   "${ECHO_TEST_NAME}_create")

set (ECHO_TEST_OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/${ECHO_TEST_NAME}/${ECHO_TEST_CONFIG}/${ECHO_TEST_NAME}.elf")

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

else (BIT_FILE)

  message (STATUS "BIT file not found, skipping creation of boot image (BOOT.bin)")

endif (BIT_FILE)

set (CLEAN_APP_NAMES ${FSBL_NAME} ${MFG_TEST_NAME} ${ECHO_TEST_NAME})

vitis_clean(
    PLATFORM_NAMES ${PLATFORM_STANDALONE}
    APP_NAMES      ${CLEAN_APP_NAMES})
