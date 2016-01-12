//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "BackEnd.h"
#include "LowererMDArch.h"
#include "Library\JavascriptGeneratorFunction.h"

const Js::OpCode LowererMD::MDExtend32Opcode = Js::OpCode::MOV;

BYTE
LowererMDArch::GetDefaultIndirScale()
{
    return IndirScale4;
}

RegNum
LowererMDArch::GetRegShiftCount()
{
    return RegECX;
}

RegNum
LowererMDArch::GetRegReturn(IRType type)
{
    return ( IRType_IsFloat(type) || IRType_IsSimd128(type) ) ? RegNOREG : RegEAX;
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
        return RegEAX;
    }
}

RegNum
LowererMDArch::GetRegStackPointer()
{
    return RegESP;
}

RegNum
LowererMDArch::GetRegBlockPointer()
{
    return RegEBP;
}

RegNum
LowererMDArch::GetRegFramePointer()
{
    return RegEBP;
}


RegNum
LowererMDArch::GetRegChkStkParam()
{
    return RegEAX;
}

RegNum
LowererMDArch::GetRegIMulDestLower()
{
    return RegEAX;
}

RegNum
LowererMDArch::GetRegIMulHighDestLower()
{
    return RegEDX;
}
RegNum
LowererMDArch::GetRegArgI4(int32 argNum)
{
    return RegNOREG;
}

RegNum
LowererMDArch::GetRegArgR8(int32 argNum)
{
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
    this->lowererMD = lowererMD;
    this->helperCallArgsCount = 0;
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
        StackSym *paramSym = StackSym::New(TyVar, this->m_func);
        this->m_func->SetArgOffset(paramSym, 5 * MachPtr);
        IR::Instr *instr = this->lowererMD->LoadStackAddress(paramSym, optionalDstOpnd);
        instrInsert->InsertBefore(instr);
        return instr;
    }
}

IR::Instr *
LowererMDArch::LoadStackArgPtr(IR::Instr * instrArgPtr)
{
    // if (actual count >= formal count)
    //     dst = ebp + 5 * sizeof(Var) -- point to the first input parameter after "this"
    // else
    //     sub esp, (size of formals) -- we'll copy the input params to the callee frame, since the caller frame
    //                                   doesn't have space for them all
    //     dst = esp + 3 * sizeof(var) -- point to the location of the first input param (after "this")
    //                                    within the area we just allocated on the callee frame

    IR::Instr * instrPrev = instrArgPtr;
    IR::LabelInstr * instrLabelExtra = nullptr;
    IR::Instr * instr;
    IR::Opnd * opnd;
    Js::ArgSlot formalsCount = this->m_func->GetInParamsCount();

    // Only need to check the number of actuals if there's at least 1 formal (plus "this")
    if (formalsCount > 1)
    {
        instrPrev = this->lowererMD->LoadInputParamCount(instrArgPtr);
        IR::Opnd * opndActuals = instrPrev->GetDst();
        IR::Opnd * opndFormals =
            IR::IntConstOpnd::New(formalsCount, TyMachReg, this->m_func);

        instr = IR::Instr::New(Js::OpCode::CMP, this->m_func);
        instr->SetSrc1(opndActuals);
        instr->SetSrc2(opndFormals);
        instrArgPtr->InsertBefore(instr);

        instrLabelExtra = IR::LabelInstr::New(Js::OpCode::Label, this->m_func, true);
        instr = IR::BranchInstr::New(Js::OpCode::JB, instrLabelExtra, this->m_func);
        instrArgPtr->InsertBefore(instr);
    }

    // Modify the original instruction to load the addr of the input parameters on the caller's frame.

    instr = LoadInputParamPtr(instrArgPtr, instrArgPtr->UnlinkDst()->AsRegOpnd());
    instrArgPtr->Remove();
    instrArgPtr = instr;

    if (instrLabelExtra)
    {
        IR::LabelInstr *instrLabelDone = IR::LabelInstr::New(Js::OpCode::Label, this->m_func);
        instr = IR::BranchInstr::New(Js::OpCode::JMP, instrLabelDone, this->m_func);
        instrArgPtr->InsertAfter(instr);

        instr->InsertAfter(instrLabelExtra);

        instrLabelExtra->InsertAfter(instrLabelDone);

        // Allocate space on the callee's frame for a copy of the formals, plus the callee object pointer
        // and the callinfo.
        // Be sure to double-align the allocation.
        // REVIEW: Do we ever need to generate a chkstk call here?
        int formalsBytes = (formalsCount + 2) * sizeof(Js::Var);
        formalsBytes = Math::Align<size_t>(formalsBytes, MachStackAlignment);
        IR::RegOpnd * espOpnd = IR::RegOpnd::New(nullptr, this->GetRegStackPointer(), TyMachReg, this->m_func);
        opnd = IR::IndirOpnd::New(espOpnd, -formalsBytes, TyMachReg, this->m_func);
        instr = IR::Instr::New(Js::OpCode::LEA, espOpnd, opnd, this->m_func);
        instrLabelDone->InsertBefore(instr);

        // Result is the pointer to the address where we'll store the first input param
        // (after "this") in the callee's frame.
        opnd = IR::IndirOpnd::New(espOpnd, 3 * sizeof(Js::Var), TyMachReg, this->m_func);
        instr = IR::Instr::New(Js::OpCode::LEA, instrArgPtr->GetDst(), opnd, this->m_func);
        instrLabelDone->InsertBefore(instr);
    }

    return instrPrev;
}

///----------------------------------------------------------------------------
///
/// LowererMDArch::LoadHeapArguments
///
///     Load the heap-based arguments object
///
///----------------------------------------------------------------------------

IR::Instr *
LowererMDArch::LoadHeapArguments(IR::Instr *instrArgs, bool force, IR::Opnd* opndInputParamCount)
{
    ASSERT_INLINEE_FUNC(instrArgs);
    Func *func = instrArgs->m_func;

    IR::Instr * instrPrev = instrArgs->m_prev;
    if (!force && func->GetHasStackArgs() && this->m_func->GetHasStackArgs()) //both inlinee & inliner has stack args. We don't support other scenarios.
    {
        // The initial args slot value is zero. (TODO: it should be possible to dead-store the LdHeapArgs in this case.)
        instrArgs->m_opcode = Js::OpCode::MOV;
        instrArgs->ReplaceSrc1(IR::IntConstOpnd::New(0, TyMachReg, func));
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
            /*
             * s3 = address of first actual argument (after "this").
             * Stack looks like arg 1 ('this')       <-- low address
             *                  ...
             *                  arg N
             *                  arguments object
             *                  function object
             *                  argc                 <-- frameStartSym
             */
            StackSym *firstRealArgSlotSym = func->GetInlineeArgvSlotOpnd()->m_sym->AsStackSym();
            this->m_func->SetArgOffset(firstRealArgSlotSym, firstRealArgSlotSym->m_offset + MachPtr);
            IR::Instr *instr = this->lowererMD->LoadStackAddress(firstRealArgSlotSym);
            instrArgs->InsertBefore(instr);
            this->LoadHelperArgument(instrArgs, instr->GetDst());

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
            // s3 = address of first actual argument (after "this")
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
            StackSym *paramSym = StackSym::New(TyMachReg, func);
            this->m_func->SetArgOffset(paramSym, 2 * MachPtr);
            IR::Opnd *srcOpnd = IR::SymOpnd::New(paramSym, TyMachReg, func);

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
/// LowererMDArch::LoadHeapArgsCached
///
///     Load the heap-based arguments object using a cached scope
///
///----------------------------------------------------------------------------

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
        // s4 = address of first actual argument (after "this")
        StackSym *firstRealArgSlotSym = func->GetInlineeArgvSlotOpnd()->m_sym->AsStackSym();
        this->m_func->SetArgOffset(firstRealArgSlotSym, firstRealArgSlotSym->m_offset + MachPtr);
        IR::Instr *instr = this->lowererMD->LoadStackAddress(firstRealArgSlotSym);
        instrArgs->InsertBefore(instr);
        this->LoadHelperArgument(instrArgs, instr->GetDst());

        // s3 = formal argument count (without counting "this")
        uint32 formalsCount = func->GetJnFunction()->GetInParamsCount() - 1;
        this->LoadHelperArgument(instrArgs, IR::IntConstOpnd::New(formalsCount, TyMachReg, func));

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
        IR::Instr *instr = this->LoadInputParamPtr(instrArgs);
        this->LoadHelperArgument(instrArgs, instr->GetDst());

        // s3 = formal argument count (without counting "this")
        uint32 formalsCount = func->GetInParamsCount() - 1;
        this->LoadHelperArgument(instrArgs, IR::IntConstOpnd::New(formalsCount, TyMachReg, func));

        // s2 = actual argument count (without counting "this")
        instr = this->lowererMD->LoadInputParamCount(instrArgs);
        instr = IR::Instr::New(Js::OpCode::DEC, instr->GetDst(), instr->GetDst(), func);
        instrArgs->InsertBefore(instr);
        this->LoadHelperArgument(instrArgs, instr->GetDst());

        // s1 = current function
        StackSym *paramSym = StackSym::New(TyMachReg, func);
        this->m_func->SetArgOffset(paramSym, 2 * MachPtr);
        IR::Opnd *srcOpnd = IR::SymOpnd::New(paramSym, TyMachReg, func);
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
        //
        // dst = current function ([ebp + 8])
        //
        StackSym *paramSym = StackSym::New(TyMachReg, func);
        this->m_func->SetArgOffset(paramSym, 2 * MachPtr);
        paramOpnd = IR::SymOpnd::New(paramSym, TyMachReg, func);
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
    // No need to do anything different for spread calls on x86 since we push args.
    IR::SymOpnd *   argOpnd     = IR::SymOpnd::New(this->m_func->m_symTable->GetArgSlotSym(1), TyVar, this->m_func);
    IR::Instr *     argInstr    = LowererMD::CreateAssign(argOpnd, dst, instr);
    return argInstr;
}

void
LowererMDArch::GenerateFunctionObjectTest(IR::Instr * callInstr, IR::RegOpnd  *functionObjOpnd, bool isHelper, IR::LabelInstr* continueAfterExLabel /* = nullptr */)
{
    AssertMsg(!m_func->IsJitInDebugMode() || continueAfterExLabel, "When jit is in debug mode, continueAfterExLabel must be provided otherwise continue after exception may cause AV.");

    if (!functionObjOpnd->IsNotTaggedValue())
    {
        IR::Instr * insertBeforeInstr = callInstr;
        // Need check and error if we are calling a tagged int.
        if (!functionObjOpnd->IsTaggedInt())
        {
            //      TEST s1, 1
            //      JEQ  $callLabel
            IR::LabelInstr * callLabel = IR::LabelInstr::New(Js::OpCode::Label, this->m_func /*, isHelper*/);
            this->lowererMD->GenerateObjectTest(functionObjOpnd, callInstr, callLabel, true);

#if DBG
            int count = 0;
            FOREACH_SLIST_ENTRY(IR::BranchInstr *, branchInstr, &callLabel->labelRefs)
            {
                branchInstr->m_isHelperToNonHelperBranch = true;
                count++;
            } NEXT_SLIST_ENTRY;
            Assert(count == 1);
#endif
            callInstr->InsertBefore(callLabel);

            insertBeforeInstr = callLabel;
        }

        lowererMD->m_lowerer->GenerateRuntimeError(insertBeforeInstr, JSERR_NeedFunction);

        if (continueAfterExLabel)
        {
            // Under debugger the RuntimeError (exception) can be ignored, generate branch right after RunTimeError instr
            // to jmp to a safe place (which would normally be debugger bailout check).
            IR::BranchInstr* continueAfterEx = IR::BranchInstr::New(LowererMD::MDUncondBranchOpcode, continueAfterExLabel, this->m_func);
            insertBeforeInstr->InsertBefore(continueAfterEx);
        }
    }
}

void
LowererMDArch::LowerInlineSpreadArgOutLoop(IR::Instr *callInstr, IR::RegOpnd *indexOpnd, IR::RegOpnd *arrayElementsStartOpnd)
{
    Func *const func = callInstr->m_func;

    // Align frame
    IR::Instr *orInstr = IR::Instr::New(Js::OpCode::OR, indexOpnd, indexOpnd, IR::IntConstOpnd::New(1, TyInt32, this->m_func), this->m_func);
    callInstr->InsertBefore(orInstr);

    IR::LabelInstr *startLoopLabel = IR::LabelInstr::New(Js::OpCode::Label, func);
    startLoopLabel->m_isLoopTop = true;
    Loop *loop = JitAnew(func->m_alloc, Loop, func->m_alloc, this->m_func);
    startLoopLabel->SetLoop(loop);
    loop->SetLoopTopInstr(startLoopLabel);
    loop->regAlloc.liveOnBackEdgeSyms = AllocatorNew(JitArenaAllocator, func->m_alloc, BVSparse<JitArenaAllocator>, func->m_alloc);
    loop->regAlloc.liveOnBackEdgeSyms->Set(indexOpnd->m_sym->m_id);
    loop->regAlloc.liveOnBackEdgeSyms->Set(arrayElementsStartOpnd->m_sym->m_id);
    callInstr->InsertBefore(startLoopLabel);

    this->lowererMD->m_lowerer->InsertSub(false, indexOpnd, indexOpnd, IR::IntConstOpnd::New(1, TyInt8, func), callInstr);

    IR::IndirOpnd *elemPtrOpnd = IR::IndirOpnd::New(arrayElementsStartOpnd, indexOpnd, GetDefaultIndirScale(), TyMachPtr, func);

    // Generate argout for n+2 arg (skipping function object + this)
    IR::Instr *argout = IR::Instr::New(Js::OpCode::ArgOut_A_Dynamic, func);
    argout->SetSrc1(elemPtrOpnd);
    callInstr->InsertBefore(argout);
    this->lowererMD->LoadDynamicArgument(argout);

    this->lowererMD->m_lowerer->InsertCompareBranch(indexOpnd,
                                                    IR::IntConstOpnd::New(0, TyUint8, func),
                                                    Js::OpCode::BrNeq_A,
                                                    true,
                                                    startLoopLabel,
                                                    callInstr);
}

IR::Instr *
LowererMDArch::LowerCallIDynamic(IR::Instr * callInstr, IR::Instr*saveThisArgOutInstr, IR::Opnd *argsLength, ushort callFlags, IR::Instr * insertBeforeInstrForCFG)
{
    callInstr->InsertBefore(saveThisArgOutInstr); //Move this Argout next to call;
    this->LoadDynamicArgument(saveThisArgOutInstr);

    Func *func = callInstr->m_func;
    bool bIsInlinee = func->IsInlinee();

    if (bIsInlinee)
    {
        Assert(argsLength->AsIntConstOpnd()->GetValue() == callInstr->m_func->actualCount);
    }
    else
    {
        Assert(argsLength->IsRegOpnd());
        /*callInfo*/
        callInstr->InsertBefore(IR::Instr::New(Js::OpCode::ADD, argsLength, argsLength, IR::IntConstOpnd::New(1, TyInt8, this->m_func), this->m_func));
    }

    IR::Instr* argout = IR::Instr::New(Js::OpCode::ArgOut_A_Dynamic, this->m_func);
    argout->SetSrc1(argsLength);
    callInstr->InsertBefore(argout);
    this->LoadDynamicArgument(argout);

    //  load native entry point from script function into eax
    AssertMsg(callInstr->GetSrc1()->IsRegOpnd() && callInstr->GetSrc1()->AsRegOpnd()->m_sym->IsStackSym(),
        "Expected call src to be stackSym");
    IR::RegOpnd * functionWrapOpnd = callInstr->UnlinkSrc1()->AsRegOpnd();
    GeneratePreCall(callInstr, functionWrapOpnd);
    LowerCall(callInstr, 0);

    //Restore stack back to original state.
    IR::RegOpnd * espOpnd = IR::RegOpnd::New(nullptr, RegESP, TyMachReg, this->m_func);
    if (bIsInlinee)
    {
        // +2 for callInfo & function object;
        IR::IndirOpnd * indirOpnd = IR::IndirOpnd::New(espOpnd, (callInstr->m_func->actualCount + (callInstr->m_func->actualCount&1) + 2) * MachPtr, TyMachReg, this->m_func);
        callInstr->InsertAfter(IR::Instr::New(Js::OpCode::LEA, espOpnd, indirOpnd, this->m_func));
    }
    else
    {
        IR::RegOpnd *argsLengthRegOpnd = argsLength->AsRegOpnd();
        //Account for callInfo & function object in argsLength
        IR::Instr * addInstr = IR::Instr::New(Js::OpCode::ADD, argsLengthRegOpnd, argsLengthRegOpnd, IR::IntConstOpnd::New(2, TyInt8, this->m_func), this->m_func);
        callInstr->InsertBefore(addInstr);

        IR::Instr *insertInstr = callInstr->m_next;

        // Align stack
        //
        // INC argLengthReg
        IR::Instr * incInstr = IR::Instr::New(Js::OpCode::INC, argsLengthRegOpnd, argsLengthRegOpnd, this->m_func);
        insertInstr->InsertBefore(incInstr);

        // AND argLengthReg, (~1)
        IR::Instr * andInstr = IR::Instr::New(Js::OpCode::AND, argsLengthRegOpnd, argsLengthRegOpnd, IR::IntConstOpnd::New(~1, TyInt32, this->m_func, true), this->m_func);
        insertInstr->InsertBefore(andInstr);

        // LEA ESP, [ESP + argsLengthReg*4]
        IR::IndirOpnd * indirOpnd = IR::IndirOpnd::New(espOpnd, argsLengthRegOpnd, IndirScale4, TyMachReg, this->m_func);
        addInstr = IR::Instr::New(Js::OpCode::LEA, espOpnd, indirOpnd, this->m_func);
        insertInstr->InsertBefore(addInstr);
    }
    return argout;
}

void
LowererMDArch::GeneratePreCall(IR::Instr * callInstr, IR::Opnd  *functionObjOpnd)
{
    IR::RegOpnd* functionTypeRegOpnd = nullptr;

    // For calls to fixed functions we load the function's type directly from the known (hard-coded) function object address.
    // For other calls, we need to load it from the function object stored in a register operand.
    if (functionObjOpnd->IsAddrOpnd() && functionObjOpnd->AsAddrOpnd()->m_isFunction)
    {
        functionTypeRegOpnd = this->lowererMD->m_lowerer->GenerateFunctionTypeFromFixedFunctionObject(callInstr, functionObjOpnd);
    }
    else if (functionObjOpnd->IsRegOpnd())
    {
        AssertMsg(functionObjOpnd->AsRegOpnd()->m_sym->IsStackSym(), "Expected call target to be stackSym");

        functionTypeRegOpnd = IR::RegOpnd::New(TyMachReg, this->m_func);

        // functionTypeRegOpnd = MOV function->type
        IR::IndirOpnd* functionTypeIndirOpnd = IR::IndirOpnd::New(functionObjOpnd->AsRegOpnd(),
            Js::RecyclableObject::GetOffsetOfType(), TyMachReg, this->m_func);
        IR::Instr* instr = IR::Instr::New(Js::OpCode::MOV, functionTypeRegOpnd, functionTypeIndirOpnd, this->m_func);
        callInstr->InsertBefore(instr);
    }
    else
    {
        AssertMsg(false, "Unexpected call target operand type.");
    }

    // Push function object
    this->LoadHelperArgument(callInstr, functionObjOpnd);

    int entryPointOffset = Js::Type::GetOffsetOfEntryPoint();
    IR::IndirOpnd* entryPointOpnd = IR::IndirOpnd::New(functionTypeRegOpnd, entryPointOffset, TyMachPtr, this->m_func);

    callInstr->SetSrc1(entryPointOpnd);

    // Atom prefers "CALL reg" over "CALL [reg]"
    IR::Instr * hoistedCallSrcInstr = nullptr;
    hoistedCallSrcInstr = callInstr->HoistSrc1(Js::OpCode::MOV);

#if defined(_CONTROL_FLOW_GUARD)
    if (!PHASE_OFF(Js::CFGInJitPhase, this->m_func))
    {
        this->lowererMD->GenerateCFGCheck(hoistedCallSrcInstr->GetDst(), callInstr);
    }
#endif
}

IR::Instr *
LowererMDArch::LowerCallI(IR::Instr *callInstr, ushort callFlags, bool isHelper, IR::Instr * insertBeforeInstrForCFG)
{
    // We need to get the calculated CallInfo in SimpleJit because that doesn't include any changes for stack alignment
    IR::IntConstOpnd *callInfo;
    int32 argCount = this->LowerCallArgs(callInstr, callFlags, 1, &callInfo);

    IR::Opnd * functionObjOpnd = callInstr->UnlinkSrc1();

    // If this is a call for new, we already pass the function operand through NewScObject,
    // which checks if the function operand is a real function or not, don't need to add a check again
    // If this is a call to a fixed function, we've already verified that the target is, indeed, a function.
    if (callInstr->m_opcode != Js::OpCode::CallIFixed && !(callFlags & Js::CallFlags_New))
    {
        AssertMsg(functionObjOpnd->IsRegOpnd() && functionObjOpnd->AsRegOpnd()->m_sym->IsStackSym(), "Expected call src to be stackSym");
        IR::LabelInstr* continueAfterExLabel = Lowerer::InsertContinueAfterExceptionLabelForDebugger(m_func, callInstr, isHelper);
        GenerateFunctionObjectTest(callInstr, functionObjOpnd->AsRegOpnd(), isHelper, continueAfterExLabel);
    }

    // Can't assert until we remove unreachable code if we have proved that it is a tagged int.
    // Assert((callFlags & Js::CallFlags_New) || !functionWrapOpnd->IsTaggedInt());
    GeneratePreCall(callInstr, functionObjOpnd);

    IR::Opnd *const finalDst = callInstr->GetDst();

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
LowererMDArch::LowerAsmJsCallE(IR::Instr *callInstr)
{
    IR::IntConstOpnd *callInfo;
    int32 argCount = this->LowerCallArgs(callInstr, Js::CallFlags_Value, 1, &callInfo);

    IR::Opnd * functionObjOpnd = callInstr->UnlinkSrc1();

    GeneratePreCall(callInstr, functionObjOpnd);

    IR::Instr* ret = this->LowerCall(callInstr, argCount);

    return ret;
}

IR::Instr *
LowererMDArch::LowerAsmJsCallI(IR::Instr * callInstr)
{

    IR::Instr * argInstr;
    int32 argCount = 0;

    // Lower args and look for StartCall

    argInstr = callInstr;

    IR::Opnd *src2 = argInstr->UnlinkSrc2();
    while (src2->IsSymOpnd())
    {
        IR::SymOpnd * argLinkOpnd = src2->AsSymOpnd();
        StackSym * argLinkSym = argLinkOpnd->m_sym->AsStackSym();
        AssertMsg(argLinkSym->IsArgSlotSym() && argLinkSym->m_isSingleDef, "Arg tree not single def...");
        argLinkOpnd->Free(m_func);

        argInstr = argLinkSym->m_instrDef;

        // Mov each arg to it's argSlot
        src2 = argInstr->UnlinkSrc2();
        lowererMD->ChangeToAssign(argInstr);
        ++argCount;
    }

    // increment again for FunctionObject
    ++argCount;

    IR::RegOpnd * argLinkOpnd = src2->AsRegOpnd();
    StackSym * argLinkSym = argLinkOpnd->m_sym->AsStackSym();
    AssertMsg(!argLinkSym->IsArgSlotSym() && argLinkSym->m_isSingleDef, "Arg tree not single def...");

    IR::Instr * startCallInstr = argLinkSym->m_instrDef;

    Assert(startCallInstr->m_opcode == Js::OpCode::StartCall);
    Assert(startCallInstr->GetSrc1()->IsIntConstOpnd());

    int32 stackAlignment = LowerStartCallAsmJs(startCallInstr, startCallInstr, callInstr);

    const uint32 argSlots = argCount + (stackAlignment / 4) + 1;
    m_func->m_argSlotsForFunctionsCalled = max(m_func->m_argSlotsForFunctionsCalled, argSlots);

    IR::Opnd * functionObjOpnd = callInstr->UnlinkSrc1();

    // we will not have function object mem ref in the case of function table calls, so we cannot calculate the call address ahead of time
    Assert(functionObjOpnd->IsRegOpnd() && functionObjOpnd->AsRegOpnd()->m_sym->IsStackSym());

    // Push function object
    IR::Instr * pushInstr = IR::Instr::New(Js::OpCode::PUSH, callInstr->m_func);

    pushInstr->SetSrc1(functionObjOpnd);
    callInstr->InsertBefore(pushInstr);

    IR::RegOpnd* functionTypeRegOpnd = IR::RegOpnd::New(TyMachReg, m_func);

    IR::IndirOpnd* functionInfoIndirOpnd = IR::IndirOpnd::New(functionObjOpnd->AsRegOpnd(), Js::RecyclableObject::GetTypeOffset(), TyMachReg, m_func);

    IR::Instr* instr = IR::Instr::New(Js::OpCode::MOV, functionTypeRegOpnd, functionInfoIndirOpnd, m_func);

    callInstr->InsertBefore(instr);

    functionInfoIndirOpnd = IR::IndirOpnd::New(functionTypeRegOpnd, Js::ScriptFunctionType::GetEntryPointInfoOffset(), TyMachReg, m_func);
    instr = IR::Instr::New(Js::OpCode::MOV, functionTypeRegOpnd, functionInfoIndirOpnd, m_func);
    callInstr->InsertBefore(instr);

    uint32 entryPointOffset = Js::ProxyEntryPointInfo::GetAddressOffset();

    IR::Opnd * entryPointOpnd = IR::IndirOpnd::New(functionTypeRegOpnd, entryPointOffset, TyMachReg, m_func);

    callInstr->SetSrc1(entryPointOpnd);

    // Atom prefers "CALL reg" over "CALL [reg]"
    IR::Instr * hoistedCallSrcInstr = callInstr->HoistSrc1(Js::OpCode::MOV);

#if defined(_CONTROL_FLOW_GUARD)
    if (!PHASE_OFF(Js::CFGInJitPhase, this->m_func))
    {
        this->lowererMD->GenerateCFGCheck(hoistedCallSrcInstr->GetDst(), callInstr);
    }
#else
    Unused(hoistedCallSrcInstr);
#endif

    IR::Instr * retInstr = callInstr;
    callInstr->m_opcode = Js::OpCode::CALL;

    if (callInstr->GetDst())
    {
        IRType dstType = callInstr->GetDst()->GetType();
        IR::Instr * movInstr = callInstr->SinkDst(GetAssignOp(dstType));

        RegNum returnReg = GetRegReturn(dstType);
        callInstr->GetDst()->AsRegOpnd()->SetReg(returnReg);
        movInstr->GetSrc1()->AsRegOpnd()->SetReg(returnReg);
        retInstr = movInstr;
    }

    return retInstr;
}
IR::Instr*
LowererMDArch::LowerAsmJsLdElemHelper(IR::Instr * instr, bool isSimdLoad /*= false*/, bool checkEndOffset /*= false*/)
{
    IR::Opnd * src1 = instr->UnlinkSrc1();
    IRType type = src1->GetType();
    IR::LabelInstr * helperLabel = Lowerer::InsertLabel(true, instr);
    IR::LabelInstr * loadLabel = Lowerer::InsertLabel(false, instr);
    IR::LabelInstr * doneLabel = Lowerer::InsertLabel(false, instr);
    IR::RegOpnd * indexOpnd = src1->AsIndirOpnd()->GetIndexOpnd();
    IR::Opnd * cmpOpnd;
    const uint8 dataWidth = instr->dataWidth;

    Assert(isSimdLoad == false || dataWidth == 4 || dataWidth == 8 || dataWidth == 12 || dataWidth == 16);

    if (indexOpnd)
    {
        cmpOpnd = indexOpnd;
    }
    else
    {
        cmpOpnd = IR::IntConstOpnd::New(src1->AsIndirOpnd()->GetOffset(), TyUint32, m_func);
    }

    // if dataWidth != byte per element, we need to check end offset
    if (isSimdLoad && checkEndOffset)
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

    if (m_func->GetJnFunction()->GetAsmJsFunctionInfo()->IsHeapBufferConst())
    {
        src1->AsIndirOpnd()->ReplaceBaseOpnd(src1->AsIndirOpnd()->UnlinkIndexOpnd());
        Js::Var* module = (Js::Var*)m_func->m_workItem->GetEntryPoint()->GetModuleAddress();
        Js::ArrayBuffer* arrayBuffer = *(Js::ArrayBuffer**)(module + Js::AsmJsModuleMemory::MemoryTableBeginOffset);
        Assert(arrayBuffer);
        src1->AsIndirOpnd()->SetOffset((uintptr)arrayBuffer->GetBuffer(), true);
    }

    if (isSimdLoad)
    {
        lowererMD->m_lowerer->GenerateRuntimeError(loadLabel, JSERR_ArgumentOutOfRange, IR::HelperOp_RuntimeRangeError);

    }
    else
    {
        if (IRType_IsFloat(type))
        {
            Lowerer::InsertMove(instr->UnlinkDst(), IR::FloatConstOpnd::New(Js::NumberConstants::NaN, type, m_func), loadLabel);
        }
        else
        {
            Lowerer::InsertMove(instr->UnlinkDst(), IR::IntConstOpnd::New(0, TyInt8, m_func), loadLabel);
        }
    }
    Lowerer::InsertBranch(Js::OpCode::Br, doneLabel, loadLabel);
    return doneLabel;
}

IR::Instr*
LowererMDArch::LowerAsmJsStElemHelper(IR::Instr * instr, bool isSimdStore /*= false*/, bool checkEndOffset /*= false*/)

{
    IR::Opnd * dst = instr->UnlinkDst();
    IR::LabelInstr * helperLabel = Lowerer::InsertLabel(true, instr);
    IR::LabelInstr * storeLabel = Lowerer::InsertLabel(false, instr);
    IR::LabelInstr * doneLabel = Lowerer::InsertLabel(false, instr);
    IR::Opnd * cmpOpnd;
    IR::RegOpnd * indexOpnd = dst->AsIndirOpnd()->GetIndexOpnd();
    const uint8 dataWidth = instr->dataWidth;

    Assert(isSimdStore == false || dataWidth == 4 || dataWidth == 8 || dataWidth == 12 || dataWidth == 16);

    if (indexOpnd)
    {
        cmpOpnd = indexOpnd;
    }
    else
    {
        cmpOpnd = IR::IntConstOpnd::New(dst->AsIndirOpnd()->GetOffset(), TyUint32, m_func);
    }
    if (isSimdStore && checkEndOffset)
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

    if (isSimdStore)
    {
        lowererMD->m_lowerer->GenerateRuntimeError(storeLabel, JSERR_ArgumentOutOfRange, IR::HelperOp_RuntimeRangeError);
    }

    Lowerer::InsertBranch(Js::OpCode::Br, storeLabel, helperLabel);

    Lowerer::InsertBranch(Js::OpCode::Br, doneLabel, storeLabel);

    if (m_func->GetJnFunction()->GetAsmJsFunctionInfo()->IsHeapBufferConst())
    {
        dst->AsIndirOpnd()->ReplaceBaseOpnd(dst->AsIndirOpnd()->UnlinkIndexOpnd());
        Js::Var* module = (Js::Var*)m_func->m_workItem->GetEntryPoint()->GetModuleAddress();
        Js::ArrayBuffer* arrayBuffer = *(Js::ArrayBuffer**)(module + Js::AsmJsModuleMemory::MemoryTableBeginOffset);
        Assert(arrayBuffer);
        dst->AsIndirOpnd()->SetOffset((uintptr)arrayBuffer->GetBuffer(), true);
    }
    return doneLabel;
}

IR::Instr *
LowererMDArch::LowerCallPut(IR::Instr *callInstr)
{
    int32 argCount = this->LowerCallArgs(callInstr, Js::CallFlags_None);

    //  load native entry point from script function into eax

    IR::Opnd * functionWrapOpnd = callInstr->UnlinkSrc1();
    AssertMsg(functionWrapOpnd->IsRegOpnd() && functionWrapOpnd->AsRegOpnd()->m_sym->IsStackSym(),
        "Expected call src to be stackSym");

    // push function wrapper

    this->LoadHelperArgument(callInstr, functionWrapOpnd);

    IR::HelperCallOpnd  *helperCallOpnd = IR::HelperCallOpnd::New(IR::HelperOp_InvokePut, this->m_func);
    callInstr->SetSrc1(helperCallOpnd);

    return this->LowerCall(callInstr, argCount);
}

int32
LowererMDArch::LowerCallArgs(IR::Instr *callInstr, ushort callFlags, Js::ArgSlot extraArgs, IR::IntConstOpnd **callInfoOpndRef)
{
    IR::Instr * argInstr;
    uint32 argCount = 0;

    // Lower args and look for StartCall

    argInstr = callInstr;

    IR::Opnd *src2 = argInstr->UnlinkSrc2();
    while (src2->IsSymOpnd())
    {
        IR::SymOpnd *   argLinkOpnd = src2->AsSymOpnd();
        StackSym *      argLinkSym  = argLinkOpnd->m_sym->AsStackSym();
        AssertMsg(argLinkSym->IsArgSlotSym() && argLinkSym->m_isSingleDef, "Arg tree not single def...");
        argLinkOpnd->Free(this->m_func);

        argInstr = argLinkSym->m_instrDef;

        // Mov each arg to it's argSlot

        src2 = argInstr->UnlinkSrc2();
        this->lowererMD->ChangeToAssign(argInstr);

        argCount++;
    }

    IR::RegOpnd * argLinkOpnd = src2->AsRegOpnd();
    StackSym *argLinkSym = argLinkOpnd->m_sym->AsStackSym();
    AssertMsg(!argLinkSym->IsArgSlotSym() && argLinkSym->m_isSingleDef, "Arg tree not single def...");

    IR::Instr *startCallInstr = argLinkSym->m_instrDef;

    if (callInstr->m_opcode == Js::OpCode::NewScObject ||
        callInstr->m_opcode == Js::OpCode::NewScObjectSpread ||
        callInstr->m_opcode == Js::OpCode::NewScObjArray ||
        callInstr->m_opcode == Js::OpCode::NewScObjArraySpread)

    {
        // These push an extra arg.
        argCount++;
    }

    AssertMsg(startCallInstr->m_opcode == Js::OpCode::StartCall || startCallInstr->m_opcode == Js::OpCode::LoweredStartCall, "Problem with arg chain.");
    AssertMsg(m_func->GetJnFunction()->GetIsAsmjsMode() || startCallInstr->GetArgOutCount(/*getInterpreterArgOutCount*/ false) == argCount, "ArgCount doesn't match StartCall count");

    //
    // Machine dependent lowering
    //

    IR::Instr * insertInstr;
    if (callInstr->IsCloned())
    {
        insertInstr = argInstr;
    }
    else
    {
        insertInstr = startCallInstr;
    }

    int32 stackAlignment;
    if (callInstr->m_opcode == Js::OpCode::AsmJsCallE)
    {
        stackAlignment = LowerStartCallAsmJs(startCallInstr, insertInstr, callInstr);
    }
    else
    {
        stackAlignment = LowerStartCall(startCallInstr, insertInstr);
    }

    startCallInstr->SetIsCloned(callInstr->IsCloned());

    // Push argCount
    IR::IntConstOpnd * argCountOpnd = Lowerer::MakeCallInfoConst(callFlags, argCount, m_func);
    if(callInfoOpndRef)
    {
        argCountOpnd->Use(m_func);
        *callInfoOpndRef = argCountOpnd;
    }
    this->LoadHelperArgument(callInstr, argCountOpnd);

    uint32 argSlots;
    argSlots = argCount + (stackAlignment / 4) + 1 + extraArgs; // + 1 for call flags
    this->m_func->m_argSlotsForFunctionsCalled = max(this->m_func->m_argSlotsForFunctionsCalled, argSlots);
    return argSlots;
}

///----------------------------------------------------------------------------
///
/// LowererMDArch::LowerCall
///
///     Machine dependent (x86) lowering for calls.
///     Adds an "ADD ESP, argCount*4" if argCount is not 0.
///
///----------------------------------------------------------------------------

IR::Instr *
LowererMDArch::LowerCall(IR::Instr * callInstr, uint32 argCount, RegNum regNum)
{
    IR::Instr *retInstr = callInstr;
    callInstr->m_opcode = Js::OpCode::CALL;

    if (callInstr->GetDst())
    {
        IR::Opnd *       dstOpnd = callInstr->GetDst();
        IRType           dstType = dstOpnd->GetType();
        Js::OpCode       assignOp = GetAssignOp(dstType);
        IR::Instr *      movInstr = nullptr;
        RegNum           reg = GetRegReturn(dstType);

        if (IRType_IsFloat(dstType))
        {
            // We should only generate this if sse2 is available
            AssertMsg(AutoSystemInfo::Data.SSE2Available(), "SSE2 not supported");

            AssertMsg(reg == RegNOREG, "No register should be assigned for float Reg");

            // We pop the Float X87 stack using FSTP for the return value of the CALL, instead of storing in XMM0 directly.

            //Before: oldDst = CALL xxx

            //After:
            // CALL xxx
            // newDstOpnd = FSTP
            // oldDst = MOVSD [newDstOpnd]

            IR::Instr * floatPopInstr = IR::Instr::New(Js::OpCode::FSTP, m_func);
            IR::Opnd * oldDst = callInstr->UnlinkDst();

            StackSym * newDstStackSym = StackSym::New(dstType, this->m_func);

            Assert(dstType == TyMachDouble);
            this->m_func->StackAllocate(newDstStackSym, MachDouble);

            IR::SymOpnd * newDstOpnd = IR::SymOpnd::New(newDstStackSym, dstType, this->m_func);

            floatPopInstr->SetDst(newDstOpnd);
            callInstr->InsertAfter(floatPopInstr);

            movInstr = IR::Instr::New(Js::OpCode::MOVSD, oldDst, newDstOpnd, this->m_func);
            floatPopInstr->InsertAfter(movInstr);
        }
        else
        {
            movInstr = callInstr->SinkDst(assignOp);
            callInstr->GetDst()->AsRegOpnd()->SetReg(reg);
            movInstr->GetSrc1()->AsRegOpnd()->SetReg(reg);
        }
        Assert(movInstr);
        retInstr = movInstr;
    }

    if (argCount)
    {
        IR::RegOpnd * espOpnd = IR::RegOpnd::New(nullptr, RegESP, TyMachReg, this->m_func);
        IR::IndirOpnd * indirOpnd = IR::IndirOpnd::New(espOpnd, argCount * MachPtr, TyMachReg, this->m_func);
        IR::Instr * addInstr = IR::Instr::New(Js::OpCode::LEA,
            espOpnd, indirOpnd, this->m_func);

        callInstr->InsertAfter(addInstr);
    }

    this->helperCallArgsCount = 0;

    return retInstr;
}

///----------------------------------------------------------------------------
///
/// LowererMDArch::LowerStartCall
///
///     Lower StartCall to a "SUB ESP, argCount * 4"
///
///----------------------------------------------------------------------------

int32
LowererMDArch::LowerStartCall(IR::Instr * startCallInstr, IR::Instr* insertInstr)
{
    AssertMsg(startCallInstr->GetSrc1()->IsIntConstOpnd(), "Bad src on StartCall");

    IR::IntConstOpnd *sizeOpnd = startCallInstr->GetSrc1()->AsIntConstOpnd();
    IntConstType sizeValue = sizeOpnd->GetValue();

    // Maintain 8 byte alignment of the stack.
    // We do this by adjusting the SUB for stackCall to make sure it maintains 8 byte alignment.
    int32 stackAlignment = Math::Align<int32>(sizeValue*MachPtr, MachStackAlignment) - sizeValue*MachPtr;

    if (stackAlignment != 0)
    {
        sizeValue += 1;
    }
    sizeValue *= MachPtr;

    IR::Instr* newStartCall;
    if ((uint32)sizeValue > AutoSystemInfo::PageSize) {

        // Convert StartCall into a chkstk
        //     mov eax, sizeOpnd->m_value
        //     call _chkstk

        IR::RegOpnd *eaxOpnd = IR::RegOpnd::New(nullptr, this->GetRegChkStkParam(), TyMachReg, this->m_func);
        this->lowererMD->CreateAssign(eaxOpnd, IR::IntConstOpnd::New(sizeValue, TyInt32, this->m_func, /*dontEncode*/true), insertInstr);

        newStartCall = IR::Instr::New(Js::OpCode::Call, this->m_func);

        newStartCall->SetSrc1(IR::HelperCallOpnd::New(IR::HelperCRT_chkstk, this->m_func));
        insertInstr->InsertBefore(newStartCall);
        this->LowerCall(newStartCall, 0);

    } else {

        // Convert StartCall into
        //     lea esp, [esp - sizeValue]

        IR::RegOpnd * espOpnd = IR::RegOpnd::New(nullptr, this->GetRegStackPointer(), TyMachReg, this->m_func);
        newStartCall = IR::Instr::New(Js::OpCode::LEA, espOpnd, IR::IndirOpnd::New(espOpnd, -sizeValue, TyMachReg, this->m_func), this->m_func);
        insertInstr->InsertBefore(newStartCall);
    }
    newStartCall->SetByteCodeOffset(startCallInstr);

    // Mark the start call as being lowered - this is required by the bailout encoding logic
    startCallInstr->m_opcode = Js::OpCode::LoweredStartCall;
    return stackAlignment;
}

int32
LowererMDArch::LowerStartCallAsmJs(IR::Instr * startCallInstr, IR::Instr * insertInstr, IR::Instr * callInstr)
{
    AssertMsg(startCallInstr->GetSrc1()->IsIntConstOpnd(), "Bad src on StartCall");

    IR::IntConstOpnd * sizeOpnd = startCallInstr->GetSrc1()->AsIntConstOpnd();

    IntConstType sizeValue = sizeOpnd->GetValue();
    if (callInstr->m_opcode == Js::OpCode::AsmJsCallI)
    {
        // we will push FunctionObject, so don't need to worry about that
        sizeValue -= MachPtr;
    }

    // Maintain 8 byte alignment of the stack.
    // We do this by adjusting the SUB for stackCall to make sure it maintains 8 byte alignment.
    int32 stackAlignment = Math::Align<int32>(sizeValue, MachStackAlignment) - sizeValue;

    if (stackAlignment != 0)
    {
        sizeValue += MachPtr;
    }

    IR::Instr* newStartCall;
    if ((uint32)sizeValue > AutoSystemInfo::PageSize) {

        // Convert StartCall into a chkstk
        //     mov eax, sizeOpnd->m_value
        //     call _chkstk

        IR::RegOpnd *eaxOpnd = IR::RegOpnd::New(nullptr, GetRegChkStkParam(), TyMachReg, m_func);
        lowererMD->CreateAssign(eaxOpnd, IR::IntConstOpnd::New(sizeValue, TyInt32, m_func, /*dontEncode*/true), insertInstr);

        newStartCall = IR::Instr::New(Js::OpCode::Call, m_func);

        newStartCall->SetSrc1(IR::HelperCallOpnd::New(IR::HelperCRT_chkstk, m_func));
        insertInstr->InsertBefore(newStartCall);
        LowerCall(newStartCall, 0);

    }
    else {
        // Convert StartCall into
        //     lea esp, [esp - sizeValue]

        IR::RegOpnd * espOpnd = IR::RegOpnd::New(nullptr, this->GetRegStackPointer(), TyMachReg, m_func);
        newStartCall = IR::Instr::New(Js::OpCode::LEA, espOpnd, IR::IndirOpnd::New(espOpnd, -sizeValue, TyMachReg, m_func), m_func);
        insertInstr->InsertBefore(newStartCall);
    }
    newStartCall->SetByteCodeOffset(startCallInstr);

    // Mark the start call as being lowered - this is required by the bailout encoding logic
    startCallInstr->m_opcode = Js::OpCode::LoweredStartCall;
    return stackAlignment;
}

///----------------------------------------------------------------------------
///
/// LowererMDArch::LoadHelperArgument
///
///     Change to a PUSH.
///
///----------------------------------------------------------------------------

IR::Instr *
LowererMDArch::LoadHelperArgument(IR::Instr * instr, IR::Opnd * opndArg)
{
    IR::Instr * pushInstr;

    pushInstr = IR::Instr::New(Js::OpCode::PUSH, instr->m_func);
    if(TySize[opndArg->GetType()] < TySize[TyMachReg])
    {
        Assert(!opndArg->IsMemoryOpnd()); // if it's a memory opnd, it would need to be loaded into a register first
        opndArg = opndArg->UseWithNewType(TyMachReg, instr->m_func);
    }
    pushInstr->SetSrc1(opndArg);
    instr->InsertBefore(pushInstr);

    this->helperCallArgsCount++;
    AssertMsg(helperCallArgsCount <= LowererMDArch::MaxArgumentsToHelper, "The # of arguments to the helper is too big.");

    return pushInstr;
}

IR::Instr *
LowererMDArch::LoadDynamicArgument(IR::Instr * instr, uint argNumber /*ignore for x86*/)
{
    //Convert to push instruction.
    instr->m_opcode = Js::OpCode::PUSH;
    return instr;
}


IR::Instr *
LowererMDArch::LoadDoubleHelperArgument(IR::Instr * instrInsert, IR::Opnd * opndArg)
{
    IR::Instr * instrPrev;
    IR::Instr * instr;
    IR::Opnd * opnd;
    IR::Opnd * float64Opnd;
    IR::RegOpnd * espOpnd = IR::RegOpnd::New(nullptr, this->GetRegStackPointer(), TyMachReg, this->m_func);

    opnd = IR::IndirOpnd::New(espOpnd, -8, TyMachReg, this->m_func);
    instrPrev = IR::Instr::New(Js::OpCode::LEA, espOpnd, opnd, this->m_func);
    instrInsert->InsertBefore(instrPrev);

    opnd = IR::IndirOpnd::New(espOpnd, (int32)0, TyFloat64, this->m_func);

    if (opndArg->GetType() == TyFloat32)
    {
        float64Opnd = IR::RegOpnd::New(TyFloat64, m_func);
        instr = IR::Instr::New(Js::OpCode::CVTSS2SD, float64Opnd, opndArg, this->m_func);
        instrInsert->InsertBefore(instr);
    }
    else
    {
        float64Opnd = opndArg;
    }
    instr = IR::Instr::New(Js::OpCode::MOVSD, opnd, float64Opnd, this->m_func);
    instrInsert->InsertBefore(instr);
    LowererMD::Legalize(instr);
    return instrPrev;
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
#ifdef ENABLE_DEBUG_CONFIG_OPTIONS
    if (Js::Configuration::Global.flags.IsEnabled(Js::CheckAlignmentFlag))
    {
        IR::Instr * callInstr = IR::Instr::New(Js::OpCode::Call, this->m_func);
        callInstr->SetSrc1(IR::HelperCallOpnd::New(IR::HelperScrFunc_CheckAlignment, this->m_func));
        entryInstr->InsertAfter(callInstr);

        this->LowerCall(callInstr, 0, RegEAX);
    }
#endif

    int32 bytesOnStack = MachRegInt+MachRegInt;  // Account for return address+push EBP...

    // PUSH used callee-saved registers

    for (RegNum reg = (RegNum)(RegNOREG + 1); reg < RegNumCount; reg = (RegNum)(reg+1))
    {
        if (LinearScan::IsCalleeSaved(reg) && (this->m_func->m_regsUsed.Test(reg)))
        {
            IR::RegOpnd * regOpnd = IR::RegOpnd::New(nullptr, reg, TyMachReg, this->m_func);
            IR::Instr * pushInstr = IR::Instr::New(Js::OpCode::PUSH, this->m_func);
            pushInstr->SetSrc1(regOpnd);
            entryInstr->InsertAfter(pushInstr);
            bytesOnStack += MachRegInt;
        }
    }

    // Allocate frame

    IR::RegOpnd * ebpOpnd = IR::RegOpnd::New(nullptr, this->GetRegBlockPointer(), TyMachReg, this->m_func);
    IR::RegOpnd * espOpnd = IR::RegOpnd::New(nullptr, this->GetRegStackPointer(), TyMachReg, this->m_func);

    // Dedicated argument slot is already included in the m_localStackHeight (see Func ctor)

    // Allocate the inlined arg out stack in the locals. Allocate an additional slot so that
    // we can unconditionally clear the argc slot of the next frame.
    this->m_func->m_localStackHeight += ((this->m_func->GetMaxInlineeArgOutCount() + 1) * MachPtr);

    bytesOnStack += this->m_func->m_localStackHeight;

    int32 alignment = Math::Align<int32>(bytesOnStack, MachStackAlignment) - bytesOnStack;
    // Make sure this frame allocation maintains 8-byte alignment.  Our point of reference is the return address
    this->m_func->m_localStackHeight += alignment;
    bytesOnStack += alignment;

    Assert(Math::Align<int32>(bytesOnStack, MachStackAlignment) == bytesOnStack);

    Assert(this->m_func->hasBailout || this->bailOutStackRestoreLabel == nullptr);
    this->m_func->frameSize = bytesOnStack;

    if (this->m_func->GetMaxInlineeArgOutCount())
    {
        this->m_func->m_workItem->GetFunctionBody()->SetFrameHeight(this->m_func->m_workItem->GetEntryPoint(), this->m_func->m_localStackHeight);
    }

    // Zero initialize the first inlinee frames argc.
    if (this->m_func->GetMaxInlineeArgOutCount())
    {
        StackSym *sym           = this->m_func->m_symTable->GetArgSlotSym((Js::ArgSlot)-1);
        sym->m_isInlinedArgSlot = true;
        sym->m_offset           = 0;
        IR::Opnd *dst           = IR::SymOpnd::New(sym, TyMachReg, this->m_func);
        entryInstr->InsertAfter(IR::Instr::New(Js::OpCode::MOV,
                                               dst,
                                               IR::AddrOpnd::NewNull(this->m_func),
                                               this->m_func));
    }

    if (this->m_func->m_localStackHeight != 0)
    {
        int32 stackSize = this->m_func->m_localStackHeight;
        if (this->m_func->HasArgumentSlot())
        {
            // We separately push the stack argument slot below
            stackSize -= MachPtr;
        }

        if (this->m_func->m_localStackHeight <= PAGESIZE)
        {
            // Generate LEA ESP, [esp - stackSize]   // Atom prefers LEA for address computations

            IR::IndirOpnd *indirOpnd = IR::IndirOpnd::New(espOpnd, -stackSize, TyMachReg, this->m_func);
            IR::Instr * subInstr = IR::Instr::New(Js::OpCode::LEA, espOpnd, indirOpnd, this->m_func);

            entryInstr->InsertAfter(subInstr);
        }
        else
        {
            // Generate chkstk call

            IR::RegOpnd *eaxOpnd = IR::RegOpnd::New(nullptr, this->GetRegChkStkParam(), TyMachReg, this->m_func);
            IR::Instr * callInstr = IR::Instr::New(Js::OpCode::Call, eaxOpnd,
                IR::HelperCallOpnd::New(IR::HelperCRT_chkstk, this->m_func), this->m_func);
            entryInstr->InsertAfter(callInstr);

            this->LowerCall(callInstr, 0, RegECX);

            IR::IntConstOpnd * stackSizeOpnd = IR::IntConstOpnd::New(stackSize, TyMachReg, this->m_func);
            this->lowererMD->CreateAssign(eaxOpnd, stackSizeOpnd, entryInstr->m_next);
        }
    }

    // Zero-initialize dedicated arguments slot

    if (this->m_func->HasArgumentSlot())
    {
        IR::Instr * pushInstr = IR::Instr::New(Js::OpCode::PUSH, this->m_func);
        pushInstr->SetSrc1(IR::IntConstOpnd::New(0, TyMachPtr, this->m_func));
        entryInstr->InsertAfter(pushInstr);
    }

    size_t frameSize = bytesOnStack + ((this->m_func->m_argSlotsForFunctionsCalled + 1) * MachPtr) + Js::Constants::MinStackJIT;
    this->GeneratePrologueStackProbe(entryInstr, frameSize);

    IR::Instr * movInstr = IR::Instr::New(Js::OpCode::MOV, ebpOpnd, espOpnd, this->m_func);
    entryInstr->InsertAfter(movInstr);

    // Generate PUSH EBP

    IR::Instr * pushInstr = IR::Instr::New(Js::OpCode::PUSH, this->m_func);
    pushInstr->SetSrc1(ebpOpnd);
    entryInstr->InsertAfter(pushInstr);

    return entryInstr;
}

IR::Instr *
LowererMDArch::LowerEntryInstrAsmJs(IR::EntryInstr * entryInstr)
{
    // PUSH EBP
    // MOV EBP, ESP
    // StackProbe
    // MOV EAX, LocalStackHeight / LEA ESP, [ESP - stackSize]
    // CALL chkstk               /
    // PUSH used nonvolatiles

    // Calculate stack size
    int32 bytesOnStack = MachRegInt + MachRegInt;  // Account for return address+push EBP...

    for (RegNum reg = (RegNum)(RegNOREG + 1); reg < RegNumCount; reg = (RegNum)(reg + 1))
    {
        if (LinearScan::IsCalleeSaved(reg) && (m_func->m_regsUsed.Test(reg)))
        {
            bytesOnStack += MachRegInt;
        }
    }
    m_func->m_localStackHeight += MachPtr;
    bytesOnStack += m_func->m_localStackHeight;

    int32 alignment = Math::Align<int32>(bytesOnStack, MachStackAlignment) - bytesOnStack;

    // Make sure this frame allocation maintains 8-byte alignment.  Our point of reference is the return address
    m_func->m_localStackHeight += alignment;
    bytesOnStack += alignment;
    Assert(Math::Align<int32>(bytesOnStack, MachStackAlignment) == bytesOnStack);

    m_func->frameSize = bytesOnStack;


    IR::Instr * insertInstr = entryInstr->m_next;

    IR::RegOpnd * ebpOpnd = IR::RegOpnd::New(nullptr, GetRegBlockPointer(), TyMachReg, m_func);
    IR::RegOpnd * espOpnd = IR::RegOpnd::New(nullptr, GetRegStackPointer(), TyMachReg, m_func);

    // Generate PUSH EBP
    IR::Instr * pushInstr = IR::Instr::New(Js::OpCode::PUSH, m_func);
    pushInstr->SetSrc1(ebpOpnd);
    insertInstr->InsertBefore(pushInstr);

    // Generate MOV EBP, ESP
    IR::Instr * movInstr = IR::Instr::New(Js::OpCode::MOV, ebpOpnd, espOpnd, m_func);
    insertInstr->InsertBefore(movInstr);

    // Generate StackProbe
    size_t frameSize = bytesOnStack + m_func->m_argSlotsForFunctionsCalled * MachPtr + Js::Constants::MinStackJIT;
    GeneratePrologueStackProbe(insertInstr->m_prev, frameSize);

    if (m_func->m_localStackHeight != 0)
    {
        int32 stackSize = m_func->m_localStackHeight - MachPtr;
        if (m_func->m_localStackHeight <= PAGESIZE)
        {
            // Generate LEA ESP, [ESP - LocalStackHeight]   // Atom prefers LEA for address computations

            IR::IndirOpnd *indirOpnd = IR::IndirOpnd::New(espOpnd, -stackSize, TyMachReg, m_func);
            IR::Instr * subInstr = IR::Instr::New(Js::OpCode::LEA, espOpnd, indirOpnd, m_func);

            insertInstr->InsertBefore(subInstr);
        }
        else
        {
            IR::RegOpnd *eaxOpnd = IR::RegOpnd::New(nullptr, GetRegChkStkParam(), TyMachReg, m_func);

            // Generate MOV EAX, LocalStackHeight
            IR::IntConstOpnd * stackSizeOpnd = IR::IntConstOpnd::New(stackSize, TyMachReg, m_func);
            lowererMD->CreateAssign(eaxOpnd, stackSizeOpnd, insertInstr);

            // Generate CALL chkstk
            IR::Instr * callInstr = IR::Instr::New(Js::OpCode::Call, eaxOpnd,
                IR::HelperCallOpnd::New(IR::HelperCRT_chkstk, m_func), m_func);
            insertInstr->InsertBefore(callInstr);

            LowerCall(callInstr, 0, RegECX);

        }
    }

    // PUSH used callee-saved registers

    for (RegNum reg = (RegNum)(RegNumCount - 1); reg > RegNOREG; reg = (RegNum)(reg - 1))
    {
        if (LinearScan::IsCalleeSaved(reg) && (m_func->m_regsUsed.Test(reg)))
        {
            IR::RegOpnd * regOpnd = IR::RegOpnd::New(nullptr, reg, TyMachReg, m_func);
            IR::Instr * pushInstr = IR::Instr::New(Js::OpCode::PUSH, m_func);
            pushInstr->SetSrc1(regOpnd);
            insertInstr->InsertBefore(pushInstr);
        }
    }

#ifdef ENABLE_DEBUG_CONFIG_OPTIONS
    if (Js::Configuration::Global.flags.IsEnabled(Js::CheckAlignmentFlag))
    {
        // CALL CheckAlignment
        IR::Instr * callInstr = IR::Instr::New(Js::OpCode::Call, m_func);
        callInstr->SetSrc1(IR::HelperCallOpnd::New(IR::HelperScrFunc_CheckAlignment, m_func));
        insertInstr->InsertBefore(callInstr);

        LowerCall(callInstr, 0, RegEAX);
    }
#endif

    return entryInstr;
}

void
LowererMDArch::GeneratePrologueStackProbe(IR::Instr *entryInstr, size_t frameSize)
{
    //
    // Generate a stack overflow check. This can be as simple as a cmp esp, const
    // because this function is guaranteed to be called on its base thread only.
    // If the check fails call ThreadContext::ProbeCurrentStack which will check again and throw if needed.
    //
    //       cmp  esp, ThreadContext::scriptStackLimit + frameSize
    //       jg   done
    //       push frameSize
    //       call ThreadContext::ProbeCurrentStack

    // For thread-agile thread context
    //       mov  eax, [ThreadContext::stackLimitForCurrentThread]
    //       add  eax, frameSize
    //       cmp  esp, eax
    //       jg   done
    //       push frameSize
    //       call ThreadContext::ProbeCurrentStack
    // done:
    //

    // For thread context with script interrupt enabled:
    //       mov  eax, [ThreadContext::stackLimitForCurrentThread]
    //       add  eax, frameSize
    //       jo   $helper
    //       cmp  esp, eax
    //       jg   done
    // $helper:
    //       push frameSize
    //       call ThreadContext::ProbeCurrentStack
    // done:
    //

    IR::LabelInstr *helperLabel = IR::LabelInstr::New(Js::OpCode::Label, this->m_func, true);
    IR::Instr *insertInstr = entryInstr->m_next;
    IR::Instr *instr;
    IR::Opnd *stackLimitOpnd;
    ThreadContext *threadContext = this->m_func->GetScriptContext()->GetThreadContext();
    bool doInterruptProbe = threadContext->DoInterruptProbe(this->m_func->GetJnFunction());

    if (doInterruptProbe || !threadContext->GetIsThreadBound())
    {
        // Load the current stack limit from the ThreadContext, then increment this value by the size of the
        // current frame. This is the value we'll compare against below.

        stackLimitOpnd = IR::RegOpnd::New(nullptr, RegEAX, TyMachReg, this->m_func);
        void *pLimit = threadContext->GetAddressOfStackLimitForCurrentThread();
        IR::MemRefOpnd * memOpnd = IR::MemRefOpnd::New(pLimit, TyMachReg, this->m_func);
        this->lowererMD->CreateAssign(stackLimitOpnd, memOpnd, insertInstr);

        instr = IR::Instr::New(Js::OpCode::ADD, stackLimitOpnd, stackLimitOpnd,
                               IR::AddrOpnd::New((void*)frameSize, IR::AddrOpndKindConstant, this->m_func), this->m_func);
        insertInstr->InsertBefore(instr);

        if (doInterruptProbe)
        {
            // If this add overflows, then we need to call out to the helper.
            instr = IR::BranchInstr::New(Js::OpCode::JO, helperLabel, this->m_func);
            insertInstr->InsertBefore(instr);
        }
    }
    else
    {
        // The incremented stack limit is a compile-time constant.
        size_t scriptStackLimit = (size_t)threadContext->GetScriptStackLimit();
        stackLimitOpnd = IR::AddrOpnd::New((void *)(frameSize + scriptStackLimit), IR::AddrOpndKindDynamicMisc, this->m_func);
    }

    IR::LabelInstr *doneLabel = IR::LabelInstr::New(Js::OpCode::Label, this->m_func, false);
    if (!IS_FAULTINJECT_STACK_PROBE_ON) // Do stack check fastpath only if not doing StackProbe fault injection
    {
        instr = IR::Instr::New(Js::OpCode::CMP, this->m_func);
        instr->SetSrc1(IR::RegOpnd::New(nullptr, GetRegStackPointer(), TyMachReg, this->m_func));
        instr->SetSrc2(stackLimitOpnd);
        insertInstr->InsertBefore(instr);

        instr = IR::BranchInstr::New(Js::OpCode::JGT, doneLabel, this->m_func);
        insertInstr->InsertBefore(instr);
    }

    insertInstr->InsertBefore(helperLabel);

    // Make sure we have zero where we expect to find the stack nested func pointer relative to EBP.
    LoadHelperArgument(insertInstr, IR::IntConstOpnd::New(0, TyMachReg, m_func));
    LoadHelperArgument(insertInstr, IR::IntConstOpnd::New(0, TyMachReg, m_func));

    // Load the arguments to the probe helper and do the call.
    lowererMD->m_lowerer->LoadScriptContext(insertInstr);
    this->lowererMD->LoadHelperArgument(
        insertInstr, IR::AddrOpnd::New((void*)frameSize, IR::AddrOpndKindConstant, this->m_func));

    instr = IR::Instr::New(Js::OpCode::Call, this->m_func);
    instr->SetSrc1(IR::HelperCallOpnd::New(IR::HelperProbeCurrentStack2, this->m_func));
    insertInstr->InsertBefore(instr);
    this->LowerCall(instr, 0, RegEAX);

    insertInstr->InsertBefore(doneLabel);
    Security::InsertRandomFunctionPad(doneLabel);
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
    exitInstr = LowerExitInstrCommon(exitInstr);

    // Insert RET
    IR::IntConstOpnd * intSrc = IR::IntConstOpnd::New(0, TyMachReg, this->m_func);
    IR::RegOpnd *eaxReg = IR::RegOpnd::New(nullptr, this->GetRegReturn(TyMachReg), TyMachReg, this->m_func);
    IR::Instr *retInstr = IR::Instr::New(Js::OpCode::RET, this->m_func);
    retInstr->SetSrc1(intSrc);
    retInstr->SetSrc2(eaxReg);
    exitInstr->InsertBefore(retInstr);

    return exitInstr;
}

IR::Instr *
LowererMDArch::LowerExitInstrAsmJs(IR::ExitInstr * exitInstr)
{
    exitInstr = LowerExitInstrCommon(exitInstr);

    // get asm.js return type
    Js::AsmJsRetType asmRetType = m_func->GetJnFunction()->GetAsmJsFunctionInfo()->GetReturnType();
    IRType regType;
    if (asmRetType.which() == Js::AsmJsRetType::Double)
    {
        regType = TyFloat64;
    }
    else if (asmRetType.which() == Js::AsmJsRetType::Float)
    {
        regType = TyFloat32;
    }
    else if (asmRetType.toVarType().isFloat32x4())
    {
        regType = TySimd128F4;
    }
    else if (asmRetType.toVarType().isInt32x4())
    {
        regType = TySimd128I4;
    }
    else if (asmRetType.toVarType().isFloat64x2())
    {
        regType = TySimd128D2;
    }
    else
    {
        Assert(asmRetType.which() == Js::AsmJsRetType::Signed || asmRetType.which() == Js::AsmJsRetType::Void);
        regType = TyInt32;
    }
    if (m_func->IsLoopBody())
    {
        // Insert RET
        IR::IntConstOpnd * intSrc = IR::IntConstOpnd::New(0, TyMachReg, this->m_func);
        IR::RegOpnd *eaxReg = IR::RegOpnd::New(nullptr, this->GetRegReturn(TyMachReg), TyMachReg, this->m_func);
        IR::Instr *retInstr = IR::Instr::New(Js::OpCode::RET, this->m_func);
        retInstr->SetSrc1(intSrc);
        retInstr->SetSrc2(eaxReg);
        exitInstr->InsertBefore(retInstr);
    }
    else
    {

        // Generate RET
        int32 alignedSize = Math::Align<int32>(m_func->GetJnFunction()->GetAsmJsFunctionInfo()->GetArgByteSize(), MachStackAlignment);
        IR::IntConstOpnd * intSrc = IR::IntConstOpnd::New(alignedSize + MachPtr, TyMachReg, m_func);
        IR::RegOpnd * retReg = IR::RegOpnd::New(nullptr, GetRegReturnAsmJs(regType), regType, m_func);
        IR::Instr *retInstr = IR::Instr::New(Js::OpCode::RET, m_func);
        retInstr->SetSrc1(intSrc);
        retInstr->SetSrc2(retReg);
        exitInstr->InsertBefore(retInstr);
    }

    return exitInstr;
}

IR::ExitInstr *
LowererMDArch::LowerExitInstrCommon(IR::ExitInstr * exitInstr)
{
    IR::RegOpnd * ebpOpnd = IR::RegOpnd::New(nullptr, GetRegBlockPointer(), TyMachReg, m_func);
    IR::RegOpnd * espOpnd = IR::RegOpnd::New(nullptr, GetRegStackPointer(), TyMachReg, m_func);

    // POP used callee-saved registers

    for (RegNum reg = (RegNum)(RegNOREG + 1); reg < RegNumCount; reg = (RegNum)(reg + 1))
    {
        if (LinearScan::IsCalleeSaved(reg) && (m_func->m_regsUsed.Test(reg)))
        {
            IR::RegOpnd * regOpnd = IR::RegOpnd::New(nullptr, reg, TyMachReg, m_func);
            IR::Instr * popInstr = IR::Instr::New(Js::OpCode::POP, regOpnd, m_func);
            exitInstr->InsertBefore(popInstr);
        }
    }

    // Restore frame
    // Generate MOV ESP, EBP

    IR::Instr * movInstr = IR::Instr::New(Js::OpCode::MOV, espOpnd, ebpOpnd, m_func);
    exitInstr->InsertBefore(movInstr);

    // Generate POP EBP

    IR::Instr * pushInstr = IR::Instr::New(Js::OpCode::POP, ebpOpnd, m_func);
    exitInstr->InsertBefore(pushInstr);

    return exitInstr;
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
LowererMDArch::EmitInt4Instr(IR::Instr *instr)
{
    IR::Instr *newInstr;
    IR::Opnd *src1, *src2;
    IR::RegOpnd *regEDX;

    bool legalize = false;
    switch(instr->m_opcode)
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
        instr->SinkDst(Js::OpCode::MOV, RegEAX);
        goto idiv_common;
    case Js::OpCode::Rem_I4:
        instr->SinkDst(Js::OpCode::MOV, RegEDX);
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
        instr->HoistSrc1(Js::OpCode::MOV, RegEAX);
        regEDX = IR::RegOpnd::New(TyInt32, instr->m_func);
        regEDX->SetReg(RegEDX);

        if (instr->GetSrc1()->GetType() == TyUint32)
        {
            // we need to ensure that register allocator doesn't muck about with edx
            instr->HoistSrc2(Js::OpCode::MOV, RegECX);

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

void
LowererMDArch::EmitLoadVar(IR::Instr *instrLoad, bool isFromUint32, bool isHelper)
{
    //  s2 = MOV src1
    //  s2 = SHL s2, Js::VarTag_Shift  -- restore the var tag on the result
    //       JO  $ToVar
    //       JB  $ToVar     [isFromUint32]
    //  s2 = INC s2
    // dst = MOV s2
    //       JMP $done
    //$ToVar:
    //       EmitLoadVarNoCheck
    //$Done:

    AssertMsg(instrLoad->GetSrc1()->IsRegOpnd(), "Should be regOpnd");
    bool isInt = false;
    bool isNotInt = false;
    IR::RegOpnd *src1 = instrLoad->GetSrc1()->AsRegOpnd();
    IR::LabelInstr *labelToVar = nullptr;
    IR::LabelInstr *labelDone = nullptr;
    IR::Instr *instr;

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
        // s2 = MOV s1

        IR::Opnd * opnd32src1 = src1->UseWithNewType(TyInt32, this->m_func);
        IR::RegOpnd * opndReg2 = IR::RegOpnd::New(TyMachReg, this->m_func);
        IR::Opnd * opnd32Reg2 = opndReg2->UseWithNewType(TyInt32, this->m_func);

        instr = IR::Instr::New(Js::OpCode::MOV, opnd32Reg2, opnd32src1, this->m_func);
        instrLoad->InsertBefore(instr);

        // s2 = SHL s2, Js::VarTag_Shift  -- restore the var tag on the result

        instr = IR::Instr::New(Js::OpCode::SHL, opnd32Reg2, opnd32Reg2,
            IR::IntConstOpnd::New(Js::VarTag_Shift, TyInt8, this->m_func),
            this->m_func);
        instrLoad->InsertBefore(instr);

        if (!isInt)
        {
            //      JO  $ToVar
            labelToVar = IR::LabelInstr::New(Js::OpCode::Label, this->m_func, true);
            instr = IR::BranchInstr::New(Js::OpCode::JO, labelToVar, this->m_func);
            instrLoad->InsertBefore(instr);

            if (isFromUint32)
            {
                //       JB  $ToVar     [isFromUint32]
                instr = IR::BranchInstr::New(Js::OpCode::JB, labelToVar, this->m_func);
                instrLoad->InsertBefore(instr);
            }
        }

        // s2 = INC s2

        instr = IR::Instr::New(Js::OpCode::INC, opndReg2, opndReg2, this->m_func);
        instrLoad->InsertBefore(instr);

        // dst = MOV s2

        instr = IR::Instr::New(Js::OpCode::MOV, instrLoad->GetDst(), opndReg2, this->m_func);
        instrLoad->InsertBefore(instr);

        if (!isInt)
        {
            //      JMP $done

            labelDone = IR::LabelInstr::New(Js::OpCode::Label, this->m_func, isHelper);
            instr = IR::BranchInstr::New(Js::OpCode::JMP, labelDone, this->m_func);
            instrLoad->InsertBefore(instr);
        }
    }

    if (!isInt)
    {
        //$ToVar:
        if (labelToVar)
        {
            instrLoad->InsertBefore(labelToVar);
        }

        this->lowererMD->EmitLoadVarNoCheck(instrLoad->GetDst()->AsRegOpnd(), src1, instrLoad, isFromUint32, isHelper || labelToVar != nullptr);
    }
    //$Done:
    if (labelDone)
    {
        instrLoad->InsertAfter(labelDone);
    }
    instrLoad->Remove();
}

void
LowererMDArch::EmitIntToFloat(IR::Opnd *dst, IR::Opnd *src, IR::Instr *instrInsert)
{
    // We should only generate this if sse2 is available
    Assert(AutoSystemInfo::Data.SSE2Available());

    Assert(dst->IsRegOpnd() && dst->IsFloat());
    Assert(src->IsRegOpnd() && (src->GetType() == TyInt32 || src->GetType() == TyUint32));

    instrInsert->InsertBefore(IR::Instr::New(dst->IsFloat64() ? Js::OpCode::CVTSI2SD : Js::OpCode::CVTSI2SS, dst, src, this->m_func));
}

void
LowererMDArch::EmitUIntToFloat(IR::Opnd *dst, IR::Opnd *src, IR::Instr *instrInsert)
{
    // We should only generate this if sse2 is available
    Assert(AutoSystemInfo::Data.SSE2Available());

    this->lowererMD->EmitIntToFloat(dst, src, instrInsert);

    IR::RegOpnd * highestBitOpnd = IR::RegOpnd::New(TyInt32, this->m_func);
    IR::Instr * instr = IR::Instr::New(Js::OpCode::MOV, highestBitOpnd, src, this->m_func);
    instrInsert->InsertBefore(instr);

    instr = IR::Instr::New(Js::OpCode::SHR, highestBitOpnd, highestBitOpnd,
        IR::IntConstOpnd::New(31, TyInt8, this->m_func, true), this->m_func);
    instrInsert->InsertBefore(instr);

    // TODO: Encode indir with base as address opnd instead
    IR::RegOpnd * baseOpnd = IR::RegOpnd::New(TyMachPtr, this->m_func);

    instr = IR::Instr::New(Js::OpCode::MOV, baseOpnd, IR::AddrOpnd::New((Js::Var)&Js::JavascriptNumber::UIntConvertConst,
        IR::AddrOpndKindDynamicMisc, this->m_func), this->m_func);

    instrInsert->InsertBefore(instr);

    instr = IR::Instr::New(Js::OpCode::ADDSD, dst, dst, IR::IndirOpnd::New(baseOpnd,
        highestBitOpnd, IndirScale8, TyFloat64, this->m_func), this->m_func);
    instrInsert->InsertBefore(instr);
}

bool
LowererMDArch::EmitLoadInt32(IR::Instr *instrLoad)
{
    // if(doShiftFirst)
    // {
    //     r1 = MOV src1
    //     r1 = SAR r1, VarTag_Shift (move last-shifted bit into CF)
    //          JAE (CF == 0) $helper or $float
    // }
    // else
    // {
    //          TEST src1, AtomTag
    //          JEQ $helper or $float
    //     r1 = MOV src1
    //     r1 = SAR r1, VarTag_Shift
    // }
    //    dst = MOV r1
    //          JMP $Done
    // $float:
    //    dst = ConvertToFloat(src1, $helper)
    // $Helper
    //    dst = ToInt32(src1)
    // $Done

    AssertMsg(instrLoad->GetSrc1()->IsRegOpnd(), "Should be regOpnd");
    bool isInt = false;
    bool isNotInt = false;
    IR::RegOpnd *src1 = instrLoad->GetSrc1()->AsRegOpnd();
    IR::LabelInstr *labelHelper = nullptr;
    IR::LabelInstr *labelDone = nullptr;
    IR::LabelInstr* labelFloat = nullptr;
    IR::Instr *instr;

    if (src1->IsTaggedInt())
    {
        isInt = true;
    }
    else if (src1->IsNotInt())
    {
        isNotInt = true;
    }

    const ValueType src1ValueType(src1->GetValueType());
    const bool doShiftFirst = src1ValueType.IsLikelyTaggedInt(); // faster to shift and check flags if it's likely tagged
    const bool doFloatToIntFastPath =
        (src1ValueType.IsLikelyFloat() || src1ValueType.IsLikelyUntaggedInt()) &&
        !(instrLoad->HasBailOutInfo() && (instrLoad->GetBailOutKind() == IR::BailOutIntOnly || instrLoad->GetBailOutKind() == IR::BailOutExpectingInteger)) &&
        AutoSystemInfo::Data.SSE2Available();

    IR::RegOpnd * r1 = nullptr;
    if(doShiftFirst)
    {
        // r1 = MOV src1
        r1 = IR::RegOpnd::New(TyVar, instrLoad->m_func);
        r1->SetValueType(src1->GetValueType());
        instr = IR::Instr::New(Js::OpCode::MOV, r1, src1, instrLoad->m_func);
        instrLoad->InsertBefore(instr);
    }

    // It could be an integer in this case
    if (!isNotInt)
    {
        if(doShiftFirst)
        {
            // r1 = SAR r1, VarTag_Shift (move last-shifted bit into CF)
            Assert(r1);
            instr = IR::Instr::New(Js::OpCode::SAR, r1, r1,
                IR::IntConstOpnd::New(Js::VarTag_Shift, TyInt8, instrLoad->m_func), instrLoad->m_func);
            instrLoad->InsertBefore(instr);
        }

        // We do not know for sure it is an integer - add a Smint test
        if (!isInt)
        {
            if(doFloatToIntFastPath)
            {
                labelFloat = IR::LabelInstr::New(Js::OpCode::Label, this->m_func, false);
            }
            else
            {
                labelHelper = IR::LabelInstr::New(Js::OpCode::Label, this->m_func, true);
            }

            if(doShiftFirst)
            {
                // JAE (CF == 0) $helper or $float
                instrLoad->InsertBefore(
                    IR::BranchInstr::New(Js::OpCode::JAE, labelFloat ? labelFloat : labelHelper, this->m_func));
            }
            else
            {
                // TEST src1, AtomTag
                // JEQ $helper or $float
                this->lowererMD->GenerateSmIntTest(src1, instrLoad, labelFloat ? labelFloat : labelHelper);
            }
        }

        if(!doShiftFirst)
        {
            if(src1->IsEqual(instrLoad->GetDst()))
            {
                // Go ahead and change src1, since it was already confirmed that we won't bail out or go to helper where src1
                // may be used
                r1 = src1;
            }
            else
            {
                // r1 = MOV src1
                Assert(!r1);
                r1 = IR::RegOpnd::New(TyVar, instrLoad->m_func);
                r1->SetValueType(src1->GetValueType());
                instr = IR::Instr::New(Js::OpCode::MOV, r1, src1, instrLoad->m_func);
                instrLoad->InsertBefore(instr);
            }

            // r1 = SAR r1, VarTag_Shift
            Assert(r1);
            instr = IR::Instr::New(Js::OpCode::SAR, r1, r1,
                IR::IntConstOpnd::New(Js::VarTag_Shift, TyInt8, instrLoad->m_func), instrLoad->m_func);
            instrLoad->InsertBefore(instr);
        }

        // dst = MOV r1
        Assert(r1);
        instr = IR::Instr::New(Js::OpCode::MOV, instrLoad->GetDst(), r1, instrLoad->m_func);
        instrLoad->InsertBefore(instr);

        if (!isInt)
        {
            // JMP $Done
            labelDone = instrLoad->GetOrCreateContinueLabel();
            instr = IR::BranchInstr::New(Js::OpCode::JMP, labelDone, this->m_func);
            instrLoad->InsertBefore(instr);
        }
    }
    // if it is not an int - we need to convert.
    if (!isInt)
    {
        if(doFloatToIntFastPath)
        {
            if(labelFloat)
            {
                instrLoad->InsertBefore(labelFloat);
            }

            if(!labelHelper)
            {
                labelHelper = IR::LabelInstr::New(Js::OpCode::Label, this->m_func, true);
            }

            if(!labelDone)
            {
                labelDone = instrLoad->GetOrCreateContinueLabel();
            }

            this->lowererMD->GenerateFloatTest(src1, instrLoad, labelHelper, instrLoad->HasBailOutInfo());

            IR::Opnd* floatOpnd = IR::IndirOpnd::New(src1, Js::JavascriptNumber::GetValueOffset(), TyMachDouble, this->m_func);
            this->lowererMD->ConvertFloatToInt32(instrLoad->GetDst(), floatOpnd, labelHelper, labelDone, instrLoad);
        }

        // $Helper
        //  dst = ToInt32(r1)
        // $Done
        if (labelHelper)
        {
            instrLoad->InsertBefore(labelHelper);
        }
        if(instrLoad->HasBailOutInfo() && (instrLoad->GetBailOutKind() == IR::BailOutIntOnly || instrLoad->GetBailOutKind() == IR::BailOutExpectingInteger))
        {
            // Avoid bailout if we have a JavascriptNumber whose value is a signed 32-bit integer
            lowererMD->m_lowerer->LoadInt32FromUntaggedVar(instrLoad);

            // Need to bail out instead of calling a helper
            return true;
        }

        this->lowererMD->m_lowerer->LowerUnaryHelperMem(instrLoad, IR::HelperConv_ToInt32);
    }
    else
    {
        instrLoad->Remove();
    }

    return false;
}

IR::Instr *
LowererMDArch::LoadCheckedFloat(
    IR::RegOpnd *opndOrig,
    IR::RegOpnd *opndFloat,
    IR::LabelInstr *labelInline,
    IR::LabelInstr *labelHelper,
    IR::Instr *instrInsert,
    const bool checkForNullInLoopBody)
{
    // Load one floating-point var into an XMM register, inserting checks to see if it's really a float:

    //     TEST src, 1
    //     JNE $non-int
    // t0 = MOV src         // convert a tagged int to float
    // t0 = SAR t0, 1
    // flt = CVTSI2SD t0
    //     JMP $labelInline
    // $non-int
    //      CMP [src], JavascriptNumber::`vtable'
    //     JNE $labelHelper
    // flt = MOVSD [t0 + offset(value)]
    IR::Opnd * opnd;
    IR::Instr * instr;
    IR::Instr * instrFirst = IR::Instr::New(Js::OpCode::TEST, this->m_func);
    instrFirst->SetSrc1(opndOrig);
    instrFirst->SetSrc2(IR::IntConstOpnd::New(Js::AtomTag, TyMachReg, this->m_func));
    instrInsert->InsertBefore(instrFirst);

    IR::LabelInstr * labelVar = IR::LabelInstr::New(Js::OpCode::Label, this->m_func);
    instr = IR::BranchInstr::New(Js::OpCode::JEQ, labelVar, this->m_func);
    instrInsert->InsertBefore(instr);

    if (opndOrig->GetValueType().IsLikelyFloat())
    {
        // Make this path helper if value is likely a float
        instrInsert->InsertBefore(IR::LabelInstr::New(Js::OpCode::Label, this->m_func, true));
    }

    opnd = IR::RegOpnd::New(TyMachReg, this->m_func);
    instr = IR::Instr::New(Js::OpCode::MOV, opnd, opndOrig, this->m_func);

    instrInsert->InsertBefore(instr);

    instr = IR::Instr::New(
        Js::OpCode::SAR, opnd, opnd, IR::IntConstOpnd::New(Js::VarTag_Shift, TyInt8, this->m_func), this->m_func);
    instrInsert->InsertBefore(instr);

    instr = IR::Instr::New(Js::OpCode::CVTSI2SD, opndFloat, opnd, this->m_func);
    instrInsert->InsertBefore(instr);

    instr = IR::BranchInstr::New(Js::OpCode::JMP, labelInline, this->m_func);
    instrInsert->InsertBefore(instr);

    instrInsert->InsertBefore(labelVar);

    lowererMD->GenerateFloatTest(opndOrig, instrInsert, labelHelper, checkForNullInLoopBody);

    opnd = IR::IndirOpnd::New(opndOrig, Js::JavascriptNumber::GetValueOffset(), TyMachDouble, this->m_func);
    instr = IR::Instr::New(Js::OpCode::MOVSD, opndFloat, opnd, this->m_func);
    instrInsert->InsertBefore(instr);

    return instrFirst;
}


IR::LabelInstr *
LowererMDArch::GetBailOutStackRestoreLabel(BailOutInfo * bailOutInfo, IR::LabelInstr * exitTargetInstr)
{
    IR::Instr * exitPrevInstr = exitTargetInstr->m_prev;

    // On x86 we push and pop the out param area, but the start call can be moved passed the bailout instruction
    // which we don't keep track of.  There isn't a flow based pass after lowerer,
    // So we don't know how much stack we need to pop.  Instead, generate a landing area to restore the stack
    // Via EBP, the prolog/epilog phase will fix up the size from EBP we need to restore to ESP before the epilog
    if (bailOutInfo->startCallCount != 0)
    {
        if (this->bailOutStackRestoreLabel == nullptr)
        {
            if (exitPrevInstr->HasFallThrough())
            {
                // Branch around the stack reload
                IR::BranchInstr * branchToExit = IR::BranchInstr::New(Js::OpCode::JMP, exitTargetInstr, this->m_func);
                exitPrevInstr->InsertAfter(branchToExit);
                exitPrevInstr = branchToExit;
            }

            this->bailOutStackRestoreLabel = IR::LabelInstr::New(Js::OpCode::Label, this->m_func, true);

            IR::RegOpnd * ebpOpnd = IR::RegOpnd::New(nullptr, RegEBP, TyMachReg, this->m_func);
            IR::RegOpnd * espOpnd = IR::RegOpnd::New(nullptr, RegESP, TyMachReg, this->m_func);

            // -4 for now, fix up in prolog/epilog phase
            IR::IndirOpnd * indirOpnd = IR::IndirOpnd::New(ebpOpnd, (size_t)-4, TyMachReg, this->m_func);
            // Lower this after register allocation, once we know the frame size.
            IR::Instr *bailOutStackRestoreInstr = IR::Instr::New(Js::OpCode::BailOutStackRestore, espOpnd, indirOpnd, this->m_func);

            exitPrevInstr->InsertAfter(bailOutStackRestoreInstr);
            exitPrevInstr->InsertAfter(this->bailOutStackRestoreLabel);
        }

        // Jump to the stack restore label instead
        exitTargetInstr = this->bailOutStackRestoreLabel;
    }
    return exitTargetInstr;
}



///----------------------------------------------------------------------------
///
/// LowererMDArch::GenerateFastShiftLeft
///
///----------------------------------------------------------------------------

bool
    LowererMDArch::GenerateFastShiftLeft(IR::Instr * instrShift)
{
    // Given:
    //
    // dst = Shl src1, src2
    //
    // Generate:
    //
    // (If not 2 Int31's, jump to $helper.)
    // s1 = MOV src1
    // s1 = SAR s1, Js::VarTag_Shift  -- Remove the var tag from the value to be shifted
    // s2 = MOV src2
    // s2 = SAR s2, Js::VarTag_Shift  -- extract the real shift amount from the var
    // s1 = SHL s1, s2      -- do the inline shift
    // s3 = MOV s1
    // s3 = SHL s3, Js::VarTag_Shift  -- restore the var tag on the result
    //      JO  $ToVar
    // s3 = INC s3
    // dst = MOV s3
    //      JMP $fallthru
    //$ToVar:
    //      PUSH scriptContext
    //      PUSH s1
    // dst = ToVar()
    //      JMP $fallthru
    // $helper:
    //      (caller generates helper call)
    // $fallthru:

    IR::LabelInstr * labelHelper = nullptr;
    IR::LabelInstr * labelFallThru;
    IR::Instr *      instr;
    IR::RegOpnd *    opndReg1;
    IR::RegOpnd *    opndReg2;
    IR::Opnd *       opndSrc1;
    IR::Opnd *       opndSrc2;

    opndSrc1 = instrShift->GetSrc1();
    opndSrc2 = instrShift->GetSrc2();
    AssertMsg(opndSrc1 && opndSrc2, "Expected 2 src opnd's on Shl instruction");

    // Not tagged ints?
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
    bool src2IsIntConst = false;
    IntConstType s2Value = 0;

    if (opndSrc2->IsRegOpnd())
    {
        if (opndSrc2->AsRegOpnd()->IsTaggedInt())
        {
            if (opndSrc2->AsRegOpnd()->m_sym->IsTaggableIntConst())
            {
                src2IsIntConst = true;
                s2Value = opndSrc2->AsRegOpnd()->m_sym->GetIntConstValue();
                s2Value = (s2Value & 0x1F);
            }
            if (opndSrc1->IsTaggedInt())
            {
                isTaggedInts = true;
            }
        }
    }
    else
    {
        AssertMsg(opndSrc2->IsAddrOpnd() && Js::TaggedInt::Is(opndSrc2->AsAddrOpnd()->m_address),
            "Expect src2 of shift right to be reg or Var.");
        src2IsIntConst = true;
        s2Value = Js::TaggedInt::ToInt32(opndSrc2->AsAddrOpnd()->m_address);
        s2Value = (s2Value & 0x1F);

        if (opndSrc1->IsTaggedInt())
        {
            isTaggedInts = true;
        }
    }

    labelHelper = IR::LabelInstr::New(Js::OpCode::Label, instrShift->m_func, true);
    if (!isTaggedInts)
    {
        // (If not 2 Int31's, jump to $helper.)

        lowererMD->GenerateSmIntPairTest(instrShift, opndSrc1, opndSrc2, labelHelper);
    }

    // s1 = MOV src1

    opndReg1 = IR::RegOpnd::New(TyMachReg, instrShift->m_func);
    instr = IR::Instr::New(Js::OpCode::MOV, opndReg1, opndSrc1, instrShift->m_func);
    instrShift->InsertBefore(instr);

    // s1 = SAR s1, Js::VarTag_Shift

    //
    // Sign of the operand matters to SAR. Hence it need to operate on Int32 only
    //

    IR::Opnd * opnd32Reg1 = opndReg1->UseWithNewType(TyInt32, instrShift->m_func);

    instr = IR::Instr::New(
        Js::OpCode::SAR, opnd32Reg1, opnd32Reg1,
        IR::IntConstOpnd::New(Js::VarTag_Shift, TyInt8, instrShift->m_func), instrShift->m_func);
    instrShift->InsertBefore(instr);

    IR::Opnd *countOpnd;

    if (src2IsIntConst)
    {
        countOpnd = IR::IntConstOpnd::New(s2Value, TyMachReg, instrShift->m_func);
    }
    else
    {
        // s2 = MOV src2

        opndReg2 = IR::RegOpnd::New(TyMachReg, instrShift->m_func);

        // Shift count needs to be in ECX

        opndReg2->SetReg(this->GetRegShiftCount());
        instr = IR::Instr::New(Js::OpCode::MOV, opndReg2, opndSrc2, instrShift->m_func);
        instrShift->InsertBefore(instr);

        // s2 = SAR s2, Js::VarTag_Shift

        //
        // Sign of the operand matters to SAR. Hence it need to operate on Int32 only
        //

        IR::Opnd * opnd32Reg2 = opndReg2->UseWithNewType(TyInt32, instrShift->m_func);

        instr = IR::Instr::New(
            Js::OpCode::SAR, opnd32Reg2, opnd32Reg2,
            IR::IntConstOpnd::New(Js::VarTag_Shift, TyInt8, instrShift->m_func), instrShift->m_func);
        instrShift->InsertBefore(instr);
        countOpnd = opndReg2;
    }

    // s1 = SHL s1, s2

    //
    // Ecmascript spec says we only need mask the shift amount by 0x1F. But intel uses 0x3F
    // for 64 operands. Hence using 32 bits. opnd32Reg1 is already refined. reusing that.
    //

    instr = IR::Instr::New(Js::OpCode::SHL, opnd32Reg1, opnd32Reg1, countOpnd, instrShift->m_func);
    instrShift->InsertBefore(instr);

    // s3 = MOV s1

    IR::RegOpnd * opndReg3 = IR::RegOpnd::New(TyMachReg, instrShift->m_func);

    IR::Opnd * opnd32Reg3 = opndReg3->UseWithNewType(TyInt32, instrShift->m_func);

    instr = IR::Instr::New(Js::OpCode::MOV, opnd32Reg3, opnd32Reg1, instrShift->m_func);
    instrShift->InsertBefore(instr);

    // s3 = SHL s3, Js::VarTag_Shift  -- restore the var tag on the result

    //
    // Ecmascript spec says we only need mask the shift amount by 0x1F. But intel uses 0x3F
    // for 64 operands. Hence using 32 bits. opnd32Reg1 is already refined. reusing that.
    //

    instr = IR::Instr::New(
        Js::OpCode::SHL, opnd32Reg3, opnd32Reg3,
        IR::IntConstOpnd::New(Js::VarTag_Shift, TyInt8, instrShift->m_func), instrShift->m_func);
    instrShift->InsertBefore(instr);

    //      JO  $ToVar
    IR::LabelInstr *labelToVar = IR::LabelInstr::New(Js::OpCode::Label, instrShift->m_func, true);
    instr = IR::BranchInstr::New(Js::OpCode::JO, labelToVar, instrShift->m_func);
    instrShift->InsertBefore(instr);

    // s3 = INC s3

    instr = IR::Instr::New(Js::OpCode::INC, opndReg3, opndReg3, instrShift->m_func);
    instrShift->InsertBefore(instr);

    // dst = MOV s3

    instr = IR::Instr::New(Js::OpCode::MOV, instrShift->GetDst(), opndReg3, instrShift->m_func);
    instrShift->InsertBefore(instr);

    //      JMP $fallthru

    labelFallThru = IR::LabelInstr::New(Js::OpCode::Label, instrShift->m_func);
    instr = IR::BranchInstr::New(Js::OpCode::JMP, labelFallThru, instrShift->m_func);
    instrShift->InsertBefore(instr);

    //$ToVar:
    instrShift->InsertBefore(labelToVar);

    IR::JnHelperMethod helperMethod;

    IR::Opnd *dst;
    dst = instrShift->GetDst();
    if (instrShift->dstIsTempNumber)
    {
        IR::Opnd *tempOpnd;
        helperMethod = IR::HelperOp_Int32ToAtomInPlace;
        Assert(dst->IsRegOpnd());
        StackSym * tempNumberSym = lowererMD->GetLowerer()->GetTempNumberSym(dst, instrShift->dstIsTempNumberTransferred);

        IR::Instr *load = lowererMD->LoadStackAddress(tempNumberSym);
        instrShift->InsertBefore(load);
        tempOpnd = load->GetDst();
        this->LoadHelperArgument(instrShift, tempOpnd);
    }
    else
    {
        helperMethod = IR::HelperOp_Int32ToAtom;
    }

    //      PUSH scriptContext
    this->lowererMD->m_lowerer->LoadScriptContext(instrShift);

    //      PUSH s1
    this->LoadHelperArgument(instrShift, opndReg1);

    // dst = ToVar()
    instr = IR::Instr::New(Js::OpCode::Call, dst,
        IR::HelperCallOpnd::New(helperMethod, instrShift->m_func), instrShift->m_func);
    instrShift->InsertBefore(instr);
    this->LowerCall(instr, 0);

    //      JMP $fallthru

    instr = IR::BranchInstr::New(Js::OpCode::JMP, labelFallThru, instrShift->m_func);
    instrShift->InsertBefore(instr);

    // $helper:
    //      (caller generates helper call)
    // $fallthru:

    instrShift->InsertBefore(labelHelper);
    instrShift->InsertAfter(labelFallThru);

    return true;
}

///----------------------------------------------------------------------------
///
/// LowererMDArch::GenerateFastShiftRight
///
///----------------------------------------------------------------------------

bool
    LowererMDArch::GenerateFastShiftRight(IR::Instr * instrShift)
{
    // Given:
    //
    // dst = Shr/Sar src1, src2
    //
    // Generate:
    //
    // s1 = MOV src1
    //      TEST s1, 1
    //      JEQ  $S1ToInt
    // s1 = SAR  s1, VarTag_Shift  -- extract the real shift amount from the var
    //      JMP  $src2
    //$S1ToInt:
    //      PUSH scriptContext
    //      PUSH s1
    // s1 = ToInt32()/ToUInt32
    //$src2:
    // Load s2
    //      TEST s2, 1
    //      JEQ  $S2ToUInt
    // s2 = SAR  s2, VarTag_Shift  -- extract the real shift amount from the var
    //      JMP  $Shr
    //$S2ToUInt:
    //      PUSH scriptContext
    //      PUSH s2
    // s2 = ToUInt32()
    //$Shr:
    // s1 = SHR/SAR s1, s2         -- do the inline shift
    // s3 = MOV s1
    //ECX = MOV s2
    // s3 = SHL s3, ECX             -- To tagInt
    //      JO  $ToVar
    //      JS  $ToVar
    // s3 = INC s3
    //      JMP $done
    //$ToVar:
    //      EmitLoadVarNoCheck
    //$Done:
    // dst = MOV s3

    IR::LabelInstr * labelS1ToInt = nullptr;
    IR::LabelInstr * labelSrc2 = nullptr;
    IR::LabelInstr * labelS2ToUInt = nullptr;
    IR::LabelInstr * labelShr = nullptr;
    IR::LabelInstr * labelToVar = nullptr;
    IR::LabelInstr * labelDone = nullptr;
    IR::Instr *      instr;
    IR::RegOpnd *    opndReg1;
    IR::RegOpnd *    opndReg2;
    IR::Opnd *       opndSrc1;
    IR::Opnd *       opndSrc2;
    bool             src1IsInt = false;
    bool             src1IsNotInt = false;
    bool             src2IsInt = false;
    bool             src2IsIntConst = false;
    bool             src2IsNotInt = false;
    bool             resultIsTaggedInt = false;
    bool             isUnsignedShift = (instrShift->m_opcode == Js::OpCode::ShrU_A);

    opndSrc1 = instrShift->UnlinkSrc1();
    opndSrc2 = instrShift->UnlinkSrc2();
    AssertMsg(opndSrc1 && opndSrc2, "Expected 2 src opnd's on Shl instruction");

    if (instrShift->HasBailOutInfo())
    {
        IR::Instr * bailOutInstr = this->lowererMD->m_lowerer->SplitBailOnImplicitCall(instrShift);
        this->lowererMD->m_lowerer->LowerBailOnEqualOrNotEqual(bailOutInstr);
    }
    AssertMsg(opndSrc1->IsRegOpnd(), "We expect this to be a regOpnd");
    opndReg1 = opndSrc1->AsRegOpnd();

    src1IsInt = opndReg1->IsTaggedInt();
    if (src1IsInt && !isUnsignedShift)
    {
        // -1 >>> 0 != taggedInt...
        resultIsTaggedInt = true;
    }

    src1IsNotInt = opndReg1->IsNotInt();

    // s1 = MOV src1

    opndReg1 = IR::RegOpnd::New(TyMachReg, instrShift->m_func);
    instr = IR::Instr::New(Js::OpCode::MOV, opndReg1, opndSrc1, instrShift->m_func);
    instrShift->InsertBefore(instr);

    IR::Opnd *dst = instrShift->GetDst();
    AssertMsg(dst->IsRegOpnd(), "We expect this to be a regOpnd");
    IntConstType s2Value = 0;

    if (opndSrc2->IsRegOpnd())
    {
        opndReg2 = opndSrc2->AsRegOpnd();
        src2IsInt = opndReg2->IsTaggedInt();
        src2IsIntConst = opndReg2->m_sym->IsTaggableIntConst();
        src2IsNotInt = opndReg2->IsNotInt();
    }
    else
    {
        AssertMsg(opndSrc2->IsAddrOpnd() && Js::TaggedInt::Is(opndSrc2->AsAddrOpnd()->m_address),
            "Expect src2 of shift right to be reg or Var.");
        src2IsInt = src2IsIntConst = true;
        opndReg2 = nullptr;
    }
    if (isUnsignedShift)
    {
        // We use the src2IsIntConst to combine the tag shifting with the actual shift.
        // The tag shift however needs to be a signed shift...
        src2IsIntConst = false;

        if (opndSrc2->IsAddrOpnd())
        {
            instr = lowererMD->CreateAssign(
                IR::RegOpnd::New(opndSrc2->GetType(), instrShift->m_func),
                opndSrc2, instrShift);
            opndSrc2 = instr->GetDst();
            opndReg2 = opndSrc2->AsRegOpnd();
        }
    }
    if (src2IsIntConst)
    {
        if (opndSrc2->IsRegOpnd())
        {
            AnalysisAssert(opndReg2);
            s2Value = opndReg2->m_sym->GetIntConstValue();
        }
        else
        {
            s2Value = Js::TaggedInt::ToInt32(opndSrc2->AsAddrOpnd()->m_address);
        }

        s2Value = (s2Value & 0x1F);

        if (s2Value >= Js::VarTag_Shift)
        {
            resultIsTaggedInt = true;
            if ((unsigned)(s2Value + Js::VarTag_Shift) > 0x1f)
            {
                // Can't combine the SHR with the AtomTag shift if we overflow...
                s2Value = 0;
                src2IsIntConst = false;
            }
        }
    }

    if (!src1IsNotInt)
    {
        if (!src1IsInt)
        {
            //      TEST s1, AtomTag
            instr = IR::Instr::New(Js::OpCode::TEST, instrShift->m_func);
            instr->SetSrc1(opndReg1);
            instr->SetSrc2(IR::IntConstOpnd::New(Js::AtomTag, TyMachReg, instrShift->m_func));
            instrShift->InsertBefore(instr);

            //      JEQ  $S1ToInt
            labelS1ToInt = IR::LabelInstr::New(Js::OpCode::Label, instrShift->m_func, true);
            instr = IR::BranchInstr::New(Js::OpCode::JEQ, labelS1ToInt, instrShift->m_func);
            instrShift->InsertBefore(instr);
        }

        // s1 = SAR s1, VarTag_Shift  -- extract the real shift amount from the var

        //
        // Sign of the operand matters to SAR. Hence it need to operate on Int32 only
        //

        IR::Opnd * opnd32Reg1 = opndReg1->UseWithNewType(TyInt32, instrShift->m_func);

        instr = IR::Instr::New(Js::OpCode::SAR, opnd32Reg1, opnd32Reg1,
            IR::IntConstOpnd::New(Js::VarTag_Shift + s2Value, TyInt8, instrShift->m_func), instrShift->m_func);
        instrShift->InsertBefore(instr);

        //      JMP  $src2
        labelSrc2 = IR::LabelInstr::New(Js::OpCode::Label, instrShift->m_func);
        instr = IR::BranchInstr::New(Js::OpCode::JMP, labelSrc2, instrShift->m_func);
        instrShift->InsertBefore(instr);
    }

    if (!src1IsInt)
    {
        if (labelS1ToInt)
        {
            //$S1ToInt:
            instrShift->InsertBefore(labelS1ToInt);
        }

        //      PUSH scriptContext
        this->lowererMD->m_lowerer->LoadScriptContext(instrShift);

        //      PUSH s1
        this->LoadHelperArgument(instrShift, opndReg1);

        // s1 = ToInt32()/ToUint32
        instr = IR::Instr::New(Js::OpCode::Call, opndReg1,
            IR::HelperCallOpnd::New((isUnsignedShift ? IR::HelperConv_ToUInt32_Full : IR::HelperConv_ToInt32_Full), instrShift->m_func),
            instrShift->m_func);
        instrShift->InsertBefore(instr);
        this->LowerCall(instr, 0);

        if (src2IsIntConst && s2Value != 0)
        {
            // s1 = SHR/SAR s1, s2         -- do the inline shift

            //
            // Sign of the operand matters to SAR. Hence it need to operate on Int32 only
            //

            IR::Opnd * opnd32Reg1 = opndReg1->UseWithNewType(TyInt32, instrShift->m_func);

            instr = IR::Instr::New(isUnsignedShift ? Js::OpCode::SHR : Js::OpCode::SAR,
                opnd32Reg1, opnd32Reg1, IR::IntConstOpnd::New(s2Value, TyInt8, instrShift->m_func), instrShift->m_func);
            instrShift->InsertBefore(instr);
        }
    }


    //$src2:
    if (labelSrc2)
    {
        instrShift->InsertBefore(labelSrc2);
    }

    if (!src2IsIntConst)
    {
        // Load s2
        opndReg2 = IR::RegOpnd::New(TyMachReg, instrShift->m_func);
        instr = IR::Instr::New(Js::OpCode::MOV, opndReg2, opndSrc2, instrShift->m_func);
        instrShift->InsertBefore(instr);
    }

    if (!src2IsNotInt)
    {
        if (!src2IsInt)
        {
            //      TEST s2, AtomTag
            instr = IR::Instr::New(Js::OpCode::TEST, instrShift->m_func);
            instr->SetSrc1(opndReg2);
            instr->SetSrc2(IR::IntConstOpnd::New(Js::AtomTag, TyMachReg, instrShift->m_func));
            instrShift->InsertBefore(instr);

            //      JEQ  $ToUInt
            labelS2ToUInt = IR::LabelInstr::New(Js::OpCode::Label, instrShift->m_func, true);
            instr = IR::BranchInstr::New(Js::OpCode::JEQ, labelS2ToUInt, instrShift->m_func);
            instrShift->InsertBefore(instr);
        }

        if (!src2IsIntConst)
        {
            // s2 = SAR s2, VarTag_Shift  -- extract the real shift amount from the var
            //
            // Sign of the operand matters to SAR. Hence it need to operate on Int32 only
            //

            IR::Opnd * opnd32Reg2 = opndReg2->UseWithNewType(TyInt32, instrShift->m_func);

            instr = IR::Instr::New(Js::OpCode::SAR, opnd32Reg2, opnd32Reg2,
                IR::IntConstOpnd::New(Js::VarTag_Shift, TyInt8, instrShift->m_func), instrShift->m_func);
            instrShift->InsertBefore(instr);
        }

        //      JMP  $shr
        labelShr = IR::LabelInstr::New(Js::OpCode::Label, instrShift->m_func);
        instr = IR::BranchInstr::New(Js::OpCode::JMP, labelShr, instrShift->m_func);
        instrShift->InsertBefore(instr);
    }
    if (!src2IsInt)
    {
        if (labelS2ToUInt)
        {
            //$S2ToUInt:
            instrShift->InsertBefore(labelS2ToUInt);
        }

        //      PUSH scriptContext
        this->lowererMD->m_lowerer->LoadScriptContext(instrShift);

        //      PUSH s2
        this->LoadHelperArgument(instrShift, opndReg2);

        // s2 = ToUInt32()
        instr = IR::Instr::New(Js::OpCode::Call, opndReg2,
            IR::HelperCallOpnd::New(IR::HelperConv_ToUInt32_Full, instrShift->m_func), instrShift->m_func);
        instrShift->InsertBefore(instr);
        this->LowerCall(instr, 0);
    }

    //$Shr:
    if (labelShr)
    {
        instrShift->InsertBefore(labelShr);
    }

    if (!src2IsIntConst)
    {
        // s1 = SHR/SAR s1, s2         -- do the inline shift
        //
        // Sign of the operand matters to SAR. Hence it need to operate on Int32 only
        //

        IR::Opnd * opnd32Reg1 = opndReg1->UseWithNewType(TyInt32, instrShift->m_func);
        IR::RegOpnd * opnd32Ecx = IR::RegOpnd::New(TyInt32, this->m_func);
        opnd32Ecx->SetReg(this->GetRegShiftCount());

        instr = IR::Instr::New(Js::OpCode::MOV, opnd32Ecx, opndReg2, this->m_func);
        instrShift->InsertBefore(instr);

        instr = IR::Instr::New(isUnsignedShift ? Js::OpCode::SHR : Js::OpCode::SAR,
            opnd32Reg1, opnd32Reg1, opnd32Ecx, instrShift->m_func);
        instrShift->InsertBefore(instr);
    }

    // s3 = MOV s1

    IR::Opnd * opnd32Reg1 = opndReg1->UseWithNewType(TyInt32, instrShift->m_func);

    IR::RegOpnd * opndReg3 = IR::RegOpnd::New(TyMachReg, instrShift->m_func);

    IR::Opnd * opnd32Reg3 = opndReg3->UseWithNewType(TyInt32, instrShift->m_func);

    instr = IR::Instr::New(Js::OpCode::MOV, opnd32Reg3, opnd32Reg1, instrShift->m_func);
    instrShift->InsertBefore(instr);

    // s3 = SHL s3, VarTag_Shift             -- To tagInt

    //
    // Ecmascript spec says we only need mask the shift amount by 0x1F. But intel uses 0x3F
    // for 64 operands. Hence using 32 bits.
    //

    instr = IR::Instr::New(Js::OpCode::SHL, opnd32Reg3, opnd32Reg3,
        IR::IntConstOpnd::New(Js::VarTag_Shift, TyInt8, instrShift->m_func), instrShift->m_func);
    instrShift->InsertBefore(instr);

    if (!resultIsTaggedInt)
    {
        //      JO  $ToVar
        labelToVar = IR::LabelInstr::New(Js::OpCode::Label, instrShift->m_func, true);
        instr = IR::BranchInstr::New(Js::OpCode::JO, labelToVar, instrShift->m_func);
        instrShift->InsertBefore(instr);

        if (isUnsignedShift)
        {
            //      JS  $ToVar
            instr = IR::BranchInstr::New(Js::OpCode::JSB, labelToVar, instrShift->m_func);
            instrShift->InsertBefore(instr);
        }
    }

    // s1 = INC s1
    instr = IR::Instr::New(Js::OpCode::INC, opndReg3, opndReg3, instrShift->m_func);
    instrShift->InsertBefore(instr);

    if (!src1IsInt || !src2IsInt || !resultIsTaggedInt)
    {
        //      JMP $done
        labelDone = IR::LabelInstr::New(Js::OpCode::Label, instrShift->m_func);
        instr = IR::BranchInstr::New(Js::OpCode::JMP, labelDone, instrShift->m_func);
        instrShift->InsertBefore(instr);
    }

    if (!resultIsTaggedInt)
    {
        //$ToVar:
        instrShift->InsertBefore(labelToVar);

        this->lowererMD->EmitLoadVarNoCheck(opndReg3, opndReg1, instrShift, isUnsignedShift, true);
    }

    if (labelDone)
    {
        //$Done:
        instrShift->InsertBefore(labelDone);
    }

    // dst = MOV s3

    instrShift->m_opcode = Js::OpCode::MOV;
    instrShift->SetSrc1(opndReg3);

    // Skip lowering call to helper
    return false;
}


///----------------------------------------------------------------------------
///
/// LowererMDArch::GenerateFastAnd
///
///----------------------------------------------------------------------------

bool
    LowererMDArch::GenerateFastAnd(IR::Instr * instrAnd)
{
    // Given:
    //
    // dst = And src1, src2
    //
    // Generate:
    //
    // s1 = MOV src1
    // s1 = AND s1, src2   -- try an inline add
    //      TEST s1, 1     -- if both opnds are ints, the int tag will be set in the result
    //      JEQ $helper
    // dst = MOV s1
    //      JMP $fallthru
    // $helper:
    //      (caller generates helper sequence)
    // $fallthru:

    IR::Instr *      instr;
    IR::LabelInstr * labelHelper=nullptr;
    IR::LabelInstr * labelFallThru;
    IR::Opnd *       opndReg;
    IR::Opnd *       opndSrc1;
    IR::Opnd *       opndSrc2;

    opndSrc1 = instrAnd->GetSrc1();
    opndSrc2 = instrAnd->GetSrc2();
    AssertMsg(opndSrc1 && opndSrc2, "Expected 2 src opnd's on And instruction");

    // Not tagged ints?
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
    // s1 = MOV src1

    opndReg = IR::RegOpnd::New(TyMachReg, instrAnd->m_func);
    instr = IR::Instr::New(Js::OpCode::MOV, opndReg, opndSrc1, instrAnd->m_func);
    instrAnd->InsertBefore(instr);

    if (opndSrc2->IsRegOpnd() && opndSrc2->AsRegOpnd()->m_sym->IsTaggableIntConst())
    {
        Js::Var value   = Js::TaggedInt::ToVarUnchecked(opndSrc2->AsRegOpnd()->m_sym->GetIntConstValue());
        opndSrc2        = IR::AddrOpnd::New(value, IR::AddrOpndKindConstantVar, instrAnd->m_func);
    }

    // s1 = AND s1, src2

    instr = IR::Instr::New(Js::OpCode::AND, opndReg, opndReg, opndSrc2, instrAnd->m_func);
    instrAnd->InsertBefore(instr);

    if (!isTaggedInts)
    {
        //      TEST s1, 1

        instr = IR::Instr::New(Js::OpCode::TEST, instrAnd->m_func);
        instr->SetSrc1(opndReg);
        instr->SetSrc2(IR::IntConstOpnd::New(Js::AtomTag, TyMachReg, instrAnd->m_func));
        instrAnd->InsertBefore(instr);

        //      JNE $helper

        labelHelper = IR::LabelInstr::New(Js::OpCode::Label, instrAnd->m_func, true);
        instr = IR::BranchInstr::New(Js::OpCode::JEQ, labelHelper, instrAnd->m_func);
        instrAnd->InsertBefore(instr);
    }

    // dst = MOV s1

    if (isTaggedInts)
    {
        // Reuse the existing instruction
        instrAnd->m_opcode = Js::OpCode::MOV;
        instrAnd->ReplaceSrc1(opndReg);
        instrAnd->FreeSrc2();

        // Skip lowering call to helper
        return false;
    }

    instr = IR::Instr::New(Js::OpCode::MOV, instrAnd->GetDst(), opndReg, instrAnd->m_func);
    instrAnd->InsertBefore(instr);

    //      JMP $fallthru

    labelFallThru = IR::LabelInstr::New(Js::OpCode::Label, instrAnd->m_func);
    instr = IR::BranchInstr::New(Js::OpCode::JMP, labelFallThru, instrAnd->m_func);
    instrAnd->InsertBefore(instr);

    // $helper:
    //      (caller generates helper sequence)
    // $fallthru:

    AssertMsg(labelHelper, "Should not be NULL");
    instrAnd->InsertBefore(labelHelper);
    instrAnd->InsertAfter(labelFallThru);

    return true;
}

///----------------------------------------------------------------------------
///
/// LowererMDArch::GenerateFastOr
///
///----------------------------------------------------------------------------

bool
    LowererMDArch::GenerateFastOr(IR::Instr * instrOr)
{
    // Given:
    //
    // dst = Or src1, src2
    //
    // Generate:
    //
    // (If not 2 Int31's, jump to $helper.)
    //
    // s1 = MOV src1
    // s1 = OR s1, src2    -- try an inline OR
    // dst = MOV s1
    //      JMP $fallthru
    // $helper:
    //      (caller generates helper sequence)
    // $fallthru:

    IR::Instr *      instr;
    IR::LabelInstr * labelHelper=nullptr;
    IR::LabelInstr * labelFallThru;
    IR::Opnd *       opndReg;
    IR::Opnd *       opndSrc1;
    IR::Opnd *       opndSrc2;

    opndSrc1 = instrOr->GetSrc1();
    opndSrc2 = instrOr->GetSrc2();
    AssertMsg(opndSrc1 && opndSrc2, "Expected 2 src opnd's on Or instruction");

    // Not tagged ints?
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

    if (!isTaggedInts)
    {
        // (If not 2 Int31's, jump to $helper.)

        labelHelper = IR::LabelInstr::New(Js::OpCode::Label, instrOr->m_func, true);
        lowererMD->GenerateSmIntPairTest(instrOr, opndSrc1, opndSrc2, labelHelper);
    }

    // s1 = MOV src1

    opndReg = IR::RegOpnd::New(TyMachReg, instrOr->m_func);
    instr = IR::Instr::New(Js::OpCode::MOV, opndReg, opndSrc1, instrOr->m_func);
    instrOr->InsertBefore(instr);

    // s1 = OR s1, src2

    instr = IR::Instr::New(Js::OpCode::OR, opndReg, opndReg, opndSrc2, instrOr->m_func);
    instrOr->InsertBefore(instr);

    // dst = MOV s1

    if (isTaggedInts)
    {
        // Reuse the existing instruction
        instrOr->m_opcode = Js::OpCode::MOV;
        instrOr->ReplaceSrc1(opndReg);
        instrOr->FreeSrc2();

        // Skip lowering call to helper
        return false;
    }

    instr = IR::Instr::New(Js::OpCode::MOV, instrOr->GetDst(), opndReg, instrOr->m_func);
    instrOr->InsertBefore(instr);

    //      JMP $fallthru

    labelFallThru = IR::LabelInstr::New(Js::OpCode::Label, instrOr->m_func);
    instr = IR::BranchInstr::New(Js::OpCode::JMP, labelFallThru, instrOr->m_func);
    instrOr->InsertBefore(instr);

    // $helper:
    //      (caller generates helper sequence)
    // $fallthru:

    AssertMsg(labelHelper, "Should not be NULL");
    instrOr->InsertBefore(labelHelper);
    instrOr->InsertAfter(labelFallThru);

    return true;
}


///----------------------------------------------------------------------------
///
/// LowererMD::GenerateFastXor
///
///----------------------------------------------------------------------------

bool
    LowererMDArch::GenerateFastXor(IR::Instr * instrXor)
{
    // Given:
    //
    // dst = Xor src1, src2
    //
    // Generate:
    //
    // (If not 2 Int31's, jump to $helper.)
    //
    // s1 = MOV src1
    // s1 = XOR s1, src2    -- try an inline XOR
    // s1 = INC s1
    // dst = MOV s1
    //      JMP $fallthru
    // $helper:
    //      (caller generates helper sequence)
    // $fallthru:

    IR::Instr *      instr;
    IR::LabelInstr * labelHelper=nullptr;
    IR::LabelInstr * labelFallThru;
    IR::Opnd *       opndReg;
    IR::Opnd *       opndSrc1;
    IR::Opnd *       opndSrc2;

    opndSrc1 = instrXor->GetSrc1();
    opndSrc2 = instrXor->GetSrc2();
    AssertMsg(opndSrc1 && opndSrc2, "Expected 2 src opnd's on Xor instruction");

    // Not tagged ints?
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

    if (!isTaggedInts)
    {
        // (If not 2 Int31's, jump to $helper.)

        labelHelper = IR::LabelInstr::New(Js::OpCode::Label, instrXor->m_func, true);
        lowererMD->GenerateSmIntPairTest(instrXor, opndSrc1, opndSrc2, labelHelper);
    }

    // s1 = MOV src1

    opndReg = IR::RegOpnd::New(TyMachReg, instrXor->m_func);
    instr = IR::Instr::New(Js::OpCode::MOV, opndReg, opndSrc1, instrXor->m_func);
    instrXor->InsertBefore(instr);

    // s1 = XOR s1, src2

    instr = IR::Instr::New(Js::OpCode::XOR, opndReg, opndReg, opndSrc2, instrXor->m_func);
    instrXor->InsertBefore(instr);

    // s1 = INC s1

    instr = IR::Instr::New(Js::OpCode::INC, opndReg, opndReg, instrXor->m_func);
    instrXor->InsertBefore(instr);

    // dst = MOV s1

    if (isTaggedInts)
    {
        // Reuse the existing instruction
        instrXor->m_opcode = Js::OpCode::MOV;
        instrXor->ReplaceSrc1(opndReg);
        instrXor->FreeSrc2();

        // Skip lowering call to helper
        return false;
    }

    instr = IR::Instr::New(Js::OpCode::MOV, instrXor->GetDst(), opndReg, instrXor->m_func);
    instrXor->InsertBefore(instr);

    //      JMP $fallthru

    labelFallThru = IR::LabelInstr::New(Js::OpCode::Label, instrXor->m_func);
    instr = IR::BranchInstr::New(Js::OpCode::JMP, labelFallThru, instrXor->m_func);
    instrXor->InsertBefore(instr);

    // $helper:
    //      (caller generates helper sequence)
    // $fallthru:

    AssertMsg(labelHelper, "Should not be NULL");
    instrXor->InsertBefore(labelHelper);
    instrXor->InsertAfter(labelFallThru);

    return true;
}

//----------------------------------------------------------------------------
//
// LowererMD::GenerateFastNot
//
//----------------------------------------------------------------------------

bool
    LowererMDArch::GenerateFastNot(IR::Instr * instrNot)
{
    // Given:
    //
    // dst = Not src
    //
    // Generate:
    //
    //       TEST src, 1    -- test for int src
    //       JEQ $helper
    // dst = MOV src
    // dst = NOT dst        -- do an inline NOT
    // dst = INC dst        -- restore the var tag on the result (!1 becomes 0, INC to get 1 again)
    //       JMP $fallthru
    // $helper:
    //      (caller generates helper call)
    // $fallthru:

    IR::Instr *      instr;
    IR::LabelInstr * labelHelper = nullptr;
    IR::LabelInstr * labelFallThru = nullptr;
    IR::Opnd *       opndSrc1;
    IR::Opnd *       opndDst;

    opndSrc1 = instrNot->GetSrc1();
    AssertMsg(opndSrc1, "Expected src opnd on Not instruction");

    if (opndSrc1->IsRegOpnd() && opndSrc1->AsRegOpnd()->m_sym->IsIntConst())
    {
        IntConstType value = opndSrc1->AsRegOpnd()->m_sym->GetIntConstValue();

        value = ~value;

        instrNot->ClearBailOutInfo();
        instrNot->FreeSrc1();
        instrNot->SetSrc1(IR::AddrOpnd::NewFromNumber(value, instrNot->m_func));
        instrNot = this->lowererMD->ChangeToAssign(instrNot);

        // Skip lowering call to helper
        return false;
    }

    bool isInt = (opndSrc1->IsTaggedInt());

    if (!isInt)
    {
        //      TEST src1, AtomTag

        instr = IR::Instr::New(Js::OpCode::TEST, instrNot->m_func);
        instr->SetSrc1(opndSrc1);
        instr->SetSrc2(IR::IntConstOpnd::New(Js::AtomTag, TyMachReg, instrNot->m_func));
        instrNot->InsertBefore(instr);

        //      JEQ $helper

        labelHelper = IR::LabelInstr::New(Js::OpCode::Label, instrNot->m_func, true);
        instr = IR::BranchInstr::New(Js::OpCode::JEQ, labelHelper, instrNot->m_func);
        instrNot->InsertBefore(instr);
    }

    // dst = MOV src

    opndDst = instrNot->GetDst();
    instr = IR::Instr::New(Js::OpCode::MOV, opndDst, opndSrc1, instrNot->m_func);
    instrNot->InsertBefore(instr);

    // dst = NOT dst

    instr = IR::Instr::New(Js::OpCode::NOT, opndDst, opndDst, instrNot->m_func);
    instrNot->InsertBefore(instr);

    // dst = INC dst

    instr = IR::Instr::New(Js::OpCode::INC, opndDst, opndDst, instrNot->m_func);
    instrNot->InsertBefore(instr);

    if (isInt)
    {
        instrNot->Remove();

        // Skip lowering call to helper
        return false;
    }

    //      JMP $fallthru

    labelFallThru = IR::LabelInstr::New(Js::OpCode::Label, instrNot->m_func);
    instr = IR::BranchInstr::New(Js::OpCode::JMP, labelFallThru, instrNot->m_func);
    instrNot->InsertBefore(instr);

    // $helper:
    //      (caller generates helper sequence)
    // $fallthru:

    AssertMsg(labelHelper, "Should not be NULL");
    instrNot->InsertBefore(labelHelper);
    instrNot->InsertAfter(labelFallThru);

    return true;
}

void
LowererMDArch::FinalLower()
{
    int32 offset;

    FOREACH_INSTR_BACKWARD_EDITING_IN_RANGE(instr, instrPrev, this->m_func->m_tailInstr, this->m_func->m_headInstr)
    {
        switch (instr->m_opcode)
        {
        case Js::OpCode::Leave:
            Assert(this->m_func->DoOptimizeTryCatch() && !this->m_func->IsLoopBodyInTry());
            this->lowererMD->LowerLeave(instr, instr->AsBranchInstr()->GetTarget(), true /*fromFinalLower*/);
            break;

        case Js::OpCode::BailOutStackRestore:
            // We don't know the frameSize at lower time...
            instr->m_opcode = Js::OpCode::LEA;
            // exclude the EBP and return address
            instr->GetSrc1()->AsIndirOpnd()->SetOffset(-(int)(this->m_func->frameSize) + 2 * MachPtr);
            break;

        case Js::OpCode::RestoreOutParam:
            Assert(instr->GetDst() != nullptr);
            Assert(instr->GetDst()->IsIndirOpnd());
            offset = instr->GetDst()->AsIndirOpnd()->GetOffset();
            offset -= this->m_func->frameSize;
            offset += 2 * sizeof(void*);
            instr->GetDst()->AsIndirOpnd()->SetOffset(offset, true);
            instr->m_opcode = Js::OpCode::MOV;
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
    }
    NEXT_INSTR_BACKWARD_EDITING_IN_RANGE;
}

//This is dependent on calling convention and harder to do common thing here.
IR::Opnd*
LowererMDArch::GenerateArgOutForStackArgs(IR::Instr* callInstr, IR::Instr* stackArgsInstr)
{
// x86:
//    s25.i32       =  LdLen_A          s4.var
//    s26.i32       =  Ld_A             s25.i32
//    s25.i32       =  Or_I4            s25.i32, 1   // For alignment
//    $L2:
//    s10.var       = LdElemI_A         [s4.var+s25.i32].var
//                    ArgOut_A_Dynamic  s10.var
//    s25.i32       = SUB_I4            s25.i32, 0x1
//                    JNE               $L2
//    $L3

    GenerateFunctionObjectTest(callInstr, callInstr->GetSrc1()->AsRegOpnd(), false);

    if (callInstr->m_func->IsInlinee())
    {
        return this->lowererMD->m_lowerer->GenerateArgOutForInlineeStackArgs(callInstr, stackArgsInstr);
    }

    Assert(stackArgsInstr->m_opcode == Js::OpCode::ArgOut_A_FromStackArgs);
    Assert(callInstr->m_opcode == Js::OpCode::CallIDynamic);

    Func *func = callInstr->m_func;
    IR::RegOpnd* stackArgs = stackArgsInstr->GetSrc1()->AsRegOpnd();

    IR::RegOpnd* ldLenDstOpnd = IR::RegOpnd::New(TyUint32, func);
    IR::Instr* ldLen = IR::Instr::New(Js::OpCode::LdLen_A, ldLenDstOpnd, stackArgs, func);
    ldLenDstOpnd->SetValueType(ValueType::GetTaggedInt()); // LdLen_A works only on stack arguments
    callInstr->InsertBefore(ldLen);
    this->lowererMD->m_lowerer->GenerateFastRealStackArgumentsLdLen(ldLen);

    IR::Instr* saveLenInstr = IR::Instr::New(Js::OpCode::MOV, IR::RegOpnd::New(TyUint32, func), ldLenDstOpnd, func);
    saveLenInstr->GetDst()->SetValueType(ValueType::GetTaggedInt());
    callInstr->InsertBefore(saveLenInstr);

    // Align frame
    IR::Instr* orInstr = IR::Instr::New(Js::OpCode::OR, ldLenDstOpnd, ldLenDstOpnd, IR::IntConstOpnd::New(1, TyInt32, this->m_func), this->m_func);
    callInstr->InsertBefore(orInstr);

    IR::LabelInstr* startLoop = IR::LabelInstr::New(Js::OpCode::Label, func);
    startLoop->m_isLoopTop = true;
    Loop *loop = JitAnew(this->m_func->m_alloc, Loop, this->m_func->m_alloc, this->m_func);
    startLoop->SetLoop(loop);
    loop->SetLoopTopInstr(startLoop);
    loop->regAlloc.liveOnBackEdgeSyms = JitAnew(func->m_alloc, BVSparse<JitArenaAllocator>, func->m_alloc);

    callInstr->InsertBefore(startLoop);

    IR::IndirOpnd *nthArgument = IR::IndirOpnd::New(stackArgs, ldLenDstOpnd, TyMachReg, func);
    nthArgument->SetOffset(-1);
    IR::RegOpnd* ldElemDstOpnd = IR::RegOpnd::New(TyMachReg,func);
    IR::Instr* ldElem = IR::Instr::New(Js::OpCode::LdElemI_A, ldElemDstOpnd, nthArgument, func);
    callInstr->InsertBefore(ldElem);
    this->lowererMD->m_lowerer->GenerateFastStackArgumentsLdElemI(ldElem);

    IR::Instr* argout = IR::Instr::New(Js::OpCode::ArgOut_A_Dynamic, func);
    argout->SetSrc1(ldElemDstOpnd);
    callInstr->InsertBefore(argout);
    this->LoadDynamicArgument(argout);


    IR::Instr *subInstr = IR::Instr::New(Js::OpCode::Sub_I4, ldLenDstOpnd, ldLenDstOpnd, IR::IntConstOpnd::New(1, TyInt8, func),func);
    callInstr->InsertBefore(subInstr);
    this->lowererMD->EmitInt4Instr(subInstr);

    IR::BranchInstr *tailBranch = IR::BranchInstr::New(Js::OpCode::JNE, startLoop, func);
    callInstr->InsertBefore(tailBranch);

    loop->regAlloc.liveOnBackEdgeSyms->Set(ldLenDstOpnd->m_sym->m_id);

    // return the length which will be used for callInfo generations & stack allocation
    return saveLenInstr->GetDst()->AsRegOpnd();
}

IR::Instr *
LowererMDArch::LowerEHRegionReturn(IR::Instr * insertBeforeInstr, IR::Opnd * targetOpnd)
{
    IR::RegOpnd *retReg = IR::RegOpnd::New(StackSym::New(TyMachReg, this->m_func), GetRegReturn(TyMachReg), TyMachReg, this->m_func);

    // Load the continuation address into the return register.
    insertBeforeInstr->InsertBefore(IR::Instr::New(Js::OpCode::MOV, retReg, targetOpnd, this->m_func));

    IR::IntConstOpnd *intSrc = IR::IntConstOpnd::New(0, TyInt32, this->m_func);
    IR::Instr * retInstr = IR::Instr::New(Js::OpCode::RET, this->m_func);
    retInstr->SetSrc1(intSrc);
    retInstr->SetSrc2(retReg);
    insertBeforeInstr->InsertBefore(retInstr);

    // return the last instruction inserted
    return retInstr;
}
