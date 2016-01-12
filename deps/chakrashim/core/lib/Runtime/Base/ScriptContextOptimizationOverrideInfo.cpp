//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "RuntimeBasePch.h"

namespace Js
{
ScriptContextOptimizationOverrideInfo::ScriptContextOptimizationOverrideInfo()
    : sideEffects(SideEffects_None),
    arraySetElementFastPathVtable(VirtualTableInfo<Js::JavascriptArray>::Address),
    intArraySetElementFastPathVtable(VirtualTableInfo<Js::JavascriptNativeIntArray>::Address),
    floatArraySetElementFastPathVtable(VirtualTableInfo<Js::JavascriptNativeFloatArray>::Address),
    crossSiteRoot(nullptr), crossSitePrev(nullptr), crossSiteNext(nullptr)
{
}

ScriptContextOptimizationOverrideInfo::~ScriptContextOptimizationOverrideInfo()
{
    ScriptContextOptimizationOverrideInfo * next = crossSiteNext;
    if (next != nullptr)
    {
        ScriptContextOptimizationOverrideInfo * root = crossSiteRoot;
        Assert(root != nullptr);
        if (this == root)
        {
            // Change the root
            ForEachCrossSiteInfo([next](ScriptContextOptimizationOverrideInfo * info)
            {
                info->crossSiteRoot = next;
            });
        }
        ScriptContextOptimizationOverrideInfo * prev = crossSitePrev;
        Assert(prev != nullptr);
        next->crossSitePrev = prev;
        prev->crossSiteNext = next;
    }
}

template <typename Fn>
void ScriptContextOptimizationOverrideInfo::ForEachCrossSiteInfo(Fn fn)
{
    Assert(crossSiteRoot != nullptr);
    ScriptContextOptimizationOverrideInfo * current = this;
    do
    {
        fn(current);
        current = current->crossSiteNext;
    }
    while (current != this);

}

template <typename Fn>
void ScriptContextOptimizationOverrideInfo::ForEachEditingCrossSiteInfo(Fn fn)
{
    Assert(crossSiteRoot != nullptr);
    ScriptContextOptimizationOverrideInfo * current = this;
    do
    {
        ScriptContextOptimizationOverrideInfo * next = current->crossSiteNext;
        fn(current);
        current = next;
    }
    while (current != this);
}

void
ScriptContextOptimizationOverrideInfo::Merge(ScriptContextOptimizationOverrideInfo * info)
{
    ScriptContextOptimizationOverrideInfo * thisRoot = this->crossSiteRoot;
    ScriptContextOptimizationOverrideInfo * infoRoot = info->crossSiteRoot;
    if (thisRoot == infoRoot)
    {
        if (thisRoot != nullptr)
        {
            // Both info is already in the same info group
            return;
        }

        // Both of them are null, just group them

        // Update this to be the template
        this->Update(info);

        // Initialize the cross site list
        this->crossSiteRoot = this;
        this->crossSitePrev = this;
        this->crossSiteNext = this;

        // Insert the info to the list
        this->Insert(info);
    }
    else
    {
        if (thisRoot == nullptr)
        {
            thisRoot = infoRoot;
            infoRoot = nullptr;
            info = this;
        }

        thisRoot->Update(info);

        // Spread the information on the current group
        thisRoot->ForEachCrossSiteInfo([thisRoot](ScriptContextOptimizationOverrideInfo * i)
        {
            thisRoot->CopyTo(i);
        });

        if (infoRoot == nullptr)
        {
            thisRoot->Insert(info);
        }
        else
        {
            // Insert the other group
            info->ForEachEditingCrossSiteInfo([thisRoot](ScriptContextOptimizationOverrideInfo * i)
            {
                thisRoot->Insert(i);
            });
        }
    }

    DebugOnly(Verify());
}

void
ScriptContextOptimizationOverrideInfo::CopyTo(ScriptContextOptimizationOverrideInfo * info)
{
    info->arraySetElementFastPathVtable = this->arraySetElementFastPathVtable;
    info->intArraySetElementFastPathVtable = this->intArraySetElementFastPathVtable;
    info->floatArraySetElementFastPathVtable = this->floatArraySetElementFastPathVtable;
    info->sideEffects = this->sideEffects;
}

void
ScriptContextOptimizationOverrideInfo::Insert(ScriptContextOptimizationOverrideInfo * info)
{
    // Copy the information
    this->CopyTo(info);

    // Insert
    // Only insert at the root
    Assert(this == this->crossSiteRoot);
    info->crossSiteRoot = this;
    info->crossSiteNext = this;
    info->crossSitePrev = this->crossSitePrev;
    this->crossSitePrev->crossSiteNext = info;
    this->crossSitePrev = info;
}

void
ScriptContextOptimizationOverrideInfo::Update(ScriptContextOptimizationOverrideInfo * info)
{
    if (!info->IsEnabledArraySetElementFastPath())
    {
        this->DisableArraySetElementFastPath();
    }

    this->sideEffects = (SideEffects)(this->sideEffects | info->sideEffects);
}

void
ScriptContextOptimizationOverrideInfo::SetSideEffects(SideEffects se)
{
    if (this->crossSiteRoot == nullptr)
    {
        sideEffects = (SideEffects)(sideEffects | se);
    }
    else if ((sideEffects & se) != se)
    {
        ForEachCrossSiteInfo([se](ScriptContextOptimizationOverrideInfo * info)
        {
            Assert((info->sideEffects & se) != se);
            info->sideEffects = (SideEffects)(info->sideEffects | se);
        });
    }
}
bool
ScriptContextOptimizationOverrideInfo::IsEnabledArraySetElementFastPath() const
{
    return arraySetElementFastPathVtable != InvalidVtable;
}

void
ScriptContextOptimizationOverrideInfo::DisableArraySetElementFastPath()
{
    if (this->crossSiteRoot == nullptr)
    {
        arraySetElementFastPathVtable = InvalidVtable;
        intArraySetElementFastPathVtable = InvalidVtable;
        floatArraySetElementFastPathVtable = InvalidVtable;
    }
    else if (IsEnabledArraySetElementFastPath())
    {
        // disable for all script context in the cross site group
        ForEachCrossSiteInfo([](ScriptContextOptimizationOverrideInfo * info)
        {
            Assert(info->IsEnabledArraySetElementFastPath());
            info->arraySetElementFastPathVtable = InvalidVtable;
            info->intArraySetElementFastPathVtable = InvalidVtable;
            info->floatArraySetElementFastPathVtable = InvalidVtable;
        });
    }
}

INT_PTR
ScriptContextOptimizationOverrideInfo::GetArraySetElementFastPathVtable() const
{
    return arraySetElementFastPathVtable;
}

INT_PTR
ScriptContextOptimizationOverrideInfo::GetIntArraySetElementFastPathVtable() const
{
    return intArraySetElementFastPathVtable;
}

INT_PTR
ScriptContextOptimizationOverrideInfo::GetFloatArraySetElementFastPathVtable() const
{
    return floatArraySetElementFastPathVtable;
}

void *
ScriptContextOptimizationOverrideInfo::GetAddressOfArraySetElementFastPathVtable()
{
    return &arraySetElementFastPathVtable;
}

void *
ScriptContextOptimizationOverrideInfo::GetAddressOfIntArraySetElementFastPathVtable()
{
    return &intArraySetElementFastPathVtable;
}

void *
ScriptContextOptimizationOverrideInfo::GetAddressOfFloatArraySetElementFastPathVtable()
{
    return &floatArraySetElementFastPathVtable;
}

#if DBG
void
ScriptContextOptimizationOverrideInfo::Verify()
{
    if (this->crossSiteRoot == nullptr)
    {
        return;
    }

    this->ForEachCrossSiteInfo([this](ScriptContextOptimizationOverrideInfo * i)
    {
        Assert(i->crossSiteRoot == this->crossSiteRoot);
        Assert(i->sideEffects == this->sideEffects);
        Assert(i->arraySetElementFastPathVtable == this->arraySetElementFastPathVtable);
        Assert(i->intArraySetElementFastPathVtable == this->intArraySetElementFastPathVtable);
        Assert(i->floatArraySetElementFastPathVtable == this->floatArraySetElementFastPathVtable);
    });
}
#endif
};
