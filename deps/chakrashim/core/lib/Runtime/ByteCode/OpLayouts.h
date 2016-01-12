//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once

namespace Js {

    ///----------------------------------------------------------------------------
    ///
    /// enum OpCode
    ///
    /// OpCode defines the set of p-code instructions available for byte-code.
    ///
    ///----------------------------------------------------------------------------

    enum class OpCode : ushort
    {
#define DEF_OP(x, y, ...) x,
#include "OpCodeList.h"
        MaxByteSizedOpcodes = 255,
#include "ExtendedOpCodeList.h"
        ByteCodeLast,
#if ENABLE_NATIVE_CODEGEN
#include "BackEndOpCodeList.h"
#endif
#undef DEF_OP
        Count  // Number of operations
    };

    inline OpCode operator+(OpCode o1, OpCode o2) { return (OpCode)((uint)o1 + (uint)o2); }
    inline uint operator+(OpCode o1, uint i) { return ((uint)o1 + i); }
    inline uint operator+(uint i, OpCode &o2) { return (i + (uint)o2); }
    inline OpCode operator++(OpCode &o) { return o = (OpCode)(o + 1U); }
    inline OpCode operator++(OpCode &o, int) { OpCode prev_o = o;  o = (OpCode)(o + 1U); return prev_o; }
    inline OpCode operator-(OpCode o1, OpCode o2) { return (OpCode)((uint)o1 - (uint)o2); }
    inline uint operator-(OpCode o1, uint i) { return ((uint)o1 - i); }
    inline uint operator-(uint i, OpCode &o2) { return (i - (uint)o2); }
    inline OpCode operator--(OpCode &o) { return o = (OpCode)(o - 1U); }
    inline OpCode operator--(OpCode &o, int) { return o = (OpCode)(o - 1U); }
    inline uint operator<<(OpCode o1, uint i) { return ((uint)o1 << i); }
    inline OpCode& operator+=(OpCode &o, uint i) { return (o = (OpCode)(o + i)); }
    inline OpCode& operator-=(OpCode &o, uint i) { return (o = (OpCode)(o - i)); }
    inline bool operator==(OpCode &o, uint i) { return ((uint)(o) == i); }
    inline bool operator==(uint i, OpCode &o) { return (i == (uint)(o)); }
    inline bool operator!=(OpCode &o, uint i) { return ((uint)(o) != i); }
    inline bool operator!=(uint i, OpCode &o) { return (i != (uint)(o)); }
    inline bool operator<(OpCode &o, uint i) { return ((uint)(o) < i); }
    inline bool operator<(uint i, OpCode &o) { return (i < (uint)(o)); }
    inline bool operator>(OpCode &o, uint i) { return ((uint)(o) > i); }
    inline bool operator>(uint i, OpCode &o) { return (i > (uint)(o)); }

#if ENABLE_NATIVE_CODEGEN
    inline bool IsSimd128Opcode(OpCode o) { return (o > Js::OpCode::Simd128_Start && o < Js::OpCode::Simd128_End) || (o > Js::OpCode::Simd128_Start_Extend && o < Js::OpCode::Simd128_End_Extend); }
    inline uint Simd128OpcodeCount() { return (uint)(Js::OpCode::Simd128_End - Js::OpCode::Simd128_Start) + 1 + (uint)(Js::OpCode::Simd128_End_Extend - Js::OpCode::Simd128_Start_Extend) + 1; }
#endif

    ///----------------------------------------------------------------------------
    ///
    /// enum OpLayoutType
    ///
    /// OpLayoutType defines a set of layouts available for OpCodes.  These layouts
    /// correspond to "OpLayout" structs defined below, such as "OpLayoutReg1".
    ///
    ///----------------------------------------------------------------------------

    BEGIN_ENUM_UINT(OpLayoutType)
    // This define only one enum for each layout type, but not for each layout variant
#define LAYOUT_TYPE(x) x,
#define LAYOUT_TYPE_WMS LAYOUT_TYPE
#include "LayoutTypes.h"
        Count,
    END_ENUM_UINT()

    ///----------------------------------------------------------------------------
    ///
    /// struct OpLayoutXYZ
    ///
    /// OpLayoutXYZ structs define the standard patterns used to layout each
    /// OpCode's arguments.
    ///
    /// Set up packing:
    /// - Since we are reading / writing from a byte-stream, and we want everything
    ///   to be tightly aligned with no unintended spaces, change to 'byte'
    ///   packing.
    /// - On processors with alignment requirements, this will automatically
    ///   generate read / write code to handle 'unaligned' access.
    ///
    /// - 3/9/10: Changing the layouts to make all fields well-aligned. This involves
    ///   reducing the RegSlot from 4 bytes to 2, increasing the OpCode from 1 byte to 2,
    ///   reordering fields, and adding pads where appropriate. Note that we assume all
    ///   layouts are preceded by a 2-byte op. The orderings and padding will need to
    ///   be revisited for Win64.
    ///
    /// - 5/2: X86-64 alignment: Changing code to expect all opcode layout structs
    ///   to be aligned on 4 byte boundaries (on I386 and x86-64). This aligns all
    ///   members on their natural boundaries except for Call and Regex which require
    ///   padding before the pointers on x86-64.
    ///
    /// - 09/12: Adding one-byte RegSlot based OpLayout. These remove all the paddings to be
    ///   able to compress the size. Also remove paddings for non ARM build
    ///
    /// - 08/22/2011: Removed paddings for ARM & x64 as well as both support unaligned access
    ///   There is still paddings to make sure every opcode starts at 2 byte boundary to avoid
    ///   pathological cases.
    ///   Note: RegSlot is changed to 4 byte instead of 1 byte in this change.
    ///
    ///----------------------------------------------------------------------------

#pragma pack(push, 1)

    template <typename SizePolicy>
    struct OpLayoutT_Reg1        // R0 <- op
    {
        typename SizePolicy::RegSlotType     R0;
    };

    template <typename SizePolicy>
    struct OpLayoutT_Reg1Unsigned1         // R0 <- op
    {
        typename SizePolicy::RegSlotType     R0;
        typename SizePolicy::UnsignedType    C1;
    };

    template <typename SizePolicy>
    struct OpLayoutT_Reg2        // R0 <- op R1
    {
        typename SizePolicy::RegSlotType     R0;
        typename SizePolicy::RegSlotType     R1;
    };

    template <typename SizePolicy>
    struct OpLayoutT_Reg2WithICIndex : public OpLayoutT_Reg2<SizePolicy>
    {
        InlineCacheIndex inlineCacheIndex;
    };

    template <typename SizePolicy>
    struct OpLayoutT_Reg3         // R0 <- R1 op R2   -- or --   R0 op R1 <- R2
    {
        typename SizePolicy::RegSlotType     R0;
        typename SizePolicy::RegSlotType     R1;
        typename SizePolicy::RegSlotType     R2;
    };

    template <typename SizePolicy>
    struct OpLayoutT_Reg3C        // R0 <- R1 op R2 with space for FastPath
    {
        typename SizePolicy::RegSlotType     R0;
        typename SizePolicy::RegSlotType     R1;
        typename SizePolicy::RegSlotType     R2;
        typename SizePolicy::CacheIdType     inlineCacheIndex;
    };

    template <typename SizePolicy>
    struct OpLayoutT_Reg2B1
    {
        typename SizePolicy::RegSlotType     R0;
        typename SizePolicy::RegSlotType     R1;
        byte                                 B2;
    };

    template <typename SizePolicy>
    struct OpLayoutT_Reg3B1
    {
        typename SizePolicy::RegSlotType     R0;
        typename SizePolicy::RegSlotType     R1;
        typename SizePolicy::RegSlotType     R2;
        byte                                 B3;
    };

    template <typename SizePolicy>
    struct OpLayoutT_Reg4         // R0 <- R1 op R2 op R3
    {
        typename SizePolicy::RegSlotType     R0;
        typename SizePolicy::RegSlotType     R1;
        typename SizePolicy::RegSlotType     R2;
        typename SizePolicy::RegSlotType     R3;
    };

    template <typename SizePolicy>
    struct OpLayoutT_Reg5         // R0 <- R1 op R2 op R3 op R4
    {
        typename SizePolicy::RegSlotType     R0;
        typename SizePolicy::RegSlotType     R1;
        typename SizePolicy::RegSlotType     R2;
        typename SizePolicy::RegSlotType     R3;
        typename SizePolicy::RegSlotType     R4;
    };

    template <typename SizePolicy>
    struct OpLayoutT_ArgNoSrc     // OutArg
    {
        typename SizePolicy::ArgSlotType     Arg;
    };

    template <typename SizePolicy>
    struct OpLayoutT_Arg          // OutArg <- Reg   -- or --   Reg <- InArg
    {
        typename SizePolicy::ArgSlotType     Arg;
        typename SizePolicy::RegSlotType     Reg;
    };

    struct OpLayoutBr             // goto Offset
    {
        JumpOffset  RelativeJumpOffset;
    };

    struct OpLayoutBrS            // if (op val) goto Offset
    {
        JumpOffset  RelativeJumpOffset;
        byte        val;
    };

    template <typename SizePolicy>
    struct OpLayoutT_BrReg1       // if (op R1) goto Offset
    {
        JumpOffset  RelativeJumpOffset;
        typename SizePolicy::RegSlotType     R1;
    };

    template <typename SizePolicy>
    struct OpLayoutT_BrReg2       // if (R1 op R2) goto Offset
    {
        JumpOffset  RelativeJumpOffset;
        typename SizePolicy::RegSlotType     R1;
        typename SizePolicy::RegSlotType     R2;
    };

    struct OpLayoutBrProperty     // if (R1.id) goto Offset
    {
        JumpOffset  RelativeJumpOffset;
        RegSlot     Instance;
        PropertyIdIndexType  PropertyIdIndex;
    };

    struct OpLayoutBrLocalProperty     // if (id) goto Offset
    {
        JumpOffset  RelativeJumpOffset;
        PropertyIdIndexType  PropertyIdIndex;
    };

    struct OpLayoutBrEnvProperty   // if ([1].id) goto Offset
    {
        JumpOffset  RelativeJumpOffset;
        PropertyIdIndexType  PropertyIdIndex;
        int32 SlotIndex;
    };

#ifdef BYTECODE_BRANCH_ISLAND
    struct OpLayoutBrLong
    {
        LongJumpOffset RelativeJumpOffset;
    };
#endif

    struct OpLayoutStartCall
    {
        ArgSlot       ArgCount;
    };

    enum CallIExtendedOptions : byte
    {
        CallIExtended_None = 0,
        CallIExtended_SpreadArgs = 1 << 0 // This call has arguments that need to be spread
    };

    template <typename SizePolicy>
    struct OpLayoutT_CallI                // Return = Function(ArgCount)
    {
        typename SizePolicy::ArgSlotType     ArgCount;
        typename SizePolicy::RegSlotSType    Return;
        typename SizePolicy::RegSlotType     Function;
    };

    template <typename SizePolicy>
    struct OpLayoutT_CallIExtended : public OpLayoutT_CallI<SizePolicy>
    {
        CallIExtendedOptions Options;
        uint32 SpreadAuxOffset; // Valid with Options & CallIExtended_SpreadArgs
    };

    template <typename SizePolicy>
    struct OpLayoutT_CallIFlags : public OpLayoutT_CallI<SizePolicy>
    {
        CallFlags callFlags;
    };

    template <typename SizePolicy>
    struct OpLayoutT_CallIWithICIndex : public OpLayoutT_CallI<SizePolicy>
    {
        InlineCacheIndex inlineCacheIndex;
    };

    template <typename SizePolicy>
    struct OpLayoutT_CallIFlagsWithICIndex : public OpLayoutT_CallIWithICIndex<SizePolicy>
    {
        CallFlags callFlags;
    };

    template <typename SizePolicy>
    struct OpLayoutT_CallIExtendedFlags : public OpLayoutT_CallIExtended<SizePolicy>
    {
        CallFlags callFlags;
    };

    template <typename SizePolicy>
    struct OpLayoutT_CallIExtendedWithICIndex : public OpLayoutT_CallIExtended<SizePolicy>
    {
        InlineCacheIndex inlineCacheIndex;
    };

    template <typename SizePolicy>
    struct OpLayoutT_CallIExtendedFlagsWithICIndex : public OpLayoutT_CallIExtendedWithICIndex<SizePolicy>
    {
        CallFlags callFlags;
    };

    template <typename SizePolicy>
    struct OpLayoutT_Class        // class _ extends Extends { Constructor(...) { ... } }
    {
        typename SizePolicy::RegSlotType     Constructor;
        typename SizePolicy::RegSlotSType    Extends;
    };

    template <typename SizePolicy>
    struct OpLayoutT_ElementU     // Instance.PropertyIndex = <some constant value>. e.g. undefined
    {
        typename SizePolicy::RegSlotType             Instance;
        typename SizePolicy::PropertyIdIndexType     PropertyIdIndex;
    };

    template <typename SizePolicy>
    struct OpLayoutT_ElementScopedU     // [env].PropertyIndex = <some constant value>. e.g. undefined
    {
        typename SizePolicy::PropertyIdIndexType     PropertyIdIndex;
    };

    template <typename SizePolicy>
    struct OpLayoutT_ElementRootU // Root.PropertyIndex = <some constant value>. e.g. undefined
    {
        typename SizePolicy::PropertyIdIndexType     PropertyIdIndex;
    };

    template <typename SizePolicy>
    struct OpLayoutT_ElementC     // Value = Instance.PropertyIndex or Instance.PropertyIndex = Value
    {
        typename SizePolicy::RegSlotType             Value;
        typename SizePolicy::RegSlotType             Instance;
        typename SizePolicy::PropertyIdIndexType     PropertyIdIndex;
    };

    template <typename SizePolicy>
    struct OpLayoutT_ElementScopedC     // Value = [env].PropertyIndex or [env].PropertyIndex = Value
    {
        typename SizePolicy::RegSlotType             Value;
        typename SizePolicy::PropertyIdIndexType     PropertyIdIndex;
    };

    template <typename SizePolicy>
    struct OpLayoutT_ElementSlot    // Value = Instance[SlotIndex] or Instance[SlotIndex] = Value
    {
        int32                                SlotIndex; // TODO: Make this one byte?
        typename SizePolicy::RegSlotType     Value;
        typename SizePolicy::RegSlotType     Instance;
    };

    template <typename SizePolicy>
    struct OpLayoutT_ElementSlotI1
    {
        int32       SlotIndex;          // TODO: Make this one byte?
        typename SizePolicy::RegSlotType     Value;
    };

    template <typename SizePolicy>
    struct OpLayoutT_ElementSlotI2
    {
        int32       SlotIndex1;          // TODO: Make this one byte?
        int32       SlotIndex2;          // TODO: Make this one byte?
        typename SizePolicy::RegSlotType     Value;
    };

    template <typename SizePolicy>
    struct OpLayoutT_ElementCP      // As OpLayoutElementC, with space for a FastPath LoadPatch
    {
        typename SizePolicy::RegSlotType     Value;
        typename SizePolicy::RegSlotType     Instance;
        typename SizePolicy::CacheIdType     inlineCacheIndex;
    };

    template <typename SizePolicy>
    struct OpLayoutT_ElementP      // As OpLayoutElementCP, but with no base pointer
    {
        typename SizePolicy::RegSlotType     Value;
        typename SizePolicy::CacheIdType     inlineCacheIndex;
    };

    template <typename SizePolicy>
    struct OpLayoutT_ElementPIndexed      // As OpLayoutElementCP, but with scope index instead of base pointer
    {
        typename SizePolicy::RegSlotType     Value;
        typename SizePolicy::CacheIdType     inlineCacheIndex;
        typename SizePolicy::UnsignedType    scopeIndex;
    };

    template <typename SizePolicy>
    struct OpLayoutT_ElementRootCP   // Same as ElementCP, but for root object
    {
        RootCacheId inlineCacheIndex;
        typename SizePolicy::RegSlotType     Value;
    };

    template <typename SizePolicy>
    struct OpLayoutT_ElementScopedC2       // [implied base].PropertyIndex = Value, Instance2
    {
        typename SizePolicy::RegSlotType     Value;
        typename SizePolicy::RegSlotType     Value2;
        typename SizePolicy::PropertyIdIndexType PropertyIdIndex;
    };

    template <typename SizePolicy>
    struct OpLayoutT_ElementC2       // Instance.PropertyIndex = Value, Instance2
    {
        typename SizePolicy::RegSlotType     Value;
        typename SizePolicy::RegSlotType     Instance;
        typename SizePolicy::RegSlotType     Value2;
        typename SizePolicy::PropertyIdIndexType PropertyIdIndex;
    };

    template <typename SizePolicy>
    struct OpLayoutT_ElementI        // Value = Instance[Element] or Instance[Element] = Value
    {
        typename SizePolicy::RegSlotType     Value;
        typename SizePolicy::RegSlotType     Instance;
        typename SizePolicy::RegSlotType     Element;
    };

    template <typename SizePolicy>
    struct OpLayoutT_ElementUnsigned1     // Value = Instance[Element] or Instance[Element] = Value
    {
        typename SizePolicy::UnsignedType    Element;
        typename SizePolicy::RegSlotType     Value;
        typename SizePolicy::RegSlotType     Instance;
    };

    struct OpLayoutW1
    {
        unsigned short C1;
    };

    struct OpLayoutReg1Int2          // R0 <- Var(C1, C2)
    {
        RegSlot     R0;
        int32       C1;
        int32       C2;
    };

    template <typename SizePolicy>
    struct OpLayoutT_Reg2Int1        // R0 <- func(R1, C1)
    {
        int32                                C1;
        typename SizePolicy::RegSlotType     R0;
        typename SizePolicy::RegSlotType     R1;
    };

    struct OpLayoutAuxNoReg
    {
        uint32      Offset;
        int32       C1;
    };

    struct OpLayoutAuxiliary : public OpLayoutAuxNoReg   // R0 <- Load(Offset, C1)
    {
        RegSlot     R0;
    };

    struct OpLayoutReg2Aux : public OpLayoutAuxiliary    // R0 <- Load(Offset, R1, C1)
    {
        RegSlot     R1;
    };

    template <typename SizePolicy>
    struct OpLayoutT_Unsigned1
    {
        typename SizePolicy::UnsignedType    C1;
    };

    // Dynamic profile layout wrapper
    template <typename LayoutType>
    struct OpLayoutDynamicProfile : public LayoutType
    {
        ProfileId profileId;
    };

    template <typename LayoutType>
    struct OpLayoutDynamicProfile2 : public LayoutType
    {
        ProfileId profileId;
        ProfileId profileId2;
    };

    // Generate the multi size layout type defs
#define LAYOUT_TYPE_WMS(layout) \
    typedef OpLayoutT_##layout<LargeLayoutSizePolicy> OpLayout##layout##_Large; \
    typedef OpLayoutT_##layout<MediumLayoutSizePolicy> OpLayout##layout##_Medium; \
    typedef OpLayoutT_##layout<SmallLayoutSizePolicy> OpLayout##layout##_Small;

    // Generate the profiled type defs
#define LAYOUT_TYPE_PROFILED(layout) \
    typedef OpLayoutDynamicProfile<OpLayout##layout> OpLayoutProfiled##layout;
#define LAYOUT_TYPE_PROFILED2(layout) \
    typedef OpLayoutDynamicProfile2<OpLayout##layout> OpLayoutProfiled2##layout;
#define LAYOUT_TYPE_PROFILED_WMS(layout) \
    LAYOUT_TYPE_WMS(layout) \
    LAYOUT_TYPE_PROFILED(layout##_Large) \
    LAYOUT_TYPE_PROFILED(layout##_Medium) \
    LAYOUT_TYPE_PROFILED(layout##_Small)
#define LAYOUT_TYPE_PROFILED2_WMS(layout) \
    LAYOUT_TYPE_PROFILED_WMS(layout) \
    LAYOUT_TYPE_PROFILED2(layout##_Large) \
    LAYOUT_TYPE_PROFILED2(layout##_Medium) \
    LAYOUT_TYPE_PROFILED2(layout##_Small)
#include "LayoutTypes.h"

#pragma pack(pop)

    // Generate structure to automatically map layout to its info
    template <OpLayoutType::_E layout> struct OpLayoutInfo;

#define LAYOUT_TYPE(layout) \
    CompileAssert(sizeof(OpLayout##layout) <= MaxLayoutSize); \
    template <> struct OpLayoutInfo<OpLayoutType::layout> \
    {  \
        static const bool HasMultiSizeLayout = false; \
    };

#define LAYOUT_TYPE_WMS(layout) \
    CompileAssert(sizeof(OpLayout##layout##_Large) <= MaxLayoutSize); \
    template <> struct OpLayoutInfo<OpLayoutType::layout> \
    {  \
        static const bool HasMultiSizeLayout = true; \
    };
#include "LayoutTypes.h"

    // Generate structure to automatically map opcode to its info
    // Also generate assert to make sure the layout and opcode use the same macro with and without multiple size layout
    template <OpCode opcode> struct OpCodeInfo;

#define DEFINE_OPCODEINFO(op, layout, extended) \
    CompileAssert(!OpLayoutInfo<OpLayoutType::layout>::HasMultiSizeLayout); \
    template <> struct OpCodeInfo<OpCode::op> \
    { \
        static const OpLayoutType::_E Layout = OpLayoutType::layout; \
        static const bool HasMultiSizeLayout = false; \
        static const bool IsExtendedOpcode = extended; \
        typedef OpLayout##layout LayoutType; \
    };
#define DEFINE_OPCODEINFO_WMS(op, layout, extended) \
    CompileAssert(OpLayoutInfo<OpLayoutType::layout>::HasMultiSizeLayout); \
    template <> struct OpCodeInfo<OpCode::op> \
    { \
        static const OpLayoutType::_E Layout = OpLayoutType::layout; \
        static const bool HasMultiSizeLayout = true; \
        static const bool IsExtendedOpcode = extended; \
        typedef OpLayout##layout##_Large LayoutType_Large; \
        typedef OpLayout##layout##_Medium LayoutType_Medium; \
        typedef OpLayout##layout##_Small LayoutType_Small; \
    };
#define MACRO(op, layout, ...) DEFINE_OPCODEINFO(op, layout, false)
#define MACRO_WMS(op, layout, ...) DEFINE_OPCODEINFO_WMS(op, layout, false)
#define MACRO_EXTEND(op, layout, ...) DEFINE_OPCODEINFO(op, layout, true)
#define MACRO_EXTEND_WMS(op, layout, ...) DEFINE_OPCODEINFO_WMS(op, layout, true)
#include "OpCodes.h"
#undef DEFINE_OPCODEINFO
#undef DEFINE_OPCODEINFO_WMS

} // namespace Js
