//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "CommonCommonPch.h"
#include "common\vtregistry.h"
#include "Common\vtinfo.h"

#if DBG
VirtualTableRegistry::TableEntry VirtualTableRegistry::m_knownVtables[MAX_KNOWN_VTABLES];
UINT VirtualTableRegistry::m_knownVtableCount = 0;

void VirtualTableRegistry::Add(INT_PTR vtable, LPCSTR className)
{
    Assert(m_knownVtableCount < MAX_KNOWN_VTABLES);
    if (m_knownVtableCount < MAX_KNOWN_VTABLES)
    {
        m_knownVtables[m_knownVtableCount].vtable = vtable;
        m_knownVtables[m_knownVtableCount].className = className;
        ++m_knownVtableCount;
    }
}

VtableHashMap *
VirtualTableRegistry::CreateVtableHashMap(ArenaAllocator * alloc)
{
    VtableHashMap * vtableHashMap = Anew(alloc, VtableHashMap, alloc, MAX_KNOWN_VTABLES);

    // All classes that derive from RecyclableObject must include DEFINE_VTABLE_CTOR which invokes VirtualTableRegistry::Add
    // at class initialization time. Here we add them to our hash table for easy lookup. Note that on a release build
    // the vtables are merged and thus not all of our types will be registered. So, we can only use this method
    // in a debug build. If we wanted use in a release build, we'll have to explicitly add the vtables to the hash and
    // then validate that we have got all the vtables by comparing in chk build against VirtualTableRegistry.
    for (UINT i=0; i < VirtualTableRegistry::m_knownVtableCount; i++)
    {
        INT_PTR vtable = VirtualTableRegistry::m_knownVtables[i].vtable;
        LPCSTR className = VirtualTableRegistry::m_knownVtables[i].className;
        vtableHashMap->Add(vtable, className);
    }
    return vtableHashMap;
}
#endif
