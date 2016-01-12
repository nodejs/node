//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once

namespace JsUtil
{

    template <
        class TKey,
        class TValue,
        class SizePolicy = PowerOf2SizePolicy,
        template <typename ValueOrKey> class Comparer = DefaultComparer,
        template <typename K, typename V> class Entry = SimpleDictionaryEntry,
        class LockPolicy = Js::DefaultListLockPolicy,   // Controls lock policy for read/map/write/add/remove items
        class SyncObject = CriticalSection
    >
    struct LeafValueDictionary
    {
        typedef JsUtil::SynchronizedDictionary<TKey,
            TValue,
            RecyclerLeafAllocator,
            SizePolicy,
            Comparer,
            Entry,
            LockPolicy,
            SyncObject
        > Type;
    };
};
