//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

#pragma once

namespace Js
{
    //
    // Walks all the nodes in this BTree in sorted order.
    //

    template<typename Func>
    void SegmentBTree::Walk(Func& func) const
    {
        if (!IsLeaf())
        {
            children[0].Walk(func);
        }

        for (unsigned int i = 0; i < segmentCount; i++)
        {
            Assert(keys[i] == segments[i]->left);

            func(segments[i]);

            if (!IsLeaf())
            {
                children[i + 1].Walk(func);
            }
        }
    }

    template <typename Fn>
    SparseArraySegmentBase *
    JavascriptArray::ForEachSegment(Fn fn) const
    {
        return ForEachSegment(this->head, fn);
    }

    template <typename Fn>
    SparseArraySegmentBase *
    JavascriptArray::ForEachSegment(SparseArraySegmentBase * segment, Fn fn)
    {
        DebugOnly(uint32 lastindex = segment? segment->left : 0);
        SparseArraySegmentBase * current = segment;
        while (current)
        {
            // Verify that all the segment are sorted
            Assert(current->left >= lastindex);
            if (fn(current))
            {
                break;
            }
            DebugOnly(lastindex = current->left + current->length);
            current = current->next;
        }
        return current;
    }

    //
    // Link prev and current. If prev is NULL, make current the head segment.
    //
    template<>
    inline void JavascriptArray::LinkSegments(SparseArraySegment<int>* prev, SparseArraySegment<int>* current)
    {
        if (prev && prev->next == nullptr && SparseArraySegmentBase::IsLeafSegment(prev, this->GetScriptContext()->GetRecycler()))
        {
            prev = this->ReallocNonLeafSegment(prev, current);
        }
        else
        {
            LinkSegmentsCommon(prev, current);
        }
    }

    template<>
    inline void JavascriptArray::LinkSegments(SparseArraySegment<double>* prev, SparseArraySegment<double>* current)
    {
        if (prev && prev->next == nullptr && SparseArraySegmentBase::IsLeafSegment(prev, this->GetScriptContext()->GetRecycler()))
        {
            prev = this->ReallocNonLeafSegment(prev, current);
        }
        else
        {
            LinkSegmentsCommon(prev, current);
        }
    }

    template<typename T>
    inline void JavascriptArray::LinkSegments(SparseArraySegment<T>* prev, SparseArraySegment<T>* current)
    {
        LinkSegmentsCommon(prev, current);
    }

    template<typename T>
    inline SparseArraySegment<T>* JavascriptArray::ReallocNonLeafSegment(SparseArraySegment<T> *seg, SparseArraySegmentBase* nextSeg)
    {
        // Find the segment prior to seg.
        SparseArraySegmentBase *prior = nullptr;
        if (seg != this->head)
        {
            for (prior = this->head; prior->next != seg; prior = prior->next)
            {
                Assert(prior->next);
            }
        }
        Recycler *recycler = this->GetScriptContext()->GetRecycler();
        SparseArraySegment<T> *newSeg = SparseArraySegment<T>::AllocateSegment(recycler, seg->left, seg->length, nextSeg);
        js_memcpy_s(newSeg->elements, sizeof(T) * seg->length, seg->elements, sizeof(T) * seg->length);

        LinkSegmentsCommon(prior, newSeg);
        LinkSegmentsCommon(newSeg, nextSeg);
        if (GetLastUsedSegment() == seg)
        {
            SetLastUsedSegment(newSeg);
        }
        SegmentBTree * segmentMap = GetSegmentMap();
        if (segmentMap)
        {
            segmentMap->SwapSegment(seg->left, seg, newSeg);
        }
        return newSeg;
    }

    /*static*/
    template<typename T, uint InlinePropertySlots>
    __inline SparseArraySegment<typename T::TElement> *JavascriptArray::InitArrayAndHeadSegment(
        T *const array,
        const uint32 length,
        const uint32 size,
        const bool wasZeroAllocated)
    {
        Assert(!array->HasSegmentMap());
        SparseArraySegment<typename T::TElement>* head =
            DetermineInlineHeadSegmentPointer<T, InlinePropertySlots, false>(array);
        if(wasZeroAllocated)
        {
            if(length != 0)
            {
                head->length = length;
            }
            head->size = size;
        }
        else
        {
            new(head) SparseArraySegment<typename T::TElement>(0, length, size);
        }
        array->SetHeadAndLastUsedSegment(head);
        array->SetHasNoMissingValues();
        return head;
    }

    template<typename unitType, typename className>
    __inline className * JavascriptArray::New(Recycler * recycler, DynamicType * type)
    {
        size_t allocationPlusSize;
        uint alignedInlineElementSlots;
        DetermineAllocationSize<className, 0>(
            SparseArraySegmentBase::SMALL_CHUNK_SIZE,
            &allocationPlusSize,
            &alignedInlineElementSlots);
        return RecyclerNewPlusZ(recycler, allocationPlusSize, className, type, alignedInlineElementSlots);
    }

    /*static*/
    template<typename unitType, typename className, uint inlineSlots>
    className* JavascriptArray::New(uint32 length, DynamicType* arrayType, Recycler* recycler)
    {
        CompileAssert(static_cast<PropertyIndex>(inlineSlots) == inlineSlots);
        Assert(DynamicTypeHandler::RoundUpInlineSlotCapacity(static_cast<PropertyIndex>(inlineSlots)) == inlineSlots);

        if(length > SparseArraySegmentBase::HEAD_CHUNK_SIZE)
        {
            // Use empty segment until we try to store something.  Call AllocateHead() at that point.
            return RecyclerNew(recycler, className, length, arrayType);
        }

        size_t allocationPlusSize;
        uint alignedInlineElementSlots;
        className* array;
        if (!length) // zero length - use default head chunk size
        {
            DetermineAllocationSize<className, inlineSlots>(
                SparseArraySegmentBase::HEAD_CHUNK_SIZE,
                &allocationPlusSize,
                &alignedInlineElementSlots);
        }
        else //Small array
        {
            Assert(length <= SparseArraySegmentBase::HEAD_CHUNK_SIZE);
            DetermineAllocationSize<className, inlineSlots>(length, &allocationPlusSize, &alignedInlineElementSlots);
        }

        array = RecyclerNewPlusZ(recycler, allocationPlusSize, className, length, arrayType);
        SparseArraySegment<unitType> *head =
            InitArrayAndHeadSegment<className, inlineSlots>(array, 0, alignedInlineElementSlots, true);
        head->FillSegmentBuffer(0, alignedInlineElementSlots);

        return array;
    }

    //
    // Allocates the segment inline up to the length of SparseArraySegmentBase::INLINE_CHUNK_SIZE. The downside of having the segment
    // inline is that the segment space will never get freed unless the Array is collected.
    //
    /*static*/
    template<typename unitType, typename className, uint inlineSlots>
    className* JavascriptArray::NewLiteral(uint32 length, DynamicType* arrayType, Recycler* recycler)
    {
        CompileAssert(static_cast<PropertyIndex>(inlineSlots) == inlineSlots);
        Assert(DynamicTypeHandler::RoundUpInlineSlotCapacity(static_cast<PropertyIndex>(inlineSlots)) == inlineSlots);

        className* array;
        if(HasInlineHeadSegment(length))
        {
            size_t allocationPlusSize;
            uint alignedInlineElementSlots;
            if(!length)
            {
                DetermineAllocationSize<className, inlineSlots>(
                    SparseArraySegmentBase::SMALL_CHUNK_SIZE,
                    &allocationPlusSize,
                    &alignedInlineElementSlots);
            }
            else
            {
                DetermineAllocationSize<className, inlineSlots>(length, &allocationPlusSize, &alignedInlineElementSlots);
            }

            array = RecyclerNewPlusZ(recycler, allocationPlusSize, className, length, arrayType);

            // An new array's head segment length is initialized to zero despite the array length being nonzero because the segment
            // doesn't have any values to begin with. An array literal though, is initialized with special op-codes that just store
            // the values and don't update the length, so update the length here.
            //
            // An array literal is also guaranteed to be fully initialized, so even though the head segment currently will have
            // missing values (after this update to length), it won't have missing values once the initialization is complete, so
            // maintain the state saying "does not have missing values". Furthermore, since the new array literal is not assigned to
            // a variable until it is fully initialized, there is no way for script code to use the array while it still has missing
            // values.
            SparseArraySegment<unitType> *head =
                InitArrayAndHeadSegment<className, inlineSlots>(array, length, alignedInlineElementSlots, true);

            head->FillSegmentBuffer(length, alignedInlineElementSlots);

            Assert(array->HasNoMissingValues());
            return array;
        }

        size_t allocationPlusSize;
        DetermineAllocationSize<className, inlineSlots>(0, &allocationPlusSize);
        array = RecyclerNewPlusZ(recycler, allocationPlusSize, className, length, arrayType);

        SparseArraySegment<unitType> *seg = SparseArraySegment<unitType>::AllocateLiteralHeadSegment(recycler, length);
        array->SetHeadAndLastUsedSegment(seg);
        array->SetHasNoMissingValues();

        // An new array's head segment length is initialized to zero despite the array length being nonzero because the segment
        // doesn't have any values to begin with. An array literal though, is initialized with special op-codes that just store
        // the values and don't update the length, so update the length here.
        //
        // An array literal is also guaranteed to be fully initialized, so even though the head segment currently will have
        // missing values (after this update to length), it won't have missing values once the initialization is complete, so
        // maintain the state saying "does not have missing values". Furthermore, since the new array literal is not assigned to
        // a variable until it is fully initialized, there is no way for script code to use the array while it still has missing
        // values.
        array->head->length = length;
        return array;
    }

#if ENABLE_COPYONACCESS_ARRAY
    //
    // Allocates the segment inline up to the length of SparseArraySegmentBase::INLINE_CHUNK_SIZE. The downside of having the segment
    // inline is that the segment space will never get freed unless the Array is collected.
    //
    /*static*/
    template<typename unitType, typename className, uint inlineSlots>
    className* JavascriptArray::NewCopyOnAccessLiteral(DynamicType* arrayType, ArrayCallSiteInfo *arrayInfo, FunctionBody *functionBody, const Js::AuxArray<int32> *ints, Recycler* recycler)
    {
        CompileAssert(static_cast<PropertyIndex>(inlineSlots) == inlineSlots);
        Assert(DynamicTypeHandler::RoundUpInlineSlotCapacity(static_cast<PropertyIndex>(inlineSlots)) == inlineSlots);
        Assert(arrayInfo->IsNativeIntArray());

        className* array = RecyclerNewZ(recycler, JavascriptCopyOnAccessNativeIntArray, ints->count, arrayType);
        JavascriptLibrary *lib = functionBody->GetScriptContext()->GetLibrary();

        SparseArraySegment<unitType> *seg;

        if (JavascriptLibrary::IsCachedCopyOnAccessArrayCallSite(functionBody->GetScriptContext()->GetLibrary() , arrayInfo))
        {
            seg = lib->cacheForCopyOnAccessArraySegments->GetSegmentByIndex(arrayInfo->copyOnAccessArrayCacheIndex);
        }
        else
        {
            seg = SparseArraySegment<unitType>::AllocateLiteralHeadSegment(recycler, ints->count);
        }

        if (!JavascriptLibrary::IsCachedCopyOnAccessArrayCallSite(lib, arrayInfo))
        {
            JavascriptOperators::AddIntsToArraySegment(seg, ints);
            arrayInfo->copyOnAccessArrayCacheIndex = lib->cacheForCopyOnAccessArraySegments->AddSegment(seg);
        }
        array->SetHeadAndLastUsedSegment(reinterpret_cast<SparseArraySegmentBase *>(arrayInfo->copyOnAccessArrayCacheIndex)); // storing index in head on purpose: expect AV if treated as other array objects

#if ENABLE_DEBUG_CONFIG_OPTIONS
        if (Js::Configuration::Global.flags.TestTrace.IsEnabled(Js::CopyOnAccessArrayPhase))
        {
            Output::Print(L"Create copy-on-access array: func(#%2d) index(%d) length(%d)\n",
                functionBody->GetFunctionNumber(), lib->cacheForCopyOnAccessArraySegments->GetCount(), ints->count);
            Output::Flush();
        }
#endif

        return array;
    }
#endif

    template<class T, uint InlinePropertySlots>
    __inline T *JavascriptArray::New(
        void *const stackAllocationPointer,
        const uint32 length,
        DynamicType *const arrayType)
    {
        Assert(arrayType);

        if(stackAllocationPointer)
        {
            bool isSufficientSpaceForInlinePropertySlots;
            const uint availableInlineElementSlots =
                DetermineAvailableInlineElementSlots<T, InlinePropertySlots>(
                    T::StackAllocationSize,
                    &isSufficientSpaceForInlinePropertySlots);
            if(isSufficientSpaceForInlinePropertySlots)
            {
                T *const array = new(stackAllocationPointer) T(length, arrayType);
                if(length <= availableInlineElementSlots)
                {
                    SparseArraySegment<typename T::TElement> *const head =
                        InitArrayAndHeadSegment<T, InlinePropertySlots>(array, 0, availableInlineElementSlots, false);
                    head->FillSegmentBuffer(0, availableInlineElementSlots);
                }
                else
                {
                    // Not enough room to allocate all required element slots inline. Leave the head segment as the empty
                    // segment and let it be allocated as necessary.
                }
                Assert(array->HasNoMissingValues());
                return array;
            }
        }

        return New<typename T::TElement, T, InlinePropertySlots>(length, arrayType, arrayType->GetRecycler());
    }

    template<class T, uint InlinePropertySlots>
    __inline T *JavascriptArray::NewLiteral(
        void *const stackAllocationPointer,
        const uint32 length,
        DynamicType *const arrayType)
    {
        Assert(arrayType);

        if(stackAllocationPointer)
        {
            bool isSufficientSpaceForInlinePropertySlots;
            const uint availableInlineElementSlots =
                DetermineAvailableInlineElementSlots<T, InlinePropertySlots>(
                    T::StackAllocationSize,
                    &isSufficientSpaceForInlinePropertySlots);
            if(isSufficientSpaceForInlinePropertySlots)
            {
                T *const array = new(stackAllocationPointer) T(length, arrayType);
                if(length <= availableInlineElementSlots)
                {
                    SparseArraySegment<typename T::TElement> *const head =
                        InitArrayAndHeadSegment<T, InlinePropertySlots>(array, length, availableInlineElementSlots, false);
                    head->FillSegmentBuffer(length, availableInlineElementSlots);
                    Assert(array->HasNoMissingValues());
                    return array;
                }

                // Not enough room to allocate all required element slots inline. Allocate the head segment separately.
                SparseArraySegment<typename T::TElement> *const head =
                    SparseArraySegment<typename T::TElement>::AllocateLiteralHeadSegment(arrayType->GetRecycler(), length);
                array->SetHeadAndLastUsedSegment(head);
                array->SetHasNoMissingValues();
                return array;
            }
        }

        return NewLiteral<typename T::TElement, T, InlinePropertySlots>(length, arrayType, arrayType->GetRecycler());
    }

    template<typename T>
    __inline void JavascriptArray::DirectSetItemAt(uint32 itemIndex, T newValue)
    {
        Assert(itemIndex < InvalidIndex); // Otherwise the code below could overflow and set length = 0

        SparseArraySegment<T> *seg = (SparseArraySegment<T>*)this->GetLastUsedSegment();
        uint32 offset = itemIndex - seg->left;
        if(itemIndex >= seg->left && offset < seg->size)
        {
            DirectSetItemInLastUsedSegmentAt(offset, newValue);
            return;
        }
        DirectSetItem_Full(itemIndex, newValue);
    }

    template<typename T>
    __inline void JavascriptArray::DirectSetItemInLastUsedSegmentAt(const uint32 offset, const T newValue)
    {
        SparseArraySegment<T> *const seg = (SparseArraySegment<T>*)GetLastUsedSegment();
        Assert(seg);
        Assert(offset < seg->size);
        Assert(!(HasNoMissingValues() &&
                 offset < seg->length &&
                 SparseArraySegment<T>::IsMissingItem(&seg->elements[offset]) &&
                 seg == head));

        const bool scanForMissingValues = NeedScanForMissingValuesUponSetItem(seg, offset);

        DebugOnly(VerifyNotNeedMarshal(newValue));
        seg->elements[offset] = newValue;
        if (offset >= seg->length)
        {
            if(offset > seg->length && seg == head)
            {
                SetHasNoMissingValues(false);
            }

            seg->length = offset + 1;
            const uint32 itemIndex = seg->left + offset;
            if (this->length <= itemIndex)
            {
                this->length = itemIndex  + 1;
            }
        }
        else if(scanForMissingValues)
        {
            ScanForMissingValues<T>();
        }
    }

#if ENABLE_PROFILE_INFO
    template<typename T>
    __inline void JavascriptArray::DirectProfiledSetItemInHeadSegmentAt(
        const uint32 offset,
        const T newValue,
        StElemInfo *const stElemInfo)
    {
        SparseArraySegment<T> *const seg = (SparseArraySegment<T>*)head;
        Assert(seg);
        Assert(offset < seg->size);
        Assert(!(HasNoMissingValues() &&
                 offset < seg->length &&
                 SparseArraySegment<T>::IsMissingItem(&seg->elements[offset])));
        Assert(stElemInfo);

        stElemInfo->filledMissingValue = offset < seg->length && SparseArraySegment<T>::IsMissingItem(&seg->elements[offset]);
        const bool scanForMissingValues = NeedScanForMissingValuesUponSetItem(seg, offset);

        DebugOnly(VerifyNotNeedMarshal(newValue));
        seg->elements[offset] = newValue;
        if (offset >= seg->length)
        {
            if(offset > seg->length)
            {
                SetHasNoMissingValues(false);
            }

            seg->length = offset + 1;
            const uint32 itemIndex = seg->left + offset;
            if (this->length <= itemIndex)
            {
                this->length = itemIndex  + 1;
            }
        }
        else if(scanForMissingValues)
        {
            ScanForMissingValues<T>();
        }
    }
#endif

    template<typename T>
    inline BOOL JavascriptArray::DirectGetItemAt(uint32 index, T* outVal)
    {
#ifdef VALIDATE_ARRAY
        ValidateArray();
#endif

        if (index >= length)
        {
            return false;
        }

#ifdef VALIDATE_ARRAY
        T v_btree = NULL;
        SparseArraySegmentBase* seg_btree = nullptr;
        bool first_pass = true;
#endif

        SparseArraySegmentBase* nextSeg;
        SegmentBTreeRoot * segmentMap = GetSegmentMap();
        if (segmentMap)
        {
            SparseArraySegmentBase* matchOrNextSeg;
            segmentMap->Find(index, nextSeg, matchOrNextSeg);

            if (!nextSeg)
            {
                nextSeg = matchOrNextSeg;
            }
        }
        else
        {
#ifdef VALIDATE_ARRAY
SECOND_PASS:
#endif
            nextSeg = this->GetBeginLookupSegment(index, false);
        }
        uint probeCost = 0;
        while (nextSeg != nullptr && nextSeg->left <= index)
        {
            uint32 limit =  nextSeg->left + nextSeg->length;
            if (index < limit)
            {
                T* v = &((SparseArraySegment<T>*)nextSeg)->elements[index - nextSeg->left];

                this->SetLastUsedSegment(nextSeg);

#ifdef VALIDATE_ARRAY
                Assert(segmentMap == GetSegmentMap());
                if (segmentMap && first_pass)
                {
                    v_btree = *v;
                    seg_btree= nextSeg;
                    first_pass = false;
                    goto SECOND_PASS;
                }
                else if (segmentMap && !first_pass)
                {
                    Assert(seg_btree == nextSeg);
                }
#endif
                if (SparseArraySegment<T>::IsMissingItem(v))
                {
                    Assert(!(HasNoMissingValues() && nextSeg == head));
                    return false;
                }
                *outVal = *v;
                return true;
            }
            nextSeg = nextSeg->next;
            Assert(segmentMap == GetSegmentMap());
            if (!segmentMap)
            {
                probeCost++;
                if (probeCost > SegmentBTree::GetLazyCrossOverLimit() && this->head != EmptySegment)
                {
                    // Build a SegmentMap
                    segmentMap = BuildSegmentMap();

                    // Find the right segment
                    SparseArraySegmentBase* matchOrNextSeg;
                    segmentMap->Find(index, nextSeg, matchOrNextSeg);
                    if (!nextSeg)
                    {
                        nextSeg = matchOrNextSeg;
                    }
                }
            }
        }

#ifdef VALIDATE_ARRAY
        Assert(segmentMap == GetSegmentMap());
        if (segmentMap && first_pass)
        {
            v_btree = NULL;
            seg_btree= nullptr;
            first_pass = false;
            goto SECOND_PASS;
        }
        else if (segmentMap && !first_pass)
        {
            Assert(v_btree == NULL && seg_btree == nullptr);
        }
#endif

        return false;
    }

    template<typename T>
    void JavascriptArray::EnsureHead()
    {
        if (this->head == EmptySegment)
        {
            this->AllocateHead<T>();
        }
    }

    template<typename T>
    void JavascriptArray::AllocateHead()
    {
        Recycler* recycler = GetRecycler();
        uint32 allocLength;

        Assert(this->head == EmptySegment);

        if (this->length)
        {
            allocLength = this->length <= MaxInitialDenseLength ? this->length : SparseArraySegmentBase::HEAD_CHUNK_SIZE;
            this->head = SparseArraySegment<T>::AllocateSegment(recycler, 0, 0, allocLength, nullptr);
        }
        else
        {
            allocLength = SparseArraySegmentBase::HEAD_CHUNK_SIZE;
            this->head = SparseArraySegment<T>::AllocateSegment(recycler, 0, 0, allocLength, nullptr);
        }
        this->SetLastUsedSegment(this->head);
        SetHasNoMissingValues();
    }

    template<typename T>
    SparseArraySegment<T>* JavascriptArray::PrepareSegmentForMemOp(uint32 startIndex, uint32 length)
    {
        uint32 endIndex;
        if(UInt32Math::Add(startIndex, length - 1, &endIndex))
        {
            JavascriptError::ThrowRangeError(this->GetScriptContext(), JSERR_ArrayLengthAssignIncorrect);
        }
        if (endIndex >= this->length)
        {
            if (endIndex < JavascriptArray::InvalidIndex)
            {
                this->length = endIndex + 1;
            }
            else
            {
                JavascriptError::ThrowRangeError(this->GetScriptContext(), JSERR_ArrayLengthAssignIncorrect);
            }
        }

        this->EnsureHead<T>();

        Recycler* recycler = GetRecycler();

        //Find the segment where itemIndex is present or is at the boundary
        SparseArraySegment<T>* current = (SparseArraySegment<T>*)this->GetBeginLookupSegment(startIndex, false);

        SparseArraySegmentBase* prev = nullptr;
        SparseArraySegmentBase* startSeg = nullptr;
        SparseArraySegmentBase* endSeg = nullptr;
        SparseArraySegmentBase* startPrev = nullptr;
        uint32 growby, startOffset, endOffset;

        const auto FindStartAndEndSegment = [&]()
        {
            if (endIndex >= current->left + current->size)
            {
                current = (SparseArraySegment<T>*)head;
            }
            else
            {
                startSeg = endSeg = current;
                current = nullptr;
            }

            while (current != nullptr)
            {
                startOffset = startIndex - current->left;
                endOffset = endIndex - current->left;
                if (!startSeg)
                {
                    if (startIndex <= current->left)
                    {
                        startPrev = prev;
                        startSeg = current;
                    }
                    else if (startOffset <= current->size)
                    {
                        if ((nullptr == current->next) || (startIndex < current->next->left))
                        {
                            startPrev = prev;
                            startSeg = current;
                        }
                    }
                }
                if (!endSeg)
                {
                    if (endIndex <= current->left)
                    {
                        endSeg = current;
                        break;
                    }
                    else if (endOffset <= current->size)
                    {
                        if ((nullptr == current->next) || (endIndex < current->next->left))
                        {
                            endSeg = current;
                            break;
                        }
                    }
                }
                prev = current;
                current = (SparseArraySegment<T>*)current->next;
            }
            if (!startSeg && !endSeg)
            {
                startPrev = prev;
            }
        };

        const auto ResizeArrayIfStartIsOutsideArrayLength = [&]()
        {
            Assert(endSeg == nullptr);
            Assert(startIndex >= head->size);
            // Reallocate head if it meets a heuristics
            if (startPrev == head                                               // prev segment is the head segment
                && !head->next                                                  // There is only one head segment in the array
                && startIndex - head->size <= MergeSegmentsLengthHeuristics     // Distance to next index is relatively small
                )
            {
                current = ((Js::SparseArraySegment<T>*)head)->GrowByMin(recycler, startIndex + length - head->size);
                current->length = endIndex + 1;
                head = current;
                SetHasNoMissingValues(false);
            }
            else
            {
                //itemIndex is greater than the (left + size) of last segment in the linked list
                current = SparseArraySegment<T>::AllocateSegment(recycler, startIndex, length, (SparseArraySegment<T> *)nullptr);
                LinkSegments((Js::SparseArraySegment<T>*)startPrev, current);
                current->length = length;
                if (current == head)
                {
                    Assert(startIndex == 0);
                    Assert(current->length == length);
                    SetHasNoMissingValues();
                }
            }
        };

        const auto ExtendStartSegmentForMemOp = [&]()
        {
            startOffset = startIndex - startSeg->left;
            if ((startIndex >= startSeg->left) && (startOffset < startSeg->size))
            {
                // startIndex is within startSeg
                if ((startOffset + length) > startSeg->size)
                {
                    // if we don't have enough space in startSeg
                    growby = length - (startSeg->size - startOffset);
                    current = ((Js::SparseArraySegment<T>*)startSeg)->GrowByMin(recycler, growby);
                    LinkSegments((Js::SparseArraySegment<T>*)startPrev, current);
                    if (current == head)
                    {
                        if (startIndex > current->length)
                        {
                            // if it's the head segment and memset starts after the segment length, missing value is introduced
                            SetHasNoMissingValues(false);
                        }
                        else if (!HasNoMissingValues())
                        {
                            // Have we overwritten all the missing values?
                            if (!ScanForMissingValues<T>(0, startOffset))
                            {
                                SetHasNoMissingValues();
                            }
                        }
                    }
                    current->length = startOffset + length;
                }
                else
                {
                    // if we have enough space in the startseg
                    current = (Js::SparseArraySegment<T>*)startSeg;
                    if (current == head)
                    {
                        if (startIndex > current->length)
                        {
                            // if it's the head segment and memset starts after the segment length, missing value is introduced
                            SetHasNoMissingValues(false);
                        }
                        else if (!HasNoMissingValues())
                        {
                            // Have we overwritten all the missing values?
                            if (!ScanForMissingValues<T>(0, startOffset))
                            {
                                SetHasNoMissingValues();
                            }
                        }
                    }
                    current->length = current->length >  (startOffset + length) ? current->length : (startOffset + length);
                }
            }
            else if ((startIndex + 1) <= startSeg->left)
            {
                if (startIndex + 1 == startSeg->left && startPrev == head)
                {
                    current = ((Js::SparseArraySegment<T>*)head)->GrowByMin(recycler, startIndex + length - head->size);
                    current->length = endIndex + 1;
                    head = current;
                }
                else
                {
                    // startIndex is in between prev and startIndex
                    current = SparseArraySegment<T>::AllocateSegment(recycler, startIndex, length, (SparseArraySegment<T> *)nullptr);
                    LinkSegments((Js::SparseArraySegment<T>*)startPrev, current);
                    if (current == head)
                    {
                        SetHasNoMissingValues();
                    }
                    current->length = length;
                }
            }
            else
            {
                Assert(startIndex == startSeg->left + startSeg->size);

                current = ((Js::SparseArraySegment<T>*)startSeg)->GrowByMin(recycler, length);
                LinkSegments((Js::SparseArraySegment<T>*)startPrev, current);

                if (current == head)
                {
                    if (startIndex > current->length)
                    {

                        // if it's the head segment and memset starts after the segment length, missing value is introduced
                        SetHasNoMissingValues(false);
                    }
                }
                current->length = startOffset + length;
            }

            startSeg = current;
        };

        const auto AppendLeftOverItemsFromEndSegment = [&]()
        {
            if (!endSeg)
            {
                // end is beyond the length of the array
                Assert(endIndex == this->length - 1);
                current->next = nullptr;
            }
            else
            {
                endOffset = endIndex - endSeg->left;
                startOffset = startIndex - current->left;
                if ((endIndex >= endSeg->left) && (endOffset < endSeg->size))
                {
                    // endIndex is within endSeg
                    if (endSeg->length - 1 > endOffset)
                    {
                        if (startSeg != endSeg)
                        {
                            // we have some leftover items on endseg
                            growby = (endSeg->length - endOffset - 1);
                            current = ((Js::SparseArraySegment<T>*)current)->GrowByMin(recycler, growby);
                            js_memcpy_s(((Js::SparseArraySegment<T>*)current)->elements + startOffset + length, sizeof(T)* growby, ((Js::SparseArraySegment<T>*)endSeg)->elements + endOffset + 1, sizeof(T)* growby);
                            LinkSegments((Js::SparseArraySegment<T>*)startPrev, current);
                            current->length = startOffset + length + growby;
                        }
                        if (current == head && HasNoMissingValues())
                        {
                            if (ScanForMissingValues<T>(startOffset + length, current->length))
                            {
                                SetHasNoMissingValues(false);
                            }
                        }
                    }
                    current->next = endSeg->next;
                }
                else if ((endIndex + 1) <= endSeg->left)
                {
                    // endIndex is between endSeg and the segment before
                    if (endIndex + 1 == endSeg->left && current == head)
                    {

                        // extend current to hold endSeg
                        growby = endSeg->length;
                        current = ((Js::SparseArraySegment<T>*)current)->GrowByMin(recycler, growby);
                        js_memcpy_s(((Js::SparseArraySegment<T>*)current)->elements + endIndex + 1, sizeof(T)* endSeg->length, ((Js::SparseArraySegment<T>*)endSeg)->elements, sizeof(T)* endSeg->length);
                        LinkSegments((Js::SparseArraySegment<T>*)startPrev, current);
                        if (HasNoMissingValues())
                        {
                            if (ScanForMissingValues<T>(endIndex + 1, endIndex + growby))
                            {
                                SetHasNoMissingValues(false);
                            }
                        }
                        current->length = endIndex + growby + 1;
                        current->next = endSeg->next;
                    }
                    else
                    {
                        current->next = endSeg;
                    }
                }
                else
                {
                    //endIndex is at the boundary of endSeg segment at the left + size
                    Assert(endIndex == endSeg->left + endSeg->size);
                    current->next = endSeg->next;
                }
            }
        };
        FindStartAndEndSegment();
        if (startSeg == nullptr)
        {
            // if start index is greater than array length then we can add a new segment (or extend the last segment based on some heuristics)
            ResizeArrayIfStartIsOutsideArrayLength();
        }
        else
        {
            // once we found the start segment we extend the start segment until startIndex+length . We don't care about what was there
            // as they will be overwritten by the memset/ memcopy. Then we need to append items from the (startIndex+length) to array.length
            // from the end segment to the new array
            ExtendStartSegmentForMemOp();
            AppendLeftOverItemsFromEndSegment();
        }

        Assert(current);
        Assert(current->left <= startIndex);
        Assert((startIndex - current->left) < current->size);
        return current;
    }

    template<typename T>
    void JavascriptArray::DirectSetItemAtRangeFromArray(uint32 toStartIndex, uint32 length, JavascriptArray *fromArray, uint32 fromStartIndex)
    {
        if (length == 0)
        {
            return;
        }

        bool isBtree = false;

#ifdef ENABLE_DEBUG_CONFIG_OPTIONS
        isBtree = Js::Configuration::Global.flags.ForceArrayBTree;
#endif
        if (GetSegmentMap() || fromArray->GetSegmentMap() || isBtree)
        {
            for (uint i = 0; i < length; i++)
            {
                T val;
                if (fromArray->DirectGetItemAt(fromStartIndex + i, &val))
                {
                    DirectSetItem_Full(toStartIndex + i, val);
                }
            }
            return;
        }

        SparseArraySegment<T> *toSegment = PrepareSegmentForMemOp<T>(toStartIndex, length);

        Assert(fromArray->head);
        Assert(fromArray->length >= (fromStartIndex + length));


        //Find the segment where itemIndex is present or is at the boundary
        SparseArraySegmentBase* current = fromArray->GetBeginLookupSegment(fromStartIndex, false);
        Assert(current);
        Assert(GetSegmentMap() == nullptr  && fromArray->GetSegmentMap() == nullptr);

        while (current && length)
        {
            int memcopySize = length;
            int startOffset;
            if (fromStartIndex >= current->left && fromStartIndex < (current->left + current->length))
            {
                startOffset = fromStartIndex - current->left;
                if ((startOffset + length) > current->length)
                {
                    memcopySize = current->length - startOffset;
                }


                js_memcpy_s(toSegment->elements + toStartIndex, memcopySize * sizeof(T), (((Js::SparseArraySegment<T>*)current)->elements + startOffset), memcopySize * sizeof(T));

                fromArray->SetLastUsedSegment(current);
                fromStartIndex = fromStartIndex + memcopySize;
                toStartIndex = toStartIndex + memcopySize;
                length = length - memcopySize;

            }
            current = current->next;
        }

        Assert(length == 0);

        this->SetLastUsedSegment(toSegment);
#if DBG
        if (Js::Configuration::Global.flags.MemOpMissingValueValidate)
        {
            if (toSegment == head)
            {
                Assert(ScanForMissingValues<T>(0, this->length) != HasNoMissingValues());
            }
        }
#endif
    }
    template<typename T>
    void JavascriptArray::DirectSetItemAtRange(uint32 startIndex, uint32 length, T newValue)
    {
        if (startIndex == 0 && head != EmptySegment && length < head->size)
        {
            if (newValue == (T)0 || newValue == (T)(-1))
            {
                memset(((Js::SparseArraySegment<T>*)head)->elements, ((int)(intptr_t)newValue), sizeof(T)* length);
            }
            else
            {
                Js::SparseArraySegment<T>* headSegment = ((Js::SparseArraySegment<T>*)head);
                for (uint32 i = 0; i < length; i++)
                {
                    headSegment->elements[i] = newValue;
                }
            }

            if (length > this->length)
            {
                this->length = length;
            }

            if (length > head->length)
            {
                head->length = length;
            }

            if (!HasNoMissingValues())
            {
                if (ScanForMissingValues<T>(length, head->length) == false)
                {
                    SetHasNoMissingValues(true);
                }
            }
            this->SetLastUsedSegment(head);
        }
        else if (startIndex == 0 && length > this->length && (head == EmptySegment || length > head->size))
        {

            Recycler *recycler = GetRecycler();
            this->length = length;
            this->EnsureHead<T>();
            SparseArraySegmentBase* current = nullptr;

            Assert(head->size < length);

            current = SparseArraySegment<T>::AllocateSegment(recycler, 0, length, (SparseArraySegment<T> *)nullptr);
            this->SetHeadAndLastUsedSegment(current);

            Assert(!HasSegmentMap());

            SetHasNoMissingValues(true);

            if (newValue == (T)0 || newValue == (T)(-1))
            {
                memset(((Js::SparseArraySegment<T>*)current)->elements, ((int)(intptr_t)newValue), sizeof(T)* length);
            }
            else
            {
                for (uint32 i = 0; i < length; i++)
                {
                    ((Js::SparseArraySegment<T>*)current)->elements[i] = newValue;
                }
            }
            this->SetLastUsedSegment(current);
        }
        else
        {
            DirectSetItemAtRangeFull<T>(startIndex, length, newValue);
        }
    }

    template<typename T>
    void JavascriptArray::DirectSetItemAtRangeFull(uint32 startIndex, uint32 length, T newValue)
    {
        if (length == 0)
        {
            return;
        }

        bool isBtree = false;

#ifdef ENABLE_DEBUG_CONFIG_OPTIONS
        isBtree = Js::Configuration::Global.flags.ForceArrayBTree;
#endif
        if (GetSegmentMap() || isBtree)
        {
            for (uint i = startIndex; i < startIndex + length; i++)
            {
                DirectSetItem_Full<T>(i, newValue);
            }
            return;
        }

        SparseArraySegment<T> *current = PrepareSegmentForMemOp<T>(startIndex, length);

        if (newValue == (T)0 || newValue == (T)(-1))
        {
            memset((((Js::SparseArraySegment<T>*)current)->elements + (startIndex - current->left)), ((int)(intptr_t)newValue), sizeof(T)* length);
        }
        else
        {
            for (uint32 i = 0; i < length; i++)
            {
                ((Js::SparseArraySegment<T>*)current)->elements[startIndex - current->left + i] = newValue;
            }
        }
        this->SetLastUsedSegment(current);
#if DBG
        if (Js::Configuration::Global.flags.MemOpMissingValueValidate)
        {
            if (current == head)
            {
                Assert(ScanForMissingValues<T>(0, this->length) != HasNoMissingValues());
            }
        }
#endif

    }

    template<typename T>
    void JavascriptArray::DirectSetItem_Full(uint32 itemIndex, T newValue)
    {
        DebugOnly(VerifyNotNeedMarshal(newValue));
        this->EnsureHead<T>();
        AnalysisAssert(head);

#ifdef VALIDATE_ARRAY
        ValidateArray();
#endif

        if (itemIndex >= this->length)
        {
            if (itemIndex != JavascriptArray::InvalidIndex)
            {
                this->length = itemIndex + 1;
            }
            else
            {
                JavascriptError::ThrowRangeError(this->GetScriptContext(), JSERR_ArrayLengthAssignIncorrect);
            }
        }

        Recycler* recycler = GetRecycler();

        //Find the segment where itemIndex is present or is at the boundary
        SparseArraySegment<T>* current = (SparseArraySegment<T>*)this->GetBeginLookupSegment(itemIndex, false);
        // If it doesn't fit in current chunk (watch for overflow), start from beginning as we'll
        // need the prev
        if (current->left + current->size > current->left || itemIndex >= current->left + current->size)
        {
            current = (SparseArraySegment<T>*)head;
        }
        SparseArraySegmentBase* prev = nullptr;

#ifdef VALIDATE_ARRAY
        SparseArraySegmentBase* current_btree = nullptr;
        SparseArraySegmentBase* prev_btree = nullptr;
        bool first_pass = true;
#endif

        SegmentBTreeRoot * segmentMap = GetSegmentMap();
        if (segmentMap)
        {
            SparseArraySegmentBase* prevSeg = nullptr;
            SparseArraySegmentBase* currentBase = current;
            segmentMap->Find(itemIndex, prevSeg, currentBase);
            current = (SparseArraySegment<T>*)currentBase;
            Assert(!prevSeg || prevSeg->next == current);
            if (prevSeg)
            {
                bool noExactMatch = !current || itemIndex < current->left;
                Assert(prevSeg->left + prevSeg->size >= prevSeg->left);
                bool extendPrevSeg = itemIndex <= prevSeg->left + prevSeg->size;
                if (noExactMatch && extendPrevSeg)
                {
                    current = (SparseArraySegment<T>*)head;
                    prev = nullptr;
                    if (prevSeg != head)
                    {
                        // Since we are going to extend prevSeg we need the
                        // address of it's left neighbor's next pointer
                        currentBase = current;
                        segmentMap->Find(prevSeg->left, prevSeg, currentBase);
                        current = (SparseArraySegment<T>*)currentBase;
                        Assert(prevSeg && prevSeg->next == current);
                        prev = prevSeg;
                    }
                }
                else
                {
                    prev = prevSeg;
                }
            }
            else
            {
                Assert(current == head);
            }


#ifdef VALIDATE_ARRAY
SECOND_PASS:
            if (!first_pass)
            {
                current = (SparseArraySegment<T>*)this->GetBeginLookupSegment(itemIndex, false);
                // If it doesn't fit in current chunk (watch for overflow), start from beginning as we'll
                // need the prev
                if (current->left + current->size > current->left || itemIndex >= current->left + current->size)
                {
                    current = (SparseArraySegment<T>*)head;
                }
                prev = nullptr;
            }
#endif
        }

        uint probeCost = 0;
        while(current != nullptr)
        {
            uint32 offset = itemIndex - current->left;
            if (itemIndex < current->left)
            {
                break;
            }
            else if (offset <= current->size)
            {
                if ((nullptr == current->next) || (itemIndex < current->next->left))
                {
                    break;
                }
            }
            prev = current;
            current = (SparseArraySegment<T>*)current->next;
            Assert(segmentMap == GetSegmentMap());
            if (!segmentMap)
            {
                probeCost++;
                if (probeCost > SegmentBTree::GetLazyCrossOverLimit())
                {
                    // Build a SegmentMap
                    segmentMap = BuildSegmentMap();

                    SparseArraySegmentBase* prevSeg = nullptr;
                    SparseArraySegmentBase* currentBase = current;
                    segmentMap->Find(itemIndex, prevSeg, currentBase);
                    current = (SparseArraySegment<T>*)currentBase;
                    Assert(prevSeg->next == current);
                    if (prevSeg)
                    {
                        bool noExactMatch = !current || itemIndex < current->left;
                        Assert(prevSeg->left + prevSeg->size >= prevSeg->left);
                        bool extendPrevSeg = itemIndex <= prevSeg->left + prevSeg->size;
                        if (noExactMatch && extendPrevSeg)
                        {
                            current = (SparseArraySegment<T>*)head;
                            prev = nullptr;
                            if (prevSeg != head)
                            {
                                // Since we are going to extend prevSeg we need the
                                // address of its left neighbor's next pointer
                                currentBase = current;
                                segmentMap->Find(prevSeg->left, prevSeg, currentBase);
                                current = (SparseArraySegment<T>*)currentBase;
                                Assert(prevSeg->next == current);
                                prev = prevSeg;
                            }
                        }
                        else
                        {
                            prev = prevSeg;
                        }
                    }
                    else
                    {
                        Assert(current == head);
                    }
                }
            }
        }

#ifdef VALIDATE_ARRAY
        Assert(segmentMap == GetSegmentMap());
        if (segmentMap && first_pass)
        {
            current_btree = current;
            prev_btree = prev;
            first_pass = false;
            goto SECOND_PASS;
        }
        else if (segmentMap)
        {
            Assert(current_btree == current && prev_btree == prev);
        }
#endif


        if (current != nullptr)
        {
            uint32 offset = itemIndex - current->left;
            if ((itemIndex >= current->left) && (offset < current->size))
            {
                //itemIndex lies in the segment
                Assert(!(HasNoMissingValues() &&
                         offset < current->length &&
                         SparseArraySegment<T>::IsMissingItem(&current->elements[offset]) &&
                         current == head));

                if(offset > current->length && current == head)
                {
                    SetHasNoMissingValues(false);
                }

                const bool scanForMissingValues = NeedScanForMissingValuesUponSetItem(current, offset);

                ((SparseArraySegment<T>*)current)->SetElement(recycler, itemIndex, newValue);

                if(scanForMissingValues)
                {
                    ScanForMissingValues<T>();
                }
            }
            else if ((itemIndex + 1) < current->left)
            {
                //itemIndex lies in between current and previous segment
                SparseArraySegment<T>* newSeg = SparseArraySegment<T>::AllocateSegment(recycler, prev, itemIndex);
                newSeg->SetElement(recycler, itemIndex, newValue);

                newSeg->next = current;
                LinkSegments((SparseArraySegment<T>*)prev, newSeg);
                current = newSeg;
                TryAddToSegmentMap(recycler, newSeg);

                Assert(current != head);
            }
            else
            {
                //itemIndex is at boundary of current segment either at the left + size or at left - 1;
                Assert((itemIndex == current->left + current->size) || (itemIndex + 1 == current->left));

                SparseArraySegment<T>* next = (SparseArraySegment<T>*)current->next;

                Assert(segmentMap == GetSegmentMap());
                if (!segmentMap && next != nullptr && (itemIndex + 1) == next->left)
                {
                    // Don't merge segments if we are using a segmentMap

                    //Special case where we need to merge two segments. itemIndex is on the size boundary
                    //of the current segment & left boundary of the next

                    const bool currentWasFull = current->length == current->size;

                    Assert(itemIndex == current->left + current->size);
                    current = SparseArraySegment<T>::CopySegment(recycler, (SparseArraySegment<T>*)current, next->left, next, next->left, next->length);
                    current->next = next->next;
                    current->SetElement(recycler, itemIndex, newValue);

                    LinkSegments((SparseArraySegment<T>*)prev, current);

                    if(HasNoMissingValues() && current == head)
                    {
                        // We just merged the head segment and its next segment and filled the only missing value in-between the
                        // two segments. We already know that the previous head segment does not have any missing values. If the
                        // previous head segment was full, scan the new head segment starting from the merge point for missing
                        // values. If the previous head segment was not full, then merging the segments would have created
                        // missing values.
                        SetHasNoMissingValues(false);
                        if(currentWasFull)
                        {
                            ScanForMissingValues<T>(offset + 1);
                        }
                    }
                }
                else
                {
                    if(offset > current->length && current == head)
                    {
                        SetHasNoMissingValues(false);
                    }

                    const bool currentWasHead = current == head;
                    SparseArraySegmentBase* oldSegment = current;
                    uint originalKey = oldSegment->left;

                    current = current->SetElementGrow(recycler, prev, itemIndex, newValue);
                    Assert(segmentMap == GetSegmentMap());
                    if (segmentMap)
                    {
                        segmentMap->SwapSegment(originalKey, oldSegment, current);
                    }

                    LinkSegments((SparseArraySegment<T>*)prev, current);

                    // Scan for missing values when the current segment was grown at the beginning and made the head segment
                    if(!currentWasHead && current == head)
                    {
                        ScanForMissingValues<T>();
                    }
                }
            }
        }
        else
        {
            // Reallocate head if need it meets a heuristics
            Assert(itemIndex >= head->size);
            if (prev == head                                    // prev segment is the head segment
                && !head->next                                  // There is only one head segment in the array
                && !segmentMap                                  // There is no segmentMap which makes sure that array is not highly fragmented.
                && itemIndex - head->size <= MergeSegmentsLengthHeuristics  // Distance to next index is relatively small
               )
            {
                current = ((Js::SparseArraySegment<T>*)head)->GrowByMin(recycler, itemIndex + 1 - head->size);
                current->elements[itemIndex] = newValue;
                current->length =  itemIndex + 1;

                head = current;

                SetHasNoMissingValues(false);
            }
            else
            {
                //itemIndex is greater than the (left + size) of last segment in the linked list
                current = SparseArraySegment<T>::AllocateSegment(recycler, itemIndex, 1, (SparseArraySegment<T> *)nullptr);
                current->SetElement(recycler, itemIndex, newValue);
                LinkSegments((SparseArraySegment<T>*)prev, current);
                TryAddToSegmentMap(recycler, current);

                if(current == head)
                {
                    Assert(itemIndex == 0);
                    Assert(current->length == 1);
                    SetHasNoMissingValues();
                }
            }
        }

        this->SetLastUsedSegment(current);

#ifdef VALIDATE_ARRAY
        ValidateArray();
#endif
    }

    template<typename T>
    bool JavascriptArray::NeedScanForMissingValuesUponSetItem(SparseArraySegment<T> *const segment, const uint32 offset) const
    {
        Assert(segment);

        // Scan for missing values upon SetItem when a missing value is being filled and the surrounding values are not missing,
        // as this could be the last missing value that is being filled
        return
            offset < segment->length &&
            SparseArraySegment<T>::IsMissingItem(&segment->elements[offset]) &&
            (offset == 0 || !SparseArraySegment<T>::IsMissingItem(&segment->elements[offset - 1])) &&
            (offset == segment->length - 1 || !SparseArraySegment<T>::IsMissingItem(&segment->elements[offset + 1])) &&
            segment == head;
    }

    template<typename T>
    void JavascriptArray::ScanForMissingValues(const uint startIndex)
    {
        Assert(head);
        Assert(!HasNoMissingValues());

        SparseArraySegment<T> *const segment = (SparseArraySegment<T>*)head;
        const uint segmentLength = segment->length;
        const T *const segmentElements = segment->elements;
        for(uint i = startIndex; i < segmentLength; ++i)
        {
            if(SparseArraySegment<T>::IsMissingItem(&segmentElements[i]))
            {
                return;
            }
        }

        SetHasNoMissingValues();
    }

    template<typename T>
    bool JavascriptArray::ScanForMissingValues(const uint startIndex, const uint endIndex)
    {
        Assert(head);
        //Assert(!HasNoMissingValues());

        SparseArraySegment<T> *const segment = (SparseArraySegment<T>*)head;
        const T *const segmentElements = segment->elements;
        for (uint i = startIndex; i < endIndex; ++i)
        {
            if (SparseArraySegment<T>::IsMissingItem(&segmentElements[i]))
            {
                return true;
            }
        }
        return false;
    }

    inline void JavascriptArray::DirectSetItemIfNotExist(uint32 index, Var newValue)
    {
        Assert(VirtualTableInfo<JavascriptArray>::HasVirtualTable(this));
        Var oldValue;
        if (!DirectGetItemAt(index, &oldValue))
        {
            DirectSetItemAt(index, newValue);
        }
    }

    //Grow the array head and try to set at the boundary
    template<typename unitType, typename classname>
    inline BOOL JavascriptArray::TryGrowHeadSegmentAndSetItem(uint32 indexInt, unitType iValue)
    {
        SparseArraySegment<unitType> *current = (SparseArraySegment<unitType> *)this->head;

        if (indexInt == current->length               // index is at the boundary of size & length
            && current->size                          // Make sure its not empty segment.
            && !current->next                         // There is only head segment.
            && current->length == current->size       // Why did we miss the fastpath?
            && !SparseArraySegment<unitType>::IsMissingItem(&iValue))      // value to set is not a missing value.
        {
            current= current->GrowByMin(this->GetRecycler(), indexInt + 1);

            DebugOnly(VerifyNotNeedMarshal(iValue));
            current->elements[indexInt] = iValue;
            current->length =  indexInt + 1;
            // There is only a head segment in this condition A segment map is not necessary
            // and most likely invalid at this point. Also we are setting the head and lastUsedSegment
            // to the same segment. Precedent in the rest of the code base dictates the use of
            // SetHeadAndLastUsedSegment which asserts if a segment map exists.
            ClearSegmentMap();
            SetHeadAndLastUsedSegment(current);

            if (this->length <= indexInt)
            {
                this->length = indexInt + 1;
            }

#ifdef VALIDATE_ARRAY
            ValidateArray();
#endif
            return true;
        }
        return false;
    }

    //
    // JavascriptArray::IndexTrace specialized on uint32 (small index)
    //
    template<>
    inline Var JavascriptArray::IndexTrace<uint32>::ToNumber(const uint32& index, ScriptContext* scriptContext)
    {
        return JavascriptNumber::ToVar(index, scriptContext);
    }

    template<>
    inline BOOL JavascriptArray::IndexTrace<uint32>::GetItem(JavascriptArray* arr, const uint32& index, Var* outVal)
    {
        return arr->DirectGetItemAt(index, outVal);
    }

    template<>
    inline BOOL JavascriptArray::IndexTrace<uint32>::SetItem(JavascriptArray* arr, const uint32& index, Var newValue)
    {
        return arr->SetItem(index, newValue, PropertyOperation_None);
    }

    template<>
    inline void JavascriptArray::IndexTrace<uint32>::SetItemIfNotExist(JavascriptArray* arr, const uint32& index, Var newValue)
    {
        arr->DirectSetItemIfNotExist(index, newValue);
    }

    template<>
    inline BOOL JavascriptArray::IndexTrace<uint32>::DeleteItem(JavascriptArray* arr, const uint32& index)
    {
        switch (arr->GetTypeId())
        {
        case TypeIds_Array:
            return arr->DirectDeleteItemAt<Var>(index);

        case TypeIds_NativeIntArray:
            return arr->DirectDeleteItemAt<int32>(index);

        case TypeIds_NativeFloatArray:
            return arr->DirectDeleteItemAt<double>(index);

        default:
            Assert(FALSE);
            return FALSE;
        }
    }

    template<>
    inline BOOL JavascriptArray::IndexTrace<uint32>::SetItem(RecyclableObject* obj, const uint32& index, Var newValue, PropertyOperationFlags flags)
    {
        ScriptContext* requestContext = obj->GetScriptContext();
        return JavascriptOperators::SetItem(obj, obj, index, newValue, requestContext, flags);
    }

    template<>
    inline BOOL JavascriptArray::IndexTrace<uint32>::DeleteItem(RecyclableObject* obj, const uint32& index, PropertyOperationFlags flags)
    {
        return JavascriptOperators::DeleteItem(obj, index, flags);
    }

    //
    // JavascriptArray::IndexTrace specialized on BigIndex
    //
    template<>
    inline Var JavascriptArray::IndexTrace<JavascriptArray::BigIndex>::ToNumber(const JavascriptArray::BigIndex& index, ScriptContext* scriptContext)
    {
        return index.ToNumber(scriptContext);
    }

    template<>
    inline BOOL JavascriptArray::IndexTrace<JavascriptArray::BigIndex>::GetItem(JavascriptArray* arr, const JavascriptArray::BigIndex& index, Var* outVal)
    {
        return index.GetItem(arr, outVal);
    }

    template<>
    inline BOOL JavascriptArray::IndexTrace<JavascriptArray::BigIndex>::SetItem(JavascriptArray* arr, const JavascriptArray::BigIndex& index, Var newValue)
    {
        return index.SetItem(arr, newValue);
    }

    template<>
    inline void JavascriptArray::IndexTrace<JavascriptArray::BigIndex>::SetItemIfNotExist(JavascriptArray* arr, const JavascriptArray::BigIndex& index, Var newValue)
    {
        index.SetItemIfNotExist(arr, newValue);
    }

    template<>
    inline BOOL JavascriptArray::IndexTrace<JavascriptArray::BigIndex>::DeleteItem(JavascriptArray* arr, const JavascriptArray::BigIndex& index)
    {
        return index.DeleteItem(arr);
    }

    template<>
    inline BOOL JavascriptArray::IndexTrace<JavascriptArray::BigIndex>::SetItem(RecyclableObject* obj, const JavascriptArray::BigIndex& index, Var newValue, PropertyOperationFlags flags)
    {
        return index.SetItem(obj, newValue, flags);
    }

    template<>
    inline BOOL JavascriptArray::IndexTrace<JavascriptArray::BigIndex>::DeleteItem(RecyclableObject* obj, const JavascriptArray::BigIndex& index, PropertyOperationFlags flags)
    {
        return index.DeleteItem(obj, flags);
    }

    template<class T, uint InlinePropertySlots>
    __inline size_t JavascriptArray::DetermineAllocationSize(
        const uint inlineElementSlots,
        size_t *const allocationPlusSizeRef,
        uint *const alignedInlineElementSlotsRef)
    {
        CompileAssert(static_cast<PropertyIndex>(InlinePropertySlots) == InlinePropertySlots);
        Assert(
            DynamicTypeHandler::RoundUpInlineSlotCapacity(static_cast<PropertyIndex>(InlinePropertySlots)) ==
            InlinePropertySlots);

        CompileAssert(
            InlinePropertySlots <=
            (UINT_MAX - (sizeof(T) + sizeof(SparseArraySegment<typename T::TElement>))) / sizeof(Var));
        const uint objectSize =
            sizeof(T) + sizeof(SparseArraySegment<typename T::TElement>) + InlinePropertySlots * sizeof(Var);
        size_t totalSize = UInt32Math::MulAdd<sizeof(typename T::TElement), objectSize>(inlineElementSlots);

    #if defined(_M_X64_OR_ARM64)
        // On x64, the total size won't be anywhere near AllocSizeMath::MaxMemory on x64, so no need to check
        totalSize = HeapInfo::GetAlignedSizeNoCheck(totalSize);
    #else
        totalSize = HeapInfo::GetAlignedSize(totalSize);
    #endif

        if(allocationPlusSizeRef)
        {
            *allocationPlusSizeRef = totalSize - sizeof(T);
        }
        if(alignedInlineElementSlotsRef)
        {
            const size_t alignedInlineElementSlots = (totalSize - objectSize) / sizeof(typename T::TElement);
            *alignedInlineElementSlotsRef = static_cast<uint>(alignedInlineElementSlots);
            Assert(*alignedInlineElementSlotsRef == alignedInlineElementSlots); // ensure no truncation above
        }

        return totalSize;
    }

    template<class T, uint InlinePropertySlots>
    __inline uint JavascriptArray::DetermineAvailableInlineElementSlots(
        const size_t allocationSize,
        bool *const isSufficientSpaceForInlinePropertySlotsRef)
    {
        CompileAssert(static_cast<PropertyIndex>(InlinePropertySlots) == InlinePropertySlots);
        Assert(
            DynamicTypeHandler::RoundUpInlineSlotCapacity(static_cast<PropertyIndex>(InlinePropertySlots)) ==
            InlinePropertySlots);
        Assert(isSufficientSpaceForInlinePropertySlotsRef);

        CompileAssert(
            InlinePropertySlots <=
            (UINT_MAX - (sizeof(T) + sizeof(SparseArraySegment<typename T::TElement>))) / sizeof(Var));
        *isSufficientSpaceForInlinePropertySlotsRef =
            sizeof(T) + InlinePropertySlots * sizeof(Var) + sizeof(SparseArraySegment<typename T::TElement>) <= allocationSize;

        const size_t availableInlineElementSlots =
            (
                allocationSize -
                (sizeof(T) + InlinePropertySlots * sizeof(Var) + sizeof(SparseArraySegment<typename T::TElement>))
            ) / sizeof(typename T::TElement);
        const uint availableInlineElementSlotsUint = static_cast<uint>(availableInlineElementSlots);
        Assert(availableInlineElementSlotsUint == availableInlineElementSlots); // ensure no truncation above
        return availableInlineElementSlotsUint;
    }

    template<class T, uint ConstInlinePropertySlots, bool UseDynamicInlinePropertySlots>
    __inline SparseArraySegment<typename T::TElement> *JavascriptArray::DetermineInlineHeadSegmentPointer(T *const array)
    {
        Assert(array);
        Assert(VirtualTableInfo<T>::HasVirtualTable(array) || VirtualTableInfo<CrossSiteObject<T>>::HasVirtualTable(array));
        Assert(!UseDynamicInlinePropertySlots || ConstInlinePropertySlots == 0);
        Assert(
            UseDynamicInlinePropertySlots ||
            ConstInlinePropertySlots == array->GetTypeHandler()->GetInlineSlotCapacity());

        const uint inlinePropertySlots =
            UseDynamicInlinePropertySlots ? array->GetTypeHandler()->GetInlineSlotCapacity() : ConstInlinePropertySlots;
        Assert(inlinePropertySlots == 0 || array->GetTypeHandler()->GetOffsetOfInlineSlots() == sizeof(T));

        return
            reinterpret_cast<SparseArraySegment<typename T::TElement> *>(
                reinterpret_cast<Var *>(array + 1) + inlinePropertySlots);
    }

    //
    // ItemTrace<T> specializations
    //
    template<>
    inline uint32 JavascriptArray::ItemTrace<JavascriptArray>::GetLength(JavascriptArray* obj, ScriptContext* scriptContext)
    {
        return obj->GetLength();
    }
    template<>
    inline BOOL JavascriptArray::ItemTrace<JavascriptArray>::GetItem(JavascriptArray* obj, uint32 index, Var* outVal, ScriptContext* scriptContext)
    {
        Assert(JavascriptArray::IsDirectAccessArray(obj));
        return obj->DirectGetItemAtFull(index, outVal); // Note this does prototype lookup
    }

    template<>
    inline uint32 JavascriptArray::ItemTrace<RecyclableObject>::GetLength(RecyclableObject* obj, ScriptContext* scriptContext)
    {
        return JavascriptConversion::ToUInt32(JavascriptOperators::OP_GetLength(obj, scriptContext), scriptContext);
    }
    template<>
    inline BOOL JavascriptArray::ItemTrace<RecyclableObject>::GetItem(RecyclableObject* obj, uint32 index, Var* outVal, ScriptContext* scriptContext)
    {
        return JavascriptOperators::GetItem(obj, index, outVal, scriptContext);
    }

} // namespace Js
