//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "CommonCommonPch.h"
#include "Common\Tick.h"

namespace Js {
    uint64      Tick::s_luFreq;
    uint64      Tick::s_luBegin;

#if DBG
    uint64      Tick::s_DEBUG_luStart   = 0;
    uint64      Tick::s_DEBUG_luSkip    = 0;
#endif

    ///----------------------------------------------------------------------------
    ///----------------------------------------------------------------------------
    ///
    /// struct Tick
    ///
    ///----------------------------------------------------------------------------
    ///----------------------------------------------------------------------------

    ///----------------------------------------------------------------------------
    ///
    /// Tick::Tick
    ///
    /// Tick() initializes a new Tick instance to an "empty" time.  This instance
    /// must be assigned to another Tick instance or Now() to have value.
    ///
    ///----------------------------------------------------------------------------

    Tick::Tick()
    {
        m_luTick = 0;
    }


    ///----------------------------------------------------------------------------
    ///
    /// Tick::Tick
    ///
    /// Tick() initializes a new Tick instance to a specific time, in native
    /// time units.
    ///
    ///----------------------------------------------------------------------------

    Tick::Tick(
        uint64 luTick)                      // Tick, in internal units
    {
        m_luTick = luTick;
    }


    ///----------------------------------------------------------------------------
    ///
    /// Tick::FromMicroseconds
    ///
    /// FromMicroseconds() returns a Tick instance from a given time in
    /// microseconds.
    ///
    ///----------------------------------------------------------------------------

    Tick
    Tick::FromMicroseconds(
        uint64 luTime)                          // Time, in microseconds
    {
        //
        // Ensure we can convert losslessly.
        //

#if DBG
        const uint64 luMaxTick = _UI64_MAX / s_luFreq;
        AssertMsg(luTime <= luMaxTick, "Ensure time can be converted losslessly");
#endif // DBG


        //
        // Create the Tick
        //

        uint64 luTick = luTime * s_luFreq / ((uint64) 1000000);
        return Tick(luTick);
    }


    ///----------------------------------------------------------------------------
    ///
    /// Tick::FromQPC
    ///
    /// FromQPC() returns a Tick instance from a given QPC time.
    ///
    ///----------------------------------------------------------------------------

    Tick
    Tick::FromQPC(
        uint64 luTime)                      // Time, in QPC units
    {
        return Tick(luTime - s_luBegin);
    }


    ///----------------------------------------------------------------------------
    ///
    /// Tick::ToQPC
    ///
    /// ToQPC() returns the QPC time for this time instance
    ///
    ///----------------------------------------------------------------------------

    uint64
    Tick::ToQPC()
    {
        return (m_luTick + s_luBegin);
    }


    ///----------------------------------------------------------------------------
    ///
    /// Tick::operator +
    ///
    /// operator +()
    ///
    ///----------------------------------------------------------------------------

    Tick
    Tick::operator +(
        TickDelta tdChange                  // RHS TickDelta
        ) const
    {
        return Tick(m_luTick + tdChange.m_lnDelta);
    }


    ///----------------------------------------------------------------------------
    ///
    /// Tick::operator -
    ///
    /// operator -()
    ///
    ///----------------------------------------------------------------------------

    Tick
    Tick::operator -(
        TickDelta tdChange                  // RHS TickDelta
        ) const
    {
        return Tick(m_luTick - tdChange.m_lnDelta);
    }


    ///----------------------------------------------------------------------------
    ///
    /// Tick::operator -
    ///
    /// operator -()
    ///
    ///----------------------------------------------------------------------------

    TickDelta
    Tick::operator -(
        Tick timeOther                      // RHS Tick
        ) const
    {
        return TickDelta(m_luTick - timeOther.m_luTick);
    }


    ///----------------------------------------------------------------------------
    ///
    /// Tick::operator ==
    ///
    /// operator ==()
    ///
    ///----------------------------------------------------------------------------

    bool
    Tick::operator ==(
        Tick timeOther                      // RHS Tick
        ) const
    {
        return m_luTick == timeOther.m_luTick;
    }


    ///----------------------------------------------------------------------------
    ///
    /// Tick::operator !=
    ///
    /// operator !=()
    ///
    ///----------------------------------------------------------------------------

    bool
    Tick::operator !=(
        Tick timeOther                      // RHS Tick
        ) const
    {
        return m_luTick != timeOther.m_luTick;
    }


    ///----------------------------------------------------------------------------
    ///
    /// Tick::operator <
    ///
    /// operator <()
    ///
    ///----------------------------------------------------------------------------

    bool
    Tick::operator <(
        Tick timeOther                      // RHS Tick
        ) const
    {
        return m_luTick < timeOther.m_luTick;
    }


    ///----------------------------------------------------------------------------
    ///
    /// Tick::operator <=
    ///
    /// operator <=()
    ///
    ///----------------------------------------------------------------------------

    bool
    Tick::operator <=(
        Tick timeOther                      // RHS Tick
        ) const
    {
        return m_luTick <= timeOther.m_luTick;
    }


    ///----------------------------------------------------------------------------
    ///
    /// Tick::operator >
    ///
    /// operator >()
    ///
    ///----------------------------------------------------------------------------

    bool
    Tick::operator >(
        Tick timeOther                      // RHS Tick
        ) const
    {
        return m_luTick > timeOther.m_luTick;
    }


    ///----------------------------------------------------------------------------
    ///
    /// Tick::operator >=
    ///
    /// operator >=()
    ///
    ///----------------------------------------------------------------------------

    bool
    Tick::operator >=(
        Tick timeOther                      // RHS Tick
        ) const
    {
        return m_luTick >= timeOther.m_luTick;
    }


    ///----------------------------------------------------------------------------
    ///----------------------------------------------------------------------------
    ///
    /// struct TickDelta
    ///
    ///----------------------------------------------------------------------------
    ///----------------------------------------------------------------------------

    ///----------------------------------------------------------------------------
    ///
    /// TickDelta::TickDelta
    ///
    /// TickDelta() initializes a new TickDelta instance to "zero" delta.
    ///
    ///----------------------------------------------------------------------------

    TickDelta::TickDelta()
    {
        m_lnDelta = 0;
    }


    ///----------------------------------------------------------------------------
    ///
    /// TickDelta::TickDelta
    ///
    /// TickDelta() initializes a new TickDelta instance to a specific time delta,
    /// in native time units.
    ///
    ///----------------------------------------------------------------------------


    TickDelta::TickDelta(
        int64 lnDelta)
    {
        m_lnDelta = lnDelta;
    }


    ///----------------------------------------------------------------------------
    ///
    /// TickDelta::ToMicroseconds
    ///
    /// ToMicroseconds() returns the time delta, in microseconds. The time is
    /// rounded to the nearest available whole units.
    ///
    ///----------------------------------------------------------------------------

    int64
    TickDelta::ToMicroseconds() const
    {
        if (*this == Infinite())
        {
            return _I64_MAX;
        }

        //
        // Ensure we can convert losslessly.
        //

        const int64 lnMinTimeDelta = _I64_MIN / ((int64) 1000000);
        const int64 lnMaxTimeDelta = _I64_MAX / ((int64) 1000000);
        AssertMsg((m_lnDelta <= lnMaxTimeDelta) && (m_lnDelta >= lnMinTimeDelta),
                "Ensure delta can be converted to microseconds losslessly");


        //
        // Compute the microseconds.
        //

        int64 lnFreq = (int64) Tick::s_luFreq;
        int64 lnTickDelta = (m_lnDelta * ((int64) 1000000)) / lnFreq;
        return lnTickDelta;
    }


    ///----------------------------------------------------------------------------
    ///
    /// TickDelta::FromMicroseconds
    ///
    /// FromMicroseconds() returns a TickDelta instance from a given delta in
    /// microseconds.
    ///
    ///----------------------------------------------------------------------------

    TickDelta
    TickDelta::FromMicroseconds(
        int64 lnTimeDelta)                  // Time delta, in 1/1000^2 sec
    {
        AssertMsg(lnTimeDelta != _I64_MAX, "Use Infinite() to create an infinite TickDelta");

        //
        // Ensure that we can convert losslessly.
        //

        int64 lnFreq = (int64) Tick::s_luFreq;

#if DBG
        const int64 lnMinTimeDelta = _I64_MIN / lnFreq;
        const int64 lnMaxTimeDelta = _I64_MAX / lnFreq;
        AssertMsg((lnTimeDelta <= lnMaxTimeDelta) && (lnTimeDelta >= lnMinTimeDelta),
                "Ensure delta can be converted to native format losslessly");
#endif // DBG


        //
        // Create the TickDelta
        //

        int64 lnTickDelta = (lnTimeDelta * lnFreq) / ((int64) 1000000);
        TickDelta td(lnTickDelta);

        AssertMsg(td != Infinite(), "Can not create infinite TickDelta");
        return td;
    }


    ///----------------------------------------------------------------------------
    ///
    /// TickDelta::FromMicroseconds
    ///
    /// FromMicroseconds() returns a TickDelta instance from a given delta in
    /// microseconds.
    ///
    ///----------------------------------------------------------------------------

    TickDelta
    TickDelta::FromMicroseconds(
        int nTimeDelta)                     // Tick delta, in 1/1000^2 sec
    {
        AssertMsg(nTimeDelta != _I32_MAX, "Use Infinite() to create an infinite TickDelta");

        return FromMicroseconds((int64) nTimeDelta);
    }


    ///----------------------------------------------------------------------------
    ///
    /// TickDelta::FromMilliseconds
    ///
    /// FromMilliseconds() returns a TickDelta instance from a given delta in
    /// milliseconds.
    ///
    ///----------------------------------------------------------------------------

    TickDelta
    TickDelta::FromMilliseconds(
        int nTimeDelta)                     // Tick delta, in 1/1000^1 sec
    {
        AssertMsg(nTimeDelta != _I32_MAX, "Use Infinite() to create an infinite TickDelta");

        return FromMicroseconds(((int64) nTimeDelta) * ((int64) 1000));
    }


    ///----------------------------------------------------------------------------
    ///
    /// TickDelta::Infinite
    ///
    /// Infinite() returns a time-delta infinitely far away.
    ///
    ///----------------------------------------------------------------------------

    TickDelta
    TickDelta::Infinite()
    {
        return TickDelta(_I64_MAX);
    }


    ///----------------------------------------------------------------------------
    ///
    /// TickDelta::IsForward
    ///
    /// IsForward() returns whether adding this TickDelta to a given Tick will
    /// not move the time backwards.
    ///
    ///----------------------------------------------------------------------------

    bool
    TickDelta::IsForward() const
    {
        return m_lnDelta >= 0;
    }


    ///----------------------------------------------------------------------------
    ///
    /// TickDelta::IsBackward
    ///
    /// IsBackward() returns whether adding this TickDelta to a given Tick will
    /// not move the time forwards.
    ///
    ///----------------------------------------------------------------------------

    bool
    TickDelta::IsBackward() const
    {
        return m_lnDelta <= 0;
    }


    ///----------------------------------------------------------------------------
    ///
    /// TickDelta::Abs
    ///
    /// Abs() returns the absolute value of the TickDelta.
    ///
    ///----------------------------------------------------------------------------

    TickDelta
    TickDelta::Abs(TickDelta tdOther)
    {
        return TickDelta(tdOther.m_lnDelta < 0 ? -tdOther.m_lnDelta : tdOther.m_lnDelta);
    }


    ///----------------------------------------------------------------------------
    ///
    /// TickDelta::operator %
    ///
    /// operator %()
    ///
    ///----------------------------------------------------------------------------

    TickDelta
    TickDelta::operator %(
        TickDelta tdOther                   // RHS TickDelta
        ) const
    {
        return TickDelta(m_lnDelta % tdOther.m_lnDelta);
    }


    ///----------------------------------------------------------------------------
    ///
    /// TickDelta::operator \
    ///
    /// operator \() - Divides one TickDelta by another, in TickDelta units
    ///
    ///----------------------------------------------------------------------------

    int64
    TickDelta::operator /(
        TickDelta tdOther                   // RHS TickDelta
        ) const
    {
        return m_lnDelta / tdOther.m_lnDelta;
    }


    ///----------------------------------------------------------------------------
    ///
    /// TickDelta::operator +
    ///
    /// operator +()
    ///
    ///----------------------------------------------------------------------------

    TickDelta
    TickDelta::operator +(
        TickDelta tdOther                   // RHS TickDelta
        ) const
    {
        AssertMsg((*this != Infinite()) && (tdOther != Infinite()),
                "Can not combine infinite TickDeltas");

        return TickDelta(m_lnDelta + tdOther.m_lnDelta);
    }


    ///----------------------------------------------------------------------------
    ///
    /// TickDelta::operator -
    ///
    /// operator -()
    ///
    ///----------------------------------------------------------------------------

    TickDelta
    TickDelta::operator -(
        TickDelta tdOther                   // RHS TickDelta
        ) const
    {
        AssertMsg((*this != Infinite()) && (tdOther != Infinite()),
                "Can not combine infinite TickDeltas");

        return TickDelta(m_lnDelta - tdOther.m_lnDelta);
    }


    ///----------------------------------------------------------------------------
    ///
    /// TickDelta::operator *
    ///
    /// operator *()
    ///
    ///----------------------------------------------------------------------------

    TickDelta
    TickDelta::operator *(
        int nScale                          // RHS scale
        ) const
    {
        AssertMsg(*this != Infinite(), "Can not combine infinite TickDeltas");

        return TickDelta(m_lnDelta * nScale);
    }


    ///----------------------------------------------------------------------------
    ///
    /// TickDelta::operator *
    ///
    /// operator *()
    ///
    ///----------------------------------------------------------------------------

    TickDelta
    TickDelta::operator *(
        float flScale                       // RHS scale
        ) const
    {
        AssertMsg(*this != Infinite(), "Can not combine infinite TickDeltas");

        return TickDelta((int64) (((double) m_lnDelta) * ((double) flScale)));
    }


    ///----------------------------------------------------------------------------
    ///
    /// TickDelta::operator /
    ///
    /// operator /()
    ///
    ///----------------------------------------------------------------------------

    TickDelta
    TickDelta::operator /(
        int nScale                          // RHS scale
        ) const
    {
        AssertMsg(*this != Infinite(), "Can not combine infinite TickDeltas");
        AssertMsg(nScale != 0, "Can not scale by 0");

        return TickDelta(m_lnDelta / nScale);
    }


    ///----------------------------------------------------------------------------
    ///
    /// TickDelta::operator /
    ///
    /// operator /()
    ///
    ///----------------------------------------------------------------------------

    TickDelta
    TickDelta::operator /(
        float flScale                       // RHS scale
        ) const
    {
        AssertMsg(*this != Infinite(), "Can not combine infinite TickDeltas");
        AssertMsg(flScale != 0, "Can not scale by 0");

        return TickDelta((int64) (((double) m_lnDelta) / ((double) flScale)));
    }


    ///----------------------------------------------------------------------------
    ///
    /// TickDelta::operator +=
    ///
    /// operator +=()
    ///
    ///----------------------------------------------------------------------------

    TickDelta
    TickDelta::operator +=(
        TickDelta tdOther)                  // RHS TickDelta
    {
        AssertMsg((*this != Infinite()) && (tdOther != Infinite()),
                "Can not combine infinite TickDeltas");

        m_lnDelta = m_lnDelta + tdOther.m_lnDelta;

        return *this;
    }


    ///----------------------------------------------------------------------------
    ///
    /// TickDelta::operator -=
    ///
    /// operator -=()
    ///
    ///----------------------------------------------------------------------------

    TickDelta
    TickDelta::operator -=(
        TickDelta tdOther)                  // RHS TickDelta
    {
        AssertMsg((*this != Infinite()) && (tdOther != Infinite()),
                "Can not combine infinite TickDeltas");

        m_lnDelta = m_lnDelta - tdOther.m_lnDelta;

        return *this;
    }


    ///----------------------------------------------------------------------------
    ///
    /// TickDelta::operator ==
    ///
    /// operator ==()
    ///
    ///----------------------------------------------------------------------------

    bool
    TickDelta::operator ==(
        TickDelta tdOther                   // RHS TickDelta
        ) const
    {
        return m_lnDelta == tdOther.m_lnDelta;
    }


    ///----------------------------------------------------------------------------
    ///
    /// TickDelta::operator !=
    ///
    /// operator !=()
    ///
    ///----------------------------------------------------------------------------

    bool
    TickDelta::operator !=(
        TickDelta tdOther                   // RHS TickDelta
        ) const
    {
        return m_lnDelta != tdOther.m_lnDelta;
    }


    ///----------------------------------------------------------------------------
    ///
    /// TickDelta::operator <
    ///
    /// operator <()
    ///
    ///----------------------------------------------------------------------------

    bool
    TickDelta::operator <(
        TickDelta tdOther                   // RHS TickDelta
        ) const
    {
        return m_lnDelta < tdOther.m_lnDelta;
    }


    ///----------------------------------------------------------------------------
    ///
    /// TickDelta::operator <=
    ///
    /// operator <=()
    ///
    ///----------------------------------------------------------------------------

    bool
    TickDelta::operator <=(
        TickDelta tdOther                   // RHS TickDelta
        ) const
    {
        return m_lnDelta <= tdOther.m_lnDelta;
    }


    ///----------------------------------------------------------------------------
    ///
    /// TickDelta::operator >
    ///
    /// operator >()
    ///
    ///----------------------------------------------------------------------------

    bool
    TickDelta::operator >(
        TickDelta tdOther                   // RHS TickDelta
        ) const
    {
        return m_lnDelta > tdOther.m_lnDelta;
    }


    ///----------------------------------------------------------------------------
    ///
    /// TickDelta::operator >=
    ///
    /// operator >=()
    ///
    ///----------------------------------------------------------------------------

    bool
    TickDelta::operator >=(
        TickDelta tdOther                   // RHS TickDelta
        ) const
    {
        return m_lnDelta >= tdOther.m_lnDelta;
    }
    void Tick::InitType()
    {
        /* CheckWin32( */ QueryPerformanceFrequency((LARGE_INTEGER *) &s_luFreq);
        /* CheckWin32( */ QueryPerformanceCounter((LARGE_INTEGER *) &s_luBegin);

#if DBG
        s_luBegin += s_DEBUG_luStart;
#endif


        //
        // Ensure that we have a sufficient amount of time so that we can handle useful time operations.
        //

        uint64 nSec = _UI64_MAX / s_luFreq;
        if (nSec < 5 * 60)
        {
#if FIXTHIS
            PromptInvalid("QueryPerformanceFrequency() will not provide at least 5 minutes");
            return Results::GenericFailure;
#endif
        }
    }

    Tick Tick::Now()
    {
        // Determine our current time
        uint64 luCurrent = s_luBegin;
        /* Verify( */ QueryPerformanceCounter((LARGE_INTEGER *) &luCurrent);

#if DBG
        luCurrent += s_DEBUG_luStart + s_DEBUG_luSkip;
#endif

        // Create a Tick instance, using our delta since we started tracking time.
        uint64 luDelta = luCurrent - s_luBegin;
        return Tick(luDelta);
    }

    uint64 Tick::ToMicroseconds() const
    {
        //
        // Convert time in microseconds (1 / 1000^2).  Because of the finite precision and wrap-around,
        // this math depends on where the Tick is.
        //

        const uint64 luOneSecUs = (uint64) 1000000;
        const uint64 luSafeTick = _UI64_MAX / luOneSecUs;
        if (m_luTick < luSafeTick)
        {
            //
            // Small enough to convert directly into microseconds.
            //

            uint64 luTick = (m_luTick * luOneSecUs) / s_luFreq;
            return luTick;
        }
        else
        {
            //
            // Number is too large, so we need to do this is stages.
            // 1. Compute the number of seconds
            // 2. Convert the remainder
            // 3. Add the two parts together
            //

            uint64 luSec    = m_luTick / s_luFreq;
            uint64 luRemain = m_luTick - luSec * s_luFreq;
            uint64 luTick   = (luRemain * luOneSecUs) / s_luFreq;
            luTick         += luSec * luOneSecUs;

            return luTick;
        }
    }

    int TickDelta::ToMilliseconds() const
    {
        if (*this == Infinite())
        {
            return _I32_MAX;
        }

        int64 nTickUs = ToMicroseconds();

        int64 lnRound = 500;
        if (nTickUs < 0)
        {
            lnRound = -500;
        }

        int64 lnDelta = (nTickUs + lnRound) / ((int64) 1000);
        AssertMsg((lnDelta <= INT_MAX) && (lnDelta >= INT_MIN), "Ensure no overflow");

        return (int) lnDelta;
    }

}
