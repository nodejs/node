//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once

namespace regex
{
    struct Interval
    {
        int begin;
        int end;

    public:
        Interval(): begin(0), end(0)
        {
        }

        Interval(int start) : begin(start), end(start)
        {
        }

        Interval(int start, int end) : begin(start), end(end)
        {
        }

        inline int Begin() { return begin; }
        inline void Begin(int value) { begin = value; }

        inline int End() { return end; }
        inline void End(int value) { end = value; }

        bool Includes(int value) const;
        bool Includes(Interval other) const;
        int CompareTo(Interval other);
        static int Compare(Interval x, Interval y);
        bool Equals(Interval other);
        static bool Equals(Interval x, Interval y);
        int GetHashCode();
        static int GetHashCode(Interval item);
    };
}
