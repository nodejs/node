//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "ARMEncode.h"

class Encoder;

enum RelocType {
    RelocTypeBranch26,
    RelocTypeBranch19,
    RelocTypeBranch14,
    RelocTypeLabel
};

enum InstructionType {
    None    = 0,
    Integer = 1,
    Vfp     = 2
};

#define FRAME_REG           RegFP

///---------------------------------------------------------------------------
///
/// class EncoderReloc
///
///---------------------------------------------------------------------------

class EncodeReloc
{
public:
    static void     New(EncodeReloc **pHead, RelocType relocType, BYTE *offset, IR::Instr *relocInstr, ArenaAllocator *alloc) { __debugbreak(); }

public:
    EncodeReloc *   m_next;
    RelocType       m_relocType;
    BYTE *          m_consumerOffset;  // offset in instruction stream
    IR::Instr *     m_relocInstr;
};



///---------------------------------------------------------------------------
///
/// class EncoderMD
///
///---------------------------------------------------------------------------

class EncoderMD
{
public:
    EncoderMD(Func * func) { }
    ptrdiff_t       Encode(IR::Instr * instr, BYTE *pc, BYTE* beginCodeAddress = nullptr) { __debugbreak(); return 0; }
    void            Init(Encoder *encoder) { __debugbreak(); }
    void            ApplyRelocs(size_t codeBufferAddress) { __debugbreak(); }
    static bool     TryConstFold(IR::Instr *instr, IR::RegOpnd *regOpnd) { __debugbreak(); return 0; }
    static bool     TryFold(IR::Instr *instr, IR::RegOpnd *regOpnd) { __debugbreak(); return 0; }
    const BYTE      GetRegEncode(IR::RegOpnd *regOpnd) { __debugbreak(); return 0; }
    const BYTE      GetFloatRegEncode(IR::RegOpnd *regOpnd) { __debugbreak(); return 0; }
    static const BYTE GetRegEncode(RegNum reg) { __debugbreak(); return 0; }
    static uint32   GetOpdope(IR::Instr *instr) { __debugbreak(); return 0; }
    static uint32   GetOpdope(Js::OpCode op) { __debugbreak(); return 0; }

    static bool     IsLoad(IR::Instr *instr) { __debugbreak(); return 0; }
    static bool     IsStore(IR::Instr *instr) { __debugbreak(); return 0; }
    static bool     IsShifterUpdate(IR::Instr *instr) { __debugbreak(); return 0; }
    static bool     IsShifterSub(IR::Instr *instr) { __debugbreak(); return 0; }
    static bool     IsShifterPost(IR::Instr *instr) { __debugbreak(); return 0; }
    static bool     SetsSBit(IR::Instr *instr) { __debugbreak(); return 0; }

    void            AddLabelReloc(BYTE* relocAddress) { __debugbreak(); }

    static bool     CanEncodeModConst12(DWORD constant) { __debugbreak(); return 0; }
    static bool     CanEncodeLoadStoreOffset(int32 offset) { __debugbreak(); return 0; }
    static void     BaseAndOffsetFromSym(IR::SymOpnd *symOpnd, RegNum *pBaseReg, int32 *pOffset, Func * func) { __debugbreak(); }
    static bool     EncodeImmediate16(long constant, DWORD * result);

    void            EncodeInlineeCallInfo(IR::Instr *instr, uint32 offset) { __debugbreak(); }

    static ENCODE_32 BranchOffset_26(int64 x);
};
