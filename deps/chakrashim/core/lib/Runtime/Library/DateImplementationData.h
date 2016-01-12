//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
// static DateImplementation data shared by runtime and hybrid debugging

namespace Js {

    const double DateImplementation::ktvMax = +8.64e15;
    const double DateImplementation::ktvMin = -8.64e15;

    // Prototype years. These are years with Jan 1 on the
    // corresponding weekday.
    const int DateImplementation::g_mpytyear[14] =
    {
        1995, 1979, 1991, 1975, 1987, 1971, 1983, // non-leap years
        1984, 1996, 1980, 1992, 1976, 1988, 1972, // leap years
    };

    const int DateImplementation::g_mpytyearpost2006[14] =
    {
        2023, 2035, 2019, 2031, 2015, 2027, 2011, // non-leap years
        2012, 2024, 2036, 2020, 2032, 2016, 2028 // leap years
    };

} // namespace Js
