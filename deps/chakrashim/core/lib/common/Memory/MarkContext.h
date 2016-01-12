//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
namespace Memory
{
class Recycler;

typedef JsUtil::SynchronizedDictionary<void *, void *, NoCheckHeapAllocator, PrimeSizePolicy, RecyclerPointerComparer, JsUtil::SimpleDictionaryEntry, Js::DefaultListLockPolicy, CriticalSection> MarkMap;

class MarkContext
{
private:
    struct MarkCandidate
    {
        void ** obj;
        size_t byteCount;
    };

public:
    static const int MarkCandidateSize = sizeof(MarkCandidate);

    MarkContext(Recycler * recycler, PagePool * pagePool);
    ~MarkContext();

    void Init(uint reservedPageCount);
    void Clear();

    Recycler * GetRecycler() { return this->recycler; }

    bool AddMarkedObject(void * obj, size_t byteCount);
    bool AddTrackedObject(FinalizableObject * obj);

    template <bool parallel, bool interior>
    void Mark(void * candidate, void * parentReference);
    template <bool parallel>
    void MarkInterior(void * candidate);
    template <bool parallel, bool interior>
    void ScanObject(void ** obj, size_t byteCount);
    template <bool parallel, bool interior>
    void ScanMemory(void ** obj, size_t byteCount);
    template <bool parallel, bool interior>
    void ProcessMark();

    void MarkTrackedObject(FinalizableObject * obj);
    void ProcessTracked();

    uint Split(uint targetCount, __in_ecount(targetCount) MarkContext ** targetContexts);

    void Abort();
    void Release();

    bool HasPendingMarkObjects() const { return !markStack.IsEmpty(); }
    bool HasPendingTrackObjects() const { return !trackStack.IsEmpty(); }
    bool HasPendingObjects() const { return HasPendingMarkObjects() || HasPendingTrackObjects(); }

    PageAllocator * GetPageAllocator() { return this->pagePool->GetPageAllocator(); }

    bool IsEmpty()
    {
        if (HasPendingObjects())
        {
            return false;
        }

        Assert(pagePool->IsEmpty());
        Assert(!GetPageAllocator()->DisableAllocationOutOfMemory());
        return true;
    }

#if DBG
    void VerifyPostMarkState()
    {
        Assert(this->markStack.HasChunk());
    }
#endif

    void Cleanup()
    {
        Assert(!HasPendingObjects());
        Assert(!GetPageAllocator()->DisableAllocationOutOfMemory());
        this->pagePool->ReleaseFreePages();
    }

    void DecommitPages() { this->pagePool->Decommit(); }


#ifdef ENABLE_DEBUG_CONFIG_OPTIONS
    void SetMaxPageCount(size_t maxPageCount) { markStack.SetMaxPageCount(maxPageCount); trackStack.SetMaxPageCount(maxPageCount); }
#endif

#ifdef RECYCLER_MARK_TRACK
    void SetMarkMap(MarkMap* markMap)
    {
        this->markMap = markMap;
    }
#endif

private:
    Recycler * recycler;
    PagePool * pagePool;
    PageStack<MarkCandidate> markStack;
    PageStack<FinalizableObject *> trackStack;

#ifdef RECYCLER_MARK_TRACK
    MarkMap* markMap;

    void OnObjectMarked(void* object, void* parent);
#endif
};


}
