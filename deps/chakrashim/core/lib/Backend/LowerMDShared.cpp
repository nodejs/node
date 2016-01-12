//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "BackEnd.h"
#include "Language\JavascriptFunctionArgIndex.h"

#include "Types\DynamicObjectEnumerator.h"
#include "Types\DynamicObjectSnapshotEnumerator.h"
#include "Types\DynamicObjectSnapshotEnumeratorWPCache.h"
#include "Library\ForInObjectEnumerator.h"

const Js::OpCode LowererMD::MDUncondBranchOpcode = Js::OpCode::JMP;
const Js::OpCode LowererMD::MDTestOpcode = Js::OpCode::TEST;
const Js::OpCode LowererMD::MDOrOpcode = Js::OpCode::OR;
const Js::OpCode LowererMD::MDOverflowBranchOpcode = Js::OpCode::JO;
const Js::OpCode LowererMD::MDNotOverflowBranchOpcode = Js::OpCode::JNO;
const Js::OpCode LowererMD::MDConvertFloat32ToFloat64Opcode = Js::OpCode::CVTSS2SD;
const Js::OpCode LowererMD::MDConvertFloat64ToFloat32Opcode = Js::OpCode::CVTSD2SS;
const Js::OpCode LowererMD::MDCallOpcode = Js::OpCode::CALL;
const Js::OpCode LowererMD::MDImulOpcode = Js::OpCode::IMUL2;

//
// Static utility fn()
//
bool
LowererMD::IsAssign(IR::Instr *instr)
{
    return instr->GetDst() && instr->m_opcode == LowererMDArch::GetAssignOp(instr->GetDst()->GetType());
}

///----------------------------------------------------------------------------
///
/// LowererMD::IsCall
///
///----------------------------------------------------------------------------

bool
LowererMD::IsCall(IR::Instr *instr)
{
    return instr->m_opcode == Js::OpCode::CALL;
}

///----------------------------------------------------------------------------
///
/// LowererMD::IsUnconditionalBranch
///
///----------------------------------------------------------------------------

bool
LowererMD::IsUnconditionalBranch(const IR::Instr *instr)
{
    return (instr->m_opcode == Js::OpCode::JMP);
}

// GenerateMemRef: Return an opnd that can be used to access the given address.
IR::Opnd *
LowererMD::GenerateMemRef(void *addr, IRType type, IR::Instr *instr, bool dontEncode)
{
    return IR::MemRefOpnd::New(addr, type, this->m_func);
}

///----------------------------------------------------------------------------
///
/// LowererMD::InvertBranch
///
///----------------------------------------------------------------------------

void
LowererMD::InvertBranch(IR::BranchInstr *branchInstr)
{
    switch (branchInstr->m_opcode)
    {
    case Js::OpCode::JA:
        branchInstr->m_opcode = Js::OpCode::JBE;
        break;
    case Js::OpCode::JAE:
        branchInstr->m_opcode = Js::OpCode::JB;
        break;
    case Js::OpCode::JB:
        branchInstr->m_opcode = Js::OpCode::JAE;
        break;
    case Js::OpCode::JBE:
        branchInstr->m_opcode = Js::OpCode::JA;
        break;
    case Js::OpCode::JEQ:
        branchInstr->m_opcode = Js::OpCode::JNE;
        break;
    case Js::OpCode::JNE:
        branchInstr->m_opcode = Js::OpCode::JEQ;
        break;
    case Js::OpCode::JGE:
        branchInstr->m_opcode = Js::OpCode::JLT;
        break;
    case Js::OpCode::JGT:
        branchInstr->m_opcode = Js::OpCode::JLE;
        break;
    case Js::OpCode::JLT:
        branchInstr->m_opcode = Js::OpCode::JGE;
        break;
    case Js::OpCode::JLE:
        branchInstr->m_opcode = Js::OpCode::JGT;
        break;
    case Js::OpCode::JO:
        branchInstr->m_opcode = Js::OpCode::JNO;
        break;
    case Js::OpCode::JNO:
        branchInstr->m_opcode = Js::OpCode::JO;
        break;
    case Js::OpCode::JP:
        branchInstr->m_opcode = Js::OpCode::JNP;
        break;
    case Js::OpCode::JNP:
        branchInstr->m_opcode = Js::OpCode::JP;
        break;
    case Js::OpCode::JSB:
        branchInstr->m_opcode = Js::OpCode::JNSB;
        break;
    case Js::OpCode::JNSB:
        branchInstr->m_opcode = Js::OpCode::JSB;
        break;
    default:
        AssertMsg(UNREACHED, "JCC missing in InvertBranch()");
    }
}

void
LowererMD::ReverseBranch(IR::BranchInstr *branchInstr)
{
    switch (branchInstr->m_opcode)
    {
    case Js::OpCode::JA:
        branchInstr->m_opcode = Js::OpCode::JB;
        break;
    case Js::OpCode::JAE:
        branchInstr->m_opcode = Js::OpCode::JBE;
        break;
    case Js::OpCode::JB:
        branchInstr->m_opcode = Js::OpCode::JA;
        break;
    case Js::OpCode::JBE:
        branchInstr->m_opcode = Js::OpCode::JAE;
        break;
    case Js::OpCode::JGE:
        branchInstr->m_opcode = Js::OpCode::JLE;
        break;
    case Js::OpCode::JGT:
        branchInstr->m_opcode = Js::OpCode::JLT;
        break;
    case Js::OpCode::JLT:
        branchInstr->m_opcode = Js::OpCode::JGT;
        break;
    case Js::OpCode::JLE:
        branchInstr->m_opcode = Js::OpCode::JGE;
        break;
    case Js::OpCode::JEQ:
    case Js::OpCode::JNE:
    case Js::OpCode::JO:
    case Js::OpCode::JNO:
    case Js::OpCode::JP:
    case Js::OpCode::JNP:
    case Js::OpCode::JSB:
    case Js::OpCode::JNSB:
        break;
    default:
        AssertMsg(UNREACHED, "JCC missing in ReverseBranch()");
    }
}

IR::Instr *
LowererMD::LowerCallHelper(IR::Instr *instrCall)
{
    IR::Opnd           *argOpnd      = instrCall->UnlinkSrc2();
    IR::Instr          *prevInstr    = nullptr;
    IR::JnHelperMethod  helperMethod = instrCall->GetSrc1()->AsHelperCallOpnd()->m_fnHelper;

    instrCall->FreeSrc1();

#ifndef _M_X64
    prevInstr = ChangeToHelperCall(instrCall, helperMethod);
#endif

    while (argOpnd)
    {
        Assert(argOpnd->IsRegOpnd());
        IR::RegOpnd *regArg = argOpnd->AsRegOpnd();

        Assert(regArg->m_sym->m_isSingleDef);
        IR::Instr *instrArg = regArg->m_sym->m_instrDef;

        Assert(instrArg->m_opcode == Js::OpCode::ArgOut_A);
        prevInstr = LoadHelperArgument(instrArg, instrArg->UnlinkSrc1());

        regArg->Free(this->m_func);
        argOpnd = instrArg->GetSrc2();

        if (argOpnd)
        {
            instrArg->UnlinkSrc2();
        }

        if (prevInstr == instrArg)
        {
            prevInstr = prevInstr->m_prev;
        }

        instrArg->Remove();
    }

    prevInstr = m_lowerer->LoadScriptContext(prevInstr);

#ifdef _M_X64
    FlipHelperCallArgsOrder();
    ChangeToHelperCall(instrCall, helperMethod);
#else
    this->lowererMDArch.ResetHelperArgsCount();
#endif

    // There might be ToVar in between the ArgOut,  need to continue lower from the call still
    return instrCall;
}

//
// forwarding functions
//

IR::Instr *
LowererMD::LowerCall(IR::Instr * callInstr, Js::ArgSlot argCount)
{
    return this->lowererMDArch.LowerCall(callInstr, argCount);
}

IR::Instr *
LowererMD::LowerCallI(IR::Instr * callInstr, ushort callFlags, bool isHelper, IR::Instr * insertBeforeInstrForCFG)
{
    return this->lowererMDArch.LowerCallI(callInstr, callFlags, isHelper, insertBeforeInstrForCFG);
}

IR::Instr *
LowererMD::LowerAsmJsCallI(IR::Instr * callInstr)
{
    return this->lowererMDArch.LowerAsmJsCallI(callInstr);
}

IR::Instr *
LowererMD::LowerAsmJsCallE(IR::Instr * callInstr)
{
    return this->lowererMDArch.LowerAsmJsCallE(callInstr);
}

IR::Instr *
LowererMD::LowerAsmJsLdElemHelper(IR::Instr * callInstr)
{
    return this->lowererMDArch.LowerAsmJsLdElemHelper(callInstr);
}
IR::Instr *
LowererMD::LowerAsmJsStElemHelper(IR::Instr * callInstr)
{
    return this->lowererMDArch.LowerAsmJsStElemHelper(callInstr);
}
IR::Instr *
LowererMD::LowerCallPut(IR::Instr * callInstr)
{
    int32 argCount = this->lowererMDArch.LowerCallArgs(callInstr, Js::CallFlags_None, 2);

    //  load native entry point from script function into eax

    IR::Opnd * functionWrapOpnd = callInstr->UnlinkSrc1();
    AssertMsg(functionWrapOpnd->IsRegOpnd() && functionWrapOpnd->AsRegOpnd()->m_sym->IsStackSym(),
              "Expected call src to be stackSym");

    this->LoadHelperArgument(callInstr, functionWrapOpnd);
    this->m_lowerer->LoadScriptContext(callInstr);

    IR::HelperCallOpnd  *helperCallOpnd = IR::HelperCallOpnd::New(IR::HelperOp_InvokePut, this->m_func);
    callInstr->SetSrc1(helperCallOpnd);

    return this->lowererMDArch.LowerCall(callInstr, argCount);
}

IR::Instr *
LowererMD::LoadHelperArgument(IR::Instr * instr, IR::Opnd * opndArg)
{
    return this->lowererMDArch.LoadHelperArgument(instr, opndArg);
}

IR::Instr *
LowererMD::LoadDoubleHelperArgument(IR::Instr * instr, IR::Opnd * opndArg)
{
    return this->lowererMDArch.LoadDoubleHelperArgument(instr, opndArg);
}

IR::Instr *
LowererMD::LowerEntryInstr(IR::EntryInstr * entryInstr)
{
    return this->lowererMDArch.LowerEntryInstr(entryInstr);
}

IR::Instr *
LowererMD::LowerExitInstr(IR::ExitInstr * exitInstr)
{
    return this->lowererMDArch.LowerExitInstr(exitInstr);
}

IR::Instr *
LowererMD::LowerEntryInstrAsmJs(IR::EntryInstr * entryInstr)
{
    return this->lowererMDArch.LowerEntryInstrAsmJs(entryInstr);
}

IR::Instr *
LowererMD::LowerExitInstrAsmJs(IR::ExitInstr * exitInstr)
{
    return this->lowererMDArch.LowerExitInstrAsmJs(exitInstr);
}

IR::Instr *
LowererMD::LoadNewScObjFirstArg(IR::Instr * instr, IR::Opnd * dst, ushort extraArgs)
{
    return this->lowererMDArch.LoadNewScObjFirstArg(instr, dst, extraArgs);
}

IR::Instr *
LowererMD::LowerTry(IR::Instr *tryInstr, IR::JnHelperMethod helperMethod)
{
    // Mark the entry to the try
    IR::Instr *instr = tryInstr->GetNextRealInstrOrLabel();
    AssertMsg(instr->IsLabelInstr(), "No label at the entry to a try?");
    IR::LabelInstr *tryAddr = instr->AsLabelInstr();

    // Arg 5: ScriptContext
    this->m_lowerer->LoadScriptContext(tryAddr);

    if (tryInstr->m_opcode == Js::OpCode::TryCatch)
    {
        // Arg 4 : hasBailedOutOffset
        IR::Opnd * hasBailedOutOffset = IR::IntConstOpnd::New(this->m_func->m_hasBailedOutSym->m_offset, TyInt32, this->m_func);
        this->LoadHelperArgument(tryAddr, hasBailedOutOffset);
    }

#ifdef _M_X64
    // Arg: args size
    IR::RegOpnd *argsSizeOpnd = IR::RegOpnd::New(TyMachReg, m_func);
    tryAddr->InsertBefore(IR::Instr::New(Js::OpCode::LdArgSize, argsSizeOpnd, this->m_func));
    this->LoadHelperArgument(tryAddr, argsSizeOpnd);

    // Arg: spill size
    IR::RegOpnd *spillSizeOpnd = IR::RegOpnd::New(TyMachReg, m_func);
    tryAddr->InsertBefore(IR::Instr::New(Js::OpCode::LdSpillSize, spillSizeOpnd, this->m_func));
    this->LoadHelperArgument(tryAddr, spillSizeOpnd);
#endif

    // Arg 3: frame pointer
    IR::RegOpnd *ebpOpnd = IR::RegOpnd::New(nullptr, lowererMDArch.GetRegBlockPointer(), TyMachReg, this->m_func);
    this->LoadHelperArgument(tryAddr, ebpOpnd);

    // Arg 2: handler address
    IR::LabelInstr *helperAddr = tryInstr->AsBranchInstr()->GetTarget();
    this->LoadHelperArgument(tryAddr, IR::LabelOpnd::New(helperAddr, this->m_func));

    // Arg 1: try address
    this->LoadHelperArgument(tryAddr, IR::LabelOpnd::New(tryAddr, this->m_func));

    // Call the helper
    IR::RegOpnd *continuationAddr =
        IR::RegOpnd::New(StackSym::New(TyMachReg, this->m_func), lowererMDArch.GetRegReturn(TyMachReg), TyMachReg, this->m_func);
    IR::Instr *callInstr = IR::Instr::New(
        Js::OpCode::Call, continuationAddr, IR::HelperCallOpnd::New(helperMethod, this->m_func), this->m_func);
    tryAddr->InsertBefore(callInstr);
    this->LowerCall(callInstr, 0);

#ifdef _M_X64
    {
        // Emit some instruction to separate the CALL from the JMP following it. The OS stack unwinder
        // mistakes the JMP for the start of the epilog otherwise.
        IR::Instr *nop = IR::Instr::New(Js::OpCode::NOP, m_func);
        tryAddr->InsertBefore(nop);
    }
#endif

    // Jump to the continuation address supplied by the helper
    IR::BranchInstr *branchInstr = IR::MultiBranchInstr::New(Js::OpCode::JMP, continuationAddr, this->m_func);
    tryAddr->InsertBefore(branchInstr);

    return tryInstr->m_prev;
}

IR::Instr *
LowererMD::LowerLeave(IR::Instr *leaveInstr, IR::LabelInstr *targetInstr, bool fromFinalLower, bool isOrphanedLeave)
{
    if (isOrphanedLeave)
    {
        Assert(this->m_func->IsLoopBodyInTry());
        leaveInstr->m_opcode = Js::OpCode::JMP;
        return leaveInstr->m_prev;
    }

    IR::Instr   *instrPrev = leaveInstr->m_prev;
    IR::LabelOpnd *labelOpnd = IR::LabelOpnd::New(targetInstr, this->m_func);

    lowererMDArch.LowerEHRegionReturn(leaveInstr, labelOpnd);

    if (fromFinalLower)
    {
        instrPrev = leaveInstr->m_prev; // Need to lower LdArgSize and LdSpillSize
    }
    leaveInstr->Remove();

    return instrPrev;
}

IR::Instr *
LowererMD::LowerEHRegionReturn(IR::Instr * insertBeforeInstr, IR::Opnd * targetOpnd)
{
    return lowererMDArch.LowerEHRegionReturn(insertBeforeInstr, targetOpnd);
}

IR::Instr *
LowererMD::LowerLeaveNull(IR::Instr *finallyEndInstr)
{
    IR::Instr *instrPrev = finallyEndInstr->m_prev;
    IR::Instr *instr     = nullptr;

    // Return a null continuation address to the helper: execution will resume at the point determined by the try
    // or the exception handler.
    IR::RegOpnd *retReg = IR::RegOpnd::New(StackSym::New(TyMachReg,this->m_func), lowererMDArch.GetRegReturn(TyMachReg), TyMachReg, this->m_func);
    instr = IR::Instr::New(Js::OpCode::XOR, retReg, this->m_func);
    IR::RegOpnd *eaxOpnd = IR::RegOpnd::New(nullptr, lowererMDArch.GetRegReturn(TyMachReg), TyMachReg, this->m_func);
    instr->SetSrc1(eaxOpnd);
    instr->SetSrc2(eaxOpnd);
    finallyEndInstr->InsertBefore(instr);

#if _M_X64
    {
        // amd64_ReturnFromCallWithFakeFrame expects to find the spill size and args size
        // in r8 and r9.

        // MOV r8, spillSize
        IR::Instr *movR8 = IR::Instr::New(Js::OpCode::LdSpillSize,
                                          IR::RegOpnd::New(nullptr, RegR8, TyMachReg, m_func),
                                          m_func);
        finallyEndInstr->InsertBefore(movR8);


        // MOV r9, argsSize
        IR::Instr *movR9 = IR::Instr::New(Js::OpCode::LdArgSize,
                                          IR::RegOpnd::New(nullptr, RegR9, TyMachReg, m_func),
                                          m_func);
        finallyEndInstr->InsertBefore(movR9);

        IR::Opnd *targetOpnd = IR::RegOpnd::New(nullptr, RegRCX, TyMachReg, m_func);
        IR::Instr *movTarget = IR::Instr::New(Js::OpCode::MOV,
            targetOpnd,
            IR::HelperCallOpnd::New(IR::HelperOp_ReturnFromCallWithFakeFrame, m_func),
            m_func);
        finallyEndInstr->InsertBefore(movTarget);

        IR::Instr *push = IR::Instr::New(Js::OpCode::PUSH, m_func);
        push->SetSrc1(targetOpnd);
        finallyEndInstr->InsertBefore(push);
    }
#endif

    IR::IntConstOpnd *intSrc = IR::IntConstOpnd::New(0, TyInt32, this->m_func);
    instr = IR::Instr::New(Js::OpCode::RET, this->m_func);
    instr->SetSrc1(intSrc);
    instr->SetSrc2(retReg);
    finallyEndInstr->InsertBefore(instr);
    finallyEndInstr->Remove();

    return instrPrev;
}

///----------------------------------------------------------------------------
///
/// LowererMD::Init
///
///----------------------------------------------------------------------------

void
LowererMD::Init(Lowerer *lowerer)
{
    m_lowerer = lowerer;
    this->lowererMDArch.Init(this);
    Simd128InitOpcodeMap();
}

///----------------------------------------------------------------------------
///
/// LowererMD::LoadInputParamCount
///
///     Load the passed-in parameter count from the appropriate EBP slot.
///
///----------------------------------------------------------------------------

IR::Instr *
LowererMD::LoadInputParamCount(IR::Instr * instrInsert, int adjust, bool needFlags)
{
    IR::Instr *   instr;
    IR::RegOpnd * dstOpnd;
    IR::SymOpnd * srcOpnd;

    srcOpnd = Lowerer::LoadCallInfo(instrInsert);
    dstOpnd = IR::RegOpnd::New(StackSym::New(TyMachReg, this->m_func), TyMachReg, this->m_func);
    instr = IR::Instr::New(Js::OpCode::MOV, dstOpnd, srcOpnd, this->m_func);
    instrInsert->InsertBefore(instr);

    // Copy the callinfo before masking off the param count

    Assert(Js::CallInfo::ksizeofCount == 24);

    // Mask off call flags from callinfo
    instr = IR::Instr::New(Js::OpCode::AND, dstOpnd, dstOpnd,
        IR::IntConstOpnd::New((Js::CallFlags_ExtraArg << Js::CallInfo::ksizeofCount) | 0x00FFFFFF, TyUint32, this->m_func, true), this->m_func);
    instrInsert->InsertBefore(instr);

    // Shift and mask the "calling eval" bit and subtract it from the incoming count.
    // ("Calling eval" means the last param is the frame display, which only the eval built-in should see.)
    instr = IR::Instr::New(Js::OpCode::BTR, dstOpnd, dstOpnd, IR::IntConstOpnd::New(Math::Log2(Js::CallFlags_ExtraArg) + Js::CallInfo::ksizeofCount, TyInt8, this->m_func), this->m_func);
    instrInsert->InsertBefore(instr);

    instr = IR::Instr::New(Js::OpCode::SBB, dstOpnd, dstOpnd, IR::IntConstOpnd::New(-adjust, TyInt32, this->m_func), this->m_func);
    instrInsert->InsertBefore(instr);

    return instr;
}

IR::Instr *
LowererMD::LoadStackArgPtr(IR::Instr * instr)
{
    if (this->m_func->IsLoopBody())
    {
        // Get the first user param from the interpreter frame instance that was passed in.
        // These args don't include the func object and callinfo; we just need to advance past "this".

        // t1 = MOV [prm1 + m_inParams]
        // dst = LEA &[t1 + sizeof(var)]

        Assert(this->m_func->m_loopParamSym);
        IR::RegOpnd *baseOpnd = IR::RegOpnd::New(this->m_func->m_loopParamSym, TyMachReg, this->m_func);
        size_t offset = Js::InterpreterStackFrame::GetOffsetOfInParams();
        IR::IndirOpnd *indirOpnd = IR::IndirOpnd::New(baseOpnd, (int32)offset, TyMachReg, this->m_func);
        IR::RegOpnd *tmpOpnd = IR::RegOpnd::New(TyMachReg, this->m_func);
        IR::Instr *instrLdParams = IR::Instr::New(Js::OpCode::MOV, tmpOpnd, indirOpnd, this->m_func);
        instr->InsertBefore(instrLdParams);

        indirOpnd = IR::IndirOpnd::New(tmpOpnd, sizeof(Js::Var), TyMachReg, this->m_func);
        instr->SetSrc1(indirOpnd);
        instr->m_opcode = Js::OpCode::LEA;

        return instr->m_prev;
    }
    else
    {
        return this->lowererMDArch.LoadStackArgPtr(instr);
    }
}

IR::Instr *
LowererMD::LoadArgumentsFromFrame(IR::Instr * instr)
{
    if (this->m_func->IsLoopBody())
    {
        // Get the arguments ptr from the interpreter frame instance that was passed in.
        Assert(this->m_func->m_loopParamSym);
        IR::RegOpnd *baseOpnd = IR::RegOpnd::New(this->m_func->m_loopParamSym, TyMachReg, this->m_func);
        int32 offset = (int32)Js::InterpreterStackFrame::GetOffsetOfArguments();
        instr->SetSrc1(IR::IndirOpnd::New(baseOpnd, offset, TyMachReg, this->m_func));
    }
    else
    {
        instr->SetSrc1(this->CreateStackArgumentsSlotOpnd());
    }

    instr->m_opcode = Js::OpCode::MOV;

    return instr->m_prev;
}

// load argument count as I4
IR::Instr *
LowererMD::LoadArgumentCount(IR::Instr * instr)
{
    if (this->m_func->IsLoopBody())
    {
        // Pull the arg count from the interpreter frame instance that was passed in.
        // (The callinfo in the loop body's frame just shows the single parameter, the interpreter frame.)
        Assert(this->m_func->m_loopParamSym);
        IR::RegOpnd *baseOpnd = IR::RegOpnd::New(this->m_func->m_loopParamSym, TyMachReg, this->m_func);
        size_t offset = Js::InterpreterStackFrame::GetOffsetOfInSlotsCount();
        instr->SetSrc1(IR::IndirOpnd::New(baseOpnd, (int32)offset, TyInt32, this->m_func));
    }
    else
    {
        StackSym *sym = StackSym::New(TyVar, this->m_func);
                this->m_func->SetArgOffset(sym, (Js::JavascriptFunctionArgIndex_CallInfo - Js::JavascriptFunctionArgIndex_Frame) * sizeof(Js::Var));
        instr->SetSrc1(IR::SymOpnd::New(sym, TyMachReg,  this->m_func));
    }

    instr->m_opcode = Js::OpCode::MOV;

    return instr->m_prev;
}

IR::Instr *
LowererMD::LoadHeapArguments(IR::Instr * instrArgs, bool force, IR::Opnd* opndInputParamCount)
{
    return this->lowererMDArch.LoadHeapArguments(instrArgs, force, opndInputParamCount);
}

IR::Instr *
LowererMD::LoadHeapArgsCached(IR::Instr * instrArgs)
{
    return this->lowererMDArch.LoadHeapArgsCached(instrArgs);
}

IR::Instr *
LowererMD::LoadFuncExpression(IR::Instr * instrFuncExpr)
{
    return this->lowererMDArch.LoadFuncExpression(instrFuncExpr);
}

///----------------------------------------------------------------------------
///
/// LowererMD::ChangeToHelperCall
///
///     Change the current instruction to a call to the given helper.
///
///----------------------------------------------------------------------------

IR::Instr *
LowererMD::ChangeToHelperCall(IR::Instr * callInstr,  IR::JnHelperMethod helperMethod, IR::LabelInstr *labelBailOut,
                              IR::Opnd *opndBailOutArg, IR::PropertySymOpnd *propSymOpnd, bool isHelperContinuation)
{
    IR::Instr * bailOutInstr = callInstr;
    if (callInstr->HasBailOutInfo())
    {
        if (callInstr->GetBailOutKind() == IR::BailOutExpectingObject)
        {
            callInstr = IR::Instr::New(callInstr->m_opcode, callInstr->m_func);
            bailOutInstr->TransferTo(callInstr);
            bailOutInstr->InsertBefore(callInstr);

            bailOutInstr->m_opcode = Js::OpCode::BailOnNotObject;
            bailOutInstr->SetSrc1(opndBailOutArg);
        }
        else
        {
            bailOutInstr = this->m_lowerer->SplitBailOnImplicitCall(callInstr);
        }
    }

    callInstr->m_opcode = Js::OpCode::CALL;

    IR::HelperCallOpnd *helperCallOpnd = Lowerer::CreateHelperCallOpnd(helperMethod, this->lowererMDArch.GetHelperArgsCount(), m_func);
    if (helperCallOpnd->IsDiagHelperCallOpnd())
    {
        // Load arguments for the wrapper.
        this->LoadHelperArgument(callInstr, IR::AddrOpnd::New((Js::Var)IR::GetMethodOriginalAddress(helperMethod), IR::AddrOpndKindDynamicMisc, m_func));
        this->m_lowerer->LoadScriptContext(callInstr);
    }
    callInstr->SetSrc1(helperCallOpnd);

    IR::Instr * instrRet = this->lowererMDArch.LowerCall(callInstr, 0);

    if (bailOutInstr != callInstr)
    {
        // The bailout needs to be lowered after we lower the helper call because the helper argument
        // has already been loaded.  We need to drain them on AMD64 before starting another helper call
        if (bailOutInstr->m_opcode == Js::OpCode::BailOnNotObject)
        {
            this->m_lowerer->LowerBailOnNotObject(bailOutInstr, nullptr, labelBailOut);
        }
        else if (bailOutInstr->m_opcode == Js::OpCode::BailOut)
        {
            this->m_lowerer->GenerateBailOut(bailOutInstr, nullptr, labelBailOut);
        }
        else
        {
            this->m_lowerer->LowerBailOnEqualOrNotEqual(bailOutInstr, nullptr, labelBailOut, propSymOpnd, isHelperContinuation);
        }
    }

    return instrRet;
}

IR::Instr* LowererMD::ChangeToHelperCallMem(IR::Instr * instr,  IR::JnHelperMethod helperMethod)
{
    this->m_lowerer->LoadScriptContext(instr);

    return this->ChangeToHelperCall(instr, helperMethod);
}

///----------------------------------------------------------------------------
///
/// LowererMD::ChangeToAssign
///
///     Change to a MOV.
///
///----------------------------------------------------------------------------

IR::Instr *
LowererMD::ChangeToAssign(IR::Instr * instr)
{
    return ChangeToAssign(instr, instr->GetDst()->GetType());
}

IR::Instr *
LowererMD::ChangeToAssign(IR::Instr * instr, IRType type)
{
    Assert(!instr->HasBailOutInfo() || instr->GetBailOutKind() == IR::BailOutExpectingString);

    instr->m_opcode = LowererMDArch::GetAssignOp(type);
    Legalize(instr);

    return instr;
}

///----------------------------------------------------------------------------
///
/// LowererMD::ChangeToLea
///
///     Change to an LEA.
///
///----------------------------------------------------------------------------

IR::Instr *
LowererMD::ChangeToLea(IR::Instr * instr)
{
    Assert(instr);
    Assert(instr->GetDst());
    Assert(instr->GetDst()->IsRegOpnd());
    Assert(instr->GetSrc1());
    Assert(instr->GetSrc1()->IsIndirOpnd() || instr->GetSrc1()->IsSymOpnd());
    Assert(!instr->GetSrc2());

    instr->m_opcode = Js::OpCode::LEA;
    return instr;
}

///----------------------------------------------------------------------------
///
/// LowererMD::CreateAssign
///
///     Create a MOV.
///
///----------------------------------------------------------------------------

IR::Instr *
LowererMD::CreateAssign(IR::Opnd *dst, IR::Opnd *src, IR::Instr *instrInsertPt)
{
    return Lowerer::InsertMove(dst, src, instrInsertPt);
}

///----------------------------------------------------------------------------
///
/// LowererMD::LowerRet
///
///     Lower Ret to "MOV EAX, src"
///     The real RET is inserted at the exit of the function when emitting the
///     epilog.
///
///----------------------------------------------------------------------------

IR::Instr *
LowererMD::LowerRet(IR::Instr * retInstr)
{
    IR::RegOpnd * retReg;

    if (m_func->GetJnFunction()->GetIsAsmjsMode() && !m_func->IsLoopBody()) // for loop body ret is the bytecodeoffset
    {
        Js::AsmJsRetType asmType = m_func->GetJnFunction()->GetAsmJsFunctionInfo()->GetReturnType();
        IRType regType = TyInt32;
        if (asmType.which() == Js::AsmJsRetType::Double)
        {
            regType = TyFloat64;
        }
        else if (asmType.which() == Js::AsmJsRetType::Float)
        {
            regType = TyFloat32;
        }
        else if (asmType.which() == Js::AsmJsRetType::Signed || asmType.which() == Js::AsmJsRetType::Void)
        {

            regType = TyInt32;
        }
        else if (asmType.which() == Js::AsmJsRetType::Float32x4)
        {
            regType = TySimd128F4;
        }
        else if (asmType.which() == Js::AsmJsRetType::Int32x4)
        {
            regType = TySimd128I4;
        }
        else if (asmType.which() == Js::AsmJsRetType::Float64x2)
        {
            regType = TySimd128D2;
        }
        else
        {
            Assert(UNREACHED);
        }

        retReg = IR::RegOpnd::New(nullptr, lowererMDArch.GetRegReturnAsmJs(regType), regType, m_func);
    }
    else
    {
        retReg = IR::RegOpnd::New(nullptr, lowererMDArch.GetRegReturn(TyMachReg), TyMachReg, m_func);
    }





    retInstr->SetDst(retReg);

    return this->ChangeToAssign(retInstr);
}


///----------------------------------------------------------------------------
///
/// LowererMD::LowerUncondBranch
///
///----------------------------------------------------------------------------

IR::Instr *
LowererMD::LowerUncondBranch(IR::Instr * instr)
{
    instr->m_opcode = Js::OpCode::JMP;

    return instr;
}

///----------------------------------------------------------------------------
///
/// LowererMD::LowerMultiBranch
///
///----------------------------------------------------------------------------

IR::Instr *
LowererMD::LowerMultiBranch(IR::Instr * instr)
{
    return LowerUncondBranch(instr);
}

///----------------------------------------------------------------------------
///
/// LowererMD::LowerUncondBranch
///
///----------------------------------------------------------------------------

IR::Instr *
LowererMD::LowerCondBranch(IR::Instr * instr)
{
    AssertMsg(instr->GetSrc1() != nullptr, "Expected src opnds on conditional branch");
    Assert(!instr->HasBailOutInfo());
    IR::Opnd *  opndSrc1 = instr->UnlinkSrc1();
    IR::Instr * instrPrev = nullptr;

    switch (instr->m_opcode)
    {
    case Js::OpCode::BrTrue_A:
    case Js::OpCode::BrFalse_A:
    case Js::OpCode::BrNotNull_A:
    case Js::OpCode::BrOnObject_A:
    case Js::OpCode::BrOnClassConstructor:
        Assert(!opndSrc1->IsFloat64());
        AssertMsg(instr->GetSrc2() == nullptr, "Expected 1 src on boolean branch");
        instrPrev = IR::Instr::New(Js::OpCode::TEST, this->m_func);
        instrPrev->SetSrc1(opndSrc1);
        instrPrev->SetSrc2(opndSrc1);
        instr->InsertBefore(instrPrev);

        if (instr->m_opcode != Js::OpCode::BrFalse_A)
        {
            instr->m_opcode = Js::OpCode::JNE;
        }
        else
        {
            instr->m_opcode = Js::OpCode::JEQ;
        }

        break;

    case Js::OpCode::BrOnEmpty:
    case Js::OpCode::BrOnNotEmpty:
        AssertMsg(0, "BrOnEmpty opcodes should not be passed to MD lowerer");
        break;

    default:
        IR::Opnd *  opndSrc2 = instr->UnlinkSrc2();
        AssertMsg(opndSrc2 != nullptr, "Expected 2 src's on non-boolean branch");

        if (opndSrc1->IsFloat())
        {
            Assert(opndSrc1->GetType() == opndSrc2->GetType());
            instrPrev = IR::Instr::New(opndSrc1->IsFloat64() ? Js::OpCode::COMISD : Js::OpCode::COMISS, m_func);
            instrPrev->SetSrc1(opndSrc1);
            instrPrev->SetSrc2(opndSrc2);
            instr->InsertBefore(instrPrev);
        }
        else
        {
            // This check assumes src1 is a variable.
            if (opndSrc2->IsIntConstOpnd() && opndSrc2->AsIntConstOpnd()->GetValue() == 0)
            {
                instrPrev = IR::Instr::New(Js::OpCode::TEST, this->m_func);
                instrPrev->SetSrc1(opndSrc1);
                instrPrev->SetSrc2(opndSrc1);
                instr->InsertBefore(instrPrev);
                opndSrc2->Free(this->m_func);
            }
            else
            {
                instrPrev = IR::Instr::New(Js::OpCode::CMP, this->m_func);

                //
                // For 32 bit arithmetic we copy them and set the size of operands to be 32 bits. This is
                // relevant only on AMD64.
                //

                opndSrc1  = instrPrev->SetSrc1(opndSrc1);
                opndSrc2  = instrPrev->SetSrc2(opndSrc2);
                instr->InsertBefore(instrPrev);
                LowererMD::Legalize(instrPrev);
            }
        }
        instr->m_opcode = LowererMD::MDBranchOpcode(instr->m_opcode);
        break;
    }

    return instrPrev;
}

///----------------------------------------------------------------------------
///
/// LowererMD::MDBranchOpcode
///
///     Map HIR branch opcode to machine-dependent equivalent.
///
///----------------------------------------------------------------------------

Js::OpCode
LowererMD::MDBranchOpcode(Js::OpCode opcode)
{
    switch (opcode)
    {
    case Js::OpCode::BrSrEq_A:
    case Js::OpCode::BrEq_A:
    case Js::OpCode::BrSrNotNeq_A:
    case Js::OpCode::BrNotNeq_A:
    case Js::OpCode::BrAddr_A:
        return Js::OpCode::JEQ;

    case Js::OpCode::BrSrNeq_A:
    case Js::OpCode::BrNeq_A:
    case Js::OpCode::BrSrNotEq_A:
    case Js::OpCode::BrNotEq_A:
    case Js::OpCode::BrNotAddr_A:
        return Js::OpCode::JNE;

    case Js::OpCode::BrLt_A:
    case Js::OpCode::BrNotGe_A:
        return Js::OpCode::JLT;

    case Js::OpCode::BrLe_A:
    case Js::OpCode::BrNotGt_A:
        return Js::OpCode::JLE;

    case Js::OpCode::BrGt_A:
    case Js::OpCode::BrNotLe_A:
        return Js::OpCode::JGT;

    case Js::OpCode::BrGe_A:
    case Js::OpCode::BrNotLt_A:
        return Js::OpCode::JGE;

    default:
        AssertMsg(0, "Branch opcode has no MD mapping");
        return opcode;
    }
}

Js::OpCode
LowererMD::MDConvertFloat64ToInt32Opcode(const RoundMode roundMode)
{
    switch (roundMode)
    {
    case RoundModeTowardZero:
        return Js::OpCode::CVTTSD2SI;
    case RoundModeTowardInteger:
        return Js::OpCode::Nop;
    case RoundModeHalfToEven:
        return Js::OpCode::CVTSD2SI;
    default:
        AssertMsg(0, "RoundMode has no MD mapping.");
        return Js::OpCode::Nop;
    }
}

Js::OpCode
LowererMD::MDUnsignedBranchOpcode(Js::OpCode opcode)
{
    switch (opcode)
    {
    case Js::OpCode::BrEq_A:
    case Js::OpCode::BrSrEq_A:
    case Js::OpCode::BrSrNotNeq_A:
    case Js::OpCode::BrNotNeq_A:
    case Js::OpCode::BrAddr_A:
        return Js::OpCode::JEQ;

    case Js::OpCode::BrNeq_A:
    case Js::OpCode::BrSrNeq_A:
    case Js::OpCode::BrSrNotEq_A:
    case Js::OpCode::BrNotEq_A:
    case Js::OpCode::BrNotAddr_A:
        return Js::OpCode::JNE;

    case Js::OpCode::BrLt_A:
    case Js::OpCode::BrNotGe_A:
        return Js::OpCode::JB;

    case Js::OpCode::BrLe_A:
    case Js::OpCode::BrNotGt_A:
        return Js::OpCode::JBE;

    case Js::OpCode::BrGt_A:
    case Js::OpCode::BrNotLe_A:
        return Js::OpCode::JA;

    case Js::OpCode::BrGe_A:
    case Js::OpCode::BrNotLt_A:
        return Js::OpCode::JAE;

    default:
        AssertMsg(0, "Branch opcode has no MD mapping");
        return opcode;
    }
}

Js::OpCode LowererMD::MDCompareWithZeroBranchOpcode(Js::OpCode opcode)
{
    Assert(opcode == Js::OpCode::BrLt_A || opcode == Js::OpCode::BrGe_A);
    return opcode == Js::OpCode::BrLt_A ? Js::OpCode::JSB : Js::OpCode::JNSB;
}

void LowererMD::ChangeToAdd(IR::Instr *const instr, const bool needFlags)
{
    Assert(instr);
    Assert(instr->GetDst());
    Assert(instr->GetSrc1());
    Assert(instr->GetSrc2());

    if(instr->GetDst()->IsFloat64())
    {
        Assert(instr->GetSrc1()->IsFloat64());
        Assert(instr->GetSrc2()->IsFloat64());
        Assert(!needFlags);
        instr->m_opcode = Js::OpCode::ADDSD;
        return;
    }
    else if (instr->GetDst()->IsFloat32())
    {
        Assert(instr->GetSrc1()->IsFloat32());
        Assert(instr->GetSrc2()->IsFloat32());
        Assert(!needFlags);
        instr->m_opcode = Js::OpCode::ADDSS;
        return;
    }

    instr->m_opcode = Js::OpCode::ADD;
    MakeDstEquSrc1(instr);

    // Prefer INC for add by one
    if(instr->GetDst()->IsEqual(instr->GetSrc1()) &&
            instr->GetSrc2()->IsIntConstOpnd() &&
            instr->GetSrc2()->AsIntConstOpnd()->GetValue() == 1 ||
        instr->GetDst()->IsEqual(instr->GetSrc2()) &&
            instr->GetSrc1()->IsIntConstOpnd() &&
            instr->GetSrc1()->AsIntConstOpnd()->GetValue() == 1)
    {
        if(instr->GetSrc1()->IsIntConstOpnd())
        {
            // Swap the operands, such that we would create (dst = INC src2)
            instr->SwapOpnds();
        }

        instr->FreeSrc2();
        instr->m_opcode = Js::OpCode::INC;
    }
}

void LowererMD::ChangeToSub(IR::Instr *const instr, const bool needFlags)
{
    Assert(instr);
    Assert(instr->GetDst());
    Assert(instr->GetSrc1());
    Assert(instr->GetSrc2());

    if(instr->GetDst()->IsFloat64())
    {
        Assert(instr->GetSrc1()->IsFloat64());
        Assert(instr->GetSrc2()->IsFloat64());
        Assert(!needFlags);
        instr->m_opcode = Js::OpCode::SUBSD;
        return;
    }

    // Prefer DEC for sub by one
    if(instr->GetDst()->IsEqual(instr->GetSrc1()) &&
        instr->GetSrc2()->IsIntConstOpnd() &&
        instr->GetSrc2()->AsIntConstOpnd()->GetValue() == 1)
    {
        instr->FreeSrc2();
        instr->m_opcode = Js::OpCode::DEC;
        return;
    }

    instr->m_opcode = Js::OpCode::SUB;
}

void LowererMD::ChangeToShift(IR::Instr *const instr, const bool needFlags)
{
    Assert(instr);
    Assert(instr->GetDst());
    Assert(instr->GetSrc1());
    Assert(instr->GetSrc2());

    switch(instr->m_opcode)
    {
        case Js::OpCode::Shl_A:
        case Js::OpCode::Shl_I4:
            instr->m_opcode = Js::OpCode::SHL;
            break;

        case Js::OpCode::Shr_A:
        case Js::OpCode::Shr_I4:
            instr->m_opcode = Js::OpCode::SAR;
            break;

        case Js::OpCode::ShrU_A:
        case Js::OpCode::ShrU_I4:
            instr->m_opcode = Js::OpCode::SHR;
            break;

        default:
            Assert(false);
            __assume(false);
    }

    if(instr->GetSrc2()->IsIntConstOpnd())
    {
        // Only values between 0-31 mean anything
        IntConstType value = instr->GetSrc2()->AsIntConstOpnd()->GetValue();
        value &= 0x1f;
        instr->GetSrc2()->AsIntConstOpnd()->SetValue(value);
    }
}

void LowererMD::ChangeToMul(IR::Instr *const instr, bool hasOverflowCheck)
{
    // If non-32 bit overflow check is needed, we have to use the IMUL form.
    if (hasOverflowCheck && !instr->ShouldCheckFor32BitOverflow() && instr->ShouldCheckForNon32BitOverflow())
    {
        IR::RegOpnd *regEAX = IR::RegOpnd::New(TyInt32, instr->m_func);
        IR::Opnd *temp2 = nullptr;
        // MOV eax, src1
        regEAX->SetReg(LowererMDArch::GetRegIMulDestLower());
        instr->InsertBefore(IR::Instr::New(Js::OpCode::MOV, regEAX, instr->GetSrc1(), instr->m_func));

        if (instr->GetSrc2()->IsImmediateOpnd())
        {
            // MOV reg, imm
            temp2 = IR::RegOpnd::New(TyInt32, instr->m_func);
            instr->InsertBefore(IR::Instr::New(Js::OpCode::MOV, temp2,
                IR::IntConstOpnd::New((IntConstType)instr->GetSrc2()->GetImmediateValue(), TyInt32, instr->m_func, true),
                instr->m_func));
        }
        // eax = IMUL eax, reg
        instr->m_opcode = Js::OpCode::IMUL;
        instr->ReplaceSrc1(regEAX);

        if (temp2 != nullptr)
            instr->ReplaceSrc2(temp2);

        auto *dst = instr->GetDst()->Copy(instr->m_func);
        instr->ReplaceDst(regEAX);

        // MOV dst, eax
        instr->InsertAfter(IR::Instr::New(Js::OpCode::MOV, dst, regEAX, instr->m_func));
    }
    else
        EmitInt4Instr(instr); // IMUL2
}

const uint16
LowererMD::GetFormalParamOffset()
{
    //In x86\x64 formal params were offset from EBP by the EBP chain, return address, and the 2 non-user params
    return 4;
}

IR::Instr *
LowererMD::LowerCatch(IR::Instr * instr)
{
    // t1 = catch    =>    t2(eax) = catch
    //               =>    t1 = t2(eax)
    IR::Opnd *catchObj = instr->UnlinkDst();
    IR::RegOpnd *catchParamReg = IR::RegOpnd::New(TyMachPtr, this->m_func);
    catchParamReg->SetReg(this->lowererMDArch.GetRegReturn(TyMachReg));

    instr->SetDst(catchParamReg);

    instr->InsertAfter(IR::Instr::New(Js::OpCode::MOV, catchObj, catchParamReg, this->m_func));

    return instr->m_prev;
}

///----------------------------------------------------------------------------
///
/// LowererMD::ForceDstToReg
///
///----------------------------------------------------------------------------

void
LowererMD::ForceDstToReg(IR::Instr *instr)
{
    IR::Opnd * dst = instr->GetDst();

    if (dst->IsRegOpnd())
    {
        return;
    }

    if(dst->IsFloat64())
    {
        instr->SinkDst(Js::OpCode::MOVSD);
        return;
    }

    instr->SinkDst(Js::OpCode::MOV);
}

template <bool verify>
void
LowererMD::Legalize(IR::Instr *const instr, bool fPostRegAlloc)
{
    Assert(instr);
    Assert(!instr->isInlineeEntryInstr
        || (instr->m_opcode == Js::OpCode::MOV && instr->GetSrc1()->IsAddrOpnd()));

    switch(instr->m_opcode)
    {
        case Js::OpCode::MOV:
        {
            Assert(instr->GetSrc2() == nullptr);

            IR::Opnd *const dst = instr->GetDst();
            const IRType dstType = dst->GetType();
            IR::Opnd *const src = instr->GetSrc1();
            const IRType srcType = src->GetType();
            if(TySize[dstType] > TySize[srcType])
            {
                if (verify)
                {
                    return;
                }
            #if DBG
                switch(dstType)
                {
                    case TyInt32:
                    case TyUint32:
            #ifdef _M_X64
                    case TyInt64:
                    case TyUint64:
            #endif
                    case TyVar:
                        break;

                    default:
                        Assert(false);
                }
            #endif

                IR::IntConstOpnd *const intConstantSrc = src->IsIntConstOpnd() ? src->AsIntConstOpnd() : nullptr;
                const auto UpdateIntConstantSrc = [&](const size_t extendedValue)
                {
                    Assert(intConstantSrc);

                #ifdef _M_X64
                    if(TySize[dstType] > sizeof(IntConstType))
                    {
                        instr->ReplaceSrc1(
                            IR::AddrOpnd::New(
                                reinterpret_cast<void *>(extendedValue),
                                IR::AddrOpndKindConstantVar,
                                instr->m_func,
                                intConstantSrc->m_dontEncode));
                    }
                    else
                #endif
                    {
                        intConstantSrc->SetType(dstType);
                        intConstantSrc->SetValue(static_cast<IntConstType>(extendedValue));
                    }
                };

                switch(srcType)
                {
                    case TyInt8:
                        if(intConstantSrc)
                        {
                            UpdateIntConstantSrc(static_cast<int8>(intConstantSrc->GetValue())); // sign-extend
                            break;
                        }
                        instr->m_opcode = Js::OpCode::MOVSX;
                        break;

                    case TyUint8:
                        if(intConstantSrc)
                        {
                            UpdateIntConstantSrc(static_cast<uint8>(intConstantSrc->GetValue())); // zero-extend
                            break;
                        }
                        instr->m_opcode = Js::OpCode::MOVZX;
                        break;

                    case TyInt16:
                        if(intConstantSrc)
                        {
                            UpdateIntConstantSrc(static_cast<int16>(intConstantSrc->GetValue())); // sign-extend
                            break;
                        }
                        instr->m_opcode = Js::OpCode::MOVSXW;
                        break;

                    case TyUint16:
                        if(intConstantSrc)
                        {
                            UpdateIntConstantSrc(static_cast<uint16>(intConstantSrc->GetValue())); // zero-extend
                            break;
                        }
                        instr->m_opcode = Js::OpCode::MOVZXW;
                        break;

                #ifdef _M_X64
                    case TyInt32:
                        if(intConstantSrc)
                        {
                            UpdateIntConstantSrc(static_cast<int32>(intConstantSrc->GetValue())); // sign-extend
                            break;
                        }
                        instr->m_opcode = Js::OpCode::MOVSXD;
                        break;

                    case TyUint32:
                        if(intConstantSrc)
                        {
                            UpdateIntConstantSrc(static_cast<uint32>(intConstantSrc->GetValue())); // zero-extend
                            break;
                        }
                        switch(dst->GetKind())
                        {
                            case IR::OpndKindReg:
                                // (mov r0.u32, r1.u32) clears the upper 32 bits of r0
                                dst->SetType(TyUint32);
                                instr->m_opcode = Js::OpCode::MOV_TRUNC;
                                break;

                            case IR::OpndKindSym:
                            case IR::OpndKindIndir:
                            case IR::OpndKindMemRef:
                                // Even if the src is a reg, we don't know if the upper 32 bits are zero. Copy the value to a
                                // reg first to zero-extend it to 64 bits, and then copy the 64-bit value to the original dst.
                                instr->HoistSrc1(Js::OpCode::MOV_TRUNC);
                                instr->GetSrc1()->SetType(dstType);
                                break;

                            default:
                                Assert(false);
                                __assume(false);
                        }
                        break;
                #endif

                    default:
                        Assert(false);
                        __assume(false);
                }
            }
            else if (TySize[dstType] < TySize[srcType])
            {
                instr->GetSrc1()->SetType(dst->GetType());
            }

            if(instr->m_opcode == Js::OpCode::MOV)
            {
                uint src1Forms = L_Reg | L_Mem | L_Ptr;     // Allow 64 bit values in x64 as well
#if _M_X64
                if (dst->IsMemoryOpnd())
                {
                    // Only allow <= 32 bit values
                    src1Forms = L_Reg | L_Imm32;
                }
#endif
                LegalizeOpnds<verify>(
                    instr,
                    L_Reg | L_Mem,
                    src1Forms,
                    L_None);
            }
            else
            {
                LegalizeOpnds<verify>(
                    instr,
                    L_Reg,
                    L_Reg | L_Mem,
                    L_None);
            }
            break;
        }

        case Js::OpCode::MOVSD:
        case Js::OpCode::MOVSS:
        {
            Assert(instr->GetDst()->GetType() == (instr->m_opcode == Js::OpCode::MOVSD? TyFloat64 : TyFloat32) || instr->GetDst()->IsSimd128());
            Assert(instr->GetSrc1()->GetType() == (instr->m_opcode == Js::OpCode::MOVSD ? TyFloat64 : TyFloat32) || instr->GetSrc1()->IsSimd128());

            LegalizeOpnds<verify>(
                instr,
                L_Reg | L_Mem,
                instr->GetDst()->IsMemoryOpnd()?
                    L_Reg : L_Reg | L_Mem,   // LegalizeOpnds doesn't check if dst/src1 are both memopnd, check it here.
                L_None);

            break;
        }

        case Js::OpCode::MOVUPS:
        case Js::OpCode::MOVAPS:
        {
            LegalizeOpnds<verify>(
                instr,
                L_Reg | L_Mem,
                instr->GetDst()->IsMemoryOpnd()?
                    L_Reg : L_Reg | L_Mem,   // LegalizeOpnds doesn't check if dst/src1 are both memopnd, check it here.
                L_None);
            break;
        }

        case Js::OpCode::CMP:
            LegalizeOpnds<verify>(
                instr,
                L_None,
                L_Reg | L_Mem,
                L_Reg | L_Mem | L_Imm32);
            break;

        case Js::OpCode::TEST:
            if(instr->GetSrc1()->IsImmediateOpnd() && !instr->GetSrc2()->IsImmediateOpnd() ||
                instr->GetSrc2()->IsMemoryOpnd() && !instr->GetSrc1()->IsMemoryOpnd())
            {
                if (verify)
                {
                    AssertMsg(false, "Missing legalization");
                    return;
                }
                instr->SwapOpnds();
            }
            LegalizeOpnds<verify>(
                instr,
                L_None,
                L_Reg | L_Mem,
                L_Reg | L_Imm32);
            break;

        case Js::OpCode::COMISD:
        case Js::OpCode::COMISS:
        case Js::OpCode::UCOMISD:
        case Js::OpCode::UCOMISS:
            LegalizeOpnds<verify>(
                instr,
                L_None,
                L_Reg,
                L_Reg | L_Mem);
            break;

        case Js::OpCode::INC:
        case Js::OpCode::DEC:
        case Js::OpCode::NEG:
            MakeDstEquSrc1<verify>(instr);
            LegalizeOpnds<verify>(
                instr,
                L_Reg | L_Mem,
                L_Reg | L_Mem,
                L_None);
            break;

        case Js::OpCode::ADD:
        case Js::OpCode::SUB:
        case Js::OpCode::AND:
        case Js::OpCode::OR:
        case Js::OpCode::XOR:
            MakeDstEquSrc1<verify>(instr);
            LegalizeOpnds<verify>(
                instr,
                L_Reg | L_Mem,
                L_Reg | L_Mem,
                L_Reg | L_Mem | L_Imm32);
            break;

        case Js::OpCode::ADDSD:
        case Js::OpCode::ADDPS:
        case Js::OpCode::ADDPD:
        case Js::OpCode::SUBSD:
        case Js::OpCode::ADDSS:
        case Js::OpCode::SUBSS:
        case Js::OpCode::ANDPS:
        case Js::OpCode::ANDPD:
        case Js::OpCode::ANDNPS:
        case Js::OpCode::ANDNPD:
        case Js::OpCode::DIVPS:
        case Js::OpCode::DIVPD:
        case Js::OpCode::MAXPD:
        case Js::OpCode::MAXPS:
        case Js::OpCode::MINPD:
        case Js::OpCode::MINPS:
        case Js::OpCode::MULPD:
        case Js::OpCode::MULPS:
        case Js::OpCode::ORPS:
        case Js::OpCode::PADDD:
        case Js::OpCode::PAND:
        case Js::OpCode::PCMPEQD:
        case Js::OpCode::PCMPGTD:
        case Js::OpCode::PMULUDQ:
        case Js::OpCode::POR:
        case Js::OpCode::PSUBD:
        case Js::OpCode::PXOR:
        case Js::OpCode::SUBPD:
        case Js::OpCode::SUBPS:
        case Js::OpCode::XORPS:
        case Js::OpCode::CMPLTPS:
        case Js::OpCode::CMPLEPS:
        case Js::OpCode::CMPEQPS:
        case Js::OpCode::CMPNEQPS:
        case Js::OpCode::CMPLTPD:
        case Js::OpCode::CMPLEPD:
        case Js::OpCode::CMPEQPD:
        case Js::OpCode::CMPNEQPD:
        case Js::OpCode::PUNPCKLDQ:

            MakeDstEquSrc1<verify>(instr);
            LegalizeOpnds<verify>(
                instr,
                L_Reg,
                L_Reg,
                L_Reg | L_Mem);
            break;

        case Js::OpCode::SHL:
        case Js::OpCode::SHR:
        case Js::OpCode::SAR:
            if (verify)
            {
                Assert(instr->GetSrc2()->IsIntConstOpnd()
                    || instr->GetSrc2()->AsRegOpnd()->GetReg() == LowererMDArch::GetRegShiftCount());
            }
            else
            {
                if(!instr->GetSrc2()->IsIntConstOpnd())
                {
                    IR::Instr *const newInstr = instr->HoistSrc2(Js::OpCode::MOV);
                    newInstr->GetDst()->AsRegOpnd()->SetReg(LowererMDArch::GetRegShiftCount());
                    instr->GetSrc2()->AsRegOpnd()->SetReg(LowererMDArch::GetRegShiftCount());
                }
                instr->GetSrc2()->SetType(TyUint8);
            }
            MakeDstEquSrc1<verify>(instr);
            LegalizeOpnds<verify>(
                instr,
                L_Reg | L_Mem,
                L_Reg | L_Mem,
                L_Reg | L_Imm32);
            break;

        case Js::OpCode::IMUL2:
            MakeDstEquSrc1<verify>(instr); // the encoder does not support IMUL3 r, r/m, imm
            LegalizeOpnds<verify>(
                instr,
                L_Reg,
                L_Reg,
                L_Reg | L_Mem | L_Imm32); // for L_Imm32, the encoder converts it into an IMUL3
            break;

        case Js::OpCode::LZCNT:
        case Js::OpCode::BSR:
            LegalizeOpnds<verify>(
                instr,
                L_Reg,
                L_Reg | L_Mem,
                L_None);
            break;

        case Js::OpCode::LEA:
            Assert(instr->GetDst()->IsRegOpnd());
            Assert(instr->GetSrc1()->IsIndirOpnd() || instr->GetSrc1()->IsSymOpnd());
            Assert(!instr->GetSrc2());
            break;


        case Js::OpCode::PSRLDQ:
        case Js::OpCode::PSLLDQ:
            MakeDstEquSrc1<verify>(instr);
            LegalizeOpnds<verify>(
                instr,
                L_Reg,
                L_Reg,
                L_Imm32);
            break;

    }

#if DBG
    // Asserting general rules
    // There should be at most 1 memory opnd in an instruction
    if (instr->GetDst() && instr->GetDst()->IsMemoryOpnd())
    {
        // All memref address need to fit in a dword
        Assert(!instr->GetDst()->IsMemRefOpnd() || Math::FitsInDWord((size_t)instr->GetDst()->AsMemRefOpnd()->GetMemLoc()));
        if (instr->GetSrc1())
        {
            Assert(instr->GetSrc1()->IsEqual(instr->GetDst()) || !instr->GetSrc1()->IsMemoryOpnd());
            if (instr->GetSrc2())
            {
                Assert(!instr->GetSrc2()->IsMemoryOpnd());
            }
        }
    }
    else if (instr->GetSrc1() && instr->GetSrc1()->IsMemoryOpnd())
    {
        // All memref address need to fit in a dword
        Assert(!instr->GetSrc1()->IsMemRefOpnd() || Math::FitsInDWord((size_t)instr->GetSrc1()->AsMemRefOpnd()->GetMemLoc()));
        Assert(!instr->GetSrc2() || !instr->GetSrc2()->IsMemoryOpnd());
    }
    else if (instr->GetSrc2() && instr->GetSrc2()->IsMemRefOpnd())
    {
        // All memref address need to fit in a dword
        Assert(Math::FitsInDWord((size_t)instr->GetSrc2()->AsMemRefOpnd()->GetMemLoc()));
    }

    // Non-MOV (second operand) immediate need to fit in DWORD for AMD64
    Assert(!instr->GetSrc2() || !instr->GetSrc2()->IsImmediateOpnd()
        || (TySize[instr->GetSrc2()->GetType()] != 8) || Math::FitsInDWord(instr->GetSrc2()->GetImmediateValue()));
#endif
}

template <bool verify>
void LowererMD::LegalizeOpnds(IR::Instr *const instr, const uint dstForms, const uint src1Forms, uint src2Forms)
{
    Assert(instr);
    Assert(!instr->GetDst() == !dstForms);
    Assert(!instr->GetSrc1() == !src1Forms);
    Assert(!instr->GetSrc2() == !src2Forms);
    Assert(src1Forms || !src2Forms);

    const auto NormalizeForms = [](uint forms) -> uint
    {
    #ifdef _M_X64
        if(forms & L_Ptr)
        {
            forms |= L_Imm32;
        }
    #else
        if(forms & (L_Imm32 | L_Ptr))
        {
            forms |= L_Imm32 | L_Ptr;
        }
    #endif
        return forms;
    };

    if(dstForms)
    {
        LegalizeDst<verify>(instr, NormalizeForms(dstForms));
    }
    if(!src1Forms)
    {
        return;
    }
    LegalizeSrc<verify>(instr, instr->GetSrc1(), NormalizeForms(src1Forms));
    if(src2Forms & L_Mem && instr->GetSrc1()->IsMemoryOpnd())
    {
        src2Forms ^= L_Mem;
    }
    if(src2Forms)
    {
        LegalizeSrc<verify>(instr, instr->GetSrc2(), NormalizeForms(src2Forms));
    }
}

template <bool verify>
void LowererMD::LegalizeDst(IR::Instr *const instr, const uint forms)
{
    Assert(instr);
    Assert(forms);

    IR::Opnd *dst = instr->GetDst();
    Assert(dst);

    switch(dst->GetKind())
    {
        case IR::OpndKindReg:
            Assert(forms & L_Reg);
            return;

        case IR::OpndKindMemRef:
        {
            IR::MemRefOpnd *const memRefOpnd = dst->AsMemRefOpnd();
            if(!LowererMDArch::IsLegalMemLoc(memRefOpnd))
            {
                if (verify)
                {
                    AssertMsg(false, "Missing legalization");
                    return;
                }
                dst = instr->HoistMemRefAddress(memRefOpnd, Js::OpCode::MOV);
            }
            // fall through
        }

        case IR::OpndKindSym:
        case IR::OpndKindIndir:
            if(forms & L_Mem)
            {
                return;
            }
            break;

        default:
            Assert(false);
            __assume(false);
    }

    if (verify)
    {
        AssertMsg(false, "Missing legalization");
        return;
    }

    // Use a reg dst, then store that reg into the original dst
    Assert(forms & L_Reg);
    const IRType irType = dst->GetType();
    IR::RegOpnd *const regOpnd = IR::RegOpnd::New(irType, instr->m_func);
    regOpnd->SetValueType(dst->GetValueType());
    instr->UnlinkDst();
    instr->SetDst(regOpnd);
    instr->InsertAfter(IR::Instr::New(GetStoreOp(irType), dst, regOpnd, instr->m_func));

    // If the original dst is the same as one of the srcs, hoist a src into the same reg and replace the same srcs with the reg
    const bool equalsSrc1 = instr->GetSrc1() && dst->IsEqual(instr->GetSrc1());
    const bool equalsSrc2 = instr->GetSrc2() && dst->IsEqual(instr->GetSrc2());
    if(!(equalsSrc1 || equalsSrc2))
    {
        return;
    }
    const Js::OpCode loadOpCode = GetLoadOp(irType);
    if(equalsSrc1)
    {
        instr->HoistSrc1(loadOpCode, RegNOREG, regOpnd->m_sym);
        if(equalsSrc2)
        {
            instr->ReplaceSrc2(regOpnd);
        }
    }
    else
    {
        instr->HoistSrc2(loadOpCode, RegNOREG, regOpnd->m_sym);
    }
}

template <bool verify>
void LowererMD::LegalizeSrc(IR::Instr *const instr, IR::Opnd *src, const uint forms)
{
    Assert(instr);
    Assert(src);
    Assert(src == instr->GetSrc1() || src == instr->GetSrc2());
    Assert(forms);


    switch(src->GetKind())
    {
        case IR::OpndKindReg:
            Assert(forms & L_Reg);
            return;

        case IR::OpndKindIntConst:
            Assert(!instr->isInlineeEntryInstr);
            if(forms & L_Imm32)
            {
                return;
            }
            break;

        case IR::OpndKindFloatConst:
            break; // assume for now that it always needs to be hoisted

        case IR::OpndKindAddr:
            if (forms & L_Ptr)
            {
                return;
            }
#ifdef _M_X64
            {
                IR::AddrOpnd * addrOpnd = src->AsAddrOpnd();
                if ((forms & L_Imm32) && ((TySize[addrOpnd->GetType()] != 8) ||
                    (!instr->isInlineeEntryInstr && Math::FitsInDWord((size_t)addrOpnd->m_address))))
                {
                    // the address fits in 32-bit, no need to hoist
                    return;
                }
                if (verify)
                {
                    AssertMsg(false, "Missing legalization");
                    return;
                }

                // The actual value for inlinee entry instr isn't determined until encoder
                // So it need to be hoisted conventionally.
                if (!instr->isInlineeEntryInstr)
                {
                    Assert(forms & L_Reg);
                    IR::IndirOpnd * indirOpnd = instr->m_func->GetTopFunc()->GetConstantAddressIndirOpnd(addrOpnd->m_address, addrOpnd->GetAddrOpndKind(), TyMachPtr, Js::OpCode::MOV);
                    if (indirOpnd != nullptr)
                    {
                        if (indirOpnd->GetOffset() == 0)
                        {
                            instr->ReplaceSrc(src, indirOpnd->GetBaseOpnd());
                        }
                        else
                        {
                            // Hoist the address load as LEA [reg + offset]
                            // with the reg = MOV <some address within 32-bit range at the start of the function
                            IR::RegOpnd * regOpnd = IR::RegOpnd::New(TyMachPtr, instr->m_func);
                            Lowerer::InsertLea(regOpnd, indirOpnd, instr);
                            instr->ReplaceSrc(src, regOpnd);
                        }
                        return;
                    }
                }
            }
#endif
            break;

        case IR::OpndKindMemRef:
        {
            IR::MemRefOpnd *const memRefOpnd = src->AsMemRefOpnd();
            if(!LowererMDArch::IsLegalMemLoc(memRefOpnd))
            {
                if (verify)
                {
                    AssertMsg(false, "Missing legalization");
                    return;
                }
                src = instr->HoistMemRefAddress(memRefOpnd, Js::OpCode::MOV);
            }
            // fall through
        }

        case IR::OpndKindSym:
        case IR::OpndKindIndir:
            if(forms & L_Mem)
            {
                return;
            }
            break;

        case IR::OpndKindHelperCall:
        case IR::OpndKindLabel:
            Assert(!instr->isInlineeEntryInstr);
            Assert(forms & L_Ptr);
            return;

        default:
            Assert(false);
            __assume(false);
    }

    if (verify)
    {
        AssertMsg(false, "Missing legalization");
        return;
    }

    // Hoist the src into a reg
    Assert(forms & L_Reg);
    Assert(!(instr->GetDst() && instr->GetDst()->IsEqual(src)));
    const Js::OpCode loadOpCode = GetLoadOp(src->GetType());
    if(src == instr->GetSrc2())
    {
        instr->HoistSrc2(loadOpCode);
        return;
    }
    const bool equalsSrc2 = instr->GetSrc2() && src->IsEqual(instr->GetSrc2());
    IR::Instr * hoistInstr = instr->HoistSrc1(loadOpCode);
    if(equalsSrc2)
    {
        instr->ReplaceSrc2(hoistInstr->GetDst());
    }
    hoistInstr->isInlineeEntryInstr = instr->isInlineeEntryInstr;
    instr->isInlineeEntryInstr = false;
}

template void LowererMD::Legalize<false>(IR::Instr *const instr, bool fPostRegAlloc);
template void LowererMD::LegalizeOpnds<false>(IR::Instr *const instr, const uint dstForms, const uint src1Forms, uint src2Forms);
template void LowererMD::LegalizeDst<false>(IR::Instr *const instr, const uint forms);
template void LowererMD::LegalizeSrc<false>(IR::Instr *const instr, IR::Opnd *src, const uint forms);
template void LowererMD::MakeDstEquSrc1<false>(IR::Instr *const instr);

#if DBG
template void LowererMD::Legalize<true>(IR::Instr *const instr, bool fPostRegAlloc);
template void LowererMD::LegalizeOpnds<true>(IR::Instr *const instr, const uint dstForms, const uint src1Forms, uint src2Forms);
template void LowererMD::LegalizeDst<true>(IR::Instr *const instr, const uint forms);
template void LowererMD::LegalizeSrc<true>(IR::Instr *const instr, IR::Opnd *src, const uint forms);
template void LowererMD::MakeDstEquSrc1<true>(IR::Instr *const instr);
#endif

IR::Instr *
LowererMD::LoadFunctionObjectOpnd(IR::Instr *instr, IR::Opnd *&functionObjOpnd)
{
    IR::Opnd * src1 = instr->GetSrc1();
    IR::Instr * instrPrev = instr->m_prev;

    if (src1 == nullptr)
    {
        IR::RegOpnd * regOpnd = IR::RegOpnd::New(TyMachPtr, m_func);
        StackSym *paramSym = StackSym::New(TyMachPtr, m_func);
        IR::SymOpnd *paramOpnd = IR::SymOpnd::New(paramSym, TyMachPtr, m_func);
        this->m_func->SetArgOffset(paramSym, 2 * MachPtr);

        IR::Instr * mov1 = IR::Instr::New(Js::OpCode::MOV, regOpnd, paramOpnd, m_func);
        instr->InsertBefore(mov1);
        functionObjOpnd = mov1->GetDst()->AsRegOpnd();
        instrPrev = mov1;
    }
    else
    {
        // Inlinee, use the function object opnd on the instruction
        functionObjOpnd = instr->UnlinkSrc1();
        if (!functionObjOpnd->IsRegOpnd())
        {
            Assert(functionObjOpnd->IsAddrOpnd());
        }
    }

    return instrPrev;
}

IR::Instr *
LowererMD::LowerLdSuper(IR::Instr *instr, IR::JnHelperMethod helperOpCode)
{
    IR::Opnd * functionObjOpnd;
    IR::Instr * instrPrev = LoadFunctionObjectOpnd(instr, functionObjOpnd);

    m_lowerer->LoadScriptContext(instr);
    LoadHelperArgument(instr, functionObjOpnd);
    ChangeToHelperCall(instr, helperOpCode);

    return instrPrev;
}

void
LowererMD::GenerateFastDivByPow2(IR::Instr *instr)
{
    //
    // Given:
    // dst = Div_A src1, src2
    // where src2 == power of 2
    //
    // Generate:
    //     MOV  s1, src1
    //     AND  s1, 0xFFFF000000000000 | (src2Value-1)            ----- test for tagged int and divisibility by src2Value     [int32]
    //     AND  s1, 0x00000001 | ((src2Value-1)<<1)                                                                           [int31]
    //     CMP  s1, AtomTag_IntPtr
    //     JNE  $divbyhalf
    //     MOV  s1, src1
    //     SAR  s1, log2(src2Value)                               ------ perform the divide
    //     OR   s1, 1
    //     MOV  dst, s1
    //     JMP  $done
    // $divbyhalf:
    //     AND  s1, 0xFFFF000000000000 | (src2Value-1>>1)        ----- test for tagged int and divisibility by src2Value /2  [int32]
    //     AND  s1, 0x00000001 | ((src2Value-1))                                                                             [int31]
    //     CMP  s1, AtomTag_IntPtr
    //     JNE  $helper
    //     MOV  s1, src1
    //     SAR  s1, log2(src2Value)                                                                                          [int32]
    //     SAR  s1, log2(src2Value) + 1                         ------ removes the tag and divides                           [int31]
    //     PUSH s1
    //     PUSH 0xXXXXXXXX (ScriptContext)
    //     CALL Op_FinishOddDivByPow2
    //     MOV  dst, eax
    //     JMP $done
    // $helper:
    //     ...
    // $done:
    //

    if (instr->GetSrc1()->IsRegOpnd() && instr->GetSrc1()->AsRegOpnd()->IsNotInt())
        return;

    IR::Opnd       *dst = instr->GetDst();
    IR::Opnd       *src1 = instr->GetSrc1();
    IR::AddrOpnd   *src2 = instr->GetSrc2()->IsAddrOpnd() ? instr->GetSrc2()->AsAddrOpnd() : nullptr;
    IR::LabelInstr *divbyhalf = IR::LabelInstr::New(Js::OpCode::Label, m_func);
    IR::LabelInstr *helper = IR::LabelInstr::New(Js::OpCode::Label, m_func, true);
    IR::LabelInstr *done = IR::LabelInstr::New(Js::OpCode::Label, m_func);
    IR::RegOpnd    *s1 = IR::RegOpnd::New(TyVar, m_func);

    AnalysisAssert(src2);
    Assert(src2->IsVar() && Js::TaggedInt::Is(src2->m_address) && (Math::IsPow2(Js::TaggedInt::ToInt32(src2->m_address))));
    int32           src2Value = Js::TaggedInt::ToInt32(src2->m_address);

    // MOV s1, src1
    instr->InsertBefore(IR::Instr::New(Js::OpCode::MOV, s1, src1, m_func));

#if INT32VAR
    // dontEncode as src2 is a power of 2.
    IR::Opnd *constant = IR::AddrOpnd::New((Js::Var)(0xFFFF000000000000 | (src2Value - 1)), IR::AddrOpndKindConstantVar, m_func, /* dontEncode = */ true);
#else
    IR::Opnd *constant = IR::IntConstOpnd::New((0x00000001 | ((src2Value - 1) << 1)), TyInt32, m_func);
#endif

    // AND  s1, constant
    {
        IR::Instr * andInstr = IR::Instr::New(Js::OpCode::AND, s1, s1, constant, m_func);
        instr->InsertBefore(andInstr);
        Legalize(andInstr);
    }

    // CMP s1, AtomTag_IntPtr
    {
        IR::Instr *cmp = IR::Instr::New(Js::OpCode::CMP, m_func);
        cmp->SetSrc1(s1);
        cmp->SetSrc2(IR::AddrOpnd::New((Js::Var)(Js::AtomTag_IntPtr), IR::AddrOpndKindConstantVar, m_func, /* dontEncode = */ true));
        instr->InsertBefore(cmp);
        Legalize(cmp);
    }

    // JNE $divbyhalf
    instr->InsertBefore(IR::BranchInstr::New(Js::OpCode::JNE, divbyhalf, m_func));

    // MOV s1, src1
    instr->InsertBefore(IR::Instr::New(Js::OpCode::MOV, s1, src1, m_func));

    s1 = s1->UseWithNewType(TyInt32, m_func)->AsRegOpnd();

    // SAR s1, log2(src2Value)
    instr->InsertBefore(IR::Instr::New(Js::OpCode::SAR, s1, s1, IR::IntConstOpnd::New(Math::Log2(src2Value), TyInt32, m_func), m_func));

    if(s1->GetSize() != MachPtr)
    {
        s1 = s1->UseWithNewType(TyMachPtr, m_func)->AsRegOpnd();
    }

#if INT32VAR
    GenerateInt32ToVarConversion(s1, instr);
#else
    // OR s1, 1
    instr->InsertBefore(IR::Instr::New(Js::OpCode::OR, s1, s1, IR::IntConstOpnd::New(1, TyInt32, m_func), m_func));
#endif

    // MOV dst, s1
    instr->InsertBefore(IR::Instr::New(Js::OpCode::MOV, dst, s1, m_func));

    // JMP $done
    instr->InsertBefore(IR::BranchInstr::New(Js::OpCode::JMP, done, m_func));

    // $divbyhalf:
    instr->InsertBefore(divbyhalf);

#if INT32VAR
    constant = IR::AddrOpnd::New((Js::Var)(0xFFFF000000000000 | ((src2Value-1) >> 1)), IR::AddrOpndKindConstantVar, m_func, /* dontEncode = */ true);
#else
    constant = IR::IntConstOpnd::New((0x00000001 | (src2Value-1)), TyInt32, m_func);
#endif

    // AND  s1, constant
    {
        IR::Instr * andInstr = IR::Instr::New(Js::OpCode::AND, s1, s1, constant, m_func);
        instr->InsertBefore(andInstr);
        Legalize(andInstr);
    }

    // CMP s1, AtomTag_IntPtr
    {
        IR::Instr *cmp = IR::Instr::New(Js::OpCode::CMP, m_func);
        cmp->SetSrc1(s1);
        cmp->SetSrc2(IR::AddrOpnd::New((Js::Var)(Js::AtomTag_IntPtr), IR::AddrOpndKindConstantVar, m_func, /* dontEncode = */ true));
        instr->InsertBefore(cmp);
        Legalize(cmp);
    }

    // JNE $helper
    instr->InsertBefore(IR::BranchInstr::New(Js::OpCode::JNE, helper, m_func));

    // MOV s1, src1
    instr->InsertBefore(IR::Instr::New(Js::OpCode::MOV, s1, src1, m_func));

    s1 = s1->UseWithNewType(TyInt32, this->m_func)->AsRegOpnd();

#if INT32VAR
    IR::Opnd* shiftOpnd = IR::IntConstOpnd::New(Math::Log2(src2Value), TyInt32, m_func);
#else
    IR::Opnd* shiftOpnd = IR::IntConstOpnd::New(Math::Log2(src2Value) + 1, TyInt32, m_func);
#endif

    // SAR s1, shiftOpnd
    instr->InsertBefore(IR::Instr::New(Js::OpCode::SAR, s1, s1, shiftOpnd, m_func));

    // PUSH s1
    // PUSH ScriptContext
    // CALL Op_FinishOddDivByPow2
    {
        IR::JnHelperMethod helperMethod;

        if (instr->dstIsTempNumber)
        {
            IR::Opnd *tempOpnd;
            helperMethod = IR::HelperOp_FinishOddDivByPow2InPlace;
            Assert(dst->IsRegOpnd());
            StackSym * tempNumberSym = this->m_lowerer->GetTempNumberSym(dst, instr->dstIsTempNumberTransferred);

            IR::Instr *load = this->LoadStackAddress(tempNumberSym);
            instr->InsertBefore(load);
            tempOpnd = load->GetDst();
            this->lowererMDArch.LoadHelperArgument(instr, tempOpnd);
        }
        else
        {
            helperMethod = IR::HelperOp_FinishOddDivByPow2;
        }

        m_lowerer->LoadScriptContext(instr);

        lowererMDArch.LoadHelperArgument(instr, s1);

        IR::Instr *call = IR::Instr::New(Js::OpCode::Call, dst, IR::HelperCallOpnd::New(helperMethod, m_func), m_func);
        instr->InsertBefore(call);
        lowererMDArch.LowerCall(call, 0);
    }


    // JMP $done
    instr->InsertBefore(IR::BranchInstr::New(Js::OpCode::JMP, done, m_func));

    // $helper:
    instr->InsertBefore(helper);

    // $done:
    instr->InsertAfter(done);
}

bool
LowererMD::GenerateFastBrString(IR::BranchInstr *branchInstr)
{
    Assert(branchInstr->m_opcode == Js::OpCode::BrSrEq_A     ||
           branchInstr->m_opcode == Js::OpCode::BrSrNeq_A    ||
           branchInstr->m_opcode == Js::OpCode::BrEq_A       ||
           branchInstr->m_opcode == Js::OpCode::BrNeq_A      ||
           branchInstr->m_opcode == Js::OpCode::BrSrNotEq_A  ||
           branchInstr->m_opcode == Js::OpCode::BrSrNotNeq_A ||
           branchInstr->m_opcode == Js::OpCode::BrNotEq_A    ||
           branchInstr->m_opcode == Js::OpCode::BrNotNeq_A
           );

    IR::Instr* instrInsert = branchInstr;
    IR::RegOpnd *srcReg1 = branchInstr->GetSrc1()->IsRegOpnd() ? branchInstr->GetSrc1()->AsRegOpnd() : nullptr;
    IR::RegOpnd *srcReg2 = branchInstr->GetSrc2()->IsRegOpnd() ? branchInstr->GetSrc2()->AsRegOpnd() : nullptr;

    if (srcReg1 && srcReg2)
    {
        if (srcReg1->IsTaggedInt() || srcReg2->IsTaggedInt())
        {
            return false;
        }

        bool isSrc1String = srcReg1->GetValueType().IsLikelyString();
        bool isSrc2String = srcReg2->GetValueType().IsLikelyString();
        //Left and right hand are both LikelyString
        if (!isSrc1String || !isSrc2String)
        {
            return false;
        }
    }
    else
    {
        return false;
    }

    // Generates:
    //      GenerateObjectTest(src1);
    //      MOV s1, [srcReg1 + offset(Type)]
    //      CMP type, static_string_type
    //      JNE $helper
    //      GenerateObjectTest(src2);
    //      MOV s2, [srcReg2 + offset(Type)]
    //      CMP type, static_string_type
    //      JNE $fail                         ; if src1 is string but not src2, src1 !== src2 if isStrict
    //      MOV s3, [srcReg1,offset(m_charLength)]
    //      CMP [srcReg2,offset(m_charLength)], s3
    //      JNE $fail                     <--- length check done
    //      MOV s4, [srcReg1,offset(m_pszValue)]
    //      CMP srcReg1, srcReg2
    //      JEQ $success
    //      CMP s4, 0
    //      JEQ $helper
    //      MOV s5, [srcReg2,offset(m_pszValue)]
    //      CMP s5, 0
    //      JEQ $helper
    //      MOV s6,[s4]
    //      CMP [s5], s6                       -First character comparison
    //      JNE $fail
    //      SHL length, 1
    //      eax = memcmp(src1String, src2String, length*2)
    //      TEST eax, eax
    //      JEQ $success
    //      JMP $fail


    IR::LabelInstr *labelHelper = IR::LabelInstr::New(Js::OpCode::Label, this->m_func, true);
    IR::LabelInstr *labelTarget   = branchInstr->GetTarget();
    IR::LabelInstr *labelBranchFail = nullptr;
    IR::LabelInstr *labelBranchSuccess = nullptr;
    bool isEqual = false;
    bool isStrict = false;

    switch (branchInstr->m_opcode)
    {
        case Js::OpCode::BrSrEq_A:
        case Js::OpCode::BrSrNotNeq_A:
            isStrict = true;
        case Js::OpCode::BrEq_A:
        case Js::OpCode::BrNotNeq_A:
            labelBranchFail = IR::LabelInstr::New(Js::OpCode::Label, this->m_func);
            labelBranchSuccess = labelTarget;
            branchInstr->InsertAfter(labelBranchFail);
            isEqual = true;
            break;

        case Js::OpCode::BrSrNeq_A:
        case Js::OpCode::BrSrNotEq_A:
            isStrict = true;
        case Js::OpCode::BrNeq_A:
        case Js::OpCode::BrNotEq_A:
            labelBranchSuccess = IR::LabelInstr::New(Js::OpCode::Label, this->m_func);
            labelBranchFail = labelTarget;
            branchInstr->InsertAfter(labelBranchSuccess);
            isEqual = false;
            break;

        default:
            Assert(UNREACHED);
            __assume(0);
    }

    this->m_lowerer->GenerateStringTest(srcReg1, instrInsert, labelHelper);

    if (isStrict)
    {
        this->m_lowerer->GenerateStringTest(srcReg2, instrInsert, labelBranchFail);
    }
    else
    {
        this->m_lowerer->GenerateStringTest(srcReg2, instrInsert, labelHelper);
    }

    //      MOV s3, [srcReg1,offset(m_charLength)]
    //      CMP [srcReg2,offset(m_charLength)], s3
    //      JNE $branchfail

    IR::RegOpnd * src1LengthOpnd = IR::RegOpnd::New(TyUint32, this->m_func);
    IR::Instr * loadSrc1LengthInstr = IR::Instr::New(Js::OpCode::MOV, src1LengthOpnd,
        IR::IndirOpnd::New(srcReg1, Js::JavascriptString::GetOffsetOfcharLength(), TyUint32,
        this->m_func), this->m_func);
    instrInsert->InsertBefore(loadSrc1LengthInstr);

    IR::Instr * checkLengthInstr = IR::Instr::New(Js::OpCode::CMP, this->m_func);
    checkLengthInstr->SetSrc1(IR::IndirOpnd::New(srcReg2, Js::JavascriptString::GetOffsetOfcharLength(), TyUint32, this->m_func));
    checkLengthInstr->SetSrc2(src1LengthOpnd);
    instrInsert->InsertBefore(checkLengthInstr);

    instrInsert->InsertBefore(IR::BranchInstr::New(Js::OpCode::JNE, labelBranchFail, this->m_func));

    //      MOV s4, [src1,offset(m_pszValue)]
    //      CMP s4, 0
    //      JEQ $helper
    //      MOV s5, [src2,offset(m_pszValue)]
    //      CMP s5, 0
    //      JEQ $helper

    IR::RegOpnd * src1FlatString = IR::RegOpnd::New(TyMachPtr, this->m_func);
    IR::Instr * loadSrc1StringInstr = IR::Instr::New(Js::OpCode::MOV, src1FlatString,
        IR::IndirOpnd::New(srcReg1, Js::JavascriptString::GetOffsetOfpszValue(), TyMachPtr,
        this->m_func), this->m_func);
    instrInsert->InsertBefore(loadSrc1StringInstr);

    IR::Instr * checkFlatString1Instr = IR::Instr::New(Js::OpCode::CMP, this->m_func);
    checkFlatString1Instr->SetSrc1(src1FlatString);
    checkFlatString1Instr->SetSrc2(IR::IntConstOpnd::New(0, TyUint32, this->m_func));
    instrInsert->InsertBefore(checkFlatString1Instr);
    instrInsert->InsertBefore(IR::BranchInstr::New(Js::OpCode::JEQ, labelHelper, this->m_func));

    IR::RegOpnd * src2FlatString = IR::RegOpnd::New(TyMachPtr, this->m_func);
    IR::Instr * loadSrc2StringInstr = IR::Instr::New(Js::OpCode::MOV, src2FlatString,
        IR::IndirOpnd::New(srcReg2, Js::JavascriptString::GetOffsetOfpszValue(), TyMachPtr,
        this->m_func), this->m_func);
    instrInsert->InsertBefore(loadSrc2StringInstr);

    //      CMP srcReg1, srcReg2                       - Ptr comparison
    //      JEQ $branchSuccess
    IR::Instr * comparePtrInstr = IR::Instr::New(Js::OpCode::CMP, this->m_func);
    comparePtrInstr->SetSrc1(srcReg1);
    comparePtrInstr->SetSrc2(srcReg2);
    instrInsert->InsertBefore(comparePtrInstr);
    instrInsert->InsertBefore(IR::BranchInstr::New(Js::OpCode::JEQ, labelBranchSuccess, this->m_func));

    IR::Instr * checkFlatString2Instr = IR::Instr::New(Js::OpCode::CMP, this->m_func);
    checkFlatString2Instr->SetSrc1(src2FlatString);
    checkFlatString2Instr->SetSrc2(IR::IntConstOpnd::New(0, TyUint32, this->m_func));
    instrInsert->InsertBefore(checkFlatString2Instr);
    instrInsert->InsertBefore(IR::BranchInstr::New(Js::OpCode::JEQ, labelHelper, this->m_func));

    //      MOV s6,[s4]
    //      CMP [s5], s6                       -First character comparison
    //      JNE $branchfail

    IR::RegOpnd * src1FirstChar = IR::RegOpnd::New(TyUint16, this->m_func);
    IR::Instr * loadSrc1CharInstr = IR::Instr::New(Js::OpCode::MOV, src1FirstChar,
        IR::IndirOpnd::New(src1FlatString, 0, TyUint16,
        this->m_func), this->m_func);
    instrInsert->InsertBefore(loadSrc1CharInstr);

    IR::Instr * compareFirstCharInstr = IR::Instr::New(Js::OpCode::CMP, this->m_func);
    compareFirstCharInstr->SetSrc1(IR::IndirOpnd::New(src2FlatString, 0, TyUint16, this->m_func));
    compareFirstCharInstr->SetSrc2(src1FirstChar);
    instrInsert->InsertBefore(compareFirstCharInstr);
    instrInsert->InsertBefore(IR::BranchInstr::New(Js::OpCode::JNE, labelBranchFail, this->m_func));

    // SHL length, 1
    instrInsert->InsertBefore(IR::Instr::New(Js::OpCode::SHL, src1LengthOpnd, src1LengthOpnd, IR::IntConstOpnd::New(1, TyUint8, this->m_func), this->m_func));

    // eax = memcmp(src1String, src2String, length*2)

    this->LoadHelperArgument(branchInstr, src1LengthOpnd);
    this->LoadHelperArgument(branchInstr, src1FlatString);
    this->LoadHelperArgument(branchInstr, src2FlatString);
    IR::RegOpnd *dstOpnd = IR::RegOpnd::New(TyInt32, this->m_func);
    IR::Instr *instrCall = IR::Instr::New(Js::OpCode::CALL, dstOpnd, IR::HelperCallOpnd::New(IR::HelperMemCmp, this->m_func), this->m_func);
    branchInstr->InsertBefore(instrCall);
    this->LowerCall(instrCall, 3);

    // TEST eax, eax
    IR::Instr *instrTest = IR::Instr::New(Js::OpCode::TEST, this->m_func);
    instrTest->SetSrc1(instrCall->GetDst());
    instrTest->SetSrc2(instrCall->GetDst());
    instrInsert->InsertBefore(instrTest);
    // JEQ success
    instrInsert->InsertBefore(IR::BranchInstr::New(Js::OpCode::JEQ, labelBranchSuccess, this->m_func));
    // JMP fail
    instrInsert->InsertBefore(IR::BranchInstr::New(Js::OpCode::JMP, labelBranchFail, this->m_func));

    branchInstr->InsertBefore(labelHelper);
    IR::LabelInstr *labelFallthrough = IR::LabelInstr::New(Js::OpCode::Label, this->m_func);
    branchInstr->InsertAfter(labelFallthrough);

#if DBG
    // The fast-path for strings assumes the case where 2 strings are equal is rare, and marks that path as 'helper'.
    // This breaks the helper label dbchecks as it can result in non-helper blocks be reachable only from helper blocks.
    // Use m_isHelperToNonHelperBranch and m_noHelperAssert to fix this.
    IR::Instr *blockEndInstr;

    if (isEqual)
    {
        blockEndInstr = labelHelper->GetNextBranchOrLabel();
    }
    else
    {
        blockEndInstr = branchInstr->GetNextBranchOrLabel();
    }

    if (blockEndInstr->IsBranchInstr())
    {
        blockEndInstr->AsBranchInstr()->m_isHelperToNonHelperBranch = true;
    }

    labelFallthrough->m_noHelperAssert = true;
#endif

    return true;
}

///----------------------------------------------------------------------------
///
/// LowererMD::GenerateFastCmSrEqConst
///
///----------------------------------------------------------------------------

bool
LowererMD::GenerateFastCmSrEqConst(IR::Instr *instr)
{
    //
    // Given:
    // s1 = CmSrEq_A s2, s3
    // where either s2 or s3 is 'null', 'true' or 'false'
    //
    // Generate:
    //
    //     CMP s2, s3
    //     JEQ $mov_true
    //     MOV s1, Library.GetFalse()
    //     JMP $done
    // $mov_true:
    //     MOV s1, Library.GetTrue()
    // $done:
    //

    Assert(m_lowerer->IsConstRegOpnd(instr->GetSrc2()->AsRegOpnd()));

    IR::Opnd       *opnd         = instr->GetSrc1();
    IR::RegOpnd    *opndReg      = instr->GetSrc2()->AsRegOpnd();
    IR::LabelInstr *labelMovTrue = IR::LabelInstr::New(Js::OpCode::Label, this->m_func);
    IR::LabelInstr *labelDone    = IR::LabelInstr::New(Js::OpCode::Label, this->m_func);

    if (!opnd->IsRegOpnd())
    {
        IR::RegOpnd *lhsReg = IR::RegOpnd::New(TyVar, m_func);
        IR::Instr *mov = IR::Instr::New(Js::OpCode::MOV, lhsReg, opnd, m_func);
        instr->InsertBefore(mov);

        opnd = lhsReg;
    }

    Assert(opnd->IsRegOpnd());

    // CMP s2, s3
    // JEQ $mov_true
    this->m_lowerer->InsertCompareBranch(opnd, opndReg->m_sym->GetConstOpnd(), Js::OpCode::BrEq_A, labelMovTrue, instr);

    // MOV s1, 'false'
    IR::Instr *instrMov = IR::Instr::New(Js::OpCode::MOV,
                                         instr->GetDst(),
                                         m_lowerer->LoadLibraryValueOpnd(instr, LibraryValue::ValueFalse),
                                         m_func);
    instr->InsertBefore(instrMov);

    // JMP $done
    IR::BranchInstr *jmp = IR::BranchInstr::New(Js::OpCode::JMP, labelDone, this->m_func);
    instr->InsertBefore(jmp);

    // $mov_true:
    instr->InsertBefore(labelMovTrue);

    // MOV s1, 'true'
    instr->m_opcode = Js::OpCode::MOV;
    instr->UnlinkSrc1();
    instr->UnlinkSrc2();
    instr->SetSrc1(m_lowerer->LoadLibraryValueOpnd(instr, LibraryValue::ValueTrue));
    instr->ClearBailOutInfo();
    Legalize(instr);

    // $done:
    instr->InsertAfter(labelDone);

    return true;
}

///----------------------------------------------------------------------------
///
/// LowererMD::GenerateFastCmXxTaggedInt
///
///----------------------------------------------------------------------------
bool LowererMD::GenerateFastCmXxTaggedInt(IR::Instr *instr)
{
    // The idea is to do an inline compare if we can prove that both sources
    // are tagged ints (i.e., are vars with the low bit set).
    //
    // Given:
    //
    //      Cmxx_A dst, src1, src2
    //
    // Generate:
    //
    // (If not Int31's, goto $helper)
    //      MOV r1, src1
    //      if (==, !=, !== or ===)
    //          SUB r1, src2
    //          NEG r1                                  // Sets CF if r1 != 0
    //          SBB r1, r1                              // CF == 1 ? r1 = -1 : r1 = 0
    //      else
    //          MOV r2, 0
    //          CMP r1, src2
    //          SETcc r2
    //          DEC r2
    //          set r1 to r2
    //      AND r1, (notEqualResult - equalResult)
    //      ADD r1, equalResult
    //      MOV dst, r1
    //      JMP $fallthru
    // $helper:
    //      (caller will generate normal helper call sequence)
    // $fallthru:

    IR::Opnd * src1 = instr->GetSrc1();
    IR::Opnd * src2 = instr->GetSrc2();
    IR::Opnd * dst = instr->GetDst();
    IR::RegOpnd * r1 = IR::RegOpnd::New(TyMachReg, m_func);
    IR::LabelInstr * helper = IR::LabelInstr::New(Js::OpCode::Label, m_func, true);
    IR::LabelInstr * fallthru = IR::LabelInstr::New(Js::OpCode::Label, m_func);

    Assert(src1 && src2 && dst);

    // Not tagged ints?
    if (src1->IsRegOpnd() && src1->AsRegOpnd()->IsNotInt())
    {
        return false;
    }
    if (src2->IsRegOpnd() && src2->AsRegOpnd()->IsNotInt())
    {
        return false;
    }

    bool isNeqOp = instr->m_opcode == Js::OpCode::CmSrNeq_A || instr->m_opcode == Js::OpCode::CmNeq_A;
    Js::Var notEqualResult = m_func->GetScriptContext()->GetLibrary()->GetTrueOrFalse(isNeqOp);
    Js::Var equalResult = m_func->GetScriptContext()->GetLibrary()->GetTrueOrFalse(!isNeqOp);

    // Tagged ints?
    bool isTaggedInts = false;
    if (src1->IsTaggedInt())
    {
        if (src2->IsTaggedInt())
        {
            isTaggedInts = true;
        }
    }

    if (!isTaggedInts)
    {
        this->GenerateSmIntPairTest(instr, src1, src2, helper);
    }

    // MOV r1, src1
    instr->InsertBefore(IR::Instr::New(Js::OpCode::MOV, r1, src1, m_func));

    Js::OpCode setCC_Opcode = Js::OpCode::Nop;

    switch(instr->m_opcode)
    {
    case Js::OpCode::CmSrEq_A:
    case Js::OpCode::CmEq_A:
        break;

    case Js::OpCode::CmSrNeq_A:
    case Js::OpCode::CmNeq_A:
        break;

    case Js::OpCode::CmGe_A:
        setCC_Opcode = Js::OpCode::SETGE;
        break;

    case Js::OpCode::CmGt_A:
        setCC_Opcode = Js::OpCode::SETG;
        break;

    case Js::OpCode::CmLe_A:
        setCC_Opcode = Js::OpCode::SETLE;
        break;

    case Js::OpCode::CmLt_A:
        setCC_Opcode = Js::OpCode::SETL;
        break;

    default:
        Assume(UNREACHED);
    }

    if (setCC_Opcode == Js::OpCode::Nop)
    {
        // SUB r1, src2
        IR::Instr * subInstr = IR::Instr::New(Js::OpCode::SUB, r1, r1, src2, m_func);
        instr->InsertBefore(subInstr);
        Legalize(subInstr);         // src2 may need legalizing

        // NEG r1
        instr->InsertBefore(IR::Instr::New(Js::OpCode::NEG, r1, r1, m_func));

        // SBB r1, r1
        instr->InsertBefore(IR::Instr::New(Js::OpCode::SBB, r1, r1, r1, m_func));
    }
    else
    {
        IR::Instr *instrNew;
        IR::RegOpnd *r2 = IR::RegOpnd::New(TyMachPtr, this->m_func);

        // MOV r2, 0
        instr->InsertBefore(IR::Instr::New(Js::OpCode::MOV, r2, IR::IntConstOpnd::New(0, TyInt32, this->m_func), m_func));

        // CMP r1, src2
        IR::Opnd *r1_32 = r1->UseWithNewType(TyInt32, this->m_func);
        IR::Opnd *src2_32 =src2->UseWithNewType(TyInt32, this->m_func);
        instrNew = IR::Instr::New(Js::OpCode::CMP, m_func);
        instrNew->SetSrc1(r1_32);
        instrNew->SetSrc2(src2_32);
        instr->InsertBefore(instrNew);

        // SETcc r2
        IR::RegOpnd *r2_i8 = (IR::RegOpnd*) r2->UseWithNewType(TyInt8, this->m_func);

        instrNew = IR::Instr::New(setCC_Opcode, r2_i8, r2_i8, m_func);
        instr->InsertBefore(instrNew);

        // DEC r2
        instr->InsertBefore(IR::Instr::New(Js::OpCode::DEC, r2, r2, m_func));
        // r1 <- r2
        r1 = r2;
    }

    // AND r1, (notEqualResult - equalResult)
    {
        IR::Instr * and = IR::Instr::New(Js::OpCode::AND, r1, r1, m_func);
        and->SetSrc2(IR::AddrOpnd::New((void*)((size_t)notEqualResult - (size_t)equalResult), IR::AddrOpndKind::AddrOpndKindDynamicMisc, this->m_func));
        instr->InsertBefore(and);
        Legalize(and);
    }

    // ADD r1, equalResult
    {
        IR::Instr * add = IR::Instr::New(Js::OpCode::ADD, r1, r1, m_func);
        add->SetSrc2(IR::AddrOpnd::New(equalResult, IR::AddrOpndKind::AddrOpndKindDynamicVar, this->m_func));
        instr->InsertBefore(add);
        Legalize(add);
    }

    // MOV dst, r1
    instr->InsertBefore(IR::Instr::New(Js::OpCode::MOV, dst, r1, m_func));

    if (isTaggedInts)
    {
        instr->Remove();
        return true;
    }

    // JMP $fallthru
    instr->InsertBefore(IR::BranchInstr::New(Js::OpCode::JMP, fallthru, m_func));

    instr->InsertBefore(helper);
    instr->InsertAfter(fallthru);

    return false;
}

void LowererMD::GenerateFastCmXxR8(IR::Instr *instr)
{
    GenerateFastCmXx(instr);
}

void LowererMD::GenerateFastCmXxI4(IR::Instr *instr)
{
    GenerateFastCmXx(instr);
}

void LowererMD::GenerateFastCmXx(IR::Instr *instr)
{
    // For float src:
    // dst = MOV 0/1
    // (U)COMISD src1, src2
    // JP $done
    // dst.i8 = SetCC dst.i8
    // $done:

    // for int src:
    // CMP src1, src2
    // dst = MOV 0 / false
    // dst.i8 = SetCC dst.i8  / CMOCcc true

    IR::Opnd * src1 = instr->UnlinkSrc1();
    IR::Opnd * src2 = instr->UnlinkSrc2();
    IR::Opnd * dst = instr->UnlinkDst();
    IR::Opnd * tmp = dst;
    bool isIntDst = dst->AsRegOpnd()->m_sym->IsInt32();
    bool isFloatSrc = src1->IsFloat();
    Assert(!isFloatSrc || src2->IsFloat());
    Assert(!isFloatSrc || isIntDst);
    Assert(!isFloatSrc || AutoSystemInfo::Data.SSE2Available());
    IR::Opnd *opnd;
    IR::Instr *newInstr;

    Assert(src1->IsRegOpnd());

    IR::Instr * done;
    if (isFloatSrc)
    {
        done = IR::LabelInstr::New(Js::OpCode::Label, m_func);
        instr->InsertBefore(done);
    }
    else
    {
        done = instr;
    }

    if (isIntDst)
    {
        // reg = MOV 0 will get peeped to XOR reg, reg which sets the flags.
        // Put the MOV before the CMP, but use a tmp if dst == src1/src2
        if (dst->IsEqual(src1) || dst->IsEqual(src2))
        {
            tmp = IR::RegOpnd::New(dst->GetType(), this->m_func);
        }
        // dst = MOV 0
        if (isFloatSrc && instr->m_opcode == Js::OpCode::CmNeq_A)
        {
            opnd = IR::IntConstOpnd::New(1, TyInt32, this->m_func);
        }
        else
        {
            opnd = IR::IntConstOpnd::New(0, TyInt32, this->m_func);
        }
        newInstr = IR::Instr::New(Js::OpCode::MOV, tmp, opnd, this->m_func);
        done->InsertBefore(newInstr);
    }

    Js::OpCode cmpOp;
    if (isFloatSrc)
    {
        if (instr->m_opcode == Js::OpCode::CmEq_A || instr->m_opcode == Js::OpCode::CmNeq_A)
        {
            cmpOp = src1->IsFloat64() ? Js::OpCode::UCOMISD : Js::OpCode::UCOMISS;
        }
        else
        {
            cmpOp = src1->IsFloat64() ? Js::OpCode::COMISD : Js::OpCode::COMISS;
        }
    }
    else
    {
        cmpOp = Js::OpCode::CMP;
    }
    // CMP src1, src2
    newInstr = IR::Instr::New(cmpOp, this->m_func);
    newInstr->SetSrc1(src1);
    newInstr->SetSrc2(src2);
    done->InsertBefore(newInstr);

    if (isFloatSrc)
    {
        newInstr = IR::BranchInstr::New(Js::OpCode::JP, done->AsLabelInstr(), this->m_func);
        done->InsertBefore(newInstr);
    }

    if (!isIntDst)
    {
        opnd = this->m_lowerer->LoadLibraryValueOpnd(instr, LibraryValue::ValueFalse);
        LowererMD::CreateAssign(tmp, opnd, done);
    }

    Js::OpCode useCC;
    switch(instr->m_opcode)
    {
    case Js::OpCode::CmEq_I4:
    case Js::OpCode::CmEq_A:
        useCC = isIntDst ? Js::OpCode::SETE : Js::OpCode::CMOVE;
        break;

    case Js::OpCode::CmNeq_I4:
    case Js::OpCode::CmNeq_A:
        useCC = isIntDst ? Js::OpCode::SETNE : Js::OpCode::CMOVNE;
        break;

    case Js::OpCode::CmGe_I4:
        useCC = isIntDst ? Js::OpCode::SETGE : Js::OpCode::CMOVGE;
        break;

    case Js::OpCode::CmGt_I4:
        useCC = isIntDst ? Js::OpCode::SETG : Js::OpCode::CMOVG;
        break;

    case Js::OpCode::CmLe_I4:
        useCC = isIntDst ? Js::OpCode::SETLE : Js::OpCode::CMOVLE;
        break;

    case Js::OpCode::CmLt_I4:
        useCC = isIntDst ? Js::OpCode::SETL : Js::OpCode::CMOVL;
        break;

    case Js::OpCode::CmUnGe_I4:
    case Js::OpCode::CmGe_A:
        useCC = isIntDst ? Js::OpCode::SETAE : Js::OpCode::CMOVAE;
        break;

    case Js::OpCode::CmUnGt_I4:
    case Js::OpCode::CmGt_A:
        useCC = isIntDst ? Js::OpCode::SETA : Js::OpCode::CMOVA;
        break;

    case Js::OpCode::CmUnLe_I4:
    case Js::OpCode::CmLe_A:
        useCC = isIntDst ? Js::OpCode::SETBE : Js::OpCode::CMOVBE;
        break;

    case Js::OpCode::CmUnLt_I4:
    case Js::OpCode::CmLt_A:
        useCC = isIntDst ? Js::OpCode::SETB : Js::OpCode::CMOVB;
        break;

    default:
        useCC = Js::OpCode::InvalidOpCode;
        Assume(UNREACHED);
    }

    if (isIntDst)
    {
        // tmp.i8 = SetCC tmp.i8
        IR::Opnd *tmp_i8 = tmp->UseWithNewType(TyInt8, this->m_func);

        newInstr = IR::Instr::New(useCC, tmp_i8, tmp_i8, this->m_func);
    }
    else
    {
        // regTrue = MOV true
        IR::Opnd *regTrue = IR::RegOpnd::New(TyMachPtr, this->m_func);
        Lowerer::InsertMove(regTrue, this->m_lowerer->LoadLibraryValueOpnd(instr, LibraryValue::ValueTrue), done);

        // tmp = CMOVcc tmp, regTrue
        newInstr = IR::Instr::New(useCC, tmp, tmp, regTrue, this->m_func);
    }
    done->InsertBefore(newInstr);

    if (tmp != dst)
    {
        newInstr = IR::Instr::New(Js::OpCode::MOV, dst, tmp, this->m_func);
        instr->InsertBefore(newInstr);
    }

    instr->Remove();
}

IR::Instr * LowererMD::GenerateConvBool(IR::Instr *instr)
{
    // TEST src1, src1
    // dst = MOV true
    // rf = MOV false
    // dst = CMOV dst, rf

    IR::Instr *instrNew, *instrFirst;
    IR::RegOpnd *dst = instr->GetDst()->AsRegOpnd();
    IR::RegOpnd *regFalse;

    // TEST src1, src2
    instrFirst = instrNew = IR::Instr::New(Js::OpCode::TEST, this->m_func);
    instrNew->SetSrc1(instr->GetSrc1());
    instrNew->SetSrc2(instr->GetSrc1());
    instr->InsertBefore(instrNew);

    // dst = MOV true
    Lowerer::InsertMove(dst, this->m_lowerer->LoadLibraryValueOpnd(instr, LibraryValue::ValueTrue), instr);

    // rf = MOV false
    regFalse = IR::RegOpnd::New(TyMachPtr, this->m_func);
    Lowerer::InsertMove(regFalse, this->m_lowerer->LoadLibraryValueOpnd(instr, LibraryValue::ValueFalse), instr);

    // Add dst as src1 of CMOV to create a pseudo use of dst.  Otherwise, the register allocator
    // won't know the previous dst is needed. and needed in the same register as the dst of the CMOV.

    // dst = CMOV dst, rf
    instrNew = IR::Instr::New(Js::OpCode::CMOVE, dst, dst, regFalse, this->m_func);
    instr->InsertBefore(instrNew);

    instr->Remove();

    return instrFirst;
}

///----------------------------------------------------------------------------
///
/// LowererMD::GenerateFastAdd
///
/// NOTE: We assume that only the sum of two Int31's will have 0x2 set. This
/// is only true until we have an var type with tag == 0x2.
///
///----------------------------------------------------------------------------
bool
LowererMD::GenerateFastAdd(IR::Instr * instrAdd)
{
    // Given:
    //
    // dst = Add src1, src2
    //
    // Generate:
    //
    // (If not 2 Int31's, jump to $helper.)
    // s1 = MOV src1
    // s1 = DEC s1          -- Get rid of one of the tag [Int31 only]
    // s1 = ADD s1, src2    -- try an inline add
    //      JO $helper      -- bail if the add overflowed
    // s1 = OR s1, AtomTag_IntPtr                        [Int32 only]
    // dst = MOV s1
    //      JMP $fallthru
    // $helper:
    //      (caller generates helper call)
    // $fallthru:

    IR::Instr *      instr;
    IR::LabelInstr * labelHelper;
    IR::LabelInstr * labelFallThru;
    IR::Opnd *       opndReg;
    IR::Opnd *       opndSrc1;
    IR::Opnd *       opndSrc2;

    opndSrc1 = instrAdd->GetSrc1();
    opndSrc2 = instrAdd->GetSrc2();
    AssertMsg(opndSrc1 && opndSrc2, "Expected 2 src opnd's on Add instruction");

    // Generate fastpath for Incr_A anyway -
    // Incrementing strings representing integers can be inter-mixed with integers e.g. "1"++ -> converts 1 to an int and thereafter, integer increment is expected.
    if (opndSrc1->IsRegOpnd() && (opndSrc1->AsRegOpnd()->IsNotInt() || opndSrc1->GetValueType().IsString()
        || (instrAdd->m_opcode != Js::OpCode::Incr_A && opndSrc1->GetValueType().IsLikelyString())))
    {
        return false;
    }

    if (opndSrc2->IsRegOpnd() && (opndSrc2->AsRegOpnd()->IsNotInt() ||
        opndSrc2->GetValueType().IsLikelyString()))
    {
        return false;
    }

    // Tagged ints?
    bool isTaggedInts = false;
    if (opndSrc1->IsTaggedInt())
    {
        if (opndSrc2->IsTaggedInt())
        {
            isTaggedInts = true;
        }
    }

    labelHelper = IR::LabelInstr::New(Js::OpCode::Label, this->m_func, true);

    if (!isTaggedInts)
    {
        // (If not 2 Int31's, jump to $helper.)

        this->GenerateSmIntPairTest(instrAdd, opndSrc1, opndSrc2, labelHelper);
    }

    if (opndSrc1->IsAddrOpnd())
    {
        // If opnd1 is a constant, just swap them.
        IR::Opnd *opndTmp = opndSrc1;
        opndSrc1 = opndSrc2;
        opndSrc2 = opndTmp;
    }

    //
    // For 32 bit arithmetic we copy them and set the size of operands to be 32 bits. This is
    // relevant only on AMD64.
    //

    opndSrc1    = opndSrc1->UseWithNewType(TyInt32, this->m_func);

    // s1 = MOV src1

    opndReg = IR::RegOpnd::New(TyInt32, this->m_func);
    instr = IR::Instr::New(Js::OpCode::MOV, opndReg, opndSrc1, this->m_func);
    instrAdd->InsertBefore(instr);

#if !INT32VAR
    // Do the DEC in place
    if (opndSrc2->IsAddrOpnd())
    {
        Assert(opndSrc2->AsAddrOpnd()->GetAddrOpndKind() == IR::AddrOpndKindConstantVar);
        opndSrc2 = IR::IntConstOpnd::New(*((int *)&(opndSrc2->AsAddrOpnd()->m_address)) - 1, TyInt32, this->m_func, opndSrc2->AsAddrOpnd()->m_dontEncode);
        opndSrc2 = opndSrc2->Use(this->m_func);
    }
    else if (opndSrc2->IsIntConstOpnd())
    {
        Assert(opndSrc2->GetType() == TyInt32);
        opndSrc2 = opndSrc2->Use(this->m_func);
        opndSrc2->AsIntConstOpnd()->DecrValue(1);
    }
    else
    {
        // s1 = DEC s1
        opndSrc2 = opndSrc2->UseWithNewType(TyInt32, this->m_func);
        instr = IR::Instr::New(Js::OpCode::DEC, opndReg, opndReg, this->m_func);
        instrAdd->InsertBefore(instr);
    }

    instr = IR::Instr::New(Js::OpCode::ADD, opndReg, opndReg, opndSrc2, this->m_func);
#else
    if (opndSrc2->IsAddrOpnd())
    {
        // truncate to untag
        int value = ::Math::PointerCastToIntegralTruncate<int>(opndSrc2->AsAddrOpnd()->m_address);
        if (value == 1)
        {
            instr = IR::Instr::New(Js::OpCode::INC, opndReg, opndReg, this->m_func);
        }
        else
        {
            opndSrc2 = IR::IntConstOpnd::New(value, TyInt32, this->m_func);
            instr = IR::Instr::New(Js::OpCode::ADD, opndReg, opndReg, opndSrc2, this->m_func);
        }
    }
    else
    {
        instr = IR::Instr::New(Js::OpCode::ADD, opndReg, opndReg, opndSrc2, this->m_func);
    }
#endif

    // s1 = ADD s1, src2
    instrAdd->InsertBefore(instr);
    Legalize(instr);

    //      JO $helper

    instr = IR::BranchInstr::New(Js::OpCode::JO, labelHelper, this->m_func);
    instrAdd->InsertBefore(instr);

    //
    // Convert TyInt32 operand, back to TyMachPtr type.
    //

    if(TyMachReg != opndReg->GetType())
    {
        opndReg = opndReg->UseWithNewType(TyMachPtr, this->m_func);
    }

#if INT32VAR
    // s1 = OR s1, AtomTag_IntPtr
    GenerateInt32ToVarConversion(opndReg, instrAdd);
#endif

    // dst = MOV s1

    instr = IR::Instr::New(Js::OpCode::MOV, instrAdd->GetDst(), opndReg, this->m_func);
    instrAdd->InsertBefore(instr);

    //      JMP $fallthru

    labelFallThru = IR::LabelInstr::New(Js::OpCode::Label, this->m_func);
    instr = IR::BranchInstr::New(Js::OpCode::JMP, labelFallThru, this->m_func);
    instrAdd->InsertBefore(instr);

    // $helper:
    //      (caller generates helper call)
    // $fallthru:

    instrAdd->InsertBefore(labelHelper);
    instrAdd->InsertAfter(labelFallThru);

    return true;
}

///----------------------------------------------------------------------------
///
/// LowererMD::GenerateFastSub
///
///
///----------------------------------------------------------------------------
bool
LowererMD::GenerateFastSub(IR::Instr * instrSub)
{
    // Given:
    //
    // dst = Sub src1, src2
    //
    // Generate:
    //
    // (If not 2 Int31's, jump to $helper.)
    // s1 = MOV src1
    // s1 = SUB s1, src2    -- try an inline sub
    //      JO $helper      -- bail if the subtract overflowed
    //      JNE $helper
    // s1 = INC s1          -- restore the var tag on the result [Int31 only]
    // s1 = OR s1, AtomTag_IntPtr                                [Int32 only]
    // dst = MOV s1
    //      JMP $fallthru
    // $helper:
    //      (caller generates helper call)
    // $fallthru:

    IR::Instr *      instr;
    IR::LabelInstr * labelHelper;
    IR::LabelInstr * labelFallThru;
    IR::Opnd *       opndReg;
    IR::Opnd *       opndSrc1;
    IR::Opnd *       opndSrc2;

    opndSrc1        =   instrSub->GetSrc1();
    opndSrc2        =   instrSub->GetSrc2();
    AssertMsg(opndSrc1 && opndSrc2, "Expected 2 src opnd's on Sub instruction");

    // Not tagged ints?
    if (opndSrc1->IsRegOpnd() && opndSrc1->AsRegOpnd()->IsNotInt())
    {
        return false;
    }
    if (opndSrc2->IsRegOpnd() && opndSrc2->AsRegOpnd()->IsNotInt())
    {
        return false;
    }

    // Tagged ints?
    bool isTaggedInts = false;
    if (opndSrc1->IsTaggedInt())
    {
        if (opndSrc2->IsTaggedInt())
        {
            isTaggedInts = true;
        }
    }

    labelHelper = IR::LabelInstr::New(Js::OpCode::Label, this->m_func, true);

    if (!isTaggedInts)
    {
        // (If not 2 Int31's, jump to $helper.)

        this->GenerateSmIntPairTest(instrSub, opndSrc1, opndSrc2, labelHelper);
    }


    //
    // For 32 bit arithmetic we copy them and set the size of operands to be 32 bits. This is
    // relevant only on AMD64.
    //

    opndSrc1    = opndSrc1->UseWithNewType(TyInt32, this->m_func);
    opndSrc2    = opndSrc2->UseWithNewType(TyInt32, this->m_func);


    // s1 = MOV src1

    opndReg = IR::RegOpnd::New(TyInt32, this->m_func);
    instr = IR::Instr::New(Js::OpCode::MOV, opndReg, opndSrc1, this->m_func);
    instrSub->InsertBefore(instr);

    // s1 = SUB s1, src2

    instr = IR::Instr::New(Js::OpCode::SUB, opndReg, opndReg, opndSrc2, this->m_func);
    instrSub->InsertBefore(instr);

    //      JO $helper

    instr = IR::BranchInstr::New(Js::OpCode::JO, labelHelper, this->m_func);
    instrSub->InsertBefore(instr);

#if !INT32VAR
    // s1 = INC s1

    instr = IR::Instr::New(Js::OpCode::INC, opndReg, opndReg, this->m_func);
    instrSub->InsertBefore(instr);
#endif
    //
    // Convert TyInt32 operand, back to TyMachPtr type.
    //

    if(TyMachReg != opndReg->GetType())
    {
        opndReg = opndReg->UseWithNewType(TyMachPtr, this->m_func);
    }

#if INT32VAR
    // s1 = OR s1, AtomTag_IntPtr
    GenerateInt32ToVarConversion(opndReg, instrSub);
#endif

    // dst = MOV s1

    instr = IR::Instr::New(Js::OpCode::MOV, instrSub->GetDst(), opndReg, this->m_func);
    instrSub->InsertBefore(instr);

    //      JMP $fallthru

    labelFallThru = IR::LabelInstr::New(Js::OpCode::Label, this->m_func);
    instr = IR::BranchInstr::New(Js::OpCode::JMP, labelFallThru, this->m_func);
    instrSub->InsertBefore(instr);

    // $helper:
    //      (caller generates helper call)
    // $fallthru:

    instrSub->InsertBefore(labelHelper);
    instrSub->InsertAfter(labelFallThru);

    return true;
}

///----------------------------------------------------------------------------
///
/// LowererMD::GenerateFastMul
///
///----------------------------------------------------------------------------

bool
LowererMD::GenerateFastMul(IR::Instr * instrMul)
{
    // Given:
    //
    // dst = Mul src1, src2
    //
    // Generate:
    //
    // (If not 2 Int31's, jump to $helper.)
    // s1 = MOV src1
    // s1 = DEC s1          -- clear the var tag from the value to be multiplied   [Int31 only]
    // s2 = MOV src2
    // s2 = SAR s2, Js::VarTag_Shift  -- extract the real src2 amount from the var [Int31 only]
    // s1 = IMUL s1, s2     -- do the signed mul
    //      JO $helper      -- bail if the result overflowed
    // s3 = MOV s1
    //      TEST s3, s3     -- Check result is 0. might be -0. Result is -0 when a negative number is multiplied with 0.
    //      JEQ $zero
    //      JMP $nonzero
    // $zero:               -- result of mul was 0. try to check for -0
    // s2 = ADD s2, src1    -- Add src1 to s2
    //      JGT $nonzero    -- positive 0.                                          [Int31 only]
    //      JGE $nonzero    -- positive 0.                                          [Int32 only]
    // dst = ToVar(-0.0)    -- load negative 0
    //      JMP $fallthru
    // $nonzero:
    // s3 = INC s3          -- restore the var tag on the result                    [Int31 only]
    // s3 = OR s3, AtomTag_IntPtr                                                   [Int32 only]
    // dst= MOV s3
    //      JMP $fallthru
    // $helper:
    //      (caller generates helper call)
    // $fallthru:

    IR::LabelInstr * labelHelper;
    IR::LabelInstr * labelFallThru;
    IR::LabelInstr * labelNonZero;
    IR::Instr *      instr;
    IR::RegOpnd *    opndReg1;
    IR::RegOpnd *    opndReg2;
    IR::RegOpnd *    s3;
    IR::Opnd *       opndSrc1;
    IR::Opnd *       opndSrc2;

    opndSrc1 = instrMul->GetSrc1();
    opndSrc2 = instrMul->GetSrc2();
    AssertMsg(opndSrc1 && opndSrc2, "Expected 2 src opnd's on mul instruction");

    if (opndSrc1->IsRegOpnd() && opndSrc1->AsRegOpnd()->IsNotInt())
    {
        return true;
    }
    if (opndSrc2->IsRegOpnd() && opndSrc2->AsRegOpnd()->IsNotInt())
    {
        return true;
    }
    // (If not 2 Int31's, jump to $helper.)

    labelHelper = IR::LabelInstr::New(Js::OpCode::Label, this->m_func, true);
    labelNonZero = IR::LabelInstr::New(Js::OpCode::Label, this->m_func);
    labelFallThru = IR::LabelInstr::New(Js::OpCode::Label, this->m_func);

    this->GenerateSmIntPairTest(instrMul, opndSrc1, opndSrc2, labelHelper);


    //
    // For 32 bit arithmetic we copy them and set the size of operands to be 32 bits. This is
    // relevant only on AMD64.
    //

    opndSrc1    = opndSrc1->UseWithNewType(TyInt32, this->m_func);
    opndSrc2    = opndSrc2->UseWithNewType(TyInt32, this->m_func);

    if (opndSrc1->IsImmediateOpnd())
    {
        IR::Opnd * temp = opndSrc1;
        opndSrc1 = opndSrc2;
        opndSrc2 = temp;
    }

    // s1 = MOV src1

    opndReg1 = IR::RegOpnd::New(TyInt32, this->m_func);
    instr = IR::Instr::New(Js::OpCode::MOV, opndReg1, opndSrc1, this->m_func);
    instrMul->InsertBefore(instr);

#if !INT32VAR
    // s1 = DEC s1

    instr = IR::Instr::New(Js::OpCode::DEC, opndReg1, opndReg1, this->m_func);
    instrMul->InsertBefore(instr);
#endif

    if (opndSrc2->IsImmediateOpnd())
    {
        Assert(opndSrc2->IsAddrOpnd() && opndSrc2->AsAddrOpnd()->IsVar());

        IR::Opnd *opnd2 = IR::IntConstOpnd::New(Js::TaggedInt::ToInt32(opndSrc2->AsAddrOpnd()->m_address), TyInt32, this->m_func);

        // s2 = MOV src2

        opndReg2 = IR::RegOpnd::New(TyInt32, this->m_func);
        instr = IR::Instr::New(Js::OpCode::MOV, opndReg2, opnd2, this->m_func);
        instrMul->InsertBefore(instr);
    }
    else
    {
        // s2 = MOV src2

        opndReg2 = IR::RegOpnd::New(TyInt32, this->m_func);
        instr = IR::Instr::New(Js::OpCode::MOV, opndReg2, opndSrc2, this->m_func);
        instrMul->InsertBefore(instr);
#if !INT32VAR
        // s2 = SAR s2, Js::VarTag_Shift
        instr = IR::Instr::New(
            Js::OpCode::SAR, opndReg2, opndReg2,
            IR::IntConstOpnd::New(Js::VarTag_Shift, TyInt8, this->m_func), this->m_func);
        instrMul->InsertBefore(instr);
#endif
    }

    // s1 = IMUL s1, s2

    instr = IR::Instr::New(Js::OpCode::IMUL2, opndReg1, opndReg1, opndReg2, this->m_func);
    instrMul->InsertBefore(instr);

    //      JO $helper

    instr = IR::BranchInstr::New(Js::OpCode::JO, labelHelper, this->m_func);
    instrMul->InsertBefore(instr);

    //      MOV s3, s1

    s3 = IR::RegOpnd::New(TyInt32, this->m_func);
    instr = IR::Instr::New(Js::OpCode::MOV, s3, opndReg1, this->m_func);
    instrMul->InsertBefore(instr);

    //      TEST s3, s3

    instr = IR::Instr::New(Js::OpCode::TEST, this->m_func);
    instr->SetSrc1(s3);
    instr->SetSrc2(s3);
    instrMul->InsertBefore(instr);

    //      JEQ $zero

    IR::LabelInstr *labelZero = IR::LabelInstr::New(Js::OpCode::Label, this->m_func, true);
    instr = IR::BranchInstr::New(Js::OpCode::JEQ, labelZero, this->m_func);
    instrMul->InsertBefore(instr);

    //      JMP $nonzero

    instr = IR::BranchInstr::New(Js::OpCode::JMP, labelNonZero, this->m_func);
    instrMul->InsertBefore(instr);

    // $zero:

    instrMul->InsertBefore(labelZero);

    // s2 = ADD s2, src1

    instr = IR::Instr::New(Js::OpCode::ADD, opndReg2, opndReg2, opndSrc1, this->m_func);
    instrMul->InsertBefore(instr);
    Legalize(instr);

    //      JGT $nonzero
#if INT32VAR
    Js::OpCode greaterOpCode = Js::OpCode::JGE;
#else
    Js::OpCode greaterOpCode = Js::OpCode::JGT;
#endif
    instr = IR::BranchInstr::New(greaterOpCode, labelNonZero, this->m_func);
    instrMul->InsertBefore(instr);

    // dst = ToVar(-0.0)    -- load negative 0

    instr = IR::Instr::New(Js::OpCode::MOV, instrMul->GetDst(), m_lowerer->LoadLibraryValueOpnd(instrMul, LibraryValue::ValueNegativeZero), this->m_func);
    instrMul->InsertBefore(instr);

    //      JMP $fallthru

    instr = IR::BranchInstr::New(Js::OpCode::JMP, labelFallThru, this->m_func);
    instrMul->InsertBefore(instr);


    // $nonzero:

    instrMul->InsertBefore(labelNonZero);
#if !INT32VAR
    // s3 = INC s3

    instr = IR::Instr::New(Js::OpCode::INC, s3, s3, this->m_func);
    instrMul->InsertBefore(instr);
#endif

    //
    // Convert TyInt32 operand, back to TyMachPtr type.
    // Cast is fine. We know ChangeType returns IR::Opnd * but it
    // preserves the Type.
    //

    if(TyMachReg != s3->GetType())
    {
        s3 = static_cast<IR::RegOpnd *>(s3->UseWithNewType(TyMachPtr, this->m_func));
    }

#if INT32VAR
    // s3 = OR s3, AtomTag_IntPtr

    GenerateInt32ToVarConversion(s3, instrMul);
#endif

    // dst = MOV s3

    instr = IR::Instr::New(Js::OpCode::MOV, instrMul->GetDst(), s3, this->m_func);
    instrMul->InsertBefore(instr);

    //      JMP $fallthru

    instr = IR::BranchInstr::New(Js::OpCode::JMP, labelFallThru, this->m_func);
    instrMul->InsertBefore(instr);

    // $helper:
    //      (caller generates helper call)
    // $fallthru:

    instrMul->InsertBefore(labelHelper);
    instrMul->InsertAfter(labelFallThru);

    return true;
}

bool
LowererMD::GenerateFastNeg(IR::Instr * instrNeg)
{
    // Given:
    //
    // dst = Not src
    //
    // Generate:
    //
    //       if not int, jump $helper
    //       if src == 0    -- test for zero (must be handled by the runtime to preserve
    //       JEQ $helper          difference btw +0 and -0)
    // dst = MOV src
    // dst = NEG dst        -- do an inline NEG
    // dst = ADD dst, 2     -- restore the var tag on the result             [int31 only]
    //       JO $helper
    // dst = OR dst, AtomTag_Ptr                                             [int32 only]
    //       JMP $fallthru
    // $helper:
    //      (caller generates helper call)
    // $fallthru:

    IR::Instr *      instr;
    IR::LabelInstr * labelHelper = nullptr;
    IR::LabelInstr * labelFallThru = nullptr;
    IR::Opnd *       opndSrc1;
    IR::Opnd *       opndDst;
    bool usingNewDst = false;
    opndSrc1 = instrNeg->GetSrc1();
    AssertMsg(opndSrc1, "Expected src opnd on Neg instruction");

    if(opndSrc1->IsEqual(instrNeg->GetDst()))
    {
        usingNewDst = true;
        opndDst = IR::RegOpnd::New(TyInt32, this->m_func);
    }
    else
    {
        opndDst = instrNeg->GetDst()->UseWithNewType(TyInt32, this->m_func);
    }

    if (opndSrc1->IsRegOpnd() && opndSrc1->AsRegOpnd()->m_sym->IsIntConst())
    {
        IR::Opnd *newOpnd;
        IntConstType value = opndSrc1->AsRegOpnd()->m_sym->GetIntConstValue();

        if (value == 0)
        {
            // If the negate operand is zero, the result is -0.0, which is a Number rather than an Int31.
            newOpnd = m_lowerer->LoadLibraryValueOpnd(instrNeg, LibraryValue::ValueNegativeZero);
        }
        else
        {
            // negation below can overflow because max negative int32 value > max positive value by 1.
            newOpnd = IR::AddrOpnd::NewFromNumber(-(int64)value, m_func);
        }

        instrNeg->ClearBailOutInfo();
        instrNeg->FreeSrc1();
        instrNeg->SetSrc1(newOpnd);
        instrNeg = this->ChangeToAssign(instrNeg);

        // Skip lowering call to helper
        return false;
    }

    bool isInt = (opndSrc1->IsTaggedInt());

    if (opndSrc1->IsRegOpnd() && opndSrc1->AsRegOpnd()->IsNotInt())
    {
        return true;
    }

    labelHelper = IR::LabelInstr::New(Js::OpCode::Label, this->m_func, true);
    if (!isInt)
    {
        GenerateSmIntTest(opndSrc1, instrNeg, labelHelper);
    }

    //
    // For 32 bit arithmetic we copy them and set the size of operands to be 32 bits. This is
    // relevant only on AMD64.
    //
    opndSrc1    = opndSrc1->UseWithNewType(TyInt32, this->m_func);
    GenerateTaggedZeroTest(opndSrc1, instrNeg, labelHelper);

    // dst = MOV src

    instr = IR::Instr::New(Js::OpCode::MOV, opndDst, opndSrc1, this->m_func);
    instrNeg->InsertBefore(instr);

    // dst = NEG dst

    instr = IR::Instr::New(Js::OpCode::NEG, opndDst, opndDst, this->m_func);
    instrNeg->InsertBefore(instr);

#if !INT32VAR
    // dst = ADD dst, 2

    instr = IR::Instr::New(Js::OpCode::ADD, opndDst, opndDst, IR::IntConstOpnd::New(2, TyInt32, this->m_func), this->m_func);
    instrNeg->InsertBefore(instr);
#endif

    // JO $helper

    instr = IR::BranchInstr::New(Js::OpCode::JO, labelHelper, this->m_func);
    instrNeg->InsertBefore(instr);

    //
    // Convert TyInt32 operand, back to TyMachPtr type.
    //
    if(TyMachReg != opndDst->GetType())
    {
        opndDst = opndDst->UseWithNewType(TyMachPtr, this->m_func);
    }

#if INT32VAR
    GenerateInt32ToVarConversion(opndDst, instrNeg);
#endif
    if(usingNewDst)
    {
        instr = IR::Instr::New(Js::OpCode::MOV, instrNeg->GetDst(), opndDst, this->m_func);
        instrNeg->InsertBefore(instr);
    }

    // JMP $fallthru

    labelFallThru = IR::LabelInstr::New(Js::OpCode::Label, this->m_func);
    instr = IR::BranchInstr::New(Js::OpCode::JMP, labelFallThru, this->m_func);
    instrNeg->InsertBefore(instr);




    // $helper:
    //      (caller generates helper sequence)
    // $fallthru:

    AssertMsg(labelHelper, "Should not be NULL");
    instrNeg->InsertBefore(labelHelper);
    instrNeg->InsertAfter(labelFallThru);

    return true;
}

void
LowererMD::GenerateFastBrS(IR::BranchInstr *brInstr)
{
    IR::Opnd *src1 = brInstr->UnlinkSrc1();

    Assert(src1->IsIntConstOpnd() || src1->IsAddrOpnd() || src1->IsRegOpnd());

    IR::Instr *cmpInstr = IR::Instr::New(Js::OpCode::TEST, this->m_func);
    cmpInstr->SetSrc1(m_lowerer->LoadOptimizationOverridesValueOpnd(brInstr, OptimizationOverridesValue::OptimizationOverridesSideEffects));
    cmpInstr->SetSrc2(src1);

    brInstr->InsertBefore(cmpInstr);
    Legalize(cmpInstr);

    Js::OpCode opcode;

    switch(brInstr->m_opcode)
    {
    case Js::OpCode::BrHasSideEffects:
        opcode = Js::OpCode::JNE;
        break;

    case Js::OpCode::BrNotHasSideEffects:
        opcode = Js::OpCode::JEQ;
        break;

    default:
        Assert(UNREACHED);
        __assume(false);
    }

    brInstr->m_opcode = opcode;
}

///----------------------------------------------------------------------------
///
/// LowererMD::GenerateSmIntPairTest
///
///     Generate code to test whether the given operands are both Int31 vars
/// and branch to the given label if not.
///
///----------------------------------------------------------------------------
#if !INT32VAR
IR::Instr *
LowererMD::GenerateSmIntPairTest(
    IR::Instr * instrInsert,
    IR::Opnd * opndSrc1,
    IR::Opnd * opndSrc2,
    IR::LabelInstr * labelFail)
{
    IR::Opnd *           opndReg;
    IR::Instr *          instrPrev = instrInsert->m_prev;
    IR::Instr *          instr;

    Assert(opndSrc1->GetType() == TyVar);
    Assert(opndSrc2->GetType() == TyVar);

    if (opndSrc1->IsTaggedInt())
    {
        IR::Opnd *tempOpnd = opndSrc1;
        opndSrc1 = opndSrc2;
        opndSrc2 = tempOpnd;
    }

    if (opndSrc2->IsTaggedInt())
    {
        if (opndSrc1->IsTaggedInt())
        {
            return instrPrev;
        }

        //      TEST src1, AtomTag
        //      JEQ $fail

        instr = IR::Instr::New(Js::OpCode::TEST, this->m_func);
        instr->SetSrc1(opndSrc1);
        instr->SetSrc2(IR::IntConstOpnd::New(Js::AtomTag, TyInt8, this->m_func));
        instrInsert->InsertBefore(instr);
    }
    else
    {

        // s1 = MOV src1
        // s1 = AND s1, 1
        //      TEST s1, src2
        //      JEQ $fail

        // s1 = MOV src1

        opndReg = IR::RegOpnd::New(TyMachReg, this->m_func);
        instr = IR::Instr::New(Js::OpCode::MOV, opndReg, opndSrc1, this->m_func);
        instrInsert->InsertBefore(instr);

        // s1 = AND s1, AtomTag

        instr = IR::Instr::New(
            Js::OpCode::AND, opndReg, opndReg, IR::IntConstOpnd::New(Js::AtomTag, TyInt8, this->m_func), this->m_func);
        instrInsert->InsertBefore(instr);

        //      TEST s1, src2

        instr = IR::Instr::New(Js::OpCode::TEST, this->m_func);
        instr->SetSrc1(opndReg);
        instr->SetSrc2(opndSrc2);
        instrInsert->InsertBefore(instr);
    }

    //      JEQ $fail

    instr = IR::BranchInstr::New(Js::OpCode::JEQ, labelFail, this->m_func);
    instrInsert->InsertBefore(instr);

    return instrPrev;
}
#else
IR::Instr *
LowererMD::GenerateSmIntPairTest(
    IR::Instr * instrInsert,
    IR::Opnd * opndSrc1,
    IR::Opnd * opndSrc2,
    IR::LabelInstr * labelFail)
{
    IR::Opnd *           opndReg;
    IR::Instr *          instrPrev = instrInsert->m_prev;
    IR::Instr *          instr;

    Assert(opndSrc1->GetType() == TyVar);
    Assert(opndSrc2->GetType() == TyVar);

    if (opndSrc1->IsTaggedInt())
    {
        IR::Opnd *tempOpnd = opndSrc1;
        opndSrc1 = opndSrc2;
        opndSrc2 = tempOpnd;
    }

    if (opndSrc2->IsTaggedInt())
    {
        if (opndSrc1->IsTaggedInt())
        {
            return instrPrev;
        }

        GenerateSmIntTest(opndSrc1, instrInsert, labelFail);
        return instrPrev;
    }
    else
    {
        opndReg = IR::RegOpnd::New(TyMachReg, this->m_func);
#ifdef SHIFTLOAD

        instr = IR::Instr::New(Js::OpCode::SHLD, opndReg, opndSrc1, IR::IntConstOpnd::New(16, TyInt8, this->m_func), this->m_func);
        instrInsert->InsertBefore(instr);

        instr = IR::Instr::New(Js::OpCode::SHLD, opndReg, opndSrc2, IR::IntConstOpnd::New(16, TyInt8, this->m_func), this->m_func);
        instrInsert->InsertBefore(instr);
#else
        IR::Opnd * opndReg1;

        // s1 = MOV src1
        // s1 = SHR s1, VarTag_Shift
        // s2 = MOV src2
        // s2 = SHR s2, 32
        // s1 = OR s1, s2        ------ move both tags to the lower 32 bits
        // CMP s1, AtomTag_Pair  ------ compare the tags together to the expected tag pair
        // JNE $fail

        // s1 = MOV src1


        instr = IR::Instr::New(Js::OpCode::MOV, opndReg, opndSrc1, this->m_func);
        instrInsert->InsertBefore(instr);

        // s1 = SHR s1, VarTag_Shift

        instr = IR::Instr::New(Js::OpCode::SHR, opndReg, opndReg, IR::IntConstOpnd::New(Js::VarTag_Shift, TyInt8, this->m_func), this->m_func);
        instrInsert->InsertBefore(instr);

        // s2 = MOV src2

        opndReg1 = IR::RegOpnd::New(TyMachReg, this->m_func);
        instr = IR::Instr::New(Js::OpCode::MOV, opndReg1, opndSrc2, this->m_func);
        instrInsert->InsertBefore(instr);

        // s2 = SHR s2, 32

        instr = IR::Instr::New(Js::OpCode::SHR, opndReg1, opndReg1, IR::IntConstOpnd::New(32, TyInt8, this->m_func), this->m_func);
        instrInsert->InsertBefore(instr);

        // s1 = OR s1, s2

        instr = IR::Instr::New(Js::OpCode::OR, opndReg, opndReg, opndReg1, this->m_func);
        instrInsert->InsertBefore(instr);
#endif
        opndReg = opndReg->UseWithNewType(TyInt32, this->m_func)->AsRegOpnd();

        // CMP s1, AtomTag_Pair
        instr = IR::Instr::New(Js::OpCode::CMP, this->m_func);
        instr->SetSrc1(opndReg);
        instr->SetSrc2(IR::IntConstOpnd::New(Js::AtomTag_Pair, TyInt32, this->m_func, true));

        instrInsert->InsertBefore(instr);
    }

    //      JNE $fail
    instr = IR::BranchInstr::New(Js::OpCode::JNE, labelFail, this->m_func);
    instrInsert->InsertBefore(instr);

    return instrPrev;
}
#endif

IR::BranchInstr *
LowererMD::GenerateLocalInlineCacheCheck(
    IR::Instr * instrLdSt,
    IR::RegOpnd * opndType,
    IR::RegOpnd * inlineCache,
    IR::LabelInstr * labelNext,
    bool checkTypeWithoutProperty)
{
    // Generate:
    //
    //      CMP s1, [&(inlineCache->u.local.type/typeWithoutProperty)]
    //      JNE $next

    IR::Instr * instr;
    IR::Opnd* typeOpnd;
    if (checkTypeWithoutProperty)
    {
        typeOpnd = IR::IndirOpnd::New(inlineCache, (int32)offsetof(Js::InlineCache, u.local.typeWithoutProperty), TyMachReg, instrLdSt->m_func);
    }
    else
    {
        typeOpnd = IR::IndirOpnd::New(inlineCache, (int32)offsetof(Js::InlineCache, u.local.type), TyMachReg, instrLdSt->m_func);
    }

    // CMP type, [&(inlineCache->u.local.type/typeWithoutProperty)]
    instr = IR::Instr::New(Js::OpCode::CMP, instrLdSt->m_func);
    instr->SetSrc1(opndType);
    instr->SetSrc2(typeOpnd);
    instrLdSt->InsertBefore(instr);

    // JNE $next
    IR::BranchInstr * branchInstr = IR::BranchInstr::New(Js::OpCode::JNE, labelNext, instrLdSt->m_func);
    instrLdSt->InsertBefore(branchInstr);

    return branchInstr;
}

IR::BranchInstr *
LowererMD::GenerateProtoInlineCacheCheck(
    IR::Instr * instrLdSt,
    IR::RegOpnd * opndType,
    IR::RegOpnd * inlineCache,
    IR::LabelInstr * labelNext)
{
    // Generate:
    //
    //      CMP s1, [&(inlineCache->u.proto.type)]
    //      JNE $next

    IR::Instr * instr;
    IR::Opnd* typeOpnd = IR::IndirOpnd::New(inlineCache, (int32)offsetof(Js::InlineCache, u.proto.type), TyMachReg, instrLdSt->m_func);

    // CMP s1, [&(inlineCache->u.proto.type)]
    instr = IR::Instr::New(Js::OpCode::CMP, instrLdSt->m_func);
    instr->SetSrc1(opndType);
    instr->SetSrc2(typeOpnd);
    instrLdSt->InsertBefore(instr);

    // JNE $next
    IR::BranchInstr * branchInstr = IR::BranchInstr::New(Js::OpCode::JNE, labelNext, instrLdSt->m_func);
    instrLdSt->InsertBefore(branchInstr);

    return branchInstr;
}

IR::BranchInstr *
LowererMD::GenerateFlagInlineCacheCheck(
    IR::Instr * instrLdSt,
    IR::RegOpnd * opndType,
    IR::RegOpnd * opndInlineCache,
    IR::LabelInstr * labelNext)
{
    // Generate:
    //
    //      CMP s1, [&(inlineCache->u.accessor.type)]
    //      JNE $next

    IR::Instr * instr;
    IR::Opnd* typeOpnd;
    typeOpnd = IR::IndirOpnd::New(opndInlineCache, (int32)offsetof(Js::InlineCache, u.accessor.type), TyMachReg, instrLdSt->m_func);

    // CMP s1, [&(inlineCache->u.flag.type)]
    instr = IR::Instr::New(Js::OpCode::CMP, instrLdSt->m_func);
    instr->SetSrc1(opndType);
    instr->SetSrc2(typeOpnd);
    instrLdSt->InsertBefore(instr);

    // JNE $next
    IR::BranchInstr * branchInstr = IR::BranchInstr::New(Js::OpCode::JNE, labelNext, instrLdSt->m_func);
    instrLdSt->InsertBefore(branchInstr);

    return branchInstr;
}

IR::BranchInstr *
LowererMD::GenerateFlagInlineCacheCheckForNoGetterSetter(
    IR::Instr * instrLdSt,
    IR::RegOpnd * opndInlineCache,
    IR::LabelInstr * labelNext)
{
    // Generate:
    //
    //      TEST [&(inlineCache->u.accessor.flags)], (Js::InlineCacheGetterFlag | Js::InlineCacheSetterFlag)
    //      JNE $next

    IR::Instr * instr;
    IR::Opnd* flagsOpnd;
    flagsOpnd = IR::IndirOpnd::New(opndInlineCache, 0, TyInt8, instrLdSt->m_func);

    // TEST [&(inlineCache->u.accessor.flags)], (Js::InlineCacheGetterFlag | Js::InlineCacheSetterFlag)
    instr = IR::Instr::New(Js::OpCode::TEST,instrLdSt->m_func);
    instr->SetSrc1(flagsOpnd);
    instr->SetSrc2(IR::IntConstOpnd::New((Js::InlineCacheGetterFlag | Js::InlineCacheSetterFlag) << 1, TyInt8, instrLdSt->m_func));
    instrLdSt->InsertBefore(instr);

    // JNE $next
    IR::BranchInstr * branchInstr = IR::BranchInstr::New(Js::OpCode::JNE, labelNext, instrLdSt->m_func);
    instrLdSt->InsertBefore(branchInstr);

    return branchInstr;
}

void
LowererMD::GenerateFlagInlineCacheCheckForGetterSetter(
    IR::Instr * insertBeforeInstr,
    IR::RegOpnd * opndInlineCache,
    IR::LabelInstr * labelNext)
{
    uint accessorFlagMask;
    if (PHASE_OFF(Js::InlineGettersPhase, insertBeforeInstr->m_func->GetJnFunction()))
    {
        accessorFlagMask = Js::InlineCache::GetSetterFlagMask();
    }
    else if (PHASE_OFF(Js::InlineSettersPhase, insertBeforeInstr->m_func->GetJnFunction()))
    {
        accessorFlagMask = Js::InlineCache::GetGetterFlagMask();
    }
    else
    {
        accessorFlagMask = Js::InlineCache::GetGetterSetterFlagMask();
    }

    // Generate:
    //
    //      TEST [&(inlineCache->u.accessor.flags)], Js::InlineCacheGetterFlag | Js::InlineCacheSetterFlag
    //      JEQ $next
    IR::Instr * instr;
    IR::Opnd* flagsOpnd;
    flagsOpnd = IR::IndirOpnd::New(opndInlineCache, (int32)offsetof(Js::InlineCache, u.accessor.rawUInt16), TyInt8, insertBeforeInstr->m_func);

    // TEST [&(inlineCache->u.accessor.flags)], InlineCacheGetterFlag | InlineCacheSetterFlag
    instr = IR::Instr::New(Js::OpCode::TEST,this->m_func);
    instr->SetSrc1(flagsOpnd);
    instr->SetSrc2(IR::IntConstOpnd::New(accessorFlagMask, TyInt8, this->m_func));
    insertBeforeInstr->InsertBefore(instr);

    // JEQ $next
    instr = IR::BranchInstr::New(Js::OpCode::JEQ, labelNext, this->m_func);
    insertBeforeInstr->InsertBefore(instr);
}

void
LowererMD::GenerateLdFldFromLocalInlineCache(
    IR::Instr * instrLdFld,
    IR::RegOpnd * opndBase,
    IR::Opnd * opndDst,
    IR::RegOpnd * inlineCache,
    IR::LabelInstr * labelFallThru,
    bool isInlineSlot)
{
    // Generate:
    //
    // s1 = MOV base->slots -- load the slot array
    // s2 = MOVZXw [&(inlineCache->u.local.slotIndex)] -- load the cached slot index
    // dst = MOV [s1 + s2* Scale]  -- load the value directly from the slot
    //      JMP $fallthru

    IR::Instr * instr;
    IR::Opnd* slotIndexOpnd;
    IR::IndirOpnd * opndIndir;
    IR::RegOpnd * opndSlotArray = nullptr;

    if (!isInlineSlot)
    {
        opndSlotArray = IR::RegOpnd::New(TyMachReg, instrLdFld->m_func);
        opndIndir = IR::IndirOpnd::New(opndBase, Js::DynamicObject::GetOffsetOfAuxSlots(), TyMachReg, instrLdFld->m_func);
        instr = IR::Instr::New(Js::OpCode::MOV, opndSlotArray, opndIndir, instrLdFld->m_func);
        instrLdFld->InsertBefore(instr);
    }

    // s2 = MOVZXw [&(inlineCache->u.local.slotIndex)] -- load the cached slot index
    IR::RegOpnd * opndReg2 = IR::RegOpnd::New(TyMachReg, instrLdFld->m_func);
    slotIndexOpnd = IR::IndirOpnd::New(inlineCache, (int32)offsetof(Js::InlineCache, u.local.slotIndex), TyUint16, instrLdFld->m_func);
    instr = IR::Instr::New(Js::OpCode::MOVZXW, opndReg2, slotIndexOpnd, instrLdFld->m_func);
    instrLdFld->InsertBefore(instr);

    if (isInlineSlot)
    {
        // dst = MOV [base + s2* Scale]  -- load the value directly from the slot
        opndIndir = IR::IndirOpnd::New(opndBase, opndReg2, LowererMDArch::GetDefaultIndirScale(), TyMachReg, instrLdFld->m_func);
        instr = IR::Instr::New(Js::OpCode::MOV, opndDst, opndIndir, instrLdFld->m_func);
        instrLdFld->InsertBefore(instr);
    }
    else
    {
        // dst = MOV [s1 + s2* Scale]  -- load the value directly from the slot
        opndIndir = IR::IndirOpnd::New(opndSlotArray, opndReg2, LowererMDArch::GetDefaultIndirScale(), TyMachReg, instrLdFld->m_func);
        instr = IR::Instr::New(Js::OpCode::MOV, opndDst, opndIndir, instrLdFld->m_func);
        instrLdFld->InsertBefore(instr);
    }

    // JMP $fallthru
    instr = IR::BranchInstr::New(Js::OpCode::JMP, labelFallThru, instrLdFld->m_func);
    instrLdFld->InsertBefore(instr);
}

void
LowererMD::GenerateLdLocalFldFromFlagInlineCache(
    IR::Instr * instrLdFld,
    IR::RegOpnd * opndBase,
    IR::Opnd * opndDst,
    IR::RegOpnd * opndInlineCache,
    IR::LabelInstr * labelFallThru,
    bool isInlineSlot)
{
    // Generate:
    //
    // s1 = MOV [&base->slots] -- load the slot array
    // s2 = MOVZXW [&(inlineCache->u.accessor.slotIndex)] -- load the cached slot index
    // dst = MOV [s1 + s2*4]
    //      JMP $fallthru

    IR::Instr * instr;
    IR::Opnd* slotIndexOpnd;
    IR::IndirOpnd * opndIndir;
    IR::RegOpnd * opndSlotArray = nullptr;

    if (!isInlineSlot)
    {
        opndSlotArray = IR::RegOpnd::New(TyMachReg, instrLdFld->m_func);
        opndIndir = IR::IndirOpnd::New(opndBase, Js::DynamicObject::GetOffsetOfAuxSlots(), TyMachReg, instrLdFld->m_func);
        instr = IR::Instr::New(Js::OpCode::MOV, opndSlotArray, opndIndir, instrLdFld->m_func);
        instrLdFld->InsertBefore(instr);
    }

    // s2 = MOVZXW [&(inlineCache->u.accessor.slotIndex)] -- load the cached slot index
    IR::RegOpnd *opndSlotIndex = IR::RegOpnd::New(TyMachReg, instrLdFld->m_func);
    slotIndexOpnd = IR::IndirOpnd::New(opndInlineCache, (int32)offsetof(Js::InlineCache, u.accessor.slotIndex), TyUint16, instrLdFld->m_func);
    instr = IR::Instr::New(Js::OpCode::MOVZXW, opndSlotIndex, slotIndexOpnd, instrLdFld->m_func);
    instrLdFld->InsertBefore(instr);

    if (isInlineSlot)
    {
        // dst = MOV [s1 + s2*4]
        opndIndir = IR::IndirOpnd::New(opndBase, opndSlotIndex, LowererMDArch::GetDefaultIndirScale(), TyMachReg, instrLdFld->m_func);
        instr = IR::Instr::New(Js::OpCode::MOV, opndDst, opndIndir, instrLdFld->m_func);
        instrLdFld->InsertBefore(instr);
    }
    else
    {
        // dst = MOV [s1 + s2*4]
        opndIndir = IR::IndirOpnd::New(opndSlotArray, opndSlotIndex, LowererMDArch::GetDefaultIndirScale(), TyMachReg, instrLdFld->m_func);
        instr = IR::Instr::New(Js::OpCode::MOV, opndDst, opndIndir, instrLdFld->m_func);
        instrLdFld->InsertBefore(instr);
    }

    // JMP $fallthru
    instr = IR::BranchInstr::New(Js::OpCode::JMP, labelFallThru, instrLdFld->m_func);
    instrLdFld->InsertBefore(instr);
}

void
LowererMD::GenerateLdFldFromFlagInlineCache(
    IR::Instr * insertBeforeInstr,
    IR::RegOpnd * opndBase,
    IR::Opnd * opndDst,
    IR::RegOpnd * opndInlineCache,
    IR::LabelInstr * labelFallThru,
    bool isInlineSlot)
{
    // Generate:
    //
    // s1 = MOV [&(inlineCache->u.accessor.object)] -- load the cached prototype object
    // s1 = MOV [&s1->slots] -- load the slot array
    // s2 = MOVZXW [&(inlineCache->u.accessor.slotIndex)] -- load the cached slot index
    // dst = MOV [s1 + s2*4]
    //      JMP $fallthru

    IR::Instr * instr;

    IR::Opnd* inlineCacheObjOpnd;
    IR::IndirOpnd * opndIndir;
    IR::RegOpnd * opndObjSlots = nullptr;

    inlineCacheObjOpnd = IR::IndirOpnd::New(opndInlineCache, (int32)offsetof(Js::InlineCache, u.accessor.object), TyMachReg, this->m_func);

    // s1 = MOV [&(inlineCache->u.accessor.object)] -- load the cached prototype object
    IR::RegOpnd *opndObject = IR::RegOpnd::New(TyMachReg, this->m_func);
    instr = IR::Instr::New(Js::OpCode::MOV, opndObject, inlineCacheObjOpnd, this->m_func);
    insertBeforeInstr->InsertBefore(instr);

    if (!isInlineSlot)
    {
        // s1 = MOV [&s1->slots] -- load the slot array
        opndObjSlots = IR::RegOpnd::New(TyMachReg, this->m_func);
        opndIndir = IR::IndirOpnd::New(opndObject, Js::DynamicObject::GetOffsetOfAuxSlots(), TyMachReg, this->m_func);
        instr = IR::Instr::New(Js::OpCode::MOV, opndObjSlots, opndIndir, this->m_func);
        insertBeforeInstr->InsertBefore(instr);
    }

    // s2 = MOVZXW [&(inlineCache->u.accessor.slotIndex)] -- load the cached slot index
    IR::RegOpnd *opndSlotIndex = IR::RegOpnd::New(TyMachReg, this->m_func);
    IR::Opnd* slotIndexOpnd = IR::IndirOpnd::New(opndInlineCache, (int32)offsetof(Js::InlineCache, u.accessor.slotIndex), TyUint16, this->m_func);
    instr = IR::Instr::New(Js::OpCode::MOVZXW, opndSlotIndex, slotIndexOpnd, this->m_func);
    insertBeforeInstr->InsertBefore(instr);

    if (isInlineSlot)
    {
        // dst = MOV [s1 + s2*4]
        opndIndir = IR::IndirOpnd::New(opndObject, opndSlotIndex, this->lowererMDArch.GetDefaultIndirScale(), TyMachReg, this->m_func);
        instr = IR::Instr::New(Js::OpCode::MOV, opndDst, opndIndir, this->m_func);
        insertBeforeInstr->InsertBefore(instr);
    }
    else
    {
        // dst = MOV [s1 + s2*4]
        opndIndir = IR::IndirOpnd::New(opndObjSlots, opndSlotIndex, this->lowererMDArch.GetDefaultIndirScale(), TyMachReg, this->m_func);
        instr = IR::Instr::New(Js::OpCode::MOV, opndDst, opndIndir, this->m_func);
        insertBeforeInstr->InsertBefore(instr);
    }

    // JMP $fallthru
    instr = IR::BranchInstr::New(Js::OpCode::JMP, labelFallThru, this->m_func);
    insertBeforeInstr->InsertBefore(instr);
}


void
LowererMD::GenerateLdFldFromProtoInlineCache(
    IR::Instr * instrLdFld,
    IR::RegOpnd * opndBase,
    IR::Opnd * opndDst,
    IR::RegOpnd * inlineCache,
    IR::LabelInstr * labelFallThru,
    bool isInlineSlot)
{
    // Generate:
    //
    // s1 = MOV [&(inlineCache->u.proto.prototypeObject)] -- load the cached prototype object
    // s1 = MOV [&s1->slots] -- load the slot array
    // s2 = MOVZXW [&(inlineCache->u.proto.slotIndex)] -- load the cached slot index
    // dst = MOV [s1 + s2*4]
    //      JMP $fallthru

    IR::Instr * instr;

    IR::Opnd* inlineCacheProtoOpnd;
    IR::IndirOpnd * opndIndir;
    IR::RegOpnd * opndProtoSlots = nullptr;

    inlineCacheProtoOpnd = IR::IndirOpnd::New(inlineCache, (int32)offsetof(Js::InlineCache, u.proto.prototypeObject), TyMachReg, instrLdFld->m_func);

    // s1 = MOV [&(inlineCache->u.proto.prototypeObject)] -- load the cached prototype object
    IR::RegOpnd *opndProto = IR::RegOpnd::New(TyMachReg, instrLdFld->m_func);
    instr = IR::Instr::New(Js::OpCode::MOV, opndProto, inlineCacheProtoOpnd, instrLdFld->m_func);
    instrLdFld->InsertBefore(instr);

    if (!isInlineSlot)
    {
        // s1 = MOV [&s1->slots] -- load the slot array
        opndProtoSlots = IR::RegOpnd::New(TyMachReg, instrLdFld->m_func);
        opndIndir = IR::IndirOpnd::New(opndProto, Js::DynamicObject::GetOffsetOfAuxSlots(), TyMachReg, instrLdFld->m_func);
        instr = IR::Instr::New(Js::OpCode::MOV, opndProtoSlots, opndIndir, instrLdFld->m_func);
        instrLdFld->InsertBefore(instr);
    }

    // s2 = MOVZXW [&(inlineCache->u.proto.slotIndex)] -- load the cached slot index
    IR::RegOpnd *opndSlotIndex = IR::RegOpnd::New(TyMachReg, instrLdFld->m_func);
    IR::Opnd* slotIndexOpnd = IR::IndirOpnd::New(inlineCache, (int32)offsetof(Js::InlineCache, u.proto.slotIndex), TyUint16, instrLdFld->m_func);
    instr = IR::Instr::New(Js::OpCode::MOVZXW, opndSlotIndex, slotIndexOpnd, instrLdFld->m_func);
    instrLdFld->InsertBefore(instr);

    if (isInlineSlot)
    {
        // dst = MOV [s1 + s2*4]
        opndIndir = IR::IndirOpnd::New(opndProto, opndSlotIndex, LowererMDArch::GetDefaultIndirScale(), TyMachReg, instrLdFld->m_func);
        instr = IR::Instr::New(Js::OpCode::MOV, opndDst, opndIndir, instrLdFld->m_func);
        instrLdFld->InsertBefore(instr);
    }
    else
    {
        // dst = MOV [s1 + s2*4]
        opndIndir = IR::IndirOpnd::New(opndProtoSlots, opndSlotIndex, LowererMDArch::GetDefaultIndirScale(), TyMachReg, instrLdFld->m_func);
        instr = IR::Instr::New(Js::OpCode::MOV, opndDst, opndIndir, instrLdFld->m_func);
        instrLdFld->InsertBefore(instr);
    }

    // JMP $fallthru
    instr = IR::BranchInstr::New(Js::OpCode::JMP, labelFallThru, instrLdFld->m_func);
    instrLdFld->InsertBefore(instr);
}

void
LowererMD::GenerateLoadTaggedType(IR::Instr * instrLdSt, IR::RegOpnd * opndType, IR::RegOpnd * opndTaggedType)
{
    // Generate
    //
    // MOV taggedType, type
    // OR taggedType, InlineCacheAuxSlotTypeTag

    // MOV taggedType, type
    {
        IR::Instr * instrMov = IR::Instr::New(Js::OpCode::MOV, opndTaggedType, opndType, instrLdSt->m_func);
        instrLdSt->InsertBefore(instrMov);
    }
    // OR taggedType, InlineCacheAuxSlotTypeTag
    {
        IR::IntConstOpnd * opndAuxSlotTag = IR::IntConstOpnd::New(InlineCacheAuxSlotTypeTag, TyInt8, instrLdSt->m_func);
        IR::Instr * instrAnd = IR::Instr::New(Js::OpCode::OR, opndTaggedType, opndTaggedType, opndAuxSlotTag, instrLdSt->m_func);
        instrLdSt->InsertBefore(instrAnd);
    }
}

///----------------------------------------------------------------------------
///
/// LowererMD::GenerateFastLdMethodFromFlags
///
/// Make use of the helper to cache the type and slot index used to do a LdFld
/// and do an inline load from the appropriate slot if the type hasn't changed
/// since the last time this LdFld was executed.
///
///----------------------------------------------------------------------------

bool
LowererMD::GenerateFastLdMethodFromFlags(IR::Instr * instrLdFld)
{
    IR::LabelInstr *   labelFallThru;
    IR::LabelInstr *   bailOutLabel;
    IR::Opnd *         opndSrc;
    IR::Opnd *         opndDst;
    IR::RegOpnd *      opndBase;
    IR::RegOpnd *      opndType;
    IR::RegOpnd *      opndInlineCache;

    opndSrc = instrLdFld->GetSrc1();

    AssertMsg(opndSrc->IsSymOpnd() && opndSrc->AsSymOpnd()->IsPropertySymOpnd() && opndSrc->AsSymOpnd()->m_sym->IsPropertySym(),
              "Expected property sym operand as src of LdFldFlags");

    IR::PropertySymOpnd * propertySymOpnd = opndSrc->AsPropertySymOpnd();

    Assert(!instrLdFld->DoStackArgsOpt(this->m_func));

    if (propertySymOpnd->IsTypeCheckSeqCandidate())
    {
        AssertMsg(propertySymOpnd->HasObjectTypeSym(), "Type optimized property sym operand without a type sym?");
        StackSym *typeSym = propertySymOpnd->GetObjectTypeSym();
        opndType = IR::RegOpnd::New(typeSym, TyMachReg, this->m_func);
    }
    else
    {
        opndType = IR::RegOpnd::New(TyMachReg, this->m_func);
    }

    opndBase = propertySymOpnd->CreatePropertyOwnerOpnd(m_func);
    opndDst = instrLdFld->GetDst();
    opndInlineCache = IR::RegOpnd::New(TyMachPtr, this->m_func);

    labelFallThru = IR::LabelInstr::New(Js::OpCode::Label, this->m_func);
    // Label to jump to (or fall through to) when bailing out
    bailOutLabel = IR::LabelInstr::New(Js::OpCode::Label, instrLdFld->m_func, true /* isOpHelper */);

    instrLdFld->InsertBefore(IR::Instr::New(Js::OpCode::MOV, opndInlineCache, m_lowerer->LoadRuntimeInlineCacheOpnd(instrLdFld, propertySymOpnd), this->m_func));
    IR::LabelInstr * labelFlagAux = IR::LabelInstr::New(Js::OpCode::Label, this->m_func);

    // Check the flag cache with the untagged type
    this->m_lowerer->GenerateObjectTestAndTypeLoad(instrLdFld, opndBase, opndType, bailOutLabel);
    // Blindly do the check for getter flag first and then do the type check
    // We avoid repeated check for getter flag when the function object may be in either
    // inline slots or auxiliary slots
    GenerateFlagInlineCacheCheckForGetterSetter(instrLdFld, opndInlineCache, bailOutLabel);
    GenerateFlagInlineCacheCheck(instrLdFld, opndType, opndInlineCache, labelFlagAux);
    GenerateLdFldFromFlagInlineCache(instrLdFld, opndBase, opndDst, opndInlineCache, labelFallThru, true);

    // Check the flag cache with the tagged type
    instrLdFld->InsertBefore(labelFlagAux);
    IR::RegOpnd * opndTaggedType = IR::RegOpnd::New(TyMachReg, this->m_func);
    GenerateLoadTaggedType(instrLdFld, opndType, opndTaggedType);
    GenerateFlagInlineCacheCheck(instrLdFld, opndTaggedType, opndInlineCache, bailOutLabel);
    GenerateLdFldFromFlagInlineCache(instrLdFld, opndBase, opndDst, opndInlineCache, labelFallThru, false);

    instrLdFld->InsertBefore(bailOutLabel);
    instrLdFld->InsertAfter(labelFallThru);
    // Generate the bailout helper call. 'instr' will be changed to the CALL into the bailout function, so it can't be used for
    // ordering instructions anymore.
    instrLdFld->UnlinkSrc1();
    this->m_lowerer->GenerateBailOut(instrLdFld);

    return true;
}

void
LowererMD::GenerateLoadPolymorphicInlineCacheSlot(IR::Instr * instrLdSt, IR::RegOpnd * opndInlineCache, IR::RegOpnd * opndType, uint polymorphicInlineCacheSize)
{
    // Generate
    //
    // MOV r1, type
    // SHR r1, PolymorphicInlineCacheShift
    // AND r1, (size - 1)
    // SHL r1, log2(sizeof(Js::InlineCache))
    // LEA inlineCache, [inlineCache + r1]

    // MOV r1, type
    IR::RegOpnd * opndOffset = IR::RegOpnd::New(TyMachPtr, instrLdSt->m_func);
    IR::Instr * instr = IR::Instr::New(Js::OpCode::MOV, opndOffset, opndType, instrLdSt->m_func);
    instrLdSt->InsertBefore(instr);

    IntConstType rightShiftAmount = PolymorphicInlineCacheShift;
    IntConstType leftShiftAmount = Math::Log2(sizeof(Js::InlineCache));
    // instead of generating
    // SHR r1, PolymorphicInlineCacheShift
    // AND r1, (size - 1)
    // SHL r1, log2(sizeof(Js::InlineCache))
    //
    // we can generate:
    // SHR r1, (PolymorphicInlineCacheShift - log2(sizeof(Js::InlineCache))
    // AND r1, (size - 1) << log2(sizeof(Js::InlineCache))
    Assert(rightShiftAmount > leftShiftAmount);
    instr = IR::Instr::New(Js::OpCode::SHR, opndOffset, opndOffset, IR::IntConstOpnd::New(rightShiftAmount - leftShiftAmount, TyUint8, instrLdSt->m_func, true), instrLdSt->m_func);
    instrLdSt->InsertBefore(instr);
    instr = IR::Instr::New(Js::OpCode::AND, opndOffset, opndOffset, IR::AddrOpnd::New((void*)((IntConstType)(polymorphicInlineCacheSize - 1) << leftShiftAmount), IR::AddrOpndKindConstant, instrLdSt->m_func, true), instrLdSt->m_func);
    instrLdSt->InsertBefore(instr);

    // LEA inlineCache, [inlineCache + r1]
    IR::IndirOpnd * indirOpnd = IR::IndirOpnd::New(opndInlineCache, opndOffset, TyMachPtr, instrLdSt->m_func);
    instr = IR::Instr::New(Js::OpCode::LEA, opndInlineCache, indirOpnd, instrLdSt->m_func);
    instrLdSt->InsertBefore(instr);
}

void
LowererMD::ChangeToWriteBarrierAssign(IR::Instr * assignInstr)
{
#ifdef RECYCLER_WRITE_BARRIER_JIT
    if (assignInstr->GetSrc1()->IsWriteBarrierTriggerableValue())
    {
        IR::RegOpnd * writeBarrierAddrRegOpnd = IR::RegOpnd::New(TyMachPtr, assignInstr->m_func);
        IR::Instr * leaInstr = IR::Instr::New(Js::OpCode::LEA, writeBarrierAddrRegOpnd, assignInstr->UnlinkDst(), assignInstr->m_func);
        assignInstr->InsertBefore(leaInstr);
        assignInstr->SetDst(IR::IndirOpnd::New(writeBarrierAddrRegOpnd, 0, TyMachReg, assignInstr->m_func));

        GenerateWriteBarrier(writeBarrierAddrRegOpnd, assignInstr->m_next);
    }
#endif
    ChangeToAssign(assignInstr);
}

void
LowererMD::GenerateWriteBarrierAssign(IR::MemRefOpnd * opndDst, IR::Opnd * opndSrc, IR::Instr * insertBeforeInstr)
{
    Lowerer::InsertMove(opndDst, opndSrc, insertBeforeInstr);
#ifdef RECYCLER_WRITE_BARRIER_JIT
    if (opndSrc->IsWriteBarrierTriggerableValue())
    {
        void * address = opndDst->AsMemRefOpnd()->GetMemLoc();
#ifdef RECYCLER_WRITE_BARRIER_BYTE
        IR::MemRefOpnd * cardTableEntry = IR::MemRefOpnd::New(
            &RecyclerWriteBarrierManager::GetAddressOfCardTable()[RecyclerWriteBarrierManager::GetCardTableIndex(address)], TyInt8, insertBeforeInstr->m_func);

        IR::Instr * movInstr = IR::Instr::New(Js::OpCode::MOV, cardTableEntry, IR::IntConstOpnd::New(1, TyInt8, insertBeforeInstr->m_func), insertBeforeInstr->m_func);
        insertBeforeInstr->InsertBefore(movInstr);
#else
        IR::MemRefOpnd * cardTableEntry = IR::MemRefOpnd::New(
            &RecyclerWriteBarrierManager::GetAddressOfCardTable()[RecyclerWriteBarrierManager::GetCardTableIndex(address)], TyMachPtr, insertBeforeInstr->m_func);
        IR::Instr * orInstr = IR::Instr::New(Js::OpCode::OR, cardTableEntry,
            IR::IntConstOpnd::New(1 << ((uint)address >> 7), TyInt32, insertBeforeInstr->m_func), insertBeforeInstr->m_func);
        insertBeforeInstr->InsertBefore(orInstr);
#endif
    }
#endif
}

void
LowererMD::GenerateWriteBarrierAssign(IR::IndirOpnd * opndDst, IR::Opnd * opndSrc, IR::Instr * insertBeforeInstr)
{
#ifdef RECYCLER_WRITE_BARRIER_JIT
    if (opndSrc->IsWriteBarrierTriggerableValue())
    {
        IR::RegOpnd * writeBarrierAddrRegOpnd = IR::RegOpnd::New(TyMachPtr, insertBeforeInstr->m_func);
        insertBeforeInstr->InsertBefore(IR::Instr::New(Js::OpCode::LEA, writeBarrierAddrRegOpnd, opndDst, insertBeforeInstr->m_func));
        insertBeforeInstr->InsertBefore(IR::Instr::New(Js::OpCode::MOV,
            IR::IndirOpnd::New(writeBarrierAddrRegOpnd, 0, TyMachReg, insertBeforeInstr->m_func), opndSrc, insertBeforeInstr->m_func));
        GenerateWriteBarrier(writeBarrierAddrRegOpnd, insertBeforeInstr);

        // The mov happens above, and it's slightly faster doing it that way since we've already calculated the address we're writing to
        return;
    }
#endif
    Lowerer::InsertMove(opndDst, opndSrc, insertBeforeInstr);
    return;
}

#ifdef RECYCLER_WRITE_BARRIER_JIT
void
LowererMD::GenerateWriteBarrier(IR::Opnd * writeBarrierAddrRegOpnd, IR::Instr * insertBeforeInstr)
{
#if defined(RECYCLER_WRITE_BARRIER_BYTE)
    IR::RegOpnd * indexOpnd = IR::RegOpnd::New(TyMachPtr, insertBeforeInstr->m_func);
    IR::Instr * loadIndexInstr = IR::Instr::New(Js::OpCode::MOV, indexOpnd, writeBarrierAddrRegOpnd, insertBeforeInstr->m_func);
    insertBeforeInstr->InsertBefore(loadIndexInstr);

    IR::Instr * shiftBitInstr = IR::Instr::New(Js::OpCode::SHR, indexOpnd, indexOpnd,
        IR::IntConstOpnd::New(12 /* 1 << 12 = 4096 */, TyInt32, insertBeforeInstr->m_func), insertBeforeInstr->m_func);
    insertBeforeInstr->InsertBefore(shiftBitInstr);

    IR::RegOpnd * cardTableRegOpnd = IR::RegOpnd::New(TyMachReg, insertBeforeInstr->m_func);
    IR::Instr * cardTableAddrInstr = IR::Instr::New(Js::OpCode::MOV, cardTableRegOpnd,
        IR::AddrOpnd::New(RecyclerWriteBarrierManager::GetAddressOfCardTable(), IR::AddrOpndKindDynamicMisc, insertBeforeInstr->m_func),
        insertBeforeInstr->m_func);
    insertBeforeInstr->InsertBefore(cardTableAddrInstr);

    IR::IndirOpnd * cardTableEntryOpnd = IR::IndirOpnd::New(cardTableRegOpnd, indexOpnd,
        TyInt8, insertBeforeInstr->m_func);
    IR::Instr * movInstr = IR::Instr::New(Js::OpCode::MOV, cardTableEntryOpnd, IR::IntConstOpnd::New(1, TyInt8, insertBeforeInstr->m_func), insertBeforeInstr->m_func);
    insertBeforeInstr->InsertBefore(movInstr);
#else
    Assert(writeBarrierAddrRegOpnd->IsRegOpnd());
    IR::RegOpnd * shiftBitOpnd = IR::RegOpnd::New(TyInt32, insertBeforeInstr->m_func);
    shiftBitOpnd->SetReg(LowererMDArch::GetRegShiftCount());
    IR::Instr * moveShiftBitOpnd = IR::Instr::New(Js::OpCode::MOV, shiftBitOpnd, writeBarrierAddrRegOpnd, insertBeforeInstr->m_func);
    insertBeforeInstr->InsertBefore(moveShiftBitOpnd);

    IR::Instr * shiftBitInstr = IR::Instr::New(Js::OpCode::SHR, shiftBitOpnd, shiftBitOpnd,
        IR::IntConstOpnd::New(7 /* 1 << 7 = 128 */, TyInt32, insertBeforeInstr->m_func), insertBeforeInstr->m_func);
    insertBeforeInstr->InsertBefore(shiftBitInstr);

    IR::RegOpnd * bitOpnd = IR::RegOpnd::New(TyInt32, insertBeforeInstr->m_func);
    IR::Instr * mov1Instr = IR::Instr::New(Js::OpCode::MOV, bitOpnd,
        IR::IntConstOpnd::New(1, TyInt32, insertBeforeInstr->m_func), insertBeforeInstr->m_func);
    insertBeforeInstr->InsertBefore(mov1Instr);

    IR::Instr * bitInstr = IR::Instr::New(Js::OpCode::SHL, bitOpnd, bitOpnd, shiftBitOpnd, insertBeforeInstr->m_func);
    insertBeforeInstr->InsertBefore(bitInstr);

    IR::RegOpnd * indexOpnd = shiftBitOpnd;
    IR::Instr * indexInstr = IR::Instr::New(Js::OpCode::SHR, indexOpnd, indexOpnd,
        IR::IntConstOpnd::New(5 /* 1 << 5 = 32 */, TyInt32, insertBeforeInstr->m_func), insertBeforeInstr->m_func);
    insertBeforeInstr->InsertBefore(indexInstr);

    IR::RegOpnd * cardTableRegOpnd = IR::RegOpnd::New(TyMachReg, insertBeforeInstr->m_func);
    IR::Instr * cardTableAddrInstr = IR::Instr::New(Js::OpCode::MOV, cardTableRegOpnd,
        IR::AddrOpnd::New(RecyclerWriteBarrierManager::GetAddressOfCardTable(), IR::AddrOpndKindDynamicMisc, insertBeforeInstr->m_func),
        insertBeforeInstr->m_func);
    insertBeforeInstr->InsertBefore(cardTableAddrInstr);

    IR::IndirOpnd * cardTableEntryOpnd = IR::IndirOpnd::New(cardTableRegOpnd, indexOpnd, LowererMDArch::GetDefaultIndirScale(),
        TyInt32, insertBeforeInstr->m_func);
    IR::Instr * orInstr = IR::Instr::New(Js::OpCode::OR, cardTableEntryOpnd, cardTableEntryOpnd,
        bitOpnd, insertBeforeInstr->m_func);
    insertBeforeInstr->InsertBefore(orInstr);
#endif
}
#endif


void
LowererMD::GenerateStFldFromLocalInlineCache(
    IR::Instr * instrStFld,
    IR::RegOpnd * opndBase,
    IR::Opnd * opndSrc,
    IR::RegOpnd * inlineCache,
    IR::LabelInstr * labelFallThru,
    bool isInlineSlot)
{
    IR::Instr * instr;
    IR::Opnd* slotIndexOpnd;
    IR::RegOpnd * opndIndirBase = opndBase;

    if (!isInlineSlot)
    {
        // slotArray = MOV base->slots -- load the slot array
        IR::RegOpnd * opndSlotArray = IR::RegOpnd::New(TyMachReg, instrStFld->m_func);
        IR::IndirOpnd * opndIndir = IR::IndirOpnd::New(opndBase, Js::DynamicObject::GetOffsetOfAuxSlots(), TyMachReg, instrStFld->m_func);
        instr = IR::Instr::New(Js::OpCode::MOV, opndSlotArray, opndIndir, instrStFld->m_func);
        instrStFld->InsertBefore(instr);
        opndIndirBase = opndSlotArray;
    }

    // slotIndex = MOV [&inlineCache->u.local.inlineSlotOffsetOrAuxSlotIndex] -- load the cached slot offset or index
    IR::RegOpnd * opndSlotIndex = IR::RegOpnd::New(TyMachReg, instrStFld->m_func);
    slotIndexOpnd = IR::IndirOpnd::New(inlineCache, (int32)offsetof(Js::InlineCache, u.local.slotIndex), TyUint16, instrStFld->m_func);
    instr = IR::Instr::New(Js::OpCode::MOVZXW, opndSlotIndex, slotIndexOpnd, instrStFld->m_func);
    instrStFld->InsertBefore(instr);

    // [base + slotIndex * (1 << indirScale)] = MOV src  -- store the value directly to the slot
    // [slotArray + slotIndex * (1 << indirScale)] = MOV src  -- store the value directly to the slot

    IR::IndirOpnd * storeLocIndirOpnd = IR::IndirOpnd::New(opndIndirBase, opndSlotIndex,
        LowererMDArch::GetDefaultIndirScale(), TyMachReg, instrStFld->m_func);

    GenerateWriteBarrierAssign(storeLocIndirOpnd, opndSrc, instrStFld);
    // JMP $fallthru
    instr = IR::BranchInstr::New(Js::OpCode::JMP, labelFallThru, instrStFld->m_func);
    instrStFld->InsertBefore(instr);
}

void LowererMD::InsertIncUInt8PreventOverflow(
    IR::Opnd *const dst,
    IR::Opnd *const src,
    IR::Instr *const insertBeforeInstr,
    IR::Instr * *const onOverflowInsertBeforeInstrRef)
{
    Assert(dst);
    Assert(dst->GetType() == TyUint8);
    Assert(src);
    Assert(src->GetType() == TyUint8);
    Assert(insertBeforeInstr);

    Func *const func = insertBeforeInstr->m_func;

    // Generate:
    //       cmp src, static_cast<uint8>(-1)
    //       jeq $done
    // dst = add src, 1
    //   $noOverflow:

    IR::LabelInstr *const noOverflowLabel = Lowerer::InsertLabel(false, insertBeforeInstr);

    Lowerer::InsertCompareBranch(src, IR::IntConstOpnd::New(static_cast<uint8>(-1), TyUint8, func, true),
        Js::OpCode::BrEq_A, noOverflowLabel, noOverflowLabel);

    //     inc dst, src
    Lowerer::InsertAdd(true, dst, src, IR::IntConstOpnd::New(1, TyUint8, func, true), noOverflowLabel);


    //   $done:

    if(onOverflowInsertBeforeInstrRef)
    {
        *onOverflowInsertBeforeInstrRef = noOverflowLabel;
    }
}

void LowererMD::InsertDecUInt8PreventOverflow(
    IR::Opnd *const dst,
    IR::Opnd *const src,
    IR::Instr *const insertBeforeInstr,
    IR::Instr * *const onOverflowInsertBeforeInstrRef)
{
    Assert(dst);
    Assert(dst->GetType() == TyUint8);
    Assert(src);
    Assert(src->GetType() == TyUint8);
    Assert(insertBeforeInstr);

    Func *const func = insertBeforeInstr->m_func;

    // Generate:
    //     sub dst, src, 1
    //     jnc $noOverflow
    //     mov dst, 0
    //   $noOverflow:

    IR::LabelInstr *const noOverflowLabel = Lowerer::InsertLabel(false, insertBeforeInstr);

    //     sub dst, src, 1
    IR::Instr *const instr = IR::Instr::New(Js::OpCode::SUB, dst, src, IR::IntConstOpnd::New(1, TyUint8, func, true), func);
    noOverflowLabel->InsertBefore(instr);
    MakeDstEquSrc1(instr);

    //     jnc $noOverflow
    Lowerer::InsertBranch(Js::OpCode::BrGe_A, true, noOverflowLabel, noOverflowLabel);

    //     mov dst, 0
    Lowerer::InsertMove(dst, IR::IntConstOpnd::New(0, TyUint8, func, true), noOverflowLabel);

    //   $noOverflow:

    if(onOverflowInsertBeforeInstrRef)
    {
        *onOverflowInsertBeforeInstrRef = noOverflowLabel;
    }
}

//----------------------------------------------------------------------------
//
// LowererMD::GenerateFastScopedLdFld
//
// Make use of the helper to cache the type and slot index used to do a ScopedLdFld
// when the scope is an array of length 1.
// Extract the only element from array and do an inline load from the appropriate slot
// if the type hasn't changed since the last time this ScopedLdFld was executed.
//
//----------------------------------------------------------------------------
IR::Instr *
LowererMD::GenerateFastScopedLdFld(IR::Instr * instrLdScopedFld)
{
    //  CMP [base + offset(length)], 1     -- get the length on array and test if it is 1.
    //  JNE $helper
    //  MOV r1, [base + offset(scopes)]       -- load the first scope
    //  MOV r2, r1->type
    //  CMP r2, [&(inlineCache->u.local.type)] -- check type
    //  JNE $helper
    //  MOV r1, r1->slots                   -- load the slots array
    //  MOV r2 , [&(inlineCache->u.local.slotIndex)] -- load the cached slot index
    //  MOV dst, [r1+r2]                    -- load the value from the slot
    //  JMP $fallthru
    //  $helper:
    //  dst = CALL PatchGetPropertyScoped(inlineCache, base, field, defaultInstance, scriptContext)
    //  $fallthru:
    IR::RegOpnd *       opndBase;
    IR::Instr *         instr;
    IR::IndirOpnd *     indirOpnd;
    IR::LabelInstr *    labelHelper;
    IR::Opnd *          opndDst;
    IR::RegOpnd *   inlineCache;
    IR::RegOpnd         *r1;
    IR::LabelInstr *    labelFallThru;

    IR::Opnd *propertySrc = instrLdScopedFld->GetSrc1();
    AssertMsg(propertySrc->IsSymOpnd() && propertySrc->AsSymOpnd()->IsPropertySymOpnd() && propertySrc->AsSymOpnd()->m_sym->IsPropertySym(),
              "Expected property sym operand as src of LdScoped");

    IR::PropertySymOpnd * propertySymOpnd = propertySrc->AsPropertySymOpnd();

    opndBase = propertySymOpnd->CreatePropertyOwnerOpnd(m_func);

    IR::Opnd *srcBase =    instrLdScopedFld->GetSrc2();
    AssertMsg(srcBase->IsRegOpnd(), "Expected reg opnd as src2");
    //opndBase = srcBase;

    //IR::IndirOpnd * indirOpnd = src->AsIndirOpnd();
    labelHelper = IR::LabelInstr::New(Js::OpCode::Label, this->m_func, true);

    AssertMsg(opndBase->m_sym->m_isSingleDef, "We assume this isn't redefined");

    //  CMP [base + offset(length)], 1     -- get the length on array and test if it is 1.
    indirOpnd = IR::IndirOpnd::New(opndBase, Js::FrameDisplay::GetOffsetOfLength(), TyInt16, this->m_func);
    instr = IR::Instr::New(Js::OpCode::CMP, this->m_func);
    instr->SetSrc1(indirOpnd);
    instr->SetSrc2(IR::IntConstOpnd::New(0x1, TyInt8, this->m_func));
    instrLdScopedFld->InsertBefore(instr);

    //  JNE $helper
    instr = IR::BranchInstr::New(Js::OpCode::JNE, labelHelper, this->m_func);
    instrLdScopedFld->InsertBefore(instr);

    //  MOV r1, [base + offset(scopes)]       -- load the first scope
    indirOpnd = IR::IndirOpnd::New(opndBase, Js::FrameDisplay::GetOffsetOfScopes(), TyMachReg, this->m_func);
    r1 = IR::RegOpnd::New(TyMachReg, this->m_func);
    instr = IR::Instr::New(Js::OpCode::MOV, r1, indirOpnd, this->m_func);
    instrLdScopedFld->InsertBefore(instr);

    //first load the inlineCache type
    inlineCache = IR::RegOpnd::New(TyMachPtr, this->m_func);
    Assert(inlineCache != nullptr);

    IR::RegOpnd * opndType = IR::RegOpnd::New(TyMachReg, this->m_func);
    opndDst = instrLdScopedFld->GetDst();

    labelFallThru = IR::LabelInstr::New(Js::OpCode::Label, this->m_func);

    r1->m_sym->m_isNotInt = true;

    // Load the type
    this->m_lowerer->GenerateObjectTestAndTypeLoad(instrLdScopedFld, r1, opndType, labelHelper);

    // Check the local cache with the tagged type
    IR::RegOpnd * opndTaggedType = IR::RegOpnd::New(TyMachReg, this->m_func);
    GenerateLoadTaggedType(instrLdScopedFld, opndType, opndTaggedType);
    instrLdScopedFld->InsertBefore(IR::Instr::New(Js::OpCode::MOV, inlineCache, m_lowerer->LoadRuntimeInlineCacheOpnd(instrLdScopedFld, propertySymOpnd), this->m_func));
    GenerateLocalInlineCacheCheck(instrLdScopedFld, opndTaggedType, inlineCache, labelHelper);
    GenerateLdFldFromLocalInlineCache(instrLdScopedFld, r1, opndDst, inlineCache, labelFallThru, false);

    //  $helper:
    //  dst = CALL PatchGetPropertyScoped(inlineCache, opndBase, propertyId, srcBase, scriptContext)
    //  $fallthru:
    instrLdScopedFld->InsertBefore(labelHelper);
    instrLdScopedFld->InsertAfter(labelFallThru);

    return instrLdScopedFld->m_prev;
}


//----------------------------------------------------------------------------
//
// LowererMD::GenerateFastScopedStFld
//
// Make use of the helper to cache the type and slot index used to do a ScopedStFld
// when the scope is an array of length 1.
// Extract the only element from array and do an inline load from the appropriate slot
// if the type hasn't changed since the last time this ScopedStFld was executed.
//
//----------------------------------------------------------------------------
IR::Instr *
LowererMD::GenerateFastScopedStFld(IR::Instr * instrStScopedFld)
{
    //  CMP [base + offset(length)], 1     -- get the length on array and test if it is 1.
    //  JNE $helper
    //  MOV r1, [base + offset(scopes)]       -- load the first scope
    //  MOV r2, r1->type
    //  CMP r2, [&(inlineCache->u.local.type)] -- check type
    //  JNE $helper
    //  MOV r1, r1->slots                   -- load the slots array
    //  MOV r2, [&(inlineCache->u.local.slotIndex)] -- load the cached slot index
    //  [r1 + r2*4] = MOV value             -- store the value directly to the slot
    //  JMP $fallthru
    //  $helper:
    //    CALL PatchSetPropertyScoped(inlineCache, base, field, value, defaultInstance, scriptContext)
    //  $fallthru:
    IR::RegOpnd *       opndBase;
    IR::Instr *         instr;
    IR::IndirOpnd *     indirOpnd;
    IR::LabelInstr *    labelHelper;
    IR::Opnd *          opndDst;
    IR::RegOpnd *   inlineCache;
    IR::RegOpnd         *r1;
    IR::LabelInstr *    labelFallThru;

    IR::Opnd *newValue = instrStScopedFld->GetSrc1();
//    IR::Opnd *defaultInstance = instrStScopedFld->UnlinkSrc2();
    opndDst = instrStScopedFld->GetDst();

    AssertMsg(opndDst->IsSymOpnd() && opndDst->AsSymOpnd()->IsPropertySymOpnd() && opndDst->AsSymOpnd()->m_sym->IsPropertySym(),
              "Expected property sym operand as dst of StScoped");

    IR::PropertySymOpnd * propertySymOpnd = opndDst->AsPropertySymOpnd();

    opndBase = propertySymOpnd->CreatePropertyOwnerOpnd(m_func);

    labelHelper = IR::LabelInstr::New(Js::OpCode::Label, this->m_func, true);

    AssertMsg(opndBase->m_sym->m_isSingleDef, "We assume this isn't redefined");

    //  CMP [base + offset(length)], 1     -- get the length on array and test if it is 1.
    indirOpnd = IR::IndirOpnd::New(opndBase, Js::FrameDisplay::GetOffsetOfLength(), TyInt16, this->m_func);
    instr = IR::Instr::New(Js::OpCode::CMP, this->m_func);
    instr->SetSrc1(indirOpnd);
    instr->SetSrc2(IR::IntConstOpnd::New(0x1, TyInt8, this->m_func));
    instrStScopedFld->InsertBefore(instr);

    //  JNE $helper
    instr = IR::BranchInstr::New(Js::OpCode::JNE, labelHelper, this->m_func);
    instrStScopedFld->InsertBefore(instr);

    //  MOV r1, [base + offset(scopes)]       -- load the first scope
    indirOpnd = IR::IndirOpnd::New(opndBase, Js::FrameDisplay::GetOffsetOfScopes(), TyMachReg, this->m_func);
    r1 = IR::RegOpnd::New(TyMachReg, this->m_func);
    instr = IR::Instr::New(Js::OpCode::MOV, r1, indirOpnd, this->m_func);
    instrStScopedFld->InsertBefore(instr);

    //first load the inlineCache type
    inlineCache = IR::RegOpnd::New(TyMachPtr, this->m_func);
    Assert(inlineCache != nullptr);

    IR::RegOpnd * opndType = IR::RegOpnd::New(TyMachReg, this->m_func);
    labelFallThru = IR::LabelInstr::New(Js::OpCode::Label, this->m_func);

    r1->m_sym->m_isNotInt = true;

    // Load the type
    this->m_lowerer->GenerateObjectTestAndTypeLoad(instrStScopedFld, r1, opndType, labelHelper);

    // Check the local cache with the tagged type
    IR::RegOpnd * opndTaggedType = IR::RegOpnd::New(TyMachReg, this->m_func);
    GenerateLoadTaggedType(instrStScopedFld, opndType, opndTaggedType);
    instrStScopedFld->InsertBefore(IR::Instr::New(Js::OpCode::MOV, inlineCache, m_lowerer->LoadRuntimeInlineCacheOpnd(instrStScopedFld, propertySymOpnd), this->m_func));
    GenerateLocalInlineCacheCheck(instrStScopedFld, opndTaggedType, inlineCache, labelHelper);
    GenerateStFldFromLocalInlineCache(instrStScopedFld, r1, newValue, inlineCache, labelFallThru, false);

    //  $helper:
    //  CALL PatchSetPropertyScoped(inlineCache, opndBase, propertyId, newValue, defaultInstance, scriptContext)
    //  $fallthru:
    instrStScopedFld->InsertBefore(labelHelper);
    instrStScopedFld->InsertAfter(labelFallThru);

    return instrStScopedFld->m_prev;
}

IR::Opnd *
LowererMD::CreateStackArgumentsSlotOpnd()
{
    StackSym *sym = StackSym::New(TyMachReg, this->m_func);
    sym->m_offset = -MachArgsSlotOffset;
    sym->m_allocated = true;

    return IR::SymOpnd::New(sym, TyMachReg, this->m_func);
}

IR::RegOpnd *
LowererMD::GenerateUntagVar(IR::RegOpnd * src, IR::LabelInstr * labelFail, IR::Instr * insertBeforeInstr, bool generateTagCheck)
{
    Assert(src->IsVar());

    //  MOV valueOpnd, index
    IR::RegOpnd *valueOpnd = IR::RegOpnd::New(TyInt32, this->m_func);

    //
    // Convert Index to 32 bits.
    //
    IR::Opnd * opnd = src->UseWithNewType(TyMachReg, this->m_func);

#if INT32VAR
    if (generateTagCheck)
    {
        Assert(!opnd->IsTaggedInt());
        this->GenerateSmIntTest(opnd, insertBeforeInstr, labelFail);
    }

    // Moving into r2 clears the tag bits on AMD64.
    IR::Instr * instr = IR::Instr::New(Js::OpCode::MOV_TRUNC, valueOpnd, opnd, this->m_func);
    insertBeforeInstr->InsertBefore(instr);
#else
    IR::Instr * instr = IR::Instr::New(Js::OpCode::MOV, valueOpnd, opnd, this->m_func);
    insertBeforeInstr->InsertBefore(instr);

    // SAR valueOpnd, Js::VarTag_Shift
    instr = IR::Instr::New(Js::OpCode::SAR, valueOpnd, valueOpnd,
        IR::IntConstOpnd::New(Js::VarTag_Shift, TyInt8, this->m_func), this->m_func);
    insertBeforeInstr->InsertBefore(instr);

    if (generateTagCheck)
    {
        Assert(!opnd->IsTaggedInt());

        // SAR set the carry flag (CF) to 1 if the lower bit is 1
        // JAE will jmp if CF = 0
        instr = IR::BranchInstr::New(Js::OpCode::JAE, labelFail, this->m_func);
        insertBeforeInstr->InsertBefore(instr);
    }
#endif
    return valueOpnd;
}

IR::RegOpnd *LowererMD::LoadNonnegativeIndex(
    IR::RegOpnd *indexOpnd,
    const bool skipNegativeCheck,
    IR::LabelInstr *const notTaggedIntLabel,
    IR::LabelInstr *const negativeLabel,
    IR::Instr *const insertBeforeInstr)
{
    Assert(indexOpnd);
    Assert(indexOpnd->IsVar() || indexOpnd->GetType() == TyInt32 || indexOpnd->GetType() == TyUint32);
    Assert(indexOpnd->GetType() != TyUint32 || skipNegativeCheck);
    Assert(!indexOpnd->IsVar() || notTaggedIntLabel);
    Assert(skipNegativeCheck || negativeLabel);
    Assert(insertBeforeInstr);

    if(indexOpnd->IsVar())
    {
        if (indexOpnd->GetValueType().IsLikelyFloat()
#ifdef _M_IX86
            && AutoSystemInfo::Data.SSE2Available()
#endif
            )
        {
            return m_lowerer->LoadIndexFromLikelyFloat(indexOpnd, skipNegativeCheck, notTaggedIntLabel, negativeLabel, insertBeforeInstr);
        }

        //     mov  intIndex, index
        //     sar  intIndex, 1
        //     jae  $notTaggedIntOrNegative
        indexOpnd = GenerateUntagVar(indexOpnd, notTaggedIntLabel, insertBeforeInstr, !indexOpnd->IsTaggedInt());
    }

    if(!skipNegativeCheck)
    {
        //     test index, index
        //     js   $notTaggedIntOrNegative
        Lowerer::InsertTestBranch(indexOpnd, indexOpnd, Js::OpCode::JSB, negativeLabel, insertBeforeInstr);
    }
    return indexOpnd;
}

IR::IndirOpnd *
LowererMD::GenerateFastElemIStringIndexCommon(IR::Instr * instrInsert, bool isStore, IR::IndirOpnd * indirOpnd, IR::LabelInstr * labelHelper)
{
    IR::RegOpnd *indexOpnd = indirOpnd->GetIndexOpnd();
    IR::RegOpnd *baseOpnd = indirOpnd->GetBaseOpnd();
    Assert(baseOpnd != nullptr);
    Assert(indexOpnd->GetValueType().IsLikelyString());

    // Generates:
    //      CMP indexOpnd, PropertyString::`vtable'                 -- check if index is property string
    //      JNE $helper
    //      MOV propertyCacheOpnd, index->propCache
    //      TEST baseOpnd, AtomTag                                  -- check base not tagged int
    //      JNE $helper
    //      MOV objectTypeOpnd, baseOpnd->type
    //      CMP [propertyCacheOpnd->type], objectTypeOpnd           -- check if object type match the cache
    //      JNE $helper
    //      CMP [propertyCacheOpnd->isInlineSlot,1]                 -- check if it is inline slots
    //      JEQ $inlineSlot
    //      MOV slotOpnd, [baseOpnd->slot]                          -- load the aux slot
    //      JMP $afterLabel
    // $inlineSlot:
    //      MOV slotOpnd, baseOpnd                                  -- use the object as start of the slot offset
    // $afterLabel:
    //      MOVZXW offsetOpnd, [propertyCacheOpnd->dataSlotIndex]   -- load the slot index
    //      <use [slotOpnd + offsetOpnd * PtrSize]>


    //      CMP indexOpnd, PropertyString::`vtable'                 -- check if index is property string
    //      JNE $helper

    this->m_lowerer->InsertCompareBranch(
        IR::IndirOpnd::New(indexOpnd, 0, TyMachPtr, this->m_func),
        m_lowerer->LoadVTableValueOpnd(instrInsert, VTableValue::VtablePropertyString),
        Js::OpCode::BrNeq_A, labelHelper, instrInsert);

    //      MOV propertyCacheOpnd, indexOpnd->propCache
    IR::RegOpnd * propertyCacheOpnd = IR::RegOpnd::New(TyMachPtr, this->m_func);
    IR::Instr * loadPropertyCacheInstr = IR::Instr::New(Js::OpCode::MOV, propertyCacheOpnd,
        IR::IndirOpnd::New(indexOpnd, Js::PropertyString::GetOffsetOfPropertyCache(), TyMachPtr,
        this->m_func), this->m_func);
    instrInsert->InsertBefore(loadPropertyCacheInstr);

    //      TEST baseOpnd, AtomTag                              -- check base not tagged int
    //      JNE $helper

    if(!baseOpnd->IsNotTaggedValue())
    {
        GenerateObjectTest(baseOpnd, instrInsert, labelHelper);
    }

    //      MOV s2, baseOpnd->type
    //      CMP [propertyCacheOpnd->type], s2                  -- check if object type match the cache
    //      JNE $helper
    IR::RegOpnd * objectTypeOpnd = IR::RegOpnd::New(TyMachPtr, this->m_func);
    IR::Instr * loadObjectTypeInstr = IR::Instr::New(Js::OpCode::MOV,
        objectTypeOpnd, IR::IndirOpnd::New(baseOpnd, Js::RecyclableObject::GetOffsetOfType(), TyMachPtr, this->m_func),
        this->m_func);
    instrInsert->InsertBefore(loadObjectTypeInstr);

    IR::Instr * checkTypeInstr = IR::Instr::New(Js::OpCode::CMP, this->m_func);
    checkTypeInstr->SetSrc1(IR::IndirOpnd::New(propertyCacheOpnd, (int32)offsetof(Js::PropertyCache, type), TyMachPtr, this->m_func));
    checkTypeInstr->SetSrc2(objectTypeOpnd);
    instrInsert->InsertBefore(checkTypeInstr);
    instrInsert->InsertBefore(IR::BranchInstr::New(Js::OpCode::JNE, labelHelper, this->m_func));

    if (isStore)
    {
        IR::IndirOpnd* isStoreEnabledOpnd = IR::IndirOpnd::New(propertyCacheOpnd, (int32)offsetof(Js::PropertyCache, isStoreFieldEnabled), TyInt8, this->m_func);
        IR::IntConstOpnd* zeroOpnd = IR::IntConstOpnd::New(0, TyInt8, this->m_func, /* dontEncode = */ true);
        this->m_lowerer->InsertCompareBranch(isStoreEnabledOpnd, zeroOpnd, Js::OpCode::BrEq_A, labelHelper, instrInsert);
    }

    //      CMP [propertyCacheOpnd->isInlineSlot,1]            -- check if it is inline slots
    //      JEQ $inlineSlot
    IR::Instr * inlineSlotTestInstr = IR::Instr::New(Js::OpCode::CMP, this->m_func);
    inlineSlotTestInstr->SetSrc1(IR::IndirOpnd::New(propertyCacheOpnd, (int32)offsetof(Js::PropertyCache, isInlineSlot), TyInt8, this->m_func));
    inlineSlotTestInstr->SetSrc2(IR::IntConstOpnd::New(1, TyInt8, this->m_func));
    instrInsert->InsertBefore(inlineSlotTestInstr);

    IR::LabelInstr * isInlineSlotLabel = IR::LabelInstr::New(Js::OpCode::Label, this->m_func);
    instrInsert->InsertBefore(IR::BranchInstr::New(Js::OpCode::JEQ, isInlineSlotLabel, this->m_func));

    //      MOV slotOpnd, [baseOpnd->slot]                -- load the aux slot
    //      JMP $afterLabel
    IR::RegOpnd * slotOpnd = IR::RegOpnd::New(TyMachPtr, this->m_func);
    instrInsert->InsertBefore(IR::Instr::New(Js::OpCode::MOV, slotOpnd,
        IR::IndirOpnd::New(baseOpnd, Js::DynamicObject::GetOffsetOfAuxSlots(), TyMachPtr, this->m_func), this->m_func));

    IR::LabelInstr * afterLabel = IR::LabelInstr::New(Js::OpCode::Label, this->m_func);
    instrInsert->InsertBefore(IR::BranchInstr::New(Js::OpCode::JMP, afterLabel, this->m_func));

    // $inlineSlot:
    //      MOV slotOpnd, baseOpnd                          -- use the object as start of the slot offset
    instrInsert->InsertBefore(isInlineSlotLabel);
    instrInsert->InsertBefore(IR::Instr::New(Js::OpCode::MOV, slotOpnd, baseOpnd, this->m_func));

    // $afterLabel:
    //      MOVZXW offsetOpnd, [propertyCacheOpnd->dataSlotIndex]          -- load the slot index
    instrInsert->InsertBefore(afterLabel);
    IR::RegOpnd * offsetOpnd = IR::RegOpnd::New(TyInt32, this->m_func);
    instrInsert->InsertBefore(IR::Instr::New(Js::OpCode::MOVZXW, offsetOpnd,
        IR::IndirOpnd::New(propertyCacheOpnd, (int32)offsetof(Js::PropertyCache, dataSlotIndex), TyUint16, this->m_func), this->m_func));

    // return [slotOpnd + offsetOpnd * PtrSize]
    return IR::IndirOpnd::New(slotOpnd, offsetOpnd, this->GetDefaultIndirScale(), TyVar, this->m_func);
}

void
LowererMD::GenerateFastBrBReturn(IR::Instr *instr)
{
    Assert(instr->m_opcode == Js::OpCode::BrOnEmpty || instr->m_opcode == Js::OpCode::BrOnNotEmpty);
    AssertMsg(instr->GetSrc1() != nullptr && instr->GetSrc2() == nullptr, "Expected 1 src opnds on BrB");
    Assert(instr->GetSrc1()->IsRegOpnd());

    IR::RegOpnd * forInEnumeratorOpnd = instr->GetSrc1()->AsRegOpnd();
    IR::LabelInstr * labelHelper = IR::LabelInstr::New(Js::OpCode::Label, this->m_func, true);

    // MOV firstPrototypeOpnd, forInEnumerator->firstPrototype
    // TEST firstPrototypeOpnd, firstPrototypeOpnd
    // JNE $helper
    IR::RegOpnd * firstPrototypeOpnd = IR::RegOpnd::New(TyMachPtr, this->m_func);
    instr->InsertBefore(IR::Instr::New(Js::OpCode::MOV, firstPrototypeOpnd,
        IR::IndirOpnd::New(forInEnumeratorOpnd, Js::ForInObjectEnumerator::GetOffsetOfFirstPrototype(), TyMachPtr, this->m_func), this->m_func));
    IR::Instr * checkFirstPrototypeNullInstr = IR::Instr::New(Js::OpCode::TEST, this->m_func);
    checkFirstPrototypeNullInstr->SetSrc1(firstPrototypeOpnd);
    checkFirstPrototypeNullInstr->SetSrc2(firstPrototypeOpnd);
    instr->InsertBefore(checkFirstPrototypeNullInstr);
    instr->InsertBefore(IR::BranchInstr::New(Js::OpCode::JNE, labelHelper, this->m_func));

    typedef Js::DynamicObjectSnapshotEnumeratorWPCache<Js::BigPropertyIndex, true, false> SmallDynamicObjectSnapshotEnumeratorWPCache;

    // MOV currentEnumeratorOpnd, forInEnumerator->currentEnumerator
    // CMP currentEnumeratorOpnd, SmallDynamicObjectSnapshotEnumeratorWPCache::`vtable
    // JNE $helper
    IR::RegOpnd * currentEnumeratorOpnd = IR::RegOpnd::New(TyMachPtr, this->m_func);
    instr->InsertBefore(IR::Instr::New(Js::OpCode::MOV, currentEnumeratorOpnd,
        IR::IndirOpnd::New(forInEnumeratorOpnd, Js::ForInObjectEnumerator::GetOffsetOfCurrentEnumerator(), TyMachPtr, this->m_func), this->m_func));

    this->m_lowerer->InsertCompareBranch(
        IR::IndirOpnd::New(currentEnumeratorOpnd, 0, TyMachPtr, this->m_func),
        m_lowerer->LoadVTableValueOpnd(instr, VTableValue::VtableSmallDynamicObjectSnapshotEnumeratorWPCache),
        Js::OpCode::BrNeq_A, labelHelper, instr);

    // MOV arrayEnumeratorOpnd, currentEnumerator->arrayEnumerator
    // TEST arrayEnumeratorOpnd, arrayEnumeratorOpnd
    // JNE $helper
    IR::RegOpnd * arrayEnumeratorOpnd = IR::RegOpnd::New(TyMachPtr, this->m_func);
    instr->InsertBefore(IR::Instr::New(Js::OpCode::MOV, arrayEnumeratorOpnd,
        IR::IndirOpnd::New(currentEnumeratorOpnd, SmallDynamicObjectSnapshotEnumeratorWPCache::GetOffsetOfArrayEnumerator(), TyMachPtr, this->m_func), this->m_func));
    IR::Instr * checkArrayEnumeratorNullInstr = IR::Instr::New(Js::OpCode::TEST, this->m_func);
    checkArrayEnumeratorNullInstr->SetSrc1(arrayEnumeratorOpnd);
    checkArrayEnumeratorNullInstr->SetSrc2(arrayEnumeratorOpnd);
    instr->InsertBefore(checkArrayEnumeratorNullInstr);
    instr->InsertBefore(IR::BranchInstr::New(Js::OpCode::JNE, labelHelper, this->m_func));

    // MOV objectOpnd, currentEnumerator->object
    // MOV initialTypeOpnd, currentEnumerator->initialType
    // CMP initialTypeOpnd, objectOpnd->type
    // JNE $helper
    IR::RegOpnd * objectOpnd = IR::RegOpnd::New(TyMachPtr, this->m_func);
    instr->InsertBefore(IR::Instr::New(Js::OpCode::MOV, objectOpnd,
        IR::IndirOpnd::New(currentEnumeratorOpnd, SmallDynamicObjectSnapshotEnumeratorWPCache::GetOffsetOfObject(), TyMachPtr, this->m_func), this->m_func));
    IR::RegOpnd * initialTypeOpnd = IR::RegOpnd::New(TyMachPtr, this->m_func);
    instr->InsertBefore(IR::Instr::New(Js::OpCode::MOV, initialTypeOpnd,
        IR::IndirOpnd::New(currentEnumeratorOpnd, SmallDynamicObjectSnapshotEnumeratorWPCache::GetOffsetOfInitialType(), TyMachPtr, this->m_func), this->m_func));

    IR::Instr * checkTypeInstr = IR::Instr::New(Js::OpCode::CMP, this->m_func);
    checkTypeInstr->SetSrc1(initialTypeOpnd);
    checkTypeInstr->SetSrc2(IR::IndirOpnd::New(objectOpnd, Js::DynamicObject::GetOffsetOfType(), TyMachPtr, this->m_func));
    instr->InsertBefore(checkTypeInstr);
    instr->InsertBefore(IR::BranchInstr::New(Js::OpCode::JNE, labelHelper, this->m_func));

    // MOV enumeratedCountOpnd, currentEnumeratorOpnd->enumeratedCount
    // MOV cachedDataOpnd, currentEnumeratorOpnd->cachedData
    // CMP enumeratedCountOpnd, cachedDataOpnd->cachedCount
    // JGE $helper
    IR::RegOpnd * enumeratedCountOpnd = IR::RegOpnd::New(TyUint32, m_func);
    instr->InsertBefore(IR::Instr::New(Js::OpCode::MOV, enumeratedCountOpnd,
        IR::IndirOpnd::New(currentEnumeratorOpnd, SmallDynamicObjectSnapshotEnumeratorWPCache::GetOffsetOfEnumeratedCount(), TyUint32, this->m_func), this->m_func));
    IR::RegOpnd * cachedDataOpnd = IR::RegOpnd::New(TyMachPtr, m_func);
    instr->InsertBefore(IR::Instr::New(Js::OpCode::MOV, cachedDataOpnd,
        IR::IndirOpnd::New(currentEnumeratorOpnd, SmallDynamicObjectSnapshotEnumeratorWPCache::GetOffsetOfCachedData(), TyMachPtr, this->m_func), this->m_func));

    IR::Instr * checkEnumeratedCountInstr = IR::Instr::New(Js::OpCode::CMP, this->m_func);
    checkEnumeratedCountInstr->SetSrc1(enumeratedCountOpnd);
    checkEnumeratedCountInstr->SetSrc2(IR::IndirOpnd::New(cachedDataOpnd, SmallDynamicObjectSnapshotEnumeratorWPCache::GetOffsetOfCachedDataCachedCount(), TyUint32, this->m_func));
    instr->InsertBefore(checkEnumeratedCountInstr);
    instr->InsertBefore(IR::BranchInstr::New(Js::OpCode::JGE, labelHelper, this->m_func));

    // MOV propertyAttributesOpnd, cachedData->attributes
    // MOV objectPropertyAttributesOpnd, propertyAttributesOpnd[enumeratedCount]
    // CMP objectPropertyAttributesOpnd & PropertyEnumerable, PropertyEnumerable
    // JNE $helper
    IR::RegOpnd * propertyAttributesOpnd = IR::RegOpnd::New(TyMachPtr, m_func);
    instr->InsertBefore(IR::Instr::New(Js::OpCode::MOV, propertyAttributesOpnd,
        IR::IndirOpnd::New(cachedDataOpnd, SmallDynamicObjectSnapshotEnumeratorWPCache::GetOffsetOfCachedDataPropertyAttributes(), TyMachPtr, this->m_func), this->m_func));
    IR::RegOpnd * objectPropertyAttributesOpnd = IR::RegOpnd::New(TyUint8, m_func);
    instr->InsertBefore(IR::Instr::New(Js::OpCode::MOV, objectPropertyAttributesOpnd,
        IR::IndirOpnd::New(propertyAttributesOpnd, enumeratedCountOpnd, IndirScale1, TyUint8, this->m_func), this->m_func));

    IR::Instr * andPropertyEnumerableInstr = Lowerer::InsertAnd(IR::RegOpnd::New(TyUint8, instr->m_func), objectPropertyAttributesOpnd, IR::IntConstOpnd::New(0x01, TyUint8, this->m_func), instr);

    IR::Instr * checkEnumerableInstr = IR::Instr::New(Js::OpCode::CMP, this->m_func);
    checkEnumerableInstr->SetSrc1(andPropertyEnumerableInstr->GetDst());
    checkEnumerableInstr->SetSrc2(IR::IntConstOpnd::New(0x01, TyUint8, this->m_func));
    instr->InsertBefore(checkEnumerableInstr);
    instr->InsertBefore(IR::BranchInstr::New(Js::OpCode::JNE, labelHelper, this->m_func));

    IR::Opnd * opndDst = instr->GetDst(); // ForIn result propertyString
    Assert(opndDst->IsRegOpnd());

    // MOV stringsOpnd, cachedData->strings
    // MOV opndDst, stringsOpnd[enumeratedCount]
    IR::RegOpnd * stringsOpnd = IR::RegOpnd::New(TyMachPtr, m_func);
    instr->InsertBefore(IR::Instr::New(Js::OpCode::MOV, stringsOpnd,
        IR::IndirOpnd::New(cachedDataOpnd, SmallDynamicObjectSnapshotEnumeratorWPCache::GetOffsetOfCachedDataStrings(), TyMachPtr, this->m_func), this->m_func));
    instr->InsertBefore(IR::Instr::New(Js::OpCode::MOV, opndDst,
        IR::IndirOpnd::New(stringsOpnd, enumeratedCountOpnd, this->GetDefaultIndirScale(), TyVar, this->m_func), this->m_func));

    // MOV indexesOpnd, cachedData->indexes
    // MOV objectIndexOpnd, indexesOpnd[enumeratedCount]
    // MOV currentEnumeratorOpnd->objectIndex, objectIndexOpnd
    IR::RegOpnd * indexesOpnd = IR::RegOpnd::New(TyMachPtr, m_func);
    instr->InsertBefore(IR::Instr::New(Js::OpCode::MOV, indexesOpnd,
        IR::IndirOpnd::New(cachedDataOpnd, SmallDynamicObjectSnapshotEnumeratorWPCache::GetOffsetOfCachedDataIndexes(), TyMachPtr, this->m_func), this->m_func));
    IR::RegOpnd * objectIndexOpnd = IR::RegOpnd::New(TyUint32, m_func);
    instr->InsertBefore(IR::Instr::New(Js::OpCode::MOV, objectIndexOpnd,
        IR::IndirOpnd::New(indexesOpnd, enumeratedCountOpnd, IndirScale4, TyUint32, this->m_func), this->m_func));
    instr->InsertBefore(IR::Instr::New(Js::OpCode::MOV,
        IR::IndirOpnd::New(currentEnumeratorOpnd, SmallDynamicObjectSnapshotEnumeratorWPCache::GetOffsetOfObjectIndex(), TyUint32, this->m_func),
        objectIndexOpnd, this->m_func));

    // INC enumeratedCountOpnd
    // MOV currentEnumeratorOpnd->enumeratedCount, enumeratedCountOpnd
    instr->InsertBefore(IR::Instr::New(Js::OpCode::INC, enumeratedCountOpnd, enumeratedCountOpnd, this->m_func));
    instr->InsertBefore(IR::Instr::New(Js::OpCode::MOV,
        IR::IndirOpnd::New(currentEnumeratorOpnd, SmallDynamicObjectSnapshotEnumeratorWPCache::GetOffsetOfEnumeratedCount(), TyUint32, this->m_func),
        enumeratedCountOpnd, this->m_func));

    // We know result propertyString (opndDst) != NULL
    IR::LabelInstr * labelAfter = instr->GetOrCreateContinueLabel();
    instr->InsertBefore(IR::BranchInstr::New(Js::OpCode::JMP,
        instr->m_opcode == Js::OpCode::BrOnNotEmpty ? instr->AsBranchInstr()->GetTarget() : labelAfter,
        this->m_func));

    // $helper
    instr->InsertBefore(labelHelper);
    // $after
}

// Inlines fast-path for int Mul/Add or int Mul/Sub.  If not int, call MulAdd/MulSub helper
bool LowererMD::TryGenerateFastMulAdd(IR::Instr * instrAdd, IR::Instr ** pInstrPrev)
{
    IR::Instr *instrMul = instrAdd->GetPrevRealInstrOrLabel();
    IR::Opnd *addSrc;
    IR::RegOpnd *addCommonSrcOpnd;

    Assert(instrAdd->m_opcode == Js::OpCode::Add_A || instrAdd->m_opcode == Js::OpCode::Sub_A);

    bool isSub = (instrAdd->m_opcode == Js::OpCode::Sub_A) ? true : false;

    // Mul needs to be a single def reg
    if (instrMul->m_opcode != Js::OpCode::Mul_A || instrMul->GetDst()->IsRegOpnd() == false)
    {
        // Cannot generate MulAdd
        return false;
    }

    if (instrMul->HasBailOutInfo())
    {
        // Bailout will be generated for the Add, but not the Mul.
        // We could handle this, but this path isn't used that much anymore.
        return false;
    }

    IR::RegOpnd *regMulDst = instrMul->GetDst()->AsRegOpnd();

    if (regMulDst->m_sym->m_isSingleDef == false)
    {
        // Cannot generate MulAdd
        return false;
    }

    // Only handle a * b + c, so dst of Mul needs to match left source of Add
    if (instrMul->GetDst()->IsEqual(instrAdd->GetSrc1()))
    {
        addCommonSrcOpnd = instrAdd->GetSrc1()->AsRegOpnd();
        addSrc = instrAdd->GetSrc2();
    }
    else if (instrMul->GetDst()->IsEqual(instrAdd->GetSrc2()))
    {
        addSrc = instrAdd->GetSrc1();
        addCommonSrcOpnd = instrAdd->GetSrc2()->AsRegOpnd();
    }
    else
    {
        return false;
    }

    // Only handle a * b + c where c != a * b
    if (instrAdd->GetSrc1()->IsEqual(instrAdd->GetSrc2()))
    {
        return false;
    }

    if (addCommonSrcOpnd->m_isTempLastUse == false)
    {
        return false;
    }

    IR::Opnd *mulSrc1 = instrMul->GetSrc1();
    IR::Opnd *mulSrc2 = instrMul->GetSrc2();

    if (mulSrc1->IsRegOpnd() && mulSrc1->AsRegOpnd()->IsTaggedInt()
        && mulSrc2->IsRegOpnd() && mulSrc2->AsRegOpnd()->IsTaggedInt())
    {
        return false;
    }

    // Save prevInstr for the main lower loop
    *pInstrPrev = instrMul->m_prev;

    // Generate int31 fast-path for Mul, go to MulAdd helper if it fails, or one of the source is marked notInt
    if (!(addSrc->IsRegOpnd() && addSrc->AsRegOpnd()->IsNotInt())
        && !(mulSrc1->IsRegOpnd() && mulSrc1->AsRegOpnd()->IsNotInt())
        && !(mulSrc2->IsRegOpnd() && mulSrc2->AsRegOpnd()->IsNotInt()))
    {
        this->GenerateFastMul(instrMul);

        IR::LabelInstr *labelHelper = IR::LabelInstr::New(Js::OpCode::Label, this->m_func, true);
        IR::Instr *instr = IR::BranchInstr::New(Js::OpCode::JMP, labelHelper, this->m_func);
        instrMul->InsertBefore(instr);

        // Generate int31 fast-path for Add
        bool success;
        if (isSub)
        {
            success = this->GenerateFastSub(instrAdd);
        }
        else
        {
            success = this->GenerateFastAdd(instrAdd);
        }

        if (!success)
        {
            labelHelper->isOpHelper = false;
        }
        // Generate MulAdd helper call
        instrAdd->InsertBefore(labelHelper);
    }

    if (instrAdd->dstIsTempNumber)
    {
        m_lowerer->LoadHelperTemp(instrAdd, instrAdd);
    }
    else
    {
        IR::Opnd *tempOpnd = IR::IntConstOpnd::New(0, TyInt32, this->m_func);
        this->LoadHelperArgument(instrAdd, tempOpnd);
    }
    this->m_lowerer->LoadScriptContext(instrAdd);

    IR::JnHelperMethod helper;

    if (addSrc == instrAdd->GetSrc2())
    {
        instrAdd->FreeSrc1();
        IR::Opnd *addOpnd = instrAdd->UnlinkSrc2();
        this->LoadHelperArgument(instrAdd, addOpnd);
        helper = isSub ? IR::HelperOp_MulSubRight : IR::HelperOp_MulAddRight;
    }
    else
    {
        instrAdd->FreeSrc2();
        IR::Opnd *addOpnd = instrAdd->UnlinkSrc1();
        this->LoadHelperArgument(instrAdd, addOpnd);
        helper = isSub ? IR::HelperOp_MulSubLeft : IR::HelperOp_MulAddLeft;
    }

    IR::Opnd *src2 = instrMul->UnlinkSrc2();
    this->LoadHelperArgument(instrAdd, src2);

    IR::Opnd *src1 = instrMul->UnlinkSrc1();
    this->LoadHelperArgument(instrAdd, src1);

    this->ChangeToHelperCall(instrAdd, helper);

    instrMul->Remove();

    return true;
}

void
LowererMD::GenerateFastAbs(IR::Opnd *dst, IR::Opnd *src, IR::Instr *callInstr, IR::Instr *insertInstr, IR::LabelInstr *labelHelper, IR::LabelInstr *doneLabel)
{
    //      TEST src1, AtomTag
    //      JEQ $float
    //      MOV EAX, src
    //      SAR EAX, AtomTag_Int32
    //      CDQ
    //      XOR EAX, EDX
    //      SUB EAX, EDX
    //      SHL EAX, AtomTag_Int32
    //      JO  $labelHelper
    //      INC EAX
    //      MOV dst, EAX
    //      JMP $done
    // $float
    //      CMP [src], JavascriptNumber.vtable
    //      JNE $helper
    //      MOVSD r1, [src + offsetof(value)]
    //      ANDPD r1, absDoubleCst
    //      dst = DoubleToVar(r1)

    IR::Instr      *instr      = nullptr;
    IR::LabelInstr *labelFloat = nullptr;
    bool            isInt      = false;
    bool            isNotInt   = false;

    if (src->IsRegOpnd())
    {
        if (src->AsRegOpnd()->IsTaggedInt())
        {
            isInt = true;

        }
        else if (src->AsRegOpnd()->IsNotInt())
        {
            isNotInt = true;
        }
    }
    else if (src->IsAddrOpnd())
    {
        IR::AddrOpnd *varOpnd = src->AsAddrOpnd();
        Assert(varOpnd->IsVar() && Js::TaggedInt::Is(varOpnd->m_address));

#ifdef _M_X64
        __int64 absValue = ::_abs64(Js::TaggedInt::ToInt32(varOpnd->m_address));
#else
        __int32 absValue = ::abs(Js::TaggedInt::ToInt32(varOpnd->m_address));
#endif

        if (!Js::TaggedInt::IsOverflow(absValue))
        {
            varOpnd->SetAddress(Js::TaggedInt::ToVarUnchecked((__int32)absValue), IR::AddrOpndKindConstantVar);

            instr = IR::Instr::New(Js::OpCode::MOV, dst, varOpnd, this->m_func);
            insertInstr->InsertBefore(instr);

            return;
        }
    }

    if (src->IsRegOpnd() == false)
    {
        IR::RegOpnd *regOpnd = IR::RegOpnd::New(TyVar, this->m_func);
        instr = IR::Instr::New(Js::OpCode::MOV, regOpnd, src, this->m_func);
        insertInstr->InsertBefore(instr);
        src = regOpnd;
    }

#ifdef _M_IX86
    bool emitFloatAbs = !isInt && AutoSystemInfo::Data.SSE2Available();
#else
    bool emitFloatAbs = !isInt;
#endif

    if (!isNotInt)
    {
        if (!isInt)
        {
            IR::LabelInstr *label = labelHelper;
            if (emitFloatAbs)
            {
                label = labelFloat = IR::LabelInstr::New(Js::OpCode::Label, this->m_func);
            }

            GenerateSmIntTest(src, insertInstr, label);
        }

        //      MOV EAX, src
        IR::RegOpnd *regEAX = IR::RegOpnd::New(TyInt32, this->m_func);
        regEAX->SetReg(LowererMDArch::GetRegIMulDestLower());

        instr = IR::Instr::New(Js::OpCode::MOV, regEAX, src, this->m_func);
        insertInstr->InsertBefore(instr);

#ifdef _M_IX86
        //      SAR EAX, AtomTag_Int32
        instr = IR::Instr::New(Js::OpCode::SAR, regEAX, regEAX, IR::IntConstOpnd::New(Js::AtomTag_Int32, TyInt32, this->m_func), this->m_func);
        insertInstr->InsertBefore(instr);
#endif

        IR::RegOpnd *regEDX = IR::RegOpnd::New(TyInt32, this->m_func);
        regEDX->SetReg(LowererMDArch::GetRegIMulHighDestLower());

        //      CDQ
        // Note: put EDX on dst to give of def to the EDX lifetime
        instr = IR::Instr::New(Js::OpCode::CDQ, regEDX, this->m_func);
        insertInstr->InsertBefore(instr);

        //      XOR EAX, EDX
        instr = IR::Instr::New(Js::OpCode::XOR, regEAX, regEAX, regEDX, this->m_func);
        insertInstr->InsertBefore(instr);

        //      SUB EAX, EDX
        instr = IR::Instr::New(Js::OpCode::SUB, regEAX, regEAX, regEDX, this->m_func);
        insertInstr->InsertBefore(instr);

#ifdef _M_X64
        // abs(INT_MIN) overflows a 32 bit integer.
        //      JO  $labelHelper
        instr = IR::BranchInstr::New(Js::OpCode::JO, labelHelper, this->m_func);
        insertInstr->InsertBefore(instr);
#endif

#ifdef _M_IX86
        //      SHL EAX, AtomTag_Int32
        instr = IR::Instr::New(Js::OpCode::SHL, regEAX, regEAX, IR::IntConstOpnd::New(Js::AtomTag_Int32, TyInt32, this->m_func), this->m_func);
        insertInstr->InsertBefore(instr);

        //      JO  $labelHelper
        instr = IR::BranchInstr::New(Js::OpCode::JO, labelHelper, this->m_func);
        insertInstr->InsertBefore(instr);

        //      INC EAX
        instr = IR::Instr::New(Js::OpCode::INC, regEAX, regEAX, this->m_func);
        insertInstr->InsertBefore(instr);
#endif

        //      MOV dst, EAX
        instr = IR::Instr::New(Js::OpCode::MOV, dst, regEAX, this->m_func);
        insertInstr->InsertBefore(instr);

#ifdef _M_X64
        GenerateInt32ToVarConversion(dst, insertInstr);
#endif
    }

    if (labelFloat)
    {
        // JMP $done
        instr = IR::BranchInstr::New(Js::OpCode::JMP, doneLabel, this->m_func);
        insertInstr->InsertBefore(instr);

        // $float
        insertInstr->InsertBefore(labelFloat);
    }

    if (emitFloatAbs)
    {
#if defined(_M_IX86)
        // CMP [src], JavascriptNumber.vtable
        IR::Opnd *opnd = IR::IndirOpnd::New(src->AsRegOpnd(), (int32)0, TyMachPtr, this->m_func);
        instr = IR::Instr::New(Js::OpCode::CMP, this->m_func);
        instr->SetSrc1(opnd);
        instr->SetSrc2(m_lowerer->LoadVTableValueOpnd(insertInstr, VTableValue::VtableJavascriptNumber));
        insertInstr->InsertBefore(instr);

        // JNE $helper
        instr = IR::BranchInstr::New(Js::OpCode::JNE, labelHelper, this->m_func);
        insertInstr->InsertBefore(instr);

        // MOVSD r1, [src + offsetof(value)]
        opnd = IR::IndirOpnd::New(src->AsRegOpnd(), Js::JavascriptNumber::GetValueOffset(), TyMachDouble, this->m_func);
        IR::RegOpnd *regOpnd = IR::RegOpnd::New(TyMachDouble, this->m_func);
        instr = IR::Instr::New(Js::OpCode::MOVSD, regOpnd, opnd, this->m_func);
        insertInstr->InsertBefore(instr);

        this->GenerateFloatAbs(regOpnd, insertInstr);

        // dst = DoubleToVar(r1)
        SaveDoubleToVar(callInstr->GetDst()->AsRegOpnd(), regOpnd, callInstr, insertInstr);
#elif defined(_M_X64)
        // if (typeof(src) == double)
        IR::RegOpnd *src64 = src->AsRegOpnd();
        GenerateFloatTest(src64, insertInstr, labelHelper);

        // dst64 = MOV src64
        insertInstr->InsertBefore(IR::Instr::New(Js::OpCode::MOV, dst, src64, this->m_func));

        // Unconditionally set the sign bit. This will get XORd away when we remove the tag.
        // dst64 = OR 0x8000000000000000
        insertInstr->InsertBefore(IR::Instr::New(Js::OpCode::OR, dst, dst, IR::AddrOpnd::New((void *)MachSignBit, IR::AddrOpndKindConstant, this->m_func), this->m_func));
#endif
    }
    else if(!isInt)
    {
        // The source is not known to be a tagged int, so either it's definitely not an int (isNotInt), or the int version of
        // abs failed the tag check and jumped here. We can't emit the float version of abs (!emitFloatAbs) due to SSE2 not
        // being available, so jump straight to the helper.

        // JMP $helper
        instr = IR::BranchInstr::New(Js::OpCode::JMP, labelHelper, this->m_func);
        insertInstr->InsertBefore(instr);
    }
}

IR::Instr * LowererMD::GenerateFloatAbs(IR::RegOpnd * regOpnd, IR::Instr * insertInstr)
{
    // ANDPS reg, absDoubleCst
    IR::Opnd * opnd;
    if (regOpnd->IsFloat64())
    {
        opnd = m_lowerer->LoadLibraryValueOpnd(insertInstr, LibraryValue::ValueAbsDoubleCst);
    }
    else
    {
        Assert(regOpnd->IsFloat32());
        opnd = IR::MemRefOpnd::New((void *)&Js::JavascriptNumber::AbsFloatCst, TyFloat32, this->m_func, IR::AddrOpndKindDynamicFloatRef);
    }

    // ANDPS has smaller encoding then ANDPD
    IR::Instr * instr = IR::Instr::New(Js::OpCode::ANDPS, regOpnd, regOpnd, opnd, this->m_func);
    insertInstr->InsertBefore(instr);
    Legalize(instr);
    return instr;
}

bool LowererMD::GenerateFastCharAt(Js::BuiltinFunction index, IR::Opnd *dst, IR::Opnd *srcStr, IR::Opnd *srcIndex, IR::Instr *callInstr,
                                  IR::Instr *insertInstr, IR::LabelInstr *labelHelper, IR::LabelInstr *doneLabel)
{
    //  if regSrcStr is not object, JMP $helper
    //  CMP [regSrcStr + offset(type)] , static string type   -- check base string type
    //  JNE $helper
    //  MOV r1, [regSrcStr + offset(m_pszValue)]
    //  TEST r1, r1
    //  JEQ $helper
    //  MOV r2, srcIndex
    //  If r2 is not int, JMP $helper
    //  Convert r2 to int
    //  CMP [regSrcStr + offsetof(length)], r2
    //  JBE $helper
    //  MOVZX r2, [r1 + r2 * 2]
    //  if (charAt)
    //      PUSH r1
    //      PUSH scriptContext
    //      CALL GetStringFromChar
    //      MOV dst, EAX
    //  else (charCodeAt)
    //      if (codePointAt)
    //          Lowerer.GenerateFastCodePointAt -- Common inline functions
    //      Convert r2 to Var
    //      MOV dst, r2
    bool isInt = false;
    bool isNotTaggedValue = false;
    IR::Instr *instr;
    IR::RegOpnd *regSrcStr;

    if (srcStr->IsRegOpnd())
    {
        if (srcStr->AsRegOpnd()->IsTaggedInt())
        {
            isInt = true;

        }
        else if (srcStr->AsRegOpnd()->IsNotTaggedValue())
        {
            isNotTaggedValue = true;
        }
    }

    if (srcStr->IsRegOpnd() == false)
    {
        IR::RegOpnd *regOpnd = IR::RegOpnd::New(TyVar, this->m_func);
        instr = IR::Instr::New(Js::OpCode::MOV, regOpnd, srcStr, this->m_func);
        insertInstr->InsertBefore(instr);
        regSrcStr = regOpnd;
    }
    else
    {
        regSrcStr = srcStr->AsRegOpnd();
    }

    if (!isNotTaggedValue)
    {
        if (!isInt)
        {
            GenerateObjectTest(regSrcStr, insertInstr, labelHelper);
        }
        else
        {
            // Insert delete branch opcode to tell the dbChecks not to assert on this helper label
            IR::Instr *fakeBr = IR::PragmaInstr::New(Js::OpCode::DeletedNonHelperBranch, 0, this->m_func);
            insertInstr->InsertBefore(fakeBr);

            instr = IR::BranchInstr::New(Js::OpCode::JMP, labelHelper, this->m_func);
            insertInstr->InsertBefore(instr);
        }
    }

    // Bail out if index a constant and is less than zero.
    if (srcIndex->IsAddrOpnd() && Js::TaggedInt::ToInt32(srcIndex->AsAddrOpnd()->m_address) < 0)
    {
        labelHelper->isOpHelper = false;
        instr = IR::BranchInstr::New(Js::OpCode::JMP, labelHelper, this->m_func);
        insertInstr->InsertBefore(instr);
        return false;
    }

    this->m_lowerer->GenerateStringTest(regSrcStr, insertInstr, labelHelper, nullptr, false);

    // r1 contains the value of the wchar_t* pointer inside JavascriptString.
    // MOV r1, [regSrcStr + offset(m_pszValue)]
    IR::RegOpnd *r1 = IR::RegOpnd::New(TyMachReg, this->m_func);
    IR::IndirOpnd * indirOpnd = IR::IndirOpnd::New(regSrcStr->AsRegOpnd(), Js::JavascriptString::GetOffsetOfpszValue(), TyMachPtr, this->m_func);
    instr = IR::Instr::New(Js::OpCode::MOV, r1, indirOpnd, this->m_func);
    insertInstr->InsertBefore(instr);

    // TEST r1, r1 -- Null pointer test
    instr = IR::Instr::New(Js::OpCode::TEST, this->m_func);
    instr->SetSrc1(r1);
    instr->SetSrc2(r1);
    insertInstr->InsertBefore(instr);

    // JEQ $helper
    instr = IR::BranchInstr::New(Js::OpCode::JEQ, labelHelper, this->m_func);
    insertInstr->InsertBefore(instr);

    IR::IndirOpnd *strLength = IR::IndirOpnd::New(regSrcStr, offsetof(Js::JavascriptString, m_charLength), TyUint32, this->m_func);
    if (srcIndex->IsAddrOpnd())
    {
        // CMP [regSrcStr + offsetof(length)], index
        instr = IR::Instr::New(Js::OpCode::CMP, this->m_func);
        instr->SetSrc1(strLength);
        instr->SetSrc2(IR::IntConstOpnd::New(Js::TaggedInt::ToUInt32(srcIndex->AsAddrOpnd()->m_address), TyUint32, this->m_func));
        insertInstr->InsertBefore(instr);

        // Use unsigned compare, this should handle negative indexes as well (they become > INT_MAX)
        // JBE $helper
        instr = IR::BranchInstr::New(Js::OpCode::JBE, labelHelper, this->m_func);
        insertInstr->InsertBefore(instr);

        indirOpnd = IR::IndirOpnd::New(r1, Js::TaggedInt::ToUInt32(srcIndex->AsAddrOpnd()->m_address) * sizeof(wchar_t), TyInt16, this->m_func);
    }
    else
    {
        IR::RegOpnd *r2 = IR::RegOpnd::New(TyVar, this->m_func);
        // MOV r2, srcIndex
        instr = IR::Instr::New(Js::OpCode::MOV, r2, srcIndex, this->m_func);
        insertInstr->InsertBefore(instr);

        if (!srcIndex->IsRegOpnd() || !srcIndex->AsRegOpnd()->IsTaggedInt())
        {
            GenerateSmIntTest(r2, insertInstr, labelHelper);
        }
#if INT32VAR
        // Remove the tag
        // MOV r2, [32-bit] r2
        IR::Opnd * r2_32 = r2->UseWithNewType(TyInt32, this->m_func);
        instr = IR::Instr::New(Js::OpCode::MOVSXD, r2, r2_32, this->m_func);
        insertInstr->InsertBefore(instr);
        r2 = r2_32->AsRegOpnd();
#else
        // r2 = SAR r2, VarTag_Shift
        instr = IR::Instr::New(Js::OpCode::SAR, r2, r2, IR::IntConstOpnd::New(Js::VarTag_Shift, TyInt8, this->m_func), this->m_func);
        insertInstr->InsertBefore(instr);
#endif

        // CMP [regSrcStr + offsetof(length)], r2
        instr = IR::Instr::New(Js::OpCode::CMP, this->m_func);
        instr->SetSrc1(strLength);
        instr->SetSrc2(r2);
        insertInstr->InsertBefore(instr);

        if (r2->GetSize() != MachPtr)
        {
            r2 = r2->UseWithNewType(TyMachPtr, this->m_func)->AsRegOpnd();
        }

        // Use unsigned compare, this should handle negative indexes as well (they become > INT_MAX)
        // JBE $helper
        instr = IR::BranchInstr::New(Js::OpCode::JBE, labelHelper, this->m_func);
        insertInstr->InsertBefore(instr);

        indirOpnd = IR::IndirOpnd::New(r1, r2, 1, TyInt16, this->m_func);
    }
    // MOVZX charReg, [r1 + r2 * 2]  -- this is the value of the char
    IR::RegOpnd *charReg = IR::RegOpnd::New(TyMachReg, this->m_func);
    instr = IR::Instr::New(Js::OpCode::MOVZXW, charReg, indirOpnd, this->m_func);
    insertInstr->InsertBefore(instr);
    if (index == Js::BuiltinFunction::String_CharAt)
    {
        this->m_lowerer->GenerateGetSingleCharString(charReg, dst, labelHelper, doneLabel, insertInstr, false);
    }
    else
    {
        Assert(index == Js::BuiltinFunction::String_CharCodeAt || index == Js::BuiltinFunction::String_CodePointAt);

        if (index == Js::BuiltinFunction::String_CodePointAt)
        {
            this->m_lowerer->GenerateFastInlineStringCodePointAt(insertInstr, this->m_func, strLength, srcIndex, charReg, r1);
        }

        GenerateInt32ToVarConversion(charReg, insertInstr);

        // MOV dst, charReg
        instr = IR::Instr::New(Js::OpCode::MOV, dst, charReg, this->m_func);
        insertInstr->InsertBefore(instr);
    }
    return true;
}

void
LowererMD::GenerateClz(IR::Instr * instr)
{
    Assert(instr->GetSrc1()->IsInt32() || instr->GetSrc1()->IsUInt32());
    Assert(IRType_IsNativeInt(instr->GetDst()->GetType()));
    if (AutoSystemInfo::Data.LZCntAvailable())
    {
        instr->m_opcode = Js::OpCode::LZCNT;
        Legalize(instr);
    }
    else
    {
        // tmp = BSR src
        // JE $label32
        // dst = SUB 31, tmp
        // JMP $done
        // label32:
        // dst = mov 32;
        // $done
        IR::LabelInstr * doneLabel = Lowerer::InsertLabel(false, instr->m_next);
        IR::Opnd * dst = instr->UnlinkDst();
        IR::Opnd * tmpOpnd = IR::RegOpnd::New(TyInt8, m_func);
        instr->SetDst(tmpOpnd);
        instr->m_opcode = Js::OpCode::BSR;
        Legalize(instr);
        IR::LabelInstr * label32 = Lowerer::InsertLabel(false, doneLabel);
        instr = IR::BranchInstr::New(Js::OpCode::JEQ, label32, m_func);
        label32->InsertBefore(instr);
        Lowerer::InsertSub(false, dst, IR::IntConstOpnd::New(31, TyInt8, m_func), tmpOpnd, label32);
        Lowerer::InsertBranch(Js::OpCode::Br, doneLabel, label32);
        Lowerer::InsertMove(dst, IR::IntConstOpnd::New(32, TyInt8, m_func), doneLabel);
    }
}

#if !FLOATVAR
void
LowererMD::GenerateNumberAllocation(IR::RegOpnd * opndDst, IR::Instr * instrInsert, bool isHelper)
{
    Js::RecyclerJavascriptNumberAllocator * allocator = this->m_lowerer->GetScriptContext()->GetNumberAllocator();

    IR::Opnd * endAddressOpnd = m_lowerer->LoadNumberAllocatorValueOpnd(instrInsert, NumberAllocatorValue::NumberAllocatorEndAddress);
    IR::Opnd * freeObjectListOpnd = m_lowerer->LoadNumberAllocatorValueOpnd(instrInsert, NumberAllocatorValue::NumberAllocatorFreeObjectList);

    // MOV dst, allocator->freeObjectList
    IR::Instr * loadMemBlockInstr = IR::Instr::New(Js::OpCode::MOV, opndDst, freeObjectListOpnd, this->m_func);
    instrInsert->InsertBefore(loadMemBlockInstr);

    // LEA nextMemBlock, [dst + allocSize]
    IR::RegOpnd * nextMemBlockOpnd = IR::RegOpnd::New(TyMachPtr, this->m_func);
    IR::Instr * loadNextMemBlockInstr = IR::Instr::New(Js::OpCode::LEA, nextMemBlockOpnd,
        IR::IndirOpnd::New(opndDst, allocator->GetAlignedAllocSize(), TyMachPtr, this->m_func), this->m_func);
    instrInsert->InsertBefore(loadNextMemBlockInstr);

    // CMP nextMemBlock, allocator->endAddress
    IR::Instr * checkInstr = IR::Instr::New(Js::OpCode::CMP, this->m_func);
    checkInstr->SetSrc1(nextMemBlockOpnd);
    checkInstr->SetSrc2(endAddressOpnd);
    instrInsert->InsertBefore(checkInstr);

    // JA $helper
    IR::LabelInstr * helperLabel = IR::LabelInstr::New(Js::OpCode::Label, this->m_func, true);
    IR::BranchInstr * branchInstr = IR::BranchInstr::New(Js::OpCode::JA, helperLabel, this->m_func);
    instrInsert->InsertBefore(branchInstr);

    // MOV allocator->freeObjectList, nextMemBlock
    IR::Instr * setFreeObjectListInstr = IR::Instr::New(Js::OpCode::MOV, freeObjectListOpnd, nextMemBlockOpnd, this->m_func);
    instrInsert->InsertBefore(setFreeObjectListInstr);

    // JMP $done
    IR::LabelInstr * doneLabel = IR::LabelInstr::New(Js::OpCode::Label, this->m_func, isHelper);
    IR::BranchInstr * branchToDoneInstr = IR::BranchInstr::New(Js::OpCode::JMP, doneLabel, this->m_func);
    instrInsert->InsertBefore(branchToDoneInstr);

    // $helper:
    instrInsert->InsertBefore(helperLabel);

    // PUSH allocator
    this->LoadHelperArgument(instrInsert, m_lowerer->LoadScriptContextValueOpnd(instrInsert,  ScriptContextValue::ScriptContextNumberAllocator));

    // dst = Call AllocUninitalizedNumber
    IR::Instr * instrCall = IR::Instr::New(Js::OpCode::CALL, opndDst,
        IR::HelperCallOpnd::New(IR::HelperAllocUninitializedNumber, this->m_func), this->m_func);
    instrInsert->InsertBefore(instrCall);
    this->lowererMDArch.LowerCall(instrCall, 0);

    // $done:
    instrInsert->InsertBefore(doneLabel);
}
#endif

#ifdef _CONTROL_FLOW_GUARD
void
LowererMD::GenerateCFGCheck(IR::Opnd * entryPointOpnd, IR::Instr * insertBeforeInstr)
{
    //PreReserve segment at this point, as we will definitely using this segment for JITted code(in almost all cases)
    //This is for CFG check optimization
    IR::LabelInstr * callLabelInstr = nullptr;
    char * preReservedRegionStartAddress = nullptr;

    if (m_func->CanAllocInPreReservedHeapPageSegment())
    {
        PreReservedVirtualAllocWrapper * preReservedVirtualAllocator = m_func->GetScriptContext()->GetThreadContext()->GetPreReservedVirtualAllocator();
        preReservedRegionStartAddress = m_func->GetEmitBufferManager()->EnsurePreReservedPageAllocation(preReservedVirtualAllocator);
        if (preReservedRegionStartAddress != nullptr)
        {
            Assert(preReservedVirtualAllocator);
            char* endAddressOfSegment = (char*)preReservedVirtualAllocator->GetPreReservedEndAddress();

            int32 segmentSize = (int32) (endAddressOfSegment - preReservedRegionStartAddress);

            // Generate instructions for local Pre-Reserved Segment Range check

            IR::AddrOpnd * endAddressOfSegmentConstOpnd = IR::AddrOpnd::New(endAddressOfSegment, IR::AddrOpndKindDynamicMisc, m_func);
            IR::RegOpnd *resultOpnd = IR::RegOpnd::New(TyMachReg, this->m_func);
#if _M_IX86
            //resultOpnd = endAddressOfSegmentConstOpnd - entryPointOpnd
            IR::Instr* subInstr = IR::Instr::New(Js::OpCode::Sub_I4, resultOpnd, endAddressOfSegmentConstOpnd, entryPointOpnd, m_func);
            insertBeforeInstr->InsertBefore(subInstr);
            this->EmitInt4Instr(subInstr);
#elif _M_X64
            //MOV resultOpnd, endAddressOfSegment
            //resultOpnd = resultOpnd - entryPointOpnd

            IR::Instr   *movInstr = IR::Instr::New(Js::OpCode::MOV, resultOpnd, endAddressOfSegmentConstOpnd, this->m_func);
            insertBeforeInstr->InsertBefore(movInstr);
            IR::Instr* subInstr = IR::Instr::New(Js::OpCode::SUB, resultOpnd, resultOpnd, entryPointOpnd, m_func);
            insertBeforeInstr->InsertBefore(subInstr);
#endif
            //CMP subResultOpnd, segmentSize
            //JL $callLabelInstr:

            AssertMsg((size_t) segmentSize == (size_t) (endAddressOfSegment - preReservedRegionStartAddress), "Need a bigger datatype for segmentSize?");
            IR::IntConstOpnd * segmentSizeOpnd = IR::IntConstOpnd::New(segmentSize, IRType::TyInt32, m_func, true);
            callLabelInstr = IR::LabelInstr::New(Js::OpCode::Label, m_func);
            this->m_lowerer->InsertCompareBranch(resultOpnd, segmentSizeOpnd, Js::OpCode::JBE, callLabelInstr, insertBeforeInstr);
        }
    }

    //MOV  ecx, entryPoint
    IR::RegOpnd * entryPointRegOpnd = IR::RegOpnd::New(TyMachReg, this->m_func);
#if _M_IX86
    entryPointRegOpnd->SetReg(RegECX);
#elif _M_X64
    entryPointRegOpnd->SetReg(RegRCX);
#endif
    entryPointRegOpnd->m_isCallArg = true;
    IR::Instr* movInstrEntryPointToRegister = IR::Instr::New(Js::OpCode::MOV, entryPointRegOpnd, entryPointOpnd, this->m_func);
    insertBeforeInstr->InsertBefore(movInstrEntryPointToRegister);

    //Generate CheckCFG CALL here
    IR::HelperCallOpnd *cfgCallOpnd = IR::HelperCallOpnd::New(IR::HelperGuardCheckCall, this->m_func);
    IR::Instr* cfgCallInstr = IR::Instr::New(Js::OpCode::CALL, this->m_func);

#if _M_IX86
    //call[__guard_check_icall_fptr]
    cfgCallInstr->SetSrc1(cfgCallOpnd);
#elif _M_X64
    //mov rax, __guard_check_icall_fptr
    IR::RegOpnd *targetOpnd = IR::RegOpnd::New(StackSym::New(TyMachPtr, m_func), RegRAX, TyMachPtr, this->m_func);
    IR::Instr   *movInstr = IR::Instr::New(Js::OpCode::MOV, targetOpnd, cfgCallOpnd, this->m_func);
    insertBeforeInstr->InsertBefore(movInstr);

    //call rax
    cfgCallInstr->SetSrc1(targetOpnd);
#endif


    //CALL cfg(rax)
    insertBeforeInstr->InsertBefore(cfgCallInstr);

    if (preReservedRegionStartAddress != nullptr)
    {
        Assert(callLabelInstr);
#if DBG
        //Always generate CFG check in DBG build to make sure that the address is still valid
        movInstrEntryPointToRegister->InsertBefore(callLabelInstr);
#else
        insertBeforeInstr->InsertBefore(callLabelInstr);
#endif
    }
}
#endif
void
LowererMD::GenerateFastRecyclerAlloc(size_t allocSize, IR::RegOpnd* newObjDst, IR::Instr* insertionPointInstr, IR::LabelInstr* allocHelperLabel, IR::LabelInstr* allocDoneLabel)
{
    IR::Opnd * endAddressOpnd;
    IR::Opnd * freeListOpnd;

    Js::ScriptContext* scriptContext = this->m_func->GetScriptContext();
    Recycler* recycler = scriptContext->GetRecycler();
    void* allocatorAddress;
    uint32 endAddressOffset;
    uint32 freeListOffset;
    size_t alignedSize = HeapInfo::GetAlignedSizeNoCheck(allocSize);

    recycler->GetNormalHeapBlockAllocatorInfoForNativeAllocation(alignedSize, allocatorAddress, endAddressOffset, freeListOffset);

    endAddressOpnd = IR::MemRefOpnd::New((char*)allocatorAddress + endAddressOffset, TyMachPtr, this->m_func, IR::AddrOpndKindDynamicRecyclerAllocatorEndAddressRef);
    freeListOpnd = IR::MemRefOpnd::New((char*)allocatorAddress + freeListOffset, TyMachPtr, this->m_func, IR::AddrOpndKindDynamicRecyclerAllocatorFreeListRef);
    const IR::AutoReuseOpnd autoReuseTempOpnd(freeListOpnd, m_func);
    // MOV newObjDst, allocator->freeObjectList
    Lowerer::InsertMove(newObjDst, freeListOpnd, insertionPointInstr);

    // LEA nextMemBlock, [newObjDst + allocSize]
    IR::RegOpnd * nextMemBlockOpnd = IR::RegOpnd::New(TyMachPtr, this->m_func);
    IR::IndirOpnd* nextMemBlockSrc = IR::IndirOpnd::New(newObjDst, (int32)alignedSize, TyMachPtr, this->m_func);
    IR::Instr * loadNextMemBlockInstr = IR::Instr::New(Js::OpCode::LEA, nextMemBlockOpnd, nextMemBlockSrc, this->m_func);
    insertionPointInstr->InsertBefore(loadNextMemBlockInstr);

    // CMP nextMemBlock, allocator->endAddress
    IR::Instr * checkInstr = IR::Instr::New(Js::OpCode::CMP, this->m_func);
    checkInstr->SetSrc1(nextMemBlockOpnd);
    checkInstr->SetSrc2(endAddressOpnd);
    insertionPointInstr->InsertBefore(checkInstr);
    Legalize(checkInstr);

    // JA $allocHelper
    IR::BranchInstr * branchToAllocHelperInstr = IR::BranchInstr::New(Js::OpCode::JA, allocHelperLabel, this->m_func);
    insertionPointInstr->InsertBefore(branchToAllocHelperInstr);

    // MOV allocator->freeObjectList, nextMemBlock
    Lowerer::InsertMove(freeListOpnd, nextMemBlockOpnd, insertionPointInstr);

    // JMP $allocDone
    IR::BranchInstr * branchToAllocDoneInstr = IR::BranchInstr::New(Js::OpCode::JMP, allocDoneLabel, this->m_func);
    insertionPointInstr->InsertBefore(branchToAllocDoneInstr);
}

void
LowererMD::SaveDoubleToVar(IR::RegOpnd * dstOpnd, IR::RegOpnd *opndFloat, IR::Instr *instrOrig, IR::Instr *instrInsert, bool isHelper)
{
    Assert(opndFloat->GetType() == TyFloat64);

    // Call JSNumber::ToVar to save the float operand to the result of the original (var) instruction
#if !FLOATVAR

    // We should only generate this if sse2 is available
    Assert(AutoSystemInfo::Data.SSE2Available());

    IR::Opnd * symVTableDst;
    IR::Opnd * symDblDst;
    IR::Opnd * symTypeDst;
    IR::Instr * newInstr;
    IR::Instr * numberInitInsertInstr = nullptr;
    if (instrOrig->dstIsTempNumber)
    {
        // Use the original dst to get the temp number sym
        StackSym * tempNumberSym = this->m_lowerer->GetTempNumberSym(instrOrig->GetDst(), instrOrig->dstIsTempNumberTransferred);

        // LEA dst, &tempSym
        IR::SymOpnd * symTempSrc = IR::SymOpnd::New(tempNumberSym, TyMachPtr, this->m_func);
        IR::Instr * loadTempNumberInstr = IR::Instr::New(Js::OpCode::LEA, dstOpnd, symTempSrc, this->m_func);
        instrInsert->InsertBefore(loadTempNumberInstr);

        symVTableDst = IR::SymOpnd::New(tempNumberSym, TyMachPtr, this->m_func);
        symDblDst = IR::SymOpnd::New(tempNumberSym, (uint32)Js::JavascriptNumber::GetValueOffset(), TyFloat64, this->m_func);
        symTypeDst = IR::SymOpnd::New(tempNumberSym, (uint32)Js::JavascriptNumber::GetOffsetOfType(), TyMachPtr, this->m_func);
        if (this->m_lowerer->outerMostLoopLabel == nullptr)
        {
            // If we are not in loop, just insert in place
            numberInitInsertInstr = instrInsert;
        }
        else
        {
            // Otherwise, initialize in the outer most loop top if we haven't initialized it yet.
            numberInitInsertInstr = this->m_lowerer->initializedTempSym->TestAndSet(tempNumberSym->m_id) ?
                nullptr : this->m_lowerer->outerMostLoopLabel;
        }
    }
    else
    {
        this->GenerateNumberAllocation(dstOpnd, instrInsert, isHelper);
        symVTableDst = IR::IndirOpnd::New(dstOpnd, 0, TyMachPtr, this->m_func);
        symDblDst = IR::IndirOpnd::New(dstOpnd, (uint32)Js::JavascriptNumber::GetValueOffset(), TyFloat64, this->m_func);
        symTypeDst = IR::IndirOpnd::New(dstOpnd, (uint32)Js::JavascriptNumber::GetOffsetOfType(), TyMachPtr, this->m_func);
        numberInitInsertInstr = instrInsert;
    }

    if (numberInitInsertInstr)
    {
        // Inline the case where the dst is marked as temp.
        IR::Opnd *jsNumberVTable = m_lowerer->LoadVTableValueOpnd(numberInitInsertInstr, VTableValue::VtableJavascriptNumber);

        // MOV dst->vtable, JavascriptNumber::vtable
        newInstr = IR::Instr::New(Js::OpCode::MOV, symVTableDst, jsNumberVTable, this->m_func);
        numberInitInsertInstr->InsertBefore(newInstr);

        // MOV dst->type, JavascriptNumber_type
        IR::Opnd *typeOpnd = m_lowerer->LoadLibraryValueOpnd(numberInitInsertInstr, LibraryValue::ValueNumberTypeStatic);
        newInstr = IR::Instr::New(Js::OpCode::MOV, symTypeDst, typeOpnd, this->m_func);
        numberInitInsertInstr->InsertBefore(newInstr);
    }

    // MOVSD dst->value, opndFloat   ; copy the float result to the temp JavascriptNumber
    newInstr = IR::Instr::New(Js::OpCode::MOVSD, symDblDst, opndFloat, this->m_func);
    instrInsert->InsertBefore(newInstr);

#else

    // s1 = MOVD opndFloat
    // s1 = XOR s1, FloatTag_Value
    // dst = s1

    IR::RegOpnd *s1 = IR::RegOpnd::New(TyMachReg, this->m_func);
    IR::Instr *movd = IR::Instr::New(Js::OpCode::MOVD, s1, opndFloat, this->m_func);
    IR::Instr *setTag = IR::Instr::New(Js::OpCode::XOR,
                                       s1,
                                       s1,
                                       IR::AddrOpnd::New((Js::Var)Js::FloatTag_Value,
                                                         IR::AddrOpndKindConstantVar,
                                                         this->m_func,
                                                         /* dontEncode = */ true),
                                       this->m_func);
    IR::Instr *movDst = IR::Instr::New(Js::OpCode::MOV, dstOpnd, s1, this->m_func);

    instrInsert->InsertBefore(movd);
    instrInsert->InsertBefore(setTag);
    instrInsert->InsertBefore(movDst);
    LowererMD::Legalize(setTag);
#endif
}

void
LowererMD::EmitLoadFloatFromNumber(IR::Opnd *dst, IR::Opnd *src, IR::Instr *insertInstr)
{
    IR::LabelInstr *labelDone;
    IR::Instr *instr;
    labelDone = EmitLoadFloatCommon(dst, src, insertInstr, insertInstr->HasBailOutInfo());

    if (labelDone == nullptr)
    {
        // We're done
        insertInstr->Remove();
        return;
    }

    // $Done   note: insertAfter
    insertInstr->InsertAfter(labelDone);

    if (!insertInstr->HasBailOutInfo())
    {
        // $Done
        insertInstr->Remove();
        return;
    }
    Assert(!m_func->GetJnFunction()->GetIsAsmjsMode());

    IR::LabelInstr *labelNoBailOut = nullptr;
    IR::SymOpnd *tempSymOpnd = nullptr;

    if (insertInstr->GetBailOutKind() == IR::BailOutPrimitiveButString)
    {
        if (!this->m_func->tempSymDouble)
        {
            this->m_func->tempSymDouble = StackSym::New(TyFloat64, this->m_func);
            this->m_func->StackAllocate(this->m_func->tempSymDouble, MachDouble);
        }

        // LEA r3, tempSymDouble
        IR::RegOpnd *reg3Opnd = IR::RegOpnd::New(TyMachReg, this->m_func);
        tempSymOpnd = IR::SymOpnd::New(this->m_func->tempSymDouble, TyFloat64, this->m_func);
        instr = IR::Instr::New(Js::OpCode::LEA, reg3Opnd, tempSymOpnd, this->m_func);
        insertInstr->InsertBefore(instr);

        // regBoolResult = to_number_fromPrimitive(value, &dst, allowUndef, scriptContext);

        this->m_lowerer->LoadScriptContext(insertInstr);
        IR::IntConstOpnd *allowUndefOpnd;
        if (insertInstr->GetBailOutKind() == IR::BailOutPrimitiveButString)
        {
            allowUndefOpnd = IR::IntConstOpnd::New(true, TyInt32, this->m_func);
        }
        else
        {
            Assert(insertInstr->GetBailOutKind() == IR::BailOutNumberOnly);
            allowUndefOpnd = IR::IntConstOpnd::New(false, TyInt32, this->m_func);
        }
        this->LoadHelperArgument(insertInstr, allowUndefOpnd);
        this->LoadHelperArgument(insertInstr, reg3Opnd);
        this->LoadHelperArgument(insertInstr, src);

        IR::RegOpnd *regBoolResult = IR::RegOpnd::New(TyInt32, this->m_func);
        instr = IR::Instr::New(Js::OpCode::CALL, regBoolResult, IR::HelperCallOpnd::New(IR::HelperOp_ConvNumber_FromPrimitive, this->m_func), this->m_func);
        insertInstr->InsertBefore(instr);

        this->lowererMDArch.LowerCall(instr, 0);

        // TEST regBoolResult, regBoolResult
        instr = IR::Instr::New(Js::OpCode::TEST, this->m_func);
        instr->SetSrc1(regBoolResult);
        instr->SetSrc2(regBoolResult);
        insertInstr->InsertBefore(instr);

        // JNE $noBailOut
        labelNoBailOut = IR::LabelInstr::New(Js::OpCode::Label, this->m_func, true);
        instr = IR::BranchInstr::New(Js::OpCode::JNE, labelNoBailOut, this->m_func);
        insertInstr->InsertBefore(instr);
    }

    // Bailout code
    Assert(insertInstr->m_opcode == Js::OpCode::FromVar);
    insertInstr->UnlinkDst();
    insertInstr->FreeSrc1();
    IR::Instr *bailoutInstr = insertInstr;
    insertInstr = bailoutInstr->m_next;
    this->m_lowerer->GenerateBailOut(bailoutInstr);

    // $noBailOut
    if (labelNoBailOut)
    {
        insertInstr->InsertBefore(labelNoBailOut);

        Assert(dst->IsRegOpnd());

        // MOVSD dst, [pResult].f64
        instr = IR::Instr::New(Js::OpCode::MOVSD, dst, tempSymOpnd, this->m_func);
        insertInstr->InsertBefore(instr);
    }
}

IR::LabelInstr*
LowererMD::EmitLoadFloatCommon(IR::Opnd *dst, IR::Opnd *src, IR::Instr *insertInstr, bool needHelperLabel)
{
    IR::Instr *instr;

    Assert(src->GetType() == TyVar);
    Assert(dst->IsFloat());
    bool isFloatConst = false;
    IR::RegOpnd *regFloatOpnd = nullptr;

    if (src->IsRegOpnd() && src->AsRegOpnd()->m_sym->m_isFltConst)
    {
        IR::RegOpnd *regOpnd = src->AsRegOpnd();
        Assert(regOpnd->m_sym->m_isSingleDef);
        Js::Var value = regOpnd->m_sym->GetFloatConstValueAsVar_PostGlobOpt();
#if FLOATVAR
        double *pDouble = NativeCodeDataNew(this->m_func->GetNativeCodeDataAllocator(), double);
        AnalysisAssert(pDouble);
        *pDouble = Js::JavascriptNumber::GetValue(value);
        IR::MemRefOpnd *memRef = IR::MemRefOpnd::New((BYTE*)pDouble, TyFloat64, this->m_func, IR::AddrOpndKindDynamicDoubleRef);
#else
        IR::MemRefOpnd *memRef = IR::MemRefOpnd::New((BYTE*)value + Js::JavascriptNumber::GetValueOffset(), TyFloat64, this->m_func,
            IR::AddrOpndKindDynamicDoubleRef);
#endif
        regFloatOpnd = IR::RegOpnd::New(TyFloat64, this->m_func);
        instr = IR::Instr::New(Js::OpCode::MOVSD, regFloatOpnd, memRef, this->m_func);
        insertInstr->InsertBefore(instr);
        Legalize(instr);
        isFloatConst = true;
    }
    // Src is constant?
    if (src->IsImmediateOpnd() || src->IsFloatConstOpnd())
    {
        regFloatOpnd = IR::RegOpnd::New(TyFloat64, this->m_func);
        m_lowerer->LoadFloatFromNonReg(src, regFloatOpnd, insertInstr);
        isFloatConst = true;
    }
    if (isFloatConst)
    {
        if (dst->GetType() == TyFloat32)
        {
            // CVTSD2SS regOpnd32.f32, regOpnd.f64    -- Convert regOpnd from f64 to f32
            IR::RegOpnd *regOpnd32 = regFloatOpnd->UseWithNewType(TyFloat32, this->m_func)->AsRegOpnd();
            instr = IR::Instr::New(Js::OpCode::CVTSD2SS, regOpnd32, regFloatOpnd, this->m_func);
            insertInstr->InsertBefore(instr);

            // MOVSS dst, regOpnd32
            instr = IR::Instr::New(Js::OpCode::MOVSS, dst, regOpnd32, this->m_func);
            insertInstr->InsertBefore(instr);
        }
        else
        {
            // MOVSD dst, regOpnd
            instr = IR::Instr::New(Js::OpCode::MOVSD, dst, regFloatOpnd, this->m_func);
            insertInstr->InsertBefore(instr);
        }
        return nullptr;
    }
    Assert(src->IsRegOpnd());

    IR::LabelInstr *labelStore = IR::LabelInstr::New(Js::OpCode::Label, this->m_func);
    IR::LabelInstr *labelHelper;
    IR::LabelInstr *labelDone = IR::LabelInstr::New(Js::OpCode::Label, this->m_func);
    if (needHelperLabel)
    {
        labelHelper = IR::LabelInstr::New(Js::OpCode::Label, this->m_func, true);
    }
    else
    {
        labelHelper = labelDone;
    }

    bool const isFloat32 = dst->GetType() == TyFloat32;
    IR::RegOpnd *reg2 = ((isFloat32 || !dst->IsRegOpnd()) ? IR::RegOpnd::New(TyMachDouble, this->m_func) : dst->AsRegOpnd());

    // Load the float value in reg2
    this->lowererMDArch.LoadCheckedFloat(src->AsRegOpnd(), reg2, labelStore, labelHelper, insertInstr, needHelperLabel);

    // $Store
    insertInstr->InsertBefore(labelStore);
    if (isFloat32)
    {
        IR::RegOpnd *reg2_32 = reg2->UseWithNewType(TyFloat32, this->m_func)->AsRegOpnd();
        // CVTSD2SS r2_32.f32, r2.f64    -- Convert regOpnd from f64 to f32
        instr = IR::Instr::New(Js::OpCode::CVTSD2SS, reg2_32, reg2, this->m_func);
        insertInstr->InsertBefore(instr);

        // MOVSS dst, r2_32
        instr = IR::Instr::New(Js::OpCode::MOVSS, dst, reg2_32, this->m_func);
        insertInstr->InsertBefore(instr);
    }
    else if (reg2 != dst)
    {
        // MOVSD dst, r2
        instr = IR::Instr::New(Js::OpCode::MOVSD, dst, reg2, this->m_func);
        insertInstr->InsertBefore(instr);
    }

    // JMP $Done
    instr = IR::BranchInstr::New(Js::OpCode::JMP, labelDone, this->m_func);
    insertInstr->InsertBefore(instr);

    if (needHelperLabel)
    {
        // $Helper
        insertInstr->InsertBefore(labelHelper);
    }

    return labelDone;
}

IR::RegOpnd *
LowererMD::EmitLoadFloat(IR::Opnd *dst, IR::Opnd *src, IR::Instr *insertInstr)
{
    IR::LabelInstr *labelDone;
    IR::Instr *instr;
    labelDone = EmitLoadFloatCommon(dst, src, insertInstr, true);

    if (labelDone == nullptr)
    {
        // We're done
        return nullptr;
    }

    IR::Opnd *memAddress = dst;

    if (dst->IsRegOpnd())
    {
        // Create an f64 stack location to store the result of the helper.
        IR::SymOpnd *symOpnd = IR::SymOpnd::New(StackSym::New(dst->GetType(), this->m_func), dst->GetType(), this->m_func);
        this->m_func->StackAllocate(symOpnd->m_sym->AsStackSym(), sizeof(double));
        memAddress = symOpnd;
    }

    // LEA r3, dst
    IR::RegOpnd *reg3Opnd = IR::RegOpnd::New(TyMachReg, this->m_func);
    instr = IR::Instr::New(Js::OpCode::LEA, reg3Opnd, memAddress, this->m_func);
    insertInstr->InsertBefore(instr);

    // to_number_full(value, &dst, scriptContext);

    // Create dummy binary op to convert into helper
    instr = IR::Instr::New(Js::OpCode::Add_A, this->m_func);
    instr->SetSrc1(src);
    instr->SetSrc2(reg3Opnd);
    insertInstr->InsertBefore(instr);

    IR::JnHelperMethod helper;
    if (dst->GetType() == TyFloat32)
    {
        helper = IR::HelperOp_ConvFloat_Helper;
    }
    else
    {
        helper = IR::HelperOp_ConvNumber_Helper;
    }
    this->m_lowerer->LowerBinaryHelperMem(instr, helper);

    if (dst->IsRegOpnd())
    {
        if (dst->GetType() == TyFloat32)
        {
            // MOVSS dst, r32
            instr = IR::Instr::New(Js::OpCode::MOVSS, dst, memAddress, this->m_func);
            insertInstr->InsertBefore(instr);
        }
        else
        {
            // MOVSD dst, [pResult].f64
            instr = IR::Instr::New(Js::OpCode::MOVSD, dst, memAddress, this->m_func);
            insertInstr->InsertBefore(instr);
        }
    }
    // $Done
    insertInstr->InsertBefore(labelDone);

    return nullptr;
}

void
LowererMD::LowerInt4NegWithBailOut(
    IR::Instr *const instr,
    const IR::BailOutKind bailOutKind,
    IR::LabelInstr *const bailOutLabel,
    IR::LabelInstr *const skipBailOutLabel)
{
    Assert(instr);
    Assert(instr->m_opcode == Js::OpCode::Neg_I4);
    Assert(!instr->HasBailOutInfo());
    Assert(bailOutKind & IR::BailOutOnResultConditions || bailOutKind == IR::BailOutOnFailedHoistedLoopCountBasedBoundCheck);
    Assert(bailOutLabel);
    Assert(instr->m_next == bailOutLabel);
    Assert(skipBailOutLabel);

    instr->ReplaceDst(instr->GetDst()->UseWithNewType(TyInt32, instr->m_func));
    instr->ReplaceSrc1(instr->GetSrc1()->UseWithNewType(TyInt32, instr->m_func));

    // Lower the instruction
    instr->m_opcode = Js::OpCode::NEG;
    Legalize(instr);

    if(bailOutKind & IR::BailOutOnOverflow || bailOutKind == IR::BailOutOnFailedHoistedLoopCountBasedBoundCheck)
    {
        bailOutLabel->InsertBefore(IR::BranchInstr::New(Js::OpCode::JO, bailOutLabel, instr->m_func));
    }

    if(bailOutKind & IR::BailOutOnNegativeZero)
    {
        bailOutLabel->InsertBefore(IR::BranchInstr::New(Js::OpCode::JEQ, bailOutLabel, instr->m_func));
    }

    // Skip bailout
    bailOutLabel->InsertBefore(IR::BranchInstr::New(LowererMD::MDUncondBranchOpcode, skipBailOutLabel, instr->m_func));
}

void
LowererMD::LowerInt4AddWithBailOut(
    IR::Instr *const instr,
    const IR::BailOutKind bailOutKind,
    IR::LabelInstr *const bailOutLabel,
    IR::LabelInstr *const skipBailOutLabel)
{
    Assert(instr);
    Assert(instr->m_opcode == Js::OpCode::Add_I4);
    Assert(!instr->HasBailOutInfo());
    Assert(
        (bailOutKind & IR::BailOutOnResultConditions) == IR::BailOutOnOverflow ||
        bailOutKind == IR::BailOutOnFailedHoistedLoopCountBasedBoundCheck);
    Assert(bailOutLabel);
    Assert(instr->m_next == bailOutLabel);
    Assert(skipBailOutLabel);

    instr->ReplaceDst(instr->GetDst()->UseWithNewType(TyInt32, instr->m_func));
    instr->ReplaceSrc1(instr->GetSrc1()->UseWithNewType(TyInt32, instr->m_func));
    instr->ReplaceSrc2(instr->GetSrc2()->UseWithNewType(TyInt32, instr->m_func));

    // Restore sources overwritten by the instruction in the bailout path
    const auto dst = instr->GetDst(), src1 = instr->GetSrc1(), src2 = instr->GetSrc2();
    Assert(dst->IsRegOpnd());
    const bool dstEquSrc1 = dst->IsEqual(src1), dstEquSrc2 = dst->IsEqual(src2);
    if(dstEquSrc1 & dstEquSrc2)
    {
        // We have:
        //     s1 += s1
        // Which is equivalent to:
        //     s1 <<= 1
        //
        // These overflow a signed 32-bit integer when for the initial s1:
        //     s1 > 0 && (s1 & 0x40000000)     - result is negative after overflow
        //     s1 < 0 && !(s1 & 0x40000000)    - result is nonnegative after overflow
        //
        // To restore s1 to its value before the operation, we first do an arithmetic right-shift by one bit to undo the
        // left-shift and preserve the sign of the result after overflow. Since the result after overflow always has the
        // opposite sign from the operands (hence the overflow), we just need to invert the sign of the result. The following
        // restores s1 to its value before the instruction:
        //     s1 = (s1 >> 1) ^ 0x80000000
        //
        // Generate:
        //     sar  s1, 1
        //     xor  s1, 0x80000000
        const auto startBailOutInstr = bailOutLabel->m_next;
        Assert(startBailOutInstr);
        startBailOutInstr->InsertBefore(
                IR::Instr::New(
                    Js::OpCode::SAR,
                    dst,
                    dst,
                    IR::IntConstOpnd::New(1, TyInt8, instr->m_func),
                    instr->m_func)
            );
        startBailOutInstr->InsertBefore(
                IR::Instr::New(
                    Js::OpCode::XOR,
                    dst,
                    dst,
                    IR::IntConstOpnd::New(INT32_MIN, TyInt32, instr->m_func, true /* dontEncode */),
                    instr->m_func)
            );
    }
    else if(dstEquSrc1 | dstEquSrc2)
    {
        // We have:
        //     s1 += s2
        //   Or:
        //     s1 = s2 + s1
        //
        // The following restores s1 to its value before the instruction:
        //     s1 -= s2
        //
        // Generate:
        //     sub  s1, s2
        if(dstEquSrc1)
        {
            Assert(src2->IsRegOpnd() || src2->IsIntConstOpnd());
        }
        else
        {
            Assert(src1->IsRegOpnd() || src1->IsIntConstOpnd());
        }
        bailOutLabel->InsertAfter(IR::Instr::New(Js::OpCode::SUB, dst, dst, dstEquSrc1 ? src2 : src1, instr->m_func));
    }

    // Lower the instruction
    ChangeToAdd(instr, true /* needFlags */);
    Legalize(instr);

    // Skip bailout on no overflow
    bailOutLabel->InsertBefore(IR::BranchInstr::New(Js::OpCode::JNO, skipBailOutLabel, instr->m_func));

    // Fall through to bailOutLabel
}

void
LowererMD::LowerInt4SubWithBailOut(
    IR::Instr *const instr,
    const IR::BailOutKind bailOutKind,
    IR::LabelInstr *const bailOutLabel,
    IR::LabelInstr *const skipBailOutLabel)
{
    Assert(instr);
    Assert(instr->m_opcode == Js::OpCode::Sub_I4);
    Assert(!instr->HasBailOutInfo());
    Assert(
        (bailOutKind & IR::BailOutOnResultConditions) == IR::BailOutOnOverflow ||
        bailOutKind == IR::BailOutOnFailedHoistedLoopCountBasedBoundCheck);
    Assert(bailOutLabel);
    Assert(instr->m_next == bailOutLabel);
    Assert(skipBailOutLabel);

    instr->ReplaceDst(instr->GetDst()->UseWithNewType(TyInt32, instr->m_func));
    instr->ReplaceSrc1(instr->GetSrc1()->UseWithNewType(TyInt32, instr->m_func));
    instr->ReplaceSrc2(instr->GetSrc2()->UseWithNewType(TyInt32, instr->m_func));

    // Restore sources overwritten by the instruction in the bailout path
    const auto dst = instr->GetDst(), src1 = instr->GetSrc1(), src2 = instr->GetSrc2();
    Assert(dst->IsRegOpnd());
    const bool dstEquSrc1 = dst->IsEqual(src1), dstEquSrc2 = dst->IsEqual(src2);
    if(dstEquSrc1 ^ dstEquSrc2)
    {
        // We have:
        //     s1 -= s2
        //   Or:
        //     s1 = s2 - s1
        //
        // The following restores s1 to its value before the instruction:
        //     s1 += s2
        //   Or:
        //     s1 = s2 - s1
        //
        // Generate:
        //     neg  s1      - only for second case
        //     add  s1, s2
        if(dstEquSrc1)
        {
            Assert(src2->IsRegOpnd() || src2->IsIntConstOpnd());
        }
        else
        {
            Assert(src1->IsRegOpnd() || src1->IsIntConstOpnd());
        }
        const auto startBailOutInstr = bailOutLabel->m_next;
        Assert(startBailOutInstr);
        if(dstEquSrc2)
        {
            startBailOutInstr->InsertBefore(IR::Instr::New(Js::OpCode::NEG, dst, dst, instr->m_func));
        }
        startBailOutInstr->InsertBefore(IR::Instr::New(Js::OpCode::ADD, dst, dst, dstEquSrc1 ? src2 : src1, instr->m_func));
    }

    // Lower the instruction
    ChangeToSub(instr, true /* needFlags */);
    Legalize(instr);

    // Skip bailout on no overflow
    bailOutLabel->InsertBefore(IR::BranchInstr::New(Js::OpCode::JNO, skipBailOutLabel, instr->m_func));

    // Fall through to bailOutLabel
}


bool
LowererMD::GenerateSimplifiedInt4Mul(
    IR::Instr *const mulInstr,
    const IR::BailOutKind bailOutKind,
    IR::LabelInstr *const bailOutLabel)
{
    if (AutoSystemInfo::Data.IsAtomPlatform())
    {
        // On Atom, always optimize unless phase is off
        if (PHASE_OFF(Js::AtomPhase, mulInstr->m_func->GetTopFunc()) ||
            PHASE_OFF(Js::MulStrengthReductionPhase, mulInstr->m_func->GetTopFunc()))
            return false;
    }
    else
    {

        // On other platforms, don't optimize unless phase is forced
        if (!PHASE_FORCE(Js::AtomPhase, mulInstr->m_func->GetTopFunc()) &&
            !PHASE_FORCE(Js::MulStrengthReductionPhase, mulInstr->m_func->GetTopFunc()))
            return false;
    }

    Assert(mulInstr);
    Assert(mulInstr->m_opcode == Js::OpCode::Mul_I4);

    IR::Instr *instr = mulInstr, *nextInstr;
    const auto dst = instr->GetDst(), src1 = instr->GetSrc1(), src2 = instr->GetSrc2();

    if (!src1->IsIntConstOpnd() && !src2->IsIntConstOpnd())
        return false;

    // if two const operands, GlobOpt would have folded the computation
    Assert(!(src1->IsIntConstOpnd() && src2->IsIntConstOpnd()));
    Assert(dst->IsRegOpnd());

    const auto constSrc = src1->IsIntConstOpnd() ? src1 : src2;
    const auto nonConstSrc = src1->IsIntConstOpnd() ? src2 : src1;
    const auto constSrcValue = constSrc->AsIntConstOpnd()->AsInt32();
    auto nonConstSrcCopy = nonConstSrc;
    Assert(nonConstSrc->IsRegOpnd());

    bool doOVF = bailOutKind & IR::BailOutOnMulOverflow || bailOutKind == IR::BailOutOnFailedHoistedLoopCountBasedBoundCheck;

    // don't simplify mul by large numbers with OF check
    if (doOVF && (constSrcValue > 3 || constSrcValue < -3))
        return false;

    switch(constSrcValue)
    {
    case -3:
    case 3:
        // if dst = src, we need to have a copy of the src for the ADD/SUB
        if (dst->IsEqual(nonConstSrc))
        {
            nonConstSrcCopy = IR::RegOpnd::New(nonConstSrc->GetType(), instr->m_func);
            // MOV
            Lowerer::InsertMove(nonConstSrcCopy, nonConstSrc, instr);
        }
        instr->UnlinkSrc1();
        instr->UnlinkSrc2();
        // SHL
        instr->m_opcode = Js::OpCode::SHL;
        instr->SetSrc1(nonConstSrc);
        instr->SetSrc2(IR::IntConstOpnd::New((IntConstType) 1, TyInt32, instr->m_func));
        constSrc->Free(instr->m_func);
        Legalize(instr);
        // JO
        if (doOVF)
        {
            nextInstr = IR::BranchInstr::New(Js::OpCode::JO, bailOutLabel, instr->m_func);
            instr->InsertAfter(nextInstr);
            instr = nextInstr;
        }
        // ADD
        nextInstr = IR::Instr::New(Js::OpCode::ADD, dst, dst, nonConstSrcCopy, instr->m_func);
        instr->InsertAfter(nextInstr);
        instr = nextInstr;
        Legalize(instr);
        if (constSrcValue == -3)
        {
            // JO
            if (doOVF)
            {
                nextInstr = IR::BranchInstr::New(Js::OpCode::JO, bailOutLabel, instr->m_func);
                instr->InsertAfter(nextInstr);
                instr = nextInstr;
            }
            // NEG
            nextInstr = IR::Instr::New(Js::OpCode::NEG, dst, dst, instr->m_func);
            instr->InsertAfter(nextInstr);
            instr = nextInstr;
            Legalize(instr);
        }
        // last JO inserted by caller
        return true;
    case -2:
    case 2:
        instr->UnlinkSrc1();
        instr->UnlinkSrc2();
        // SHL
        instr->m_opcode = Js::OpCode::SHL;
        instr->SetSrc1(nonConstSrc);
        instr->SetSrc2(IR::IntConstOpnd::New((IntConstType) 1, TyInt32, instr->m_func));
        constSrc->Free(instr->m_func);
        Legalize(instr);

        if (constSrcValue == -2)
        {
            // JO
            if (doOVF)
            {
                nextInstr = IR::BranchInstr::New(Js::OpCode::JO, bailOutLabel, instr->m_func);
                instr->InsertAfter(nextInstr);
                instr = nextInstr;
            }
            // NEG
            nextInstr = IR::Instr::New(Js::OpCode::NEG, dst, dst, instr->m_func);
            instr->InsertAfter(nextInstr);
            instr = nextInstr;
            Legalize(instr);
        }
        // last JO inserted by caller
        return true;
    case -1:
        instr->UnlinkSrc1();
        instr->UnlinkSrc2();
        // NEG
        instr->m_opcode = Js::OpCode::NEG;
        instr->SetSrc1(nonConstSrc);
        constSrc->Free(instr->m_func);
        Legalize(instr);
        // JO inserted by caller
        return true;
    case 0:
        instr->FreeSrc1();
        instr->FreeSrc2();
        // MOV
        instr->m_opcode = Js::OpCode::MOV;
        instr->SetSrc1(IR::IntConstOpnd::New((IntConstType) 0, TyInt32, instr->m_func));
        Legalize(instr);
        // JO inserted by caller are removed in later phases
        return true;
    case 1:
        instr->UnlinkSrc1();
        instr->UnlinkSrc2();
        // MOV
        instr->m_opcode = Js::OpCode::MOV;
        instr->SetSrc1(nonConstSrc);
        constSrc->Free(instr->m_func);
        Legalize(instr);
        // JO inserted by caller are removed in later phases
        return true;
    default:
        // large numbers with no OF check
        Assert(!doOVF);
        // 2^i
        // -2^i
        if (Math::IsPow2(constSrcValue) || Math::IsPow2(-constSrcValue))
        {
            uint32 shamt = constSrcValue > 0 ? Math::Log2(constSrcValue) : Math::Log2(-constSrcValue);
            instr->UnlinkSrc1();
            instr->UnlinkSrc2();
            // SHL
            instr->m_opcode = Js::OpCode::SHL;
            instr->SetSrc1(nonConstSrc);
            instr->SetSrc2(IR::IntConstOpnd::New((IntConstType) shamt, TyInt32, instr->m_func));
            constSrc->Free(instr->m_func);
            Legalize(instr);
            if (constSrcValue < 0)
            {
                // NEG
                nextInstr = IR::Instr::New(Js::OpCode::NEG, dst, dst, instr->m_func);
                instr->InsertAfter(nextInstr);
                Legalize(instr);
            }
            return true;
        }

        // 2^i + 1
        // 2^i - 1
        if (Math::IsPow2(constSrcValue - 1) || Math::IsPow2(constSrcValue + 1))
        {
            bool plusOne = Math::IsPow2(constSrcValue - 1);
            uint32 shamt = plusOne ? Math::Log2(constSrcValue - 1) : Math::Log2(constSrcValue + 1);

            if (dst->IsEqual(nonConstSrc))
            {
                nonConstSrcCopy = IR::RegOpnd::New(nonConstSrc->GetType(), instr->m_func);
                // MOV
                Lowerer::InsertMove(nonConstSrcCopy, nonConstSrc, instr);
            }
            instr->UnlinkSrc1();
            instr->UnlinkSrc2();
            // SHL
            instr->m_opcode = Js::OpCode::SHL;
            instr->SetSrc1(nonConstSrc);
            instr->SetSrc2(IR::IntConstOpnd::New((IntConstType) shamt, TyInt32, instr->m_func));
            constSrc->Free(instr->m_func);
            Legalize(instr);
            // ADD/SUB
            nextInstr = IR::Instr::New(plusOne ? Js::OpCode::ADD : Js::OpCode::SUB, dst, dst, nonConstSrcCopy, instr->m_func);
            instr->InsertAfter(nextInstr);
            instr = nextInstr;
            Legalize(instr);
            return true;
        }
        return false;
    }
}

void
LowererMD::LowerInt4MulWithBailOut(
    IR::Instr *const instr,
    const IR::BailOutKind bailOutKind,
    IR::LabelInstr *const bailOutLabel,
    IR::LabelInstr *const skipBailOutLabel)
{
    Assert(instr);
    Assert(instr->m_opcode == Js::OpCode::Mul_I4);
    Assert(!instr->HasBailOutInfo());
    Assert(bailOutKind & IR::BailOutOnResultConditions || bailOutKind == IR::BailOutOnFailedHoistedLoopCountBasedBoundCheck);
    Assert(bailOutLabel);
    Assert(instr->m_next == bailOutLabel);
    Assert(skipBailOutLabel);

    instr->ReplaceDst(instr->GetDst()->UseWithNewType(TyInt32, instr->m_func));
    instr->ReplaceSrc1(instr->GetSrc1()->UseWithNewType(TyInt32, instr->m_func));
    instr->ReplaceSrc2(instr->GetSrc2()->UseWithNewType(TyInt32, instr->m_func));

    IR::LabelInstr *checkForNegativeZeroLabel = nullptr;
    if(bailOutKind & IR::BailOutOnNegativeZero)
    {
        // We have:
        //     s3 = s1 * s2
        //
        // If the result is zero, we need to check and only bail out if it would be -0. The following determines this:
        //     bailOut = (s1 < 0 || s2 < 0) (either s1 or s2 has to be zero for the result to be zero, so we don't emit zero checks)
        //
        // Note, however, that if in future we decide to ignore mul overflow in some cases, and overflow occurs with one of the operands as negative,
        // this can lead to bailout. Will handle that case if ever we decide to ignore mul overflow.
        //
        // Generate:
        //   $checkForNegativeZeroLabel:
        //     test s1, s1
        //     js   $bailOutLabel
        //     test s2, s2
        //     jns  $skipBailOutLabel
        //   (fall through to bail out)

        const auto dst = instr->GetDst(), src1 = instr->GetSrc1(), src2 = instr->GetSrc2();
        Assert(dst->IsRegOpnd());
        Assert(!src1->IsEqual(src2)); // cannot result in -0 if both operands are the same; GlobOpt should have figured that out

        checkForNegativeZeroLabel = IR::LabelInstr::New(Js::OpCode::Label, instr->m_func, true);
        bailOutLabel->InsertBefore(checkForNegativeZeroLabel);

        if(src1->IsIntConstOpnd() || src2->IsIntConstOpnd())
        {
            Assert(!(src1->IsIntConstOpnd() && src2->IsIntConstOpnd())); // if this results in -0, GlobOpt should have avoided type specialization

            const auto constSrc = src1->IsIntConstOpnd() ? src1 : src2;
            const auto nonConstSrc = src1->IsIntConstOpnd() ? src2 : src1;
            Assert(nonConstSrc->IsRegOpnd());

            const auto newInstr = IR::Instr::New(Js::OpCode::TEST, instr->m_func);
            newInstr->SetSrc1(nonConstSrc);
            newInstr->SetSrc2(nonConstSrc);
            bailOutLabel->InsertBefore(newInstr);

            const auto constSrcValue = constSrc->AsIntConstOpnd()->GetValue();
            if(constSrcValue == 0)
            {
                bailOutLabel->InsertBefore(IR::BranchInstr::New(Js::OpCode::JNSB, skipBailOutLabel, instr->m_func));
            }
            else
            {
                Assert(constSrcValue < 0); // cannot result in -0 if one operand is positive; GlobOpt should have figured that out
                bailOutLabel->InsertBefore(IR::BranchInstr::New(Js::OpCode::JNE, skipBailOutLabel, instr->m_func));
            }
        }
        else
        {
            auto newInstr = IR::Instr::New(Js::OpCode::TEST, instr->m_func);
            newInstr->SetSrc1(src1);
            newInstr->SetSrc2(src1);
            bailOutLabel->InsertBefore(newInstr);

            bailOutLabel->InsertBefore(IR::BranchInstr::New(Js::OpCode::JSB, bailOutLabel, instr->m_func));

            newInstr = IR::Instr::New(Js::OpCode::TEST, instr->m_func);
            newInstr->SetSrc1(src2);
            newInstr->SetSrc2(src2);
            bailOutLabel->InsertBefore(newInstr);

            bailOutLabel->InsertBefore(IR::BranchInstr::New(Js::OpCode::JNSB, skipBailOutLabel, instr->m_func));
        }

        // Fall through to bailOutLabel
    }

    const bool needsOverflowCheck =
        bailOutKind & IR::BailOutOnMulOverflow || bailOutKind == IR::BailOutOnFailedHoistedLoopCountBasedBoundCheck;
    AssertMsg(!instr->ShouldCheckForNon32BitOverflow() || (needsOverflowCheck && instr->ShouldCheckForNon32BitOverflow()), "Non 32-bit overflow check required without bailout info");
    bool simplifiedMul = LowererMD::GenerateSimplifiedInt4Mul(instr, bailOutKind, bailOutLabel);
    // Lower the instruction
    if (!simplifiedMul)
    {
        LowererMD::ChangeToMul(instr, needsOverflowCheck);
    }

    const auto insertBeforeInstr = checkForNegativeZeroLabel ? checkForNegativeZeroLabel : bailOutLabel;

    if(needsOverflowCheck)
    {
        // do we care about int32 or non-int32 overflow ?
        if (!simplifiedMul && !instr->ShouldCheckFor32BitOverflow() && instr->ShouldCheckForNon32BitOverflow())
            LowererMD::EmitNon32BitOvfCheck(instr, insertBeforeInstr, bailOutLabel);
        else
            insertBeforeInstr->InsertBefore(IR::BranchInstr::New(Js::OpCode::JO, bailOutLabel, instr->m_func));
    }

    if(bailOutKind & IR::BailOutOnNegativeZero)
    {
        // On zero, branch to determine whether the result would be -0

        Assert(checkForNegativeZeroLabel);

        const auto newInstr = IR::Instr::New(Js::OpCode::TEST, instr->m_func);
        const auto dst = instr->GetDst();
        newInstr->SetSrc1(dst);
        newInstr->SetSrc2(dst);
        insertBeforeInstr->InsertBefore(newInstr);

        insertBeforeInstr->InsertBefore(IR::BranchInstr::New(Js::OpCode::JEQ, checkForNegativeZeroLabel, instr->m_func));
    }

    // Skip bailout
    insertBeforeInstr->InsertBefore(IR::BranchInstr::New(LowererMD::MDUncondBranchOpcode, skipBailOutLabel, instr->m_func));
}

void
LowererMD::LowerInt4RemWithBailOut(
    IR::Instr *const instr,
    const IR::BailOutKind bailOutKind,
    IR::LabelInstr *const bailOutLabel,
    IR::LabelInstr *const skipBailOutLabel) const
{


    Assert(instr);
    Assert(instr->m_opcode == Js::OpCode::Rem_I4);
    Assert(!instr->HasBailOutInfo());
    Assert(bailOutKind & IR::BailOutOnNegativeZero);
    Assert(bailOutLabel);
    Assert(instr->m_next == bailOutLabel);
    Assert(skipBailOutLabel);

    instr->ReplaceDst(instr->GetDst()->UseWithNewType(TyInt32, instr->m_func));
    instr->ReplaceSrc1(instr->GetSrc1()->UseWithNewType(TyInt32, instr->m_func));
    instr->ReplaceSrc2(instr->GetSrc2()->UseWithNewType(TyInt32, instr->m_func));

    bool fastPath = m_lowerer->GenerateSimplifiedInt4Rem(instr, skipBailOutLabel);

    // We have:
    //     s3 = s1 % s2
    //
    // If the result is zero, we need to check and only bail out if it would be -0. The following determines this:
    //     bailOut = (s3 == 0 && s1 < 0)
    //
    // Generate:
    //   $checkForNegativeZeroLabel:
    //     test s3, s3
    //     jne  $skipBailOutLabel
    //     test s1, s1
    //     jns  $skipBailOutLabel
    //   (fall through to bail out)

    IR::Opnd *dst = instr->GetDst(), *src1 = instr->GetSrc1();
    Assert(dst->IsRegOpnd());

    IR::Instr * newInstr = IR::Instr::New(Js::OpCode::TEST, instr->m_func);
    newInstr->SetSrc1(dst);
    newInstr->SetSrc2(dst);
    bailOutLabel->InsertBefore(newInstr);

    bailOutLabel->InsertBefore(IR::BranchInstr::New(Js::OpCode::JNE, skipBailOutLabel, instr->m_func));

    // Fast path already checks if s1 >= 0
    if (!fastPath)
    {
        newInstr = IR::Instr::New(Js::OpCode::TEST, instr->m_func);
        newInstr->SetSrc1(src1);
        newInstr->SetSrc2(src1);
        bailOutLabel->InsertBefore(newInstr);

        bailOutLabel->InsertBefore(IR::BranchInstr::New(Js::OpCode::JNSB, skipBailOutLabel, instr->m_func));
    }
    // Fall through to bailOutLabel


    // Lower the instruction
    LowererMDArch::EmitInt4Instr(instr);

}

IR::Instr *
LowererMD::LoadFloatZero(IR::Opnd * opndDst, IR::Instr * instrInsert)
{
    return IR::Instr::New(Js::OpCode::MOVSD_ZERO, opndDst, instrInsert->m_func);
}

IR::Instr *
LowererMD::LoadFloatValue(IR::Opnd * opndDst, double value, IR::Instr * instrInsert)
{
    // Floating point zero is a common value to load.  Let's use a single memory location instead of allocating new memory for each.
    const bool isFloatZero = value == 0.0 && !Js::JavascriptNumber::IsNegZero(value); // (-0.0 == 0.0) yields true
    IR::Instr * instr;
    if (isFloatZero)
    {
        instr = LoadFloatZero(opndDst, instrInsert);
    }
    else if (opndDst->IsFloat64())
    {
        double *pValue = NativeCodeDataNew(instrInsert->m_func->GetNativeCodeDataAllocator(), double, value);
        IR::Opnd * opnd = IR::MemRefOpnd::New((void*)pValue, TyMachDouble, instrInsert->m_func, IR::AddrOpndKindDynamicDoubleRef);
        instr = IR::Instr::New(LowererMDArch::GetAssignOp(TyMachDouble), opndDst, opnd, instrInsert->m_func);
    }
    else
    {
        Assert(opndDst->IsFloat32());
        float * pValue = NativeCodeDataNew(instrInsert->m_func->GetNativeCodeDataAllocator(), float, (float)value);
        IR::Opnd * opnd = IR::MemRefOpnd::New((void *)pValue, TyFloat32, instrInsert->m_func, IR::AddrOpndKindDynamicFloatRef);
        instr = IR::Instr::New(LowererMDArch::GetAssignOp(TyFloat32), opndDst, opnd, instrInsert->m_func);
    }
    instrInsert->InsertBefore(instr);
    Legalize(instr);
    return instr;
}

IR::Instr *
LowererMD::EnsureAdjacentArgs(IR::Instr * instrArg)
{
    // Ensure that the arg instructions for a given call site are adjacent.
    // This isn't normally desirable for CQ, but it's required by, for instance, the cloner,
    // which must clone a complete call sequence.
    IR::Opnd * opnd = instrArg->GetSrc2();
    IR::Instr * instrNextArg;
    StackSym * sym;

    AssertMsg(opnd, "opnd");
    while (opnd->IsSymOpnd())
    {
        sym = opnd->AsSymOpnd()->m_sym->AsStackSym();
        IR::Instr * instrNextArg = sym->m_instrDef;
        Assert(instrNextArg);
        instrNextArg->SinkInstrBefore(instrArg);
        instrArg = instrNextArg;
        opnd = instrArg->GetSrc2();
    }
    sym = opnd->AsRegOpnd()->m_sym;
    instrNextArg = sym->m_instrDef;
    Assert(instrNextArg && instrNextArg->m_opcode == Js::OpCode::StartCall);

    // The StartCall can be trivially moved down.
    if (instrNextArg->m_next != instrArg)
    {
        instrNextArg->UnlinkStartCallFromBailOutInfo(instrArg);
        instrNextArg->Unlink();
        instrArg->InsertBefore(instrNextArg);
    }

    return instrNextArg->m_prev;
}

#if INT32VAR
//
// Convert an int32 to Var representation.
//
void LowererMD::GenerateInt32ToVarConversion( IR::Opnd * opndSrc, IR::Instr * insertInstr )
{
    AssertMsg(TySize[opndSrc->GetType()] == MachPtr, "For this to work it should be a 64-bit register");

    IR::Instr* instr = IR::Instr::New(Js::OpCode::BTS, opndSrc, opndSrc, IR::IntConstOpnd::New(Js::VarTag_Shift, TyInt8, this->m_func), this->m_func);
    insertInstr->InsertBefore(instr);
}

//
// jump to $labelHelper, based on the result of CMP
//
void LowererMD::GenerateSmIntTest(IR::Opnd *opndSrc, IR::Instr *insertInstr, IR::LabelInstr *labelHelper, IR::Instr **instrFirst /* = nullptr */, bool fContinueLabel /*= false*/)
{
    AssertMsg(opndSrc->GetSize() == MachPtr, "64-bit register required");

    IR::Opnd  * opndReg = IR::RegOpnd::New(TyMachReg, this->m_func);

#ifdef SHIFTLOAD
    // s1 = SHLD src1, 16 - Shift top 16-bits of src1 to s1
    IR::Instr* instr = IR::Instr::New(Js::OpCode::SHLD, opndReg, opndSrc, IR::IntConstOpnd::New(16, TyInt8, this->m_func), this->m_func);
    insertInstr->InsertBefore(instr);

    if (instrFirst)
    {
        *instrFirst = instr;
    }

    // CMP s1.i16, AtomTag.i16
    IR::Opnd *opndReg16 = opndReg->Copy(m_func);
    opndReg16->SetType(TyInt16);
    instr = IR::Instr::New(Js::OpCode::CMP, this->m_func);
    instr->SetSrc1(opndReg16);
    instr->SetSrc2(IR::IntConstOpnd::New(Js::AtomTag, TyInt16, this->m_func, /* dontEncode = */ true));
    insertInstr->InsertBefore(instr);
#else
    // s1 = MOV src1 - Move to a temporary
    IR::Instr * instr   = IR::Instr::New(Js::OpCode::MOV, opndReg, opndSrc, this->m_func);
    insertInstr->InsertBefore(instr);

    if (instrFirst)
    {
        *instrFirst = instr;
    }

    // s1 = SHR s1, VarTag_Shift
    instr = IR::Instr::New(Js::OpCode::SHR, opndReg, opndReg, IR::IntConstOpnd::New(Js::VarTag_Shift, TyInt8, this->m_func), this->m_func);
    insertInstr->InsertBefore(instr);

    // CMP s1, AtomTag
    instr = IR::Instr::New(Js::OpCode::CMP, this->m_func);
    instr->SetSrc1(opndReg);
    instr->SetSrc2(IR::IntConstOpnd::New(Js::AtomTag, TyInt32, this->m_func, /* dontEncode = */ true));
    insertInstr->InsertBefore(instr);
#endif
    if(fContinueLabel)
    {
        // JEQ $labelHelper
        instr = IR::BranchInstr::New(Js::OpCode::JEQ, labelHelper, this->m_func);
    }
    else
    {
        // JNE $labelHelper
        instr = IR::BranchInstr::New(Js::OpCode::JNE, labelHelper, this->m_func);
    }
    insertInstr->InsertBefore(instr);
}

//
// If lower 32-bits are zero (value is zero), jump to $helper.
//
void LowererMD::GenerateTaggedZeroTest( IR::Opnd * opndSrc, IR::Instr * insertInstr, IR::LabelInstr * labelHelper )
{
    // Cast the var to 32 bit integer.
    if(opndSrc->GetSize() != 4)
    {
        opndSrc = opndSrc->UseWithNewType(TyUint32, this->m_func);
    }
    AssertMsg(TySize[opndSrc->GetType()] == 4, "This technique works only on the 32-bit version");

    // TEST src1, src1
    IR::Instr* instr = IR::Instr::New(Js::OpCode::TEST, this->m_func);
    instr->SetSrc1(opndSrc);
    instr->SetSrc2(opndSrc);
    insertInstr->InsertBefore(instr);

    if(labelHelper != nullptr)
    {
        // JZ $labelHelper
        instr = IR::BranchInstr::New(Js::OpCode::JEQ, labelHelper, this->m_func);
        insertInstr->InsertBefore(instr);
    }
}

//
// If top 16 bits are not zero i.e. it is NOT object, jump to $helper.
//
bool LowererMD::GenerateObjectTest(IR::Opnd * opndSrc, IR::Instr * insertInstr, IR::LabelInstr * labelTarget, bool fContinueLabel)
{
    AssertMsg(opndSrc->GetSize() == MachPtr, "64-bit register required");

    if (opndSrc->IsTaggedValue() && fContinueLabel)
    {
        // Insert delete branch opcode to tell the dbChecks not to assert on the helper label we may fall through into
        IR::Instr *fakeBr = IR::PragmaInstr::New(Js::OpCode::DeletedNonHelperBranch, 0, this->m_func);
        insertInstr->InsertBefore(fakeBr);

        return false;
    }
    else if (opndSrc->IsNotTaggedValue() && !fContinueLabel)
    {
        return false;
    }

    IR::Opnd  * opndReg = IR::RegOpnd::New(TyMachReg, this->m_func);

    // s1 = MOV src1 - Move to a temporary
    IR::Instr * instr   = IR::Instr::New(Js::OpCode::MOV, opndReg, opndSrc, this->m_func);
    insertInstr->InsertBefore(instr);

    // s1 = SHR s1, VarTag_Shift
    instr = IR::Instr::New(Js::OpCode::SHR, opndReg, opndReg, IR::IntConstOpnd::New(Js::VarTag_Shift, TyInt8, this->m_func), this->m_func);
    insertInstr->InsertBefore(instr);


    if (fContinueLabel)
    {
        // JEQ $labelHelper
        instr = IR::BranchInstr::New(Js::OpCode::JEQ, labelTarget, this->m_func);
        insertInstr->InsertBefore(instr);
        IR::LabelInstr *labelHelper = IR::LabelInstr::New(Js::OpCode::Label, this->m_func, true);
        insertInstr->InsertBefore(labelHelper);
    }
    else
    {
        // JNZ $labelHelper
        instr = IR::BranchInstr::New(Js::OpCode::JNE, labelTarget, this->m_func);
        insertInstr->InsertBefore(instr);
    }
    return true;
}

#else
//
// Convert an int32 value to a Var.
//
void LowererMD::GenerateInt32ToVarConversion( IR::Opnd * opndSrc, IR::Instr * insertInstr )
{
    // SHL r1, AtomTag

    IR::Instr * instr = IR::Instr::New(Js::OpCode::SHL, opndSrc, opndSrc, IR::IntConstOpnd::New(Js::AtomTag, TyInt32, this->m_func), this->m_func);
    insertInstr->InsertBefore(instr);

    // INC r1

    instr = IR::Instr::New(Js::OpCode::INC, opndSrc, opndSrc, this->m_func);
    insertInstr->InsertBefore(instr);
}

//
// jump to $labelHelper, based on the result of TEST
//
void LowererMD::GenerateSmIntTest(IR::Opnd *opndSrc, IR::Instr *insertInstr, IR::LabelInstr *labelHelper, IR::Instr **instrFirst /* = nullptr */, bool fContinueLabel /*= false*/)
{
    if (opndSrc->IsTaggedInt() && !fContinueLabel)
    {
        return;
    }
    else if (opndSrc->IsNotTaggedValue() && fContinueLabel)
    {
        return;
    }

    //      TEST src1, AtomTag
    IR::Instr* instr = IR::Instr::New(Js::OpCode::TEST, this->m_func);
    instr->SetSrc1(opndSrc);
    instr->SetSrc2(IR::IntConstOpnd::New(Js::AtomTag, TyInt8, this->m_func));
    insertInstr->InsertBefore(instr);

    if (instrFirst)
    {
        *instrFirst = instr;
    }

    if(fContinueLabel)
    {
        //      JNE $labelHelper
        instr = IR::BranchInstr::New(Js::OpCode::JNE, labelHelper, this->m_func);
    }
    else
    {
        //      JEQ $labelHelper
        instr = IR::BranchInstr::New(Js::OpCode::JEQ, labelHelper, this->m_func);
    }
    insertInstr->InsertBefore(instr);
}

//
// If value is zero in tagged int representation, jump to $labelHelper.
//
void LowererMD::GenerateTaggedZeroTest( IR::Opnd * opndSrc, IR::Instr * insertInstr, IR::LabelInstr * labelHelper )
{
    if (opndSrc->IsNotTaggedValue())
    {
        return;
    }

   // CMP src1, AtomTag
    IR::Instr* instr = IR::Instr::New(Js::OpCode::CMP, this->m_func);
    instr->SetSrc1(opndSrc);
    instr->SetSrc2(IR::IntConstOpnd::New(Js::AtomTag, TyInt32, this->m_func));
    insertInstr->InsertBefore(instr);

    // JEQ $helper
    if(labelHelper != nullptr)
    {
        // JEQ $labelHelper
        instr = IR::BranchInstr::New(Js::OpCode::JEQ, labelHelper, this->m_func);
        insertInstr->InsertBefore(instr);
    }
}

//
// If not object, jump to $labelHelper.
//
bool LowererMD::GenerateObjectTest(IR::Opnd * opndSrc, IR::Instr * insertInstr, IR::LabelInstr * labelTarget, bool fContinueLabel)
{
    if (opndSrc->IsTaggedInt() && fContinueLabel)
    {
        // Insert delete branch opcode to tell the dbChecks not to assert on this helper label
        IR::Instr *fakeBr = IR::PragmaInstr::New(Js::OpCode::DeletedNonHelperBranch, 0, this->m_func);
        insertInstr->InsertBefore(fakeBr);

        return false;
    }
    else if (opndSrc->IsNotTaggedValue() && !fContinueLabel)
    {
        return false;
    }

    // TEST src1, AtomTag
    IR::Instr* instr = IR::Instr::New(Js::OpCode::TEST, this->m_func);
    instr->SetSrc1(opndSrc);
    instr->SetSrc2(IR::IntConstOpnd::New(Js::AtomTag, TyInt8, this->m_func));
    insertInstr->InsertBefore(instr);

    if (fContinueLabel)
    {
        // JEQ $labelHelper
        instr = IR::BranchInstr::New(Js::OpCode::JEQ, labelTarget, this->m_func);
        insertInstr->InsertBefore(instr);
        IR::LabelInstr *labelHelper = IR::LabelInstr::New(Js::OpCode::Label, this->m_func, true);
        insertInstr->InsertBefore(labelHelper);
    }
    else
    {
        // JNE $labelHelper
        instr = IR::BranchInstr::New(Js::OpCode::JNE, labelTarget, this->m_func);
        insertInstr->InsertBefore(instr);
    }
    return true;
}

#endif

bool LowererMD::GenerateJSBooleanTest(IR::RegOpnd * regSrc, IR::Instr * insertInstr, IR::LabelInstr * labelTarget, bool fContinueLabel)
{
    IR::Instr* instr;
    if (regSrc->GetValueType().IsBoolean())
    {
        if (fContinueLabel)
        {
            // JMP $labelTarget
            instr = IR::BranchInstr::New(Js::OpCode::JMP, labelTarget, this->m_func);
            insertInstr->InsertBefore(instr);
#if DBG
            if (labelTarget->isOpHelper)
            {
                labelTarget->m_noHelperAssert = true;
            }
#endif
        }
        return false;
    }

    // CMP src1, vtable<JavaScriptBoolean>
    instr = IR::Instr::New(Js::OpCode::CMP, this->m_func);
    IR::IndirOpnd *vtablePtrOpnd = IR::IndirOpnd::New(regSrc, 0, TyMachPtr, this->m_func);
    instr->SetSrc1(vtablePtrOpnd);
    IR::Opnd *jsBooleanVTable = m_lowerer->LoadVTableValueOpnd(insertInstr, VTableValue::VtableJavascriptBoolean);
    instr->SetSrc2(jsBooleanVTable);
    insertInstr->InsertBefore(instr);
    Legalize(instr);

    if (fContinueLabel)
    {
        // JEQ $labelTarget
        instr = IR::BranchInstr::New(Js::OpCode::JEQ, labelTarget, this->m_func);
        insertInstr->InsertBefore(instr);
        IR::LabelInstr *labelHelper = IR::LabelInstr::New(Js::OpCode::Label, this->m_func, true);
        insertInstr->InsertBefore(labelHelper);
    }
    else
    {
        // JNE $labelTarget
        instr = IR::BranchInstr::New(Js::OpCode::JNE, labelTarget, this->m_func);
        insertInstr->InsertBefore(instr);
    }
    return true;
}

#if FLOATVAR
//
// If any of the top 14 bits are not set, then the var is not a float value and hence, jump to $labelHelper.
//
void LowererMD::GenerateFloatTest(IR::RegOpnd * opndSrc, IR::Instr * insertInstr, IR::LabelInstr* labelHelper, const bool checkForNullInLoopBody)
{
    if (opndSrc->GetValueType().IsFloat())
    {
        return;
    }

    AssertMsg(opndSrc->GetSize() == MachPtr, "64-bit register required");

    // s1 = MOV src1 - Move to a temporary
    IR::Opnd  * opndReg = IR::RegOpnd::New(TyMachReg, this->m_func);
    IR::Instr * instr   = IR::Instr::New(Js::OpCode::MOV, opndReg, opndSrc, this->m_func);
    insertInstr->InsertBefore(instr);

    // s1 = SHR s1, 50
    instr = IR::Instr::New(Js::OpCode::SHR, opndReg, opndReg, IR::IntConstOpnd::New(50, TyInt8, this->m_func), this->m_func);
    insertInstr->InsertBefore(instr);

    // JZ $helper
    instr = IR::BranchInstr::New(Js::OpCode::JEQ /* JZ */, labelHelper, this->m_func);
    insertInstr->InsertBefore(instr);
}

IR::RegOpnd* LowererMD::CheckFloatAndUntag(IR::RegOpnd * opndSrc, IR::Instr * insertInstr, IR::LabelInstr* labelHelper)
{
    IR::Opnd* floatTag = IR::AddrOpnd::New((Js::Var)Js::FloatTag_Value, IR::AddrOpndKindConstantVar, this->m_func, /* dontEncode = */ true);
    IR::RegOpnd* regOpndFloatTag = IR::RegOpnd::New(TyUint64, this->m_func);

    // MOV floatTagReg, FloatTag_Value
    IR::Instr* instr = IR::Instr::New(Js::OpCode::MOV, regOpndFloatTag, floatTag, this->m_func);
    insertInstr->InsertBefore(instr);

    if (!opndSrc->GetValueType().IsFloat())
    {
        // TEST s1, floatTagReg
        instr = IR::Instr::New(Js::OpCode::TEST, this->m_func);
        instr->SetSrc1(opndSrc);
        instr->SetSrc2(regOpndFloatTag);
        insertInstr->InsertBefore(instr);

        // JZ $helper
        instr = IR::BranchInstr::New(Js::OpCode::JEQ /* JZ */, labelHelper, this->m_func);
        insertInstr->InsertBefore(instr);
    }

    // untaggedFloat = XOR floatTagReg, s1 // where untaggedFloat == floatTagReg; use floatTagReg temporarily for the untagged float
    IR::RegOpnd* untaggedFloat = regOpndFloatTag;
    instr = IR::Instr::New(Js::OpCode::XOR, untaggedFloat, regOpndFloatTag, opndSrc, this->m_func);
    insertInstr->InsertBefore(instr);

    IR::RegOpnd *floatReg = IR::RegOpnd::New(TyMachDouble, this->m_func);
    instr = IR::Instr::New(Js::OpCode::MOVD, floatReg, untaggedFloat, this->m_func);
    insertInstr->InsertBefore(instr);
    return floatReg;
}
#else
void LowererMD::GenerateFloatTest(IR::RegOpnd * opndSrc, IR::Instr * insertInstr, IR::LabelInstr* labelHelper, const bool checkForNullInLoopBody)
{
    if (opndSrc->GetValueType().IsFloat())
    {
        return;
    }

    AssertMsg(opndSrc->GetSize() == MachPtr, "64-bit register required");

    if(checkForNullInLoopBody && m_func->IsLoopBody())
    {
        // It's possible that the value was determined dead by the jitted function and was not restored. The jitted loop
        // body may not realize that it's dead and may try to use it. Check for null in loop bodies.
        //     test src1, src1
        //     jz $helper (bail out)
        m_lowerer->InsertCompareBranch(
            opndSrc,
            IR::AddrOpnd::NewNull(m_func),
            Js::OpCode::BrEq_A,
            labelHelper,
            insertInstr);
    }

    IR::Instr* instr = IR::Instr::New(Js::OpCode::CMP, insertInstr->m_func);
    instr->SetSrc1(IR::IndirOpnd::New(opndSrc, 0, TyMachPtr, insertInstr->m_func));
    instr->SetSrc2(m_lowerer->LoadVTableValueOpnd(insertInstr, VTableValue::VtableJavascriptNumber));
    insertInstr->InsertBefore(instr);

    // JNZ $helper
    instr = IR::BranchInstr::New(Js::OpCode::JNE /* JZ */, labelHelper, this->m_func);
    insertInstr->InsertBefore(instr);
}

#endif


#if DBG
//
// Helps in debugging of fast paths.
//
void LowererMD::GenerateDebugBreak( IR::Instr * insertInstr )
{
    // int 3

    IR::Instr *int3 = IR::Instr::New(Js::OpCode::INT, insertInstr->m_func);
    int3->SetSrc1(IR::IntConstOpnd::New(3, TyInt32, insertInstr->m_func));
    insertInstr->InsertBefore(int3);
}
#endif

IR::Instr *
LowererMD::LoadStackAddress(StackSym *sym, IR::RegOpnd *optionalDstOpnd /* = nullptr */)
{
    IR::RegOpnd * regDst = optionalDstOpnd != nullptr ? optionalDstOpnd : IR::RegOpnd::New(TyMachReg, this->m_func);
    IR::SymOpnd * symSrc = IR::SymOpnd::New(sym, TyMachPtr, this->m_func);
    IR::Instr * lea = IR::Instr::New(Js::OpCode::LEA, regDst, symSrc, this->m_func);

    return lea;
}

template <bool verify>
void
LowererMD::MakeDstEquSrc1(IR::Instr *const instr)
{
    Assert(instr);
    Assert(instr->IsLowered());
    Assert(instr->GetDst());
    Assert(instr->GetSrc1());

    if(instr->GetDst()->IsEqual(instr->GetSrc1()))
    {
        return;
    }

    if (verify)
    {
        AssertMsg(false, "Missing legalization");
        return;
    }

    if(instr->GetSrc2() && instr->GetDst()->IsEqual(instr->GetSrc2()))
    {
        switch(instr->m_opcode)
        {
            case Js::OpCode::Add_I4:
            case Js::OpCode::Mul_I4:
            case Js::OpCode::Or_I4:
            case Js::OpCode::Xor_I4:
            case Js::OpCode::And_I4:
            case Js::OpCode::Add_Ptr:
            case Js::OpCode::ADD:
            case Js::OpCode::IMUL2:
            case Js::OpCode::OR:
            case Js::OpCode::XOR:
            case Js::OpCode::AND:
            case Js::OpCode::ADDSD:
            case Js::OpCode::MULSD:
            case Js::OpCode::ADDSS:
            case Js::OpCode::MULSS:
            case Js::OpCode::ADDPS:
                // For (a = b & a), generate (a = a & b)
                instr->SwapOpnds();
                return;
        }

        // For (a = b - a), generate (c = a; a = b - c) and fall through
        ChangeToAssign(instr->HoistSrc2(Js::OpCode::Ld_A));
    }

    // For (a = b - c), generate (a = b; a = a - c)
    IR::Instr *const mov = IR::Instr::New(Js::OpCode::Ld_A, instr->GetDst(), instr->UnlinkSrc1(), instr->m_func);
    instr->InsertBefore(mov);
    ChangeToAssign(mov);
    instr->SetSrc1(instr->GetDst());
}

void
LowererMD::EmitPtrInstr(IR::Instr *instr)
{
    LowererMDArch::EmitPtrInstr(instr);
}

void
LowererMD::EmitInt4Instr(IR::Instr *instr)
{
    LowererMDArch::EmitInt4Instr(instr);
}

void
LowererMD::EmitLoadVar(IR::Instr *instrLoad, bool isFromUint32, bool isHelper)
{
    lowererMDArch.EmitLoadVar(instrLoad, isFromUint32, isHelper);
}

bool
LowererMD::EmitLoadInt32(IR::Instr *instrLoad)
{
    return lowererMDArch.EmitLoadInt32(instrLoad);
}

void
LowererMD::EmitIntToFloat(IR::Opnd *dst, IR::Opnd *src, IR::Instr *instrInsert)
{
    this->lowererMDArch.EmitIntToFloat(dst, src, instrInsert);
}

void
LowererMD::EmitUIntToFloat(IR::Opnd *dst, IR::Opnd *src, IR::Instr *instrInsert)
{
    this->lowererMDArch.EmitUIntToFloat(dst, src, instrInsert);
}

void
LowererMD::EmitFloat32ToFloat64(IR::Opnd *dst, IR::Opnd *src, IR::Instr *instrInsert)
{
    // We should only generate this if sse2 is available
    Assert(AutoSystemInfo::Data.SSE2Available());

    Assert(dst->IsRegOpnd() && dst->IsFloat64());
    Assert(src->IsRegOpnd() && src->GetType() == TyFloat32);

    instrInsert->InsertBefore(IR::Instr::New(Js::OpCode::CVTSS2SD, dst, src, this->m_func));
}

void
LowererMD::EmitNon32BitOvfCheck(IR::Instr *instr, IR::Instr *insertInstr, IR::LabelInstr* bailOutLabel)
{
    AssertMsg(instr->m_opcode == Js::OpCode::IMUL, "IMUL should be used to check for non-32 bit overflow check on x86.");

    IR::RegOpnd *edxSym = IR::RegOpnd::New(TyInt32, instr->m_func);
#ifdef _M_IX86
    edxSym->SetReg(RegEDX);
#else
    edxSym->SetReg(RegRDX);
#endif

    // dummy def for edx to force RegAlloc to generate a lifetime. This is removed later by the Peeps phase.
    IR::Instr *newInstr = IR::Instr::New(Js::OpCode::NOP, edxSym, instr->m_func);
    insertInstr->InsertBefore(newInstr);

    IR::RegOpnd *temp = IR::RegOpnd::New(TyInt32, instr->m_func);
    Assert(instr->ignoreOverflowBitCount > 32);
    uint8 shamt = 64 - instr->ignoreOverflowBitCount;

    // MOV temp, edx
    newInstr = IR::Instr::New(Js::OpCode::MOV, temp, edxSym, instr->m_func);
    insertInstr->InsertBefore(newInstr);

    // SHL temp, shamt
    newInstr = IR::Instr::New(Js::OpCode::SHL, temp, temp, IR::IntConstOpnd::New(shamt, TyInt8, instr->m_func, true), instr->m_func);
    insertInstr->InsertBefore(newInstr);

    // SAR temp, shamt
    newInstr = IR::Instr::New(Js::OpCode::SAR, temp, temp, IR::IntConstOpnd::New(shamt, TyInt8, instr->m_func, true), instr->m_func);
    insertInstr->InsertBefore(newInstr);

    // CMP temp, edx
    newInstr = IR::Instr::New(Js::OpCode::CMP, instr->m_func);
    newInstr->SetSrc1(temp);
    newInstr->SetSrc2(edxSym);
    insertInstr->InsertBefore(newInstr);

    // JNE
    Lowerer::InsertBranch(Js::OpCode::JNE, false, bailOutLabel, insertInstr);
}

void LowererMD::ConvertFloatToInt32(IR::Opnd* intOpnd, IR::Opnd* floatOpnd, IR::LabelInstr * labelHelper, IR::LabelInstr * labelDone, IR::Instr * instInsert)
{
    UNREFERENCED_PARAMETER(labelHelper); // used on ARM
#if defined(_M_IX86)
    // We should only generate this if sse2 is available
    Assert(AutoSystemInfo::Data.SSE2Available());
#endif
    Assert((floatOpnd->IsRegOpnd() && floatOpnd->IsFloat()) || (floatOpnd->IsIndirOpnd() && floatOpnd->GetType() == TyMachDouble));

    Assert(intOpnd->GetType() == TyInt32);

    IR::Instr* instr;

    {
#ifdef _M_X64
        IR::Opnd* dstOpnd = IR::RegOpnd::New(TyInt64, m_func);
#else
        IR::Opnd* dstOpnd = intOpnd;
#endif
        // CVTTSD2SI dst, floatOpnd
        IR::Instr* instr = IR::Instr::New(floatOpnd->IsFloat64() ? Js::OpCode::CVTTSD2SI : Js::OpCode::CVTTSS2SI, dstOpnd, floatOpnd, this->m_func);
        instInsert->InsertBefore(instr);

        // CMP dst, 0x80000000 {0x8000000000000000 on x64}    -- Check for overflow
        instr = IR::Instr::New(Js::OpCode::CMP, this->m_func);
        instr->SetSrc1(dstOpnd);
        instr->SetSrc2(IR::AddrOpnd::New((Js::Var)MachSignBit, IR::AddrOpndKindConstant, this->m_func, true));
        instInsert->InsertBefore(instr);
        Legalize(instr);

#ifdef _M_X64
        // Truncate to int32 for x64.  We still need to go to helper though if we have int64 overflow.

        // MOV_TRUNC intOpnd, tmpOpnd
        instr = IR::Instr::New(Js::OpCode::MOV_TRUNC, intOpnd, dstOpnd, this->m_func);
        instInsert->InsertBefore(instr);
#endif
    }

    // JNE $done
    instr = IR::BranchInstr::New(Js::OpCode::JNE, labelDone, this->m_func);
    instInsert->InsertBefore(instr);

    // It does overflow - Let's try using FISTTP which uses 64 bits and is relevant only for x86
    // but requires going to memory and should only be used in overflow scenarios
#ifdef _M_IX86
    if (AutoSystemInfo::Data.SSE3Available())
    {
        IR::Opnd* floatStackOpnd;

        StackSym* tempSymDouble = this->m_func->tempSymDouble;
        if (!tempSymDouble)
        {
            this->m_func->tempSymDouble = StackSym::New(TyFloat64, this->m_func);
            this->m_func->StackAllocate(this->m_func->tempSymDouble, MachDouble);
            tempSymDouble = this->m_func->tempSymDouble;
        }
        IR::Opnd * float64Opnd;
        if (floatOpnd->IsFloat32())
        {
            float64Opnd = IR::RegOpnd::New(TyFloat64, m_func);
            IR::Instr* instr = IR::Instr::New(Js::OpCode::CVTSS2SD, float64Opnd, floatOpnd, m_func);
            instInsert->InsertBefore(instr);
        }
        else
        {
            float64Opnd = floatOpnd;
        }

        if (float64Opnd->IsRegOpnd())
        {
            floatStackOpnd = IR::SymOpnd::New(tempSymDouble, TyMachDouble, m_func);
            IR::Instr* instr = IR::Instr::New(Js::OpCode::MOVSD, floatStackOpnd, float64Opnd, m_func);
            instInsert->InsertBefore(instr);
        }
        else
        {
            floatStackOpnd = float64Opnd;
        }

        // FLD [tmpDouble]
        instr = IR::Instr::New(Js::OpCode::FLD, floatStackOpnd, floatStackOpnd, m_func);
        instInsert->InsertBefore(instr);

        if (!float64Opnd->IsRegOpnd())
        {
            floatStackOpnd = IR::SymOpnd::New(tempSymDouble, TyMachDouble, m_func);
        }

        // FISTTP qword ptr [tmpDouble]
        instr = IR::Instr::New(Js::OpCode::FISTTP, floatStackOpnd, m_func);
        instInsert->InsertBefore(instr);

        StackSym *intSym = StackSym::New(TyInt32, m_func);
        intSym->m_offset = tempSymDouble->m_offset;
        intSym->m_allocated = true;
        IR::Opnd* lowerBitsOpnd = IR::SymOpnd::New(intSym, TyInt32, m_func);

        // MOV dst, dword ptr [tmpDouble]
        instr = IR::Instr::New(Js::OpCode::MOV, intOpnd, lowerBitsOpnd, m_func);
        instInsert->InsertBefore(instr);

        // TEST dst, dst -- Check for overflow
        instr = IR::Instr::New(Js::OpCode::TEST, this->m_func);
        instr->SetSrc1(intOpnd);
        instr->SetSrc2(intOpnd);
        instInsert->InsertBefore(instr);

        instr = IR::BranchInstr::New(Js::OpCode::JNE, labelDone, this->m_func);
        instInsert->InsertBefore(instr);

        // CMP [tmpDouble - 4], 0x80000000
        StackSym* higherBitsSym = StackSym::New(TyInt32, m_func);
        higherBitsSym->m_offset = tempSymDouble->m_offset + 4;
        higherBitsSym->m_allocated = true;
        instr = IR::Instr::New(Js::OpCode::CMP, this->m_func);
        instr->SetSrc1(IR::SymOpnd::New(higherBitsSym, TyInt32, m_func));
        instr->SetSrc2(IR::IntConstOpnd::New(0x80000000, TyInt32, this->m_func, true));
        instInsert->InsertBefore(instr);

        instr = IR::BranchInstr::New(Js::OpCode::JNE, labelDone, this->m_func);
        instInsert->InsertBefore(instr);
    }
#endif
}

IR::Instr *
LowererMD::InsertConvertFloat64ToInt32(const RoundMode roundMode, IR::Opnd *const dst, IR::Opnd *const src, IR::Instr *const insertBeforeInstr)
{
    Assert(dst);
    Assert(dst->IsInt32());
    Assert(src);
    Assert(src->IsFloat64());
    Assert(insertBeforeInstr);

    // The caller is expected to check for overflow. To have that work be done automatically, use LowererMD::EmitFloatToInt.

    Func *const func = insertBeforeInstr->m_func;
    IR::AutoReuseOpnd autoReuseSrcPlusHalf;
    IR::Instr *instr = nullptr;

    switch (roundMode)
    {
        case RoundModeTowardInteger:
        {
            // Conversion with rounding towards nearest integer is not supported by the architecture. Add 0.5 and do a
            // round-toward-zero conversion instead.
            IR::RegOpnd *const srcPlusHalf = IR::RegOpnd::New(TyFloat64, func);
            autoReuseSrcPlusHalf.Initialize(srcPlusHalf, func);
            Lowerer::InsertAdd(
                false /* needFlags */,
                srcPlusHalf,
                src,
                IR::MemRefOpnd::New((double*)&(Js::JavascriptNumber::k_PointFive), TyFloat64, func,
                    IR::AddrOpndKindDynamicDoubleRef),
                insertBeforeInstr);

            instr = IR::Instr::New(LowererMD::MDConvertFloat64ToInt32Opcode(RoundModeTowardZero), dst, srcPlusHalf, func);

            insertBeforeInstr->InsertBefore(instr);
            LowererMD::Legalize(instr);
            return instr;
        }
        case RoundModeHalfToEven:
        {
            instr = IR::Instr::New(LowererMD::MDConvertFloat64ToInt32Opcode(RoundModeHalfToEven), dst, src, func);
            insertBeforeInstr->InsertBefore(instr);
            LowererMD::Legalize(instr);
            return instr;
        }
        default:
            AssertMsg(0, "RoundMode not supported.");
            return nullptr;
    }
}

void
LowererMD::EmitFloatToInt(IR::Opnd *dst, IR::Opnd *src, IR::Instr *instrInsert)
{
    IR::LabelInstr *labelDone = IR::LabelInstr::New(Js::OpCode::Label, this->m_func);
    IR::LabelInstr *labelHelper = IR::LabelInstr::New(Js::OpCode::Label, this->m_func, true);
    IR::Instr *instr;

    ConvertFloatToInt32(dst, src, labelHelper, labelDone, instrInsert);

    // $Helper
    instrInsert->InsertBefore(labelHelper);

#ifdef _M_X64
    // On x64, we can simply pass the var, this way we don't have to worry having to
    // pass a double in a param reg

    // s1 = MOVD src
    IR::RegOpnd *s1 = IR::RegOpnd::New(TyMachReg, this->m_func);
    instr = IR::Instr::New(Js::OpCode::MOVD, s1, src, this->m_func);
    instrInsert->InsertBefore(instr);

    // s1 = XOR s1, FloatTag_Value
    instr = IR::Instr::New(Js::OpCode::XOR, s1, s1,
                           IR::AddrOpnd::New((Js::Var)Js::FloatTag_Value, IR::AddrOpndKindConstantVar, this->m_func, /* dontEncode = */ true),
                           this->m_func);
    instrInsert->InsertBefore(instr);
    LowererMD::Legalize(instr);

    // dst = ToInt32_Full(s1, scriptContext);
    m_lowerer->LoadScriptContext(instrInsert);
    LoadHelperArgument(instrInsert, s1);

    instr = IR::Instr::New(Js::OpCode::CALL, dst, this->m_func);
    instrInsert->InsertBefore(instr);
    this->ChangeToHelperCall(instr, IR::HelperConv_ToInt32_Full);
#else
    // dst = ToInt32Core(src);
    LoadDoubleHelperArgument(instrInsert, src);

    instr = IR::Instr::New(Js::OpCode::CALL, dst, this->m_func);
    instrInsert->InsertBefore(instr);
    this->ChangeToHelperCall(instr, IR::HelperConv_ToInt32Core);
#endif
    // $Done
    instrInsert->InsertBefore(labelDone);
}

void
LowererMD::EmitLoadVarNoCheck(IR::RegOpnd * dst, IR::RegOpnd * src, IR::Instr *instrLoad, bool isFromUint32, bool isHelper)
{
#ifdef _M_IX86
    if (!AutoSystemInfo::Data.SSE2Available())
    {
        IR::JnHelperMethod helperMethod;

        // PUSH &floatTemp
        IR::Opnd *tempOpnd;
        if (instrLoad->dstIsTempNumber)
        {
            helperMethod = isFromUint32 ? IR::HelperOp_UInt32ToAtomInPlace : IR::HelperOp_Int32ToAtomInPlace;

            // Use the original dst to get the temp number sym
            StackSym * tempNumberSym = this->m_lowerer->GetTempNumberSym(instrLoad->GetDst(), instrLoad->dstIsTempNumberTransferred);

            IR::Instr *load = this->LoadStackAddress(tempNumberSym);
            instrLoad->InsertBefore(load);
            tempOpnd = load->GetDst();
            this->LoadHelperArgument(instrLoad, tempOpnd);
        }
        else
        {
            helperMethod = isFromUint32 ? IR::HelperOp_UInt32ToAtom : IR::HelperOp_Int32ToAtom;
        }
        //      PUSH memContext
        this->m_lowerer->LoadScriptContext(instrLoad);

        //      PUSH s1
        this->LoadHelperArgument(instrLoad, src);

        // dst = ToVar()
        IR::Instr * instr = IR::Instr::New(Js::OpCode::Call, dst,
            IR::HelperCallOpnd::New(helperMethod, this->m_func), this->m_func);
        instrLoad->InsertBefore(instr);
        this->LowerCall(instr, 0);
        return;
    }
#endif

    IR::RegOpnd * floatReg = IR::RegOpnd::New(TyFloat64, this->m_func);
    if (isFromUint32)
    {
        this->EmitUIntToFloat(floatReg, src, instrLoad);
    }
    else
    {
        this->EmitIntToFloat(floatReg, src, instrLoad);
    }
    this->SaveDoubleToVar(dst, floatReg, instrLoad, instrLoad, isHelper);
}

IR::Instr *
LowererMD::LowerGetCachedFunc(IR::Instr *instr)
{
    // src1 is an ActivationObjectEx, and we want to get the function object identified by the index (src2)
    // dst = MOV (src1)->GetFuncCacheEntry(src2)->func
    //
    // => [src1 + (offsetof(src1, cache) + (src2 * sizeof(FuncCacheEntry)) + offsetof(FuncCacheEntry, func))]

    IR::IntConstOpnd *src2Opnd = instr->UnlinkSrc2()->AsIntConstOpnd();
    IR::RegOpnd *src1Opnd = instr->UnlinkSrc1()->AsRegOpnd();

    instr->m_opcode = Js::OpCode::MOV;

    IntConstType offset = (src2Opnd->GetValue() * sizeof(Js::FuncCacheEntry)) + Js::ActivationObjectEx::GetOffsetOfCache() + offsetof(Js::FuncCacheEntry, func);
    Assert(Math::FitsInDWord(offset));
    instr->SetSrc1(IR::IndirOpnd::New(src1Opnd, (int32)offset, TyVar, this->m_func));

    src2Opnd->Free(this->m_func);

    return instr->m_prev;
}

IR::Instr *
LowererMD::LowerCommitScope(IR::Instr *instrCommit)
{
    IR::Instr *instrPrev = instrCommit->m_prev;
    IR::RegOpnd *baseOpnd = instrCommit->UnlinkSrc1()->AsRegOpnd();
    IR::Opnd *opnd;
    IR::Instr * insertInstr = instrCommit->m_next;

    // Write undef to all the local var slots.

    opnd = IR::IndirOpnd::New(baseOpnd, Js::ActivationObjectEx::GetOffsetOfCommitFlag(), TyInt8, this->m_func);
    instrCommit->SetDst(opnd);
    instrCommit->SetSrc1(IR::IntConstOpnd::New(1, TyInt8, this->m_func));
    IR::IntConstOpnd *intConstOpnd = instrCommit->UnlinkSrc2()->AsIntConstOpnd();
    LowererMD::ChangeToAssign(instrCommit);

    const Js::PropertyIdArray *propIds = Js::ByteCodeReader::ReadPropertyIdArray(intConstOpnd->AsUint32(), instrCommit->m_func->GetJnFunction());
    intConstOpnd->Free(this->m_func);

    uint firstVarSlot = (uint)Js::ActivationObjectEx::GetFirstVarSlot(propIds);
    if (firstVarSlot < propIds->count)
    {
        IR::RegOpnd *undefOpnd = IR::RegOpnd::New(TyMachReg, this->m_func);
        LowererMD::CreateAssign(undefOpnd, m_lowerer->LoadLibraryValueOpnd(insertInstr, LibraryValue::ValueUndefined), insertInstr);

        IR::RegOpnd *slotBaseOpnd = IR::RegOpnd::New(TyMachReg, this->m_func);

        // Load a pointer to the aux slots. We assume that all ActivationObject's have only aux slots.

        opnd = IR::IndirOpnd::New(baseOpnd, Js::DynamicObject::GetOffsetOfAuxSlots(), TyMachReg, this->m_func);
        this->CreateAssign(slotBaseOpnd, opnd, insertInstr);

        for (uint i = firstVarSlot; i < propIds->count; i++)
        {
            opnd = IR::IndirOpnd::New(slotBaseOpnd, i << this->GetDefaultIndirScale(), TyMachReg, this->m_func);
            this->CreateAssign(opnd, undefOpnd, insertInstr);
        }
    }

    return instrPrev;
}

void
LowererMD::ImmedSrcToReg(IR::Instr * instr, IR::Opnd * newOpnd, int srcNum)
{
    if (srcNum == 2)
    {
        instr->SetSrc2(newOpnd);
    }
    else
    {
        Assert(srcNum == 1);
        instr->SetSrc1(newOpnd);
    }
}

IR::LabelInstr *
LowererMD::GetBailOutStackRestoreLabel(BailOutInfo * bailOutInfo, IR::LabelInstr * exitTargetInstr)
{
    return lowererMDArch.GetBailOutStackRestoreLabel(bailOutInfo, exitTargetInstr);
}

StackSym *
LowererMD::GetImplicitParamSlotSym(Js::ArgSlot argSlot)
{
    return GetImplicitParamSlotSym(argSlot, this->m_func);
}

StackSym *
LowererMD::GetImplicitParamSlotSym(Js::ArgSlot argSlot, Func * func)
{
    // Stack looks like (EBP chain)+0, (return addr)+4, (function object)+8, (arg count)+12, (this)+16, actual args
    // Pass in the EBP+8 to start at the function object, the start of the implicit param slots

    StackSym * stackSym = StackSym::NewParamSlotSym(argSlot, func);
    func->SetArgOffset(stackSym, (2 + argSlot) * MachPtr);
    return stackSym;
}

bool LowererMD::GenerateFastAnd(IR::Instr * instrAnd)
{
    return this->lowererMDArch.GenerateFastAnd(instrAnd);
}

bool LowererMD::GenerateFastXor(IR::Instr * instrXor)
{
    return this->lowererMDArch.GenerateFastXor(instrXor);
}

bool LowererMD::GenerateFastOr(IR::Instr * instrOr)
{
    return this->lowererMDArch.GenerateFastOr(instrOr);
}

bool LowererMD::GenerateFastNot(IR::Instr * instrNot)
{
    return this->lowererMDArch.GenerateFastNot(instrNot);
}

bool LowererMD::GenerateFastShiftLeft(IR::Instr * instrShift)
{
    return this->lowererMDArch.GenerateFastShiftLeft(instrShift);
}

bool LowererMD::GenerateFastShiftRight(IR::Instr * instrShift)
{
    return this->lowererMDArch.GenerateFastShiftRight(instrShift);
}

void LowererMD::GenerateIsDynamicObject(IR::RegOpnd *regOpnd, IR::Instr *insertInstr, IR::LabelInstr *labelHelper, bool fContinueLabel)
{
    // CMP [srcReg], Js::DynamicObject::`vtable'
    {
        IR::Instr *cmp = IR::Instr::New(Js::OpCode::CMP, this->m_func);
        cmp->SetSrc1(IR::IndirOpnd::New(regOpnd, 0, TyMachPtr, m_func));
        cmp->SetSrc2(m_lowerer->LoadVTableValueOpnd(insertInstr, VTableValue::VtableDynamicObject));
        insertInstr->InsertBefore(cmp);
        Legalize(cmp);
    }

    if (fContinueLabel)
    {
        // JEQ $fallThough
        IR::Instr * jne = IR::BranchInstr::New(Js::OpCode::JEQ, labelHelper, this->m_func);
        insertInstr->InsertBefore(jne);
    }
    else
    {
        // JNE $helper
        IR::Instr * jne = IR::BranchInstr::New(Js::OpCode::JNE, labelHelper, this->m_func);
        insertInstr->InsertBefore(jne);
    }
}

void LowererMD::GenerateIsRecyclableObject(IR::RegOpnd *regOpnd, IR::Instr *insertInstr, IR::LabelInstr *labelHelper, bool checkObjectAndDynamicObject)
{
    // CMP [srcReg], Js::DynamicObject::`vtable'
    // JEQ $fallThough
    // MOV r1, [src1 + offset(type)]                      -- get the type id
    // MOV r1, [r1 + offset(typeId)]
    // ADD r1, ~TypeIds_LastJavascriptPrimitiveType       -- if (typeId > TypeIds_LastJavascriptPrimitiveType && typeId <= TypeIds_LastTrueJavascriptObjectType)
    // CMP r1, (TypeIds_LastTrueJavascriptObjectType - TypeIds_LastJavascriptPrimitiveType - 1)
    // JA $helper
    //fallThrough:

    IR::LabelInstr *labelFallthrough = IR::LabelInstr::New(Js::OpCode::Label, this->m_func);

    if (checkObjectAndDynamicObject)
    {
        if (!regOpnd->IsNotTaggedValue())
        {
            GenerateObjectTest(regOpnd, insertInstr, labelHelper);
        }

        this->GenerateIsDynamicObject(regOpnd, insertInstr, labelFallthrough, true);
    }

    IR::RegOpnd * typeRegOpnd = IR::RegOpnd::New(TyMachReg, this->m_func);
    IR::RegOpnd * typeIdRegOpnd = IR::RegOpnd::New(TyInt32, this->m_func);

    //  MOV r1, [src1 + offset(type)]
    {
        IR::IndirOpnd * indirOpnd = IR::IndirOpnd::New(regOpnd, Js::RecyclableObject::GetOffsetOfType(), TyMachReg, this->m_func);
        IR::Instr * mov = IR::Instr::New(Js::OpCode::MOV, typeRegOpnd, indirOpnd, this->m_func);
        insertInstr->InsertBefore(mov);
    }

    //  MOV r1, [r1 + offset(typeId)]
    {
        IR::IndirOpnd * indirOpnd = IR::IndirOpnd::New(typeRegOpnd, Js::Type::GetOffsetOfTypeId(), TyInt32, this->m_func);
        IR::Instr * mov = IR::Instr::New(Js::OpCode::MOV, typeIdRegOpnd, indirOpnd, this->m_func);
        insertInstr->InsertBefore(mov);
    }

    // ADD r1, ~TypeIds_LastJavascriptPrimitiveType
    {
        IR::Instr * add = IR::Instr::New(Js::OpCode::ADD, typeIdRegOpnd, typeIdRegOpnd, IR::IntConstOpnd::New(~Js::TypeIds_LastJavascriptPrimitiveType, TyInt32, this->m_func, true), this->m_func);
        insertInstr->InsertBefore(add);
    }

    // CMP r1, (TypeIds_LastTrueJavascriptObjectType - TypeIds_LastJavascriptPrimitiveType - 1)
    {
        IR::Instr * cmp = IR::Instr::New(Js::OpCode::CMP, this->m_func);
        cmp->SetSrc1(typeIdRegOpnd);
        cmp->SetSrc2(IR::IntConstOpnd::New(Js::TypeIds_LastTrueJavascriptObjectType - Js::TypeIds_LastJavascriptPrimitiveType - 1, TyInt32, this->m_func));
        insertInstr->InsertBefore(cmp);
    }

    // JA $helper
    {
        IR::Instr * jbe = IR::BranchInstr::New(Js::OpCode::JA, labelHelper, this->m_func);
        insertInstr->InsertBefore(jbe);
    }

    // $fallThrough
    insertInstr->InsertBefore(labelFallthrough);
}

bool
LowererMD::GenerateLdThisCheck(IR::Instr * instr)
{
    //
    // If not an recyclable object, jump to $helper
    // MOV dst, src1                                      -- return the object itself
    // JMP $fallthrough
    // $helper:
    //      (caller generates helper call)
    // $fallthrough:
    //
    IR::RegOpnd * src1 = instr->GetSrc1()->AsRegOpnd();
    IR::LabelInstr * helper = IR::LabelInstr::New(Js::OpCode::Label, m_func, true);
    IR::LabelInstr * fallthrough = IR::LabelInstr::New(Js::OpCode::Label, m_func);

    this->GenerateIsRecyclableObject(src1, instr, helper);

    // MOV dst, src1
    if (instr->GetDst() && !instr->GetDst()->IsEqual(src1))
    {
        IR::Instr * mov = IR::Instr::New(Js::OpCode::MOV, instr->GetDst(), src1, this->m_func);
        instr->InsertBefore(mov);
    }

    // JMP $fallthrough
    {
        IR::Instr * jmp = IR::BranchInstr::New(Js::OpCode::JMP, fallthrough, this->m_func);
        instr->InsertBefore(jmp);
    }

    // $helper:
    //      (caller generates helper call)
    // $fallthrough:
    instr->InsertBefore(helper);
    instr->InsertAfter(fallthrough);

    return true;
}


//
// TEST src, Js::AtomTag
// JNE $done
// MOV typeReg, objectSrc + offsetof(RecyclableObject::type)
// CMP [typeReg + offsetof(Type::typeid)], TypeIds_ActivationObject
// JEQ $helper
// $done:
// MOV dst, src
// JMP $fallthru
// helper:
// MOV dst, undefined
// $fallthru:
bool
LowererMD::GenerateLdThisStrict(IR::Instr* instr)
{
    IR::RegOpnd * src1 = instr->GetSrc1()->AsRegOpnd();
    IR::RegOpnd * typeReg = IR::RegOpnd::New(TyMachReg, this->m_func);
    IR::LabelInstr * done = IR::LabelInstr::New(Js::OpCode::Label, m_func);
    IR::LabelInstr * fallthru = IR::LabelInstr::New(Js::OpCode::Label, m_func);
    IR::LabelInstr * helper = IR::LabelInstr::New(Js::OpCode::Label, m_func, /*helper*/true);

    bool assign = instr->GetDst() && !instr->GetDst()->IsEqual(src1);
    // TEST src1, Js::AtomTag
    // JNE $done
    if(!src1->IsNotTaggedValue())
    {
        GenerateObjectTest(src1, instr, assign ? done : fallthru);
    }

    // MOV typeReg, objectSrc + offsetof(RecyclableObject::type)
    instr->InsertBefore(IR::Instr::New(Js::OpCode::MOV, typeReg,
        IR::IndirOpnd::New(src1, Js::RecyclableObject::GetOffsetOfType(), TyMachReg, m_func),
        m_func));

    // CMP [typeReg + offsetof(Type::typeid)], TypeIds_ActivationObject
    {
        IR::Instr * cmp = IR::Instr::New(Js::OpCode::CMP, m_func);
        cmp->SetSrc1(IR::IndirOpnd::New(typeReg, Js::Type::GetOffsetOfTypeId(), TyInt32, m_func));
        cmp->SetSrc2(IR::IntConstOpnd::New(Js::TypeId::TypeIds_ActivationObject, TyInt32, m_func));
        instr->InsertBefore(cmp);
    }

    // JEQ $helper
    instr->InsertBefore(IR::BranchInstr::New(Js::OpCode::JEQ, helper, m_func));

    if (assign)
    {
        // $done:
        // MOV dst, src
        instr->InsertBefore(done);
        instr->InsertBefore(IR::Instr::New(Js::OpCode::MOV, instr->GetDst(), src1, m_func));
    }

    // JMP $fallthru
    instr->InsertBefore(IR::BranchInstr::New(Js::OpCode::JMP, fallthru, m_func));

    instr->InsertBefore(helper);
    if (instr->GetDst())
    {
        // MOV dst, undefined
        instr->InsertBefore(IR::Instr::New(Js::OpCode::MOV, instr->GetDst(),
            m_lowerer->LoadLibraryValueOpnd(instr, LibraryValue::ValueUndefined), m_func));
    }
    // $fallthru:
    instr->InsertAfter(fallthru);

    return true;
}

// given object instanceof function, functionReg is a register with function,
// objectReg is a register with instance and inlineCache is an InstIsInlineCache.
// We want to generate:
//
// fallback on helper (will patch the inline cache) if function does not match the cache
// MOV dst, Js::false
// CMP functionReg, [&(inlineCache->function)]
// JNE helper
//
// fallback if object is a tagged int
// TEST objectReg, Js::AtomTag
// JNE done
//


// fallback if object's type is not the cached type
// MOV typeReg, objectSrc + offsetof(RecyclableObject::type)
// CMP typeReg, [&(inlineCache->type]
// JNE checkPrimType

// use the cached result and fallthrough
// MOV dst, [&(inlineCache->result)]
// JMP done

// return false if object is a primitive
// $checkPrimType
// CMP [typeReg + offsetof(Type::typeid)], TypeIds_LastJavascriptPrimitiveType
// JLE done
//
//
// $helper
// $done
bool
LowererMD::GenerateFastIsInst(IR::Instr * instr)
{
    IR::LabelInstr * helper = IR::LabelInstr::New(Js::OpCode::Label, m_func, true);
    IR::LabelInstr * checkPrimType = IR::LabelInstr::New(Js::OpCode::Label, m_func, true);
    IR::LabelInstr * done = IR::LabelInstr::New(Js::OpCode::Label, m_func);
    IR::RegOpnd * typeReg = IR::RegOpnd::New(TyMachReg, this->m_func);
    IR::Opnd * objectSrc;
    IR::RegOpnd * objectReg;
    IR::Opnd * functionSrc;
    IR::RegOpnd * functionReg;
    Js::IsInstInlineCache * inlineCache;
    IR::Instr * instrArg;

    // We are going to use the extra ArgOut_A instructions to lower the helper call later,
    // so we leave them alone here and clean them up then.
    inlineCache = instr->m_func->GetJnFunction()->GetIsInstInlineCache(instr->GetSrc1()->AsIntConstOpnd()->AsUint32());
    Assert(instr->GetSrc2()->AsRegOpnd()->m_sym->m_isSingleDef);
    instrArg = instr->GetSrc2()->AsRegOpnd()->m_sym->m_instrDef;

    objectSrc = instrArg->GetSrc1();
    Assert(instrArg->GetSrc2()->AsRegOpnd()->m_sym->m_isSingleDef);
    instrArg = instrArg->GetSrc2()->AsRegOpnd()->m_sym->m_instrDef;

    functionSrc = instrArg->GetSrc1();
    Assert(instrArg->GetSrc2() == nullptr);

    // MOV dst, Js::false
    Lowerer::InsertMove(instr->GetDst(), m_lowerer->LoadLibraryValueOpnd(instr, LibraryValue::ValueFalse), instr);

    if (functionSrc->IsRegOpnd())
    {
        functionReg = functionSrc->AsRegOpnd();
    }
    else
    {
        functionReg = IR::RegOpnd::New(TyMachReg, this->m_func);
        // MOV functionReg, functionSrc
        instr->InsertBefore(IR::Instr::New(Js::OpCode::MOV, functionReg, functionSrc, m_func));
    }

    // CMP functionReg, [&(inlineCache->function)]
    {
        IR::Instr * cmp = IR::Instr::New(Js::OpCode::CMP, m_func);
        cmp->SetSrc1(functionReg);
        cmp->SetSrc2(IR::MemRefOpnd::New((void*)&(inlineCache->function), TyMachReg, m_func,
            IR::AddrOpndKindDynamicIsInstInlineCacheFunctionRef));
        instr->InsertBefore(cmp);
        Legalize(cmp);
    }

    // JNE helper
    instr->InsertBefore(IR::BranchInstr::New(Js::OpCode::JNE, helper, m_func));

    if (objectSrc->IsRegOpnd())
    {
        objectReg = objectSrc->AsRegOpnd();
    }
    else
    {
        objectReg = IR::RegOpnd::New(TyMachReg, this->m_func);
        // MOV objectReg, objectSrc
        instr->InsertBefore(IR::Instr::New(Js::OpCode::MOV, objectReg, objectSrc, m_func));
    }

    // TEST objectReg, Js::AtomTag
    // JNE done
    GenerateObjectTest(objectReg, instr, done);

    // MOV typeReg, objectSrc + offsetof(RecyclableObject::type)
    instr->InsertBefore(IR::Instr::New(Js::OpCode::MOV, typeReg,
        IR::IndirOpnd::New(objectReg, Js::RecyclableObject::GetOffsetOfType(), TyMachReg, m_func),
        m_func));

    // CMP typeReg, [&(inlineCache->type]
    {
        IR::Instr * cmp = IR::Instr::New(Js::OpCode::CMP, m_func);
        cmp->SetSrc1(typeReg);
        cmp->SetSrc2(IR::MemRefOpnd::New((void*)&(inlineCache->type), TyMachReg, m_func,
            IR::AddrOpndKindDynamicIsInstInlineCacheTypeRef));
        instr->InsertBefore(cmp);
        Legalize(cmp);
    }

    // JNE checkPrimType
    instr->InsertBefore(IR::BranchInstr::New(Js::OpCode::JNE, checkPrimType, m_func));

    // MOV dst, [&(inlineCache->result)]
    Lowerer::InsertMove(instr->GetDst(), IR::MemRefOpnd::New((void*)&(inlineCache->result), TyMachReg, m_func,
        IR::AddrOpndKindDynamicIsInstInlineCacheResultRef), instr);

    // JMP done
    instr->InsertBefore(IR::BranchInstr::New(Js::OpCode::JMP, done, m_func));

    // LABEL checkPrimType
    instr->InsertBefore(checkPrimType);

    // CMP [typeReg + offsetof(Type::typeid)], TypeIds_LastJavascriptPrimitiveType
    {
        IR::Instr * cmp = IR::Instr::New(Js::OpCode::CMP, m_func);
        cmp->SetSrc1(IR::IndirOpnd::New(typeReg, Js::Type::GetOffsetOfTypeId(), TyInt32, m_func));
        cmp->SetSrc2(IR::IntConstOpnd::New(Js::TypeId::TypeIds_LastJavascriptPrimitiveType, TyInt32, m_func));
        instr->InsertBefore(cmp);
    }

    // JLE done
    instr->InsertBefore(IR::BranchInstr::New(Js::OpCode::JLE, done, m_func));

    // LABEL helper
    instr->InsertBefore(helper);

    instr->InsertAfter(done);

    return true;
}

void LowererMD::GenerateIsJsObjectTest(IR::RegOpnd* instanceReg, IR::Instr* insertInstr, IR::LabelInstr* labelHelper)
{
    // TEST instanceReg, (Js::AtomTag_IntPtr | Js::FloatTag_Value )
    GenerateObjectTest(instanceReg, insertInstr, labelHelper);

    IR::RegOpnd * typeReg = IR::RegOpnd::New(TyMachReg, this->m_func);

    // MOV typeReg, instanceReg + offsetof(RecyclableObject::type)
    insertInstr->InsertBefore(IR::Instr::New(Js::OpCode::MOV, typeReg,
        IR::IndirOpnd::New(instanceReg, Js::RecyclableObject::GetOffsetOfType(), TyMachReg, m_func),
        m_func));

    // CMP [typeReg + offsetof(Type::typeid)], TypeIds_LastJavascriptPrimitiveType
    IR::Instr * cmp = IR::Instr::New(Js::OpCode::CMP, this->m_func);
    cmp->SetSrc1(IR::IndirOpnd::New(typeReg, Js::Type::GetOffsetOfTypeId(), TyInt32, this->m_func));
    cmp->SetSrc2(IR::IntConstOpnd::New(Js::TypeId::TypeIds_LastJavascriptPrimitiveType, TyInt32, this->m_func));
    insertInstr->InsertBefore(cmp);

    // JLE labelHelper
    insertInstr->InsertBefore(IR::BranchInstr::New(Js::OpCode::JLE, labelHelper, this->m_func));
}

IR::Instr *
LowererMD::LowerToFloat(IR::Instr *instr)
{
    switch (instr->m_opcode)
    {
    case Js::OpCode::Add_A:
        Assert(instr->GetDst()->GetType() == instr->GetSrc1()->GetType());
        Assert(instr->GetSrc1()->GetType() == instr->GetSrc2()->GetType());
        instr->m_opcode = instr->GetSrc1()->IsFloat64() ? Js::OpCode::ADDSD : Js::OpCode::ADDSS;
        break;

    case Js::OpCode::Sub_A:
        Assert(instr->GetDst()->GetType() == instr->GetSrc1()->GetType());
        Assert(instr->GetSrc1()->GetType() == instr->GetSrc2()->GetType());
        instr->m_opcode = instr->GetSrc1()->IsFloat64() ? Js::OpCode::SUBSD : Js::OpCode::SUBSS;
        break;

    case Js::OpCode::Mul_A:
        Assert(instr->GetDst()->GetType() == instr->GetSrc1()->GetType());
        Assert(instr->GetSrc1()->GetType() == instr->GetSrc2()->GetType());
        instr->m_opcode = instr->GetSrc1()->IsFloat64() ? Js::OpCode::MULSD : Js::OpCode::MULSS;
        break;

    case Js::OpCode::Div_A:
        Assert(instr->GetDst()->GetType() == instr->GetSrc1()->GetType());
        Assert(instr->GetSrc1()->GetType() == instr->GetSrc2()->GetType());
        instr->m_opcode = instr->GetSrc1()->IsFloat64() ? Js::OpCode::DIVSD : Js::OpCode::DIVSS;
        break;

    case Js::OpCode::Neg_A:
    {
        IR::Opnd *opnd;
        instr->m_opcode = Js::OpCode::XORPS;
        if (instr->GetDst()->IsFloat32())
        {
            opnd = IR::MemRefOpnd::New((void*)&Js::JavascriptNumber::MaskNegFloat, TyMachDouble, this->m_func, IR::AddrOpndKindDynamicFloatRef);
        }
        else
        {
            Assert(instr->GetDst()->IsFloat64());
            opnd = IR::MemRefOpnd::New((void*)&Js::JavascriptNumber::MaskNegDouble, TyMachDouble, this->m_func, IR::AddrOpndKindDynamicDoubleRef);
        }
        instr->SetSrc2(opnd);
        Legalize(instr);
        break;
    }

    case Js::OpCode::BrEq_A:
    case Js::OpCode::BrNeq_A:
    case Js::OpCode::BrSrEq_A:
    case Js::OpCode::BrSrNeq_A:
    case Js::OpCode::BrGt_A:
    case Js::OpCode::BrGe_A:
    case Js::OpCode::BrLt_A:
    case Js::OpCode::BrLe_A:
    case Js::OpCode::BrNotEq_A:
    case Js::OpCode::BrNotNeq_A:
    case Js::OpCode::BrSrNotEq_A:
    case Js::OpCode::BrSrNotNeq_A:
    case Js::OpCode::BrNotGt_A:
    case Js::OpCode::BrNotGe_A:
    case Js::OpCode::BrNotLt_A:
    case Js::OpCode::BrNotLe_A:
        return this->LowerFloatCondBranch(instr->AsBranchInstr());

    default:
        Assume(UNREACHED);
    }

    this->MakeDstEquSrc1(instr);

    return instr;
}

IR::BranchInstr *
LowererMD::LowerFloatCondBranch(IR::BranchInstr *instrBranch, bool ignoreNan)
{
    Js::OpCode brOpcode = Js::OpCode::InvalidOpCode;
    Js::OpCode cmpOpcode = Js::OpCode::InvalidOpCode;
    IR::Instr *instr;
    bool swapCmpOpnds = false;
    bool addJP = false;
    IR::LabelInstr *labelNaN = nullptr;

    // Generate float compare that behave correctly for NaN's.
    // These branch on unordered:
    //  JB
    //  JBE
    //  JE
    // These don't branch on unordered:
    //  JA
    //  JAE
    //  JNE
    // Unfortunately, only JA and JAE do what we'd like....

    Func * func = instrBranch->m_func;
    IR::Opnd *src1 = instrBranch->UnlinkSrc1();
    IR::Opnd *src2 = instrBranch->UnlinkSrc2();

    Assert(src1->GetType() == src2->GetType());

    switch (instrBranch->m_opcode)
    {
    case Js::OpCode::BrSrEq_A:
    case Js::OpCode::BrEq_A:
    case Js::OpCode::BrSrNotNeq_A:
    case Js::OpCode::BrNotNeq_A:
        cmpOpcode = src1->IsFloat64() ? Js::OpCode::UCOMISD : Js::OpCode::UCOMISS;
        brOpcode = Js::OpCode::JEQ;

        if (!ignoreNan)
        {
            // Don't jump on NaN's
            labelNaN = instrBranch->GetOrCreateContinueLabel();
            addJP = true;
        }

        break;

    case Js::OpCode::BrNeq_A:
    case Js::OpCode::BrSrNeq_A:
    case Js::OpCode::BrSrNotEq_A:
    case Js::OpCode::BrNotEq_A:
        cmpOpcode = src1->IsFloat64() ? Js::OpCode::UCOMISD : Js::OpCode::UCOMISS;
        brOpcode = Js::OpCode::JNE;
        if (!ignoreNan)
        {
            // Jump on NaN's
            labelNaN = instrBranch->GetTarget();
            addJP = true;
        }
        break;

    case Js::OpCode::BrLe_A:
        swapCmpOpnds = true;
        brOpcode = Js::OpCode::JAE;
        break;

    case Js::OpCode::BrLt_A:
        swapCmpOpnds = true;
        brOpcode = Js::OpCode::JA;
        break;

    case Js::OpCode::BrGe_A:
        brOpcode = Js::OpCode::JAE;
        break;

    case Js::OpCode::BrGt_A:
        brOpcode = Js::OpCode::JA;
        break;

    case Js::OpCode::BrNotLe_A:
        swapCmpOpnds = true;
        brOpcode = Js::OpCode::JB;
        break;

    case Js::OpCode::BrNotLt_A:
        swapCmpOpnds = true;
        brOpcode = Js::OpCode::JBE;
        break;

    case Js::OpCode::BrNotGe_A:
        brOpcode = Js::OpCode::JB;
        break;

    case Js::OpCode::BrNotGt_A:
        brOpcode = Js::OpCode::JBE;
        break;
    default:
        Assume(UNREACHED);
    }

    // if we haven't set cmpOpcode, then we are using COMISD/COMISS
    if (cmpOpcode == Js::OpCode::InvalidOpCode)
    {
        cmpOpcode = src1->IsFloat64() ? Js::OpCode::COMISD : Js::OpCode::COMISS;
    }

    if (swapCmpOpnds)
    {
        IR::Opnd *tmp = src1;
        src1 = src2;
        src2 = tmp;
    }
    // VC generates UCOMISD for BrEq/BrNeq, and COMISD for all others, accordingly to IEEE 754.
    // We'll do the same.

    //  COMISD / UCOMISD src1, src2
    IR::Instr *instrCmp = IR::Instr::New(cmpOpcode, func);

    instrCmp->SetSrc1(src1);
    instrCmp->SetSrc2(src2);
    instrBranch->InsertBefore(instrCmp);
    Legalize(instrCmp);

    if (addJP)
    {
        // JP $LabelNaN
        instr = IR::BranchInstr::New(Js::OpCode::JP, labelNaN, func);
        instrBranch->InsertBefore(instr);
    }

    //  Jcc $L

    instr = IR::BranchInstr::New(brOpcode, instrBranch->GetTarget(), func);
    instrBranch->InsertBefore(instr);

    instrBranch->Remove();
    return instr->AsBranchInstr();
}
void LowererMD::HelperCallForAsmMathBuiltin(IR::Instr* instr, IR::JnHelperMethod helperMethodFloat, IR::JnHelperMethod helperMethodDouble)
{
    bool isFloat32 = (instr->GetSrc1()->GetType() == TyFloat32) ? true : false;
    switch (instr->m_opcode)
    {
    case Js::OpCode::InlineMathFloor:
    case Js::OpCode::InlineMathCeil:
    {
        AssertMsg(instr->GetDst()->IsFloat(), "dst must be float.");
        AssertMsg(instr->GetSrc1()->IsFloat(), "src1 must be float.");
        AssertMsg(!instr->GetSrc2() || instr->GetSrc2()->IsFloat(), "src2 must be float.");

        // Before:
        //      dst = <Built-in call> src1, src2
        // After:
        // I386:
        //      XMM0 = MOVSD src1
        //             CALL  helperMethod
        //      dst  = MOVSD call->dst
        // AMD64:
        //      XMM0 = MOVSD src1
        //      RAX =  MOV helperMethod
        //             CALL  RAX
        //      dst =  MOVSD call->dst

        // Src1
        IR::Instr* argOut = nullptr;
        IR::RegOpnd* dst1 = nullptr;
        if (isFloat32)
        {
            argOut = IR::Instr::New(Js::OpCode::MOVSS, this->m_func);
            dst1 = IR::RegOpnd::New(nullptr, (RegNum)FIRST_FLOAT_ARG_REG, TyFloat32, this->m_func);
        }
        else
        {
            argOut = IR::Instr::New(Js::OpCode::MOVSD, this->m_func);
            dst1 = IR::RegOpnd::New(nullptr, (RegNum)FIRST_FLOAT_ARG_REG, TyMachDouble, this->m_func);
        }

        dst1->m_isCallArg = true; // This is to make sure that lifetime of opnd is virtually extended until next CALL instr.
        argOut->SetDst(dst1);
        argOut->SetSrc1(instr->UnlinkSrc1());
        instr->InsertBefore(argOut);

        // Call CRT.
        IR::RegOpnd* floatCallDst = IR::RegOpnd::New(nullptr, (RegNum)(FIRST_FLOAT_REG), (isFloat32)?TyFloat32:TyMachDouble, this->m_func);   // Dst in XMM0.
        // s1 = MOV helperAddr
        IR::RegOpnd* s1 = IR::RegOpnd::New(TyMachReg, this->m_func);
        IR::AddrOpnd* helperAddr = IR::AddrOpnd::New((Js::Var)IR::GetMethodOriginalAddress((isFloat32)?helperMethodFloat:helperMethodDouble), IR::AddrOpndKind::AddrOpndKindDynamicMisc, this->m_func);
        IR::Instr* mov = IR::Instr::New(Js::OpCode::MOV, s1, helperAddr, this->m_func);
        instr->InsertBefore(mov);

        // dst(XMM0) = CALL s1
        IR::Instr *floatCall = IR::Instr::New(Js::OpCode::CALL, floatCallDst, s1, this->m_func);
        instr->InsertBefore(floatCall);
        // Save the result.
        instr->m_opcode = (isFloat32) ? Js::OpCode::MOVSS:Js::OpCode::MOVSD;
        instr->SetSrc1(floatCall->GetDst());
        break;
    }
    default:
        Assume(false);
        break;
    }
}
void LowererMD::GenerateFastInlineBuiltInCall(IR::Instr* instr, IR::JnHelperMethod helperMethod)
{
    switch (instr->m_opcode)
    {
    case Js::OpCode::InlineMathSqrt:
        // Sqrt maps directly to the SSE2 instruction.
        // src and dst should already be XMM registers, all we need is just change the opcode.
        Assert(helperMethod == (IR::JnHelperMethod)0);
        Assert(instr->GetSrc2() == nullptr);
        instr->m_opcode = instr->GetSrc1()->IsFloat64() ? Js::OpCode::SQRTSD : Js::OpCode::SQRTSS;
        break;

    case Js::OpCode::InlineMathAbs:
        Assert(helperMethod == (IR::JnHelperMethod)0);
        return GenerateFastInlineBuiltInMathAbs(instr);

    case Js::OpCode::InlineMathAcos:
    case Js::OpCode::InlineMathAsin:
    case Js::OpCode::InlineMathAtan:
    case Js::OpCode::InlineMathAtan2:
    case Js::OpCode::InlineMathCos:
    case Js::OpCode::InlineMathExp:
    case Js::OpCode::InlineMathLog:
    case Js::OpCode::InlineMathPow:
    case Js::OpCode::Expo_A:        //** operator reuses InlineMathPow fastpath
    case Js::OpCode::InlineMathSin:
    case Js::OpCode::InlineMathTan:
        {
            AssertMsg(instr->GetDst()->IsFloat(), "dst must be float.");
            AssertMsg(instr->GetSrc1()->IsFloat(), "src1 must be float.");
            AssertMsg(!instr->GetSrc2() || instr->GetSrc2()->IsFloat(), "src2 must be float.");

            // Before:
            //      dst = <Built-in call> src1, src2
            // After:
            // I386:
            //      XMM0 = MOVSD src1
            //             CALL  helperMethod
            //      dst  = MOVSD call->dst
            // AMD64:
            //      XMM0 = MOVSD src1
            //      RAX =  MOV helperMethod
            //             CALL  RAX
            //      dst =  MOVSD call->dst

            // Src1
            IR::Instr* argOut = IR::Instr::New(Js::OpCode::MOVSD, this->m_func);
            IR::RegOpnd* dst1 = IR::RegOpnd::New(nullptr, (RegNum)FIRST_FLOAT_ARG_REG, TyMachDouble, this->m_func);
            dst1->m_isCallArg = true; // This is to make sure that lifetime of opnd is virtually extended until next CALL instr.
            argOut->SetDst(dst1);
            argOut->SetSrc1(instr->UnlinkSrc1());
            instr->InsertBefore(argOut);

            // Src2
            if (instr->GetSrc2() != nullptr)
            {
                IR::Instr* argOut2 = IR::Instr::New(Js::OpCode::MOVSD, this->m_func);
                IR::RegOpnd* dst2 = IR::RegOpnd::New(nullptr, (RegNum)(FIRST_FLOAT_ARG_REG + 1), TyMachDouble, this->m_func);
                dst2->m_isCallArg = true;   // This is to make sure that lifetime of opnd is virtually extended until next CALL instr.
                argOut2->SetDst(dst2);
                argOut2->SetSrc1(instr->UnlinkSrc2());
                instr->InsertBefore(argOut2);
            }

            // Call CRT.
            IR::RegOpnd* floatCallDst = IR::RegOpnd::New(nullptr, (RegNum)(FIRST_FLOAT_REG), TyMachDouble, this->m_func);   // Dst in XMM0.
#ifdef _M_IX86
            IR::Instr* floatCall = IR::Instr::New(Js::OpCode::CALL, floatCallDst, this->m_func);
            floatCall->SetSrc1(IR::HelperCallOpnd::New(helperMethod, this->m_func));
            instr->InsertBefore(floatCall);
#else
            // s1 = MOV helperAddr
            IR::RegOpnd* s1 = IR::RegOpnd::New(TyMachReg, this->m_func);
            IR::AddrOpnd* helperAddr = IR::AddrOpnd::New((Js::Var)IR::GetMethodOriginalAddress(helperMethod), IR::AddrOpndKind::AddrOpndKindDynamicMisc, this->m_func);
            IR::Instr* mov = IR::Instr::New(Js::OpCode::MOV, s1, helperAddr, this->m_func);
            instr->InsertBefore(mov);

            // dst(XMM0) = CALL s1
            IR::Instr *floatCall = IR::Instr::New(Js::OpCode::CALL, floatCallDst, s1, this->m_func);
            instr->InsertBefore(floatCall);
#endif
            // Save the result.
            instr->m_opcode = Js::OpCode::MOVSD;
            instr->SetSrc1(floatCall->GetDst());
            break;
        }

    case Js::OpCode::InlineMathFloor:
    case Js::OpCode::InlineMathCeil:
    case Js::OpCode::InlineMathRound:
        {
            Assert(instr->GetDst()->IsInt32() || instr->GetDst()->IsFloat());
            //     MOVSD roundedFloat, src
            //
            // if(round)
            // {
            //     CMP roundedFloat. -0.5
            //     JL $addHalfToRoundSrc
            //     CMP roundedFloat, 0
            //     JL $bailoutLabel
            // }
            //
            // $addHalfToRoundSrc:
            //     ADDSD roundedFloat, 0.5
            //
            // if(isNotCeil)
            // {
            //     CMP roundedFloat, 0
            //     JGE $skipRoundSd
            // }
            //     ROUNDSD roundedFloat, roundedFloat, round_mode
            //
            // $skipRoundSd:
            //     if(isNotCeil)
            //         MOVSD checkNegZeroOpnd, roundedFloat
            //     else if (ceil)
            //         MOVSD checkNegZeroOpnd, src
            //
            //     CMP checkNegZeroOpnd, 0
            //     JNE $convertToInt
            //
            // if(instr->ShouldCheckForNegativeZero())
            // {
            //     isNegZero CALL IsNegZero(checkNegZeroOpnd)
            //     CMP isNegZero, 0
            //     JNE $bailoutLabel
            // }
            //
            // $convertToInt:
            //     CVT(T)SD2SI dst, roundedFloat //CVTTSD2SI for floor/round and CVTSD2SI for ceil
            //     CMP dst 0x80000000
            //     JNE $fallthrough
            //
            // if(!sharedBailout)
            // {
            //     $bailoutLabel:
            // }
            //     GenerateBailout(instr)
            //
            // $fallthrough:

            bool isNotCeil = instr->m_opcode != Js::OpCode::InlineMathCeil;

            // MOVSD roundedFloat, src
            IR::Opnd * src = instr->UnlinkSrc1();
            IR::RegOpnd* roundedFloat = IR::RegOpnd::New(src->GetType(), this->m_func);
            IR::Instr* argOut = IR::Instr::New(LowererMDArch::GetAssignOp(src->GetType()), roundedFloat, src, this->m_func);
            instr->InsertBefore(argOut);
            bool negZeroCheckDone = false;


            IR::LabelInstr * bailoutLabel = nullptr;
            bool sharedBailout = false;
            if (instr->GetDst()->IsInt32())
            {
                sharedBailout = (instr->GetBailOutInfo()->bailOutInstr != instr) ? true : false;
                if (sharedBailout)
                {
                    bailoutLabel = instr->GetBailOutInfo()->bailOutInstr->AsLabelInstr();
                }
                else
                {
                    bailoutLabel = IR::LabelInstr::New(Js::OpCode::Label, this->m_func, /*helperLabel*/true);
                }
            }

            IR::Opnd * zero;
            if (src->IsFloat64())
            {
                zero = IR::MemRefOpnd::New((double*)&(Js::JavascriptNumber::k_Zero), TyFloat64, this->m_func, IR::AddrOpndKindDynamicDoubleRef);
            }
            else
            {
                Assert(src->IsFloat32());
                zero = IR::MemRefOpnd::New((float*)&Js::JavascriptNumber::k_Float32Zero, TyFloat32, this->m_func, IR::AddrOpndKindDynamicFloatRef);
            }
            if(instr->m_opcode == Js::OpCode::InlineMathRound)
            {
                if(instr->ShouldCheckForNegativeZero())
                {
                    IR::LabelInstr * addHalfToRoundSrcLabel = IR::LabelInstr::New(Js::OpCode::Label, this->m_func);

                    IR::LabelInstr* negativeCheckLabel = IR::LabelInstr::New(Js::OpCode::Label, this->m_func, /*helperLabel*/ true);
                    IR::LabelInstr* negZeroTest = IR::LabelInstr::New(Js::OpCode::Label, this->m_func, /*helperLabel*/ true);
                    this->m_lowerer->InsertCompareBranch(roundedFloat, zero, Js::OpCode::BrGt_A, addHalfToRoundSrcLabel, instr);
                    instr->InsertBefore(negativeCheckLabel);

                    this->m_lowerer->InsertBranch(Js::OpCode::BrEq_A, negZeroTest, instr);

                    IR::Opnd * negPointFive;
                    if (src->IsFloat64())
                    {
                        negPointFive = IR::MemRefOpnd::New((double*)&Js::JavascriptNumber::k_NegPointFive, IRType::TyFloat64, this->m_func, IR::AddrOpndKindDynamicDoubleRef);
                    }
                    else
                    {
                        Assert(src->IsFloat32());
                        negPointFive = IR::MemRefOpnd::New((float*)&Js::JavascriptNumber::k_Float32NegPointFive, TyFloat32, this->m_func, IR::AddrOpndKindDynamicFloatRef);
                    }
                    this->m_lowerer->InsertCompareBranch(roundedFloat, negPointFive, Js::OpCode::BrGe_A, bailoutLabel, instr);
                    this->m_lowerer->InsertBranch(Js::OpCode::Br, addHalfToRoundSrcLabel, instr);

                    instr->InsertBefore(negZeroTest);
                    IR::Opnd* isNegZero = IsOpndNegZero(src, instr);
                    this->m_lowerer->InsertTestBranch(isNegZero, isNegZero, Js::OpCode::BrNeq_A, bailoutLabel, instr);
                    this->m_lowerer->InsertBranch(Js::OpCode::Br, addHalfToRoundSrcLabel, instr);
                    negZeroCheckDone = true;

                    instr->InsertBefore(addHalfToRoundSrcLabel);
                }
                IR::Opnd * pointFive;
                if (src->IsFloat64())
                {
                    pointFive = IR::MemRefOpnd::New((double*)&(Js::JavascriptNumber::k_PointFive), TyFloat64, this->m_func,
                        IR::AddrOpndKindDynamicDoubleRef);
                }
                else
                {
                    Assert(src->IsFloat32());
                    pointFive = IR::MemRefOpnd::New((float*)&Js::JavascriptNumber::k_Float32PointFive, TyFloat32, this->m_func, IR::AddrOpndKindDynamicFloatRef);
                }
                IR::Instr * addInstr = IR::Instr::New(src->IsFloat64() ? Js::OpCode::ADDSD : Js::OpCode::ADDSS, roundedFloat, roundedFloat, pointFive, this->m_func);
                instr->InsertBefore(addInstr);
                Legalize(addInstr);
            }

            IR::LabelInstr * skipRoundSd = IR::LabelInstr::New(Js::OpCode::Label, this->m_func);
            if (instr->m_opcode == Js::OpCode::InlineMathFloor && instr->GetDst()->IsInt32())
            {
                this->m_lowerer->InsertCompareBranch(roundedFloat, zero, Js::OpCode::BrGe_A, skipRoundSd, instr);
            }

            // ROUNDSD srcCopy, srcCopy, round_mode
            IR::Opnd * roundMode;
            if(isNotCeil)
            {
                roundMode = IR::IntConstOpnd::New(0x01, TyInt32, this->m_func);
            }
            else if (instr->GetDst()->IsInt32() || instr->m_opcode != Js::OpCode::InlineMathFloor)
            {
                roundMode = IR::IntConstOpnd::New(0x02, TyInt32, this->m_func);
            }
            else
            {
                roundMode = IR::IntConstOpnd::New(0x03, TyInt32, this->m_func);
            }
            if (!skipRoundSd)
            {
                Assert(AutoSystemInfo::Data.SSE4_1Available());
            }
            IR::Instr* roundInstr = IR::Instr::New(src->IsFloat64() ? Js::OpCode::ROUNDSD : Js::OpCode::ROUNDSS, roundedFloat, roundedFloat, roundMode, this->m_func);

            instr->InsertBefore(roundInstr);


            if (instr->GetDst()->IsInt32())
            {
                if (instr->m_opcode == Js::OpCode::InlineMathFloor)
                {
                    instr->InsertBefore(skipRoundSd);
                }

                //negZero bailout
                if(instr->ShouldCheckForNegativeZero() && !negZeroCheckDone)
                {
                    IR::LabelInstr * convertToInt = IR::LabelInstr::New(Js::OpCode::Label, this->m_func);
                    IR::Opnd * checkNegZeroOpnd;

                    if(isNotCeil)
                    {
                        checkNegZeroOpnd = src;
                    }
                    else
                    {
                        checkNegZeroOpnd = roundedFloat;
                    }
                    this->m_lowerer->InsertCompareBranch(checkNegZeroOpnd, zero, Js::OpCode::BrNeq_A, convertToInt, instr);

                    IR::Opnd* isNegZero = IsOpndNegZero(checkNegZeroOpnd, instr);

                    this->m_lowerer->InsertCompareBranch(isNegZero, IR::IntConstOpnd::New(0x00000000, IRType::TyInt32, this->m_func), Js::OpCode::BrNeq_A, bailoutLabel, instr);
                    instr->InsertBefore(convertToInt);
                }

                IR::Opnd * originalDst = instr->UnlinkDst();

                // CVT(T)SD2SI dst, srcCopy
                IR::Instr* convertToIntInstr;
                if (isNotCeil)
                {
                    convertToIntInstr = IR::Instr::New(src->IsFloat64() ? Js::OpCode::CVTTSD2SI : Js::OpCode::CVTTSS2SI, originalDst, roundedFloat, this->m_func);
                }
                else
                {
                    convertToIntInstr = IR::Instr::New(src->IsFloat64() ? Js::OpCode::CVTSD2SI : Js::OpCode::CVTSS2SI, originalDst, roundedFloat, this->m_func);
                }
                instr->InsertBefore(convertToIntInstr);

                IR::LabelInstr * fallthrough = IR::LabelInstr::New(Js::OpCode::Label, this->m_func);
                IR::Opnd * intOverflowValue = IR::IntConstOpnd::New(INT32_MIN, IRType::TyInt32, this->m_func, true);
                this->m_lowerer->InsertCompareBranch(originalDst, intOverflowValue, Js::OpCode::BrNeq_A, fallthrough, instr);

                instr->InsertAfter(fallthrough);
                if (!sharedBailout)
                {
                    instr->InsertBefore(bailoutLabel);
                }
                this->m_lowerer->GenerateBailOut(instr);
            }
            else
            {
                IR::Opnd * originalDst = instr->UnlinkDst();
                Assert(originalDst->IsFloat());
                Assert(originalDst->GetType() == roundedFloat->GetType());
                IR::Instr * movInstr = IR::Instr::New(originalDst->IsFloat64() ? Js::OpCode::MOVSD : Js::OpCode::MOVSS, originalDst, roundedFloat, this->m_func);
                instr->InsertBefore(movInstr);
                instr->Remove();
            }
            break;
        }

    case Js::OpCode::InlineMathMin:
    case Js::OpCode::InlineMathMax:
        {
            IR::Opnd* src1 = instr->GetSrc1();
            IR::Opnd* src2 = instr->GetSrc2();
            IR::Opnd* dst = instr->GetDst();
            IR::LabelInstr* doneLabel = IR::LabelInstr::New(Js::OpCode::Label, this->m_func);
            IR::LabelInstr* labelNaNHelper = IR::LabelInstr::New(Js::OpCode::Label, this->m_func, true);
            IR::LabelInstr* labelNegZeroAndNaNCheckHelper = IR::LabelInstr::New(Js::OpCode::Label, this->m_func, true);
            IR::Instr* branchInstr;

            bool min = instr->m_opcode == Js::OpCode::InlineMathMin ? true : false;

            // CMP src1, src2
            if(dst->IsInt32())
            {
                //MOV dst, src2;
                Assert(!dst->IsEqual(src2));
                this->m_lowerer->InsertMove(dst, src2, instr);
                if(min)
                {
                    // JLT $continueLabel
                    branchInstr = IR::BranchInstr::New(Js::OpCode::BrGt_I4, doneLabel, src1, src2, instr->m_func);
                    instr->InsertBefore(branchInstr);
                    LowererMDArch::EmitInt4Instr(branchInstr);
                }
                else
                {
                    // JGT $continueLabel
                    branchInstr = IR::BranchInstr::New(Js::OpCode::BrLt_I4, doneLabel, src1, src2, instr->m_func);
                    instr->InsertBefore(branchInstr);
                    LowererMDArch::EmitInt4Instr(branchInstr);
                }
                    // MOV dst, src1
                    this->m_lowerer->InsertMove(dst, src1, instr);
            }

            else if(dst->IsFloat64())
            {
                //      COMISD src1 (src2), src2 (src1)
                //      JA $doneLabel
                //      JEQ $labelNegZeroAndNaNCheckHelper
                //      MOVSD dst, src2
                //      JMP $doneLabel
                //
                // $labelNegZeroAndNaNCheckHelper
                //      JP $labelNaNHelper
                //      if(min)
                //      {
                //          if(src2 == -0.0)
                //              MOVSD dst, src2
                //      }
                //      else
                //      {
                //          if(src1 == -0.0)
                //              MOVSD dst, src2
                //      }
                //      JMP $doneLabel
                //
                // $labelNaNHelper
                //      MOVSD dst, NaN
                //
                // $doneLabel

                //MOVSD dst, src1;
                Assert(!dst->IsEqual(src1));
                this->m_lowerer->InsertMove(dst, src1, instr);

                if(min)
                {
                    this->m_lowerer->InsertCompareBranch(src1, src2, Js::OpCode::BrLt_A, doneLabel, instr); // Lowering of BrLt_A for floats is done to JA with operands swapped
                }
                else
                {
                    this->m_lowerer->InsertCompareBranch(src1, src2, Js::OpCode::BrGt_A, doneLabel, instr);
                }

                instr->InsertBefore(IR::BranchInstr::New(Js::OpCode::JEQ, labelNegZeroAndNaNCheckHelper, instr->m_func));

                this->m_lowerer->InsertMove(dst, src2, instr);
                instr->InsertBefore(IR::BranchInstr::New(Js::OpCode::JMP, doneLabel, instr->m_func));

                instr->InsertBefore(labelNegZeroAndNaNCheckHelper);

                instr->InsertBefore(IR::BranchInstr::New(Js::OpCode::JP, labelNaNHelper, instr->m_func));

                IR::Opnd* isNegZero;
                if(min)
                {
                    isNegZero =  IsOpndNegZero(src2, instr);
                }
                else
                {
                    isNegZero =  IsOpndNegZero(src1, instr);
                }

                this->m_lowerer->InsertCompareBranch(isNegZero, IR::IntConstOpnd::New(0x00000000, IRType::TyInt32, this->m_func), Js::OpCode::BrEq_A, doneLabel, instr);

                this->m_lowerer->InsertMove(dst, src2, instr);
                instr->InsertBefore(IR::BranchInstr::New(Js::OpCode::JMP, doneLabel, instr->m_func));

                instr->InsertBefore(labelNaNHelper);
                IR::Opnd * opndNaN = IR::MemRefOpnd::New((double*)&(Js::JavascriptNumber::k_Nan), IRType::TyFloat64, this->m_func);
                this->m_lowerer->InsertMove(dst, opndNaN, instr);
            }
            instr->InsertBefore(doneLabel);

            instr->Remove();
            break;
        }

    default:
        AssertMsg(FALSE, "Unknown inline built-in opcode");
        break;
    }
}

IR::Opnd* LowererMD::IsOpndNegZero(IR::Opnd* opnd, IR::Instr* instr)
{
    IR::Opnd * isNegZero = IR::RegOpnd::New(TyInt32, this->m_func);

#if defined(_M_IX86)
    LoadDoubleHelperArgument(instr, opnd);
    IR::Instr * helperCallInstr = IR::Instr::New(Js::OpCode::CALL, isNegZero, this->m_func);
    instr->InsertBefore(helperCallInstr);
    this->ChangeToHelperCall(helperCallInstr, IR::HelperIsNegZero);

#else
    IR::RegOpnd* regXMM0 = IR::RegOpnd::New(nullptr, (RegNum)FIRST_FLOAT_ARG_REG, TyMachDouble, this->m_func);
    regXMM0->m_isCallArg = true;
    IR::Instr * movInstr = IR::Instr::New(Js::OpCode::MOVSD, regXMM0, opnd, this->m_func);
    instr->InsertBefore(movInstr);

    IR::RegOpnd* reg1 = IR::RegOpnd::New(TyMachReg, this->m_func);
    IR::AddrOpnd* helperAddr = IR::AddrOpnd::New((Js::Var)IR::GetMethodOriginalAddress(IR::HelperIsNegZero), IR::AddrOpndKind::AddrOpndKindDynamicMisc, this->m_func);
    IR::Instr* mov = IR::Instr::New(Js::OpCode::MOV, reg1, helperAddr, this->m_func);
    instr->InsertBefore(mov);

    IR::Instr *helperCallInstr = IR::Instr::New(Js::OpCode::CALL, isNegZero, reg1, this->m_func);
    instr->InsertBefore(helperCallInstr);

#endif

    return isNegZero;
}

void LowererMD::GenerateFastInlineBuiltInMathAbs(IR::Instr* inlineInstr)
{
    IR::Opnd* src = inlineInstr->GetSrc1();
    IR::Opnd* dst = inlineInstr->UnlinkDst();
    Assert(src);
    IR::Instr* tmpInstr;

    IR::Instr* nextInstr = IR::LabelInstr::New(Js::OpCode::Label, m_func);
    IR::Instr* continueInstr = m_lowerer->LowerBailOnIntMin(inlineInstr);
    continueInstr->InsertAfter(nextInstr);

    IRType srcType = src->GetType();
    if (srcType == IRType::TyInt32)
    {
        // Note: if execution gets so far, we always get (untagged) int32 here.
        // Since -x = ~x + 1, abs(x) = x, abs(-x) = -x, sign-extend(x) = 0, sign_extend(-x) = -1, where 0 <= x.
        // Then: abs(x) = sign-extend(x) XOR x - sign-extend(x)

        // Expected input (otherwise bailout):
        // - src1 is (untagged) int, not equal to int_min (abs(int_min) would produce overflow, as there's no corresponding positive int).

        //      MOV EAX, src
        IR::RegOpnd *regEAX = IR::RegOpnd::New(TyInt32, this->m_func);
        regEAX->SetReg(LowererMDArch::GetRegIMulDestLower());

        tmpInstr = IR::Instr::New(Js::OpCode::MOV, regEAX, src, this->m_func);
        nextInstr->InsertBefore(tmpInstr);

        IR::RegOpnd *regEDX = IR::RegOpnd::New(TyInt32, this->m_func);
        regEDX->SetReg(LowererMDArch::GetRegIMulHighDestLower());

        //      CDQ (sign-extend EAX into EDX, producing 64bit EDX:EAX value)
        // Note: put EDX on dst to give of def to the EDX lifetime
        tmpInstr = IR::Instr::New(Js::OpCode::CDQ, regEDX, this->m_func);
        nextInstr->InsertBefore(tmpInstr);

        //      XOR EAX, EDX
        tmpInstr = IR::Instr::New(Js::OpCode::XOR, regEAX, regEAX, regEDX, this->m_func);
        nextInstr->InsertBefore(tmpInstr);

        //      SUB EAX, EDX
        tmpInstr = IR::Instr::New(Js::OpCode::SUB, regEAX, regEAX, regEDX, this->m_func);
        nextInstr->InsertBefore(tmpInstr);

        //      MOV dst, EAX
        tmpInstr = IR::Instr::New(Js::OpCode::MOV, dst, regEAX, this->m_func);
        nextInstr->InsertBefore(tmpInstr);
    }
    else if (srcType == IRType::TyFloat64)
    {
        if (!dst->IsRegOpnd())
        {
            // MOVSD tempRegOpnd, src
            IR::RegOpnd* tempRegOpnd = IR::RegOpnd::New(nullptr, TyMachDouble, this->m_func);
            tempRegOpnd->m_isCallArg = true; // This is to make sure that lifetime of opnd is virtually extended until next CALL instr.
            tmpInstr = IR::Instr::New(Js::OpCode::MOVSD, tempRegOpnd, src, this->m_func);
            nextInstr->InsertBefore(tmpInstr);

            // This saves the result in the same register.
            this->GenerateFloatAbs(static_cast<IR::RegOpnd*>(tempRegOpnd), nextInstr);

            // MOVSD dst, tempRegOpnd
            tmpInstr = IR::Instr::New(Js::OpCode::MOVSD, dst, tempRegOpnd, this->m_func);
            nextInstr->InsertBefore(tmpInstr);
        }
        else
        {
            // MOVSD dst, src
            tmpInstr = IR::Instr::New(Js::OpCode::MOVSD, dst, src, this->m_func);
            nextInstr->InsertBefore(tmpInstr);

            // This saves the result in the same register.
            this->GenerateFloatAbs(static_cast<IR::RegOpnd*>(dst), nextInstr);
        }
    }
    else if (srcType == IRType::TyFloat32)
    {
        if (!dst->IsRegOpnd())
        {
            // MOVSS tempRegOpnd, src
            IR::RegOpnd* tempRegOpnd = IR::RegOpnd::New(nullptr, TyFloat32, this->m_func);
            tempRegOpnd->m_isCallArg = true; // This is to make sure that lifetime of opnd is virtually extended until next CALL instr.
            tmpInstr = IR::Instr::New(Js::OpCode::MOVSS, tempRegOpnd, src, this->m_func);
            nextInstr->InsertBefore(tmpInstr);

            // This saves the result in the same register.
            this->GenerateFloatAbs(static_cast<IR::RegOpnd*>(tempRegOpnd), nextInstr);

            // MOVSS dst, tempRegOpnd
            tmpInstr = IR::Instr::New(Js::OpCode::MOVSS, dst, tempRegOpnd, this->m_func);
            nextInstr->InsertBefore(tmpInstr);
        }
        else
        {
            // MOVSS dst, src
            tmpInstr = IR::Instr::New(Js::OpCode::MOVSS, dst, src, this->m_func);
            nextInstr->InsertBefore(tmpInstr);

            // This saves the result in the same register.
            this->GenerateFloatAbs(static_cast<IR::RegOpnd*>(dst), nextInstr);
        }
    }
    else
    {
        AssertMsg(FALSE, "GenerateFastInlineBuiltInMathAbs: unexpected type of the src!");
    }
}

void
LowererMD::FinalLower()
{
    this->lowererMDArch.FinalLower();
}

IR::Instr *
LowererMD::LowerDivI4AndBailOnReminder(IR::Instr * instr, IR::LabelInstr * bailOutLabel)
{
    // Don't have save the operand for bailout because the lowering of IDIV don't overwrite their values

    //       (EDX) = CDQ
    //         EAX = numerator
    //    (EDX:EAX)= IDIV (EAX), denominator
    //               TEST EDX, EDX
    //               JNE bailout
    //               <Caller insert more checks here>
    //         dst = MOV EAX                             <-- insertBeforeInstr

    Assert(instr);
    Assert(instr->m_opcode == Js::OpCode::Div_I4);
    Assert(!instr->HasBailOutInfo());

    EmitInt4Instr(instr);

    Assert(instr->m_opcode == Js::OpCode::IDIV);
    IR::Instr * prev = instr->m_prev;
    Assert(prev->m_opcode == Js::OpCode::CDQ);
#ifdef _M_IX86
    Assert(prev->GetDst()->AsRegOpnd()->GetReg() == RegEDX);
#else
    Assert(prev->GetDst()->AsRegOpnd()->GetReg() == RegRDX);
#endif
    IR::Opnd * reminderOpnd = prev->GetDst();

    // Insert all check before the assignment to the actual dst.
    IR::Instr * insertBeforeInstr = instr->m_next;
    Assert(insertBeforeInstr->m_opcode == Js::OpCode::MOV);
#ifdef _M_IX86
    Assert(insertBeforeInstr->GetSrc1()->AsRegOpnd()->GetReg() == RegEAX);
#else
    Assert(insertBeforeInstr->GetSrc1()->AsRegOpnd()->GetReg() == RegRAX);
#endif
    // Jump to bailout if the reminder is not 0 (not int result)
    this->m_lowerer->InsertTestBranch(reminderOpnd, reminderOpnd, Js::OpCode::BrNeq_A, bailOutLabel, insertBeforeInstr);
    return insertBeforeInstr;
}
