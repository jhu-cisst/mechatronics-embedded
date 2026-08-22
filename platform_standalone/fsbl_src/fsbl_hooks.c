/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-    */
/* ex: set filetype=cpp softtabstop=4 shiftwidth=4 tabstop=4 cindent expandtab: */

/******************************************************************************
* Copyright (c) 2012 - 2020 Xilinx, Inc.  All rights reserved.
* SPDX-License-Identifier: MIT
******************************************************************************/


/*****************************************************************************
*
* @file fsbl_hooks.c
*
* This file provides functions that serve as user hooks.  The user can add the
* additional functionality required into these routines.  This would help retain
* the normal FSBL flow unchanged.
*
* <pre>
* MODIFICATION HISTORY:
*
* Ver   Who  Date        Changes
* ----- ---- -------- -------------------------------------------------------
* 3.00a np   08/03/12 Initial release
* </pre>
*
* @note
*
******************************************************************************/

#include <stdbool.h>
#include "fsbl.h"
#include "xstatus.h"
#include "xil_printf.h"
#include "xil_io.h"
#include "fsbl_hooks.h"
#include "qspi.h"
#include "fpgav3_emio.h"
#include "fpgav3_qspi.h"
#include "fpgav3_version.h"

/************************** Function Prototypes ******************************/

bool FpgaV3_Init_SD();
bool FpgaV3_Init_QSPI();

/******************************************************************************
* This function is the hook which will be called  before the bitstream download.
* The user can add all the customized code required to be executed before the
* bitstream download to this routine.
*
* @param None
*
* @return
*       - XST_SUCCESS to indicate success
*       - XST_FAILURE.to indicate failure
*
****************************************************************************/
u32 FsblHookBeforeBitstreamDload(void)
{
    u32 Status;

    Status = XST_SUCCESS;

    /*
     * User logic to be added here. Errors to be stored in the status variable
     * and returned
     */
    fsbl_printf(DEBUG_INFO,"In FsblHookBeforeBitstreamDload function \r\n");

    return (Status);
}

/******************************************************************************
* This function is the hook which will be called  after the bitstream download.
* The user can add all the customized code required to be executed after the
* bitstream download to this routine.
*
* @param None
*
* @return
*       - XST_SUCCESS to indicate success
*       - XST_FAILURE.to indicate failure
*
****************************************************************************/
u32 FsblHookAfterBitstreamDload(void)
{
    u32 Status;

    Status = XST_SUCCESS;

    /*
     * User logic to be added here.
     * Errors to be stored in the status variable and returned
     */
    fsbl_printf(DEBUG_INFO, "In FsblHookAfterBitstreamDload function \r\n");

    return (Status);
}

/******************************************************************************
* This function is the hook which will be called  before the FSBL does a handoff
* to the application. The user can add all the customized code required to be
* executed before the handoff to this routine.
*
* @param None
*
* @return
*       - XST_SUCCESS to indicate success
*       - XST_FAILURE.to indicate failure
*
****************************************************************************/
u32 FsblHookBeforeHandoff(void)
{
    u32 Status;

    Status = XST_SUCCESS;

    /*
     * User logic to be added here.
     * Errors to be stored in the status variable and returned
     */
    fsbl_printf(DEBUG_INFO,"In FsblHookBeforeHandoff function \r\n");

    u32 BootModeRegister;
    BootModeRegister = Xil_In32(BOOT_MODE_REG);
    BootModeRegister &= BOOT_MODES_MASK;
    if (BootModeRegister == SD_MODE) {
        xil_printf("\r\nBooting from SD\r\n");
        FpgaV3_Init_SD();
    }
    else if (BootModeRegister == QSPI_MODE) {
        xil_printf("\r\nBooting from QSPI\r\n");
        FpgaV3_Init_QSPI();
    }
    return (Status);
}


/******************************************************************************
* This function is the hook which will be called in case FSBL fall back
*
* @param None
*
* @return None
*
****************************************************************************/
void FsblHookFallback(void)
{
    /*
     * User logic to be added here.
     * Errors to be stored in the status variable and returned
     */
    fsbl_printf(DEBUG_INFO,"In FsblHookFallback function \r\n");
    while(1);
}

// Initialization function called when booting from SD. This function checks
// the QSPI and programs the quad enable (QE) bit if needed.
bool FpgaV3_Init_SD()
{
    return QSPI_Configure();
}

// Initialization function called when booting from QSPI. This function
// displays information about the board and copies the FPGA S/N from QSPI
// to FPGA.
bool FpgaV3_Init_QSPI()
{
    xil_printf("\r\n*** FPGA V3 ***\r\n\n");

    // Display software version (defined in fpgav3_version.h)
    xil_printf("Software Version %s", FPGAV3_VERSION);
    if (strcmp(FPGAV3_GIT_VERSION, FPGAV3_VERSION) != 0)
        xil_printf(" (git %s)", FPGAV3_GIT_VERSION);
    xil_printf("\r\n\n");

    // Initialize EMIO bus interface
    EMIO_Init();

    // Check connected board
    uint32_t hw_version;
    EMIO_ReadQuadlet(4, &hw_version);
    if (hw_version == 0x42434647) {   // "BCFG"
        uint32_t board_status;
        EMIO_ReadQuadlet(0, &board_status);
        unsigned int board_id = (board_status&0x0f000000)>>24;
        xil_printf("Board ID: %d\r\n", board_id);
        xil_printf("Board status: %x", board_status);
        if (board_status&0x00800000) xil_printf(", No board");
        if (board_status&0x00400000) xil_printf(", QLA");
        if (board_status&0x00200000) xil_printf(", DQLA");
        if (board_status&0x00100000) xil_printf(", DRAC");
        if (board_status&0x00002000) xil_printf(", TEST");
        // Check for V3.0 takes some time, so not valid yet
        // if (board_status&0x00080000) xil_printf(", V3.0");
        xil_printf("\r\n");
    }
    else {
        xil_printf("Did not find BCFG firmware: %x\r\n", hw_version);
    }

    // Query flash and get FPGA S/N
    char sn_buff[16];
    if (QspiAccess(0xff0000, (u32)sn_buff, sizeof(sn_buff)) == XST_SUCCESS) {
        if (strncmp(sn_buff, "FPGA ", 5) == 0) {
            // Write to FPGA PROM registers
            EMIO_WritePromData(sn_buff, sizeof(sn_buff));
            char *p = strchr(sn_buff, 0xff);
            if (p)
                *p = 0;                  // Null terminate at first 0xff
            else
                sn_buff[13] = 0;         // or at end of string

            xil_printf("FPGA S/N: %s\r\n", sn_buff+5);
        }
    }
    else {
        xil_printf("Failed to read from QSPI\r\n");
    }

    return true;
}
