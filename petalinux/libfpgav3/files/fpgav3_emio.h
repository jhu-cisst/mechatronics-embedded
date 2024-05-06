/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-    */
/* ex: set filetype=cpp softtabstop=4 shiftwidth=4 tabstop=4 cindent expandtab: */

/*
 * fpgav3_emio (Linux library)
 *
 * This library provides an interface from the Zynq PS to the read and write buses
 * on the FPGA (PL), via the EMIO bits. The read and write buses each consist of
 * a 16-bit address bus and a 32-bit data bus. The EMIO interface uses the same
 * 16 bits for the read/write address and the same 32 bits for the read/write data.
 * The address bus is always output from the PS, whereas the data bus is bidirectional
 * (PS input when reading, PS output when writing).
 *
 * This file defines the base class, EMIO_Interface. See the two derived classes,
 * EMIO_Interface_Gpiod and EMIO_Interface_Mmap.
 */

#ifndef FPGAV3_EMIO_H
#define FPGAV3_EMIO_H

#include <stdint.h>
#include <stdbool.h>
#include <time.h>

#ifdef USE_TIMEOFDAY
typedef struct timeval fpgav3_time_t;
#else
typedef struct timespec fpgav3_time_t;
#endif

class EMIO_Interface
{
protected:

    bool isInput;              // true if data lines are input
    bool isVerbose;            // true if error messages should be printed
    bool useEvents;            // true if using events to wait
    unsigned int doTiming;     // timing mode (0, 1 or 2)
    unsigned int version;      // Version from FPGA
    fpgav3_time_t startTime;   // Start time for measurement
    fpgav3_time_t endTime;     // End time for measurement
    double timingOverhead;     // Overhead due to timing calls
    double timeout_us;         // Timeout in microseconds

 public:

    EMIO_Interface() : isInput(true), isVerbose(false), useEvents(false), doTiming(0),
                       version(0), timingOverhead(0.0), timeout_us(250.0)
    {}

    virtual ~EMIO_Interface()
    {}

    // Returns true if successfully initialized
    virtual bool IsOK() const = 0;

    // Get bus interface version number
    virtual unsigned int GetVersion() const
    { return version; }

    // Get/Set verbose flag (true -> prints messages when waiting for FPGA I/O to complete)
    bool GetVerbose() const
    { return isVerbose; }

    void SetVerbose(bool newState)
    { isVerbose = newState; }

    // Get/Set timeout value in microseconds
    double GetTimeout_us() const
    { return timeout_us; }

    void SetTimeout_us(double new_timeout_us)
    { timeout_us = new_timeout_us; }

    // Get/Set timing mode (true -> measure timing info)
    //   0 --> no timing
    //   1 --> only total time
    //   2 --> total and intermediate times
    unsigned int GetTimingMode() const
    { return doTiming; }

    void SetTimingMode(unsigned int newState);

    // Get/Set event flag (true -> use events instead of polling)
    bool GetEventMode() const
    { return useEvents; }

    virtual void SetEventMode(bool newState)
    { useEvents = newState; }

    // ReadQuadlet
    //   Reads a quadlet (32-bit register) from the FPGA.
    // Parameters:
    //     addr  16-bit register address
    //     data  pointer to location for storing 32-bit data
    // Returns:  true if success
    virtual bool ReadQuadlet(uint16_t addr, uint32_t &data) = 0;

    // WriteQuadlet
    //   Writes a quadlet (32-bit register) to the FPGA.
    // Parameters:
    //     addr  16-bit register address
    //     data  32-bit value to write
    // Returns:  true if success
    virtual bool WriteQuadlet(uint16_t addr, uint32_t data) = 0;

    // ReadBlock
    //   Reads a block of 32-bit data from the FPGA.
    // Parameters:
    //     addr   16-bit register address
    //     data   pointer to location for storing 32-bit data
    //     nBytes number of bytes to read (rounds up to multiple of 4)
    // Returns:  true if success
    virtual bool ReadBlock(uint16_t addr, uint32_t *data, unsigned int nBytes) = 0;

    // WriteBlock
    //   Write a block of 32-bit data to the FPGA.
    // Parameters:
    //     addr   16-bit register address
    //     data   pointer to location that contains 32-bit data
    //     nBytes number of bytes to write (rounds up to multiple of 4)
    // Returns:  true if success
    virtual bool WriteBlock(uint16_t addr, const uint32_t *data, unsigned int nBytes) = 0;

    // EMIO_WritePromData
    //   Writes the specified bytes to the PROM registers on the FPGA
    //   (this is intended to be used to copy the FPGA S/N to the PROM).
    // Parameters:
    //     data    pointer to data
    //     nBytes  number of data bytes (will be rounded up to multiple of 4)
    // Returns:    true if success
    bool WritePromData(char *data, unsigned int nBytes);

protected:

    // Local methods for timing measurements
    static void GetCurTime(fpgav3_time_t *curTime);
    static double TimeDiff_us(fpgav3_time_t *startTime, fpgav3_time_t *endTime);

};

#endif // FPGAV3_EMIO_H
