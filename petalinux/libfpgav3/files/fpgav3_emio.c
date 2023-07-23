/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-    */
/* ex: set filetype=cpp softtabstop=4 shiftwidth=4 tabstop=4 cindent expandtab: */

#include <stdio.h>
#include <gpiod.h>
#include <byteswap.h>
#include "fpgav3_emio.h"

// Some hard-coded values
const unsigned int INDEX_EMIO_START = 54;
const unsigned int INDEX_EMIO_END = 118;

// GPIO (EMIO) access to FPGA registers

struct EMIO_Info {
    struct gpiod_chip *chip;
    struct gpiod_line_bulk reg_data_lines;
    struct gpiod_line_bulk reg_addr_lines;
    struct gpiod_line *req_bus_line;
    struct gpiod_line *op_done_line;
    struct gpiod_line *reg_wen_line;
    struct gpiod_line *blk_wstart_line;
    struct gpiod_line *blk_wen_line;
};

struct EMIO_Info *EMIO_Init()
{
    unsigned int reg_data_offsets[32];
    unsigned int reg_addr_offsets[16];
    unsigned int req_bus_offset;
    unsigned int op_done_offset;
    unsigned int reg_wen_offset;
    unsigned int blk_wstart_offset;
    unsigned int blk_wen_offset;
    int reg_addr_values[16];

    struct EMIO_Info *info;
    info = (struct EMIO_Info *) malloc(sizeof(struct EMIO_Info));
    if (!info)
        return 0;

    size_t i;
    for (i = 0; i < 32; i++)
        reg_data_offsets[i] = INDEX_EMIO_START+31-i;
    for (i = 0; i < 16; i++) {
        reg_addr_offsets[i] = INDEX_EMIO_START+47-i;
        reg_addr_values[i] = 0;
    }
    req_bus_offset = INDEX_EMIO_START+48;
    op_done_offset = INDEX_EMIO_START+49;
    reg_wen_offset = INDEX_EMIO_START+50;
    blk_wstart_offset = INDEX_EMIO_START+51;
    blk_wen_offset = INDEX_EMIO_START+52;

    // First, open the chip
    info->chip = gpiod_chip_open("/dev/gpiochip0");
    if (info->chip == NULL) {
        printf("Failed to open /dev/gpiochip0\n");
        return false;
    }

    // Get a group of lines (bits) for reg_data
    if (gpiod_chip_get_lines(info->chip, reg_data_offsets, 32, &info->reg_data_lines) != 0) {
        printf("Failed to get reg_data_lines\n");
        gpiod_chip_close(info->chip);
        return false;
    }
    // Set all lines to input
    gpiod_line_request_bulk_input(&info->reg_data_lines, "fpgav3init");

    // Get a group of lines (bits) for reg_addr
    if (gpiod_chip_get_lines(info->chip, reg_addr_offsets, 16, &info->reg_addr_lines) != 0) {
        printf("Failed to get reg_addr_lines\n");
        gpiod_chip_close(info->chip);
        return false;
    }
    // Set all lines to output, initialized to 0
    gpiod_line_request_bulk_output(&info->reg_addr_lines, "fpgav3init", reg_addr_values);

    info->req_bus_line = gpiod_chip_get_line(info->chip, req_bus_offset);
    gpiod_line_request_output(info->req_bus_line, "fpgav3init", 0);

    info->op_done_line = gpiod_chip_get_line(info->chip, op_done_offset);
    gpiod_line_request_input(info->op_done_line, "fpgav3init");

    info->reg_wen_line = gpiod_chip_get_line(info->chip, reg_wen_offset);
    gpiod_line_request_output(info->reg_wen_line, "fpgav3init", 0);

    info->blk_wstart_line = gpiod_chip_get_line(info->chip, blk_wstart_offset);
    gpiod_line_request_output(info->blk_wstart_line, "fpgav3init", 0);

    info->blk_wen_line = gpiod_chip_get_line(info->chip, blk_wen_offset);
    gpiod_line_request_output(info->blk_wen_line, "fpgav3init", 0);

    return info;
}

void EMIO_Release(struct EMIO_Info *info)
{
    gpiod_line_release_bulk(&info->reg_data_lines);
    gpiod_line_release_bulk(&info->reg_addr_lines);
    gpiod_chip_close(info->chip);
    free(info);
}

// EMIO_ReadQuadlet: read quadlet from specified address
bool EMIO_ReadQuadlet(struct EMIO_Info *info, uint16_t addr, uint32_t *data)
{
    int reg_rdata_values[32];
    int reg_addr_values[16];
    size_t i;

    for (i = 0; i < 16; i++)
        reg_addr_values[i] = (addr&(0x8000>>i)) ? 1 : 0;

    // Set all data lines to input
    gpiod_line_release_bulk(&info->reg_data_lines);
    if (gpiod_line_request_bulk_input(&info->reg_data_lines, "fpgav3init") != 0) {
        printf("EMIO_ReadQuadlet: could not set data lines as input\n");
        return false;
    }

    // Write reg_addr (read address)
    gpiod_line_set_value_bulk(&info->reg_addr_lines, reg_addr_values);
    // Set req_bus to 1 (rising edge requests firmware to read register)
    gpiod_line_set_value(info->req_bus_line, 1);
    // op_done should be set quickly by firmware, to indicate
    // that read has completed
    if (gpiod_line_get_value(info->op_done_line) != 1) {
        printf("Waiting to read");
        // Should set a timeout for following while loop
        while (gpiod_line_get_value(info->op_done_line) != 1)
            printf(".");
        printf("\n");
    }
    // Read data from reg_data
    gpiod_line_get_value_bulk(&info->reg_data_lines, reg_rdata_values);
    // Set req_bus to 0
    gpiod_line_set_value(info->req_bus_line, 0);
    for (i = 0; i < 32; i++)
        *data = (*data<<1)|reg_rdata_values[i];
    return true;
}

// EMIO_WriteQuadlet: write quadlet to specified address
bool EMIO_WriteQuadlet(struct EMIO_Info *info, uint16_t addr, uint32_t data)
{
    int reg_wdata_values[32];
    int reg_addr_values[16];
    size_t i;

    for (i = 0; i < 16; i++)
        reg_addr_values[i] = (addr&(0x8000>>i)) ? 1 : 0;

    for (i = 0; i < 32; i++)
        reg_wdata_values[i] = (data&(0x80000000>>i)) ? 1 : 0;

    // Set all data lines to output and write values
    gpiod_line_release_bulk(&info->reg_data_lines);
    if (gpiod_line_request_bulk_output(&info->reg_data_lines, "fpgav3init", reg_wdata_values) != 0) {
        printf("EMIO_WriteQuadlet: could not set data lines as output\n");
        return false;
    }

    // Write reg_addr (write address)
    gpiod_line_set_value_bulk(&info->reg_addr_lines, reg_addr_values);

    // Set reg_wen to 1
    gpiod_line_set_value(info->reg_wen_line, 1);

    // Set req_bus to 1 (rising edge requests firmware to write register)
    gpiod_line_set_value(info->req_bus_line, 1);
    // op_done should be set quickly by firmware, to indicate
    // that write has completed
    if (gpiod_line_get_value(info->op_done_line) != 1) {
        printf("Waiting to write");
        // Should set a timeout for following while loop
        while (gpiod_line_get_value(info->op_done_line) != 1)
            printf(".");
        printf("\n");
    }
    // Set req_bus to 0
    gpiod_line_set_value(info->req_bus_line, 0);
    return true;
}

bool EMIO_WritePromData(struct EMIO_Info *info, char *data, unsigned int nBytes)
{
    // Round up to nearest multiple of 4
    uint16_t nQuads = (nBytes+3)/4;
    nBytes = 4*nQuads;
    for (uint16_t i = 0; i < nQuads; i++) {
        uint32_t qdata = bswap_32(*(uint32_t *)(data+i*4));
        if (!EMIO_WriteQuadlet(info, 0x2000+i, qdata))
            return false;
    }
    return true;
}
