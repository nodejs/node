//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#if DBG
typedef JsUtil::BaseDictionary<INT_PTR, LPCSTR, ArenaAllocator> VtableHashMap;
class VirtualTableRegistry
{
public:
    static void Add(INT_PTR vtable, LPCSTR className);
    static VtableHashMap * CreateVtableHashMap(ArenaAllocator * allocator);
private:
    struct TableEntry
    {
        INT_PTR vtable;
        LPCSTR className;
    };
    static TableEntry m_knownVtables[];
    static UINT m_knownVtableCount;

};
#endif
