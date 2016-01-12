//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
template <typename T>
class PageStack
{
private:
    struct Chunk : public PagePoolPage
    {
        Chunk * nextChunk;
        T entries[];
    };

    static const size_t EntriesPerChunk = (AutoSystemInfo::PageSize - sizeof(Chunk)) / sizeof(T);

public:
    PageStack(PagePool * pagePool);
    ~PageStack();

    void Init(uint reservedPageCount = 0);
    void Clear();

    bool Pop(T * item);
    bool Push(T item);

    uint Split(uint targetCount, __in_ecount(targetCount) PageStack<T> ** targetStacks);

    void Abort();
    void Release();

    bool IsEmpty() const;
#if DBG
    bool HasChunk() const
    {
        return this->currentChunk != nullptr;
    }
#endif

#ifdef ENABLE_DEBUG_CONFIG_OPTIONS
    void SetMaxPageCount(size_t maxPageCount) { this->maxPageCount = max<size_t>(maxPageCount, 1); }
#endif

    static const uint MaxSplitTargets = 3;     // Not counting original stack, so this supports 4-way parallel

private:
    Chunk * CreateChunk();
    void FreeChunk(Chunk * chunk);

private:
    T * nextEntry;
    T * chunkStart;
    T * chunkEnd;
    Chunk * currentChunk;
    PagePool * pagePool;
    bool usesReservedPages;

#if DBG
    size_t count;
#endif
#ifdef ENABLE_DEBUG_CONFIG_OPTIONS
    size_t pageCount;
    size_t maxPageCount;
#endif
};


template <typename T>
__inline
bool PageStack<T>::Pop(T * item)
{
    Assert(currentChunk != nullptr);

    if (nextEntry == chunkStart)
    {
        // We're at the beginning of the chunk.  Move to the previous chunk, if any
        if (currentChunk->nextChunk == nullptr)
        {
            // All done
            Assert(count == 0);
            return false;
        }

        Chunk * temp = currentChunk;
        currentChunk = currentChunk->nextChunk;
        FreeChunk(temp);

        chunkStart = currentChunk->entries;
        chunkEnd = &currentChunk->entries[EntriesPerChunk];
        nextEntry = chunkEnd;
    }

    Assert(nextEntry > chunkStart && nextEntry <= chunkEnd);

    nextEntry--;
    *item = *nextEntry;

#if DBG
    count--;
    Assert(count == (nextEntry - chunkStart) + (pageCount - 1) * EntriesPerChunk);
#endif

    return true;
}

template <typename T>
__inline
bool PageStack<T>::Push(T item)
{
    if (nextEntry == chunkEnd)
    {
        Chunk * newChunk = CreateChunk();
        if (newChunk == nullptr)
        {
            return false;
        }

        newChunk->nextChunk = currentChunk;
        currentChunk = newChunk;

        chunkStart = currentChunk->entries;
        chunkEnd = &currentChunk->entries[EntriesPerChunk];
        nextEntry = chunkStart;
    }

    Assert(nextEntry >= chunkStart && nextEntry < chunkEnd);

    *nextEntry = item;
    nextEntry++;

#if DBG
    count++;
    Assert(count == (nextEntry - chunkStart) + (pageCount - 1) * EntriesPerChunk);
#endif

    return true;
}


template <typename T>
PageStack<T>::PageStack(PagePool * pagePool) :
    pagePool(pagePool),
    currentChunk(nullptr),
    nextEntry(nullptr),
    chunkStart(nullptr),
    chunkEnd(nullptr),
    usesReservedPages(false)
{
#ifdef ENABLE_DEBUG_CONFIG_OPTIONS
    pageCount = 0;
    maxPageCount = (size_t)-1;  // Default to no limit
#endif

#if DBG
    count = 0;
#endif
}


template <typename T>
PageStack<T>::~PageStack()
{
    Assert(currentChunk == nullptr);
    Assert(nextEntry == nullptr);
    Assert(count == 0);
    Assert(pageCount == 0);
}


template <typename T>
void PageStack<T>::Init(uint reservedPageCount)
{
    if (reservedPageCount > 0)
    {
        this->usesReservedPages = true;
        this->pagePool->ReservePages(reservedPageCount);
    }

    // Preallocate one chunk.
    Assert(currentChunk == nullptr);
    currentChunk = CreateChunk();
    if (currentChunk == nullptr)
    {
        Js::Throw::OutOfMemory();
    }
    currentChunk->nextChunk = nullptr;
    chunkStart = currentChunk->entries;
    chunkEnd = &currentChunk->entries[EntriesPerChunk];
    nextEntry = chunkStart;
}


template <typename T>
void PageStack<T>::Clear()
{
    currentChunk = nullptr;
    nextEntry = nullptr;
#if DBG
    count = 0;
    pageCount = 0;
#endif
}


template <typename T>
typename PageStack<T>::Chunk * PageStack<T>::CreateChunk()
{
#ifdef ENABLE_DEBUG_CONFIG_OPTIONS
    if (pageCount >= maxPageCount)
    {
        return nullptr;
    }
#endif
    Chunk * newChunk = (Chunk *)this->pagePool->GetPage(usesReservedPages);

    if (newChunk == nullptr)
    {
        return nullptr;
    }

#ifdef ENABLE_DEBUG_CONFIG_OPTIONS
    pageCount++;
#endif
    return newChunk;
}


template <typename T>
void PageStack<T>::FreeChunk(Chunk * chunk)
{
#ifdef ENABLE_DEBUG_CONFIG_OPTIONS
    pageCount--;
#endif
    this->pagePool->FreePage(chunk);
}


template <typename T>
uint PageStack<T>::Split(uint targetCount, __in_ecount(targetCount) PageStack<T> ** targetStacks)
{
    // Split the current stack up to [targetCount + 1] ways.
    // [targetStacks] contains the target stacks and must have [targetCount] elements.

    Assert(targetCount > 0 && targetCount <= MaxSplitTargets);
    Assert(targetStacks);
    __analysis_assume(targetCount <= MaxSplitTargets);

    Chunk * mainCurrent;
    Chunk * targetCurrents[MaxSplitTargets];

    // Do the initial split of first pages for each target stack.
    // During this, if we run out of pages, we will return a value < maxSplit to
    // indicate that the split was less than the maximum possible.

    Chunk * chunk = this->currentChunk;
    Assert(chunk != nullptr);

    // The first chunk is assigned to the main stack, and since it's already there,
    // we just advance to the next chunk and start assigning to each target stack.
    mainCurrent = chunk;
    chunk = chunk->nextChunk;

    uint targetIndex = 0;
    while (targetIndex < targetCount)
    {
        if (chunk == nullptr)
        {
            // No more pages.  Adjust targetCount down to what we were actually able to do.
            // We'll return this number below so the caller knows.
            targetCount = targetIndex;
            break;
        }

        // Target stack should be empty.
        // If it has a free page currently, release it.
        Assert(targetStacks[targetIndex]->IsEmpty());
        targetStacks[targetIndex]->Release();

        targetStacks[targetIndex]->currentChunk = chunk;
        targetStacks[targetIndex]->chunkStart = chunk->entries;
        targetStacks[targetIndex]->chunkEnd = &chunk->entries[EntriesPerChunk];
        targetStacks[targetIndex]->nextEntry = targetStacks[targetIndex]->chunkEnd;

#ifdef ENABLE_DEBUG_CONFIG_OPTIONS
        this->pageCount--;
        targetStacks[targetIndex]->pageCount = 1;
#endif
#if DBG
        this->count -= EntriesPerChunk;
        targetStacks[targetIndex]->count = EntriesPerChunk;
#endif

        targetCurrents[targetIndex] = chunk;

        chunk = chunk->nextChunk;
        targetIndex++;
    }

    // Loop through the remaining chunks (if any),
    // assigning each chunk to the main chunk and the target chunks in turn,
    // and linking each chunk to the end of the respective list.
    while (true)
    {
        if (chunk == nullptr)
        {
            break;
        }

        mainCurrent->nextChunk = chunk;
        mainCurrent = chunk;

        chunk = chunk->nextChunk;

        targetIndex = 0;
        while (targetIndex < targetCount)
        {
            if (chunk == nullptr)
            {
                break;
            }

            targetCurrents[targetIndex]->nextChunk = chunk;
            targetCurrents[targetIndex] = chunk;

#ifdef ENABLE_DEBUG_CONFIG_OPTIONS
            this->pageCount--;
            targetStacks[targetIndex]->pageCount++;
#endif
#if DBG
            this->count -= EntriesPerChunk;
            targetStacks[targetIndex]->count += EntriesPerChunk;
#endif

            chunk = chunk->nextChunk;
            targetIndex++;
        }
    }

    // Terminate all the split chunk lists with null
    mainCurrent->nextChunk = nullptr;
    targetIndex = 0;
    while (targetIndex < targetCount)
    {
        targetCurrents[targetIndex]->nextChunk = nullptr;
        targetIndex++;
    }

    // Return the actual split count we were able to do, which may have been lowered above.
    return targetCount;
}


template <typename T>
void PageStack<T>::Abort()
{
    // Abandon the current entries in the stack and reset to initialized state.

    if (currentChunk == nullptr)
    {
        Assert(count == 0);
        return;
    }

    // Free all the chunks except the first one
    while (currentChunk->nextChunk != nullptr)
    {
        Chunk * temp = currentChunk;
        currentChunk = currentChunk->nextChunk;
        FreeChunk(temp);
    }

    chunkStart = currentChunk->entries;
    chunkEnd = &currentChunk->entries[EntriesPerChunk];
    nextEntry = chunkStart;

#if DBG
    count = 0;
#endif
}


template <typename T>
void PageStack<T>::Release()
{
    Assert(IsEmpty());

    // We may have a preallocated chunk still held; if so release it.
    if (currentChunk != nullptr)
    {
        Assert(currentChunk->nextChunk == nullptr);
        FreeChunk(currentChunk);
        currentChunk = nullptr;
    }

    nextEntry = nullptr;
    chunkStart = nullptr;
    chunkEnd = nullptr;
}


template <typename T>
bool PageStack<T>::IsEmpty() const
{
    if (currentChunk == nullptr)
    {
        Assert(count == 0);
        Assert(nextEntry == nullptr);
        return true;
    }

    if (nextEntry == chunkStart && currentChunk->nextChunk == nullptr)
    {
        Assert(count == 0);
        return true;
    }

    Assert(count != 0);
    return false;
}

