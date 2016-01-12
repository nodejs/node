//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once

namespace UnifiedRegex
{
    enum RegexFlags : uint8
    {
        NoRegexFlags        = 0,
        IgnoreCaseRegexFlag = 1 << 0,
        GlobalRegexFlag     = 1 << 1,
        MultilineRegexFlag  = 1 << 2,
        UnicodeRegexFlag    = 1 << 3,
        StickyRegexFlag     = 1 << 4,
        AllRegexFlags       = (1 << 5) - 1
    };
}
