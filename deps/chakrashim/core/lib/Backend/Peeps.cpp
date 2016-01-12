//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "BackEnd.h"

// Peeps::PeepFunc
// Run peeps on this function.  For now, it just cleans the redundant reloads
// from the register allocator, and reg/reg movs.  The code tracks which sym is
// in which registers on extended basic blocks.
void
Peeps::PeepFunc()
{
    bool peepsEnabled = true;
    if (PHASE_OFF(Js::PeepsPhase, this->func))
    {
        peepsEnabled = false;
    }

#if defined(_M_IX86) || defined(_M_X64)
    // Agen dependency elimination pass
    // Since it can reveal load elimination opportunities for the normal peeps pass, we do it separately.
    this->peepsAgen.PeepFunc();
#endif

    this->peepsMD.Init(this);

    // Init regMap
    memset(this->regMap, 0, sizeof(this->regMap));

    // Scratch field needs to be cleared.
    this->func->m_symTable->ClearStackSymScratch();

    bool isInHelper = false;

    FOREACH_INSTR_IN_FUNC_EDITING(instr, instrNext, this->func)
    {
        switch (instr->GetKind())
        {
        case IR::InstrKindLabel:
        case IR::InstrKindProfiledLabel:
        {
            if (!peepsEnabled)
            {
                break;
            }
            // Don't carry any regMap info across label
            this->ClearRegMap();

            // Remove unreferenced labels
            if (instr->AsLabelInstr()->IsUnreferenced())
            {
                bool peeped;
                instrNext = PeepUnreachableLabel(instr->AsLabelInstr(), isInHelper, &peeped);
                if(peeped)
                {
                    continue;
                }
            }
            else
            {
                // Try to peep a previous branch again after dead label blocks are removed. For instance:
                //         jmp L2
                //     L3:
                //         // dead code
                //     L2:
                // L3 is unreferenced, so after that block is removed, the branch-to-next can be removed. After that, if L2 is
                // unreferenced and only has fallthrough, it can be removed as well.
                IR::Instr *const prevInstr = instr->GetPrevRealInstr();
                if(prevInstr->IsBranchInstr())
                {
                    IR::BranchInstr *const branch = prevInstr->AsBranchInstr();
                    if(branch->IsUnconditional() && !branch->IsMultiBranch() && branch->GetTarget() == instr)
                    {
                        bool peeped;
                        IR::Instr *const branchNext = branch->m_next;
                        IR::Instr *const branchNextAfterPeep = PeepBranch(branch, &peeped);
                        if(peeped || branchNextAfterPeep != branchNext)
                        {
                            // The peep did something, restart from after the branch
                            instrNext = branchNextAfterPeep;
                            continue;
                        }
                    }
                }
            }

            isInHelper = instr->AsLabelInstr()->isOpHelper;

            if (instrNext->IsLabelInstr())
            {
                // CLean up double label
                instrNext = this->CleanupLabel(instr->AsLabelInstr(), instrNext->AsLabelInstr());
            }

#if defined(_M_IX86) || defined(_M_X64)
            Assert(instrNext->IsLabelInstr() || instrNext->m_prev->IsLabelInstr());
            IR::LabelInstr *const peepCondMoveLabel =
                instrNext->IsLabelInstr() ? instrNext->AsLabelInstr() : instrNext->m_prev->AsLabelInstr();
            instrNext = PeepCondMove(peepCondMoveLabel, instrNext, isInHelper && peepCondMoveLabel->isOpHelper);
#endif

            break;
        }

        case IR::InstrKindBranch:
            if (!peepsEnabled || instr->m_opcode == Js::OpCode::Leave)
            {
                break;
            }
            instrNext = Peeps::PeepBranch(instr->AsBranchInstr());
#if defined(_M_IX86) || defined(_M_X64)
            Assert(instrNext && instrNext->m_prev);
            if (instrNext->m_prev->IsBranchInstr())
            {
                instrNext = this->HoistSameInstructionAboveSplit(instrNext->m_prev->AsBranchInstr(), instrNext);
            }

#endif
            break;

        case IR::InstrKindPragma:
            if (instr->m_opcode == Js::OpCode::Nop)
            {
                instr->Remove();
            }
            break;

        default:
            if (LowererMD::IsAssign(instr))
            {
                if (!peepsEnabled)
                {
                    break;
                }
                // Cleanup spill code
                instrNext = this->PeepAssign(instr);
            }
            else if (instr->m_opcode == Js::OpCode::ArgOut_A_InlineBuiltIn
                || instr->m_opcode == Js::OpCode::StartCall
                || instr->m_opcode == Js::OpCode::LoweredStartCall)
            {
                // ArgOut/StartCall are normally lowered by the lowering of the associated call instr.
                // If the call becomes unreachable, we could end up with an orphan ArgOut or StartCall.
                // Just delete these StartCalls
                instr->Remove();
            }
#if defined(_M_IX86) || defined(_M_X64)
            else if (instr->m_opcode == Js::OpCode::MOVSD_ZERO)
            {
                this->peepsMD.PeepAssign(instr);
                IR::Opnd *dst = instr->GetDst();

                // Look for explicit reg kills
                if (dst && dst->IsRegOpnd())
                {
                    this->ClearReg(dst->AsRegOpnd()->GetReg());
                }
            }
            else if ( (instr->m_opcode == Js::OpCode::INC ) || (instr->m_opcode == Js::OpCode::DEC) )
            {
                // Check for any of the following patterns which can cause partial flag dependency
                //
                //                                                        Jcc or SHL or SHR or SAR or SHLD(in case of x64)
                // Jcc or SHL or SHR or SAR or SHLD(in case of x64)       Any Instruction
                // INC or DEC                                             INC or DEC
                // -------------------------------------------------- OR -----------------------
                // INC or DEC                                             INC or DEC
                // Jcc or SHL or SHR or SAR or SHLD(in case of x64)       Any Instruction
                //                                                        Jcc or SHL or SHR or SAR or SHLD(in case of x64)
                //
                // With this optimization if any of the above pattern found, substitute INC/DEC with ADD/SUB respectively.
                if (!peepsEnabled)
                {
                    break;
                }

                if (AutoSystemInfo::Data.IsAtomPlatform() || PHASE_FORCE(Js::AtomPhase, this->func))
                {
                    bool pattern_found=false;

                    if ( !(instr->IsEntryInstr()) )
                    {
                        IR::Instr *prevInstr = instr->GetPrevRealInstr();
                        if ( IsJccOrShiftInstr(prevInstr)  )
                        {
                            pattern_found = true;
                        }
                        else if ( !(prevInstr->IsEntryInstr()) && IsJccOrShiftInstr(prevInstr->GetPrevRealInstr()) )
                        {
                            pattern_found=true;
                        }
                    }

                    if ( !pattern_found && !(instr->IsExitInstr()) )
                    {
                        IR::Instr *nextInstr = instr->GetNextRealInstr();
                        if ( IsJccOrShiftInstr(nextInstr) )
                        {
                            pattern_found = true;
                        }
                        else if ( !(nextInstr->IsExitInstr() ) && (IsJccOrShiftInstr(nextInstr->GetNextRealInstr())) )
                        {
                            pattern_found = true;
                        }
                    }

                    if (pattern_found)
                    {
                        IR::IntConstOpnd* constOne  = IR::IntConstOpnd::New((IntConstType) 1, TyInt32, instr->m_func);
                        IR::Instr * addOrSubInstr = IR::Instr::New(Js::OpCode::ADD, instr->GetDst(), instr->GetDst(), constOne, instr->m_func);

                        if (instr->m_opcode == Js::OpCode::DEC)
                        {
                            addOrSubInstr->m_opcode = Js::OpCode::SUB;
                        }

                        instr->InsertAfter(addOrSubInstr);
                        instr->Remove();
                        instr = addOrSubInstr;
                    }
                }

                IR::Opnd *dst = instr->GetDst();

                // Look for explicit reg kills
                if (dst && dst->IsRegOpnd())
                {
                    this->ClearReg(dst->AsRegOpnd()->GetReg());
                }
            }

#endif
            else
            {
                if (!peepsEnabled)
                {
                    break;
                }
#if defined(_M_IX86) || defined(_M_X64)
               instr = this->PeepRedundant(instr);
#endif

                IR::Opnd *dst = instr->GetDst();

                // Look for explicit reg kills
                if (dst && dst->IsRegOpnd())
                {
                    this->ClearReg(dst->AsRegOpnd()->GetReg());
                }
                // Kill callee-saved regs across calls and other implicit regs
                this->peepsMD.ProcessImplicitRegs(instr);

#if defined(_M_IX86) || defined(_M_X64)
                if (instr->m_opcode == Js::OpCode::TEST && instr->GetSrc2()->IsIntConstOpnd()
                    && ((instr->GetSrc2()->AsIntConstOpnd()->GetValue() & 0xFFFFFF00) == 0)
                    && instr->GetSrc1()->IsRegOpnd() && (LinearScan::GetRegAttribs(instr->GetSrc1()->AsRegOpnd()->GetReg()) & RA_BYTEABLE))
                {
                    // Only support if the branch is JEQ or JNE to ensure we don't look at the sign flag
                    if (instrNext->IsBranchInstr() &&
                        (instrNext->m_opcode == Js::OpCode::JNE || instrNext->m_opcode == Js::OpCode::JEQ))
                    {
                        instr->GetSrc1()->SetType(TyInt8);
                        instr->GetSrc2()->SetType(TyInt8);
                    }
                }

                if (instr->m_opcode == Js::OpCode::CVTSI2SD)
                {
                    IR::Instr *xorps = IR::Instr::New(Js::OpCode::XORPS, instr->GetDst(), instr->GetDst(), instr->GetDst(), instr->m_func);
                    instr->InsertBefore(xorps);
                }
#endif
            }
        }
    } NEXT_INSTR_IN_FUNC_EDITING;
}

#if defined(_M_IX86) || defined(_M_X64)
// Peeps::IsJccOrShiftInstr()
// Check if instruction is any of the Shift or conditional jump variant
bool
Peeps::IsJccOrShiftInstr(IR::Instr *instr)
{
    bool instrFound = (instr->IsBranchInstr() && instr->AsBranchInstr()->IsConditional()) ||
        (instr->m_opcode == Js::OpCode::SHL) || (instr->m_opcode == Js::OpCode::SHR) || (instr->m_opcode == Js::OpCode::SAR);

#if defined(_M_X64)
    instrFound = instrFound || (instr->m_opcode == Js::OpCode::SHLD);
#endif

    return (instrFound);
}
#endif

// Peeps::PeepAssign
// Remove useless MOV reg, reg as well as redundant reloads
IR::Instr *
Peeps::PeepAssign(IR::Instr *assign)
{
    IR::Opnd *dst = assign->GetDst();
    IR::Opnd *src = assign->GetSrc1();
    IR::Instr *instrNext = assign->m_next;

    // MOV reg, sym
    if (src->IsSymOpnd())
    {
        AssertMsg(src->AsSymOpnd()->m_sym->IsStackSym(), "Only expect stackSyms at this point");
        StackSym *sym = src->AsSymOpnd()->m_sym->AsStackSym();

        if (sym->scratch.peeps.reg != RegNOREG)
        {
            // Found a redundant load
            AssertMsg(this->regMap[sym->scratch.peeps.reg] == sym, "Something is wrong...");
            assign->ReplaceSrc1(IR::RegOpnd::New(sym, sym->scratch.peeps.reg, src->GetType(), this->func));
            src = assign->GetSrc1();
        }
        else
        {
            // Keep track of this load

            AssertMsg(dst->IsRegOpnd(), "For now, we assume dst = sym means dst is a reg");

            RegNum reg = dst->AsRegOpnd()->GetReg();
            this->SetReg(reg, sym);

            return instrNext;
        }
    }
    if (dst->IsRegOpnd())
    {
        // MOV reg, reg

        // Useless?
        if (src->IsRegOpnd() && src->AsRegOpnd()->IsSameReg(dst))
        {
            assign->Remove();
            if (instrNext->m_prev->IsBranchInstr())
            {
                return this->PeepBranch(instrNext->m_prev->AsBranchInstr());
            }
            else
            {
                return instrNext;
            }
        }
        else
        {
            // We could copy the a of the src, but we don't have
            // a way to track 2 regs on the sym...  So let's just clear
            // the info of the dst.
            RegNum dstReg = dst->AsRegOpnd()->GetReg();
            this->ClearReg(dstReg);
        }
    }
    else if (dst->IsSymOpnd() && src->IsRegOpnd())
    {
        // MOV Sym, Reg
        // Track this reg
        RegNum reg = src->AsRegOpnd()->GetReg();
        StackSym *sym = dst->AsSymOpnd()->m_sym->AsStackSym();
        this->SetReg(reg, sym);

    }
    this->peepsMD.PeepAssign(assign);

    return instrNext;
}

// Peeps::ClearRegMap
// Empty the regMap.
// Note: might be faster to have a count and exit if zero?
void
Peeps::ClearRegMap()
{
    for (RegNum reg = (RegNum)(RegNOREG+1); reg != RegNumCount; reg = (RegNum)(reg+1))
    {
        this->ClearReg(reg);
    }
}

// Peeps::SetReg
// Track that this sym is live in this reg
void
Peeps::SetReg(RegNum reg, StackSym *sym)
{
    this->ClearReg(sym->scratch.peeps.reg);
    this->ClearReg(reg);

    this->regMap[reg] = sym;
    sym->scratch.peeps.reg = reg;
}

// Peeps::ClearReg
void
Peeps::ClearReg(RegNum reg)
{
    StackSym *sym = this->regMap[reg];

    if (sym)
    {
        AssertMsg(sym->scratch.peeps.reg == reg, "Something is wrong here...");
        sym->scratch.peeps.reg = RegNOREG;
        this->regMap[reg] = NULL;
    }
}

// Peeps::PeepBranch
//      Remove branch-to-next
//      Invert condBranch/uncondBranch/label
//      Retarget branch to branch
IR::Instr *
Peeps::PeepBranch(IR::BranchInstr *branchInstr, bool *const peepedRef)
{
    if(peepedRef)
    {
        *peepedRef = false;
    }

    IR::LabelInstr *targetInstr = branchInstr->GetTarget();
    IR::Instr *instrNext;

    if (branchInstr->IsUnconditional())
    {
        // Cleanup unreachable code after unconditional branch
        instrNext = RemoveDeadBlock(branchInstr->m_next);
    }

    instrNext = branchInstr->GetNextRealInstrOrLabel();

    if (instrNext != NULL && instrNext->IsLabelInstr())
    {
        //
        // Remove branch-to-next
        //
        if (targetInstr == instrNext)
        {
            if (!branchInstr->IsLowered())
            {
                if (branchInstr->HasAnyImplicitCalls())
                {
                    Assert(!branchInstr->m_func->GetJnFunction()->GetIsAsmjsMode());
                    // if (x > y) might trigger a call to valueof() or something for x and y.
                    // We can't just delete them.
                    Js::OpCode newOpcode;
                    switch(branchInstr->m_opcode)
                    {
                    case Js::OpCode::BrEq_A:
                    case Js::OpCode::BrNeq_A:
                    case Js::OpCode::BrNotEq_A:
                    case Js::OpCode::BrNotNeq_A:
                        newOpcode = Js::OpCode::DeadBrEqual;
                        break;

                    case Js::OpCode::BrSrEq_A:
                    case Js::OpCode::BrSrNeq_A:
                    case Js::OpCode::BrSrNotEq_A:
                    case Js::OpCode::BrSrNotNeq_A:
                        newOpcode = Js::OpCode::DeadBrSrEqual;
                        break;

                    case Js::OpCode::BrGe_A:
                    case Js::OpCode::BrGt_A:
                    case Js::OpCode::BrLe_A:
                    case Js::OpCode::BrLt_A:
                    case Js::OpCode::BrNotGe_A:
                    case Js::OpCode::BrNotGt_A:
                    case Js::OpCode::BrNotLe_A:
                    case Js::OpCode::BrNotLt_A:
                    case Js::OpCode::BrUnGe_A:
                    case Js::OpCode::BrUnGt_A:
                    case Js::OpCode::BrUnLe_A:
                    case Js::OpCode::BrUnLt_A:
                        newOpcode = Js::OpCode::DeadBrRelational;
                        break;

                    case Js::OpCode::BrOnHasProperty:
                    case Js::OpCode::BrOnNoProperty:
                        newOpcode = Js::OpCode::DeadBrOnHasProperty;
                        break;

                    default:
                        Assert(UNREACHED);
                        newOpcode = Js::OpCode::Nop;
                    }
                    IR::Instr *newInstr = IR::Instr::New(newOpcode, branchInstr->m_func);
                    newInstr->SetSrc1(branchInstr->GetSrc1());
                    newInstr->SetSrc2(branchInstr->GetSrc2());
                    branchInstr->InsertBefore(newInstr);
                    newInstr->SetByteCodeOffset(branchInstr);
                }
                else if (branchInstr->GetSrc1() && !branchInstr->GetSrc2())
                {
                    // We only have cases with one src
                    Assert(branchInstr->GetSrc1()->IsRegOpnd());

                    IR::RegOpnd *regSrc = branchInstr->GetSrc1()->AsRegOpnd();
                    StackSym *symSrc = regSrc->GetStackSym();

                    if (symSrc->HasByteCodeRegSlot())
                    {
                        // No side-effects to worry about, but need to insert bytecodeUse.
                        IR::ByteCodeUsesInstr *byteCodeUsesInstr = IR::ByteCodeUsesInstr::New(branchInstr->m_func);
                        byteCodeUsesInstr->SetByteCodeOffset(branchInstr);
                        byteCodeUsesInstr->byteCodeUpwardExposedUsed = JitAnew(branchInstr->m_func->m_alloc, BVSparse<JitArenaAllocator>, branchInstr->m_func->m_alloc);
                        byteCodeUsesInstr->byteCodeUpwardExposedUsed->Set(regSrc->GetStackSym()->m_id);
                        branchInstr->InsertBefore(byteCodeUsesInstr);
                    }
                }
            }
            // Note: if branch is conditional, we have a dead test/cmp left behind...
            if(peepedRef)
            {
                *peepedRef = true;
            }
            branchInstr->Remove();
            if (targetInstr->IsUnreferenced())
            {
                // We may have exposed an unreachable label by deleting the branch
                instrNext = Peeps::PeepUnreachableLabel(targetInstr, false);
            }
            else
            {
                instrNext = targetInstr;
            }
            IR::Instr *instrPrev = instrNext->GetPrevRealInstrOrLabel();
            if (instrPrev->IsBranchInstr())
            {
                // The branch removal could have exposed a branch to next opportunity.
                return Peeps::PeepBranch(instrPrev->AsBranchInstr());
            }
            return instrNext;
        }
    }
    else if (branchInstr->IsConditional())
    {
        AnalysisAssert(instrNext);
        if (instrNext->IsBranchInstr()
            && instrNext->AsBranchInstr()->IsUnconditional()
            && targetInstr == instrNext->AsBranchInstr()->GetNextRealInstrOrLabel()
            && !instrNext->AsBranchInstr()->IsMultiBranch())
        {
            //
            // Invert condBranch/uncondBranch/label:
            //
            //      JCC L1                   JinvCC L3
            //      JMP L3       =>
            //      L1:
            IR::BranchInstr *uncondBranch = instrNext->AsBranchInstr();

            if (branchInstr->IsLowered())
            {
                LowererMD::InvertBranch(branchInstr);
            }
            else
            {
                branchInstr->Invert();
            }

            targetInstr = uncondBranch->GetTarget();
            branchInstr->SetTarget(targetInstr);
            if (targetInstr->IsUnreferenced())
            {
                Peeps::PeepUnreachableLabel(targetInstr, false);
            }

            uncondBranch->Remove();

            return PeepBranch(branchInstr, peepedRef);
        }
    }

    if(branchInstr->IsMultiBranch())
    {
        IR::MultiBranchInstr * multiBranchInstr = branchInstr->AsMultiBrInstr();

        multiBranchInstr->UpdateMultiBrLabels([=](IR::LabelInstr * targetInstr) -> IR::LabelInstr *
        {
            IR::LabelInstr * labelInstr = RetargetBrToBr(branchInstr, targetInstr);
            return labelInstr;
        });
    }
    else
    {
        RetargetBrToBr(branchInstr, targetInstr);
    }

    return branchInstr->m_next;
}

#if defined(_M_IX86) || defined(_M_X64)
//
// For conditional branch JE $LTarget, if both target and fallthrough branch has the same
// instruction B, hoist it up and tail dup target branch:
//
//      A                      <unconditional branch>
//      JE  $LTarget           $LTarget:
//      B                           B
//      ...                         JMP $L2
//
//====> hoist B up: move B up from fallthrough branch, remove B in target branch, retarget to $L2
// $LTarget to $L2
//
//      A                      <unconditional branch>
//      B                      $LTarget:
//      JE  $L2                     JMP $L2
//      ...
//
//====> now $LTarget becomes to be an empty BB, which can be removed if it's an unreferenced label
//
//      A
//      B
//      JE  $L2
//      ...
//
//====> Note B will be hoist above compare instruction A if there are no dependency between A and B
//
//      B
//      A          (cmp instr)
//      JE  $L2
//      ...
//
IR::Instr *
Peeps::HoistSameInstructionAboveSplit(IR::BranchInstr *branchInstr, IR::Instr *instrNext)
{
    Assert(branchInstr);
    if (!branchInstr->IsConditional() || branchInstr->IsMultiBranch() || !branchInstr->IsLowered())
    {
        return instrNext;   // this optimization only applies to single conditional branch
    }

    IR::LabelInstr *targetLabel = branchInstr->GetTarget();
    Assert(targetLabel);

    // give up if there are other branch entries to the target label
    if (targetLabel->labelRefs.Count() > 1)
    {
        return instrNext;
    }

    // Give up if previous instruction before target label has fallthrough, cannot hoist up
    IR::Instr *targetPrev = targetLabel->GetPrevRealInstrOrLabel();
    Assert(targetPrev);
    if (targetPrev->HasFallThrough())
    {
        return instrNext;
    }

    IR::Instr *instrSetCondition = NULL;
    IR::Instr *branchPrev = branchInstr->GetPrevRealInstrOrLabel();
    while (branchPrev && !branchPrev->StartsBasicBlock())
    {
        if (!instrSetCondition && EncoderMD::SetsConditionCode(branchPrev))
        {   // located compare instruction for the branch
            instrSetCondition = branchPrev;
        }
        branchPrev = branchPrev->GetPrevRealInstrOrLabel(); // keep looking previous instr in BB
    }

    if (branchPrev && branchPrev->IsLabelInstr() && branchPrev->AsLabelInstr()->isOpHelper)
    {   // don't apply the optimization when branch is in helper section
        return instrNext;
    }

    if (!instrSetCondition)
    {   // give up if we cannot find the compare instruction in the BB, should be very rare
        return instrNext;
    }
    Assert(instrSetCondition);

    bool hoistAboveSetConditionInstr = false;
    if (instrSetCondition == branchInstr->GetPrevRealInstrOrLabel())
    {   // if compare instruction is right before branch instruction, we can hoist above cmp instr
        hoistAboveSetConditionInstr = true;
    }   // otherwise we hoist the identical instructions above conditional branch split only

    IR::Instr *instr = branchInstr->GetNextRealInstrOrLabel();
    IR::Instr *targetInstr = targetLabel->GetNextRealInstrOrLabel();
    IR::Instr *branchNextInstr = NULL;
    IR::Instr *targetNextInstr = NULL;
    IR::Instr *instrHasHoisted = NULL;

    Assert(instr && targetInstr);
    while (!instr->EndsBasicBlock() && !instr->IsLabelInstr() && instr->IsEqual(targetInstr) &&
        !EncoderMD::UsesConditionCode(instr) && !EncoderMD::SetsConditionCode(instr) &&
        !this->peepsAgen.DependentInstrs(instrSetCondition, instr))
    {
        branchNextInstr = instr->GetNextRealInstrOrLabel();
        targetNextInstr = targetInstr->GetNextRealInstrOrLabel();

        instr->Unlink();                            // hoist up instr in fallthrough branch
        if (hoistAboveSetConditionInstr)
        {
            instrSetCondition->InsertBefore(instr); // hoist above compare instruction
        }
        else
        {
            branchInstr->InsertBefore(instr);       // hoist above branch split
        }
        targetInstr->Remove();                      // remove the same instruction in target branch

        if (!instrHasHoisted)
            instrHasHoisted = instr;                // points to the first hoisted instruction

        instr = branchNextInstr;
        targetInstr = targetNextInstr;
        Assert(instr && targetInstr);
    }

    if (instrHasHoisted)
    {   // instructions have been hoisted, now check tail branch to see if it can be duplicated
        if (targetInstr->IsBranchInstr())
        {
            IR::BranchInstr *tailBranch = targetInstr->AsBranchInstr();
            if (tailBranch->IsUnconditional() && !tailBranch->IsMultiBranch())
            {   // target can be replaced since tail branch is a single unconditional jmp
                branchInstr->ReplaceTarget(targetLabel, tailBranch->GetTarget());
            }

            // now targeLabel is an empty Basic Block, remove it if it's not referenced
            if (targetLabel->IsUnreferenced())
            {
                Peeps::PeepUnreachableLabel(targetLabel, targetLabel->isOpHelper);
            }
        }
        return instrHasHoisted;
    }

    return instrNext;
}
#endif

IR::LabelInstr *
Peeps::RetargetBrToBr(IR::BranchInstr *branchInstr, IR::LabelInstr * targetInstr)
{
    AnalysisAssert(targetInstr);
    IR::Instr *targetInstrNext = targetInstr->GetNextRealInstr();
    AnalysisAssertMsg(targetInstrNext, "GetNextRealInstr() failed to get next target");

    // Removing branch to branch breaks some lexical assumptions about loop in sccliveness/linearscan/second chance.
    if (!branchInstr->IsLowered())
    {
        return targetInstr;
    }

    //
    // Retarget branch-to-branch
    //
#if DBG
    uint counter = 0;
#endif

    IR::LabelInstr *lastLoopTop = NULL;

    while (targetInstrNext->IsBranchInstr() && targetInstrNext->AsBranchInstr()->IsUnconditional())
    {
        IR::BranchInstr *branchAtTarget = targetInstrNext->AsBranchInstr();

        // We don't want to skip the loop entry, unless we're right before the encoder
        if (targetInstr->m_isLoopTop && !branchAtTarget->IsLowered())
        {
            break;
        }

        if (targetInstr->m_isLoopTop)
        {
            if (targetInstr == lastLoopTop)
            {
                // We are back to a loopTop already visited.
                // Looks like an infinite loop somewhere...
                break;
            }
            lastLoopTop = targetInstr;
        }
#if DBG
        if (!branchInstr->IsMultiBranch() && branchInstr->GetTarget()->m_noHelperAssert && !branchAtTarget->IsMultiBranch())
        {
            branchAtTarget->GetTarget()->m_noHelperAssert = true;
        }

        AssertMsg(++counter < 10000, "We should not be looping this many times!");
#endif

        IR::LabelInstr * reTargetLabel = branchAtTarget->GetTarget();
        AnalysisAssert(reTargetLabel);
        if (targetInstr == reTargetLabel)
        {
            // Infinite loop.
            //    JCC $L1
            // L1:
            //    JMP $L1
            break;
        }

        if(branchInstr->IsMultiBranch())
        {
            IR::MultiBranchInstr * multiBranchInstr = branchInstr->AsMultiBrInstr();
            multiBranchInstr->ChangeLabelRef(targetInstr, reTargetLabel);
        }
        else
        {
            branchInstr->SetTarget(reTargetLabel);
        }

        if (targetInstr->IsUnreferenced())
        {
            Peeps::PeepUnreachableLabel(targetInstr, false);
        }

        targetInstr = reTargetLabel;
        targetInstrNext = targetInstr->GetNextRealInstr();
    }
    return targetInstr;
}

IR::Instr *
Peeps::PeepUnreachableLabel(IR::LabelInstr *deadLabel, const bool isInHelper, bool *const peepedRef)
{
    Assert(deadLabel);
    Assert(deadLabel->IsUnreferenced());

    IR::Instr *prevFallthroughInstr = deadLabel;
    do
    {
        prevFallthroughInstr = prevFallthroughInstr->GetPrevRealInstrOrLabel();
        // The previous dead label may have been kept around due to a StatementBoundary, see comment in RemoveDeadBlock.
    } while(prevFallthroughInstr->IsLabelInstr() && prevFallthroughInstr->AsLabelInstr()->IsUnreferenced());

    IR::Instr *instrReturn;
    bool removeLabel;

    // If code is now unreachable, delete block
    if (!prevFallthroughInstr->HasFallThrough())
    {
        bool wasStatementBoundaryKeptInDeadBlock = false;
        instrReturn = RemoveDeadBlock(deadLabel->m_next, &wasStatementBoundaryKeptInDeadBlock);

        // Remove label only if we didn't have to keep last StatementBoundary in the dead block,
        // see comment in RemoveDeadBlock.
        removeLabel = !wasStatementBoundaryKeptInDeadBlock;

        if(peepedRef)
        {
            *peepedRef = true;
        }
    }
    else
    {
        instrReturn = deadLabel->m_next;
        removeLabel =
            deadLabel->isOpHelper == isInHelper
#if DBG
            && !deadLabel->m_noHelperAssert
#endif
            ;
        if(peepedRef)
        {
            *peepedRef = removeLabel;
        }
    }

    if (removeLabel && deadLabel->IsUnreferenced())
    {
        deadLabel->Remove();
    }

    return instrReturn;
}

IR::Instr *
Peeps::CleanupLabel(IR::LabelInstr * instr, IR::LabelInstr * instrNext)
{
    IR::Instr * returnInstr;

    IR::LabelInstr * labelToRemove;
    IR::LabelInstr * labelToKeep;

    // Just for dump, always keep loop top labels
    // We also can remove label that has non branch references
    if (instrNext->m_isLoopTop || instrNext->m_hasNonBranchRef)
    {
        if (instr->m_isLoopTop || instr->m_hasNonBranchRef)
        {
            // Don't remove loop top labels or labels with non branch references
            return instrNext;
        }
        labelToRemove = instr;
        labelToKeep = instrNext;
        returnInstr = instrNext;
    }
    else
    {
        labelToRemove = instrNext;
        labelToKeep = instr;
        returnInstr = instrNext->m_next;
    }
    while (!labelToRemove->labelRefs.Empty())
    {
        bool replaced = labelToRemove->labelRefs.Head()->ReplaceTarget(labelToRemove, labelToKeep);
        Assert(replaced);
    }

    if (labelToRemove->isOpHelper)
    {
        labelToKeep->isOpHelper = true;
#if DBG
        if (labelToRemove->m_noHelperAssert)
        {
            labelToKeep->m_noHelperAssert = true;
        }
#endif
    }

    labelToRemove->Remove();
    return returnInstr;
}

//
// Removes instrs starting from one specified by the 'instr' parameter.
// Keeps last statement boundary in the whole block to remove.
// Stops at label or exit instr.
// Return value:
// - 1st instr that is label or end instr, except the case when forceRemoveFirstInstr is true, in which case
//   we start checking for exit loop condition from next instr.
// Notes:
// - if wasStmtBoundaryKeptInDeadBlock is not NULL, it receives true when we didn't remove last
//   StatementBoundary pragma instr as otherwise it would be non-helper/helper move of the pragma instr.
//   If there was no stmt boundary or last stmt boundary moved to after next label, that would receive false.
//
IR::Instr *Peeps::RemoveDeadBlock(IR::Instr *instr, bool* wasStmtBoundaryKeptInDeadBlock /* = nullptr */)
{
    IR::Instr* lastStatementBoundary = nullptr;

    while (instr && !instr->IsLabelInstr() && !instr->IsExitInstr())
    {
        IR::Instr *deadInstr = instr;
        instr = instr->m_next;

        if (deadInstr->IsPragmaInstr() && deadInstr->m_opcode == Js::OpCode::StatementBoundary)
        {
            if (lastStatementBoundary)
            {
                //Its enough if we keep latest statement boundary. Rest are dead anyway.
                lastStatementBoundary->Remove();
            }
            lastStatementBoundary = deadInstr;
        }
        else
        {
            deadInstr->Remove();
        }
    }

    // Do not let StatementBoundary to move across non-helper and helper blocks, very important under debugger:
    // if we let that happen, helper block can be moved to the end of the func so that statement maps will miss one statement.
    // Issues can be when (normally, StatementBoundary should never belong to a helper label):
    // - if we remove the label and prev label is a helper, StatementBoundary will be moved inside helper.
    // - if we move StatementBoundary under next label which is a helper, same problem again.
    bool canMoveStatementBoundaryUnderNextLabel = instr && instr->IsLabelInstr() && !instr->AsLabelInstr()->isOpHelper;

    if (lastStatementBoundary && canMoveStatementBoundaryUnderNextLabel)
    {
        lastStatementBoundary->Unlink();
        instr->InsertAfter(lastStatementBoundary);
    }

    if (wasStmtBoundaryKeptInDeadBlock)
    {
        *wasStmtBoundaryKeptInDeadBlock = lastStatementBoundary && !canMoveStatementBoundaryUnderNextLabel;
    }

    return instr;
}

// Shared code for x86 and amd64
#if defined(_M_IX86) || defined(_M_X64)
IR::Instr *
Peeps::PeepRedundant(IR::Instr *instr)
{
    IR::Instr *retInstr = instr;

    if (instr->m_opcode == Js::OpCode::ADD || instr->m_opcode == Js::OpCode::SUB || instr->m_opcode == Js::OpCode::OR)
    {
        Assert(instr->GetSrc1() && instr->GetSrc2());
        if( (instr->GetSrc2()->IsIntConstOpnd() && instr->GetSrc2()->AsIntConstOpnd()->GetValue() == 0))
        {
            // remove instruction
            retInstr = instr->m_next;
            instr->Remove();
        }
    }
#if _M_IX86
    RegNum edx = RegEDX;
#else
    RegNum edx = RegRDX;
#endif
    if (instr->m_opcode == Js::OpCode::NOP && instr->GetDst() != NULL
        && instr->GetDst()->IsRegOpnd() && instr->GetDst()->AsRegOpnd()->GetReg() == edx)
    {
        // dummy def used for non-32bit ovf check for IMUL
        // check edx is not killed between IMUL and edx = NOP, then remove the NOP
        bool found = false;
        IR::Instr *nopInstr = instr;
        do
        {
            instr = instr->GetPrevRealInstrOrLabel();
            if (instr->m_opcode == Js::OpCode::IMUL)
            {
                found = true;
                break;
            }
        } while(!instr->StartsBasicBlock());

        if (found)
        {
            retInstr = nopInstr->m_next;
            nopInstr->Remove();
        }
        else
        {
            instr = nopInstr;
            do
            {
                instr = instr->GetNextRealInstrOrLabel();
                if (instr->m_opcode == Js::OpCode::DIV)
                {
                    found = true;
                    retInstr = nopInstr->m_next;
                    nopInstr->Remove();
                    break;
                }
            } while (!instr->EndsBasicBlock());

            AssertMsg(found, "edx = NOP without an IMUL or DIV");
        }
    }
    return retInstr;
}

/*
    Work backwards from the label instruction to look for this pattern:
        Jcc $Label
        Mov
        Mov
        ..
        Mov
    Label:

    If found, we remove the Jcc, convert MOVs to CMOVcc and remove the label if unreachable.
*/
IR::Instr*
Peeps::PeepCondMove(IR::LabelInstr *labelInstr, IR::Instr *nextInstr, const bool isInHelper)
{
    IR::Instr *instr = labelInstr->GetPrevRealInstrOrLabel();

    Js::OpCode newOpCode;

    // Check if BB is all MOVs with both RegOpnd
    while(instr->m_opcode == Js::OpCode::MOV)
    {
        if (!instr->GetSrc1()->IsRegOpnd() || !instr->GetDst()->IsRegOpnd())
            return nextInstr;
        instr = instr->GetPrevRealInstrOrLabel();
    }

    // Did we hit a conditional branch ?
    if (instr->IsBranchInstr() && instr->AsBranchInstr()->IsConditional() &&
        !instr->AsBranchInstr()->IsMultiBranch() &&
        instr->AsBranchInstr()->GetTarget() == labelInstr &&
        instr->m_opcode != Js::OpCode::Leave)
    {
        IR::BranchInstr *brInstr = instr->AsBranchInstr();

        // Get the correct CMOVcc
        switch(brInstr->m_opcode)
        {
        case Js::OpCode::JA:
                newOpCode = Js::OpCode::CMOVBE;
                break;
        case Js::OpCode::JAE:
                newOpCode = Js::OpCode::CMOVB;
                break;
        case Js::OpCode::JB:
                newOpCode = Js::OpCode::CMOVAE;
                break;
        case Js::OpCode::JBE:
                newOpCode = Js::OpCode::CMOVA;
                break;
        case Js::OpCode::JEQ:
                newOpCode = Js::OpCode::CMOVNE;
                break;
        case Js::OpCode::JNE:
                newOpCode = Js::OpCode::CMOVE;
                break;
        case Js::OpCode::JNP:
                newOpCode = Js::OpCode::CMOVP;
                break;
        case Js::OpCode::JLT:
                newOpCode = Js::OpCode::CMOVGE;
                break;
        case Js::OpCode::JLE:
                newOpCode = Js::OpCode::CMOVG;
                break;
        case Js::OpCode::JGT:
                newOpCode = Js::OpCode::CMOVLE;
                break;
        case Js::OpCode::JGE:
                newOpCode = Js::OpCode::CMOVL;
                break;
        case Js::OpCode::JNO:
                newOpCode = Js::OpCode::CMOVO;
                break;
        case Js::OpCode::JO:
                newOpCode = Js::OpCode::CMOVNO;
                break;
        case Js::OpCode::JP:
                newOpCode = Js::OpCode::CMOVNP;
                break;
        case Js::OpCode::JNSB:
                newOpCode = Js::OpCode::CMOVS;
                break;
        case Js::OpCode::JSB:
                newOpCode = Js::OpCode::CMOVNS;
                break;
        default:
                Assert(UNREACHED);
                __assume(UNREACHED);
        }

        // convert the entire block to CMOVs
        instr = brInstr->GetNextRealInstrOrLabel();
        while(instr != labelInstr)
        {
            instr->m_opcode = newOpCode;
            instr = instr->GetNextRealInstrOrLabel();
        }

        // remove the Jcc
        brInstr->Remove();
        // We may have exposed an unreachable label by deleting the branch
        if (labelInstr->IsUnreferenced())
           return Peeps::PeepUnreachableLabel(labelInstr, isInHelper);
    }
    return nextInstr;
}

#endif
