//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "BackEnd.h"
#include "ARMEncode.h"
#include "Language\JavascriptFunctionArgIndex.h"

const FormTable * InstrEncode[]={
#define MACRO(name, jnLayout, attrib, byte2, form, opbyte, ...) opbyte,
#include "MdOpcodes.h"
#undef ASMDAT
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
}

///----------------------------------------------------------------------------
///
/// EncoderMD::GetRegEncode
///
///     Get the encoding of a given register.
///
///----------------------------------------------------------------------------

const BYTE
EncoderMD::GetRegEncode(IR::RegOpnd *regOpnd)
{
    return GetRegEncode(regOpnd->GetReg());
}

const BYTE
EncoderMD::GetRegEncode(RegNum reg)
{
    return RegEncode[reg];
}

const BYTE
EncoderMD::GetFloatRegEncode(IR::RegOpnd *regOpnd)
{
    //Each double register holds two single precision registers.
    BYTE regEncode = GetRegEncode(regOpnd->GetReg()) * 2;
    AssertMsg(regEncode <= LAST_FLOAT_REG_NUM, "Impossible to allocate higher registers on VFP");
    return regEncode;
}

///----------------------------------------------------------------------------
///
/// EncoderMD::GetOpdope
///
///     Get the dope vector of a particular instr.  The dope vector describes
///     certain properties of an instr.
///
///----------------------------------------------------------------------------

uint32
EncoderMD::GetOpdope(IR::Instr *instr)
{
    return GetOpdope(instr->m_opcode);
}

uint32
EncoderMD::GetOpdope(Js::OpCode op)
{
    return Opdope[op - (Js::OpCode::MDStart+1)];
}

//
// EncoderMD::CanonicalizeInstr :
//     Put the instruction in its final form for encoding. This may involve
// expanding a pseudo-op such as LEA or changing an opcode to indicate the
// op bits the encoder should use.
//
//     Return the size of the final instruction's encoding.
//

InstructionType EncoderMD::CanonicalizeInstr(IR::Instr* instr)
{
    if (!instr->IsLowered())
    {
        return InstructionType::None;
    }

    switch (instr->m_opcode)
    {

        CASE_OPCODES_ALWAYS_THUMB2
            return InstructionType::Thumb2;

        CASE_OPCODES_NEVER_THUMB2
            return InstructionType::Thumb;

        case Js::OpCode::MOV:
            return this->CanonicalizeMov(instr);

        case Js::OpCode::B:
            return InstructionType::Thumb2;  // For now only T2 branches are encoded

        case Js::OpCode::BL:
            return InstructionType::Thumb2;

        case Js::OpCode::BNE:
        case Js::OpCode::BEQ:
        case Js::OpCode::BLT:
        case Js::OpCode::BLE:
        case Js::OpCode::BGE:
        case Js::OpCode::BGT:
        case Js::OpCode::BCS:
        case Js::OpCode::BCC:
        case Js::OpCode::BHI:
        case Js::OpCode::BLS:
        case Js::OpCode::BMI:
        case Js::OpCode::BPL:
        case Js::OpCode::BVS:
        case Js::OpCode::BVC:
            return InstructionType::Thumb2;  // For now only T2 branches are encoded

        case Js::OpCode::CMP:
            return this->CmpEncodeType(instr);

        case Js::OpCode::CMN:
            return this->CmnEncodeType(instr);

        case Js::OpCode::CMP_ASR31:
            return InstructionType::Thumb2;

        case Js::OpCode::POP:
            return this->PushPopEncodeType(instr->GetSrc1()->AsIndirOpnd(), instr->GetDst()->AsRegBVOpnd());

        case Js::OpCode::PUSH:
            return this->PushPopEncodeType(instr->GetDst()->AsIndirOpnd(), instr->GetSrc1()->AsRegBVOpnd());

        case Js::OpCode::LDR:
            return this->CanonicalizeLoad(instr);

        case Js::OpCode::STR:
            return this->CanonicalizeStore(instr);

        case Js::OpCode::LEA:
            return this->CanonicalizeLea(instr);

        case Js::OpCode::ADD:
        case Js::OpCode::ADDS:
            return this->CanonicalizeAdd(instr);

        case Js::OpCode::SUB:
        case Js::OpCode::SUBS:
            return this->CanonicalizeSub(instr);

        case Js::OpCode::AND:
        case Js::OpCode::EOR:
        case Js::OpCode::MUL:
        case Js::OpCode::ORR:
        case Js::OpCode::RSB:
        case Js::OpCode::RSBS:
        case Js::OpCode::BIC:
            return this->Alu3EncodeType(instr);

        case Js::OpCode::EOR_ASR31:
            return InstructionType::Thumb2;

        case Js::OpCode::SMULL:
        case Js::OpCode::SMLAL:
            return InstructionType::Thumb2;

        case Js::OpCode::MVN:
            return this->Alu2EncodeType(instr->GetDst(), instr->GetSrc1());

        case Js::OpCode::TST:
            return this->Alu2EncodeType(instr->GetSrc1(), instr->GetSrc2());

        case Js::OpCode::ASR:
        case Js::OpCode::ASRS:
        case Js::OpCode::LSL:
        case Js::OpCode::LSR:
            return this->ShiftEncodeType(instr);

        case Js::OpCode::VSTR:
        case Js::OpCode::VSTR32:
        case Js::OpCode::VLDR:
        case Js::OpCode::VLDR32:
        case Js::OpCode::VABS:
        case Js::OpCode::VSQRT:
        case Js::OpCode::VMOV:
        case Js::OpCode::VMOVARMVFP:
        case Js::OpCode::VCVTF64F32:
        case Js::OpCode::VCVTF32F64:
        case Js::OpCode::VCVTF64S32:
        case Js::OpCode::VCVTF64U32:
        case Js::OpCode::VCVTS32F64:
        case Js::OpCode::VCVTRS32F64:
        case Js::OpCode::VPUSH:
        case Js::OpCode::VPOP:
        case Js::OpCode::VADDF64:
        case Js::OpCode::VSUBF64:
        case Js::OpCode::VMULF64:
        case Js::OpCode::VDIVF64:
        case Js::OpCode::VNEGF64:
        case Js::OpCode::VCMPF64:
        case Js::OpCode::VMRS:
        case Js::OpCode::VMRSR:
        case Js::OpCode::VMSR:
            return InstructionType::Vfp;

        default:
            AssertMsg(UNREACHED, "Unexpected opcode in IsInstrThumb2");
            return InstructionType::None;
    }
}

// CanonicalizeMov: Determine the size of the encoding and change the opcode
// if necessary to indicate a wide instruction. (We do this for MOV, LDR, and STR
// to cut down on the time it takes to search all the possible forms.)
InstructionType EncoderMD::CanonicalizeMov(IR::Instr * instr)
{
    // 3 possibilities:
    // 1. MOV (T1):
    // - uint8 to low reg
    // - any reg to reg
    // 2. MOVW (T2):
    // - uint16 to reg
    // 3. MOV_W (T2):
    // - mod const imm to reg

    IR::RegOpnd *dstOpnd = instr->GetDst()->AsRegOpnd();
    IR::Opnd *srcOpnd = instr->GetSrc1();

    if (srcOpnd->IsRegOpnd())
    {
        // All reg to reg copies are 2 bytes.
        return InstructionType::Thumb;
    }

    IntConstType immed = srcOpnd->GetImmediateValue();
    if (IS_LOWREG(dstOpnd->GetReg()) &&
        IS_CONST_UINT8(immed))
    {
        // uint8 -> low reg
        return InstructionType::Thumb;
    }

    // Wide MOV instruction. Choose the opcode based on the constant.
    if (IS_CONST_UINT16(immed))
    {
        instr->m_opcode = Js::OpCode::MOVW;
    }
    else
    {
        Assert(CanEncodeModConst12(immed));
        instr->m_opcode = Js::OpCode::MOV_W;
    }

    return InstructionType::Thumb2;
}

// CanonicalizeLoad: Determine the size of the encoding and change the opcode
// if necessary to indicate a wide instruction. (We do this for MOV, LDR, and STR
// to cut down on the time it takes to search all the possible forms.)
InstructionType EncoderMD::CanonicalizeLoad(IR::Instr * instr)
{
    IR::Opnd *memOpnd = instr->GetSrc1();
    // Note: sign-extension of less-than-4-byte loads requires a wide instruction.
    if (memOpnd->GetSize() == 4 || memOpnd->IsUnsigned())
    {
        if (!this->IsWideMemInstr(instr->GetSrc1(), instr->GetDst()->AsRegOpnd()))
        {
            return InstructionType::Thumb;
        }
    }
    instr->m_opcode = Js::OpCode::LDR_W;
    return InstructionType::Thumb2;
}

// CanonicalizeStore: Determine the size of the encoding and change the opcode
// if necessary to indicate a wide instruction. (We do this for MOV, LDR, and STR
// to cut down on the time it takes to search all the possible forms.)
InstructionType EncoderMD::CanonicalizeStore(IR::Instr * instr)
{
    if (this->IsWideMemInstr(instr->GetDst(), instr->GetSrc1()->AsRegOpnd()))
    {
        instr->m_opcode = Js::OpCode::STR_W;
        return InstructionType::Thumb2;
    }
    return InstructionType::Thumb;
}

// IsWideMemInstr: Shared by LDR and STR.
// Determine the width of the encoding based on the operand properties.
bool EncoderMD::IsWideMemInstr(IR::Opnd *memOpnd, IR::RegOpnd *regOpnd)
{
    // LDR/STR rn, [rbase + rindex], or
    // LDR/STR rn, [rbase + offset]

    // If rn is not low reg, instr is wide.
    if (!IS_LOWREG(regOpnd->GetReg()))
    {
        return true;
    }

    // Pull the base and index/offset from the indirection.
    RegNum baseReg;
    IR::RegOpnd *indexOpnd;
    int32 offset;
    if (memOpnd->IsSymOpnd())
    {
        indexOpnd = nullptr;
        this->BaseAndOffsetFromSym(memOpnd->AsSymOpnd(), &baseReg, &offset, this->m_func);
    }
    else
    {
        IR::IndirOpnd *indirOpnd = memOpnd->AsIndirOpnd();
        // Scaled index operands require wide instruction.
        if (indirOpnd->GetScale() > 0)
        {
            return true;
        }
        baseReg = indirOpnd->GetBaseOpnd()->GetReg();
        indexOpnd = indirOpnd->GetIndexOpnd();
        offset = indirOpnd->GetOffset();
    }

    Assert(offset == 0 || indexOpnd == nullptr);

    if (indexOpnd)
    {
        // Both base and index must be low regs.
        return !IS_LOWREG(baseReg) || !IS_LOWREG(indexOpnd->GetReg());
    }
    else
    {
        size_t size = memOpnd->GetSize();
        if (!IS_LOWREG(baseReg) && (baseReg != RegSP || size != 4))
        {
            // Base reg must be low or SP (and we only have 4-byte SP-relative ops).
            return true;
        }
        // Short encodings shift the offset based on the size of the load/store.
        // (E.g., 4-byte load shifts the offset by 2.)
        if (offset & (size - 1))
        {
            // Can't use a short encoding if we lose bits by shifting the offset.
            return true;
        }
        uint32 shiftBits = Math::Log2(size);
        if (baseReg == RegSP)
        {
            // LDR/STR rn, [SP + uint8:00]
            return !IS_CONST_UINT8(offset >> shiftBits);
        }
        else
        {
            // LDR/STR rn, [base + uint5:size]
            return !IS_CONST_UINT5(offset >> shiftBits);
        }
    }
}

InstructionType EncoderMD::CanonicalizeAdd(IR::Instr * instr)
{
    IR::Opnd *src2 = instr->GetSrc2();
    IntConstType immed = 0;

    // Check cases that apply to ADD but not SUB.
    if (src2->IsRegOpnd())
    {
        // Check for rm = ADD rm, rn
        if (instr->m_opcode != Js::OpCode::ADDS &&
            instr->GetDst()->AsRegOpnd()->IsSameReg(instr->GetSrc1()))
        {
            return InstructionType::Thumb;
        }
    }
    else
    {
        immed = src2->GetImmediateValue();

        // Check for rm = ADD SP, uint8:00
        if (IS_LOWREG(instr->GetDst()->AsRegOpnd()->GetReg()))
        {
            if (instr->GetSrc1()->AsRegOpnd()->GetReg() == RegSP)
            {
                if ((immed & 3) == 0 && IS_CONST_UINT8(immed >> 2))
                {
                    return InstructionType::Thumb;
                }
            }
        }
    }

    // Now check the shared ADD/SUB cases.
    if (this->IsWideAddSub(instr))
    {
        // The instr is definitely wide. Let the opcode indicate that if we're using the uint12 form.
        // Note that the uint12 form can't set the status bits.
        if (!src2->IsRegOpnd() && !this->CanEncodeModConst12(immed))
        {
            Assert(instr->m_opcode != Js::OpCode::ADDS);
            Assert(IS_CONST_UINT12(immed));
            instr->m_opcode = Js::OpCode::ADDW;
        }

        return InstructionType::Thumb2;
    }

    return InstructionType::Thumb;
}

InstructionType EncoderMD::CanonicalizeSub(IR::Instr * instr)
{
    if (this->IsWideAddSub(instr))
    {
        IR::Opnd *src2 = instr->GetSrc2();

        // The instr is definitely wide. Let the opcode indicate that if we're using the uint12 form.
        // Note that the uint12 form can't set the status bits.
        if (!src2->IsRegOpnd() && !this->CanEncodeModConst12(src2->GetImmediateValue()))
        {
            Assert(instr->m_opcode != Js::OpCode::SUBS);
            Assert(IS_CONST_UINT12(src2->GetImmediateValue()));
            instr->m_opcode = Js::OpCode::SUBW;
        }

        return InstructionType::Thumb2;
    }

    return InstructionType::Thumb;
}

bool EncoderMD::IsWideAddSub(IR::Instr * instr)
{
    IR::RegOpnd *dst = instr->GetDst()->AsRegOpnd();
    IR::RegOpnd *src1 = instr->GetSrc1()->AsRegOpnd();
    IR::Opnd *src2 = instr->GetSrc2();
    IntConstType immed;

    if (dst->GetReg() == RegSP)
    {
        // The one short form is SP = op SP, uint7:00
        if (src1->GetReg() != RegSP)
        {
            return true;
        }
        if (src2->IsRegOpnd())
        {
            return true;
        }
        immed = src2->GetImmediateValue();
        return ((immed & 3) != 0) || !IS_CONST_UINT7(immed >> 2);
    }
    else
    {
        // low1 = op low2, low3       or
        // low1 = op low2, uint3      or
        // low1 = op low1, uint8
        if (!IS_LOWREG(dst->GetReg()) || !IS_LOWREG(src1->GetReg()))
        {
            return true;
        }
        if (src2->IsRegOpnd())
        {
            return !IS_LOWREG(src2->AsRegOpnd()->GetReg());
        }
        else
        {
            immed = src2->GetImmediateValue();
            return dst->IsSameReg(src1) ? !IS_CONST_UINT8(immed) : !IS_CONST_UINT3(immed);
        }
    }
}

InstructionType EncoderMD::CanonicalizeLea(IR::Instr * instr)
{
    RegNum baseReg;
    int32 offset;

    IR::Opnd* src1 = instr->UnlinkSrc1();

    if (src1->IsSymOpnd())
    {
        // We may as well turn this LEA into the equivalent ADD instruction and let the common ADD
        // logic handle it.
        IR::SymOpnd *symOpnd = src1->AsSymOpnd();

        this->BaseAndOffsetFromSym(symOpnd, &baseReg, &offset, this->m_func);
        symOpnd->Free(this->m_func);
        instr->SetSrc1(IR::RegOpnd::New(nullptr, baseReg, TyMachReg, this->m_func));
        instr->SetSrc2(IR::IntConstOpnd::New(offset, TyMachReg, this->m_func));
    }
    else
    {
        IR::IndirOpnd *indirOpnd = src1->AsIndirOpnd();
        IR::RegOpnd *baseOpnd = indirOpnd->GetBaseOpnd();
        IR::RegOpnd *indexOpnd = indirOpnd->GetIndexOpnd();
        offset = indirOpnd->GetOffset();

        Assert(offset == 0 || indexOpnd == nullptr);
        instr->SetSrc1(baseOpnd);

        if (indexOpnd)
        {
            AssertMsg(indirOpnd->GetScale() == 0, "NYI Needs shifted register support for ADD");
            instr->SetSrc2(indexOpnd);
        }
        else
        {
            instr->SetSrc2(IR::IntConstOpnd::New(offset, TyMachReg, this->m_func));
        }
        indirOpnd->Free(this->m_func);
    }
    instr->m_opcode = Js::OpCode::ADD;
    return this->CanonicalizeAdd(instr);
}

InstructionType EncoderMD::CmpEncodeType(IR::Instr * instr)
{
    // CMP:
    // - low reg, uint8
    // - any reg, any reg
    IR::Opnd *src2 = instr->GetSrc2();
    if (src2->IsRegOpnd())
    {
        Assert(instr->GetSrc1()->IsRegOpnd());
        return InstructionType::Thumb;
    }

    if (IS_LOWREG(instr->GetSrc1()->AsRegOpnd()->GetReg()) &&
        IS_CONST_UINT8(src2->GetImmediateValue()))
    {
        return InstructionType::Thumb;
    }

    return InstructionType::Thumb2;
}

InstructionType EncoderMD::CmnEncodeType(IR::Instr * instr)
{
    // CMN:
    // - low reg, low reg
    // - any reg, uint8
    // - any reg, any reg
    IR::Opnd *src2 = instr->GetSrc2();

    if (src2->IsRegOpnd())
    {
        // low reg, low reg
        if (IS_LOWREG(instr->GetSrc1()->AsRegOpnd()->GetReg()) && IS_LOWREG(instr->GetSrc2()->AsRegOpnd()->GetReg()))
        {
            return InstructionType::Thumb;
        }
    }

    // any reg, uint8
    // any reg, any reg
    return InstructionType::Thumb2;
}


InstructionType EncoderMD::PushPopEncodeType(IR::IndirOpnd *target, IR::RegBVOpnd * opnd)
{
    if(target->GetBaseOpnd()->GetReg() != RegSP)
    {
        return InstructionType::Thumb2;
    }

    // NOTE: because T1 encoding permits LR here, we could theoretically check for it specially,
    // but in practice we never push LR without R11, so it would never help. If that changes, we
    // should make this function smarter.

    BYTE lastRegEncode = (BYTE)opnd->m_value.GetPrevBit();
    Assert(lastRegEncode != BVInvalidIndex);
    return lastRegEncode > RegEncode[RegR7] ? InstructionType::Thumb2 : InstructionType::Thumb;
}

InstructionType EncoderMD::Alu2EncodeType(IR::Opnd *opnd1, IR::Opnd *opnd2)
{
    // Shared by TST (checks src1 and src2) and MVN (checks dst and src1), which is why we pass
    // operands rather than the whole instruction.
    // Short encoding requires two low regs as operands.
    if (!opnd1->IsRegOpnd() || !IS_LOWREG(opnd1->AsRegOpnd()->GetReg()))
    {
        return InstructionType::Thumb2;
    }
    if (!opnd2->IsRegOpnd() || !IS_LOWREG(opnd2->AsRegOpnd()->GetReg()))
    {
        return InstructionType::Thumb2;
    }
    return InstructionType::Thumb;
}

InstructionType EncoderMD::Alu3EncodeType(IR::Instr * instr)
{
    // Check for rm = op rm, rn

    IR::RegOpnd *dst = instr->GetDst()->AsRegOpnd();
    if (!IS_LOWREG(dst->GetReg()) ||
        !dst->IsSameReg(instr->GetSrc1()))
    {
        return InstructionType::Thumb2;
    }

    IR::Opnd *src2 = instr->GetSrc2();
    if (!src2->IsRegOpnd() || !IS_LOWREG(src2->AsRegOpnd()->GetReg()))
    {
        return InstructionType::Thumb2;
    }

    return InstructionType::Thumb;
}

InstructionType EncoderMD::ShiftEncodeType(IR::Instr * instr)
{
    // 2 short forms:
    // rm = op rn, uint5
    // rm = op rm, rn

    IR::RegOpnd *dst = instr->GetDst()->AsRegOpnd();
    if (!IS_LOWREG(dst->GetReg()))
    {
        return InstructionType::Thumb2;
    }

    IR::RegOpnd *src1 = instr->GetSrc1()->AsRegOpnd();
    IR::Opnd *src2 = instr->GetSrc2();
    if (src2->IsRegOpnd())
    {
        return (IS_LOWREG(src2->AsRegOpnd()->GetReg()) && dst->IsSameReg(src1)) ? InstructionType::Thumb : InstructionType::Thumb2;
    }
    else
    {
        Assert(IS_CONST_UINT5(src2->GetImmediateValue()));
        return IS_LOWREG(src1->GetReg()) ? InstructionType::Thumb : InstructionType::Thumb2;
    }
}

int
EncoderMD::IndirForm(int form, int *pOpnnum, RegNum baseReg, IR::Opnd *indexOpnd)
{
    int opnnum = *pOpnnum;

    form |= FSRC(INDIR, opnnum++);

    switch (baseReg)
    {
    case RegSP:
        form |= FSRC(SP, opnnum++);
        break ;
    case RegPC:
        form |= FSRC(PC, opnnum++);
        break;
    default:
        form |= FSRC(REG, opnnum++);
        break;
    }

    if (indexOpnd == nullptr)
    {
        // UTC does this for OPBASED. Seems to be based on the assumption
        // that we have either an offset or an index, but not both.
        form |= FSRC(CONST, opnnum++);          // OFFSET
    }
    else
    {
        form |= FSRC(REG, opnnum++);            // INDEX
    }

    *pOpnnum = opnnum;
    return form;
}

//---------------------------------------------------------------------------
//
// CoGenIForms()
//
// parses the instruction tuple and generates the corresponding 'form' constant
//
//---------------------------------------------------------------------------

int
EncoderMD::GetForm(IR::Instr *instr, int32 size)
{
    int form;
    int opnnum;                 //Current looping operand in the instruction
    int operands;               //Represents if the current operand is dst or source
    RegNum regNum;
    IR::Opnd* dst;
    IR::Opnd* opn;
    IR::IndirOpnd *indirOpnd;
    bool sameSrcDst = false;
    bool T2instr = false;

    form = 0;

    T2instr = (size == 4);

    // Set THUMB or THUMB2 instruction, this is to figure out if the form is T2 or T1.
    if (T2instr)
    {
        form |= FTHUMB2;
    }
    else
    {
        sameSrcDst = true;
        form |= FTHUMB;
    }

    dst  = instr->GetDst();

    if (dst == nullptr || LowererMD::IsCall(instr))
    {
        opn = instr->GetSrc1();
        opnnum = 1;
        operands = 1;
        if (instr->IsBranchInstr() && instr->AsBranchInstr()->GetTarget())
        {
            // Treat the label reference as the first source.
            form |= FSRC(LABEL, opnnum++);
        }
    }
    else
    {
        opn = dst;
        opnnum = 0;
        operands = 0;
    }

    bool done = false;

    while (opn != nullptr)
    {
        switch (opn->GetKind())
        {
            case IR::OpndKindIntConst:
            case IR::OpndKindFloatConst:
            case IR::OpndKindAddr: //UTC - CASE_DATAADDRTUPLE
                {
                    form |= FSRC(CONST, opnnum++);
                }
                break;

            case IR::OpndKindReg:
                {
                    regNum = opn->AsRegOpnd()->GetReg();
                    switch (regNum)
                    {
                    case RegSP:
                    case RegPC:
                        if (size != 4 || instr->m_opcode == Js::OpCode::LDRRET)
                        {
                            if (regNum == RegSP)
                            {
                                form |= FSRC(SP, opnnum++);
                            }
                            else
                            {
                                form |= FSRC(PC, opnnum++);
                            }
                            break;
                        }

                        // FALL THROUGH!
                    default:
                        if (regNum >= RegR0 && regNum <= RegPC)
                        {
                            if ((regNum > RegR7) && (!T2instr))
                            {
                                form |= FSET(REG,28);
                            }

                            if (operands == 0)
                            { // dst operands
                                form |= FSRC(REG,opnnum++);
                            }
                            else
                            { // src operands
                                if (sameSrcDst && dst && opn->AsRegOpnd()->IsSameReg(dst))
                                {
                                    form |= FSRC(REG,0);   // same src,dst
                                    sameSrcDst = false;
                                }
                                else
                                {
                                    form |= FSRC(REG, opnnum++);
                                }
                            }
                        }
                        else if (regNum >= RegR0 && regNum <= LAST_DOUBLE_REG)
                        {
                            form |= FSRC(DREG, opnnum++);
                        }
                        break;
                    }
                }
                break;

            case IR::OpndKindHelperCall:
            {
                form |= FSRC(CODE, opnnum++);
            }
            break;

            case IR::OpndKindRegBV:
                {
                    Assert(instr->m_opcode == Js::OpCode::PUSH || instr->m_opcode == Js::OpCode::POP
                            || instr->m_opcode == Js::OpCode::VPUSH || instr->m_opcode == Js::OpCode::VPOP);
                    BVIndex count = opn->AsRegBVOpnd()->GetValue().Count();
                    Assert(count > 0);
                    // Note: only the wide encoding distinguishes between single- and multiple-register push/pop.
                    if (count == 1 && T2instr)
                    {
                        form |= FSRC(REG, opnnum++);
                    }
                    break;
                }

            case IR::OpndKindIndir:
                indirOpnd = opn->AsIndirOpnd();
                form = this->IndirForm(form, &opnnum, indirOpnd->GetBaseOpnd()->GetReg(), indirOpnd->GetIndexOpnd());
                break;

            case IR::OpndKindSym:
            {
                RegNum baseReg;
                int32 offset;
                AssertMsg(opn->AsSymOpnd()->m_sym->IsStackSym(), "Should only see stackSym syms in encoder.");
                form |= FSRC(INDIR, opnnum++);
                this->BaseAndOffsetFromSym(opn->AsSymOpnd(), &baseReg, &offset, this->m_func);
                if (baseReg == RegSP)
                {
                    form |= FSRC(SP, opnnum++);
                }
                else
                {
                    form |= FSRC(REG, opnnum++);
                    form |= FSRC(CONST, opnnum++);
                }
                break;
            }

            case IR::OpndKindLabel:
                form |= FSRC(LABEL, opnnum++);
                break;

            case IR::OpndKindMemRef:
                // Deref of literal address
                AssertMsg(0, "NYI");
                return 0;

            default:
               AssertMsg(UNREACHED, "Unrecognized kind");
               return 0;
        }

        if (done)
        {
            //If we have traversed all the 3 operands exit.
            break;
        }

        if (LowererMD::IsCall(instr))
        {
            break;
        }

        if (opn == dst)
        {
            opn = instr->GetSrc1();
            if (instr->IsBranchInstr() && instr->AsBranchInstr()->GetTarget())
            {
                // Treat the label reference as the first source.
                form |= FSRC(LABEL, opnnum++);
            }
        }
        else
        {
            opn = instr->GetSrc2();
            done = true;
        }
        operands = 1;
    }

    return (form);
}

bool EncoderMD::EncodeImmediate16(long constant, DWORD * result)
{
    if (constant > 0xFFFF)
    {
        return FALSE;
    }

    DWORD encode = ((constant & 0xFF) << 16) |
        ((constant & 0x0700) << 20) |
        ((constant & 0x0800) >> 1) |
        ((constant & 0xF000) >> 12);

    *result |= encode;
    return TRUE;
}

ENCODE_32
EncoderMD::EncodeT2Immediate12(ENCODE_32 encode, long constant)
{
    Assert((constant & 0xFFFFF000) == 0);

    ENCODE_32 encoded = (constant & 0x800) >> (11-10);
    encoded |= (constant & 0x700) << (16+12-8);
    encoded |= (constant & 0xFF) << 16;
    encode |= encoded;

    return encode;
}

ENCODE_32
EncoderMD::EncodeT2Offset(ENCODE_32 encode, IR::Instr *instr, int offset, int bitOffset)
{
    if (EncoderMD::IsShifterUpdate(instr))
    {
        Assert(IS_CONST_INT8(offset));
        encode |= 9 << 24;
        if (!EncoderMD::IsShifterSub(instr))
        {
            encode |= 1 << 25;
        }
        if (!EncoderMD::IsShifterPost(instr))
        {
            encode |= 1 << 26;
        }
    }
    else
    {
        if (offset >=0)
        {
            Assert(IS_CONST_UINT12(offset));
            encode |= 1 << 7;
        }
        else
        {
            offset = -offset;
            Assert(IS_CONST_UINT8(offset));
            encode |= 0x0C000000;
        }
    }
    encode |= offset << bitOffset;

    return encode;
}

//---------------------------------------------------------------------------
//
// GenerateEncoding()
//
// generates the encoding for the specified tuple/form by applying the
// associated encoding steps
//
//---------------------------------------------------------------------------
ENCODE_32
EncoderMD::GenerateEncoding(IR::Instr* instr, IFORM iform, BYTE *pc, int32 size, InstructionType instrType)
{
    ENCODE_32 encode = 0 ;
    DWORD encoded = 0;

    IR::Opnd* opn = 0;
    IR::Opnd* dst = 0;
    IR::Opnd* reg = 0; //tupReg;
    IR::IndirOpnd *indirOpnd;

    Js::OpCode  opcode = instr->m_opcode;
    const AssemblyStep *AsmSteps = nullptr;
    const FormTable *ftp = nullptr;

    int bitOffset;
    int offset;

    bool fUpdate;
    bool fSub;
    bool fPost;

    int done = false;
    long constant = 0; //UTC IVALTYPE
    bool constantValid = false;
    RegNum regNum;
    unsigned int iType = 0, SFlag = 0;

    dst  = instr->GetDst();

    if(opcode == Js::OpCode::MLS)
    {
        Assert(instr->m_prev->GetDst()->IsRegOpnd() && (instr->m_prev->GetDst()->AsRegOpnd()->GetReg() == RegR12));
    }

    if (dst == nullptr || LowererMD::IsCall(instr))
    {
        opn = instr->GetSrc1();
        reg = opn;
    }
    else if (opcode == Js::OpCode::POP || opcode == Js::OpCode::VPOP)
    {
        opn = instr->GetSrc1();
        reg = dst;
    }
    else
    {
        opn = dst;
        reg = opn;
    }

    for (ftp = InstrEncode[opcode - (Js::OpCode::MDStart + 1)]; !done && ftp->form != FORM_NOMORE; ftp++)
    {
        if (ftp->form != iform)
        {
            if (!((iform & (1<<28)) == 0 && THUMB2_THUMB1_FORM(ftp->form, iform)))
            {
                continue;
            }
        }

        AsmSteps = ftp->steps;
        done = false;
        constantValid=0;

        while (!done)
        {
            switch (*AsmSteps++)
            {
                case STEP_NEXTOPN:
                    // Get Next operand
                   if (opn == dst)
                   {
                       opn = instr->GetSrc1();
                   }
                   else
                   {
                       Assert(opn == instr->GetSrc1());
                       opn = instr->GetSrc2();
                   }

                   reg = opn;
                   continue;

                case STEP_CONSTANT:
                    Assert(opn->IsImmediateOpnd());
                    constant = opn->GetImmediateValue();
                    constantValid = true;
                    continue;

                case STEP_CALL:
                    continue;

                case STEP_T2_BRANCH24:
                    // Constant encoded with 24bits
                    EncodeReloc::New(&m_relocList, RelocTypeBranch24, m_pc, instr->AsBranchInstr()->GetTarget(), m_encoder->m_tempAlloc);
                    continue;

                case STEP_T2_BRANCH20:
                    // Constant encoded with 20bits.
                    EncodeReloc::New(&m_relocList, RelocTypeBranch20, m_pc, instr->AsBranchInstr()->GetTarget(), m_encoder->m_tempAlloc);
                    continue;

                case STEP_REG:
                    Assert(reg != nullptr);
                    Assert(reg->IsRegOpnd());

                    bitOffset = *AsmSteps++;
                    regNum = (RegNum)this->GetRegEncode(reg->AsRegOpnd());
                    encode |= regNum << bitOffset;
                    continue;

                case STEP_HREG:
                    Assert(reg != nullptr);
                    Assert(reg->IsRegOpnd());
                    bitOffset = *AsmSteps++;
                    regNum = (RegNum)this->GetRegEncode(reg->AsRegOpnd());
                    encode |= (regNum & 0x7) << bitOffset;
                    continue;

                case STEP_R12:
                    bitOffset = *AsmSteps++;
                    regNum = (RegNum)this->GetRegEncode(RegR12);
                    encode |= regNum << bitOffset;
                    continue;

                case STEP_HBIT:
                    Assert(reg != nullptr);
                    Assert(reg->IsRegOpnd());
                    regNum = (RegNum)this->GetRegEncode(reg->AsRegOpnd());
                    if (regNum >= MAX_INT_REGISTERS_LOW)
                    {
                        bitOffset = *AsmSteps;
                        encode |= 1 << bitOffset;
                    }
                    AsmSteps++;
                    continue;

                case STEP_OPEQ:
                    Assert(instr->GetDst()->AsRegOpnd()->IsSameReg(instr->GetSrc1()));
                    continue;

                case STEP_DUMMY_REG:
                    Assert(opn->AsRegOpnd()->GetReg() == RegSP ||
                           opn->AsRegOpnd()->GetReg() == RegPC);
                    continue;

                case STEP_OPCODE:
                    //ASSERTTNR(!(instr & ftp->inst), tupInstr);
                    encode |= ftp->inst;
                    continue;

                case STEP_FIXUP:
                    /*
                    if (TU_ISINDIR(tupOpn)) {
                        if (fApplyFixup)
                            CoApplyFixup(tupOpn, dataBuf);
                            }*/
                    continue;

                case STEP_LDR:
                    Assert(!constantValid);
                    switch (opn->GetType())
                    {
                    case TyInt8:
                        constant = 0x5600;
                    case TyInt16:
                        constant = 0x5e00;
                        break;
                    case TyInt32:
                    case TyUint32:
                    case TyVar:
                        constant = 0x5800;
                        break;

                    case TyUint8:
                        constant = 0x5c00;
                        break;

                    case TyUint16:
                        constant = 0x5a00;
                        break;
                    }
                    encode |= constant;
                    continue;

                case STEP_LDRI:
                    Assert(!constantValid);
                    switch (opn->GetType())
                    {
                    case TyInt8:
                    case TyInt16:
                        constant = 0;
                        break;

                    case TyInt32:
                    case TyUint32:
                    case TyVar:
                        constant = 0x6800;
                        break;

                    case TyUint8:
                        constant = 0x7800;
                        break;

                    case TyUint16:
                        constant = 0x8800;
                        break;
                    }
                    encode |= constant;
                    continue;

                case STEP_STRI:
                    Assert(!constantValid);
                    switch (opn->GetType())
                    {
                    case TyInt8:
                    case TyUint8:
                        constant = 0x7000;
                        break;

                    case TyInt16:
                    case TyUint16:
                        constant = 0x8000;
                        break;

                    case TyInt32:
                    case TyUint32:
                    case TyVar:
                        constant = 0x6000;
                        break;
                    }
                    encode |= constant;
                    continue;

                case STEP_STR:
                    Assert(!constantValid);
                    switch (opn->GetType())
                    {
                    case TyInt8:
                    case TyUint8:
                        constant = 0x5400;
                        break;

                    case TyInt16:
                    case TyUint16:
                        constant = 0x5200;
                        break;

                    case TyInt32:
                    case TyUint32:
                    case TyVar:
                        constant = 0x5000;
                        break;
                    }
                    encode |= constant;
                    continue;

                case STEP_IMM:
                    bitOffset = *AsmSteps++;
                    if (opn->IsIndirOpnd())
                    {
                        offset = opn->AsIndirOpnd()->GetOffset();
                    }
                    else
                    {
                        this->BaseAndOffsetFromSym(opn->AsSymOpnd(), &regNum, &offset, this->m_func);
                    }

                    switch (opn->GetSize())
                    {
                        case 1:
                            break;

                        case 2:
                            Assert(!(offset & 0x1));
                            offset = offset >> 1;
                            break;

                        case 4:
                            Assert(!(offset & 0x3)); //check for word-align
                            offset = offset >> 2;
                            break;

                        default:
                            Assert(UNREACHED);
                            offset = 0;
                    }
                    Assert(IS_CONST_UINT5(offset));
                    encode |= offset << bitOffset;
                    continue;

                case STEP_UIMM3:
                    bitOffset = *AsmSteps++;
                    Assert(constantValid);
                    Assert(IS_CONST_UINT3(constant));
                    encode |= constant << bitOffset;
                    continue;

                case STEP_IMM_W7:
                    Assert(constantValid);
                    Assert(!(constant & 0x3)); // check for word-alignment
                    constant = constant >> 2;  // remove rightmost two zero bits
                    Assert(IS_CONST_UINT7(constant));
                    encode |= constant;
                    constantValid = false;
                    continue;

                case STEP_IMM_DPW8:
                    Assert(constantValid);
                    Assert(IS_CONST_UINT8(constant >> 2));
                    Assert(constant % 4 == 0);
                    encode |= constant >> 2;
                    constantValid = false;
                    continue;

                case STEP_IMM_W8:
                    if (opn->IsSymOpnd())
                    {
                        this->BaseAndOffsetFromSym(opn->AsSymOpnd(), &regNum, &offset, this->m_func);
                    }
                    else
                    {
                        offset = opn->AsIndirOpnd()->GetOffset();
                    }

                    Assert(offset % 4 == 0);
                    Assert(IS_CONST_UINT8(offset >> 2));
                    encode |= offset >> 2;
                    continue;

                case STEP_T2_IMM_16:
                    if (!EncodeImmediate16(constant, &encoded))
                    {
                        AssertMsg(false,"constant > than 16 bits");
                    }
                    encode |= encoded;
                    continue;

                case STEP_T2_IMM_12:
                    encode = this->EncodeT2Immediate12(encode, constant);
                    continue;

                case STEP_OFFSET:
                {
                    unsigned int R_bit = 0;

                    if (ISSTORE(instr->m_opcode))
                    {
                        if (TESTREGBIT(constant, RegLR))
                        {
                            R_bit = 1 << 8;
                        }
                        CLEARREGBIT(constant, RegLR);
                    }
                    else
                    {
                        if (TESTREGBIT(constant, RegPC))
                        {
                            R_bit = 1 << 8;
                        }
                        CLEARREGBIT(constant, RegPC);
                    }

                    Assert(IS_CONST_UINT8(constant));

                    encode |= (CO_UIMMED8(constant) | R_bit);
                    constantValid=false;
                    continue;
                }

                case STEP_SCALE_CONST:
                {
                    bitOffset = *AsmSteps++;
                    byte scale = opn->AsIndirOpnd()->GetScale();
                    Assert(IS_CONST_UINT5(scale));
                    encode |= scale << bitOffset;
                    continue;
                }

                case STEP_SHIFTER_CONST:
                    bitOffset = *AsmSteps++;

                    // TODO: When we have IR that can send Shifts we will
                    // need to translate the following:
                    // As of now instructions do not have a mechanism to
                    // provide the shift offset.

                    //ASSERTNR(IS_CONST_UINT5(TU_SHIFTER_SHIFT(tupOpn)));
                    //instr |= TU_SHIFTER_SHIFT(tupOpn) << to;
                    continue;

                case STEP_BASEREG:
                    bitOffset = *AsmSteps++;
                    if (opn->IsIndirOpnd())
                    {
                        regNum = opn->AsIndirOpnd()->GetBaseOpnd()->GetReg();
                    }
                    else
                    {
                        this->BaseAndOffsetFromSym(opn->AsSymOpnd(), &regNum, &offset, this->m_func);
                    }
                    encode |= RegEncode[regNum] << bitOffset;
                    continue;

                case STEP_INDEXED:
                    Assert(opn->IsIndirOpnd());
                    Assert(opn->AsIndirOpnd()->GetIndexOpnd() != nullptr);
                    Assert(opn->AsIndirOpnd()->GetOffset() == 0);
                    continue;

                case STEP_INDEXREG:
                    bitOffset = *AsmSteps++;
                    reg = opn->AsIndirOpnd()->GetIndexOpnd();
                    Assert(reg != nullptr);
                    Assert(reg->IsRegOpnd());
                    regNum = (RegNum)this->GetRegEncode(reg->AsRegOpnd());
                    encode |= regNum << bitOffset;
                    continue;

                case STEP_INDIR:
                    Assert(opn->IsIndirOpnd() ||
                        (opn->IsSymOpnd() && opn->AsSymOpnd()->m_sym->IsStackSym()));
                    continue;

                case STEP_BASED:
                    Assert((opn->IsIndirOpnd() && opn->AsIndirOpnd()->GetIndexOpnd() == nullptr) ||
                           (opn->IsSymOpnd() && opn->AsSymOpnd()->m_sym->IsStackSym()));
                    continue;

                case STEP_T2_REGLIST:
                    //ASSERTTNR(constant_valid, tupOpn);
                    encode |= constant << 16;
                    constantValid = false;
                    if (EncoderMD::IsShifterUpdate(instr))
                    {
                        encode |= 0x20;
                    }
                    continue;

                case STEP_UIMM5:
                    bitOffset = *AsmSteps++;
                    Assert(constantValid);
                    Assert(IS_CONST_UINT5(constant));
                    encode |= constant << bitOffset;
                    constantValid = false;
                    continue;

                case STEP_UIMM8:
                    Assert(constantValid);
                    Assert(IS_CONST_UINT8(constant));
                    encode |= constant;
                    constantValid = false;
                    continue;

                case STEP_REGLIST:
                    {
                        indirOpnd = opn->AsIndirOpnd();
                        Assert(indirOpnd->GetIndexOpnd() == nullptr);
                        constant = indirOpnd->GetOffset();

                        IR::Opnd *opndRD;
                        if (EncoderMD::IsLoad(instr))
                        {
                            opndRD = instr->GetDst();
                        }
                        else
                        {
                            opndRD = instr->GetSrc1();
                        }

                        if (!constant)
                        {
                            BVUnit32 registers = opndRD->AsRegBVOpnd()->GetValue();
                            unsigned long regenc;
                            BVIndex index = registers.GetNextBit();

                            // Note: only the wide encoding distinguishes between
                            // single- and multiple-register push/pop.
                            if (registers.Count() > 1 || size == 2)
                            {
                                // Add the physical register number
                                do
                                {
                                    regenc = 1 << index;
                                    constant |= regenc;
                                }while ((index = registers.GetNextBit(index + 1))!= BVInvalidIndex);

                            }
                            else
                            {
                                bitOffset = *AsmSteps++;
                                Assert(index < RegEncode[RegSP]);
                                encode |= index << bitOffset;
                                continue;
                            }
                        }

                        if (size == 4)
                        {
                            fSub = EncoderMD::IsShifterSub(instr);
                            fUpdate = EncoderMD::IsShifterUpdate(instr);
                            encode |= fSub << 8;
                            encode |= !fSub << 7;
                            encode |= fUpdate << 5;
                        }

                        constantValid=true;
                    }
                    continue;

                case STEP_T1_SETS_CR0:
                    {
                        //ASSERTTNR(Tuple::FindReg(TU_DST(tupInstr), RG_SYM(CR0)) != nullptr, tupInstr);
                    }
                    continue;

                case STEP_SBIT:
                    {
                        if (this->SetsSBit(instr))
                        {
                            bitOffset = *AsmSteps;
                            encode |= 1 << bitOffset;
                        }
                        AsmSteps++;
                    }
                    continue;

                case STEP_NOSBIT:
                    // just asserts that we're not supposed to set the condition flags
                    //
                    Assert(!this->SetsSBit(instr));
                    continue;

                case STEP_MODCONST_12:
                    if (!EncodeModConst12(constant, &encoded))
                    {
                        Assert(UNREACHED);
                    }
                    encode |= encoded;
                    continue;

                case STEP_T2_MEMIMM_POS12_NEG8:
                    bitOffset = *AsmSteps++;
                    Assert(opn != nullptr);
                    if (opn->IsIndirOpnd())
                    {
                        Assert(opn->AsIndirOpnd()->GetIndexOpnd() == nullptr);
                        offset = opn->AsIndirOpnd()->GetOffset();
                        // TODO: Handle literal pool loads, if necessary
                        // <tfs #775202>: LDR_W could have $Label Fixup for literal-pool
                        //if (TU_FEFIXUPSYM(tupOpn) && SS_ISLABEL(TU_FEFIXUPSYM(tupOpn))) {
                        //    offset += (SS_OFFSET(TU_FEFIXUPSYM(tupOpn)) - ((pc & (~3)) + 4));
                        //}
                    }
                    else if (opn->IsSymOpnd())
                    {
                        Assert(opn->AsSymOpnd()->m_sym->IsStackSym());
                        this->BaseAndOffsetFromSym(opn->AsSymOpnd(), &regNum, &offset, this->m_func);
                    }
                    else
                    {
                        Assert(opn->IsImmediateOpnd());
                        offset = opn->GetImmediateValue();
                    }
                    encode = this->EncodeT2Offset(encode, instr, offset, bitOffset);
                    continue;

                case STEP_T2_IMMSTACK_POS12_NEG8:
                    bitOffset = *AsmSteps++;
                    Assert(opn != nullptr);
                    Assert(opn->IsSymOpnd() && opn->AsSymOpnd()->m_sym->IsStackSym());
                    this->BaseAndOffsetFromSym(opn->AsSymOpnd(), &regNum, &offset, this->m_func);

                    encode = this->EncodeT2Offset(encode, instr, offset, bitOffset);
                    encode |= RegEncode[regNum];
                    continue;

                case STEP_T2_STACKSYM_IMM_12:
                    // Used by LEA. Encode base reg at the given bit offset and 12-bit constant
                    // as a normal ADDW immediate.
                    bitOffset = *AsmSteps++;
                    Assert(opn != nullptr);
                    Assert(opn->IsSymOpnd() && opn->AsSymOpnd()->m_sym->IsStackSym());
                    this->BaseAndOffsetFromSym(opn->AsSymOpnd(), &regNum, &offset, this->m_func);

                    encode |= RegEncode[regNum] << bitOffset;
                    encode = this->EncodeT2Immediate12(encode, offset);
                    continue;

                case STEP_T2_MEM_TYPE:
                {
                    Assert((ftp->inst & 0xFF00) == 0xF800);
                    switch (opn->GetType())
                    {
                    case TyInt8:
                        SFlag = 1;
                        iType = 0;
                        break;
                    case TyUint8:
                        SFlag = 0;
                        iType = 0;
                        break;
                    case TyInt16:
                        SFlag = 1;
                        iType = 1;
                        break;
                    case TyUint16:
                        iType = 1;
                        SFlag = 0;
                        break;
                    case TyInt32:
                    case TyUint32:
                    case TyVar:
                        SFlag = 0;
                        iType = 2;
                        break;
                    default:
                        Assert(UNREACHED);
                    }
                    if (!EncoderMD::IsLoad(instr))
                    {
                        SFlag = 0;
                    }
                    encode |= (SFlag << 8) | (iType << 5);
                    continue;
                }

                case STEP_T2_SHIFT_IMM_5:
#if DBG
                    if(instr->m_opcode == Js::OpCode::ASR ||
                       instr->m_opcode == Js::OpCode::ASRS ||
                       instr->m_opcode == Js::OpCode::LSR)
                    {
                        // Encoding zero is interpreted as 32
                        // for these instructions.
                        Assert(constant != 0);
                    }
#endif
                    Assert(IS_CONST_UINT5(constant));
                    encoded = (constant & 0x03) << (16+6);
                    encoded |= (constant & 0x1c) << (16+12-2);
                    encode |= encoded;
                    continue;

                case STEP_MOVW_reloc:
                    Assert(opn && opn->IsLabelOpnd());
                    if (opn->AsLabelOpnd()->GetLabel()->m_isDataLabel)
                    {
                        Assert(!opn->AsLabelOpnd()->GetLabel()->isInlineeEntryInstr);
                        EncodeReloc::New(&m_relocList, RelocTypeDataLabelLow, m_pc, opn->AsLabelOpnd()->GetLabel(), m_encoder->m_tempAlloc);
                    }
                    else
                    {
                        EncodeReloc::New(&m_relocList, RelocTypeLabelLow, m_pc, opn->AsLabelOpnd()->GetLabel(), m_encoder->m_tempAlloc);
                    }
                    continue;

                case STEP_MOVT_reloc:
                    Assert(opn && opn->IsLabelOpnd());
                    EncodeReloc::New(&m_relocList, RelocTypeLabelHigh, m_pc, opn->AsLabelOpnd()->GetLabel(), m_encoder->m_tempAlloc);
                    continue;

                case STEP_DREG:
                {
                    int bbit = 0;
                    DWORD tmp = 0;

                    Assert(opn != nullptr && opn->IsRegOpnd());

                    bitOffset = *AsmSteps++;
                    bbit = *AsmSteps++;

                    regNum = (RegNum)GetRegEncode(opn->AsRegOpnd());
                    //Check to see if register number is valid
                    Assert(regNum >= 0 && regNum <= LAST_DOUBLE_REG_NUM);

                    tmp |= (regNum & 0xf) << bitOffset;
                    tmp |= ((regNum >> 4) & 0x1) << bbit;
                    Assert(0 == (encode & tmp));

                    encode |= tmp;
                    continue;
                }

                case STEP_SREG:
                {
                    int bbit = 0;
                    DWORD tmp = 0;

                    Assert(opn != nullptr && opn->IsRegOpnd());

                    bitOffset = *AsmSteps++;
                    bbit = *AsmSteps++;

                    regNum = (RegNum)GetFloatRegEncode(opn->AsRegOpnd());
                    //Check to see if register number is valid
                    Assert(regNum >= 0 && regNum <= LAST_FLOAT_REG_NUM);

                    tmp |= (regNum & 0x1) << bbit;
                    tmp |= (regNum >> 1) << bitOffset;

                    Assert(0 == (encode & tmp));

                    encode |= tmp;
                    continue;
                }

                case STEP_IMM_S8:
                {
                    Assert(opn!=nullptr);
                    AssertMsg(instrType == InstructionType::Vfp, "This step is specific to VFP instructions");
                    if (opn->IsIndirOpnd())
                    {
                        Assert(opn->AsIndirOpnd()->GetIndexOpnd() == nullptr);
                        offset = opn->AsIndirOpnd()->GetOffset();
                        // TODO: Handle literal pool loads, if necessary
                        // <tfs #775202>: LDR_W could have $Label Fixup for literal-pool
                        //if (TU_FEFIXUPSYM(tupOpn) && SS_ISLABEL(TU_FEFIXUPSYM(tupOpn))) {
                        //    offset += (SS_OFFSET(TU_FEFIXUPSYM(tupOpn)) - ((pc & (~3)) + 4));
                        //}
                    }
                    else if (opn->IsSymOpnd())
                    {
                        Assert(opn->AsSymOpnd()->m_sym->IsStackSym());
                        this->BaseAndOffsetFromSym(opn->AsSymOpnd(), &regNum, &offset, this->m_func);
                    }
                    else
                    {
                        offset  = 0;
                        AssertMsg(false, "Why are we here");
                    }

                    if (offset < 0)
                    {
                        //IsShifterSub(tupOpn) = TRUE; //Doesn't seem necessary for us, why does UTC set this?
                        offset = -offset;
                        encode &= ~(1 << 7);
                    }
                    else
                    {
                        encode |= (1 << 7);
                    }

                    // Set the W (writeback) bit if IsShifterUpdate is
                    // specified.
                    if (EncoderMD::IsShifterUpdate(instr))
                    {
                        encode |= (1 << 5);

                        // Set the P (pre-indexed) bit to be the complement
                        // of IsShifterPost
                        if (EncoderMD::IsShifterPost(instr))
                        {
                            encode &= ~(1 << 8);
                        }
                        else
                        {
                            encode |= (1 << 8);
                        }
                    }
                    else
                    {
                        // Clear the W bit and set the P bit (offset
                        // addressing).
                        encode &= ~(1 << 5);
                        encode |= (1 << 8);
                    }

                    Assert(IS_CONST_UINT8(offset >> 2));
                    encode |= ((offset >> 2) << 16);
                    continue;
                }

                case STEP_DREGLIST:
                    {
                        IR::Opnd *opndRD;
                        if (EncoderMD::IsLoad(instr))
                        {
                            opndRD = instr->GetDst();
                        }
                        else
                        {
                            opndRD = instr->GetSrc1();
                        }

                        BVUnit32 registers = opndRD->AsRegBVOpnd()->GetValue();
                        DWORD first = registers.GetNextBit();
                        DWORD last = (DWORD)-1;
                        _BitScanReverse((DWORD*)&last, (DWORD)registers.GetWord());
                        Assert(last >= first && last <= LAST_DOUBLE_CALLEE_SAVED_REG_NUM);

                        encode |= (CO_UIMMED8((last - first + 1) * 2)) << 16;
                        encode |= (first << 28);
                    }
                    continue;


                case STEP_AM5:
                    Assert(opn->IsIndirOpnd());
                    Assert(opn->AsIndirOpnd()->GetIndexOpnd() == nullptr);
                    Assert(opn->AsIndirOpnd()->GetOffset() == 0);

                    fPost = EncoderMD::IsShifterPost(instr);
                    fSub = EncoderMD::IsShifterSub(instr);
                    fUpdate = EncoderMD::IsShifterUpdate(instr);
                    // update addressing mode
                    encode |= !fPost << 8;
                    encode |= !fSub << 7;
                    encode |= fUpdate << 5;
                    continue;


                case STEP_DONE:
                    done = true;
                    break;

                default:
#if DBG
                    instr->Dump();
                    AssertMsg(UNREACHED, "Unrecognized assembly step");
#endif
                    return 0;
            }

            break;
        }
    }

#if DBG
    if (!done)
    {
        instr->Dump();
        Output::Flush();
        AssertMsg(UNREACHED, "Unsupported Instruction Form");
    }
#endif
    return encode;
}

#ifdef INSERT_NOPS
ptrdiff_t insertNops(BYTE *pc, ENCODE_32 outInstr, uint count, uint size)
{
        //Insert count nops in the beginning
        for(int i = 0; i < count;i++)
        {
            *(ENCODE_32 *)(pc + i * sizeof(ENCODE_32)) = 0x8000F3AF;
        }

        if (size == sizeof(ENCODE_16))
        {
            *(ENCODE_16 *)(pc + count * sizeof(ENCODE_32)) = (ENCODE_16)(outInstr & 0x0000ffff);
            *(ENCODE_16 *)(pc + sizeof(ENCODE_16) + count * sizeof(ENCODE_32)) = (ENCODE_16)(0xBF00);
        }
        else
        {
            Assert(size == sizeof(ENCODE_32));
            *(ENCODE_32 *)(pc + count * sizeof(ENCODE_32)) = outInstr;
        }

        //Insert count nops at the end;
        for(int i = count + 1; i < (2 *count + 1); i++)
        {
            *(ENCODE_32 *)(pc + i * sizeof(ENCODE_32)) = 0x8000F3AF;
        }

        return MachInt*(2*count + 1);
}
#endif //INSERT_NOPS

#ifdef SOFTWARE_FIXFOR_HARDWARE_BUGWIN8_502326
bool
EncoderMD::IsBuggyHardware()
{
    return true;
    // TODO: Enable this for restricting to Qualcomm Krait cores affected: KR28M2A10, KR28M2A11, KR28M2A12
    /*
    AssertMsg(AutoSystemInfo::Data.wProcessorArchitecture == 5, "This has to be ARM architecture");
    if (((AutoSystemInfo::Data.wProcessorLevel & 0xFC0) == 0x40)  && ((AutoSystemInfo::Data.wProcessorRevision & 0xF0) == 0))
    {
#if DBG_DUMP
        if (Js::Configuration::Global.flags.Trace.IsEnabled(Js::EncoderPhase))
        {
            Output::Print(L"TRACE: Running in buggy hardware.\n");
        }
#endif
        return true;
    }
    return false;
    */
}

bool
EncoderMD::CheckBranchInstrCriteria(IR::Instr* instr)
{
    if (ISQBUGGYBR(instr->m_opcode))
    {
        return true;
    }
#if DBG
    switch (instr->m_opcode)
    {
        case Js::OpCode::RET: //This is never Thumb2 hence we are safe in this hardware bug
            return false;

        case Js::OpCode::BL:
            AssertMsg(false, "We don't generate these now. Include in the opcode list above for BL T1 encodings");
            return false;

        case Js::OpCode::BLX:
            AssertMsg(instr->GetSrc1()->IsRegOpnd(),"If  we generate label include in the opcode list above");
            //Fallthrough
        default:
            {
                //Assert to make sure none of the other instructions have target as PC register.
                if (instr->GetDst() && instr->GetDst()->IsRegOpnd())
                {
                    AssertMsg(instr->GetDst()->AsRegOpnd()->GetReg() != RegPC, "Check for this opcode above");
                }
            }
    }
#endif

    return false;
}

#endif //SOFTWARE_FIXFOR_HARDWARE_BUGWIN8_502326
///----------------------------------------------------------------------------
///
/// EncoderMD::Encode
///
///     Emit the ARM encoding for the given instruction in the passed in
///     buffer ptr.
///
///----------------------------------------------------------------------------

ptrdiff_t
EncoderMD::Encode(IR::Instr *instr, BYTE *pc, BYTE* beginCodeAddress)
{
    m_pc = pc;

    ENCODE_32  outInstr;
    IFORM  iform;
    int    size = 0;

    // Instructions must be lowered, we don't handle non-MD opcodes here.
    Assert(instr != nullptr);

    if (instr->IsLowered() == false)
    {
        if (instr->IsLabelInstr())
        {
            if (instr->isInlineeEntryInstr)
            {
                Js::Var inlineeCallInfo = 0;
                const bool encodeResult = Js::InlineeCallInfo::Encode(inlineeCallInfo, instr->AsLabelInstr()->GetOffset(), m_pc - m_encoder->m_encodeBuffer);
                Assert(encodeResult);
                //We are re-using offset to save the inlineeCallInfo which will be patched in ApplyRelocs
                //This is a cleaner way to patch MOVW\MOVT pair with the right inlineeCallInfo
                instr->AsLabelInstr()->ResetOffset((uint32)inlineeCallInfo);
            }
            else
            {
                instr->AsLabelInstr()->SetPC(m_pc);
                if (instr->AsLabelInstr()->m_id == m_func->m_unwindInfo.GetPrologStartLabel())
                {
                    m_func->m_unwindInfo.SetPrologOffset(m_pc - m_encoder->m_encodeBuffer);
                }
                else if (instr->AsLabelInstr()->m_id == m_func->m_unwindInfo.GetEpilogEndLabel())
                {
                    // This is the last instruction in the epilog. Any instructions that follow
                    // are separated code, so the unwind info will have to represent them as a function
                    // fragment. (If there's no separated code, then this offset will equal the total
                    // code size.)
                    m_func->m_unwindInfo.SetEpilogEndOffset(m_pc - m_encoder->m_encodeBuffer - m_func->m_unwindInfo.GetPrologOffset());
                }
            }
        }
    #if DBG_DUMP
        if (instr->IsEntryInstr() && Js::Configuration::Global.flags.DebugBreak.Contains(m_func->GetFunctionNumber()))
        {
            IR::Instr *int3 = IR::Instr::New(Js::OpCode::DEBUGBREAK, m_func);
            return this->Encode(int3, m_pc);
        }
    #endif
        return 0;
    }

#ifdef SOFTWARE_FIXFOR_HARDWARE_BUGWIN8_502326
    if (IsBuggyHardware())
    {
        // Hardware bug is in Qualcomm 8960. 32 bit thumb branch instructions might not jump to the correct address in following
        // conditions:
        //  a.3 T16 thumb instruction followed by T32 branch instruction (conditional\unconditional & RegPc load).
        //  b.Branch instruction starts at  0x*****FBE

        // As we don't know the final address, instead of checking for 0x*FBE we just check for offset 0x*E
        // as the final function start address is always aligned at 16 byte boundary (EMIT_BUFFER_ALIGNMENT)
        if (consecutiveThumbInstrCount >=  3 && (((uint)(m_pc - beginCodeAddress) & 0xF) == 0xE) && CheckBranchInstrCriteria(instr))
        {
            Assert(beginCodeAddress);
            IR::Instr *nop = IR::Instr::New(Js::OpCode::VMOV,
                                                        IR::RegOpnd::New(nullptr, RegD15, TyMachDouble, this->m_func),
                                                        IR::RegOpnd::New(nullptr, RegD15, TyMachDouble, this->m_func),
                                                        m_func);
            size = this->Encode(nop, m_pc);
            consecutiveThumbInstrCount = 0;
            size+= this->Encode(instr, m_pc + size);
    #if DBG_DUMP
            if (Js::Configuration::Global.flags.Trace.IsEnabled(Js::EncoderPhase))
            {
                Output::Print(L"TRACE: Avoiding Branch instruction and Dummy nops at 0x*E \n");
            }
    #endif
            Assert(size == 8);
            // We are okay with returning size 8 as the previous 3 thumb instructions any way would have saved 6 bytes
            // and doesn't alter the logic of allocating temp buffer based on MachMaxInstrSize
            return size;
        }
    }
#endif

    InstructionType instrType = this->CanonicalizeInstr(instr);
    switch(instrType)
    {
        case Thumb:
            size = 2;
            consecutiveThumbInstrCount++;
            break;
        case Thumb2:
            size = 4;
            consecutiveThumbInstrCount = 0;
            break;
        case Vfp:
            size = 4;
            consecutiveThumbInstrCount = 0;
            break;

        default: Assert(false);
    }

    AssertMsg(size != MachChar, "Thumb2 is never single Byte");

    iform = (IFORM)GetForm(instr, size);
    outInstr = GenerateEncoding(instr, iform, m_pc, size, instrType);

    if (outInstr == 0)
    {
        return 0;
    }

    // TODO: Check if VFP/Neon instructions in Thumb-2 mode we need to swap the instruction halfwords
    if (size == sizeof(ENCODE_16))
    {
#ifdef INSERT_NOPS
        return insertNops(m_pc, outInstr, CountNops, sizeof(ENCODE_16));
#else
        //2 byte Thumb encoding
        Assert((outInstr & 0xffff0000) == 0);
        *(ENCODE_16 *)m_pc = (ENCODE_16)(outInstr & 0x0000ffff);
        return MachShort;
#endif
    }
    else if (size == sizeof(ENCODE_32))
    {
#ifdef INSERT_NOPS
        return insertNops(m_pc, outInstr, CountNops, sizeof(ENCODE_32));
#else
        //4 byte Thumb2 encoding
        *(ENCODE_32 *)m_pc = outInstr ;
        return MachInt;
#endif
    }

    AssertMsg(UNREACHED, "Unexpected size");
    return 0;
}

bool
EncoderMD::CanEncodeModConst12(DWORD constant)
{
    DWORD encode;
    return EncodeModConst12(constant, &encode);
}

bool
EncoderMD::EncodeModConst12(DWORD constant, DWORD * result)
{
    unsigned int a, b, c, d, rotation, firstbit, lastbit, temp=0;

    if (constant == 0)
    {
        *result = 0;
        return true;
    }

    a = constant & 0xff;
    b = (constant >> 8) & 0xff;
    c = (constant >> 16) & 0xff;
    d = (constant >> 24) & 0xff;

    _BitScanReverse((DWORD*)&firstbit, constant);
    _BitScanForward((DWORD*)&lastbit, constant);

    if (! ((a == 0 && c == 0 && b == d)
        || (b == 0 && d == 0 && a == c)
        || (a == b && b == c && c == d)
        || (firstbit-lastbit < 8) ))
    {
        return false;
    }

    *result = 0;
    if (constant <= 0xFF)
    {
        *result |= constant << 16;
    }
    else if (firstbit-lastbit < 8)
    {
        if (firstbit > 7)
        {
            temp |= 0x7F & (constant >> (firstbit-7));
            rotation = 32-firstbit+7;
        }
        else
        {
            temp |= 0x7F & (constant << (7-firstbit));
            rotation = 7-firstbit;
        }
        *result = (temp & 0xFF) << 16;
        *result |= (0x10 & rotation) << 6;
        *result |= (0xE & rotation) << 27;
        *result |= (0x1 & rotation) << 23;
    }
    else
    {
        if (a==0 && c==0 && b==d)
        {
            *result |= 0x20000000;     // HW2[12]
            *result |= (0xFF & b) << 16;
        }
        else if (a==c && b==0 && d==0)
        {
            *result |= 0x10000000;
            *result |= (0xFF & a) << 16;
        }
        else if (a==b && b==c && c==d)
        {
            *result |= 0x30000000;
            *result |= (0xFF & d) << 16;
        }
        else
        {
            Assert(UNREACHED);
        }
    }
    return true;
}

///----------------------------------------------------------------------------
///
/// EncodeReloc::New
///
///----------------------------------------------------------------------------

void
EncodeReloc::New(EncodeReloc **pHead, RelocType relocType, BYTE *offset, IR::Instr *relocInstr, ArenaAllocator *alloc)
{
    EncodeReloc *newReloc      = AnewStruct(alloc, EncodeReloc);
    newReloc->m_relocType      = relocType;
    newReloc->m_consumerOffset = offset;
    newReloc->m_next           = *pHead;
    newReloc->m_relocInstr     = relocInstr;
    *pHead                     = newReloc;
}


ENCODE_32 EncoderMD::CallOffset(int x)
{
    Assert(IS_CONST_INT24(x >> 1));

    ENCODE_32 ret;
    int Sflag = (x & 0x1000000) >> 24;
    int off23 = (x & 0x800000) >> 23;
    int off22 = (x & 0x400000) >> 22;

    ret = (x & 0xFFE) << 15;
    ret |= (x & 0x3FF000) >> 12;
    ret |= (((~off23) ^ Sflag) & 0x1) << (16+13);
    ret |= (((~off22) ^ Sflag) & 0x1) << (16+11);
    ret |= (Sflag << 10);
    return ret;
}

ENCODE_32 EncoderMD::BranchOffset_T2_24(int x)
{
    x -= 4;
    Assert(IS_CONST_INT24(x >> 1));

    int ret;
    int Sflag = (x & 0x1000000) >> 24;
    int off23 = (x & 0x800000) >> 23;
    int off22 = (x & 0x400000) >> 22;

    ret = (x & 0xFFE) << 15;
    ret |= (x & 0x3FF000) >> 12;
    ret |= (((~off23) ^ Sflag) & 0x1) << (16+13);
    ret |= (((~off22) ^ Sflag) & 0x1) << (16+11);
    ret |= (Sflag << 10);
    return INSTR_TYPE(ret);
}

ENCODE_32 EncoderMD::BranchOffset_T2_20(int x)
{
    x -= 4;
    Assert(IS_CONST_INT21(x));

    uint32 ret;
    uint32 Sflag = (x & 0x100000) >> 20;
    uint32 off19 = (x & 0x80000) >> 19;
    uint32 off18 = (x & 0x40000) >> 18;

    ret = (x & 0xFFE) << 15;
    ret |= (x & 0x3F000) >> 12;
    ret |= off18 << (13+16);
    ret |= off19 << (11+16);
    ret |= (Sflag << 10);
    return ret;
}

void
EncoderMD::BaseAndOffsetFromSym(IR::SymOpnd *symOpnd, RegNum *pBaseReg, int32 *pOffset, Func * func)
{
    StackSym *stackSym = symOpnd->m_sym->AsStackSym();

    RegNum baseReg = func->GetLocalsPointer();
    int32 offset = stackSym->m_offset + symOpnd->m_offset;
    if (baseReg == RegSP)
    {
        // SP points to the base of the argument area. Non-reg SP points directly to the locals.
        offset += (func->m_argSlotsForFunctionsCalled * MachRegInt);
        if (func->GetMaxInlineeArgOutCount())
        {
            Assert(func->HasInlinee());
            if ((!stackSym->IsArgSlotSym() || stackSym->m_isOrphanedArg) && !stackSym->IsParamSlotSym())
            {
                offset += func->GetInlineeArgumentStackSize();
            }
        }
    }

    if (stackSym->IsParamSlotSym())
    {
        offset += func->m_localStackHeight + func->m_ArgumentsOffset;
        if (!EncoderMD::CanEncodeLoadStoreOffset(offset))
        {
            // Use the frame pointer. No need to hoist an offset for a param.
            baseReg = FRAME_REG;
            offset = stackSym->m_offset + symOpnd->m_offset - (Js::JavascriptFunctionArgIndex_Frame * MachRegInt);
            Assert(EncoderMD::CanEncodeLoadStoreOffset(offset));
        }
    }
#ifdef DBG
    else
    {
        // Locals are offset by the size of the area allocated for stack args.
        Assert(offset >= 0);
        Assert(baseReg != RegSP || (uint)offset >= (func->m_argSlotsForFunctionsCalled * MachRegInt));

        if (func->GetMaxInlineeArgOutCount())
        {
            Assert(baseReg == RegSP);
            if (stackSym->IsArgSlotSym() && !stackSym->m_isOrphanedArg)
            {
                Assert(stackSym->m_isInlinedArgSlot);
                Assert((uint)offset <= ((func->m_argSlotsForFunctionsCalled + func->GetMaxInlineeArgOutCount()) * MachRegInt));
            }
            else
            {
                AssertMsg(stackSym->IsAllocated(), "StackSym offset should be set");
                Assert((uint)offset > ((func->m_argSlotsForFunctionsCalled + func->GetMaxInlineeArgOutCount()) * MachRegInt));
            }
        }
        // TODO: restore the following assert (very useful) once we have a way to tell whether prolog/epilog
        // gen is complete.
        //Assert(offset < func->m_localStackHeight);
    }
#endif
    *pBaseReg = baseReg;
    *pOffset = offset;
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
    for (EncodeReloc *reloc = m_relocList; reloc; reloc = reloc->m_next)
    {
        BYTE * relocAddress = reloc->m_consumerOffset;
        int32 pcrel;
        ENCODE_32 encode = *(ENCODE_32*)relocAddress;
        switch (reloc->m_relocType)
        {
        case RelocTypeBranch20:
            {
                IR::LabelInstr * labelInstr = reloc->m_relocInstr->AsLabelInstr();
                Assert(!labelInstr->isInlineeEntryInstr);
                AssertMsg(labelInstr->GetPC() != nullptr, "Branch to unemitted label?");
                pcrel = (uint32)(labelInstr->GetPC() - reloc->m_consumerOffset);
                encode |= BranchOffset_T2_20(pcrel);
                *(uint32 *)relocAddress = encode;
                break;
            }

        case RelocTypeBranch24:
            {
                IR::LabelInstr * labelInstr = reloc->m_relocInstr->AsLabelInstr();
                Assert(!labelInstr->isInlineeEntryInstr);
                AssertMsg(labelInstr->GetPC() != nullptr, "Branch to unemitted label?");
                pcrel = (uint32)(labelInstr->GetPC() - reloc->m_consumerOffset);
                encode |= BranchOffset_T2_24(pcrel);
                *(ENCODE_32 *)relocAddress = encode;
                break;
            }

        case RelocTypeDataLabelLow:
            {
                IR::LabelInstr * labelInstr = reloc->m_relocInstr->AsLabelInstr();
                Assert(!labelInstr->isInlineeEntryInstr && labelInstr->m_isDataLabel);

                AssertMsg(labelInstr->GetPC() != nullptr, "Branch to unemitted label?");

                pcrel = ((labelInstr->GetPC() - m_encoder->m_encodeBuffer + codeBufferAddress) & 0xFFFF);

                if (!EncodeImmediate16(pcrel, (DWORD*) &encode))
                {
                    Assert(UNREACHED);
                }
                *(ENCODE_32 *) relocAddress = encode;
                break;
            }

        case RelocTypeLabelLow:
            {
                // Absolute (not relative) label address (lower 16 bits)
                IR::LabelInstr * labelInstr = reloc->m_relocInstr->AsLabelInstr();
                if (!labelInstr->isInlineeEntryInstr)
                {
                    AssertMsg(labelInstr->GetPC() != nullptr, "Branch to unemitted label?");
                    // Note that the bottom bit must be set, since this is a Thumb code address.
                    pcrel = ((labelInstr->GetPC() - m_encoder->m_encodeBuffer + codeBufferAddress) & 0xFFFF) | 1;
                }
                else
                {
                    //This is a encoded low 16 bits.
                    pcrel = labelInstr->GetOffset() & 0xFFFF;
                }
                if (!EncodeImmediate16(pcrel, (DWORD*) &encode))
                {
                    Assert(UNREACHED);
                }
                *(ENCODE_32 *) relocAddress = encode;
                break;
            }

        case RelocTypeLabelHigh:
            {
                // Absolute (not relative) label address (upper 16 bits)
                IR::LabelInstr * labelInstr = reloc->m_relocInstr->AsLabelInstr();
                if (!labelInstr->isInlineeEntryInstr)
                {
                    AssertMsg(labelInstr->GetPC() != nullptr, "Branch to unemitted label?");
                    pcrel = (labelInstr->GetPC() - m_encoder->m_encodeBuffer + codeBufferAddress) >> 16;
                    // We only record the relocation on the low byte of the pair
                }
                else
                {
                    //This is a encoded high 16 bits.
                    pcrel = labelInstr->GetOffset() >> 16;
                }
                if (!EncodeImmediate16(pcrel, (DWORD*) &encode))
                {
                    Assert(UNREACHED);
                }
                *(ENCODE_32 *) relocAddress = encode;
                break;
            }

        case RelocTypeLabel:
            {
                IR::LabelInstr * labelInstr = reloc->m_relocInstr->AsLabelInstr();
                AssertMsg(labelInstr->GetPC() != nullptr, "Branch to unemitted label?");
                /* For Thumb instruction set -> OR 1 with the address*/
                *(uint32 *)relocAddress = (uint32)(labelInstr->GetPC() - m_encoder->m_encodeBuffer + codeBufferAddress)  | 1;
                break;
            }
        default:
            AssertMsg(UNREACHED, "Unknown reloc type");
        }
    }
}

void
EncoderMD::EncodeInlineeCallInfo(IR::Instr *instr, uint32 codeOffset)
{
     IR::LabelInstr* inlineeStart = instr->AsLabelInstr();
     Assert((inlineeStart->GetOffset() & 0x0F) == inlineeStart->GetOffset());
     return;
}

bool EncoderMD::TryConstFold(IR::Instr *instr, IR::RegOpnd *regOpnd)
{
    Assert(regOpnd->m_sym->IsConst());

    if (instr->m_opcode == Js::OpCode::MOV)
    {
        if (instr->GetSrc1() != regOpnd)
        {
            return false;
        }
        if (!instr->GetDst()->IsRegOpnd())
        {
            return false;
        }

        instr->ReplaceSrc(regOpnd, regOpnd->m_sym->GetConstOpnd());
        LegalizeMD::LegalizeInstr(instr, false);

        return true;
    }
    else
    {
        return false;
    }
}

bool EncoderMD::TryFold(IR::Instr *instr, IR::RegOpnd *regOpnd)
{
    if (LowererMD::IsAssign(instr))
    {
        if (!instr->GetDst()->IsRegOpnd() || regOpnd != instr->GetSrc1())
        {
            return false;
        }
        IR::SymOpnd *symOpnd = IR::SymOpnd::New(regOpnd->m_sym, regOpnd->GetType(), instr->m_func);
        instr->ReplaceSrc(regOpnd, symOpnd);
        LegalizeMD::LegalizeInstr(instr, false);

        return true;
    }
    else
    {
        return false;
    }
}

void EncoderMD::AddLabelReloc(BYTE* relocAddress)
{
    Assert(relocAddress != nullptr);
    EncodeReloc::New(&m_relocList, RelocTypeLabel, relocAddress, *(IR::Instr**)relocAddress, m_encoder->m_tempAlloc);
}

