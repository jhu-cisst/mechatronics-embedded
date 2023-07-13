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
    Bits_BlkWStart      = 0x00080000,   // Block write start (output)
    Bits_BlkWen         = 0x00100000,   // Block write enable (output)
    Bits_Write          = 0x00200000    // Write operation detected (based on tristate)
};

void EMIO_Init()
{
    // emio[31:0] is reg_data (initialize as input)
    Xil_Out32(XPS_GPIO_BASEADDR + Reg_DirLower, 0x00000000);
    // emio[53] is write (input)
    // emio[52] is blk_wen (output)
    // emio[51] is blk_wstart (output)
    // emio[50] is reg_wen (output)
    // emio[49] is op_done (input)
    // emio[48] is req_bus (output)
    // emio[47:32] is reg_addr (output)
    Xil_Out32(XPS_GPIO_BASEADDR + Reg_DirUpper,
              Bits_RegAddr | Bits_RequestBus | Bits_RegWen | Bits_BlkWStart | Bits_BlkWen);
    // Initialize all outputs to 0
    Xil_Out32(XPS_GPIO_BASEADDR + Reg_OutputUpper, 0x00000000);
    // Enable all outputs
    Xil_Out32(XPS_GPIO_BASEADDR + Reg_OutEnUpper,
              Bits_RegAddr | Bits_RequestBus | Bits_RegWen | Bits_BlkWStart | Bits_BlkWen);
}

bool EMIO_ReadQuadlet(uint16_t addr, uint32_t *data)
{
    // We assume the data lines (reg_data) are set for input, since that
    // is their initial value and the Write methods set them to input before
    // returning. But, the following test will verify this assumption.
    uint32_t upper;
    const uint32_t upper_mask = Bits_RequestBus | Bits_Write;
    upper = Xil_In32(XPS_GPIO_BASEADDR + Reg_InputUpper);
    // Do not want RequestBus or Write to be set
    if (upper & upper_mask) {
        xil_printf("EMIO_ReadQuadlet: invalid state %x\r\n", upper);
        return false;
    }
    uint32_t outreg = addr;
    // Write reg_addr (read address)
    // Also set req_bus to 1 (rising edge requests firmware to read register)
    // Because the firmware latches req_bus, there is enough delay that we can set it now
    Xil_Out32(XPS_GPIO_BASEADDR + Reg_OutputUpper, outreg | Bits_RequestBus);
    // op_done should be set quickly by firmware, to indicate that read
    // has completed. If the FPGA bus is not busy (due to Firewire or Ethernet
    // access), it should only take about 1 iteration of the following loop.
    int i;
    upper = 0;
    for (i = 0; (i < 5) && !(upper & Bits_OpDone); i++) {
        upper = Xil_In32(XPS_GPIO_BASEADDR + Reg_InputUpper);
    }
    if (i == 5) {
        // If the above loop timed out, we print a message and keep checking.
        // It would be better to set a timeout to avoid an infinite loop.
        xil_printf("Waiting to read addr %x", addr);
        while (!(upper & Bits_OpDone)) {
            upper = Xil_In32(XPS_GPIO_BASEADDR + Reg_InputUpper);
            xil_printf(".");
            i++;
        }
        xil_printf("\r\nWaited %d iterations\r\n", i);
    }
    // Read data from reg_data
    *data = Xil_In32(XPS_GPIO_BASEADDR + Reg_InputLower);
    // Set req_bus to 0 (also sets reg_addr to 0)
    Xil_Out32(XPS_GPIO_BASEADDR + Reg_OutputUpper, 0x00000000);
    return true;
}

bool EMIO_WriteQuadlet(uint16_t addr, uint32_t data)
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
        xil_printf("EMIO_WriteQuadlet: invalid state %x\r\n", upper);
        return false;
    }
    // Write reg_wdata (write data)
    Xil_Out32(XPS_GPIO_BASEADDR + Reg_OutputLower, data);
    // Write reg_waddr (write address) and reg_wen
    // Also set req_bus to 1 (rising edge requests firmware to write register)
    // Because the firmware latches req_bus, there is enough delay that we can set it now
    uint32_t outreg = addr;
    Xil_Out32(XPS_GPIO_BASEADDR + Reg_OutputUpper, outreg | Bits_RegWen | Bits_RequestBus);
    // op_done should be set quickly by firmware, to indicate that write
    // has completed. If the FPGA bus is not busy (due to Firewire or Ethernet
    // access), it should only take about 1 iteration of the following loop.
    int i;
    upper = 0;
    for (i = 0; (i < 5) && !(upper & Bits_OpDone); i++) {
        upper = Xil_In32(XPS_GPIO_BASEADDR + Reg_InputUpper);
    }
    if (i == 5) {
        // If the above loop timed out, we print a message and keep checking.
        // It would be better to set a timeout to avoid an infinite loop.
        xil_printf("Waiting to write addr %x", addr);
        while (!(upper & Bits_OpDone)) {
            upper = Xil_In32(XPS_GPIO_BASEADDR + Reg_InputUpper);
            xil_printf(".");
            i++;
        }
        xil_printf("\r\nWaited %d iterations\r\n", i);
    }
    // Set req_bus to 0 (also sets reg_addr and reg_wen to 0)
    Xil_Out32(XPS_GPIO_BASEADDR + Reg_OutputUpper, 0x00000000);
    // Set reg_data as input (default state)
    Xil_Out32(XPS_GPIO_BASEADDR + Reg_DirLower, 0x00000000);
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
