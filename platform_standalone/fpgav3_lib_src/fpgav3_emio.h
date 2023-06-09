/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-    */
/* ex: set filetype=cpp softtabstop=4 shiftwidth=4 tabstop=4 cindent expandtab: */

/*
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

// EMIO_Init
//
//   Initializes EMIO to provide an interface to the internal read and write buses
//   on the FPGA.
void EMIO_Init();

// EMIO_ReadQuadlet
//   Reads a quadlet (32-bit register) from the FPGA.
// Parameters:
//     addr  16-bit register address
//     data  pointer to location for storing 32-bit data
// Returns:  true if success
bool EMIO_ReadQuadlet(uint16_t addr, uint32_t *data);

// EMIO_WriteQuadlet
//   Writes a quadlet (32-bit register) to the FPGA.
// Parameters:
//     addr  16-bit register address
//     data  32-bit value to write
// Returns:  true if success
bool EMIO_WriteQuadlet(uint16_t addr, uint32_t data);

// EMIO_WritePromData
//   Writes the specified bytes to the PROM registers on the FPGA
//   (this is intended to be used to copy the FPGA S/N to the PROM).
// Parameters:
//     data    pointer to data
//     nBytes  number of data bytes (will be rounded up to multiple of 4)
// Returns:    true if success
bool EMIO_WritePromData(char *data, unsigned int nBytes);

#endif // FPGAV3_EMIO_H
