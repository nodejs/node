//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once

#include "CommonMinMemory.h"

typedef _Return_type_success_(return >= 0) LONG NTSTATUS;
#define NT_SUCCESS(Status) (((NTSTATUS)(Status)) >= 0)

#include <wchar.h>

// === C Runtime Header Files ===
#include <stdarg.h>
#include <float.h>
#include <limits.h>
#if defined(_UCRT)
#include <cmath>
#else
#include <math.h>
#endif
#include <time.h>

#include <io.h>

#include <malloc.h>
extern "C" void * _AddressOfReturnAddress(void);

#include "Common\GetCurrentFrameID.h"

namespace Js
{
    typedef int32 PropertyId;
    typedef unsigned long ModuleID;
}

#define IsTrueOrFalse(value)     ((value) ? L"True" : L"False")

// Header files
#include "core\BinaryFeatureControl.h"
#include "TemplateParameter.h"

#include "Common\vtinfo.h"
#include "EnumClassHelp.h"
#include "Common\Tick.h"

#include "Common\Int16Math.h"
#include "Common\Int32Math.h"
#include "Common\UInt16Math.h"
#include "Common\UInt32Math.h"
#include "common\Int64Math.h"

template<typename T> struct IntMath { using Type = void; };
template<> struct IntMath<int16> { using Type = Int16Math; };
template<> struct IntMath<int32> { using Type = Int32Math; };
template<> struct IntMath<uint16> { using Type = UInt16Math; };
template<> struct IntMath<uint32> { using Type = UInt32Math; };
template<> struct IntMath<int64> { using Type = Int64Math; };

#include "Common\DaylightTimeHelper.h"
#include "Common\DateUtilities.h"
#include "Common\NumberUtilitiesBase.h"
#include "Common\NumberUtilities.h"
#include <codex\Utf8Codex.h>
#include "Common\unicode.h"

#include "core\DelayLoadLibrary.h"
#include "core\EtwTraceCore.h"

#include "Common\RejitReason.h"
#include "Common\ThreadService.h"

// Exceptions
#include "Exceptions\Exceptionbase.h"
#include "Exceptions\InternalErrorException.h"
#include "Exceptions\OutOfMemoryException.h"
#include "Exceptions\OperationAbortedException.h"
#include "Exceptions\RejitException.h"
#include "Exceptions\ScriptAbortException.h"
#include "Exceptions\StackOverflowException.h"
#include "Exceptions\NotImplementedException.h"
#include "Exceptions\AsmJsParseException.h"

#include "Memory\AutoPtr.h"
#include "Memory\AutoAllocatorObjectPtr.h"
#include "Memory\leakreport.h"

#include "DataStructures\DoublyLinkedListElement.h"
#include "DataStructures\DoublyLinkedList.h"
#include "DataStructures\SimpleHashTable.h"
#include "Memory\XDataAllocator.h"
#include "Memory\CustomHeap.h"

#include "Core\FinalizableObject.h"
#include "Memory\RecyclerRootPtr.h"
#include "Memory\RecyclerFastAllocator.h"
#include "Memory\RecyclerPointers.h"
#include "util\pinned.h"

// Data Structures 2

#include "DataStructures\StringBuilder.h"
#include "DataStructures\WeakReferenceDictionary.h"
#include "DataStructures\LeafValueDictionary.h"
#include "DataStructures\Dictionary.h"
#include "DataStructures\List.h"
#include "DataStructures\Stack.h"
#include "DataStructures\Queue.h"
#include "DataStructures\CharacterBuffer.h"
#include "DataStructures\InternalString.h"
#include "DataStructures\Interval.h"
#include "DataStructures\InternalStringNoCaseComparer.h"
#include "DataStructures\SparseArray.h"
#include "DataStructures\growingArray.h"
#include "DataStructures\EvalMapString.h"
#include "DataStructures\RegexKey.h"
#include "DataStructures\LineOffsetCache.h"

#include "core\ICustomConfigFlags.h"
#include "core\CmdParser.h"
#include "core\ProfileInstrument.h"
#include "core\ProfileMemory.h"
#include "core\StackBackTrace.h"

#include "Common\Event.h"
#include "Common\Jobs.h"

#include "common\vtregistry.h" // Depends on SimpleHashTable.h
#include "DataStructures\Cache.h" // Depends on config flags
#include "DataStructures\MruDictionary.h" // Depends on DoublyLinkedListElement

#include "common\SmartFPUControl.h"

// This class is only used by AutoExp.dat
class AutoExpDummyClass
{
};

#pragma warning(push)
#if defined(PROFILE_RECYCLER_ALLOC) || defined(HEAP_TRACK_ALLOC) || defined(ENABLE_DEBUG_CONFIG_OPTIONS)
#include <typeinfo.h>
#endif
#pragma warning(pop)

