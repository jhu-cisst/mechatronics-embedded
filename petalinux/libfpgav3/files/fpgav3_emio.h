/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-    */
/* ex: set filetype=cpp softtabstop=4 shiftwidth=4 tabstop=4 cindent expandtab: */

/*
 * fpgav3_emio (Linux library)
 *
 * This library provides an interface from the Zynq PS to the read and write buses
 * on the FPGA (PL), via the EMIO bits. The read and write buses each consist of
 * a 16-bit address bus and a 32-bit data bus. The EMIO interface uses the same
 * 16 bits for the read/write address and the same 32 bits for the read/write data.
 * The address bus is always output from the PS, whereas the data bus is bidirectional
 * (PS input when reading, PS output when writing).
 */

#ifndef FPGAV3_EMIO_H
#define FPGAV3_EMIO_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// Forward declaration
struct EMIO_Info;

// EMIO_Init
//
//   Initializes EMIO to provide an interface to the internal read and write buses
//   on the FPGA. Returns 0 (null pointer) on failure.
struct EMIO_Info *EMIO_Init();

// Release the resources allocated by EMIO_Init
void EMIO_Release(struct EMIO_Info *info);

// Get/Set verbose flag (true -> prints messages when waiting for FPGA I/O to complete)
bool EMIO_GetVerbose(struct EMIO_Info *info);
void EMIO_SetVerbose(struct EMIO_Info *info, bool newState);

// EMIO_ReadQuadlet
//   Reads a quadlet (32-bit register) from the FPGA.
// Parameters:
//     addr  16-bit register address
//     data  pointer to location for storing 32-bit data
// Returns:  true if success
bool EMIO_ReadQuadlet(struct EMIO_Info *info, uint16_t addr, uint32_t *data);

// EMIO_WriteQuadlet
//   Writes a quadlet (32-bit register) to the FPGA.
// Parameters:
//     addr  16-bit register address
//     data  32-bit value to write
// Returns:  true if success
bool EMIO_WriteQuadlet(struct EMIO_Info *info, uint16_t addr, uint32_t data);

// EMIO_ReadBlock
//   Reads a block of 32-bit data from the FPGA. This implementation makes multiple
//   calls to EMIO_ReadQuadlet.
// Parameters:
//     addr   16-bit register address
//     data   pointer to location for storing 32-bit data
//     nBytes number of bytes to read (rounds up to multiple of 4)
// Returns:  true if success
bool EMIO_ReadBlock(struct EMIO_Info *info, uint16_t addr, uint32_t *data, unsigned int nBytes);

// EMIO_WriteBlock
//   Write a block of 32-bit data to the FPGA. This temporary implementation makes multiple
//   calls to EMIO_WriteQuadlet, which does not issue all block write signals, such as
//   blk_wstart and blk_wen.
// Parameters:
//     addr   16-bit register address
//     data   pointer to location that contains 32-bit data
//     nBytes number of bytes to write (rounds up to multiple of 4)
// Returns:  true if success
bool EMIO_WriteBlock(struct EMIO_Info *info, uint16_t addr, uint32_t *data, unsigned int nBytes);

// EMIO_WritePromData
//   Writes the specified bytes to the PROM registers on the FPGA
//   (this is intended to be used to copy the FPGA S/N to the PROM).
// Parameters:
//     data    pointer to data
//     nBytes  number of data bytes (will be rounded up to multiple of 4)
// Returns:    true if success
bool EMIO_WritePromData(struct EMIO_Info *info, char *data, unsigned int nBytes);

#ifdef __cplusplus
} /* extern "C" */
#endif

#ifdef __cplusplus
class EMIO_Interface
{

    EMIO_Info *info;

public:

    EMIO_Interface()
    { info = EMIO_Init(); }

    ~EMIO_Interface()
    { EMIO_Release(info); }

    bool GetVerbose()
    { return EMIO_GetVerbose(info); }

    void SetVerbose(bool newState)
    { EMIO_SetVerbose(info, newState); }

    bool ReadQuadlet(uint16_t addr, uint32_t &data)
    { return EMIO_ReadQuadlet(info, addr, &data); }

    bool WriteQuadlet(uint16_t addr, uint32_t data)
    { return EMIO_WriteQuadlet(info, addr, data); }

    bool WritePromData(char *data, unsigned int nBytes)
    { return EMIO_WritePromData(info, data, nBytes); }

};
#endif

#endif // FPGAV3_EMIO_H
