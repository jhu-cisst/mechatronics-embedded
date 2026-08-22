/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-    */
/* ex: set filetype=cpp softtabstop=4 shiftwidth=4 tabstop=4 cindent expandtab: */

#ifndef FPGAV3_QSPI_H
#define FPGAV3_QSPI_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// Checks QSPI Flash and programs QE bit if needed (e.g., for W25Q128JV-M)
bool QSPI_Configure();

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif  // FPGAV3_QSPI_H
