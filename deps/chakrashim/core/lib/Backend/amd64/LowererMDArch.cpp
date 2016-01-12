//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "BackEnd.h"
#include "LowererMDArch.h"
#include "Library\JavascriptGeneratorFunction.h"

const Js::OpCode LowererMD::MDExtend32Opcode = Js::OpCode::MOVSXD;

extern const IRType RegTypes[RegNumCount];

BYTE
LowererMDArch::GetDefaultIndirScale()
{
    return IndirScale8;
}

RegNum
LowererMDArch::GetRegShiftCount()
{
    return RegRCX;
}

RegNum
LowererMDArch::GetRegReturn(IRType type)
{
    return ( IRType_IsFloat(type) || IRType_IsSimd128(type) ) ? RegXMM0 : RegRAX;
}

RegNum
LowererMDArch::GetRegReturnAsmJs(IRType type)
{
    if (IRType_IsFloat(type))
    {
        return RegXMM0;
    }
    else if (IRType_IsSimd128(type))
    {
        return RegXMM0;
    }
    else
    {
        return RegRAX;
    }
}

RegNum
LowererMDArch::GetRegStackPointer()
{
    return RegRSP;
}

RegNum
LowererMDArch::GetRegBlockPointer()
{
    return RegRBP;
}

RegNum
LowererMDArch::GetRegFramePointer()
{
    return RegRBP;
}

RegNum
LowererMDArch::GetRegChkStkParam()
{
    return RegRAX;
}

RegNum
LowererMDArch::GetRegIMulDestLower()
{
    return RegRAX;
}

RegNum
LowererMDArch::GetRegIMulHighDestLower()
{
    return RegRDX;
}
RegNum
LowererMDArch::GetRegArgI4(int32 argNum)
{
    // TODO: decide on registers to use for int
    return RegNOREG;
}

RegNum
LowererMDArch::GetRegArgR8(int32 argNum)
{
    // TODO: decide on registers to use for double
    return RegNOREG;
}

Js::OpCode
LowererMDArch::GetAssignOp(IRType type)
{
    switch (type)
    {
    case TyFloat64:
        return Js::OpCode::MOVSD;
    case TyFloat32:
        return Js::OpCode::MOVSS;
    case TySimd128F4:
    case TySimd128I4:
    case TySimd128D2:
        return Js::OpCode::MOVUPS;
    default:
        return Js::OpCode::MOV;
    }
}

void
LowererMDArch::Init(LowererMD *lowererMD)
{
    this->lowererMD                 = lowererMD;
    this->helperCallArgsCount       = 0;
}

///----------------------------------------------------------------------------
///
/// LowererMD::LoadInputParamPtr
///
///     Load the address of the start of the passed-in parameters not including
///     the this parameter.
///
///----------------------------------------------------------------------------

IR::Instr *
LowererMDArch::LoadInputParamPtr(IR::Instr *instrInsert, IR::RegOpnd *optionalDstOpnd /* = nullptr */)
{
    if (this->m_func->GetJnFunction()->IsGenerator())
    {
        IR::RegOpnd * argPtrRegOpnd = Lowerer::LoadGeneratorArgsPtr(instrInsert);
        IR::IndirOpnd * indirOpnd = IR::IndirOpnd::New(argPtrRegOpnd, 1 * MachPtr, TyMachPtr, this->m_func);
        IR::RegOpnd * dstOpnd = optionalDstOpnd != nullptr ? optionalDstOpnd : IR::RegOpnd::New(TyMachPtr, this->m_func);

        return Lowerer::InsertLea(dstOpnd, indirOpnd, instrInsert);
    }
    else
    {
        // Stack looks like (EBP chain)+0, (return addr)+4, (function object)+8, (arg count)+12, (this)+16, actual args
        StackSym *paramSym = StackSym::New(TyMachReg, this->m_func);
        this->m_func->SetArgOffset(paramSym, 5 * MachPtr);
        IR::Instr *instr = this->lowererMD->LoadStackAddress(paramSym, optionalDstOpnd);
        instrInsert->InsertBefore(instr);
        return instr;
    }
}


IR::Instr *
LowererMDArch::LoadStackArgPtr(IR::Instr * instrArgPtr)
{
    // Get the args pointer relative to the frame pointer.
    // NOTE: This code is sufficient for the apply-args optimization, but not for StackArguments,
    // if and when that is enabled.

    // dst = LEA &[rbp + "this" offset + sizeof(var)]

    IR::Instr * instr = LoadInputParamPtr(instrArgPtr, instrArgPtr->UnlinkDst()->AsRegOpnd());
    instrArgPtr->Remove();

    return instr->m_prev;
}

IR::Instr *
LowererMDArch::LoadHeapArgsCached(IR::Instr *instrArgs)
{
    // s7 = formals are let decls
    // s6 = memory context
    // s5 = local frame instance
    // s4 = address of first actual argument (after "this")
    // s3 = formal argument count
    // s2 = actual argument count
    // s1 = current function
    // dst = JavascriptOperators::LoadArguments(s1, s2, s3, s4, s5, s6, s7)

    ASSERT_INLINEE_FUNC(instrArgs);
    Func *func = instrArgs->m_func;
    IR::Instr *instrPrev = instrArgs->m_prev;

    // s7 = formals are let decls
    IR::Opnd * formalsAreLetDecls = IR::IntConstOpnd::New((IntConstType)(instrArgs->m_opcode == Js::OpCode::LdLetHeapArgsCached), TyUint8, func);
    this->LoadHelperArgument(instrArgs, formalsAreLetDecls);

    // s6 = memory context
    this->lowererMD->m_lowerer->LoadScriptContext(instrArgs);

    // s5 = local frame instance
    IR::Opnd *frameObj = instrArgs->UnlinkSrc1();
    this->LoadHelperArgument(instrArgs, frameObj);

    if (func->IsInlinee())
    {
        // s4 = address of first actual argument (after "this").
        StackSym *firstRealArgSlotSym = func->GetInlineeArgvSlotOpnd()->m_sym->AsStackSym();
        this->m_func->SetArgOffset(firstRealArgSlotSym, firstRealArgSlotSym->m_offset + MachPtr);
        IR::Instr *instr = this->lowererMD->LoadStackAddress(firstRealArgSlotSym);
        instrArgs->InsertBefore(instr);
        this->LoadHelperArgument(instrArgs, instr->GetDst());

        // s3 = formal argument count (without counting "this").
        uint32 formalsCount = func->GetJnFunction()->GetInParamsCount() - 1;
        this->LoadHelperArgument(instrArgs, IR::IntConstOpnd::New(formalsCount, TyUint32, func));

        // s2 = actual argument count (without counting "this").
        instr = IR::Instr::New(Js::OpCode::MOV,
                               IR::RegOpnd::New(TyMachReg, func),
                               IR::IntConstOpnd::New(func->actualCount - 1, TyUint32, func),
                               func);
        instrArgs->InsertBefore(instr);
        this->LoadHelperArgument(instrArgs, instr->GetDst());

        // s1 = current function.
        this->LoadHelperArgument(instrArgs, func->GetInlineeFunctionObjectSlotOpnd());

        // Save the newly-created args object to its dedicated stack slot.
        IR::SymOpnd *argObjSlotOpnd = func->GetInlineeArgumentsObjectSlotOpnd();
        instr = IR::Instr::New(Js::OpCode::MOV,
                               argObjSlotOpnd,
                               instrArgs->GetDst(),
                               func);
        instrArgs->InsertAfter(instr);
    }
    else
    {
        // s4 = address of first actual argument (after "this")
        // Stack looks like (EBP chain)+0, (return addr)+4, (function object)+8, (arg count)+12, (this)+16, actual args
        IR::Instr *instr = this->LoadInputParamPtr(instrArgs);
        this->LoadHelperArgument(instrArgs, instr->GetDst());

        // s3 = formal argument count (without counting "this")
        uint32 formalsCount = func->GetInParamsCount() - 1;
        this->LoadHelperArgument(instrArgs, IR::IntConstOpnd::New(formalsCount, TyInt32, func));

        // s2 = actual argument count (without counting "this")
        instr = this->lowererMD->LoadInputParamCount(instrArgs);
        instr = IR::Instr::New(Js::OpCode::DEC, instr->GetDst(), instr->GetDst(), func);
        instrArgs->InsertBefore(instr);
        this->LoadHelperArgument(instrArgs, instr->GetDst());

        // s1 = current function
        StackSym *paramSym = StackSym::New(TyMachReg, func);
        this->m_func->SetArgOffset(paramSym, 2 * MachPtr);
        IR::Opnd * srcOpnd = IR::SymOpnd::New(paramSym, TyMachReg, func);
        this->LoadHelperArgument(instrArgs, srcOpnd);

        // Save the newly-created args object to its dedicated stack slot.
        IR::Opnd *opnd = this->lowererMD->CreateStackArgumentsSlotOpnd();
        instr = IR::Instr::New(Js::OpCode::MOV, opnd, instrArgs->GetDst(), func);
        instrArgs->InsertAfter(instr);
    }

    this->lowererMD->ChangeToHelperCall(instrArgs, IR::HelperOp_LoadHeapArgsCached);

    return instrPrev;
}

///----------------------------------------------------------------------------
///
/// LowererMDArch::LoadHeapArguments
///
///     Load the arguments object
///     NOTE: The same caveat regarding arguments passed on the stack applies here
///           as in LoadInputParamCount above.
///----------------------------------------------------------------------------

IR::Instr *
LowererMDArch::LoadHeapArguments(IR::Instr *instrArgs, bool force /* = false */, IR::Opnd *opndInputParamCount /* = nullptr */)
{
    ASSERT_INLINEE_FUNC(instrArgs);
    Func *func = instrArgs->m_func;

    IR::Instr *instrPrev = instrArgs->m_prev;
    if (!force && func->GetHasStackArgs() && this->m_func->GetHasStackArgs())
    {
        // The initial args slot value is zero. (TODO: it should be possible to dead-store the LdHeapArgs in this case.)
        instrArgs->m_opcode = Js::OpCode::MOV;
        instrArgs->ReplaceSrc1(IR::AddrOpnd::NewNull(func));
        instrArgs->FreeSrc2();
    }
    else
    {
        // s7 = formals are let decls
        // s6 = memory context
        // s5 = array of property ID's
        // s4 = local frame instance
        // s3 = address of first actual argument (after "this")
        // s2 = actual argument count
        // s1 = current function
        // dst = JavascriptOperators::LoadHeapArguments(s1, s2, s3, s4, s5, s6, s7)

        // s7 = formals are let decls
        this->LoadHelperArgument(instrArgs, IR::IntConstOpnd::New(instrArgs->m_opcode == Js::OpCode::LdLetHeapArguments ? TRUE : FALSE, TyUint8, func));

        // s6 = memory context
        instrPrev = this->lowererMD->m_lowerer->LoadScriptContext(instrArgs);

        // s5 = array of property ID's
        IR::Opnd *argArray = instrArgs->UnlinkSrc2();
        this->LoadHelperArgument(instrArgs, argArray);

        // s4 = local frame instance
        IR::Opnd *frameObj = instrArgs->UnlinkSrc1();
        this->LoadHelperArgument(instrArgs, frameObj);

        if (func->IsInlinee())
        {
            // s3 = address of first actual argument (after "this").
            StackSym *firstRealArgSlotSym = func->GetInlineeArgvSlotOpnd()->m_sym->AsStackSym();
            this->m_func->SetArgOffset(firstRealArgSlotSym, firstRealArgSlotSym->m_offset + MachPtr);
            IR::Instr *instr = this->lowererMD->LoadStackAddress(firstRealArgSlotSym);
            instrArgs->InsertBefore(instr);
            this->LoadHelperArgument(instrArgs, instr->GetDst());

            // s2 = actual argument count (without counting "this").
            instr = IR::Instr::New(Js::OpCode::MOV,
                                   IR::RegOpnd::New(TyUint32, func),
                                   IR::IntConstOpnd::New(func->actualCount - 1, TyUint32, func),
                                   func);
            instrArgs->InsertBefore(instr);
            this->LoadHelperArgument(instrArgs, instr->GetDst());

            // s1 = current function.
            this->LoadHelperArgument(instrArgs, func->GetInlineeFunctionObjectSlotOpnd());

            // Save the newly-created args object to its dedicated stack slot.
            IR::SymOpnd *argObjSlotOpnd = func->GetInlineeArgumentsObjectSlotOpnd();
            instr = IR::Instr::New(Js::OpCode::MOV,
                                   argObjSlotOpnd,
                                   instrArgs->GetDst(),
                                   func);
            instrArgs->InsertAfter(instr);
        }
        else
        {
            // s3 = address of first actual argument (after "this")
            // Stack looks like (EBP chain)+0, (return addr)+4, (function object)+8, (arg count)+12, (this)+16, actual args
            IR::Instr *instr = this->LoadInputParamPtr(instrArgs);
            this->LoadHelperArgument(instrArgs, instr->GetDst());

            // s2 = actual argument count (without counting "this")
            if (opndInputParamCount == nullptr)
            {
                instr = this->lowererMD->LoadInputParamCount(instrArgs, -1);
                opndInputParamCount = instr->GetDst();
            }
            this->LoadHelperArgument(instrArgs, opndInputParamCount);

            // s1 = current function
            StackSym * paramSym = StackSym::New(TyMachReg, func);
            this->m_func->SetArgOffset(paramSym, 2 * MachPtr);
            IR::Opnd * srcOpnd = IR::SymOpnd::New(paramSym, TyMachReg, func);

            if (this->m_func->GetJnFunction()->IsGenerator())
            {
                // the function object for generator calls is a GeneratorVirtualScriptFunction object
                // and we need to pass the real JavascriptGeneratorFunction object so grab it instead
                IR::RegOpnd *tmpOpnd = IR::RegOpnd::New(TyMachReg, func);
                LowererMD::CreateAssign(tmpOpnd, srcOpnd, instrArgs);

                srcOpnd = IR::IndirOpnd::New(tmpOpnd, Js::GeneratorVirtualScriptFunction::GetRealFunctionOffset(), TyMachPtr, func);
            }

            this->LoadHelperArgument(instrArgs, srcOpnd);

            // Save the newly-created args object to its dedicated stack slot.
            IR::Opnd *opnd = this->lowererMD->CreateStackArgumentsSlotOpnd();
            instr = IR::Instr::New(Js::OpCode::MOV, opnd, instrArgs->GetDst(), func);
            instrArgs->InsertAfter(instr);
        }

        this->lowererMD->ChangeToHelperCall(instrArgs, IR::HelperOp_LoadHeapArguments);
    }

    return instrPrev;
}

///----------------------------------------------------------------------------
///
/// LowererMDArch::LoadFuncExpression
///
///     Load the function expression to src1 from [ebp + 8]
///
///----------------------------------------------------------------------------

IR::Instr *
LowererMDArch::LoadFuncExpression(IR::Instr *instrFuncExpr)
{
    ASSERT_INLINEE_FUNC(instrFuncExpr);
    Func *func = instrFuncExpr->m_func;

    IR::Opnd *paramOpnd = nullptr;
    if (func->IsInlinee())
    {
        paramOpnd = func->GetInlineeFunctionObjectSlotOpnd();
    }
    else
    {
        StackSym *paramSym = StackSym::New(TyMachReg, this->m_func);
        this->m_func->SetArgOffset(paramSym, 2 * MachPtr);
        paramOpnd = IR::SymOpnd::New(paramSym, TyMachReg, this->m_func);
    }

    if (this->m_func->GetJnFunction()->IsGenerator())
    {
        // the function object for generator calls is a GeneratorVirtualScriptFunction object
        // and we need to return the real JavascriptGeneratorFunction object so grab it before
        // assigning to the dst
        IR::RegOpnd *tmpOpnd = IR::RegOpnd::New(TyMachReg, func);
        LowererMD::CreateAssign(tmpOpnd, paramOpnd, instrFuncExpr);

        paramOpnd = IR::IndirOpnd::New(tmpOpnd, Js::GeneratorVirtualScriptFunction::GetRealFunctionOffset(), TyMachPtr, func);
    }

    // mov dst, param
    instrFuncExpr->SetSrc1(paramOpnd);
    LowererMD::ChangeToAssign(instrFuncExpr);

    return instrFuncExpr;
}

//
// Load the parameter in the first argument slot
//

IR::Instr *
LowererMDArch::LoadNewScObjFirstArg(IR::Instr * instr, IR::Opnd * dst, ushort extraArgs)
{
    // Spread moves down the argument slot by one.
    IR::Opnd *      argOpnd         = this->GetArgSlotOpnd(3 + extraArgs);
    IR::Instr *     argInstr        = LowererMD::CreateAssign(argOpnd, dst, instr);

    return argInstr;
}

int32
LowererMDArch::LowerCallArgs(IR::Instr *callInstr, ushort callFlags, Js::ArgSlot extraParams, IR::IntConstOpnd **callInfoOpndRef /* = nullptr */)
{
    AssertMsg(this->helperCallArgsCount == 0, "We don't support nested helper calls yet");

    const Js::ArgSlot       argOffset       = 1;
    uint32                  argCount        = 0;

    // Lower args and look for StartCall

    IR::Instr * argInstr = callInstr;
    IR::Instr * cfgInsertLoc = callInstr->GetPrevRealInstr();
    IR::Opnd *src2 = argInstr->UnlinkSrc2();
    while (src2->IsSymOpnd())
    {
        IR::SymOpnd *   argLinkOpnd = src2->AsSymOpnd();
        StackSym *      argLinkSym  = argLinkOpnd->m_sym->AsStackSym();
        AssertMsg(argLinkSym->IsArgSlotSym() && argLinkSym->m_isSingleDef, "Arg tree not single def...");
        argLinkOpnd->Free(this->m_func);

        argInstr = argLinkSym->m_instrDef;

        src2 = argInstr->UnlinkSrc2();
        this->lowererMD->ChangeToAssign(argInstr);

        // Mov each arg to its argSlot

        Js::ArgSlot     argPosition = argInstr->GetDst()->AsSymOpnd()->m_sym->AsStackSym()->GetArgSlotNum();
        Js::ArgSlot     index       = argOffset + argPosition;
        if(index < argPosition)
        {
            Js::Throw::OutOfMemory();
        }
        index += extraParams;
        if(index < extraParams)
        {
            Js::Throw::OutOfMemory();
        }

        IR::Opnd *      dstOpnd     = this->GetArgSlotOpnd(index, argLinkSym);

        argInstr->ReplaceDst(dstOpnd);

        cfgInsertLoc = argInstr->GetPrevRealInstr();

        // The arg sym isn't assigned a constant directly anymore
        // TODO: We can just move the instruction down next to the call if it is just an constant assignment
        // but AMD64 doesn't have the MOV mem,imm64 encoding, and we have no code to detect if the value can fit
        // into imm32 and hoist the src if it is not.
        argLinkSym->m_isConst = false;
        argLinkSym->m_isIntConst = false;
        argLinkSym->m_isTaggableIntConst = false;

        argInstr->Unlink();
        callInstr->InsertBefore(argInstr);
        argCount++;
    }


    IR::RegOpnd *       argLinkOpnd = src2->AsRegOpnd();
    StackSym *          argLinkSym  = argLinkOpnd->m_sym->AsStackSym();
    AssertMsg(!argLinkSym->IsArgSlotSym() && argLinkSym->m_isSingleDef, "Arg tree not single def...");

    IR::Instr *startCallInstr = argLinkSym->m_instrDef;

    if (callInstr->m_opcode == Js::OpCode::NewScObject ||
        callInstr->m_opcode == Js::OpCode::NewScObjectSpread ||
        callInstr->m_opcode == Js::OpCode::NewScObjectLiteral ||
        callInstr->m_opcode == Js::OpCode::NewScObjArray ||
        callInstr->m_opcode == Js::OpCode::NewScObjArraySpread)
    {
        // These push an extra arg.
        argCount++;
    }

    AssertMsg(startCallInstr->m_opcode == Js::OpCode::StartCall ||
              startCallInstr->m_opcode == Js::OpCode::LoweredStartCall ||
              startCallInstr->m_opcode == Js::OpCode::StartCallAsmJsE ||
              startCallInstr->m_opcode == Js::OpCode::StartCallAsmJsI,
              "Problem with arg chain.");
    AssertMsg(startCallInstr->GetArgOutCount(/*getInterpreterArgOutCount*/ false) == argCount ||
              m_func->GetJnFunction()->GetIsAsmjsMode(),
        "ArgCount doesn't match StartCall count");
    //
    // Machine dependent lowering
    //

    if (callInstr->m_opcode != Js::OpCode::AsmJsCallI)
    {
        // Push argCount
        IR::IntConstOpnd *argCountOpnd = Lowerer::MakeCallInfoConst(callFlags, argCount, m_func);
        if (callInfoOpndRef)
        {
            argCountOpnd->Use(m_func);
            *callInfoOpndRef = argCountOpnd;
        }
        Lowerer::InsertMove(this->GetArgSlotOpnd(1 + extraParams), argCountOpnd, callInstr);
    }
    startCallInstr = this->LowerStartCall(startCallInstr);

    const uint32 argSlots = argCount + 1 + extraParams; // + 1 for call flags
    this->m_func->m_argSlotsForFunctionsCalled = max(this->m_func->m_argSlotsForFunctionsCalled, argSlots);

    if (m_func->GetJnFunction()->GetIsAsmjsMode())
    {
        IR::Opnd * functionObjOpnd = callInstr->UnlinkSrc1();
        GeneratePreCall(callInstr, functionObjOpnd, cfgInsertLoc->GetNextRealInstr());
    }

    return argSlots;
}

void
LowererMDArch::SetMaxArgSlots(Js::ArgSlot actualCount /*including this*/)
{
    Js::ArgSlot offset = 3;//For function object & callInfo & this
    if (this->m_func->m_argSlotsForFunctionsCalled < (uint32) (actualCount + offset))
    {
        this->m_func->m_argSlotsForFunctionsCalled = (uint32)(actualCount + offset);
    }
    return;
}

IR::Instr *
LowererMDArch::LowerCallIDynamic(IR::Instr *callInstr, IR::Instr*saveThisArgOutInstr, IR::Opnd *argsLength, ushort callFlags, IR::Instr * insertBeforeInstrForCFG)
{
    callInstr->InsertBefore(saveThisArgOutInstr); //Move this Argout next to call;
    this->LoadDynamicArgument(saveThisArgOutInstr, 3); /*this pointer is the 3rd argument

    /*callInfo*/
    if (callInstr->m_func->IsInlinee())
    {
        Assert(argsLength->AsIntConstOpnd()->GetValue() == callInstr->m_func->actualCount);
        this->SetMaxArgSlots((Js::ArgSlot)callInstr->m_func->actualCount);
    }
    else
    {
        callInstr->InsertBefore(IR::Instr::New(Js::OpCode::ADD, argsLength, argsLength, IR::IntConstOpnd::New(1, TyInt8, this->m_func), this->m_func));
        this->SetMaxArgSlots(Js::InlineeCallInfo::MaxInlineeArgoutCount);
    }
    callInstr->InsertBefore(IR::Instr::New(Js::OpCode::MOV, this->GetArgSlotOpnd(2), argsLength, this->m_func));

    IR::Opnd    *funcObjOpnd = callInstr->UnlinkSrc1();
    GeneratePreCall(callInstr, funcObjOpnd, insertBeforeInstrForCFG);
    LowerCall(callInstr, 0);

    return callInstr;
}

void
LowererMDArch::GenerateFunctionObjectTest(IR::Instr * callInstr, IR::RegOpnd  *functionObjOpnd, bool isHelper, IR::LabelInstr* continueAfterExLabel /* = nullptr */)
{
    AssertMsg(!m_func->IsJitInDebugMode() || continueAfterExLabel, "When jit is in debug mode, continueAfterExLabel must be provided otherwise continue after exception may cause AV.");

    IR::RegOpnd *functionObjRegOpnd = functionObjOpnd->AsRegOpnd();
    IR::Instr * insertBeforeInstr = callInstr;

    // Need check and error if we are calling a tagged int.
    if (!functionObjRegOpnd->IsNotTaggedValue())
    {
        IR::LabelInstr * helperLabel = IR::LabelInstr::New(Js::OpCode::Label, this->m_func, true);
        if (this->lowererMD->GenerateObjectTest(functionObjRegOpnd, callInstr, helperLabel))
        {

            IR::LabelInstr * callLabel = IR::LabelInstr::New(Js::OpCode::Label, this->m_func, isHelper);
            IR::Instr* instr = IR::BranchInstr::New(Js::OpCode::JMP, callLabel, this->m_func);
            callInstr->InsertBefore(instr);

            callInstr->InsertBefore(helperLabel);
            callInstr->InsertBefore(callLabel);

            insertBeforeInstr = callLabel;
            lowererMD->m_lowerer->GenerateRuntimeError(insertBeforeInstr, JSERR_NeedFunction);

            if (continueAfterExLabel)
            {
                // Under debugger the RuntimeError (exception) can be ignored, generate branch to jmp to safe place
                // (which would normally be debugger bailout check).
                IR::BranchInstr* continueAfterEx = IR::BranchInstr::New(LowererMD::MDUncondBranchOpcode, continueAfterExLabel, this->m_func);
                insertBeforeInstr->InsertBefore(continueAfterEx);
            }
        }
    }
}

void
LowererMDArch::GeneratePreCall(IR::Instr * callInstr, IR::Opnd  *functionObjOpnd, IR::Instr * insertBeforeInstrForCFGCheck)
{
    if (insertBeforeInstrForCFGCheck == nullptr)
    {
        insertBeforeInstrForCFGCheck = callInstr;
    }

    IR::RegOpnd * functionTypeRegOpnd = nullptr;
    IR::IndirOpnd * entryPointIndirOpnd = nullptr;
    if (callInstr->m_opcode == Js::OpCode::AsmJsCallI)
    {
        functionTypeRegOpnd = IR::RegOpnd::New(TyMachReg, m_func);

        IR::IndirOpnd* functionInfoIndirOpnd = IR::IndirOpnd::New(functionObjOpnd->AsRegOpnd(), Js::RecyclableObject::GetTypeOffset(), TyMachReg, m_func);

        IR::Instr* instr = IR::Instr::New(Js::OpCode::MOV, functionTypeRegOpnd, functionInfoIndirOpnd, m_func);

        insertBeforeInstrForCFGCheck->InsertBefore(instr);

        functionInfoIndirOpnd = IR::IndirOpnd::New(functionTypeRegOpnd, Js::ScriptFunctionType::GetEntryPointInfoOffset(), TyMachReg, m_func);
        instr = IR::Instr::New(Js::OpCode::MOV, functionTypeRegOpnd, functionInfoIndirOpnd, m_func);
        insertBeforeInstrForCFGCheck->InsertBefore(instr);

        uint32 entryPointOffset = Js::ProxyEntryPointInfo::GetAddressOffset();

        entryPointIndirOpnd = IR::IndirOpnd::New(functionTypeRegOpnd, entryPointOffset, TyMachReg, m_func);
    }
    else
    {
        // For calls to fixed functions we load the function's type directly from the known (hard-coded) function object address.
        // For other calls, we need to load it from the function object stored in a register operand.
        if (functionObjOpnd->IsAddrOpnd() && functionObjOpnd->AsAddrOpnd()->m_isFunction)
        {
            functionTypeRegOpnd = this->lowererMD->m_lowerer->GenerateFunctionTypeFromFixedFunctionObject(insertBeforeInstrForCFGCheck, functionObjOpnd);
        }
        else if (functionObjOpnd->IsRegOpnd())
        {
            AssertMsg(functionObjOpnd->AsRegOpnd()->m_sym->IsStackSym(), "Expected call target to be a stack symbol.");

            functionTypeRegOpnd = IR::RegOpnd::New(TyMachReg, m_func);
            // functionTypeRegOpnd(RAX) = MOV function->type
            {
                IR::IndirOpnd * functionTypeIndirOpnd = IR::IndirOpnd::New(functionObjOpnd->AsRegOpnd(),
                    Js::DynamicObject::GetOffsetOfType(), TyMachReg, m_func);
                IR::Instr * mov = IR::Instr::New(Js::OpCode::MOV, functionTypeRegOpnd, functionTypeIndirOpnd, m_func);
                insertBeforeInstrForCFGCheck->InsertBefore(mov);
            }
        }
        else
        {
            AnalysisAssertMsg(false, "Unexpected call target operand type.");
        }
        // entryPointRegOpnd(RAX) = MOV type->entryPoint
        entryPointIndirOpnd = IR::IndirOpnd::New(functionTypeRegOpnd, Js::Type::GetOffsetOfEntryPoint(), TyMachPtr, m_func);
    }

    IR::RegOpnd *entryPointRegOpnd = functionTypeRegOpnd;
    entryPointRegOpnd->m_isCallArg = true;

    IR::Instr *mov = IR::Instr::New(Js::OpCode::MOV, entryPointRegOpnd, entryPointIndirOpnd, m_func);
    insertBeforeInstrForCFGCheck->InsertBefore(mov);

    // entryPointRegOpnd(RAX) = CALL entryPointRegOpnd(RAX)
    callInstr->SetSrc1(entryPointRegOpnd);

#if defined(_CONTROL_FLOW_GUARD)
    // verify that the call target is valid (CFG Check)
    if (!PHASE_OFF(Js::CFGInJitPhase, this->m_func))
    {
        this->lowererMD->GenerateCFGCheck(entryPointRegOpnd, insertBeforeInstrForCFGCheck);
    }
#endif

    // Setup the first call argument - pointer to the function being called.
    {
        IR::Instr *mov = IR::Instr::New(Js::OpCode::MOV, GetArgSlotOpnd(1), functionObjOpnd, m_func);
        callInstr->InsertBefore(mov);
    }
}

IR::Instr *
LowererMDArch::LowerCallI(IR::Instr * callInstr, ushort callFlags, bool isHelper, IR::Instr * insertBeforeInstrForCFG)
{
    AssertMsg(this->helperCallArgsCount == 0, "We don't support nested helper calls yet");

    IR::Opnd * functionObjOpnd = callInstr->UnlinkSrc1();
    IR::Instr * insertBeforeInstrForCFGCheck = callInstr;

    // If this is a call for new, we already pass the function operand through NewScObject,
    // which checks if the function operand is a real function or not, don't need to add a check again
    // If this is a call to a fixed function, we've already verified that the target is, indeed, a function.
    if (callInstr->m_opcode != Js::OpCode::CallIFixed && !(callFlags & Js::CallFlags_New))
    {
        Assert(functionObjOpnd->IsRegOpnd());
        IR::LabelInstr* continueAfterExLabel = Lowerer::InsertContinueAfterExceptionLabelForDebugger(m_func, callInstr, isHelper);
        GenerateFunctionObjectTest(callInstr, functionObjOpnd->AsRegOpnd(), isHelper, continueAfterExLabel);
    }
    else if (insertBeforeInstrForCFG != nullptr)
    {
        RegNum dstReg = insertBeforeInstrForCFG->GetDst()->AsRegOpnd()->GetReg();
        AssertMsg(dstReg == RegR8 || dstReg == RegR9, "NewScObject should insert the first Argument in R8/R9 only based on Spread call or not.");
        insertBeforeInstrForCFGCheck = insertBeforeInstrForCFG;
    }

    GeneratePreCall(callInstr, functionObjOpnd, insertBeforeInstrForCFGCheck);

    // We need to get the calculated CallInfo in SimpleJit because that doesn't include any changes for stack alignment
    IR::IntConstOpnd *callInfo;
    int32 argCount = LowerCallArgs(callInstr, callFlags, 1, &callInfo);


    IR::Opnd *const finalDst = callInstr->GetDst();

    // x64 keeps track of argCount for us, so pass just an arbitrary value there
    IR::Instr* ret = this->LowerCall(callInstr, argCount);

    IR::AutoReuseOpnd autoReuseSavedFunctionObjOpnd;
    if (callInstr->IsJitProfilingInstr())
    {
        Assert(callInstr->m_func->IsSimpleJit());
        Assert(!Js::FunctionBody::IsNewSimpleJit());

        if(finalDst &&
            finalDst->IsRegOpnd() &&
            functionObjOpnd->IsRegOpnd() &&
            finalDst->AsRegOpnd()->m_sym == functionObjOpnd->AsRegOpnd()->m_sym)
        {
            // The function object sym is going to be overwritten, so save it in a temp for profiling
            IR::RegOpnd *const savedFunctionObjOpnd = IR::RegOpnd::New(functionObjOpnd->GetType(), callInstr->m_func);
            autoReuseSavedFunctionObjOpnd.Initialize(savedFunctionObjOpnd, callInstr->m_func);
            Lowerer::InsertMove(savedFunctionObjOpnd, functionObjOpnd, callInstr->m_next);
            functionObjOpnd = savedFunctionObjOpnd;
        }

        auto instr = callInstr->AsJitProfilingInstr();
        ret = this->lowererMD->m_lowerer->GenerateCallProfiling(
            instr->profileId,
            instr->inlineCacheIndex,
            instr->GetDst(),
            functionObjOpnd,
            callInfo,
            instr->isProfiledReturnCall,
            callInstr,
            ret);
    }
    return ret;
}

IR::Instr *
LowererMDArch::LowerCallPut(IR::Instr *callInstr)
{
    // Note: what we have to do here is call a helper with the Jscript calling convention,
    // so we need to factor the lowering of arguments out of the CallI expansion.

    AssertMsg(FALSE, "TODO: LowerCallPut not implemented");
    return nullptr;
}

IR::Instr *
LowererMDArch::LowerCall(IR::Instr * callInstr, uint32 argCount)
{
    IR::Instr *retInstr = callInstr;
    callInstr->m_opcode = Js::OpCode::CALL;

    if (callInstr->GetDst())
    {
        IR::Opnd *       dstOpnd;

        this->lowererMD->ForceDstToReg(callInstr);
        dstOpnd = callInstr->GetDst();
        IRType dstType = dstOpnd->GetType();
        Js::OpCode       assignOp = GetAssignOp(dstType);

        if (callInstr->GetSrc1()->IsHelperCallOpnd())
        {
            // Truncate the result of a conversion to 32-bit int, because the C++ code doesn't.
            IR::HelperCallOpnd *helperOpnd = callInstr->GetSrc1()->AsHelperCallOpnd();
            if (helperOpnd->m_fnHelper == IR::HelperConv_ToInt32 ||
                helperOpnd->m_fnHelper == IR::HelperConv_ToInt32_Full ||
                helperOpnd->m_fnHelper == IR::HelperConv_ToInt32Core ||
                helperOpnd->m_fnHelper == IR::HelperConv_ToUInt32 ||
                helperOpnd->m_fnHelper == IR::HelperConv_ToUInt32_Full ||
                helperOpnd->m_fnHelper == IR::HelperConv_ToUInt32Core)
            {
                assignOp = Js::OpCode::MOV_TRUNC;
            }
        }

        IR::Instr * movInstr = callInstr->SinkDst(assignOp);
        RegNum      reg = GetRegReturn(dstType);

        callInstr->GetDst()->AsRegOpnd()->SetReg(reg);
        movInstr->GetSrc1()->AsRegOpnd()->SetReg(reg);
        retInstr = movInstr;
    }

    //
    // assign the arguments to appropriate positions
    //

    AssertMsg(this->helperCallArgsCount >= 0, "Fatal. helper call arguments ought to be positive");
    AssertMsg(this->helperCallArgsCount < 255, "Too many helper call arguments");

    uint16 argsLeft = static_cast<uint16>(this->helperCallArgsCount);

    while (argsLeft > 0)
    {
        IR::Opnd * helperSrc = this->helperCallArgs[this->helperCallArgsCount - argsLeft];
        StackSym * helperSym = m_func->m_symTable->GetArgSlotSym(static_cast<uint16>(argsLeft));
        helperSym->m_type = helperSrc->GetType();
        Lowerer::InsertMove(
            this->GetArgSlotOpnd(argsLeft, helperSym),
            helperSrc,
            callInstr);
        --argsLeft;
    }


    //
    // load the address into a register because we cannot directly access 64 bit constants
    // in CALL instruction. Non helper call methods will already be accessed indirectly.
    //
    // Skip this for bailout calls. The register allocator will lower that as appropriate, without affecting spill choices.
    //
    // Also skip this for relocatable helper calls. These will be turned into indirect
    // calls in lower.

    if (callInstr->GetSrc1()->IsHelperCallOpnd() && !callInstr->HasBailOutInfo())
    {
        IR::RegOpnd *targetOpnd = IR::RegOpnd::New(StackSym::New(TyMachPtr,m_func), RegRAX, TyMachPtr, this->m_func);
        IR::Instr   *movInstr   = IR::Instr::New(Js::OpCode::MOV, targetOpnd, callInstr->GetSrc1(),  this->m_func);
        targetOpnd->m_isCallArg = true;

        callInstr->UnlinkSrc1();
        callInstr->SetSrc1(targetOpnd);
        callInstr->InsertBefore(movInstr);
    }

    //
    // Reset the call
    //

    this->m_func->m_argSlotsForFunctionsCalled  = max(this->m_func->m_argSlotsForFunctionsCalled , (uint32)this->helperCallArgsCount);
    this->helperCallArgsCount = 0;

    return retInstr;
}

//
// Returns the opnd where the corresponding argument would have been stored. On amd64,
// the first 4 arguments go in registers and the rest are on stack.
//
IR::Opnd *
LowererMDArch::GetArgSlotOpnd(uint16 index, StackSym * argSym)
{
    Assert(index != 0);

    uint16 argPosition = index;

    // Without SIMD the index is the Var offset and is also the argument index. Since each arg = 1 Var.
    // With SIMD, args are of variable length and we need to the argument position in the args list.
    if (m_func->GetScriptContext()->GetConfig()->IsSimdjsEnabled() &&
        m_func->GetJnFunction()->GetIsAsmJsFunction() &&
        argSym != nullptr &&
        argSym->m_argPosition != 0)
    {
        argPosition = (uint16)argSym->m_argPosition;
    }

    IR::Opnd *argSlotOpnd = nullptr;

    if (argSym != nullptr)
    {
        argSym->m_offset = (index - 1) * MachPtr;
        argSym->m_allocated = true;
    }

    IRType type = argSym ? argSym->GetType() : TyMachReg;
    if (argPosition <= 4)
    {
        RegNum reg = RegNOREG;

        if (IRType_IsFloat(type) || IRType_IsSimd128(type))
        {
            switch (argPosition)
            {
            case 4:
                reg = RegXMM3;
                break;
            case 3:
                reg = RegXMM2;
                break;
            case 2:
                reg = RegXMM1;
                break;
            case 1:
                reg = RegXMM0;
                break;
            default:
                Assume(UNREACHED);
            }
        }
        else
        {
            switch (argPosition)
            {
            case 4:
                reg = RegR9;
                break;
            case 3:
                reg = RegR8;
                break;
            case 2:
                reg = RegRDX;
                break;
            case 1:
                reg = RegRCX;
                break;
            default:
                Assume(UNREACHED);
            }
        }

        IR::RegOpnd *regOpnd = IR::RegOpnd::New(argSym, reg, type, m_func);
        regOpnd->m_isCallArg = true;

        argSlotOpnd = regOpnd;
    }
    else
    {
        if (argSym == nullptr)
        {
            argSym = this->m_func->m_symTable->GetArgSlotSym(static_cast<uint16>(index));
        }

        //
        // More than 4 arguments. Assign them to appropriate slots
        //

        argSlotOpnd = IR::SymOpnd::New(argSym, type, this->m_func);
    }

    return argSlotOpnd;
}

IR::Instr *
LowererMDArch::LowerAsmJsCallE(IR::Instr *callInstr)
{
    IR::IntConstOpnd *callInfo;
    int32 argCount = this->LowerCallArgs(callInstr, Js::CallFlags_Value, 1, &callInfo);

    IR::Instr* ret = this->LowerCall(callInstr, argCount);

    return ret;
}

IR::Instr *
LowererMDArch::LowerAsmJsCallI(IR::Instr * callInstr)
{
    int32 argCount = this->LowerCallArgs(callInstr, Js::CallFlags_Value, 0);

    IR::Instr* ret = this->LowerCall(callInstr, argCount);

    return ret;
}

IR::Instr*
LowererMDArch::LowerAsmJsLdElemHelper(IR::Instr * instr, bool isSimdLoad /*= false*/, bool checkEndOffset /*= false*/)
{
    IR::Instr* done;
    IR::Opnd * src1 = instr->UnlinkSrc1();
    IR::RegOpnd * indexOpnd = src1->AsIndirOpnd()->GetIndexOpnd();
    const uint8 dataWidth = instr->dataWidth;

    Assert(isSimdLoad == false || dataWidth == 4 || dataWidth == 8 || dataWidth == 12 || dataWidth == 16);

    // For x64, bound checks are required only for SIMD loads.
    if (isSimdLoad)
    {
        IR::LabelInstr * helperLabel = Lowerer::InsertLabel(true, instr);
        IR::LabelInstr * loadLabel = Lowerer::InsertLabel(false, instr);
        IR::LabelInstr * doneLabel = Lowerer::InsertLabel(false, instr);
        IR::Opnd *cmpOpnd;
        if (indexOpnd)
        {
            cmpOpnd = indexOpnd;
        }
        else
        {
            cmpOpnd = IR::IntConstOpnd::New(src1->AsIndirOpnd()->GetOffset(), TyUint32, m_func);
        }

        // if dataWidth != byte per element, we need to check end offset
        if (checkEndOffset)
        {
            IR::RegOpnd *tmp = IR::RegOpnd::New(cmpOpnd->GetType(), m_func);
            // MOV tmp, cmpOnd
            Lowerer::InsertMove(tmp, cmpOpnd, helperLabel);
            // ADD tmp, dataWidth
            Lowerer::InsertAdd(false, tmp, tmp, IR::IntConstOpnd::New((uint32)dataWidth, TyInt8, m_func, true), helperLabel);
            // CMP tmp, size
            // JG  $helper
            lowererMD->m_lowerer->InsertCompareBranch(tmp, instr->UnlinkSrc2(), Js::OpCode::BrGt_A, true, helperLabel, helperLabel);
        }
        else
        {
            lowererMD->m_lowerer->InsertCompareBranch(cmpOpnd, instr->UnlinkSrc2(), Js::OpCode::BrGe_A, true, helperLabel, helperLabel);
        }
        Lowerer::InsertBranch(Js::OpCode::Br, loadLabel, helperLabel);

        lowererMD->m_lowerer->GenerateRuntimeError(loadLabel, JSERR_ArgumentOutOfRange, IR::HelperOp_RuntimeRangeError);

        Lowerer::InsertBranch(Js::OpCode::Br, doneLabel, loadLabel);
        done = doneLabel;
    }
    else
    {
        Assert(!instr->GetSrc2());
        done = instr;
    }
    return done;
}

IR::Instr*
LowererMDArch::LowerAsmJsStElemHelper(IR::Instr * instr, bool isSimdStore /*= false*/, bool checkEndOffset /*= false*/)
{
    IR::Instr* done;
    IR::Opnd * dst = instr->UnlinkDst();
    IR::RegOpnd * indexOpnd = dst->AsIndirOpnd()->GetIndexOpnd();
    const uint8 dataWidth = instr->dataWidth;

    Assert(isSimdStore == false || dataWidth == 4 || dataWidth == 8 || dataWidth == 12 || dataWidth == 16);

    // For x64, bound checks are required only for SIMD loads.
    if (isSimdStore)
    {
        IR::LabelInstr * helperLabel = Lowerer::InsertLabel(true, instr);
        IR::LabelInstr * storeLabel = Lowerer::InsertLabel(false, instr);
        IR::LabelInstr * doneLabel = Lowerer::InsertLabel(false, instr);
        IR::Opnd * cmpOpnd;
        if (indexOpnd)
        {
            cmpOpnd = dst->AsIndirOpnd()->GetIndexOpnd();
        }
        else
        {
            cmpOpnd = IR::IntConstOpnd::New(dst->AsIndirOpnd()->GetOffset(), TyUint32, m_func);
        }

        // if dataWidth != byte per element, we need to check end offset
        if (checkEndOffset)
        {
            IR::RegOpnd *tmp = IR::RegOpnd::New(cmpOpnd->GetType(), m_func);
            // MOV tmp, cmpOnd
            Lowerer::InsertMove(tmp, cmpOpnd, helperLabel);
            // ADD tmp, dataWidth
            Lowerer::InsertAdd(false, tmp, tmp, IR::IntConstOpnd::New((uint32)dataWidth, TyInt8, m_func, true), helperLabel);
            // CMP tmp, size
            // JG  $helper
            lowererMD->m_lowerer->InsertCompareBranch(tmp, instr->UnlinkSrc2(), Js::OpCode::BrGt_A, true, helperLabel, helperLabel);
        }
        else
        {
            lowererMD->m_lowerer->InsertCompareBranch(cmpOpnd, instr->UnlinkSrc2(), Js::OpCode::BrGe_A, true, helperLabel, helperLabel);
        }
        Lowerer::InsertBranch(Js::OpCode::Br, storeLabel, helperLabel);

        lowererMD->m_lowerer->GenerateRuntimeError(storeLabel, JSERR_ArgumentOutOfRange, IR::HelperOp_RuntimeRangeError);

        Lowerer::InsertBranch(Js::OpCode::Br, doneLabel, storeLabel);
        done = doneLabel;
    }
    else
    {
        Assert(!instr->GetSrc2());
        done = instr;
    }

    return done;
}

///----------------------------------------------------------------------------
///
/// LowererMDArch::LowerStartCall
///
///
///----------------------------------------------------------------------------

IR::Instr *
LowererMDArch::LowerStartCall(IR::Instr * startCallInstr)
{
    startCallInstr->m_opcode = Js::OpCode::LoweredStartCall;
    return startCallInstr;
}


///----------------------------------------------------------------------------
///
/// LowererMDArch::LoadHelperArgument
///
///     Assign register or push on stack as per AMD64 calling convention
///
///----------------------------------------------------------------------------

IR::Instr *
LowererMDArch::LoadHelperArgument(IR::Instr *instr, IR::Opnd *opndArg)
{
    IR::Opnd *destOpnd;
    IR::Instr *instrToReturn;
    if(opndArg->IsImmediateOpnd())
    {
        destOpnd = opndArg;
        instrToReturn = instr;
    }
    else
    {
        destOpnd = IR::RegOpnd::New(opndArg->GetType(), this->m_func);
        instrToReturn = instr->m_prev;
        Lowerer::InsertMove(destOpnd, opndArg, instr);
        instrToReturn = instrToReturn->m_next;
    }

    helperCallArgs[helperCallArgsCount++] = destOpnd;
    AssertMsg(helperCallArgsCount < LowererMDArch::MaxArgumentsToHelper,
              "We do not yet support any no. of arguments to the helper");

    return instrToReturn;
}

IR::Instr *
LowererMDArch::LoadDynamicArgument(IR::Instr *instr, uint argNumber)
{
    Assert(instr->m_opcode == Js::OpCode::ArgOut_A_Dynamic);
    Assert(instr->GetSrc2() == nullptr);

    instr->m_opcode = Js::OpCode::MOV;
    IR::Opnd* dst = GetArgSlotOpnd((Js::ArgSlot) argNumber);
    instr->SetDst(dst);

    if (!dst->IsRegOpnd())
    {
        //TODO: Move it to legalizer.
        IR::RegOpnd *tempOpnd = IR::RegOpnd::New(TyMachReg, instr->m_func);
        instr->InsertBefore(IR::Instr::New(Js::OpCode::MOV, tempOpnd, instr->GetSrc1(), instr->m_func));
        instr->ReplaceSrc1(tempOpnd);
    }
    return instr;
}

IR::Instr *
LowererMDArch::LoadDynamicArgumentUsingLength(IR::Instr *instr)
{
    Assert(instr->m_opcode == Js::OpCode::ArgOut_A_Dynamic);
    IR::RegOpnd* src2 = instr->UnlinkSrc2()->AsRegOpnd();

    IR::Instr*mov = IR::Instr::New(Js::OpCode::MOV, IR::RegOpnd::New(TyInt32, this->m_func), src2, this->m_func);
    instr->InsertBefore(mov);
    //We need store nth actuals, so stack location is after function object, callinfo & this pointer
    instr->InsertBefore(IR::Instr::New(Js::OpCode::ADD, mov->GetDst(), mov->GetDst(), IR::IntConstOpnd::New(3, TyInt8, this->m_func), this->m_func));
    IR::RegOpnd *stackPointer   = IR::RegOpnd::New(nullptr, GetRegStackPointer(), TyMachReg, this->m_func);
    IR::IndirOpnd *actualsLocation = IR::IndirOpnd::New(stackPointer, mov->GetDst()->AsRegOpnd(), GetDefaultIndirScale(), TyMachReg, this->m_func);
    instr->SetDst(actualsLocation);

    instr->m_opcode = Js::OpCode::MOV;
    return instr;
}


IR::Instr *
LowererMDArch::LoadDoubleHelperArgument(IR::Instr * instrInsert, IR::Opnd * opndArg)
{
    Assert(opndArg->IsFloat64());
    return LoadHelperArgument(instrInsert, opndArg);
}

//
// Emits the code to allocate 'size' amount of space on stack. for values smaller than PAGE_SIZE
// this will just emit sub rsp,size otherwise calls _chkstk.
//
void
LowererMDArch::GenerateStackAllocation(IR::Instr *instr, uint32 size)
{
    Assert(size > 0);

    IR::RegOpnd *       rspOpnd         = IR::RegOpnd::New(nullptr, RegRSP, TyMachReg, this->m_func);

    //review: size should fit in 32bits

    IR::IntConstOpnd *  stackSizeOpnd   = IR::IntConstOpnd::New(size, TyInt32, this->m_func);

    if (size <= PAGESIZE)
    {
        // Generate SUB RSP, stackSize

        IR::Instr * subInstr = IR::Instr::New(Js::OpCode::SUB,
            rspOpnd, rspOpnd, stackSizeOpnd, this->m_func);

        instr->InsertAfter(subInstr);
    }
    else
    {
        // Generate _chkstk call

        //
        // REVIEW: Call to helper functions assume the address of the variable to be present in
        // RAX. But _chkstk method accepts argument in RAX. Hence handling this one manually.
        // fix this later when CALLHELPER leaved dependency on RAX.
        //

        IR::RegOpnd *raxOpnd = IR::RegOpnd::New(nullptr, RegRAX, TyMachReg, this->m_func);
        IR::RegOpnd *rcxOpnd = IR::RegOpnd::New(nullptr, RegRCX, TyMachReg, this->m_func);
        IR::RegOpnd *rspOpnd = IR::RegOpnd::New(nullptr, RegRSP, TyMachReg, this->m_func);

        IR::Instr * subInstr = IR::Instr::New(Js::OpCode::SUB, rspOpnd, rspOpnd, stackSizeOpnd, this->m_func);
        instr->InsertAfter(subInstr);

        // Leave off the src until we've calculated it below.
        IR::Instr * callInstr = IR::Instr::New(Js::OpCode::Call, raxOpnd, rcxOpnd, this->m_func);
        instr->InsertAfter(callInstr);

        this->LowerCall(callInstr, 0);

        {
            IR::Instr   *movHelperAddrInstr = IR::Instr::New(
                Js::OpCode::MOV,
                rcxOpnd,
                IR::HelperCallOpnd::New(IR::HelperCRT_chkstk, this->m_func),
                this->m_func);

            instr->InsertAfter(movHelperAddrInstr);
        }

        LowererMD::CreateAssign(raxOpnd, stackSizeOpnd, instr->m_next);
    }
}

void
LowererMDArch::MovArgFromReg2Stack(IR::Instr * instr, RegNum reg, uint16 slotNumber, IRType type)
{
    StackSym *          slotSym   = this->m_func->m_symTable->GetArgSlotSym(slotNumber + 1);
    slotSym->m_type = type;
    IR::SymOpnd *       dst       = IR::SymOpnd::New(slotSym, type,  this->m_func);
    IR::RegOpnd *       src       = IR::RegOpnd::New(nullptr, reg, type,  this->m_func);
    IR::Instr *         movInstr =  IR::Instr::New(GetAssignOp(type), dst, src, this->m_func);
    instr->InsertAfter(movInstr);
}

///----------------------------------------------------------------------------
///
/// LowererMDArch::LowerEntryInstr
///
///     Emit prolog.
///
///----------------------------------------------------------------------------
IR::Instr *
LowererMDArch::LowerEntryInstr(IR::EntryInstr * entryInstr)
{
    /*
     * push   rbp
     * mov    rbp,                       rsp
     * sub    rsp,                       localVariablesHeight + floatCalleeSavedRegsSize
     * movsdx qword ptr [rsp + 0],       xmm6  ------\
     * movsdx qword ptr [rsp + 8],       xmm7        |
     * ...                                           |
     * movsdx qword ptr [rsp + (N * 8)], xmmN        |- Callee saved registers.
     * push   rsi                                    |
     * ...                                           |
     * push   rbx                              ------/
     * sub    rsp,                       ArgumentsBacking
     */

    uint savedRegSize           = 0;
    IR::Instr *firstPrologInstr = nullptr;
    IR::Instr *lastPrologInstr  = nullptr;

    // PUSH used callee-saved registers.
    IR::Instr *secondInstr      = entryInstr->m_next;
    AssertMsg(secondInstr, "Instruction chain broken.");

    IR::RegOpnd *stackPointer   = IR::RegOpnd::New(nullptr, GetRegStackPointer(), TyMachReg, this->m_func);
    unsigned     xmmOffset      = 0;

    // PDATA doesn't seem to like two consecutive "SUB RSP, size" instructions. Temporarily save and
    // restore RBX always so that the pattern doesn't occur in the prolog.
    for (RegNum reg = (RegNum)(RegNOREG + 1); reg < RegNumCount; reg = (RegNum)(reg + 1))
    {
        if (LinearScan::IsCalleeSaved(reg) && (this->m_func->HasTry() || this->m_func->m_regsUsed.Test(reg)))
        {
            IRType       type      = RegTypes[reg];
            IR::RegOpnd *regOpnd   = IR::RegOpnd::New(nullptr, reg, type, this->m_func);

            if (type == TyFloat64)
            {
                IR::Instr *saveInstr = IR::Instr::New(Js::OpCode::MOVAPS,
                                                      IR::IndirOpnd::New(stackPointer,
                                                                         xmmOffset,
                                                                         type,
                                                                         this->m_func),
                                                      regOpnd,
                                                      this->m_func);

                xmmOffset += (MachDouble * 2);
                entryInstr->InsertAfter(saveInstr);

                m_func->m_prologEncoder.RecordXmmRegSave();
            }
            else
            {
                Assert(type == TyInt64);

                IR::Instr *pushInstr = IR::Instr::New(Js::OpCode::PUSH, this->m_func);
                pushInstr->SetSrc1(regOpnd);
                entryInstr->InsertAfter(pushInstr);

                m_func->m_prologEncoder.RecordNonVolRegSave();
                savedRegSize += MachPtr;
            }
        }
    }
    //
    // Now that we know the exact stack size, lets fix it for alignment
    // The stack on entry would be aligned. VC++ recommends that the stack
    // should always be 16 byte aligned.
    //
    uint32 argSlotsForFunctionsCalled = this->m_func->m_argSlotsForFunctionsCalled;
    // Stack is always reserved for at least 4 parameters.
    if (argSlotsForFunctionsCalled < 4)
        argSlotsForFunctionsCalled = 4;

    uint32 stackArgsSize    = MachPtr * (argSlotsForFunctionsCalled + 1);

    this->m_func->m_localStackHeight = Math::Align<int32>(this->m_func->m_localStackHeight, 8);

    // Allocate the inlined arg out stack in the locals. Allocate an additional slot so that
    // we can unconditionally clear the first slot past the current frame.
    this->m_func->m_localStackHeight += ((this->m_func->GetMaxInlineeArgOutCount() + 1) * MachPtr);

    uint32 stackLocalsSize  = this->m_func->m_localStackHeight;
    if(xmmOffset != 0)
    {
        // Xmm registers need to be saved to 16-byte-aligned addresses. The stack locals size is aligned here and the total
        // size will be aligned below, which guarantees that the offset from rsp will be 16-byte-aligned.
        stackLocalsSize = ::Math::Align(stackLocalsSize + xmmOffset, static_cast<uint32>(MachDouble * 2));
    }

    uint32 totalStackSize   = stackLocalsSize +
                              stackArgsSize   +
                              savedRegSize;

    AssertMsg(0 == (totalStackSize % 8), "Stack should always be 8 byte aligned");
    uint32 alignmentPadding = (totalStackSize % 16) ? MachPtr : 0;

    stackArgsSize += alignmentPadding;
    Assert(
        xmmOffset == 0 ||
        ::Math::Align(stackArgsSize + savedRegSize, static_cast<uint32>(MachDouble * 2)) == stackArgsSize + savedRegSize);

    totalStackSize += alignmentPadding;
    if(totalStackSize > (1u << 20)) // 1 MB
    {
        // Total stack size is > 1 MB, let's just bail. There are things that need to be changed to allow using large stack
        // sizes, for instance in the unwind info, the offset to saved xmm registers can be (1 MB - 16) at most for the op-code
        // we're currently using (UWOP_SAVE_XMM128). To support larger offsets, we need to use a FAR version of the op-code.
        throw Js::OperationAbortedException();
    }

    if (this->m_func->GetMaxInlineeArgOutCount())
    {
        this->m_func->m_workItem->GetFunctionBody()->SetFrameHeight(this->m_func->m_workItem->GetEntryPoint(), this->m_func->m_localStackHeight);
    }

    //
    // This is the last instruction so should have been emitted before, register saves.
    // But we did not have 'savedRegSize' by then. So we saved secondInstr. We now insert w.r.t that
    // instruction.
    //
    this->m_func->SetArgsSize(stackArgsSize);
    this->m_func->SetSavedRegSize(savedRegSize);
    this->m_func->SetSpillSize(stackLocalsSize);

    if (secondInstr == entryInstr->m_next)
    {
        // There is no register save at all, just combine the stack allocation
        uint combineStackAllocationSize = stackArgsSize + stackLocalsSize;
        this->GenerateStackAllocation(secondInstr->m_prev, combineStackAllocationSize);
        m_func->m_prologEncoder.RecordAlloca(combineStackAllocationSize);
    }
    else
    {
        this->GenerateStackAllocation(secondInstr->m_prev, stackArgsSize);
        m_func->m_prologEncoder.RecordAlloca(stackArgsSize);

        // Allocate frame.
        if (stackLocalsSize)
        {
            this->GenerateStackAllocation(entryInstr, stackLocalsSize);
            m_func->m_prologEncoder.RecordAlloca(stackLocalsSize);
        }
    }

    lastPrologInstr = secondInstr->m_prev;
    Assert(lastPrologInstr != entryInstr);

    // Zero-initialize dedicated arguments slot.
    IR::Instr *movRax0 = nullptr;
    IR::Opnd *raxOpnd = nullptr;
    if (this->m_func->HasArgumentSlot())
    {
        // TODO: Support mov [rbp - n], IMM64
        raxOpnd = IR::RegOpnd::New(nullptr, RegRAX, TyUint32, this->m_func);
        movRax0 = IR::Instr::New(Js::OpCode::XOR, raxOpnd, raxOpnd, raxOpnd, this->m_func);
        secondInstr->m_prev->InsertAfter(movRax0);

        IR::Opnd *opnd = this->lowererMD->CreateStackArgumentsSlotOpnd();
        IR::Instr *movNullInstr = IR::Instr::New(Js::OpCode::MOV, opnd, raxOpnd->UseWithNewType(TyMachReg, this->m_func), this->m_func);
        secondInstr->m_prev->InsertAfter(movNullInstr);
    }

    // Zero initialize the first inlinee frames argc.
    if (this->m_func->GetMaxInlineeArgOutCount())
    {
        if(!movRax0)
        {
            raxOpnd = IR::RegOpnd::New(nullptr, RegRAX, TyUint32, this->m_func);
            movRax0 = IR::Instr::New(Js::OpCode::XOR, raxOpnd, raxOpnd, raxOpnd, this->m_func);
            secondInstr->m_prev->InsertAfter(movRax0);
        }

        StackSym *sym           = this->m_func->m_symTable->GetArgSlotSym((Js::ArgSlot)-1);
        sym->m_isInlinedArgSlot = true;
        sym->m_offset           = 0;
        IR::Opnd *dst           = IR::SymOpnd::New(sym, 0, TyMachReg, this->m_func);
        secondInstr->m_prev->InsertAfter(IR::Instr::New(Js::OpCode::MOV,
                                                        dst,
                                                        raxOpnd->UseWithNewType(TyMachReg, this->m_func),
                                                        this->m_func));
    }

    // Generate MOV RBP, RSP
    IR::RegOpnd * rbpOpnd = IR::RegOpnd::New(nullptr, RegRBP, TyMachReg, this->m_func);
    IR::RegOpnd * rspOpnd = IR::RegOpnd::New(nullptr, RegRSP, TyMachReg, this->m_func);

    IR::Instr * movInstr = IR::Instr::New(Js::OpCode::MOV, rbpOpnd, rspOpnd, this->m_func);
    entryInstr->InsertAfter(movInstr);

    // Generate PUSH RBP
    IR::Instr * pushInstr = IR::Instr::New(Js::OpCode::PUSH, this->m_func);
    pushInstr->SetSrc1(rbpOpnd);
    entryInstr->InsertAfter(pushInstr);
    m_func->m_prologEncoder.RecordNonVolRegSave();
    firstPrologInstr = pushInstr;

    //
    // Insert pragmas that tell the prolog encoder the extent of the prolog.
    //
    firstPrologInstr->InsertBefore(IR::PragmaInstr::New(Js::OpCode::PrologStart, 0, m_func));
    lastPrologInstr->InsertAfter(IR::PragmaInstr::New(Js::OpCode::PrologEnd, 0, m_func));

    //
    // Now store all the arguments in the register in the stack slots
    //
    this->MovArgFromReg2Stack(entryInstr, RegRCX, 1);
    if (m_func->GetJnFunction()->GetIsAsmjsMode() && !m_func->IsLoopBody())
    {
        uint16 offset = 2;
        for (uint16 i = 0; i < m_func->GetJnFunction()->GetAsmJsFunctionInfo()->GetArgCount() && i < 3; i++)
        {
            switch (m_func->GetJnFunction()->GetAsmJsFunctionInfo()->GetArgType(i).which())
            {
            case Js::AsmJsVarType::Int:
                this->MovArgFromReg2Stack(entryInstr, i == 0 ? RegRDX : i == 1 ? RegR8 : RegR9, offset, TyInt32);
                offset++;
                break;
            case Js::AsmJsVarType::Float:
                // registers we need are contiguous, so calculate it from XMM1
                this->MovArgFromReg2Stack(entryInstr, (RegNum)(RegXMM1 + i), offset, TyFloat32);
                offset++;
                break;
            case Js::AsmJsVarType::Double:
                this->MovArgFromReg2Stack(entryInstr, (RegNum)(RegXMM1 + i), offset, TyFloat64);
                offset++;
                break;
            case Js::AsmJsVarType::Float32x4:
                this->MovArgFromReg2Stack(entryInstr, (RegNum)(RegXMM1 + i), offset, TySimd128F4);
                offset += 2;
                break;
            case Js::AsmJsVarType::Int32x4:
                this->MovArgFromReg2Stack(entryInstr, (RegNum)(RegXMM1 + i), offset, TySimd128I4);
                offset += 2;
                break;
            case Js::AsmJsVarType::Float64x2:
                this->MovArgFromReg2Stack(entryInstr, (RegNum)(RegXMM1 + i), offset, TySimd128D2);
                offset += 2;
                break;
            default:
                Assume(UNREACHED);
            }
        }
    }
    else
    {
        this->MovArgFromReg2Stack(entryInstr, RegRDX, 2);
        this->MovArgFromReg2Stack(entryInstr, RegR8, 3);
        this->MovArgFromReg2Stack(entryInstr, RegR9, 4);
    }

    IntConstType frameSize = Js::Constants::MinStackJIT + stackArgsSize + stackLocalsSize + savedRegSize;
    this->GeneratePrologueStackProbe(entryInstr, frameSize);

    return entryInstr;
}

void
LowererMDArch::GeneratePrologueStackProbe(IR::Instr *entryInstr, IntConstType frameSize)
{
    //
    // Generate a stack overflow check. Since ProbeCurrentStack throws an exception it needs
    // an unwindable stack. Should we need to call ProbeCurrentStack, instead of creating a new frame here,
    // we make it appear like our caller directly called ProbeCurrentStack.
    //
    // For thread-bound thread context
    //    MOV  rax, ThreadContext::scriptStackLimit + frameSize
    //    CMP  rsp, rax
    //    JG   $done
    //    MOV  rax, ThreadContext::ProbeCurrentStack
    //    MOV  rcx, frameSize
    //    MOV  rdx, scriptContext
    //    JMP  rax
    // $done:
    //

    // For thread-agile thread context
    //    MOV  rax, [ThreadContext::scriptStackLimit]
    //    ADD  rax, frameSize
    //    CMP  rsp, rax
    //    JG   $done
    //    MOV  rax, ThreadContext::ProbeCurrentStack
    //    MOV  rcx, frameSize
    //    MOV  rdx, scriptContext
    //    JMP  rax
    // $done:
    //

    // For thread context with script interrupt enabled
    //    MOV  rax, [ThreadContext::scriptStackLimit]
    //    ADD  rax, frameSize
    //    JO   $helper
    //    CMP  rsp, rax
    //    JG   $done
    // $helper:
    //    MOV  rax, ThreadContext::ProbeCurrentStack
    //    MOV  rcx, frameSize
    //    MOV  rdx, scriptContext
    //    JMP  rax
    // $done:
    //

    IR::LabelInstr *helperLabel = IR::LabelInstr::New(Js::OpCode::Label, this->m_func, true);
    IR::Instr *insertInstr = entryInstr->m_next;
    IR::Instr *instr;
    IR::Opnd *stackLimitOpnd;
    ThreadContext   *threadContext = this->m_func->GetScriptContext()->GetThreadContext();
    bool doInterruptProbe = threadContext->DoInterruptProbe(this->m_func->GetJnFunction());

    // MOV rax, ThreadContext::scriptStackLimit + frameSize
    stackLimitOpnd = IR::RegOpnd::New(nullptr, RegRAX, TyMachReg, this->m_func);
    if (doInterruptProbe || !threadContext->GetIsThreadBound())
    {
        // Load the current stack limit from the ThreadContext and add the current frame size.
        {
            void *pLimit = threadContext->GetAddressOfStackLimitForCurrentThread();
            IR::RegOpnd *baseOpnd = IR::RegOpnd::New(nullptr, RegRAX, TyMachReg, this->m_func);
            this->lowererMD->CreateAssign(baseOpnd, IR::AddrOpnd::New(pLimit, IR::AddrOpndKindDynamicMisc, this->m_func), insertInstr);
            IR::IndirOpnd *indirOpnd = IR::IndirOpnd::New(baseOpnd, 0, TyMachReg, this->m_func);

            this->lowererMD->CreateAssign(stackLimitOpnd, indirOpnd, insertInstr);
        }

        instr = IR::Instr::New(Js::OpCode::ADD, stackLimitOpnd, stackLimitOpnd,
                               IR::AddrOpnd::New((void*)frameSize, IR::AddrOpndKindConstant, this->m_func), this->m_func);
        insertInstr->InsertBefore(instr);

        if (doInterruptProbe)
        {
            // If the add overflows, call the probe helper.
            instr = IR::BranchInstr::New(Js::OpCode::JO, helperLabel, this->m_func);
            insertInstr->InsertBefore(instr);
        }
    }
    else
    {
        size_t scriptStackLimit = (size_t)this->m_func->GetScriptContext()->GetThreadContext()->GetScriptStackLimit();
        this->lowererMD->CreateAssign(stackLimitOpnd, IR::AddrOpnd::New((void *)(frameSize + scriptStackLimit), IR::AddrOpndKindConstant, this->m_func), insertInstr);
    }

    // CMP rsp, rax
    instr = IR::Instr::New(Js::OpCode::CMP, this->m_func);
    instr->SetSrc1(IR::RegOpnd::New(nullptr, GetRegStackPointer(), TyMachReg, m_func));
    instr->SetSrc2(stackLimitOpnd);
    insertInstr->InsertBefore(instr);


    IR::LabelInstr * doneLabel = nullptr;
    if (!PHASE_OFF(Js::LayoutPhase, this->m_func))
    {
        // JLE $helper
        instr = IR::BranchInstr::New(Js::OpCode::JLE, helperLabel, m_func);
        insertInstr->InsertBefore(instr);
        Security::InsertRandomFunctionPad(insertInstr);

        // This is generated after layout.   Generate the block at the end of the function manually
        insertInstr = IR::PragmaInstr::New(Js::OpCode::StatementBoundary, Js::Constants::NoStatementIndex, m_func);

        this->m_func->m_tailInstr->InsertAfter(insertInstr);
        this->m_func->m_tailInstr = insertInstr;
    }
    else
    {
        doneLabel = IR::LabelInstr::New(Js::OpCode::Label, this->m_func);
        // JGT $done
        instr = IR::BranchInstr::New(Js::OpCode::JGT, doneLabel, m_func);
        insertInstr->InsertBefore(instr);
    }

    insertInstr->InsertBefore(helperLabel);

    IR::RegOpnd *target;
    {
        // MOV rdx, scriptContext
        this->lowererMD->CreateAssign(
            IR::RegOpnd::New(nullptr, RegRDX, TyMachReg, m_func),
            this->lowererMD->m_lowerer->LoadScriptContextOpnd(insertInstr), insertInstr);

        // MOV rcx, frameSize
        this->lowererMD->CreateAssign(
            IR::RegOpnd::New(nullptr, RegRCX, TyMachReg, this->m_func),
            IR::AddrOpnd::New((void*)frameSize, IR::AddrOpndKindConstant, this->m_func), insertInstr);

        // MOV rax, ThreadContext::ProbeCurrentStack
        target = IR::RegOpnd::New(nullptr, RegRAX, TyMachReg, m_func);
        this->lowererMD->CreateAssign(target, IR::HelperCallOpnd::New(IR::HelperProbeCurrentStack, m_func), insertInstr);
    }

    // JMP rax
    instr = IR::MultiBranchInstr::New(Js::OpCode::JMP, target, m_func);
    insertInstr->InsertBefore(instr);

    if (doneLabel)
    {
        // $done:
        insertInstr->InsertBefore(doneLabel);
        Security::InsertRandomFunctionPad(doneLabel);
    }
}


///----------------------------------------------------------------------------
///
/// LowererMDArch::LowerExitInstr
///
///     Emit epilog.
///
///----------------------------------------------------------------------------

IR::Instr *
LowererMDArch::LowerExitInstr(IR::ExitInstr * exitInstr)
{
    uint32 savedRegSize = 0;

    // POP used callee-saved registers

    IR::Instr * exitPrevInstr = exitInstr->m_prev;
    AssertMsg(exitPrevInstr, "Can a function have only 1 instr ? Or is the instr chain broken");

    IR::RegOpnd *stackPointer = IR::RegOpnd::New(nullptr, GetRegStackPointer(), TyMachReg, this->m_func);

    unsigned xmmOffset = 0;

    for (RegNum reg = (RegNum)(RegNOREG + 1); reg < RegNumCount; reg = (RegNum)(reg+1))
    {
        if (LinearScan::IsCalleeSaved(reg) && (this->m_func->HasTry() || this->m_func->m_regsUsed.Test(reg)))
        {
            IRType       type    = RegTypes[reg];
            IR::RegOpnd *regOpnd = IR::RegOpnd::New(nullptr, reg, type, this->m_func);

            if (type == TyFloat64)
            {
                IR::Instr *restoreInstr = IR::Instr::New(Js::OpCode::MOVAPS,
                                                         regOpnd,
                                                         IR::IndirOpnd::New(stackPointer,
                                                                            xmmOffset,
                                                                            type,
                                                                            this->m_func),
                                                         this->m_func);
                xmmOffset += (MachDouble * 2);
                exitInstr->InsertBefore(restoreInstr);
            }
            else
            {
                Assert(type == TyInt64);

                IR::Instr *popInstr = IR::Instr::New(Js::OpCode::POP, regOpnd, this->m_func);
                exitInstr->InsertBefore(popInstr);

                savedRegSize += MachPtr;
            }
        }
    }

    Assert(savedRegSize == (uint)this->m_func->GetSavedRegSize());


    // Generate ADD RSP, argsStackSize before the register restore (if there are any)
    uint32 stackArgsSize = this->m_func->GetArgsSize();
    Assert(stackArgsSize);
    if (savedRegSize || xmmOffset)
    {
        IR::IntConstOpnd *stackSizeOpnd = IR::IntConstOpnd::New(stackArgsSize, TyInt32, this->m_func);
        IR::Instr *addInstr = IR::Instr::New(Js::OpCode::ADD, stackPointer, stackPointer, stackSizeOpnd, this->m_func);
        exitPrevInstr->InsertAfter(addInstr);
    }

    //
    // useful register operands
    //
    IR::RegOpnd * rspOpnd = IR::RegOpnd::New(nullptr, RegRSP, TyMachReg, this->m_func);
    IR::RegOpnd * rbpOpnd = IR::RegOpnd::New(nullptr, RegRBP, TyMachReg, this->m_func);

    // Restore frame

    // Generate MOV RSP, RBP

    IR::Instr * movInstr = IR::Instr::New(Js::OpCode::MOV, rspOpnd, rbpOpnd, this->m_func);
    exitInstr->InsertBefore(movInstr);

    // Generate POP RBP

    IR::Instr * pushInstr = IR::Instr::New(Js::OpCode::POP, rbpOpnd, this->m_func);
    exitInstr->InsertBefore(pushInstr);

    // Insert RET
    IR::IntConstOpnd * intSrc = IR::IntConstOpnd::New(0, TyInt32, this->m_func);
    IR::RegOpnd *retReg = nullptr;
    if (m_func->GetJnFunction()->GetIsAsmjsMode() && !m_func->IsLoopBody())
    {
        switch (m_func->GetJnFunction()->GetAsmJsFunctionInfo()->GetReturnType().which())
        {
        case Js::AsmJsRetType::Double:
        case Js::AsmJsRetType::Float:
            retReg = IR::RegOpnd::New(nullptr, this->GetRegReturnAsmJs(TyMachDouble), TyMachDouble, this->m_func);
            break;
        case Js::AsmJsRetType::Int32x4:
            retReg = IR::RegOpnd::New(nullptr, this->GetRegReturnAsmJs(TySimd128I4), TySimd128I4, this->m_func);
            break;
        case Js::AsmJsRetType::Float32x4:
            retReg = IR::RegOpnd::New(nullptr, this->GetRegReturnAsmJs(TySimd128F4), TySimd128F4, this->m_func);
            break;
        case Js::AsmJsRetType::Float64x2:
            retReg = IR::RegOpnd::New(nullptr, this->GetRegReturnAsmJs(TySimd128D2), TySimd128D2, this->m_func);
            break;
        case Js::AsmJsRetType::Signed:
        case Js::AsmJsRetType::Void:
            retReg = IR::RegOpnd::New(nullptr, this->GetRegReturn(TyMachReg), TyMachReg, this->m_func);
            break;
        default:
            Assume(UNREACHED);
        }
    }
    else
    {
        retReg = IR::RegOpnd::New(nullptr, this->GetRegReturn(TyMachReg), TyMachReg, this->m_func);
    }
    IR::Instr *retInstr = IR::Instr::New(Js::OpCode::RET, this->m_func);
    retInstr->SetSrc1(intSrc);
    retInstr->SetSrc2(retReg);
    exitInstr->InsertBefore(retInstr);

    retInstr->m_opcode = Js::OpCode::RET;

    return exitInstr;
}

IR::Instr *
LowererMDArch::LowerEntryInstrAsmJs(IR::EntryInstr * entryInstr)
{
    // prologue is almost identical on x64, except for loading args
    return LowerEntryInstr(entryInstr);
}

IR::Instr *
LowererMDArch::LowerExitInstrAsmJs(IR::ExitInstr * exitInstr)
{
    // epilogue is almost identical on x64, except for return register
    return LowerExitInstr(exitInstr);
}

void
LowererMDArch::EmitPtrInstr(IR::Instr *instr)
{
    bool legalize = false;
    switch (instr->m_opcode)
    {
    case Js::OpCode::Add_Ptr:
        LowererMD::ChangeToAdd(instr, false /* needFlags */);
        legalize = true;
        break;

    default:
        AssertMsg(UNREACHED, "Un-implemented ptr opcode");
    }
    // OpEq's

    if (legalize)
    {
        LowererMD::Legalize(instr);
    }
    else
    {
        LowererMD::MakeDstEquSrc1(instr);
    }
}

void
LowererMDArch::EmitInt4Instr(IR::Instr *instr, bool signExtend /* = false */)
{
    IR::Opnd *dst       = instr->GetDst();
    IR::Opnd *src1      = instr->GetSrc1();
    IR::Opnd *src2      = instr->GetSrc2();
    IR::Instr *newInstr = nullptr;
    IR::RegOpnd *regEDX;

    if (dst && !dst->IsUInt32())
    {
        dst->SetType(TyInt32);
    }
    if (!src1->IsUInt32())
    {
        src1->SetType(TyInt32);
    }
    if (src2 && !src2->IsUInt32())
    {
        src2->SetType(TyInt32);
    }

    bool legalize = false;
    switch (instr->m_opcode)
    {
    case Js::OpCode::Neg_I4:
        instr->m_opcode = Js::OpCode::NEG;
        break;

    case Js::OpCode::Not_I4:
        instr->m_opcode = Js::OpCode::NOT;
        break;

    case Js::OpCode::Add_I4:
        LowererMD::ChangeToAdd(instr, false /* needFlags */);
        legalize = true;
        break;

    case Js::OpCode::Sub_I4:
        LowererMD::ChangeToSub(instr, false /* needFlags */);
        legalize = true;
        break;

    case Js::OpCode::Mul_I4:
        instr->m_opcode = Js::OpCode::IMUL2;
        break;

    case Js::OpCode::Div_I4:
        instr->SinkDst(Js::OpCode::MOV, RegRAX);
        goto idiv_common;
    case Js::OpCode::Rem_I4:
        instr->SinkDst(Js::OpCode::MOV, RegRDX);
idiv_common:
        if (instr->GetSrc1()->GetType() == TyUint32)
        {
            Assert(instr->GetSrc2()->GetType() == TyUint32);
            instr->m_opcode = Js::OpCode::DIV;
        }
        else
        {
            instr->m_opcode = Js::OpCode::IDIV;
        }
        instr->HoistSrc1(Js::OpCode::MOV, RegRAX);

        regEDX = IR::RegOpnd::New(TyInt32, instr->m_func);
        regEDX->SetReg(RegRDX);
        if (instr->GetSrc1()->GetType() == TyUint32)
        {
            // we need to ensure that register allocator doesn't muck about with rdx
            instr->HoistSrc2(Js::OpCode::MOV, RegRCX);

            newInstr = IR::Instr::New(Js::OpCode::Ld_I4, regEDX, IR::IntConstOpnd::New(0, TyInt32, instr->m_func), instr->m_func);
            instr->InsertBefore(newInstr);
            LowererMD::ChangeToAssign(newInstr);
            // NOP ensures that the EDX = Ld_I4 0 doesn't get deadstored, will be removed in peeps
            instr->InsertBefore(IR::Instr::New(Js::OpCode::NOP, regEDX, regEDX, instr->m_func));
        }
        else
        {
            if (instr->GetSrc2()->IsImmediateOpnd())
            {
                instr->HoistSrc2(Js::OpCode::MOV);
            }
            instr->InsertBefore(IR::Instr::New(Js::OpCode::CDQ, regEDX, instr->m_func));
        }
        return;

    case Js::OpCode::Or_I4:
        instr->m_opcode = Js::OpCode::OR;
        break;

    case Js::OpCode::Xor_I4:
        instr->m_opcode = Js::OpCode::XOR;
        break;

    case Js::OpCode::And_I4:
        instr->m_opcode = Js::OpCode::AND;
        break;

    case Js::OpCode::Shl_I4:
    case Js::OpCode::ShrU_I4:
    case Js::OpCode::Shr_I4:
        LowererMD::ChangeToShift(instr, false /* needFlags */);
        legalize = true;
        break;

    case Js::OpCode::BrTrue_I4:
        instr->m_opcode = Js::OpCode::JNE;
        goto br1_Common;

    case Js::OpCode::BrFalse_I4:
        instr->m_opcode = Js::OpCode::JEQ;
br1_Common:
        src1 = instr->UnlinkSrc1();
        newInstr = IR::Instr::New(Js::OpCode::TEST, instr->m_func);
        instr->InsertBefore(newInstr);
        newInstr->SetSrc1(src1);
        newInstr->SetSrc2(src1);
        return;

    case Js::OpCode::BrEq_I4:
        instr->m_opcode = Js::OpCode::JEQ;
        goto br2_Common;

    case Js::OpCode::BrNeq_I4:
        instr->m_opcode = Js::OpCode::JNE;
        goto br2_Common;

    case Js::OpCode::BrUnGt_I4:
        instr->m_opcode = Js::OpCode::JA;
        goto br2_Common;

    case Js::OpCode::BrUnGe_I4:
        instr->m_opcode = Js::OpCode::JAE;
        goto br2_Common;

    case Js::OpCode::BrUnLe_I4:
        instr->m_opcode = Js::OpCode::JBE;
        goto br2_Common;

    case Js::OpCode::BrUnLt_I4:
        instr->m_opcode = Js::OpCode::JB;
        goto br2_Common;

    case Js::OpCode::BrGt_I4:
        instr->m_opcode = Js::OpCode::JGT;
        goto br2_Common;

    case Js::OpCode::BrGe_I4:
        instr->m_opcode = Js::OpCode::JGE;
        goto br2_Common;

    case Js::OpCode::BrLe_I4:
        instr->m_opcode = Js::OpCode::JLE;
        goto br2_Common;

    case Js::OpCode::BrLt_I4:
        instr->m_opcode = Js::OpCode::JLT;
br2_Common:
        src1 = instr->UnlinkSrc1();
        src2 = instr->UnlinkSrc2();
        newInstr = IR::Instr::New(Js::OpCode::CMP, instr->m_func);
        instr->InsertBefore(newInstr);
        newInstr->SetSrc1(src1);
        newInstr->SetSrc2(src2);
        return;

    default:
        AssertMsg(UNREACHED, "Un-implemented int4 opcode");
    }

    if (signExtend)
    {
        Assert(instr->GetDst());
        IR::Opnd *dst64 = instr->GetDst()->Copy(instr->m_func);
        dst64->SetType(TyMachReg);
        instr->InsertAfter(IR::Instr::New(Js::OpCode::MOVSXD, dst64, instr->GetDst(), instr->m_func));
    }

    if(legalize)
    {
        LowererMD::Legalize(instr);
    }
    else
    {
        // OpEq's
        LowererMD::MakeDstEquSrc1(instr);
    }
}

#if !FLOATVAR
void
LowererMDArch::EmitLoadVar(IR::Instr *instrLoad, bool isFromUint32, bool isHelper)
{
    //    e1  = MOV e_src1
    //    e1  = SHL e1, Js::VarTag_Shift
    //          JO $ToVar
    //          JB  $ToVar     [isFromUint32]
    //    e1  = INC e1
    //  r_dst = MOVSXD e1
    //          JMP $done
    //  $ToVar:
    //          EmitLoadVarNoCheck
    //  $Done:

    Assert(instrLoad->GetSrc1()->IsRegOpnd());
    Assert(instrLoad->GetDst()->GetType() == TyVar);

    bool isInt            = false;
    bool isNotInt         = false;
    IR::Opnd *dst         = instrLoad->GetDst();
    IR::RegOpnd *src1     = instrLoad->GetSrc1()->AsRegOpnd();
    IR::LabelInstr *toVar = nullptr;
    IR::LabelInstr *done  = nullptr;

    // TODO: Fix bad lowering. We shouldn't get TyVars here.
    // Assert(instrLoad->GetSrc1()->GetType() == TyInt32);
    src1->SetType(TyInt32);

    if (src1->IsTaggedInt())
    {
        isInt = true;
    }
    else if (src1->IsNotInt())
    {
        isNotInt = true;
    }

    if (!isNotInt)
    {
        // e1 = MOV e_src1
        IR::RegOpnd *e1 = IR::RegOpnd::New(TyInt32, m_func);
        instrLoad->InsertBefore(IR::Instr::New(Js::OpCode::MOV, e1, instrLoad->GetSrc1(), m_func));

        // e1 = SHL e1, Js::VarTag_Shift
        instrLoad->InsertBefore(IR::Instr::New(Js::OpCode::SHL,
            e1,
            e1,
            IR::IntConstOpnd::New(Js::VarTag_Shift, TyInt8, m_func), m_func));


        if (!isInt)
        {
            // JO $ToVar
            toVar = IR::LabelInstr::New(Js::OpCode::Label, m_func, true);
            instrLoad->InsertBefore(IR::BranchInstr::New(Js::OpCode::JO, toVar, m_func));

            if (isFromUint32)
            {
                //       JB  $ToVar     [isFromUint32]
                instrLoad->InsertBefore(IR::BranchInstr::New(Js::OpCode::JB, toVar, this->m_func));
            }
        }

        // e1 = INC e1
        instrLoad->InsertBefore(IR::Instr::New(Js::OpCode::INC, e1, e1, m_func));

        // dst = MOVSXD e1
        instrLoad->InsertBefore(IR::Instr::New(Js::OpCode::MOVSXD, dst, e1, m_func));

        if (!isInt)
        {
            // JMP $done
            done = IR::LabelInstr::New(Js::OpCode::Label, m_func, isHelper);
            instrLoad->InsertBefore(IR::BranchInstr::New(Js::OpCode::JMP, done, m_func));
        }
    }

    IR::Instr *insertInstr = instrLoad;
    if (!isInt)
    {
        // $toVar:
        if (toVar)
        {
            instrLoad->InsertBefore(toVar);
        }

        // ToVar()
        this->lowererMD->EmitLoadVarNoCheck(dst->AsRegOpnd(), src1, instrLoad, isFromUint32, isHelper || toVar != nullptr);
    }

    if (done)
    {
        instrLoad->InsertAfter(done);
    }

    instrLoad->Remove();
}
#else

void
LowererMDArch::EmitLoadVar(IR::Instr *instrLoad, bool isFromUint32, bool isHelper)
{
    //    MOV e1, e_src1
    //    CMP e1, 0             [uint32]
    //    JLT $Helper           [uint32]  -- overflows?
    //    BTS r1, VarTag_Shift
    //    MOV r_dst, r1
    //    JMP $done             [uint32]
    // $helper                  [uint32]
    //    EmitLoadVarNoCheck
    // $done                    [uint32]


    Assert(instrLoad->GetSrc1()->IsRegOpnd());
    Assert(instrLoad->GetDst()->GetType() == TyVar);

    bool isInt            = false;
    IR::Opnd *dst         = instrLoad->GetDst();
    IR::RegOpnd *src1     = instrLoad->GetSrc1()->AsRegOpnd();
    IR::LabelInstr *labelHelper = nullptr;

    // TODO: Fix bad lowering. We shouldn't get TyVars here.
    // Assert(instrLoad->GetSrc1()->GetType() == TyInt32);
    src1->SetType(TyInt32);

    if (src1->IsTaggedInt())
    {
        isInt = true;
    }
    else if (src1->IsNotInt())
    {
        // ToVar()
        this->lowererMD->EmitLoadVarNoCheck(dst->AsRegOpnd(), src1, instrLoad, isFromUint32, isHelper);
        return;
    }

    IR::RegOpnd *r1 = IR::RegOpnd::New(TyVar, m_func);

    // e1 = MOV e_src1
    IR::RegOpnd *e1 = r1->Copy(m_func)->AsRegOpnd();
    e1->SetType(TyInt32);
    instrLoad->InsertBefore(IR::Instr::New(Js::OpCode::MOV,
        e1,
        src1,
        m_func));

    if (!isInt && isFromUint32)
    {
        // CMP e1, 0
        IR::Instr *instr = IR::Instr::New(Js::OpCode::CMP, m_func);
        instr->SetSrc1(e1);
        instr->SetSrc2(IR::IntConstOpnd::New(0, TyInt32, m_func));
        instrLoad->InsertBefore(instr);

        Assert(!labelHelper);
        labelHelper = IR::LabelInstr::New(Js::OpCode::Label, m_func, true);

        // JLT $helper
        instr = IR::BranchInstr::New(Js::OpCode::JLT, labelHelper, m_func);
        instrLoad->InsertBefore(instr);
    }

    // The previous operation clears the top 32 bits.
    // BTS r1, VarTag_Shift
    this->lowererMD->GenerateInt32ToVarConversion(r1, instrLoad);

    // REVIEW: We need r1 only if we could generate sn = Ld_A_I4 sn. i.e. the destination and
    // source are the same.
    // r_dst = MOV r1
    instrLoad->InsertBefore(IR::Instr::New(Js::OpCode::MOV,
        dst,
        r1,
        m_func));

    if (labelHelper)
    {
        Assert(isFromUint32);

        // JMP $done
        IR::LabelInstr * labelDone = IR::LabelInstr::New(Js::OpCode::Label, m_func, isHelper);
        instrLoad->InsertBefore(IR::BranchInstr::New(Js::OpCode::JMP, labelDone, m_func));

        // $helper
        instrLoad->InsertBefore(labelHelper);

        // ToVar()
        this->lowererMD->EmitLoadVarNoCheck(dst->AsRegOpnd(), src1, instrLoad, isFromUint32, true);

        // $done
        instrLoad->InsertBefore(labelDone);
    }
    instrLoad->Remove();
}
#endif

void
LowererMDArch::EmitIntToFloat(IR::Opnd *dst, IR::Opnd *src, IR::Instr *instrInsert)
{
    Assert(dst->IsRegOpnd() && dst->IsFloat());
    Assert(src->IsRegOpnd() && src->IsInt32());
    if (dst->IsFloat64())
    {
        // Use MOVD to make sure we sign extended the 32-bit src
        instrInsert->InsertBefore(IR::Instr::New(Js::OpCode::MOVD, dst, src, this->m_func));

        // Convert to float
        instrInsert->InsertBefore(IR::Instr::New(Js::OpCode::CVTDQ2PD, dst, dst, this->m_func));
    }
    else
    {
        Assert(dst->IsFloat32());
        instrInsert->InsertBefore(IR::Instr::New(Js::OpCode::CVTSI2SS, dst, src, this->m_func));
    }
}

void
LowererMDArch::EmitUIntToFloat(IR::Opnd *dst, IR::Opnd *src, IR::Instr *instrInsert)
{
    Assert(dst->IsRegOpnd() && dst->IsFloat());
    Assert(src->IsRegOpnd() && (src->IsInt32() || src->IsUInt32()));

    // MOV tempReg.i32, src  - make sure the top bits are 0
    IR::RegOpnd * tempReg = IR::RegOpnd::New(TyInt32, this->m_func);
    instrInsert->InsertBefore(IR::Instr::New(Js::OpCode::MOV_TRUNC, tempReg, src, this->m_func));

    // CVTSI2SD dst, tempReg.i64  (Use the tempreg as if it is 64 bit without sign extension)
    instrInsert->InsertBefore(IR::Instr::New(dst->IsFloat64() ? Js::OpCode::CVTSI2SD : Js::OpCode::CVTSI2SS, dst,
        tempReg->UseWithNewType(TyInt64, this->m_func), this->m_func));
}

bool
LowererMDArch::EmitLoadInt32(IR::Instr *instrLoad)
{
    //
    //    r1 = MOV src1
    // rtest = MOV src1
    //         SHR rtest, AtomTag_Shift
    //         CMP rtest, 1
    //         JNE $helper or $float
    // r_dst = MOV_TRUNC e_src1
    //         JMP $done
    //  $float:
    //     dst = ConvertToFloat(r1, $helper)
    // $helper:
    // r_dst = ToInt32()
    //

    Assert(instrLoad->GetSrc1()->IsRegOpnd());
    Assert(instrLoad->GetSrc1()->GetType() == TyVar);

    // TODO: Fix bad lowering. We shouldn't see TyVars here.
    // Assert(instrLoad->GetDst()->GetType() == TyInt32);

    bool isInt             = false;
    bool isNotInt          = false;
    IR::Opnd *dst          = instrLoad->GetDst();
    IR::RegOpnd *src1      = instrLoad->GetSrc1()->AsRegOpnd();
    IR::LabelInstr *helper = nullptr;
    IR::LabelInstr *labelFloat = nullptr;
    IR::LabelInstr *done   = nullptr;

    if (src1->IsTaggedInt())
    {
        isInt = true;
    }
    else if (src1->IsNotInt())
    {
        isNotInt = true;
    }

    if (src1->IsEqual(instrLoad->GetDst()) == false)
    {
        // r1 = MOV src1
        IR::RegOpnd *r1 = IR::RegOpnd::New(TyVar, instrLoad->m_func);
        r1->SetValueType(src1->GetValueType());
        instrLoad->InsertBefore(IR::Instr::New(Js::OpCode::MOV, r1, src1, instrLoad->m_func));
        src1 = r1;
    }

    const ValueType src1ValueType(src1->GetValueType());
    const bool doFloatToIntFastPath =
        (src1ValueType.IsLikelyFloat() || src1ValueType.IsLikelyUntaggedInt()) &&
                !(instrLoad->HasBailOutInfo() && (instrLoad->GetBailOutKind() == IR::BailOutIntOnly || instrLoad->GetBailOutKind() == IR::BailOutExpectingInteger));

    if (!isNotInt)
    {
        if (!isInt)
        {
            if(doFloatToIntFastPath)
            {
                labelFloat = IR::LabelInstr::New(Js::OpCode::Label, instrLoad->m_func, false);
            }
            else
            {
                helper = IR::LabelInstr::New(Js::OpCode::Label, instrLoad->m_func, true);
            }

            this->lowererMD->GenerateSmIntTest(src1, instrLoad, labelFloat ? labelFloat : helper);
        }
        IR::RegOpnd *src132 = src1->UseWithNewType(TyInt32, instrLoad->m_func)->AsRegOpnd();
#if !INT32VAR
        // src1 = SAR src1, VarTag_Shift
        instrLoad->InsertBefore(IR::Instr::New(Js::OpCode::SAR,
                                               src132,
                                               src132,
                                               IR::IntConstOpnd::New(Js::VarTag_Shift, TyInt8, instrLoad->m_func),
                                               instrLoad->m_func));

        // r_dst = MOV src1
        // This is only a MOV (and not a MOVSXD) because we do a signed shift right, but we'll copy
        // all 64 bits.


        instrLoad->InsertBefore(IR::Instr::New(Js::OpCode::MOV,
                                               dst->UseWithNewType(TyMachReg, instrLoad->m_func),
                                               src1,
                                               instrLoad->m_func));

#else

        instrLoad->InsertBefore(IR::Instr::New(Js::OpCode::MOV_TRUNC,
                                               dst->UseWithNewType(TyInt32, instrLoad->m_func),
                                               src132,
                                               instrLoad->m_func));
#endif
        if (!isInt)
        {
            // JMP $done
            done = instrLoad->GetOrCreateContinueLabel();
            instrLoad->InsertBefore(IR::BranchInstr::New(Js::OpCode::JMP, done, m_func));
        }
    }

    if (!isInt)
    {
        if(doFloatToIntFastPath)
        {
            if(labelFloat)
            {
                instrLoad->InsertBefore(labelFloat);
            }
            if(!helper)
            {
                helper = IR::LabelInstr::New(Js::OpCode::Label, instrLoad->m_func, true);
            }

            if(!done)
            {
                done = instrLoad->GetOrCreateContinueLabel();
            }
#if FLOATVAR
            IR::RegOpnd* floatOpnd = this->lowererMD->CheckFloatAndUntag(src1, instrLoad, helper);
#else
            this->lowererMD->GenerateFloatTest(src1, instrLoad, helper, instrLoad->HasBailOutInfo());
            IR::IndirOpnd* floatOpnd = IR::IndirOpnd::New(src1, Js::JavascriptNumber::GetValueOffset(), TyMachDouble, this->m_func);
#endif
            this->lowererMD->ConvertFloatToInt32(instrLoad->GetDst(), floatOpnd, helper, done, instrLoad);
        }

        // $helper:
        if (helper)
        {
            instrLoad->InsertBefore(helper);
        }
        if(instrLoad->HasBailOutInfo() && (instrLoad->GetBailOutKind() == IR::BailOutIntOnly || instrLoad->GetBailOutKind() == IR::BailOutExpectingInteger))
        {
            // Avoid bailout if we have a JavascriptNumber whose value is a signed 32-bit integer
            lowererMD->m_lowerer->LoadInt32FromUntaggedVar(instrLoad);

            // Need to bail out instead of calling a helper
            return true;
        }
        lowererMD->m_lowerer->LowerUnaryHelperMem(instrLoad, IR::HelperConv_ToInt32);
    }
    else
    {
        instrLoad->Remove();
    }

    return false;
}

IR::Instr *
LowererMDArch::LoadCheckedFloat(IR::RegOpnd *opndOrig, IR::RegOpnd *opndFloat, IR::LabelInstr *labelInline, IR::LabelInstr *labelHelper, IR::Instr *instrInsert, const bool checkForNullInLoopBody)
{
    //
    //   if (TaggedInt::Is(opndOrig))
    //       s1        = MOVSXD opndOrig_32
    //       opndFloat = CVTSI2SD s1
    //                   JMP $labelInline
    //   else
    //                   JMP $labelOpndIsNotInt
    //
    // $labelOpndIsNotInt:
    //   if (TaggedFloat::Is(opndOrig))
    //       s2        = MOV opndOrig
    //       s2        = XOR FloatTag_Value
    //       opndFloat = MOVD s2
    //   else
    //                   JMP $labelHelper
    //
    // $labelInline:
    //

    IR::Instr *instrFirst = nullptr;

    IR::LabelInstr *labelOpndIsNotInt = IR::LabelInstr::New(Js::OpCode::Label, this->m_func);
    lowererMD->GenerateSmIntTest(opndOrig, instrInsert, labelOpndIsNotInt, &instrFirst);

    if (opndOrig->GetValueType().IsLikelyFloat())
    {
        // Make this path helper if value is likely a float
        instrInsert->InsertBefore(IR::LabelInstr::New(Js::OpCode::Label, this->m_func, true));
    }

    IR::RegOpnd *s1          = IR::RegOpnd::New(TyMachReg, this->m_func);
    IR::Opnd    *opndOrig_32 = opndOrig->UseWithNewType(TyInt32, this->m_func);

    IR::Instr   *movsxd      = IR::Instr::New(Js::OpCode::MOVSXD, s1, opndOrig_32, this->m_func);
    instrInsert->InsertBefore(movsxd);

    IR::Instr   *cvtsi2sd    = IR::Instr::New(Js::OpCode::CVTSI2SD, opndFloat, s1, this->m_func);
    instrInsert->InsertBefore(cvtsi2sd);

    IR::Instr   *jmpInline   = IR::BranchInstr::New(Js::OpCode::JMP, labelInline, this->m_func);
    instrInsert->InsertBefore(jmpInline);

    instrInsert->InsertBefore(labelOpndIsNotInt);

    lowererMD->GenerateFloatTest(opndOrig, instrInsert, labelHelper, checkForNullInLoopBody);

    IR::RegOpnd *s2          = IR::RegOpnd::New(TyMachReg, this->m_func);
    IR::Instr   *mov         = IR::Instr::New(Js::OpCode::MOV, s2, opndOrig, this->m_func);
    instrInsert->InsertBefore(mov);

    IR::Instr   *xorTag      = IR::Instr::New(Js::OpCode::XOR,
                                              s2,
                                              s2,
                                              IR::AddrOpnd::New((Js::Var)Js::FloatTag_Value,
                                                                IR::AddrOpndKindConstantVar,
                                                                this->m_func,
                                                                /* dontEncode = */ true),
                                              this->m_func);
    instrInsert->InsertBefore(xorTag);
    LowererMD::Legalize(xorTag);

    IR::Instr   *movFloat    = IR::Instr::New(Js::OpCode::MOVD, opndFloat, s2, this->m_func);
    instrInsert->InsertBefore(movFloat);

    return instrFirst;
}

IR::LabelInstr *
LowererMDArch::GetBailOutStackRestoreLabel(BailOutInfo * bailOutInfo, IR::LabelInstr * exitTargetInstr)
{
    return exitTargetInstr;
}

bool LowererMDArch::GenerateFastAnd(IR::Instr * instrAnd)
{
    return true;
}

bool LowererMDArch::GenerateFastXor(IR::Instr * instrXor)
{
    return true;
}

bool LowererMDArch::GenerateFastOr(IR::Instr * instrOr)
{
    return true;
}

bool LowererMDArch::GenerateFastNot(IR::Instr * instrNot)
{
    return true;
}

bool LowererMDArch::GenerateFastShiftLeft(IR::Instr * instrShift)
{
    return true;
}

bool LowererMDArch::GenerateFastShiftRight(IR::Instr * instrShift)
{
    // Given:
    //
    // dst = Shr/ShrU src1, src2
    //
    // Generate:
    //
    // (If not 2 Int31's, jump to $helper.)
    // s1 = MOV src1
    //RCX = MOV src2
    //      TEST RCX, 0x1F             [unsigned only]  // Bail if unsigned and not shifting,
    //      JEQ $helper                [unsigned only]  // as we may not end up with a taggable int
    // s1 = SAR/SHR s1, RCX
    //      BTS s1, VarTag_Shift
    //dst = MOV s1
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
    Assert(instrShift->m_opcode == Js::OpCode::ShrU_A || instrShift->m_opcode == Js::OpCode::Shr_A);
    bool             isUnsigned = (instrShift->m_opcode == Js::OpCode::ShrU_A);

    opndSrc1 = instrShift->GetSrc1();
    opndSrc2 = instrShift->GetSrc2();
    AssertMsg(opndSrc1 && opndSrc2, "Expected 2 src opnd's on Add instruction");

    // Not int?
    if (opndSrc1->IsRegOpnd() && opndSrc1->AsRegOpnd()->IsNotInt())
    {
        return true;
    }
    if (opndSrc2->IsRegOpnd() && opndSrc2->AsRegOpnd()->IsNotInt())
    {
        return true;
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

    IntConstType s2Value = 0;
    bool src2IsIntConst = false;

    if (isUnsigned)
    {
        if (opndSrc2->IsRegOpnd())
        {
            src2IsIntConst = opndSrc2->AsRegOpnd()->m_sym->IsTaggableIntConst();
            if (src2IsIntConst)
            {
                s2Value = opndSrc2->AsRegOpnd()->m_sym->GetIntConstValue();
            }
        }
        else
        {
            AssertMsg(opndSrc2->IsAddrOpnd() && Js::TaggedInt::Is(opndSrc2->AsAddrOpnd()->m_address),
                "Expect src2 of shift right to be reg or Var.");
            src2IsIntConst = true;
            s2Value = Js::TaggedInt::ToInt32(opndSrc2->AsAddrOpnd()->m_address);
        }

        // 32-bit Shifts only uses the bottom 5 bits.
        s2Value &=  0x1F;

        // Unsigned shift by 0 could yield a value not encodable as an tagged int.
        if (isUnsigned && src2IsIntConst && s2Value == 0)
        {
            return true;
        }
    }


    labelHelper = IR::LabelInstr::New(Js::OpCode::Label, this->m_func, true);

    if (!isTaggedInts)
    {
        // (If not 2 Int31's, jump to $helper.)

        this->lowererMD->GenerateSmIntPairTest(instrShift, opndSrc1, opndSrc2, labelHelper);
    }

    opndSrc1    = opndSrc1->UseWithNewType(TyInt32, this->m_func);
    if (src2IsIntConst)
    {
        opndSrc2 = IR::IntConstOpnd::New(s2Value, TyInt32, this->m_func);
    }
    else
    {
        // RCX = MOV src2
        opndSrc2 = opndSrc2->UseWithNewType(TyInt32, this->m_func);
        opndReg = IR::RegOpnd::New(TyInt32, this->m_func);
        opndReg->AsRegOpnd()->SetReg(this->GetRegShiftCount());
        instr = IR::Instr::New(Js::OpCode::MOV, opndReg, opndSrc2, this->m_func);
        instrShift->InsertBefore(instr);
        opndSrc2 = opndReg;
    }

    if (!src2IsIntConst && isUnsigned)
    {
        //      TEST RCX, 0x1F             [unsigned only]  // Bail if unsigned and not shifting,
        instr = IR::Instr::New(Js::OpCode::TEST, this->m_func);
        instr->SetSrc1(opndSrc2);
        instr->SetSrc2(IR::IntConstOpnd::New(0x1F, TyInt32, this->m_func));
        instrShift->InsertBefore(instr);

        //      JEQ $helper                  [unsigned only]  // as we may not end up with a taggable int
        instr = IR::BranchInstr::New(Js::OpCode::JEQ, labelHelper, this->m_func);
        instrShift->InsertBefore(instr);
    }

    // s1 = MOV src1
    opndReg = IR::RegOpnd::New(TyInt32, this->m_func);
    instr = IR::Instr::New(Js::OpCode::MOV, opndReg, opndSrc1, this->m_func);
    instrShift->InsertBefore(instr);

    // s1 = SAR/SHR s1, RCX
    instr = IR::Instr::New(isUnsigned ? Js::OpCode::SHR : Js::OpCode::SAR, opndReg, opndReg, opndSrc2, this->m_func);
    instrShift->InsertBefore(instr);

    //
    // Convert TyInt32 operand, back to TyMachPtr type.
    //

    if(TyMachReg != opndReg->GetType())
    {
        opndReg = opndReg->UseWithNewType(TyMachPtr, this->m_func);
    }

    //    BTS s1, VarTag_Shift
    this->lowererMD->GenerateInt32ToVarConversion(opndReg, instrShift);

    // dst = MOV s1

    instr = IR::Instr::New(Js::OpCode::MOV, instrShift->GetDst(), opndReg, this->m_func);
    instrShift->InsertBefore(instr);

    //      JMP $fallthru

    labelFallThru = IR::LabelInstr::New(Js::OpCode::Label, this->m_func);
    instr = IR::BranchInstr::New(Js::OpCode::JMP, labelFallThru, this->m_func);
    instrShift->InsertBefore(instr);

    // $helper:
    //      (caller generates helper call)
    // $fallthru:

    instrShift->InsertBefore(labelHelper);
    instrShift->InsertAfter(labelFallThru);

    return true;
}

void
LowererMDArch::FinalLower()
{
    IR::IntConstOpnd *intOpnd;

    FOREACH_INSTR_BACKWARD_EDITING_IN_RANGE(instr, instrPrev, this->m_func->m_tailInstr, this->m_func->m_headInstr)
    {
        switch (instr->m_opcode)
        {
        case Js::OpCode::LdArgSize:
            Assert(this->m_func->HasTry());
            instr->m_opcode = Js::OpCode::MOV;
            intOpnd = IR::IntConstOpnd::New(this->m_func->GetArgsSize(), TyUint32, this->m_func);
            instr->SetSrc1(intOpnd);
            LowererMD::Legalize(instr);
            break;

        case Js::OpCode::LdSpillSize:
            Assert(this->m_func->HasTry());
            instr->m_opcode = Js::OpCode::MOV;
            intOpnd = IR::IntConstOpnd::New(this->m_func->GetSpillSize(), TyUint32, this->m_func);
            instr->SetSrc1(intOpnd);
            LowererMD::Legalize(instr);
            break;

        case Js::OpCode::Leave:
            Assert(this->m_func->DoOptimizeTryCatch() && !this->m_func->IsLoopBodyInTry());
            instrPrev = this->lowererMD->LowerLeave(instr, instr->AsBranchInstr()->GetTarget(), true /*fromFinalLower*/);
            break;

        case Js::OpCode::CMOVA:
        case Js::OpCode::CMOVAE:
        case Js::OpCode::CMOVB:
        case Js::OpCode::CMOVBE:
        case Js::OpCode::CMOVE:
        case Js::OpCode::CMOVG:
        case Js::OpCode::CMOVGE:
        case Js::OpCode::CMOVL:
        case Js::OpCode::CMOVLE:
        case Js::OpCode::CMOVNE:
        case Js::OpCode::CMOVNO:
        case Js::OpCode::CMOVNP:
        case Js::OpCode::CMOVNS:
        case Js::OpCode::CMOVO:
        case Js::OpCode::CMOVP:
        case Js::OpCode::CMOVS:
            // Get rid of fake src1.
            if (instr->GetSrc2())
            {
                // CMOV inserted before regalloc have a dummy src1 to simulate the fact that
                // CMOV is not an definite def of the dst.
                instr->SwapOpnds();
                instr->FreeSrc2();
            }
            break;
        }
    } NEXT_INSTR_BACKWARD_EDITING_IN_RANGE;
}

IR::Opnd*
LowererMDArch::GenerateArgOutForStackArgs(IR::Instr* callInstr, IR::Instr* stackArgsInstr)
{
    return this->lowererMD->m_lowerer->GenerateArgOutForStackArgs(callInstr, stackArgsInstr);
}

void
LowererMDArch::LowerInlineSpreadArgOutLoop(IR::Instr *callInstr, IR::RegOpnd *indexOpnd, IR::RegOpnd *arrayElementsStartOpnd)
{
    this->lowererMD->m_lowerer->LowerInlineSpreadArgOutLoopUsingRegisters(callInstr, indexOpnd, arrayElementsStartOpnd);
}

IR::Instr *
LowererMDArch::LowerEHRegionReturn(IR::Instr * insertBeforeInstr, IR::Opnd * targetOpnd)
{
    IR::RegOpnd *retReg = IR::RegOpnd::New(StackSym::New(TyMachReg, this->m_func), GetRegReturn(TyMachReg), TyMachReg, this->m_func);

    // Load the continuation address into the return register.
    insertBeforeInstr->InsertBefore(IR::Instr::New(Js::OpCode::MOV, retReg, targetOpnd, this->m_func));

    // MOV r8, spillSize
    IR::Instr *movR8 = IR::Instr::New(Js::OpCode::LdSpillSize,
        IR::RegOpnd::New(nullptr, RegR8, TyMachReg, m_func),
        m_func);
    insertBeforeInstr->InsertBefore(movR8);


    // MOV r9, argsSize
    IR::Instr *movR9 = IR::Instr::New(Js::OpCode::LdArgSize,
        IR::RegOpnd::New(nullptr, RegR9, TyMachReg, m_func),
        m_func);
    insertBeforeInstr->InsertBefore(movR9);

    // MOV rcx, amd64_ReturnFromCallWithFakeFrame
    // PUSH rcx
    // RET
    IR::Opnd *endCallWithFakeFrame = endCallWithFakeFrame = IR::RegOpnd::New(nullptr, RegRCX, TyMachReg, m_func);
    IR::Instr *movTarget = IR::Instr::New(Js::OpCode::MOV,
        endCallWithFakeFrame,
        IR::HelperCallOpnd::New(IR::HelperOp_ReturnFromCallWithFakeFrame, m_func),
        m_func);
    insertBeforeInstr->InsertBefore(movTarget);

    IR::Instr *push = IR::Instr::New(Js::OpCode::PUSH, m_func);
    push->SetSrc1(endCallWithFakeFrame);
    insertBeforeInstr->InsertBefore(push);

#if 0
        // TODO: This block gets deleted if we emit a JMP instead of a RET.
    IR::BranchInstr *jmp = IR::BranchInstr::New(Js::OpCode::JMP,
        nullptr,
        targetOpnd,
        m_func);
    leaveInstr->InsertBefore(jmp);
#endif

    IR::IntConstOpnd *intSrc = IR::IntConstOpnd::New(0, TyInt32, this->m_func);
    IR::Instr * retInstr = IR::Instr::New(Js::OpCode::RET, this->m_func);
    retInstr->SetSrc1(intSrc);
    retInstr->SetSrc2(retReg);
    insertBeforeInstr->InsertBefore(retInstr);

    // return the last instruction inserted
    return retInstr;
}
