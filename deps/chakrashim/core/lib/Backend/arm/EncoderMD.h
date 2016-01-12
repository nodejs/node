//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "ARMEncode.h"
#include "LegalizeMD.h"

class Encoder;

enum RelocType {
    RelocTypeBranch20,
    RelocTypeBranch24,
    RelocTypeDataLabelLow,
    RelocTypeLabelLow,
    RelocTypeLabelHigh,
    RelocTypeLabel
};

enum InstructionType {
    None    = 0,
    Thumb   = 2,
    Vfp     = 3,
    Thumb2  = 4,
};

///---------------------------------------------------------------------------
///
/// class EncoderReloc
///
///---------------------------------------------------------------------------

class EncodeReloc
{
public:
    static void     New(EncodeReloc **pHead, RelocType relocType, BYTE *offset, IR::Instr *relocInstr, ArenaAllocator *alloc);

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
    EncoderMD(Func * func) : m_func(func), consecutiveThumbInstrCount(0) { }
    ptrdiff_t       Encode(IR::Instr * instr, BYTE *pc, BYTE* beginCodeAddress = nullptr);
    void            Init(Encoder *encoder);
    void            ApplyRelocs(uint32 codeBufferAddress);
    static bool     TryConstFold(IR::Instr *instr, IR::RegOpnd *regOpnd);
    static bool     TryFold(IR::Instr *instr, IR::RegOpnd *regOpnd);
    const BYTE      GetRegEncode(IR::RegOpnd *regOpnd);
    const BYTE      GetFloatRegEncode(IR::RegOpnd *regOpnd);
    static const BYTE GetRegEncode(RegNum reg);
    static uint32   GetOpdope(IR::Instr *instr);
    static uint32   GetOpdope(Js::OpCode op);

    static bool     IsLoad(IR::Instr *instr)
    {
        return ISLOAD(instr->m_opcode);
    }

    static bool     IsStore(IR::Instr *instr)
    {
        return ISSTORE(instr->m_opcode);
    }

    static bool     IsShifterUpdate(IR::Instr *instr)
    {
        return ISSHIFTERUPDATE(instr->m_opcode);
    }

    static bool     IsShifterSub(IR::Instr *instr)
    {
        return ISSHIFTERSUB(instr->m_opcode);
    }

    static bool     IsShifterPost(IR::Instr *instr)
    {
        return ISSHIFTERPOST(instr->m_opcode);
    }

    static bool     SetsSBit(IR::Instr *instr)
    {
        return SETS_SBIT(instr->m_opcode);
    }

    void            AddLabelReloc(BYTE* relocAddress);

    static bool     CanEncodeModConst12(DWORD constant);
    static bool     CanEncodeLoadStoreOffset(int32 offset) { return IS_CONST_UINT12(offset); }
    static void     BaseAndOffsetFromSym(IR::SymOpnd *symOpnd, RegNum *pBaseReg, int32 *pOffset, Func * func);
    static bool     EncodeImmediate16(long constant, DWORD * result);
    static ENCODE_32  BranchOffset_T2_24(int x);
    void            EncodeInlineeCallInfo(IR::Instr *instr, uint32 offset);
private:
    Func *          m_func;
    Encoder *       m_encoder;
    BYTE *          m_pc;
    EncodeReloc *   m_relocList;
    uint            consecutiveThumbInstrCount; //Count of consecutive 16 bit thumb instructions.
private:
    int             GetForm(IR::Instr *instr, int32 size);
    ENCODE_32       GenerateEncoding(IR::Instr* instr, IFORM iform, BYTE *pc, int32 size, InstructionType intrType);
    InstructionType CanonicalizeInstr(IR::Instr *instr);
    InstructionType CanonicalizeAdd(IR::Instr * instr);
    InstructionType CanonicalizeSub(IR::Instr * instr);
    InstructionType CanonicalizeMov(IR::Instr * instr);
    InstructionType CanonicalizeLoad(IR::Instr * instr);
    InstructionType CanonicalizeStore(IR::Instr * instr);
    InstructionType CanonicalizeLea(IR::Instr * instr);
    InstructionType CmpEncodeType(IR::Instr * instr);
    InstructionType CmnEncodeType(IR::Instr * instr);
    InstructionType PushPopEncodeType(IR::IndirOpnd *target, IR::RegBVOpnd * opnd);
    InstructionType Alu2EncodeType(IR::Opnd *opnd1, IR::Opnd *opnd2);
    InstructionType Alu3EncodeType(IR::Instr * instr);
    InstructionType ShiftEncodeType(IR::Instr * instr);
    bool            IsWideMemInstr(IR::Opnd * memOpnd, IR::RegOpnd * regOpnd);
    bool            IsWideAddSub(IR::Instr * instr);

    static ENCODE_32 EncodeT2Immediate12(ENCODE_32 encode, long constant);
    static bool     EncodeModConst12(DWORD constant, DWORD * result);
    static ENCODE_32 EncodeT2Offset(ENCODE_32 encode, IR::Instr *instr, int offset, int bitOffset);

#ifdef SOFTWARE_FIXFOR_HARDWARE_BUGWIN8_502326
    static bool     CheckBranchInstrCriteria(IR::Instr* instr);
    static bool     IsBuggyHardware();
#endif

    ENCODE_32       BranchOffset_T2_20(int x);
    ENCODE_32       CallOffset(int x);

    int             IndirForm(int form, int *pOpnnum, RegNum baseReg, IR::Opnd *indexOpnd);

};
