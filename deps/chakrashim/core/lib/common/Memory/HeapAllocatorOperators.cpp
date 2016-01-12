//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "CommonMemoryPch.h"

//----------------------------------------
// Default operator new/delete overrides
//----------------------------------------

_Ret_maybenull_ void * __cdecl
operator new(size_t byteSize)
{
    return HeapNewNoThrowArray(char, byteSize);
}

_Ret_maybenull_ void * __cdecl
operator new[](size_t byteSize)
{
    return HeapNewNoThrowArray(char, byteSize);
}

void __cdecl
operator delete(void * obj)
{
    HeapAllocator::Instance.Free(obj, (size_t)-1);
}

void __cdecl
operator delete[](void * obj)
{
    HeapAllocator::Instance.Free(obj, (size_t)-1);
}
