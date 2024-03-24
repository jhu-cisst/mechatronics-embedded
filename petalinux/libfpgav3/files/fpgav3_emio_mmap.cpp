/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-    */
/* ex: set filetype=cpp softtabstop=4 shiftwidth=4 tabstop=4 cindent expandtab: */

#include <iostream>
#include <byteswap.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include "fpgav3_emio_mmap.h"

// System level control registers
const uint32_t SLCR_BASE_ADDR     = 0xF8000000;
const uint32_t APER_CLK_CTRL      = 0x0000012C;   // APER clock control register
const uint32_t GPIO_CLK_BIT       = (1 << 22);    // Bit 22 is for GPIO AMBA clock control

// GPIO control registers
const uint32_t GPIO_BASE_ADDR     = 0xE000A000;
// Following size includes all GPIO registers (from 0x0000 to 0x02E4), which is
// more than enough (we only use 0x0048 - 0x02c8).
const uint32_t GPIO_SIZE          = 0x000002E8;

// GPIO registers used for EMIO interface
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

EMIO_Interface_Mmap::EMIO_Interface_Mmap() : EMIO_Interface()
{
    if (!Init()) {
        if (fd >= 0) close(fd);
        fd = -1;
        mmap_region = 0;
    }
}

bool EMIO_Interface_Mmap::Init()
{
    // Open /dev/mem for accessing physical memory
    if ((fd = open("/dev/mem", O_RDWR | O_SYNC)) < 0) {
        std::cout << "fpgav3_emio_mmap: failed to open /dev/mem" << std::endl;
        return false;
    }

    // Temporarily use mmap_region for SLCR registers (clock)
    mmap_region = mmap_init(SLCR_BASE_ADDR, APER_CLK_CTRL+4);
    if (!mmap_region)
        return false;

    // Check clock bit and set if necessary
    uint32_t aper_clk_reg = RegisterRead(APER_CLK_CTRL);

    if ((aper_clk_reg & GPIO_CLK_BIT) == 0) {
        if (isVerbose)
            std::cout << "Setting clock bit" << std::endl;
        RegisterWrite(APER_CLK_CTRL, aper_clk_reg | GPIO_CLK_BIT);
    }

    // Release mmap_region
    munmap(mmap_region, APER_CLK_CTRL+4);

    // Now, map the EMIO registers
    mmap_region = mmap_init(GPIO_BASE_ADDR, GPIO_SIZE);
    if (!mmap_region)
        return false;

    // Set upper bits as input and read bus version number
    RegisterWrite(Reg_DirUpper, 0x00000000);
    uint32_t upper;
    upper = RegisterRead(Reg_InputUpper);
    version = (upper & Bits_Version) >> 28;

    if (version > 1) {
        std::cout << "EMIO bus interface version " << version << " not supported" << std::endl;
        return false;
    }

    // emio[31:0] is reg_data (initialize as input)
    RegisterWrite(Reg_DirLower, 0x00000000);
    isInput = true;
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
    RegisterWrite(Reg_DirUpper,
                  Bits_RegAddr | Bits_RequestBus | Bits_RegWen | Bits_BlkStart | Bits_BlkEnd | Bits_LSB);
    // Initialize all outputs to 0
    RegisterWrite(Reg_OutputUpper, 0x00000000);
    // Enable all outputs
    RegisterWrite(Reg_OutEnUpper,
                  Bits_RegAddr | Bits_RequestBus | Bits_RegWen | Bits_BlkStart | Bits_BlkEnd | Bits_LSB);

    return true;
}

// Local method to map physical memory
void *EMIO_Interface_Mmap::mmap_init(uint32_t base_addr, uint16_t size)
{
    void *region = mmap(
        NULL,
        size,
        PROT_READ | PROT_WRITE,
        MAP_SHARED,
        fd,
        base_addr
    );

    if (region == MAP_FAILED) {
        std::cout << "fpgav3_emio_mmap: failed to mmap addr "
                  << std::hex << base_addr << std::dec << std::endl;
        return 0;
    }

    return region;
}

EMIO_Interface_Mmap::~EMIO_Interface_Mmap()
{
    if (mmap_region) {
        munmap(mmap_region, GPIO_SIZE);
        if (fd >= 0) close(fd);
    }
}

void EMIO_Interface_Mmap::SetEventMode(bool newState)
{
    if (newState)
        std::cout << "EMIO_Interface_Mmap: events not implemented (using polling)" << std::endl;
    useEvents = false;
}

uint32_t EMIO_Interface_Mmap::RegisterRead(uint32_t reg_addr)
{
    return *(reinterpret_cast<uint32_t *>(mmap_region)+(reg_addr/sizeof(uint32_t)));
}

void EMIO_Interface_Mmap::RegisterWrite(uint32_t reg_addr, uint32_t reg_data)
{
    *(reinterpret_cast<uint32_t *>(mmap_region)+(reg_addr/sizeof(uint32_t))) = reg_data;
}

// Local method to wait for op_done to be set, using polling
bool EMIO_Interface_Mmap::WaitOpDone(const char *opType, unsigned int num)
{
    // op_done should be set quickly by firmware, to indicate that read or write
    // has completed. If the FPGA bus is not busy (due to Firewire or Ethernet
    // access), it should be ready right away.
    uint32_t upper = RegisterRead(Reg_InputUpper);
    if (!(upper & Bits_OpDone)) {
        fpgav3_time_t waitStart;
        fpgav3_time_t curTime;
        GetCurTime(&waitStart);
        double dt = 0.0;   // elapsed time in us
        while (dt < timeout_us) {
            upper = RegisterRead(Reg_InputUpper);
            if (upper & Bits_OpDone)
                break;
            GetCurTime(&curTime);
            dt = TimeDiff_us(&waitStart, &curTime);
        }
        if (isVerbose) {
            if (upper & Bits_OpDone)
                std::cout << "Waited " << dt << " us for " << opType << " quadlet " << num << std::endl;
            else
                std::cout << "EMIO polling timeout waiting for " << opType << " quadlet " << num << std::endl;
        }
    }
    return true;
}

// ReadQuadlet: read quadlet from specified address
bool EMIO_Interface_Mmap::ReadQuadlet(uint16_t addr, uint32_t &data)
{
    // For timing measurements
    fpgav3_time_t beforeWait;
    fpgav3_time_t afterWait;

    // Get start time for measurement
    if (doTiming > 0)
        GetCurTime(&startTime);

    // Set all data lines to input
    if (!isInput) {
       RegisterWrite(Reg_DirLower, 0x00000000);
       isInput = true;
    }

    uint32_t outreg = addr;
    // Write reg_addr (read address)
    // Also set req_bus to 1 (rising edge requests firmware to read register)
    // Because the firmware latches req_bus, there is enough delay that we can set it now
    RegisterWrite(Reg_OutputUpper, outreg | Bits_RequestBus);

    // Get time before wait
    if (doTiming > 1)
        GetCurTime(&beforeWait);

    // Wait for op_done to be set
    if (!WaitOpDone("read", 0)) {
        RegisterWrite(Reg_OutputUpper, outreg & (~Bits_RequestBus));
        return false;
    }

    // Get time after wait
    if (doTiming > 1)
        GetCurTime(&afterWait);

    // Read data
    data = RegisterRead(Reg_InputLower);
    // Set req_bus to 0 (also sets reg_addr to 0)
    RegisterWrite(Reg_OutputUpper, 0x00000000);

    // Get end time
    if (doTiming > 0) {
        GetCurTime(&endTime);
        double dt = TimeDiff_us(&startTime, &endTime)-timingOverhead;
        if (doTiming > 1) dt -= 2*timingOverhead;
        std::cout << "ReadQuadlet total time = " << dt << " us" << std::endl;
        if (doTiming > 1) {
            std::cout << "Start-wait-end times = " 
                      << TimeDiff_us(&startTime, &beforeWait)-timingOverhead << ", "
                      << TimeDiff_us(&beforeWait, &afterWait)-timingOverhead << ", "
                      << TimeDiff_us(&afterWait, &endTime)-timingOverhead << " us" << std::endl;
        }
    }

    return true;
}

// WriteQuadlet: write quadlet to specified address
bool EMIO_Interface_Mmap::WriteQuadlet(uint16_t addr, uint32_t data)
{
    // For timing measurements
    fpgav3_time_t beforeWait;
    fpgav3_time_t afterWait;

    // Get start time for measurement
    if (doTiming > 0)
        GetCurTime(&startTime);

    if (isInput) {
        // Set all data lines to output
        RegisterWrite(Reg_DirLower, 0xffffffff);
        // Enable all outputs
        RegisterWrite(Reg_OutEnLower, 0xffffffff);
        isInput = false;
    }

    // Write reg_wdata (write data)
    RegisterWrite(Reg_OutputLower, data);

    // Write reg_waddr (write address) and reg_wen (no longer used)
    // Also set req_bus to 1 (rising edge requests firmware to write register)
    // Because the firmware latches req_bus, there is enough delay that we can set it now
    uint32_t outreg = addr;
    RegisterWrite(Reg_OutputUpper, outreg | Bits_RegWen | Bits_RequestBus);

    // Get time before wait
    if (doTiming > 1)
        GetCurTime(&beforeWait);

    // Wait for op_done to be set
    bool ret = WaitOpDone("write", 0);

    // Get time after wait
    if (ret && (doTiming > 1))
        GetCurTime(&afterWait);

    // Set req_bus to 0 (also sets reg_addr and reg_wen to 0)
    RegisterWrite(Reg_OutputUpper, 0x00000000);
    // Set reg_data as input (default state)
    RegisterWrite(Reg_DirLower, 0x00000000);

    // Get end time
    if (ret && (doTiming > 0)) {
        GetCurTime(&endTime);
        double dt = TimeDiff_us(&startTime, &endTime)-timingOverhead;
        if (doTiming > 1) dt -= 2*timingOverhead;
        std::cout << "WriteQuadlet total time = " << dt << " us" << std::endl;
        if (doTiming > 1) {
            std::cout << "Start-wait-end times = "
                      << TimeDiff_us(&startTime,  &beforeWait)-timingOverhead << ", "
                      << TimeDiff_us(&beforeWait, &afterWait)-timingOverhead << ", "
                      << TimeDiff_us(&afterWait,  &endTime)-timingOverhead << " us" << std::endl;
        }
    }

    return true;
}

// Read block of data
bool EMIO_Interface_Mmap::ReadBlock(uint16_t addr, uint32_t *data, unsigned int nBytes)
{
    // For timing measurements
    fpgav3_time_t firstRead;
    fpgav3_time_t lastRead;

    if (version < 1) {
        std::cout << "ReadBlock (mmap): not supported for version " << version << std::endl;
        return false;
    }

    // Get start time for measurement
    if (doTiming > 0)
        GetCurTime(&startTime);

    // Set all data lines to input
    if (!isInput) {
       RegisterWrite(Reg_DirLower, 0x00000000);
       isInput = true;
    }

    uint32_t val;
    unsigned int q;
    unsigned int nQuads = (nBytes+3)/4;

    // Set base address, blk_start
    // Also set req_bus to 1
    // Because the firmware latches req_bus, there is enough delay that we can set it now
    uint32_t outreg = addr | Bits_BlkStart | Bits_RequestBus;

    if (doTiming > 1)
        GetCurTime(&firstRead);

    for (q = 0; q < nQuads; q++) {

        if (q == nQuads-1) {
            // Set blk_end to 1 to indicate end of block write
            outreg &= ~Bits_BlkStart;
            outreg |=  Bits_BlkEnd;
        }
        // Update LSB and write to register
        if (addr&0x0001) outreg |=  Bits_LSB;
        else             outreg &= ~Bits_LSB;
        RegisterWrite(Reg_OutputUpper, outreg);

        // Increment address (it would be enough to toggle LSB)
        addr++;

        if (!WaitOpDone("read", q)) {
            RegisterWrite(Reg_OutputUpper, 0);
            return false;
        }

        // Read data from reg_data
        val = RegisterRead(Reg_InputLower);
        data[q] = bswap_32(val);
    }

    if (doTiming > 1)
        GetCurTime(&lastRead);

    // Set all lines to 0
    RegisterWrite(Reg_OutputUpper, 0);

    // Get end time
    if (doTiming > 0) {
        GetCurTime(&endTime);
        double dt = TimeDiff_us(&startTime, &endTime)-timingOverhead;
        if (doTiming > 1) dt -= 2*timingOverhead;
        std::cout << "ReadBlock total time = " << dt << " us" << std::endl;
        if (doTiming > 1) {
            std::cout << "Start-loop-end times = "
                      << TimeDiff_us(&startTime, &firstRead)-timingOverhead << ", "
                      << TimeDiff_us(&firstRead, &lastRead)-timingOverhead << ", "
                      << TimeDiff_us(&lastRead,  &endTime)-timingOverhead << " us" << std::endl;
        }
    }

    return true;
}

// Write block of data.
bool EMIO_Interface_Mmap::WriteBlock(uint16_t addr, const uint32_t *data, unsigned int nBytes)
{
    // For timing measurements
    fpgav3_time_t firstWrite;
    fpgav3_time_t lastWrite;

    uint32_t val;
    unsigned int q;
    unsigned int nQuads = (nBytes+3)/4;

    if (version < 1) {
        std::cout << "WriteBlock (mmap): not supported for version " << version << std::endl;
        return false;
    }

    // Get start time for measurement
    if (doTiming > 0)
        GetCurTime(&startTime);

    if (isInput) {
        // Set all data lines to output
        RegisterWrite(Reg_DirLower, 0xffffffff);
        // Enable all outputs
        RegisterWrite(Reg_OutEnLower, 0xffffffff);
        isInput = false;
    }

    // Set addr, blk_start and req_bus
    // Because the firmware latches req_bus, there is enough delay that we can set it now
    uint32_t outreg = addr | Bits_BlkStart | Bits_RequestBus;

    if (doTiming > 1)
        GetCurTime(&firstWrite);

    for (q = 0; q < nQuads; q++) {

        // Write data
        val = bswap_32(data[q]);
        RegisterWrite(Reg_OutputLower, val);

        if (q == nQuads-1) {
            // Set blk_end to 1 to indicate end of block write
            outreg &= ~Bits_BlkStart;
            outreg |=  Bits_BlkEnd;
        }

        // Update LSB and write to register
        if (addr&0x0001) outreg |=  Bits_LSB;
        else             outreg &= ~Bits_LSB;
        RegisterWrite(Reg_OutputUpper, outreg);

        // Increment address (it would be enough to toggle LSB)
        addr++;

        // Wait for op_done to be set
        if (!WaitOpDone("write", q)) {
            RegisterWrite(Reg_OutputUpper, 0);
            RegisterWrite(Reg_DirLower, 0);
            return false;
        }
    }

    if (doTiming > 1)
        GetCurTime(&lastWrite);

    // Set all lines to 0
    RegisterWrite(Reg_OutputUpper, 0);
    // Set reg_data as input (default state)
    RegisterWrite(Reg_DirLower, 0);

    // Get end time
    if (doTiming > 0) {
        GetCurTime(&endTime);
        double dt = TimeDiff_us(&startTime, &endTime)-timingOverhead;
        if (doTiming > 1) dt -= 2*timingOverhead;
        std::cout << "WriteBlock total time = " << dt << " us" << std::endl;
        if (doTiming > 1) {
            std::cout << "Start-loop-end times = "
                      << TimeDiff_us(&startTime,  &firstWrite)-timingOverhead << ", "
                      << TimeDiff_us(&firstWrite, &lastWrite)-timingOverhead << ", "
                      << TimeDiff_us(&lastWrite,  &endTime)-timingOverhead << " us" << std::endl;
        }
    }

    return true;
}
