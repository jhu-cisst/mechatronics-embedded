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
#include "xqspips.h"
#include "fpgav3_emio.h"

/************************** Variable Definitions *****************************/

// Defined in qspi.c
const u8 READ_ID_CMD = 0x9F;
const u8 READ_ID_SIZE = 4;          /* Read ID command + 3 bytes ID response */
const u8 WRITE_ENABLE_CMD = 0x06;
const u8 WRITE_ENABLE_CMD_SIZE = 1; /* WE command */

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
        xil_printf("Booting from SD\r\n");
        FpgaV3_Init_SD();
    }
    else if (BootModeRegister == QSPI_MODE) {
        xil_printf("Booting from QSPI\r\n");
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
// the QSPI and programs the quad enable (QE) bit if needed -- TBD.
bool FpgaV3_Init_SD()
{
    XQspiPs QspiInstance;
    XQspiPs_Config *QspiConfig;
    u8 ReadBuffer[4];
    u8 WriteBuffer[4];

    QspiConfig = XQspiPs_LookupConfig(XPAR_XQSPIPS_0_DEVICE_ID);
    if (!QspiConfig) {
        xil_printf("Failed to look up QSPI config\r\n");
        return false;
    }

    if (XQspiPs_CfgInitialize(&QspiInstance, QspiConfig, QspiConfig->BaseAddress) != XST_SUCCESS) {
        xil_printf("Failed to initialize QSPI interface\r\n");
        return false;
    }

    // Set Manual Chip select options and drive HOLD_B pin high.
    XQspiPs_SetOptions(&QspiInstance, XQSPIPS_FORCE_SSELECT_OPTION | XQSPIPS_HOLD_B_DRIVE_OPTION);

    // Set the prescaler for QSPI clock
    XQspiPs_SetClkPrescaler(&QspiInstance, XQSPIPS_CLK_PRESCALE_8);

    // Assert the FLASH chip select.
    XQspiPs_SetSlaveSelect(&QspiInstance);

    // Read the Chip ID. We expect this to be WINBOND 128MB. The only unknown is whether
    // it is a standard chip (0x40) or an "M" chip (0x70)
    WriteBuffer[0] = READ_ID_CMD;
    WriteBuffer[1] = 0x00;
    WriteBuffer[2] = 0x00;
    WriteBuffer[3] = 0x00;

    u32 ret = XQspiPs_PolledTransfer(&QspiInstance, WriteBuffer, ReadBuffer, READ_ID_SIZE);
    if (ret != XST_SUCCESS) {
        xil_printf("Failed to read QSPI Flash ID\r\n");
        return false;
    }

    if (ReadBuffer[1] == WINBOND_ID) {
        xil_printf("QSPI: WINBOND ");
        if (ReadBuffer[2] == 0x70)
            xil_printf("M ");
        if (ReadBuffer[3] == 0x18)
            xil_printf("128MBits\r\n");
        else
            xil_printf("Unexpected size (%x)\r\n", ReadBuffer[3]);
    }
    else {
        xil_printf("Unexpected QSPI Flash ID (%x)\r\n", ReadBuffer[1]);
    }

    return true;
}

// Initialization function called when booting from QSPI. This function
// displays information about the board and copies the FPGA S/N from QSPI
// to FPGA.
bool FpgaV3_Init_QSPI()
{
    xil_printf("\r\n*** FPGA V3 ***\r\n\n");

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
        // Check for V3.0 takes some time, so not valid yet
        // if (board_status&0x00080000) xil_printf(", V3.0");
        xil_printf("\r\n");
    }
    else {
        xil_printf("Did not find BCFG firmware: %x\r\n", hw_version);
    }

    // Query flash and get FPGA S/N
    if (InitQspi() == XST_SUCCESS) {
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
    }
    else {
        xil_printf("Failed to initialize QSPI\r\n");
    }
    return true;
}
