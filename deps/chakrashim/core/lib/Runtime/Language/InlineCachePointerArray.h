//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once

namespace Js
{
    template<class T>
    class InlineCachePointerArray
    {
    private:
        WriteBarrierPtr<T*> inlineCaches;
#if DBG
        uint inlineCacheCount;
#endif

    public:
        InlineCachePointerArray<T>();

    private:
        void EnsureInlineCaches(Recycler *const recycler, FunctionBody *const functionBody);

    public:
        T *GetInlineCache(FunctionBody *const functionBody, const uint index) const;
        void SetInlineCache(
            Recycler *const recycler,
            FunctionBody *const functionBody,
            const uint index,
            T *const inlineCache);
        void Reset();

        template <class Fn>
        void Map(Fn fn, uint count) const
        {
            if (NULL != inlineCaches)
            {
                for (uint i =0; i < count; i++)
                {
                    T* inlineCache = inlineCaches[i];

                    if (inlineCache != NULL)
                    {
                        fn(inlineCache);
                    }
                }
            }
        };

        PREVENT_COPY(InlineCachePointerArray)
    };
}
