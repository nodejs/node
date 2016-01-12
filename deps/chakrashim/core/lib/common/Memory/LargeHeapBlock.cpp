//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "CommonMemoryPch.h"

CompileAssert(
    sizeof(LargeObjectHeader) == HeapConstants::ObjectGranularity ||
    sizeof(LargeObjectHeader) == HeapConstants::ObjectGranularity * 2);

void *
LargeObjectHeader::GetAddress() { return ((char *)this) + sizeof(LargeObjectHeader); }

#ifdef LARGEHEAPBLOCK_ENCODING
// decodedNext = decoded next field
// decodedAttributes = decoded attributes part of attributesAndChecksum
// Decode 'next' and 'attributes' using _cookie
// If next=B1B2B3B4, checksum = (B1^B2^B3^B4^attributes)
// Encode 'next' and 'attributes' using _cookie
unsigned char
LargeObjectHeader::CalculateCheckSum(LargeObjectHeader* decodedNext, unsigned char decodedAttributes)
{
    unsigned char checksum = 0;
    byte *nextField = (byte *)&decodedNext;

    checksum = nextField[0] ^ nextField[1] ^ nextField[2] ^ nextField[3] ^ decodedAttributes;
    return checksum;
}

LargeObjectHeader*
LargeObjectHeader::EncodeNext(uint cookie, LargeObjectHeader* next)
{
    return (LargeObjectHeader *)((uintptr)next ^ cookie);
}

ushort
LargeObjectHeader::EncodeAttributesAndChecksum(uint cookie, ushort attributesAndChecksum)
{
    return attributesAndChecksum ^ (ushort)cookie;
}

LargeObjectHeader*
LargeObjectHeader::DecodeNext(uint cookie, LargeObjectHeader* next) { return EncodeNext(cookie, next); }

ushort
LargeObjectHeader::DecodeAttributesAndChecksum(uint cookie) { return EncodeAttributesAndChecksum(cookie, this->attributesAndChecksum); }

#else
// If heap block encoding is disabled then have an API to expose
// pointer to attributes so that can be passed to RecyclerHeapObjectInfo()
// which updates the attributes field.
unsigned char *
LargeObjectHeader::GetAttributesPtr()
{
    return &this->attributes;
}
#endif

void
LargeObjectHeader::SetNext(uint cookie, LargeObjectHeader* next)
{
#ifdef LARGEHEAPBLOCK_ENCODING
    ushort decodedAttributesAndChecksum = this->DecodeAttributesAndChecksum(cookie);

    // Calculate the checksum value with new next
    unsigned char newCheckSumValue = this->CalculateCheckSum(next, (unsigned char)(decodedAttributesAndChecksum >> 8));
    // pack the (attribute + checksum)
    ushort newAttributeWithCheckSum = (decodedAttributesAndChecksum & 0xFF00) | newCheckSumValue;

    // encode the packed (attribute + checksum), next and set them
    this->attributesAndChecksum = this->EncodeAttributesAndChecksum(cookie, newAttributeWithCheckSum);
    this->next = this->EncodeNext(cookie, next);
#else
    this->next = next;
#endif
}

LargeObjectHeader *
LargeObjectHeader::GetNext(uint cookie)
{
#ifdef LARGEHEAPBLOCK_ENCODING
    LargeObjectHeader *decodedNext = this->DecodeNext(cookie, this->next);
    ushort decodedAttributesAndChecksum = this->DecodeAttributesAndChecksum(cookie);

    unsigned char checkSum = (unsigned char)(decodedAttributesAndChecksum & 0xFF);
    unsigned char calculatedCheckSumField = this->CalculateCheckSum(decodedNext, (unsigned char)(decodedAttributesAndChecksum >> 8));
    if (checkSum != calculatedCheckSumField)
    {
        LargeHeapBlock_Metadata_Corrupted((ULONG_PTR)this, calculatedCheckSumField);
    }
    // If checksum matches return the up-to-date next (in case other thread changed it from last time
    // we read it in this method.
    return this->DecodeNext(cookie, this->next);

#else
    return this->next;
#endif
}

void
LargeObjectHeader::SetAttributes(uint cookie, unsigned char attributes)
{
#ifdef LARGEHEAPBLOCK_ENCODING
    LargeObjectHeader *decodedNext = this->DecodeNext(cookie, this->next);

    // Calculate the checksum value with new attribute
    unsigned char newCheckSumValue = this->CalculateCheckSum(decodedNext, attributes);
    // pack the (attribute + checksum)
    ushort newAttributeWithCheckSum = ((ushort)attributes << 8) | newCheckSumValue;
    // encode the packed (attribute + checksum) and set it
    this->attributesAndChecksum = this->EncodeAttributesAndChecksum(cookie, newAttributeWithCheckSum);
#else
    this->attributes = attributes;
#endif
}

unsigned char
LargeObjectHeader::GetAttributes(uint cookie)
{
#ifdef LARGEHEAPBLOCK_ENCODING

    LargeObjectHeader *decodedNext = this->DecodeNext(cookie, this->next);
    ushort decodedAttributesAndChecksum = this->DecodeAttributesAndChecksum(cookie);

    unsigned char checkSum = (unsigned char)(decodedAttributesAndChecksum & 0xFF);
    unsigned char calculatedCheckSumField = this->CalculateCheckSum(decodedNext, (unsigned char)(decodedAttributesAndChecksum >> 8));
    if (checkSum != calculatedCheckSumField)
    {
        LargeHeapBlock_Metadata_Corrupted((ULONG_PTR)this, calculatedCheckSumField);
    }

    // If checksum matches return the up-to-date attributes (in case other thread changed it from last time
    // we read it in this method.
    return this->DecodeAttributesAndChecksum(cookie) >> 8;
#else
    return this->attributes;
#endif
}

size_t
LargeHeapBlock::GetAllocPlusSize(uint objectCount)
{
    // Large Heap Block Layout:
    //      LargeHeapBlock
    //      LargeObjectHeader * [objectCount]
    //      TrackerData *       [objectCount] (Optional)
    size_t allocPlusSize = objectCount * (sizeof(LargeObjectHeader *));
#ifdef PROFILE_RECYCLER_ALLOC
    if (Recycler::DoProfileAllocTracker())
    {
        allocPlusSize += objectCount * sizeof(void *);
    }
#endif
    return allocPlusSize;
}

LargeHeapBlock *
LargeHeapBlock::New(__in char * address, size_t pageCount, Segment * segment, uint objectCount, LargeHeapBucket* bucket)
{
    return NoMemProtectHeapNewNoThrowPlusZ(GetAllocPlusSize(objectCount), LargeHeapBlock, address, pageCount, segment, objectCount, bucket);
}

void
LargeHeapBlock::Delete(LargeHeapBlock * heapBlock)
{
    NoMemProtectHeapDeletePlus(GetAllocPlusSize(heapBlock->objectCount), heapBlock);
}

LargeHeapBlock::LargeHeapBlock(__in char * address, size_t pageCount, Segment * segment, uint objectCount, LargeHeapBucket* bucket)
    : HeapBlock(LargeBlockType), pageCount(pageCount), allocAddressEnd(address), objectCount(objectCount), bucket(bucket), freeList(this)
{
    Assert(address != nullptr);
    Assert(pageCount != 0);
    Assert(objectCount != 0);
    Assert(lastCollectAllocCount == 0);
    Assert(finalizeCount == 0);
    Assert(next == nullptr);
    Assert(!hasPartialFreeObjects);

    this->address = address;
    this->segment = segment;
    this->isPendingConcurrentSweep = false;
    this->addressEnd = this->address + this->pageCount * AutoSystemInfo::PageSize;

    RECYCLER_PERF_COUNTER_INC(LargeHeapBlockCount);
    RECYCLER_PERF_COUNTER_ADD(LargeHeapBlockPageSize, pageCount * AutoSystemInfo::PageSize);
}

LargeHeapBlock::~LargeHeapBlock()
{
    AssertMsg(this->segment == nullptr || this->heapInfo->recycler->recyclerLargeBlockPageAllocator.IsClosed(),
        "ReleasePages needs to be called before delete");
    RECYCLER_PERF_COUNTER_DEC(LargeHeapBlockCount);

#ifdef RECYCLER_PAGE_HEAP
    if (this->pageHeapAllocStack != nullptr)
    {
        this->pageHeapAllocStack->Delete(&NoCheckHeapAllocator::Instance);
        this->pageHeapAllocStack = nullptr;
    }

    // REVIEW: This means that the old free stack is lost when we get free the heap block
    // Is this okay? Should we delay freeing heap blocks till process/thread shutdown time?
    if (this->pageHeapFreeStack != nullptr)
    {
        this->pageHeapFreeStack->Delete(&NoCheckHeapAllocator::Instance);
        this->pageHeapFreeStack = nullptr;
    }
#endif
}

Recycler *
LargeHeapBlock::GetRecycler() const
{
    return this->bucket->heapInfo->recycler;
}

LargeObjectHeader **
LargeHeapBlock::HeaderList()
{
    // See LargeHeapBlock::GetAllocPlusSize for layout description
    return (LargeObjectHeader **)(((byte *)this) + sizeof(LargeHeapBlock));
}

void
LargeHeapBlock::FinalizeAllObjects()
{
    if (this->finalizeCount != 0)
    {
        DebugOnly(uint processedCount = 0);
        for (uint i = 0; i < allocCount; i++)
        {
            LargeObjectHeader * header = this->GetHeader(i);
            if (header == nullptr || ((header->GetAttributes(this->heapInfo->recycler->Cookie) & FinalizeBit) == 0))
            {
                continue;
            }

            FinalizableObject * finalizableObject = ((FinalizableObject *)header->GetAddress());

            finalizableObject->Finalize(true);
            finalizableObject->Dispose(true);
#ifdef RECYCLER_FINALIZE_CHECK
            this->heapInfo->liveFinalizableObjectCount--;
#endif
            DebugOnly(processedCount++);
        }

        while (pendingDisposeObject != nullptr)
        {
            LargeObjectHeader * header = pendingDisposeObject;
            pendingDisposeObject = header->GetNext(this->heapInfo->recycler->Cookie);
            Assert(header->GetAttributes(this->heapInfo->recycler->Cookie) & FinalizeBit);
            Assert(this->HeaderList()[header->objectIndex] == nullptr);

            void * objectAddress = header->GetAddress();
            ((FinalizableObject *)objectAddress)->Dispose(true);
#ifdef RECYCLER_FINALIZE_CHECK
            this->heapInfo->liveFinalizableObjectCount--;
            this->heapInfo->pendingDisposableObjectCount--;
#endif
            DebugOnly(processedCount++);
        }

        Assert(this->finalizeCount == processedCount);
    }
}


void
LargeHeapBlock::ReleasePagesShutdown(Recycler * recycler)
{
#if DBG
    recycler->heapBlockMap.ClearHeapBlock(this->address, this->pageCount);

    // Don't release the page in shut down, the page allocator will release them faster
    Assert(recycler->recyclerLargeBlockPageAllocator.IsClosed());
#endif
}

template void LargeHeapBlock::ReleasePagesSweep<true>(Recycler * recycler);
template void LargeHeapBlock::ReleasePagesSweep<false>(Recycler * recycler);

template<bool pageheap>
void
LargeHeapBlock::ReleasePagesSweep(Recycler * recycler)
{
    recycler->heapBlockMap.ClearHeapBlock(this->address, this->pageCount);

    ReleasePages<pageheap>(recycler);
}


template void LargeHeapBlock::ReleasePages<true>(Recycler * recycler);
template void LargeHeapBlock::ReleasePages<false>(Recycler * recycler);

template<bool pageheap>
void
LargeHeapBlock::ReleasePages(Recycler * recycler)
{
    Assert(segment != nullptr);

    char* pageAddress = address;
    size_t realPageCount = pageCount;
#ifdef RECYCLER_PAGE_HEAP
    if (pageheap)
    {
        if (InPageHeapMode())
        {
            if (guardPageAddress != nullptr)
            {
                DWORD noAccess;
                if (::VirtualProtect(static_cast<LPVOID>(guardPageAddress), AutoSystemInfo::PageSize, guardPageOldProtectFlags, &noAccess) == FALSE)
                {
                    AssertMsg(false, "Unable to set permission for guard page.");
                    return;
                }
                AssertMsg(noAccess == PAGE_NOACCESS, "Guard page should be PAGE_NOACCESS");

                if (this->pageHeapMode == PageHeapMode::PageHeapModeBlockStart)
                {
                    pageAddress = guardPageAddress;
                }

                realPageCount = actualPageCount;
            }
        }
    }
#endif

#ifdef RECYCLER_FREE_MEM_FILL
    memset(this->address, DbgMemFill, AutoSystemInfo::PageSize * pageCount);
#endif
    recycler->recyclerLargeBlockPageAllocator.Release(pageAddress, realPageCount, segment);
    RECYCLER_PERF_COUNTER_SUB(LargeHeapBlockPageSize, pageCount * AutoSystemInfo::PageSize);

    this->segment = nullptr;
}

BOOL
LargeHeapBlock::IsValidObject(void* objectAddress)
{
    LargeObjectHeader * header = GetHeader(objectAddress);
    return ((char *)header >= this->address && header->objectIndex < this->allocCount && this->HeaderList()[header->objectIndex] == header);
}

#if DBG
BOOL
LargeHeapBlock::IsFreeObject(void * objectAddress)
{
    LargeObjectHeader * header = GetHeader(objectAddress);
    return ((char *)header >= this->address && header->objectIndex < this->allocCount && this->GetHeader(header->objectIndex) == nullptr);
}
#endif
size_t
LargeHeapBlock::GetPagesNeeded(size_t size, bool multiplyRequest)
{
    if (multiplyRequest)
    {
        size = AllocSizeMath::Mul(size, 4);
    }

    uint pageSize = AutoSystemInfo::PageSize;
    size = AllocSizeMath::Add(size, sizeof(LargeObjectHeader) + (pageSize - 1));
    if (size == (size_t)-1)
    {
        return 0;
    }
    size_t pageCount = size / pageSize;
    return pageCount;
}

char*
LargeHeapBlock::TryAllocFromFreeList(size_t size, ObjectInfoBits attributes)
{
    Assert((attributes & InternalObjectInfoBitMask) == attributes);

    LargeHeapBlockFreeListEntry** prev = &this->freeList.entries;
    LargeHeapBlockFreeListEntry* freeListEntry = this->freeList.entries;

    char* memBlock = nullptr;

    // Walk through the free list, find the first entry that can fit our desired size
    while (freeListEntry)
    {
        LargeHeapBlockFreeListEntry* next = freeListEntry->next;
        LargeHeapBlock* heapBlock = freeListEntry->heapBlock;

        if (freeListEntry->objectSize >= size)
        {
            memBlock = heapBlock->AllocFreeListEntry(size, attributes, freeListEntry);
            if (memBlock)
            {
                (*prev) = next;

                break;
            }
        }

        prev = &freeListEntry->next;
        freeListEntry = freeListEntry->next;
    }

    if (this->freeList.entries == nullptr)
    {
        this->bucket->UnregisterFreeList(&this->freeList);
    }

    return memBlock;
}

char*
LargeHeapBlock::AllocFreeListEntry(size_t size, ObjectInfoBits attributes, LargeHeapBlockFreeListEntry* entry)
{
    Assert((attributes & InternalObjectInfoBitMask) == attributes);
    Assert(HeapInfo::IsAlignedSize(size));
    AssertMsg((attributes & TrackBit) == 0, "Large tracked object collection not implemented");
    Assert(entry->heapBlock == this);
    Assert(entry->headerIndex < this->objectCount);
    Assert(this->HeaderList()[entry->headerIndex] == nullptr);

    uint headerIndex = entry->headerIndex;
    size_t originalSize = entry->objectSize;

    LargeObjectHeader * header = (LargeObjectHeader *) entry;

    char * allocObject = ((char*) entry) + sizeof(LargeObjectHeader);       // shouldn't overflow
    char * newAllocAddressEnd = allocObject + size;
    char * originalAllocEnd = allocObject + originalSize;
    if (newAllocAddressEnd > addressEnd || newAllocAddressEnd < allocObject || (originalAllocEnd < newAllocAddressEnd))
    {
        return nullptr;
    }

#ifdef RECYCLER_MEMORY_VERIFY
    if (this->heapInfo->recycler->VerifyEnabled())
    {
        this->heapInfo->recycler->VerifyCheckFill(allocObject , originalSize);
    }
#endif

    memset(entry, 0, sizeof(LargeObjectHeader) + originalSize);

#ifdef RECYCLER_MEMORY_VERIFY
    // If we're in recyclerVerify mode, fill the non-header part of the allocation
    // with the verification pattern
    if (this->heapInfo->recycler->VerifyEnabled())
    {
        memset(allocObject, Recycler::VerifyMemFill, originalSize);
    }
#endif

#if DBG
    LargeAllocationVerboseTrace(this->heapInfo->recycler->GetRecyclerFlagsTable(), L"Allocated object of size 0x%x in from free list entry at address 0x%p\n", size, allocObject);
#endif

    Assert(allocCount <= objectCount);

    header->objectIndex = headerIndex;
    header->objectSize = originalSize;
    header->SetAttributes(this->heapInfo->recycler->Cookie, (attributes & StoredObjectInfoBitMask));
    header->markOnOOMRescan = nullptr;
    header->SetNext(this->heapInfo->recycler->Cookie, nullptr);

    HeaderList()[headerIndex] = header;
    finalizeCount += ((attributes & FinalizeBit) != 0);

#ifdef RECYCLER_FINALIZE_CHECK
    if (attributes & FinalizeBit)
    {
        HeapInfo * heapInfo = this->heapInfo;
        heapInfo->liveFinalizableObjectCount++;
        heapInfo->newFinalizableObjectCount++;
    }
#endif

    return allocObject;
}

char*
LargeHeapBlock::Alloc(size_t size, ObjectInfoBits attributes)
{
    Assert(HeapInfo::IsAlignedSize(size));
    Assert((attributes & InternalObjectInfoBitMask) == attributes);
    AssertMsg((attributes & TrackBit) == 0, "Large tracked object collection not implemented");

    LargeObjectHeader * header = (LargeObjectHeader *)allocAddressEnd;
#if defined(PARTIAL_GC_ENABLED) && defined(CONCURRENT_GC_ENABLED)
    Assert(!IsPartialSweptHeader(header));
#endif
    char * allocObject = allocAddressEnd + sizeof(LargeObjectHeader);       // shouldn't overflow
    char * newAllocAddressEnd = allocObject + size;
    if (newAllocAddressEnd > addressEnd || newAllocAddressEnd < allocObject)
    {
        return nullptr;
    }

    Recycler* recycler = this->heapInfo->recycler;
#if DBG
    LargeAllocationVerboseTrace(recycler->GetRecyclerFlagsTable(), L"Allocated object of size 0x%x in existing heap block at address 0x%p\n", size, allocObject);
#endif

    Assert(allocCount < objectCount);
    allocAddressEnd = newAllocAddressEnd;
#ifdef RECYCLER_ZERO_MEM_CHECK
    recycler->VerifyZeroFill(header, sizeof(LargeObjectHeader));
#endif
#ifdef RECYCLER_MEMORY_VERIFY
    if (recycler->VerifyEnabled())
    {
        memset(header, 0, sizeof(LargeObjectHeader));
    }
#endif
    header->objectIndex = allocCount;
    header->objectSize = size;
    header->SetAttributes(recycler->Cookie, (attributes & StoredObjectInfoBitMask));
    HeaderList()[allocCount++] = header;
    finalizeCount += ((attributes & FinalizeBit) != 0);

#ifdef RECYCLER_FINALIZE_CHECK
    if (attributes & FinalizeBit)
    {
        HeapInfo * heapInfo = this->heapInfo;
        heapInfo->liveFinalizableObjectCount++;
        heapInfo->newFinalizableObjectCount++;
    }
#endif

    return allocObject;
}

__declspec(noinline)
void
LargeHeapBlock::Mark(void* objectAddress, MarkContext * markContext)
{
    LargeObjectHeader * header = GetHeader(objectAddress);

    if ((char *)header < this->address)
    {
        return;
    }

    uint index = header->objectIndex;

    if (index >= this->allocCount)
    {
        // Not allocated yet.
        return;
    }

    if (this->HeaderList()[index] != header)
    {
        // header doesn't match, not a real object
        return;
    }

    DUMP_OBJECT_REFERENCE(markContext->GetRecycler(), objectAddress);

    if (!UpdateAttributesOfMarkedObjects(markContext, objectAddress, header->objectSize, header->GetAttributes(this->heapInfo->recycler->Cookie),
        [&](unsigned char attributes) { header->SetAttributes(this->heapInfo->recycler->Cookie, attributes); }))
    {
        // Couldn't mark children- bail out and come back later
        this->SetNeedOOMRescan(markContext->GetRecycler());

        // Single page large heap block rescan all marked object on oom rescan
        if (this->GetPageCount() != 1)
        {
            // Failed to mark the objects referenced by this object, so we'll
            // revisit this on rescan
            header->markOnOOMRescan = true;
        }
    }
}

bool
LargeHeapBlock::TestObjectMarkedBit(void* objectAddress)
{
    Assert(IsValidObject(objectAddress));

    LargeObjectHeader* pHeader = nullptr;

    if (GetObjectHeader(objectAddress, &pHeader))
    {
        Recycler* recycler = this->heapInfo->recycler;

        return recycler->heapBlockMap.IsMarked(objectAddress);
    }

    return FALSE;
}

void
LargeHeapBlock::SetObjectMarkedBit(void* objectAddress)
{
    Assert(IsValidObject(objectAddress));

    LargeObjectHeader* pHeader = nullptr;

    if (GetObjectHeader(objectAddress, &pHeader))
    {
        Recycler* recycler = this->heapInfo->recycler;

        recycler->heapBlockMap.SetMark(objectAddress);
    }
}

bool
LargeHeapBlock::FindImplicitRootObject(void* objectAddress, Recycler * recycler, RecyclerHeapObjectInfo& heapObject)
{
    if (!IsValidObject(objectAddress))
    {
        return false;
    }

    LargeObjectHeader* pHeader = nullptr;

    if (!GetObjectHeader(objectAddress, &pHeader))
    {
        return false;
    }

#ifdef LARGEHEAPBLOCK_ENCODING
    heapObject = RecyclerHeapObjectInfo(objectAddress, recycler, this, nullptr);
    heapObject.SetLargeHeapBlockHeader(pHeader);
#else
    heapObject = RecyclerHeapObjectInfo(objectAddress, recycler, this, pHeader->GetAttributesPtr());
#endif
    return true;
}

bool
LargeHeapBlock::FindHeapObject(void* objectAddress, Recycler * recycler, FindHeapObjectFlags, RecyclerHeapObjectInfo& heapObject)
{
    // Currently the same actual implementation (flags is ignored)
    return FindImplicitRootObject(objectAddress, recycler, heapObject);
}

bool
LargeHeapBlock::GetObjectHeader(void* objectAddress, LargeObjectHeader** ppHeader)
{
    (*ppHeader) = nullptr;

    LargeObjectHeader * header = GetHeader(objectAddress);
    if ((char *)header < this->address)
    {
        return false;
    }

    uint index = header->objectIndex;

    if (this->HeaderList()[index] != header)
    {
        // header doesn't match, not a real object
        return false;
    }

    Assert(index < this->allocCount);
    (*ppHeader) = header;
    return true;
}

void
LargeHeapBlock::ResetMarks(ResetMarkFlags flags, Recycler* recycler)
{
    Assert(!this->needOOMRescan);

    // Update the lastCollectAllocCount for sweep
    this->lastCollectAllocCount = this->allocCount;

    Assert(this->GetMarkCount() == 0);

    Assert(!this->isPendingConcurrentSweep);

    if (flags & ResetMarkFlags_ScanImplicitRoot)
    {
        for (uint objectIndex = 0; objectIndex < allocCount; objectIndex++)
        {
            // object is allocated during the concurrent mark or it is marked, do rescan
            LargeObjectHeader * header = this->GetHeader(objectIndex);
            // check if the object index is not allocated
            if (header == nullptr)
            {
                continue;
            }

            // check whether the object is a leaf and doesn't need to be scanned
            if ((header->GetAttributes(this->heapInfo->recycler->Cookie) & ImplicitRootBit) != 0)
            {
                recycler->heapBlockMap.SetMark(header->GetAddress());
            }
        }
    }
}

LargeObjectHeader *
LargeHeapBlock::GetHeader(void * objectAddress)
{
    Assert(objectAddress >= this->address && objectAddress < this->addressEnd);
    return GetHeaderFromAddress(objectAddress);
}

LargeObjectHeader *
LargeHeapBlock::GetHeaderFromAddress(void * objectAddress)
{
    return (LargeObjectHeader*)(((char *)objectAddress) - sizeof(LargeObjectHeader));
}

byte *
LargeHeapBlock::GetRealAddressFromInterior(void * interiorAddress)
{
    for (uint i = 0; i < allocCount; i++)
    {
        LargeObjectHeader * header = this->HeaderList()[i];

        if (header != nullptr && !IsPartialSweptHeader(header))
        {
            Assert(header->objectIndex == i);
            byte * startAddress = (byte *)header->GetAddress();
            if (startAddress <= interiorAddress && (startAddress + header->objectSize > interiorAddress))
            {
                return startAddress;
            }
        }
    }

    return nullptr;
}

#ifdef RECYCLER_VERIFY_MARK

void
LargeHeapBlock::VerifyMark()
{
    Assert(!this->needOOMRescan);
    Recycler* recycler = this->heapInfo->recycler;

    for (uint i = 0; i < allocCount; i++)
    {
        LargeObjectHeader * header = this->GetHeader(i);
        if (header == nullptr)
        {
            continue;
        }

        char * objectAddress = (char *)header->GetAddress();
        if (!recycler->heapBlockMap.IsMarked(objectAddress))
        {
            continue;
        }

        unsigned char attributes = header->GetAttributes(this->heapInfo->recycler->Cookie);

        Assert((attributes & NewFinalizeBit) == 0);

        if ((attributes & LeafBit) != 0)
        {
            continue;
        }

        Assert(!header->markOnOOMRescan);

        char * objectAddressEnd = objectAddress + header->objectSize;

        while (objectAddress + sizeof(void *) <= objectAddressEnd)
        {
            void* target = *(void **)objectAddress;

            recycler->VerifyMark(target);

            objectAddress += sizeof(void *);
        }
    }
}

void
LargeHeapBlock::VerifyMark(void * objectAddress)
{
    LargeObjectHeader * header = GetHeader(objectAddress);

    if ((char *)header < this->address)
    {
        return;
    }

    uint index = header->objectIndex;

    if (index >= this->allocCount)
    {
        // object not allocated
        return;
    }

    if (this->HeaderList()[index] != header)
    {
        // header doesn't match, not a real object
        return;
    }

    bool isMarked = this->heapInfo->recycler->heapBlockMap.IsMarked(objectAddress);

#if DBG
    Assert(isMarked);
#else
    if (!isMarked)
    {
        DebugBreak();
    }
#endif
}

#endif

void
LargeHeapBlock::ScanInitialImplicitRoots(Recycler * recycler)
{
    Assert(recycler->enableScanImplicitRoots);
    const HeapBlockMap& heapBlockMap = recycler->heapBlockMap;
    for (uint objectIndex = 0; objectIndex < allocCount; objectIndex++)
    {
        // object is allocated during the concurrent mark or it is marked, do rescan
        LargeObjectHeader * header = this->GetHeader(objectIndex);
        // check if the object index is not allocated
        if (header == nullptr)
        {
            continue;
        }

        // check whether the object is a leaf and doesn't need to be scanned
        if ((header->GetAttributes(this->heapInfo->recycler->Cookie) & LeafBit) != 0)
        {
            continue;
        }

        char * objectAddress = (char *)header->GetAddress();

        // it is not marked, don't scan implicit root
        if (!heapBlockMap.IsMarked(objectAddress))
        {
            continue;
        }

        // TODO: Assume scan interior?
        DUMP_IMPLICIT_ROOT(recycler, objectAddress);
        recycler->ScanObjectInlineInterior((void **)objectAddress, header->objectSize);
    }
}


void
LargeHeapBlock::ScanNewImplicitRoots(Recycler * recycler)
{
    Assert(recycler->enableScanImplicitRoots);

    uint objectIndex = 0;
    HeapBlockMap& heapBlockMap = recycler->heapBlockMap;
    while (objectIndex < allocCount)
    {
        // object is allocated during the concurrent mark or it is marked, do rescan
        LargeObjectHeader * header = this->GetHeader(objectIndex);
        objectIndex++;

        // check if the object index is not allocated
        if (header == nullptr)
        {
            continue;
        }

        // check whether the object is an implicit root
        if ((header->GetAttributes(this->heapInfo->recycler->Cookie) & ImplicitRootBit) == 0)
        {
            continue;
        }

        char * objectAddress = (char *)header->GetAddress();

        bool marked = heapBlockMap.TestAndSetMark(objectAddress);
        if (!marked)
        {
            DUMP_IMPLICIT_ROOT(recycler, objectAddress);

            // check whether the object is a leaf and doesn't need to be scanned
            if ((header->GetAttributes(this->heapInfo->recycler->Cookie) & LeafBit) != 0)
            {
                continue;
            }

            // TODO: Assume scan interior
            recycler->ScanObjectInlineInterior((void **)objectAddress, header->objectSize);
        }
    }
}

#if defined(PARTIAL_GC_ENABLED) || defined(CONCURRENT_GC_ENABLED)
bool
LargeHeapBlock::RescanOnePage(Recycler * recycler, DWORD const writeWatchFlags)
{
    Assert(this->GetPageCount() == 1);
    bool const oldNeedOOMRescan = this->needOOMRescan;

    // Reset this, we'll increment this if we OOM again
    this->needOOMRescan = false;

    // don't need to get the write watch bit if we already need to oom rescan
    if (!oldNeedOOMRescan)
    {
        if (recycler->inEndMarkOnLowMemory)
        {
            // we only do oom rescan if we are on low memory mark
            return false;
        }

        // Check the write watch bit to see if we need to rescan
        ULONG_PTR count = 1;
        DWORD pageSize = AutoSystemInfo::PageSize;
        void * written;
        if (GetWriteWatch(writeWatchFlags, this->GetBeginAddress(), AutoSystemInfo::PageSize, &written, &count, &pageSize) == 0 && (count != 1))
        {
            return false;
        }
    }

    RECYCLER_STATS_INC(recycler, markData.rescanLargePageCount);

    for (uint objectIndex = 0; objectIndex < allocCount; objectIndex++)
    {
        // object is allocated during the concurrent mark or it is marked, do rescan
        LargeObjectHeader * header = this->GetHeader(objectIndex);

        // check if the object index is not allocated
        if (header == nullptr)
        {
            continue;
        }

        char * objectAddress = (char *)header->GetAddress();

        // it is not marked, don't rescan
        if (!recycler->heapBlockMap.IsMarked(objectAddress))
        {
            continue;
        }

        unsigned char attributes = header->GetAttributes(this->heapInfo->recycler->Cookie);

#ifdef RECYCLER_STATS
        if (((attributes & FinalizeBit) != 0) && ((attributes & NewFinalizeBit) != 0))
        {
            // The concurrent thread saw a false reference to this object and marked it before the attribute was set.
            // As such, our finalizeCount is not correct. Update it now.

            RECYCLER_STATS_INC(recycler, finalizeCount);
            header->SetAttributes(this->heapInfo->recycler->Cookie, (attributes & ~NewFinalizeBit));
        }
#endif

        // check whether the object is a leaf and doesn't need to be scanned
        if ((attributes & LeafBit) != 0)
        {
            continue;
        }

        RECYCLER_STATS_INC(recycler, markData.rescanLargeObjectCount);
        RECYCLER_STATS_ADD(recycler, markData.rescanLargeByteCount, header->objectSize);

        if (!recycler->AddMark(objectAddress, header->objectSize))
        {
            this->SetNeedOOMRescan(recycler);
        }
    }
    return true;
}

size_t
LargeHeapBlock::Rescan(Recycler * recycler, bool isPartialSwept, RescanFlags flags)
{
    // Update the lastCollectAllocCount for sweep
    this->lastCollectAllocCount = this->allocCount;

    Assert(recycler->collectionState != CollectionStateConcurrentFinishMark || (flags & RescanFlags_ResetWriteWatch));

    DWORD const writeWatchFlags = (flags & RescanFlags_ResetWriteWatch? WRITE_WATCH_FLAG_RESET : 0);
    if (this->GetPageCount() == 1)
    {
        return RescanOnePage(recycler, writeWatchFlags);
    }

    // Need to rescan for finish mark even if it is done on the background thread
    if (recycler->collectionState != CollectionStateConcurrentFinishMark && recycler->IsConcurrentMarkState())
    {
        // CONCURRENT-TODO: Don't do background rescan for pages with multiple pages because
        // we don't track which page we have queued up
        return 0;
    }

    return RescanMultiPage(recycler, writeWatchFlags);
}

size_t
LargeHeapBlock::RescanMultiPage(Recycler * recycler, DWORD const writeWatchFlags)
{
    Assert(this->GetPageCount() != 1);
    DebugOnly(bool oldNeedOOMRescan = this->needOOMRescan);

    // Reset this, we'll increment this if we OOM again
    this->needOOMRescan = false;

    size_t rescanCount = 0;
    DWORD pageSize = AutoSystemInfo::PageSize;
    uint objectIndex = 0;
    char * lastPageCheckedForWriteWatch = nullptr;
    bool isLastPageCheckedForWriteWatchDirty = false;

    const HeapBlockMap& heapBlockMap = recycler->heapBlockMap;

    while (objectIndex < allocCount)
    {
        // object is allocated during the concurrent mark or it is marked, do rescan
        LargeObjectHeader * header = this->GetHeader(objectIndex);
        objectIndex++;

        // check if the object index is not allocated
        if (header == nullptr)
        {
            continue;
        }

        char * objectAddress = (char *)header->GetAddress();

        // it is not marked, don't rescan
        if (!heapBlockMap.IsMarked(objectAddress))
        {
            continue;
        }

        unsigned char attributes = header->GetAttributes(this->heapInfo->recycler->Cookie);

#ifdef RECYCLER_STATS
        if (((attributes & FinalizeBit) != 0) && ((attributes & NewFinalizeBit) != 0))
        {
            // The concurrent thread saw a false reference to this object and marked it before the attribute was set.
            // As such, our finalizeCount is not correct.  Update it now.

            RECYCLER_STATS_INC(recycler, finalizeCount);
            header->SetAttributes(this->heapInfo->recycler->Cookie, (attributes & ~NewFinalizeBit));
        }
#endif

        // check whether the object is a leaf and doesn't need to be scanned
        if ((attributes & LeafBit) != 0)
        {
            continue;
        }

#ifdef RECYCLER_STATS
        bool objectScanned = false;
#endif
        Assert(oldNeedOOMRescan || !header->markOnOOMRescan);
        // Avoid writing to the page unnecessary by checking first
        if (header->markOnOOMRescan)
        {
            if (!recycler->AddMark(objectAddress, header->objectSize))
            {
                this->SetNeedOOMRescan(recycler);
                header->markOnOOMRescan = true;

                // We need to bail out of rescan early only if the recycler is
                // trying to finish marking because of low memory. If this is
                // a regular rescan, we want to try and rescan all the objects
                // on the page. It's possible that the rescan OOMs but if the
                // object rescan does OOM, we'll set the right bit on the
                // object header. When we later rescan it in a low memory
                // situation, when the bit is set, we don't need to check for
                // write-watch etc. since we'd have already done that before
                // setting the bit in the non-low-memory rescan case.
                if (!recycler->inEndMarkOnLowMemory)
                {
                    continue;
                }

                return rescanCount;
            }
            header->markOnOOMRescan = false;
#ifdef RECYCLER_STATS
            objectScanned = true;
#endif
        }
        else if (!recycler->inEndMarkOnLowMemory)
        {
            char * objectAddressEnd = objectAddress + header->objectSize;
            // Walk through the object, checking if any of its pages have been written to
            // If it has, then queue up this object for marking
            do
            {
                char * pageStart = (char *)(((size_t)objectAddress) & ~(size_t)(AutoSystemInfo::PageSize - 1));

                /*
                * The rescan logic for large object is as follows:
                *  - We rescan the object if it was marked during concurrent mark
                *  - If it was marked, since the large object has multiple pages, we'll rescan only the parts that were changed
                *  - So for each page in the large object, check if it's been written to, and if it hasn't, skip looking at that region
                *  - If we can't get the write watch, rescan that region
                *  - However, this logic applies only if we're not rescanning because of an OOM
                *  - If we are rescanning this object because of OOM (i.e !rescanBecauseOfOOM = false), rescan the whole object
                *
                * We cache the result of the write watch and the page that it was checked on so that we don't call GetWriteWatch on the same
                * page twice and inadvertently reset the write watch on a page where we've already scanned an object
                */
                if (lastPageCheckedForWriteWatch != pageStart)
                {
                    void * written = nullptr;
                    ULONG_PTR count = 1;

                    lastPageCheckedForWriteWatch = pageStart;

                    isLastPageCheckedForWriteWatchDirty = true;

                    if (GetWriteWatch(writeWatchFlags, pageStart, AutoSystemInfo::PageSize, &written, &count, &pageSize) == 0 && (count != 1))
                    {
                        // Fall through to the case below where we'll update objectAddress and continue
                        isLastPageCheckedForWriteWatchDirty = false;
                    }
                }

                if (!isLastPageCheckedForWriteWatchDirty)
                {
                    objectAddress = pageStart + AutoSystemInfo::PageSize;
                    continue;
                }

                // We're interested in only rescanning the parts of the object that have changed, not the whole
                // object. So just queue that up for marking
                char * checkEnd = min(pageStart + AutoSystemInfo::PageSize, objectAddressEnd);
                if (!recycler->AddMark(objectAddress, (checkEnd - objectAddress)))
                {
                    this->SetNeedOOMRescan(recycler);
                    header->markOnOOMRescan = true;
                }

#ifdef RECYCLER_STATS
                objectScanned = true;
                recycler->collectionStats.markData.rescanLargePageCount++;
                recycler->collectionStats.markData.rescanLargeByteCount += (checkEnd - objectAddress);
#endif
                objectAddress = checkEnd;
                rescanCount++;
            }
            while (objectAddress < objectAddressEnd);
        }
        RECYCLER_STATS_ADD(recycler, markData.rescanLargeObjectCount, objectScanned);
    }

    return rescanCount;
}
#endif

/*
* Sweep the large heap block
*
* If there are no finalizable or weak referenced objects, and if nothing is marked
* that means that everything in this heap block is considered free. So the heap block
* can be released.
* In that case, return SweepStateEmpty
* If there are objects to be freed, first see if they are any finalizable objects. If there
* aren't any in this heap block, then this heap block can be swept concurrently. So return SweepStatePendingSweep
* If there are finalizable objects, sweep them in thread. They would have been added to the pendingDispose list
* during the finalize phase, so we return SweepStatePendingDispose.
* In any case, if the pendingDispose list is not empty, we return SweepStatePendingDispose.
* If the allocCount equals the max object count, or if there's no more space to allocate a large object,
* we return SweepStateFull, so that the HeapInfo can move this to the full block list. Otherwise,
* we return SweepStateSwept.
*/

template SweepState LargeHeapBlock::Sweep<true>(RecyclerSweep& recyclerSweep, bool queuePendingSweep);
template SweepState LargeHeapBlock::Sweep<false>(RecyclerSweep& recyclerSweep, bool queuePendingSweep);

template<bool pageheap>
SweepState
LargeHeapBlock::Sweep(RecyclerSweep& recyclerSweep, bool queuePendingSweep)
{
    Recycler * recycler = recyclerSweep.GetRecycler();

    uint markCount = GetMarkCount();
#if DBG
    Assert(this->lastCollectAllocCount == this->allocCount);
    Assert(markCount <= allocCount);
#endif

    RECYCLER_STATS_INC(recycler, heapBlockCount[HeapBlock::LargeBlockType]);

#if DBG
    this->expectedSweepCount = allocCount - markCount;
#endif

    Assert(!this->isPendingConcurrentSweep);

    bool isAllFreed = (finalizeCount == 0 && markCount == 0);
    if (isAllFreed)
    {
        recycler->NotifyFree<pageheap>(this);
        Assert(this->pendingDisposeObject == nullptr);
        return SweepStateEmpty;
    }

    RECYCLER_STATS_ADD(recycler, largeHeapBlockTotalByteCount, this->pageCount * AutoSystemInfo::PageSize);
    RECYCLER_STATS_ADD(recycler, heapBlockFreeByteCount[HeapBlock::LargeBlockType],
        addressEnd - allocAddressEnd <= HeapConstants::MaxSmallObjectSize? 0 : (size_t)(addressEnd - allocAddressEnd));

    // If the number of objects marked is not equal to the number of objects
    // that have been allocated by this large heap block, that means that there
    // could be some objects that need to be swept
    if (markCount != allocCount)
    {
        Assert(this->expectedSweepCount != 0);

        // We need to sweep in thread if there are any finalizable objects so
        // that the PrepareFinalize() can be called before concurrent sweep
        // and other finalizers. This gives the object an opportunity before any
        // other script can be ran to clean up its references/states that are not
        // valid since we've determined that the object is not live any more.
        //
        // An example is the ITrackable's tracking alias. The reference to the alias
        // object needs to be clear so that the reference will not be given out again
        // in other script during concurrent sweep or finalizer called before.

        Assert(!recyclerSweep.IsBackground());
#ifdef CONCURRENT_GC_ENABLED
        if (queuePendingSweep && finalizeCount == 0)
        {
            this->isPendingConcurrentSweep = true;
            return SweepStatePendingSweep;
        }
#else
        Assert(!queuePendingSweep);
#endif

        SweepObjects<pageheap, SweepMode_InThread>(recycler);
        if (TransferSweptObjects())
        {
            return SweepStatePendingDispose;
        }
    }
#ifdef RECYCLER_STATS
    else
    {
        Assert(expectedSweepCount == 0);
        isForceSweeping = true;
        SweepObjects<pageheap, SweepMode_InThread>(recycler);
        isForceSweeping = false;
    }
#endif
    if (this->pendingDisposeObject != nullptr)
    {
        return SweepStatePendingDispose;
    }
    return (allocCount == objectCount || addressEnd - allocAddressEnd <= HeapConstants::MaxSmallObjectSize) && this->freeList.entries == nullptr ?
    SweepStateFull : SweepStateSwept;
}

bool
LargeHeapBlock::TrimObject(Recycler* recycler, LargeObjectHeader* header, size_t sizeOfObject, bool inDispose)
{
    IdleDecommitPageAllocator* pageAllocator = recycler->GetRecyclerLargeBlockPageAllocator();
    uint pageSize = AutoSystemInfo::PageSize ;

    // If we have to trim an object, either we need to have more than one object in the
    // heap block or we're being called as a part of force-sweep or dispose
    Assert(this->allocCount > 1 || this->isForceSweeping || inDispose);

    // If we have more than 1 page of bytes to free
    // make sure that the number of bytes doesn't exceed the cap for a PageSegment
    // since this optimization can only be applied to heap blocks using page segments.
    // We also skip this optimization if the allocCount is 1 since that means
    // the heap block is empty and we've been called only because we're force sweeping.
    // So, skip the opt since we're going to be marking the heap block as empty soon
    if (sizeOfObject > pageSize &&
        this->segment->GetPageCount() <= pageAllocator->GetMaxAllocPageCount() &&
        this->allocCount > 1)
    {
        Assert(!this->hadTrimmed);

        // We want to decommit the free pages beyond 4K (the page size)
        // The way large allocations work is that at most we can have 4 objects in a large heap block
        // The first object can span multiple pages, the remaining 3 objects must all fit within a page
        // So if the object being freed is greater than 1 page, then it must be the first object
        // The objectIndex must be 0 and the header must be same as this->address
        // The end address is (baseAddress + objectSize) & ~(4k - 1)
        // The number of pages to free is (freePageEnd - freePageStart) / pageSize

        char* objectAddress = (char*) header;
        char* objectEndAddress = objectAddress + sizeof(LargeObjectHeader) + header->objectSize;

        uintptr alignmentMask = ~((uintptr) (AutoSystemInfo::PageSize - 1));

        uintptr objectFreeAddress = (uintptr) objectAddress;
        uintptr objectFreeEndAddress = ((uintptr) objectEndAddress) & alignmentMask;

        size_t bytesToFree = (objectFreeEndAddress - objectFreeAddress);

        // Verify assumptions
        // Make sure that the object being freed is the first object since
        // the expectation in a large heap block is that the first object is the largest
        // object.
        // The amount of bytes to free is always less than the size of the object being freed including its header
        // The exception is if the original object's size + header size is a multiple of the page size
        Assert(objectAddress == this->address);
        Assert(header->objectIndex == 0);
        Assert(objectFreeEndAddress <= (uintptr) objectEndAddress);
        Assert(objectFreeAddress <= objectFreeEndAddress);
        Assert(bytesToFree < sizeOfObject + sizeof(LargeObjectHeader) || (uintptr) objectEndAddress == objectFreeEndAddress);

        // If we actually have something to free, release those pages
        // Move the heap block to start from the new start address
        // Change the heap block map to contain an entry for only the pages that haven't been freed
        // Fill up the old object's unreleased memory if we have to
        Assert(bytesToFree > 0);
        Assert((bytesToFree & (AutoSystemInfo::PageSize - 1)) == 0);
        size_t freePageCount = bytesToFree / AutoSystemInfo::PageSize;
        Assert(freePageCount > 0);
        Assert(freePageCount < this->pageCount);

        // If this call to trim needs idle decommit to be suspended (e.g. dispose case)
        // check if IdleDecommit has been suspended already. If it hasn't, suspend it
        // This is to prevent reentrant idle decommits (e.g. sometimes dispose is called with
        if (inDispose)
        {
            pageAllocator->SuspendIdleDecommit();
        }

        pageAllocator->Release((char*) objectFreeAddress, freePageCount, this->GetSegment());

        if (inDispose)
        {
            pageAllocator->ResumeIdleDecommit();
        }

        // Remove the freed pages from the heap block map
        // and move the heap block to start from after the pages that were freed
        // and update the page count
        recycler->heapBlockMap.ClearHeapBlock(this->address, freePageCount);

        this->address = (char*) objectFreeEndAddress;
        this->pageCount -= freePageCount;

        FillFreeMemory(recycler, (void*) objectFreeEndAddress, (size_t) (objectEndAddress - objectFreeEndAddress));

#if DBG
        this->hadTrimmed = true;
#endif
        return true;
    }

    return false;
}


template <>
void
LargeHeapBlock::SweepObject<SweepMode_InThread>(Recycler * recycler, LargeObjectHeader * header)
{
    Assert(this->HeaderList()[header->objectIndex] == header);

    // Set the header and object to null only if this is not a finalizable object
    // If it's finalizable, it'll be zeroed out during dispose
    if ((header->GetAttributes(this->heapInfo->recycler->Cookie) & FinalizeBit) != FinalizeBit)
    {
        this->HeaderList()[header->objectIndex] = nullptr;

        size_t sizeOfObject = header->objectSize;

        bool objectTrimmed = false;

        if (this->bucket == nullptr)
        {
            objectTrimmed = TrimObject(recycler, header, sizeOfObject);
        }

        if (!objectTrimmed)
        {
            FillFreeMemory(recycler, header, sizeof(LargeObjectHeader) + sizeOfObject);
        }
    }
}

//
// Call the finalizer on the heapblock object and add it to the pending dispose list
//
void
LargeHeapBlock::FinalizeObject(Recycler* recycler, LargeObjectHeader* header)
{
    // The header count can also be null if this object has already been finalized
    // but this method should never be called if the header list header is null
    Assert(this->HeaderList()[header->objectIndex] == header);
    Assert(header->GetAttributes(this->heapInfo->recycler->Cookie) & FinalizeBit);

    // Call finalize to do clean up that needs to be done immediately
    // (e.g. Clear the ITrackable alias reference, so it can't be revived during
    // other finalizers or concurrent sweep)
    // Call it only if it hasn't already been finalized
    ((FinalizableObject *)header->GetAddress())->Finalize(false);
    header->SetNext(this->heapInfo->recycler->Cookie, this->pendingDisposeObject);
    this->pendingDisposeObject = header;

    // Null out the header in the header list- this means that this object has already
    // been finalized and is just pending dispose
    this->HeaderList()[header->objectIndex] = nullptr;

#ifdef RECYCLER_FINALIZE_CHECK
    recycler->autoHeap.pendingDisposableObjectCount++;
#endif
}

// Explicitly instantiate all the sweep modes
template void LargeHeapBlock::SweepObjects<false, SweepMode_InThread>(Recycler * recycler);
template void LargeHeapBlock::SweepObjects<true, SweepMode_InThread>(Recycler * recycler);
#ifdef CONCURRENT_GC_ENABLED
template <>
void
LargeHeapBlock::SweepObject<SweepMode_Concurrent>(Recycler * recycler, LargeObjectHeader * header)
{
    Assert(!(header->GetAttributes(this->heapInfo->recycler->Cookie) & FinalizeBit));
    Assert(this->HeaderList()[header->objectIndex] == header);
    this->HeaderList()[header->objectIndex] = nullptr;
    FillFreeMemory(recycler, header, sizeof(LargeObjectHeader) + header->objectSize);
}

// Explicitly instantiate all the sweep modes
template void LargeHeapBlock::SweepObjects<false, SweepMode_Concurrent>(Recycler * recycler);
#ifdef PARTIAL_GC_ENABLED
template <>
void
LargeHeapBlock::SweepObject<SweepMode_ConcurrentPartial>(Recycler * recycler, LargeObjectHeader * header)
{
    Assert(!(header->GetAttributes(this->heapInfo->recycler->Cookie) & FinalizeBit));
    Assert(this->HeaderList()[header->objectIndex] == header);
    this->HeaderList()[header->objectIndex] = (LargeObjectHeader *)((size_t)header | PartialFreeBit);
    DebugOnly(this->hasPartialFreeObjects = true);
}

// Explicitly instantiate all the sweep modes
template void LargeHeapBlock::SweepObjects<false, SweepMode_ConcurrentPartial>(Recycler * recycler);
#endif
#endif

//
// Walk through the objects in this heap block and call finalize
// on them if they're not marked and finalizable.
//
// At the end of this phase, if there were any finalizable objects,
// they would be in the pendingDisposeObject list. When we later call
// sweep on this heapblock, we'd simply null out the header and zero out the memory
// and then Sweep would return PendingDispose as its state
//
void LargeHeapBlock::FinalizeObjects(Recycler* recycler)
{
    const HeapBlockMap& heapBlockMap = recycler->heapBlockMap;

    for (uint i = 0; i < this->lastCollectAllocCount; i++)
    {
        LargeObjectHeader * header = this->GetHeader(i);
        if (header == nullptr)
        {
            continue;
        }

        Assert(header->objectIndex == i);

        // Skip finalization if the object is alive
        if (heapBlockMap.IsMarked(header->GetAddress()))
        {
            continue;
        }

        if ((header->GetAttributes(this->heapInfo->recycler->Cookie) & FinalizeBit) == FinalizeBit)
        {
            recycler->NotifyFree((char *)header->GetAddress(), header->objectSize);
            FinalizeObject(recycler, header);
        }
    }
}

template <bool pageheap, SweepMode mode>
void
LargeHeapBlock::SweepObjects(Recycler * recycler)
{
    Assert(mode == SweepMode_InThread || this->isPendingConcurrentSweep);

    const HeapBlockMap& heapBlockMap = recycler->heapBlockMap;
#if DBG
    uint markCount = GetMarkCount();

    // mark count included newly allocated objects
#ifdef CONCURRENT_GC_ENABLED
    Assert(expectedSweepCount == allocCount - markCount || recycler->collectionState == CollectionStateConcurrentSweep);
#else
    Assert(expectedSweepCount == allocCount - markCount);
#endif
    Assert(expectedSweepCount != 0 || isForceSweeping);
    uint sweepCount = 0;
#endif

    for (uint i = 0; i < lastCollectAllocCount; i++)
    {
        RECYCLER_STATS_ADD(recycler, objectSweepScanCount, !isForceSweeping);
        LargeObjectHeader * header = this->GetHeader(i);
        if (header == nullptr)
        {
#if DBG
            Assert(expectedSweepCount != 0);
            expectedSweepCount--;
#endif
#if DBG
            LargeAllocationVerboseTrace(recycler->GetRecyclerFlagsTable(), L"Index %d empty\n", i);
#endif
            continue;
        }

        Assert(header->objectIndex == i);

        // Skip sweep if the object is alive
        if (heapBlockMap.IsMarked(header->GetAddress()))
        {
#if DBG
            Assert((header->GetAttributes(recycler->Cookie) & NewFinalizeBit) == 0);
#endif

            RECYCLER_STATS_ADD(recycler, largeHeapBlockUsedByteCount, this->GetHeader(i)->objectSize);
            continue;
        }

        size_t objectSize = header->objectSize;
        recycler->NotifyFree((char *)header->GetAddress(), objectSize);

        SweepObject<mode>(recycler, header);

        if (this->bucket != nullptr
#ifdef RECYCLER_STATS
            && !isForceSweeping
#endif
            )
        {
            LargeHeapBlockFreeListEntry* head = this->freeList.entries;
            LargeHeapBlockFreeListEntry* entry = (LargeHeapBlockFreeListEntry*) header;
            entry->headerIndex = i;
            entry->heapBlock = this;
            entry->next = head;
            entry->objectSize = objectSize;
            this->freeList.entries = entry;
        }

#if DBG
        sweepCount++;
#endif
    }

    Assert(sweepCount == expectedSweepCount);
    this->isPendingConcurrentSweep = false;
}

bool
LargeHeapBlock::TransferSweptObjects()
{
    // TODO : Large heap block doesn't do free listing yet
    return pendingDisposeObject != nullptr;
}

void
LargeHeapBlock::DisposeObjects(Recycler * recycler)
{
    Assert(this->pendingDisposeObject != nullptr || this->hasDisposeBeenCalled);

    while (pendingDisposeObject != nullptr)
    {
#if DBG
        this->hasDisposeBeenCalled = true;
#endif

        LargeObjectHeader * header = pendingDisposeObject;
        pendingDisposeObject = header->GetNext(this->heapInfo->recycler->Cookie);
        Assert(header->GetAttributes(this->heapInfo->recycler->Cookie) & FinalizeBit);
        Assert(this->HeaderList()[header->objectIndex] == nullptr);

        void * objectAddress = header->GetAddress();
        ((FinalizableObject *)objectAddress)->Dispose(false);

        Assert(finalizeCount != 0);
        finalizeCount--;

        bool objectTrimmed = false;

        if (this->bucket == nullptr)
        {
            objectTrimmed = TrimObject(recycler, header, header->objectSize, true /* need suspend */);
        }

        // GCTODO: Consider free listing items after Dispose too
        // GCTODO: Consider compacting heap blocks- if the last n items are free, move the address pointer
        // back to before the nth item so we can bump allocate from this heap block
        if (!objectTrimmed)
        {
            FillFreeMemory(recycler, header, sizeof(LargeObjectHeader) + header->objectSize);
        }

        RECYCLER_STATS_INC(recycler, finalizeSweepCount);
#ifdef RECYCLER_FINALIZE_CHECK
        this->heapInfo->liveFinalizableObjectCount--;
        this->heapInfo->pendingDisposableObjectCount--;
#endif
    }
}

#if defined(PARTIAL_GC_ENABLED) && defined(CONCURRENT_GC_ENABLED)
void
LargeHeapBlock::PartialTransferSweptObjects()
{
    // Nothing to do
    Assert(this->hasPartialFreeObjects);
}

void
LargeHeapBlock::FinishPartialCollect(Recycler * recycler)
{
    Assert(this->hasPartialFreeObjects);
    for (uint i = 0; i < allocCount; i++)
    {
        LargeObjectHeader * header = this->HeaderList()[i];

        if (header != nullptr && IsPartialSweptHeader(header))
        {
            header = (LargeObjectHeader *)((size_t)header & ~PartialFreeBit);
            Assert(header->objectIndex == i);
            this->HeaderList()[i] = nullptr;
            FillFreeMemory(recycler, header, sizeof(LargeObjectHeader) + header->objectSize);
        }
    }
    DebugOnly(this->hasPartialFreeObjects = false);
}
#endif

void
LargeHeapBlock::EnumerateObjects(ObjectInfoBits infoBits, void (*CallBackFunction)(void * address, size_t size))
{
    for (uint i = 0; i < allocCount; i++)
    {
        LargeObjectHeader * header = this->GetHeader(i);
        if (header == nullptr)
        {
            continue;
        }
        if ((header->GetAttributes(this->heapInfo->recycler->Cookie) & infoBits) != 0)
        {
            CallBackFunction(header->GetAddress(), header->objectSize);
        }
    }
}


uint
LargeHeapBlock::GetMaxLargeObjectCount(size_t pageCount, size_t firstAllocationSize)
{
    size_t freeSize = (AutoSystemInfo::PageSize * pageCount) - firstAllocationSize - sizeof(LargeObjectHeader);
    Assert(freeSize < AutoSystemInfo::Data.dwAllocationGranularity);
    size_t objectCount = (freeSize / HeapConstants::MaxSmallObjectSize) + 1;
    Assert(objectCount <= UINT_MAX);
    return (uint)objectCount;
}

#ifdef RECYCLER_SLOW_CHECK_ENABLED
void
LargeHeapBlock::Check(bool expectFull, bool expectPending)
{
    for (uint i = 0; i < allocCount; i++)
    {
        LargeObjectHeader * header = this->HeaderList()[i];
        if (header == nullptr)
        {
            continue;
        }
#if defined(PARTIAL_GC_ENABLED) && defined(CONCURRENT_GC_ENABLED)
        header = (LargeObjectHeader *)((size_t)header & ~PartialFreeBit);
        Assert(this->hasPartialFreeObjects || header == this->HeaderList()[i]);
#endif
        Assert(header->objectIndex == i);
    }
}
#endif

void LargeHeapBlock::FillFreeMemory(Recycler * recycler, __in_bcount(size) void * address, size_t size)
{
    // For now, we don't do anything in release build because we don't reuse this memory until we return
    // the pages to the allocator which will zero out the whole page

#ifdef RECYCLER_MEMORY_VERIFY
    if (recycler->VerifyEnabled())
    {
        memset(address, Recycler::VerifyMemFill, size);
        return;
    }
#endif
#ifdef RECYCLER_FREE_MEM_FILL
    memset(address, DbgMemFill, size);
#endif
}

size_t LargeHeapBlock::GetObjectSize(void* objectAddress)
{
    LargeObjectHeader * header = GetHeader(objectAddress);

    Assert((char *)header >= this->address);

    return header->objectSize;
}


#ifdef RECYCLER_MEMORY_VERIFY


void
LargeHeapBlock::Verify(Recycler * recycler)
{
    char * lastAddress = this->address;
    uint verifyFinalizeCount = 0;
    for (uint i = 0; i < allocCount; i++)
    {
        LargeObjectHeader * header = this->HeaderList()[i];
        if (header == nullptr)
        {
            // Check if the object if on the free list
            LargeHeapBlockFreeListEntry* current = this->freeList.entries;

            while (current != nullptr)
            {
                // Verify the free listed object
                if (current->headerIndex == i)
                {
                    BYTE* objectAddress = (BYTE *)current + sizeof(LargeObjectHeader);
                    Recycler::VerifyCheck(current->heapBlock == this, L"Invalid heap block", this, current->heapBlock);
                    Recycler::VerifyCheck((char *)current >= lastAddress, L"LargeHeapBlock invalid object header order", this->address, current);
                    Recycler::VerifyCheckFill(lastAddress, (char *)current - lastAddress);
                    recycler->VerifyCheckPad(objectAddress, current->objectSize);
                    lastAddress = (char *) objectAddress + current->objectSize;
                    break;
                }

                current = current->next;
            }

            continue;
        }

        Recycler::VerifyCheck((char *)header >= lastAddress, L"LargeHeapBlock invalid object header order", this->address, header);
        Recycler::VerifyCheckFill(lastAddress, (char *)header - lastAddress);
        Recycler::VerifyCheck(header->objectIndex == i, L"LargeHeapBlock object index mismatch", this->address, &header->objectIndex);
        recycler->VerifyCheckPad((BYTE *)header->GetAddress(), header->objectSize);

        verifyFinalizeCount += ((header->GetAttributes(this->heapInfo->recycler->Cookie) & FinalizeBit) != 0);
        lastAddress = (char *)header->GetAddress() + header->objectSize;
    }

    Recycler::VerifyCheck(verifyFinalizeCount == this->finalizeCount, L"LargeHeapBlock finalize object count mismatch", this->address, &this->finalizeCount);
}
#endif

uint
LargeHeapBlock::GetMarkCount()
{
    uint markCount = 0;
    const HeapBlockMap& heapBlockMap = this->heapInfo->recycler->heapBlockMap;

    for (uint i = 0; i < allocCount; i++)
    {
        LargeObjectHeader* header = this->HeaderList()[i];
        if (header && header->objectIndex == i && heapBlockMap.IsMarked(header->GetAddress()))
        {
            markCount++;
        }
    }

    return markCount;
}

#ifdef RECYCLER_PERF_COUNTERS
void
LargeHeapBlock::UpdatePerfCountersOnFree()
{
    Assert(GetMarkCount() == 0);
    size_t usedCount = 0;
    size_t usedBytes = 0;
    for (uint i = 0; i < allocCount; i++)
    {
        LargeObjectHeader * header = this->HeaderList()[i];
        if (header == nullptr)
        {
            continue;
        }
        usedCount++;
        usedBytes += header->objectSize;
    }

    RECYCLER_PERF_COUNTER_SUB(LargeHeapBlockLiveObject, usedCount);
    RECYCLER_PERF_COUNTER_SUB(LargeHeapBlockLiveObjectSize, usedBytes);
    RECYCLER_PERF_COUNTER_SUB(LargeHeapBlockFreeObjectSize, this->GetPageCount() * AutoSystemInfo::PageSize - usedBytes);

    RECYCLER_PERF_COUNTER_SUB(LiveObject, usedCount);
    RECYCLER_PERF_COUNTER_SUB(LiveObjectSize, usedBytes);
    RECYCLER_PERF_COUNTER_SUB(FreeObjectSize, this->GetPageCount() * AutoSystemInfo::PageSize - usedBytes);
}
#endif

#ifdef PROFILE_RECYCLER_ALLOC
void *
LargeHeapBlock::GetTrackerData(void * address)
{
    Assert(Recycler::DoProfileAllocTracker());
    LargeObjectHeader * header = GetHeader(address);
    Assert((char *)header >= this->address);
    uint index = header->objectIndex;
    Assert(index < this->allocCount);
    Assert(this->HeaderList()[index] == header);
    return this->GetTrackerDataArray()[index];
}

void
LargeHeapBlock::SetTrackerData(void * address, void * data)
{
    Assert(Recycler::DoProfileAllocTracker());
    LargeObjectHeader * header = GetHeader(address);
    Assert((char *)header >= this->address);
    uint index = header->objectIndex;
    Assert(index < this->allocCount);
    Assert(this->HeaderList()[index] == header);
    this->GetTrackerDataArray()[index] = data;
}

void **
LargeHeapBlock::GetTrackerDataArray()
{
    // See LargeHeapBlock::GetAllocPlusSize for layout description
    return (void **)((char *)(this + 1) + LargeHeapBlock::GetAllocPlusSize(this->objectCount) - this->objectCount * sizeof(void *));
}
#endif
