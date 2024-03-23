/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-    */
/* ex: set filetype=cpp softtabstop=4 shiftwidth=4 tabstop=4 cindent expandtab: */

/*
 * Application to initialize FPGA V3
 *
 * This program is auto-run at startup and does the following:
 *   1) Reads FPGA serial number from QSPI
 *   2) Reads EMIO to determine board information
 *   3) Exports FPGAV3 environment variables (to shell)
 *   4) Loads correct firmware based on detected board type (QLA, DQLA, DRAC)
 *   5) Copies qspi-boot.bin to flash if different
 *   6) Sets the Ethernet MAC and IP addresses
 *   7) Copies FPGA serial number from QSPI to FPGA (via EMIO)
 */

#include <stdio.h>
#include <stdlib.h>
#include <iostream>
#include <iomanip>
#include <string>
#include <unistd.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/sendfile.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <net/if.h>
#include <net/if_arp.h>
#include <arpa/inet.h>
#include <fpgav3_emio_gpiod.h>
#include <fpgav3_qspi.h>

// Detected board type
enum BoardType { BOARD_UNKNOWN, BOARD_NONE, BOARD_QLA, BOARD_DQLA, BOARD_DRAC };
char *BoardName[5] = { "Unknown", "None", "QLA", "DQLA", "DRAC" };
char *FirmwareName[5] = { "", "", "FPGA1394V3-QLA", "FPGA1394V3-DQLA", "FPGA1394V3-DRAC" };

// CopyFile from srcDir to destDir.
// Note that srcDir and destDir should not have a trailing '/' character.
bool CopyFile(const std::string &filename, const std::string &srcDir, const std::string &destDir)
{
    std::string srcFile(srcDir + "/" + filename);
    int src_fd = open(srcFile.c_str(), O_RDONLY);
    if (src_fd == -1) {
        std::cout << "CopyFile: could not open source file " << srcFile << std::endl;
        return false;
    }

    std::string destFile(destDir + "/" + filename);
    mode_t mode = S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH;
    int dest_fd = open(destFile.c_str(), O_WRONLY|O_CREAT|O_TRUNC, mode);
    if (dest_fd == -1) {
        std::cout << "CopyFile: could not open destination file " << destFile << std::endl;
        close(src_fd);
        return false;
    }
    struct stat src_stat;
    fstat(src_fd, &src_stat);
    std::cout << "Copying " << filename << " from " << srcDir << " to " << destDir
              << " ( " << src_stat.st_size << " bytes)" << std::endl;
    ssize_t nwritten = sendfile(dest_fd, src_fd, NULL, src_stat.st_size);
    if (nwritten != src_stat.st_size) {
        std::cout << "Warning: wrote " << nwritten << " bytes" << std::endl;
    }
    close(src_fd);
    close(dest_fd);
    return true;
}

bool ConvertBitstream(const char *firmwareName, const std::string &fileDir)
{
    char bifFile[40];
    char bitFile[40];
    char sysCmd[128];

    sprintf(bifFile, "%s/%s.bif", fileDir.c_str(), firmwareName);
    FILE *fp = fopen(bifFile, "w");
    if (fp == NULL) {
        printf("Error opening %s\n", bifFile);
        return false;
    }

    sprintf(bitFile, "%s/%s.bit", fileDir.c_str(), firmwareName);
    fprintf(fp, "all:\n{\n    %s\n}\n", bitFile);
    fclose(fp);

    sprintf(sysCmd, "bootgen -image %s -arch zynq -w on -process_bitstream bin", bifFile);
    int ret = system(sysCmd);
    if (ret != 0)
        std::cout << "bootgen failed, return code " << ret << std::endl;
    return (ret == 0);
}

bool FpgaLoad(const std::string &binFile)
{
    int fd;
    int nWrite;

    // Write '0' to flags
    fd = open("/sys/class/fpga_manager/fpga0/flags", O_WRONLY);
    if (fd == -1) {
        std::cout << "FpgaLoad: could not open FPGA flags" << std::endl;
        return false;
    }
    char str[2] = { '0', 0 };
    nWrite = write(fd, str, sizeof(str));
    close(fd);
    if (nWrite != sizeof(str)) {
        std::cout << "FpgaLoad: error writing to FPGA flags, return code "<< nWrite << std::endl;
        return false;
    }

    // Open destination (FPGA) for writing
    fd = open("/sys/class/fpga_manager/fpga0/firmware", O_WRONLY);
    if (fd == -1) {
        std::cout << "FpgaLoad: could not open FPGA firmware" << std::endl;
        return false;
    }
    nWrite = write(fd, binFile.c_str(), binFile.size()+1);
    close(fd);
    if (nWrite != binFile.size()+1) {
        std::cout << "FpgaLoad: error writing to FPGA firmware, return code " << nWrite << std::endl;
        return false;
    }

    return true;
}

bool ProgramFpga(const char *firmwareName)
{
    // Copy bit file from /media to /tmp
    char bitFile[32];
    sprintf(bitFile, "%s.bit", firmwareName);
    if (!CopyFile(bitFile, "/media", "/tmp"))
        return false;

    // Create bin file in /tmp
    std::cout << "Converting bitstream " << bitFile << std::endl;
    if (!ConvertBitstream(firmwareName, "/tmp"))
        return false;

    // Create /lib/firmware if it does not already exist
    struct stat dir_stat;
    stat("/lib/firmware", &dir_stat);
    if (S_ISDIR(dir_stat.st_mode)) {
        std::cout << "Directory /lib/firmware already exists" << std::endl;
    }
    else {
        std::cout << "Creating directory /lib/firmware" << std::endl;
        if (mkdir("/lib/firmware", S_IRWXU|S_IRGRP|S_IXGRP|S_IROTH|S_IXOTH) != 0) {
            std::cout << "Failed to create directory" << std::endl;
            return false;
        }
    }

    // Copy bin file from /tmp to /lib/firmware (FPGA Manager requires firmware
    // to be in /lib/firmware)
    char binFile[36];
    sprintf(binFile, "%s.bin", bitFile);
    CopyFile(binFile, "/tmp", "/lib/firmware");

    std::cout << "Loading bitstream to FPGA" << std::endl;
    return FpgaLoad(binFile);
}

// Export FPGA information as shell variables
bool ExportFpgaInfo(char *fpga_ver, char *fpga_sn, char *board_type, unsigned int board_id)
{
    mode_t mode = S_IRWXU|S_IRGRP|S_IXGRP|S_IROTH|S_IXOTH;
    int fd = open("/etc/profile.d/fpgav3.sh", O_WRONLY|O_CREAT|O_TRUNC, mode);
    if (fd == -1) {
        std::cout << "ExportFpgaInfo: could not open /etc/profile.d/fpgav3.sh" << std::endl;
        return false;
    }
    char buf[64];
    sprintf(buf, "export FPGAV3_VER=%s\n", fpga_ver);
    write(fd, buf, strlen(buf)+1);
    if (fpga_sn[0]) {
        sprintf(buf, "export FPGAV3_SN=%s\n", fpga_sn);
        write(fd, buf, strlen(buf)+1);
    }
    sprintf(buf, "export FPGAV3_HW=%s\n", board_type);
    write(fd, buf, strlen(buf)+1);
    if (board_id < 16) {
        sprintf(buf, "export FPGAV3_ID=%d\n", board_id);
        write(fd, buf, strlen(buf)+1);
    }
    close(fd);
    return true;
}

// Set MAC and IP addresses for specified Ethernet adapter
bool SetMACandIP(const char *ethName, unsigned int ethNum, unsigned int board_id)
{
    struct ifreq ifr;

    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd == -1)
        return false;

    // Set interface name
    strcpy(ifr.ifr_name, ethName);

    // Get flags
    if (ioctl(sockfd, SIOCGIFFLAGS, &ifr) == -1)
        return false;

    // Disable interface (ifdown)
    if (ifr.ifr_flags & IFF_UP) {
        ifr.ifr_flags &= ~IFF_UP;
        if (ioctl(sockfd, SIOCSIFFLAGS, &ifr) == -1)
            return false;
    }

    // Set MAC address
    // The first 3 bytes are the JHU LCSR CID
    ifr.ifr_hwaddr.sa_data[0] = 0xFA;
    ifr.ifr_hwaddr.sa_data[1] = 0x61;
    ifr.ifr_hwaddr.sa_data[2] = 0x0E;
    ifr.ifr_hwaddr.sa_data[3] = 0x03;       // FPGA V3
    ifr.ifr_hwaddr.sa_data[4] = ethNum;     // 0 or 1
    ifr.ifr_hwaddr.sa_data[5] = board_id;   // Board id: 0-15
    ifr.ifr_hwaddr.sa_family = ARPHRD_ETHER;
    if (ioctl(sockfd, SIOCSIFHWADDR, &ifr) == -1)
        return false;

    // Set IP address
    char ip_addr[16];
    sprintf(ip_addr, "169.254.%d.%d", (10+ethNum), board_id);
    ifr.ifr_addr.sa_family = AF_INET;
    struct sockaddr_in* addr = (struct sockaddr_in*)&ifr.ifr_addr;
    inet_pton(AF_INET, ip_addr, &addr->sin_addr);
    if (ioctl(sockfd, SIOCSIFADDR, &ifr) == -1)
        return false;

    // Set netmask
    inet_pton(AF_INET, "255.255.0.0", &addr->sin_addr);
    if (ioctl(sockfd, SIOCSIFNETMASK, &ifr) == -1)
        return false;

    // Enable interface
    ifr.ifr_flags |= IFF_UP;
    if (ioctl(sockfd, SIOCSIFFLAGS, &ifr) == -1)
        return false;

    return true;
}

// CopyQspiToFpga: Copy bytes from QSPI flash to FPGA PROM registers. This is
//   used to copy the FPGA S/N (first 16 bytes).
// Parameters:
//    qspiDev   QSPI device name (e.g., "/dev/mtd4ro")
//    emio      EMIO_Interface object used to write to FPGA registers via EMIO
//    nbytes    Number of bytes to copy

bool CopyQspiToFpga(const std::string &qspiDev, EMIO_Interface *emio, uint16_t nbytes)
{
    int fd = open(qspiDev.c_str(), O_RDONLY);
    if (fd < 0) {
        std::cout << "CopyQspiToFpga: cannot open QSPI flash device " << qspiDev << std::endl;
        return false;
    }

    // Round up to nearest multiple of 4
    uint16_t nQuads = (nbytes+3)/4;
    nbytes = 4*nQuads;
    char *data = new char[nbytes];
    int n = read(fd, data, nbytes);
    close(fd);
    if (n != nbytes) {
        std::cout << "CopyQspiToFpga: attempted to read " << nbytes << " bytes, but read returned "
                  << n << std::endl;
        return false;
    }
    emio->WritePromData(data, nbytes);
    delete [] data;
    return true;
}

int main(int argc, char **argv)
{
    std::cout << "*** FPGAV3 Initialization ***" << std::endl << std::endl;

    // Get FPGA Serial Number
    char fpga_sn[FPGA_SN_SIZE];
    GetFpgaSerialNumber(fpga_sn);
    if (fpga_sn[0])
        std::cout << "FPGA S/N: " << fpga_sn << std::endl;

    uint32_t reg_hw, reg_status, reg_ethctrl;
    EMIO_Interface_Gpiod emio;
    if (!emio.IsOK())
        return -1;

    emio.ReadQuadlet(4, reg_hw);
    emio.ReadQuadlet(0, reg_status);

    char hwStr[5];
    hwStr[0] = (reg_hw & 0xff000000) >> 24;
    hwStr[1] = (reg_hw & 0x00ff0000) >> 16;
    hwStr[2] = (reg_hw & 0x0000ff00) >> 8;
    hwStr[3] = (reg_hw & 0x000000ff);
    hwStr[4] = 0;
    if (strcmp(hwStr, "BCFG") != 0) {
        std::cout << "fpgav3init: did not detect BCFG firmware, exiting" << std::endl;
        return -1;
    }
    std::cout << "Hardware version: " << hwStr << std::endl;
    std::cout << "Status reg: " << std::hex << std::setw(8) << std::setfill('0')
              << reg_status << std::dec << std::endl;
    unsigned int board_id = (reg_status&0x0f000000)>>24;
    // board_type bitmask: BOARD_NONE, BOARD_QLA, BOARD_DQLA, BOARD_DRAC
    unsigned int board_mask = (reg_status & 0x00f00000)>>20;
    bool isV30 = (reg_status&0x00080000);

    if (isV30)
        std::cout << "FPGA V3.0 detected!" << std::endl;

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
    std::cout << "Board type: " << BoardName[board_type] << std::endl;

    std::cout << "Board ID: " << board_id << std::endl << std::endl;

    std::cout << "Exporting FPGAV3 environment variables" << std::endl;
    char fpga_ver[4];
    fpga_ver[0] = '3';
    fpga_ver[1] = '.';
    fpga_ver[2] = isV30 ? '0' : '1';
    fpga_ver[3] = '\0';
    ExportFpgaInfo(fpga_ver, fpga_sn, BoardName[board_type], board_id);

    // MicroSD card should be auto-mounted

    if (strlen(FirmwareName[board_type]) > 0)
        ProgramFpga(FirmwareName[board_type]);

    ProgramFlash("/media/qspi-boot.bin", "/dev/mtd0");

    std::cout << std::endl << "Enabling PS Ethernet" << std::endl;
    // Bit 25: mask for PS Ethernet enable
    // Bit 16: enable PS eth (Rev 9)
    // Bit  8: enable eth1 (Rev 8)
    // Bit  0: enable eth0 (Rev 8)
    reg_ethctrl = 0x02010101;
    emio.WriteQuadlet(12, reg_ethctrl);

    std::cout << "Setting Ethernet MAC and IP addresses" << std::endl;
    if (!SetMACandIP("eth0", 0, board_id))
        std::cout << "Failed to set MAC or IP address for eth0" << std::endl;

    // Copy first 16 bytes (i.e., FPGA S/N)
    // from QSPI flash to FPGA registers
    std::cout << "Writing FPGA S/N to FPGA" << std::endl << std::endl;
    CopyQspiToFpga("/dev/mtd4ro", &emio, 16);

    std::cout << "*** FPGAV3 Initialization Complete ***" << std::endl << std::endl;
    return 0;
}