//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once

namespace Js
{

class ScriptContextOptimizationOverrideInfo
{
public:
    ScriptContextOptimizationOverrideInfo();
    ~ScriptContextOptimizationOverrideInfo();

    static DWORD GetSideEffectsOffset() { return offsetof(ScriptContextOptimizationOverrideInfo, sideEffects); }
    static DWORD GetArraySetElementFastPathVtableOffset() { return offsetof(ScriptContextOptimizationOverrideInfo, arraySetElementFastPathVtable); }
    static DWORD GetIntArraySetElementFastPathVtableOffset() { return offsetof(ScriptContextOptimizationOverrideInfo, intArraySetElementFastPathVtable); }
    static DWORD GetFloatArraySetElementFastPathVtableOffset() { return offsetof(ScriptContextOptimizationOverrideInfo, floatArraySetElementFastPathVtable); }

    void SetSideEffects(SideEffects se);
    SideEffects GetSideEffects() { return sideEffects; }
    SideEffects * GetAddressOfSideEffects() { return &sideEffects; }

    bool IsEnabledArraySetElementFastPath() const;
    void DisableArraySetElementFastPath();
    INT_PTR GetArraySetElementFastPathVtable() const;
    INT_PTR GetIntArraySetElementFastPathVtable() const;
    INT_PTR GetFloatArraySetElementFastPathVtable() const;
    void * GetAddressOfArraySetElementFastPathVtable();
    void * GetAddressOfIntArraySetElementFastPathVtable();
    void * GetAddressOfFloatArraySetElementFastPathVtable();

    void Merge(ScriptContextOptimizationOverrideInfo * info);

    // Use a small integer so JIT'ed code can encode in a smaller instruction
    static const INT_PTR InvalidVtable = (INT_PTR)1;
private:
    // Optimization overrides
    SideEffects sideEffects;
    INT_PTR arraySetElementFastPathVtable;
    INT_PTR intArraySetElementFastPathVtable;
    INT_PTR floatArraySetElementFastPathVtable;

    // Cross site tracking
    ScriptContextOptimizationOverrideInfo * crossSiteRoot;
    ScriptContextOptimizationOverrideInfo * crossSitePrev;
    ScriptContextOptimizationOverrideInfo * crossSiteNext;

    template <typename Fn>
    void ForEachCrossSiteInfo(Fn fn);
    template <typename Fn>
    void ForEachEditingCrossSiteInfo(Fn fn);
    void Update(ScriptContextOptimizationOverrideInfo * info);
    void CopyTo(ScriptContextOptimizationOverrideInfo * info);
    void Insert(ScriptContextOptimizationOverrideInfo * info);

#if DBG
    void Verify();
#endif
};

};
