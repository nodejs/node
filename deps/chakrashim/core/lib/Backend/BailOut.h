//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once

struct GlobalBailOutRecordDataTable;
struct GlobalBailOutRecordDataRow;

class BailOutInfo
{
public:

#ifdef _M_IX86
    typedef struct
    {
        IR::Instr * instr;
        uint argCount;
        uint argRestoreAdjustCount;
    } StartCallInfo;
#else
    typedef uint StartCallInfo;
#endif

    BailOutInfo(uint32 bailOutOffset, Func* bailOutFunc) :
        bailOutOffset(bailOutOffset), bailOutFunc(bailOutFunc),
        byteCodeUpwardExposedUsed(nullptr), polymorphicCacheIndex((uint)-1), startCallCount(0), startCallInfo(nullptr), bailOutInstr(nullptr),
        totalOutParamCount(0), argOutSyms(nullptr), bailOutRecord(nullptr), wasCloned(false), isInvertedBranch(false), sharedBailOutKind(true), outParamInlinedArgSlot(nullptr),
        liveVarSyms(nullptr), liveLosslessInt32Syms(nullptr), liveFloat64Syms(nullptr), liveSimd128F4Syms(nullptr), liveSimd128I4Syms(nullptr), liveSimd128D2Syms(nullptr), branchConditionOpnd(nullptr),
        stackLiteralBailOutInfoCount(0), stackLiteralBailOutInfo(nullptr)
    {
        Assert(bailOutOffset != Js::Constants::NoByteCodeOffset);
#ifdef _M_IX86
        outParamFrameAdjustArgSlot = nullptr;
#endif
#if DBG
        wasCopied = false;
#endif
        this->capturedValues.argObjSyms = nullptr;
        this->usedCapturedValues.argObjSyms = nullptr;
    }
    void Clear(JitArenaAllocator * allocator);

    void FinalizeBailOutRecord(Func * func);
#ifdef MD_GROW_LOCALS_AREA_UP
    void FinalizeOffsets(__in_ecount(count) int * offsets, uint count, Func *func, BVSparse<JitArenaAllocator> *bvInlinedArgSlot);
#endif

    void RecordStartCallInfo(uint i, uint argRestoreAdjustCount, IR::Instr *instr);
    uint GetStartCallOutParamCount(uint i) const;
#ifdef _M_IX86
    bool NeedsStartCallAdjust(uint i, const IR::Instr * instr) const;
    void UnlinkStartCall(const IR::Instr * instr);
#endif

    static bool IsBailOutOnImplicitCalls(IR::BailOutKind kind)
    {
        const IR::BailOutKind kindMinusBits = kind & ~IR::BailOutKindBits;
        return kindMinusBits == IR::BailOutOnImplicitCalls ||
            kindMinusBits == IR::BailOutOnImplicitCallsPreOp;
    }

#if DBG
    static bool IsBailOutHelper(IR::JnHelperMethod helper);
#endif
    bool wasCloned;
    bool isInvertedBranch;
    bool sharedBailOutKind;

#if DBG
    bool wasCopied;
#endif
    uint32 bailOutOffset;
    BailOutRecord * bailOutRecord;
    CapturedValues capturedValues;                                      // Values we know about after forward pass
    CapturedValues usedCapturedValues;                                  // Values that need to be restored in the bail out
    BVSparse<JitArenaAllocator> * byteCodeUpwardExposedUsed;            // Non-constant stack syms that needs to be restored in the bail out
    uint polymorphicCacheIndex;
    uint startCallCount;
    uint totalOutParamCount;
    Func ** startCallFunc;

    StartCallInfo * startCallInfo;
    StackSym ** argOutSyms;

    struct StackLiteralBailOutInfo
    {
        StackSym * stackSym;
        uint initFldCount;
    };
    uint stackLiteralBailOutInfoCount;
    StackLiteralBailOutInfo * stackLiteralBailOutInfo;

    BVSparse<JitArenaAllocator> * liveVarSyms;
    BVSparse<JitArenaAllocator> * liveLosslessInt32Syms;                // These are only the live int32 syms that fully represent the var-equivalent sym's value (see GlobOpt::FillBailOutInfo)
    BVSparse<JitArenaAllocator> * liveFloat64Syms;

    // SIMD_JS
    BVSparse<JitArenaAllocator> * liveSimd128F4Syms;
    BVSparse<JitArenaAllocator> * liveSimd128I4Syms;
    BVSparse<JitArenaAllocator> * liveSimd128D2Syms;

    int * outParamOffsets;

    BVSparse<JitArenaAllocator> * outParamInlinedArgSlot;
#ifdef _M_IX86
    BVSparse<JitArenaAllocator> * outParamFrameAdjustArgSlot;
    BVFixed * inlinedStartCall;
#endif

#ifdef MD_GROW_LOCALS_AREA_UP
    // Use a bias to guarantee that all sym offsets are non-zero.
    static const int32 StackSymBias = MachStackAlignment;
#endif

    // The actual bailout instr, this is normally the instr that has the bailout info.
    // 1) If we haven't generated bailout (which happens in lowerer) for this bailout info, this is either of:
    // - the instr that has bailout info.
    // - in case of shared bailout this will be the BailTarget instr (corresponds to the call to SaveReesgtersAndBailOut,
    //   while other instrs sharing bailout info will just have checks and JMP to BailTarget).
    // 2) After we generated bailout, this becomes label instr. In case of shared bailout other instrs JMP to this label.
    IR::Instr * bailOutInstr;

#if ENABLE_DEBUG_CONFIG_OPTIONS
    Js::OpCode bailOutOpcode;
#endif
    Func * bailOutFunc;
    IR::Opnd * branchConditionOpnd;

    template<class Fn>
    void IterateArgOutSyms(Fn callback)
    {
        uint argOutIndex = 0;
        for(uint i = 0; i < this->startCallCount; i++)
        {
            uint outParamCount = this->GetStartCallOutParamCount(i);
            for(uint j = 0; j < outParamCount; j++)
            {
                StackSym* sym = this->argOutSyms[argOutIndex];
                if(sym)
                {
                    callback(argOutIndex, i + sym->GetArgSlotNum(), sym);
                }
                argOutIndex++;
            }
        }
    }
};

class BailOutRecord
{
public:
    BailOutRecord(uint32 bailOutOffset, uint bailOutCacheIndex, IR::BailOutKind kind, Func *bailOutFunc);
    static Js::Var BailOut(BailOutRecord const * bailOutRecord);
    static Js::Var BailOutFromFunction(Js::JavascriptCallStackLayout * layout, BailOutRecord const * bailOutRecord, void * returnAddress, void * argoutRestoreAddress);
    static uint32 BailOutFromLoopBody(Js::JavascriptCallStackLayout * layout, BailOutRecord const * bailOutRecord);

    static Js::Var BailOutInlined(Js::JavascriptCallStackLayout * layout, BailOutRecord const * bailOutRecord, void * returnAddress);
    static uint32 BailOutFromLoopBodyInlined(Js::JavascriptCallStackLayout * layout, BailOutRecord const * bailOutRecord, void * returnAddress);

    static Js::Var BailOutForElidedYield(void * framePointer);

    static size_t GetOffsetOfPolymorphicCacheIndex() { return offsetof(BailOutRecord, polymorphicCacheIndex); }
    static size_t GetOffsetOfBailOutKind() { return offsetof(BailOutRecord, bailOutKind); }

    static bool IsArgumentsObject(uint32 offset);
    static uint32 GetArgumentsObjectOffset();
    static const uint BailOutRegisterSaveSlotCount = LinearScanMD::RegisterSaveSlotCount;

public:
    template <size_t N>
    void FillNativeRegToByteCodeRegMap(uint (&nativeRegToByteCodeRegMap)[N]);

    void IsOffsetNativeIntOrFloat(uint offsetIndex, int argOutSlotStart, bool * pIsFloat64, bool * pIsInt32, bool * pIsSimd128F4, bool * pIsSImd128I4) const;

    template <typename Fn>
    void MapStartCallParamCounts(Fn fn);

    void SetBailOutKind(IR::BailOutKind bailOutKind) { this->bailOutKind = bailOutKind; }
    uint32 GetBailOutOffset() { return bailOutOffset; }
#if ENABLE_DEBUG_CONFIG_OPTIONS
    Js::OpCode GetBailOutOpCode() { return bailOutOpcode; }
#endif
    template <typename Fn>
    void MapArgOutOffsets(Fn fn);

protected:
    struct BailOutReturnValue
    {
        Js::Var returnValue;
        Js::RegSlot returnValueRegSlot;
    };
    static Js::Var BailOutCommonNoCodeGen(Js::JavascriptCallStackLayout * layout, BailOutRecord const * bailOutRecord,
        uint32 bailOutOffset, void * returnAddress, IR::BailOutKind bailOutKind, Js::Var branchValue = nullptr, Js::Var * registerSaves = nullptr,
        BailOutReturnValue * returnValue = nullptr, void * argoutRestoreAddress = nullptr);
    static Js::Var BailOutCommon(Js::JavascriptCallStackLayout * layout, BailOutRecord const * bailOutRecord,
        uint32 bailOutOffset, void * returnAddress, IR::BailOutKind bailOutKind, Js::Var branchValue = nullptr, BailOutReturnValue * returnValue = nullptr, void * argoutRestoreAddress = nullptr);
    static Js::Var BailOutInlinedCommon(Js::JavascriptCallStackLayout * layout, BailOutRecord const * bailOutRecord,
        uint32 bailOutOffset, void * returnAddress, IR::BailOutKind bailOutKind, Js::Var branchValue = nullptr);
    static uint32 BailOutFromLoopBodyCommon(Js::JavascriptCallStackLayout * layout, BailOutRecord const * bailOutRecord,
        uint32 bailOutOffset, IR::BailOutKind bailOutKind, Js::Var branchValue = nullptr);
    static uint32 BailOutFromLoopBodyInlinedCommon(Js::JavascriptCallStackLayout * layout, BailOutRecord const * bailOutRecord,
        uint32 bailOutOffset, void * returnAddress, IR::BailOutKind bailOutKind, Js::Var branchValue = nullptr);

    static Js::Var BailOutHelper(Js::JavascriptCallStackLayout * layout, Js::ScriptFunction ** functionRef, Js::Arguments& args, const bool boxArgs,
        BailOutRecord const * bailOutRecord, uint32 bailOutOffset, void * returnAddress, IR::BailOutKind bailOutKind, Js::Var * registerSaves, BailOutReturnValue * returnValue, Js::Var* pArgumentsObject,
        Js::Var branchValue = nullptr, void * argoutRestoreAddress = nullptr);
    static void BailOutInlinedHelper(Js::JavascriptCallStackLayout * layout, BailOutRecord const *& bailOutRecord, uint32 bailOutOffset, void * returnAddress,
        IR::BailOutKind bailOutKind, Js::Var * registerSaves, BailOutReturnValue * returnValue, Js::ScriptFunction ** innerMostInlinee, bool isInLoopBody, Js::Var branchValue = nullptr);
    static uint32 BailOutFromLoopBodyHelper(Js::JavascriptCallStackLayout * layout, BailOutRecord const * bailOutRecord,
        uint32 bailOutOffset, IR::BailOutKind bailOutKind, Js::Var branchValue = nullptr, Js::Var * registerSaves = nullptr, BailOutReturnValue * returnValue = nullptr);

    static void UpdatePolymorphicFieldAccess(Js::JavascriptFunction *  function, BailOutRecord const * bailOutRecord);

    static void ScheduleFunctionCodeGen(Js::ScriptFunction * function, Js::ScriptFunction * innerMostInlinee, BailOutRecord const * bailOutRecord, IR::BailOutKind bailOutKind, void * returnAddress);
    static void ScheduleLoopBodyCodeGen(Js::ScriptFunction * function, Js::ScriptFunction * innerMostInlinee, BailOutRecord const * bailOutRecord, IR::BailOutKind bailOutKind);

    void RestoreValues(IR::BailOutKind bailOutKind, Js::JavascriptCallStackLayout * layout, Js::InterpreterStackFrame * newInstance, Js::ScriptContext * scriptContext,
        bool fromLoopBody, Js::Var * registerSaves, BailOutReturnValue * returnValue, Js::Var* pArgumentsObject, Js::Var branchValue = nullptr, void* returnAddress = nullptr, bool useStartCall = true, void * argoutRestoreAddress = nullptr) const;
    void RestoreValues(IR::BailOutKind bailOutKind, Js::JavascriptCallStackLayout * layout, uint count, __in_ecount_opt(count) int * offsets, int argOutSlotId,
        __out_ecount(count)Js::Var * values, Js::ScriptContext * scriptContext, bool fromLoopBody, Js::Var * registerSaves, Js::InterpreterStackFrame * newInstance, Js::Var* pArgumentsObject,
        void * argoutRestoreAddress = nullptr) const;
    void RestoreValue(IR::BailOutKind bailOutKind, Js::JavascriptCallStackLayout * layout, Js::Var * values, Js::ScriptContext * scriptContext,
        bool fromLoopBody, Js::Var * registerSaves, Js::InterpreterStackFrame * newInstance, Js::Var* pArgumentsObject, void * argoutRestoreAddress,
        uint regSlot, int offset, bool isLocal, bool isFloat64, bool isInt32, bool isSimd128F4, bool isSimd128I4) const;
    void RestoreInlineFrame(InlinedFrameLayout *inlinedFrame, Js::JavascriptCallStackLayout * layout, Js::Var * registerSaves);

    void AdjustOffsetsForDiagMode(Js::JavascriptCallStackLayout * layout, Js::ScriptFunction * function) const;

    Js::Var EnsureArguments(Js::InterpreterStackFrame * newInstance, Js::JavascriptCallStackLayout * layout, Js::ScriptContext* scriptContext, Js::Var* pArgumentsObject) const;

    Js::JavascriptCallStackLayout *GetStackLayout() const;

    struct StackLiteralBailOutRecord
    {
        Js::RegSlot regSlot;
        uint initFldCount;
    };

    struct ArgOutOffsetInfo
    {
        BVFixed * argOutFloat64Syms;        // Used for float-type-specialized ArgOut symbols. Index = [0 .. BailOutInfo::totalOutParamCount].
        BVFixed * argOutLosslessInt32Syms;  // Used for int-type-specialized ArgOut symbols (which are native int and for bailout we need tagged ints).
        // SIMD_JS
        BVFixed * argOutSimd128F4Syms;
        BVFixed * argOutSimd128I4Syms;
        uint * startCallOutParamCounts;
        int * outParamOffsets;
        uint startCallCount;
        uint argOutSymStart;
    };

    // The offset to 'globalBailOutRecordTable' is hard-coded in LinearScanMD::SaveAllRegisters, so let this be the first member variable
    GlobalBailOutRecordDataTable *globalBailOutRecordTable;
    ArgOutOffsetInfo *argOutOffsetInfo;
    BailOutRecord * parent;
    Js::Var * constants;
    Js::EHBailoutData * ehBailoutData;
    StackLiteralBailOutRecord * stackLiteralBailOutRecord;
#ifdef _M_IX86
    uint * startCallArgRestoreAdjustCounts;
#endif

    // Index of start of section of argOuts for current record/current func, used with argOutFloat64Syms and
    // argOutLosslessInt32Syms when restoring values, as BailOutInfo uses one argOuts array for all funcs.
    // Similar to outParamOffsets which points to current func section for the offsets.
    Js::RegSlot localOffsetsCount;
    uint32 const bailOutOffset;

    uint stackLiteralBailOutRecordCount;


    Js::RegSlot branchValueRegSlot;

    uint polymorphicCacheIndex;
    IR::BailOutKind bailOutKind;
#if DBG
    Js::ArgSlot actualCount;
    uint constantCount;
    int inlineDepth;
    void DumpArgOffsets(uint count, int* offsets, int argOutSlotStart);
    void DumpLocalOffsets(uint count, int argOutSlotStart);
    void DumpValue(int offset, bool isFloat64);
#endif
    ushort bailOutCount;
    uint32 m_bailOutRecordId;

    friend class LinearScan;
    friend class BailOutInfo;
    friend struct FuncBailOutData;
#if ENABLE_DEBUG_CONFIG_OPTIONS
public:
    Js::OpCode bailOutOpcode;
#if DBG
   void Dump();
#endif
#endif
};

class BranchBailOutRecord : public BailOutRecord
{
public:
    BranchBailOutRecord(uint32 trueBailOutOffset, uint32 falseBailOutOffset, Js::RegSlot resultByteCodeReg, IR::BailOutKind kind, Func *bailOutFunc);

    static Js::Var BailOut(BranchBailOutRecord const * bailOutRecord, BOOL cond);
    static Js::Var BailOutFromFunction(Js::JavascriptCallStackLayout * layout, BranchBailOutRecord const * bailOutRecord, BOOL cond, void * returnAddress, void * argoutRestoreAddress);
    static uint32 BailOutFromLoopBody(Js::JavascriptCallStackLayout * layout, BranchBailOutRecord const * bailOutRecord, BOOL cond);

    static Js::Var BailOutInlined(Js::JavascriptCallStackLayout * layout, BranchBailOutRecord const * bailOutRecord, BOOL cond, void * returnAddress);
    static uint32 BailOutFromLoopBodyInlined(Js::JavascriptCallStackLayout * layout, BranchBailOutRecord const * bailOutRecord, BOOL cond, void * returnAddress);
private:
    uint falseBailOutOffset;
};

class FunctionBailOutRecord
{
public:
    FunctionBailOutRecord() : constantCount(0), constants(nullptr) {}

    uint constantCount;
    Js::Var * constants;
};

template <typename Fn>
inline void BailOutRecord::MapStartCallParamCounts(Fn fn)
{
    if (this->argOutOffsetInfo)
    {
        for (uint i = 0; i < this->argOutOffsetInfo->startCallCount; i++)
        {
            fn(this->argOutOffsetInfo->startCallOutParamCounts[i]);
        }
    }
}

template <typename Fn>
inline void BailOutRecord::MapArgOutOffsets(Fn fn)
{
    uint outParamSlot = 0;
    uint argOutSlotOffset = 0;

    if (this->argOutOffsetInfo)
    {
        for (uint i = 0; i < this->argOutOffsetInfo->startCallCount; i++)
        {
            uint startCallOutParamCount = this->argOutOffsetInfo->startCallOutParamCounts[i];
            argOutSlotOffset += 1; // skip pointer to self which is pushed by OP_StartCall

            for (uint j = 0; j < startCallOutParamCount; j++, outParamSlot++, argOutSlotOffset++)
            {
                if (this->argOutOffsetInfo->outParamOffsets[outParamSlot] != 0)
                {
                    fn(argOutSlotOffset, this->argOutOffsetInfo->outParamOffsets[outParamSlot]);
                }
            }
        }
    }
}

struct GlobalBailOutRecordDataRow
{
    int32 offset;
    uint32 start;           // start bailOutId
    uint32 end;             // end bailOutId
    unsigned regSlot : 30;
    unsigned isFloat : 1;
    unsigned isInt : 1;
    // SIMD_JS
    unsigned isSimd128F4 : 1;
    unsigned isSimd128I4 : 1;
};

struct GlobalBailOutRecordDataTable
{
    // The offset to 'registerSaveSpace' is hard-coded in LinearScanMD::SaveAllRegisters, so let this be the first member variable
    Js::Var *registerSaveSpace;
    GlobalBailOutRecordDataRow *globalBailOutRecordDataRows;
    uint32 length;
    uint32 size;
    int32  firstActualStackOffset;
    Js::RegSlot returnValueRegSlot;
    bool isInlinedFunction;
    bool isInlinedConstructor;
    bool isLoopBody;
    void Finalize(NativeCodeData::Allocator *allocator, JitArenaAllocator *tempAlloc);
    void AddOrUpdateRow(JitArenaAllocator *allocator, uint32 bailOutRecordId, uint32 regSlot, bool isFloat, bool isInt, bool isSimd128F4, bool isSimd128I4, int32 offset, uint *lastUpdatedRowIndex);

    template<class Fn>
    void IterateGlobalBailOutRecordTableRows(uint32 bailOutRecordId, Fn callback)
    {
        // Visit all the rows that have this bailout ID in their range.
        for (uint i = 0; i < this->length; i++)
        {
            if (bailOutRecordId > globalBailOutRecordDataRows[i].end)
            {
                // Not in range.
                continue;
            }

            if (globalBailOutRecordDataRows[i].start > bailOutRecordId)
            {
                // Not in range, and we know there are no more in range (since the table is sorted by "start").
                return;
            }

            // In range: take action.
            callback(&globalBailOutRecordDataRows[i]);
        }
    }

    template<class Fn>
    void VisitGlobalBailOutRecordTableRowsAtFirstBailOut(uint32 bailOutRecordId, Fn callback)
    {
        // Visit all the rows that have this bailout ID as the start of their range.
        // (I.e., visit each row once in a walk of the whole function)
        for (uint i = 0; i < this->length; i++)
        {
            if (bailOutRecordId == globalBailOutRecordDataRows[i].start)
            {
                // Matching start ID: take action.
                callback(&globalBailOutRecordDataRows[i]);
            }
            else if (globalBailOutRecordDataRows[i].start > bailOutRecordId)
            {
                // We know there are no more in range (since the table is sorted by "start").
                return;
            }
        }
    }
};
