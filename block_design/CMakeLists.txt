
set (FPGAV3_DEVICE  "xc7z020")
set (FPGAV3_PACKAGE "clg400")
set (FPGAV3_SPEED   "-2")
set (FPGAV3_PARTNUM "${FPGAV3_DEVICE}${FPGAV3_PACKAGE}${FPGAV3_SPEED}")

if (BUILD_FPGAV30)

  vivado_block_build(PROJ_NAME      ${BLOCK_DESIGN_NAME}
                     BD_NAME        "FpgaV30"
                     FPGA_PARTNUM   ${FPGAV3_PARTNUM}
                     EXPORTED_TCL   "${CMAKE_CURRENT_SOURCE_DIR}/exported-block-v30.tcl"
                     HW_FILE        ${HW_XSA_FILE})

else (BUILD_FPGAV30)

  vivado_block_build(PROJ_NAME      ${BLOCK_DESIGN_NAME}
                     BD_NAME        "FpgaV31"
                     FPGA_PARTNUM   ${FPGAV3_PARTNUM}
                     EXPORTED_TCL   "${CMAKE_CURRENT_SOURCE_DIR}/exported-block-v31.tcl"
                     HW_FILE        ${HW_XSA_FILE})

endif (BUILD_FPGAV30)
