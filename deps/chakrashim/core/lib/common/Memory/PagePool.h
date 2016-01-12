//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
// PagePool caches freed pages in a pool for reuse, and more importantly,
// defers freeing them until ReleaseFreePages is called.
// This allows us to free the pages when we know it is multi-thread safe to do so,
// e.g. after all parallel marking is completed.

namespace Memory
{
class PagePoolPage
{
private:
    PageAllocator * pageAllocator;
    PageSegment * pageSegment;
    bool isReserved;

public:
    static PagePoolPage * New(PageAllocator * pageAllocator, bool isReserved = false)
    {
        PageSegment * pageSegment;
        PagePoolPage * newPage = (PagePoolPage *)pageAllocator->AllocPages(1, &pageSegment);
        if (newPage == nullptr)
        {
            return nullptr;
        }

        newPage->pageAllocator = pageAllocator;
        newPage->pageSegment = pageSegment;
        newPage->isReserved = isReserved;

        return newPage;
    }

    void Free()
    {
        this->pageAllocator->ReleasePages(this, 1, this->pageSegment);
    }

    bool IsReserved()
    {
        return isReserved;
    }
};

class PagePool
{
private:
    class PagePoolFreePage : public PagePoolPage
    {
    public:
        PagePoolFreePage * nextFreePage;
    };

    PageAllocator pageAllocator;
    PagePoolFreePage * freePageList;

    // List of pre-allocated pages that are
    // freed only when the page pool is destroyed
    PagePoolFreePage * reservedPageList;

public:
    PagePool(Js::ConfigFlagsTable& flagsTable) :
        pageAllocator(NULL, flagsTable, PageAllocatorType_GCThread, PageAllocator::DefaultMaxFreePageCount, false, nullptr, PageAllocator::DefaultMaxAllocPageCount, 0, true),
        freePageList(nullptr),
        reservedPageList(nullptr)
    {
    }

    void ReservePages(uint reservedPageCount)
    {
        for (uint i = 0; i < reservedPageCount; i++)
        {
            PagePoolPage* page = PagePoolPage::New(&pageAllocator, true);

            if (page == nullptr)
            {
                Js::Throw::OutOfMemory();
            }
            FreeReservedPage(page);
        }
    }

    ~PagePool()
    {
        Assert(freePageList == nullptr);

        if (reservedPageList != nullptr)
        {
            while (reservedPageList != nullptr)
            {
                PagePoolFreePage * page = reservedPageList;
                Assert(page->IsReserved());
                reservedPageList = reservedPageList->nextFreePage;
                page->Free();
            }
        }
    }

    PageAllocator * GetPageAllocator() { return &this->pageAllocator; }

    PagePoolPage * GetPage(bool useReservedPages = false)
    {
        if (freePageList != nullptr)
        {
            PagePoolPage * page = freePageList;
            freePageList = freePageList->nextFreePage;
            Assert(!page->IsReserved());
            return page;
        }

        if (useReservedPages && reservedPageList != nullptr)
        {
            PagePoolPage * page = reservedPageList;
            reservedPageList = reservedPageList->nextFreePage;
            Assert(page->IsReserved());
            return page;
        }

        return PagePoolPage::New(&pageAllocator);
    }

    void FreePage(PagePoolPage * page)
    {
        PagePoolFreePage * freePage = (PagePoolFreePage *)page;
        if (freePage->IsReserved())
        {
            return FreeReservedPage(page);

        }

        freePage->nextFreePage = freePageList;
        freePageList = freePage;
    }

    void ReleaseFreePages()
    {
        while (freePageList != nullptr)
        {
            PagePoolFreePage * page = freePageList;
            Assert(!page->IsReserved());
            freePageList = freePageList->nextFreePage;
            page->Free();
        }
    }

    void Decommit()
    {
        pageAllocator.DecommitNow();
    }

#if DBG
    bool IsEmpty() const { return (freePageList == nullptr); }
#endif

private:
    void FreeReservedPage(PagePoolPage * page)
    {
        Assert(page->IsReserved());
        PagePoolFreePage * freePage = (PagePoolFreePage *)page;

        freePage->nextFreePage = reservedPageList;
        reservedPageList = freePage;
    }
 };
}
