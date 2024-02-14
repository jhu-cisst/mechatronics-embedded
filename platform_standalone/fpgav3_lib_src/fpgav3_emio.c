/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-    */
/* ex: set filetype=cpp softtabstop=4 shiftwidth=4 tabstop=4 cindent expandtab: */

#include <stdbool.h>
#include "xparameters.h"
#include "xil_printf.h"
#include "xil_io.h"
#include "fpgav3_emio.h"

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
    Bits_RequestBus     = 0x00010000,   // Request read or write bus (output)
    Bits_OpDone         = 0x00020000,   // Read or write done (input)
    Bits_RegWen         = 0x00040000,   // Register write enable (output)
    Bits_BlkStart       = 0x00080000,   // Block start (output)
    Bits_BlkEnd         = 0x00100000,   // Block end (output)
    Bits_Write          = 0x00200000,   // Write operation detected (based on tristate)
    Bits_GrantBus       = 0x00400000,   // Grant of read or write bus (input)
    Bits_LSB            = 0x00800000,   // Address least significant bit (output)
    Bits_Version        = 0xf0000000    // Bus interface version (input)
};

void EMIO_Init()
{
    // emio[31:0] is reg_data (initialize as input)
    Xil_Out32(XPS_GPIO_BASEADDR + Reg_DirLower, 0x00000000);
    // emio[63:60] is version number (input)
    // emio[55] is addr_lsb (output)
    // emio[54] is grant_bus (input)
    // emio[53] is write (input)
    // emio[52] is blk_end (output)
    // emio[51] is blk_start (output)
    // emio[50] is reg_wen (output)
    // emio[49] is op_done (input)
    // emio[48] is req_bus (output)
    // emio[47:32] is reg_addr (output)
    Xil_Out32(XPS_GPIO_BASEADDR + Reg_DirUpper,
              Bits_RegAddr | Bits_RequestBus | Bits_RegWen | Bits_BlkStart | Bits_BlkEnd | Bits_LSB);
    // Initialize all outputs to 0
    Xil_Out32(XPS_GPIO_BASEADDR + Reg_OutputUpper, 0x00000000);
    // Enable all outputs
    Xil_Out32(XPS_GPIO_BASEADDR + Reg_OutEnUpper,
              Bits_RegAddr | Bits_RequestBus | Bits_RegWen | Bits_BlkStart | Bits_BlkEnd | Bits_LSB);
}

unsigned int EMIO_GetVersion()
{
    uint32_t upper;
    upper = Xil_In32(XPS_GPIO_BASEADDR + Reg_InputUpper);
    return (upper & Bits_Version) >> 28;
}

static bool WaitOpDone(const char *opType, uint16_t addr, unsigned int num)
{
    // op_done should be set quickly by firmware, to indicate that read or write
    // has completed. If the FPGA bus is not busy (due to Firewire or Ethernet
    // access), it should only take about 1 iteration of the following loop.
    int i;
    uint32_t upper = 0;
    for (i = 0; (i < num) && !(upper & Bits_OpDone); i++) {
        upper = Xil_In32(XPS_GPIO_BASEADDR + Reg_InputUpper);
    }
    if (i == num) {
        // If the above loop timed out, we print a message and keep checking.
        // It would be better to set a timeout to avoid an infinite loop.
        xil_printf("Waiting to %s addr %x", opType, addr);
        while (!(upper & Bits_OpDone)) {
            upper = Xil_In32(XPS_GPIO_BASEADDR + Reg_InputUpper);
            xil_printf(".");
            i++;
        }
        xil_printf("\r\nWaited %d iterations\r\n", i);
    }
    return true;
}

// Local method to check whether data lines are set for input.
// This verifies the assumption that reg_data is set for input, since that
// is their initial value and the Write methods set them to input before
// returning.
static bool isDataInput(const char *name)
{
    uint32_t upper;
    const uint32_t upper_mask = Bits_RequestBus | Bits_Write;
    upper = Xil_In32(XPS_GPIO_BASEADDR + Reg_InputUpper);
    // Do not want RequestBus or Write to be set
    if (upper & upper_mask) {
        xil_printf("%s: invalid state %x\r\n", name, upper);
        return false;
    }
    return true;
}

/// Local method to set data lines as output and do some sanity checking.
static bool setDataOutput(const char *name)
{
    // Set reg_data as output
    Xil_Out32(XPS_GPIO_BASEADDR + Reg_DirLower, 0xffffffff);
    // Enable all outputs
    Xil_Out32(XPS_GPIO_BASEADDR + Reg_OutEnLower, 0xffffffff);
    // Now, do some sanity checking
    uint32_t upper;
    const uint32_t upper_mask = Bits_RequestBus | Bits_Write;
    upper = Xil_In32(XPS_GPIO_BASEADDR + Reg_InputUpper);
    // Do not want RequestBus to be set, but Write must be set
    if ((upper & upper_mask) != Bits_Write) {
        xil_printf("%s: invalid state %x\r\n", name, upper);
        return false;
    }
    return true;
}

bool EMIO_ReadQuadlet(uint16_t addr, uint32_t *data)
{
    // Verify that data lines set for input
    if (!isDataInput("EMIO_ReadQuadlet"))
        return false;

    uint32_t outreg = addr;
    // Write reg_addr (read address)
    // Also set req_bus to 1 (rising edge requests firmware to read register)
    // Because the firmware latches req_bus, there is enough delay that we can set it now
    Xil_Out32(XPS_GPIO_BASEADDR + Reg_OutputUpper, outreg | Bits_RequestBus);
    // Wait for op_done to be set
    WaitOpDone("read", addr, 5);
    // Local method to wait for op_done to be set, using either events or polling (default)
    // Read data from reg_data
    *data = Xil_In32(XPS_GPIO_BASEADDR + Reg_InputLower);
    // Set req_bus to 0 (also sets reg_addr to 0)
    Xil_Out32(XPS_GPIO_BASEADDR + Reg_OutputUpper, 0x00000000);
    return true;
}

bool EMIO_WriteQuadlet(uint16_t addr, uint32_t data)
{
    if (!setDataOutput("EMIO_WriteQuadlet"))
        return false;

    // Write reg_wdata (write data)
    Xil_Out32(XPS_GPIO_BASEADDR + Reg_OutputLower, data);
    // Write reg_waddr (write address) and reg_wen (no longer used)
    // Also set req_bus to 1 (rising edge requests firmware to write register)
    // Because the firmware latches req_bus, there is enough delay that we can set it now
    uint32_t outreg = addr;
    Xil_Out32(XPS_GPIO_BASEADDR + Reg_OutputUpper, outreg | Bits_RegWen | Bits_RequestBus);
    // Wait for op_done to be set
    WaitOpDone("write", addr, 5);
    // Set req_bus to 0 (also sets reg_addr and reg_wen to 0)
    Xil_Out32(XPS_GPIO_BASEADDR + Reg_OutputUpper, 0x00000000);
    // Set reg_data as input (default state)
    Xil_Out32(XPS_GPIO_BASEADDR + Reg_DirLower, 0x00000000);
    return true;
}

bool EMIO_ReadBlock(uint16_t addr, uint32_t *data, unsigned int nBytes)
{
    unsigned int ver = EMIO_GetVersion();
    if (ver != 1) {
        xil_printf("EMIO_ReadBlock: unsupported interface version %d\r\n", ver);
        return false;
    }

    if (!isDataInput("EMIO_ReadBlock"))
        return false;

    uint32_t val;
    unsigned int q;
    unsigned int nQuads = (nBytes+3)/4;

    // Set base address, blk_start
    // Also set req_bus to 1
    // Because the firmware latches req_bus, there is enough delay that we can set it now
    uint32_t outreg = addr | Bits_BlkStart | Bits_RequestBus;

    for (q = 0; q < nQuads; q++) {

        if (q == nQuads-1) {
            // Set blk_end to 1 to indicate end of block write
            outreg &= ~Bits_BlkStart;
            outreg |=  Bits_BlkEnd;
        }
        // Update LSB and write to register
        if (addr&0x0001) outreg |=  Bits_LSB;
        else             outreg &= ~Bits_LSB;
        Xil_Out32(XPS_GPIO_BASEADDR + Reg_OutputUpper, outreg);

        // Increment address (it would be enough to toggle LSB)
        addr++;

        WaitOpDone("read", addr, 5);

        // Read data from reg_data
        val = Xil_In32(XPS_GPIO_BASEADDR + Reg_InputLower);
        // data[q] = Xil_EndianSwap32(val);
        data[q] = val;
    }

    // Set all lines to 0
    Xil_Out32(XPS_GPIO_BASEADDR + Reg_OutputUpper, 0);

    return true;
}

bool EMIO_WriteBlock(uint16_t addr, uint32_t *data, unsigned int nBytes)
{
    unsigned int ver = EMIO_GetVersion();
    if (ver != 1) {
        xil_printf("EMIO_WriteBlock: unsupported interface version %d\r\n", ver);
        return false;
    }

    uint32_t val;
    unsigned int q;
    unsigned int nQuads = (nBytes+3)/4;

    if (!setDataOutput("EMIO_WriteBlock"))
        return false;

    // Set addr, blk_start and req_bus
    // Because the firmware latches req_bus, there is enough delay that we can set it now
    uint32_t outreg = addr | Bits_BlkStart | Bits_RequestBus;

    for (q = 0; q < nQuads; q++) {

        // Write data
        // val = Xil_EndianSwap32(data[q]);
        val = data[q];
        Xil_Out32(XPS_GPIO_BASEADDR + Reg_OutputLower, val);

        if (q == nQuads-1) {
            // Set blk_end to 1 to indicate end of block write
            outreg &= ~Bits_BlkStart;
            outreg |=  Bits_BlkEnd;
        }

        // Update LSB and write to register
        if (addr&0x0001) outreg |=  Bits_LSB;
        else             outreg &= ~Bits_LSB;
        Xil_Out32(XPS_GPIO_BASEADDR + Reg_OutputUpper, outreg);

        // Increment address (it would be enough to toggle LSB)
        addr++;

        // Wait for op_done to be set
        WaitOpDone("write", addr, 5);
    }

    // Set all lines to 0
    Xil_Out32(XPS_GPIO_BASEADDR + Reg_OutputUpper, 0);
    // Set reg_data as input (default state)
    Xil_Out32(XPS_GPIO_BASEADDR + Reg_DirLower, 0);
    return true;
}

bool EMIO_WritePromData(char *data, unsigned int nBytes)
{
    uint16_t nQuads = (nBytes+3)/4;
    for (uint16_t i = 0; i < nQuads; i++) {
        uint32_t prom_data = Xil_EndianSwap32(*(u32 *)(data+i*4));
        if (!EMIO_WriteQuadlet(0x2000+i, prom_data)) {
            xil_printf("EMIO_WritePromData failed\r\n");
            return false;
        }
    }
    return true;
}
