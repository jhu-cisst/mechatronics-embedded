/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-    */
/* ex: set filetype=cpp softtabstop=4 shiftwidth=4 tabstop=4 cindent expandtab: */

/*
 * Test application for Avcom
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include "xil_printf.h"
#include "sleep.h"
#include "xqspips.h"
#include "qspi.h"
#include "fpgav3_emio.h"

extern void outbyte(char c);
extern char inbyte();

bool TestBoardID();
bool TestQSPI();
bool TestMemory();
bool TestIO();

void menu()
{
    // Initialize EMIO bus interface
    EMIO_Init();

    // Query flash and get FPGA S/N
    if (InitQspi() == XST_SUCCESS) {
        char sn_buff[16];
        if (QspiAccess(0xff0000, (u32)sn_buff, sizeof(sn_buff)) == XST_SUCCESS) {
            if (strncmp(sn_buff, "FPGA ", 5) == 0) {
                // Write to FPGA PROM registers
                EMIO_WritePromData(sn_buff, sizeof(sn_buff));
                char *p = strchr(sn_buff, 0xff);
                if (p)
                    *p = 0;                  // Null terminate at first 0xff
                else
                    sn_buff[13] = 0;         // or at end of string

                xil_printf("FPGA S/N: %s\r\n", sn_buff+5);
            }
        }
        else {
            xil_printf("Failed to read from QSPI\r\n");
        }
    }
    else {
        xil_printf("Failed to initialize QSPI\r\n");
    }

    // Check whether connected to TEST board (Manufacturing Test)
    uint32_t status_reg;
    EMIO_ReadQuadlet(0, &status_reg);
    bool isTestBoard = ((status_reg & 0x00f02000) == 0x00002000);

    char option = 1;
    printf("\r\n");
    while (option != '0') {
        printf("0) Exit\r\n");
        printf("1) Test board id\r\n");
        printf("2) Test QSPI\r\n");
        printf("3) Test memory\r\n");
        if (isTestBoard)
            printf("4) Test I/O\r\n");
        printf("Enter selection:\r\n");

        option = inbyte();
        if (option == '\r')
        {
          outbyte('\n');
        }
        printf("\n\rOption Selected : %c", option);
        outbyte(option);
        printf("\n\n");

        if (option == '1') {
           TestBoardID();
        }
        else if (option == '2') {
            TestQSPI();
        }
        else if (option == '3') {
            TestMemory();
        }
        else if (isTestBoard && (option == '4')) {
            TestIO();
        }
    }   
    printf("Exiting\r\n");    
}

extern void memtest_run(void);

bool TestMemory()
{
    memtest_run();
    printf("... Finished\r\n\r\n");
    return true;
}

bool TestBoardID()
{
    printf("Rotate switch to all 16 settings\r\n");
    int switchOK = 0;
    int last_id = 16;
    int prev_id = 16;
    int debounce = 0;
    while (switchOK != 0x0000ffff) {
        uint32_t status_reg;
        EMIO_ReadQuadlet(0, &status_reg);
        int board_id = (status_reg&0x0f000000)>>24;
        if (board_id == prev_id) {
            debounce++;
        }
        else {
            prev_id = board_id;
            debounce = 0;
        }
        usleep(10000);
        if ((board_id != last_id) && (debounce >= 10)) {
            int mask = (1 << board_id);
            if (!(switchOK&mask)) {
                printf("  Found %d\r\n", board_id);
                switchOK |= mask;
            }
            last_id = board_id;
            debounce = 0;
        }
    }
    printf("\r\n");
    return true;
}

extern int QspiPsSelfTestExample(u16 DeviceId);

bool TestQSPI()
{
    int status = QspiPsSelfTestExample(XPAR_PS7_QSPI_0_DEVICE_ID);
    bool ret = (status == 0);
    if (ret) printf("QSPI Test PASS\r\n\r\n");
    else printf("QSPI Test FAIL\r\n\r\n");
    return ret;
}

// Channels (see BootConfig.v)
//  Channel 1:  IO1[31:0]
//  Channel 2:  IO1[33:32]
//  Channel 3:  IO2[31:0]
//  Channel 4:  IO2[39:32]

// IO_Channel: returns the I/O channel number (1-4)
// Parameters:
//   bank:    1 --> IO1, 2 --> IO2
//   index:   0-39

int IO_Channel(unsigned int bank, int index)
{
    return (index < 32) ? (2*bank-1) : (2*bank);
}

// Channel register offsets (see BootConfig.v)
const uint16_t OFF_BCFG_IO_IN  = 0;
const uint16_t OFF_BCFG_IO_DIR = 1;
const uint16_t OFF_BCFG_IO_OUT = 2;

// Loopback configuration
//   IOn_Loop[i] = j means that IOn[i] <--> IOn[j], where n = 1 or 2
//   IOn_Loop[i] = -1 means that IOn[i] is not used in loopback (IO1[1:4] are used for SPI, IO1[34:39] do not exist)

int IO1_Loop[40] = {
//                  0   1   2   3   4   5   6   7   8   9  10  11  12  13  14  15
                    5, -1, -1, -1, -1,  0,  7,  6,  9,  8, 11, 10, 23, 14, 13, 16,
//                  16  17  18  19  20  21  22  23  24  25  26  27  28  29  30  31
                    15, 18, 17, 20, 19, 22, 21, 12, 25, 24, 27, 26, 29, 28, 31, 30,
//                  32  33  34  35  36  37  38  39
                    33, 32, -1, -1, -1, -1, -1, -1 };

int IO2_Loop[40] = {
//                  0   1   2   3   4   5   6   7   8   9  10  11  12  13  14  15
                    1,  0,  3,  2,  5,  4,  7,  6,  9,  8, 11, 10, 25, 14, 13, 16,
//                  16  17  18  19  20  21  22  23  24  25  26  27  28  29  30  31
                    15, 18, 17, 20, 19, 22, 21, 24, 23, 12, 27, 26, 29, 28, 31, 30,
//                  32  33  34  35  36  37  38  39
                    36, 38, 35, 34, 32, 39, 33, 37 };

int *IO_Loop[3] = { 0, IO1_Loop, IO2_Loop };

// IO1[1:4] are SPI to EEPROM, STM32 on test board has pull-ups on all except MISO (output), which has pull-down
// Following are the loopback pairs:
//   IO1: 0-5, 6-7, 8-9, 10-11, 12-23, 13-14, 15-16, 17-18, 19-20, 21-22, 24-25, 26-27, 28-29, 30-31, 32-33
//   IO2: 0-1, 2-3, 4-5, 6-7, 8-9, 10-11, 12-25, 13-14, 15-16, 17-18, 19-20, 21-22, 23-24, 26-27, 28-29, 30-31,
//        32-36, 33-38, 34-35, 37-39
bool TestIO()
{
    int i, j;
    unsigned int bank, chan;

    printf("Testing I/O\r\n");

    // ChanDir will hold the IO_DIR settings (1 --> output)
    // ChanMask will mask out any bits not used for loopbacks (-1)
    uint32_t ChanDir[5];
    uint32_t ChanMask[5];
    for (chan = 0; chan <= 4; chan++) {
        ChanDir[chan] = 0;
        ChanMask[chan] = 0;
    }

    // First, check that IO1_Loop and IO2_Loop are consistent (should be, unless a programming error),
    // and initialize ChanDir so that each lowest numbered index (in loopback pair) is set as output.
    for (bank = 1; bank <= 2; bank++) {
        for (i = 0; i < 40; i++) {
            j = IO_Loop[bank][i];
            if (j < 0)
                continue;
            if (IO_Loop[bank][j] != i) {
                printf("IO inconsistency, IO%d[%d] = %d\r\n", bank, i, j);
                return false;
            }
            chan = IO_Channel(bank, i);
            uint32_t bit_mask = 1 << (i%32);
            ChanMask[chan] |= bit_mask;
            if (i < j)
                ChanDir[chan] |= bit_mask;
        }
    }

    // Make sure all I/O are input
    for (chan = 1; chan <= 4; chan++) {
        uint32_t io_dir;
        EMIO_ReadQuadlet((chan<<4) | OFF_BCFG_IO_DIR, &io_dir);
        if (io_dir != 0) {
            printf("Channel %d: io_dir = %lx, resetting\r\n", chan, io_dir);
            EMIO_WriteQuadlet((chan<<4) | OFF_BCFG_IO_DIR, 0);
        }
    }

    // Set the direction
    for (chan = 1; chan <= 4; chan++) {
        EMIO_WriteQuadlet((chan << 4) | OFF_BCFG_IO_DIR, ChanDir[chan]);
    }

    // Run the walking bit test, first with a single 0, then with a single 1
    unsigned int num_errors = 0;
    for (unsigned int val = 0; val <= 1; val++) {
        printf("\r\nWalking bit test (%d) ", val);
        for (bank = 1; bank <= 2; bank++) {
            for (i = 0; i < 40; i++) {
                j = IO_Loop[bank][i];
                if (i < j) {
                    chan = IO_Channel(bank, i);
                    uint32_t out = 1 << (i%32);
                    uint32_t expected = out | (1 << (j%32));
                    if (val == 0) {
                        out = (~out)&ChanMask[chan];
                        expected = (~expected)&ChanMask[chan];
                    }
                    EMIO_WriteQuadlet((chan << 4) | OFF_BCFG_IO_OUT, out);
                    printf(".");
                    uint32_t in;
                    EMIO_ReadQuadlet((chan << 4) | OFF_BCFG_IO_IN, &in);
                    in &= ChanMask[chan];
                    if (in != expected) {
                        printf("\r\nIO%d ERROR: wrote %lx (i=%d), expected %lx (j=%d), read %lx\r\n",
                               bank, out, i, (expected&ChanMask[chan]), j, (in&ChanMask[chan]));
                        num_errors++;
                    }
                }
            }
        }
    }

    // Set all directions to input
    for (chan = 1; chan <= 4; chan++)
        EMIO_WriteQuadlet((chan << 4) | OFF_BCFG_IO_DIR, 0);

    if (num_errors > 0)
        printf("\r\n*** Detected %d errors\r\n\r\n", num_errors);
    else
        printf("\r\nLoopback test successful\r\n\r\n");

    return (num_errors == 0);
}
