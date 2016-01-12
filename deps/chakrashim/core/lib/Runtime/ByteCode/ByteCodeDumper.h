//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once

#if DBG_DUMP
namespace Js
{
    struct ByteCodeDumper /* All static */
    {
    public:
        static void DumpRecursively(FunctionBody* dumpFunction);
        static void Dump(FunctionBody * dumpFunction);
        static void DumpConstantTable(FunctionBody *dumpFunction);
        static void DumpOp(OpCode op, LayoutSize layoutSize, ByteCodeReader& reader, FunctionBody * dumpFunction);

    protected:
        static void DumpImplicitArgIns(FunctionBody * dumpFunction);
        static void DumpI4(int value);
        static void DumpU4(uint32 value);
        static void DumpU2(uint16 value);
        static void DumpOffset(int byteOffset, ByteCodeReader const& reader);
        static void DumpAddr(void* addr);
        static void DumpR8(double value);
        static void DumpReg(RegSlot registerID);
        static void DumpReg(RegSlot_TwoByte registerID);
        static void DumpReg(RegSlot_OneByte registerID);
        static void DumpProfileId(uint id);

#define LAYOUT_TYPE(layout) \
    static void Dump##layout(OpCode op, const unaligned OpLayout##layout* data, FunctionBody * dumpFunction, ByteCodeReader& reader);
#define LAYOUT_TYPE_WMS(layout) \
    template <class T> static void Dump##layout(OpCode op, const unaligned T* data, FunctionBody * dumpFunction, ByteCodeReader& reader);
#define LAYOUT_TYPE_PROFILED(layout) \
        LAYOUT_TYPE(layout) \
        static void DumpProfiled##layout(OpCode op, const unaligned OpLayoutProfiled##layout * data, FunctionBody * dumpFunction, ByteCodeReader& reader)  \
        { \
            Assert(OpCodeUtil::GetOpCodeLayout(op) == OpLayoutType::Profiled##layout); \
            Js::OpCodeUtil::ConvertOpToNonProfiled(op); \
            Dump##layout(op, data, dumpFunction, reader); \
            DumpProfileId(data->profileId); \
        }
#define LAYOUT_TYPE_PROFILED2(layout) \
        LAYOUT_TYPE(layout) \
        static void DumpProfiled2##layout(OpCode op, const unaligned OpLayoutProfiled2##layout * data, FunctionBody * dumpFunction, ByteCodeReader& reader)  \
        { \
            Assert(OpCodeUtil::GetOpCodeLayout(op) == OpLayoutType::Profiled2##layout); \
            Js::OpCodeUtil::ConvertOpToNonProfiled(op); \
            Dump##layout(op, data, dumpFunction, reader); \
            DumpProfileId(data->profileId); \
            DumpProfileId(data->profileId2); \
        }
#define LAYOUT_TYPE_PROFILED_WMS(layout) \
        LAYOUT_TYPE_WMS(layout) \
        template <class T> \
        static void DumpProfiled##layout(OpCode op, const unaligned OpLayoutDynamicProfile<T> * data, FunctionBody * dumpFunction, ByteCodeReader& reader)  \
        { \
            Assert(OpCodeUtil::GetOpCodeLayout(op) == OpLayoutType::Profiled##layout); \
            Js::OpCodeUtil::ConvertOpToNonProfiled(op); \
            Dump##layout<T>(op, data, dumpFunction, reader); \
            DumpProfileId(data->profileId); \
        }
#define LAYOUT_TYPE_PROFILED2_WMS(layout) \
        LAYOUT_TYPE_PROFILED_WMS(layout) \
        template <class T> \
        static void DumpProfiled2##layout(OpCode op, const unaligned OpLayoutDynamicProfile2<T> * data, FunctionBody * dumpFunction, ByteCodeReader& reader)  \
        { \
            Assert(OpCodeUtil::GetOpCodeLayout(op) == OpLayoutType::Profiled2##layout); \
            Js::OpCodeUtil::ConvertOpToNonProfiled(op); \
            Dump##layout<T>(op, data, dumpFunction, reader); \
            DumpProfileId(data->profileId); \
            DumpProfileId(data->profileId2); \
        }
#include "LayoutTypes.h"
    };

} // namespace Js
#endif
