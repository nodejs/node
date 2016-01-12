//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "BackEnd.h"

/* ===================================================================================
 * TempTracker runs the mark temp algorithm.  The template parameter provides information
 * what are valid temp use, temp transfer, or temp producing operations and what bit to
 * set once a symbol def can be marked temp.
 *
 * NumberTemp mark temp JavascriptNumber creation for math operations, run during deadstore
 *
 * ObjectTemp mark temp object allocations, run during backward pass so that it can provide
 * information to the globopt to install pre op bailout on implicit call while during stack
 * allocation objects.
 *
 * ObjectTempVerify runs a similar mark temp during deadstore in debug mode to assert
 * that globopt have install the pre op necessary and a marked temp def is still valid
 * as a mark temp
 *
 * The basic of the mark temp algorithm is very simple: we keep track if we have seen
 * any use of a symbol that is not a valid mark temp (the nonTempSyms bitvector)
 * and on definition of the symbol, if the all the use allow temp object (not in nonTempSyms
 * bitvector) then it is mark them able.
 *
 * However, the complication comes when the stack object is transfered to another symbol
 * and we are in a loop.  We need to make sure that the stack object isn't still referred
 * by another symbol when we allocate the number/object in the next iteration
 *
 * For example:
 *      Loop top:
 *          s1 = NewScObject
 *             = s6
 *          s6 = s1
 *          Goto Loop top
 *
 * We cannot mark them this case because when s1 is created, the object might still be
 * referred to by s6 from previous iteration, and thus if we mark them we would have
 * change the content of s6 as well.
 *
 * To detect this dependency, we conservatively collect "all" transfers in the pre pass
 * of the loop.  We have to be conservative to detect reverse dependencies without
 * iterating more than 2 times for the loop.
 * =================================================================================== */

JitArenaAllocator *
TempTrackerBase::GetAllocator() const
{
    return nonTempSyms.GetAllocator();
}

TempTrackerBase::TempTrackerBase(JitArenaAllocator * alloc, bool inLoop)
    : nonTempSyms(alloc), tempTransferredSyms(alloc)
{
    if (inLoop)
    {
        tempTransferDependencies = HashTable<BVSparse<JitArenaAllocator> *>::New(alloc, 16);
    }
    else
    {
        tempTransferDependencies = nullptr;
    }
}

TempTrackerBase::~TempTrackerBase()
{
    if (this->tempTransferDependencies != nullptr)
    {
        JitArenaAllocator * alloc = this->GetAllocator();
        FOREACH_HASHTABLE_ENTRY(BVSparse<JitArenaAllocator> *, bucket, this->tempTransferDependencies)
        {
            JitAdelete(alloc, bucket.element);
        }
        NEXT_HASHTABLE_ENTRY;
        this->tempTransferDependencies->Delete();
    }
}
void
TempTrackerBase::MergeData(TempTrackerBase * fromData, bool deleteData)
{
    nonTempSyms.Or(&fromData->nonTempSyms);
    tempTransferredSyms.Or(&fromData->tempTransferredSyms);
    MergeDependencies(tempTransferDependencies, fromData->tempTransferDependencies, deleteData);
}

void
TempTrackerBase::AddTransferDependencies(int sourceId, SymID dstSymID, HashTable<BVSparse<JitArenaAllocator> *> * dependencies)
{
    // Add to the transfer dependencies set
    BVSparse<JitArenaAllocator> ** pBVSparse = dependencies->FindOrInsertNew(sourceId);
    if (*pBVSparse == nullptr)
    {
        *pBVSparse = JitAnew(this->GetAllocator(), BVSparse<JitArenaAllocator>, this->GetAllocator());
    }
    AddTransferDependencies(*pBVSparse, dstSymID);
}

void
TempTrackerBase::AddTransferDependencies(BVSparse<JitArenaAllocator> * bv, SymID dstSymID)
{
    bv->Set(dstSymID);

    // Add the indirect transfers (always from tempTransferDepencies)
    BVSparse<JitArenaAllocator> *dstBVSparse = this->tempTransferDependencies->GetAndClear(dstSymID);
    if (dstBVSparse != nullptr)
    {
        bv->Or(dstBVSparse);
        JitAdelete(this->GetAllocator(), dstBVSparse);
    }
}

template <typename T>
TempTracker<T>::TempTracker(JitArenaAllocator * alloc, bool inLoop):
    T(alloc, inLoop)
{
}

template <typename T>
void
TempTracker<T>::MergeData(TempTracker<T> * fromData, bool deleteData)
{
    TempTrackerBase::MergeData(fromData, deleteData);
    T::MergeData(fromData, deleteData);

    if (deleteData)
    {
        JitAdelete(this->GetAllocator(), fromData);
    }
}

void
TempTrackerBase::OrHashTableOfBitVector(HashTable<BVSparse<JitArenaAllocator> *> * toData, HashTable<BVSparse<JitArenaAllocator> *> *& fromData, bool deleteData)
{
    Assert(toData != nullptr);
    Assert(fromData != nullptr);
    toData->Or(fromData,
        [=](BVSparse<JitArenaAllocator> * bv1, BVSparse<JitArenaAllocator> * bv2) -> BVSparse<JitArenaAllocator> *
    {
        if (bv1 == nullptr)
        {
            if (deleteData)
            {
                return bv2;
            }
            return bv2->CopyNew(this->GetAllocator());
        }
        bv1->Or(bv2);
        if (deleteData)
        {
            JitAdelete(this->GetAllocator(), bv2);
        }
        return bv1;
    });

    if (deleteData)
    {
        fromData->Delete();
        fromData = nullptr;
    }
}

void
TempTrackerBase::MergeDependencies(HashTable<BVSparse<JitArenaAllocator> *> * toData, HashTable<BVSparse<JitArenaAllocator> *> *& fromData, bool deleteData)
{
    if (fromData != nullptr)
    {
        if (toData != nullptr)
        {
            OrHashTableOfBitVector(toData, fromData, deleteData);
        }
        else if (deleteData)
        {
            FOREACH_HASHTABLE_ENTRY(BVSparse<JitArenaAllocator> *, bucket, fromData)
            {
                JitAdelete(this->GetAllocator(), bucket.element);
            }
            NEXT_HASHTABLE_ENTRY;
            fromData->Delete();
            fromData = nullptr;
        }
    }
}

#if DBG_DUMP
void
TempTrackerBase::Dump(wchar_t const * traceName)
{
    Output::Print(L"%s:        Non temp syms:", traceName);
    this->nonTempSyms.Dump();
    Output::Print(L"%s: Temp transfered syms:", traceName);
    this->tempTransferredSyms.Dump();
    if (this->tempTransferDependencies != nullptr)
    {
        Output::Print(L"%s: Temp transfer dependencies:\n", traceName);
        this->tempTransferDependencies->Dump();
    }
}
#endif

template <typename T>
void
TempTracker<T>::ProcessUse(StackSym * sym, BackwardPass * backwardPass)
{
    // Don't care about type specialized syms
    if (!sym->IsVar())
    {
        return;
    }
    IR::Instr * instr = backwardPass->currentInstr;
    SymID usedSymID = sym->m_id;
    bool isTempPropertyTransferStore = T::IsTempPropertyTransferStore(instr, backwardPass);
    bool isTempUse = isTempPropertyTransferStore || T::IsTempUse(instr, sym, backwardPass);
    if (!isTempUse)
    {
        this->nonTempSyms.Set(usedSymID);
    }
#if DBG
    if (T::DoTrace(backwardPass))
    {
        Output::Print(L"%s: %8s%4sTemp Use (s%-3d): ", T::GetTraceName(),
            backwardPass->IsPrePass() ? L"Prepass " : L"", isTempUse ? L"" : L"Non ", usedSymID);
        instr->DumpSimple();
        Output::Flush();
    }
#endif
    if (T::IsTempTransfer(instr))
    {
        this->tempTransferredSyms.Set(usedSymID);

        // Track dependencies if we are in loop only
        if (this->tempTransferDependencies != nullptr)
        {
            IR::Opnd * dstOpnd = instr->GetDst();
            if (dstOpnd->IsRegOpnd())
            {
                SymID dstSymID = dstOpnd->AsRegOpnd()->m_sym->m_id;

                if (dstSymID != usedSymID)
                {
                    // Record that the usedSymID may propagate to dstSymID and all the symbols
                    // that it may propagate to as well
                    AddTransferDependencies(usedSymID, dstSymID, this->tempTransferDependencies);
#if DBG_DUMP
                    if (T::DoTrace(backwardPass))
                    {
                        Output::Print(L"%s: %8s s%d -> s%d: ", T::GetTraceName(),
                            backwardPass->IsPrePass() ? L"Prepass " : L"", dstSymID, usedSymID);
                        (*this->tempTransferDependencies->Get(usedSymID))->Dump();
                    }
#endif
                }
            }
        }
    }

    if (isTempPropertyTransferStore)
    {
        this->tempTransferredSyms.Set(usedSymID);
        PropertySym * propertySym = instr->GetDst()->AsSymOpnd()->m_sym->AsPropertySym();
        PropagateTempPropertyTransferStoreDependencies(usedSymID, propertySym, backwardPass);

#if DBG_DUMP
        if (T::DoTrace(backwardPass) && this->tempTransferDependencies)
        {
            Output::Print(L"%s: %8s (PropId:%d %s)+[] -> s%d: ", T::GetTraceName(),
                backwardPass->IsPrePass() ? L"Prepass " : L"", propertySym->m_propertyId,
                backwardPass->func->GetScriptContext()->GetPropertyNameLocked(propertySym->m_propertyId)->GetBuffer(), usedSymID);
            BVSparse<JitArenaAllocator> ** transferDependencies = this->tempTransferDependencies->Get(usedSymID);
            if (transferDependencies)
            {
                (*transferDependencies)->Dump();
            }
            else
            {
                Output::Print(L"[]\n");
            }
        }
#endif
    }
};

template <typename T>
void
TempTracker<T>::MarkTemp(StackSym * sym, BackwardPass * backwardPass)
{
    // Don't care about type specialized syms
    Assert(sym->IsVar());

    IR::Instr * instr = backwardPass->currentInstr;
    BOOLEAN nonTemp = this->nonTempSyms.TestAndClear(sym->m_id);
    BOOLEAN isTempTransfered;
    BVSparse<JitArenaAllocator> * bvTempTransferDependencies = nullptr;

    bool const isTransferOperation =
        T::IsTempTransfer(instr)
        || T::IsTempPropertyTransferLoad(instr, backwardPass)
        || T::IsTempIndirTransferLoad(instr, backwardPass);

    if (this->tempTransferDependencies != nullptr)
    {
        // Since we don't iterate "while (!changed)" in loops, we don't have complete accurate dataflow
        // for loop carried dependencies. So don't clear the dependency transfer info.  WOOB:1121525

        // Check if this dst is transfered (assigned) to another symbol
        if (isTransferOperation)
        {
            isTempTransfered = this->tempTransferredSyms.Test(sym->m_id);
        }
        else
        {
            isTempTransfered = this->tempTransferredSyms.TestAndClear(sym->m_id);
        }

        // We only need to look at the dependencies if we are in a loop because of the back edge
        // Also we don't need to if we are in pre pass
        if (isTempTransfered)
        {
            if (!backwardPass->IsPrePass())
            {
                if (isTransferOperation)
                {
                    // Transfer operation, load but not clear the information
                    BVSparse<JitArenaAllocator> **pBv = this->tempTransferDependencies->Get(sym->m_id);
                    if (pBv)
                    {
                        bvTempTransferDependencies = *pBv;
                    }
                }
                else
                {
                    // Non transfer operation, load and clear the information and the dst value is replaced
                    bvTempTransferDependencies = this->tempTransferDependencies->GetAndClear(sym->m_id);
                }
            }
            else if (!isTransferOperation)
            {
                // In pre pass, and not a transfer operation (just an assign).  We can clear the dependency info
                // and not look at it.
                this->tempTransferDependencies->Clear(sym->m_id);
            }
        }
    }
    else
    {
        isTempTransfered = this->tempTransferredSyms.TestAndClear(sym->m_id);
    }

    // Reset the dst is temp bit (we set it optimistically on the loop pre pass)
    bool dstIsTemp = false;
    bool dstIsTempTransferred = false;

    if (nonTemp)
    {
#if DBG_DUMP
        if (T::DoTrace(backwardPass)  && !backwardPass->IsPrePass() && T::CanMarkTemp(instr, backwardPass))
        {
            Output::Print(L"%s: Not temp (s%-03d):", T::GetTraceName(), sym->m_id);
            instr->DumpSimple();
        }
#endif
    }
    else if (backwardPass->IsPrePass())
    {
        // On pre pass, we don't have complete information about whether it is tempable or
        // not from the back edge.  If we already discovered that it is not a temp (above), then
        // we don't mark it, other wise, assume that it is okay to be tempable and have the
        // second pass set the bit correctly.  The only works on dependency chain that is in order
        // e.g.
        //      s1 = Add
        //      s2 = s1
        //      s3 = s2
        // The dependencies tracking to catch the case whether the dependency chain is out of order
        // e.g
        //      s1 = Add
        //      s3 = s2
        //      s2 = s3

        Assert(isTransferOperation == T::IsTempTransfer(instr)
            || T::IsTempPropertyTransferLoad(instr, backwardPass)
            || T::IsTempIndirTransferLoad(instr, backwardPass));
        if (isTransferOperation)
        {
            dstIsTemp = true;
        }
    }
    else if (T::CanMarkTemp(instr, backwardPass))
    {
        dstIsTemp = true;

        if (isTempTransfered)
        {
            // Track whether the dst is transfered or not, and allocate separate stack slot for them
            // so that another dst will not overrides the value
            dstIsTempTransferred = true;

            // The temp is aliased, need to trace if there is another use of the set of aliased
            // sym that is still live so that we won't mark them this symbol and destroy the value

            if (bvTempTransferDependencies != nullptr)
            {
                // Inside a loop we need to track if any of the reg that we transfered to is still live
                //      s1 = Add
                //         = s2
                //      s2 = s1
                // Since s2 is still live on the next iteration when we reassign s1, making s1 a temp
                // will cause the value of s2 to change before it's use.

                // The upwardExposedUses are the live regs, check if it intersect with the set
                // of dependency or not.

#if DBG_DUMP
                if (T::DoTrace(backwardPass) && Js::Configuration::Global.flags.Verbose)
                {
                     Output::Print(L"%s: Loop mark temp check instr:\n", T::GetTraceName());
                     instr->DumpSimple();
                     Output::Print(L"Transfer dependencies: ");
                     bvTempTransferDependencies->Dump();
                     Output::Print(L"Upward exposed Uses  : ");
                     backwardPass->currentBlock->upwardExposedUses->Dump();
                     Output::Print(L"\n");
                }
#endif

                BVSparse<JitArenaAllocator> * upwardExposedUses = backwardPass->currentBlock->upwardExposedUses;
                bool hasExposedDependencies = bvTempTransferDependencies->Test(upwardExposedUses)
                    || T::HasExposedFieldDependencies(bvTempTransferDependencies, backwardPass);
                if (hasExposedDependencies)
                {
#if DBG_DUMP
                    if (T::DoTrace(backwardPass))
                    {
                        Output::Print(L"%s: Not temp (s%-03d): ", T::GetTraceName(), sym->m_id);
                        instr->DumpSimple();
                        Output::Print(L"       Transfered exposed uses: ");
                        JitArenaAllocator tempAllocator(L"temp", this->GetAllocator()->GetPageAllocator(), Js::Throw::OutOfMemory);
                        bvTempTransferDependencies->AndNew(upwardExposedUses, &tempAllocator)->Dump();
                    }
#endif

                    dstIsTemp = false;
                    dstIsTempTransferred = false;
#if DBG
                    if (IsObjectTempVerify<T>())
                    {
                        dstIsTemp = ObjectTempVerify::DependencyCheck(instr, bvTempTransferDependencies, backwardPass);
                    }
#endif
                    // Only ObjectTmepVerify would do the do anything here. All other returns false
                }
            }
        }
    }

    T::SetDstIsTemp(dstIsTemp, dstIsTempTransferred, instr, backwardPass);
}

NumberTemp::NumberTemp(JitArenaAllocator * alloc, bool inLoop)
    : TempTrackerBase(alloc, inLoop), elemLoadDependencies(alloc), nonTempElemLoad(false),
      upwardExposedMarkTempObjectLiveFields(alloc), upwardExposedMarkTempObjectSymsProperties(nullptr)
{
    propertyIdsTempTransferDependencies = inLoop ? HashTable<BVSparse<JitArenaAllocator> *>::New(alloc, 16) : nullptr;
}

void
NumberTemp::MergeData(NumberTemp * fromData, bool deleteData)
{
    nonTempElemLoad = nonTempElemLoad || fromData->nonTempElemLoad;
    if (!nonTempElemLoad)       // Don't bother merging other data if we already have a nonTempElemLoad
    {
        if (IsInLoop())
        {
            // in loop
            elemLoadDependencies.Or(&fromData->elemLoadDependencies);
        }
        MergeDependencies(propertyIdsTempTransferDependencies, fromData->propertyIdsTempTransferDependencies, deleteData);

        if (fromData->upwardExposedMarkTempObjectSymsProperties)
        {
            if (upwardExposedMarkTempObjectSymsProperties)
            {
                OrHashTableOfBitVector(upwardExposedMarkTempObjectSymsProperties, fromData->upwardExposedMarkTempObjectSymsProperties, deleteData);
            }
            else if (deleteData)
            {
                upwardExposedMarkTempObjectSymsProperties = fromData->upwardExposedMarkTempObjectSymsProperties;
                fromData->upwardExposedMarkTempObjectSymsProperties = nullptr;
            }
            else
            {
                upwardExposedMarkTempObjectSymsProperties = HashTable<BVSparse<JitArenaAllocator> *>::New(this->GetAllocator(), 16);
                OrHashTableOfBitVector(upwardExposedMarkTempObjectSymsProperties, fromData->upwardExposedMarkTempObjectSymsProperties, deleteData);
            }
        }

        upwardExposedMarkTempObjectLiveFields.Or(&fromData->upwardExposedMarkTempObjectLiveFields);
    }
}

bool
NumberTemp::IsTempUse(IR::Instr * instr, Sym * sym, BackwardPass * backwardPass)
{
    Js::OpCode opcode = instr->m_opcode;
    if (OpCodeAttr::NonTempNumberSources(opcode)
        || (OpCodeAttr::TempNumberTransfer(opcode) && !instr->dstIsTempNumber))
    {
        // For TypedArray stores, we don't store the Var object, so MarkTemp is valid
        if (opcode != Js::OpCode::StElemI_A
            || !instr->GetDst()->AsIndirOpnd()->GetBaseOpnd()->GetValueType().IsLikelyOptimizedTypedArray())
        {
            // Mark the symbol as non-tempable if the instruction doesn't allow temp sources,
            // or it is transfered to a non-temp dst
            return false;
        }
    }
    return true;
}

bool
NumberTemp::IsTempTransfer(IR::Instr * instr)
{
    return OpCodeAttr::TempNumberTransfer(instr->m_opcode);
}

bool
NumberTemp::IsTempProducing(IR::Instr * instr)
{
    Js::OpCode opcode = instr->m_opcode;
    if (OpCodeAttr::TempNumberProducing(opcode))
    {
        return true;
    }

    // Loads from float typedArrays usually require a conversion to Var, which we can MarkTemp.
    if (opcode == Js::OpCode::LdElemI_A)
    {
        const ValueType baseValueType(instr->GetSrc1()->AsIndirOpnd()->GetBaseOpnd()->GetValueType());
        if (baseValueType.IsLikelyObject()
            && (baseValueType.GetObjectType() == ObjectType::Float32Array
            || baseValueType.GetObjectType() == ObjectType::Float64Array))
        {
            return true;
        }
    }

    return false;
}

bool
NumberTemp::CanMarkTemp(IR::Instr * instr, BackwardPass * backwardPass)
{
    if (IsTempTransfer(instr) || IsTempProducing(instr))
    {
        return true;
    }

    // REVIEW: this is added a long time ago, and I am not sure what this is for any more.
    if (OpCodeAttr::InlineCallInstr(instr->m_opcode))
    {
        return true;
    }

    if (NumberTemp::IsTempIndirTransferLoad(instr, backwardPass)
        || NumberTemp::IsTempPropertyTransferLoad(instr, backwardPass))
    {
        return true;
    }

    // the opcode is not temp producing or a transfer, this is not a tmp
    // Also mark calls which may get inlined.
    return false;
}

void
NumberTemp::ProcessInstr(IR::Instr * instr, BackwardPass * backwardPass)
{
#if DBG
    if (instr->m_opcode == Js::OpCode::BailOnNoProfile)
    {
        // If we see BailOnNoProfile, we shouldn't have any successor to have any non temp syms
        Assert(!this->nonTempElemLoad);
        Assert(this->nonTempSyms.IsEmpty());
        Assert(this->tempTransferredSyms.IsEmpty());
        Assert(this->elemLoadDependencies.IsEmpty());
        Assert(this->upwardExposedMarkTempObjectLiveFields.IsEmpty());
    }
#endif

    // We don't get to process all dst in MarkTemp. Do it here for the upwardExposedMarkTempObjectLiveFields
    if (!this->DoMarkTempNumbersOnTempObjects(backwardPass))
    {
        return;
    }

    IR::Opnd * dst = instr->GetDst();
    if (dst == nullptr || !dst->IsRegOpnd())
    {
        return;
    }
    StackSym * dstSym = dst->AsRegOpnd()->m_sym;
    if (!dstSym->IsVar())
    {
        dstSym = dstSym->GetVarEquivSym(nullptr);
        if (dstSym == nullptr)
        {
            return;
        }
    }

    SymID dstSymId = dstSym->m_id;
    if (this->upwardExposedMarkTempObjectSymsProperties)
    {
        // We are assigning to dstSym, it no longer has upward exposed use, get the information and clear it from the hash table
        BVSparse<JitArenaAllocator> * dstBv = this->upwardExposedMarkTempObjectSymsProperties->GetAndClear(dstSymId);
        if (dstBv)
        {
            // Clear the upward exposed live fields of all the property sym id associated to dstSym
            this->upwardExposedMarkTempObjectLiveFields.Minus(dstBv);
            if (ObjectTemp::IsTempTransfer(instr) && instr->GetSrc1()->IsRegOpnd())
            {
                // If it is transfer, copy the dst info to the src
                SymID srcStackSymId = instr->GetSrc1()->AsRegOpnd()->m_sym->AsStackSym()->m_id;

                SymTable * symTable = backwardPass->func->m_symTable;
                FOREACH_BITSET_IN_SPARSEBV(index, dstBv)
                {
                    PropertySym * propertySym = symTable->FindPropertySym(srcStackSymId, index);
                    if (propertySym)
                    {
                        this->upwardExposedMarkTempObjectLiveFields.Set(propertySym->m_id);
                    }
                }
                NEXT_BITSET_IN_SPARSEBV;

                BVSparse<JitArenaAllocator> ** srcBv = this->upwardExposedMarkTempObjectSymsProperties->FindOrInsert(dstBv, srcStackSymId);
                if (srcBv)
                {
                    (*srcBv)->Or(dstBv);
                    JitAdelete(this->GetAllocator(), dstBv);
                }
            }
            else
            {
                JitAdelete(this->GetAllocator(), dstBv);
            }
        }
    }
}

void
NumberTemp::SetDstIsTemp(bool dstIsTemp, bool dstIsTempTransferred, IR::Instr * instr, BackwardPass * backwardPass)
{
    Assert(dstIsTemp || !dstIsTempTransferred);
    Assert(!instr->dstIsTempNumberTransferred);
    instr->dstIsTempNumber = dstIsTemp;
    instr->dstIsTempNumberTransferred = dstIsTempTransferred;
#if DBG_DUMP
    if (!backwardPass->IsPrePass() && IsTempProducing(instr))
    {
        backwardPass->numMarkTempNumber += dstIsTemp;
        backwardPass->numMarkTempNumberTransfered += dstIsTempTransferred;
    }
#endif
}

bool
NumberTemp::IsTempPropertyTransferLoad(IR::Instr * instr, BackwardPass * backwardPass)
{
    if (DoMarkTempNumbersOnTempObjects(backwardPass))
    {
        switch (instr->m_opcode)
        {
        case Js::OpCode::LdFld:
        case Js::OpCode::LdFldForTypeOf:
        case Js::OpCode::LdMethodFld:
        case Js::OpCode::LdFldForCallApplyTarget:
        case Js::OpCode::LdMethodFromFlags:
            {
                // Only care about load from possible stack allocated object.
                return instr->GetSrc1()->CanStoreTemp();
            }
        };

        // All other opcode shouldn't have sym opnd that can store temp, See ObjectTemp::IsTempUseOpCodeSym.
        Assert(instr->GetSrc1() == nullptr
            || instr->GetDst() == nullptr           // this isn't a value loading instruction
            || instr->GetSrc1()->IsIndirOpnd()      // this is detected in IsTempIndirTransferLoad
            || !instr->GetSrc1()->CanStoreTemp());
    }
    return false;
}

bool
NumberTemp::IsTempPropertyTransferStore(IR::Instr * instr, BackwardPass * backwardPass)
{
    if (DoMarkTempNumbersOnTempObjects(backwardPass))
    {
        switch (instr->m_opcode)
        {
        case Js::OpCode::InitFld:
        case Js::OpCode::StFld:
        case Js::OpCode::StFldStrict:
            {
                IR::Opnd * dst = instr->GetDst();
                Assert(dst->IsSymOpnd());
                if (!dst->CanStoreTemp())
                {
                    return false;
                }
                // We don't mark temp store of numeric properties (e.g. object literal { 86: foo });
                // This should only happen for InitFld, as StFld should have changed to StElem
                PropertySym *propertySym = dst->AsSymOpnd()->m_sym->AsPropertySym();
                SymID propertySymId = this->GetRepresentativePropertySymId(propertySym, backwardPass);
                return !this->nonTempSyms.Test(propertySymId) &&
                    !instr->m_func->GetScriptContext()->GetPropertyNameLocked(propertySym->m_propertyId)->IsNumeric();
            }
        };

        // All other opcode shouldn't have sym opnd that can store temp, see ObjectTemp::IsTempUseOpCodeSym.
        // We also never mark the dst indir as can store temp for StElemI_A because we don't know what property
        // it is storing in (or it could be an array index).
        Assert(instr->GetDst() == nullptr || !instr->GetDst()->CanStoreTemp());
    }
    return false;
}

bool
NumberTemp::IsTempIndirTransferLoad(IR::Instr * instr, BackwardPass * backwardPass)
{
    if (DoMarkTempNumbersOnTempObjects(backwardPass))
    {
        if (instr->m_opcode == Js::OpCode::LdElemI_A)
        {
            // If the index is an int, then we don't care about the non-temp use
            IR::Opnd * src1Opnd = instr->GetSrc1();
            IR::RegOpnd * indexOpnd = src1Opnd->AsIndirOpnd()->GetIndexOpnd();
            if (indexOpnd && (indexOpnd->m_sym->m_isNotInt || !indexOpnd->GetValueType().IsInt()))
            {
                return src1Opnd->CanStoreTemp();
            }
        }
        else
        {
            // All other opcode shouldn't have sym opnd that can store temp, See ObjectTemp::IsTempUseOpCodeSym.
            Assert(instr->GetSrc1() == nullptr || instr->GetSrc1()->IsSymOpnd()
                || !instr->GetSrc1()->CanStoreTemp());
        }
    }
    return false;
}
void
NumberTemp::PropagateTempPropertyTransferStoreDependencies(SymID usedSymID, PropertySym * propertySym, BackwardPass * backwardPass)
{
    Assert(!this->nonTempElemLoad);
    upwardExposedMarkTempObjectLiveFields.Clear(propertySym->m_id);

    if (!this->IsInLoop())
    {
        // Don't need to track dependencies outside of loop, as we already marked the
        // use as temp transfer already and we won't have a case where the "dst" is reused again (outside of loop)
        return;
    }

    Assert(this->tempTransferDependencies != nullptr);
    SymID dstSymID = this->GetRepresentativePropertySymId(propertySym, backwardPass);
    AddTransferDependencies(usedSymID, dstSymID, this->tempTransferDependencies);

    Js::PropertyId storedPropertyId = propertySym->m_propertyId;
    // The symbol this properties are transfered to
    BVSparse<JitArenaAllocator> ** pPropertyTransferDependencies = this->propertyIdsTempTransferDependencies->Get(storedPropertyId);
    BVSparse<JitArenaAllocator> * transferDependencies = nullptr;
    if (pPropertyTransferDependencies == nullptr)
    {
        if (elemLoadDependencies.IsEmpty())
        {
            // No dependencies to transfer
            return;
        }

        transferDependencies = &elemLoadDependencies;
    }
    else
    {
        transferDependencies = *pPropertyTransferDependencies;
    }

    BVSparse<JitArenaAllocator> ** pBVSparse = this->tempTransferDependencies->FindOrInsertNew(usedSymID);
    if (*pBVSparse == nullptr)
    {
        *pBVSparse = transferDependencies->CopyNew(this->GetAllocator());
    }
    else
    {
        (*pBVSparse)->Or(transferDependencies);
    }

    if (transferDependencies != &elemLoadDependencies)
    {
        // Always include the element load dependencies as well
        (*pBVSparse)->Or(&elemLoadDependencies);
    }

    // Track the propertySym as well for the case where the dependence is not carried by the use
    // Loop1
    //      o.x = e
    //      Loop2
    //              f = o.x
    //                = f
    //              e = e + blah
    // Here, although we can detect that e and f has dependent relationship, f's life time doesn't cross with e's.
    // But o.x will keep the value of e alive, so e can't be mark temp because o.x is still in use (not f)
    // We will add the property sym int he dependency set and check with the upward exposed mark temp object live fields
    // that we keep track of in NumberTemp
    (*pBVSparse)->Set(propertySym->m_id);
}

SymID
NumberTemp::GetRepresentativePropertySymId(PropertySym * propertySym, BackwardPass * backwardPass)
{
    // Since we don't track alias with objects, all property accesses are all grouped together.
    // Use a single property sym id to represent a propertyId to track dependencies.
    SymID symId;
    Js::PropertyId propertyId = propertySym->m_propertyId;
    if (!backwardPass->numberTempRepresentativePropertySym->TryGetValue(propertyId, &symId))
    {
        symId = propertySym->m_id;
        backwardPass->numberTempRepresentativePropertySym->Add(propertyId, symId);
    }
    return symId;
}

void
NumberTemp::ProcessIndirUse(IR::IndirOpnd * indirOpnd, IR::Instr * instr, BackwardPass * backwardPass)
{
    Assert(backwardPass->DoMarkTempNumbersOnTempObjects());
    if (!NumberTemp::IsTempIndirTransferLoad(instr, backwardPass))
    {
        return;
    }

    bool isTempUse = instr->dstIsTempNumber;
    if (!isTempUse)
    {
        nonTempElemLoad = true;
    }
    else if (this->IsInLoop())
    {
        // We didn't already detect non temp use of this property id. so we should track the dependencies in loops
        IR::Opnd * dstOpnd = instr->GetDst();
        Assert(dstOpnd->IsRegOpnd());
        SymID dstSymID = dstOpnd->AsRegOpnd()->m_sym->m_id;

        // Use the no property id as a place holder for elem dependencies
        AddTransferDependencies(&elemLoadDependencies, dstSymID);
#if DBG_DUMP
        if (NumberTemp::DoTrace(backwardPass))
        {
            Output::Print(L"%s: %8s s%d -> []: ", NumberTemp::GetTraceName(),
                backwardPass->IsPrePass() ? L"Prepass " : L"", dstSymID);
            elemLoadDependencies.Dump();
        }
#endif
    }

#if DBG_DUMP
    if (NumberTemp::DoTrace(backwardPass))
    {
        Output::Print(L"%s: %8s%4sTemp Use ([] )", NumberTemp::GetTraceName(),
            backwardPass->IsPrePass() ? L"Prepass " : L"", isTempUse ? L"" : L"Non ");
        instr->DumpSimple();
    }
#endif
}

void
NumberTemp::ProcessPropertySymUse(IR::SymOpnd * symOpnd, IR::Instr * instr, BackwardPass * backwardPass)
{
    Assert(backwardPass->DoMarkTempNumbersOnTempObjects());

    // We only care about instruction that may transfer the property value
    if (!NumberTemp::IsTempPropertyTransferLoad(instr, backwardPass))
    {
        return;
    }

    PropertySym * propertySym = symOpnd->m_sym->AsPropertySym();
    upwardExposedMarkTempObjectLiveFields.Set(propertySym->m_id);
    if (upwardExposedMarkTempObjectSymsProperties == nullptr)
    {
        upwardExposedMarkTempObjectSymsProperties = HashTable<BVSparse<JitArenaAllocator> *>::New(this->GetAllocator(), 16);
    }
    BVSparse<JitArenaAllocator> ** bv = upwardExposedMarkTempObjectSymsProperties->FindOrInsertNew(propertySym->m_stackSym->m_id);
    if (*bv == nullptr)
    {
        *bv = JitAnew(this->GetAllocator(), BVSparse<JitArenaAllocator>, this->GetAllocator());
    }
    (*bv)->Set(propertySym->m_propertyId);

    SymID propertySymId = this->GetRepresentativePropertySymId(propertySym, backwardPass);
    bool isTempUse = instr->dstIsTempNumber;
    if (!isTempUse)
    {
        // Use of the value is non temp, track the property ID's property representative sym so we don't mark temp
        // assignment to this property on stack objects.
        this->nonTempSyms.Set(propertySymId);
    }
    else if (this->IsInLoop() && !this->nonTempSyms.Test(propertySymId))
    {
        // We didn't already detect non temp use of this property id. so we should track the dependencies in loops
        IR::Opnd * dstOpnd = instr->GetDst();
        Assert(dstOpnd->IsRegOpnd());
        SymID dstSymID = dstOpnd->AsRegOpnd()->m_sym->m_id;
        AddTransferDependencies(propertySym->m_propertyId, dstSymID, this->propertyIdsTempTransferDependencies);
#if DBG_DUMP
        if (NumberTemp::DoTrace(backwardPass))
        {
            Output::Print(L"%s: %8s s%d -> PropId:%d %s: ", NumberTemp::GetTraceName(),
                backwardPass->IsPrePass() ? L"Prepass " : L"", dstSymID, propertySym->m_propertyId,
                backwardPass->func->GetScriptContext()->GetPropertyNameLocked(propertySym->m_propertyId)->GetBuffer());
            (*this->propertyIdsTempTransferDependencies->Get(propertySym->m_propertyId))->Dump();
        }
#endif
    }

#if DBG_DUMP
    if (NumberTemp::DoTrace(backwardPass))
    {
        Output::Print(L"%s: %8s%4sTemp Use (PropId:%d %s)", NumberTemp::GetTraceName(),
            backwardPass->IsPrePass() ? L"Prepass " : L"", isTempUse ? L"" : L"Non ", propertySym->m_propertyId,
            backwardPass->func->GetScriptContext()->GetPropertyNameLocked(propertySym->m_propertyId)->GetBuffer());
        instr->DumpSimple();
    }
#endif
}


bool
NumberTemp::HasExposedFieldDependencies(BVSparse<JitArenaAllocator> * bvTempTransferDependencies, BackwardPass * backwardPass)
{
    if (!DoMarkTempNumbersOnTempObjects(backwardPass))
    {
        return false;
    }
    return bvTempTransferDependencies->Test(&upwardExposedMarkTempObjectLiveFields);
}

bool
NumberTemp::DoMarkTempNumbersOnTempObjects(BackwardPass * backwardPass) const
{
    return backwardPass->DoMarkTempNumbersOnTempObjects() && !this->nonTempElemLoad;
}

#if DBG
void
NumberTemp::Dump(wchar_t const * traceName)
{
    if (nonTempElemLoad)
    {
        Output::Print(L"%s: Has Non Temp Elem Load\n", traceName);
    }
    else
    {
        Output::Print(L"%s: Non Temp Syms", traceName);
        this->nonTempSyms.Dump();
        if (this->propertyIdsTempTransferDependencies != nullptr)
        {
            Output::Print(L"%s: Temp transfer propertyId dependencies:\n", traceName);
            this->propertyIdsTempTransferDependencies->Dump();
        }
    }
}
#endif
//=================================================================================================
// ObjectTemp
//=================================================================================================
bool
ObjectTemp::IsTempUse(IR::Instr * instr, Sym * sym, BackwardPass * backwardPass)
{
    Js::OpCode opcode = instr->m_opcode;

    // If the opcode has implicit call and the profile say we have implicit call, then it is not a temp use
    // TODO: More precise implicit call tracking
    if (instr->HasAnyImplicitCalls()
        &&
        ((backwardPass->currentBlock->loop != nullptr ?
            !GlobOpt::ImplicitCallFlagsAllowOpts(backwardPass->currentBlock->loop) :
            !GlobOpt::ImplicitCallFlagsAllowOpts(backwardPass->func))
        || instr->CallsAccessor())
       )
    {
        return false;
    }

    return IsTempUseOpCodeSym(instr, opcode, sym);
}

bool
ObjectTemp::IsTempUseOpCodeSym(IR::Instr * instr, Js::OpCode opcode, Sym * sym)
{
    // Special case ArgOut_A which communicate information about CallDirect
    switch (opcode)
    {
    case Js::OpCode::ArgOut_A:
        return instr->dstIsTempObject;
    case Js::OpCode::LdFld:
    case Js::OpCode::LdFldForTypeOf:
    case Js::OpCode::LdMethodFld:
    case Js::OpCode::LdFldForCallApplyTarget:
    case Js::OpCode::LdMethodFromFlags:
        return instr->GetSrc1()->AsPropertySymOpnd()->GetObjectSym() == sym;
    case Js::OpCode::InitFld:
        if (Js::PropertyRecord::DefaultAttributesForPropertyId(
                instr->GetDst()->AsPropertySymOpnd()->GetPropertySym()->m_propertyId, true) & PropertyDeleted)
        {
            // If the property record is marked PropertyDeleted, the InitFld will cause a type handler conversion,
            // which may result in creation of a weak reference to the object itself.
            return false;
        }
        // Fall through
    case Js::OpCode::StFld:
    case Js::OpCode::StFldStrict:
        return
            !(instr->GetSrc1() && instr->GetSrc1()->GetStackSym() == sym) &&
            !(instr->GetSrc2() && instr->GetSrc2()->GetStackSym() == sym) &&
            instr->GetDst()->AsPropertySymOpnd()->GetObjectSym() == sym;
    case Js::OpCode::LdElemI_A:
        return instr->GetSrc1()->AsIndirOpnd()->GetBaseOpnd()->m_sym == sym;
    case Js::OpCode::StElemI_A:
    case Js::OpCode::StElemI_A_Strict:
        return instr->GetDst()->AsIndirOpnd()->GetBaseOpnd()->m_sym == sym;
    case Js::OpCode::Memset:
        return instr->GetDst()->AsIndirOpnd()->GetBaseOpnd()->m_sym == sym || instr->GetSrc1()->IsRegOpnd() && instr->GetSrc1()->AsRegOpnd()->m_sym == sym;
    case Js::OpCode::Memcopy:
        return instr->GetDst()->AsIndirOpnd()->GetBaseOpnd()->m_sym == sym || instr->GetSrc1()->AsIndirOpnd()->GetBaseOpnd()->m_sym == sym;

    // Special case FromVar for now until we can allow CallsValueOf opcode to be accept temp use
    case Js::OpCode::FromVar:
        return true;
    }

    // TODO: Currently, when we disable implicit call, we still don't allow valueOf/toString that has no side effects
    // So we shouldn't mark them if we have use of the sym on opcode that does CallsValueOf yet.
    if (OpCodeAttr::CallsValueOf(opcode))
    {
        return false;
    }

    // Mark the symbol as non-tempable if the instruction doesn't allow temp sources,
    // or it is transfered to a non-temp dst
    return (OpCodeAttr::TempObjectSources(opcode)
        && (!OpCodeAttr::TempObjectTransfer(opcode) || instr->dstIsTempObject));
}

bool
ObjectTemp::IsTempTransfer(IR::Instr * instr)
{
    return OpCodeAttr::TempObjectTransfer(instr->m_opcode);
}

bool
ObjectTemp::IsTempProducing(IR::Instr * instr)
{
    Js::OpCode opcode = instr->m_opcode;
    if (OpCodeAttr::TempObjectProducing(opcode))
    {
        return true;
    }

    // TODO: Process NewScObject and CallI with isCtorCall when the ctor is fixed
    return false;
}

bool
ObjectTemp::CanStoreTemp(IR::Instr * instr)
{
    // In order to allow storing temp number on temp objects,
    // We have to make sure that if the instr is marked as dstIsTempObject
    // we will always generate the code to allocate the object on the stack (so no helper call).
    // Currently, we only do this for NewRegEx, NewScObjectSimple, NewScObjectLiteral and
    // NewScObjectNoCtor (where the ctor is inlined).

    // CONSIDER: review lowering of other TempObjectProducing opcode and see if we can always allocate on the stack
    // (for example, NewScArray should be able to, but plain NewScObject can't because the size depends on the
    // number inline slots)
    Js::OpCode opcode = instr->m_opcode;
    if (OpCodeAttr::TempObjectCanStoreTemp(opcode))
    {
        // Special cases where stack allocation doesn't happen
#if ENABLE_REGEX_CONFIG_OPTIONS
        if (opcode == Js::OpCode::NewRegEx && REGEX_CONFIG_FLAG(RegexTracing))
        {
            return false;
        }
#endif
        if (opcode == Js::OpCode::NewScObjectNoCtor)
        {
            if (PHASE_OFF(Js::FixedNewObjPhase, instr->m_func->GetJnFunction()) && PHASE_OFF(Js::ObjTypeSpecNewObjPhase, instr->m_func->GetTopFunc()))
            {
                return false;
            }

            // Only if we have BailOutFailedCtorGuardCheck would we generate a stack object.
            // Otherwise we will call the helper, which will not generate stack object.
            return instr->HasBailOutInfo();
        }

        return true;
    }
    return false;
}

bool
ObjectTemp::CanMarkTemp(IR::Instr * instr, BackwardPass * backwardPass)
{
    // We mark the ArgOut with the call in ProcessInstr, no need to do it here
    return IsTempProducing(instr) || IsTempTransfer(instr);
}

void
ObjectTemp::ProcessBailOnNoProfile(IR::Instr * instr)
{
    Assert(instr->m_opcode == Js::OpCode::BailOnNoProfile);

    // ObjectTemp is done during Backward pass, which hasn't change all succ to BailOnNoProfile
    // to dead yet, so we need to manually clear all the information
    this->nonTempSyms.ClearAll();
    this->tempTransferredSyms.ClearAll();
    if (this->tempTransferDependencies)
    {
        this->tempTransferDependencies->ClearAll();
    }
}

void
ObjectTemp::ProcessInstr(IR::Instr * instr)
{
    if (instr->m_opcode != Js::OpCode::CallDirect)
    {
        return;
    }

    IR::HelperCallOpnd * helper = instr->GetSrc1()->AsHelperCallOpnd();
    switch (helper->m_fnHelper)
    {
        case IR::JnHelperMethod::HelperString_Match:
        case IR::JnHelperMethod::HelperString_Replace:
        {
            // First (non-this) parameter is either an regexp or search string.
            // It doesn't escape.
            IR::Instr * instrArgDef;
            instr->FindCallArgumentOpnd(2, &instrArgDef);
            instrArgDef->dstIsTempObject = true;
            break;
        }

        case IR::JnHelperMethod::HelperRegExp_Exec:
        {
            IR::Instr * instrArgDef;
            instr->FindCallArgumentOpnd(1, &instrArgDef);
            instrArgDef->dstIsTempObject = true;
            break;
        }
    };
}


void
ObjectTemp::SetDstIsTemp(bool dstIsTemp, bool dstIsTempTransferred, IR::Instr * instr, BackwardPass * backwardPass)
{
    Assert(dstIsTemp || !dstIsTempTransferred);

    // ArgOut_A are marked by CallDirect and don't need to be set
    if (instr->m_opcode == Js::OpCode::ArgOut_A)
    {
        return;
    }
    instr->dstIsTempObject = dstIsTemp;

    if (!backwardPass->IsPrePass())
    {
        if (OpCodeAttr::TempObjectProducing(instr->m_opcode))
        {
            backwardPass->func->SetHasMarkTempObjects();
#if DBG_DUMP
            backwardPass->numMarkTempObject += dstIsTemp;
#endif
        }
    }
}

StackSym *
ObjectTemp::GetStackSym(IR::Opnd * opnd, IR::PropertySymOpnd ** pPropertySymOpnd)
{
    StackSym * stackSym = nullptr;
    switch (opnd->GetKind())
    {
        case IR::OpndKindReg:
            stackSym = opnd->AsRegOpnd()->m_sym;
            break;
        case IR::OpndKindSym:
        {
            IR::SymOpnd * symOpnd = opnd->AsSymOpnd();
            if (symOpnd->IsPropertySymOpnd())
            {
                IR::PropertySymOpnd * propertySymOpnd = symOpnd->AsPropertySymOpnd();
                *pPropertySymOpnd = propertySymOpnd;
                stackSym = propertySymOpnd->GetObjectSym();
            }
            else if (symOpnd->m_sym->IsPropertySym())
            {
                stackSym = symOpnd->m_sym->AsPropertySym()->m_stackSym;
            }
            break;
        }
        case IR::OpndKindIndir:
            stackSym = opnd->AsIndirOpnd()->GetBaseOpnd()->m_sym;
            break;
    };
    return stackSym;
}

#if DBG
//=================================================================================================
// ObjectTempVerify
//=================================================================================================
ObjectTempVerify::ObjectTempVerify(JitArenaAllocator * alloc, bool inLoop)
    : TempTrackerBase(alloc, inLoop), removedUpwardExposedUse(alloc)
{
}


bool
ObjectTempVerify::IsTempUse(IR::Instr * instr, Sym * sym, BackwardPass * backwardPass)
{
    Js::OpCode opcode = instr->m_opcode;

    // If the opcode has implicit call and the profile say we have implicit call, then it is not a temp use.
    // TODO: More precise implicit call tracking
    bool isLandingPad = backwardPass->currentBlock->IsLandingPad();
    if (OpCodeAttr::HasImplicitCall(opcode) && !isLandingPad
        &&
        ((backwardPass->currentBlock->loop != nullptr ?
            !GlobOpt::ImplicitCallFlagsAllowOpts(backwardPass->currentBlock->loop) :
            !GlobOpt::ImplicitCallFlagsAllowOpts(backwardPass->func))
        || instr->CallsAccessor())
       )
    {
        return false;
    }

    if (!ObjectTemp::IsTempUseOpCodeSym(instr, opcode, sym))
    {
        // the opcode and sym is not a temp use, just return
        return false;
    }

    // In the backward pass, this would have been a temp use already.  Continue to verify
    // if we have install sufficient bailout on implicit call

    if (isLandingPad || !GlobOpt::MayNeedBailOnImplicitCall(instr, nullptr, nullptr))
    {
        // Implicit call would not happen, or we are in the landing pad where implicit call is disabled.
        return true;
    }

    if (instr->HasBailOutInfo())
    {
        // make sure we have mark the bailout for mark temp object,
        // so that we won't optimize it away in DeadStoreImplicitCalls
        return ((instr->GetBailOutKind() & IR::BailOutMarkTempObject) != 0);
    }

    // Review (ObjTypeSpec): This is a bit conservative now that we don't revert from obj type specialized operations to live cache
    // access even if the operation is isolated.  Once we decide a given instruction is an object type spec candidate, we know it
    // will never need an implicit call, so we could basically do opnd->IsObjTypeSpecOptimized() here, instead.
    if (GlobOpt::IsTypeCheckProtected(instr))
    {
        return true;
    }

    return false;
}

bool
ObjectTempVerify::IsTempTransfer(IR::Instr * instr)
{
    if (ObjectTemp::IsTempTransfer(instr)
        // Add the Ld_I4, and LdC_A_I4 as the forward pass might have changed Ld_A to these
        || instr->m_opcode == Js::OpCode::Ld_I4
        || instr->m_opcode == Js::OpCode::LdC_A_I4)
    {
        if (!instr->dstIsTempObject && instr->GetDst() && instr->GetDst()->IsRegOpnd()
          && instr->GetDst()->AsRegOpnd()->GetValueType().IsNotObject())
        {
            // Globopt has proved that dst is not a object, so this is not really an object transfer.
            // This prevents the case where glob opt turned a Conv_Num to Ld_A and expose additional
            // transfer.
            return false;
        }
        return true;
    }
    return false;
}

bool
ObjectTempVerify::CanMarkTemp(IR::Instr * instr, BackwardPass * backwardPass)
{
    // We mark the ArgOut with the call in ProcessInstr, no need to do it here
    return ObjectTemp::IsTempProducing(instr)
        || IsTempTransfer(instr);
}

void
ObjectTempVerify::ProcessInstr(IR::Instr * instr, BackwardPass * backwardPass)
{
    if (instr->m_opcode == Js::OpCode::InlineThrow)
    {
        // We cannot accurately track mark temp for any upward exposed symbol here
        this->removedUpwardExposedUse.Or(backwardPass->currentBlock->byteCodeUpwardExposedUsed);
        return;
    }

    if (instr->m_opcode != Js::OpCode::CallDirect)
    {
        return;
    }

    IR::HelperCallOpnd * helper = instr->GetSrc1()->AsHelperCallOpnd();
    switch (helper->m_fnHelper)
    {
        case IR::JnHelperMethod::HelperString_Match:
        case IR::JnHelperMethod::HelperString_Replace:
        {
            // First (non-this) parameter is either an regexp or search string
            // It doesn't escape
            IR::Instr * instrArgDef;
            instr->FindCallArgumentOpnd(2, &instrArgDef);
            Assert(instrArgDef->dstIsTempObject);
            break;
        }

        case IR::JnHelperMethod::HelperRegExp_Exec:
        {
            IR::Instr * instrArgDef;
            instr->FindCallArgumentOpnd(1, &instrArgDef);
            Assert(instrArgDef->dstIsTempObject);
            break;
        }
    };
}


void
ObjectTempVerify::SetDstIsTemp(bool dstIsTemp, bool dstIsTempTransferred, IR::Instr * instr, BackwardPass * backwardPass)
{
    Assert(dstIsTemp || !dstIsTempTransferred);

    // ArgOut_A are marked by CallDirect and don't need to be set
    if (instr->m_opcode == Js::OpCode::ArgOut_A)
    {
        return;
    }

    if (OpCodeAttr::TempObjectProducing(instr->m_opcode))
    {
        if (!backwardPass->IsPrePass())
        {
            if (dstIsTemp)
            {
                // Don't assert if we have detected a removed upward exposed use that could
                // expose a new mark temp object. Don't assert if it is set in removedUpwardExposedUse
                bool isBailOnNoProfileUpwardExposedUse =
                    !!this->removedUpwardExposedUse.Test(instr->GetDst()->AsRegOpnd()->m_sym->m_id);
#if DBG
                if (DoTrace(backwardPass) && !instr->dstIsTempObject && !isBailOnNoProfileUpwardExposedUse)
                {
                    Output::Print(L"%s: Missed Mark Temp Object: ", GetTraceName());
                    instr->DumpSimple();
                    Output::Flush();
                }
#endif
                // TODO: Unfortunately we still hit this a lot as we are not accounting for some of the globopt changes
                // to the IR.  It is just reporting that we have missed mark temp object opportunity, so it doesn't
                // indicate a functional failure.  Disable for now.
                // Assert(instr->dstIsTempObject || isBailOnNoProfileUpwardExposedUse);
            }
            else
            {
                // If we have marked the dst is temp in the backward pass, the globopt
                // should have maintained it, and it will be wrong to have detect that it is not
                // temp now in the deadstore pass (whether there is BailOnNoProfile or not)
#if DBG
                if (DoTrace(backwardPass) && instr->dstIsTempObject)
                {
                    Output::Print(L"%s: Invalid Mark Temp Object: ", GetTraceName());
                    instr->DumpSimple();
                    Output::Flush();
                }
#endif
                Assert(!instr->dstIsTempObject);
            }
        }
    }
    else if (IsTempTransfer(instr))
    {
        // Only set the transfer
        instr->dstIsTempObject = dstIsTemp;
    }
    else
    {
        Assert(!dstIsTemp);
        Assert(!instr->dstIsTempObject);
    }

    // clear or transfer the bailOnNoProfile upward exposed use
    if (this->removedUpwardExposedUse.TestAndClear(instr->GetDst()->AsRegOpnd()->m_sym->m_id)
        && IsTempTransfer(instr) && instr->GetSrc1()->IsRegOpnd())
    {
        this->removedUpwardExposedUse.Set(instr->GetSrc1()->AsRegOpnd()->m_sym->m_id);
    }
}

void
ObjectTempVerify::MergeData(ObjectTempVerify * fromData, bool deleteData)
{
    this->removedUpwardExposedUse.Or(&fromData->removedUpwardExposedUse);
}

void
ObjectTempVerify::MergeDeadData(BasicBlock * block)
{
    MergeData(block->tempObjectVerifyTracker, false);
    if (!block->isDead)
    {
        // If there was dead flow to a block that is not dead, it might expose
        // new mark temp object, so all its current used (upwardExposedUsed) and optimized
        // use (byteCodeupwardExposedUsed) might not be trace for "missed" mark temp object
        this->removedUpwardExposedUse.Or(block->upwardExposedUses);
        if (block->byteCodeUpwardExposedUsed)
        {
            this->removedUpwardExposedUse.Or(block->byteCodeUpwardExposedUsed);
        }
    }
}

void
ObjectTempVerify::NotifyBailOutRemoval(IR:: Instr * instr, BackwardPass * backwardPass)
{
    Js::OpCode opcode = instr->m_opcode;
    switch (opcode)
    {
        case Js::OpCode::LdFld:
        case Js::OpCode::LdFldForTypeOf:
        case Js::OpCode::LdMethodFld:
            ((TempTracker<ObjectTempVerify> *)this)->ProcessUse(instr->GetSrc1()->AsPropertySymOpnd()->GetObjectSym(), backwardPass);
            break;
        case Js::OpCode::InitFld:
        case Js::OpCode::StFld:
        case Js::OpCode::StFldStrict:
            ((TempTracker<ObjectTempVerify> *)this)->ProcessUse(instr->GetDst()->AsPropertySymOpnd()->GetObjectSym(), backwardPass);
            break;
        case Js::OpCode::LdElemI_A:
            ((TempTracker<ObjectTempVerify> *)this)->ProcessUse(instr->GetSrc1()->AsIndirOpnd()->GetBaseOpnd()->m_sym, backwardPass);
            break;
        case Js::OpCode::StElemI_A:
            ((TempTracker<ObjectTempVerify> *)this)->ProcessUse(instr->GetDst()->AsIndirOpnd()->GetBaseOpnd()->m_sym, backwardPass);
            break;
    }
}

void
ObjectTempVerify::NotifyReverseCopyProp(IR::Instr * instr)
{
    Assert(instr->GetDst());
    SymID symId = instr->GetDst()->AsRegOpnd()->m_sym->m_id;
    this->removedUpwardExposedUse.Clear(symId);
    this->nonTempSyms.Clear(symId);
}

void
ObjectTempVerify::NotifyDeadStore(IR::Instr * instr, BackwardPass * backwardPass)
{
    // Even if we dead store, simulate the uses
    IR::Opnd * src1 = instr->GetSrc1();
    if (src1)
    {
        IR::PropertySymOpnd * propertySymOpnd;
        StackSym * stackSym = ObjectTemp::GetStackSym(src1, &propertySymOpnd);
        if (stackSym)
        {
            ((TempTracker<ObjectTempVerify> *)this)->ProcessUse(stackSym, backwardPass);
        }

        IR::Opnd * src2 = instr->GetSrc2();
        if (src2)
        {
            stackSym = ObjectTemp::GetStackSym(src2, &propertySymOpnd);
            if (stackSym)
            {
                ((TempTracker<ObjectTempVerify> *)this)->ProcessUse(stackSym, backwardPass);
            }
        }
    }
}

void
ObjectTempVerify::NotifyDeadByteCodeUses(IR::Instr * instr)
{
    if (instr->GetDst())
    {
        SymID symId = instr->GetDst()->AsRegOpnd()->m_sym->m_id;
        this->removedUpwardExposedUse.Clear(symId);
        this->nonTempSyms.Clear(symId);
    }

    IR::ByteCodeUsesInstr *byteCodeUsesInstr = instr->AsByteCodeUsesInstr();
    BVSparse<JitArenaAllocator> * byteCodeUpwardExposedUsed = byteCodeUsesInstr->byteCodeUpwardExposedUsed;
    if (byteCodeUpwardExposedUsed != nullptr)
    {
        this->removedUpwardExposedUse.Or(byteCodeUpwardExposedUsed);
    }
}

bool
ObjectTempVerify::DependencyCheck(IR::Instr * instr, BVSparse<JitArenaAllocator> * bvTempTransferDependencies, BackwardPass * backwardPass)
{
    if (!instr->dstIsTempObject)
    {
        // The instruction is not marked as temp object anyway, no need to do extra check
        return false;
    }

    // Since our algorithm is conservative, there are cases where even though two defs are unrelated, the use will still
    // seem like overlapping and not mark-temp-able
    // For example:
    //         = s6.blah
    //      s1 = LdRootFld
    //      s6 = s1
    //      s1 = NewScObject            // s1 is dependent of s6, and s6 is upward exposed.
    //         = s6.blah
    //      s6 = s1
    // Here, although s1 is mark temp able because the s6.blah use is not related, we only know that s1 is dependent of s6
    // so it looks like s1 may overlap through the iterations.  The backward pass will be able to catch that and not mark temp them

    // However, the globopt may create situation like the above while it wasn't there in the backward phase
    // For example:
    //         = s6.blah
    //      s1 = LdRootFld g
    //      s6 = s1
    //      s1 = NewScObject
    //      s7 = LdRootFld g
    //         = s7.blah                // Globopt copy prop s7 -> s6, creating the example above.
    //      s6 = s1
    // This make it impossible to verify whether we did the right thing using the conservative algorithm.

    // Luckily, this case is very rare (ExprGen didn't hit it with > 100K test cases)
    // So we can use this rather expensive algorithm to find out if any of upward exposed used that we think overlaps
    // really get their value from the marked temp sym or not.

    // See unittest\Object\stackobject_dependency.js (with -maxinterpretcount:1 -off:inline)

    BasicBlock * currentBlock = backwardPass->currentBlock;
    BVSparse<JitArenaAllocator> * upwardExposedUses = currentBlock->upwardExposedUses;
    JitArenaAllocator tempAllocator(L"temp", instr->m_func->m_alloc->GetPageAllocator(), Js::Throw::OutOfMemory);
    BVSparse<JitArenaAllocator> * dependentSyms = bvTempTransferDependencies->AndNew(upwardExposedUses, &tempAllocator);
    BVSparse<JitArenaAllocator> * initialDependentSyms = dependentSyms->CopyNew();
    Assert(!dependentSyms->IsEmpty());
    struct BlockRecord
    {
        BasicBlock * block;
        BVSparse<JitArenaAllocator> * dependentSyms;
    };
    SList<BlockRecord> blockStack(&tempAllocator);
    JsUtil::BaseDictionary<BasicBlock *, BVSparse<JitArenaAllocator> *, JitArenaAllocator> processedSyms(&tempAllocator);

    IR::Instr * currentInstr = instr;
    Assert(instr->GetDst()->AsRegOpnd()->m_sym->IsVar());
    SymID markTempSymId = instr->GetDst()->AsRegOpnd()->m_sym->m_id;
    bool initial = true;
    while (true)
    {
        while (currentInstr != currentBlock->GetFirstInstr())
        {
            if (initial)
            {
                initial = false;
            }
            else if (currentInstr == instr)
            {
                if (dependentSyms->Test(markTempSymId))
                {
                    // One of the dependent sym from the original set get it's value from the current marked temp dst.
                    // The dst definitely cannot be temp because it's lifetime overlaps across iterations.
                    return false;
                }

                // If we have already check the same dependent sym, no need to do it again.
                // It will produce the same result anyway.
                dependentSyms->Minus(initialDependentSyms);
                if (dependentSyms->IsEmpty())
                {
                    break;
                }
                // Add in newly discovered dependentSym so we won't do it again when it come back here.
                initialDependentSyms->Or(dependentSyms);
            }

            if (currentInstr->GetDst() && currentInstr->GetDst()->IsRegOpnd())
            {
                // Clear the def and mark the src if it is transfered.

                // If the dst sym is a type specialized sym, clear the var sym instead.
                StackSym * dstSym = currentInstr->GetDst()->AsRegOpnd()->m_sym;
                if (!dstSym->IsVar())
                {
                    dstSym = dstSym->GetVarEquivSym(nullptr);
                }
                if (dstSym && dependentSyms->TestAndClear(dstSym->m_id) &&
                    IsTempTransfer(currentInstr) && currentInstr->GetSrc1()->IsRegOpnd())
                {
                    // We only really care about var syms uses for object temp.
                    StackSym * srcSym = currentInstr->GetSrc1()->AsRegOpnd()->m_sym;
                    if (srcSym->IsVar())
                    {
                        dependentSyms->Set(srcSym->m_id);
                    }
                }

                if (dependentSyms->IsEmpty())
                {
                    // No more dependent sym, we found the def of all of them we can move on to the next block.
                    break;
                }
            }
            currentInstr = currentInstr->m_prev;
        }

        if (currentBlock->isLoopHeader && !dependentSyms->IsEmpty())
        {
            Assert(currentInstr == currentBlock->GetFirstInstr());
            // If we have try to propagate the symbol through the loop before, we don't need to propagate it again.
            BVSparse<JitArenaAllocator> * currentLoopProcessedSyms = processedSyms.Lookup(currentBlock, nullptr);
            if (currentLoopProcessedSyms == nullptr)
            {
                processedSyms.Add(currentBlock, dependentSyms->CopyNew());
            }
            else
            {
                dependentSyms->Minus(currentLoopProcessedSyms);
                currentLoopProcessedSyms->Or(dependentSyms);
            }
        }

        if (!dependentSyms->IsEmpty())
        {
            Assert(currentInstr == currentBlock->GetFirstInstr());

            FOREACH_PREDECESSOR_BLOCK(predBlock, currentBlock)
            {
                if (predBlock->loop == nullptr)
                {
                    // No need to track outside of loops.
                    continue;
                }
                BlockRecord record;
                record.block = predBlock;
                record.dependentSyms = dependentSyms->CopyNew();
                blockStack.Prepend(record);
            }
            NEXT_PREDECESSOR_BLOCK;
        }
        JitAdelete(&tempAllocator, dependentSyms);

        if (blockStack.Empty())
        {
            // No more blocks. We are done.
            break;
        }

        currentBlock = blockStack.Head().block;
        dependentSyms = blockStack.Head().dependentSyms;
        blockStack.RemoveHead();
        currentInstr = currentBlock->GetLastInstr();
    }

    // All the dependent sym doesn't get their value from the marked temp def, so it can really be marked temp.
#if DBG
    if (DoTrace(backwardPass))
    {
        Output::Print(L"%s: Unrelated overlap mark temp (s%-3d): ", GetTraceName(), markTempSymId);
        instr->DumpSimple();
        Output::Flush();
    }
#endif
    return true;
}
#endif


#if DBG
bool
NumberTemp::DoTrace(BackwardPass * backwardPass)
{
    return PHASE_TRACE(Js::MarkTempNumberPhase, backwardPass->func);
}

bool
ObjectTemp::DoTrace(BackwardPass * backwardPass)
{
    return PHASE_TRACE(Js::MarkTempObjectPhase, backwardPass->func);
}

bool
ObjectTempVerify::DoTrace(BackwardPass * backwardPass)
{
    return PHASE_TRACE(Js::MarkTempObjectPhase, backwardPass->func);
}
#endif

// explicit instantiation
template class TempTracker<NumberTemp>;
template class TempTracker<ObjectTemp>;

#if DBG
template class TempTracker<ObjectTempVerify>;
#endif
