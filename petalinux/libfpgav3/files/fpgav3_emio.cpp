/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-    */
/* ex: set filetype=cpp softtabstop=4 shiftwidth=4 tabstop=4 cindent expandtab: */

#include <iostream>
#include <byteswap.h>
#include "fpgav3_emio.h"

void EMIO_Interface::SetTimingMode( unsigned int newMode)
{
    timingOverhead = 0.0;
    doTiming = newMode;

    if (doTiming > 0) {
        // Timing calibration
        for (unsigned int i = 0; i < 10; i++) {
            GetCurTime(&startTime);
            GetCurTime(&endTime);
            timingOverhead += TimeDiff_us(&startTime, &endTime);
        }
        timingOverhead /= 10.0;
        if (isVerbose)
            std::cout << "Calibrated timing overhead of " << timingOverhead << " us" << std::endl;
    }
}

bool EMIO_Interface::WritePromData(char *data, unsigned int nBytes)
{
    // Round up to nearest multiple of 4
    uint16_t nQuads = (nBytes+3)/4;
    for (uint16_t i = 0; i < nQuads; i++) {
        uint32_t qdata = bswap_32(*(uint32_t *)(data+i*4));
        if (!WriteQuadlet(0x2000+i, qdata))
            return false;
    }
    return true;
}

// Local method to get current time
void EMIO_Interface::GetCurTime(fpgav3_time_t *curTime)
{
#ifdef USE_TIMEOFDAY
    gettimeofday(&curTime, NULL);
#else
    clock_gettime(CLOCK_MONOTONIC, curTime);
#endif
}

// Local method to compute time differences in microseconds
double EMIO_Interface::TimeDiff_us(fpgav3_time_t *startTime, fpgav3_time_t *endTime)
{
#ifdef USE_TIMEOFDAY
    return (endTime->tv_sec-startTime->tv_sec)*1.0e6 + (endTime->tv_usec-startTime->tv_usec);
#else
    return (endTime->tv_sec-startTime->tv_sec)*1.0e6 + (endTime->tv_nsec-startTime->tv_nsec)*1.0e-3;
#endif
}
