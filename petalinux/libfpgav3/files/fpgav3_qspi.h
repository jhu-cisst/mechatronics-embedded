/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-    */
/* ex: set filetype=cpp softtabstop=4 shiftwidth=4 tabstop=4 cindent expandtab: */

#ifndef FPGAV3_QSPI_H
#define FPGAV3_QSPI_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// Serial number format is 1234-56 or 1234-567, so maximum length (including null
// termination) is 9 bytes.
const size_t FPGA_SN_SIZE = 9;

// GetFpgaSerialNumber
//
//   This function reads the FPGA serial number from teh QSPI flash partition.

void GetFpgaSerialNumber(char *sn);

// ProgramFpgaSerialNumber
//
//   This function writes the FPGA serial number to QSPI flash partition /dev/mtd4.

bool ProgramFpgaSerialNumber(const char *sn);

// ProgramFlash
//
//   This function writes the specified file (fileName) to the specified QSPI
//   flash partition (devName). To avoid unecessary erase/write cycles, it
//   checks (sector-by-sector) whether the file contents are already present
//   in the flash partition.

bool ProgramFlash(const char *fileName, const char *devName);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif  // FPGAV3_QSPI_H
