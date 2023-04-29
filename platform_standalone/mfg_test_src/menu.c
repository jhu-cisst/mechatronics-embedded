/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-    */
/* ex: set filetype=cpp softtabstop=4 shiftwidth=4 tabstop=4 cindent expandtab: */

/*
 * Test application for Avcom
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include "xil_printf.h"
#include "xil_io.h"
#include "sleep.h"
#include "xqspips.h"

extern void outbyte(char c);
extern char inbyte();

bool TestBoardID();
bool TestQSPI();
bool TestMemory();

void menu()
{
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
        int status_reg = Xil_In32(XPS_GPIO_BASEADDR+0x00000068);
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
