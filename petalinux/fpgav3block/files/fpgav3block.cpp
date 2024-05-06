/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-    */
/* ex: set filetype=cpp softtabstop=4 shiftwidth=4 tabstop=4 cindent expandtab: */

/*
 * fpgav3block (and fpgav3quad)
 *
 * Application to access FPGA registers via EMIO bus, similar to block1394.c
 * (mechatronics-software). Note that renaming the executable to fpgav3quad will
 * cause it to assume quadlet read/write.
 */

#include <iostream>
#include <iomanip>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <byteswap.h>
#include <fpgav3_emio_gpiod.h>
#include <fpgav3_emio_mmap.h>
#include <fpgav3_lib.h>

int main(int argc, char **argv)
{
    int i, j;
    int args_found;
    uint16_t addr;
    int num;
    uint32_t data1;
    uint32_t *data = 0;

    bool isQuad = (strstr(argv[0], "fpgav3quad") != 0);
    bool isVerbose = false;
    bool useGpiod = false;
    unsigned int eventMode = 2;
    unsigned int timingMode = 0;

    j = 0;
    num = 1;
    args_found = 0;
    for (i = 1; i < argc; i++) {
        if (argv[i][0] == '-') {
            if (argv[i][1] == 'v') {
                isVerbose = true;
            }
            else if (argv[i][1] == 'g') {
                useGpiod = true;
            }
            else if (argv[i][1] == 't') {
                if (argv[i][2]) timingMode = argv[i][2]-'0';
            }
            else if (argv[i][1] == 'e') {
                if (argv[i][2]) eventMode = argv[i][2]-'0';
            }
        }
        else {
            if (args_found == 0)
                addr = strtoul(argv[i], 0, 16);
            else if ((args_found == 1) && (isQuad))
                data1 = strtoul(argv[i], 0, 16);
            else if ((args_found == 1) && (!isQuad)) {
                num = strtoul(argv[i], 0, 10);
                // Allocate data array, initializing contents to 0
                data = new uint32_t[num];
                memset(data, 0, num*sizeof(uint32_t));
            }
            else if (!isQuad && (j < num))
                data[j++] = bswap_32(strtoul(argv[i], 0, 16));
            else
                std::cout << "Warning: extra parameter: " << argv[i] << std::endl;

            args_found++;
        }
    }

    if (args_found < 1) {
        std::cout << "Usage: " << argv[0] << " [-v] [-g] [-e<n>] [-t<n>] <address in hex> ";
        if (isQuad)
            std::cout << "[value to write in hex]" << std::endl;
        else
            std::cout << "<address in hex> <number of quadlets> [write data quadlets in hex]" << std::endl;
        std::cout << "       where -v is for verbose output" << std::endl
                  << "             -g specifies to use gpiod interface" << std::endl
                  << "             -e<n> is to use polling (0) or events (1)" << std::endl
                  << "             -t<n> is for timing measurement: 0 (no timing), 1 (total time only), 2+ (all timing)"
                  << std::endl;
        return 0;
    }

    EMIO_Interface *emio;
    if (useGpiod) {
        if (isVerbose)
            std::cout << "Using EMIO gpiod interface" << std::endl;
        emio = new EMIO_Interface_Gpiod;
    }
    else {
        if (isVerbose)
            std::cout << "Using EMIO mmap interface" << std::endl;
        emio = new EMIO_Interface_Mmap;
    }
    if (!emio->IsOK()) {
        std::cout << "Error initializing EMIO bus interface" << std::endl;
        return -1;
    }

    emio->SetVerbose(isVerbose);

    bool defaultEventMode = true;
    if (eventMode == 0) {
        emio->SetEventMode(false);
        defaultEventMode = false;
    }
    else if (eventMode == 1) {
        emio->SetEventMode(true);
        defaultEventMode = false;
    }

    emio->SetTimingMode(timingMode);

    if (isVerbose) {
        print_fpgav3_versions(std::cout);
        if (useGpiod)
            std::cout << "GPIOD library version " << EMIO_gpiod_version_string() << std::endl;
        std::cout << "EMIO bus interface version " << emio->GetVersion() << std::endl;
        if (emio->GetEventMode())
            std::cout << "Configured to use events";
        else
            std::cout << "Configured to use polling";
        if (defaultEventMode) std::cout << " (default)";
        std::cout << std::endl;
        std::cout << "EMIO timeout is " << emio->GetTimeout_us() << " us" << std::endl;
    }

    // Determine whether to read or write based on args_found
    if ((isQuad && (args_found == 1)) ||
        (!isQuad && (args_found <= 2)))
    {
        // Read the data and print out the values (if no error)
        if (num == 1) {
            if (emio->ReadQuadlet(addr, data1))
                std::cout << "0x" << std::hex << std::setw(8) << std::setfill('0') << data1 << std::endl;
        }
        else if (num > 1) {
            if (emio->ReadBlock(addr, data, 4*num)) {
                for (j = 0; j < num; j++)
                    std::cout << "0x" << std::hex << std::setw(8) << std::setfill('0') << bswap_32(data[j]) << std::endl;
            }
        }
    }
    else {
        // write the data
        bool ret = false;
        if (num == 1) {
            if (!isQuad) data1 = data[0];
            ret = emio->WriteQuadlet(addr, data1);
        }
        else if (num > 1)
            ret = emio->WriteBlock(addr, data, 4*num);
        if (!ret)
            std::cout << "Write failed" << std::endl;
    }

    delete [] data;
    delete emio;
    return 0;
}
