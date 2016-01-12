//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "CommonDataStructuresPch.h"
#include "DataStructures\Interval.h"

namespace regex
{
    bool Interval::Includes(int value) const
    {
        return (begin <= value) && (end >= value);
    }

    bool Interval::Includes(Interval other) const
    {
        return (Includes(other.Begin()) && (Includes(other.End())));
    }

    int Interval::CompareTo(Interval other)
    {
        if (begin < other.begin)
        {
            return -1;
        }
        else if (begin == other.begin)
        {
            if (end < other.end)
            {
                return -1;
            }
            else if (end == other.end)
            {
                return 0;
            }
            else
            {
                return 1;
            }
        }
        else
        {
            return 1;
        }
    }

    int Interval::Compare(Interval x, Interval y)
    {
        return x.CompareTo(y);
    }

    bool Interval::Equals(Interval other)
    {
        return CompareTo(other) == 0;
    }

    bool Interval::Equals(Interval x, Interval y)
    {
        return x.CompareTo(y) == 0;
    }

    int Interval::GetHashCode()
    {
        return  _rotl(begin, 7) ^ end;
    }

    int Interval::GetHashCode(Interval item)
    {
        return item.GetHashCode();
    }
}
