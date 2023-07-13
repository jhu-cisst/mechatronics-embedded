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

    char option = 1;
    printf("\r\n");
    while (option != '0') {
        printf("0) Exit\r\n");
        printf("1) Test board id\r\n");
        printf("2) Test QSPI\r\n");
        printf("3) Test memory\r\n");
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
