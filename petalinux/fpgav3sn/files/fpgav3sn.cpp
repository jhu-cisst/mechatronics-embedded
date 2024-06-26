/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-    */
/* ex: set filetype=cpp softtabstop=4 shiftwidth=4 tabstop=4 cindent expandtab: */

/*
 * Application to query/program FPGA V3 serial number in QSPI partition
 */

#include <iostream>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <fpgav3_qspi.h>

int main(int argc, char **argv)
{
    if (argc == 1) {
        // No arguments: read serial number
        char fpga_sn[FPGA_SN_SIZE];
        GetFpgaSerialNumber(fpga_sn);
        if (fpga_sn[0])
            std::cout << fpga_sn << std::endl;
    }
    else if ((argc > 2) && (argv[1][0] == '-') && (argv[1][1] == 'p')) {
        // -p sn (2 arguments): program serial number
        if (!ProgramFpgaSerialNumber(argv[2]))
            return -1;
    }
    else {
        // Anything else: print help information
        std::cout << "Syntax: fpgav3sn [-p sn]" << std::endl
                  << "  Read or program (-p option) FPGA V3 serial number" << std::endl;
    }
    return 0;
}
