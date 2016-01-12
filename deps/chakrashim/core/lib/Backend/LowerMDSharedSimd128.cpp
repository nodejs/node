//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "BackEnd.h"

#define GET_SIMDOPCODE(irOpcode) m_simd128OpCodesMap[(uint32)(irOpcode - Js::OpCode::Simd128_Start)]


#define SET_SIMDOPCODE(irOpcode, mdOpcode) \
    Assert((uint32)m_simd128OpCodesMap[(uint32)(Js::OpCode::irOpcode - Js::OpCode::Simd128_Start)] == 0); \
    Assert(Js::OpCode::mdOpcode > Js::OpCode::MDStart); \
    m_simd128OpCodesMap[(uint32)(Js::OpCode::irOpcode - Js::OpCode::Simd128_Start)] = Js::OpCode::mdOpcode;

IR::Instr* LowererMD::Simd128Instruction(IR::Instr *instr)
{
    // Currently only handles type-specialized/asm.js opcodes

    if (!instr->GetDst())
    {
        // SIMD ops always have DST in asmjs
        Assert(!instr->m_func->GetJnFunction()->GetIsAsmjsMode());
        // unused result. Do nothing.
        IR::Instr * pInstr = instr->m_prev;
        instr->Remove();
        return pInstr;
    }

    if (Simd128TryLowerMappedInstruction(instr))
    {
        return instr->m_prev;
    }
    return Simd128LowerUnMappedInstruction(instr);
}

bool LowererMD::Simd128TryLowerMappedInstruction(IR::Instr *instr)
{
    bool legalize = true;
    Js::OpCode opcode = GET_SIMDOPCODE(instr->m_opcode);

    if ((uint32)opcode == 0)
        return false;

    Assert(instr->GetDst() && instr->GetDst()->IsRegOpnd() && instr->GetDst()->IsSimd128() || instr->GetDst()->GetType() == TyInt32);
    Assert(instr->GetSrc1() && instr->GetSrc1()->IsRegOpnd() && instr->GetSrc1()->IsSimd128());
    Assert(!instr->GetSrc2() || (((instr->GetSrc2()->IsRegOpnd() && instr->GetSrc2()->IsSimd128()) || (instr->GetSrc2()->IsIntConstOpnd() && instr->GetSrc2()->GetType() == TyInt8))));

    switch (instr->m_opcode)
    {
    case Js::OpCode::Simd128_Abs_F4:
        Assert(opcode == Js::OpCode::ANDPS);
        instr->SetSrc2(IR::MemRefOpnd::New((void*)&X86_ABS_MASK_F4, instr->GetSrc1()->GetType(), m_func));
        break;
    case Js::OpCode::Simd128_Abs_D2:
        Assert(opcode == Js::OpCode::ANDPD);
        instr->SetSrc2(IR::MemRefOpnd::New((void*)&X86_ABS_MASK_D2, instr->GetSrc1()->GetType(), m_func));
        break;
    case Js::OpCode::Simd128_Neg_F4:
        Assert(opcode == Js::OpCode::XORPS);
        instr->SetSrc2(IR::MemRefOpnd::New((void*)&X86_NEG_MASK_F4, instr->GetSrc1()->GetType(), m_func));
        break;
    case Js::OpCode::Simd128_Neg_D2:
        Assert(opcode == Js::OpCode::XORPS);
        instr->SetSrc2(IR::MemRefOpnd::New((void*)&X86_NEG_MASK_D2, instr->GetSrc1()->GetType(), m_func));
        break;
    case Js::OpCode::Simd128_Not_F4:
    case Js::OpCode::Simd128_Not_I4:
        Assert(opcode == Js::OpCode::XORPS);
        instr->SetSrc2(IR::MemRefOpnd::New((void*)&X86_ALL_NEG_ONES, instr->GetSrc1()->GetType(), m_func));
        break;
    case Js::OpCode::Simd128_Gt_F4:
    case Js::OpCode::Simd128_Gt_D2:
    case Js::OpCode::Simd128_GtEq_F4:
    case Js::OpCode::Simd128_GtEq_D2:
    case Js::OpCode::Simd128_Lt_I4:
    {
        Assert(opcode == Js::OpCode::CMPLTPS || opcode == Js::OpCode::CMPLTPD || opcode == Js::OpCode::CMPLEPS || opcode == Js::OpCode::CMPLEPD || opcode == Js::OpCode::PCMPGTD);
        // swap operands
        auto *src1 = instr->UnlinkSrc1();
        auto *src2 = instr->UnlinkSrc2();
        instr->SetSrc1(src2);
        instr->SetSrc2(src1);
        break;
    }
    case Js::OpCode::Simd128_LdSignMask_F4:
    case Js::OpCode::Simd128_LdSignMask_I4:
    case Js::OpCode::Simd128_LdSignMask_D2:
        legalize = false;
        break;
    }
    instr->m_opcode = opcode;
    if (legalize)
    {
        //MakeDstEquSrc1(instr);
        Legalize(instr);
    }

    return true;
}

IR::Instr* LowererMD::Simd128LowerUnMappedInstruction(IR::Instr *instr)
{
    switch (instr->m_opcode)
    {
    case Js::OpCode::Simd128_LdC:
        return Simd128LoadConst(instr);

    case Js::OpCode::Simd128_FloatsToF4:
    case Js::OpCode::Simd128_IntsToI4:
    case Js::OpCode::Simd128_DoublesToD2:
        return Simd128LowerConstructor(instr);

    case Js::OpCode::Simd128_ExtractLane_I4:
    case Js::OpCode::Simd128_ExtractLane_F4:
        return Simd128LowerLdLane(instr);

    case Js::OpCode::Simd128_ReplaceLane_I4:
    case Js::OpCode::Simd128_ReplaceLane_F4:
        return SIMD128LowerReplaceLane(instr);

    case Js::OpCode::Simd128_Splat_F4:
    case Js::OpCode::Simd128_Splat_I4:
    case Js::OpCode::Simd128_Splat_D2:
        return Simd128LowerSplat(instr);

    case Js::OpCode::Simd128_Rcp_F4:
    case Js::OpCode::Simd128_Rcp_D2:
        return Simd128LowerRcp(instr);

    case Js::OpCode::Simd128_Sqrt_F4:
    case Js::OpCode::Simd128_Sqrt_D2:
        return Simd128LowerSqrt(instr);

    case Js::OpCode::Simd128_RcpSqrt_F4:
    case Js::OpCode::Simd128_RcpSqrt_D2:
        return Simd128LowerRcpSqrt(instr);

    case Js::OpCode::Simd128_Select_F4:
    case Js::OpCode::Simd128_Select_I4:
    case Js::OpCode::Simd128_Select_D2:
        return Simd128LowerSelect(instr);

    case Js::OpCode::Simd128_Neg_I4:
        return Simd128LowerNegI4(instr);

    case Js::OpCode::Simd128_Mul_I4:
        return Simd128LowerMulI4(instr);

    case Js::OpCode::Simd128_LdArr_I4:
    case Js::OpCode::Simd128_LdArr_F4:
    case Js::OpCode::Simd128_LdArr_D2:
    case Js::OpCode::Simd128_LdArrConst_I4:
    case Js::OpCode::Simd128_LdArrConst_F4:
    case Js::OpCode::Simd128_LdArrConst_D2:
        return Simd128LowerLoadElem(instr);

    case Js::OpCode::Simd128_StArr_I4:
    case Js::OpCode::Simd128_StArr_F4:
    case Js::OpCode::Simd128_StArr_D2:
    case Js::OpCode::Simd128_StArrConst_I4:
    case Js::OpCode::Simd128_StArrConst_F4:
    case Js::OpCode::Simd128_StArrConst_D2:
        return Simd128LowerStoreElem(instr);

    case Js::OpCode::Simd128_Swizzle_I4:
    case Js::OpCode::Simd128_Swizzle_F4:
    case Js::OpCode::Simd128_Swizzle_D2:
    case Js::OpCode::Simd128_Shuffle_I4:
    case Js::OpCode::Simd128_Shuffle_F4:
    case Js::OpCode::Simd128_Shuffle_D2:
        return Simd128LowerShuffle(instr);


    default:
        AssertMsg(UNREACHED, "Unsupported Simd128 instruction");
    }
    return nullptr;
}

IR::Instr* LowererMD::Simd128LoadConst(IR::Instr* instr)
{
    Assert(instr->GetDst() && instr->m_opcode == Js::OpCode::Simd128_LdC);
    if (instr->GetDst()->IsSimd128())
    {
        Assert(instr->GetSrc1()->IsSimd128());
        Assert(instr->GetSrc1()->IsSimd128ConstOpnd());
        Assert(instr->GetSrc2() == nullptr);

        AsmJsSIMDValue value = instr->GetSrc1()->AsSimd128ConstOpnd()->m_value;

        // MOVUPS dst, [const]
        AsmJsSIMDValue *pValue = NativeCodeDataNew(instr->m_func->GetNativeCodeDataAllocator(), AsmJsSIMDValue);
        pValue->SetValue(value);
        IR::Opnd * opnd = IR::MemRefOpnd::New((void *)pValue, instr->GetDst()->GetType(), instr->m_func);
        instr->ReplaceSrc1(opnd);
        instr->m_opcode = LowererMDArch::GetAssignOp(instr->GetDst()->GetType());
        Legalize(instr);
        return instr->m_prev;
    }
    else
    {
        AssertMsg(UNREACHED, "Non-typespecialized form of Simd128_LdC is unsupported");
    }
    return nullptr;
}

IR::Instr* LowererMD::Simd128LowerConstructor(IR::Instr *instr)
{

    IR::Opnd* dst  = nullptr;
    IR::Opnd* src1 = nullptr;
    IR::Opnd* src2 = nullptr;
    IR::Opnd* src3 = nullptr;
    IR::Opnd* src4 = nullptr;
    IR::Instr* newInstr = nullptr;

    if (instr->m_opcode == Js::OpCode::Simd128_FloatsToF4 || instr->m_opcode == Js::OpCode::Simd128_IntsToI4)
    {
        // use MOVSS for both int32x4 and float32x4. MOVD zeroes upper bits.
        Js::OpCode movOpcode = Js::OpCode::MOVSS;
        Js::OpCode shiftOpcode = Js::OpCode::PSLLDQ;
        SList<IR::Opnd*> *args = Simd128GetExtendedArgs(instr);

        // The number of src opnds should be exact. If opnds are missing, they should be filled in by globopt during type-spec.
        Assert(args->Count() == 5);

        dst = args->Pop();
        src1 = args->Pop();
        src2 = args->Pop();
        src3 = args->Pop();
        src4 = args->Pop();

        if (instr->m_opcode == Js::OpCode::Simd128_FloatsToF4)
        {
            // We don't have f32 type-spec, so we type-spec to f64 and convert to f32 before use.

            if (src1->IsFloat64())
            {
                IR::RegOpnd *regOpnd32 = IR::RegOpnd::New(TyFloat32, this->m_func);
                // CVTSD2SS regOpnd32.f32, src.f64    -- Convert regOpnd from f64 to f32
                newInstr = IR::Instr::New(Js::OpCode::CVTSD2SS, regOpnd32, src1, this->m_func);
                instr->InsertBefore(newInstr);
                src1 = regOpnd32;
            }
            if (src2->IsFloat64())
            {
                IR::RegOpnd *regOpnd32 = IR::RegOpnd::New(TyFloat32, this->m_func);
                // CVTSD2SS regOpnd32.f32, src.f64    -- Convert regOpnd from f64 to f32
                newInstr = IR::Instr::New(Js::OpCode::CVTSD2SS, regOpnd32, src2, this->m_func);
                instr->InsertBefore(newInstr);
                src2 = regOpnd32;
            }
            if (src3->IsFloat64())
            {
                IR::RegOpnd *regOpnd32 = IR::RegOpnd::New(TyFloat32, this->m_func);
                // CVTSD2SS regOpnd32.f32, src.f64    -- Convert regOpnd from f64 to f32
                newInstr = IR::Instr::New(Js::OpCode::CVTSD2SS, regOpnd32, src3, this->m_func);
                instr->InsertBefore(newInstr);
                src3 = regOpnd32;
            }
            if (src4->IsFloat64())
            {
                IR::RegOpnd *regOpnd32 = IR::RegOpnd::New(TyFloat32, this->m_func);
                // CVTSD2SS regOpnd32.f32, src.f64    -- Convert regOpnd from f64 to f32
                newInstr = IR::Instr::New(Js::OpCode::CVTSD2SS, regOpnd32, src4, this->m_func);
                instr->InsertBefore(newInstr);
                src4 = regOpnd32;
            }

            Assert(src1->IsRegOpnd() && src1->GetType() == TyFloat32);
            Assert(src2->IsRegOpnd() && src2->GetType() == TyFloat32);
            Assert(src3->IsRegOpnd() && src3->GetType() == TyFloat32);
            Assert(src4->IsRegOpnd() && src4->GetType() == TyFloat32);

            // MOVSS dst, src4
            instr->InsertBefore(IR::Instr::New(movOpcode, dst, src4, m_func));
            // PSLLDQ dst, dst, 4
            instr->InsertBefore(IR::Instr::New(shiftOpcode, dst, dst, IR::IntConstOpnd::New(4, TyInt8, m_func, true), m_func));

            // MOVSS dst, src3
            instr->InsertBefore(IR::Instr::New(movOpcode, dst, src3, m_func));
            // PSLLDQ dst, 4
            instr->InsertBefore(IR::Instr::New(shiftOpcode, dst, dst, IR::IntConstOpnd::New(4, TyInt8, m_func, true), m_func));

            // MOVSS dst, src2
            instr->InsertBefore(IR::Instr::New(movOpcode, dst, src2, m_func));
            // PSLLDQ dst, 4
            instr->InsertBefore(IR::Instr::New(shiftOpcode, dst, dst, IR::IntConstOpnd::New(4, TyInt8, m_func, true), m_func));

            // MOVSS dst, src1
            instr->InsertBefore(IR::Instr::New(movOpcode, dst, src1, m_func));
        }
        else
        {
            //Simd128_IntsToI4

            // b-namost: better way to implement this on SSE2? Using MOVD directly zeroes upper bits.
            IR::RegOpnd *temp = IR::RegOpnd::New(TyFloat32, m_func);

            // src's might have been constant prop'ed. Enregister them if so.
            src4 = EnregisterIntConst(instr, src4);
            src3 = EnregisterIntConst(instr, src3);
            src2 = EnregisterIntConst(instr, src2);
            src1 = EnregisterIntConst(instr, src1);

            Assert(src1->GetType() == TyInt32 && src1->IsRegOpnd());
            Assert(src2->GetType() == TyInt32 && src2->IsRegOpnd());
            Assert(src3->GetType() == TyInt32 && src3->IsRegOpnd());
            Assert(src4->GetType() == TyInt32 && src4->IsRegOpnd());

            // MOVD t(TyFloat32), src4(TyInt32)
            instr->InsertBefore(IR::Instr::New(Js::OpCode::MOVD, temp, src4, m_func));

            // MOVSS dst, t
            instr->InsertBefore(IR::Instr::New(movOpcode, dst, temp, m_func));
            // PSLLDQ dst, dst, 4
            instr->InsertBefore(IR::Instr::New(shiftOpcode, dst, dst, IR::IntConstOpnd::New(TySize[TyInt32], TyInt8, m_func, true), m_func));

            // MOVD t(TyFloat32), sr34(TyInt32)
            instr->InsertBefore(IR::Instr::New(Js::OpCode::MOVD, temp, src3, m_func));
            // MOVSS dst, t
            instr->InsertBefore(IR::Instr::New(movOpcode, dst, temp, m_func));
            // PSLLDQ dst, dst, 4
            instr->InsertBefore(IR::Instr::New(shiftOpcode, dst, dst, IR::IntConstOpnd::New(TySize[TyInt32], TyInt8, m_func, true), m_func));

            // MOVD t(TyFloat32), src2(TyInt32)
            instr->InsertBefore(IR::Instr::New(Js::OpCode::MOVD, temp, src2, m_func));
            // MOVSS dst, t
            instr->InsertBefore(IR::Instr::New(movOpcode, dst, temp, m_func));
            // PSLLDQ dst, dst, 4
            instr->InsertBefore(IR::Instr::New(shiftOpcode, dst, dst, IR::IntConstOpnd::New(TySize[TyInt32], TyInt8, m_func, true), m_func));

            // MOVD t(TyFloat32), src1(TyInt32)
            instr->InsertBefore(IR::Instr::New(Js::OpCode::MOVD, temp, src1, m_func));
            // MOVSS dst, t
            instr->InsertBefore(IR::Instr::New(movOpcode, dst, temp, m_func));
        }
    }
    else
    {
        Assert(instr->m_opcode == Js::OpCode::Simd128_DoublesToD2);
        dst = instr->GetDst();

        src1 = instr->GetSrc1();
        src2 = instr->GetSrc2();

        Assert(src1->IsRegOpnd() && src1->GetType() == TyFloat64);
        Assert(src2->IsRegOpnd() && src2->GetType() == TyFloat64);
        // MOVSD dst, src2
        instr->InsertBefore(IR::Instr::New(Js::OpCode::MOVSD, dst, src2, m_func));
        // PSLLDQ dst, dst, 8
        instr->InsertBefore(IR::Instr::New(Js::OpCode::PSLLDQ, dst, dst, IR::IntConstOpnd::New(TySize[TyFloat64], TyInt8, m_func, true), m_func));
        // MOVSD dst, src1
        instr->InsertBefore(IR::Instr::New(Js::OpCode::MOVSD, dst, src1, m_func));
    }

    Assert(dst->IsRegOpnd() && dst->IsSimd128());
    IR::Instr* prevInstr;
    prevInstr = instr->m_prev;
    instr->Remove();
    return prevInstr;
}

IR::Instr* LowererMD::Simd128LowerLdLane(IR::Instr *instr)
{
    IR::Opnd* dst, *src1, *src2;
    Js::OpCode movOpcode = Js::OpCode::MOVSS;
    uint laneSize = 0, laneIndex = 0;

    dst = instr->GetDst();
    src1 = instr->GetSrc1();
    src2 = instr->GetSrc2();

    Assert(dst && dst->IsRegOpnd() && (dst->GetType() == TyFloat32 || dst->GetType() == TyInt32 || dst->GetType() == TyFloat64));
    Assert(src1 && src1->IsRegOpnd() && src1->IsSimd128());
    Assert(src2 && src2->IsIntConstOpnd());

    laneIndex = (uint)src2->AsIntConstOpnd()->AsUint32();

    switch (instr->m_opcode)
    {
    case Js::OpCode::Simd128_ExtractLane_F4:
        laneSize = 4;
        movOpcode = Js::OpCode::MOVSS;
        Assert(laneIndex < 4);
        break;
    case Js::OpCode::Simd128_ExtractLane_I4:
        laneSize = 4;
        movOpcode = Js::OpCode::MOVD;
        Assert(laneIndex < 4);
        break;
    default:
        Assert(UNREACHED);
    }

    IR::Opnd* tmp = src1;
    if (laneIndex != 0)
    {
        // tmp = PSRLDQ src1, shamt
        tmp = IR::RegOpnd::New(src1->GetType(), m_func);
        IR::Instr *shiftInstr = IR::Instr::New(Js::OpCode::PSRLDQ, tmp, src1, IR::IntConstOpnd::New(laneSize * laneIndex, TyInt8, m_func, true), m_func);
        instr->InsertBefore(shiftInstr);
        //MakeDstEquSrc1(shiftInstr);
        Legalize(shiftInstr);
    }
    // MOVSS/MOVSD/MOVD dst, tmp
    instr->InsertBefore(IR::Instr::New(movOpcode, dst, tmp, m_func));
    IR::Instr* prevInstr = instr->m_prev;
    instr->Remove();

    return prevInstr;
}

IR::Instr* LowererMD::Simd128LowerSplat(IR::Instr *instr)
{
    Js::OpCode shufOpCode = Js::OpCode::SHUFPS, movOpCode = Js::OpCode::MOVSS;
    IR::Opnd *dst, *src1;
    dst = instr->GetDst();
    src1 = instr->GetSrc1();

    Assert(dst && dst->IsRegOpnd() && dst->IsSimd128());
    Assert(src1 && src1->IsRegOpnd() && (src1->GetType() == TyFloat32 || src1->GetType() == TyInt32 || src1->GetType() == TyFloat64));
    Assert(!instr->GetSrc2());

    switch (instr->m_opcode)
    {
    case Js::OpCode::Simd128_Splat_F4:
        shufOpCode = Js::OpCode::SHUFPS;
        movOpCode = Js::OpCode::MOVSS;
        break;
    case Js::OpCode::Simd128_Splat_I4:
        shufOpCode = Js::OpCode::PSHUFD;
        movOpCode = Js::OpCode::MOVD;
        break;
    case Js::OpCode::Simd128_Splat_D2:
        shufOpCode = Js::OpCode::SHUFPD;
        movOpCode = Js::OpCode::MOVSD;
        break;
    default:
        Assert(UNREACHED);
    }

    instr->InsertBefore(IR::Instr::New(movOpCode, dst, src1, m_func));
    instr->InsertBefore(IR::Instr::New(shufOpCode, dst, dst, IR::IntConstOpnd::New(0, TyInt8, m_func, true), m_func));

    IR::Instr* prevInstr = instr->m_prev;
    instr->Remove();

    return prevInstr;

}


IR::Instr* LowererMD::Simd128LowerRcp(IR::Instr *instr, bool removeInstr)
{
    Js::OpCode opcode = Js::OpCode::DIVPS;
    void* x86_allones_mask = nullptr;
    IR::Opnd *dst, *src1;
    dst = instr->GetDst();
    src1 = instr->GetSrc1();


    Assert(dst && dst->IsRegOpnd());
    Assert(src1 && src1->IsRegOpnd());
    Assert(instr->GetSrc2() == nullptr);
    if (instr->m_opcode == Js::OpCode::Simd128_Rcp_F4 || instr->m_opcode == Js::OpCode::Simd128_RcpSqrt_F4)
    {
        Assert(src1->IsSimd128F4() || src1->IsSimd128I4());
        opcode = Js::OpCode::DIVPS;
        x86_allones_mask = (void*)(&X86_ALL_ONES_F4);
    }
    else
    {
        Assert(instr->m_opcode == Js::OpCode::Simd128_Rcp_D2 || instr->m_opcode == Js::OpCode::Simd128_RcpSqrt_D2);
        Assert(src1->IsSimd128D2());
        opcode = Js::OpCode::DIVPD;
        x86_allones_mask = (void*)(&X86_ALL_ONES_D2);
    }
    IR::RegOpnd* tmp = IR::RegOpnd::New(src1->GetType(), m_func);
    IR::Instr* movInstr = IR::Instr::New(Js::OpCode::MOVAPS, tmp, IR::MemRefOpnd::New(x86_allones_mask, src1->GetType(), m_func), m_func);
    instr->InsertBefore(movInstr);
    Legalize(movInstr);

    instr->InsertBefore(IR::Instr::New(opcode, tmp, tmp, src1, m_func));
    instr->InsertBefore(IR::Instr::New(Js::OpCode::MOVAPS, dst, tmp, m_func));
    if (removeInstr)
    {
        IR::Instr* prevInstr = instr->m_prev;
        instr->Remove();
        return prevInstr;
    }
    return instr;
}

IR::Instr* LowererMD::Simd128LowerSqrt(IR::Instr *instr)
{
    Js::OpCode opcode = Js::OpCode::SQRTPS;

    IR::Opnd *dst, *src1;
    dst = instr->GetDst();
    src1 = instr->GetSrc1();

    Assert(dst && dst->IsRegOpnd());
    Assert(src1 && src1->IsRegOpnd());
    Assert(instr->GetSrc2() == nullptr);
    if (instr->m_opcode == Js::OpCode::Simd128_Sqrt_F4)
    {
        opcode = Js::OpCode::SQRTPS;
    }
    else
    {
        Assert(instr->m_opcode == Js::OpCode::Simd128_Sqrt_D2);
        opcode = Js::OpCode::SQRTPD;
    }

    instr->InsertBefore(IR::Instr::New(opcode, dst, src1, m_func));

    IR::Instr* prevInstr = instr->m_prev;
    instr->Remove();
    return prevInstr;
}

IR::Instr* LowererMD::Simd128LowerRcpSqrt(IR::Instr *instr)
{
    Js::OpCode opcode = Js::OpCode::SQRTPS;
    Simd128LowerRcp(instr, false);

    if (instr->m_opcode == Js::OpCode::Simd128_RcpSqrt_F4)
    {
        opcode = Js::OpCode::SQRTPS;
    }
    else
    {
        Assert(instr->m_opcode == Js::OpCode::Simd128_RcpSqrt_D2);
        opcode = Js::OpCode::SQRTPD;
    }
    instr->InsertBefore(IR::Instr::New(opcode, instr->GetDst(), instr->GetDst(), m_func));
    IR::Instr* prevInstr = instr->m_prev;
    instr->Remove();
    return prevInstr;
}

IR::Instr* LowererMD::Simd128LowerSelect(IR::Instr *instr)
{
    Assert(instr->m_opcode == Js::OpCode::Simd128_Select_F4 || instr->m_opcode == Js::OpCode::Simd128_Select_I4 || instr->m_opcode == Js::OpCode::Simd128_Select_D2);
    IR::Opnd* dst = nullptr;
    IR::Opnd* src1 = nullptr;
    IR::Opnd* src2 = nullptr;
    IR::Opnd* src3 = nullptr;
    SList<IR::Opnd*> *args = Simd128GetExtendedArgs(instr);
    // The number of src opnds should be exact. Missing opnds means type-error, and we should generate an exception throw instead (or globopt does).
    Assert(args->Count() == 4);

    dst = args->Pop();
    src1 = args->Pop(); // mask
    src2 = args->Pop(); // trueValue
    src3 = args->Pop(); // falseValue

    Assert(dst->IsRegOpnd() && dst->IsSimd128());
    Assert(src1->IsRegOpnd() && src1->IsSimd128());
    Assert(src2->IsRegOpnd() && src2->IsSimd128());
    Assert(src3->IsRegOpnd() && src3->IsSimd128());


    IR::RegOpnd *tmp = IR::RegOpnd::New(src1->GetType(), m_func);
    IR::Instr *pInstr = nullptr;
    // ANDPS tmp1, mask, tvalue
    pInstr = IR::Instr::New(Js::OpCode::ANDPS, tmp, src1, src2, m_func);
    instr->InsertBefore(pInstr);
    //MakeDstEquSrc1(pInstr);
    Legalize(pInstr);
    // ANDPS dst, mask, fvalue
    pInstr = IR::Instr::New(Js::OpCode::ANDNPS, dst, src1, src3, m_func);
    instr->InsertBefore(pInstr);
    //MakeDstEquSrc1(pInstr);
    Legalize(pInstr);
    // ORPS dst, dst, tmp1
    pInstr = IR::Instr::New(Js::OpCode::ORPS, dst, dst, tmp, m_func);
    instr->InsertBefore(pInstr);

    pInstr = instr->m_prev;
    instr->Remove();
    return pInstr;
}

IR::Instr* LowererMD::Simd128LowerNegI4(IR::Instr *instr)
{
    Assert(instr->m_opcode == Js::OpCode::Simd128_Neg_I4);
    IR::Opnd* dst = instr->GetDst();
    IR::Opnd* src1 = instr->GetSrc1();

    Assert(dst->IsRegOpnd() && dst->IsSimd128());
    Assert(src1->IsRegOpnd() && src1->IsSimd128());
    Assert(instr->GetSrc2() == nullptr);

    // MOVAPS dst, src1
    IR::Instr *pInstr = IR::Instr::New(Js::OpCode::MOVAPS, dst, src1, m_func);
    instr->InsertBefore(pInstr);

    // XORPS dst, dst, 0xfff...f
    pInstr = IR::Instr::New(Js::OpCode::XORPS, dst, dst, IR::MemRefOpnd::New((void*)&X86_ALL_NEG_ONES, src1->GetType(), m_func), m_func);
    instr->InsertBefore(pInstr);
    Legalize(pInstr);

    // PADDD dst, dst, {1,1,1,1}
    pInstr = IR::Instr::New(Js::OpCode::PADDD, dst, dst, IR::MemRefOpnd::New((void*)&X86_ALL_ONES_I4, src1->GetType(), m_func), m_func);
    instr->InsertBefore(pInstr);
    Legalize(pInstr);

    pInstr = instr->m_prev;
    instr->Remove();
    return pInstr;
}

IR::Instr* LowererMD::Simd128LowerMulI4(IR::Instr *instr)
{
    Assert(instr->m_opcode == Js::OpCode::Simd128_Mul_I4);
    IR::Instr *pInstr;
    IR::Opnd* dst = instr->GetDst();
    IR::Opnd* src1 = instr->GetSrc1();
    IR::Opnd* src2 = instr->GetSrc2();
    IR::Opnd* temp1, *temp2, *temp3;
    Assert(dst->IsRegOpnd() && dst->IsSimd128());
    Assert(src1->IsRegOpnd() && src1->IsSimd128());
    Assert(src2->IsRegOpnd() && src2->IsSimd128());

    temp1 = IR::RegOpnd::New(src1->GetType(), m_func);
    temp2 = IR::RegOpnd::New(src1->GetType(), m_func);
    temp3 = IR::RegOpnd::New(src1->GetType(), m_func);

    // temp1 = PMULUDQ src1, src2
    pInstr = IR::Instr::New(Js::OpCode::PMULUDQ, temp1, src1, src2, m_func);
    instr->InsertBefore(pInstr);
    //MakeDstEquSrc1(pInstr);
    Legalize(pInstr);

    // temp2 = PSLRD src1, 0x4
    pInstr = IR::Instr::New(Js::OpCode::PSRLDQ, temp2, src1, IR::IntConstOpnd::New(TySize[TyInt32], TyInt8, m_func, true), m_func);
    instr->InsertBefore(pInstr);
    //MakeDstEquSrc1(pInstr);
    Legalize(pInstr);

    // temp3 = PSLRD src2, 0x4
    pInstr = IR::Instr::New(Js::OpCode::PSRLDQ, temp3, src2, IR::IntConstOpnd::New(TySize[TyInt32], TyInt8, m_func, true), m_func);
    instr->InsertBefore(pInstr);
    //MakeDstEquSrc1(pInstr);
    Legalize(pInstr);

    // temp2 = PMULUDQ temp2, temp3
    pInstr = IR::Instr::New(Js::OpCode::PMULUDQ, temp2, temp2, temp3, m_func);
    instr->InsertBefore(pInstr);
    Legalize(pInstr);

    //PSHUFD temp1, temp1, 0x8
    instr->InsertBefore(IR::Instr::New(Js::OpCode::PSHUFD, temp1, temp1, IR::IntConstOpnd::New( 8 /*b00001000*/, TyInt8, m_func, true), m_func));

    //PSHUFD temp2, temp2, 0x8
    instr->InsertBefore(IR::Instr::New(Js::OpCode::PSHUFD, temp2, temp2, IR::IntConstOpnd::New(8 /*b00001000*/, TyInt8, m_func, true), m_func));

    // PUNPCKLDQ dst, temp1, temp2
    pInstr = IR::Instr::New(Js::OpCode::PUNPCKLDQ, dst, temp1, temp2, m_func);
    instr->InsertBefore(pInstr);
    Legalize(pInstr);


    pInstr = instr->m_prev;
    instr->Remove();
    return pInstr;
}

IR::Instr* LowererMD::SIMD128LowerReplaceLane(IR::Instr* instr)
{
    SList<IR::Opnd*> *args = Simd128GetExtendedArgs(instr);

    int lane = 0, byteWidth = 0;

    IR::Opnd *dst  = args->Pop();
    IR::Opnd *src1 = args->Pop();
    IR::Opnd *src2 = args->Pop();
    IR::Opnd *src3 = args->Pop();

    Assert(dst->IsSimd128() && src1->IsSimd128());
    IRType type = dst->GetType();
    lane = src2->AsIntConstOpnd()->AsInt32();

    IR::Opnd* laneValue = EnregisterIntConst(instr, src3);

    switch (instr->m_opcode)
    {
    case Js::OpCode::Simd128_ReplaceLane_I4:
        byteWidth = TySize[TyInt32];
        break;
    case Js::OpCode::Simd128_ReplaceLane_F4:
        byteWidth = TySize[TyFloat32];
        break;
    default:
        Assert(UNREACHED);
    }

    // MOVAPS dst, src1
    instr->InsertBefore(IR::Instr::New(Js::OpCode::MOVAPS, dst, src1, m_func));
    if (byteWidth == TySize[TyFloat32])
    {
        if (laneValue->GetType() == TyInt32)
        {
            IR::RegOpnd *tempReg = IR::RegOpnd::New(TyFloat32, m_func); //mov intval to xmm
            //MOVD
            instr->InsertBefore(IR::Instr::New(Js::OpCode::MOVD, tempReg, laneValue, m_func));
            laneValue = tempReg;
        }
        Assert(laneValue->GetType() == TyFloat32);
        if (lane == 0)
        {
            // MOVSS for both TyFloat32 and TyInt32. MOVD zeroes upper bits.
            instr->InsertBefore(IR::Instr::New(Js::OpCode::MOVSS, dst, laneValue, m_func));
        }
        else if (lane == 2)
        {
            IR::RegOpnd *tmp = IR::RegOpnd::New(type, m_func);
            instr->InsertBefore(IR::Instr::New(Js::OpCode::MOVHLPS, tmp, dst, m_func));
            instr->InsertBefore(IR::Instr::New(Js::OpCode::MOVSS, tmp, laneValue, m_func));
            instr->InsertBefore(IR::Instr::New(Js::OpCode::MOVLHPS, dst, tmp, m_func));
        }
        else
        {
            Assert(lane == 1 || lane == 3);
            uint8 shufMask = 0xE4; // 11 10 01 00
            shufMask |= lane;      // 11 10 01 id
            shufMask &= ~(0x03 << (lane << 1)); // set 2 bits corresponding to lane index to 00

            // SHUFPS dst, dst, shufMask
            instr->InsertBefore(IR::Instr::New(Js::OpCode::SHUFPS, dst, dst, IR::IntConstOpnd::New(shufMask, TyInt8, m_func, true), m_func));

            // MOVSS dst, value
            instr->InsertBefore(IR::Instr::New(Js::OpCode::MOVSS, dst, laneValue, m_func));

            // SHUFPS dst, dst, shufMask
            instr->InsertBefore(IR::Instr::New(Js::OpCode::SHUFPS, dst, dst, IR::IntConstOpnd::New(shufMask, TyInt8, m_func, true), m_func));
        }
    }
    IR::Instr* prevInstr = instr->m_prev;
    instr->Remove();
    return prevInstr;
}

IR::Instr* LowererMD::Simd128LowerShuffle(IR::Instr* instr)
{
    Js::OpCode shufOpcode = Js::OpCode::SHUFPS;
    Js::OpCode irOpcode = instr->m_opcode;
    bool isShuffle = false;

    SList<IR::Opnd*> *args = Simd128GetExtendedArgs(instr);

    IR::Opnd *dst = args->Pop();
    IR::Opnd *srcs[6] = { nullptr, nullptr, nullptr, nullptr, nullptr, nullptr };

    int i = 0;
    while (!args->Empty() && i < 6)
    {
        srcs[i++] = args->Pop();
    }

    int8 shufMask = 0;
    int lane0 = 0, lane1 = 0, lane2 = 0, lane3 = 0;
    IR::Instr *pInstr = instr->m_prev;

    Assert(dst->IsSimd128() && srcs[0] && srcs[0]->IsSimd128());

    // globOpt will type-spec if all lane indices are constants, and within range constraints to match a single SSE instruction
    if (irOpcode == Js::OpCode::Simd128_Swizzle_I4 ||
        irOpcode == Js::OpCode::Simd128_Swizzle_F4 ||
        irOpcode == Js::OpCode::Simd128_Swizzle_D2)
    {
        isShuffle = false;

        AssertMsg(srcs[1] && srcs[1]->IsIntConstOpnd() &&
            srcs[2] && srcs[2]->IsIntConstOpnd() &&
            (irOpcode == Js::OpCode::Simd128_Swizzle_D2 || (srcs[3] && srcs[3]->IsIntConstOpnd())) &&
            (irOpcode == Js::OpCode::Simd128_Swizzle_D2 || (srcs[4] && srcs[4]->IsIntConstOpnd())), "Type-specialized swizzle is supported only with constant lane indices");

        if (irOpcode == Js::OpCode::Simd128_Swizzle_D2)
        {
            lane0 = srcs[1]->AsIntConstOpnd()->AsInt32();
            lane1 = srcs[2]->AsIntConstOpnd()->AsInt32();
            Assert(lane0 >= 0 && lane0 < 2);
            Assert(lane1 >= 0 && lane1 < 2);
            shufMask = (int8)((lane1 << 1) | lane0);
        }
        else
        {
            AnalysisAssert(srcs[3] != nullptr && srcs[4] != nullptr);
            lane0 = srcs[1]->AsIntConstOpnd()->AsInt32();
            lane1 = srcs[2]->AsIntConstOpnd()->AsInt32();
            lane2 = srcs[3]->AsIntConstOpnd()->AsInt32();
            lane3 = srcs[4]->AsIntConstOpnd()->AsInt32();
            Assert(lane1 >= 0 && lane1 < 4);
            Assert(lane2 >= 0 && lane2 < 4);
            Assert(lane2 >= 0 && lane2 < 4);
            Assert(lane3 >= 0 && lane3 < 4);
            shufMask = (int8)((lane3 << 6) | (lane2 << 4) | (lane1 << 2) | lane0);
        }
    }
    else if (irOpcode == Js::OpCode::Simd128_Shuffle_I4 ||
        irOpcode == Js::OpCode::Simd128_Shuffle_F4 ||
        irOpcode == Js::OpCode::Simd128_Shuffle_D2)
    {
        isShuffle = true;
        Assert(srcs[1] && srcs[1]->IsSimd128());

        AssertMsg(srcs[2] && srcs[2]->IsIntConstOpnd() &&
            srcs[3] && srcs[3]->IsIntConstOpnd() &&
            (irOpcode == Js::OpCode::Simd128_Shuffle_D2 || (srcs[4] && srcs[4]->IsIntConstOpnd())) &&
            (irOpcode == Js::OpCode::Simd128_Shuffle_D2 || (srcs[5] && srcs[5]->IsIntConstOpnd())), "Type-specialized shuffle is supported only with constant lane indices");

        if (irOpcode == Js::OpCode::Simd128_Shuffle_D2)
        {
            Assert(srcs[2]->IsIntConstOpnd() && srcs[3]->IsIntConstOpnd());

            lane0 = srcs[2]->AsIntConstOpnd()->AsInt32();
            lane1 = srcs[3]->AsIntConstOpnd()->AsInt32() - 2;
            Assert(lane0 >= 0 && lane0 < 2);
            Assert(lane1 >= 0 && lane1 < 2);
            shufMask = (int8)((lane1 << 1) | lane0);
        }
        else
        {
            AnalysisAssert(srcs[4] != nullptr && srcs[5] != nullptr);
            lane0 = srcs[2]->AsIntConstOpnd()->AsInt32();
            lane1 = srcs[3]->AsIntConstOpnd()->AsInt32();
            lane2 = srcs[4]->AsIntConstOpnd()->AsInt32() - 4;
            lane3 = srcs[5]->AsIntConstOpnd()->AsInt32() - 4;
            Assert(lane0 >= 0 && lane0 < 4);
            Assert(lane1 >= 0 && lane1 < 4);
            Assert(lane2 >= 0 && lane2 < 4);
            Assert(lane3 >= 0 && lane3 < 4);
            shufMask = (int8)((lane3 << 6) | (lane2 << 4) | (lane1 << 2) | lane0);
        }
    }
    else
    {
        Assert(UNREACHED);
    }

    if (instr->m_opcode == Js::OpCode::Simd128_Swizzle_D2 || instr->m_opcode == Js::OpCode::Simd128_Shuffle_D2)
    {
        shufOpcode = Js::OpCode::SHUFPD;
    }

    // Lower shuffle/swizzle

    instr->m_opcode = shufOpcode;
    instr->SetDst(dst);

    // MOVAPS dst, src1
    instr->InsertBefore(IR::Instr::New(Js::OpCode::MOVAPS, dst, srcs[0], m_func));
    if (isShuffle)
    {
        // SHUF dst, src2, imm8
        instr->SetSrc1(srcs[1]);
    }
    else
    {
        // SHUF dst, dst, imm8
        instr->SetSrc1(dst);
    }
    instr->SetSrc2(IR::IntConstOpnd::New((IntConstType)shufMask, TyInt8, m_func, true));

    return pInstr;
}

IR::Instr* LowererMD::Simd128LowerLoadElem(IR::Instr *instr)
{
    Assert(instr->m_opcode == Js::OpCode::Simd128_LdArr_I4 ||
        instr->m_opcode == Js::OpCode::Simd128_LdArr_F4 ||
        instr->m_opcode == Js::OpCode::Simd128_LdArr_D2 ||
        instr->m_opcode == Js::OpCode::Simd128_LdArrConst_I4 ||
        instr->m_opcode == Js::OpCode::Simd128_LdArrConst_F4 ||
        instr->m_opcode == Js::OpCode::Simd128_LdArrConst_D2
        );

    IR::Instr * instrPrev = instr->m_prev;
    IR::RegOpnd * indexOpnd = instr->GetSrc1()->AsIndirOpnd()->GetIndexOpnd();
    IR::RegOpnd * baseOpnd = instr->GetSrc1()->AsIndirOpnd()->GetBaseOpnd();
    IR::Opnd * dst = instr->GetDst();
    IR::Opnd * src1 = instr->GetSrc1();
    IR::Opnd * src2 = instr->GetSrc2();
    ValueType arrType = baseOpnd->GetValueType();
    uint8 dataWidth = instr->dataWidth;

    // Type-specialized.
    Assert(dst->IsSimd128() && src1->IsSimd128() && src2->GetType() == TyUint32);

    IR::Instr * done;
    if (indexOpnd ||  (((uint32)src1->AsIndirOpnd()->GetOffset() + dataWidth) > 0x1000000 /* 16 MB */))
    {
        // CMP indexOpnd, src2(arrSize)
        // JA $helper
        // JMP $load
        // $helper:
        // Throw RangeError
        // JMP $done
        // $load:
        // MOVUPS dst, src1([arrayBuffer + indexOpnd]) // or other based on data width
        // $done:

        uint32 bpe = 1;
        switch (arrType.GetObjectType())
        {
        case ObjectType::Int8Array:
        case ObjectType::Uint8Array:
            break;
        case ObjectType::Int16Array:
        case ObjectType::Uint16Array:
            bpe = 2;
            break;
        case ObjectType::Int32Array:
        case ObjectType::Uint32Array:
        case ObjectType::Float32Array:
            bpe = 4;
            break;
        case ObjectType::Float64Array:
            bpe = 8;
            break;
        default:
            Assert(UNREACHED);
        }

        // bound check and helper
        done = this->lowererMDArch.LowerAsmJsLdElemHelper(instr, true, bpe != dataWidth);
    }
    else
    {
        // Reaching here means:
        // We have a constant index, and either
        // (1) constant heap or (2) variable heap with constant index < 16MB.
        // Case (1) requires static bound check. Case (2) means we are always in bound.

        instr->UnlinkDst();

        // this can happen in cases where globopt props a constant access which was not known at bytecodegen time or when heap is non-constant

        if (src2->IsIntConstOpnd() && ((uint32)src1->AsIndirOpnd()->GetOffset() + dataWidth > src2->AsIntConstOpnd()->AsUint32()))
        {
            m_lowerer->GenerateRuntimeError(instr, JSERR_ArgumentOutOfRange, IR::HelperOp_RuntimeRangeError);
            instr->FreeSrc1();
            instr->FreeSrc2();
            instr->Remove();
            return instrPrev;
        }
        instr->FreeSrc2();
        done = instr;
    }

    IR::Instr *newInstr = nullptr;
    switch (dataWidth)
    {
    case 16:
        // MOVUPS dst, src1([arrayBuffer + indexOpnd])
        newInstr = IR::Instr::New(LowererMDArch::GetAssignOp(src1->GetType()), dst, src1, m_func);
        instr->InsertBefore(newInstr);
        Legalize(newInstr);
        break;
    case 12:
    {
       IR::RegOpnd *temp = IR::RegOpnd::New(src1->GetType(), m_func);

       // MOVSD dst, src1([arrayBuffer + indexOpnd])
       newInstr = IR::Instr::New(Js::OpCode::MOVSD, dst, src1, m_func);
       instr->InsertBefore(newInstr);
       Legalize(newInstr);

       // MOVSS temp, src1([arrayBuffer + indexOpnd + 8])
       newInstr = IR::Instr::New(Js::OpCode::MOVSS, temp, src1, m_func);
       instr->InsertBefore(newInstr);
       newInstr->GetSrc1()->AsIndirOpnd()->SetOffset(src1->AsIndirOpnd()->GetOffset() + 8, true);
       Legalize(newInstr);

       // PSLLDQ temp, 0x08
       instr->InsertBefore(IR::Instr::New(Js::OpCode::PSLLDQ, temp, temp, IR::IntConstOpnd::New(8, TyInt8, m_func, true), m_func));

       // ORPS dst, temp
       newInstr = IR::Instr::New(Js::OpCode::ORPS, dst, dst, temp, m_func);
       instr->InsertBefore(newInstr);
       Legalize(newInstr);

       break;
    }

    case 8:
        // MOVSD dst, src1([arrayBuffer + indexOpnd])
        newInstr = IR::Instr::New(Js::OpCode::MOVSD, dst, src1, m_func);
        instr->InsertBefore(newInstr);
        Legalize(newInstr);
        break;
    case 4:
        // MOVSS dst, src1([arrayBuffer + indexOpnd])
        newInstr = IR::Instr::New(Js::OpCode::MOVSS, dst, src1, m_func);
        instr->InsertBefore(newInstr);
        Legalize(newInstr);
        break;
    default:
        Assume(UNREACHED);
    }

    instr->Remove();
    return instrPrev;
}

IR::Instr* LowererMD::Simd128LowerStoreElem(IR::Instr *instr)
{

    Assert(instr->m_opcode == Js::OpCode::Simd128_StArr_I4 ||
        instr->m_opcode == Js::OpCode::Simd128_StArr_F4 ||
        instr->m_opcode == Js::OpCode::Simd128_StArr_D2 ||
        instr->m_opcode == Js::OpCode::Simd128_StArrConst_I4 ||
        instr->m_opcode == Js::OpCode::Simd128_StArrConst_F4 ||
        instr->m_opcode == Js::OpCode::Simd128_StArrConst_D2
        );

    IR::Instr * instrPrev = instr->m_prev;
    IR::RegOpnd * indexOpnd = instr->GetDst()->AsIndirOpnd()->GetIndexOpnd();
    IR::RegOpnd * baseOpnd = instr->GetDst()->AsIndirOpnd()->GetBaseOpnd();
    IR::Opnd * dst = instr->GetDst();
    IR::Opnd * src1 = instr->GetSrc1();
    IR::Opnd * src2 = instr->GetSrc2();
    ValueType arrType = baseOpnd->GetValueType();
    uint8 dataWidth = instr->dataWidth;

    // Type-specialized.
    Assert(dst->IsSimd128() && src1->IsSimd128() && src2->GetType() == TyUint32);

    IR::Instr * done;
    bool doStore = true;
    if (indexOpnd || ((uint32)dst->AsIndirOpnd()->GetOffset() + dataWidth > 0x1000000))
    {
        // CMP indexOpnd, src2(arrSize)
        // JA $helper
        // JMP $store
        // $helper:
        // Throw RangeError
        // JMP $done
        // $store:
        // MOV dst([arrayBuffer + indexOpnd]), src1
        // $done:

        uint32 bpe = 1;
        switch (arrType.GetObjectType())
        {
        case ObjectType::Int8Array:
        case ObjectType::Uint8Array:
            break;
        case ObjectType::Int16Array:
        case ObjectType::Uint16Array:
            bpe = 2;
            break;
        case ObjectType::Int32Array:
        case ObjectType::Uint32Array:
        case ObjectType::Float32Array:
            bpe = 4;
            break;
        case ObjectType::Float64Array:
            bpe = 8;
            break;
        default:
            Assert(UNREACHED);
        }
        done = this->lowererMDArch.LowerAsmJsStElemHelper(instr, true, bpe != dataWidth);
    }
    else
    {
        instr->UnlinkDst();
        instr->UnlinkSrc1();

        // we might have a constant index if globopt propped a constant store. we can ahead of time check if it is in-bounds
        if (src2->IsIntConstOpnd() && ((uint32)dst->AsIndirOpnd()->GetOffset() + dataWidth > src2->AsIntConstOpnd()->AsUint32()))
        {
            m_lowerer->GenerateRuntimeError(instr, JSERR_ArgumentOutOfRange, IR::HelperOp_RuntimeRangeError);

            doStore = false;

            src1->Free(m_func);
            dst->Free(m_func);
        }
        done = instr;
        instr->FreeSrc2();
    }
    if (doStore)
    {
        switch (dataWidth)
        {
        case 16:
            // MOVUPS dst([arrayBuffer + indexOpnd]), src1
            instr->InsertBefore(IR::Instr::New(LowererMDArch::GetAssignOp(src1->GetType()), dst, src1, m_func));
            break;
        case 12:
        {
                   IR::RegOpnd *temp = IR::RegOpnd::New(src1->GetType(), m_func);
                   IR::Instr *movss;
                   // MOVAPS temp, src
                   instr->InsertBefore(IR::Instr::New(Js::OpCode::MOVAPS, temp, src1, m_func));
                   // MOVSD dst([arrayBuffer + indexOpnd]), temp
                   instr->InsertBefore(IR::Instr::New(Js::OpCode::MOVSD, dst, temp, m_func));
                   // PSRLDQ temp, 0x08
                   instr->InsertBefore(IR::Instr::New(Js::OpCode::PSRLDQ, temp, temp, IR::IntConstOpnd::New(8, TyInt8, m_func, true), m_func));
                   // MOVSS dst([arrayBuffer + indexOpnd + 8]), temp
                   movss = IR::Instr::New(Js::OpCode::MOVSS, dst, temp, m_func);
                   instr->InsertBefore(movss);
                   movss->GetDst()->AsIndirOpnd()->SetOffset(dst->AsIndirOpnd()->GetOffset() + 8, true);
                   break;
        }
        case 8:
            // MOVSD dst([arrayBuffer + indexOpnd]), src1
            instr->InsertBefore(IR::Instr::New(Js::OpCode::MOVSD, dst, src1, m_func));
            break;
        case 4:
            // MOVSS dst([arrayBuffer + indexOpnd]), src1
            instr->InsertBefore(IR::Instr::New(Js::OpCode::MOVSS, dst, src1, m_func));
            break;
        default:;
            Assume(UNREACHED);
        }
    }
    instr->Remove();
    return instrPrev;
}

// Builds args list <dst, src1, src2, src3 ..>
SList<IR::Opnd*> * LowererMD::Simd128GetExtendedArgs(IR::Instr *instr)
{
    SList<IR::Opnd*> * args = JitAnew(m_lowerer->m_alloc, SList<IR::Opnd*>, m_lowerer->m_alloc);
    IR::Instr *pInstr = instr;
    IR::Opnd *dst, *src1, *src2;

    dst = src1 = src2 = nullptr;

    if (pInstr->GetDst())
    {
        dst = pInstr->UnlinkDst();
    }


    src1 = pInstr->UnlinkSrc1();
    Assert(src1->GetStackSym()->IsSingleDef());


    pInstr = src1->GetStackSym()->GetInstrDef();

    while (pInstr && pInstr->m_opcode == Js::OpCode::ExtendArg_A)
    {
        Assert(pInstr->GetSrc1());
        src1 = pInstr->GetSrc1()->Copy(this->m_func);
        if (src1->IsRegOpnd())
        {
            this->m_lowerer->addToLiveOnBackEdgeSyms->Set(src1->AsRegOpnd()->m_sym->m_id);
        }
        args->Push(src1);

        if (pInstr->GetSrc2())
        {
            src2 = pInstr->GetSrc2();
            Assert(src2->GetStackSym()->IsSingleDef());
            pInstr = src2->GetStackSym()->GetInstrDef();
        }
        else
        {
            pInstr = nullptr;
        }

    }
    args->Push(dst);
    Assert(args->Count() > 3);
    return args;
}

IR::Opnd* LowererMD::EnregisterIntConst(IR::Instr* instr, IR::Opnd *constOpnd)
{
    if (constOpnd->IsRegOpnd())
    {
        // already a register
        return constOpnd;
    }
    Assert(constOpnd->GetType() == TyInt32);
    IR::RegOpnd *tempReg = IR::RegOpnd::New(TyInt32, m_func);

    // MOV tempReg, constOpnd
    instr->InsertBefore(IR::Instr::New(Js::OpCode::MOV, tempReg, constOpnd, m_func));
    return tempReg;
}

void LowererMD::Simd128InitOpcodeMap()
{
    m_simd128OpCodesMap = JitAnewArrayZ(m_lowerer->m_alloc, Js::OpCode, Js::Simd128OpcodeCount());

    // All simd ops should be contiguous for this mapping to work
    Assert(Js::OpCode::Simd128_End + (Js::OpCode) 1 == Js::OpCode::Simd128_Start_Extend);

    SET_SIMDOPCODE(Simd128_FromFloat64x2_I4     , CVTTPD2DQ);
    SET_SIMDOPCODE(Simd128_FromFloat64x2Bits_I4 , MOVAPS);
    SET_SIMDOPCODE(Simd128_FromFloat32x4_I4     , CVTTPS2DQ);
    SET_SIMDOPCODE(Simd128_FromFloat32x4Bits_I4 , MOVAPS);
    SET_SIMDOPCODE(Simd128_Add_I4               , PADDD);
    SET_SIMDOPCODE(Simd128_Sub_I4               , PSUBD);
    SET_SIMDOPCODE(Simd128_Lt_I4                , PCMPGTD);
    SET_SIMDOPCODE(Simd128_Gt_I4                , PCMPGTD);
    SET_SIMDOPCODE(Simd128_Eq_I4                , PCMPEQD);
    SET_SIMDOPCODE(Simd128_And_I4               , PAND);
    SET_SIMDOPCODE(Simd128_Or_I4                , POR);
    SET_SIMDOPCODE(Simd128_Xor_I4               , XORPS);
    SET_SIMDOPCODE(Simd128_Not_I4               , XORPS);
    SET_SIMDOPCODE(Simd128_LdSignMask_I4        , MOVMSKPS);


    SET_SIMDOPCODE(Simd128_FromFloat64x2_F4      , CVTPD2PS);
    SET_SIMDOPCODE(Simd128_FromFloat64x2Bits_F4  , MOVAPS);
    SET_SIMDOPCODE(Simd128_FromInt32x4_F4        , CVTDQ2PS);
    SET_SIMDOPCODE(Simd128_FromInt32x4Bits_F4    , MOVAPS);
    SET_SIMDOPCODE(Simd128_Abs_F4                , ANDPS);
    SET_SIMDOPCODE(Simd128_Neg_F4                , XORPS);
    SET_SIMDOPCODE(Simd128_Add_F4                , ADDPS);
    SET_SIMDOPCODE(Simd128_Sub_F4                , SUBPS);
    SET_SIMDOPCODE(Simd128_Mul_F4                , MULPS);
    SET_SIMDOPCODE(Simd128_Div_F4                , DIVPS);
    SET_SIMDOPCODE(Simd128_Min_F4                , MINPS);
    SET_SIMDOPCODE(Simd128_Max_F4                , MAXPS);
    SET_SIMDOPCODE(Simd128_Sqrt_F4               , SQRTPS);
    SET_SIMDOPCODE(Simd128_Lt_F4                 , CMPLTPS); // CMPLTPS
    SET_SIMDOPCODE(Simd128_LtEq_F4               , CMPLEPS); // CMPLEPS
    SET_SIMDOPCODE(Simd128_Eq_F4                 , CMPEQPS); // CMPEQPS
    SET_SIMDOPCODE(Simd128_Neq_F4                , CMPNEQPS); // CMPNEQPS
    SET_SIMDOPCODE(Simd128_Gt_F4                 , CMPLTPS); // CMPLTPS (swap srcs)
    SET_SIMDOPCODE(Simd128_GtEq_F4               , CMPLEPS); // CMPLEPS (swap srcs)
    SET_SIMDOPCODE(Simd128_And_F4                , ANDPS);
    SET_SIMDOPCODE(Simd128_Or_F4                 , ORPS);
    SET_SIMDOPCODE(Simd128_Xor_F4                , XORPS );
    SET_SIMDOPCODE(Simd128_Not_F4                , XORPS );
    SET_SIMDOPCODE(Simd128_LdSignMask_F4         , MOVMSKPS );


    SET_SIMDOPCODE(Simd128_FromFloat32x4_D2     , CVTPS2PD);
    SET_SIMDOPCODE(Simd128_FromFloat32x4Bits_D2 , MOVAPS);
    SET_SIMDOPCODE(Simd128_FromInt32x4_D2       , CVTDQ2PD);
    SET_SIMDOPCODE(Simd128_FromInt32x4Bits_D2   , MOVAPS);
    SET_SIMDOPCODE(Simd128_Neg_D2               , XORPS);
    SET_SIMDOPCODE(Simd128_Add_D2               , ADDPD);
    SET_SIMDOPCODE(Simd128_Abs_D2               , ANDPD);
    SET_SIMDOPCODE(Simd128_Sub_D2               , SUBPD);
    SET_SIMDOPCODE(Simd128_Mul_D2               , MULPD);
    SET_SIMDOPCODE(Simd128_Div_D2               , DIVPD);
    SET_SIMDOPCODE(Simd128_Min_D2               , MINPD);
    SET_SIMDOPCODE(Simd128_Max_D2               , MAXPD);
    SET_SIMDOPCODE(Simd128_Sqrt_D2              , SQRTPD);
    SET_SIMDOPCODE(Simd128_Lt_D2                , CMPLTPD); // CMPLTPD
    SET_SIMDOPCODE(Simd128_LtEq_D2              , CMPLEPD); // CMPLEPD
    SET_SIMDOPCODE(Simd128_Eq_D2                , CMPEQPD); // CMPEQPD
    SET_SIMDOPCODE(Simd128_Neq_D2               , CMPNEQPD); // CMPNEQPD
    SET_SIMDOPCODE(Simd128_Gt_D2                , CMPLTPD); // CMPLTPD (swap srcs)
    SET_SIMDOPCODE(Simd128_GtEq_D2              , CMPLEPD); // CMPLEPD (swap srcs)
    SET_SIMDOPCODE(Simd128_LdSignMask_D2        , MOVMSKPD);

}
#undef SIMD_SETOPCODE
#undef SIMD_GETOPCODE

// FromVar
void LowererMD::GenerateCheckedSimdLoad(IR::Instr * instr)
{
    Assert(instr->m_opcode == Js::OpCode::FromVar);
    Assert(instr->GetSrc1()->GetType() == TyVar);
    Assert(IRType_IsSimd128(instr->GetDst()->GetType()));

    bool checkRequired = instr->HasBailOutInfo();
    IR::LabelInstr * labelHelper = nullptr, * labelDone = nullptr;
    IR::Instr * insertInstr = instr, * newInstr;
    IR::RegOpnd * src = instr->GetSrc1()->AsRegOpnd(), * dst = instr->GetDst()->AsRegOpnd();
    Assert(!checkRequired || instr->GetBailOutKind() == IR::BailOutSimd128F4Only || instr->GetBailOutKind() == IR::BailOutSimd128I4Only);


    if (checkRequired)
    {
        labelHelper = IR::LabelInstr::New(Js::OpCode::Label, this->m_func, true);
        labelDone = IR::LabelInstr::New(Js::OpCode::Label, this->m_func);
        instr->InsertBefore(labelHelper);
        instr->InsertAfter(labelDone);
        insertInstr = labelHelper;

        GenerateObjectTest(instr->GetSrc1(), insertInstr, labelHelper);

        newInstr = IR::Instr::New(Js::OpCode::CMP, instr->m_func);
        newInstr->SetSrc1(IR::IndirOpnd::New(instr->GetSrc1()->AsRegOpnd(), 0, TyMachPtr, instr->m_func));
        newInstr->SetSrc2(m_lowerer->LoadVTableValueOpnd(instr, dst->GetType() == TySimd128F4 ? VTableValue::VtableSimd128F4 : VTableValue::VtableSimd128I4));
        insertInstr->InsertBefore(newInstr);
        Legalize(newInstr);
        insertInstr->InsertBefore(IR::BranchInstr::New(Js::OpCode::JNE, labelHelper, this->m_func));
        instr->UnlinkSrc1();
        instr->UnlinkDst();
        this->m_lowerer->GenerateBailOut(instr);

    }
    size_t valueOffset = dst->GetType() == TySimd128F4 ? Js::JavascriptSIMDFloat32x4::GetOffsetOfValue() : Js::JavascriptSIMDInt32x4::GetOffsetOfValue();
    Assert(valueOffset < INT_MAX);
    newInstr = IR::Instr::New(Js::OpCode::MOVUPS, dst, IR::IndirOpnd::New(src, static_cast<int>(valueOffset), dst->GetType(), this->m_func), this->m_func);
    insertInstr->InsertBefore(newInstr);

    insertInstr->InsertBefore(IR::BranchInstr::New(Js::OpCode::JMP, labelDone, this->m_func));
    // FromVar is converted to BailOut call. Don't remove.
}

// ToVar
void LowererMD::GenerateSimdStore(IR::Instr * instr)
{
    IR::RegOpnd *dst, *src;
    IRType type;
    dst = instr->GetDst()->AsRegOpnd();
    src = instr->GetSrc1()->AsRegOpnd();
    type = src->GetType();

    this->m_lowerer->LoadScriptContext(instr);
    IR::Instr * instrCall = IR::Instr::New(Js::OpCode::CALL, instr->GetDst(),
        IR::HelperCallOpnd::New(type == TySimd128F4 ? IR::HelperAllocUninitializedSimdF4 : IR::HelperAllocUninitializedSimdI4, this->m_func), this->m_func);
    instr->InsertBefore(instrCall);
    this->lowererMDArch.LowerCall(instrCall, 0);

    IR::Opnd * valDst;
    if (type == TySimd128F4)
    {
        valDst = IR::IndirOpnd::New(dst, (int32)Js::JavascriptSIMDFloat32x4::GetOffsetOfValue(), TySimd128F4, this->m_func);
    }
    else
    {
        valDst = IR::IndirOpnd::New(dst, (int32)Js::JavascriptSIMDInt32x4::GetOffsetOfValue(), TySimd128I4, this->m_func);
    }

    instr->InsertBefore(IR::Instr::New(Js::OpCode::MOVUPS, valDst, src, this->m_func));
    instr->Remove();
}
