//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once

class Lowerer;

enum LegalForms : uint
{
    L_None = 0x0,
    L_Reg = 0x1,
    L_Mem = 0x2,
    L_Imm32 = 0x4,  // supports 8-bit, 16-bit, and 32-bit immediate values
    L_Ptr = 0x8     // supports 8-bit, 16-bit, 32-bit, and 64-bit immediate values on 64-bit architectures
};

#include "LowererMDArch.h"

///---------------------------------------------------------------------------
///
/// class LowererMD
///
///---------------------------------------------------------------------------

class LowererMD
{
public:
    LowererMD(Func *func) :
        m_func(func),
        lowererMDArch(func),
        floatTmpMap(nullptr),
        bvFloatTmpInits(nullptr),
        m_simd128OpCodesMap(nullptr)
    {
    }

    friend class LowererMDArch;

    static  bool            IsAssign(IR::Instr *instr);
    static  bool            IsCall(IR::Instr *instr);
    static  bool            IsUnconditionalBranch(const IR::Instr *instr);
    static  void            InvertBranch(IR::BranchInstr *instr);
    static  void            ReverseBranch(IR::BranchInstr *branchInstr);
    static  Js::OpCode      MDBranchOpcode(Js::OpCode opcode);
    static Js::OpCode       MDUnsignedBranchOpcode(Js::OpCode opcode);
    static Js::OpCode       MDCompareWithZeroBranchOpcode(Js::OpCode opcode);
    static Js::OpCode       MDConvertFloat64ToInt32Opcode(const RoundMode roundMode);
    static void             ChangeToAdd(IR::Instr *const instr, const bool needFlags);
    static void             ChangeToSub(IR::Instr *const instr, const bool needFlags);
    static void             ChangeToShift(IR::Instr *const instr, const bool needFlags);
    static void             ChangeToMul(IR::Instr *const instr, const bool hasOverflowCheck = false);
    static const uint16     GetFormalParamOffset();
    static const Js::OpCode MDUncondBranchOpcode;
    static const Js::OpCode MDExtend32Opcode;
    static const Js::OpCode MDTestOpcode;
    static const Js::OpCode MDOrOpcode;
    static const Js::OpCode MDOverflowBranchOpcode;
    static const Js::OpCode MDNotOverflowBranchOpcode;
    static const Js::OpCode MDConvertFloat32ToFloat64Opcode;
    static const Js::OpCode MDConvertFloat64ToFloat32Opcode;
    static const Js::OpCode MDCallOpcode;
    static const Js::OpCode MDImulOpcode;

    UINT FloatPrefThreshold;

public:
            void            Init(Lowerer *lowerer);
            IR::Opnd *      GenerateMemRef(void *addr, IRType type, IR::Instr *instr, bool dontEncode = false);
            IR::Instr *     ChangeToHelperCall(IR::Instr * instr, IR::JnHelperMethod helperMethod, IR::LabelInstr *labelBailOut = nullptr,
                                               IR::Opnd *opndInstance = nullptr, IR::PropertySymOpnd * propSymOpnd = nullptr, bool isHelperContinuation = false);
            void            FinalLower();
#ifdef _M_X64
            void            FlipHelperCallArgsOrder()
            {
                lowererMDArch.FlipHelperCallArgsOrder();
            }
#endif
            IR::Instr *     ChangeToHelperCallMem(IR::Instr * instr, IR::JnHelperMethod helperMethod);
    static  IR::Instr *     CreateAssign(IR::Opnd *dst, IR::Opnd *src, IR::Instr *instrInsertPt);

    static  IR::Instr *     ChangeToAssign(IR::Instr * instr);
    static  IR::Instr *     ChangeToAssign(IR::Instr * instr, IRType type);
    static  IR::Instr *     ChangeToLea(IR::Instr *const instr);
    static  void            ImmedSrcToReg(IR::Instr * instr, IR::Opnd * newOpnd, int srcNum);

            IR::Instr *     LoadInputParamCount(IR::Instr * instr, int adjust = 0, bool needFlags = false);
            IR::Instr *     LoadStackArgPtr(IR::Instr * instr);
            IR::Opnd *      CreateStackArgumentsSlotOpnd();
            IR::Instr *     LoadArgumentsFromFrame(IR::Instr * instr);
            IR::Instr *     LoadArgumentCount(IR::Instr * instr);
            IR::Instr *     LoadHeapArguments(IR::Instr * instr, bool force = false, IR::Opnd* opndInputParamCount = nullptr);
            IR::Instr *     LoadHeapArgsCached(IR::Instr * instr);
            IR::Instr *     LoadFuncExpression(IR::Instr * instr);
            IR::Instr *     LowerRet(IR::Instr * instr);
            IR::Instr *     LowerUncondBranch(IR::Instr * instr);
            IR::Instr *     LowerMultiBranch(IR::Instr * instr);
            IR::Instr *     LowerCondBranch(IR::Instr * instr);
            IR::Instr *     LoadFunctionObjectOpnd(IR::Instr *instr, IR::Opnd *&functionObjOpnd);
            IR::Instr *     LowerLdSuper(IR::Instr *instr, IR::JnHelperMethod helperOpCode);
            IR::Instr *     LowerNewScObject(IR::Instr *newObjInstr);
            void            ForceDstToReg(IR::Instr *instr);

public:
            template <bool verify = false>
            static void     Legalize(IR::Instr *const instr, bool fPostRegAlloc = false);
private:
            template <bool verify>
            static void     LegalizeOpnds(IR::Instr *const instr, const uint dstForms, const uint src1Forms, uint src2Forms);
            template <bool verify>
            static void     LegalizeDst(IR::Instr *const instr, const uint forms);
            template <bool verify>
            static void     LegalizeSrc(IR::Instr *const instr, IR::Opnd *src, const uint forms);
            template <bool verify = false>
            static void     MakeDstEquSrc1(IR::Instr *const instr);
public:
            IR::Instr *     GenerateSmIntPairTest(IR::Instr * instrInsert, IR::Opnd * opndSrc1, IR::Opnd * opndSrc2, IR::LabelInstr * labelFail);
            void            GenerateSmIntTest(IR::Opnd *opndSrc, IR::Instr *instrInsert, IR::LabelInstr *labelHelper, IR::Instr **instrFirst = nullptr, bool fContinueLabel = false);
            void            GenerateTaggedZeroTest( IR::Opnd * opndSrc, IR::Instr * instrInsert, IR::LabelInstr * labelHelper = nullptr);
            bool            GenerateObjectTest(IR::Opnd * opndSrc, IR::Instr * insertInstr, IR::LabelInstr * labelInstr, bool fContinueLabel = false);
            bool            GenerateJSBooleanTest(IR::RegOpnd * regSrc, IR::Instr * insertInstr, IR::LabelInstr * labelTarget, bool fContinueLabel = false);
            void            GenerateBooleanTest(IR::Opnd * regSrc, IR::Instr * insertInstr, IR::LabelInstr * labelTarget, bool fContinueLabel = false);
            void            GenerateInt32ToVarConversion( IR::Opnd * opndSrc, IR::Instr * insertInstr );
            void            GenerateFloatTest( IR::RegOpnd * opndSrc, IR::Instr * insertInstr, IR::LabelInstr* labelHelper, const bool checkForNullInLoopBody = false);
            void            GenerateIsDynamicObject(IR::RegOpnd *regOpnd, IR::Instr *insertInstr, IR::LabelInstr *labelHelper, bool fContinueLabel = false);
            void            GenerateIsRecyclableObject(IR::RegOpnd *regOpnd, IR::Instr *insertInstr, IR::LabelInstr *labelhelper, bool checkObjectAndDynamicObject = true);
#if FLOATVAR
            IR::RegOpnd*    CheckFloatAndUntag(IR::RegOpnd * opndSrc, IR::Instr * insertInstr, IR::LabelInstr* labelHelper);
#endif
            bool            GenerateFastBrString(IR::BranchInstr* instr);
            bool            GenerateFastCmSrEqConst(IR::Instr *instr);
            bool            GenerateFastCmXxTaggedInt(IR::Instr *instr);
            void            GenerateFastCmXxI4(IR::Instr *instr);
            void            GenerateFastCmXxR8(IR::Instr *instr);
            void            GenerateFastCmXx(IR::Instr *instr);
            IR::Instr *     GenerateConvBool(IR::Instr *instr);
            void            GenerateFastDivByPow2(IR::Instr *instr);
            bool            GenerateFastAdd(IR::Instr * instrAdd);
#if DBG
            static void     GenerateDebugBreak( IR::Instr * insertInstr );
#endif
            bool            GenerateFastSub(IR::Instr * instrSub);
            bool            GenerateFastMul(IR::Instr * instrMul);
            bool            GenerateFastAnd(IR::Instr * instrAnd);
            bool            GenerateFastXor(IR::Instr * instrXor);
            bool            GenerateFastOr(IR::Instr * instrOr);
            bool            GenerateFastNot(IR::Instr * instrNot);

            bool            GenerateFastShiftLeft(IR::Instr * instrShift);
            bool            GenerateFastShiftRight(IR::Instr * instrShift);
            bool            GenerateFastNeg(IR::Instr * instrNeg);
            void            GenerateFastBrS(IR::BranchInstr *brInstr);
            IR::IndirOpnd*  GetArgsIndirOpndForTopFunction(IR::Instr* ldElem, IR::Opnd* valueOpnd);
            IR::IndirOpnd*  GetArgsIndirOpndForInlinee(IR::Instr* ldElem, IR::Opnd* valueOpnd);
            void            GenerateCheckForArgumentsLength(IR::Instr* ldElem, IR::LabelInstr* labelCreateHeapArgs, IR::Opnd* actualParamOpnd, IR::Opnd* valueOpnd, Js::OpCode);
            IR::RegOpnd *   LoadNonnegativeIndex(IR::RegOpnd *indexOpnd, const bool skipNegativeCheck, IR::LabelInstr *const notTaggedIntLabel, IR::LabelInstr *const negativeLabel, IR::Instr *const insertBeforeInstr);
            IR::RegOpnd *   GenerateUntagVar(IR::RegOpnd * opnd, IR::LabelInstr * labelFail, IR::Instr * insertBeforeInstr, bool generateTagCheck = true);
            IR::IndirOpnd * GenerateFastElemIStringIndexCommon(IR::Instr * ldElem, bool isStore, IR::IndirOpnd * indirOpnd,  IR::LabelInstr * labelHelper);
            bool            GenerateFastLdMethodFromFlags(IR::Instr * instrLdFld);
            IR::Instr *     GenerateFastScopedLdFld(IR::Instr * instrLdFld);
            IR::Instr *     GenerateFastScopedStFld(IR::Instr * instrStFld);
            void            GenerateFastBrBReturn(IR::Instr *instr);
            void            GenerateFastAbs(IR::Opnd *dst, IR::Opnd *src, IR::Instr *callInstr, IR::Instr *insertInstr, IR::LabelInstr *labelHelper, IR::LabelInstr *doneLabel);
            IR::Instr *     GenerateFloatAbs(IR::RegOpnd * regOpnd, IR::Instr * insertInstr);
            bool            GenerateFastCharAt(Js::BuiltinFunction index, IR::Opnd *dst, IR::Opnd *srcStr, IR::Opnd *srcIndex, IR::Instr *callInstr, IR::Instr *insertInstr,
                IR::LabelInstr *labelHelper, IR::LabelInstr *doneLabel);
            void            GenerateClz(IR::Instr * instr);
            bool            TryGenerateFastMulAdd(IR::Instr * instrAdd, IR::Instr ** pInstrPrev);
            bool            GenerateLdThisCheck(IR::Instr * instr);
            bool            GenerateLdThisStrict(IR::Instr * instr);
            BVSparse<JitArenaAllocator>* GatherFltTmps();
            void            GenerateFastInlineBuiltInCall(IR::Instr* instr, IR::JnHelperMethod helperMethod);
            void            HelperCallForAsmMathBuiltin(IR::Instr* instr, IR::JnHelperMethod helperMethodFloat, IR::JnHelperMethod helperMethodDouble);
            void            GenerateFastInlineBuiltInMathAbs(IR::Instr* instr);
            IR::Opnd*       IsOpndNegZero(IR::Opnd* opnd, IR::Instr* instr);
            IR::Instr *     CloneSlowPath(IR::Instr * instrEndFloatRange, IR::Instr * instrInsert);
            bool            IsCloneDone(IR::Instr * instr, BVSparse<JitArenaAllocator> *bvTmps);
            IR::Instr *     EnsureAdjacentArgs(IR::Instr * instrArg);
            void            SaveDoubleToVar(IR::RegOpnd * dstOpnd, IR::RegOpnd *opndFloat, IR::Instr *instrOrig, IR::Instr *instrInsert, bool isHelper = false);
#if !FLOATVAR
            void            GenerateNumberAllocation(IR::RegOpnd * opndDst, IR::Instr * instrInsert, bool isHelper);
#endif
            void            GenerateFastRecyclerAlloc(size_t allocSize, IR::RegOpnd* newObjDst, IR::Instr* insertionPointInstr, IR::LabelInstr* allocHelperLabel, IR::LabelInstr* allocDoneLabel);
#ifdef _CONTROL_FLOW_GUARD
            void            GenerateCFGCheck(IR::Opnd * entryPointOpnd, IR::Instr * insertBeforeInstr);
#endif
            static IR::Instr *LoadFloatZero(IR::Opnd * opndDst, IR::Instr * instrInsert);
            static IR::Instr *LoadFloatValue(IR::Opnd * opndDst, double value, IR::Instr * instrInsert);
            IR::Instr *     LoadStackAddress(StackSym *sym, IR::RegOpnd *optionalDstOpnd = nullptr);
     static void            EmitInt4Instr(IR::Instr *instr);
     static void            EmitPtrInstr(IR::Instr *instr);
            void            EmitLoadVar(IR::Instr *instr, bool isFromUint32 = false, bool isHelper = false);
            void            EmitLoadVarNoCheck(IR::RegOpnd * dst, IR::RegOpnd * src, IR::Instr *instrLoad, bool isFromUint32, bool isHelper);
            bool            EmitLoadInt32(IR::Instr *instr);
            void            EmitIntToFloat(IR::Opnd *dst, IR::Opnd *src, IR::Instr *instrInsert);
            void            EmitUIntToFloat(IR::Opnd *dst, IR::Opnd *src, IR::Instr *instrInsert);
            void            EmitFloatToInt(IR::Opnd *dst, IR::Opnd *src, IR::Instr *instrInsert);
            void            EmitFloat32ToFloat64(IR::Opnd *dst, IR::Opnd *src, IR::Instr *instrInsert);
            static IR::Instr *InsertConvertFloat64ToInt32(const RoundMode roundMode, IR::Opnd *const dst, IR::Opnd *const src, IR::Instr *const insertBeforeInstr);
            void            ConvertFloatToInt32(IR::Opnd* intOpnd, IR::Opnd* floatOpnd, IR::LabelInstr * labelHelper, IR::LabelInstr * labelDone, IR::Instr * instInsert);
            void            EmitLoadFloatFromNumber(IR::Opnd *dst, IR::Opnd *src, IR::Instr *insertInstr);
            IR::RegOpnd *   EmitLoadFloat(IR::Opnd *dst, IR::Opnd *src, IR::Instr *insertInstr);
            static void     EmitNon32BitOvfCheck(IR::Instr *instr, IR::Instr *insertInstr, IR::LabelInstr* bailOutLabel);

            static void     LowerInt4NegWithBailOut(IR::Instr *const instr, const IR::BailOutKind bailOutKind, IR::LabelInstr *const bailOutLabel, IR::LabelInstr *const skipBailOutLabel);
            static void     LowerInt4AddWithBailOut(IR::Instr *const instr, const IR::BailOutKind bailOutKind, IR::LabelInstr *const bailOutLabel, IR::LabelInstr *const skipBailOutLabel);
            static void     LowerInt4SubWithBailOut(IR::Instr *const instr, const IR::BailOutKind bailOutKind, IR::LabelInstr *const bailOutLabel, IR::LabelInstr *const skipBailOutLabel);
            static void     LowerInt4MulWithBailOut(IR::Instr *const instr, const IR::BailOutKind bailOutKind, IR::LabelInstr *const bailOutLabel, IR::LabelInstr *const skipBailOutLabel);
            void     LowerInt4RemWithBailOut(IR::Instr *const instr, const IR::BailOutKind bailOutKind, IR::LabelInstr *const bailOutLabel, IR::LabelInstr *const skipBailOutLabel) const;

            static bool     GenerateSimplifiedInt4Mul(IR::Instr *const mulInstr, const IR::BailOutKind bailOutKind = IR::BailOutInvalid, IR::LabelInstr *const bailOutLabel = nullptr);
            static bool     GenerateSimplifiedInt4Rem(IR::Instr *const remInstr, IR::LabelInstr *const skipBailOutLabel = nullptr);

            IR::Instr *     LowerCatch(IR::Instr *instr);

            IR::Instr *     LowerGetCachedFunc(IR::Instr *instr);
            IR::Instr *     LowerCommitScope(IR::Instr *instr);
            IR::Instr *     LowerTry(IR::Instr *instr, IR::JnHelperMethod helperMethod);
            IR::Instr *     LowerLeave(IR::Instr *instr, IR::LabelInstr * targetInstr, bool fromFinalLower, bool isOrphanedLeave = false);
            IR::Instr *     LowerEHRegionReturn(IR::Instr * insertBeforeInstr, IR::Opnd * targetOpnd);
            IR::Instr *     LowerLeaveNull(IR::Instr *instr);
            IR::Instr *     LowerCallHelper(IR::Instr *instrCall);

            IR::LabelInstr *GetBailOutStackRestoreLabel(BailOutInfo * bailOutInfo, IR::LabelInstr * exitTargetInstr);
            StackSym *      GetImplicitParamSlotSym(Js::ArgSlot argSlot);
     static StackSym *      GetImplicitParamSlotSym(Js::ArgSlot argSlot, Func * func);

            Lowerer*        GetLowerer() { return m_lowerer; }

            bool            GenerateFastIsInst(IR::Instr * instr);
            void            GenerateIsJsObjectTest(IR::RegOpnd* instanceReg, IR::Instr* insertInstr, IR::LabelInstr* labelHelper);

public:
            //
            // These methods are simply forwarded to lowererMDArch
            //
            IR::Instr *         LowerAsmJsCallI(IR::Instr * callInstr);
            IR::Instr *         LowerAsmJsCallE(IR::Instr * callInstr);
            IR::Instr *         LowerAsmJsLdElemHelper(IR::Instr * callInstr);
            IR::Instr *         LowerAsmJsStElemHelper(IR::Instr * callInstr);
            IR::Instr *         LowerCall(IR::Instr * callInstr, Js::ArgSlot argCount);
            IR::Instr *         LowerCallI(IR::Instr * callInstr, ushort callFlags, bool isHelper = false, IR::Instr * insertBeforeInstrForCFG = nullptr);
            IR::Instr *         LowerCallPut(IR::Instr * callInstr);
            IR::Instr *         LoadHelperArgument(IR::Instr * instr, IR::Opnd * opndArg);
            IR::Instr *         LoadDoubleHelperArgument(IR::Instr * instr, IR::Opnd * opndArg);
            IR::Instr *         LowerEntryInstr(IR::EntryInstr * entryInstr);
            IR::Instr *         LowerExitInstr(IR::ExitInstr * exitInstr);
            IR::Instr *         LowerEntryInstrAsmJs(IR::EntryInstr * entryInstr);
            IR::Instr *         LowerExitInstrAsmJs(IR::ExitInstr * exitInstr);
            IR::Instr *         LoadNewScObjFirstArg(IR::Instr * instr, IR::Opnd * dst, ushort extraArgs = 0);
            IR::Instr *         LowerToFloat(IR::Instr *instr);
     static IR::BranchInstr *   LowerFloatCondBranch(IR::BranchInstr *instrBranch, bool ignoreNan = false);

     static Js::OpCode          GetLoadOp(IRType type) { return LowererMDArch::GetAssignOp(type); }
     static Js::OpCode          GetStoreOp(IRType type) { return LowererMDArch::GetAssignOp(type); }
     static RegNum              GetRegStackPointer() { return LowererMDArch::GetRegStackPointer(); }
     static RegNum              GetRegArgI4(int32 argNum) { return LowererMDArch::GetRegArgI4(argNum); }
     static RegNum              GetRegArgR8(int32 argNum) { return LowererMDArch::GetRegArgR8(argNum); }
     static RegNum              GetRegReturn(IRType type) { return LowererMDArch::GetRegReturn(type); }

            //All the following functions delegate to lowererMDArch
            IR::Instr *         LowerCallIDynamic(IR::Instr * callInstr, IR::Instr* saveThis, IR::Opnd* argsLengthOpnd, ushort callFlags, IR::Instr * insertBeforeInstrForCFG = nullptr)
            {
                return this->lowererMDArch.LowerCallIDynamic(callInstr, saveThis, argsLengthOpnd, callFlags, insertBeforeInstrForCFG);
            }

            IR::Instr *         LoadDynamicArgument(IR::Instr * instr, uint argNumber = 1) { return this->lowererMDArch.LoadDynamicArgument(instr, argNumber);}
            IR::Opnd*           GenerateArgOutForStackArgs(IR::Instr* callInstr, IR::Instr* stackArgsInstr) { return lowererMDArch.GenerateArgOutForStackArgs(callInstr, stackArgsInstr);}
     static RegNum              GetRegFramePointer() { return LowererMDArch::GetRegFramePointer(); }
     static BYTE                GetDefaultIndirScale() { return LowererMDArch::GetDefaultIndirScale(); }
            IR::Instr *         LoadDynamicArgumentUsingLength(IR::Instr *instr) { return this->lowererMDArch.LoadDynamicArgumentUsingLength(instr); }
            void                GenerateFunctionObjectTest(IR::Instr * callInstr, IR::RegOpnd  *functionObjOpnd, bool isHelper, IR::LabelInstr* afterCallLabel = nullptr) { this->lowererMDArch.GenerateFunctionObjectTest(callInstr, functionObjOpnd, isHelper, afterCallLabel); }
            int32               LowerCallArgs(IR::Instr *callInstr, ushort callFlags, ushort extraArgsCount = 1 /* for function object */) { return this->lowererMDArch.LowerCallArgs(callInstr, callFlags, extraArgsCount); }

            static void GenerateLoadTaggedType(IR::Instr * instrLdSt, IR::RegOpnd * opndType, IR::RegOpnd * opndTaggedType);
            static void GenerateLoadPolymorphicInlineCacheSlot(IR::Instr * instrLdSt, IR::RegOpnd * opndInlineCache, IR::RegOpnd * opndType, uint polymorphicInlineCacheSize);

            static void GenerateWriteBarrierAssign(IR::IndirOpnd * opndDst, IR::Opnd * opndSrc, IR::Instr * insertBeforeInstr);
            static void GenerateWriteBarrierAssign(IR::MemRefOpnd * opndDst, IR::Opnd * opndSrc, IR::Instr * insertBeforeInstr);
            static void ChangeToWriteBarrierAssign(IR::Instr * assignInstr);

            static IR::BranchInstr * GenerateLocalInlineCacheCheck(IR::Instr * instrLdSt, IR::RegOpnd * opndType, IR::RegOpnd * opndInlineCache, IR::LabelInstr * labelNext, bool checkTypeWithoutProperty = false);
            static IR::BranchInstr * GenerateProtoInlineCacheCheck(IR::Instr * instrLdSt, IR::RegOpnd * opndType, IR::RegOpnd * opndInlineCache, IR::LabelInstr * labelNext);
            static IR::BranchInstr * GenerateFlagInlineCacheCheck(IR::Instr * instrLdSt, IR::RegOpnd * opndType, IR::RegOpnd * opndInlineCache, IR::LabelInstr * labelNext);
            static IR::BranchInstr * GenerateFlagInlineCacheCheckForNoGetterSetter(IR::Instr * instrLdSt, IR::RegOpnd * opndInlineCache, IR::LabelInstr * labelNext);
            static IR::BranchInstr * GenerateFlagInlineCacheCheckForLocal(IR::Instr * instrLdSt, IR::RegOpnd * opndType, IR::RegOpnd * opndInlineCache, IR::LabelInstr * labelNext);
            static void GenerateLdFldFromLocalInlineCache(IR::Instr * instrLdFld, IR::RegOpnd * opndBase, IR::Opnd * opndDst, IR::RegOpnd * opndInlineCache, IR::LabelInstr * labelFallThru, bool isInlineSlot);
            static void GenerateLdFldFromProtoInlineCache(IR::Instr * instrLdFld, IR::RegOpnd * opndBase, IR::Opnd * opndDst, IR::RegOpnd * opndInlineCache, IR::LabelInstr * labelFallThru, bool isInlineSlot);
            static void GenerateLdLocalFldFromFlagInlineCache(IR::Instr * instrLdFld, IR::RegOpnd * opndBase, IR::Opnd * opndDst, IR::RegOpnd * opndInlineCache, IR::LabelInstr * labelFallThru, bool isInlineSlot);
            static void GenerateStFldFromLocalInlineCache(IR::Instr * instrStFld, IR::RegOpnd * opndBase, IR::Opnd * opndSrc, IR::RegOpnd * opndInlineCache, IR::LabelInstr * labelFallThru, bool isInlineSlot);

            IR::Instr *         LowerDivI4AndBailOnReminder(IR::Instr * instr, IR::LabelInstr * bailOutLabel);

            void                LowerInlineSpreadArgOutLoop(IR::Instr *callInstr, IR::RegOpnd *indexOpnd, IR::RegOpnd *arrayElementsStartOpnd)
            {
                this->lowererMDArch.LowerInlineSpreadArgOutLoop(callInstr, indexOpnd, arrayElementsStartOpnd);
            }

public:
    static void InsertIncUInt8PreventOverflow(IR::Opnd *const dst, IR::Opnd *const src, IR::Instr *const insertBeforeInstr, IR::Instr * *const onOverflowInsertBeforeInstrRef = nullptr);
    static void InsertDecUInt8PreventOverflow(IR::Opnd *const dst, IR::Opnd *const src, IR::Instr *const insertBeforeInstr, IR::Instr * *const onOverflowInsertBeforeInstrRef = nullptr);

    void                Simd128InitOpcodeMap();
    IR::Instr*          Simd128Instruction(IR::Instr* instr);
    IR::Instr*          Simd128LoadConst(IR::Instr* instr);
    bool                Simd128TryLowerMappedInstruction(IR::Instr *instr);
    IR::Instr*          Simd128LowerUnMappedInstruction(IR::Instr *instr);
    IR::Instr*          Simd128LowerConstructor(IR::Instr *instr);
    IR::Instr*          Simd128LowerLdLane(IR::Instr *instr);
    IR::Instr*          SIMD128LowerReplaceLane(IR::Instr *instr);
    IR::Instr*          Simd128LowerSplat(IR::Instr *instr);
    IR::Instr*          Simd128LowerRcp(IR::Instr *instr, bool removeInstr = true);
    IR::Instr*          Simd128LowerSqrt(IR::Instr *instr);
    IR::Instr*          Simd128LowerRcpSqrt(IR::Instr *instr);
    IR::Instr*          Simd128LowerSelect(IR::Instr *instr);
    IR::Instr*          Simd128LowerNegI4(IR::Instr *instr);
    IR::Instr*          Simd128LowerMulI4(IR::Instr *instr);
    IR::Instr*          Simd128LowerLoadElem(IR::Instr *instr);
    IR::Instr*          Simd128LowerStoreElem(IR::Instr *instr);
    IR::Instr*          Simd128LowerShuffle(IR::Instr *instr);
    IR::Opnd *          EnregisterIntConst(IR::Instr* instr, IR::Opnd *constOpnd);
    SList<IR::Opnd*>  * Simd128GetExtendedArgs(IR::Instr *instr);
    void                GenerateCheckedSimdLoad(IR::Instr * instr);
    void                GenerateSimdStore(IR::Instr * instr);

private:
    void GenerateFlagInlineCacheCheckForGetterSetter(
        IR::Instr * insertBeforeInstr,
        IR::RegOpnd * opndInlineCache,
        IR::LabelInstr * labelNext);

    void GenerateLdFldFromFlagInlineCache(
        IR::Instr * insertBeforeInstr,
        IR::RegOpnd * opndBase,
        IR::Opnd * opndDst,
        IR::RegOpnd * opndInlineCache,
        IR::LabelInstr * labelFallThru,
        bool isInlineSlot);

    IR::LabelInstr*   EmitLoadFloatCommon(IR::Opnd *dst, IR::Opnd *src, IR::Instr *insertInstr, bool needLabelHelper);
#ifdef RECYCLER_WRITE_BARRIER
    static void GenerateWriteBarrier(IR::Opnd * writeBarrierAddrRegOpnd, IR::Instr * insertBeforeInstr);
#endif

    // Data
protected:
    Func                     *m_func;
    Lowerer                  *m_lowerer;
    LowererMDArch             lowererMDArch;
    StackSymMap              *floatTmpMap;
    BVSparse<JitArenaAllocator> *bvFloatTmpInits;
    Js::OpCode              *m_simd128OpCodesMap; // used to map single-opcode SIMD operations
};
