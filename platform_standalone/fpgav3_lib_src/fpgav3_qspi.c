/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-    */
/* ex: set filetype=cpp softtabstop=4 shiftwidth=4 tabstop=4 cindent expandtab: */

#include <stdint.h>
#include "xil_printf.h"
#include "qspi.h"
#include "xqspips.h"
#include "fpgav3_qspi.h"

/************************** Variable Definitions *****************************/

// Defined in qspi.c
const u8 READ_ID_CMD = 0x9F;
const u8 READ_STATUS_REG2 = 0x35;
const u8 WRITE_STATUS_REG2 = 0x31;
const u8 QE_BIT_MASK = 0x02;

const u8 WRITE_ENABLE_CMD = 0x06;

/******************************************************************************/

bool QSPI_Configure()
{
    XQspiPs QspiInstance;
    XQspiPs_Config *QspiConfig;
    u32 ret;
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

    ret = XQspiPs_PolledTransfer(&QspiInstance, WriteBuffer, ReadBuffer, 4);
    if (ret != XST_SUCCESS) {
        xil_printf("Failed to read QSPI Flash ID\r\n");
        return false;
    }

    if (ReadBuffer[1] == WINBOND_ID) {
        xil_printf("QSPI: WINBOND ");
        if (ReadBuffer[3] == 0x18) {
            if (ReadBuffer[2] == 0x40) {
                xil_printf("W25Q128JV\r\n");
            }
            else if (ReadBuffer[2] == 0x70) {
                xil_printf("W25Q128JV-M\r\n");
            }
            else {
                xil_printf("Unexpected device id (%x)\r\n", ReadBuffer[2]);
                return false;
            }
        }
        else {
            xil_printf("Unexpected size (%x)\r\n", ReadBuffer[3]);
            return false;
        }
    }
    else {
        xil_printf("Unexpected QSPI Flash ID (%x)\r\n", ReadBuffer[1]);
        return false;
    }

    // Now, check the Quad Enable (QE) bit in Status Register 2. This should be 1,
    // but for the W25Q128JV-M, it is factory programmed to 0.
    // If the bit is 0, we program it to 1.
    WriteBuffer[0] = READ_STATUS_REG2;
    WriteBuffer[1] = 0x00;

    ret = XQspiPs_PolledTransfer(&QspiInstance, WriteBuffer, ReadBuffer, 2);
    if (ret != XST_SUCCESS) {
        xil_printf("Failed to read QSPI Status Register 2\r\n");
        return false;
    }

    if (ReadBuffer[1] & QE_BIT_MASK) {
        xil_printf("QE bit set\r\n");
    }
    else {
        xil_printf("Programming QE bit\r\n");
        // Set Write Enable (WE)
        WriteBuffer[0] = WRITE_ENABLE_CMD;
        ret = XQspiPs_PolledTransfer(&QspiInstance, WriteBuffer, ReadBuffer, 1);
        if (ret != XST_SUCCESS) {
            xil_printf("Failed to Write Enable QSPI\r\n");
            return false;
        }
        // Write to Status Register 2
        WriteBuffer[0] = WRITE_STATUS_REG2;
        WriteBuffer[1] = ReadBuffer[1] | QE_BIT_MASK;
        ret = XQspiPs_PolledTransfer(&QspiInstance, WriteBuffer, ReadBuffer, 2);
        if (ret != XST_SUCCESS) {
            xil_printf("Failed to write QSPI Status Register 2\r\n");
            return false;
        }
        // Should wait for BUSY bit to be cleared, but since we are not going to access
        // QSPI for awhile (e.g., until Linux boots), it should be fine to skip it.
    }

    return true;
}
