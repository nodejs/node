//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once

enum UnwindCode;

class UnwindInfoManager
{
public:
    UnwindInfoManager() :
        func(NULL),
        fragmentStart(NULL),
        fragmentLength(0),
        epilogEndOffset(0),
        prologOffset(0),
        savedRegMask(0),
        savedDoubleRegMask(0),
        homedParamCount(0),
        stackDepth(0),
        hasCalls(false),
        prologLabelId(0),
        epilogEndLabelId(0),
        workItem(nullptr),
        xdataTotal(0),
        pdataIndex(0),
        savedScratchReg(false)
    {
    }

    void Init(Func * func);
    void EmitUnwindInfo(CodeGenWorkItem *workItem);
    DWORD EmitLongUnwindInfoChunk(DWORD remainingLength);

    void SetFunc(Func *func)
    {
        Assert(this->func == NULL);
        this->func = func;
    }
    Func * GetFunc() const
    {
        return this->func;
    }

    void SetFragmentStart(size_t pStart)
    {
        this->fragmentStart = pStart;
    }
    size_t GetFragmentStart() const
    {
        return this->fragmentStart;
    }

    void SetEpilogEndOffset(DWORD offset)
    {
        Assert(this->epilogEndOffset == 0);
        this->epilogEndOffset = offset;
    }
    DWORD GetEpilogEndOffset() const
    {
        return this->epilogEndOffset;
    }

    void SetPrologOffset(DWORD offset)
    {
        Assert(this->prologOffset == 0);
        this->prologOffset = offset;
    }
    DWORD GetPrologOffset() const
    {
        return this->prologOffset;
    }

    void SetFragmentLength(DWORD length)
    {
        this->fragmentLength = length;
    }
    DWORD GetFragmentLength() const
    {
        return this->fragmentLength;
    }

    void SetHomedParamCount(BYTE count)
    {
        Assert(this->homedParamCount == 0);
        this->homedParamCount = count;
    }
    DWORD GetHomedParamCount() const
    {
        return this->homedParamCount;
    }

    void SetStackDepth(DWORD depth)
    {
        Assert(this->stackDepth == 0);
        this->stackDepth = depth;
    }
    DWORD GetStackDepth() const
    {
        return this->stackDepth;
    }

    void SetHasCalls(bool has)
    {
        this->hasCalls = has;
    }
    bool GetHasCalls() const
    {
        return this->hasCalls;
    }

    void SetPrologStartLabel(DWORD id)
    {
        Assert(this->prologLabelId == 0);
        this->prologLabelId = id;
    }
    DWORD GetPrologStartLabel() const
    {
        return this->prologLabelId;
    }

    void SetEpilogEndLabel(DWORD id)
    {
        Assert(this->epilogEndLabelId == 0);
        this->epilogEndLabelId = id;
    }
    DWORD GetEpilogEndLabel() const
    {
        return this->epilogEndLabelId;
    }

    bool GetHasChkStk() const;
    DWORD GetPDataCount(DWORD length);
    void SetSavedReg(BYTE reg);
    DWORD ClearSavedReg(DWORD mask, BYTE reg) const;

    void SetDoubleSavedRegList(DWORD doubleRegMask);
    DWORD GetDoubleSavedRegList() const;

    static BYTE GetLastSavedReg(DWORD mask);
    static BYTE GetFirstSavedReg(DWORD mask);

    void SetSavedScratchReg(bool value) { savedScratchReg = value; }
    bool GetSavedScratchReg() { return savedScratchReg; }

private:

    Func * func;
    size_t fragmentStart;
    int pdataIndex;
    int xdataTotal;
    CodeGenWorkItem *workItem;
    DWORD fragmentLength;
    DWORD prologOffset;
    DWORD prologLabelId;
    DWORD epilogEndLabelId;
    DWORD epilogEndOffset;
    DWORD savedRegMask;
    DWORD savedDoubleRegMask;
    DWORD stackDepth;
    BYTE homedParamCount;
    bool hasCalls;
    bool fragmentHasProlog;
    bool fragmentHasEpilog;
    bool savedScratchReg;

    void EmitPdata();
    bool CanEmitPackedPdata() const;
    void EncodePackedUnwindData();
    void EncodeExpandedUnwindData();
    BYTE * GetBaseAddress();

    bool IsPdataPacked(const DWORD *pdata) const;
    bool IsR4SavedRegRange(bool saveR11) const;
    static bool IsR4SavedRegRange(DWORD saveRegMask);

    DWORD XdataTemplate(UnwindCode op) const;
    DWORD XdataLength(UnwindCode op) const;

    DWORD EmitXdataStackAlloc(BYTE xData[], DWORD byte, DWORD stack);
    DWORD EmitXdataHomeParams(BYTE xData[], DWORD byte);
    DWORD EmitXdataRestoreRegs(BYTE xData[], DWORD byte, DWORD savedRegMask, bool restoreLR);
    DWORD EmitXdataRestoreDoubleRegs(BYTE xData[], DWORD byte, DWORD savedDoubleRegMask);
    DWORD EmitXdataIndirReturn(BYTE xData[], DWORD byte);
    DWORD EmitXdataNop32(BYTE xData[], DWORD byte);
    DWORD EmitXdataNop16(BYTE xData[], DWORD byte);
    DWORD EmitXdataEnd(BYTE xData[], DWORD byte);
    DWORD EmitXdataEndPlus16(BYTE xData[], DWORD byte);
    DWORD EmitXdataLocalsPointer(BYTE xData[], DWORD byte, BYTE regEncode);

    DWORD RelativeRegEncoding(RegNum reg, RegNum baseReg) const;
    DWORD WriteXdataBytes(BYTE xdata[], DWORD byte, DWORD encoding, DWORD length);

    // Constants defined in the ABI.

    static const DWORD MaxPackedPdataFuncLength = 0xFFE;
    static const DWORD MaxPackedPdataStackDepth = 0xFCC;

    static const DWORD PackedPdataFlagMask = 3;
    static const DWORD ExpandedPdataFlag = 0;

    // Function length is required to have only these bits set.
    static const DWORD PackedFuncLengthMask = 0xFFE;
    // Bit offset of length within pdata dword, combined with right-shift of encoded length.
    static const DWORD PackedFuncLengthShift = 1;

    static const DWORD PackedNoPrologBits = 2;
    static const DWORD PackedNormalFuncBits = 1;

    static const DWORD PackedNonLeafRetBits = 0;
    static const DWORD PackedLeafRetBits = (1 << 13);
    static const DWORD PackedNoEpilogBits = (3 << 13);

    // C (frame chaining) and L (save LR) bits.
    static const DWORD PackedNonLeafFunctionBits = (1 << 20) | (1 << 21);

    static const DWORD PackedHomedParamsBit = (1 << 15);

    static const DWORD PackedRegShift = 16;
    static const DWORD PackedRegMask = 7;
    // Indicate no saved regs with a Reg field of 0x111 and the R bit set.
    static const DWORD PackedNoSavedRegsBits = (7 << PackedRegShift) | (1 << 19);

    // Stack depth is required to have only these bits set.
    static const DWORD PackedStackDepthMask = 0xFFC;
    // Bit offset of stack depth within pdata dword, combined with right-shift of encoded value.
    static const DWORD PackedStackDepthShift = 20;

    static const DWORD MaxXdataFuncLength = 0x7FFFE;
    static const DWORD XdataFuncLengthMask = 0x7FFFE;
    static const DWORD XdataFuncLengthAdjust = 1;

    static const DWORD XdataSingleEpilogShift = 21;
    static const DWORD XdataFuncFragmentShift = 22;
    static const DWORD XdataEpilogCountShift = 23;

    static const DWORD MaxXdataEpilogCount = 0x1F;
    static const DWORD MaxXdataDwordCount = 0xF;
    static const DWORD XdataDwordCountShift = 28;

public:
    // Xdata constants.
    static const DWORD MaxXdataBytes = 40; //buffer of 4 for any future additions
    //
    // 28 == 4 (header DWORD) +
    //      (4 (max stack alloc code) +
    //       1 (locals pointer setup) +
    //       5 (NOP for _chkstk case) +
    //       1 (double reg saves) +
    //       2 (reg saves) +
    //       1 (r11 setup) +
    //       2 (r11,lr saves) +
    //       1 (home params) +
    //       1 (NOP) +
    //       1 (end prolog) +
    //       4 (max stack alloc code, in case of locals pointer setup) +
    //       1 (double reg saves) +
    //       2 (reg saves) +
    //       2 (r11 save) +
    //       2 (indir return) +
    //       1 (end epilog)) rounded up to a DWORD boundary

};
