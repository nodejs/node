//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

#pragma once

template <bool interlocked>
__inline
bool
HeapBlockMap32::MarkInternal(L2MapChunk * chunk, void * candidate)
{
    uint bitIndex = chunk->GetMarkBitIndex(candidate);

    if (interlocked)
    {
        // Use an interlocked BTS instruction to ensure atomicity.
        // Since this is expensive, do a non-interlocked test first.
        // Mark bits never go from set to clear during marking, so if we find the bit is already set, we're done.
        if (chunk->markBits.TestIntrinsic(bitIndex))
        {
            // Already marked; no further processing needed
            return true;
        }

        if (chunk->markBits.TestAndSetInterlocked(bitIndex))
        {
            // Already marked; no further processing needed
            return true;
        }
    }
    else
    {
        if (chunk->markBits.TestAndSet(bitIndex))
        {
            // Already marked; no further processing needed
            return true;
        }
    }

#if DBG
    InterlockedIncrement16((short *)&chunk->pageMarkCount[GetLevel2Id(candidate)]);
#endif

    return false;
}

//
// Mark a particular object
// If the object is already marked, or if it's invalid, return true
// (indicating there's no further processing to be done for this object)
// If the object is newly marked, then the out param heapBlock is written to, and false is returned
//

template <bool interlocked>
__inline
void
HeapBlockMap32::Mark(void * candidate, MarkContext * markContext)
{
    uint id1 = GetLevel1Id(candidate);

    L2MapChunk * chunk = map[id1];
    if (chunk == nullptr)
    {
        // False refernce; no further processing needed.
        return;
    }

    if (MarkInternal<interlocked>(chunk, candidate))
    {
        return;
    }

    uint id2 = GetLevel2Id(candidate);
    HeapBlock::HeapBlockType blockType = chunk->blockInfo[id2].blockType;

    Assert(blockType == HeapBlock::HeapBlockType::FreeBlockType || chunk->map[id2]->GetHeapBlockType() == blockType);

    // Switch on the HeapBlockType to determine how to process the newly marked object.
    switch (blockType)
    {
    case HeapBlock::HeapBlockType::FreeBlockType:
        // False reference.  Do nothing.
        break;

    case HeapBlock::HeapBlockType::SmallLeafBlockType:
    case HeapBlock::HeapBlockType::MediumLeafBlockType:
        // Leaf blocks don't need to be scanned.  Do nothing.
        break;

    case HeapBlock::HeapBlockType::SmallNormalBlockType:
#ifdef RECYCLER_WRITE_BARRIER
    case HeapBlock::HeapBlockType::SmallNormalBlockWithBarrierType:
#endif
        {
            byte bucketIndex = chunk->blockInfo[id2].bucketIndex;

            // See if it's an invalid offset using the invalid bit vector and if so, do nothing.
            if (!HeapInfo::GetInvalidBitVectorForBucket<SmallAllocationBlockAttributes>(bucketIndex)->Test(SmallHeapBlock::GetAddressBitIndex(candidate)))
            {
                uint objectSize = HeapInfo::GetObjectSizeForBucketIndex<SmallAllocationBlockAttributes>(bucketIndex);
                if (!markContext->AddMarkedObject(candidate, objectSize))
                {
                    // Failed to mark due to OOM.
                    ((SmallHeapBlock *)chunk->map[id2])->SetNeedOOMRescan(markContext->GetRecycler());
                }
            }
        }
        break;
    case HeapBlock::HeapBlockType::MediumNormalBlockType:
#ifdef RECYCLER_WRITE_BARRIER
    case HeapBlock::HeapBlockType::MediumNormalBlockWithBarrierType:
#endif
        {
            byte bucketIndex = chunk->blockInfo[id2].bucketIndex;
            // See if it's an invalid offset using the invalid bit vector and if so, do nothing.
            if (!HeapInfo::GetInvalidBitVectorForBucket<MediumAllocationBlockAttributes>(bucketIndex)->Test(MediumHeapBlock::GetAddressBitIndex(candidate)))
            {
                uint objectSize = HeapInfo::GetObjectSizeForBucketIndex<MediumAllocationBlockAttributes>(bucketIndex);
                if (!markContext->AddMarkedObject(candidate, objectSize))
                {
                    // Failed to mark due to OOM.
                    ((MediumHeapBlock *)chunk->map[id2])->SetNeedOOMRescan(markContext->GetRecycler());
                }
            }
        }
        break;
    case HeapBlock::HeapBlockType::SmallFinalizableBlockType:
#ifdef RECYCLER_WRITE_BARRIER
    case HeapBlock::HeapBlockType::SmallFinalizableBlockWithBarrierType:
#endif
        ((SmallFinalizableHeapBlock*)chunk->map[id2])->ProcessMarkedObject(candidate, markContext);
        break;
    case HeapBlock::HeapBlockType::MediumFinalizableBlockType:
#ifdef RECYCLER_WRITE_BARRIER
    case HeapBlock::HeapBlockType::MediumFinalizableBlockWithBarrierType:
#endif
        ((MediumFinalizableHeapBlock*)chunk->map[id2])->ProcessMarkedObject(candidate, markContext);
        break;

    case HeapBlock::HeapBlockType::LargeBlockType:
        ((LargeHeapBlock*)chunk->map[id2])->Mark(candidate, markContext);
        break;

#if DBG
    default:
        AssertMsg(false, "what's the new heap block type?");
#endif
    }
}

template <bool interlocked, bool updateChunk>
__inline
bool
HeapBlockMap32::MarkInteriorInternal(MarkContext * markContext, L2MapChunk *& chunk, void * originalCandidate, void * realCandidate)
{
    if (originalCandidate == realCandidate)
    {
        // The initial mark performed was correct (we had a base pointer)
        return false;
    }

    if (realCandidate == nullptr)
    {
        // We had an invalid interior pointer, so we bail out
        return true;
    }

    if (updateChunk)
    {
#if defined(_M_X64_OR_ARM64)
        if (HeapBlockMap64::GetNodeIndex(originalCandidate) != HeapBlockMap64::GetNodeIndex(realCandidate))
        {
            // We crossed a node boundary (very rare) so we should just re-start from the real candidate.
            // In this case we are no longer marking an interior reference.
            markContext->GetRecycler()->heapBlockMap.Mark<interlocked>(realCandidate, markContext);

            // This mark code therefore has nothing to do (it has already happened).
            return true;
        }
#endif
        // Update the chunk as the interior pointer may cross an L2 boundary (e.g., a large object)
        chunk = map[GetLevel1Id(realCandidate)];
    }

    // Perform the actual mark for the interior pointer
    return MarkInternal<interlocked>(chunk, realCandidate);
}

template <bool interlocked>
__inline
void
HeapBlockMap32::MarkInterior(void * candidate, MarkContext * markContext)
{
    // Align the candidate to object granularity
    candidate = reinterpret_cast<void*>(reinterpret_cast<size_t>(candidate) & ~HeapInfo::ObjectAlignmentMask);
    uint id1 = GetLevel1Id(candidate);

    L2MapChunk * chunk = map[id1];
    if (chunk == nullptr)
    {
        // False reference; no further processing needed.
        return;
    }

    if (MarkInternal<interlocked>(chunk, candidate))
    {
        // Already marked (mark internal-then-actual first)
        return;
    }

    uint id2 = GetLevel2Id(candidate);
    HeapBlock::HeapBlockType blockType = chunk->blockInfo[id2].blockType;

    // Switch on the HeapBlockType to determine how to map interior->base and process object.
    switch (blockType)
    {
    case HeapBlock::HeapBlockType::FreeBlockType:
        // False reference.  Do nothing.
        break;

    case HeapBlock::HeapBlockType::SmallLeafBlockType:
    case HeapBlock::HeapBlockType::MediumLeafBlockType:
        // Leaf blocks don't need to be scanned.  Do nothing.
        break;

    case HeapBlock::HeapBlockType::SmallNormalBlockType:
#ifdef RECYCLER_WRITE_BARRIER
    case HeapBlock::HeapBlockType::SmallNormalBlockWithBarrierType:
#endif
        {
            byte bucketIndex = chunk->blockInfo[id2].bucketIndex;
            uint objectSize = HeapInfo::GetObjectSizeForBucketIndex<SmallAllocationBlockAttributes>(bucketIndex);
            void * realCandidate = SmallHeapBlock::GetRealAddressFromInterior(candidate, objectSize, bucketIndex);
            if (MarkInteriorInternal<interlocked, false>(markContext, chunk, candidate, realCandidate))
            {
                break;
            }

            if (!markContext->AddMarkedObject(realCandidate, objectSize))
            {
                // Failed to mark due to OOM.
                ((SmallHeapBlock *)chunk->map[id2])->SetNeedOOMRescan(markContext->GetRecycler());
            }
        }
        break;
    case HeapBlock::HeapBlockType::MediumNormalBlockType:
#ifdef RECYCLER_WRITE_BARRIER
    case HeapBlock::HeapBlockType::MediumNormalBlockWithBarrierType:
#endif
        {
            byte bucketIndex = chunk->blockInfo[id2].bucketIndex;
            uint objectSize = HeapInfo::GetObjectSizeForBucketIndex<MediumAllocationBlockAttributes>(bucketIndex);
            void * realCandidate = MediumHeapBlock::GetRealAddressFromInterior(candidate, objectSize, bucketIndex);
            if (MarkInteriorInternal<interlocked, false>(markContext, chunk, candidate, realCandidate))
            {
                break;
            }

            if (!markContext->AddMarkedObject(realCandidate, objectSize))
            {
                // Failed to mark due to OOM.
                ((MediumHeapBlock *)chunk->map[id2])->SetNeedOOMRescan(markContext->GetRecycler());
            }
        }
        break;
    case HeapBlock::HeapBlockType::SmallFinalizableBlockType:
#ifdef RECYCLER_WRITE_BARRIER
    case HeapBlock::HeapBlockType::SmallFinalizableBlockWithBarrierType:
#endif
        {
            void * realCandidate = ((SmallFinalizableHeapBlock*)chunk->map[id2])->GetRealAddressFromInterior(candidate);
            if (MarkInteriorInternal<interlocked, false>(markContext, chunk, candidate, realCandidate))
            {
                break;
            }

            ((SmallFinalizableHeapBlock*)chunk->map[id2])->ProcessMarkedObject(realCandidate, markContext);
        }
        break;
    case HeapBlock::HeapBlockType::MediumFinalizableBlockType:
#ifdef RECYCLER_WRITE_BARRIER
    case HeapBlock::HeapBlockType::MediumFinalizableBlockWithBarrierType:
#endif
        {
            void * realCandidate = ((MediumFinalizableHeapBlock*)chunk->map[id2])->GetRealAddressFromInterior(candidate);
            if (MarkInteriorInternal<interlocked, false>(markContext, chunk, candidate, realCandidate))
            {
                break;
            }

            ((MediumFinalizableHeapBlock*)chunk->map[id2])->ProcessMarkedObject(realCandidate, markContext);
        }
        break;

    case HeapBlock::HeapBlockType::LargeBlockType:
        {
            void * realCandidate = ((LargeHeapBlock*)chunk->map[id2])->GetRealAddressFromInterior(candidate);
            if (MarkInteriorInternal<interlocked, true>(markContext, chunk, candidate, realCandidate))
            {
                break;
            }

            ((LargeHeapBlock*)chunk->map[GetLevel2Id(realCandidate)])->Mark(realCandidate, markContext);
        }
        break;

#if DBG
    default:
        AssertMsg(false, "what's the new heap block type?");
#endif
    }
}

#if defined(_M_X64_OR_ARM64)

//
// 64-bit Mark
// See HeapBlockMap32::Mark for explanation of return values
//

template <bool interlocked>
__inline
void
HeapBlockMap64::Mark(void * candidate, MarkContext * markContext)
{
    uint index = GetNodeIndex(candidate);

    Node * node = list;
    while (node != nullptr)
    {
        if (node->nodeIndex == index)
        {
            // Found the correct Node.
            // Process the mark and return.
            node->map.Mark<interlocked>(candidate, markContext);
            return;
        }

        node = node->next;
    }

    // No Node found; must be an invalid reference. Do nothing.
}

template <bool interlocked>
__inline
void
HeapBlockMap64::MarkInterior(void * candidate, MarkContext * markContext)
{
    uint index = GetNodeIndex(candidate);

    Node * node = list;
    while (node != nullptr)
    {
        if (node->nodeIndex == index)
        {
            // Found the correct Node.
            // Process the mark and return.
            node->map.MarkInterior<interlocked>(candidate, markContext);
            return;
        }

        node = node->next;
    }

    // No Node found; must be an invalid reference. Do nothing.
}

#endif // defined(_M_X64_OR_ARM64)
