//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
// SIMD_JS
// GlobOpt bits related to SIMD.js

#include "Backend.h"

/*
Handles all Simd128 type-spec of an instr, if possible.
*/
bool
GlobOpt::TypeSpecializeSimd128(
IR::Instr *instr,
Value **pSrc1Val,
Value **pSrc2Val,
Value **pDstVal
)
{
    if (func->m_workItem->GetFunctionBody()->GetIsAsmjsMode() || SIMD128_TYPE_SPEC_FLAG == false)
    {
        // no type-spec for ASMJS code or flag is off.
        return false;
    }

    switch (instr->m_opcode)
    {
    case Js::OpCode::ArgOut_A_InlineBuiltIn:
        if (instr->GetSrc1()->IsRegOpnd())
        {
            StackSym *sym = instr->GetSrc1()->AsRegOpnd()->m_sym;
            if (IsSimd128TypeSpecialized(sym, this->currentBlock))
            {
                ValueType valueType = (*pSrc1Val)->GetValueInfo()->Type();
                Assert(valueType.IsSimd128());
                ToTypeSpecUse(instr, instr->GetSrc1(), this->currentBlock, *pSrc1Val, nullptr, GetIRTypeFromValueType(valueType), GetBailOutKindFromValueType(valueType));

                return true;
            }
        }
        return false;

    case Js::OpCode::Ld_A:
        if (instr->GetSrc1()->IsRegOpnd())
        {
            StackSym *sym = instr->GetSrc1()->AsRegOpnd()->m_sym;
            IRType type = TyIllegal;
            if (IsSimd128F4TypeSpecialized(sym, this->currentBlock))
            {
                type = TySimd128F4;
                sym->GetSimd128F4EquivSym(func);
            }
            else if (IsSimd128I4TypeSpecialized(sym, this->currentBlock))
            {
                type = TySimd128I4;
                sym->GetSimd128I4EquivSym(func);
            }
            else
            {
                return false;
            }

            ToTypeSpecUse(instr, instr->GetSrc1(), this->currentBlock, *pSrc1Val, nullptr, type, IR::BailOutSimd128F4Only /*not used for Ld_A*/);
            TypeSpecializeSimd128Dst(type, instr, *pSrc1Val, *pSrc1Val, pDstVal);
            return true;
        }

        return false;

    case Js::OpCode::ExtendArg_A:

        if (Simd128DoTypeSpec(instr, *pSrc1Val, *pSrc2Val, *pDstVal))
        {
            Assert(instr->m_opcode == Js::OpCode::ExtendArg_A);
            Assert(instr->GetDst()->GetType() == TyVar);
            ValueType valueType = instr->GetDst()->GetValueType();

            // Type-spec src1 only based on dst type. Dst type is set by the inliner based on func signature.
            ToTypeSpecUse(instr, instr->GetSrc1(), this->currentBlock, *pSrc1Val, nullptr, GetIRTypeFromValueType(valueType), GetBailOutKindFromValueType(valueType), true /*lossy*/);
            ToVarRegOpnd(instr->GetDst()->AsRegOpnd(), this->currentBlock);
            return true;
        }
        return false;
    }

    if (!Js::IsSimd128Opcode(instr->m_opcode))
    {
        return false;
    }

    // Simd instr
    if (Simd128DoTypeSpec(instr, *pSrc1Val, *pSrc2Val, *pDstVal))
    {
        ThreadContext::SimdFuncSignature simdFuncSignature;
        instr->m_func->GetScriptContext()->GetThreadContext()->GetSimdFuncSignatureFromOpcode(instr->m_opcode, simdFuncSignature);
        // type-spec logic

        // For op with ExtendArg. All sources are already type-specialized, just type-specialize dst
        if (simdFuncSignature.argCount <= 2)
        {
            Assert(instr->GetSrc1());
            ToTypeSpecUse(instr, instr->GetSrc1(), this->currentBlock, *pSrc1Val, nullptr, GetIRTypeFromValueType(simdFuncSignature.args[0]), GetBailOutKindFromValueType(simdFuncSignature.args[0]));

            if (instr->GetSrc2())
            {
                ToTypeSpecUse(instr, instr->GetSrc2(), this->currentBlock, *pSrc2Val, nullptr, GetIRTypeFromValueType(simdFuncSignature.args[1]), GetBailOutKindFromValueType(simdFuncSignature.args[1]));
            }
        }
        if (instr->GetDst())
        {
            TypeSpecializeSimd128Dst(GetIRTypeFromValueType(simdFuncSignature.returnType), instr, nullptr, *pSrc1Val, pDstVal);
        }
        return true;
    }
    else
    {
        // We didn't type-spec
        if (!IsLoopPrePass())
        {
            // Emit bailout if not loop prepass.
            // The inliner inserts bytecodeUses of original args after the instruction. Bailout is safe.
            IR::Instr * bailoutInstr = IR::BailOutInstr::New(Js::OpCode::BailOnNoSimdTypeSpec, IR::BailOutNoSimdTypeSpec, instr, this->func);
            bailoutInstr->SetByteCodeOffset(instr);
            instr->InsertAfter(bailoutInstr);

            instr->m_opcode = Js::OpCode::Nop;
            if (instr->GetSrc1())
            {
                instr->FreeSrc1();
                if (instr->GetSrc2())
                {
                    instr->FreeSrc2();
                }
            }
            if (instr->GetDst())
            {
                instr->FreeDst();
            }

            if (this->byteCodeUses)
            {
                // All inlined SIMD ops have jitOptimizedReg srcs
                Assert(this->byteCodeUses->IsEmpty());
                JitAdelete(this->alloc, this->byteCodeUses);
                this->byteCodeUses = nullptr;
            }
            RemoveCodeAfterNoFallthroughInstr(bailoutInstr);
            return true;
        }
    }
    return false;
}

bool
GlobOpt::Simd128DoTypeSpec(IR::Instr *instr, const Value *src1Val, const Value *src2Val, const Value *dstVal)
{
    bool doTypeSpec = true;

    // TODO: Some operations require additional opnd constraints (e.g. shuffle/swizzle).
    if (Js::IsSimd128Opcode(instr->m_opcode))
    {
        ThreadContext::SimdFuncSignature simdFuncSignature;
        instr->m_func->GetScriptContext()->GetThreadContext()->GetSimdFuncSignatureFromOpcode(instr->m_opcode, simdFuncSignature);
        if (!simdFuncSignature.valid)
        {
            // not implemented yet.
            return false;
        }

        const uint argCount = simdFuncSignature.argCount;
        switch (argCount)
        {
        case 2:
            Assert(src2Val);
            doTypeSpec = doTypeSpec && Simd128CanTypeSpecOpnd(src2Val->GetValueInfo()->Type(), simdFuncSignature.args[1]);
            // fall-through
        case 1:
            Assert(src1Val);
            doTypeSpec = doTypeSpec && Simd128CanTypeSpecOpnd(src1Val->GetValueInfo()->Type(), simdFuncSignature.args[0]);
            break;
        default:
        {
            // extended args
            Assert(argCount > 2);
            // Check if all args have been type specialized.

            int arg = argCount - 1;
            IR::Instr * eaInstr = GetExtendedArg(instr);

            while (arg>=0)
            {
                Assert(eaInstr);
                Assert(eaInstr->m_opcode == Js::OpCode::ExtendArg_A);

                ValueType expectedType = simdFuncSignature.args[arg];
                IR::Opnd * opnd = eaInstr->GetSrc1();
                StackSym * sym = opnd->GetStackSym();

                // In Forward Prepass: Check liveness through liveness bits, not IR type, since in prepass no actual type-spec happens.
                // In the Forward Pass: Check IRType since Sym can be null, because of const prop.
                if (expectedType.IsSimd128Float32x4())
                {
                    if (sym && !IsSimd128F4TypeSpecialized(sym, &currentBlock->globOptData) ||
                        !sym && opnd->GetType() != TySimd128F4)
                    {
                        return false;
                    }
                }
                else if (expectedType.IsSimd128Int32x4())
                {
                    if (sym && !IsSimd128I4TypeSpecialized(sym, &currentBlock->globOptData) ||
                        !sym && opnd->GetType() != TySimd128I4)
                    {
                        return false;
                    }
                }
                else if (expectedType.IsFloat())
                {
                    if (sym && !IsFloat64TypeSpecialized(sym, &currentBlock->globOptData) ||
                        !sym&& opnd->GetType() != TyFloat64)
                    {
                        return false;
                    }

                }
                else if (expectedType.IsInt())
                {
                    if ((sym && !IsInt32TypeSpecialized(sym, &currentBlock->globOptData) && !currentBlock->globOptData.liveLossyInt32Syms->Test(sym->m_id)) ||
                        !sym && opnd->GetType() != TyInt32)
                    {
                        return false;
                    }
                }
                else
                {
                    Assert(UNREACHED);
                }

                eaInstr = GetExtendedArg(instr);
                arg--;
            }
            // all args are type-spec'd
            doTypeSpec = true;
        }
        }
    }
    else
    {
        Assert(instr->m_opcode == Js::OpCode::ExtendArg_A);
        // For ExtendArg, the expected type is encoded in the dst(link) operand.
        doTypeSpec = doTypeSpec && Simd128CanTypeSpecOpnd(src1Val->GetValueInfo()->Type(), instr->GetDst()->GetValueType());
    }
    return doTypeSpec;
}


// We can type spec an opnd if:
// Both profiled/propagated and expected types are not Simd128. e.g. expected type is f64/f32/i32 where there is a conversion logic from the incoming type.
// Opnd type is (Likely) SIMD128 and matches expected type.
// Opnd type is Object. e.g. possibly result of merging different SIMD types.
// Simd128 values merged with Undefined/Null are still specialized.
// Opnd type is LikelyUndefined: we don't have profile info for the operands.

bool GlobOpt::Simd128CanTypeSpecOpnd(const ValueType opndType, ValueType expectedType)
{
    if (!opndType.IsSimd128() && !expectedType.IsSimd128())
    {
        // Non-Simd types can be coerced or we bailout by a FromVar.
        return true;
    }

    // Simd type
    if (opndType.HasBeenNull() || opndType.HasBeenUndefined())
    {
        return false;
    }

    if (
            (opndType.IsLikelyObject() && opndType.ToDefiniteObject() == expectedType) ||
            (opndType.IsLikelyObject() && opndType.GetObjectType() == ObjectType::Object)
       )
    {
        return true;
    }
    return false;
}

IR::Instr * GlobOpt::GetExtendedArg(IR::Instr *instr)
{
    IR::Opnd *src1, *src2;

    src1 = instr->GetSrc1();
    src2 = instr->GetSrc2();

    if (instr->m_opcode == Js::OpCode::ExtendArg_A)
    {
        if (src2)
        {
            // mid chain
            Assert(src2->GetStackSym()->IsSingleDef());
            return src2->GetStackSym()->GetInstrDef();
        }
        else
        {
            // end of chain
            return nullptr;
        }
    }
    else
    {
        // start of chain
        Assert(Js::IsSimd128Opcode(instr->m_opcode));
        Assert(src1);
        Assert(src1->GetStackSym()->IsSingleDef());
        return src1->GetStackSym()->GetInstrDef();
    }
}

IRType GlobOpt::GetIRTypeFromValueType(const ValueType &valueType)
{
    if (valueType.IsFloat())
    {
        return TyFloat64;
    }
    else if (valueType.IsInt())
    {
        return TyInt32;
    }
    else if (valueType.IsSimd128Float32x4())
    {
        return TySimd128F4;
    }
    else
    {
        Assert(valueType.IsSimd128Int32x4());
        return TySimd128I4;
    }
}

ValueType GlobOpt::GetValueTypeFromIRType(const IRType &type)
{
    switch (type)
    {
    case TyInt32:
        return ValueType::GetInt(false);
    case TyFloat64:
        return ValueType::Float;
    case TySimd128F4:
        return ValueType::GetSimd128(ObjectType::Simd128Float32x4);
    case TySimd128I4:
        return ValueType::GetSimd128(ObjectType::Simd128Int32x4);
    default:
        Assert(UNREACHED);

    }
    return ValueType::UninitializedObject;

}

IR::BailOutKind GlobOpt::GetBailOutKindFromValueType(const ValueType &valueType)
{
    if (valueType.IsFloat())
    {
        // if required valueType is Float, then allow coercion from any primitive except String.
        return IR::BailOutPrimitiveButString;
    }
    else if (valueType.IsInt())
    {
        return IR::BailOutIntOnly;
    }
    else if (valueType.IsSimd128Float32x4())
    {
        return IR::BailOutSimd128F4Only;
    }
    else
    {
        Assert(valueType.IsSimd128Int32x4());
        return IR::BailOutSimd128I4Only;
    }
}
