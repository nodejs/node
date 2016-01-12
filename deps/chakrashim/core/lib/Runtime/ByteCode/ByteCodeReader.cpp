//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "RuntimeByteCodePch.h"

namespace Js
{
    void ByteCodeReader::Create(FunctionBody * functionRead, uint startOffset /* = 0 */)
    {
        Assert(functionRead);
        ByteCodeReader::Create(functionRead, startOffset, /* useOriginalByteCode = */ false);
    }

    void ByteCodeReader::Create(FunctionBody* functionRead, uint startOffset, bool useOriginalByteCode)
    {
        AssertMsg(functionRead != nullptr, "Must provide valid function to execute");

        ByteBlock * byteCodeBlock = useOriginalByteCode ?
            functionRead->GetOriginalByteCode() :
            functionRead->GetByteCode();

        AssertMsg(byteCodeBlock != nullptr, "Must have valid byte-code to read");

        m_startLocation = byteCodeBlock->GetBuffer();
        m_currentLocation = m_startLocation + startOffset;

#if DBG
        m_endLocation = m_startLocation + byteCodeBlock->GetLength();
        Assert(m_currentLocation <= m_endLocation);
#endif
    }

    template<typename LayoutType>
    const unaligned LayoutType * ByteCodeReader::GetLayout()
    {
        size_t layoutSize = sizeof(LayoutType);

        AssertMsg((layoutSize > 0) && (layoutSize < 100), "Ensure valid layout size");

        const byte * layoutData = m_currentLocation;
        m_currentLocation += layoutSize;

        Assert(m_currentLocation <= m_endLocation);

        return reinterpret_cast<const unaligned LayoutType *>(layoutData);
    }

    template<typename LayoutType>
    const unaligned LayoutType * ByteCodeReader::GetLayout(const byte*& ip)
    {
        size_t layoutSize = sizeof(LayoutType);

        AssertMsg((layoutSize > 0) && (layoutSize < 100), "Ensure valid layout size");

        const byte * layoutData = ip;
        ip += layoutSize;
        m_currentLocation = ip;

        Assert(m_currentLocation <= m_endLocation);

        return reinterpret_cast<const unaligned LayoutType *>(layoutData);
    }

    template<>
    const unaligned OpLayoutEmpty * ByteCodeReader::GetLayout<OpLayoutEmpty>()
    {
        return nullptr;
    }

    template<>
    const unaligned OpLayoutEmpty * ByteCodeReader::GetLayout<OpLayoutEmpty>(const byte*& ip)
    {
        m_currentLocation = ip;
        return nullptr;
    }

    OpCode ByteCodeReader::ReadOp(const byte *&ip, LayoutSize& layoutSize) const
    {
        // Return current location and advance past data.

        Assert(ip < m_endLocation);
        OpCode op = (OpCode)*ip++;

        if (!OpCodeUtil::IsPrefixOpcode(op))
        {
            layoutSize = SmallLayout;
            return op;
        }

        return ReadPrefixedOp(ip, layoutSize, op);
    }

    OpCode ByteCodeReader::ReadPrefixedOp(const byte *&ip, LayoutSize& layoutSize, OpCode prefix) const
    {
        Assert(ip < m_endLocation);
        OpCode op = (OpCode)*ip++;
        switch (prefix)
        {
        case Js::OpCode::MediumLayoutPrefix:
            layoutSize = MediumLayout;
            return op;
        case Js::OpCode::LargeLayoutPrefix:
            layoutSize = LargeLayout;
            return op;
        case Js::OpCode::ExtendedOpcodePrefix:
            layoutSize = SmallLayout;
            break;
        case Js::OpCode::ExtendedMediumLayoutPrefix:
            layoutSize = MediumLayout;
            break;
        default:
            Assert(prefix == Js::OpCode::ExtendedLargeLayoutPrefix);
            layoutSize = LargeLayout;
        }

        return (OpCode)(op + (Js::OpCode::ExtendedOpcodePrefix << 8));
    }

    OpCode ByteCodeReader::ReadOp(LayoutSize& layoutSize)
    {
        OpCode op = ReadOp(m_currentLocation, layoutSize);
#if ENABLE_NATIVE_CODEGEN
        Assert(!OpCodeAttr::BackEndOnly(op));
#endif
        return op;
    }

    OpCode ByteCodeReader::ReadPrefixedOp(LayoutSize& layoutSize, OpCode prefix)
    {
        Assert(OpCodeUtil::IsPrefixOpcode(prefix));
        return ReadPrefixedOp(m_currentLocation, layoutSize, prefix);
    }
    OpCode ByteCodeReader::PeekOp(LayoutSize& layoutSize) const
    {
        const byte * ip = m_currentLocation;
        return ReadOp(ip, layoutSize);
    }

    OpCode ByteCodeReader::PeekOp(const byte * ip, LayoutSize& layoutSize)
    {
        return ReadOp(ip, layoutSize);
    }

    OpCode ByteCodeReader::ReadByteOp(const byte*& ip)
    {
        return (OpCode)*ip++;
    }

    OpCode ByteCodeReader::PeekByteOp(const byte * ip)
    {
        return (OpCode)*ip;
    }

    const byte* ByteCodeReader::GetIP()
    {
        return m_currentLocation;
    }

    void ByteCodeReader::SetIP(const byte *const ip)
    {
        Assert(ip >= m_startLocation);
        Assert(ip < m_endLocation);

        m_currentLocation = ip;
    }

    // Define reading functions
#define LAYOUT_TYPE(layout) \
    const unaligned OpLayout##layout * ByteCodeReader::layout() \
    { \
        return GetLayout<OpLayout##layout>(); \
    } \
    const unaligned OpLayout##layout * ByteCodeReader::layout(const byte*& ip) \
    { \
        return GetLayout<OpLayout##layout>(ip); \
    }
#include "LayoutTypes.h"
    // Define reading functions
#define LAYOUT_TYPE(layout) \
    const unaligned OpLayout##layout * ByteCodeReader::layout() \
    { \
        return GetLayout<OpLayout##layout>(); \
    } \
    const unaligned OpLayout##layout * ByteCodeReader::layout(const byte*& ip) \
    { \
        return GetLayout<OpLayout##layout>(ip); \
    }
#define EXCLUDE_DUP_LAYOUT
#include "LayoutTypesAsmJs.h"

    uint ByteCodeReader::GetCurrentOffset() const
    {
        Assert(m_currentLocation >= m_startLocation);
        Assert(m_currentLocation - m_startLocation <= UINT_MAX);
        return (uint)(m_currentLocation - m_startLocation);
    }

    const byte * ByteCodeReader::SetCurrentOffset(int byteOffset)
    {
        const byte * ip = m_startLocation + byteOffset;
        Assert(ip < m_endLocation);
        m_currentLocation = ip;
        return ip;
    }

    const byte * ByteCodeReader::SetCurrentRelativeOffset(const byte * ip, int byteOffset)
    {
        Assert(ip < m_endLocation);
        const byte * targetip = ip + byteOffset;
        Assert(targetip < m_endLocation);
        m_currentLocation = targetip;
        return targetip;
    }

    template <typename T>
    AuxArray<T> const * ByteCodeReader::ReadAuxArray(uint offset, FunctionBody * functionBody)
    {
        Js::AuxArray<T> const * auxArray = (Js::AuxArray<T> const *)(functionBody->GetAuxiliaryData()->GetBuffer() + offset);
        Assert(offset + auxArray->GetDataSize() <= functionBody->GetAuxiliaryData()->GetLength());
        return auxArray;
    }

    // explicit instantiations
    template AuxArray<Var> const * ByteCodeReader::ReadAuxArray<Var>(uint offset, FunctionBody * functionBody);
    template AuxArray<int32> const * ByteCodeReader::ReadAuxArray<int32>(uint offset, FunctionBody * functionBody);
    template AuxArray<uint32> const * ByteCodeReader::ReadAuxArray<uint32>(uint offset, FunctionBody * functionBody);
    template AuxArray<double> const * ByteCodeReader::ReadAuxArray<double>(uint offset, FunctionBody * functionBody);
    template AuxArray<FuncInfoEntry> const * ByteCodeReader::ReadAuxArray<FuncInfoEntry>(uint offset, FunctionBody * functionBody);

    const Js::PropertyIdArray * ByteCodeReader::ReadPropertyIdArray(uint offset, FunctionBody * functionBody, uint extraSlots)
    {
        Js::PropertyIdArray const * propIds = (Js::PropertyIdArray const *)(functionBody->GetAuxiliaryData()->GetBuffer() + offset);
        Assert(offset + propIds->GetDataSize(extraSlots) <= functionBody->GetAuxiliaryData()->GetLength());
        return propIds;
    }

    size_t VarArrayVarCount::GetDataSize() const
    {
        return sizeof(VarArrayVarCount) + sizeof(Var) * TaggedInt::ToInt32(count);
    }

    void VarArrayVarCount::SetCount(uint count)
    {
        this->count = Js::TaggedInt::ToVarUnchecked(count);
    }

    const Js::VarArrayVarCount * ByteCodeReader::ReadVarArrayVarCount(uint offset, FunctionBody * functionBody)
    {
        Js::VarArrayVarCount const * varArray = (Js::VarArrayVarCount const *)(functionBody->GetAuxiliaryContextData()->GetBuffer() + offset);
        Assert(offset + varArray->GetDataSize() <= functionBody->GetAuxiliaryContextData()->GetLength());
        return varArray;
    }

#if DBG_DUMP
    byte ByteCodeReader::GetRawByte(int i)
    {
        return m_startLocation[i];
    }
#endif
} // namespace Js
