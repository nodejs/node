//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "BackEnd.h"

FlowGraph *
FlowGraph::New(Func * func, JitArenaAllocator * alloc)
{
    FlowGraph * graph;

    graph = JitAnew(alloc, FlowGraph, func, alloc);

    return graph;
}

///----------------------------------------------------------------------------
///
/// FlowGraph::Build
///
/// Construct flow graph and loop structures for the current state of the function.
///
///----------------------------------------------------------------------------
void
FlowGraph::Build(void)
{
    Func * func = this->func;

    BEGIN_CODEGEN_PHASE(func, Js::FGPeepsPhase);
    this->RunPeeps();
    END_CODEGEN_PHASE(func, Js::FGPeepsPhase);

    // We don't optimize fully with SimpleJit. But, when JIT loop body is enabled, we do support
    // bailing out from a simple jitted function to do a full jit of a loop body in the function
    // (BailOnSimpleJitToFullJitLoopBody). For that purpose, we need the flow from try to catch.
    if (this->func->HasTry() &&
        (this->func->DoOptimizeTryCatch() ||
        this->func->IsSimpleJit() && this->func->GetJnFunction()->DoJITLoopBody()
        )
       )
    {
        this->catchLabelStack = JitAnew(this->alloc, SList<IR::LabelInstr*>, this->alloc);
    }

    IR::Instr * currLastInstr = nullptr;
    BasicBlock * block = nullptr;
    BasicBlock * nextBlock = nullptr;
    bool hasCall = false;
    FOREACH_INSTR_IN_FUNC_BACKWARD_EDITING(instr, instrPrev, func)
    {
        if (currLastInstr == nullptr || instr->EndsBasicBlock())
        {
            // Start working on a new block.
            // If we're currently processing a block, then wrap it up before beginning a new one.
            if (currLastInstr != nullptr)
            {
                nextBlock = block;
                block = this->AddBlock(instr->m_next, currLastInstr, nextBlock);
                block->hasCall = hasCall;
                hasCall = false;
            }

            currLastInstr = instr;
        }

        if (instr->StartsBasicBlock())
        {
            // Insert a BrOnException after the loop top if we are in a try-catch. This is required to
            // model flow from the loop to the catch block for loops that don't have a break condition.
            if (instr->IsLabelInstr() && instr->AsLabelInstr()->m_isLoopTop &&
                this->catchLabelStack && !this->catchLabelStack->Empty() &&
                instr->m_next->m_opcode != Js::OpCode::BrOnException)
            {
                IR::BranchInstr * brOnException = IR::BranchInstr::New(Js::OpCode::BrOnException, this->catchLabelStack->Top(), instr->m_func);
                instr->InsertAfter(brOnException);
                instrPrev = brOnException; // process BrOnException before adding a new block for loop top label.
                continue;
            }

            // Wrap up the current block and get ready to process a new one.
            nextBlock = block;
            block = this->AddBlock(instr, currLastInstr, nextBlock);
            block->hasCall = hasCall;
            hasCall = false;
            currLastInstr = nullptr;
        }

        switch (instr->m_opcode)
        {
        case Js::OpCode::Catch:
            Assert(instr->m_prev->IsLabelInstr());
            if (this->catchLabelStack)
            {
                this->catchLabelStack->Push(instr->m_prev->AsLabelInstr());
            }
            break;

        case Js::OpCode::TryCatch:
            if (this->catchLabelStack)
            {
                this->catchLabelStack->Pop();
            }
            break;

        case Js::OpCode::CloneBlockScope:
        case Js::OpCode::CloneInnerScopeSlots:
            // It would be nice to do this in IRBuilder, but doing so gives us
            // trouble when doing the DoSlotArrayCheck since it assume single def
            // of the sym to do its check properly. So instead we assign the dst
            // here in FlowGraph.
            instr->SetDst(instr->GetSrc1());
            break;

        }

        if (OpCodeAttr::UseAllFields(instr->m_opcode))
        {
            // UseAllFields opcode are call instruction or opcode that would call.
            hasCall = true;

            if (OpCodeAttr::CallInstr(instr->m_opcode))
            {
                if (!instr->isCallInstrProtectedByNoProfileBailout)
                {
                    instr->m_func->SetHasCallsOnSelfAndParents();
                }

                // For ARM & X64 because of their register calling convention
                // the ArgOuts need to be moved next to the call.
#if defined(_M_ARM) || defined(_M_X64)

                IR::Instr* argInsertInstr = instr;
                instr->IterateArgInstrs([&](IR::Instr* argInstr)
                {
                    if (argInstr->m_opcode != Js::OpCode::LdSpreadIndices &&
                        argInstr->m_opcode != Js::OpCode::ArgOut_A_Dynamic &&
                        argInstr->m_opcode != Js::OpCode::ArgOut_A_FromStackArgs &&
                        argInstr->m_opcode != Js::OpCode::ArgOut_A_SpreadArg)
                    {
                        // don't have bailout in asm.js so we don't need BytecodeArgOutCapture
                        if (!argInstr->m_func->GetJnFunction()->GetIsAsmjsMode())
                        {
                            // Need to always generate byte code arg out capture,
                            // because bailout can't restore from the arg out as it is
                            // replaced by new sym for register calling convention in lower
                            argInstr->GenerateBytecodeArgOutCapture();
                        }
                        // Check if the instruction is already next
                        if (argInstr != argInsertInstr->m_prev)
                        {
                            // It is not, move it.
                            argInstr->Move(argInsertInstr);
                        }
                        argInsertInstr = argInstr;
                    }
                    return false;
                });
#endif
            }
        }
    }
    NEXT_INSTR_IN_FUNC_BACKWARD_EDITING;
    this->func->isFlowGraphValid = true;
    Assert(!this->catchLabelStack || this->catchLabelStack->Empty());

    // We've been walking backward so that edge lists would be in the right order. Now walk the blocks
    // forward to number the blocks in lexical order.
    unsigned int blockNum = 0;
    FOREACH_BLOCK(block, this)
    {
        block->SetBlockNum(blockNum++);
    }NEXT_BLOCK;
    AssertMsg(blockNum == this->blockCount, "Block count is out of whack");

    this->RemoveUnreachableBlocks();

    this->FindLoops();

    bool breakBlocksRelocated = this->CanonicalizeLoops();

#if DBG
    this->VerifyLoopGraph();
#endif

    // Renumber the blocks. Break block remove code has likely inserted new basic blocks.
    blockNum = 0;

    // Regions need to be assigned before Globopt because:
    //     1. FullJit: The Backward Pass will set the write-through symbols on the regions and the forward pass will
    //        use this information to insert ToVars for those symbols. Also, for a symbol determined as write-through
    //        in the try region to be restored correctly by the bailout, it should not be removed from the
    //        byteCodeUpwardExposedUsed upon a def in the try region (the def might be preempted by an exception).
    //
    //     2. SimpleJit: Same case of correct restoration as above applies in SimpleJit too. However, the only bailout
    //        we have in Simple Jitted code right now is BailOnSimpleJitToFullJitLoopBody, installed in IRBuilder. So,
    //        for now, we can just check if the func has a bailout to assign regions pre globopt while running SimpleJit.

    bool assignRegionsBeforeGlobopt = this->func->HasTry() &&
                                (this->func->DoOptimizeTryCatch() || (this->func->IsSimpleJit() && this->func->hasBailout));
    Region ** blockToRegion = nullptr;
    if (assignRegionsBeforeGlobopt)
    {
        blockToRegion = JitAnewArrayZ(this->alloc, Region*, this->blockCount);
    }
    FOREACH_BLOCK_ALL(block, this)
    {
        block->SetBlockNum(blockNum++);
        if (assignRegionsBeforeGlobopt)
        {
            if (block->isDeleted && !block->isDead)
            {
                continue;
            }
            this->UpdateRegionForBlock(block, blockToRegion);
        }
    } NEXT_BLOCK_ALL;

    AssertMsg (blockNum == this->blockCount, "Block count is out of whack");

    if (breakBlocksRelocated)
    {
        // Sort loop lists only if there is break block removal.
        SortLoopLists();
    }
#if DBG_DUMP
    this->Dump(false, nullptr);
#endif
}

void
FlowGraph::SortLoopLists()
{
    // Sort the blocks in loopList
    for (Loop *loop = this->loopList; loop; loop = loop->next)
    {
        unsigned int lastBlockNumber = loop->GetHeadBlock()->GetBlockNum();
        // Insertion sort as the blockList is almost sorted in the loop.
        FOREACH_BLOCK_IN_LOOP_EDITING(block, loop, iter)
        {
            if (lastBlockNumber <= block->GetBlockNum())
            {
                lastBlockNumber = block->GetBlockNum();
            }
            else
            {
                iter.UnlinkCurrent();
                FOREACH_BLOCK_IN_LOOP_EDITING(insertBlock,loop,newIter)
                {
                    if (insertBlock->GetBlockNum() > block->GetBlockNum())
                    {
                        break;
                    }
                }NEXT_BLOCK_IN_LOOP_EDITING;
                newIter.InsertBefore(block);
            }
        }NEXT_BLOCK_IN_LOOP_EDITING;
    }
}

void
FlowGraph::RunPeeps()
{
    if (this->func->HasTry())
    {
        return;
    }

    if (PHASE_OFF(Js::FGPeepsPhase, this->func))
    {
        return;
    }

    IR::Instr * instrCm = nullptr;
    bool tryUnsignedCmpPeep = false;

    FOREACH_INSTR_IN_FUNC_EDITING(instr, instrNext, this->func)
    {
        switch(instr->m_opcode)
        {
        case Js::OpCode::Br:
        case Js::OpCode::BrEq_I4:
        case Js::OpCode::BrGe_I4:
        case Js::OpCode::BrGt_I4:
        case Js::OpCode::BrLt_I4:
        case Js::OpCode::BrLe_I4:
        case Js::OpCode::BrUnGe_I4:
        case Js::OpCode::BrUnGt_I4:
        case Js::OpCode::BrUnLt_I4:
        case Js::OpCode::BrUnLe_I4:
        case Js::OpCode::BrNeq_I4:
        case Js::OpCode::BrEq_A:
        case Js::OpCode::BrGe_A:
        case Js::OpCode::BrGt_A:
        case Js::OpCode::BrLt_A:
        case Js::OpCode::BrLe_A:
        case Js::OpCode::BrUnGe_A:
        case Js::OpCode::BrUnGt_A:
        case Js::OpCode::BrUnLt_A:
        case Js::OpCode::BrUnLe_A:
        case Js::OpCode::BrNotEq_A:
        case Js::OpCode::BrNotNeq_A:
        case Js::OpCode::BrSrNotEq_A:
        case Js::OpCode::BrSrNotNeq_A:
        case Js::OpCode::BrNotGe_A:
        case Js::OpCode::BrNotGt_A:
        case Js::OpCode::BrNotLt_A:
        case Js::OpCode::BrNotLe_A:
        case Js::OpCode::BrNeq_A:
        case Js::OpCode::BrNotNull_A:
        case Js::OpCode::BrNotAddr_A:
        case Js::OpCode::BrAddr_A:
        case Js::OpCode::BrSrEq_A:
        case Js::OpCode::BrSrNeq_A:
        case Js::OpCode::BrOnHasProperty:
        case Js::OpCode::BrOnNoProperty:
        case Js::OpCode::BrHasSideEffects:
        case Js::OpCode::BrNotHasSideEffects:
        case Js::OpCode::BrFncEqApply:
        case Js::OpCode::BrFncNeqApply:
        case Js::OpCode::BrOnEmpty:
        case Js::OpCode::BrOnNotEmpty:
        case Js::OpCode::BrFncCachedScopeEq:
        case Js::OpCode::BrFncCachedScopeNeq:
        case Js::OpCode::BrOnObject_A:
        case Js::OpCode::BrOnClassConstructor:
            if (tryUnsignedCmpPeep)
            {
                this->UnsignedCmpPeep(instr);
            }
            instrNext = Peeps::PeepBranch(instr->AsBranchInstr());
            break;

        case Js::OpCode::MultiBr:
            // TODO: Run peeps on these as well...
            break;

        case Js::OpCode::BrTrue_I4:
        case Js::OpCode::BrFalse_I4:
        case Js::OpCode::BrTrue_A:
        case Js::OpCode::BrFalse_A:
            if (instrCm)
            {
                if (instrCm->GetDst()->IsInt32())
                {
                    Assert(instr->m_opcode == Js::OpCode::BrTrue_I4 || instr->m_opcode == Js::OpCode::BrFalse_I4);
                    instrNext = this->PeepTypedCm(instrCm);
                }
                else
                {
                    instrNext = this->PeepCm(instrCm);
                }
                instrCm = nullptr;

                if (instrNext == nullptr)
                {
                    // Set instrNext back to the current instr.
                    instrNext = instr;
                }
            }
            else
            {
                instrNext = Peeps::PeepBranch(instr->AsBranchInstr());
            }
            break;

        case Js::OpCode::CmEq_I4:
        case Js::OpCode::CmGe_I4:
        case Js::OpCode::CmGt_I4:
        case Js::OpCode::CmLt_I4:
        case Js::OpCode::CmLe_I4:
        case Js::OpCode::CmNeq_I4:
        case Js::OpCode::CmEq_A:
        case Js::OpCode::CmGe_A:
        case Js::OpCode::CmGt_A:
        case Js::OpCode::CmLt_A:
        case Js::OpCode::CmLe_A:
        case Js::OpCode::CmNeq_A:
        case Js::OpCode::CmSrEq_A:
        case Js::OpCode::CmSrNeq_A:
            if (tryUnsignedCmpPeep)
            {
                this->UnsignedCmpPeep(instr);
            }
        case Js::OpCode::CmUnGe_I4:
        case Js::OpCode::CmUnGt_I4:
        case Js::OpCode::CmUnLt_I4:
        case Js::OpCode::CmUnLe_I4:
        case Js::OpCode::CmUnGe_A:
        case Js::OpCode::CmUnGt_A:
        case Js::OpCode::CmUnLt_A:
        case Js::OpCode::CmUnLe_A:
            // There may be useless branches between the Cm instr and the branch that uses the result.
            // So save the last Cm instr seen, and trigger the peep on the next BrTrue/BrFalse.
            instrCm = instr;
            break;

        case Js::OpCode::Label:
            if (instr->AsLabelInstr()->IsUnreferenced())
            {
                instrNext = Peeps::PeepUnreachableLabel(instr->AsLabelInstr(), false);
            }
            break;

        case Js::OpCode::StatementBoundary:
            instr->ClearByteCodeOffset();
            instr->SetByteCodeOffset(instr->GetNextRealInstrOrLabel());
            break;

        case Js::OpCode::ShrU_I4:
        case Js::OpCode::ShrU_A:
            if (tryUnsignedCmpPeep)
            {
                break;
            }
            if (instr->GetDst()->AsRegOpnd()->m_sym->IsSingleDef()
                && instr->GetSrc2()->IsRegOpnd() && instr->GetSrc2()->AsRegOpnd()->m_sym->IsTaggableIntConst()
                && instr->GetSrc2()->AsRegOpnd()->m_sym->GetIntConstValue() == 0)
            {
                tryUnsignedCmpPeep = true;
            }
            break;
        default:
            Assert(!instr->IsBranchInstr());
        }
   } NEXT_INSTR_IN_FUNC_EDITING;
}

void
Loop::InsertLandingPad(FlowGraph *fg)
{
    BasicBlock *headBlock = this->GetHeadBlock();

    // Always create a landing pad.  This allows globopt to easily hoist instructions
    // and re-optimize the block if needed.
    BasicBlock *landingPad = BasicBlock::New(fg);
    this->landingPad = landingPad;
    IR::Instr * headInstr = headBlock->GetFirstInstr();
    IR::LabelInstr *landingPadLabel = IR::LabelInstr::New(Js::OpCode::Label, headInstr->m_func);
    landingPadLabel->SetByteCodeOffset(headInstr);
    headInstr->InsertBefore(landingPadLabel);

    landingPadLabel->SetBasicBlock(landingPad);

    landingPad->SetBlockNum(fg->blockCount++);
    landingPad->SetFirstInstr(landingPadLabel);
    landingPad->SetLastInstr(landingPadLabel);

    landingPad->prev = headBlock->prev;
    landingPad->prev->next = landingPad;
    landingPad->next = headBlock;
    headBlock->prev = landingPad;

    Loop *parentLoop = this->parent;
    landingPad->loop = parentLoop;

    // We need to add this block to the block list of the parent loops
    while (parentLoop)
    {
        // Find the head block in the block list of the parent loop
        FOREACH_BLOCK_IN_LOOP_EDITING(block, parentLoop, iter)
        {
            if (block == headBlock)
            {
                // Add the landing pad to the block list
                iter.InsertBefore(landingPad);
                break;
            }
        } NEXT_BLOCK_IN_LOOP_EDITING;

        parentLoop = parentLoop->parent;
    }

    // Fix predecessor flow edges
    FOREACH_PREDECESSOR_EDGE_EDITING(edge, headBlock, iter)
    {
        // Make sure it isn't a back-edge
        if (edge->GetPred()->loop != this && !this->IsDescendentOrSelf(edge->GetPred()->loop))
        {
            if (edge->GetPred()->GetLastInstr()->IsBranchInstr() && headBlock->GetFirstInstr()->IsLabelInstr())
            {
                IR::BranchInstr *branch = edge->GetPred()->GetLastInstr()->AsBranchInstr();
                branch->ReplaceTarget(headBlock->GetFirstInstr()->AsLabelInstr(), landingPadLabel);
            }
            headBlock->UnlinkPred(edge->GetPred(), false);
            landingPad->AddPred(edge, fg);
            edge->SetSucc(landingPad);
        }
    } NEXT_PREDECESSOR_EDGE_EDITING;

    fg->AddEdge(landingPad, headBlock);
}

bool
Loop::RemoveBreakBlocks(FlowGraph *fg)
{
    bool breakBlockRelocated = false;
    if (PHASE_OFF(Js::RemoveBreakBlockPhase, fg->GetFunc()))
    {
        return false;
    }

    BasicBlock *loopTailBlock = nullptr;
    FOREACH_BLOCK_IN_LOOP(block, this)
    {
        loopTailBlock = block;
    }NEXT_BLOCK_IN_LOOP;

    AnalysisAssert(loopTailBlock);

    FOREACH_BLOCK_BACKWARD_IN_RANGE_EDITING(breakBlockEnd, loopTailBlock, this->GetHeadBlock(), blockPrev)
    {
        while (!this->IsDescendentOrSelf(breakBlockEnd->loop))
        {
            // Found at least one break block;
            breakBlockRelocated = true;

#if DBG
            breakBlockEnd->isBreakBlock = true;
#endif
            // Find the first block in this break block sequence.
            BasicBlock *breakBlockStart = breakBlockEnd;
            BasicBlock *breakBlockStartPrev = breakBlockEnd->GetPrev();

            // Walk back the blocks until we find a block which belongs to that block.
            // Note: We don't really care if there are break blocks corresponding to different loops. We move the blocks conservatively to the end of the loop.

            // Algorithm works on one loop at a time.
            while((breakBlockStartPrev->loop == breakBlockEnd->loop) || !this->IsDescendentOrSelf(breakBlockStartPrev->loop))
            {
                breakBlockStart = breakBlockStartPrev;
                breakBlockStartPrev = breakBlockStartPrev->GetPrev();
            }

#if DBG
            breakBlockStart->isBreakBlock = true; // Mark the first block as well.
#endif

            BasicBlock *exitLoopTail = loopTailBlock;
            // Move these break blocks to the tail of the loop.
            fg->MoveBlocksBefore(breakBlockStart, breakBlockEnd, exitLoopTail->next);

#if DBG_DUMP
            fg->Dump(true /*needs verbose flag*/, L"\n After Each iteration of canonicalization \n");
#endif
            // Again be conservative, there are edits to the loop graph. Start fresh for this loop.
            breakBlockEnd = loopTailBlock;
            blockPrev = breakBlockEnd->prev;
        }
    } NEXT_BLOCK_BACKWARD_IN_RANGE_EDITING;

    return breakBlockRelocated;
}

void
FlowGraph::MoveBlocksBefore(BasicBlock *blockStart, BasicBlock *blockEnd, BasicBlock *insertBlock)
{
    BasicBlock *srcPredBlock = blockStart->prev;
    BasicBlock *srcNextBlock = blockEnd->next;
    BasicBlock *dstPredBlock = insertBlock->prev;
    IR::Instr* dstPredBlockLastInstr = dstPredBlock->GetLastInstr();
    IR::Instr* blockEndLastInstr = blockEnd->GetLastInstr();

    // Fix block linkage
    srcPredBlock->next = srcNextBlock;
    srcNextBlock->prev = srcPredBlock;

    dstPredBlock->next = blockStart;
    insertBlock->prev = blockEnd;

    blockStart->prev = dstPredBlock;
    blockEnd->next = insertBlock;

    // Fix instruction linkage
    IR::Instr::MoveRangeAfter(blockStart->GetFirstInstr(), blockEndLastInstr, dstPredBlockLastInstr);

    // Fix instruction flow
    IR::Instr *srcLastInstr = srcPredBlock->GetLastInstr();
    if (srcLastInstr->IsBranchInstr() && srcLastInstr->AsBranchInstr()->HasFallThrough())
    {
        // There was a fallthrough in the break blocks original position.
        IR::BranchInstr *srcBranch = srcLastInstr->AsBranchInstr();
        IR::Instr *srcBranchNextInstr = srcBranch->GetNextRealInstrOrLabel();

        // Save the target and invert the branch.
        IR::LabelInstr *srcBranchTarget = srcBranch->GetTarget();
        srcPredBlock->InvertBranch(srcBranch);
        IR::LabelInstr *srcLabel = blockStart->GetFirstInstr()->AsLabelInstr();

        // Point the inverted branch to break block.
        srcBranch->SetTarget(srcLabel);

        if (srcBranchNextInstr != srcBranchTarget)
        {
            FlowEdge *srcEdge  = this->FindEdge(srcPredBlock, srcBranchTarget->GetBasicBlock());
            Assert(srcEdge);

            BasicBlock *compensationBlock = this->InsertCompensationCodeForBlockMove(srcEdge, true /*insert compensation block to loop list*/, false /*At source*/);
            Assert(compensationBlock);
        }
    }

    IR::Instr *dstLastInstr = dstPredBlockLastInstr;
    if (dstLastInstr->IsBranchInstr() && dstLastInstr->AsBranchInstr()->HasFallThrough())
    {
        //There is a fallthrough in the block after which break block is inserted.
        FlowEdge *dstEdge = this->FindEdge(dstPredBlock, blockEnd->GetNext());
        Assert(dstEdge);

        BasicBlock *compensationBlock = this->InsertCompensationCodeForBlockMove(dstEdge, true /*insert compensation block to loop list*/, true /*At sink*/);
        Assert(compensationBlock);
    }
}

FlowEdge *
FlowGraph::FindEdge(BasicBlock *predBlock, BasicBlock *succBlock)
{
        FlowEdge *srcEdge = nullptr;
        FOREACH_SUCCESSOR_EDGE(edge, predBlock)
        {
            if (edge->GetSucc() == succBlock)
            {
                srcEdge = edge;
                break;
            }
        } NEXT_SUCCESSOR_EDGE;

        return srcEdge;
}

void
BasicBlock::InvertBranch(IR::BranchInstr *branch)
{
    Assert(this->GetLastInstr() == branch);
    Assert(this->GetSuccList()->HasTwo());

    branch->Invert();
    this->GetSuccList()->Reverse();
}

bool
FlowGraph::CanonicalizeLoops()
{
    if (this->func->HasProfileInfo())
    {
        this->implicitCallFlags = this->func->GetProfileInfo()->GetImplicitCallFlags();
        for (Loop *loop = this->loopList; loop; loop = loop->next)
        {
            this->implicitCallFlags = (Js::ImplicitCallFlags)(this->implicitCallFlags | loop->GetImplicitCallFlags());
        }
    }

#if DBG_DUMP
    this->Dump(true, L"\n Before cannonicalizeLoops \n");
#endif

    bool breakBlockRelocated = false;

    for (Loop *loop = this->loopList; loop; loop = loop->next)
    {
        loop->InsertLandingPad(this);
        if (!this->func->HasTry() || this->func->DoOptimizeTryCatch())
        {
            bool relocated = loop->RemoveBreakBlocks(this);
            if (!breakBlockRelocated && relocated)
            {
                breakBlockRelocated  = true;
            }
        }
    }

#if DBG_DUMP
    this->Dump(true, L"\n After cannonicalizeLoops \n");
#endif

    return breakBlockRelocated;
}

// Find the loops in this function, build the loop structure, and build a linked
// list of the basic blocks in this loop (including blocks of inner loops). The
// list preserves the reverse post-order of the blocks in the flowgraph block list.
void
FlowGraph::FindLoops()
{
    if (!this->hasLoop)
    {
        return;
    }

    Func * func = this->func;

    FOREACH_BLOCK_BACKWARD_IN_FUNC(block, func)
    {
        if (block->loop != nullptr)
        {
            // Block already visited
            continue;
        }
        FOREACH_SUCCESSOR_BLOCK(succ, block)
        {
            if (succ->isLoopHeader && succ->loop == nullptr)
            {
                // Found a loop back-edge
                BuildLoop(succ, block);
            }
        } NEXT_SUCCESSOR_BLOCK;
        if (block->isLoopHeader && block->loop == nullptr)
        {
            // We would have built a loop for it if it was a loop...
            block->isLoopHeader = false;
            block->GetFirstInstr()->AsLabelInstr()->m_isLoopTop = false;
        }
    } NEXT_BLOCK_BACKWARD_IN_FUNC;
}

void
FlowGraph::BuildLoop(BasicBlock *headBlock, BasicBlock *tailBlock, Loop *parentLoop)
{
    // This function is recursive, so when jitting in the foreground, probe the stack
    if(!func->IsBackgroundJIT())
    {
        PROBE_STACK(func->GetScriptContext(), Js::Constants::MinStackDefault);
    }

    if (tailBlock->number < headBlock->number)
    {
        // Not a loop.  We didn't see any back-edge.
        headBlock->isLoopHeader = false;
        headBlock->GetFirstInstr()->AsLabelInstr()->m_isLoopTop = false;
        return;
    }

    Assert(headBlock->isLoopHeader);
    Loop *loop = JitAnewZ(this->GetFunc()->m_alloc, Loop, this->GetFunc()->m_alloc, this->GetFunc());
    loop->next = this->loopList;
    this->loopList = loop;
    headBlock->loop = loop;
    loop->headBlock = headBlock;
    loop->int32SymsOnEntry = nullptr;
    loop->lossyInt32SymsOnEntry = nullptr;

    // If parentLoop is a parent of loop, it's headBlock better appear first.
    if (parentLoop && loop->headBlock->number > parentLoop->headBlock->number)
    {
        loop->parent = parentLoop;
        parentLoop->isLeaf = false;
    }
    loop->hasDeadStoreCollectionPass = false;
    loop->hasDeadStorePrepass = false;
    loop->memOpInfo = nullptr;

    NoRecoverMemoryJitArenaAllocator tempAlloc(L"BE-LoopBuilder", this->func->m_alloc->GetPageAllocator(), Js::Throw::OutOfMemory);

    WalkLoopBlocks(tailBlock, loop, &tempAlloc);

    Assert(loop->GetHeadBlock() == headBlock);

    IR::LabelInstr * firstInstr = loop->GetLoopTopInstr();

    firstInstr->SetLoop(loop);

    if (firstInstr->IsProfiledLabelInstr())
    {
        loop->SetImplicitCallFlags(firstInstr->AsProfiledLabelInstr()->loopImplicitCallFlags);
        loop->SetLoopFlags(firstInstr->AsProfiledLabelInstr()->loopFlags);
    }
    else
    {
        // Didn't collect profile information, don't do optimizations
        loop->SetImplicitCallFlags(Js::ImplicitCall_All);
    }
}

Loop::MemCopyCandidate* Loop::MemOpCandidate::AsMemCopy()
{
    Assert(this->IsMemCopy());
    return (Loop::MemCopyCandidate*)this;
}

Loop::MemSetCandidate* Loop::MemOpCandidate::AsMemSet()
{
    Assert(this->IsMemSet());
    return (Loop::MemSetCandidate*)this;
}

bool Loop::EnsureMemOpVariablesInitialized()
{
    if (this->memOpInfo == nullptr)
    {
        JitArenaAllocator *allocator = this->GetFunc()->GetTopFunc()->m_fg->alloc;
        this->memOpInfo = JitAnewStruct(allocator, Loop::MemOpInfo);
        this->memOpInfo->inductionVariablesUsedAfterLoop = nullptr;
        this->memOpInfo->startIndexOpndCache[0] = nullptr;
        this->memOpInfo->startIndexOpndCache[1] = nullptr;
        this->memOpInfo->startIndexOpndCache[2] = nullptr;
        this->memOpInfo->startIndexOpndCache[3] = nullptr;
        if (this->GetLoopFlags().isInterpreted && !this->GetLoopFlags().memopMinCountReached)
        {
#if DBG_DUMP
            Func* func = this->GetFunc();
            if (Js::Configuration::Global.flags.Verbose && PHASE_TRACE(Js::MemOpPhase, func))
            {
                wchar_t debugStringBuffer[MAX_FUNCTION_BODY_DEBUG_STRING_SIZE];
                Output::Print(L"MemOp skipped: minimum loop count not reached: Function: %s %s,  Loop: %d\n",
                              func->GetJnFunction()->GetDisplayName(),
                              func->GetJnFunction()->GetDebugNumberSet(debugStringBuffer),
                              this->GetLoopNumber()
                              );
            }
#endif
            this->memOpInfo->doMemOp = false;
            this->memOpInfo->inductionVariableChangeInfoMap = nullptr;
            this->memOpInfo->inductionVariableOpndPerUnrollMap = nullptr;
            this->memOpInfo->candidates = nullptr;
            return false;
        }
        this->memOpInfo->doMemOp = true;
        this->memOpInfo->inductionVariableChangeInfoMap = JitAnew(allocator, Loop::InductionVariableChangeInfoMap, allocator);
        this->memOpInfo->inductionVariableOpndPerUnrollMap = JitAnew(allocator, Loop::InductionVariableOpndPerUnrollMap, allocator);
        this->memOpInfo->candidates = JitAnew(allocator, Loop::MemOpList, allocator);
    }
    return true;
}

// Walk the basic blocks backwards until we find the loop header.
// Mark basic blocks in the loop by looking at the predecessors
// of blocks known to be in the loop.
// Recurse on inner loops.
void
FlowGraph::WalkLoopBlocks(BasicBlock *block, Loop *loop, JitArenaAllocator *tempAlloc)
{
    AnalysisAssert(loop);

    BVSparse<JitArenaAllocator> *loopBlocksBv = JitAnew(tempAlloc, BVSparse<JitArenaAllocator>, tempAlloc);
    BasicBlock *tailBlock = block;
    BasicBlock *lastBlock;
    loopBlocksBv->Set(block->GetBlockNum());

    this->AddBlockToLoop(block, loop);

    if (block == loop->headBlock)
    {
        // Single block loop, we're done
        return;
    }

    do
    {
        BOOL isInLoop = loopBlocksBv->Test(block->GetBlockNum());

        FOREACH_SUCCESSOR_BLOCK(succ, block)
        {
            if (succ->isLoopHeader)
            {
                // Found a loop back-edge
                if (loop->headBlock == succ)
                {
                    isInLoop = true;
                }
                else if (succ->loop == nullptr || succ->loop->headBlock != succ)
                {
                    // Recurse on inner loop
                    BuildLoop(succ, block, isInLoop ? loop : nullptr);
                }
            }
        } NEXT_SUCCESSOR_BLOCK;

        if (isInLoop)
        {
            // This block is in the loop.  All of it's predecessors should be contained in the loop as well.
            FOREACH_PREDECESSOR_BLOCK(pred, block)
            {
                // Fix up loop parent if it isn't set already.
                // If pred->loop != loop, we're looking at an inner loop, which was already visited.
                // If pred->loop->parent == nullptr, this is the first time we see this loop from an outer
                // loop, so this must be an immediate child.
                if (pred->loop && pred->loop != loop && loop->headBlock->number < pred->loop->headBlock->number
                    && (pred->loop->parent == nullptr || pred->loop->parent->headBlock->number < loop->headBlock->number))
                {
                    pred->loop->parent = loop;
                    loop->isLeaf = false;
                    if (pred->loop->hasCall)
                    {
                        loop->SetHasCall();
                    }
                    loop->SetImplicitCallFlags(pred->loop->GetImplicitCallFlags());
                }
                // Add pred to loop bit vector
                loopBlocksBv->Set(pred->GetBlockNum());
            } NEXT_PREDECESSOR_BLOCK;

            if (block->loop == nullptr || block->loop->IsDescendentOrSelf(loop))
            {
                block->loop = loop;
            }

            if (block != tailBlock)
            {
                this->AddBlockToLoop(block, loop);
            }
        }
        lastBlock = block;
        block = block->GetPrev();
    } while (lastBlock != loop->headBlock);
}

// Add block to this loop, and it's parent loops.
void
FlowGraph::AddBlockToLoop(BasicBlock *block, Loop *loop)
{
    loop->blockList.Prepend(block);
    if (block->hasCall)
    {
        loop->SetHasCall();
    }
}

///----------------------------------------------------------------------------
///
/// FlowGraph::AddBlock
///
/// Finish processing of a new block: hook up successor arcs, note loops, etc.
///
///----------------------------------------------------------------------------
BasicBlock *
FlowGraph::AddBlock(
    IR::Instr * firstInstr,
    IR::Instr * lastInstr,
    BasicBlock * nextBlock)
{
    BasicBlock * block;
    IR::LabelInstr * labelInstr;

    if (firstInstr->IsLabelInstr())
    {
        labelInstr = firstInstr->AsLabelInstr();
    }
    else
    {
        labelInstr = IR::LabelInstr::New(Js::OpCode::Label, firstInstr->m_func);
        labelInstr->SetByteCodeOffset(firstInstr);
        if (firstInstr->IsEntryInstr())
        {
            firstInstr->InsertAfter(labelInstr);
        }
        else
        {
            firstInstr->InsertBefore(labelInstr);
        }
        firstInstr = labelInstr;
    }

    block = labelInstr->GetBasicBlock();
    if (block == nullptr)
    {
        block = BasicBlock::New(this);
        labelInstr->SetBasicBlock(block);
        // Remember last block in function to target successor of RETs.
        if (!this->tailBlock)
        {
            this->tailBlock = block;
        }
    }

    // Hook up the successor edges
    if (lastInstr->EndsBasicBlock())
    {
        BasicBlock * blockTarget = nullptr;

        if (lastInstr->IsBranchInstr())
        {
            // Hook up a successor edge to the branch target.
            IR::BranchInstr * branchInstr = lastInstr->AsBranchInstr();

            if(branchInstr->IsMultiBranch())
            {
                BasicBlock * blockMultiBrTarget;

                IR::MultiBranchInstr * multiBranchInstr = branchInstr->AsMultiBrInstr();

                multiBranchInstr->MapUniqueMultiBrLabels([&](IR::LabelInstr * labelInstr) -> void
                {
                    blockMultiBrTarget = SetBlockTargetAndLoopFlag(labelInstr);
                    this->AddEdge(block, blockMultiBrTarget);
                });
            }
            else
            {
                IR::LabelInstr * labelInstr = branchInstr->GetTarget();
                blockTarget = SetBlockTargetAndLoopFlag(labelInstr);
                if (branchInstr->IsConditional())
                {
                    IR::Instr *instrNext = branchInstr->GetNextRealInstrOrLabel();

                    if (instrNext->IsLabelInstr())
                    {
                        SetBlockTargetAndLoopFlag(instrNext->AsLabelInstr());
                    }
                }
            }
        }
        else if (lastInstr->m_opcode == Js::OpCode::Ret && block != this->tailBlock)
        {
            blockTarget = this->tailBlock;
        }

        if (blockTarget)
        {
            this->AddEdge(block, blockTarget);
        }
    }

    if (lastInstr->HasFallThrough())
    {
        // Add a branch to next instruction so that we don't have to update the flow graph
        // when the glob opt tries to insert instructions.
        // We don't run the globopt with try/catch, don't need to insert branch to next for fall through blocks.
        if (!this->func->HasTry() && !lastInstr->IsBranchInstr())
        {
            IR::BranchInstr * instr = IR::BranchInstr::New(Js::OpCode::Br,
                lastInstr->m_next->AsLabelInstr(), lastInstr->m_func);
            instr->SetByteCodeOffset(lastInstr->m_next);
            lastInstr->InsertAfter(instr);
            lastInstr = instr;
        }
        this->AddEdge(block, nextBlock);
    }

    block->SetBlockNum(this->blockCount++);
    block->SetFirstInstr(firstInstr);
    block->SetLastInstr(lastInstr);

    if (this->blockList)
    {
        this->blockList->prev = block;
    }
    block->next = this->blockList;
    this->blockList = block;

    return block;
}

BasicBlock *
FlowGraph::SetBlockTargetAndLoopFlag(IR::LabelInstr * labelInstr)
{
    BasicBlock * blockTarget = nullptr;
    blockTarget = labelInstr->GetBasicBlock();

    if (blockTarget == nullptr)
    {
        blockTarget = BasicBlock::New(this);
        labelInstr->SetBasicBlock(blockTarget);
    }
    if (labelInstr->m_isLoopTop)
    {
        blockTarget->isLoopHeader = true;
        this->hasLoop = true;
    }

    return blockTarget;
}

///----------------------------------------------------------------------------
///
/// FlowGraph::AddEdge
///
/// Add an edge connecting the two given blocks.
///
///----------------------------------------------------------------------------
FlowEdge *
FlowGraph::AddEdge(BasicBlock * blockPred, BasicBlock * blockSucc)
{
    FlowEdge * edge = FlowEdge::New(this);
    edge->SetPred(blockPred);
    edge->SetSucc(blockSucc);
    blockPred->AddSucc(edge, this);
    blockSucc->AddPred(edge, this);

    return edge;
}

///----------------------------------------------------------------------------
///
/// FlowGraph::Destroy
///
/// Remove all references to FG structures from the IR in preparation for freeing
/// the FG.
///
///----------------------------------------------------------------------------
void
FlowGraph::Destroy(void)
{
    BOOL fHasTry = this->func->HasTry();
    Region ** blockToRegion = nullptr;
    if (fHasTry)
    {
        blockToRegion = JitAnewArrayZ(this->alloc, Region*, this->blockCount);
        // Do unreachable code removal up front to avoid problems
        // with unreachable back edges, etc.
        this->RemoveUnreachableBlocks();
    }

    FOREACH_BLOCK_ALL(block, this)
    {
        IR::Instr * firstInstr = block->GetFirstInstr();
        if (block->isDeleted && !block->isDead)
        {
            if (firstInstr->IsLabelInstr())
            {
                IR::LabelInstr * labelInstr = firstInstr->AsLabelInstr();
                labelInstr->UnlinkBasicBlock();
                // Removing the label for non try blocks as we have a deleted block which has the label instruction
                // still not removed; this prevents the assert for cases where the deleted blocks fall through to a helper block,
                // i.e. helper introduced by polymorphic inlining bailout.
                // Skipping Try blocks as we have dependency on blocks to get the last instr(see below in this function)
                if (!fHasTry)
                {
                    if (this->func->GetJnFunction()->IsGenerator())
                    {
                        // the label could be a yield resume label, in which case we also need to remove it from the YieldOffsetResumeLabels list
                        this->func->MapUntilYieldOffsetResumeLabels([this, &labelInstr](int i, const YieldOffsetResumeLabel& yorl)
                        {
                            if (labelInstr == yorl.Second())
                            {
                                labelInstr->m_hasNonBranchRef = false;
                                this->func->RemoveYieldOffsetResumeLabel(yorl);
                                return true;
                            }
                            return false;
                        });
                    }

                    Assert(labelInstr->IsUnreferenced());
                    labelInstr->Remove();
                }
            }
            continue;
        }

        if (block->isLoopHeader && !block->isDead)
        {
            // Mark the tail block of this loop (the last back-edge).  The register allocator
            // uses this to lexically find loops.
            BasicBlock *loopTail = nullptr;

            AssertMsg(firstInstr->IsLabelInstr() && firstInstr->AsLabelInstr()->m_isLoopTop,
                "Label not marked as loop top...");
            FOREACH_BLOCK_IN_LOOP(loopBlock, block->loop)
            {
                FOREACH_SUCCESSOR_BLOCK(succ, loopBlock)
                {
                    if (succ == block)
                    {
                        loopTail = loopBlock;
                        break;
                    }
                } NEXT_SUCCESSOR_BLOCK;
            } NEXT_BLOCK_IN_LOOP;

            if (loopTail)
            {
                AssertMsg(loopTail->GetLastInstr()->IsBranchInstr(), "LastInstr of loop should always be a branch no?");
                block->loop->SetLoopTopInstr(block->GetFirstInstr()->AsLabelInstr());
            }
            else
            {
                // This loop doesn't have a back-edge: that is, it is not a loop
                // anymore...
                firstInstr->AsLabelInstr()->m_isLoopTop = FALSE;
            }
        }

        if (fHasTry)
        {
            this->UpdateRegionForBlock(block, blockToRegion);
        }

        if (firstInstr->IsLabelInstr())
        {
            IR::LabelInstr * labelInstr = firstInstr->AsLabelInstr();
            labelInstr->UnlinkBasicBlock();
            if (labelInstr->IsUnreferenced() && !fHasTry)
            {
                // This is an unreferenced label, probably added by FG building.
                // Delete it now to make extended basic blocks visible.
                if (firstInstr == block->GetLastInstr())
                {
                    labelInstr->Remove();
                    continue;
                }
                else
                {
                    labelInstr->Remove();
                }
            }
        }

        // We don't run the globopt with try/catch, don't need to remove branch to next for fall through blocks
        IR::Instr * lastInstr = block->GetLastInstr();
        if (!fHasTry && lastInstr->IsBranchInstr())
        {
            IR::BranchInstr * branchInstr = lastInstr->AsBranchInstr();
            if (!branchInstr->IsConditional() && branchInstr->GetTarget() == branchInstr->m_next)
            {
                // Remove branch to next
                branchInstr->Remove();
            }
        }
    }
    NEXT_BLOCK;

#if DBG

    if (fHasTry)
    {
        // Now that all blocks have regions, we should see consistently propagated regions at all
        // block boundaries.
        FOREACH_BLOCK(block, this)
        {
            Region * region = blockToRegion[block->GetBlockNum()];
            Region * predRegion = nullptr;
            FOREACH_PREDECESSOR_BLOCK(predBlock, block)
            {
                predRegion = blockToRegion[predBlock->GetBlockNum()];
                if (predBlock->GetLastInstr() == nullptr)
                {
                    AssertMsg(region == predRegion, "Bad region propagation through empty block");
                }
                else
                {
                    switch (predBlock->GetLastInstr()->m_opcode)
                    {
                    case Js::OpCode::TryCatch:
                    case Js::OpCode::TryFinally:
                        AssertMsg(region->GetParent() == predRegion, "Bad region prop on entry to try-catch/finally");
                        if (block->GetFirstInstr() == predBlock->GetLastInstr()->AsBranchInstr()->GetTarget())
                        {
                            if (predBlock->GetLastInstr()->m_opcode == Js::OpCode::TryCatch)
                            {
                                AssertMsg(region->GetType() == RegionTypeCatch, "Bad region type on entry to catch");
                            }
                            else
                            {
                                AssertMsg(region->GetType() == RegionTypeFinally, "Bad region type on entry to finally");
                            }
                        }
                        else
                        {
                            AssertMsg(region->GetType() == RegionTypeTry, "Bad region type on entry to try");
                        }
                        break;
                    case Js::OpCode::Leave:
                    case Js::OpCode::LeaveNull:
                        AssertMsg(region == predRegion->GetParent() || (region == predRegion && this->func->IsLoopBodyInTry()), "Bad region prop on leaving try-catch/finally");
                        break;

                    // If the try region has a branch out of the loop,
                    // - the branch is moved out of the loop as part of break block removal, and
                    // - BrOnException is inverted to BrOnNoException and a Br is inserted after it.
                    // Otherwise,
                    // - FullJit: BrOnException is removed in the forward pass.
                    case Js::OpCode::BrOnException:
                        Assert(!this->func->DoGlobOpt());
                    case Js::OpCode::BrOnNoException:
                        Assert(this->func->HasTry() &&
                               ((!this->func->HasFinally() && !this->func->IsLoopBody() && !PHASE_OFF(Js::OptimizeTryCatchPhase, this->func)) ||
                               (this->func->IsSimpleJit() && this->func->GetJnFunction()->DoJITLoopBody()))); // should be relaxed as more bailouts are added in Simple Jit

                        Assert(region->GetType() == RegionTypeTry || region->GetType() == RegionTypeCatch);
                        if (region->GetType() == RegionTypeCatch)
                        {
                            Assert((predRegion->GetType() == RegionTypeTry) || (predRegion->GetType() == RegionTypeCatch));
                        }
                        else if (region->GetType() == RegionTypeTry)
                        {
                            Assert(region == predRegion);
                        }
                        break;
                    case Js::OpCode::Br:
                        if (region->GetType() == RegionTypeCatch && region != predRegion)
                        {
                            AssertMsg(predRegion->GetType() == RegionTypeTry, "Bad region type for the try");
                        }
                        else
                        {
                            AssertMsg(region == predRegion, "Bad region propagation through interior block");
                        }
                        break;
                    default:
                        AssertMsg(region == predRegion, "Bad region propagation through interior block");
                        break;
                    }
                }
            }
            NEXT_PREDECESSOR_BLOCK;

            switch (region->GetType())
            {
            case RegionTypeRoot:
                Assert(!region->GetMatchingTryRegion() && !region->GetMatchingCatchRegion() && !region->GetMatchingFinallyRegion());
                break;

            case RegionTypeTry:
                Assert(!(region->GetMatchingCatchRegion() && region->GetMatchingFinallyRegion()));
                break;

            case RegionTypeCatch:
            case RegionTypeFinally:
                Assert(region->GetMatchingTryRegion());
                break;
            }
        }
        NEXT_BLOCK;
        FOREACH_BLOCK_DEAD_OR_ALIVE(block, this)
        {
            if (block->GetFirstInstr()->IsLabelInstr())
            {
                IR::LabelInstr *labelInstr = block->GetFirstInstr()->AsLabelInstr();
                if (labelInstr->IsUnreferenced())
                {
                    // This is an unreferenced label, probably added by FG building.
                    // Delete it now to make extended basic blocks visible.
                    labelInstr->Remove();
                }
            }
        } NEXT_BLOCK_DEAD_OR_ALIVE;
    }
#endif

    this->func->isFlowGraphValid = false;
}

// Propagate the region forward from the block's predecessor(s), tracking the effect
// of the flow transition. Record the region in the block-to-region map provided
// and on the label at the entry to the block (if any).
void
FlowGraph::UpdateRegionForBlock(BasicBlock * block, Region ** blockToRegion)
{
    Region *region;
    Region * predRegion = nullptr;
    IR::Instr * tryInstr = nullptr;
    IR::Instr * firstInstr = block->GetFirstInstr();
    if (firstInstr->IsLabelInstr() && firstInstr->AsLabelInstr()->GetRegion())
    {
        Assert(this->func->HasTry() && (this->func->DoOptimizeTryCatch() || (this->func->IsSimpleJit() && this->func->hasBailout)));
        blockToRegion[block->GetBlockNum()] = firstInstr->AsLabelInstr()->GetRegion();
        return;
    }

    if (block == this->blockList)
    {
        // Head of the graph: create the root region.
        region = Region::New(RegionTypeRoot, nullptr, this->func);
    }
    else
    {
        // Propagate the region forward by finding a predecessor we've already processed.
        // We require that there be one, since we've already removed unreachable blocks.
        region = nullptr;
        FOREACH_PREDECESSOR_BLOCK(predBlock, block)
        {
            AssertMsg(predBlock->GetBlockNum() < this->blockCount, "Misnumbered block at teardown time?");
            predRegion = blockToRegion[predBlock->GetBlockNum()];
            if (predRegion != nullptr)
            {
                region = this->PropagateRegionFromPred(block, predBlock, predRegion, tryInstr);
                break;
            }
        }
        NEXT_PREDECESSOR_BLOCK;
    }

    AnalysisAssertMsg(region != nullptr, "Failed to find region for block");
    if (!region->ehBailoutData)
    {
        region->AllocateEHBailoutData(this->func, tryInstr);
    }

    // Record the region in the block-to-region map.
    blockToRegion[block->GetBlockNum()] = region;
    if (firstInstr->IsLabelInstr())
    {
        // Record the region on the label and make sure it stays around as a region
        // marker if we're entering a region at this point.
        IR::LabelInstr * labelInstr = firstInstr->AsLabelInstr();
        labelInstr->SetRegion(region);
        if (region != predRegion)
        {
            labelInstr->m_hasNonBranchRef = true;
        }
    }
}

Region *
FlowGraph::PropagateRegionFromPred(BasicBlock * block, BasicBlock * predBlock, Region * predRegion, IR::Instr * &tryInstr)
{
    // Propagate predRegion to region, looking at the flow transition for an opcode
    // that affects the region.
    Region * region = nullptr;
    IR::Instr * predLastInstr = predBlock->GetLastInstr();
    IR::Instr * firstInstr = block->GetFirstInstr();
    if (predLastInstr == nullptr)
    {
        // Empty block: trivially propagate the region.
        region = predRegion;
    }
    else
    {
        Region * tryRegion = nullptr;
        IR::LabelInstr * tryInstrNext = nullptr;
        switch (predLastInstr->m_opcode)
        {
        case Js::OpCode::TryCatch:
            // Entry to a try-catch. See whether we're entering the try or the catch
            // by looking for the handler label.
            Assert(predLastInstr->m_next->IsLabelInstr());
            tryInstrNext = predLastInstr->m_next->AsLabelInstr();
            tryRegion = tryInstrNext->GetRegion();

            if (firstInstr == predLastInstr->AsBranchInstr()->GetTarget())
            {
                region = Region::New(RegionTypeCatch, predRegion, this->func);
                Assert(tryRegion);
                region->SetMatchingTryRegion(tryRegion);
                tryRegion->SetMatchingCatchRegion(region);
            }
            else
            {
                region = Region::New(RegionTypeTry, predRegion, this->func);
                tryInstr = predLastInstr;
            }
            break;

        case Js::OpCode::TryFinally:
            // Entry to a try-finally. See whether we're entering the try or the finally
            // by looking for the handler label.
            Assert(predLastInstr->m_next->IsLabelInstr());
            tryInstrNext = predLastInstr->m_next->AsLabelInstr();
            tryRegion = tryInstrNext->GetRegion();

            if (firstInstr == predLastInstr->AsBranchInstr()->GetTarget())
            {
                region = Region::New(RegionTypeFinally, predRegion, this->func);
                Assert(tryRegion);
                region->SetMatchingTryRegion(tryRegion);
                tryRegion->SetMatchingFinallyRegion(region);
            }
            else
            {
                region = Region::New(RegionTypeTry, predRegion, this->func);
                tryInstr = predLastInstr;
            }
            break;

        case Js::OpCode::Leave:
        case Js::OpCode::LeaveNull:
            // Exiting a try or handler. Retrieve the current region's parent.
            region = predRegion->GetParent();
            if (region == nullptr)
            {
                // We found a Leave in the root region- this can only happen when a jitted loop body
                // in a try block has a return statement.
                Assert(this->func->IsLoopBodyInTry());
                predLastInstr->AsBranchInstr()->m_isOrphanedLeave = true;
                region = predRegion;
            }
            break;

        default:
            // Normal (non-EH) transition: just propagate the region.
            region = predRegion;
            break;
        }
    }
    return region;
}

void
FlowGraph::InsertCompBlockToLoopList(Loop *loop, BasicBlock* compBlock, BasicBlock* targetBlock, bool postTarget)
{
    if (loop)
    {
        bool found = false;
        FOREACH_BLOCK_IN_LOOP_EDITING(loopBlock, loop, iter)
        {
            if (loopBlock == targetBlock)
            {
                found = true;
                break;
            }
        } NEXT_BLOCK_IN_LOOP_EDITING;
        if (found)
        {
            if (postTarget)
            {
                iter.Next();
            }
            iter.InsertBefore(compBlock);
        }
        InsertCompBlockToLoopList(loop->parent, compBlock, targetBlock, postTarget);
    }
}

// Insert a block on the given edge
BasicBlock *
FlowGraph::InsertAirlockBlock(FlowEdge * edge)
{
    BasicBlock * airlockBlock = BasicBlock::New(this);
    BasicBlock * sourceBlock = edge->GetPred();
    BasicBlock * sinkBlock = edge->GetSucc();

    BasicBlock * sinkPrevBlock = sinkBlock->prev;
    IR::Instr *  sinkPrevBlockLastInstr = sinkPrevBlock->GetLastInstr();
    IR::Instr * sourceLastInstr = sourceBlock->GetLastInstr();

    airlockBlock->loop = sinkBlock->loop;
    airlockBlock->SetBlockNum(this->blockCount++);
#ifdef DBG
    airlockBlock->isAirLockBlock = true;
#endif
    //
    // Fixup block linkage
    //

    // airlock block is inserted right before sourceBlock
    airlockBlock->prev = sinkBlock->prev;
    sinkBlock->prev = airlockBlock;

    airlockBlock->next = sinkBlock;
    airlockBlock->prev->next = airlockBlock;

    //
    // Fixup flow edges
    //

    sourceBlock->RemoveSucc(sinkBlock, this, false);

    // Add sourceBlock -> airlockBlock
    this->AddEdge(sourceBlock, airlockBlock);

    // Add airlockBlock -> sinkBlock
    edge->SetPred(airlockBlock);
    airlockBlock->AddSucc(edge, this);

    // Fixup data use count
    airlockBlock->SetDataUseCount(1);
    sourceBlock->DecrementDataUseCount();

    //
    // Fixup IR
    //

    // Maintain the instruction region for inlining
    IR::LabelInstr *sinkLabel = sinkBlock->GetFirstInstr()->AsLabelInstr();
    Func * sinkLabelFunc = sinkLabel->m_func;

    IR::LabelInstr *airlockLabel = IR::LabelInstr::New(Js::OpCode::Label, sinkLabelFunc);

    sinkPrevBlockLastInstr->InsertAfter(airlockLabel);

    airlockBlock->SetFirstInstr(airlockLabel);
    airlockLabel->SetBasicBlock(airlockBlock);

    // Add br to sinkBlock from airlock block
    IR::BranchInstr *airlockBr = IR::BranchInstr::New(Js::OpCode::Br, sinkLabel, sinkLabelFunc);
    airlockBr->SetByteCodeOffset(sinkLabel);
    airlockLabel->InsertAfter(airlockBr);
    airlockBlock->SetLastInstr(airlockBr);

    airlockLabel->SetByteCodeOffset(sinkLabel);

    // Fixup flow out of sourceBlock
    IR::BranchInstr *sourceBr = sourceLastInstr->AsBranchInstr();
    if (sourceBr->IsMultiBranch())
    {
        const bool replaced = sourceBr->AsMultiBrInstr()->ReplaceTarget(sinkLabel, airlockLabel);
        Assert(replaced);
    }
    else if (sourceBr->GetTarget() == sinkLabel)
    {
        sourceBr->SetTarget(airlockLabel);
    }

    if (!sinkPrevBlockLastInstr->IsBranchInstr() || sinkPrevBlockLastInstr->AsBranchInstr()->HasFallThrough())
    {
        if (!sinkPrevBlock->isDeleted)
        {
            FlowEdge *dstEdge = this->FindEdge(sinkPrevBlock, sinkBlock);
            if (dstEdge) // Possibility that sourceblock may be same as sinkPrevBlock
            {
                BasicBlock* compensationBlock = this->InsertCompensationCodeForBlockMove(dstEdge, true /*insert comp block to loop list*/, true);
                compensationBlock->IncrementDataUseCount();
                // We need to skip airlock compensation block in globopt as its inserted while globopt is iteration over the blocks.
                compensationBlock->isAirLockCompensationBlock = true;
            }
        }
    }

#if DBG_DUMP
    this->Dump(true, L"\n After insertion of airlock block \n");
#endif

    return airlockBlock;
}

// Insert a block on the given edge
BasicBlock *
FlowGraph::InsertCompensationCodeForBlockMove(FlowEdge * edge,  bool insertToLoopList, bool sinkBlockLoop)
{
    BasicBlock * compBlock = BasicBlock::New(this);
    BasicBlock * sourceBlock = edge->GetPred();
    BasicBlock * sinkBlock = edge->GetSucc();
    BasicBlock * fallthroughBlock = sourceBlock->next;
    IR::Instr *  sourceLastInstr = sourceBlock->GetLastInstr();

    compBlock->SetBlockNum(this->blockCount++);

    if (insertToLoopList)
    {
        // For flow graph edits in
        if (sinkBlockLoop)
        {
            if (sinkBlock->loop && sinkBlock->loop->GetHeadBlock() == sinkBlock)
            {
                // BLUE 531255: sinkblock may be the head block of new loop, we shouldn't insert compensation block to that loop
                // Insert it to all the parent loop lists.
                compBlock->loop = sinkBlock->loop->parent;
                InsertCompBlockToLoopList(compBlock->loop, compBlock, sinkBlock, false);
            }
            else
            {
                compBlock->loop = sinkBlock->loop;
                InsertCompBlockToLoopList(compBlock->loop, compBlock, sinkBlock, false); // sinkBlock or fallthroughBlock?
            }
#ifdef DBG
            compBlock->isBreakCompensationBlockAtSink = true;
#endif
        }
        else
        {
            compBlock->loop = sourceBlock->loop;
            InsertCompBlockToLoopList(compBlock->loop, compBlock, sourceBlock, true);
#ifdef DBG
            compBlock->isBreakCompensationBlockAtSource = true;
#endif
        }
    }

    //
    // Fixup block linkage
    //

    // compensation block is inserted right after sourceBlock
    compBlock->next = fallthroughBlock;
    fallthroughBlock->prev = compBlock;

    compBlock->prev = sourceBlock;
    sourceBlock->next = compBlock;

    //
    // Fixup flow edges
    //
    sourceBlock->RemoveSucc(sinkBlock, this, false);

    // Add sourceBlock -> airlockBlock
    this->AddEdge(sourceBlock, compBlock);

    // Add airlockBlock -> sinkBlock
    edge->SetPred(compBlock);
    compBlock->AddSucc(edge, this);

    //
    // Fixup IR
    //

    // Maintain the instruction region for inlining
    IR::LabelInstr *sinkLabel = sinkBlock->GetFirstInstr()->AsLabelInstr();
    Func * sinkLabelFunc = sinkLabel->m_func;

    IR::LabelInstr *compLabel = IR::LabelInstr::New(Js::OpCode::Label, sinkLabelFunc);
    sourceLastInstr->InsertAfter(compLabel);
    compBlock->SetFirstInstr(compLabel);
    compLabel->SetBasicBlock(compBlock);

    // Add br to sinkBlock from compensation block
    IR::BranchInstr *compBr = IR::BranchInstr::New(Js::OpCode::Br, sinkLabel, sinkLabelFunc);
    compBr->SetByteCodeOffset(sinkLabel);
    compLabel->InsertAfter(compBr);
    compBlock->SetLastInstr(compBr);

    compLabel->SetByteCodeOffset(sinkLabel);

    // Fixup flow out of sourceBlock
    if (sourceLastInstr->IsBranchInstr())
    {
        IR::BranchInstr *sourceBr = sourceLastInstr->AsBranchInstr();
        Assert(sourceBr->IsMultiBranch() || sourceBr->IsConditional());
        if (sourceBr->IsMultiBranch())
        {
            const bool replaced = sourceBr->AsMultiBrInstr()->ReplaceTarget(sinkLabel, compLabel);
            Assert(replaced);
        }
    }

    return compBlock;
}

void
FlowGraph::RemoveUnreachableBlocks()
{
    AnalysisAssert(this->blockList);

    FOREACH_BLOCK(block, this)
    {
        block->isVisited = false;
    }
    NEXT_BLOCK;

    this->blockList->isVisited = true;

    FOREACH_BLOCK_EDITING(block, this)
    {
        if (block->isVisited)
        {
            FOREACH_SUCCESSOR_BLOCK(succ, block)
            {
                succ->isVisited = true;
            } NEXT_SUCCESSOR_BLOCK;
        }
        else
        {
            this->RemoveBlock(block);
        }
    }
    NEXT_BLOCK_EDITING;
}

// If block has no predecessor, remove it.
bool
FlowGraph::RemoveUnreachableBlock(BasicBlock *block, GlobOpt * globOpt)
{
    bool isDead = false;

    if ((block->GetPredList() == nullptr || block->GetPredList()->Empty()) && block != this->func->m_fg->blockList)
    {
        isDead = true;
    }
    else if (block->isLoopHeader)
    {
        // A dead loop still has back-edges pointing to it...
        isDead = true;
        FOREACH_PREDECESSOR_BLOCK(pred, block)
        {
            if (!block->loop->IsDescendentOrSelf(pred->loop))
            {
                isDead = false;
            }
        } NEXT_PREDECESSOR_BLOCK;
    }

    if (isDead)
    {
        this->RemoveBlock(block, globOpt);
        return true;
    }
    return false;
}

IR::Instr *
FlowGraph::PeepTypedCm(IR::Instr *instr)
{
    // Basic pattern, peep:
    //      t1 = CmEq a, b
    //      BrTrue_I4 $L1, t1
    // Into:
    //      t1 = 1
    //      BrEq $L1, a, b
    //      t1 = 0

    IR::Instr * instrNext = instr->GetNextRealInstrOrLabel();

    // find intermediate Lds
    IR::Instr * instrLd = nullptr;
    if (instrNext->m_opcode == Js::OpCode::Ld_I4)
    {
        instrLd = instrNext;
        instrNext = instrNext->GetNextRealInstrOrLabel();
    }

    IR::Instr * instrLd2 = nullptr;
    if (instrNext->m_opcode == Js::OpCode::Ld_I4)
    {
        instrLd2 = instrNext;
        instrNext = instrNext->GetNextRealInstrOrLabel();
    }

    // Find BrTrue/BrFalse
    IR::Instr *instrBr;
    bool brIsTrue;
    if (instrNext->m_opcode == Js::OpCode::BrTrue_I4)
    {
        instrBr = instrNext;
        brIsTrue = true;
    }
    else if (instrNext->m_opcode == Js::OpCode::BrFalse_I4)
    {
        instrBr = instrNext;
        brIsTrue = false;
    }
    else
    {
        return nullptr;
    }

    // if we have intermediate Lds, then make sure pattern is:
    //      t1 = CmEq a, b
    //      t2 = Ld_A t1
    //      BrTrue $L1, t2
    if (instrLd && !instrLd->GetSrc1()->IsEqual(instr->GetDst()))
    {
        return nullptr;
    }

    if (instrLd2 && !instrLd2->GetSrc1()->IsEqual(instrLd->GetDst()))
    {
        return nullptr;
    }

    // Make sure we have:
    //      t1 = CmEq a, b
    //           BrTrue/BrFalse t1
    if (!(instr->GetDst()->IsEqual(instrBr->GetSrc1()) || (instrLd && instrLd->GetDst()->IsEqual(instrBr->GetSrc1())) || (instrLd2 && instrLd2->GetDst()->IsEqual(instrBr->GetSrc1()))))
    {
        return nullptr;
    }

    IR::Opnd * src1 = instr->UnlinkSrc1();
    IR::Opnd * src2 = instr->UnlinkSrc2();

    IR::Instr * instrNew;
    IR::Opnd * tmpOpnd;
    if (instr->GetDst()->IsEqual(src1) || (instrLd && instrLd->GetDst()->IsEqual(src1)) || (instrLd2 && instrLd2->GetDst()->IsEqual(src1)))
    {
        Assert(src1->IsInt32());

        tmpOpnd = IR::RegOpnd::New(TyInt32, instr->m_func);
        instrNew = IR::Instr::New(Js::OpCode::Ld_I4, tmpOpnd, src1, instr->m_func);
        instrNew->SetByteCodeOffset(instr);
        instr->InsertBefore(instrNew);
        src1 = tmpOpnd;
    }

    if (instr->GetDst()->IsEqual(src2) || (instrLd && instrLd->GetDst()->IsEqual(src2)) || (instrLd2 && instrLd2->GetDst()->IsEqual(src2)))
    {
        Assert(src2->IsInt32());

        tmpOpnd = IR::RegOpnd::New(TyInt32, instr->m_func);
        instrNew = IR::Instr::New(Js::OpCode::Ld_I4, tmpOpnd, src2, instr->m_func);
        instrNew->SetByteCodeOffset(instr);
        instr->InsertBefore(instrNew);
        src2 = tmpOpnd;
    }

    instrBr->ReplaceSrc1(src1);
    instrBr->SetSrc2(src2);

    Js::OpCode newOpcode;
    switch (instr->m_opcode)
    {
    case Js::OpCode::CmEq_I4:
        newOpcode = Js::OpCode::BrEq_I4;
        break;
    case Js::OpCode::CmGe_I4:
        newOpcode = Js::OpCode::BrGe_I4;
        break;
    case Js::OpCode::CmGt_I4:
        newOpcode = Js::OpCode::BrGt_I4;
        break;
    case Js::OpCode::CmLt_I4:
        newOpcode = Js::OpCode::BrLt_I4;
        break;
    case Js::OpCode::CmLe_I4:
        newOpcode = Js::OpCode::BrLe_I4;
        break;
    case Js::OpCode::CmUnGe_I4:
        newOpcode = Js::OpCode::BrUnGe_I4;
        break;
    case Js::OpCode::CmUnGt_I4:
        newOpcode = Js::OpCode::BrUnGt_I4;
        break;
    case Js::OpCode::CmUnLt_I4:
        newOpcode = Js::OpCode::BrUnLt_I4;
        break;
    case Js::OpCode::CmUnLe_I4:
        newOpcode = Js::OpCode::BrUnLe_I4;
        break;
    case Js::OpCode::CmNeq_I4:
        newOpcode = Js::OpCode::BrNeq_I4;
        break;
    case Js::OpCode::CmEq_A:
        newOpcode = Js::OpCode::BrEq_A;
        break;
    case Js::OpCode::CmGe_A:
        newOpcode = Js::OpCode::BrGe_A;
        break;
    case Js::OpCode::CmGt_A:
        newOpcode = Js::OpCode::BrGt_A;
        break;
    case Js::OpCode::CmLt_A:
        newOpcode = Js::OpCode::BrLt_A;
        break;
    case Js::OpCode::CmLe_A:
        newOpcode = Js::OpCode::BrLe_A;
        break;
    case Js::OpCode::CmUnGe_A:
        newOpcode = Js::OpCode::BrUnGe_A;
        break;
    case Js::OpCode::CmUnGt_A:
        newOpcode = Js::OpCode::BrUnGt_A;
        break;
    case Js::OpCode::CmUnLt_A:
        newOpcode = Js::OpCode::BrUnLt_A;
        break;
    case Js::OpCode::CmUnLe_A:
        newOpcode = Js::OpCode::BrUnLe_A;
        break;
    case Js::OpCode::CmNeq_A:
        newOpcode = Js::OpCode::BrNeq_A;
        break;
    case Js::OpCode::CmSrEq_A:
        newOpcode = Js::OpCode::BrSrEq_A;
        break;
    case Js::OpCode::CmSrNeq_A:
        newOpcode = Js::OpCode::BrSrNeq_A;
        break;
    default:
        newOpcode = Js::OpCode::InvalidOpCode;
        Assume(UNREACHED);
    }

    instrBr->m_opcode = newOpcode;

    if (brIsTrue)
    {
        instr->SetSrc1(IR::IntConstOpnd::New(1, TyInt8, instr->m_func));
        instr->m_opcode = Js::OpCode::Ld_I4;
        instrNew = IR::Instr::New(Js::OpCode::Ld_I4, instr->GetDst(), IR::IntConstOpnd::New(0, TyInt8, instr->m_func), instr->m_func);
        instrNew->SetByteCodeOffset(instrBr);
        instrBr->InsertAfter(instrNew);
        if (instrLd)
        {
            instrLd->ReplaceSrc1(IR::IntConstOpnd::New(1, TyInt8, instr->m_func));
            instrNew = IR::Instr::New(Js::OpCode::Ld_I4, instrLd->GetDst(), IR::IntConstOpnd::New(0, TyInt8, instr->m_func), instr->m_func);
            instrNew->SetByteCodeOffset(instrBr);
            instrBr->InsertAfter(instrNew);

            if (instrLd2)
            {
                instrLd2->ReplaceSrc1(IR::IntConstOpnd::New(1, TyInt8, instr->m_func));
                instrNew = IR::Instr::New(Js::OpCode::Ld_I4, instrLd2->GetDst(), IR::IntConstOpnd::New(0, TyInt8, instr->m_func), instr->m_func);
                instrNew->SetByteCodeOffset(instrBr);
                instrBr->InsertAfter(instrNew);
            }
        }
    }
    else
    {
        instrBr->AsBranchInstr()->Invert();

        instr->SetSrc1(IR::IntConstOpnd::New(0, TyInt8, instr->m_func));
        instr->m_opcode = Js::OpCode::Ld_I4;
        instrNew = IR::Instr::New(Js::OpCode::Ld_I4, instr->GetDst(), IR::IntConstOpnd::New(1, TyInt8, instr->m_func), instr->m_func);
        instrNew->SetByteCodeOffset(instrBr);
        instrBr->InsertAfter(instrNew);
        if (instrLd)
        {
            instrLd->ReplaceSrc1(IR::IntConstOpnd::New(0, TyInt8, instr->m_func));
            instrNew = IR::Instr::New(Js::OpCode::Ld_I4, instrLd->GetDst(), IR::IntConstOpnd::New(1, TyInt8, instr->m_func), instr->m_func);
            instrNew->SetByteCodeOffset(instrBr);
            instrBr->InsertAfter(instrNew);

            if (instrLd2)
            {
                instrLd2->ReplaceSrc1(IR::IntConstOpnd::New(0, TyInt8, instr->m_func));
                instrNew = IR::Instr::New(Js::OpCode::Ld_I4, instrLd2->GetDst(), IR::IntConstOpnd::New(1, TyInt8, instr->m_func), instr->m_func);
                instrNew->SetByteCodeOffset(instrBr);
                instrBr->InsertAfter(instrNew);
            }
        }
    }

    return instrBr;
}

IR::Instr *
FlowGraph::PeepCm(IR::Instr *instr)
{
    // Basic pattern, peep:
    //      t1 = CmEq a, b
    //      t2 = Ld_A t1
    //      BrTrue $L1, t2
    // Into:
    //      t1 = True
    //      t2 = True
    //      BrEq $L1, a, b
    //      t1 = False
    //      t2 = False
    //
    //  The true/false Ld_A's will most likely end up being dead-stores...

    //  Alternate Pattern
    //      t1= CmEq a, b
    //      BrTrue $L1, t1
    // Into:
    //      BrEq $L1, a, b

    Func *func = instr->m_func;

    // Find Ld_A
    IR::Instr *instrNext = instr->GetNextRealInstrOrLabel();
    IR::Instr *inlineeEndInstr = nullptr;
    IR::Instr *instrNew;
    IR::Instr *instrLd = nullptr, *instrLd2 = nullptr;
    IR::Instr *instrByteCode = instrNext;
    bool ldFound = false;
    IR::Opnd *brSrc = instr->GetDst();

    if (instrNext->m_opcode == Js::OpCode::Ld_A && instrNext->GetSrc1()->IsEqual(instr->GetDst()))
    {
        ldFound = true;
        instrLd = instrNext;
        brSrc = instrNext->GetDst();

        if (brSrc->IsEqual(instr->GetSrc1()) || brSrc->IsEqual(instr->GetSrc2()))
        {
            return nullptr;
        }

        instrNext = instrLd->GetNextRealInstrOrLabel();

        // Is there a second Ld_A?
        if (instrNext->m_opcode == Js::OpCode::Ld_A && instrNext->GetSrc1()->IsEqual(brSrc))
        {
            // We have:
            //      t1 = Cm
            //      t2 = t1     // ldSrc = t1
            //      t3 = t2     // ldDst = t3
            //      BrTrue/BrFalse t3

            instrLd2 = instrNext;
            brSrc = instrLd2->GetDst();
            instrNext = instrLd2->GetNextRealInstrOrLabel();
            if (brSrc->IsEqual(instr->GetSrc1()) || brSrc->IsEqual(instr->GetSrc2()))
            {
                return nullptr;
            }
        }
    }

    // Skip over InlineeEnd
    if (instrNext->m_opcode == Js::OpCode::InlineeEnd)
    {
        inlineeEndInstr = instrNext;
        instrNext = inlineeEndInstr->GetNextRealInstrOrLabel();
    }

    // Find BrTrue/BrFalse
    bool brIsTrue;
    if (instrNext->m_opcode == Js::OpCode::BrTrue_A)
    {
        brIsTrue = true;
    }
    else if (instrNext->m_opcode == Js::OpCode::BrFalse_A)
    {
        brIsTrue = false;
    }
    else
    {
        return nullptr;
    }

    IR::Instr *instrBr = instrNext;

    // Make sure we have:
    //      t1 = Ld_A
    //         BrTrue/BrFalse t1
    if (!instr->GetDst()->IsEqual(instrBr->GetSrc1()) && !brSrc->IsEqual(instrBr->GetSrc1()))
    {
        return nullptr;
    }

    //
    // We have a match.  Generate the new branch
    //

    // BrTrue/BrFalse t1
    // Keep a copy of the inliner func and the bytecode offset of the original BrTrue/BrFalse if we end up inserting a new branch out of the inlinee,
    // and sym id of t1 for proper restoration on a bailout before the branch.
    Func* origBrFunc = instrBr->m_func;
    uint32 origBrByteCodeOffset = instrBr->GetByteCodeOffset();
    uint32 origBranchSrcSymId = instrBr->GetSrc1()->GetStackSym()->m_id;

    instrBr->Unlink();
    instr->InsertBefore(instrBr);
    instrBr->ClearByteCodeOffset();
    instrBr->SetByteCodeOffset(instr);
    instrBr->FreeSrc1();
    instrBr->SetSrc1(instr->UnlinkSrc1());
    instrBr->SetSrc2(instr->UnlinkSrc2());
    instrBr->m_func = instr->m_func;

    Js::OpCode newOpcode;

    switch(instr->m_opcode)
    {
    case Js::OpCode::CmEq_A:
        newOpcode = Js::OpCode::BrEq_A;
        break;
    case Js::OpCode::CmGe_A:
        newOpcode = Js::OpCode::BrGe_A;
        break;
    case Js::OpCode::CmGt_A:
        newOpcode = Js::OpCode::BrGt_A;
        break;
    case Js::OpCode::CmLt_A:
        newOpcode = Js::OpCode::BrLt_A;
        break;
    case Js::OpCode::CmLe_A:
        newOpcode = Js::OpCode::BrLe_A;
        break;
    case Js::OpCode::CmUnGe_A:
        newOpcode = Js::OpCode::BrUnGe_A;
        break;
    case Js::OpCode::CmUnGt_A:
        newOpcode = Js::OpCode::BrUnGt_A;
        break;
    case Js::OpCode::CmUnLt_A:
        newOpcode = Js::OpCode::BrUnLt_A;
        break;
    case Js::OpCode::CmUnLe_A:
        newOpcode = Js::OpCode::BrUnLe_A;
        break;
    case Js::OpCode::CmNeq_A:
        newOpcode = Js::OpCode::BrNeq_A;
        break;
    case Js::OpCode::CmSrEq_A:
        newOpcode = Js::OpCode::BrSrEq_A;
        break;
    case Js::OpCode::CmSrNeq_A:
        newOpcode = Js::OpCode::BrSrNeq_A;
        break;
    default:
        Assert(UNREACHED);
        __assume(UNREACHED);
    }

    instrBr->m_opcode = newOpcode;

    IR::AddrOpnd* trueOpnd = IR::AddrOpnd::New(func->GetScriptContext()->GetLibrary()->GetTrue(), IR::AddrOpndKindDynamicVar, func, true);
    IR::AddrOpnd* falseOpnd = IR::AddrOpnd::New(func->GetScriptContext()->GetLibrary()->GetFalse(), IR::AddrOpndKindDynamicVar, func, true);

    trueOpnd->SetValueType(ValueType::Boolean);
    falseOpnd->SetValueType(ValueType::Boolean);

    if (ldFound)
    {
        // Split Ld_A into "Ld_A TRUE"/"Ld_A FALSE"
        if (brIsTrue)
        {
            instrNew = IR::Instr::New(Js::OpCode::Ld_A, instrLd->GetSrc1(), trueOpnd, instrBr->m_func);
            instrNew->SetByteCodeOffset(instrBr);
            instrNew->GetDst()->AsRegOpnd()->m_fgPeepTmp = true;
            instrBr->InsertBefore(instrNew);
            instrNew = IR::Instr::New(Js::OpCode::Ld_A, instrLd->GetDst(), trueOpnd, instrBr->m_func);
            instrNew->SetByteCodeOffset(instrBr);
            instrNew->GetDst()->AsRegOpnd()->m_fgPeepTmp = true;
            instrBr->InsertBefore(instrNew);

            instrNew = IR::Instr::New(Js::OpCode::Ld_A, instrLd->GetSrc1(), falseOpnd, instrLd->m_func);
            instrLd->InsertBefore(instrNew);
            instrNew->SetByteCodeOffset(instrLd);
            instrNew->GetDst()->AsRegOpnd()->m_fgPeepTmp = true;
            instrLd->ReplaceSrc1(falseOpnd);

            if (instrLd2)
            {
                instrLd2->ReplaceSrc1(falseOpnd);

                instrNew = IR::Instr::New(Js::OpCode::Ld_A, instrLd2->GetDst(), trueOpnd, instrBr->m_func);
                instrBr->InsertBefore(instrNew);
                instrNew->SetByteCodeOffset(instrBr);
                instrNew->GetDst()->AsRegOpnd()->m_fgPeepTmp = true;
            }
        }
        else
        {
            instrBr->AsBranchInstr()->Invert();

            instrNew = IR::Instr::New(Js::OpCode::Ld_A, instrLd->GetSrc1(), falseOpnd, instrBr->m_func);
            instrBr->InsertBefore(instrNew);
            instrNew->SetByteCodeOffset(instrBr);
            instrNew->GetDst()->AsRegOpnd()->m_fgPeepTmp = true;
            instrNew = IR::Instr::New(Js::OpCode::Ld_A, instrLd->GetDst(), falseOpnd, instrBr->m_func);
            instrBr->InsertBefore(instrNew);
            instrNew->SetByteCodeOffset(instrBr);
            instrNew->GetDst()->AsRegOpnd()->m_fgPeepTmp = true;

            instrNew = IR::Instr::New(Js::OpCode::Ld_A, instrLd->GetSrc1(), trueOpnd, instrLd->m_func);
            instrLd->InsertBefore(instrNew);
            instrNew->SetByteCodeOffset(instrLd);
            instrLd->ReplaceSrc1(trueOpnd);
            instrNew->GetDst()->AsRegOpnd()->m_fgPeepTmp = true;

            if (instrLd2)
            {
                instrLd2->ReplaceSrc1(trueOpnd);
                instrNew = IR::Instr::New(Js::OpCode::Ld_A, instrLd->GetSrc1(), trueOpnd, instrBr->m_func);
                instrBr->InsertBefore(instrNew);
                instrNew->SetByteCodeOffset(instrBr);
                instrNew->GetDst()->AsRegOpnd()->m_fgPeepTmp = true;
            }
        }
    }

    // Fix InlineeEnd
    if (inlineeEndInstr)
    {
        this->InsertInlineeOnFLowEdge(instrBr->AsBranchInstr(), inlineeEndInstr, instrByteCode , origBrFunc, origBrByteCodeOffset, origBranchSrcSymId);
    }

    if (instr->GetDst()->AsRegOpnd()->m_sym->HasByteCodeRegSlot())
    {
        Assert(!instrBr->AsBranchInstr()->HasByteCodeReg());
        StackSym *dstSym = instr->GetDst()->AsRegOpnd()->m_sym;
        instrBr->AsBranchInstr()->SetByteCodeReg(dstSym->GetByteCodeRegSlot());
    }
    instr->Remove();

    //
    // Try optimizing through a second branch.
    // Peep:
    //
    //      t2 = True;
    //      BrTrue  $L1
    //      ...
    //   L1:
    //      t1 = Ld_A t2
    //      BrTrue  $L2
    //
    // Into:
    //      t2 = True;
    //      t1 = True;
    //      BrTrue  $L2 <---
    //      ...
    //   L1:
    //      t1 = Ld_A t2
    //      BrTrue  $L2
    //
    // This cleanup helps expose second level Cm peeps.

    IR::Instr *instrLd3 = instrBr->AsBranchInstr()->GetTarget()->GetNextRealInstrOrLabel();

    // Skip over branch to branch
    while (instrLd3->m_opcode == Js::OpCode::Br)
    {
        instrLd3 = instrLd3->AsBranchInstr()->GetTarget()->GetNextRealInstrOrLabel();
    }

    // Find Ld_A
    if (instrLd3->m_opcode != Js::OpCode::Ld_A)
    {
        return instrBr;
    }

    IR::Instr *instrBr2 = instrLd3->GetNextRealInstrOrLabel();
    IR::Instr *inlineeEndInstr2 = nullptr;

    // InlineeEnd?
    // REVIEW: Can we handle 2 inlineeEnds?
    if (instrBr2->m_opcode == Js::OpCode::InlineeEnd && !inlineeEndInstr)
    {
        inlineeEndInstr2 = instrBr2;
        instrBr2 = instrBr2->GetNextRealInstrOrLabel();
    }

    // Find branch
    bool brIsTrue2;
    if (instrBr2->m_opcode == Js::OpCode::BrTrue_A)
    {
        brIsTrue2 = true;
    }
    else if (instrBr2->m_opcode == Js::OpCode::BrFalse_A)
    {
        brIsTrue2 = false;
    }
    else
    {
        return nullptr;
    }

    // Make sure Ld_A operates on the right tmps.
    if (!instrLd3->GetDst()->IsEqual(instrBr2->GetSrc1()) || !brSrc->IsEqual(instrLd3->GetSrc1()))
    {
        return nullptr;
    }

    if (instrLd3->GetDst()->IsEqual(instrBr->GetSrc1()) || instrLd3->GetDst()->IsEqual(instrBr->GetSrc2()))
    {
        return nullptr;
    }

    // Make sure that the reg we're assigning to is not live in the intervening instructions (if this is a forward branch).
    if (instrLd3->GetByteCodeOffset() > instrBr->GetByteCodeOffset())
    {
        StackSym *symLd3 = instrLd3->GetDst()->AsRegOpnd()->m_sym;
        if (IR::Instr::FindRegUseInRange(symLd3, instrBr->m_next, instrLd3))
        {
            return nullptr;
        }
    }

    //
    // We have a match!
    //

    if(inlineeEndInstr2)
    {
        origBrFunc = instrBr2->m_func;
        origBrByteCodeOffset = instrBr2->GetByteCodeOffset();
        origBranchSrcSymId = instrBr2->GetSrc1()->GetStackSym()->m_id;
    }

    // Fix Ld_A
    if (brIsTrue)
    {
        instrNew = IR::Instr::New(Js::OpCode::Ld_A, instrLd3->GetDst(), trueOpnd, instrBr->m_func);
        instrBr->InsertBefore(instrNew);
        instrNew->SetByteCodeOffset(instrBr);
        instrNew->GetDst()->AsRegOpnd()->m_fgPeepTmp = true;
    }
    else
    {
        instrNew = IR::Instr::New(Js::OpCode::Ld_A, instrLd3->GetDst(), falseOpnd, instrBr->m_func);
        instrBr->InsertBefore(instrNew);
        instrNew->SetByteCodeOffset(instrBr);
        instrNew->GetDst()->AsRegOpnd()->m_fgPeepTmp = true;
    }

    IR::LabelInstr *brTarget2;

    // Retarget branch
    if (brIsTrue2 == brIsTrue)
    {
        brTarget2 = instrBr2->AsBranchInstr()->GetTarget();
    }
    else
    {
        brTarget2 = IR::LabelInstr::New(Js::OpCode::Label, instrBr2->m_func);
        brTarget2->SetByteCodeOffset(instrBr2->m_next);
        instrBr2->InsertAfter(brTarget2);
    }

    instrBr->AsBranchInstr()->SetTarget(brTarget2);

    // InlineeEnd?
    if (inlineeEndInstr2)
    {
        this->InsertInlineeOnFLowEdge(instrBr->AsBranchInstr(), inlineeEndInstr2, instrByteCode, origBrFunc, origBrByteCodeOffset, origBranchSrcSymId);
    }

    return instrBr;
}

void
FlowGraph::InsertInlineeOnFLowEdge(IR::BranchInstr *instrBr, IR::Instr *inlineeEndInstr, IR::Instr *instrBytecode, Func* origBrFunc, uint32 origByteCodeOffset, uint32 origBranchSrcSymId)
{
    // Helper for PeepsCm code.
    //
    // We've skipped some InlineeEnd.  Globopt expects to see these
    // on all flow paths out of the inlinee.  Insert an InlineeEnd
    // on the new path:
    //      BrEq $L1, a, b
    // Becomes:
    //      BrNeq $L2, a, b
    //      InlineeEnd
    //      Br $L1
    //  L2:

    instrBr->AsBranchInstr()->Invert();

    IR::BranchInstr *newBr = IR::BranchInstr::New(Js::OpCode::Br, instrBr->AsBranchInstr()->GetTarget(), origBrFunc);
    newBr->SetByteCodeOffset(origByteCodeOffset);
    instrBr->InsertAfter(newBr);

    IR::LabelInstr *newLabel = IR::LabelInstr::New(Js::OpCode::Label, instrBr->m_func);
    newLabel->SetByteCodeOffset(instrBytecode);
    newBr->InsertAfter(newLabel);
    instrBr->AsBranchInstr()->SetTarget(newLabel);

    IR::Instr *newInlineeEnd = IR::Instr::New(Js::OpCode::InlineeEnd, inlineeEndInstr->m_func);
    newInlineeEnd->SetSrc1(inlineeEndInstr->GetSrc1());
    newInlineeEnd->SetSrc2(inlineeEndInstr->GetSrc2());
    newInlineeEnd->SetByteCodeOffset(instrBytecode);
    newInlineeEnd->SetIsCloned(true);  // Mark it as cloned - this is used later by the inlinee args optimization
    newBr->InsertBefore(newInlineeEnd);

    IR::ByteCodeUsesInstr * useOrigBranchSrcInstr = IR::ByteCodeUsesInstr::New(origBrFunc);
    useOrigBranchSrcInstr->SetByteCodeOffset(origByteCodeOffset);
    useOrigBranchSrcInstr->byteCodeUpwardExposedUsed = JitAnew(origBrFunc->m_alloc, BVSparse<JitArenaAllocator>,origBrFunc->m_alloc);
    useOrigBranchSrcInstr->byteCodeUpwardExposedUsed->Set(origBranchSrcSymId);
    newBr->InsertBefore(useOrigBranchSrcInstr);

    uint newBrFnNumber = newBr->m_func->m_workItem->GetFunctionNumber();
    Assert(newBrFnNumber == origBrFunc->m_workItem->GetFunctionNumber());

    // The function numbers of the new branch and the inlineeEnd instruction should be different (ensuring that the new branch is not added in the inlinee but in the inliner).
    // Only case when they can be same is recursive calls - inlinee and inliner are the same function
    Assert(newBrFnNumber != inlineeEndInstr->m_func->m_workItem->GetFunctionNumber() ||
        newBrFnNumber == inlineeEndInstr->m_func->GetParentFunc()->m_workItem->GetFunctionNumber());
}

BasicBlock *
BasicBlock::New(FlowGraph * graph)
{
    BasicBlock * block;

    block = JitAnew(graph->alloc, BasicBlock, graph->alloc, graph->GetFunc());

    return block;
}

void
BasicBlock::AddPred(FlowEdge * edge, FlowGraph * graph)
{
    this->predList.Prepend(graph->alloc, edge);
}

void
BasicBlock::AddSucc(FlowEdge * edge, FlowGraph * graph)
{
    this->succList.Prepend(graph->alloc, edge);
}

void
BasicBlock::RemovePred(BasicBlock *block, FlowGraph * graph)
{
    this->RemovePred(block, graph, true, false);
}

void
BasicBlock::RemoveSucc(BasicBlock *block, FlowGraph * graph)
{
    this->RemoveSucc(block, graph, true, false);
}

void
BasicBlock::RemoveDeadPred(BasicBlock *block, FlowGraph * graph)
{
    this->RemovePred(block, graph, true, true);
}

void
BasicBlock::RemoveDeadSucc(BasicBlock *block, FlowGraph * graph)
{
    this->RemoveSucc(block, graph, true, true);
}

void
BasicBlock::RemovePred(BasicBlock *block, FlowGraph * graph, bool doCleanSucc, bool moveToDead)
{
    FOREACH_SLISTBASECOUNTED_ENTRY_EDITING(FlowEdge*, edge, this->GetPredList(), iter)
    {
        if (edge->GetPred() == block)
        {
            BasicBlock *blockSucc = edge->GetSucc();
            if (moveToDead)
            {
                iter.MoveCurrentTo(this->GetDeadPredList());

            }
            else
            {
                iter.RemoveCurrent(graph->alloc);
            }
            if (doCleanSucc)
            {
                block->RemoveSucc(this, graph, false, moveToDead);
            }
            if (blockSucc->isLoopHeader && blockSucc->loop && blockSucc->GetPredList()->HasOne())
            {
                Loop *loop = blockSucc->loop;
                loop->isDead = true;
            }
            return;
        }
    } NEXT_SLISTBASECOUNTED_ENTRY_EDITING;
    AssertMsg(UNREACHED, "Edge not found.");
}

void
BasicBlock::RemoveSucc(BasicBlock *block, FlowGraph * graph, bool doCleanPred, bool moveToDead)
{
    FOREACH_SLISTBASECOUNTED_ENTRY_EDITING(FlowEdge*, edge, this->GetSuccList(), iter)
    {
        if (edge->GetSucc() == block)
        {
            if (moveToDead)
            {
                iter.MoveCurrentTo(this->GetDeadSuccList());
            }
            else
            {
                iter.RemoveCurrent(graph->alloc);
            }

            if (doCleanPred)
            {
                block->RemovePred(this, graph, false, moveToDead);
            }

            if (block->isLoopHeader && block->loop && block->GetPredList()->HasOne())
            {
                Loop *loop = block->loop;
                loop->isDead = true;
            }
            return;
        }
    } NEXT_SLISTBASECOUNTED_ENTRY_EDITING;
    AssertMsg(UNREACHED, "Edge not found.");
}

void
BasicBlock::UnlinkPred(BasicBlock *block)
{
    this->UnlinkPred(block, true);
}

void
BasicBlock::UnlinkSucc(BasicBlock *block)
{
    this->UnlinkSucc(block, true);
}

void
BasicBlock::UnlinkPred(BasicBlock *block, bool doCleanSucc)
{
    FOREACH_SLISTBASECOUNTED_ENTRY_EDITING(FlowEdge*, edge, this->GetPredList(), iter)
    {
        if (edge->GetPred() == block)
        {
            iter.UnlinkCurrent();
            if (doCleanSucc)
            {
                block->UnlinkSucc(this, false);
            }
            return;
        }
    } NEXT_SLISTBASECOUNTED_ENTRY_EDITING;
    AssertMsg(UNREACHED, "Edge not found.");
}

void
BasicBlock::UnlinkSucc(BasicBlock *block, bool doCleanPred)
{
    FOREACH_SLISTBASECOUNTED_ENTRY_EDITING(FlowEdge*, edge, this->GetSuccList(), iter)
    {
        if (edge->GetSucc() == block)
        {
            iter.UnlinkCurrent();
            if (doCleanPred)
            {
                block->UnlinkPred(this, false);
            }
            return;
        }
    } NEXT_SLISTBASECOUNTED_ENTRY_EDITING;
    AssertMsg(UNREACHED, "Edge not found.");
}

bool
BasicBlock::IsLandingPad()
{
    BasicBlock * nextBlock = this->GetNext();
    return nextBlock && nextBlock->loop && nextBlock->isLoopHeader && nextBlock->loop->landingPad == this;
}

IR::Instr *
FlowGraph::RemoveInstr(IR::Instr *instr, GlobOpt * globOpt)
{
    IR::Instr *instrPrev = instr->m_prev;
    if (globOpt)
    {
        // Removing block during glob opt.  Need to maintain the graph so that
        // bailout will record the byte code use in case the dead code is exposed
        // by dyno-pogo optimization (where bailout need the byte code uses from
        // the dead blocks where it may not be dead after bailing out)
        if (instr->IsLabelInstr())
        {
            instr->AsLabelInstr()->m_isLoopTop = false;
            return instr;
        }
        else if (instr->IsByteCodeUsesInstr())
        {
            return instr;
        }

        Js::OpCode opcode = instr->m_opcode;
        IR::ByteCodeUsesInstr * newByteCodeUseInstr = globOpt->ConvertToByteCodeUses(instr);
        if (newByteCodeUseInstr != nullptr)
        {
            // We don't care about property used in these instruction
            // It is only necessary for field copy prop so that we will keep the implicit call
            // up to the copy prop location.
            newByteCodeUseInstr->propertySymUse = nullptr;

            if (opcode == Js::OpCode::Yield)
            {
                IR::Instr *instrLabel = newByteCodeUseInstr->m_next;
                while (instrLabel->m_opcode != Js::OpCode::Label)
                {
                    instrLabel = instrLabel->m_next;
                }
                func->RemoveDeadYieldOffsetResumeLabel(instrLabel->AsLabelInstr());
                instrLabel->AsLabelInstr()->m_hasNonBranchRef = false;
            }

            // Save the last instruction to update the block with
            return newByteCodeUseInstr;
        }
        else
        {
            return instrPrev;
        }
    }
    else
    {
        instr->Remove();
        return instrPrev;
    }
}

void
FlowGraph::RemoveBlock(BasicBlock *block, GlobOpt * globOpt, bool tailDuping)
{
    Assert(!block->isDead && !block->isDeleted);
    IR::Instr * lastInstr = nullptr;
    FOREACH_INSTR_IN_BLOCK_EDITING(instr, instrNext, block)
    {
        if (instr->m_opcode == Js::OpCode::FunctionExit)
        {
            // Removing FunctionExit causes problems downstream...
            // We could change the opcode, or have FunctionEpilog/FunctionExit to get
            // rid of the epilog.
            break;
        }
        if (instr == block->GetFirstInstr())
        {
            Assert(instr->IsLabelInstr());
            instr->AsLabelInstr()->m_isLoopTop = false;
        }
        else
        {
            lastInstr = this->RemoveInstr(instr, globOpt);
        }
    } NEXT_INSTR_IN_BLOCK_EDITING;

    if (lastInstr)
    {
        block->SetLastInstr(lastInstr);
    }
    FOREACH_SLISTBASECOUNTED_ENTRY(FlowEdge*, edge, block->GetPredList())
    {
        edge->GetPred()->RemoveSucc(block, this, false, globOpt != nullptr);
    } NEXT_SLISTBASECOUNTED_ENTRY;

    FOREACH_SLISTBASECOUNTED_ENTRY(FlowEdge*, edge, block->GetSuccList())
    {
        edge->GetSucc()->RemovePred(block, this, false, globOpt != nullptr);
    } NEXT_SLISTBASECOUNTED_ENTRY;

    if (block->isLoopHeader && this->loopList)
    {
        // If loop graph is built, remove loop from loopList
        Loop **pPrevLoop = &this->loopList;

        while (*pPrevLoop != block->loop)
        {
            pPrevLoop = &((*pPrevLoop)->next);
        }
        *pPrevLoop = (*pPrevLoop)->next;
        this->hasLoop = (this->loopList != nullptr);
    }
    if (globOpt != nullptr)
    {
        block->isDead = true;
        block->GetPredList()->MoveTo(block->GetDeadPredList());
        block->GetSuccList()->MoveTo(block->GetDeadSuccList());
    }
    if (tailDuping)
    {
        block->isDead = true;
    }
    block->isDeleted = true;
    block->SetDataUseCount(0);
}

void
BasicBlock::UnlinkInstr(IR::Instr * instr)
{
    Assert(this->Contains(instr));
    Assert(this->GetFirstInstr() != this->GetLastInstr());
    if (instr == this->GetFirstInstr())
    {
        Assert(!this->GetFirstInstr()->IsLabelInstr());
        this->SetFirstInstr(instr->m_next);
    }
    else if (instr == this->GetLastInstr())
    {
        this->SetLastInstr(instr->m_prev);
    }

    instr->Unlink();
}

void
BasicBlock::RemoveInstr(IR::Instr * instr)
{
    Assert(this->Contains(instr));
    if (instr == this->GetFirstInstr())
    {
        this->SetFirstInstr(instr->m_next);
    }
    else if (instr == this->GetLastInstr())
    {
        this->SetLastInstr(instr->m_prev);
    }

    instr->Remove();
}

void
BasicBlock::InsertInstrBefore(IR::Instr *newInstr, IR::Instr *beforeThisInstr)
{
    Assert(this->Contains(beforeThisInstr));
    beforeThisInstr->InsertBefore(newInstr);

    if(this->GetFirstInstr() == beforeThisInstr)
    {
        Assert(!beforeThisInstr->IsLabelInstr());
        this->SetFirstInstr(newInstr);
    }
}

void
BasicBlock::InsertInstrAfter(IR::Instr *newInstr, IR::Instr *afterThisInstr)
{
    Assert(this->Contains(afterThisInstr));
    afterThisInstr->InsertAfter(newInstr);

    if (this->GetLastInstr() == afterThisInstr)
    {
        Assert(afterThisInstr->HasFallThrough());
        this->SetLastInstr(newInstr);
    }
}

void
BasicBlock::InsertAfter(IR::Instr *newInstr)
{
    Assert(this->GetLastInstr()->HasFallThrough());
    this->GetLastInstr()->InsertAfter(newInstr);
    this->SetLastInstr(newInstr);
}

void
Loop::SetHasCall()
{
    Loop * current = this;
    do
    {
        if (current->hasCall)
        {
#if DBG
            current = current->parent;
            while (current)
            {
                Assert(current->hasCall);
                current = current->parent;
            }
#endif
            break;
        }
        current->hasCall = true;
        current = current->parent;
    }
    while (current != nullptr);
}

void
Loop::SetImplicitCallFlags(Js::ImplicitCallFlags newFlags)
{
    Loop * current = this;
    do
    {
        if ((current->implicitCallFlags & newFlags) == newFlags)
        {
#if DBG
            current = current->parent;
            while (current)
            {
                Assert((current->implicitCallFlags & newFlags) == newFlags);
                current = current->parent;
            }
#endif
            break;
        }
        newFlags = (Js::ImplicitCallFlags)(implicitCallFlags | newFlags);
        current->implicitCallFlags = newFlags;
        current = current->parent;
    }
    while (current != nullptr);
}

Js::ImplicitCallFlags
Loop::GetImplicitCallFlags()
{
    if (this->implicitCallFlags == Js::ImplicitCall_HasNoInfo)
    {
        if (this->parent == nullptr)
        {
            // We don't have any information, and we don't have any parent, so just assume that there aren't any implicit calls
            this->implicitCallFlags = Js::ImplicitCall_None;
        }
        else
        {
            // We don't have any information, get it from the parent and hope for the best
            this->implicitCallFlags = this->parent->GetImplicitCallFlags();
        }
    }
    return this->implicitCallFlags;
}

bool
Loop::CanDoFieldCopyProp()
{
#if DBG_DUMP
    if (((this->implicitCallFlags & ~(Js::ImplicitCall_External)) == 0) &&
        Js::Configuration::Global.flags.Trace.IsEnabled(Js::HostOptPhase))
    {
        Output::Print(L"fieldcopyprop disabled because external: loop count: %d", GetLoopNumber());
        GetFunc()->GetJnFunction()->DumpFullFunctionName();
        Output::Print(L"\n");
        Output::Flush();
    }
#endif
    return GlobOpt::ImplicitCallFlagsAllowOpts(this);
}

bool
Loop::CanDoFieldHoist()
{
    // We can do field hoist wherever we can do copy prop
    return CanDoFieldCopyProp();
}

bool
Loop::CanHoistInvariants()
{
    Func * func = this->GetHeadBlock()->firstInstr->m_func->GetTopFunc();

    if (PHASE_OFF(Js::InvariantsPhase, func))
    {
        return false;
    }

    return true;
}

IR::LabelInstr *
Loop::GetLoopTopInstr() const
{
    IR::LabelInstr * instr = nullptr;
    if (this->topFunc->isFlowGraphValid)
    {
        instr = this->GetHeadBlock()->GetFirstInstr()->AsLabelInstr();
    }
    else
    {
        // Flowgraph gets torn down after the globopt, so can't get the loopTop from the head block.
        instr = this->loopTopLabel;
    }
    if (instr)
    {
        Assert(instr->m_isLoopTop);
    }
    return instr;
}

void
Loop::SetLoopTopInstr(IR::LabelInstr * loopTop)
{
    this->loopTopLabel = loopTop;
}

#if DBG_DUMP
uint
Loop::GetLoopNumber() const
{
    IR::LabelInstr * loopTopInstr = this->GetLoopTopInstr();
    if (loopTopInstr->IsProfiledLabelInstr())
    {
        return loopTopInstr->AsProfiledLabelInstr()->loopNum;
    }
    return Js::LoopHeader::NoLoop;
}

bool
BasicBlock::Contains(IR::Instr * instr)
{
    FOREACH_INSTR_IN_BLOCK(blockInstr, this)
    {
        if (instr == blockInstr)
        {
            return true;
        }
    }
    NEXT_INSTR_IN_BLOCK;
    return false;
}
#endif

FlowEdge *
FlowEdge::New(FlowGraph * graph)
{
    FlowEdge * edge;

    edge = JitAnew(graph->alloc, FlowEdge);

    return edge;
}

bool
Loop::IsDescendentOrSelf(Loop const * loop) const
{
    Loop const * currentLoop = loop;
    while (currentLoop != nullptr)
    {
        if (currentLoop == this)
        {
            return true;
        }
        currentLoop = currentLoop->parent;
    }
    return false;
}

void FlowGraph::SafeRemoveInstr(IR::Instr *instr)
{
    BasicBlock *block;

    if (instr->m_next->IsLabelInstr())
    {
        block = instr->m_next->AsLabelInstr()->GetBasicBlock()->GetPrev();
        block->RemoveInstr(instr);
    }
    else if (instr->IsLabelInstr())
    {
        block = instr->AsLabelInstr()->GetBasicBlock();
        block->RemoveInstr(instr);
    }
    else
    {
        Assert(!instr->EndsBasicBlock() && !instr->StartsBasicBlock());
        instr->Remove();
    }
}

bool FlowGraph::IsUnsignedOpnd(IR::Opnd *src, IR::Opnd **pShrSrc1)
{
    // Look for an unsigned constant, or the result of an unsigned shift by zero
    if (!src->IsRegOpnd())
    {
        return false;
    }
    if (!src->AsRegOpnd()->m_sym->IsSingleDef())
    {
        return false;
    }

    if (src->AsRegOpnd()->m_sym->IsIntConst())
    {
        int32 intConst = src->AsRegOpnd()->m_sym->GetIntConstValue();

        if (intConst >= 0)
        {
            *pShrSrc1 = src;
            return true;
        }
        else
        {
            return false;
        }
    }

    IR::Instr * shrUInstr = src->AsRegOpnd()->m_sym->GetInstrDef();

    if (shrUInstr->m_opcode != Js::OpCode::ShrU_A)
    {
        return false;
    }

    IR::Opnd *shrCnt = shrUInstr->GetSrc2();

    if (!shrCnt->IsRegOpnd() || !shrCnt->AsRegOpnd()->m_sym->IsTaggableIntConst() || shrCnt->AsRegOpnd()->m_sym->GetIntConstValue() != 0)
    {
        return false;
    }

    *pShrSrc1 = shrUInstr->GetSrc1();
    return true;
}

bool FlowGraph::UnsignedCmpPeep(IR::Instr *cmpInstr)
{
    IR::Opnd *cmpSrc1 = cmpInstr->GetSrc1();
    IR::Opnd *cmpSrc2 = cmpInstr->GetSrc2();
    IR::Opnd *newSrc1;
    IR::Opnd *newSrc2;

    // Look for something like:
    //  t1 = ShrU_A x, 0
    //  t2 = 10;
    //  BrGt t1, t2, L
    //
    // Peep to:
    //
    //  t1 = ShrU_A x, 0
    //  t2 = 10;
    //       ByteCodeUse t1
    //  BrUnGt x, t2, L
    //
    // Hopefully dead-store can get rid of the ShrU

    if (!this->func->DoGlobOpt() || !GlobOpt::DoAggressiveIntTypeSpec(this->func) || !GlobOpt::DoLossyIntTypeSpec(this->func))
    {
        return false;
    }

    if (cmpInstr->IsBranchInstr() && !cmpInstr->AsBranchInstr()->IsConditional())
    {
        return false;
    }

    if (!cmpInstr->GetSrc2())
    {
        return false;
    }

    if (!this->IsUnsignedOpnd(cmpSrc1, &newSrc1))
    {
        return false;
    }
    if (!this->IsUnsignedOpnd(cmpSrc2, &newSrc2))
    {
        return false;
    }

    switch(cmpInstr->m_opcode)
    {
    case Js::OpCode::BrEq_A:
    case Js::OpCode::BrNeq_A:
    case Js::OpCode::BrSrEq_A:
    case Js::OpCode::BrSrNeq_A:
        break;
    case Js::OpCode::BrLe_A:
        cmpInstr->m_opcode = Js::OpCode::BrUnLe_A;
        break;
    case Js::OpCode::BrLt_A:
        cmpInstr->m_opcode = Js::OpCode::BrUnLt_A;
        break;
    case Js::OpCode::BrGe_A:
        cmpInstr->m_opcode = Js::OpCode::BrUnGe_A;
        break;
    case Js::OpCode::BrGt_A:
        cmpInstr->m_opcode = Js::OpCode::BrUnGt_A;
        break;
    case Js::OpCode::CmLe_A:
        cmpInstr->m_opcode = Js::OpCode::CmUnLe_A;
        break;
    case Js::OpCode::CmLt_A:
        cmpInstr->m_opcode = Js::OpCode::CmUnLt_A;
        break;
    case Js::OpCode::CmGe_A:
        cmpInstr->m_opcode = Js::OpCode::CmUnGe_A;
        break;
    case Js::OpCode::CmGt_A:
        cmpInstr->m_opcode = Js::OpCode::CmUnGt_A;
        break;

    default:
        return false;
    }

    IR::ByteCodeUsesInstr * bytecodeInstr = IR::ByteCodeUsesInstr::New(cmpInstr->m_func);
    bytecodeInstr->SetByteCodeOffset(cmpInstr);
    bytecodeInstr->byteCodeUpwardExposedUsed = Anew(cmpInstr->m_func->m_alloc, BVSparse<JitArenaAllocator>,cmpInstr->m_func->m_alloc);
    cmpInstr->InsertBefore(bytecodeInstr);

    if (cmpSrc1 != newSrc1)
    {
        if (cmpSrc1->IsRegOpnd())
        {
            bytecodeInstr->byteCodeUpwardExposedUsed->Set(cmpSrc1->AsRegOpnd()->m_sym->m_id);
        }
        cmpInstr->ReplaceSrc1(newSrc1);
        if (newSrc1->IsRegOpnd())
        {
            cmpInstr->GetSrc1()->AsRegOpnd()->SetIsJITOptimizedReg(true);
        }
    }
    if (cmpSrc2 != newSrc2)
    {
        if (cmpSrc2->IsRegOpnd())
        {
            bytecodeInstr->byteCodeUpwardExposedUsed->Set(cmpSrc2->AsRegOpnd()->m_sym->m_id);
        }
        cmpInstr->ReplaceSrc2(newSrc2);
        if (newSrc2->IsRegOpnd())
        {
            cmpInstr->GetSrc2()->AsRegOpnd()->SetIsJITOptimizedReg(true);
        }
    }

    return true;
}


#if DBG

void
FlowGraph::VerifyLoopGraph()
{
    FOREACH_BLOCK(block, this)
    {
        Loop *loop = block->loop;
        FOREACH_SUCCESSOR_BLOCK(succ, block)
        {
            if (loop == succ->loop)
            {
                Assert(succ->isLoopHeader == false || loop->GetHeadBlock() == succ);
                continue;
            }
            if (succ->isLoopHeader)
            {
                Assert(succ->loop->parent == loop
                    || (!loop->IsDescendentOrSelf(succ->loop)));
                continue;
            }
            Assert(succ->loop == nullptr || succ->loop->IsDescendentOrSelf(loop));
        } NEXT_SUCCESSOR_BLOCK;

        if (!PHASE_OFF(Js::RemoveBreakBlockPhase, this->GetFunc()))
        {
            // Make sure all break blocks have been removed.
            if (loop && !block->isLoopHeader && !(this->func->HasTry() && !this->func->DoOptimizeTryCatch()))
            {
                Assert(loop->IsDescendentOrSelf(block->GetPrev()->loop));
            }
        }
    } NEXT_BLOCK;
}

#endif

#if DBG_DUMP

void
FlowGraph::Dump(bool onlyOnVerboseMode, const wchar_t *form)
{
    if(PHASE_DUMP(Js::FGBuildPhase, this->GetFunc()))
    {
        if (!onlyOnVerboseMode || Js::Configuration::Global.flags.Verbose)
        {
            if (form)
            {
                Output::Print(form);
            }
            this->Dump();
        }
    }
}

void
FlowGraph::Dump()
{
    Output::Print(L"\nFlowGraph\n");
    FOREACH_BLOCK(block, this)
    {
        Loop * loop = block->loop;
        while (loop)
        {
            Output::Print(L"    ");
            loop = loop->parent;
        }
        block->DumpHeader(false);
    } NEXT_BLOCK;

    Output::Print(L"\nLoopGraph\n");

    for (Loop *loop = this->loopList; loop; loop = loop->next)
    {
        Output::Print(L"\nLoop\n");
        FOREACH_BLOCK_IN_LOOP(block, loop)
        {
            block->DumpHeader(false);
        }NEXT_BLOCK_IN_LOOP;
        Output::Print(L"Loop  Ends\n");
    }
}

void
BasicBlock::DumpHeader(bool insertCR)
{
    if (insertCR)
    {
        Output::Print(L"\n");
    }
    Output::Print(L"BLOCK %d:", this->number);

    if (this->isDead)
    {
        Output::Print(L" **** DEAD ****");
    }

    if (this->isBreakBlock)
    {
        Output::Print(L" **** Break Block ****");
    }
    else if (this->isAirLockBlock)
    {
        Output::Print(L" **** Air lock Block ****");
    }
    else if (this->isBreakCompensationBlockAtSource)
    {
        Output::Print(L" **** Break Source Compensation Code ****");
    }
    else if (this->isBreakCompensationBlockAtSink)
    {
        Output::Print(L" **** Break Sink Compensation Code ****");
    }
    else if (this->isAirLockCompensationBlock)
    {
        Output::Print(L" **** Airlock block Compensation Code ****");
    }

    if (!this->predList.Empty())
    {
        BOOL fFirst = TRUE;
        Output::Print(L" In(");
        FOREACH_PREDECESSOR_BLOCK(blockPred, this)
        {
            if (!fFirst)
            {
                Output::Print(L", ");
            }
            Output::Print(L"%d", blockPred->GetBlockNum());
            fFirst = FALSE;
        }
        NEXT_PREDECESSOR_BLOCK;
        Output::Print(L")");
    }


    if (!this->succList.Empty())
    {
        BOOL fFirst = TRUE;
        Output::Print(L" Out(");
        FOREACH_SUCCESSOR_BLOCK(blockSucc, this)
        {
            if (!fFirst)
            {
                Output::Print(L", ");
            }
            Output::Print(L"%d", blockSucc->GetBlockNum());
            fFirst = FALSE;
        }
        NEXT_SUCCESSOR_BLOCK;
        Output::Print(L")");
    }

    if (!this->deadPredList.Empty())
    {
        BOOL fFirst = TRUE;
        Output::Print(L" DeadIn(");
        FOREACH_DEAD_PREDECESSOR_BLOCK(blockPred, this)
        {
            if (!fFirst)
            {
                Output::Print(L", ");
            }
            Output::Print(L"%d", blockPred->GetBlockNum());
            fFirst = FALSE;
        }
        NEXT_DEAD_PREDECESSOR_BLOCK;
        Output::Print(L")");
    }

    if (!this->deadSuccList.Empty())
    {
        BOOL fFirst = TRUE;
        Output::Print(L" DeadOut(");
        FOREACH_DEAD_SUCCESSOR_BLOCK(blockSucc, this)
        {
            if (!fFirst)
            {
                Output::Print(L", ");
            }
            Output::Print(L"%d", blockSucc->GetBlockNum());
            fFirst = FALSE;
        }
        NEXT_DEAD_SUCCESSOR_BLOCK;
        Output::Print(L")");
    }

    if (this->loop)
    {
        Output::Print(L"   Loop(%d) header: %d", this->loop->loopNumber, this->loop->GetHeadBlock()->GetBlockNum());

        if (this->loop->parent)
        {
            Output::Print(L" parent(%d): %d", this->loop->parent->loopNumber, this->loop->parent->GetHeadBlock()->GetBlockNum());
        }

        if (this->loop->GetHeadBlock() == this)
        {
            Output::SkipToColumn(50);
            Output::Print(L"Call Exp/Imp: ");
            if (this->loop->GetHasCall())
            {
                Output::Print(L"yes/");
            }
            else
            {
                Output::Print(L" no/");
            }
            Output::Print(Js::DynamicProfileInfo::GetImplicitCallFlagsString(this->loop->GetImplicitCallFlags()));
        }
    }

    Output::Print(L"\n");
    if (insertCR)
    {
        Output::Print(L"\n");
    }
}

void
BasicBlock::Dump()
{
    // Dumping the first instruction (label) will dump the block header as well.
    FOREACH_INSTR_IN_BLOCK(instr, this)
    {
        instr->Dump();
    }
    NEXT_INSTR_IN_BLOCK;
}

void
AddPropertyCacheBucket::Dump() const
{
    Assert(this->initialType != nullptr);
    Assert(this->finalType != nullptr);
    Output::Print(L" initial type: 0x%x, final type: 0x%x ", this->initialType, this->finalType);
}

void
ObjTypeGuardBucket::Dump() const
{
    Assert(this->guardedPropertyOps != nullptr);
    this->guardedPropertyOps->Dump();
}

void
ObjWriteGuardBucket::Dump() const
{
    Assert(this->writeGuards != nullptr);
    this->writeGuards->Dump();
}

#endif
