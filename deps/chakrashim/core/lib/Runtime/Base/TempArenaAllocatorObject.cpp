//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "RuntimeBasePch.h"

namespace Js
{
    template <bool isGuestArena>
    TempArenaAllocatorWrapper<isGuestArena>* TempArenaAllocatorWrapper<isGuestArena>::Create(ThreadContext * threadContext)
    {
        Recycler * recycler = threadContext->GetRecycler();
        TempArenaAllocatorWrapper<isGuestArena> * wrapper = RecyclerNewFinalizedLeaf(recycler, Js::TempArenaAllocatorWrapper<isGuestArena>,
            L"temp", threadContext->GetPageAllocator(), Js::Throw::OutOfMemory);
        if (isGuestArena)
        {
            wrapper->externalGuestArenaRef = recycler->RegisterExternalGuestArena(wrapper->GetAllocator());
            wrapper->recycler = recycler;
            if (wrapper->externalGuestArenaRef == nullptr)
            {
                Js::Throw::OutOfMemory();
            }
        }
        return wrapper;
    }

    template <bool isGuestArena>
    TempArenaAllocatorWrapper<isGuestArena>::TempArenaAllocatorWrapper(__in LPCWSTR name, PageAllocator * pageAllocator, void (*outOfMemoryFunc)()) :
        allocator(name, pageAllocator, outOfMemoryFunc), recycler(nullptr), externalGuestArenaRef(nullptr)
    {
    }

    template <bool isGuestArena>
    void TempArenaAllocatorWrapper<isGuestArena>::Dispose(bool isShutdown)
    {
        allocator.Clear();
        if (isGuestArena && externalGuestArenaRef != nullptr)
        {
            this->recycler->UnregisterExternalGuestArena(externalGuestArenaRef);
            externalGuestArenaRef = nullptr;
        }

        Assert(allocator.AllocatedSize() == 0);
    }

    // Explicit instantiation
    template class TempArenaAllocatorWrapper<true>;
    template class TempArenaAllocatorWrapper<false>;

} // namespace Js
