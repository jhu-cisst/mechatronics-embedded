/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-    */
/* ex: set filetype=cpp softtabstop=4 shiftwidth=4 tabstop=4 cindent expandtab: */

/*
 * Demo application (based on Xilinx Hello World template)
 */


#include <stdio.h>
#include "xil_printf.h"
#include "platform.h"
#include "fpgav3_emio.h"

extern void outbyte(char c);
extern char inbyte();

#define MAX_ARG     20
#define MAX_LINE   120
#define MAX_QUADS   32

static char argv[MAX_ARG][MAX_LINE+1];

// Forward declarations
int get_input();
void print_help();
uint32_t get_hex(char *str);
uint32_t get_dec(char *str);

int main()
{
    init_platform();

    // Initialize EMIO bus interface
    EMIO_Init();

    xil_printf("EMIO interface version: %d\r\n", EMIO_GetVersion());
    xil_printf("Type \"help\" for more information\r\n");

    uint16_t addr;   // Address for read/write
    int num;         // Number of quadlets to read/write
    uint32_t data[MAX_QUADS];

    for (;;) {
        xil_printf("fpgav3: ");
        int argc = get_input();
        if (argc == 0) continue;

        if (strcmp(argv[0], "help") == 0) {
            print_help();
        }
        else if ((strcmp(argv[0], "quad") == 0) && (argc > 1))  {
            addr = get_hex(argv[1]);
            if (argc > 2) {
                data[0] = get_hex(argv[2]);
                if (!EMIO_WriteQuadlet(addr, data[0]))
                    xil_printf("Error writing quadlet\r\n");
            }
            else {
                if (EMIO_ReadQuadlet(addr, &data[0]))
                    xil_printf("  %08x\r\n", data[0]);
                else
                    xil_printf("Error reading quadlet\r\n");
            }
        }
        else if ((strcmp(argv[0], "block") == 0) && (argc > 2)) {
            addr = get_hex(argv[1]);
            num = get_dec(argv[2]);
            if (num > MAX_QUADS) {
                xil_printf("Too many quadlets (max %d)\r\n", MAX_QUADS);
                num = MAX_QUADS;
            }
            if (argc > 3) {
                if (num > argc-3) {
                    xil_printf("Not enough quadlets specified (writing %d of %d)\r\n",
                               argc-3, num);
                    num = argc-3;
                }
                else if (num < argc-3) {
                    int diff = (argc-3)-num;
                    xil_printf("Ignoring %d extra quadlets\r\n", diff);
                }
                for (int i = 0; i < num; i++)
                    data[i] = get_hex(argv[3+i]);
                if (!EMIO_WriteBlock(addr, data, num*sizeof(uint32_t))) {
                    xil_printf("Error writing block\r\n");
                }
            }
            else {
                if (EMIO_ReadBlock(addr, data, num*sizeof(uint32_t))) {
                    for (int i = 0; i < num; i++)
                        xil_printf("  %08x\r\n", data[i]);
                }
                else {
                    xil_printf("Error reading block\r\n");
                }
            }
        }
        else {
            xil_printf("Unknown command (or too few arguments): %s\r\n", argv[0]);
        }
    }

    cleanup_platform();
    return 0;
}

int get_input()
{
    int argc;
    char c;
    bool ignore;
    bool is_space;
    int numChar;

    argc = 0;
    numChar = 0;
    ignore = false;
    is_space = true;
    while ((c = inbyte()) != '\r') {
        outbyte(c);   // echo byte
        if (!is_space && (c == ' ')) {
            // end of arg
            argv[argc][numChar] = 0;
            if (argc < MAX_ARG) {
                argc++;
            }
            else {
                ignore = true;
                xil_printf("\r\nToo many arguments (max %d) - ignored", MAX_ARG);
                break;
            }
            numChar = 0;
        }
        is_space = (c == ' ');
        if (!is_space) {
            if (numChar < MAX_LINE) {
                argv[argc][numChar++] = c;
            }
            else {
                ignore = true;
                xil_printf("\r\nArgument too long (max %d chars) - ignored", MAX_LINE);
                break;
            }
        }
    }
    xil_printf("\r\n");
    if (ignore) {
        argc = 0;
    }
    else {
        argv[argc][numChar] = 0;
        if (!is_space) argc++;
    }
    return argc;
}

void print_help()
{
    xil_printf("Available commands (hex:/dec: indicate hexadecimal or decimal):\r\n");
    xil_printf("  quad <addr>                       - quadlet read from <hex:addr>\r\n");
    xil_printf("  quad <addr> <data>                - quadlet write <hex:data> to <hex:addr>\r\n");
    xil_printf("  block <addr> <num>                - block read <dec:num> quadlets from <hex:addr>\r\n");
    xil_printf("  block <addr> <num> <d0> ... <dN>  - block write <dec:num> quadlets to <hex:addr>, where\r\n");
    xil_printf("                                         <hex:d0> .. <hex:dN> are the quadlet data (N=num-1)\r\n");
    xil_printf("  help                              - print this message\r\n");
    xil_printf("\r\n");
}

// Convert hex string to integer
uint32_t get_hex(char *str)
{
    char *p;
    unsigned char c;
    uint32_t val = 0;
    for (p = str; *p; p++) {
        if ((*p >= '0') && (*p <= '9'))
            c = *p - '0';
        else if ((*p >= 'a') && (*p <= 'f'))
            c = 10 + (*p - 'a');
        else if ((*p >= 'A') && (*p <= 'F'))
            c = 10 + (*p - 'A');
        else
            break;  // ignore character (could instead note an error)
        val = 16*val + (uint32_t)c;
    }
    return val;
}

// Convert decimal string to integer (assumes positive numbers)
uint32_t get_dec(char *str)
{
    char *p;
    unsigned char c;
    uint32_t val = 0;
    for (p = str; *p; p++) {
        if ((*p >= '0') && (*p <= '9'))
            c = *p - '0';
        else
            break;  // ignore character (could instead note an error)
        val = 10*val + (uint32_t)c;
    }
    return val;
}
