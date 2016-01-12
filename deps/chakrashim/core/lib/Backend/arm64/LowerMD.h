//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once

class Lowerer;

///---------------------------------------------------------------------------
///
/// class LowererMD
///
///---------------------------------------------------------------------------

#ifdef DBG
#define INSERTDEBUGBREAK(instrInsert)\
    {\
        IR::Instr *int3 = IR::Instr::New(Js::OpCode::DEBUGBREAK, m_func);\
        instrInsert->InsertBefore(int3);\
    }
#else
#define INSERTDEBUGBREAK(instrInsert)
#endif

class LowererMD
{
public:
    static const int    MaxArgumentsToHelper = 16;

    LowererMD(Func *func) { }

    static  bool            IsUnconditionalBranch(const IR::Instr *instr) { __debugbreak(); return 0; }
    static  bool            IsAssign(const IR::Instr *instr) { __debugbreak(); return 0; }
    static  bool            IsCall(const IR::Instr *instr) { __debugbreak(); return 0; }
    static  bool            IsIndirectBranch(const IR::Instr *instr) { __debugbreak(); return 0; }
    static  bool            IsReturnInstr(const IR::Instr *instr) { __debugbreak(); return 0; }
    static  void            InvertBranch(IR::BranchInstr *instr) { __debugbreak(); }
    static Js::OpCode       MDBranchOpcode(Js::OpCode opcode) { __debugbreak(); return Js::OpCode::InvalidOpCode; }
    static Js::OpCode       MDUnsignedBranchOpcode(Js::OpCode opcode) { __debugbreak(); return Js::OpCode::InvalidOpCode; }
    static Js::OpCode       MDCompareWithZeroBranchOpcode(Js::OpCode opcode) { __debugbreak(); return Js::OpCode::InvalidOpCode; }
    static Js::OpCode       MDConvertFloat64ToInt32Opcode(const bool roundTowardZero) { __debugbreak(); return Js::OpCode::InvalidOpCode; }
    static void             ChangeToAdd(IR::Instr *const instr, const bool needFlags) { __debugbreak(); }
    static void             ChangeToSub(IR::Instr *const instr, const bool needFlags) { __debugbreak(); }
    static void             ChangeToShift(IR::Instr *const instr, const bool needFlags) { __debugbreak(); }
    static const uint16     GetFormalParamOffset() { __debugbreak(); return 0; }

    static const Js::OpCode MDUncondBranchOpcode;
    static const Js::OpCode MDTestOpcode;
    static const Js::OpCode MDOrOpcode;
    static const Js::OpCode MDOverflowBranchOpcode;
    static const Js::OpCode MDNotOverflowBranchOpcode;
    static const Js::OpCode MDConvertFloat32ToFloat64Opcode;
    static const Js::OpCode MDConvertFloat64ToFloat32Opcode;
    static const Js::OpCode MDCallOpcode;
    static const Js::OpCode MDImulOpcode;

public:
            void            Init(Lowerer *lowerer) { __debugbreak(); }
            void            FinalLower(){ __debugbreak(); }
            bool            FinalLowerAssign(IR::Instr* instr){ __debugbreak(); return 0;  };
            IR::Opnd *      GenerateMemRef(void *addr, IRType type, IR::Instr *instr, bool dontEncode = false) { __debugbreak(); return 0; }
            IR::Instr *     ChangeToHelperCall(IR::Instr * instr, IR::JnHelperMethod helperMethod, IR::LabelInstr *labelBailOut = NULL,
                            IR::Opnd *opndInstance = NULL, IR::PropertySymOpnd * propSymOpnd = nullptr, bool isHelperContinuation = false) { __debugbreak(); return 0; }
            IR::Instr *     ChangeToHelperCallMem(IR::Instr * instr, IR::JnHelperMethod helperMethod) { __debugbreak(); return 0; }
    static  IR::Instr *     CreateAssign(IR::Opnd *dst, IR::Opnd *src, IR::Instr *instrInsertPt) { __debugbreak(); return 0; }
    static  IR::Instr *     ChangeToAssign(IR::Instr * instr) { __debugbreak(); return 0; }
    static  IR::Instr *     ChangeToAssign(IR::Instr * instr, IRType type) { __debugbreak(); return 0; }
    static  IR::Instr *     ChangeToLea(IR::Instr *const instr) { __debugbreak(); return 0; }
    static  IR::Instr *     ForceDstToReg(IR::Instr *instr) { __debugbreak(); return 0; }
    static  void            ImmedSrcToReg(IR::Instr * instr, IR::Opnd * newOpnd, int srcNum) { __debugbreak(); }

            IR::Instr *     LoadArgumentCount(IR::Instr * instr) { __debugbreak(); return 0; }
            IR::Instr *     LoadStackArgPtr(IR::Instr * instr) { __debugbreak(); return 0; }
              IR::Instr *     LoadHeapArguments(IR::Instr * instrArgs, bool force = false, IR::Opnd *opndInputParamCount = nullptr) { __debugbreak(); return 0; }
              IR::Instr *     LoadHeapArgsCached(IR::Instr * instr) { __debugbreak(); return 0; }
              IR::Instr *     LoadInputParamCount(IR::Instr * instr, int adjust = 0, bool needFlags = false) { __debugbreak(); return 0; }
              IR::Instr *     LoadArgumentsFromFrame(IR::Instr * instr) { __debugbreak(); return 0; }
              IR::Instr *     LoadFuncExpression(IR::Instr * instr) { __debugbreak(); return 0; }
              IR::Instr *     LowerRet(IR::Instr * instr) { __debugbreak(); return 0; }
      static  IR::Instr *     LowerUncondBranch(IR::Instr * instr) { __debugbreak(); return 0; }
      static  IR::Instr *     LowerMultiBranch(IR::Instr * instr) { __debugbreak(); return 0; }
              IR::Instr *     LowerCondBranch(IR::Instr * instr) { __debugbreak(); return 0; }
              IR::Instr *     LoadFunctionObjectOpnd(IR::Instr *instr, IR::Opnd *&functionObjOpnd) { __debugbreak(); return 0; }
              IR::Instr *     LowerLdSuper(IR::Instr * instr, IR::JnHelperMethod helperOpCode) { __debugbreak(); return 0; }
              IR::Instr *     GenerateSmIntPairTest(IR::Instr * instrInsert, IR::Opnd * opndSrc1, IR::Opnd * opndSrc2, IR::LabelInstr * labelFail) { __debugbreak(); return 0; }
              void            GenerateTaggedZeroTest( IR::Opnd * opndSrc, IR::Instr * instrInsert, IR::LabelInstr * labelHelper = NULL) { __debugbreak(); }
              void            GenerateObjectPairTest(IR::Opnd * opndSrc1, IR::Opnd * opndSrc2, IR::Instr * insertInstr, IR::LabelInstr * labelTarget) { __debugbreak(); }
              bool            GenerateObjectTest(IR::Opnd * opndSrc, IR::Instr * insertInstr, IR::LabelInstr * labelTarget, bool fContinueLabel = false) { __debugbreak(); return false; }
              bool            GenerateFastBrString(IR::BranchInstr* instr) { __debugbreak(); return 0; }
              bool            GenerateFastCmSrEqConst(IR::Instr *instr) { __debugbreak(); return 0; }
              bool            GenerateFastCmXxI4(IR::Instr *instr) { __debugbreak(); return 0; }
              bool            GenerateFastCmXxR8(IR::Instr *instr) { Assert(UNREACHED); return NULL; }
              bool            GenerateFastCmXxTaggedInt(IR::Instr *instr) { __debugbreak(); return 0; }
              IR::Instr *     GenerateConvBool(IR::Instr *instr) { __debugbreak(); return 0; }


              void            GenerateClz(IR::Instr * instr) { __debugbreak(); }
              void            GenerateFastDivByPow2(IR::Instr *instr) { __debugbreak(); }
              bool            GenerateFastAdd(IR::Instr * instrAdd) { __debugbreak(); return 0; }
              bool            GenerateFastSub(IR::Instr * instrSub) { __debugbreak(); return 0; }
              bool            GenerateFastMul(IR::Instr * instrMul) { __debugbreak(); return 0; }
              bool            GenerateFastAnd(IR::Instr * instrAnd) { __debugbreak(); return 0; }
              bool            GenerateFastXor(IR::Instr * instrXor) { __debugbreak(); return 0; }
              bool            GenerateFastOr(IR::Instr * instrOr) { __debugbreak(); return 0; }
              bool            GenerateFastNot(IR::Instr * instrNot) { __debugbreak(); return 0; }
              bool            GenerateFastNeg(IR::Instr * instrNeg) { __debugbreak(); return 0; }
              bool            GenerateFastShiftLeft(IR::Instr * instrShift) { __debugbreak(); return 0; }
              bool            GenerateFastShiftRight(IR::Instr * instrShift) { __debugbreak(); return 0; }
              void            GenerateFastBrS(IR::BranchInstr *brInstr) { __debugbreak(); }
              IR::IndirOpnd * GenerateFastElemIStringIndexCommon(IR::Instr * instr, bool isStore, IR::IndirOpnd *indirOpnd, IR::LabelInstr * labelHelper) { __debugbreak(); return 0; }
              void            GenerateFastInlineBuiltInCall(IR::Instr* instr, IR::JnHelperMethod helperMethod) { __debugbreak(); }
              IR::Opnd *      CreateStackArgumentsSlotOpnd() { __debugbreak(); return 0; }
              void            GenerateSmIntTest(IR::Opnd *opndSrc, IR::Instr *insertInstr, IR::LabelInstr *labelHelper, IR::Instr **instrFirst = nullptr, bool fContinueLabel = false) { __debugbreak(); }
              IR::RegOpnd *   LoadNonnegativeIndex(IR::RegOpnd *indexOpnd, const bool skipNegativeCheck, IR::LabelInstr *const notTaggedIntLabel, IR::LabelInstr *const negativeLabel, IR::Instr *const insertBeforeInstr) { __debugbreak(); return nullptr; }
              IR::RegOpnd *   GenerateUntagVar(IR::RegOpnd * opnd, IR::LabelInstr * labelFail, IR::Instr * insertBeforeInstr, bool generateTagCheck = true) { __debugbreak(); return 0; }
              bool            GenerateFastLdMethodFromFlags(IR::Instr * instrLdFld) { __debugbreak(); return 0; }
              void            GenerateInt32ToVarConversion( IR::Opnd * opndSrc, IR::Instr * insertInstr ) { __debugbreak(); }
              IR::Instr *     GenerateFastScopedFld(IR::Instr * instrScopedFld, bool isLoad) { __debugbreak(); return 0; }
              IR::Instr *     GenerateFastScopedLdFld(IR::Instr * instrLdFld) { __debugbreak(); return 0; }
              IR::Instr *     GenerateFastScopedStFld(IR::Instr * instrStFld) { __debugbreak(); return 0; }
              bool            GenerateJSBooleanTest(IR::RegOpnd * regSrc, IR::Instr * insertInstr, IR::LabelInstr * labelTarget, bool fContinueLabel = false) { __debugbreak(); return 0; }
              void            GenerateFastBrBReturn(IR::Instr *instr) { __debugbreak(); }
              bool            TryGenerateFastFloatOp(IR::Instr * instr, IR::Instr ** pInsertHelper, bool *pfNoLower) { __debugbreak(); return 0; }
              bool            GenerateFastFloatCall(IR::Instr * instr, IR::Instr ** pInsertHelper, bool noFieldFastPath, bool *pfNoLower, IR::Instr **pInstrPrev) { __debugbreak(); return 0; }
              bool            GenerateFastFloatBranch(IR::BranchInstr * instr, IR::Instr ** pInsertHelper, bool *pfNoLower) { __debugbreak(); return 0; }
              void            GenerateFastAbs(IR::Opnd *dst, IR::Opnd *src, IR::Instr *callInstr, IR::Instr *insertInstr, IR::LabelInstr *labelHelper, IR::LabelInstr *doneLabel) { __debugbreak(); }
              bool            GenerateFastCharAt(Js::BuiltinFunction index, IR::Opnd *dst, IR::Opnd *srcStr, IR::Opnd *srcIndex, IR::Instr *callInstr, IR::Instr *insertInstr,
                  IR::LabelInstr *labelHelper, IR::LabelInstr *doneLabel) { __debugbreak(); return 0; }
              bool            TryGenerateFastMulAdd(IR::Instr * instrAdd, IR::Instr ** pInstrPrev) { __debugbreak(); return 0; }
              void            GenerateIsDynamicObject(IR::RegOpnd *regOpnd, IR::Instr *insertInstr, IR::LabelInstr *labelHelper, bool fContinueLabel = false) { __debugbreak(); }
              void            GenerateIsRecyclableObject(IR::RegOpnd *regOpnd, IR::Instr *insertInstr, IR::LabelInstr *labelHelper, bool checkObjectAndDynamicObject = true) { __debugbreak(); }
              bool            GenerateLdThisCheck(IR::Instr * instr) { __debugbreak(); return 0; }
              bool            GenerateLdThisStrict(IR::Instr* instr) { __debugbreak(); return 0; }
              void            GenerateFloatTest(IR::RegOpnd * opndSrc, IR::Instr * insertInstr, IR::LabelInstr* labelHelper) { __debugbreak(); }
              void            GenerateFunctionObjectTest(IR::Instr * callInstr, IR::RegOpnd  *functionObjOpnd, bool isHelper, IR::LabelInstr* afterCallLabel = nullptr) { __debugbreak(); }

       static void            EmitInt4Instr(IR::Instr *instr) { __debugbreak(); }
       static void            EmitPtrInstr(IR::Instr *instr) { __debugbreak(); }
              void            EmitLoadVar(IR::Instr *instr, bool isFromUint32 = false, bool isHelper = false) { __debugbreak(); }
              bool            EmitLoadInt32(IR::Instr *instr) { __debugbreak(); return 0; }

       static void            LowerInt4NegWithBailOut(IR::Instr *const instr, const IR::BailOutKind bailOutKind, IR::LabelInstr *const bailOutLabel, IR::LabelInstr *const skipBailOutLabel) { __debugbreak(); }
       static void            LowerInt4AddWithBailOut(IR::Instr *const instr, const IR::BailOutKind bailOutKind, IR::LabelInstr *const bailOutLabel, IR::LabelInstr *const skipBailOutLabel) { __debugbreak(); }
       static void            LowerInt4SubWithBailOut(IR::Instr *const instr, const IR::BailOutKind bailOutKind, IR::LabelInstr *const bailOutLabel, IR::LabelInstr *const skipBailOutLabel) { __debugbreak(); }
       static void            LowerInt4MulWithBailOut(IR::Instr *const instr, const IR::BailOutKind bailOutKind, IR::LabelInstr *const bailOutLabel, IR::LabelInstr *const skipBailOutLabel) { __debugbreak(); }
       static void            LowerInt4RemWithBailOut(IR::Instr *const instr, const IR::BailOutKind bailOutKind, IR::LabelInstr *const bailOutLabel, IR::LabelInstr *const skipBailOutLabel) { __debugbreak(); }
              void            MarkOneFltTmpSym(StackSym *sym, BVSparse<ArenaAllocator> *bvTmps, bool fFltPrefOp) { __debugbreak(); }
              void            GenerateNumberAllocation(IR::RegOpnd * opndDst, IR::Instr * instrInsert, bool isHelper) { __debugbreak(); }
              void            GenerateFastRecyclerAlloc(size_t allocSize, IR::RegOpnd* newObjDst, IR::Instr* insertionPointInstr, IR::LabelInstr* allocHelperLabel, IR::LabelInstr* allocDoneLabel) { __debugbreak(); }
              void            SaveDoubleToVar(IR::RegOpnd * dstOpnd, IR::RegOpnd *opndFloat, IR::Instr *instrOrig, IR::Instr *instrInsert, bool isHelper = false) { __debugbreak(); }
              IR::RegOpnd *   EmitLoadFloat(IR::Opnd *dst, IR::Opnd *src, IR::Instr *insertInstr) { __debugbreak(); return 0; }
              IR::Instr *     LoadCheckedFloat(IR::RegOpnd *opndOrig, IR::RegOpnd *opndFloat, IR::LabelInstr *labelInline, IR::LabelInstr *labelHelper, IR::Instr *instrInsert) { __debugbreak(); return 0; }

              void LoadFloatValue(IR::RegOpnd * javascriptNumber, IR::RegOpnd * opndFloat, IR::LabelInstr * labelHelper, IR::Instr * instrInsert) { __debugbreak(); }

              IR::Instr *     LoadStackAddress(StackSym *sym, IR::RegOpnd* regDst = nullptr) { __debugbreak(); return 0; }
              IR::Instr *     LowerCatch(IR::Instr *instr) { __debugbreak(); return 0; }

              IR::Instr *     LowerGetCachedFunc(IR::Instr *instr) { __debugbreak(); return 0; }
              IR::Instr *     LowerCommitScope(IR::Instr *instr) { __debugbreak(); return 0; }

              IR::Instr *     LowerCallHelper(IR::Instr *instrCall) { __debugbreak(); return 0; }

              IR::LabelInstr *GetBailOutStackRestoreLabel(BailOutInfo * bailOutInfo, IR::LabelInstr * exitTargetInstr) { __debugbreak(); return 0; }
              bool            AnyFloatTmps(void) { __debugbreak(); return 0; }
              IR::LabelInstr* InsertBeforeRecoveryForFloatTemps(IR::Instr * insertBefore, IR::LabelInstr * labelRecover) { __debugbreak(); return 0; }
              StackSym *      GetImplicitParamSlotSym(Js::ArgSlot argSlot) { __debugbreak(); return 0; }
       static StackSym *      GetImplicitParamSlotSym(Js::ArgSlot argSlot, Func * func) { __debugbreak(); return 0; }
              bool            GenerateFastIsInst(IR::Instr * instr, Js::ScriptContext * scriptContext) { __debugbreak(); return 0; }

              IR::Instr *     LowerDivI4AndBailOnReminder(IR::Instr * instr, IR::LabelInstr * bailOutLabel) { __debugbreak(); return NULL; }
              bool            GenerateFastIsInst(IR::Instr * instr) { __debugbreak(); return false; }
  public:
              IR::Instr *         LowerCall(IR::Instr * callInstr, Js::ArgSlot argCount) { __debugbreak(); return 0; }
              IR::Instr *         LowerCallI(IR::Instr * callInstr, ushort callFlags, bool isHelper = false, IR::Instr* insertBeforeInstrForCFG = nullptr) { __debugbreak(); return 0; }
              IR::Instr *         LowerCallPut(IR::Instr * callInstr) { __debugbreak(); return 0; }
              int32               LowerCallArgs(IR::Instr * callInstr, IR::Instr * stackParamInsert, ushort callFlags) { __debugbreak(); return 0; }
              int32               LowerCallArgs(IR::Instr * callInstr, ushort callFlags, Js::ArgSlot extraParams = 1 /* for function object */, IR::IntConstOpnd **callInfoOpndRef = nullptr) { __debugbreak(); return 0; }
              IR::Instr *         LowerStartCall(IR::Instr * instr) { __debugbreak(); return 0; }
              IR::Instr *         LowerAsmJsCallI(IR::Instr * callInstr) { Assert(UNREACHED); return NULL; }
              IR::Instr *         LowerAsmJsCallE(IR::Instr * callInstr) { Assert(UNREACHED); return NULL; }
              IR::Instr *         LowerAsmJsLdElemHelper(IR::Instr * callInstr) { Assert(UNREACHED); return NULL; }
              IR::Instr *         LowerAsmJsStElemHelper(IR::Instr * callInstr) { Assert(UNREACHED); return NULL; }
              IR::Instr *         LowerCallIDynamic(IR::Instr *callInstr, IR::Instr*saveThisArgOutInstr, IR::Opnd *argsLength, ushort callFlags, IR::Instr * insertBeforeInstrForCFG = nullptr) { __debugbreak(); return 0; }
              IR::Instr *         LoadHelperArgument(IR::Instr * instr, IR::Opnd * opndArg) { __debugbreak(); return 0; }
              IR::Instr *         LoadDynamicArgument(IR::Instr * instr, uint argNumber = 1) { __debugbreak(); return 0; }
              IR::Instr *         LoadDynamicArgumentUsingLength(IR::Instr *instr) { __debugbreak(); return 0; }
              IR::Instr *         LoadDoubleHelperArgument(IR::Instr * instr, IR::Opnd * opndArg) { __debugbreak(); return 0; }
              IR::Instr *         LowerToFloat(IR::Instr *instr) { __debugbreak(); return 0; }
       static IR::BranchInstr *   LowerFloatCondBranch(IR::BranchInstr *instrBranch, bool ignoreNaN = false) { __debugbreak(); return 0; }
              void                ConvertFloatToInt32(IR::Opnd* intOpnd, IR::Opnd* floatOpnd, IR::LabelInstr * labelHelper, IR::LabelInstr * labelDone, IR::Instr * instInsert) { __debugbreak(); }
              void                CheckOverflowOnFloatToInt32(IR::Instr* instr, IR::Opnd* intOpnd, IR::LabelInstr * labelHelper, IR::LabelInstr * labelDone) { __debugbreak(); }
              void                EmitLoadVarNoCheck(IR::RegOpnd * dst, IR::RegOpnd * src, IR::Instr *instrLoad, bool isFromUint32, bool isHelper) { __debugbreak(); }
              void                EmitIntToFloat(IR::Opnd *dst, IR::Opnd *src, IR::Instr *instrInsert) { __debugbreak(); }
              void                EmitUIntToFloat(IR::Opnd *dst, IR::Opnd *src, IR::Instr *instrInsert) { __debugbreak(); }
              void                EmitFloatToInt(IR::Opnd *dst, IR::Opnd *src, IR::Instr *instrInsert) { __debugbreak(); }
              void                EmitFloat32ToFloat64(IR::Opnd *dst, IR::Opnd *src, IR::Instr *instrInsert) { __debugbreak(); }
              static IR::Instr *  InsertConvertFloat64ToInt32(const RoundMode roundMode, IR::Opnd *const dst, IR::Opnd *const src, IR::Instr *const insertBeforeInstr) { __debugbreak(); return 0; }
              void                EmitLoadFloatFromNumber(IR::Opnd *dst, IR::Opnd *src, IR::Instr *insertInstr) { __debugbreak(); }
              IR::LabelInstr*     EmitLoadFloatCommon(IR::Opnd *dst, IR::Opnd *src, IR::Instr *insertInstr, bool needHelperLabel) { __debugbreak(); return 0; }
              static IR::Instr *  LoadFloatZero(IR::Opnd * opndDst, IR::Instr * instrInsert) { __debugbreak(); return 0; }
              static IR::Instr *  LoadFloatValue(IR::Opnd * opndDst, double value, IR::Instr * instrInsert) { __debugbreak(); return 0; }

              IR::Instr *         LowerEntryInstr(IR::EntryInstr * entryInstr) { __debugbreak(); return 0; }
              IR::Instr *         LowerExitInstr(IR::ExitInstr * exitInstr) { __debugbreak(); return 0; }
              IR::Instr *         LowerEntryInstrAsmJs(IR::EntryInstr * entryInstr) { Assert(UNREACHED); return NULL; }
              IR::Instr *         LowerExitInstrAsmJs(IR::ExitInstr * exitInstr) { Assert(UNREACHED); return NULL; }
              IR::Instr *         LoadNewScObjFirstArg(IR::Instr * instr, IR::Opnd * dst, ushort extraArgs = 0) { __debugbreak(); return 0; }
              IR::Instr *         LowerTry(IR::Instr *instr, IR::JnHelperMethod helperMethod) { __debugbreak(); return 0; }
              IR::Instr *         LowerLeave(IR::Instr *instr, IR::LabelInstr * targetInstr, bool fromFinalLower, bool isOrphanedLeave = false) { __debugbreak(); return 0; }
              IR::Instr *         LowerLeaveNull(IR::Instr *instr) { __debugbreak(); return 0; }
              IR::LabelInstr *    EnsureEpilogLabel() { __debugbreak(); return 0; }
              IR::Instr *         LowerEHRegionReturn(IR::Instr * retInstr, IR::Opnd * targetOpnd) { __debugbreak(); return 0; }
              void                FinishArgLowering() { __debugbreak(); }
              IR::Opnd *          GetOpndForArgSlot(Js::ArgSlot argSlot, bool isDoubleArgument = false) { __debugbreak(); return 0; }
              void                GenerateStackAllocation(IR::Instr *instr, uint32 allocSize, uint32 probeSize) { __debugbreak(); }
              void                GenerateStackDeallocation(IR::Instr *instr, uint32 allocSize) { __debugbreak(); }
              void                GenerateStackProbe(IR::Instr *instr, bool afterProlog) { __debugbreak(); }
              IR::Opnd*           GenerateArgOutForStackArgs(IR::Instr* callInstr, IR::Instr* stackArgsInstr) { __debugbreak(); return 0; }

              template <bool verify = false>
              static void         Legalize(IR::Instr *const instr, bool fPostRegAlloc = false) { __debugbreak(); }

              IR::Opnd*           IsOpndNegZero(IR::Opnd* opnd, IR::Instr* instr) { __debugbreak(); return 0; }
              void                GenerateFastInlineBuiltInMathAbs(IR::Instr *callInstr) { __debugbreak(); }
              void                GenerateFastInlineBuiltInMathFloor(IR::Instr *callInstr) { __debugbreak(); }
              void                GenerateFastInlineBuiltInMathCeil(IR::Instr *callInstr) { __debugbreak(); }
              void                GenerateFastInlineBuiltInMathRound(IR::Instr *callInstr) { __debugbreak(); }
              static RegNum       GetRegStackPointer() { return RegSP; }
              static RegNum       GetRegFramePointer() { return RegFP; }
              static RegNum       GetRegReturn(IRType type) { return IRType_IsFloat(type) ? RegNOREG : RegX0; }
              static Js::OpCode   GetLoadOp(IRType type) { __debugbreak(); return Js::OpCode::InvalidOpCode; }
              static Js::OpCode   GetStoreOp(IRType type) { __debugbreak(); return Js::OpCode::InvalidOpCode; }
              static Js::OpCode   GetMoveOp(IRType type) { __debugbreak(); return Js::OpCode::InvalidOpCode; }
              static RegNum       GetRegArgI4(int32 argNum) { return RegNOREG; }
              static RegNum       GetRegArgR8(int32 argNum) { return RegNOREG; }

              static BYTE         GetDefaultIndirScale() { return IndirScale4; }

              // -4 is to avoid alignment issues popping up, we are conservative here.
              // We might check for IsSmallStack first to push R4 register & then align.
              static bool         IsSmallStack(uint32 size)   { return (size < (PAGESIZE - 4)); }

              static void GenerateLoadTaggedType(IR::Instr * instrLdSt, IR::RegOpnd * opndType, IR::RegOpnd * opndTaggedType) { __debugbreak(); }
              static void GenerateLoadPolymorphicInlineCacheSlot(IR::Instr * instrLdSt, IR::RegOpnd * opndInlineCache, IR::RegOpnd * opndType, uint polymorphicInlineCacheSize) { __debugbreak(); }
              static IR::BranchInstr * GenerateLocalInlineCacheCheck(IR::Instr * instrLdSt, IR::RegOpnd * opndType, IR::RegOpnd * opndInlineCache, IR::LabelInstr * labelNext, bool checkTypeWithoutProperty = false) { __debugbreak(); return 0; }
              static IR::BranchInstr * GenerateProtoInlineCacheCheck(IR::Instr * instrLdSt, IR::RegOpnd * opndType, IR::RegOpnd * opndInlineCache, IR::LabelInstr * labelNext) { __debugbreak(); return 0; }
              static IR::BranchInstr * GenerateFlagInlineCacheCheck(IR::Instr * instrLdSt, IR::RegOpnd * opndType, IR::RegOpnd * opndInlineCache, IR::LabelInstr * labelNext) { __debugbreak(); return 0; }
              static IR::BranchInstr * GenerateFlagInlineCacheCheckForNoGetterSetter(IR::Instr * instrLdSt, IR::RegOpnd * opndInlineCache, IR::LabelInstr * labelNext) { __debugbreak(); return 0; }
              static IR::BranchInstr * GenerateFlagInlineCacheCheckForLocal(IR::Instr * instrLdSt, IR::RegOpnd * opndType, IR::RegOpnd * opndInlineCache, IR::LabelInstr * labelNext) { __debugbreak(); return 0; }
              static void GenerateLdFldFromLocalInlineCache(IR::Instr * instrLdFld, IR::RegOpnd * opndBase, IR::Opnd * opndDst, IR::RegOpnd * opndInlineCache, IR::LabelInstr * labelFallThru, bool isInlineSlot) { __debugbreak(); }
              static void GenerateLdFldFromProtoInlineCache(IR::Instr * instrLdFld, IR::RegOpnd * opndBase, IR::Opnd * opndDst, IR::RegOpnd * opndInlineCache, IR::LabelInstr * labelFallThru, bool isInlineSlot) { __debugbreak(); }
              static void GenerateLdLocalFldFromFlagInlineCache(IR::Instr * instrLdFld, IR::RegOpnd * opndBase, IR::Opnd * opndDst, IR::RegOpnd * opndInlineCache, IR::LabelInstr * labelFallThru, bool isInlineSlot) { __debugbreak(); }
              static void GenerateStFldFromLocalInlineCache(IR::Instr * instrStFld, IR::RegOpnd * opndBase, IR::Opnd * opndSrc, IR::RegOpnd * opndInlineCache, IR::LabelInstr * labelFallThru, bool isInlineSlot) { __debugbreak(); }

              static void ChangeToWriteBarrierAssign(IR::Instr * assignInstr) { __debugbreak(); }

              int                 GetHelperArgsCount() { __debugbreak(); return 0; }
              void                ResetHelperArgsCount() { __debugbreak(); }

              void                LowerInlineSpreadArgOutLoop(IR::Instr *callInstr, IR::RegOpnd *indexOpnd, IR::RegOpnd *arrayElementsStartOpnd) { __debugbreak(); }

public:
    static void InsertIncUInt8PreventOverflow(IR::Opnd *const dst, IR::Opnd *const src, IR::Instr *const insertBeforeInstr, IR::Instr * *const onOverflowInsertBeforeInstrRef = nullptr) { __debugbreak(); }
    static void InsertDecUInt8PreventOverflow(IR::Opnd *const dst, IR::Opnd *const src, IR::Instr *const insertBeforeInstr, IR::Instr * *const onOverflowInsertBeforeInstrRef = nullptr) { __debugbreak(); }
};
