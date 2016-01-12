//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "BackEnd.h"

#include "X86Encode.h"


static const BYTE OpcodeByte2[]={
#define MACRO(name, jnLayout, attrib, byte2, ...) byte2,
#include "MdOpcodes.h"
#undef MACRO
};

struct FormTemplate{ BYTE form[6]; };

#define f(form) TEMPLATE_FORM_ ## form
static const struct FormTemplate OpcodeFormTemplate[] =
{
#define MACRO(name, jnLayout, attrib, byte2, form, ...) form ,
#include "MdOpcodes.h"
#undef MACRO
};
#undef f

struct OpbyteTemplate { byte opbyte[6]; };

static const struct OpbyteTemplate Opbyte[] =
{
#define MACRO(name, jnLayout, attrib, byte2, form, opbyte, ...) opbyte,
#include "MdOpcodes.h"
#undef MACRO
};

static const uint32 Opdope[] =
{
#define MACRO(name, jnLayout, attrib, byte2, form, opbyte, dope, ...) dope,
#include "MdOpcodes.h"
#undef MACRO
};

static const BYTE RegEncode[] =
{
#define REGDAT(Name, Listing, Encoding, ...) Encoding,
#include "RegList.h"
#undef REGDAT
};

static const enum Forms OpcodeForms[] =
{
#define MACRO(name, jnLayout, attrib, byte2, form, ...) form,
#include "MdOpcodes.h"
#undef MACRO
};

static const uint32 OpcodeLeadIn[] =
{
#define MACRO(name, jnLayout, attrib, byte2, form, opByte, dope, leadIn, ...) leadIn,
#include "MdOpcodes.h"
#undef MACRO
};

static const BYTE  Nop1[] = { 0x90 };                   /* nop                     */
static const BYTE  Nop2[] = { 0x66, 0x90 };             /* 66 nop                  */
static const BYTE  Nop3[] = { 0x0F, 0x1F, 0x00 };       /* nop dword ptr [eax]     */
static const BYTE  Nop4[] = { 0x0F, 0x1F, 0x40, 0x00 }; /* nop dword ptr [eax + 0] */
static const BYTE *Nop[4] = { Nop1, Nop2, Nop3, Nop4 };


enum CMP_IMM8
{
    EQ,
    LT,
    LE,
    UNORD,
    NEQ,
    NLT,
    NLE,
    ORD
};

///----------------------------------------------------------------------------
///
/// EncoderMD::Init
///
///----------------------------------------------------------------------------

void
EncoderMD::Init(Encoder *encoder)
{
    m_encoder = encoder;
    m_relocList = nullptr;
    m_lastLoopLabelPosition = -1;
}


///----------------------------------------------------------------------------
///
/// EncoderMD::GetOpcodeByte2
///
///     Get the second byte encoding of the given instr.
///
///----------------------------------------------------------------------------

const BYTE
EncoderMD::GetOpcodeByte2(IR::Instr *instr)
{
    return OpcodeByte2[instr->m_opcode - (Js::OpCode::MDStart+1)];
}

///----------------------------------------------------------------------------
///
/// EncoderMD::GetInstrForm
///
///     Get the form list of the given instruction.  The form list contains
///     the possible encoding forms of an instruction.
///
///----------------------------------------------------------------------------

Forms
EncoderMD::GetInstrForm(IR::Instr *instr)
{
    return OpcodeForms[instr->m_opcode - (Js::OpCode::MDStart+1)];
}

const BYTE *
EncoderMD::GetFormTemplate(IR::Instr *instr)
{
    return OpcodeFormTemplate[instr->m_opcode - (Js::OpCode::MDStart + 1)].form;
}

///----------------------------------------------------------------------------
///
/// EncoderMD::GetOpbyte
///
///     Get the first byte opcode of an instr.
///
///----------------------------------------------------------------------------

const BYTE *
EncoderMD::GetOpbyte(IR::Instr *instr)
{
    return Opbyte[instr->m_opcode - (Js::OpCode::MDStart+1)].opbyte;
}

///----------------------------------------------------------------------------
///
/// EncoderMD::GetRegEncode
///
///     Get the x86 encoding of a given register.
///
///----------------------------------------------------------------------------

const BYTE
EncoderMD::GetRegEncode(IR::RegOpnd *regOpnd)
{
    AssertMsg(regOpnd->GetReg() != RegNOREG, "RegOpnd should have valid reg in encoder");

    return RegEncode[regOpnd->GetReg()];
}

///----------------------------------------------------------------------------
///
/// EncoderMD::GetOpdope
///
///     Get the dope vector of a particular instr.  The dope vector describes
///     certain properties of an instr.
///
///----------------------------------------------------------------------------

const uint32
EncoderMD::GetOpdope(IR::Instr *instr)
{
    return Opdope[instr->m_opcode - (Js::OpCode::MDStart+1)];
}

///----------------------------------------------------------------------------
///
/// EncoderMD::GetLeadIn
///
///     Get the leadin of a particular instr.
///
///----------------------------------------------------------------------------

const uint32
EncoderMD::GetLeadIn(IR::Instr * instr)
{
    return OpcodeLeadIn[instr->m_opcode - (Js::OpCode::MDStart+1)];
}
///----------------------------------------------------------------------------
///
/// EncoderMD::FitsInByte
///
///     Can we encode this value into a signed extended byte?
///
///----------------------------------------------------------------------------

bool
EncoderMD::FitsInByte(size_t value)
{
    return ((size_t)(signed char)(value & 0xFF) == value);
}

///----------------------------------------------------------------------------
///
/// EncoderMD::GetMod
///
///     Get the "MOD" part of a MODRM encoding for a given operand.
///
///----------------------------------------------------------------------------

BYTE
EncoderMD::GetMod(IR::IndirOpnd * opr, int* pDispSize)
{
    return GetMod(opr->AsIndirOpnd()->GetOffset(), (opr->GetBaseOpnd()->GetReg() == RegEBP), pDispSize);
}

BYTE
EncoderMD::GetMod(IR::SymOpnd * symOpnd, int * pDispSize, RegNum& rmReg)
{
    StackSym * stackSym = symOpnd->m_sym->AsStackSym();
    int32 offset = stackSym->m_offset;
    rmReg = RegEBP;
    if (stackSym->IsArgSlotSym() && !stackSym->m_isOrphanedArg)
    {
        if (stackSym->m_isInlinedArgSlot)
        {
            Assert(offset >= 0);
            offset -= this->m_func->m_localStackHeight;
            stackSym->m_offset = offset;
            stackSym->m_allocated = true;
        }
        else
        {
            rmReg = RegESP;
        }
    }
    else
    {
        Assert(offset != 0);
    }
    return GetMod(offset + symOpnd->m_offset, rmReg == RegEBP, pDispSize);
}

BYTE
EncoderMD::GetMod(size_t offset, bool baseRegIsEBP, int * pDispSize)
{
    if (offset == 0 && !baseRegIsEBP)
    {
        *(pDispSize) = 0;
        return 0x00;
    }
    else if (this->FitsInByte(offset))
    {
        *(pDispSize) = 1;
        return 0x40;
    }
    else
    {
        *(pDispSize) = 4;
        return 0x80;
    }
}

///----------------------------------------------------------------------------
///
/// EncoderMD::EmitModRM
///
///     Emit an effective address using the MODRM byte.
///
///----------------------------------------------------------------------------

void
EncoderMD::EmitModRM(IR::Instr * instr, IR::Opnd *opnd, BYTE reg1)
{
    RegNum  rmReg;
    int dispSize;
    IR::IndirOpnd *indirOpnd;
    IR::RegOpnd *regOpnd;
    IR::RegOpnd *baseOpnd;
    IR::RegOpnd *indexOpnd;
    BYTE reg;
    BYTE regBase;
    BYTE regIndex;

#ifdef DBG
    dispSize = -1;
#endif

    reg1 = (reg1 & 7) << 3;       // mask and put in reg field

    switch (opnd->GetKind())
    {
    case IR::OpndKindReg:
        regOpnd = opnd->AsRegOpnd();

        AssertMsg(regOpnd->GetReg() != RegNOREG, "All regOpnd should have a valid reg set during encoder");

        reg = this->GetRegEncode(regOpnd);

        // Special handling for TEST_AH
        if (instr->m_opcode == Js::OpCode::TEST_AH)
        {
            // We can't represent AH in the IR.  We should have AL now, add 4 to represent AH.
            Assert(regOpnd->GetReg() == RegEAX && regOpnd->GetType() == TyInt8);
            reg += 4;
        }
        this->EmitConst((0xC0| reg1 | reg), 1);

        return;

    case IR::OpndKindSym:
        AssertMsg(opnd->AsSymOpnd()->m_sym->IsStackSym(), "Should only see stackSym syms in encoder.");

        BYTE mod;
        BYTE byte;
        uint32 baseRegEncode;

        mod = this->GetMod(opnd->AsSymOpnd(), &dispSize, rmReg);
        AssertMsg(rmReg != RegNOREG, "rmReg should have been set");
        baseRegEncode = rmReg - RegEAX;
        byte = (BYTE)(mod | reg1 | baseRegEncode);
        *(m_pc++) = byte;
        if (rmReg == RegESP)
        {
            byte = (BYTE)(((baseRegEncode & 7) << 3) | (baseRegEncode & 7));
            *(m_pc++) = byte;
        }
        else
        {
            AssertMsg(opnd->AsSymOpnd()->m_sym->AsStackSym()->m_offset, "Expected stackSym offset to be set.");
        }

        break;

    case IR::OpndKindIndir:
        indirOpnd = opnd->AsIndirOpnd();
        AssertMsg(indirOpnd->GetBaseOpnd() != nullptr, "Expected base to be set in indirOpnd");

        baseOpnd = indirOpnd->GetBaseOpnd();
        indexOpnd = indirOpnd->GetIndexOpnd();
        AssertMsg(!indexOpnd || indexOpnd->GetReg() != RegESP, "ESP cannot be the index of an indir.");

        regBase = this->GetRegEncode(baseOpnd);
        if (indexOpnd != nullptr)
        {
            regIndex = this->GetRegEncode(indexOpnd);
            *(m_pc++) = (this->GetMod(indirOpnd, &dispSize) | reg1 | 0x4);
            *(m_pc++) = (((indirOpnd->GetScale() & 3) << 6) | ((regIndex & 7) << 3) | (regBase & 7));
        }
        else
        {
            *(m_pc++) = (this->GetMod(indirOpnd, &dispSize) | reg1 | regBase);
            if (baseOpnd->GetReg() == RegESP)
            {
                // needs SIB byte
                *(m_pc++) = ((regBase & 7) << 3) | (regBase & 7);
            }
        }
        break;

    case IR::OpndKindMemRef:
        *(m_pc++) = (char)(reg1 | 0x5);
        dispSize = 4;
        break;

    default:
#if DBG
        AssertMsg(UNREACHED, "Unexpected operand kind");
        dispSize = -1;  // Satisfy prefast
        break;
#else
        __assume(UNREACHED);
#endif
    }

    AssertMsg(dispSize != -1, "Uninitialized dispSize");

    this->EmitImmed(opnd, dispSize, 0);
}

///----------------------------------------------------------------------------
///
/// EncoderMD::EmitConst
///
///     Emit a constant of the given size.
///
///----------------------------------------------------------------------------

void
EncoderMD::EmitConst(size_t val, int size)
{
    switch (size) {
    case 0:
        return;

    case 1:
        *(uint8*)m_pc = (char)val;
        break;

    case 2:
        *(uint16*)m_pc = (short)val;
        break;

    case 4:
        *(uint32*)m_pc = (uint32)val;
        break;

    default:
        AssertMsg(UNREACHED, "Unexpected size");
    }
    m_pc += size;
}

///----------------------------------------------------------------------------
///
/// EncoderMD::EmitImmed
///
///     Emit the immediate value of the given operand.  It returns 0x2 for a
///     sbit encoding, 0 otherwise.
///
///----------------------------------------------------------------------------

int
EncoderMD::EmitImmed(IR::Opnd * opnd, int opSize, int sbit)
{
    int retval = 0;
    StackSym *stackSym;
    size_t value = 0;

    switch (opnd->GetKind()) {
    case IR::OpndKindAddr:
        value = (uint32)opnd->AsAddrOpnd()->m_address;
        goto intConst;

    case IR::OpndKindIntConst:
        value = opnd->AsIntConstOpnd()->GetValue();
intConst:
        if (sbit && opSize > 1 && this->FitsInByte(value))
        {
            opSize = 1;
            retval = 0x2;   /* set S bit */
        }
        break;

    case IR::OpndKindSym:
        AssertMsg(opnd->AsSymOpnd()->m_sym->IsStackSym(), "Should only see stackSym here");
        stackSym = opnd->AsSymOpnd()->m_sym->AsStackSym();
        value = stackSym->m_offset + opnd->AsSymOpnd()->m_offset;
        break;

    case IR::OpndKindIndir:
        value = opnd->AsIndirOpnd()->GetOffset();
        break;

    case IR::OpndKindMemRef:
        value = (size_t)opnd->AsMemRefOpnd()->GetMemLoc();
        break;

    default:
        AssertMsg(UNREACHED, "Unimplemented...");
    }

    this->EmitConst(value, opSize);

    return retval;
}

///----------------------------------------------------------------------------
///
/// EncoderMD::EmitCondBranch
///
///     First pass of branch encoding: create a branch reloc that pairs the
/// branch with its label and records the branch's byte offset.
///
///----------------------------------------------------------------------------

void
EncoderMD::EmitCondBranch(IR::BranchInstr * branchInstr)
{
    IR::LabelInstr * labelInstr;

    // TODO: Make this more table-driven by mapping opcodes to condcodes.
    // (Will become more useful when we're emitting short branches as well.)

    switch (branchInstr->m_opcode)
    {
    case Js::OpCode::JA:
        *(m_pc++) = 0x87;
        break;

    case Js::OpCode::JAE:
        *(m_pc++) = 0x83;
        break;

    case Js::OpCode::JEQ:
        *(m_pc++) = 0x84;
        break;

    case Js::OpCode::JNE:
        *(m_pc++) = 0x85;
        break;

    case Js::OpCode::JNP:
        *(m_pc++) = 0x8B;
        break;

    case Js::OpCode::JB:
        *(m_pc++) = 0x82;
        break;

    case Js::OpCode::JBE:
        *(m_pc++) = 0x86;
        break;

    case Js::OpCode::JLT:
        *(m_pc++) = 0x8c;
        break;

    case Js::OpCode::JLE:
        *(m_pc++) = 0x8e;
        break;

    case Js::OpCode::JGT:
        *(m_pc++) = 0x8f;
        break;

    case Js::OpCode::JGE:
        *(m_pc++) = 0x8d;
        break;

    case Js::OpCode::JO:
        *(m_pc++) = 0x80;
        break;

    case Js::OpCode::JP:
        *(m_pc++) = 0x8A;
        break;

    case Js::OpCode::JNO:
        *(m_pc++) = 0x81;
        break;

    case Js::OpCode::JSB:
        *(m_pc++) = 0x88;
        break;

    case Js::OpCode::JNSB:
        *(m_pc++) = 0x89;
        break;

    default:
        AssertMsg(0, "Unsupported branch opcode");
        break;
    }

    AppendRelocEntry(RelocTypeBranch, (void*) m_pc);

    // Save the target LabelInstr's address in the encoder buffer itself, using the 4-byte
    // pcrel field of the branch instruction. This only works for long branches, obviously.
    labelInstr = branchInstr->GetTarget();
    this->EmitConst((uint32)labelInstr, MachInt);
}

///----------------------------------------------------------------------------
///
/// EncoderMD::Encode
///
///     Emit the x86 encoding for the given instruction in the passed in
///     buffer ptr.
///
///----------------------------------------------------------------------------

ptrdiff_t
EncoderMD::Encode(IR::Instr *instr, BYTE *pc, BYTE* beginCodeAddress)
{
    BYTE *opcodeByte;
    BYTE *instrStart, *instrRestart;

    m_pc = pc;

    pc = nullptr;  // just to avoid using it...

    if (instr->IsLowered() == false)
    {
        if (instr->IsLabelInstr())
        {
            IR::LabelInstr *labelInstr = instr->AsLabelInstr();
            labelInstr->SetPC(m_pc);
            if(!labelInstr->IsUnreferenced())
            {
                int relocEntryPosition = AppendRelocEntry(RelocTypeLabel, (void*) instr);
                if (!PHASE_OFF(Js::LoopAlignPhase, m_func))
                {
                    // we record position of last loop-top label (leaf loops) for loop alignment
                    if (labelInstr->m_isLoopTop && labelInstr->GetLoop()->isLeaf)
                    {
                        m_relocList->Item(relocEntryPosition).m_type = RelocType::RelocTypeAlignedLabel;
                    }
                }
            }
        }
#if DBG_DUMP
        if( instr->IsEntryInstr() && Js::Configuration::Global.flags.DebugBreak.Contains( m_func->GetFunctionNumber() ) )
        {
            IR::Instr *int3 = IR::Instr::New(Js::OpCode::INT, m_func);
            int3->SetSrc1(IR::IntConstOpnd::New(3, TyMachReg, m_func));

            return this->Encode(int3, m_pc);
        }
#endif
        return 0;
    }

    IR::Opnd  *dst = instr->GetDst();
    IR::Opnd  *src1 = instr->GetSrc1();
    IR::Opnd  *src2 = instr->GetSrc2();
    IR::Opnd  *opr1;
    IR::Opnd  *opr2;

    // Canonicalize operands.
    if (this->GetOpdope(instr) & DDST)
    {
        opr1 = dst;
        opr2 = src1;
    }
    else
    {
        opr1 = src1;
        opr2 = src2;
    }

    int instrSize = TySize[opr1 != nullptr? opr1->GetType() : 0];

    const BYTE *form = EncoderMD::GetFormTemplate(instr);
    const BYTE *opcodeTemplate = EncoderMD::GetOpbyte(instr);
    const uint32 leadIn = EncoderMD::GetLeadIn(instr);
    instrRestart = instrStart = m_pc;

    if (instrSize == 2 && (this->GetOpdope(instr) & (DNO16|DFLT)) == 0)
    {
        *instrRestart++ = 0x66;
    }
    if (this->GetOpdope(instr) & D66EX)
    {
        if (opr1->IsFloat64() || opr2->IsFloat64())
        {
            *instrRestart++ = 0x66;
        }
    }
    if (this->GetOpdope(instr) & (DZEROF|DF2|DF3|D66))
    {
        if (this->GetOpdope(instr) & DZEROF)
        {
        }
        else if (this->GetOpdope(instr) & DF2)
        {
            *instrRestart++ = 0xf2;
        }
        else if (this->GetOpdope(instr) & DF3)
        {
            *instrRestart++ = 0xf3;
        }
        else if (this->GetOpdope(instr) & D66)
        {
            *instrRestart++ = 0x66;
        }
        else
        {
            Assert(UNREACHED);
        }

        *instrRestart++ = 0xf;

        switch(leadIn)
        {
        case OLB_NONE:
            break;

        case OLB_0F3A:
            *instrRestart++ = 0x3a;
            break;

        default:
            Assert(UNREACHED);
            __assume(UNREACHED);
        }

    }

    // Try each form 1 by 1, until we find the one appropriate for this instruction
    // and its operands

    for(;; opcodeTemplate++, form++)
    {
        AssertMsg(m_pc - instrStart <= MachMaxInstrSize, "MachMaxInstrSize not set correctly");

        m_pc = instrRestart;
        opcodeByte = m_pc;

        // Set first opcode byte.

        *(m_pc++) = *opcodeTemplate;

        switch ((*form) & FORM_MASK)
        {
        case AX_IM:
            AnalysisAssert(opr1);
            if (!opr1->IsRegOpnd() || opr1->AsRegOpnd()->GetReg() != RegEAX
                || !opr2->IsImmediateOpnd())
            {
                continue;
            }

            size_t value;
            switch (opr2->GetKind())
            {
            case IR::OpndKindIntConst:
                value = opr2->AsIntConstOpnd()->GetValue();
                break;
            case IR::OpndKindAddr:
                value = (size_t)opr2->AsAddrOpnd()->m_address;
                break;
            default:
                Assert(UNREACHED);
                __assume(false);
            }

            if ((*form & SBIT) && FitsInByte(value))
            {
                // If the SBIT is set on this form, then it means
                // that there is a short immediate form of this instruction
                // available, and the short immediate encoding is a bit
                // smaller for DWORD sized instrs
                if (instrSize == 4)
                {
                    continue;
                }

                // Stay away from the 16-bit immediate form below.  It will
                // cause an LCP stall.  Use the 8-bit sign extended immediate
                // form which uses the same number of instruction bytes.
                if (instrSize == 2)
                {
                    continue;
                }
            }

            *opcodeByte |= this->EmitImmed(opr2, instrSize, 0);
            break;

        case AX_MEM:
            continue;

        // general case immediate.  Special cases have already been checked
        case IMM:
            if (!opr2->IsImmediateOpnd() && !opr2->IsLabelOpnd())
            {
                continue;
            }

            this->EmitModRM(instr, opr1, this->GetOpcodeByte2(instr) >> 3);
            if (opr2->IsLabelOpnd())
            {
                AppendRelocEntry( RelocTypeLabelUse, (void*) m_pc);
                this->EmitConst((uint32)opr2->AsLabelOpnd()->GetLabel(), 4);
            }
            else
            {
                *opcodeByte |= this->EmitImmed(opr2, instrSize, *form & SBIT);
            }
            break;

        case NO:
            {
                BYTE byte2 = this->GetOpcodeByte2(instr);

                if (byte2)
                {
                    *(m_pc)++ = byte2;
                }
            }
            break;

        // Short immediate/reg
        case SHIMR:
            if (!opr1->IsRegOpnd())
            {
                continue;
            }
            if (!opr2->IsIntConstOpnd() && !opr2->IsAddrOpnd())
            {
                continue;
            }
            *opcodeByte |= this->GetRegEncode(opr1->AsRegOpnd());
            if (instrSize > 1)
            {
                *opcodeByte |= 0x8; /* set the W bit */
            }
            this->EmitImmed(opr2, instrSize, 0);  /* S bit known to be 0 */
            break;

        case AXMODRM:
            AssertMsg(opr1->AsRegOpnd()->GetReg() == RegEAX, "Expected src1 of IMUL/IDIV to be EAX");

            opr1 = opr2;
            opr2 = nullptr;

            // FALLTHROUGH
        case MODRM:
modrm:
            if ((instr->m_opcode == Js::OpCode::MOVSD || instr->m_opcode == Js::OpCode::MOVSS) &&
                (!opr1->IsRegOpnd() || !REGNUM_ISXMMXREG(opr1->AsRegOpnd()->GetReg())))
            {
                *opcodeByte |= 1;
            }

            if (opr2 == nullptr)
            {
                BYTE byte2 = (this->GetOpcodeByte2(instr) >> 3);

                this->EmitModRM(instr, opr1, byte2);
                break;
            }

            if (opr1->IsRegOpnd())
            {
                this->EmitModRM(instr, opr2, this->GetRegEncode(opr1->AsRegOpnd()));

                if ((*form) & DBIT)
                {
                    *opcodeByte |= 0x2;     // set D bit
                }
            }
            else
            {
                AssertMsg(opr2->IsRegOpnd(), "Expected opr2 to be a valid reg");
                this->EmitModRM(instr, opr1, this->GetRegEncode(opr2->AsRegOpnd()));
            }
            break;

        /* floating pt with modrm, all are "binary" */
        case FUMODRM:
            /* make the opr1 be the mem operand (if any) */

            if (opr1->IsRegOpnd())
            {
                opr1 = opr2;
            }
            if (!opr1->IsRegOpnd() && (((*form) & FINT) ? instrSize == 2 : instrSize == 8))
            {
                *opcodeByte |= 4;   /* memsize bit */
            }
            this->EmitModRM(instr, opr1, this->GetOpcodeByte2(instr)>>3);
            break;

        // reg in opbyte. Only whole register allowed
        case SH_REG:
            if (!opr1->IsRegOpnd())
            {
                continue;
            }

            *opcodeByte |= this->GetRegEncode(opr1->AsRegOpnd());
            break;

        // short form immed. (must be unary)
        case SH_IM:
            if (!opr1->IsIntConstOpnd() && !opr1->IsAddrOpnd())
            {
                if (!opr1->IsLabelOpnd())
                {
                    continue;
                }
                AppendRelocEntry(RelocTypeLabelUse, (void*) m_pc);
                this->EmitConst((uint32)opr1->AsLabelOpnd()->GetLabel(), 4);
            }
            else
            {
                *opcodeByte |= this->EmitImmed(opr1, instrSize, 1);
            }
            break;

        case SHFT:
            this->EmitModRM(instr, opr1, this->GetOpcodeByte2(instr) >> 3);
            if (opr2->IsRegOpnd())
            {
                AssertMsg(opr2->AsRegOpnd()->GetReg() == RegECX, "Expected ECX as opr2 of variable shift");
                *opcodeByte |= *(opcodeTemplate + 1);
            }
            else
            {
                uint32 value;
                AssertMsg(opr2->IsIntConstOpnd(), "Expected register or constant as shift amount opnd");
                value = opr2->AsIntConstOpnd()->GetValue();
                if (value == 1)
                {
                    *opcodeByte |= 0x10;
                }
                else
                {
                    this->EmitConst(value, 1);
                }
            }
            break;

        case LABREL1:
            // TODO
            continue;

        // jmp, call with full relative disp
        case LABREL2:

            if (opr1 == nullptr)
            {
                // Unconditional branch
                AssertMsg(instr->IsBranchInstr(), "Invalid LABREL2 form");

                AppendRelocEntry(RelocTypeBranch, (void*)m_pc);

                // Save the target LabelInstr's address in the encoder buffer itself, using the 4-byte
                // pcrel field of the branch instruction. This only works for long branches, obviously.
                this->EmitConst((uint32)instr->AsBranchInstr()->GetTarget(), 4);
            }
            else if (opr1->IsIntConstOpnd())
            {
                AppendRelocEntry(RelocTypeCallPcrel, (void*)m_pc);
                this->EmitConst(opr1->AsIntConstOpnd()->GetValue(), 4);
                AssertMsg( ( ((BYTE*)opr1->AsIntConstOpnd()->GetValue()) < m_encoder->m_encodeBuffer || ((BYTE *)opr1->AsIntConstOpnd()->GetValue()) >= m_encoder->m_encodeBuffer + m_encoder->m_encodeBufferSize), "Call Target within buffer.");
            }
            else if (opr1->IsHelperCallOpnd())
            {
                AppendRelocEntry(RelocTypeCallPcrel, (void*)m_pc);
                const void* fnAddress = IR::GetMethodAddress(opr1->AsHelperCallOpnd());
                AssertMsg(sizeof(uint32) == sizeof(void*), "Sizes of void* assumed to be 32-bits");
                this->EmitConst((uint32)fnAddress, 4);
                AssertMsg( (((BYTE*)fnAddress) < m_encoder->m_encodeBuffer || ((BYTE *)fnAddress) >= m_encoder->m_encodeBuffer + m_encoder->m_encodeBufferSize), "Call Target within buffer.");
            }
            else
            {
                continue;
            }
            break;

        // Special form which doesn't fit any existing patterns.

        case SPECIAL:

            switch (instr->m_opcode)
            {
            case Js::OpCode::RET: {
                AssertMsg(opr1->IsIntConstOpnd(), "RET should have intConst as src");
                uint32 value = opr1->AsIntConstOpnd()->GetValue();

                if (value==0)
                {
                    *opcodeByte |= 0x1; // no imm16 follows
                }
                else {
                    this->EmitConst(value, 2);
                }
                break;
            }

            case Js::OpCode::JA:
            case Js::OpCode::JAE:
            case Js::OpCode::JEQ:
            case Js::OpCode::JB:
            case Js::OpCode::JBE:
            case Js::OpCode::JNE:
            case Js::OpCode::JLT:
            case Js::OpCode::JLE:
            case Js::OpCode::JGT:
            case Js::OpCode::JGE:
            case Js::OpCode::JO:
            case Js::OpCode::JNO:
            case Js::OpCode::JP:
            case Js::OpCode::JNP:
            case Js::OpCode::JSB:
            case Js::OpCode::JNSB:
            {
                *opcodeByte = 0xf;
                this->EmitCondBranch(instr->AsBranchInstr());
                break;
            }

            case Js::OpCode::IMUL2:
                AssertMsg(opr1->IsRegOpnd() && instrSize != 1, "Illegal IMUL2");

                if (!opr2->IsImmediateOpnd())
                {
                    continue;
                }

                // turn an 'imul2 reg, immed' into an 'imul3 reg, reg, immed'

                *instrStart = *opcodeTemplate;          // hammer prefix
                opcodeByte = instrStart;                // reset pointers
                m_pc = instrStart + 1;

                this->EmitModRM(instr, opr1, this->GetRegEncode(opr1->AsRegOpnd()));

                *opcodeByte |= this->EmitImmed(opr2, instrSize, 1);
                break;

            case Js::OpCode::INT:
                if (opr1->AsIntConstOpnd()->GetValue() != 3)
                {
                    *opcodeByte |= 1;
                    *(m_pc)++ = (char)opr1->AsIntConstOpnd()->GetValue();
                }
                break;

            case Js::OpCode::FSTP:
                if (opr1->IsRegOpnd())
                {
                    *opcodeByte |= 4;
                    this->EmitModRM(instr, opr1, this->GetOpcodeByte2(instr)>>3);
                    break;
                }
                if (instrSize != 10)
                {
                    continue;
                }
                *opcodeByte |= 2;
                this->EmitModRM(instr, opr1, (this->GetOpcodeByte2(instr) >> 3)|5);
                break;

            case Js::OpCode::MOVD:

                // is second operand a MMX register? if so use "store" form

                if (opr2->IsRegOpnd() && REGNUM_ISXMMXREG(opr2->AsRegOpnd()->GetReg()))
                {
                    if (opr1->IsRegOpnd())
                    {
                        // have 2 choices - we do it this way to match Intel's
                        // tools; Have to swap operands to get right behavior from
                        // modrm code.
                        IR::Opnd *oprTmp = opr1;
                        opr1 = opr2;
                        opr2 = oprTmp;
                    }
                    *opcodeByte |= 0x10;
                }
                Assert(opr1->IsRegOpnd() && REGNUM_ISXMMXREG(opr1->AsRegOpnd()->GetReg()));
                goto modrm;

            case Js::OpCode::MOVLHPS:
            case Js::OpCode::MOVHLPS:
                Assert(opr1->IsRegOpnd() && REGNUM_ISXMMXREG(opr1->AsRegOpnd()->GetReg()));
                Assert(opr2->IsRegOpnd() && REGNUM_ISXMMXREG(opr2->AsRegOpnd()->GetReg()));
                goto modrm;

            case Js::OpCode::MOVMSKPD:
            case Js::OpCode::MOVMSKPS:
                /* Instruction form is "MOVMSKP[S/D] r32, xmm" */
                Assert(opr1->IsRegOpnd() && opr2->IsRegOpnd() && REGNUM_ISXMMXREG(opr2->AsRegOpnd()->GetReg()));
                goto modrm;

            case Js::OpCode::MOVSS:
            case Js::OpCode::MOVAPS:
            case Js::OpCode::MOVUPS:
            case Js::OpCode::MOVHPD:
                if (!opr1->IsRegOpnd())
                {
                    Assert(opr2->IsRegOpnd());

                    *opcodeByte |= 0x01;
                }
                goto modrm;

            case Js::OpCode::FISTTP:
                if (opr1->GetSize() != 8)
                    continue;  /* handle 8bytes here, others the usual way */
                this->EmitModRM(instr, opr1, this->GetOpcodeByte2(instr) >> 3);
                break;
            case Js::OpCode::FLD:
                if (instrSize != 10)
                {
                    continue;
                }
                *opcodeByte |= 0x2;
                this->EmitModRM(instr, opr1, (this->GetOpcodeByte2(instr) >> 3)|5);
                break;

            case Js::OpCode::NOP:
                if (AutoSystemInfo::Data.SSE2Available() && instr->GetSrc1())
                {
                    // Multibyte NOP. Encode fast NOPs on SSE2 supported x86 system
                    Assert(instr->GetSrc1()->IsIntConstOpnd() && instr->GetSrc1()->GetType() == TyInt8);
                    unsigned nopSize = instr->GetSrc1()->AsIntConstOpnd()->GetValue();
                    Assert(nopSize >= 2 && nopSize <= 4);
                    nopSize = max(2u, min(4u, nopSize)); // satisfy oacr
                    const BYTE *nopEncoding = Nop[nopSize - 1];
                    *opcodeByte = nopEncoding[0];
                    for (unsigned i = 1; i < nopSize; i++)
                    {
                        *(m_pc)++ = nopEncoding[i];
                    }
                }
                else
                {
                    BYTE byte2 = this->GetOpcodeByte2(instr);

                    if (byte2)
                    {
                        *(m_pc)++ = byte2;
                    }
                }
                break;

            case Js::OpCode::XCHG:
                if (instrSize == 1)
                    continue;

                if (opr1->IsRegOpnd() && opr1->AsRegOpnd()->GetReg() == RegEAX
                    && opr2->IsRegOpnd())
                {
                    *opcodeByte |= this->GetRegEncode(opr2->AsRegOpnd());
                }
                else if (opr2->IsRegOpnd() && opr2->AsRegOpnd()->GetReg() == RegEAX
                    && opr1->IsRegOpnd())
                {
                    *opcodeByte |= this->GetRegEncode(opr1->AsRegOpnd());
                }
                else
                {
                    continue;
                }
                break;
            case Js::OpCode::BT:
            case Js::OpCode::BTR:
                /*
                *       0F A3 /r      BT  r/m16, r16
                *       0F A3 /r      BT  r/m32, r32
                *       0F BA /4 ib   BT  r/m16, imm8
                *       0F BA /4 ib   BT  r/m32, imm8
                * or
                *       0F B3 /r      BTR r/m16, r32
                *       0F B3 /r      BTR r/m32, r64
                *       0F BA /6 ib   BTR r/m16, imm8
                *       0F BA /6 ib   BTR r/m32, imm8
                */
                Assert(instr->m_opcode != Js::OpCode::BT || dst == nullptr);
                AssertMsg(instr->m_opcode == Js::OpCode::BT ||
                    dst && (dst->IsRegOpnd() || dst->IsMemRefOpnd() || dst->IsIndirOpnd()), "Invalid dst type on BTR/BTS instruction.");

                if (src2->IsImmediateOpnd())
                {
                    this->EmitModRM(instr, src1, this->GetOpcodeByte2(instr) >> 3);
                    Assert(src2->IsIntConstOpnd() && src2->GetType() == TyInt8);
                    *opcodeByte |= EmitImmed(src2, 1, 0);
                }
                else
                {
                    /* this is special dbit modrm in which opr1 can be a reg*/
                    Assert(src2->IsRegOpnd());
                    form++;
                    opcodeTemplate++;
                    Assert(((*form) & FORM_MASK) == MODRM);
                    *opcodeByte = *opcodeTemplate;
                    this->EmitModRM(instr, src1, this->GetRegEncode(src2->AsRegOpnd()));
                }
                break;
            case Js::OpCode::PSLLDQ:
            case Js::OpCode::PSRLDQ:
                // SSE shift
                Assert(opr1->IsRegOpnd());
                this->EmitModRM(instr, opr1, this->GetOpcodeByte2(instr) >> 3);
                break;

            default:
                AssertMsg(UNREACHED, "Unhandled opcode in SPECIAL form");
            }
            break;

        case INVALID:
            return m_pc - instrStart;

        default:

            AssertMsg(UNREACHED, "Unhandled encoder form");
        }

        // if instr has W bit, set it appropriately
        if ((*form & WBIT) && !(this->GetOpdope(instr) & DFLT) && instrSize != 1)
        {
            *opcodeByte |= 0x1; // set WBIT
        }

        AssertMsg(m_pc - instrStart <= MachMaxInstrSize, "MachMaxInstrSize not set correctly");
        if (this->GetOpdope(instr) & DSSE)
        {
            // extra imm8 byte for SSE instructions.
            uint valueImm = 0;
            if (src2 &&src2->IsIntConstOpnd())
            {
                valueImm = src2->AsIntConstOpnd()->GetImmediateValue();
            }
            else
            {
                // src2(comparison byte) is missing in CMP instructions and is part of the opcode instead.
                switch (instr->m_opcode)
                {
                case Js::OpCode::CMPLTPS:
                case Js::OpCode::CMPLTPD:
                    valueImm = CMP_IMM8::LT;
                    break;
                case Js::OpCode::CMPLEPS:
                case Js::OpCode::CMPLEPD:
                    valueImm = CMP_IMM8::LE;
                    break;
                case Js::OpCode::CMPEQPS:
                case Js::OpCode::CMPEQPD:
                    valueImm = CMP_IMM8::EQ;
                    break;
                case Js::OpCode::CMPNEQPS:
                case Js::OpCode::CMPNEQPD:
                    valueImm = CMP_IMM8::NEQ;
                    break;
                default:
                    Assert(UNREACHED);
                }
            }
            *(m_pc++) = (valueImm & 0xff);
        }
        return m_pc - instrStart;
    }
}

int
EncoderMD::AppendRelocEntry(RelocType type, void *ptr)
{
    if (m_relocList == nullptr)
        m_relocList = Anew(m_encoder->m_tempAlloc, RelocList, m_encoder->m_tempAlloc);

    EncodeRelocAndLabels reloc;
    reloc.init(type, ptr);

    return m_relocList->Add(reloc);
}

int
EncoderMD::FixRelocListEntry(uint32 index, int32 totalBytesSaved, BYTE *buffStart, BYTE* buffEnd)
{
    BYTE* currentPc;
    EncodeRelocAndLabels &relocRecord = m_relocList->Item(index);
    int result = totalBytesSaved;

    // LabelInstr ?
    if (relocRecord.isLabel())
    {
        BYTE* newPC;
        currentPc = relocRecord.getLabelCurrPC();

        AssertMsg(currentPc >= buffStart && currentPc < buffEnd, "LabelInstr offset has to be within buffer.");
        newPC = currentPc - totalBytesSaved;

        // find the number of nops needed to align this loop top
        if (relocRecord.isAlignedLabel() && !PHASE_OFF(Js::LoopAlignPhase, m_func))
        {
            uint32 offset = (uint32)newPC - (uint32)buffStart;
            // Since the final code buffer is page aligned, it is enough to align the offset of the label.
            BYTE nopCount = m_encoder->FindNopCountFor16byteAlignment(offset);
            if (nopCount <= Js::Configuration::Global.flags.LoopAlignNopLimit)
            {
                // new label pc
                newPC += nopCount;
                relocRecord.setLabelNopCount(nopCount);
                // adjust bytes saved
                result -= nopCount;
            }
        }
        relocRecord.setLabelCurrPC(newPC);

    }
    else
    {
        currentPc = (BYTE*) relocRecord.m_origPtr;
        // ignore outside buffer offsets (e.g. JumpTable entries)
        if (currentPc >= buffStart && currentPc < buffEnd)
        {
            if (relocRecord.m_type == RelocTypeInlineeEntryOffset)
            {
                // ptr points to imm32 offset of the instruction that needs to be adjusted
                // offset is in top 28-bits, arg count in bottom 4
                uint32 field = *((uint32*) relocRecord.m_origPtr);
                uint32 offset = field >> 4;
                uint32 count = field & 0xf;

                AssertMsg(offset < (uint32)(buffEnd - buffStart), "Inlinee entry offset out of range");
                *((uint32*) relocRecord.m_origPtr) = ((offset - totalBytesSaved) << 4) | count;
            }
            // adjust the ptr to the buffer itself
            relocRecord.m_ptr = (BYTE*) relocRecord.m_ptr - totalBytesSaved;
        }
    }
    return result;
}

///----------------------------------------------------------------------------
///
/// EncoderMD::FixMaps
/// Fixes the inlinee frame records and map based on BR shortening
///
///----------------------------------------------------------------------------
void
EncoderMD::FixMaps(uint32 brOffset, int32 bytesSaved, uint32 *inlineeFrameRecordsIndex, uint32 *inlineeFrameMapIndex,  uint32 *pragmaInstToRecordOffsetIndex, uint32 *offsetBuffIndex)

{
    InlineeFrameRecords *recList = m_encoder->m_inlineeFrameRecords;
    InlineeFrameMap *mapList = m_encoder->m_inlineeFrameMap;
    PragmaInstrList *pInstrList = m_encoder->m_pragmaInstrToRecordOffset;
    int32 i;
    for (i = *inlineeFrameRecordsIndex; i < recList->Count() && recList->Item(i)->inlineeStartOffset <= brOffset; i++)
        recList->Item(i)->inlineeStartOffset -= bytesSaved;

    *inlineeFrameRecordsIndex = i;

    for (i = *inlineeFrameMapIndex; i < mapList->Count() && mapList->Item(i).offset <= brOffset; i++)
            mapList->Item(i).offset -= bytesSaved;

    *inlineeFrameMapIndex = i;

    for (i = *pragmaInstToRecordOffsetIndex; i < pInstrList->Count() && pInstrList->Item(i)->m_offsetInBuffer <= brOffset; i++)
        pInstrList->Item(i)->m_offsetInBuffer -= bytesSaved;

    *pragmaInstToRecordOffsetIndex = i;

#if DBG_DUMP
    for (i = *offsetBuffIndex; (uint)i < m_encoder->m_instrNumber && m_encoder->m_offsetBuffer[i] <= brOffset; i++)
        m_encoder->m_offsetBuffer[i] -= bytesSaved;

    *offsetBuffIndex = i;
#endif
}



///----------------------------------------------------------------------------
///
/// EncoderMD::ApplyRelocs
/// We apply relocations to the temporary buffer using the target buffer's address
/// before we copy the contents of the temporary buffer to the target buffer.
///----------------------------------------------------------------------------
void
EncoderMD::ApplyRelocs(uint32 codeBufferAddress)
{
    for (int32 i = 0; i < m_relocList->Count(); i++)
    {
        EncodeRelocAndLabels *reloc = &m_relocList->Item(i);
        BYTE * relocAddress = (BYTE*)reloc->m_ptr;
        uint32 pcrel;

        switch (reloc->m_type)
        {
        case RelocTypeCallPcrel:
            {
                pcrel = (uint32)(codeBufferAddress + (BYTE*)reloc->m_ptr - m_encoder->m_encodeBuffer + 4);
                *(uint32 *)relocAddress -= pcrel;
                break;
            }
        case RelocTypeBranch:
            {
                IR::LabelInstr * labelInstr = reloc->getBrTargetLabel();
                AssertMsg(labelInstr->GetPC() != nullptr, "Branch to unemitted label?");
                if (reloc->isShortBr())
                {
                    // short branch
                    pcrel = (uint32)(labelInstr->GetPC() - ((BYTE*)reloc->m_ptr + 1));
                    AssertMsg((int32)pcrel >= -128 && (int32)pcrel <= 127, "Offset doesn't fit in imm8.");
                    *(BYTE*)relocAddress = (BYTE)pcrel;
                }
                else
                {
                    pcrel = (uint32)(labelInstr->GetPC() - ((BYTE*)reloc->m_ptr + 4));
                    *(uint32 *)relocAddress = pcrel;
                }
                break;
            }
        case RelocTypeLabelUse:
            {
                IR::LabelInstr * labelInstr = *(IR::LabelInstr**)relocAddress;
                AssertMsg(labelInstr->GetPC() != nullptr, "Branch to unemitted label?");
                *(uint32 *)relocAddress = (uint32)(labelInstr->GetPC() - m_encoder->m_encodeBuffer + codeBufferAddress);
                break;
            }
        case RelocTypeLabel:
        case RelocTypeAlignedLabel:
        case RelocTypeInlineeEntryOffset:
            break;
        default:
            AssertMsg(UNREACHED, "Unknown reloc type");
        }
    }
}

///----------------------------------------------------------------------------
///
/// EncodeRelocAndLabels::VerifyRelocList
/// Verify that the list of offsets within the encoder buffer range are in ascending order.
/// This includes offsets of immediate fields in the code and offsets of LabelInstrs
///----------------------------------------------------------------------------

#ifdef DBG
void
EncoderMD::VerifyRelocList(BYTE *buffStart, BYTE *buffEnd)
{
    BYTE *last_pc = 0, *pc;

    for (int32 i = 0; i < m_relocList->Count(); i ++)
    {
        EncodeRelocAndLabels &p = m_relocList->Item(i);
        // LabelInstr ?
        if (p.isLabel())
        {
            AssertMsg(p.m_ptr < buffStart || p.m_ptr >= buffEnd, "Invalid label instruction pointer.");
            pc = ((IR::LabelInstr*)p.m_ptr)->GetPC();
            AssertMsg(pc >= buffStart && pc < buffEnd, "LabelInstr offset has to be within buffer.");
        }
        else
            pc = (BYTE*)p.m_ptr;

        // The list is partially sorted, out of bound ptrs (JumpTable entries) don't follow.
        if (pc >= buffStart && pc < buffEnd)
        {
            if (last_pc)
                AssertMsg(pc >= last_pc, "Unordered reloc list.");
            last_pc = pc;
        }

    }

}
#endif

void
EncoderMD::EncodeInlineeCallInfo(IR::Instr *instr, uint32 codeOffset)
{
    Assert(instr->GetDst() &&
            instr->GetDst()->IsSymOpnd() &&
            instr->GetDst()->AsSymOpnd()->m_sym->IsStackSym() &&
            instr->GetDst()->AsSymOpnd()->m_sym->AsStackSym()->m_isInlinedArgSlot);

    Assert(instr->GetSrc1() &&
        instr->GetSrc1()->IsAddrOpnd() &&
        (instr->GetSrc1()->AsAddrOpnd()->m_address == (Js::Var)((size_t)instr->GetSrc1()->AsAddrOpnd()->m_address & 0xF)));

    Js::Var inlineeCallInfo = 0;
    // 28 (x86) bits on the InlineeCallInfo to store the
    // offset of the start of the inlinee. We shouldn't have gotten here with more arguments
    // than can fit in as many bits.
    const bool encodeResult = Js::InlineeCallInfo::Encode(inlineeCallInfo, (uint32)instr->GetSrc1()->AsAddrOpnd()->m_address, codeOffset);
    Assert(encodeResult);

    instr->GetSrc1()->AsAddrOpnd()->m_address = inlineeCallInfo;
}

bool EncoderMD::TryConstFold(IR::Instr *instr, IR::RegOpnd *regOpnd)
{
    Assert(regOpnd->m_sym->IsConst());

    switch(GetInstrForm(instr))
    {
    case FORM_MOV:
        if (!instr->GetSrc1()->IsRegOpnd())
        {
            return false;
        }
        break;

    case FORM_PSHPOP:
        if (instr->m_opcode != Js::OpCode::PUSH)
        {
            return false;
        }
        if (!instr->GetSrc1()->IsRegOpnd())
        {
            return false;
        }
        break;

    case FORM_BINOP:
    case FORM_SHIFT:
        if (regOpnd != instr->GetSrc2())
        {
            return false;
        }
        break;

    default:
        return false;
    }

    if(regOpnd != instr->GetSrc1() && regOpnd != instr->GetSrc2())
    {
        if(!regOpnd->m_sym->IsConst() || regOpnd->m_sym->IsFloatConst())
        {
            return false;
        }

        // Check if it's the index opnd inside an indir
        bool foundUse = false;
        bool foldedAllUses = true;
        IR::Opnd *const srcs[] = { instr->GetSrc1(), instr->GetSrc2(), instr->GetDst() };
        for(int i = 0; i < sizeof(srcs) / sizeof(srcs[0]); ++i)
        {
            const auto src = srcs[i];
            if(!src || !src->IsIndirOpnd())
            {
                continue;
            }

            const auto indir = src->AsIndirOpnd();
            if(regOpnd == indir->GetBaseOpnd())
            {
                // Can't const-fold into the base opnd
                foundUse = true;
                foldedAllUses = false;
                continue;
            }
            if(regOpnd != indir->GetIndexOpnd())
            {
                continue;
            }

            foundUse = true;
            if(!regOpnd->m_sym->IsIntConst())
            {
                foldedAllUses = false;
                continue;
            }

            // offset = indir.offset + (index << scale)
            IntConstType offset = regOpnd->m_sym->GetIntConstValue();
            if(indir->GetScale() != 0 && Int32Math::Shl(offset, indir->GetScale(), &offset) ||
                indir->GetOffset() != 0 && Int32Math::Add(indir->GetOffset(), offset, &offset))
            {
                foldedAllUses = false;
                continue;
            }
            indir->SetOffset(offset);
            indir->SetIndexOpnd(nullptr);
        }

        return foundUse && foldedAllUses;
    }

    instr->ReplaceSrc(regOpnd, regOpnd->m_sym->GetConstOpnd());
    return true;
}

bool EncoderMD::TryFold(IR::Instr *instr, IR::RegOpnd *regOpnd)
{
    IR::Opnd *src1 = instr->GetSrc1();
    IR::Opnd *src2 = instr->GetSrc2();

    switch(GetInstrForm(instr))
    {
    case FORM_MOV:
        if (!instr->GetDst()->IsRegOpnd() || regOpnd != src1)
        {
            return false;
        }
        break;

    case FORM_BINOP:

        if (regOpnd == src1 && instr->m_opcode == Js::OpCode::CMP && (src2->IsRegOpnd() || src1->IsImmediateOpnd()))
        {
            IR::Instr *instrNext = instr->GetNextRealInstrOrLabel();

            if (instrNext->IsBranchInstr() && instrNext->AsBranchInstr()->IsConditional())
            {
                // Swap src and reverse branch
                src2 = instr->UnlinkSrc1();
                src1 = instr->UnlinkSrc2();
                instr->SetSrc1(src1);
                instr->SetSrc2(src2);
                LowererMD::ReverseBranch(instrNext->AsBranchInstr());
            }
            else
            {
                return false;
            }
        }
        if (regOpnd != src2 || !src1->IsRegOpnd())
        {
            return false;
        }
        break;

    case FORM_MODRM:
        if (src2 == nullptr)
        {
            if (!instr->GetDst()->IsRegOpnd() || regOpnd != src1 || EncoderMD::IsOPEQ(instr))
            {
                return false;
            }
        }
        else
        {
            if (regOpnd != src2 || !src1->IsRegOpnd())
            {
                return false;
            }
        }
        break;

    case FORM_PSHPOP:
        if (instr->m_opcode != Js::OpCode::PUSH)
        {
            return false;
        }
        if (!instr->GetSrc1()->IsRegOpnd())
        {
            return false;
        }
        break;

    case FORM_TEST:
        if (regOpnd == src1)
        {
            if (!src2->IsRegOpnd() && !src2->IsIntConstOpnd())
            {
                return false;
            }
        }
        else if (src1->IsRegOpnd())
        {
            instr->SwapOpnds();
        }
        else
        {
            return false;
        }
        break;

    default:
        return false;
    }

    IR::SymOpnd *symOpnd = IR::SymOpnd::New(regOpnd->m_sym, regOpnd->GetType(), instr->m_func);
    instr->ReplaceSrc(regOpnd, symOpnd);
    return true;
}

bool EncoderMD::SetsConditionCode(IR::Instr *instr)
{
    return instr->IsLowered() && (EncoderMD::GetOpdope(instr) & DSETCC);
}

bool EncoderMD::UsesConditionCode(IR::Instr *instr)
{
    return instr->IsLowered() && (EncoderMD::GetOpdope(instr) & DUSECC);
}

void EncoderMD::UpdateRelocListWithNewBuffer(RelocList * relocList, BYTE * newBuffer, BYTE * oldBufferStart, BYTE * oldBufferEnd)
{
    for (int32 i = 0; i < relocList->Count(); i++)
    {
        EncodeRelocAndLabels &reloc = relocList->Item(i);
        if (reloc.isLabel())
        {
            IR::LabelInstr* label = reloc.getLabel();
            AssertMsg((BYTE*)label < oldBufferStart || (BYTE*)label >= oldBufferEnd, "Invalid label pointer.");

            BYTE* labelPC = label->GetPC();
            Assert((BYTE*) reloc.m_origPtr >= oldBufferStart && (BYTE*) reloc.m_origPtr < oldBufferEnd);

            label->SetPC(labelPC - oldBufferStart + newBuffer);
            // nothing more to be done for a label
            continue;
        }
        else if (reloc.m_type >= RelocTypeBranch && reloc.m_type <= RelocTypeLabelUse &&
            (BYTE*) reloc.m_origPtr >= oldBufferStart && (BYTE*) reloc.m_origPtr < oldBufferEnd)
        {
            // we need to relocate all new offset that were originally within buffer
            reloc.m_ptr = (BYTE*) reloc.m_ptr - oldBufferStart + newBuffer;
        }
    }
}

bool EncoderMD::IsOPEQ(IR::Instr *instr)
{
    return instr->IsLowered() && (EncoderMD::GetOpdope(instr) & DOPEQ);
}

void EncoderMD::AddLabelReloc(BYTE* relocAddress)
{
    AppendRelocEntry(RelocTypeLabel, relocAddress);
}
