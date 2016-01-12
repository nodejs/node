//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once

namespace Js
{
    struct PropertyIdArray
    {
        uint32 count;
        bool   hadDuplicates;
        bool   has__proto__; // Only used for object literal
        bool   hasNonSimpleParams;
        PropertyId elements[];
        PropertyIdArray(uint32 count, bool hadDuplicates = false, bool has__proto__ = false, bool hasNonSimpleParams = false) : count(count), hadDuplicates(hadDuplicates), has__proto__(has__proto__), hasNonSimpleParams(hasNonSimpleParams)
        {
        }

        size_t GetDataSize(uint32 extraSlots) const { return sizeof(PropertyIdArray) + sizeof(PropertyId) * (count + extraSlots); }
    };
};
