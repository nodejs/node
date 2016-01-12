//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once

#define ASSERT_INLINEE_FUNC(instr) Assert(instr->m_func->IsInlinee() ? (instr->m_func != this->m_func) : (instr->m_func == this->m_func))

enum IndirScale : BYTE {
    IndirScale1 = 0,
    IndirScale2 = 1,
    IndirScale4 = 2,
    IndirScale8 = 3
};

enum RoundMode : BYTE {
    RoundModeTowardZero = 0,
    RoundModeTowardInteger = 1,
    RoundModeHalfToEven = 2
};

#if defined(_M_IX86) || defined(_M_AMD64)
#include "LowerMDShared.h"
#elif defined(_M_ARM) || defined(_M_ARM64)
#include "LowerMD.h"
#endif

#define IR_HELPER_OP_FULL_OR_INPLACE(op) IR::HelperOp_##op##_Full, IR::HelperOp_##op##InPlace

///---------------------------------------------------------------------------
///
/// class Lowerer
///
///     Lower machine independent IR to machine dependent instrs.
///
///---------------------------------------------------------------------------

class Lowerer
{
    friend class LowererMD;
    friend class LowererMDArch;
    friend class Encoder;
    friend class Func;
    friend class ExternalLowerer;

public:
    Lowerer(Func * func) : m_func(func), m_lowererMD(func), nextStackFunctionOpnd(nullptr), outerMostLoopLabel(nullptr),
        initializedTempSym(nullptr), addToLiveOnBackEdgeSyms(nullptr), currentRegion(nullptr)
    {
    }

    void Lower();
    void LowerRange(IR::Instr *instrStart, IR::Instr *instrEnd, bool defaultDoFastPath, bool defaultDoLoopFastPath);
    void LowerPrologEpilog();
    void LowerPrologEpilogAsmJs();
    void LowerGeneratorResumeJumpTable();

    void DoInterruptProbes();
    IR::Instr *PreLowerPeepInstr(IR::Instr *instr, IR::Instr **pInstrPrev);
    IR::Instr *PeepShl(IR::Instr *instr);
    IR::Instr *PeepBrBool(IR::Instr *instr);
    uint DoLoopProbeAndNumber(IR::BranchInstr *branchInstr);
    void InsertOneLoopProbe(IR::Instr *insertInstr, IR::LabelInstr *loopLabel);
    void FinalLower();
    void EHBailoutPatchUp();
    inline Js::ScriptContext* GetScriptContext()
    {
        return m_func->GetScriptContext();
    }

    StackSym *      GetTempNumberSym(IR::Opnd * opnd, bool isTempTransferred);
    static bool     HasSideEffects(IR::Instr *instr);

#if DBG
    static bool     ValidOpcodeAfterLower(IR::Instr* instr, Func * func);
#endif
private:
    IR::Instr *     LowerNewRegEx(IR::Instr * instr);
    void            LowerNewScObjectSimple(IR::Instr *instr);
    void            LowerNewScObjectLiteral(IR::Instr *instr);
    IR::Instr *     LowerInitCachedFuncs(IR::Instr *instrInit);

    IR::Instr *     LowerNewScObject(IR::Instr *instr, bool callCtor, bool hasArgs, bool isBaseClassConstructorNewScObject = false);
    IR::Instr *     LowerNewScObjArray(IR::Instr *instr);
    IR::Instr *     LowerNewScObjArrayNoArg(IR::Instr *instr);
    bool            TryLowerNewScObjectWithFixedCtorCache(IR::Instr* newObjInstr, IR::RegOpnd* newObjDst, IR::LabelInstr* helperOrBailoutLabel, IR::LabelInstr* callCtorLabel,
                        bool& skipNewScObj, bool& returnNewScObj, bool& emitHelper);
    void            GenerateRecyclerAllocAligned(IR::JnHelperMethod allocHelper, size_t allocSize, IR::RegOpnd* newObjDst, IR::Instr* insertionPointInstr, bool inOpHelper = false);
    IR::Instr *     LowerGetNewScObject(IR::Instr *const instr);
    void            LowerGetNewScObjectCommon(IR::RegOpnd *const resultObjOpnd, IR::RegOpnd *const constructorReturnOpnd, IR::RegOpnd *const newObjOpnd, IR::Instr *insertBeforeInstr);
    IR::Instr *     LowerUpdateNewScObjectCache(IR::Instr * updateInstr, IR::Opnd *dst, IR::Opnd *src1, const bool isCtorFunction);
    bool            GenerateLdFldWithCachedType(IR::Instr * instrLdFld, bool* continueAsHelperOut, IR::LabelInstr** labelHelperOut, IR::RegOpnd** typeOpndOut);
    bool            GenerateCheckFixedFld(IR::Instr * instrChkFld);
    void            GenerateCheckObjType(IR::Instr * instrChkObjType);
    void            LowerAdjustObjType(IR::Instr * instrAdjustObjType);
    bool            GenerateNonConfigurableLdRootFld(IR::Instr * instrLdFld);
    IR::Instr *     LowerProfiledLdFld(IR::JitProfilingInstr *instr);
    void            LowerProfiledBeginSwitch(IR::JitProfilingInstr *instr);
    void            LowerFunctionExit(IR::Instr* funcExit);
    void            LowerFunctionEntry(IR::Instr* funcEntry);
    void            GenerateNullOutGeneratorFrame(IR::Instr* instrInsert);
    void            LowerFunctionBodyCallCountChange(IR::Instr *const insertBeforeInstr);
    IR::Instr*      LowerProfiledNewArray(IR::JitProfilingInstr* instr, bool hasArgs);
    IR::Instr *     LowerProfiledLdSlot(IR::JitProfilingInstr *instr);
    void            LowerProfileLdSlot(IR::Opnd *const valueOpnd, Func *const ldSlotFunc, const Js::ProfileId profileId, IR::Instr *const insertBeforeInstr);
    void            LowerProfiledBinaryOp(IR::JitProfilingInstr* instr, IR::JnHelperMethod meth);
    IR::Instr *     LowerLdFld(IR::Instr *instr, IR::JnHelperMethod helperMethod, IR::JnHelperMethod polymorphicHelperMethod, bool useInlineCache, IR::LabelInstr *labelBailOut = nullptr, bool isHelper = false);
    template<bool isRoot>
    IR::Instr*      GenerateCompleteLdFld(IR::Instr* instr, bool emitFastPath, IR::JnHelperMethod monoHelperAfterFastPath, IR::JnHelperMethod polyHelperAfterFastPath,
                        IR::JnHelperMethod monoHelperWithoutFastPath, IR::JnHelperMethod polyHelperWithoutFastPath);
    IR::Instr *     LowerScopedLdFld(IR::Instr *instr, IR::JnHelperMethod helperMethod, bool withInlineCache);
    IR::Instr *     LowerScopedLdInst(IR::Instr *instr, IR::JnHelperMethod helperMethod);
    IR::Instr *     LowerDelFld(IR::Instr *instr, IR::JnHelperMethod helperMethod, bool useInlineCache, bool strictMode);
    IR::Instr *     LowerIsInst(IR::Instr *instr, IR::JnHelperMethod helperMethod);
    IR::Instr *     LowerScopedDelFld(IR::Instr *instr, IR::JnHelperMethod helperMethod, bool withInlineCache, bool strictMode);
    IR::Instr *     LowerNewScFunc(IR::Instr *instr);
    IR::Instr *     LowerNewScGenFunc(IR::Instr *instr);
    IR::Instr*      GenerateCompleteStFld(IR::Instr* instr, bool emitFastPath, IR::JnHelperMethod monoHelperAfterFastPath, IR::JnHelperMethod polyHelperAfterFastPath,
                        IR::JnHelperMethod monoHelperWithoutFastPath, IR::JnHelperMethod polyHelperWithoutFastPath, bool withPutFlags, Js::PropertyOperationFlags flags);
    bool            GenerateStFldWithCachedType(IR::Instr * instrStFld, bool* continueAsHelperOut, IR::LabelInstr** labelHelperOut, IR::RegOpnd** typeOpndOut);
    bool            GenerateStFldWithCachedFinalType(IR::Instr * instrStFld, IR::PropertySymOpnd *propertySymOpnd);
    IR::RegOpnd *   GenerateCachedTypeCheck(IR::Instr *instrInsert, IR::PropertySymOpnd *propertySymOpnd,
                        IR::LabelInstr* labelObjCheckFailed, IR::LabelInstr *labelTypeCheckFailed, IR::LabelInstr *labelSecondChance = nullptr);
    void            GenerateCachedTypeWithoutPropertyCheck(IR::Instr *instrInsert, IR::PropertySymOpnd *propertySymOpnd, IR::Opnd *typeOpnd, IR::LabelInstr *labelTypeCheckFailed);
    void            GenerateFixedFieldGuardCheck(IR::Instr *insertPointInstr, IR::PropertySymOpnd *propertySymOpnd, IR::LabelInstr *labelBailOut);
    Js::JitTypePropertyGuard* CreateTypePropertyGuardForGuardedProperties(Js::Type* type, IR::PropertySymOpnd* propertySymOpnd);
    Js::JitEquivalentTypeGuard* CreateEquivalentTypeGuardAndLinkToGuardedProperties(Js::Type* type, IR::PropertySymOpnd* propertySymOpnd);
    bool            LinkCtorCacheToGuardedProperties(Js::JitTimeConstructorCache* cache);
    template<typename LinkFunc>
    bool            LinkGuardToGuardedProperties(Js::EntryPointInfo* entryPointInfo, const BVSparse<JitArenaAllocator>* guardedPropOps, LinkFunc link);
    void            GeneratePropertyGuardCheck(IR::Instr *insertPointInstr, IR::PropertySymOpnd *propertySymOpnd, IR::LabelInstr *labelBailOut);
    IR::Instr *     GeneratePropertyGuardCheckBailoutAndLoadType(IR::Instr *insertInstr);
    void            GenerateNonWritablePropertyCheck(IR::Instr *instrInsert, IR::PropertySymOpnd *propertySymOpnd, IR::LabelInstr *labelBailOut);
    void            GenerateFieldStoreWithTypeChange(IR::Instr * instrStFld, IR::PropertySymOpnd *propertySymOpnd, Js::Type* initialType, Js::Type* finalType);
    void            GenerateDirectFieldStore(IR::Instr* instrStFld, IR::PropertySymOpnd* propertySymOpnd);
    void            GenerateAdjustSlots(IR::Instr * instrStFld, IR::PropertySymOpnd *propertySymOpnd, Js::Type* initialType, Js::Type* finalType);
    bool            GenerateAdjustBaseSlots(IR::Instr * instrStFld, IR::RegOpnd *baseOpnd, Js::Type* initialType, Js::Type* finalType);
    void            GeneratePrototypeCacheInvalidateCheck(IR::PropertySymOpnd *propertySymOpnd, IR::Instr *instrStFld);
    void            PinTypeRef(Js::Type* type, void* typeRef, IR::Instr* instr, Js::PropertyId propertyId);
    IR::RegOpnd *   GenerateIsBuiltinRecyclableObject(IR::RegOpnd *regOpnd, IR::Instr *insertInstr, IR::LabelInstr *labelHelper, bool checkObjectAndDynamicObject = true, IR::LabelInstr *labelFastExternal = nullptr);

    void            EnsureStackFunctionListStackSym();
    void            EnsureZeroLastStackFunctionNext();
    void            AllocStackClosure();
    IR::Instr *     GenerateNewStackScFunc(IR::Instr * newScFuncInstr);
    void            GenerateStackScriptFunctionInit(StackSym * stackSym, Js::FunctionProxyPtrPtr nestedProxy);
    void            GenerateScriptFunctionInit(IR::RegOpnd * regOpnd, IR::Opnd * vtableAddressOpnd,
                        Js::FunctionProxyPtrPtr nestedProxy, IR::Opnd * envOpnd, IR::Instr * insertBeforeInstr, bool isZeroed = false);
    void            GenerateStackScriptFunctionInit(IR::RegOpnd * regOpnd, Js::FunctionProxyPtrPtr nestedProxy, IR::Opnd * envOpnd, IR::Instr * insertBeforeInstr);
    IR::Instr *     LowerProfiledStFld(IR::JitProfilingInstr * instr, Js::PropertyOperationFlags flags);
    IR::Instr *     LowerStFld(IR::Instr * stFldInstr, IR::JnHelperMethod helperMethod, IR::JnHelperMethod polymorphicHelperMethod, bool withInlineCache, IR::LabelInstr *ppBailOutLabel = nullptr, bool isHelper = false, bool withPutFlags = false, Js::PropertyOperationFlags flags = Js::PropertyOperation_None);
    IR::Instr *     LowerScopedStFld(IR::Instr * stFldInstr, IR::JnHelperMethod helperMethod, bool withInlineCache,
                                bool withPropertyOperationFlags = false, Js::PropertyOperationFlags flags = Js::PropertyOperation_None);
    void            LowerProfiledLdElemI(IR::JitProfilingInstr *const instr);
    void            LowerProfiledStElemI(IR::JitProfilingInstr *const instr, const Js::PropertyOperationFlags flags);
    IR::Instr *     LowerStElemI(IR::Instr *instr, Js::PropertyOperationFlags flags, bool isHelper, IR::JnHelperMethod helperMethod = IR::HelperOp_SetElementI);
    IR::Instr *     LowerLdElemI(IR::Instr *instr, IR::JnHelperMethod helperMethod, bool isHelper);
    void            LowerLdLen(IR::Instr *const instr, const bool isHelper);

    IR::Instr *     LowerMemOp(IR::Instr * instr);
    void            LowerMemset(IR::Instr * instr, IR::RegOpnd * helperRet);
    void            LowerMemcopy(IR::Instr * instr, IR::RegOpnd * helperRet);

    IR::Instr *     LowerLdArrViewElem(IR::Instr * instr);
    IR::Instr *     LowerStArrViewElem(IR::Instr * instr);
    IR::Instr *     LowerArrayDetachedCheck(IR::Instr * instr);
    IR::Instr *     LowerDeleteElemI(IR::Instr *instr, bool strictMode);
    IR::Instr *     LowerStElemC(IR::Instr *instr);
    void            LowerLdArrHead(IR::Instr *instr);
    IR::Instr *     LowerStSlot(IR::Instr *instr);
    IR::Instr *     LowerStSlotChkUndecl(IR::Instr *instr);
    void            LowerStLoopBodyCount(IR::Instr* instr);
#if !FLOATVAR
    IR::Instr *     LowerStSlotBoxTemp(IR::Instr *instr);
#endif
    IR::Instr *     LowerLdSlot(IR::Instr *instr);
    IR::Instr *     LowerChkUndecl(IR::Instr *instr);
    void            GenUndeclChk(IR::Instr *insertInsert, IR::Opnd *opnd);
    IR::Instr *     LowerStLen(IR::Instr *instr);
    IR::Instr *     LoadPropertySymAsArgument(IR::Instr *instr, IR::Opnd *fieldSrc);
    IR::Instr *     LoadFunctionBodyAsArgument(IR::Instr *instr, IR::IntConstOpnd * functionBodySlotOpnd, IR::RegOpnd * envOpnd);
    IR::Instr *     LoadHelperTemp(IR::Instr * instr, IR::Instr * instrInsert);
    IR::Instr *     LowerLoadVar(IR::Instr *instr, IR::Opnd *opnd);
    void            LoadArgumentCount(IR::Instr *const instr);
    void            LoadStackArgPtr(IR::Instr *const instr);
    void            LoadArgumentsFromFrame(IR::Instr *const instr);
    IR::Instr *     LowerUnaryHelper(IR::Instr *instr, IR::JnHelperMethod helperMethod);
    IR::Instr *     LowerUnaryHelperMem(IR::Instr *instr, IR::JnHelperMethod helperMethod);
    IR::Instr *     LowerUnaryHelperMemWithFuncBody(IR::Instr *instr, IR::JnHelperMethod helperMethod);
    IR::Instr *     LowerBinaryHelperMemWithFuncBody(IR::Instr *instr, IR::JnHelperMethod helperMethod);
    IR::Instr *     LowerUnaryHelperMemWithTemp(IR::Instr *instr, IR::JnHelperMethod helperMethod);
    IR::Instr *     LowerUnaryHelperMemWithTemp2(IR::Instr *instr, IR::JnHelperMethod helperMethod, IR::JnHelperMethod helperMethodWithTemp);
    IR::Instr *     LowerBinaryHelperMem(IR::Instr *instr, IR::JnHelperMethod helperMethod);
    IR::Instr *     LowerBinaryHelperMemWithTemp(IR::Instr *instr, IR::JnHelperMethod helperMethod);
    IR::Instr *     LowerBinaryHelperMemWithTemp2(IR::Instr *instr, IR::JnHelperMethod helperMethod, IR::JnHelperMethod helperMethodWithTemp);
    IR::Instr *     LowerBinaryHelperMemWithTemp3(IR::Instr *instr, IR::JnHelperMethod helperMethod,
        IR::JnHelperMethod helperMethodWithTemp, IR::JnHelperMethod helperMethodLeftDead);
    IR::Instr *     LowerAddLeftDeadForString(IR::Instr *instr);
    IR::Instr *     LowerBinaryHelper(IR::Instr *instr, IR::JnHelperMethod helperMethod);
    IR::Instr *     LowerBrBReturn(IR::Instr * instr, IR::JnHelperMethod helperMethod, bool isHelper);
    IR::Instr *     LowerBrBMem(IR::Instr *instr, IR::JnHelperMethod helperMethod);
    IR::Instr *     LowerBrOnObject(IR::Instr *instr, IR::JnHelperMethod helperMethod);
    IR::Instr *     LowerBrCMem(IR::Instr * instr, IR::JnHelperMethod helperMethod, bool noMathFastPath, bool isHelper = true);
    IR::Instr *     LowerBrFncApply(IR::Instr * instr, IR::JnHelperMethod helperMethod);
    IR::Instr *     LowerBrProperty(IR::Instr * instr, IR::JnHelperMethod helperMethod);
    IR::Instr *     LowerBrOnClassConstructor(IR::Instr *instr, IR::JnHelperMethod helperMethod);
    IR::Instr*      LowerMultiBr(IR::Instr * instr, IR::JnHelperMethod helperMethod);
    IR::Instr*      LowerMultiBr(IR::Instr * instr);
    IR::Instr *     LowerElementUndefined(IR::Instr * instr, IR::JnHelperMethod helper);
    IR::Instr *     LowerElementUndefinedMem(IR::Instr * instr, IR::JnHelperMethod helper);
    IR::Instr *     LowerElementUndefinedScoped(IR::Instr * instr, IR::JnHelperMethod helper);
    IR::Instr *     LowerElementUndefinedScopedMem(IR::Instr * instr, IR::JnHelperMethod helper);
    IR::Instr *     LowerLdElemUndef(IR::Instr * instr);
    IR::Instr *     LowerRestParameter(IR::Opnd *formalsOpnd, IR::Opnd *dstOpnd, IR::Opnd *excessOpnd, IR::Instr *instr, IR::RegOpnd *generatorArgsPtrOpnd);
    IR::Instr *     LowerArgIn(IR::Instr *instr);
    IR::Instr *     LowerArgInAsmJs(IR::Instr *instr);
    IR::Instr *     LowerProfiledNewScArray(IR::JitProfilingInstr* arrInstr);
    IR::Instr *     LowerNewScArray(IR::Instr *arrInstr);
    IR::Instr *     LowerNewScIntArray(IR::Instr *arrInstr);
    IR::Instr *     LowerNewScFltArray(IR::Instr *arrInstr);
    IR::Instr *     LowerArraySegmentVars(IR::Instr *instr);
    template <typename ArrayType>
    BOOL            IsSmallObject(uint32 length);
#ifdef ENABLE_DOM_FAST_PATH
    void            LowerFastInlineDOMFastPathGetter(IR::Instr* getterInstr);
#endif
    void            GenerateProfiledNewScIntArrayFastPath(IR::Instr *instr, Js::ArrayCallSiteInfo * arrayInfo, RecyclerWeakReference<Js::FunctionBody> * weakFuncRef);
    void            GenerateArrayInfoIsNativeIntArrayTest(IR::Instr * instr,  Js::ArrayCallSiteInfo * arrayInfo, IR::LabelInstr * helperLabel);
    void            GenerateProfiledNewScFloatArrayFastPath(IR::Instr *instr, Js::ArrayCallSiteInfo * arrayInfo, RecyclerWeakReference<Js::FunctionBody> * weakFuncRef);
    void            GenerateArrayInfoIsNativeFloatAndNotIntArrayTest(IR::Instr * instr,  Js::ArrayCallSiteInfo * arrayInfo, IR::LabelInstr * helperLabel);
    bool            IsEmitTempDst(IR::Opnd *opnd);
    bool            IsEmitTempSrc(IR::Opnd *opnd);
    bool            IsNullOrUndefRegOpnd(IR::RegOpnd *opnd) const;
    bool            IsConstRegOpnd(IR::RegOpnd *opnd) const;
    IR::Instr *     GenerateRuntimeError(IR::Instr * insertBeforeInstr, Js::MessageId errorCode, IR::JnHelperMethod helper = IR::JnHelperMethod::HelperOp_RuntimeTypeError);
    bool            InlineBuiltInLibraryCall(IR::Instr *callInstr);
    void            LowerInlineBuiltIn(IR::Instr* instr);
    Js::JavascriptFunction** GetObjRefForBuiltInTarget(IR::RegOpnd * opnd);
    bool            TryGenerateFastCmSrEq(IR::Instr * instr);
    bool            TryGenerateFastBrEq(IR::Instr * instr);
    bool            TryGenerateFastBrNeq(IR::Instr * instr);
    bool            GenerateFastBrSrEq(IR::Instr * instr, IR::RegOpnd * srcReg1, IR::RegOpnd * srcReg2, IR::Instr ** pInstrPrev, bool noMathFastPath);
    bool            GenerateFastBrSrNeq(IR::Instr * instr, IR::Instr ** pInstrPrev);
    IR::BranchInstr* GenerateFastBrConst(IR::BranchInstr *branchInstr, IR::Opnd * constOpnd, bool isEqual);
    bool            GenerateFastCondBranch(IR::BranchInstr * instrBranch, bool *pIsHelper);
    bool            GenerateFastBrEqLikely(IR::BranchInstr * instrBranch, bool *pNeedHelper);
    bool            GenerateFastBrBool(IR::BranchInstr *const instr);
    static IR::Instr *LoadFloatFromNonReg(IR::Opnd * opndOrig, IR::Opnd * regOpnd, IR::Instr * instrInsert);
    void            LoadInt32FromUntaggedVar(IR::Instr *const instrLoad);
    bool            GetValueFromIndirOpnd(IR::IndirOpnd *indirOpnd, IR::Opnd **pValueOpnd, IntConstType *pValue);
    void            GenerateFastBrOnObject(IR::Instr *instr);

    void            GenerateObjectTypeTest(IR::RegOpnd *srcReg, IR::Instr *instrInsert, IR::LabelInstr *labelHelper);
    static          IR::LabelInstr* InsertContinueAfterExceptionLabelForDebugger(Func* func, IR::Instr* insertAfterInstr, bool isHelper);
    void            GenerateObjectHeaderInliningTest(IR::RegOpnd *baseOpnd, IR::LabelInstr * target, IR::Instr *insertBeforeInstr);

// Static tables that will be used by the GetArray* methods below
private:
    static const VTableValue VtableAddresses[static_cast<ValueType::TSize>(ObjectType::Count)];
    static const uint32 OffsetsOfHeadSegment[static_cast<ValueType::TSize>(ObjectType::Count)];
    static const uint32 OffsetsOfLength[static_cast<ValueType::TSize>(ObjectType::Count)];
    static const IRType IndirTypes[static_cast<ValueType::TSize>(ObjectType::Count)];
    static const BYTE IndirScales[static_cast<ValueType::TSize>(ObjectType::Count)];

private:
    static VTableValue GetArrayVtableAddress(const ValueType valueType, bool getVirtual = false);

public:
    static uint32   GetArrayOffsetOfHeadSegment(const ValueType valueType);
    static uint32   GetArrayOffsetOfLength(const ValueType valueType);
    static IRType   GetArrayIndirType(const ValueType valueType);

private:
    BYTE            GetArrayIndirScale(const ValueType valueType) const;

    bool            ShouldGenerateArrayFastPath(const IR::Opnd *const arrayOpnd, const bool supportsObjectsWithArrays, const bool supportsTypedArrays, const bool requiresSse2ForFloatArrays) const;
    IR::RegOpnd *   LoadObjectArray(IR::RegOpnd *const baseOpnd, IR::Instr *const insertBeforeInstr);
    IR::RegOpnd *   GenerateArrayTest(IR::RegOpnd *const baseOpnd, IR::LabelInstr *const isNotObjectLabel, IR::LabelInstr *const isNotArrayLabel, IR::Instr *const insertBeforeInstr, const bool forceFloat, const bool isStore = false, const bool allowDefiniteArray = false);
    void            GenerateIsEnabledArraySetElementFastPathCheck(IR::LabelInstr * isDisabledLabel, IR::Instr * const insertBeforeInstr);
    void            GenerateIsEnabledIntArraySetElementFastPathCheck(IR::LabelInstr * isDisabledLabel, IR::Instr * const insertBeforeInstr);
    void            GenerateIsEnabledFloatArraySetElementFastPathCheck(IR::LabelInstr * isDisabledLabel, IR::Instr * const insertBeforeInstr);
    void            GenerateTypeIdCheck(Js::TypeId typeId, IR::RegOpnd * opnd, IR::LabelInstr * labelFail, IR::Instr * insertBeforeInstr, bool generateObjectCheck = true);
    void            GenerateStringTest(IR::RegOpnd *srcReg, IR::Instr *instrInsert, IR::LabelInstr * failLabel, IR::LabelInstr * succeedLabel = nullptr, bool generateObjectCheck = true);
    IR::RegOpnd *   GenerateUntagVar(IR::RegOpnd * opnd, IR::LabelInstr * labelFail, IR::Instr * insertBeforeInstr, bool generateTagCheck = true);
    void            GenerateNotZeroTest( IR::Opnd * opndSrc, IR::LabelInstr * labelZero, IR::Instr * instrInsert);
    IR::Opnd *      CreateOpndForSlotAccess(IR::Opnd * opnd);

    void            GenerateSwitchStringLookup(IR::Instr * instr);
    void            GenerateSingleCharStrJumpTableLookup(IR::Instr * instr);
    void            LowerJumpTableMultiBranch(IR::MultiBranchInstr * multiBrInstr, IR::RegOpnd * indexOpnd);

    void            LowerConvNum(IR::Instr *instrLoad, bool noMathFastPath);

    void            InsertBitTestBranch(IR::Opnd * bitMaskOpnd, IR::Opnd * bitIndex, bool jumpIfBitOn, IR::LabelInstr * targetLabel, IR::Instr * insertBeforeInstr);
    void            GenerateGetSingleCharString(IR::RegOpnd * charCodeOpnd, IR::Opnd * resultOpnd, IR::LabelInstr * labelHelper, IR::LabelInstr * doneLabel, IR::Instr * instr, bool isCodePoint);
public:
    static IR::LabelInstr *     InsertLabel(const bool isHelper, IR::Instr *const insertBeforeInstr);

    static IR::Instr *          InsertMove(IR::Opnd *dst, IR::Opnd *src, IR::Instr *const insertBeforeInstr, bool generateWriteBarrier = false);
    static IR::Instr *          InsertMoveWithBarrier(IR::Opnd *dst, IR::Opnd *src, IR::Instr *const insertBeforeInstr);
    static IR::BranchInstr *    InsertBranch(const Js::OpCode opCode, IR::LabelInstr *const target, IR::Instr *const insertBeforeInstr);
    static IR::BranchInstr *    InsertBranch(const Js::OpCode opCode, const bool isUnsigned, IR::LabelInstr *const target, IR::Instr *const insertBeforeInstr);
    static IR::Instr *          InsertCompare(IR::Opnd *const src1, IR::Opnd *const src2, IR::Instr *const insertBeforeInstr);
    static IR::BranchInstr *    InsertCompareBranch(IR::Opnd *const compareSrc1, IR::Opnd *const compareSrc2, Js::OpCode branchOpCode, IR::LabelInstr *const target, IR::Instr *const insertBeforeInstr, const bool ignoreNaN = false);
    static IR::BranchInstr *    InsertCompareBranch(IR::Opnd *compareSrc1, IR::Opnd *compareSrc2, Js::OpCode branchOpCode, const bool isUnsigned, IR::LabelInstr *const target, IR::Instr *const insertBeforeInstr, const bool ignoreNaN = false);
    static IR::Instr *          InsertTest(IR::Opnd *const src1, IR::Opnd *const src2, IR::Instr *const insertBeforeInstr);
    static IR::BranchInstr *    InsertTestBranch(IR::Opnd *const testSrc1, IR::Opnd *const testSrc2, const Js::OpCode branchOpCode, IR::LabelInstr *const target, IR::Instr *const insertBeforeInstr);
    static IR::BranchInstr *    InsertTestBranch(IR::Opnd *const testSrc1, IR::Opnd *const testSrc2, const Js::OpCode branchOpCode, const bool isUnsigned, IR::LabelInstr *const target, IR::Instr *const insertBeforeInstr);
    static IR::Instr *          InsertAdd(const bool needFlags, IR::Opnd *const dst, IR::Opnd *src1, IR::Opnd *src2, IR::Instr *const insertBeforeInstr);
    static IR::Instr *          InsertSub(const bool needFlags, IR::Opnd *const dst, IR::Opnd *src1, IR::Opnd *src2, IR::Instr *const insertBeforeInstr);
    static IR::Instr *          InsertLea(IR::RegOpnd *const dst, IR::Opnd *const src, IR::Instr *const insertBeforeInstr);
    static IR::Instr *          InsertAnd(IR::Opnd *const dst, IR::Opnd *const src1, IR::Opnd *const src2, IR::Instr *const insertBeforeInstr);
    static IR::Instr *          InsertOr(IR::Opnd *const dst, IR::Opnd *const src1, IR::Opnd *const src2, IR::Instr *const insertBeforeInstr);
    static IR::Instr *          InsertShift(const Js::OpCode opCode, const bool needFlags, IR::Opnd *const dst, IR::Opnd *const src1, IR::Opnd *const src2, IR::Instr *const insertBeforeInstr);
    static IR::Instr *          InsertShiftBranch(const Js::OpCode shiftOpCode, IR::Opnd *const dst, IR::Opnd *const src1, IR::Opnd *const src2, const Js::OpCode branchOpCode, IR::LabelInstr *const target, IR::Instr *const insertBeforeInstr);
    static IR::Instr *          InsertShiftBranch(const Js::OpCode shiftOpCode, IR::Opnd *const dst, IR::Opnd *const src1, IR::Opnd *const src2, const Js::OpCode branchOpCode, const bool isUnsigned, IR::LabelInstr *const target, IR::Instr *const insertBeforeInstr);
    static IR::Instr *          InsertConvertFloat32ToFloat64(IR::Opnd *const dst, IR::Opnd *const src, IR::Instr *const insertBeforeInstr);
    static IR::Instr *          InsertConvertFloat64ToFloat32(IR::Opnd *const dst, IR::Opnd *const src, IR::Instr *const insertBeforeInstr);

public:
    static void InsertIncUInt8PreventOverflow(IR::Opnd *const dst, IR::Opnd *const src, IR::Instr *const insertBeforeInstr, IR::Instr * *const onOverflowInsertBeforeInstrRef = nullptr);
    static void InsertDecUInt8PreventOverflow(IR::Opnd *const dst, IR::Opnd *const src, IR::Instr *const insertBeforeInstr, IR::Instr * *const onOverflowInsertBeforeInstrRef = nullptr);
    void InsertFloatCheckForZeroOrNanBranch(IR::Opnd *const src, const bool branchOnZeroOrNan, IR::LabelInstr *const target, IR::LabelInstr *const fallthroughLabel, IR::Instr *const insertBeforeInstr);

public:
    static IR::HelperCallOpnd*  CreateHelperCallOpnd(IR::JnHelperMethod helperMethod, int helperArgCount, Func* func);
    static IR::Opnd *           GetMissingItemOpnd(IRType type, Func *func);
    static IR::Opnd *           GetImplicitCallFlagsOpnd(Func * func);
    inline static IR::IntConstOpnd* MakeCallInfoConst(ushort flags, int32 argCount, Func* func) {
#ifdef _M_X64
        // This was defined differently for x64
        Js::CallInfo callInfo = Js::CallInfo((Js::CallFlags)flags, (unsigned __int16)argCount);
        return IR::IntConstOpnd::New(*((IntConstType *)((void *)&callInfo)), TyInt32, func, true);
#else
        AssertMsg(!(argCount & 0xFF000000), "Too many arguments"); //final 8 bits are for flags
        AssertMsg(!(flags & ~0xFF), "Flags are invalid!"); //8 bits for flags
        return IR::IntConstOpnd::New(argCount | (flags << 24), TyMachReg, func, true);
#endif
    }

private:
    IR::IndirOpnd * GenerateFastElemICommon(IR::Instr * ldElem, bool isStore, IR::IndirOpnd * indirOpnd, IR::LabelInstr * labelHelper, IR::LabelInstr * labelCantUseArray, IR::LabelInstr *labelFallthrough, bool * pIsTypedArrayElement, bool * pIsStringIndex, bool *emitBailoutRef, IR::LabelInstr **pLabelSegmentLengthIncreased = nullptr, bool checkArrayLengthOverflow = true, bool forceGenerateFastPath = false, bool returnLength = false, IR::LabelInstr *bailOutLabelInstr = nullptr);
    IR::IndirOpnd * GenerateFastElemIIntIndexCommon(IR::Instr * ldElem, bool isStore, IR::IndirOpnd * indirOpnd, IR::LabelInstr * labelHelper, IR::LabelInstr * labelCantUseArray, IR::LabelInstr *labelFallthrough, bool * pIsTypedArrayElement, bool *emitBailoutRef, IR::LabelInstr **pLabelSegmentLengthIncreased, bool checkArrayLengthOverflow, bool forceGenerateFastPath = false, bool returnLength = false, IR::LabelInstr *bailOutLabelInstr = nullptr);
    bool            GenerateFastLdElemI(IR::Instr *& ldElem, bool *instrIsInHelperBlockRef);
    bool            GenerateFastStElemI(IR::Instr *& StElem, bool *instrIsInHelperBlockRef);
    bool            GenerateFastLdLen(IR::Instr *ldLen, bool *instrIsInHelperBlockRef);
    bool            GenerateFastInlineGlobalObjectParseInt(IR::Instr *instr);
    bool            GenerateFastInlineStringFromCharCode(IR::Instr* instr);
    bool            GenerateFastInlineStringFromCodePoint(IR::Instr* instr);
    void            GenerateFastInlineStringCodePointAt(IR::Instr* doneLabel, Func* func, IR::Opnd *strLength, IR::Opnd *srcIndex, IR::RegOpnd *lowerChar, IR::RegOpnd *strPtr);
    bool            GenerateFastInlineStringCharCodeAt(IR::Instr* instr, Js::BuiltinFunction index);
    bool            GenerateFastInlineStringReplace(IR::Instr* instr);
    void            GenerateFastInlineArrayPush(IR::Instr * instr);
    void            GenerateFastInlineArrayPop(IR::Instr * instr);
    void            GenerateFastInlineStringSplitMatch(IR::Instr * instr);
    void            GenerateFastInlineMathImul(IR::Instr* instr);
    void            GenerateFastInlineMathClz32(IR::Instr* instr);
    void            GenerateFastInlineMathFround(IR::Instr* instr);
    void            GenerateFastInlineRegExpExec(IR::Instr * instr);
    bool            GenerateFastPush(IR::Opnd *baseOpndParam, IR::Opnd *src, IR::Instr *callInstr, IR::Instr *insertInstr, IR::LabelInstr *labelHelper, IR::LabelInstr *doneLabel, IR::LabelInstr * bailOutLabelHelper, bool returnLength = false);
    bool            GenerateFastReplace(IR::Opnd* strOpnd, IR::Opnd* src1, IR::Opnd* src2, IR::Instr *callInstr, IR::Instr *insertInstr, IR::LabelInstr *labelHelper, IR::LabelInstr *doneLabel);
    bool            ShouldGenerateStringReplaceFastPath(IR::Instr * instr, IntConstType argCount);
    bool            GenerateFastPop(IR::Opnd *baseOpndParam, IR::Instr *callInstr, IR::LabelInstr *labelHelper, IR::LabelInstr *doneLabel, IR::LabelInstr * bailOutLabelHelper);
    bool            GenerateFastStringLdElem(IR::Instr * ldElem, IR::LabelInstr * labelHelper, IR::LabelInstr * labelFallThru);
    IR::Instr *     LowerCallDirect(IR::Instr * instr);
    IR::Instr *     GenerateDirectCall(IR::Instr* inlineInstr, IR::Opnd* funcObj, ushort callflags);
    IR::Instr *     GenerateFastInlineBuiltInMathRandom(IR::Instr* instr);
    IR::Instr *     GenerateHelperToArrayPushFastPath(IR::Instr * instr, IR::LabelInstr * bailOutLabelHelper);
    IR::Instr *     GenerateHelperToArrayPopFastPath(IR::Instr * instr, IR::LabelInstr * doneLabel, IR::LabelInstr * bailOutLabelHelper);
    IR::Instr *     LowerCondBranchCheckBailOut(IR::BranchInstr * instr, IR::Instr * helperCall, bool isHelper);
    IR::Instr *     LowerBailOnEqualOrNotEqual(IR::Instr * instr, IR::BranchInstr * branchInstr = nullptr, IR::LabelInstr * labelBailOut = nullptr, IR::PropertySymOpnd * propSymOpnd = nullptr, bool isHelper = false);
    void            LowerBailOnNegative(IR::Instr *const instr);
    void            LowerBailoutCheckAndLabel(IR::Instr *instr, bool onEqual, bool isHelper);
    IR::Instr *     LowerBailOnNotSpreadable(IR::Instr *instr);
    IR::Instr *     LowerBailOnNotPolymorphicInlinee(IR::Instr * instr);
    IR::Instr *     LowerBailOnNotStackArgs(IR::Instr * instr);
    IR::Instr *     LowerBailOnNotObject(IR::Instr *instr, IR::BranchInstr *branchInstr = nullptr, IR::LabelInstr *labelBailOut = nullptr);
    IR::Instr *     LowerBailOnNotBuiltIn(IR::Instr *instr, IR::BranchInstr *branchInstr = nullptr, IR::LabelInstr *labelBailOut = nullptr);
    IR::Instr *     LowerBailOnNotInteger(IR::Instr *instr, IR::BranchInstr *branchInstr = nullptr, IR::LabelInstr *labelBailOut = nullptr);
    IR::Instr *     LowerBailOnIntMin(IR::Instr *instr, IR::BranchInstr *branchInstr = nullptr, IR::LabelInstr *labelBailOut = nullptr);
    void            LowerBailOnNotString(IR::Instr *instr);
    IR::Instr *     LowerBailForDebugger(IR::Instr* instr, bool isInsideHelper = false);
    IR::Instr *     LowerBailOnException(IR::Instr* instr);

    void            LowerOneBailOutKind(IR::Instr *const instr, const IR::BailOutKind bailOutKindToLower, const bool isInHelperBlock, const bool preserveBailOutKindInInstr = false);

    void            SplitBailOnNotArray(IR::Instr *const instr, IR::Instr * *const bailOnNotArrayRef, IR::Instr * *const bailOnMissingValueRef);
    IR::RegOpnd *   LowerBailOnNotArray(IR::Instr *const instr);
    void            LowerBailOnMissingValue(IR::Instr *const instr, IR::RegOpnd *const arrayOpnd);
    void            LowerBailOnInvalidatedArrayHeadSegment(IR::Instr *const instr, const bool isInHelperBlock);
    void            LowerBailOnInvalidatedArrayLength(IR::Instr *const instr, const bool isInHelperBlock);
    void            LowerBailOnCreatedMissingValue(IR::Instr *const instr, const bool isInHelperBlock);
    void            LowerBoundCheck(IR::Instr *const instr);

    IR::Instr *     LowerBailTarget(IR::Instr * instr);
    IR::Instr *     SplitBailOnImplicitCall(IR::Instr *& instr);
    IR::Instr *     SplitBailOnImplicitCall(IR::Instr * instr, IR::Instr * helperCall, IR::Instr * insertBeforeInstr);

    IR::Instr *     SplitBailForDebugger(IR::Instr* instr);

    IR::Instr *     SplitBailOnResultCondition(IR::Instr *const instr) const;
    void            LowerBailOnResultCondition(IR::Instr *const instr, IR::LabelInstr * *const bailOutLabel, IR::LabelInstr * *const skipBailOutLabel);
    void            PreserveSourcesForBailOnResultCondition(IR::Instr *const instr, IR::LabelInstr *const skipBailOutLabel) const;
    void            LowerInstrWithBailOnResultCondition(IR::Instr *const instr, const IR::BailOutKind bailOutKind, IR::LabelInstr *const bailOutLabel, IR::LabelInstr *const skipBailOutLabel) const;
    void            GenerateObjectTestAndTypeLoad(IR::Instr *instrLdSt, IR::RegOpnd *opndBase, IR::RegOpnd *opndType, IR::LabelInstr *labelHelper);
    IR::LabelInstr *GenerateBailOut(IR::Instr * instr, IR::BranchInstr * branchInstr = nullptr, IR::LabelInstr * labelBailOut = nullptr);
    void            GenerateJumpToEpilogForBailOut(BailOutInfo * bailOutInfo, IR::Instr *instrAfter);

    void            LowerDivI4(IR::Instr * const instr);
    void            LowerRemI4(IR::Instr * const instr);
    void            LowerDivI4Common(IR::Instr * const instr);
    void            LowerRemR8(IR::Instr * const instr);
    void            LowerRemR4(IR::Instr * const instr);

    void            LowerInlineeStart(IR::Instr * instr);
    void            LowerInlineeEnd(IR::Instr * instr);

    IR::Instr*      LoadArgumentsFromStack(IR::Instr * instr);

    static
    IR::SymOpnd*    LoadCallInfo(IR::Instr * instrInsert);

    IR::Instr *     LowerCallIDynamic(IR::Instr * callInstr, ushort callFlags);
    IR::Opnd*       GenerateArgOutForInlineeStackArgs(IR::Instr* callInstr, IR::Instr* stackArgsInstr);
    IR::Opnd*       GenerateArgOutForStackArgs(IR::Instr* callInstr, IR::Instr* stackArgsInstr);

    bool            GenerateFastStackArgumentsLdElemI(IR::Instr* ldElem);
    IR::IndirOpnd*  GetArgsIndirOpndForInlinee(IR::Instr* ldElem, IR::Opnd* valueOpnd);
    IR::IndirOpnd*  GetArgsIndirOpndForTopFunction(IR::Instr* ldElem, IR::Opnd* valueOpnd);
    void            GenerateCheckForArgumentsLength(IR::Instr* ldElem, IR::LabelInstr* labelCreateHeapArgs, IR::Opnd* actualParamOpnd, IR::Opnd* valueOpnd, Js::OpCode opcode);
    bool            GenerateFastArgumentsLdElemI(IR::Instr* ldElem, IR::LabelInstr * labelHelper, IR::LabelInstr *labelFallThru);
    bool            GenerateFastRealStackArgumentsLdLen(IR::Instr *ldLen);
    bool            GenerateFastArgumentsLdLen(IR::Instr *ldLen, IR::LabelInstr* labelHelper, IR::LabelInstr* labelFallThru);
    static const uint16  GetFormalParamOffset() { /*formal start after frame pointer, return address, function object, callInfo*/ return 4;};

    IR::RegOpnd*    GenerateFunctionTypeFromFixedFunctionObject(IR::Instr *callInstr, IR::Opnd* functionObjOpnd);

    bool GenerateFastLdFld(IR::Instr * const instrLdFld, IR::JnHelperMethod helperMethod, IR::JnHelperMethod polymorphicHelperMethod,
        IR::LabelInstr ** labelBailOut, IR::RegOpnd* typeOpnd, bool* pIsHelper, IR::LabelInstr** pLabelHelper);
    void GenerateAuxSlotAdjustmentRequiredCheck(IR::Instr * instrToInsertBefore, IR::RegOpnd * opndInlineCache, IR::LabelInstr * labelHelper);
    void GenerateSetObjectTypeFromInlineCache(IR::Instr * instrToInsertBefore, IR::RegOpnd * opndBase, IR::RegOpnd * opndInlineCache, bool isTypeTagged);
    bool GenerateFastStFld(IR::Instr * const instrStFld, IR::JnHelperMethod helperMethod, IR::JnHelperMethod polymorphicHelperMethod,
        IR::LabelInstr ** labelBailOut, IR::RegOpnd* typeOpnd, bool* pIsHelper, IR::LabelInstr** pLabelHelper, bool withPutFlags = false, Js::PropertyOperationFlags flags = Js::PropertyOperation_None);

    bool            GenerateFastStFldForCustomProperty(IR::Instr *const instr, IR::LabelInstr * *const labelHelperRef);

    void            RelocateCallDirectToHelperPath(IR::Instr* argoutInlineSpecialized, IR::LabelInstr* labelHelper);

    bool            TryGenerateFastBrOrCmTypeOf(IR::Instr *instr, IR::Instr **prev, bool *pfNoLower);
    void            GenerateFastBrTypeOf(IR::Instr *branch, IR::RegOpnd *object, IR::IntConstOpnd *typeIdOpnd, IR::Instr *typeOf, bool *pfNoLower);
    void            GenerateFastCmTypeOf(IR::Instr *compare, IR::RegOpnd *object, IR::IntConstOpnd *typeIdOpnd, IR::Instr *typeOf, bool *pfNoLower);
    void            GenerateFalsyObjectTest(IR::Instr *insertInstr, IR::RegOpnd *TypeOpnd, Js::TypeId typeIdToCheck, IR::LabelInstr* target, IR::LabelInstr* done, bool isNeqOp);

    void            GenerateLoadNewTarget(IR::Instr* instrInsert);
    void            GenerateCheckForCallFlagNew(IR::Instr* instrInsert);
    void            GenerateGetCurrentFunctionObject(IR::Instr * instr);
    IR::Opnd *      GetInlineCacheFromFuncObjectForRuntimeUse(IR::Instr * instr, IR::PropertySymOpnd * propSymOpnd, bool isHelper);

    IR::Instr *     LowerInitClass(IR::Instr * instr);

    IR::RegOpnd *   GenerateGetImmutableOrScriptUnreferencedString(IR::RegOpnd * strOpnd, IR::Instr * insertBeforeInstr, IR::JnHelperMethod helperMethod, bool reloadDst = true);
    void            LowerNewConcatStrMulti(IR::Instr * instr);
    void            LowerNewConcatStrMultiBE(IR::Instr * instr);
    void            LowerSetConcatStrMultiItem(IR::Instr * instr);
    void            LowerConvStr(IR::Instr * instr);
    void            LowerCoerseStr(IR::Instr * instr);
    void            LowerCoerseRegex(IR::Instr * instr);
    void            LowerCoerseStrOrRegex(IR::Instr * instr);
    void            LowerConvPrimStr(IR::Instr * instr);
    void            LowerConvStrCommon(IR::JnHelperMethod helper, IR::Instr * instr);

    void            GenerateRecyclerAlloc(IR::JnHelperMethod allocHelper, size_t allocSize, IR::RegOpnd* newObjDst, IR::Instr* insertionPointInstr, bool inOpHelper = false);

    template <typename ArrayType>
    IR::RegOpnd *   GenerateArrayAlloc(IR::Instr *instr, uint32 * psize, Js::ArrayCallSiteInfo * arrayInfo, bool * pIsHeadSegmentZeroed);

    void            GenerateProfiledNewScObjArrayFastPath(IR::Instr *instr, Js::ArrayCallSiteInfo * arrayInfo, RecyclerWeakReference<Js::FunctionBody> * weakFuncRef, uint32 length);
    void            GenerateProfiledNewScArrayFastPath(IR::Instr *instr, Js::ArrayCallSiteInfo * arrayInfo, RecyclerWeakReference<Js::FunctionBody> * weakFuncRef, uint32 length);
    void            GenerateMemInit(IR::RegOpnd * opnd, int32 offset, int32 value, IR::Instr * insertBeforeInstr, bool isZeroed = false);
    void            GenerateMemInit(IR::RegOpnd * opnd, int32 offset, uint32 value, IR::Instr * insertBeforeInstr, bool isZeroed = false);
    void            GenerateMemInitNull(IR::RegOpnd * opnd, int32 offset, IR::Instr * insertBeforeInstr, bool isZeroed = false);
    void            GenerateMemInit(IR::RegOpnd * opnd, int32 offset, IR::Opnd * value, IR::Instr * insertBeforeInstr, bool isZeroed = false);
    void            GenerateRecyclerMemInit(IR::RegOpnd * opnd, int32 offset, int32 value, IR::Instr * insertBeforeInstr);
    void            GenerateRecyclerMemInit(IR::RegOpnd * opnd, int32 offset, uint32 value, IR::Instr * insertBeforeInstr);
    void            GenerateRecyclerMemInitNull(IR::RegOpnd * opnd, int32 offset, IR::Instr * insertBeforeInstr);
    void            GenerateRecyclerMemInit(IR::RegOpnd * opnd, int32 offset, IR::Opnd * value, IR::Instr * insertBeforeInstr);
    void            GenerateMemCopy(IR::Opnd * dst, IR::Opnd * src, uint32 size, IR::Instr * insertBeforeInstr);

    void            GenerateDynamicObjectAlloc(IR::Instr * newObjInstr, uint inlineSlotCount, uint slotCount, IR::RegOpnd * newObjDst, IR::Opnd * typeSrc);
    bool            GenerateSimplifiedInt4Rem(IR::Instr *const remInstr, IR::LabelInstr *const skipBailOutLabel = nullptr) const;
    IR::Instr*      GenerateCallProfiling(Js::ProfileId profileId, Js::InlineCacheIndex inlineCacheIndex, IR::Opnd* retval, IR::Opnd*calleeFunctionObjOpnd, IR::Opnd* callInfo, bool returnTypeOnly, IR::Instr*callInstr, IR::Instr*insertAfter);
    IR::Opnd*       GetImplicitCallFlagsOpnd();
    IR::Opnd*       CreateClearImplicitCallFlagsOpnd();

    IR::Instr *         LoadScriptContext(IR::Instr *instr);
    IR::Instr *         LoadFunctionBody(IR::Instr * instr);
    IR::Opnd *          LoadFunctionBodyOpnd(IR::Instr *instr);
    IR::Opnd *          LoadScriptContextOpnd(IR::Instr *instr);
    IR::Opnd *          LoadScriptContextValueOpnd(IR::Instr * instr, ScriptContextValue valueType);
    IR::Opnd *          LoadLibraryValueOpnd(IR::Instr * instr, LibraryValue valueType, RegNum regNum = RegNOREG);
    IR::Opnd *          LoadVTableValueOpnd(IR::Instr * instr, VTableValue vtableType);
    IR::Opnd *          LoadOptimizationOverridesValueOpnd(IR::Instr *instr, OptimizationOverridesValue valueType);
    IR::Opnd *          LoadNumberAllocatorValueOpnd(IR::Instr *instr, NumberAllocatorValue valueType);
    IR::Opnd *          LoadIsInstInlineCacheOpnd(IR::Instr *instr, uint inlineCacheIndex);
    IR::Opnd *          LoadRuntimeInlineCacheOpnd(IR::Instr * instr, IR::PropertySymOpnd * sym, bool isHelper = false);

    void            LowerSpreadArrayLiteral(IR::Instr *instr);
    IR::Instr*      LowerSpreadCall(IR::Instr *instr, Js::CallFlags callFlags, bool setupProfiledVersion = false);
    void            LowerInlineSpreadArgOutLoopUsingRegisters(IR::Instr *callInstr, IR::RegOpnd *indexOpnd, IR::RegOpnd *arrayElementsStartOpnd);
    IR::Instr*      LowerCallIDynamicSpread(IR::Instr * callInstr, ushort callFlags);

    void            LowerNewScopeSlots(IR::Instr * instr, bool doStackSlots);
    void            LowerLdFrameDisplay(IR::Instr * instr, bool doStackDisplay);
    void            LowerLdInnerFrameDisplay(IR::Instr * instr);

    IR::AddrOpnd *  CreateFunctionBodyOpnd(Func *const func) const;
    IR::AddrOpnd *  CreateFunctionBodyOpnd(Js::FunctionBody *const functionBody) const;


    bool            GenerateRecyclerOrMarkTempAlloc(IR::Instr * instr, IR::RegOpnd * dstOpnd, IR::JnHelperMethod allocHelper, size_t allocSize, IR::SymOpnd ** tempObjectSymOpnd);
    IR::SymOpnd *   GenerateMarkTempAlloc(IR::RegOpnd *const dstOpnd, const size_t allocSize, IR::Instr *const insertBeforeInstr);
    void            LowerBrFncCachedScopeEq(IR::Instr *instr);

    IR::Instr*      InsertLoweredRegionStartMarker(IR::Instr* instrToInsertBefore);
    IR::Instr*      RemoveLoweredRegionStartMarker(IR::Instr* startMarkerInstr);

    void                    ConvertArgOpndIfGeneratorFunction(IR::Instr *instrArgIn, IR::RegOpnd *generatorArgsPtrOpnd);
    static IR::RegOpnd *    LoadGeneratorArgsPtr(IR::Instr *instrInsert);
    static IR::Instr *      LoadGeneratorObject(IR::Instr *instrInsert);

    IR::Opnd *      LoadSlotArrayWithCachedLocalType(IR::Instr * instrInsert, IR::PropertySymOpnd *propertySymOpnd);
    IR::Opnd *      LoadSlotArrayWithCachedProtoType(IR::Instr * instrInsert, IR::PropertySymOpnd *propertySymOpnd);
    IR::Instr *     LowerLdAsmJsEnv(IR::Instr *instr);
    IR::Instr *     LowerLdEnv(IR::Instr *instr);
    IR::Instr *     LowerFrameDisplayCheck(IR::Instr * instr);
    IR::Instr *     LowerSlotArrayCheck(IR::Instr * instr);
    void            InsertSlotArrayCheck(IR::Instr * instr, StackSym * dstSym, uint32 slotId);
    void            InsertFrameDisplayCheck(IR::Instr * instr, StackSym * dstSym, FrameDisplayCheckRecord * record);

    IR::RegOpnd *   LoadIndexFromLikelyFloat(IR::RegOpnd *indexOpnd, const bool skipNegativeCheck, IR::LabelInstr *const notTaggedIntLabel, IR::LabelInstr *const negativeLabel, IR::Instr *const insertBeforeInstr);

    void MarkConstantAddressRegOpndLiveOnBackEdge(IR::LabelInstr * loopTop);
#if DBG
    static void LegalizeVerifyRange(IR::Instr * instrStart, IR::Instr * instrLast);
#endif

    IR::Instr*      LowerTry(IR::Instr* instr, bool tryCatch);
    void            EnsureBailoutReturnValueSym();
    void            EnsureHasBailedOutSym();
    void            InsertReturnThunkForRegion(Region* region, IR::LabelInstr* restoreLabel);
    void            SetHasBailedOut(IR::Instr * bailoutInstr);
    IR::Instr*      EmitEHBailoutStackRestore(IR::Instr * bailoutInstr);
    void            EmitSaveEHBailoutReturnValueAndJumpToRetThunk(IR::Instr * instr);
    void            EmitRestoreReturnValueFromEHBailout(IR::LabelInstr * restoreLabel, IR::LabelInstr * epilogLabel);

public:
    static IRType   GetImplicitCallFlagsType()
    {
        static_assert(sizeof(Js::ImplicitCallFlags) == 1, "If this size changes, change TyUint8 in the line below to the right type.");
        return TyUint8;
    }
    static IRType   GetFldInfoFlagsType()
    {
        static_assert(sizeof(Js::FldInfoFlags) == 1, "If this size changes, change TyUint8 in the line below to the right type.");
        return TyUint8;
    }

    static bool     IsSpreadCall(IR::Instr *instr);
    static
    IR::Instr*      GetLdSpreadIndicesInstr(IR::Instr *instr);
    static bool DoLazyBailout(Func* func) { return PHASE_ON(Js::LazyBailoutPhase, func) && !func->IsLoopBody(); }
    static bool DoLazyFixedTypeBailout(Func* func) { return DoLazyBailout(func) && !PHASE_OFF(Js::LazyFixedTypeBailoutPhase, func); }
    static bool DoLazyFixedDataBailout(Func* func) { return DoLazyBailout(func) && !PHASE_OFF(Js::LazyFixedDataBailoutPhase, func); }
    LowererMD * GetLowererMD() { return &m_lowererMD; }
private:
    Func *          m_func;
    LowererMD       m_lowererMD;
    JitArenaAllocator *m_alloc;
    IR::Opnd       * nextStackFunctionOpnd;
    IR::LabelInstr * outerMostLoopLabel;
    BVSparse<JitArenaAllocator> * initializedTempSym;
    BVSparse<JitArenaAllocator> * addToLiveOnBackEdgeSyms;
    Region *        currentRegion;

};
