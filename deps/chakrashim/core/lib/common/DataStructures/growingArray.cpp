//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "CommonDataStructuresPch.h"
#include "Common\UInt32Math.h"
#include "DataStructures\growingArray.h"

namespace JsUtil
{
    GrowingUint32HeapArray* GrowingArray<uint32, HeapAllocator>::Create(uint32 _length)
    {
        return HeapNew(GrowingUint32HeapArray, &HeapAllocator::Instance, _length);
    }

}
