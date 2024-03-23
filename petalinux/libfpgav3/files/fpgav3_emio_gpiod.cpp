/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-    */
/* ex: set filetype=cpp softtabstop=4 shiftwidth=4 tabstop=4 cindent expandtab: */

#include <iostream>
#include <gpiod.h>
#include <byteswap.h>
#include "fpgav3_emio_gpiod.h"

// Some hard-coded values
const unsigned int INDEX_EMIO_START = 54;
const unsigned int INDEX_EMIO_END = 118;

// Initially, had CTRL_REQ_BUS as first enum value, but it did not work;
// perhaps the GPIO lines must be sequential, at least in libgpiod 1.6.3.
// Thus, moved CTRL_REQ_BUS to a separate line (req_bus_line).
enum { CTRL_REG_WEN=0, CTRL_BLK_START, CTRL_BLK_END, CTRL_MAX };

int ctrl_zero[CTRL_MAX];    // all 0
int ctrl_qwrite[CTRL_MAX];  // quadlet write (reg_wen, blk_end)

// Return GPIOD library version string
const char *EMIO_gpiod_version_string()
{
    return gpiod_version_string();
}

// GPIO (EMIO) access to FPGA registers

struct gpiod_info {
    struct gpiod_chip *chip;
    struct gpiod_line_bulk reg_data_lines;
    struct gpiod_line_bulk reg_addr_lines;
    struct gpiod_line      *addr_lsb_line;
    struct gpiod_line      *op_done_line;
    struct gpiod_line      *bus_grant_line;
    struct gpiod_line      *req_bus_line;
    struct gpiod_line_bulk ctrl_lines;
};

EMIO_Interface_Gpiod::EMIO_Interface_Gpiod() : EMIO_Interface()
{
    if (!Init()) {
        delete info;
        info = 0;
    }
}

bool EMIO_Interface_Gpiod::Init()
{
    unsigned int version_offsets[4];
    unsigned int reg_data_offsets[32];
    unsigned int reg_addr_offsets[16];
    unsigned int req_bus_offset;
    unsigned int op_done_offset;
    unsigned int reg_wen_offset;
    unsigned int blk_start_offset;
    unsigned int blk_end_offset;
    unsigned int bus_grant_offset;
    unsigned int addr_lsb_offset;
    unsigned int ctrl_offsets[CTRL_MAX];
    int version_values[4];
    int reg_addr_values[16];
    size_t i;

    info = new gpiod_info;

    for (i = 0; i < 4; i++)
        version_offsets[i] = INDEX_EMIO_START+63-i;
    for (i = 0; i < 32; i++)
        reg_data_offsets[i] = INDEX_EMIO_START+31-i;
    for (i = 0; i < 16; i++) {
        reg_addr_offsets[i] = INDEX_EMIO_START+47-i;
        reg_addr_values[i] = 0;
    }
    for (i = 0; i < CTRL_MAX; i++) {
        ctrl_zero[i] = 0;
        ctrl_qwrite[i] = 0;
    }

    // For quadlet write, set reg_wen, blk_end and req_bus to 1
    // Rising edge of req_bus requests firmware to write register; note that this
    // can be set at the same time as the other signals because it is delayed in
    // the firmware.
    ctrl_qwrite[CTRL_REG_WEN] = 1;   // no longer used (version 1)
    ctrl_qwrite[CTRL_BLK_END] = 1;   // no longer used for quadlet write (version 1)

    req_bus_offset = INDEX_EMIO_START+48;
    op_done_offset = INDEX_EMIO_START+49;
    reg_wen_offset = INDEX_EMIO_START+50;
    blk_start_offset = INDEX_EMIO_START+51;
    blk_end_offset = INDEX_EMIO_START+52;
    bus_grant_offset = INDEX_EMIO_START+54;
    addr_lsb_offset = INDEX_EMIO_START+55;

    // Ctrl offsets
    ctrl_offsets[CTRL_REG_WEN] = reg_wen_offset;
    ctrl_offsets[CTRL_BLK_START] = blk_start_offset;
    ctrl_offsets[CTRL_BLK_END] = blk_end_offset;

    // First, open the chip
    info->chip = gpiod_chip_open("/dev/gpiochip0");
    if (info->chip == NULL) {
        std::cout << "Failed to open /dev/gpiochip0" << std::endl;
        return false;
    }

    // Get a group of lines for version info (this is a local variable because
    // we only need to use it during initialization)
    struct gpiod_line_bulk version_lines;
    if (gpiod_chip_get_lines(info->chip, version_offsets, 4, &version_lines) != 0) {
        std::cout << "Failed to get version_lines" << std::endl;
        gpiod_chip_close(info->chip);
        return false;
    }
    // Set all lines to input
    if (gpiod_line_request_bulk_input(&version_lines, "libfpgav3") != 0) {
        std::cout << "Failed to set version_lines to input" << std::endl;
        gpiod_chip_close(info->chip);
        return false;
    }
    // Read version data
    gpiod_line_get_value_bulk(&version_lines, version_values);
    version = 0;
    for (i = 0; i < 4; i++)
        version = (version<<1)|version_values[i];
    // No longer need these lines
    gpiod_line_release_bulk(&version_lines);

    if (version > 1) {
        std::cout << "EMIO bus interface version " << version << " not supported" << std::endl;
        gpiod_chip_close(info->chip);
        return false;
    }

    // Get a group of lines (bits) for reg_data
    if (gpiod_chip_get_lines(info->chip, reg_data_offsets, 32, &info->reg_data_lines) != 0) {
        std::cout << "Failed to get reg_data_lines" << std::endl;
        gpiod_chip_close(info->chip);
        return false;
    }
    // Set all lines to input
    if (gpiod_line_request_bulk_input(&info->reg_data_lines, "libfpgav3") != 0) {
        std::cout << "Failed to set reg_data_lines to input" << std::endl;
        gpiod_chip_close(info->chip);
        return false;
    }
    isInput = true;

    // Get a group of lines (bits) for reg_addr
    if (gpiod_chip_get_lines(info->chip, reg_addr_offsets, 16, &info->reg_addr_lines) != 0) {
        std::cout << "Failed to get reg_addr_lines" << std::endl;
        gpiod_chip_close(info->chip);
        return false;
    }
    // Set all lines to output, initialized to 0
    if (gpiod_line_request_bulk_output(&info->reg_addr_lines, "libfpgav3", reg_addr_values) != 0) {
        std::cout << "Failed to set reg_addr_lines to output" << std::endl;
        gpiod_chip_close(info->chip);
        return false;
    }
    // Get a separate output line for reg_addr[15] (the least significant bit) for use by
    // block read/write. This is due to a limitation of libgpiod, which would require us to
    // release all 16 address lines so that we can request to drive only 1 address line.
    // We therefore create a duplicate address line (for the lsb) to avoid this overhead.
    info->addr_lsb_line = gpiod_chip_get_line(info->chip, addr_lsb_offset);
    if (gpiod_line_request_output(info->addr_lsb_line, "libfpgav3", reg_addr_values[15]) != 0) {
        std::cout << "Failed to set addr_lsb_line to output" << std::endl;
        gpiod_chip_close(info->chip);
        return false;
    }

    // Get op_done line, additional settings in SetEventMode
    info->op_done_line = gpiod_chip_get_line(info->chip, op_done_offset);
    if (!info->op_done_line) {
        std::cout << "Failed to get op_done_line" << std::endl;
        gpiod_chip_close(info->chip);
        return false;
    }

    // Following only available in version 1+, and is not currently used anyway
    info->bus_grant_line = gpiod_chip_get_line(info->chip, bus_grant_offset);
    gpiod_line_request_rising_edge_events(info->bus_grant_line, "libfpgav3");

    // Get req_bus line (previously was included in ctrl_lines)
    info->req_bus_line = gpiod_chip_get_line(info->chip, req_bus_offset);
    if (!info->req_bus_line) {
        std::cout << "Failed to get req_bus_line" << std::endl;
        gpiod_chip_close(info->chip);
        return false;
    }
    if (gpiod_line_request_output(info->req_bus_line, "libfpgav3", 0) != 0) {
        std::cout << "Failed to set req_bus_line to output" << std::endl;
        gpiod_chip_close(info->chip);
        return false;
    }

    // Get a group of lines (bits) for ctrl (reg_wen, blk_start, blk_end)
    // Previously included req_bus, but that did not work
    if (gpiod_chip_get_lines(info->chip, ctrl_offsets, CTRL_MAX, &info->ctrl_lines) != 0) {
        std::cout << "Failed to get ctrl_lines" << std::endl;
        gpiod_chip_close(info->chip);
        return false;
    }
    // Set all lines to output, initialized to 0
    if (gpiod_line_request_bulk_output(&info->ctrl_lines, "libfpgav3", ctrl_zero) != 0) {
        std::cout << "Failed to set ctrl_lines to output" << std::endl;
        gpiod_chip_close(info->chip);
        return false;
    }

    // Make sure event mode is properly initialized
    SetEventMode(useEvents);

    return true;
}

EMIO_Interface_Gpiod::~EMIO_Interface_Gpiod()
{
    if (info) {
        gpiod_line_release_bulk(&info->reg_data_lines);
        gpiod_line_release_bulk(&info->reg_addr_lines);
        gpiod_line_release_bulk(&info->ctrl_lines);
        gpiod_chip_close(info->chip);
        delete info;
    }
}

void EMIO_Interface_Gpiod::SetEventMode(bool newState)
{
    useEvents = newState;
    if (info) {
        // First, release line
        gpiod_line_release(info->op_done_line);
        int ret;
        if (newState)
            ret = gpiod_line_request_rising_edge_events(info->op_done_line, "libfpgav3");
        else
            ret = gpiod_line_request_input(info->op_done_line, "libfpgav3");
        if ((ret != 0) && isVerbose)
            std::cout << "SetEventMode (gpiod): failed to configure op_done_line" << std::endl;
    }
}

// Local method to wait for op_done to be set, using either events or polling (default)
bool EMIO_Interface_Gpiod::WaitOpDone(const char *opType, unsigned int num)
{
    bool ret;
    if (useEvents) {
        struct timespec timeout;
        struct gpiod_line_event event;
        timeout.tv_sec = timeout_us*1.0e-6;
        timeout.tv_nsec = (timeout_us - 1.0e6*timeout.tv_sec)*1.0e3;
        int rc = gpiod_line_event_wait(info->op_done_line, &timeout);
        if (isVerbose) {
            if (rc == 0)
                std::cout << "EMIO event timeout waiting for " << opType << " quadlet " << num << std::endl;
            else if (rc < 0)
                std::cout << "EMIO event error waiting for " << opType << " quadlet " << num << std::endl;
        }
        if (rc == 1) {
            // If successful, read event
            if (gpiod_line_event_read(info->op_done_line, &event) != 0) {
                if (isVerbose)
                    std::cout << "EMIO error reading event for " << opType << " quadlet " << num << std::endl;
                return false;
            }
        }
        ret = (rc == 1);
    }
    else {
       // First, check if op_done is set, since this is often the case
       int val = gpiod_line_get_value(info->op_done_line);
       if (val <= 0) {
           fpgav3_time_t waitStart;
           fpgav3_time_t curTime;
           GetCurTime(&waitStart);
           double dt = 0.0;   // elapsed time in us
           while ((val <= 0) && (dt < timeout_us)) {
               val = gpiod_line_get_value(info->op_done_line);
               GetCurTime(&curTime);
               dt = TimeDiff_us(&waitStart, &curTime);
           }
           if (isVerbose) {
               if (val < 0)
                   std::cout << "EMIO error polling op_done for " << opType << " quadlet " << num << std::endl;
               else if (val == 0)
                   std::cout << "EMIO polling timeout waiting for " << opType << " quadlet " << num << std::endl;
               else
                   std::cout << "Waited " << dt << " us for " << opType << " quadlet " << num << std::endl;
           }
        }
        ret = (val == 1);
    }
    return ret;
}

// ReadQuadlet: read quadlet from specified address
bool EMIO_Interface_Gpiod::ReadQuadlet(uint16_t addr, uint32_t &data)
{
    int reg_rdata_values[32];
    int reg_addr_values[16];
    size_t i;

    // For timing measurements
    fpgav3_time_t beforeWait;
    fpgav3_time_t afterWait;

    // Get start time for measurement
    if (doTiming > 0)
        GetCurTime(&startTime);

    for (i = 0; i < 16; i++)
        reg_addr_values[i] = (addr&(0x8000>>i)) ? 1 : 0;

    // Set all data lines to input
    if (!isInput) {
        if (gpiod_line_set_direction_input_bulk(&info->reg_data_lines) != 0) {
            std::cout << "ReadQuadlet (gpiod): could not set data lines as input" << std::endl;
            return false;
        }
        isInput = true;
    }

    // Write reg_addr (read address)
    gpiod_line_set_value_bulk(&info->reg_addr_lines, reg_addr_values);
    // Set ctrl_lines to 0
    gpiod_line_set_value_bulk(&info->ctrl_lines, ctrl_zero);
    // Set req_bus to 1 (rising edge requests firmware to read register)
    gpiod_line_set_value(info->req_bus_line, 1);

    // Get time before wait
    if (doTiming > 1)
        GetCurTime(&beforeWait);

    if (!WaitOpDone("read", 0)) {
        gpiod_line_set_value(info->req_bus_line, 0);
        return false;
    }

    // Get time after wait
    if (doTiming > 1)
        GetCurTime(&afterWait);

    // Read data from reg_data
    bool ret = gpiod_line_get_value_bulk(&info->reg_data_lines, reg_rdata_values);

    // Set req_bus to 0
    gpiod_line_set_value(info->req_bus_line, 0);

    if (ret != 0) {
        if (isVerbose)
            std::cout << "EMIO_ReadQuadlet: error reading data lines" << std::endl;
        return false;
    }

    data = 0;
    for (i = 0; i < 32; i++)
        data = (data<<1)|reg_rdata_values[i];

    // Get end time
    if (doTiming > 0) {
        GetCurTime(&endTime);
        double dt = TimeDiff_us(&startTime, &endTime)-timingOverhead;
        if (doTiming > 1) dt -= 2*timingOverhead;
        std::cout << "ReadQuadlet total time = " << dt << " us" << std::endl;
        if (doTiming > 1) {
            std::cout << "Start-wait-end times = " 
                      << TimeDiff_us(&startTime, &beforeWait)-timingOverhead << ", "
                      << TimeDiff_us(&beforeWait, &afterWait)-timingOverhead << ", "
                      << TimeDiff_us(&afterWait, &endTime)-timingOverhead << " us" << std::endl;
        }
    }

    return true;
}

// WriteQuadlet: write quadlet to specified address
bool EMIO_Interface_Gpiod::WriteQuadlet(uint16_t addr, uint32_t data)
{
    int reg_wdata_values[32];
    int reg_addr_values[16];
    size_t i;

    // For timing measurements
    fpgav3_time_t beforeWait;
    fpgav3_time_t afterWait;

    // Get start time for measurement
    if (doTiming > 0)
        GetCurTime(&startTime);

    for (i = 0; i < 16; i++)
        reg_addr_values[i] = (addr&(0x8000>>i)) ? 1 : 0;

    for (i = 0; i < 32; i++)
        reg_wdata_values[i] = (data&(0x80000000>>i)) ? 1 : 0;

    if (isInput) {
        // Set all data lines to output and write values
        if (gpiod_line_set_direction_output_bulk(&info->reg_data_lines, reg_wdata_values) != 0) {
            std::cout << "WriteQuadlet (gpiod): could not set data lines as output" << std::endl;
            return false;
        }
        isInput = false;
    }
    else {
        // Output already set, just write values
        gpiod_line_set_value_bulk(&info->reg_data_lines, reg_wdata_values);
    }

    // Write reg_addr (write address)
    gpiod_line_set_value_bulk(&info->reg_addr_lines, reg_addr_values);

    // Set ctrl_lines for quadlet write
    gpiod_line_set_value_bulk(&info->ctrl_lines, ctrl_qwrite);
    // Set req_bus to 1 (rising edge requests firmware to write register)
    gpiod_line_set_value(info->req_bus_line, 1);

    // Get time before wait
    if (doTiming > 1)
        GetCurTime(&beforeWait);

    // op_done should be set quickly by firmware, to indicate
    // that write has completed
    bool ret = WaitOpDone("write", 0);

    // Get time after wait
    if (ret && (doTiming > 1))
        GetCurTime(&afterWait);

    // Set req_bus (and other ctrl lines) to 0
    gpiod_line_set_value(info->req_bus_line, 0);
    gpiod_line_set_value_bulk(&info->ctrl_lines, ctrl_zero);

    // Get end time
    if (ret && (doTiming > 0)) {
        GetCurTime(&endTime);
        double dt = TimeDiff_us(&startTime, &endTime)-timingOverhead;
        if (doTiming > 1) dt -= 2*timingOverhead;
        std::cout << "WriteQuadlet total time = " << dt << " us" << std::endl;
        if (doTiming > 1) {
            std::cout << "Start-wait-end times = "
                      << TimeDiff_us(&startTime,  &beforeWait)-timingOverhead << ", "
                      << TimeDiff_us(&beforeWait, &afterWait)-timingOverhead << ", "
                      << TimeDiff_us(&afterWait,  &endTime)-timingOverhead << " us" << std::endl;
        }
    }

    return ret;
}

// Read block of data
bool EMIO_Interface_Gpiod::ReadBlock(uint16_t addr, uint32_t *data, unsigned int nBytes)
{
    unsigned int i, q;
    unsigned int nQuads = (nBytes+3)/4;
    int reg_rdata_values[32];
    int reg_addr_values[16];
    int ctrl_values[CTRL_MAX];
    uint32_t val;

    // For timing measurements
    fpgav3_time_t firstRead;
    fpgav3_time_t lastRead;

    if (version < 1) {
        std::cout << "ReadBlock (gpiod): not supported for version " << version << std::endl;
        return false;
    }

    // Get start time for measurement
    if (doTiming > 0)
        GetCurTime(&startTime);

    // Set all data lines to input
    if (!isInput) {
        if (gpiod_line_set_direction_input_bulk(&info->reg_data_lines) != 0) {
            std::cout << "ReadBlock (gpiod): could not set data lines as input" << std::endl;
            return false;
        }
        isInput = true;
    }

    // Set blk_start to 1
    ctrl_values[CTRL_REG_WEN] = 0;
    ctrl_values[CTRL_BLK_START] = 1;
    ctrl_values[CTRL_BLK_END] = 0;
    if (gpiod_line_set_value_bulk(&info->ctrl_lines, ctrl_values) != 0) {
        if (isVerbose)
            std::cout << "_ReadBlock (gpiod): error setting ctrl (blk_start)" << std::endl;
        return false;
    }

    // Initialize reg_addr_values
    for (i = 0; i < 16; i++)
        reg_addr_values[i] = (addr&(0x8000>>i)) ? 1 : 0;

    // Write reg_addr (read address)
    if (gpiod_line_set_value_bulk(&info->reg_addr_lines, reg_addr_values) != 0) {
        if (isVerbose)
            std::cout << "ReadBlock (gpiod): error setting initial address "
                      << std::hex << addr << std::dec << std::endl;
        gpiod_line_set_value_bulk(&info->ctrl_lines, ctrl_zero);
        return false;
    }
    // Make sure addr_lsb_line is set to reg_addr_values[15] (lsb)
    gpiod_line_set_value(info->addr_lsb_line, reg_addr_values[15]);

    // Set req_bus
    if (gpiod_line_set_value(info->req_bus_line, 1) != 0) {
        if (isVerbose)
            std::cout << "EMIO_ReadBlock: error setting req_bus" << std::endl;
        gpiod_line_set_value_bulk(&info->ctrl_lines, ctrl_zero);
        return false;
    }

    if (doTiming > 1)
        GetCurTime(&firstRead);

    for (q = 0; q < nQuads; q++) {

        if (q > 0) {
            if (q == nQuads-1) {
                // Set blk_end to 1 to indicate end of block write
                ctrl_values[CTRL_BLK_START] = 0;
                ctrl_values[CTRL_BLK_END] = 1;
                if (gpiod_line_set_value_bulk(&info->ctrl_lines, ctrl_values) != 0) {
                    if (isVerbose)
                        std::cout << "ReadBlock (gpiod): error setting ctrl (blk_end)" << std::endl;
                    gpiod_line_set_value(info->req_bus_line, 0);
                    gpiod_line_set_value_bulk(&info->ctrl_lines, ctrl_zero);
                    return false;
                 }
            }

            // Increment lsb of address
            reg_addr_values[15] = ~reg_addr_values[15];

            // Write reg_addr_lsb (lsb of read address) last because address change
            // will trigger read
            if (gpiod_line_set_value(info->addr_lsb_line, reg_addr_values[15]) != 0) {
                if (isVerbose)
                    std::cout << "ReadBlock (gpiod): error setting address for quadlet " << q << std::endl;
                gpiod_line_set_value(info->req_bus_line, 0);
                gpiod_line_set_value_bulk(&info->ctrl_lines, ctrl_zero);
                return false;
            }
        }

        // op_done set by firmware, to indicate that quadlet
        // has been read
        if (!WaitOpDone("read", q)) {
            gpiod_line_set_value(info->req_bus_line, 0);
            gpiod_line_set_value_bulk(&info->ctrl_lines, ctrl_zero);
            return false;
        }

        // Read data from reg_data
        if (gpiod_line_get_value_bulk(&info->reg_data_lines, reg_rdata_values) != 0) {
            if (isVerbose)
                std::cout << "ReadBlock (gpiod): error reading quadlet " << q
                          << " (of " << nQuads << ")" << std::endl;
            // Set all lines to 0
            gpiod_line_set_value(info->req_bus_line, 0);
            gpiod_line_set_value_bulk(&info->ctrl_lines, ctrl_zero);
            return false;
        }

        val = 0;
        for (i = 0; i < 32; i++)
            val = (val<<1)|reg_rdata_values[i];
        data[q] = bswap_32(val);
    }

    if (doTiming > 1)
        GetCurTime(&lastRead);

    // Set all lines to 0
    gpiod_line_set_value(info->req_bus_line, 0);
    gpiod_line_set_value_bulk(&info->ctrl_lines, ctrl_zero);

    // Get end time
    if (doTiming > 0) {
        GetCurTime(&endTime);
        double dt = TimeDiff_us(&startTime, &endTime)-timingOverhead;
        if (doTiming > 1) dt -= 2*timingOverhead;
        std::cout << "ReadBlock total time = " << dt << " us" << std::endl;
        if (doTiming > 1) {
            std::cout << "Start-loop-end times = "
                      << TimeDiff_us(&startTime, &firstRead)-timingOverhead << ", "
                      << TimeDiff_us(&firstRead, &lastRead)-timingOverhead << ", "
                      << TimeDiff_us(&lastRead,  &endTime)-timingOverhead << " us" << std::endl;
        }
    }

    return true;
}

// Write block of data.
bool EMIO_Interface_Gpiod::WriteBlock(uint16_t addr, const uint32_t *data, unsigned int nBytes)
{
    int reg_wdata_values[32];
    int reg_addr_values[16];
    int ctrl_values[CTRL_MAX];
    unsigned int i, q;

    // For timing measurements
    fpgav3_time_t firstWrite;
    fpgav3_time_t lastWrite;

    unsigned int nQuads = (nBytes+3)/4;

    if (version < 1) {
        std::cout << "WriteBlock (gpiod): not supported for version " << version << std::endl;
        return false;
    }

    // Get start time for measurement
    if (doTiming > 0)
        GetCurTime(&startTime);

    uint32_t val = bswap_32(data[0]);
    for (i = 0; i < 32; i++)
        reg_wdata_values[i] = (val&(0x80000000>>i)) ? 1 : 0;

    if (isInput) {
        // Set all data lines to output (and write values)
        if (gpiod_line_set_direction_output_bulk(&info->reg_data_lines, reg_wdata_values) != 0) {
            std::cout << "WriteBlock (gpiod): could not set data lines as output" << std::endl;
            return false;
        }
        isInput = false;
    }

    // Write initial address
    for (i = 0; i < 16; i++)
        reg_addr_values[i] = (addr&(0x8000>>i)) ? 1 : 0;
    gpiod_line_set_value_bulk(&info->reg_addr_lines, reg_addr_values);
    // Make sure addr_lsb_line is set to reg_addr_values[15] (lsb)
    gpiod_line_set_value(info->addr_lsb_line, reg_addr_values[15]);

    // Set blk_start and req_bus to 1 (reg_wen no longer used)
    ctrl_values[CTRL_REG_WEN] = 0;
    ctrl_values[CTRL_BLK_START] = 1;
    ctrl_values[CTRL_BLK_END] = 0;
    gpiod_line_set_value_bulk(&info->ctrl_lines, ctrl_values);
    gpiod_line_set_value(info->req_bus_line, 1);

    // op_done set by firmware, to indicate that blk_start has been
    // generated and first quadlet has been written
    if (!WaitOpDone("write", 0)) {
        gpiod_line_set_value(info->req_bus_line, 0);
        gpiod_line_set_value_bulk(&info->ctrl_lines, ctrl_zero);
        return false;
    }

    if (doTiming > 1)
        GetCurTime(&firstWrite);

    for (q = 1; q < nQuads; q++) {

        // Increment lsb of address
        reg_addr_values[15] = ~reg_addr_values[15];

        val = bswap_32(data[q]);
        for (i = 0; i < 32; i++)
            reg_wdata_values[i] = (val&(0x80000000>>i)) ? 1 : 0;

        // Write reg_data
        gpiod_line_set_value_bulk(&info->reg_data_lines, reg_wdata_values);

        if (q == nQuads-1) {
            // Set blk_end to 1 to indicate end of block write
            ctrl_values[CTRL_BLK_START] = 0;
            ctrl_values[CTRL_BLK_END] = 1;
            gpiod_line_set_value_bulk(&info->ctrl_lines, ctrl_values);
        }

        // Write reg_addr_lsb (lsb of write address) last because address change
        // will trigger actual write
        if (gpiod_line_set_value(info->addr_lsb_line, reg_addr_values[15]) != 0) {
            if (isVerbose)
                std::cout << "WriteBlock (gpiod): error setting address for quadlet " << q << std::endl;
            gpiod_line_set_value(info->req_bus_line, 0);
            gpiod_line_set_value_bulk(&info->ctrl_lines, ctrl_zero);
            return false;
        }

        // op_done set by firmware, to indicate that quadlet
        // has been written
        if (!WaitOpDone("write", q)) {
            gpiod_line_set_value(info->req_bus_line, 0);
            gpiod_line_set_value_bulk(&info->ctrl_lines, ctrl_zero);
            return false;
        }
    }

    if (doTiming > 1)
        GetCurTime(&lastWrite);

    // Set all lines to 0
    gpiod_line_set_value(info->req_bus_line, 0);
    gpiod_line_set_value_bulk(&info->ctrl_lines, ctrl_zero);

    // Get end time
    if (doTiming > 0) {
        GetCurTime(&endTime);
        double dt = TimeDiff_us(&startTime, &endTime)-timingOverhead;
        if (doTiming > 1) dt -= 2*timingOverhead;
        std::cout << "WriteBlock total time = " << dt << " us" << std::endl;
        if (doTiming > 1) {
            std::cout << "Start-loop-end times = "
                      << TimeDiff_us(&startTime,  &firstWrite)-timingOverhead << ", "
                      << TimeDiff_us(&firstWrite, &lastWrite)-timingOverhead << ", "
                      << TimeDiff_us(&lastWrite,  &endTime)-timingOverhead << " us" << std::endl;
        }
    }

    return true;
}
