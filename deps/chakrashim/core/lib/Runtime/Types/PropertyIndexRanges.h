//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

#pragma once

namespace Js
{
    template <typename Ranges>
    struct PropertyIndexRangesBase
    {
        static void VerifySlotCapacity(int requestedCapacity);
    };

    template <typename TPropertyIndex>
    struct PropertyIndexRanges
    {
    };

    template <>
    struct PropertyIndexRanges<PropertyIndex> : public PropertyIndexRangesBase<PropertyIndexRanges<PropertyIndex>>
    {
        static const PropertyIndex MaxValue = Constants::PropertyIndexMax;
        static const PropertyIndex NoSlots = Constants::NoSlot;
    };

    template <>
    struct PropertyIndexRanges<BigPropertyIndex> : public PropertyIndexRangesBase<PropertyIndexRanges<BigPropertyIndex>>
    {
        static const BigPropertyIndex MaxValue = 0x3FFFFFFF;
        static const BigPropertyIndex NoSlots = Constants::NoBigSlot;
    };
};
