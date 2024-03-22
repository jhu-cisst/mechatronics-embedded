/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-    */
/* ex: set filetype=cpp softtabstop=4 shiftwidth=4 tabstop=4 cindent expandtab: */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <mtd/mtd-user.h>
#include "fpgav3_qspi.h"

// Format: FPGA 1234-56 (12 bytes) or FPGA 1234-567 (13 bytes).
// Note that on PROM, the string is terminated by 0xff because the sector
// is first erased (all bytes set to 0xff) before the string is written.
// Maximum length of S/N is 9 bytes, including null termination.

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

// Program QSPI flash
//
//   This function writes the specified file (fileName) to the specified QSPI
//   flash partition (devName). To avoid unecessary erase/write cycles, it
//   checks (sector-by-sector) whether the file contents are already present
//   in the flash partition.

bool ProgramFlash(const char *fileName, const char *devName)
{
    int fdFile = open(fileName, O_RDONLY);
    if (fdFile < 0) {
        printf("ProgramFlash: cannot open file %s\n", fileName);
        return false;
    }

    int fdFlash = open(devName, O_RDWR);
    if (fdFlash < 0) {
        printf("ProgramFlash: cannot open QSPI flash device %s\n", devName);
        close(fdFile);
        return false;
    }

    // Get flash info
    mtd_info_t mtd_info;
    ioctl(fdFlash, MEMGETINFO, &mtd_info);

    // Allocate buffers to hold contents of a sector (mtd_info.erasesize)
    char *fileBuf = (char *) malloc(mtd_info.erasesize);
    char *devBuf  = (char *) malloc(mtd_info.erasesize);
    if (!fileBuf || !devBuf) {
        printf("ProgramFlash: failed to allocate buffer of size %d\n", mtd_info.erasesize);
        free(fileBuf);
        free(devBuf);
        close(fdFile);
        close(fdFlash);
        return false;
    }

    // Get file size
    struct stat src_stat;
    fstat(fdFile, &src_stat);

    // Now, loop through number of sectors and check whether data in sector needs
    // to be updated.
    unsigned int numSame = 0;
    unsigned int numDiff = 0;
    erase_info_t ei;
    ei.length = mtd_info.erasesize;
    for (size_t nBytes = 0; nBytes < src_stat.st_size; nBytes += mtd_info.erasesize) {
        size_t bytesLeft = src_stat.st_size - nBytes;
        size_t nb = (bytesLeft < mtd_info.erasesize) ? bytesLeft : mtd_info.erasesize;
        read(fdFile, fileBuf, nb);
        read(fdFlash, devBuf, nb);
        if (memcmp(fileBuf, devBuf, nb) == 0) {
            numSame++;
        }
        else {
            numDiff++;
            // Back up in device file
            lseek(fdFlash, -nb, SEEK_CUR);
            // Erase sector
            ei.start = nBytes;
            ioctl(fdFlash, MEMUNLOCK, &ei);
            ioctl(fdFlash, MEMERASE, &ei);
            // write new contents
            write(fdFlash, fileBuf, nb);
        }
    }
    if (numDiff == 0) {
        printf("ProgramFlash:  %s already present in %s\n", fileName, devName);
    }
    else {
        printf("ProgramFlash:  updated %s in %s (%d of %d sectors)\n", fileName,
               devName, numDiff, (numSame+numDiff));
    }
    free(fileBuf);
    free(devBuf);
    close(fdFile);
    close(fdFlash);
    return true;
}
