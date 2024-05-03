/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-    */
/* ex: set filetype=cpp softtabstop=4 shiftwidth=4 tabstop=4 cindent expandtab: */

/*
 * fpgav3_emio_mmap (Linux library)
 *
 * This library provides an interface from the Zynq PS to the read and write buses
 * on the FPGA (PL), via the EMIO bits. The read and write buses each consist of
 * a 16-bit address bus and a 32-bit data bus. The EMIO interface uses the same
 * 16 bits for the read/write address and the same 32 bits for the read/write data.
 * The address bus is always output from the PS, whereas the data bus is bidirectional
 * (PS input when reading, PS output when writing).
 *
 * This derived class uses Linux mmap for direct access to the GPIO registers.
 */

#ifndef FPGAV3_EMIO_MMAP_H
#define FPGAV3_EMIO_MMAP_H

#include "fpgav3_emio.h"

class EMIO_Interface_Mmap : public EMIO_Interface
{
    int fd;
    void *mmap_region;

public:

    EMIO_Interface_Mmap();

    ~EMIO_Interface_Mmap();

    bool IsOK() const
    { return (mmap_region != 0); }

    void SetEventMode(bool newState);

    bool ReadQuadlet(uint16_t addr, uint32_t &data);

    bool WriteQuadlet(uint16_t addr, uint32_t data);

    bool ReadBlock(uint16_t addr, uint32_t *data, unsigned int nBytes);

    bool WriteBlock(uint16_t addr, const uint32_t *data, unsigned int nBytes);

protected:

    void *mmap_init(uint32_t base_addr, uint16_t size);

    bool Init();

    uint32_t RegisterRead(uint32_t reg_addr);

    void RegisterWrite(uint32_t reg_addr, uint32_t reg_data);

    bool WaitOpDone(const char *opType, unsigned int num, bool state = true);

};

#endif // FPGAV3_EMIO_MMAP_H
