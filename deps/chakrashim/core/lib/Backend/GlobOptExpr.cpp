//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "Backend.h"


uint8 OpCodeToHash[Js::OpCode::Count];
static Js::OpCode HashToOpCode[Js::OpCode::Count];

class CSEInit
{
public:
    // Initializer for OpCodeToHash and HashToOpCode maps.
    CSEInit()
    {
        uint8 hash = 1;

        for (Js::OpCode opcode = (Js::OpCode)0; opcode < Js::OpCode::Count; opcode = (Js::OpCode)(opcode + 1))
        {
            if (Js::OpCodeUtil::IsValidOpcode(opcode) && OpCodeAttr::CanCSE(opcode) && !OpCodeAttr::ByteCodeOnly(opcode))
            {
                OpCodeToHash[(int)opcode] = hash;
                HashToOpCode[hash] = opcode;
                hash++;
                AssertMsg(hash != 0, "Too many CSE'able opcodes");
            }

        }
    }
}CSEInit_Dummy;


bool
GlobOpt::GetHash(IR::Instr *instr, Value *src1Val, Value *src2Val, ExprAttributes exprAttributes, ExprHash *pHash)
{
    Js::OpCode opcode = instr->m_opcode;

    // Candidate?
    if (!OpCodeAttr::CanCSE(opcode) && (opcode != Js::OpCode::StElemI_A && opcode != Js::OpCode::StElemI_A_Strict))
    {
        return false;
    }

    ValueNumber valNum1 = 0;
    ValueNumber valNum2 = 0;

    // Get the value number of src1 and src2
    if (instr->GetSrc1())
    {
        if (!src1Val)
        {
            return false;
        }
        valNum1 = src1Val->GetValueNumber();
        if (instr->GetSrc2())
        {
            if (!src2Val)
            {
                return false;
            }
            valNum2 = src2Val->GetValueNumber();
        }
    }

    if (src1Val)
    {
        valNum1 = src1Val->GetValueNumber();
        if (src2Val)
        {
            valNum2 = src2Val->GetValueNumber();
        }
    }

    switch (opcode)
    {
    case Js::OpCode::Ld_I4:
    case Js::OpCode::Ld_A:
        // Copy-prop should handle these
        return false;
    case Js::OpCode::Add_I4:
    case Js::OpCode::Add_Ptr:
        opcode = Js::OpCode::Add_A;
        break;
    case Js::OpCode::Sub_I4:
        opcode = Js::OpCode::Sub_A;
        break;
    case Js::OpCode::Mul_I4:
        opcode = Js::OpCode::Mul_A;
        break;
    case Js::OpCode::Rem_I4:
        opcode = Js::OpCode::Rem_A;
        break;
    case Js::OpCode::Div_I4:
        opcode = Js::OpCode::Div_A;
        break;
    case Js::OpCode::Neg_I4:
        opcode = Js::OpCode::Neg_A;
        break;
    case Js::OpCode::Not_I4:
        opcode = Js::OpCode::Not_A;
        break;
    case Js::OpCode::And_I4:
        opcode = Js::OpCode::And_A;
        break;
    case Js::OpCode::Or_I4:
        opcode = Js::OpCode::Or_A;
        break;
    case Js::OpCode::Xor_I4:
        opcode = Js::OpCode::Xor_A;
        break;
    case Js::OpCode::Shl_I4:
        opcode = Js::OpCode::Shl_A;
        break;
    case Js::OpCode::Shr_I4:
        opcode = Js::OpCode::Shr_A;
        break;
    case Js::OpCode::ShrU_I4:
        opcode = Js::OpCode::ShrU_A;
        break;
    case Js::OpCode::StElemI_A:
    case Js::OpCode::StElemI_A_Strict:
        // Make the load available as a CSE
        opcode = Js::OpCode::LdElemI_A;
        break;
    case Js::OpCode::CmEq_I4:
        opcode = Js::OpCode::CmEq_A;
        break;
    case Js::OpCode::CmNeq_I4:
        opcode = Js::OpCode::CmNeq_A;
        break;
    case Js::OpCode::CmLt_I4:
        opcode = Js::OpCode::CmLt_A;
        break;
    case Js::OpCode::CmLe_I4:
        opcode = Js::OpCode::CmLe_A;
        break;
    case Js::OpCode::CmGt_I4:
        opcode = Js::OpCode::CmGt_A;
        break;
    case Js::OpCode::CmGe_I4:
        opcode = Js::OpCode::CmGe_A;
        break;
    case Js::OpCode::CmUnLt_I4:
        opcode = Js::OpCode::CmUnLt_A;
        break;
    case Js::OpCode::CmUnLe_I4:
        opcode = Js::OpCode::CmUnLe_A;
        break;
    case Js::OpCode::CmUnGt_I4:
        opcode = Js::OpCode::CmUnGt_A;
        break;
    case Js::OpCode::CmUnGe_I4:
        opcode = Js::OpCode::CmUnGe_A;
        break;
    }

    pHash->Init(opcode, valNum1, valNum2, exprAttributes);

#if DBG_DUMP
    if (!pHash->IsValid() && Js::Configuration::Global.flags.Trace.IsEnabled(Js::CSEPhase, this->func->GetSourceContextId(), this->func->GetLocalFunctionId()))
    {
        Output::Print(L" >>>>  CSE: Value numbers too big to be hashed in function %s!\n", this->func->GetJnFunction()->GetDisplayName());
    }
#endif
#if ENABLE_DEBUG_CONFIG_OPTIONS
    if (!pHash->IsValid() && Js::Configuration::Global.flags.TestTrace.IsEnabled(Js::CSEPhase, this->func->GetSourceContextId(), this->func->GetLocalFunctionId()))
    {
        Output::Print(L" >>>>  CSE: Value numbers too big to be hashed in function %s!\n", this->func->GetJnFunction()->GetDisplayName());
    }
#endif

    return pHash->IsValid();
}

void
GlobOpt::CSEAddInstr(
    BasicBlock *block,
    IR::Instr *instr,
    Value *dstVal,
    Value *src1Val,
    Value *src2Val,
    Value *dstIndirIndexVal,
    Value *src1IndirIndexVal)
{
    ExprAttributes exprAttributes;
    ExprHash hash;

    if (!this->DoCSE())
    {
        return;
    }

    bool isArray = false;

    switch(instr->m_opcode)
    {
    case Js::OpCode::LdElemI_A:
    case Js::OpCode::LdInt8ArrViewElem:
    case Js::OpCode::LdUInt8ArrViewElem:
    case Js::OpCode::LdInt16ArrViewElem:
    case Js::OpCode::LdUInt16ArrViewElem:
    case Js::OpCode::LdInt32ArrViewElem:
    case Js::OpCode::LdUInt32ArrViewElem:
    case Js::OpCode::LdFloat32ArrViewElem:
    case Js::OpCode::LdFloat64ArrViewElem:
    case Js::OpCode::StElemI_A:
    case Js::OpCode::StElemI_A_Strict:
    {
        // For arrays, hash the value # of the baseOpnd and indexOpnd
        IR::IndirOpnd *arrayOpnd;
        Value *indirIndexVal;
        if (instr->m_opcode == Js::OpCode::StElemI_A || instr->m_opcode == Js::OpCode::StElemI_A_Strict)
        {
            if (!this->CanCSEArrayStore(instr))
            {
                return;
            }
            dstVal = src1Val;
            arrayOpnd = instr->GetDst()->AsIndirOpnd();
            indirIndexVal = dstIndirIndexVal;
        }
        else
        {
            // all the LdElem and Ld*ArrViewElem
            arrayOpnd = instr->GetSrc1()->AsIndirOpnd();
            indirIndexVal = src1IndirIndexVal;
        }

        src1Val = this->FindValue(block->globOptData.symToValueMap, arrayOpnd->GetBaseOpnd()->m_sym);
        if(indirIndexVal)
        {
            src2Val = indirIndexVal;
        }
        else if (arrayOpnd->GetIndexOpnd())
        {
            src2Val = this->FindValue(block->globOptData.symToValueMap, arrayOpnd->GetIndexOpnd()->m_sym);
        }
        else
        {
            return;
        }
        isArray = true;

        // for typed array do not add instructions whose dst are guaranteed to be int or number
        // as we will try to eliminate bound check for these typed arrays
        if (src1Val->GetValueInfo()->IsLikelyOptimizedVirtualTypedArray())
        {
            exprAttributes = DstIsIntOrNumberAttributes(!instr->dstIsAlwaysConvertedToInt32, !instr->dstIsAlwaysConvertedToNumber);
        }
        break;
    }

    case Js::OpCode::Mul_I4:
        // If int32 overflow is ignored, we only add MULs with 53-bit overflow check to expr map
        if (instr->HasBailOutInfo() && (instr->GetBailOutKind() & IR::BailOutOnMulOverflow) &&
            !instr->ShouldCheckFor32BitOverflow() && instr->ignoreOverflowBitCount != 53)
        {
            return;
        }

        // fall-through

    case Js::OpCode::Neg_I4:
    case Js::OpCode::Add_I4:
    case Js::OpCode::Sub_I4:
    case Js::OpCode::Div_I4:
    case Js::OpCode::Rem_I4:
    case Js::OpCode::Add_Ptr:
    case Js::OpCode::ShrU_I4:
    {
        // Can't CSE and Add where overflow doesn't matter (and no bailout) with one where it does matter... Record whether int
        // overflow or negative zero were ignored.
        exprAttributes = IntMathExprAttributes(ignoredIntOverflowForCurrentInstr, ignoredNegativeZeroForCurrentInstr);
        break;
    }

    case Js::OpCode::InlineMathFloor:
    case Js::OpCode::InlineMathCeil:
    case Js::OpCode::InlineMathRound:
        if (!instr->ShouldCheckForNegativeZero())
        {
            return;
        }
        break;
    }

    ValueInfo *valueInfo = NULL;

    if (instr->GetDst())
    {
        if (!dstVal)
        {
            return;
        }

        valueInfo = dstVal->GetValueInfo();
        if(valueInfo->GetSymStore() == NULL &&
            !(isArray && valueInfo->HasIntConstantValue() && valueInfo->IsIntAndLikelyTagged() && instr->GetSrc1()->IsAddrOpnd()))
        {
            return;
        }
    }

    if (!this->GetHash(instr, src1Val, src2Val, exprAttributes, &hash))
    {
        return;
    }

    int32 intConstantValue;
    if(valueInfo && !valueInfo->GetSymStore() && valueInfo->TryGetIntConstantValue(&intConstantValue))
    {
        Assert(isArray);
        Assert(valueInfo->IsIntAndLikelyTagged());
        Assert(instr->GetSrc1()->IsAddrOpnd());

        // We need a sym associated with a value in the expression value table. Hoist the address into a stack sym associated
        // with the int constant value.
        StackSym *const constStackSym = GetOrCreateTaggedIntConstantStackSym(intConstantValue);
        instr->HoistSrc1(Js::OpCode::Ld_A, RegNOREG, constStackSym);
        SetValue(&blockData, dstVal, constStackSym);
    }

    // We have a candidate.  Add it to the exprToValueMap.
    Value ** pVal = block->globOptData.exprToValueMap->FindOrInsertNew(hash);
    *pVal = dstVal;

    if (isArray)
    {
        block->globOptData.liveArrayValues->Set(hash);
    }

    if (MayNeedBailOnImplicitCall(instr, src1Val, src2Val))
    {
        this->currentBlock->globOptData.hasCSECandidates = true;

        // Use LiveFields to track is object.valueOf/toString could get overridden.
        IR::Opnd *src1 = instr->GetSrc1();
        if (src1)
        {
            if (src1->IsRegOpnd())
            {
                StackSym *varSym = src1->AsRegOpnd()->m_sym;

                if (varSym->IsTypeSpec())
                {
                    varSym = varSym->GetVarEquivSym(this->func);
                }
                block->globOptData.liveFields->Set(varSym->m_id);
            }
            IR::Opnd *src2 = instr->GetSrc2();
            if (src2 && src2->IsRegOpnd())
            {
                StackSym *varSym = src2->AsRegOpnd()->m_sym;

                if (varSym->IsTypeSpec())
                {
                    varSym = varSym->GetVarEquivSym(this->func);
                }
                block->globOptData.liveFields->Set(varSym->m_id);
            }
        }
    }
}

bool
GlobOpt::CSEOptimize(BasicBlock *block, IR::Instr * *const instrRef, Value **pSrc1Val, Value **pSrc2Val, Value **pSrc1IndirIndexVal, bool intMathExprOnly)
{
    Assert(instrRef);
    IR::Instr *&instr = *instrRef;
    Assert(instr);
    if (!this->DoCSE())
    {
        return false;
    }

    Value *src1Val = *pSrc1Val;
    Value *src2Val = *pSrc2Val;
    Value *src1IndirIndexVal = *pSrc1IndirIndexVal;
    bool isArray = false;
    ExprAttributes exprAttributes;
    ExprHash hash;

    // For arrays, hash the value # of the baseOpnd and indexOpnd
    switch(instr->m_opcode)
    {
        case Js::OpCode::LdInt8ArrViewElem:
        case Js::OpCode::LdUInt8ArrViewElem:
        case Js::OpCode::LdInt16ArrViewElem:
        case Js::OpCode::LdUInt16ArrViewElem:
        case Js::OpCode::LdInt32ArrViewElem:
        case Js::OpCode::LdUInt32ArrViewElem:
        case Js::OpCode::LdFloat32ArrViewElem:
        case Js::OpCode::LdFloat64ArrViewElem:
        case Js::OpCode::LdElemI_A:
        {
            if(intMathExprOnly)
            {
                return false;
            }

            IR::IndirOpnd *arrayOpnd = instr->GetSrc1()->AsIndirOpnd();

            src1Val = this->FindValue(block->globOptData.symToValueMap, arrayOpnd->GetBaseOpnd()->m_sym);
            if(src1IndirIndexVal)
            {
                src2Val = src1IndirIndexVal;
            }
            else if (arrayOpnd->GetIndexOpnd())
            {
                src2Val = this->FindValue(block->globOptData.symToValueMap, arrayOpnd->GetIndexOpnd()->m_sym);
            }
            else
            {
                return false;
            }
            // for typed array do not add instructions whose dst are guaranteed to be int or number
            // as we will try to eliminate bound check for these typed arrays
            if (src1Val->GetValueInfo()->IsLikelyOptimizedVirtualTypedArray())
            {
                exprAttributes = DstIsIntOrNumberAttributes(!instr->dstIsAlwaysConvertedToInt32, !instr->dstIsAlwaysConvertedToNumber);
            }
            isArray = true;
            break;
        }

        case Js::OpCode::Neg_A:
        case Js::OpCode::Add_A:
        case Js::OpCode::Sub_A:
        case Js::OpCode::Mul_A:
        case Js::OpCode::Div_A:
        case Js::OpCode::Rem_A:
        case Js::OpCode::ShrU_A:
            // If the previously-computed matching expression ignored int overflow or negative zero, those attributes must match
            // to be able to CSE this expression
            if(intMathExprOnly && !ignoredIntOverflowForCurrentInstr && !ignoredNegativeZeroForCurrentInstr)
            {
                // Already tried CSE with default attributes
                return false;
            }
            exprAttributes = IntMathExprAttributes(ignoredIntOverflowForCurrentInstr, ignoredNegativeZeroForCurrentInstr);
            break;

        default:
            if(intMathExprOnly)
            {
                return false;
            }
            break;
    }

    if (!this->GetHash(instr, src1Val, src2Val, exprAttributes, &hash))
    {
        return false;
    }

    // See if we have a value for that expression
    Value ** pVal = block->globOptData.exprToValueMap->Get(hash);

    if (pVal == NULL)
    {
        return false;
    }

    ValueInfo *valueInfo = NULL;
    Sym *symStore = NULL;
    Value *val = NULL;

    if (instr->GetDst())
    {

        if (*pVal == NULL)
        {
            return false;
        }

        val = *pVal;

        // Make sure the array value is still live.  We can't CSE something like:
        //    ... = A[i];
        //   B[j] = ...;
        //    ... = A[i];
        if (isArray && !block->globOptData.liveArrayValues->Test(hash))
        {
            return false;
        }

        // See if the symStore is still valid
        valueInfo = val->GetValueInfo();
        symStore = valueInfo->GetSymStore();
        Value * symStoreVal = NULL;

        int32 intConstantValue;
        if (!symStore && valueInfo->TryGetIntConstantValue(&intConstantValue))
        {
            // Handle:
            //  A[i] = 10;
            //    ... = A[i];
            if (!isArray)
            {
                return false;
            }
            if (!valueInfo->IsIntAndLikelyTagged())
            {
                return false;
            }

            symStore = GetTaggedIntConstantStackSym(intConstantValue);
        }

        if(!symStore || !symStore->IsStackSym())
        {
            return false;
        }
        symStoreVal = this->FindValue(block->globOptData.symToValueMap, symStore);

        if (!symStoreVal || symStoreVal->GetValueNumber() != val->GetValueNumber())
        {
            return false;
        }
        val = symStoreVal;
        valueInfo = val->GetValueInfo();
    }

    // Make sure srcs still have same values

    if (instr->GetSrc1())
    {
        if (!src1Val)
        {
            return false;
        }
        if (hash.GetSrc1ValueNumber() != src1Val->GetValueNumber())
        {
            return false;
        }

        IR::Opnd *src1 = instr->GetSrc1();

        if (src1->IsSymOpnd() && src1->AsSymOpnd()->IsPropertySymOpnd())
        {
            Assert(instr->m_opcode == Js::OpCode::CheckFixedFld);
            IR::PropertySymOpnd *propOpnd = src1->AsSymOpnd()->AsPropertySymOpnd();

            if (!propOpnd->IsTypeChecked() && !propOpnd->IsRootObjectNonConfigurableFieldLoad())
            {
                // Require m_CachedTypeChecked for 2 reasons:
                // - We may be relying on this instruction to do a type check for a downstream reference.
                // - This instruction may have a different inline cache, and thus need to check a different type,
                //   than the upstream check.
                // REVIEW: We could process this differently somehow to get the type check on the next reference.
                return false;
            }
        }
        if (instr->GetSrc2())
        {
            if (!src2Val)
            {
                return false;
            }
            if (hash.GetSrc2ValueNumber() != src2Val->GetValueNumber())
            {
                return false;
            }
        }
    }
    bool needsBailOnImplicitCall = false;

    // Need implicit call bailouts?
    if (this->MayNeedBailOnImplicitCall(instr, src1Val, src2Val))
    {
        needsBailOnImplicitCall = true;

        IR::Opnd *src1 = instr->GetSrc1();

        if (instr->m_opcode != Js::OpCode::StElemI_A && instr->m_opcode != Js::OpCode::StElemI_A_Strict
            && src1 && src1->IsRegOpnd())
        {
            StackSym *sym = src1->AsRegOpnd()->m_sym;

            if (this->IsTypeSpecialized(sym, block) || block->globOptData.liveInt32Syms->Test(sym->m_id))
            {
                IR::Opnd *src2 = instr->GetSrc2();

                if (!src2 || src2->IsImmediateOpnd())
                {
                    needsBailOnImplicitCall = false;
                }
                else if (src2->IsRegOpnd())
                {
                    StackSym *sym = src2->AsRegOpnd()->m_sym;
                    if (this->IsTypeSpecialized(src2->AsRegOpnd()->m_sym, block) || block->globOptData.liveInt32Syms->Test(sym->m_id))
                    {
                        needsBailOnImplicitCall = false;
                    }
                }
            }
        }
    }

    if (needsBailOnImplicitCall)
    {
        IR::Opnd *src1 = instr->GetSrc1();
        if (src1)
        {
            if (src1->IsRegOpnd())
            {
                StackSym *varSym = src1->AsRegOpnd()->m_sym;

                Assert(!varSym->IsTypeSpec());
                if (!block->globOptData.liveFields->Test(varSym->m_id))
                {
                    return false;
                }
                IR::Opnd *src2 = instr->GetSrc2();
                if (src2 && src2->IsRegOpnd())
                {
                    StackSym *varSym = src2->AsRegOpnd()->m_sym;

                    Assert(!varSym->IsTypeSpec());
                    if (!block->globOptData.liveFields->Test(varSym->m_id))
                    {
                        return false;
                    }
                }
            }
        }
    }
    // in asmjs we can have a symstore with a different type
    //  x = HEAPF32[i >> 2]
    //  y  = HEAPI32[i >> 2]
    if (instr->GetDst() && (instr->GetDst()->GetType() != symStore->AsStackSym()->GetType()))
    {
        Assert(GetIsAsmJSFunc());
        return false;
    }

    // SIMD_JS
    if (instr->m_opcode == Js::OpCode::ExtendArg_A)
    {
        // we don't want to CSE ExtendArgs, only the operation using them. To do that, we mimic CSE by transferring the symStore valueInfo to the dst.
        IR::Opnd *dst = instr->GetDst();
        Value *dstVal = this->FindValue(symStore);
        this->SetValue(&this->blockData, dstVal, dst);
        dst->AsRegOpnd()->m_sym->CopySymAttrs(symStore->AsStackSym());
        return false;
    }

    //
    // Success, do the CSE rewrite.
    //

#if DBG_DUMP
    if (Js::Configuration::Global.flags.Trace.IsEnabled(Js::CSEPhase, this->func->GetSourceContextId(), this->func->GetLocalFunctionId()))
    {
        Output::Print(L" --- CSE (%s): ", this->func->GetJnFunction()->GetDisplayName());
        instr->Dump();
    }
#endif
#if ENABLE_DEBUG_CONFIG_OPTIONS
    if (Js::Configuration::Global.flags.TestTrace.IsEnabled(Js::CSEPhase, this->func->GetSourceContextId(), this->func->GetLocalFunctionId()))
    {
        Output::Print(L" --- CSE (%s): %s\n", this->func->GetJnFunction()->GetDisplayName(), Js::OpCodeUtil::GetOpCodeName(instr->m_opcode));
    }
#endif

    this->CaptureByteCodeSymUses(instr);

    if (!instr->GetDst())
    {
        instr->m_opcode = Js::OpCode::Nop;
        return true;
    }

    AnalysisAssert(valueInfo);

    IR::Opnd *cseOpnd;

    cseOpnd = IR::RegOpnd::New(symStore->AsStackSym(), instr->GetDst()->GetType(), instr->m_func);
    cseOpnd->SetValueType(valueInfo->Type());
    cseOpnd->SetIsJITOptimizedReg(true);

    if (needsBailOnImplicitCall)
    {
        this->CaptureNoImplicitCallUses(cseOpnd, false);
    }

    int32 intConstantValue;
    if (valueInfo->TryGetIntConstantValue(&intConstantValue) && valueInfo->IsIntAndLikelyTagged())
    {
        cseOpnd->Free(func);
        cseOpnd = IR::AddrOpnd::New(Js::TaggedInt::ToVarUnchecked(intConstantValue), IR::AddrOpndKindConstantVar, instr->m_func);
        cseOpnd->SetValueType(valueInfo->Type());
    }

    *pSrc1Val = val;

    {
        // Profiled instructions have data that is interpreted differently based on the op code. Since we're changing the op
        // code and due to other similar potential issues, always create a new instr instead of changing the existing one.
        IR::Instr *const originalInstr = instr;
        instr = IR::Instr::New(Js::OpCode::Ld_A, instr->GetDst(), cseOpnd, instr->m_func);
        originalInstr->TransferDstAttributesTo(instr);
        block->InsertInstrBefore(instr, originalInstr);
        block->RemoveInstr(originalInstr);
    }

    *pSrc2Val = NULL;
    *pSrc1IndirIndexVal = NULL;
    return true;
}

void
GlobOpt::ProcessArrayValueKills(IR::Instr *instr)
{
    switch (instr->m_opcode)
    {
    case Js::OpCode::StElemI_A:
    case Js::OpCode::StElemI_A_Strict:
    case Js::OpCode::DeleteElemI_A:
    case Js::OpCode::DeleteElemIStrict_A:
    case Js::OpCode::StFld:
    case Js::OpCode::StRootFld:
    case Js::OpCode::StFldStrict:
    case Js::OpCode::StRootFldStrict:
    case Js::OpCode::StSlot:
    case Js::OpCode::StSlotChkUndecl:
    case Js::OpCode::DeleteFld:
    case Js::OpCode::DeleteRootFld:
    case Js::OpCode::DeleteFldStrict:
    case Js::OpCode::DeleteRootFldStrict:
    case Js::OpCode::StInt8ArrViewElem:
    case Js::OpCode::StUInt8ArrViewElem:
    case Js::OpCode::StInt16ArrViewElem:
    case Js::OpCode::StUInt16ArrViewElem:
    case Js::OpCode::StInt32ArrViewElem:
    case Js::OpCode::StUInt32ArrViewElem:
    case Js::OpCode::StFloat32ArrViewElem:
    case Js::OpCode::StFloat64ArrViewElem:
    // These array helpers may change A.length (and A[i] could be A.length)...
    case Js::OpCode::InlineArrayPush:
    case Js::OpCode::InlineArrayPop:
        this->blockData.liveArrayValues->ClearAll();
        break;

    case Js::OpCode::CallDirect:
        Assert(instr->GetSrc1());
        switch(instr->GetSrc1()->AsHelperCallOpnd()->m_fnHelper)
        {
            // These array helpers may change A[i]
            case IR::HelperArray_Reverse:
            case IR::HelperArray_Shift:
            case IR::HelperArray_Unshift:
            case IR::HelperArray_Splice:
                this->blockData.liveArrayValues->ClearAll();
                break;
        }
        break;
    default:
        if (instr->UsesAllFields())
        {
            this->blockData.liveArrayValues->ClearAll();
        }
        break;
    }
}

bool
GlobOpt::NeedBailOnImplicitCallForCSE(BasicBlock *block, bool isForwardPass)
{
    return isForwardPass && block->globOptData.hasCSECandidates;
}

bool
GlobOpt::DoCSE()
{
    if (PHASE_OFF(Js::CSEPhase, this->func))
    {
        return false;
    }
    if (this->IsLoopPrePass())
    {
        return false;
    }

    if (PHASE_FORCE(Js::CSEPhase, this->func))
    {
        // Force always turn it on
        return true;
    }

    if (!this->DoFieldOpts(this->currentBlock->loop) && !GetIsAsmJSFunc())
    {
        return false;
    }

    return true;
}

bool
GlobOpt::CanCSEArrayStore(IR::Instr *instr)
{
    IR::Opnd *arrayOpnd = instr->GetDst();
    Assert(arrayOpnd->IsIndirOpnd());
    IR::RegOpnd *baseOpnd = arrayOpnd->AsIndirOpnd()->GetBaseOpnd();

    ValueType baseValueType(baseOpnd->GetValueType());

    // Only handle definite arrays for now.  Typed Arrays would require truncation of the CSE'd value.
    if (!baseValueType.IsArrayOrObjectWithArray())
    {
        return false;
    }

    return true;
}


#if DBG_DUMP
void
DumpExpr(ExprHash hash)
{
    Output::Print(L"Opcode: %10s   src1Val: %d  src2Val: %d\n", Js::OpCodeUtil::GetOpCodeName(HashToOpCode[(int)hash.GetOpcode()]), hash.GetSrc1ValueNumber(), hash.GetSrc2ValueNumber());
}
#endif
