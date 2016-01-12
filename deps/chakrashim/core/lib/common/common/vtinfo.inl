//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once

// Note: VirtualTableInfo<T>::RegisterVirtualTable can be put into .cpp file but for that
// we'll have to define quite a lot of explicit template instantiations for all classes that
// use DEFINE_VTABLE_CTOR, to fix unresolved externals.
#if DBG
// static
template <typename T>
inline INT_PTR VirtualTableInfo<T>::RegisterVirtualTable(INT_PTR vtable)
{
#if ENABLE_VALIDATE_VTABLE_CTOR
    //printf("m_vtableMapHash->Add(VirtualTableInfo<%s>::Address, dummy);\n", typeid(T).name());
#endif
    if (T::RegisterVTable)
    {
        VirtualTableRegistry::Add(vtable, typeid(T).name());
    }
    return vtable;
}
#else
// static
template <typename T>
inline INT_PTR VirtualTableInfo<T>::RegisterVirtualTable(INT_PTR vtable)
{
    return vtable;
}
#endif DBG
