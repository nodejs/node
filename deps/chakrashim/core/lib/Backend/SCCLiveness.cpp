//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "BackEnd.h"

#include "SCCLiveness.h"


// Build SCC liveness.  SCC stands for Strongly Connected Components.  It's a simple
// conservative algorithm which has the advantage of being O(N).  A simple forward walk
// of the IR looks at the first and last use of each symbols and creates the lifetimes.
// The code assumes the blocks are in R-DFO order to start with.  For loops, the lifetimes
// are simply extended to cover the whole loop.
//
// The disadvantages are:
//      o Separate lifetimes of a given symbol are not separated
//      o Very conservative in loops, which is where we'd like precise info...
//
// Single-def symbols do not have the first issue.  We also try to make up for number 2
// by not extending the lifetime of symbols if the first def and the last use are in
// same loop.
//
// The code builds a list of lifetimes sorted in start order.
// We actually build the list in reverse start order, and then reverse it.

void
SCCLiveness::Build()
{
    // First, lets number each instruction to get an ordering.
    // Note that we assume the blocks are in RDFO.

    // NOTE: Currently the DoInterruptProbe pass will number the instructions. If it has,
    // then the numbering here is not necessary. But there should be no phase between the two
    // that can invalidate the numbering.
    if (!this->func->HasInstrNumber())
    {
        this->func->NumberInstrs();
    }

    IR::LabelInstr *lastLabelInstr = nullptr;

    FOREACH_INSTR_IN_FUNC_EDITING(instr, instrNext, this->func)
    {
        IR::Opnd *dst, *src1, *src2;
        uint32 instrNum = instr->GetNumber();

        // End of loop?
        if (this->curLoop && instrNum >= this->curLoop->regAlloc.loopEnd)
        {
            AssertMsg(this->loopNest > 0, "Loop nest is messed up");
            AssertMsg(instr->IsBranchInstr(), "Loop tail should be a branchInstr");
            AssertMsg(instr->AsBranchInstr()->IsLoopTail(this->func), "Loop tail not marked correctly");

            FOREACH_SLIST_ENTRY(Lifetime *, lifetime, this->curLoop->regAlloc.extendedLifetime)
            {
                if (this->curLoop->regAlloc.hasNonOpHelperCall)
                {
                    lifetime->isLiveAcrossUserCalls = true;
                }
                if (this->curLoop->regAlloc.hasCall)
                {
                    lifetime->isLiveAcrossCalls = true;
                }
                if(lifetime->end == this->curLoop->regAlloc.loopEnd)
                {
                    lifetime->totalOpHelperLengthByEnd = this->totalOpHelperFullVisitedLength + CurrentOpHelperVisitedLength(instr);
                }
            }
            NEXT_SLIST_ENTRY;

            this->curLoop->regAlloc.helperLength = this->totalOpHelperFullVisitedLength + CurrentOpHelperVisitedLength(instr);
            while (this->curLoop && instrNum >= this->curLoop->regAlloc.loopEnd)
            {
                this->curLoop = this->curLoop->parent;
                this->loopNest--;
            }
        }

        if (instr->HasBailOutInfo())
        {
            // At this point, the bailout should be lowered to a CALL to BailOut
#if DBG
            Assert(LowererMD::IsCall(instr));
            IR::Opnd * helperOpnd = nullptr;
            if (instr->GetSrc1()->IsHelperCallOpnd())
            {
                helperOpnd = instr->GetSrc1();
            }
            else if (instr->GetSrc1()->AsRegOpnd()->m_sym)
            {
                Assert(instr->GetSrc1()->AsRegOpnd()->m_sym->m_instrDef);
                helperOpnd = instr->GetSrc1()->AsRegOpnd()->m_sym->m_instrDef->GetSrc1();
            }
            Assert(!helperOpnd || BailOutInfo::IsBailOutHelper(helperOpnd->AsHelperCallOpnd()->m_fnHelper));
#endif
            ProcessBailOutUses(instr);
        }

        if (instr->m_opcode == Js::OpCode::InlineeEnd  && instr->m_func->m_hasInlineArgsOpt)
        {
            instr->m_func->frameInfo->IterateSyms([=](StackSym* argSym)
            {
                this->ProcessStackSymUse(argSym, instr);
            });
        }

        // Process srcs
        src1 = instr->GetSrc1();
        if (src1)
        {
            this->ProcessSrc(src1, instr);

            src2 = instr->GetSrc2();
            if (src2)
            {
                this->ProcessSrc(src2, instr);
            }
        }

        // Keep track of the last call instruction number to find out whether a lifetime crosses a call
        // Do not count call to bailout which exits anyways
        if (LowererMD::IsCall(instr) && !instr->HasBailOutInfo())
        {
            if (this->lastOpHelperLabel == nullptr)
            {
                // Catch only user calls (non op helper calls)
                this->lastNonOpHelperCall = instr->GetNumber();
                if (this->curLoop)
                {
                    this->curLoop->regAlloc.hasNonOpHelperCall = true;
                }
            }
            // Catch all calls
            this->lastCall = instr->GetNumber();
            if (this->curLoop)
            {
                this->curLoop->regAlloc.hasCall = true;
            }
        }

        // Process dst
        dst = instr->GetDst();
        if (dst)
        {
            this->ProcessDst(dst, instr);
        }


        if (instr->IsLabelInstr())
        {
            IR::LabelInstr * labelInstr = instr->AsLabelInstr();

            if (labelInstr->IsUnreferenced())
            {
                // Unreferenced labels can potentially be removed. See if the label tells
                // us we're transitioning between a helper and non-helper block.
                if (labelInstr->isOpHelper == (this->lastOpHelperLabel != nullptr)
                    && lastLabelInstr && labelInstr->isOpHelper == lastLabelInstr->isOpHelper)
                {
                    // No such transition. Remove the label.
                    Assert(!labelInstr->GetRegion() || labelInstr->GetRegion() == this->curRegion);
                    labelInstr->Remove();
                    continue;
                }
            }
            lastLabelInstr = labelInstr;

            Region * region = labelInstr->GetRegion();
            if (region != nullptr)
            {
                if (this->curRegion && this->curRegion != region)
                {
                    this->curRegion->SetEnd(labelInstr->m_prev);
                }
                if (region->GetStart() == nullptr)
                {
                    region->SetStart(labelInstr);
                }
                region->SetEnd(nullptr);
                this->curRegion = region;
            }
            else
            {
                labelInstr->SetRegion(this->curRegion);
            }

            // Look for start of loop
            if (labelInstr->m_isLoopTop)
            {
                this->loopNest++;       // used in spill cost calculation.

                IR::LabelInstr * labelInstr = instr->AsLabelInstr();
                uint32 lastBranchNum = 0;
                IR::BranchInstr *lastBranchInstr = nullptr;

                FOREACH_SLISTCOUNTED_ENTRY(IR::BranchInstr *, ref, &labelInstr->labelRefs)
                {
                    if (ref->GetNumber() > lastBranchNum)
                    {
                        lastBranchInstr = ref;
                        lastBranchNum = lastBranchInstr->GetNumber();
                    }
                }
                NEXT_SLISTCOUNTED_ENTRY;

                AssertMsg(instrNum < lastBranchNum, "Didn't find back edge...");
                AssertMsg(lastBranchInstr->IsLoopTail(this->func), "Loop tail not marked properly");

                Loop * loop = labelInstr->GetLoop();
                loop->parent = this->curLoop;
                this->curLoop = loop;
                loop->regAlloc.loopStart = instrNum;
                loop->regAlloc.loopEnd = lastBranchNum;

                // Tail duplication can result in cases in which an outer loop lexically ends before the inner loop.
                // The register allocator could then thrash in the inner loop registers used for a live-on-back-edge
                // sym on the outer loop. To prevent this, we need to mark the end of the outer loop as the end of the
                // inner loop and update the lifetimes already extended in the outer loop in keeping with this change.
                for (Loop* parentLoop = loop->parent; parentLoop != nullptr; parentLoop = parentLoop->parent)
                {
                    if (parentLoop->regAlloc.loopEnd < loop->regAlloc.loopEnd)
                    {
                        // We need to go over extended lifetimes in outer loops to update the lifetimes of symbols that might
                        // have had their lifetime extended to the outer loop end (which is before the current loop end) and
                        // may not have any uses in the current loop to extend their lifetimes to the current loop end.
                        FOREACH_SLIST_ENTRY(Lifetime *, lifetime, parentLoop->regAlloc.extendedLifetime)
                        {
                            if (lifetime->end == parentLoop->regAlloc.loopEnd)
                            {
                                lifetime->end = loop->regAlloc.loopEnd;
                            }
                        }
                        NEXT_SLIST_ENTRY;
                        parentLoop->regAlloc.loopEnd = loop->regAlloc.loopEnd;
                    }
                }
                loop->regAlloc.extendedLifetime = JitAnew(this->tempAlloc, SList<Lifetime *>, this->tempAlloc);
                loop->regAlloc.hasNonOpHelperCall = false;
                loop->regAlloc.hasCall = false;
                loop->regAlloc.hasAirLock = false;
            }

            // track whether we are in a helper block or not
            if (this->lastOpHelperLabel != nullptr)
            {
                this->EndOpHelper(labelInstr);
            }
            if (labelInstr->isOpHelper && !PHASE_OFF(Js::OpHelperRegOptPhase, this->func))
            {
                this->lastOpHelperLabel = labelInstr;
            }
        }
        else if (instr->IsBranchInstr() && !instr->AsBranchInstr()->IsMultiBranch())
        {
            IR::LabelInstr * branchTarget = instr->AsBranchInstr()->GetTarget();
            Js::OpCode brOpcode = instr->m_opcode;
            if (branchTarget->GetRegion() == nullptr && this->func->HasTry())
            {
                Assert(brOpcode != Js::OpCode::Leave && brOpcode != Js::OpCode::TryCatch && brOpcode != Js::OpCode::TryFinally);
                branchTarget->SetRegion(this->curRegion);
            }
        }
        if (this->lastOpHelperLabel != nullptr && instr->IsBranchInstr())
        {
            IR::LabelInstr *targetLabel = instr->AsBranchInstr()->GetTarget();

            if (targetLabel->isOpHelper && instr->AsBranchInstr()->IsConditional())
            {
                // If we have:
                //    L1: [helper]
                //           CMP
                //           JCC  helperLabel
                //           code
                // Insert a helper label before 'code' to mark this is also helper code.
                IR::Instr *branchInstrNext = instr->GetNextRealInstrOrLabel();
                if (!branchInstrNext->IsLabelInstr())
                {
                    instrNext = IR::LabelInstr::New(Js::OpCode::Label, instr->m_func, true);
                    instr->InsertAfter(instrNext);
                    instrNext->CopyNumber(instrNext->m_next);
                }
            }
            this->EndOpHelper(instr);
        }

    }NEXT_INSTR_IN_FUNC_EDITING;

    if (this->func->HasTry())
    {
#if DBG
        FOREACH_INSTR_IN_FUNC(instr, this->func)
        {
            if (instr->IsLabelInstr())
            {
                Assert(instr->AsLabelInstr()->GetRegion() != nullptr);
            }
        }
        NEXT_INSTR_IN_FUNC
#endif
        AssertMsg(this->curRegion, "Function with try but no regions?");
        AssertMsg(this->curRegion->GetStart() && !this->curRegion->GetEnd(), "Current region not active?");

        // Check for lifetimes that have been extended such that they now span multiple regions.
        this->curRegion->SetEnd(this->func->m_exitInstr);
        if (this->func->HasTry() && !this->func->DoOptimizeTryCatch())
        {
            FOREACH_SLIST_ENTRY(Lifetime *, lifetime, &this->lifetimeList)
            {
                if (lifetime->dontAllocate)
                {
                    continue;
                }
                if (lifetime->start < lifetime->region->GetStart()->GetNumber() ||
                    lifetime->end > lifetime->region->GetEnd()->GetNumber())
                {
                    lifetime->dontAllocate = true;
                }
            }
            NEXT_SLIST_ENTRY;
        }
    }

    AssertMsg(this->loopNest == 0, "LoopNest is messed up");

    // The list is built in reverse order.  Let's flip it in increasing start order.

    this->lifetimeList.Reverse();
    this->opHelperBlockList.Reverse();

#if DBG_DUMP
    if (PHASE_DUMP(Js::LivenessPhase, this->func))
    {
        this->Dump();
    }
#endif
}

void
SCCLiveness::EndOpHelper(IR::Instr * instr)
{
    Assert(this->lastOpHelperLabel != nullptr);

    OpHelperBlock * opHelperBlock = this->opHelperBlockList.PrependNode(this->tempAlloc);
    Assert(opHelperBlock != nullptr);
    opHelperBlock->opHelperLabel = this->lastOpHelperLabel;
    opHelperBlock->opHelperEndInstr = instr;

    this->totalOpHelperFullVisitedLength += opHelperBlock->Length();
    this->lastOpHelperLabel = nullptr;
}

// SCCLiveness::ProcessSrc
void
SCCLiveness::ProcessSrc(IR::Opnd *src, IR::Instr *instr)
{
    if (src->IsRegOpnd())
    {
        this->ProcessRegUse(src->AsRegOpnd(), instr);
    }
    else if (src->IsIndirOpnd())
    {
        IR::IndirOpnd *indirOpnd = src->AsIndirOpnd();

        AssertMsg(indirOpnd->GetBaseOpnd(), "Indir should have a base...");

        if (!this->FoldIndir(instr, indirOpnd))
        {
            this->ProcessRegUse(indirOpnd->GetBaseOpnd(), instr);

            if (indirOpnd->GetIndexOpnd())
            {
                this->ProcessRegUse(indirOpnd->GetIndexOpnd(), instr);
            }
        }
    }
}

// SCCLiveness::ProcessDst
void
SCCLiveness::ProcessDst(IR::Opnd *dst, IR::Instr *instr)
{
    if (dst->IsIndirOpnd())
    {
        // Indir regs are really uses

        IR::IndirOpnd *indirOpnd = dst->AsIndirOpnd();

        AssertMsg(indirOpnd->GetBaseOpnd(), "Indir should have a base...");

        if (!this->FoldIndir(instr, indirOpnd))
        {
            this->ProcessRegUse(indirOpnd->GetBaseOpnd(), instr);

            if (indirOpnd->GetIndexOpnd())
            {
                this->ProcessRegUse(indirOpnd->GetIndexOpnd(), instr);
            }
        }
    }
    else if (dst->IsRegOpnd())
    {
        this->ProcessRegDef(dst->AsRegOpnd(), instr);
    }
}

void
SCCLiveness::ProcessBailOutUses(IR::Instr * instr)
{
    BailOutInfo * bailOutInfo = instr->GetBailOutInfo();
    FOREACH_BITSET_IN_SPARSEBV(id, bailOutInfo->byteCodeUpwardExposedUsed)
    {
        StackSym * stackSym = this->func->m_symTable->FindStackSym(id);
        Assert(stackSym != nullptr);
        ProcessStackSymUse(stackSym, instr);
    }
    NEXT_BITSET_IN_SPARSEBV;

    FOREACH_SLISTBASE_ENTRY(CopyPropSyms, copyPropSyms, &bailOutInfo->usedCapturedValues.copyPropSyms)
    {
        ProcessStackSymUse(copyPropSyms.Value(), instr);
    }
    NEXT_SLISTBASE_ENTRY;


    bailOutInfo->IterateArgOutSyms([=] (uint, uint, StackSym* sym) {
        if(!sym->IsArgSlotSym() && sym->m_isBailOutReferenced)
        {
            ProcessStackSymUse(sym, instr);
        }
    });

    if(bailOutInfo->branchConditionOpnd)
    {
        ProcessSrc(bailOutInfo->branchConditionOpnd, instr);
    }

    // BailOnNoProfile might have caused the deletion of a cloned InlineeEnd. As a result, argument
    // lifetimes wouldn't have been extended beyond the bailout point (InlineeEnd extends the lifetimes)
    // Extend argument lifetimes up to the bail out point to allow LinearScan::SpillInlineeArgs to spill
    // inlinee args.
    if ((instr->GetBailOutKind() == IR::BailOutOnNoProfile) && !instr->m_func->IsTopFunc())
    {
        Func * inlinee = instr->m_func;
        while (!inlinee->IsTopFunc())
        {
            if (inlinee->m_hasInlineArgsOpt && inlinee->frameInfo->isRecorded)
            {
                inlinee->frameInfo->IterateSyms([=](StackSym* argSym)
                {
                    this->ProcessStackSymUse(argSym, instr);
                });
                inlinee = inlinee->GetParentFunc();
            }
            else
            {
                // if an inlinee's arguments haven't been optimized away, it's ancestors' shouldn't have been too.
                break;
            }
        }
    }
}

void
SCCLiveness::ProcessStackSymUse(StackSym * stackSym, IR::Instr * instr, int usageSize)
{
    Lifetime * lifetime = stackSym->scratch.linearScan.lifetime;

    if (lifetime == nullptr)
    {
#if DBG
        wchar_t debugStringBuffer[MAX_FUNCTION_BODY_DEBUG_STRING_SIZE];
        Output::Print(L"Function: %s (%s)       ", this->func->GetJnFunction()->GetDisplayName(), this->func->GetJnFunction()->GetDebugNumberSet(debugStringBuffer));
        Output::Print(L"Reg: ");
        stackSym->Dump();
        Output::Print(L"\n");
        Output::Flush();
#endif
        AnalysisAssertMsg(UNREACHED, "Uninitialized reg?");
    }
    else
    {
        if (lifetime->region != this->curRegion && !this->func->DoOptimizeTryCatch())
        {
            lifetime->dontAllocate = true;
        }

        ExtendLifetime(lifetime, instr);
    }
    lifetime->AddToUseCount(LinearScan::GetUseSpillCost(this->loopNest, (this->lastOpHelperLabel != nullptr)), this->curLoop, this->func);
    if (lifetime->start < this->lastCall)
    {
        lifetime->isLiveAcrossCalls = true;
    }
    if (lifetime->start < this->lastNonOpHelperCall)
    {
        lifetime->isLiveAcrossUserCalls = true;
    }
    lifetime->isDeadStore = false;

    lifetime->intUsageBv.Set(usageSize);
}

// SCCLiveness::ProcessRegUse
void
SCCLiveness::ProcessRegUse(IR::RegOpnd *regUse, IR::Instr *instr)
{
    StackSym * stackSym = regUse->m_sym;

    if (stackSym == nullptr)
    {
        return;
    }

    ProcessStackSymUse(stackSym, instr, TySize[regUse->GetType()]);

}

// SCCLiveness::ProcessRegDef
void
SCCLiveness::ProcessRegDef(IR::RegOpnd *regDef, IR::Instr *instr)
{
    StackSym * stackSym = regDef->m_sym;

    // PhysReg
    if (stackSym == nullptr || regDef->GetReg() != RegNOREG)
    {
        IR::Opnd *src = instr->GetSrc1();

        // If this symbol is assigned to a physical register, let's tell the register
        // allocator to prefer assigning that register to the lifetime.
        //
        // Note: this only pays off if this is the last-use of the symbol, but
        // unfortunately we don't have a way to tell that currently...
        if (LowererMD::IsAssign(instr) && src->IsRegOpnd() && src->AsRegOpnd()->m_sym)
        {
            StackSym *srcSym = src->AsRegOpnd()->m_sym;

            srcSym->scratch.linearScan.lifetime->regPreference.Set(regDef->GetReg());
        }

        // This physreg doesn't have a lifetime, just return.
        if (stackSym == nullptr)
        {
            return;
        }
    }

    // Arg slot sym can be in an RegOpnd for param passed via registers
    // Skip creating a lifetime for those.
    if (stackSym->IsArgSlotSym())
    {
        return;
    }

    // We'll extend the lifetime only if there are uses in a different loop region
    // from one of the defs.

    Lifetime * lifetime = stackSym->scratch.linearScan.lifetime;

    if (lifetime == nullptr)
    {
        lifetime = this->InsertLifetime(stackSym, regDef->GetReg(), instr);
        lifetime->region = this->curRegion;
        lifetime->isFloat = regDef->IsFloat();
        lifetime->isSimd128F4 = regDef->IsSimd128F4();
        lifetime->isSimd128I4 = regDef->IsSimd128I4();
        lifetime->isSimd128D2 = regDef->IsSimd128D2();
    }
    else
    {
        AssertMsg(lifetime->start <= instr->GetNumber(), "Lifetime start not set correctly");

        ExtendLifetime(lifetime, instr);

        if (lifetime->region != this->curRegion && !this->func->DoOptimizeTryCatch())
        {
            lifetime->dontAllocate = true;
        }
    }
    lifetime->AddToUseCount(LinearScan::GetUseSpillCost(this->loopNest, (this->lastOpHelperLabel != nullptr)), this->curLoop, this->func);
    lifetime->intUsageBv.Set(TySize[regDef->GetType()]);
}

// SCCLiveness::ExtendLifetime
//      Manages extend lifetimes to the end of loops if the corresponding symbol
//      is live on the back edge of the loop
void
SCCLiveness::ExtendLifetime(Lifetime *lifetime, IR::Instr *instr)
{
    AssertMsg(lifetime != nullptr, "Lifetime not provided");
    AssertMsg(lifetime->sym != nullptr, "Lifetime has no symbol");
    Assert(this->extendedLifetimesLoopList->Empty());

    // Find the loop that we need to extend the lifetime to
    StackSym * sym = lifetime->sym;
    Loop * loop = this->curLoop;
    uint32 extendedLifetimeStart = lifetime->start;
    uint32 extendedLifetimeEnd = lifetime->end;
    bool isLiveOnBackEdge = false;
    bool loopAddedToList = false;

    while (loop)
    {
        if (loop->regAlloc.liveOnBackEdgeSyms->Test(sym->m_id))
        {
            isLiveOnBackEdge = true;
            if (loop->regAlloc.loopStart < extendedLifetimeStart)
            {
                extendedLifetimeStart = loop->regAlloc.loopStart;
                this->extendedLifetimesLoopList->Prepend(this->tempAlloc, loop);
                loopAddedToList = true;
            }
            if (loop->regAlloc.loopEnd > extendedLifetimeEnd)
            {
                extendedLifetimeEnd = loop->regAlloc.loopEnd;
                if (!loopAddedToList)
                {
                    this->extendedLifetimesLoopList->Prepend(this->tempAlloc, loop);
                }
            }
        }
        loop = loop->parent;
        loopAddedToList = false;
    }

    if (!isLiveOnBackEdge)
    {
        // Don't extend lifetime to loop boundary if the use are not live on back edge
        // Note: the above loop doesn't detect a reg that is live on an outer back edge
        // but not an inner one, so we can't assume here that the lifetime hasn't been extended
        // past the current instruction.
        if (lifetime->end < instr->GetNumber())
        {
            lifetime->end = instr->GetNumber();
            lifetime->totalOpHelperLengthByEnd = this->totalOpHelperFullVisitedLength + CurrentOpHelperVisitedLength(instr);
        }
    }
    else
    {
        // extend lifetime to the outer most loop boundary that have the symbol live on back edge.
        bool isLifetimeExtended = false;
        if (lifetime->start > extendedLifetimeStart)
        {
            isLifetimeExtended = true;
            lifetime->start = extendedLifetimeStart;
        }

        if (lifetime->end < extendedLifetimeEnd)
        {
            isLifetimeExtended = true;
            lifetime->end = extendedLifetimeEnd;
            // The total op helper length by the end of this lifetime will be updated once we reach the loop tail
        }

        if (isLifetimeExtended)
        {
            // Keep track of the lifetime extended for this loop so we can update the call bits
            FOREACH_SLISTBASE_ENTRY(Loop *, loop, this->extendedLifetimesLoopList)
            {
                loop->regAlloc.extendedLifetime->Prepend(lifetime);
            }
            NEXT_SLISTBASE_ENTRY
        }
        AssertMsg(lifetime->end > instr->GetNumber(), "Lifetime end not set correctly");
    }
    this->extendedLifetimesLoopList->Clear(this->tempAlloc);
}

// SCCLiveness::InsertLifetime
//      Insert a new lifetime in the list of lifetime.  The lifetime are inserted
//      in the reverse order of the lifetime starts.
Lifetime *
SCCLiveness::InsertLifetime(StackSym *stackSym, RegNum reg, IR::Instr *const currentInstr)
{
    const uint start = currentInstr->GetNumber(), end = start;
    Lifetime * lifetime = JitAnew(tempAlloc, Lifetime, tempAlloc, stackSym, reg, start, end, this->func);
    lifetime->totalOpHelperLengthByEnd = this->totalOpHelperFullVisitedLength + CurrentOpHelperVisitedLength(currentInstr);

    // Find insertion point
    // This looks like a search, but we should almost exit on the first iteration, except
    // when we have loops and some lifetimes where extended.
    FOREACH_SLIST_ENTRY_EDITING(Lifetime *, lifetime, &this->lifetimeList, iter)
    {
        if (lifetime->start <= start)
        {
            break;
        }
    }
    NEXT_SLIST_ENTRY_EDITING;

    iter.InsertBefore(lifetime);

    // let's say 'var a = 10;'. if a is not used in the function, we still want to have the instr, otherwise the write-through will not happen and upon debug bailout
    // we would not be able to restore the values to see in locals window.
    if (this->func->IsJitInDebugMode() && stackSym->HasByteCodeRegSlot() && this->func->IsNonTempLocalVar(stackSym->GetByteCodeRegSlot()))
    {
        lifetime->isDeadStore = false;
    }

    stackSym->scratch.linearScan.lifetime = lifetime;
    return lifetime;
}

bool
SCCLiveness::FoldIndir(IR::Instr *instr, IR::Opnd *opnd)
{
#ifdef _M_ARM
    // Can't be folded on ARM
    return false;
#else
    IR::IndirOpnd *indir = opnd->AsIndirOpnd();

    if(indir->GetIndexOpnd())
    {
        IR::RegOpnd *index = indir->GetIndexOpnd();
        if (!index->m_sym || !index->m_sym->IsIntConst())
        {
            return false;
        }

        // offset = indir.offset + (index << scale)
        int32 offset = index->m_sym->GetIntConstValue();
        if(indir->GetScale() != 0 && Int32Math::Shl(offset, indir->GetScale(), &offset) ||
            indir->GetOffset() != 0 && Int32Math::Add(indir->GetOffset(), offset, &offset))
        {
            return false;
        }
        indir->SetOffset(offset);
        indir->SetIndexOpnd(nullptr);
    }

    IR::RegOpnd *base = indir->GetBaseOpnd();
    if (!base->m_sym || !base->m_sym->IsConst() || base->m_sym->IsIntConst() || base->m_sym->IsFloatConst())
    {
        return false;
    }

    uint8 *constValue = static_cast<uint8 *>(base->m_sym->GetConstAddress());
    if(indir->GetOffset() != 0)
    {
        if(indir->GetOffset() < 0 ? constValue + indir->GetOffset() > constValue : constValue + indir->GetOffset() < constValue)
        {
            return false;
        }
        constValue += indir->GetOffset();
    }

#ifdef _M_X64
    // Encoding only allows 32bits worth
    if(!Math::FitsInDWord((size_t)constValue))
    {
        return false;
    }
#endif

    IR::MemRefOpnd *memref = IR::MemRefOpnd::New(constValue, indir->GetType(), instr->m_func);

    if (indir == instr->GetDst())
    {
        instr->ReplaceDst(memref);
    }
    else
    {
        instr->ReplaceSrc(indir, memref);
    }
    return true;
#endif
}

uint SCCLiveness::CurrentOpHelperVisitedLength(IR::Instr *const currentInstr) const
{
    Assert(currentInstr);

    if(!lastOpHelperLabel)
    {
        return 0;
    }

    Assert(currentInstr->GetNumber() >= lastOpHelperLabel->GetNumber());
    uint visitedLength = currentInstr->GetNumber() - lastOpHelperLabel->GetNumber();
    if(!currentInstr->IsLabelInstr())
    {
        // Consider the current instruction to have been visited
        ++visitedLength;
    }
    return visitedLength;
}

#if DBG_DUMP

// SCCLiveness::Dump
void
SCCLiveness::Dump()
{
    this->func->DumpHeader();
    Output::Print(L"************   Liveness   ************\n");

    FOREACH_SLIST_ENTRY(Lifetime *, lifetime, &this->lifetimeList)
    {
        lifetime->sym->Dump();
        Output::Print(L": live range %3d - %3d (XUserCall: %d, XCall: %d)\n", lifetime->start, lifetime->end,
            lifetime->isLiveAcrossUserCalls,
            lifetime->isLiveAcrossCalls);
    }
    NEXT_SLIST_ENTRY;


    FOREACH_INSTR_IN_FUNC(instr, func)
    {
        Output::Print(L"%3d > ", instr->GetNumber());
        instr->Dump();
    } NEXT_INSTR_IN_FUNC;
}

#endif

uint OpHelperBlock::Length() const
{
    Assert(opHelperLabel);
    Assert(opHelperEndInstr);

    uint length = opHelperEndInstr->GetNumber() - opHelperLabel->GetNumber();
    if(!opHelperEndInstr->IsLabelInstr())
    {
        ++length;
    }
    return length;
}
