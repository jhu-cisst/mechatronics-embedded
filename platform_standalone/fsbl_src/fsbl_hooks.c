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

#include "fsbl.h"
#include "xstatus.h"
#include "xil_printf.h"
#include "xil_io.h"
#include "fsbl_hooks.h"
#include "qspi.h"

/************************** Variable Definitions *****************************/


/************************** Function Prototypes ******************************/

enum EMIO_Reg {
    Reg_OutputLower = 0x00000048,   // Data output, emio[31:0]
    Reg_OutputUpper = 0x0000004c,   // Data input, emio[63:32]
    Reg_InputLower  = 0x00000068,   // Data input, emio[31:0]
    Reg_InputUpper  = 0x0000006c,   // Data input, emio[63:32]
    Reg_DirLower    = 0x00000284,   // Direction: 0 = input (default), 1 = output
    Reg_OutEnLower  = 0x00000288,   // Output enable: 0 = disabled (default), 1 = enabled
    Reg_DirUpper    = 0x000002c4,   // Direction: 0 = input (default), 1 = output
    Reg_OutEnUpper  = 0x000002c8    // Output enable: 0 = disabled (default), 1 = enabled
};

// Bit masks for upper EMIO bits
enum EMIO_Bits {
    Bits_RegAddr        = 0x0000ffff,   // 16-bit address (output)
    Bits_RequestReadBus = 0x00010000,   // Request read bus (output)
    Bits_ReadDataValid  = 0x00020000    // Read data valid (input)
};

void EMIO_Init()
{
    // emio[31:0] is reg_data (currently input only)
    Xil_Out32(XPS_GPIO_BASEADDR + Reg_DirLower, 0x00000000);
    // emio[49] is reg_rdata_valid (input)
    // emio[48] is req_read_bus (output)
    // emio[47:32] is reg_addr (output)
    Xil_Out32(XPS_GPIO_BASEADDR + Reg_DirUpper,
              Bits_RegAddr | Bits_RequestReadBus);
    // Initialize all outputs to 0
    Xil_Out32(XPS_GPIO_BASEADDR + Reg_OutputUpper, 0x00000000);
    // Enable all outputs
    Xil_Out32(XPS_GPIO_BASEADDR + Reg_OutEnUpper,
              Bits_RegAddr | Bits_RequestReadBus);
}

void EMIO_ReadQuadlet(uint16_t addr, uint32_t *data)
{
    uint32_t outreg = addr;
    // Write reg_addr (read address)
    Xil_Out32(XPS_GPIO_BASEADDR + Reg_OutputUpper, outreg);
    // Set req_read_bus to 1 (rising edge requests firmware to read register)
    outreg |= Bits_RequestReadBus;
    Xil_Out32(XPS_GPIO_BASEADDR + Reg_OutputUpper, outreg);
    // reg_rdata_valid should be set quickly by firmware, to indicate that read
    // has completed. If the FPGA bus is not busy (due to Firewire or Ethernet
    // access), it should only take about 1 iteration of the following loop.
    int i;
    for (i = 0; i < 3; i++) {
        if (Xil_In32(XPS_GPIO_BASEADDR + Reg_InputUpper) & Bits_ReadDataValid) break;
    }
    if (i == 3) {
        // If the above loop timed out, we print a message and keep checking.
        // It would be better to set a timeout to avoid an infinite loop.
        xil_printf("Waiting to read addr %x", addr);
        while (!(Xil_In32(XPS_GPIO_BASEADDR + Reg_InputUpper) & Bits_ReadDataValid)) {
            xil_printf(".");
	    i++;
	}
        xil_printf("\r\nWaited %d iterations\r\n", i);
    }
    // Read data from reg_data
    *data = Xil_In32(XPS_GPIO_BASEADDR + Reg_InputLower);
    // Set req_read_bus to 0 (also sets reg_addr to 0)
    Xil_Out32(XPS_GPIO_BASEADDR + Reg_OutputUpper, 0x00000000);
}

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

    xil_printf("\r\n*** FPGA V3 ***\r\n\n");

    // Check connected board
    EMIO_Init();
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
        char sn_buff[14];
        if (QspiAccess(0xff0000, (u32)sn_buff, sizeof(sn_buff)) == XST_SUCCESS) {
            if (strncmp(sn_buff, "FPGA ", 5) == 0) {
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
