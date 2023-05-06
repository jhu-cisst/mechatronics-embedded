/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-    */
/* ex: set filetype=cpp softtabstop=4 shiftwidth=4 tabstop=4 cindent expandtab: */

/*
 * Application to load firmware to FPGA V3.
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <fcntl.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/sendfile.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <net/if.h>
#include <net/if_arp.h>
#include <arpa/inet.h>

// Some hard-coded values
const unsigned int GPIO_BASE = 906;
const unsigned int INDEX_EMIO_START = 54;
const unsigned int INDEX_EMIO_END = 118;

// Detected board type
enum BoardType { BOARD_UNKNOWN, BOARD_NONE, BOARD_QLA, BOARD_DQLA, BOARD_DRAC };
char *BoardName[5] = { "Unknown", "None", "QLA", "DQLA", "DRAC" };
char *FirmwareName[5] = { "", "", "FPGA1394V3-QLA", "FPGA1394V3-DQLA", "FPGA1394V3-DRAC" };

bool EMIO_Init(bool do_export)
{
    int fd;
    if (do_export)
        fd = open("/sys/class/gpio/export", O_WRONLY);
    else
        fd = open("/sys/class/gpio/unexport", O_WRONLY);

    if (fd < 0) {
        printf("Cannot open GPIO export/unexport\n");
        return false;
    }

    char str[5];
    for (unsigned int idx = INDEX_EMIO_START; idx < INDEX_EMIO_END; idx++) {
        sprintf(str, "%d", GPIO_BASE+idx);
        int nWrite = write(fd, str, strlen(str)+1);
        if (nWrite != strlen(str)+1)
            printf("Warning: error writing to GPIO export, return code %d\n", nWrite);
    }
    close(fd);

    return true;
}

bool EMIO_Read(uint32_t *upper, uint32_t *lower)
{
    char fname[128];
    char str[2];
    for (unsigned int idx = INDEX_EMIO_END-1; idx >= INDEX_EMIO_START; idx--) {
        sprintf(fname, "/sys/class/gpio/gpio%d/value", GPIO_BASE+idx);
        int fd = open(fname, O_RDWR);
        if (fd < 0) {
            printf("Cannot open GPIO to read %d\n", idx);
            return false;
        }
        int nRead = read(fd, str, sizeof(str));
        if (nRead != sizeof(str))
            printf("Warning: error reading GPIO, return code %d\n", nRead);
        close(fd);
        unsigned int val = str[0]-'0';
        if ((val != 0) && (val != 1)) {
            printf("Invalid value %c (%d) from GPIO %d\n", str[0], (int)str[0], idx);
            return false;
        }
        if (idx < (INDEX_EMIO_START+32))
            *lower = (*lower<<1)|val;
        else
            *upper = (*upper<<1)|val;
    }
    return true;
}

// CopyFile from srcDir to destDir.
// Note that srcDir and destDir should not have a trailing '/' character.
bool CopyFile(const char *filename, const char *srcDir, const char *destDir)
{
    size_t srcNameSize = strlen(srcDir)+strlen(filename)+2;
    char *srcFile = (char *) malloc(srcNameSize);
    sprintf(srcFile, "%s/%s", srcDir, filename);
    size_t destNameSize = strlen(destDir)+strlen(filename)+2;
    int src_fd = open(srcFile, O_RDONLY);
    if (src_fd == -1) {
        printf("CopyFile: could not open source file %s\n", srcFile);
        free(srcFile);
        return false;
    }

    char *destFile = (char *) malloc(destNameSize);
    sprintf(destFile, "%s/%s", destDir, filename);
    mode_t mode = S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH;
    int dest_fd = open(destFile, O_WRONLY|O_CREAT|O_TRUNC, mode);
    if (dest_fd == -1) {
        printf("CopyFile: could not open destination file %s\n", destFile);
        close(src_fd);
        free(srcFile);
        free(destFile);
        return false;
    }
    struct stat src_stat;
    fstat(src_fd, &src_stat);
    printf("Copying %s from %s to %s (%ld bytes)\n", filename, srcDir, destDir, src_stat.st_size);
    ssize_t nwritten = sendfile(dest_fd, src_fd, NULL, src_stat.st_size);
    if (nwritten != src_stat.st_size) {
        printf("Warning: wrote %d bytes\n", nwritten);
    }
    close(src_fd);
    close(dest_fd);
    free(srcFile);
    free(destFile);
    return true;
}

bool ConvertBitstream(const char *firmwareName, const char *fileDir)
{
    char bifFile[40];
    char bitFile[40];
    char sysCmd[128];

    sprintf(bifFile, "%s/%s.bif", fileDir, firmwareName);
    FILE *fp = fopen(bifFile, "w");
    if (fp == NULL) {
        printf("Error opening %s\n", bifFile);
        return false;
    }

    sprintf(bitFile, "%s/%s.bit", fileDir, firmwareName);
    fprintf(fp, "all:\n{\n    %s\n}\n", bitFile);
    fclose(fp);

    sprintf(sysCmd, "bootgen -image %s -arch zynq -w on -process_bitstream bin", bifFile);
    int ret = system(sysCmd);
    if (ret != 0)
        printf("bootgen failed, return code %d\n", ret);
    return (ret == 0);
}

bool FpgaLoad(const char *binFile)
{
    int fd;
    int nWrite;

    // Write '0' to flags
    fd = open("/sys/class/fpga_manager/fpga0/flags", O_WRONLY);
    if (fd == -1) {
        printf("FpgaLoad: could not open FPGA flags\n");
        return false;
    }
    char str[2] = { '0', 0 };
    nWrite = write(fd, str, sizeof(str));
    close(fd);
    if (nWrite != sizeof(str)) {
        printf("FpgaLoad: error writing to FPGA flags, return code %d\n", nWrite);
        return false;
    }

    // Open destination (FPGA) for writing
    fd = open("/sys/class/fpga_manager/fpga0/firmware", O_WRONLY);
    if (fd == -1) {
        printf("FpgaLoad: could not open FPGA firmware\n");
        return false;
    }
    nWrite = write(fd, binFile, strlen(binFile)+1);
    close(fd);
    if (nWrite != strlen(binFile)+1) {
        printf("FpgaLoad: error writing to FPGA firmware, return code %d\n", nWrite);
        return false;
    }

    return true;
}

bool ProgramFpga(const char *firmwareName)
{
    printf("Mounting SD card\n");
    if (mount("/dev/mmcblk0p1", "/media", "vfat", 0L, NULL) != 0) {
        printf("Failed to mount SD card\n");
        return false;
    }

    // Copy bit file from /media to /tmp
    char bitFile[32];
    sprintf(bitFile, "%s.bit", firmwareName);
    CopyFile(bitFile, "/media", "/tmp");

    // Unmount SD card from /media
    if (umount("/media")  != 0) {
        printf("Warning: failed to unmount SD card\n");
    }

    // Create bin file in /tmp
    printf("Converting bitstream %s\n", bitFile);
    if (!ConvertBitstream(firmwareName, "/tmp"))
        return false;

    // Create /lib/firmware if it does not already exist
    struct stat dir_stat;
    stat("/lib/firmware", &dir_stat);
    if (S_ISDIR(dir_stat.st_mode)) {
        printf("Directory /lib/firmware already exists\n");
    }
    else {
        printf("Creating directory /lib/firmware\n");
        if (mkdir("/lib/firmware", S_IRWXU|S_IRGRP|S_IXGRP|S_IROTH|S_IXOTH) != 0) {
            printf("Failed to create directory\n");
            return false;
        }
    }

    // Copy bin file from /tmp to /lib/firmware (FPGA Manager requires firmware
    // to be in /lib/firmware)
    char binFile[36];
    sprintf(binFile, "%s.bin", bitFile);
    CopyFile(binFile, "/tmp", "/lib/firmware");

    printf("Loading bitstream to FPGA\n");
    return FpgaLoad(binFile);
}

// Set MAC and IP addresses for specified Ethernet adapter
bool SetMACandIP(const char *ethName, unsigned int ethNum, unsigned int board_id)
{
    struct ifreq ifr;

    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd == -1)
        return false;

    // Set MAC address
    // The first 3 bytes are the JHU LCSR CID
    strcpy(ifr.ifr_name, ethName);
    ifr.ifr_hwaddr.sa_data[0] = 0xFA;
    ifr.ifr_hwaddr.sa_data[1] = 0x61;
    ifr.ifr_hwaddr.sa_data[2] = 0x0E;
    ifr.ifr_hwaddr.sa_data[3] = 0x00;
    ifr.ifr_hwaddr.sa_data[4] = ethNum;
    ifr.ifr_hwaddr.sa_data[5] = board_id;
    ifr.ifr_hwaddr.sa_family = ARPHRD_ETHER;
    if (ioctl(sockfd, SIOCSIFHWADDR, &ifr) == -1)
        return false;

    // Set IP address
    char ip_addr[16];
    sprintf(ip_addr, "192.168.%d.%d", (10+ethNum), board_id);
    ifr.ifr_addr.sa_family = AF_INET;
    struct sockaddr_in* addr = (struct sockaddr_in*)&ifr.ifr_addr;
    inet_pton(AF_INET, ip_addr, &addr->sin_addr);
    return (ioctl(sockfd, SIOCSIFADDR, &ifr) != -1);
}

int main(int argc, char **argv)
{
    printf("*** FPGAV3 Initialization ***\n\n");

    // Initialize EMIO interface
    if (!EMIO_Init(true))
        return -1;

    // Read EMIO bits
    uint32_t reg_hw, reg_status;
    if (!EMIO_Read(&reg_hw, &reg_status))
        return -1;

    char hwStr[5];
    hwStr[0] = (reg_hw & 0xff000000) >> 24;
    hwStr[1] = (reg_hw & 0x00ff0000) >> 16;
    hwStr[2] = (reg_hw & 0x0000ff00) >> 8;
    hwStr[3] = (reg_hw & 0x000000ff);
    hwStr[4] = 0;
    if (strcmp(hwStr, "BCFG") != 0) {
        printf("fpgav3init: did not detect BCFG firmware, exiting\n");
        return -1;
    }
    printf("Hardware version: %s\n", hwStr);
    printf("Status reg: %08x\n", reg_status);
    unsigned int board_id = (reg_status&0x0f000000)>>24;
    // board_type bitmask: BOARD_NONE, BOARD_QLA, BOARD_DQLA, BOARD_DRAC
    unsigned int board_mask = (reg_status & 0x00f00000)>>20;
    bool isV30 = (reg_status&0x00080000);

    if (isV30)
        printf("FPGA V3.0 detected!\n");

    enum BoardType board_type;
    switch (board_mask) {
        case 8: board_type = BOARD_NONE;
                break;
        case 4: board_type = BOARD_QLA;
                break;
        case 2: board_type = BOARD_DQLA;
                break;
        case 1: board_type = BOARD_DRAC;
                break;
        default:
                board_type = BOARD_UNKNOWN;
    }
    printf("Board type: %s\n", BoardName[board_type]);

    printf("Board ID: %d\n", board_id);

    // Close EMIO interface
    if (!EMIO_Init(false))
        return -1;

    if (strlen(FirmwareName[board_type]) > 0) {
        ProgramFpga(FirmwareName[board_type]);
    }

    printf("\nSetting Ethernet MAC and IP addresses\n\n");
    if (!SetMACandIP("eth0", 0, board_id))
        printf("Failed to set MAC or IP address for eth0\n");
    if (!SetMACandIP("eth1", 1, board_id))
        printf("Failed to set MAC or IP address for eth1\n");

    printf("*** FPGAV3 Initialization Complete ***\n\n");
    return 0;
}
