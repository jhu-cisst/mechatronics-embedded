/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-    */
/* ex: set filetype=cpp softtabstop=4 shiftwidth=4 tabstop=4 cindent expandtab: */

#include <stdio.h>
#include <gpiod.h>
#include <byteswap.h>
#include <time.h>
#include "fpgav3_emio.h"

#ifdef USE_TIMEOFDAY
typedef struct timeval fpgav3_time_t;
#else
typedef struct timespec fpgav3_time_t;
#endif

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

// Local method to get current time
static void GetCurTime(fpgav3_time_t *curTime)
{
#ifdef USE_TIMEOFDAY
    gettimeofday(&curTime, NULL);
#else
    clock_gettime(CLOCK_MONOTONIC, curTime);
#endif
}

// Local method to compute time differences in microseconds
static double TimeDiff_us(fpgav3_time_t *startTime, fpgav3_time_t *endTime)
{
#ifdef USE_TIMEOFDAY
    return (endTime->tv_sec-startTime->tv_sec)*1.0e6 + (endTime->tv_usec-startTime->tv_usec);
#else
    return (endTime->tv_sec-startTime->tv_sec)*1.0e6 + (endTime->tv_nsec-startTime->tv_nsec)*1.0e-3;
#endif
}

// GPIO (EMIO) access to FPGA registers

struct EMIO_Info {
    struct gpiod_chip *chip;
    struct gpiod_line_bulk reg_data_lines;
    struct gpiod_line_bulk reg_addr_lines;
    struct gpiod_line      *addr_lsb_line;
    struct gpiod_line      *op_done_line;
    struct gpiod_line      *bus_grant_line;
    struct gpiod_line      *req_bus_line;
    struct gpiod_line_bulk ctrl_lines;
    bool isInput;              // true if data lines are input
    bool isVerbose;            // true if error messages should be printed
    bool useEvents;            // true if using events to wait
    unsigned int doTiming;     // timing mode (0, 1 or 2)
    unsigned int version;      // Version from FPGA
    fpgav3_time_t startTime;   // Start time for measurement
    fpgav3_time_t endTime;     // End time for measurement
    double timingOverhead;     // Overhead due to timing calls
};

struct EMIO_Info *EMIO_Init()
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

    struct EMIO_Info *info;
    info = (struct EMIO_Info *) malloc(sizeof(struct EMIO_Info));
    if (!info)
        return 0;

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
        printf("Failed to open /dev/gpiochip0\n");
        return 0;
    }

    // Get a group of lines for version info (this is a local variable because
    // we only need to use it during initialization)
    struct gpiod_line_bulk version_lines;
    if (gpiod_chip_get_lines(info->chip, version_offsets, 4, &version_lines) != 0) {
        printf("Failed to get version_lines\n");
        gpiod_chip_close(info->chip);
        return 0;
    }
    // Set all lines to input
    if (gpiod_line_request_bulk_input(&version_lines, "libfpgav3") != 0) {
        printf("Failed to set version_lines to input\n");
        gpiod_chip_close(info->chip);
        return 0;
    }
    // Read version data
    gpiod_line_get_value_bulk(&version_lines, version_values);
    info->version = 0;
    for (i = 0; i < 4; i++)
        info->version = (info->version<<1)|version_values[i];
    // No longer need these lines
    gpiod_line_release_bulk(&version_lines);

    if (info->version > 1) {
        printf("EMIO bus interface verion %u not supported\n", info->version);
        gpiod_chip_close(info->chip);
        return 0;
    }

    // Get a group of lines (bits) for reg_data
    if (gpiod_chip_get_lines(info->chip, reg_data_offsets, 32, &info->reg_data_lines) != 0) {
        printf("Failed to get reg_data_lines\n");
        gpiod_chip_close(info->chip);
        return 0;
    }
    // Set all lines to input
    if (gpiod_line_request_bulk_input(&info->reg_data_lines, "libfpgav3") != 0) {
        printf("Failed to set reg_data_lines to input\n");
        gpiod_chip_close(info->chip);
        return 0;
    }
    info->isInput = true;

    // Get a group of lines (bits) for reg_addr
    if (gpiod_chip_get_lines(info->chip, reg_addr_offsets, 16, &info->reg_addr_lines) != 0) {
        printf("Failed to get reg_addr_lines\n");
        gpiod_chip_close(info->chip);
        return 0;
    }
    // Set all lines to output, initialized to 0
    if (gpiod_line_request_bulk_output(&info->reg_addr_lines, "libfpgav3", reg_addr_values) != 0) {
        printf("Failed to set reg_addr_lines to output\n");
        gpiod_chip_close(info->chip);
        return 0;
    }
    // Get a separate output line for reg_addr[15] (the least significant bit) for use by
    // block read/write. This is due to a limitation of libgpiod, which would require us to
    // release all 16 address lines so that we can request to drive only 1 address line.
    // We therefore create a duplicate address line (for the lsb) to avoid this overhead.
    info->addr_lsb_line = gpiod_chip_get_line(info->chip, addr_lsb_offset);
    if (gpiod_line_request_output(info->addr_lsb_line, "libfpgav3", reg_addr_values[15]) != 0) {
        printf("Failed to set addr_lsb_line to output\n");
        gpiod_chip_close(info->chip);
        return 0;
    }

    // Get op_done line, additional settings in SetEventMode
    info->op_done_line = gpiod_chip_get_line(info->chip, op_done_offset);
    if (!info->op_done_line) {
        printf("Failed to get op_done_line\n");
        gpiod_chip_close(info->chip);
        return 0;
    }

    // Following only available in version 1+, and is not currently used anyway
    info->bus_grant_line = gpiod_chip_get_line(info->chip, bus_grant_offset);
    gpiod_line_request_rising_edge_events(info->bus_grant_line, "libfpgav3");

    // Get req_bus line (previously was included in ctrl_lines)
    info->req_bus_line = gpiod_chip_get_line(info->chip, req_bus_offset);
    if (!info->req_bus_line) {
        printf("Failed to get req_bus_line\n");
        gpiod_chip_close(info->chip);
        return 0;
    }
    if (gpiod_line_request_output(info->req_bus_line, "libfpgav3", 0) != 0) {
        printf("Failed to set req_bus_line to output\n");
        gpiod_chip_close(info->chip);
        return 0;
    }

    // Get a group of lines (bits) for ctrl (reg_wen, blk_start, blk_end)
    // Previously included req_bus, but that did not work
    if (gpiod_chip_get_lines(info->chip, ctrl_offsets, CTRL_MAX, &info->ctrl_lines) != 0) {
        printf("Failed to get ctrl_lines\n");
        gpiod_chip_close(info->chip);
        return 0;
    }
    // Set all lines to output, initialized to 0
    if (gpiod_line_request_bulk_output(&info->ctrl_lines, "libfpgav3", ctrl_zero) != 0) {
        printf("Failed to set ctrl_lines to output\n");
        gpiod_chip_close(info->chip);
        return 0;
    }

    // Set defaults values of flags
    EMIO_SetVerbose(info, false);
    EMIO_SetEventMode(info, false);
    EMIO_SetTimingMode(info, 0);

    return info;
}

void EMIO_Release(struct EMIO_Info *info)
{
    if (info) {
        gpiod_line_release_bulk(&info->reg_data_lines);
        gpiod_line_release_bulk(&info->reg_addr_lines);
        gpiod_line_release_bulk(&info->ctrl_lines);
        gpiod_chip_close(info->chip);
        free(info);
    }
}

unsigned int EMIO_GetVersion(struct EMIO_Info *info)
{
    return info ? info->version : 0;
}

bool EMIO_GetVerbose(struct EMIO_Info *info)
{
    return info ? info->isVerbose : false;
}

void EMIO_SetVerbose(struct EMIO_Info *info, bool newState)
{
    if (info) info->isVerbose = newState;
}

unsigned int EMIO_GetTimingMode(struct EMIO_Info *info)
{
    return info ? info->doTiming : 0;
}

void EMIO_SetTimingMode(struct EMIO_Info *info, unsigned int newMode)
{
    if (info) {
        info->timingOverhead = 0.0;
        info->doTiming = newMode;

        if (info->doTiming > 0) {
            // Timing calibration
            for (unsigned int i = 0; i < 10; i++) {
                GetCurTime(&info->startTime);
                GetCurTime(&info->endTime);
                info->timingOverhead += TimeDiff_us(&info->startTime, &info->endTime);
            }
            info->timingOverhead /= 10.0;
            if (info->isVerbose)
                printf("Calibrated timing overhead of %lf us\n", info->timingOverhead);
        }
    }
}

bool EMIO_GetEventMode(struct EMIO_Info *info)
{
    return info ? info->useEvents : false;
}

void EMIO_SetEventMode(struct EMIO_Info *info, bool newState)
{
    if (info) {
        info->useEvents = newState;
        // First, release line
        gpiod_line_release(info->op_done_line);
        int ret;
        if (newState)
            ret = gpiod_line_request_rising_edge_events(info->op_done_line, "libfpgav3");
        else
            ret = gpiod_line_request_input(info->op_done_line, "libfpgav3");
        if ((ret != 0) && info->isVerbose)
            printf("EMIO_SetEventMode: failed to configure op_done_line\n");
    }
}

// Local method to wait for op_done to be set, using either events or polling (default)
static bool WaitOpDone(struct EMIO_Info *info, const char *opType, unsigned int num)
{
    bool ret;
    if (info->useEvents) {
        struct timespec timeout;
        struct gpiod_line_event event;
        timeout.tv_sec = 0;
        timeout.tv_nsec = 250000;  // 250 us
        int rc = gpiod_line_event_wait(info->op_done_line, &timeout);
        if (info->isVerbose) {
            if (rc == 0)
                printf("EMIO event timeout waiting for %s quadlet %d\n", opType, num);
            else if (rc < 0)
                printf("EMIO event error waiting for %s quadlet %d\n", opType, num);
        }
        if (rc == 1) {
            // If successful, read event
            if (gpiod_line_event_read(info->op_done_line, &event) != 0) {
                if (info->isVerbose)
                    printf("EMIO error reading event for %s quadlet %d\n", opType, num);
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
           while ((val <= 0) && (dt < 250.0)) {
               val = gpiod_line_get_value(info->op_done_line);
               GetCurTime(&curTime);
               dt = TimeDiff_us(&waitStart, &curTime);
           }
           if (info->isVerbose) {
               if (val < 0)
                   printf("EMIO error polling op_done for %s quadlet %d\n", opType, num);
               else if (val == 0)
                   printf("EMIO polling timeout waiting for %s quadlet %d\n", opType, num);
               else
                   printf("Waited %lf us for %s quadlet %d\n", dt, opType, num);
           }
        }
        ret = (val == 1);
    }
    return ret;
}

// EMIO_ReadQuadlet: read quadlet from specified address
bool EMIO_ReadQuadlet(struct EMIO_Info *info, uint16_t addr, uint32_t *data)
{
    int reg_rdata_values[32];
    int reg_addr_values[16];
    size_t i;

    // For timing measurements
    fpgav3_time_t beforeWait;
    fpgav3_time_t afterWait;

    if (!info) {
        printf("EMIO_ReadQuadlet: null pointer\n");
        return false;
    }

    // Get start time for measurement
    if (info->doTiming > 0)
        GetCurTime(&info->startTime);

    for (i = 0; i < 16; i++)
        reg_addr_values[i] = (addr&(0x8000>>i)) ? 1 : 0;

    // Set all data lines to input
    if (!info->isInput) {
        if (gpiod_line_set_direction_input_bulk(&info->reg_data_lines) != 0) {
            printf("EMIO_ReadQuadlet: could not set data lines as input\n");
            return false;
        }
        info->isInput = true;
    }

    // Write reg_addr (read address)
    gpiod_line_set_value_bulk(&info->reg_addr_lines, reg_addr_values);
    // Set ctrl_lines to 0
    gpiod_line_set_value_bulk(&info->ctrl_lines, ctrl_zero);
    // Set req_bus to 1 (rising edge requests firmware to read register)
    gpiod_line_set_value(info->req_bus_line, 1);

    // Get time before wait
    if (info->doTiming > 1)
        GetCurTime(&beforeWait);

    if (!WaitOpDone(info, "read", 0)) {
        gpiod_line_set_value(info->req_bus_line, 0);
        return false;
    }

    // Get time after wait
    if (info->doTiming > 1)
        GetCurTime(&afterWait);

    // Read data from reg_data
    bool ret = gpiod_line_get_value_bulk(&info->reg_data_lines, reg_rdata_values);

    // Set req_bus to 0
    gpiod_line_set_value(info->req_bus_line, 0);

    if (ret != 0) {
        if (info->isVerbose)
            printf("EMIO_ReadQuadlet: error reading data lines\n");
        return false;
    }

    *data = 0;
    for (i = 0; i < 32; i++)
        *data = (*data<<1)|reg_rdata_values[i];

    // Get end time
    if (info->doTiming > 0) {
        GetCurTime(&info->endTime);
        double dt = TimeDiff_us(&info->startTime, &info->endTime)-info->timingOverhead;
        if (info->doTiming > 1) dt -= 2*info->timingOverhead;
        printf("ReadQuadlet total time = %lf us\n", dt);
        if (info->doTiming > 1) {
            printf("Start-wait-end times = %lf, %lf, %lf us\n",
                   TimeDiff_us(&info->startTime, &beforeWait)-info->timingOverhead,
                   TimeDiff_us(&beforeWait, &afterWait)-info->timingOverhead,
                   TimeDiff_us(&afterWait, &info->endTime)-info->timingOverhead);
        }
    }

    return true;
}

// EMIO_WriteQuadlet: write quadlet to specified address
bool EMIO_WriteQuadlet(struct EMIO_Info *info, uint16_t addr, uint32_t data)
{
    int reg_wdata_values[32];
    int reg_addr_values[16];
    size_t i;

    // For timing measurements
    fpgav3_time_t beforeWait;
    fpgav3_time_t afterWait;

    if (!info) {
        printf("EMIO_WriteQuadlet: null pointer\n");
        return false;
    }

    // Get start time for measurement
    if (info->doTiming > 0)
        GetCurTime(&info->startTime);

    for (i = 0; i < 16; i++)
        reg_addr_values[i] = (addr&(0x8000>>i)) ? 1 : 0;

    for (i = 0; i < 32; i++)
        reg_wdata_values[i] = (data&(0x80000000>>i)) ? 1 : 0;

    if (info->isInput) {
        // Set all data lines to output and write values
        if (gpiod_line_set_direction_output_bulk(&info->reg_data_lines, reg_wdata_values) != 0) {
            printf("EMIO_WriteQuadlet: could not set data lines as output\n");
            return false;
        }
        info->isInput = false;
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
    if (info->doTiming > 1)
        GetCurTime(&beforeWait);

    // op_done should be set quickly by firmware, to indicate
    // that write has completed
    bool ret = WaitOpDone(info, "write", 0);

    // Get time after wait
    if (ret && info->doTiming > 1)
        GetCurTime(&afterWait);

    // Set req_bus (and other ctrl lines) to 0
    gpiod_line_set_value(info->req_bus_line, 0);
    gpiod_line_set_value_bulk(&info->ctrl_lines, ctrl_zero);

    // Get end time
    if (ret && (info->doTiming > 0)) {
        GetCurTime(&info->endTime);
        double dt = TimeDiff_us(&info->startTime, &info->endTime)-info->timingOverhead;
        if (info->doTiming > 1) dt -= 2*info->timingOverhead;
        printf("WriteQuadlet total time = %lf us\n", dt);
        if (info->doTiming > 1) {
            printf("Start-wait-end times = %lf, %lf, %lf us\n",
                   TimeDiff_us(&info->startTime, &beforeWait)-info->timingOverhead,
                   TimeDiff_us(&beforeWait, &afterWait)-info->timingOverhead,
                   TimeDiff_us(&afterWait, &info->endTime)-info->timingOverhead);
        }
    }

    return ret;
}

// Real-time block read
//
// The real-time block read (i.e., read from address 0) is handled as a special case.
// It assumes Firmware Rev 8 (since older firmware versions are not supported on Zynq).
// This implementation assumes the following based on the number of quadlets:
//       nQuads = 4 + 2*NumMotors + 5*NumEncoders
// BCFG: NumMotors = NumEncoders = 0     --> nQuads = 4
// QLA:  NumMotors = NumEncoders = 4     --> nQuads = 32
// DQLA: NumMotors = NumEncoders = 8     --> nQuads = 60
// DRAC: NumMotors = 10, NumEncoders = 7 --> nQuads = 59
//
static bool EMIO_ReadBlockRt(struct EMIO_Info *info, uint32_t *data, unsigned int nQuads)
{
    unsigned int i, j;
    unsigned int nMotors;
    unsigned int nEncoders;

    if (nQuads == 59) {
        nMotors = 10;
        nEncoders = 7;
    }
    else {
        // No error checking (if nQuads is not 4, 32 or 60)
        nMotors = (nQuads-4)/7;
        nEncoders = nMotors;
    }
    unsigned int idx = 0;
    data[idx++] = 0;         // Cannot read timestamp
    uint32_t val;
    if (!EMIO_ReadQuadlet(info,  0, &val))  // Status
        return false;
    data[idx++] = bswap_32(val);
    if (!EMIO_ReadQuadlet(info, 10, &val))  // Digital I/O
        return false;
    data[idx++] = bswap_32(val);
    if (!EMIO_ReadQuadlet(info,  5, &val))  // Temperature
        return false;
    data[idx++] = bswap_32(val);
    for (i = 0; i < nMotors; i++) {
        // Register 0: ADC (pot+cur)
        if (!EMIO_ReadQuadlet(info,  ((i+1)<<4), &val))
            return false;
        data[idx++] = bswap_32(val);
    }
    for (j= 0; j < 5; j++) {
        // Encoder register offsets: 5,6,7,9,10
        uint16_t off = (j < 3) ? (5 + j) : (6 + j);
        for (i = 0; i < nEncoders; i++) {
            if (!EMIO_ReadQuadlet(info,  ((i+1)<<4)|off, &val))
                return false;
            data[idx++] = bswap_32(val);
        }
    }
    for (i = 0; i < nMotors; i++) {
        // Register 12: Motor Status
        if (!EMIO_ReadQuadlet(info,  ((i+1)<<4)|12, &val))
            return false;
        data[idx++] = bswap_32(val);
    }
    return true;
}

// Read block of data
//
bool EMIO_ReadBlock(struct EMIO_Info *info, uint16_t addr, uint32_t *data, unsigned int nBytes)
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

    if (!info) {
        printf("EMIO_ReadBlock: null pointer\n");
        return false;
    }

    if (info->version == 0) {
        // Local startTime and endTime because ReadQuadlet will overwrite info struct
        fpgav3_time_t startTime;
        fpgav3_time_t endTime;

        if (info->doTiming > 0)
            GetCurTime(&startTime);

        bool ret;
        if (addr == 0) {
            ret = EMIO_ReadBlockRt(info, data, nQuads);
        }
        else {
            ret = true;
            for (i = 0; i < nQuads; i++) {
                if (EMIO_ReadQuadlet(info, addr+i, &val)) {
                    data[i] = bswap_32(val);
                }
                else {
                    ret = false;
                    break;
                }
            }
        }
        if (ret && (info->doTiming > 0)) {
            GetCurTime(&endTime);
            printf("ReadBlock (version 0) total time = %lf us\n",
                   TimeDiff_us(&startTime, &endTime)-info->timingOverhead);
        }
        return ret;
    }

    // Get start time for measurement
    if (info->doTiming > 0)
        GetCurTime(&info->startTime);

    // Set all data lines to input
    if (!info->isInput) {
        if (gpiod_line_set_direction_input_bulk(&info->reg_data_lines) != 0) {
            printf("EMIO_ReadBlock: could not set data lines as input\n");
            return false;
        }
        info->isInput = true;
    }

    // Set blk_start to 1
    ctrl_values[CTRL_REG_WEN] = 0;
    ctrl_values[CTRL_BLK_START] = 1;
    ctrl_values[CTRL_BLK_END] = 0;
    if (gpiod_line_set_value_bulk(&info->ctrl_lines, ctrl_values) != 0) {
        if (info->isVerbose)
            printf("EMIO_ReadBlock: error setting ctrl (blk_start)\n");
        return false;
    }

    // Initialize reg_addr_values
    for (i = 0; i < 16; i++)
        reg_addr_values[i] = (addr&(0x8000>>i)) ? 1 : 0;

    // Write reg_addr (read address)
    if (gpiod_line_set_value_bulk(&info->reg_addr_lines, reg_addr_values) != 0) {
        if (info->isVerbose)
            printf("EMIO_ReadBlock: error setting initial address %x\n", addr);
        gpiod_line_set_value_bulk(&info->ctrl_lines, ctrl_zero);
        return false;
    }
    // Make sure addr_lsb_line is set to reg_addr_values[15] (lsb)
    gpiod_line_set_value(info->addr_lsb_line, reg_addr_values[15]);

    // Set req_bus
    if (gpiod_line_set_value(info->req_bus_line, 1) != 0) {
        if (info->isVerbose)
            printf("EMIO_ReadBlock: error setting req_bus\n");
        gpiod_line_set_value_bulk(&info->ctrl_lines, ctrl_zero);
        return false;
    }

    if (info->doTiming > 1)
        GetCurTime(&firstRead);

    for (q = 0; q < nQuads; q++) {

        if (q > 0) {
            if (q == nQuads-1) {
                // Set blk_end to 1 to indicate end of block write
                ctrl_values[CTRL_BLK_START] = 0;
                ctrl_values[CTRL_BLK_END] = 1;
                if (gpiod_line_set_value_bulk(&info->ctrl_lines, ctrl_values) != 0) {
                    if (info->isVerbose)
                        printf("EMIO_ReadBlock: error setting ctrl (blk_end)\n");
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
                if (info->isVerbose)
                    printf("EMIO_ReadBlock: error setting address for quadlet %d\n", q);
                gpiod_line_set_value(info->req_bus_line, 0);
                gpiod_line_set_value_bulk(&info->ctrl_lines, ctrl_zero);
                return false;
            }
        }

        // op_done set by firmware, to indicate that quadlet
        // has been read
        if (!WaitOpDone(info, "read", q)) {
            gpiod_line_set_value(info->req_bus_line, 0);
            gpiod_line_set_value_bulk(&info->ctrl_lines, ctrl_zero);
            return false;
        }

        // Read data from reg_data
        if (gpiod_line_get_value_bulk(&info->reg_data_lines, reg_rdata_values) != 0) {
            if (info->isVerbose)
                printf("EMIO_ReadBlock: error reading quadlet %d (of %d)\n", q, nQuads);
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

    if (info->doTiming > 1)
        GetCurTime(&lastRead);

    // Set all lines to 0
    gpiod_line_set_value(info->req_bus_line, 0);
    gpiod_line_set_value_bulk(&info->ctrl_lines, ctrl_zero);

    // Get end time
    if (info->doTiming > 0) {
        GetCurTime(&info->endTime);
        double dt = TimeDiff_us(&info->startTime, &info->endTime)-info->timingOverhead;
        if (info->doTiming > 1) dt -= 2*info->timingOverhead;
        printf("ReadBlock total time = %lf us\n", dt);
        if (info->doTiming > 1) {
            printf("Start-loop-end times = %lf, %lf, %lf us\n",
                   TimeDiff_us(&info->startTime, &firstRead)-info->timingOverhead,
                   TimeDiff_us(&firstRead, &lastRead)-info->timingOverhead,
                   TimeDiff_us(&lastRead, &info->endTime)-info->timingOverhead);
        }
    }

    return true;
}

// Real-time block write
//
// The real-time block write (i.e., write to address 0) is handled as a special case.
// It assumes Firmware Rev 8 (since older firmware versions are not supported on Zynq)
// and does not check validity of the real-time block write header.
// It essentially replicates code from WriteRtData.v
//
// Due to the poor support for block write via EMIO, and the possibility of bus conflicts
// with Firewire or Ethernet, this implementation instead uses multiple quadlet writes.
//
static bool EMIO_WriteBlockRt(struct EMIO_Info *info, uint32_t *data, unsigned int nQuads)
{
    unsigned int i;

    // Check number of quadlets specified in header
    // We should also check board id
    unsigned int headerQuads = bswap_32(data[0])&0x000000ff;
    if (headerQuads != nQuads) {
        printf("EMIO_WriteBlockRt: header quadlets %d does not match expected value %d\n",
               headerQuads, nQuads);
        return false;
    }

    bool noDacWrite = true;
    for (i = 1; i < nQuads-1; i++) {
        // Write out DAC values, if DAC valid bit (31) or
        // AmpEn mask (29) are set
        uint32_t dac = bswap_32(data[i]);
        if (dac&0xa0000000) {
            noDacWrite = false;
            if (!EMIO_WriteQuadlet(info, ((i<<4)|1), dac))
                return false;
        }
    }
    // The last quadlet is the power control register.
    // We need to write this if the lower 20 bits are non-zero, or
    // if there was no DAC write (to refresh watchdog).
    bool ret = true;
    uint32_t ctrl = bswap_32(data[nQuads-1]);
    if (noDacWrite || ((ctrl&0x000fffff) != 0))
        ret = EMIO_WriteQuadlet(info, 0, ctrl);

    return ret;
}

// Write block of data.
//
bool EMIO_WriteBlock(struct EMIO_Info *info, uint16_t addr, uint32_t *data, unsigned int nBytes)
{
    int reg_wdata_values[32];
    int reg_addr_values[16];
    int ctrl_values[CTRL_MAX];
    unsigned int i, q;

    // For timing measurements
    fpgav3_time_t firstWrite;
    fpgav3_time_t lastWrite;

    if (!info) {
        printf("EMIO_WriteBlock: null pointer\n");
        return false;
    }

    unsigned int nQuads = (nBytes+3)/4;

    if (info->version == 0) {
        bool ret = false;
        if (addr == 0) {
            // Special case for real-time block write
            // Local startTime and endTime because WriteQuadlet will overwrite info struct
            fpgav3_time_t startTime;
            fpgav3_time_t endTime;
            if (info->doTiming > 0)
                GetCurTime(&startTime);

            ret = EMIO_WriteBlockRt(info, data, nQuads);

            if (info->doTiming > 0) {
                GetCurTime(&endTime);
                printf("WriteBlock (version 0) total time = %lf us\n",
                       TimeDiff_us(&startTime, &endTime)-info->timingOverhead);
            }

        }
        else if (info->isVerbose) {
            printf("EMIO_WriteBlock: only real-time block write supported for Version 0\n");
        }
        return ret;
    }

    // Get start time for measurement
    if (info->doTiming > 0)
        GetCurTime(&info->startTime);

    uint32_t val = bswap_32(data[0]);
    for (i = 0; i < 32; i++)
        reg_wdata_values[i] = (val&(0x80000000>>i)) ? 1 : 0;

    if (info->isInput) {
        // Set all data lines to output (and write values)
        if (gpiod_line_set_direction_output_bulk(&info->reg_data_lines, reg_wdata_values) != 0) {
            printf("EMIO_WriteBlock: could not set data lines as output\n");
            return false;
        }
        info->isInput = false;
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
    if (!WaitOpDone(info, "write", 0)) {
        gpiod_line_set_value(info->req_bus_line, 0);
        gpiod_line_set_value_bulk(&info->ctrl_lines, ctrl_zero);
        return false;
    }

    if (info->doTiming > 1)
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
            if (info->isVerbose)
                printf("EMIO_WriteBlock: error setting address for quadlet %d\n", q);
            gpiod_line_set_value(info->req_bus_line, 0);
            gpiod_line_set_value_bulk(&info->ctrl_lines, ctrl_zero);
            return false;
        }

        // op_done set by firmware, to indicate that quadlet
        // has been written
        if (!WaitOpDone(info, "write", q)) {
            gpiod_line_set_value(info->req_bus_line, 0);
            gpiod_line_set_value_bulk(&info->ctrl_lines, ctrl_zero);
            return false;
        }
    }

    if (info->doTiming > 1)
        GetCurTime(&lastWrite);

    // Set all lines to 0
    gpiod_line_set_value(info->req_bus_line, 0);
    gpiod_line_set_value_bulk(&info->ctrl_lines, ctrl_zero);

    // Get end time
    if (info->doTiming > 0) {
        GetCurTime(&info->endTime);
        double dt = TimeDiff_us(&info->startTime, &info->endTime)-info->timingOverhead;
        if (info->doTiming > 1) dt -= 2*info->timingOverhead;
        printf("WriteBlock total time = %lf us\n", dt);
        if (info->doTiming > 1) {
            printf("Start-loop-end times = %lf, %lf, %lf us\n",
                   TimeDiff_us(&info->startTime, &firstWrite)-info->timingOverhead,
                   TimeDiff_us(&firstWrite, &lastWrite)-info->timingOverhead,
                   TimeDiff_us(&lastWrite, &info->endTime)-info->timingOverhead);
        }
    }

    return true;
}

bool EMIO_WritePromData(struct EMIO_Info *info, char *data, unsigned int nBytes)
{
    if (!info) {
        printf("EMIO_WritePromData: null pointer\n");
        return false;
    }

    // Round up to nearest multiple of 4
    uint16_t nQuads = (nBytes+3)/4;
    for (uint16_t i = 0; i < nQuads; i++) {
        uint32_t qdata = bswap_32(*(uint32_t *)(data+i*4));
        if (!EMIO_WriteQuadlet(info, 0x2000+i, qdata))
            return false;
    }
    return true;
}
