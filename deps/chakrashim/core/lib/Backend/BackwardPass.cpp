//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
class JitArenaAllocator;
template <>
void
BVSparse<JitArenaAllocator>::QueueInFreeList(BVSparseNode *curNode)
{
    AllocatorDeleteInline(JitArenaAllocator, this->alloc, curNode);
}

#include "BackEnd.h"

#define INLINEEMETAARG_COUNT 3

BackwardPass::BackwardPass(Func * func, GlobOpt * globOpt, Js::Phase tag)
    : func(func), globOpt(globOpt), tag(tag), currentPrePassLoop(nullptr), tempAlloc(nullptr),
    preOpBailOutInstrToProcess(nullptr),
    considerSymAsRealUseInNoImplicitCallUses(nullptr),
    isCollectionPass(false), currentRegion(nullptr)
{
    // Those are the only two phase dead store will be used currently
    Assert(tag == Js::BackwardPhase || tag == Js::DeadStorePhase);

    this->implicitCallBailouts = 0;
    this->fieldOpts = 0;

#if DBG_DUMP
    this->numDeadStore = 0;
    this->numMarkTempNumber = 0;
    this->numMarkTempNumberTransfered = 0;
    this->numMarkTempObject = 0;
#endif
}

bool
BackwardPass::DoSetDead() const
{
    // Note: Dead bit on the Opnd records flow-based liveness.
    // This is distinct from isLastUse, which records lexical last-ness.
    return this->tag == Js::BackwardPhase && !this->IsPrePass();
}

bool
BackwardPass::DoByteCodeUpwardExposedUsed() const
{
    return (this->tag == Js::DeadStorePhase && this->func->hasBailout) ||
        (this->func->HasTry() && this->func->DoOptimizeTryCatch() && this->tag == Js::BackwardPhase);
}

bool
BackwardPass::DoFieldHoistCandidates() const
{
    return DoFieldHoistCandidates(this->currentBlock->loop);
}

bool
BackwardPass::DoFieldHoistCandidates(Loop * loop) const
{
    // We only need to do one pass to generate this data
    return this->tag == Js::BackwardPhase
        && !this->IsPrePass() && loop && GlobOpt::DoFieldHoisting(loop);
}

bool
BackwardPass::DoMarkTempNumbers() const
{
#if FLOATVAR
    return false;
#else
    // only mark temp number on the dead store phase
    return (tag == Js::DeadStorePhase) && !PHASE_OFF(Js::MarkTempPhase, this->func) &&
        !PHASE_OFF(Js::MarkTempNumberPhase, this->func) && func->DoFastPaths() && (!this->func->HasTry());
#endif
}

bool
BackwardPass::DoMarkTempObjects() const
{
    // only mark temp object on the backward store phase
    return (tag == Js::BackwardPhase) && !PHASE_OFF(Js::MarkTempPhase, this->func) &&
        !PHASE_OFF(Js::MarkTempObjectPhase, this->func) && func->DoGlobOpt() && func->GetHasTempObjectProducingInstr() &&
        !func->IsJitInDebugMode() &&
        func->DoGlobOptsForGeneratorFunc();

    // Why MarkTempObject is disabled under debugger:
    //   We add 'identified so far dead non-temp locals' to byteCodeUpwardExposedUsed in ProcessBailOutInfo,
    //   this may cause MarkTempObject to convert some temps back to non-temp when it sees a 'transfered exposed use'
    //   from a temp to non-temp. That's in general not a supported conversion (while non-temp -> temp is fine).
}

bool
BackwardPass::DoMarkTempNumbersOnTempObjects() const
{
    return !PHASE_OFF(Js::MarkTempNumberOnTempObjectPhase, this->func) && DoMarkTempNumbers() && this->func->GetHasMarkTempObjects();
}

#if DBG
bool
BackwardPass::DoMarkTempObjectVerify() const
{
    // only mark temp object on the backward store phase
    return (tag == Js::DeadStorePhase) && !PHASE_OFF(Js::MarkTempPhase, this->func) &&
        !PHASE_OFF(Js::MarkTempObjectPhase, this->func) && func->DoGlobOpt() && func->GetHasTempObjectProducingInstr();
}
#endif

// static
bool
BackwardPass::DoDeadStore(Func* func)
{
    return
        !PHASE_OFF(Js::DeadStorePhase, func) &&
        (!func->HasTry() || func->DoOptimizeTryCatch());
}

bool
BackwardPass::DoDeadStore() const
{
    return
        this->tag == Js::DeadStorePhase &&
        DoDeadStore(this->func);
}

bool
BackwardPass::DoDeadStoreSlots() const
{
    // only dead store fields if glob opt is on to generate the trackable fields bitvector
    return (tag == Js::DeadStorePhase && this->func->DoGlobOpt()
        && (!this->func->HasTry()));
}

// Whether dead store is enabled for given func and sym.
// static
bool
BackwardPass::DoDeadStore(Func* func, StackSym* sym)
{
    // Dead store is disabled under debugger for non-temp local vars.
    return
        DoDeadStore(func) &&
        !(func->IsJitInDebugMode() && sym->HasByteCodeRegSlot() && func->IsNonTempLocalVar(sym->GetByteCodeRegSlot())) &&
        func->DoGlobOptsForGeneratorFunc();
}

bool
BackwardPass::DoTrackNegativeZero() const
{
    return
        !PHASE_OFF(Js::TrackIntUsagePhase, func) &&
        !PHASE_OFF(Js::TrackNegativeZeroPhase, func) &&
        func->DoGlobOpt() &&
        !IsPrePass() &&
        !func->IsJitInDebugMode() &&
        func->DoGlobOptsForGeneratorFunc();
}

bool
BackwardPass::DoTrackBitOpsOrNumber() const
{
#if _WIN64
    return
        !PHASE_OFF1(Js::TypedArrayVirtualPhase) &&
        tag == Js::BackwardPhase &&
        func->DoGlobOpt() &&
        !IsPrePass() &&
        !func->IsJitInDebugMode() &&
        func->DoGlobOptsForGeneratorFunc();
#else
    return false;
#endif
}

bool
BackwardPass::DoTrackIntOverflow() const
{
    return
        !PHASE_OFF(Js::TrackIntUsagePhase, func) &&
        !PHASE_OFF(Js::TrackIntOverflowPhase, func) &&
        tag == Js::BackwardPhase &&
        !IsPrePass() &&
        globOpt->DoLossyIntTypeSpec() &&
        !func->IsJitInDebugMode() &&
        func->DoGlobOptsForGeneratorFunc();
}

bool
BackwardPass::DoTrackCompoundedIntOverflow() const
{
    return
        !PHASE_OFF(Js::TrackCompoundedIntOverflowPhase, func) &&
        DoTrackIntOverflow() &&
        !func->GetProfileInfo()->IsTrackCompoundedIntOverflowDisabled();
}

bool
BackwardPass::DoTrackNon32BitOverflow() const
{
    // enabled only for IA
#if defined(_M_IX86) || defined(_M_X64)
    return true;
#else
    return false;
#endif
}

void
BackwardPass::CleanupBackwardPassInfoInFlowGraph()
{
    if (!this->func->m_fg->hasBackwardPassInfo)
    {
        // No information to clean up
        return;
    }

    // The backward pass temp arena has already been deleted, we can just reset the data

    FOREACH_BLOCK_IN_FUNC_DEAD_OR_ALIVE(block, this->func)
    {
        block->upwardExposedUses = nullptr;
        block->upwardExposedFields = nullptr;
        block->typesNeedingKnownObjectLayout = nullptr;
        block->fieldHoistCandidates = nullptr;
        block->slotDeadStoreCandidates = nullptr;
        block->byteCodeUpwardExposedUsed = nullptr;
#if DBG
        block->byteCodeRestoreSyms = nullptr;
#endif
        block->tempNumberTracker = nullptr;
        block->tempObjectTracker = nullptr;
#if DBG
        block->tempObjectVerifyTracker = nullptr;
#endif
        block->stackSymToFinalType = nullptr;
        block->stackSymToGuardedProperties = nullptr;
        block->stackSymToWriteGuardsMap = nullptr;
        block->cloneStrCandidates = nullptr;
        block->noImplicitCallUses = nullptr;
        block->noImplicitCallNoMissingValuesUses = nullptr;
        block->noImplicitCallNativeArrayUses = nullptr;
        block->noImplicitCallJsArrayHeadSegmentSymUses = nullptr;
        block->noImplicitCallArrayLengthSymUses = nullptr;

        if (block->loop != nullptr)
        {
            block->loop->hasDeadStoreCollectionPass = false;
            block->loop->hasDeadStorePrepass = false;
        }
    }
    NEXT_BLOCK_IN_FUNC_DEAD_OR_ALIVE;
}
void
BackwardPass::Optimize()
{
    if (tag == Js::BackwardPhase && PHASE_OFF(tag, this->func))
    {
        return;
    }

    NoRecoverMemoryJitArenaAllocator localAlloc(tag == Js::BackwardPhase? L"BE-Backward" : L"BE-DeadStore",
        this->func->m_alloc->GetPageAllocator(), Js::Throw::OutOfMemory);

    this->tempAlloc = &localAlloc;
#if DBG_DUMP
    if (this->IsTraceEnabled())
    {
        this->func->DumpHeader();
    }
#endif

    this->CleanupBackwardPassInfoInFlowGraph();

    // Info about whether a sym is used in a way in which -0 differs from +0, or whether the sym is used in a way in which an
    // int32 overflow when generating the value of the sym matters, in the current block. The info is transferred to
    // instructions that define the sym in the current block as they are encountered. The info in these bit vectors is discarded
    // after optimizing each block, so the only info that remains for GlobOpt is that which is transferred to instructions.
    BVSparse<JitArenaAllocator> localNegativeZeroDoesNotMatterBySymId(tempAlloc);
    negativeZeroDoesNotMatterBySymId = &localNegativeZeroDoesNotMatterBySymId;

    BVSparse<JitArenaAllocator> localSymUsedOnlyForBitOpsBySymId(tempAlloc);
    symUsedOnlyForBitOpsBySymId = &localSymUsedOnlyForBitOpsBySymId;
    BVSparse<JitArenaAllocator> localSymUsedOnlyForNumberBySymId(tempAlloc);
    symUsedOnlyForNumberBySymId = &localSymUsedOnlyForNumberBySymId;

    BVSparse<JitArenaAllocator> localIntOverflowDoesNotMatterBySymId(tempAlloc);
    intOverflowDoesNotMatterBySymId = &localIntOverflowDoesNotMatterBySymId;
    BVSparse<JitArenaAllocator> localIntOverflowDoesNotMatterInRangeBySymId(tempAlloc);
    intOverflowDoesNotMatterInRangeBySymId = &localIntOverflowDoesNotMatterInRangeBySymId;
    BVSparse<JitArenaAllocator> localCandidateSymsRequiredToBeInt(tempAlloc);
    candidateSymsRequiredToBeInt = &localCandidateSymsRequiredToBeInt;
    BVSparse<JitArenaAllocator> localCandidateSymsRequiredToBeLossyInt(tempAlloc);
    candidateSymsRequiredToBeLossyInt = &localCandidateSymsRequiredToBeLossyInt;
    intOverflowCurrentlyMattersInRange = true;

    FloatSymEquivalenceMap localFloatSymEquivalenceMap(tempAlloc);
    floatSymEquivalenceMap = &localFloatSymEquivalenceMap;

    NumberTempRepresentativePropertySymMap localNumberTempRepresentativePropertySym(tempAlloc);
    numberTempRepresentativePropertySym = &localNumberTempRepresentativePropertySym;

    FOREACH_BLOCK_BACKWARD_IN_FUNC_DEAD_OR_ALIVE(block, this->func)
    {
        this->OptBlock(block);
    }
    NEXT_BLOCK_BACKWARD_IN_FUNC_DEAD_OR_ALIVE;

    if (this->tag == Js::DeadStorePhase && !PHASE_OFF(Js::MemOpPhase, this->func))
    {
        this->RemoveEmptyLoops();
    }
    this->func->m_fg->hasBackwardPassInfo = true;

    if(DoTrackCompoundedIntOverflow())
    {
        // Tracking int overflow makes use of a scratch field in stack syms, which needs to be cleared
        func->m_symTable->ClearStackSymScratch();
    }

#if DBG_DUMP
    if (PHASE_STATS(this->tag, this->func))
    {
        this->func->DumpHeader();
        Output::Print(this->tag == Js::BackwardPhase? L"Backward Phase Stats:\n" : L"Deadstore Phase Stats:\n");
        if (this->DoDeadStore())
        {
            Output::Print(L"  Deadstore             : %3d\n", this->numDeadStore);
        }
        if (this->DoMarkTempNumbers())
        {
            Output::Print(L"  Temp Number           : %3d\n", this->numMarkTempNumber);
            Output::Print(L"  Transfered Temp Number: %3d\n", this->numMarkTempNumberTransfered);
        }
        if (this->DoMarkTempObjects())
        {
            Output::Print(L"  Temp Object           : %3d\n", this->numMarkTempObject);
        }
    }
#endif
}

void
BackwardPass::MergeSuccBlocksInfo(BasicBlock * block)
{
    // Can't reuse the bv in the current block, because its successor can be itself.

    TempNumberTracker * tempNumberTracker = nullptr;
    TempObjectTracker * tempObjectTracker = nullptr;
#if DBG
    TempObjectVerifyTracker * tempObjectVerifyTracker = nullptr;
#endif
    HashTable<AddPropertyCacheBucket> * stackSymToFinalType = nullptr;
    HashTable<ObjTypeGuardBucket> * stackSymToGuardedProperties = nullptr;
    HashTable<ObjWriteGuardBucket> * stackSymToWriteGuardsMap = nullptr;
    BVSparse<JitArenaAllocator> * cloneStrCandidates = nullptr;
    BVSparse<JitArenaAllocator> * noImplicitCallUses = nullptr;
    BVSparse<JitArenaAllocator> * noImplicitCallNoMissingValuesUses = nullptr;
    BVSparse<JitArenaAllocator> * noImplicitCallNativeArrayUses = nullptr;
    BVSparse<JitArenaAllocator> * noImplicitCallJsArrayHeadSegmentSymUses = nullptr;
    BVSparse<JitArenaAllocator> * noImplicitCallArrayLengthSymUses = nullptr;
    BVSparse<JitArenaAllocator> * upwardExposedUses = nullptr;
    BVSparse<JitArenaAllocator> * upwardExposedFields = nullptr;
    BVSparse<JitArenaAllocator> * typesNeedingKnownObjectLayout = nullptr;
    BVSparse<JitArenaAllocator> * fieldHoistCandidates = nullptr;
    BVSparse<JitArenaAllocator> * slotDeadStoreCandidates = nullptr;
    BVSparse<JitArenaAllocator> * byteCodeUpwardExposedUsed = nullptr;
#if DBG
    uint byteCodeLocalsCount = func->GetJnFunction()->GetLocalsCount();
    StackSym ** byteCodeRestoreSyms = nullptr;
#endif

    Assert(!block->isDead || block->GetSuccList()->Empty());

    if (this->DoByteCodeUpwardExposedUsed())
    {
        byteCodeUpwardExposedUsed = JitAnew(this->tempAlloc, BVSparse<JitArenaAllocator>, this->tempAlloc);
#if DBG
        byteCodeRestoreSyms = JitAnewArrayZ(this->tempAlloc, StackSym *, byteCodeLocalsCount);
#endif
    }

#if DBG
    if (!IsCollectionPass() && this->DoMarkTempObjectVerify())
    {
        tempObjectVerifyTracker = JitAnew(this->tempAlloc, TempObjectVerifyTracker, this->tempAlloc, block->loop != nullptr);
    }
#endif

    if (!block->isDead)
    {
        bool keepUpwardExposed = (this->tag == Js::BackwardPhase);
        JitArenaAllocator *upwardExposedArena = nullptr;
        if(!IsCollectionPass())
        {
            upwardExposedArena = keepUpwardExposed ? this->globOpt->alloc : this->tempAlloc;
            upwardExposedUses = JitAnew(upwardExposedArena, BVSparse<JitArenaAllocator>, upwardExposedArena);
            upwardExposedFields = JitAnew(upwardExposedArena, BVSparse<JitArenaAllocator>, upwardExposedArena);

            if (this->tag == Js::DeadStorePhase)
            {
                typesNeedingKnownObjectLayout = JitAnew(this->tempAlloc, BVSparse<JitArenaAllocator>, this->tempAlloc);
            }

            if (this->DoFieldHoistCandidates())
            {
                fieldHoistCandidates = JitAnew(this->tempAlloc, BVSparse<JitArenaAllocator>, this->tempAlloc);
            }
            if (this->DoDeadStoreSlots())
            {
                slotDeadStoreCandidates = JitAnew(this->tempAlloc, BVSparse<JitArenaAllocator>, this->tempAlloc);
            }
            if (this->DoMarkTempNumbers())
            {
                tempNumberTracker = JitAnew(this->tempAlloc, TempNumberTracker, this->tempAlloc,  block->loop != nullptr);
            }
            if (this->DoMarkTempObjects())
            {
                tempObjectTracker = JitAnew(this->tempAlloc, TempObjectTracker, this->tempAlloc, block->loop != nullptr);
            }

            noImplicitCallUses = JitAnew(this->tempAlloc, BVSparse<JitArenaAllocator>, this->tempAlloc);
            noImplicitCallNoMissingValuesUses = JitAnew(this->tempAlloc, BVSparse<JitArenaAllocator>, this->tempAlloc);
            noImplicitCallNativeArrayUses = JitAnew(this->tempAlloc, BVSparse<JitArenaAllocator>, this->tempAlloc);
            noImplicitCallJsArrayHeadSegmentSymUses = JitAnew(this->tempAlloc, BVSparse<JitArenaAllocator>, this->tempAlloc);
            noImplicitCallArrayLengthSymUses = JitAnew(this->tempAlloc, BVSparse<JitArenaAllocator>, this->tempAlloc);
            if (this->tag == Js::BackwardPhase)
            {
                cloneStrCandidates = JitAnew(this->globOpt->alloc, BVSparse<JitArenaAllocator>, this->globOpt->alloc);
            }
        }

        bool firstSucc = true;
        FOREACH_SUCCESSOR_BLOCK(blockSucc, block)
        {
#if defined(DBG_DUMP) || defined(ENABLE_DEBUG_CONFIG_OPTIONS)

            wchar_t debugStringBuffer[MAX_FUNCTION_BODY_DEBUG_STRING_SIZE];
#endif
            // save the byteCodeUpwardExposedUsed from deleting for the block right after the memop loop
            if (this->tag == Js::DeadStorePhase && !this->IsPrePass() && globOpt->DoMemOp(block->loop) && blockSucc->loop != block->loop)
            {
                Assert(block->loop->memOpInfo->inductionVariablesUsedAfterLoop == nullptr);
                block->loop->memOpInfo->inductionVariablesUsedAfterLoop = JitAnew(this->tempAlloc, BVSparse<JitArenaAllocator>, this->tempAlloc);
                block->loop->memOpInfo->inductionVariablesUsedAfterLoop->Or(blockSucc->byteCodeUpwardExposedUsed);
                block->loop->memOpInfo->inductionVariablesUsedAfterLoop->Or(blockSucc->upwardExposedUses);
            }

            bool deleteData = false;
            if (!blockSucc->isLoopHeader && blockSucc->backwardPassCurrentLoop == this->currentPrePassLoop)
            {
                Assert(blockSucc->GetDataUseCount() != 0);
                deleteData = (blockSucc->DecrementDataUseCount() == 0);
            }

            Assert((byteCodeUpwardExposedUsed == nullptr) == !this->DoByteCodeUpwardExposedUsed());
            if (byteCodeUpwardExposedUsed && blockSucc->byteCodeUpwardExposedUsed)
            {
                byteCodeUpwardExposedUsed->Or(blockSucc->byteCodeUpwardExposedUsed);
                if (this->tag == Js::DeadStorePhase)
                {
#if DBG
                    for (uint i = 0; i < byteCodeLocalsCount; i++)
                    {
                        if (byteCodeRestoreSyms[i] == nullptr)
                        {
                            byteCodeRestoreSyms[i] = blockSucc->byteCodeRestoreSyms[i];
                        }
                        else
                        {
                            Assert(blockSucc->byteCodeRestoreSyms[i] == nullptr
                                || byteCodeRestoreSyms[i] == blockSucc->byteCodeRestoreSyms[i]);
                        }
                    }
#endif
                    if (deleteData)
                    {
                        // byteCodeUpwardExposedUsed is required to populate the writeThroughSymbolsSet for the try region. So, don't delete it in the backwards pass.
                        JitAdelete(this->tempAlloc, blockSucc->byteCodeUpwardExposedUsed);
                        blockSucc->byteCodeUpwardExposedUsed = nullptr;
                    }
                }
#if DBG
                if (deleteData)
                {
                    JitAdeleteArray(this->tempAlloc, byteCodeLocalsCount, blockSucc->byteCodeRestoreSyms);
                    blockSucc->byteCodeRestoreSyms = nullptr;
                }
#endif

            }
            else
            {
                Assert(blockSucc->byteCodeUpwardExposedUsed == nullptr);
                Assert(blockSucc->byteCodeRestoreSyms == nullptr);
            }

            if(IsCollectionPass())
            {
                continue;
            }

            Assert((blockSucc->upwardExposedUses != nullptr)
                || (blockSucc->isLoopHeader && (this->IsPrePass() || blockSucc->loop->IsDescendentOrSelf(block->loop))));
            Assert((blockSucc->upwardExposedFields != nullptr)
                || (blockSucc->isLoopHeader && (this->IsPrePass() || blockSucc->loop->IsDescendentOrSelf(block->loop))));
            Assert((blockSucc->typesNeedingKnownObjectLayout != nullptr)
                || (blockSucc->isLoopHeader && (this->IsPrePass() || blockSucc->loop->IsDescendentOrSelf(block->loop)))
                || this->tag != Js::DeadStorePhase);
            Assert((blockSucc->fieldHoistCandidates != nullptr)
                || blockSucc->isLoopHeader
                || !this->DoFieldHoistCandidates(blockSucc->loop));
            Assert((blockSucc->slotDeadStoreCandidates != nullptr)
                || (blockSucc->isLoopHeader && (this->IsPrePass() || blockSucc->loop->IsDescendentOrSelf(block->loop)))
                || !this->DoDeadStoreSlots());
            Assert((blockSucc->tempNumberTracker != nullptr)
                || (blockSucc->isLoopHeader && (this->IsPrePass() || blockSucc->loop->IsDescendentOrSelf(block->loop)))
                || !this->DoMarkTempNumbers());
            Assert((blockSucc->tempObjectTracker != nullptr)
                || (blockSucc->isLoopHeader && (this->IsPrePass() || blockSucc->loop->IsDescendentOrSelf(block->loop)))
                || !this->DoMarkTempObjects());
            Assert((blockSucc->tempObjectVerifyTracker != nullptr)
                || (blockSucc->isLoopHeader && (this->IsPrePass() || blockSucc->loop->IsDescendentOrSelf(block->loop)))
                || !this->DoMarkTempObjectVerify());
            if (blockSucc->upwardExposedUses != nullptr)
            {
                upwardExposedUses->Or(blockSucc->upwardExposedUses);

                if (deleteData && (!keepUpwardExposed
                                   || (this->IsPrePass() && blockSucc->backwardPassCurrentLoop == this->currentPrePassLoop)))
                {
                    JitAdelete(upwardExposedArena, blockSucc->upwardExposedUses);
                    blockSucc->upwardExposedUses = nullptr;
                }
            }

            if (blockSucc->upwardExposedFields != nullptr)
            {
                upwardExposedFields->Or(blockSucc->upwardExposedFields);

                if (deleteData && (!keepUpwardExposed
                                   || (this->IsPrePass() && blockSucc->backwardPassCurrentLoop == this->currentPrePassLoop)))
                {
                    JitAdelete(upwardExposedArena, blockSucc->upwardExposedFields);
                    blockSucc->upwardExposedFields = nullptr;
                }
            }

            if (blockSucc->typesNeedingKnownObjectLayout != nullptr)
            {
                typesNeedingKnownObjectLayout->Or(blockSucc->typesNeedingKnownObjectLayout);
                if (deleteData)
                {
                    JitAdelete(this->tempAlloc, blockSucc->typesNeedingKnownObjectLayout);
                    blockSucc->typesNeedingKnownObjectLayout = nullptr;
                }
            }

            if (fieldHoistCandidates && blockSucc->fieldHoistCandidates != nullptr)
            {
                fieldHoistCandidates->Or(blockSucc->fieldHoistCandidates);
                if (deleteData)
                {
                    JitAdelete(this->tempAlloc, blockSucc->fieldHoistCandidates);
                    blockSucc->fieldHoistCandidates = nullptr;
                }
            }
            if (blockSucc->slotDeadStoreCandidates != nullptr)
            {
                slotDeadStoreCandidates->And(blockSucc->slotDeadStoreCandidates);
                if (deleteData)
                {
                    JitAdelete(this->tempAlloc, blockSucc->slotDeadStoreCandidates);
                    blockSucc->slotDeadStoreCandidates = nullptr;
                }
            }
            if (blockSucc->tempNumberTracker != nullptr)
            {
                Assert((blockSucc->loop != nullptr) == blockSucc->tempNumberTracker->HasTempTransferDependencies());
                tempNumberTracker->MergeData(blockSucc->tempNumberTracker, deleteData);
                if (deleteData)
                {
                    blockSucc->tempNumberTracker = nullptr;
                }
            }
            if (blockSucc->tempObjectTracker != nullptr)
            {
                Assert((blockSucc->loop != nullptr) == blockSucc->tempObjectTracker->HasTempTransferDependencies());
                tempObjectTracker->MergeData(blockSucc->tempObjectTracker, deleteData);
                if (deleteData)
                {
                    blockSucc->tempObjectTracker = nullptr;
                }
            }
#if DBG
            if (blockSucc->tempObjectVerifyTracker != nullptr)
            {
                Assert((blockSucc->loop != nullptr) == blockSucc->tempObjectVerifyTracker->HasTempTransferDependencies());
                tempObjectVerifyTracker->MergeData(blockSucc->tempObjectVerifyTracker, deleteData);
                if (deleteData)
                {
                    blockSucc->tempObjectVerifyTracker = nullptr;
                }
            }
#endif

            PHASE_PRINT_TRACE(Js::ObjTypeSpecStorePhase, this->func,
                              L"ObjTypeSpecStore: func %s, edge %d => %d: ",
                              this->func->GetJnFunction()->GetDebugNumberSet(debugStringBuffer),
                              block->GetBlockNum(), blockSucc->GetBlockNum());

            auto fixupFrom = [block, blockSucc, this](Bucket<AddPropertyCacheBucket> &bucket)
            {
                AddPropertyCacheBucket *fromData = &bucket.element;
                if (fromData->GetInitialType() == nullptr ||
                    fromData->GetFinalType() == fromData->GetInitialType())
                {
                    return;
                }

                this->InsertTypeTransitionsAtPriorSuccessors(block, blockSucc, bucket.value, fromData);
            };

            auto fixupTo = [blockSucc, this](Bucket<AddPropertyCacheBucket> &bucket)
            {
                AddPropertyCacheBucket *toData = &bucket.element;
                if (toData->GetInitialType() == nullptr ||
                    toData->GetFinalType() == toData->GetInitialType())
                {
                    return;
                }

                this->InsertTypeTransitionAtBlock(blockSucc, bucket.value, toData);
            };

            if (blockSucc->stackSymToFinalType != nullptr)
            {
#if DBG_DUMP
                if (PHASE_TRACE(Js::ObjTypeSpecStorePhase, this->func))
                {
                    blockSucc->stackSymToFinalType->Dump();
                }
#endif
                if (firstSucc)
                {
                    stackSymToFinalType = blockSucc->stackSymToFinalType->Copy();
                }
                else if (stackSymToFinalType != nullptr)
                {
                    if (this->IsPrePass())
                    {
                        stackSymToFinalType->And(blockSucc->stackSymToFinalType);
                    }
                    else
                    {
                        // Insert any type transitions that can't be merged past this point.
                        stackSymToFinalType->AndWithFixup(blockSucc->stackSymToFinalType, fixupFrom, fixupTo);
                    }
                }
                else if (!this->IsPrePass())
                {
                    FOREACH_HASHTABLE_ENTRY(AddPropertyCacheBucket, bucket, blockSucc->stackSymToFinalType)
                    {
                        fixupTo(bucket);
                    }
                    NEXT_HASHTABLE_ENTRY;
                }

                if (deleteData)
                {
                    blockSucc->stackSymToFinalType->Delete();
                    blockSucc->stackSymToFinalType = nullptr;
                }
            }
            else
            {
                PHASE_PRINT_TRACE(Js::ObjTypeSpecStorePhase, this->func, L"null\n");
                if (stackSymToFinalType)
                {
                    if (!this->IsPrePass())
                    {
                        FOREACH_HASHTABLE_ENTRY(AddPropertyCacheBucket, bucket, stackSymToFinalType)
                        {
                            fixupFrom(bucket);
                        }
                        NEXT_HASHTABLE_ENTRY;
                    }

                    stackSymToFinalType->Delete();
                    stackSymToFinalType = nullptr;
                }
            }
            if (tag == Js::BackwardPhase)
            {
                if (blockSucc->cloneStrCandidates != nullptr)
                {
                    Assert(cloneStrCandidates != nullptr);
                    cloneStrCandidates->Or(blockSucc->cloneStrCandidates);
                    if (deleteData)
                    {
                        JitAdelete(this->globOpt->alloc, blockSucc->cloneStrCandidates);
                        blockSucc->cloneStrCandidates = nullptr;
                    }
                }
#if DBG_DUMP
                if (PHASE_VERBOSE_TRACE(Js::TraceObjTypeSpecWriteGuardsPhase, this->func))
                {
                    wchar_t debugStringBuffer2[MAX_FUNCTION_BODY_DEBUG_STRING_SIZE];
                    Js::FunctionBody* topFunctionBody = this->func->GetTopFunc()->GetJnFunction();
                    Js::FunctionBody* functionBody = this->func->GetJnFunction();
                    Output::Print(L"ObjTypeSpec: top function %s (%s), function %s (%s), write guard symbols on edge %d => %d: ",
                        topFunctionBody->GetDisplayName(), topFunctionBody->GetDebugNumberSet(debugStringBuffer), functionBody->GetDisplayName(),
                        functionBody->GetDebugNumberSet(debugStringBuffer2), block->GetBlockNum(), blockSucc->GetBlockNum());
                }
#endif
                if (blockSucc->stackSymToWriteGuardsMap != nullptr)
                {
#if DBG_DUMP
                    if (PHASE_VERBOSE_TRACE(Js::TraceObjTypeSpecWriteGuardsPhase, this->func))
                    {
                        Output::Print(L"\n");
                        blockSucc->stackSymToWriteGuardsMap->Dump();
                    }
#endif
                    if (stackSymToWriteGuardsMap == nullptr)
                    {
                        stackSymToWriteGuardsMap = blockSucc->stackSymToWriteGuardsMap->Copy();
                    }
                    else
                    {
                        stackSymToWriteGuardsMap->Or(
                            blockSucc->stackSymToWriteGuardsMap, &BackwardPass::MergeWriteGuards);
                    }

                    if (deleteData)
                    {
                        blockSucc->stackSymToWriteGuardsMap->Delete();
                        blockSucc->stackSymToWriteGuardsMap = nullptr;
                    }
                }
                else
                {
#if DBG_DUMP
                    if (PHASE_VERBOSE_TRACE(Js::TraceObjTypeSpecWriteGuardsPhase, this->func))
                    {
                        Output::Print(L"null\n");
                    }
#endif
                }
            }
            else
            {
#if DBG_DUMP
                if (PHASE_VERBOSE_TRACE(Js::TraceObjTypeSpecTypeGuardsPhase, this->func))
                {
                    Js::FunctionBody* topFunctionBody = this->func->GetTopFunc()->GetJnFunction();
                    Js::FunctionBody* functionBody = this->func->GetJnFunction();
                    wchar_t debugStringBuffer2[MAX_FUNCTION_BODY_DEBUG_STRING_SIZE];
                    Output::Print(L"ObjTypeSpec: top function %s (%s), function %s (%s), guarded property operations on edge %d => %d: \n",
                        topFunctionBody->GetDisplayName(), topFunctionBody->GetDebugNumberSet(debugStringBuffer), functionBody->GetDisplayName(), functionBody->GetDebugNumberSet(debugStringBuffer2),
                        block->GetBlockNum(), blockSucc->GetBlockNum());
                }
#endif
                if (blockSucc->stackSymToGuardedProperties != nullptr)
                {
#if DBG_DUMP
                    if (PHASE_VERBOSE_TRACE(Js::TraceObjTypeSpecTypeGuardsPhase, this->func))
                    {
                        blockSucc->stackSymToGuardedProperties->Dump();
                        Output::Print(L"\n");
                    }
#endif
                    if (stackSymToGuardedProperties == nullptr)
                    {
                        stackSymToGuardedProperties = blockSucc->stackSymToGuardedProperties->Copy();
                    }
                    else
                    {
                        stackSymToGuardedProperties->Or(
                            blockSucc->stackSymToGuardedProperties, &BackwardPass::MergeGuardedProperties);
                    }

                    if (deleteData)
                    {
                        blockSucc->stackSymToGuardedProperties->Delete();
                        blockSucc->stackSymToGuardedProperties = nullptr;
                    }
                }
                else
                {
#if DBG_DUMP
                    if (PHASE_VERBOSE_TRACE(Js::TraceObjTypeSpecTypeGuardsPhase, this->func))
                    {
                        Output::Print(L"null\n");
                    }
#endif
                }
            }

            if (blockSucc->noImplicitCallUses != nullptr)
            {
                noImplicitCallUses->Or(blockSucc->noImplicitCallUses);
                if (deleteData)
                {
                    JitAdelete(this->tempAlloc, blockSucc->noImplicitCallUses);
                    blockSucc->noImplicitCallUses = nullptr;
                }
            }
            if (blockSucc->noImplicitCallNoMissingValuesUses != nullptr)
            {
                noImplicitCallNoMissingValuesUses->Or(blockSucc->noImplicitCallNoMissingValuesUses);
                if (deleteData)
                {
                    JitAdelete(this->tempAlloc, blockSucc->noImplicitCallNoMissingValuesUses);
                    blockSucc->noImplicitCallNoMissingValuesUses = nullptr;
                }
            }
            if (blockSucc->noImplicitCallNativeArrayUses != nullptr)
            {
                noImplicitCallNativeArrayUses->Or(blockSucc->noImplicitCallNativeArrayUses);
                if (deleteData)
                {
                    JitAdelete(this->tempAlloc, blockSucc->noImplicitCallNativeArrayUses);
                    blockSucc->noImplicitCallNativeArrayUses = nullptr;
                }
            }
            if (blockSucc->noImplicitCallJsArrayHeadSegmentSymUses != nullptr)
            {
                noImplicitCallJsArrayHeadSegmentSymUses->Or(blockSucc->noImplicitCallJsArrayHeadSegmentSymUses);
                if (deleteData)
                {
                    JitAdelete(this->tempAlloc, blockSucc->noImplicitCallJsArrayHeadSegmentSymUses);
                    blockSucc->noImplicitCallJsArrayHeadSegmentSymUses = nullptr;
                }
            }
            if (blockSucc->noImplicitCallArrayLengthSymUses != nullptr)
            {
                noImplicitCallArrayLengthSymUses->Or(blockSucc->noImplicitCallArrayLengthSymUses);
                if (deleteData)
                {
                    JitAdelete(this->tempAlloc, blockSucc->noImplicitCallArrayLengthSymUses);
                    blockSucc->noImplicitCallArrayLengthSymUses = nullptr;
                }
            }

            firstSucc = false;
        }
        NEXT_SUCCESSOR_BLOCK;

#if DBG_DUMP
        wchar_t debugStringBuffer[MAX_FUNCTION_BODY_DEBUG_STRING_SIZE];
        if (PHASE_TRACE(Js::ObjTypeSpecStorePhase, this->func))
        {
            Output::Print(L"ObjTypeSpecStore: func %s, block %d: ",
                          this->func->GetJnFunction()->GetDebugNumberSet(debugStringBuffer),
                          block->GetBlockNum());
            if (stackSymToFinalType)
            {
                stackSymToFinalType->Dump();
            }
            else
            {
                Output::Print(L"null\n");
            }
        }

        if (PHASE_TRACE(Js::TraceObjTypeSpecTypeGuardsPhase, this->func))
        {
            Output::Print(L"ObjTypeSpec: func %s, block %d, guarded properties:\n",
                this->func->GetJnFunction()->GetDebugNumberSet(debugStringBuffer), block->GetBlockNum());
            if (stackSymToGuardedProperties)
            {
                stackSymToGuardedProperties->Dump();
                Output::Print(L"\n");
            }
            else
            {
                Output::Print(L"null\n");
            }
        }

        if (PHASE_TRACE(Js::TraceObjTypeSpecWriteGuardsPhase, this->func))
        {
            Output::Print(L"ObjTypeSpec: func %s, block %d, write guards: ",
                this->func->GetJnFunction()->GetDebugNumberSet(debugStringBuffer), block->GetBlockNum());
            if (stackSymToWriteGuardsMap)
            {
                Output::Print(L"\n");
                stackSymToWriteGuardsMap->Dump();
                Output::Print(L"\n");
            }
            else
            {
                Output::Print(L"null\n");
            }
        }
#endif
    }

#if DBG
    if (tempObjectVerifyTracker)
    {
        FOREACH_DEAD_SUCCESSOR_BLOCK(deadBlockSucc, block)
        {
            Assert(deadBlockSucc->tempObjectVerifyTracker || deadBlockSucc->isLoopHeader);
            if (deadBlockSucc->tempObjectVerifyTracker != nullptr)
            {
                Assert((deadBlockSucc->loop != nullptr) == deadBlockSucc->tempObjectVerifyTracker->HasTempTransferDependencies());
                // Dead block don't effect non temp use,  we only need to carry the removed use bit vector forward
                // and put all the upward exposed use to the set that we might found out to be mark temp
                // after globopt
                tempObjectVerifyTracker->MergeDeadData(deadBlockSucc);
            }

            if (!byteCodeUpwardExposedUsed)
            {
                if (!deadBlockSucc->isLoopHeader && deadBlockSucc->backwardPassCurrentLoop == this->currentPrePassLoop)
                {
                    Assert(deadBlockSucc->GetDataUseCount() != 0);
                    if (deadBlockSucc->DecrementDataUseCount() == 0)
                    {
                        this->DeleteBlockData(deadBlockSucc);
                    }
                }
            }
        }
        NEXT_DEAD_SUCCESSOR_BLOCK;
    }
#endif

    if (byteCodeUpwardExposedUsed)
    {
        FOREACH_DEAD_SUCCESSOR_BLOCK(deadBlockSucc, block)
        {
            Assert(deadBlockSucc->byteCodeUpwardExposedUsed || deadBlockSucc->isLoopHeader);
            if (deadBlockSucc->byteCodeUpwardExposedUsed)
            {
                byteCodeUpwardExposedUsed->Or(deadBlockSucc->byteCodeUpwardExposedUsed);
                if (this->tag == Js::DeadStorePhase)
                {
#if DBG
                    for (uint i = 0; i < byteCodeLocalsCount; i++)
                    {
                        if (byteCodeRestoreSyms[i] == nullptr)
                        {
                            byteCodeRestoreSyms[i] = deadBlockSucc->byteCodeRestoreSyms[i];
                        }
                        else
                        {
                            Assert(deadBlockSucc->byteCodeRestoreSyms[i] == nullptr
                                || byteCodeRestoreSyms[i] == deadBlockSucc->byteCodeRestoreSyms[i]);
                        }
                    }
#endif
                }
            }

            if (!deadBlockSucc->isLoopHeader && deadBlockSucc->backwardPassCurrentLoop == this->currentPrePassLoop)
            {
                Assert(deadBlockSucc->GetDataUseCount() != 0);
                if (deadBlockSucc->DecrementDataUseCount() == 0)
                {
                    this->DeleteBlockData(deadBlockSucc);
                }
            }
        }
        NEXT_DEAD_SUCCESSOR_BLOCK;
    }

    if (block->isLoopHeader)
    {
        this->DeleteBlockData(block);
    }
    else
    {
        if(block->GetDataUseCount() == 0)
        {
            Assert(block->slotDeadStoreCandidates == nullptr);
            Assert(block->tempNumberTracker == nullptr);
            Assert(block->tempObjectTracker == nullptr);
            Assert(block->tempObjectVerifyTracker == nullptr);
            Assert(block->upwardExposedUses == nullptr);
            Assert(block->upwardExposedFields == nullptr);
            Assert(block->typesNeedingKnownObjectLayout == nullptr);
            Assert(block->fieldHoistCandidates == nullptr);
            // byteCodeUpwardExposedUsed is required to populate the writeThroughSymbolsSet for the try region in the backwards pass
            Assert(block->byteCodeUpwardExposedUsed == nullptr || (this->tag == Js::BackwardPhase && this->func->HasTry() && this->func->DoOptimizeTryCatch()));
            Assert(block->byteCodeRestoreSyms == nullptr);
            Assert(block->stackSymToFinalType == nullptr);
            Assert(block->stackSymToGuardedProperties == nullptr);
            Assert(block->stackSymToWriteGuardsMap == nullptr);
            Assert(block->cloneStrCandidates == nullptr);
            Assert(block->noImplicitCallUses == nullptr);
            Assert(block->noImplicitCallNoMissingValuesUses == nullptr);
            Assert(block->noImplicitCallNativeArrayUses == nullptr);
            Assert(block->noImplicitCallJsArrayHeadSegmentSymUses == nullptr);
            Assert(block->noImplicitCallArrayLengthSymUses == nullptr);
        }
        else
        {
            // The collection pass sometimes does not know whether it can delete a successor block's data, so it may leave some
            // blocks with data intact. Delete the block data now.
            Assert(block->backwardPassCurrentLoop);
            Assert(block->backwardPassCurrentLoop->hasDeadStoreCollectionPass);
            Assert(!block->backwardPassCurrentLoop->hasDeadStorePrepass);

            DeleteBlockData(block);
        }

        block->backwardPassCurrentLoop = this->currentPrePassLoop;

        if (this->DoByteCodeUpwardExposedUsed()
#if DBG
            || this->DoMarkTempObjectVerify()
#endif
            )
        {
            block->SetDataUseCount(block->GetPredList()->Count() + block->GetDeadPredList()->Count());
        }
        else
        {
            block->SetDataUseCount(block->GetPredList()->Count());
        }
    }
    block->upwardExposedUses = upwardExposedUses;
    block->upwardExposedFields = upwardExposedFields;
    block->typesNeedingKnownObjectLayout = typesNeedingKnownObjectLayout;
    block->fieldHoistCandidates = fieldHoistCandidates;
    block->byteCodeUpwardExposedUsed = byteCodeUpwardExposedUsed;
#if DBG
    block->byteCodeRestoreSyms = byteCodeRestoreSyms;
#endif
    block->slotDeadStoreCandidates = slotDeadStoreCandidates;

    block->tempNumberTracker = tempNumberTracker;
    block->tempObjectTracker = tempObjectTracker;
#if DBG
    block->tempObjectVerifyTracker = tempObjectVerifyTracker;
#endif
    block->stackSymToFinalType = stackSymToFinalType;
    block->stackSymToGuardedProperties = stackSymToGuardedProperties;
    block->stackSymToWriteGuardsMap = stackSymToWriteGuardsMap;
    block->cloneStrCandidates = cloneStrCandidates;
    block->noImplicitCallUses = noImplicitCallUses;
    block->noImplicitCallNoMissingValuesUses = noImplicitCallNoMissingValuesUses;
    block->noImplicitCallNativeArrayUses = noImplicitCallNativeArrayUses;
    block->noImplicitCallJsArrayHeadSegmentSymUses = noImplicitCallJsArrayHeadSegmentSymUses;
    block->noImplicitCallArrayLengthSymUses = noImplicitCallArrayLengthSymUses;
}

ObjTypeGuardBucket
BackwardPass::MergeGuardedProperties(ObjTypeGuardBucket bucket1, ObjTypeGuardBucket bucket2)
{
    BVSparse<JitArenaAllocator> *guardedPropertyOps1 = bucket1.GetGuardedPropertyOps();
    BVSparse<JitArenaAllocator> *guardedPropertyOps2 = bucket2.GetGuardedPropertyOps();
    Assert(guardedPropertyOps1 || guardedPropertyOps2);

    BVSparse<JitArenaAllocator> *mergedPropertyOps;
    if (guardedPropertyOps1)
    {
        mergedPropertyOps = guardedPropertyOps1->CopyNew();
        if (guardedPropertyOps2)
        {
            mergedPropertyOps->Or(guardedPropertyOps2);
        }
    }
    else
    {
        mergedPropertyOps = guardedPropertyOps2->CopyNew();
    }

    ObjTypeGuardBucket bucket;
    bucket.SetGuardedPropertyOps(mergedPropertyOps);
    if (bucket1.NeedsMonoCheck() || bucket2.NeedsMonoCheck())
    {
        bucket.SetNeedsMonoCheck(true);
    }
    return bucket;
}

ObjWriteGuardBucket
BackwardPass::MergeWriteGuards(ObjWriteGuardBucket bucket1, ObjWriteGuardBucket bucket2)
{
    BVSparse<JitArenaAllocator> *writeGuards1 = bucket1.GetWriteGuards();
    BVSparse<JitArenaAllocator> *writeGuards2 = bucket2.GetWriteGuards();
    Assert(writeGuards1 || writeGuards2);

    BVSparse<JitArenaAllocator> *mergedWriteGuards;
    if (writeGuards1)
    {
        mergedWriteGuards = writeGuards1->CopyNew();
        if (writeGuards2)
        {
            mergedWriteGuards->Or(writeGuards2);
        }
    }
    else
    {
        mergedWriteGuards = writeGuards2->CopyNew();
    }

    ObjWriteGuardBucket bucket;
    bucket.SetWriteGuards(mergedWriteGuards);
    return bucket;
}

void
BackwardPass::DeleteBlockData(BasicBlock * block)
{
    if (block->slotDeadStoreCandidates != nullptr)
    {
        JitAdelete(this->tempAlloc, block->slotDeadStoreCandidates);
        block->slotDeadStoreCandidates = nullptr;
    }
    if (block->tempNumberTracker != nullptr)
    {
        JitAdelete(this->tempAlloc, block->tempNumberTracker);
        block->tempNumberTracker = nullptr;
    }
    if (block->tempObjectTracker != nullptr)
    {
        JitAdelete(this->tempAlloc, block->tempObjectTracker);
        block->tempObjectTracker = nullptr;
    }
#if DBG
    if (block->tempObjectVerifyTracker != nullptr)
    {
        JitAdelete(this->tempAlloc, block->tempObjectVerifyTracker);
        block->tempObjectVerifyTracker = nullptr;
    }
#endif
    if (block->stackSymToFinalType != nullptr)
    {
        block->stackSymToFinalType->Delete();
        block->stackSymToFinalType = nullptr;
    }
    if (block->stackSymToGuardedProperties != nullptr)
    {
        block->stackSymToGuardedProperties->Delete();
        block->stackSymToGuardedProperties = nullptr;
    }
    if (block->stackSymToWriteGuardsMap != nullptr)
    {
        block->stackSymToWriteGuardsMap->Delete();
        block->stackSymToWriteGuardsMap = nullptr;
    }
    if (block->cloneStrCandidates != nullptr)
    {
        Assert(this->tag == Js::BackwardPhase);
        JitAdelete(this->globOpt->alloc, block->cloneStrCandidates);
        block->cloneStrCandidates = nullptr;
    }
    if (block->noImplicitCallUses != nullptr)
    {
        JitAdelete(this->tempAlloc, block->noImplicitCallUses);
        block->noImplicitCallUses = nullptr;
    }
    if (block->noImplicitCallNoMissingValuesUses != nullptr)
    {
        JitAdelete(this->tempAlloc, block->noImplicitCallNoMissingValuesUses);
        block->noImplicitCallNoMissingValuesUses = nullptr;
    }
    if (block->noImplicitCallNativeArrayUses != nullptr)
    {
        JitAdelete(this->tempAlloc, block->noImplicitCallNativeArrayUses);
        block->noImplicitCallNativeArrayUses = nullptr;
    }
    if (block->noImplicitCallJsArrayHeadSegmentSymUses != nullptr)
    {
        JitAdelete(this->tempAlloc, block->noImplicitCallJsArrayHeadSegmentSymUses);
        block->noImplicitCallJsArrayHeadSegmentSymUses = nullptr;
    }
    if (block->noImplicitCallArrayLengthSymUses != nullptr)
    {
        JitAdelete(this->tempAlloc, block->noImplicitCallArrayLengthSymUses);
        block->noImplicitCallArrayLengthSymUses = nullptr;
    }
    if (block->upwardExposedUses != nullptr)
    {
        JitArenaAllocator *upwardExposedArena = (this->tag == Js::BackwardPhase) ? this->globOpt->alloc : this->tempAlloc;
        JitAdelete(upwardExposedArena, block->upwardExposedUses);
        block->upwardExposedUses = nullptr;
    }
    if (block->upwardExposedFields != nullptr)
    {
        JitArenaAllocator *upwardExposedArena = (this->tag == Js::BackwardPhase) ? this->globOpt->alloc : this->tempAlloc;
        JitAdelete(upwardExposedArena, block->upwardExposedFields);
        block->upwardExposedFields = nullptr;
    }
    if (block->typesNeedingKnownObjectLayout != nullptr)
    {
        JitAdelete(this->tempAlloc, block->typesNeedingKnownObjectLayout);
        block->typesNeedingKnownObjectLayout = nullptr;
    }
    if (block->fieldHoistCandidates != nullptr)
    {
        JitAdelete(this->tempAlloc, block->fieldHoistCandidates);
        block->fieldHoistCandidates = nullptr;
    }
    if (block->byteCodeUpwardExposedUsed != nullptr)
    {
        JitAdelete(this->tempAlloc, block->byteCodeUpwardExposedUsed);
        block->byteCodeUpwardExposedUsed = nullptr;
#if DBG
        JitAdeleteArray(this->tempAlloc, func->GetJnFunction()->GetLocalsCount(), block->byteCodeRestoreSyms);
        block->byteCodeRestoreSyms = nullptr;
#endif
    }
}

void
BackwardPass::ProcessLoopCollectionPass(BasicBlock *const lastBlock)
{
    // The collection pass is done before the prepass, to collect and propagate a minimal amount of information into nested
    // loops, for cases where the information is needed to make appropriate decisions on changing other state. For instance,
    // bailouts in nested loops need to be able to see all byte-code uses that are exposed to the bailout so that the
    // appropriate syms can be made upwards-exposed during the prepass. Byte-code uses that occur before the bailout in the
    // flow, or byte-code uses after the current loop, are not seen by bailouts inside the loop. The collection pass collects
    // byte-code uses and propagates them at least into each loop's header such that when bailouts are processed in the prepass,
    // they will have full visibility of byte-code upwards-exposed uses.
    //
    // For the collection pass, one pass is needed to collect all byte-code uses of a loop to the loop header. If the loop has
    // inner loops, another pass is needed to propagate byte-code uses in the outer loop into the inner loop's header, since
    // some byte-code uses may occur before the inner loop in the flow. The process continues recursively for inner loops. The
    // second pass only needs to walk as far as the first inner loop's header, since the purpose of that pass is only to
    // propagate collected information into the inner loops' headers.
    //
    // Consider the following case:
    //   (Block 1, Loop 1 header)
    //     ByteCodeUses s1
    //       (Block 2, Loop 2 header)
    //           (Block 3, Loop 3 header)
    //           (Block 4)
    //             BailOut
    //           (Block 5, Loop 3 back-edge)
    //       (Block 6, Loop 2 back-edge)
    //   (Block 7, Loop 1 back-edge)
    //
    // Assume that the exit branch in each of these loops is in the loop's header block, like a 'while' loop. For the byte-code
    // use of 's1' to become visible to the bailout in the innermost loop, we need to walk the following blocks:
    // - Collection pass
    //     - 7, 6, 5, 4, 3, 2, 1, 7 - block 1 is the first block in loop 1 that sees 's1', and since block 7 has block 1 as its
    //       successor, block 7 sees 's1' now as well
    //     - 6, 5, 4, 3, 2, 6 -  block 2 is the first block in loop 2 that sees 's1', and since block 6 has block 2 as its
    //       successor, block 6 sees 's1' now as well
    //     - 5, 4, 3 - block 3 is the first block in loop 3 that sees 's1'
    //     - The collection pass does not have to do another pass through the innermost loop because it does not have any inner
    //       loops of its own. It's sufficient to propagate the byte-code uses up to the loop header of each loop, as the
    //       prepass will do the remaining propagation.
    // - Prepass
    //     - 7, 6, 5, 4, ... - since block 5 has block 3 as its successor, block 5 sees 's1', and so does block 4. So, the bailout
    //       finally sees 's1' as a byte-code upwards-exposed use.
    //
    // The collection pass walks as described above, and consists of one pass, followed by another pass if there are inner
    // loops. The second pass only walks up to the first inner loop's header block, and during this pass upon reaching an inner
    // loop, the algorithm goes recursively for that inner loop, and once it returns, the second pass continues from above that
    // inner loop. Each bullet of the walk in the example above is a recursive call to ProcessLoopCollectionPass, except the
    // first line, which is the initial call.
    //
    // Imagine the whole example above is inside another loop, and at the bottom of that loop there is an assignment to 's1'. If
    // the bailout is the only use of 's1', then it needs to register 's1' as a use in the prepass to prevent treating the
    // assignment to 's1' as a dead store.

    Assert(tag == Js::DeadStorePhase);
    Assert(IsCollectionPass());
    Assert(lastBlock);

    Loop *const collectionPassLoop = lastBlock->loop;
    Assert(collectionPassLoop);
    Assert(!collectionPassLoop->hasDeadStoreCollectionPass);
    collectionPassLoop->hasDeadStoreCollectionPass = true;

    Loop *const previousPrepassLoop = currentPrePassLoop;
    currentPrePassLoop = collectionPassLoop;
    Assert(IsPrePass());

    // First pass
    BasicBlock *firstInnerLoopHeader = nullptr;
    {
#if DBG_DUMP
        if(IsTraceEnabled())
        {
            Output::Print(L"******* COLLECTION PASS 1 START: Loop %u ********\n", collectionPassLoop->GetLoopTopInstr()->m_id);
        }
#endif

        FOREACH_BLOCK_BACKWARD_IN_RANGE_DEAD_OR_ALIVE(block, lastBlock, nullptr)
        {
            ProcessBlock(block);

            if(block->isLoopHeader)
            {
                if(block->loop == collectionPassLoop)
                {
                    break;
                }

                // Keep track of the first inner loop's header for the second pass, which need only walk up to that block
                firstInnerLoopHeader = block;
            }
        } NEXT_BLOCK_BACKWARD_IN_RANGE_DEAD_OR_ALIVE;

#if DBG_DUMP
        if(IsTraceEnabled())
        {
            Output::Print(L"******** COLLECTION PASS 1 END: Loop %u *********\n", collectionPassLoop->GetLoopTopInstr()->m_id);
        }
#endif
    }

    // Second pass, only needs to run if there are any inner loops, to propagate collected information into those loops
    if(firstInnerLoopHeader)
    {
#if DBG_DUMP
        if(IsTraceEnabled())
        {
            Output::Print(L"******* COLLECTION PASS 2 START: Loop %u ********\n", collectionPassLoop->GetLoopTopInstr()->m_id);
        }
#endif

        FOREACH_BLOCK_BACKWARD_IN_RANGE_DEAD_OR_ALIVE(block, lastBlock, firstInnerLoopHeader)
        {
            Loop *const loop = block->loop;
            if(loop && loop != collectionPassLoop && !loop->hasDeadStoreCollectionPass)
            {
                // About to make a recursive call, so when jitting in the foreground, probe the stack
                if(!func->IsBackgroundJIT())
                {
                    PROBE_STACK(func->GetScriptContext(), Js::Constants::MinStackDefault);
                }
                ProcessLoopCollectionPass(block);

                // The inner loop's collection pass would have propagated collected information to its header block. Skip to the
                // inner loop's header block and continue from the block before it.
                block = loop->GetHeadBlock();
                Assert(block->isLoopHeader);
                continue;
            }

            ProcessBlock(block);
        } NEXT_BLOCK_BACKWARD_IN_RANGE_DEAD_OR_ALIVE;

#if DBG_DUMP
        if(IsTraceEnabled())
        {
            Output::Print(L"******** COLLECTION PASS 2 END: Loop %u *********\n", collectionPassLoop->GetLoopTopInstr()->m_id);
        }
#endif
    }

    currentPrePassLoop = previousPrepassLoop;
}

void
BackwardPass::ProcessLoop(BasicBlock * lastBlock)
{
#if DBG_DUMP
    if (this->IsTraceEnabled())
    {
        Output::Print(L"******* PREPASS START ********\n");
    }
#endif

    Loop *loop = lastBlock->loop;

    // This code doesn't work quite as intended. It is meant to capture fields that are live out of a loop to limit the
    // number of implicit call bailouts the forward pass must create (only compiler throughput optimization, no impact
    // on emitted code), but because it looks only at the lexically last block in the loop, it does the right thing only
    // for do-while loops. For other loops (for and while) the last block does not exit the loop. Even for do-while loops
    // this tracking can have the adverse effect of killing fields that should stay live after copy prop. Disabled by default.
    // Left in under a flag, in case we find compiler throughput issues and want to do additional experiments.
    if (PHASE_ON(Js::LiveOutFieldsPhase, this->func))
    {
        if (this->globOpt->DoFieldOpts(loop) || this->globOpt->DoFieldRefOpts(loop))
        {
            // Get the live-out set at the loop bottom.
            // This may not be the only loop exit, but all loop exits either leave the function or pass through here.
            // In the forward pass, we'll use this set to trim the live fields on exit from the loop
            // in order to limit the number of bailout points following the loop.
            BVSparse<JitArenaAllocator> *bv = JitAnew(this->func->m_fg->alloc, BVSparse<JitArenaAllocator>, this->func->m_fg->alloc);
            FOREACH_SUCCESSOR_BLOCK(blockSucc, lastBlock)
            {
                if (blockSucc->loop != loop)
                {
                    // Would like to assert this, but in strange exprgen cases involving "break LABEL" in nested
                    // loops the loop graph seems to get confused.
                    //Assert(!blockSucc->loop || blockSucc->loop->IsDescendentOrSelf(loop));
                    Assert(!blockSucc->loop || blockSucc->loop->hasDeadStorePrepass);

                    bv->Or(blockSucc->upwardExposedFields);
                }
            }
            NEXT_SUCCESSOR_BLOCK;
            lastBlock->loop->liveOutFields = bv;
        }
    }

    if(tag == Js::DeadStorePhase && !loop->hasDeadStoreCollectionPass)
    {
        Assert(!IsCollectionPass());
        Assert(!IsPrePass());
        isCollectionPass = true;
        ProcessLoopCollectionPass(lastBlock);
        isCollectionPass = false;
    }

    Assert(!this->IsPrePass());
    this->currentPrePassLoop = loop;

    FOREACH_BLOCK_BACKWARD_IN_RANGE_DEAD_OR_ALIVE(block, lastBlock, nullptr)
    {
        this->ProcessBlock(block);

        if (block->isLoopHeader && block->loop == lastBlock->loop)
        {
            Assert(block->fieldHoistCandidates == nullptr);
            break;
        }
    }
    NEXT_BLOCK_BACKWARD_IN_RANGE_DEAD_OR_ALIVE;

    this->currentPrePassLoop = nullptr;
    Assert(lastBlock);
    __analysis_assume(lastBlock);
    lastBlock->loop->hasDeadStorePrepass = true;

#if DBG_DUMP
    if (this->IsTraceEnabled())
    {
        Output::Print(L"******** PREPASS END *********\n");
    }
#endif
}

void
BackwardPass::OptBlock(BasicBlock * block)
{
    this->func->ThrowIfScriptClosed();

    if (block->loop && !block->loop->hasDeadStorePrepass)
    {
        ProcessLoop(block);
    }

    this->ProcessBlock(block);

    if(DoTrackNegativeZero())
    {
        negativeZeroDoesNotMatterBySymId->ClearAll();
    }
    if (DoTrackBitOpsOrNumber())
    {
        symUsedOnlyForBitOpsBySymId->ClearAll();
        symUsedOnlyForNumberBySymId->ClearAll();
    }

    if(DoTrackIntOverflow())
    {
        intOverflowDoesNotMatterBySymId->ClearAll();
        if(DoTrackCompoundedIntOverflow())
        {
            intOverflowDoesNotMatterInRangeBySymId->ClearAll();
        }
    }
}

void
BackwardPass::ProcessBailOutArgObj(BailOutInfo * bailOutInfo, BVSparse<JitArenaAllocator> * byteCodeUpwardExposedUsed)
{
    Assert(this->tag != Js::BackwardPhase);

    if (this->globOpt->TrackArgumentsObject() && bailOutInfo->capturedValues.argObjSyms)
    {
        FOREACH_BITSET_IN_SPARSEBV(symId, bailOutInfo->capturedValues.argObjSyms)
        {
            if (byteCodeUpwardExposedUsed->TestAndClear(symId))
            {
                if (bailOutInfo->usedCapturedValues.argObjSyms == nullptr)
                {
                    bailOutInfo->usedCapturedValues.argObjSyms = JitAnew(this->func->m_alloc,
                        BVSparse<JitArenaAllocator>, this->func->m_alloc);
                }
                bailOutInfo->usedCapturedValues.argObjSyms->Set(symId);
            }
        }
        NEXT_BITSET_IN_SPARSEBV;
    }
    if (bailOutInfo->usedCapturedValues.argObjSyms)
    {
        byteCodeUpwardExposedUsed->Minus(bailOutInfo->usedCapturedValues.argObjSyms);
    }
}

void
BackwardPass::ProcessBailOutConstants(BailOutInfo * bailOutInfo, BVSparse<JitArenaAllocator> * byteCodeUpwardExposedUsed, BVSparse<JitArenaAllocator>* bailoutReferencedArgSymsBv)
{
    Assert(this->tag != Js::BackwardPhase);

    // Remove constants that we are already going to restore
    SListBase<ConstantStackSymValue> * usedConstantValues = &bailOutInfo->usedCapturedValues.constantValues;
    FOREACH_SLISTBASE_ENTRY(ConstantStackSymValue, value, usedConstantValues)
    {
        byteCodeUpwardExposedUsed->Clear(value.Key()->m_id);
        bailoutReferencedArgSymsBv->Clear(value.Key()->m_id);
    }
    NEXT_SLISTBASE_ENTRY;

    // Find other constants that we need to restore
    FOREACH_SLISTBASE_ENTRY_EDITING(ConstantStackSymValue, value, &bailOutInfo->capturedValues.constantValues, iter)
    {
        if (byteCodeUpwardExposedUsed->TestAndClear(value.Key()->m_id) || bailoutReferencedArgSymsBv->TestAndClear(value.Key()->m_id))
        {
            // Constant need to be restore, move it to the restore list
            iter.MoveCurrentTo(usedConstantValues);
        }
        else if (!this->IsPrePass())
        {
            // Constants don't need to be restored, delete
            iter.RemoveCurrent(this->func->m_alloc);
        }
    }
    NEXT_SLISTBASE_ENTRY_EDITING;
}

void
BackwardPass::ProcessBailOutCopyProps(BailOutInfo * bailOutInfo, BVSparse<JitArenaAllocator> * byteCodeUpwardExposedUsed, BVSparse<JitArenaAllocator>* bailoutReferencedArgSymsBv)
{
    Assert(this->tag != Js::BackwardPhase);
    Assert(!this->func->GetJnFunction()->GetIsAsmjsMode());

    // Remove copy prop that we were already going to restore
    SListBase<CopyPropSyms> * usedCopyPropSyms = &bailOutInfo->usedCapturedValues.copyPropSyms;
    FOREACH_SLISTBASE_ENTRY(CopyPropSyms, copyPropSyms, usedCopyPropSyms)
    {
        byteCodeUpwardExposedUsed->Clear(copyPropSyms.Key()->m_id);
        this->currentBlock->upwardExposedUses->Set(copyPropSyms.Value()->m_id);
    }
    NEXT_SLISTBASE_ENTRY;

    JitArenaAllocator * allocator = this->func->m_alloc;
    BasicBlock * block = this->currentBlock;
    BVSparse<JitArenaAllocator> * upwardExposedUses = block->upwardExposedUses;

    // Find other copy prop that we need to restore
    FOREACH_SLISTBASE_ENTRY_EDITING(CopyPropSyms, copyPropSyms, &bailOutInfo->capturedValues.copyPropSyms, iter)
    {
        // Copy prop syms should be vars
        Assert(!copyPropSyms.Key()->IsTypeSpec());
        Assert(!copyPropSyms.Value()->IsTypeSpec());
        if (byteCodeUpwardExposedUsed->TestAndClear(copyPropSyms.Key()->m_id) || bailoutReferencedArgSymsBv->TestAndClear(copyPropSyms.Key()->m_id))
        {
            // This copy-prop sym needs to be restored; add it to the restore list.

            /*
            - copyPropSyms.Key() - original sym that is byte-code upwards-exposed, its corresponding byte-code register needs
              to be restored
            - copyPropSyms.Value() - copy-prop sym whose value the original sym has at the point of this instruction

            Heuristic:
            - By default, use the copy-prop sym to restore its corresponding byte code register
            - This is typically better because that allows the value of the original sym, if it's not used after the copy-prop
              sym is changed, to be discarded and we only have one lifetime (the copy-prop sym's lifetime) in to deal with for
              register allocation
            - Additionally, if the transferring store, which caused the original sym to have the same value as the copy-prop
              sym, becomes a dead store, the original sym won't actually attain the value of the copy-prop sym. In that case,
              the copy-prop sym must be used to restore the byte code register corresponding to original sym.

            Special case for functional correctness:
            - Consider that we always use the copy-prop sym to restore, and consider the following case:
                b = a
                a = c * d <Pre-op bail-out>
                  = b
            - This is rewritten by the lowerer as follows:
                b = a
                a = c
                a = a * d <Pre-op bail-out> (to make dst and src1 the same)
                  = b
            - The problem here is that at the point of the bail-out instruction, 'a' would be used to restore the value of 'b',
              but the value of 'a' has changed before the bail-out (at 'a = c').
            - In this case, we need to use 'b' (the original sym) to restore the value of 'b'. Because 'b' is upwards-exposed,
              'b = a' cannot be a dead store, therefore making it valid to use 'b' to restore.
            - Use the original sym to restore when all of the following are true:
                - The bailout is a pre-op bailout, and the bailout check is done after overwriting the destination
                - It's an int-specialized unary or binary operation that produces a value
                - The copy-prop sym is the destination of this instruction
                - None of the sources are the copy-prop sym. Otherwise, the value of the copy-prop sym will be saved as
                  necessary by the bailout code.
            */
            StackSym * stackSym = copyPropSyms.Key(); // assume that we'll use the original sym to restore
            SymID symId = stackSym->m_id;
            IR::Instr *const instr = bailOutInfo->bailOutInstr;
            StackSym *const dstSym = IR::RegOpnd::TryGetStackSym(instr->GetDst());
            if(instr->GetBailOutKind() & IR::BailOutOnResultConditions &&
                instr->GetByteCodeOffset() != Js::Constants::NoByteCodeOffset &&
                bailOutInfo->bailOutOffset <= instr->GetByteCodeOffset() &&
                dstSym &&
                dstSym->IsInt32() &&
                dstSym->IsTypeSpec() &&
                dstSym->GetVarEquivSym(nullptr) == copyPropSyms.Value() &&
                instr->GetSrc1() &&
                !instr->GetDst()->IsEqual(instr->GetSrc1()) &&
                !(instr->GetSrc2() && instr->GetDst()->IsEqual(instr->GetSrc2())))
            {
                Assert(bailOutInfo->bailOutOffset == instr->GetByteCodeOffset());

                // Need to use the original sym to restore. The original sym is byte-code upwards-exposed, which is why it needs
                // to be restored. Because the original sym needs to be restored and the copy-prop sym is changing here, the
                // original sym must be live in some fashion at the point of this instruction, that will be verified below. The
                // original sym will also be made upwards-exposed from here, so the aforementioned transferring store of the
                // copy-prop sym to the original sym will not be a dead store.
            }
            else if (block->upwardExposedUses->Test(stackSym->m_id) && !block->upwardExposedUses->Test(copyPropSyms.Value()->m_id))
            {
                // Don't use the copy prop sym if it is not used and the orig sym still has uses.
                // No point in extending the lifetime of the copy prop sym unnecessarily.
            }
            else
            {
                // Need to use the copy-prop sym to restore
                stackSym = copyPropSyms.Value();
                symId = stackSym->m_id;
            }

            // Prefer to restore from type-specialized versions of the sym, as that will reduce the need for potentially
            // expensive ToVars that can more easily be eliminated due to being dead stores
            StackSym * int32StackSym = nullptr;
            StackSym * float64StackSym = nullptr;
            StackSym * simd128StackSym = nullptr;
            if (bailOutInfo->liveLosslessInt32Syms->Test(symId))
            {
                // Var version of the sym is not live, use the int32 version
                int32StackSym = stackSym->GetInt32EquivSym(nullptr);
                Assert(int32StackSym);
            }
            else if(bailOutInfo->liveFloat64Syms->Test(symId))
            {
                // Var/int32 version of the sym is not live, use the float64 version
                float64StackSym = stackSym->GetFloat64EquivSym(nullptr);
                Assert(float64StackSym);
            }
            // SIMD_JS
            else if (bailOutInfo->liveSimd128F4Syms->Test(symId))
            {
                simd128StackSym = stackSym->GetSimd128F4EquivSym(nullptr);
            }
            else if (bailOutInfo->liveSimd128I4Syms->Test(symId))
            {
                simd128StackSym = stackSym->GetSimd128I4EquivSym(nullptr);
            }
            else
            {
                Assert(bailOutInfo->liveVarSyms->Test(symId));
            }

            // We did not end up using the copy prop sym. Let's make sure the use of the original sym by the bailout is captured.
            if (stackSym != copyPropSyms.Value() && stackSym->HasArgSlotNum())
            {
                bailoutReferencedArgSymsBv->Set(stackSym->m_id);
            }

            if (int32StackSym != nullptr)
            {
                Assert(float64StackSym == nullptr);
                usedCopyPropSyms->PrependNode(allocator, copyPropSyms.Key(), int32StackSym);
                iter.RemoveCurrent(allocator);
                upwardExposedUses->Set(int32StackSym->m_id);
            }
            else if (float64StackSym != nullptr)
            {
                // This float-specialized sym is going to be used to restore the corresponding byte-code register. Need to
                // ensure that the float value can be precisely coerced back to the original Var value by requiring that it is
                // specialized using BailOutNumberOnly.
                float64StackSym->m_requiresBailOnNotNumber = true;

                usedCopyPropSyms->PrependNode(allocator, copyPropSyms.Key(), float64StackSym);
                iter.RemoveCurrent(allocator);
                upwardExposedUses->Set(float64StackSym->m_id);
            }
            // SIMD_JS
            else if (simd128StackSym != nullptr)
            {
                usedCopyPropSyms->PrependNode(allocator, copyPropSyms.Key(), simd128StackSym);
                iter.RemoveCurrent(allocator);
                upwardExposedUses->Set(simd128StackSym->m_id);
            }
            else
            {
                usedCopyPropSyms->PrependNode(allocator, copyPropSyms.Key(), stackSym);
                iter.RemoveCurrent(allocator);
                upwardExposedUses->Set(symId);
            }
        }
        else if (!this->IsPrePass())
        {
            // Copy prop sym doesn't need to be restored, delete.
            iter.RemoveCurrent(allocator);
        }
    }
    NEXT_SLISTBASE_ENTRY_EDITING;
}

bool
BackwardPass::ProcessBailOutInfo(IR::Instr * instr)
{
    if (this->tag == Js::BackwardPhase)
    {
        // We don't need to fill in the bailout instruction in backward pass
        Assert(this->func->hasBailout || !instr->HasBailOutInfo());
        Assert(!instr->HasBailOutInfo() || instr->GetBailOutInfo()->byteCodeUpwardExposedUsed == nullptr || (this->func->HasTry() && this->func->DoOptimizeTryCatch()));

        if (instr->IsByteCodeUsesInstr())
        {
            // FGPeeps inserts bytecodeuses instrs with srcs.  We need to look at them to set the proper
            // UpwardExposedUsed info and keep the defs alive.
            // The inliner inserts bytecodeuses instrs withs dsts, but we don't want to look at them for upwardExposedUsed
            // as it would cause real defs to look dead.  We use these for bytecodeUpwardExposedUsed info only, which is needed
            // in the dead-store pass only.
            //
            // Handle the source side.
            IR::ByteCodeUsesInstr *byteCodeUsesInstr = instr->AsByteCodeUsesInstr();
            BVSparse<JitArenaAllocator> * byteCodeUpwardExposedUsed = byteCodeUsesInstr->byteCodeUpwardExposedUsed;
            if (byteCodeUpwardExposedUsed != nullptr)
            {
                this->currentBlock->upwardExposedUses->Or(byteCodeUpwardExposedUsed);
            }
            return true;
        }
        return false;
    }

    if (instr->IsByteCodeUsesInstr())
    {
        Assert(instr->m_opcode == Js::OpCode::ByteCodeUses);
#if DBG
        if (this->DoMarkTempObjectVerify() && (this->currentBlock->isDead || !this->func->hasBailout))
        {
            if (IsCollectionPass())
            {
                if (!this->func->hasBailout)
                {
                    // Prevent byte code uses from being remove on collection pass for mark temp object verify
                    // if we don't have any bailout
                    return true;
                }
            }
            else
            {
                this->currentBlock->tempObjectVerifyTracker->NotifyDeadByteCodeUses(instr);
            }
        }
#endif

        if (this->func->hasBailout)
        {
            Assert(this->DoByteCodeUpwardExposedUsed());

            // Just collect the byte code uses, and remove the instruction
            // We are going backward, process the dst first and then the src
            IR::Opnd * dst = instr->GetDst();
            if (dst)
            {
                IR::RegOpnd * dstRegOpnd = dst->AsRegOpnd();
                StackSym * dstStackSym = dstRegOpnd->m_sym->AsStackSym();
                Assert(!dstRegOpnd->GetIsJITOptimizedReg());
                Assert(dstStackSym->GetByteCodeRegSlot() != Js::Constants::NoRegister);
                if (dstStackSym->GetType() != TyVar)
                {
                    dstStackSym = dstStackSym->GetVarEquivSym(nullptr);
                }

                // If the current region is a Try, symbols in its write-through set shouldn't be cleared.
                // Otherwise, symbols in the write-through set of the first try ancestor shouldn't be cleared.
                if (!this->currentRegion ||
                    !this->CheckWriteThroughSymInRegion(this->currentRegion, dstStackSym))
                {
                    this->currentBlock->byteCodeUpwardExposedUsed->Clear(dstStackSym->m_id);
#if DBG
                    // We can only track first level function stack syms right now
                    if (dstStackSym->GetByteCodeFunc() == this->func)
                    {
                        this->currentBlock->byteCodeRestoreSyms[dstStackSym->GetByteCodeRegSlot()] = nullptr;
                    }
#endif
                }
            }

            IR::ByteCodeUsesInstr *byteCodeUsesInstr = instr->AsByteCodeUsesInstr();
            BVSparse<JitArenaAllocator> * byteCodeUpwardExposedUsed = byteCodeUsesInstr->byteCodeUpwardExposedUsed;
            if (byteCodeUpwardExposedUsed != nullptr)
            {
                this->currentBlock->byteCodeUpwardExposedUsed->Or(byteCodeUpwardExposedUsed);
#if DBG
                FOREACH_BITSET_IN_SPARSEBV(symId, byteCodeUpwardExposedUsed)
                {
                    StackSym * stackSym = this->func->m_symTable->FindStackSym(symId);
                    Assert(!stackSym->IsTypeSpec());
                    // We can only track first level function stack syms right now
                    if (stackSym->GetByteCodeFunc() == this->func)
                    {
                        Js::RegSlot byteCodeRegSlot = stackSym->GetByteCodeRegSlot();
                        Assert(byteCodeRegSlot != Js::Constants::NoRegister);
                        if (this->currentBlock->byteCodeRestoreSyms[byteCodeRegSlot] != stackSym)
                        {
                            AssertMsg(this->currentBlock->byteCodeRestoreSyms[byteCodeRegSlot] == nullptr,
                                "Can't have two active lifetime for the same byte code register");
                            this->currentBlock->byteCodeRestoreSyms[byteCodeRegSlot] = stackSym;
                        }
                    }
                }
                NEXT_BITSET_IN_SPARSEBV;
#endif
            }

            if(IsCollectionPass())
            {
                return true;
            }

            ProcessPendingPreOpBailOutInfo(instr);

            PropertySym *propertySymUse = byteCodeUsesInstr->propertySymUse;
            if (propertySymUse && !this->currentBlock->isDead)
            {
                this->currentBlock->upwardExposedFields->Set(propertySymUse->m_id);
            }

            if (this->IsPrePass())
            {
                // Don't remove the instruction yet if we are in the prepass
                // But tell the caller we don't need to process the instruction any more
                return true;
            }
        }

        this->currentBlock->RemoveInstr(instr);
        return true;
    }

    if(IsCollectionPass())
    {
        return false;
    }

    if (instr->HasBailOutInfo())
    {
        Assert(this->func->hasBailout);
        Assert(this->DoByteCodeUpwardExposedUsed());

        BailOutInfo * bailOutInfo = instr->GetBailOutInfo();

        // Only process the bailout info if this is the main bailout point (instead of shared)
        if (bailOutInfo->bailOutInstr == instr)
        {
            if(instr->GetByteCodeOffset() == Js::Constants::NoByteCodeOffset ||
                bailOutInfo->bailOutOffset > instr->GetByteCodeOffset())
            {
                // Currently, we only have post-op bailout with BailOutOnImplicitCalls
                // or JIT inserted operation (which no byte code offsets).
                // If there are other bailouts that we want to bailout after the operation,
                // we have to make sure that it still doesn't do the implicit call
                // if it is done on the stack object.
                // Otherwise, the stack object will be passed to the implicit call functions.
                Assert(instr->GetByteCodeOffset() == Js::Constants::NoByteCodeOffset
                    || (instr->GetBailOutKind() & ~IR::BailOutKindBits) == IR::BailOutOnImplicitCalls
                    || (instr->GetBailOutKind() & ~IR::BailOutKindBits) == IR::BailOutInvalid);

                // This instruction bails out to a later byte-code instruction, so process the bailout info now
                ProcessBailOutInfo(instr, bailOutInfo);
            }
            else
            {
                // This instruction bails out to the equivalent byte code instruction. This instruction and ByteCodeUses
                // instructions relevant to this instruction need to be processed before the bailout info for this instruction
                // can be processed, so that it can be determined what byte code registers are used by the equivalent byte code
                // instruction and need to be restored. Save the instruction for bailout info processing later.
                Assert(bailOutInfo->bailOutOffset == instr->GetByteCodeOffset());
                Assert(!preOpBailOutInstrToProcess);
                preOpBailOutInstrToProcess = instr;
            }
        }
    }

    return false;
}

bool
BackwardPass::IsImplicitCallBailOutCurrentlyNeeded(IR::Instr * instr, bool mayNeedImplicitCallBailOut, bool hasLiveFields)
{
    return this->globOpt->IsImplicitCallBailOutCurrentlyNeeded(
        instr, nullptr, nullptr, this->currentBlock, hasLiveFields, mayNeedImplicitCallBailOut, false);
}

void
BackwardPass::DeadStoreTypeCheckBailOut(IR::Instr * instr)
{
    // Good news: There are cases where the forward pass installs BailOutFailedTypeCheck, but the dead store pass
    // discovers that the checked type is dead.
    // Bad news: We may still need implicit call bailout, and it's up to the dead store pass to figure this out.
    // Worse news: BailOutFailedTypeCheck is pre-op, and BailOutOnImplicitCall is post-op. We'll use a special
    // bailout kind to indicate implicit call bailout that targets its own instruction. The lowerer will emit
    // code to disable/re-enable implicit calls around the operation.

    Assert(this->tag == Js::DeadStorePhase);

    if (this->IsPrePass() || !instr->HasBailOutInfo())
    {
        return;
    }

    IR::BailOutKind oldBailOutKind = instr->GetBailOutKind();
    if (!IR::IsTypeCheckBailOutKind(oldBailOutKind))
    {
        return;
    }

    // Either src1 or dst must be a property sym operand
    Assert((instr->GetSrc1() && instr->GetSrc1()->IsSymOpnd() && instr->GetSrc1()->AsSymOpnd()->IsPropertySymOpnd()) ||
        (instr->GetDst() && instr->GetDst()->IsSymOpnd() && instr->GetDst()->AsSymOpnd()->IsPropertySymOpnd()));

    IR::PropertySymOpnd *propertySymOpnd =
        (instr->GetDst() && instr->GetDst()->IsSymOpnd()) ? instr->GetDst()->AsPropertySymOpnd() : instr->GetSrc1()->AsPropertySymOpnd();

    bool isTypeCheckProtected = false;
    IR::BailOutKind bailOutKind;
    if (GlobOpt::NeedsTypeCheckBailOut(instr, propertySymOpnd, propertySymOpnd == instr->GetDst(), &isTypeCheckProtected, &bailOutKind))
    {
        // If we installed a failed type check bailout in the forward pass, but we are now discovering that the checked
        // type is dead, we may still need a bailout on failed fixed field type check. These type checks are required
        // regardless of whether the checked type is dead.  Hence, the bailout kind may change here.
        Assert((oldBailOutKind & ~IR::BailOutKindBits) == bailOutKind ||
            bailOutKind == IR::BailOutFailedFixedFieldTypeCheck || bailOutKind == IR::BailOutFailedEquivalentFixedFieldTypeCheck);
        instr->SetBailOutKind(bailOutKind);
        return;
    }
    else if (isTypeCheckProtected)
    {
        instr->ClearBailOutInfo();
        if (preOpBailOutInstrToProcess == instr)
        {
            preOpBailOutInstrToProcess = nullptr;
        }
        return;
    }

    Assert(!propertySymOpnd->IsTypeCheckProtected());

    // If all we're doing here is checking the type (e.g. because we've hoisted a field load or store out of the loop, but needed
    // the type check to remain in the loop), and now it turns out we don't need the type checked, we can simply turn this into
    // a NOP and remove the bailout.
    if (instr->m_opcode == Js::OpCode::CheckObjType)
    {
        Assert(instr->GetDst() == nullptr && instr->GetSrc1() != nullptr && instr->GetSrc2() == nullptr);
        instr->m_opcode = Js::OpCode::Nop;
        instr->FreeSrc1();
        instr->ClearBailOutInfo();
        if (this->preOpBailOutInstrToProcess == instr)
        {
            this->preOpBailOutInstrToProcess = nullptr;
        }
        return;
    }

    // We don't need BailOutFailedTypeCheck but may need BailOutOnImplicitCall.
    // Consider: are we in the loop landing pad? If so, no bailout, since implicit calls will be checked at
    // the end of the block.
    if (this->currentBlock->IsLandingPad())
    {
        // We're in the landing pad.
        if (preOpBailOutInstrToProcess == instr)
        {
            preOpBailOutInstrToProcess = nullptr;
        }
        instr->UnlinkBailOutInfo();
        return;
    }

    // We're not checking for polymorphism, so don't let the bailout indicate that we
    // detected polymorphism.
    instr->GetBailOutInfo()->polymorphicCacheIndex = (uint)-1;

    // Keep the mark temp object bit if it is there so that we will not remove the implicit call check
    instr->SetBailOutKind(IR::BailOutOnImplicitCallsPreOp | (oldBailOutKind & IR::BailOutMarkTempObject));
}

void
BackwardPass::DeadStoreImplicitCallBailOut(IR::Instr * instr, bool hasLiveFields)
{
    Assert(this->tag == Js::DeadStorePhase);

    if (this->IsPrePass() || !instr->HasBailOutInfo())
    {
        // Don't do this in the pre-pass, because, for instance, we don't have live-on-back-edge fields yet.
        return;
    }

    if (OpCodeAttr::BailOutRec(instr->m_opcode))
    {
        // This is something like OpCode::BailOutOnNotEqual. Assume it needs what it's got.
        return;
    }

    UpdateArrayBailOutKind(instr);

    // Install the implicit call PreOp for mark temp object if we need one.
    IR::BailOutKind kind = instr->GetBailOutKind();
    IR::BailOutKind kindNoBits = kind & ~IR::BailOutKindBits;
    if ((kind & IR::BailOutMarkTempObject) != 0 && kindNoBits != IR::BailOutOnImplicitCallsPreOp)
    {
        Assert(kindNoBits != IR::BailOutOnImplicitCalls);
        if (kindNoBits == IR::BailOutInvalid)
        {
            // We should only have combined with array bits
            Assert((kind & ~IR::BailOutForArrayBits) == IR::BailOutMarkTempObject);
            // Don't need to install if we are not going to do helper calls,
            // or we are in the landingPad since implicit calls are already turned off.
            if ((kind & IR::BailOutOnArrayAccessHelperCall) == 0 && !this->currentBlock->IsLandingPad())
            {
                kind += IR::BailOutOnImplicitCallsPreOp;
                instr->SetBailOutKind(kind);
            }
        }
    }

    // Currently only try to eliminate these bailout kinds. The others are required in cases
    // where we don't necessarily have live/hoisted fields.
    const bool mayNeedBailOnImplicitCall = BailOutInfo::IsBailOutOnImplicitCalls(kind);
    if (!mayNeedBailOnImplicitCall && kind != IR::BailOutExpectingObject)
    {
        if (kind & IR::BailOutMarkTempObject)
        {
            if (kind == IR::BailOutMarkTempObject)
            {
                // Landing pad does not need per-instr implicit call bailouts.
                Assert(this->currentBlock->IsLandingPad());
                instr->ClearBailOutInfo();
                if (this->preOpBailOutInstrToProcess == instr)
                {
                    this->preOpBailOutInstrToProcess = nullptr;
                }
            }
            else
            {
                // Mark temp object bit is not needed after dead store pass
                instr->SetBailOutKind(kind & ~IR::BailOutMarkTempObject);
            }
        }
        return;
    }

    // We have an implicit call bailout in the code, and we want to make sure that it's required.
    // Do this now, because only in the dead store pass do we have complete forward and backward liveness info.
    bool needsBailOutOnImplicitCall = this->IsImplicitCallBailOutCurrentlyNeeded(instr, mayNeedBailOnImplicitCall, hasLiveFields);

    if(!UpdateImplicitCallBailOutKind(instr, needsBailOutOnImplicitCall))
    {
        instr->ClearBailOutInfo();
        if (preOpBailOutInstrToProcess == instr)
        {
            preOpBailOutInstrToProcess = nullptr;
        }
#if DBG
        if (this->DoMarkTempObjectVerify())
        {
            this->currentBlock->tempObjectVerifyTracker->NotifyBailOutRemoval(instr, this);
        }
#endif
    }
}

void
BackwardPass::ProcessPendingPreOpBailOutInfo(IR::Instr *const currentInstr)
{
    Assert(!IsCollectionPass());

    if(!preOpBailOutInstrToProcess)
    {
        return;
    }

    IR::Instr *const prevInstr = currentInstr->m_prev;
    if(prevInstr &&
        prevInstr->IsByteCodeUsesInstr() &&
        prevInstr->AsByteCodeUsesInstr()->GetByteCodeOffset() == preOpBailOutInstrToProcess->GetByteCodeOffset())
    {
        return;
    }

    // A pre-op bailout instruction was saved for bailout info processing after the instruction and relevant ByteCodeUses
    // instructions before it have been processed. We can process the bailout info for that instruction now.
    BailOutInfo *const bailOutInfo = preOpBailOutInstrToProcess->GetBailOutInfo();
    Assert(bailOutInfo->bailOutInstr == preOpBailOutInstrToProcess);
    Assert(bailOutInfo->bailOutOffset == preOpBailOutInstrToProcess->GetByteCodeOffset());
    ProcessBailOutInfo(preOpBailOutInstrToProcess, bailOutInfo);
    preOpBailOutInstrToProcess = nullptr;
}

void
BackwardPass::ProcessBailOutInfo(IR::Instr * instr, BailOutInfo * bailOutInfo)
{
    /*
    When we optimize functions having try-catch, we install a bailout at the starting of the catch block, namely, BailOnException.
    We don't have flow edges from all the possible exception points in the try to the catch block. As a result, this bailout should
    not try to restore from the constant values or copy-prop syms or the type specialized syms, as these may not necessarily be/have
    the right values. For example,

        //constant values
        c =
        try
        {
            <exception>
            c = k (constant)
        }
        catch
        {
            BailOnException
            = c  <-- We need to restore c from the value outside the try.
        }

        //copy-prop syms
        c =
        try
        {
            b = a
            <exception>
            c = b
        }
        catch
        {
            BailOnException
            = c  <-- We really want to restore c from its original sym, and not from its copy-prop sym, a
        }

        //type specialized syms
        a =
        try
        {
            <exception>
            a++  <-- type specializes a
        }
        catch
        {
            BailOnException
            = a  <-- We need to restore a from its var version.
        }
    */
    BasicBlock * block = this->currentBlock;
    BVSparse<JitArenaAllocator> * byteCodeUpwardExposedUsed = block->byteCodeUpwardExposedUsed;

    Assert(bailOutInfo->bailOutInstr == instr);

    // The byteCodeUpwardExposedUsed should only be assigned once. The only case which would break this
    // assumption is when we are optimizing a function having try-catch. In that case, we need the
    // byteCodeUpwardExposedUsed analysis in the initial backward pass too.
    Assert(bailOutInfo->byteCodeUpwardExposedUsed == nullptr || (this->func->HasTry() && this->func->DoOptimizeTryCatch()));

    // Make a copy of the byteCodeUpwardExposedUsed so we can remove the constants
    if (!this->IsPrePass())
    {
        // Create the BV of symbols that need to be restored in the BailOutRecord
        byteCodeUpwardExposedUsed = byteCodeUpwardExposedUsed->CopyNew(this->func->m_alloc);
        bailOutInfo->byteCodeUpwardExposedUsed = byteCodeUpwardExposedUsed;
    }
    else
    {
        // Create a temporary byteCodeUpwardExposedUsed
        byteCodeUpwardExposedUsed = byteCodeUpwardExposedUsed->CopyNew(this->tempAlloc);
    }

    // All the register-based argument syms need to be tracked. They are either:
    //      1. Referenced as constants in bailOutInfo->usedcapturedValues.constantValues
    //      2. Referenced using copy prop syms in bailOutInfo->usedcapturedValues.copyPropSyms
    //      3. Marked as m_isBailOutReferenced = true & added to upwardExposedUsed bit vector to ensure we do not dead store their defs.
    //      The third set of syms is represented by the bailoutReferencedArgSymsBv.
    BVSparse<JitArenaAllocator>* bailoutReferencedArgSymsBv = JitAnew(this->tempAlloc, BVSparse<JitArenaAllocator>, this->tempAlloc);
    if (!this->IsPrePass())
    {
        bailOutInfo->IterateArgOutSyms([=](uint, uint, StackSym* sym) {
            if (!sym->IsArgSlotSym())
            {
                bailoutReferencedArgSymsBv->Set(sym->m_id);
            }
        });
    }

    // Process Argument object first, as they can be found on the stack and don't need to rely on copy prop
    this->ProcessBailOutArgObj(bailOutInfo, byteCodeUpwardExposedUsed);

    if (instr->m_opcode != Js::OpCode::BailOnException) // see comment at the beginning of this function
    {
        this->ProcessBailOutConstants(bailOutInfo, byteCodeUpwardExposedUsed, bailoutReferencedArgSymsBv);
        this->ProcessBailOutCopyProps(bailOutInfo, byteCodeUpwardExposedUsed, bailoutReferencedArgSymsBv);
    }

    BVSparse<JitArenaAllocator> * tempBv = JitAnew(this->tempAlloc, BVSparse<JitArenaAllocator>, this->tempAlloc);

    if (bailOutInfo->liveVarSyms)
    {
        // Prefer to restore from type-specialized versions of the sym, as that will reduce the need for potentially expensive
        // ToVars that can more easily be eliminated due to being dead stores.

#if DBG
        // SIMD_JS
        // Simd128 syms should be live in at most one form
        tempBv->And(bailOutInfo->liveSimd128F4Syms, bailOutInfo->liveSimd128I4Syms);
        Assert(tempBv->IsEmpty());

        // Verify that all syms to restore are live in some fashion
        tempBv->Minus(byteCodeUpwardExposedUsed, bailOutInfo->liveVarSyms);
        tempBv->Minus(bailOutInfo->liveLosslessInt32Syms);
        tempBv->Minus(bailOutInfo->liveFloat64Syms);
        tempBv->Minus(bailOutInfo->liveSimd128F4Syms);
        tempBv->Minus(bailOutInfo->liveSimd128I4Syms);
        Assert(tempBv->IsEmpty());
#endif

        if (this->func->IsJitInDebugMode())
        {
            // Add to byteCodeUpwardExposedUsed the non-temp local vars used so far to restore during bail out.
            // The ones that are not used so far will get their values from bytecode when we continue after bail out in interpreter.
            Assert(this->func->m_nonTempLocalVars);
            tempBv->And(this->func->m_nonTempLocalVars, bailOutInfo->liveVarSyms);

            // Remove syms that are restored in other ways than byteCodeUpwardExposedUsed.
            FOREACH_SLIST_ENTRY(ConstantStackSymValue, value, &bailOutInfo->usedCapturedValues.constantValues)
            {
                Assert(value.Key()->HasByteCodeRegSlot() || value.Key()->GetInstrDef()->m_opcode == Js::OpCode::BytecodeArgOutCapture);
                if (value.Key()->HasByteCodeRegSlot())
                {
                    tempBv->Clear(value.Key()->GetByteCodeRegSlot());
                }
            }
            NEXT_SLIST_ENTRY;
            FOREACH_SLIST_ENTRY(CopyPropSyms, value, &bailOutInfo->usedCapturedValues.copyPropSyms)
            {
                Assert(value.Key()->HasByteCodeRegSlot() || value.Key()->GetInstrDef()->m_opcode == Js::OpCode::BytecodeArgOutCapture);
                if (value.Key()->HasByteCodeRegSlot())
                {
                    tempBv->Clear(value.Key()->GetByteCodeRegSlot());
                }
            }
            NEXT_SLIST_ENTRY;
            if (bailOutInfo->usedCapturedValues.argObjSyms)
            {
                tempBv->Minus(bailOutInfo->usedCapturedValues.argObjSyms);
            }

            byteCodeUpwardExposedUsed->Or(tempBv);
        }

        if (instr->m_opcode != Js::OpCode::BailOnException) // see comment at the beginning of this function
        {
            // Int32
            tempBv->And(byteCodeUpwardExposedUsed, bailOutInfo->liveLosslessInt32Syms);
            byteCodeUpwardExposedUsed->Minus(tempBv);
            FOREACH_BITSET_IN_SPARSEBV(symId, tempBv)
            {
                StackSym * stackSym = this->func->m_symTable->FindStackSym(symId);
                Assert(stackSym->GetType() == TyVar);
                StackSym * int32StackSym = stackSym->GetInt32EquivSym(nullptr);
                Assert(int32StackSym);
                byteCodeUpwardExposedUsed->Set(int32StackSym->m_id);
            }
            NEXT_BITSET_IN_SPARSEBV;

            // Float64
            tempBv->And(byteCodeUpwardExposedUsed, bailOutInfo->liveFloat64Syms);
            byteCodeUpwardExposedUsed->Minus(tempBv);
            FOREACH_BITSET_IN_SPARSEBV(symId, tempBv)
            {
                StackSym * stackSym = this->func->m_symTable->FindStackSym(symId);
                Assert(stackSym->GetType() == TyVar);
                StackSym * float64StackSym = stackSym->GetFloat64EquivSym(nullptr);
                Assert(float64StackSym);
                byteCodeUpwardExposedUsed->Set(float64StackSym->m_id);

                // This float-specialized sym is going to be used to restore the corresponding byte-code register. Need to
                // ensure that the float value can be precisely coerced back to the original Var value by requiring that it is
                // specialized using BailOutNumberOnly.
                float64StackSym->m_requiresBailOnNotNumber = true;
            }
            NEXT_BITSET_IN_SPARSEBV;

            // SIMD_JS
            tempBv->Or(bailOutInfo->liveSimd128F4Syms, bailOutInfo->liveSimd128I4Syms);
            tempBv->And(byteCodeUpwardExposedUsed);
            byteCodeUpwardExposedUsed->Minus(tempBv);
            FOREACH_BITSET_IN_SPARSEBV(symId, tempBv)
            {
                StackSym * stackSym = this->func->m_symTable->FindStackSym(symId);
                Assert(stackSym->GetType() == TyVar);
                StackSym * simd128Sym = nullptr;
                if (bailOutInfo->liveSimd128F4Syms->Test(symId))
                {
                    simd128Sym = stackSym->GetSimd128F4EquivSym(nullptr);
                }
                else
                {
                    Assert(bailOutInfo->liveSimd128I4Syms->Test(symId));
                    simd128Sym = stackSym->GetSimd128I4EquivSym(nullptr);
                }
                byteCodeUpwardExposedUsed->Set(simd128Sym->m_id);
            }
            NEXT_BITSET_IN_SPARSEBV;
        }
        // Var
        // Any remaining syms to restore will be restored from their var versions
    }
    else
    {
        Assert(!this->func->DoGlobOpt());
    }

    JitAdelete(this->tempAlloc, tempBv);

    // BailOnNoProfile makes some edges dead. Upward exposed symbols info set after the BailOnProfile won't
    // flow through these edges, and, in turn, not through predecessor edges of the block containing the
    // BailOnNoProfile. This is specifically bad for an inlinee's argout syms as they are set as upward exposed
    // when we see the InlineeEnd, but may not look so to some blocks and may get overwritten.
    // Set the argout syms as upward exposed here.
    if (instr->m_opcode == Js::OpCode::BailOnNoProfile && instr->m_func->IsInlinee() &&
        instr->m_func->m_hasInlineArgsOpt && instr->m_func->frameInfo->isRecorded)
    {
        instr->m_func->frameInfo->IterateSyms([=](StackSym* argSym)
        {
            this->currentBlock->upwardExposedUses->Set(argSym->m_id);
        });
    }

    // Mark all the register that we need to restore as used (excluding constants)
    block->upwardExposedUses->Or(byteCodeUpwardExposedUsed);
    block->upwardExposedUses->Or(bailoutReferencedArgSymsBv);

    if (!this->IsPrePass())
    {
        bailOutInfo->IterateArgOutSyms([=](uint index, uint, StackSym* sym) {
            if (sym->IsArgSlotSym() || bailoutReferencedArgSymsBv->Test(sym->m_id))
            {
                bailOutInfo->argOutSyms[index]->m_isBailOutReferenced = true;
            }
        });
    }
    JitAdelete(this->tempAlloc, bailoutReferencedArgSymsBv);

    if (this->IsPrePass())
    {
        JitAdelete(this->tempAlloc, byteCodeUpwardExposedUsed);
    }
}

void
BackwardPass::ProcessBlock(BasicBlock * block)
{
    this->currentBlock = block;
    this->MergeSuccBlocksInfo(block);
#if DBG_DUMP
    if (this->IsTraceEnabled())
    {
        Output::Print(L"******************************* Before Process Block *******************************n");
        DumpBlockData(block);
    }
#endif
    FOREACH_INSTR_BACKWARD_IN_BLOCK_EDITING(instr, instrPrev, block)
    {
#if DBG_DUMP
        if (!IsCollectionPass() && IsTraceEnabled() && Js::Configuration::Global.flags.Verbose)
        {
            Output::Print(L">>>>>>>>>>>>>>>>>>>>>> %s: Instr Start\n", tag == Js::BackwardPhase? L"BACKWARD" : L"DEADSTORE");
            instr->Dump();
            Output::SkipToColumn(10);
            Output::Print(L"   Exposed Use: ");
            block->upwardExposedUses->Dump();
            Output::SkipToColumn(10);
            Output::Print(L"Exposed Fields: ");
            block->upwardExposedFields->Dump();
            if (block->byteCodeUpwardExposedUsed)
            {
                Output::SkipToColumn(10);
                Output::Print(L" Byte Code Use: ");
                block->byteCodeUpwardExposedUsed->Dump();
            }
            Output::Print(L"--------------------\n");
        }
#endif

        this->currentInstr = instr;
        this->currentRegion = this->currentBlock->GetFirstInstr()->AsLabelInstr()->GetRegion();

        if (ProcessNoImplicitCallUses(instr) || this->ProcessBailOutInfo(instr))
        {
            continue;
        }

        IR::Instr *instrNext = instr->m_next;
        if (this->TrackNoImplicitCallInlinees(instr))
        {
            instrPrev = instrNext->m_prev;
            continue;
        }

        bool hasLiveFields = (block->upwardExposedFields && !block->upwardExposedFields->IsEmpty());

        IR::Opnd * opnd = instr->GetDst();
        if (opnd != nullptr)
        {
            bool isRemoved = ReverseCopyProp(instr);
            if (isRemoved)
            {
                instrPrev = instrNext->m_prev;
                continue;
            }
            if (instr->m_opcode == Js::OpCode::Conv_Bool)
            {
                isRemoved = this->FoldCmBool(instr);
                if (isRemoved)
                {
                    continue;
                }
            }

            ProcessNewScObject(instr);

            this->ProcessTransfers(instr);

            isRemoved = this->ProcessDef(opnd);
            if (isRemoved)
            {
                continue;
            }
        }

        if(!IsCollectionPass())
        {
            this->MarkTempProcessInstr(instr);
            this->ProcessFieldKills(instr);

            if (this->DoDeadStoreSlots()
                && (instr->HasAnyImplicitCalls() || instr->HasBailOutInfo() || instr->UsesAllFields()))
            {
                // Can't dead-store slots if there can be an implicit-call, an exception, or a bailout
                block->slotDeadStoreCandidates->ClearAll();
            }

            if (this->DoFieldHoistCandidates())
            {
                this->ProcessFieldHoistKills(instr);
            }

            TrackIntUsage(instr);
            TrackBitWiseOrNumberOp(instr);

            TrackFloatSymEquivalence(instr);
        }

        opnd = instr->GetSrc1();
        if (opnd != nullptr)
        {
            this->ProcessUse(opnd);

            opnd = instr->GetSrc2();
            if (opnd != nullptr)
            {
                this->ProcessUse(opnd);
            }
        }

        if(IsCollectionPass())
        {
            continue;
        }

        if (this->tag == Js::DeadStorePhase)
        {
            switch(instr->m_opcode)
            {
                case Js::OpCode::InlineArrayPush:
                case Js::OpCode::InlineArrayPop:
                {
                    IR::Opnd *const thisOpnd = instr->GetSrc1();
                    if(thisOpnd && thisOpnd->IsRegOpnd())
                    {
                        IR::RegOpnd *const thisRegOpnd = thisOpnd->AsRegOpnd();
                        if(thisRegOpnd->IsArrayRegOpnd())
                        {
                            // Process the array use at the point of the array built-in call, since the array will actually
                            // be used at the call, not at the ArgOut_A_InlineBuiltIn
                            ProcessArrayRegOpndUse(instr, thisRegOpnd->AsArrayRegOpnd());
                        }
                    }
                }

            #if !INT32VAR // the following is not valid on 64-bit platforms
                case Js::OpCode::BoundCheck:
                {
                    if(IsPrePass())
                    {
                        break;
                    }

                    // Look for:
                    //     BoundCheck 0 <= s1
                    //     BoundCheck s1 <= s2 + c, where c == 0 || c == -1
                    //
                    // And change it to:
                    //     UnsignedBoundCheck s1 <= s2 + c
                    //
                    // The BoundCheck instruction is a signed operation, so any unsigned operand used in the instruction must be
                    // guaranteed to be >= 0 and <= int32 max when its value is interpreted as signed. Due to the restricted
                    // range of s2 above, by using an unsigned comparison instead, the negative check on s1 will also be
                    // covered.
                    //
                    // A BoundCheck instruction takes the form (src1 <= src2 + dst).

                    // Check the current instruction's pattern for:
                    //     BoundCheck s1 <= s2 + c, where c <= 0
                    if(!instr->GetSrc1()->IsRegOpnd() ||
                        !instr->GetSrc1()->IsInt32() ||
                        !instr->GetSrc2() ||
                        instr->GetSrc2()->IsIntConstOpnd())
                    {
                        break;
                    }
                    if(instr->GetDst())
                    {
                        const int c = instr->GetDst()->AsIntConstOpnd()->GetValue();
                        if(c != 0 && c != -1)
                        {
                            break;
                        }
                    }

                    // Check the previous instruction's pattern for:
                    //     BoundCheck 0 <= s1
                    IR::Instr *const lowerBoundCheck = instr->m_prev;
                    if(lowerBoundCheck->m_opcode != Js::OpCode::BoundCheck ||
                        !lowerBoundCheck->GetSrc1()->IsIntConstOpnd() ||
                        lowerBoundCheck->GetSrc1()->AsIntConstOpnd()->GetValue() != 0 ||
                        !lowerBoundCheck->GetSrc2() ||
                        !instr->GetSrc1()->AsRegOpnd()->IsEqual(lowerBoundCheck->GetSrc2()) ||
                        lowerBoundCheck->GetDst() && lowerBoundCheck->GetDst()->AsIntConstOpnd()->GetValue() != 0)
                    {
                        break;
                    }

                    // Remove the previous lower bound check, and change the current upper bound check to:
                    //     UnsignedBoundCheck s1 <= s2 + c
                    instr->m_opcode = Js::OpCode::UnsignedBoundCheck;
                    currentBlock->RemoveInstr(lowerBoundCheck);
                    instrPrev = instr->m_prev;
                    break;
                }
            #endif
            }

            DeadStoreTypeCheckBailOut(instr);
            DeadStoreImplicitCallBailOut(instr, hasLiveFields);

            if (block->stackSymToFinalType != nullptr)
            {
                this->InsertTypeTransitionsAtPotentialKills();
            }

            // NoImplicitCallUses transfers need to be processed after determining whether implicit calls need to be disabled
            // for the current instruction, because the instruction where the def occurs also needs implicit calls disabled.
            // Array value type for the destination needs to be updated before transfers have been processed by
            // ProcessNoImplicitCallDef, and array value types for sources need to be updated after transfers have been
            // processed by ProcessNoImplicitCallDef, as it requires the no-implicit-call tracking bit-vectors to be precise at
            // the point of the update.
            if(!IsPrePass())
            {
                UpdateArrayValueTypes(instr, instr->GetDst());
            }
            ProcessNoImplicitCallDef(instr);
            if(!IsPrePass())
            {
                UpdateArrayValueTypes(instr, instr->GetSrc1());
                UpdateArrayValueTypes(instr, instr->GetSrc2());
            }
        }
        else
        {
            switch (instr->m_opcode)
            {
                case Js::OpCode::BailOnNoProfile:
                {
                    this->ProcessBailOnNoProfile(instr, block);
                    // this call could change the last instr of the previous block...  Adjust instrStop.
                    instrStop = block->GetFirstInstr()->m_prev;
                    Assert(this->tag != Js::DeadStorePhase);
                    continue;
                }
                case Js::OpCode::Catch:
                {
                    if (this->func->DoOptimizeTryCatch() && !this->IsPrePass())
                    {
                        // Execute the "Catch" in the JIT'ed code, and bailout to the next instruction. This way, the bailout will restore the exception object automatically.
                        IR::BailOutInstr* bailOnException = IR::BailOutInstr::New(Js::OpCode::BailOnException, IR::BailOutOnException, instr->m_next, instr->m_func);
                        instr->InsertAfter(bailOnException);

                        Assert(instr->GetDst()->IsRegOpnd() && instr->GetDst()->GetStackSym()->HasByteCodeRegSlot());
                        StackSym * exceptionObjSym = instr->GetDst()->GetStackSym();

                        Assert(instr->m_prev->IsLabelInstr() && (instr->m_prev->AsLabelInstr()->GetRegion()->GetType() == RegionTypeCatch));
                        instr->m_prev->AsLabelInstr()->GetRegion()->SetExceptionObjectSym(exceptionObjSym);
                    }
                    break;
                }
            }
        }

        if (instr->m_opcode == Js::OpCode::InlineeEnd)
        {
            this->ProcessInlineeEnd(instr);
        }

        if (instr->IsLabelInstr() && instr->m_next->m_opcode == Js::OpCode::Catch)
        {
            if (!this->currentRegion)
            {
                Assert(!this->func->DoOptimizeTryCatch() && !(this->func->IsSimpleJit() && this->func->hasBailout));
            }
            else
            {
                Assert(this->currentRegion->GetType() == RegionTypeCatch);
                Region * matchingTryRegion = this->currentRegion->GetMatchingTryRegion();
                Assert(matchingTryRegion);

                // We need live-on-back-edge info to accurately set write-through symbols for try-catches in a loop.
                // Don't set write-through symbols in pre-pass
                if (!this->IsPrePass() && !matchingTryRegion->writeThroughSymbolsSet)
                {
                    if (this->tag == Js::DeadStorePhase)
                    {
                        Assert(!this->func->DoGlobOpt());
                    }
                    // FullJit: Write-through symbols info must be populated in the backward pass as
                    //      1. the forward pass needs it to insert ToVars.
                    //      2. the deadstore pass needs it to not clear such symbols from the
                    //         byteCodeUpwardExposedUsed BV upon a def in the try region. This is required
                    //         because any bailout in the try region needs to restore all write-through
                    //         symbols.
                    // SimpleJit: Won't run the initial backward pass, but write-through symbols info is still
                    //      needed in the deadstore pass for <2> above.
                    this->SetWriteThroughSymbolsSetForRegion(this->currentBlock, matchingTryRegion);
                }
            }
        }
#if DBG
        if (instr->m_opcode == Js::OpCode::TryCatch)
        {
            if (!this->IsPrePass() && (this->func->DoOptimizeTryCatch() || (this->func->IsSimpleJit() && this->func->hasBailout)))
            {
                Assert(instr->m_next->IsLabelInstr() && (instr->m_next->AsLabelInstr()->GetRegion() != nullptr));
                Region * tryRegion = instr->m_next->AsLabelInstr()->GetRegion();
                Assert(tryRegion->writeThroughSymbolsSet);
            }
        }
#endif
        ProcessPendingPreOpBailOutInfo(instr);

#if DBG_DUMP
        if (!IsCollectionPass() && IsTraceEnabled() && Js::Configuration::Global.flags.Verbose)
        {
            Output::Print(L"-------------------\n");
            instr->Dump();
            Output::SkipToColumn(10);
            Output::Print(L"   Exposed Use: ");
            block->upwardExposedUses->Dump();
            Output::SkipToColumn(10);
            Output::Print(L"Exposed Fields: ");
            block->upwardExposedFields->Dump();
            if (block->byteCodeUpwardExposedUsed)
            {
                Output::SkipToColumn(10);
                Output::Print(L" Byte Code Use: ");
                block->byteCodeUpwardExposedUsed->Dump();
            }
            Output::Print(L"<<<<<<<<<<<<<<<<<<<<<< %s: Instr End\n", tag == Js::BackwardPhase? L"BACKWARD" : L"DEADSTORE");
        }
#endif
    }
    NEXT_INSTR_BACKWARD_IN_BLOCK_EDITING;

    EndIntOverflowDoesNotMatterRange();

    if (this->DoFieldHoistCandidates() && !block->isDead && block->isLoopHeader)
    {
        Assert(block->loop->fieldHoistCandidates == nullptr);
        block->loop->fieldHoistCandidates = block->fieldHoistCandidates->CopyNew(this->func->m_alloc);
    }

    if (!this->IsPrePass() && !block->isDead && block->isLoopHeader)
    {
        // Copy the upward exposed use as the live on back edge regs
        block->loop->regAlloc.liveOnBackEdgeSyms = block->upwardExposedUses->CopyNew(this->func->m_alloc);
    }

    Assert(!considerSymAsRealUseInNoImplicitCallUses);

#if DBG_DUMP
    if (this->IsTraceEnabled())
    {
        Output::Print(L"******************************* After Process Block *******************************n");
        DumpBlockData(block);
    }
#endif
}

#if DBG_DUMP
void
BackwardPass::DumpBlockData(BasicBlock * block)
{
    block->DumpHeader();
    if (block->upwardExposedUses) // may be null for dead blocks
    {
        Output::Print(L"             Exposed Uses: ");
        block->upwardExposedUses->Dump();
    }

    if (block->typesNeedingKnownObjectLayout)
    {
        Output::Print(L"            Needs Known Object Layout: ");
        block->typesNeedingKnownObjectLayout->Dump();
    }

    if (this->DoFieldHoistCandidates() && !block->isDead)
    {
        Output::Print(L"            Exposed Field: ");
        block->fieldHoistCandidates->Dump();
    }

    if (block->byteCodeUpwardExposedUsed)
    {
        Output::Print(L"   Byte Code Exposed Uses: ");
        block->byteCodeUpwardExposedUsed->Dump();
    }

    if (!this->IsCollectionPass())
    {
        if (!block->isDead)
        {
            if (this->DoDeadStoreSlots())
            {
                Output::Print(L"Slot deadStore candidates: ");
                block->slotDeadStoreCandidates->Dump();
            }
            DumpMarkTemp();
        }
    }
    Output::Flush();
}
#endif

bool
BackwardPass::UpdateImplicitCallBailOutKind(IR::Instr *const instr, bool needsBailOutOnImplicitCall)
{
    Assert(instr);
    Assert(instr->HasBailOutInfo());

    IR::BailOutKind implicitCallBailOutKind = needsBailOutOnImplicitCall ? IR::BailOutOnImplicitCalls : IR::BailOutInvalid;

    const IR::BailOutKind instrBailOutKind = instr->GetBailOutKind();
    if (instrBailOutKind & IR::BailOutMarkTempObject)
    {
        // Don't remove the implicit call pre op bailout for mark temp object
        // Remove the mark temp object bit, as we don't need it after the dead store pass
        instr->SetBailOutKind(instrBailOutKind & ~IR::BailOutMarkTempObject);
        return true;
    }

    const IR::BailOutKind instrImplicitCallBailOutKind = instrBailOutKind & ~IR::BailOutKindBits;
    if(instrImplicitCallBailOutKind == IR::BailOutOnImplicitCallsPreOp)
    {
        if(needsBailOutOnImplicitCall)
        {
            implicitCallBailOutKind = IR::BailOutOnImplicitCallsPreOp;
        }
    }
    else if(instrImplicitCallBailOutKind != IR::BailOutOnImplicitCalls && instrImplicitCallBailOutKind != IR::BailOutInvalid)
    {
        // This bailout kind (the value of 'instrImplicitCallBailOutKind') must guarantee that implicit calls will not happen.
        // If it doesn't make such a guarantee, it must be possible to merge this bailout kind with an implicit call bailout
        // kind, and therefore should be part of BailOutKindBits.
        Assert(!needsBailOutOnImplicitCall);
        return true;
    }

    if(instrImplicitCallBailOutKind == implicitCallBailOutKind)
    {
        return true;
    }

    const IR::BailOutKind newBailOutKind = instrBailOutKind - instrImplicitCallBailOutKind + implicitCallBailOutKind;
    if(newBailOutKind == IR::BailOutInvalid)
    {
        return false;
    }

    instr->SetBailOutKind(newBailOutKind);
    return true;
}

bool
BackwardPass::ProcessNoImplicitCallUses(IR::Instr *const instr)
{
    Assert(instr);

    if(instr->m_opcode != Js::OpCode::NoImplicitCallUses)
    {
        return false;
    }
    Assert(tag == Js::DeadStorePhase);
    Assert(!instr->GetDst());
    Assert(instr->GetSrc1());
    Assert(instr->GetSrc1()->IsRegOpnd() || instr->GetSrc1()->IsSymOpnd());
    Assert(!instr->GetSrc2() || instr->GetSrc2()->IsRegOpnd() || instr->GetSrc2()->IsSymOpnd());

    if(IsCollectionPass())
    {
        return true;
    }

    IR::Opnd *const srcs[] = { instr->GetSrc1(), instr->GetSrc2() };
    for(int i = 0; i < sizeof(srcs) / sizeof(srcs[0]) && srcs[i]; ++i)
    {
        IR::Opnd *const src = srcs[i];
        IR::ArrayRegOpnd *arraySrc = nullptr;
        Sym *sym;
        switch(src->GetKind())
        {
            case IR::OpndKindReg:
            {
                IR::RegOpnd *const regSrc = src->AsRegOpnd();
                sym = regSrc->m_sym;
                if(considerSymAsRealUseInNoImplicitCallUses && considerSymAsRealUseInNoImplicitCallUses == sym)
                {
                    considerSymAsRealUseInNoImplicitCallUses = nullptr;
                    ProcessStackSymUse(sym->AsStackSym(), true);
                }
                if(regSrc->IsArrayRegOpnd())
                {
                    arraySrc = regSrc->AsArrayRegOpnd();
                }
                break;
            }

            case IR::OpndKindSym:
                sym = src->AsSymOpnd()->m_sym;
                Assert(sym->IsPropertySym());
                break;

            default:
                Assert(false);
                __assume(false);
        }

        currentBlock->noImplicitCallUses->Set(sym->m_id);
        const ValueType valueType(src->GetValueType());
        if(valueType.IsArrayOrObjectWithArray())
        {
            if(valueType.HasNoMissingValues())
            {
                currentBlock->noImplicitCallNoMissingValuesUses->Set(sym->m_id);
            }
            if(!valueType.HasVarElements())
            {
                currentBlock->noImplicitCallNativeArrayUses->Set(sym->m_id);
            }
            if(arraySrc)
            {
                ProcessArrayRegOpndUse(instr, arraySrc);
            }
        }
    }

    if(!IsPrePass())
    {
        currentBlock->RemoveInstr(instr);
    }
    return true;
}

void
BackwardPass::ProcessNoImplicitCallDef(IR::Instr *const instr)
{
    Assert(tag == Js::DeadStorePhase);
    Assert(instr);

    IR::Opnd *const dst = instr->GetDst();
    if(!dst)
    {
        return;
    }

    Sym *dstSym;
    switch(dst->GetKind())
    {
        case IR::OpndKindReg:
            dstSym = dst->AsRegOpnd()->m_sym;
            break;

        case IR::OpndKindSym:
            dstSym = dst->AsSymOpnd()->m_sym;
            if(!dstSym->IsPropertySym())
            {
                return;
            }
            break;

        default:
            return;
    }

    if(!currentBlock->noImplicitCallUses->TestAndClear(dstSym->m_id))
    {
        Assert(!currentBlock->noImplicitCallNoMissingValuesUses->Test(dstSym->m_id));
        Assert(!currentBlock->noImplicitCallNativeArrayUses->Test(dstSym->m_id));
        Assert(!currentBlock->noImplicitCallJsArrayHeadSegmentSymUses->Test(dstSym->m_id));
        Assert(!currentBlock->noImplicitCallArrayLengthSymUses->Test(dstSym->m_id));
        return;
    }
    const bool transferNoMissingValuesUse = !!currentBlock->noImplicitCallNoMissingValuesUses->TestAndClear(dstSym->m_id);
    const bool transferNativeArrayUse = !!currentBlock->noImplicitCallNativeArrayUses->TestAndClear(dstSym->m_id);
    const bool transferJsArrayHeadSegmentSymUse =
        !!currentBlock->noImplicitCallJsArrayHeadSegmentSymUses->TestAndClear(dstSym->m_id);
    const bool transferArrayLengthSymUse = !!currentBlock->noImplicitCallArrayLengthSymUses->TestAndClear(dstSym->m_id);

    IR::Opnd *const src = instr->GetSrc1();
    if(!src || instr->GetSrc2())
    {
        return;
    }
    if(dst->IsRegOpnd() && src->IsRegOpnd())
    {
        if(!OpCodeAttr::NonIntTransfer(instr->m_opcode))
        {
            return;
        }
    }
    else if(
        !(
            // LdFld or similar
            dst->IsRegOpnd() && src->IsSymOpnd() && src->AsSymOpnd()->m_sym->IsPropertySym() ||

            // StFld or similar. Don't transfer a field opnd from StFld into the reg opnd src unless the field's value type is
            // definitely array or object with array, because only those value types require implicit calls to be disabled as
            // long as they are live. Other definite value types only require implicit calls to be disabled as long as a live
            // field holds the value, which is up to the StFld when going backwards.
            src->IsRegOpnd() && dst->GetValueType().IsArrayOrObjectWithArray()
        ) ||
        !GlobOpt::TransferSrcValue(instr))
    {
        return;
    }

    Sym *srcSym;
    switch(src->GetKind())
    {
        case IR::OpndKindReg:
            srcSym = src->AsRegOpnd()->m_sym;
            break;

        case IR::OpndKindSym:
            srcSym = src->AsSymOpnd()->m_sym;
            Assert(srcSym->IsPropertySym());
            break;

        default:
            Assert(false);
            __assume(false);
    }

    currentBlock->noImplicitCallUses->Set(srcSym->m_id);
    if(transferNoMissingValuesUse)
    {
        currentBlock->noImplicitCallNoMissingValuesUses->Set(srcSym->m_id);
    }
    if(transferNativeArrayUse)
    {
        currentBlock->noImplicitCallNativeArrayUses->Set(srcSym->m_id);
    }
    if(transferJsArrayHeadSegmentSymUse)
    {
        currentBlock->noImplicitCallJsArrayHeadSegmentSymUses->Set(srcSym->m_id);
    }
    if(transferArrayLengthSymUse)
    {
        currentBlock->noImplicitCallArrayLengthSymUses->Set(srcSym->m_id);
    }
}

template<class F>
IR::Opnd *
BackwardPass::FindNoImplicitCallUse(
    IR::Instr *const instr,
    StackSym *const sym,
    const F IsCheckedUse,
    IR::Instr * *const noImplicitCallUsesInstrRef)
{
    IR::RegOpnd *const opnd = IR::RegOpnd::New(sym, sym->GetType(), instr->m_func);
    IR::Opnd *const use = FindNoImplicitCallUse(instr, opnd, IsCheckedUse, noImplicitCallUsesInstrRef);
    opnd->FreeInternal(instr->m_func);
    return use;
}

template<class F>
IR::Opnd *
BackwardPass::FindNoImplicitCallUse(
    IR::Instr *const instr,
    IR::Opnd *const opnd,
    const F IsCheckedUse,
    IR::Instr * *const noImplicitCallUsesInstrRef)
{
    Assert(instr);
    Assert(instr->m_opcode != Js::OpCode::NoImplicitCallUses);

    // Skip byte-code uses
    IR::Instr *prevInstr = instr->m_prev;
    while(
        prevInstr &&
        !prevInstr->IsLabelInstr() &&
        (!prevInstr->IsRealInstr() || prevInstr->IsByteCodeUsesInstr()) &&
        prevInstr->m_opcode != Js::OpCode::NoImplicitCallUses)
    {
        prevInstr = prevInstr->m_prev;
    }

    // Find the corresponding use in a NoImplicitCallUses instruction
    for(; prevInstr && prevInstr->m_opcode == Js::OpCode::NoImplicitCallUses; prevInstr = prevInstr->m_prev)
    {
        IR::Opnd *const checkedSrcs[] = { prevInstr->GetSrc1(), prevInstr->GetSrc2() };
        for(int i = 0; i < sizeof(checkedSrcs) / sizeof(checkedSrcs[0]) && checkedSrcs[i]; ++i)
        {
            IR::Opnd *const checkedSrc = checkedSrcs[i];
            if(checkedSrc->IsEqual(opnd) && IsCheckedUse(checkedSrc))
            {
                if(noImplicitCallUsesInstrRef)
                {
                    *noImplicitCallUsesInstrRef = prevInstr;
                }
                return checkedSrc;
            }
        }
    }

    if(noImplicitCallUsesInstrRef)
    {
        *noImplicitCallUsesInstrRef = nullptr;
    }
    return nullptr;
}

void
BackwardPass::ProcessArrayRegOpndUse(IR::Instr *const instr, IR::ArrayRegOpnd *const arrayRegOpnd)
{
    Assert(tag == Js::DeadStorePhase);
    Assert(!IsCollectionPass());
    Assert(instr);
    Assert(arrayRegOpnd);

    if(!(arrayRegOpnd->HeadSegmentSym() || arrayRegOpnd->HeadSegmentLengthSym() || arrayRegOpnd->LengthSym()))
    {
        return;
    }

    const ValueType arrayValueType(arrayRegOpnd->GetValueType());
    const bool isJsArray = !arrayValueType.IsLikelyTypedArray();
    Assert(isJsArray == arrayValueType.IsArrayOrObjectWithArray());
    Assert(!isJsArray == arrayValueType.IsOptimizedTypedArray());

    BasicBlock *const block = currentBlock;
    if(!IsPrePass() &&
        (arrayRegOpnd->HeadSegmentSym() || arrayRegOpnd->HeadSegmentLengthSym()) &&
        (!isJsArray || instr->m_opcode != Js::OpCode::NoImplicitCallUses))
    {
        bool headSegmentIsLoadedButUnused =
            instr->loadedArrayHeadSegment &&
            arrayRegOpnd->HeadSegmentSym() &&
            !block->upwardExposedUses->Test(arrayRegOpnd->HeadSegmentSym()->m_id);
        const bool headSegmentLengthIsLoadedButUnused =
            instr->loadedArrayHeadSegmentLength &&
            arrayRegOpnd->HeadSegmentLengthSym() &&
            !block->upwardExposedUses->Test(arrayRegOpnd->HeadSegmentLengthSym()->m_id);
        if(headSegmentLengthIsLoadedButUnused && instr->extractedUpperBoundCheckWithoutHoisting)
        {
            // Find the upper bound check (index[src1] <= headSegmentLength[src2] + offset[dst])
            IR::Instr *upperBoundCheck = this->globOpt->FindUpperBoundsCheckInstr(instr);
            Assert(upperBoundCheck && upperBoundCheck != instr);
            Assert(upperBoundCheck->GetSrc2()->AsRegOpnd()->m_sym == arrayRegOpnd->HeadSegmentLengthSym());

            // Find the head segment length load
            IR::Instr *headSegmentLengthLoad = this->globOpt->FindArraySegmentLoadInstr(upperBoundCheck);

            Assert(headSegmentLengthLoad->GetDst()->AsRegOpnd()->m_sym == arrayRegOpnd->HeadSegmentLengthSym());
            Assert(
                headSegmentLengthLoad->GetSrc1()->AsIndirOpnd()->GetBaseOpnd()->m_sym ==
                (isJsArray ? arrayRegOpnd->HeadSegmentSym() : arrayRegOpnd->m_sym));

            // Fold the head segment length load into the upper bound check. Keep the load instruction there with a Nop so that
            // the head segment length sym can be marked as unused before the Nop. The lowerer will remove it.
            upperBoundCheck->ReplaceSrc2(headSegmentLengthLoad->UnlinkSrc1());
            headSegmentLengthLoad->m_opcode = Js::OpCode::Nop;

            if(isJsArray)
            {
                // The head segment length is on the head segment, so the bound check now uses the head segment sym
                headSegmentIsLoadedButUnused = false;
            }
        }

        if(headSegmentIsLoadedButUnused || headSegmentLengthIsLoadedButUnused)
        {
            // Check if the head segment / head segment length are being loaded here. If so, remove them and let the fast
            // path load them since it does a better job.
            IR::ArrayRegOpnd *noImplicitCallArrayUse = nullptr;
            if(isJsArray)
            {
                IR::Opnd *const use =
                    FindNoImplicitCallUse(
                        instr,
                        arrayRegOpnd,
                        [&](IR::Opnd *const checkedSrc) -> bool
                        {
                            const ValueType checkedSrcValueType(checkedSrc->GetValueType());
                            if(!checkedSrcValueType.IsLikelyObject() ||
                                checkedSrcValueType.GetObjectType() != arrayValueType.GetObjectType())
                            {
                                return false;
                            }

                            IR::RegOpnd *const checkedRegSrc = checkedSrc->AsRegOpnd();
                            if(!checkedRegSrc->IsArrayRegOpnd())
                            {
                                return false;
                            }

                            IR::ArrayRegOpnd *const checkedArraySrc = checkedRegSrc->AsArrayRegOpnd();
                            if(headSegmentIsLoadedButUnused &&
                                checkedArraySrc->HeadSegmentSym() != arrayRegOpnd->HeadSegmentSym())
                            {
                                return false;
                            }
                            if(headSegmentLengthIsLoadedButUnused &&
                                checkedArraySrc->HeadSegmentLengthSym() != arrayRegOpnd->HeadSegmentLengthSym())
                            {
                                return false;
                            }
                            return true;
                        });
                if(use)
                {
                    noImplicitCallArrayUse = use->AsRegOpnd()->AsArrayRegOpnd();
                }
            }
            else if(headSegmentLengthIsLoadedButUnused)
            {
                // A typed array's head segment length may be zeroed when the typed array's buffer is transferred to a web
                // worker, so the head segment length sym use is included in a NoImplicitCallUses instruction. Since there
                // are no forward uses of the head segment length sym, to allow removing the extracted head segment length
                // load, the corresponding head segment length sym use in the NoImplicitCallUses instruction must also be
                // removed.
                IR::Instr *noImplicitCallUsesInstr;
                IR::Opnd *const use =
                    FindNoImplicitCallUse(
                        instr,
                        arrayRegOpnd->HeadSegmentLengthSym(),
                        [&](IR::Opnd *const checkedSrc) -> bool
                        {
                            return checkedSrc->AsRegOpnd()->m_sym == arrayRegOpnd->HeadSegmentLengthSym();
                        },
                        &noImplicitCallUsesInstr);
                if(use)
                {
                    Assert(noImplicitCallUsesInstr);
                    Assert(!noImplicitCallUsesInstr->GetDst());
                    Assert(noImplicitCallUsesInstr->GetSrc1());
                    if(use == noImplicitCallUsesInstr->GetSrc1())
                    {
                        if(noImplicitCallUsesInstr->GetSrc2())
                        {
                            noImplicitCallUsesInstr->ReplaceSrc1(noImplicitCallUsesInstr->UnlinkSrc2());
                        }
                        else
                        {
                            noImplicitCallUsesInstr->FreeSrc1();
                            noImplicitCallUsesInstr->m_opcode = Js::OpCode::Nop;
                        }
                    }
                    else
                    {
                        Assert(use == noImplicitCallUsesInstr->GetSrc2());
                        noImplicitCallUsesInstr->FreeSrc2();
                    }
                }
            }

            if(headSegmentIsLoadedButUnused &&
                (!isJsArray || !arrayRegOpnd->HeadSegmentLengthSym() || headSegmentLengthIsLoadedButUnused))
            {
                // For JS arrays, the head segment length load is dependent on the head segment. So, only remove the head
                // segment load if the head segment length load can also be removed.
                arrayRegOpnd->RemoveHeadSegmentSym();
                instr->loadedArrayHeadSegment = false;
                if(noImplicitCallArrayUse)
                {
                    noImplicitCallArrayUse->RemoveHeadSegmentSym();
                }
            }
            if(headSegmentLengthIsLoadedButUnused)
            {
                arrayRegOpnd->RemoveHeadSegmentLengthSym();
                instr->loadedArrayHeadSegmentLength = false;
                if(noImplicitCallArrayUse)
                {
                    noImplicitCallArrayUse->RemoveHeadSegmentLengthSym();
                }
            }
        }
    }

    if(isJsArray && instr->m_opcode != Js::OpCode::NoImplicitCallUses)
    {
        // Only uses in NoImplicitCallUses instructions are counted toward liveness
        return;
    }

    // Treat dependent syms as uses. For JS arrays, only uses in NoImplicitCallUses count because only then the assumptions made
    // on the dependent syms are guaranteed to be valid. Similarly for typed arrays, a head segment length sym use counts toward
    // liveness only in a NoImplicitCallUses instruction.
    if(arrayRegOpnd->HeadSegmentSym())
    {
        ProcessStackSymUse(arrayRegOpnd->HeadSegmentSym(), true);
        if(isJsArray)
        {
            block->noImplicitCallUses->Set(arrayRegOpnd->HeadSegmentSym()->m_id);
            block->noImplicitCallJsArrayHeadSegmentSymUses->Set(arrayRegOpnd->HeadSegmentSym()->m_id);
        }
    }
    if(arrayRegOpnd->HeadSegmentLengthSym())
    {
        if(isJsArray)
        {
            ProcessStackSymUse(arrayRegOpnd->HeadSegmentLengthSym(), true);
            block->noImplicitCallUses->Set(arrayRegOpnd->HeadSegmentLengthSym()->m_id);
            block->noImplicitCallJsArrayHeadSegmentSymUses->Set(arrayRegOpnd->HeadSegmentLengthSym()->m_id);
        }
        else
        {
            // ProcessNoImplicitCallUses automatically marks JS array reg opnds and their corresponding syms as live. A typed
            // array's head segment length sym also needs to be marked as live at its use in the NoImplicitCallUses instruction,
            // but it is just in a reg opnd. Flag the opnd to have the sym be marked as live when that instruction is processed.
            Assert(!considerSymAsRealUseInNoImplicitCallUses);
            IR::Opnd *const use =
                FindNoImplicitCallUse(
                    instr,
                    arrayRegOpnd->HeadSegmentLengthSym(),
                    [&](IR::Opnd *const checkedSrc) -> bool
                    {
                        return checkedSrc->AsRegOpnd()->m_sym == arrayRegOpnd->HeadSegmentLengthSym();
                    });
            if(use)
            {
                considerSymAsRealUseInNoImplicitCallUses = arrayRegOpnd->HeadSegmentLengthSym();
            }
        }
    }
    StackSym *const lengthSym = arrayRegOpnd->LengthSym();
    if(lengthSym && lengthSym != arrayRegOpnd->HeadSegmentLengthSym())
    {
        ProcessStackSymUse(lengthSym, true);
        Assert(arrayValueType.IsArray());
        block->noImplicitCallUses->Set(lengthSym->m_id);
        block->noImplicitCallArrayLengthSymUses->Set(lengthSym->m_id);
    }
}

void
BackwardPass::ProcessNewScObject(IR::Instr* instr)
{
    if (this->tag != Js::DeadStorePhase || IsCollectionPass())
    {
        return;
    }

    if (!instr->IsNewScObjectInstr())
    {
        return;
    }

    if (instr->HasBailOutInfo())
    {
        Assert(instr->IsProfiledInstr());
        Assert(instr->GetBailOutKind() == IR::BailOutFailedCtorGuardCheck);
        Assert(instr->GetDst()->IsRegOpnd());

        BasicBlock * block = this->currentBlock;
        StackSym* objSym = instr->GetDst()->AsRegOpnd()->GetStackSym();

        if (block->upwardExposedUses->Test(objSym->m_id))
        {
            // If the object created here is used downstream, let's capture any property operations we must protect.

            Assert(instr->GetDst()->AsRegOpnd()->GetStackSym()->HasObjectTypeSym());

            Js::JitTimeConstructorCache* ctorCache = instr->m_func->GetConstructorCache(static_cast<Js::ProfileId>(instr->AsProfiledInstr()->u.profileId));

            if (block->stackSymToFinalType != nullptr)
            {
                // NewScObject is the origin of the object pointer. If we have a final type in hand, do the
                // transition here.
                AddPropertyCacheBucket *pBucket = block->stackSymToFinalType->Get(objSym->m_id);
                if (pBucket &&
                    pBucket->GetInitialType() != nullptr &&
                    pBucket->GetFinalType() != pBucket->GetInitialType())
                {
                    Assert(pBucket->GetInitialType() == ctorCache->type);
                    if (!this->IsPrePass())
                    {
                        this->InsertTypeTransition(instr->m_next, objSym, pBucket);
                    }
#if DBG
                    pBucket->deadStoreUnavailableInitialType = pBucket->GetInitialType();
                    if (pBucket->deadStoreUnavailableFinalType == nullptr)
                    {
                        pBucket->deadStoreUnavailableFinalType = pBucket->GetFinalType();
                    }
                    pBucket->SetInitialType(nullptr);
                    pBucket->SetFinalType(nullptr);
#else
                    block->stackSymToFinalType->Clear(objSym->m_id);
#endif
                }
            }

            if (block->stackSymToGuardedProperties != nullptr)
            {
                ObjTypeGuardBucket* bucket = block->stackSymToGuardedProperties->Get(objSym->m_id);
                if (bucket != nullptr)
                {
                    BVSparse<JitArenaAllocator>* guardedPropertyOps = bucket->GetGuardedPropertyOps();
                    if (guardedPropertyOps != nullptr)
                    {
                        ctorCache->EnsureGuardedPropOps(this->func->m_alloc);
                        ctorCache->AddGuardedPropOps(guardedPropertyOps);

                        bucket->SetGuardedPropertyOps(nullptr);
                        JitAdelete(this->tempAlloc, guardedPropertyOps);
                        block->stackSymToGuardedProperties->Clear(objSym->m_id);
                    }
                }
            }
        }
        else
        {
            // If the object is not used downstream, let's remove the bailout and let the lowerer emit a fast path along with
            // the fallback on helper, if the ctor cache ever became invalid.
            instr->ClearBailOutInfo();
            if (preOpBailOutInstrToProcess == instr)
            {
                preOpBailOutInstrToProcess = nullptr;
            }

#if DBG
            // We're creating a brand new object here, so no type check upstream could protect any properties of this
            // object. Let's make sure we don't have any left to protect.
            ObjTypeGuardBucket* bucket = block->stackSymToGuardedProperties != nullptr ?
                block->stackSymToGuardedProperties->Get(objSym->m_id) : nullptr;
            Assert(bucket == nullptr || bucket->GetGuardedPropertyOps()->IsEmpty());
#endif
        }
    }
}

void
BackwardPass::UpdateArrayValueTypes(IR::Instr *const instr, IR::Opnd *origOpnd)
{
    Assert(tag == Js::DeadStorePhase);
    Assert(!IsPrePass());
    Assert(instr);

    if(!origOpnd)
    {
        return;
    }

    IR::Instr *opndOwnerInstr = instr;
    switch(instr->m_opcode)
    {
        case Js::OpCode::StElemC:
        case Js::OpCode::StArrSegElemC:
            // These may not be fixed if we are unsure about the type of the array they're storing to
            // (because it relies on profile data) and we weren't able to hoist the array check.
            return;
    }

    Sym *sym;
    IR::Opnd* opnd = origOpnd;
    IR::ArrayRegOpnd *arrayOpnd;
    switch(opnd->GetKind())
    {
        case IR::OpndKindIndir:
            opnd = opnd->AsIndirOpnd()->GetBaseOpnd();
            // fall-through

        case IR::OpndKindReg:
        {
            IR::RegOpnd *const regOpnd = opnd->AsRegOpnd();
            sym = regOpnd->m_sym;
            arrayOpnd = regOpnd->IsArrayRegOpnd() ? regOpnd->AsArrayRegOpnd() : nullptr;
            break;
        }

        case IR::OpndKindSym:
            sym = opnd->AsSymOpnd()->m_sym;
            if(!sym->IsPropertySym())
            {
                return;
            }
            arrayOpnd = nullptr;
            break;

        default:
            return;
    }

    const ValueType valueType(opnd->GetValueType());
    if(!valueType.IsAnyOptimizedArray())
    {
        return;
    }

    const bool isJsArray = valueType.IsArrayOrObjectWithArray();
    Assert(!isJsArray == valueType.IsOptimizedTypedArray());

    const bool noForwardImplicitCallUses = currentBlock->noImplicitCallUses->IsEmpty();
    bool changeArray = isJsArray && !opnd->IsValueTypeFixed() && noForwardImplicitCallUses;
    bool changeNativeArray =
        isJsArray &&
        !opnd->IsValueTypeFixed() &&
        !valueType.HasVarElements() &&
        currentBlock->noImplicitCallNativeArrayUses->IsEmpty();
    bool changeNoMissingValues =
        isJsArray &&
        !opnd->IsValueTypeFixed() &&
        valueType.HasNoMissingValues() &&
        currentBlock->noImplicitCallNoMissingValuesUses->IsEmpty();
    const bool noForwardJsArrayHeadSegmentSymUses = currentBlock->noImplicitCallJsArrayHeadSegmentSymUses->IsEmpty();
    bool removeHeadSegmentSym = isJsArray && arrayOpnd && arrayOpnd->HeadSegmentSym() && noForwardJsArrayHeadSegmentSymUses;
    bool removeHeadSegmentLengthSym =
        arrayOpnd &&
        arrayOpnd->HeadSegmentLengthSym() &&
        (isJsArray ? noForwardJsArrayHeadSegmentSymUses : noForwardImplicitCallUses);
    Assert(!isJsArray || !arrayOpnd || !arrayOpnd->LengthSym() || valueType.IsArray());
    bool removeLengthSym =
        isJsArray &&
        arrayOpnd &&
        arrayOpnd->LengthSym() &&
        currentBlock->noImplicitCallArrayLengthSymUses->IsEmpty();
    if(!(changeArray || changeNoMissingValues || changeNativeArray || removeHeadSegmentSym || removeHeadSegmentLengthSym))
    {
        return;
    }

    // We have a definitely-array value type for the base, but either implicit calls are not currently being disabled for
    // legally using the value type as a definite array, or we are not currently bailing out upon creating a missing value
    // for legally using the value type as a definite array with no missing values.

    // For source opnds, ensure that a NoImplicitCallUses immediately precedes this instruction. Otherwise, convert the value
    // type to an appropriate version so that the lowerer doesn't incorrectly treat it as it says.
    if(opnd != opndOwnerInstr->GetDst())
    {
        if(isJsArray)
        {
            IR::Opnd *const checkedSrc =
                FindNoImplicitCallUse(
                    instr,
                    opnd,
                    [&](IR::Opnd *const checkedSrc) -> bool
                    {
                        const ValueType checkedSrcValueType(checkedSrc->GetValueType());
                        return
                            checkedSrcValueType.IsLikelyObject() &&
                            checkedSrcValueType.GetObjectType() == valueType.GetObjectType();
                    });
            if(checkedSrc)
            {
                // Implicit calls will be disabled to the point immediately before this instruction
                changeArray = false;

                const ValueType checkedSrcValueType(checkedSrc->GetValueType());
                if(changeNativeArray &&
                    !checkedSrcValueType.HasVarElements() &&
                    checkedSrcValueType.HasIntElements() == valueType.HasIntElements())
                {
                    // If necessary, instructions before this will bail out on converting a native array
                    changeNativeArray = false;
                }

                if(changeNoMissingValues && checkedSrcValueType.HasNoMissingValues())
                {
                    // If necessary, instructions before this will bail out on creating a missing value
                    changeNoMissingValues = false;
                }

                if((removeHeadSegmentSym || removeHeadSegmentLengthSym || removeLengthSym) && checkedSrc->IsRegOpnd())
                {
                    IR::RegOpnd *const checkedRegSrc = checkedSrc->AsRegOpnd();
                    if(checkedRegSrc->IsArrayRegOpnd())
                    {
                        IR::ArrayRegOpnd *const checkedArraySrc = checkedSrc->AsRegOpnd()->AsArrayRegOpnd();
                        if(removeHeadSegmentSym && checkedArraySrc->HeadSegmentSym() == arrayOpnd->HeadSegmentSym())
                        {
                            // If necessary, instructions before this will bail out upon invalidating head segment sym
                            removeHeadSegmentSym = false;
                        }
                        if(removeHeadSegmentLengthSym &&
                            checkedArraySrc->HeadSegmentLengthSym() == arrayOpnd->HeadSegmentLengthSym())
                        {
                            // If necessary, instructions before this will bail out upon invalidating head segment length sym
                            removeHeadSegmentLengthSym = false;
                        }
                        if(removeLengthSym && checkedArraySrc->LengthSym() == arrayOpnd->LengthSym())
                        {
                            // If necessary, instructions before this will bail out upon invalidating a length sym
                            removeLengthSym = false;
                        }
                    }
                }
            }
        }
        else
        {
            Assert(removeHeadSegmentLengthSym);

            // A typed array's head segment length may be zeroed when the typed array's buffer is transferred to a web worker,
            // so the head segment length sym use is included in a NoImplicitCallUses instruction. Since there are no forward
            // uses of any head segment length syms, to allow removing the extracted head segment length
            // load, the corresponding head segment length sym use in the NoImplicitCallUses instruction must also be
            // removed.
            IR::Opnd *const use =
                FindNoImplicitCallUse(
                    instr,
                    arrayOpnd->HeadSegmentLengthSym(),
                    [&](IR::Opnd *const checkedSrc) -> bool
                    {
                        return checkedSrc->AsRegOpnd()->m_sym == arrayOpnd->HeadSegmentLengthSym();
                    });
            if(use)
            {
                // Implicit calls will be disabled to the point immediately before this instruction
                removeHeadSegmentLengthSym = false;
            }
        }
    }

    if(changeArray || changeNativeArray)
    {
        if(arrayOpnd)
        {
            opnd = arrayOpnd->CopyAsRegOpnd(opndOwnerInstr->m_func);
            if (origOpnd->IsIndirOpnd())
            {
                origOpnd->AsIndirOpnd()->ReplaceBaseOpnd(opnd->AsRegOpnd());
            }
            else
            {
                opndOwnerInstr->Replace(arrayOpnd, opnd);
            }
            arrayOpnd = nullptr;
        }
        opnd->SetValueType(valueType.ToLikely());
    }
    else
    {
        if(changeNoMissingValues)
        {
            opnd->SetValueType(valueType.SetHasNoMissingValues(false));
        }
        if(removeHeadSegmentSym)
        {
            Assert(arrayOpnd);
            arrayOpnd->RemoveHeadSegmentSym();
        }
        if(removeHeadSegmentLengthSym)
        {
            Assert(arrayOpnd);
            arrayOpnd->RemoveHeadSegmentLengthSym();
        }
        if(removeLengthSym)
        {
            Assert(arrayOpnd);
            arrayOpnd->RemoveLengthSym();
        }
    }
}

void
BackwardPass::UpdateArrayBailOutKind(IR::Instr *const instr)
{
    Assert(!IsPrePass());
    Assert(instr);
    Assert(instr->HasBailOutInfo());

    if (instr->m_opcode != Js::OpCode::StElemI_A && instr->m_opcode != Js::OpCode::StElemI_A_Strict &&
        instr->m_opcode != Js::OpCode::Memcopy && instr->m_opcode != Js::OpCode::Memset ||
        !instr->GetDst()->IsIndirOpnd())
    {
        return;
    }

    IR::RegOpnd *const baseOpnd = instr->GetDst()->AsIndirOpnd()->GetBaseOpnd();
    const ValueType baseValueType(baseOpnd->GetValueType());
    if(baseValueType.IsNotArrayOrObjectWithArray())
    {
        return;
    }

    IR::BailOutKind includeBailOutKinds = IR::BailOutInvalid;
    if(!baseValueType.IsNotNativeArray() &&
        (!baseValueType.IsLikelyNativeArray() || instr->GetSrc1()->IsVar()) &&
        !currentBlock->noImplicitCallNativeArrayUses->IsEmpty())
    {
        // There is an upwards-exposed use of a native array. Since the array referenced by this instruction can be aliased,
        // this instruction needs to bail out if it converts the native array even if this array specifically is not
        // upwards-exposed.
        includeBailOutKinds |= IR::BailOutConvertedNativeArray;
    }

    if(baseOpnd->IsArrayRegOpnd() && baseOpnd->AsArrayRegOpnd()->EliminatedUpperBoundCheck())
    {
        if(instr->extractedUpperBoundCheckWithoutHoisting && !currentBlock->noImplicitCallJsArrayHeadSegmentSymUses->IsEmpty())
        {
            // See comment below regarding head segment invalidation. A failed upper bound check usually means that it will
            // invalidate the head segment length, so change the bailout kind on the upper bound check to have it bail out for
            // the right reason. Even though the store may actually occur in a non-head segment, which would not invalidate the
            // head segment or length, any store outside the head segment bounds causes head segment load elimination to be
            // turned off for the store, because the segment structure of the array is not guaranteed to be the same every time.
            IR::Instr *upperBoundCheck = this->globOpt->FindUpperBoundsCheckInstr(instr);
            Assert(upperBoundCheck && upperBoundCheck != instr);

            if(upperBoundCheck->GetBailOutKind() == IR::BailOutOnArrayAccessHelperCall)
            {
                upperBoundCheck->SetBailOutKind(IR::BailOutOnInvalidatedArrayHeadSegment);
            }
            else
            {
                Assert(upperBoundCheck->GetBailOutKind() == IR::BailOutOnFailedHoistedBoundCheck);
            }
        }
    }
    else
    {
        if(!currentBlock->noImplicitCallJsArrayHeadSegmentSymUses->IsEmpty())
        {
            // There is an upwards-exposed use of a segment sym. Since the head segment syms referenced by this instruction can
            // be aliased, this instruction needs to bail out if it changes the segment syms it references even if the ones it
            // references specifically are not upwards-exposed. This bailout kind also guarantees that this element store will
            // not create missing values.
            includeBailOutKinds |= IR::BailOutOnInvalidatedArrayHeadSegment;
        }
        else if(
            !currentBlock->noImplicitCallNoMissingValuesUses->IsEmpty() &&
            !(instr->GetBailOutKind() & IR::BailOutOnArrayAccessHelperCall))
        {
            // There is an upwards-exposed use of an array with no missing values. Since the array referenced by this
            // instruction can be aliased, this instruction needs to bail out if it creates a missing value in the array even if
            // this array specifically is not upwards-exposed.
            includeBailOutKinds |= IR::BailOutOnMissingValue;
        }

        if(!baseValueType.IsNotArray() && !currentBlock->noImplicitCallArrayLengthSymUses->IsEmpty())
        {
            // There is an upwards-exposed use of a length sym. Since the length sym referenced by this instruction can be
            // aliased, this instruction needs to bail out if it changes the length sym it references even if the ones it
            // references specifically are not upwards-exposed.
            includeBailOutKinds |= IR::BailOutOnInvalidatedArrayLength;
        }
    }

    if(!includeBailOutKinds)
    {
        return;
    }

    Assert(!(includeBailOutKinds & ~IR::BailOutKindBits));
    instr->SetBailOutKind(instr->GetBailOutKind() | includeBailOutKinds);
}

bool
BackwardPass::ProcessStackSymUse(StackSym * stackSym, BOOLEAN isNonByteCodeUse)
{
    BasicBlock * block = this->currentBlock;

    if (this->DoByteCodeUpwardExposedUsed())
    {
        if (!isNonByteCodeUse && stackSym->HasByteCodeRegSlot())
        {
            // Always track the sym use on the var sym.
            StackSym * byteCodeUseSym = stackSym;
            if (byteCodeUseSym->IsTypeSpec())
            {
                // It has to have a var version for byte code regs
                byteCodeUseSym = byteCodeUseSym->GetVarEquivSym(nullptr);
            }
            block->byteCodeUpwardExposedUsed->Set(byteCodeUseSym->m_id);
#if DBG
            // We can only track first level function stack syms right now
            if (byteCodeUseSym->GetByteCodeFunc() == this->func)
            {
                Js::RegSlot byteCodeRegSlot = byteCodeUseSym->GetByteCodeRegSlot();
                if (block->byteCodeRestoreSyms[byteCodeRegSlot] != byteCodeUseSym)
                {
                    AssertMsg(block->byteCodeRestoreSyms[byteCodeRegSlot] == nullptr,
                        "Can't have two active lifetime for the same byte code register");
                    block->byteCodeRestoreSyms[byteCodeRegSlot] = byteCodeUseSym;
                }
            }
#endif
        }
    }

    if(IsCollectionPass())
    {
        return true;
    }

    if (this->DoMarkTempObjects())
    {
        Assert((block->loop != nullptr) == block->tempObjectTracker->HasTempTransferDependencies());
        block->tempObjectTracker->ProcessUse(stackSym, this);
    }
#if DBG
    if (this->DoMarkTempObjectVerify())
    {
        Assert((block->loop != nullptr) == block->tempObjectVerifyTracker->HasTempTransferDependencies());
        block->tempObjectVerifyTracker->ProcessUse(stackSym, this);
    }
#endif
    return !!block->upwardExposedUses->TestAndSet(stackSym->m_id);
}

bool
BackwardPass::ProcessSymUse(Sym * sym, bool isRegOpndUse, BOOLEAN isNonByteCodeUse)
{
    BasicBlock * block = this->currentBlock;
    if (sym->IsPropertySym())
    {
        PropertySym * propertySym = sym->AsPropertySym();
        ProcessStackSymUse(propertySym->m_stackSym, isNonByteCodeUse);

        if(IsCollectionPass())
        {
            return true;
        }

        Assert((block->fieldHoistCandidates != nullptr) == this->DoFieldHoistCandidates());

        if (block->fieldHoistCandidates && GlobOpt::TransferSrcValue(this->currentInstr))
        {
            // If the instruction doesn't transfer the src value to dst, it will not be copyprop'd
            // So we can't hoist those.
            block->fieldHoistCandidates->Set(propertySym->m_id);
        }

        if (this->DoDeadStoreSlots())
        {
            block->slotDeadStoreCandidates->Clear(propertySym->m_id);
        }

        if (tag == Js::BackwardPhase)
        {
            // Backward phase tracks liveness of fields to tell GlobOpt where we may need bailout.
            return this->ProcessPropertySymUse(propertySym);
        }
        else
        {
            // Dead-store phase tracks copy propped syms, so it only cares about ByteCodeUses we inserted,
            // not live fields.
            return false;
        }
    }

    StackSym * stackSym = sym->AsStackSym();
    bool isUsed = ProcessStackSymUse(stackSym, isNonByteCodeUse);

    if (!IsCollectionPass() && isRegOpndUse && this->DoMarkTempNumbers())
    {
        // Collect mark temp number information
        Assert((block->loop != nullptr) == block->tempNumberTracker->HasTempTransferDependencies());
        block->tempNumberTracker->ProcessUse(stackSym, this);
    }

    return isUsed;
}

bool
BackwardPass::MayPropertyBeWrittenTo(Js::PropertyId propertyId)
{
    return this->func->anyPropertyMayBeWrittenTo ||
        (this->func->propertiesWrittenTo != nullptr && this->func->propertiesWrittenTo->ContainsKey(propertyId));
}

void
BackwardPass::ProcessPropertySymOpndUse(IR::PropertySymOpnd * opnd)
{

    // If this operand doesn't participate in the type check sequence it's a pass-through.
    // We will not set any bits on the operand and we will ignore them when lowering.
    if (!opnd->IsTypeCheckSeqCandidate())
    {
        return;
    }

    AssertMsg(opnd->HasObjectTypeSym(), "Optimized property sym operand without a type sym?");
    SymID typeSymId = opnd->GetObjectTypeSym()->m_id;

    BasicBlock * block = this->currentBlock;
    if (this->tag == Js::BackwardPhase)
    {
        // In the backward phase, we have no availability info, and we're trying to see
        // where there are live fields so we can decide where to put bailouts.

        Assert(opnd->MayNeedTypeCheckProtection());

        block->upwardExposedFields->Set(typeSymId);

        TrackObjTypeSpecWriteGuards(opnd, block);
    }
    else
    {
        // In the dead-store phase, we're trying to see where the lowered code needs to make sure to check
        // types for downstream load/stores. We're also setting up the upward-exposed uses at loop headers
        // so register allocation will be correct.

        Assert(opnd->MayNeedTypeCheckProtection());

        const bool isStore = opnd == this->currentInstr->GetDst();

        // Note that we don't touch upwardExposedUses here.
        if (opnd->IsTypeAvailable())
        {
            opnd->SetTypeDead(!block->upwardExposedFields->TestAndSet(typeSymId));

            if (opnd->IsTypeChecked() && opnd->IsObjectHeaderInlined())
            {
                // The object's type must not change in a way that changes the layout.
                // If we see a StFld with a type check bailout between here and the type check that guards this
                // property, we must not dead-store the StFld's type check bailout, even if that operand's type appears
                // dead, because that object may alias this one.
                BVSparse<JitArenaAllocator>* bv = block->typesNeedingKnownObjectLayout;
                if (bv == nullptr)
                {
                    bv = JitAnew(this->tempAlloc, BVSparse<JitArenaAllocator>, this->tempAlloc);
                    block->typesNeedingKnownObjectLayout = bv;
                }
                bv->Set(typeSymId);
            }
        }
        else
        {
            opnd->SetTypeDead(
                !block->upwardExposedFields->TestAndClear(typeSymId) &&
                (
                    // Don't set the type dead if this is a store that may change the layout in a way that invalidates
                    // optimized load/stores downstream. Leave it non-dead in that case so the type check bailout
                    // is preserved and so that Lower will generate the bailout properly.
                    !isStore ||
                    !block->typesNeedingKnownObjectLayout ||
                    block->typesNeedingKnownObjectLayout->IsEmpty()
                )
            );

            BVSparse<JitArenaAllocator>* bv = block->typesNeedingKnownObjectLayout;
            if (bv != nullptr)
            {
                bv->Clear(typeSymId);
            }
        }

        bool mayNeedTypeTransition = true;
        if (!opnd->HasTypeMismatch() && func->DoGlobOpt())
        {
            mayNeedTypeTransition = !isStore;
        }
        if (mayNeedTypeTransition &&
            !this->IsPrePass() &&
            !this->currentInstr->HasBailOutInfo() &&
            (opnd->NeedsPrimaryTypeCheck() ||
             opnd->NeedsLocalTypeCheck() ||
             opnd->NeedsLoadFromProtoTypeCheck()))
        {
            // This is a "checked" opnd that nevertheless will have some kind of type check generated for it.
            // (Typical case is a load from prototype with no upstream guard.)
            // If the type check fails, we will call a helper, which will require that the type be correct here.
            // Final type can't be pushed up past this point. Do whatever type transition is required.
            if (block->stackSymToFinalType != nullptr)
            {
                StackSym *baseSym = opnd->GetObjectSym();
                AddPropertyCacheBucket *pBucket = block->stackSymToFinalType->Get(baseSym->m_id);
                if (pBucket &&
                    pBucket->GetFinalType() != nullptr &&
                    pBucket->GetFinalType() != pBucket->GetInitialType())
                {
                    this->InsertTypeTransition(this->currentInstr->m_next, baseSym, pBucket);
                    pBucket->SetFinalType(pBucket->GetInitialType());
                }
            }
        }
        if (!opnd->HasTypeMismatch() && func->DoGlobOpt())
        {
            // Do this after the above code, as the value of the final type may change there.
            TrackAddPropertyTypes(opnd, block);
        }

        TrackObjTypeSpecProperties(opnd, block);
        TrackObjTypeSpecWriteGuards(opnd, block);
    }
}

void
BackwardPass::TrackObjTypeSpecProperties(IR::PropertySymOpnd *opnd, BasicBlock *block)
{
    Assert(tag == Js::DeadStorePhase);
    Assert(opnd->IsTypeCheckSeqCandidate());

    // Now that we're in the dead store pass and we know definitively which operations will have a type
    // check and which are protected by an upstream type check, we can push the lists of guarded properties
    // up the flow graph and drop them on the type checks for the corresponding object symbol.
    if (opnd->IsTypeCheckSeqParticipant())
    {
        // Add this operation to the list of guarded operations for this object symbol.
        HashTable<ObjTypeGuardBucket>* stackSymToGuardedProperties = block->stackSymToGuardedProperties;
        if (stackSymToGuardedProperties == nullptr)
        {
            stackSymToGuardedProperties = HashTable<ObjTypeGuardBucket>::New(this->tempAlloc, 8);
            block->stackSymToGuardedProperties = stackSymToGuardedProperties;
        }

        StackSym* objSym = opnd->GetObjectSym();
        ObjTypeGuardBucket* bucket = stackSymToGuardedProperties->FindOrInsertNew(objSym->m_id);
        BVSparse<JitArenaAllocator>* guardedPropertyOps = bucket->GetGuardedPropertyOps();
        if (guardedPropertyOps == nullptr)
        {
            // The bit vectors we push around the flow graph only need to live as long as this phase.
            guardedPropertyOps = JitAnew(this->tempAlloc, BVSparse<JitArenaAllocator>, this->tempAlloc);
            bucket->SetGuardedPropertyOps(guardedPropertyOps);
        }

#if DBG
        FOREACH_BITSET_IN_SPARSEBV(propOpId, guardedPropertyOps)
        {
            Js::ObjTypeSpecFldInfo* existingFldInfo = this->func->GetGlobalObjTypeSpecFldInfo(propOpId);
            Assert(existingFldInfo != nullptr);

            if (existingFldInfo->GetPropertyId() != opnd->GetPropertyId())
            {
                continue;
            }
            // It would be very nice to assert that the info we have for this property matches all properties guarded thus far.
            // Unfortunately, in some cases of object pointer copy propagation into a loop, we may end up with conflicting
            // information for the same property. We simply ignore the conflict and emit an equivalent type check, which
            // will attempt to check for one property on two different slots, and obviously fail. Thus we may have a
            // guaranteed bailout, but we'll simply re-JIT with equivalent object type spec disabled. To avoid this
            // issue altogether, we would need to track the set of guarded properties along with the type value in the
            // forward pass, and when a conflict is detected either not optimize the offending instruction, or correct
            // its information based on the info from the property in the type value info.
            //Assert(!existingFldInfo->IsPoly() || !opnd->IsPoly() || GlobOpt::AreTypeSetsIdentical(existingFldInfo->GetEquivalentTypeSet(), opnd->GetEquivalentTypeSet()));
            //Assert(existingFldInfo->GetSlotIndex() == opnd->GetSlotIndex());

            if (PHASE_TRACE(Js::EquivObjTypeSpecPhase, this->func))
            {
                if (existingFldInfo->IsPoly() && opnd->IsPoly() &&
                    (!GlobOpt::AreTypeSetsIdentical(existingFldInfo->GetEquivalentTypeSet(), opnd->GetEquivalentTypeSet()) ||
                    (existingFldInfo->GetSlotIndex() != opnd->GetSlotIndex())))
                {
                    wchar_t debugStringBuffer[MAX_FUNCTION_BODY_DEBUG_STRING_SIZE];
                    Js::FunctionBody* topFunctionBody = this->func->GetJnFunction();
                    Js::ScriptContext* scriptContext = topFunctionBody->GetScriptContext();

                    Output::Print(L"EquivObjTypeSpec: top function %s (%s): duplicate property clash on %s(#%d) on operation %u \n",
                        topFunctionBody->GetDisplayName(), topFunctionBody->GetDebugNumberSet(debugStringBuffer),
                        scriptContext->GetPropertyNameLocked(opnd->GetPropertyId())->GetBuffer(), opnd->GetPropertyId(), opnd->GetObjTypeSpecFldId());
                    Output::Flush();
                }
            }
        }
        NEXT_BITSET_IN_SPARSEBV
#endif

        bucket->AddToGuardedPropertyOps(opnd->GetObjTypeSpecFldId());
        if (opnd->NeedsMonoCheck())
        {
            Assert(opnd->IsMono());
            bucket->SetNeedsMonoCheck(true);
        }

        if (opnd->NeedsPrimaryTypeCheck())
        {
            // Grab the guarded properties which match this type check with respect to polymorphism and drop them
            // on the operand. Only equivalent type checks can protect polymorphic properties to avoid a case where
            // we have 1) a cache with type set {t1, t2} and property a, followed by 2) a cache with type t3 and
            // property b, and 3) a cache with type set {t1, t2} and property c, where the slot index of property c
            // on t1 and t2 is different than on t3. If cache 2 were to protect property c it would not verify that
            // it resides on the correct slot for cache 3.  Yes, an equivalent type check could protect monomorphic
            // properties, but it would then unnecessarily verify their equivalence on the slow path.

            // Also, make sure the guarded properties on the operand are allocated from the func's allocator to
            // persists until lowering.

            Assert(guardedPropertyOps != nullptr);
            opnd->EnsureGuardedPropOps(this->func->m_alloc);
            opnd->AddGuardedPropOps(guardedPropertyOps);
            if (bucket->NeedsMonoCheck())
            {
                if (this->currentInstr->HasEquivalentTypeCheckBailOut())
                {
                    // Some instr protected by this one requires a monomorphic type check. (E.g., final type opt,
                    // fixed field not loaded from prototype.)
                    Assert(opnd->IsMono());
                    opnd->SetMustDoMonoCheck(true);
                    this->currentInstr->ChangeEquivalentToMonoTypeCheckBailOut();
                }
                bucket->SetNeedsMonoCheck(false);
            }

            bucket->SetGuardedPropertyOps(nullptr);
            JitAdelete(this->tempAlloc, guardedPropertyOps);
            block->stackSymToGuardedProperties->Clear(objSym->m_id);

#if DBG
            // If there is no upstream type check that is live and could protect guarded properties, we better
            // not have any properties remaining.
            ObjTypeGuardBucket* bucket = block->stackSymToGuardedProperties->Get(opnd->GetObjectSym()->m_id);
            Assert(opnd->IsTypeAvailable() || bucket == nullptr || bucket->GetGuardedPropertyOps()->IsEmpty());
#endif
        }
    }
    else if (opnd->NeedsLocalTypeCheck())
    {
        opnd->EnsureGuardedPropOps(this->func->m_alloc);
        opnd->SetGuardedPropOp(opnd->GetObjTypeSpecFldId());
    }
}

void
BackwardPass::TrackObjTypeSpecWriteGuards(IR::PropertySymOpnd *opnd, BasicBlock *block)
{
    // TODO (ObjTypeSpec): Move write guard tracking to the forward pass, by recording on the type value
    // which property IDs have been written since the last type check. This will result in more accurate
    // tracking in cases when object pointer copy prop kicks in.
    if (this->tag == Js::BackwardPhase)
    {
        // If this operation may need a write guard (load from proto or fixed field check) then add its
        // write guard symbol to the map for this object. If it remains live (hasn't been written to)
        // until the type check upstream, it will get recorded there so that the type check can be registered
        // for invalidation on this property used in this operation.

        // (ObjTypeSpec): Consider supporting polymorphic write guards as well. We can't currently distinguish between mono and
        // poly write guards, and a type check can only protect operations matching with respect to polymorphism (see
        // BackwardPass::TrackObjTypeSpecProperties for details), so for now we only target monomorphic operations.
        if (opnd->IsMono() && opnd->MayNeedWriteGuardProtection())
        {
            if (block->stackSymToWriteGuardsMap == nullptr)
            {
                block->stackSymToWriteGuardsMap = HashTable<ObjWriteGuardBucket>::New(this->tempAlloc, 8);
            }

            ObjWriteGuardBucket* bucket = block->stackSymToWriteGuardsMap->FindOrInsertNew(opnd->GetObjectSym()->m_id);

            BVSparse<JitArenaAllocator>* writeGuards = bucket->GetWriteGuards();
            if (writeGuards == nullptr)
            {
                // The bit vectors we push around the flow graph only need to live as long as this phase.
                writeGuards = JitAnew(this->tempAlloc, BVSparse<JitArenaAllocator>, this->tempAlloc);
                bucket->SetWriteGuards(writeGuards);
            }

            PropertySym *propertySym = opnd->m_sym->AsPropertySym();
            Assert(propertySym->m_writeGuardSym != nullptr);
            SymID writeGuardSymId = propertySym->m_writeGuardSym->m_id;
            writeGuards->Set(writeGuardSymId);
        }

        // Record any live (upward exposed) write guards on this operation, if this operation may end up with
        // a type check.  If we ultimately don't need a type check here, we will simply ignore the guards, because
        // an earlier type check will protect them.
        if (!IsPrePass() && opnd->IsMono() && !opnd->IsTypeDead())
        {
            Assert(opnd->GetWriteGuards() == nullptr);
            if (block->stackSymToWriteGuardsMap != nullptr)
            {
                ObjWriteGuardBucket* bucket = block->stackSymToWriteGuardsMap->Get(opnd->GetObjectSym()->m_id);
                if (bucket != nullptr)
                {
                    // Get all the write guards associated with this object sym and filter them down to those that
                    // are upward exposed. If we end up emitting a type check for this instruction, we will create
                    // a type property guard registered for all guarded proto properties and we will set the write
                    // guard syms live during forward pass, such that we can avoid unnecessary write guard type
                    // checks and bailouts on every proto property (as long as it hasn't been written to since the
                    // primary type check).
                    auto writeGuards = bucket->GetWriteGuards()->CopyNew(this->func->m_alloc);
                    writeGuards->And(block->upwardExposedFields);
                    opnd->SetWriteGuards(writeGuards);
                }
            }
        }
    }
    else
    {
        // If we know this property has never been written to in this function (either on this object or any
        // of its aliases) we don't need the local type check.
        if (opnd->MayNeedWriteGuardProtection() && !opnd->IsWriteGuardChecked() && !MayPropertyBeWrittenTo(opnd->GetPropertyId()))
        {
            opnd->SetWriteGuardChecked(true);
        }

        // If we don't need a primary type check here let's clear the write guards. The primary type check upstream will
        // register the type check for the corresponding properties.
        if (!IsPrePass() && !opnd->NeedsPrimaryTypeCheck())
        {
            opnd->ClearWriteGuards();
        }
    }
}

void
BackwardPass::TrackAddPropertyTypes(IR::PropertySymOpnd *opnd, BasicBlock *block)
{
    // Do the work of objtypespec add-property opt even if it's disabled by PHASE option, so that we have
    // the dataflow info that can be inspected.

    Assert(this->tag == Js::DeadStorePhase);
    Assert(opnd->IsMono() || opnd->HasEquivalentTypeSet());

    Js::Type *typeWithProperty = opnd->IsMono() ? opnd->GetType() : opnd->GetFirstEquivalentType();
    Js::Type *typeWithoutProperty = opnd->HasInitialType() ? opnd->GetInitialType() : nullptr;

    if (typeWithoutProperty == nullptr ||
        typeWithProperty == typeWithoutProperty ||
        (opnd->IsTypeChecked() && !opnd->IsInitialTypeChecked()))
    {
        if (!this->IsPrePass() && block->stackSymToFinalType != nullptr && !this->currentInstr->HasBailOutInfo())
        {
            PropertySym *propertySym = opnd->m_sym->AsPropertySym();
            AddPropertyCacheBucket *pBucket =
                block->stackSymToFinalType->Get(propertySym->m_stackSym->m_id);
            if (pBucket && pBucket->GetFinalType() && pBucket->GetInitialType() != pBucket->GetFinalType())
            {
                opnd->SetFinalType(pBucket->GetFinalType());
            }
        }

        return;
    }

#if DBG
    Assert(typeWithProperty != nullptr);
    Js::DynamicTypeHandler * typeWithoutPropertyTypeHandler = static_cast<Js::DynamicType *>(typeWithoutProperty)->GetTypeHandler();
    Js::DynamicTypeHandler * typeWithPropertyTypeHandler = static_cast<Js::DynamicType *>(typeWithProperty)->GetTypeHandler();
    Assert(typeWithoutPropertyTypeHandler->GetPropertyCount() + 1 == typeWithPropertyTypeHandler->GetPropertyCount());
    AssertMsg(Js::DynamicObject::IsTypeHandlerCompatibleForObjectHeaderInlining(typeWithoutPropertyTypeHandler, typeWithPropertyTypeHandler),
        "TypeHandlers are not compatible for transition?");
    Assert(typeWithoutPropertyTypeHandler->GetSlotCapacity() <= typeWithPropertyTypeHandler->GetSlotCapacity());
#endif

    // If there's already a final type for this instance, record it on the operand.
    // If not, start tracking it.
    if (block->stackSymToFinalType == nullptr)
    {
        block->stackSymToFinalType = HashTable<AddPropertyCacheBucket>::New(this->tempAlloc, 8);
    }

    // Find or create the type-tracking record for this instance in this block.
    PropertySym *propertySym = opnd->m_sym->AsPropertySym();
    AddPropertyCacheBucket *pBucket =
        block->stackSymToFinalType->FindOrInsertNew(propertySym->m_stackSym->m_id);

    Js::Type* finalType = nullptr;
#if DBG
    Js::Type * deadStoreUnavailableFinalType = nullptr;
#endif
    if (pBucket->GetInitialType() == nullptr || opnd->GetType() != pBucket->GetInitialType())
    {
#if DBG
        if (opnd->GetType() == pBucket->deadStoreUnavailableInitialType)
        {
            deadStoreUnavailableFinalType = pBucket->deadStoreUnavailableFinalType;
        }
#endif
        // No info found, or the info was bad, so initialize it from this cache.
        finalType = opnd->GetType();
        pBucket->SetFinalType(finalType);
    }
    else
    {
        // Match: The type we push upward is now the typeWithoutProperty at this point,
        // and the final type is the one we've been tracking.
        finalType = pBucket->GetFinalType();
#if DBG
        deadStoreUnavailableFinalType = pBucket->deadStoreUnavailableFinalType;
#endif
    }

    pBucket->SetInitialType(typeWithoutProperty);

    if (!PHASE_OFF(Js::ObjTypeSpecStorePhase, this->func))
    {
#if DBG

        // We may regress in this case:
        // if (b)
        //      t1 = {};
        //      o = t1;
        //      o.x =
        // else
        //      t2 = {};
        //      o = t2;
        //      o.x =
        // o.y =
        //
        // Where the backward pass will propagate the final type in o.y to o.x, then globopt will copy prop t1 and t2 to o.x.
        // But not o.y (because of the merge).  Then, in the dead store pass, o.y's final type will not propagate to t1.x and t2.x
        // respectively, thus regression the final type.  However, in both cases, the types of t1 and t2 are dead anyways.
        //
        // if the type is dead, we don't care if we have regressed the type, as no one is depending on it to skip type check anyways
        if (!opnd->IsTypeDead())
        {
            // This is the type that would have been propagated if we didn't kill it because the type isn't available
            Js::Type * checkFinalType = deadStoreUnavailableFinalType ? deadStoreUnavailableFinalType : finalType;
            if (opnd->HasFinalType() && opnd->GetFinalType() != checkFinalType)
            {
                // Final type discovery must be progressively better (unless we kill it in the deadstore pass
                // when the type is not available during the forward pass)
                Js::DynamicTypeHandler * oldFinalTypeHandler = static_cast<Js::DynamicType *>(opnd->GetFinalType())->GetTypeHandler();
                Js::DynamicTypeHandler * checkFinalTypeHandler = static_cast<Js::DynamicType *>(checkFinalType)->GetTypeHandler();
                Assert(oldFinalTypeHandler->GetPropertyCount() < checkFinalTypeHandler->GetPropertyCount());
                AssertMsg(Js::DynamicObject::IsTypeHandlerCompatibleForObjectHeaderInlining(oldFinalTypeHandler, checkFinalTypeHandler),
                    "TypeHandlers should be compatible for transition.");
                Assert(oldFinalTypeHandler->GetSlotCapacity() <= checkFinalTypeHandler->GetSlotCapacity());
            }
        }
#endif
        Assert(opnd->IsBeingAdded());
        if (!this->IsPrePass())
        {
            opnd->SetFinalType(finalType);
        }
        if (!opnd->IsTypeChecked())
        {
            // Transition from initial to final type will only happen at type check points.
            if (opnd->IsTypeAvailable())
            {
                pBucket->SetFinalType(pBucket->GetInitialType());
            }
        }
    }

#if DBG_DUMP
    if (PHASE_TRACE(Js::ObjTypeSpecStorePhase, this->func))
    {
        Output::Print(L"ObjTypeSpecStore: ");
        this->currentInstr->Dump();
        pBucket->Dump();
    }
#endif

    // In the dead-store pass, we have forward information that tells us whether a "final type"
    // reached this point from an earlier store. If it didn't (i.e., it's not available here),
    // remove it from the backward map so that upstream stores will use the final type that is
    // live there. (This avoids unnecessary bailouts in cases where the final type is only live
    // on one branch of an "if", a case that the initial backward pass can't detect.)
    // An example:
    //  if (cond)
    //      o.x =
    //  o.y =

    if (!opnd->IsTypeAvailable())
    {
#if DBG
        pBucket->deadStoreUnavailableInitialType = pBucket->GetInitialType();
        if (pBucket->deadStoreUnavailableFinalType == nullptr)
        {
            pBucket->deadStoreUnavailableFinalType = pBucket->GetFinalType();
        }
        pBucket->SetInitialType(nullptr);
        pBucket->SetFinalType(nullptr);
#else
        block->stackSymToFinalType->Clear(propertySym->m_stackSym->m_id);
#endif
    }
}

void
BackwardPass::InsertTypeTransition(IR::Instr *instrInsertBefore, int symId, AddPropertyCacheBucket *data)
{
    StackSym *objSym = this->func->m_symTable->FindStackSym(symId);
    Assert(objSym);
    this->InsertTypeTransition(instrInsertBefore, objSym, data);
}

void
BackwardPass::InsertTypeTransition(IR::Instr *instrInsertBefore, StackSym *objSym, AddPropertyCacheBucket *data)
{
    IR::RegOpnd *baseOpnd = IR::RegOpnd::New(objSym, TyMachReg, this->func);
    baseOpnd->SetIsJITOptimizedReg(true);

    IR::AddrOpnd *initialTypeOpnd =
        IR::AddrOpnd::New(data->GetInitialType(), IR::AddrOpndKindDynamicType, this->func);
    IR::AddrOpnd *finalTypeOpnd =
        IR::AddrOpnd::New(data->GetFinalType(), IR::AddrOpndKindDynamicType, this->func);

    IR::Instr *adjustTypeInstr =
        IR::Instr::New(Js::OpCode::AdjustObjType, finalTypeOpnd, baseOpnd, initialTypeOpnd, this->func);

    instrInsertBefore->InsertBefore(adjustTypeInstr);
}

void
BackwardPass::InsertTypeTransitionAfterInstr(IR::Instr *instr, int symId, AddPropertyCacheBucket *data)
{
    if (!this->IsPrePass())
    {
        // Transition to the final type if we don't bail out.
        if (instr->EndsBasicBlock())
        {
            // The instr with the bailout is something like a branch that may not fall through.
            // Insert the transitions instead at the beginning of each successor block.
            this->InsertTypeTransitionsAtPriorSuccessors(this->currentBlock, nullptr, symId, data);
        }
        else
        {
            this->InsertTypeTransition(instr->m_next, symId, data);
        }
    }
    // Note: we could probably clear this entry out of the table, but I don't know
    // whether it's worth it, because it's likely coming right back.
    data->SetFinalType(data->GetInitialType());
}

void
BackwardPass::InsertTypeTransitionAtBlock(BasicBlock *block, int symId, AddPropertyCacheBucket *data)
{
    bool inserted = false;
    FOREACH_INSTR_IN_BLOCK(instr, block)
    {
        if (instr->IsRealInstr())
        {
            // Check for pre-existing type transition. There may be more than one AdjustObjType here,
            // so look at them all.
            if (instr->m_opcode == Js::OpCode::AdjustObjType)
            {
                if (instr->GetSrc1()->AsRegOpnd()->m_sym->m_id == (SymID)symId)
                {
                    // This symbol already has a type transition at this point.
                    // It *must* be doing the same transition we're already trying to do.
                    Assert(instr->GetDst()->AsAddrOpnd()->m_address == data->GetFinalType() &&
                           instr->GetSrc2()->AsAddrOpnd()->m_address == data->GetInitialType());
                    // Nothing to do.
                    return;
                }
            }
            else
            {
                this->InsertTypeTransition(instr, symId, data);
                inserted = true;
                break;
            }
        }
    }
    NEXT_INSTR_IN_BLOCK;
    if (!inserted)
    {
        Assert(block->GetLastInstr()->m_next);
        this->InsertTypeTransition(block->GetLastInstr()->m_next, symId, data);
    }
}

void
BackwardPass::InsertTypeTransitionsAtPriorSuccessors(
    BasicBlock *block,
    BasicBlock *blockSucc,
    int symId,
    AddPropertyCacheBucket *data)
{
    // For each successor of block prior to blockSucc, adjust the type.
    FOREACH_SUCCESSOR_BLOCK(blockFix, block)
    {
        if (blockFix == blockSucc)
        {
            return;
        }

        this->InsertTypeTransitionAtBlock(blockFix, symId, data);
    }
    NEXT_SUCCESSOR_BLOCK;
}

void
BackwardPass::InsertTypeTransitionsAtPotentialKills()
{
    // Final types can't be pushed up past certain instructions.
    IR::Instr *instr = this->currentInstr;

    if (instr->HasBailOutInfo() || instr->m_opcode == Js::OpCode::UpdateNewScObjectCache)
    {
        // Final types can't be pushed up past a bailout point.
        // Insert any transitions called for by the current state of add-property buckets.
        // Also do this for ctor cache updates, to avoid putting a type in the ctor cache that extends past
        // the end of the ctor that the cache covers.
        this->ForEachAddPropertyCacheBucket([&](int symId, AddPropertyCacheBucket *data)->bool {
            this->InsertTypeTransitionAfterInstr(instr, symId, data);
            return false;
        });
    }
    else
    {
        // If this is a load/store that expects an object-header-inlined type, don't push another sym's transition from
        // object-header-inlined to non-object-header-inlined type past it, because the two syms may be aliases.
        IR::PropertySymOpnd *propertySymOpnd = instr->GetPropertySymOpnd();
        if (propertySymOpnd && propertySymOpnd->IsObjectHeaderInlined())
        {
            SymID opndId = propertySymOpnd->m_sym->AsPropertySym()->m_stackSym->m_id;
            this->ForEachAddPropertyCacheBucket([&](int symId, AddPropertyCacheBucket *data)->bool {
                if ((SymID)symId == opndId)
                {
                    // This is the sym we're tracking. No aliasing to worry about.
                    return false;
                }
                if (propertySymOpnd->IsMono() && data->GetInitialType() != propertySymOpnd->GetType())
                {
                    // Type mismatch in a monomorphic case -- no aliasing.
                    return false;
                }
                if (this->TransitionUndoesObjectHeaderInlining(data))
                {
                    // We're transitioning from inlined to non-inlined, so we can't push it up any farther.
                    this->InsertTypeTransitionAfterInstr(instr, symId, data);
                }
                return false;
            });
        }
    }
}

template<class Fn>
void
BackwardPass::ForEachAddPropertyCacheBucket(Fn fn)
{
    BasicBlock *block = this->currentBlock;
    if (block->stackSymToFinalType == nullptr)
    {
        return;
    }

    FOREACH_HASHTABLE_ENTRY(AddPropertyCacheBucket, bucket, block->stackSymToFinalType)
    {
        AddPropertyCacheBucket *data = &bucket.element;
        if (data->GetInitialType() != nullptr &&
            data->GetInitialType() != data->GetFinalType())
        {
            bool done = fn(bucket.value, data);
            if (done)
            {
                break;
            }
        }
    }
    NEXT_HASHTABLE_ENTRY;
}

bool
BackwardPass::TransitionUndoesObjectHeaderInlining(AddPropertyCacheBucket *data) const
{
    Js::Type *type = data->GetInitialType();
    if (type == nullptr || !Js::DynamicType::Is(type->GetTypeId()))
    {
        return false;
    }
    Js::DynamicType *dynamicType = static_cast<Js::DynamicType*>(type);
    if (!dynamicType->GetTypeHandler()->IsObjectHeaderInlinedTypeHandler())
    {
        return false;
    }

    type = data->GetFinalType();
    if (type == nullptr || !Js::DynamicType::Is(type->GetTypeId()))
    {
        return false;
    }
    dynamicType = static_cast<Js::DynamicType*>(type);
    return !dynamicType->GetTypeHandler()->IsObjectHeaderInlinedTypeHandler();
}

void
BackwardPass::CollectCloneStrCandidate(IR::Opnd * opnd)
{
    IR::RegOpnd *regOpnd = opnd->AsRegOpnd();
    Assert(regOpnd != nullptr);
    StackSym *sym = regOpnd->m_sym;

    if (tag == Js::BackwardPhase
        && currentInstr->m_opcode == Js::OpCode::Add_A
        && currentInstr->GetSrc1() == opnd
        && !this->IsPrePass()
        && !this->IsCollectionPass()
        &&  this->currentBlock->loop)
    {
        Assert(currentBlock->cloneStrCandidates != nullptr);

        currentBlock->cloneStrCandidates->Set(sym->m_id);
    }
}

void
BackwardPass::InvalidateCloneStrCandidate(IR::Opnd * opnd)
{
    IR::RegOpnd *regOpnd = opnd->AsRegOpnd();
    Assert(regOpnd != nullptr);
    StackSym *sym = regOpnd->m_sym;

    if (tag == Js::BackwardPhase &&
        (currentInstr->m_opcode != Js::OpCode::Add_A || currentInstr->GetSrc1()->AsRegOpnd()->m_sym->m_id != sym->m_id) &&
        !this->IsPrePass() &&
        !this->IsCollectionPass() &&
        this->currentBlock->loop)
    {
            currentBlock->cloneStrCandidates->Clear(sym->m_id);
    }
}

void
BackwardPass::ProcessUse(IR::Opnd * opnd)
{
    switch (opnd->GetKind())
    {
    case IR::OpndKindReg:
        {
            IR::RegOpnd *regOpnd = opnd->AsRegOpnd();
            StackSym *sym = regOpnd->m_sym;

            if (!IsCollectionPass())
            {
                // isTempLastUse is only used for string concat right now, so lets not mark it if it's not a string.
                // If it's upward exposed, it is not it's last use.
                if (regOpnd->m_isTempLastUse && (regOpnd->GetValueType().IsNotString() || this->currentBlock->upwardExposedUses->Test(sym->m_id) || sym->m_mayNotBeTempLastUse))
                {
                    regOpnd->m_isTempLastUse = false;
                }
                this->CollectCloneStrCandidate(opnd);
            }

            if (!this->ProcessSymUse(sym, true, regOpnd->GetIsJITOptimizedReg()) && this->DoSetDead())
            {
                regOpnd->SetIsDead();
            }

            if (IsCollectionPass())
            {
                break;
            }

            if (tag == Js::DeadStorePhase && regOpnd->IsArrayRegOpnd())
            {
                ProcessArrayRegOpndUse(currentInstr, regOpnd->AsArrayRegOpnd());
            }

            if (currentInstr->m_opcode == Js::OpCode::BailOnNotArray)
            {
                Assert(tag == Js::DeadStorePhase);

                const ValueType valueType(regOpnd->GetValueType());
                if(valueType.IsLikelyArrayOrObjectWithArray())
                {
                    currentBlock->noImplicitCallUses->Clear(sym->m_id);

                    // We are being conservative here to always check for missing value
                    // if any of them expect no missing value. That is because we don't know
                    // what set of sym is equivalent (copied) from the one we are testing for right now.
                    if(valueType.HasNoMissingValues() &&
                        !currentBlock->noImplicitCallNoMissingValuesUses->IsEmpty() &&
                        !IsPrePass())
                    {
                        // There is a use of this sym that requires this array to have no missing values, so this instruction
                        // needs to bail out if the array has missing values.
                        Assert(currentInstr->GetBailOutKind() == IR::BailOutOnNotArray ||
                               currentInstr->GetBailOutKind() == IR::BailOutOnNotNativeArray);
                        currentInstr->SetBailOutKind(currentInstr->GetBailOutKind() | IR::BailOutOnMissingValue);
                    }

                    currentBlock->noImplicitCallNoMissingValuesUses->Clear(sym->m_id);
                    currentBlock->noImplicitCallNativeArrayUses->Clear(sym->m_id);
                }
            }
        }
        break;
    case IR::OpndKindSym:
        {
            IR::SymOpnd *symOpnd = opnd->AsSymOpnd();
            Sym * sym = symOpnd->m_sym;
            if (!this->ProcessSymUse(sym, false, opnd->GetIsJITOptimizedReg()) && this->DoSetDead())
            {
                symOpnd->SetIsDead();
            }

            if (IsCollectionPass())
            {
                break;
            }

            if (sym->IsPropertySym())
            {
                // TODO: We don't have last use info for property sym
                // and we don't set the last use of the stacksym inside the property sym
                if (tag == Js::BackwardPhase)
                {
                    if (opnd->AsSymOpnd()->IsPropertySymOpnd())
                    {
                        this->globOpt->PreparePropertySymOpndForTypeCheckSeq(symOpnd->AsPropertySymOpnd(), this->currentInstr, this->currentBlock->loop);
                    }
                }

                if (this->DoMarkTempNumbersOnTempObjects())
                {
                    this->currentBlock->tempNumberTracker->ProcessPropertySymUse(symOpnd, this->currentInstr, this);
                }

                if (symOpnd->IsPropertySymOpnd())
                {
                    this->ProcessPropertySymOpndUse(symOpnd->AsPropertySymOpnd());
                }
            }
        }
        break;
    case IR::OpndKindIndir:
        {
            IR::IndirOpnd * indirOpnd = opnd->AsIndirOpnd();
            IR::RegOpnd * baseOpnd = indirOpnd->GetBaseOpnd();

            if (!this->ProcessSymUse(baseOpnd->m_sym, false, baseOpnd->GetIsJITOptimizedReg()) && this->DoSetDead())
            {
                baseOpnd->SetIsDead();
            }

            IR::RegOpnd * indexOpnd = indirOpnd->GetIndexOpnd();
            if (indexOpnd)
            {
                if (!this->ProcessSymUse(indexOpnd->m_sym, false, indexOpnd->GetIsJITOptimizedReg()) && this->DoSetDead())
                {
                    indexOpnd->SetIsDead();
                }
            }

            if(IsCollectionPass())
            {
                break;
            }

            if (this->DoMarkTempNumbersOnTempObjects())
            {
                this->currentBlock->tempNumberTracker->ProcessIndirUse(indirOpnd, currentInstr, this);
            }

            if(tag == Js::DeadStorePhase && baseOpnd->IsArrayRegOpnd())
            {
                ProcessArrayRegOpndUse(currentInstr, baseOpnd->AsArrayRegOpnd());
            }
        }
        break;
    }
}

bool
BackwardPass::ProcessPropertySymUse(PropertySym *propertySym)
{
    Assert(this->tag == Js::BackwardPhase);

    BasicBlock *block = this->currentBlock;

    bool isLive = !!block->upwardExposedFields->TestAndSet(propertySym->m_id);

    if (propertySym->m_propertyEquivSet)
    {
        block->upwardExposedFields->Or(propertySym->m_propertyEquivSet);
    }

    return isLive;
}

void
BackwardPass::MarkTemp(StackSym * sym)
{
    Assert(!IsCollectionPass());
    // Don't care about type specialized syms
    if (!sym->IsVar())
    {
        return;
    }

    BasicBlock * block = this->currentBlock;
    if (this->DoMarkTempNumbers())
    {
        Assert((block->loop != nullptr) == block->tempNumberTracker->HasTempTransferDependencies());
        block->tempNumberTracker->MarkTemp(sym, this);
    }
    if (this->DoMarkTempObjects())
    {
        Assert((block->loop != nullptr) == block->tempObjectTracker->HasTempTransferDependencies());
        block->tempObjectTracker->MarkTemp(sym, this);
    }
#if DBG
    if (this->DoMarkTempObjectVerify())
    {
        Assert((block->loop != nullptr) == block->tempObjectVerifyTracker->HasTempTransferDependencies());
        block->tempObjectVerifyTracker->MarkTemp(sym, this);
    }
#endif
}

void
BackwardPass::MarkTempProcessInstr(IR::Instr * instr)
{
    Assert(!IsCollectionPass());

    if (this->currentBlock->isDead)
    {
        return;
    }

    BasicBlock * block;
    block = this->currentBlock;
    if (this->DoMarkTempNumbers())
    {
        block->tempNumberTracker->ProcessInstr(instr, this);
    }

    if (this->DoMarkTempObjects())
    {
        block->tempObjectTracker->ProcessInstr(instr);
    }

#if DBG
    if (this->DoMarkTempObjectVerify())
    {
        block->tempObjectVerifyTracker->ProcessInstr(instr, this);
    }
#endif
}

#if DBG_DUMP
void
BackwardPass::DumpMarkTemp()
{
    Assert(!IsCollectionPass());

    BasicBlock * block = this->currentBlock;
    if (this->DoMarkTempNumbers())
    {
        block->tempNumberTracker->Dump();
    }
    if (this->DoMarkTempObjects())
    {
        block->tempObjectTracker->Dump();
    }
#if DBG
    if (this->DoMarkTempObjectVerify())
    {
        block->tempObjectVerifyTracker->Dump();
    }
#endif
}
#endif

void
BackwardPass::SetSymIsUsedOnlyInNumberIfLastUse(IR::Opnd *const opnd)
{
    StackSym *const stackSym = IR::RegOpnd::TryGetStackSym(opnd);
    if (stackSym && !currentBlock->upwardExposedUses->Test(stackSym->m_id))
    {
        symUsedOnlyForNumberBySymId->Set(stackSym->m_id);
    }
}

void
BackwardPass::SetSymIsNotUsedOnlyInNumber(IR::Opnd *const opnd)
{
    StackSym *const stackSym = IR::RegOpnd::TryGetStackSym(opnd);
    if (stackSym)
    {
        symUsedOnlyForNumberBySymId->Clear(stackSym->m_id);
    }
}

void
BackwardPass::SetSymIsUsedOnlyInBitOpsIfLastUse(IR::Opnd *const opnd)
{
    StackSym *const stackSym = IR::RegOpnd::TryGetStackSym(opnd);
    if (stackSym && !currentBlock->upwardExposedUses->Test(stackSym->m_id))
    {
        symUsedOnlyForBitOpsBySymId->Set(stackSym->m_id);
    }
}

void
BackwardPass::SetSymIsNotUsedOnlyInBitOps(IR::Opnd *const opnd)
{
    StackSym *const stackSym = IR::RegOpnd::TryGetStackSym(opnd);
    if (stackSym)
    {
        symUsedOnlyForBitOpsBySymId->Clear(stackSym->m_id);
    }
}

void
BackwardPass::TrackBitWiseOrNumberOp(IR::Instr *const instr)
{
    Assert(instr);
    const bool trackBitWiseop = DoTrackBitOpsOrNumber();
    const bool trackNumberop = trackBitWiseop;
    const Js::OpCode opcode = instr->m_opcode;
    StackSym *const dstSym = IR::RegOpnd::TryGetStackSym(instr->GetDst());
    if (!trackBitWiseop && !trackNumberop)
    {
        return;
    }

    if (!instr->IsRealInstr())
    {
        return;
    }

    if (dstSym)
    {
        // For a dst where the def is in this block, transfer the current info into the instruction
        if (trackBitWiseop && symUsedOnlyForBitOpsBySymId->TestAndClear(dstSym->m_id))
        {
            instr->dstIsAlwaysConvertedToInt32 = true;
        }
        if (trackNumberop && symUsedOnlyForNumberBySymId->TestAndClear(dstSym->m_id))
        {
            instr->dstIsAlwaysConvertedToNumber = true;
        }
    }

    // If the instruction can cause src values to escape the local scope, the srcs can't be optimized
    if (OpCodeAttr::NonTempNumberSources(opcode))
    {
        if (trackBitWiseop)
        {
            SetSymIsNotUsedOnlyInBitOps(instr->GetSrc1());
            SetSymIsNotUsedOnlyInBitOps(instr->GetSrc2());
        }
        if (trackNumberop)
        {
            SetSymIsNotUsedOnlyInNumber(instr->GetSrc1());
            SetSymIsNotUsedOnlyInNumber(instr->GetSrc2());
        }
        return;
    }

    if (trackBitWiseop)
    {
        switch (opcode)
        {
            // Instructions that can cause src values to escape the local scope have already been excluded

        case Js::OpCode::Not_A:
        case Js::OpCode::And_A:
        case Js::OpCode::Or_A:
        case Js::OpCode::Xor_A:
        case Js::OpCode::Shl_A:
        case Js::OpCode::Shr_A:

        case Js::OpCode::Not_I4:
        case Js::OpCode::And_I4:
        case Js::OpCode::Or_I4:
        case Js::OpCode::Xor_I4:
        case Js::OpCode::Shl_I4:
        case Js::OpCode::Shr_I4:
            // These instructions don't generate -0, and their behavior is the same for any src that is -0 or +0
            SetSymIsUsedOnlyInBitOpsIfLastUse(instr->GetSrc1());
            SetSymIsUsedOnlyInBitOpsIfLastUse(instr->GetSrc2());
            break;
        default:
            SetSymIsNotUsedOnlyInBitOps(instr->GetSrc1());
            SetSymIsNotUsedOnlyInBitOps(instr->GetSrc2());
            break;
        }
    }

    if (trackNumberop)
    {
        switch (opcode)
        {
            // Instructions that can cause src values to escape the local scope have already been excluded

        case Js::OpCode::Conv_Num:
        case Js::OpCode::Div_A:
        case Js::OpCode::Mul_A:
        case Js::OpCode::Sub_A:
        case Js::OpCode::Rem_A:
        case Js::OpCode::Incr_A:
        case Js::OpCode::Decr_A:
        case Js::OpCode::Neg_A:
        case Js::OpCode::Not_A:
        case Js::OpCode::ShrU_A:
        case Js::OpCode::ShrU_I4:
        case Js::OpCode::And_A:
        case Js::OpCode::Or_A:
        case Js::OpCode::Xor_A:
        case Js::OpCode::Shl_A:
        case Js::OpCode::Shr_A:
            // These instructions don't generate -0, and their behavior is the same for any src that is -0 or +0
            SetSymIsUsedOnlyInNumberIfLastUse(instr->GetSrc1());
            SetSymIsUsedOnlyInNumberIfLastUse(instr->GetSrc2());
            break;
        default:
            SetSymIsNotUsedOnlyInNumber(instr->GetSrc1());
            SetSymIsNotUsedOnlyInNumber(instr->GetSrc2());
            break;
        }
    }
}

void
BackwardPass::TrackIntUsage(IR::Instr *const instr)
{
    Assert(instr);

    const bool trackNegativeZero = DoTrackNegativeZero();
    const bool trackIntOverflow = DoTrackIntOverflow();
    const bool trackCompoundedIntOverflow = DoTrackCompoundedIntOverflow();
    const bool trackNon32BitOverflow = DoTrackNon32BitOverflow();

    if(!(trackNegativeZero || trackIntOverflow || trackCompoundedIntOverflow))
    {
        return;
    }

    const Js::OpCode opcode = instr->m_opcode;
    if(trackCompoundedIntOverflow && opcode == Js::OpCode::StatementBoundary && instr->AsPragmaInstr()->m_statementIndex == 0)
    {
        // Cannot bail out before the first statement boundary, so the range cannot extend beyond this instruction
        Assert(!instr->ignoreIntOverflowInRange);
        EndIntOverflowDoesNotMatterRange();
        return;
    }

    if(!instr->IsRealInstr())
    {
        return;
    }

    StackSym *const dstSym = IR::RegOpnd::TryGetStackSym(instr->GetDst());
    bool ignoreIntOverflowCandidate = false;
    if(dstSym)
    {
        // For a dst where the def is in this block, transfer the current info into the instruction
        if(trackNegativeZero && negativeZeroDoesNotMatterBySymId->TestAndClear(dstSym->m_id))
        {
            instr->ignoreNegativeZero = true;
            if(tag == Js::DeadStorePhase && instr->HasBailOutInfo())
            {
                IR::BailOutKind bailOutKind = instr->GetBailOutKind();
                if(bailOutKind & IR::BailOutOnNegativeZero)
                {
                    bailOutKind -= IR::BailOutOnNegativeZero;
                    if(bailOutKind)
                    {
                        instr->SetBailOutKind(bailOutKind);
                    }
                    else
                    {
                        instr->ClearBailOutInfo();
                        if(preOpBailOutInstrToProcess == instr)
                        {
                            preOpBailOutInstrToProcess = nullptr;
                        }
                    }
                }
            }
        }
        if(trackIntOverflow)
        {
            ignoreIntOverflowCandidate = !!intOverflowDoesNotMatterBySymId->TestAndClear(dstSym->m_id);
            if(trackCompoundedIntOverflow)
            {
                instr->ignoreIntOverflowInRange = !!intOverflowDoesNotMatterInRangeBySymId->TestAndClear(dstSym->m_id);
            }
        }
    }

    // If the instruction can cause src values to escape the local scope, the srcs can't be optimized
    if(OpCodeAttr::NonTempNumberSources(opcode))
    {
        if(trackNegativeZero)
        {
            SetNegativeZeroMatters(instr->GetSrc1());
            SetNegativeZeroMatters(instr->GetSrc2());
        }
        if(trackIntOverflow)
        {
            SetIntOverflowMatters(instr->GetSrc1());
            SetIntOverflowMatters(instr->GetSrc2());
            if(trackCompoundedIntOverflow)
            {
                instr->ignoreIntOverflowInRange = false;
                SetIntOverflowMattersInRange(instr->GetSrc1());
                SetIntOverflowMattersInRange(instr->GetSrc2());
                EndIntOverflowDoesNotMatterRange();
            }
        }
        return;
    }

    // -0 tracking

    if(trackNegativeZero)
    {
        switch(opcode)
        {
            // Instructions that can cause src values to escape the local scope have already been excluded

            case Js::OpCode::FromVar:
            case Js::OpCode::Conv_Prim:
                Assert(dstSym);
                Assert(instr->GetSrc1());
                Assert(!instr->GetSrc2());

                if(instr->GetDst()->IsInt32())
                {
                    // Conversion to int32 that is either explicit, or has a bailout check ensuring that it's an int value
                    SetNegativeZeroDoesNotMatterIfLastUse(instr->GetSrc1());
                    break;
                }
                // fall-through

            default:
                if(dstSym && !instr->ignoreNegativeZero)
                {
                    // -0 matters for dst, so -0 also matters for srcs
                    SetNegativeZeroMatters(instr->GetSrc1());
                    SetNegativeZeroMatters(instr->GetSrc2());
                    break;
                }
                if(opcode == Js::OpCode::Div_A || opcode == Js::OpCode::Div_I4)
                {
                    // src1 is being divided by src2, so -0 matters for src2
                    SetNegativeZeroDoesNotMatterIfLastUse(instr->GetSrc1());
                    SetNegativeZeroMatters(instr->GetSrc2());
                    break;
                }
                // fall-through

            case Js::OpCode::Incr_A:
            case Js::OpCode::Decr_A:
                // Adding 1 to something or subtracting 1 from something does not generate -0

            case Js::OpCode::Not_A:
            case Js::OpCode::And_A:
            case Js::OpCode::Or_A:
            case Js::OpCode::Xor_A:
            case Js::OpCode::Shl_A:
            case Js::OpCode::Shr_A:
            case Js::OpCode::ShrU_A:

            case Js::OpCode::Not_I4:
            case Js::OpCode::And_I4:
            case Js::OpCode::Or_I4:
            case Js::OpCode::Xor_I4:
            case Js::OpCode::Shl_I4:
            case Js::OpCode::Shr_I4:
            case Js::OpCode::ShrU_I4:

            case Js::OpCode::Conv_Str:
            case Js::OpCode::Coerse_Str:
            case Js::OpCode::Coerse_Regex:
            case Js::OpCode::Coerse_StrOrRegex:
            case Js::OpCode::Conv_PrimStr:

            case Js::OpCode::Add_Ptr:
                // These instructions don't generate -0, and their behavior is the same for any src that is -0 or +0
                SetNegativeZeroDoesNotMatterIfLastUse(instr->GetSrc1());
                SetNegativeZeroDoesNotMatterIfLastUse(instr->GetSrc2());
                break;

            case Js::OpCode::Add_I4:
                Assert(dstSym);
                Assert(instr->GetSrc1());
                Assert(instr->GetSrc1()->IsRegOpnd() || instr->GetSrc1()->IsIntConstOpnd());
                Assert(instr->GetSrc2());
                Assert(instr->GetSrc2()->IsRegOpnd() || instr->GetSrc2()->IsIntConstOpnd());

                if(instr->ignoreNegativeZero ||
                    !(instr->GetSrc1()->IsRegOpnd() && instr->GetSrc1()->AsRegOpnd()->m_wasNegativeZeroPreventedByBailout) ||
                    !(instr->GetSrc2()->IsRegOpnd() && instr->GetSrc2()->AsRegOpnd()->m_wasNegativeZeroPreventedByBailout))
                {
                    // -0 does not matter for dst, or this instruction does not generate -0 since one of the srcs is not -0
                    // (regardless of -0 bailout checks)
                    SetNegativeZeroDoesNotMatterIfLastUse(instr->GetSrc1());
                    SetNegativeZeroDoesNotMatterIfLastUse(instr->GetSrc2());
                    break;
                }

                // -0 + -0 == -0. As long as one src is guaranteed to not be -0, -0 does not matter for the other src. Pick a
                // src for which to ignore negative zero, based on which sym is last-use. If both syms are last-use, src2 is
                // picked arbitrarily.
                if(instr->GetSrc2()->IsRegOpnd() &&
                    !currentBlock->upwardExposedUses->Test(instr->GetSrc2()->AsRegOpnd()->m_sym->m_id))
                {
                    SetNegativeZeroDoesNotMatterIfLastUse(instr->GetSrc2());
                    SetNegativeZeroMatters(instr->GetSrc1());
                }
                else
                {
                    SetNegativeZeroDoesNotMatterIfLastUse(instr->GetSrc1());
                    SetNegativeZeroMatters(instr->GetSrc2());
                }
                break;

            case Js::OpCode::Add_A:
                Assert(dstSym);
                Assert(instr->GetSrc1());
                Assert(instr->GetSrc1()->IsRegOpnd() || instr->GetSrc1()->IsAddrOpnd());
                Assert(instr->GetSrc2());
                Assert(instr->GetSrc2()->IsRegOpnd() || instr->GetSrc2()->IsAddrOpnd());

                if(instr->ignoreNegativeZero || instr->GetSrc1()->IsAddrOpnd() || instr->GetSrc2()->IsAddrOpnd())
                {
                    // -0 does not matter for dst, or this instruction does not generate -0 since one of the srcs is not -0
                    SetNegativeZeroDoesNotMatterIfLastUse(instr->GetSrc1());
                    SetNegativeZeroDoesNotMatterIfLastUse(instr->GetSrc2());
                    break;
                }

                SetNegativeZeroMatters(instr->GetSrc1());
                SetNegativeZeroMatters(instr->GetSrc2());
                break;

            case Js::OpCode::Sub_I4:
                Assert(dstSym);
                Assert(instr->GetSrc1());
                Assert(instr->GetSrc1()->IsRegOpnd() || instr->GetSrc1()->IsIntConstOpnd());
                Assert(instr->GetSrc2());
                Assert(instr->GetSrc2()->IsRegOpnd() || instr->GetSrc2()->IsIntConstOpnd());

                if(instr->ignoreNegativeZero ||
                    !(instr->GetSrc1()->IsRegOpnd() && instr->GetSrc1()->AsRegOpnd()->m_wasNegativeZeroPreventedByBailout) ||
                    instr->GetSrc2()->IsIntConstOpnd() && instr->GetSrc2()->AsIntConstOpnd()->GetValue() != 0)
                {
                    // At least one of the following is true:
                    //     - -0 does not matter for dst
                    //     - Src1 is not -0 (regardless of -0 bailout checks), and so this instruction cannot generate -0
                    //     - Src2 is a nonzero int constant, and so this instruction cannot generate -0
                    SetNegativeZeroDoesNotMatterIfLastUse(instr->GetSrc1());
                    SetNegativeZeroDoesNotMatterIfLastUse(instr->GetSrc2());
                    break;
                }
                goto NegativeZero_Sub_Default;

            case Js::OpCode::Sub_A:
                Assert(dstSym);
                Assert(instr->GetSrc1());
                Assert(instr->GetSrc1()->IsRegOpnd() || instr->GetSrc1()->IsAddrOpnd());
                Assert(instr->GetSrc2());
                Assert(instr->GetSrc2()->IsRegOpnd() || instr->GetSrc2()->IsAddrOpnd() || instr->GetSrc2()->IsIntConstOpnd());

                if(instr->ignoreNegativeZero ||
                    instr->GetSrc1()->IsAddrOpnd() ||
                    (
                        instr->GetSrc2()->IsAddrOpnd() &&
                        instr->GetSrc2()->AsAddrOpnd()->IsVar() &&
                        Js::TaggedInt::ToInt32(instr->GetSrc2()->AsAddrOpnd()->m_address) != 0
                    ))
                {
                    // At least one of the following is true:
                    //     - -0 does not matter for dst
                    //     - Src1 is not -0, and so this instruction cannot generate -0
                    //     - Src2 is a nonzero tagged int constant, and so this instruction cannot generate -0
                    SetNegativeZeroDoesNotMatterIfLastUse(instr->GetSrc1());
                    SetNegativeZeroDoesNotMatterIfLastUse(instr->GetSrc2());
                    break;
                }
                // fall-through

            NegativeZero_Sub_Default:
                // -0 - 0 == -0. As long as src1 is guaranteed to not be -0, -0 does not matter for src2.
                SetNegativeZeroMatters(instr->GetSrc1());
                SetNegativeZeroDoesNotMatterIfLastUse(instr->GetSrc2());
                break;

            case Js::OpCode::BrEq_I4:
            case Js::OpCode::BrTrue_I4:
            case Js::OpCode::BrFalse_I4:
            case Js::OpCode::BrGe_I4:
            case Js::OpCode::BrUnGe_I4:
            case Js::OpCode::BrGt_I4:
            case Js::OpCode::BrUnGt_I4:
            case Js::OpCode::BrLt_I4:
            case Js::OpCode::BrUnLt_I4:
            case Js::OpCode::BrLe_I4:
            case Js::OpCode::BrUnLe_I4:
            case Js::OpCode::BrNeq_I4:
                // Int-specialized branches may prove that one of the src must be zero purely based on the int range, in which
                // case they rely on prior -0 bailouts to guarantee that the src cannot be -0. So, consider that -0 matters for
                // the srcs.

                // fall-through

            case Js::OpCode::InlineMathAtan2:
                // Atan(y,x) - signs of y, x is used to determine the quadrant of the result
                SetNegativeZeroMatters(instr->GetSrc1());
                SetNegativeZeroMatters(instr->GetSrc2());
                break;

            case Js::OpCode::Expo_A:
            case Js::OpCode::InlineMathPow:
                // Negative zero matters for src1
                //   Pow( 0, <neg>) is  Infinity
                //   Pow(-0, <neg>) is -Infinity
                SetNegativeZeroMatters(instr->GetSrc1());
                SetNegativeZeroDoesNotMatterIfLastUse(instr->GetSrc2());
                break;

            case Js::OpCode::LdElemI_A:
                // There is an implicit ToString on the index operand, which doesn't differentiate -0 from +0
                SetNegativeZeroDoesNotMatterIfLastUse(instr->GetSrc1()->AsIndirOpnd()->GetIndexOpnd());
                break;

            case Js::OpCode::StElemI_A:
            case Js::OpCode::StElemI_A_Strict:
                // There is an implicit ToString on the index operand, which doesn't differentiate -0 from +0
                SetNegativeZeroDoesNotMatterIfLastUse(instr->GetDst()->AsIndirOpnd()->GetIndexOpnd());
                break;
        }
    }

    // Int overflow tracking

    if(!trackIntOverflow)
    {
        return;
    }

    switch(opcode)
    {
        // Instructions that can cause src values to escape the local scope have already been excluded

        default:
            // Unlike the -0 tracking, we use an inclusion list of op-codes for overflow tracking rather than an exclusion list.
            // Assume for any instructions other than those listed above, that int-overflowed values in the srcs are
            // insufficient.
            ignoreIntOverflowCandidate = false;
            // fall-through
        case Js::OpCode::Incr_A:
        case Js::OpCode::Decr_A:
        case Js::OpCode::Add_A:
        case Js::OpCode::Sub_A:
            // The sources are not guaranteed to be converted to int32. Let the compounded int overflow tracking handle this.
            SetIntOverflowMatters(instr->GetSrc1());
            SetIntOverflowMatters(instr->GetSrc2());
            break;

        case Js::OpCode::Mul_A:
            if (trackNon32BitOverflow)
            {
                if (ignoreIntOverflowCandidate)
                    instr->ignoreOverflowBitCount = 53;
            }
            else
            {
                ignoreIntOverflowCandidate = false;
            }
            SetIntOverflowMatters(instr->GetSrc1());
            SetIntOverflowMatters(instr->GetSrc2());
            break;

        case Js::OpCode::Neg_A:
        case Js::OpCode::Ld_A:
        case Js::OpCode::Conv_Num:
        case Js::OpCode::ShrU_A:
            if(!ignoreIntOverflowCandidate)
            {
                // Int overflow matters for dst, so int overflow also matters for srcs
                SetIntOverflowMatters(instr->GetSrc1());
                SetIntOverflowMatters(instr->GetSrc2());
                break;
            }
            // fall-through

        case Js::OpCode::Not_A:
        case Js::OpCode::And_A:
        case Js::OpCode::Or_A:
        case Js::OpCode::Xor_A:
        case Js::OpCode::Shl_A:
        case Js::OpCode::Shr_A:
            // These instructions convert their srcs to int32s, and hence don't care about int-overflowed values in the srcs (as
            // long as the overflowed values did not overflow the 53 bits that 'double' values have to precisely represent
            // ints). ShrU_A is not included here because it converts its srcs to uint32 rather than int32, so it would make a
            // difference if the srcs have int32-overflowed values.
            SetIntOverflowDoesNotMatterIfLastUse(instr->GetSrc1());
            SetIntOverflowDoesNotMatterIfLastUse(instr->GetSrc2());
            break;
    }

    if(ignoreIntOverflowCandidate)
    {
        instr->ignoreIntOverflow = true;
    }

    // Compounded int overflow tracking

    if(!trackCompoundedIntOverflow)
    {
        return;
    }

    if(instr->GetByteCodeOffset() == Js::Constants::NoByteCodeOffset)
    {
        // The forward pass may need to insert conversions with bailouts before the first instruction in the range. Since this
        // instruction does not have a valid byte code offset for bailout purposes, end the current range now.
        instr->ignoreIntOverflowInRange = false;
        SetIntOverflowMattersInRange(instr->GetSrc1());
        SetIntOverflowMattersInRange(instr->GetSrc2());
        EndIntOverflowDoesNotMatterRange();
        return;
    }

    if(ignoreIntOverflowCandidate)
    {
        instr->ignoreIntOverflowInRange = true;
        if(dstSym)
        {
            dstSym->scratch.globOpt.numCompoundedAddSubUses = 0;
        }
    }

    bool lossy = false;
    switch(opcode)
    {
        // Instructions that can cause src values to escape the local scope have already been excluded

        case Js::OpCode::Incr_A:
        case Js::OpCode::Decr_A:
        case Js::OpCode::Add_A:
        case Js::OpCode::Sub_A:
        {
            if(!instr->ignoreIntOverflowInRange)
            {
                // Int overflow matters for dst, so int overflow also matters for srcs
                SetIntOverflowMattersInRange(instr->GetSrc1());
                SetIntOverflowMattersInRange(instr->GetSrc2());
                break;
            }
            AnalysisAssert(dstSym);

            // The number of compounded add/sub uses of each src is at least the number of compounded add/sub uses of the dst,
            // + 1 for the current instruction
            Assert(dstSym->scratch.globOpt.numCompoundedAddSubUses >= 0);
            Assert(dstSym->scratch.globOpt.numCompoundedAddSubUses <= MaxCompoundedUsesInAddSubForIgnoringIntOverflow);
            const int addSubUses = dstSym->scratch.globOpt.numCompoundedAddSubUses + 1;
            if(addSubUses > MaxCompoundedUsesInAddSubForIgnoringIntOverflow)
            {
                // There are too many compounded add/sub uses of the srcs. There is a possibility that combined, the number
                // eventually overflows the 53 bits that 'double' values have to precisely represent ints
                instr->ignoreIntOverflowInRange = false;
                SetIntOverflowMattersInRange(instr->GetSrc1());
                SetIntOverflowMattersInRange(instr->GetSrc2());
                break;
            }

            TransferCompoundedAddSubUsesToSrcs(instr, addSubUses);
            break;
        }

        case Js::OpCode::Neg_A:
        case Js::OpCode::Ld_A:
        case Js::OpCode::Conv_Num:
        case Js::OpCode::ShrU_A:
        {
            if(!instr->ignoreIntOverflowInRange)
            {
                // Int overflow matters for dst, so int overflow also matters for srcs
                SetIntOverflowMattersInRange(instr->GetSrc1());
                SetIntOverflowMattersInRange(instr->GetSrc2());
                break;
            }
            AnalysisAssert(dstSym);

            TransferCompoundedAddSubUsesToSrcs(instr, dstSym->scratch.globOpt.numCompoundedAddSubUses);
            lossy = opcode == Js::OpCode::ShrU_A;
            break;
        }

        case Js::OpCode::Not_A:
        case Js::OpCode::And_A:
        case Js::OpCode::Or_A:
        case Js::OpCode::Xor_A:
        case Js::OpCode::Shl_A:
        case Js::OpCode::Shr_A:
            // These instructions convert their srcs to int32s, and hence don't care about int-overflowed values in the srcs (as
            // long as the overflowed values did not overflow the 53 bits that 'double' values have to precisely represent
            // ints). ShrU_A is not included here because it converts its srcs to uint32 rather than int32, so it would make a
            // difference if the srcs have int32-overflowed values.
            instr->ignoreIntOverflowInRange = true;
            lossy = true;
            SetIntOverflowDoesNotMatterInRangeIfLastUse(instr->GetSrc1(), 0);
            SetIntOverflowDoesNotMatterInRangeIfLastUse(instr->GetSrc2(), 0);
            break;

        case Js::OpCode::LdSlotArr:
        case Js::OpCode::LdSlot:
        {
            Assert(dstSym);
            Assert(!instr->GetSrc2()); // at the moment, this list contains only unary operations

            if(intOverflowCurrentlyMattersInRange)
            {
                // These instructions will not begin a range, so just return. They don't begin a range because their initial
                // value may not be available until after the instruction is processed in the forward pass.
                Assert(!instr->ignoreIntOverflowInRange);
                return;
            }
            Assert(currentBlock->intOverflowDoesNotMatterRange);

            // Int overflow does not matter for dst, so the srcs need to be tracked as inputs into the region of
            // instructions where int overflow does not matter. Since these instructions will not begin or end a range, they
            // are tracked in separate candidates bit-vectors and once we have confirmed that they don't begin the range,
            // they will be transferred to 'SymsRequiredToBe[Lossy]Int'. Furthermore, once this instruction is included in
            // the range, its dst sym has to be removed. Since this instructions may not be included in the range, add the
            // dst sym to the candidates bit-vectors. If they are included, the process of transferring will remove the dst
            // syms and add the src syms.

            // Remove the dst using the candidate bit-vectors
            Assert(
                !instr->ignoreIntOverflowInRange ||
                currentBlock->intOverflowDoesNotMatterRange->SymsRequiredToBeInt()->Test(dstSym->m_id));
            if(instr->ignoreIntOverflowInRange ||
                currentBlock->intOverflowDoesNotMatterRange->SymsRequiredToBeInt()->Test(dstSym->m_id))
            {
                candidateSymsRequiredToBeInt->Set(dstSym->m_id);
                if(currentBlock->intOverflowDoesNotMatterRange->SymsRequiredToBeLossyInt()->Test(dstSym->m_id))
                {
                    candidateSymsRequiredToBeLossyInt->Set(dstSym->m_id);
                }
            }

            if(!instr->ignoreIntOverflowInRange)
            {
                // These instructions will not end a range, so just return. They may be included in the middle of a range, but
                // since int overflow matters for the dst, the src does not need to be counted as an input into the range.
                return;
            }
            instr->ignoreIntOverflowInRange = false;

            // Add the src using the candidate bit-vectors. The src property sym may already be included in the range or as
            // a candidate. The xor of the final bit-vector with the candidate is the set of syms required to be int,
            // assuming all instructions up to and not including this one are included in the range.
            const SymID srcSymId = instr->GetSrc1()->AsSymOpnd()->m_sym->m_id;
            const bool srcIncluded =
                !!currentBlock->intOverflowDoesNotMatterRange->SymsRequiredToBeInt()->Test(srcSymId) ^
                !!candidateSymsRequiredToBeInt->Test(srcSymId);
            const bool srcIncludedAsLossy =
                srcIncluded &&
                !!currentBlock->intOverflowDoesNotMatterRange->SymsRequiredToBeLossyInt()->Test(srcSymId) ^
                !!candidateSymsRequiredToBeLossyInt->Test(srcSymId);
            const bool srcNeedsToBeLossless =
                !currentBlock->intOverflowDoesNotMatterRange->SymsRequiredToBeLossyInt()->Test(dstSym->m_id) ||
                srcIncluded && !srcIncludedAsLossy;
            if(srcIncluded)
            {
                if(srcIncludedAsLossy && srcNeedsToBeLossless)
                {
                    candidateSymsRequiredToBeLossyInt->Compliment(srcSymId);
                }
            }
            else
            {
                candidateSymsRequiredToBeInt->Compliment(srcSymId);
                if(!srcNeedsToBeLossless)
                {
                    candidateSymsRequiredToBeLossyInt->Compliment(srcSymId);
                }
            }

            // These instructions will not end a range, so just return. They may be included in the middle of a range, and the
            // src has been included as a candidate input into the range.
            return;
        }

        case Js::OpCode::Mul_A:
            if (trackNon32BitOverflow)
            {
                // MULs will always be at the start of a range. Either included in the range if int32 overflow is ignored, or excluded if int32 overflow matters. Even if int32 can be ignored, MULs can still bailout on 53-bit.
                // That's why it cannot be in the middle of a range.
                if (instr->ignoreIntOverflowInRange)
                {
                    AnalysisAssert(dstSym);
                    Assert(dstSym->scratch.globOpt.numCompoundedAddSubUses >= 0);
                    Assert(dstSym->scratch.globOpt.numCompoundedAddSubUses <= MaxCompoundedUsesInAddSubForIgnoringIntOverflow);
                    instr->ignoreOverflowBitCount = (uint8) (53 - dstSym->scratch.globOpt.numCompoundedAddSubUses);

                    // We have the max number of compounded adds/subs. 32-bit overflow cannot be ignored.
                    if (instr->ignoreOverflowBitCount == 32)
                    {
                        instr->ignoreIntOverflowInRange = false;
                    }
                }

                SetIntOverflowMattersInRange(instr->GetSrc1());
                SetIntOverflowMattersInRange(instr->GetSrc2());
                break;
            }
            // fall-through

        default:
            // Unlike the -0 tracking, we use an inclusion list of op-codes for overflow tracking rather than an exclusion list.
            // Assume for any instructions other than those listed above, that int-overflowed values in the srcs are
            // insufficient.
            instr->ignoreIntOverflowInRange = false;
            SetIntOverflowMattersInRange(instr->GetSrc1());
            SetIntOverflowMattersInRange(instr->GetSrc2());
            break;
    }

    if(!instr->ignoreIntOverflowInRange)
    {
        EndIntOverflowDoesNotMatterRange();
        return;
    }

    if(intOverflowCurrentlyMattersInRange)
    {
        // This is the last instruction in a new range of instructions where int overflow does not matter
        intOverflowCurrentlyMattersInRange = false;
        IR::Instr *const boundaryInstr = IR::PragmaInstr::New(Js::OpCode::NoIntOverflowBoundary, 0, instr->m_func);
        boundaryInstr->SetByteCodeOffset(instr);
        currentBlock->InsertInstrAfter(boundaryInstr, instr);
        currentBlock->intOverflowDoesNotMatterRange =
            IntOverflowDoesNotMatterRange::New(
                globOpt->alloc,
                instr,
                boundaryInstr,
                currentBlock->intOverflowDoesNotMatterRange);
    }
    else
    {
        Assert(currentBlock->intOverflowDoesNotMatterRange);

        // Extend the current range of instructions where int overflow does not matter, to include this instruction. We also need to
        // include the tracked syms for instructions that have not yet been included in the range, which are tracked in the range's
        // bit-vector. 'SymsRequiredToBeInt' will contain both the dst and src syms of instructions not yet included in the range;
        // the xor will remove the dst syms and add the src syms.
        currentBlock->intOverflowDoesNotMatterRange->SymsRequiredToBeInt()->Xor(candidateSymsRequiredToBeInt);
        currentBlock->intOverflowDoesNotMatterRange->SymsRequiredToBeLossyInt()->Xor(candidateSymsRequiredToBeLossyInt);
        candidateSymsRequiredToBeInt->ClearAll();
        candidateSymsRequiredToBeLossyInt->ClearAll();
        currentBlock->intOverflowDoesNotMatterRange->SetFirstInstr(instr);
    }

    // Track syms that are inputs into the range based on the current instruction, which was just added to the range. The dst
    // sym is obtaining a new value so it isn't required to be an int at the start of the range, but the srcs are.
    if(dstSym)
    {
        currentBlock->intOverflowDoesNotMatterRange->SymsRequiredToBeInt()->Clear(dstSym->m_id);
        currentBlock->intOverflowDoesNotMatterRange->SymsRequiredToBeLossyInt()->Clear(dstSym->m_id);
    }
    IR::Opnd *const srcs[] = { instr->GetSrc1(), instr->GetSrc2() };
    for(int i = 0; i < sizeof(srcs) / sizeof(srcs[0]) && srcs[i]; ++i)
    {
        StackSym *srcSym = IR::RegOpnd::TryGetStackSym(srcs[i]);
        if(!srcSym)
        {
            continue;
        }

        if(currentBlock->intOverflowDoesNotMatterRange->SymsRequiredToBeInt()->TestAndSet(srcSym->m_id))
        {
            if(!lossy)
            {
                currentBlock->intOverflowDoesNotMatterRange->SymsRequiredToBeLossyInt()->Clear(srcSym->m_id);
            }
        }
        else if(lossy)
        {
            currentBlock->intOverflowDoesNotMatterRange->SymsRequiredToBeLossyInt()->Set(srcSym->m_id);
        }
    }

    // If the last instruction included in the range is a MUL, we have to end the range.
    // MULs with ignoreIntOverflow can still bailout on 53-bit overflow, so they cannot be in the middle of a range
    if (trackNon32BitOverflow && instr->m_opcode == Js::OpCode::Mul_A)
    {
        // range would have ended already if int32 overflow matters
        Assert(instr->ignoreIntOverflowInRange && instr->ignoreOverflowBitCount != 32);
        EndIntOverflowDoesNotMatterRange();
    }
}

void
BackwardPass::SetNegativeZeroDoesNotMatterIfLastUse(IR::Opnd *const opnd)
{
    StackSym *const stackSym = IR::RegOpnd::TryGetStackSym(opnd);
    if(stackSym && !currentBlock->upwardExposedUses->Test(stackSym->m_id))
    {
        negativeZeroDoesNotMatterBySymId->Set(stackSym->m_id);
    }
}

void
BackwardPass::SetNegativeZeroMatters(IR::Opnd *const opnd)
{
    StackSym *const stackSym = IR::RegOpnd::TryGetStackSym(opnd);
    if(stackSym)
    {
        negativeZeroDoesNotMatterBySymId->Clear(stackSym->m_id);
    }
}

void
BackwardPass::SetIntOverflowDoesNotMatterIfLastUse(IR::Opnd *const opnd)
{
    StackSym *const stackSym = IR::RegOpnd::TryGetStackSym(opnd);
    if(stackSym && !currentBlock->upwardExposedUses->Test(stackSym->m_id))
    {
        intOverflowDoesNotMatterBySymId->Set(stackSym->m_id);
    }
}

void
BackwardPass::SetIntOverflowMatters(IR::Opnd *const opnd)
{
    StackSym *const stackSym = IR::RegOpnd::TryGetStackSym(opnd);
    if(stackSym)
    {
        intOverflowDoesNotMatterBySymId->Clear(stackSym->m_id);
    }
}

bool
BackwardPass::SetIntOverflowDoesNotMatterInRangeIfLastUse(IR::Opnd *const opnd, const int addSubUses)
{
    StackSym *const stackSym = IR::RegOpnd::TryGetStackSym(opnd);
    return stackSym && SetIntOverflowDoesNotMatterInRangeIfLastUse(stackSym, addSubUses);
}

bool
BackwardPass::SetIntOverflowDoesNotMatterInRangeIfLastUse(StackSym *const stackSym, const int addSubUses)
{
    Assert(stackSym);
    Assert(addSubUses >= 0);
    Assert(addSubUses <= MaxCompoundedUsesInAddSubForIgnoringIntOverflow);

    if(currentBlock->upwardExposedUses->Test(stackSym->m_id))
    {
        return false;
    }

    intOverflowDoesNotMatterInRangeBySymId->Set(stackSym->m_id);
    stackSym->scratch.globOpt.numCompoundedAddSubUses = addSubUses;
    return true;
}

void
BackwardPass::SetIntOverflowMattersInRange(IR::Opnd *const opnd)
{
    StackSym *const stackSym = IR::RegOpnd::TryGetStackSym(opnd);
    if(stackSym)
    {
        intOverflowDoesNotMatterInRangeBySymId->Clear(stackSym->m_id);
    }
}

void
BackwardPass::TransferCompoundedAddSubUsesToSrcs(IR::Instr *const instr, const int addSubUses)
{
    Assert(instr);
    Assert(addSubUses >= 0);
    Assert(addSubUses <= MaxCompoundedUsesInAddSubForIgnoringIntOverflow);

    IR::Opnd *const srcs[] = { instr->GetSrc1(), instr->GetSrc2() };
    for(int i = 0; i < _countof(srcs) && srcs[i]; ++i)
    {
        StackSym *const srcSym = IR::RegOpnd::TryGetStackSym(srcs[i]);
        if(!srcSym)
        {
            // Int overflow tracking is only done for StackSyms in RegOpnds. Int overflow matters for the src, so it is
            // guaranteed to be in the int range at this point if the instruction is int-specialized.
            continue;
        }

        Assert(srcSym->scratch.globOpt.numCompoundedAddSubUses >= 0);
        Assert(srcSym->scratch.globOpt.numCompoundedAddSubUses <= MaxCompoundedUsesInAddSubForIgnoringIntOverflow);

        if(SetIntOverflowDoesNotMatterInRangeIfLastUse(srcSym, addSubUses))
        {
            // This is the last use of the src
            continue;
        }

        if(intOverflowDoesNotMatterInRangeBySymId->Test(srcSym->m_id))
        {
            // Since an src may be compounded through different chains of add/sub instructions, the greater number must be
            // preserved
            srcSym->scratch.globOpt.numCompoundedAddSubUses =
                max(srcSym->scratch.globOpt.numCompoundedAddSubUses, addSubUses);
        }
        else
        {
            // Int overflow matters for the src, so it is guaranteed to be in the int range at this point if the instruction is
            // int-specialized
        }
    }
}

void
BackwardPass::EndIntOverflowDoesNotMatterRange()
{
    if(intOverflowCurrentlyMattersInRange)
    {
        return;
    }
    intOverflowCurrentlyMattersInRange = true;

    if(currentBlock->intOverflowDoesNotMatterRange->FirstInstr()->m_next ==
        currentBlock->intOverflowDoesNotMatterRange->LastInstr())
    {
        // Don't need a range for a single-instruction range
        IntOverflowDoesNotMatterRange *const rangeToDelete = currentBlock->intOverflowDoesNotMatterRange;
        currentBlock->intOverflowDoesNotMatterRange = currentBlock->intOverflowDoesNotMatterRange->Next();
        currentBlock->RemoveInstr(rangeToDelete->LastInstr());
        rangeToDelete->Delete(globOpt->alloc);
    }
    else
    {
        // End the current range of instructions where int overflow does not matter
        IR::Instr *const boundaryInstr =
            IR::PragmaInstr::New(
                Js::OpCode::NoIntOverflowBoundary,
                0,
                currentBlock->intOverflowDoesNotMatterRange->FirstInstr()->m_func);
        boundaryInstr->SetByteCodeOffset(currentBlock->intOverflowDoesNotMatterRange->FirstInstr());
        currentBlock->InsertInstrBefore(boundaryInstr, currentBlock->intOverflowDoesNotMatterRange->FirstInstr());
        currentBlock->intOverflowDoesNotMatterRange->SetFirstInstr(boundaryInstr);

#if DBG_DUMP
        if(PHASE_TRACE(Js::TrackCompoundedIntOverflowPhase, func->GetJnFunction()))
        {
            wchar_t debugStringBuffer[MAX_FUNCTION_BODY_DEBUG_STRING_SIZE];
            Output::Print(
                L"TrackCompoundedIntOverflow - Top function: %s (%s), Phase: %s, Block: %u\n",
                func->GetJnFunction()->GetDisplayName(),
                func->GetJnFunction()->GetDebugNumberSet(debugStringBuffer),
                Js::PhaseNames[Js::BackwardPhase],
                currentBlock->GetBlockNum());
            Output::Print(L"    Input syms to be int-specialized (lossless): ");
            candidateSymsRequiredToBeInt->Minus(
                currentBlock->intOverflowDoesNotMatterRange->SymsRequiredToBeInt(),
                currentBlock->intOverflowDoesNotMatterRange->SymsRequiredToBeLossyInt()); // candidate bit-vectors are cleared below anyway
            candidateSymsRequiredToBeInt->Dump();
            Output::Print(L"    Input syms to be converted to int (lossy):   ");
            currentBlock->intOverflowDoesNotMatterRange->SymsRequiredToBeLossyInt()->Dump();
            Output::Print(L"    First instr: ");
            currentBlock->intOverflowDoesNotMatterRange->FirstInstr()->m_next->Dump();
            Output::Flush();
        }
#endif
    }

    // Reset candidates for the next range
    candidateSymsRequiredToBeInt->ClearAll();
    candidateSymsRequiredToBeLossyInt->ClearAll();

    // Syms are not tracked across different ranges of instructions where int overflow does not matter, since instructions
    // between the ranges may bail out. The value of the dst of an int operation where overflow is ignored is incorrect until
    // the last use of that sym is converted to int. If the int operation and the last use of the sym are in different ranges
    // and an instruction between the ranges bails out, other inputs into the second range are no longer guaranteed to be ints,
    // so the incorrect value of the sym may be used in non-int operations.
    intOverflowDoesNotMatterInRangeBySymId->ClearAll();
}

void
BackwardPass::TrackFloatSymEquivalence(IR::Instr *const instr)
{
    /*
    This function determines sets of float-specialized syms where any two syms in a set may have the same value number at some
    point in the function. Conversely, if two float-specialized syms are not in the same set, it guarantees that those two syms
    will never have the same value number. These sets are referred to as equivalence classes here.

    The equivalence class for a sym is used to determine whether a bailout FromVar generating a float value for the sym needs to
    bail out on any non-number value. For instance, for syms s1 and s5 in an equivalence class (say we have s5 = s1 at some
    point), if there's a FromVar that generates a float value for s1 but only bails out on strings or non-primitives, and s5 is
    returned from the function, it has to be ensured that s5 is not converted to Var. If the source of the FromVar was null, the
    FromVar would not have bailed out, and s1 and s5 would have the value +0. When s5 is returned, we need to return null and
    not +0, so the equivalence class is used to determine that since s5 requires a bailout on any non-number value, so does s1.

    The tracking is very conservative because the bit that says "I require bailout on any non-number value" is on the sym itself
    (referred to as non-number bailout bit below).

    Data:
    - BackwardPass::floatSymEquivalenceMap
        - hash table mapping a float sym ID to its equivalence class
    - FloatSymEquivalenceClass
        - bit vector of float sym IDs that are in the equivalence class
        - one non-number bailout bit for all syms in the equivalence class

    Algorithm:
    - In a loop prepass or when not in loop:
        - For a float sym transfer (s0.f = s1.f), add both syms to an equivalence class (set the syms in a bit vector)
            - If either sym requires bailout on any non-number value, set the equivalence class' non-number bailout bit
        - If one of the syms is already in an equivalence class, merge the two equivalence classes by OR'ing the two bit vectors
          and the non-number bailout bit.
        - Note that for functions with a loop, dependency tracking is done using equivalence classes and that information is not
          transferred back into each sym's non-number bailout bit
    - In a loop non-prepass or when not in loop, for a FromVar instruction that requires bailout only on strings and
      non-primitives:
        - If the destination float sym's non-number bailout bit is set, or the sym is in an equivalence class whose non-number
          bailout bit is set, change the bailout to bail out on any non-number value

    The result is that if a float-specialized sym's value is used in a way in which it would be invalid to use the float value
    through any other float-specialized sym that acquires the value, the FromVar generating the float value will be modified to
    bail out on any non-number value.
    */

    Assert(instr);

    if(tag != Js::DeadStorePhase || instr->GetSrc2() || !instr->m_func->hasBailout)
    {
        return;
    }

    if(!instr->GetDst() || !instr->GetDst()->IsRegOpnd())
    {
        return;
    }
    const auto dst = instr->GetDst()->AsRegOpnd()->m_sym;
    if(!dst->IsFloat64())
    {
        return;
    }

    if(!instr->GetSrc1() || !instr->GetSrc1()->IsRegOpnd())
    {
        return;
    }
    const auto src = instr->GetSrc1()->AsRegOpnd()->m_sym;

    if(OpCodeAttr::NonIntTransfer(instr->m_opcode) && (!currentBlock->loop || IsPrePass()))
    {
        Assert(src->IsFloat64()); // dst is specialized, and since this is a float transfer, src must be specialized too

        if(dst == src)
        {
            return;
        }

        if(!func->m_fg->hasLoop)
        {
            // Special case for functions with no loops, since there can only be in-order dependencies. Just merge the two
            // non-number bailout bits and put the result in the source.
            if(dst->m_requiresBailOnNotNumber)
            {
                src->m_requiresBailOnNotNumber = true;
            }
            return;
        }

        FloatSymEquivalenceClass *dstEquivalenceClass, *srcEquivalenceClass;
        const bool dstHasEquivalenceClass = floatSymEquivalenceMap->TryGetValue(dst->m_id, &dstEquivalenceClass);
        const bool srcHasEquivalenceClass = floatSymEquivalenceMap->TryGetValue(src->m_id, &srcEquivalenceClass);

        if(!dstHasEquivalenceClass)
        {
            if(srcHasEquivalenceClass)
            {
                // Just add the destination into the source's equivalence class
                srcEquivalenceClass->Set(dst);
                floatSymEquivalenceMap->Add(dst->m_id, srcEquivalenceClass);
                return;
            }

            dstEquivalenceClass = JitAnew(tempAlloc, FloatSymEquivalenceClass, tempAlloc);
            dstEquivalenceClass->Set(dst);
            floatSymEquivalenceMap->Add(dst->m_id, dstEquivalenceClass);
        }

        if(!srcHasEquivalenceClass)
        {
            // Just add the source into the destination's equivalence class
            dstEquivalenceClass->Set(src);
            floatSymEquivalenceMap->Add(src->m_id, dstEquivalenceClass);
            return;
        }

        if(dstEquivalenceClass == srcEquivalenceClass)
        {
            return;
        }

        Assert(!dstEquivalenceClass->Bv()->Test(src->m_id));
        Assert(!srcEquivalenceClass->Bv()->Test(dst->m_id));

        // Merge the two equivalence classes. The source's equivalence class is typically smaller, so it's merged into the
        // destination's equivalence class. To save space and prevent a potential explosion of bit vector size,
        // 'floatSymEquivalenceMap' is updated for syms in the source's equivalence class to map to the destination's now merged
        // equivalence class, and the source's equivalence class is discarded.
        dstEquivalenceClass->Or(srcEquivalenceClass);
        FOREACH_BITSET_IN_SPARSEBV(id, srcEquivalenceClass->Bv())
        {
            floatSymEquivalenceMap->Item(id, dstEquivalenceClass);
        } NEXT_BITSET_IN_SPARSEBV;
        JitAdelete(tempAlloc, srcEquivalenceClass);

        return;
    }

    // Not a float transfer, and non-prepass (not necessarily in a loop)

    if(!instr->HasBailOutInfo() || instr->GetBailOutKind() != IR::BailOutPrimitiveButString)
    {
        return;
    }
    Assert(instr->m_opcode == Js::OpCode::FromVar);

    // If either the destination or its equivalence class says it requires bailout on any non-number value, adjust the bailout
    // kind on the instruction. Both are checked because in functions without loops, equivalence tracking is not done and only
    // the sym's non-number bailout bit will have the information, and in functions with loops, equivalence tracking is done
    // throughout the function and checking just the sym's non-number bailout bit is insufficient.
    FloatSymEquivalenceClass *dstEquivalenceClass;
    if(dst->m_requiresBailOnNotNumber ||
        floatSymEquivalenceMap->TryGetValue(dst->m_id, &dstEquivalenceClass) && dstEquivalenceClass->RequiresBailOnNotNumber())
    {
        instr->SetBailOutKind(IR::BailOutNumberOnly);
    }
}

bool
BackwardPass::ProcessDef(IR::Opnd * opnd)
{
    BOOLEAN isJITOptimizedReg = false;
    Sym * sym;
    if (opnd->IsRegOpnd())
    {
        sym = opnd->AsRegOpnd()->m_sym;
        isJITOptimizedReg = opnd->GetIsJITOptimizedReg();
        if (!IsCollectionPass())
        {
            this->InvalidateCloneStrCandidate(opnd);
        }
    }
    else if (opnd->IsSymOpnd())
    {
        sym = opnd->AsSymOpnd()->m_sym;
        isJITOptimizedReg = opnd->GetIsJITOptimizedReg();
    }
    else
    {
        if (opnd->IsIndirOpnd())
        {
            this->ProcessUse(opnd);
        }
        return false;
    }

    BasicBlock * block = this->currentBlock;
    BOOLEAN isUsed = true;
    BOOLEAN keepSymLiveForException = false;
    BOOLEAN keepVarSymLiveForException = false;
    IR::Instr * instr = this->currentInstr;
    Assert(!instr->IsByteCodeUsesInstr());
    if (sym->IsPropertySym())
    {
        if(IsCollectionPass())
        {
            return false;
        }

        Assert((block->fieldHoistCandidates != nullptr) == this->DoFieldHoistCandidates());
        if (block->fieldHoistCandidates)
        {
            block->fieldHoistCandidates->Clear(sym->m_id);
        }
        PropertySym *propertySym = sym->AsPropertySym();
        if (this->DoDeadStoreSlots())
        {
            if (propertySym->m_fieldKind == PropertyKindLocalSlots || propertySym->m_fieldKind == PropertyKindSlots)
            {
                isUsed = !block->slotDeadStoreCandidates->TestAndSet(propertySym->m_id);
                // we should not do any dead slots in asmjs loop body
                Assert(!(this->func->GetJnFunction()->GetIsAsmJsFunction() && this->func->IsLoopBody() && !isUsed));
                Assert(isUsed || !block->upwardExposedUses->Test(propertySym->m_id));
            }
        }

        if (!block->upwardExposedFields->TestAndClear(propertySym->m_id) && this->DoSetDead())
        {
            opnd->SetIsDead();
        }
        ProcessStackSymUse(propertySym->m_stackSym, isJITOptimizedReg);
        if (tag == Js::BackwardPhase)
        {
            if (opnd->AsSymOpnd()->IsPropertySymOpnd())
            {
                this->globOpt->PreparePropertySymOpndForTypeCheckSeq(opnd->AsPropertySymOpnd(), instr, this->currentBlock->loop);
            }
        }
        if (opnd->AsSymOpnd()->IsPropertySymOpnd())
        {
            this->ProcessPropertySymOpndUse(opnd->AsPropertySymOpnd());
        }
    }
    else
    {
        Assert(!instr->IsByteCodeUsesInstr());

        if (this->DoByteCodeUpwardExposedUsed())
        {
            if (sym->AsStackSym()->HasByteCodeRegSlot())
            {
                StackSym * varSym = sym->AsStackSym();
                if (varSym->IsTypeSpec())
                {
                    // It has to have a var version for byte code regs
                    varSym = varSym->GetVarEquivSym(nullptr);
                }

                if (this->currentRegion)
                {
                    keepSymLiveForException = this->CheckWriteThroughSymInRegion(this->currentRegion, sym->AsStackSym());
                    keepVarSymLiveForException = this->CheckWriteThroughSymInRegion(this->currentRegion, varSym);
                }

                if (!isJITOptimizedReg)
                {
                    if (!DoDeadStore(this->func, sym->AsStackSym()))
                    {
                        // Don't deadstore the bytecodereg sym, so that we could do write to get the locals inspection
                        if (opnd->IsRegOpnd())
                        {
                            opnd->AsRegOpnd()->m_dontDeadStore = true;
                        }
                    }

                    // write through symbols should not be cleared from the byteCodeUpwardExposedUsed BV upon defs in the Try region:
                    //      try
                    //          x =
                    //          <bailout> <-- this bailout should restore x from its first def. This would not happen if x is cleared
                    //                        from byteCodeUpwardExposedUsed when we process its second def
                    //          <exception>
                    //          x =
                    //      catch
                    //          = x
                    if (!keepVarSymLiveForException)
                    {
                        // Always track the sym use on the var sym.
                        block->byteCodeUpwardExposedUsed->Clear(varSym->m_id);
#if DBG
                        // TODO: We can only track first level function stack syms right now
                        if (varSym->GetByteCodeFunc() == this->func)
                        {
                            block->byteCodeRestoreSyms[varSym->GetByteCodeRegSlot()] = nullptr;
                        }
#endif
                    }
                }
            }
        }

        if(IsCollectionPass())
        {
            return false;
        }

        // Don't care about property sym for mark temps
        if (opnd->IsRegOpnd())
        {
            this->MarkTemp(sym->AsStackSym());
        }

        if (this->tag == Js::BackwardPhase &&
            instr->m_opcode == Js::OpCode::Ld_A &&
            instr->GetSrc1()->IsRegOpnd() &&
            block->upwardExposedFields->Test(sym->m_id))
        {
            block->upwardExposedFields->Set(instr->GetSrc1()->AsRegOpnd()->m_sym->m_id);
        }

        if (!keepSymLiveForException)
        {
            isUsed = block->upwardExposedUses->TestAndClear(sym->m_id);
        }
    }

    if (isUsed || !this->DoDeadStore())
    {
        return false;
    }

    // FromVar on a primitive value has no side-effects
    // TODO: There may be more cases where FromVars can be dead-stored, such as cases where they have a bailout that would bail
    // out on non-primitive vars, thereby causing no side effects anyway. However, it needs to be ensured that no assumptions
    // that depend on the bailout are made later in the function.

    // Special case StFld for trackable fields
    bool hasSideEffects = instr->HasAnySideEffects()
        && instr->m_opcode != Js::OpCode::StFld
        && instr->m_opcode != Js::OpCode::StRootFld
        && instr->m_opcode != Js::OpCode::StFldStrict
        && instr->m_opcode != Js::OpCode::StRootFldStrict;

    if (this->IsPrePass() || hasSideEffects)
    {
        return false;
    }

    if (opnd->IsRegOpnd() && opnd->AsRegOpnd()->m_dontDeadStore)
    {
        return false;
    }

    if (instr->HasBailOutInfo())
    {
        // A bailout inserted for aggressive or lossy int type specialization causes assumptions to be made on the value of
        // the instruction's destination later on, as though the bailout did not happen. If the value is an int constant and
        // that value is propagated forward, it can cause the bailout instruction to become a dead store and be removed,
        // thereby invalidating the assumptions made. Or for lossy int type specialization, the lossy conversion to int32
        // may have side effects and so cannot be dead-store-removed. As one way of solving that problem, bailout
        // instructions resulting from aggressive or lossy int type spec are not dead-stored.
        const auto bailOutKind = instr->GetBailOutKind();
        if(bailOutKind & IR::BailOutOnResultConditions)
        {
            return false;
        }
        switch(bailOutKind & ~IR::BailOutKindBits)
        {
            case IR::BailOutIntOnly:
            case IR::BailOutNumberOnly:
            case IR::BailOutExpectingInteger:
            case IR::BailOutPrimitiveButString:
            case IR::BailOutExpectingString:
            case IR::BailOutOnLossyToInt32ImplicitCalls:
            case IR::BailOutFailedInlineTypeCheck:
            case IR::BailOutOnFloor:
            case IR::BailOnModByPowerOf2:
            case IR::BailOnDivResultNotInt:
            case IR::BailOnIntMin:
                return false;
        }
    }

    // Dead store
    DeadStoreInstr(instr);
    return true;
}

bool
BackwardPass::DeadStoreInstr(IR::Instr *instr)
{
    BasicBlock * block = this->currentBlock;

#if DBG_DUMP
    if (this->IsTraceEnabled())
    {
        Output::Print(L"Deadstore instr: ");
        instr->Dump();
    }
    this->numDeadStore++;
#endif

    // Before we remove the dead store, we need to track the byte code uses
    if (this->DoByteCodeUpwardExposedUsed())
    {
#if DBG
        BVSparse<JitArenaAllocator> tempBv(this->tempAlloc);
        tempBv.Copy(this->currentBlock->byteCodeUpwardExposedUsed);
#endif
        PropertySym *unusedPropertySym = nullptr;
        GlobOpt::TrackByteCodeSymUsed(instr, this->currentBlock->byteCodeUpwardExposedUsed, &unusedPropertySym);
#if DBG
        BVSparse<JitArenaAllocator> tempBv2(this->tempAlloc);
        tempBv2.Copy(this->currentBlock->byteCodeUpwardExposedUsed);
        tempBv2.Minus(&tempBv);
        FOREACH_BITSET_IN_SPARSEBV(symId, &tempBv2)
        {
            StackSym * stackSym = this->func->m_symTable->FindStackSym(symId);
            Assert(stackSym->GetType() == TyVar);
            // TODO: We can only track first level function stack syms right now
            if (stackSym->GetByteCodeFunc() == this->func)
            {
                Js::RegSlot byteCodeRegSlot = stackSym->GetByteCodeRegSlot();
                Assert(byteCodeRegSlot != Js::Constants::NoRegister);
                if (this->currentBlock->byteCodeRestoreSyms[byteCodeRegSlot] != stackSym)
                {
                    AssertMsg(this->currentBlock->byteCodeRestoreSyms[byteCodeRegSlot] == nullptr,
                        "Can't have two active lifetime for the same byte code register");
                    this->currentBlock->byteCodeRestoreSyms[byteCodeRegSlot] = stackSym;
                }
            }
        }
        NEXT_BITSET_IN_SPARSEBV;
#endif
    }

    // If this is a pre-op bailout instruction, we may have saved it for bailout info processing. It's being removed now, so no
    // need to process the bailout info anymore.
    Assert(!preOpBailOutInstrToProcess || preOpBailOutInstrToProcess == instr);
    preOpBailOutInstrToProcess = nullptr;

#if DBG
    if (this->DoMarkTempObjectVerify())
    {
        this->currentBlock->tempObjectVerifyTracker->NotifyDeadStore(instr, this);
    }
#endif

    block->RemoveInstr(instr);
    return true;
}

void
BackwardPass::ProcessTransfers(IR::Instr * instr)
{
    if (this->tag == Js::DeadStorePhase &&
        this->currentBlock->upwardExposedFields &&
        instr->m_opcode == Js::OpCode::Ld_A &&
        instr->GetDst()->GetStackSym() &&
        !instr->GetDst()->GetStackSym()->IsTypeSpec() &&
        instr->GetDst()->GetStackSym()->HasObjectInfo() &&
        instr->GetSrc1() &&
        instr->GetSrc1()->GetStackSym() &&
        !instr->GetSrc1()->GetStackSym()->IsTypeSpec() &&
        instr->GetSrc1()->GetStackSym()->HasObjectInfo())
    {
        StackSym * dstStackSym = instr->GetDst()->GetStackSym();
        PropertySym * dstPropertySym = dstStackSym->GetObjectInfo()->m_propertySymList;
        BVSparse<JitArenaAllocator> transferFields(this->tempAlloc);
        while (dstPropertySym != nullptr)
        {
            Assert(dstPropertySym->m_stackSym == dstStackSym);
            transferFields.Set(dstPropertySym->m_id);
            dstPropertySym = dstPropertySym->m_nextInStackSymList;
        }

        StackSym * srcStackSym = instr->GetSrc1()->GetStackSym();
        PropertySym * srcPropertySym = srcStackSym->GetObjectInfo()->m_propertySymList;
        BVSparse<JitArenaAllocator> equivFields(this->tempAlloc);

        while (srcPropertySym != nullptr && !transferFields.IsEmpty())
        {
            Assert(srcPropertySym->m_stackSym == srcStackSym);
            if (srcPropertySym->m_propertyEquivSet)
            {
                equivFields.And(&transferFields, srcPropertySym->m_propertyEquivSet);
                if (!equivFields.IsEmpty())
                {
                    transferFields.Minus(&equivFields);
                    this->currentBlock->upwardExposedFields->Set(srcPropertySym->m_id);
                }
            }
            srcPropertySym = srcPropertySym->m_nextInStackSymList;
        }
    }
}

void
BackwardPass::ProcessFieldKills(IR::Instr * instr)
{
    if (this->currentBlock->upwardExposedFields)
    {
        this->globOpt->ProcessFieldKills(instr, this->currentBlock->upwardExposedFields, false);
    }

    this->ClearBucketsOnFieldKill(instr, currentBlock->stackSymToFinalType);
    this->ClearBucketsOnFieldKill(instr, currentBlock->stackSymToGuardedProperties);
}

template<typename T>
void
BackwardPass::ClearBucketsOnFieldKill(IR::Instr *instr, HashTable<T> *table)
{
    if (table)
    {
        if (instr->UsesAllFields())
        {
            table->ClearAll();
        }
        else
        {
            IR::Opnd *dst = instr->GetDst();
            if (dst && dst->IsRegOpnd())
            {
                table->Clear(dst->AsRegOpnd()->m_sym->m_id);
            }
        }
    }
}

void
BackwardPass::ProcessFieldHoistKills(IR::Instr * instr)
{
    // The backward pass, we optimistically will not kill on a[] access
    // So that the field hoist candidate will be more then what can be hoisted
    // The root prepass will figure out the exact set of field that is hoisted
    this->globOpt->ProcessFieldKills(instr, this->currentBlock->fieldHoistCandidates, false);

    switch (instr->m_opcode)
    {
    case Js::OpCode::BrOnHasProperty:
    case Js::OpCode::BrOnNoProperty:
        // Should not hoist pass these instructions
        this->currentBlock->fieldHoistCandidates->Clear(instr->GetSrc1()->AsSymOpnd()->m_sym->m_id);
        break;
    }
}

bool
BackwardPass::TrackNoImplicitCallInlinees(IR::Instr *instr)
{
    if (this->tag != Js::DeadStorePhase || this->IsPrePass())
    {
        return false;
    }

    if (instr->HasBailOutInfo()
        || OpCodeAttr::CallInstr(instr->m_opcode)
        || instr->CallsAccessor()
        || GlobOpt::MayNeedBailOnImplicitCall(instr, nullptr, nullptr)
        || instr->m_opcode == Js::OpCode::LdHeapArguments
        || instr->m_opcode == Js::OpCode::LdLetHeapArguments
        || instr->m_opcode == Js::OpCode::LdHeapArgsCached
        || instr->m_opcode == Js::OpCode::LdLetHeapArgsCached
        || instr->m_opcode == Js::OpCode::LdFuncExpr)
    {
        // This func has instrs with bailouts or implicit calls
        Assert(instr->m_opcode != Js::OpCode::InlineeStart);
        instr->m_func->SetHasImplicitCallsOnSelfAndParents();
        return false;
    }

    if (instr->m_opcode == Js::OpCode::InlineeStart)
    {
        if (!instr->GetSrc1())
        {
            Assert(instr->m_func->m_hasInlineArgsOpt);
            return false;
        }
        return this->ProcessInlineeStart(instr);
    }

    return false;
}

bool
BackwardPass::ProcessInlineeStart(IR::Instr* inlineeStart)
{
    inlineeStart->m_func->SetFirstArgOffset(inlineeStart);

    IR::Instr* startCallInstr = nullptr;
    bool noImplicitCallsInInlinee = false;
    // Inlinee has no bailouts or implicit calls.  Get rid of the inline overhead.
    auto removeInstr = [&](IR::Instr* argInstr)
    {
        Assert(argInstr->m_opcode == Js::OpCode::InlineeStart || argInstr->m_opcode == Js::OpCode::ArgOut_A || argInstr->m_opcode == Js::OpCode::ArgOut_A_Inline);
        IR::Opnd *opnd = argInstr->GetSrc1();
        StackSym *sym = opnd->GetStackSym();
        if (!opnd->GetIsJITOptimizedReg() && sym && sym->HasByteCodeRegSlot())
        {
            // Replace instrs with bytecodeUses
            IR::ByteCodeUsesInstr *bytecodeUse = IR::ByteCodeUsesInstr::New(argInstr, sym->m_id);
            argInstr->InsertBefore(bytecodeUse);
        }
        startCallInstr = argInstr->GetSrc2()->GetStackSym()->m_instrDef;
        FlowGraph::SafeRemoveInstr(argInstr);
        return false;
    };

    // If there are no implicit calls - bailouts/throws - we can remove all inlining overhead.
    if (!inlineeStart->m_func->GetHasImplicitCalls())
    {
        noImplicitCallsInInlinee = true;
        inlineeStart->IterateArgInstrs(removeInstr);

        inlineeStart->IterateMetaArgs([](IR::Instr* metArg)
        {
            FlowGraph::SafeRemoveInstr(metArg);
            return false;
        });
        inlineeStart->m_func->m_hasInlineArgsOpt = false;
        removeInstr(inlineeStart);
        return true;
    }

    if (!inlineeStart->m_func->m_hasInlineArgsOpt)
    {
        PHASE_PRINT_TESTTRACE(Js::InlineArgsOptPhase, func, L"%s[%d]: Skipping inline args optimization: %s[%d] HasCalls: %s 'arguments' access: %s Can do inlinee args opt: %s\n",
                func->GetJnFunction()->GetExternalDisplayName(), func->GetJnFunction()->GetFunctionNumber(),
                inlineeStart->m_func->GetJnFunction()->GetExternalDisplayName(), inlineeStart->m_func->GetJnFunction()->GetFunctionNumber(),
                IsTrueOrFalse(inlineeStart->m_func->GetHasCalls()),
                IsTrueOrFalse(inlineeStart->m_func->GetHasUnoptimizedArgumentsAcccess()),
                IsTrueOrFalse(inlineeStart->m_func->m_canDoInlineArgsOpt));
        return false;
    }

    if (!inlineeStart->m_func->frameInfo->isRecorded)
    {
        PHASE_PRINT_TESTTRACE(Js::InlineArgsOptPhase, func, L"%s[%d]: InlineeEnd not found - usually due to a throw or a BailOnNoProfile (stressed, most likely)\n",
            func->GetJnFunction()->GetExternalDisplayName(), func->GetJnFunction()->GetFunctionNumber());
        inlineeStart->m_func->DisableCanDoInlineArgOpt();
        return false;
    }

    inlineeStart->IterateArgInstrs(removeInstr);
    int i = 0;
    inlineeStart->IterateMetaArgs([&](IR::Instr* metaArg)
    {
        if (i == Js::Constants::InlineeMetaArgIndex_ArgumentsObject &&
            inlineeStart->m_func->GetHasArgumentObject())
        {
            Assert(!inlineeStart->m_func->GetHasUnoptimizedArgumentsAcccess());
            // Do not remove arguments object meta arg if there is a reference to arguments object
        }
        else
        {
            FlowGraph::SafeRemoveInstr(metaArg);
        }
        i++;
        return false;
    });

    IR::Opnd *src1 = inlineeStart->GetSrc1();

    StackSym *sym = src1->GetStackSym();
    if (!src1->GetIsJITOptimizedReg() && sym && sym->HasByteCodeRegSlot())
    {
        // Replace instrs with bytecodeUses
        IR::ByteCodeUsesInstr *bytecodeUse = IR::ByteCodeUsesInstr::New(inlineeStart, sym->m_id);
        inlineeStart->InsertBefore(bytecodeUse);
    }

    // This indicates to the lowerer that this inlinee has been optimized
    // and it should not be lowered - Now this instruction is used to mark inlineeStart
    inlineeStart->FreeSrc1();
    inlineeStart->FreeSrc2();
    inlineeStart->FreeDst();
    return true;
}

void
BackwardPass::ProcessInlineeEnd(IR::Instr* instr)
{
    if (this->IsPrePass())
    {
        return;
    }
    if (this->tag == Js::BackwardPhase)
    {
        if (!GlobOpt::DoInlineArgsOpt(instr->m_func))
        {
            return;
        }

        // This adds a use for function sym as part of InlineeStart & all the syms referenced by the args.
        // It ensure they do not get cleared from the copy prop sym map.
        instr->IterateArgInstrs([=](IR::Instr* argInstr){
            if (argInstr->GetSrc1()->IsRegOpnd())
            {
                this->currentBlock->upwardExposedUses->Set(argInstr->GetSrc1()->AsRegOpnd()->m_sym->m_id);
            }
            return false;
        });
    }
    else if (this->tag == Js::DeadStorePhase)
    {
        if (instr->m_func->m_hasInlineArgsOpt)
        {
            Assert(instr->m_func->frameInfo);
            instr->m_func->frameInfo->IterateSyms([=](StackSym* argSym)
            {
                this->currentBlock->upwardExposedUses->Set(argSym->m_id);
            });
        }
    }
}

bool
BackwardPass::ProcessBailOnNoProfile(IR::Instr *instr, BasicBlock *block)
{
    Assert(this->tag == Js::BackwardPhase);
    Assert(instr->m_opcode == Js::OpCode::BailOnNoProfile);
    Assert(!instr->HasBailOutInfo());
    AnalysisAssert(block);

    if (this->IsPrePass())
    {
        return false;
    }

    IR::Instr *curInstr = instr->m_prev;

    if (curInstr->IsLabelInstr() && curInstr->AsLabelInstr()->isOpHelper)
    {
        // Already processed

        if (this->DoMarkTempObjects())
        {
            block->tempObjectTracker->ProcessBailOnNoProfile(instr);
        }
        return false;
    }

    // Don't hoist if we see calls with profile data (recursive calls)
    while(!curInstr->StartsBasicBlock())
    {
        // If a function was inlined, it must have had profile info.
        if (curInstr->m_opcode == Js::OpCode::InlineeEnd || curInstr->m_opcode == Js::OpCode::InlineBuiltInEnd || curInstr->m_opcode == Js::OpCode::InlineNonTrackingBuiltInEnd
            || curInstr->m_opcode == Js::OpCode::InlineeStart || curInstr->m_opcode == Js::OpCode::EndCallForPolymorphicInlinee)
        {
            break;
        }
        else if (OpCodeAttr::CallInstr(curInstr->m_opcode))
        {
            if (curInstr->m_prev->m_opcode != Js::OpCode::BailOnNoProfile)
            {
                break;
            }
        }
        curInstr = curInstr->m_prev;
    }

    // Didn't get to the top of the block, delete this BailOnNoProfile...
    if (!curInstr->IsLabelInstr())
    {
        block->RemoveInstr(instr);
        return true;
    }

    // We can't bail in the middle of a "tmp = CmEq s1, s2; BrTrue tmp" turned into a "BrEq s1, s2",
    // because the bailout wouldn't be able to restore tmp.
    IR::Instr *curNext = curInstr->GetNextRealInstrOrLabel();
    if (curNext->m_opcode == Js::OpCode::Ld_A && curNext->GetDst()->IsRegOpnd() && curNext->GetDst()->AsRegOpnd()->m_fgPeepTmp)
    {
        block->RemoveInstr(instr);
        return true;
    }

    curInstr = instr->m_prev;

    // Move to top of block.
    while(!curInstr->StartsBasicBlock())
    {
        // Delete redundant BailOnNoProfile
        if (curInstr->m_opcode == Js::OpCode::BailOnNoProfile)
        {
            Assert(!curInstr->HasBailOutInfo());
            curInstr = curInstr->m_next;
            curInstr->m_prev->Remove();
        }
        curInstr = curInstr->m_prev;
    }

    if (instr == block->GetLastInstr())
    {
        block->SetLastInstr(instr->m_prev);
    }

    instr->Unlink();

    // Now try to move this up the flowgraph to the predecessor blocks
    bool curBlockNeedsBail = false;
    FOREACH_PREDECESSOR_BLOCK(pred, block)
    {
        bool hoistBailToPred = true;

        if (block->isLoopHeader && pred->loop == block->loop)
        {
            // Skip loop back-edges
            continue;
        }

        // If all successors of this predecessor start with a BailOnNoProfile, we should be
        // okay to hoist this bail to the predecessor.
        FOREACH_SUCCESSOR_BLOCK(predSucc, pred)
        {
            if (predSucc == block)
            {
                continue;
            }
            if (predSucc->GetFirstInstr()->m_next->m_opcode != Js::OpCode::BailOnNoProfile)
            {
                hoistBailToPred = false;
                break;
            }
        } NEXT_SUCCESSOR_BLOCK;

        if (hoistBailToPred)
        {
            IR::Instr *predInstr = pred->GetLastInstr();
            IR::Instr *instrCopy = instr->Copy();

            if (predInstr->EndsBasicBlock())
            {
                if (predInstr->m_prev->m_opcode == Js::OpCode::BailOnNoProfile)
                {
                    // We already have one, we don't need a second.
                    instrCopy->Free();
                }
                else if (predInstr->AsBranchInstr()->m_isSwitchBr)
                {
                    // Don't put a bailout in the middle of a switch dispatch sequence.
                    // The bytecode offsets are not in order, and it would lead to incorrect
                    // bailout info.
                    curBlockNeedsBail = true;
                }
                else
                {
                    instrCopy->m_func = predInstr->m_func;
                    predInstr->InsertBefore(instrCopy);
                }
            }
            else
            {
                if (predInstr->m_opcode == Js::OpCode::BailOnNoProfile)
                {
                    // We already have one, we don't need a second.
                    instrCopy->Free();
                }
                else
                {
                    instrCopy->m_func = predInstr->m_func;
                    predInstr->InsertAfter(instrCopy);
                    pred->SetLastInstr(instrCopy);
                }
            }
        }
        else
        {
            curBlockNeedsBail = true;
        }
    } NEXT_PREDECESSOR_BLOCK;

    if (curBlockNeedsBail)
    {
        curInstr->AsLabelInstr()->isOpHelper = true;
#if DBG
        curInstr->AsLabelInstr()->m_noHelperAssert = true;
#endif

        instr->m_func = curInstr->m_func;
        curInstr->InsertAfter(instr);

        bool setLastInstr = (curInstr == block->GetLastInstr());
        if (setLastInstr)
        {
            block->SetLastInstr(instr);
        }

        if (this->DoMarkTempObjects())
        {
            block->tempObjectTracker->ProcessBailOnNoProfile(instr);
        }
        return false;
    }
    else
    {
        instr->Free();
        return true;
    }
}

bool
BackwardPass::ReverseCopyProp(IR::Instr *instr)
{
    // Look for :
    //
    //  t1 = instr
    //       [bytecodeuse t1]
    //  t2 = Ld_A t1            >> t1 !upwardExposed
    //
    // Transform into:
    //
    //  t2 = instr
    //
    if (PHASE_OFF(Js::ReverseCopyPropPhase, this->func))
    {
        return false;
    }
    if (this->tag != Js::DeadStorePhase || this->IsPrePass() || this->IsCollectionPass())
    {
        return false;
    }
    if (this->func->HasTry())
    {
        // UpwardExposedUsed info can't be relied on
        return false;
    }

    // Find t2 = Ld_A t1
    switch (instr->m_opcode)
    {
    case Js::OpCode::Ld_A:
    case Js::OpCode::Ld_I4:
        break;

    default:
        return false;
    }

    if (!instr->GetDst()->IsRegOpnd())
    {
        return false;
    }
    if (!instr->GetSrc1()->IsRegOpnd())
    {
        return false;
    }
    if (instr->HasBailOutInfo())
    {
        return false;
    }

    IR::RegOpnd *dst = instr->GetDst()->AsRegOpnd();
    IR::RegOpnd *src = instr->GetSrc1()->AsRegOpnd();
    IR::Instr *instrPrev = instr->GetPrevRealInstrOrLabel();

    IR::ByteCodeUsesInstr *byteCodeUseInstr = nullptr;
    StackSym *varSym = src->m_sym;

    if (varSym->IsTypeSpec())
    {
        varSym = varSym->GetVarEquivSym(this->func);
    }

    // SKip ByteCodeUse instr if possible
    //       [bytecodeuse t1]
    if (!instrPrev->GetDst())
    {
        if (instrPrev->m_opcode == Js::OpCode::ByteCodeUses)
        {
            byteCodeUseInstr = instrPrev->AsByteCodeUsesInstr();

            if (byteCodeUseInstr->byteCodeUpwardExposedUsed && byteCodeUseInstr->byteCodeUpwardExposedUsed->Test(varSym->m_id) && byteCodeUseInstr->byteCodeUpwardExposedUsed->Count() == 1)
            {
                instrPrev = byteCodeUseInstr->GetPrevRealInstrOrLabel();

                if (!instrPrev->GetDst())
                {
                    return false;
                }
            }
            else
            {
                return false;
            }
        }
        else
        {
            return false;
        }
    }

    // The fast-path for these doesn't handle dst == src.
    // REVIEW: I believe the fast-path for LdElemI_A has been fixed... Nope, still broken for "i = A[i]" for –prejit
    switch (instrPrev->m_opcode)
    {
    case Js::OpCode::LdElemI_A:
    case Js::OpCode::IsInst:
    case Js::OpCode::ByteCodeUses:
        return false;
    }

    // Can't do it if post-op bailout would need result
    // REVIEW: enable for pre-opt bailout?
    if (instrPrev->HasBailOutInfo() && instrPrev->GetByteCodeOffset() != instrPrev->GetBailOutInfo()->bailOutOffset)
    {
        return false;
    }

    // Make sure src of Ld_A == dst of instr
    //  t1 = instr
    if (!instrPrev->GetDst()->IsEqual(src))
    {
        return false;
    }

    // Make sure t1 isn't used later
    if (this->currentBlock->upwardExposedUses->Test(src->m_sym->m_id))
    {
        return false;
    }

    if (this->currentBlock->byteCodeUpwardExposedUsed && this->currentBlock->byteCodeUpwardExposedUsed->Test(varSym->m_id))
    {
        return false;
    }

    // Make sure we can dead-store this sym (debugger mode?)
    if (!this->DoDeadStore(this->func, src->m_sym))
    {
        return false;
    }

    StackSym *const dstSym = dst->m_sym;
    if(instrPrev->HasBailOutInfo() && dstSym->IsInt32() && dstSym->IsTypeSpec())
    {
        StackSym *const prevDstSym = IR::RegOpnd::TryGetStackSym(instrPrev->GetDst());
        if(instrPrev->GetBailOutKind() & IR::BailOutOnResultConditions &&
            prevDstSym &&
            prevDstSym->IsInt32() &&
            prevDstSym->IsTypeSpec() &&
            instrPrev->GetSrc1() &&
            !instrPrev->GetDst()->IsEqual(instrPrev->GetSrc1()) &&
            !(instrPrev->GetSrc2() && instrPrev->GetDst()->IsEqual(instrPrev->GetSrc2())))
        {
            // The previous instruction's dst value may be trashed by the time of the pre-op bailout. Skip reverse copy-prop if
            // it would replace the previous instruction's dst with a sym that bailout had decided to use to restore a value for
            // the pre-op bailout, which can't be trashed before bailout. See big comment in ProcessBailOutCopyProps for the
            // reasoning behind the tests above.
            FOREACH_SLISTBASE_ENTRY(
                CopyPropSyms,
                usedCopyPropSym,
                &instrPrev->GetBailOutInfo()->usedCapturedValues.copyPropSyms)
            {
                if(dstSym == usedCopyPropSym.Value())
                {
                    return false;
                }
            } NEXT_SLISTBASE_ENTRY;
        }
    }

    if (byteCodeUseInstr)
    {
        if (this->currentBlock->byteCodeUpwardExposedUsed && instrPrev->GetDst()->AsRegOpnd()->GetIsJITOptimizedReg() && varSym->HasByteCodeRegSlot())
        {
            if(varSym->HasByteCodeRegSlot())
            {
                this->currentBlock->byteCodeUpwardExposedUsed->Set(varSym->m_id);
            }

            if (src->IsEqual(dst) && instrPrev->GetDst()->GetIsJITOptimizedReg())
            {
                //      s2(s1).i32     =  FromVar        s1.var      #0000  Bailout: #0000 (BailOutIntOnly)
                //                        ByteCodeUses   s1
                //      s2(s1).i32     =  Ld_A           s2(s1).i32
                //
                // Since the dst on the FromVar is marked JITOptimized, we need to set it on the new dst as well,
                // or we'll change the bytecode liveness of s1

                dst->SetIsJITOptimizedReg(true);
            }
        }
        byteCodeUseInstr->Remove();
    }
    else if (instrPrev->GetDst()->AsRegOpnd()->GetIsJITOptimizedReg() && !src->GetIsJITOptimizedReg() && varSym->HasByteCodeRegSlot())
    {
        this->currentBlock->byteCodeUpwardExposedUsed->Set(varSym->m_id);
    }

#if DBG
    if (this->DoMarkTempObjectVerify())
    {
        this->currentBlock->tempObjectVerifyTracker->NotifyReverseCopyProp(instrPrev);
    }
#endif

    dst->SetValueType(instrPrev->GetDst()->GetValueType());
    instrPrev->ReplaceDst(dst);

    instr->Remove();

    return true;
}

bool
BackwardPass::FoldCmBool(IR::Instr *instr)
{
    Assert(instr->m_opcode == Js::OpCode::Conv_Bool);

    if (this->tag != Js::DeadStorePhase || this->IsPrePass() || this->IsCollectionPass())
    {
        return false;
    }
    if (this->func->HasTry())
    {
        // UpwardExposedUsed info can't be relied on
        return false;
    }

    IR::RegOpnd *intOpnd = instr->GetSrc1()->AsRegOpnd();

    Assert(intOpnd->m_sym->IsInt32());

    if (!intOpnd->m_sym->IsSingleDef())
    {
        return false;
    }

    IR::Instr *cmInstr = intOpnd->m_sym->GetInstrDef();

    // Should be a Cm instr...
    if (!cmInstr->GetSrc2())
    {
        return false;
    }

    IR::Instr *instrPrev = instr->GetPrevRealInstrOrLabel();

    if (instrPrev != cmInstr)
    {
        return false;
    }

    switch (cmInstr->m_opcode)
    {
    case Js::OpCode::CmEq_A:
    case Js::OpCode::CmGe_A:
    case Js::OpCode::CmUnGe_A:
    case Js::OpCode::CmGt_A:
    case Js::OpCode::CmUnGt_A:
    case Js::OpCode::CmLt_A:
    case Js::OpCode::CmUnLt_A:
    case Js::OpCode::CmLe_A:
    case Js::OpCode::CmUnLe_A:
    case Js::OpCode::CmNeq_A:
    case Js::OpCode::CmSrEq_A:
    case Js::OpCode::CmSrNeq_A:
    case Js::OpCode::CmEq_I4:
    case Js::OpCode::CmNeq_I4:
    case Js::OpCode::CmLt_I4:
    case Js::OpCode::CmLe_I4:
    case Js::OpCode::CmGt_I4:
    case Js::OpCode::CmGe_I4:
    case Js::OpCode::CmUnLt_I4:
    case Js::OpCode::CmUnLe_I4:
    case Js::OpCode::CmUnGt_I4:
    case Js::OpCode::CmUnGe_I4:
        break;

    default:
        return false;
    }

    IR::RegOpnd *varDst = instr->GetDst()->AsRegOpnd();

    if (this->currentBlock->upwardExposedUses->Test(intOpnd->m_sym->m_id) || !this->currentBlock->upwardExposedUses->Test(varDst->m_sym->m_id))
    {
        return false;
    }

    varDst = instr->UnlinkDst()->AsRegOpnd();

    cmInstr->ReplaceDst(varDst);

    this->currentBlock->RemoveInstr(instr);

    return true;
}

void
BackwardPass::SetWriteThroughSymbolsSetForRegion(BasicBlock * catchBlock, Region * tryRegion)
{
    tryRegion->writeThroughSymbolsSet = JitAnew(this->func->m_alloc, BVSparse<JitArenaAllocator>, this->func->m_alloc);

    if (this->DoByteCodeUpwardExposedUsed())
    {
        Assert(catchBlock->byteCodeUpwardExposedUsed);
        if (!catchBlock->byteCodeUpwardExposedUsed->IsEmpty())
        {
            FOREACH_BITSET_IN_SPARSEBV(id, catchBlock->byteCodeUpwardExposedUsed)
            {
                tryRegion->writeThroughSymbolsSet->Set(id);
            }
            NEXT_BITSET_IN_SPARSEBV
        }
#if DBG
        // Symbols write-through in the parent try region should be marked as write-through in the current try region as well.
        // x =
        // try{
        //      try{
        //          x =         <-- x needs to be write-through here. With the current mechanism of not clearing a write-through
        //                          symbol from the bytecode upward-exposed on a def, x should be marked as write-through as
        //                          write-through symbols for a try are basically the bytecode upward exposed symbols at the
        //                          beginning of the corresponding catch block).
        //                          Verify that it still holds.
        //          <exception>
        //      }
        //      catch(){}
        //      x =
        // }
        // catch(){}
        // = x
        if (tryRegion->GetParent()->GetType() == RegionTypeTry)
        {
            Region * parentTry = tryRegion->GetParent();
            Assert(parentTry->writeThroughSymbolsSet);
            FOREACH_BITSET_IN_SPARSEBV(id, parentTry->writeThroughSymbolsSet)
            {
                Assert(tryRegion->writeThroughSymbolsSet->Test(id));
            }
            NEXT_BITSET_IN_SPARSEBV
        }
#endif
    }
    else
    {
        // this can happen with -off:globopt
        return;
    }
}

bool
BackwardPass::CheckWriteThroughSymInRegion(Region* region, StackSym* sym)
{
    if (region->GetType() == RegionTypeRoot || region->GetType() == RegionTypeFinally)
    {
        return false;
    }

    // if the current region is a try region, check in its write-through set,
    // otherwise (current = catch region) look in the first try ancestor's write-through set
    Region * selfOrFirstTryAncestor = region->GetSelfOrFirstTryAncestor();
    if (!selfOrFirstTryAncestor)
    {
        return false;
    }
    Assert(selfOrFirstTryAncestor->GetType() == RegionTypeTry);
    return selfOrFirstTryAncestor->writeThroughSymbolsSet && selfOrFirstTryAncestor->writeThroughSymbolsSet->Test(sym->m_id);
}

bool
BackwardPass::DoDeadStoreLdStForMemop(IR::Instr *instr)
{
    Assert(this->tag == Js::DeadStorePhase && this->currentBlock->loop != nullptr);

    Loop *loop = this->currentBlock->loop;

    if (globOpt->DoMemOp(loop))
    {
        if (instr->m_opcode == Js::OpCode::StElemI_A && instr->GetDst()->IsIndirOpnd())
        {
            SymID base = this->globOpt->GetVarSymID(instr->GetDst()->AsIndirOpnd()->GetBaseOpnd()->GetStackSym());
            SymID index = this->globOpt->GetVarSymID(instr->GetDst()->AsIndirOpnd()->GetIndexOpnd()->GetStackSym());

            FOREACH_MEMOP_CANDIDATES(candidate, loop)
            {
                if (base == candidate->base && index == candidate->index)
                {
                    return true;
                }
            } NEXT_MEMOP_CANDIDATE
        }
        else if (instr->m_opcode == Js::OpCode::LdElemI_A &&  instr->GetSrc1()->IsIndirOpnd())
        {
            SymID base = this->globOpt->GetVarSymID(instr->GetSrc1()->AsIndirOpnd()->GetBaseOpnd()->GetStackSym());
            SymID index = this->globOpt->GetVarSymID(instr->GetSrc1()->AsIndirOpnd()->GetIndexOpnd()->GetStackSym());

            FOREACH_MEMCOPY_CANDIDATES(candidate, loop)
            {
                if (base == candidate->ldBase && index == candidate->index)
                {
                    return true;
                }
            } NEXT_MEMCOPY_CANDIDATE
        }
    }
    return false;
}

void
BackwardPass::RestoreInductionVariableValuesAfterMemOp(Loop *loop)
{
    const auto RestoreInductionVariable = [&](SymID symId, Loop::InductionVariableChangeInfo inductionVariableChangeInfo, Loop *loop)
    {
        Js::OpCode opCode = Js::OpCode::Add_I4;
        if (!inductionVariableChangeInfo.isIncremental)
        {
            opCode = Js::OpCode::Sub_I4;
        }
        Func *localFunc = loop->GetFunc();
        StackSym *sym = localFunc->m_symTable->FindStackSym(symId)->GetInt32EquivSym(localFunc);

        IR::Opnd *inductionVariableOpnd = IR::RegOpnd::New(sym, IRType::TyInt32, localFunc);
        IR::Opnd *sizeOpnd = globOpt->GenerateInductionVariableChangeForMemOp(loop, inductionVariableChangeInfo.unroll);
        loop->landingPad->InsertAfter(IR::Instr::New(opCode, inductionVariableOpnd, inductionVariableOpnd, sizeOpnd, loop->GetFunc()));
    };

    for (auto it = loop->memOpInfo->inductionVariableChangeInfoMap->GetIterator(); it.IsValid(); it.MoveNext())
    {
        Loop::InductionVariableChangeInfo iv = it.CurrentValue();
        SymID sym = it.CurrentKey();
        if (iv.unroll != Js::Constants::InvalidLoopUnrollFactor)
        {
            // if the variable is being used after the loop restore it
            if (loop->memOpInfo->inductionVariablesUsedAfterLoop->Test(sym))
            {
                RestoreInductionVariable(sym, iv, loop);
            }
        }
    }
}

bool
BackwardPass::IsEmptyLoopAfterMemOp(Loop *loop)
{
    if (globOpt->DoMemOp(loop))
    {
        const auto IsInductionVariableUse = [&](IR::Opnd *opnd) -> bool
        {
            Loop::InductionVariableChangeInfo  inductionVariableChangeInfo = { 0, 0 };
            return (opnd &&
                opnd->GetStackSym() &&
                loop->memOpInfo->inductionVariableChangeInfoMap->ContainsKey(this->globOpt->GetVarSymID(opnd->GetStackSym())) &&
                (((Loop::InductionVariableChangeInfo)
                    loop->memOpInfo->inductionVariableChangeInfoMap->
                    LookupWithKey(this->globOpt->GetVarSymID(opnd->GetStackSym()), inductionVariableChangeInfo)).unroll != Js::Constants::InvalidLoopUnrollFactor));
        };

        Assert(loop->blockList.HasTwo());

        FOREACH_BLOCK_IN_LOOP(bblock, loop)
        {

            FOREACH_INSTR_IN_BLOCK_EDITING(instr, instrPrev, bblock)
            {
                if (instr->IsLabelInstr() || !instr->IsRealInstr() || instr->m_opcode == Js::OpCode::IncrLoopBodyCount || instr->m_opcode == Js::OpCode::StLoopBodyCount
                    || (instr->IsBranchInstr() && instr->AsBranchInstr()->IsUnconditional()))
                {
                    continue;
                }
                else
                {
                    switch (instr->m_opcode)
                    {
                    case Js::OpCode::Nop:
                        break;
                    case Js::OpCode::Ld_I4:
                    case Js::OpCode::Add_I4:
                    case Js::OpCode::Sub_I4:

                        if (!IsInductionVariableUse(instr->GetDst()))
                        {
                            Assert(instr->GetDst());
                            if (instr->GetDst()->GetStackSym()
                                && loop->memOpInfo->inductionVariablesUsedAfterLoop->Test(globOpt->GetVarSymID(instr->GetDst()->GetStackSym())))
                            {
                                // We have use after the loop for a variable defined inside the loop. So the loop can't be removed.
                                return false;
                            }
                        }
                        break;
                    case Js::OpCode::Decr_A:
                    case Js::OpCode::Incr_A:
                        if (!IsInductionVariableUse(instr->GetSrc1()))
                        {
                            return false;
                        }
                        break;
                    default:
                        if (instr->IsBranchInstr())
                        {
                            if (IsInductionVariableUse(instr->GetSrc1()) || IsInductionVariableUse(instr->GetSrc2()))
                            {
                                break;
                            }
                        }
                        return false;
                    }
                }

            }
            NEXT_INSTR_IN_BLOCK_EDITING;

        }NEXT_BLOCK_IN_LIST;

        return true;
    }

    return false;
}

void
BackwardPass::RemoveEmptyLoops()
{
    if (PHASE_OFF(Js::MemOpPhase, this->func))
    {
        return;

    }
    const auto DeleteMemOpInfo = [&](Loop *loop)
    {
        JitArenaAllocator *alloc = this->func->GetTopFunc()->m_fg->alloc;

        if (!loop->memOpInfo)
        {
            return;
        }

        if (loop->memOpInfo->candidates)
        {
            loop->memOpInfo->candidates->Clear();
            JitAdelete(alloc, loop->memOpInfo->candidates);
        }

        if (loop->memOpInfo->inductionVariableChangeInfoMap)
        {
            loop->memOpInfo->inductionVariableChangeInfoMap->Clear();
            JitAdelete(alloc, loop->memOpInfo->inductionVariableChangeInfoMap);
        }

        if (loop->memOpInfo->inductionVariableOpndPerUnrollMap)
        {
            loop->memOpInfo->inductionVariableOpndPerUnrollMap->Clear();
            JitAdelete(alloc, loop->memOpInfo->inductionVariableOpndPerUnrollMap);
        }

        if (loop->memOpInfo->inductionVariablesUsedAfterLoop)
        {
            JitAdelete(this->tempAlloc, loop->memOpInfo->inductionVariablesUsedAfterLoop);
        }
        JitAdelete(alloc, loop->memOpInfo);
    };

    FOREACH_LOOP_IN_FUNC_EDITING(loop, this->func)
    {
        if (IsEmptyLoopAfterMemOp(loop))
        {
            RestoreInductionVariableValuesAfterMemOp(loop);
            RemoveEmptyLoopAfterMemOp(loop);
        }
        // Remove memop info as we don't need them after this point.
        DeleteMemOpInfo(loop);

    } NEXT_LOOP_IN_FUNC_EDITING;

}

void
BackwardPass::RemoveEmptyLoopAfterMemOp(Loop *loop)
{
    BasicBlock *head = loop->GetHeadBlock();
    BasicBlock *tail = head->next;
    BasicBlock *landingPad = loop->landingPad;
    BasicBlock *outerBlock = nullptr;
    SListBaseCounted<FlowEdge *> *succList = head->GetSuccList();
    Assert(succList->HasTwo());

    // Between the two successors of head, one is tail and the other one is the outerBlock
    SListBaseCounted<FlowEdge *>::Iterator  iter(succList);
    iter.Next();
    if (iter.Data()->GetSucc() == tail)
    {
        iter.Next();
        outerBlock = iter.Data()->GetSucc();
    }
    else
    {
        outerBlock = iter.Data()->GetSucc();
#ifdef DBG
        iter.Next();
        Assert(iter.Data()->GetSucc() == tail);
#endif
    }

    outerBlock->RemovePred(head, this->func->m_fg);
    landingPad->RemoveSucc(head, this->func->m_fg);
    this->func->m_fg->AddEdge(landingPad, outerBlock);

    this->func->m_fg->RemoveBlock(head, nullptr);

    if (head != tail)
    {
        this->func->m_fg->RemoveBlock(tail, nullptr);
    }
}

#if DBG_DUMP
bool
BackwardPass::IsTraceEnabled() const
{
    return
        Js::Configuration::Global.flags.Trace.IsEnabled(tag, this->func->GetSourceContextId(), this->func->GetLocalFunctionId()) &&
        (PHASE_TRACE(Js::SimpleJitPhase, func->GetJnFunction()) || !func->IsSimpleJit());
}
#endif
