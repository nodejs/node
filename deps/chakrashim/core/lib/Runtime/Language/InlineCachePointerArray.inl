//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
namespace Js
{
    template<class T>
    InlineCachePointerArray<T>::InlineCachePointerArray()
        : inlineCaches(nullptr)
#if DBG
        , inlineCacheCount(0)
#endif
    {
    }

    template<class T>
    void InlineCachePointerArray<T>::EnsureInlineCaches(Recycler *const recycler, FunctionBody *const functionBody)
    {
        Assert(recycler);
        Assert(functionBody);
        Assert(functionBody->GetInlineCacheCount() != 0);

        if(inlineCaches)
        {
            Assert(functionBody->GetInlineCacheCount() == inlineCacheCount);
            return;
        }

        inlineCaches = RecyclerNewArrayZ(recycler, T *, functionBody->GetInlineCacheCount());
#if DBG
        inlineCacheCount = functionBody->GetInlineCacheCount();
#endif
    }

    template<class T>
    T *InlineCachePointerArray<T>::GetInlineCache(FunctionBody *const functionBody, const uint index) const
    {
        Assert(functionBody);
        Assert(!inlineCaches || functionBody->GetInlineCacheCount() == inlineCacheCount);
        Assert(index < functionBody->GetInlineCacheCount());
        return inlineCaches ? inlineCaches[index] : nullptr;
    }

    template<class T>
    void InlineCachePointerArray<T>::SetInlineCache(
        Recycler *const recycler,
        FunctionBody *const functionBody,
        const uint index,
        T *const inlineCache)
    {
        Assert(recycler);
        Assert(functionBody);
        Assert(!inlineCaches || functionBody->GetInlineCacheCount() == inlineCacheCount);
        Assert(index < functionBody->GetInlineCacheCount());
        Assert(inlineCache);

        EnsureInlineCaches(recycler, functionBody);
        inlineCaches[index] = inlineCache;
    }

    template<class T>
    void InlineCachePointerArray<T>::Reset()
    {
        inlineCaches = nullptr;
#if DBG
        inlineCacheCount = 0;
#endif
    }
}
