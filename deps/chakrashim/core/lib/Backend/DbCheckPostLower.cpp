//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "BackEnd.h"

#if DBG

void
DbCheckPostLower::Check()
{
    bool doOpHelperCheck = (Js::Configuration::Global.flags.CheckOpHelpers && !this->func->isPostLayout);
    bool isInHelperBlock = false;

    FOREACH_INSTR_IN_FUNC_EDITING(instr, instrNext, this->func)
    {
        Assert(Lowerer::ValidOpcodeAfterLower(instr, this->func));
        LowererMD::Legalize</*verify*/true>(instr);
        switch(instr->GetKind())
        {
        case IR::InstrKindLabel:
        case IR::InstrKindProfiledLabel:
            isInHelperBlock = instr->AsLabelInstr()->isOpHelper;
            if (doOpHelperCheck && !isInHelperBlock && !instr->AsLabelInstr()->m_noHelperAssert)
            {
                bool foundNonHelperPath = false;
                bool isDeadLabel = true;

                IR::LabelInstr* labelInstr = instr->AsLabelInstr();

                while (1)
                {
                    FOREACH_SLIST_ENTRY(IR::BranchInstr *, branchInstr, &labelInstr->labelRefs)
                    {
                        isDeadLabel = false;
                        IR::Instr *instrPrev = branchInstr->m_prev;
                        while (instrPrev && !instrPrev->IsLabelInstr())
                        {
                            instrPrev = instrPrev->m_prev;
                        }
                        if (!instrPrev || !instrPrev->AsLabelInstr()->isOpHelper || branchInstr->m_isHelperToNonHelperBranch)
                        {
                            foundNonHelperPath = true;
                            break;
                        }
                    } NEXT_SLIST_ENTRY;

                    if (!labelInstr->m_next->IsLabelInstr())
                    {
                        break;
                    }
                    IR::LabelInstr *const nextLabel = labelInstr->m_next->AsLabelInstr();

                    // It is generally not expected for a non-helper label to be immediately followed by a helper label. Some
                    // special cases may flag the helper label with m_noHelperAssert = true. Peeps can cause non-helper blocks
                    // to fall through into helper blocks, so skip this check after peeps.
                    Assert(func->isPostPeeps || nextLabel->m_noHelperAssert || !nextLabel->isOpHelper);

                    if(nextLabel->isOpHelper)
                    {
                        break;
                    }
                    labelInstr = nextLabel;
                }

                instrNext = labelInstr->m_next;

                // This label is unreachable or at least one path to it is not from a helper block.

                if (!foundNonHelperPath && !instr->GetNextRealInstrOrLabel()->IsExitInstr() && !isDeadLabel)
                {
                    IR::Instr *prevInstr = labelInstr->GetPrevRealInstrOrLabel();
                    if (prevInstr->HasFallThrough() && !(prevInstr->IsBranchInstr() && prevInstr->AsBranchInstr()->m_isHelperToNonHelperBranch))
                    {
                        while (prevInstr && !prevInstr->IsLabelInstr())
                        {
                            prevInstr = prevInstr->m_prev;
                        }

                        AssertMsg(prevInstr && prevInstr->IsLabelInstr() && !prevInstr->AsLabelInstr()->isOpHelper, "Inconsistency in Helper label annotations");
                    }
                }
            }
            break;

        case IR::InstrKindBranch:
            if (doOpHelperCheck && !isInHelperBlock)
            {
                IR::LabelInstr *targetLabel = instr->AsBranchInstr()->GetTarget();

                // This branch needs a path to a non-helper block.
                if (instr->AsBranchInstr()->IsConditional())
                {
                    if (targetLabel->isOpHelper && !targetLabel->m_noHelperAssert)
                    {
                        IR::Instr *instrNext = instr->GetNextRealInstrOrLabel();
                        Assert(!(instrNext->IsLabelInstr() && instrNext->AsLabelInstr()->isOpHelper));
                    }
                }
                else
                {
                    Assert(instr->AsBranchInstr()->IsUnconditional());

                    if (targetLabel)
                    {
                        if (!targetLabel->isOpHelper || targetLabel->m_noHelperAssert)
                        {
                            break;
                        }
                        // Target is opHelper

                        IR::Instr *instrPrev = instr->m_prev;

                        if (this->func->isPostRegAlloc)
                        {
                            while (LowererMD::IsAssign(instrPrev))
                            {
                                // Skip potential register allocation compensation code
                                instrPrev = instrPrev->m_prev;
                            }
                        }

                        if (instrPrev->m_opcode == Js::OpCode::DeletedNonHelperBranch)
                        {
                            break;
                        }

                        Assert((instrPrev->IsBranchInstr() && instrPrev->AsBranchInstr()->IsConditional()
                            && (!instrPrev->AsBranchInstr()->GetTarget()->isOpHelper || instrPrev->AsBranchInstr()->GetTarget()->m_noHelperAssert)));
                    }
                    else
                    {
                        Assert(instr->GetSrc1());
                    }
                }
            }
            break;

        default:
            this->Check(instr->GetDst());
            this->Check(instr->GetSrc1());
            this->Check(instr->GetSrc2());

#if defined(_M_IX86) || defined(_M_X64)
            if (EncoderMD::IsOPEQ(instr))
            {
                Assert(instr->GetDst()->IsEqual(instr->GetSrc1()));
            }
            switch (instr->m_opcode)
            {
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
                if (instr->GetSrc2())
                {
                    // CMOV inserted before regAlloc need a fake use of the dst register to make up for the
                    // fact that the CMOV may not set the dst. Regalloc needs to assign the same physical register for dst and src1.
                    Assert(instr->GetDst()->IsEqual(instr->GetSrc1()));
                }
                else
                {
                    // These must have been inserted post-regalloc.
                    Assert(instr->GetDst()->AsRegOpnd()->GetReg() != RegNOREG);
                }
                break;
            }
#endif
            break;
        }
    } NEXT_INSTR_IN_FUNC_EDITING;
}

void DbCheckPostLower::Check(IR::Opnd *opnd)
{
    if (opnd == NULL)
    {
        return;
    }

    if (opnd->IsRegOpnd())
    {
        this->Check(opnd->AsRegOpnd());
    }
    else if (opnd->IsIndirOpnd())
    {
        this->Check(opnd->AsIndirOpnd()->GetBaseOpnd());
        this->Check(opnd->AsIndirOpnd()->GetIndexOpnd());
    }
    else if (opnd->IsSymOpnd() && opnd->AsSymOpnd()->m_sym->IsStackSym())
    {
        if (this->func->isPostRegAlloc)
        {
            AssertMsg(opnd->AsSymOpnd()->m_sym->AsStackSym()->IsAllocated(), "No Stack space allocated for StackSym?");
        }
        IRType symType = opnd->AsSymOpnd()->m_sym->AsStackSym()->GetType();
        if (symType != TyMisc)
        {
            uint symSize = static_cast<uint>(max(TySize[symType], MachRegInt));
            AssertMsg(static_cast<uint>(TySize[opnd->AsSymOpnd()->GetType()]) + opnd->AsSymOpnd()->m_offset <= symSize, "SymOpnd cannot refer to a size greater than Sym's reference");
        }
    }
}

void DbCheckPostLower::Check(IR::RegOpnd *regOpnd)
{
    if (regOpnd == NULL)
    {
        return;
    }

    RegNum reg = regOpnd->GetReg();
    if (reg != RegNOREG)
    {
        if (IRType_IsFloat(LinearScan::GetRegType(reg)))
        {
            // both simd128 and float64 map to float64 regs
            Assert(IRType_IsFloat(regOpnd->GetType()) || IRType_IsSimd128(regOpnd->GetType()));
        }
        else
        {
            Assert(IRType_IsNativeInt(regOpnd->GetType()) || regOpnd->GetType() == TyVar);
#if defined(_M_IX86) || defined(_M_X64)
            if (regOpnd->GetSize() == 1)
            {
                Assert(LinearScan::GetRegAttribs(reg) & RA_BYTEABLE);
            }
#endif
        }
    }
}

#endif // DBG
