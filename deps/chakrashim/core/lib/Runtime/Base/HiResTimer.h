//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once

namespace Js
{
    class HiResTimer
    {

    private:
        double dBaseTime;
        double dLastTime;
        double dAdjustFactor;
        uint64 baseMsCount;
        uint64 freq;

        bool fReset;
        bool fInit;
        bool fHiResAvailable;

        double GetAdjustFactor();
    public:
        HiResTimer(): fInit(false), dBaseTime(0), baseMsCount(0),  fHiResAvailable(true), dLastTime(0), dAdjustFactor(1), fReset(true) {}
        double Now();
        void Reset() { fReset = true; }
        static double GetSystemTime();
    };
} // namespace Js.
