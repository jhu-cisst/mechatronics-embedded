/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-    */
/* ex: set filetype=cpp softtabstop=4 shiftwidth=4 tabstop=4 cindent expandtab: */

/*
 * Application to query/program FPGA V3 serial number in QSPI partition
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <fcntl.h>

// Format: FPGA 1234-56 (12 bytes) or FPGA 1234-567 (13 bytes).
// Note that on PROM, the string is terminated by 0xff because the sector
// is first erased (all bytes set to 0xff) before the string is written.
// Maximum length of S/N is 9 bytes, including null termination.
const size_t FPGA_SN_SIZE = 9;

void GetFPGASerialNumber(char *sn)
{
    sn[0] = 0;   // Initialize to empty string

    int fd = open("/dev/mtd4", O_RDONLY);
    if (fd < 0) {
        printf("GetFPGASerialNumber: cannot open QSPI flash device\n");
        return;
    }

    char data[5];
    int n = read(fd, data, 5);
    if ((n == 5) && (strncmp(data, "FPGA ", 5) == 0)) {
        read(fd, sn, FPGA_SN_SIZE-1);
        char *p = strchr(sn, 0xff);
        if (p)
            *p = 0;                  // Null terminate at first 0xff
        else
            sn[FPGA_SN_SIZE-1] = 0;  // or at end of string
    }
    close(fd);
}

bool ProgramFPGASerialNumber(const char *sn)
{
    int fd = open("/dev/mtd4", O_WRONLY);
    if (fd < 0) {
        printf("ProgramFPGASerialNumber: cannot open QSPI flash device\n");
        return false;
    }
    int n = write(fd, "FPGA ", 5);
    if (n != 5) {
        printf("ProgramFPGASerialNumber: error writing FPGA to QSPI flash device\n");
        return false;
    }
    size_t sn_len = strlen(sn);
    if (sn_len >= FPGA_SN_SIZE) {
        printf("ProgramFPGASerialNumber: S/N too long (limit = %ld characters)\n", FPGA_SN_SIZE-1);
        return false;
    }
    n = write(fd, sn, sn_len);
    if (n != sn_len) {
        printf("ProgramFPGASerialNumber: error writing S/N %s to QSPI flash device\n", sn);
    }
    close(fd);
    return (n == sn_len);
}

int main(int argc, char **argv)
{
    if (argc == 1) {
        // No arguments: read serial number
        char fpga_sn[FPGA_SN_SIZE];
        GetFPGASerialNumber(fpga_sn);
        if (fpga_sn[0])
            printf("%s\n", fpga_sn);
    }
    else if ((argc > 2) && (argv[1][0] == '-') && (argv[1][1] == 'p')) {
        // -p sn (2 arguments): program serial number
        if (!ProgramFPGASerialNumber(argv[2]))
            return -1;
    }
    else {
        // Anything else: print help information
        printf("Syntax: fpgav3sn [-p sn]\n");
        printf("  Read or program (-p option) FPGA V3 serial number\n");
    }
    return 0;
}
