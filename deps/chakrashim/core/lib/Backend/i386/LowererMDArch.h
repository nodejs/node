//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once

class LowererMD;

///---------------------------------------------------------------------------
///
/// class LowererMDArch
///
/// this contains the lowering code that depends on the x86 architecture.
/// this code not part of LowererMD because for intel we can share much of the
/// code between amd64 and x86. so we splitted the code into LowererMD and
/// LowererMDArch.
///
///---------------------------------------------------------------------------

class LowererMDArch
{
    friend LowererMD;

private:
            Func *              m_func;
            LowererMD *         lowererMD;
            IR::LabelInstr *    bailOutStackRestoreLabel;
            int                 helperCallArgsCount;

public:
            static const int    MaxArgumentsToHelper = 16;

            LowererMDArch(Func* function)
                : m_func(function),
                  bailOutStackRestoreLabel(nullptr)
            {
            }

    static  bool                IsLegalMemLoc(IR::MemRefOpnd *opnd)
            {
                return true;
            }

            void                Init(LowererMD * lowererMD);
            void                FinalLower();
            IR::Instr *         LoadNewScObjFirstArg(IR::Instr * instr, IR::Opnd * dst, ushort extraArgs = 0);

            IR::Instr *         LowerAsmJsCallI(IR::Instr * callInstr);
            IR::Instr *         LowerAsmJsCallE(IR::Instr * callInstr);
            IR::Instr *         LowerAsmJsLdElemHelper(IR::Instr * instr, bool isSimdLoad = false, bool checkEndOffset = false);
            IR::Instr *         LowerAsmJsStElemHelper(IR::Instr * instr, bool isSimdStore = false, bool checkEndOffset = false);

            IR::Instr *         LoadInputParamPtr(IR::Instr *instrInsert, IR::RegOpnd *optionalDstOpnd = nullptr);
            IR::Instr *         LowerCall(IR::Instr * callInstr, uint32 argCount, RegNum regNum = RegNOREG);
            IR::Instr *         LowerCallI(IR::Instr * callInstr, ushort callFlags, bool isHelper = false, IR::Instr * insertBeforeInstrForCFG = nullptr);
            IR::Instr *         LowerCallIDynamic(IR::Instr * callInstr, IR::Instr* saveThis, IR::Opnd* argsLengthOpnd, ushort callFlags, IR::Instr * insertBeforeInstrForCFG = nullptr);
            IR::Instr *         LowerCallPut(IR::Instr * callInstr);
            int32               LowerCallArgs(IR::Instr * callInstr, ushort callFlags, Js::ArgSlot extraArgs = 1 /* for function object */, IR::IntConstOpnd **callInfoOpndRef = nullptr);
            int32               LowerStartCall(IR::Instr * startCallInstr, IR::Instr* insertInstr);
            int32               LowerStartCallAsmJs(IR::Instr * startCallInstr, IR::Instr * insertInstr, IR::Instr * callInstr);
            IR::Instr *         LoadHelperArgument(IR::Instr * instr, IR::Opnd * opndArg);
            IR::Instr *         LoadDynamicArgument(IR::Instr * instr, uint argNumber = 1);
            IR::Instr *         LoadDoubleHelperArgument(IR::Instr * instr, IR::Opnd * opndArg);
            IR::Instr *         LoadStackArgPtr(IR::Instr * instr);
            IR::Instr *         LoadHeapArguments(IR::Instr * instr, bool force = false, IR::Opnd* opndInputParamCount = nullptr);
            IR::Instr *         LoadHeapArgsCached(IR::Instr * instr);
            IR::Instr *         LoadFuncExpression(IR::Instr * instr);
            IR::Instr *         LowerEntryInstr(IR::EntryInstr * entryInstr);
            IR::Instr *         LowerExitInstr(IR::ExitInstr * exitInstr);
            IR::Instr *         LowerEntryInstrAsmJs(IR::EntryInstr * entryInstr);
            IR::Instr *         LowerExitInstrAsmJs(IR::ExitInstr * exitInstr);
            IR::ExitInstr *     LowerExitInstrCommon(IR::ExitInstr * exitInstr);
            void                GeneratePrologueStackProbe(IR::Instr *entryInstr, size_t frameSize);
            static void         EmitInt4Instr(IR::Instr *instr);
            static void         EmitPtrInstr(IR::Instr *instr);
            void                EmitLoadVar(IR::Instr *instrLoad, bool isFromUint32 = false, bool isHelper = false);
            void                EmitIntToFloat(IR::Opnd *dst, IR::Opnd *src, IR::Instr *instrInsert);
            void                EmitUIntToFloat(IR::Opnd *dst, IR::Opnd *src, IR::Instr *instrInsert);
            bool                EmitLoadInt32(IR::Instr *instrLoad);

            IR::Instr *         LoadCheckedFloat(IR::RegOpnd *opndOrig, IR::RegOpnd *opndFloat, IR::LabelInstr *labelInline, IR::LabelInstr *labelHelper, IR::Instr *instrInsert, const bool checkForNullInLoopBody = false);

            static BYTE         GetDefaultIndirScale();
            static RegNum       GetRegShiftCount();
            static RegNum       GetRegReturn(IRType type);
            static RegNum       GetRegReturnAsmJs(IRType type);
            static RegNum       GetRegStackPointer();
            static RegNum       GetRegBlockPointer();
            static RegNum       GetRegFramePointer();
            static RegNum       GetRegChkStkParam();
            static RegNum       GetRegIMulDestLower();
            static RegNum       GetRegIMulHighDestLower();
            static RegNum       GetRegArgI4(int32 argNum);
            static RegNum       GetRegArgR8(int32 argNum);
            static Js::OpCode   GetAssignOp(IRType type);


            bool                GenerateFastAnd(IR::Instr * instrAnd);
            bool                GenerateFastXor(IR::Instr * instrXor);
            bool                GenerateFastOr(IR::Instr * instrOr);
            bool                GenerateFastNot(IR::Instr * instrNot);
            bool                GenerateFastShiftLeft(IR::Instr * instrShift);
            bool                GenerateFastShiftRight(IR::Instr * instrShift);

            IR::LabelInstr *    GetBailOutStackRestoreLabel(BailOutInfo * bailOutInfo, IR::LabelInstr * exitTargetInstr);
            IR::Opnd*           GenerateArgOutForStackArgs(IR::Instr* callInstr, IR::Instr* stackArgsInstr);
            IR::Instr *         LoadDynamicArgumentUsingLength(IR::Instr *instr) { Assert(false); return instr; }
            void                GenerateFunctionObjectTest(IR::Instr * callInstr, IR::RegOpnd  *functionObjOpnd, bool isHelper, IR::LabelInstr* afterCallLabel = nullptr);
            int                 GetHelperArgsCount() { return this->helperCallArgsCount; }
            void                ResetHelperArgsCount() { this->helperCallArgsCount = 0; }


            void                LowerInlineSpreadArgOutLoop(IR::Instr *callInstr, IR::RegOpnd *indexOpnd, IR::RegOpnd *arrayElementsStartOpnd);
            IR::Instr *         LowerEHRegionReturn(IR::Instr * insertBeforeInstr, IR::Opnd * targetOpnd);

private:
            void                GeneratePreCall(IR::Instr * callInstr, IR::Opnd  *functionObjOpnd);
};
