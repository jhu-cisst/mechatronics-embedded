/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-    */
/* ex: set filetype=cpp softtabstop=4 shiftwidth=4 tabstop=4 cindent expandtab: */

/*
 * fpgav3_emio_gpiod (Linux library)
 *
 * This library provides an interface from the Zynq PS to the read and write buses
 * on the FPGA (PL), via the EMIO bits. The read and write buses each consist of
 * a 16-bit address bus and a 32-bit data bus. The EMIO interface uses the same
 * 16 bits for the read/write address and the same 32 bits for the read/write data.
 * The address bus is always output from the PS, whereas the data bus is bidirectional
 * (PS input when reading, PS output when writing).
 *
 * This derived class uses the Linux gpiod driver.
 */

#ifndef FPGAV3_EMIO_GPIOD_H
#define FPGAV3_EMIO_GPIOD_H

#include "fpgav3_emio.h"

// Return GPIOD library version string
const char *EMIO_gpiod_version_string();

// Forward declaration
struct gpiod_info;

class EMIO_Interface_Gpiod : public EMIO_Interface
{

    gpiod_info *info;

public:

    EMIO_Interface_Gpiod();

    ~EMIO_Interface_Gpiod();

    bool IsOK() const
    { return (info != 0); }

    void SetEventMode(bool newState);

    bool ReadQuadlet(uint16_t addr, uint32_t &data);

    bool WriteQuadlet(uint16_t addr, uint32_t data);

    bool ReadBlock(uint16_t addr, uint32_t *data, unsigned int nBytes);

    bool WriteBlock(uint16_t addr, const uint32_t *data, unsigned int nBytes);

protected:

    bool Init();

    bool WaitOpDone(const char *opType, unsigned int num);

};

#endif // FPGAV3_EMIO_GPIOD_H
