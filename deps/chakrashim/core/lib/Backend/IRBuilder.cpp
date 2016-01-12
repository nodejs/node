//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "BackEnd.h"

#pragma prefast(push)
#pragma prefast(disable:28652, "Prefast complains that the OR are causing the compiler to emit dynamic initializers and the variable to be allocated in read/write mem...")

static const IR::BailOutKind c_debuggerBailOutKindForCall =
    IR::BailOutForceByFlag | IR::BailOutStackFrameBase | IR::BailOutBreakPointInFunction | IR::BailOutLocalValueChanged | IR::BailOutIgnoreException;
static const IR::BailOutKind c_debuggerBaseBailOutKindForHelper = IR::BailOutIgnoreException | IR::BailOutForceByFlag;

#pragma prefast(pop)

uint
IRBuilder::AddStatementBoundary(uint statementIndex, uint offset)
{
    // Under debugger we use full stmt map, so that we know exactly start and end of each user stmt.
    // We insert additional instrs in between statements, such as ProfiledLoopStart, for these bytecode reader acts as
    // there is "unknown" stmt boundary with statementIndex == -1. Don't add stmt boundary for that as later
    // it may cause issues, e.g. see WinBlue 218307.
    if (!(statementIndex == Js::Constants::NoStatementIndex && this->m_func->IsJitInDebugMode()))
    {
        IR::PragmaInstr* pragmaInstr = IR::PragmaInstr::New(Js::OpCode::StatementBoundary, statementIndex, m_func);
        this->AddInstr(pragmaInstr, offset);
    }

#ifdef BAILOUT_INJECTION
    // Don't inject bailout if the function have trys
    if (!this->m_func->GetTopFunc()->HasTry() && (statementIndex != Js::Constants::NoStatementIndex))
    {
        if (Js::Configuration::Global.flags.IsEnabled(Js::BailOutFlag) && !this->m_func->GetJnFunction()->GetUtf8SourceInfo()->GetIsLibraryCode())
        {
            ULONG line;
            LONG col;

            // Since we're on a separate thread, don't allow the line cache to be allocated in the Recycler.
            if (this->m_func->GetJnFunction()->GetLineCharOffset(this->m_jnReader.GetCurrentOffset(), &line, &col, false /*canAllocateLineCache*/))
            {
                line++;
                if (Js::Configuration::Global.flags.BailOut.Contains(line, (uint32)col) || Js::Configuration::Global.flags.BailOut.Contains(line, (uint32)-1))
                {
                    this->InjectBailOut(offset);
                }
            }
        } else if(Js::Configuration::Global.flags.IsEnabled(Js::BailOutAtEveryLineFlag)) {
            this->InjectBailOut(offset);
        }
    }
#endif
    return m_statementReader.MoveNextStatementBoundary();
}

// Add conditional bailout for breaking into interpreter debug thunk - for fast F12.
void
IRBuilder::InsertBailOutForDebugger(uint byteCodeOffset, IR::BailOutKind kind, IR::Instr* insertBeforeInstr /* default nullptr */)
{
    Assert(m_func->IsJitInDebugMode());
    Assert(byteCodeOffset != Js::Constants::NoByteCodeOffset);

    BailOutInfo * bailOutInfo = JitAnew(m_func->m_alloc, BailOutInfo, byteCodeOffset, m_func);
    IR::BailOutInstr * instr = IR::BailOutInstr::New(Js::OpCode::BailForDebugger, kind, bailOutInfo, bailOutInfo->bailOutFunc);
    if (insertBeforeInstr)
    {
        instr->SetByteCodeOffset(byteCodeOffset);
        uint32 offset = insertBeforeInstr->GetByteCodeOffset();
        if (m_offsetToInstruction[offset] == insertBeforeInstr)
        {
            m_offsetToInstruction[offset] = instr;
        }
        insertBeforeInstr->InsertBefore(instr);
    }
    else
    {
        this->AddInstr(instr, m_lastInstr->GetByteCodeOffset());
    }
}

bool
IRBuilder::DoBailOnNoProfile()
{
    if (PHASE_OFF(Js::BailOnNoProfilePhase, this->m_func->GetTopFunc()))
    {
        return false;
    }

    Func *const topFunc = m_func->GetTopFunc();
    if(topFunc->m_jitTimeData->GetProfiledIterations() == 0)
    {
        // The top function has not been profiled yet. Some switch must have been used to force jitting. This is not a
        // real-world case, but for the purpose of testing the JIT, it's beneficial to generate code in unprofiled paths.
        return false;
    }

    if (this->m_func->GetProfileInfo()->IsNoProfileBailoutsDisabled())
    {
        return false;
    }

    if (!m_func->DoGlobOpt() || m_func->GetTopFunc()->HasTry())
    {
        return false;
    }

#ifdef ENABLE_DEBUG_CONFIG_OPTIONS
    if (this->m_func->GetTopFunc() != this->m_func && Js::Configuration::Global.flags.IsEnabled(Js::ForceJITLoopBodyFlag))
    {
        // No profile data for loop bodies with -force...
        return false;
    }
#endif

    if (!this->m_func->HasProfileInfo())
    {
        return false;
    }

    return true;
}

void
IRBuilder::InsertBailOnNoProfile(uint offset)
{
    Assert(DoBailOnNoProfile());

    if (this->callTreeHasSomeProfileInfo)
    {
        return;
    }

    IR::Instr *startCall = nullptr;
    int count = 0;
    FOREACH_SLIST_ENTRY(IR::Instr *, argInstr, this->m_argStack)
    {
        if (argInstr->m_opcode == Js::OpCode::StartCall)
        {
            startCall = argInstr;
            count++;
            if (count > 1)
            {
                return;
            }
        }
    } NEXT_SLIST_ENTRY;

    AnalysisAssert(startCall);

    if (startCall->m_prev->m_opcode != Js::OpCode::BailOnNoProfile)
    {
        InsertBailOnNoProfile(startCall);
    }
}

void IRBuilder::InsertBailOnNoProfile(IR::Instr *const insertBeforeInstr)
{
    Assert(DoBailOnNoProfile());

    IR::Instr *const bailOnNoProfileInstr = IR::Instr::New(Js::OpCode::BailOnNoProfile, m_func);
    bailOnNoProfileInstr->SetByteCodeOffset(insertBeforeInstr);
    uint32 offset = insertBeforeInstr->GetByteCodeOffset();
    if (m_offsetToInstruction[offset] == insertBeforeInstr)
    {
        m_offsetToInstruction[offset] = bailOnNoProfileInstr;
    }
    insertBeforeInstr->InsertBefore(bailOnNoProfileInstr);
}

#ifdef BAILOUT_INJECTION
void
IRBuilder::InjectBailOut(uint offset)
{
    if(m_func->IsSimpleJit())
    {
        return; // bailout injection is only applicable to full JIT
    }

    IR::IntConstOpnd * opnd = IR::IntConstOpnd::New(0, TyInt32, m_func);
    uint bailOutOffset = offset;
    if (bailOutOffset == Js::Constants::NoByteCodeOffset)
    {
        bailOutOffset = m_lastInstr->GetByteCodeOffset();
    }
    BailOutInfo * bailOutInfo = JitAnew(m_func->m_alloc, BailOutInfo, bailOutOffset, m_func);
    IR::BailOutInstr * instr = IR::BailOutInstr::New(Js::OpCode::BailOnEqual, IR::BailOutInjected, bailOutInfo, bailOutInfo->bailOutFunc);
    instr->SetSrc1(opnd);
    instr->SetSrc2(opnd);
    this->AddInstr(instr, offset);
}

void
IRBuilder::CheckBailOutInjection(Js::OpCode opcode)
{
    // Detect these sequence and disable Bailout injection between them:
    //   LdStackArgPtr
    //   LdArgCnt
    //   ApplyArgs
    // This assumes that LdStackArgPtr, LdArgCnt and ApplyArgs can
    // only be emitted in these sequence. This will need to be modified if it changes.
    //
    // Also insert a single bailout before the beginning of a switch case block for all the case labels.
    switch(opcode)
    {
    case Js::OpCode::LdStackArgPtr:
        Assert(!seenLdStackArgPtr);
        Assert(!expectApplyArg);
        seenLdStackArgPtr = true;
        break;
    case Js::OpCode::LdArgCnt:
        Assert(seenLdStackArgPtr);
        Assert(!expectApplyArg);
        expectApplyArg = true;
        break;
    case Js::OpCode::ApplyArgs:
        Assert(seenLdStackArgPtr);
        Assert(expectApplyArg);
        seenLdStackArgPtr = false;
        expectApplyArg = false;
        break;

    case Js::OpCode::BeginSwitch:
    case Js::OpCode::ProfiledBeginSwitch:
        Assert(!seenProfiledBeginSwitch);
        seenProfiledBeginSwitch = true;
        break;
    case Js::OpCode::EndSwitch:
        Assert(seenProfiledBeginSwitch);
        seenProfiledBeginSwitch = false;
        break;

    default:
        Assert(!seenLdStackArgPtr);
        Assert(!expectApplyArg);
        break;
    }
}
#endif

bool
IRBuilder::IsLoopBody() const
{
    return m_func->IsLoopBody();
}

bool
IRBuilder::IsLoopBodyInTry() const
{
    return m_func->IsLoopBodyInTry();
}

bool
IRBuilder::IsLoopBodyReturnIPInstr(IR::Instr * instr) const
{
     IR::Opnd * dst = instr->GetDst();
     return (dst && dst->IsRegOpnd() && dst->AsRegOpnd()->m_sym == m_loopBodyRetIPSym);
}


bool
IRBuilder::IsLoopBodyOuterOffset(uint offset) const
{
    if (!IsLoopBody())
    {
        return false;
    }

    JsLoopBodyCodeGen* loopBodyCodeGen = (JsLoopBodyCodeGen*)m_func->m_workItem;
    return (offset >= loopBodyCodeGen->loopHeader->endOffset || offset < loopBodyCodeGen->loopHeader->startOffset);
}

uint
IRBuilder::GetLoopBodyExitInstrOffset() const
{
    // End of loop body, start of StSlot and Ret instruction at endOffset + 1
    return ((JsLoopBodyCodeGen*)m_func->m_workItem)->loopHeader->endOffset + 1;
}

Js::RegSlot
IRBuilder::GetEnvReg() const
{
    return this->m_func->GetJnFunction()->GetEnvReg();
}

Js::RegSlot
IRBuilder::GetEnvRegForInnerFrameDisplay() const
{
    Js::RegSlot envReg = this->m_func->GetJnFunction()->GetLocalFrameDisplayReg();
    if (envReg == Js::Constants::NoRegister)
    {
        envReg = this->GetEnvReg();
    }

    return envReg;
}

void
IRBuilder::AddEnvOpndForInnerFrameDisplay(IR::Instr *instr, uint offset)
{
    Js::RegSlot envReg = this->GetEnvRegForInnerFrameDisplay();
    if (envReg != Js::Constants::NoRegister)
    {
        IR::RegOpnd *src2Opnd;
        if (envReg == this->m_functionBody->GetLocalFrameDisplayReg() &&
            this->m_functionBody->DoStackFrameDisplay() &&
            m_func->IsTopFunc())
        {
            src2Opnd = IR::RegOpnd::New(TyVar, m_func);
            this->AddInstr(
                IR::Instr::New(
                    Js::OpCode::LdSlot, src2Opnd,
                    this->BuildFieldOpnd(Js::OpCode::LdSlot, m_func->GetLocalFrameDisplaySym()->m_id, 0, (Js::PropertyIdIndexType)-1, PropertyKindSlots),
                    m_func),
                offset);
        }
        else
        {
            src2Opnd = this->BuildSrcOpnd(envReg);
        }
        instr->SetSrc2(src2Opnd);
    }
}

bool
IRBuilder::DoSlotArrayCheck(IR::SymOpnd *fieldOpnd, bool doDynamicCheck)
{
    if (PHASE_OFF(Js::ClosureRangeCheckPhase, m_func))
    {
        return true;
    }

    PropertySym *propertySym = fieldOpnd->m_sym->AsPropertySym();
    IR::Instr *instrDef = propertySym->m_stackSym->m_instrDef;
    IR::Opnd *allocOpnd = nullptr;

    if (instrDef == nullptr)
    {
        if (doDynamicCheck)
        {
            return false;
        }
        Js::Throw::FatalInternalError();
    }
    switch(instrDef->m_opcode)
    {
    case Js::OpCode::NewScopeSlots:
    case Js::OpCode::NewStackScopeSlots:
    case Js::OpCode::NewScopeSlotsWithoutPropIds:
        allocOpnd = instrDef->GetSrc1();
        break;

    case Js::OpCode::LdSlot:
    case Js::OpCode::LdSlotArr:
        if (doDynamicCheck)
        {
            return false;
        }
        // fall through
    default:
        Js::Throw::FatalInternalError();
    }

    uint32 allocCount = allocOpnd->AsIntConstOpnd()->AsUint32();
    uint32 slotId = (uint32)propertySym->m_propertyId;

    if (slotId >= allocCount)
    {
        Js::Throw::FatalInternalError();
    }

    return true;
}

///----------------------------------------------------------------------------
///
/// IRBuilder::Build
///
///     IRBuilder main entry point.  Read the bytecode for this function and
///     generate IR.
///
///----------------------------------------------------------------------------

void
IRBuilder::Build()
{
    m_funcAlloc = m_func->m_alloc;


    NoRecoverMemoryJitArenaAllocator localAlloc(L"BE-IRBuilder", m_funcAlloc->GetPageAllocator(), Js::Throw::OutOfMemory);
    m_tempAlloc = &localAlloc;

    uint32 offset;
    uint32 statementIndex = m_statementReader.GetStatementIndex();

    m_argStack = JitAnew(m_tempAlloc, SList<IR::Instr *>, m_tempAlloc);

    this->branchRelocList = JitAnew(m_tempAlloc, SList<BranchReloc *>, m_tempAlloc);
    Func * topFunc = this->m_func->GetTopFunc();
    if (topFunc->HasTry() &&
        ((!topFunc->HasFinally() && !topFunc->IsLoopBody() && !PHASE_OFF(Js::OptimizeTryCatchPhase, topFunc)) ||
        (topFunc->IsSimpleJit() && topFunc->GetJnFunction()->DoJITLoopBody()))) // should be relaxed as more bailouts are added in Simple Jit
    {
        this->catchOffsetStack = JitAnew(m_tempAlloc, SList<uint>, m_tempAlloc);
    }

    // Set up for renaming of temp registers to allow us to identify distinct lifetimes.
    Js::FunctionBody *funcBody = m_functionBody;

    this->firstTemp = funcBody->GetFirstTmpReg();
    Js::RegSlot tempCount = funcBody->GetTempCount();
    if (tempCount > 0)
    {
        this->tempMap = (SymID*)m_tempAlloc->AllocZero(sizeof(SymID) * tempCount);
        this->fbvTempUsed = BVFixed::New<JitArenaAllocator>(tempCount, m_tempAlloc);
    }
    else
    {
        this->tempMap = nullptr;
        this->fbvTempUsed = nullptr;
    }

    m_func->m_headInstr = IR::EntryInstr::New(Js::OpCode::FunctionEntry, m_func);
    m_func->m_exitInstr = IR::ExitInstr::New(Js::OpCode::FunctionExit, m_func);
    m_func->m_tailInstr = m_func->m_exitInstr;
    m_func->m_headInstr->InsertAfter(m_func->m_tailInstr);
    m_func->m_isLeaf = true;  // until proven otherwise

    if (this->m_func->GetJnFunction()->GetLocalClosureReg() != Js::Constants::NoRegister)
    {
        m_func->InitLocalClosureSyms();
    }

    m_functionStartOffset = m_jnReader.GetCurrentOffset();
    m_lastInstr = m_func->m_headInstr;

    AssertMsg(sizeof(SymID) >= sizeof(Js::RegSlot), "sizeof(SymID) != sizeof(Js::RegSlot)!!");

    offset = m_functionStartOffset;

    // Skip the last EndOfBlock opcode
    Assert(!OpCodeAttr::HasMultiSizeLayout(Js::OpCode::EndOfBlock));
    uint32 lastOffset = m_functionBody->GetByteCode()->GetLength() - Js::OpCodeUtil::EncodedSize(Js::OpCode::EndOfBlock, Js::SmallLayout);
    uint32 offsetToInstructionCount = lastOffset;
    if (this->IsLoopBody())
    {
        // LdSlot needs to cover all the register, including the temps, because we might treat
        // those as if they are local for the value of the with statement
        this->m_ldSlots = BVFixed::New<JitArenaAllocator>(funcBody->GetLocalsCount(), m_tempAlloc);
        this->m_stSlots = BVFixed::New<JitArenaAllocator>(funcBody->GetFirstTmpReg(), m_tempAlloc);
        this->m_loopBodyRetIPSym = StackSym::New(TyInt32, this->m_func);
#if DBG
        if (funcBody->GetTempCount() != 0)
        {
            this->m_usedAsTemp = BVFixed::New<JitArenaAllocator>(funcBody->GetTempCount(), m_tempAlloc);
        }
#endif
        JsLoopBodyCodeGen* loopBodyCodeGen = (JsLoopBodyCodeGen*)m_func->m_workItem;
        lastOffset = loopBodyCodeGen->loopHeader->endOffset;
        // Ret is created at lastOffset + 1, so we need lastOffset + 2 entries
        offsetToInstructionCount = lastOffset + 2;

        // Compute the offset of the start of the locals array as a Var index.
        size_t localsOffset = Js::InterpreterStackFrame::GetOffsetOfLocals();
        Assert(localsOffset % sizeof(Js::Var) == 0);
        this->m_loopBodyLocalsStartSlot = (Js::PropertyId)(localsOffset / sizeof(Js::Var));
    }

#if DBG
    m_offsetToInstructionCount = offsetToInstructionCount;
#endif
    m_offsetToInstruction = JitAnewArrayZ(m_tempAlloc, IR::Instr *, offsetToInstructionCount);

#ifdef BYTECODE_BRANCH_ISLAND
    longBranchMap = JitAnew(m_tempAlloc, LongBranchMap, m_tempAlloc);
#endif

    m_switchBuilder.Init(m_func, m_tempAlloc, false);

    this->BuildConstantLoads();
    this->BuildGeneratorPreamble();

    if (!this->IsLoopBody() && funcBody->GetHasImplicitArgIns())
    {
        this->BuildImplicitArgIns();
    }

    if (!this->IsLoopBody() && funcBody->GetHasRestParameter())
    {
        this->BuildArgInRest();
    }

    if (m_func->IsJitInDebugMode())
    {
        // This is first bailout in the function, the locals at stack have not initialized to undefined, so do not restore them.
        this->InsertBailOutForDebugger(offset, IR::BailOutForceByFlag | IR::BailOutBreakPointInFunction | IR::BailOutStep, nullptr);
    }

#ifdef BAILOUT_INJECTION
    // Start bailout inject after the constant and arg load. We don't bailout before that
    IR::Instr * lastInstr = m_lastInstr;
#endif

    if (m_statementReader.AtStatementBoundary(&m_jnReader))
    {
        statementIndex = this->AddStatementBoundary(statementIndex, offset);
    }

    if (!this->IsLoopBody())
    {
        IR::Instr *instr;

        // Do the implicit operations LdEnv, NewScopeSlots, LdFrameDisplay, as indicated by function body attributes.
        Js::RegSlot envReg = funcBody->GetEnvReg();
        if (envReg != Js::Constants::NoRegister && !this->RegIsConstant(envReg))
        {
            Js::OpCode newOpcode;
            Js::RegSlot thisReg = funcBody->GetThisRegForEventHandler();
            IR::RegOpnd *srcOpnd = nullptr;
            IR::RegOpnd *dstOpnd = nullptr;
            if (thisReg != Js::Constants::NoRegister)
            {
                this->BuildArgIn0(offset, thisReg);

                srcOpnd = BuildSrcOpnd(thisReg);
                newOpcode = Js::OpCode::LdHandlerScope;
            }
            else
            {
                newOpcode = Js::OpCode::LdEnv;
            }
            dstOpnd = BuildDstOpnd(envReg);
            instr = IR::Instr::New(newOpcode, dstOpnd, m_func);
            if (srcOpnd)
            {
                instr->SetSrc1(srcOpnd);
            }
            if (dstOpnd->m_sym->m_isSingleDef)
            {
                dstOpnd->m_sym->m_isNotInt = true;
            }
            this->AddInstr(instr, offset);
        }

        Js::RegSlot frameDisplayReg = funcBody->GetLocalFrameDisplayReg();
        Js::RegSlot funcExprScopeReg = funcBody->GetFuncExprScopeReg();
        IR::RegOpnd *frameDisplayOpnd = nullptr;
        if (funcExprScopeReg != Js::Constants::NoRegister)
        {
            IR::RegOpnd *funcExprScopeOpnd = BuildDstOpnd(funcExprScopeReg);
            instr = IR::Instr::New(Js::OpCode::NewPseudoScope, funcExprScopeOpnd, m_func);
            this->AddInstr(instr, (uint)-1);

            frameDisplayOpnd = BuildDstOpnd(frameDisplayReg);
            instr = IR::Instr::New(Js::OpCode::LdFrameDisplay, frameDisplayOpnd, funcExprScopeOpnd, m_func);
            if (envReg != Js::Constants::NoRegister)
            {
                instr->SetSrc2(BuildSrcOpnd(envReg));
            }
            this->AddInstr(instr, (uint)-1);
        }

        Js::RegSlot closureReg = funcBody->GetLocalClosureReg();
        IR::RegOpnd *closureOpnd = nullptr;
        if (closureReg != Js::Constants::NoRegister)
        {
            Assert(!this->RegIsConstant(closureReg));
            if (m_func->DoStackScopeSlots())
            {
                closureOpnd = IR::RegOpnd::New(TyVar, m_func);
            }
            else
            {
                closureOpnd = this->BuildDstOpnd(closureReg);
            }
            if (funcBody->HasScopeObject())
            {
                if (funcBody->HasCachedScopePropIds())
                {
                    this->BuildInitCachedScope(0, offset);
                }
                else
                {
                    instr = IR::Instr::New(Js::OpCode::NewScopeObject, closureOpnd, m_func);
                    this->AddInstr(instr, offset);
                }
            }
            else
            {
                Js::OpCode op = 
                    m_func->DoStackScopeSlots() ? Js::OpCode::NewStackScopeSlots : Js::OpCode::NewScopeSlots;

                IR::Opnd * srcOpnd = IR::IntConstOpnd::New(
                    m_func->GetJnFunction()->scopeSlotArraySize + Js::ScopeSlots::FirstSlotIndex, TyUint32, m_func);
                instr = IR::Instr::New(op, closureOpnd, srcOpnd, m_func);
                this->AddInstr(instr, offset);
            }
            if (closureOpnd->m_sym->m_isSingleDef)
            {
                closureOpnd->m_sym->m_isNotInt = true;
            }

            if (m_func->DoStackScopeSlots())
            {
                // Init the stack closure sym and use it to save the scope slot pointer.
                this->AddInstr(
                    IR::Instr::New(
                        Js::OpCode::InitLocalClosure, this->BuildDstOpnd(m_func->GetLocalClosureSym()->m_id), m_func),
                    (uint32)-1);

                this->AddInstr(
                    IR::Instr::New(
                        Js::OpCode::StSlot,
                        this->BuildFieldOpnd(
                            Js::OpCode::StSlot, m_func->GetLocalClosureSym()->m_id, 0, (Js::PropertyIdIndexType)-1, PropertyKindSlots),
                        closureOpnd, m_func),
                    (uint32)-1);
            }
        }

        if (frameDisplayReg != Js::Constants::NoRegister && closureReg != Js::Constants::NoRegister)
        {
            Assert(!this->RegIsConstant(frameDisplayReg));

            Js::OpCode op = m_func->DoStackScopeSlots() ? Js::OpCode::NewStackFrameDisplay : Js::OpCode::LdFrameDisplay;

            IR::RegOpnd *dstOpnd;
            if (m_func->DoStackScopeSlots() && m_func->IsTopFunc())
            {
                dstOpnd = IR::RegOpnd::New(TyVar, m_func);
            }
            else
            {
                dstOpnd = this->BuildDstOpnd(frameDisplayReg);
            }
            IR::Instr *instr = IR::Instr::New(op, dstOpnd, closureOpnd, m_func);
            if (frameDisplayOpnd != nullptr)
            {
                instr->SetSrc2(frameDisplayOpnd);
            }
            else if (envReg != Js::Constants::NoRegister)
            {
                instr->SetSrc2(this->BuildSrcOpnd(envReg));
            }
            this->AddInstr(instr, offset);
            if (dstOpnd->m_sym->m_isSingleDef)
            {
                dstOpnd->m_sym->m_isNotInt = true;
            }

            if (m_func->DoStackFrameDisplay())
            {
                // Use the stack closure sym to save the frame display pointer.
                this->AddInstr(
                    IR::Instr::New(
                        Js::OpCode::InitLocalClosure, this->BuildDstOpnd(m_func->GetLocalFrameDisplaySym()->m_id), m_func),
                    (uint32)-1);

                this->AddInstr(
                    IR::Instr::New(
                        Js::OpCode::StSlot,
                        this->BuildFieldOpnd(Js::OpCode::StSlot, m_func->GetLocalFrameDisplaySym()->m_id, 0, (Js::PropertyIdIndexType)-1, PropertyKindSlots),
                        dstOpnd, m_func),
                    (uint32)-1);
            }
        }
    }

    // For label instr we can add bailout only after all labels were finalized. Put to list/add in the end.
    JsUtil::BaseDictionary<IR::Instr*, int, JitArenaAllocator> ignoreExBranchInstrToOffsetMap(m_tempAlloc);

    Js::LayoutSize layoutSize;
    for (Js::OpCode newOpcode = m_jnReader.ReadOp(layoutSize); (uint)m_jnReader.GetCurrentOffset() <= lastOffset; newOpcode = m_jnReader.ReadOp(layoutSize))
    {
        Assert(newOpcode != Js::OpCode::EndOfBlock);

#ifdef BAILOUT_INJECTION
        if (!this->m_func->GetTopFunc()->HasTry()
#ifdef BYTECODE_BRANCH_ISLAND
            && newOpcode != Js::OpCode::BrLong  // Don't inject bailout on BrLong as they are just redirecting to a different offset anyways
#endif
            )
        {
            if (!seenLdStackArgPtr && !seenProfiledBeginSwitch)
            {
                if(Js::Configuration::Global.flags.IsEnabled(Js::BailOutByteCodeFlag))
                {
                    ThreadContext * threadContext = this->m_func->GetScriptContext()->GetThreadContext();
                    if (Js::Configuration::Global.flags.BailOutByteCode.Contains(threadContext->bailOutByteCodeLocationCount))
                    {
                        this->InjectBailOut(offset);
                    }
                }
                else if(Js::Configuration::Global.flags.IsEnabled(Js::BailOutAtEveryByteCodeFlag))
                {
                    this->InjectBailOut(offset);
                }
            }

            CheckBailOutInjection(newOpcode);
        }
#endif
        AssertMsg(Js::OpCodeUtil::IsValidByteCodeOpcode(newOpcode), "Error getting opcode from m_jnReader.Op()");

        uint layoutAndSize = layoutSize * Js::OpLayoutType::Count + Js::OpCodeUtil::GetOpCodeLayout(newOpcode);
        switch(layoutAndSize)
        {
#define LAYOUT_TYPE(layout) \
        case Js::OpLayoutType::layout: \
            Assert(layoutSize == Js::SmallLayout); \
            this->Build##layout(newOpcode, offset); \
            break;
#define LAYOUT_TYPE_WMS(layout) \
        case Js::SmallLayout * Js::OpLayoutType::Count + Js::OpLayoutType::layout: \
            this->Build##layout<Js::SmallLayoutSizePolicy>(newOpcode, offset); \
            break; \
        case Js::MediumLayout * Js::OpLayoutType::Count + Js::OpLayoutType::layout: \
            this->Build##layout<Js::MediumLayoutSizePolicy>(newOpcode, offset); \
            break; \
        case Js::LargeLayout * Js::OpLayoutType::Count + Js::OpLayoutType::layout: \
            this->Build##layout<Js::LargeLayoutSizePolicy>(newOpcode, offset); \
            break;
#include "ByteCode\LayoutTypes.h"

        default:
            AssertMsg(0, "Unimplemented layout");
            break;
        }

#ifdef BAILOUT_INJECTION
        if (!this->m_func->GetTopFunc()->HasTry() && Js::Configuration::Global.flags.IsEnabled(Js::BailOutByteCodeFlag))
        {
            ThreadContext * threadContext = this->m_func->GetScriptContext()->GetThreadContext();
            if (lastInstr != m_lastInstr)
            {
                lastInstr = lastInstr->GetNextRealInstr();
                if (lastInstr->HasBailOutInfo())
                {
                    lastInstr = lastInstr->m_next;
                }
                lastInstr->bailOutByteCodeLocation = threadContext->bailOutByteCodeLocationCount;
                lastInstr = m_lastInstr;
            }
            threadContext->bailOutByteCodeLocationCount++;
        }
#endif
        if (IsLoopBodyInTry() && m_lastInstr->GetDst() && m_lastInstr->GetDst()->IsRegOpnd() && m_lastInstr->GetDst()->GetStackSym()->HasByteCodeRegSlot())
        {
            StackSym * dstSym = m_lastInstr->GetDst()->GetStackSym();
            Js::RegSlot dstRegSlot = dstSym->GetByteCodeRegSlot();
            if (!this->RegIsTemp(dstRegSlot) && !this->RegIsConstant(dstRegSlot))
            {
                SymID symId = dstSym->m_id;
                if (this->m_stSlots->Test(symId))
                {
                    // For jitted loop bodies that are in a try block, we consider any symbol that has a
                    // non-temp bytecode reg slot, to be write-through. Hence, generating StSlots at all
                    // defs for such symbols
                    IR::Instr * stSlot = this->GenerateLoopBodyStSlot(dstRegSlot);
                    m_lastInstr->InsertAfter(stSlot);
                    m_lastInstr = stSlot;

                    this->m_stSlots->Clear(symId);
                }
                else
                {
                    Assert(dstSym->m_isCatchObjectSym);
                }
            }
        }

        offset = m_jnReader.GetCurrentOffset();

        if (m_func->IsJitInDebugMode())
        {
            bool needBailoutForHelper = CONFIG_FLAG(EnableContinueAfterExceptionWrappersForHelpers) &&
                (OpCodeAttr::NeedsPostOpDbgBailOut(newOpcode) ||
                    m_lastInstr->m_opcode == Js::OpCode::CallHelper && m_lastInstr->GetSrc1() &&
                    HelperMethodAttributes::CanThrow(m_lastInstr->GetSrc1()->AsHelperCallOpnd()->m_fnHelper));

            if (needBailoutForHelper)
            {
                // Insert bailout after return from a helper call.
                // For now use offset of next instr, when we get & ignore exception, we replace this with next statement offset.
                if (m_lastInstr->IsBranchInstr())
                {
                    // Debugger bailout on branches goes to different block which can become dead. Keep bailout with real instr.
                    // Can't convert to bailout at this time, can do that only after branches are finalized, remember for later.
                    ignoreExBranchInstrToOffsetMap.Add(m_lastInstr, offset);
                }
                else if (
                    m_lastInstr->m_opcode == Js::OpCode::Throw ||
                    m_lastInstr->m_opcode == Js::OpCode::RuntimeReferenceError ||
                    m_lastInstr->m_opcode == Js::OpCode::RuntimeTypeError)
                {
                    uint32 lastInstrOffset = m_lastInstr->GetByteCodeOffset();
                    Assert(lastInstrOffset < m_offsetToInstructionCount);
#if DBG
                    __analysis_assume(lastInstrOffset < this->m_offsetToInstructionCount);
#endif
                    bool isLastInstrUpdateNeeded = m_offsetToInstruction[lastInstrOffset] == m_lastInstr;

                    BailOutInfo * bailOutInfo = JitAnew(this->m_func->m_alloc, BailOutInfo, offset, this->m_func);
                    m_lastInstr = m_lastInstr->ConvertToBailOutInstr(bailOutInfo, c_debuggerBaseBailOutKindForHelper, true);

                    if (isLastInstrUpdateNeeded)
                    {
                        m_offsetToInstruction[lastInstrOffset] = m_lastInstr;
                    }
                }
                else
                {
                    IR::BailOutKind bailOutKind = c_debuggerBaseBailOutKindForHelper;
                    if (OpCodeAttr::HasImplicitCall(newOpcode) || OpCodeAttr::CallsValueOf(newOpcode))
                    {
                        // When we get out of e.g. valueOf called by a helper (e.g. Add_A) during stepping,
                        // we need to bail out to continue debugging calling function in interpreter,
                        // essentially this is similar to bail out on return from a method.
                        bailOutKind |= c_debuggerBailOutKindForCall;
                    }

                    this->InsertBailOutForDebugger(offset, bailOutKind);
                }
            }
        }

        while (m_statementReader.AtStatementBoundary(&m_jnReader))
        {
            statementIndex = this->AddStatementBoundary(statementIndex, offset);
        }
    }

    if (Js::Constants::NoStatementIndex != statementIndex)
    {
        // If we are inside a user statement then create a trailing line pragma instruction
        statementIndex = this->AddStatementBoundary(statementIndex, Js::Constants::NoByteCodeOffset);
    }

    if (IsLoopBody())
    {
        // Insert the LdSlot/StSlot and Ret
        IR::Opnd * retOpnd = this->InsertLoopBodyReturnIPInstr(offset, offset);

        // Restore and Ret are at the last offset + 1
        GenerateLoopBodySlotAccesses(lastOffset + 1);

        InsertDoneLoopBodyLoopCounter(lastOffset);

        IR::Instr *      retInstr = IR::Instr::New(Js::OpCode::Ret, m_func);
        retInstr->SetSrc1(retOpnd);
        this->AddInstr(retInstr, lastOffset + 1);
    }

    // Now fix up the targets for all the branches we've introduced.

    InsertLabels();

    Assert(!this->catchOffsetStack || this->catchOffsetStack->Empty());

    // Insert bailout for ignore exception for labels, after all labels were finalized.
    ignoreExBranchInstrToOffsetMap.Map([this](IR::Instr* instr, int byteCodeOffset) {
        BailOutInfo * bailOutInfo = JitAnew(this->m_func->m_alloc, BailOutInfo, byteCodeOffset, this->m_func);
        instr->ConvertToBailOutInstr(bailOutInfo, c_debuggerBaseBailOutKindForHelper, true);
    });

    // Now that we know whether the func is a leaf or not, decide whether we'll emit fast paths.
    // Do this once and for all, per-func, since the source size on the ThreadContext will be
    // changing while we JIT.

    if (this->m_func->IsTopFunc())
    {
        this->m_func->SetDoFastPaths();
        this->EmitClosureRangeChecks();
    }
}

void
IRBuilder::EmitClosureRangeChecks()
{
    // Emit closure range checks
    if (m_func->slotArrayCheckTable)
    {
        // Local slot array checks, should only be necessary in jitted loop bodies.
        FOREACH_HASHTABLE_ENTRY(uint32, bucket, m_func->slotArrayCheckTable)
        {
            uint32 slotId = bucket.element;
            Assert(slotId != (uint32)-1 && slotId >= Js::ScopeSlots::FirstSlotIndex);

            if (slotId > Js::ScopeSlots::FirstSlotIndex)
            {
                // Emit a SlotArrayCheck instruction, chained to the instruction (LdSlot) that defines the pointer.
                StackSym *stackSym = m_func->m_symTable->FindStackSym(bucket.value);
                Assert(stackSym && stackSym->m_instrDef);

                IR::Instr *instrDef = stackSym->m_instrDef;
                IR::Instr *insertInstr = instrDef->m_next;
                IR::RegOpnd *dstOpnd = instrDef->UnlinkDst()->AsRegOpnd();
                IR::Instr *instr = IR::Instr::New(Js::OpCode::SlotArrayCheck, dstOpnd, m_func);

                dstOpnd = IR::RegOpnd::New(TyVar, m_func);
                instrDef->SetDst(dstOpnd);
                instr->SetSrc1(dstOpnd);

                // Attach the slot ID to the check instruction.
                IR::IntConstOpnd *slotIdOpnd = IR::IntConstOpnd::New(bucket.element, TyUint32, m_func);
                instr->SetSrc2(slotIdOpnd);

                insertInstr->InsertBefore(instr);
            }
        }
        NEXT_HASHTABLE_ENTRY;
    }

    if (m_func->frameDisplayCheckTable)
    {
        // Frame display checks. Again, chain to the instruction (LdEnv/LdSlot).
        FOREACH_HASHTABLE_ENTRY(FrameDisplayCheckRecord*, bucket, m_func->frameDisplayCheckTable)
        {
            StackSym *stackSym = m_func->m_symTable->FindStackSym(bucket.value);
            Assert(stackSym && stackSym->m_instrDef);

            IR::Instr *instrDef = stackSym->m_instrDef;
            IR::Instr *insertInstr = instrDef->m_next;
            IR::RegOpnd *dstOpnd = instrDef->UnlinkDst()->AsRegOpnd();
            IR::Instr *instr = IR::Instr::New(Js::OpCode::FrameDisplayCheck, dstOpnd, m_func);

            dstOpnd = IR::RegOpnd::New(TyVar, m_func);
            instrDef->SetDst(dstOpnd);
            instr->SetSrc1(dstOpnd);

            // Attach the two-dimensional check info.
            IR::AddrOpnd *recordOpnd = IR::AddrOpnd::New(bucket.element, IR::AddrOpndKindDynamicMisc, m_func, true);
            instr->SetSrc2(recordOpnd);

            insertInstr->InsertBefore(instr);
        }
        NEXT_HASHTABLE_ENTRY;
    }

    // If not a loop, but there are loops and trys, restore scope slot pointer and FD
    if (!m_func->IsLoopBody() && m_func->HasTry() && m_func->GetJnFunction()->GetByteCodeInLoopCount() != 0)
    {
        BVSparse<JitArenaAllocator> * bv = nullptr;
        if (m_func->GetLocalClosureSym() && m_func->GetLocalClosureSym()->HasByteCodeRegSlot())
        {
            bv = JitAnew(m_func->m_alloc, BVSparse<JitArenaAllocator>, m_func->m_alloc);
            bv->Set(m_func->GetLocalClosureSym()->m_id);
        }
        if (m_func->GetLocalFrameDisplaySym() && m_func->GetLocalFrameDisplaySym()->HasByteCodeRegSlot())
        {
            if (!bv)
            {
                bv = JitAnew(m_func->m_alloc, BVSparse<JitArenaAllocator>, m_func->m_alloc);
            }
            bv->Set(m_func->GetLocalFrameDisplaySym()->m_id);
        }
        if (bv)
        {
            IR::ByteCodeUsesInstr * byteCodeUse = IR::ByteCodeUsesInstr::New(m_func);
            byteCodeUse->byteCodeUpwardExposedUsed = bv;
            FOREACH_INSTR_IN_FUNC_BACKWARD(instr, m_func)
            {
                if (instr->m_opcode == Js::OpCode::Ret)
                {
                    instr->InsertBefore(byteCodeUse);
                    break;
                }
            }
            NEXT_INSTR_IN_FUNC_BACKWARD;
        }
    }
}

///----------------------------------------------------------------------------
///
/// IRBuilder::InsertLabels
///
///     Insert label instructions at the offsets recorded in the branch reloc list.
///
///----------------------------------------------------------------------------

void
IRBuilder::InsertLabels()
{
    AssertMsg(this->branchRelocList, "Malformed branch reloc list");

    SList<BranchReloc *>::Iterator iter(this->branchRelocList);

    while (iter.Next())
    {
        IR::LabelInstr * labelInstr;
        BranchReloc * reloc = iter.Data();
        IR::BranchInstr * branchInstr = reloc->GetBranchInstr();
        uint offset = reloc->GetOffset();
        uint const branchOffset = reloc->GetBranchOffset();

        Assert(!IsLoopBody() || offset <= GetLoopBodyExitInstrOffset());

        if(branchInstr->m_opcode == Js::OpCode::MultiBr)
        {
            IR::MultiBranchInstr * multiBranchInstr = branchInstr->AsBranchInstr()->AsMultiBrInstr();

            multiBranchInstr->UpdateMultiBrTargetOffsets([&](uint32 offset) -> IR::LabelInstr *
            {
                labelInstr = this->CreateLabel(branchInstr, offset);
                multiBranchInstr->ChangeLabelRef(nullptr, labelInstr);
                return labelInstr;
            });
        }
        else
        {
            labelInstr = this->CreateLabel(branchInstr, offset);
            branchInstr->SetTarget(labelInstr);
        }

        if (!reloc->IsNotBackEdge() && branchOffset >= offset)
        {
            bool wasLoopTop = labelInstr->m_isLoopTop;
            labelInstr->m_isLoopTop = true;

            if (m_func->IsJitInDebugMode())
            {
                // Add bailout for Async Break.
                IR::BranchInstr* backEdgeBranchInstr = reloc->GetBranchInstr();
                this->InsertBailOutForDebugger(backEdgeBranchInstr->GetByteCodeOffset(), IR::BailOutForceByFlag | IR::BailOutBreakPointInFunction, backEdgeBranchInstr);
            }

            if (!wasLoopTop && m_loopCounterSym)
            {
                this->InsertIncrLoopBodyLoopCounter(labelInstr);
            }

        }
    }
}

IR::LabelInstr *
IRBuilder::CreateLabel(IR::BranchInstr * branchInstr, uint& offset)
{
    IR::LabelInstr *     labelInstr;
    IR::Instr *          targetInstr;

    for (;;)
    {
        Assert(offset < m_offsetToInstructionCount);
        targetInstr = this->m_offsetToInstruction[offset];
        if (targetInstr != nullptr)
        {
#ifdef BYTECODE_BRANCH_ISLAND
            // If we have a long branch, remap it to the target offset
            if (targetInstr == VirtualLongBranchInstr)
            {
                offset = ResolveVirtualLongBranch(branchInstr, offset);
                continue;
            }
#endif
            break;
        }
        offset++;
    }

    IR::Instr *instrPrev = targetInstr->m_prev;

    if (instrPrev)
    {
        instrPrev = targetInstr->GetPrevRealInstrOrLabel();
    }

    if (instrPrev && instrPrev->IsLabelInstr())
    {
        // Found an existing label at the right offset. Just reuse it.
        labelInstr = instrPrev->AsLabelInstr();
    }
    else
    {
        // No label at the desired offset. Create one.

        labelInstr = IR::LabelInstr::New( Js::OpCode::Label, this->m_func);
        labelInstr->SetByteCodeOffset(offset);
        if (instrPrev)
        {
            instrPrev->InsertAfter(labelInstr);
        }
        else
        {
            targetInstr->InsertBefore(labelInstr);
        }
    }
    return labelInstr;
}

///----------------------------------------------------------------------------
///
/// IRBuilder::AddInstr
///
///     Add an instruction to the current instr list.  Also add this instr to
///     offsetToinstruction table to patch branches/labels afterwards.
///
///----------------------------------------------------------------------------

void
IRBuilder::AddInstr(IR::Instr *instr, uint32 offset)
{
    m_lastInstr->InsertAfter(instr);
    if (offset != Js::Constants::NoByteCodeOffset)
    {
        Assert(offset < m_offsetToInstructionCount);
        if (m_offsetToInstruction[offset] == nullptr)
        {
            m_offsetToInstruction[offset] = instr;
        }
        else
        {
            Assert(m_lastInstr->GetByteCodeOffset() == offset);
        }
        instr->SetByteCodeOffset(offset);
    }
    else if (m_lastInstr)
    {
        instr->SetByteCodeOffset(m_lastInstr->GetByteCodeOffset());
    }
    m_lastInstr = instr;

    Func *topFunc = this->m_func->GetTopFunc();
    if (!topFunc->GetHasTempObjectProducingInstr())
    {
        if (OpCodeAttr::TempObjectProducing(instr->m_opcode))
        {
            topFunc->SetHasTempObjectProducingInstr(true);
        }
    }

#if DBG_DUMP
    if (Js::Configuration::Global.flags.Trace.IsEnabled(Js::IRBuilderPhase, this->m_func->GetTopFunc()->GetSourceContextId(), this->m_func->GetTopFunc()->GetLocalFunctionId()))
    {
        instr->Dump();
    }
#endif
}

IR::IndirOpnd *
IRBuilder::BuildIndirOpnd(IR::RegOpnd *baseReg, IR::RegOpnd *indexReg)
{
    IR::IndirOpnd *indirOpnd = IR::IndirOpnd::New(baseReg, indexReg, TyVar, m_func);
    return indirOpnd;
}

IR::IndirOpnd *
IRBuilder::BuildIndirOpnd(IR::RegOpnd *baseReg, uint32 offset)
{
    IR::IndirOpnd *indirOpnd = IR::IndirOpnd::New(baseReg, offset, TyVar, m_func);
    return indirOpnd;
}

#if DBG_DUMP || defined(ENABLE_IR_VIEWER)
IR::IndirOpnd *
IRBuilder::BuildIndirOpnd(IR::RegOpnd *baseReg, uint32 offset, const wchar_t *desc)
{
    IR::IndirOpnd *indirOpnd = IR::IndirOpnd::New(baseReg, offset, TyVar, desc, m_func);
    return indirOpnd;
}
#endif

IR::SymOpnd *
IRBuilder::BuildFieldOpnd(Js::OpCode newOpcode, Js::RegSlot reg, Js::PropertyId propertyId, Js::PropertyIdIndexType propertyIdIndex, PropertyKind propertyKind, uint inlineCacheIndex)
{
    PropertySym * propertySym = BuildFieldSym(reg, propertyId, propertyIdIndex, inlineCacheIndex, propertyKind);
    IR::SymOpnd * symOpnd;

    // If we plan to apply object type optimization to this instruction or if we intend to emit a fast path using an inline
    // cache, we will need a property sym operand.
    if (OpCodeAttr::FastFldInstr(newOpcode) || inlineCacheIndex != (uint)-1)
    {
        Assert(propertyKind == PropertyKindData);
        symOpnd = IR::PropertySymOpnd::New(propertySym, inlineCacheIndex, TyVar, this->m_func);

        if (inlineCacheIndex != (uint)-1 && propertySym->m_loadInlineCacheIndex == (uint)-1)
        {
            if (GlobOpt::IsPREInstrCandidateLoad(newOpcode))
            {
                propertySym->m_loadInlineCacheIndex = inlineCacheIndex;
                propertySym->m_loadInlineCacheFunc = this->m_func;
            }
        }
    }
    else
    {
        symOpnd = IR::SymOpnd::New(propertySym, TyVar, this->m_func);
    }

    return symOpnd;
}

PropertySym *
IRBuilder::BuildFieldSym(Js::RegSlot reg, Js::PropertyId propertyId, Js::PropertyIdIndexType propertyIdIndex, uint inlineCacheIndex, PropertyKind propertyKind)
{
    PropertySym * propertySym;
    SymID         symId = this->BuildSrcStackSymID(reg);

    AssertMsg(m_func->m_symTable->FindStackSym(symId), "Tried to use an undefined stacksym?");

    propertySym = PropertySym::FindOrCreate(symId, propertyId, propertyIdIndex, inlineCacheIndex, propertyKind, m_func);

    return propertySym;
}

SymID
IRBuilder::BuildSrcStackSymID(Js::RegSlot regSlot)
{
    SymID symID;

    if (this->RegIsTemp(regSlot))
    {
        // This is a use of a temp. Map the reg slot to its sym ID.
        //     !!!NOTE: always process an instruction's temp uses before its temp defs!!!
        symID = this->GetMappedTemp(regSlot);
        if (symID == 0)
        {
            // We might have temps that are live through the loop body via "with" statement
            // We need to treat those as if they are locals and don't remap them
            Assert(this->IsLoopBody());
            Assert(!this->m_usedAsTemp->Test(regSlot - m_func->GetJnFunction()->GetFirstTmpReg()));

            symID = static_cast<SymID>(regSlot);
            this->SetMappedTemp(regSlot, symID);
            this->EnsureLoopBodyLoadSlot(symID);
        }
        this->SetTempUsed(regSlot, TRUE);
    }
    else
    {
        symID = static_cast<SymID>(regSlot);
        if (IsLoopBody() && !this->RegIsConstant(regSlot))
        {
            this->EnsureLoopBodyLoadSlot(symID);
        }
    }
    return symID;
}

///----------------------------------------------------------------------------
///
/// IRBuilder::BuildSrcOpnd
///
///     Create a StackSym and return a RegOpnd for this RegSlot.
///
///----------------------------------------------------------------------------

IR::RegOpnd *
IRBuilder::BuildSrcOpnd(Js::RegSlot srcRegSlot, IRType type)
{
    StackSym * symSrc = m_func->m_symTable->FindStackSym(BuildSrcStackSymID(srcRegSlot));
    AssertMsg(symSrc, "Tried to use an undefined stack slot?");
    IR::RegOpnd *regOpnd = IR::RegOpnd::New(symSrc, type, m_func);

    return regOpnd;
}

///----------------------------------------------------------------------------
///
/// IRBuilder::BuildDstOpnd
///
///     Create a StackSym and return a RegOpnd for this RegSlot.
///     If the RegSlot is '0', it may have multiple defs, so use FindOrCreate.
///
///----------------------------------------------------------------------------

IR::RegOpnd *
IRBuilder::BuildDstOpnd(Js::RegSlot dstRegSlot, IRType type, bool isCatchObjectSym)
{
    StackSym *   symDst;
    SymID        symID;

    if (this->RegIsTemp(dstRegSlot))
    {
#if DBG
        if (this->IsLoopBody())
        {
            // If we are doing loop body, and a temp reg slot is loaded via LdSlot
            // That means that we have detected that the slot is live coming in to the loop.
            // This would only happen for the value of a "with" statement, so there shouldn't
            // be any def for those
            Assert(!this->m_ldSlots->Test(dstRegSlot));
            this->m_usedAsTemp->Set(dstRegSlot - this->m_func->GetJnFunction()->GetFirstTmpReg());
        }
#endif

        // This is a def of a temp. Create a new sym ID for it if it's been used since its last def.
        //     !!!NOTE: always process an instruction's temp uses before its temp defs!!!
        if (this->GetTempUsed(dstRegSlot))
        {
            symID = m_func->m_symTable->NewID();
            this->SetTempUsed(dstRegSlot, FALSE);
            this->SetMappedTemp(dstRegSlot, symID);
        }
        else
        {
            symID = this->GetMappedTemp(dstRegSlot);
            // The temp hasn't been used since its last def. There are 2 possibilities:
            if (symID == 0)
            {
                // First time we've seen the temp. Just use the number that the front end gave it.
                symID = static_cast<SymID>(dstRegSlot);
                this->SetMappedTemp(dstRegSlot, symID);
            }
        }

    }
    else
    {
        symID = static_cast<SymID>(dstRegSlot);
        if (this->RegIsConstant(dstRegSlot))
        {
            // Don't need to track constant registers for bailout. Don't set the byte code register for constant.
            dstRegSlot = Js::Constants::NoRegister;
        }
        else if (IsLoopBody())
        {
            // Loop body and not constants
            this->SetLoopBodyStSlot(symID, isCatchObjectSym);

            // We need to make sure that the symbols is loaded as well
            // so that the sym will be defined on all path.
            this->EnsureLoopBodyLoadSlot(symID, isCatchObjectSym);
        }
    }

    symDst = StackSym::FindOrCreate(symID, dstRegSlot, m_func);

    // Always reset isSafeThis to false.  We'll set it to true for singleDef cases,
    // but want to reset it to false if it is multi-def.
    // NOTE: We could handle the multiDef if they are all safe, but it probably isn't very common.
    symDst->m_isSafeThis = false;

    IR::RegOpnd *regOpnd =  IR::RegOpnd::New(symDst, type, m_func);
    return regOpnd;
}

void
IRBuilder::BuildImplicitArgIns()
{
    Js::FunctionBody * functionBody = this->m_func->GetJnFunction();
    Js::RegSlot startReg = functionBody->GetConstantCount() - 1;
    for (Js::ArgSlot i = 1; i < functionBody->GetInParamsCount(); i++)
    {
        this->BuildArgIn((uint32)-1, startReg + i, i);
    }
}

#if DBG_DUMP || defined(ENABLE_IR_VIEWER)
#define POINTER_OFFSET(opnd, c, field) \
    BuildIndirOpnd((opnd), c::Get##field##Offset(), L#c L"." L#field)
#else
#define POINTER_OFFSET(opnd, c, field) \
    BuildIndirOpnd((opnd), c::Get##field##Offset())
#endif

void
IRBuilder::BuildGeneratorPreamble()
{
    if (!this->m_func->GetJnFunction()->IsGenerator())
    {
        return;
    }

    // Build code to check if the generator already has state and if it does then jump to the corresponding resume point.
    // Otherwise jump to the start of the function.  The generator object is the first argument by convention established
    // in JavascriptGenerator::EntryNext/EntryReturn/EntryThrow.
    //
    // s1 = Ld_A prm1
    // s2 = Ld_A s1[offset of JavascriptGenerator::frame]
    //      BrAddr_A s2 nullptr $startOfFunc
    // s3 = Ld_A s2[offset of InterpreterStackFrame::m_reader.m_currentLocation]
    // s4 = Ld_A s2[offset of InterpreterStackFrame::m_reader.m_startLocation]
    // s5 = Sub_I4 s3 s4
    //      GeneratorResumeJumpTable s5
    // $startOfFunc:
    //

    StackSym *genParamSym = StackSym::NewParamSlotSym(1, this->m_func);
    this->m_func->SetArgOffset(genParamSym, LowererMD::GetFormalParamOffset() * MachPtr);

    IR::SymOpnd *genParamOpnd = IR::SymOpnd::New(genParamSym, TyMachPtr, this->m_func);
    IR::RegOpnd *genRegOpnd = IR::RegOpnd::New(TyMachPtr, this->m_func);
    IR::Instr *instr = IR::Instr::New(Js::OpCode::Ld_A, genRegOpnd, genParamOpnd, this->m_func);
    this->AddInstr(instr, Js::Constants::NoByteCodeOffset);

    IR::RegOpnd *genFrameOpnd = IR::RegOpnd::New(TyMachPtr, this->m_func);
    instr = IR::Instr::New(Js::OpCode::Ld_A, genFrameOpnd, POINTER_OFFSET(genRegOpnd, Js::JavascriptGenerator, Frame), this->m_func);
    this->AddInstr(instr, Js::Constants::NoByteCodeOffset);

    IR::LabelInstr *labelInstr = IR::LabelInstr::New(Js::OpCode::Label, this->m_func);
    IR::BranchInstr *branchInstr = IR::BranchInstr::New(Js::OpCode::BrAddr_A, labelInstr, genFrameOpnd, IR::AddrOpnd::NewNull(this->m_func), this->m_func);
    this->AddInstr(branchInstr, Js::Constants::NoByteCodeOffset);

    IR::RegOpnd *curLocOpnd = IR::RegOpnd::New(TyMachPtr, this->m_func);
    instr = IR::Instr::New(Js::OpCode::Ld_A, curLocOpnd, POINTER_OFFSET(genFrameOpnd, Js::InterpreterStackFrame, CurrentLocation), this->m_func);
    this->AddInstr(instr, Js::Constants::NoByteCodeOffset);

    IR::RegOpnd *startLocOpnd = IR::RegOpnd::New(TyMachPtr, this->m_func);
    instr = IR::Instr::New(Js::OpCode::Ld_A, startLocOpnd, POINTER_OFFSET(genFrameOpnd, Js::InterpreterStackFrame, StartLocation), this->m_func);
    this->AddInstr(instr, Js::Constants::NoByteCodeOffset);

    IR::RegOpnd *curOffsetOpnd = IR::RegOpnd::New(TyUint32, this->m_func);
    instr = IR::Instr::New(Js::OpCode::Sub_I4, curOffsetOpnd, curLocOpnd, startLocOpnd, this->m_func);
    this->AddInstr(instr, Js::Constants::NoByteCodeOffset);

    instr = IR::Instr::New(Js::OpCode::GeneratorResumeJumpTable, this->m_func);
    instr->SetSrc1(curOffsetOpnd);
    this->AddInstr(instr, Js::Constants::NoByteCodeOffset);

    this->AddInstr(labelInstr, Js::Constants::NoByteCodeOffset);
}

void
IRBuilder::BuildConstantLoads()
{
    Js::FunctionBody *func = this->m_func->GetJnFunction();
    Js::RegSlot count = func->GetConstantCount();

    for (Js::RegSlot reg = Js::FunctionBody::FirstRegSlot; reg < count; reg++)
    {
        Js::Var varConst = func->GetConstantVar(reg);
        Assert(varConst != nullptr);

        IR::RegOpnd *dstOpnd = this->BuildDstOpnd(reg);
        Assert(this->RegIsConstant(reg));
        dstOpnd->m_sym->SetIsFromByteCodeConstantTable();

        IR::Instr *instr = IR::Instr::NewConstantLoad(dstOpnd, varConst, m_func);
        this->AddInstr(instr, Js::Constants::NoByteCodeOffset);
    }

}


///----------------------------------------------------------------------------
///
/// IRBuilder::BuildReg1
///
///     Build IR instr for a Reg1 instruction.
///
///----------------------------------------------------------------------------

template <typename SizePolicy>
void
IRBuilder::BuildReg1(Js::OpCode newOpcode, uint32 offset)
{
    Assert(!OpCodeAttr::IsProfiledOp(newOpcode));
    Assert(OpCodeAttr::HasMultiSizeLayout(newOpcode));
    auto layout = m_jnReader.GetLayout<Js::OpLayoutT_Reg1<SizePolicy>>();

    if (!PHASE_OFF(Js::ClosureRegCheckPhase, m_func))
    {
        this->DoClosureRegCheck(layout->R0);
    }

    BuildReg1(newOpcode, offset, layout->R0);
}

void
IRBuilder::BuildReg1(Js::OpCode newOpcode, uint32 offset, Js::RegSlot R0)
{
    if (newOpcode == Js::OpCode::ArgIn0)
    {
        this->BuildArgIn0(offset, R0);
        return;
    }

    IR::Instr *     instr;
    Js::RegSlot     srcRegOpnd, dstRegSlot;
    srcRegOpnd = dstRegSlot = R0;

    IR::Opnd * srcOpnd = nullptr;
    bool isNotInt = false;
    bool dstIsCatchObject = false;
    ValueType dstValueType;
    switch (newOpcode)
    {
    case Js::OpCode::LdHeapArgsCached:
    case Js::OpCode::LdLetHeapArgsCached:
        this->m_func->SetHasArgumentObject();
        if (!m_func->GetJnFunction()->HasScopeObject())
        {
            Js::Throw::FatalInternalError();
        }
        srcOpnd = BuildSrcOpnd(m_func->GetJnFunction()->GetLocalClosureReg());
        isNotInt = true;        
        break;

    case Js::OpCode::LdLocalObj:
        if (!m_func->GetJnFunction()->HasScopeObject())
        {
            Js::Throw::FatalInternalError();
        }
        srcOpnd = BuildSrcOpnd(m_func->GetJnFunction()->GetLocalClosureReg());
        isNotInt = true;
        newOpcode = Js::OpCode::Ld_A;
        break;

    case Js::OpCode::Throw:
        {
            srcOpnd = this->BuildSrcOpnd(srcRegOpnd);

            if (this->catchOffsetStack && !this->catchOffsetStack->Empty())
            {
                newOpcode = Js::OpCode::EHThrow;
            }
            instr = IR::Instr::New(newOpcode, m_func);
            instr->SetSrc1(srcOpnd);

            this->AddInstr(instr, offset);

            if(DoBailOnNoProfile())
            {
                //So optimistically assume it doesn't throw and introduce bailonnoprofile here.
                //If there are continuous bailout bailonnoprofile will be disabled.
                InsertBailOnNoProfile(instr);
            }
            return;
        }
    case Js::OpCode::ReleaseForInEnumerator:
    case Js::OpCode::ObjectFreeze:
        {
            srcOpnd = this->BuildSrcOpnd(srcRegOpnd);

            instr = IR::Instr::New(newOpcode, m_func);
            instr->SetSrc1(srcOpnd);

            this->AddInstr(instr, offset);
            return;
        }

    case Js::OpCode::LdC_A_Null:
        {
            const auto addrOpnd = IR::AddrOpnd::New(m_func->GetScriptContext()->GetLibrary()->GetNull(), IR::AddrOpndKindDynamicVar, m_func, true);
            addrOpnd->SetValueType(ValueType::Null);
            srcOpnd = addrOpnd;
            newOpcode = Js::OpCode::Ld_A;
            break;
        }

    case Js::OpCode::LdUndef:
        {
            const auto addrOpnd = IR::AddrOpnd::New(m_func->GetScriptContext()->GetLibrary()->GetUndefined(), IR::AddrOpndKindDynamicVar, m_func, true);
            addrOpnd->SetValueType(ValueType::Undefined);
            srcOpnd = addrOpnd;
            newOpcode = Js::OpCode::Ld_A;
            break;
        }

    case Js::OpCode::LdInfinity:
        {
            const auto floatConstOpnd = IR::FloatConstOpnd::New(Js::JavascriptNumber::POSITIVE_INFINITY, TyFloat64, m_func);
            srcOpnd = floatConstOpnd;
            newOpcode = Js::OpCode::LdC_A_R8;
            break;
        }

    case Js::OpCode::LdNaN:
        {
            const auto floatConstOpnd = IR::FloatConstOpnd::New(Js::JavascriptNumber::NaN, TyFloat64, m_func);
            srcOpnd = floatConstOpnd;
            newOpcode = Js::OpCode::LdC_A_R8;
            break;
        }

    case Js::OpCode::LdFalse:
        {
            const auto addrOpnd = IR::AddrOpnd::New(m_func->GetScriptContext()->GetLibrary()->GetFalse(), IR::AddrOpndKindDynamicVar, m_func, true);
            addrOpnd->SetValueType(ValueType::Boolean);
            srcOpnd = addrOpnd;
            newOpcode = Js::OpCode::Ld_A;
            break;
        }

    case Js::OpCode::LdTrue:
        {
            const auto addrOpnd = IR::AddrOpnd::New(m_func->GetScriptContext()->GetLibrary()->GetTrue(), IR::AddrOpndKindDynamicVar, m_func, true);
            addrOpnd->SetValueType(ValueType::Boolean);
            srcOpnd = addrOpnd;
            newOpcode = Js::OpCode::Ld_A;
            break;
        }

    case Js::OpCode::NewScObjectSimple:
        dstValueType = ValueType::GetObject(ObjectType::UninitializedObject);
        // fall-through
    case Js::OpCode::LdFuncExpr:
        m_func->DisableCanDoInlineArgOpt();
        break;
    case Js::OpCode::LdEnv:
    case Js::OpCode::LdSuper:
    case Js::OpCode::LdSuperCtor:
        isNotInt = TRUE;
        break;

    case Js::OpCode::Unused:
        // Don't generate anything. Just indicate that the temp reg is used.
        Assert(this->RegIsTemp(dstRegSlot));
        this->SetTempUsed(dstRegSlot, TRUE);
        return;

    case Js::OpCode::InitUndecl:
        srcOpnd = IR::AddrOpnd::New(m_func->GetScriptContext()->GetLibrary()->GetUndeclBlockVar(), IR::AddrOpndKindDynamicVar, m_func, true);
        srcOpnd->SetValueType(ValueType::PrimitiveOrObject);
        newOpcode = Js::OpCode::Ld_A;
        break;

    case Js::OpCode::ChkUndecl:
        srcOpnd = BuildSrcOpnd(srcRegOpnd);
        instr = IR::Instr::New(Js::OpCode::ChkUndecl, m_func);
        instr->SetSrc1(srcOpnd);
        this->AddInstr(instr, offset);
        return;

    case Js::OpCode::Catch:
        if (this->catchOffsetStack)
        {
            this->catchOffsetStack->Pop();
        }
        dstIsCatchObject = true;
        break;
    }

    IR::RegOpnd *   dstOpnd = this->BuildDstOpnd(dstRegSlot, TyVar, dstIsCatchObject);
    dstOpnd->SetValueType(dstValueType);
    StackSym *      dstSym = dstOpnd->m_sym;
    dstSym->m_isCatchObjectSym = dstIsCatchObject;

    instr = IR::Instr::New(newOpcode, dstOpnd, m_func);
    if (srcOpnd)
    {
        instr->SetSrc1(srcOpnd);
        if (dstSym->m_isSingleDef)
        {
            if (srcOpnd->IsHelperCallOpnd())
            {
                // Don't do anything
            }
            else if (srcOpnd->IsIntConstOpnd())
            {
                dstSym->SetIsIntConst(srcOpnd->AsIntConstOpnd()->GetValue());
            }
            else if (srcOpnd->IsFloatConstOpnd())
            {
                dstSym->SetIsFloatConst();
            }
            else if (srcOpnd->IsAddrOpnd())
            {
                dstSym->m_isConst = true;
                dstSym->m_isNotInt = true;
            }
        }
    }
    if (isNotInt && dstSym->m_isSingleDef)
    {
        dstSym->m_isNotInt = true;
    }

    this->AddInstr(instr, offset);
}

///----------------------------------------------------------------------------
///
/// IRBuilder::BuildReg2
///
///     Build IR instr for a Reg2 instruction.
///
///----------------------------------------------------------------------------

template <typename SizePolicy>
void
IRBuilder::BuildReg2(Js::OpCode newOpcode, uint32 offset)
{
    Assert(!OpCodeAttr::IsProfiledOp(newOpcode) || newOpcode == Js::OpCode::ProfiledStrictLdThis);
    Assert(OpCodeAttr::HasMultiSizeLayout(newOpcode));
    auto layout = m_jnReader.GetLayout<Js::OpLayoutT_Reg2<SizePolicy>>();

    if (!PHASE_OFF(Js::ClosureRegCheckPhase, m_func))
    {
        this->DoClosureRegCheck(layout->R0);
        this->DoClosureRegCheck(layout->R1);
    }

    BuildReg2(newOpcode, offset, layout->R0, layout->R1, m_jnReader.GetCurrentOffset());
}

void
IRBuilder::BuildReg2(Js::OpCode newOpcode, uint32 offset, Js::RegSlot R0, Js::RegSlot R1, uint32 nextOffset)
{
    IR::RegOpnd *   src1Opnd = this->BuildSrcOpnd(R1);
    StackSym *      symSrc1 = src1Opnd->m_sym;

    switch (newOpcode)
    {
    case Js::OpCode::LdHeapArguments:
    case Js::OpCode::LdLetHeapArguments:
    {
        IR::Opnd * opndFrameObj;
        if (m_func->GetJnFunction()->HasScopeObject() && 
            src1Opnd->m_sym->m_instrDef &&
            src1Opnd->m_sym->m_instrDef->m_opcode == Js::OpCode::LdPropIds)
        {
            Js::RegSlot regFrameObj = m_func->GetJnFunction()->GetLocalClosureReg();
            Assert(regFrameObj != Js::Constants::NoRegister);
            opndFrameObj = BuildSrcOpnd(regFrameObj);
        }
        else
        {
            opndFrameObj = IR::AddrOpnd::New(
                m_func->GetScriptContext()->GetLibrary()->GetNull(), IR::AddrOpndKindDynamicVar, m_func, true);
        }
        IR::RegOpnd * dstOpnd = BuildDstOpnd(R0);
        IR::Instr * instr = IR::Instr::New(newOpcode, dstOpnd, opndFrameObj, src1Opnd, m_func);  
        this->AddInstr(instr, offset);
        StackSym * dstSym = dstOpnd->m_sym;
        if (dstSym->m_isSingleDef)
        {
            dstSym->m_isSafeThis = true;
            dstSym->m_isNotInt = true;
        }
        this->m_func->SetHasArgumentObject();
        return;
    }
    case Js::OpCode::SetHomeObj:
    {
        IR::Instr *instr = IR::Instr::New(Js::OpCode::SetHomeObj, m_func);
        instr->SetSrc1(this->BuildSrcOpnd(R0));
        instr->SetSrc2(src1Opnd);
        this->AddInstr(instr, offset);
        return;
    }
    case Js::OpCode::SetComputedNameVar:
    {
        IR::Instr *instr = IR::Instr::New(Js::OpCode::SetComputedNameVar, m_func);
        instr->SetSrc1(this->BuildSrcOpnd(R0));
        instr->SetSrc2(src1Opnd);
        this->AddInstr(instr, offset);
        return;
    }
    case Js::OpCode::LdFuncExprFrameDisplay:
    {
        IR::RegOpnd *dstOpnd = IR::RegOpnd::New(TyVar, m_func);
        IR::Instr *instr = IR::Instr::New(Js::OpCode::LdFrameDisplay, dstOpnd, src1Opnd, m_func);
        Js::RegSlot envReg = this->GetEnvReg();
        if (envReg != Js::Constants::NoRegister)
        {
            instr->SetSrc2(BuildSrcOpnd(envReg));
        }
        this->AddInstr(instr, offset);

        IR::RegOpnd *src2Opnd = dstOpnd;
        src1Opnd = BuildSrcOpnd(R0);
        dstOpnd = BuildDstOpnd(this->m_func->GetJnFunction()->GetLocalFrameDisplayReg());
        instr = IR::Instr::New(Js::OpCode::LdFrameDisplay, dstOpnd, src1Opnd, src2Opnd, m_func);
        dstOpnd->m_sym->m_isNotInt = true;
        this->AddInstr(instr, offset);
        return;
    }
    }

    IR::RegOpnd *   dstOpnd = this->BuildDstOpnd(R0);
    StackSym *      dstSym = dstOpnd->m_sym;

    IR::Instr * instr = nullptr;
    switch (newOpcode)
    {
    case Js::OpCode::Ld_A:
        if (symSrc1->m_builtInIndex != Js::BuiltinFunction::None)
        {
            // Note: don't set dstSym->m_builtInIndex to None here (see Win8 399972)
            dstSym->m_builtInIndex = symSrc1->m_builtInIndex;
        }
        break;

    case Js::OpCode::ProfiledStrictLdThis:
        newOpcode = Js::OpCode::StrictLdThis;
        if (m_func->HasProfileInfo())
        {
            dstOpnd->SetValueType(m_func->GetProfileInfo()->GetThisInfo().valueType);
        }

        if (m_func->DoSimpleJitDynamicProfile())
        {
            IR::JitProfilingInstr* newInstr = IR::JitProfilingInstr::New(Js::OpCode::StrictLdThis, dstOpnd, src1Opnd, m_func);
            instr = newInstr;
        }
        break;
    case Js::OpCode::GetForInEnumerator:
        if (dstSym->m_isSingleDef)
        {
            dstSym->m_isNotInt = true;
        }
        break;
    case Js::OpCode::Delete_A:
        dstOpnd->SetValueType(ValueType::Boolean);
        break;
    case Js::OpCode::BeginSwitch:
        m_switchBuilder.BeginSwitch();
        newOpcode = Js::OpCode::Ld_A;
        break;
    case Js::OpCode::LdArrHead:
        src1Opnd->SetValueType(
            ValueType::GetObject(ObjectType::Array).SetHasNoMissingValues(false).SetArrayTypeId(Js::TypeIds_Array));
        src1Opnd->SetValueTypeFixed();
        break;

    case Js::OpCode::LdInnerFrameDisplayNoParent:
    {
        instr = IR::Instr::New(Js::OpCode::LdInnerFrameDisplay, dstOpnd, src1Opnd, m_func);
        this->AddEnvOpndForInnerFrameDisplay(instr, offset);
        if (dstSym->m_isSingleDef)
        {
            dstSym->m_isNotInt = true;
        }
        this->AddInstr(instr, offset);

        return;
    }

    case Js::OpCode::Conv_Str:
        dstOpnd->SetValueType(ValueType::String);
        break;

    case Js::OpCode::Yield:
        instr = IR::Instr::New(newOpcode, dstOpnd, src1Opnd, m_func);
        this->AddInstr(instr, offset);
        this->m_lastInstr = instr->ConvertToBailOutInstr(instr, IR::BailOutForGeneratorYield);

        IR::LabelInstr* label = IR::LabelInstr::New(Js::OpCode::Label, m_func);
        label->m_hasNonBranchRef = true;
        this->AddInstr(label, Js::Constants::NoByteCodeOffset);

        this->m_func->AddYieldOffsetResumeLabel(nextOffset, label);

        return;
    }

    if (instr == nullptr)
    {
        instr = IR::Instr::New(newOpcode, dstOpnd, src1Opnd, m_func);
    }

    this->AddInstr(instr, offset);
}

template <typename SizePolicy>
void
IRBuilder::BuildReg2WithICIndex(Js::OpCode newOpcode, uint32 offset)
{
    AssertMsg(false, "NYI");
}

template <typename SizePolicy>
void
IRBuilder::BuildProfiledReg2WithICIndex(Js::OpCode newOpcode, uint32 offset)
{
    Assert(OpCodeAttr::IsProfiledOpWithICIndex(newOpcode));
    Assert(OpCodeAttr::HasMultiSizeLayout(newOpcode));
    auto layout = m_jnReader.GetLayout<Js::OpLayoutDynamicProfile<Js::OpLayoutT_Reg2WithICIndex<SizePolicy>>>();

    if (!PHASE_OFF(Js::ClosureRegCheckPhase, m_func))
    {
        this->DoClosureRegCheck(layout->R0);
        this->DoClosureRegCheck(layout->R1);
    }

    BuildProfiledReg2WithICIndex(newOpcode, offset, layout->R0, layout->R1, layout->profileId, layout->inlineCacheIndex);
}

void
IRBuilder::BuildProfiledReg2WithICIndex(Js::OpCode newOpcode, uint32 offset, Js::RegSlot dstRegSlot, Js::RegSlot srcRegSlot, Js::ProfileId profileId, Js::InlineCacheIndex inlineCacheIndex)
{
    BuildProfiledReg2(newOpcode, offset, dstRegSlot, srcRegSlot, profileId, inlineCacheIndex);
}

///----------------------------------------------------------------------------
///
/// IRBuilder::BuildProfiledReg2
///
///     Build IR instr for a profiled Reg2 instruction.
///
///----------------------------------------------------------------------------
template <typename SizePolicy>
void
IRBuilder::BuildProfiledReg2(Js::OpCode newOpcode, uint32 offset)
{
    Assert(OpCodeAttr::IsProfiledOp(newOpcode));
    Assert(OpCodeAttr::HasMultiSizeLayout(newOpcode));
    auto layout = m_jnReader.GetLayout<Js::OpLayoutDynamicProfile<Js::OpLayoutT_Reg2<SizePolicy>>>();

    if (!PHASE_OFF(Js::ClosureRegCheckPhase, m_func))
    {
        this->DoClosureRegCheck(layout->R0);
        this->DoClosureRegCheck(layout->R1);
    }

    BuildProfiledReg2(newOpcode, offset, layout->R0, layout->R1, layout->profileId);
}

void
IRBuilder::BuildProfiledReg2(Js::OpCode newOpcode, uint32 offset, Js::RegSlot dstRegSlot, Js::RegSlot srcRegSlot, Js::ProfileId profileId, Js::InlineCacheIndex inlineCacheIndex)
{
    bool switchFound = false;

    Js::OpCodeUtil::ConvertNonCallOpToNonProfiled(newOpcode);

    IR::RegOpnd *   src1Opnd = this->BuildSrcOpnd(srcRegSlot);
    IR::RegOpnd *   dstOpnd;
    if(newOpcode == Js::OpCode::BeginSwitch && srcRegSlot == dstRegSlot)
    {
        //if the operands are the same for BeginSwitch, don't build a new operand in IR.
        dstOpnd = src1Opnd;
    }
    else
    {
        dstOpnd = this->BuildDstOpnd(dstRegSlot);
    }

    bool isProfiled = true;
    const Js::LdElemInfo *ldElemInfo = nullptr;
    if (newOpcode == Js::OpCode::BeginSwitch)
    {
        m_switchBuilder.BeginSwitch();
        switchFound = true;
        newOpcode = Js::OpCode::Ld_A;   // BeginSwitch is originally equivalent to Ld_A
    }
    else
    {
        Assert(newOpcode == Js::OpCode::LdLen_A);
        if(m_func->HasProfileInfo())
        {
            ldElemInfo = m_func->GetProfileInfo()->GetLdElemInfo(m_func->GetJnFunction(), profileId);
            ValueType arrayType(ldElemInfo->GetArrayType());
            if(arrayType.IsLikelyNativeArray() &&
                (
                    !(m_func->GetTopFunc()->HasTry() && !m_func->GetTopFunc()->DoOptimizeTryCatch()) && m_func->GetWeakFuncRef() && !m_func->HasArrayInfo() ||
                    m_func->IsJitInDebugMode()
                ))
            {
                arrayType = arrayType.SetArrayTypeId(Js::TypeIds_Array);

                // An opnd's value type will get replaced in the forward phase when it is not fixed. Store the array type in the
                // ProfiledInstr.
                Js::LdElemInfo *const newLdElemInfo = JitAnew(m_func->m_alloc, Js::LdElemInfo, *ldElemInfo);
                newLdElemInfo->arrayType = arrayType;
                ldElemInfo = newLdElemInfo;
            }
            src1Opnd->SetValueType(arrayType);

            if (m_func->GetTopFunc()->HasTry() && !m_func->GetTopFunc()->DoOptimizeTryCatch())
            {
                isProfiled = false;
            }
        }
        else
        {
            isProfiled = false;
        }
    }

    IR::Instr *instr;


    if (m_func->DoSimpleJitDynamicProfile())
    {
        // Since we're in simplejit, we want to keep track of the profileid:
        IR::JitProfilingInstr *profiledInstr = IR::JitProfilingInstr::New(newOpcode, dstOpnd, src1Opnd, m_func);
        profiledInstr->profileId = profileId;
        profiledInstr->isBeginSwitch = newOpcode == Js::OpCode::Ld_A;
        profiledInstr->inlineCacheIndex = inlineCacheIndex;
        instr = profiledInstr;
    }
    else if(isProfiled)
    {
        IR::ProfiledInstr *profiledInstr = IR::ProfiledInstr::New(newOpcode, dstOpnd, src1Opnd, m_func);
        instr = profiledInstr;

        switch (newOpcode) {
        case Js::OpCode::Ld_A:
            profiledInstr->u.FldInfo() = Js::FldInfo();
            break;
        case Js::OpCode::LdLen_A:
            profiledInstr->u.ldElemInfo = ldElemInfo;
            break;
        default:
            Assert(false);
            __assume(false);
        }
    }
    else
    {
        instr = IR::Instr::New(newOpcode, dstOpnd, src1Opnd, m_func);
    }

    this->AddInstr(instr, offset);

    if(newOpcode == Js::OpCode::LdLen_A && ldElemInfo && !ldElemInfo->WasProfiled() && DoBailOnNoProfile())
    {
        InsertBailOnNoProfile(instr);
    }

    if(switchFound && instr->IsProfiledInstr())
    {
        m_switchBuilder.SetProfiledInstruction(instr, profileId);
    }
}

///----------------------------------------------------------------------------
///
/// IRBuilder::BuildReg3
///
///     Build IR instr for a Reg3 instruction.
///
///----------------------------------------------------------------------------

template <typename SizePolicy>
void
IRBuilder::BuildReg3(Js::OpCode newOpcode, uint32 offset)
{
    Assert(!OpCodeAttr::IsProfiledOp(newOpcode));
    Assert(OpCodeAttr::HasMultiSizeLayout(newOpcode));
    auto layout = m_jnReader.GetLayout<Js::OpLayoutT_Reg3<SizePolicy>>();

    if (!PHASE_OFF(Js::ClosureRegCheckPhase, m_func) && newOpcode != Js::OpCode::NewInnerScopeSlots)
    {
        this->DoClosureRegCheck(layout->R0);
        this->DoClosureRegCheck(layout->R1);
        this->DoClosureRegCheck(layout->R2);
    }

    BuildReg3(newOpcode, offset, layout->R0, layout->R1, layout->R2, Js::Constants::NoProfileId);
}

template <typename SizePolicy>
void
IRBuilder::BuildProfiledReg3(Js::OpCode newOpcode, uint32 offset)
{
    Assert(OpCodeAttr::IsProfiledOp(newOpcode));
    Assert(OpCodeAttr::HasMultiSizeLayout(newOpcode));
    auto layout = m_jnReader.GetLayout<Js::OpLayoutDynamicProfile<Js::OpLayoutT_Reg3<SizePolicy>>>();

    if (!PHASE_OFF(Js::ClosureRegCheckPhase, m_func))
    {
        this->DoClosureRegCheck(layout->R0);
        this->DoClosureRegCheck(layout->R1);
        this->DoClosureRegCheck(layout->R2);
    }

    Js::OpCodeUtil::ConvertNonCallOpToNonProfiled(newOpcode);
    BuildReg3(newOpcode, offset, layout->R0, layout->R1, layout->R2, layout->profileId);
}

void
IRBuilder::BuildReg3(Js::OpCode newOpcode, uint32 offset, Js::RegSlot dstRegSlot, Js::RegSlot src1RegSlot,
                        Js::RegSlot src2RegSlot, Js::ProfileId profileId)
{
    IR::Instr *     instr;

    if (newOpcode == Js::OpCode::NewInnerScopeSlots)
    {
        if (dstRegSlot >= m_func->GetJnFunction()->GetInnerScopeCount())
        {
            Js::Throw::FatalInternalError();
        }
        newOpcode = Js::OpCode::NewScopeSlotsWithoutPropIds;
        dstRegSlot += m_func->GetJnFunction()->FirstInnerScopeReg();
        instr = IR::Instr::New(newOpcode, BuildDstOpnd(dstRegSlot),
                               IR::IntConstOpnd::New(src1RegSlot, TyVar, m_func),
                               IR::IntConstOpnd::New(src2RegSlot, TyVar, m_func),
                               m_func);
        if (instr->GetDst()->AsRegOpnd()->m_sym->m_isSingleDef)
        {
            instr->GetDst()->AsRegOpnd()->m_sym->m_isNotInt = true;
        }
        this->AddInstr(instr, offset);
        return;
    }

    IR::RegOpnd *   src1Opnd = this->BuildSrcOpnd(src1RegSlot);
    IR::RegOpnd *   src2Opnd = this->BuildSrcOpnd(src2RegSlot);
    IR::RegOpnd *   dstOpnd = this->BuildDstOpnd(dstRegSlot);
    StackSym *      dstSym = dstOpnd->m_sym;

    if (profileId != Js::Constants::NoProfileId)
    {
        if (m_func->DoSimpleJitDynamicProfile())
        {
            instr = IR::JitProfilingInstr::New(newOpcode, dstOpnd, src1Opnd, src2Opnd, m_func);
            instr->AsJitProfilingInstr()->profileId = profileId;
        }
        else
        {
            instr = IR::ProfiledInstr::New(newOpcode, dstOpnd, src1Opnd, src2Opnd, m_func);
            instr->AsProfiledInstr()->u.profileId = profileId;
        }
    }
    else
    {
        instr = IR::Instr::New(newOpcode, dstOpnd, src1Opnd, src2Opnd, m_func);
    }

    this->AddInstr(instr, offset);

    switch (newOpcode)
    {
    case Js::OpCode::LdHandlerScope:
    case Js::OpCode::NewScopeSlotsWithoutPropIds:
        if (dstSym->m_isSingleDef)
        {
            dstSym->m_isNotInt = true;
        }
        break;

    case Js::OpCode::LdInnerFrameDisplay:
        if (dstSym->m_isSingleDef)
        {
            dstSym->m_isNotInt = true;
        }
        break;
    }
}

///----------------------------------------------------------------------------
///
/// IRBuilder::BuildReg3C
///
///     Build IR instr for a Reg3C instruction.
///
///----------------------------------------------------------------------------


template <typename SizePolicy>
void
IRBuilder::BuildReg3C(Js::OpCode newOpcode, uint32 offset)
{
    Assert(!OpCodeAttr::IsProfiledOp(newOpcode));
    Assert(OpCodeAttr::HasMultiSizeLayout(newOpcode));
    auto layout = m_jnReader.GetLayout<Js::OpLayoutT_Reg3C<SizePolicy>>();

    if (!PHASE_OFF(Js::ClosureRegCheckPhase, m_func))
    {
        this->DoClosureRegCheck(layout->R0);
        this->DoClosureRegCheck(layout->R1);
        this->DoClosureRegCheck(layout->R2);
    }

    BuildReg3C(newOpcode, offset, layout->R0, layout->R1, layout->R2, layout->inlineCacheIndex);
}

void
IRBuilder::BuildReg3C(Js::OpCode newOpCode, uint32 offset, Js::RegSlot dstRegSlot, Js::RegSlot src1RegSlot,
                            Js::RegSlot src2RegSlot, Js::CacheId inlineCacheIndex)
{
    Assert(OpCodeAttr::HasMultiSizeLayout(newOpCode));

    IR::Instr *     instr;
    IR::RegOpnd *   src1Opnd = this->BuildSrcOpnd(src1RegSlot);
    IR::RegOpnd *   src2Opnd = this->BuildSrcOpnd(src2RegSlot);
    IR::RegOpnd *   dstOpnd = this->BuildDstOpnd(dstRegSlot);

    instr = IR::Instr::New(Js::OpCode::ArgOut_A, IR::RegOpnd::New(TyVar, m_func), src2Opnd, m_func);
    this->AddInstr(instr, offset);

    instr = IR::Instr::New(Js::OpCode::ArgOut_A, IR::RegOpnd::New(TyVar, m_func), src1Opnd, instr->GetDst(), m_func);
    this->AddInstr(instr, Js::Constants::NoByteCodeOffset);

    instr = IR::Instr::New(newOpCode, dstOpnd, IR::IntConstOpnd::New(inlineCacheIndex, TyUint32, m_func), instr->GetDst(), m_func);
    this->AddInstr(instr, Js::Constants::NoByteCodeOffset);
}

///----------------------------------------------------------------------------
///
/// IRBuilder::BuildReg4
///
///     Build IR instr for a Reg4 instruction.
///
///----------------------------------------------------------------------------

template <typename SizePolicy>
void
IRBuilder::BuildReg4(Js::OpCode newOpcode, uint32 offset)
{
    Assert(!OpCodeAttr::IsProfiledOp(newOpcode));
    Assert(OpCodeAttr::HasMultiSizeLayout(newOpcode));
    auto layout = m_jnReader.GetLayout<Js::OpLayoutT_Reg4<SizePolicy>>();

    if (!PHASE_OFF(Js::ClosureRegCheckPhase, m_func))
    {
        this->DoClosureRegCheck(layout->R0);
        this->DoClosureRegCheck(layout->R1);
        this->DoClosureRegCheck(layout->R2);
        this->DoClosureRegCheck(layout->R3);
    }

    BuildReg4(newOpcode, offset, layout->R0, layout->R1, layout->R2, layout->R3);
}

void
IRBuilder::BuildReg4(Js::OpCode newOpcode, uint32 offset, Js::RegSlot dstRegSlot, Js::RegSlot src1RegSlot,
                    Js::RegSlot src2RegSlot, Js::RegSlot src3RegSlot)
{
    IR::Instr *     instr;
    Assert(newOpcode == Js::OpCode::Concat3);

    IR::RegOpnd * src1Opnd = this->BuildSrcOpnd(src1RegSlot);
    IR::RegOpnd * src2Opnd = this->BuildSrcOpnd(src2RegSlot);
    IR::RegOpnd * src3Opnd = this->BuildSrcOpnd(src3RegSlot);
    IR::RegOpnd * dstOpnd = this->BuildDstOpnd(dstRegSlot);

    IR::RegOpnd * str1Opnd = InsertConvPrimStr(src1Opnd, offset, true);
    IR::RegOpnd * str2Opnd = InsertConvPrimStr(src2Opnd, Js::Constants::NoByteCodeOffset, true);
    IR::RegOpnd * str3Opnd = InsertConvPrimStr(src3Opnd, Js::Constants::NoByteCodeOffset, true);

    // Need to insert a byte code use for src1/src2 that if ConvPrimStr of the src2/src3 bail out
    // we will restore it.
    bool src1HasByteCodeRegSlot = src1Opnd->m_sym->HasByteCodeRegSlot();
    bool src2HasByteCodeRegSlot = src2Opnd->m_sym->HasByteCodeRegSlot();
    if (src1HasByteCodeRegSlot || src2HasByteCodeRegSlot)
    {
        IR::ByteCodeUsesInstr * byteCodeUse = IR::ByteCodeUsesInstr::New(m_func);
        byteCodeUse->byteCodeUpwardExposedUsed = JitAnew(m_func->m_alloc, BVSparse<JitArenaAllocator>, m_func->m_alloc);
        if (src1HasByteCodeRegSlot)
        {
            byteCodeUse->byteCodeUpwardExposedUsed->Set(src1Opnd->m_sym->m_id);
        }
        if (src2HasByteCodeRegSlot)
        {
            byteCodeUse->byteCodeUpwardExposedUsed->Set(src2Opnd->m_sym->m_id);
        }
        this->AddInstr(byteCodeUse, Js::Constants::NoByteCodeOffset);
    }

    if (!PHASE_OFF(Js::BackendConcatExprOptPhase, this->m_func))
    {
        IR::RegOpnd* tmpDstOpnd1 = IR::RegOpnd::New(StackSym::New(this->m_func), TyVar, this->m_func);
        IR::RegOpnd* tmpDstOpnd2 = IR::RegOpnd::New(StackSym::New(this->m_func), TyVar, this->m_func);
        IR::RegOpnd* tmpDstOpnd3 = IR::RegOpnd::New(StackSym::New(this->m_func), TyVar, this->m_func);

        instr = IR::Instr::New(Js::OpCode::SetConcatStrMultiItemBE, tmpDstOpnd1, str1Opnd, m_func);
        this->AddInstr(instr, Js::Constants::NoByteCodeOffset);
        instr = IR::Instr::New(Js::OpCode::SetConcatStrMultiItemBE, tmpDstOpnd2, str2Opnd, tmpDstOpnd1, m_func);
        this->AddInstr(instr, Js::Constants::NoByteCodeOffset);
        instr = IR::Instr::New(Js::OpCode::SetConcatStrMultiItemBE, tmpDstOpnd3, str3Opnd, tmpDstOpnd2, m_func);
        this->AddInstr(instr, Js::Constants::NoByteCodeOffset);

        IR::IntConstOpnd * countIntConstOpnd = IR::IntConstOpnd::New(3, TyUint32, m_func, true);
        instr = IR::Instr::New(Js::OpCode::NewConcatStrMultiBE, dstOpnd, countIntConstOpnd, tmpDstOpnd3, m_func);
        dstOpnd->SetValueType(ValueType::String);
        this->AddInstr(instr, Js::Constants::NoByteCodeOffset);
    }
    else
    {
        instr = IR::Instr::New(Js::OpCode::NewConcatStrMulti, dstOpnd, IR::IntConstOpnd::New(3, TyUint32, m_func, true), m_func);
        dstOpnd->SetValueType(ValueType::String);
        this->AddInstr(instr, Js::Constants::NoByteCodeOffset);

        instr = IR::Instr::New(Js::OpCode::SetConcatStrMultiItem, IR::IndirOpnd::New(dstOpnd, 0, TyVar, m_func), str1Opnd, m_func);
        this->AddInstr(instr, Js::Constants::NoByteCodeOffset);
        instr = IR::Instr::New(Js::OpCode::SetConcatStrMultiItem, IR::IndirOpnd::New(dstOpnd, 1, TyVar, m_func), str2Opnd, m_func);
        this->AddInstr(instr, Js::Constants::NoByteCodeOffset);
        instr = IR::Instr::New(Js::OpCode::SetConcatStrMultiItem, IR::IndirOpnd::New(dstOpnd, 2, TyVar, m_func), str3Opnd, m_func);
        this->AddInstr(instr, Js::Constants::NoByteCodeOffset);
    }
}

IR::RegOpnd *
IRBuilder::InsertConvPrimStr(IR::RegOpnd * srcOpnd, uint offset, bool forcePreOpBailOutIfNeeded)
{
    IR::RegOpnd * strOpnd = IR::RegOpnd::New(TyVar, this->m_func);
    IR::Instr * instr = IR::Instr::New(Js::OpCode::Conv_PrimStr, strOpnd, srcOpnd, m_func);
    instr->forcePreOpBailOutIfNeeded = forcePreOpBailOutIfNeeded;
    strOpnd->SetValueType(ValueType::String);
    strOpnd->SetValueTypeFixed();
    this->AddInstr(instr, offset);
    return strOpnd;
}

template <typename SizePolicy>
void
IRBuilder::BuildReg2B1(Js::OpCode newOpcode, uint32 offset)
{
    Assert(!OpCodeAttr::IsProfiledOp(newOpcode));
    Assert(OpCodeAttr::HasMultiSizeLayout(newOpcode));
    auto layout = m_jnReader.GetLayout<Js::OpLayoutT_Reg2B1<SizePolicy>>();

    if (!PHASE_OFF(Js::ClosureRegCheckPhase, m_func))
    {
        this->DoClosureRegCheck(layout->R0);
        this->DoClosureRegCheck(layout->R1);
    }

    BuildReg2B1(newOpcode, offset, layout->R0, layout->R1, layout->B2);
}

void
IRBuilder::BuildReg2B1(Js::OpCode newOpcode, uint32 offset, Js::RegSlot dstRegSlot, Js::RegSlot srcRegSlot, byte index)
{
    Assert(OpCodeAttr::HasMultiSizeLayout(newOpcode));
    Assert(newOpcode == Js::OpCode::SetConcatStrMultiItem);

    IR::Instr *     instr;
    IR::RegOpnd * srcOpnd = this->BuildSrcOpnd(srcRegSlot);
    IR::RegOpnd * dstOpnd = this->BuildDstOpnd(dstRegSlot);

    IR::IndirOpnd * indir1Opnd = IR::IndirOpnd::New(dstOpnd, index, TyVar, m_func);

    dstOpnd->SetValueType(ValueType::String);

    instr = IR::Instr::New(Js::OpCode::SetConcatStrMultiItem, indir1Opnd, InsertConvPrimStr(srcOpnd, offset, true), m_func);
    this->AddInstr(instr, Js::Constants::NoByteCodeOffset);
}

template <typename SizePolicy>
void
IRBuilder::BuildReg3B1(Js::OpCode newOpcode, uint32 offset)
{
    Assert(!OpCodeAttr::IsProfiledOp(newOpcode));
    Assert(OpCodeAttr::HasMultiSizeLayout(newOpcode));
    auto layout = m_jnReader.GetLayout<Js::OpLayoutT_Reg3B1<SizePolicy>>();

    if (!PHASE_OFF(Js::ClosureRegCheckPhase, m_func))
    {
        this->DoClosureRegCheck(layout->R0);
        this->DoClosureRegCheck(layout->R1);
        this->DoClosureRegCheck(layout->R2);
    }

    BuildReg3B1(newOpcode, offset, layout->R0, layout->R1, layout->R2, layout->B3);
}

void
IRBuilder::BuildReg3B1(Js::OpCode newOpcode, uint32 offset, Js::RegSlot dstRegSlot, Js::RegSlot src1RegSlot,
                    Js::RegSlot src2RegSlot, uint8 index)
{
    Assert(OpCodeAttr::HasMultiSizeLayout(newOpcode));

    IR::Instr *     instr;
    IR::RegOpnd * src1Opnd = this->BuildSrcOpnd(src1RegSlot);
    IR::RegOpnd * src2Opnd = this->BuildSrcOpnd(src2RegSlot);
    IR::RegOpnd * dstOpnd = this->BuildDstOpnd(dstRegSlot);
    dstOpnd->SetValueType(ValueType::String);

    IR::Instr * newConcatStrMulti = nullptr;
    switch (newOpcode)
    {
    case Js::OpCode::NewConcatStrMulti:

        newConcatStrMulti = IR::Instr::New(Js::OpCode::NewConcatStrMulti, dstOpnd, IR::IntConstOpnd::New(index, TyUint32, m_func), m_func);
        index = 0;
        break;
    case Js::OpCode::SetConcatStrMultiItem2:
        break;
    default:
        Assert(false);
    };
    IR::IndirOpnd * indir1Opnd = IR::IndirOpnd::New(dstOpnd, index, TyVar, m_func);
    IR::IndirOpnd * indir2Opnd = IR::IndirOpnd::New(dstOpnd, index + 1, TyVar, m_func);

    // Need to do the to str first, as they may have side effects.
    IR::RegOpnd * str1Opnd = InsertConvPrimStr(src1Opnd, offset, true);
    IR::RegOpnd * str2Opnd = InsertConvPrimStr(src2Opnd, Js::Constants::NoByteCodeOffset, true);

    // Need to insert a byte code use for src1 so that if ConvPrimStr of the src2 bail out
    // we will restore it.
    if (src1Opnd->m_sym->HasByteCodeRegSlot())
    {
        IR::ByteCodeUsesInstr * byteCodeUse = IR::ByteCodeUsesInstr::New(m_func);
        byteCodeUse->byteCodeUpwardExposedUsed = JitAnew(m_func->m_alloc, BVSparse<JitArenaAllocator>, m_func->m_alloc);
        byteCodeUse->byteCodeUpwardExposedUsed->Set(src1Opnd->m_sym->m_id);
        this->AddInstr(byteCodeUse, Js::Constants::NoByteCodeOffset);
    }

    if (newConcatStrMulti)
    {
        // Allocate the concat str after the ConvToStr
        this->AddInstr(newConcatStrMulti, Js::Constants::NoByteCodeOffset);
    }
    instr = IR::Instr::New(Js::OpCode::SetConcatStrMultiItem, indir1Opnd, str1Opnd, m_func);
    this->AddInstr(instr, Js::Constants::NoByteCodeOffset);

    instr = IR::Instr::New(Js::OpCode::SetConcatStrMultiItem, indir2Opnd, str2Opnd, m_func);
    this->AddInstr(instr, Js::Constants::NoByteCodeOffset);
}

///----------------------------------------------------------------------------
///
/// IRBuilder::BuildReg5
///
///     Build IR instr for a Reg5 instruction.
///
///----------------------------------------------------------------------------
template <typename SizePolicy>
void
IRBuilder::BuildReg5(Js::OpCode newOpcode, uint32 offset)
{
    Assert(!OpCodeAttr::IsProfiledOp(newOpcode));
    Assert(OpCodeAttr::HasMultiSizeLayout(newOpcode));
    auto layout = m_jnReader.GetLayout<Js::OpLayoutT_Reg5<SizePolicy>>();

    if (!PHASE_OFF(Js::ClosureRegCheckPhase, m_func))
    {
        this->DoClosureRegCheck(layout->R0);
        this->DoClosureRegCheck(layout->R1);
        this->DoClosureRegCheck(layout->R2);
        this->DoClosureRegCheck(layout->R3);
        this->DoClosureRegCheck(layout->R4);
    }

    BuildReg5(newOpcode, offset, layout->R0, layout->R1, layout->R2, layout->R3, layout->R4);
}

void
IRBuilder::BuildReg5(Js::OpCode newOpcode, uint32 offset, Js::RegSlot dstRegSlot, Js::RegSlot src1RegSlot,
                            Js::RegSlot src2RegSlot, Js::RegSlot src3RegSlot, Js::RegSlot src4RegSlot)
{
    Assert(OpCodeAttr::HasMultiSizeLayout(newOpcode));

    IR::Instr *     instr;
    IR::RegOpnd *   dstOpnd;
    IR::RegOpnd *   src1Opnd;
    IR::RegOpnd *   src2Opnd;
    IR::RegOpnd *   src3Opnd;
    IR::RegOpnd *   src4Opnd;
    // We can't support instructions with more than 2 srcs. Instead create a CallHelper instructions,
    // and pass the srcs as ArgOut_A instructions.
    src1Opnd = this->BuildSrcOpnd(src1RegSlot);
    src2Opnd = this->BuildSrcOpnd(src2RegSlot);
    src3Opnd = this->BuildSrcOpnd(src3RegSlot);
    src4Opnd = this->BuildSrcOpnd(src4RegSlot);
    dstOpnd = this->BuildDstOpnd(dstRegSlot);

    instr = IR::Instr::New(Js::OpCode::ArgOut_A, IR::RegOpnd::New(TyVar, m_func), src4Opnd, m_func);
    this->AddInstr(instr, offset);

    instr = IR::Instr::New(Js::OpCode::ArgOut_A, IR::RegOpnd::New(TyVar, m_func), src3Opnd, instr->GetDst(), m_func);
    this->AddInstr(instr, Js::Constants::NoByteCodeOffset);

    instr = IR::Instr::New(Js::OpCode::ArgOut_A, IR::RegOpnd::New(TyVar, m_func), src2Opnd, instr->GetDst(), m_func);
    this->AddInstr(instr, Js::Constants::NoByteCodeOffset);

    instr = IR::Instr::New(Js::OpCode::ArgOut_A, IR::RegOpnd::New(TyVar, m_func), src1Opnd, instr->GetDst(), m_func);
    this->AddInstr(instr, Js::Constants::NoByteCodeOffset);

    IR::HelperCallOpnd *helperOpnd;

    switch (newOpcode) {
    case Js::OpCode::ApplyArgs:
        helperOpnd=IR::HelperCallOpnd::New(IR::HelperOp_OP_ApplyArgs, this->m_func);
        break;
    default:
        AssertMsg(UNREACHED, "Unknown Reg5 opcode");
        Fatal();
    }
    instr = IR::Instr::New(Js::OpCode::CallHelper, dstOpnd, helperOpnd, instr->GetDst(), m_func);
    this->AddInstr(instr, Js::Constants::NoByteCodeOffset);
}

void
IRBuilder::BuildW1(Js::OpCode newOpcode, uint32 offset)
{
    Assert(!OpCodeAttr::HasMultiSizeLayout(newOpcode));

    unsigned short           C1;

    const unaligned Js::OpLayoutW1 *regLayout = m_jnReader.W1();
    C1 = regLayout->C1;

    IR::Instr *     instr;
    IntConstType    value = C1;
    IR::IntConstOpnd * srcOpnd;

    srcOpnd = IR::IntConstOpnd::New(value, TyInt32, m_func);
    instr = IR::Instr::New(newOpcode, m_func);
    instr->SetSrc1(srcOpnd);

    this->AddInstr(instr, offset);

    if (newOpcode == Js::OpCode::RuntimeReferenceError || newOpcode == Js::OpCode::RuntimeTypeError)
    {
        if (DoBailOnNoProfile())
        {
            // RuntimeReferenceError are extremely rare as they are guaranteed to throw. Insert BailonNoProfile to optimize this code path.
            // If there are continues bailout bailonnoprofile will be disabled.
            InsertBailOnNoProfile(instr);
        }
    }
}

template <typename SizePolicy>
void
IRBuilder::BuildUnsigned1(Js::OpCode newOpcode, uint32 offset)
{
    Assert(!OpCodeAttr::IsProfiledOp(newOpcode));
    Assert(OpCodeAttr::HasMultiSizeLayout(newOpcode));
    auto layout = m_jnReader.GetLayout<Js::OpLayoutT_Unsigned1<SizePolicy>>();
    BuildUnsigned1(newOpcode, offset, layout->C1);
}

void
IRBuilder::BuildUnsigned1(Js::OpCode newOpcode, uint32 offset, uint32 num)
{
    switch (newOpcode)
    {
        case Js::OpCode::EmitTmpRegCount:
            // Note: EmitTmpRegCount is inserted when debugging, not needed for jit.
            //       It's only needed by the debugger to see how many tmp regs are active.
            Assert(m_func->IsJitInDebugMode());
            return;

        case Js::OpCode::NewBlockScope:
        case Js::OpCode::NewPseudoScope:
        {
            if (num >= m_func->GetJnFunction()->GetInnerScopeCount())
            {
                Js::Throw::FatalInternalError();
            }
            Js::RegSlot dstRegSlot = num + m_func->GetJnFunction()->FirstInnerScopeReg();
            IR::RegOpnd * dstOpnd = BuildDstOpnd(dstRegSlot);
            IR::Instr * instr = IR::Instr::New(newOpcode, dstOpnd, m_func);
            this->AddInstr(instr, offset);
            if (dstOpnd->m_sym->m_isSingleDef)
            {
                dstOpnd->m_sym->m_isNotInt = true;
            }
            break;
        }

        case Js::OpCode::CloneInnerScopeSlots:
        case Js::OpCode::CloneBlockScope:
        {
            if (num >= m_func->GetJnFunction()->GetInnerScopeCount())
            {
                Js::Throw::FatalInternalError();
            }
            Js::RegSlot srcRegSlot = num + m_func->GetJnFunction()->FirstInnerScopeReg();
            IR::RegOpnd * srcOpnd = BuildSrcOpnd(srcRegSlot);
            IR::Instr * instr = IR::Instr::New(newOpcode, m_func);
            instr->SetSrc1(srcOpnd);
            this->AddInstr(instr, offset);
            break;
        }

        case Js::OpCode::ProfiledLoopBodyStart:
        {
            if (!(m_func->DoSimpleJitDynamicProfile() && m_func->GetJnFunction()->DoJITLoopBody()))
            {
                // This opcode is removed from the IR when we aren't doing Profiling SimpleJit or not jitting loop bodies
                break;
            }

            // Attach a register to the dest of this instruction to communicate whether we should bail out (the deciding of this is done in lowering)
            IR::Opnd* fullJitExists = IR::RegOpnd::New(TyUint8, m_func);
            auto start = m_lastInstr->m_prev;

            if (start->m_opcode == Js::OpCode::InitLoopBodyCount)
            {
                Assert(this->IsLoopBody());
                start = start->m_prev;
            }

            Assert(start->m_opcode == Js::OpCode::ProfiledLoopStart && start->GetDst());
            IR::JitProfilingInstr* instr = IR::JitProfilingInstr::New(Js::OpCode::ProfiledLoopBodyStart, fullJitExists, start->GetDst(), m_func);
            // profileId is used here to represent the loop number
            instr->loopNumber = num;
            this->AddInstr(instr, offset);

            // If fullJitExists isn't 0, bail out so that we can get the fulljitted version
            BailOutInfo * bailOutInfo = JitAnew(m_func->m_alloc, BailOutInfo, instr->GetByteCodeOffset(), m_func);
            IR::BailOutInstr * bailInstr = IR::BailOutInstr::New(Js::OpCode::BailOnNotEqual, IR::BailOnSimpleJitToFullJitLoopBody, bailOutInfo, bailOutInfo->bailOutFunc);
            bailInstr->SetSrc1(fullJitExists);
            bailInstr->SetSrc2(IR::IntConstOpnd::New(0, TyUint8, m_func, true));
            this->AddInstr(bailInstr, offset);

            break;
        }

        case Js::OpCode::LoopBodyStart:
            break;

        case Js::OpCode::ProfiledLoopStart:
        {
            // If we're in profiling SimpleJit and jitting loop bodies, we need to keep this until lowering.
            if (m_func->DoSimpleJitDynamicProfile() && m_func->GetJnFunction()->DoJITLoopBody())
            {
                // In order for the JIT engine to correctly allocate registers we need to have this set up before lowering.

                // There may be 0 to many LoopEnds, but there will only ever be one LoopStart
                Assert(!this->m_saveLoopImplicitCallFlags[num]);

                const auto ty = Lowerer::GetImplicitCallFlagsType();
                auto saveOpnd = IR::RegOpnd::New(ty, m_func);
                this->m_saveLoopImplicitCallFlags[num] = saveOpnd;
                // Note that we insert this instruction /before/ the actual ProfiledLoopStart opcode. This is because Lowering is backwards
                //    and this is just a fake instruction which is only used to pass on the saveOpnd; this instruction will eventually be removed.
                auto instr = IR::JitProfilingInstr::New(Js::OpCode::Ld_A, saveOpnd, IR::MemRefOpnd::New(0, ty, m_func), m_func);
                instr->isLoopHelper = true;
                this->AddInstr(instr, offset);

                instr = IR::JitProfilingInstr::New(Js::OpCode::ProfiledLoopStart, IR::RegOpnd::New(TyMachPtr, m_func), nullptr, m_func);
                instr->loopNumber = num;
                this->AddInstr(instr, offset);
            }

            Js::ImplicitCallFlags flags = Js::ImplicitCall_HasNoInfo;
            Js::LoopFlags loopFlags;
            if (this->m_func->HasProfileInfo())
            {
                Js::ReadOnlyDynamicProfileInfo * dynamicProfileInfo = this->m_func->GetProfileInfo();
                flags = dynamicProfileInfo->GetLoopImplicitCallFlags(this->m_func->GetJnFunction(), num);
                loopFlags = dynamicProfileInfo->GetLoopFlags(num);
            }

            if (this->IsLoopBody() && !m_loopCounterSym)
            {
                InsertInitLoopBodyLoopCounter(num);
            }

            // Put a label the instruction stream to carry the profile info
            IR::ProfiledLabelInstr * labelInstr = IR::ProfiledLabelInstr::New(Js::OpCode::Label, this->m_func, flags, loopFlags);
#if DBG
            labelInstr->loopNum = num;
#endif
            m_lastInstr->InsertAfter(labelInstr);
            m_lastInstr = labelInstr;

            // Set it to the offset to the start of the loop
            labelInstr->SetByteCodeOffset(m_jnReader.GetCurrentOffset());
            break;
        }

        case Js::OpCode::ProfiledLoopEnd:
        {
            // TODO: Decide whether we want the implicit loop call flags to be recorded in simplejitted loop bodies
            if (m_func->DoSimpleJitDynamicProfile() && m_func->GetJnFunction()->DoJITLoopBody())
            {
                Assert(this->m_saveLoopImplicitCallFlags[num]);

                // In profiling simplejit we need this opcode in order to restore the implicit call flags
                auto instr = IR::JitProfilingInstr::New(Js::OpCode::ProfiledLoopEnd, nullptr, this->m_saveLoopImplicitCallFlags[num], m_func);
                this->AddInstr(instr, offset);
                instr->loopNumber = num;
            }

            if (!this->IsLoopBody())
            {
                break;
            }

            // In the early exit case (return), we generated ProfiledLoopEnd for all the outer loop.
            // If we see one of these profile loop, just load the IP of the immediate outer loop of the loop body being JIT'ed
            // and then skip all the other loops using the fact that we have already loaded the return IP

            if (IsLoopBodyReturnIPInstr(m_lastInstr))
            {
                // Already loaded the loop IP sym, skip
                break;
            }

            // See we are ending an outer loop and load the return IP to the ProfiledLoopEnd opcode
            // instead of following the normal branch

            Js::LoopHeader * loopHeader = this->m_func->GetJnFunction()->GetLoopHeader(num);

            JsLoopBodyCodeGen* loopBodyCodeGen = (JsLoopBodyCodeGen*)m_func->m_workItem;
            if (loopHeader != loopBodyCodeGen->loopHeader && loopHeader->Contains(loopBodyCodeGen->loopHeader))
            {
                this->InsertLoopBodyReturnIPInstr(offset, offset);
            }
            else
            {
                Assert(loopBodyCodeGen->loopHeader->Contains(loopHeader));
            }
            break;
        }

        case Js::OpCode::InvalCachedScope:
        {
            // The reg and constant are both src operands.
            IR::Instr* instr = IR::Instr::New(Js::OpCode::InvalCachedScope, m_func);
            IR::RegOpnd *envOpnd = this->BuildSrcOpnd(m_func->GetJnFunction()->GetEnvReg());
            instr->SetSrc1(envOpnd);
            IR::IntConstOpnd *envIndex = IR::IntConstOpnd::New(num, TyInt32, m_func, true);
            instr->SetSrc2(envIndex);
            this->AddInstr(instr, offset);
            return;
        }

        default:
            Assert(false);
            __assume(false);
    }
}

void
IRBuilder::BuildReg1Int2(Js::OpCode newOpcode, uint32 offset)
{
    Assert(!OpCodeAttr::HasMultiSizeLayout(newOpcode));

    const unaligned Js::OpLayoutReg1Int2 *regLayout = m_jnReader.Reg1Int2();
    Js::RegSlot     R0 = regLayout->R0;
    int32           C1 = regLayout->C1;
    int32           C2 = regLayout->C2;

    if (!PHASE_OFF(Js::ClosureRegCheckPhase, m_func))
    {
        this->DoClosureRegCheck(R0);
    }

    IR::RegOpnd* dstOpnd = this->BuildDstOpnd(R0);

    IR::IntConstOpnd * srcOpnd = IR::IntConstOpnd::New(C1, TyInt32, m_func);
    IR::IntConstOpnd * src2Opnd = IR::IntConstOpnd::New(C2, TyInt32, m_func);
    IR::Instr* instr = IR::Instr::New(newOpcode, dstOpnd, srcOpnd, src2Opnd, m_func);

    this->AddInstr(instr, offset);
}

template <typename SizePolicy>
void
IRBuilder::BuildProfiledReg1Unsigned1(Js::OpCode newOpcode, uint32 offset)
{
    Assert(OpCodeAttr::IsProfiledOp(newOpcode));
    Assert(OpCodeAttr::HasMultiSizeLayout(newOpcode));
    auto layout = m_jnReader.GetLayout<Js::OpLayoutDynamicProfile<Js::OpLayoutT_Reg1Unsigned1<SizePolicy>>>();

    if (!PHASE_OFF(Js::ClosureRegCheckPhase, m_func))
    {
        this->DoClosureRegCheck(layout->R0);
    }

    BuildProfiledReg1Unsigned1(newOpcode, offset, layout->R0, layout->C1, layout->profileId);
}

void
IRBuilder::BuildProfiledReg1Unsigned1(Js::OpCode newOpcode, uint32 offset, Js::RegSlot R0, int32 C1, Js::ProfileId profileId)
{
    Assert(newOpcode == Js::OpCode::ProfiledNewScArray);
    Js::OpCodeUtil::ConvertNonCallOpToNonProfiled(newOpcode);

    IR::Instr *instr;
    Js::RegSlot     dstRegSlot = R0;
    IR::RegOpnd *   dstOpnd = this->BuildDstOpnd(dstRegSlot);

    StackSym *      dstSym = dstOpnd->m_sym;
    int32           value = C1;
    IR::IntConstOpnd * srcOpnd;

    srcOpnd = IR::IntConstOpnd::New(value, TyInt32, m_func);
    if (m_func->DoSimpleJitDynamicProfile())
    {
        instr = IR::JitProfilingInstr::New(newOpcode, dstOpnd, srcOpnd, m_func);
        instr->AsJitProfilingInstr()->profileId = profileId;
    }
    else
    {
        instr = IR::ProfiledInstr::New(newOpcode, dstOpnd, srcOpnd, m_func);
        instr->AsProfiledInstr()->u.profileId = profileId;
    }

    this->AddInstr(instr, offset);

    if (dstSym->m_isSingleDef)
    {
        dstSym->m_isSafeThis = true;
        dstSym->m_isNotInt = true;
    }

    // Undefined values in array literals ([0, undefined, 1]) are treated as missing values in some versions
    Js::ArrayCallSiteInfo *arrayInfo = nullptr;
    if (m_func->HasArrayInfo())
    {
        arrayInfo = m_func->GetProfileInfo()->GetArrayCallSiteInfo(m_func->GetJnFunction(), profileId);
    }
    Js::TypeId arrayTypeId = Js::TypeIds_Array;
    if (arrayInfo && !m_func->IsJitInDebugMode() && Js::JavascriptArray::HasInlineHeadSegment(value))
    {
        if (arrayInfo->IsNativeIntArray())
        {
            arrayTypeId = Js::TypeIds_NativeIntArray;
        }
        else if (arrayInfo->IsNativeFloatArray())
        {
            arrayTypeId = Js::TypeIds_NativeFloatArray;
        }
    }
    dstOpnd->SetValueType(ValueType::GetObject(ObjectType::Array).SetHasNoMissingValues(true).SetArrayTypeId(arrayTypeId));
    if (dstOpnd->GetValueType().HasVarElements())
    {
        dstOpnd->SetValueTypeFixed();
    }
    else
    {
        dstOpnd->SetValueType(dstOpnd->GetValueType().ToLikely());
    }
}

template <typename SizePolicy>
void
IRBuilder::BuildReg1Unsigned1(Js::OpCode newOpcode, uint32 offset)
{
    Assert(!OpCodeAttr::IsProfiledOp(newOpcode));
    Assert(OpCodeAttr::HasMultiSizeLayout(newOpcode));
    auto layout = m_jnReader.GetLayout<Js::OpLayoutT_Reg1Unsigned1<SizePolicy>>();

    if (!PHASE_OFF(Js::ClosureRegCheckPhase, m_func))
    {
        this->DoClosureRegCheck(layout->R0);
    }

    BuildReg1Unsigned1(newOpcode, offset, layout->R0, layout->C1);
}

void
IRBuilder::BuildReg1Unsigned1(Js::OpCode newOpcode, uint offset, Js::RegSlot R0, int32 C1)
{
    switch (newOpcode)
    {
        case Js::OpCode::NewRegEx:
            this->BuildRegexFromPattern(R0, C1, offset);
            return;

        case Js::OpCode::LdInnerScope:
        {
            IR::RegOpnd * srcOpnd = BuildSrcOpnd(this->InnerScopeIndexToRegSlot(C1));
            IR::RegOpnd * dstOpnd = BuildDstOpnd(R0);
            IR::Instr * instr = IR::Instr::New(Js::OpCode::Ld_A, dstOpnd, srcOpnd, m_func);
            if (dstOpnd->m_sym->m_isSingleDef)
            {
                dstOpnd->m_sym->m_isNotInt = true;
            }
            this->AddInstr(instr, offset);
            return;
        }

        case Js::OpCode::LdIndexedFrameDisplayNoParent:
        {
            newOpcode = Js::OpCode::LdFrameDisplay;
            IR::RegOpnd *srcOpnd = this->BuildSrcOpnd(this->InnerScopeIndexToRegSlot(C1));
            IR::RegOpnd *dstOpnd = this->BuildDstOpnd(R0);
            IR::Instr *instr = IR::Instr::New(newOpcode, dstOpnd, srcOpnd, m_func);
            this->AddEnvOpndForInnerFrameDisplay(instr, offset);
            if (dstOpnd->m_sym->m_isSingleDef)
            {
                dstOpnd->m_sym->m_isNotInt = true;
            }
            this->AddInstr(instr, offset);
            return;
        }

        case Js::OpCode::GetCachedFunc:
        {
            IR::RegOpnd *src1Opnd = this->BuildSrcOpnd(m_func->GetJnFunction()->GetLocalClosureReg());
            IR::Opnd *src2Opnd = IR::IntConstOpnd::New(C1, TyUint32, m_func);
            IR::RegOpnd *dstOpnd = this->BuildDstOpnd(R0);
            IR::Instr *instr = IR::Instr::New(newOpcode, dstOpnd, src1Opnd, src2Opnd, m_func);
            if (dstOpnd->m_sym->m_isSingleDef)
            {
                dstOpnd->m_sym->m_isNotInt = true;
            }
            this->AddInstr(instr, offset);
            return;
        }
    }


    IR::RegOpnd *   dstOpnd = this->BuildDstOpnd(R0);

    StackSym *      dstSym = dstOpnd->m_sym;
    IntConstType    value = C1;
    IR::IntConstOpnd * srcOpnd = IR::IntConstOpnd::New(value, TyInt32, m_func);
    IR::Instr *     instr = IR::Instr::New(newOpcode, dstOpnd, srcOpnd, m_func);

    this->AddInstr(instr, offset);

    if (newOpcode == Js::OpCode::NewScopeSlots)
    {
        this->AddInstr(
            IR::Instr::New(
                Js::OpCode::Ld_A, IR::RegOpnd::New(m_func->GetLocalClosureSym(), TyVar, m_func), dstOpnd, m_func),
            (uint32)-1);
    }

    if (dstSym->m_isSingleDef)
    {
        switch (newOpcode)
        {
        case Js::OpCode::NewScArray:
        case Js::OpCode::NewScArrayWithMissingValues:
            dstSym->m_isSafeThis = true;
            dstSym->m_isNotInt = true;
            break;
        }
    }

    if (newOpcode == Js::OpCode::NewScArray || newOpcode == Js::OpCode::NewScArrayWithMissingValues)
    {
        // Undefined values in array literals ([0, undefined, 1]) are treated as missing values in some versions
        dstOpnd->SetValueType(
            ValueType::GetObject(ObjectType::Array)
                .SetHasNoMissingValues(newOpcode == Js::OpCode::NewScArray)
                .SetArrayTypeId(Js::TypeIds_Array));
        dstOpnd->SetValueTypeFixed();
    }
}
///----------------------------------------------------------------------------
///
/// IRBuilder::BuildReg2Int1
///
///     Build IR instr for a Reg2I4 instruction.
///
///----------------------------------------------------------------------------

template <typename SizePolicy>
void
IRBuilder::BuildReg2Int1(Js::OpCode newOpcode, uint32 offset)
{
    Assert(OpCodeAttr::HasMultiSizeLayout(newOpcode));
    auto layout = m_jnReader.GetLayout<Js::OpLayoutT_Reg2Int1<SizePolicy>>();

    if (!PHASE_OFF(Js::ClosureRegCheckPhase, m_func))
    {
        this->DoClosureRegCheck(layout->R0);
        this->DoClosureRegCheck(layout->R1);
    }

    BuildReg2Int1(newOpcode, offset, layout->R0, layout->R1, layout->C1);
}

void
IRBuilder::BuildReg2Int1(Js::OpCode newOpcode, uint32 offset, Js::RegSlot dstRegSlot, Js::RegSlot srcRegSlot, int32 value)
{
    Assert(OpCodeAttr::HasMultiSizeLayout(newOpcode));

    IR::Instr *     instr;

    if (newOpcode == Js::OpCode::LdIndexedFrameDisplay)
    {
        newOpcode = Js::OpCode::LdFrameDisplay;
        if ((uint)value >= m_func->GetJnFunction()->GetInnerScopeCount())
        {
            Js::Throw::FatalInternalError();
        }
        IR::RegOpnd *src1Opnd = this->BuildSrcOpnd(value + m_func->GetJnFunction()->FirstInnerScopeReg());
        IR::RegOpnd *src2Opnd = this->BuildSrcOpnd(srcRegSlot);
        IR::RegOpnd *dstOpnd = this->BuildDstOpnd(dstRegSlot);
        instr = IR::Instr::New(newOpcode, dstOpnd, src1Opnd, src2Opnd, m_func);
        if (dstOpnd->m_sym->m_isSingleDef)
        {
            dstOpnd->m_sym->m_isNotInt = true;
        }
        this->AddInstr(instr, offset);
        return;
    }

    IR::RegOpnd *   src1Opnd = this->BuildSrcOpnd(srcRegSlot);
    IR::IntConstOpnd * src2Opnd = IR::IntConstOpnd::New(value, TyInt32, m_func);
    IR::RegOpnd *   dstOpnd = this->BuildDstOpnd(dstRegSlot);

    switch (newOpcode)
    {
    case Js::OpCode::ProfiledLdThis:
        newOpcode = Js::OpCode::LdThis;
        if(m_func->HasProfileInfo())
        {
            dstOpnd->SetValueType(m_func->GetProfileInfo()->GetThisInfo().valueType);
        }

        if(m_func->DoSimpleJitDynamicProfile())
        {
            instr = IR::JitProfilingInstr::New(newOpcode, dstOpnd, src1Opnd, src2Opnd, m_func);

            // Break out since we just made the instr
            break;
        }
        // fall-through


    default:
        Assert(!OpCodeAttr::IsProfiledOp(newOpcode));
        instr = IR::Instr::New(newOpcode, dstOpnd, src1Opnd, src2Opnd, m_func);
        break;
    }

    this->AddInstr(instr, offset);
}

///----------------------------------------------------------------------------
///
/// IRBuilder::BuildElementC
///
///     Build IR instr for an ElementC instruction.
///
///----------------------------------------------------------------------------


template <typename SizePolicy>
void
IRBuilder::BuildElementScopedC(Js::OpCode newOpcode, uint32 offset)
{
    Assert(!OpCodeAttr::IsProfiledOp(newOpcode));
    Assert(OpCodeAttr::HasMultiSizeLayout(newOpcode));
    auto layout = m_jnReader.GetLayout<Js::OpLayoutT_ElementScopedC<SizePolicy>>();

    if (!PHASE_OFF(Js::ClosureRegCheckPhase, m_func))
    {
        this->DoClosureRegCheck(layout->Value);
    }

    BuildElementScopedC(newOpcode, offset, layout->Value, layout->PropertyIdIndex);
}

void
IRBuilder::BuildElementScopedC(Js::OpCode newOpcode, uint32 offset, Js::RegSlot regSlot, Js::PropertyIdIndexType propertyIdIndex)
{
    IR::Instr *     instr;
    Js::FunctionBody * functionBody = this->m_func->GetJnFunction();
    Js::PropertyId  propertyId = functionBody->GetReferencedPropertyId(propertyIdIndex);
    PropertyKind    propertyKind = PropertyKindData;
    IR::RegOpnd * regOpnd;
    Js::RegSlot     fieldRegSlot = this->GetEnvRegForEvalCode();
    IR::SymOpnd *   fieldSymOpnd = this->BuildFieldOpnd(newOpcode, fieldRegSlot, propertyId, propertyIdIndex, propertyKind);

    switch (newOpcode)
    {
    case Js::OpCode::ScopedEnsureNoRedeclFld:
    {
        regOpnd = this->BuildSrcOpnd(regSlot);
        instr = IR::Instr::New(newOpcode, fieldSymOpnd, regOpnd, m_func);
        break;
    }

    case Js::OpCode::ScopedDeleteFld:
    case Js::OpCode::ScopedDeleteFldStrict:
    {
        // Implicit root object as default instance
        IR::Opnd * instance2Opnd = this->BuildSrcOpnd(Js::FunctionBody::RootObjectRegSlot);
        regOpnd = this->BuildDstOpnd(regSlot);
        instr = IR::Instr::New(newOpcode, regOpnd, fieldSymOpnd, instance2Opnd, m_func);
        break;
    }

    case Js::OpCode::ScopedInitFunc:
    {
        // Implicit root object as default instance
        IR::Opnd * instance2Opnd = this->BuildSrcOpnd(Js::FunctionBody::RootObjectRegSlot);
        regOpnd = this->BuildSrcOpnd(regSlot);
        instr = IR::Instr::New(newOpcode, fieldSymOpnd, regOpnd, instance2Opnd, m_func);
        break;
    }

    default:
        AssertMsg(UNREACHED, "Unknown ElementScopedC opcode");
        Fatal();
    }

    this->AddInstr(instr, offset);
}

template <typename SizePolicy>
void
IRBuilder::BuildElementC(Js::OpCode newOpcode, uint32 offset)
{
    Assert(!OpCodeAttr::IsProfiledOp(newOpcode));
    Assert(OpCodeAttr::HasMultiSizeLayout(newOpcode));
    auto layout = m_jnReader.GetLayout<Js::OpLayoutT_ElementC<SizePolicy>>();

    if (!PHASE_OFF(Js::ClosureRegCheckPhase, m_func))
    {
        this->DoClosureRegCheck(layout->Value);
        this->DoClosureRegCheck(layout->Instance);
    }

    BuildElementC(newOpcode, offset, layout->Instance, layout->Value, layout->PropertyIdIndex);
}

void
IRBuilder::BuildElementC(Js::OpCode newOpcode, uint32 offset, Js::RegSlot fieldRegSlot, Js::RegSlot regSlot, Js::PropertyIdIndexType propertyIdIndex)
{
    IR::Instr *     instr;
    Js::FunctionBody * functionBody = this->m_func->GetJnFunction();
    Js::PropertyId  propertyId = functionBody->GetReferencedPropertyId(propertyIdIndex);
    PropertyKind    propertyKind = PropertyKindData;
    IR::SymOpnd *   fieldSymOpnd = this->BuildFieldOpnd(newOpcode, fieldRegSlot, propertyId, propertyIdIndex, propertyKind);
    IR::RegOpnd * regOpnd;

    switch (newOpcode)
    {
    case Js::OpCode::DeleteFld:
    case Js::OpCode::DeleteRootFld:
    case Js::OpCode::DeleteFldStrict:
    case Js::OpCode::DeleteRootFldStrict:
        // Load
        regOpnd = this->BuildDstOpnd(regSlot);
        instr = IR::Instr::New(newOpcode, regOpnd, fieldSymOpnd, m_func);
        break;

    case Js::OpCode::InitSetFld:
    case Js::OpCode::InitGetFld:
    case Js::OpCode::InitClassMemberGet:
    case Js::OpCode::InitClassMemberSet:
    case Js::OpCode::InitProto:
    case Js::OpCode::StFuncExpr:
        // Store
        regOpnd = this->BuildSrcOpnd(regSlot);
        instr = IR::Instr::New(newOpcode, fieldSymOpnd, regOpnd, m_func);
        break;

    default:
        AssertMsg(UNREACHED, "Unknown ElementC opcode");
        Fatal();
    }

    this->AddInstr(instr, offset);
}

///----------------------------------------------------------------------------
///
/// IRBuilder::BuildElementSlot
///
///     Build IR instr for an ElementSlot instruction.
///
///----------------------------------------------------------------------------

IR::Instr *
IRBuilder::BuildProfiledSlotLoad(Js::OpCode loadOp, IR::RegOpnd *dstOpnd, IR::SymOpnd *srcOpnd, Js::ProfileId profileId, bool *pUnprofiled)
{
    IR::Instr * instr = nullptr;

    if (m_func->DoSimpleJitDynamicProfile())
    {
        instr = IR::JitProfilingInstr::New(loadOp, dstOpnd, srcOpnd, m_func);
        instr->AsJitProfilingInstr()->profileId = profileId;
    }
    else if(this->m_func->GetJnFunction()->HasDynamicProfileInfo())
    {
        instr = IR::ProfiledInstr::New(loadOp, dstOpnd, srcOpnd, m_func);
        instr->AsProfiledInstr()->u.FldInfo().valueType =
            this->m_func->GetJnFunction()->GetAnyDynamicProfileInfo()->GetSlotLoad(this->m_func->GetJnFunction(), profileId);
        *pUnprofiled = instr->AsProfiledInstr()->u.FldInfo().valueType.IsUninitialized();
#if ENABLE_DEBUG_CONFIG_OPTIONS
        if(Js::Configuration::Global.flags.TestTrace.IsEnabled(Js::DynamicProfilePhase))
        {
            Js::FunctionBody * func = this->m_func->GetJnFunction();
            const ValueType valueType(instr->AsProfiledInstr()->u.FldInfo().valueType);
            char valueTypeStr[VALUE_TYPE_MAX_STRING_SIZE];
            valueType.ToString(valueTypeStr);
            wchar_t debugStringBuffer[MAX_FUNCTION_BODY_DEBUG_STRING_SIZE];
            Output::Print(L"TestTrace function %s (#%s) ValueType = %S ", func->GetDisplayName(), func->GetDebugNumberSet(debugStringBuffer), valueTypeStr);
            instr->DumpTestTrace();
        }
#endif
    }

    return instr;
}

template <typename SizePolicy>
void
IRBuilder::BuildElementSlot(Js::OpCode newOpcode, uint32 offset)
{
    Assert(!OpCodeAttr::IsProfiledOp(newOpcode));
    Assert(OpCodeAttr::HasMultiSizeLayout(newOpcode));
    auto layout = m_jnReader.GetLayout<Js::OpLayoutT_ElementSlot<SizePolicy>>();

    if (!PHASE_OFF(Js::ClosureRegCheckPhase, m_func))
    {
        this->DoClosureRegCheck(layout->Value);
        this->DoClosureRegCheck(layout->Instance);
    }

    BuildElementSlot(newOpcode, offset, layout->Instance, layout->Value, layout->SlotIndex, Js::Constants::NoProfileId);
}

template <typename SizePolicy>
void
IRBuilder::BuildProfiledElementSlot(Js::OpCode newOpcode, uint32 offset)
{
    Assert(OpCodeAttr::IsProfiledOp(newOpcode));
    Assert(OpCodeAttr::HasMultiSizeLayout(newOpcode));
    auto layout = m_jnReader.GetLayout<Js::OpLayoutDynamicProfile<Js::OpLayoutT_ElementSlot<SizePolicy>>>();

    if (!PHASE_OFF(Js::ClosureRegCheckPhase, m_func))
    {
        this->DoClosureRegCheck(layout->Value);
        this->DoClosureRegCheck(layout->Instance);
    }

    Js::OpCodeUtil::ConvertNonCallOpToNonProfiled(newOpcode);
    BuildElementSlot(newOpcode, offset, layout->Instance, layout->Value, layout->SlotIndex, layout->profileId);
}

void
IRBuilder::BuildElementSlot(Js::OpCode newOpcode, uint32 offset, Js::RegSlot fieldRegSlot, Js::RegSlot regSlot,
    int32 slotId, Js::ProfileId profileId)
{
    Assert(OpCodeAttr::HasMultiSizeLayout(newOpcode));

    IR::Instr *     instr;
    IR::RegOpnd * regOpnd;

    IR::SymOpnd *   fieldSymOpnd;
    PropertyKind    propertyKind = PropertyKindSlots;
    PropertySym *   fieldSym;
    bool isLdSlotThatWasNotProfiled = false;

    switch (newOpcode)
    {
    case Js::OpCode::NewInnerScFunc:
    case Js::OpCode::NewInnerStackScFunc:
        newOpcode = Js::OpCode::NewScFunc;
        goto NewScFuncCommon;

    case Js::OpCode::NewInnerScGenFunc:
        newOpcode = Js::OpCode::NewScGenFunc;
NewScFuncCommon:
    {
        IR::Opnd * functionBodySlotOpnd = IR::IntConstOpnd::New(slotId, TyInt32, m_func, true);
        IR::Opnd * environmentOpnd = this->BuildSrcOpnd(fieldRegSlot);
        regOpnd = this->BuildDstOpnd(regSlot);
        instr = IR::Instr::New(newOpcode, regOpnd, functionBodySlotOpnd, environmentOpnd, m_func);
        if (regOpnd->m_sym->m_isSingleDef)
        {
            regOpnd->m_sym->m_isSafeThis = true;
            regOpnd->m_sym->m_isNotInt = true;
        }
        this->AddInstr(instr, offset);
        return;
    }

    case Js::OpCode::LdObjSlot:
        newOpcode = Js::OpCode::LdSlot;
        goto ObjSlotCommon;

    case Js::OpCode::StObjSlot:
        newOpcode = Js::OpCode::StSlot;
        goto ObjSlotCommon;

    case Js::OpCode::StObjSlotChkUndecl:
        newOpcode = Js::OpCode::StSlotChkUndecl;

ObjSlotCommon:
        regOpnd = IR::RegOpnd::New(TyVar, m_func);
        fieldSymOpnd = this->BuildFieldOpnd(newOpcode, fieldRegSlot, (Js::DynamicObject::GetOffsetOfAuxSlots())/sizeof(Js::Var), (Js::PropertyIdIndexType)-1, PropertyKindSlotArray);
        instr = IR::Instr::New(Js::OpCode::LdSlotArr, regOpnd, fieldSymOpnd, m_func);
        this->AddInstr(instr, offset);

        fieldSym = PropertySym::New(regOpnd->m_sym, slotId, (uint32)-1, (uint)-1, PropertyKindSlots, m_func);
        fieldSymOpnd = IR::SymOpnd::New(fieldSym, TyVar, m_func);

        if (newOpcode == Js::OpCode::StSlot || newOpcode == Js::OpCode::StSlotChkUndecl)
        {
            goto StSlotCommon;
        }
        goto LdSlotCommon;

    case Js::OpCode::LdSlotArr:
        propertyKind = PropertyKindSlotArray;
    case Js::OpCode::LdSlot:
        // Load
        fieldSymOpnd = this->BuildFieldOpnd(newOpcode, fieldRegSlot, slotId, (Js::PropertyIdIndexType)-1, propertyKind);

LdSlotCommon:
        regOpnd = this->BuildDstOpnd(regSlot);

        instr = nullptr;
        if (profileId != Js::Constants::NoProfileId)
        {
            instr = this->BuildProfiledSlotLoad(newOpcode, regOpnd, fieldSymOpnd, profileId, &isLdSlotThatWasNotProfiled);
        }
        if (!instr)
        {
            instr = IR::Instr::New(newOpcode, regOpnd, fieldSymOpnd, m_func);
        }
        break;

    case Js::OpCode::StSlot:
    case Js::OpCode::StSlotChkUndecl:
        // Store
        fieldSymOpnd = this->BuildFieldOpnd(newOpcode, fieldRegSlot, slotId, (Js::PropertyIdIndexType)-1, propertyKind);

StSlotCommon:
        regOpnd = this->BuildSrcOpnd(regSlot);

        instr = IR::Instr::New(newOpcode, fieldSymOpnd, regOpnd, m_func);
        if (newOpcode == Js::OpCode::StSlotChkUndecl)
        {
            // ChkUndecl includes an implicit read of the destination. Communicate the liveness by using the destination in src2.
            instr->SetSrc2(fieldSymOpnd);
        }
        break;

    default:
        AssertMsg(UNREACHED, "Unknown ElementSlot opcode");
        Fatal();
    }

    this->AddInstr(instr, offset);

    if(isLdSlotThatWasNotProfiled && DoBailOnNoProfile())
    {
        InsertBailOnNoProfile(instr);
    }
}

template <typename SizePolicy>
void
IRBuilder::BuildElementSlotI1(Js::OpCode newOpcode, uint32 offset)
{
    Assert(!OpCodeAttr::IsProfiledOp(newOpcode));
    Assert(OpCodeAttr::HasMultiSizeLayout(newOpcode));
    auto layout = m_jnReader.GetLayout<Js::OpLayoutT_ElementSlotI1<SizePolicy>>();

    if (!PHASE_OFF(Js::ClosureRegCheckPhase, m_func))
    {
        this->DoClosureRegCheck(layout->Value);
    }

    BuildElementSlotI1(newOpcode, offset, layout->Value, layout->SlotIndex, Js::Constants::NoProfileId);
}

template <typename SizePolicy>
void
IRBuilder::BuildProfiledElementSlotI1(Js::OpCode newOpcode, uint32 offset)
{
    Assert(OpCodeAttr::IsProfiledOp(newOpcode));
    Assert(OpCodeAttr::HasMultiSizeLayout(newOpcode));
    auto layout = m_jnReader.GetLayout<Js::OpLayoutDynamicProfile<Js::OpLayoutT_ElementSlotI1<SizePolicy>>>();

    if (!PHASE_OFF(Js::ClosureRegCheckPhase, m_func))
    {
        this->DoClosureRegCheck(layout->Value);
    }

    Js::OpCodeUtil::ConvertNonCallOpToNonProfiled(newOpcode);
    BuildElementSlotI1(newOpcode, offset, layout->Value, layout->SlotIndex, layout->profileId);
}

void
IRBuilder::BuildElementSlotI1(Js::OpCode newOpcode, uint32 offset, Js::RegSlot regSlot,
                              int32 slotId, Js::ProfileId profileId)
{
    IR::RegOpnd *regOpnd;
    IR::SymOpnd *fieldOpnd;
    IR::Instr   *instr = nullptr;
    IR::ByteCodeUsesInstr *byteCodeUse;
    PropertySym *fieldSym;
    SymID        symID;
    bool isLdSlotThatWasNotProfiled = false;

    switch (newOpcode)
    {
        case Js::OpCode::LdLocalSlot:

            if (PHASE_ON(Js::ClosureRangeCheckPhase, m_func))
            {
                if ((uint32)slotId >= m_func->GetJnFunction()->scopeSlotArraySize + Js::ScopeSlots::FirstSlotIndex)
                {
                    Js::Throw::FatalInternalError();
                }
            }

            if (m_func->GetLocalClosureSym()->HasByteCodeRegSlot())
            {
                byteCodeUse = IR::ByteCodeUsesInstr::New(m_func);
                byteCodeUse->byteCodeUpwardExposedUsed = JitAnew(m_func->m_alloc, BVSparse<JitArenaAllocator>, m_func->m_alloc);
                byteCodeUse->byteCodeUpwardExposedUsed->Set(m_func->GetLocalClosureSym()->m_id);
                this->AddInstr(byteCodeUse, offset);
            }

            // Read the scope slot pointer back using the stack closure sym.
            newOpcode = Js::OpCode::LdSlot;
            if (m_func->DoStackFrameDisplay())
            {
                // Read the scope slot pointer back using the stack closure sym.
                fieldOpnd = this->BuildFieldOpnd(Js::OpCode::LdSlotArr, m_func->GetLocalClosureSym()->m_id, 0, (Js::PropertyIdIndexType)-1, PropertyKindSlotArray);

                regOpnd = IR::RegOpnd::New(TyVar, m_func);
                instr = IR::Instr::New(Js::OpCode::LdSlotArr, regOpnd, fieldOpnd, m_func);
                this->AddInstr(instr, offset);
                symID = regOpnd->m_sym->m_id;

                if (IsLoopBody())
                {
                    fieldOpnd = this->BuildFieldOpnd(Js::OpCode::LdSlotArr, m_func->GetLocalClosureSym()->m_id, slotId, (Js::PropertyIdIndexType)-1, PropertyKindSlotArray);
                    // Need a dynamic check on the size of the local slot array.
                    m_func->GetTopFunc()->AddSlotArrayCheck(fieldOpnd);
                }
            }
            else
            {
                symID = m_func->GetJnFunction()->GetLocalClosureReg();
                if (IsLoopBody())
                {
                    this->EnsureLoopBodyLoadSlot(symID);
                }
            }
            fieldSym = PropertySym::FindOrCreate(symID, slotId, (uint32)-1, (uint)-1, PropertyKindSlots, m_func);
            fieldOpnd = IR::SymOpnd::New(fieldSym, TyVar, m_func);
            regOpnd = this->BuildDstOpnd(regSlot);
            instr = nullptr;
            if (profileId != Js::Constants::NoProfileId)
            {
                instr = this->BuildProfiledSlotLoad(Js::OpCode::LdSlot, regOpnd, fieldOpnd, profileId, &isLdSlotThatWasNotProfiled);
            }
            if (!instr)
            {
                instr = IR::Instr::New(Js::OpCode::LdSlot, regOpnd, fieldOpnd, m_func);
            }
            this->AddInstr(instr, offset);

            if (!m_func->DoStackFrameDisplay() && IsLoopBody())
            {
                // Need a dynamic check on the size of the local slot array.
                m_func->GetTopFunc()->AddSlotArrayCheck(fieldOpnd);
            }
            break;

        case Js::OpCode::LdLocalObjSlot:
            if (m_func->GetLocalClosureSym()->HasByteCodeRegSlot())
            {
                byteCodeUse = IR::ByteCodeUsesInstr::New(m_func);
                byteCodeUse->byteCodeUpwardExposedUsed = JitAnew(m_func->m_alloc, BVSparse<JitArenaAllocator>, m_func->m_alloc);
                byteCodeUse->byteCodeUpwardExposedUsed->Set(m_func->GetLocalClosureSym()->m_id);
                this->AddInstr(byteCodeUse, offset);
            }

            fieldOpnd = this->BuildFieldOpnd(newOpcode, m_func->GetJnFunction()->GetLocalClosureReg(), (Js::DynamicObject::GetOffsetOfAuxSlots())/sizeof(Js::Var), (Js::PropertyIdIndexType)-1, PropertyKindSlotArray);
            regOpnd = IR::RegOpnd::New(TyVar, m_func);
            instr = IR::Instr::New(Js::OpCode::LdSlotArr, regOpnd, fieldOpnd, m_func);
            this->AddInstr(instr, offset);

            fieldSym = PropertySym::New(regOpnd->m_sym, slotId, (uint32)-1, (uint)-1, PropertyKindSlots, m_func);
            fieldOpnd = IR::SymOpnd::New(fieldSym, TyVar, m_func);

            regOpnd = this->BuildDstOpnd(regSlot);
            instr = nullptr;
            newOpcode = Js::OpCode::LdSlot;
            if (profileId != Js::Constants::NoProfileId)
            {
                instr = this->BuildProfiledSlotLoad(newOpcode, regOpnd, fieldOpnd, profileId, &isLdSlotThatWasNotProfiled);
            }
            if (!instr)
            {
                instr = IR::Instr::New(newOpcode, regOpnd, fieldOpnd, m_func);
            }
            this->AddInstr(instr, offset);
            break;      
            
        case Js::OpCode::StLocalSlot:
        case Js::OpCode::StLocalSlotChkUndecl:

            if (PHASE_ON(Js::ClosureRangeCheckPhase, m_func))
            {
                if ((uint32)slotId >= m_func->GetJnFunction()->scopeSlotArraySize + Js::ScopeSlots::FirstSlotIndex)
                {
                    Js::Throw::FatalInternalError();
                }
            }

            if (m_func->GetLocalClosureSym()->HasByteCodeRegSlot())
            {
                byteCodeUse = IR::ByteCodeUsesInstr::New(m_func);
                byteCodeUse->byteCodeUpwardExposedUsed = JitAnew(m_func->m_alloc, BVSparse<JitArenaAllocator>, m_func->m_alloc);
                byteCodeUse->byteCodeUpwardExposedUsed->Set(m_func->GetLocalClosureSym()->m_id);
                this->AddInstr(byteCodeUse, offset);
            }

            newOpcode = newOpcode == Js::OpCode::StLocalSlot ? Js::OpCode::StSlot : Js::OpCode::StSlotChkUndecl;
            if (m_func->DoStackFrameDisplay())
            {
                regOpnd = IR::RegOpnd::New(TyVar, m_func);
                // Read the scope slot pointer back using the stack closure sym.
                fieldOpnd = this->BuildFieldOpnd(Js::OpCode::LdSlotArr, m_func->GetLocalClosureSym()->m_id, 0, (Js::PropertyIdIndexType)-1, PropertyKindSlotArray);

                instr = IR::Instr::New(Js::OpCode::LdSlotArr, regOpnd, fieldOpnd, m_func);
                this->AddInstr(instr, offset);
                symID = regOpnd->m_sym->m_id;

                if (IsLoopBody())
                {
                    fieldOpnd = this->BuildFieldOpnd(Js::OpCode::LdSlotArr, m_func->GetLocalClosureSym()->m_id, slotId, (Js::PropertyIdIndexType)-1, PropertyKindSlotArray);
                    // Need a dynamic check on the size of the local slot array.
                    m_func->GetTopFunc()->AddSlotArrayCheck(fieldOpnd);
                }
            }
            else
            {
                symID = m_func->GetJnFunction()->GetLocalClosureReg();
                if (IsLoopBody())
                {
                    this->EnsureLoopBodyLoadSlot(symID);
                }
            }
            fieldSym = PropertySym::FindOrCreate(symID, slotId, (uint32)-1, (uint)-1, PropertyKindSlots, m_func);
            fieldOpnd = IR::SymOpnd::New(fieldSym, TyVar, m_func);
            regOpnd = this->BuildSrcOpnd(regSlot);
            instr = IR::Instr::New(newOpcode, fieldOpnd, regOpnd, m_func);
            this->AddInstr(instr, offset);
            if (newOpcode == Js::OpCode::StSlotChkUndecl)
            {
                instr->SetSrc2(fieldOpnd);
            }

            if (!m_func->DoStackFrameDisplay() && IsLoopBody())
            {
                // Need a dynamic check on the size of the local slot array.
                m_func->GetTopFunc()->AddSlotArrayCheck(fieldOpnd);
            }
            break;

        case Js::OpCode::StLocalObjSlot:
        case Js::OpCode::StLocalObjSlotChkUndecl:

            if (m_func->GetLocalClosureSym()->HasByteCodeRegSlot())
            {
                byteCodeUse = IR::ByteCodeUsesInstr::New(m_func);
                byteCodeUse->byteCodeUpwardExposedUsed = JitAnew(m_func->m_alloc, BVSparse<JitArenaAllocator>, m_func->m_alloc);
                byteCodeUse->byteCodeUpwardExposedUsed->Set(m_func->GetLocalClosureSym()->m_id);
                this->AddInstr(byteCodeUse, offset);
            }

            regOpnd = IR::RegOpnd::New(TyVar, m_func);
            fieldOpnd = this->BuildFieldOpnd(Js::OpCode::LdSlotArr, m_func->GetJnFunction()->GetLocalClosureReg(), (Js::DynamicObject::GetOffsetOfAuxSlots())/sizeof(Js::Var), (Js::PropertyIdIndexType)-1, PropertyKindSlotArray);
            instr = IR::Instr::New(Js::OpCode::LdSlotArr, regOpnd, fieldOpnd, m_func);
            this->AddInstr(instr, offset);

            newOpcode = newOpcode == Js::OpCode::StLocalObjSlot ? Js::OpCode::StSlot : Js::OpCode::StSlotChkUndecl;
            fieldSym = PropertySym::New(regOpnd->m_sym, slotId, (uint32)-1, (uint)-1, PropertyKindSlots, m_func);
            fieldOpnd = IR::SymOpnd::New(fieldSym, TyVar, m_func);
            regOpnd = this->BuildSrcOpnd(regSlot);

            instr = IR::Instr::New(newOpcode, fieldOpnd, regOpnd, m_func);
            if (newOpcode == Js::OpCode::StSlotChkUndecl)
            {
                // ChkUndecl includes an implicit read of the destination. Communicate the liveness by using the destination in src2.
                instr->SetSrc2(fieldOpnd);
            }
            this->AddInstr(instr, offset);
            break;

        case Js::OpCode::LdEnvObj:
            fieldOpnd = this->BuildFieldOpnd(Js::OpCode::LdSlotArr, this->GetEnvReg(), slotId, (Js::PropertyIdIndexType)-1, PropertyKindSlotArray);
            regOpnd = this->BuildDstOpnd(regSlot);
            instr = IR::Instr::New(Js::OpCode::LdSlotArr, regOpnd, fieldOpnd, m_func);
            this->AddInstr(instr, offset);

            m_func->GetTopFunc()->AddFrameDisplayCheck(fieldOpnd);
            break;

        case Js::OpCode::NewScFunc:
        case Js::OpCode::NewStackScFunc:
            newOpcode = Js::OpCode::NewScFunc;
            goto NewScFuncCommon;

        case Js::OpCode::NewScGenFunc:
            newOpcode = Js::OpCode::NewScGenFunc;
NewScFuncCommon:
            {
                IR::Opnd * functionBodySlotOpnd = IR::IntConstOpnd::New(slotId, TyInt32, m_func, true);
                SymID symID;

                // The byte code doesn't refer directly to a closure environment. Get the implicit one
                // that's pointed to by the function body.
                if (m_func->DoStackFrameDisplay() && m_func->GetLocalFrameDisplaySym())
                {
                    // Read the scope slot pointer back using the stack closure sym.
                    fieldOpnd = this->BuildFieldOpnd(Js::OpCode::LdSlotArr, m_func->GetLocalFrameDisplaySym()->m_id, 0, (Js::PropertyIdIndexType)-1, PropertyKindSlotArray);

                    regOpnd = IR::RegOpnd::New(TyVar, m_func);
                    instr = IR::Instr::New(Js::OpCode::LdSlotArr, regOpnd, fieldOpnd, m_func);
                    this->AddInstr(instr, offset);
                    symID = regOpnd->m_sym->m_id;
                }
                else
                {
                    symID = this->GetEnvRegForInnerFrameDisplay();
                    Assert(symID != Js::Constants::NoRegister);
                    if (IsLoopBody() && !RegIsConstant(symID))
                    {
                        this->EnsureLoopBodyLoadSlot(symID);
                    }
                }

                StackSym *stackSym = StackSym::FindOrCreate(symID, (Js::RegSlot)symID, m_func);
                IR::Opnd * environmentOpnd = IR::RegOpnd::New(stackSym, TyVar, m_func);
                regOpnd = this->BuildDstOpnd(regSlot);
                instr = IR::Instr::New(newOpcode, regOpnd, functionBodySlotOpnd, environmentOpnd, m_func);
                if (regOpnd->m_sym->m_isSingleDef)
                {
                    regOpnd->m_sym->m_isSafeThis = true;
                    regOpnd->m_sym->m_isNotInt = true;
                }
                this->AddInstr(instr, offset);
                return;
            }

        default:
            Assert(0);
    }


    if(isLdSlotThatWasNotProfiled && DoBailOnNoProfile())
    {
        InsertBailOnNoProfile(instr);
    }
}

template <typename SizePolicy>
void
IRBuilder::BuildElementSlotI2(Js::OpCode newOpcode, uint32 offset)
{
    Assert(!OpCodeAttr::IsProfiledOp(newOpcode));
    Assert(OpCodeAttr::HasMultiSizeLayout(newOpcode));
    auto layout = m_jnReader.GetLayout<Js::OpLayoutT_ElementSlotI2<SizePolicy>>();

    if (!PHASE_OFF(Js::ClosureRegCheckPhase, m_func))
    {
        this->DoClosureRegCheck(layout->Value);
    }

    BuildElementSlotI2(newOpcode, offset, layout->Value, layout->SlotIndex1, layout->SlotIndex2, Js::Constants::NoProfileId);
}

template <typename SizePolicy>
void
IRBuilder::BuildProfiledElementSlotI2(Js::OpCode newOpcode, uint32 offset)
{
    Assert(OpCodeAttr::IsProfiledOp(newOpcode));
    Assert(OpCodeAttr::HasMultiSizeLayout(newOpcode));
    auto layout = m_jnReader.GetLayout<Js::OpLayoutDynamicProfile<Js::OpLayoutT_ElementSlotI2<SizePolicy>>>();

    if (!PHASE_OFF(Js::ClosureRegCheckPhase, m_func))
    {
        this->DoClosureRegCheck(layout->Value);
    }

    Js::OpCodeUtil::ConvertNonCallOpToNonProfiled(newOpcode);
    BuildElementSlotI2(newOpcode, offset, layout->Value, layout->SlotIndex1, layout->SlotIndex2, layout->profileId);
}

void
IRBuilder::BuildElementSlotI2(Js::OpCode newOpcode, uint32 offset, Js::RegSlot regSlot,
                              int32 slotId1, int32 slotId2, Js::ProfileId profileId)
{
    IR::RegOpnd *regOpnd;
    IR::SymOpnd *fieldOpnd;
    IR::Instr   *instr;
    PropertySym *fieldSym;
    bool isLdSlotThatWasNotProfiled = false;

    switch (newOpcode)
    {
        case Js::OpCode::LdEnvSlot:
        case Js::OpCode::LdEnvObjSlot:
        case Js::OpCode::StEnvSlot:
        case Js::OpCode::StEnvSlotChkUndecl:
        case Js::OpCode::StEnvObjSlot:
        case Js::OpCode::StEnvObjSlotChkUndecl:

            fieldOpnd = this->BuildFieldOpnd(Js::OpCode::LdSlotArr, this->GetEnvReg(), slotId1, (Js::PropertyIdIndexType)-1, PropertyKindSlotArray);
            regOpnd = IR::RegOpnd::New(TyVar, m_func);
            instr = IR::Instr::New(Js::OpCode::LdSlotArr, regOpnd, fieldOpnd, m_func);
            this->AddInstr(instr, offset);

            switch (newOpcode)
            {
                case Js::OpCode::LdEnvObjSlot:
                case Js::OpCode::StEnvObjSlot:
                case Js::OpCode::StEnvObjSlotChkUndecl:

                    m_func->GetTopFunc()->AddFrameDisplayCheck(fieldOpnd, (uint32)-1);
                    fieldSym = PropertySym::New(regOpnd->m_sym, (Js::DynamicObject::GetOffsetOfAuxSlots())/sizeof(Js::Var),
                                                (uint32)-1, (uint)-1, PropertyKindSlotArray, m_func);
                    fieldOpnd = IR::SymOpnd::New(fieldSym, TyVar, m_func);
                    regOpnd = IR::RegOpnd::New(TyVar, m_func);
                    instr = IR::Instr::New(Js::OpCode::LdSlotArr, regOpnd, fieldOpnd, m_func);
                    this->AddInstr(instr, offset);
                    break;

                default:
                    m_func->GetTopFunc()->AddFrameDisplayCheck(fieldOpnd, slotId2);
                    break;
            }

            fieldSym = PropertySym::New(regOpnd->m_sym, slotId2, (uint32)-1, (uint)-1, PropertyKindSlots, m_func);
            fieldOpnd = IR::SymOpnd::New(fieldSym, TyVar, m_func);

            switch (newOpcode)
            {
                case Js::OpCode::LdEnvSlot:
                case Js::OpCode::LdEnvObjSlot:
                    newOpcode = Js::OpCode::LdSlot;
                    regOpnd = this->BuildDstOpnd(regSlot);
                    instr = nullptr;
                    if (profileId != Js::Constants::NoProfileId)
                    {
                        instr = this->BuildProfiledSlotLoad(newOpcode, regOpnd, fieldOpnd, profileId, &isLdSlotThatWasNotProfiled);
                    }
                    if (!instr)
                    {
                        instr = IR::Instr::New(newOpcode, regOpnd, fieldOpnd, m_func);
                    }
                    break;

                default:
                    newOpcode =
                        newOpcode == Js::OpCode::StEnvSlot || newOpcode == Js::OpCode::StEnvObjSlot ? Js::OpCode::StSlot : Js::OpCode::StSlotChkUndecl;
                    regOpnd = this->BuildSrcOpnd(regSlot);
                    instr = IR::Instr::New(newOpcode, fieldOpnd, regOpnd, m_func);
                    if (newOpcode == Js::OpCode::StSlotChkUndecl)
                    {
                        // ChkUndecl includes an implicit read of the destination. Communicate the liveness by using the destination in src2.
                        instr->SetSrc2(fieldOpnd);
                    }
                    break;
            }
            this->AddInstr(instr, offset);
            if(isLdSlotThatWasNotProfiled && DoBailOnNoProfile())
            {
                InsertBailOnNoProfile(instr);
            }
            break;

        case Js::OpCode::StInnerObjSlot:
        case Js::OpCode::StInnerSlot:
            if ((uint)slotId1 >= m_func->GetJnFunction()->GetInnerScopeCount())
            {
                Js::Throw::FatalInternalError();
            }
            regOpnd = this->BuildSrcOpnd(regSlot);
            slotId1 += this->m_func->GetJnFunction()->FirstInnerScopeReg();
            if ((uint)slotId1 >= this->m_func->GetJnFunction()->GetLocalsCount())
            {
                Js::Throw::FatalInternalError();
            }
            if (newOpcode == Js::OpCode::StInnerObjSlot)
            {
                IR::RegOpnd * slotOpnd = IR::RegOpnd::New(TyVar, m_func);
                fieldOpnd = this->BuildFieldOpnd(Js::OpCode::LdSlotArr, slotId1, (Js::DynamicObject::GetOffsetOfAuxSlots())/sizeof(Js::Var), (Js::PropertyIdIndexType)-1, PropertyKindSlotArray);
                instr = IR::Instr::New(Js::OpCode::LdSlotArr, slotOpnd, fieldOpnd, m_func);
                this->AddInstr(instr, offset);

                PropertySym *propertySym = PropertySym::New(slotOpnd->m_sym, slotId2, (uint32)-1, (uint)-1, PropertyKindSlots, m_func);
                fieldOpnd = IR::PropertySymOpnd::New(propertySym, (Js::CacheId)-1, TyVar, m_func);
            }
            else
            {
                fieldOpnd = this->BuildFieldOpnd(Js::OpCode::StSlot, slotId1, slotId2, (Js::PropertyIdIndexType)-1, PropertyKindSlots);
                if (!this->DoSlotArrayCheck(fieldOpnd, IsLoopBody()))
                {
                    // Need a dynamic check on the size of the local slot array.
                    m_func->GetTopFunc()->AddSlotArrayCheck(fieldOpnd);
                }
            }
            instr = IR::Instr::New(Js::OpCode::StSlot, fieldOpnd, regOpnd, m_func);
            this->AddInstr(instr, offset);

            break;

        case Js::OpCode::LdInnerSlot:
        case Js::OpCode::LdInnerObjSlot:
            if ((uint)slotId1 >= m_func->GetJnFunction()->GetInnerScopeCount())
            {
                Js::Throw::FatalInternalError();
            }
            slotId1 += this->m_func->GetJnFunction()->FirstInnerScopeReg();
            if ((uint)slotId1 >= this->m_func->GetJnFunction()->GetLocalsCount())
            {
                Js::Throw::FatalInternalError();
            }
            if (newOpcode == Js::OpCode::LdInnerObjSlot)
            {
                IR::RegOpnd * slotOpnd = IR::RegOpnd::New(TyVar, m_func);
                fieldOpnd = this->BuildFieldOpnd(Js::OpCode::LdSlotArr, slotId1, (Js::DynamicObject::GetOffsetOfAuxSlots())/sizeof(Js::Var), (Js::PropertyIdIndexType)-1, PropertyKindSlotArray);
                instr = IR::Instr::New(Js::OpCode::LdSlotArr, slotOpnd, fieldOpnd, m_func);
                this->AddInstr(instr, offset);

                PropertySym *propertySym = PropertySym::New(slotOpnd->m_sym, slotId2, (uint32)-1, (uint)-1, PropertyKindSlots, m_func);
                fieldOpnd = IR::PropertySymOpnd::New(propertySym, (Js::CacheId)-1, TyVar, m_func);
            }
            else
            {
                fieldOpnd = this->BuildFieldOpnd(Js::OpCode::LdSlot, slotId1, slotId2, (Js::PropertyIdIndexType)-1, PropertyKindSlots);
                if (!this->DoSlotArrayCheck(fieldOpnd, IsLoopBody()))
                {
                    // Need a dynamic check on the size of the local slot array.
                    m_func->GetTopFunc()->AddSlotArrayCheck(fieldOpnd);
                }
            }
            regOpnd = this->BuildDstOpnd(regSlot);
            instr = IR::Instr::New(Js::OpCode::LdSlot, regOpnd, fieldOpnd, m_func);
            this->AddInstr(instr, offset);

            break;

        default:
            Assert(0);
    }
}
IR::SymOpnd *
IRBuilder::BuildLoopBodySlotOpnd(SymID symId)
{
    Assert(!this->RegIsConstant((Js::RegSlot)symId));

    // Get the interpreter frame instance that was passed in.
    StackSym *loopParamSym = m_func->EnsureLoopParamSym();

    PropertySym * fieldSym     = PropertySym::FindOrCreate(loopParamSym->m_id, (Js::PropertyId)(symId + this->m_loopBodyLocalsStartSlot), (uint32)-1, (uint)-1, PropertyKindLocalSlots, m_func);
    return IR::SymOpnd::New(fieldSym, TyVar, m_func);
}

void
IRBuilder::EnsureLoopBodyLoadSlot(SymID symId, bool isCatchObjectSym)
{
    // No need to emit LdSlot for a catch object. In fact, if we do, we might be loading an uninitialized value from the slot.
    if (isCatchObjectSym)
    {
        return;
    }
    StackSym * symDst = StackSym::FindOrCreate(symId, (Js::RegSlot)symId, m_func);
    if (symDst->m_isCatchObjectSym || this->m_ldSlots->TestAndSet(symId))
    {
        return;
    }

    IR::SymOpnd * fieldSymOpnd = this->BuildLoopBodySlotOpnd(symId);
    IR::RegOpnd * dstOpnd = IR::RegOpnd::New(symDst, TyVar, m_func);
    IR::Instr * ldSlotInstr;

    JsLoopBodyCodeGen* loopBodyCodeGen = (JsLoopBodyCodeGen*)m_func->m_workItem;

    ValueType symValueType;
    if(loopBodyCodeGen->symIdToValueTypeMap && loopBodyCodeGen->symIdToValueTypeMap->TryGetValue(symId, &symValueType))
    {
        ldSlotInstr = IR::ProfiledInstr::New(Js::OpCode::LdSlot, dstOpnd, fieldSymOpnd, m_func);
        ldSlotInstr->AsProfiledInstr()->u.FldInfo().valueType = symValueType;
    }
    else
    {
        ldSlotInstr = IR::Instr::New(Js::OpCode::LdSlot, dstOpnd, fieldSymOpnd, m_func);
    }

    m_func->m_headInstr->InsertAfter(ldSlotInstr);
    if (m_lastInstr == m_func->m_headInstr)
    {
        m_lastInstr = ldSlotInstr;
    }
}

void
IRBuilder::SetLoopBodyStSlot(SymID symID, bool isCatchObjectSym)
{
    if (this->m_func->HasTry() && !PHASE_OFF(Js::JITLoopBodyInTryCatchPhase, this->m_func))
    {
        // No need to emit StSlot for a catch object. In fact, if we do, we might be storing an uninitialized value to the slot.
        if (isCatchObjectSym)
        {
            return;
        }
        StackSym * dstSym = StackSym::FindOrCreate(symID, (Js::RegSlot)symID, m_func);
        Assert(dstSym);
        if (dstSym->m_isCatchObjectSym)
        {
            return;
        }
    }
    this->m_stSlots->Set(symID);
}

///----------------------------------------------------------------------------
///
/// IRBuilder::BuildElementCP
///
///     Build IR instr for an ElementCP or ElementRootCP instruction.
///
///----------------------------------------------------------------------------

IR::Instr *
IRBuilder::BuildProfiledFieldLoad(Js::OpCode loadOp, IR::RegOpnd *dstOpnd, IR::SymOpnd *srcOpnd, Js::CacheId inlineCacheIndex, bool *pUnprofiled)
{
    IR::Instr * instr = nullptr;

    // Prefer JitProfilingInstr if we're in simplejit
    if (m_func->DoSimpleJitDynamicProfile())
    {
        instr = IR::JitProfilingInstr::New(loadOp, dstOpnd, srcOpnd, m_func);
    }
    else if (this->m_func->HasProfileInfo())
    {
        Js::ReadOnlyDynamicProfileInfo * profile = this->m_func->GetProfileInfo();
        instr = IR::ProfiledInstr::New(loadOp, dstOpnd, srcOpnd, m_func);
        instr->AsProfiledInstr()->u.FldInfo() = *(profile->GetFldInfo(this->m_func->GetJnFunction(), inlineCacheIndex));
        *pUnprofiled = !instr->AsProfiledInstr()->u.FldInfo().WasLdFldProfiled();
        dstOpnd->SetValueType(instr->AsProfiledInstr()->u.FldInfo().valueType);
#if ENABLE_DEBUG_CONFIG_OPTIONS
        if(Js::Configuration::Global.flags.TestTrace.IsEnabled(Js::DynamicProfilePhase))
        {
            Js::FunctionBody * func = this->m_func->GetJnFunction();
            const ValueType valueType(instr->AsProfiledInstr()->u.FldInfo().valueType);
            char valueTypeStr[VALUE_TYPE_MAX_STRING_SIZE];
            valueType.ToString(valueTypeStr);
            wchar_t debugStringBuffer[MAX_FUNCTION_BODY_DEBUG_STRING_SIZE];
            Output::Print(L"TestTrace function %s (%s) ValueType = %i ", func->GetDisplayName(), func->GetDebugNumberSet(debugStringBuffer), valueTypeStr);
            instr->DumpTestTrace();
        }
#endif
    }

    return instr;
}

Js::RegSlot IRBuilder::GetEnvRegForEvalCode() const
{
    Js::FunctionBody * functionBody = m_func->GetJnFunction();

    if (functionBody->GetIsStrictMode() && functionBody->GetIsGlobalFunc())
    {
        return functionBody->GetLocalFrameDisplayReg();
    }
    else
    {
        return functionBody->GetEnvReg();
    }
}

template <typename SizePolicy>
void
IRBuilder::BuildElementP(Js::OpCode newOpcode, uint32 offset)
{
    Assert(OpCodeAttr::HasMultiSizeLayout(newOpcode));
    auto layout = m_jnReader.GetLayout<Js::OpLayoutT_ElementP<SizePolicy>>();

    if (!PHASE_OFF(Js::ClosureRegCheckPhase, m_func))
    {
        this->DoClosureRegCheck(layout->Value);
    }

    BuildElementP(newOpcode, offset, layout->Value, layout->inlineCacheIndex);
}

void
IRBuilder::BuildElementP(Js::OpCode newOpcode, uint32 offset, Js::RegSlot regSlot, Js::CacheId inlineCacheIndex)
{
    Assert(OpCodeAttr::HasMultiSizeLayout(newOpcode));

    IR::Instr *     instr;
    IR::RegOpnd *   regOpnd;
    IR::Opnd *      srcOpnd;
    IR::SymOpnd *   fieldSymOpnd;
    Js::PropertyId  propertyId;
    bool isProfiled = OpCodeAttr::IsProfiledOp(newOpcode);
    bool isLdFldThatWasNotProfiled = false;

    if (isProfiled)
    {
        Js::OpCodeUtil::ConvertNonCallOpToNonProfiled(newOpcode);
    }

    propertyId = this->m_func->GetJnFunction()->GetPropertyIdFromCacheId(inlineCacheIndex);

    Js::RegSlot instance = this->GetEnvRegForEvalCode();

    switch (newOpcode)
    {
    case Js::OpCode::LdLocalFld:
        if (m_func->GetLocalClosureSym()->HasByteCodeRegSlot())
        {
            IR::ByteCodeUsesInstr * byteCodeUse = IR::ByteCodeUsesInstr::New(m_func);
            byteCodeUse->byteCodeUpwardExposedUsed = JitAnew(m_func->m_alloc, BVSparse<JitArenaAllocator>, m_func->m_alloc);
            byteCodeUse->byteCodeUpwardExposedUsed->Set(m_func->GetLocalClosureSym()->m_id);
            this->AddInstr(byteCodeUse, offset);
        }

        newOpcode = Js::OpCode::LdFld;
        fieldSymOpnd = this->BuildFieldOpnd(newOpcode, m_func->GetJnFunction()->GetLocalClosureReg(), propertyId, (Js::PropertyIdIndexType)-1, PropertyKindData, inlineCacheIndex);
        if (fieldSymOpnd->IsPropertySymOpnd())
        {
            fieldSymOpnd->AsPropertySymOpnd()->TryDisableRuntimePolymorphicCache();
        }
        regOpnd = this->BuildDstOpnd(regSlot);

        instr = nullptr;
        if (isProfiled)
        {
            instr = this->BuildProfiledFieldLoad(newOpcode, regOpnd, fieldSymOpnd, inlineCacheIndex, &isLdFldThatWasNotProfiled);
        }

        // If it hasn't been set yet
        if (!instr)
        {
            instr = IR::Instr::New(newOpcode, regOpnd, fieldSymOpnd, m_func);
        }
        break;

    case Js::OpCode::StLocalFld:
        if (m_func->GetLocalClosureSym()->HasByteCodeRegSlot())
        {
            IR::ByteCodeUsesInstr * byteCodeUse = IR::ByteCodeUsesInstr::New(m_func);
            byteCodeUse->byteCodeUpwardExposedUsed = JitAnew(m_func->m_alloc, BVSparse<JitArenaAllocator>, m_func->m_alloc);
            byteCodeUse->byteCodeUpwardExposedUsed->Set(m_func->GetLocalClosureSym()->m_id);
            this->AddInstr(byteCodeUse, offset);
        }

        fieldSymOpnd = this->BuildFieldOpnd(newOpcode, m_func->GetJnFunction()->GetLocalClosureReg(), propertyId, (Js::PropertyIdIndexType)-1, PropertyKindData, inlineCacheIndex);
        if (fieldSymOpnd->IsPropertySymOpnd())
        {
            fieldSymOpnd->AsPropertySymOpnd()->TryDisableRuntimePolymorphicCache();
        }
        srcOpnd = this->BuildSrcOpnd(regSlot);
        newOpcode = Js::OpCode::StFld;
        goto stCommon;

    case Js::OpCode::InitLocalFld:
    case Js::OpCode::InitLocalLetFld:
    case Js::OpCode::InitUndeclLocalLetFld:
    case Js::OpCode::InitUndeclLocalConstFld:
    {
        if (m_func->GetLocalClosureSym()->HasByteCodeRegSlot())
        {
            IR::ByteCodeUsesInstr * byteCodeUse = IR::ByteCodeUsesInstr::New(m_func);
            byteCodeUse->byteCodeUpwardExposedUsed = JitAnew(m_func->m_alloc, BVSparse<JitArenaAllocator>, m_func->m_alloc);
            byteCodeUse->byteCodeUpwardExposedUsed->Set(m_func->GetLocalClosureSym()->m_id);
            this->AddInstr(byteCodeUse, offset);
        }

        fieldSymOpnd = this->BuildFieldOpnd(newOpcode, m_func->GetJnFunction()->GetLocalClosureReg(), propertyId, (Js::PropertyIdIndexType)-1, PropertyKindData, inlineCacheIndex);
        // Store
        if (newOpcode == Js::OpCode::InitUndeclLocalLetFld)
        {
            srcOpnd = IR::AddrOpnd::New(m_func->GetScriptContext()->GetLibrary()->GetUndeclBlockVar(), IR::AddrOpndKindDynamicVar, this->m_func, true);
            srcOpnd->SetValueType(ValueType::PrimitiveOrObject);
            newOpcode = Js::OpCode::InitLetFld;
        }
        else if (newOpcode == Js::OpCode::InitUndeclLocalConstFld)
        {
            srcOpnd = IR::AddrOpnd::New(m_func->GetScriptContext()->GetLibrary()->GetUndeclBlockVar(), IR::AddrOpndKindDynamicVar, this->m_func, true);
            srcOpnd->SetValueType(ValueType::PrimitiveOrObject);
            newOpcode = Js::OpCode::InitConstFld;
        }
        else
        {
            srcOpnd = this->BuildSrcOpnd(regSlot);
            newOpcode = newOpcode == Js::OpCode::InitLocalFld ? Js::OpCode::InitFld : Js::OpCode::InitLetFld;
        }

stCommon:
        instr = nullptr;
        if (isProfiled)
        {
            if (m_func->DoSimpleJitDynamicProfile())
            {
                instr = IR::JitProfilingInstr::New(newOpcode, fieldSymOpnd, srcOpnd, m_func);
            }
            else if (this->m_func->HasProfileInfo())
            {

                Js::ReadOnlyDynamicProfileInfo * profile = this->m_func->GetProfileInfo();
                instr = IR::ProfiledInstr::New(newOpcode, fieldSymOpnd, srcOpnd, m_func);
                instr->AsProfiledInstr()->u.FldInfo() = *(profile->GetFldInfo(this->m_func->GetJnFunction(), inlineCacheIndex));
            }
        }

        // If it hasn't been set yet
        if (!instr)
        {
            instr = IR::Instr::New(newOpcode, fieldSymOpnd, srcOpnd, m_func);
        }
        break;
    }

    case Js::OpCode::ScopedLdFld:
    case Js::OpCode::ScopedLdFldForTypeOf:
    {
        Assert(!isProfiled);

        IR::SymOpnd *   fieldSymOpnd = this->BuildFieldOpnd(newOpcode, instance, propertyId, (Js::PropertyIdIndexType)-1, PropertyKindData, inlineCacheIndex);

        // Implicit root object as default instance
        IR::Opnd * instance2Opnd = this->BuildSrcOpnd(Js::FunctionBody::RootObjectRegSlot);
        regOpnd = this->BuildDstOpnd(regSlot);
        instr = IR::Instr::New(newOpcode, regOpnd, fieldSymOpnd, instance2Opnd, m_func);
        break;
    }

    case Js::OpCode::ScopedStFld:
    case Js::OpCode::ConsoleScopedStFld:
    case Js::OpCode::ScopedStFldStrict:
    {
        Assert(!isProfiled);

        IR::SymOpnd *   fieldSymOpnd = this->BuildFieldOpnd(newOpcode, instance, propertyId, (Js::PropertyIdIndexType)-1, PropertyKindData, inlineCacheIndex);

        // Implicit root object as default instance
        IR::Opnd * instance2Opnd = this->BuildSrcOpnd(Js::FunctionBody::RootObjectRegSlot);
        regOpnd = this->BuildSrcOpnd(regSlot);
        instr = IR::Instr::New(newOpcode, fieldSymOpnd, regOpnd, instance2Opnd, m_func);
        break;
    }

    default:
        AssertMsg(UNREACHED, "Unknown ElementP opcode");
        Fatal();
    }

    this->AddInstr(instr, offset);

    if(isLdFldThatWasNotProfiled && DoBailOnNoProfile())
    {
        InsertBailOnNoProfile(instr);
    }
}

template <typename SizePolicy>
void
IRBuilder::BuildElementPIndexed(Js::OpCode newOpcode, uint32 offset)
{
    Assert(OpCodeAttr::HasMultiSizeLayout(newOpcode));
    auto layout = m_jnReader.GetLayout<Js::OpLayoutT_ElementPIndexed<SizePolicy>>();

    if (!PHASE_OFF(Js::ClosureRegCheckPhase, m_func))
    {
        this->DoClosureRegCheck(layout->Value);
    }

    switch (newOpcode)
    {
    case Js::OpCode::InitInnerFld:
        newOpcode = Js::OpCode::InitFld;
        goto initinnerfldcommon;

    case Js::OpCode::InitInnerLetFld:
        newOpcode = Js::OpCode::InitLetFld;
        // fall through
initinnerfldcommon:
    case Js::OpCode::InitUndeclLetFld:
    case Js::OpCode::InitUndeclConstFld:
        BuildElementCP(newOpcode, offset, InnerScopeIndexToRegSlot(layout->scopeIndex), layout->Value, layout->inlineCacheIndex);
        break;

    default:
        AssertMsg(false, "Unknown opcode for ElementPIndexed");
        break;
    }
}

template <typename SizePolicy>
void
IRBuilder::BuildElementCP(Js::OpCode newOpcode, uint32 offset)
{
    Assert(OpCodeAttr::HasMultiSizeLayout(newOpcode));
    auto layout = m_jnReader.GetLayout<Js::OpLayoutT_ElementCP<SizePolicy>>();

    if (!PHASE_OFF(Js::ClosureRegCheckPhase, m_func))
    {
        this->DoClosureRegCheck(layout->Value);
        this->DoClosureRegCheck(layout->Instance);
    }

    BuildElementCP(newOpcode, offset, layout->Instance, layout->Value, layout->inlineCacheIndex);
}

template <typename SizePolicy>
void
IRBuilder::BuildElementRootCP(Js::OpCode newOpcode, uint32 offset)
{
    Assert(OpCodeAttr::HasMultiSizeLayout(newOpcode));
    auto layout = m_jnReader.GetLayout<Js::OpLayoutT_ElementRootCP<SizePolicy>>();

    if (!PHASE_OFF(Js::ClosureRegCheckPhase, m_func))
    {
        this->DoClosureRegCheck(layout->Value);
    }

    BuildElementCP(newOpcode, offset, Js::FunctionBody::RootObjectRegSlot, layout->Value, layout->inlineCacheIndex);
}

void
IRBuilder::BuildElementCP(Js::OpCode newOpcode, uint32 offset, Js::RegSlot instance, Js::RegSlot regSlot, Js::CacheId inlineCacheIndex)
{
    Assert(OpCodeAttr::HasMultiSizeLayout(newOpcode));

    Js::PropertyId  propertyId;
    bool isProfiled = OpCodeAttr::IsProfiledOp(newOpcode);

    if (isProfiled)
    {
        Js::OpCodeUtil::ConvertNonCallOpToNonProfiled(newOpcode);
    }

    propertyId = this->m_func->GetJnFunction()->GetPropertyIdFromCacheId(inlineCacheIndex);

    IR::SymOpnd *   fieldSymOpnd = this->BuildFieldOpnd(newOpcode, instance, propertyId, (Js::PropertyIdIndexType)-1, PropertyKindData, inlineCacheIndex);
    IR::RegOpnd *   regOpnd;

    IR::Instr *     instr = nullptr;
    bool isLdFldThatWasNotProfiled = false;
    switch (newOpcode)
    {
    case Js::OpCode::LdFldForTypeOf:
    case Js::OpCode::LdFld:
        if (fieldSymOpnd->IsPropertySymOpnd())
        {
            fieldSymOpnd->AsPropertySymOpnd()->TryDisableRuntimePolymorphicCache();
        }
    case Js::OpCode::LdFldForCallApplyTarget:
    case Js::OpCode::LdRootFldForTypeOf:
    case Js::OpCode::LdRootFld:
    case Js::OpCode::LdMethodFld:
    case Js::OpCode::LdRootMethodFld:
    case Js::OpCode::ScopedLdMethodFld:
        // Load
        // LdMethodFromFlags is backend only. Don't need to be added here.
        regOpnd = this->BuildDstOpnd(regSlot);

        if (isProfiled)
        {
            instr = this->BuildProfiledFieldLoad(newOpcode, regOpnd, fieldSymOpnd, inlineCacheIndex, &isLdFldThatWasNotProfiled);
        }

        // If it hasn't been set yet
        if (!instr)
        {
            instr = IR::Instr::New(newOpcode, regOpnd, fieldSymOpnd, m_func);
        }

        if (newOpcode == Js::OpCode::LdFld ||
            newOpcode == Js::OpCode::LdFldForCallApplyTarget ||
            newOpcode == Js::OpCode::LdMethodFld ||
            newOpcode == Js::OpCode::LdRootMethodFld ||
            newOpcode == Js::OpCode::ScopedLdMethodFld)
        {
            // Check whether we're loading (what appears to be) a built-in method.
            Js::BuiltinFunction builtInIndex = Js::BuiltinFunction::None;
            PropertySym *fieldSym = fieldSymOpnd->m_sym->AsPropertySym();
            this->CheckBuiltIn(fieldSym, &builtInIndex);
            regOpnd->m_sym->m_builtInIndex = builtInIndex;
        }
        break;

    case Js::OpCode::StFld:
        if (fieldSymOpnd->IsPropertySymOpnd())
        {
            fieldSymOpnd->AsPropertySymOpnd()->TryDisableRuntimePolymorphicCache();
        }
    case Js::OpCode::InitFld:
    case Js::OpCode::InitRootFld:
    case Js::OpCode::InitLetFld:
    case Js::OpCode::InitRootLetFld:
    case Js::OpCode::InitConstFld:
    case Js::OpCode::InitRootConstFld:
    case Js::OpCode::InitUndeclLetFld:
    case Js::OpCode::InitUndeclConstFld:
    case Js::OpCode::InitClassMember:
    case Js::OpCode::StRootFld:
    case Js::OpCode::StFldStrict:
    case Js::OpCode::StRootFldStrict:
    {
        IR::Opnd *srcOpnd;
        // Store
        if (newOpcode == Js::OpCode::InitUndeclLetFld)
        {
            srcOpnd = IR::AddrOpnd::New(m_func->GetScriptContext()->GetLibrary()->GetUndeclBlockVar(), IR::AddrOpndKindDynamicVar, this->m_func, true);
            srcOpnd->SetValueType(ValueType::PrimitiveOrObject);
            newOpcode = Js::OpCode::InitLetFld;
        }
        else if (newOpcode == Js::OpCode::InitUndeclConstFld)
        {
            srcOpnd = IR::AddrOpnd::New(m_func->GetScriptContext()->GetLibrary()->GetUndeclBlockVar(), IR::AddrOpndKindDynamicVar, this->m_func, true);
            srcOpnd->SetValueType(ValueType::PrimitiveOrObject);
            newOpcode = Js::OpCode::InitConstFld;
        }
        else
        {
            srcOpnd = this->BuildSrcOpnd(regSlot);
        }

        if (isProfiled)
        {
            if (m_func->DoSimpleJitDynamicProfile())
            {
                instr = IR::JitProfilingInstr::New(newOpcode, fieldSymOpnd, srcOpnd, m_func);
            }
            else if (this->m_func->HasProfileInfo())
            {

                Js::ReadOnlyDynamicProfileInfo * profile = this->m_func->GetProfileInfo();
                instr = IR::ProfiledInstr::New(newOpcode, fieldSymOpnd, srcOpnd, m_func);
                instr->AsProfiledInstr()->u.FldInfo() = *(profile->GetFldInfo(this->m_func->GetJnFunction(), inlineCacheIndex));
            }
        }

        // If it hasn't been set yet
        if (!instr)
        {
            instr = IR::Instr::New(newOpcode, fieldSymOpnd, srcOpnd, m_func);
        }
        break;
    }

    default:
        AssertMsg(UNREACHED, "Unknown ElementCP opcode");
        Fatal();
    }

    this->AddInstr(instr, offset);

    if(isLdFldThatWasNotProfiled && DoBailOnNoProfile())
    {
        InsertBailOnNoProfile(instr);
    }
}


///----------------------------------------------------------------------------
///
/// IRBuilder::BuildElementC2
///
///     Build IR instr for an ElementC2 instruction.
///
///----------------------------------------------------------------------------

template <typename SizePolicy>
void
IRBuilder::BuildElementScopedC2(Js::OpCode newOpcode, uint32 offset)
{
    Assert(OpCodeAttr::HasMultiSizeLayout(newOpcode));
    auto layout = m_jnReader.GetLayout<Js::OpLayoutT_ElementScopedC2<SizePolicy>>();

    if (!PHASE_OFF(Js::ClosureRegCheckPhase, m_func))
    {
        this->DoClosureRegCheck(layout->Value);
        this->DoClosureRegCheck(layout->Value2);
    }

    BuildElementScopedC2(newOpcode, offset, layout->Value2, layout->Value, layout->PropertyIdIndex);
}

void
IRBuilder::BuildElementScopedC2(Js::OpCode newOpcode, uint32 offset, Js::RegSlot value2Slot,
                                Js::RegSlot regSlot, Js::PropertyIdIndexType propertyIdIndex)
{
    IR::Instr *     instr = nullptr;

    Js::FunctionBody * functionBody = this->m_func->GetJnFunction();
    Js::PropertyId  propertyId;
    IR::RegOpnd *   regOpnd;
    IR::RegOpnd *   value2Opnd;
    IR::SymOpnd * fieldSymOpnd;

    Js::RegSlot   instanceSlot = this->GetEnvRegForEvalCode();

    switch (newOpcode)
    {
    case Js::OpCode::ScopedLdInst:
        {
            propertyId = functionBody->GetReferencedPropertyId(propertyIdIndex);
            fieldSymOpnd = this->BuildFieldOpnd(newOpcode, instanceSlot, propertyId, propertyIdIndex, PropertyKindData);
            regOpnd = this->BuildDstOpnd(regSlot);
            value2Opnd = this->BuildDstOpnd(value2Slot);

            IR::Instr *newInstr = IR::Instr::New(Js::OpCode::Unused, value2Opnd, m_func);
            this->AddInstr(newInstr, offset);

            instr = IR::Instr::New(newOpcode, regOpnd, fieldSymOpnd, newInstr->GetDst(), m_func);
            this->AddInstr(instr, offset);
        }
        break;

    default:
        AssertMsg(UNREACHED, "Unknown ElementC2 opcode");
        Fatal();
    }
}

template <typename SizePolicy>
void
IRBuilder::BuildElementC2(Js::OpCode newOpcode, uint32 offset)
{
    Assert(OpCodeAttr::HasMultiSizeLayout(newOpcode));
    auto layout = m_jnReader.GetLayout<Js::OpLayoutT_ElementC2<SizePolicy>>();

    if (!PHASE_OFF(Js::ClosureRegCheckPhase, m_func))
    {
        this->DoClosureRegCheck(layout->Value);
        this->DoClosureRegCheck(layout->Value2);
        this->DoClosureRegCheck(layout->Instance);
    }

    BuildElementC2(newOpcode, offset, layout->Instance, layout->Value2, layout->Value, layout->PropertyIdIndex);
}

void
IRBuilder::BuildElementC2(Js::OpCode newOpcode, uint32 offset, Js::RegSlot instanceSlot, Js::RegSlot value2Slot,
                                Js::RegSlot regSlot, Js::PropertyIdIndexType propertyIdIndex)
{
    IR::Instr *     instr = nullptr;

    Js::FunctionBody * functionBody = this->m_func->GetJnFunction();
    Js::PropertyId  propertyId;
    IR::RegOpnd *   regOpnd;
    IR::RegOpnd *   value2Opnd;
    IR::SymOpnd * fieldSymOpnd;

    switch (newOpcode)
    {
    case Js::OpCode::ProfiledLdSuperFld:
        Js::OpCodeUtil::ConvertNonCallOpToNonProfiled(newOpcode);
        // fall-through

    case Js::OpCode::LdSuperFld:
        {
            propertyId = functionBody->GetPropertyIdFromCacheId(propertyIdIndex);
            fieldSymOpnd = this->BuildFieldOpnd(newOpcode, instanceSlot, propertyId, (Js::PropertyIdIndexType) - 1, PropertyKindData, propertyIdIndex);
            if (fieldSymOpnd->IsPropertySymOpnd())
            {
                fieldSymOpnd->AsPropertySymOpnd()->TryDisableRuntimePolymorphicCache();
            }

            value2Opnd = this->BuildDstOpnd(value2Slot);
            regOpnd = this->BuildDstOpnd(regSlot);

            instr = IR::Instr::New(newOpcode, regOpnd, fieldSymOpnd, value2Opnd, m_func);
            this->AddInstr(instr, offset);
        }
        break;

    case Js::OpCode::ProfiledStSuperFld:
        Js::OpCodeUtil::ConvertNonCallOpToNonProfiled(newOpcode);
        // fall-through

    case Js::OpCode::StSuperFld:
    {
        propertyId = functionBody->GetPropertyIdFromCacheId(propertyIdIndex);
        fieldSymOpnd = this->BuildFieldOpnd(newOpcode, instanceSlot, propertyId, (Js::PropertyIdIndexType) - 1, PropertyKindData, propertyIdIndex);
        if (fieldSymOpnd->IsPropertySymOpnd())
        {
            fieldSymOpnd->AsPropertySymOpnd()->TryDisableRuntimePolymorphicCache();
        }

        regOpnd = this->BuildSrcOpnd(regSlot);
        value2Opnd = this->BuildSrcOpnd(value2Slot);

        instr = IR::Instr::New(newOpcode, fieldSymOpnd, regOpnd, value2Opnd, m_func);

        this->AddInstr(instr, offset);
        break;
    }

    default:
        AssertMsg(UNREACHED, "Unknown ElementC2 opcode");
        Fatal();
    }
}

///----------------------------------------------------------------------------
///
/// IRBuilder::BuildElementU
///
///     Build IR instr for an ElementU or ElementRootU instruction.
///
///----------------------------------------------------------------------------

template <typename SizePolicy>
void
IRBuilder::BuildElementU(Js::OpCode newOpcode, uint32 offset)
{
    Assert(!OpCodeAttr::IsProfiledOp(newOpcode));
    Assert(OpCodeAttr::HasMultiSizeLayout(newOpcode));
    auto layout = m_jnReader.GetLayout<Js::OpLayoutT_ElementU<SizePolicy>>();

    if (!PHASE_OFF(Js::ClosureRegCheckPhase, m_func))
    {
        this->DoClosureRegCheck(layout->Instance);
    }

    BuildElementU(newOpcode, offset, layout->Instance, layout->PropertyIdIndex);
}

template <typename SizePolicy>
void
IRBuilder::BuildElementRootU(Js::OpCode newOpcode, uint32 offset)
{
    Assert(!OpCodeAttr::IsProfiledOp(newOpcode));
    Assert(OpCodeAttr::HasMultiSizeLayout(newOpcode));
    auto layout = m_jnReader.GetLayout<Js::OpLayoutT_ElementRootU<SizePolicy>>();
    BuildElementU(newOpcode, offset, Js::FunctionBody::RootObjectRegSlot, layout->PropertyIdIndex);
}

template <typename SizePolicy>
void
IRBuilder::BuildElementScopedU(Js::OpCode newOpcode, uint32 offset)
{
    Assert(!OpCodeAttr::IsProfiledOp(newOpcode));
    Assert(OpCodeAttr::HasMultiSizeLayout(newOpcode));
    auto layout = m_jnReader.GetLayout<Js::OpLayoutT_ElementScopedU<SizePolicy>>();
    BuildElementU(newOpcode, offset, m_func->GetJnFunction()->GetEnvReg(), layout->PropertyIdIndex);
}

void
IRBuilder::BuildElementU(Js::OpCode newOpcode, uint32 offset, Js::RegSlot instance, Js::PropertyIdIndexType propertyIdIndex)
{
    IR::Instr *     instr;
    IR::RegOpnd *   regOpnd;
    IR::SymOpnd *   fieldSymOpnd;
    Js::FunctionBody * functionBody = this->m_func->GetJnFunction();
    Js::PropertyId propertyId = functionBody->GetReferencedPropertyId(propertyIdIndex);

    switch (newOpcode)
    {
        case Js::OpCode::LdLocalElemUndef:
            if (m_func->GetLocalClosureSym()->HasByteCodeRegSlot())
            {
                IR::ByteCodeUsesInstr * byteCodeUse = IR::ByteCodeUsesInstr::New(m_func);
                byteCodeUse->byteCodeUpwardExposedUsed = JitAnew(m_func->m_alloc, BVSparse<JitArenaAllocator>, m_func->m_alloc);
                byteCodeUse->byteCodeUpwardExposedUsed->Set(m_func->GetLocalClosureSym()->m_id);
                this->AddInstr(byteCodeUse, offset);
            }

            instance = m_func->GetJnFunction()->GetLocalClosureReg();
            newOpcode = Js::OpCode::LdElemUndef;
            fieldSymOpnd = this->BuildFieldOpnd(newOpcode, instance, propertyId, propertyIdIndex, PropertyKindData);
            instr = IR::Instr::New(newOpcode, fieldSymOpnd, m_func);
            break;

            // fall through
        case Js::OpCode::LdElemUndefScoped:
        {
             // Store
            PropertyKind propertyKind = PropertyKindData;
            fieldSymOpnd = this->BuildFieldOpnd(newOpcode, instance, propertyId, propertyIdIndex, propertyKind);
            // Implicit root object as default instance
            IR::RegOpnd * regOpnd = this->BuildSrcOpnd(Js::FunctionBody::RootObjectRegSlot);

            instr = IR::Instr::New(newOpcode, fieldSymOpnd, regOpnd, m_func);
            break;
        }
        case Js::OpCode::ClearAttributes:
        {
            instr = IR::Instr::New(newOpcode, m_func);
            IR::RegOpnd * src1Opnd = this->BuildSrcOpnd(instance);
            IR::IntConstOpnd * src2Opnd = IR::IntConstOpnd::New(propertyId, TyInt32, m_func);

            instr->SetSrc1(src1Opnd);
            instr->SetSrc2(src2Opnd);
            break;
        }

        case Js::OpCode::StLocalFuncExpr:
            fieldSymOpnd = this->BuildFieldOpnd(newOpcode, m_func->GetJnFunction()->GetLocalClosureReg(), propertyId, propertyIdIndex, PropertyKindData);
            regOpnd = this->BuildSrcOpnd(instance);
            newOpcode = Js::OpCode::StFuncExpr;
            instr = IR::Instr::New(newOpcode, fieldSymOpnd, regOpnd, m_func);
            break;

        case Js::OpCode::DeleteLocalFld:
            newOpcode = Js::OpCode::DeleteFld;
            fieldSymOpnd = BuildFieldOpnd(newOpcode, m_func->GetJnFunction()->GetLocalClosureReg(), propertyId, propertyIdIndex, PropertyKindData);
            regOpnd = BuildDstOpnd(instance);
            instr = IR::Instr::New(newOpcode, regOpnd, fieldSymOpnd, m_func);
            break;
            
        default:
        {
            fieldSymOpnd = this->BuildFieldOpnd(newOpcode, instance, propertyId, propertyIdIndex, PropertyKindData);

            instr = IR::Instr::New(newOpcode, fieldSymOpnd, m_func);
            break;
        }
    }

    this->AddInstr(instr, offset);
}

///----------------------------------------------------------------------------
///
/// IRBuilder::BuildAuxiliary
///
///     Build IR instr for a Auxiliary instruction.
///
///----------------------------------------------------------------------------

void
IRBuilder::BuildAuxNoReg(Js::OpCode newOpcode, uint32 offset)
{
    Assert(!OpCodeAttr::HasMultiSizeLayout(newOpcode));

    IR::Instr * instr;
    const unaligned Js::OpLayoutAuxNoReg *auxInsn = m_jnReader.AuxNoReg();

    switch (newOpcode)
    {
        case Js::OpCode::CommitScope:
        {
            IR::RegOpnd *   src1Opnd;
            IR::Opnd*       src2Opnd;

            src1Opnd = this->BuildSrcOpnd(m_func->GetJnFunction()->GetLocalClosureReg());
            src2Opnd = IR::IntConstOpnd::New(auxInsn->Offset, TyUint32, this->m_func);

            IR::LabelInstr *labelNull = IR::LabelInstr::New(Js::OpCode::Label, this->m_func);

            IR::RegOpnd * funcExprOpnd = IR::RegOpnd::New(TyVar, m_func);
            instr = IR::Instr::New(Js::OpCode::LdFuncExpr, funcExprOpnd, m_func);
            this->AddInstr(instr, offset);

            IR::BranchInstr *branchInstr = IR::BranchInstr::New(Js::OpCode::BrFncCachedScopeNeq, labelNull,
                funcExprOpnd, src1Opnd, this->m_func);
            this->AddInstr(branchInstr, offset);

            instr = IR::Instr::New(newOpcode, this->m_func);
            instr->SetSrc1(src1Opnd);
            instr->SetSrc2(src2Opnd);

            this->AddInstr(instr, offset);

            this->AddInstr(labelNull, Js::Constants::NoByteCodeOffset);
            return;
        }

        case Js::OpCode::InitCachedFuncs:
        {
            IR::Opnd   *src1Opnd = this->BuildSrcOpnd(m_func->GetJnFunction()->GetLocalClosureReg());
            IR::Opnd   *src2Opnd = this->BuildSrcOpnd(m_func->GetJnFunction()->GetLocalFrameDisplayReg());
            IR::Opnd   *src3Opnd = this->BuildAuxArrayOpnd(AuxArrayValue::AuxFuncInfoArray, offset, auxInsn->Offset);

            instr = IR::Instr::New(Js::OpCode::ArgOut_A, IR::RegOpnd::New(TyVar, m_func), src3Opnd, m_func);
            this->AddInstr(instr, offset);

            instr = IR::Instr::New(Js::OpCode::ArgOut_A, IR::RegOpnd::New(TyVar, m_func), src2Opnd, instr->GetDst(), m_func);
            this->AddInstr(instr, Js::Constants::NoByteCodeOffset);

            instr = IR::Instr::New(Js::OpCode::ArgOut_A, IR::RegOpnd::New(TyVar, m_func), src1Opnd, instr->GetDst(), m_func);
            this->AddInstr(instr, Js::Constants::NoByteCodeOffset);

            IR::HelperCallOpnd *helperOpnd;

            helperOpnd = IR::HelperCallOpnd::New(IR::HelperOP_InitCachedFuncs, this->m_func);
            src2Opnd = instr->GetDst();

            instr = IR::Instr::New(Js::OpCode::CallHelper, m_func);
            instr->SetSrc1(helperOpnd);
            instr->SetSrc2(src2Opnd);
            this->AddInstr(instr, Js::Constants::NoByteCodeOffset);

            return;
        }

        default:
        {
            AssertMsg(UNREACHED, "Unknown AuxNoReg opcode");
            Fatal();
            break;
        }
    }    
}

void
IRBuilder::BuildAuxiliary(Js::OpCode newOpcode, uint32 offset)
{
    Assert(!OpCodeAttr::HasMultiSizeLayout(newOpcode));

    const unaligned Js::OpLayoutAuxiliary *auxInsn = m_jnReader.Auxiliary();

    if (!PHASE_OFF(Js::ClosureRegCheckPhase, m_func))
    {
        this->DoClosureRegCheck(auxInsn->R0);
    }

    IR::Instr *instr;

    switch (newOpcode)
    {
    case Js::OpCode::NewScObjectLiteral:
        {
            int literalObjectId = auxInsn->C1;

            IR::RegOpnd *   dstOpnd;
            IR::Opnd*       srcOpnd;

            Js::RegSlot     dstRegSlot = auxInsn->R0;

            // The property ID array needs to be both relocatable and available (so we can
            // get the slot capacity), so we need to just pass the offset to lower and let
            // lower take it from there...
            srcOpnd = IR::IntConstOpnd::New(auxInsn->Offset, TyUint32, m_func, true);
            dstOpnd = this->BuildDstOpnd(dstRegSlot);
            dstOpnd->SetValueType(ValueType::GetObject(ObjectType::UninitializedObject));
            instr = IR::Instr::New(newOpcode, dstOpnd, srcOpnd, m_func);

            // Because we're going to be making decisions based off the value, we have to defer
            // this until we get to lowering.
            instr->SetSrc2(IR::IntConstOpnd::New(literalObjectId, TyUint32, m_func, true));

            if (dstOpnd->m_sym->m_isSingleDef)
            {
                dstOpnd->m_sym->m_isSafeThis = true;
            }
            break;
        }

    case Js::OpCode::LdPropIds:
        {
            IR::RegOpnd *   dstOpnd;
            IR::Opnd*       srcOpnd;

            Js::RegSlot     dstRegSlot = auxInsn->R0;

            srcOpnd = this->BuildAuxArrayOpnd(AuxArrayValue::AuxPropertyIdArray, offset, auxInsn->Offset);
            dstOpnd = this->BuildDstOpnd(dstRegSlot);
            instr = IR::Instr::New(newOpcode, dstOpnd, srcOpnd, m_func);

            if (dstOpnd->m_sym->m_isSingleDef)
            {
                dstOpnd->m_sym->m_isNotInt = true;
            }

            break;
        }

    case Js::OpCode::NewScIntArray:
        {
            IR::RegOpnd*   dstOpnd;
            IR::Opnd*      src1Opnd;

            src1Opnd = this->BuildAuxArrayOpnd(AuxArrayValue::AuxIntArray, offset, auxInsn->Offset);
            dstOpnd = this->BuildDstOpnd(auxInsn->R0);

            instr = IR::Instr::New(newOpcode, dstOpnd, src1Opnd, m_func);

            const Js::TypeId arrayTypeId = m_func->IsJitInDebugMode() ? Js::TypeIds_Array : Js::TypeIds_NativeIntArray;
            dstOpnd->SetValueType(
                ValueType::GetObject(ObjectType::Array).SetHasNoMissingValues(true).SetArrayTypeId(arrayTypeId));
            dstOpnd->SetValueTypeFixed();

            break;
        }

    case Js::OpCode::NewScFltArray:
        {
            IR::RegOpnd*   dstOpnd;
            IR::Opnd*      src1Opnd;

            src1Opnd = this->BuildAuxArrayOpnd(AuxArrayValue::AuxFloatArray, offset, auxInsn->Offset);
            dstOpnd = this->BuildDstOpnd(auxInsn->R0);

            instr = IR::Instr::New(newOpcode, dstOpnd, src1Opnd, m_func);

            const Js::TypeId arrayTypeId = m_func->IsJitInDebugMode() ? Js::TypeIds_Array : Js::TypeIds_NativeFloatArray;
            dstOpnd->SetValueType(
                ValueType::GetObject(ObjectType::Array).SetHasNoMissingValues(true).SetArrayTypeId(arrayTypeId));
            dstOpnd->SetValueTypeFixed();

            break;
        }

    case Js::OpCode::StArrSegItem_A:
        {
            IR::RegOpnd*   src1Opnd;
            IR::Opnd*      src2Opnd;

            src1Opnd = this->BuildSrcOpnd(auxInsn->R0);
            src2Opnd = this->BuildAuxArrayOpnd(AuxArrayValue::AuxVarsArray, offset, auxInsn->Offset);

            instr = IR::Instr::New(newOpcode, m_func);
            instr->SetSrc1(src1Opnd);
            instr->SetSrc2(src2Opnd);

            break;
        }

    case Js::OpCode::NewScObject_A:
        {
            const Js::VarArrayVarCount *vars = Js::ByteCodeReader::ReadVarArrayVarCount(auxInsn->Offset, m_functionBody);

            int count = Js::TaggedInt::ToInt32(vars->count);

            StackSym *      symDst;
            IR::SymOpnd *   dstOpnd;
            IR::Opnd *      src1Opnd;

            //
            // PUSH all the parameters on the auxiliary context, to the stack
            //
            for (int i=0;i<count; i++)
            {
                m_argsOnStack++;

                symDst = m_func->m_symTable->GetArgSlotSym((uint16)(i + 2));
                if (symDst == nullptr || (uint16)(i + 2) != (i + 2))
                {
                    AssertMsg(UNREACHED, "Arg count too big...");
                    Fatal();
                }

                dstOpnd = IR::SymOpnd::New(symDst, TyVar, m_func);
                src1Opnd = IR::AddrOpnd::New(vars->elements[i], IR::AddrOpndKindDynamicVar, this->m_func, true);
                instr = IR::Instr::New(Js::OpCode::ArgOut_A, dstOpnd, src1Opnd, m_func);

                this->AddInstr(instr, offset);

                m_argStack->Push(instr);
            }

            BuildCallI_Helper(Js::OpCode::NewScObject, offset, (Js::RegSlot)auxInsn->R0, (Js::RegSlot)auxInsn->C1, (Js::ArgSlot)count+1, Js::Constants::NoProfileId);
            return;
        }

    default:
        {
            AssertMsg(UNREACHED, "Unknown Auxiliary opcode");
            Fatal();
            break;
        }
    }

    this->AddInstr(instr, offset);
}

void
IRBuilder::BuildProfiledAuxiliary(Js::OpCode newOpcode, uint32 offset)
{
    Assert(!OpCodeAttr::HasMultiSizeLayout(newOpcode));

    const unaligned Js::OpLayoutDynamicProfile<Js::OpLayoutAuxiliary> *auxInsn = m_jnReader.ProfiledAuxiliary();

    if (!PHASE_OFF(Js::ClosureRegCheckPhase, m_func))
    {
        this->DoClosureRegCheck(auxInsn->R0);
    }

    switch (newOpcode)
    {
    case Js::OpCode::ProfiledNewScIntArray:
        {
            Js::ProfileId profileId = static_cast<Js::ProfileId>(auxInsn->profileId);
            IR::RegOpnd*   dstOpnd;
            IR::Opnd*      src1Opnd;

            src1Opnd = this->BuildAuxArrayOpnd(AuxArrayValue::AuxIntArray, offset, auxInsn->Offset);
            dstOpnd = this->BuildDstOpnd(auxInsn->R0);

            Js::OpCodeUtil::ConvertNonCallOpToNonProfiled(newOpcode);
            IR::Instr *instr;
            Js::ArrayCallSiteInfo *arrayInfo = nullptr;
            Js::TypeId arrayTypeId = Js::TypeIds_Array;


            if (m_func->DoSimpleJitDynamicProfile())
            {
                instr = IR::JitProfilingInstr::New(newOpcode, dstOpnd, src1Opnd, m_func);
                instr->AsJitProfilingInstr()->profileId = profileId;
            }
            else if (m_func->HasArrayInfo())
            {
                instr = IR::ProfiledInstr::New(newOpcode, dstOpnd, src1Opnd, m_func);
                instr->AsProfiledInstr()->u.profileId = profileId;
                arrayInfo = m_func->GetProfileInfo()->GetArrayCallSiteInfo(m_func->GetJnFunction(), profileId);
                if (arrayInfo && !m_func->IsJitInDebugMode())
                {
                    if (arrayInfo->IsNativeIntArray())
                    {
                        arrayTypeId = Js::TypeIds_NativeIntArray;
                    }
                    else if (arrayInfo->IsNativeFloatArray())
                    {
                        arrayTypeId = Js::TypeIds_NativeFloatArray;
                    }
                }
            }
            else
            {
                instr = IR::Instr::New(newOpcode, dstOpnd, src1Opnd, m_func);
            }

            ValueType dstValueType(
                ValueType::GetObject(ObjectType::Array).SetHasNoMissingValues(true).SetArrayTypeId(arrayTypeId));
            if (dstValueType.IsLikelyNativeArray())
            {
                dstOpnd->SetValueType(dstValueType.ToLikely());
            }
            else
            {
                dstOpnd->SetValueType(dstValueType);
                dstOpnd->SetValueTypeFixed();
            }

            StackSym *dstSym = dstOpnd->AsRegOpnd()->m_sym;
            if (dstSym->m_isSingleDef)
            {
                dstSym->m_isSafeThis = true;
                dstSym->m_isNotInt = true;
            }
            this->AddInstr(instr, offset);

            break;
        }

    case Js::OpCode::ProfiledNewScFltArray:
        {
            Js::ProfileId profileId = static_cast<Js::ProfileId>(auxInsn->profileId);
            IR::RegOpnd*   dstOpnd;
            IR::Opnd*      src1Opnd;

            src1Opnd = this->BuildAuxArrayOpnd(AuxArrayValue::AuxFloatArray, offset, auxInsn->Offset);
            dstOpnd = this->BuildDstOpnd(auxInsn->R0);

            Js::OpCodeUtil::ConvertNonCallOpToNonProfiled(newOpcode);
            IR::Instr *instr;

            Js::ArrayCallSiteInfo *arrayInfo = nullptr;

            if (m_func->DoSimpleJitDynamicProfile())
            {
                instr = IR::JitProfilingInstr::New(newOpcode, dstOpnd, src1Opnd, m_func);
                instr->AsJitProfilingInstr()->profileId = profileId;
                // Keep arrayInfo null because we aren't using profile data in profiling simplejit
            }
            else
            {
                instr = IR::ProfiledInstr::New(newOpcode, dstOpnd, src1Opnd, m_func);
                instr->AsProfiledInstr()->u.profileId = profileId;
                if (m_func->HasArrayInfo()) {
                    arrayInfo = m_func->GetProfileInfo()->GetArrayCallSiteInfo(m_func->GetJnFunction(), profileId);
                }
            }

            Js::TypeId arrayTypeId;
            if (arrayInfo && arrayInfo->IsNativeFloatArray())
            {
                arrayTypeId = Js::TypeIds_NativeFloatArray;
            }
            else
            {
                arrayTypeId = Js::TypeIds_Array;
            }

            ValueType dstValueType(
                ValueType::GetObject(ObjectType::Array).SetHasNoMissingValues(true).SetArrayTypeId(arrayTypeId));
            if (dstValueType.IsLikelyNativeArray())
            {
                dstOpnd->SetValueType(dstValueType.ToLikely());
            }
            else
            {
                dstOpnd->SetValueType(dstValueType);
                dstOpnd->SetValueTypeFixed();
            }

            StackSym *dstSym = dstOpnd->AsRegOpnd()->m_sym;
            if (dstSym->m_isSingleDef)
            {
                dstSym->m_isSafeThis = true;
                dstSym->m_isNotInt = true;
            }

            this->AddInstr(instr, offset);

            break;
        }

    default:
        {
            AssertMsg(UNREACHED, "Unknown Auxiliary opcode");
            Fatal();
            break;
        }

    }
}

///----------------------------------------------------------------------------
///
/// IRBuilder::BuildReg2Aux
///
///     Build IR instr for a Reg2Aux instruction.
///
///----------------------------------------------------------------------------

void IRBuilder::BuildInitCachedScope(int auxOffset, int offset)
{
    IR::Instr *     instr;
    IR::RegOpnd *   dstOpnd;
    IR::RegOpnd *   src1Opnd;
    IR::AddrOpnd *  src2Opnd;
    IR::Opnd*       src3Opnd;
    IR::Opnd*       formalsAreLetDeclOpnd;

    src2Opnd = this->BuildAuxArrayOpnd(AuxArrayValue::AuxPropertyIdArray, offset, auxOffset, Js::ActivationObjectEx::ExtraSlotCount());
    Js::PropertyIdArray * propIds = (Js::PropertyIdArray *)src2Opnd->AsAddrOpnd()->m_address;
    src3Opnd = this->BuildAuxObjectLiteralTypeRefOpnd(Js::ActivationObjectEx::GetLiteralObjectRef(propIds), offset);
    dstOpnd = this->BuildDstOpnd(m_func->GetJnFunction()->GetLocalClosureReg());

    formalsAreLetDeclOpnd = IR::IntConstOpnd::New(propIds->hasNonSimpleParams, TyUint8, m_func);

    instr = IR::Instr::New(Js::OpCode::ArgOut_A, IR::RegOpnd::New(TyVar, m_func), formalsAreLetDeclOpnd, m_func);
    this->AddInstr(instr, offset);

    instr = IR::Instr::New(Js::OpCode::ArgOut_A, IR::RegOpnd::New(TyVar, m_func), src3Opnd, instr->GetDst(), m_func);
    this->AddInstr(instr, Js::Constants::NoByteCodeOffset);

    instr = IR::Instr::New(Js::OpCode::ArgOut_A, IR::RegOpnd::New(TyVar, m_func), src2Opnd, instr->GetDst(), m_func);
    this->AddInstr(instr, Js::Constants::NoByteCodeOffset);

    // Disable opt that normally gets disabled when we see LdFuncExpr in the byte code.
    m_func->DisableCanDoInlineArgOpt();
    src1Opnd = IR::RegOpnd::New(TyVar, m_func);
    IR::Instr * instrLdFuncExpr = IR::Instr::New(Js::OpCode::LdFuncExpr, src1Opnd, m_func);
    this->AddInstr(instrLdFuncExpr, Js::Constants::NoByteCodeOffset);

    instr = IR::Instr::New(Js::OpCode::ArgOut_A, IR::RegOpnd::New(TyVar, m_func), src1Opnd, instr->GetDst(), m_func);
    this->AddInstr(instr, Js::Constants::NoByteCodeOffset);

    IR::HelperCallOpnd *helperOpnd;

    helperOpnd = IR::HelperCallOpnd::New(IR::HelperOP_InitCachedScope, this->m_func);

    instr = IR::Instr::New(Js::OpCode::CallHelper, dstOpnd, helperOpnd, instr->GetDst(), m_func);
    this->AddInstr(instr, Js::Constants::NoByteCodeOffset);
}

void
IRBuilder::BuildReg2Aux(Js::OpCode newOpcode, uint32 offset)
{
    Assert(!OpCodeAttr::HasMultiSizeLayout(newOpcode));

    const unaligned Js::OpLayoutReg2Aux *auxInsn = m_jnReader.Reg2Aux();

    if (!PHASE_OFF(Js::ClosureRegCheckPhase, m_func))
    {
        this->DoClosureRegCheck(auxInsn->R0);
        this->DoClosureRegCheck(auxInsn->R1);
    }

    IR::Instr *instr;

    switch (newOpcode)
    {
    case Js::OpCode::InitCachedScope:
    case Js::OpCode::InitLetCachedScope:
        {
            int literalObjectId = auxInsn->C1;

            IR::RegOpnd *   dstOpnd;
            IR::RegOpnd *   src1Opnd;
            IR::Opnd*       src2Opnd;
            IR::Opnd*       src3Opnd;
            IR::Opnd*       formalsAreLetDeclOpnd;

            Js::RegSlot     dstRegSlot = auxInsn->R0;
            Js::RegSlot     srcRegSlot = auxInsn->R1;

            src1Opnd = this->BuildSrcOpnd(srcRegSlot);
            Assert(this->m_func->GetFuncObjSym() == nullptr);
            this->m_func->SetFuncObjSym(src1Opnd->m_sym);

            src2Opnd = this->BuildAuxArrayOpnd(AuxArrayValue::AuxPropertyIdArray, offset, auxInsn->Offset, 3);
            src3Opnd = this->BuildAuxObjectLiteralTypeRefOpnd(literalObjectId, offset);
            dstOpnd = this->BuildDstOpnd(dstRegSlot);

            formalsAreLetDeclOpnd = IR::IntConstOpnd::New((IntConstType) (newOpcode == Js::OpCode::InitLetCachedScope), TyUint8, m_func);

            instr = IR::Instr::New(Js::OpCode::ArgOut_A, IR::RegOpnd::New(TyVar, m_func), formalsAreLetDeclOpnd, m_func);
            this->AddInstr(instr, offset);

            instr = IR::Instr::New(Js::OpCode::ArgOut_A, IR::RegOpnd::New(TyVar, m_func), src3Opnd, instr->GetDst(), m_func);
            this->AddInstr(instr, Js::Constants::NoByteCodeOffset);

            instr = IR::Instr::New(Js::OpCode::ArgOut_A, IR::RegOpnd::New(TyVar, m_func), src2Opnd, instr->GetDst(), m_func);
            this->AddInstr(instr, Js::Constants::NoByteCodeOffset);

            instr = IR::Instr::New(Js::OpCode::ArgOut_A, IR::RegOpnd::New(TyVar, m_func), src1Opnd, instr->GetDst(), m_func);
            this->AddInstr(instr, Js::Constants::NoByteCodeOffset);

            IR::HelperCallOpnd *helperOpnd;

            helperOpnd = IR::HelperCallOpnd::New(IR::HelperOP_InitCachedScope, this->m_func);

            instr = IR::Instr::New(Js::OpCode::CallHelper, dstOpnd, helperOpnd, instr->GetDst(), m_func);
            this->AddInstr(instr, Js::Constants::NoByteCodeOffset);

            if (dstOpnd->m_sym->m_isSingleDef)
            {
                dstOpnd->m_sym->m_isNotInt = true;
            }
            break;
        }

    case Js::OpCode::SpreadArrayLiteral:
        {
            IR::RegOpnd *   dstOpnd;
            IR::RegOpnd *   src1Opnd;
            IR::Opnd*       src2Opnd;

            Js::RegSlot     dstRegSlot = auxInsn->R0;
            Js::RegSlot     srcRegSlot = auxInsn->R1;

            src1Opnd = this->BuildSrcOpnd(srcRegSlot);

            src2Opnd = this->BuildAuxArrayOpnd(AuxArrayValue::AuxIntArray, offset, auxInsn->Offset);
            dstOpnd = this->BuildDstOpnd(dstRegSlot);

            instr = IR::Instr::New(Js::OpCode::SpreadArrayLiteral, dstOpnd, src1Opnd, src2Opnd, m_func);
            this->AddInstr(instr, Js::Constants::NoByteCodeOffset);

            if (dstOpnd->m_sym->m_isSingleDef)
            {
                dstOpnd->m_sym->m_isNotInt = true;
            }
            break;
        }

    default:
        {
            AssertMsg(UNREACHED, "Unknown Reg2Aux opcode");
            Fatal();
            break;
        }
    }
}

///----------------------------------------------------------------------------
///
/// IRBuilder::BuildElementI
///
///     Build IR instr for an ElementI instruction.
///
///----------------------------------------------------------------------------


template <typename SizePolicy>
void
IRBuilder::BuildElementI(Js::OpCode newOpcode, uint32 offset)
{
    Assert(!OpCodeAttr::IsProfiledOp(newOpcode));
    Assert(OpCodeAttr::HasMultiSizeLayout(newOpcode));
    auto layout = m_jnReader.GetLayout<Js::OpLayoutT_ElementI<SizePolicy>>();

    if (!PHASE_OFF(Js::ClosureRegCheckPhase, m_func))
    {
        this->DoClosureRegCheck(layout->Value);
        this->DoClosureRegCheck(layout->Instance);
        this->DoClosureRegCheck(layout->Element);
    }

    BuildElementI(newOpcode, offset, layout->Instance, layout->Element, layout->Value, Js::Constants::NoProfileId);
}


template <typename SizePolicy>
void
IRBuilder::BuildProfiledElementI(Js::OpCode newOpcode, uint32 offset)
{
    Assert(OpCodeAttr::IsProfiledOp(newOpcode));
    Assert(OpCodeAttr::HasMultiSizeLayout(newOpcode));
    auto layout = m_jnReader.GetLayout<Js::OpLayoutDynamicProfile<Js::OpLayoutT_ElementI<SizePolicy>>>();

    if (!PHASE_OFF(Js::ClosureRegCheckPhase, m_func))
    {
        this->DoClosureRegCheck(layout->Value);
        this->DoClosureRegCheck(layout->Instance);
        this->DoClosureRegCheck(layout->Element);
    }

    Js::OpCodeUtil::ConvertNonCallOpToNonProfiled(newOpcode);
    BuildElementI(newOpcode, offset, layout->Instance, layout->Element, layout->Value, layout->profileId);
}

void
IRBuilder::BuildElementI(Js::OpCode newOpcode, uint32 offset, Js::RegSlot baseRegSlot, Js::RegSlot indexRegSlot,
                        Js::RegSlot regSlot, Js::ProfileId profileId)
{
    Assert(OpCodeAttr::HasMultiSizeLayout(newOpcode));

    ValueType arrayType;
    const Js::LdElemInfo *ldElemInfo = nullptr;
    const Js::StElemInfo *stElemInfo = nullptr;
    bool isProfiledLoad = false;
    bool isProfiledStore = false;
    bool isProfiledInstr = (profileId != Js::Constants::NoProfileId);
    bool isLdElemOrStElemThatWasNotProfiled = false;

    if (isProfiledInstr)
    {
        switch (newOpcode)
        {
        case Js::OpCode::LdElemI_A:
            if (!this->m_func->HasProfileInfo() ||
                (
                    PHASE_OFF(Js::TypedArrayPhase, this->m_func->GetTopFunc()) &&
                    PHASE_OFF(Js::ArrayCheckHoistPhase, this->m_func->GetJnFunction())
                ))
            {
                break;
            }
            ldElemInfo = this->m_func->GetProfileInfo()->GetLdElemInfo(m_func->GetJnFunction(), profileId);
            arrayType = ldElemInfo->GetArrayType();
            isLdElemOrStElemThatWasNotProfiled = !ldElemInfo->WasProfiled();
            isProfiledLoad = true;
            break;

        case Js::OpCode::StElemI_A:
        case Js::OpCode::StElemI_A_Strict:
            if (!this->m_func->HasProfileInfo() ||
                (
                    PHASE_OFF(Js::TypedArrayPhase, this->m_func->GetTopFunc()) &&
                    PHASE_OFF(Js::ArrayCheckHoistPhase, this->m_func->GetJnFunction())
                ))
            {
                break;
            }
            isProfiledStore = true;
            stElemInfo = this->m_func->GetProfileInfo()->GetStElemInfo(this->m_func->GetJnFunction(), profileId);
            arrayType = stElemInfo->GetArrayType();
            isLdElemOrStElemThatWasNotProfiled = !stElemInfo->WasProfiled();
            break;
        }
    }

    IR::Instr *     instr;
    IR::RegOpnd *   regOpnd;

    IR::IndirOpnd * indirOpnd;
    indirOpnd = this->BuildIndirOpnd(this->BuildSrcOpnd(baseRegSlot), this->BuildSrcOpnd(indexRegSlot));

    if (isProfiledLoad || isProfiledStore)
    {
        if(arrayType.IsLikelyNativeArray() &&
            (
                !(m_func->GetTopFunc()->HasTry() && !m_func->GetTopFunc()->DoOptimizeTryCatch()) && m_func->GetWeakFuncRef() && !m_func->HasArrayInfo() ||
                m_func->IsJitInDebugMode()
            ))
        {
            arrayType = arrayType.SetArrayTypeId(Js::TypeIds_Array);

            // An opnd's value type will get replaced in the forward phase when it is not fixed. Store the array type in the
            // ProfiledInstr.
            if(isProfiledLoad)
            {
                Js::LdElemInfo *const newLdElemInfo = JitAnew(m_func->m_alloc, Js::LdElemInfo, *ldElemInfo);
                newLdElemInfo->arrayType = arrayType;
                ldElemInfo = newLdElemInfo;
            }
            else
            {
                Js::StElemInfo *const newStElemInfo = JitAnew(m_func->m_alloc, Js::StElemInfo, *stElemInfo);
                newStElemInfo->arrayType = arrayType;
                stElemInfo = newStElemInfo;
            }
        }
        indirOpnd->GetBaseOpnd()->SetValueType(arrayType);

        if (m_func->GetTopFunc()->HasTry() && !m_func->GetTopFunc()->DoOptimizeTryCatch())
        {
            isProfiledLoad = false;
            isProfiledStore = false;
        }
    }

    switch (newOpcode)
    {
    case Js::OpCode::LdMethodElem:
    case Js::OpCode::LdElemI_A:
    case Js::OpCode::DeleteElemI_A:
    case Js::OpCode::DeleteElemIStrict_A:
    case Js::OpCode::TypeofElem:
        {
            // Evaluate to register

            regOpnd = this->BuildDstOpnd(regSlot);

            if (m_func->DoSimpleJitDynamicProfile() && isProfiledInstr)
            {
                instr = IR::JitProfilingInstr::New(newOpcode, regOpnd, indirOpnd, m_func);
                instr->AsJitProfilingInstr()->profileId = profileId;
            }
            else if (isProfiledLoad)
            {
                instr = IR::ProfiledInstr::New(newOpcode, regOpnd, indirOpnd, m_func);
                instr->AsProfiledInstr()->u.ldElemInfo = ldElemInfo;
            }
            else
            {
                instr = IR::Instr::New(newOpcode, regOpnd, indirOpnd, m_func);
            }
            break;
        }

    case Js::OpCode::StElemI_A:
    case Js::OpCode::StElemI_A_Strict:
        {
            // Store

            regOpnd = this->BuildSrcOpnd(regSlot);

            if (m_func->DoSimpleJitDynamicProfile() && isProfiledInstr)
            {
                instr = IR::JitProfilingInstr::New(newOpcode, indirOpnd, regOpnd, m_func);
                instr->AsJitProfilingInstr()->profileId = profileId;
            }
            else if (isProfiledStore)
            {
                instr = IR::ProfiledInstr::New(newOpcode, indirOpnd, regOpnd, m_func);
                instr->AsProfiledInstr()->u.stElemInfo = stElemInfo;
            }
            else
            {
                instr = IR::Instr::New(newOpcode, indirOpnd, regOpnd, m_func);
            }
            break;
        }

    case Js::OpCode::InitSetElemI:
    case Js::OpCode::InitGetElemI:
    case Js::OpCode::InitComputedProperty:
    case Js::OpCode::InitClassMemberComputedName:
    case Js::OpCode::InitClassMemberGetComputedName:
    case Js::OpCode::InitClassMemberSetComputedName:
        {

            regOpnd = this->BuildSrcOpnd(regSlot);

            instr = IR::Instr::New(newOpcode, indirOpnd, regOpnd, m_func);
            break;
        }

    default:
        AssertMsg(false, "Unknown ElementI opcode");
        return;

    }

    this->AddInstr(instr, offset);

    if(isLdElemOrStElemThatWasNotProfiled && DoBailOnNoProfile())
    {
        InsertBailOnNoProfile(instr);
    }
}

///----------------------------------------------------------------------------
///
/// IRBuilder::BuildElementUnsigned1
///
///     Build IR instr for an ElementUnsigned1 instruction.
///
///----------------------------------------------------------------------------


template <typename SizePolicy>
void
IRBuilder::BuildElementUnsigned1(Js::OpCode newOpcode, uint32 offset)
{
    Assert(!OpCodeAttr::IsProfiledOp(newOpcode));
    Assert(OpCodeAttr::HasMultiSizeLayout(newOpcode));
    auto layout = m_jnReader.GetLayout<Js::OpLayoutT_ElementUnsigned1<SizePolicy>>();

    if (!PHASE_OFF(Js::ClosureRegCheckPhase, m_func))
    {
        this->DoClosureRegCheck(layout->Value);
        this->DoClosureRegCheck(layout->Instance);
    }

    BuildElementUnsigned1(newOpcode, offset, layout->Instance, layout->Element, layout->Value);
}

void
IRBuilder::BuildElementUnsigned1(Js::OpCode newOpcode, uint32 offset, Js::RegSlot baseRegSlot, uint32 index, Js::RegSlot regSlot)
{
    // This is an array-style access with a constant (integer) index.
    // Embed the index in the indir opnd as a constant offset.

    IR::Instr *     instr;

    const bool simpleJit = m_func->DoSimpleJitDynamicProfile();

    IR::RegOpnd *   regOpnd;
    IR::IndirOpnd * indirOpnd;
    IR::RegOpnd * baseOpnd;
    Js::OpCode opcode;
    switch (newOpcode)
    {
    case Js::OpCode::StArrItemI_CI4:
        {
            baseOpnd = this->BuildSrcOpnd(baseRegSlot);

            // This instruction must not create missing values in the array
            baseOpnd->SetValueType(
                ValueType::GetObject(ObjectType::Array).SetHasNoMissingValues(false).SetArrayTypeId(Js::TypeIds_Array));
            baseOpnd->SetValueTypeFixed();

            // In the case of simplejit, we won't know the exact type of array used until run time. Due to this,
            //    we must use the specialized version of StElemC in Lowering.
            opcode = simpleJit ? Js::OpCode::StElemC : Js::OpCode::StElemI_A;
            break;
        }

    case Js::OpCode::StArrItemC_CI4:
        {
            baseOpnd = IR::RegOpnd::New(TyVar, m_func);
            // Insert LdArrHead as the next instr and clear the offset to avoid duplication.
            IR::RegOpnd *const arrayOpnd = this->BuildSrcOpnd(baseRegSlot);

            // This instruction must not create missing values in the array
            arrayOpnd->SetValueType(
                ValueType::GetObject(ObjectType::Array).SetHasNoMissingValues(false).SetArrayTypeId(Js::TypeIds_Array));
            arrayOpnd->SetValueTypeFixed();

            this->AddInstr(IR::Instr::New(Js::OpCode::LdArrHead, baseOpnd, arrayOpnd, m_func), offset);
            offset = Js::Constants::NoByteCodeOffset;
            opcode = Js::OpCode::StArrSegElemC;
            break;
        }
    case Js::OpCode::StArrSegItem_CI4:
        {
            baseOpnd = this->BuildSrcOpnd(baseRegSlot, TyVar);

            // This instruction must not create missing values in the array

            opcode = Js::OpCode::StArrSegElemC;
            break;
        }
    case Js::OpCode::StArrInlineItem_CI4:
        {
            baseOpnd = this->BuildSrcOpnd(baseRegSlot);

            IR::Opnd *defOpnd = baseOpnd->m_sym->m_instrDef ? baseOpnd->m_sym->m_instrDef->GetDst() : nullptr;
            if (!defOpnd)
            {
                // The array sym may be multi-def because of oddness in the renumbering of temps -- for instance,
                // if there's a loop increment expression whose result is unused (ExprGen only, probably).
                FOREACH_INSTR_BACKWARD(tmpInstr, m_func->m_exitInstr->m_prev)
                {
                    if (tmpInstr->GetDst())
                    {
                        if (tmpInstr->GetDst()->IsEqual(baseOpnd))
                        {
                            defOpnd = tmpInstr->GetDst();
                            break;
                        }
                        else if (tmpInstr->m_opcode == Js::OpCode::StElemC &&
                                 tmpInstr->GetDst()->AsIndirOpnd()->GetBaseOpnd()->IsEqual(baseOpnd))
                        {
                            defOpnd = tmpInstr->GetDst()->AsIndirOpnd()->GetBaseOpnd();
                            break;
                        }
                    }
                }
                NEXT_INSTR_BACKWARD;
            }
            AnalysisAssert(defOpnd);

            // This instruction must not create missing values in the array
            baseOpnd->SetValueType(defOpnd->GetValueType());

            opcode = Js::OpCode::StElemC;
            break;
        }
    default:
        AssertMsg(false, "Unknown ElementUnsigned1 opcode");
        return;

    }

    indirOpnd = this->BuildIndirOpnd(baseOpnd, index);
    regOpnd = this->BuildSrcOpnd(regSlot);
    if (simpleJit)
    {
        instr = IR::JitProfilingInstr::New(opcode, indirOpnd, regOpnd, m_func);
    }
    else if(opcode == Js::OpCode::StElemC && !baseOpnd->GetValueType().IsUninitialized())
    {
        // An opnd's value type will get replaced in the forward phase when it is not fixed. Store the array type in the
        // ProfiledInstr.
        IR::ProfiledInstr *const profiledInstr = IR::ProfiledInstr::New(opcode, indirOpnd, regOpnd, m_func);
        Js::StElemInfo *const stElemInfo = JitAnew(m_func->m_alloc, Js::StElemInfo);
        stElemInfo->arrayType = baseOpnd->GetValueType();
        profiledInstr->u.stElemInfo = stElemInfo;
        instr = profiledInstr;
    }
    else
    {
        instr = IR::Instr::New(opcode, indirOpnd, regOpnd, m_func);
    }

    this->AddInstr(instr, offset);
}

///----------------------------------------------------------------------------
///
/// IRBuilder::BuildArgIn
///
///     Build IR instr for an ArgIn instruction.
///
///----------------------------------------------------------------------------

void
IRBuilder::BuildArgIn0(uint32 offset, Js::RegSlot dstRegSlot)
{
    Assert(OpCodeAttr::HasMultiSizeLayout(Js::OpCode::ArgIn0));
    BuildArgIn(offset, dstRegSlot, 0);
}

void
IRBuilder::BuildArgIn(uint32 offset, Js::RegSlot dstRegSlot, uint16 argument)
{
    IR::Instr *     instr;
    IR::SymOpnd *   srcOpnd;
    IR::RegOpnd *   dstOpnd;
    StackSym *      symSrc = StackSym::NewParamSlotSym(argument + 1, m_func);

    this->m_func->SetArgOffset(symSrc, (argument + LowererMD::GetFormalParamOffset()) * MachPtr);

    srcOpnd = IR::SymOpnd::New(symSrc, TyVar, m_func);
    dstOpnd = this->BuildDstOpnd(dstRegSlot);

    if (!this->m_func->IsLoopBody() && this->m_func->HasProfileInfo())
    {
        // Skip "this" pointer; "this" profile data is captured by ProfiledLdThis.
        // Subtract 1 to skip "this" pointer, subtract 1 again to get the index to index into profileData->parameterInfo.
        int paramSlotIndex = symSrc->GetParamSlotNum() - 2;
        if (paramSlotIndex >= 0)
        {
            ValueType profiledValueType;
            profiledValueType = this->m_func->GetProfileInfo()->GetParameterInfo(
                this->m_func->GetJnFunction(), static_cast<Js::ArgSlot>(paramSlotIndex));
            dstOpnd->SetValueType(profiledValueType);
        }
    }

    instr = IR::Instr::New(Js::OpCode::ArgIn_A, dstOpnd, srcOpnd, m_func);
    this->AddInstr(instr, offset);
}

void
IRBuilder::BuildArgInRest()
{
    Js::FunctionBody * functionBody = this->m_func->GetJnFunction();
    IR::RegOpnd * dstOpnd = this->BuildDstOpnd(functionBody->GetRestParamRegSlot());
    IR::Instr *instr = IR::Instr::New(Js::OpCode::ArgIn_Rest, dstOpnd, m_func);
    this->AddInstr(instr, (uint32)-1);
}

///----------------------------------------------------------------------------
///
/// IRBuilder::BuildArg
///
///     Build IR instr for an ArgOut instruction.
///
///----------------------------------------------------------------------------

template <typename SizePolicy>
void
IRBuilder::BuildArgNoSrc(Js::OpCode newOpcode, uint32 offset)
{
    Assert(!OpCodeAttr::IsProfiledOp(newOpcode));
    Assert(OpCodeAttr::HasMultiSizeLayout(newOpcode));
    auto layout = m_jnReader.GetLayout<Js::OpLayoutT_ArgNoSrc<SizePolicy>>();
    BuildArg(Js::OpCode::ArgOut_A, offset, layout->Arg, this->GetEnvRegForInnerFrameDisplay());
}

template <typename SizePolicy>
void
IRBuilder::BuildArg(Js::OpCode newOpcode, uint32 offset)
{
    Assert(!OpCodeAttr::IsProfiledOp(newOpcode));
    Assert(OpCodeAttr::HasMultiSizeLayout(newOpcode));
    auto layout = m_jnReader.GetLayout<Js::OpLayoutT_Arg<SizePolicy>>();

    if (!PHASE_OFF(Js::ClosureRegCheckPhase, m_func))
    {
        this->DoClosureRegCheck(layout->Reg);
    }

    BuildArg(newOpcode, offset, layout->Arg, layout->Reg);
}


template <typename SizePolicy>
void
IRBuilder::BuildProfiledArg(Js::OpCode newOpcode, uint32 offset)
{
    Assert(OpCodeAttr::IsProfiledOp(newOpcode));
    Assert(OpCodeAttr::HasMultiSizeLayout(newOpcode));
    auto layout = m_jnReader.GetLayout<Js::OpLayoutDynamicProfile<Js::OpLayoutT_Arg<SizePolicy>>>();

    if (!PHASE_OFF(Js::ClosureRegCheckPhase, m_func))
    {
        this->DoClosureRegCheck(layout->Reg);
    }

    newOpcode = Js::OpCode::ArgOut_A;
    BuildArg(newOpcode, offset, layout->Arg, layout->Reg);
}

void
IRBuilder::BuildArg(Js::OpCode newOpcode, uint32 offset, Js::ArgSlot argument, Js::RegSlot srcRegSlot)
{
    Assert(OpCodeAttr::HasMultiSizeLayout(newOpcode));

    IR::Instr *     instr;
    IRType type = TyVar;
    if (newOpcode == Js::OpCode::ArgOut_ANonVar)
    {
        newOpcode = Js::OpCode::ArgOut_A;
        type = TyMachPtr;
    }
    m_argsOnStack++;
    StackSym *      symDst;

    Assert(argument < USHRT_MAX);
    symDst = m_func->m_symTable->GetArgSlotSym((uint16)(argument+1));
    if (symDst == nullptr || (uint16)(argument + 1) != (argument + 1))
    {
        AssertMsg(UNREACHED, "Arg count too big...");
        Fatal();
    }

    IR::SymOpnd * dstOpnd = IR::SymOpnd::New(symDst, type, m_func);
    IR::RegOpnd *  src1Opnd = this->BuildSrcOpnd(srcRegSlot, type);
    instr = IR::Instr::New(newOpcode, dstOpnd, src1Opnd, m_func);

    this->AddInstr(instr, offset);

    m_argStack->Push(instr);
}

///----------------------------------------------------------------------------
///
/// IRBuilder::BuildStartCall
///
///     Build IR instr for a StartCall instruction.
///
///----------------------------------------------------------------------------

void
IRBuilder::BuildStartCall(Js::OpCode newOpcode, uint32 offset)
{
    Assert(newOpcode == Js::OpCode::StartCall);

    const unaligned Js::OpLayoutStartCall * regLayout = m_jnReader.StartCall();
    Js::ArgSlot ArgCount = regLayout->ArgCount;

    IR::Instr *     instr;
    IR::RegOpnd *   dstOpnd;

    // Dst of StartCall is always r0...  Let's give it a new dst such that it can
    // be singleDef.

    dstOpnd = IR::RegOpnd::New(TyVar, m_func);

#if DBG
    m_callsOnStack++;
#endif

    IntConstType    value = ArgCount;
    IR::IntConstOpnd * srcOpnd;

    srcOpnd = IR::IntConstOpnd::New(value, TyInt32, m_func);
    instr = IR::Instr::New(newOpcode, dstOpnd, srcOpnd, m_func);

    this->AddInstr(instr, offset);

    // Keep a stack of arg instructions such that we can link them up once we see
    // the call that consumes them.

    m_argStack->Push(instr);
}

///----------------------------------------------------------------------------
///
/// IRBuilder::BuildCallI
///
///     Build IR instr for a CallI instruction.
///
///----------------------------------------------------------------------------


template <typename SizePolicy>
void
IRBuilder::BuildCallI(Js::OpCode newOpcode, uint32 offset)
{
    this->m_func->m_isLeaf = false;
    Assert(Js::OpCodeUtil::IsCallOp(newOpcode) || newOpcode == Js::OpCode::NewScObject || newOpcode == Js::OpCode::NewScObjArray);
    Assert(OpCodeAttr::HasMultiSizeLayout(newOpcode));
    auto layout = m_jnReader.GetLayout<Js::OpLayoutT_CallI<SizePolicy>>();

    if (!PHASE_OFF(Js::ClosureRegCheckPhase, m_func))
    {
        this->DoClosureRegCheck(layout->Return);
        this->DoClosureRegCheck(layout->Function);
    }

    BuildCallI_Helper(newOpcode, offset, layout->Return, layout->Function, layout->ArgCount, Js::Constants::NoProfileId);
}

template <typename SizePolicy>
void
IRBuilder::BuildCallIFlags(Js::OpCode newOpcode, uint32 offset)
{
    this->m_func->m_isLeaf = false;
    Assert(Js::OpCodeUtil::IsCallOp(newOpcode) || newOpcode == Js::OpCode::NewScObject || newOpcode == Js::OpCode::NewScObjArray);
    Assert(OpCodeAttr::HasMultiSizeLayout(newOpcode));
    auto layout = m_jnReader.GetLayout<Js::OpLayoutT_CallIFlags<SizePolicy>>();

    if (!PHASE_OFF(Js::ClosureRegCheckPhase, m_func))
    {
        this->DoClosureRegCheck(layout->Return);
        this->DoClosureRegCheck(layout->Function);
    }

    IR::Instr* instr = BuildCallI_Helper(newOpcode, offset, layout->Return, layout->Function, layout->ArgCount, Js::Constants::NoProfileId);
    Assert(instr->m_opcode == Js::OpCode::CallIFlags);
    if (instr->m_opcode == Js::OpCode::CallIFlags)
    {
        instr->m_opcode =
            layout->callFlags == Js::CallFlags::CallFlags_NewTarget ? Js::OpCode::CallIPut :
            layout->callFlags == (Js::CallFlags::CallFlags_NewTarget | Js::CallFlags::CallFlags_New | Js::CallFlags::CallFlags_ExtraArg) ? Js::OpCode::CallINewTargetNew :
            layout->callFlags == Js::CallFlags::CallFlags_New ? Js::OpCode::CallINew :
            instr->m_opcode;
    }
}

void IRBuilder::BuildLdSpreadIndices(uint32 offset, uint32 spreadAuxOffset)
{
    // Link up the LdSpreadIndices instr to be the first in the arg chain. This will allow us to find it in Lowerer easier.
    IR::Opnd *auxArg = this->BuildAuxArrayOpnd(AuxArrayValue::AuxIntArray, offset, spreadAuxOffset);
    IR::Instr *instr = IR::Instr::New(Js::OpCode::LdSpreadIndices, m_func);
    instr->SetSrc1(auxArg);

    // Create the link to the first arg.
    Js::RegSlot lastArg = m_argStack->Head()->GetDst()->AsSymOpnd()->GetStackSym()->GetArgSlotNum();
    instr->SetDst(IR::SymOpnd::New(m_func->m_symTable->GetArgSlotSym((uint16) (lastArg + 1)), TyVar, m_func));
    this->AddInstr(instr, Js::Constants::NoByteCodeOffset);

    m_argStack->Push(instr);
}

template <typename SizePolicy>
void
IRBuilder::BuildCallIExtended(Js::OpCode newOpcode, uint32 offset)
{
    this->m_func->m_isLeaf = false;
    Assert(OpCodeAttr::HasMultiSizeLayout(newOpcode));
    auto layout = m_jnReader.GetLayout<Js::OpLayoutT_CallIExtended<SizePolicy>>();

    if (!PHASE_OFF(Js::ClosureRegCheckPhase, m_func))
    {
        this->DoClosureRegCheck(layout->Return);
        this->DoClosureRegCheck(layout->Function);
    }

    BuildCallIExtended(newOpcode, offset, layout->Return, layout->Function, layout->ArgCount, layout->Options, layout->SpreadAuxOffset);
}

void
IRBuilder::BuildCallIExtended(Js::OpCode newOpcode, uint32 offset, Js::RegSlot returnValue, Js::RegSlot function,
                               Js::ArgSlot argCount, Js::CallIExtendedOptions options, uint32 spreadAuxOffset)
{
    if (options & Js::CallIExtended_SpreadArgs)
    {
        BuildLdSpreadIndices(offset, spreadAuxOffset);
    }
    BuildCallI_Helper(newOpcode, offset, returnValue, function, argCount, Js::Constants::NoProfileId);
}

template <typename SizePolicy>
void
IRBuilder::BuildCallIWithICIndex(Js::OpCode newOpcode, uint32 offset)
{
    AssertMsg(false, "NYI");
}

template <typename SizePolicy>
void
IRBuilder::BuildCallIFlagsWithICIndex(Js::OpCode newOpcode, uint32 offset)
{
    AssertMsg(false, "NYI");
}

template <typename SizePolicy>
void
IRBuilder::BuildCallIExtendedFlags(Js::OpCode newOpcode, uint32 offset)
{
    AssertMsg(false, "NYI");
}

template <typename SizePolicy>
void
IRBuilder::BuildCallIExtendedWithICIndex(Js::OpCode newOpcode, uint32 offset)
{
    AssertMsg(false, "NYI");
}

template <typename SizePolicy>
void
IRBuilder::BuildCallIExtendedFlagsWithICIndex(Js::OpCode newOpcode, uint32 offset)
{
    AssertMsg(false, "NYI");
}

template <typename SizePolicy>
void
IRBuilder::BuildProfiledCallIFlagsWithICIndex(Js::OpCode newOpcode, uint32 offset)
{
    this->m_func->m_isLeaf = false;
    Assert(OpCodeAttr::IsProfiledOpWithICIndex(newOpcode) || Js::OpCodeUtil::IsProfiledCallOpWithICIndex(newOpcode));
    Assert(OpCodeAttr::HasMultiSizeLayout(newOpcode));
    auto layout = m_jnReader.GetLayout<Js::OpLayoutDynamicProfile<Js::OpLayoutT_CallIFlagsWithICIndex<SizePolicy>>>();

    if (!PHASE_OFF(Js::ClosureRegCheckPhase, m_func))
    {
        this->DoClosureRegCheck(layout->Return);
        this->DoClosureRegCheck(layout->Function);
    }

    IR::Instr* instr = BuildProfiledCallIWithICIndex(newOpcode, offset, layout->Return, layout->Function, layout->ArgCount, layout->profileId, layout->inlineCacheIndex);
    Assert(instr->m_opcode == Js::OpCode::CallIFlags);
    if (instr->m_opcode == Js::OpCode::CallIFlags)
    {
        instr->m_opcode =
            layout->callFlags == Js::CallFlags::CallFlags_NewTarget ? Js::OpCode::CallIPut :
            layout->callFlags == (Js::CallFlags::CallFlags_NewTarget | Js::CallFlags::CallFlags_New | Js::CallFlags::CallFlags_ExtraArg) ? Js::OpCode::CallINewTargetNew :
            layout->callFlags == Js::CallFlags::CallFlags_New ? Js::OpCode::CallINew :
            instr->m_opcode;
    }
}

template <typename SizePolicy>
void
IRBuilder::BuildProfiledCallIWithICIndex(Js::OpCode newOpcode, uint32 offset)
{
    this->m_func->m_isLeaf = false;
    Assert(OpCodeAttr::IsProfiledOpWithICIndex(newOpcode) || Js::OpCodeUtil::IsProfiledCallOpWithICIndex(newOpcode));
    Assert(OpCodeAttr::HasMultiSizeLayout(newOpcode));
    auto layout = m_jnReader.GetLayout<Js::OpLayoutDynamicProfile<Js::OpLayoutT_CallIWithICIndex<SizePolicy>>>();

    if (!PHASE_OFF(Js::ClosureRegCheckPhase, m_func))
    {
        this->DoClosureRegCheck(layout->Return);
        this->DoClosureRegCheck(layout->Function);
    }

    BuildProfiledCallIWithICIndex(newOpcode, offset, layout->Return, layout->Function, layout->ArgCount, layout->profileId, layout->inlineCacheIndex);
}

IR::Instr*
IRBuilder::BuildProfiledCallIWithICIndex(Js::OpCode opcode, uint32 offset, Js::RegSlot returnValue, Js::RegSlot function,
                            Js::ArgSlot argCount, Js::ProfileId profileId, Js::InlineCacheIndex inlineCacheIndex)
{
    return BuildProfiledCallI(opcode, offset, returnValue, function, argCount, profileId, inlineCacheIndex);
}

template <typename SizePolicy>
void
IRBuilder::BuildProfiledCallIExtendedFlags(Js::OpCode newOpcode, uint32 offset)
{
    this->m_func->m_isLeaf = false;
    Assert(OpCodeAttr::IsProfiledOp(newOpcode) || Js::OpCodeUtil::IsProfiledCallOp(newOpcode)
        || Js::OpCodeUtil::IsProfiledReturnTypeCallOp(newOpcode));
    Assert(OpCodeAttr::HasMultiSizeLayout(newOpcode));
    auto layout = m_jnReader.GetLayout<Js::OpLayoutDynamicProfile<Js::OpLayoutT_CallIExtendedFlags<SizePolicy>>>();

    if (!PHASE_OFF(Js::ClosureRegCheckPhase, m_func))
    {
        this->DoClosureRegCheck(layout->Return);
        this->DoClosureRegCheck(layout->Function);
    }

    IR::Instr* instr = BuildProfiledCallIExtended(newOpcode, offset, layout->Return, layout->Function, layout->ArgCount, layout->profileId, layout->Options, layout->SpreadAuxOffset);
    Assert(instr->m_opcode == Js::OpCode::CallIExtendedFlags);
    if (instr->m_opcode == Js::OpCode::CallIExtendedFlags)
    {
        instr->m_opcode =
            layout->callFlags == Js::CallFlags::CallFlags_ExtraArg ? Js::OpCode::CallIEval :
            layout->callFlags == Js::CallFlags::CallFlags_New ? Js::OpCode::CallIExtendedNew :
            layout->callFlags == (Js::CallFlags::CallFlags_New | Js::CallFlags::CallFlags_ExtraArg | Js::CallFlags::CallFlags_NewTarget) ? Js::OpCode::CallIExtendedNewTargetNew :
            instr->m_opcode;
    }
}

template <typename SizePolicy>
void
IRBuilder::BuildProfiledCallIExtendedWithICIndex(Js::OpCode newOpcode, uint32 offset)
{
    this->m_func->m_isLeaf = false;
    Assert(OpCodeAttr::IsProfiledOpWithICIndex(newOpcode) || Js::OpCodeUtil::IsProfiledCallOpWithICIndex(newOpcode));
    Assert(OpCodeAttr::HasMultiSizeLayout(newOpcode));
    auto layout = m_jnReader.GetLayout<Js::OpLayoutDynamicProfile<Js::OpLayoutT_CallIExtendedWithICIndex<SizePolicy>>>();

    if (!PHASE_OFF(Js::ClosureRegCheckPhase, m_func))
    {
        this->DoClosureRegCheck(layout->Return);
        this->DoClosureRegCheck(layout->Function);
    }

    BuildProfiledCallIExtendedWithICIndex(newOpcode, offset, layout->Return, layout->Function, layout->ArgCount, layout->profileId, layout->Options, layout->SpreadAuxOffset);
}

void
IRBuilder::BuildProfiledCallIExtendedWithICIndex(Js::OpCode opcode, uint32 offset, Js::RegSlot returnValue, Js::RegSlot function,
                            Js::ArgSlot argCount, Js::ProfileId profileId, Js::CallIExtendedOptions options, uint32 spreadAuxOffset)
{
    BuildProfiledCallIExtended(opcode, offset, returnValue, function, argCount, profileId, options, spreadAuxOffset);
}

template <typename SizePolicy>
void
IRBuilder::BuildProfiledCallIExtendedFlagsWithICIndex(Js::OpCode newOpcode, uint32 offset)
{
    this->m_func->m_isLeaf = false;
    Assert(OpCodeAttr::IsProfiledOpWithICIndex(newOpcode) || Js::OpCodeUtil::IsProfiledCallOpWithICIndex(newOpcode));
    Assert(OpCodeAttr::HasMultiSizeLayout(newOpcode));
    auto layout = m_jnReader.GetLayout<Js::OpLayoutDynamicProfile<Js::OpLayoutT_CallIExtendedFlagsWithICIndex<SizePolicy>>>();

    if (!PHASE_OFF(Js::ClosureRegCheckPhase, m_func))
    {
        this->DoClosureRegCheck(layout->Return);
        this->DoClosureRegCheck(layout->Function);
    }

    IR::Instr* instr = BuildProfiledCallIExtended(newOpcode, offset, layout->Return, layout->Function, layout->ArgCount, layout->profileId, layout->Options, layout->SpreadAuxOffset);
    Assert(instr->m_opcode == Js::OpCode::CallIExtendedFlags);
    if (instr->m_opcode == Js::OpCode::CallIExtendedFlags)
    {
        instr->m_opcode =
            layout->callFlags == Js::CallFlags::CallFlags_ExtraArg ? Js::OpCode::CallIEval :
            layout->callFlags == Js::CallFlags::CallFlags_New ? Js::OpCode::CallIExtendedNew :
            layout->callFlags == (Js::CallFlags::CallFlags_New | Js::CallFlags::CallFlags_ExtraArg | Js::CallFlags::CallFlags_NewTarget) ? Js::OpCode::CallIExtendedNewTargetNew :
            instr->m_opcode;
    }
}

template <typename SizePolicy>
void
IRBuilder::BuildProfiledCallI(Js::OpCode newOpcode, uint32 offset)
{
    this->m_func->m_isLeaf = false;
    Assert(OpCodeAttr::IsProfiledOp(newOpcode) || Js::OpCodeUtil::IsProfiledCallOp(newOpcode)
        || Js::OpCodeUtil::IsProfiledReturnTypeCallOp(newOpcode));
    Assert(OpCodeAttr::HasMultiSizeLayout(newOpcode));
    auto layout = m_jnReader.GetLayout<Js::OpLayoutDynamicProfile<Js::OpLayoutT_CallI<SizePolicy>>>();

    if (!PHASE_OFF(Js::ClosureRegCheckPhase, m_func))
    {
        this->DoClosureRegCheck(layout->Return);
        this->DoClosureRegCheck(layout->Function);
    }

    BuildProfiledCallI(newOpcode, offset, layout->Return, layout->Function, layout->ArgCount, layout->profileId);
}

template <typename SizePolicy>
void
IRBuilder::BuildProfiledCallIFlags(Js::OpCode newOpcode, uint32 offset)
{
    this->m_func->m_isLeaf = false;
    Assert(OpCodeAttr::IsProfiledOp(newOpcode) || Js::OpCodeUtil::IsProfiledCallOp(newOpcode)
        || Js::OpCodeUtil::IsProfiledReturnTypeCallOp(newOpcode));
    Assert(OpCodeAttr::HasMultiSizeLayout(newOpcode));
    auto layout = m_jnReader.GetLayout<Js::OpLayoutDynamicProfile<Js::OpLayoutT_CallIFlags<SizePolicy>>>();

    if (!PHASE_OFF(Js::ClosureRegCheckPhase, m_func))
    {
        this->DoClosureRegCheck(layout->Return);
        this->DoClosureRegCheck(layout->Function);
    }

    IR::Instr* instr = BuildProfiledCallI(newOpcode, offset, layout->Return, layout->Function, layout->ArgCount, layout->profileId);
    Assert(instr->m_opcode == Js::OpCode::CallIFlags);
    if (instr->m_opcode == Js::OpCode::CallIFlags)
    {
        instr->m_opcode = Js::OpCode::CallIPut;

        instr->m_opcode =
            layout->callFlags == Js::CallFlags::CallFlags_NewTarget ? Js::OpCode::CallIPut :
            layout->callFlags == (Js::CallFlags::CallFlags_NewTarget | Js::CallFlags::CallFlags_New | Js::CallFlags::CallFlags_ExtraArg) ? Js::OpCode::CallINewTargetNew :
            layout->callFlags == Js::CallFlags::CallFlags_New ? Js::OpCode::CallINew :
            instr->m_opcode;
    }
}

IR::Instr *
IRBuilder::BuildProfiledCallI(Js::OpCode opcode, uint32 offset, Js::RegSlot returnValue, Js::RegSlot function,
                            Js::ArgSlot argCount, Js::ProfileId profileId, Js::InlineCacheIndex inlineCacheIndex)
{
    Js::OpCode newOpcode;
    ValueType returnType;
    bool isProtectedByNoProfileBailout = false;

    if (opcode == Js::OpCode::ProfiledNewScObject || opcode == Js::OpCode::ProfiledNewScObjectWithICIndex
        || opcode == Js::OpCode::ProfiledNewScObjectSpread)
    {
        newOpcode = opcode;
        Js::OpCodeUtil::ConvertNonCallOpToNonProfiled(newOpcode);
        Assert(newOpcode == Js::OpCode::NewScObject || newOpcode == Js::OpCode::NewScObjectSpread);
        returnType = ValueType::GetObject(ObjectType::UninitializedObject);
    }
    else
    {
        if (this->m_func->HasProfileInfo())
        {
            returnType = this->m_func->GetProfileInfo()->GetReturnType(this->m_func->GetJnFunction(), opcode, profileId);
        }

        if (opcode < Js::OpCode::ProfiledReturnTypeCallI)
        {
            newOpcode = Js::OpCodeUtil::ConvertProfiledCallOpToNonProfiled(opcode);
            if(DoBailOnNoProfile())
            {
                if(this->m_func->m_jitTimeData)
                {
                    const Js::FunctionCodeGenJitTimeData *inlinerData = this->m_func->m_jitTimeData;
                    if(!this->IsLoopBody() && inlinerData->inlineesBv && (!inlinerData->inlineesBv->Test(profileId)
#if DBG
                        || (PHASE_STRESS(Js::BailOnNoProfilePhase, this->m_func->GetTopFunc()) &&
                            (CONFIG_FLAG(SkipFuncCountForBailOnNoProfile) < 0 ||
                            this->m_func->m_callSiteCount >= (uint)CONFIG_FLAG(SkipFuncCountForBailOnNoProfile)))
#endif
                        ))
                    {
                        this->InsertBailOnNoProfile(offset);
                        isProtectedByNoProfileBailout = true;
                    }
                    else
                    {
                        this->callTreeHasSomeProfileInfo = true;
                    }
                }
#if DBG
                this->m_func->m_callSiteCount++;
#endif
            }
        }
        else
        {
            // Changing this opcode into a non ReturnTypeCall* opcode is done in BuildCallI_Helper
            newOpcode = opcode;
        }
    }
    IR::Instr * callInstr = BuildCallI_Helper(newOpcode, offset, returnValue, function, argCount, profileId, inlineCacheIndex);
    callInstr->isCallInstrProtectedByNoProfileBailout = isProtectedByNoProfileBailout;

    if (callInstr->GetDst() && callInstr->GetDst()->GetValueType().IsUninitialized())
    {
        callInstr->GetDst()->SetValueType(returnType);
    }
    return callInstr;
}

template <typename SizePolicy>
void
IRBuilder::BuildProfiledCallIExtended(Js::OpCode newOpcode, uint32 offset)
{
    this->m_func->m_isLeaf = false;
    Assert(OpCodeAttr::IsProfiledOp(newOpcode) || Js::OpCodeUtil::IsProfiledCallOp(newOpcode)
        || Js::OpCodeUtil::IsProfiledReturnTypeCallOp(newOpcode));
    Assert(OpCodeAttr::HasMultiSizeLayout(newOpcode));
    auto layout = m_jnReader.GetLayout<Js::OpLayoutDynamicProfile<Js::OpLayoutT_CallIExtended<SizePolicy>>>();

    if (!PHASE_OFF(Js::ClosureRegCheckPhase, m_func))
    {
        this->DoClosureRegCheck(layout->Return);
        this->DoClosureRegCheck(layout->Function);
    }

    BuildProfiledCallIExtended(newOpcode, offset, layout->Return, layout->Function, layout->ArgCount, layout->profileId, layout->Options, layout->SpreadAuxOffset);
}

IR::Instr *
IRBuilder::BuildProfiledCallIExtended(Js::OpCode opcode, uint32 offset, Js::RegSlot returnValue, Js::RegSlot function,
                                      Js::ArgSlot argCount, Js::ProfileId profileId, Js::CallIExtendedOptions options,
                                      uint32 spreadAuxOffset)
{
    if (options & Js::CallIExtended_SpreadArgs)
    {
        BuildLdSpreadIndices(offset, spreadAuxOffset);
    }
    return BuildProfiledCallI(opcode, offset, returnValue, function, argCount, profileId);
}

template <typename SizePolicy>
void
IRBuilder::BuildProfiled2CallI(Js::OpCode newOpcode, uint32 offset)
{
    this->m_func->m_isLeaf = false;
    Assert(OpCodeAttr::IsProfiledOp(newOpcode));
    Assert(OpCodeAttr::HasMultiSizeLayout(newOpcode));
    auto layout = m_jnReader.GetLayout<Js::OpLayoutDynamicProfile2<Js::OpLayoutT_CallI<SizePolicy>>>();

    if (!PHASE_OFF(Js::ClosureRegCheckPhase, m_func))
    {
        this->DoClosureRegCheck(layout->Return);
        this->DoClosureRegCheck(layout->Function);
    }

    BuildProfiled2CallI(newOpcode, offset, layout->Return, layout->Function, layout->ArgCount, layout->profileId, layout->profileId2);
}

void
IRBuilder::BuildProfiled2CallI(Js::OpCode opcode, uint32 offset, Js::RegSlot returnValue, Js::RegSlot function,
                            Js::ArgSlot argCount, Js::ProfileId profileId, Js::ProfileId profileId2)
{
    Assert(opcode == Js::OpCode::ProfiledNewScObjArray || opcode == Js::OpCode::ProfiledNewScObjArraySpread);
    Js::OpCodeUtil::ConvertNonCallOpToNonProfiled(opcode);

    Js::OpCode useOpcode = opcode;
    // We either want to provide the array profile id (profileId2) to the native array creation or the call profileid (profileId)
    //     to the call to NewScObject
    Js::ProfileId useProfileId = profileId2;
    Js::TypeId arrayTypeId = Js::TypeIds_Array;
    if (returnValue != Js::Constants::NoRegister)
    {
        Js::ArrayCallSiteInfo *arrayCallSiteInfo = nullptr;
        if (m_func->HasArrayInfo())
        {
            arrayCallSiteInfo = m_func->GetProfileInfo()->GetArrayCallSiteInfo(m_func->GetJnFunction(), profileId2);
        }
        if (arrayCallSiteInfo && !m_func->IsJitInDebugMode())
        {
            if (arrayCallSiteInfo->IsNativeIntArray())
            {
                arrayTypeId = Js::TypeIds_NativeIntArray;
            }
            else if (arrayCallSiteInfo->IsNativeFloatArray())
            {
                arrayTypeId = Js::TypeIds_NativeFloatArray;
            }
        }
        else
        {
            useOpcode = (opcode == Js::OpCode::NewScObjArraySpread) ? Js::OpCode::NewScObjectSpread : Js::OpCode::NewScObject;
            useProfileId = profileId;
        }
    }
    else
    {
        useOpcode = (opcode == Js::OpCode::NewScObjArraySpread) ? Js::OpCode::NewScObjectSpread : Js::OpCode::NewScObject;
        useProfileId = profileId;
    }
    IR::Instr * callInstr = BuildCallI_Helper(useOpcode, offset, returnValue, function, argCount, useProfileId);
    if (callInstr->GetDst())
    {
        callInstr->GetDst()->SetValueType(
            ValueType::GetObject(ObjectType::Array).ToLikely().SetHasNoMissingValues(true).SetArrayTypeId(arrayTypeId));
    }
    if (callInstr->IsJitProfilingInstr())
    {
        // If we happened to decide in BuildCallI_Helper that this should be a jit profiling instr, then save the fact that it is
        //    a "new Array(args, ...)" call and also save the array profile id (profileId2)
        callInstr->AsJitProfilingInstr()->isNewArray = true;
        callInstr->AsJitProfilingInstr()->arrayProfileId = profileId2;
        // Double check that this profileId made it to the JitProfilingInstr like we expect it to.
        Assert(callInstr->AsJitProfilingInstr()->profileId == profileId);
    }
}

template <typename SizePolicy>
void
IRBuilder::BuildProfiled2CallIExtended(Js::OpCode newOpcode, uint32 offset)
{
    this->m_func->m_isLeaf = false;
    Assert(OpCodeAttr::IsProfiledOp(newOpcode));
    Assert(OpCodeAttr::HasMultiSizeLayout(newOpcode));
    auto layout = m_jnReader.GetLayout<Js::OpLayoutDynamicProfile2<Js::OpLayoutT_CallIExtended<SizePolicy>>>();

    if (!PHASE_OFF(Js::ClosureRegCheckPhase, m_func))
    {
        this->DoClosureRegCheck(layout->Return);
        this->DoClosureRegCheck(layout->Function);
    }

    BuildProfiled2CallIExtended(newOpcode, offset, layout->Return, layout->Function, layout->ArgCount, layout->profileId, layout->profileId2, layout->Options, layout->SpreadAuxOffset);
}

void
IRBuilder::BuildProfiled2CallIExtended(Js::OpCode opcode, uint32 offset, Js::RegSlot returnValue, Js::RegSlot function,
                                       Js::ArgSlot argCount, Js::ProfileId profileId, Js::ProfileId profileId2,
                                       Js::CallIExtendedOptions options, uint32 spreadAuxOffset)
{
    if (options & Js::CallIExtended_SpreadArgs)
    {
        BuildLdSpreadIndices(offset, spreadAuxOffset);
    }
    BuildProfiled2CallI(opcode, offset, returnValue, function, argCount, profileId, profileId2);
}

IR::Instr *
IRBuilder::BuildCallI_Helper(Js::OpCode newOpcode, uint32 offset, Js::RegSlot dstRegSlot, Js::RegSlot Src1RegSlot, Js::ArgSlot ArgCount, Js::ProfileId profileId, Js::InlineCacheIndex inlineCacheIndex)
{
    IR::Instr * instr;
    IR::RegOpnd *   dstOpnd;
    IR::RegOpnd *   src1Opnd;
    StackSym *      symDst;

    src1Opnd = this->BuildSrcOpnd(Src1RegSlot);
    if (dstRegSlot == Js::Constants::NoRegister)
    {
        dstOpnd = nullptr;
        symDst = nullptr;
    }
    else
    {
        dstOpnd = this->BuildDstOpnd(dstRegSlot);
        symDst = dstOpnd->m_sym;
    }

    const bool jitProfiling = m_func->DoSimpleJitDynamicProfile();
    bool profiledReturn = false;
    if (Js::OpCodeUtil::IsProfiledReturnTypeCallOp(newOpcode))
    {
        profiledReturn = true;
        newOpcode = Js::OpCodeUtil::ConvertProfiledReturnTypeCallOpToNonProfiled(newOpcode);

        // If we're profiling in the jitted code we want to propagate the profileId
        //   If we're using profile data instead of collecting it, we don't want to
        //   use the profile data from a return type call (this was previously done in IRBuilder::BuildProfiledCallI)
        if (!jitProfiling)
        {
            profileId = Js::Constants::NoProfileId;
        }
    }

    if (profileId != Js::Constants::NoProfileId)
    {
        if (jitProfiling)
        {
            // In SimpleJit we want this call to be a profiled call after being jitted
            instr = IR::JitProfilingInstr::New(newOpcode, dstOpnd, src1Opnd, m_func);
            instr->AsJitProfilingInstr()->profileId = profileId;
            instr->AsJitProfilingInstr()->isProfiledReturnCall = profiledReturn;
            instr->AsJitProfilingInstr()->inlineCacheIndex = inlineCacheIndex;
        }
        else
        {
            instr = IR::ProfiledInstr::New(newOpcode, dstOpnd, src1Opnd, m_func);
            instr->AsProfiledInstr()->u.profileId = profileId;
        }
    }
    else
    {
        instr = IR::Instr::New(newOpcode, m_func);
        instr->SetSrc1(src1Opnd);
        if (dstOpnd != nullptr)
        {
            instr->SetDst(dstOpnd);
        }
    }

    if (dstOpnd && newOpcode == Js::OpCode::NewScObject)
    {
        dstOpnd->SetValueType(ValueType::GetObject(ObjectType::UninitializedObject));
    }

    if (symDst && symDst->m_isSingleDef)
    {
        switch (instr->m_opcode)
        {
        case Js::OpCode::NewScObject:
        case Js::OpCode::NewScObjectSpread:
        case Js::OpCode::NewScObjectLiteral:
        case Js::OpCode::NewScObjArray:
        case Js::OpCode::NewScObjArraySpread:
            symDst->m_isSafeThis = true;
            symDst->m_isNotInt = true;
            break;
        }
    }

    this->AddInstr(instr, offset);

    this->BuildCallCommon(instr, symDst, ArgCount);

    return instr;
}

void
IRBuilder::BuildCallCommon(IR::Instr * instr, StackSym * symDst, Js::ArgSlot argCount)
{
    Js::OpCode newOpcode = instr->m_opcode;

    IR::Instr *     argInstr = nullptr;
    IR::Instr *     prevInstr = instr;
#if DBG
    int count = 0;
#endif

    // Link all the args of this call by creating a def/use chain through the src2.

    for (argInstr = this->m_argStack->Pop();
        argInstr && argInstr->m_opcode != Js::OpCode::StartCall;
        argInstr = this->m_argStack->Pop())
    {
        prevInstr->SetSrc2(argInstr->GetDst());
        prevInstr = argInstr;
#if DBG
        count++;
#endif
    }

    if (this->m_argStack->Empty())
    {
        this->callTreeHasSomeProfileInfo = false;
    }

    if (newOpcode == Js::OpCode::NewScObject || newOpcode == Js::OpCode::NewScObjArray
        || newOpcode == Js::OpCode::NewScObjectSpread || newOpcode == Js::OpCode::NewScObjArraySpread)
    {
#if DBG
        count++;
#endif
        m_argsOnStack++;
    }

    if (argInstr)
    {
        prevInstr->SetSrc2(argInstr->GetDst());
        AssertMsg(instr->m_prev->m_opcode == Js::OpCode::LdSpreadIndices
            // All non-spread calls need StartCall to have the same number of args
            || (argInstr->GetSrc1()->IsIntConstOpnd()
                    && argInstr->GetSrc1()->AsIntConstOpnd()->GetValue() == count
                    && count == argCount), "StartCall has wrong number of arguments...");
    }
    else
    {
        AssertMsg(false, "Expect StartCall on other opcodes...");
    }

    // Update Func if this is the highest amount of stack we've used so far
    // to push args.
#if DBG
    m_callsOnStack--;
#endif
    if (m_func->m_argSlotsForFunctionsCalled < m_argsOnStack)
        m_func->m_argSlotsForFunctionsCalled = m_argsOnStack;
#if DBG
    if (m_callsOnStack == 0)
        Assert(m_argsOnStack == argCount);
#endif
    m_argsOnStack -= argCount;

    if (m_func->IsJitInDebugMode())
    {
        // Insert bailout after return from a call, script or library function call.
        this->InsertBailOutForDebugger(
            m_jnReader.GetCurrentOffset(), // bailout will resume at the offset of next instr.
            c_debuggerBailOutKindForCall);
    }
}

///----------------------------------------------------------------------------
///
/// IRBuilder::BuildClass
///
///     Build IR instr for an InitClass instruction.
///
///----------------------------------------------------------------------------


template <typename SizePolicy>
void
IRBuilder::BuildClass(Js::OpCode newOpcode, uint32 offset)
{
    Assert(!OpCodeAttr::IsProfiledOp(newOpcode));
    Assert(OpCodeAttr::HasMultiSizeLayout(newOpcode));
    auto layout = m_jnReader.GetLayout<Js::OpLayoutT_Class<SizePolicy>>();

    if (!PHASE_OFF(Js::ClosureRegCheckPhase, m_func))
    {
        this->DoClosureRegCheck(layout->Constructor);
        this->DoClosureRegCheck(layout->Extends);
    }

    BuildClass(newOpcode, offset, layout->Constructor, layout->Extends);
}

void
IRBuilder::BuildClass(Js::OpCode newOpcode, uint32 offset, Js::RegSlot constructor, Js::RegSlot extends)
{
    Assert(newOpcode == Js::OpCode::InitClass);

    IR::Instr * insn = IR::Instr::New(newOpcode, m_func);
    insn->SetSrc1(this->BuildSrcOpnd(constructor));

    if (extends != Js::Constants::NoRegister)
    {
        insn->SetSrc2(this->BuildSrcOpnd(extends));
    }

    this->AddInstr(insn, offset);
}


///----------------------------------------------------------------------------
///
/// IRBuilder::BuildBrReg1
///
///     Build IR instr for a BrReg1 instruction.
///     This is a conditional branch with a single source operand (e.g., "if (x)" ...)
///
///----------------------------------------------------------------------------


template <typename SizePolicy>
void
IRBuilder::BuildBrReg1(Js::OpCode newOpcode, uint32 offset)
{
    Assert(!OpCodeAttr::IsProfiledOp(newOpcode));
    Assert(OpCodeAttr::HasMultiSizeLayout(newOpcode));
    auto layout = m_jnReader.GetLayout<Js::OpLayoutT_BrReg1<SizePolicy>>();

    if (!PHASE_OFF(Js::ClosureRegCheckPhase, m_func))
    {
        this->DoClosureRegCheck(layout->R1);
    }

    BuildBrReg1(newOpcode, offset, m_jnReader.GetCurrentOffset() + layout->RelativeJumpOffset, layout->R1);
}

void
IRBuilder::BuildBrReg1(Js::OpCode newOpcode, uint32 offset, uint targetOffset, Js::RegSlot srcRegSlot)
{
    IR::BranchInstr * branchInstr;
    IR::RegOpnd *     srcOpnd;
    srcOpnd = this->BuildSrcOpnd(srcRegSlot);

    if (newOpcode == Js::OpCode::BrNotUndecl_A) {
        IR::AddrOpnd *srcOpnd2 = IR::AddrOpnd::New(this->m_func->GetScriptContext()->GetLibrary()->GetUndeclBlockVar(),
            IR::AddrOpndKindDynamicVar, this->m_func);
        branchInstr = IR::BranchInstr::New(Js::OpCode::BrNotAddr_A, nullptr, srcOpnd, srcOpnd2, m_func);
    } else {
        branchInstr = IR::BranchInstr::New(newOpcode, nullptr, srcOpnd, m_func);
    }

    this->AddBranchInstr(branchInstr, offset, targetOffset);
}

///----------------------------------------------------------------------------
///
/// IRBuilder::BuildBrReg2
///
///     Build IR instr for a BrReg2 instruction.
///     This is a conditional branch with a 2 source operands (e.g., "if (x == y)" ...)
///
///----------------------------------------------------------------------------

template <typename SizePolicy>
void
IRBuilder::BuildBrReg2(Js::OpCode newOpcode, uint32 offset)
{
    Assert(!OpCodeAttr::IsProfiledOp(newOpcode));
    Assert(OpCodeAttr::HasMultiSizeLayout(newOpcode));
    auto layout = m_jnReader.GetLayout<Js::OpLayoutT_BrReg2<SizePolicy>>();

    if (!PHASE_OFF(Js::ClosureRegCheckPhase, m_func))
    {
        this->DoClosureRegCheck(layout->R1);
        this->DoClosureRegCheck(layout->R2);
    }

    BuildBrReg2(newOpcode, offset, m_jnReader.GetCurrentOffset() + layout->RelativeJumpOffset, layout->R1, layout->R2);
}

void
IRBuilder::BuildBrBReturn(Js::OpCode newOpcode, uint32 offset, Js::RegSlot DestRegSlot, Js::RegSlot SrcRegSlot, uint32 targetOffset)
{
    IR::RegOpnd *     srcOpnd = this->BuildSrcOpnd(SrcRegSlot);
    IR::RegOpnd *     destOpnd = this->BuildDstOpnd(DestRegSlot);
    IR::BranchInstr * branchInstr = IR::BranchInstr::New(newOpcode, destOpnd, nullptr, srcOpnd, m_func);
    this->AddBranchInstr(branchInstr, offset, targetOffset);

    switch (newOpcode)
    {
    case Js::OpCode::BrOnEmpty:
        destOpnd->SetValueType(ValueType::String);
        break;
    default:
        Assert(false);
        break;
    };
}

void
IRBuilder::BuildBrReg2(Js::OpCode newOpcode, uint32 offset, uint targetOffset, Js::RegSlot R1, Js::RegSlot R2)
{
    IR::BranchInstr * branchInstr;

    if (newOpcode == Js::OpCode::BrOnEmpty
        /* || newOpcode == Js::OpCode::BrOnNotEmpty */     // BrOnNotEmpty not generate by the byte code
            )
    {
        BuildBrBReturn(newOpcode, offset, R1, R2, targetOffset);
        return;
    }

    IR::RegOpnd *     src1Opnd;
    IR::RegOpnd *     src2Opnd;

    src1Opnd = this->BuildSrcOpnd(R1);
    src2Opnd = this->BuildSrcOpnd(R2);

    if (newOpcode == Js::OpCode::Case)
    {
        // generating branches for Cases is entirely handled
        // by the SwitchIRBuilder

        m_switchBuilder.OnCase(src1Opnd, src2Opnd, offset, targetOffset);

#ifdef BYTECODE_BRANCH_ISLAND
        // Make sure that if there are branch island between the cases, we consume it first
        EnsureConsumeBranchIsland();
#endif

        // some instructions can't be optimized past, such as LdFld for objects. In these cases we have
        // to inform the SwitchBuilder to flush any optimized cases that it has stored up to this point
        // peeks the next opcode - to check if it is not a case statement (for example: the next instr can be a LdFld for objects)
        Js::OpCode peekOpcode = m_jnReader.PeekOp();
        if (peekOpcode != Js::OpCode::Case && peekOpcode != Js::OpCode::EndSwitch)
        {
            m_switchBuilder.FlushCases(m_jnReader.GetCurrentOffset());
        }
    }
    else
    {
        branchInstr = IR::BranchInstr::New(newOpcode, nullptr, src1Opnd, src2Opnd, m_func);
        branchInstr->m_isSwitchBr = true;
        this->AddBranchInstr(branchInstr, offset, targetOffset);
    }
}

void
IRBuilder::BuildEmpty(Js::OpCode newOpcode, uint32 offset)
{
    IR::Instr *instr;

    m_jnReader.Empty();

    instr = IR::Instr::New(newOpcode, m_func);

    switch (newOpcode)
    {
    case Js::OpCode::Ret:
    {
        IR::RegOpnd *regOpnd = BuildDstOpnd(0);
        instr->SetSrc1(regOpnd);
        this->AddInstr(instr, offset);
        break;
    }

    case Js::OpCode::Leave:
    {
        IR::BranchInstr * branchInstr;
        IR::LabelInstr * labelInstr;

        if (this->catchOffsetStack && !this->catchOffsetStack->Empty())
        {
            // If the try region has a break block, we don't want the Flowgraph to move all of that code out of the loop
            // because an exception will bring the control back into the loop. The branch out of the loop (which is the
            // reason for the code to be a break block) can still be moved out though.
            //
            // "BrOnException $catch" is inserted before Leave's in the try region to instrument flow from the try region
            // to the catch region (which is in the loop).
            IR::BranchInstr * brOnException = IR::BranchInstr::New(Js::OpCode::BrOnException, nullptr, this->m_func);
            this->AddBranchInstr(brOnException, offset, this->catchOffsetStack->Top());
        }

        labelInstr = IR::LabelInstr::New(Js::OpCode::Label, this->m_func);
        branchInstr = IR::BranchInstr::New(newOpcode, labelInstr, this->m_func);
        this->AddInstr(branchInstr, offset);
        this->AddInstr(labelInstr, Js::Constants::NoByteCodeOffset);

        break;
    }

    case Js::OpCode::Break:
        if (m_func->IsJitInDebugMode())
        {
            // Add explicit bailout.
            this->InsertBailOutForDebugger(offset, IR::BailOutExplicit);
        }
        else
        {
            // Default behavior, let's keep it for now, removed in lowerer.
            this->AddInstr(instr, offset);
        }
        break;

    default:
        this->AddInstr(instr, offset);
        break;
    }
}

#ifdef BYTECODE_BRANCH_ISLAND
void
IRBuilder::EnsureConsumeBranchIsland()
{
    if (m_jnReader.PeekOp() == Js::OpCode::Br)
    {
        // Save the old offset
        uint offset = m_jnReader.GetCurrentOffset();

        // Read the potentially a branch around
        Js::LayoutSize layoutSize;
        Js::OpCode opcode = m_jnReader.ReadOp(layoutSize);
        Assert(opcode == Js::OpCode::Br);
        Assert(layoutSize == Js::SmallLayout);
        const unaligned Js::OpLayoutBr * playout = m_jnReader.Br();
        unsigned int      targetOffset = m_jnReader.GetCurrentOffset() + playout->RelativeJumpOffset;

        uint branchIslandOffset = m_jnReader.GetCurrentOffset();
        if (branchIslandOffset == targetOffset)
        {
            // branch to next, there is no long branch
            m_jnReader.SetCurrentOffset(offset);
            return;
        }

        // Ignore all the BrLong
        while (m_jnReader.PeekOp() == Js::OpCode::BrLong)
        {
            opcode = m_jnReader.ReadOp(layoutSize);
            Assert(opcode == Js::OpCode::BrLong);
            Assert(layoutSize == Js::SmallLayout);
            m_jnReader.BrLong();
        }

        // Confirm that is a branch around
        if ((uint)m_jnReader.GetCurrentOffset() == targetOffset)
        {
            // Really consume the branch island
            m_jnReader.SetCurrentOffset(branchIslandOffset);
            ConsumeBranchIsland();

            // Mark the virtual branch around as a redirect long branch as well
            // so that if it is the target of a another branch, it will just keep pass
            // the branch island
            Assert(longBranchMap);
            Assert(offset < m_offsetToInstructionCount);
            Assert(m_offsetToInstruction[offset] == nullptr);
            m_offsetToInstruction[offset] = VirtualLongBranchInstr;
            longBranchMap->Add(offset, targetOffset);
        }
        else
        {
            // Reset the offset
            m_jnReader.SetCurrentOffset(offset);
        }
    }
}

IR::Instr * const IRBuilder::VirtualLongBranchInstr = (IR::Instr *)-1;

void
IRBuilder::ConsumeBranchIsland()
{
    do
    {
        uint32 offset = m_jnReader.GetCurrentOffset();
        Js::LayoutSize layoutSize;
        Js::OpCode opcode = m_jnReader.ReadOp(layoutSize);
        Assert(opcode == Js::OpCode::BrLong);
        Assert(layoutSize == Js::SmallLayout);
        BuildBrLong(Js::OpCode::BrLong, offset);
    }
    while (m_jnReader.PeekOp() == Js::OpCode::BrLong);
}

void
IRBuilder::BuildBrLong(Js::OpCode newOpcode, uint32 offset)
{
    Assert(newOpcode == Js::OpCode::BrLong);
    Assert(!OpCodeAttr::HasMultiSizeLayout(newOpcode));
    Assert(offset != Js::Constants::NoByteCodeOffset);

    const unaligned   Js::OpLayoutBrLong *branchInsn = m_jnReader.BrLong();
    unsigned int      targetOffset = m_jnReader.GetCurrentOffset() + branchInsn->RelativeJumpOffset;

    Assert(offset < m_offsetToInstructionCount);
    Assert(m_offsetToInstruction[offset] == nullptr);

    // BrLong are also just the target of another branch, just set a virtual long branch instr
    // and remap the original branch to the actual destination in ResolveVirtualLongBranch
    m_offsetToInstruction[offset] = VirtualLongBranchInstr;
    longBranchMap->Add(offset, targetOffset);
}


uint
IRBuilder::ResolveVirtualLongBranch(IR::BranchInstr * branchInstr, uint offset)
{
    Assert(longBranchMap);
    uint32 targetOffset;
    if (!longBranchMap->TryGetValue(offset, &targetOffset))
    {
        // If we see a VirtualLongBranchInstr, we must have a mapping to the real target offset
        Assert(false);
        Fatal();
    }

    //  If this is a jump out of the loop body we need to load the return IP and jump to the loop exit instead
    if (!IsLoopBodyOuterOffset(targetOffset))
    {
        return targetOffset;
    }

    // Multi branch shouldn't be exiting a loop
    Assert(branchInstr->m_opcode != Js::OpCode::MultiBr);

    // Don't load the return IP if it is already loaded (for the case of early exit)
    if (!IsLoopBodyReturnIPInstr(branchInstr->m_prev))
    {
        IR::Instr * returnIPInstr = CreateLoopBodyReturnIPInstr(targetOffset, branchInstr->GetByteCodeOffset());
        branchInstr->InsertBefore(returnIPInstr);

        // Any jump to this branch to jump to the return IP load instr first
        uint32 offset = branchInstr->GetByteCodeOffset();
        if (this->m_offsetToInstruction[offset] == branchInstr)
        {
            this->m_offsetToInstruction[offset] = returnIPInstr;
        }
        else
        {
            Assert(this->m_offsetToInstruction[offset]->HasBailOutInfo() &&
                   this->m_offsetToInstruction[offset]->GetBailOutKind() == IR::BailOutInjected);
        }
    }
    return GetLoopBodyExitInstrOffset();
}
#endif

///----------------------------------------------------------------------------
///
/// IRBuilder::BuildBr
///
///     Build IR instr for a Br (unconditional branch) instruction.
///     or TryCatch/TryFinally
///
///----------------------------------------------------------------------------

void
IRBuilder::BuildBr(Js::OpCode newOpcode, uint32 offset)
{
    Assert(!OpCodeAttr::HasMultiSizeLayout(newOpcode));

    IR::BranchInstr * branchInstr;
    const unaligned   Js::OpLayoutBr *branchInsn = m_jnReader.Br();
    unsigned int      targetOffset = m_jnReader.GetCurrentOffset() + branchInsn->RelativeJumpOffset;

#ifdef BYTECODE_BRANCH_ISLAND
    bool isLongBranchIsland = (m_jnReader.PeekOp() == Js::OpCode::BrLong);
    if (isLongBranchIsland)
    {
        ConsumeBranchIsland();
    }
#endif

    if(newOpcode == Js::OpCode::EndSwitch)
    {
        m_switchBuilder.EndSwitch(offset, targetOffset);
        return;
    }
#ifdef PERF_HINT
    else if (PHASE_TRACE1(Js::PerfHintPhase) && (newOpcode == Js::OpCode::TryCatch || newOpcode == Js::OpCode::TryFinally) )
    {
        WritePerfHint(PerfHints::HasTryBlock, this->m_func->GetJnFunction(), offset);
    }
#endif

#ifdef BYTECODE_BRANCH_ISLAND
    if (isLongBranchIsland && (targetOffset == (uint)m_jnReader.GetCurrentOffset()))
    {
        // Branch to next (probably after consume branch island), try to not emit the branch

        // Mark the jump around instruction as a virtual long branch as well so we can just
        // fall through instead of branch to exit
        Assert(offset < m_offsetToInstructionCount);
        if (m_offsetToInstruction[offset] == nullptr)
        {
            m_offsetToInstruction[offset] = VirtualLongBranchInstr;
            longBranchMap->Add(offset, targetOffset);
            return;
        }

        // We may have already create an instruction on this offset as a statement boundary
        // or in the bailout at every byte code case.

        // The statement boundary case only happens if we have emitted the long branch island
        // after an existing no fall through instruction, but that instruction also happen to be
        // branch to next.  We will just generate an actual branch to next instruction.

        Assert(m_offsetToInstruction[offset]->m_opcode == Js::OpCode::StatementBoundary
            || (Js::Configuration::Global.flags.IsEnabled(Js::BailOutAtEveryByteCodeFlag)
            && m_offsetToInstruction[offset]->m_opcode == Js::OpCode::BailOnEqual));
    }
#endif

    if ((newOpcode == Js::OpCode::TryCatch) && this->catchOffsetStack)
    {
        this->catchOffsetStack->Push(targetOffset);
    }
    branchInstr = IR::BranchInstr::New(newOpcode, nullptr, m_func);
    this->AddBranchInstr(branchInstr, offset, targetOffset);
}

void
IRBuilder::BuildBrS(Js::OpCode newOpcode, uint32 offset)
{
    Assert(!OpCodeAttr::HasMultiSizeLayout(newOpcode));

    IR::BranchInstr * branchInstr;
    const unaligned   Js::OpLayoutBrS *branchInsn = m_jnReader.BrS();

    unsigned int      targetOffset = m_jnReader.GetCurrentOffset() + branchInsn->RelativeJumpOffset;

    branchInstr = IR::BranchInstr::New(newOpcode, nullptr,
        IR::IntConstOpnd::New(branchInsn->val,
        TyInt32, m_func),m_func);
    this->AddBranchInstr(branchInstr, offset, targetOffset);
}

///----------------------------------------------------------------------------
///
/// IRBuilder::BuildBrProperty
///
///     Build IR instr for a BrProperty instruction.
///     This is a conditional branch that tests whether the given property
/// is present on the given instance.
///
///----------------------------------------------------------------------------

void
IRBuilder::BuildBrProperty(Js::OpCode newOpcode, uint32 offset)
{
    Assert(!OpCodeAttr::HasMultiSizeLayout(newOpcode));

    const unaligned   Js::OpLayoutBrProperty *branchInsn = m_jnReader.BrProperty();

    if (!PHASE_OFF(Js::ClosureRegCheckPhase, m_func))
    {
        this->DoClosureRegCheck(branchInsn->Instance);
    }

    IR::BranchInstr * branchInstr;
    Js::PropertyId    propertyId =
        this->m_func->GetJnFunction()->GetReferencedPropertyId(branchInsn->PropertyIdIndex);
    unsigned int      targetOffset = m_jnReader.GetCurrentOffset() + branchInsn->RelativeJumpOffset;
    IR::SymOpnd *     fieldSymOpnd = this->BuildFieldOpnd(newOpcode, branchInsn->Instance, propertyId, branchInsn->PropertyIdIndex, PropertyKindData);

    branchInstr = IR::BranchInstr::New(newOpcode, nullptr, fieldSymOpnd, m_func);
    this->AddBranchInstr(branchInstr, offset, targetOffset);
}

void
IRBuilder::BuildBrLocalProperty(Js::OpCode newOpcode, uint32 offset)
{
    Assert(!OpCodeAttr::HasMultiSizeLayout(newOpcode));

    switch (newOpcode)
    {
    case Js::OpCode::BrOnNoLocalProperty:
        newOpcode = Js::OpCode::BrOnNoProperty;
        break;

    default:
        Assert(0);
        break;
    }

    const unaligned   Js::OpLayoutBrLocalProperty *branchInsn = m_jnReader.BrLocalProperty();

    if (m_func->GetLocalClosureSym()->HasByteCodeRegSlot())
    {
        IR::ByteCodeUsesInstr * byteCodeUse = IR::ByteCodeUsesInstr::New(m_func);
        byteCodeUse->byteCodeUpwardExposedUsed = JitAnew(m_func->m_alloc, BVSparse<JitArenaAllocator>, m_func->m_alloc);
        byteCodeUse->byteCodeUpwardExposedUsed->Set(m_func->GetLocalClosureSym()->m_id);
        this->AddInstr(byteCodeUse, offset);
    }

    IR::BranchInstr * branchInstr;
    Js::PropertyId    propertyId =
        this->m_func->GetJnFunction()->GetReferencedPropertyId(branchInsn->PropertyIdIndex);
    unsigned int      targetOffset = m_jnReader.GetCurrentOffset() + branchInsn->RelativeJumpOffset;
    IR::SymOpnd *     fieldSymOpnd = this->BuildFieldOpnd(newOpcode, m_func->GetJnFunction()->GetLocalClosureReg(), propertyId, branchInsn->PropertyIdIndex, PropertyKindData);

    branchInstr = IR::BranchInstr::New(newOpcode, nullptr, fieldSymOpnd, m_func);
    this->AddBranchInstr(branchInstr, offset, targetOffset);
}

void
IRBuilder::BuildBrEnvProperty(Js::OpCode newOpcode, uint32 offset)
{
    Assert(!OpCodeAttr::HasMultiSizeLayout(newOpcode));

    const unaligned   Js::OpLayoutBrEnvProperty *branchInsn = m_jnReader.BrEnvProperty();
    IR::Instr *instr;
    IR::BranchInstr * branchInstr;
    IR::RegOpnd *regOpnd;
    IR::SymOpnd *fieldOpnd;
    PropertySym *fieldSym;

    fieldOpnd = this->BuildFieldOpnd(Js::OpCode::LdSlotArr, this->GetEnvReg(), branchInsn->SlotIndex, (Js::PropertyIdIndexType)-1, PropertyKindSlotArray);
    regOpnd = IR::RegOpnd::New(TyVar, m_func);
    instr = IR::Instr::New(Js::OpCode::LdSlotArr, regOpnd, fieldOpnd, m_func);
    this->AddInstr(instr, offset);

    Js::PropertyId    propertyId =
        this->m_func->GetJnFunction()->GetReferencedPropertyId(branchInsn->PropertyIdIndex);
    unsigned int      targetOffset = m_jnReader.GetCurrentOffset() + branchInsn->RelativeJumpOffset;\
    fieldSym = PropertySym::New(regOpnd->m_sym, propertyId, branchInsn->PropertyIdIndex, (uint)-1, PropertyKindData, m_func);
    fieldOpnd = IR::SymOpnd::New(fieldSym, TyVar, m_func);

    branchInstr = IR::BranchInstr::New(Js::OpCode::BrOnNoProperty, nullptr, fieldOpnd, m_func);
    this->AddBranchInstr(branchInstr, offset, targetOffset);
}

///----------------------------------------------------------------------------
///
/// IRBuilder::AddBranchInstr
///
///     Create a branch/offset pair which will be fixed up at the end of the
/// IRBuilder phase and add the instruction
///
///----------------------------------------------------------------------------

BranchReloc *
IRBuilder::AddBranchInstr(IR::BranchInstr * branchInstr, uint32 offset, uint32 targetOffset)
{
    //
    // Loop jitting would be done only till the LoopEnd
    // Any branches beyond that offset are for the return stmt
    //
    if (IsLoopBodyOuterOffset(targetOffset))
    {
        // if we have loaded the loop IP sym from the ProfiledLoopEnd then don't add it here
        if (!IsLoopBodyReturnIPInstr(m_lastInstr))
        {
            this->InsertLoopBodyReturnIPInstr(targetOffset, offset);
        }

        // Jump the restore StSlot and Ret instruction
        targetOffset = GetLoopBodyExitInstrOffset();
    }

    BranchReloc *  reloc = nullptr;
    reloc = this->CreateRelocRecord(branchInstr, offset, targetOffset);

    this->AddInstr(branchInstr, offset);
    return reloc;
}

BranchReloc *
IRBuilder::CreateRelocRecord(IR::BranchInstr * branchInstr, uint32 offset, uint32 targetOffset)
{
    BranchReloc *  reloc = JitAnew(this->m_tempAlloc, BranchReloc, branchInstr, offset, targetOffset);
    this->branchRelocList->Prepend(reloc);
    return reloc;
}
///----------------------------------------------------------------------------
///
/// IRBuilder::BuildRegexFromPattern
///
/// Build a new RegEx instruction. Simply construct a var to hold the regex
/// and load it as an immediate into a register.
///
///----------------------------------------------------------------------------

void
IRBuilder::BuildRegexFromPattern(Js::RegSlot dstRegSlot, uint32 patternIndex, uint32 offset)
{
    IR::Instr * instr;

    IR::RegOpnd* dstOpnd = this->BuildDstOpnd(dstRegSlot);
    dstOpnd->SetValueType(ValueType::GetObject(ObjectType::RegExp));

    IR::Opnd * regexOpnd = IR::AddrOpnd::New(this->m_func->GetJnFunction()->GetLiteralRegex(patternIndex), IR::AddrOpndKindDynamicMisc, this->m_func);

    instr = IR::Instr::New(Js::OpCode::NewRegEx, dstOpnd, regexOpnd, this->m_func);
    this->AddInstr(instr, offset);
}


bool
IRBuilder::IsFloatFunctionCallsite(Js::BuiltinFunction index, size_t argc)
{
    return Js::JavascriptLibrary::IsFloatFunctionCallsite(index, argc);
}

void
IRBuilder::CheckBuiltIn(PropertySym * propertySym, Js::BuiltinFunction *puBuiltInIndex)
{
    Js::BuiltinFunction index = Js::BuiltinFunction::None;

    // Check whether the propertySym appears to be a built-in.
    if (propertySym->m_fieldKind != PropertyKindData)
    {
        return;
    }

    index = Js::JavascriptLibrary::GetBuiltinFunctionForPropId(propertySym->m_propertyId);
    if (index == Js::BuiltinFunction::None)
    {
        return;
    }

    // If the target is one of the Math built-ins, see whether the stack sym is the
    // global "Math".
    if (Js::JavascriptLibrary::IsFltFunc(index))
    {
        if (!propertySym->m_stackSym->m_isSingleDef)
        {
            return;
        }

        IR::Instr *instr = propertySym->m_stackSym->m_instrDef;
        AssertMsg(instr != nullptr, "Single-def stack sym w/o def instr?");

        if (instr->m_opcode != Js::OpCode::LdRootFld && instr->m_opcode != Js::OpCode::LdRootFldForTypeOf)
        {
            return;
        }

        IR::Opnd * opnd = instr->GetSrc1();
        AssertMsg(opnd != nullptr && opnd->IsSymOpnd() && opnd->AsSymOpnd()->m_sym->IsPropertySym(),
            "LdRootFld w/o propertySym src?");

        if (opnd->AsSymOpnd()->m_sym->AsPropertySym()->m_propertyId != Js::PropertyIds::Math)
        {
            return;
        }
    }

    *puBuiltInIndex = index;
}

void
IRBuilder::GenerateLoopBodySlotAccesses(uint offset)
{
    //
    // The interpreter instance is passed as 0th argument to the JITted loop body function.
    // Always load the argument, then use it to generate any necessary store-slots.
    //
    uint16      argument = 0;

    StackSym *symSrc     = StackSym::NewParamSlotSym(argument + 1, m_func);
    symSrc->m_offset     = (argument + LowererMD::GetFormalParamOffset()) * MachPtr;
    symSrc->m_allocated = true;
    IR::SymOpnd *srcOpnd = IR::SymOpnd::New(symSrc, TyVar, m_func);

    StackSym *loopParamSym = m_func->EnsureLoopParamSym();
    IR::RegOpnd *loopParamOpnd = IR::RegOpnd::New(loopParamSym, TyMachPtr, m_func);

    IR::Instr *instrArgIn = IR::Instr::New(Js::OpCode::ArgIn_A, loopParamOpnd, srcOpnd, m_func);
    m_func->m_headInstr->InsertAfter(instrArgIn);

    GenerateLoopBodyStSlots(loopParamSym->m_id, offset);
}

void
IRBuilder::GenerateLoopBodyStSlots(SymID loopParamSymId, uint offset)
{
    if (this->m_stSlots->Count() == 0)
    {
        return;
    }

    FOREACH_BITSET_IN_FIXEDBV(regSlot, this->m_stSlots)
    {
        this->GenerateLoopBodyStSlot(regSlot, offset);
    }
    NEXT_BITSET_IN_FIXEDBV;
}

IR::Instr *
IRBuilder::GenerateLoopBodyStSlot(Js::RegSlot regSlot, uint offset)
{
    Assert(!this->RegIsConstant((Js::RegSlot)regSlot));

    StackSym *loopParamSym = m_func->EnsureLoopParamSym();
    PropertySym * fieldSym = PropertySym::FindOrCreate(loopParamSym->m_id, (Js::PropertyId)(regSlot + this->m_loopBodyLocalsStartSlot), (uint32)-1, (uint)-1, PropertyKindLocalSlots, m_func);
    IR::SymOpnd * fieldSymOpnd = IR::SymOpnd::New(fieldSym, TyVar, m_func);

    IR::RegOpnd * regOpnd = this->BuildSrcOpnd((Js::RegSlot)regSlot);
#if !FLOATVAR
    Js::OpCode opcode = Js::OpCode::StSlotBoxTemp;
#else
    Js::OpCode opcode = Js::OpCode::StSlot;
#endif
    IR::Instr * stSlotInstr = IR::Instr::New(opcode, fieldSymOpnd, regOpnd, m_func);
    if (offset != Js::Constants::NoByteCodeOffset)
    {
        this->AddInstr(stSlotInstr, offset);
        return nullptr;
    }
    else
    {
        return stSlotInstr;
    }
}

IR::Instr *
IRBuilder::CreateLoopBodyReturnIPInstr(uint targetOffset, uint offset)
{
    IR::RegOpnd * retOpnd = IR::RegOpnd::New(m_loopBodyRetIPSym, TyMachReg, m_func);
    IR::IntConstOpnd * exitOffsetOpnd = IR::IntConstOpnd::New(targetOffset, TyInt32, m_func);
    return IR::Instr::New(Js::OpCode::Ld_I4, retOpnd, exitOffsetOpnd, m_func);
}

IR::Opnd *
IRBuilder::InsertLoopBodyReturnIPInstr(uint targetOffset, uint offset)
{
    IR::Instr * setRetValueInstr = CreateLoopBodyReturnIPInstr(targetOffset, offset);
    this->AddInstr(setRetValueInstr, offset);
    return setRetValueInstr->GetDst();
}

void
IRBuilder::InsertDoneLoopBodyLoopCounter(uint32 lastOffset)
{
    if (m_loopCounterSym == nullptr)
    {
        return;
    }

    IR::Instr * loopCounterStoreInstr = IR::Instr::New(Js::OpCode::StLoopBodyCount, m_func);
    IR::RegOpnd *countRegOpnd = IR::RegOpnd::New(m_loopCounterSym, TyInt32, this->m_func);
    countRegOpnd->SetIsJITOptimizedReg(true);

    loopCounterStoreInstr->SetSrc1(countRegOpnd);
    this->AddInstr(loopCounterStoreInstr, lastOffset + 1);

    return;
}

void
IRBuilder::InsertIncrLoopBodyLoopCounter(IR::LabelInstr *loopTopLabelInstr)
{
    Assert(this->IsLoopBody());

    IR::RegOpnd *loopCounterOpnd = IR::RegOpnd::New(m_loopCounterSym, TyInt32, this->m_func);
    IR::Instr * incr = IR::Instr::New(Js::OpCode::IncrLoopBodyCount, loopCounterOpnd, loopCounterOpnd, this->m_func);
    loopCounterOpnd->SetIsJITOptimizedReg(true);

    IR::Instr* nextRealInstr = loopTopLabelInstr->GetNextRealInstr();
    nextRealInstr->InsertBefore(incr);

    Assert(nextRealInstr->GetByteCodeOffset() < m_offsetToInstructionCount);
    if(this->m_offsetToInstruction[nextRealInstr->GetByteCodeOffset()] == nextRealInstr)
    {
        this->m_offsetToInstruction[nextRealInstr->GetByteCodeOffset()] = incr;
    }
    incr->SetByteCodeOffset(nextRealInstr->GetByteCodeOffset());
    return;
}

void
IRBuilder::InsertInitLoopBodyLoopCounter(uint loopNum)
{
    Assert(this->IsLoopBody());

    Js::LoopHeader * loopHeader = this->m_func->GetJnFunction()->GetLoopHeader(loopNum);
    JsLoopBodyCodeGen* loopBodyCodeGen = (JsLoopBodyCodeGen*)m_func->m_workItem;
    Assert(loopBodyCodeGen->loopHeader == loopHeader);  //Init only once

    m_loopCounterSym = StackSym::New(TyVar, this->m_func);

    IR::RegOpnd* loopCounterOpnd = IR::RegOpnd::New(m_loopCounterSym, TyVar, this->m_func);
    loopCounterOpnd->SetIsJITOptimizedReg(true);

    IR::Instr * initInstr = IR::Instr::New(Js::OpCode::InitLoopBodyCount, loopCounterOpnd, this->m_func);
    m_lastInstr->InsertAfter(initInstr);
    m_lastInstr = initInstr;
    initInstr->SetByteCodeOffset(m_jnReader.GetCurrentOffset());
}

IR::AddrOpnd *
IRBuilder::BuildAuxArrayOpnd(AuxArrayValue auxArrayType, uint32 offset, uint32 auxArrayOffset, uint extraSlots)
{
    switch (auxArrayType)
    {
    case AuxArrayValue::AuxPropertyIdArray:
        return IR::AddrOpnd::New((Js::Var)Js::ByteCodeReader::ReadPropertyIdArray(auxArrayOffset, m_functionBody, extraSlots), IR::AddrOpndKindDynamicMisc, m_func);
    case AuxArrayValue::AuxIntArray:
        return IR::AddrOpnd::New((Js::Var)Js::ByteCodeReader::ReadAuxArray<int32>(auxArrayOffset, m_functionBody), IR::AddrOpndKindDynamicMisc, m_func);
    case AuxArrayValue::AuxFloatArray:
        return IR::AddrOpnd::New((Js::Var)Js::ByteCodeReader::ReadAuxArray<double>(auxArrayOffset, m_functionBody), IR::AddrOpndKindDynamicMisc, m_func);
    case AuxArrayValue::AuxVarsArray:
        return IR::AddrOpnd::New((Js::Var)Js::ByteCodeReader::ReadAuxArray<Js::Var>(auxArrayOffset, m_functionBody), IR::AddrOpndKindDynamicMisc, m_func);
    case AuxArrayValue::AuxVarArrayVarCount:
        return IR::AddrOpnd::New((Js::Var)Js::ByteCodeReader::ReadVarArrayVarCount(auxArrayOffset, m_functionBody), IR::AddrOpndKindDynamicMisc, m_func);
    case AuxArrayValue::AuxFuncInfoArray:
        return IR::AddrOpnd::New((Js::Var)Js::ByteCodeReader::ReadAuxArray<Js::FuncInfoEntry>(auxArrayOffset, m_functionBody), IR::AddrOpndKindDynamicMisc, m_func);
    default:
        Assert(false);
        return nullptr;
    }
}

IR::Opnd *
IRBuilder::BuildAuxObjectLiteralTypeRefOpnd(int objectId, uint32 offset)
{
    return IR::AddrOpnd::New(m_func->GetJnFunction()->GetObjectLiteralTypeRef(objectId), IR::AddrOpndKindDynamicMisc, this->m_func);
}

void
IRBuilder::DoClosureRegCheck(Js::RegSlot reg)
{
    if (reg == Js::Constants::NoRegister)
    {
        return;
    }

    Js::FunctionBody * functionBody = this->m_func->GetJnFunction();
    if (reg == functionBody->GetEnvReg() ||
        reg == functionBody->GetLocalClosureReg() ||
        reg == functionBody->GetLocalFrameDisplayReg())
    {
        Js::Throw::FatalInternalError();
    }
}

Js::RegSlot
IRBuilder::InnerScopeIndexToRegSlot(uint32 index) const
{
    if (index >= m_func->GetJnFunction()->GetInnerScopeCount())
    {
        Js::Throw::FatalInternalError();
    }
    Js::RegSlot reg = m_func->GetJnFunction()->FirstInnerScopeReg() + index;
    if (reg >= m_func->GetJnFunction()->GetLocalsCount())
    {
        Js::Throw::FatalInternalError();
    }
    return reg;
}
