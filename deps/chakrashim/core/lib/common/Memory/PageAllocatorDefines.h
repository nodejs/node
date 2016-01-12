//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once

#define PAGE_ALLOCATOR_TYPE(MACRO) \
    MACRO(Thread) \
    MACRO(Diag) \
    MACRO(CustomHeap) \
    MACRO(BGJIT) \
    MACRO(GCThread) \
    MACRO(Recycler) \

#define DEFINE_PAGE_ALLOCATOR_TYPE_ENUM(x) PageAllocatorType_ ## x,

enum PageAllocatorType
{
    PAGE_ALLOCATOR_TYPE(DEFINE_PAGE_ALLOCATOR_TYPE_ENUM)
    PageAllocatorType_Max
};

