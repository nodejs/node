//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once

class Security
{
private:
    Func *func;

public:
    Security(Func * func) : func(func) {}

    void            EncodeLargeConstants();
    void            InsertNOPs();
    static bool     DontEncode(IR::Opnd *opnd);
    static void     InsertRandomFunctionPad(IR::Instr * instrBeforeInstr);

private:
    void            EncodeOpnd(IR::Instr *instr, IR::Opnd *opnd);
    IntConstType    EncodeValue(IR::Instr *instr, IR::Opnd *opnd, IntConstType constValue, IR::RegOpnd ** pNewOpnd);
#ifdef _M_X64
    size_t          EncodeAddress(IR::Instr *instr, IR::Opnd *opnd, size_t value, IR::RegOpnd **pNewOpnd);
#endif

    // Large constants have more than 16 significant bits.
    // Constants except these are considered large: 0x0000????, 0xffff????, 0x????0000, 0x????ffff
    static bool     IsLargeConstant(int32 value) { return static_cast<int16>(value) != 0 && static_cast<int16>(value) != -1 && (value >> 16) != 0 && (value >> 16) != -1; }

    void            InsertNOPBefore(IR::Instr *instr);
    int             GetNextNOPInsertPoint();

    // Insert 1-4 bytes of NOPs
    static void     InsertSmallNOP(IR::Instr * instrBeforeInstr, DWORD nopSize);
};
