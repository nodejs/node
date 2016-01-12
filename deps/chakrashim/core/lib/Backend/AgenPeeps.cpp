//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "BackEnd.h"

///----------------------------------------------------------------------------
///
/// AgenPeeps::PeepFunc
///
///     Looks for AGEN dependencies between a MOV and another instruction. The heuristic works as follows:
///     - Look for an AGEN dependency. If found, move the first instruction upwards in the BB as far as possible.
///     - For subsequent dependencies (dep. chain), we move the instruction at least 3 slots from the previous instruction.
///     - We assume no dependency between different dep. chains.
///     Example:
///         BB_start
///         ...
///         s2 = MOV [s1 + offset]      ; start of dep chain. Move instruction up as far as possible
///         s3 = MOV [s2]               ; inside dep chain. Move instruction up at least 3 slots from prev. instruction
///         s4 = MOV [s3 + offset]      ; inside dep chain. Move instruction up at least 3 slots from prev. instruction
///         ...
///         s5 = MOV [xxxx]             ; new dep chain. Move instruction up as far as possible
///         s6 = MOV [s5 + offset]
///----------------------------------------------------------------------------
void AgenPeeps::PeepFunc()
{
    int distance = 0;
    IR::Instr *blockStart, *nextRealInstr;
    const uint stall_cycles = 3;

    if (AutoSystemInfo::Data.IsAtomPlatform())
    {
        // On Atom, always optimize unless phase is off
        if (PHASE_OFF(Js::AtomPhase, func) || PHASE_OFF(Js::AgenPeepsPhase, func))
            return;
    }
    else
    {
        // On other platforms, don't optimize unless phase is forced
        if (!PHASE_FORCE(Js::AtomPhase, func) && !PHASE_FORCE(Js::AgenPeepsPhase, func))
            return;
    }

    blockStart = nullptr;
    FOREACH_INSTR_IN_FUNC_EDITING(instr, instrNext, this->func)
    {
        if (!instr->IsRealInstr() && !instr->IsLabelInstr())
            continue;

        // BB boundary ?
        if (instr->EndsBasicBlock() || instr->StartsBasicBlock())
        {
            distance = 0;
            instrNext = blockStart = instr->GetNextRealInstr();
            continue;
        }
        nextRealInstr = instr->GetNextRealInstrOrLabel();
        // Check for AGEN dependency with the next instruction in the same BB
        if (!nextRealInstr->EndsBasicBlock() && !nextRealInstr->StartsBasicBlock() &&
            AgenDependentInstrs(instr, nextRealInstr))
        {
            Assert(blockStart);
            // Move instr up
            distance = MoveInstrUp(instr, blockStart, distance) - stall_cycles;
        } else
            distance = 0;

    } NEXT_INSTR_IN_FUNC_EDITING;
}

///----------------------------------------------------------------------------
///
/// AgenPeeps::MoveInstrUp
///
///     Moves an instruction up in the BB up to a bound or until it hits
///     a dependent instruction. If  bound <= 0, move as far as possible.
///----------------------------------------------------------------------------

int AgenPeeps::MoveInstrUp(IR::Instr *instrToMove, IR::Instr *blockStart, int bound)
{
    AssertMsg(LowererMD::IsAssign(instrToMove), "We only reschedule move instructions for now");
    IR::Instr *instr;
    int i = 1;
    if (instrToMove == blockStart || DependentInstrs(instrToMove, instrToMove->GetPrevRealInstr()))
        return 0;

    instr = instrToMove->GetPrevRealInstr();
    instrToMove->Unlink();

    while (!DependentInstrs(instrToMove, instr))
    {
        if (instr == blockStart || (bound > 0 && i == bound))
        {
            instr->InsertBefore(instrToMove);
            return i;
        }
        instr = instr->GetPrevRealInstr();
        i++;
    }
    // Insert after dependent instruction
    instr->InsertAfter(instrToMove);

    return i - 1;
}

///----------------------------------------------------------------------------
///
/// AgenPeeps::AgenDependentInstr
///
///     Determines if two instructions are Agen dependent
///
///----------------------------------------------------------------------------
bool AgenPeeps::AgenDependentInstrs(IR::Instr *instr1, IR::Instr *instr2)
{
    // We only deal with assign instructions for now.
    if (!LowererMD::IsAssign(instr1) || !DependentInstrs(instr1, instr2))
        return false;

    if (instr1->GetDst()->IsRegOpnd())
    {
        IR::IndirOpnd *src1 = (instr2->GetSrc1() && instr2->GetSrc1()->IsIndirOpnd()) ? instr2->GetSrc1()->AsIndirOpnd() : nullptr;
        IR::IndirOpnd *src2 = (instr2->GetSrc2() && instr2->GetSrc2()->IsIndirOpnd()) ? instr2->GetSrc2()->AsIndirOpnd() : nullptr;
        IR::IndirOpnd *dst  = (instr2->GetDst()  && instr2->GetDst()->IsIndirOpnd())  ? instr2->GetDst()->AsIndirOpnd() : nullptr;
        IR::RegOpnd   *regOpnd = instr1->GetDst()->AsRegOpnd();

        IR::RegOpnd *base, *index;
        if (src1)
        {
            base = src1->GetBaseOpnd();
            index = src1->GetIndexOpnd();
            return (base && regOpnd->IsSameRegUntyped(base)) || (index && regOpnd->IsSameRegUntyped(index));
        }

        if (src2)
        {
            base = src2->GetBaseOpnd();
            index = src2->GetIndexOpnd();
            return (base && regOpnd->IsSameRegUntyped(base)) || (index && regOpnd->IsSameRegUntyped(index));
        }

        if (dst)
        {
            base = dst->GetBaseOpnd();
            index = dst->GetIndexOpnd();
            return (base && regOpnd->IsSameRegUntyped(base)) || (index && regOpnd->IsSameRegUntyped(index));
        }
    }
    return false;
}

///----------------------------------------------------------------------------
///
/// AgenPeeps::DependentInstr
///
///     Determines if two instructions are dependent
///
///----------------------------------------------------------------------------
bool AgenPeeps::DependentInstrs(IR::Instr *instr1, IR::Instr *instr2)
{

    if (AlwaysDependent(instr1) || AlwaysDependent(instr2))
        return true;

    // Check for RAW, WAR and WAW dependence
    return \
        DependentOpnds(instr1->GetSrc1(), instr2->GetDst()) ||
        DependentOpnds(instr1->GetSrc2(), instr2->GetDst()) ||
        DependentOpnds(instr1->GetDst(), instr2->GetDst()) ||
        DependentOpnds(instr2->GetSrc1(), instr1->GetDst()) ||
        DependentOpnds(instr2->GetSrc2(), instr1->GetDst()) ||
        (instr1->m_opcode == Js::OpCode::XCHG &&                    // XCHG's src2 is also a dst
            (DependentOpnds(instr2->GetSrc1(), instr1->GetSrc2()) ||
            DependentOpnds(instr2->GetSrc2(), instr1->GetSrc2()))) ||
        (instr2->m_opcode == Js::OpCode::XCHG &&                    // XCHG's src2 is also a dst
            (DependentOpnds(instr1->GetSrc1(), instr2->GetSrc2()) ||
            DependentOpnds(instr1->GetSrc2(), instr2->GetSrc2())));
}

// Being conservative here about instructions that implicitly reads/writes memory/regs without explicit Opnds
bool AgenPeeps::AlwaysDependent(IR::Instr *instr)
{
    return LowererMD::IsCall(instr) ||
        instr->m_opcode == Js::OpCode::PUSH ||
        instr->m_opcode == Js::OpCode::POP ||
        instr->m_opcode == Js::OpCode::DIV ||
        instr->m_opcode == Js::OpCode::IDIV ||
        instr->m_opcode == Js::OpCode::IMUL;
}

///----------------------------------------------------------------------------
///
/// AgenPeeps::DependentOpnd
///
///     Determines if two operands are dependent. This function is commutative.
///
///----------------------------------------------------------------------------
bool AgenPeeps::DependentOpnds(IR::Opnd *opnd1, IR::Opnd *opnd2)
{
#if defined (_M_IX86)
    const RegNum baseReg = RegEBP;
#elif defined (_M_X64)
    const RegNum baseReg = RegRBP;
#else
    AssertMsg(false, "Optimization not supported for ARM");
#endif

    if (!opnd1 || !opnd2)
        return false;

    // Memory dependence
    if (IsMemoryOpnd(opnd1) && IsMemoryOpnd(opnd2))
    {
        IR::SymOpnd *symOpnd1 = opnd1->IsSymOpnd() ? opnd1->AsSymOpnd() : nullptr;
        IR::SymOpnd *symOpnd2 = opnd2->IsSymOpnd() ? opnd2->AsSymOpnd() : nullptr;

        if (symOpnd1 || symOpnd2)
        {
            // SymOpnd do not alias with  MemRefOpnd/IndirOpnd
            if (opnd1->IsMemRefOpnd() || opnd2->IsMemRefOpnd() || opnd1->IsIndirOpnd() || opnd2->IsIndirOpnd())
                return false;

            // Two symOpnds are dependent if they point to the same stack symbol
            if (symOpnd1 && symOpnd2 &&
                symOpnd1->m_sym->IsStackSym() && symOpnd2->m_sym->IsStackSym() )
            {
                return symOpnd1->m_sym->AsStackSym()->m_offset == symOpnd2->m_sym->AsStackSym()->m_offset;
            }
        }
        // all other memory operands are dependent
        return true;
    }

    IR::RegOpnd *regOpnd, *base, *index;

    // Register dependences
    if (opnd1->IsRegOpnd())
    {
        regOpnd = opnd1->AsRegOpnd();
        // reg-to-reg
        if (opnd2->IsRegOpnd() && regOpnd->IsSameRegUntyped(opnd2->AsRegOpnd()))
            return true;

        // opnd2 = [base + indx + offset] and (opnd1 = base or opnd1 = indx)
        if (opnd2->IsIndirOpnd())
        {
            base = opnd2->AsIndirOpnd()->GetBaseOpnd();
            index = opnd2->AsIndirOpnd()->GetIndexOpnd();
            if ( (base && regOpnd->IsSameRegUntyped(base)) || (index && regOpnd->IsSameRegUntyped(index)) )
                return true;
        }
        // opnd1 = ebp/rbp and opnd2 = [ebp/rbp + offset]
        if (opnd2->IsSymOpnd() && opnd2->AsSymOpnd()->m_sym->IsStackSym() && regOpnd->GetReg() == baseReg)
            return true;
    }

    if (opnd2->IsRegOpnd())
    {
        regOpnd = opnd2->AsRegOpnd();

        // opnd1 = [base + indx + offset] and (opnd2 = base or opnd2 = indx)
        if (opnd1->IsIndirOpnd())
        {
            base = opnd1->AsIndirOpnd()->GetBaseOpnd();
            index = opnd1->AsIndirOpnd()->GetIndexOpnd();
            if ( (base && regOpnd->IsSameRegUntyped(base)) || (index && regOpnd->IsSameRegUntyped(index)) )
                return true;
        }

        // opnd2 = ebp/rbp and opnd1 = [ebp/rbp + offset]
        if (opnd1->IsSymOpnd() && opnd1->AsSymOpnd()->m_sym->IsStackSym() && regOpnd->GetReg() == baseReg)
            return true;
    }
    return false;
}

bool AgenPeeps::IsMemoryOpnd(IR::Opnd *opnd)
{
    return opnd->IsSymOpnd() || opnd->IsIndirOpnd() || opnd->IsMemRefOpnd();
}
