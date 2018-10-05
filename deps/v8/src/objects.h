// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_H_
#define V8_OBJECTS_H_

#include <iosfwd>
#include <memory>

#include "include/v8-internal.h"
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
//         - JSV8BreakIterator     // If V8_INTL_SUPPORT enabled.
//         - JSCollator            // If V8_INTL_SUPPORT enabled.
//         - JSDateTimeFormat      // If V8_INTL_SUPPORT enabled.
//         - JSListFormat          // If V8_INTL_SUPPORT enabled.
//         - JSLocale              // If V8_INTL_SUPPORT enabled.
//         - JSNumberFormat        // If V8_INTL_SUPPORT enabled.
//         - JSPluralRules         // If V8_INTL_SUPPORT enabled.
//         - JSRelativeTimeFormat  // If V8_INTL_SUPPORT enabled.
//         - JSSegmenter           // If V8_INTL_SUPPORT enabled.
//         - WasmExceptionObject
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
//       - MicrotaskQueue
//       - Module
//       - ModuleInfoEntry
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
const uint32_t kUncachedExternalStringMask = 0x20;
const uint32_t kUncachedExternalStringTag = 0x20;

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
  UNCACHED_EXTERNAL_INTERNALIZED_STRING_TYPE =
      EXTERNAL_INTERNALIZED_STRING_TYPE | kUncachedExternalStringTag |
      kInternalizedTag,
  UNCACHED_EXTERNAL_ONE_BYTE_INTERNALIZED_STRING_TYPE =
      EXTERNAL_ONE_BYTE_INTERNALIZED_STRING_TYPE | kUncachedExternalStringTag |
      kInternalizedTag,
  UNCACHED_EXTERNAL_INTERNALIZED_STRING_WITH_ONE_BYTE_DATA_TYPE =
      EXTERNAL_INTERNALIZED_STRING_WITH_ONE_BYTE_DATA_TYPE |
      kUncachedExternalStringTag | kInternalizedTag,
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
  UNCACHED_EXTERNAL_STRING_TYPE =
      UNCACHED_EXTERNAL_INTERNALIZED_STRING_TYPE | kNotInternalizedTag,
  UNCACHED_EXTERNAL_ONE_BYTE_STRING_TYPE =
      UNCACHED_EXTERNAL_ONE_BYTE_INTERNALIZED_STRING_TYPE | kNotInternalizedTag,
  UNCACHED_EXTERNAL_STRING_WITH_ONE_BYTE_DATA_TYPE =
      UNCACHED_EXTERNAL_INTERNALIZED_STRING_WITH_ONE_BYTE_DATA_TYPE |
      kNotInternalizedTag,
  THIN_STRING_TYPE = kTwoByteStringTag | kThinStringTag | kNotInternalizedTag,
  THIN_ONE_BYTE_STRING_TYPE =
      kOneByteStringTag | kThinStringTag | kNotInternalizedTag,

  // Non-string names
  SYMBOL_TYPE =
      1 + (kIsNotInternalizedMask | kUncachedExternalStringMask |
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

  MICROTASK_QUEUE_TYPE,

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
  AWAIT_CONTEXT_TYPE,  // FIRST_CONTEXT_TYPE
  BLOCK_CONTEXT_TYPE,
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
  JS_INTL_V8_BREAK_ITERATOR_TYPE,
  JS_INTL_COLLATOR_TYPE,
  JS_INTL_DATE_TIME_FORMAT_TYPE,
  JS_INTL_LIST_FORMAT_TYPE,
  JS_INTL_LOCALE_TYPE,
  JS_INTL_NUMBER_FORMAT_TYPE,
  JS_INTL_PLURAL_RULES_TYPE,
  JS_INTL_RELATIVE_TIME_FORMAT_TYPE,
  JS_INTL_SEGMENTER_TYPE,
#endif  // V8_INTL_SUPPORT

  WASM_EXCEPTION_TYPE,
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
  FIRST_CONTEXT_TYPE = AWAIT_CONTEXT_TYPE,
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
class KeyAccumulator;
class LayoutDescriptor;
class LookupIterator;
class FieldType;
class MicrotaskQueue;
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
class TemplateInfo;
class TransitionArray;
class TemplateList;
template <typename T>
class ZoneForwardList;

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
  V(JSRegExpResult)                            \
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
  V(WasmExceptionObject)                       \
  V(WasmGlobalObject)                          \
  V(WasmInstanceObject)                        \
  V(WasmMemoryObject)                          \
  V(WasmModuleObject)                          \
  V(WasmTableObject)                           \
  V(WeakFixedArray)                            \
  V(WeakArrayList)

#ifdef V8_INTL_SUPPORT
#define HEAP_OBJECT_ORDINARY_TYPE_LIST(V) \
  HEAP_OBJECT_ORDINARY_TYPE_LIST_BASE(V)  \
  V(JSV8BreakIterator)                    \
  V(JSCollator)                           \
  V(JSDateTimeFormat)                     \
  V(JSListFormat)                         \
  V(JSLocale)                             \
  V(JSNumberFormat)                       \
  V(JSPluralRules)                        \
  V(JSRelativeTimeFormat)                 \
  V(JSSegmenter)
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

// List of object types that have a single unique instance type.
#define INSTANCE_TYPE_CHECKERS_SINGLE_BASE(V)                          \
  V(AllocationSite, ALLOCATION_SITE_TYPE)                              \
  V(BigInt, BIGINT_TYPE)                                               \
  V(ObjectBoilerplateDescription, OBJECT_BOILERPLATE_DESCRIPTION_TYPE) \
  V(BreakPoint, TUPLE2_TYPE)                                           \
  V(BreakPointInfo, TUPLE2_TYPE)                                       \
  V(ByteArray, BYTE_ARRAY_TYPE)                                        \
  V(BytecodeArray, BYTECODE_ARRAY_TYPE)                                \
  V(CallHandlerInfo, CALL_HANDLER_INFO_TYPE)                           \
  V(Cell, CELL_TYPE)                                                   \
  V(Code, CODE_TYPE)                                                   \
  V(CodeDataContainer, CODE_DATA_CONTAINER_TYPE)                       \
  V(CoverageInfo, FIXED_ARRAY_TYPE)                                    \
  V(DescriptorArray, DESCRIPTOR_ARRAY_TYPE)                            \
  V(EphemeronHashTable, EPHEMERON_HASH_TABLE_TYPE)                     \
  V(FeedbackCell, FEEDBACK_CELL_TYPE)                                  \
  V(FeedbackMetadata, FEEDBACK_METADATA_TYPE)                          \
  V(FeedbackVector, FEEDBACK_VECTOR_TYPE)                              \
  V(FixedArrayExact, FIXED_ARRAY_TYPE)                                 \
  V(FixedDoubleArray, FIXED_DOUBLE_ARRAY_TYPE)                         \
  V(Foreign, FOREIGN_TYPE)                                             \
  V(FreeSpace, FREE_SPACE_TYPE)                                        \
  V(GlobalDictionary, GLOBAL_DICTIONARY_TYPE)                          \
  V(HeapNumber, HEAP_NUMBER_TYPE)                                      \
  V(JSArgumentsObject, JS_ARGUMENTS_TYPE)                              \
  V(JSArray, JS_ARRAY_TYPE)                                            \
  V(JSArrayBuffer, JS_ARRAY_BUFFER_TYPE)                               \
  V(JSArrayIterator, JS_ARRAY_ITERATOR_TYPE)                           \
  V(JSAsyncFromSyncIterator, JS_ASYNC_FROM_SYNC_ITERATOR_TYPE)         \
  V(JSAsyncGeneratorObject, JS_ASYNC_GENERATOR_OBJECT_TYPE)            \
  V(JSBoundFunction, JS_BOUND_FUNCTION_TYPE)                           \
  V(JSContextExtensionObject, JS_CONTEXT_EXTENSION_OBJECT_TYPE)        \
  V(JSDataView, JS_DATA_VIEW_TYPE)                                     \
  V(JSDate, JS_DATE_TYPE)                                              \
  V(JSError, JS_ERROR_TYPE)                                            \
  V(JSFunction, JS_FUNCTION_TYPE)                                      \
  V(JSGlobalObject, JS_GLOBAL_OBJECT_TYPE)                             \
  V(JSGlobalProxy, JS_GLOBAL_PROXY_TYPE)                               \
  V(JSMap, JS_MAP_TYPE)                                                \
  V(JSMessageObject, JS_MESSAGE_OBJECT_TYPE)                           \
  V(JSModuleNamespace, JS_MODULE_NAMESPACE_TYPE)                       \
  V(JSPromise, JS_PROMISE_TYPE)                                        \
  V(JSProxy, JS_PROXY_TYPE)                                            \
  V(JSRegExp, JS_REGEXP_TYPE)                                          \
  V(JSRegExpResult, JS_ARRAY_TYPE)                                     \
  V(JSRegExpStringIterator, JS_REGEXP_STRING_ITERATOR_TYPE)            \
  V(JSSet, JS_SET_TYPE)                                                \
  V(JSStringIterator, JS_STRING_ITERATOR_TYPE)                         \
  V(JSTypedArray, JS_TYPED_ARRAY_TYPE)                                 \
  V(JSValue, JS_VALUE_TYPE)                                            \
  V(JSWeakMap, JS_WEAK_MAP_TYPE)                                       \
  V(JSWeakSet, JS_WEAK_SET_TYPE)                                       \
  V(LoadHandler, LOAD_HANDLER_TYPE)                                    \
  V(Map, MAP_TYPE)                                                     \
  V(MutableHeapNumber, MUTABLE_HEAP_NUMBER_TYPE)                       \
  V(NameDictionary, NAME_DICTIONARY_TYPE)                              \
  V(NativeContext, NATIVE_CONTEXT_TYPE)                                \
  V(NumberDictionary, NUMBER_DICTIONARY_TYPE)                          \
  V(Oddball, ODDBALL_TYPE)                                             \
  V(OrderedHashMap, ORDERED_HASH_MAP_TYPE)                             \
  V(OrderedHashSet, ORDERED_HASH_SET_TYPE)                             \
  V(PreParsedScopeData, PRE_PARSED_SCOPE_DATA_TYPE)                    \
  V(PropertyArray, PROPERTY_ARRAY_TYPE)                                \
  V(PropertyCell, PROPERTY_CELL_TYPE)                                  \
  V(PropertyDescriptorObject, FIXED_ARRAY_TYPE)                        \
  V(ScopeInfo, SCOPE_INFO_TYPE)                                        \
  V(ScriptContextTable, SCRIPT_CONTEXT_TABLE_TYPE)                     \
  V(SharedFunctionInfo, SHARED_FUNCTION_INFO_TYPE)                     \
  V(SimpleNumberDictionary, SIMPLE_NUMBER_DICTIONARY_TYPE)             \
  V(SmallOrderedHashMap, SMALL_ORDERED_HASH_MAP_TYPE)                  \
  V(SmallOrderedHashSet, SMALL_ORDERED_HASH_SET_TYPE)                  \
  V(SourcePositionTableWithFrameCache, TUPLE2_TYPE)                    \
  V(StoreHandler, STORE_HANDLER_TYPE)                                  \
  V(StringTable, STRING_TABLE_TYPE)                                    \
  V(Symbol, SYMBOL_TYPE)                                               \
  V(TemplateObjectDescription, TUPLE2_TYPE)                            \
  V(TransitionArray, TRANSITION_ARRAY_TYPE)                            \
  V(UncompiledDataWithoutPreParsedScope,                               \
    UNCOMPILED_DATA_WITHOUT_PRE_PARSED_SCOPE_TYPE)                     \
  V(UncompiledDataWithPreParsedScope,                                  \
    UNCOMPILED_DATA_WITH_PRE_PARSED_SCOPE_TYPE)                        \
  V(WasmExceptionObject, WASM_EXCEPTION_TYPE)                          \
  V(WasmGlobalObject, WASM_GLOBAL_TYPE)                                \
  V(WasmInstanceObject, WASM_INSTANCE_TYPE)                            \
  V(WasmMemoryObject, WASM_MEMORY_TYPE)                                \
  V(WasmModuleObject, WASM_MODULE_TYPE)                                \
  V(WasmTableObject, WASM_TABLE_TYPE)                                  \
  V(WeakArrayList, WEAK_ARRAY_LIST_TYPE)
#ifdef V8_INTL_SUPPORT

#define INSTANCE_TYPE_CHECKERS_SINGLE(V)                     \
  INSTANCE_TYPE_CHECKERS_SINGLE_BASE(V)                      \
  V(JSV8BreakIterator, JS_INTL_V8_BREAK_ITERATOR_TYPE)       \
  V(JSCollator, JS_INTL_COLLATOR_TYPE)                       \
  V(JSDateTimeFormat, JS_INTL_DATE_TIME_FORMAT_TYPE)         \
  V(JSListFormat, JS_INTL_LIST_FORMAT_TYPE)                  \
  V(JSLocale, JS_INTL_LOCALE_TYPE)                           \
  V(JSNumberFormat, JS_INTL_NUMBER_FORMAT_TYPE)              \
  V(JSPluralRules, JS_INTL_PLURAL_RULES_TYPE)                \
  V(JSRelativeTimeFormat, JS_INTL_RELATIVE_TIME_FORMAT_TYPE) \
  V(JSSegmenter, JS_INTL_SEGMENTER_TYPE)

#else

#define INSTANCE_TYPE_CHECKERS_SINGLE(V) INSTANCE_TYPE_CHECKERS_SINGLE_BASE(V)

#endif  // V8_INTL_SUPPORT

#define INSTANCE_TYPE_CHECKERS_RANGE(V)                             \
  V(Context, FIRST_CONTEXT_TYPE, LAST_CONTEXT_TYPE)                 \
  V(Dictionary, FIRST_DICTIONARY_TYPE, LAST_DICTIONARY_TYPE)        \
  V(FixedArray, FIRST_FIXED_ARRAY_TYPE, LAST_FIXED_ARRAY_TYPE)      \
  V(FixedTypedArrayBase, FIRST_FIXED_TYPED_ARRAY_TYPE,              \
    LAST_FIXED_TYPED_ARRAY_TYPE)                                    \
  V(HashTable, FIRST_HASH_TABLE_TYPE, LAST_HASH_TABLE_TYPE)         \
  V(JSMapIterator, FIRST_MAP_ITERATOR_TYPE, LAST_MAP_ITERATOR_TYPE) \
  V(JSSetIterator, FIRST_SET_ITERATOR_TYPE, LAST_SET_ITERATOR_TYPE) \
  V(Microtask, FIRST_MICROTASK_TYPE, LAST_MICROTASK_TYPE)           \
  V(Name, FIRST_TYPE, LAST_NAME_TYPE)                               \
  V(String, FIRST_TYPE, FIRST_NONSTRING_TYPE - 1)                   \
  V(WeakFixedArray, FIRST_WEAK_FIXED_ARRAY_TYPE, LAST_WEAK_FIXED_ARRAY_TYPE)

#define INSTANCE_TYPE_CHECKERS_CUSTOM(V) \
  V(FixedArrayBase)                      \
  V(InternalizedString)                  \
  V(JSObject)                            \
  V(JSReceiver)

#define INSTANCE_TYPE_CHECKERS(V)  \
  INSTANCE_TYPE_CHECKERS_SINGLE(V) \
  INSTANCE_TYPE_CHECKERS_RANGE(V)  \
  INSTANCE_TYPE_CHECKERS_CUSTOM(V)

namespace InstanceTypeChecker {
#define IS_TYPE_FUNCTION_DECL(Type, ...) \
  V8_INLINE bool Is##Type(InstanceType instance_type);

INSTANCE_TYPE_CHECKERS(IS_TYPE_FUNCTION_DECL)

#define TYPED_ARRAY_IS_TYPE_FUNCTION_DECL(Type, ...) \
  IS_TYPE_FUNCTION_DECL(Fixed##Type##Array)
TYPED_ARRAYS(TYPED_ARRAY_IS_TYPE_FUNCTION_DECL)
#undef TYPED_ARRAY_IS_TYPE_FUNCTION_DECL

#define STRUCT_IS_TYPE_FUNCTION_DECL(NAME, Name, name) \
  IS_TYPE_FUNCTION_DECL(Name)
STRUCT_LIST(STRUCT_IS_TYPE_FUNCTION_DECL)
#undef STRUCT_IS_TYPE_FUNCTION_DECL

#undef IS_TYPE_FUNCTION_DECL
}  // namespace InstanceTypeChecker

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

#define MAYBE_ASSIGN_RETURN_FAILURE_ON_EXCEPTION(isolate, dst, call) \
  do {                                                               \
    Isolate* __isolate__ = (isolate);                                \
    if (!(call).To(&dst)) {                                          \
      DCHECK(__isolate__->has_pending_exception());                  \
      return ReadOnlyRoots(__isolate__).exception();                 \
    }                                                                \
  } while (false)

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
      Isolate* isolate, Handle<JSReceiver> object);

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
      StoreOrigin store_origin);
  V8_WARN_UNUSED_RESULT static MaybeHandle<Object> SetProperty(
      Isolate* isolate, Handle<Object> object, Handle<Name> name,
      Handle<Object> value, LanguageMode language_mode,
      StoreOrigin store_origin = StoreOrigin::kMaybeKeyed);
  V8_WARN_UNUSED_RESULT static inline MaybeHandle<Object> SetPropertyOrElement(
      Isolate* isolate, Handle<Object> object, Handle<Name> name,
      Handle<Object> value, LanguageMode language_mode,
      StoreOrigin store_origin = StoreOrigin::kMaybeKeyed);

  V8_WARN_UNUSED_RESULT static Maybe<bool> SetSuperProperty(
      LookupIterator* it, Handle<Object> value, LanguageMode language_mode,
      StoreOrigin store_origin);

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
      ShouldThrow should_throw, StoreOrigin store_origin);
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
      StoreOrigin store_origin, bool* found);

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
  V8_EXPORT_PRIVATE explicit Brief(const Object* v);
  explicit Brief(const MaybeObject* v) : value(v) {}
  const MaybeObject* value;
};

V8_EXPORT_PRIVATE std::ostream& operator<<(std::ostream& os, const Brief& v);

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

  // Compare two Smis x, y as if they were converted to strings and then
  // compared lexicographically. Returns:
  // -1 if x < y.
  //  0 if x == y.
  //  1 if x > y.
  static Smi* LexicographicCompare(Isolate* isolate, Smi* x, Smi* y);

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
class MapWord {
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

template <class ParentBodyDescriptor, class ChildBodyDescriptor>
class SubclassBodyDescriptor;

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

// Utility superclass for stack-allocated objects that must be updated
// on gc.  It provides two ways for the gc to update instances, either
// iterating or updating after gc.
class Relocatable {
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

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(PropertyCell);
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
