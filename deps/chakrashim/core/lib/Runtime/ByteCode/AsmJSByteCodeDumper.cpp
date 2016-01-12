//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "RuntimeByteCodePch.h"

#ifndef TEMP_DISABLE_ASMJS
#if DBG_DUMP
#include "Language\AsmJsModule.h"
#include "ByteCode\AsmJSByteCodeDumper.h"

namespace Js
{
    void AsmJsByteCodeDumper::Dump(AsmJsFunc* func, FunctionBody* body)
    {
        ByteCodeReader reader;
        reader.Create(body);
        StatementReader statementReader;
        statementReader.Create(body);
        body->DumpFullFunctionName();
        Output::Print(L" Asm.js (");
        const ArgSlot argCount = func->GetArgCount();
        for (ArgSlot i = 0; i < argCount; i++)
        {
            AsmJsType var = func->GetArgType(i);
            if (i > 0)
            {
                Output::Print(L", ");
            }
            if (var.isDouble())
            {
                Output::Print(L"+In%hu", i);
            }
            else if (var.isFloat())
            {
                Output::Print(L"flt(In%hu)", i);
            }
            else if (var.isInt())
            {
                Output::Print(L"In%hu|0", i);
            }
            else if (var.isSIMDType())
            {
                switch (var.GetWhich())
                {
                case AsmJsType::Int32x4:
                    Output::Print(L"I4(In%hu)", i);
                    break;
                case AsmJsType::Float32x4:
                    Output::Print(L"F4(In%hu)", i);
                    break;
                case AsmJsType::Float64x2:
                    Output::Print(L"D2(In%hu)", i);
                    break;
                }
            }
            else
            {
                Assert(UNREACHED);
            }
        }

        Output::Print(L") ");
        Output::Print(L"(size: %d [%d])\n", body->GetByteCodeCount(), body->GetByteCodeWithoutLDACount());
        const auto& intRegisters = func->GetRegisterSpace<int>();
        const auto& doubleRegisters = func->GetRegisterSpace<double>();
        const auto& floatRegisters = func->GetRegisterSpace<float>();
        Output::Print(
            L"      Integer : %u locals (%u temps from I%u)\n",
            intRegisters.GetVarCount(),
            intRegisters.GetTmpCount(),
            intRegisters.GetFirstTmpRegister());
        Output::Print(
            L"      Doubles : %u locals (%u temps from D%u)\n",
            doubleRegisters.GetVarCount(),
            doubleRegisters.GetTmpCount(),
            doubleRegisters.GetFirstTmpRegister());

        Output::Print(
            L"      Floats : %u locals (%u temps from F%u)\n",
            floatRegisters.GetVarCount(),
            floatRegisters.GetTmpCount(),
            floatRegisters.GetFirstTmpRegister());

        const auto& simdRegisters = func->GetRegisterSpace<AsmJsSIMDValue>();
        Output::Print(
            L"      SIMDs : %u locals (%u temps from SIMD%u)\n",
            simdRegisters.GetVarCount(),
            simdRegisters.GetTmpCount(),
            simdRegisters.GetFirstTmpRegister());

        uint32 statementIndex = 0;
        DumpConstants(func, body);

        Output::Print(L"    Implicit Arg Ins:\n    ======== =====\n    ");
        int iArg = intRegisters.GetConstCount(), dArg = doubleRegisters.GetConstCount(), fArg = floatRegisters.GetConstCount();
        int simdArg = simdRegisters.GetConstCount();
        for (ArgSlot i = 0; i < argCount; i++)
        {
            const AsmJsType& var = func->GetArgType(i);
            if (var.isDouble())
            {
                Output::Print(L" D%d  In%d", dArg++, i);
            }
            else if (var.isFloat())
            {
                Output::Print(L" F%d  In%d", fArg++, i);
            }
            else if (var.isInt())
            {
                Output::Print(L" I%d  In%d", iArg++, i);
            }
            else if (var.isSIMDType())
            {
                Output::Print(L" SIMD%d  In%d", simdArg++, i);
            }
            else
            {
                Assert(UNREACHED);
            }
            Output::Print(L"\n    ");
        }
        Output::Print(L"\n");

        if (func->GetReturnType() == AsmJsRetType::Void)
        {
            Output::Print(L"    0000   %-20s R0\n", OpCodeUtilAsmJs::GetOpCodeName(OpCodeAsmJs::LdUndef));
        }

        while (true)
        {
            while (statementReader.AtStatementBoundary(&reader))
            {
                body->PrintStatementSourceLine(statementIndex);
                statementIndex = statementReader.MoveNextStatementBoundary();
            }
            int byteOffset = reader.GetCurrentOffset();
            LayoutSize layoutSize;
            OpCodeAsmJs op = (OpCodeAsmJs)reader.ReadOp(layoutSize);
            if (op == OpCodeAsmJs::EndOfBlock)
            {
                Assert(reader.GetCurrentOffset() == body->GetByteCode()->GetLength());
                break;
            }
            Output::Print(L"    %04x %2s", byteOffset, layoutSize == LargeLayout ? L"L-" : layoutSize == MediumLayout ? L"M-" : L"");
            DumpOp(op, layoutSize, reader, body);
            if (Js::Configuration::Global.flags.Verbose)
            {
                int layoutStart = byteOffset + 2; // Account for the prefix op
                int endByteOffset = reader.GetCurrentOffset();
                Output::SkipToColumn(70);
                if (layoutSize == LargeLayout)
                {
                    Output::Print(L"%02X ",
                        op > Js::OpCodeAsmJs::MaxByteSizedOpcodes ?
                        Js::OpCodeAsmJs::ExtendedLargeLayoutPrefix : Js::OpCodeAsmJs::LargeLayoutPrefix);
                }
                else if (layoutSize == MediumLayout)
                {
                    Output::Print(L"%02X ",
                        op > Js::OpCodeAsmJs::MaxByteSizedOpcodes ?
                        Js::OpCodeAsmJs::ExtendedMediumLayoutPrefix : Js::OpCodeAsmJs::MediumLayoutPrefix);
                }
                else
                {
                    Assert(layoutSize == SmallLayout);
                    if (op > Js::OpCodeAsmJs::MaxByteSizedOpcodes)
                    {
                        Output::Print(L"%02X ", Js::OpCodeAsmJs::ExtendedOpcodePrefix);
                    }
                    else
                    {
                        Output::Print(L"   ");
                        layoutStart--; // don't have a prefix
                    }
                }

                Output::Print(L"%02x", (byte)op);
                for (int i = layoutStart; i < endByteOffset; i++)
                {
                    Output::Print(L" %02x", reader.GetRawByte(i));
                }
            }
            Output::Print(L"\n");
        }
        if (statementReader.AtStatementBoundary(&reader))
        {
            body->PrintStatementSourceLine(statementIndex);
            statementIndex = statementReader.MoveNextStatementBoundary();
        }
        Output::Print(L"\n");
        Output::Flush();
    }

    void AsmJsByteCodeDumper::DumpConstants(AsmJsFunc* func, FunctionBody* body)
    {
        const auto& intRegisters = func->GetRegisterSpace<int>();
        const auto& doubleRegisters = func->GetRegisterSpace<double>();
        const auto& floatRegisters = func->GetRegisterSpace<float>();

        int nbIntConst = intRegisters.GetConstCount();
        int nbDoubleConst = doubleRegisters.GetConstCount();
        int nbFloatConst = floatRegisters.GetConstCount();

        int* constTable = (int*)((Var*)body->GetConstTable() + (AsmJsFunctionMemory::RequiredVarConstants - 1));
        if (nbIntConst > 0)
        {
            Output::Print(L"    Constant Integer:\n    ======== =======\n    ");
            for (int i = 0; i < nbIntConst; i++)
            {
                Output::Print(L" I%d  %d\n    ", i, *constTable);
                ++constTable;
            }
        }

        float* floatTable = (float*)constTable;
        Output::Print(L"\n");
        if (nbFloatConst > 0)
        {

            // const int inc = sizeof( double ) / sizeof( void* );
            Output::Print(L"    Constant Floats:\n    ======== ======\n    ");
            for (int i = 0; i < nbFloatConst; i++)
            {
                Output::Print(L" F%d  %.4f\n    ", i, *floatTable);
                ++floatTable;
                ++constTable;
            }
        }

        double* doubleTable = (double*)constTable;
        Output::Print(L"\n");
        if (nbDoubleConst > 0)
        {

            // const int inc = sizeof( double ) / sizeof( void* );
            Output::Print(L"    Constant Doubles:\n    ======== ======\n    ");
            for (int i = 0; i < nbDoubleConst; i++)
            {
                Output::Print(L" D%d  %.4f\n    ", i, *doubleTable);
                ++doubleTable;
            }
        }
        // SIMD reg space is un-typed.
        // We print each register in its 3 possible types to ease debugging.
        const auto& simdRegisters = func->GetRegisterSpace<AsmJsSIMDValue>();
        int nbSimdConst = simdRegisters.GetConstCount();

        Output::Print(L"\n");
        if (nbSimdConst > 0)
        {
            AsmJsSIMDValue* simdTable = (AsmJsSIMDValue*)doubleTable;
            Output::Print(L"    Constant SIMD values:\n    ======== ======\n    ");
            for (int i = 0; i < nbSimdConst; i++)
            {
                Output::Print(L"SIMD%d ", i);
                Output::Print(L"\tI4(%d, %d, %d, %d),", simdTable->i32[SIMD_X], simdTable->i32[SIMD_Y], simdTable->i32[SIMD_Z], simdTable->i32[SIMD_W]);
                Output::Print(L"\tF4(%.4f, %.4f, %.4f, %.4f),", simdTable->f32[SIMD_X], simdTable->f32[SIMD_Y], simdTable->f32[SIMD_Z], simdTable->f32[SIMD_W]);
                Output::Print(L"\tD2(%.4f, %.4f)\n    ", simdTable->f64[SIMD_X], simdTable->f64[SIMD_Y]);
                ++simdTable;
            }
        }
        Output::Print(L"\n");
    }

    void AsmJsByteCodeDumper::DumpOp(OpCodeAsmJs op, LayoutSize layoutSize, ByteCodeReader& reader, FunctionBody* dumpFunction)
    {
        Output::Print(L"%-20s", OpCodeUtilAsmJs::GetOpCodeName(op));
        OpLayoutTypeAsmJs nType = OpCodeUtilAsmJs::GetOpCodeLayout(op);
        switch (layoutSize * OpLayoutTypeAsmJs::Count + nType)
        {
#define LAYOUT_TYPE(layout) \
            case OpLayoutTypeAsmJs::layout: \
                Assert(layoutSize == SmallLayout); \
                Dump##layout(op, reader.layout(), dumpFunction, reader); \
                break;
#define LAYOUT_TYPE_WMS(layout) \
            case SmallLayout * OpLayoutTypeAsmJs::Count + OpLayoutTypeAsmJs::layout: \
                Dump##layout(op, reader.layout##_Small(), dumpFunction, reader); \
                break; \
            case MediumLayout * OpLayoutTypeAsmJs::Count + OpLayoutTypeAsmJs::layout: \
                Dump##layout(op, reader.layout##_Medium(), dumpFunction, reader); \
                break; \
            case LargeLayout * OpLayoutTypeAsmJs::Count + OpLayoutTypeAsmJs::layout: \
                Dump##layout(op, reader.layout##_Large(), dumpFunction, reader); \
                break;
#include "LayoutTypesAsmJs.h"

        default:
            AssertMsg(false, "Unknown OpLayout");
            break;
        }
    }

    void AsmJsByteCodeDumper::DumpIntReg(RegSlot reg)
    {
        Output::Print(L" I%d ", (int)reg);
    }
    void AsmJsByteCodeDumper::DumpDoubleReg(RegSlot reg)
    {
        Output::Print(L" D%d ", (int)reg);
    }

    void AsmJsByteCodeDumper::DumpFloatReg(RegSlot reg)
    {
        Output::Print(L" F%d ", (int)reg);
    }
    void AsmJsByteCodeDumper::DumpR8Float(float value)
    {
        Output::Print(L" float:%f ", value);
    }

    // Float32x4
    void AsmJsByteCodeDumper::DumpFloat32x4Reg(RegSlot reg)
    {
        Output::Print(L"F4_%d ", (int)reg);
    }

    // Int32x4
    void AsmJsByteCodeDumper::DumpInt32x4Reg(RegSlot reg)
    {
        Output::Print(L"I4_%d ", (int)reg);
    }

    // Float64x2
    void AsmJsByteCodeDumper::DumpFloat64x2Reg(RegSlot reg)
    {
        Output::Print(L"D2_%d ", (int)reg);
    }

    template <class T>
    void AsmJsByteCodeDumper::DumpElementSlot(OpCodeAsmJs op, const unaligned T * data, FunctionBody * dumpFunction, ByteCodeReader& reader)
    {
        switch (op)
        {
        case OpCodeAsmJs::LdSlot:
        case OpCodeAsmJs::LdSlotArr:
            Output::Print(L" R%d = R%d[%d] ", data->Value, data->Instance, data->SlotIndex);
            break;
        case OpCodeAsmJs::LdArr_Func:
            Output::Print(L" R%d = R%d[I%d] ", data->Value, data->Instance, data->SlotIndex);
            break;
        case OpCodeAsmJs::StSlot_Int:
            Output::Print(L" R%d[%d] = I%d ", data->Instance, data->SlotIndex, data->Value);
            break;
        case OpCodeAsmJs::StSlot_Flt:
            Output::Print(L" R%d[%d] = F%d ", data->Instance, data->SlotIndex, data->Value);
            break;
        case OpCodeAsmJs::StSlot_Db:
            Output::Print(L" R%d[%d] = D%d ", data->Instance, data->SlotIndex, data->Value);
            break;
        case OpCodeAsmJs::LdSlot_Int:
            Output::Print(L" I%d = R%d[%d] ", data->Value, data->Instance, data->SlotIndex);
            break;
        case OpCodeAsmJs::LdSlot_Flt:
            Output::Print(L" F%d = R%d[%d] ", data->Value, data->Instance, data->SlotIndex);
            break;
        case OpCodeAsmJs::LdSlot_Db:
            Output::Print(L" D%d = R%d[%d] ", data->Value, data->Instance, data->SlotIndex);
            break;
        case OpCodeAsmJs::Simd128_LdSlot_F4:
            Output::Print(L" F4_%d = R%d[%d] ", data->Value, data->Instance, data->SlotIndex);
            break;
        case OpCodeAsmJs::Simd128_LdSlot_I4:
            Output::Print(L" I4_%d = R%d[%d] ", data->Value, data->Instance, data->SlotIndex);
            break;
        case OpCodeAsmJs::Simd128_LdSlot_D2:
            Output::Print(L" D2_%d = R%d[%d] ", data->Value, data->Instance, data->SlotIndex);
            break;

        case OpCodeAsmJs::Simd128_StSlot_F4:
            Output::Print(L" R%d[%d]  = F4_%d", data->Instance, data->SlotIndex, data->Value);
            break;
        case OpCodeAsmJs::Simd128_StSlot_I4:
            Output::Print(L" R%d[%d]  = I4_%d", data->Instance, data->SlotIndex, data->Value);
            break;
        case OpCodeAsmJs::Simd128_StSlot_D2:
            Output::Print(L" R%d[%d]  = D2_%d", data->Instance, data->SlotIndex, data->Value);
            break;
        default:
        {
            AssertMsg(false, "Unknown OpCode for OpLayoutElementSlot");
            break;
        }
        }
    }

    template <class T>
    void AsmJsByteCodeDumper::DumpAsmTypedArr(OpCodeAsmJs op, const unaligned T * data, FunctionBody * dumpFunction, ByteCodeReader& reader)
    {
        wchar_t* heapTag = nullptr;
        wchar_t valueTag = 'I';
        switch (data->ViewType)
        {
        case ArrayBufferView::TYPE_INT8:
            heapTag = L"HEAP8"; valueTag = 'I';  break;
        case ArrayBufferView::TYPE_UINT8:
            heapTag = L"HEAPU8"; valueTag = 'U'; break;
        case ArrayBufferView::TYPE_INT16:
            heapTag = L"HEAP16"; valueTag = 'I'; break;
        case ArrayBufferView::TYPE_UINT16:
            heapTag = L"HEAPU16"; valueTag = 'U'; break;
        case ArrayBufferView::TYPE_INT32:
            heapTag = L"HEAP32"; valueTag = 'I'; break;
        case ArrayBufferView::TYPE_UINT32:
            heapTag = L"HEAPU32"; valueTag = 'U'; break;
        case ArrayBufferView::TYPE_FLOAT32:
            heapTag = L"HEAPF32"; valueTag = 'F'; break;
        case ArrayBufferView::TYPE_FLOAT64:
            heapTag = L"HEAPF64"; valueTag = 'D'; break;
        default:
            Assert(false);
            __assume(false);
            break;
        }

        switch (op)
        {
        case OpCodeAsmJs::LdArr:
            Output::Print(L"%c%d = %s[I%d]", valueTag, data->Value, heapTag, data->SlotIndex); break;
        case OpCodeAsmJs::LdArrConst:
            Output::Print(L"%c%d = %s[%d]", valueTag, data->Value, heapTag, data->SlotIndex); break;
        case OpCodeAsmJs::StArr:
            Output::Print(L"%s[I%d] = %c%d", heapTag, data->SlotIndex, valueTag, data->Value); break;
        case OpCodeAsmJs::StArrConst:
            Output::Print(L"%s[%d] = %c%d", heapTag, data->SlotIndex, valueTag, data->Value); break;
        default:
            Assert(false);
            __assume(false);
            break;
        }
    }

    void AsmJsByteCodeDumper::DumpStartCall(OpCodeAsmJs op, const unaligned OpLayoutStartCall* data, FunctionBody * dumpFunction, ByteCodeReader& reader)
    {
        Assert(op == OpCodeAsmJs::StartCall || op == OpCodeAsmJs::I_StartCall);
        Output::Print(L" ArgSize: %d", data->ArgCount);
    }

    template <class T>
    void AsmJsByteCodeDumper::DumpAsmCall(OpCodeAsmJs op, const unaligned T * data, FunctionBody * dumpFunction, ByteCodeReader& reader)
    {
        if (data->Return != Constants::NoRegister)
        {
            DumpReg((RegSlot)data->Return);
            Output::Print(L"=");
        }
        Output::Print(L" R%d(ArgCount: %d)", data->Function, data->ArgCount);
    }

    template <class T>
    void AsmJsByteCodeDumper::DumpAsmUnsigned1(OpCodeAsmJs op, const unaligned T * data, FunctionBody * dumpFunction, ByteCodeReader& reader)
    {
        DumpU4(data->C1);
    }
    void AsmJsByteCodeDumper::DumpEmpty(OpCodeAsmJs op, const unaligned OpLayoutEmpty* data, FunctionBody * dumpFunction, ByteCodeReader& reader)
    {
        // empty
    }

    void AsmJsByteCodeDumper::DumpAsmBr(OpCodeAsmJs op, const unaligned OpLayoutAsmBr* data, FunctionBody * dumpFunction, ByteCodeReader& reader)
    {
        DumpOffset(data->RelativeJumpOffset, reader);
    }

    template <class T>
    void AsmJsByteCodeDumper::DumpAsmReg1(OpCodeAsmJs op, const unaligned T * data, FunctionBody * dumpFunction, ByteCodeReader& reader)
    {
        DumpReg(data->R0);
    }
    template <class T>
    void AsmJsByteCodeDumper::DumpAsmReg2(OpCodeAsmJs op, const unaligned T * data, FunctionBody * dumpFunction, ByteCodeReader& reader)
    {
        DumpReg(data->R0);
        DumpReg(data->R1);
    }
    template <class T>
    void AsmJsByteCodeDumper::DumpAsmReg3(OpCodeAsmJs op, const unaligned T * data, FunctionBody * dumpFunction, ByteCodeReader& reader)
    {
        DumpReg(data->R0);
        DumpReg(data->R1);
        DumpReg(data->R2);
    }
    template <class T>
    void AsmJsByteCodeDumper::DumpAsmReg4(OpCodeAsmJs op, const unaligned T * data, FunctionBody * dumpFunction, ByteCodeReader& reader)
    {
        DumpReg(data->R0);
        DumpReg(data->R1);
        DumpReg(data->R2);
        DumpReg(data->R3);
    }
    template <class T>
    void AsmJsByteCodeDumper::DumpAsmReg5(OpCodeAsmJs op, const unaligned T * data, FunctionBody * dumpFunction, ByteCodeReader& reader)
    {
        DumpReg(data->R0);
        DumpReg(data->R1);
        DumpReg(data->R2);
        DumpReg(data->R3);
        DumpReg(data->R4);
    }
    template <class T>
    void AsmJsByteCodeDumper::DumpAsmReg6(OpCodeAsmJs op, const unaligned T * data, FunctionBody * dumpFunction, ByteCodeReader& reader)
    {
        DumpReg(data->R0);
        DumpReg(data->R1);
        DumpReg(data->R2);
        DumpReg(data->R3);
        DumpReg(data->R4);
        DumpReg(data->R5);
    }
    template <class T>
    void AsmJsByteCodeDumper::DumpAsmReg7(OpCodeAsmJs op, const unaligned T * data, FunctionBody * dumpFunction, ByteCodeReader& reader)
    {
        DumpReg(data->R0);
        DumpReg(data->R1);
        DumpReg(data->R2);
        DumpReg(data->R3);
        DumpReg(data->R4);
        DumpReg(data->R5);
        DumpReg(data->R6);
    }

    template <class T>
    void AsmJsByteCodeDumper::DumpAsmReg2IntConst1(OpCodeAsmJs op, const unaligned T * data, FunctionBody * dumpFunction, ByteCodeReader& reader)
    {
        DumpReg(data->R0);
        DumpReg(data->R1);
        DumpI4(data->C2);
    }
    template <class T>
    void AsmJsByteCodeDumper::DumpInt1Double1(OpCodeAsmJs op, const unaligned T * data, FunctionBody * dumpFunction, ByteCodeReader& reader)
    {
        DumpIntReg(data->I0);
        DumpDoubleReg(data->D1);
    }

    template <class T>
    void AsmJsByteCodeDumper::DumpInt1Float1(OpCodeAsmJs op, const unaligned T * data, FunctionBody * dumpFunction, ByteCodeReader& reader)
    {
        DumpIntReg(data->I0);
        DumpFloatReg(data->F1);
    }

    template <class T>
    void AsmJsByteCodeDumper::DumpDouble1Int1(OpCodeAsmJs op, const unaligned T * data, FunctionBody * dumpFunction, ByteCodeReader& reader)
    {
        DumpDoubleReg(data->D0);
        DumpIntReg(data->I1);
    }
    template <class T>
    void AsmJsByteCodeDumper::DumpDouble1Float1(OpCodeAsmJs op, const unaligned T * data, FunctionBody * dumpFunction, ByteCodeReader& reader)
    {
        DumpDoubleReg(data->D0);
        DumpFloatReg(data->F1);
    }
    template <class T>
    void AsmJsByteCodeDumper::DumpDouble1Reg1(OpCodeAsmJs op, const unaligned T * data, FunctionBody * dumpFunction, ByteCodeReader& reader)
    {
        DumpDoubleReg(data->D0);
        DumpReg(data->R1);
    }

    template <class T>
    void AsmJsByteCodeDumper::DumpFloat1Reg1(OpCodeAsmJs op, const unaligned T * data, FunctionBody * dumpFunction, ByteCodeReader& reader)
    {
        DumpFloatReg(data->F0);
        DumpReg(data->R1);
    }

    template <class T>
    void AsmJsByteCodeDumper::DumpInt1Reg1(OpCodeAsmJs op, const unaligned T * data, FunctionBody * dumpFunction, ByteCodeReader& reader)
    {
        DumpIntReg(data->I0);
        DumpReg(data->R1);
    }

    template <class T>
    void AsmJsByteCodeDumper::DumpReg1Double1(OpCodeAsmJs op, const unaligned T * data, FunctionBody * dumpFunction, ByteCodeReader& reader)
    {
        DumpReg(data->R0);
        DumpDoubleReg(data->D1);
    }

    template <class T>
    void AsmJsByteCodeDumper::DumpReg1Float1(OpCodeAsmJs op, const unaligned T * data, FunctionBody * dumpFunction, ByteCodeReader& reader)
    {
        DumpReg(data->R0);
        DumpFloatReg(data->F1);
    }

    template <class T>
    void AsmJsByteCodeDumper::DumpInt1Double2(OpCodeAsmJs op, const unaligned T * data, FunctionBody * dumpFunction, ByteCodeReader& reader)
    {
        DumpIntReg(data->I0);
        DumpDoubleReg(data->D1);
        DumpDoubleReg(data->D2);
    }

    template <class T>
    void AsmJsByteCodeDumper::DumpInt1Float2(OpCodeAsmJs op, const unaligned T * data, FunctionBody * dumpFunction, ByteCodeReader& reader)
    {
        DumpIntReg(data->I0);
        DumpFloatReg(data->F1);
        DumpFloatReg(data->F2);
    }

    template <class T>
    void AsmJsByteCodeDumper::DumpReg1Int1(OpCodeAsmJs op, const unaligned T * data, FunctionBody * dumpFunction, ByteCodeReader& reader)
    {
        DumpReg(data->R0);
        DumpIntReg(data->I1);
    }

    template <class T>
    void AsmJsByteCodeDumper::DumpInt2(OpCodeAsmJs op, const unaligned T * data, FunctionBody * dumpFunction, ByteCodeReader& reader)
    {
        DumpIntReg(data->I0);
        DumpIntReg(data->I1);
    }

    template <class T>
    void AsmJsByteCodeDumper::DumpInt1Const1(OpCodeAsmJs op, const unaligned T * data, FunctionBody * dumpFunction, ByteCodeReader& reader)
    {
        DumpIntReg(data->I0);
        DumpI4(data->C1);
    }

    template <class T>
    void AsmJsByteCodeDumper::DumpInt3(OpCodeAsmJs op, const unaligned T * data, FunctionBody * dumpFunction, ByteCodeReader& reader)
    {
        DumpIntReg(data->I0);
        DumpIntReg(data->I1);
        DumpIntReg(data->I2);
    }

    template <class T>
    void AsmJsByteCodeDumper::DumpDouble2(OpCodeAsmJs op, const unaligned T * data, FunctionBody * dumpFunction, ByteCodeReader& reader)
    {
        DumpDoubleReg(data->D0);
        DumpDoubleReg(data->D1);
    }

    template <class T>
    void AsmJsByteCodeDumper::DumpFloat2(OpCodeAsmJs op, const unaligned T * data, FunctionBody * dumpFunction, ByteCodeReader& reader)
    {
        DumpFloatReg(data->F0);
        DumpFloatReg(data->F1);
    }
    template <class T>
    void AsmJsByteCodeDumper::DumpFloat3(OpCodeAsmJs op, const unaligned T * data, FunctionBody * dumpFunction, ByteCodeReader& reader)
    {
        DumpFloatReg(data->F0);
        DumpFloatReg(data->F1);
        DumpFloatReg(data->F2);
    }
    template <class T>
    void AsmJsByteCodeDumper::DumpFloat1Double1(OpCodeAsmJs op, const unaligned T * data, FunctionBody * dumpFunction, ByteCodeReader& reader)
    {
        DumpFloatReg(data->F0);
        DumpDoubleReg(data->D1);
    }

    template <class T>
    void AsmJsByteCodeDumper::DumpFloat1Int1(OpCodeAsmJs op, const unaligned T * data, FunctionBody * dumpFunction, ByteCodeReader& reader)
    {
        DumpFloatReg(data->F0);
        DumpIntReg(data->I1);
    }

    template <class T>
    void AsmJsByteCodeDumper::DumpDouble3(OpCodeAsmJs op, const unaligned T * data, FunctionBody * dumpFunction, ByteCodeReader& reader)
    {
        DumpDoubleReg(data->D0);
        DumpDoubleReg(data->D1);
        DumpDoubleReg(data->D2);
    }

    template <class T>
    void AsmJsByteCodeDumper::DumpBrInt1(OpCodeAsmJs op, const unaligned T * data, FunctionBody * dumpFunction, ByteCodeReader& reader)
    {
        DumpOffset(data->RelativeJumpOffset, reader);
        DumpIntReg(data->I1);
    }

    template <class T>
    void AsmJsByteCodeDumper::DumpBrInt2(OpCodeAsmJs op, const unaligned T * data, FunctionBody * dumpFunction, ByteCodeReader& reader)
    {
        DumpOffset(data->RelativeJumpOffset, reader);
        DumpIntReg(data->I1);
        DumpIntReg(data->I2);
    }

    // Float32x4
    template <class T>
    void AsmJsByteCodeDumper::DumpFloat32x4_2(OpCodeAsmJs op, const unaligned T * data, FunctionBody * dumpFunction, ByteCodeReader& reader)
    {
        DumpFloat32x4Reg(data->F4_0);
        DumpFloat32x4Reg(data->F4_1);
    }

    template <class T>
    void AsmJsByteCodeDumper::DumpFloat32x4_3(OpCodeAsmJs op, const unaligned T * data, FunctionBody * dumpFunction, ByteCodeReader& reader)
    {
        DumpFloat32x4Reg(data->F4_0);
        DumpFloat32x4Reg(data->F4_1);
        DumpFloat32x4Reg(data->F4_2);
    }

    template <class T>
    void AsmJsByteCodeDumper::DumpFloat32x4_4(OpCodeAsmJs op, const unaligned T * data, FunctionBody * dumpFunction, ByteCodeReader& reader)
    {
        DumpFloat32x4Reg(data->F4_0);
        DumpFloat32x4Reg(data->F4_1);
        DumpFloat32x4Reg(data->F4_2);
        DumpFloat32x4Reg(data->F4_3);
    }

    template <class T>
    void AsmJsByteCodeDumper::DumpFloat32x4_1Float4(OpCodeAsmJs op, const unaligned T * data, FunctionBody * dumpFunction, ByteCodeReader& reader)
    {
        DumpFloat32x4Reg(data->F4_0);
        DumpFloatReg(data->F1);
        DumpFloatReg(data->F2);
        DumpFloatReg(data->F3);
        DumpFloatReg(data->F4);
    }

    template <class T>
    void AsmJsByteCodeDumper::DumpFloat32x4_2Int4(OpCodeAsmJs op, const unaligned T * data, FunctionBody * dumpFunction, ByteCodeReader& reader)
    {
        DumpFloat32x4Reg(data->F4_0);
        DumpFloat32x4Reg(data->F4_1);
        DumpIntReg(data->I2);
        DumpIntReg(data->I3);
        DumpIntReg(data->I4);
        DumpIntReg(data->I5);
    }

    template <class T>
    void AsmJsByteCodeDumper::DumpFloat32x4_3Int4(OpCodeAsmJs op, const unaligned T * data, FunctionBody * dumpFunction, ByteCodeReader& reader)
    {
        DumpFloat32x4Reg(data->F4_0);
        DumpFloat32x4Reg(data->F4_1);
        DumpFloat32x4Reg(data->F4_2);
        DumpIntReg(data->I3);
        DumpIntReg(data->I4);
        DumpIntReg(data->I5);
        DumpIntReg(data->I6);
    }

    template <class T>
    void AsmJsByteCodeDumper::DumpFloat32x4_1Float1(OpCodeAsmJs op, const unaligned T * data, FunctionBody * dumpFunction, ByteCodeReader& reader)
    {
        DumpFloat32x4Reg(data->F4_0);
        DumpFloatReg(data->F1);
    }

    template <class T>
    void AsmJsByteCodeDumper::DumpFloat32x4_2Float1(OpCodeAsmJs op, const unaligned T * data, FunctionBody * dumpFunction, ByteCodeReader& reader)
    {
        DumpFloat32x4Reg(data->F4_0);
        DumpFloat32x4Reg(data->F4_1);
        DumpFloatReg(data->F2);
    }

    template <class T>
    void AsmJsByteCodeDumper::DumpFloat32x4_1Float64x2_1(OpCodeAsmJs op, const unaligned T * data, FunctionBody * dumpFunction, ByteCodeReader& reader)
    {
        DumpFloat32x4Reg(data->F4_0);
        DumpFloat64x2Reg(data->D2_1);
    }

    template <class T>
    void AsmJsByteCodeDumper::DumpFloat32x4_1Int32x4_1(OpCodeAsmJs op, const unaligned T * data, FunctionBody * dumpFunction, ByteCodeReader& reader)
    {
        DumpFloat32x4Reg(data->F4_0);
        DumpInt32x4Reg(data->I4_1);
    }

    template <class T>
    void AsmJsByteCodeDumper::DumpFloat32x4_1Int32x4_1Float32x4_2(OpCodeAsmJs op, const unaligned T * data, FunctionBody * dumpFunction, ByteCodeReader& reader)
    {
        DumpFloat32x4Reg(data->F4_0);
        DumpInt32x4Reg(data->I4_1);
        DumpFloat32x4Reg(data->F4_2);
        DumpFloat32x4Reg(data->F4_3);
    }

    template <class T>
    void AsmJsByteCodeDumper::DumpReg1Float32x4_1(OpCodeAsmJs op, const unaligned T * data, FunctionBody * dumpFunction, ByteCodeReader& reader)
    {
        DumpReg(data->R0);
        DumpFloat32x4Reg(data->F4_1);
    }

    template <class T>
    void AsmJsByteCodeDumper::DumpInt1Float32x4_1(OpCodeAsmJs op, const unaligned T * data, FunctionBody * dumpFunction, ByteCodeReader& reader)
    {
        DumpIntReg(data->I0);
        DumpFloat32x4Reg(data->F4_1);
    }

    // Int32x4
    template <class T>
    void AsmJsByteCodeDumper::DumpInt32x4_2(OpCodeAsmJs op, const unaligned T * data, FunctionBody * dumpFunction, ByteCodeReader& reader)
    {
        DumpInt32x4Reg(data->I4_0);
        DumpInt32x4Reg(data->I4_1);
    }

    template <class T>
    void AsmJsByteCodeDumper::DumpInt32x4_3(OpCodeAsmJs op, const unaligned T * data, FunctionBody * dumpFunction, ByteCodeReader& reader)
    {
        DumpInt32x4Reg(data->I4_0);
        DumpInt32x4Reg(data->I4_1);
        DumpInt32x4Reg(data->I4_2);
    }

    template <class T>
    void AsmJsByteCodeDumper::DumpInt32x4_4(OpCodeAsmJs op, const unaligned T * data, FunctionBody * dumpFunction, ByteCodeReader& reader)
    {
        DumpInt32x4Reg(data->I4_0);
        DumpInt32x4Reg(data->I4_1);
        DumpInt32x4Reg(data->I4_2);
        DumpInt32x4Reg(data->I4_3);
    }


    template <class T>
    void AsmJsByteCodeDumper::DumpInt32x4_1Int4(OpCodeAsmJs op, const unaligned T * data, FunctionBody * dumpFunction, ByteCodeReader& reader)
    {
        DumpInt32x4Reg(data->I4_0);
        DumpIntReg(data->I1);
        DumpIntReg(data->I2);
        DumpIntReg(data->I3);
        DumpIntReg(data->I4);
    }

    template <class T>
    void AsmJsByteCodeDumper::DumpInt32x4_2Int4(OpCodeAsmJs op, const unaligned T * data, FunctionBody * dumpFunction, ByteCodeReader& reader)
    {
        DumpInt32x4Reg(data->I4_0);
        DumpInt32x4Reg(data->I4_1);
        DumpIntReg(data->I2);
        DumpIntReg(data->I3);
        DumpIntReg(data->I4);
        DumpIntReg(data->I5);
    }

    template <class T>
    void AsmJsByteCodeDumper::DumpInt32x4_2Int2(OpCodeAsmJs op, const unaligned T * data, FunctionBody * dumpFunction, ByteCodeReader& reader)
    {
        DumpInt32x4Reg(data->I4_0);
        DumpInt32x4Reg(data->I4_1);
        DumpIntReg(data->I2);
        DumpIntReg(data->I3);
    }

    template <class T>
    void AsmJsByteCodeDumper::DumpInt1Int32x4_1Int1(OpCodeAsmJs op, const unaligned T * data, FunctionBody * dumpFunction, ByteCodeReader& reader)
    {
        DumpIntReg(data->I0);
        DumpInt32x4Reg(data->I4_1);
        DumpIntReg(data->I2);
    }

    template <class T>
    void AsmJsByteCodeDumper::DumpFloat32x4_2Int1Float1(OpCodeAsmJs op, const unaligned T * data, FunctionBody * dumpFunction, ByteCodeReader& reader)
    {
        DumpFloat32x4Reg(data->F4_0);
        DumpFloat32x4Reg(data->F4_1);
        DumpIntReg(data->I2);
        DumpFloatReg(data->F3);
    }

    template <class T>
    void AsmJsByteCodeDumper::DumpFloat1Float32x4_1Int1(OpCodeAsmJs op, const unaligned T * data, FunctionBody * dumpFunction, ByteCodeReader& reader)
    {
        DumpFloatReg(data->F0);
        DumpInt32x4Reg(data->F4_1);
        DumpIntReg(data->I2);
    }

    template <class T>
    void AsmJsByteCodeDumper::DumpInt32x4_3Int4(OpCodeAsmJs op, const unaligned T * data, FunctionBody * dumpFunction, ByteCodeReader& reader)
    {
        DumpInt32x4Reg(data->I4_0);
        DumpInt32x4Reg(data->I4_1);
        DumpInt32x4Reg(data->I4_2);
        DumpIntReg(data->I3);
        DumpIntReg(data->I4);
        DumpIntReg(data->I5);
        DumpIntReg(data->I6);
    }

    template <class T>
    void AsmJsByteCodeDumper::DumpInt32x4_1Int1(OpCodeAsmJs op, const unaligned T * data, FunctionBody * dumpFunction, ByteCodeReader& reader)
    {
        DumpInt32x4Reg(data->I4_0);
        DumpIntReg(data->I1);
    }

    template <class T>
    void AsmJsByteCodeDumper::DumpInt32x4_2Int1(OpCodeAsmJs op, const unaligned T * data, FunctionBody * dumpFunction, ByteCodeReader& reader)
    {
        DumpInt32x4Reg(data->I4_0);
        DumpInt32x4Reg(data->I4_1);
        DumpIntReg(data->I2);
    }

    template <class T>
    void AsmJsByteCodeDumper::DumpReg1Int32x4_1(OpCodeAsmJs op, const unaligned T * data, FunctionBody * dumpFunction, ByteCodeReader& reader)
    {
        DumpReg(data->R0);
        DumpInt32x4Reg(data->I4_1);
    }

    template <class T>
    void AsmJsByteCodeDumper::DumpInt32x4_1Float32x4_1(OpCodeAsmJs op, const unaligned T * data, FunctionBody * dumpFunction, ByteCodeReader& reader)
    {
        DumpInt32x4Reg(data->I4_0);
        DumpFloat32x4Reg(data->F4_1);
    }

    template <class T>
    void AsmJsByteCodeDumper::DumpInt32x4_1Float64x2_1(OpCodeAsmJs op, const unaligned T * data, FunctionBody * dumpFunction, ByteCodeReader& reader)
    {
        DumpInt32x4Reg(data->I4_0);
        DumpFloat64x2Reg(data->D2_1);
    }

    template <class T>
    void AsmJsByteCodeDumper::DumpInt1Int32x4_1(OpCodeAsmJs op, const unaligned T * data, FunctionBody * dumpFunction, ByteCodeReader& reader)
    {
        DumpIntReg(data->I0);
        DumpFloat64x2Reg(data->I4_1);
    }

    // Float64x2
    template <class T>
    void AsmJsByteCodeDumper::DumpFloat64x2_2(OpCodeAsmJs op, const unaligned T * data, FunctionBody * dumpFunction, ByteCodeReader& reader)
    {
        DumpFloat64x2Reg(data->D2_0);
        DumpFloat64x2Reg(data->D2_1);
    }

    template <class T>
    void AsmJsByteCodeDumper::DumpFloat64x2_3(OpCodeAsmJs op, const unaligned T * data, FunctionBody * dumpFunction, ByteCodeReader& reader)
    {
        DumpFloat64x2Reg(data->D2_0);
        DumpFloat64x2Reg(data->D2_1);
        DumpFloat64x2Reg(data->D2_2);
    }

    template <class T>
    void AsmJsByteCodeDumper::DumpFloat64x2_4(OpCodeAsmJs op, const unaligned T * data, FunctionBody * dumpFunction, ByteCodeReader& reader)
    {
        DumpFloat64x2Reg(data->D2_0);
        DumpFloat64x2Reg(data->D2_1);
        DumpFloat64x2Reg(data->D2_2);
        DumpFloat64x2Reg(data->D2_3);
    }

    template <class T>
    void AsmJsByteCodeDumper::DumpFloat64x2_1Double2(OpCodeAsmJs op, const unaligned T * data, FunctionBody * dumpFunction, ByteCodeReader& reader)
    {
        DumpFloat64x2Reg(data->D2_0);
        DumpDoubleReg(data->D1);
        DumpDoubleReg(data->D2);
    }

    template <class T>
    void AsmJsByteCodeDumper::DumpFloat64x2_1Double1(OpCodeAsmJs op, const unaligned T * data, FunctionBody * dumpFunction, ByteCodeReader& reader)
    {
        DumpFloat64x2Reg(data->D2_0);
        DumpDoubleReg(data->D1);
    }

    template <class T>
    void AsmJsByteCodeDumper::DumpFloat64x2_2Double1(OpCodeAsmJs op, const unaligned T * data, FunctionBody * dumpFunction, ByteCodeReader& reader)
    {
        DumpFloat64x2Reg(data->D2_0);
        DumpFloat64x2Reg(data->D2_1);
        DumpDoubleReg(data->D2);
    }

    template <class T>
    void AsmJsByteCodeDumper::DumpFloat64x2_2Int2(OpCodeAsmJs op, const unaligned T * data, FunctionBody * dumpFunction, ByteCodeReader& reader)
    {
        DumpFloat64x2Reg(data->D2_0);
        DumpFloat64x2Reg(data->D2_1);
        DumpIntReg(data->I2);
        DumpIntReg(data->I3);
    }

    template <class T>
    void AsmJsByteCodeDumper::DumpFloat64x2_3Int2(OpCodeAsmJs op, const unaligned T * data, FunctionBody * dumpFunction, ByteCodeReader& reader)
    {
        DumpFloat64x2Reg(data->D2_0);
        DumpFloat64x2Reg(data->D2_1);
        DumpFloat64x2Reg(data->D2_2);
        DumpIntReg(data->I3);
        DumpIntReg(data->I4);
    }

    template <class T>
    void AsmJsByteCodeDumper::DumpFloat64x2_1Float32x4_1(OpCodeAsmJs op, const unaligned T * data, FunctionBody * dumpFunction, ByteCodeReader& reader)
    {
        DumpFloat64x2Reg(data->D2_0);
        DumpFloat32x4Reg(data->F4_1);
    }

    template <class T>
    void AsmJsByteCodeDumper::DumpFloat64x2_1Int32x4_1(OpCodeAsmJs op, const unaligned T * data, FunctionBody * dumpFunction, ByteCodeReader& reader)
    {
        DumpFloat64x2Reg(data->D2_0);
        DumpInt32x4Reg(data->I4_1);
    }

    template <class T>
    void AsmJsByteCodeDumper::DumpFloat64x2_1Int32x4_1Float64x2_2(OpCodeAsmJs op, const unaligned T * data, FunctionBody * dumpFunction, ByteCodeReader& reader)
    {
        DumpFloat64x2Reg(data->D2_0);
        DumpInt32x4Reg(data->I4_1);
        DumpFloat64x2Reg(data->D2_2);
        DumpFloat64x2Reg(data->D2_3);
    }

    template <class T>
    void AsmJsByteCodeDumper::DumpReg1Float64x2_1(OpCodeAsmJs op, const unaligned T * data, FunctionBody * dumpFunction, ByteCodeReader& reader)
    {
        DumpReg(data->R0);
        DumpFloat64x2Reg(data->D2_1);
    }

    template <class T>
    void AsmJsByteCodeDumper::DumpInt1Float64x2_1(OpCodeAsmJs op, const unaligned T * data, FunctionBody * dumpFunction, ByteCodeReader& reader)
    {
        DumpIntReg(data->I0);
        DumpFloat64x2Reg(data->D2_1);
    }

    template <class T>
    void AsmJsByteCodeDumper::DumpAsmSimdTypedArr(OpCodeAsmJs op, const unaligned T * data, FunctionBody * dumpFunction, ByteCodeReader& reader)
    {
        wchar_t* heapTag = nullptr;

        switch (data->ViewType)
        {
        case ArrayBufferView::TYPE_INT8:
            heapTag = L"HEAP8"; break;
        case ArrayBufferView::TYPE_UINT8:
            heapTag = L"HEAPU8"; break;
        case ArrayBufferView::TYPE_INT16:
            heapTag = L"HEAP16"; break;
        case ArrayBufferView::TYPE_UINT16:
            heapTag = L"HEAPU16"; break;
        case ArrayBufferView::TYPE_INT32:
            heapTag = L"HEAP32"; break;
        case ArrayBufferView::TYPE_UINT32:
            heapTag = L"HEAPU32"; break;
        case ArrayBufferView::TYPE_FLOAT32:
            heapTag = L"HEAPF32"; break;
        case ArrayBufferView::TYPE_FLOAT64:
            heapTag = L"HEAPF64"; break;
        default:
            Assert(false);
            __assume(false);
            break;
        }

        switch (op)
        {
        case OpCodeAsmJs::Simd128_LdArrConst_I4:
        case OpCodeAsmJs::Simd128_LdArrConst_F4:
        case OpCodeAsmJs::Simd128_LdArrConst_D2:
        case OpCodeAsmJs::Simd128_StArrConst_I4:
        case OpCodeAsmJs::Simd128_StArrConst_F4:
        case OpCodeAsmJs::Simd128_StArrConst_D2:
            Output::Print(L" %s[%d] ", heapTag, data->SlotIndex);
            break;
        case OpCodeAsmJs::Simd128_LdArr_I4:
        case OpCodeAsmJs::Simd128_LdArr_F4:
        case OpCodeAsmJs::Simd128_LdArr_D2:
        case OpCodeAsmJs::Simd128_StArr_I4:
        case OpCodeAsmJs::Simd128_StArr_F4:
        case OpCodeAsmJs::Simd128_StArr_D2:
            Output::Print(L" %s[I%d] ", heapTag, data->SlotIndex);
            break;
        default:
            Assert(false);
            __assume(false);
            break;
        }

        switch (op)
        {
        case OpCodeAsmJs::Simd128_LdArr_I4:
        case OpCodeAsmJs::Simd128_LdArrConst_I4:
        case OpCodeAsmJs::Simd128_StArr_I4:
        case OpCodeAsmJs::Simd128_StArrConst_I4:
            DumpInt32x4Reg(data->Value);
            break;
        case OpCodeAsmJs::Simd128_LdArr_F4:
        case OpCodeAsmJs::Simd128_LdArrConst_F4:
        case OpCodeAsmJs::Simd128_StArr_F4:
        case OpCodeAsmJs::Simd128_StArrConst_F4:
            DumpFloat32x4Reg(data->Value);
            break;
        case OpCodeAsmJs::Simd128_LdArr_D2:
        case OpCodeAsmJs::Simd128_LdArrConst_D2:
        case OpCodeAsmJs::Simd128_StArr_D2:
        case OpCodeAsmJs::Simd128_StArrConst_D2:
            DumpFloat64x2Reg(data->Value);
            break;
        default:
            Assert(false);
            __assume(false);
            break;
        }

        // data width
        Output::Print(L" %d bytes ", data->DataWidth);
    }
}

#endif
#endif
