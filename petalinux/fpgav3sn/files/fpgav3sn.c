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
#include <sys/ioctl.h>
#include <mtd/mtd-user.h>

// GetFpgaSerialNumber is duplicated in fpgav3init.c and fpgav3sn.c.
// Eventually, this function (and others) can be moved to a shared library.

// Format: FPGA 1234-56 (12 bytes) or FPGA 1234-567 (13 bytes).
// Note that on PROM, the string is terminated by 0xff because the sector
// is first erased (all bytes set to 0xff) before the string is written.
// Maximum length of S/N is 9 bytes, including null termination.
const size_t FPGA_SN_SIZE = 9;

void GetFpgaSerialNumber(char *sn)
{
    sn[0] = 0;   // Initialize to empty string

    int fd = open("/dev/mtd4ro", O_RDONLY);
    if (fd < 0) {
        printf("GetFpgaSerialNumber: cannot open QSPI flash device\n");
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

bool ProgramFpgaSerialNumber(const char *sn)
{
    size_t sn_len = strlen(sn);
    if (sn_len >= FPGA_SN_SIZE) {
        printf("ProgramFpgaSerialNumber: S/N too long (limit = %d characters)\n", FPGA_SN_SIZE-1);
        return false;
    }

    int fd = open("/dev/mtd4", O_WRONLY);
    if (fd < 0) {
        printf("ProgramFpgaSerialNumber: cannot open QSPI flash device\n");
        return false;
    }

    // Get flash info
    mtd_info_t mtd_info;
    ioctl(fd, MEMGETINFO, &mtd_info);
    // Erase first sector
    erase_info_t ei;
    ei.start = 0;
    ei.length = mtd_info.erasesize;
    ioctl(fd, MEMUNLOCK, &ei);
    ioctl(fd, MEMERASE, &ei);

    int n = write(fd, "FPGA ", 5);
    if (n != 5) {
        printf("ProgramFpgaSerialNumber: error writing FPGA to QSPI flash device\n");
        close(fd);
        return false;
    }
    n = write(fd, sn, sn_len);
    if (n != sn_len) {
        printf("ProgramFpgaSerialNumber: error writing S/N %s to QSPI flash device\n", sn);
    }
    close(fd);
    return (n == sn_len);
}

int main(int argc, char **argv)
{
    if (argc == 1) {
        // No arguments: read serial number
        char fpga_sn[FPGA_SN_SIZE];
        GetFpgaSerialNumber(fpga_sn);
        if (fpga_sn[0])
            printf("%s\n", fpga_sn);
    }
    else if ((argc > 2) && (argv[1][0] == '-') && (argv[1][1] == 'p')) {
        // -p sn (2 arguments): program serial number
        if (!ProgramFpgaSerialNumber(argv[2]))
            return -1;
    }
    else {
        // Anything else: print help information
        printf("Syntax: fpgav3sn [-p sn]\n");
        printf("  Read or program (-p option) FPGA V3 serial number\n");
    }
    return 0;
}
