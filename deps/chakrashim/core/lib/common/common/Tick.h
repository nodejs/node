//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once

namespace Js {

    struct TickDelta;

    ///----------------------------------------------------------------------------
    ///----------------------------------------------------------------------------
    ///
    /// struct Tick
    ///
    /// Tick represents an abstract point in time in some internal units.  This
    /// class is designed to be very fast and very accurate to use, internally
    /// utilizing the system's "high-fidelity" time mechanism.  Tick always
    /// "moves forward", meaning that time values continually increase.
    ///
    /// This class is carefully designed to use two's complement and wrap-around
    /// overflow math.  The entire storage needs to be used, and the internal time
    /// format can not be converted to a different unit, or there will be problems
    /// when the system time wraps around.  Since we are using
    /// QueryPerformanceCounter(), this is variable, but it can happen every 2.5
    /// hours.  This also means that we should never have TickDeltas / arithmetic
    /// that is for any great period of time.
    ///
    ///----------------------------------------------------------------------------
    ///----------------------------------------------------------------------------

    struct Tick
    {
    // Construction
    public:
        static  void            InitType();

                                Tick();
    private:
                                Tick(uint64 luTick);

    // Properties
    public:
                uint64          ToMicroseconds() const;
        static  Tick            FromMicroseconds(uint64 luTick);
        static  Tick            FromQPC(uint64 luQPCTick);

        static  Tick            Now();

                Tick            operator +(TickDelta tdChange) const;
                Tick            operator -(TickDelta tdChange) const;
                TickDelta       operator -(Tick timeOther) const;
                bool            operator ==(Tick timeOther) const;
                bool            operator !=(Tick timeOther) const;
                bool            operator <(Tick timeOther) const;
                bool            operator <=(Tick timeOther) const;
                bool            operator >(Tick timeOther) const;
                bool            operator >=(Tick timeOther) const;

                uint64          ToQPC();

    // Data
    private:
        static  uint64          s_luFreq;           // Frequency
        static  uint64          s_luBegin;          // Beginning time
    #if DBG
        static  uint64          s_DEBUG_luStart;    // Tick start offset for debugging
        static  uint64          s_DEBUG_luSkip;     // Tick skip offset for debugging
    #endif

                uint64          m_luTick;           // Current time sample

        friend TickDelta;
    };


    /***************************************************************************\
    *****************************************************************************
    *
    * struct TickDelta
    *
    * TickDelta represents the measured period between two Ticks. TickDelta values
    * may be combined with each other, and then added to Ticks to create new Ticks.
    *
    *****************************************************************************
    \***************************************************************************/

    struct TickDelta
    {
    // Construction
    public:
                TickDelta();
    private:
                TickDelta(int64 lnDelta);

    // Properties
    public:
                int64           ToMicroseconds() const;
                int             ToMilliseconds() const;
        static  TickDelta       FromMicroseconds(int nTickDelta);
        static  TickDelta       FromMicroseconds(int64 lnTickDelta);
        static  TickDelta       FromMilliseconds(int nTickDelta);
                bool            IsForward() const;
                bool            IsBackward() const;
        static  TickDelta       Infinite();
        static  TickDelta       Abs(TickDelta tdOther);

                TickDelta       operator +(TickDelta tdOther) const;
                TickDelta       operator -(TickDelta tdOther) const;
                TickDelta       operator %(TickDelta tdOther) const;
                int64           operator /(TickDelta tdOther) const;
                TickDelta       operator *(int nScale) const;
                TickDelta       operator *(float flScale) const;
                TickDelta       operator /(int nScale) const;
                TickDelta       operator /(float flScale) const;

                TickDelta       operator +=(TickDelta tdOther);
                TickDelta       operator -=(TickDelta tdOther);

                bool            operator ==(TickDelta tdOther) const;
                bool            operator !=(TickDelta tdOther) const;
                bool            operator <(TickDelta tdOther) const;
                bool            operator <=(TickDelta tdOther) const;
                bool            operator >(TickDelta tdOther) const;
                bool            operator >=(TickDelta tdOther) const;

    // Data
    private:
                int64           m_lnDelta;          // Tick delta

        friend Tick;
    };

} // namespace Js
