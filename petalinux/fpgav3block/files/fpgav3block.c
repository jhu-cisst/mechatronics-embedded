/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-    */
/* ex: set filetype=cpp softtabstop=4 shiftwidth=4 tabstop=4 cindent expandtab: */

/*
 * fpgav3block (and fpgav3quad)
 *
 * Application to access FPGA registers via EMIO bus, similar to block1394.c
 * (mechatronics-software). Note that renaming the executable to fpgav3quad will
 * cause it to assume quadlet read/write.
 */

#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <byteswap.h>
#include <string.h>
#include <fpgav3_emio.h>

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
    bool useEvents = false;
    unsigned int timingMode = 0;

    j = 0;
    num = 1;
    args_found = 0;
    for (i = 1; i < argc; i++) {
        if (argv[i][0] == '-') {
            if (argv[i][1] == 'v') {
                isVerbose = true;
            }
            else if (argv[i][1] == 't') {
                if (argv[i][2]) timingMode = argv[i][2]-'0';
            }
            else if (argv[i][1] == 'e') {
                useEvents = true;
            }
        }
        else {
            if (args_found == 0)
                addr = strtoul(argv[i], 0, 16);
            else if ((args_found == 1) && (isQuad))
                data1 = strtoul(argv[i], 0, 16);
            else if ((args_found == 1) && (!isQuad)) {
                num = strtoul(argv[i], 0, 10);
                /* Allocate data array, initializing contents to 0 */
                data = (uint32_t *) calloc(num, sizeof(uint32_t));
                if (!data) {
                    printf("Failed to allocate memory for %d quadlets", num);
                    return -1;
                }
            }
            else if (!isQuad && (j < num))
                data[j++] = bswap_32(strtoul(argv[i], 0, 16));
            else
                printf("Warning: extra parameter: %s\n", argv[i]);

            args_found++;
        }
    }

    if (args_found < 1) {
        if (isQuad)
            printf("Usage: %s [-v] [-t<n>] [-e]  <address in hex> [value to write in hex]\n", argv[0]);
        else
            printf("Usage: %s [-v]  <address in hex> <number of quadlets> [write data quadlets in hex]\n", argv[0]);
        printf("       where -v is for verbose output\n");
        printf("             -e is to use events instead of polling\n");
        printf("             -tn is for timing measurement, n = 0 (no timing), 1 (total time only), 2+ (all timing)\n");
        return 0;
    }

    struct EMIO_Info *info = EMIO_Init();
    if (info == 0) {
        printf("Error initializing EMIO bus interface\n");
        return -1;
    }

    EMIO_SetVerbose(info, isVerbose);
    EMIO_SetEventMode(info, useEvents);
    EMIO_SetTimingMode(info, timingMode);

    if (isVerbose)
        printf("EMIO bus interface version %d\n", EMIO_GetVersion(info));

    // Determine whether to read or write based on args_found
    if ((isQuad && (args_found == 1)) ||
        (!isQuad && (args_found <= 2)))
    {
        // Read the data and print out the values (if no error)
        if (num == 1) {
            if (EMIO_ReadQuadlet(info, addr, &data1))
                printf("0x%08X\n", data1);
        }
        else if (num > 1) {
            if (EMIO_ReadBlock(info, addr, data, 4*num)) {
                for (j = 0; j < num; j++)
                    printf("0x%08X\n", bswap_32(data[j]));
            }
        }
    }
    else {
        // write the data
        bool ret = false;
        if (num == 1) {
            if (!isQuad) data1 = data[0];
            ret = EMIO_WriteQuadlet(info, addr, data1);
        }
        else if (num > 1)
            ret = EMIO_WriteBlock(info, addr, data, 4*num);
        if (!ret)
            printf("Write failed\n");
    }

    EMIO_Release(info);
    free(data);
    return 0;
}
