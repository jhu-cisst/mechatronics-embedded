
############################ Platform #############################

if (BUILD_FPGAV30)
  set (PLATFORM_LINUX      "FpgaV30Linux")
else (BUILD_FPGAV30)
  set (PLATFORM_LINUX      "FpgaV31Linux")
endif (BUILD_FPGAV30)

vitis_platform_create(
    PLATFORM_NAME     ${PLATFORM_LINUX}
    HW_FILE           ${HW_XSA_FILE}
    PROC_NAME         "ps7_cortexa9"
    OS_NAME           "linux"
    DEPENDENCIES      ${BLOCK_DESIGN_NAME})

######################## fpgav3init App ##########################

set (FPGAV3INIT_NAME   "fpgav3init")
set (FPGAV3INIT_CONFIG "Release")

# Set a distinct target name to avoid conflict with petalinux build
set (FPGAV3INIT_TARGET "fpgav3initonly")

# Additional source files
set (FPGAV3INIT_SOURCE "${CMAKE_SOURCE_DIR}/petalinux/fpgav3init/files/fpgav3init.c")

vitis_app_create(
    TARGET_NAME    ${FPGAV3INIT_TARGET}
    APP_NAME       ${FPGAV3INIT_NAME}
    PLATFORM_NAME  ${PLATFORM_LINUX}
    TEMPLATE_NAME  "Linux Empty Application"
    ADD_SOURCE     ${FPGAV3INIT_SOURCE}
    BUILD_CONFIG   ${FPGAV3INIT_CONFIG})

get_property(FPGAV3INIT_OUTPUT TARGET ${FPGAV3INIT_TARGET} PROPERTY OUTPUT_NAME)

# Exclude from build for now (gpiod not yet supported)
set_target_properties(${FPGAV3INIT_TARGET} PROPERTIES EXCLUDE_FROM_ALL ON)
