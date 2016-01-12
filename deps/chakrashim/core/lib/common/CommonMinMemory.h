//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once

#include "CommonMin.h"

// === Recycler Memory Header Files ===
class FinalizableObject;
namespace Js
{
    class DefaultListLockPolicy;
}

#ifdef ENABLE_BASIC_TELEMETRY
#include "GCTelemetry.h"
#endif

#include "Memory\IdleDecommitPageAllocator.h"
#include "Memory\RecyclerPageAllocator.h"
#include "Memory\FreeObject.h"
#include "Memory\PagePool.h"

#include "DataStructures\SimpleHashTable.h"
#include "DataStructures\PageStack.h"
#include "DataStructures\ContinuousPageStack.h"

#include "Memory\RecyclerWriteBarrierManager.h"
#include "Memory\HeapConstants.h"
#include "Memory\HeapBlock.h"
#include "Memory\SmallHeapBlockAllocator.h"
#include "Memory\SmallNormalHeapBlock.h"
#include "Memory\SmallLeafHeapBlock.h"
#include "Memory\SmallFinalizableHeapBlock.h"
#include "Memory\LargeHeapBlock.h"
#include "Memory\HeapBucket.h"
#include "Memory\SmallLeafHeapBucket.h"
#include "Memory\SmallNormalHeapBucket.h"
#include "Memory\SmallFinalizableHeapBucket.h"
#include "Memory\LargeHeapBucket.h"
#include "Memory\HeapInfo.h"

#include "Memory\HeapBlockMap.h"
#include "Memory\RecyclerObjectDumper.h"
#include "Memory\RecyclerWeakReference.h"
#include "Memory\RecyclerSweep.h"
#include "Memory\RecyclerHeuristic.h"
#include "Memory\MarkContext.h"
#include "Memory\RecyclerWatsonTelemtry.h"
#include "Memory\Recycler.h"
