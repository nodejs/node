//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once

namespace Js
{
    template <bool isGuestArena>
    class TempArenaAllocatorWrapper sealed : public FinalizableObject
    {
    private:
        ArenaAllocator allocator;
        ArenaData ** externalGuestArenaRef;
        Recycler * recycler;

        TempArenaAllocatorWrapper(__in LPCWSTR name, PageAllocator * pageAllocator, void (*outOfMemoryFunc)());

    public:


        static TempArenaAllocatorWrapper* Create(ThreadContext * threadContext);

        virtual void Finalize(bool isShutdown) override
        {
        }

        virtual void Dispose(bool isShutdown) override;
        virtual void Mark(Recycler *recycler) override { AssertMsg(false, "Mark called on object that isn't TrackableObject"); }

        ArenaAllocator *GetAllocator()
        {
            return &allocator;
        }

    };

    typedef TempArenaAllocatorWrapper<true> TempGuestArenaAllocatorObject;
    typedef TempArenaAllocatorWrapper<false> TempArenaAllocatorObject;
 }
