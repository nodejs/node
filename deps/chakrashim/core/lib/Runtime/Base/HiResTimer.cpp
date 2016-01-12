//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "RuntimeBasePch.h"

namespace Js
{
    double HiResTimer::GetSystemTime()
    {
        SYSTEMTIME stTime;
        ::GetSystemTime(&stTime);
        return DateUtilities::TimeFromSt(&stTime);
    }

    // determine if the system time is being adjusted every tick to gradually
    // bring it inline with a time server.
    double HiResTimer::GetAdjustFactor()
    {
        DWORD dwTimeAdjustment = 0;
        DWORD dwTimeIncrement = 0;
        BOOL fAdjustmentDisabled = FALSE;
        BOOL fSuccess = GetSystemTimeAdjustment(&dwTimeAdjustment, &dwTimeIncrement, &fAdjustmentDisabled);
        if (!fSuccess || fAdjustmentDisabled)
        {
            return 1;
        }
        return ((double)dwTimeAdjustment) / ((double)dwTimeIncrement);
    }

    double HiResTimer::Now()
    {
        if(!fHiResAvailable)
        {
            return GetSystemTime();
        }

        if(!fInit)
        {
            if (!QueryPerformanceFrequency((LARGE_INTEGER *) &freq))
            {
                fHiResAvailable = false;
                return GetSystemTime();
            }
            fInit = true;
        }

#if DBG
        uint64 f;
        Assert(QueryPerformanceFrequency((LARGE_INTEGER *)&f) && f == freq);
#endif
        // try better resolution time using perf counters
        uint64 count;
        if( !QueryPerformanceCounter((LARGE_INTEGER *) &count))
        {
            fHiResAvailable = false;
            return GetSystemTime();
        }

        double time = GetSystemTime();

        // there is a base time and count set.
        if (!fReset
            && (count >= baseMsCount))                    // Make sure we don't regress
        {
            double elapsed = ((double)(count - baseMsCount)) * 1000 / freq;

            // if the system time is being adjusted every tick, adjust the
            // precise time delta accordingly.
            if (dAdjustFactor != 1)
            {
                elapsed = elapsed * dAdjustFactor;
            }

            double preciseTime = dBaseTime + elapsed;

            if (fabs(preciseTime - time) < 25              // the time computed via perf counter is off by 25ms
                && preciseTime >= dLastTime)              // the time computed via perf counter is running backwards
            {
                dLastTime = preciseTime;
                return dLastTime;
            }
        }

        //reset
        dBaseTime = time;
        dAdjustFactor = GetAdjustFactor();
        baseMsCount = count;

        double dSinceLast = time - dLastTime;
        if (dSinceLast < -3000)                           // if new time is significantly behind (3s), use it:
        {                                                 // the clock may have been set backwards.
            dLastTime = time;
        }
        else
        {
            dLastTime = max(dLastTime, time);             // otherwise, make sure we don't regress the time.
        }

        fReset = false;
        return dLastTime;
    }
} // namespace Js.
