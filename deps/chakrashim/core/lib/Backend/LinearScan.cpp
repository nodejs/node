//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "BackEnd.h"

#include "SCCLiveness.h"

#if DBG_DUMP || ENABLE_DEBUG_CONFIG_OPTIONS
char const * const RegNames[RegNumCount] =
{
#define REGDAT(Name, ListName, ...) "" STRINGIZE(ListName) "",
#include "RegList.h"
#undef REGDAT
};

wchar_t const * const RegNamesW[RegNumCount] =
{
#define REGDAT(Name, ListName, ...) L"" STRINGIZEW(ListName) L"",
#include "RegList.h"
#undef REGDAT
};
#endif

static const uint8 RegAttribs[RegNumCount] =
{
#define REGDAT(Name, ListName, Encode, Type, Attribs) Attribs,
#include "RegList.h"
#undef REGDAT
};

extern const IRType RegTypes[RegNumCount] =
{
#define REGDAT(Name, ListName, Encode, Type, Attribs) Type,
#include "RegList.h"
#undef REGDAT
};


LoweredBasicBlock* LoweredBasicBlock::New(JitArenaAllocator* allocator)
{
    return JitAnew(allocator, LoweredBasicBlock, allocator);
}

void LoweredBasicBlock::Copy(LoweredBasicBlock* block)
{
    this->inlineeFrameLifetimes.Copy(&block->inlineeFrameLifetimes);
    this->inlineeStack.Copy(&block->inlineeStack);
    this->inlineeFrameSyms.Copy(&block->inlineeFrameSyms);
}

bool LoweredBasicBlock::HasData()
{
    return this->inlineeFrameLifetimes.Count() > 0 || this->inlineeStack.Count() > 0;
}

LoweredBasicBlock* LoweredBasicBlock::Clone(JitArenaAllocator* allocator)
{
    if (this->HasData())
    {
        LoweredBasicBlock* clone = LoweredBasicBlock::New(allocator);
        clone->Copy(this);
        return clone;
    }
    return nullptr;
}

bool LoweredBasicBlock::Equals(LoweredBasicBlock* otherBlock)
{
    if(this->HasData() != otherBlock->HasData())
    {
        return false;
    }
    if (!this->inlineeFrameLifetimes.Equals(&otherBlock->inlineeFrameLifetimes))
    {
        return false;
    }
    if (!this->inlineeStack.Equals(&otherBlock->inlineeStack))
    {
        return false;
    }
    return true;
}

// LinearScan::RegAlloc
// This register allocator is based on the 1999 linear scan register allocation paper
// by Poletto and Sarkar.  This code however walks the IR while doing the lifetime
// allocations, and assigns the regs to all the RegOpnd as it goes.  It assumes
// the IR is in R-DFO, and that the lifetime list is sorted in starting order.
// Lifetimes are allocated as they become live, and retired as they go dead.  RegOpnd
// are assigned their register.  If a lifetime becomes active and there are no free
// registers left, a lifetime is picked to be spilled.
// When we spill, the whole lifetime is spilled.  All the loads and stores are done
// through memory for that lifetime, even the ones allocated before the current instruction.
// We do optimize this slightly by not reloading the previous loads that were not in loops.

void
LinearScan::RegAlloc()
{
    NoRecoverMemoryJitArenaAllocator tempAlloc(L"BE-LinearScan", this->func->m_alloc->GetPageAllocator(), Js::Throw::OutOfMemory);
    this->tempAlloc = &tempAlloc;
    this->opHelperSpilledLiveranges = JitAnew(&tempAlloc, SList<Lifetime *>, &tempAlloc);
    this->activeLiveranges = JitAnew(&tempAlloc, SList<Lifetime *>, &tempAlloc);
    this->liveOnBackEdgeSyms = JitAnew(&tempAlloc, BVSparse<JitArenaAllocator>, &tempAlloc);
    this->stackPackInUseLiveRanges = JitAnew(&tempAlloc, SList<Lifetime *>, &tempAlloc);
    this->stackSlotsFreeList = JitAnew(&tempAlloc, SList<StackSlot *>, &tempAlloc);
    this->currentBlock = LoweredBasicBlock::New(&tempAlloc);

    IR::Instr *currentInstr = this->func->m_headInstr;

    SCCLiveness liveness(this->func, this->tempAlloc);
    BEGIN_CODEGEN_PHASE(this->func, Js::LivenessPhase);

    // Build the lifetime list
    liveness.Build();
    END_CODEGEN_PHASE(this->func, Js::LivenessPhase);


    this->lifetimeList = &liveness.lifetimeList;

    this->opHelperBlockList = &liveness.opHelperBlockList;
    this->opHelperBlockIter = SList<OpHelperBlock>::Iterator(this->opHelperBlockList);
    this->opHelperBlockIter.Next();

    this->Init();

    NativeCodeData::Allocator * nativeAllocator = this->func->GetNativeCodeDataAllocator();
    if (func->hasBailout)
    {
        this->globalBailOutRecordTables = NativeCodeDataNewArrayZ(nativeAllocator, GlobalBailOutRecordDataTable *,  func->m_inlineeId + 1);
        this->lastUpdatedRowIndices = JitAnewArrayZ(this->tempAlloc, uint *, func->m_inlineeId + 1);

#ifdef PROFILE_BAILOUT_RECORD_MEMORY
        if (Js::Configuration::Global.flags.ProfileBailOutRecordMemory)
        {
            this->func->GetScriptContext()->bailOutOffsetBytes += (sizeof(GlobalBailOutRecordDataTable *) * (func->m_inlineeId + 1));
            this->func->GetScriptContext()->bailOutRecordBytes += (sizeof(GlobalBailOutRecordDataTable *) * (func->m_inlineeId + 1));
        }
#endif
    }

    m_bailOutRecordCount = 0;
    IR::Instr * insertBailInAfter = nullptr;
    BailOutInfo * bailOutInfoForBailIn = nullptr;
    bool endOfBasicBlock = true;
    FOREACH_INSTR_EDITING(instr, instrNext, currentInstr)
    {
        if (instr->GetNumber() == 0)
        {
            AssertMsg(LowererMD::IsAssign(instr), "Only expect spill code here");
            continue;
        }

#if DBG_DUMP && defined(ENABLE_DEBUG_CONFIG_OPTIONS)
        if (Js::Configuration::Global.flags.Trace.IsEnabled(Js::LinearScanPhase, this->func->GetSourceContextId(), this->func->GetLocalFunctionId()))
        {
            instr->Dump();
        }
#endif // DBG

        this->currentInstr = instr;
        if(instr->StartsBasicBlock() || endOfBasicBlock)
        {
            endOfBasicBlock = false;
            ++currentBlockNumber;
        }

        if (instr->IsLabelInstr())
        {
            this->lastLabel = instr->AsLabelInstr();
            if (this->lastLabel->m_loweredBasicBlock)
            {
                this->currentBlock = this->lastLabel->m_loweredBasicBlock;
            }
            else if(currentBlock->HasData())
            {
                // Check if the previous block has fall-through. If so, retain the block info. If not, create empty info.
                IR::Instr *const prevInstr = instr->GetPrevRealInstrOrLabel();
                Assert(prevInstr);
                if(!prevInstr->HasFallThrough())
                {
                    currentBlock = LoweredBasicBlock::New(&tempAlloc);
                }
            }
            this->currentRegion = this->lastLabel->GetRegion();
        }
        else if (instr->IsBranchInstr())
        {
            if (this->func->HasTry() && this->func->DoOptimizeTryCatch())
            {
                this->ProcessEHRegionBoundary(instr);
            }
            this->ProcessSecondChanceBoundary(instr->AsBranchInstr());
        }

        this->CheckIfInLoop(instr);

        if (this->RemoveDeadStores(instr))
        {
            continue;
        }

        if (instr->HasBailOutInfo())
        {
            if (this->currentRegion)
            {
                RegionType curRegType = this->currentRegion->GetType();
                Assert(curRegType != RegionTypeFinally); //Finally regions are not optimized yet
                if (curRegType == RegionTypeTry || curRegType == RegionTypeCatch)
                {
                    this->func->hasBailoutInEHRegion = true;
                }
            }

            this->FillBailOutRecord(instr);
            if (instr->GetBailOutKind() == IR::BailOutForGeneratorYield)
            {
                Assert(instr->m_next->IsLabelInstr());
                insertBailInAfter = instr->m_next;
                bailOutInfoForBailIn = instr->GetBailOutInfo();
            }
        }

        this->SetSrcRegs(instr);
        this->EndDeadLifetimes(instr);

        this->CheckOpHelper(instr);

        this->KillImplicitRegs(instr);

        this->AllocateNewLifetimes(instr);
        this->SetDstReg(instr);

        this->EndDeadOpHelperLifetimes(instr);

        if (instr->IsLabelInstr())
        {
            this->ProcessSecondChanceBoundary(instr->AsLabelInstr());
        }

#if DBG
        this->CheckInvariants();
#endif // DBG

        if(instr->EndsBasicBlock())
        {
            endOfBasicBlock = true;
        }

        if (insertBailInAfter == instr)
        {
            instrNext = linearScanMD.GenerateBailInForGeneratorYield(instr, bailOutInfoForBailIn);
            insertBailInAfter = nullptr;
            bailOutInfoForBailIn = nullptr;
        }
    }NEXT_INSTR_EDITING;

    if (func->hasBailout)
    {
        for (uint i = 0; i <= func->m_inlineeId; i++)
        {
            if (globalBailOutRecordTables[i] != nullptr)
            {
                globalBailOutRecordTables[i]->Finalize(nativeAllocator, &tempAlloc);
#ifdef PROFILE_BAILOUT_RECORD_MEMORY
                if (Js::Configuration::Global.flags.ProfileBailOutRecordMemory)
                {
                    func->GetScriptContext()->bailOutOffsetBytes += sizeof(GlobalBailOutRecordDataRow) * globalBailOutRecordTables[i]->length;
                    func->GetScriptContext()->bailOutRecordBytes += sizeof(GlobalBailOutRecordDataRow) * globalBailOutRecordTables[i]->length;
                }
#endif
            }
        }
    }

    AssertMsg((this->intRegUsedCount + this->floatRegUsedCount) == this->linearScanMD.UnAllocatableRegCount(this->func) , "RegUsedCount is wrong");
    AssertMsg(this->activeLiveranges->Empty(), "Active list not empty");
    AssertMsg(this->stackPackInUseLiveRanges->Empty(), "Spilled list not empty");

    AssertMsg(!this->opHelperBlockIter.IsValid(), "Got to the end with a helper block still on the list?");

    Assert(this->currentBlock->inlineeStack.Count() == 0);
    this->InsertOpHelperSpillAndRestores();

#if _M_IX86
# if ENABLE_DEBUG_CONFIG_OPTIONS
    if (Js::Configuration::Global.flags.Instrument.IsEnabled(Js::LinearScanPhase, this->func->GetJnFunction()->GetSourceContextId(),this->func->GetJnFunction()->GetLocalFunctionId()))
    {
        this->DynamicStatsInstrument();
    }
# endif
#endif

#if DBG_DUMP
    if (PHASE_STATS(Js::LinearScanPhase, this->func))
    {
        this->PrintStats();
    }
    if (PHASE_TRACE(Js::StackPackPhase, this->func))
    {
        Output::Print(L"---------------------------\n");
    }
#endif // DBG_DUMP
    DebugOnly(this->func->allowRemoveBailOutArgInstr = true);
}

JitArenaAllocator *
LinearScan::GetTempAlloc()
{
    Assert(tempAlloc);
    return tempAlloc;
}

#if DBG
void
LinearScan::CheckInvariants() const
{
    BitVector bv = this->nonAllocatableRegs;
    uint32 lastend = 0;
    FOREACH_SLIST_ENTRY(Lifetime *, lifetime, this->activeLiveranges)
    {
        // Make sure there are only one lifetime per reg
        Assert(!bv.Test(lifetime->reg));
        bv.Set(lifetime->reg);
        Assert(!lifetime->isOpHelperSpilled);
        Assert(!lifetime->isSpilled);
        Assert(lifetime->end >= lastend);
        lastend = lifetime->end;
    }
    NEXT_SLIST_ENTRY;

    // Make sure the active reg bit vector is correct
    Assert(bv.Equal(this->activeRegs));

    uint ints = 0, floats = 0;
    FOREACH_BITSET_IN_UNITBV(index, this->activeRegs, BitVector)
    {
        if (IRType_IsFloat(RegTypes[index]))
        {
            floats++;
        }
        else
        {
            ints++;
        }
    }
    NEXT_BITSET_IN_UNITBV;
    Assert(ints == this->intRegUsedCount);
    Assert(floats == this->floatRegUsedCount);
    Assert((this->intRegUsedCount + this->floatRegUsedCount) == this->activeRegs.Count());

    bv.ClearAll();

    lastend = 0;
    FOREACH_SLIST_ENTRY(Lifetime *, lifetime, this->opHelperSpilledLiveranges)
    {
        // Make sure there are only one lifetime per reg in the op helper spilled liveranges
        Assert(!bv.Test(lifetime->reg));
        if (!lifetime->cantOpHelperSpill)
        {
            bv.Set(lifetime->reg);
            Assert(lifetime->isOpHelperSpilled);
            Assert(!lifetime->isSpilled);
        }
        Assert(lifetime->end >= lastend);
        lastend = lifetime->end;
    }
    NEXT_SLIST_ENTRY;

    // Make sure the opHelperSpilledRegs bit vector is correct
    Assert(bv.Equal(this->opHelperSpilledRegs));

    for (int i = 0; i < RegNumCount; i++)
    {
        if (this->tempRegs.Test(i))
        {
            Assert(this->tempRegLifetimes[i]->reg == i);
        }
    }

    FOREACH_BITSET_IN_UNITBV(reg, this->secondChanceRegs, BitVector)
    {
        Lifetime *lifetime = this->regContent[reg];
        Assert(lifetime);
        StackSym *sym = lifetime->sym;
        Assert(lifetime->isSecondChanceAllocated);
        Assert(sym->IsConst() || sym->IsAllocated());  // Should have been spilled already.
    } NEXT_BITSET_IN_UNITBV;

}
#endif // DBG

// LinearScan::Init
// Initialize bit vectors
void
LinearScan::Init()
{
    FOREACH_REG(reg)
    {
        // Registers that can't be used are set to active, and will remain this way
        if (!LinearScan::IsAllocatable(reg))
        {
            this->activeRegs.Set(reg);
            if (IRType_IsFloat(RegTypes[reg]))
            {
                this->floatRegUsedCount++;
            }
            else
            {
                this->intRegUsedCount++;
            }
        }
        if (RegTypes[reg] == TyMachReg)
        {
            // JIT64_TODO: Rename int32Regs to machIntRegs.
            this->int32Regs.Set(reg);
            numInt32Regs++;
        }
        else if (RegTypes[reg] == TyFloat64)
        {
            this->floatRegs.Set(reg);
            numFloatRegs++;
        }
        if (LinearScan::IsCallerSaved(reg))
        {
            this->callerSavedRegs.Set(reg);
        }
        if (LinearScan::IsCalleeSaved(reg))
        {
            this->calleeSavedRegs.Set(reg);
        }
        this->regContent[reg] = nullptr;
    } NEXT_REG;

    this->instrUseRegs.ClearAll();
    this->secondChanceRegs.ClearAll();

    this->linearScanMD.Init(this);

#if DBG
    this->nonAllocatableRegs = this->activeRegs;
#endif

#if DBG_DUMP
    if (PHASE_TRACE(Js::LinearScanPhase, this->func))
    {
        this->func->DumpHeader();
    }
#endif
}

// LinearScan::CheckIfInLoop
// Track whether the current instruction is in a loop or not.
bool
LinearScan::CheckIfInLoop(IR::Instr *instr)
{
    if (this->IsInLoop())
    {
        // Look for end of loop

        AssertMsg(this->curLoop->regAlloc.loopEnd != 0, "Something is wrong here....");

        if (instr->GetNumber() >= this->curLoop->regAlloc.loopEnd)
        {
            AssertMsg(instr->IsBranchInstr(), "Loop tail should be a branchInstr");
            while (this->IsInLoop() && instr->GetNumber() >= this->curLoop->regAlloc.loopEnd)
            {
                this->loopNest--;
                this->curLoop->isProcessed = true;
                this->curLoop = this->curLoop->parent;
                if (this->loopNest == 0)
                {
                    this->liveOnBackEdgeSyms->ClearAll();
                }
            }
        }
    }
    if (instr->IsLabelInstr() && instr->AsLabelInstr()->m_isLoopTop)
    {
        IR::LabelInstr * labelInstr = instr->AsLabelInstr();
        Loop *parentLoop = this->curLoop;
        if (parentLoop)
        {
            parentLoop->isLeaf = false;
        }
        this->curLoop = labelInstr->GetLoop();
        this->curLoop->isProcessed = false;
        // Lexically nested may not always nest in a flow based way:
        //      while(i--) {
        //          if (cond) {
        //              while(j--) {
        //              }
        //              break;
        //          }
        //      }
        // These look nested, but they are not...
        // So update the flow based parent to be lexical or we won't be able to figure out when we get back
        // to the outer loop.
        // REVIEW: This isn't necessary anymore now that break blocks are moved out of the loops.

        this->curLoop->parent = parentLoop;
        this->curLoop->regAlloc.defdInLoopBv = JitAnew(this->tempAlloc, BVSparse<JitArenaAllocator>, this->tempAlloc);
        this->curLoop->regAlloc.symRegUseBv = JitAnew(this->tempAlloc, BVSparse<JitArenaAllocator>, this->tempAlloc);
        this->curLoop->regAlloc.loopStart = labelInstr->GetNumber();
        this->curLoop->regAlloc.exitRegContentList = JitAnew(this->tempAlloc, SList<Lifetime **>, this->tempAlloc);
        this->curLoop->regAlloc.regUseBv = 0;

        this->liveOnBackEdgeSyms->Or(this->curLoop->regAlloc.liveOnBackEdgeSyms);
        this->loopNest++;
    }
    return this->IsInLoop();
}

void
LinearScan::InsertOpHelperSpillAndRestores()
{
    linearScanMD.InsertOpHelperSpillAndRestores(opHelperBlockList);
}

void
LinearScan::CheckOpHelper(IR::Instr *instr)
{
    if (this->IsInHelperBlock())
    {
        if (this->currentOpHelperBlock->opHelperEndInstr == instr)
        {
            // Get targetInstr if we can.
            // We can deterministically get it only for unconditional branches, as conditional branch may fall through.
            IR::Instr * targetInstr = nullptr;
            if (instr->IsBranchInstr() && instr->AsBranchInstr()->IsUnconditional())
            {
                AssertMsg(!instr->AsBranchInstr()->IsMultiBranch(), "Not supported for Multibranch");
                targetInstr = instr->AsBranchInstr()->GetTarget();
            }

            /*
             * Keep track of the number of registers we've had to
             * store and restore around a helper block for LinearScanMD (on ARM
             * and X64). We need this to be able to allocate space in the frame.
             * We can't emit a PUSH/POP sequence around the block like IA32 because
             * the stack pointer can't move outside the prolog.
             */
            uint32 helperSpilledLiverangeCount = 0;

            // Exiting a helper block.  We are going to insert
            // the restore here after linear scan.  So put all the restored
            // lifetime back to active
            while (!this->opHelperSpilledLiveranges->Empty())
            {
                Lifetime * lifetime = this->opHelperSpilledLiveranges->Pop();
                lifetime->isOpHelperSpilled = false;

                if (!lifetime->cantOpHelperSpill)
                {
                    // Put the life time back to active
                    this->AssignActiveReg(lifetime, lifetime->reg);
                    bool reload = true;
                    // Lifetime ends before the target after helper block, don't need to save and restore helper spilled lifetime.
                    if (targetInstr && lifetime->end < targetInstr->GetNumber())
                    {
                        // However, if lifetime is spilled as arg - we still need to spill it because the helper assumes the value
                        // to be available in the stack
                        if (lifetime->isOpHelperSpillAsArg)
                        {
                            // we should not attempt to restore it as it is dead on return from the helper.
                            reload = false;
                        }
                        else
                        {
                            Assert(!instr->AsBranchInstr()->IsLoopTail(this->func));
                            continue;
                        }
                    }

                    // Save all the lifetime that needs to be restored
                    OpHelperSpilledLifetime spilledLifetime;
                    spilledLifetime.lifetime = lifetime;
                    spilledLifetime.spillAsArg = lifetime->isOpHelperSpillAsArg;
                    spilledLifetime.reload = reload;
                    /*
                     * Can't unfortunately move this into the else block above because we don't know if this
                     * lifetime will actually get spilled until register allocation completes.
                     * Instead we allocate a slot to this StackSym in LinearScanMD iff
                     * !(lifetime.isSpilled && lifetime.noReloadsIfSpilled).
                     */
                    helperSpilledLiverangeCount++;

                    // save the reg in case it is spilled later.  We still need to save and restore
                    // for the non-loop case.
                    spilledLifetime.reg = lifetime->reg;
                    this->currentOpHelperBlock->spilledLifetime.Prepend(spilledLifetime);
                }
                else
                {
                    // Clear it for the next helper block
                    lifetime->cantOpHelperSpill = false;
                }
                lifetime->isOpHelperSpillAsArg = false;
            }

            this->totalOpHelperFullVisitedLength += this->currentOpHelperBlock->Length();

            // Use a dummy label as the insertion point of the reloads, as second-chance-allocation
            // may insert compensation code right before the branch
            IR::PragmaInstr *dummyLabel = IR::PragmaInstr::New(Js::OpCode::Nop, 0, this->func);
            this->currentOpHelperBlock->opHelperEndInstr->InsertBefore(dummyLabel);
            dummyLabel->CopyNumber(this->currentOpHelperBlock->opHelperEndInstr);
            this->currentOpHelperBlock->opHelperEndInstr = dummyLabel;

            this->opHelperSpilledRegs.ClearAll();
            this->currentOpHelperBlock = nullptr;

            linearScanMD.EndOfHelperBlock(helperSpilledLiverangeCount);
        }
    }

    if (this->opHelperBlockIter.IsValid())
    {
        AssertMsg(
            !instr->IsLabelInstr() ||
            !instr->AsLabelInstr()->isOpHelper ||
            this->opHelperBlockIter.Data().opHelperLabel == instr,
            "Found a helper label that doesn't begin the next helper block in the list?");

        if (this->opHelperBlockIter.Data().opHelperLabel == instr)
        {
            this->currentOpHelperBlock = &this->opHelperBlockIter.Data();
            this->opHelperBlockIter.Next();
        }
    }
}

uint
LinearScan::HelperBlockStartInstrNumber() const
{
    Assert(IsInHelperBlock());
    return this->currentOpHelperBlock->opHelperLabel->GetNumber();
}

uint
LinearScan::HelperBlockEndInstrNumber() const
{
    Assert(IsInHelperBlock());
    return this->currentOpHelperBlock->opHelperEndInstr->GetNumber();
}
// LinearScan::AddToActive
// Add a lifetime to the active list.  The list is kept sorted in order lifetime end.
// This makes it easier to pick the lifetimes to retire.
void
LinearScan::AddToActive(Lifetime * lifetime)
{
    LinearScan::AddLiveRange(this->activeLiveranges, lifetime);
    this->regContent[lifetime->reg] = lifetime;
    if (lifetime->isSecondChanceAllocated)
    {
        this->secondChanceRegs.Set(lifetime->reg);
    }
    else
    {
        Assert(!this->secondChanceRegs.Test(lifetime->reg));
    }
}

void
LinearScan::AddOpHelperSpilled(Lifetime * lifetime)
{
    RegNum reg = lifetime->reg;
    Assert(this->IsInHelperBlock());
    Assert(!this->opHelperSpilledRegs.Test(reg));
    Assert(lifetime->isOpHelperSpilled == false);
    Assert(lifetime->cantOpHelperSpill == false);


    this->opHelperSpilledRegs.Set(reg);
    lifetime->isOpHelperSpilled = true;

    this->regContent[reg] = nullptr;
    this->secondChanceRegs.Clear(reg);

    // If a lifetime is being OpHelper spilled and it's an inlinee arg sym
    // we need to make sure its spilled to the sym offset spill space, i.e. isOpHelperSpillAsArg
    // is set. Otherwise, it's value will not be available on inline frame reconstruction.
    if (this->currentBlock->inlineeFrameSyms.Count() > 0 &&
        this->currentBlock->inlineeFrameSyms.ContainsKey(lifetime->sym->m_id) &&
        (lifetime->sym->m_isSingleDef || !lifetime->defList.Empty()))
    {
        lifetime->isOpHelperSpillAsArg = true;
        if (!lifetime->sym->IsAllocated())
        {
            this->AllocateStackSpace(lifetime);
        }
        this->RecordLoopUse(lifetime, lifetime->reg);
    }
    LinearScan::AddLiveRange(this->opHelperSpilledLiveranges, lifetime);
}

void
LinearScan::RemoveOpHelperSpilled(Lifetime * lifetime)
{
    Assert(this->IsInHelperBlock());
    Assert(lifetime->isOpHelperSpilled);
    Assert(lifetime->cantOpHelperSpill == false);
    Assert(this->opHelperSpilledRegs.Test(lifetime->reg));

    this->opHelperSpilledRegs.Clear(lifetime->reg);
    lifetime->isOpHelperSpilled = false;
    lifetime->cantOpHelperSpill = false;
    lifetime->isOpHelperSpillAsArg = false;
    this->opHelperSpilledLiveranges->Remove(lifetime);
}

void
LinearScan::SetCantOpHelperSpill(Lifetime * lifetime)
{
    Assert(this->IsInHelperBlock());
    Assert(lifetime->isOpHelperSpilled);
    Assert(lifetime->cantOpHelperSpill == false);

    this->opHelperSpilledRegs.Clear(lifetime->reg);
    lifetime->isOpHelperSpilled = false;
    lifetime->cantOpHelperSpill = true;
}

void
LinearScan::AddLiveRange(SList<Lifetime *> * list, Lifetime * newLifetime)
{
    FOREACH_SLIST_ENTRY_EDITING(Lifetime *, lifetime, list, iter)
    {
        if (newLifetime->end < lifetime->end)
        {
            break;
        }
    }
    NEXT_SLIST_ENTRY_EDITING;

    iter.InsertBefore(newLifetime);
}

Lifetime *
LinearScan::RemoveRegLiveRange(SList<Lifetime *> * list, RegNum reg)
{
    // Find the register in the active set
    FOREACH_SLIST_ENTRY_EDITING(Lifetime *, lifetime, list, iter)
    {
        if (lifetime->reg == reg)
        {
            Lifetime * lifetimeReturn = lifetime;
            iter.RemoveCurrent();
            return lifetimeReturn;
        }
    } NEXT_SLIST_ENTRY_EDITING;

    AssertMsg(false, "Can't find life range for a reg");
    return nullptr;
}


// LinearScan::SetDstReg
// Set the reg on each RegOpnd def.
void
LinearScan::SetDstReg(IR::Instr *instr)
{
    //
    // Enregister dst
    //

    IR::Opnd *dst = instr->GetDst();

    if (dst == nullptr)
    {
        return;
    }

    if (!dst->IsRegOpnd())
    {
        // This could be, for instance, a store to a sym with a large offset
        // that was just assigned when we saw the use.
        this->linearScanMD.LegalizeDef(instr);
        return;
    }

    IR::RegOpnd * regOpnd = dst->AsRegOpnd();
    /*
     * If this is a register used to setup a callsite per
     * a calling convention then mark it unavailable to allocate
     * until we see a CALL.
     */
    if (regOpnd->m_isCallArg)
    {
        RegNum callSetupReg = regOpnd->GetReg();
        callSetupRegs.Set(callSetupReg);
    }

    StackSym * stackSym = regOpnd->m_sym;

    // Arg slot sym can be in an RegOpnd for param passed via registers
    // Just use the assigned register
    if (stackSym == nullptr || stackSym->IsArgSlotSym())
    {
        //
        // Already allocated register. just spill the destination
        //
        RegNum reg = regOpnd->GetReg();
        if(LinearScan::IsAllocatable(reg))
        {
            this->SpillReg(reg);
        }
        this->tempRegs.Clear(reg);
    }
    else
    {
        if (regOpnd->GetReg() != RegNOREG)
        {
            this->RecordLoopUse(nullptr, regOpnd->GetReg());
            // Nothing to do
            return;
        }

        Lifetime * lifetime = stackSym->scratch.linearScan.lifetime;
        uint32 useCountCost = LinearScan::GetUseSpillCost(this->loopNest, (this->currentOpHelperBlock != nullptr));
        // Optimistically decrease the useCount.  We'll undo this if we put it on the defList.
        lifetime->SubFromUseCount(useCountCost, this->curLoop);

        if (lifetime->isSpilled)
        {
            if (stackSym->IsConst() && !IsSymNonTempLocalVar(stackSym))
            {
                // We will reload the constant (but in debug mode, we still need to process this if this is a user var).
                return;
            }

            RegNum reg = regOpnd->GetReg();

            if (reg != RegNOREG)
            {
                // It is already assigned, just record it as a temp reg
                this->AssignTempReg(lifetime, reg);
            }
            else
            {
                IR::Opnd *src1 = instr->GetSrc1();
                IR::Opnd *src2 = instr->GetSrc2();

                if ((src1 && src1->IsRegOpnd() && src1->AsRegOpnd()->m_sym == stackSym) ||
                    (src2 && src2->IsRegOpnd() && src2->AsRegOpnd()->m_sym == stackSym))
                {
                    // OpEQ: src1 should have a valid reg (put src2 for other targets)
                    reg = this->GetAssignedTempReg(lifetime, dst->GetType());
                    Assert(reg != RegNOREG);
                    RecordDef(lifetime, instr, 0);
                }
                else
                {
                    // Try second chance
                    reg = this->SecondChanceAllocation(lifetime, false);
                    if (reg != RegNOREG)
                    {

                        Assert(!stackSym->m_isSingleDef);

                        this->SetReg(regOpnd);
                        // Keep track of defs for this lifetime, in case it gets spilled.
                        RecordDef(lifetime, instr, useCountCost);
                        return;
                    }
                    else
                    {
                        reg = this->GetAssignedTempReg(lifetime, dst->GetType());
                        RecordDef(lifetime, instr, 0);
                    }
                }
                if (LowererMD::IsAssign(instr) && instr->GetSrc1()->IsRegOpnd())
                {
                    // Fold the spilled store
                    if (reg != RegNOREG)
                    {
                        // If the value is in a temp reg, it's not valid any more.
                        this->tempRegs.Clear(reg);
                    }

                    IRType srcType = instr->GetSrc1()->GetType();

                    instr->ReplaceDst(IR::SymOpnd::New(stackSym, srcType, this->func));
                    this->linearScanMD.LegalizeDef(instr);
                    return;
                }


                if (reg == RegNOREG)
                {
                    IR::Opnd *src = instr->GetSrc1();
                    if (src && src->IsRegOpnd() && src->AsRegOpnd()->m_sym == stackSym)
                    {
                        // Handle OPEQ's for x86/x64
                        reg = src->AsRegOpnd()->GetReg();
                        AssertMsg(!this->activeRegs.Test(reg), "Shouldn't be active");
                    }
                    else
                    {
                        // The lifetime was spilled, but we still need a reg for this operand.
                        reg = this->FindReg(nullptr, regOpnd);
                    }
                    this->AssignTempReg(lifetime, reg);
                }
            }

            if (!lifetime->isDeadStore && !lifetime->isSecondChanceAllocated)
            {
                // Insert a store since the lifetime is spilled
                IR::Opnd *nextDst = instr->m_next->GetDst();

                // Don't need the store however if the next instruction has the same dst
                if (nextDst == nullptr || !nextDst->IsEqual(regOpnd))
                {
                    this->InsertStore(instr, regOpnd->m_sym, reg);
                }
            }
        }
        else
        {
            if (lifetime->isOpHelperSpilled)
            {
                // We must be in a helper block and the lifetime must
                // start before the helper block
                Assert(this->IsInHelperBlock());
                Assert(lifetime->start < this->HelperBlockStartInstrNumber());

                RegNum reg = lifetime->reg;
                Assert(this->opHelperSpilledRegs.Test(reg));

                if (this->activeRegs.Test(reg))
                {
                    // The reg must have been used locally in the helper block
                    // by some other lifetime. Just spill it

                    this->SpillReg(reg);
                }

                // We can't save/restore this reg across the helper call because the restore would overwrite
                // this def, but the def means we don't need to spill at all.  Mark the lifetime as cantOpHelperSpill
                // however in case another helper call in this block tries to spill it.
                this->SetCantOpHelperSpill(lifetime);

                this->AddToActive(lifetime);

                this->tempRegs.Clear(reg);
                this->activeRegs.Set(reg);
                if (RegTypes[reg] == TyMachReg)
                {
                    this->intRegUsedCount++;
                }
                else
                {
                    Assert(RegTypes[reg] == TyFloat64);
                    this->floatRegUsedCount++;
                }
            }

            // Keep track of defs for this lifetime, in case it gets spilled.
            RecordDef(lifetime, instr, useCountCost);
        }

        this->SetReg(regOpnd);
    }
}

// Get the stack offset of the non temp locals from the stack.
int32 LinearScan::GetStackOffset(Js::RegSlot regSlotId)
{
    Assert(this->func->GetJnFunction());

    int32 stackSlotId = regSlotId - this->func->GetJnFunction()->GetFirstNonTempLocalIndex();
    Assert(stackSlotId >= 0);
    return this->func->GetLocalVarSlotOffset(stackSlotId);
}


//
// This helper function is used for saving bytecode stack sym value to memory / local slots on stack so that we can read it for the locals inspection.
void
LinearScan::WriteThroughForLocal(IR::RegOpnd* regOpnd, Lifetime* lifetime, IR::Instr* instrInsertAfter)
{
    Assert(regOpnd);
    Assert(lifetime);
    Assert(instrInsertAfter);

    StackSym* sym = regOpnd->m_sym;
    Assert(IsSymNonTempLocalVar(sym));

    Js::RegSlot slotIndex = sym->GetByteCodeRegSlot();

    // First we insert the write through moves

    sym->m_offset = GetStackOffset(slotIndex);
    sym->m_allocated = true;
    // Save the value on reg to local var slot.
    this->InsertStore(instrInsertAfter, sym, lifetime->reg);
}

bool
LinearScan::NeedsWriteThrough(StackSym * sym)
{
    return this->NeedsWriteThroughForEH(sym) || this->IsSymNonTempLocalVar(sym);
}

bool
LinearScan::NeedsWriteThroughForEH(StackSym * sym)
{
    if (!this->func->HasTry() || !this->func->DoOptimizeTryCatch() || !sym->HasByteCodeRegSlot())
    {
        return false;
    }

    Assert(this->currentRegion);
    return this->currentRegion->writeThroughSymbolsSet && this->currentRegion->writeThroughSymbolsSet->Test(sym->m_id);
}

// Helper routine to check if current sym belongs to non temp bytecodereg
bool
LinearScan::IsSymNonTempLocalVar(StackSym *sym)
{
    Assert(sym);

    if (this->func->IsJitInDebugMode() && sym->HasByteCodeRegSlot())
    {
        Js::RegSlot slotIndex = sym->GetByteCodeRegSlot();

        return this->func->IsNonTempLocalVar(slotIndex);
    }
    return false;
}


// LinearScan::SetSrcRegs
// Set the reg on each RegOpnd use.
// Note that this includes regOpnd of indir dsts...
void
LinearScan::SetSrcRegs(IR::Instr *instr)
{
    //
    // Enregister srcs
    //

    IR::Opnd *src1 = instr->GetSrc1();

    if (src1 != nullptr)
    {
        // Capture src2 now as folding in SetUses could swab the srcs...
        IR::Opnd *src2 = instr->GetSrc2();

        this->SetUses(instr, src1);

        if (src2 != nullptr)
        {
            this->SetUses(instr, src2);
        }
    }

    IR::Opnd *dst = instr->GetDst();

    if (dst && dst->IsIndirOpnd())
    {
        this->SetUses(instr, dst);
    }

    this->instrUseRegs.ClearAll();
}

// LinearScan::SetUses
void
LinearScan::SetUses(IR::Instr *instr, IR::Opnd *opnd)
{
    switch (opnd->GetKind())
    {
    case IR::OpndKindReg:
        this->SetUse(instr, opnd->AsRegOpnd());
        break;

    case IR::OpndKindSym:
        {
            Sym * sym = opnd->AsSymOpnd()->m_sym;
            if (sym->IsStackSym())
            {
                StackSym* stackSym = sym->AsStackSym();
                if (!stackSym->IsAllocated())
                {
                    func->StackAllocate(stackSym, opnd->GetSize());
                    // StackSym's lifetime is allocated during SCCLiveness::ProcessDst
                    // we might not need to set the flag if the sym is not a dst.
                    if (stackSym->scratch.linearScan.lifetime)
                    {
                        stackSym->scratch.linearScan.lifetime->cantStackPack = true;
                    }
                }
                this->linearScanMD.LegalizeUse(instr, opnd);
            }
        }
        break;
    case IR::OpndKindIndir:
        {
            IR::IndirOpnd * indirOpnd = opnd->AsIndirOpnd();

            this->SetUse(instr, indirOpnd->GetBaseOpnd());

            if (indirOpnd->GetIndexOpnd())
            {
                this->SetUse(instr, indirOpnd->GetIndexOpnd());
            }
        }
        break;
    case IR::OpndKindIntConst:
    case IR::OpndKindAddr:
        this->linearScanMD.LegalizeConstantUse(instr, opnd);
        break;
    };
}

struct FillBailOutState
{
    SListCounted<Js::Var> constantList;
    uint registerSaveCount;
    StackSym * registerSaveSyms[RegNumCount - 1];

    FillBailOutState(JitArenaAllocator * allocator) : constantList(allocator) {}
};


void
LinearScan::FillBailOutOffset(int * offset, StackSym * stackSym, FillBailOutState * state, IR::Instr * instr)
{
    AssertMsg(*offset == 0, "Can't have two active lifetime for the same byte code register");
    if (stackSym->IsConst())
    {
        state->constantList.Prepend(reinterpret_cast<Js::Var>(stackSym->GetLiteralConstValue_PostGlobOpt()));

        // Constant offset are offset by the number of register save slots
        *offset = state->constantList.Count() + GetBailOutRegisterSaveSlotCount() + GetBailOutReserveSlotCount();
    }
    else if (stackSym->m_isEncodedConstant)
    {
        Assert(!stackSym->m_isSingleDef);
        state->constantList.Prepend((Js::Var)stackSym->constantValue);

        // Constant offset are offset by the number of register save slots
        *offset = state->constantList.Count() + GetBailOutRegisterSaveSlotCount() + GetBailOutReserveSlotCount();
    }
    else
    {
        Lifetime * lifetime = stackSym->scratch.linearScan.lifetime;
        Assert(lifetime && lifetime->start < instr->GetNumber() && instr->GetNumber() <= lifetime->end);
        if (instr->GetBailOutKind() == IR::BailOutOnException)
        {
            // Apart from the exception object sym, lifetimes for all other syms that need to be restored at this bailout,
            // must have been spilled at least once (at the TryCatch, or at the Leave, or both)
            // Post spilling, a lifetime could have been second chance allocated. But, it should still have stack allocated for its sym
            Assert(stackSym->IsAllocated() || (stackSym == this->currentRegion->GetExceptionObjectSym()));
        }

        this->PrepareForUse(lifetime);
        if (lifetime->isSpilled ||
            ((instr->GetBailOutKind() == IR::BailOutOnException) && (stackSym != this->currentRegion->GetExceptionObjectSym()))) // BailOutOnException must restore from memory
        {
            Assert(stackSym->IsAllocated());
#ifdef MD_GROW_LOCALS_AREA_UP
            *offset = -((int)stackSym->m_offset + BailOutInfo::StackSymBias);
#else
            // Stack offset are negative, includes the PUSH EBP and return address
            *offset = stackSym->m_offset - (2 * MachPtr);
#endif
        }
        else
        {
            Assert(lifetime->reg != RegNOREG);
            Assert(state->registerSaveSyms[lifetime->reg - 1] == nullptr ||
                state->registerSaveSyms[lifetime->reg - 1] == stackSym);
            AssertMsg((stackSym->IsFloat64() || stackSym->IsSimd128()) && RegTypes[lifetime->reg] == TyFloat64 ||
                !(stackSym->IsFloat64() || stackSym->IsSimd128()) && RegTypes[lifetime->reg] != TyFloat64,
                      "Trying to save float64 sym into non-float64 reg or non-float64 sym into float64 reg");

            // Save the register value to the register save space using the reg enum value as index
            state->registerSaveSyms[lifetime->reg - 1] = stackSym;
            *offset = LinearScanMD::GetRegisterSaveIndex(lifetime->reg);

            state->registerSaveCount++;
        }
    }
}

struct FuncBailOutData
{
    Func * func;
    BailOutRecord * bailOutRecord;
    int * localOffsets;
    BVFixed * losslessInt32Syms;
    BVFixed * float64Syms;

    // SIMD_JS
    BVFixed * simd128F4Syms;
    BVFixed * simd128I4Syms;

    void Initialize(Func * func, JitArenaAllocator * tempAllocator);
    void FinalizeLocalOffsets(JitArenaAllocator *allocator, GlobalBailOutRecordDataTable *table, uint **lastUpdatedRowIndices);
    void Clear(JitArenaAllocator * tempAllocator);
};

void
FuncBailOutData::Initialize(Func * func, JitArenaAllocator * tempAllocator)
{
    Js::RegSlot localsCount = func->GetJnFunction()->GetLocalsCount();
    this->func = func;
    this->localOffsets = AnewArrayZ(tempAllocator, int, localsCount);
    this->losslessInt32Syms = BVFixed::New(localsCount, tempAllocator);
    this->float64Syms = BVFixed::New(localsCount, tempAllocator);
    // SIMD_JS
    this->simd128F4Syms = BVFixed::New(localsCount, tempAllocator);
    this->simd128I4Syms = BVFixed::New(localsCount, tempAllocator);
}

void
FuncBailOutData::FinalizeLocalOffsets(JitArenaAllocator *allocator, GlobalBailOutRecordDataTable *globalBailOutRecordDataTable, uint **lastUpdatedRowIndices)
{
    Js::RegSlot localsCount = func->GetJnFunction()->GetLocalsCount();

    Assert(globalBailOutRecordDataTable != nullptr);
    Assert(lastUpdatedRowIndices != nullptr);

    if (*lastUpdatedRowIndices == nullptr)
    {
        *lastUpdatedRowIndices = JitAnewArrayZ(allocator, uint, localsCount);
        memset(*lastUpdatedRowIndices, -1, sizeof(uint)*localsCount);
    }
    uint32 bailOutRecordId = bailOutRecord->m_bailOutRecordId;
    bailOutRecord->localOffsetsCount = 0;
    for (uint32 i = 0; i < localsCount; i++)
    {
        // if the sym is live
        if (localOffsets[i] != 0)
        {
            bool isFloat = float64Syms->Test(i) != 0;
            bool isInt = losslessInt32Syms->Test(i) != 0;

            // SIMD_JS
            bool isSimd128F4 = simd128F4Syms->Test(i) != 0;
            bool isSimd128I4 = simd128I4Syms->Test(i) != 0;

            globalBailOutRecordDataTable->AddOrUpdateRow(allocator, bailOutRecordId, i, isFloat, isInt, isSimd128F4, isSimd128I4, localOffsets[i], &((*lastUpdatedRowIndices)[i]));
            Assert(globalBailOutRecordDataTable->globalBailOutRecordDataRows[(*lastUpdatedRowIndices)[i]].regSlot  == i);
            bailOutRecord->localOffsetsCount++;
        }
    }
}

void
FuncBailOutData::Clear(JitArenaAllocator * tempAllocator)
{
    Js::RegSlot localsCount = func->GetJnFunction()->GetLocalsCount();
    JitAdeleteArray(tempAllocator, localsCount, localOffsets);
    losslessInt32Syms->Delete(tempAllocator);
    float64Syms->Delete(tempAllocator);
    // SIMD_JS
    simd128F4Syms->Delete(tempAllocator);
    simd128I4Syms->Delete(tempAllocator);
}

GlobalBailOutRecordDataTable *
LinearScan::EnsureGlobalBailOutRecordTable(Func *func)
{
    Assert(globalBailOutRecordTables != nullptr);
    Func *topFunc = func->GetTopFunc();
    bool isTopFunc = (func == topFunc);
    uint32 inlineeID = isTopFunc ? 0 : func->m_inlineeId;
    NativeCodeData::Allocator * allocator = this->func->GetNativeCodeDataAllocator();

    GlobalBailOutRecordDataTable *globalBailOutRecordDataTable = globalBailOutRecordTables[inlineeID];
    if (globalBailOutRecordDataTable == nullptr)
    {
        globalBailOutRecordDataTable = globalBailOutRecordTables[inlineeID] = NativeCodeDataNew(allocator, GlobalBailOutRecordDataTable);
        globalBailOutRecordDataTable->length = globalBailOutRecordDataTable->size = 0;
        globalBailOutRecordDataTable->isInlinedFunction = !isTopFunc;
        globalBailOutRecordDataTable->isInlinedConstructor = func->IsInlinedConstructor();
        globalBailOutRecordDataTable->isLoopBody = topFunc->IsLoopBody();
        globalBailOutRecordDataTable->returnValueRegSlot = func->returnValueRegSlot;
        globalBailOutRecordDataTable->firstActualStackOffset = -1;
        globalBailOutRecordDataTable->registerSaveSpace = func->GetScriptContext()->GetThreadContext()->GetBailOutRegisterSaveSpace();
        globalBailOutRecordDataTable->globalBailOutRecordDataRows = nullptr;

#ifdef PROFILE_BAILOUT_RECORD_MEMORY
        if (Js::Configuration::Global.flags.ProfileBailOutRecordMemory)
        {
            topFunc->GetScriptContext()->bailOutOffsetBytes += sizeof(GlobalBailOutRecordDataTable);
            topFunc->GetScriptContext()->bailOutRecordBytes += sizeof(GlobalBailOutRecordDataTable);
        }
#endif
    }
    return globalBailOutRecordDataTable;
}

void
LinearScan::FillBailOutRecord(IR::Instr * instr)
{
    BailOutInfo * bailOutInfo = instr->GetBailOutInfo();

    if (this->func->HasTry())
    {
        RegionType currentRegionType = this->currentRegion->GetType();
        if (currentRegionType == RegionTypeTry || currentRegionType == RegionTypeCatch)
        {
            bailOutInfo->bailOutRecord->ehBailoutData = this->currentRegion->ehBailoutData;
        }
    }

    BVSparse<JitArenaAllocator> * byteCodeUpwardExposedUsed = bailOutInfo->byteCodeUpwardExposedUsed;

    Func * bailOutFunc = bailOutInfo->bailOutFunc;
    uint funcCount = bailOutFunc->inlineDepth + 1;
    FuncBailOutData * funcBailOutData = AnewArray(this->tempAlloc, FuncBailOutData, funcCount);
    uint index = funcCount - 1;
    funcBailOutData[index].Initialize(bailOutFunc, this->tempAlloc);
    funcBailOutData[index].bailOutRecord = bailOutInfo->bailOutRecord;
    bailOutInfo->bailOutRecord->m_bailOutRecordId = m_bailOutRecordCount++;
    bailOutInfo->bailOutRecord->globalBailOutRecordTable = EnsureGlobalBailOutRecordTable(bailOutFunc);

    NativeCodeData::Allocator * allocator = this->func->GetNativeCodeDataAllocator();

#if DBG_DUMP
    if(PHASE_DUMP(Js::BailOutPhase, this->func))
    {
        Output::Print(L"-------------------Bailout dump -------------------------\n");
        instr->Dump();
    }
#endif


    // Generate chained bailout record for inlined functions
    Func * currentFunc = bailOutFunc->GetParentFunc();
    uint bailOutOffset = bailOutFunc->postCallByteCodeOffset;
    while (currentFunc != nullptr)
    {
        Assert(index > 0);
        Assert(bailOutOffset != Js::Constants::NoByteCodeOffset);
        BailOutRecord * bailOutRecord = NativeCodeDataNewZ(allocator, BailOutRecord, bailOutOffset, (uint)-1, IR::BailOutInvalid, currentFunc);
        bailOutRecord->m_bailOutRecordId = m_bailOutRecordCount++;
        bailOutRecord->globalBailOutRecordTable = EnsureGlobalBailOutRecordTable(currentFunc);
#if ENABLE_DEBUG_CONFIG_OPTIONS
        // To indicate this is a subsequent bailout from an inlinee
        bailOutRecord->bailOutOpcode = Js::OpCode::InlineeEnd;
#endif
        funcBailOutData[index].bailOutRecord->parent = bailOutRecord;
        index--;
        funcBailOutData[index].bailOutRecord = bailOutRecord;
        funcBailOutData[index].Initialize(currentFunc, this->tempAlloc);

        bailOutOffset = currentFunc->postCallByteCodeOffset;
        currentFunc = currentFunc->GetParentFunc();
    }

    Assert(index == 0);
    Assert(bailOutOffset == Js::Constants::NoByteCodeOffset);

    FillBailOutState state(this->tempAlloc);
    state.registerSaveCount = 0;
    memset(state.registerSaveSyms, 0, sizeof(state.registerSaveSyms));

    // Fill in the constants
    FOREACH_SLISTBASE_ENTRY_EDITING(ConstantStackSymValue, value, &bailOutInfo->usedCapturedValues.constantValues, iterator)
    {
        AssertMsg(bailOutInfo->bailOutRecord->bailOutKind != IR::BailOutForGeneratorYield, "constant prop syms unexpected for bail-in for generator yield");
        StackSym * stackSym = value.Key();
        if(stackSym->HasArgSlotNum())
        {
            continue;
        }
        Assert(stackSym->HasByteCodeRegSlot());
        Js::RegSlot i = stackSym->GetByteCodeRegSlot();
        Func * stackSymFunc = stackSym->GetByteCodeFunc();
        uint index = stackSymFunc->inlineDepth;

        Assert(i != Js::Constants::NoRegister);
        Assert(i < stackSymFunc->GetJnFunction()->GetLocalsCount());

        Assert(index < funcCount);
        __analysis_assume(index < funcCount);
        Assert(funcBailOutData[index].func == stackSymFunc);

        Assert(!byteCodeUpwardExposedUsed->Test(stackSym->m_id));

        BailoutConstantValue constValue = value.Value();
        Js::Var varValue = constValue.ToVar(this->func, stackSymFunc->GetScriptContext());

        state.constantList.Prepend(varValue);
        AssertMsg(funcBailOutData[index].localOffsets[i] == 0, "Can't have two active lifetime for the same byte code register");
        // Constant offset are offset by the number of register save slots
        funcBailOutData[index].localOffsets[i] = state.constantList.Count() + GetBailOutRegisterSaveSlotCount() + GetBailOutReserveSlotCount();

#if DBG_DUMP
        if(PHASE_DUMP(Js::BailOutPhase, this->func))
        {
            Output::Print(L"Constant stack sym #%d (argOut:%s): ",  i, IsTrueOrFalse(stackSym->HasArgSlotNum()));
            stackSym->Dump();
            Output::Print(L" (0x%p (Var) Offset: %d)\n", varValue, funcBailOutData[index].localOffsets[i]);
        }
#endif
        iterator.RemoveCurrent(this->func->m_alloc);
    }
    NEXT_SLISTBASE_ENTRY_EDITING;

    // Fill in the copy prop syms
    FOREACH_SLISTBASE_ENTRY_EDITING(CopyPropSyms, copyPropSyms, &bailOutInfo->usedCapturedValues.copyPropSyms, iter)
    {
        AssertMsg(bailOutInfo->bailOutRecord->bailOutKind != IR::BailOutForGeneratorYield, "copy prop syms unexpected for bail-in for generator yield");
        StackSym * stackSym = copyPropSyms.Key();
        if(stackSym->HasArgSlotNum())
        {
            continue;
        }
        Js::RegSlot i = stackSym->GetByteCodeRegSlot();
        Func * stackSymFunc = stackSym->GetByteCodeFunc();
        uint index = stackSymFunc->inlineDepth;

        Assert(i != Js::Constants::NoRegister);
        Assert(i < stackSymFunc->GetJnFunction()->GetLocalsCount());

        Assert(index < funcCount);
        __analysis_assume(index < funcCount);
        Assert(funcBailOutData[index].func == stackSymFunc);

        AssertMsg(funcBailOutData[index].localOffsets[i] == 0, "Can't have two active lifetime for the same byte code register");
        Assert(!byteCodeUpwardExposedUsed->Test(stackSym->m_id));

        StackSym * copyStackSym = copyPropSyms.Value();
        this->FillBailOutOffset(&funcBailOutData[index].localOffsets[i], copyStackSym, &state, instr);
        if (copyStackSym->IsInt32())
        {
            funcBailOutData[index].losslessInt32Syms->Set(i);
        }
        else if (copyStackSym->IsFloat64())
        {
            funcBailOutData[index].float64Syms->Set(i);
        }
        // SIMD_JS
        else if (copyStackSym->IsSimd128F4())
        {
            funcBailOutData[index].simd128F4Syms->Set(i);
        }
        else if (copyStackSym->IsSimd128I4())
        {
            funcBailOutData[index].simd128I4Syms->Set(i);
        }
        iter.RemoveCurrent(this->func->m_alloc);
    }
    NEXT_SLISTBASE_ENTRY_EDITING;

    // Fill in the upward exposed syms
    FOREACH_BITSET_IN_SPARSEBV(id, byteCodeUpwardExposedUsed)
    {
        StackSym * stackSym = this->func->m_symTable->FindStackSym(id);
        Assert(stackSym != nullptr);
        Js::RegSlot i = stackSym->GetByteCodeRegSlot();
        Func * stackSymFunc = stackSym->GetByteCodeFunc();
        uint index = stackSymFunc->inlineDepth;

        Assert(i != Js::Constants::NoRegister);
        Assert(i < stackSymFunc->GetJnFunction()->GetLocalsCount());

        Assert(index < funcCount);
         __analysis_assume(index < funcCount);
        Assert(funcBailOutData[index].func == stackSymFunc);

        AssertMsg(funcBailOutData[index].localOffsets[i] == 0, "Can't have two active lifetime for the same byte code register");

        this->FillBailOutOffset(&funcBailOutData[index].localOffsets[i], stackSym, &state, instr);
        if (stackSym->IsInt32())
        {
            funcBailOutData[index].losslessInt32Syms->Set(i);
        }
        else if (stackSym->IsFloat64())
        {
            funcBailOutData[index].float64Syms->Set(i);
        }
        // SIMD_JS
        else if (stackSym->IsSimd128F4())
        {
            funcBailOutData[index].simd128F4Syms->Set(i);
        }
        else if (stackSym->IsSimd128I4())
        {
            funcBailOutData[index].simd128I4Syms->Set(i);
        }
    }
    NEXT_BITSET_IN_SPARSEBV;

    if (bailOutInfo->usedCapturedValues.argObjSyms)
    {
        FOREACH_BITSET_IN_SPARSEBV(id, bailOutInfo->usedCapturedValues.argObjSyms)
        {
            StackSym * stackSym = this->func->m_symTable->FindStackSym(id);
            Assert(stackSym != nullptr);
            Js::RegSlot i = stackSym->GetByteCodeRegSlot();
            Func * stackSymFunc = stackSym->GetByteCodeFunc();
            uint index = stackSymFunc->inlineDepth;

            Assert(i != Js::Constants::NoRegister);
            Assert(i < stackSymFunc->GetJnFunction()->GetLocalsCount());

            Assert(index < funcCount);
            __analysis_assume(index < funcCount);

            Assert(funcBailOutData[index].func == stackSymFunc);
            AssertMsg(funcBailOutData[index].localOffsets[i] == 0, "Can't have two active lifetime for the same byte code register");

            funcBailOutData[index].localOffsets[i] =  BailOutRecord::GetArgumentsObjectOffset();
        }
        NEXT_BITSET_IN_SPARSEBV;
    }

    // In the debug mode, fill in the rest of non temp locals as well in the records so that the restore stub will just get it automatically.

    if (this->func->IsJitInDebugMode())
    {
        Js::FunctionBody* functionBody = this->func->GetJnFunction();
        Assert(functionBody);

        // Need to allow filling the formal args slots.

        Js::PropertyIdOnRegSlotsContainer *propertyIdContainer = functionBody->GetPropertyIdOnRegSlotsContainer();
        bool hasFormalArgs = propertyIdContainer != nullptr && propertyIdContainer->propertyIdsForFormalArgs != nullptr;

        if (hasFormalArgs)
        {
            for (uint32 index = functionBody->GetFirstNonTempLocalIndex(); index < functionBody->GetEndNonTempLocalIndex(); index++)
            {
                StackSym * stackSym = this->func->m_symTable->FindStackSym(index);
                if (stackSym != nullptr)
                {
                    Func * stackSymFunc = stackSym->GetByteCodeFunc();

                    Js::RegSlot regSlotId = stackSym->GetByteCodeRegSlot();
                    if (functionBody->IsNonTempLocalVar(regSlotId))
                    {
                        if (!propertyIdContainer->IsRegSlotFormal(regSlotId - functionBody->GetFirstNonTempLocalIndex()))
                        {
                            continue;
                        }

                        uint dataIndex = stackSymFunc->inlineDepth;
                        Assert(dataIndex == 0);     // There is no inlining while in debug mode

                        // Filling in which are not filled already.
                        __analysis_assume(dataIndex == 0);
                        if (funcBailOutData[dataIndex].localOffsets[regSlotId] == 0)
                        {
                            int32 offset = GetStackOffset(regSlotId);

#ifdef MD_GROW_LOCALS_AREA_UP
                            Assert(offset >= 0);
#else
                            Assert(offset < 0);
#endif

                            funcBailOutData[dataIndex].localOffsets[regSlotId] = this->func->AdjustOffsetValue(offset);

                            // We don't support typespec for debug, rework on the bellow assert once we start support them.
                            Assert(!stackSym->IsInt32() && !stackSym->IsFloat64() && !stackSym->IsSimd128());
                        }
                    }
                }
            }
        }
    }

    // fill in the out params
    uint startCallCount = bailOutInfo->startCallCount;

    if (bailOutInfo->totalOutParamCount != 0)
    {
        Assert(startCallCount != 0);
        uint argOutSlot = 0;
        uint * startCallOutParamCounts = NativeCodeDataNewArray(allocator, uint, startCallCount);
#ifdef _M_IX86
        uint * startCallArgRestoreAdjustCounts = NativeCodeDataNewArray(allocator, uint, startCallCount);
#endif
        BVFixed * argOutFloat64Syms = BVFixed::New(bailOutInfo->totalOutParamCount, allocator);
        BVFixed * argOutLosslessInt32Syms = BVFixed::New(bailOutInfo->totalOutParamCount, allocator);
        // SIMD_JS
        BVFixed * argOutSimd128F4Syms = BVFixed::New(bailOutInfo->totalOutParamCount, allocator);
        BVFixed * argOutSimd128I4Syms = BVFixed::New(bailOutInfo->totalOutParamCount, allocator);

        int* outParamOffsets = bailOutInfo->outParamOffsets = NativeCodeDataNewArrayZ(allocator, int, bailOutInfo->totalOutParamCount);
#ifdef _M_IX86
        int currentStackOffset = 0;
        bailOutInfo->outParamFrameAdjustArgSlot = JitAnew(this->func->m_alloc, BVSparse<JitArenaAllocator>, this->func->m_alloc);
#endif
        if (this->func->HasInlinee())
        {
            bailOutInfo->outParamInlinedArgSlot = JitAnew(this->func->m_alloc, BVSparse<JitArenaAllocator>, this->func->m_alloc);
        }

#if DBG
        uint lastFuncIndex = 0;
#endif
        for (uint i = 0; i < startCallCount; i++)
        {
            uint outParamStart = argOutSlot;                     // Start of the out param offset for the current start call
            // Number of out param for the current start call
            uint outParamCount = bailOutInfo->GetStartCallOutParamCount(i);
            startCallOutParamCounts[i] = outParamCount;
#ifdef _M_IX86
            startCallArgRestoreAdjustCounts[i] = bailOutInfo->startCallInfo[i].argRestoreAdjustCount;
            // Only x86 has a progression of pushes of out args, with stack alignment.
            bool fDoStackAdjust = false;
            if (!bailOutInfo->inlinedStartCall->Test(i))
            {
                // Only do the stack adjustment if the StartCall has not been moved down past the bailout.
                fDoStackAdjust = bailOutInfo->NeedsStartCallAdjust(i, instr);
                if (fDoStackAdjust)
                {
                    currentStackOffset -= Math::Align<int>(outParamCount * MachPtr, MachStackAlignment);
                }
            }
#endif

            Func * currentStartCallFunc = bailOutInfo->startCallFunc[i];
#if DBG
            Assert(lastFuncIndex <= currentStartCallFunc->inlineDepth);
            lastFuncIndex = currentStartCallFunc->inlineDepth;
#endif

            FuncBailOutData& currentFuncBailOutData = funcBailOutData[currentStartCallFunc->inlineDepth];
            BailOutRecord * currentBailOutRecord = currentFuncBailOutData.bailOutRecord;
            if (currentBailOutRecord->argOutOffsetInfo == nullptr)
            {
                currentBailOutRecord->argOutOffsetInfo = NativeCodeDataNew(allocator, BailOutRecord::ArgOutOffsetInfo);
                currentBailOutRecord->argOutOffsetInfo->argOutFloat64Syms = nullptr;
                currentBailOutRecord->argOutOffsetInfo->argOutLosslessInt32Syms = nullptr;
                // SIMD_JS
                currentBailOutRecord->argOutOffsetInfo->argOutSimd128F4Syms = nullptr;
                currentBailOutRecord->argOutOffsetInfo->argOutSimd128I4Syms = nullptr;
                currentBailOutRecord->argOutOffsetInfo->argOutSymStart = 0;
                currentBailOutRecord->argOutOffsetInfo->outParamOffsets = nullptr;
                currentBailOutRecord->argOutOffsetInfo->startCallOutParamCounts = nullptr;

#ifdef PROFILE_BAILOUT_RECORD_MEMORY
                if (Js::Configuration::Global.flags.ProfileBailOutRecordMemory)
                {
                    this->func->GetScriptContext()->bailOutRecordBytes += sizeof(BailOutRecord::ArgOutOffsetInfo);
                }
#endif
            }

            currentBailOutRecord->argOutOffsetInfo->startCallCount++;
            if (currentBailOutRecord->argOutOffsetInfo->outParamOffsets == nullptr)
            {
                Assert(currentBailOutRecord->argOutOffsetInfo->startCallOutParamCounts == nullptr);
                currentBailOutRecord->argOutOffsetInfo->startCallOutParamCounts = &startCallOutParamCounts[i];
#ifdef _M_IX86
                currentBailOutRecord->startCallArgRestoreAdjustCounts = &startCallArgRestoreAdjustCounts[i];
#endif
                currentBailOutRecord->argOutOffsetInfo->outParamOffsets = &outParamOffsets[outParamStart];
                currentBailOutRecord->argOutOffsetInfo->argOutSymStart = outParamStart;
                currentBailOutRecord->argOutOffsetInfo->argOutFloat64Syms = argOutFloat64Syms;
                currentBailOutRecord->argOutOffsetInfo->argOutLosslessInt32Syms = argOutLosslessInt32Syms;
                // SIMD_JS
                currentBailOutRecord->argOutOffsetInfo->argOutSimd128F4Syms = argOutSimd128F4Syms;
                currentBailOutRecord->argOutOffsetInfo->argOutSimd128I4Syms = argOutSimd128I4Syms;


            }
#if DBG_DUMP
            if (PHASE_DUMP(Js::BailOutPhase, this->func))
            {
                Output::Print(L"Bailout function: %s [#%d] \n", currentStartCallFunc->GetJnFunction()->GetDisplayName(),
                    currentStartCallFunc->GetJnFunction()->GetFunctionNumber());
            }
#endif
            for (uint j = 0; j < outParamCount; j++, argOutSlot++)
            {
                StackSym * sym = bailOutInfo->argOutSyms[argOutSlot];
                if (sym == nullptr)
                {
                    // This can happen when instr with bailout occurs before all ArgOuts for current call instr are processed.
                    continue;
                }

                Assert(sym->GetArgSlotNum() > 0 && sym->GetArgSlotNum() <= outParamCount);
                uint argSlot = sym->GetArgSlotNum() - 1;
                uint outParamOffsetIndex = outParamStart + argSlot;
                if (!sym->m_isBailOutReferenced && !sym->IsArgSlotSym())
                {
                    FOREACH_SLISTBASE_ENTRY_EDITING(ConstantStackSymValue, constantValue, &bailOutInfo->usedCapturedValues.constantValues, iterator)
                    {
                        if (constantValue.Key()->m_id == sym->m_id)
                        {
                            Js::Var varValue = constantValue.Value().ToVar(func, currentStartCallFunc->GetScriptContext());
                            state.constantList.Prepend(varValue);
                            outParamOffsets[outParamOffsetIndex] = state.constantList.Count() + GetBailOutRegisterSaveSlotCount() + GetBailOutReserveSlotCount();

#if DBG_DUMP
                            if (PHASE_DUMP(Js::BailOutPhase, this->func))
                            {
                                Output::Print(L"OutParam #%d: ", argSlot);
                                sym->Dump();
                                Output::Print(L" (0x%p (Var)))\n", varValue);
                            }
#endif
                            iterator.RemoveCurrent(func->m_alloc);
                            break;
                        }
                    }
                    NEXT_SLISTBASE_ENTRY_EDITING;
                    if (outParamOffsets[outParamOffsetIndex])
                    {
                        continue;
                    }

                    FOREACH_SLISTBASE_ENTRY_EDITING(CopyPropSyms, copyPropSym, &bailOutInfo->usedCapturedValues.copyPropSyms, iter)
                    {
                        if (copyPropSym.Key()->m_id == sym->m_id)
                        {
                            StackSym * copyStackSym = copyPropSym.Value();

                            BVSparse<JitArenaAllocator>* argObjSyms = bailOutInfo->usedCapturedValues.argObjSyms;
                            if (argObjSyms && argObjSyms->Test(copyStackSym->m_id))
                            {
                                outParamOffsets[outParamOffsetIndex] = BailOutRecord::GetArgumentsObjectOffset();
                            }
                            else
                            {
                                this->FillBailOutOffset(&outParamOffsets[outParamOffsetIndex], copyStackSym, &state, instr);
                                if (copyStackSym->IsInt32())
                                {
                                    argOutLosslessInt32Syms->Set(outParamOffsetIndex);
                                }
                                else if (copyStackSym->IsFloat64())
                                {
                                    argOutFloat64Syms->Set(outParamOffsetIndex);
                                }
                                // SIMD_JS
                                else if (copyStackSym->IsSimd128F4())
                                {
                                    argOutSimd128F4Syms->Set(outParamOffsetIndex);
                                }
                                else if (copyStackSym->IsSimd128I4())
                                {
                                    argOutSimd128I4Syms->Set(outParamOffsetIndex);
                                }
                            }
#if DBG_DUMP
                            if (PHASE_DUMP(Js::BailOutPhase, this->func))
                            {
                                Output::Print(L"OutParam #%d: ", argSlot);
                                sym->Dump();
                                Output::Print(L" Copy Prop sym:");
                                copyStackSym->Dump();
                                Output::Print(L"\n");
                            }
#endif
                            iter.RemoveCurrent(func->m_alloc);
                            break;
                        }
                    }
                    NEXT_SLISTBASE_ENTRY_EDITING;
                    Assert(outParamOffsets[outParamOffsetIndex] != 0);
                }
                else
                {
                    if (sym->IsArgSlotSym())
                    {
                        if (sym->m_isSingleDef)
                        {
                            Assert(sym->m_instrDef->m_func == currentStartCallFunc);

                            IR::Instr * instrDef = sym->m_instrDef;
                            Assert(LowererMD::IsAssign(instrDef));

                            if (instrDef->GetNumber() < instr->GetNumber())
                            {
                                // The ArgOut instr is above current bailout instr.
                                AssertMsg(sym->IsVar(), "Arg out slot can only be var.");
                                if (sym->m_isInlinedArgSlot)
                                {
                                    Assert(this->func->HasInlinee());
#ifdef MD_GROW_LOCALS_AREA_UP
                                    outParamOffsets[outParamOffsetIndex] = -((int)sym->m_offset + BailOutInfo::StackSymBias);
#else
                                    outParamOffsets[outParamOffsetIndex] = sym->m_offset;
#endif
                                    bailOutInfo->outParamInlinedArgSlot->Set(outParamOffsetIndex);
                                }
                                else if (sym->m_isOrphanedArg)
                                {
#ifdef MD_GROW_LOCALS_AREA_UP
                                    outParamOffsets[outParamOffsetIndex] = -((int)sym->m_offset + BailOutInfo::StackSymBias);
#else
                                    // Stack offset are negative, includes the PUSH EBP and return address
                                    outParamOffsets[outParamOffsetIndex] = sym->m_offset - (2 * MachPtr);
#endif
                                }
#ifdef _M_IX86
                                else if (fDoStackAdjust)
                                {
                                    // If we've got args on the stack, then we must have seen (and adjusted for) the StartCall.
                                    // The values is already on the stack
                                    // On AMD64/ARM, ArgOut should have been moved next to the call, and shouldn't have bailout between them
                                    // Except for inlined arg outs
                                    outParamOffsets[outParamOffsetIndex] = currentStackOffset + argSlot * MachPtr;
                                    bailOutInfo->outParamFrameAdjustArgSlot->Set(outParamOffsetIndex);
                                }
#endif
                                else
                                {
                                    this->FillBailOutOffset(&outParamOffsets[outParamOffsetIndex], sym, &state, instr);
                                }
                            }
                            else
                            {
                                // The ArgOut instruction might have moved down right next to the call,
                                // because of a register calling convention, cloning, etc.  This loop walks the chain
                                // of assignments to try to find the original location of the assignment where
                                // the value is available.
                                while (!sym->IsConst())
                                {
                                    // the value is in the register
                                    IR::RegOpnd * regOpnd = instrDef->GetSrc1()->AsRegOpnd();
                                    sym = regOpnd->m_sym;

                                    if (sym->scratch.linearScan.lifetime->start < instr->GetNumber())
                                    {
                                        break;
                                    }

                                    if (sym->m_isEncodedConstant)
                                    {
                                        break;
                                    }
                                    // For out parameter we might need to follow multiple assignments
                                    Assert(sym->m_isSingleDef);
                                    instrDef = sym->m_instrDef;
                                    Assert(LowererMD::IsAssign(instrDef));
                                }

                                if (bailOutInfo->usedCapturedValues.argObjSyms && bailOutInfo->usedCapturedValues.argObjSyms->Test(sym->m_id))
                                {
                                    //foo.apply(this,arguments) case and we bailout when the apply is overridden. We need to restore the arguments object.
                                    outParamOffsets[outParamOffsetIndex] = BailOutRecord::GetArgumentsObjectOffset();
                                }
                                else
                                {
                                    this->FillBailOutOffset(&outParamOffsets[outParamOffsetIndex], sym, &state, instr);
                                }
                            }
                        }
                    }
                    else
                    {
                        this->FillBailOutOffset(&outParamOffsets[outParamOffsetIndex], sym, &state, instr);
                    }

                    if (sym->IsFloat64())
                    {
                        argOutFloat64Syms->Set(outParamOffsetIndex);
                    }
                    else if (sym->IsInt32())
                    {
                        argOutLosslessInt32Syms->Set(outParamOffsetIndex);
                    }
                    // SIMD_JS
                    else if (sym->IsSimd128F4())
                    {
                        argOutSimd128F4Syms->Set(outParamOffsetIndex);
                    }
                    else if (sym->IsSimd128I4())
                    {
                        argOutSimd128I4Syms->Set(outParamOffsetIndex);
                    }
#if DBG_DUMP
                    if (PHASE_DUMP(Js::BailOutPhase, this->func))
                    {
                        Output::Print(L"OutParam #%d: ", argSlot);
                        sym->Dump();
                        Output::Print(L"\n");
                    }
#endif
                }
            }
        }
    }
    else
    {
        Assert(bailOutInfo->argOutSyms == nullptr);
        Assert(bailOutInfo->startCallCount == 0);
    }

    if (this->currentBlock->inlineeStack.Count() > 0)
    {
        this->SpillInlineeArgs(instr);
    }
    else
    {
        // There is a chance that the instruction was hoisting from an inlinee func
        // but if there are no inlinee frames - make sure the instr belongs to the outer func
        // to ensure encoder does not encode an inline frame here - which does not really exist
        instr->m_func = this->func;
    }

    linearScanMD.GenerateBailOut(instr, state.registerSaveSyms, _countof(state.registerSaveSyms));

    // generate the constant table
    Js::Var * constants = NativeCodeDataNewArray(allocator, Js::Var, state.constantList.Count());
    uint constantCount = state.constantList.Count();
    while (!state.constantList.Empty())
    {
        Js::Var value = state.constantList.Head();
        state.constantList.RemoveHead();
        constants[state.constantList.Count()] = value;
    }

    // Generate the stack literal bail out info
    FillStackLiteralBailOutRecord(instr, bailOutInfo, funcBailOutData, funcCount);

    for (uint i = 0; i < funcCount; i++)
    {

        funcBailOutData[i].bailOutRecord->constants = constants;
#if DBG
        funcBailOutData[i].bailOutRecord->inlineDepth = funcBailOutData[i].func->inlineDepth;
        funcBailOutData[i].bailOutRecord->constantCount = constantCount;
#endif
        uint32 tableIndex = funcBailOutData[i].func->IsTopFunc() ? 0 : funcBailOutData[i].func->m_inlineeId;
        funcBailOutData[i].FinalizeLocalOffsets(tempAlloc, this->globalBailOutRecordTables[tableIndex], &(this->lastUpdatedRowIndices[tableIndex]));
#if DBG_DUMP
        if(PHASE_DUMP(Js::BailOutPhase, this->func))
        {
            wchar_t debugStringBuffer[MAX_FUNCTION_BODY_DEBUG_STRING_SIZE];
            Output::Print(L"Bailout function: %s [%s]\n", funcBailOutData[i].func->GetJnFunction()->GetDisplayName(), funcBailOutData[i].func->GetJnFunction()->GetDebugNumberSet(debugStringBuffer), i);
            funcBailOutData[i].bailOutRecord->Dump();
        }
#endif
        funcBailOutData[i].Clear(this->tempAlloc);

#ifdef PROFILE_BAILOUT_RECORD_MEMORY
        if (Js::Configuration::Global.flags.ProfileBailOutRecordMemory)
        {
            this->func->GetScriptContext()->bailOutRecordBytes += sizeof(BailOutRecord);
        }
#endif
    }
    JitAdeleteArray(this->tempAlloc, funcCount, funcBailOutData);
}

template <typename Fn>
void
LinearScan::ForEachStackLiteralBailOutInfo(IR::Instr * instr, BailOutInfo * bailOutInfo, FuncBailOutData * funcBailOutData, uint funcCount, Fn fn)
{
    for (uint i = 0; i < bailOutInfo->stackLiteralBailOutInfoCount; i++)
    {
        BailOutInfo::StackLiteralBailOutInfo& stackLiteralBailOutInfo = bailOutInfo->stackLiteralBailOutInfo[i];
        StackSym * stackSym = stackLiteralBailOutInfo.stackSym;
        Assert(stackSym->scratch.linearScan.lifetime->start < instr->GetNumber());
        Assert(stackSym->scratch.linearScan.lifetime->end >= instr->GetNumber());

        Js::RegSlot regSlot = stackSym->GetByteCodeRegSlot();
        Func * stackSymFunc = stackSym->GetByteCodeFunc();
        uint index = stackSymFunc->inlineDepth;

        Assert(regSlot != Js::Constants::NoRegister);
        Assert(regSlot < stackSymFunc->GetJnFunction()->GetLocalsCount());
        Assert(index < funcCount);
        Assert(funcBailOutData[index].func == stackSymFunc);
        Assert(funcBailOutData[index].localOffsets[regSlot] != 0);
        fn(index, stackLiteralBailOutInfo, regSlot);
    }
}

void
LinearScan::FillStackLiteralBailOutRecord(IR::Instr * instr, BailOutInfo * bailOutInfo, FuncBailOutData * funcBailOutData, uint funcCount)
{
    if (bailOutInfo->stackLiteralBailOutInfoCount)
    {
        // Count the data
        ForEachStackLiteralBailOutInfo(instr, bailOutInfo, funcBailOutData, funcCount,
            [=](uint funcIndex, BailOutInfo::StackLiteralBailOutInfo& stackLiteralBailOutInfo, Js::RegSlot regSlot)
        {
            funcBailOutData[funcIndex].bailOutRecord->stackLiteralBailOutRecordCount++;
        });

        // Allocate the data
        NativeCodeData::Allocator * allocator = this->func->GetNativeCodeDataAllocator();
        for (uint i = 0; i < funcCount; i++)
        {
            uint stackLiteralBailOutRecordCount = funcBailOutData[i].bailOutRecord->stackLiteralBailOutRecordCount;
            if (stackLiteralBailOutRecordCount)
            {
                funcBailOutData[i].bailOutRecord->stackLiteralBailOutRecord =
                    NativeCodeDataNewArray(allocator, BailOutRecord::StackLiteralBailOutRecord, stackLiteralBailOutRecordCount);
                // reset the count so we can track how much we have filled below
                funcBailOutData[i].bailOutRecord->stackLiteralBailOutRecordCount = 0;
            }
        }

        // Fill out the data
        ForEachStackLiteralBailOutInfo(instr, bailOutInfo, funcBailOutData, funcCount,
            [=](uint funcIndex, BailOutInfo::StackLiteralBailOutInfo& stackLiteralBailOutInfo, Js::RegSlot regSlot)
        {
            uint& recordIndex = funcBailOutData[funcIndex].bailOutRecord->stackLiteralBailOutRecordCount;
            BailOutRecord::StackLiteralBailOutRecord& stackLiteralBailOutRecord =
                funcBailOutData[funcIndex].bailOutRecord->stackLiteralBailOutRecord[recordIndex++];
            stackLiteralBailOutRecord.regSlot = regSlot;
            stackLiteralBailOutRecord.initFldCount = stackLiteralBailOutInfo.initFldCount;
        });
    }
}

void
LinearScan::PrepareForUse(Lifetime * lifetime)
{
    if (lifetime->isOpHelperSpilled)
    {
        // using a value in a helper that has been spilled in the helper block.
        // Just spill it for real

        // We must be in a helper block and the lifetime must
        // start before the helper block

        Assert(this->IsInHelperBlock());
        Assert(lifetime->start < this->HelperBlockStartInstrNumber());

        IR::Instr *insertionInstr = this->currentOpHelperBlock->opHelperLabel;

        this->RemoveOpHelperSpilled(lifetime);
        this->SpillLiveRange(lifetime, insertionInstr);
    }
}

void
LinearScan::RecordUse(Lifetime * lifetime, IR::Instr * instr, IR::RegOpnd * regOpnd, bool isFromBailout)
{
    uint32 useCountCost = LinearScan::GetUseSpillCost(this->loopNest, (this->currentOpHelperBlock != nullptr || isFromBailout));

    // We only spill at the use for constants (i.e. reload) or for function with try blocks. We don't
    // have real accurate flow info for the later.
    if ((regOpnd && regOpnd->m_sym->IsConst())
          || (
                 (this->func->HasTry() && !this->func->DoOptimizeTryCatch()) &&
                 this->IsInLoop() &&
                 lifetime->lastUseLabel != this->lastLabel &&
                 this->liveOnBackEdgeSyms->Test(lifetime->sym->m_id) &&
                 !(lifetime->previousDefBlockNumber == currentBlockNumber && !lifetime->defList.Empty())
             ))
    {
        // Keep track of all the uses of this lifetime in case we decide to spill it.
        // Note that we won't need to insert reloads if the use are not in a loop,
        // unless it is a const. We always reload const instead of spilling to the stack.
        //
        // We also don't need to insert reloads if the previous use was in the same basic block (the first use in the block
        // would have done the reload), or the previous def is in the same basic block and the value is still live. Furthermore,
        // if the previous def is in the same basic block, the value is still live, and there's another def after this use in
        // the same basic block, the previous def may not do a spill store, so we must not reload the value from the stack.
        lifetime->useList.Prepend(instr);
        lifetime->lastUseLabel = this->lastLabel;
        lifetime->AddToUseCountAdjust(useCountCost, this->curLoop, this->func);
    }
    else
    {
        if (!isFromBailout)
        {
            // Since we won't reload this use if the lifetime gets spilled, adjust the spill cost to reflect this.
            lifetime->SubFromUseCount(useCountCost, this->curLoop);
        }
    }
    if (this->IsInLoop())
    {
        this->RecordLoopUse(lifetime, lifetime->reg);
    }
}

void LinearScan::RecordLoopUse(Lifetime *lifetime, RegNum reg)
{
    if (!this->IsInLoop())
    {
        return;
    }

    if (this->func->HasTry() && !this->func->DoOptimizeTryCatch())
    {
        return;
    }

    // Record on each loop which register live into the loop ended up being used.
    // We are trying to avoid the need for compensation at the bottom of the loop if
    // the reg ends up being spilled before it is actually used.
    Loop *curLoop = this->curLoop;
    SymID symId = (SymID)-1;

    if (lifetime)
    {
        symId = lifetime->sym->m_id;
    }

    while (curLoop)
    {
        // Note that if the lifetime is spilled and reallocated to the same register,
        // will mark it as used when we shouldn't.  However, it is hard at this point to handle
        // the case were a flow edge from the previous allocation merges in with the new allocation.
        // No compensation is inserted to let us know with previous lifetime needs reloading at the bottom of the loop...
        if (lifetime && curLoop->regAlloc.loopTopRegContent[reg] == lifetime)
        {
            curLoop->regAlloc.symRegUseBv->Set(symId);
        }
        curLoop->regAlloc.regUseBv.Set(reg);
        curLoop = curLoop->parent;
    }
}

void
LinearScan::RecordDef(Lifetime *const lifetime, IR::Instr *const instr, const uint32 useCountCost)
{
    Assert(lifetime);
    Assert(instr);
    Assert(instr->GetDst());

    IR::RegOpnd * regOpnd = instr->GetDst()->AsRegOpnd();
    Assert(regOpnd);

    StackSym *const sym = regOpnd->m_sym;

    if (this->IsInLoop())
    {
        Loop *curLoop = this->curLoop;

        while (curLoop)
        {
            curLoop->regAlloc.defdInLoopBv->Set(lifetime->sym->m_id);
            curLoop->regAlloc.regUseBv.Set(lifetime->reg);
            curLoop = curLoop->parent;
        }
    }

    if (lifetime->isSpilled)
    {
        return;
    }

    if (this->NeedsWriteThrough(sym))
    {
        if (this->IsSymNonTempLocalVar(sym))
        {
            // In the debug mode, we will write through on the stack location.
            WriteThroughForLocal(regOpnd, lifetime, instr);
        }
        else
        {
            // If this is a write-through sym, it should be live on the entry to 'try' and should have already
            // been allocated when we spilled all active lifetimes there.
            // If it was not part of the active lifetimes on entry to the 'try' then it must have been spilled
            // earlier and should have stack allocated for it.

            Assert(this->NeedsWriteThroughForEH(sym) && sym->IsAllocated());
            this->InsertStore(instr, sym, lifetime->reg);
        }

        // No need to record-def further as we already have stack allocated for it.
        return;
    }

    if (sym->m_isSingleDef)
    {
        lifetime->AddToUseCount(useCountCost, this->curLoop, this->func);
        // the def of a single-def sym is already on the sym
        return;
    }

    if(lifetime->previousDefBlockNumber == currentBlockNumber && !lifetime->defList.Empty())
    {
        // Only keep track of the last def in each basic block. When there are multiple defs of a sym in a basic block, upon
        // spill of that sym, a store needs to be inserted only after the last def of the sym.
        Assert(lifetime->defList.Head()->GetDst()->AsRegOpnd()->m_sym == sym);
        lifetime->defList.Head() = instr;
    }
    else
    {
        // First def of this sym in the current basic block
        lifetime->previousDefBlockNumber = currentBlockNumber;
        lifetime->defList.Prepend(instr);

        // Keep track of the cost of reinserting all the defs if we choose to spill this way.
        lifetime->allDefsCost += useCountCost;
    }
}

// LinearScan::SetUse
void
LinearScan::SetUse(IR::Instr *instr, IR::RegOpnd *regOpnd)
{
    if (regOpnd->GetReg() != RegNOREG)
    {
        this->RecordLoopUse(nullptr, regOpnd->GetReg());
        return;
    }

    StackSym *sym = regOpnd->m_sym;
    Lifetime * lifetime = sym->scratch.linearScan.lifetime;

    this->PrepareForUse(lifetime);

    if (lifetime->isSpilled)
    {
        // See if it has been loaded in this basic block
        RegNum reg = this->GetAssignedTempReg(lifetime, regOpnd->GetType());
        if (reg == RegNOREG)
        {
            if (sym->IsConst() && EncoderMD::TryConstFold(instr, regOpnd))
            {
                return;
            }

            reg = this->SecondChanceAllocation(lifetime, false);
            if (reg != RegNOREG)
            {
                IR::Instr *insertInstr = this->TryHoistLoad(instr, lifetime);
                this->InsertLoad(insertInstr, sym, reg);
            }
            else
            {
                // Try folding if there are no registers available
                if (!sym->IsConst() && !this->RegsAvailable(regOpnd->GetType()) && EncoderMD::TryFold(instr, regOpnd))
                {
                    return;
                }

                // We need a reg no matter what.  Try to force second chance to re-allocate this.
                reg = this->SecondChanceAllocation(lifetime, true);

                if (reg == RegNOREG)
                {
                    // Forcing second chance didn't work.
                    // Allocate a new temp reg for it
                    reg = this->FindReg(nullptr, regOpnd);
                    this->AssignTempReg(lifetime, reg);
                }

                this->InsertLoad(instr, sym, reg);
            }
        }
    }
    if (!lifetime->isSpilled && instr->GetNumber() < lifetime->end)
    {
        // Don't border to record the use if this is the last use of the lifetime.
        this->RecordUse(lifetime, instr, regOpnd);
    }
    else
    {
        lifetime->SubFromUseCount(LinearScan::GetUseSpillCost(this->loopNest, (this->currentOpHelperBlock != nullptr)), this->curLoop);
    }
    this->instrUseRegs.Set(lifetime->reg);

    this->SetReg(regOpnd);
}

// LinearScan::SetReg
void
LinearScan::SetReg(IR::RegOpnd *regOpnd)
{
    if (regOpnd->GetReg() == RegNOREG)
    {
        RegNum reg = regOpnd->m_sym->scratch.linearScan.lifetime->reg;
        AssertMsg(reg != RegNOREG, "Reg should be allocated here...");
        regOpnd->SetReg(reg);
    }
}

bool
LinearScan::SkipNumberedInstr(IR::Instr *instr)
{
    if (instr->IsLabelInstr())
    {
        if (instr->AsLabelInstr()->m_isLoopTop)
        {
            Assert(instr->GetNumber() != instr->m_next->GetNumber()
                && (instr->GetNumber() != instr->m_prev->GetNumber() || instr->m_prev->m_opcode == Js::OpCode::Nop));
        }
        else
        {
            return true;
        }
    }
    return false;
}

// LinearScan::EndDeadLifetimes
// Look for lifetimes that are ending here, and retire them.
void
LinearScan::EndDeadLifetimes(IR::Instr *instr)
{
    Lifetime * deadLifetime;

    if (this->SkipNumberedInstr(instr))
    {
        return;
    }

    // Retire all active lifetime ending at this instruction
    while (!this->activeLiveranges->Empty() && this->activeLiveranges->Head()->end <= instr->GetNumber())
    {
        deadLifetime = this->activeLiveranges->Head();
        deadLifetime->defList.Clear();
        deadLifetime->useList.Clear();

        this->activeLiveranges->RemoveHead();
        RegNum reg = deadLifetime->reg;
        this->activeRegs.Clear(reg);
        this->regContent[reg] = nullptr;
        this->secondChanceRegs.Clear(reg);
        if (RegTypes[reg] == TyMachReg)
        {
            this->intRegUsedCount--;
        }
        else
        {
            Assert(RegTypes[reg] == TyFloat64);
            this->floatRegUsedCount--;
        }
    }

    // Look for spilled lifetimes which end here such that we can make their stack slot
    // available for stack-packing.
    while (!this->stackPackInUseLiveRanges->Empty() && this->stackPackInUseLiveRanges->Head()->end <= instr->GetNumber())
    {
        deadLifetime = this->stackPackInUseLiveRanges->Head();
        deadLifetime->defList.Clear();
        deadLifetime->useList.Clear();

        this->stackPackInUseLiveRanges->RemoveHead();
        if (!deadLifetime->cantStackPack)
        {
            Assert(deadLifetime->spillStackSlot);
            deadLifetime->spillStackSlot->lastUse = deadLifetime->end;
            this->stackSlotsFreeList->Push(deadLifetime->spillStackSlot);
        }
    }
}

void
LinearScan::EndDeadOpHelperLifetimes(IR::Instr * instr)
{
    if (this->SkipNumberedInstr(instr))
    {
        return;
    }

    while (!this->opHelperSpilledLiveranges->Empty() &&
           this->opHelperSpilledLiveranges->Head()->end <= instr->GetNumber())
    {
        Lifetime * deadLifetime;

        // The lifetime doesn't extend beyond the helper block
        // No need to save and restore around the helper block
        Assert(this->IsInHelperBlock());

        deadLifetime = this->opHelperSpilledLiveranges->Head();
        this->opHelperSpilledLiveranges->RemoveHead();
        if (!deadLifetime->cantOpHelperSpill)
        {
            this->opHelperSpilledRegs.Clear(deadLifetime->reg);
        }
        deadLifetime->isOpHelperSpilled = false;
        deadLifetime->cantOpHelperSpill = false;
        deadLifetime->isOpHelperSpillAsArg = false;
    }
}

// LinearScan::AllocateNewLifetimes
// Look for lifetimes coming live, and allocate a register for them.
void
LinearScan::AllocateNewLifetimes(IR::Instr *instr)
{
    if (this->SkipNumberedInstr(instr))
    {
        return;
    }

    // Try to catch:
    //      x = MOV y(r1)
    // where y's lifetime just ended and x's lifetime is starting.
    // If so, set r1 as a preferred register for x, which may allow peeps to remove the MOV
    if (instr->GetSrc1() && instr->GetSrc1()->IsRegOpnd() && LowererMD::IsAssign(instr) && instr->GetDst() && instr->GetDst()->IsRegOpnd() && instr->GetDst()->AsRegOpnd()->m_sym)
    {
        IR::RegOpnd *src = instr->GetSrc1()->AsRegOpnd();
        StackSym *srcSym = src->m_sym;
        // If src is a physReg ref, or src's lifetime ends here.
        if (!srcSym || srcSym->scratch.linearScan.lifetime->end == instr->GetNumber())
        {
            Lifetime *dstLifetime = instr->GetDst()->AsRegOpnd()->m_sym->scratch.linearScan.lifetime;
            if (dstLifetime)
            {
                dstLifetime->regPreference.Set(src->GetReg());
            }
        }
    }

    // Look for starting lifetimes
    while (!this->lifetimeList->Empty() && this->lifetimeList->Head()->start <= instr->GetNumber())
    {
        // We're at the start of a new live range

        Lifetime * newLifetime = this->lifetimeList->Head();
        newLifetime->lastAllocationStart = instr->GetNumber();

        this->lifetimeList->RemoveHead();

        if (newLifetime->dontAllocate)
        {
            // Lifetime spilled before beginning allocation (e.g., a lifetime known to span
            // multiple EH regions.) Do the work of spilling it now without adding it to the list.
            this->SpillLiveRange(newLifetime);
            continue;
        }

        RegNum reg;
        if (newLifetime->reg == RegNOREG)
        {
            if (newLifetime->isDeadStore)
            {
                // No uses, let's not waste a reg.
                newLifetime->isSpilled = true;
                continue;
            }
            reg = this->FindReg(newLifetime, nullptr);
        }
        else
        {
            // This lifetime is already assigned a physical register.  Make
            // sure that register is available by calling SpillReg
            reg = newLifetime->reg;

            // If we're in a helper block, the physical register we're trying to ensure is available might get helper
            // spilled. Don't allow that if this lifetime's end lies beyond the end of the helper block because
            // spill code assumes that this physical register isn't active at the end of the helper block when it tries
            // to restore it. So we'd have to really spill the lifetime then anyway.
            this->SpillReg(reg, IsInHelperBlock() ? (newLifetime->end > currentOpHelperBlock->opHelperEndInstr->GetNumber()) : false);
            newLifetime->cantSpill = true;
        }

        // If we did get a register for this lifetime, add it to the active set.
        if (newLifetime->isSpilled == false)
        {
            this->AssignActiveReg(newLifetime, reg);
        }
    }
}

// LinearScan::FindReg
// Look for an available register.  If one isn't available, spill something.
// Note that the newLifetime passed in could be the one we end up spilling.
RegNum
LinearScan::FindReg(Lifetime *newLifetime, IR::RegOpnd *regOpnd, bool force)
{
    BVIndex regIndex = BVInvalidIndex;
    IRType type;
    bool tryCallerSavedRegs = false;
    BitVector callerSavedAvailableBv;

    if (newLifetime)
    {
        if (newLifetime->isFloat)
        {
            type = TyFloat64;
        }
        else if (newLifetime->isSimd128F4)
        {
            type = TySimd128F4;
        }
        else if (newLifetime->isSimd128I4)
        {
            type = TySimd128I4;
        }
        else if (newLifetime->isSimd128D2)
        {
            type = TySimd128D2;
        }
        else
        {
            type = TyMachReg;
        }
    }
    else
    {
        Assert(regOpnd);
        type = regOpnd->GetType();
    }

    if (this->RegsAvailable(type))
    {
        BitVector regsBv;
        regsBv.Copy(this->activeRegs);
        regsBv.Or(this->instrUseRegs);
        regsBv.Or(this->callSetupRegs);
        regsBv.ComplimentAll();

        if (newLifetime)
        {
            if (this->IsInHelperBlock())
            {
                if (newLifetime->end >= this->HelperBlockEndInstrNumber())
                {
                    // this lifetime goes beyond the helper function
                    // We need to exclude the helper spilled register as well.
                    regsBv.Minus(this->opHelperSpilledRegs);
                }
            }

            if (newLifetime->isFloat || newLifetime->isSimd128())
            {
#ifdef _M_IX86
                Assert(AutoSystemInfo::Data.SSE2Available());
#endif
                regsBv.And(this->floatRegs);
            }
            else
            {
                regsBv.And(this->int32Regs);
                regsBv = this->linearScanMD.FilterRegIntSizeConstraints(regsBv, newLifetime->intUsageBv);
            }


            if (newLifetime->isLiveAcrossCalls)
            {
                // Try to find a callee saved regs
                BitVector regsBvTemp = regsBv;
                regsBvTemp.And(this->calleeSavedRegs);

                regIndex = GetPreferrencedRegIndex(newLifetime, regsBvTemp);

                if (regIndex == BVInvalidIndex)
                {
                    if (!newLifetime->isLiveAcrossUserCalls)
                    {
                        // No callee saved regs is found and the lifetime only across helper
                        // calls, we can also use a caller saved regs to make use of the
                        // save and restore around helper blocks
                        regIndex = GetPreferrencedRegIndex(newLifetime, regsBv);
                    }
                    else
                    {
                        // If we can't find a callee-saved reg, we can try using a caller-saved reg instead.
                        // We'll hopefully get a few loads enregistered that way before we get to the call.
                        tryCallerSavedRegs = true;
                        callerSavedAvailableBv = regsBv;
                    }
                }
            }
            else
            {
                regIndex = GetPreferrencedRegIndex(newLifetime, regsBv);
            }
        }
        else
        {
            AssertMsg(regOpnd, "Need a lifetime or a regOpnd passed in");

            if (regOpnd->IsFloat() || regOpnd->IsSimd128())
            {
#ifdef _M_IX86
                Assert(AutoSystemInfo::Data.SSE2Available());
#endif
                regsBv.And(this->floatRegs);
            }
            else
            {
                regsBv.And(this->int32Regs);
                BitVector regSizeBv;
                regSizeBv.ClearAll();
                regSizeBv.Set(TySize[regOpnd->GetType()]);

                regsBv = this->linearScanMD.FilterRegIntSizeConstraints(regsBv, regSizeBv);
            }

            if (!this->tempRegs.IsEmpty())
            {
                // avoid the temp reg that we have loaded in this basic block
                BitVector regsBvTemp = regsBv;
                regsBvTemp.Minus(this->tempRegs);
                regIndex = regsBvTemp.GetPrevBit();
            }

            if (regIndex == BVInvalidIndex)
            {
                // allocate a temp reg from the other end of the bit vector so that it can
                // keep live for longer.
                regIndex = regsBv.GetPrevBit();
            }
        }
    }

    RegNum reg;

    if (BVInvalidIndex != regIndex)
    {
        Assert(regIndex < RegNumCount);
        reg = (RegNum)regIndex;
    }
    else
    {
        if (tryCallerSavedRegs)
        {
            Assert(newLifetime);
            regIndex = GetPreferrencedRegIndex(newLifetime, callerSavedAvailableBv);
            if (BVInvalidIndex == regIndex)
            {
                tryCallerSavedRegs = false;
            }
        }

        bool dontSpillCurrent = tryCallerSavedRegs;

        if (newLifetime && newLifetime->isSpilled)
        {
            // Second chance allocation
            dontSpillCurrent = true;
        }

        // Can't find reg, spill some lifetime.
        reg = this->Spill(newLifetime, regOpnd, dontSpillCurrent, force);

        if (reg == RegNOREG && tryCallerSavedRegs)
        {
            Assert(BVInvalidIndex != regIndex);
            reg = (RegNum)regIndex;
            // This lifetime will get spilled once we get to the call it overlaps with (note: this may not be true
            // for second chance allocation as we may be beyond the call).  Mark it as a cheap spill to give up the register
            // if some lifetime not overlapping with a call needs it.
            newLifetime->isCheapSpill = true;
        }
    }

    // We always have to return a reg if we are allocate temp reg.
    // If we are allocating for a new lifetime, we return RegNOREG, if we
    // spill the new lifetime
    Assert(newLifetime != nullptr || (reg != RegNOREG && reg < RegNumCount));
    return reg;
}

BVIndex
LinearScan::GetPreferrencedRegIndex(Lifetime *lifetime, BitVector freeRegs)
{
    BitVector freePreferencedRegs = freeRegs;

    freePreferencedRegs.And(lifetime->regPreference);

    // If one of the preferred register (if any) is available, use it.  Otherwise, just pick one of free register.
    if (!freePreferencedRegs.IsEmpty())
    {
        return freePreferencedRegs.GetNextBit();
    }
    else
    {
        return freeRegs.GetNextBit();
    }
}


// LinearScan::Spill
// We need to spill something to free up a reg. If the newLifetime
// past in isn't NULL, we can spill this one instead of an active one.
RegNum
LinearScan::Spill(Lifetime *newLifetime, IR::RegOpnd *regOpnd, bool dontSpillCurrent, bool force)
{
    uint minSpillCost = (uint)-1;

    Assert(!newLifetime || !regOpnd || newLifetime->isFloat == (regOpnd->GetType() == TyMachDouble) || newLifetime->isSimd128() == (regOpnd->IsSimd128()));
    bool isFloatReg;
    BitVector intUsageBV;
    bool needCalleeSaved;

    // For now, we just spill the lifetime with the lowest spill cost.
    if (newLifetime)
    {
        isFloatReg = newLifetime->isFloat || newLifetime->isSimd128();

        if (!force)
        {
            minSpillCost = this->GetSpillCost(newLifetime);
        }
        intUsageBV = newLifetime->intUsageBv;
        needCalleeSaved = newLifetime->isLiveAcrossUserCalls;
    }
    else
    {
        needCalleeSaved = false;
        if (regOpnd->IsFloat() || regOpnd->IsSimd128())
        {
            isFloatReg = true;
        }
        else
        {
            // Filter for int reg size constraints
            isFloatReg = false;
            intUsageBV.ClearAll();
            intUsageBV.Set(TySize[regOpnd->GetType()]);
        }
    }

    SList<Lifetime *>::EditingIterator candidate;
    FOREACH_SLIST_ENTRY_EDITING(Lifetime *, lifetime, this->activeLiveranges, iter)
    {
        uint spillCost = this->GetSpillCost(lifetime);
        if (spillCost < minSpillCost                        &&
            this->instrUseRegs.Test(lifetime->reg) == false &&
            (lifetime->isFloat || lifetime->isSimd128()) == isFloatReg  &&
            !lifetime->cantSpill                            &&
            (!needCalleeSaved || this->calleeSavedRegs.Test(lifetime->reg)) &&
            this->linearScanMD.FitRegIntSizeConstraints(lifetime->reg, intUsageBV))
        {
            minSpillCost = spillCost;
            candidate = iter;
        }
    } NEXT_SLIST_ENTRY_EDITING;
    AssertMsg(newLifetime || candidate.IsValid(), "Didn't find anything to spill?!?");

    Lifetime * spilledRange;
    if (candidate.IsValid())
    {
        spilledRange = candidate.Data();
        candidate.RemoveCurrent();

        this->activeRegs.Clear(spilledRange->reg);
        if (spilledRange->isFloat || spilledRange->isSimd128())
        {
            this->floatRegUsedCount--;
        }
        else
        {
            this->intRegUsedCount--;
        }
    }
    else if (dontSpillCurrent)
    {
        return RegNOREG;
    }
    else
    {
        spilledRange = newLifetime;
    }

    return this->SpillLiveRange(spilledRange);
}

// LinearScan::SpillLiveRange
RegNum
LinearScan::SpillLiveRange(Lifetime * spilledRange, IR::Instr *insertionInstr)
{
    Assert(!spilledRange->isSpilled);

    RegNum reg = spilledRange->reg;
    StackSym *sym = spilledRange->sym;

    spilledRange->isSpilled = true;
    spilledRange->isCheapSpill = false;
    spilledRange->reg = RegNOREG;

    // Don't allocate stack space for const, we always reload them. (For debugm mode, allocate on the stack)
    if (!sym->IsAllocated() && (!sym->IsConst() || IsSymNonTempLocalVar(sym)))
    {
       this->AllocateStackSpace(spilledRange);
    }

    // No need to insert loads or stores if there are no uses.
    if (!spilledRange->isDeadStore)
    {
        // In the debug mode, don't do insertstore for this stacksym, as we want to retain the IsConst for the sym,
        // and later we are going to find the reg for it.
        if (!IsSymNonTempLocalVar(sym))
        {
            this->InsertStores(spilledRange, reg, insertionInstr);
        }

        if (this->IsInLoop() || sym->IsConst())
        {
            this->InsertLoads(sym, reg);
        }
        else
        {
            sym->scratch.linearScan.lifetime->useList.Clear();
        }
        // Adjust useCount in case of second chance allocation
        spilledRange->ApplyUseCountAdjust(this->curLoop);
    }

    Assert(reg == RegNOREG || spilledRange->reg == RegNOREG || this->regContent[reg] == spilledRange);
    if (spilledRange->isSecondChanceAllocated)
    {
        Assert(reg == RegNOREG || spilledRange->reg == RegNOREG
            || (this->regContent[reg] == spilledRange && this->secondChanceRegs.Test(reg)));
        this->secondChanceRegs.Clear(reg);
        spilledRange->isSecondChanceAllocated = false;
    }
    else
    {
        Assert(!this->secondChanceRegs.Test(reg));
    }
    this->regContent[reg] = nullptr;

#if DBG_DUMP
    if (PHASE_TRACE(Js::LinearScanPhase, this->func))
    {
        Output::Print(L"**** Spill: ");
        sym->Dump();
        Output::Print(L"(%S)", RegNames[reg]);
        Output::Print(L"  SpillCount:%d  Length:%d   Cost:%d\n",
            spilledRange->useCount, spilledRange->end - spilledRange->start, this->GetSpillCost(spilledRange));
    }
#endif
    return reg;
}

// LinearScan::SpillReg
// Spill a given register.
void
LinearScan::SpillReg(RegNum reg, bool forceSpill /* = false */)
{
    Lifetime *spilledRange = nullptr;
    if (activeRegs.Test(reg))
    {
        spilledRange = LinearScan::RemoveRegLiveRange(activeLiveranges, reg);
    }
    else if (opHelperSpilledRegs.Test(reg) && forceSpill)
    {
        // If a lifetime that was assigned this register was helper spilled,
        // really spill it now.
        Assert(IsInHelperBlock());

        // Look for the liverange in opHelperSpilledLiveranges instead of
        // activeLiveranges.
        FOREACH_SLIST_ENTRY(Lifetime *, lifetime, opHelperSpilledLiveranges)
        {
            if (lifetime->reg == reg)
            {
                spilledRange = lifetime;
                break;
            }
        } NEXT_SLIST_ENTRY;

        Assert(spilledRange);
        Assert(!spilledRange->cantSpill);
        RemoveOpHelperSpilled(spilledRange);
        // Really spill this liverange below.
    }
    else
    {
        return;
    }

    AnalysisAssert(spilledRange);
    Assert(!spilledRange->cantSpill);

    if ((!forceSpill) && this->IsInHelperBlock() && spilledRange->start < this->HelperBlockStartInstrNumber() && !spilledRange->cantOpHelperSpill)
    {
        // if the lifetime starts before the helper block, we can do save and restore
        // around the helper block instead.

        this->AddOpHelperSpilled(spilledRange);
    }
    else
    {
        if (spilledRange->cantOpHelperSpill)
        {
            // We're really spilling this liverange, so take it out of the helper-spilled liveranges
            // to avoid confusion (see Win8 313433).
            Assert(!spilledRange->isOpHelperSpilled);
            spilledRange->cantOpHelperSpill = false;
            this->opHelperSpilledLiveranges->Remove(spilledRange);
        }
        this->SpillLiveRange(spilledRange);
    }

    if (this->activeRegs.Test(reg))
    {
        this->activeRegs.Clear(reg);
        if (RegTypes[reg] == TyMachReg)
        {
            this->intRegUsedCount--;
        }
        else
        {
            Assert(RegTypes[reg] == TyFloat64);
            this->floatRegUsedCount--;
        }
    }
}

void
LinearScan::ProcessEHRegionBoundary(IR::Instr * instr)
{
    Assert(instr->IsBranchInstr());
    Assert(instr->m_opcode != Js::OpCode::TryFinally); // finallys are not supported for optimization yet.
    if (instr->m_opcode != Js::OpCode::TryCatch && instr->m_opcode != Js::OpCode::Leave)
    {
        return;
    }

    // Spill everything upon entry to the try region and upon a Leave.
    IR::Instr* insertionInstr = instr->m_opcode != Js::OpCode::Leave ? instr : instr->m_prev;
    FOREACH_SLIST_ENTRY_EDITING(Lifetime *, lifetime, this->activeLiveranges, iter)
    {
        this->activeRegs.Clear(lifetime->reg);
        if (lifetime->isFloat || lifetime->isSimd128())
        {
            this->floatRegUsedCount--;
        }
        else
        {
            this->intRegUsedCount--;
        }
        this->SpillLiveRange(lifetime, insertionInstr);
        iter.RemoveCurrent();
    }
    NEXT_SLIST_ENTRY_EDITING;
}

void
LinearScan::AllocateStackSpace(Lifetime *spilledRange)
{
    if (spilledRange->sym->IsAllocated())
    {
        return;
    }

    uint32 size = TySize[spilledRange->sym->GetType()];

    // For the bytecodereg syms instead of spilling to the any other location lets re-use the already created slot.
    if (IsSymNonTempLocalVar(spilledRange->sym))
    {
        Js::RegSlot slotIndex = spilledRange->sym->GetByteCodeRegSlot();

        // Get the offset which is already allocated from this local, and always spill on that location.

        spilledRange->sym->m_offset = GetStackOffset(slotIndex);
        spilledRange->sym->m_allocated = true;

        return;
    }

    StackSlot * newStackSlot = nullptr;

    if (!PHASE_OFF(Js::StackPackPhase, this->func) && !this->func->IsJitInDebugMode() && !spilledRange->cantStackPack)
    {
        // Search for a free stack slot to re-use
        FOREACH_SLIST_ENTRY_EDITING(StackSlot *, slot, this->stackSlotsFreeList, iter)
        {
            // Heuristic: should we use '==' or '>=' for the size?
            if (slot->lastUse <= spilledRange->start && slot->size >= size)
            {
                StackSym *spilledSym = spilledRange->sym;

                Assert(!spilledSym->IsArgSlotSym() && !spilledSym->IsParamSlotSym());
                Assert(!spilledSym->IsAllocated());
                spilledRange->spillStackSlot = slot;
                spilledSym->m_offset = slot->offset;
                spilledSym->m_allocated = true;

                iter.RemoveCurrent();

#if DBG_DUMP
                if (Js::Configuration::Global.flags.Trace.IsEnabled(Js::StackPackPhase, this->func->GetSourceContextId(), this->func->GetLocalFunctionId()))
                {
                    spilledSym->Dump();
                    Output::Print(L" *** stack packed at offset %3d  (%4d - %4d)\n", spilledSym->m_offset, spilledRange->start, spilledRange->end);
                }
#endif
                break;
            }
        } NEXT_SLIST_ENTRY_EDITING;

        if (spilledRange->spillStackSlot == nullptr)
        {
            newStackSlot = JitAnewStruct(this->tempAlloc, StackSlot);
            newStackSlot->size = size;
            spilledRange->spillStackSlot = newStackSlot;
        }
        this->AddLiveRange(this->stackPackInUseLiveRanges, spilledRange);
    }

    if (!spilledRange->sym->IsAllocated())
    {
        // Can't stack pack, allocate new stack slot.
        StackSym *spilledSym = spilledRange->sym;
        this->func->StackAllocate(spilledSym, size);

#if DBG_DUMP
        if (Js::Configuration::Global.flags.Trace.IsEnabled(Js::StackPackPhase, this->func->GetSourceContextId(), this->func->GetLocalFunctionId()))
        {
            spilledSym->Dump();
            Output::Print(L" at offset %3d  (%4d - %4d)\n", spilledSym->m_offset, spilledRange->start, spilledRange->end);
        }
#endif
        if (newStackSlot != nullptr)
        {
            newStackSlot->offset = spilledSym->m_offset;
        }
    }
}

// LinearScan::InsertLoads
void
LinearScan::InsertLoads(StackSym *sym, RegNum reg)
{
    Lifetime *lifetime = sym->scratch.linearScan.lifetime;

    FOREACH_SLIST_ENTRY(IR::Instr *, instr, &lifetime->useList)
    {
        this->InsertLoad(instr, sym, reg);
    } NEXT_SLIST_ENTRY;

    lifetime->useList.Clear();
}

// LinearScan::InsertStores
void
LinearScan::InsertStores(Lifetime *lifetime, RegNum reg, IR::Instr *insertionInstr)
{
    StackSym *sym = lifetime->sym;

    // If single def, use instrDef on the symbol
    if (sym->m_isSingleDef)
    {
        IR::Instr * defInstr = sym->m_instrDef;
        if (!sym->IsConst() && defInstr->GetDst()->AsRegOpnd()->GetReg() == RegNOREG
            || this->secondChanceRegs.Test(reg))
        {
            // This can happen if we were trying to allocate this lifetime,
            // and it is getting spilled right away.
            // For second chance allocations, this should have already been handled.

            return;
        }
        this->InsertStore(defInstr, defInstr->FindRegDef(sym)->m_sym, reg);
        return;
    }

    if (reg == RegNOREG)
    {
        return;
    }

    uint localStoreCost = LinearScan::GetUseSpillCost(this->loopNest, (this->currentOpHelperBlock != nullptr));

    // Is it cheaper to spill all the defs we've seen so far or just insert a store at the current point?
    if ((this->func->HasTry() && !this->func->DoOptimizeTryCatch()) || localStoreCost >= lifetime->allDefsCost)
    {
        // Insert a store for each def point we've seen so far
        FOREACH_SLIST_ENTRY(IR::Instr *, instr, &(lifetime->defList))
        {
            if (instr->GetDst()->AsRegOpnd()->GetReg() != RegNOREG)
            {
                IR::RegOpnd *regOpnd = instr->FindRegDef(sym);

                // Note that reg may not be equal to regOpnd->GetReg() if the lifetime has been re-allocated since we've seen this def
                this->InsertStore(instr, regOpnd->m_sym, regOpnd->GetReg());
            }

        } NEXT_SLIST_ENTRY;

        lifetime->defList.Clear();
        lifetime->allDefsCost = 0;
        lifetime->needsStoreCompensation = false;
    }
    else if (!lifetime->defList.Empty())
    {
        // Insert a def right here at the current instr, and then we'll use compensation code for paths not covered by this def.
        if (!insertionInstr)
        {
            insertionInstr = this->currentInstr->m_prev;
        }

        this->InsertStore(insertionInstr, sym, reg);
        if (this->IsInLoop())
        {
            RecordLoopUse(lifetime, reg);
        }
        // We now need to insert all store compensations when needed, unless we spill all the defs later on.
        lifetime->needsStoreCompensation = true;
    }
}

// LinearScan::InsertStore
void
LinearScan::InsertStore(IR::Instr *instr, StackSym *sym, RegNum reg)
{
    // Win8 Bug 391484: We cannot use regOpnd->GetType() here because it
    // can lead to truncation as downstream usage of the register might be of a size
    // greater than the current use. Using RegTypes[reg] works only if the stack slot size
    // is always at least of size MachPtr

    // In the debug mode, if the current sym belongs to the byte code locals, then do not unlink this instruction, as we need to have this instruction to be there
    // to produce the write-through instruction.
    if (sym->IsConst() && !IsSymNonTempLocalVar(sym))
    {
        // Let's just delete the def.  We'll reload the constant.
        // We can't just delete the instruction however since the
        // uses will look at the def to get the value.

        // Make sure it wasn't already deleted.
        if (sym->m_instrDef->m_next)
        {
            sym->m_instrDef->Unlink();
            sym->m_instrDef->m_next = nullptr;
        }
        return;
    }

    Assert(reg != RegNOREG);

    IRType type = sym->GetType();

    if (sym->IsSimd128())
    {
        type = sym->GetType();
    }

    IR::Instr *store = IR::Instr::New(LowererMD::GetStoreOp(type),
        IR::SymOpnd::New(sym, type, this->func),
        IR::RegOpnd::New(sym, reg, type, this->func), this->func);
    instr->InsertAfter(store);
    store->CopyNumber(instr);
    this->linearScanMD.LegalizeDef(store);

#if DBG_DUMP
    if (PHASE_TRACE(Js::LinearScanPhase, this->func))
    {
        Output::Print(L"...Inserting store for ");
        sym->Dump();
        Output::Print(L"  Cost:%d\n", this->GetSpillCost(sym->scratch.linearScan.lifetime));
    }
#endif
}

// LinearScan::InsertLoad
void
LinearScan::InsertLoad(IR::Instr *instr, StackSym *sym, RegNum reg)
{
    IR::Opnd *src;
    // The size of loads and stores to memory need to match. See the comment
    // around type in InsertStore above.
    IRType type = sym->GetType();

    if (sym->IsSimd128())
    {
        type = sym->GetType();
    }

    bool isMovSDZero = false;
    if (sym->IsConst())
    {
        Assert(!sym->IsAllocated() || IsSymNonTempLocalVar(sym));
        // For an intConst, reload the constant instead of using the stack.
        // Create a new StackSym to make sure the old sym remains singleDef
        src = sym->GetConstOpnd();
        if (!src)
        {
            isMovSDZero = true;
            sym = StackSym::New(sym->GetType(), this->func);
            sym->m_isConst = true;
            sym->m_isFltConst = true;
        }
        else
        {
            StackSym * oldSym = sym;
            sym = StackSym::New(TyVar, this->func);
            sym->m_isConst = true;
            sym->m_isIntConst = oldSym->m_isIntConst;
            sym->m_isTaggableIntConst = sym->m_isTaggableIntConst;
        }
    }
    else
    {
        src = IR::SymOpnd::New(sym, type, this->func);
    }
    IR::Instr * load;
#if defined(_M_IX86) || defined(_M_X64)
    if (isMovSDZero)
    {
        load = IR::Instr::New(Js::OpCode::MOVSD_ZERO,
            IR::RegOpnd::New(sym, reg, type, this->func), this->func);
        instr->InsertBefore(load);
    }
    else
#endif
    {
        load = Lowerer::InsertMove(IR::RegOpnd::New(sym, reg, type, this->func), src, instr);
    }
    load->CopyNumber(instr);
    if (!isMovSDZero)
    {
        this->linearScanMD.LegalizeUse(load, src);
    }

    this->RecordLoopUse(nullptr, reg);

#if DBG_DUMP
    if (PHASE_TRACE(Js::LinearScanPhase, this->func))
    {
        Output::Print(L"...Inserting load for ");
        sym->Dump();
        if (sym->scratch.linearScan.lifetime)
        {
            Output::Print(L"  Cost:%d\n", this->GetSpillCost(sym->scratch.linearScan.lifetime));
        }
        else
        {
            Output::Print(L"\n");
        }
    }
#endif
}

uint8
LinearScan::GetRegAttribs(RegNum reg)
{
    return RegAttribs[reg];
}

IRType
LinearScan::GetRegType(RegNum reg)
{
    return RegTypes[reg];
}

bool
LinearScan::IsCalleeSaved(RegNum reg)
{
    return (RegAttribs[reg] & RA_CALLEESAVE) != 0;
}

bool
LinearScan::IsCallerSaved(RegNum reg) const
{
    return !LinearScan::IsCalleeSaved(reg) && LinearScan::IsAllocatable(reg);
}

bool
LinearScan::IsAllocatable(RegNum reg) const
{
    return !(RegAttribs[reg] & RA_DONTALLOCATE) && this->linearScanMD.IsAllocatable(reg, this->func);
}

void
LinearScan::KillImplicitRegs(IR::Instr *instr)
{
    if (instr->IsLabelInstr() || instr->IsBranchInstr())
    {
        // Note: need to clear these for branch as well because this info isn't recorded for second chance
        //       allocation on branch boundaries
        this->tempRegs.ClearAll();
    }

#if defined(_M_IX86) || defined(_M_X64)
    if (instr->m_opcode == Js::OpCode::IMUL)
    {
        this->SpillReg(LowererMDArch::GetRegIMulHighDestLower());
        this->tempRegs.Clear(LowererMDArch::GetRegIMulHighDestLower());

        this->RecordLoopUse(nullptr, LowererMDArch::GetRegIMulHighDestLower());
        return;
    }
#endif

    this->TrackInlineeArgLifetimes(instr);

    // Don't care about kills on bailout calls as we are going to exit anyways
    // Also, for bailout scenarios we have already handled the inlinee frame spills
    Assert(LowererMD::IsCall(instr) || !instr->HasBailOutInfo());
    if (!LowererMD::IsCall(instr) || instr->HasBailOutInfo())
    {
        return;
    }

    if (this->currentBlock->inlineeStack.Count() > 0)
    {
        this->SpillInlineeArgs(instr);
    }
    else
    {
        instr->m_func = this->func;
    }

    //
    // Spill caller-saved registers that are active.
    //
    BitVector deadRegs;
    deadRegs.Copy(this->activeRegs);
    deadRegs.And(this->callerSavedRegs);
    FOREACH_BITSET_IN_UNITBV(reg, deadRegs, BitVector)
    {
        this->SpillReg((RegNum)reg);
    }
    NEXT_BITSET_IN_UNITBV;
    this->tempRegs.And(this->calleeSavedRegs);

    if (callSetupRegs.Count())
    {
        callSetupRegs.ClearAll();
    }
    Loop *loop = this->curLoop;
    while (loop)
    {
        loop->regAlloc.regUseBv.Or(this->callerSavedRegs);
        loop = loop->parent;
    }
}

//
// Before a call, all inlinee frame syms need to be spilled to a pre-defined location
//
void LinearScan::SpillInlineeArgs(IR::Instr* instr)
{
    Assert(this->currentBlock->inlineeStack.Count() > 0);

    // Ensure the call instruction is tied to the current inlinee
    // This is used in the encoder to encode mapping or return offset and InlineeFrameRecord
    instr->m_func = this->currentBlock->inlineeStack.Last();

    BitVector spilledRegs;
    this->currentBlock->inlineeFrameLifetimes.Map([&](uint i, Lifetime* lifetime){
        Assert(lifetime->start < instr->GetNumber() && lifetime->end >= instr->GetNumber());
        Assert(!lifetime->sym->IsConst());
        Assert(this->currentBlock->inlineeFrameSyms.ContainsKey(lifetime->sym->m_id));
        if (lifetime->reg == RegNOREG)
        {
            return;
        }

        StackSym* sym = lifetime->sym;
        if (!lifetime->isSpilled && !lifetime->isOpHelperSpilled &&
            (!lifetime->isDeadStore && (lifetime->sym->m_isSingleDef || !lifetime->defList.Empty()))) // if deflist is empty - we have already spilled at all defs - and the value is current
        {
            if (!spilledRegs.Test(lifetime->reg))
            {
                spilledRegs.Set(lifetime->reg);
                if (!sym->IsAllocated())
                {
                    this->AllocateStackSpace(lifetime);
                }

                this->RecordLoopUse(lifetime, lifetime->reg);
                Assert(this->regContent[lifetime->reg] != nullptr);
                if (sym->m_isSingleDef)
                {
                    // For a single def - we do not track the deflist - the def below will remove the single def on the sym
                    // hence, we need to track the original def.
                    Assert(lifetime->defList.Empty());
                    lifetime->defList.Prepend(sym->m_instrDef);
                }

                this->InsertStore(instr->m_prev, sym, lifetime->reg);
            }
        }
    });
}

void LinearScan::TrackInlineeArgLifetimes(IR::Instr* instr)
{
    if (instr->m_opcode == Js::OpCode::InlineeStart)
    {
        if (instr->m_func->m_hasInlineArgsOpt)
        {
            instr->m_func->frameInfo->IterateSyms([=](StackSym* sym){
                Lifetime* lifetime = sym->scratch.linearScan.lifetime;
                this->currentBlock->inlineeFrameLifetimes.Add(lifetime);

                // We need to maintain as count because the same sym can be used for multiple arguments
                uint* value;
                if (this->currentBlock->inlineeFrameSyms.TryGetReference(sym->m_id, &value))
                {
                    *value = *value + 1;
                }
                else
                {
                    this->currentBlock->inlineeFrameSyms.Add(sym->m_id, 1);
                }
            });
            if (this->currentBlock->inlineeStack.Count() > 0)
            {
                Assert(instr->m_func->inlineDepth == this->currentBlock->inlineeStack.Last()->inlineDepth + 1);
            }
            this->currentBlock->inlineeStack.Add(instr->m_func);
        }
        else
        {
            Assert(this->currentBlock->inlineeStack.Count() == 0);
        }
    }
    else if (instr->m_opcode == Js::OpCode::InlineeEnd)
    {
        if (instr->m_func->m_hasInlineArgsOpt)
        {
            instr->m_func->frameInfo->AllocateRecord(this->func, instr->m_func->GetJnFunction());

            if(this->currentBlock->inlineeStack.Count() == 0)
            {
                // Block is unreachable
                Assert(this->currentBlock->inlineeFrameLifetimes.Count() == 0);
                Assert(this->currentBlock->inlineeFrameSyms.Count() == 0);
            }
            else
            {
                Func* func = this->currentBlock->inlineeStack.RemoveAtEnd();
                Assert(func == instr->m_func);

                instr->m_func->frameInfo->IterateSyms([=](StackSym* sym){
                    Lifetime* lifetime = this->currentBlock->inlineeFrameLifetimes.RemoveAtEnd();

                    uint* value;
                    if (this->currentBlock->inlineeFrameSyms.TryGetReference(sym->m_id, &value))
                    {
                        *value = *value - 1;
                        if (*value == 0)
                        {
                            bool removed = this->currentBlock->inlineeFrameSyms.Remove(sym->m_id);
                            Assert(removed);
                        }
                    }
                    else
                    {
                        Assert(UNREACHED);
                    }
                    Assert(sym->scratch.linearScan.lifetime == lifetime);
                }, /*reverse*/ true);
            }
        }
    }
}

// GetSpillCost
// The spill cost is trying to estimate the usage density of the lifetime,
// by dividing the useCount by the lifetime length.
uint
LinearScan::GetSpillCost(Lifetime *lifetime)
{
    uint useCount = lifetime->GetRegionUseCount(this->curLoop);
    uint spillCost;
    // Get local spill cost.  Ignore helper blocks as we'll also need compensation on the main path.
    uint localUseCost = LinearScan::GetUseSpillCost(this->loopNest, false);

    if (lifetime->reg && !lifetime->isSpilled)
    {
        // If it is in a reg, we'll need a store
        if (localUseCost >= lifetime->allDefsCost)
        {
            useCount += lifetime->allDefsCost;
        }
        else
        {
            useCount += localUseCost;
        }

        if (this->curLoop && !lifetime->sym->IsConst()
            && this->curLoop->regAlloc.liveOnBackEdgeSyms->Test(lifetime->sym->m_id))
        {
            // If we spill here, we'll need to insert a load at the bottom of the loop
            // (it would be nice to be able to check is was in a reg at the top of the loop)...
            useCount += localUseCost;
        }
    }

    // When comparing 2 lifetimes, we don't really care about the actual length of the lifetimes.
    // What matters is how much longer will they use the register.
    const uint start = currentInstr->GetNumber();
    uint end = max(start, lifetime->end);
    uint lifetimeTotalOpHelperFullVisitedLength = lifetime->totalOpHelperLengthByEnd;

    if (this->curLoop && this->curLoop->regAlloc.loopEnd < end && !PHASE_OFF(Js::RegionUseCountPhase, this->func))
    {
        end = this->curLoop->regAlloc.loopEnd;
        lifetimeTotalOpHelperFullVisitedLength  = this->curLoop->regAlloc.helperLength;
    }
    uint length = end - start + 1;

    // Exclude helper block length since helper block paths are typically infrequently taken paths and not as important
    const uint totalOpHelperVisitedLength = this->totalOpHelperFullVisitedLength + CurrentOpHelperVisitedLength(currentInstr);
    Assert(lifetimeTotalOpHelperFullVisitedLength >= totalOpHelperVisitedLength);
    const uint lifetimeHelperLength = lifetimeTotalOpHelperFullVisitedLength - totalOpHelperVisitedLength;
    Assert(length >= lifetimeHelperLength);
    length -= lifetimeHelperLength;
    if(length == 0)
    {
        length = 1;
    }

    // Add a base length so that the difference between a length of 1 and a length of 2 is not so large
#ifdef _M_X64
    length += 64;
#else
    length += 16;
#endif

    spillCost = (useCount << 13) / length;

    if (lifetime->isSecondChanceAllocated)
    {
        // Second chance allocation have additional overhead, so de-prioritize them
        // Note: could use more tuning...
        spillCost = spillCost * 4/5;
    }
    if (lifetime->isCheapSpill)
    {
        // This lifetime will get spilled eventually, so lower the spill cost to favor other lifetimes
        // Note: could use more tuning...
        spillCost /= 2;
    }

    if (lifetime->sym->IsConst())
    {
        spillCost = spillCost / 16;
    }

    return spillCost;
}

bool
LinearScan::RemoveDeadStores(IR::Instr *instr)
{
    IR::Opnd *dst = instr->GetDst();

    if (dst && dst->IsRegOpnd() && dst->AsRegOpnd()->m_sym && !dst->AsRegOpnd()->m_isCallArg)
    {
        IR::RegOpnd *regOpnd = dst->AsRegOpnd();
        Lifetime * lifetime = regOpnd->m_sym->scratch.linearScan.lifetime;

        if (lifetime->isDeadStore)
        {
            if (Lowerer::HasSideEffects(instr) == false)
            {
                // If all the bailouts referencing this arg are removed (which can happen in some scenarios)
                //- then it's OK to remove this def of the arg
                DebugOnly(this->func->allowRemoveBailOutArgInstr = true);

                // We are removing this instruction, end dead life time now
                this->EndDeadLifetimes(instr);
                instr->Remove();

                DebugOnly(this->func->allowRemoveBailOutArgInstr = false);
                return true;
            }
        }
    }

    return false;
}

void
LinearScan::AssignActiveReg(Lifetime * lifetime, RegNum reg)
{
    Assert(!this->activeRegs.Test(reg));
    Assert(!lifetime->isSpilled);
    Assert(lifetime->reg == RegNOREG || lifetime->reg == reg);
    this->func->m_regsUsed.Set(reg);
    lifetime->reg = reg;
    this->activeRegs.Set(reg);
    if (lifetime->isFloat || lifetime->isSimd128())
    {
        this->floatRegUsedCount++;
    }
    else
    {
        this->intRegUsedCount++;
    }
    this->AddToActive(lifetime);

    this->tempRegs.Clear(reg);
}
void
LinearScan::AssignTempReg(Lifetime * lifetime, RegNum reg)
{
    Assert(reg > RegNOREG && reg < RegNumCount);
    Assert(!this->activeRegs.Test(reg));
    Assert(lifetime->isSpilled);
    this->func->m_regsUsed.Set(reg);
    lifetime->reg = reg;
    this->tempRegs.Set(reg);
    __analysis_assume(reg > 0 && reg < RegNumCount);
    this->tempRegLifetimes[reg] = lifetime;

    this->RecordLoopUse(nullptr, reg);
}

RegNum
LinearScan::GetAssignedTempReg(Lifetime * lifetime, IRType type)
{
    if (this->tempRegs.Test(lifetime->reg) && this->tempRegLifetimes[lifetime->reg] == lifetime)
    {
        if (this->linearScanMD.FitRegIntSizeConstraints(lifetime->reg, type))
        {
            this->RecordLoopUse(nullptr, lifetime->reg);
            return lifetime->reg;
        }
        else
        {
            // Free this temp, we'll need to find another one.
            this->tempRegs.Clear(lifetime->reg);
            lifetime->reg = RegNOREG;
        }
    }
    return RegNOREG;
}

uint
LinearScan::GetUseSpillCost(uint loopNest, BOOL isInHelperBlock)
{
    if (isInHelperBlock)
    {
        // Helper block uses are not as important.
        return 0;
    }
    else if (loopNest < 6)
    {
        return (1 << (loopNest * 3));
    }
    else
    {
        // Slow growth for deep nest to avoid overflow
        return (1 << (5 * 3)) * (loopNest-5);
    }
}

void
LinearScan::ProcessSecondChanceBoundary(IR::BranchInstr *branchInstr)
{
    if (this->func->HasTry() && !this->func->DoOptimizeTryCatch())
    {
        return;
    }

    if (this->currentOpHelperBlock && this->currentOpHelperBlock->opHelperEndInstr == branchInstr)
    {
        // Lifetimes opHelperSpilled won't get recorded by SaveRegContent().  Do it here.
        FOREACH_SLIST_ENTRY(Lifetime *, lifetime, this->opHelperSpilledLiveranges)
        {
            if (!lifetime->cantOpHelperSpill)
            {
                if (lifetime->isSecondChanceAllocated)
                {
                    this->secondChanceRegs.Set(lifetime->reg);
                }
                this->regContent[lifetime->reg] = lifetime;
            }
        } NEXT_SLIST_ENTRY;
    }

    if(branchInstr->IsMultiBranch())
    {
        IR::MultiBranchInstr * multiBranchInstr = branchInstr->AsMultiBrInstr();

        multiBranchInstr->MapUniqueMultiBrLabels([=](IR::LabelInstr * branchLabel) -> void
        {
            this->ProcessSecondChanceBoundaryHelper(branchInstr, branchLabel);
        });
    }
    else
    {
        IR::LabelInstr *branchLabel = branchInstr->GetTarget();
        this->ProcessSecondChanceBoundaryHelper(branchInstr, branchLabel);
    }

    this->SaveRegContent(branchInstr);
}

void
LinearScan::ProcessSecondChanceBoundaryHelper(IR::BranchInstr *branchInstr, IR::LabelInstr *branchLabel)
{
    if (branchInstr->GetNumber() > branchLabel->GetNumber())
    {
        // Loop back-edge
        Assert(branchLabel->m_isLoopTop);
        branchInstr->m_regContent = nullptr;
        this->InsertSecondChanceCompensation(this->regContent, branchLabel->m_regContent, branchInstr, branchLabel);
    }
    else
    {
        // Forward branch
        this->SaveRegContent(branchInstr);
        if (this->curLoop)
        {
            this->curLoop->regAlloc.exitRegContentList->Prepend(branchInstr->m_regContent);
        }
        if (!branchLabel->m_loweredBasicBlock)
        {
            if (branchInstr->IsConditional() || branchInstr->IsMultiBranch())
            {
                // Clone with deep copy
                branchLabel->m_loweredBasicBlock = this->currentBlock->Clone(this->tempAlloc);
            }
            else
            {
                // If the unconditional branch leads to the end of the function for the scenario of a bailout - we do not want to
                // copy the lowered inlinee info.
                IR::Instr* nextInstr = branchLabel->GetNextRealInstr();
                if (nextInstr->m_opcode != Js::OpCode::FunctionExit &&
                    nextInstr->m_opcode != Js::OpCode::BailOutStackRestore &&
                    this->currentBlock->HasData())
                {
                    // Clone with shallow copy
                    branchLabel->m_loweredBasicBlock = this->currentBlock;

                }
            }
        }
        else
        {
            // The lowerer sometimes generates unreachable blocks that would have empty data.
            Assert(!currentBlock->HasData() || branchLabel->m_loweredBasicBlock->Equals(this->currentBlock));
        }
    }
}

void
LinearScan::ProcessSecondChanceBoundary(IR::LabelInstr *labelInstr)
{
    if (this->func->HasTry() && !this->func->DoOptimizeTryCatch())
    {
        return;
    }

    if (labelInstr->m_isLoopTop)
    {
        this->SaveRegContent(labelInstr);
        Lifetime ** regContent = AnewArrayZ(this->tempAlloc, Lifetime *, RegNumCount);
        js_memcpy_s(regContent, (RegNumCount * sizeof(Lifetime *)), this->regContent, sizeof(this->regContent));
        this->curLoop->regAlloc.loopTopRegContent = regContent;
    }

    FOREACH_SLISTCOUNTED_ENTRY_EDITING(IR::BranchInstr *, branchInstr, &labelInstr->labelRefs, iter)
    {
        if (branchInstr->m_isAirlock)
        {
            // This branch was just inserted... Skip it.
            continue;
        }

        Assert(branchInstr->GetNumber() && labelInstr->GetNumber());
        if (branchInstr->GetNumber() < labelInstr->GetNumber())
        {
            // Normal branch
            this->InsertSecondChanceCompensation(branchInstr->m_regContent, this->regContent, branchInstr, labelInstr);
        }
        else
        {
            // Loop back-edge
            Assert(labelInstr->m_isLoopTop);
        }
    } NEXT_SLISTCOUNTED_ENTRY_EDITING;
}

IR::Instr * LinearScan::EnsureAirlock(bool needsAirlock, bool *pHasAirlock, IR::Instr *insertionInstr,
                                      IR::Instr **pInsertionStartInstr, IR::BranchInstr *branchInstr, IR::LabelInstr *labelInstr)
{
    if (needsAirlock && !(*pHasAirlock))
    {
        // We need an extra block for the compensation code.
        insertionInstr = this->InsertAirlock(branchInstr, labelInstr);
        *pInsertionStartInstr = insertionInstr->m_prev;
        *pHasAirlock = true;
    }
    return insertionInstr;
}

bool LinearScan::NeedsLoopBackEdgeCompensation(Lifetime *lifetime, IR::LabelInstr *loopTopLabel)
{
    if (!lifetime)
    {
        return false;
    }

    if (lifetime->sym->IsConst())
    {
        return false;
    }

    // No need if lifetime begins in the loop
    if (lifetime->start > loopTopLabel->GetNumber())
    {
        return false;
    }

    // Only needed if lifetime is live on the back-edge, and the register is used inside the loop, or the lifetime extends
    // beyond the loop (and compensation out of the loop may use this reg)...
    if (!loopTopLabel->GetLoop()->regAlloc.liveOnBackEdgeSyms->Test(lifetime->sym->m_id)
        || (this->currentInstr->GetNumber() >= lifetime->end && !this->curLoop->regAlloc.symRegUseBv->Test(lifetime->sym->m_id)))
    {
        return false;
    }

    return true;
}

void
LinearScan::InsertSecondChanceCompensation(Lifetime ** branchRegContent, Lifetime **labelRegContent,
                                           IR::BranchInstr *branchInstr, IR::LabelInstr *labelInstr)
{
    IR::Instr *prevInstr = branchInstr->GetPrevRealInstrOrLabel();
    bool needsAirlock = branchInstr->IsConditional() || (prevInstr->IsBranchInstr() && prevInstr->AsBranchInstr()->IsConditional() || branchInstr->IsMultiBranch());
    bool hasAirlock = false;
    IR::Instr *insertionInstr = branchInstr;
    IR::Instr *insertionStartInstr = branchInstr->m_prev;
    // For loop back-edge, we want to keep the insertionStartInstr before the branch as spill need to happen on all paths
    // Pass a dummy instr address to airLockBlock insertion code.
    BitVector thrashedRegs(0);
    bool isLoopBackEdge = (this->regContent == branchRegContent);
    Lifetime * tmpRegContent[RegNumCount];
    Lifetime **regContent = this->regContent;

    if (isLoopBackEdge)
    {
        Loop *loop = labelInstr->GetLoop();

        js_memcpy_s(&tmpRegContent, (RegNumCount * sizeof(Lifetime *)), this->regContent, sizeof(this->regContent));

        branchRegContent = tmpRegContent;
        regContent = tmpRegContent;

#if defined(_M_IX86) || defined(_M_X64)
        // Insert XCHG to avoid some conflicts for int regs
        // Note: no XCHG on ARM or SSE2.  We could however use 3 XOR on ARM...
        this->AvoidCompensationConflicts(labelInstr, branchInstr, labelRegContent, branchRegContent,
                                         &insertionInstr, &insertionStartInstr, needsAirlock, &hasAirlock);
#endif


        FOREACH_BITSET_IN_UNITBV(reg, this->secondChanceRegs, BitVector)
        {
            Lifetime *labelLifetime = labelRegContent[reg];
            Lifetime *lifetime = branchRegContent[reg];

            // 1.  Insert Stores
            //          Lifetime starts before the loop
            //          Lifetime was re-allocated within the loop (i.e.: a load was most likely inserted)
            //          Lifetime is live on back-edge and has unsaved defs.

            if (lifetime && lifetime->start < labelInstr->GetNumber() && lifetime->lastAllocationStart > labelInstr->GetNumber()
                && (labelInstr->GetLoop()->regAlloc.liveOnBackEdgeSyms->Test(lifetime->sym->m_id))
                && !lifetime->defList.Empty())
            {
                insertionInstr = this->EnsureAirlock(needsAirlock, &hasAirlock, insertionInstr, &insertionStartInstr, branchInstr, labelInstr);
                // If the lifetime was second chance allocated inside the loop, there might
                // be spilled loads of this symbol in the loop.  Insert the stores.
                // We don't need to do this if the lifetime was re-allocated before the loop.
                //
                // Note that reg may not be equal to lifetime->reg because of inserted XCHG...
                this->InsertStores(lifetime, lifetime->reg, insertionStartInstr);
            }

            if (lifetime == labelLifetime)
            {
                continue;
            }

            // 2.   MOV labelReg/MEM, branchReg
            //          Move current register to match content at the top of the loop
            if (this->NeedsLoopBackEdgeCompensation(lifetime, labelInstr))
            {
                // Mismatch, we need to insert compensation code
                insertionInstr = this->EnsureAirlock(needsAirlock, &hasAirlock, insertionInstr, &insertionStartInstr, branchInstr, labelInstr);

                // MOV ESI, EAX
                // MOV EDI, ECX
                // MOV ECX, ESI
                // MOV EAX, EDI <<<
                this->ReconcileRegContent(branchRegContent, labelRegContent, branchInstr, labelInstr,
                                          lifetime, (RegNum)reg, &thrashedRegs, insertionInstr, insertionStartInstr);
            }

            // 2.   MOV labelReg, MEM
            //          Lifetime was in a reg at the top of the loop but is spilled right now.
            if (labelLifetime && labelLifetime->isSpilled && !labelLifetime->sym->IsConst() && labelLifetime->end >= branchInstr->GetNumber())
            {
                if (!loop->regAlloc.liveOnBackEdgeSyms->Test(labelLifetime->sym->m_id))
                {
                    continue;
                }
                if (this->ClearLoopExitIfRegUnused(labelLifetime, (RegNum)reg, branchInstr, loop))
                {
                    continue;
                }
                insertionInstr = this->EnsureAirlock(needsAirlock, &hasAirlock, insertionInstr, &insertionStartInstr, branchInstr, labelInstr);

                this->ReconcileRegContent(branchRegContent, labelRegContent, branchInstr, labelInstr,
                                          labelLifetime, (RegNum)reg, &thrashedRegs, insertionInstr, insertionStartInstr);
            }
        } NEXT_BITSET_IN_UNITBV;

        // 3.   MOV labelReg, MEM
        //          Finish up reloading lifetimes needed at the top.  #2 only handled secondChanceRegs.

        FOREACH_REG(reg)
        {
            // Handle lifetimes in a register at the top of the loop, but not currently.
            Lifetime *labelLifetime = labelRegContent[reg];
            if (labelLifetime && !labelLifetime->sym->IsConst() && labelLifetime != branchRegContent[reg] && !thrashedRegs.Test(reg)
                && (loop->regAlloc.liveOnBackEdgeSyms->Test(labelLifetime->sym->m_id)))
            {
                if (this->ClearLoopExitIfRegUnused(labelLifetime, (RegNum)reg, branchInstr, loop))
                {
                    continue;
                }
                // Mismatch, we need to insert compensation code
                insertionInstr = this->EnsureAirlock(needsAirlock, &hasAirlock, insertionInstr, &insertionStartInstr, branchInstr, labelInstr);

                this->ReconcileRegContent(branchRegContent, labelRegContent, branchInstr, labelInstr,
                                            labelLifetime, (RegNum)reg, &thrashedRegs, insertionInstr, insertionStartInstr);
            }
        } NEXT_REG;

        if (hasAirlock)
        {
            loop->regAlloc.hasAirLock = true;
        }
    }
    else
    {
        //
        // Non-loop-back-edge merge
        //
        FOREACH_REG(reg)
        {
            Lifetime *branchLifetime = branchRegContent[reg];
            Lifetime *lifetime = regContent[reg];

            if (lifetime == branchLifetime)
            {
                continue;
            }

            if (branchLifetime && branchLifetime->isSpilled && !branchLifetime->sym->IsConst() && branchLifetime->end > labelInstr->GetNumber())
            {
                // The lifetime was in a reg at the branch and is now spilled.  We need a store on this path.
                //
                //  MOV  MEM, branch_REG
                insertionInstr = this->EnsureAirlock(needsAirlock, &hasAirlock, insertionInstr, &insertionStartInstr, branchInstr, labelInstr);
                this->ReconcileRegContent(branchRegContent, labelRegContent, branchInstr, labelInstr,
                                          branchLifetime, (RegNum)reg, &thrashedRegs, insertionInstr, insertionStartInstr);
            }
            if (lifetime && !lifetime->sym->IsConst() && lifetime->start <= branchInstr->GetNumber())
            {
                // MOV  label_REG, branch_REG / MEM
                insertionInstr = this->EnsureAirlock(needsAirlock, &hasAirlock, insertionInstr, &insertionStartInstr, branchInstr, labelInstr);
                this->ReconcileRegContent(branchRegContent, labelRegContent, branchInstr, labelInstr,
                                          lifetime, (RegNum)reg, &thrashedRegs, insertionInstr, insertionStartInstr);
            }
        } NEXT_REG;
    }

    if (hasAirlock)
    {
        // Fix opHelper on airlock label.
        if (insertionInstr->m_prev->IsLabelInstr() && insertionInstr->IsLabelInstr())
        {
            if (insertionInstr->m_prev->AsLabelInstr()->isOpHelper && !insertionInstr->AsLabelInstr()->isOpHelper)
            {
                insertionInstr->m_prev->AsLabelInstr()->isOpHelper = false;
            }
        }
    }
}

void
LinearScan::ReconcileRegContent(Lifetime ** branchRegContent, Lifetime **labelRegContent,
                                IR::BranchInstr *branchInstr, IR::LabelInstr *labelInstr,
                                Lifetime *lifetime, RegNum reg, BitVector *thrashedRegs, IR::Instr *insertionInstr, IR::Instr *insertionStartInstr)
{
    RegNum originalReg = RegNOREG;
    IRType type = RegTypes[reg];
    Assert(labelRegContent[reg] != branchRegContent[reg]);
    bool matchBranchReg = (branchRegContent[reg] == lifetime);
    Lifetime **originalRegContent = (matchBranchReg ? labelRegContent : branchRegContent);
    bool isLoopBackEdge = (branchInstr->GetNumber() > labelInstr->GetNumber());

    if (lifetime->sym->IsConst())
    {
        return;
    }

    // Look if this lifetime was in a different register in the previous block.
    // Split the search in 2 to speed this up.
    if (type == TyMachReg)
    {
        FOREACH_INT_REG(regIter)
        {
            if (originalRegContent[regIter] == lifetime)
            {
                originalReg = regIter;
                break;
            }
        } NEXT_INT_REG;
    }
    else
    {
        Assert(type == TyFloat64 || IRType_IsSimd128(type));

        FOREACH_FLOAT_REG(regIter)
        {
            if (originalRegContent[regIter] == lifetime)
            {
                originalReg = regIter;
                break;
            }
        } NEXT_FLOAT_REG;
    }

    RegNum branchReg, labelReg;
    if (matchBranchReg)
    {
        branchReg = reg;
        labelReg = originalReg;
    }
    else
    {
        branchReg = originalReg;
        labelReg = reg;
    }

    if (branchReg != RegNOREG && !thrashedRegs->Test(branchReg) && !lifetime->sym->IsConst())
    {
        Assert(branchRegContent[branchReg] == lifetime);
        if (labelReg != RegNOREG)
        {
            // MOV labelReg, branchReg
            Assert(labelRegContent[labelReg] == lifetime);
            IR::Instr *load = IR::Instr::New(LowererMD::GetLoadOp(type),
                IR::RegOpnd::New(lifetime->sym, labelReg, type, this->func),
                IR::RegOpnd::New(lifetime->sym, branchReg, type, this->func), this->func);

            insertionInstr->InsertBefore(load);
            load->CopyNumber(insertionInstr);

            // symRegUseBv needs to be set properly.  Unfortunately, we need to go conservative as we don't know
            // which allocation it was at the source of the branch.
            if (this->IsInLoop())
            {
                this->RecordLoopUse(lifetime, branchReg);
            }
            thrashedRegs->Set(labelReg);
        }
        else if (!lifetime->sym->IsSingleDef() && lifetime->needsStoreCompensation && !isLoopBackEdge)
        {
            Assert(!lifetime->sym->IsConst());
            Assert(matchBranchReg);
            Assert(branchRegContent[branchReg] == lifetime);

            // MOV mem, branchReg
            this->InsertStores(lifetime, branchReg, insertionInstr->m_prev);

            // symRegUseBv needs to be set properly.  Unfortunately, we need to go conservative as we don't know
            // which allocation it was at the source of the branch.
            if (this->IsInLoop())
            {
                this->RecordLoopUse(lifetime, branchReg);
            }
        }
    }
    else if (labelReg != RegNOREG)
    {
        Assert(labelRegContent[labelReg] == lifetime);
        Assert(lifetime->sym->IsConst() || lifetime->sym->IsAllocated());

        if (branchReg != RegNOREG && !lifetime->sym->IsSingleDef())
        {
            Assert(thrashedRegs->Test(branchReg));

            // We can't insert a "MOV labelReg, branchReg" at the insertion point
            // because branchReg was thrashed by a previous reload.
            // Look for that reload to see if we can insert before it.
            IR::Instr *newInsertionInstr = insertionInstr->m_prev;
            bool foundIt = false;
            while (LowererMD::IsAssign(newInsertionInstr))
            {
                IR::Opnd *dst = newInsertionInstr->GetDst();
                IR::Opnd *src = newInsertionInstr->GetSrc1();
                if (src->IsRegOpnd() && src->AsRegOpnd()->GetReg() == labelReg)
                {
                    // This uses labelReg, give up...
                    break;
                }
                if (dst->IsRegOpnd() && dst->AsRegOpnd()->GetReg() == branchReg)
                {
                    // Success!
                    foundIt = true;
                    break;
                }
                newInsertionInstr = newInsertionInstr->m_prev;
            }

            if (foundIt)
            {
                // MOV labelReg, branchReg
                Assert(labelRegContent[labelReg] == lifetime);
                IR::Instr *load = IR::Instr::New(LowererMD::GetLoadOp(type),
                    IR::RegOpnd::New(lifetime->sym, labelReg, type, this->func),
                    IR::RegOpnd::New(lifetime->sym, branchReg, type, this->func), this->func);

                newInsertionInstr->InsertBefore(load);
                load->CopyNumber(newInsertionInstr);

                // symRegUseBv needs to be set properly.  Unfortunately, we need to go conservative as we don't know
                // which allocation it was at the source of the branch.
                if (this->IsInLoop())
                {
                    this->RecordLoopUse(lifetime, branchReg);
                }
                thrashedRegs->Set(labelReg);
                return;
            }

            Assert(thrashedRegs->Test(branchReg));
            this->InsertStores(lifetime, branchReg, insertionStartInstr);
            // symRegUseBv needs to be set properly.  Unfortunately, we need to go conservative as we don't know
            // which allocation it was at the source of the branch.
            if (this->IsInLoop())
            {
                this->RecordLoopUse(lifetime, branchReg);
            }
        }

        // MOV labelReg, mem
        this->InsertLoad(insertionInstr, lifetime->sym, labelReg);

        thrashedRegs->Set(labelReg);
    }
    else if (!lifetime->sym->IsConst())
    {
        Assert(matchBranchReg);
        Assert(branchReg != RegNOREG);
        // The lifetime was in a register at the top of the loop, but we thrashed it with a previous reload...
        if (!lifetime->sym->IsSingleDef())
        {
            this->InsertStores(lifetime, branchReg, insertionStartInstr);
        }
#if DBG_DUMP
        if (PHASE_TRACE(Js::SecondChancePhase, this->func))
        {
            Output::Print(L"****** Spilling reg because of bad compensation code order: ");
            lifetime->sym->Dump();
            Output::Print(L"\n");
        }
#endif
    }
}

bool LinearScan::ClearLoopExitIfRegUnused(Lifetime *lifetime, RegNum reg, IR::BranchInstr *branchInstr, Loop *loop)
{
    // If a lifetime was enregistered into the loop and then spilled, we need compensation at the bottom
    // of the loop to reload the lifetime into that register.
    // If that lifetime was spilled before it was ever used, we don't need the compensation code.
    // We do however need to clear the regContent on any loop exit as the register will not
    // be available anymore on that path.
    // Note: If the lifetime was reloaded into the same register, we might clear the regContent unnecessarily...
    if (!PHASE_OFF(Js::ClearRegLoopExitPhase, this->func))
    {
        return false;
    }
    if (!loop->regAlloc.symRegUseBv->Test(lifetime->sym->m_id) && !lifetime->needsStoreCompensation)
    {
        if (lifetime->end > branchInstr->GetNumber())
        {
            FOREACH_SLIST_ENTRY(Lifetime **, regContent, loop->regAlloc.exitRegContentList)
            {
                if (regContent[reg] == lifetime)
                {
                    regContent[reg] = nullptr;
                }
            } NEXT_SLIST_ENTRY;
        }
        return true;
    }

    return false;
}
#if defined(_M_IX86) || defined(_M_X64)

void LinearScan::AvoidCompensationConflicts(IR::LabelInstr *labelInstr, IR::BranchInstr *branchInstr,
                                            Lifetime *labelRegContent[], Lifetime *branchRegContent[],
                                            IR::Instr **pInsertionInstr, IR::Instr **pInsertionStartInstr, bool needsAirlock, bool *pHasAirlock)
{
    bool changed = true;

    // Look for conflicts in the incoming compensation code:
    //      MOV     ESI, EAX
    //      MOV     ECX, ESI    <<  ESI was lost...
    // Using XCHG:
    //      XCHG    ESI, EAX
    //      MOV     ECX, EAX
    //
    // Note that we need to iterate while(changed) to catch all conflicts
    while(changed) {
        RegNum conflictRegs[RegNumCount] = {RegNOREG};
        changed = false;

        FOREACH_BITSET_IN_UNITBV(reg, this->secondChanceRegs, BitVector)
        {
            Lifetime *labelLifetime = labelRegContent[reg];
            Lifetime *lifetime = branchRegContent[reg];

            // We don't have an XCHG for SSE2 regs
            if (lifetime == labelLifetime || IRType_IsFloat(RegTypes[reg]))
            {
                continue;
            }

            if (this->NeedsLoopBackEdgeCompensation(lifetime, labelInstr))
            {
                // Mismatch, we need to insert compensation code
                *pInsertionInstr = this->EnsureAirlock(needsAirlock, pHasAirlock, *pInsertionInstr, pInsertionStartInstr, branchInstr, labelInstr);

                if (conflictRegs[reg] != RegNOREG)
                {
                    // Eliminate conflict with an XCHG
                    IR::RegOpnd *reg1 = IR::RegOpnd::New(branchRegContent[reg]->sym, (RegNum)reg, RegTypes[reg], this->func);
                    IR::RegOpnd *reg2 = IR::RegOpnd::New(branchRegContent[reg]->sym, conflictRegs[reg], RegTypes[reg], this->func);
                    IR::Instr *instrXchg = IR::Instr::New(Js::OpCode::XCHG, reg1, reg1, reg2, this->func);
                    (*pInsertionInstr)->InsertBefore(instrXchg);
                    instrXchg->CopyNumber(*pInsertionInstr);

                    Lifetime *tmpLifetime = branchRegContent[reg];
                    branchRegContent[reg] = branchRegContent[conflictRegs[reg]];
                    branchRegContent[conflictRegs[reg]] = tmpLifetime;
                    reg = conflictRegs[reg];

                    changed = true;
                }
                RegNum labelReg = RegNOREG;
                FOREACH_INT_REG(regIter)
                {
                    if (labelRegContent[regIter] == branchRegContent[reg])
                    {
                        labelReg = regIter;
                        break;
                    }
                } NEXT_INT_REG;

                if (labelReg != RegNOREG)
                {
                    conflictRegs[labelReg] = (RegNum)reg;
                }
            }
        } NEXT_BITSET_IN_UNITBV;
    }
}
#endif
RegNum
LinearScan::SecondChanceAllocation(Lifetime *lifetime, bool force)
{
    if (PHASE_OFF(Js::SecondChancePhase, this->func) || this->func->HasTry())
    {
        return RegNOREG;
    }

    // Don't start a second chance allocation from a helper block
    if (lifetime->dontAllocate || this->IsInHelperBlock() || lifetime->isDeadStore)
    {
        return RegNOREG;
    }

    Assert(lifetime->isSpilled);
    Assert(lifetime->sym->IsConst() || lifetime->sym->IsAllocated());

    RegNum oldReg = lifetime->reg;
    RegNum reg;

    if (lifetime->start == this->currentInstr->GetNumber() || lifetime->end == this->currentInstr->GetNumber())
    {
        // No point doing second chance if the lifetime ends here, or starts here (normal allocation would
        // have found a register if one is available).
        return RegNOREG;
    }
    if (lifetime->sym->IsConst())
    {
        // Can't second-chance allocate because we might have deleted the initial def instr, after
        //         having set the reg content on a forward branch...
        return RegNOREG;
    }

    lifetime->reg = RegNOREG;
    lifetime->isSecondChanceAllocated = true;
    reg = this->FindReg(lifetime, nullptr, force);
    lifetime->reg = oldReg;

    if (reg == RegNOREG)
    {
        lifetime->isSecondChanceAllocated = false;
        return reg;
    }

    // Success!! We're re-allocating this lifetime...

    this->SecondChanceAllocateToReg(lifetime, reg);

    return reg;
}

void LinearScan::SecondChanceAllocateToReg(Lifetime *lifetime, RegNum reg)
{
    RegNum oldReg = lifetime->reg;

    if (oldReg != RegNOREG && this->tempRegLifetimes[oldReg] == lifetime)
    {
        this->tempRegs.Clear(oldReg);
    }

    lifetime->isSpilled = false;
    lifetime->isSecondChanceAllocated = true;
    lifetime->lastAllocationStart = this->currentInstr->GetNumber();

    lifetime->reg = RegNOREG;
    this->AssignActiveReg(lifetime, reg);
    this->secondChanceRegs.Set(reg);

    lifetime->sym->scratch.linearScan.lifetime->useList.Clear();

#if DBG_DUMP
    if (PHASE_TRACE(Js::SecondChancePhase, this->func))
    {
        Output::Print(L"**** Second chance: ");
        lifetime->sym->Dump();
        Output::Print(L"\t Reg: %S  ", RegNames[reg]);
        Output::Print(L"  SpillCount:%d  Length:%d   Cost:%d  %S\n",
            lifetime->useCount, lifetime->end - lifetime->start, this->GetSpillCost(lifetime),
            lifetime->isLiveAcrossCalls ? "LiveAcrossCalls" : "");
    }
#endif
}

IR::Instr *
LinearScan::InsertAirlock(IR::BranchInstr *branchInstr, IR::LabelInstr *labelInstr)
{
    // Insert a new block on a flow arc:
    //   JEQ L1             JEQ L2
    //   ...        =>      ...
    //  <fallthrough>       JMP L1
    // L1:                 L2:
    //                       <new block>
    //                     L1:
    // An airlock is needed when we need to add code on a flow arc, and the code can't
    // be added directly at the source or sink of that flow arc without impacting other
    // code paths.

    bool isOpHelper = labelInstr->isOpHelper;

    if (!isOpHelper)
    {
        // Check if branch is coming from helper block.
        IR::Instr *prevLabel = branchInstr->m_prev;
        while (prevLabel && !prevLabel->IsLabelInstr())
        {
            prevLabel = prevLabel->m_prev;
        }
        if (prevLabel && prevLabel->AsLabelInstr()->isOpHelper)
        {
            isOpHelper = true;
        }
    }
    IR::LabelInstr *airlockLabel = IR::LabelInstr::New(Js::OpCode::Label, this->func, isOpHelper);
    airlockLabel->SetRegion(this->currentRegion);
#if DBG
    if (isOpHelper)
    {
        if (branchInstr->m_isHelperToNonHelperBranch)
        {
            labelInstr->m_noHelperAssert = true;
        }
        if (labelInstr->isOpHelper && labelInstr->m_noHelperAssert)
        {
            airlockLabel->m_noHelperAssert = true;
        }
    }
#endif
    bool replaced = branchInstr->ReplaceTarget(labelInstr, airlockLabel);
    Assert(replaced);

    IR::Instr * prevInstr = labelInstr->GetPrevRealInstrOrLabel();
    if (prevInstr->HasFallThrough())
    {
        IR::BranchInstr *branchOverAirlock = IR::BranchInstr::New(LowererMD::MDUncondBranchOpcode, labelInstr, this->func);
        prevInstr->InsertAfter(branchOverAirlock);
        branchOverAirlock->CopyNumber(prevInstr);
        prevInstr = branchOverAirlock;
        branchOverAirlock->m_isAirlock = true;
        branchOverAirlock->m_regContent = nullptr;
    }

    prevInstr->InsertAfter(airlockLabel);
    airlockLabel->CopyNumber(prevInstr);
    prevInstr = labelInstr->GetPrevRealInstrOrLabel();

    return labelInstr;
}

void
LinearScan::SaveRegContent(IR::Instr *instr)
{
    bool isLabelLoopTop = false;
    Lifetime ** regContent = AnewArrayZ(this->tempAlloc, Lifetime *, RegNumCount);

    if (instr->IsBranchInstr())
    {
        instr->AsBranchInstr()->m_regContent = regContent;
    }
    else
    {
        Assert(instr->IsLabelInstr());
        Assert(instr->AsLabelInstr()->m_isLoopTop);
        instr->AsLabelInstr()->m_regContent = regContent;
        isLabelLoopTop = true;
    }

    js_memcpy_s(regContent, (RegNumCount * sizeof(Lifetime *)), this->regContent, sizeof(this->regContent));

#if DBG
    FOREACH_SLIST_ENTRY(Lifetime *, lifetime, this->activeLiveranges)
    {
        Assert(regContent[lifetime->reg] == lifetime);
    } NEXT_SLIST_ENTRY;
#endif
}

bool LinearScan::RegsAvailable(IRType type)
{
    if (IRType_IsFloat(type) || IRType_IsSimd128(type))
    {
        return (this->floatRegUsedCount < FLOAT_REG_COUNT);
    }
    else
    {
        return (this->intRegUsedCount < INT_REG_COUNT);
    }
}

uint LinearScan::GetRemainingHelperLength(Lifetime *const lifetime)
{
    // Walk the helper block linked list starting from the next helper block until the end of the lifetime
    uint helperLength = 0;
    SList<OpHelperBlock>::Iterator it(opHelperBlockIter);
    Assert(it.IsValid());
    const uint end = max(currentInstr->GetNumber(), lifetime->end);
    do
    {
        const OpHelperBlock &helper = it.Data();
        const uint helperStart = helper.opHelperLabel->GetNumber();
        if(helperStart > end)
        {
            break;
        }

        const uint helperEnd = min(end, helper.opHelperEndInstr->GetNumber());
        helperLength += helperEnd - helperStart;
        if(helperEnd != helper.opHelperEndInstr->GetNumber() || !helper.opHelperEndInstr->IsLabelInstr())
        {
            // A helper block that ends at a label does not return to the function. Since this helper block does not end
            // at a label, include the end instruction as well.
            ++helperLength;
        }
    } while(it.Next());

    return helperLength;
}

uint LinearScan::CurrentOpHelperVisitedLength(IR::Instr *const currentInstr) const
{
    Assert(currentInstr);

    if(!currentOpHelperBlock)
    {
        return 0;
    }

    // Consider the current instruction to have not yet been visited
    Assert(currentInstr->GetNumber() >= currentOpHelperBlock->opHelperLabel->GetNumber());
    return currentInstr->GetNumber() - currentOpHelperBlock->opHelperLabel->GetNumber();
}

IR::Instr * LinearScan::TryHoistLoad(IR::Instr *instr, Lifetime *lifetime)
{
    // If we are loading a lifetime into a register inside a loop, try to hoist that load outside the loop
    // if that register hasn't been used yet.
    RegNum reg = lifetime->reg;
    IR::Instr *insertInstr = instr;

    if (PHASE_OFF(Js::RegHoistLoadsPhase, this->func))
    {
        return insertInstr;
    }

    if ((this->func->HasTry() && !this->func->DoOptimizeTryCatch()) || (this->currentRegion && this->currentRegion->GetType() != RegionTypeRoot))
    {
        return insertInstr;
    }

    // Register unused, and lifetime unused yet.
    if (this->IsInLoop() && !this->curLoop->regAlloc.regUseBv.Test(reg)
        && !this->curLoop->regAlloc.defdInLoopBv->Test(lifetime->sym->m_id)
        && !this->curLoop->regAlloc.symRegUseBv->Test(lifetime->sym->m_id)
        && !this->curLoop->regAlloc.hasAirLock)
    {
        // Let's hoist!
        insertInstr = insertInstr->m_prev;

        // Walk each instructions until the top of the loop looking for branches
        while (!insertInstr->IsLabelInstr() || !insertInstr->AsLabelInstr()->m_isLoopTop || !insertInstr->AsLabelInstr()->GetLoop()->IsDescendentOrSelf(this->curLoop))
        {
            if (insertInstr->IsBranchInstr() && insertInstr->AsBranchInstr()->m_regContent)
            {
                IR::BranchInstr *branchInstr = insertInstr->AsBranchInstr();
                // That lifetime might have been in another register coming into the loop, and spilled before used.
                // Clear the reg content.
                FOREACH_REG(regIter)
                {
                    if (branchInstr->m_regContent[regIter] == lifetime)
                    {
                        branchInstr->m_regContent[regIter] = nullptr;
                    }
                } NEXT_REG;
                // Set the regContent for that reg to the lifetime on this branch
                branchInstr->m_regContent[reg] = lifetime;
            }
            insertInstr = insertInstr->m_prev;
        }

        IR::LabelInstr *loopTopLabel = insertInstr->AsLabelInstr();

        // Set the reg content for the loop top correctly as well
        FOREACH_REG(regIter)
        {
            if (loopTopLabel->m_regContent[regIter] == lifetime)
            {
                loopTopLabel->m_regContent[regIter] = nullptr;
                this->curLoop->regAlloc.loopTopRegContent[regIter] = nullptr;
            }
        } NEXT_REG;

        Assert(loopTopLabel->GetLoop() == this->curLoop);
        loopTopLabel->m_regContent[reg] = lifetime;
        this->curLoop->regAlloc.loopTopRegContent[reg] = lifetime;

        this->RecordLoopUse(lifetime, reg);

        IR::LabelInstr *loopLandingPad = nullptr;

        Assert(loopTopLabel->GetNumber() != Js::Constants::NoByteCodeOffset);

        // Insert load in landing pad.
        // Redirect branches to new landing pad.
        FOREACH_SLISTCOUNTED_ENTRY_EDITING(IR::BranchInstr *, branchInstr, &loopTopLabel->labelRefs, iter)
        {
            Assert(branchInstr->GetNumber() != Js::Constants::NoByteCodeOffset);
            // <= because the branch may be newly inserted and have the same instr number as the loop top...
            if (branchInstr->GetNumber() <= loopTopLabel->GetNumber())
            {
                if (!loopLandingPad)
                {
                    loopLandingPad = IR::LabelInstr::New(Js::OpCode::Label, this->func);
                    loopLandingPad->SetRegion(this->currentRegion);
                    loopTopLabel->InsertBefore(loopLandingPad);
                    loopLandingPad->CopyNumber(loopTopLabel);
                }
                branchInstr->ReplaceTarget(loopTopLabel, loopLandingPad);
            }
        } NEXT_SLISTCOUNTED_ENTRY_EDITING;
    }

    return insertInstr;
}

#if DBG_DUMP

void LinearScan::PrintStats() const
{
    uint loopNest = 0;
    uint storeCount = 0;
    uint loadCount = 0;
    uint wStoreCount = 0;
    uint wLoadCount = 0;
    uint instrCount = 0;
    bool isInHelper = false;

    FOREACH_INSTR_IN_FUNC_BACKWARD(instr, this->func)
    {
        switch (instr->GetKind())
        {
        case IR::InstrKindPragma:
            continue;

        case IR::InstrKindBranch:
            if (instr->AsBranchInstr()->IsLoopTail(this->func))
            {
                loopNest++;
            }

            instrCount++;
            break;

        case IR::InstrKindLabel:
        case IR::InstrKindProfiledLabel:
            if (instr->AsLabelInstr()->m_isLoopTop)
            {
                Assert(loopNest);
                loopNest--;
            }

            isInHelper = instr->AsLabelInstr()->isOpHelper;
            break;

        default:
            {
                Assert(instr->IsRealInstr());

                if (isInHelper)
                {
                    continue;
                }

                IR::Opnd *dst = instr->GetDst();
                if (dst && dst->IsSymOpnd() && dst->AsSymOpnd()->m_sym->IsStackSym() && dst->AsSymOpnd()->m_sym->AsStackSym()->IsAllocated())
                {
                    storeCount++;
                    wStoreCount += LinearScan::GetUseSpillCost(loopNest, false);
                }
                IR::Opnd *src1 = instr->GetSrc1();
                if (src1)
                {
                    if (src1->IsSymOpnd() && src1->AsSymOpnd()->m_sym->IsStackSym() && src1->AsSymOpnd()->m_sym->AsStackSym()->IsAllocated())
                    {
                        loadCount++;
                        wLoadCount += LinearScan::GetUseSpillCost(loopNest, false);
                    }
                    IR::Opnd *src2 = instr->GetSrc2();
                    if (src2 && src2->IsSymOpnd() && src2->AsSymOpnd()->m_sym->IsStackSym() && src2->AsSymOpnd()->m_sym->AsStackSym()->IsAllocated())
                    {
                        loadCount++;
                        wLoadCount += LinearScan::GetUseSpillCost(loopNest, false);
                    }
                }
            }
            break;
        }
    } NEXT_INSTR_IN_FUNC_BACKWARD;

    Assert(loopNest == 0);

    this->func->GetJnFunction()->DumpFullFunctionName();
    Output::SkipToColumn(45);

    Output::Print(L"Instrs:%5d, Lds:%4d, Strs:%4d, WLds: %4d, WStrs: %4d, WRefs: %4d\n",
        instrCount, loadCount, storeCount, wLoadCount, wStoreCount, wLoadCount+wStoreCount);
}

#endif


#ifdef _M_IX86

# if ENABLE_DEBUG_CONFIG_OPTIONS

IR::Instr * LinearScan::GetIncInsertionPoint(IR::Instr *instr)
{
    // Make sure we don't insert an INC between an instr setting the condition code, and one using it.
    IR::Instr *instrNext = instr;
    while(!EncoderMD::UsesConditionCode(instrNext) && !EncoderMD::SetsConditionCode(instrNext))
    {
        if (instrNext->IsLabelInstr() || instrNext->IsExitInstr() || instrNext->IsBranchInstr())
        {
            break;
        }
        instrNext = instrNext->GetNextRealInstrOrLabel();
    }

    if (instrNext->IsLowered() && EncoderMD::UsesConditionCode(instrNext))
    {
        IR::Instr *instrPrev = instr->GetPrevRealInstrOrLabel();
        while(!EncoderMD::SetsConditionCode(instrPrev))
        {
            instrPrev = instrPrev->GetPrevRealInstrOrLabel();
            Assert(!instrPrev->IsLabelInstr());
        }

        return instrPrev;
    }

    return instr;
}

void LinearScan::DynamicStatsInstrument()
{
    IR::Instr *firstInstr = this->func->m_headInstr;
    IR::MemRefOpnd *memRefOpnd = IR::MemRefOpnd::New(&(this->func->GetJnFunction()->callCountStats), TyUint32, this->func);
    firstInstr->InsertAfter(IR::Instr::New(Js::OpCode::INC, memRefOpnd, memRefOpnd, this->func));

    FOREACH_INSTR_IN_FUNC(instr, this->func)
    {
        if (!instr->IsRealInstr() || !instr->IsLowered())
        {
            continue;
        }

        if (EncoderMD::UsesConditionCode(instr) && instr->GetPrevRealInstrOrLabel()->IsLabelInstr())
        {
            continue;
        }

        IR::Opnd *dst = instr->GetDst();
        if (dst && dst->IsSymOpnd() && dst->AsSymOpnd()->m_sym->IsStackSym() && dst->AsSymOpnd()->m_sym->AsStackSym()->IsAllocated())
        {
            IR::Instr *insertionInstr = this->GetIncInsertionPoint(instr);
            IR::MemRefOpnd *memRefOpnd = IR::MemRefOpnd::New(&(this->func->GetJnFunction()->regAllocStoreCount), TyUint32, this->func);
            insertionInstr->InsertBefore(IR::Instr::New(Js::OpCode::INC, memRefOpnd, memRefOpnd, this->func));
        }
        IR::Opnd *src1 = instr->GetSrc1();
        if (src1)
        {
            if (src1->IsSymOpnd() && src1->AsSymOpnd()->m_sym->IsStackSym() && src1->AsSymOpnd()->m_sym->AsStackSym()->IsAllocated())
            {
                IR::Instr *insertionInstr = this->GetIncInsertionPoint(instr);
                IR::MemRefOpnd *memRefOpnd = IR::MemRefOpnd::New(&(this->func->GetJnFunction()->regAllocLoadCount), TyUint32, this->func);
                insertionInstr->InsertBefore(IR::Instr::New(Js::OpCode::INC, memRefOpnd, memRefOpnd, this->func));
            }
            IR::Opnd *src2 = instr->GetSrc2();
            if (src2 && src2->IsSymOpnd() && src2->AsSymOpnd()->m_sym->IsStackSym() && src2->AsSymOpnd()->m_sym->AsStackSym()->IsAllocated())
            {
                IR::Instr *insertionInstr = this->GetIncInsertionPoint(instr);
                IR::MemRefOpnd *memRefOpnd = IR::MemRefOpnd::New(&(this->func->GetJnFunction()->regAllocLoadCount), TyUint32, this->func);
                insertionInstr->InsertBefore(IR::Instr::New(Js::OpCode::INC, memRefOpnd, memRefOpnd, this->func));
            }
        }
    } NEXT_INSTR_IN_FUNC;
}

# endif  //ENABLE_DEBUG_CONFIG_OPTIONS
#endif  // _M_IX86

IR::Instr* LinearScan::InsertMove(IR::Opnd *dst, IR::Opnd *src, IR::Instr *const insertBeforeInstr)
{
    IR::Instr *instrPrev = insertBeforeInstr->m_prev;

    IR::Instr *instrRet = Lowerer::InsertMove(dst, src, insertBeforeInstr);

    for (IR::Instr *instr = instrPrev->m_next; instr != insertBeforeInstr; instr = instr->m_next)
    {
        instr->CopyNumber(insertBeforeInstr);
    }

    return instrRet;
}
