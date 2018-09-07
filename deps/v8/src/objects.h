// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_H_
#define V8_OBJECTS_H_

#include <iosfwd>
#include <memory>

#include "include/v8.h"
#include "include/v8config.h"
#include "src/assert-scope.h"
#include "src/base/bits.h"
#include "src/base/build_config.h"
#include "src/base/flags.h"
#include "src/base/logging.h"
#include "src/checks.h"
#include "src/elements-kind.h"
#include "src/field-index.h"
#include "src/flags.h"
#include "src/messages.h"
#include "src/objects-definitions.h"
#include "src/property-details.h"
#include "src/roots.h"
#include "src/utils.h"

#if V8_TARGET_ARCH_ARM
#include "src/arm/constants-arm.h"  // NOLINT
#elif V8_TARGET_ARCH_ARM64
#include "src/arm64/constants-arm64.h"  // NOLINT
#elif V8_TARGET_ARCH_MIPS
#include "src/mips/constants-mips.h"  // NOLINT
#elif V8_TARGET_ARCH_MIPS64
#include "src/mips64/constants-mips64.h"  // NOLINT
#elif V8_TARGET_ARCH_PPC
#include "src/ppc/constants-ppc.h"  // NOLINT
#elif V8_TARGET_ARCH_S390
#include "src/s390/constants-s390.h"  // NOLINT
#endif

// Has to be the last include (doesn't have include guards):
#include "src/objects/object-macros.h"

//
// Most object types in the V8 JavaScript are described in this file.
//
// Inheritance hierarchy:
// - Object
//   - Smi          (immediate small integer)
//   - HeapObject   (superclass for everything allocated in the heap)
//     - JSReceiver  (suitable for property access)
//       - JSObject
//         - JSArray
//         - JSArrayBuffer
//         - JSArrayBufferView
//           - JSTypedArray
//           - JSDataView
//         - JSBoundFunction
//         - JSCollection
//           - JSSet
//           - JSMap
//         - JSStringIterator
//         - JSSetIterator
//         - JSMapIterator
//         - JSWeakCollection
//           - JSWeakMap
//           - JSWeakSet
//         - JSRegExp
//         - JSFunction
//         - JSGeneratorObject
//         - JSGlobalObject
//         - JSGlobalProxy
//         - JSValue
//           - JSDate
//         - JSMessageObject
//         - JSModuleNamespace
//         - JSLocale  // If V8_INTL_SUPPORT enabled.
//         - JSRelativeTimeFormat  // If V8_INTL_SUPPORT enabled.
//         - WasmGlobalObject
//         - WasmInstanceObject
//         - WasmMemoryObject
//         - WasmModuleObject
//         - WasmTableObject
//       - JSProxy
//     - FixedArrayBase
//       - ByteArray
//       - BytecodeArray
//       - FixedArray
//         - DescriptorArray
//         - FrameArray
//         - HashTable
//           - Dictionary
//           - StringTable
//           - StringSet
//           - CompilationCacheTable
//           - MapCache
//         - OrderedHashTable
//           - OrderedHashSet
//           - OrderedHashMap
//         - Context
//         - FeedbackMetadata
//         - TemplateList
//         - TransitionArray
//         - ScopeInfo
//         - ModuleInfo
//         - ScriptContextTable
//         - FixedArrayOfWeakCells
//       - FixedDoubleArray
//     - Name
//       - String
//         - SeqString
//           - SeqOneByteString
//           - SeqTwoByteString
//         - SlicedString
//         - ConsString
//         - ThinString
//         - ExternalString
//           - ExternalOneByteString
//           - ExternalTwoByteString
//         - InternalizedString
//           - SeqInternalizedString
//             - SeqOneByteInternalizedString
//             - SeqTwoByteInternalizedString
//           - ConsInternalizedString
//           - ExternalInternalizedString
//             - ExternalOneByteInternalizedString
//             - ExternalTwoByteInternalizedString
//       - Symbol
//     - HeapNumber
//     - BigInt
//     - Cell
//     - PropertyCell
//     - PropertyArray
//     - Code
//     - AbstractCode, a wrapper around Code or BytecodeArray
//     - Map
//     - Oddball
//     - Foreign
//     - SmallOrderedHashTable
//       - SmallOrderedHashMap
//       - SmallOrderedHashSet
//     - SharedFunctionInfo
//     - Struct
//       - AccessorInfo
//       - PromiseReaction
//       - PromiseCapability
//       - AccessorPair
//       - AccessCheckInfo
//       - InterceptorInfo
//       - CallHandlerInfo
//       - EnumCache
//       - TemplateInfo
//         - FunctionTemplateInfo
//         - ObjectTemplateInfo
//       - Script
//       - DebugInfo
//       - BreakPoint
//       - BreakPointInfo
//       - StackFrameInfo
//       - SourcePositionTableWithFrameCache
//       - CodeCache
//       - PrototypeInfo
//       - Microtask
//         - CallbackTask
//         - CallableTask
//         - PromiseReactionJobTask
//           - PromiseFulfillReactionJobTask
//           - PromiseRejectReactionJobTask
//         - PromiseResolveThenableJobTask
//       - Module
//       - ModuleInfoEntry
//     - WeakCell
//     - FeedbackCell
//     - FeedbackVector
//     - PreParsedScopeData
//     - UncompiledData
//       - UncompiledDataWithoutPreParsedScope
//       - UncompiledDataWithPreParsedScope
//
// Formats of Object*:
//  Smi:        [31 bit signed int] 0
//  HeapObject: [32 bit direct pointer] (4 byte aligned) | 01

namespace v8 {
namespace internal {

struct InliningPosition;
class PropertyDescriptorObject;

enum KeyedAccessLoadMode {
  STANDARD_LOAD,
  LOAD_IGNORE_OUT_OF_BOUNDS,
};

enum KeyedAccessStoreMode {
  STANDARD_STORE,
  STORE_TRANSITION_TO_OBJECT,
  STORE_TRANSITION_TO_DOUBLE,
  STORE_AND_GROW_NO_TRANSITION_HANDLE_COW,
  STORE_AND_GROW_TRANSITION_TO_OBJECT,
  STORE_AND_GROW_TRANSITION_TO_DOUBLE,
  STORE_NO_TRANSITION_IGNORE_OUT_OF_BOUNDS,
  STORE_NO_TRANSITION_HANDLE_COW
};

enum MutableMode {
  MUTABLE,
  IMMUTABLE
};


static inline bool IsTransitionStoreMode(KeyedAccessStoreMode store_mode) {
  return store_mode == STORE_TRANSITION_TO_OBJECT ||
         store_mode == STORE_TRANSITION_TO_DOUBLE ||
         store_mode == STORE_AND_GROW_TRANSITION_TO_OBJECT ||
         store_mode == STORE_AND_GROW_TRANSITION_TO_DOUBLE;
}

static inline bool IsCOWHandlingStoreMode(KeyedAccessStoreMode store_mode) {
  return store_mode == STORE_NO_TRANSITION_HANDLE_COW ||
         store_mode == STORE_AND_GROW_NO_TRANSITION_HANDLE_COW;
}

static inline KeyedAccessStoreMode GetNonTransitioningStoreMode(
    KeyedAccessStoreMode store_mode, bool receiver_was_cow) {
  switch (store_mode) {
    case STORE_AND_GROW_NO_TRANSITION_HANDLE_COW:
    case STORE_AND_GROW_TRANSITION_TO_OBJECT:
    case STORE_AND_GROW_TRANSITION_TO_DOUBLE:
      store_mode = STORE_AND_GROW_NO_TRANSITION_HANDLE_COW;
      break;
    case STANDARD_STORE:
    case STORE_TRANSITION_TO_OBJECT:
    case STORE_TRANSITION_TO_DOUBLE:
      store_mode =
          receiver_was_cow ? STORE_NO_TRANSITION_HANDLE_COW : STANDARD_STORE;
      break;
    case STORE_NO_TRANSITION_IGNORE_OUT_OF_BOUNDS:
    case STORE_NO_TRANSITION_HANDLE_COW:
      break;
  }
  DCHECK(!IsTransitionStoreMode(store_mode));
  DCHECK_IMPLIES(receiver_was_cow, IsCOWHandlingStoreMode(store_mode));
  return store_mode;
}


static inline bool IsGrowStoreMode(KeyedAccessStoreMode store_mode) {
  return store_mode >= STORE_AND_GROW_NO_TRANSITION_HANDLE_COW &&
         store_mode <= STORE_AND_GROW_TRANSITION_TO_DOUBLE;
}


enum IcCheckType { ELEMENT, PROPERTY };


// SKIP_WRITE_BARRIER skips the write barrier.
// UPDATE_WEAK_WRITE_BARRIER skips the marking part of the write barrier and
// only performs the generational part.
// UPDATE_WRITE_BARRIER is doing the full barrier, marking and generational.
enum WriteBarrierMode {
  SKIP_WRITE_BARRIER,
  UPDATE_WEAK_WRITE_BARRIER,
  UPDATE_WRITE_BARRIER
};


// PropertyNormalizationMode is used to specify whether to keep
// inobject properties when normalizing properties of a JSObject.
enum PropertyNormalizationMode {
  CLEAR_INOBJECT_PROPERTIES,
  KEEP_INOBJECT_PROPERTIES
};


// Indicates whether transitions can be added to a source map or not.
enum TransitionFlag {
  INSERT_TRANSITION,
  OMIT_TRANSITION
};


// Indicates whether the transition is simple: the target map of the transition
// either extends the current map with a new property, or it modifies the
// property that was added last to the current map.
enum SimpleTransitionFlag {
  SIMPLE_PROPERTY_TRANSITION,
  PROPERTY_TRANSITION,
  SPECIAL_TRANSITION
};

// Indicates whether we are only interested in the descriptors of a particular
// map, or in all descriptors in the descriptor array.
enum DescriptorFlag {
  ALL_DESCRIPTORS,
  OWN_DESCRIPTORS
};

// Instance size sentinel for objects of variable size.
const int kVariableSizeSentinel = 0;

// We may store the unsigned bit field as signed Smi value and do not
// use the sign bit.
const int kStubMajorKeyBits = 8;
const int kStubMinorKeyBits = kSmiValueSize - kStubMajorKeyBits - 1;

// We use the full 16 bits of the instance_type field to encode heap object
// instance types. All the high-order bits (bit 7-15) are cleared if the object
// is a string, and contain set bits if it is not a string.
const uint32_t kIsNotStringMask = 0xff80;
const uint32_t kStringTag = 0x0;

// Bit 6 indicates that the object is an internalized string (if set) or not.
// Bit 7 has to be clear as well.
const uint32_t kIsNotInternalizedMask = 0x40;
const uint32_t kNotInternalizedTag = 0x40;
const uint32_t kInternalizedTag = 0x0;

// If bit 7 is clear then bit 3 indicates whether the string consists of
// two-byte characters or one-byte characters.
const uint32_t kStringEncodingMask = 0x8;
const uint32_t kTwoByteStringTag = 0x0;
const uint32_t kOneByteStringTag = 0x8;

// If bit 7 is clear, the low-order 3 bits indicate the representation
// of the string.
const uint32_t kStringRepresentationMask = 0x07;
enum StringRepresentationTag {
  kSeqStringTag = 0x0,
  kConsStringTag = 0x1,
  kExternalStringTag = 0x2,
  kSlicedStringTag = 0x3,
  kThinStringTag = 0x5
};
const uint32_t kIsIndirectStringMask = 0x1;
const uint32_t kIsIndirectStringTag = 0x1;
STATIC_ASSERT((kSeqStringTag & kIsIndirectStringMask) == 0);  // NOLINT
STATIC_ASSERT((kExternalStringTag & kIsIndirectStringMask) == 0);  // NOLINT
STATIC_ASSERT((kConsStringTag &
               kIsIndirectStringMask) == kIsIndirectStringTag);  // NOLINT
STATIC_ASSERT((kSlicedStringTag &
               kIsIndirectStringMask) == kIsIndirectStringTag);  // NOLINT
STATIC_ASSERT((kThinStringTag & kIsIndirectStringMask) == kIsIndirectStringTag);

// If bit 7 is clear, then bit 4 indicates whether this two-byte
// string actually contains one byte data.
const uint32_t kOneByteDataHintMask = 0x10;
const uint32_t kOneByteDataHintTag = 0x10;

// If bit 7 is clear and string representation indicates an external string,
// then bit 5 indicates whether the data pointer is cached.
const uint32_t kShortExternalStringMask = 0x20;
const uint32_t kShortExternalStringTag = 0x20;

// A ConsString with an empty string as the right side is a candidate
// for being shortcut by the garbage collector. We don't allocate any
// non-flat internalized strings, so we do not shortcut them thereby
// avoiding turning internalized strings into strings. The bit-masks
// below contain the internalized bit as additional safety.
// See heap.cc, mark-compact.cc and objects-visiting.cc.
const uint32_t kShortcutTypeMask =
    kIsNotStringMask |
    kIsNotInternalizedMask |
    kStringRepresentationMask;
const uint32_t kShortcutTypeTag = kConsStringTag | kNotInternalizedTag;

static inline bool IsShortcutCandidate(int type) {
  return ((type & kShortcutTypeMask) == kShortcutTypeTag);
}

enum InstanceType : uint16_t {
  // String types.
  INTERNALIZED_STRING_TYPE = kTwoByteStringTag | kSeqStringTag |
                             kInternalizedTag,  // FIRST_PRIMITIVE_TYPE
  ONE_BYTE_INTERNALIZED_STRING_TYPE =
      kOneByteStringTag | kSeqStringTag | kInternalizedTag,
  EXTERNAL_INTERNALIZED_STRING_TYPE =
      kTwoByteStringTag | kExternalStringTag | kInternalizedTag,
  EXTERNAL_ONE_BYTE_INTERNALIZED_STRING_TYPE =
      kOneByteStringTag | kExternalStringTag | kInternalizedTag,
  EXTERNAL_INTERNALIZED_STRING_WITH_ONE_BYTE_DATA_TYPE =
      EXTERNAL_INTERNALIZED_STRING_TYPE | kOneByteDataHintTag |
      kInternalizedTag,
  SHORT_EXTERNAL_INTERNALIZED_STRING_TYPE = EXTERNAL_INTERNALIZED_STRING_TYPE |
                                            kShortExternalStringTag |
                                            kInternalizedTag,
  SHORT_EXTERNAL_ONE_BYTE_INTERNALIZED_STRING_TYPE =
      EXTERNAL_ONE_BYTE_INTERNALIZED_STRING_TYPE | kShortExternalStringTag |
      kInternalizedTag,
  SHORT_EXTERNAL_INTERNALIZED_STRING_WITH_ONE_BYTE_DATA_TYPE =
      EXTERNAL_INTERNALIZED_STRING_WITH_ONE_BYTE_DATA_TYPE |
      kShortExternalStringTag | kInternalizedTag,
  STRING_TYPE = INTERNALIZED_STRING_TYPE | kNotInternalizedTag,
  ONE_BYTE_STRING_TYPE =
      ONE_BYTE_INTERNALIZED_STRING_TYPE | kNotInternalizedTag,
  CONS_STRING_TYPE = kTwoByteStringTag | kConsStringTag | kNotInternalizedTag,
  CONS_ONE_BYTE_STRING_TYPE =
      kOneByteStringTag | kConsStringTag | kNotInternalizedTag,
  SLICED_STRING_TYPE =
      kTwoByteStringTag | kSlicedStringTag | kNotInternalizedTag,
  SLICED_ONE_BYTE_STRING_TYPE =
      kOneByteStringTag | kSlicedStringTag | kNotInternalizedTag,
  EXTERNAL_STRING_TYPE =
      EXTERNAL_INTERNALIZED_STRING_TYPE | kNotInternalizedTag,
  EXTERNAL_ONE_BYTE_STRING_TYPE =
      EXTERNAL_ONE_BYTE_INTERNALIZED_STRING_TYPE | kNotInternalizedTag,
  EXTERNAL_STRING_WITH_ONE_BYTE_DATA_TYPE =
      EXTERNAL_INTERNALIZED_STRING_WITH_ONE_BYTE_DATA_TYPE |
      kNotInternalizedTag,
  SHORT_EXTERNAL_STRING_TYPE =
      SHORT_EXTERNAL_INTERNALIZED_STRING_TYPE | kNotInternalizedTag,
  SHORT_EXTERNAL_ONE_BYTE_STRING_TYPE =
      SHORT_EXTERNAL_ONE_BYTE_INTERNALIZED_STRING_TYPE | kNotInternalizedTag,
  SHORT_EXTERNAL_STRING_WITH_ONE_BYTE_DATA_TYPE =
      SHORT_EXTERNAL_INTERNALIZED_STRING_WITH_ONE_BYTE_DATA_TYPE |
      kNotInternalizedTag,
  THIN_STRING_TYPE = kTwoByteStringTag | kThinStringTag | kNotInternalizedTag,
  THIN_ONE_BYTE_STRING_TYPE =
      kOneByteStringTag | kThinStringTag | kNotInternalizedTag,

  // Non-string names
  SYMBOL_TYPE =
      1 + (kIsNotInternalizedMask | kShortExternalStringMask |
           kOneByteDataHintMask | kStringEncodingMask |
           kStringRepresentationMask),  // FIRST_NONSTRING_TYPE, LAST_NAME_TYPE

  // Other primitives (cannot contain non-map-word pointers to heap objects).
  HEAP_NUMBER_TYPE,
  BIGINT_TYPE,
  ODDBALL_TYPE,  // LAST_PRIMITIVE_TYPE

  // Objects allocated in their own spaces (never in new space).
  MAP_TYPE,
  CODE_TYPE,

  // "Data", objects that cannot contain non-map-word pointers to heap
  // objects.
  MUTABLE_HEAP_NUMBER_TYPE,
  FOREIGN_TYPE,
  BYTE_ARRAY_TYPE,
  BYTECODE_ARRAY_TYPE,
  FREE_SPACE_TYPE,
  FIXED_INT8_ARRAY_TYPE,  // FIRST_FIXED_TYPED_ARRAY_TYPE
  FIXED_UINT8_ARRAY_TYPE,
  FIXED_INT16_ARRAY_TYPE,
  FIXED_UINT16_ARRAY_TYPE,
  FIXED_INT32_ARRAY_TYPE,
  FIXED_UINT32_ARRAY_TYPE,
  FIXED_FLOAT32_ARRAY_TYPE,
  FIXED_FLOAT64_ARRAY_TYPE,
  FIXED_UINT8_CLAMPED_ARRAY_TYPE,
  FIXED_BIGINT64_ARRAY_TYPE,
  FIXED_BIGUINT64_ARRAY_TYPE,  // LAST_FIXED_TYPED_ARRAY_TYPE
  FIXED_DOUBLE_ARRAY_TYPE,
  FEEDBACK_METADATA_TYPE,
  FILLER_TYPE,  // LAST_DATA_TYPE

  // Structs.
  ACCESS_CHECK_INFO_TYPE,
  ACCESSOR_INFO_TYPE,
  ACCESSOR_PAIR_TYPE,
  ALIASED_ARGUMENTS_ENTRY_TYPE,
  ALLOCATION_MEMENTO_TYPE,
  ASYNC_GENERATOR_REQUEST_TYPE,
  DEBUG_INFO_TYPE,
  FUNCTION_TEMPLATE_INFO_TYPE,
  INTERCEPTOR_INFO_TYPE,
  INTERPRETER_DATA_TYPE,
  MODULE_INFO_ENTRY_TYPE,
  MODULE_TYPE,
  OBJECT_TEMPLATE_INFO_TYPE,
  PROMISE_CAPABILITY_TYPE,
  PROMISE_REACTION_TYPE,
  PROTOTYPE_INFO_TYPE,
  SCRIPT_TYPE,
  STACK_FRAME_INFO_TYPE,
  TUPLE2_TYPE,
  TUPLE3_TYPE,
  ARRAY_BOILERPLATE_DESCRIPTION_TYPE,
  WASM_DEBUG_INFO_TYPE,
  WASM_EXPORTED_FUNCTION_DATA_TYPE,

  CALLABLE_TASK_TYPE,  // FIRST_MICROTASK_TYPE
  CALLBACK_TASK_TYPE,
  PROMISE_FULFILL_REACTION_JOB_TASK_TYPE,
  PROMISE_REJECT_REACTION_JOB_TASK_TYPE,
  PROMISE_RESOLVE_THENABLE_JOB_TASK_TYPE,  // LAST_MICROTASK_TYPE

  ALLOCATION_SITE_TYPE,
  // FixedArrays.
  FIXED_ARRAY_TYPE,  // FIRST_FIXED_ARRAY_TYPE
  OBJECT_BOILERPLATE_DESCRIPTION_TYPE,
  HASH_TABLE_TYPE,        // FIRST_HASH_TABLE_TYPE
  ORDERED_HASH_MAP_TYPE,  // FIRST_DICTIONARY_TYPE
  ORDERED_HASH_SET_TYPE,
  NAME_DICTIONARY_TYPE,
  GLOBAL_DICTIONARY_TYPE,
  NUMBER_DICTIONARY_TYPE,
  SIMPLE_NUMBER_DICTIONARY_TYPE,  // LAST_DICTIONARY_TYPE
  STRING_TABLE_TYPE,              // LAST_HASH_TABLE_TYPE
  EPHEMERON_HASH_TABLE_TYPE,
  SCOPE_INFO_TYPE,
  SCRIPT_CONTEXT_TABLE_TYPE,
  BLOCK_CONTEXT_TYPE,  // FIRST_CONTEXT_TYPE
  CATCH_CONTEXT_TYPE,
  DEBUG_EVALUATE_CONTEXT_TYPE,
  EVAL_CONTEXT_TYPE,
  FUNCTION_CONTEXT_TYPE,
  MODULE_CONTEXT_TYPE,
  NATIVE_CONTEXT_TYPE,
  SCRIPT_CONTEXT_TYPE,
  WITH_CONTEXT_TYPE,  // LAST_FIXED_ARRAY_TYPE, LAST_CONTEXT_TYPE

  WEAK_FIXED_ARRAY_TYPE,  // FIRST_WEAK_FIXED_ARRAY_TYPE
  DESCRIPTOR_ARRAY_TYPE,
  TRANSITION_ARRAY_TYPE,  // LAST_WEAK_FIXED_ARRAY_TYPE

  // Misc.
  CALL_HANDLER_INFO_TYPE,
  CELL_TYPE,
  CODE_DATA_CONTAINER_TYPE,
  FEEDBACK_CELL_TYPE,
  FEEDBACK_VECTOR_TYPE,
  LOAD_HANDLER_TYPE,
  PRE_PARSED_SCOPE_DATA_TYPE,
  PROPERTY_ARRAY_TYPE,
  PROPERTY_CELL_TYPE,
  SHARED_FUNCTION_INFO_TYPE,
  SMALL_ORDERED_HASH_MAP_TYPE,
  SMALL_ORDERED_HASH_SET_TYPE,
  STORE_HANDLER_TYPE,
  UNCOMPILED_DATA_WITHOUT_PRE_PARSED_SCOPE_TYPE,
  UNCOMPILED_DATA_WITH_PRE_PARSED_SCOPE_TYPE,
  WEAK_CELL_TYPE,
  WEAK_ARRAY_LIST_TYPE,

  // All the following types are subtypes of JSReceiver, which corresponds to
  // objects in the JS sense. The first and the last type in this range are
  // the two forms of function. This organization enables using the same
  // compares for checking the JS_RECEIVER and the NONCALLABLE_JS_OBJECT range.
  // Some of the following instance types are exposed in v8.h, so to not
  // unnecessarily change the ABI when we introduce new instance types in the
  // future, we leave some space between instance types.
  JS_PROXY_TYPE = 0x0400,  // FIRST_JS_RECEIVER_TYPE
  JS_GLOBAL_OBJECT_TYPE,   // FIRST_JS_OBJECT_TYPE
  JS_GLOBAL_PROXY_TYPE,
  JS_MODULE_NAMESPACE_TYPE,
  // Like JS_API_OBJECT_TYPE, but requires access checks and/or has
  // interceptors.
  JS_SPECIAL_API_OBJECT_TYPE = 0x0410,  // LAST_SPECIAL_RECEIVER_TYPE
  JS_VALUE_TYPE,                        // LAST_CUSTOM_ELEMENTS_RECEIVER
  // Like JS_OBJECT_TYPE, but created from API function.
  JS_API_OBJECT_TYPE = 0x0420,
  JS_OBJECT_TYPE,
  JS_ARGUMENTS_TYPE,
  JS_ARRAY_BUFFER_TYPE,
  JS_ARRAY_ITERATOR_TYPE,
  JS_ARRAY_TYPE,
  JS_ASYNC_FROM_SYNC_ITERATOR_TYPE,
  JS_ASYNC_GENERATOR_OBJECT_TYPE,
  JS_CONTEXT_EXTENSION_OBJECT_TYPE,
  JS_DATE_TYPE,
  JS_ERROR_TYPE,
  JS_GENERATOR_OBJECT_TYPE,
  JS_MAP_TYPE,
  JS_MAP_KEY_ITERATOR_TYPE,
  JS_MAP_KEY_VALUE_ITERATOR_TYPE,
  JS_MAP_VALUE_ITERATOR_TYPE,
  JS_MESSAGE_OBJECT_TYPE,
  JS_PROMISE_TYPE,
  JS_REGEXP_TYPE,
  JS_REGEXP_STRING_ITERATOR_TYPE,
  JS_SET_TYPE,
  JS_SET_KEY_VALUE_ITERATOR_TYPE,
  JS_SET_VALUE_ITERATOR_TYPE,
  JS_STRING_ITERATOR_TYPE,
  JS_WEAK_MAP_TYPE,
  JS_WEAK_SET_TYPE,

  JS_TYPED_ARRAY_TYPE,
  JS_DATA_VIEW_TYPE,

#ifdef V8_INTL_SUPPORT
  JS_INTL_LOCALE_TYPE,
  JS_INTL_RELATIVE_TIME_FORMAT_TYPE,
#endif  // V8_INTL_SUPPORT

  WASM_GLOBAL_TYPE,
  WASM_INSTANCE_TYPE,
  WASM_MEMORY_TYPE,
  WASM_MODULE_TYPE,
  WASM_TABLE_TYPE,
  JS_BOUND_FUNCTION_TYPE,
  JS_FUNCTION_TYPE,  // LAST_JS_OBJECT_TYPE, LAST_JS_RECEIVER_TYPE

  // Pseudo-types
  FIRST_TYPE = 0x0,
  LAST_TYPE = JS_FUNCTION_TYPE,
  FIRST_NAME_TYPE = FIRST_TYPE,
  LAST_NAME_TYPE = SYMBOL_TYPE,
  FIRST_UNIQUE_NAME_TYPE = INTERNALIZED_STRING_TYPE,
  LAST_UNIQUE_NAME_TYPE = SYMBOL_TYPE,
  FIRST_NONSTRING_TYPE = SYMBOL_TYPE,
  FIRST_PRIMITIVE_TYPE = FIRST_NAME_TYPE,
  LAST_PRIMITIVE_TYPE = ODDBALL_TYPE,
  FIRST_FUNCTION_TYPE = JS_BOUND_FUNCTION_TYPE,
  LAST_FUNCTION_TYPE = JS_FUNCTION_TYPE,
  // Boundaries for testing if given HeapObject is a subclass of FixedArray.
  FIRST_FIXED_ARRAY_TYPE = FIXED_ARRAY_TYPE,
  LAST_FIXED_ARRAY_TYPE = WITH_CONTEXT_TYPE,
  // Boundaries for testing if given HeapObject is a subclass of HashTable
  FIRST_HASH_TABLE_TYPE = HASH_TABLE_TYPE,
  LAST_HASH_TABLE_TYPE = STRING_TABLE_TYPE,
  // Boundaries for testing if given HeapObject is a subclass of Dictionary
  FIRST_DICTIONARY_TYPE = ORDERED_HASH_MAP_TYPE,
  LAST_DICTIONARY_TYPE = SIMPLE_NUMBER_DICTIONARY_TYPE,
  // Boundaries for testing if given HeapObject is a subclass of WeakFixedArray.
  FIRST_WEAK_FIXED_ARRAY_TYPE = WEAK_FIXED_ARRAY_TYPE,
  LAST_WEAK_FIXED_ARRAY_TYPE = TRANSITION_ARRAY_TYPE,
  // Boundaries for testing if given HeapObject is a Context
  FIRST_CONTEXT_TYPE = BLOCK_CONTEXT_TYPE,
  LAST_CONTEXT_TYPE = WITH_CONTEXT_TYPE,
  // Boundaries for testing if given HeapObject is a subclass of Microtask.
  FIRST_MICROTASK_TYPE = CALLABLE_TASK_TYPE,
  LAST_MICROTASK_TYPE = PROMISE_RESOLVE_THENABLE_JOB_TASK_TYPE,
  // Boundaries for testing for a fixed typed array.
  FIRST_FIXED_TYPED_ARRAY_TYPE = FIXED_INT8_ARRAY_TYPE,
  LAST_FIXED_TYPED_ARRAY_TYPE = FIXED_BIGUINT64_ARRAY_TYPE,
  // Boundary for promotion to old space.
  LAST_DATA_TYPE = FILLER_TYPE,
  // Boundary for objects represented as JSReceiver (i.e. JSObject or JSProxy).
  // Note that there is no range for JSObject or JSProxy, since their subtypes
  // are not continuous in this enum! The enum ranges instead reflect the
  // external class names, where proxies are treated as either ordinary objects,
  // or functions.
  FIRST_JS_RECEIVER_TYPE = JS_PROXY_TYPE,
  LAST_JS_RECEIVER_TYPE = LAST_TYPE,
  // Boundaries for testing the types represented as JSObject
  FIRST_JS_OBJECT_TYPE = JS_GLOBAL_OBJECT_TYPE,
  LAST_JS_OBJECT_TYPE = LAST_TYPE,
  // Boundary for testing JSReceivers that need special property lookup handling
  LAST_SPECIAL_RECEIVER_TYPE = JS_SPECIAL_API_OBJECT_TYPE,
  // Boundary case for testing JSReceivers that may have elements while having
  // an empty fixed array as elements backing store. This is true for string
  // wrappers.
  LAST_CUSTOM_ELEMENTS_RECEIVER = JS_VALUE_TYPE,

  FIRST_SET_ITERATOR_TYPE = JS_SET_KEY_VALUE_ITERATOR_TYPE,
  LAST_SET_ITERATOR_TYPE = JS_SET_VALUE_ITERATOR_TYPE,

  FIRST_MAP_ITERATOR_TYPE = JS_MAP_KEY_ITERATOR_TYPE,
  LAST_MAP_ITERATOR_TYPE = JS_MAP_VALUE_ITERATOR_TYPE,
};

STATIC_ASSERT((FIRST_NONSTRING_TYPE & kIsNotStringMask) != kStringTag);
STATIC_ASSERT(JS_OBJECT_TYPE == Internals::kJSObjectType);
STATIC_ASSERT(JS_API_OBJECT_TYPE == Internals::kJSApiObjectType);
STATIC_ASSERT(JS_SPECIAL_API_OBJECT_TYPE == Internals::kJSSpecialApiObjectType);
STATIC_ASSERT(FIRST_NONSTRING_TYPE == Internals::kFirstNonstringType);
STATIC_ASSERT(ODDBALL_TYPE == Internals::kOddballType);
STATIC_ASSERT(FOREIGN_TYPE == Internals::kForeignType);

V8_EXPORT_PRIVATE std::ostream& operator<<(std::ostream& os,
                                           InstanceType instance_type);

// Result of an abstract relational comparison of x and y, implemented according
// to ES6 section 7.2.11 Abstract Relational Comparison.
enum class ComparisonResult {
  kLessThan,     // x < y
  kEqual,        // x = y
  kGreaterThan,  // x > y
  kUndefined     // at least one of x or y was undefined or NaN
};

// (Returns false whenever {result} is kUndefined.)
bool ComparisonResultToBool(Operation op, ComparisonResult result);

enum class OnNonExistent { kThrowReferenceError, kReturnUndefined };

class AbstractCode;
class AccessorPair;
class AccessCheckInfo;
class AllocationSite;
class ByteArray;
class Cell;
class ConsString;
class DependentCode;
class ElementsAccessor;
class EnumCache;
class FixedArrayBase;
class PropertyArray;
class FunctionLiteral;
class FunctionTemplateInfo;
class JSGlobalObject;
#ifdef V8_INTL_SUPPORT
class JSLocale;
class JSRelativeTimeFormat;
#endif  // V8_INTL_SUPPORT
class JSPromise;
class KeyAccumulator;
class LayoutDescriptor;
class LookupIterator;
class FieldType;
class Module;
class ModuleInfoEntry;
class ObjectHashTable;
class ObjectTemplateInfo;
class ObjectVisitor;
class PreParsedScopeData;
class PropertyCell;
class PropertyDescriptor;
class RootVisitor;
class SafepointEntry;
class SharedFunctionInfo;
class StringStream;
class FeedbackCell;
class FeedbackMetadata;
class FeedbackVector;
class UncompiledData;
class WeakCell;
class TemplateInfo;
class TransitionArray;
class TemplateList;
template <typename T>
class ZoneForwardList;

// A template-ized version of the IsXXX functions.
template <class C> inline bool Is(Object* obj);

#ifdef OBJECT_PRINT
#define DECL_PRINTER(Name) void Name##Print(std::ostream& os);  // NOLINT
#else
#define DECL_PRINTER(Name)
#endif

#define OBJECT_TYPE_LIST(V) \
  V(Smi)                    \
  V(LayoutDescriptor)       \
  V(HeapObject)             \
  V(Primitive)              \
  V(Number)                 \
  V(Numeric)

#define HEAP_OBJECT_ORDINARY_TYPE_LIST_BASE(V) \
  V(AbstractCode)                              \
  V(AccessCheckNeeded)                         \
  V(AllocationSite)                            \
  V(ArrayList)                                 \
  V(BigInt)                                    \
  V(BigIntWrapper)                             \
  V(ObjectBoilerplateDescription)              \
  V(Boolean)                                   \
  V(BooleanWrapper)                            \
  V(BreakPoint)                                \
  V(BreakPointInfo)                            \
  V(ByteArray)                                 \
  V(BytecodeArray)                             \
  V(CallHandlerInfo)                           \
  V(Callable)                                  \
  V(Cell)                                      \
  V(ClassBoilerplate)                          \
  V(Code)                                      \
  V(CodeDataContainer)                         \
  V(CompilationCacheTable)                     \
  V(ConsString)                                \
  V(Constructor)                               \
  V(Context)                                   \
  V(CoverageInfo)                              \
  V(DataHandler)                               \
  V(DeoptimizationData)                        \
  V(DependentCode)                             \
  V(DescriptorArray)                           \
  V(EphemeronHashTable)                        \
  V(EnumCache)                                 \
  V(ExternalOneByteString)                     \
  V(ExternalString)                            \
  V(ExternalTwoByteString)                     \
  V(FeedbackCell)                              \
  V(FeedbackMetadata)                          \
  V(FeedbackVector)                            \
  V(Filler)                                    \
  V(FixedArray)                                \
  V(FixedArrayBase)                            \
  V(FixedArrayExact)                           \
  V(FixedArrayOfWeakCells)                     \
  V(FixedBigInt64Array)                        \
  V(FixedBigUint64Array)                       \
  V(FixedDoubleArray)                          \
  V(FixedFloat32Array)                         \
  V(FixedFloat64Array)                         \
  V(FixedInt16Array)                           \
  V(FixedInt32Array)                           \
  V(FixedInt8Array)                            \
  V(FixedTypedArrayBase)                       \
  V(FixedUint16Array)                          \
  V(FixedUint32Array)                          \
  V(FixedUint8Array)                           \
  V(FixedUint8ClampedArray)                    \
  V(Foreign)                                   \
  V(FrameArray)                                \
  V(FreeSpace)                                 \
  V(Function)                                  \
  V(GlobalDictionary)                          \
  V(HandlerTable)                              \
  V(HeapNumber)                                \
  V(InternalizedString)                        \
  V(JSArgumentsObject)                         \
  V(JSArray)                                   \
  V(JSArrayBuffer)                             \
  V(JSArrayBufferView)                         \
  V(JSArrayIterator)                           \
  V(JSAsyncFromSyncIterator)                   \
  V(JSAsyncGeneratorObject)                    \
  V(JSBoundFunction)                           \
  V(JSCollection)                              \
  V(JSContextExtensionObject)                  \
  V(JSDataView)                                \
  V(JSDate)                                    \
  V(JSError)                                   \
  V(JSFunction)                                \
  V(JSGeneratorObject)                         \
  V(JSGlobalObject)                            \
  V(JSGlobalProxy)                             \
  V(JSMap)                                     \
  V(JSMapIterator)                             \
  V(JSMessageObject)                           \
  V(JSModuleNamespace)                         \
  V(JSObject)                                  \
  V(JSPromise)                                 \
  V(JSProxy)                                   \
  V(JSReceiver)                                \
  V(JSRegExp)                                  \
  V(JSRegExpStringIterator)                    \
  V(JSSet)                                     \
  V(JSSetIterator)                             \
  V(JSSloppyArgumentsObject)                   \
  V(JSStringIterator)                          \
  V(JSTypedArray)                              \
  V(JSValue)                                   \
  V(JSWeakCollection)                          \
  V(JSWeakMap)                                 \
  V(JSWeakSet)                                 \
  V(LoadHandler)                               \
  V(Map)                                       \
  V(MapCache)                                  \
  V(Microtask)                                 \
  V(ModuleInfo)                                \
  V(MutableHeapNumber)                         \
  V(Name)                                      \
  V(NameDictionary)                            \
  V(NativeContext)                             \
  V(NormalizedMapCache)                        \
  V(NumberDictionary)                          \
  V(NumberWrapper)                             \
  V(ObjectHashSet)                             \
  V(ObjectHashTable)                           \
  V(Oddball)                                   \
  V(OrderedHashMap)                            \
  V(OrderedHashSet)                            \
  V(PreParsedScopeData)                        \
  V(PromiseReactionJobTask)                    \
  V(PropertyArray)                             \
  V(PropertyCell)                              \
  V(PropertyDescriptorObject)                  \
  V(RegExpMatchInfo)                           \
  V(ScopeInfo)                                 \
  V(ScriptContextTable)                        \
  V(ScriptWrapper)                             \
  V(SeqOneByteString)                          \
  V(SeqString)                                 \
  V(SeqTwoByteString)                          \
  V(SharedFunctionInfo)                        \
  V(SimpleNumberDictionary)                    \
  V(SlicedString)                              \
  V(SloppyArgumentsElements)                   \
  V(SmallOrderedHashMap)                       \
  V(SmallOrderedHashSet)                       \
  V(SourcePositionTableWithFrameCache)         \
  V(StoreHandler)                              \
  V(String)                                    \
  V(StringSet)                                 \
  V(StringTable)                               \
  V(StringWrapper)                             \
  V(Struct)                                    \
  V(Symbol)                                    \
  V(SymbolWrapper)                             \
  V(TemplateInfo)                              \
  V(TemplateList)                              \
  V(TemplateObjectDescription)                 \
  V(ThinString)                                \
  V(TransitionArray)                           \
  V(UncompiledData)                            \
  V(UncompiledDataWithPreParsedScope)          \
  V(UncompiledDataWithoutPreParsedScope)       \
  V(Undetectable)                              \
  V(UniqueName)                                \
  V(WasmGlobalObject)                          \
  V(WasmInstanceObject)                        \
  V(WasmMemoryObject)                          \
  V(WasmModuleObject)                          \
  V(WasmTableObject)                           \
  V(WeakCell)                                  \
  V(WeakFixedArray)                            \
  V(WeakArrayList)

#ifdef V8_INTL_SUPPORT
#define HEAP_OBJECT_ORDINARY_TYPE_LIST(V) \
  HEAP_OBJECT_ORDINARY_TYPE_LIST_BASE(V)  \
  V(JSLocale)                             \
  V(JSRelativeTimeFormat)
#else
#define HEAP_OBJECT_ORDINARY_TYPE_LIST(V) HEAP_OBJECT_ORDINARY_TYPE_LIST_BASE(V)
#endif  // V8_INTL_SUPPORT

#define HEAP_OBJECT_TEMPLATE_TYPE_LIST(V) \
  V(Dictionary)                           \
  V(HashTable)

#define HEAP_OBJECT_TYPE_LIST(V)    \
  HEAP_OBJECT_ORDINARY_TYPE_LIST(V) \
  HEAP_OBJECT_TEMPLATE_TYPE_LIST(V)

#define ODDBALL_LIST(V)                 \
  V(Undefined, undefined_value)         \
  V(Null, null_value)                   \
  V(TheHole, the_hole_value)            \
  V(Exception, exception)               \
  V(Uninitialized, uninitialized_value) \
  V(True, true_value)                   \
  V(False, false_value)                 \
  V(ArgumentsMarker, arguments_marker)  \
  V(OptimizedOut, optimized_out)        \
  V(StaleRegister, stale_register)

// The element types selection for CreateListFromArrayLike.
enum class ElementTypes { kAll, kStringAndSymbol };

// Object is the abstract superclass for all classes in the
// object hierarchy.
// Object does not use any virtual functions to avoid the
// allocation of the C++ vtable.
// Since both Smi and HeapObject are subclasses of Object no
// data members can be present in Object.
class Object {
 public:
  // Type testing.
  bool IsObject() const { return true; }

#define IS_TYPE_FUNCTION_DECL(Type) V8_INLINE bool Is##Type() const;
  OBJECT_TYPE_LIST(IS_TYPE_FUNCTION_DECL)
  HEAP_OBJECT_TYPE_LIST(IS_TYPE_FUNCTION_DECL)
#undef IS_TYPE_FUNCTION_DECL

  V8_INLINE bool IsExternal(Isolate* isolate) const;

// Oddball checks are faster when they are raw pointer comparisons, so the
// isolate/read-only roots overloads should be preferred where possible.
#define IS_TYPE_FUNCTION_DECL(Type, Value)            \
  V8_INLINE bool Is##Type(Isolate* isolate) const;    \
  V8_INLINE bool Is##Type(ReadOnlyRoots roots) const; \
  V8_INLINE bool Is##Type() const;
  ODDBALL_LIST(IS_TYPE_FUNCTION_DECL)
#undef IS_TYPE_FUNCTION_DECL

  V8_INLINE bool IsNullOrUndefined(Isolate* isolate) const;
  V8_INLINE bool IsNullOrUndefined(ReadOnlyRoots roots) const;
  V8_INLINE bool IsNullOrUndefined() const;

  // A non-keyed store is of the form a.x = foo or a["x"] = foo whereas
  // a keyed store is of the form a[expression] = foo.
  enum StoreFromKeyed {
    MAY_BE_STORE_FROM_KEYED,
    CERTAINLY_NOT_STORE_FROM_KEYED
  };

  enum class Conversion { kToNumber, kToNumeric };

#define RETURN_FAILURE(isolate, should_throw, call) \
  do {                                              \
    if ((should_throw) == kDontThrow) {             \
      return Just(false);                           \
    } else {                                        \
      isolate->Throw(*isolate->factory()->call);    \
      return Nothing<bool>();                       \
    }                                               \
  } while (false)

#define MAYBE_RETURN(call, value)         \
  do {                                    \
    if ((call).IsNothing()) return value; \
  } while (false)

#define MAYBE_RETURN_NULL(call) MAYBE_RETURN(call, MaybeHandle<Object>())

#define DECL_STRUCT_PREDICATE(NAME, Name, name) V8_INLINE bool Is##Name() const;
  STRUCT_LIST(DECL_STRUCT_PREDICATE)
#undef DECL_STRUCT_PREDICATE

  // ES6, #sec-isarray.  NOT to be confused with %_IsArray.
  V8_INLINE
  V8_WARN_UNUSED_RESULT static Maybe<bool> IsArray(Handle<Object> object);

  V8_INLINE bool IsSmallOrderedHashTable() const;

  // Extract the number.
  inline double Number() const;
  V8_INLINE bool IsNaN() const;
  V8_INLINE bool IsMinusZero() const;
  V8_EXPORT_PRIVATE bool ToInt32(int32_t* value);
  inline bool ToUint32(uint32_t* value) const;

  inline Representation OptimalRepresentation();

  inline ElementsKind OptimalElementsKind();

  inline bool FitsRepresentation(Representation representation);

  // Checks whether two valid primitive encodings of a property name resolve to
  // the same logical property. E.g., the smi 1, the string "1" and the double
  // 1 all refer to the same property, so this helper will return true.
  inline bool KeyEquals(Object* other);

  inline bool FilterKey(PropertyFilter filter);

  Handle<FieldType> OptimalType(Isolate* isolate,
                                Representation representation);

  inline static Handle<Object> NewStorageFor(Isolate* isolate,
                                             Handle<Object> object,
                                             Representation representation);

  inline static Handle<Object> WrapForRead(Isolate* isolate,
                                           Handle<Object> object,
                                           Representation representation);

  // Returns true if the object is of the correct type to be used as a
  // implementation of a JSObject's elements.
  inline bool HasValidElements();

  // ECMA-262 9.2.
  bool BooleanValue(Isolate* isolate);

  // ES6 section 7.2.11 Abstract Relational Comparison
  V8_WARN_UNUSED_RESULT static Maybe<ComparisonResult> Compare(
      Isolate* isolate, Handle<Object> x, Handle<Object> y);

  // ES6 section 7.2.12 Abstract Equality Comparison
  V8_WARN_UNUSED_RESULT static Maybe<bool> Equals(Isolate* isolate,
                                                  Handle<Object> x,
                                                  Handle<Object> y);

  // ES6 section 7.2.13 Strict Equality Comparison
  bool StrictEquals(Object* that);

  // ES6 section 7.1.13 ToObject
  // Convert to a JSObject if needed.
  // native_context is used when creating wrapper object.
  //
  // Passing a non-null method_name allows us to give a more informative
  // error message for those cases where ToObject is being called on
  // the receiver of a built-in method.
  V8_WARN_UNUSED_RESULT static inline MaybeHandle<JSReceiver> ToObject(
      Isolate* isolate, Handle<Object> object,
      const char* method_name = nullptr);
  V8_WARN_UNUSED_RESULT static MaybeHandle<JSReceiver> ToObject(
      Isolate* isolate, Handle<Object> object, Handle<Context> native_context,
      const char* method_name = nullptr);

  // ES6 section 9.2.1.2, OrdinaryCallBindThis for sloppy callee.
  V8_WARN_UNUSED_RESULT static MaybeHandle<JSReceiver> ConvertReceiver(
      Isolate* isolate, Handle<Object> object);

  // ES6 section 7.1.14 ToPropertyKey
  V8_WARN_UNUSED_RESULT static inline MaybeHandle<Name> ToName(
      Isolate* isolate, Handle<Object> input);

  // ES6 section 7.1.1 ToPrimitive
  V8_WARN_UNUSED_RESULT static inline MaybeHandle<Object> ToPrimitive(
      Handle<Object> input, ToPrimitiveHint hint = ToPrimitiveHint::kDefault);

  // ES6 section 7.1.3 ToNumber
  V8_WARN_UNUSED_RESULT static inline MaybeHandle<Object> ToNumber(
      Isolate* isolate, Handle<Object> input);

  V8_WARN_UNUSED_RESULT static inline MaybeHandle<Object> ToNumeric(
      Isolate* isolate, Handle<Object> input);

  // ES6 section 7.1.4 ToInteger
  V8_WARN_UNUSED_RESULT static inline MaybeHandle<Object> ToInteger(
      Isolate* isolate, Handle<Object> input);

  // ES6 section 7.1.5 ToInt32
  V8_WARN_UNUSED_RESULT static inline MaybeHandle<Object> ToInt32(
      Isolate* isolate, Handle<Object> input);

  // ES6 section 7.1.6 ToUint32
  V8_WARN_UNUSED_RESULT inline static MaybeHandle<Object> ToUint32(
      Isolate* isolate, Handle<Object> input);

  // ES6 section 7.1.12 ToString
  V8_WARN_UNUSED_RESULT static inline MaybeHandle<String> ToString(
      Isolate* isolate, Handle<Object> input);

  static Handle<String> NoSideEffectsToString(Isolate* isolate,
                                              Handle<Object> input);

  // ES6 section 7.1.14 ToPropertyKey
  V8_WARN_UNUSED_RESULT static inline MaybeHandle<Object> ToPropertyKey(
      Isolate* isolate, Handle<Object> value);

  // ES6 section 7.1.15 ToLength
  V8_WARN_UNUSED_RESULT static inline MaybeHandle<Object> ToLength(
      Isolate* isolate, Handle<Object> input);

  // ES6 section 7.1.17 ToIndex
  V8_WARN_UNUSED_RESULT static inline MaybeHandle<Object> ToIndex(
      Isolate* isolate, Handle<Object> input,
      MessageTemplate::Template error_index);

  // ES6 section 7.3.9 GetMethod
  V8_WARN_UNUSED_RESULT static MaybeHandle<Object> GetMethod(
      Handle<JSReceiver> receiver, Handle<Name> name);

  // ES6 section 7.3.17 CreateListFromArrayLike
  V8_WARN_UNUSED_RESULT static MaybeHandle<FixedArray> CreateListFromArrayLike(
      Isolate* isolate, Handle<Object> object, ElementTypes element_types);

  // Get length property and apply ToLength.
  V8_WARN_UNUSED_RESULT static MaybeHandle<Object> GetLengthFromArrayLike(
      Isolate* isolate, Handle<Object> object);

  // ES6 section 12.5.6 The typeof Operator
  static Handle<String> TypeOf(Isolate* isolate, Handle<Object> object);

  // ES6 section 12.7 Additive Operators
  V8_WARN_UNUSED_RESULT static MaybeHandle<Object> Add(Isolate* isolate,
                                                       Handle<Object> lhs,
                                                       Handle<Object> rhs);

  // ES6 section 12.9 Relational Operators
  V8_WARN_UNUSED_RESULT static inline Maybe<bool> GreaterThan(Isolate* isolate,
                                                              Handle<Object> x,
                                                              Handle<Object> y);
  V8_WARN_UNUSED_RESULT static inline Maybe<bool> GreaterThanOrEqual(
      Isolate* isolate, Handle<Object> x, Handle<Object> y);
  V8_WARN_UNUSED_RESULT static inline Maybe<bool> LessThan(Isolate* isolate,
                                                           Handle<Object> x,
                                                           Handle<Object> y);
  V8_WARN_UNUSED_RESULT static inline Maybe<bool> LessThanOrEqual(
      Isolate* isolate, Handle<Object> x, Handle<Object> y);

  // ES6 section 7.3.19 OrdinaryHasInstance (C, O).
  V8_WARN_UNUSED_RESULT static MaybeHandle<Object> OrdinaryHasInstance(
      Isolate* isolate, Handle<Object> callable, Handle<Object> object);

  // ES6 section 12.10.4 Runtime Semantics: InstanceofOperator(O, C)
  V8_WARN_UNUSED_RESULT static MaybeHandle<Object> InstanceOf(
      Isolate* isolate, Handle<Object> object, Handle<Object> callable);

  V8_EXPORT_PRIVATE V8_WARN_UNUSED_RESULT static MaybeHandle<Object>
  GetProperty(LookupIterator* it,
              OnNonExistent on_non_existent = OnNonExistent::kReturnUndefined);

  // ES6 [[Set]] (when passed kDontThrow)
  // Invariants for this and related functions (unless stated otherwise):
  // 1) When the result is Nothing, an exception is pending.
  // 2) When passed kThrowOnError, the result is never Just(false).
  // In some cases, an exception is thrown regardless of the ShouldThrow
  // argument.  These cases are either in accordance with the spec or not
  // covered by it (eg., concerning API callbacks).
  V8_WARN_UNUSED_RESULT static Maybe<bool> SetProperty(
      LookupIterator* it, Handle<Object> value, LanguageMode language_mode,
      StoreFromKeyed store_mode);
  V8_WARN_UNUSED_RESULT static MaybeHandle<Object> SetProperty(
      Isolate* isolate, Handle<Object> object, Handle<Name> name,
      Handle<Object> value, LanguageMode language_mode,
      StoreFromKeyed store_mode = MAY_BE_STORE_FROM_KEYED);
  V8_WARN_UNUSED_RESULT static inline MaybeHandle<Object> SetPropertyOrElement(
      Isolate* isolate, Handle<Object> object, Handle<Name> name,
      Handle<Object> value, LanguageMode language_mode,
      StoreFromKeyed store_mode = MAY_BE_STORE_FROM_KEYED);

  V8_WARN_UNUSED_RESULT static Maybe<bool> SetSuperProperty(
      LookupIterator* it, Handle<Object> value, LanguageMode language_mode,
      StoreFromKeyed store_mode);

  V8_WARN_UNUSED_RESULT static Maybe<bool> CannotCreateProperty(
      Isolate* isolate, Handle<Object> receiver, Handle<Object> name,
      Handle<Object> value, ShouldThrow should_throw);
  V8_WARN_UNUSED_RESULT static Maybe<bool> WriteToReadOnlyProperty(
      LookupIterator* it, Handle<Object> value, ShouldThrow should_throw);
  V8_WARN_UNUSED_RESULT static Maybe<bool> WriteToReadOnlyProperty(
      Isolate* isolate, Handle<Object> receiver, Handle<Object> name,
      Handle<Object> value, ShouldThrow should_throw);
  V8_WARN_UNUSED_RESULT static Maybe<bool> RedefineIncompatibleProperty(
      Isolate* isolate, Handle<Object> name, Handle<Object> value,
      ShouldThrow should_throw);
  V8_WARN_UNUSED_RESULT static Maybe<bool> SetDataProperty(
      LookupIterator* it, Handle<Object> value);
  V8_WARN_UNUSED_RESULT static Maybe<bool> AddDataProperty(
      LookupIterator* it, Handle<Object> value, PropertyAttributes attributes,
      ShouldThrow should_throw, StoreFromKeyed store_mode);
  V8_WARN_UNUSED_RESULT static inline MaybeHandle<Object> GetPropertyOrElement(
      Isolate* isolate, Handle<Object> object, Handle<Name> name);
  V8_WARN_UNUSED_RESULT static inline MaybeHandle<Object> GetPropertyOrElement(
      Handle<Object> receiver, Handle<Name> name, Handle<JSReceiver> holder);
  V8_WARN_UNUSED_RESULT static inline MaybeHandle<Object> GetProperty(
      Isolate* isolate, Handle<Object> object, Handle<Name> name);

  V8_WARN_UNUSED_RESULT static MaybeHandle<Object> GetPropertyWithAccessor(
      LookupIterator* it);
  V8_WARN_UNUSED_RESULT static Maybe<bool> SetPropertyWithAccessor(
      LookupIterator* it, Handle<Object> value, ShouldThrow should_throw);

  V8_WARN_UNUSED_RESULT static MaybeHandle<Object> GetPropertyWithDefinedGetter(
      Handle<Object> receiver, Handle<JSReceiver> getter);
  V8_WARN_UNUSED_RESULT static Maybe<bool> SetPropertyWithDefinedSetter(
      Handle<Object> receiver, Handle<JSReceiver> setter, Handle<Object> value,
      ShouldThrow should_throw);

  V8_WARN_UNUSED_RESULT static inline MaybeHandle<Object> GetElement(
      Isolate* isolate, Handle<Object> object, uint32_t index);

  V8_WARN_UNUSED_RESULT static inline MaybeHandle<Object> SetElement(
      Isolate* isolate, Handle<Object> object, uint32_t index,
      Handle<Object> value, LanguageMode language_mode);

  // Returns the permanent hash code associated with this object. May return
  // undefined if not yet created.
  inline Object* GetHash();

  // Returns the permanent hash code associated with this object depending on
  // the actual object type. May create and store a hash code if needed and none
  // exists.
  Smi* GetOrCreateHash(Isolate* isolate);
  static Smi* GetOrCreateHash(Isolate* isolate, Object* key);

  // Checks whether this object has the same value as the given one.  This
  // function is implemented according to ES5, section 9.12 and can be used
  // to implement the Harmony "egal" function.
  V8_EXPORT_PRIVATE bool SameValue(Object* other);

  // Checks whether this object has the same value as the given one.
  // +0 and -0 are treated equal. Everything else is the same as SameValue.
  // This function is implemented according to ES6, section 7.2.4 and is used
  // by ES6 Map and Set.
  bool SameValueZero(Object* other);

  // ES6 section 9.4.2.3 ArraySpeciesCreate (part of it)
  V8_WARN_UNUSED_RESULT static MaybeHandle<Object> ArraySpeciesConstructor(
      Isolate* isolate, Handle<Object> original_array);

  // ES6 section 7.3.20 SpeciesConstructor ( O, defaultConstructor )
  V8_WARN_UNUSED_RESULT static MaybeHandle<Object> SpeciesConstructor(
      Isolate* isolate, Handle<JSReceiver> recv,
      Handle<JSFunction> default_ctor);

  // Tries to convert an object to an array length. Returns true and sets the
  // output parameter if it succeeds.
  inline bool ToArrayLength(uint32_t* index) const;

  // Tries to convert an object to an array index. Returns true and sets the
  // output parameter if it succeeds. Equivalent to ToArrayLength, but does not
  // allow kMaxUInt32.
  inline bool ToArrayIndex(uint32_t* index) const;

  // Returns true if the result of iterating over the object is the same
  // (including observable effects) as simply accessing the properties between 0
  // and length.
  bool IterationHasObservableEffects();

  DECL_VERIFIER(Object)
#ifdef VERIFY_HEAP
  // Verify a pointer is a valid object pointer.
  static void VerifyPointer(Isolate* isolate, Object* p);
#endif

  inline void VerifyApiCallResultType();

  // Prints this object without details.
  void ShortPrint(FILE* out = stdout);

  // Prints this object without details to a message accumulator.
  void ShortPrint(StringStream* accumulator);

  void ShortPrint(std::ostream& os);  // NOLINT

  DECL_CAST(Object)

  // Layout description.
  static const int kHeaderSize = 0;  // Object does not take up any space.

#ifdef OBJECT_PRINT
  // For our gdb macros, we should perhaps change these in the future.
  void Print();

  // Prints this object with details.
  void Print(std::ostream& os);  // NOLINT
#else
  void Print() { ShortPrint(); }
  void Print(std::ostream& os) { ShortPrint(os); }  // NOLINT
#endif

 private:
  friend class LookupIterator;
  friend class StringStream;

  // Return the map of the root of object's prototype chain.
  Map* GetPrototypeChainRootMap(Isolate* isolate) const;

  // Returns a non-SMI for JSReceivers, but returns the hash code for
  // simple objects.  This avoids a double lookup in the cases where
  // we know we will add the hash to the JSReceiver if it does not
  // already exist.
  //
  // Despite its size, this needs to be inlined for performance
  // reasons.
  static inline Object* GetSimpleHash(Object* object);

  // Helper for SetProperty and SetSuperProperty.
  // Return value is only meaningful if [found] is set to true on return.
  V8_WARN_UNUSED_RESULT static Maybe<bool> SetPropertyInternal(
      LookupIterator* it, Handle<Object> value, LanguageMode language_mode,
      StoreFromKeyed store_mode, bool* found);

  V8_WARN_UNUSED_RESULT static MaybeHandle<Name> ConvertToName(
      Isolate* isolate, Handle<Object> input);
  V8_WARN_UNUSED_RESULT static MaybeHandle<Object> ConvertToPropertyKey(
      Isolate* isolate, Handle<Object> value);
  V8_WARN_UNUSED_RESULT static MaybeHandle<String> ConvertToString(
      Isolate* isolate, Handle<Object> input);
  V8_WARN_UNUSED_RESULT static MaybeHandle<Object> ConvertToNumberOrNumeric(
      Isolate* isolate, Handle<Object> input, Conversion mode);
  V8_WARN_UNUSED_RESULT static MaybeHandle<Object> ConvertToInteger(
      Isolate* isolate, Handle<Object> input);
  V8_WARN_UNUSED_RESULT static MaybeHandle<Object> ConvertToInt32(
      Isolate* isolate, Handle<Object> input);
  V8_WARN_UNUSED_RESULT static MaybeHandle<Object> ConvertToUint32(
      Isolate* isolate, Handle<Object> input);
  V8_WARN_UNUSED_RESULT static MaybeHandle<Object> ConvertToLength(
      Isolate* isolate, Handle<Object> input);
  V8_WARN_UNUSED_RESULT static MaybeHandle<Object> ConvertToIndex(
      Isolate* isolate, Handle<Object> input,
      MessageTemplate::Template error_index);

  DISALLOW_IMPLICIT_CONSTRUCTORS(Object);
};


// In objects.h to be usable without objects-inl.h inclusion.
bool Object::IsSmi() const { return HAS_SMI_TAG(this); }
bool Object::IsHeapObject() const {
  DCHECK_EQ(!IsSmi(), Internals::HasHeapObjectTag(this));
  return !IsSmi();
}

struct Brief {
  explicit Brief(const Object* const v) : value(v) {}
  const Object* value;
};

struct MaybeObjectBrief {
  explicit MaybeObjectBrief(const MaybeObject* const v) : value(v) {}
  const MaybeObject* value;
};

V8_EXPORT_PRIVATE std::ostream& operator<<(std::ostream& os, const Brief& v);
V8_EXPORT_PRIVATE std::ostream& operator<<(std::ostream& os,
                                           const MaybeObjectBrief& v);

// Smi represents integer Numbers that can be stored in 31 bits.
// Smis are immediate which means they are NOT allocated in the heap.
// The this pointer has the following format: [31 bit signed int] 0
// For long smis it has the following format:
//     [32 bit signed int] [31 bits zero padding] 0
// Smi stands for small integer.
class Smi: public Object {
 public:
  // Returns the integer value.
  inline int value() const { return Internals::SmiValue(this); }
  inline Smi* ToUint32Smi() {
    if (value() <= 0) return Smi::kZero;
    return Smi::FromInt(static_cast<uint32_t>(value()));
  }

  // Convert a Smi object to an int.
  static inline int ToInt(const Object* object);

  // Convert a value to a Smi object.
  static inline Smi* FromInt(int value) {
    DCHECK(Smi::IsValid(value));
    return reinterpret_cast<Smi*>(Internals::IntToSmi(value));
  }

  static inline Smi* FromIntptr(intptr_t value) {
    DCHECK(Smi::IsValid(value));
    int smi_shift_bits = kSmiTagSize + kSmiShiftSize;
    return reinterpret_cast<Smi*>((value << smi_shift_bits) | kSmiTag);
  }

  template <typename E,
            typename = typename std::enable_if<std::is_enum<E>::value>::type>
  static inline Smi* FromEnum(E value) {
    STATIC_ASSERT(sizeof(E) <= sizeof(int));
    return FromInt(static_cast<int>(value));
  }

  // Returns whether value can be represented in a Smi.
  static inline bool IsValid(intptr_t value) {
    bool result = Internals::IsValidSmi(value);
    DCHECK_EQ(result, value >= kMinValue && value <= kMaxValue);
    return result;
  }

  DECL_CAST(Smi)

  // Dispatched behavior.
  V8_EXPORT_PRIVATE void SmiPrint(std::ostream& os) const;  // NOLINT
  DECL_VERIFIER(Smi)

  static constexpr Smi* const kZero = nullptr;
  static const int kMinValue = kSmiMinValue;
  static const int kMaxValue = kSmiMaxValue;

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(Smi);
};


// Heap objects typically have a map pointer in their first word.  However,
// during GC other data (e.g. mark bits, forwarding addresses) is sometimes
// encoded in the first word.  The class MapWord is an abstraction of the
// value in a heap object's first word.
class MapWord BASE_EMBEDDED {
 public:
  // Normal state: the map word contains a map pointer.

  // Create a map word from a map pointer.
  static inline MapWord FromMap(const Map* map);

  // View this map word as a map pointer.
  inline Map* ToMap() const;

  // Scavenge collection: the map word of live objects in the from space
  // contains a forwarding address (a heap object pointer in the to space).

  // True if this map word is a forwarding address for a scavenge
  // collection.  Only valid during a scavenge collection (specifically,
  // when all map words are heap object pointers, i.e. not during a full GC).
  inline bool IsForwardingAddress() const;

  // Create a map word from a forwarding address.
  static inline MapWord FromForwardingAddress(HeapObject* object);

  // View this map word as a forwarding address.
  inline HeapObject* ToForwardingAddress();

  static inline MapWord FromRawValue(uintptr_t value) {
    return MapWord(value);
  }

  inline uintptr_t ToRawValue() {
    return value_;
  }

 private:
  // HeapObject calls the private constructor and directly reads the value.
  friend class HeapObject;

  explicit MapWord(uintptr_t value) : value_(value) {}

  uintptr_t value_;
};


// HeapObject is the superclass for all classes describing heap allocated
// objects.
class HeapObject: public Object {
 public:
  // [map]: Contains a map which contains the object's reflective
  // information.
  inline Map* map() const;
  inline void set_map(Map* value);

  inline HeapObject** map_slot();

  // The no-write-barrier version.  This is OK if the object is white and in
  // new space, or if the value is an immortal immutable object, like the maps
  // of primitive (non-JS) objects like strings, heap numbers etc.
  inline void set_map_no_write_barrier(Map* value);

  // Get the map using acquire load.
  inline Map* synchronized_map() const;
  inline MapWord synchronized_map_word() const;

  // Set the map using release store
  inline void synchronized_set_map(Map* value);
  inline void synchronized_set_map_word(MapWord map_word);

  // Initialize the map immediately after the object is allocated.
  // Do not use this outside Heap.
  inline void set_map_after_allocation(
      Map* value, WriteBarrierMode mode = UPDATE_WRITE_BARRIER);

  // During garbage collection, the map word of a heap object does not
  // necessarily contain a map pointer.
  inline MapWord map_word() const;
  inline void set_map_word(MapWord map_word);

  // TODO(v8:7464): Once RO_SPACE is shared between isolates, this method can be
  // removed as ReadOnlyRoots will be accessible from a global variable. For now
  // this method exists to help remove GetIsolate/GetHeap from HeapObject, in a
  // way that doesn't require passing Isolate/Heap down huge call chains or to
  // places where it might not be safe to access it.
  inline ReadOnlyRoots GetReadOnlyRoots() const;

  // The Heap the object was allocated in. Used also to access Isolate.
#ifdef DEPRECATE_GET_ISOLATE
  [[deprecated("Pass Heap explicitly or use a NeverReadOnlySpaceObject")]]
#endif
      inline Heap*
      GetHeap() const;

// Convenience method to get current isolate.
#ifdef DEPRECATE_GET_ISOLATE
  [[deprecated("Pass Isolate explicitly or use a NeverReadOnlySpaceObject")]]
#endif
      inline Isolate*
      GetIsolate() const;

#define IS_TYPE_FUNCTION_DECL(Type) V8_INLINE bool Is##Type() const;
  HEAP_OBJECT_TYPE_LIST(IS_TYPE_FUNCTION_DECL)
#undef IS_TYPE_FUNCTION_DECL

  V8_INLINE bool IsExternal(Isolate* isolate) const;

// Oddball checks are faster when they are raw pointer comparisons, so the
// isolate/read-only roots overloads should be preferred where possible.
#define IS_TYPE_FUNCTION_DECL(Type, Value)            \
  V8_INLINE bool Is##Type(Isolate* isolate) const;    \
  V8_INLINE bool Is##Type(ReadOnlyRoots roots) const; \
  V8_INLINE bool Is##Type() const;
  ODDBALL_LIST(IS_TYPE_FUNCTION_DECL)
#undef IS_TYPE_FUNCTION_DECL

  V8_INLINE bool IsNullOrUndefined(Isolate* isolate) const;
  V8_INLINE bool IsNullOrUndefined(ReadOnlyRoots roots) const;
  V8_INLINE bool IsNullOrUndefined() const;

#define DECL_STRUCT_PREDICATE(NAME, Name, name) V8_INLINE bool Is##Name() const;
  STRUCT_LIST(DECL_STRUCT_PREDICATE)
#undef DECL_STRUCT_PREDICATE

  // Converts an address to a HeapObject pointer.
  static inline HeapObject* FromAddress(Address address) {
    DCHECK_TAG_ALIGNED(address);
    return reinterpret_cast<HeapObject*>(address + kHeapObjectTag);
  }

  // Returns the address of this HeapObject.
  inline Address address() const {
    return reinterpret_cast<Address>(this) - kHeapObjectTag;
  }

  // Iterates over pointers contained in the object (including the Map).
  // If it's not performance critical iteration use the non-templatized
  // version.
  void Iterate(ObjectVisitor* v);

  template <typename ObjectVisitor>
  inline void IterateFast(ObjectVisitor* v);

  // Iterates over all pointers contained in the object except the
  // first map pointer.  The object type is given in the first
  // parameter. This function does not access the map pointer in the
  // object, and so is safe to call while the map pointer is modified.
  // If it's not performance critical iteration use the non-templatized
  // version.
  void IterateBody(ObjectVisitor* v);
  void IterateBody(Map* map, int object_size, ObjectVisitor* v);

  template <typename ObjectVisitor>
  inline void IterateBodyFast(ObjectVisitor* v);

  template <typename ObjectVisitor>
  inline void IterateBodyFast(Map* map, int object_size, ObjectVisitor* v);

  // Returns true if the object contains a tagged value at given offset.
  // It is used for invalid slots filtering. If the offset points outside
  // of the object or to the map word, the result is UNDEFINED (!!!).
  bool IsValidSlot(Map* map, int offset);

  // Returns the heap object's size in bytes
  inline int Size() const;

  // Given a heap object's map pointer, returns the heap size in bytes
  // Useful when the map pointer field is used for other purposes.
  // GC internal.
  inline int SizeFromMap(Map* map) const;

  // Returns the field at offset in obj, as a read/write Object* reference.
  // Does no checking, and is safe to use during GC, while maps are invalid.
  // Does not invoke write barrier, so should only be assigned to
  // during marking GC.
  static inline Object** RawField(const HeapObject* obj, int offset);
  static inline MaybeObject** RawMaybeWeakField(HeapObject* obj, int offset);

  DECL_CAST(HeapObject)

  // Return the write barrier mode for this. Callers of this function
  // must be able to present a reference to an DisallowHeapAllocation
  // object as a sign that they are not going to use this function
  // from code that allocates and thus invalidates the returned write
  // barrier mode.
  inline WriteBarrierMode GetWriteBarrierMode(
      const DisallowHeapAllocation& promise);

  // Dispatched behavior.
  void HeapObjectShortPrint(std::ostream& os);  // NOLINT
#ifdef OBJECT_PRINT
  void PrintHeader(std::ostream& os, const char* id);  // NOLINT
#endif
  DECL_PRINTER(HeapObject)
  DECL_VERIFIER(HeapObject)
#ifdef VERIFY_HEAP
  inline void VerifyObjectField(Isolate* isolate, int offset);
  inline void VerifySmiField(int offset);
  inline void VerifyMaybeObjectField(Isolate* isolate, int offset);

  // Verify a pointer is a valid HeapObject pointer that points to object
  // areas in the heap.
  static void VerifyHeapPointer(Isolate* isolate, Object* p);
#endif

  static inline AllocationAlignment RequiredAlignment(Map* map);

  // Whether the object needs rehashing. That is the case if the object's
  // content depends on FLAG_hash_seed. When the object is deserialized into
  // a heap with a different hash seed, these objects need to adapt.
  inline bool NeedsRehashing() const;

  // Rehashing support is not implemented for all objects that need rehashing.
  // With objects that need rehashing but cannot be rehashed, rehashing has to
  // be disabled.
  bool CanBeRehashed() const;

  // Rehash the object based on the layout inferred from its map.
  void RehashBasedOnMap(Isolate* isolate);

  // Layout description.
  // First field in a heap object is map.
  static const int kMapOffset = Object::kHeaderSize;
  static const int kHeaderSize = kMapOffset + kPointerSize;

  STATIC_ASSERT(kMapOffset == Internals::kHeapObjectMapOffset);

  inline Address GetFieldAddress(int field_offset) const;

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(HeapObject);
};

// Mixin class for objects that can never be in RO space.
// TODO(leszeks): Add checks in the factory that we never allocate these objects
// in RO space.
class NeverReadOnlySpaceObject {
 public:
  // The Heap the object was allocated in. Used also to access Isolate.
  inline Heap* GetHeap() const;

  // Convenience method to get current isolate.
  inline Isolate* GetIsolate() const;
};

template <int start_offset, int end_offset, int size>
class FixedBodyDescriptor;


template <int start_offset>
class FlexibleBodyDescriptor;

// The HeapNumber class describes heap allocated numbers that cannot be
// represented in a Smi (small integer). MutableHeapNumber is the same, but its
// number value can change over time (it is used only as property storage).
// HeapNumberBase merely exists to avoid code duplication.
class HeapNumberBase : public HeapObject {
 public:
  // [value]: number value.
  inline double value() const;
  inline void set_value(double value);

  inline uint64_t value_as_bits() const;
  inline void set_value_as_bits(uint64_t bits);

  inline int get_exponent();
  inline int get_sign();

  // Layout description.
  static const int kValueOffset = HeapObject::kHeaderSize;
  // IEEE doubles are two 32 bit words.  The first is just mantissa, the second
  // is a mixture of sign, exponent and mantissa. The offsets of two 32 bit
  // words within double numbers are endian dependent and they are set
  // accordingly.
#if defined(V8_TARGET_LITTLE_ENDIAN)
  static const int kMantissaOffset = kValueOffset;
  static const int kExponentOffset = kValueOffset + 4;
#elif defined(V8_TARGET_BIG_ENDIAN)
  static const int kMantissaOffset = kValueOffset + 4;
  static const int kExponentOffset = kValueOffset;
#else
#error Unknown byte ordering
#endif

  static const int kSize = kValueOffset + kDoubleSize;
  static const uint32_t kSignMask = 0x80000000u;
  static const uint32_t kExponentMask = 0x7ff00000u;
  static const uint32_t kMantissaMask = 0xfffffu;
  static const int kMantissaBits = 52;
  static const int kExponentBits = 11;
  static const int kExponentBias = 1023;
  static const int kExponentShift = 20;
  static const int kInfinityOrNanExponent =
      (kExponentMask >> kExponentShift) - kExponentBias;
  static const int kMantissaBitsInTopWord = 20;
  static const int kNonMantissaBitsInTopWord = 12;

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(HeapNumberBase)
};

class HeapNumber : public HeapNumberBase {
 public:
  DECL_CAST(HeapNumber)
  V8_EXPORT_PRIVATE void HeapNumberPrint(std::ostream& os);

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(HeapNumber)
};

class MutableHeapNumber : public HeapNumberBase {
 public:
  DECL_CAST(MutableHeapNumber)
  V8_EXPORT_PRIVATE void MutableHeapNumberPrint(std::ostream& os);

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(MutableHeapNumber)
};

enum EnsureElementsMode {
  DONT_ALLOW_DOUBLE_ELEMENTS,
  ALLOW_COPIED_DOUBLE_ELEMENTS,
  ALLOW_CONVERTED_DOUBLE_ELEMENTS
};


// Indicator for one component of an AccessorPair.
enum AccessorComponent {
  ACCESSOR_GETTER,
  ACCESSOR_SETTER
};

enum class GetKeysConversion {
  kKeepNumbers = static_cast<int>(v8::KeyConversionMode::kKeepNumbers),
  kConvertToString = static_cast<int>(v8::KeyConversionMode::kConvertToString)
};

enum class KeyCollectionMode {
  kOwnOnly = static_cast<int>(v8::KeyCollectionMode::kOwnOnly),
  kIncludePrototypes =
      static_cast<int>(v8::KeyCollectionMode::kIncludePrototypes)
};

enum class AllocationSiteUpdateMode { kUpdate, kCheckOnly };

class PropertyArray : public HeapObject {
 public:
  // [length]: length of the array.
  inline int length() const;

  // Get the length using acquire loads.
  inline int synchronized_length() const;

  // This is only used on a newly allocated PropertyArray which
  // doesn't have an existing hash.
  inline void initialize_length(int length);

  inline void SetHash(int hash);
  inline int Hash() const;

  inline Object* get(int index) const;

  inline void set(int index, Object* value);
  // Setter with explicit barrier mode.
  inline void set(int index, Object* value, WriteBarrierMode mode);

  // Gives access to raw memory which stores the array's data.
  inline Object** data_start();

  // Garbage collection support.
  static constexpr int SizeFor(int length) {
    return kHeaderSize + length * kPointerSize;
  }

  DECL_CAST(PropertyArray)
  DECL_PRINTER(PropertyArray)
  DECL_VERIFIER(PropertyArray)

  // Layout description.
  static const int kLengthAndHashOffset = HeapObject::kHeaderSize;
  static const int kHeaderSize = kLengthAndHashOffset + kPointerSize;

  // Garbage collection support.
  typedef FlexibleBodyDescriptor<kHeaderSize> BodyDescriptor;
  // No weak fields.
  typedef BodyDescriptor BodyDescriptorWeak;

  static const int kLengthFieldSize = 10;
  class LengthField : public BitField<int, 0, kLengthFieldSize> {};
  static const int kMaxLength = LengthField::kMax;
  class HashField : public BitField<int, kLengthFieldSize,
                                    kSmiValueSize - kLengthFieldSize - 1> {};

  static const int kNoHashSentinel = 0;

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(PropertyArray);
};

// JSReceiver includes types on which properties can be defined, i.e.,
// JSObject and JSProxy.
class JSReceiver : public HeapObject, public NeverReadOnlySpaceObject {
 public:
  // Use the mixin methods over the HeapObject methods.
  // TODO(v8:7786) Remove once the HeapObject methods are gone.
  using NeverReadOnlySpaceObject::GetHeap;
  using NeverReadOnlySpaceObject::GetIsolate;

  // Returns true if there is no slow (ie, dictionary) backing store.
  inline bool HasFastProperties() const;

  // Returns the properties array backing store if it
  // exists. Otherwise, returns an empty_property_array when there's a
  // Smi (hash code) or an empty_fixed_array for a fast properties
  // map.
  inline PropertyArray* property_array() const;

  // Gets slow properties for non-global objects.
  inline NameDictionary* property_dictionary() const;

  // Sets the properties backing store and makes sure any existing hash is moved
  // to the new properties store. To clear out the properties store, pass in the
  // empty_fixed_array(), the hash will be maintained in this case as well.
  void SetProperties(HeapObject* properties);

  // There are five possible values for the properties offset.
  // 1) EmptyFixedArray/EmptyPropertyDictionary - This is the standard
  // placeholder.
  //
  // 2) Smi - This is the hash code of the object.
  //
  // 3) PropertyArray - This is similar to a FixedArray but stores
  // the hash code of the object in its length field. This is a fast
  // backing store.
  //
  // 4) NameDictionary - This is the dictionary-mode backing store.
  //
  // 4) GlobalDictionary - This is the backing store for the
  // GlobalObject.
  //
  // This is used only in the deoptimizer and heap. Please use the
  // above typed getters and setters to access the properties.
  DECL_ACCESSORS(raw_properties_or_hash, Object)

  inline void initialize_properties();

  // Deletes an existing named property in a normalized object.
  static void DeleteNormalizedProperty(Handle<JSReceiver> object, int entry);

  DECL_CAST(JSReceiver)

  // ES6 section 7.1.1 ToPrimitive
  V8_WARN_UNUSED_RESULT static MaybeHandle<Object> ToPrimitive(
      Handle<JSReceiver> receiver,
      ToPrimitiveHint hint = ToPrimitiveHint::kDefault);

  // ES6 section 7.1.1.1 OrdinaryToPrimitive
  V8_WARN_UNUSED_RESULT static MaybeHandle<Object> OrdinaryToPrimitive(
      Handle<JSReceiver> receiver, OrdinaryToPrimitiveHint hint);

  static MaybeHandle<Context> GetFunctionRealm(Handle<JSReceiver> receiver);

  // Get the first non-hidden prototype.
  static inline MaybeHandle<Object> GetPrototype(Isolate* isolate,
                                                 Handle<JSReceiver> receiver);

  V8_WARN_UNUSED_RESULT static Maybe<bool> HasInPrototypeChain(
      Isolate* isolate, Handle<JSReceiver> object, Handle<Object> proto);

  // Reads all enumerable own properties of source and adds them to
  // target, using either Set or CreateDataProperty depending on the
  // use_set argument. This only copies values not present in the
  // maybe_excluded_properties list.
  V8_WARN_UNUSED_RESULT static Maybe<bool> SetOrCopyDataProperties(
      Isolate* isolate, Handle<JSReceiver> target, Handle<Object> source,
      const ScopedVector<Handle<Object>>* excluded_properties = nullptr,
      bool use_set = true);

  // Implementation of [[HasProperty]], ECMA-262 5th edition, section 8.12.6.
  V8_WARN_UNUSED_RESULT static Maybe<bool> HasProperty(LookupIterator* it);
  V8_WARN_UNUSED_RESULT static inline Maybe<bool> HasProperty(
      Handle<JSReceiver> object, Handle<Name> name);
  V8_WARN_UNUSED_RESULT static inline Maybe<bool> HasElement(
      Handle<JSReceiver> object, uint32_t index);

  V8_WARN_UNUSED_RESULT static Maybe<bool> HasOwnProperty(
      Handle<JSReceiver> object, Handle<Name> name);
  V8_WARN_UNUSED_RESULT static inline Maybe<bool> HasOwnProperty(
      Handle<JSReceiver> object, uint32_t index);

  V8_WARN_UNUSED_RESULT static inline MaybeHandle<Object> GetProperty(
      Isolate* isolate, Handle<JSReceiver> receiver, const char* key);
  V8_WARN_UNUSED_RESULT static inline MaybeHandle<Object> GetProperty(
      Isolate* isolate, Handle<JSReceiver> receiver, Handle<Name> name);
  V8_WARN_UNUSED_RESULT static inline MaybeHandle<Object> GetElement(
      Isolate* isolate, Handle<JSReceiver> receiver, uint32_t index);

  // Implementation of ES6 [[Delete]]
  V8_WARN_UNUSED_RESULT static Maybe<bool> DeletePropertyOrElement(
      Handle<JSReceiver> object, Handle<Name> name,
      LanguageMode language_mode = LanguageMode::kSloppy);
  V8_WARN_UNUSED_RESULT static Maybe<bool> DeleteProperty(
      Handle<JSReceiver> object, Handle<Name> name,
      LanguageMode language_mode = LanguageMode::kSloppy);
  V8_WARN_UNUSED_RESULT static Maybe<bool> DeleteProperty(
      LookupIterator* it, LanguageMode language_mode);
  V8_WARN_UNUSED_RESULT static Maybe<bool> DeleteElement(
      Handle<JSReceiver> object, uint32_t index,
      LanguageMode language_mode = LanguageMode::kSloppy);

  V8_WARN_UNUSED_RESULT static Object* DefineProperty(
      Isolate* isolate, Handle<Object> object, Handle<Object> name,
      Handle<Object> attributes);
  V8_WARN_UNUSED_RESULT static MaybeHandle<Object> DefineProperties(
      Isolate* isolate, Handle<Object> object, Handle<Object> properties);

  // "virtual" dispatcher to the correct [[DefineOwnProperty]] implementation.
  V8_WARN_UNUSED_RESULT static Maybe<bool> DefineOwnProperty(
      Isolate* isolate, Handle<JSReceiver> object, Handle<Object> key,
      PropertyDescriptor* desc, ShouldThrow should_throw);

  // ES6 7.3.4 (when passed kDontThrow)
  V8_WARN_UNUSED_RESULT static Maybe<bool> CreateDataProperty(
      LookupIterator* it, Handle<Object> value, ShouldThrow should_throw);

  // ES6 9.1.6.1
  V8_WARN_UNUSED_RESULT static Maybe<bool> OrdinaryDefineOwnProperty(
      Isolate* isolate, Handle<JSObject> object, Handle<Object> key,
      PropertyDescriptor* desc, ShouldThrow should_throw);
  V8_WARN_UNUSED_RESULT static Maybe<bool> OrdinaryDefineOwnProperty(
      LookupIterator* it, PropertyDescriptor* desc, ShouldThrow should_throw);
  // ES6 9.1.6.2
  V8_WARN_UNUSED_RESULT static Maybe<bool> IsCompatiblePropertyDescriptor(
      Isolate* isolate, bool extensible, PropertyDescriptor* desc,
      PropertyDescriptor* current, Handle<Name> property_name,
      ShouldThrow should_throw);
  // ES6 9.1.6.3
  // |it| can be NULL in cases where the ES spec passes |undefined| as the
  // receiver. Exactly one of |it| and |property_name| must be provided.
  V8_WARN_UNUSED_RESULT static Maybe<bool> ValidateAndApplyPropertyDescriptor(
      Isolate* isolate, LookupIterator* it, bool extensible,
      PropertyDescriptor* desc, PropertyDescriptor* current,
      ShouldThrow should_throw, Handle<Name> property_name);

  V8_EXPORT_PRIVATE V8_WARN_UNUSED_RESULT static Maybe<bool>
  GetOwnPropertyDescriptor(Isolate* isolate, Handle<JSReceiver> object,
                           Handle<Object> key, PropertyDescriptor* desc);
  V8_WARN_UNUSED_RESULT static Maybe<bool> GetOwnPropertyDescriptor(
      LookupIterator* it, PropertyDescriptor* desc);

  typedef PropertyAttributes IntegrityLevel;

  // ES6 7.3.14 (when passed kDontThrow)
  // 'level' must be SEALED or FROZEN.
  V8_WARN_UNUSED_RESULT static Maybe<bool> SetIntegrityLevel(
      Handle<JSReceiver> object, IntegrityLevel lvl, ShouldThrow should_throw);

  // ES6 7.3.15
  // 'level' must be SEALED or FROZEN.
  V8_WARN_UNUSED_RESULT static Maybe<bool> TestIntegrityLevel(
      Handle<JSReceiver> object, IntegrityLevel lvl);

  // ES6 [[PreventExtensions]] (when passed kDontThrow)
  V8_WARN_UNUSED_RESULT static Maybe<bool> PreventExtensions(
      Handle<JSReceiver> object, ShouldThrow should_throw);

  V8_WARN_UNUSED_RESULT static Maybe<bool> IsExtensible(
      Handle<JSReceiver> object);

  // Returns the class name ([[Class]] property in the specification).
  V8_EXPORT_PRIVATE String* class_name();

  // Returns the constructor name (the name (possibly, inferred name) of the
  // function that was used to instantiate the object).
  static Handle<String> GetConstructorName(Handle<JSReceiver> receiver);

  Handle<Context> GetCreationContext();

  V8_WARN_UNUSED_RESULT static inline Maybe<PropertyAttributes>
  GetPropertyAttributes(Handle<JSReceiver> object, Handle<Name> name);
  V8_WARN_UNUSED_RESULT static inline Maybe<PropertyAttributes>
  GetOwnPropertyAttributes(Handle<JSReceiver> object, Handle<Name> name);
  V8_WARN_UNUSED_RESULT static inline Maybe<PropertyAttributes>
  GetOwnPropertyAttributes(Handle<JSReceiver> object, uint32_t index);

  V8_WARN_UNUSED_RESULT static inline Maybe<PropertyAttributes>
  GetElementAttributes(Handle<JSReceiver> object, uint32_t index);
  V8_WARN_UNUSED_RESULT static inline Maybe<PropertyAttributes>
  GetOwnElementAttributes(Handle<JSReceiver> object, uint32_t index);

  V8_WARN_UNUSED_RESULT static Maybe<PropertyAttributes> GetPropertyAttributes(
      LookupIterator* it);

  // Set the object's prototype (only JSReceiver and null are allowed values).
  V8_WARN_UNUSED_RESULT static Maybe<bool> SetPrototype(
      Handle<JSReceiver> object, Handle<Object> value, bool from_javascript,
      ShouldThrow should_throw);

  inline static Handle<Object> GetDataProperty(Handle<JSReceiver> object,
                                               Handle<Name> name);
  static Handle<Object> GetDataProperty(LookupIterator* it);


  // Retrieves a permanent object identity hash code. The undefined value might
  // be returned in case no hash was created yet.
  Object* GetIdentityHash(Isolate* isolate);

  // Retrieves a permanent object identity hash code. May create and store a
  // hash code if needed and none exists.
  static Smi* CreateIdentityHash(Isolate* isolate, JSReceiver* key);
  Smi* GetOrCreateIdentityHash(Isolate* isolate);

  // Stores the hash code. The hash passed in must be masked with
  // JSReceiver::kHashMask.
  void SetIdentityHash(int masked_hash);

  // ES6 [[OwnPropertyKeys]] (modulo return type)
  V8_WARN_UNUSED_RESULT static inline MaybeHandle<FixedArray> OwnPropertyKeys(
      Handle<JSReceiver> object);

  V8_WARN_UNUSED_RESULT static MaybeHandle<FixedArray> GetOwnValues(
      Handle<JSReceiver> object, PropertyFilter filter,
      bool try_fast_path = true);

  V8_WARN_UNUSED_RESULT static MaybeHandle<FixedArray> GetOwnEntries(
      Handle<JSReceiver> object, PropertyFilter filter,
      bool try_fast_path = true);

  V8_WARN_UNUSED_RESULT static Handle<FixedArray> GetOwnElementIndices(
      Isolate* isolate, Handle<JSReceiver> receiver, Handle<JSObject> object);

  static const int kHashMask = PropertyArray::HashField::kMask;

  // Layout description.
  static const int kPropertiesOrHashOffset = HeapObject::kHeaderSize;
  static const int kHeaderSize = HeapObject::kHeaderSize + kPointerSize;

  bool HasProxyInPrototype(Isolate* isolate);

  bool HasComplexElements();

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(JSReceiver);
};


// The JSObject describes real heap allocated JavaScript objects with
// properties.
// Note that the map of JSObject changes during execution to enable inline
// caching.
class JSObject: public JSReceiver {
 public:
  static bool IsUnmodifiedApiObject(Object** o);

  static V8_WARN_UNUSED_RESULT MaybeHandle<JSObject> New(
      Handle<JSFunction> constructor, Handle<JSReceiver> new_target,
      Handle<AllocationSite> site = Handle<AllocationSite>::null());

  static MaybeHandle<Context> GetFunctionRealm(Handle<JSObject> object);

  // [elements]: The elements (properties with names that are integers).
  //
  // Elements can be in two general modes: fast and slow. Each mode
  // corresponds to a set of object representations of elements that
  // have something in common.
  //
  // In the fast mode elements is a FixedArray and so each element can
  // be quickly accessed. This fact is used in the generated code. The
  // elements array can have one of three maps in this mode:
  // fixed_array_map, sloppy_arguments_elements_map or
  // fixed_cow_array_map (for copy-on-write arrays). In the latter case
  // the elements array may be shared by a few objects and so before
  // writing to any element the array must be copied. Use
  // EnsureWritableFastElements in this case.
  //
  // In the slow mode the elements is either a NumberDictionary, a
  // FixedArray parameter map for a (sloppy) arguments object.
  DECL_ACCESSORS(elements, FixedArrayBase)
  inline void initialize_elements();
  static inline void SetMapAndElements(Handle<JSObject> object,
                                       Handle<Map> map,
                                       Handle<FixedArrayBase> elements);
  inline ElementsKind GetElementsKind();
  ElementsAccessor* GetElementsAccessor();
  // Returns true if an object has elements of PACKED_SMI_ELEMENTS or
  // HOLEY_SMI_ELEMENTS ElementsKind.
  inline bool HasSmiElements();
  // Returns true if an object has elements of PACKED_ELEMENTS or
  // HOLEY_ELEMENTS ElementsKind.
  inline bool HasObjectElements();
  // Returns true if an object has elements of PACKED_SMI_ELEMENTS,
  // HOLEY_SMI_ELEMENTS, PACKED_ELEMENTS, or HOLEY_ELEMENTS.
  inline bool HasSmiOrObjectElements();
  // Returns true if an object has any of the "fast" elements kinds.
  inline bool HasFastElements();
  // Returns true if an object has any of the PACKED elements kinds.
  inline bool HasFastPackedElements();
  // Returns true if an object has elements of PACKED_DOUBLE_ELEMENTS or
  // HOLEY_DOUBLE_ELEMENTS ElementsKind.
  inline bool HasDoubleElements();
  // Returns true if an object has elements of HOLEY_SMI_ELEMENTS,
  // HOLEY_DOUBLE_ELEMENTS, or HOLEY_ELEMENTS ElementsKind.
  inline bool HasHoleyElements();
  inline bool HasSloppyArgumentsElements();
  inline bool HasStringWrapperElements();
  inline bool HasDictionaryElements();

  inline bool HasFixedTypedArrayElements();

  inline bool HasFixedUint8ClampedElements();
  inline bool HasFixedArrayElements();
  inline bool HasFixedInt8Elements();
  inline bool HasFixedUint8Elements();
  inline bool HasFixedInt16Elements();
  inline bool HasFixedUint16Elements();
  inline bool HasFixedInt32Elements();
  inline bool HasFixedUint32Elements();
  inline bool HasFixedFloat32Elements();
  inline bool HasFixedFloat64Elements();
  inline bool HasFixedBigInt64Elements();
  inline bool HasFixedBigUint64Elements();

  inline bool HasFastArgumentsElements();
  inline bool HasSlowArgumentsElements();
  inline bool HasFastStringWrapperElements();
  inline bool HasSlowStringWrapperElements();
  bool HasEnumerableElements();

  inline NumberDictionary* element_dictionary();  // Gets slow elements.

  // Requires: HasFastElements().
  static void EnsureWritableFastElements(Handle<JSObject> object);

  V8_WARN_UNUSED_RESULT static Maybe<bool> SetPropertyWithInterceptor(
      LookupIterator* it, ShouldThrow should_throw, Handle<Object> value);

  // The API currently still wants DefineOwnPropertyIgnoreAttributes to convert
  // AccessorInfo objects to data fields. We allow FORCE_FIELD as an exception
  // to the default behavior that calls the setter.
  enum AccessorInfoHandling { FORCE_FIELD, DONT_FORCE_FIELD };

  V8_WARN_UNUSED_RESULT static MaybeHandle<Object>
  DefineOwnPropertyIgnoreAttributes(
      LookupIterator* it, Handle<Object> value, PropertyAttributes attributes,
      AccessorInfoHandling handling = DONT_FORCE_FIELD);

  V8_WARN_UNUSED_RESULT static Maybe<bool> DefineOwnPropertyIgnoreAttributes(
      LookupIterator* it, Handle<Object> value, PropertyAttributes attributes,
      ShouldThrow should_throw,
      AccessorInfoHandling handling = DONT_FORCE_FIELD);

  V8_WARN_UNUSED_RESULT static MaybeHandle<Object>
  SetOwnPropertyIgnoreAttributes(Handle<JSObject> object, Handle<Name> name,
                                 Handle<Object> value,
                                 PropertyAttributes attributes);

  V8_WARN_UNUSED_RESULT static MaybeHandle<Object>
  SetOwnElementIgnoreAttributes(Handle<JSObject> object, uint32_t index,
                                Handle<Object> value,
                                PropertyAttributes attributes);

  // Equivalent to one of the above depending on whether |name| can be converted
  // to an array index.
  V8_WARN_UNUSED_RESULT static MaybeHandle<Object>
  DefinePropertyOrElementIgnoreAttributes(Handle<JSObject> object,
                                          Handle<Name> name,
                                          Handle<Object> value,
                                          PropertyAttributes attributes = NONE);

  // Adds or reconfigures a property to attributes NONE. It will fail when it
  // cannot.
  V8_WARN_UNUSED_RESULT static Maybe<bool> CreateDataProperty(
      LookupIterator* it, Handle<Object> value,
      ShouldThrow should_throw = kDontThrow);

  static void AddProperty(Isolate* isolate, Handle<JSObject> object,
                          Handle<Name> name, Handle<Object> value,
                          PropertyAttributes attributes);

  static void AddDataElement(Handle<JSObject> receiver, uint32_t index,
                             Handle<Object> value,
                             PropertyAttributes attributes);

  // Extend the receiver with a single fast property appeared first in the
  // passed map. This also extends the property backing store if necessary.
  static void AllocateStorageForMap(Handle<JSObject> object, Handle<Map> map);

  // Migrates the given object to a map whose field representations are the
  // lowest upper bound of all known representations for that field.
  static void MigrateInstance(Handle<JSObject> instance);

  // Migrates the given object only if the target map is already available,
  // or returns false if such a map is not yet available.
  static bool TryMigrateInstance(Handle<JSObject> instance);

  // Sets the property value in a normalized object given (key, value, details).
  // Handles the special representation of JS global objects.
  static void SetNormalizedProperty(Handle<JSObject> object, Handle<Name> name,
                                    Handle<Object> value,
                                    PropertyDetails details);
  static void SetDictionaryElement(Handle<JSObject> object, uint32_t index,
                                   Handle<Object> value,
                                   PropertyAttributes attributes);
  static void SetDictionaryArgumentsElement(Handle<JSObject> object,
                                            uint32_t index,
                                            Handle<Object> value,
                                            PropertyAttributes attributes);

  static void OptimizeAsPrototype(Handle<JSObject> object,
                                  bool enable_setup_mode = true);
  static void ReoptimizeIfPrototype(Handle<JSObject> object);
  static void MakePrototypesFast(Handle<Object> receiver,
                                 WhereToStart where_to_start, Isolate* isolate);
  static void LazyRegisterPrototypeUser(Handle<Map> user, Isolate* isolate);
  static void UpdatePrototypeUserRegistration(Handle<Map> old_map,
                                              Handle<Map> new_map,
                                              Isolate* isolate);
  static bool UnregisterPrototypeUser(Handle<Map> user, Isolate* isolate);
  static Map* InvalidatePrototypeChains(Map* map);
  static void InvalidatePrototypeValidityCell(JSGlobalObject* global);

  // Updates prototype chain tracking information when an object changes its
  // map from |old_map| to |new_map|.
  static void NotifyMapChange(Handle<Map> old_map, Handle<Map> new_map,
                              Isolate* isolate);

  // Utility used by many Array builtins and runtime functions
  static inline bool PrototypeHasNoElements(Isolate* isolate, JSObject* object);

  // To be passed to PrototypeUsers::Compact.
  static void PrototypeRegistryCompactionCallback(HeapObject* value,
                                                  int old_index, int new_index);

  // Retrieve interceptors.
  inline InterceptorInfo* GetNamedInterceptor();
  inline InterceptorInfo* GetIndexedInterceptor();

  // Used from JSReceiver.
  V8_WARN_UNUSED_RESULT static Maybe<PropertyAttributes>
  GetPropertyAttributesWithInterceptor(LookupIterator* it);
  V8_WARN_UNUSED_RESULT static Maybe<PropertyAttributes>
  GetPropertyAttributesWithFailedAccessCheck(LookupIterator* it);

  // Defines an AccessorPair property on the given object.
  // TODO(mstarzinger): Rename to SetAccessor().
  static MaybeHandle<Object> DefineAccessor(Handle<JSObject> object,
                                            Handle<Name> name,
                                            Handle<Object> getter,
                                            Handle<Object> setter,
                                            PropertyAttributes attributes);
  static MaybeHandle<Object> DefineAccessor(LookupIterator* it,
                                            Handle<Object> getter,
                                            Handle<Object> setter,
                                            PropertyAttributes attributes);

  // Defines an AccessorInfo property on the given object.
  V8_WARN_UNUSED_RESULT static MaybeHandle<Object> SetAccessor(
      Handle<JSObject> object, Handle<Name> name, Handle<AccessorInfo> info,
      PropertyAttributes attributes);

  // The result must be checked first for exceptions. If there's no exception,
  // the output parameter |done| indicates whether the interceptor has a result
  // or not.
  V8_WARN_UNUSED_RESULT static MaybeHandle<Object> GetPropertyWithInterceptor(
      LookupIterator* it, bool* done);

  static void ValidateElements(JSObject* object);

  // Makes sure that this object can contain HeapObject as elements.
  static inline void EnsureCanContainHeapObjectElements(Handle<JSObject> obj);

  // Makes sure that this object can contain the specified elements.
  static inline void EnsureCanContainElements(
      Handle<JSObject> object,
      Object** elements,
      uint32_t count,
      EnsureElementsMode mode);
  static inline void EnsureCanContainElements(
      Handle<JSObject> object,
      Handle<FixedArrayBase> elements,
      uint32_t length,
      EnsureElementsMode mode);
  static void EnsureCanContainElements(
      Handle<JSObject> object,
      Arguments* arguments,
      uint32_t first_arg,
      uint32_t arg_count,
      EnsureElementsMode mode);

  // Would we convert a fast elements array to dictionary mode given
  // an access at key?
  bool WouldConvertToSlowElements(uint32_t index);

  static const uint32_t kMinAddedElementsCapacity = 16;

  // Computes the new capacity when expanding the elements of a JSObject.
  static uint32_t NewElementsCapacity(uint32_t old_capacity) {
    // (old_capacity + 50%) + kMinAddedElementsCapacity
    return old_capacity + (old_capacity >> 1) + kMinAddedElementsCapacity;
  }

  // These methods do not perform access checks!
  template <AllocationSiteUpdateMode update_or_check =
                AllocationSiteUpdateMode::kUpdate>
  static bool UpdateAllocationSite(Handle<JSObject> object,
                                   ElementsKind to_kind);

  // Lookup interceptors are used for handling properties controlled by host
  // objects.
  inline bool HasNamedInterceptor();
  inline bool HasIndexedInterceptor();

  // Support functions for v8 api (needed for correct interceptor behavior).
  V8_WARN_UNUSED_RESULT static Maybe<bool> HasRealNamedProperty(
      Handle<JSObject> object, Handle<Name> name);
  V8_WARN_UNUSED_RESULT static Maybe<bool> HasRealElementProperty(
      Handle<JSObject> object, uint32_t index);
  V8_WARN_UNUSED_RESULT static Maybe<bool> HasRealNamedCallbackProperty(
      Handle<JSObject> object, Handle<Name> name);

  // Get the header size for a JSObject.  Used to compute the index of
  // embedder fields as well as the number of embedder fields.
  // The |function_has_prototype_slot| parameter is needed only for
  // JSFunction objects.
  static int GetHeaderSize(InstanceType instance_type,
                           bool function_has_prototype_slot = false);
  static inline int GetHeaderSize(const Map* map);
  inline int GetHeaderSize() const;

  static inline int GetEmbedderFieldCount(const Map* map);
  inline int GetEmbedderFieldCount() const;
  inline int GetEmbedderFieldOffset(int index);
  inline Object* GetEmbedderField(int index);
  inline void SetEmbedderField(int index, Object* value);
  inline void SetEmbedderField(int index, Smi* value);
  bool WasConstructedFromApiFunction();

  // Returns a new map with all transitions dropped from the object's current
  // map and the ElementsKind set.
  static Handle<Map> GetElementsTransitionMap(Handle<JSObject> object,
                                              ElementsKind to_kind);
  static void TransitionElementsKind(Handle<JSObject> object,
                                     ElementsKind to_kind);

  // Always use this to migrate an object to a new map.
  // |expected_additional_properties| is only used for fast-to-slow transitions
  // and ignored otherwise.
  static void MigrateToMap(Handle<JSObject> object, Handle<Map> new_map,
                           int expected_additional_properties = 0);

  // Forces a prototype without any of the checks that the regular SetPrototype
  // would do.
  static void ForceSetPrototype(Handle<JSObject> object, Handle<Object> proto);

  // Convert the object to use the canonical dictionary
  // representation. If the object is expected to have additional properties
  // added this number can be indicated to have the backing store allocated to
  // an initial capacity for holding these properties.
  static void NormalizeProperties(Handle<JSObject> object,
                                  PropertyNormalizationMode mode,
                                  int expected_additional_properties,
                                  const char* reason);

  // Convert and update the elements backing store to be a
  // NumberDictionary dictionary.  Returns the backing after conversion.
  static Handle<NumberDictionary> NormalizeElements(Handle<JSObject> object);

  void RequireSlowElements(NumberDictionary* dictionary);

  // Transform slow named properties to fast variants.
  static void MigrateSlowToFast(Handle<JSObject> object,
                                int unused_property_fields, const char* reason);

  inline bool IsUnboxedDoubleField(FieldIndex index);

  // Access fast-case object properties at index.
  static Handle<Object> FastPropertyAt(Handle<JSObject> object,
                                       Representation representation,
                                       FieldIndex index);
  inline Object* RawFastPropertyAt(FieldIndex index);
  inline double RawFastDoublePropertyAt(FieldIndex index);
  inline uint64_t RawFastDoublePropertyAsBitsAt(FieldIndex index);

  inline void FastPropertyAtPut(FieldIndex index, Object* value);
  inline void RawFastPropertyAtPut(FieldIndex index, Object* value);
  inline void RawFastDoublePropertyAsBitsAtPut(FieldIndex index, uint64_t bits);
  inline void WriteToField(int descriptor, PropertyDetails details,
                           Object* value);

  // Access to in object properties.
  inline int GetInObjectPropertyOffset(int index);
  inline Object* InObjectPropertyAt(int index);
  inline Object* InObjectPropertyAtPut(int index,
                                       Object* value,
                                       WriteBarrierMode mode
                                       = UPDATE_WRITE_BARRIER);

  // Set the object's prototype (only JSReceiver and null are allowed values).
  V8_WARN_UNUSED_RESULT static Maybe<bool> SetPrototype(
      Handle<JSObject> object, Handle<Object> value, bool from_javascript,
      ShouldThrow should_throw);

  // Makes the object prototype immutable
  // Never called from JavaScript
  static void SetImmutableProto(Handle<JSObject> object);

  // Initializes the body starting at |start_offset|. It is responsibility of
  // the caller to initialize object header. Fill the pre-allocated fields with
  // pre_allocated_value and the rest with filler_value.
  // Note: this call does not update write barrier, the caller is responsible
  // to ensure that |filler_value| can be collected without WB here.
  inline void InitializeBody(Map* map, int start_offset,
                             Object* pre_allocated_value, Object* filler_value);

  // Check whether this object references another object
  bool ReferencesObject(Object* obj);

  V8_WARN_UNUSED_RESULT static Maybe<bool> TestIntegrityLevel(
      Handle<JSObject> object, IntegrityLevel lvl);

  V8_WARN_UNUSED_RESULT static Maybe<bool> PreventExtensions(
      Handle<JSObject> object, ShouldThrow should_throw);

  static bool IsExtensible(Handle<JSObject> object);

  DECL_CAST(JSObject)

  // Dispatched behavior.
  void JSObjectShortPrint(StringStream* accumulator);
  DECL_PRINTER(JSObject)
  DECL_VERIFIER(JSObject)
#ifdef OBJECT_PRINT
  bool PrintProperties(std::ostream& os);  // NOLINT
  void PrintElements(std::ostream& os);    // NOLINT
#endif
#if defined(DEBUG) || defined(OBJECT_PRINT)
  void PrintTransitions(std::ostream& os);  // NOLINT
#endif

  static void PrintElementsTransition(
      FILE* file, Handle<JSObject> object,
      ElementsKind from_kind, Handle<FixedArrayBase> from_elements,
      ElementsKind to_kind, Handle<FixedArrayBase> to_elements);

  void PrintInstanceMigration(FILE* file, Map* original_map, Map* new_map);

#ifdef DEBUG
  // Structure for collecting spill information about JSObjects.
  class SpillInformation {
   public:
    void Clear();
    void Print();
    int number_of_objects_;
    int number_of_objects_with_fast_properties_;
    int number_of_objects_with_fast_elements_;
    int number_of_fast_used_fields_;
    int number_of_fast_unused_fields_;
    int number_of_slow_used_properties_;
    int number_of_slow_unused_properties_;
    int number_of_fast_used_elements_;
    int number_of_fast_unused_elements_;
    int number_of_slow_used_elements_;
    int number_of_slow_unused_elements_;
  };

  void IncrementSpillStatistics(Isolate* isolate, SpillInformation* info);
#endif

#ifdef VERIFY_HEAP
  // If a GC was caused while constructing this object, the elements pointer
  // may point to a one pointer filler map. The object won't be rooted, but
  // our heap verification code could stumble across it.
  bool ElementsAreSafeToExamine();
#endif

  Object* SlowReverseLookup(Object* value);

  // Maximal number of elements (numbered 0 .. kMaxElementCount - 1).
  // Also maximal value of JSArray's length property.
  static const uint32_t kMaxElementCount = 0xffffffffu;

  // Constants for heuristics controlling conversion of fast elements
  // to slow elements.

  // Maximal gap that can be introduced by adding an element beyond
  // the current elements length.
  static const uint32_t kMaxGap = 1024;

  // Maximal length of fast elements array that won't be checked for
  // being dense enough on expansion.
  static const int kMaxUncheckedFastElementsLength = 5000;

  // Same as above but for old arrays. This limit is more strict. We
  // don't want to be wasteful with long lived objects.
  static const int kMaxUncheckedOldFastElementsLength = 500;

  // This constant applies only to the initial map of "global.Object" and
  // not to arbitrary other JSObject maps.
  static const int kInitialGlobalObjectUnusedPropertiesCount = 4;

  static const int kMaxInstanceSize = 255 * kPointerSize;

  // When extending the backing storage for property values, we increase
  // its size by more than the 1 entry necessary, so sequentially adding fields
  // to the same object requires fewer allocations and copies.
  static const int kFieldsAdded = 3;
  STATIC_ASSERT(kMaxNumberOfDescriptors + kFieldsAdded <=
                PropertyArray::kMaxLength);

  // Layout description.
  static const int kElementsOffset = JSReceiver::kHeaderSize;
  static const int kHeaderSize = kElementsOffset + kPointerSize;

  STATIC_ASSERT(kHeaderSize == Internals::kJSObjectHeaderSize);
  static const int kMaxInObjectProperties =
      (kMaxInstanceSize - kHeaderSize) >> kPointerSizeLog2;
  STATIC_ASSERT(kMaxInObjectProperties <= kMaxNumberOfDescriptors);
  // TODO(cbruni): Revisit calculation of the max supported embedder fields.
  static const int kMaxEmbedderFields =
      ((1 << kFirstInobjectPropertyOffsetBitCount) - 1 - kHeaderSize) >>
      kPointerSizeLog2;
  STATIC_ASSERT(kMaxEmbedderFields <= kMaxInObjectProperties);

  class BodyDescriptor;
  // No weak fields.
  typedef BodyDescriptor BodyDescriptorWeak;

  class FastBodyDescriptor;
  // No weak fields.
  typedef FastBodyDescriptor FastBodyDescriptorWeak;

  // Gets the number of currently used elements.
  int GetFastElementsUsage();

  static bool AllCanRead(LookupIterator* it);
  static bool AllCanWrite(LookupIterator* it);

 private:
  friend class JSReceiver;
  friend class Object;

  // Used from Object::GetProperty().
  V8_WARN_UNUSED_RESULT static MaybeHandle<Object>
  GetPropertyWithFailedAccessCheck(LookupIterator* it);

  V8_WARN_UNUSED_RESULT static Maybe<bool> SetPropertyWithFailedAccessCheck(
      LookupIterator* it, Handle<Object> value, ShouldThrow should_throw);

  V8_WARN_UNUSED_RESULT static Maybe<bool> DeletePropertyWithInterceptor(
      LookupIterator* it, ShouldThrow should_throw);

  bool ReferencesObjectFromElements(FixedArray* elements,
                                    ElementsKind kind,
                                    Object* object);

  // Helper for fast versions of preventExtensions, seal, and freeze.
  // attrs is one of NONE, SEALED, or FROZEN (depending on the operation).
  template <PropertyAttributes attrs>
  V8_WARN_UNUSED_RESULT static Maybe<bool> PreventExtensionsWithTransition(
      Handle<JSObject> object, ShouldThrow should_throw);

  DISALLOW_IMPLICIT_CONSTRUCTORS(JSObject);
};


// JSAccessorPropertyDescriptor is just a JSObject with a specific initial
// map. This initial map adds in-object properties for "get", "set",
// "enumerable" and "configurable" properties, as assigned by the
// FromPropertyDescriptor function for regular accessor properties.
class JSAccessorPropertyDescriptor: public JSObject {
 public:
  // Offsets of object fields.
  static const int kGetOffset = JSObject::kHeaderSize;
  static const int kSetOffset = kGetOffset + kPointerSize;
  static const int kEnumerableOffset = kSetOffset + kPointerSize;
  static const int kConfigurableOffset = kEnumerableOffset + kPointerSize;
  static const int kSize = kConfigurableOffset + kPointerSize;
  // Indices of in-object properties.
  static const int kGetIndex = 0;
  static const int kSetIndex = 1;
  static const int kEnumerableIndex = 2;
  static const int kConfigurableIndex = 3;

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(JSAccessorPropertyDescriptor);
};


// JSDataPropertyDescriptor is just a JSObject with a specific initial map.
// This initial map adds in-object properties for "value", "writable",
// "enumerable" and "configurable" properties, as assigned by the
// FromPropertyDescriptor function for regular data properties.
class JSDataPropertyDescriptor: public JSObject {
 public:
  // Offsets of object fields.
  static const int kValueOffset = JSObject::kHeaderSize;
  static const int kWritableOffset = kValueOffset + kPointerSize;
  static const int kEnumerableOffset = kWritableOffset + kPointerSize;
  static const int kConfigurableOffset = kEnumerableOffset + kPointerSize;
  static const int kSize = kConfigurableOffset + kPointerSize;
  // Indices of in-object properties.
  static const int kValueIndex = 0;
  static const int kWritableIndex = 1;
  static const int kEnumerableIndex = 2;
  static const int kConfigurableIndex = 3;

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(JSDataPropertyDescriptor);
};


// JSIteratorResult is just a JSObject with a specific initial map.
// This initial map adds in-object properties for "done" and "value",
// as specified by ES6 section 25.1.1.3 The IteratorResult Interface
class JSIteratorResult: public JSObject {
 public:
  DECL_ACCESSORS(value, Object)

  DECL_ACCESSORS(done, Object)

  // Offsets of object fields.
  static const int kValueOffset = JSObject::kHeaderSize;
  static const int kDoneOffset = kValueOffset + kPointerSize;
  static const int kSize = kDoneOffset + kPointerSize;
  // Indices of in-object properties.
  static const int kValueIndex = 0;
  static const int kDoneIndex = 1;

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(JSIteratorResult);
};

// FreeSpace are fixed-size free memory blocks used by the heap and GC.
// They look like heap objects (are heap object tagged and have a map) so that
// the heap remains iterable.  They have a size and a next pointer.
// The next pointer is the raw address of the next FreeSpace object (or NULL)
// in the free list.
class FreeSpace: public HeapObject {
 public:
  // [size]: size of the free space including the header.
  inline int size() const;
  inline void set_size(int value);

  inline int relaxed_read_size() const;
  inline void relaxed_write_size(int value);

  inline int Size();

  // Accessors for the next field.
  inline FreeSpace* next();
  inline void set_next(FreeSpace* next);

  inline static FreeSpace* cast(HeapObject* obj);

  // Dispatched behavior.
  DECL_PRINTER(FreeSpace)
  DECL_VERIFIER(FreeSpace)

  // Layout description.
  // Size is smi tagged when it is stored.
  static const int kSizeOffset = HeapObject::kHeaderSize;
  static const int kNextOffset = POINTER_SIZE_ALIGN(kSizeOffset + kPointerSize);
  static const int kSize = kNextOffset + kPointerSize;

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(FreeSpace);
};

class PrototypeInfo;

// An abstract superclass, a marker class really, for simple structure classes.
// It doesn't carry much functionality but allows struct classes to be
// identified in the type system.
class Struct: public HeapObject {
 public:
  inline void InitializeBody(int object_size);
  DECL_CAST(Struct)
  void BriefPrintDetails(std::ostream& os);
};

class Tuple2 : public Struct {
 public:
  DECL_ACCESSORS(value1, Object)
  DECL_ACCESSORS(value2, Object)

  DECL_CAST(Tuple2)

  // Dispatched behavior.
  DECL_PRINTER(Tuple2)
  DECL_VERIFIER(Tuple2)
  void BriefPrintDetails(std::ostream& os);

  static const int kValue1Offset = HeapObject::kHeaderSize;
  static const int kValue2Offset = kValue1Offset + kPointerSize;
  static const int kSize = kValue2Offset + kPointerSize;

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(Tuple2);
};

class Tuple3 : public Tuple2 {
 public:
  DECL_ACCESSORS(value3, Object)

  DECL_CAST(Tuple3)

  // Dispatched behavior.
  DECL_PRINTER(Tuple3)
  DECL_VERIFIER(Tuple3)
  void BriefPrintDetails(std::ostream& os);

  static const int kValue3Offset = Tuple2::kSize;
  static const int kSize = kValue3Offset + kPointerSize;

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(Tuple3);
};

class AsyncGeneratorRequest : public Struct {
 public:
  // Holds an AsyncGeneratorRequest, or Undefined.
  DECL_ACCESSORS(next, Object)
  DECL_INT_ACCESSORS(resume_mode)
  DECL_ACCESSORS(value, Object)
  DECL_ACCESSORS(promise, Object)

  static const int kNextOffset = Struct::kHeaderSize;
  static const int kResumeModeOffset = kNextOffset + kPointerSize;
  static const int kValueOffset = kResumeModeOffset + kPointerSize;
  static const int kPromiseOffset = kValueOffset + kPointerSize;
  static const int kSize = kPromiseOffset + kPointerSize;

  DECL_CAST(AsyncGeneratorRequest)
  DECL_PRINTER(AsyncGeneratorRequest)
  DECL_VERIFIER(AsyncGeneratorRequest)

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(AsyncGeneratorRequest);
};

// List of builtin functions we want to identify to improve code
// generation.
//
// Each entry has a name of a global object property holding an object
// optionally followed by ".prototype", a name of a builtin function
// on the object (the one the id is set for), and a label.
//
// Installation of ids for the selected builtin functions is handled
// by the bootstrapper.
#define FUNCTIONS_WITH_ID_LIST(V)                           \
  V(Array, isArray, ArrayIsArray)                           \
  V(Array.prototype, concat, ArrayConcat)                   \
  V(Array.prototype, every, ArrayEvery)                     \
  V(Array.prototype, fill, ArrayFill)                       \
  V(Array.prototype, filter, ArrayFilter)                   \
  V(Array.prototype, findIndex, ArrayFindIndex)             \
  V(Array.prototype, forEach, ArrayForEach)                 \
  V(Array.prototype, includes, ArrayIncludes)               \
  V(Array.prototype, indexOf, ArrayIndexOf)                 \
  V(Array.prototype, join, ArrayJoin)                       \
  V(Array.prototype, lastIndexOf, ArrayLastIndexOf)         \
  V(Array.prototype, map, ArrayMap)                         \
  V(Array.prototype, pop, ArrayPop)                         \
  V(Array.prototype, push, ArrayPush)                       \
  V(Array.prototype, reverse, ArrayReverse)                 \
  V(Array.prototype, shift, ArrayShift)                     \
  V(Array.prototype, slice, ArraySlice)                     \
  V(Array.prototype, some, ArraySome)                       \
  V(Array.prototype, splice, ArraySplice)                   \
  V(Array.prototype, unshift, ArrayUnshift)                 \
  V(Date, now, DateNow)                                     \
  V(Date.prototype, getDate, DateGetDate)                   \
  V(Date.prototype, getDay, DateGetDay)                     \
  V(Date.prototype, getFullYear, DateGetFullYear)           \
  V(Date.prototype, getHours, DateGetHours)                 \
  V(Date.prototype, getMilliseconds, DateGetMilliseconds)   \
  V(Date.prototype, getMinutes, DateGetMinutes)             \
  V(Date.prototype, getMonth, DateGetMonth)                 \
  V(Date.prototype, getSeconds, DateGetSeconds)             \
  V(Date.prototype, getTime, DateGetTime)                   \
  V(Function.prototype, apply, FunctionApply)               \
  V(Function.prototype, bind, FunctionBind)                 \
  V(Function.prototype, call, FunctionCall)                 \
  V(Object, assign, ObjectAssign)                           \
  V(Object, create, ObjectCreate)                           \
  V(Object, is, ObjectIs)                                   \
  V(Object.prototype, hasOwnProperty, ObjectHasOwnProperty) \
  V(Object.prototype, isPrototypeOf, ObjectIsPrototypeOf)   \
  V(Object.prototype, toString, ObjectToString)             \
  V(RegExp.prototype, compile, RegExpCompile)               \
  V(RegExp.prototype, exec, RegExpExec)                     \
  V(RegExp.prototype, test, RegExpTest)                     \
  V(RegExp.prototype, toString, RegExpToString)             \
  V(String.prototype, charCodeAt, StringCharCodeAt)         \
  V(String.prototype, charAt, StringCharAt)                 \
  V(String.prototype, codePointAt, StringCodePointAt)       \
  V(String.prototype, concat, StringConcat)                 \
  V(String.prototype, endsWith, StringEndsWith)             \
  V(String.prototype, includes, StringIncludes)             \
  V(String.prototype, indexOf, StringIndexOf)               \
  V(String.prototype, lastIndexOf, StringLastIndexOf)       \
  V(String.prototype, repeat, StringRepeat)                 \
  V(String.prototype, slice, StringSlice)                   \
  V(String.prototype, startsWith, StringStartsWith)         \
  V(String.prototype, substr, StringSubstr)                 \
  V(String.prototype, substring, StringSubstring)           \
  V(String.prototype, toLowerCase, StringToLowerCase)       \
  V(String.prototype, toString, StringToString)             \
  V(String.prototype, toUpperCase, StringToUpperCase)       \
  V(String.prototype, trim, StringTrim)                     \
  V(String.prototype, trimLeft, StringTrimStart)            \
  V(String.prototype, trimRight, StringTrimEnd)             \
  V(String.prototype, valueOf, StringValueOf)               \
  V(String, fromCharCode, StringFromCharCode)               \
  V(String, fromCodePoint, StringFromCodePoint)             \
  V(String, raw, StringRaw)                                 \
  V(Math, random, MathRandom)                               \
  V(Math, floor, MathFloor)                                 \
  V(Math, round, MathRound)                                 \
  V(Math, ceil, MathCeil)                                   \
  V(Math, abs, MathAbs)                                     \
  V(Math, log, MathLog)                                     \
  V(Math, log1p, MathLog1p)                                 \
  V(Math, log2, MathLog2)                                   \
  V(Math, log10, MathLog10)                                 \
  V(Math, cbrt, MathCbrt)                                   \
  V(Math, exp, MathExp)                                     \
  V(Math, expm1, MathExpm1)                                 \
  V(Math, sqrt, MathSqrt)                                   \
  V(Math, pow, MathPow)                                     \
  V(Math, max, MathMax)                                     \
  V(Math, min, MathMin)                                     \
  V(Math, cos, MathCos)                                     \
  V(Math, cosh, MathCosh)                                   \
  V(Math, sign, MathSign)                                   \
  V(Math, sin, MathSin)                                     \
  V(Math, sinh, MathSinh)                                   \
  V(Math, tan, MathTan)                                     \
  V(Math, tanh, MathTanh)                                   \
  V(Math, acos, MathAcos)                                   \
  V(Math, acosh, MathAcosh)                                 \
  V(Math, asin, MathAsin)                                   \
  V(Math, asinh, MathAsinh)                                 \
  V(Math, atan, MathAtan)                                   \
  V(Math, atan2, MathAtan2)                                 \
  V(Math, atanh, MathAtanh)                                 \
  V(Math, imul, MathImul)                                   \
  V(Math, clz32, MathClz32)                                 \
  V(Math, fround, MathFround)                               \
  V(Math, trunc, MathTrunc)                                 \
  V(Number, isFinite, NumberIsFinite)                       \
  V(Number, isInteger, NumberIsInteger)                     \
  V(Number, isNaN, NumberIsNaN)                             \
  V(Number, isSafeInteger, NumberIsSafeInteger)             \
  V(Number, parseFloat, NumberParseFloat)                   \
  V(Number, parseInt, NumberParseInt)                       \
  V(Number.prototype, toString, NumberToString)             \
  V(Map.prototype, clear, MapClear)                         \
  V(Map.prototype, delete, MapDelete)                       \
  V(Map.prototype, entries, MapEntries)                     \
  V(Map.prototype, forEach, MapForEach)                     \
  V(Map.prototype, has, MapHas)                             \
  V(Map.prototype, keys, MapKeys)                           \
  V(Map.prototype, get, MapGet)                             \
  V(Map.prototype, set, MapSet)                             \
  V(Map.prototype, values, MapValues)                       \
  V(Set.prototype, add, SetAdd)                             \
  V(Set.prototype, clear, SetClear)                         \
  V(Set.prototype, delete, SetDelete)                       \
  V(Set.prototype, entries, SetEntries)                     \
  V(Set.prototype, forEach, SetForEach)                     \
  V(Set.prototype, has, SetHas)                             \
  V(Set.prototype, values, SetValues)                       \
  V(WeakMap.prototype, delete, WeakMapDelete)               \
  V(WeakMap.prototype, has, WeakMapHas)                     \
  V(WeakMap.prototype, set, WeakMapSet)                     \
  V(WeakSet.prototype, add, WeakSetAdd)                     \
  V(WeakSet.prototype, delete, WeakSetDelete)               \
  V(WeakSet.prototype, has, WeakSetHas)

#define ATOMIC_FUNCTIONS_WITH_ID_LIST(V)              \
  V(Atomics, load, AtomicsLoad)                       \
  V(Atomics, store, AtomicsStore)                     \
  V(Atomics, exchange, AtomicsExchange)               \
  V(Atomics, compareExchange, AtomicsCompareExchange) \
  V(Atomics, add, AtomicsAdd)                         \
  V(Atomics, sub, AtomicsSub)                         \
  V(Atomics, and, AtomicsAnd)                         \
  V(Atomics, or, AtomicsOr)                           \
  V(Atomics, xor, AtomicsXor)

enum BuiltinFunctionId {
  kInvalidBuiltinFunctionId = -1,
  kArrayConstructor,
#define DECL_FUNCTION_ID(ignored1, ignore2, name) k##name,
  FUNCTIONS_WITH_ID_LIST(DECL_FUNCTION_ID)
      ATOMIC_FUNCTIONS_WITH_ID_LIST(DECL_FUNCTION_ID)
#undef DECL_FUNCTION_ID
  // These are manually assigned to special getters during bootstrapping.
  kArrayBufferByteLength,
  kArrayBufferIsView,
  kArrayEntries,
  kArrayKeys,
  kArrayValues,
  kArrayIteratorNext,
  kBigIntConstructor,
  kMapSize,
  kSetSize,
  kMapIteratorNext,
  kSetIteratorNext,
  kDataViewBuffer,
  kDataViewByteLength,
  kDataViewByteOffset,
  kFunctionHasInstance,
  kGlobalDecodeURI,
  kGlobalDecodeURIComponent,
  kGlobalEncodeURI,
  kGlobalEncodeURIComponent,
  kGlobalEscape,
  kGlobalUnescape,
  kGlobalIsFinite,
  kGlobalIsNaN,
  kNumberConstructor,
  kSymbolConstructor,
  kTypedArrayByteLength,
  kTypedArrayByteOffset,
  kTypedArrayEntries,
  kTypedArrayKeys,
  kTypedArrayLength,
  kTypedArrayToStringTag,
  kTypedArrayValues,
  kSharedArrayBufferByteLength,
  kStringConstructor,
  kStringIterator,
  kStringIteratorNext,
  kStringToLowerCaseIntl,
  kStringToUpperCaseIntl
};

class JSGeneratorObject: public JSObject {
 public:
  // [function]: The function corresponding to this generator object.
  DECL_ACCESSORS(function, JSFunction)

  // [context]: The context of the suspended computation.
  DECL_ACCESSORS(context, Context)

  // [receiver]: The receiver of the suspended computation.
  DECL_ACCESSORS(receiver, Object)

  // [input_or_debug_pos]
  // For executing generators: the most recent input value.
  // For suspended generators: debug information (bytecode offset).
  // There is currently no need to remember the most recent input value for a
  // suspended generator.
  DECL_ACCESSORS(input_or_debug_pos, Object)

  // [resume_mode]: The most recent resume mode.
  enum ResumeMode { kNext, kReturn, kThrow };
  DECL_INT_ACCESSORS(resume_mode)

  // [continuation]
  //
  // A positive value indicates a suspended generator.  The special
  // kGeneratorExecuting and kGeneratorClosed values indicate that a generator
  // cannot be resumed.
  inline int continuation() const;
  inline void set_continuation(int continuation);
  inline bool is_closed() const;
  inline bool is_executing() const;
  inline bool is_suspended() const;

  // For suspended generators: the source position at which the generator
  // is suspended.
  int source_position() const;

  // [parameters_and_registers]: Saved interpreter register file.
  DECL_ACCESSORS(parameters_and_registers, FixedArray)

  DECL_CAST(JSGeneratorObject)

  // Dispatched behavior.
  DECL_PRINTER(JSGeneratorObject)
  DECL_VERIFIER(JSGeneratorObject)

  // Magic sentinel values for the continuation.
  static const int kGeneratorExecuting = -2;
  static const int kGeneratorClosed = -1;

  // Layout description.
  static const int kFunctionOffset = JSObject::kHeaderSize;
  static const int kContextOffset = kFunctionOffset + kPointerSize;
  static const int kReceiverOffset = kContextOffset + kPointerSize;
  static const int kInputOrDebugPosOffset = kReceiverOffset + kPointerSize;
  static const int kResumeModeOffset = kInputOrDebugPosOffset + kPointerSize;
  static const int kContinuationOffset = kResumeModeOffset + kPointerSize;
  static const int kParametersAndRegistersOffset =
      kContinuationOffset + kPointerSize;
  static const int kSize = kParametersAndRegistersOffset + kPointerSize;

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(JSGeneratorObject);
};

class JSAsyncGeneratorObject : public JSGeneratorObject {
 public:
  DECL_CAST(JSAsyncGeneratorObject)

  // Dispatched behavior.
  DECL_VERIFIER(JSAsyncGeneratorObject)

  // [queue]
  // Pointer to the head of a singly linked list of AsyncGeneratorRequest, or
  // undefined.
  DECL_ACCESSORS(queue, HeapObject)

  // [is_awaiting]
  // Whether or not the generator is currently awaiting.
  DECL_INT_ACCESSORS(is_awaiting)

  // Layout description.
  static const int kQueueOffset = JSGeneratorObject::kSize;
  static const int kIsAwaitingOffset = kQueueOffset + kPointerSize;
  static const int kSize = kIsAwaitingOffset + kPointerSize;

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(JSAsyncGeneratorObject);
};

// JSBoundFunction describes a bound function exotic object.
class JSBoundFunction : public JSObject {
 public:
  // [bound_target_function]: The wrapped function object.
  inline Object* raw_bound_target_function() const;
  DECL_ACCESSORS(bound_target_function, JSReceiver)

  // [bound_this]: The value that is always passed as the this value when
  // calling the wrapped function.
  DECL_ACCESSORS(bound_this, Object)

  // [bound_arguments]: A list of values whose elements are used as the first
  // arguments to any call to the wrapped function.
  DECL_ACCESSORS(bound_arguments, FixedArray)

  static MaybeHandle<String> GetName(Isolate* isolate,
                                     Handle<JSBoundFunction> function);
  static Maybe<int> GetLength(Isolate* isolate,
                              Handle<JSBoundFunction> function);
  static MaybeHandle<Context> GetFunctionRealm(
      Handle<JSBoundFunction> function);

  DECL_CAST(JSBoundFunction)

  // Dispatched behavior.
  DECL_PRINTER(JSBoundFunction)
  DECL_VERIFIER(JSBoundFunction)

  // The bound function's string representation implemented according
  // to ES6 section 19.2.3.5 Function.prototype.toString ( ).
  static Handle<String> ToString(Handle<JSBoundFunction> function);

  // Layout description.
  static const int kBoundTargetFunctionOffset = JSObject::kHeaderSize;
  static const int kBoundThisOffset = kBoundTargetFunctionOffset + kPointerSize;
  static const int kBoundArgumentsOffset = kBoundThisOffset + kPointerSize;
  static const int kSize = kBoundArgumentsOffset + kPointerSize;

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(JSBoundFunction);
};


// JSFunction describes JavaScript functions.
class JSFunction: public JSObject {
 public:
  // [prototype_or_initial_map]:
  DECL_ACCESSORS(prototype_or_initial_map, Object)

  // [shared]: The information about the function that
  // can be shared by instances.
  DECL_ACCESSORS(shared, SharedFunctionInfo)

  static const int kLengthDescriptorIndex = 0;
  static const int kNameDescriptorIndex = 1;
  // Home object descriptor index when function has a [[HomeObject]] slot.
  static const int kMaybeHomeObjectDescriptorIndex = 2;

  // [context]: The context for this function.
  inline Context* context();
  inline bool has_context() const;
  inline void set_context(Object* context);
  inline JSObject* global_proxy();
  inline Context* native_context();

  static Handle<Object> GetName(Isolate* isolate, Handle<JSFunction> function);
  static Maybe<int> GetLength(Isolate* isolate, Handle<JSFunction> function);
  static Handle<Context> GetFunctionRealm(Handle<JSFunction> function);

  // [code]: The generated code object for this function.  Executed
  // when the function is invoked, e.g. foo() or new foo(). See
  // [[Call]] and [[Construct]] description in ECMA-262, section
  // 8.6.2, page 27.
  inline Code* code();
  inline void set_code(Code* code);
  inline void set_code_no_write_barrier(Code* code);

  // Get the abstract code associated with the function, which will either be
  // a Code object or a BytecodeArray.
  inline AbstractCode* abstract_code();

  // Tells whether or not this function is interpreted.
  //
  // Note: function->IsInterpreted() does not necessarily return the same value
  // as function->shared()->IsInterpreted() because the closure might have been
  // optimized.
  inline bool IsInterpreted();

  // Tells whether or not this function checks its optimization marker in its
  // feedback vector.
  inline bool ChecksOptimizationMarker();

  // Tells whether or not this function holds optimized code.
  //
  // Note: Returning false does not necessarily mean that this function hasn't
  // been optimized, as it may have optimized code on its feedback vector.
  inline bool IsOptimized();

  // Tells whether or not this function has optimized code available to it,
  // either because it is optimized or because it has optimized code in its
  // feedback vector.
  inline bool HasOptimizedCode();

  // Tells whether or not this function has a (non-zero) optimization marker.
  inline bool HasOptimizationMarker();

  // Mark this function for lazy recompilation. The function will be recompiled
  // the next time it is executed.
  void MarkForOptimization(ConcurrencyMode mode);

  // Tells whether or not the function is already marked for lazy recompilation.
  inline bool IsMarkedForOptimization();
  inline bool IsMarkedForConcurrentOptimization();

  // Tells whether or not the function is on the concurrent recompilation queue.
  inline bool IsInOptimizationQueue();

  // Clears the optimized code slot in the function's feedback vector.
  inline void ClearOptimizedCodeSlot(const char* reason);

  // Sets the optimization marker in the function's feedback vector.
  inline void SetOptimizationMarker(OptimizationMarker marker);

  // Clears the optimization marker in the function's feedback vector.
  inline void ClearOptimizationMarker();

  // Completes inobject slack tracking on initial map if it is active.
  inline void CompleteInobjectSlackTrackingIfActive();

  // [feedback_cell]: The FeedbackCell used to hold the FeedbackVector
  // eventually.
  DECL_ACCESSORS(feedback_cell, FeedbackCell)

  // feedback_vector() can be used once the function is compiled.
  inline FeedbackVector* feedback_vector() const;
  inline bool has_feedback_vector() const;
  static void EnsureFeedbackVector(Handle<JSFunction> function);

  // Unconditionally clear the type feedback vector.
  void ClearTypeFeedbackInfo();

  inline bool has_prototype_slot() const;

  // The initial map for an object created by this constructor.
  inline Map* initial_map();
  static void SetInitialMap(Handle<JSFunction> function, Handle<Map> map,
                            Handle<Object> prototype);
  inline bool has_initial_map();
  static void EnsureHasInitialMap(Handle<JSFunction> function);

  // Creates a map that matches the constructor's initial map, but with
  // [[prototype]] being new.target.prototype. Because new.target can be a
  // JSProxy, this can call back into JavaScript.
  static V8_WARN_UNUSED_RESULT MaybeHandle<Map> GetDerivedMap(
      Isolate* isolate, Handle<JSFunction> constructor,
      Handle<JSReceiver> new_target);

  // Get and set the prototype property on a JSFunction. If the
  // function has an initial map the prototype is set on the initial
  // map. Otherwise, the prototype is put in the initial map field
  // until an initial map is needed.
  inline bool has_prototype();
  inline bool has_instance_prototype();
  inline Object* prototype();
  inline Object* instance_prototype();
  static void SetPrototype(Handle<JSFunction> function,
                           Handle<Object> value);

  // Returns if this function has been compiled to native code yet.
  inline bool is_compiled();

  static int GetHeaderSize(bool function_has_prototype_slot) {
    return function_has_prototype_slot ? JSFunction::kSizeWithPrototype
                                       : JSFunction::kSizeWithoutPrototype;
  }

  // Prints the name of the function using PrintF.
  void PrintName(FILE* out = stdout);

  DECL_CAST(JSFunction)

  // Calculate the instance size and in-object properties count.
  static bool CalculateInstanceSizeForDerivedClass(
      Handle<JSFunction> function, InstanceType instance_type,
      int requested_embedder_fields, int* instance_size,
      int* in_object_properties);
  static void CalculateInstanceSizeHelper(InstanceType instance_type,
                                          bool has_prototype_slot,
                                          int requested_embedder_fields,
                                          int requested_in_object_properties,
                                          int* instance_size,
                                          int* in_object_properties);

  class BodyDescriptor;

  // Dispatched behavior.
  DECL_PRINTER(JSFunction)
  DECL_VERIFIER(JSFunction)

  // The function's name if it is configured, otherwise shared function info
  // debug name.
  static Handle<String> GetName(Handle<JSFunction> function);

  // ES6 section 9.2.11 SetFunctionName
  // Because of the way this abstract operation is used in the spec,
  // it should never fail, but in practice it will fail if the generated
  // function name's length exceeds String::kMaxLength.
  static V8_WARN_UNUSED_RESULT bool SetName(Handle<JSFunction> function,
                                            Handle<Name> name,
                                            Handle<String> prefix);

  // The function's displayName if it is set, otherwise name if it is
  // configured, otherwise shared function info
  // debug name.
  static Handle<String> GetDebugName(Handle<JSFunction> function);

  // The function's string representation implemented according to
  // ES6 section 19.2.3.5 Function.prototype.toString ( ).
  static Handle<String> ToString(Handle<JSFunction> function);

// Layout description.
#define JS_FUNCTION_FIELDS(V)                              \
  /* Pointer fields. */                                    \
  V(kSharedFunctionInfoOffset, kPointerSize)               \
  V(kContextOffset, kPointerSize)                          \
  V(kFeedbackCellOffset, kPointerSize)                     \
  V(kEndOfStrongFieldsOffset, 0)                           \
  V(kCodeOffset, kPointerSize)                             \
  /* Size of JSFunction object without prototype field. */ \
  V(kSizeWithoutPrototype, 0)                              \
  V(kPrototypeOrInitialMapOffset, kPointerSize)            \
  /* Size of JSFunction object with prototype field. */    \
  V(kSizeWithPrototype, 0)

  DEFINE_FIELD_OFFSET_CONSTANTS(JSObject::kHeaderSize, JS_FUNCTION_FIELDS)
#undef JS_FUNCTION_FIELDS

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(JSFunction);
};


// JSGlobalProxy's prototype must be a JSGlobalObject or null,
// and the prototype is hidden. JSGlobalProxy always delegates
// property accesses to its prototype if the prototype is not null.
//
// A JSGlobalProxy can be reinitialized which will preserve its identity.
//
// Accessing a JSGlobalProxy requires security check.

class JSGlobalProxy : public JSObject {
 public:
  // [native_context]: the owner native context of this global proxy object.
  // It is null value if this object is not used by any context.
  DECL_ACCESSORS(native_context, Object)

  DECL_CAST(JSGlobalProxy)

  inline bool IsDetachedFrom(JSGlobalObject* global) const;

  static int SizeWithEmbedderFields(int embedder_field_count);

  // Dispatched behavior.
  DECL_PRINTER(JSGlobalProxy)
  DECL_VERIFIER(JSGlobalProxy)

  // Layout description.
  static const int kNativeContextOffset = JSObject::kHeaderSize;
  static const int kSize = kNativeContextOffset + kPointerSize;

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(JSGlobalProxy);
};


// JavaScript global object.
class JSGlobalObject : public JSObject {
 public:
  // [native context]: the natives corresponding to this global object.
  DECL_ACCESSORS(native_context, Context)

  // [global proxy]: the global proxy object of the context
  DECL_ACCESSORS(global_proxy, JSObject)

  // Gets global object properties.
  inline GlobalDictionary* global_dictionary();
  inline void set_global_dictionary(GlobalDictionary* dictionary);

  static void InvalidatePropertyCell(Handle<JSGlobalObject> object,
                                     Handle<Name> name);
  // Ensure that the global object has a cell for the given property name.
  static Handle<PropertyCell> EnsureEmptyPropertyCell(
      Handle<JSGlobalObject> global, Handle<Name> name,
      PropertyCellType cell_type, int* entry_out = nullptr);

  DECL_CAST(JSGlobalObject)

  inline bool IsDetached();

  // Dispatched behavior.
  DECL_PRINTER(JSGlobalObject)
  DECL_VERIFIER(JSGlobalObject)

  // Layout description.
  static const int kNativeContextOffset = JSObject::kHeaderSize;
  static const int kGlobalProxyOffset = kNativeContextOffset + kPointerSize;
  static const int kHeaderSize = kGlobalProxyOffset + kPointerSize;
  static const int kSize = kHeaderSize;

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(JSGlobalObject);
};


// Representation for JS Wrapper objects, String, Number, Boolean, etc.
class JSValue: public JSObject {
 public:
  // [value]: the object being wrapped.
  DECL_ACCESSORS(value, Object)

  DECL_CAST(JSValue)

  // Dispatched behavior.
  DECL_PRINTER(JSValue)
  DECL_VERIFIER(JSValue)

  // Layout description.
  static const int kValueOffset = JSObject::kHeaderSize;
  static const int kSize = kValueOffset + kPointerSize;

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(JSValue);
};


class DateCache;

// Representation for JS date objects.
class JSDate: public JSObject {
 public:
  static V8_WARN_UNUSED_RESULT MaybeHandle<JSDate> New(
      Handle<JSFunction> constructor, Handle<JSReceiver> new_target, double tv);

  // If one component is NaN, all of them are, indicating a NaN time value.
  // [value]: the time value.
  DECL_ACCESSORS(value, Object)
  // [year]: caches year. Either undefined, smi, or NaN.
  DECL_ACCESSORS(year, Object)
  // [month]: caches month. Either undefined, smi, or NaN.
  DECL_ACCESSORS(month, Object)
  // [day]: caches day. Either undefined, smi, or NaN.
  DECL_ACCESSORS(day, Object)
  // [weekday]: caches day of week. Either undefined, smi, or NaN.
  DECL_ACCESSORS(weekday, Object)
  // [hour]: caches hours. Either undefined, smi, or NaN.
  DECL_ACCESSORS(hour, Object)
  // [min]: caches minutes. Either undefined, smi, or NaN.
  DECL_ACCESSORS(min, Object)
  // [sec]: caches seconds. Either undefined, smi, or NaN.
  DECL_ACCESSORS(sec, Object)
  // [cache stamp]: sample of the date cache stamp at the
  // moment when chached fields were cached.
  DECL_ACCESSORS(cache_stamp, Object)

  DECL_CAST(JSDate)

  // Returns the time value (UTC) identifying the current time.
  static double CurrentTimeValue(Isolate* isolate);

  // Returns the date field with the specified index.
  // See FieldIndex for the list of date fields.
  static Object* GetField(Object* date, Smi* index);

  static Handle<Object> SetValue(Handle<JSDate> date, double v);

  void SetValue(Object* value, bool is_value_nan);

  // Dispatched behavior.
  DECL_PRINTER(JSDate)
  DECL_VERIFIER(JSDate)

  // The order is important. It must be kept in sync with date macros
  // in macros.py.
  enum FieldIndex {
    kDateValue,
    kYear,
    kMonth,
    kDay,
    kWeekday,
    kHour,
    kMinute,
    kSecond,
    kFirstUncachedField,
    kMillisecond = kFirstUncachedField,
    kDays,
    kTimeInDay,
    kFirstUTCField,
    kYearUTC = kFirstUTCField,
    kMonthUTC,
    kDayUTC,
    kWeekdayUTC,
    kHourUTC,
    kMinuteUTC,
    kSecondUTC,
    kMillisecondUTC,
    kDaysUTC,
    kTimeInDayUTC,
    kTimezoneOffset
  };

  // Layout description.
  static const int kValueOffset = JSObject::kHeaderSize;
  static const int kYearOffset = kValueOffset + kPointerSize;
  static const int kMonthOffset = kYearOffset + kPointerSize;
  static const int kDayOffset = kMonthOffset + kPointerSize;
  static const int kWeekdayOffset = kDayOffset + kPointerSize;
  static const int kHourOffset = kWeekdayOffset  + kPointerSize;
  static const int kMinOffset = kHourOffset + kPointerSize;
  static const int kSecOffset = kMinOffset + kPointerSize;
  static const int kCacheStampOffset = kSecOffset + kPointerSize;
  static const int kSize = kCacheStampOffset + kPointerSize;

 private:
  inline Object* DoGetField(FieldIndex index);

  Object* GetUTCField(FieldIndex index, double value, DateCache* date_cache);

  // Computes and caches the cacheable fields of the date.
  inline void SetCachedFields(int64_t local_time_ms, DateCache* date_cache);


  DISALLOW_IMPLICIT_CONSTRUCTORS(JSDate);
};


// Representation of message objects used for error reporting through
// the API. The messages are formatted in JavaScript so this object is
// a real JavaScript object. The information used for formatting the
// error messages are not directly accessible from JavaScript to
// prevent leaking information to user code called during error
// formatting.
class JSMessageObject: public JSObject {
 public:
  // [type]: the type of error message.
  inline int type() const;
  inline void set_type(int value);

  // [arguments]: the arguments for formatting the error message.
  DECL_ACCESSORS(argument, Object)

  // [script]: the script from which the error message originated.
  DECL_ACCESSORS(script, Script)

  // [stack_frames]: an array of stack frames for this error object.
  DECL_ACCESSORS(stack_frames, Object)

  // [start_position]: the start position in the script for the error message.
  inline int start_position() const;
  inline void set_start_position(int value);

  // [end_position]: the end position in the script for the error message.
  inline int end_position() const;
  inline void set_end_position(int value);

  // Returns the line number for the error message (1-based), or
  // Message::kNoLineNumberInfo if the line cannot be determined.
  int GetLineNumber() const;

  // Returns the offset of the given position within the containing line.
  int GetColumnNumber() const;

  // Returns the source code line containing the given source
  // position, or the empty string if the position is invalid.
  Handle<String> GetSourceLine() const;

  inline int error_level() const;
  inline void set_error_level(int level);

  DECL_CAST(JSMessageObject)

  // Dispatched behavior.
  DECL_PRINTER(JSMessageObject)
  DECL_VERIFIER(JSMessageObject)

  // Layout description.
  static const int kTypeOffset = JSObject::kHeaderSize;
  static const int kArgumentsOffset = kTypeOffset + kPointerSize;
  static const int kScriptOffset = kArgumentsOffset + kPointerSize;
  static const int kStackFramesOffset = kScriptOffset + kPointerSize;
  static const int kStartPositionOffset = kStackFramesOffset + kPointerSize;
  static const int kEndPositionOffset = kStartPositionOffset + kPointerSize;
  static const int kErrorLevelOffset = kEndPositionOffset + kPointerSize;
  static const int kSize = kErrorLevelOffset + kPointerSize;

  typedef FixedBodyDescriptor<HeapObject::kMapOffset,
                              kStackFramesOffset + kPointerSize,
                              kSize> BodyDescriptor;
  // No weak fields.
  typedef BodyDescriptor BodyDescriptorWeak;
};

class AllocationSite : public Struct, public NeverReadOnlySpaceObject {
 public:
  static const uint32_t kMaximumArrayBytesToPretransition = 8 * 1024;
  static const double kPretenureRatio;
  static const int kPretenureMinimumCreated = 100;

  // Values for pretenure decision field.
  enum PretenureDecision {
    kUndecided = 0,
    kDontTenure = 1,
    kMaybeTenure = 2,
    kTenure = 3,
    kZombie = 4,
    kLastPretenureDecisionValue = kZombie
  };

  // Use the mixin methods over the HeapObject methods.
  // TODO(v8:7786) Remove once the HeapObject methods are gone.
  using NeverReadOnlySpaceObject::GetHeap;
  using NeverReadOnlySpaceObject::GetIsolate;

  const char* PretenureDecisionName(PretenureDecision decision);

  // Contains either a Smi-encoded bitfield or a boilerplate. If it's a Smi the
  // AllocationSite is for a constructed Array.
  DECL_ACCESSORS(transition_info_or_boilerplate, Object)
  DECL_ACCESSORS(boilerplate, JSObject)
  DECL_INT_ACCESSORS(transition_info)

  // nested_site threads a list of sites that represent nested literals
  // walked in a particular order. So [[1, 2], 1, 2] will have one
  // nested_site, but [[1, 2], 3, [4]] will have a list of two.
  DECL_ACCESSORS(nested_site, Object)

  // Bitfield containing pretenuring information.
  DECL_INT32_ACCESSORS(pretenure_data)

  DECL_INT32_ACCESSORS(pretenure_create_count)
  DECL_ACCESSORS(dependent_code, DependentCode)

  // heap->allocation_site_list() points to the last AllocationSite which form
  // a linked list through the weak_next property. The GC might remove elements
  // from the list by updateing weak_next.
  DECL_ACCESSORS(weak_next, Object)

  inline void Initialize();

  // Checks if the allocation site contain weak_next field;
  inline bool HasWeakNext() const;

  // This method is expensive, it should only be called for reporting.
  bool IsNested();

  // transition_info bitfields, for constructed array transition info.
  class ElementsKindBits:       public BitField<ElementsKind, 0,  15> {};
  class UnusedBits:             public BitField<int,          15, 14> {};
  class DoNotInlineBit:         public BitField<bool,         29,  1> {};

  // Bitfields for pretenure_data
  class MementoFoundCountBits:  public BitField<int,               0, 26> {};
  class PretenureDecisionBits:  public BitField<PretenureDecision, 26, 3> {};
  class DeoptDependentCodeBit:  public BitField<bool,              29, 1> {};
  STATIC_ASSERT(PretenureDecisionBits::kMax >= kLastPretenureDecisionValue);

  // Increments the mementos found counter and returns true when the first
  // memento was found for a given allocation site.
  inline bool IncrementMementoFoundCount(int increment = 1);

  inline void IncrementMementoCreateCount();

  PretenureFlag GetPretenureMode() const;

  void ResetPretenureDecision();

  inline PretenureDecision pretenure_decision() const;
  inline void set_pretenure_decision(PretenureDecision decision);

  inline bool deopt_dependent_code() const;
  inline void set_deopt_dependent_code(bool deopt);

  inline int memento_found_count() const;
  inline void set_memento_found_count(int count);

  inline int memento_create_count() const;
  inline void set_memento_create_count(int count);

  // The pretenuring decision is made during gc, and the zombie state allows
  // us to recognize when an allocation site is just being kept alive because
  // a later traversal of new space may discover AllocationMementos that point
  // to this AllocationSite.
  inline bool IsZombie() const;

  inline bool IsMaybeTenure() const;

  inline void MarkZombie();

  inline bool MakePretenureDecision(PretenureDecision current_decision,
                                    double ratio,
                                    bool maximum_size_scavenge);

  inline bool DigestPretenuringFeedback(bool maximum_size_scavenge);

  inline ElementsKind GetElementsKind() const;
  inline void SetElementsKind(ElementsKind kind);

  inline bool CanInlineCall() const;
  inline void SetDoNotInlineCall();

  inline bool PointsToLiteral() const;

  template <AllocationSiteUpdateMode update_or_check =
                AllocationSiteUpdateMode::kUpdate>
  static bool DigestTransitionFeedback(Handle<AllocationSite> site,
                                       ElementsKind to_kind);

  DECL_PRINTER(AllocationSite)
  DECL_VERIFIER(AllocationSite)

  DECL_CAST(AllocationSite)
  static inline bool ShouldTrack(ElementsKind boilerplate_elements_kind);
  static bool ShouldTrack(ElementsKind from, ElementsKind to);
  static inline bool CanTrack(InstanceType type);

// Layout description.
// AllocationSite has to start with TransitionInfoOrboilerPlateOffset
// and end with WeakNext field.
#define ALLOCATION_SITE_FIELDS(V)                     \
  V(kTransitionInfoOrBoilerplateOffset, kPointerSize) \
  V(kNestedSiteOffset, kPointerSize)                  \
  V(kDependentCodeOffset, kPointerSize)               \
  V(kCommonPointerFieldEndOffset, 0)                  \
  V(kPretenureDataOffset, kInt32Size)                 \
  V(kPretenureCreateCountOffset, kInt32Size)          \
  /* Size of AllocationSite without WeakNext field */ \
  V(kSizeWithoutWeakNext, 0)                          \
  V(kWeakNextOffset, kPointerSize)                    \
  /* Size of AllocationSite with WeakNext field */    \
  V(kSizeWithWeakNext, 0)

  DEFINE_FIELD_OFFSET_CONSTANTS(HeapObject::kHeaderSize, ALLOCATION_SITE_FIELDS)

  static const int kStartOffset = HeapObject::kHeaderSize;

  template <bool includeWeakNext>
  class BodyDescriptorImpl;

  // BodyDescriptor is used to traverse all the pointer fields including
  // weak_next
  typedef BodyDescriptorImpl<true> BodyDescriptor;

  // BodyDescriptorWeak is used to traverse all the pointer fields
  // except for weak_next
  typedef BodyDescriptorImpl<false> BodyDescriptorWeak;

 private:
  inline bool PretenuringDecisionMade() const;

  DISALLOW_IMPLICIT_CONSTRUCTORS(AllocationSite);
};


class AllocationMemento: public Struct {
 public:
  static const int kAllocationSiteOffset = HeapObject::kHeaderSize;
  static const int kSize = kAllocationSiteOffset + kPointerSize;

  DECL_ACCESSORS(allocation_site, Object)

  inline bool IsValid() const;
  inline AllocationSite* GetAllocationSite() const;
  inline Address GetAllocationSiteUnchecked() const;

  DECL_PRINTER(AllocationMemento)
  DECL_VERIFIER(AllocationMemento)

  DECL_CAST(AllocationMemento)

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(AllocationMemento);
};


// Utility superclass for stack-allocated objects that must be updated
// on gc.  It provides two ways for the gc to update instances, either
// iterating or updating after gc.
class Relocatable BASE_EMBEDDED {
 public:
  explicit inline Relocatable(Isolate* isolate);
  inline virtual ~Relocatable();
  virtual void IterateInstance(RootVisitor* v) {}
  virtual void PostGarbageCollection() { }

  static void PostGarbageCollectionProcessing(Isolate* isolate);
  static int ArchiveSpacePerThread();
  static char* ArchiveState(Isolate* isolate, char* to);
  static char* RestoreState(Isolate* isolate, char* from);
  static void Iterate(Isolate* isolate, RootVisitor* v);
  static void Iterate(RootVisitor* v, Relocatable* top);
  static char* Iterate(RootVisitor* v, char* t);

 private:
  Isolate* isolate_;
  Relocatable* prev_;
};


// The Oddball describes objects null, undefined, true, and false.
class Oddball: public HeapObject {
 public:
  // [to_number_raw]: Cached raw to_number computed at startup.
  inline double to_number_raw() const;
  inline void set_to_number_raw(double value);
  inline void set_to_number_raw_as_bits(uint64_t bits);

  // [to_string]: Cached to_string computed at startup.
  DECL_ACCESSORS(to_string, String)

  // [to_number]: Cached to_number computed at startup.
  DECL_ACCESSORS(to_number, Object)

  // [typeof]: Cached type_of computed at startup.
  DECL_ACCESSORS(type_of, String)

  inline byte kind() const;
  inline void set_kind(byte kind);

  // ES6 section 7.1.3 ToNumber for Boolean, Null, Undefined.
  V8_WARN_UNUSED_RESULT static inline Handle<Object> ToNumber(
      Isolate* isolate, Handle<Oddball> input);

  DECL_CAST(Oddball)

  // Dispatched behavior.
  DECL_VERIFIER(Oddball)

  // Initialize the fields.
  static void Initialize(Isolate* isolate, Handle<Oddball> oddball,
                         const char* to_string, Handle<Object> to_number,
                         const char* type_of, byte kind);

  // Layout description.
  static const int kToNumberRawOffset = HeapObject::kHeaderSize;
  static const int kToStringOffset = kToNumberRawOffset + kDoubleSize;
  static const int kToNumberOffset = kToStringOffset + kPointerSize;
  static const int kTypeOfOffset = kToNumberOffset + kPointerSize;
  static const int kKindOffset = kTypeOfOffset + kPointerSize;
  static const int kSize = kKindOffset + kPointerSize;

  static const byte kFalse = 0;
  static const byte kTrue = 1;
  static const byte kNotBooleanMask = static_cast<byte>(~1);
  static const byte kTheHole = 2;
  static const byte kNull = 3;
  static const byte kArgumentsMarker = 4;
  static const byte kUndefined = 5;
  static const byte kUninitialized = 6;
  static const byte kOther = 7;
  static const byte kException = 8;
  static const byte kOptimizedOut = 9;
  static const byte kStaleRegister = 10;
  static const byte kSelfReferenceMarker = 10;

  typedef FixedBodyDescriptor<kToStringOffset, kTypeOfOffset + kPointerSize,
                              kSize> BodyDescriptor;
  // No weak fields.
  typedef BodyDescriptor BodyDescriptorWeak;

  STATIC_ASSERT(kToNumberRawOffset == HeapNumber::kValueOffset);
  STATIC_ASSERT(kKindOffset == Internals::kOddballKindOffset);
  STATIC_ASSERT(kNull == Internals::kNullOddballKind);
  STATIC_ASSERT(kUndefined == Internals::kUndefinedOddballKind);

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(Oddball);
};


class Cell: public HeapObject {
 public:
  // [value]: value of the cell.
  DECL_ACCESSORS(value, Object)

  DECL_CAST(Cell)

  static inline Cell* FromValueAddress(Address value) {
    Object* result = FromAddress(value - kValueOffset);
    return static_cast<Cell*>(result);
  }

  inline Address ValueAddress() {
    return address() + kValueOffset;
  }

  // Dispatched behavior.
  DECL_PRINTER(Cell)
  DECL_VERIFIER(Cell)

  // Layout description.
  static const int kValueOffset = HeapObject::kHeaderSize;
  static const int kSize = kValueOffset + kPointerSize;

  typedef FixedBodyDescriptor<kValueOffset,
                              kValueOffset + kPointerSize,
                              kSize> BodyDescriptor;
  // No weak fields.
  typedef BodyDescriptor BodyDescriptorWeak;

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(Cell);
};

// This is a special cell used to maintain both the link between a
// closure and it's feedback vector, as well as a way to count the
// number of closures created for a certain function per native
// context. There's at most one FeedbackCell for each function in
// a native context.
class FeedbackCell : public Struct {
 public:
  // [value]: value of the cell.
  DECL_ACCESSORS(value, HeapObject)

  DECL_CAST(FeedbackCell)

  // Dispatched behavior.
  DECL_PRINTER(FeedbackCell)
  DECL_VERIFIER(FeedbackCell)

  static const int kValueOffset = HeapObject::kHeaderSize;
  static const int kSize = kValueOffset + kPointerSize;

  typedef FixedBodyDescriptor<kValueOffset, kValueOffset + kPointerSize, kSize>
      BodyDescriptor;
  // No weak fields.
  typedef BodyDescriptor BodyDescriptorWeak;

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(FeedbackCell);
};

class PropertyCell : public HeapObject {
 public:
  // [name]: the name of the global property.
  DECL_ACCESSORS(name, Name)
  // [property_details]: details of the global property.
  DECL_ACCESSORS(property_details_raw, Object)
  // [value]: value of the global property.
  DECL_ACCESSORS(value, Object)
  // [dependent_code]: dependent code that depends on the type of the global
  // property.
  DECL_ACCESSORS(dependent_code, DependentCode)

  inline PropertyDetails property_details() const;
  inline void set_property_details(PropertyDetails details);

  PropertyCellConstantType GetConstantType();

  // Computes the new type of the cell's contents for the given value, but
  // without actually modifying the details.
  static PropertyCellType UpdatedType(Isolate* isolate,
                                      Handle<PropertyCell> cell,
                                      Handle<Object> value,
                                      PropertyDetails details);
  // Prepares property cell at given entry for receiving given value.
  // As a result the old cell could be invalidated and/or dependent code could
  // be deoptimized. Returns the prepared property cell.
  static Handle<PropertyCell> PrepareForValue(
      Isolate* isolate, Handle<GlobalDictionary> dictionary, int entry,
      Handle<Object> value, PropertyDetails details);

  static Handle<PropertyCell> InvalidateEntry(
      Isolate* isolate, Handle<GlobalDictionary> dictionary, int entry);

  static void SetValueWithInvalidation(Isolate* isolate,
                                       Handle<PropertyCell> cell,
                                       Handle<Object> new_value);

  DECL_CAST(PropertyCell)

  // Dispatched behavior.
  DECL_PRINTER(PropertyCell)
  DECL_VERIFIER(PropertyCell)

  // Layout description.
  static const int kDetailsOffset = HeapObject::kHeaderSize;
  static const int kNameOffset = kDetailsOffset + kPointerSize;
  static const int kValueOffset = kNameOffset + kPointerSize;
  static const int kDependentCodeOffset = kValueOffset + kPointerSize;
  static const int kSize = kDependentCodeOffset + kPointerSize;

  typedef FixedBodyDescriptor<kNameOffset, kSize, kSize> BodyDescriptor;
  // No weak fields.
  typedef BodyDescriptor BodyDescriptorWeak;

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(PropertyCell);
};


class WeakCell : public HeapObject {
 public:
  inline Object* value() const;

  // This should not be called by anyone except GC.
  inline void clear();

  // This should not be called by anyone except allocator.
  inline void initialize(HeapObject* value);

  inline bool cleared() const;

  DECL_CAST(WeakCell)

  DECL_PRINTER(WeakCell)
  DECL_VERIFIER(WeakCell)

  // Layout description.
  static const int kValueOffset = HeapObject::kHeaderSize;
  static const int kSize = kValueOffset + kPointerSize;

  typedef FixedBodyDescriptor<kValueOffset, kSize, kSize> BodyDescriptor;

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(WeakCell);
};


// The JSProxy describes EcmaScript Harmony proxies
class JSProxy: public JSReceiver {
 public:
  V8_WARN_UNUSED_RESULT static MaybeHandle<JSProxy> New(Isolate* isolate,
                                                        Handle<Object>,
                                                        Handle<Object>);

  // [handler]: The handler property.
  DECL_ACCESSORS(handler, Object)
  // [target]: The target property.
  DECL_ACCESSORS(target, Object)

  static MaybeHandle<Context> GetFunctionRealm(Handle<JSProxy> proxy);

  DECL_CAST(JSProxy)

  V8_INLINE bool IsRevoked() const;
  static void Revoke(Handle<JSProxy> proxy);

  // ES6 9.5.1
  static MaybeHandle<Object> GetPrototype(Handle<JSProxy> receiver);

  // ES6 9.5.2
  V8_WARN_UNUSED_RESULT static Maybe<bool> SetPrototype(
      Handle<JSProxy> proxy, Handle<Object> value, bool from_javascript,
      ShouldThrow should_throw);
  // ES6 9.5.3
  V8_WARN_UNUSED_RESULT static Maybe<bool> IsExtensible(Handle<JSProxy> proxy);

  // ES6, #sec-isarray.  NOT to be confused with %_IsArray.
  V8_WARN_UNUSED_RESULT static Maybe<bool> IsArray(Handle<JSProxy> proxy);

  // ES6 9.5.4 (when passed kDontThrow)
  V8_WARN_UNUSED_RESULT static Maybe<bool> PreventExtensions(
      Handle<JSProxy> proxy, ShouldThrow should_throw);

  // ES6 9.5.5
  V8_WARN_UNUSED_RESULT static Maybe<bool> GetOwnPropertyDescriptor(
      Isolate* isolate, Handle<JSProxy> proxy, Handle<Name> name,
      PropertyDescriptor* desc);

  // ES6 9.5.6
  V8_WARN_UNUSED_RESULT static Maybe<bool> DefineOwnProperty(
      Isolate* isolate, Handle<JSProxy> object, Handle<Object> key,
      PropertyDescriptor* desc, ShouldThrow should_throw);

  // ES6 9.5.7
  V8_WARN_UNUSED_RESULT static Maybe<bool> HasProperty(Isolate* isolate,
                                                       Handle<JSProxy> proxy,
                                                       Handle<Name> name);

  // This function never returns false.
  // It returns either true or throws.
  V8_WARN_UNUSED_RESULT static Maybe<bool> CheckHasTrap(
      Isolate* isolate, Handle<Name> name, Handle<JSReceiver> target);

  // ES6 9.5.8
  V8_WARN_UNUSED_RESULT static MaybeHandle<Object> GetProperty(
      Isolate* isolate, Handle<JSProxy> proxy, Handle<Name> name,
      Handle<Object> receiver, bool* was_found);

  enum AccessKind { kGet, kSet };

  static MaybeHandle<Object> CheckGetSetTrapResult(Isolate* isolate,
                                                   Handle<Name> name,
                                                   Handle<JSReceiver> target,
                                                   Handle<Object> trap_result,
                                                   AccessKind access_kind);

  // ES6 9.5.9
  V8_WARN_UNUSED_RESULT static Maybe<bool> SetProperty(
      Handle<JSProxy> proxy, Handle<Name> name, Handle<Object> value,
      Handle<Object> receiver, LanguageMode language_mode);

  // ES6 9.5.10 (when passed LanguageMode::kSloppy)
  V8_WARN_UNUSED_RESULT static Maybe<bool> DeletePropertyOrElement(
      Handle<JSProxy> proxy, Handle<Name> name, LanguageMode language_mode);

  // ES6 9.5.12
  V8_WARN_UNUSED_RESULT static Maybe<bool> OwnPropertyKeys(
      Isolate* isolate, Handle<JSReceiver> receiver, Handle<JSProxy> proxy,
      PropertyFilter filter, KeyAccumulator* accumulator);

  V8_WARN_UNUSED_RESULT static Maybe<PropertyAttributes> GetPropertyAttributes(
      LookupIterator* it);

  // Dispatched behavior.
  DECL_PRINTER(JSProxy)
  DECL_VERIFIER(JSProxy)

  static const int kMaxIterationLimit = 100 * 1024;

  // Layout description.
  static const int kTargetOffset = JSReceiver::kHeaderSize;
  static const int kHandlerOffset = kTargetOffset + kPointerSize;
  static const int kSize = kHandlerOffset + kPointerSize;

  // kTargetOffset aliases with the elements of JSObject. The fact that
  // JSProxy::target is a Javascript value which cannot be confused with an
  // elements backing store is exploited by loading from this offset from an
  // unknown JSReceiver.
  STATIC_ASSERT(JSObject::kElementsOffset == JSProxy::kTargetOffset);

  typedef FixedBodyDescriptor<JSReceiver::kPropertiesOrHashOffset, kSize, kSize>
      BodyDescriptor;
  // No weak fields.
  typedef BodyDescriptor BodyDescriptorWeak;

  static Maybe<bool> SetPrivateSymbol(Isolate* isolate, Handle<JSProxy> proxy,
                                      Handle<Symbol> private_name,
                                      PropertyDescriptor* desc,
                                      ShouldThrow should_throw);

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(JSProxy);
};

// JSProxyRevocableResult is just a JSObject with a specific initial map.
// This initial map adds in-object properties for "proxy" and "revoke".
// See https://tc39.github.io/ecma262/#sec-proxy.revocable
class JSProxyRevocableResult : public JSObject {
 public:
  // Offsets of object fields.
  static const int kProxyOffset = JSObject::kHeaderSize;
  static const int kRevokeOffset = kProxyOffset + kPointerSize;
  static const int kSize = kRevokeOffset + kPointerSize;
  // Indices of in-object properties.
  static const int kProxyIndex = 0;
  static const int kRevokeIndex = 1;

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(JSProxyRevocableResult);
};

// The [Async-from-Sync Iterator] object
// (proposal-async-iteration/#sec-async-from-sync-iterator-objects)
// An object which wraps an ordinary Iterator and converts it to behave
// according to the Async Iterator protocol.
// (See https://tc39.github.io/proposal-async-iteration/#sec-iteration)
class JSAsyncFromSyncIterator : public JSObject {
 public:
  DECL_CAST(JSAsyncFromSyncIterator)
  DECL_PRINTER(JSAsyncFromSyncIterator)
  DECL_VERIFIER(JSAsyncFromSyncIterator)

  // Async-from-Sync Iterator instances are ordinary objects that inherit
  // properties from the %AsyncFromSyncIteratorPrototype% intrinsic object.
  // Async-from-Sync Iterator instances are initially created with the internal
  // slots listed in Table 4.
  // (proposal-async-iteration/#table-async-from-sync-iterator-internal-slots)
  DECL_ACCESSORS(sync_iterator, JSReceiver)

  // The "next" method is loaded during GetIterator, and is not reloaded for
  // subsequent "next" invocations.
  DECL_ACCESSORS(next, Object)

  // Offsets of object fields.
  static const int kSyncIteratorOffset = JSObject::kHeaderSize;
  static const int kNextOffset = kSyncIteratorOffset + kPointerSize;
  static const int kSize = kNextOffset + kPointerSize;

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(JSAsyncFromSyncIterator);
};

class JSStringIterator : public JSObject {
 public:
  // Dispatched behavior.
  DECL_PRINTER(JSStringIterator)
  DECL_VERIFIER(JSStringIterator)

  DECL_CAST(JSStringIterator)

  // [string]: the [[IteratedString]] inobject property.
  DECL_ACCESSORS(string, String)

  // [index]: The [[StringIteratorNextIndex]] inobject property.
  inline int index() const;
  inline void set_index(int value);

  static const int kStringOffset = JSObject::kHeaderSize;
  static const int kNextIndexOffset = kStringOffset + kPointerSize;
  static const int kSize = kNextIndexOffset + kPointerSize;

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(JSStringIterator);
};

// Foreign describes objects pointing from JavaScript to C structures.
class Foreign: public HeapObject {
 public:
  // [address]: field containing the address.
  inline Address foreign_address();

  static inline bool IsNormalized(Object* object);

  DECL_CAST(Foreign)

  // Dispatched behavior.
  DECL_PRINTER(Foreign)
  DECL_VERIFIER(Foreign)

  // Layout description.

  static const int kForeignAddressOffset = HeapObject::kHeaderSize;
  static const int kSize = kForeignAddressOffset + kPointerSize;

  STATIC_ASSERT(kForeignAddressOffset == Internals::kForeignAddressOffset);

  class BodyDescriptor;
  // No weak fields.
  typedef BodyDescriptor BodyDescriptorWeak;

 private:
  friend class Factory;
  friend class SerializerDeserializer;
  friend class StartupSerializer;

  inline void set_foreign_address(Address value);

  DISALLOW_IMPLICIT_CONSTRUCTORS(Foreign);
};

// Support for JavaScript accessors: A pair of a getter and a setter. Each
// accessor can either be
//   * a JavaScript function or proxy: a real accessor
//   * a FunctionTemplateInfo: a real (lazy) accessor
//   * undefined: considered an accessor by the spec, too, strangely enough
//   * null: an accessor which has not been set
class AccessorPair: public Struct {
 public:
  DECL_ACCESSORS(getter, Object)
  DECL_ACCESSORS(setter, Object)

  DECL_CAST(AccessorPair)

  static Handle<AccessorPair> Copy(Isolate* isolate, Handle<AccessorPair> pair);

  inline Object* get(AccessorComponent component);
  inline void set(AccessorComponent component, Object* value);

  // Note: Returns undefined if the component is not set.
  static Handle<Object> GetComponent(Isolate* isolate,
                                     Handle<AccessorPair> accessor_pair,
                                     AccessorComponent component);

  // Set both components, skipping arguments which are a JavaScript null.
  inline void SetComponents(Object* getter, Object* setter);

  inline bool Equals(AccessorPair* pair);
  inline bool Equals(Object* getter_value, Object* setter_value);

  inline bool ContainsAccessor();

  // Dispatched behavior.
  DECL_PRINTER(AccessorPair)
  DECL_VERIFIER(AccessorPair)

  static const int kGetterOffset = HeapObject::kHeaderSize;
  static const int kSetterOffset = kGetterOffset + kPointerSize;
  static const int kSize = kSetterOffset + kPointerSize;

 private:
  // Strangely enough, in addition to functions and harmony proxies, the spec
  // requires us to consider undefined as a kind of accessor, too:
  //    var obj = {};
  //    Object.defineProperty(obj, "foo", {get: undefined});
  //    assertTrue("foo" in obj);
  inline bool IsJSAccessor(Object* obj);

  DISALLOW_IMPLICIT_CONSTRUCTORS(AccessorPair);
};

class StackFrameInfo : public Struct, public NeverReadOnlySpaceObject {
 public:
  using NeverReadOnlySpaceObject::GetHeap;
  using NeverReadOnlySpaceObject::GetIsolate;

  DECL_INT_ACCESSORS(line_number)
  DECL_INT_ACCESSORS(column_number)
  DECL_INT_ACCESSORS(script_id)
  DECL_ACCESSORS(script_name, Object)
  DECL_ACCESSORS(script_name_or_source_url, Object)
  DECL_ACCESSORS(function_name, Object)
  DECL_BOOLEAN_ACCESSORS(is_eval)
  DECL_BOOLEAN_ACCESSORS(is_constructor)
  DECL_BOOLEAN_ACCESSORS(is_wasm)
  DECL_INT_ACCESSORS(flag)
  DECL_INT_ACCESSORS(id)

  DECL_CAST(StackFrameInfo)

  // Dispatched behavior.
  DECL_PRINTER(StackFrameInfo)
  DECL_VERIFIER(StackFrameInfo)

  static const int kLineNumberIndex = Struct::kHeaderSize;
  static const int kColumnNumberIndex = kLineNumberIndex + kPointerSize;
  static const int kScriptIdIndex = kColumnNumberIndex + kPointerSize;
  static const int kScriptNameIndex = kScriptIdIndex + kPointerSize;
  static const int kScriptNameOrSourceUrlIndex =
      kScriptNameIndex + kPointerSize;
  static const int kFunctionNameIndex =
      kScriptNameOrSourceUrlIndex + kPointerSize;
  static const int kFlagIndex = kFunctionNameIndex + kPointerSize;
  static const int kIdIndex = kFlagIndex + kPointerSize;
  static const int kSize = kIdIndex + kPointerSize;

 private:
  // Bit position in the flag, from least significant bit position.
  static const int kIsEvalBit = 0;
  static const int kIsConstructorBit = 1;
  static const int kIsWasmBit = 2;

  DISALLOW_IMPLICIT_CONSTRUCTORS(StackFrameInfo);
};

class SourcePositionTableWithFrameCache : public Tuple2 {
 public:
  DECL_ACCESSORS(source_position_table, ByteArray)
  DECL_ACCESSORS(stack_frame_cache, SimpleNumberDictionary)

  DECL_CAST(SourcePositionTableWithFrameCache)

  static const int kSourcePositionTableIndex = Struct::kHeaderSize;
  static const int kStackFrameCacheIndex =
      kSourcePositionTableIndex + kPointerSize;
  static const int kSize = kStackFrameCacheIndex + kPointerSize;

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(SourcePositionTableWithFrameCache);
};

// BooleanBit is a helper class for setting and getting a bit in an integer.
class BooleanBit : public AllStatic {
 public:
  static inline bool get(int value, int bit_position) {
    return (value & (1 << bit_position)) != 0;
  }

  static inline int set(int value, int bit_position, bool v) {
    if (v) {
      value |= (1 << bit_position);
    } else {
      value &= ~(1 << bit_position);
    }
    return value;
  }
};


}  // NOLINT, false-positive due to second-order macros.
}  // NOLINT, false-positive due to second-order macros.

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_H_
