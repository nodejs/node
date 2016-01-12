//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once

namespace Js {

    typedef uint8 ArgSlot_OneByte;
    typedef uint32 CacheId;
    typedef uint8 CacheId_OneByte;
    typedef uint16 CacheId_TwoByte;
    typedef uint32 RootCacheId;
    typedef uint16 PropertyIdIndexType_TwoByte;
    typedef uint32 PropertyIdIndexType;
#ifdef BYTECODE_BRANCH_ISLAND
    typedef int16 JumpOffset;
    typedef int32 LongJumpOffset;
#else
    typedef int32 JumpOffset;
#endif
    // This is used to estimate when we need to emit long branches
    uint const MaxLayoutSize = 28; // Increase this when we see larger layout

    uint const MaxOpCodeSize = 2;

    enum LayoutSize
    {
        SmallLayout,
        MediumLayout,
        LargeLayout,
    };

    template <LayoutSize layoutSize>
    struct LayoutSizePolicy;

    template <>
    struct LayoutSizePolicy<LargeLayout>
    {
        typedef RegSlot RegSlotType;
        typedef RegSlot RegSlotSType;
        typedef ArgSlot ArgSlotType;
        typedef CacheId CacheIdType;
        typedef PropertyIdIndexType PropertyIdIndexType;
        typedef uint32 UnsignedType;
        static const LayoutSize LayoutEnum = LargeLayout;
        template <typename T>
        static bool Assign(T& dst, T src) { dst = src; return true; }
    };

    typedef LayoutSizePolicy<LargeLayout> LargeLayoutSizePolicy;

    template <>
    struct LayoutSizePolicy<SmallLayout>
    {
        typedef RegSlot_OneByte RegSlotType;
        typedef RegSlot_OneSByte RegSlotSType;
        typedef ArgSlot_OneByte ArgSlotType;
        typedef CacheId_OneByte CacheIdType;
        typedef PropertyIdIndexType_TwoByte PropertyIdIndexType;
        typedef byte UnsignedType;
        static const LayoutSize LayoutEnum = SmallLayout;

        template <typename T1, typename T2>
        static bool Assign(T1& dst, T2 src)
        {
#ifdef BYTECODE_TESTING
            if (Configuration::Global.flags.LargeByteCodeLayout
                || Configuration::Global.flags.MediumByteCodeLayout)
            {
                return false;
            }
#endif
            dst = (T1)src;
            return ((T2)dst == src);
        }
    };

    typedef LayoutSizePolicy<SmallLayout> SmallLayoutSizePolicy;

    template <>
    struct LayoutSizePolicy<MediumLayout>
    {
        typedef RegSlot_TwoByte RegSlotType;
        typedef RegSlot_TwoSByte RegSlotSType;
        typedef ArgSlot_OneByte ArgSlotType;
        typedef CacheId_TwoByte CacheIdType;
        typedef PropertyIdIndexType_TwoByte PropertyIdIndexType;
        typedef uint16 UnsignedType;
        static const LayoutSize LayoutEnum = MediumLayout;

        template <typename T1, typename T2>
        static bool Assign(T1& dst, T2 src)
        {
#ifdef BYTECODE_TESTING
            if (Configuration::Global.flags.LargeByteCodeLayout)
            {
                return false;
            }
#endif
            dst = (T1)src;
            return ((T2)dst == src);
        }
    };

    typedef LayoutSizePolicy<MediumLayout> MediumLayoutSizePolicy;

    struct OpLayoutEmpty
    {
        // Although empty structs are one byte, the Empty layout are not written out.
    };

} // namespace Js
