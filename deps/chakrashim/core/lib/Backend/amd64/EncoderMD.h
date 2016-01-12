//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
class Encoder;


enum RelocType {
    RelocTypeBranch,                // cond, uncond branch
    RelocTypeCallPcrel,             // calls
    RelocTypeLabelUse,              // direct use of a label
    RelocTypeLabel,                 // points to label instr
    RelocTypeAlignedLabel,          // points to loop-top label instr that needs alignment
    RelocTypeInlineeEntryOffset,    // points to offset immediate in buffer
};


///---------------------------------------------------------------------------
///
/// class EncoderReloc
///
///---------------------------------------------------------------------------

class EncodeRelocAndLabels
{
public:
    RelocType           m_type;
    void *              m_ptr;          // points to encoded buffer byte or LabelInstr if RelocTypeLabel
    void *              m_origPtr;      // copy of m_ptr to be used to get offset in the source buffer during BR shortening

private:
    union
    {
        IR::LabelInstr* m_labelInstr;        // ptr to Br Label
        BYTE            m_nopCount;
        uint64          m_origInlineeOffset;
    };
    bool                m_isShortBr;

public:
    void                init(RelocType type, void* ptr, IR::LabelInstr* labelInstr = nullptr)
    {
        m_type = type;
        m_ptr = ptr;
        if (type == RelocTypeLabel)
        {
            // preserve original PC for labels
            m_origPtr   = (void*)((IR::LabelInstr*)ptr)->GetPC();
            m_nopCount  = 0;
        }
        else
        {
            m_origPtr = ptr;
            // in case we have to revert, we need to store original offset in code buffer
            if (type == RelocTypeInlineeEntryOffset)
            {
                m_origInlineeOffset = *((uint64*)m_origPtr);
            }
            else if (type == RelocTypeBranch)
            {
                Assert(labelInstr);
                m_labelInstr = labelInstr;
                m_isShortBr = false;
            }
        }
    }

    void                revert()
    {
        // recover old label PC
        if (isLabel())
        {
            // recover old label PC and reset alignment nops
            // we keep aligned labels type so we align them on the second attempt.
            setLabelCurrPC(getLabelOrigPC());
            m_nopCount = 0;
            return;
        }

        // re-write original inlinee offset to code buffer
        if (m_type == RelocTypeInlineeEntryOffset)
        {
            *(uint64*) m_origPtr = m_origInlineeOffset;
        }

        if (m_type == RelocTypeBranch)
        {
            m_isShortBr = false;
        }

        m_ptr = m_origPtr;
    }

    bool                isLabel()           const { return isAlignedLabel() || m_type == RelocTypeLabel; }
    bool                isAlignedLabel()    const { return m_type == RelocTypeAlignedLabel; }
    bool                isLongBr()          const { return m_type == RelocTypeBranch && !m_isShortBr; }
    bool                isShortBr()         const { return m_type == RelocTypeBranch && m_isShortBr; }
    BYTE*               getBrOpCodeByte()   const { return (BYTE*)m_origPtr - 1;}

    IR::LabelInstr *    getBrTargetLabel()  const
    {
        Assert(m_type == RelocTypeBranch && m_labelInstr);
        return m_labelInstr;
    }
    IR::LabelInstr *    getLabel()  const
    {
        Assert(isLabel());
        return (IR::LabelInstr*) m_ptr;
    }
    // get label original PC without shortening/alignment
    BYTE *  getLabelOrigPC()  const
    {
        Assert(isLabel());
        return ((BYTE*) m_origPtr);
    }

    // get label PC after shortening/alignment
    BYTE *  getLabelCurrPC()  const
    {
        Assert(isLabel());
        return getLabel()->GetPC();
    }

    BYTE    getLabelNopCount() const
    {
        Assert(isAlignedLabel());
        return m_nopCount;
    }

    void    setLabelCurrPC(BYTE* pc)
    {
        Assert(isLabel());
        getLabel()->SetPC(pc);
    }

    void    setLabelNopCount(BYTE nopCount)
    {
        Assert(isAlignedLabel());
        Assert (nopCount >= 0 && nopCount < 16);
        m_nopCount = nopCount;
    }

    void                setAsShortBr()
    {
        Assert(m_type == RelocTypeBranch);
        m_isShortBr = true;
    }

    // Validates if the branch is short and its target PC fits in one byte
    bool                validateShortBrTarget() const
    {
        return isShortBr() &&
            getBrTargetLabel()->GetPC() - ((BYTE*)m_ptr + 1) >= -128 &&
            getBrTargetLabel()->GetPC() - ((BYTE*)m_ptr + 1) <= 127;
    }
};



///---------------------------------------------------------------------------
///
/// class EncoderMD
///
///---------------------------------------------------------------------------
enum Forms : BYTE;

typedef JsUtil::List<EncodeRelocAndLabels, ArenaAllocator> RelocList;
typedef JsUtil::List<InlineeFrameRecord*, ArenaAllocator> InlineeFrameRecords;

class EncoderMD
{
public:
    EncoderMD(Func * func) : m_func(func) {}
    ptrdiff_t       Encode(IR::Instr * instr, BYTE *pc, BYTE* beginCodeAddress = nullptr);
    void            Init(Encoder *encoder);
    void            ApplyRelocs(size_t codeBufferAddress);
    void            EncodeInlineeCallInfo(IR::Instr *instr, uint32 offset);
    static bool     TryConstFold(IR::Instr *instr, IR::RegOpnd *regOpnd);
    static bool     TryFold(IR::Instr *instr, IR::RegOpnd *regOpnd);
    static bool     SetsConditionCode(IR::Instr *instr);
    static bool     UsesConditionCode(IR::Instr *instr);
    static bool     IsOPEQ(IR::Instr *instr);
    RelocList*      GetRelocList() const { return m_relocList; }
    int             AppendRelocEntry(RelocType type, void *ptr, IR::LabelInstr *label= nullptr);
    int             FixRelocListEntry(uint32 index, int totalBytesSaved, BYTE *buffStart, BYTE* buffEnd);
    void            FixMaps(uint32 brOffset, uint32 bytesSaved, uint32 *inlineeFrameRecordsIndex, uint32 *inlineeFrameMapIndex,  uint32 *pragmaInstToRecordOffsetIndex, uint32 *offsetBuffIndex);
    void            UpdateRelocListWithNewBuffer(RelocList * relocList, BYTE * newBuffer, BYTE * oldBufferStart, BYTE * oldBufferEnd);
#ifdef DBG
    void            VerifyRelocList(BYTE *buffStart, BYTE *buffEnd);
#endif
    void            AddLabelReloc(BYTE* relocAddress);

private:
    const BYTE      GetOpcodeByte2(IR::Instr *instr);
    static Forms    GetInstrForm(IR::Instr *instr);
    const BYTE *    GetFormTemplate(IR::Instr *instr);
    const BYTE *    GetOpbyte(IR::Instr *instr);
    const BYTE      GetRegEncode(IR::RegOpnd *regOpnd);
    const BYTE      GetRegEncode(RegNum reg);
    static const uint32 GetOpdope(IR::Instr *instr);
    const uint32    GetLeadIn(IR::Instr * instr);
    BYTE            EmitModRM(IR::Instr * instr, IR::Opnd *opnd, BYTE reg1);
    void            EmitConst(size_t val, int size, bool allowImm64 = false);
    BYTE            EmitImmed(IR::Opnd * opnd, int opSize, int sbit, bool allowImm64 = false);
    static bool     FitsInByte(size_t value);
    bool            IsExtendedRegister(RegNum reg);
    BYTE            GetMod(IR::IndirOpnd * opr, int* pDispSize);
    BYTE            GetMod(IR::SymOpnd * opr, int* pDispSize, RegNum& rmReg);
    BYTE            GetMod(size_t offset, bool regIsRbpOrR13, int * pDispSize);
    BYTE            GetRexByte(BYTE rexCode, IR::Opnd * opnd);
    BYTE            GetRexByte(BYTE rexCode, RegNum reg);
    int             GetOpndSize(IR::Opnd * opnd);

    void            EmitRexByte(BYTE * prexByte, BYTE rexByte, bool skipRexByte, bool reservedRexByte);

    enum
    {
        REXOVERRIDE  = 0x40,
        REXW = 8,
        REXR = 4,
        REXX = 2,
        REXB = 1,
    };

    static const BYTE Mod00 = 0x00 << 6;
    static const BYTE Mod01 = 0x01 << 6;
    static const BYTE Mod10 = 0x02 << 6;
    static const BYTE Mod11 = 0x03 << 6;

private:
    Func *          m_func;
    Encoder *       m_encoder;
    BYTE *          m_pc;
    RelocList *     m_relocList;
    int             m_lastLoopLabelPosition;
};
