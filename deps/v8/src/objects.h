// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_H_
#define V8_OBJECTS_H_

#include <iosfwd>
#include <memory>

#include "src/assert-scope.h"
#include "src/bailout-reason.h"
#include "src/base/bits.h"
#include "src/base/flags.h"
#include "src/builtins/builtins-definitions.h"
#include "src/checks.h"
#include "src/elements-kind.h"
#include "src/field-index.h"
#include "src/flags.h"
#include "src/list.h"
#include "src/messages.h"
#include "src/property-details.h"
#include "src/unicode-decoder.h"
#include "src/unicode.h"

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
//           - CodeCacheHashTable
//           - MapCache
//         - OrderedHashTable
//           - OrderedHashSet
//           - OrderedHashMap
//         - Context
//         - FeedbackMetadata
//         - FeedbackVector
//         - TemplateList
//         - TransitionArray
//         - ScopeInfo
//         - ModuleInfo
//         - ScriptContextTable
//         - WeakFixedArray
//         - WasmSharedModuleData
//         - WasmCompiledModule
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
//       - PromiseResolveThenableJobInfo
//       - PromiseReactionJobInfo
//       - AccessorPair
//       - AccessCheckInfo
//       - InterceptorInfo
//       - CallHandlerInfo
//       - TemplateInfo
//         - FunctionTemplateInfo
//         - ObjectTemplateInfo
//       - Script
//       - DebugInfo
//       - BreakPointInfo
//       - StackFrameInfo
//       - SourcePositionTableWithFrameCache
//       - CodeCache
//       - PrototypeInfo
//       - Module
//       - ModuleInfoEntry
//       - PreParsedScopeData
//     - WeakCell
//
// Formats of Object*:
//  Smi:        [31 bit signed int] 0
//  HeapObject: [32 bit direct pointer] (4 byte aligned) | 01

namespace v8 {
namespace internal {

struct InliningPosition;

enum KeyedAccessStoreMode {
  STANDARD_STORE,
  STORE_TRANSITION_TO_OBJECT,
  STORE_TRANSITION_TO_DOUBLE,
  STORE_AND_GROW_NO_TRANSITION,
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


static inline KeyedAccessStoreMode GetNonTransitioningStoreMode(
    KeyedAccessStoreMode store_mode) {
  if (store_mode >= STORE_NO_TRANSITION_IGNORE_OUT_OF_BOUNDS) {
    return store_mode;
  }
  if (store_mode >= STORE_AND_GROW_NO_TRANSITION) {
    return STORE_AND_GROW_NO_TRANSITION;
  }
  return STANDARD_STORE;
}


static inline bool IsGrowStoreMode(KeyedAccessStoreMode store_mode) {
  return store_mode >= STORE_AND_GROW_NO_TRANSITION &&
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

// ICs store extra state in a Code object. The default extra state is
// kNoExtraICState.
typedef int ExtraICState;
static const ExtraICState kNoExtraICState = 0;

// Instance size sentinel for objects of variable size.
const int kVariableSizeSentinel = 0;

// We may store the unsigned bit field as signed Smi value and do not
// use the sign bit.
const int kStubMajorKeyBits = 8;
const int kStubMinorKeyBits = kSmiValueSize - kStubMajorKeyBits - 1;

// All Maps have a field instance_type containing a InstanceType.
// It describes the type of the instances.
//
// As an example, a JavaScript object is a heap object and its map
// instance_type is JS_OBJECT_TYPE.
//
// The names of the string instance types are intended to systematically
// mirror their encoding in the instance_type field of the map.  The default
// encoding is considered TWO_BYTE.  It is not mentioned in the name.  ONE_BYTE
// encoding is mentioned explicitly in the name.  Likewise, the default
// representation is considered sequential.  It is not mentioned in the
// name.  The other representations (e.g. CONS, EXTERNAL) are explicitly
// mentioned.  Finally, the string is either a STRING_TYPE (if it is a normal
// string) or a INTERNALIZED_STRING_TYPE (if it is a internalized string).
//
// NOTE: The following things are some that depend on the string types having
// instance_types that are less than those of all other types:
// HeapObject::Size, HeapObject::IterateBody, the typeof operator, and
// Object::IsString.
//
// NOTE: Everything following JS_VALUE_TYPE is considered a
// JSObject for GC purposes. The first four entries here have typeof
// 'object', whereas JS_FUNCTION_TYPE has typeof 'function'.
#define INSTANCE_TYPE_LIST(V)                                                  \
  V(INTERNALIZED_STRING_TYPE)                                                  \
  V(EXTERNAL_INTERNALIZED_STRING_TYPE)                                         \
  V(ONE_BYTE_INTERNALIZED_STRING_TYPE)                                         \
  V(EXTERNAL_ONE_BYTE_INTERNALIZED_STRING_TYPE)                                \
  V(EXTERNAL_INTERNALIZED_STRING_WITH_ONE_BYTE_DATA_TYPE)                      \
  V(SHORT_EXTERNAL_INTERNALIZED_STRING_TYPE)                                   \
  V(SHORT_EXTERNAL_ONE_BYTE_INTERNALIZED_STRING_TYPE)                          \
  V(SHORT_EXTERNAL_INTERNALIZED_STRING_WITH_ONE_BYTE_DATA_TYPE)                \
  V(STRING_TYPE)                                                               \
  V(CONS_STRING_TYPE)                                                          \
  V(EXTERNAL_STRING_TYPE)                                                      \
  V(SLICED_STRING_TYPE)                                                        \
  V(THIN_STRING_TYPE)                                                          \
  V(ONE_BYTE_STRING_TYPE)                                                      \
  V(CONS_ONE_BYTE_STRING_TYPE)                                                 \
  V(EXTERNAL_ONE_BYTE_STRING_TYPE)                                             \
  V(SLICED_ONE_BYTE_STRING_TYPE)                                               \
  V(THIN_ONE_BYTE_STRING_TYPE)                                                 \
  V(EXTERNAL_STRING_WITH_ONE_BYTE_DATA_TYPE)                                   \
  V(SHORT_EXTERNAL_STRING_TYPE)                                                \
  V(SHORT_EXTERNAL_ONE_BYTE_STRING_TYPE)                                       \
  V(SHORT_EXTERNAL_STRING_WITH_ONE_BYTE_DATA_TYPE)                             \
                                                                               \
  V(SYMBOL_TYPE)                                                               \
  V(HEAP_NUMBER_TYPE)                                                          \
  V(ODDBALL_TYPE)                                                              \
                                                                               \
  V(MAP_TYPE)                                                                  \
  V(CODE_TYPE)                                                                 \
  V(MUTABLE_HEAP_NUMBER_TYPE)                                                  \
  V(FOREIGN_TYPE)                                                              \
  V(BYTE_ARRAY_TYPE)                                                           \
  V(BYTECODE_ARRAY_TYPE)                                                       \
  V(FREE_SPACE_TYPE)                                                           \
                                                                               \
  V(FIXED_INT8_ARRAY_TYPE)                                                     \
  V(FIXED_UINT8_ARRAY_TYPE)                                                    \
  V(FIXED_INT16_ARRAY_TYPE)                                                    \
  V(FIXED_UINT16_ARRAY_TYPE)                                                   \
  V(FIXED_INT32_ARRAY_TYPE)                                                    \
  V(FIXED_UINT32_ARRAY_TYPE)                                                   \
  V(FIXED_FLOAT32_ARRAY_TYPE)                                                  \
  V(FIXED_FLOAT64_ARRAY_TYPE)                                                  \
  V(FIXED_UINT8_CLAMPED_ARRAY_TYPE)                                            \
                                                                               \
  V(FIXED_DOUBLE_ARRAY_TYPE)                                                   \
  V(FILLER_TYPE)                                                               \
                                                                               \
  V(ACCESSOR_INFO_TYPE)                                                        \
  V(ACCESSOR_PAIR_TYPE)                                                        \
  V(ACCESS_CHECK_INFO_TYPE)                                                    \
  V(INTERCEPTOR_INFO_TYPE)                                                     \
  V(FUNCTION_TEMPLATE_INFO_TYPE)                                               \
  V(OBJECT_TEMPLATE_INFO_TYPE)                                                 \
  V(ALLOCATION_SITE_TYPE)                                                      \
  V(ALLOCATION_MEMENTO_TYPE)                                                   \
  V(SCRIPT_TYPE)                                                               \
  V(ALIASED_ARGUMENTS_ENTRY_TYPE)                                              \
  V(PROMISE_RESOLVE_THENABLE_JOB_INFO_TYPE)                                    \
  V(PROMISE_REACTION_JOB_INFO_TYPE)                                            \
  V(DEBUG_INFO_TYPE)                                                           \
  V(STACK_FRAME_INFO_TYPE)                                                     \
  V(PROTOTYPE_INFO_TYPE)                                                       \
  V(TUPLE2_TYPE)                                                               \
  V(TUPLE3_TYPE)                                                               \
  V(CONTEXT_EXTENSION_TYPE)                                                    \
  V(MODULE_TYPE)                                                               \
  V(MODULE_INFO_ENTRY_TYPE)                                                    \
  V(ASYNC_GENERATOR_REQUEST_TYPE)                                              \
  V(PREPARSED_SCOPE_DATA_TYPE)                                                 \
  V(FIXED_ARRAY_TYPE)                                                          \
  V(PROPERTY_ARRAY_TYPE)                                                       \
  V(TRANSITION_ARRAY_TYPE)                                                     \
  V(SHARED_FUNCTION_INFO_TYPE)                                                 \
  V(CELL_TYPE)                                                                 \
  V(WEAK_CELL_TYPE)                                                            \
  V(PROPERTY_CELL_TYPE)                                                        \
  V(SMALL_ORDERED_HASH_MAP_TYPE)                                               \
  V(SMALL_ORDERED_HASH_SET_TYPE)                                               \
                                                                               \
  V(JS_PROXY_TYPE)                                                             \
  V(JS_GLOBAL_OBJECT_TYPE)                                                     \
  V(JS_GLOBAL_PROXY_TYPE)                                                      \
  V(JS_SPECIAL_API_OBJECT_TYPE)                                                \
  V(JS_VALUE_TYPE)                                                             \
  V(JS_MESSAGE_OBJECT_TYPE)                                                    \
  V(JS_DATE_TYPE)                                                              \
  V(JS_API_OBJECT_TYPE)                                                        \
  V(JS_OBJECT_TYPE)                                                            \
  V(JS_ARGUMENTS_TYPE)                                                         \
  V(JS_CONTEXT_EXTENSION_OBJECT_TYPE)                                          \
  V(JS_GENERATOR_OBJECT_TYPE)                                                  \
  V(JS_ASYNC_GENERATOR_OBJECT_TYPE)                                            \
  V(JS_MODULE_NAMESPACE_TYPE)                                                  \
  V(JS_ARRAY_TYPE)                                                             \
  V(JS_ARRAY_BUFFER_TYPE)                                                      \
  V(JS_TYPED_ARRAY_TYPE)                                                       \
  V(JS_DATA_VIEW_TYPE)                                                         \
  V(JS_SET_TYPE)                                                               \
  V(JS_MAP_TYPE)                                                               \
  V(JS_SET_KEY_VALUE_ITERATOR_TYPE)                                            \
  V(JS_SET_VALUE_ITERATOR_TYPE)                                                \
  V(JS_MAP_KEY_ITERATOR_TYPE)                                                  \
  V(JS_MAP_KEY_VALUE_ITERATOR_TYPE)                                            \
  V(JS_MAP_VALUE_ITERATOR_TYPE)                                                \
  V(JS_WEAK_MAP_TYPE)                                                          \
  V(JS_WEAK_SET_TYPE)                                                          \
  V(JS_PROMISE_CAPABILITY_TYPE)                                                \
  V(JS_PROMISE_TYPE)                                                           \
  V(JS_REGEXP_TYPE)                                                            \
  V(JS_ERROR_TYPE)                                                             \
  V(JS_ASYNC_FROM_SYNC_ITERATOR_TYPE)                                          \
  V(JS_STRING_ITERATOR_TYPE)                                                   \
                                                                               \
  V(JS_TYPED_ARRAY_KEY_ITERATOR_TYPE)                                          \
  V(JS_FAST_ARRAY_KEY_ITERATOR_TYPE)                                           \
  V(JS_GENERIC_ARRAY_KEY_ITERATOR_TYPE)                                        \
                                                                               \
  V(JS_UINT8_ARRAY_KEY_VALUE_ITERATOR_TYPE)                                    \
  V(JS_INT8_ARRAY_KEY_VALUE_ITERATOR_TYPE)                                     \
  V(JS_UINT16_ARRAY_KEY_VALUE_ITERATOR_TYPE)                                   \
  V(JS_INT16_ARRAY_KEY_VALUE_ITERATOR_TYPE)                                    \
  V(JS_UINT32_ARRAY_KEY_VALUE_ITERATOR_TYPE)                                   \
  V(JS_INT32_ARRAY_KEY_VALUE_ITERATOR_TYPE)                                    \
  V(JS_FLOAT32_ARRAY_KEY_VALUE_ITERATOR_TYPE)                                  \
  V(JS_FLOAT64_ARRAY_KEY_VALUE_ITERATOR_TYPE)                                  \
  V(JS_UINT8_CLAMPED_ARRAY_KEY_VALUE_ITERATOR_TYPE)                            \
                                                                               \
  V(JS_FAST_SMI_ARRAY_KEY_VALUE_ITERATOR_TYPE)                                 \
  V(JS_FAST_HOLEY_SMI_ARRAY_KEY_VALUE_ITERATOR_TYPE)                           \
  V(JS_FAST_ARRAY_KEY_VALUE_ITERATOR_TYPE)                                     \
  V(JS_FAST_HOLEY_ARRAY_KEY_VALUE_ITERATOR_TYPE)                               \
  V(JS_FAST_DOUBLE_ARRAY_KEY_VALUE_ITERATOR_TYPE)                              \
  V(JS_FAST_HOLEY_DOUBLE_ARRAY_KEY_VALUE_ITERATOR_TYPE)                        \
  V(JS_GENERIC_ARRAY_KEY_VALUE_ITERATOR_TYPE)                                  \
                                                                               \
  V(JS_UINT8_ARRAY_VALUE_ITERATOR_TYPE)                                        \
  V(JS_INT8_ARRAY_VALUE_ITERATOR_TYPE)                                         \
  V(JS_UINT16_ARRAY_VALUE_ITERATOR_TYPE)                                       \
  V(JS_INT16_ARRAY_VALUE_ITERATOR_TYPE)                                        \
  V(JS_UINT32_ARRAY_VALUE_ITERATOR_TYPE)                                       \
  V(JS_INT32_ARRAY_VALUE_ITERATOR_TYPE)                                        \
  V(JS_FLOAT32_ARRAY_VALUE_ITERATOR_TYPE)                                      \
  V(JS_FLOAT64_ARRAY_VALUE_ITERATOR_TYPE)                                      \
  V(JS_UINT8_CLAMPED_ARRAY_VALUE_ITERATOR_TYPE)                                \
                                                                               \
  V(JS_FAST_SMI_ARRAY_VALUE_ITERATOR_TYPE)                                     \
  V(JS_FAST_HOLEY_SMI_ARRAY_VALUE_ITERATOR_TYPE)                               \
  V(JS_FAST_ARRAY_VALUE_ITERATOR_TYPE)                                         \
  V(JS_FAST_HOLEY_ARRAY_VALUE_ITERATOR_TYPE)                                   \
  V(JS_FAST_DOUBLE_ARRAY_VALUE_ITERATOR_TYPE)                                  \
  V(JS_FAST_HOLEY_DOUBLE_ARRAY_VALUE_ITERATOR_TYPE)                            \
  V(JS_GENERIC_ARRAY_VALUE_ITERATOR_TYPE)                                      \
                                                                               \
  V(WASM_INSTANCE_TYPE)                                                        \
  V(WASM_MEMORY_TYPE)                                                          \
  V(WASM_MODULE_TYPE)                                                          \
  V(WASM_TABLE_TYPE)                                                           \
  V(JS_BOUND_FUNCTION_TYPE)                                                    \
  V(JS_FUNCTION_TYPE)

// Since string types are not consecutive, this macro is used to
// iterate over them.
#define STRING_TYPE_LIST(V)                                                   \
  V(STRING_TYPE, kVariableSizeSentinel, string, String)                       \
  V(ONE_BYTE_STRING_TYPE, kVariableSizeSentinel, one_byte_string,             \
    OneByteString)                                                            \
  V(CONS_STRING_TYPE, ConsString::kSize, cons_string, ConsString)             \
  V(CONS_ONE_BYTE_STRING_TYPE, ConsString::kSize, cons_one_byte_string,       \
    ConsOneByteString)                                                        \
  V(SLICED_STRING_TYPE, SlicedString::kSize, sliced_string, SlicedString)     \
  V(SLICED_ONE_BYTE_STRING_TYPE, SlicedString::kSize, sliced_one_byte_string, \
    SlicedOneByteString)                                                      \
  V(EXTERNAL_STRING_TYPE, ExternalTwoByteString::kSize, external_string,      \
    ExternalString)                                                           \
  V(EXTERNAL_ONE_BYTE_STRING_TYPE, ExternalOneByteString::kSize,              \
    external_one_byte_string, ExternalOneByteString)                          \
  V(EXTERNAL_STRING_WITH_ONE_BYTE_DATA_TYPE, ExternalTwoByteString::kSize,    \
    external_string_with_one_byte_data, ExternalStringWithOneByteData)        \
  V(SHORT_EXTERNAL_STRING_TYPE, ExternalTwoByteString::kShortSize,            \
    short_external_string, ShortExternalString)                               \
  V(SHORT_EXTERNAL_ONE_BYTE_STRING_TYPE, ExternalOneByteString::kShortSize,   \
    short_external_one_byte_string, ShortExternalOneByteString)               \
  V(SHORT_EXTERNAL_STRING_WITH_ONE_BYTE_DATA_TYPE,                            \
    ExternalTwoByteString::kShortSize,                                        \
    short_external_string_with_one_byte_data,                                 \
    ShortExternalStringWithOneByteData)                                       \
                                                                              \
  V(INTERNALIZED_STRING_TYPE, kVariableSizeSentinel, internalized_string,     \
    InternalizedString)                                                       \
  V(ONE_BYTE_INTERNALIZED_STRING_TYPE, kVariableSizeSentinel,                 \
    one_byte_internalized_string, OneByteInternalizedString)                  \
  V(EXTERNAL_INTERNALIZED_STRING_TYPE, ExternalTwoByteString::kSize,          \
    external_internalized_string, ExternalInternalizedString)                 \
  V(EXTERNAL_ONE_BYTE_INTERNALIZED_STRING_TYPE, ExternalOneByteString::kSize, \
    external_one_byte_internalized_string, ExternalOneByteInternalizedString) \
  V(EXTERNAL_INTERNALIZED_STRING_WITH_ONE_BYTE_DATA_TYPE,                     \
    ExternalTwoByteString::kSize,                                             \
    external_internalized_string_with_one_byte_data,                          \
    ExternalInternalizedStringWithOneByteData)                                \
  V(SHORT_EXTERNAL_INTERNALIZED_STRING_TYPE,                                  \
    ExternalTwoByteString::kShortSize, short_external_internalized_string,    \
    ShortExternalInternalizedString)                                          \
  V(SHORT_EXTERNAL_ONE_BYTE_INTERNALIZED_STRING_TYPE,                         \
    ExternalOneByteString::kShortSize,                                        \
    short_external_one_byte_internalized_string,                              \
    ShortExternalOneByteInternalizedString)                                   \
  V(SHORT_EXTERNAL_INTERNALIZED_STRING_WITH_ONE_BYTE_DATA_TYPE,               \
    ExternalTwoByteString::kShortSize,                                        \
    short_external_internalized_string_with_one_byte_data,                    \
    ShortExternalInternalizedStringWithOneByteData)                           \
  V(THIN_STRING_TYPE, ThinString::kSize, thin_string, ThinString)             \
  V(THIN_ONE_BYTE_STRING_TYPE, ThinString::kSize, thin_one_byte_string,       \
    ThinOneByteString)

// A struct is a simple object a set of object-valued fields.  Including an
// object type in this causes the compiler to generate most of the boilerplate
// code for the class including allocation and garbage collection routines,
// casts and predicates.  All you need to define is the class, methods and
// object verification routines.  Easy, no?
//
// Note that for subtle reasons related to the ordering or numerical values of
// type tags, elements in this list have to be added to the INSTANCE_TYPE_LIST
// manually.
#define STRUCT_LIST(V)                                                       \
  V(ACCESSOR_INFO, AccessorInfo, accessor_info)                              \
  V(ACCESSOR_PAIR, AccessorPair, accessor_pair)                              \
  V(ACCESS_CHECK_INFO, AccessCheckInfo, access_check_info)                   \
  V(INTERCEPTOR_INFO, InterceptorInfo, interceptor_info)                     \
  V(FUNCTION_TEMPLATE_INFO, FunctionTemplateInfo, function_template_info)    \
  V(OBJECT_TEMPLATE_INFO, ObjectTemplateInfo, object_template_info)          \
  V(ALLOCATION_SITE, AllocationSite, allocation_site)                        \
  V(ALLOCATION_MEMENTO, AllocationMemento, allocation_memento)               \
  V(SCRIPT, Script, script)                                                  \
  V(ALIASED_ARGUMENTS_ENTRY, AliasedArgumentsEntry, aliased_arguments_entry) \
  V(PROMISE_RESOLVE_THENABLE_JOB_INFO, PromiseResolveThenableJobInfo,        \
    promise_resolve_thenable_job_info)                                       \
  V(PROMISE_REACTION_JOB_INFO, PromiseReactionJobInfo,                       \
    promise_reaction_job_info)                                               \
  V(DEBUG_INFO, DebugInfo, debug_info)                                       \
  V(STACK_FRAME_INFO, StackFrameInfo, stack_frame_info)                      \
  V(PROTOTYPE_INFO, PrototypeInfo, prototype_info)                           \
  V(TUPLE2, Tuple2, tuple2)                                                  \
  V(TUPLE3, Tuple3, tuple3)                                                  \
  V(CONTEXT_EXTENSION, ContextExtension, context_extension)                  \
  V(MODULE, Module, module)                                                  \
  V(MODULE_INFO_ENTRY, ModuleInfoEntry, module_info_entry)                   \
  V(ASYNC_GENERATOR_REQUEST, AsyncGeneratorRequest, async_generator_request) \
  V(PREPARSED_SCOPE_DATA, PreParsedScopeData, preparsed_scope_data)

// We use the full 8 bits of the instance_type field to encode heap object
// instance types.  The high-order bit (bit 7) is set if the object is not a
// string, and cleared if it is a string.
const uint32_t kIsNotStringMask = 0x80;
const uint32_t kStringTag = 0x0;
const uint32_t kNotStringTag = 0x80;

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

enum InstanceType : uint8_t {
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
  SYMBOL_TYPE = kNotStringTag,  // FIRST_NONSTRING_TYPE, LAST_NAME_TYPE

  // Other primitives (cannot contain non-map-word pointers to heap objects).
  HEAP_NUMBER_TYPE,
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
  FIXED_UINT8_CLAMPED_ARRAY_TYPE,  // LAST_FIXED_TYPED_ARRAY_TYPE
  FIXED_DOUBLE_ARRAY_TYPE,
  FILLER_TYPE,  // LAST_DATA_TYPE

  // Structs.
  ACCESSOR_INFO_TYPE,
  ACCESSOR_PAIR_TYPE,
  ACCESS_CHECK_INFO_TYPE,
  INTERCEPTOR_INFO_TYPE,
  FUNCTION_TEMPLATE_INFO_TYPE,
  OBJECT_TEMPLATE_INFO_TYPE,
  ALLOCATION_SITE_TYPE,
  ALLOCATION_MEMENTO_TYPE,
  SCRIPT_TYPE,
  ALIASED_ARGUMENTS_ENTRY_TYPE,
  PROMISE_RESOLVE_THENABLE_JOB_INFO_TYPE,
  PROMISE_REACTION_JOB_INFO_TYPE,
  DEBUG_INFO_TYPE,
  STACK_FRAME_INFO_TYPE,
  PROTOTYPE_INFO_TYPE,
  TUPLE2_TYPE,
  TUPLE3_TYPE,
  CONTEXT_EXTENSION_TYPE,
  MODULE_TYPE,
  MODULE_INFO_ENTRY_TYPE,
  ASYNC_GENERATOR_REQUEST_TYPE,
  PREPARSED_SCOPE_DATA_TYPE,
  FIXED_ARRAY_TYPE,
  PROPERTY_ARRAY_TYPE,
  TRANSITION_ARRAY_TYPE,
  SHARED_FUNCTION_INFO_TYPE,
  CELL_TYPE,
  WEAK_CELL_TYPE,
  PROPERTY_CELL_TYPE,
  SMALL_ORDERED_HASH_MAP_TYPE,
  SMALL_ORDERED_HASH_SET_TYPE,

  // All the following types are subtypes of JSReceiver, which corresponds to
  // objects in the JS sense. The first and the last type in this range are
  // the two forms of function. This organization enables using the same
  // compares for checking the JS_RECEIVER and the NONCALLABLE_JS_OBJECT range.
  JS_PROXY_TYPE,          // FIRST_JS_RECEIVER_TYPE
  JS_GLOBAL_OBJECT_TYPE,  // FIRST_JS_OBJECT_TYPE
  JS_GLOBAL_PROXY_TYPE,
  // Like JS_API_OBJECT_TYPE, but requires access checks and/or has
  // interceptors.
  JS_SPECIAL_API_OBJECT_TYPE,  // LAST_SPECIAL_RECEIVER_TYPE
  JS_VALUE_TYPE,               // LAST_CUSTOM_ELEMENTS_RECEIVER
  JS_MESSAGE_OBJECT_TYPE,
  JS_DATE_TYPE,
  // Like JS_OBJECT_TYPE, but created from API function.
  JS_API_OBJECT_TYPE,
  JS_OBJECT_TYPE,
  JS_ARGUMENTS_TYPE,
  JS_CONTEXT_EXTENSION_OBJECT_TYPE,
  JS_GENERATOR_OBJECT_TYPE,
  JS_ASYNC_GENERATOR_OBJECT_TYPE,
  JS_MODULE_NAMESPACE_TYPE,
  JS_ARRAY_TYPE,
  JS_ARRAY_BUFFER_TYPE,
  JS_TYPED_ARRAY_TYPE,
  JS_DATA_VIEW_TYPE,
  JS_SET_TYPE,
  JS_MAP_TYPE,
  JS_SET_KEY_VALUE_ITERATOR_TYPE,
  JS_SET_VALUE_ITERATOR_TYPE,
  JS_MAP_KEY_ITERATOR_TYPE,
  JS_MAP_KEY_VALUE_ITERATOR_TYPE,
  JS_MAP_VALUE_ITERATOR_TYPE,
  JS_WEAK_MAP_TYPE,
  JS_WEAK_SET_TYPE,
  JS_PROMISE_CAPABILITY_TYPE,
  JS_PROMISE_TYPE,
  JS_REGEXP_TYPE,
  JS_ERROR_TYPE,
  JS_ASYNC_FROM_SYNC_ITERATOR_TYPE,
  JS_STRING_ITERATOR_TYPE,

  JS_TYPED_ARRAY_KEY_ITERATOR_TYPE,
  JS_FAST_ARRAY_KEY_ITERATOR_TYPE,
  JS_GENERIC_ARRAY_KEY_ITERATOR_TYPE,

  JS_UINT8_ARRAY_KEY_VALUE_ITERATOR_TYPE,
  JS_INT8_ARRAY_KEY_VALUE_ITERATOR_TYPE,
  JS_UINT16_ARRAY_KEY_VALUE_ITERATOR_TYPE,
  JS_INT16_ARRAY_KEY_VALUE_ITERATOR_TYPE,
  JS_UINT32_ARRAY_KEY_VALUE_ITERATOR_TYPE,
  JS_INT32_ARRAY_KEY_VALUE_ITERATOR_TYPE,
  JS_FLOAT32_ARRAY_KEY_VALUE_ITERATOR_TYPE,
  JS_FLOAT64_ARRAY_KEY_VALUE_ITERATOR_TYPE,
  JS_UINT8_CLAMPED_ARRAY_KEY_VALUE_ITERATOR_TYPE,

  JS_FAST_SMI_ARRAY_KEY_VALUE_ITERATOR_TYPE,
  JS_FAST_HOLEY_SMI_ARRAY_KEY_VALUE_ITERATOR_TYPE,
  JS_FAST_ARRAY_KEY_VALUE_ITERATOR_TYPE,
  JS_FAST_HOLEY_ARRAY_KEY_VALUE_ITERATOR_TYPE,
  JS_FAST_DOUBLE_ARRAY_KEY_VALUE_ITERATOR_TYPE,
  JS_FAST_HOLEY_DOUBLE_ARRAY_KEY_VALUE_ITERATOR_TYPE,
  JS_GENERIC_ARRAY_KEY_VALUE_ITERATOR_TYPE,

  JS_UINT8_ARRAY_VALUE_ITERATOR_TYPE,
  JS_INT8_ARRAY_VALUE_ITERATOR_TYPE,
  JS_UINT16_ARRAY_VALUE_ITERATOR_TYPE,
  JS_INT16_ARRAY_VALUE_ITERATOR_TYPE,
  JS_UINT32_ARRAY_VALUE_ITERATOR_TYPE,
  JS_INT32_ARRAY_VALUE_ITERATOR_TYPE,
  JS_FLOAT32_ARRAY_VALUE_ITERATOR_TYPE,
  JS_FLOAT64_ARRAY_VALUE_ITERATOR_TYPE,
  JS_UINT8_CLAMPED_ARRAY_VALUE_ITERATOR_TYPE,

  JS_FAST_SMI_ARRAY_VALUE_ITERATOR_TYPE,
  JS_FAST_HOLEY_SMI_ARRAY_VALUE_ITERATOR_TYPE,
  JS_FAST_ARRAY_VALUE_ITERATOR_TYPE,
  JS_FAST_HOLEY_ARRAY_VALUE_ITERATOR_TYPE,
  JS_FAST_DOUBLE_ARRAY_VALUE_ITERATOR_TYPE,
  JS_FAST_HOLEY_DOUBLE_ARRAY_VALUE_ITERATOR_TYPE,
  JS_GENERIC_ARRAY_VALUE_ITERATOR_TYPE,

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
  // Boundaries for testing for a fixed typed array.
  FIRST_FIXED_TYPED_ARRAY_TYPE = FIXED_INT8_ARRAY_TYPE,
  LAST_FIXED_TYPED_ARRAY_TYPE = FIXED_UINT8_CLAMPED_ARRAY_TYPE,
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

  FIRST_ARRAY_KEY_ITERATOR_TYPE = JS_TYPED_ARRAY_KEY_ITERATOR_TYPE,
  LAST_ARRAY_KEY_ITERATOR_TYPE = JS_GENERIC_ARRAY_KEY_ITERATOR_TYPE,

  FIRST_ARRAY_KEY_VALUE_ITERATOR_TYPE = JS_UINT8_ARRAY_KEY_VALUE_ITERATOR_TYPE,
  LAST_ARRAY_KEY_VALUE_ITERATOR_TYPE = JS_GENERIC_ARRAY_KEY_VALUE_ITERATOR_TYPE,

  FIRST_ARRAY_VALUE_ITERATOR_TYPE = JS_UINT8_ARRAY_VALUE_ITERATOR_TYPE,
  LAST_ARRAY_VALUE_ITERATOR_TYPE = JS_GENERIC_ARRAY_VALUE_ITERATOR_TYPE,

  FIRST_ARRAY_ITERATOR_TYPE = FIRST_ARRAY_KEY_ITERATOR_TYPE,
  LAST_ARRAY_ITERATOR_TYPE = LAST_ARRAY_VALUE_ITERATOR_TYPE,

  FIRST_SET_ITERATOR_TYPE = JS_SET_KEY_VALUE_ITERATOR_TYPE,
  LAST_SET_ITERATOR_TYPE = JS_SET_VALUE_ITERATOR_TYPE,

  FIRST_MAP_ITERATOR_TYPE = JS_MAP_KEY_ITERATOR_TYPE,
  LAST_MAP_ITERATOR_TYPE = JS_MAP_VALUE_ITERATOR_TYPE,
};

STATIC_ASSERT(JS_OBJECT_TYPE == Internals::kJSObjectType);
STATIC_ASSERT(JS_API_OBJECT_TYPE == Internals::kJSApiObjectType);
STATIC_ASSERT(FIRST_NONSTRING_TYPE == Internals::kFirstNonstringType);
STATIC_ASSERT(ODDBALL_TYPE == Internals::kOddballType);
STATIC_ASSERT(FOREIGN_TYPE == Internals::kForeignType);

V8_EXPORT_PRIVATE std::ostream& operator<<(std::ostream& os,
                                           InstanceType instance_type);

#define FIXED_ARRAY_SUB_INSTANCE_TYPE_LIST(V)    \
  V(BYTECODE_ARRAY_CONSTANT_POOL_SUB_TYPE)       \
  V(BYTECODE_ARRAY_HANDLER_TABLE_SUB_TYPE)       \
  V(CODE_STUBS_TABLE_SUB_TYPE)                   \
  V(COMPILATION_CACHE_TABLE_SUB_TYPE)            \
  V(CONTEXT_SUB_TYPE)                            \
  V(COPY_ON_WRITE_SUB_TYPE)                      \
  V(DEOPTIMIZATION_DATA_SUB_TYPE)                \
  V(DESCRIPTOR_ARRAY_SUB_TYPE)                   \
  V(EMBEDDED_OBJECT_SUB_TYPE)                    \
  V(ENUM_CACHE_SUB_TYPE)                         \
  V(ENUM_INDICES_CACHE_SUB_TYPE)                 \
  V(DEPENDENT_CODE_SUB_TYPE)                     \
  V(DICTIONARY_ELEMENTS_SUB_TYPE)                \
  V(DICTIONARY_PROPERTIES_SUB_TYPE)              \
  V(EMPTY_PROPERTIES_DICTIONARY_SUB_TYPE)        \
  V(PACKED_ELEMENTS_SUB_TYPE)                    \
  V(FAST_PROPERTIES_SUB_TYPE)                    \
  V(FAST_TEMPLATE_INSTANTIATIONS_CACHE_SUB_TYPE) \
  V(HANDLER_TABLE_SUB_TYPE)                      \
  V(JS_COLLECTION_SUB_TYPE)                      \
  V(JS_WEAK_COLLECTION_SUB_TYPE)                 \
  V(MAP_CODE_CACHE_SUB_TYPE)                     \
  V(NOSCRIPT_SHARED_FUNCTION_INFOS_SUB_TYPE)     \
  V(NUMBER_STRING_CACHE_SUB_TYPE)                \
  V(OBJECT_TO_CODE_SUB_TYPE)                     \
  V(OPTIMIZED_CODE_LITERALS_SUB_TYPE)            \
  V(OPTIMIZED_CODE_MAP_SUB_TYPE)                 \
  V(PROTOTYPE_USERS_SUB_TYPE)                    \
  V(REGEXP_MULTIPLE_CACHE_SUB_TYPE)              \
  V(RETAINED_MAPS_SUB_TYPE)                      \
  V(SCOPE_INFO_SUB_TYPE)                         \
  V(SCRIPT_LIST_SUB_TYPE)                        \
  V(SERIALIZED_TEMPLATES_SUB_TYPE)               \
  V(SHARED_FUNCTION_INFOS_SUB_TYPE)              \
  V(SINGLE_CHARACTER_STRING_CACHE_SUB_TYPE)      \
  V(SLOW_TEMPLATE_INSTANTIATIONS_CACHE_SUB_TYPE) \
  V(STRING_SPLIT_CACHE_SUB_TYPE)                 \
  V(STRING_TABLE_SUB_TYPE)                       \
  V(TEMPLATE_INFO_SUB_TYPE)                      \
  V(FEEDBACK_VECTOR_SUB_TYPE)                    \
  V(FEEDBACK_METADATA_SUB_TYPE)                  \
  V(WEAK_NEW_SPACE_OBJECT_TO_CODE_SUB_TYPE)

enum FixedArraySubInstanceType {
#define DEFINE_FIXED_ARRAY_SUB_INSTANCE_TYPE(name) name,
  FIXED_ARRAY_SUB_INSTANCE_TYPE_LIST(DEFINE_FIXED_ARRAY_SUB_INSTANCE_TYPE)
#undef DEFINE_FIXED_ARRAY_SUB_INSTANCE_TYPE
      LAST_FIXED_ARRAY_SUB_TYPE = WEAK_NEW_SPACE_OBJECT_TO_CODE_SUB_TYPE
};


// TODO(bmeurer): Remove this in favor of the ComparisonResult below.
enum CompareResult {
  LESS      = -1,
  EQUAL     =  0,
  GREATER   =  1,

  NOT_EQUAL = GREATER
};


// Result of an abstract relational comparison of x and y, implemented according
// to ES6 section 7.2.11 Abstract Relational Comparison.
enum class ComparisonResult {
  kLessThan,     // x < y
  kEqual,        // x = y
  kGreaterThan,  // x > y
  kUndefined     // at least one of x or y was undefined or NaN
};


class AbstractCode;
class AccessorPair;
class AllocationSite;
class Cell;
class ConsString;
class ElementsAccessor;
class FindAndReplacePattern;
class FixedArrayBase;
class PropertyArray;
class FunctionLiteral;
class JSGlobalObject;
class KeyAccumulator;
class LayoutDescriptor;
class LookupIterator;
class FieldType;
class Module;
class ModuleDescriptor;
class ModuleInfoEntry;
class ModuleInfo;
class ObjectHashTable;
class ObjectVisitor;
class PropertyCell;
class PropertyDescriptor;
class RootVisitor;
class SafepointEntry;
class SharedFunctionInfo;
class StringStream;
class TypeFeedbackInfo;
class FeedbackMetadata;
class FeedbackVector;
class WeakCell;
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
  V(Number)

#define HEAP_OBJECT_TYPE_LIST(V)       \
  V(AbstractCode)                      \
  V(AccessCheckNeeded)                 \
  V(ArrayList)                         \
  V(BoilerplateDescription)            \
  V(Boolean)                           \
  V(BreakPointInfo)                    \
  V(ByteArray)                         \
  V(BytecodeArray)                     \
  V(Callable)                          \
  V(CallHandlerInfo)                   \
  V(Cell)                              \
  V(Code)                              \
  V(CodeCacheHashTable)                \
  V(CompilationCacheTable)             \
  V(ConsString)                        \
  V(ConstantElementsPair)              \
  V(Constructor)                       \
  V(Context)                           \
  V(CoverageInfo)                      \
  V(DeoptimizationInputData)           \
  V(DependentCode)                     \
  V(DescriptorArray)                   \
  V(Dictionary)                        \
  V(External)                          \
  V(ExternalOneByteString)             \
  V(ExternalString)                    \
  V(ExternalTwoByteString)             \
  V(FeedbackMetadata)                  \
  V(FeedbackVector)                    \
  V(Filler)                            \
  V(FixedArray)                        \
  V(FixedArrayBase)                    \
  V(FixedDoubleArray)                  \
  V(FixedFloat32Array)                 \
  V(FixedFloat64Array)                 \
  V(FixedInt16Array)                   \
  V(FixedInt32Array)                   \
  V(FixedInt8Array)                    \
  V(FixedTypedArrayBase)               \
  V(FixedUint16Array)                  \
  V(FixedUint32Array)                  \
  V(FixedUint8Array)                   \
  V(FixedUint8ClampedArray)            \
  V(Foreign)                           \
  V(FrameArray)                        \
  V(FreeSpace)                         \
  V(Function)                          \
  V(HandlerTable)                      \
  V(HashTable)                         \
  V(HeapNumber)                        \
  V(InternalizedString)                \
  V(JSArgumentsObject)                 \
  V(JSArray)                           \
  V(JSArrayBuffer)                     \
  V(JSArrayBufferView)                 \
  V(JSArrayIterator)                   \
  V(JSAsyncFromSyncIterator)           \
  V(JSAsyncGeneratorObject)            \
  V(JSBoundFunction)                   \
  V(JSCollection)                      \
  V(JSContextExtensionObject)          \
  V(JSDataView)                        \
  V(JSDate)                            \
  V(JSError)                           \
  V(JSFunction)                        \
  V(JSGeneratorObject)                 \
  V(JSGlobalObject)                    \
  V(JSGlobalProxy)                     \
  V(JSMap)                             \
  V(JSMapIterator)                     \
  V(JSMessageObject)                   \
  V(JSModuleNamespace)                 \
  V(WasmInstanceObject)                \
  V(WasmMemoryObject)                  \
  V(WasmModuleObject)                  \
  V(WasmTableObject)                   \
  V(JSObject)                          \
  V(JSPromise)                         \
  V(JSPromiseCapability)               \
  V(JSProxy)                           \
  V(JSReceiver)                        \
  V(JSRegExp)                          \
  V(JSSet)                             \
  V(JSSetIterator)                     \
  V(JSSloppyArgumentsObject)           \
  V(JSStringIterator)                  \
  V(JSTypedArray)                      \
  V(JSValue)                           \
  V(JSWeakCollection)                  \
  V(JSWeakMap)                         \
  V(JSWeakSet)                         \
  V(Map)                               \
  V(MapCache)                          \
  V(ModuleInfo)                        \
  V(MutableHeapNumber)                 \
  V(Name)                              \
  V(NativeContext)                     \
  V(NormalizedMapCache)                \
  V(ObjectHashSet)                     \
  V(ObjectHashTable)                   \
  V(Oddball)                           \
  V(OrderedHashTable)                  \
  V(PropertyArray)                     \
  V(PropertyCell)                      \
  V(RegExpMatchInfo)                   \
  V(ScopeInfo)                         \
  V(ScriptContextTable)                \
  V(SeqOneByteString)                  \
  V(SeqString)                         \
  V(SeqTwoByteString)                  \
  V(SharedFunctionInfo)                \
  V(SlicedString)                      \
  V(SloppyArgumentsElements)           \
  V(SmallOrderedHashMap)               \
  V(SmallOrderedHashSet)               \
  V(SourcePositionTableWithFrameCache) \
  V(String)                            \
  V(StringSet)                         \
  V(StringTable)                       \
  V(StringWrapper)                     \
  V(Struct)                            \
  V(Symbol)                            \
  V(TemplateInfo)                      \
  V(TemplateList)                      \
  V(ThinString)                        \
  V(TransitionArray)                   \
  V(TypeFeedbackInfo)                  \
  V(Undetectable)                      \
  V(UniqueName)                        \
  V(UnseededNumberDictionary)          \
  V(WeakCell)                          \
  V(WeakFixedArray)                    \
  V(WeakHashTable)

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

#define IS_TYPE_FUNCTION_DECL(Type) INLINE(bool Is##Type() const);
  OBJECT_TYPE_LIST(IS_TYPE_FUNCTION_DECL)
  HEAP_OBJECT_TYPE_LIST(IS_TYPE_FUNCTION_DECL)
#undef IS_TYPE_FUNCTION_DECL

#define IS_TYPE_FUNCTION_DECL(Type, Value) \
  INLINE(bool Is##Type(Isolate* isolate) const);
  ODDBALL_LIST(IS_TYPE_FUNCTION_DECL)
#undef IS_TYPE_FUNCTION_DECL

  INLINE(bool IsNullOrUndefined(Isolate* isolate) const);

  // A non-keyed store is of the form a.x = foo or a["x"] = foo whereas
  // a keyed store is of the form a[expression] = foo.
  enum StoreFromKeyed {
    MAY_BE_STORE_FROM_KEYED,
    CERTAINLY_NOT_STORE_FROM_KEYED
  };

  enum ShouldThrow { THROW_ON_ERROR, DONT_THROW };

#define RETURN_FAILURE(isolate, should_throw, call) \
  do {                                              \
    if ((should_throw) == DONT_THROW) {             \
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

#define DECL_STRUCT_PREDICATE(NAME, Name, name) INLINE(bool Is##Name() const);
  STRUCT_LIST(DECL_STRUCT_PREDICATE)
#undef DECL_STRUCT_PREDICATE

  // ES6, #sec-isarray.  NOT to be confused with %_IsArray.
  INLINE(MUST_USE_RESULT static Maybe<bool> IsArray(Handle<Object> object));

  INLINE(bool IsNameDictionary() const);
  INLINE(bool IsGlobalDictionary() const);
  INLINE(bool IsSeededNumberDictionary() const);
  INLINE(bool IsOrderedHashSet() const);
  INLINE(bool IsOrderedHashMap() const);
  INLINE(bool IsSmallOrderedHashTable() const);

  // Extract the number.
  inline double Number() const;
  INLINE(bool IsNaN() const);
  INLINE(bool IsMinusZero() const);
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

  inline bool HasSpecificClassOf(String* name);

  bool BooleanValue();                                      // ECMA-262 9.2.

  // ES6 section 7.2.11 Abstract Relational Comparison
  MUST_USE_RESULT static Maybe<ComparisonResult> Compare(Handle<Object> x,
                                                         Handle<Object> y);

  // ES6 section 7.2.12 Abstract Equality Comparison
  MUST_USE_RESULT static Maybe<bool> Equals(Handle<Object> x, Handle<Object> y);

  // ES6 section 7.2.13 Strict Equality Comparison
  bool StrictEquals(Object* that);

  // ES6 section 7.1.13 ToObject
  // Convert to a JSObject if needed.
  // native_context is used when creating wrapper object.
  //
  // Passing a non-null method_name allows us to give a more informative
  // error message for those cases where ToObject is being called on
  // the receiver of a built-in method.
  MUST_USE_RESULT static inline MaybeHandle<JSReceiver> ToObject(
      Isolate* isolate, Handle<Object> object,
      const char* method_name = nullptr);
  MUST_USE_RESULT static MaybeHandle<JSReceiver> ToObject(
      Isolate* isolate, Handle<Object> object, Handle<Context> native_context,
      const char* method_name = nullptr);

  // ES6 section 9.2.1.2, OrdinaryCallBindThis for sloppy callee.
  MUST_USE_RESULT static MaybeHandle<JSReceiver> ConvertReceiver(
      Isolate* isolate, Handle<Object> object);

  // ES6 section 7.1.14 ToPropertyKey
  MUST_USE_RESULT static inline MaybeHandle<Name> ToName(Isolate* isolate,
                                                         Handle<Object> input);

  // ES6 section 7.1.1 ToPrimitive
  MUST_USE_RESULT static inline MaybeHandle<Object> ToPrimitive(
      Handle<Object> input, ToPrimitiveHint hint = ToPrimitiveHint::kDefault);

  // ES6 section 7.1.3 ToNumber
  MUST_USE_RESULT static inline MaybeHandle<Object> ToNumber(
      Handle<Object> input);

  // ES6 section 7.1.4 ToInteger
  MUST_USE_RESULT static inline MaybeHandle<Object> ToInteger(
      Isolate* isolate, Handle<Object> input);

  // ES6 section 7.1.5 ToInt32
  MUST_USE_RESULT static inline MaybeHandle<Object> ToInt32(
      Isolate* isolate, Handle<Object> input);

  // ES6 section 7.1.6 ToUint32
  MUST_USE_RESULT inline static MaybeHandle<Object> ToUint32(
      Isolate* isolate, Handle<Object> input);

  // ES6 section 7.1.12 ToString
  MUST_USE_RESULT static inline MaybeHandle<String> ToString(
      Isolate* isolate, Handle<Object> input);

  static Handle<String> NoSideEffectsToString(Isolate* isolate,
                                              Handle<Object> input);

  // ES6 section 7.1.14 ToPropertyKey
  MUST_USE_RESULT static inline MaybeHandle<Object> ToPropertyKey(
      Isolate* isolate, Handle<Object> value);

  // ES6 section 7.1.15 ToLength
  MUST_USE_RESULT static inline MaybeHandle<Object> ToLength(
      Isolate* isolate, Handle<Object> input);

  // ES6 section 7.1.17 ToIndex
  MUST_USE_RESULT static inline MaybeHandle<Object> ToIndex(
      Isolate* isolate, Handle<Object> input,
      MessageTemplate::Template error_index);

  // ES6 section 7.3.9 GetMethod
  MUST_USE_RESULT static MaybeHandle<Object> GetMethod(
      Handle<JSReceiver> receiver, Handle<Name> name);

  // ES6 section 7.3.17 CreateListFromArrayLike
  MUST_USE_RESULT static MaybeHandle<FixedArray> CreateListFromArrayLike(
      Isolate* isolate, Handle<Object> object, ElementTypes element_types);

  // Get length property and apply ToLength.
  MUST_USE_RESULT static MaybeHandle<Object> GetLengthFromArrayLike(
      Isolate* isolate, Handle<Object> object);

  // ES6 section 12.5.6 The typeof Operator
  static Handle<String> TypeOf(Isolate* isolate, Handle<Object> object);

  // ES6 section 12.6 Multiplicative Operators
  MUST_USE_RESULT static MaybeHandle<Object> Multiply(Isolate* isolate,
                                                      Handle<Object> lhs,
                                                      Handle<Object> rhs);
  MUST_USE_RESULT static MaybeHandle<Object> Divide(Isolate* isolate,
                                                    Handle<Object> lhs,
                                                    Handle<Object> rhs);
  MUST_USE_RESULT static MaybeHandle<Object> Modulus(Isolate* isolate,
                                                     Handle<Object> lhs,
                                                     Handle<Object> rhs);

  // ES6 section 12.7 Additive Operators
  MUST_USE_RESULT static MaybeHandle<Object> Add(Isolate* isolate,
                                                 Handle<Object> lhs,
                                                 Handle<Object> rhs);
  MUST_USE_RESULT static MaybeHandle<Object> Subtract(Isolate* isolate,
                                                      Handle<Object> lhs,
                                                      Handle<Object> rhs);

  // ES6 section 12.8 Bitwise Shift Operators
  MUST_USE_RESULT static MaybeHandle<Object> ShiftLeft(Isolate* isolate,
                                                       Handle<Object> lhs,
                                                       Handle<Object> rhs);
  MUST_USE_RESULT static MaybeHandle<Object> ShiftRight(Isolate* isolate,
                                                        Handle<Object> lhs,
                                                        Handle<Object> rhs);
  MUST_USE_RESULT static MaybeHandle<Object> ShiftRightLogical(
      Isolate* isolate, Handle<Object> lhs, Handle<Object> rhs);

  // ES6 section 12.9 Relational Operators
  MUST_USE_RESULT static inline Maybe<bool> GreaterThan(Handle<Object> x,
                                                        Handle<Object> y);
  MUST_USE_RESULT static inline Maybe<bool> GreaterThanOrEqual(
      Handle<Object> x, Handle<Object> y);
  MUST_USE_RESULT static inline Maybe<bool> LessThan(Handle<Object> x,
                                                     Handle<Object> y);
  MUST_USE_RESULT static inline Maybe<bool> LessThanOrEqual(Handle<Object> x,
                                                            Handle<Object> y);

  // ES6 section 12.11 Binary Bitwise Operators
  MUST_USE_RESULT static MaybeHandle<Object> BitwiseAnd(Isolate* isolate,
                                                        Handle<Object> lhs,
                                                        Handle<Object> rhs);
  MUST_USE_RESULT static MaybeHandle<Object> BitwiseOr(Isolate* isolate,
                                                       Handle<Object> lhs,
                                                       Handle<Object> rhs);
  MUST_USE_RESULT static MaybeHandle<Object> BitwiseXor(Isolate* isolate,
                                                        Handle<Object> lhs,
                                                        Handle<Object> rhs);

  // ES6 section 7.3.19 OrdinaryHasInstance (C, O).
  MUST_USE_RESULT static MaybeHandle<Object> OrdinaryHasInstance(
      Isolate* isolate, Handle<Object> callable, Handle<Object> object);

  // ES6 section 12.10.4 Runtime Semantics: InstanceofOperator(O, C)
  MUST_USE_RESULT static MaybeHandle<Object> InstanceOf(
      Isolate* isolate, Handle<Object> object, Handle<Object> callable);

  V8_EXPORT_PRIVATE MUST_USE_RESULT static MaybeHandle<Object> GetProperty(
      LookupIterator* it);

  // ES6 [[Set]] (when passed DONT_THROW)
  // Invariants for this and related functions (unless stated otherwise):
  // 1) When the result is Nothing, an exception is pending.
  // 2) When passed THROW_ON_ERROR, the result is never Just(false).
  // In some cases, an exception is thrown regardless of the ShouldThrow
  // argument.  These cases are either in accordance with the spec or not
  // covered by it (eg., concerning API callbacks).
  MUST_USE_RESULT static Maybe<bool> SetProperty(LookupIterator* it,
                                                 Handle<Object> value,
                                                 LanguageMode language_mode,
                                                 StoreFromKeyed store_mode);
  MUST_USE_RESULT static MaybeHandle<Object> SetProperty(
      Handle<Object> object, Handle<Name> name, Handle<Object> value,
      LanguageMode language_mode,
      StoreFromKeyed store_mode = MAY_BE_STORE_FROM_KEYED);
  MUST_USE_RESULT static inline MaybeHandle<Object> SetPropertyOrElement(
      Handle<Object> object, Handle<Name> name, Handle<Object> value,
      LanguageMode language_mode,
      StoreFromKeyed store_mode = MAY_BE_STORE_FROM_KEYED);

  MUST_USE_RESULT static Maybe<bool> SetSuperProperty(
      LookupIterator* it, Handle<Object> value, LanguageMode language_mode,
      StoreFromKeyed store_mode);

  MUST_USE_RESULT static Maybe<bool> CannotCreateProperty(
      Isolate* isolate, Handle<Object> receiver, Handle<Object> name,
      Handle<Object> value, ShouldThrow should_throw);
  MUST_USE_RESULT static Maybe<bool> WriteToReadOnlyProperty(
      LookupIterator* it, Handle<Object> value, ShouldThrow should_throw);
  MUST_USE_RESULT static Maybe<bool> WriteToReadOnlyProperty(
      Isolate* isolate, Handle<Object> receiver, Handle<Object> name,
      Handle<Object> value, ShouldThrow should_throw);
  MUST_USE_RESULT static Maybe<bool> RedefineIncompatibleProperty(
      Isolate* isolate, Handle<Object> name, Handle<Object> value,
      ShouldThrow should_throw);
  MUST_USE_RESULT static Maybe<bool> SetDataProperty(LookupIterator* it,
                                                     Handle<Object> value);
  MUST_USE_RESULT static Maybe<bool> AddDataProperty(
      LookupIterator* it, Handle<Object> value, PropertyAttributes attributes,
      ShouldThrow should_throw, StoreFromKeyed store_mode);
  MUST_USE_RESULT static inline MaybeHandle<Object> GetPropertyOrElement(
      Handle<Object> object, Handle<Name> name);
  MUST_USE_RESULT static inline MaybeHandle<Object> GetPropertyOrElement(
      Handle<Object> receiver, Handle<Name> name, Handle<JSReceiver> holder);
  MUST_USE_RESULT static inline MaybeHandle<Object> GetProperty(
      Handle<Object> object, Handle<Name> name);

  MUST_USE_RESULT static MaybeHandle<Object> GetPropertyWithAccessor(
      LookupIterator* it);
  MUST_USE_RESULT static Maybe<bool> SetPropertyWithAccessor(
      LookupIterator* it, Handle<Object> value, ShouldThrow should_throw);

  MUST_USE_RESULT static MaybeHandle<Object> GetPropertyWithDefinedGetter(
      Handle<Object> receiver,
      Handle<JSReceiver> getter);
  MUST_USE_RESULT static Maybe<bool> SetPropertyWithDefinedSetter(
      Handle<Object> receiver, Handle<JSReceiver> setter, Handle<Object> value,
      ShouldThrow should_throw);

  MUST_USE_RESULT static inline MaybeHandle<Object> GetElement(
      Isolate* isolate, Handle<Object> object, uint32_t index);

  MUST_USE_RESULT static inline MaybeHandle<Object> SetElement(
      Isolate* isolate, Handle<Object> object, uint32_t index,
      Handle<Object> value, LanguageMode language_mode);

  // Returns the permanent hash code associated with this object. May return
  // undefined if not yet created.
  Object* GetHash();

  // Returns the permanent hash code associated with this object depending on
  // the actual object type. May create and store a hash code if needed and none
  // exists.
  static Smi* GetOrCreateHash(Isolate* isolate, Handle<Object> object);

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
  MUST_USE_RESULT static MaybeHandle<Object> ArraySpeciesConstructor(
      Isolate* isolate, Handle<Object> original_array);

  // ES6 section 7.3.20 SpeciesConstructor ( O, defaultConstructor )
  MUST_USE_RESULT static MaybeHandle<Object> SpeciesConstructor(
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
  static void VerifyPointer(Object* p);
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

  // Helper for SetProperty and SetSuperProperty.
  // Return value is only meaningful if [found] is set to true on return.
  MUST_USE_RESULT static Maybe<bool> SetPropertyInternal(
      LookupIterator* it, Handle<Object> value, LanguageMode language_mode,
      StoreFromKeyed store_mode, bool* found);

  MUST_USE_RESULT static MaybeHandle<Name> ConvertToName(Isolate* isolate,
                                                         Handle<Object> input);
  MUST_USE_RESULT static MaybeHandle<Object> ConvertToPropertyKey(
      Isolate* isolate, Handle<Object> value);
  MUST_USE_RESULT static MaybeHandle<String> ConvertToString(
      Isolate* isolate, Handle<Object> input);
  MUST_USE_RESULT static MaybeHandle<Object> ConvertToNumber(
      Isolate* isolate, Handle<Object> input);
  MUST_USE_RESULT static MaybeHandle<Object> ConvertToInteger(
      Isolate* isolate, Handle<Object> input);
  MUST_USE_RESULT static MaybeHandle<Object> ConvertToInt32(
      Isolate* isolate, Handle<Object> input);
  MUST_USE_RESULT static MaybeHandle<Object> ConvertToUint32(
      Isolate* isolate, Handle<Object> input);
  MUST_USE_RESULT static MaybeHandle<Object> ConvertToLength(
      Isolate* isolate, Handle<Object> input);
  MUST_USE_RESULT static MaybeHandle<Object> ConvertToIndex(
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

  DECL_CAST(Smi)

  // Dispatched behavior.
  V8_EXPORT_PRIVATE void SmiPrint(std::ostream& os) const;  // NOLINT
  DECL_VERIFIER(Smi)

  static constexpr Smi* const kZero = nullptr;
  static const int kMinValue =
      (static_cast<unsigned int>(-1)) << (kSmiValueSize - 1);
  static const int kMaxValue = -(kMinValue + 1);

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

  // The Heap the object was allocated in. Used also to access Isolate.
  inline Heap* GetHeap() const;

  // Convenience method to get current isolate.
  inline Isolate* GetIsolate() const;

#define IS_TYPE_FUNCTION_DECL(Type) INLINE(bool Is##Type() const);
  HEAP_OBJECT_TYPE_LIST(IS_TYPE_FUNCTION_DECL)
#undef IS_TYPE_FUNCTION_DECL

#define IS_TYPE_FUNCTION_DECL(Type, Value) \
  INLINE(bool Is##Type(Isolate* isolate) const);
  ODDBALL_LIST(IS_TYPE_FUNCTION_DECL)
#undef IS_TYPE_FUNCTION_DECL

  INLINE(bool IsNullOrUndefined(Isolate* isolate) const);

#define DECL_STRUCT_PREDICATE(NAME, Name, name) INLINE(bool Is##Name() const);
  STRUCT_LIST(DECL_STRUCT_PREDICATE)
#undef DECL_STRUCT_PREDICATE

  // Converts an address to a HeapObject pointer.
  static inline HeapObject* FromAddress(Address address) {
    DCHECK_TAG_ALIGNED(address);
    return reinterpret_cast<HeapObject*>(address + kHeapObjectTag);
  }

  // Returns the address of this HeapObject.
  inline Address address() const {
    return reinterpret_cast<Address>(const_cast<HeapObject*>(this)) -
           kHeapObjectTag;
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
  void IterateBody(InstanceType type, int object_size, ObjectVisitor* v);

  template <typename ObjectVisitor>
  inline void IterateBodyFast(ObjectVisitor* v);

  template <typename ObjectVisitor>
  inline void IterateBodyFast(InstanceType type, int object_size,
                              ObjectVisitor* v);

  // Returns true if the object contains a tagged value at given offset.
  // It is used for invalid slots filtering. If the offset points outside
  // of the object or to the map word, the result is UNDEFINED (!!!).
  bool IsValidSlot(int offset);

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
  static inline Object** RawField(HeapObject* obj, int offset);

  // Adds the |code| object related to |name| to the code cache of this map. If
  // this map is a dictionary map that is shared, the map copied and installed
  // onto the object.
  static void UpdateMapCodeCache(Handle<HeapObject> object,
                                 Handle<Name> name,
                                 Handle<Code> code);

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
  inline void VerifyObjectField(int offset);
  inline void VerifySmiField(int offset);

  // Verify a pointer is a valid HeapObject pointer that points to object
  // areas in the heap.
  static void VerifyHeapPointer(Object* p);
#endif

  inline AllocationAlignment RequiredAlignment() const;

  // Layout description.
  // First field in a heap object is map.
  static const int kMapOffset = Object::kHeaderSize;
  static const int kHeaderSize = kMapOffset + kPointerSize;

  STATIC_ASSERT(kMapOffset == Internals::kHeapObjectMapOffset);

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(HeapObject);
};


template <int start_offset, int end_offset, int size>
class FixedBodyDescriptor;


template <int start_offset>
class FlexibleBodyDescriptor;


// The HeapNumber class describes heap allocated numbers that cannot be
// represented in a Smi (small integer)
class HeapNumber: public HeapObject {
 public:
  // [value]: number value.
  inline double value() const;
  inline void set_value(double value);

  inline uint64_t value_as_bits() const;
  inline void set_value_as_bits(uint64_t bits);

  DECL_CAST(HeapNumber)

  // Dispatched behavior.
  bool HeapNumberBooleanValue();

  V8_EXPORT_PRIVATE void HeapNumberPrint(std::ostream& os);  // NOLINT
  DECL_VERIFIER(HeapNumber)

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
  DISALLOW_IMPLICIT_CONSTRUCTORS(HeapNumber);
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

enum class GetKeysConversion { kKeepNumbers, kConvertToString };

enum class KeyCollectionMode {
  kOwnOnly = static_cast<int>(v8::KeyCollectionMode::kOwnOnly),
  kIncludePrototypes =
      static_cast<int>(v8::KeyCollectionMode::kIncludePrototypes)
};

enum class AllocationSiteUpdateMode { kUpdate, kCheckOnly };

// JSReceiver includes types on which properties can be defined, i.e.,
// JSObject and JSProxy.
class JSReceiver: public HeapObject {
 public:
  // Returns true if there is no slow (ie, dictionary) backing store.
  inline bool HasFastProperties() const;

  // Returns the properties array backing store if it
  // exists. Otherwise, returns an empty_property_array when there's a
  // Smi (hash code) or an empty_fixed_array for a fast properties
  // map.
  inline PropertyArray* property_array() const;

  // Gets slow properties for non-global objects.
  inline NameDictionary* property_dictionary() const;

  inline void SetProperties(HeapObject* properties);

  // There are four possible value for the properties offset.
  // 1) EmptyFixedArray -- This is the standard placeholder.
  //
  // 2) TODO(gsathya): Smi -- This is the hash code of the object.
  //
  // 3) PropertyArray - This is similar to a FixedArray but stores
  // the hash code of the object in its length field. This is a fast
  // backing store.
  //
  // 4) NameDictionary - This is the dictionary-mode backing store.
  //
  // This is used only in the deoptimizer and heap. Please use the
  // above typed getters and setters to access the properties.
  DECL_ACCESSORS(raw_properties_or_hash, Object)

  inline void initialize_properties();

  // Deletes an existing named property in a normalized object.
  static void DeleteNormalizedProperty(Handle<JSReceiver> object, int entry);

  DECL_CAST(JSReceiver)

  // ES6 section 7.1.1 ToPrimitive
  MUST_USE_RESULT static MaybeHandle<Object> ToPrimitive(
      Handle<JSReceiver> receiver,
      ToPrimitiveHint hint = ToPrimitiveHint::kDefault);

  // ES6 section 7.1.1.1 OrdinaryToPrimitive
  MUST_USE_RESULT static MaybeHandle<Object> OrdinaryToPrimitive(
      Handle<JSReceiver> receiver, OrdinaryToPrimitiveHint hint);

  static MaybeHandle<Context> GetFunctionRealm(Handle<JSReceiver> receiver);

  // Get the first non-hidden prototype.
  static inline MaybeHandle<Object> GetPrototype(Isolate* isolate,
                                                 Handle<JSReceiver> receiver);

  MUST_USE_RESULT static Maybe<bool> HasInPrototypeChain(
      Isolate* isolate, Handle<JSReceiver> object, Handle<Object> proto);

  // Reads all enumerable own properties of source and adds them to
  // target, using either Set or CreateDataProperty depending on the
  // use_set argument. This only copies values not present in the
  // maybe_excluded_properties list.
  MUST_USE_RESULT static Maybe<bool> SetOrCopyDataProperties(
      Isolate* isolate, Handle<JSReceiver> target, Handle<Object> source,
      const ScopedVector<Handle<Object>>* excluded_properties = nullptr,
      bool use_set = true);

  // Implementation of [[HasProperty]], ECMA-262 5th edition, section 8.12.6.
  MUST_USE_RESULT static Maybe<bool> HasProperty(LookupIterator* it);
  MUST_USE_RESULT static inline Maybe<bool> HasProperty(
      Handle<JSReceiver> object, Handle<Name> name);
  MUST_USE_RESULT static inline Maybe<bool> HasElement(
      Handle<JSReceiver> object, uint32_t index);

  MUST_USE_RESULT static inline Maybe<bool> HasOwnProperty(
      Handle<JSReceiver> object, Handle<Name> name);
  MUST_USE_RESULT static inline Maybe<bool> HasOwnProperty(
      Handle<JSReceiver> object, uint32_t index);

  MUST_USE_RESULT static inline MaybeHandle<Object> GetProperty(
      Isolate* isolate, Handle<JSReceiver> receiver, const char* key);
  MUST_USE_RESULT static inline MaybeHandle<Object> GetProperty(
      Handle<JSReceiver> receiver, Handle<Name> name);
  MUST_USE_RESULT static inline MaybeHandle<Object> GetElement(
      Isolate* isolate, Handle<JSReceiver> receiver, uint32_t index);

  // Implementation of ES6 [[Delete]]
  MUST_USE_RESULT static Maybe<bool> DeletePropertyOrElement(
      Handle<JSReceiver> object, Handle<Name> name,
      LanguageMode language_mode = SLOPPY);
  MUST_USE_RESULT static Maybe<bool> DeleteProperty(
      Handle<JSReceiver> object, Handle<Name> name,
      LanguageMode language_mode = SLOPPY);
  MUST_USE_RESULT static Maybe<bool> DeleteProperty(LookupIterator* it,
                                                    LanguageMode language_mode);
  MUST_USE_RESULT static Maybe<bool> DeleteElement(
      Handle<JSReceiver> object, uint32_t index,
      LanguageMode language_mode = SLOPPY);

  MUST_USE_RESULT static Object* DefineProperty(Isolate* isolate,
                                                Handle<Object> object,
                                                Handle<Object> name,
                                                Handle<Object> attributes);
  MUST_USE_RESULT static MaybeHandle<Object> DefineProperties(
      Isolate* isolate, Handle<Object> object, Handle<Object> properties);

  // "virtual" dispatcher to the correct [[DefineOwnProperty]] implementation.
  MUST_USE_RESULT static Maybe<bool> DefineOwnProperty(
      Isolate* isolate, Handle<JSReceiver> object, Handle<Object> key,
      PropertyDescriptor* desc, ShouldThrow should_throw);

  // ES6 7.3.4 (when passed DONT_THROW)
  MUST_USE_RESULT static Maybe<bool> CreateDataProperty(
      LookupIterator* it, Handle<Object> value, ShouldThrow should_throw);

  // ES6 9.1.6.1
  MUST_USE_RESULT static Maybe<bool> OrdinaryDefineOwnProperty(
      Isolate* isolate, Handle<JSObject> object, Handle<Object> key,
      PropertyDescriptor* desc, ShouldThrow should_throw);
  MUST_USE_RESULT static Maybe<bool> OrdinaryDefineOwnProperty(
      LookupIterator* it, PropertyDescriptor* desc, ShouldThrow should_throw);
  // ES6 9.1.6.2
  MUST_USE_RESULT static Maybe<bool> IsCompatiblePropertyDescriptor(
      Isolate* isolate, bool extensible, PropertyDescriptor* desc,
      PropertyDescriptor* current, Handle<Name> property_name,
      ShouldThrow should_throw);
  // ES6 9.1.6.3
  // |it| can be NULL in cases where the ES spec passes |undefined| as the
  // receiver. Exactly one of |it| and |property_name| must be provided.
  MUST_USE_RESULT static Maybe<bool> ValidateAndApplyPropertyDescriptor(
      Isolate* isolate, LookupIterator* it, bool extensible,
      PropertyDescriptor* desc, PropertyDescriptor* current,
      ShouldThrow should_throw, Handle<Name> property_name);

  V8_EXPORT_PRIVATE MUST_USE_RESULT static Maybe<bool> GetOwnPropertyDescriptor(
      Isolate* isolate, Handle<JSReceiver> object, Handle<Object> key,
      PropertyDescriptor* desc);
  MUST_USE_RESULT static Maybe<bool> GetOwnPropertyDescriptor(
      LookupIterator* it, PropertyDescriptor* desc);

  typedef PropertyAttributes IntegrityLevel;

  // ES6 7.3.14 (when passed DONT_THROW)
  // 'level' must be SEALED or FROZEN.
  MUST_USE_RESULT static Maybe<bool> SetIntegrityLevel(
      Handle<JSReceiver> object, IntegrityLevel lvl, ShouldThrow should_throw);

  // ES6 7.3.15
  // 'level' must be SEALED or FROZEN.
  MUST_USE_RESULT static Maybe<bool> TestIntegrityLevel(
      Handle<JSReceiver> object, IntegrityLevel lvl);

  // ES6 [[PreventExtensions]] (when passed DONT_THROW)
  MUST_USE_RESULT static Maybe<bool> PreventExtensions(
      Handle<JSReceiver> object, ShouldThrow should_throw);

  MUST_USE_RESULT static Maybe<bool> IsExtensible(Handle<JSReceiver> object);

  // Returns the class name ([[Class]] property in the specification).
  V8_EXPORT_PRIVATE String* class_name();

  // Returns the constructor name (the name (possibly, inferred name) of the
  // function that was used to instantiate the object).
  static Handle<String> GetConstructorName(Handle<JSReceiver> receiver);

  Handle<Context> GetCreationContext();

  MUST_USE_RESULT static inline Maybe<PropertyAttributes> GetPropertyAttributes(
      Handle<JSReceiver> object, Handle<Name> name);
  MUST_USE_RESULT static inline Maybe<PropertyAttributes>
  GetOwnPropertyAttributes(Handle<JSReceiver> object, Handle<Name> name);
  MUST_USE_RESULT static inline Maybe<PropertyAttributes>
  GetOwnPropertyAttributes(Handle<JSReceiver> object, uint32_t index);

  MUST_USE_RESULT static inline Maybe<PropertyAttributes> GetElementAttributes(
      Handle<JSReceiver> object, uint32_t index);
  MUST_USE_RESULT static inline Maybe<PropertyAttributes>
  GetOwnElementAttributes(Handle<JSReceiver> object, uint32_t index);

  MUST_USE_RESULT static Maybe<PropertyAttributes> GetPropertyAttributes(
      LookupIterator* it);

  // Set the object's prototype (only JSReceiver and null are allowed values).
  MUST_USE_RESULT static Maybe<bool> SetPrototype(Handle<JSReceiver> object,
                                                  Handle<Object> value,
                                                  bool from_javascript,
                                                  ShouldThrow should_throw);

  inline static Handle<Object> GetDataProperty(Handle<JSReceiver> object,
                                               Handle<Name> name);
  static Handle<Object> GetDataProperty(LookupIterator* it);


  // Retrieves a permanent object identity hash code. The undefined value might
  // be returned in case no hash was created yet.
  static inline Object* GetIdentityHash(Isolate* isolate,
                                        Handle<JSReceiver> object);

  // Retrieves a permanent object identity hash code. May create and store a
  // hash code if needed and none exists.
  inline static Smi* GetOrCreateIdentityHash(Isolate* isolate,
                                             Handle<JSReceiver> object);

  // ES6 [[OwnPropertyKeys]] (modulo return type)
  MUST_USE_RESULT static inline MaybeHandle<FixedArray> OwnPropertyKeys(
      Handle<JSReceiver> object);

  MUST_USE_RESULT static MaybeHandle<FixedArray> GetOwnValues(
      Handle<JSReceiver> object, PropertyFilter filter);

  MUST_USE_RESULT static MaybeHandle<FixedArray> GetOwnEntries(
      Handle<JSReceiver> object, PropertyFilter filter);

  // Layout description.
  static const int kPropertiesOrHashOffset = HeapObject::kHeaderSize;
  static const int kHeaderSize = HeapObject::kHeaderSize + kPointerSize;

  bool HasProxyInPrototype(Isolate* isolate);

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

  static MUST_USE_RESULT MaybeHandle<JSObject> New(
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

  inline bool HasFastArgumentsElements();
  inline bool HasSlowArgumentsElements();
  inline bool HasFastStringWrapperElements();
  inline bool HasSlowStringWrapperElements();
  bool HasEnumerableElements();

  inline SeededNumberDictionary* element_dictionary();  // Gets slow elements.

  // Requires: HasFastElements().
  static void EnsureWritableFastElements(Handle<JSObject> object);

  MUST_USE_RESULT static Maybe<bool> SetPropertyWithInterceptor(
      LookupIterator* it, ShouldThrow should_throw, Handle<Object> value);

  // The API currently still wants DefineOwnPropertyIgnoreAttributes to convert
  // AccessorInfo objects to data fields. We allow FORCE_FIELD as an exception
  // to the default behavior that calls the setter.
  enum AccessorInfoHandling { FORCE_FIELD, DONT_FORCE_FIELD };

  MUST_USE_RESULT static MaybeHandle<Object> DefineOwnPropertyIgnoreAttributes(
      LookupIterator* it, Handle<Object> value, PropertyAttributes attributes,
      AccessorInfoHandling handling = DONT_FORCE_FIELD);

  MUST_USE_RESULT static Maybe<bool> DefineOwnPropertyIgnoreAttributes(
      LookupIterator* it, Handle<Object> value, PropertyAttributes attributes,
      ShouldThrow should_throw,
      AccessorInfoHandling handling = DONT_FORCE_FIELD);

  MUST_USE_RESULT static MaybeHandle<Object> SetOwnPropertyIgnoreAttributes(
      Handle<JSObject> object, Handle<Name> name, Handle<Object> value,
      PropertyAttributes attributes);

  MUST_USE_RESULT static MaybeHandle<Object> SetOwnElementIgnoreAttributes(
      Handle<JSObject> object, uint32_t index, Handle<Object> value,
      PropertyAttributes attributes);

  // Equivalent to one of the above depending on whether |name| can be converted
  // to an array index.
  MUST_USE_RESULT static MaybeHandle<Object>
  DefinePropertyOrElementIgnoreAttributes(Handle<JSObject> object,
                                          Handle<Name> name,
                                          Handle<Object> value,
                                          PropertyAttributes attributes = NONE);

  // Adds or reconfigures a property to attributes NONE. It will fail when it
  // cannot.
  MUST_USE_RESULT static Maybe<bool> CreateDataProperty(
      LookupIterator* it, Handle<Object> value,
      ShouldThrow should_throw = DONT_THROW);

  static void AddProperty(Handle<JSObject> object, Handle<Name> name,
                          Handle<Object> value, PropertyAttributes attributes);

  MUST_USE_RESULT static Maybe<bool> AddDataElement(
      Handle<JSObject> receiver, uint32_t index, Handle<Object> value,
      PropertyAttributes attributes, ShouldThrow should_throw);
  MUST_USE_RESULT static MaybeHandle<Object> AddDataElement(
      Handle<JSObject> receiver, uint32_t index, Handle<Object> value,
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

  static void OptimizeAsPrototype(Handle<JSObject> object);
  static void ReoptimizeIfPrototype(Handle<JSObject> object);
  static void MakePrototypesFast(Handle<Object> receiver,
                                 WhereToStart where_to_start, Isolate* isolate);
  static void LazyRegisterPrototypeUser(Handle<Map> user, Isolate* isolate);
  static void UpdatePrototypeUserRegistration(Handle<Map> old_map,
                                              Handle<Map> new_map,
                                              Isolate* isolate);
  static bool UnregisterPrototypeUser(Handle<Map> user, Isolate* isolate);
  static void InvalidatePrototypeChains(Map* map);

  // Updates prototype chain tracking information when an object changes its
  // map from |old_map| to |new_map|.
  static void NotifyMapChange(Handle<Map> old_map, Handle<Map> new_map,
                              Isolate* isolate);

  // Utility used by many Array builtins and runtime functions
  static inline bool PrototypeHasNoElements(Isolate* isolate, JSObject* object);

  // Alternative implementation of WeakFixedArray::NullCallback.
  class PrototypeRegistryCompactionCallback {
   public:
    static void Callback(Object* value, int old_index, int new_index);
  };

  // Retrieve interceptors.
  inline InterceptorInfo* GetNamedInterceptor();
  inline InterceptorInfo* GetIndexedInterceptor();

  // Used from JSReceiver.
  MUST_USE_RESULT static Maybe<PropertyAttributes>
  GetPropertyAttributesWithInterceptor(LookupIterator* it);
  MUST_USE_RESULT static Maybe<PropertyAttributes>
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
  MUST_USE_RESULT static MaybeHandle<Object> SetAccessor(
      Handle<JSObject> object,
      Handle<AccessorInfo> info);

  // The result must be checked first for exceptions. If there's no exception,
  // the output parameter |done| indicates whether the interceptor has a result
  // or not.
  MUST_USE_RESULT static MaybeHandle<Object> GetPropertyWithInterceptor(
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
  MUST_USE_RESULT static Maybe<bool> HasRealNamedProperty(
      Handle<JSObject> object, Handle<Name> name);
  MUST_USE_RESULT static Maybe<bool> HasRealElementProperty(
      Handle<JSObject> object, uint32_t index);
  MUST_USE_RESULT static Maybe<bool> HasRealNamedCallbackProperty(
      Handle<JSObject> object, Handle<Name> name);

  // Get the header size for a JSObject.  Used to compute the index of
  // embedder fields as well as the number of embedder fields.
  static int GetHeaderSize(InstanceType instance_type);
  inline int GetHeaderSize();

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
  // SeededNumberDictionary dictionary.  Returns the backing after conversion.
  static Handle<SeededNumberDictionary> NormalizeElements(
      Handle<JSObject> object);

  void RequireSlowElements(SeededNumberDictionary* dictionary);

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
  MUST_USE_RESULT static Maybe<bool> SetPrototype(Handle<JSObject> object,
                                                  Handle<Object> value,
                                                  bool from_javascript,
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

  MUST_USE_RESULT static Maybe<bool> TestIntegrityLevel(Handle<JSObject> object,
                                                        IntegrityLevel lvl);

  MUST_USE_RESULT static Maybe<bool> PreventExtensions(
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

  void IncrementSpillStatistics(SpillInformation* info);
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

  // Layout description.
  static const int kElementsOffset = JSReceiver::kHeaderSize;
  static const int kHeaderSize = kElementsOffset + kPointerSize;

  STATIC_ASSERT(kHeaderSize == Internals::kJSObjectHeaderSize);
  static const int kMaxInObjectProperties =
      (kMaxInstanceSize - kHeaderSize) >> kPointerSizeLog2;

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
  MUST_USE_RESULT static MaybeHandle<Object> GetPropertyWithFailedAccessCheck(
      LookupIterator* it);

  MUST_USE_RESULT static Maybe<bool> SetPropertyWithFailedAccessCheck(
      LookupIterator* it, Handle<Object> value, ShouldThrow should_throw);

  MUST_USE_RESULT static Maybe<bool> DeletePropertyWithInterceptor(
      LookupIterator* it, ShouldThrow should_throw);

  bool ReferencesObjectFromElements(FixedArray* elements,
                                    ElementsKind kind,
                                    Object* object);

  static Object* GetIdentityHash(Isolate* isolate, Handle<JSObject> object);

  static Smi* GetOrCreateIdentityHash(Isolate* isolate,
                                      Handle<JSObject> object);

  // Helper for fast versions of preventExtensions, seal, and freeze.
  // attrs is one of NONE, SEALED, or FROZEN (depending on the operation).
  template <PropertyAttributes attrs>
  MUST_USE_RESULT static Maybe<bool> PreventExtensionsWithTransition(
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


// Common superclass for FixedArrays that allow implementations to share
// common accessors and some code paths.
class FixedArrayBase: public HeapObject {
 public:
  // [length]: length of the array.
  inline int length() const;
  inline void set_length(int value);

  // Get and set the length using acquire loads and release stores.
  inline int synchronized_length() const;
  inline void synchronized_set_length(int value);

  DECL_CAST(FixedArrayBase)

  static int GetMaxLengthForNewSpaceAllocation(ElementsKind kind);

  bool IsCowArray() const;

  // Layout description.
  // Length is smi tagged when it is stored.
  static const int kLengthOffset = HeapObject::kHeaderSize;
  static const int kHeaderSize = kLengthOffset + kPointerSize;
};


class FixedDoubleArray;
class IncrementalMarking;


// FixedArray describes fixed-sized arrays with element type Object*.
class FixedArray: public FixedArrayBase {
 public:
  // Setter and getter for elements.
  inline Object* get(int index) const;
  static inline Handle<Object> get(FixedArray* array, int index,
                                   Isolate* isolate);
  template <class T>
  MaybeHandle<T> GetValue(Isolate* isolate, int index) const;

  template <class T>
  Handle<T> GetValueChecked(Isolate* isolate, int index) const;

  // Return a grown copy if the index is bigger than the array's length.
  static Handle<FixedArray> SetAndGrow(Handle<FixedArray> array, int index,
                                       Handle<Object> value);

  // Setter that uses write barrier.
  inline void set(int index, Object* value);
  inline bool is_the_hole(Isolate* isolate, int index);

  // Setter that doesn't need write barrier.
  inline void set(int index, Smi* value);
  // Setter with explicit barrier mode.
  inline void set(int index, Object* value, WriteBarrierMode mode);

  // Setters for frequently used oddballs located in old space.
  inline void set_undefined(int index);
  inline void set_undefined(Isolate* isolate, int index);
  inline void set_null(int index);
  inline void set_null(Isolate* isolate, int index);
  inline void set_the_hole(int index);
  inline void set_the_hole(Isolate* isolate, int index);

  inline Object** GetFirstElementAddress();
  inline bool ContainsOnlySmisOrHoles();

  // Gives access to raw memory which stores the array's data.
  inline Object** data_start();

  inline void FillWithHoles(int from, int to);

  // Shrink length and insert filler objects.
  void Shrink(int length);

  // Copy a sub array from the receiver to dest.
  void CopyTo(int pos, FixedArray* dest, int dest_pos, int len) const;

  // Garbage collection support.
  static constexpr int SizeFor(int length) {
    return kHeaderSize + length * kPointerSize;
  }

  // Code Generation support.
  static constexpr int OffsetOfElementAt(int index) { return SizeFor(index); }

  // Garbage collection support.
  inline Object** RawFieldOfElementAt(int index);

  DECL_CAST(FixedArray)

  // Maximal allowed size, in bytes, of a single FixedArray.
  // Prevents overflowing size computations, as well as extreme memory
  // consumption.
  static const int kMaxSize = 128 * MB * kPointerSize;
  // Maximally allowed length of a FixedArray.
  static const int kMaxLength = (kMaxSize - kHeaderSize) / kPointerSize;
  // Maximally allowed length for regular (non large object space) object.
  STATIC_ASSERT(kMaxRegularHeapObjectSize < kMaxSize);
  static const int kMaxRegularLength =
      (kMaxRegularHeapObjectSize - kHeaderSize) / kPointerSize;

  // Dispatched behavior.
  DECL_PRINTER(FixedArray)
  DECL_VERIFIER(FixedArray)
#ifdef DEBUG
  // Checks if two FixedArrays have identical contents.
  bool IsEqualTo(FixedArray* other);
#endif

  typedef FlexibleBodyDescriptor<kHeaderSize> BodyDescriptor;
  // No weak fields.
  typedef BodyDescriptor BodyDescriptorWeak;

 protected:
  // Set operation on FixedArray without using write barriers. Can
  // only be used for storing old space objects or smis.
  static inline void NoWriteBarrierSet(FixedArray* array,
                                       int index,
                                       Object* value);

 private:
  STATIC_ASSERT(kHeaderSize == Internals::kFixedArrayHeaderSize);

  DISALLOW_IMPLICIT_CONSTRUCTORS(FixedArray);
};

// FixedDoubleArray describes fixed-sized arrays with element type double.
class FixedDoubleArray: public FixedArrayBase {
 public:
  // Setter and getter for elements.
  inline double get_scalar(int index);
  inline uint64_t get_representation(int index);
  static inline Handle<Object> get(FixedDoubleArray* array, int index,
                                   Isolate* isolate);
  inline void set(int index, double value);
  inline void set_the_hole(Isolate* isolate, int index);
  inline void set_the_hole(int index);

  // Checking for the hole.
  inline bool is_the_hole(Isolate* isolate, int index);
  inline bool is_the_hole(int index);

  // Garbage collection support.
  inline static int SizeFor(int length) {
    return kHeaderSize + length * kDoubleSize;
  }

  // Gives access to raw memory which stores the array's data.
  inline double* data_start();

  inline void FillWithHoles(int from, int to);

  // Code Generation support.
  static int OffsetOfElementAt(int index) { return SizeFor(index); }

  DECL_CAST(FixedDoubleArray)

  // Maximal allowed size, in bytes, of a single FixedDoubleArray.
  // Prevents overflowing size computations, as well as extreme memory
  // consumption.
  static const int kMaxSize = 512 * MB;
  // Maximally allowed length of a FixedArray.
  static const int kMaxLength = (kMaxSize - kHeaderSize) / kDoubleSize;

  // Dispatched behavior.
  DECL_PRINTER(FixedDoubleArray)
  DECL_VERIFIER(FixedDoubleArray)

  class BodyDescriptor;
  // No weak fields.
  typedef BodyDescriptor BodyDescriptorWeak;

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(FixedDoubleArray);
};

class WeakFixedArray : public FixedArray {
 public:
  // If |maybe_array| is not a WeakFixedArray, a fresh one will be allocated.
  // This function does not check if the value exists already, callers must
  // ensure this themselves if necessary.
  static Handle<WeakFixedArray> Add(Handle<Object> maybe_array,
                                    Handle<HeapObject> value,
                                    int* assigned_index = NULL);

  // Returns true if an entry was found and removed.
  bool Remove(Handle<HeapObject> value);

  class NullCallback {
   public:
    static void Callback(Object* value, int old_index, int new_index) {}
  };

  template <class CompactionCallback>
  void Compact();

  inline Object* Get(int index) const;
  inline void Clear(int index);
  inline int Length() const;

  inline bool IsEmptySlot(int index) const;
  static Object* Empty() { return Smi::kZero; }

  class Iterator {
   public:
    explicit Iterator(Object* maybe_array) : list_(NULL) { Reset(maybe_array); }
    void Reset(Object* maybe_array);

    template <class T>
    inline T* Next();

   private:
    int index_;
    WeakFixedArray* list_;
#ifdef DEBUG
    int last_used_index_;
    DisallowHeapAllocation no_gc_;
#endif  // DEBUG
    DISALLOW_COPY_AND_ASSIGN(Iterator);
  };

  DECL_CAST(WeakFixedArray)

 private:
  static const int kLastUsedIndexIndex = 0;
  static const int kFirstIndex = 1;

  static Handle<WeakFixedArray> Allocate(
      Isolate* isolate, int size, Handle<WeakFixedArray> initialize_from);

  static void Set(Handle<WeakFixedArray> array, int index,
                  Handle<HeapObject> value);
  inline void clear(int index);

  inline int last_used_index() const;
  inline void set_last_used_index(int index);

  // Disallow inherited setters.
  void set(int index, Smi* value);
  void set(int index, Object* value);
  void set(int index, Object* value, WriteBarrierMode mode);
  DISALLOW_IMPLICIT_CONSTRUCTORS(WeakFixedArray);
};

// Generic array grows dynamically with O(1) amortized insertion.
//
// ArrayList is a FixedArray with static convenience methods for adding more
// elements. The Length() method returns the number of elements in the list, not
// the allocated size. The number of elements is stored at kLengthIndex and is
// updated with every insertion. The elements of the ArrayList are stored in the
// underlying FixedArray starting at kFirstIndex.
class ArrayList : public FixedArray {
 public:
  enum AddMode {
    kNone,
    // Use this if GC can delete elements from the array.
    kReloadLengthAfterAllocation,
  };
  static Handle<ArrayList> Add(Handle<ArrayList> array, Handle<Object> obj,
                               AddMode mode = kNone);
  static Handle<ArrayList> Add(Handle<ArrayList> array, Handle<Object> obj1,
                               Handle<Object> obj2, AddMode = kNone);
  static Handle<ArrayList> New(Isolate* isolate, int size);

  // Returns the number of elements in the list, not the allocated size, which
  // is length(). Lower and upper case length() return different results!
  inline int Length() const;

  // Sets the Length() as used by Elements(). Does not change the underlying
  // storage capacity, i.e., length().
  inline void SetLength(int length);
  inline Object* Get(int index) const;
  inline Object** Slot(int index);

  // Set the element at index to obj. The underlying array must be large enough.
  // If you need to grow the ArrayList, use the static Add() methods instead.
  inline void Set(int index, Object* obj,
                  WriteBarrierMode mode = UPDATE_WRITE_BARRIER);

  // Set the element at index to undefined. This does not change the Length().
  inline void Clear(int index, Object* undefined);

  // Return a copy of the list of size Length() without the first entry. The
  // number returned by Length() is stored in the first entry.
  static Handle<FixedArray> Elements(Handle<ArrayList> array);
  bool IsFull();
  DECL_CAST(ArrayList)

 private:
  static Handle<ArrayList> EnsureSpace(Handle<ArrayList> array, int length);
  static const int kLengthIndex = 0;
  static const int kFirstIndex = 1;
  DISALLOW_IMPLICIT_CONSTRUCTORS(ArrayList);
};

class PropertyArray : public HeapObject {
 public:
  // [length]: length of the array.
  inline int length() const;
  inline void set_length(int length);

  // Get and set the length using acquire loads and release stores.
  inline int synchronized_length() const;
  inline void synchronized_set_length(int value);

  inline Object* get(int index) const;

  // Setter that doesn't need write barrier.
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
  static const int kLengthOffset = HeapObject::kHeaderSize;
  static const int kHeaderSize = kLengthOffset + kPointerSize;

  // Garbage collection support.
  typedef FlexibleBodyDescriptor<kHeaderSize> BodyDescriptor;
  // No weak fields.
  typedef BodyDescriptor BodyDescriptorWeak;

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(PropertyArray);
};

enum SearchMode { ALL_ENTRIES, VALID_ENTRIES };

template <SearchMode search_mode, typename T>
inline int Search(T* array, Name* name, int valid_entries = 0,
                  int* out_insertion_index = NULL);

// HandlerTable is a fixed array containing entries for exception handlers in
// the code object it is associated with. The tables comes in two flavors:
// 1) Based on ranges: Used for unoptimized code. Contains one entry per
//    exception handler and a range representing the try-block covered by that
//    handler. Layout looks as follows:
//      [ range-start , range-end , handler-offset , handler-data ]
// 2) Based on return addresses: Used for turbofanned code. Contains one entry
//    per call-site that could throw an exception. Layout looks as follows:
//      [ return-address-offset , handler-offset ]
class HandlerTable : public FixedArray {
 public:
  // Conservative prediction whether a given handler will locally catch an
  // exception or cause a re-throw to outside the code boundary. Since this is
  // undecidable it is merely an approximation (e.g. useful for debugger).
  enum CatchPrediction {
    UNCAUGHT,    // The handler will (likely) rethrow the exception.
    CAUGHT,      // The exception will be caught by the handler.
    PROMISE,     // The exception will be caught and cause a promise rejection.
    DESUGARING,  // The exception will be caught, but both the exception and the
                 // catching are part of a desugaring and should therefore not
                 // be visible to the user (we won't notify the debugger of such
                 // exceptions).
    ASYNC_AWAIT,  // The exception will be caught and cause a promise rejection
                  // in the desugaring of an async function, so special
                  // async/await handling in the debugger can take place.
  };

  // Getters for handler table based on ranges.
  inline int GetRangeStart(int index) const;
  inline int GetRangeEnd(int index) const;
  inline int GetRangeHandler(int index) const;
  inline int GetRangeData(int index) const;

  // Setters for handler table based on ranges.
  inline void SetRangeStart(int index, int value);
  inline void SetRangeEnd(int index, int value);
  inline void SetRangeHandler(int index, int offset, CatchPrediction pred);
  inline void SetRangeData(int index, int value);

  // Setters for handler table based on return addresses.
  inline void SetReturnOffset(int index, int value);
  inline void SetReturnHandler(int index, int offset);

  // Lookup handler in a table based on ranges. The {pc_offset} is an offset to
  // the start of the potentially throwing instruction (using return addresses
  // for this value would be invalid).
  int LookupRange(int pc_offset, int* data, CatchPrediction* prediction);

  // Lookup handler in a table based on return addresses.
  int LookupReturn(int pc_offset);

  // Returns the number of entries in the table.
  inline int NumberOfRangeEntries() const;

  // Returns the required length of the underlying fixed array.
  static int LengthForRange(int entries) { return entries * kRangeEntrySize; }
  static int LengthForReturn(int entries) { return entries * kReturnEntrySize; }

  DECL_CAST(HandlerTable)

#ifdef ENABLE_DISASSEMBLER
  void HandlerTableRangePrint(std::ostream& os);   // NOLINT
  void HandlerTableReturnPrint(std::ostream& os);  // NOLINT
#endif

 private:
  // Layout description for handler table based on ranges.
  static const int kRangeStartIndex = 0;
  static const int kRangeEndIndex = 1;
  static const int kRangeHandlerIndex = 2;
  static const int kRangeDataIndex = 3;
  static const int kRangeEntrySize = 4;

  // Layout description for handler table based on return addresses.
  static const int kReturnOffsetIndex = 0;
  static const int kReturnHandlerIndex = 1;
  static const int kReturnEntrySize = 2;

  // Encoding of the {handler} field.
  class HandlerPredictionField : public BitField<CatchPrediction, 0, 3> {};
  class HandlerOffsetField : public BitField<int, 3, 29> {};
};

// ByteArray represents fixed sized byte arrays.  Used for the relocation info
// that is attached to code objects.
class ByteArray: public FixedArrayBase {
 public:
  inline int Size();

  // Setter and getter.
  inline byte get(int index) const;
  inline void set(int index, byte value);

  // Copy in / copy out whole byte slices.
  inline void copy_out(int index, byte* buffer, int length);
  inline void copy_in(int index, const byte* buffer, int length);

  // Treat contents as an int array.
  inline int get_int(int index) const;
  inline void set_int(int index, int value);

  inline uint32_t get_uint32(int index) const;
  inline void set_uint32(int index, uint32_t value);

  static int SizeFor(int length) {
    return OBJECT_POINTER_ALIGN(kHeaderSize + length);
  }
  // We use byte arrays for free blocks in the heap.  Given a desired size in
  // bytes that is a multiple of the word size and big enough to hold a byte
  // array, this function returns the number of elements a byte array should
  // have.
  static int LengthFor(int size_in_bytes) {
    DCHECK(IsAligned(size_in_bytes, kPointerSize));
    DCHECK(size_in_bytes >= kHeaderSize);
    return size_in_bytes - kHeaderSize;
  }

  // Returns data start address.
  inline Address GetDataStartAddress();

  inline int DataSize() const;

  // Returns a pointer to the ByteArray object for a given data start address.
  static inline ByteArray* FromDataStartAddress(Address address);

  DECL_CAST(ByteArray)

  // Dispatched behavior.
  inline int ByteArraySize();
  DECL_PRINTER(ByteArray)
  DECL_VERIFIER(ByteArray)

  // Layout description.
  static const int kAlignedSize = OBJECT_POINTER_ALIGN(kHeaderSize);

  // Maximal memory consumption for a single ByteArray.
  static const int kMaxSize = 512 * MB;
  // Maximal length of a single ByteArray.
  static const int kMaxLength = kMaxSize - kHeaderSize;

  class BodyDescriptor;
  // No weak fields.
  typedef BodyDescriptor BodyDescriptorWeak;

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(ByteArray);
};

// Wrapper class for ByteArray which can store arbitrary C++ classes, as long
// as they can be copied with memcpy.
template <class T>
class PodArray : public ByteArray {
 public:
  static Handle<PodArray<T>> New(Isolate* isolate, int length,
                                 PretenureFlag pretenure = NOT_TENURED);
  void copy_out(int index, T* result) {
    ByteArray::copy_out(index * sizeof(T), reinterpret_cast<byte*>(result),
                        sizeof(T));
  }
  T get(int index) {
    T result;
    copy_out(index, &result);
    return result;
  }
  void set(int index, const T& value) {
    copy_in(index * sizeof(T), reinterpret_cast<const byte*>(&value),
            sizeof(T));
  }
  int length() { return ByteArray::length() / sizeof(T); }
  DECL_CAST(PodArray<T>)

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(PodArray<T>);
};

// BytecodeArray represents a sequence of interpreter bytecodes.
class BytecodeArray : public FixedArrayBase {
 public:
#define DECL_BYTECODE_AGE_ENUM(X) k##X##BytecodeAge,
  enum Age {
    kNoAgeBytecodeAge = 0,
    CODE_AGE_LIST(DECL_BYTECODE_AGE_ENUM) kAfterLastBytecodeAge,
    kFirstBytecodeAge = kNoAgeBytecodeAge,
    kLastBytecodeAge = kAfterLastBytecodeAge - 1,
    kBytecodeAgeCount = kAfterLastBytecodeAge - kFirstBytecodeAge - 1,
    kIsOldBytecodeAge = kSexagenarianBytecodeAge
  };
#undef DECL_BYTECODE_AGE_ENUM

  static int SizeFor(int length) {
    return OBJECT_POINTER_ALIGN(kHeaderSize + length);
  }

  // Setter and getter
  inline byte get(int index);
  inline void set(int index, byte value);

  // Returns data start address.
  inline Address GetFirstBytecodeAddress();

  // Accessors for frame size.
  inline int frame_size() const;
  inline void set_frame_size(int frame_size);

  // Accessor for register count (derived from frame_size).
  inline int register_count() const;

  // Accessors for parameter count (including implicit 'this' receiver).
  inline int parameter_count() const;
  inline void set_parameter_count(int number_of_parameters);

  // Accessors for profiling count.
  inline int interrupt_budget() const;
  inline void set_interrupt_budget(int interrupt_budget);

  // Accessors for OSR loop nesting level.
  inline int osr_loop_nesting_level() const;
  inline void set_osr_loop_nesting_level(int depth);

  // Accessors for bytecode's code age.
  inline Age bytecode_age() const;
  inline void set_bytecode_age(Age age);

  // Accessors for the constant pool.
  DECL_ACCESSORS(constant_pool, FixedArray)

  // Accessors for handler table containing offsets of exception handlers.
  DECL_ACCESSORS(handler_table, FixedArray)

  // Accessors for source position table containing mappings between byte code
  // offset and source position or SourcePositionTableWithFrameCache.
  DECL_ACCESSORS(source_position_table, Object)

  inline ByteArray* SourcePositionTable();

  DECL_CAST(BytecodeArray)

  // Dispatched behavior.
  inline int BytecodeArraySize();

  inline int instruction_size();

  // Returns the size of bytecode and its metadata. This includes the size of
  // bytecode, constant pool, source position table, and handler table.
  inline int SizeIncludingMetadata();

  int SourcePosition(int offset);
  int SourceStatementPosition(int offset);

  DECL_PRINTER(BytecodeArray)
  DECL_VERIFIER(BytecodeArray)

  void Disassemble(std::ostream& os);

  void CopyBytecodesTo(BytecodeArray* to);

  // Bytecode aging
  bool IsOld() const;
  void MakeOlder();

  // Layout description.
  static const int kConstantPoolOffset = FixedArrayBase::kHeaderSize;
  static const int kHandlerTableOffset = kConstantPoolOffset + kPointerSize;
  static const int kSourcePositionTableOffset =
      kHandlerTableOffset + kPointerSize;
  static const int kFrameSizeOffset = kSourcePositionTableOffset + kPointerSize;
  static const int kParameterSizeOffset = kFrameSizeOffset + kIntSize;
  static const int kInterruptBudgetOffset = kParameterSizeOffset + kIntSize;
  static const int kOSRNestingLevelOffset = kInterruptBudgetOffset + kIntSize;
  static const int kBytecodeAgeOffset = kOSRNestingLevelOffset + kCharSize;
  static const int kHeaderSize = kBytecodeAgeOffset + kCharSize;

  // Maximal memory consumption for a single BytecodeArray.
  static const int kMaxSize = 512 * MB;
  // Maximal length of a single BytecodeArray.
  static const int kMaxLength = kMaxSize - kHeaderSize;

  static const int kPointerFieldsBeginOffset = kConstantPoolOffset;
  static const int kPointerFieldsEndOffset = kFrameSizeOffset;

  class BodyDescriptor;
  // No weak fields.
  typedef BodyDescriptor BodyDescriptorWeak;

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(BytecodeArray);
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


// V has parameters (Type, type, TYPE, C type, element_size)
#define TYPED_ARRAYS(V) \
  V(Uint8, uint8, UINT8, uint8_t, 1)                                           \
  V(Int8, int8, INT8, int8_t, 1)                                               \
  V(Uint16, uint16, UINT16, uint16_t, 2)                                       \
  V(Int16, int16, INT16, int16_t, 2)                                           \
  V(Uint32, uint32, UINT32, uint32_t, 4)                                       \
  V(Int32, int32, INT32, int32_t, 4)                                           \
  V(Float32, float32, FLOAT32, float, 4)                                       \
  V(Float64, float64, FLOAT64, double, 8)                                      \
  V(Uint8Clamped, uint8_clamped, UINT8_CLAMPED, uint8_t, 1)


class FixedTypedArrayBase: public FixedArrayBase {
 public:
  // [base_pointer]: Either points to the FixedTypedArrayBase itself or nullptr.
  DECL_ACCESSORS(base_pointer, Object)

  // [external_pointer]: Contains the offset between base_pointer and the start
  // of the data. If the base_pointer is a nullptr, the external_pointer
  // therefore points to the actual backing store.
  DECL_ACCESSORS(external_pointer, void)

  // Dispatched behavior.
  DECL_CAST(FixedTypedArrayBase)

  static const int kBasePointerOffset = FixedArrayBase::kHeaderSize;
  static const int kExternalPointerOffset = kBasePointerOffset + kPointerSize;
  static const int kHeaderSize =
      DOUBLE_POINTER_ALIGN(kExternalPointerOffset + kPointerSize);

  static const int kDataOffset = kHeaderSize;

  static const int kMaxElementSize = 8;

#ifdef V8_HOST_ARCH_32_BIT
  static const size_t kMaxByteLength = std::numeric_limits<size_t>::max();
#else
  static const size_t kMaxByteLength =
      static_cast<size_t>(Smi::kMaxValue) * kMaxElementSize;
#endif  // V8_HOST_ARCH_32_BIT

  static const size_t kMaxLength = Smi::kMaxValue;

  class BodyDescriptor;
  // No weak fields.
  typedef BodyDescriptor BodyDescriptorWeak;

  inline int size() const;

  static inline int TypedArraySize(InstanceType type, int length);
  inline int TypedArraySize(InstanceType type) const;

  // Use with care: returns raw pointer into heap.
  inline void* DataPtr();

  inline int DataSize() const;

 private:
  static inline int ElementSize(InstanceType type);

  inline int DataSize(InstanceType type) const;

  DISALLOW_IMPLICIT_CONSTRUCTORS(FixedTypedArrayBase);
};


template <class Traits>
class FixedTypedArray: public FixedTypedArrayBase {
 public:
  typedef typename Traits::ElementType ElementType;
  static const InstanceType kInstanceType = Traits::kInstanceType;

  DECL_CAST(FixedTypedArray<Traits>)

  inline ElementType get_scalar(int index);
  static inline Handle<Object> get(FixedTypedArray* array, int index);
  inline void set(int index, ElementType value);

  static inline ElementType from(int value);
  static inline ElementType from(uint32_t value);
  static inline ElementType from(double value);

  // This accessor applies the correct conversion from Smi, HeapNumber
  // and undefined.
  inline void SetValue(uint32_t index, Object* value);

  DECL_PRINTER(FixedTypedArray)
  DECL_VERIFIER(FixedTypedArray)

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(FixedTypedArray);
};

#define FIXED_TYPED_ARRAY_TRAITS(Type, type, TYPE, elementType, size)    \
  STATIC_ASSERT(size <= FixedTypedArrayBase::kMaxElementSize);           \
  class Type##ArrayTraits {                                              \
   public: /* NOLINT */                                                  \
    typedef elementType ElementType;                                     \
    static const InstanceType kInstanceType = FIXED_##TYPE##_ARRAY_TYPE; \
    static const char* Designator() { return #type " array"; }           \
    static inline Handle<Object> ToHandle(Isolate* isolate,              \
                                          elementType scalar);           \
    static inline elementType defaultValue();                            \
  };                                                                     \
                                                                         \
  typedef FixedTypedArray<Type##ArrayTraits> Fixed##Type##Array;

TYPED_ARRAYS(FIXED_TYPED_ARRAY_TRAITS)

#undef FIXED_TYPED_ARRAY_TRAITS

// DeoptimizationInputData is a fixed array used to hold the deoptimization
// data for optimized code.  It also contains information about functions that
// were inlined.  If N different functions were inlined then first N elements of
// the literal array will contain these functions.
//
// It can be empty.
class DeoptimizationInputData: public FixedArray {
 public:
  // Layout description.  Indices in the array.
  static const int kTranslationByteArrayIndex = 0;
  static const int kInlinedFunctionCountIndex = 1;
  static const int kLiteralArrayIndex = 2;
  static const int kOsrBytecodeOffsetIndex = 3;
  static const int kOsrPcOffsetIndex = 4;
  static const int kOptimizationIdIndex = 5;
  static const int kSharedFunctionInfoIndex = 6;
  static const int kWeakCellCacheIndex = 7;
  static const int kInliningPositionsIndex = 8;
  static const int kFirstDeoptEntryIndex = 9;

  // Offsets of deopt entry elements relative to the start of the entry.
  static const int kBytecodeOffsetRawOffset = 0;
  static const int kTranslationIndexOffset = 1;
  static const int kTrampolinePcOffset = 2;
  static const int kPcOffset = 3;
  static const int kDeoptEntrySize = 4;

  // Simple element accessors.
#define DECL_ELEMENT_ACCESSORS(name, type) \
  inline type* name();                     \
  inline void Set##name(type* value);

  DECL_ELEMENT_ACCESSORS(TranslationByteArray, ByteArray)
  DECL_ELEMENT_ACCESSORS(InlinedFunctionCount, Smi)
  DECL_ELEMENT_ACCESSORS(LiteralArray, FixedArray)
  DECL_ELEMENT_ACCESSORS(OsrBytecodeOffset, Smi)
  DECL_ELEMENT_ACCESSORS(OsrPcOffset, Smi)
  DECL_ELEMENT_ACCESSORS(OptimizationId, Smi)
  DECL_ELEMENT_ACCESSORS(SharedFunctionInfo, Object)
  DECL_ELEMENT_ACCESSORS(WeakCellCache, Object)
  DECL_ELEMENT_ACCESSORS(InliningPositions, PodArray<InliningPosition>)

#undef DECL_ELEMENT_ACCESSORS

// Accessors for elements of the ith deoptimization entry.
#define DECL_ENTRY_ACCESSORS(name, type) \
  inline type* name(int i);              \
  inline void Set##name(int i, type* value);

  DECL_ENTRY_ACCESSORS(BytecodeOffsetRaw, Smi)
  DECL_ENTRY_ACCESSORS(TranslationIndex, Smi)
  DECL_ENTRY_ACCESSORS(TrampolinePc, Smi)
  DECL_ENTRY_ACCESSORS(Pc, Smi)

#undef DECL_ENTRY_ACCESSORS

  inline BailoutId BytecodeOffset(int i);

  inline void SetBytecodeOffset(int i, BailoutId value);

  inline int DeoptCount();

  static const int kNotInlinedIndex = -1;

  // Returns the inlined function at the given position in LiteralArray, or the
  // outer function if index == kNotInlinedIndex.
  class SharedFunctionInfo* GetInlinedFunction(int index);

  // Allocates a DeoptimizationInputData.
  static Handle<DeoptimizationInputData> New(Isolate* isolate,
                                             int deopt_entry_count,
                                             PretenureFlag pretenure);

  DECL_CAST(DeoptimizationInputData)

#ifdef ENABLE_DISASSEMBLER
  void DeoptimizationInputDataPrint(std::ostream& os);  // NOLINT
#endif

 private:
  static int IndexForEntry(int i) {
    return kFirstDeoptEntryIndex + (i * kDeoptEntrySize);
  }


  static int LengthFor(int entry_count) { return IndexForEntry(entry_count); }
};

class TemplateList : public FixedArray {
 public:
  static Handle<TemplateList> New(Isolate* isolate, int size);
  inline int length() const;
  inline Object* get(int index) const;
  inline void set(int index, Object* value);
  static Handle<TemplateList> Add(Isolate* isolate, Handle<TemplateList> list,
                                  Handle<Object> value);
  DECL_CAST(TemplateList)
 private:
  static const int kLengthIndex = 0;
  static const int kFirstElementIndex = kLengthIndex + 1;
  DISALLOW_IMPLICIT_CONSTRUCTORS(TemplateList);
};

// Code describes objects with on-the-fly generated machine code.
class Code: public HeapObject {
 public:
  // Opaque data type for encapsulating code flags like kind, inline
  // cache state, and arguments count.
  typedef uint32_t Flags;

#define NON_IC_KIND_LIST(V) \
  V(FUNCTION)               \
  V(OPTIMIZED_FUNCTION)     \
  V(BYTECODE_HANDLER)       \
  V(STUB)                   \
  V(HANDLER)                \
  V(BUILTIN)                \
  V(REGEXP)                 \
  V(WASM_FUNCTION)          \
  V(WASM_TO_JS_FUNCTION)    \
  V(JS_TO_WASM_FUNCTION)    \
  V(WASM_INTERPRETER_ENTRY)

#define IC_KIND_LIST(V) \
  V(LOAD_IC)            \
  V(LOAD_GLOBAL_IC)     \
  V(KEYED_LOAD_IC)      \
  V(STORE_IC)           \
  V(STORE_GLOBAL_IC)    \
  V(KEYED_STORE_IC)     \
  V(COMPARE_IC)

#define CODE_KIND_LIST(V) \
  NON_IC_KIND_LIST(V)     \
  IC_KIND_LIST(V)

  enum Kind {
#define DEFINE_CODE_KIND_ENUM(name) name,
    CODE_KIND_LIST(DEFINE_CODE_KIND_ENUM)
#undef DEFINE_CODE_KIND_ENUM
    NUMBER_OF_KINDS
  };

  static const char* Kind2String(Kind kind);

  static const int kPrologueOffsetNotSet = -1;

#if defined(OBJECT_PRINT) || defined(ENABLE_DISASSEMBLER)
  // Printing
  static const char* ICState2String(InlineCacheState state);
  static void PrintExtraICState(std::ostream& os,  // NOLINT
                                Kind kind, ExtraICState extra);
#endif  // defined(OBJECT_PRINT) || defined(ENABLE_DISASSEMBLER)

#ifdef ENABLE_DISASSEMBLER
  void Disassemble(const char* name, std::ostream& os);  // NOLINT
#endif  // ENABLE_DISASSEMBLER

  // [instruction_size]: Size of the native instructions
  inline int instruction_size() const;
  inline void set_instruction_size(int value);

  // [relocation_info]: Code relocation information
  DECL_ACCESSORS(relocation_info, ByteArray)
  void InvalidateRelocation();
  void InvalidateEmbeddedObjects();

  // [handler_table]: Fixed array containing offsets of exception handlers.
  DECL_ACCESSORS(handler_table, FixedArray)

  // [deoptimization_data]: Array containing data for deopt.
  DECL_ACCESSORS(deoptimization_data, FixedArray)

  // [source_position_table]: ByteArray for the source positions table or
  // SourcePositionTableWithFrameCache.
  DECL_ACCESSORS(source_position_table, Object)

  inline ByteArray* SourcePositionTable() const;

  // [trap_handler_index]: An index into the trap handler's master list of code
  // objects.
  DECL_ACCESSORS(trap_handler_index, Smi)

  // [raw_type_feedback_info]: This field stores various things, depending on
  // the kind of the code object.
  //   FUNCTION           => type feedback information.
  //   STUB and ICs       => major/minor key as Smi.
  DECL_ACCESSORS(raw_type_feedback_info, Object)
  inline Object* type_feedback_info() const;
  inline void set_type_feedback_info(
      Object* value, WriteBarrierMode mode = UPDATE_WRITE_BARRIER);
  inline uint32_t stub_key() const;
  inline void set_stub_key(uint32_t key);

  // [next_code_link]: Link for lists of optimized or deoptimized code.
  // Note that storage for this field is overlapped with typefeedback_info.
  DECL_ACCESSORS(next_code_link, Object)

  // [prologue_offset]: Offset of the function prologue, used for aging
  // FUNCTIONs and OPTIMIZED_FUNCTIONs.
  inline int prologue_offset() const;
  inline void set_prologue_offset(int offset);

  // [constant_pool offset]: Offset of the constant pool.
  // Valid for FLAG_enable_embedded_constant_pool only
  inline int constant_pool_offset() const;
  inline void set_constant_pool_offset(int offset);

  // Unchecked accessors to be used during GC.
  inline ByteArray* unchecked_relocation_info() const;

  inline int relocation_size() const;

  // [flags]: Various code flags.
  inline Flags flags() const;
  inline void set_flags(Flags flags);

  // [flags]: Access to specific code flags.
  inline Kind kind() const;
  inline ExtraICState extra_ic_state() const;  // Only valid for IC stubs.

  // Testers for IC stub kinds.
  inline bool is_inline_cache_stub() const;
  inline bool is_debug_stub() const;
  inline bool is_handler() const;
  inline bool is_stub() const;
  inline bool is_compare_ic_stub() const;
  inline bool is_optimized_code() const;
  inline bool is_wasm_code() const;

  inline bool IsCodeStubOrIC() const;

  inline void set_raw_kind_specific_flags1(int value);
  inline void set_raw_kind_specific_flags2(int value);

  // Testers for interpreter builtins.
  inline bool is_interpreter_trampoline_builtin() const;

  // Tells whether the code checks the optimization marker in the function's
  // feedback vector.
  inline bool checks_optimization_marker() const;

  // [is_crankshafted]: For kind STUB or ICs, tells whether or not a code
  // object was generated by either the hydrogen or the TurboFan optimizing
  // compiler (but it may not be an optimized function).
  inline bool is_crankshafted() const;
  inline bool is_hydrogen_stub() const;  // Crankshafted, but not a function.
  inline void set_is_crankshafted(bool value);

  // [has_tagged_params]: For compiled code or builtins: Tells whether the
  // outgoing parameters of this code are tagged pointers. True for other kinds.
  inline bool has_tagged_params() const;
  inline void set_has_tagged_params(bool value);

  // [is_turbofanned]: For kind STUB or OPTIMIZED_FUNCTION, tells whether the
  // code object was generated by the TurboFan optimizing compiler.
  inline bool is_turbofanned() const;
  inline void set_is_turbofanned(bool value);

  // [can_have_weak_objects]: For kind OPTIMIZED_FUNCTION, tells whether the
  // embedded objects in code should be treated weakly.
  inline bool can_have_weak_objects() const;
  inline void set_can_have_weak_objects(bool value);

  // [is_construct_stub]: For kind BUILTIN, tells whether the code object
  // represents a hand-written construct stub
  // (e.g., NumberConstructor_ConstructStub).
  inline bool is_construct_stub() const;
  inline void set_is_construct_stub(bool value);

  // [has_debug_break_slots]: For FUNCTION kind, tells if it has
  // been compiled with debug break slots.
  inline bool has_debug_break_slots() const;
  inline void set_has_debug_break_slots(bool value);

  // [has_reloc_info_for_serialization]: For FUNCTION kind, tells if its
  // reloc info includes runtime and external references to support
  // serialization/deserialization.
  inline bool has_reloc_info_for_serialization() const;
  inline void set_has_reloc_info_for_serialization(bool value);

  // [allow_osr_at_loop_nesting_level]: For FUNCTION kind, tells for
  // how long the function has been marked for OSR and therefore which
  // level of loop nesting we are willing to do on-stack replacement
  // for.
  inline void set_allow_osr_at_loop_nesting_level(int level);
  inline int allow_osr_at_loop_nesting_level() const;

  // [builtin_index]: For builtins, tells which builtin index the code object
  // has. Note that builtins can have a code kind other than BUILTIN. The
  // builtin index is a non-negative integer for builtins, and -1 otherwise.
  inline int builtin_index() const;
  inline void set_builtin_index(int id);

  // [stack_slots]: For kind OPTIMIZED_FUNCTION, the number of stack slots
  // reserved in the code prologue.
  inline unsigned stack_slots() const;
  inline void set_stack_slots(unsigned slots);

  // [safepoint_table_start]: For kind OPTIMIZED_FUNCTION, the offset in
  // the instruction stream where the safepoint table starts.
  inline unsigned safepoint_table_offset() const;
  inline void set_safepoint_table_offset(unsigned offset);

  // [back_edge_table_start]: For kind FUNCTION, the offset in the
  // instruction stream where the back edge table starts.
  inline unsigned back_edge_table_offset() const;
  inline void set_back_edge_table_offset(unsigned offset);

  inline bool back_edges_patched_for_osr() const;

  // [to_boolean_foo]: For kind TO_BOOLEAN_IC tells what state the stub is in.
  inline uint16_t to_boolean_state();

  // [marked_for_deoptimization]: For kind OPTIMIZED_FUNCTION tells whether
  // the code is going to be deoptimized because of dead embedded maps.
  inline bool marked_for_deoptimization() const;
  inline void set_marked_for_deoptimization(bool flag);

  // [deopt_already_counted]: For kind OPTIMIZED_FUNCTION tells whether
  // the code was already deoptimized.
  inline bool deopt_already_counted() const;
  inline void set_deopt_already_counted(bool flag);

  // [is_promise_rejection]: For kind BUILTIN tells whether the
  // exception thrown by the code will lead to promise rejection or
  // uncaught if both this and is_exception_caught is set.
  // Use GetBuiltinCatchPrediction to access this.
  inline void set_is_promise_rejection(bool flag);

  // [is_exception_caught]: For kind BUILTIN tells whether the
  // exception thrown by the code will be caught internally or
  // uncaught if both this and is_promise_rejection is set.
  // Use GetBuiltinCatchPrediction to access this.
  inline void set_is_exception_caught(bool flag);

  // [constant_pool]: The constant pool for this function.
  inline Address constant_pool();

  // Get the safepoint entry for the given pc.
  SafepointEntry GetSafepointEntry(Address pc);

  // Find an object in a stub with a specified map
  Object* FindNthObject(int n, Map* match_map);

  // Find the first allocation site in an IC stub.
  AllocationSite* FindFirstAllocationSite();

  // Find the first map in an IC stub.
  Map* FindFirstMap();

  // For each (map-to-find, object-to-replace) pair in the pattern, this
  // function replaces the corresponding placeholder in the code with the
  // object-to-replace. The function assumes that pairs in the pattern come in
  // the same order as the placeholders in the code.
  // If the placeholder is a weak cell, then the value of weak cell is matched
  // against the map-to-find.
  void FindAndReplace(const FindAndReplacePattern& pattern);

  // The entire code object including its header is copied verbatim to the
  // snapshot so that it can be written in one, fast, memcpy during
  // deserialization. The deserializer will overwrite some pointers, rather
  // like a runtime linker, but the random allocation addresses used in the
  // mksnapshot process would still be present in the unlinked snapshot data,
  // which would make snapshot production non-reproducible. This method wipes
  // out the to-be-overwritten header data for reproducible snapshots.
  inline void WipeOutHeader();

  // Flags operations.
  static inline Flags ComputeFlags(
      Kind kind, ExtraICState extra_ic_state = kNoExtraICState);

  static inline Flags ComputeHandlerFlags(Kind handler_kind);

  static inline Kind ExtractKindFromFlags(Flags flags);
  static inline ExtraICState ExtractExtraICStateFromFlags(Flags flags);

  // Convert a target address into a code object.
  static inline Code* GetCodeFromTargetAddress(Address address);

  // Convert an entry address into an object.
  static inline Object* GetObjectFromEntryAddress(Address location_of_address);

  // Convert a code entry into an object.
  static inline Object* GetObjectFromCodeEntry(Address code_entry);

  // Returns the address of the first instruction.
  inline byte* instruction_start() const;

  // Returns the address right after the last instruction.
  inline byte* instruction_end() const;

  // Returns the size of the instructions, padding, relocation and unwinding
  // information.
  inline int body_size() const;

  // Returns the size of code and its metadata. This includes the size of code
  // relocation information, deoptimization data and handler table.
  inline int SizeIncludingMetadata() const;

  // Returns the address of the first relocation info (read backwards!).
  inline byte* relocation_start() const;

  // [has_unwinding_info]: Whether this code object has unwinding information.
  // If it doesn't, unwinding_information_start() will point to invalid data.
  //
  // The body of all code objects has the following layout.
  //
  //  +--------------------------+  <-- instruction_start()
  //  |       instructions       |
  //  |           ...            |
  //  +--------------------------+
  //  |      relocation info     |
  //  |           ...            |
  //  +--------------------------+  <-- instruction_end()
  //
  // If has_unwinding_info() is false, instruction_end() points to the first
  // memory location after the end of the code object. Otherwise, the body
  // continues as follows:
  //
  //  +--------------------------+
  //  |    padding to the next   |
  //  |  8-byte aligned address  |
  //  +--------------------------+  <-- instruction_end()
  //  |   [unwinding_info_size]  |
  //  |        as uint64_t       |
  //  +--------------------------+  <-- unwinding_info_start()
  //  |       unwinding info     |
  //  |            ...           |
  //  +--------------------------+  <-- unwinding_info_end()
  //
  // and unwinding_info_end() points to the first memory location after the end
  // of the code object.
  //
  DECL_BOOLEAN_ACCESSORS(has_unwinding_info)

  // [unwinding_info_size]: Size of the unwinding information.
  inline int unwinding_info_size() const;
  inline void set_unwinding_info_size(int value);

  // Returns the address of the unwinding information, if any.
  inline byte* unwinding_info_start() const;

  // Returns the address right after the end of the unwinding information.
  inline byte* unwinding_info_end() const;

  // Code entry point.
  inline byte* entry() const;

  // Returns true if pc is inside this object's instructions.
  inline bool contains(byte* pc);

  // Relocate the code by delta bytes. Called to signal that this code
  // object has been moved by delta bytes.
  void Relocate(intptr_t delta);

  // Migrate code described by desc.
  void CopyFrom(const CodeDesc& desc);

  // Returns the object size for a given body (used for allocation).
  static int SizeFor(int body_size) {
    DCHECK_SIZE_TAG_ALIGNED(body_size);
    return RoundUp(kHeaderSize + body_size, kCodeAlignment);
  }

  // Calculate the size of the code object to report for log events. This takes
  // the layout of the code object into account.
  inline int ExecutableSize() const;

  DECL_CAST(Code)

  // Dispatched behavior.
  inline int CodeSize() const;

  DECL_PRINTER(Code)
  DECL_VERIFIER(Code)

  void ClearInlineCaches();

  BailoutId TranslatePcOffsetToBytecodeOffset(uint32_t pc_offset);
  uint32_t TranslateBytecodeOffsetToPcOffset(BailoutId bytecode_offset);

#define DECL_CODE_AGE_ENUM(X) k##X##CodeAge,
  enum Age {
    kToBeExecutedOnceCodeAge = -3,
    kNotExecutedCodeAge = -2,
    kExecutedOnceCodeAge = -1,
    kNoAgeCodeAge = 0,
    CODE_AGE_LIST(DECL_CODE_AGE_ENUM) kAfterLastCodeAge,
    kFirstCodeAge = kToBeExecutedOnceCodeAge,
    kLastCodeAge = kAfterLastCodeAge - 1,
    kCodeAgeCount = kAfterLastCodeAge - kFirstCodeAge - 1,
    kIsOldCodeAge = kSexagenarianCodeAge,
    kPreAgedCodeAge = kIsOldCodeAge - 1
  };
#undef DECL_CODE_AGE_ENUM

  // Code aging.  Indicates how many full GCs this code has survived without
  // being entered through the prologue.  Used to determine when to flush code
  // held in the compilation cache.
  static void MakeCodeAgeSequenceYoung(byte* sequence, Isolate* isolate);
  static void MarkCodeAsExecuted(byte* sequence, Isolate* isolate);
  void MakeYoung(Isolate* isolate);
  void PreAge(Isolate* isolate);
  void MarkToBeExecutedOnce(Isolate* isolate);
  void MakeOlder();
  static bool IsYoungSequence(Isolate* isolate, byte* sequence);
  bool IsOld();
  Age GetAge();
  static inline Code* GetPreAgedCodeAgeStub(Isolate* isolate) {
    return GetCodeAgeStub(isolate, kNotExecutedCodeAge);
  }

  void PrintDeoptLocation(FILE* out, Address pc);
  bool CanDeoptAt(Address pc);

  inline HandlerTable::CatchPrediction GetBuiltinCatchPrediction();
#ifdef VERIFY_HEAP
  void VerifyEmbeddedObjectsDependency();
#endif

#ifdef DEBUG
  enum VerifyMode { kNoContextSpecificPointers, kNoContextRetainingPointers };
  void VerifyEmbeddedObjects(VerifyMode mode = kNoContextRetainingPointers);
  static void VerifyRecompiledCode(Code* old_code, Code* new_code);
#endif  // DEBUG

  inline bool CanContainWeakObjects();

  inline bool IsWeakObject(Object* object);

  static inline bool IsWeakObjectInOptimizedCode(Object* object);

  static Handle<WeakCell> WeakCellFor(Handle<Code> code);
  WeakCell* CachedWeakCell();

  static const int kConstantPoolSize =
      FLAG_enable_embedded_constant_pool ? kIntSize : 0;

  // Layout description.
  static const int kRelocationInfoOffset = HeapObject::kHeaderSize;
  static const int kHandlerTableOffset = kRelocationInfoOffset + kPointerSize;
  static const int kDeoptimizationDataOffset =
      kHandlerTableOffset + kPointerSize;
  static const int kSourcePositionTableOffset =
      kDeoptimizationDataOffset + kPointerSize;
  // For FUNCTION kind, we store the type feedback info here.
  static const int kTypeFeedbackInfoOffset =
      kSourcePositionTableOffset + kPointerSize;
  static const int kNextCodeLinkOffset = kTypeFeedbackInfoOffset + kPointerSize;
  static const int kInstructionSizeOffset = kNextCodeLinkOffset + kPointerSize;
  static const int kFlagsOffset = kInstructionSizeOffset + kIntSize;
  static const int kKindSpecificFlags1Offset = kFlagsOffset + kIntSize;
  static const int kKindSpecificFlags2Offset =
      kKindSpecificFlags1Offset + kIntSize;
  // Note: We might be able to squeeze this into the flags above.
  static const int kPrologueOffset = kKindSpecificFlags2Offset + kIntSize;
  static const int kConstantPoolOffset = kPrologueOffset + kIntSize;
  static const int kBuiltinIndexOffset =
      kConstantPoolOffset + kConstantPoolSize;
  static const int kTrapHandlerIndex = kBuiltinIndexOffset + kIntSize;
  static const int kHeaderPaddingStart = kTrapHandlerIndex + kPointerSize;

  enum TrapFields { kTrapCodeOffset, kTrapLandingOffset, kTrapDataSize };


  // Add padding to align the instruction start following right after
  // the Code object header.
  static const int kHeaderSize =
      (kHeaderPaddingStart + kCodeAlignmentMask) & ~kCodeAlignmentMask;

  inline int GetUnwindingInfoSizeOffset() const;

  class BodyDescriptor;

  // Flags layout.  BitField<type, shift, size>.
  class HasUnwindingInfoField : public BitField<bool, 0, 1> {};
  class KindField : public BitField<Kind, HasUnwindingInfoField::kNext, 5> {};
  STATIC_ASSERT(NUMBER_OF_KINDS <= KindField::kMax);
  class ExtraICStateField
      : public BitField<ExtraICState, KindField::kNext,
                        PlatformSmiTagging::kSmiValueSize - KindField::kNext> {
  };

  // KindSpecificFlags1 layout (FUNCTION)
  static const int kFullCodeFlags = kKindSpecificFlags1Offset;
  static const int kFullCodeFlagsHasDebugBreakSlotsField = 0;
  static const int kFullCodeFlagsHasRelocInfoForSerialization = 1;
  class FullCodeFlagsHasDebugBreakSlotsField
      : public BitField<bool, kFullCodeFlagsHasDebugBreakSlotsField, 1> {};
  class FullCodeFlagsHasRelocInfoForSerialization
      : public BitField<bool, kFullCodeFlagsHasRelocInfoForSerialization, 1> {};

  // KindSpecificFlags1 layout (STUB, BUILTIN and OPTIMIZED_FUNCTION)
  static const int kStackSlotsFirstBit = 0;
  static const int kStackSlotsBitCount = 24;
  static const int kMarkedForDeoptimizationBit =
      kStackSlotsFirstBit + kStackSlotsBitCount;
  static const int kDeoptAlreadyCountedBit = kMarkedForDeoptimizationBit + 1;
  static const int kIsTurbofannedBit = kDeoptAlreadyCountedBit + 1;
  static const int kCanHaveWeakObjects = kIsTurbofannedBit + 1;
  // Could be moved to overlap previous bits when we need more space.
  static const int kIsConstructStub = kCanHaveWeakObjects + 1;
  static const int kIsPromiseRejection = kIsConstructStub + 1;
  static const int kIsExceptionCaught = kIsPromiseRejection + 1;

  STATIC_ASSERT(kStackSlotsFirstBit + kStackSlotsBitCount <= 32);
  STATIC_ASSERT(kIsExceptionCaught + 1 <= 32);

  class StackSlotsField: public BitField<int,
      kStackSlotsFirstBit, kStackSlotsBitCount> {};  // NOLINT
  class MarkedForDeoptimizationField
      : public BitField<bool, kMarkedForDeoptimizationBit, 1> {};  // NOLINT
  class DeoptAlreadyCountedField
      : public BitField<bool, kDeoptAlreadyCountedBit, 1> {};  // NOLINT
  class IsTurbofannedField : public BitField<bool, kIsTurbofannedBit, 1> {
  };  // NOLINT
  class CanHaveWeakObjectsField
      : public BitField<bool, kCanHaveWeakObjects, 1> {};  // NOLINT
  class IsConstructStubField : public BitField<bool, kIsConstructStub, 1> {
  };  // NOLINT
  class IsPromiseRejectionField
      : public BitField<bool, kIsPromiseRejection, 1> {};  // NOLINT
  class IsExceptionCaughtField : public BitField<bool, kIsExceptionCaught, 1> {
  };  // NOLINT

  // KindSpecificFlags2 layout (ALL)
  static const int kIsCrankshaftedBit = 0;
  class IsCrankshaftedField : public BitField<bool, kIsCrankshaftedBit, 1> {};
  static const int kHasTaggedStackBit = kIsCrankshaftedBit + 1;
  class HasTaggedStackField : public BitField<bool, kHasTaggedStackBit, 1> {};

  // KindSpecificFlags2 layout (STUB and OPTIMIZED_FUNCTION)
  static const int kSafepointTableOffsetFirstBit = kHasTaggedStackBit + 1;
  static const int kSafepointTableOffsetBitCount = 30;

  STATIC_ASSERT(kSafepointTableOffsetFirstBit +
                kSafepointTableOffsetBitCount <= 32);
  STATIC_ASSERT(1 + kSafepointTableOffsetBitCount <= 32);

  class SafepointTableOffsetField: public BitField<int,
      kSafepointTableOffsetFirstBit,
      kSafepointTableOffsetBitCount> {};  // NOLINT

  // KindSpecificFlags2 layout (FUNCTION)
  class BackEdgeTableOffsetField: public BitField<int,
      kIsCrankshaftedBit + 1, 27> {};  // NOLINT
  class AllowOSRAtLoopNestingLevelField: public BitField<int,
      kIsCrankshaftedBit + 1 + 27, 4> {};  // NOLINT

  static const int kArgumentsBits = 16;
  static const int kMaxArguments = (1 << kArgumentsBits) - 1;

 private:
  friend class RelocIterator;
  friend class Deoptimizer;  // For FindCodeAgeSequence.

  // Code aging
  byte* FindCodeAgeSequence();
  static Age GetCodeAge(Isolate* isolate, byte* sequence);
  static Age GetAgeOfCodeAgeStub(Code* code);
  static Code* GetCodeAgeStub(Isolate* isolate, Age age);

  // Code aging -- platform-specific
  static void PatchPlatformCodeAge(Isolate* isolate, byte* sequence, Age age);

  bool is_promise_rejection() const;
  bool is_exception_caught() const;

  DISALLOW_IMPLICIT_CONSTRUCTORS(Code);
};

class AbstractCode : public HeapObject {
 public:
  // All code kinds and INTERPRETED_FUNCTION.
  enum Kind {
#define DEFINE_CODE_KIND_ENUM(name) name,
    CODE_KIND_LIST(DEFINE_CODE_KIND_ENUM)
#undef DEFINE_CODE_KIND_ENUM
        INTERPRETED_FUNCTION,
    NUMBER_OF_KINDS
  };

  static const char* Kind2String(Kind kind);

  int SourcePosition(int offset);
  int SourceStatementPosition(int offset);

  // Returns the address of the first instruction.
  inline Address instruction_start();

  // Returns the address right after the last instruction.
  inline Address instruction_end();

  // Returns the size of the code instructions.
  inline int instruction_size();

  // Return the source position table.
  inline ByteArray* source_position_table();

  // Set the source position table.
  inline void set_source_position_table(ByteArray* source_position_table);

  inline Object* stack_frame_cache();
  static void SetStackFrameCache(Handle<AbstractCode> abstract_code,
                                 Handle<UnseededNumberDictionary> cache);
  void DropStackFrameCache();

  // Returns the size of instructions and the metadata.
  inline int SizeIncludingMetadata();

  // Returns true if pc is inside this object's instructions.
  inline bool contains(byte* pc);

  // Returns the AbstractCode::Kind of the code.
  inline Kind kind();

  // Calculate the size of the code object to report for log events. This takes
  // the layout of the code object into account.
  inline int ExecutableSize();

  DECL_CAST(AbstractCode)
  inline Code* GetCode();
  inline BytecodeArray* GetBytecodeArray();

  // Max loop nesting marker used to postpose OSR. We don't take loop
  // nesting that is deeper than 5 levels into account.
  static const int kMaxLoopNestingMarker = 6;
  STATIC_ASSERT(Code::AllowOSRAtLoopNestingLevelField::kMax >=
                kMaxLoopNestingMarker);
};

// Dependent code is a singly linked list of fixed arrays. Each array contains
// code objects in weak cells for one dependent group. The suffix of the array
// can be filled with the undefined value if the number of codes is less than
// the length of the array.
//
// +------+-----------------+--------+--------+-----+--------+-----------+-----+
// | next | count & group 1 | code 1 | code 2 | ... | code n | undefined | ... |
// +------+-----------------+--------+--------+-----+--------+-----------+-----+
//    |
//    V
// +------+-----------------+--------+--------+-----+--------+-----------+-----+
// | next | count & group 2 | code 1 | code 2 | ... | code m | undefined | ... |
// +------+-----------------+--------+--------+-----+--------+-----------+-----+
//    |
//    V
// empty_fixed_array()
//
// The list of fixed arrays is ordered by dependency groups.

class DependentCode: public FixedArray {
 public:
  enum DependencyGroup {
    // Group of code that weakly embed this map and depend on being
    // deoptimized when the map is garbage collected.
    kWeakCodeGroup,
    // Group of code that embed a transition to this map, and depend on being
    // deoptimized when the transition is replaced by a new version.
    kTransitionGroup,
    // Group of code that omit run-time prototype checks for prototypes
    // described by this map. The group is deoptimized whenever an object
    // described by this map changes shape (and transitions to a new map),
    // possibly invalidating the assumptions embedded in the code.
    kPrototypeCheckGroup,
    // Group of code that depends on global property values in property cells
    // not being changed.
    kPropertyCellChangedGroup,
    // Group of code that omit run-time checks for field(s) introduced by
    // this map, i.e. for the field type.
    kFieldOwnerGroup,
    // Group of code that omit run-time type checks for initial maps of
    // constructors.
    kInitialMapChangedGroup,
    // Group of code that depends on tenuring information in AllocationSites
    // not being changed.
    kAllocationSiteTenuringChangedGroup,
    // Group of code that depends on element transition information in
    // AllocationSites not being changed.
    kAllocationSiteTransitionChangedGroup
  };

  static const int kGroupCount = kAllocationSiteTransitionChangedGroup + 1;
  static const int kNextLinkIndex = 0;
  static const int kFlagsIndex = 1;
  static const int kCodesStartIndex = 2;

  bool Contains(DependencyGroup group, WeakCell* code_cell);
  bool IsEmpty(DependencyGroup group);

  static Handle<DependentCode> InsertCompilationDependencies(
      Handle<DependentCode> entries, DependencyGroup group,
      Handle<Foreign> info);

  static Handle<DependentCode> InsertWeakCode(Handle<DependentCode> entries,
                                              DependencyGroup group,
                                              Handle<WeakCell> code_cell);

  void UpdateToFinishedCode(DependencyGroup group, Foreign* info,
                            WeakCell* code_cell);

  void RemoveCompilationDependencies(DependentCode::DependencyGroup group,
                                     Foreign* info);

  void DeoptimizeDependentCodeGroup(Isolate* isolate,
                                    DependentCode::DependencyGroup group);

  bool MarkCodeForDeoptimization(Isolate* isolate,
                                 DependentCode::DependencyGroup group);

  // The following low-level accessors should only be used by this class
  // and the mark compact collector.
  inline DependentCode* next_link();
  inline void set_next_link(DependentCode* next);
  inline int count();
  inline void set_count(int value);
  inline DependencyGroup group();
  inline void set_group(DependencyGroup group);
  inline Object* object_at(int i);
  inline void set_object_at(int i, Object* object);
  inline void clear_at(int i);
  inline void copy(int from, int to);
  DECL_CAST(DependentCode)

  static const char* DependencyGroupName(DependencyGroup group);
  static void SetMarkedForDeoptimization(Code* code, DependencyGroup group);

 private:
  static Handle<DependentCode> Insert(Handle<DependentCode> entries,
                                      DependencyGroup group,
                                      Handle<Object> object);
  static Handle<DependentCode> New(DependencyGroup group, Handle<Object> object,
                                   Handle<DependentCode> next);
  static Handle<DependentCode> EnsureSpace(Handle<DependentCode> entries);
  // Compact by removing cleared weak cells and return true if there was
  // any cleared weak cell.
  bool Compact();
  static int Grow(int number_of_entries) {
    if (number_of_entries < 5) return number_of_entries + 1;
    return number_of_entries * 5 / 4;
  }
  inline int flags();
  inline void set_flags(int flags);
  class GroupField : public BitField<int, 0, 3> {};
  class CountField : public BitField<int, 3, 27> {};
  STATIC_ASSERT(kGroupCount <= GroupField::kMax + 1);
};

class PrototypeInfo;

// An abstract superclass, a marker class really, for simple structure classes.
// It doesn't carry much functionality but allows struct classes to be
// identified in the type system.
class Struct: public HeapObject {
 public:
  inline void InitializeBody(int object_size);
  DECL_CAST(Struct)
};

// A container struct to hold state required for PromiseResolveThenableJob.
class PromiseResolveThenableJobInfo : public Struct {
 public:
  DECL_ACCESSORS(thenable, JSReceiver)
  DECL_ACCESSORS(then, JSReceiver)
  DECL_ACCESSORS(resolve, JSFunction)
  DECL_ACCESSORS(reject, JSFunction)

  DECL_ACCESSORS(context, Context)

  static const int kThenableOffset = Struct::kHeaderSize;
  static const int kThenOffset = kThenableOffset + kPointerSize;
  static const int kResolveOffset = kThenOffset + kPointerSize;
  static const int kRejectOffset = kResolveOffset + kPointerSize;
  static const int kContextOffset = kRejectOffset + kPointerSize;
  static const int kSize = kContextOffset + kPointerSize;

  DECL_CAST(PromiseResolveThenableJobInfo)
  DECL_PRINTER(PromiseResolveThenableJobInfo)
  DECL_VERIFIER(PromiseResolveThenableJobInfo)

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(PromiseResolveThenableJobInfo);
};

class JSPromise;

// Struct to hold state required for PromiseReactionJob.
class PromiseReactionJobInfo : public Struct {
 public:
  DECL_ACCESSORS(value, Object)
  DECL_ACCESSORS(tasks, Object)

  // Check comment in JSPromise for information on what state these
  // deferred fields could be in.
  DECL_ACCESSORS(deferred_promise, Object)
  DECL_ACCESSORS(deferred_on_resolve, Object)
  DECL_ACCESSORS(deferred_on_reject, Object)

  DECL_INT_ACCESSORS(debug_id)

  DECL_ACCESSORS(context, Context)

  static const int kValueOffset = Struct::kHeaderSize;
  static const int kTasksOffset = kValueOffset + kPointerSize;
  static const int kDeferredPromiseOffset = kTasksOffset + kPointerSize;
  static const int kDeferredOnResolveOffset =
      kDeferredPromiseOffset + kPointerSize;
  static const int kDeferredOnRejectOffset =
      kDeferredOnResolveOffset + kPointerSize;
  static const int kContextOffset = kDeferredOnRejectOffset + kPointerSize;
  static const int kSize = kContextOffset + kPointerSize;

  DECL_CAST(PromiseReactionJobInfo)
  DECL_PRINTER(PromiseReactionJobInfo)
  DECL_VERIFIER(PromiseReactionJobInfo)

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(PromiseReactionJobInfo);
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

// Container for metadata stored on each prototype map.
class PrototypeInfo : public Struct {
 public:
  static const int UNREGISTERED = -1;

  // [weak_cell]: A WeakCell containing this prototype. ICs cache the cell here.
  DECL_ACCESSORS(weak_cell, Object)

  // [prototype_users]: WeakFixedArray containing maps using this prototype,
  // or Smi(0) if uninitialized.
  DECL_ACCESSORS(prototype_users, Object)

  // [object_create_map]: A field caching the map for Object.create(prototype).
  static inline void SetObjectCreateMap(Handle<PrototypeInfo> info,
                                        Handle<Map> map);
  inline Map* ObjectCreateMap();
  inline bool HasObjectCreateMap();

  // [registry_slot]: Slot in prototype's user registry where this user
  // is stored. Returns UNREGISTERED if this prototype has not been registered.
  inline int registry_slot() const;
  inline void set_registry_slot(int slot);
  // [validity_cell]: Cell containing the validity bit for prototype chains
  // going through this object, or Smi(0) if uninitialized.
  // When a prototype object changes its map, then both its own validity cell
  // and those of all "downstream" prototypes are invalidated; handlers for a
  // given receiver embed the currently valid cell for that receiver's prototype
  // during their compilation and check it on execution.
  DECL_ACCESSORS(validity_cell, Object)
  // [bit_field]
  inline int bit_field() const;
  inline void set_bit_field(int bit_field);

  DECL_BOOLEAN_ACCESSORS(should_be_fast_map)

  DECL_CAST(PrototypeInfo)

  // Dispatched behavior.
  DECL_PRINTER(PrototypeInfo)
  DECL_VERIFIER(PrototypeInfo)

  static const int kWeakCellOffset = HeapObject::kHeaderSize;
  static const int kPrototypeUsersOffset = kWeakCellOffset + kPointerSize;
  static const int kRegistrySlotOffset = kPrototypeUsersOffset + kPointerSize;
  static const int kValidityCellOffset = kRegistrySlotOffset + kPointerSize;
  static const int kObjectCreateMap = kValidityCellOffset + kPointerSize;
  static const int kBitFieldOffset = kObjectCreateMap + kPointerSize;
  static const int kSize = kBitFieldOffset + kPointerSize;

  // Bit field usage.
  static const int kShouldBeFastBit = 0;

 private:
  DECL_ACCESSORS(object_create_map, Object)

  DISALLOW_IMPLICIT_CONSTRUCTORS(PrototypeInfo);
};

class Tuple2 : public Struct {
 public:
  DECL_ACCESSORS(value1, Object)
  DECL_ACCESSORS(value2, Object)

  DECL_CAST(Tuple2)

  // Dispatched behavior.
  DECL_PRINTER(Tuple2)
  DECL_VERIFIER(Tuple2)

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

  static const int kValue3Offset = Tuple2::kSize;
  static const int kSize = kValue3Offset + kPointerSize;

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(Tuple3);
};

// Pair used to store both a ScopeInfo and an extension object in the extension
// slot of a block, catch, or with context. Needed in the rare case where a
// declaration block scope (a "varblock" as used to desugar parameter
// destructuring) also contains a sloppy direct eval, or for with and catch
// scopes. (In no other case both are needed at the same time.)
class ContextExtension : public Struct {
 public:
  // [scope_info]: Scope info.
  DECL_ACCESSORS(scope_info, ScopeInfo)
  // [extension]: Extension object.
  DECL_ACCESSORS(extension, Object)

  DECL_CAST(ContextExtension)

  // Dispatched behavior.
  DECL_PRINTER(ContextExtension)
  DECL_VERIFIER(ContextExtension)

  static const int kScopeInfoOffset = HeapObject::kHeaderSize;
  static const int kExtensionOffset = kScopeInfoOffset + kPointerSize;
  static const int kSize = kExtensionOffset + kPointerSize;

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(ContextExtension);
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
  V(String.prototype, trimLeft, StringTrimLeft)             \
  V(String.prototype, trimRight, StringTrimRight)           \
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
  kArrayCode,
#define DECL_FUNCTION_ID(ignored1, ignore2, name) k##name,
  FUNCTIONS_WITH_ID_LIST(DECL_FUNCTION_ID)
      ATOMIC_FUNCTIONS_WITH_ID_LIST(DECL_FUNCTION_ID)
#undef DECL_FUNCTION_ID
  // Fake id for a special case of Math.pow. Note, it continues the
  // list of math functions.
  kMathPowHalf,
  // These are manually assigned to special getters during bootstrapping.
  kArrayBufferByteLength,
  kArrayEntries,
  kArrayKeys,
  kArrayValues,
  kArrayIteratorNext,
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
  kTypedArrayByteLength,
  kTypedArrayByteOffset,
  kTypedArrayEntries,
  kTypedArrayKeys,
  kTypedArrayLength,
  kTypedArrayValues,
  kSharedArrayBufferByteLength,
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

  // [register_file]: Saved interpreter register file.
  DECL_ACCESSORS(register_file, FixedArray)

  DECL_CAST(JSGeneratorObject)

  // Dispatched behavior.
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
  static const int kRegisterFileOffset = kContinuationOffset + kPointerSize;
  static const int kSize = kRegisterFileOffset + kPointerSize;

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

  // [awaited_promise]
  // A reference to the Promise of an AwaitExpression.
  DECL_ACCESSORS(awaited_promise, HeapObject)

  // Layout description.
  static const int kQueueOffset = JSGeneratorObject::kSize;
  static const int kAwaitedPromiseOffset = kQueueOffset + kPointerSize;
  static const int kSize = kAwaitedPromiseOffset + kPointerSize;

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(JSAsyncGeneratorObject);
};

// When importing a module namespace (import * as foo from "bar"), a
// JSModuleNamespace object (representing module "bar") is created and bound to
// the declared variable (foo).  A module can have at most one namespace object.
class JSModuleNamespace : public JSObject {
 public:
  DECL_CAST(JSModuleNamespace)
  DECL_PRINTER(JSModuleNamespace)
  DECL_VERIFIER(JSModuleNamespace)

  // The actual module whose namespace is being represented.
  DECL_ACCESSORS(module, Module)

  // Retrieve the value exported by [module] under the given [name]. If there is
  // no such export, return Just(undefined). If the export is uninitialized,
  // schedule an exception and return Nothing.
  MUST_USE_RESULT MaybeHandle<Object> GetExport(Handle<String> name);

  // In-object fields.
  enum {
    kToStringTagFieldIndex,
    kInObjectFieldCount,
  };

  static const int kModuleOffset = JSObject::kHeaderSize;
  static const int kHeaderSize = kModuleOffset + kPointerSize;

  static const int kSize = kHeaderSize + kPointerSize * kInObjectFieldCount;

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(JSModuleNamespace);
};

// A Module object is a mapping from export names to cells
// This is still very much in flux.
class Module : public Struct {
 public:
  DECL_CAST(Module)
  DECL_VERIFIER(Module)
  DECL_PRINTER(Module)

  // The code representing this Module, or an abstraction thereof.
  // This is either a SharedFunctionInfo or a JSFunction or a ModuleInfo
  // depending on whether the module has been instantiated and evaluated.  See
  // Module::ModuleVerify() for the precise invariant.
  DECL_ACCESSORS(code, Object)

  // Arrays of cells corresponding to regular exports and regular imports.
  // A cell's position in the array is determined by the cell index of the
  // associated module entry (which coincides with the variable index of the
  // associated variable).
  DECL_ACCESSORS(regular_exports, FixedArray)
  DECL_ACCESSORS(regular_imports, FixedArray)

  // The complete export table, mapping an export name to its cell.
  // TODO(neis): We may want to remove the regular exports from the table.
  DECL_ACCESSORS(exports, ObjectHashTable)

  // Hash for this object (a random non-zero Smi).
  DECL_INT_ACCESSORS(hash)

  // Status.
  DECL_INT_ACCESSORS(status)
  enum Status {
    // Order matters!
    kUninstantiated,
    kPreInstantiating,
    kInstantiating,
    kInstantiated,
    kEvaluating,
    kEvaluated,
    kErrored
  };

  // The exception in the case {status} is kErrored.
  Object* GetException();

  // The namespace object (or undefined).
  DECL_ACCESSORS(module_namespace, HeapObject)

  // Modules imported or re-exported by this module.
  // Corresponds 1-to-1 to the module specifier strings in
  // ModuleInfo::module_requests.
  DECL_ACCESSORS(requested_modules, FixedArray)

  // [script]: Script from which the module originates.
  DECL_ACCESSORS(script, Script)

  // Get the ModuleInfo associated with the code.
  inline ModuleInfo* info() const;

  // Implementation of spec operation ModuleDeclarationInstantiation.
  // Returns false if an exception occurred during instantiation, true
  // otherwise. (In the case where the callback throws an exception, that
  // exception is propagated.)
  static MUST_USE_RESULT bool Instantiate(Handle<Module> module,
                                          v8::Local<v8::Context> context,
                                          v8::Module::ResolveCallback callback);

  // Implementation of spec operation ModuleEvaluation.
  static MUST_USE_RESULT MaybeHandle<Object> Evaluate(Handle<Module> module);

  Cell* GetCell(int cell_index);
  static Handle<Object> LoadVariable(Handle<Module> module, int cell_index);
  static void StoreVariable(Handle<Module> module, int cell_index,
                            Handle<Object> value);

  // Get the namespace object for [module_request] of [module].  If it doesn't
  // exist yet, it is created.
  static Handle<JSModuleNamespace> GetModuleNamespace(Handle<Module> module,
                                                      int module_request);

  // Get the namespace object for [module].  If it doesn't exist yet, it is
  // created.
  static Handle<JSModuleNamespace> GetModuleNamespace(Handle<Module> module);

  static const int kCodeOffset = HeapObject::kHeaderSize;
  static const int kExportsOffset = kCodeOffset + kPointerSize;
  static const int kRegularExportsOffset = kExportsOffset + kPointerSize;
  static const int kRegularImportsOffset = kRegularExportsOffset + kPointerSize;
  static const int kHashOffset = kRegularImportsOffset + kPointerSize;
  static const int kModuleNamespaceOffset = kHashOffset + kPointerSize;
  static const int kRequestedModulesOffset =
      kModuleNamespaceOffset + kPointerSize;
  static const int kStatusOffset = kRequestedModulesOffset + kPointerSize;
  static const int kDfsIndexOffset = kStatusOffset + kPointerSize;
  static const int kDfsAncestorIndexOffset = kDfsIndexOffset + kPointerSize;
  static const int kExceptionOffset = kDfsAncestorIndexOffset + kPointerSize;
  static const int kScriptOffset = kExceptionOffset + kPointerSize;
  static const int kSize = kScriptOffset + kPointerSize;

 private:
  friend class Factory;

  DECL_ACCESSORS(exception, Object)

  // TODO(neis): Don't store those in the module object?
  DECL_INT_ACCESSORS(dfs_index)
  DECL_INT_ACCESSORS(dfs_ancestor_index)

  // Helpers for Instantiate and Evaluate.

  static void CreateExport(Handle<Module> module, int cell_index,
                           Handle<FixedArray> names);
  static void CreateIndirectExport(Handle<Module> module, Handle<String> name,
                                   Handle<ModuleInfoEntry> entry);

  // The [must_resolve] argument indicates whether or not an exception should be
  // thrown in case the module does not provide an export named [name]
  // (including when a cycle is detected).  An exception is always thrown in the
  // case of conflicting star exports.
  //
  // If [must_resolve] is true, a null result indicates an exception. If
  // [must_resolve] is false, a null result may or may not indicate an
  // exception (so check manually!).
  class ResolveSet;
  static MUST_USE_RESULT MaybeHandle<Cell> ResolveExport(
      Handle<Module> module, Handle<String> name, MessageLocation loc,
      bool must_resolve, ResolveSet* resolve_set);
  static MUST_USE_RESULT MaybeHandle<Cell> ResolveImport(
      Handle<Module> module, Handle<String> name, int module_request,
      MessageLocation loc, bool must_resolve, ResolveSet* resolve_set);

  // Helper for ResolveExport.
  static MUST_USE_RESULT MaybeHandle<Cell> ResolveExportUsingStarExports(
      Handle<Module> module, Handle<String> name, MessageLocation loc,
      bool must_resolve, ResolveSet* resolve_set);

  static MUST_USE_RESULT bool PrepareInstantiate(
      Handle<Module> module, v8::Local<v8::Context> context,
      v8::Module::ResolveCallback callback);
  static MUST_USE_RESULT bool FinishInstantiate(
      Handle<Module> module, ZoneForwardList<Handle<Module>>* stack,
      unsigned* dfs_index, Zone* zone);
  static MUST_USE_RESULT MaybeHandle<Object> Evaluate(
      Handle<Module> module, ZoneForwardList<Handle<Module>>* stack,
      unsigned* dfs_index);

  static void MaybeTransitionComponent(Handle<Module> module,
                                       ZoneForwardList<Handle<Module>>* stack,
                                       Status new_status);

  // To set status to kErrored, RecordError should be used.
  void SetStatus(Status status);
  void RecordError();

  DISALLOW_IMPLICIT_CONSTRUCTORS(Module);
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

  // [context]: The context for this function.
  inline Context* context();
  inline bool has_context() const;
  inline void set_context(Object* context);
  inline JSObject* global_proxy();
  inline Context* native_context();

  static Handle<Object> GetName(Isolate* isolate, Handle<JSFunction> function);
  static MaybeHandle<Smi> GetLength(Isolate* isolate,
                                    Handle<JSFunction> function);
  static Handle<Context> GetFunctionRealm(Handle<JSFunction> function);

  // [code]: The generated code object for this function.  Executed
  // when the function is invoked, e.g. foo() or new foo(). See
  // [[Call]] and [[Construct]] description in ECMA-262, section
  // 8.6.2, page 27.
  inline Code* code();
  inline void set_code(Code* code);
  inline void set_code_no_write_barrier(Code* code);
  inline void ReplaceCode(Code* code);

  // Get the abstract code associated with the function, which will either be
  // a Code object or a BytecodeArray.
  inline AbstractCode* abstract_code();

  // Tells whether this function inlines the given shared function info.
  bool Inlines(SharedFunctionInfo* candidate);

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

  // [feedback_vector_cell]: Fixed array holding the feedback vector.
  DECL_ACCESSORS(feedback_vector_cell, Cell)

  enum FeedbackVectorState {
    TOP_LEVEL_SCRIPT_NEEDS_VECTOR,
    NEEDS_VECTOR,
    HAS_VECTOR
  };

  inline FeedbackVectorState GetFeedbackVectorState(Isolate* isolate) const;

  // feedback_vector() can be used once the function is compiled.
  inline FeedbackVector* feedback_vector() const;
  inline bool has_feedback_vector() const;
  static void EnsureLiterals(Handle<JSFunction> function);

  // Unconditionally clear the type feedback vector.
  void ClearTypeFeedbackInfo();

  // The initial map for an object created by this constructor.
  inline Map* initial_map();
  static void SetInitialMap(Handle<JSFunction> function, Handle<Map> map,
                            Handle<Object> prototype);
  inline bool has_initial_map();
  static void EnsureHasInitialMap(Handle<JSFunction> function);

  // Creates a map that matches the constructor's initial map, but with
  // [[prototype]] being new.target.prototype. Because new.target can be a
  // JSProxy, this can call back into JavaScript.
  static MUST_USE_RESULT MaybeHandle<Map> GetDerivedMap(
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

  // [next_function_link]: Links functions into various lists, e.g. the list
  // of optimized functions hanging off the native_context. Treated weakly
  // by the garbage collector.
  DECL_ACCESSORS(next_function_link, Object)

  // Prints the name of the function using PrintF.
  void PrintName(FILE* out = stdout);

  DECL_CAST(JSFunction)

  // Calculate the instance size and in-object properties count.
  static void CalculateInstanceSizeForDerivedClass(
      Handle<JSFunction> function, InstanceType instance_type,
      int requested_embedder_fields, int* instance_size,
      int* in_object_properties);
  static void CalculateInstanceSizeHelper(InstanceType instance_type,
                                          int requested_embedder_fields,
                                          int requested_in_object_properties,
                                          int* instance_size,
                                          int* in_object_properties);
  enum BodyVisitingPolicy { kIgnoreWeakness, kRespectWeakness };
  // Iterates the function object according to the visiting policy.
  template <BodyVisitingPolicy>
  class BodyDescriptorImpl;

  typedef BodyDescriptorImpl<kIgnoreWeakness> BodyDescriptor;
  typedef BodyDescriptorImpl<kRespectWeakness> BodyDescriptorWeak;

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
  static MUST_USE_RESULT bool SetName(Handle<JSFunction> function,
                                      Handle<Name> name, Handle<String> prefix);

  // The function's displayName if it is set, otherwise name if it is
  // configured, otherwise shared function info
  // debug name.
  static Handle<String> GetDebugName(Handle<JSFunction> function);

  // The function's string representation implemented according to
  // ES6 section 19.2.3.5 Function.prototype.toString ( ).
  static Handle<String> ToString(Handle<JSFunction> function);

  // Layout descriptors. The last property (from kNonWeakFieldsEndOffset to
  // kSize) is weak and has special handling during garbage collection.
  static const int kPrototypeOrInitialMapOffset = JSObject::kHeaderSize;
  static const int kSharedFunctionInfoOffset =
      kPrototypeOrInitialMapOffset + kPointerSize;
  static const int kContextOffset = kSharedFunctionInfoOffset + kPointerSize;
  static const int kFeedbackVectorOffset = kContextOffset + kPointerSize;
  static const int kNonWeakFieldsEndOffset =
      kFeedbackVectorOffset + kPointerSize;
  static const int kCodeEntryOffset = kNonWeakFieldsEndOffset;
  static const int kNextFunctionLinkOffset = kCodeEntryOffset + kPointerSize;
  static const int kSize = kNextFunctionLinkOffset + kPointerSize;

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

  // [hash]: The hash code property (undefined if not initialized yet).
  DECL_ACCESSORS(hash, Object)

  DECL_CAST(JSGlobalProxy)

  inline bool IsDetachedFrom(JSGlobalObject* global) const;

  static int SizeWithEmbedderFields(int embedder_field_count);

  // Dispatched behavior.
  DECL_PRINTER(JSGlobalProxy)
  DECL_VERIFIER(JSGlobalProxy)

  // Layout description.
  static const int kNativeContextOffset = JSObject::kHeaderSize;
  static const int kHashOffset = kNativeContextOffset + kPointerSize;
  static const int kSize = kHashOffset + kPointerSize;

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
  static MUST_USE_RESULT MaybeHandle<JSDate> New(Handle<JSFunction> constructor,
                                                 Handle<JSReceiver> new_target,
                                                 double tv);

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
  DECL_ACCESSORS(script, Object)

  // [stack_frames]: an array of stack frames for this error object.
  DECL_ACCESSORS(stack_frames, Object)

  // [start_position]: the start position in the script for the error message.
  inline int start_position() const;
  inline void set_start_position(int value);

  // [end_position]: the end position in the script for the error message.
  inline int end_position() const;
  inline void set_end_position(int value);

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

class JSPromise;

// TODO(caitp): Make this a Struct once properties are no longer accessed from
// JS
class JSPromiseCapability : public JSObject {
 public:
  DECL_CAST(JSPromiseCapability)

  DECL_VERIFIER(JSPromiseCapability)

  DECL_ACCESSORS(promise, Object)
  DECL_ACCESSORS(resolve, Object)
  DECL_ACCESSORS(reject, Object)

  static const int kPromiseOffset = JSObject::kHeaderSize;
  static const int kResolveOffset = kPromiseOffset + kPointerSize;
  static const int kRejectOffset = kResolveOffset + kPointerSize;
  static const int kSize = kRejectOffset + kPointerSize;

  enum InObjectPropertyIndex {
    kPromiseIndex,
    kResolveIndex,
    kRejectIndex,
    kInObjectPropertyCount  // Dummy.
  };

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(JSPromiseCapability);
};

class JSPromise : public JSObject {
 public:
  DECL_INT_ACCESSORS(status)
  DECL_ACCESSORS(result, Object)

  // There are 3 possible states for these fields --
  // 1) Undefined -- This is the zero state when there is no callback
  // or deferred fields registered.
  //
  // 2) Object -- There is a single callback directly attached to the
  // fulfill_reactions, reject_reactions and the deferred fields are
  // directly attached to the slots. In this state, deferred_promise
  // is a JSReceiver and deferred_on_{resolve, reject} are Callables.
  //
  // 3) FixedArray -- There is more than one callback and deferred
  // fields attached to a FixedArray.
  //
  // The callback can be a Callable or a Symbol.
  DECL_ACCESSORS(deferred_promise, Object)
  DECL_ACCESSORS(deferred_on_resolve, Object)
  DECL_ACCESSORS(deferred_on_reject, Object)
  DECL_ACCESSORS(fulfill_reactions, Object)
  DECL_ACCESSORS(reject_reactions, Object)

  DECL_INT_ACCESSORS(flags)

  // [has_handler]: Whether this promise has a reject handler or not.
  DECL_BOOLEAN_ACCESSORS(has_handler)

  // [handled_hint]: Whether this promise will be handled by a catch
  // block in an async function.
  DECL_BOOLEAN_ACCESSORS(handled_hint)

  static const char* Status(int status);

  DECL_CAST(JSPromise)

  // Dispatched behavior.
  DECL_PRINTER(JSPromise)
  DECL_VERIFIER(JSPromise)

  // Layout description.
  static const int kStatusOffset = JSObject::kHeaderSize;
  static const int kResultOffset = kStatusOffset + kPointerSize;
  static const int kDeferredPromiseOffset = kResultOffset + kPointerSize;
  static const int kDeferredOnResolveOffset =
      kDeferredPromiseOffset + kPointerSize;
  static const int kDeferredOnRejectOffset =
      kDeferredOnResolveOffset + kPointerSize;
  static const int kFulfillReactionsOffset =
      kDeferredOnRejectOffset + kPointerSize;
  static const int kRejectReactionsOffset =
      kFulfillReactionsOffset + kPointerSize;
  static const int kFlagsOffset = kRejectReactionsOffset + kPointerSize;
  static const int kSize = kFlagsOffset + kPointerSize;
  static const int kSizeWithEmbedderFields =
      kSize + v8::Promise::kEmbedderFieldCount * kPointerSize;

  // Flags layout.
  static const int kHasHandlerBit = 0;
  static const int kHandledHintBit = 1;
};

// Regular expressions
// The regular expression holds a single reference to a FixedArray in
// the kDataOffset field.
// The FixedArray contains the following data:
// - tag : type of regexp implementation (not compiled yet, atom or irregexp)
// - reference to the original source string
// - reference to the original flag string
// If it is an atom regexp
// - a reference to a literal string to search for
// If it is an irregexp regexp:
// - a reference to code for Latin1 inputs (bytecode or compiled), or a smi
// used for tracking the last usage (used for regexp code flushing).
// - a reference to code for UC16 inputs (bytecode or compiled), or a smi
// used for tracking the last usage (used for regexp code flushing).
// - max number of registers used by irregexp implementations.
// - number of capture registers (output values) of the regexp.
class JSRegExp: public JSObject {
 public:
  // Meaning of Type:
  // NOT_COMPILED: Initial value. No data has been stored in the JSRegExp yet.
  // ATOM: A simple string to match against using an indexOf operation.
  // IRREGEXP: Compiled with Irregexp.
  enum Type { NOT_COMPILED, ATOM, IRREGEXP };
  enum Flag {
    kNone = 0,
    kGlobal = 1 << 0,
    kIgnoreCase = 1 << 1,
    kMultiline = 1 << 2,
    kSticky = 1 << 3,
    kUnicode = 1 << 4,
    kDotAll = 1 << 5,
    // Update FlagCount when adding new flags.
  };
  typedef base::Flags<Flag> Flags;

  static int FlagCount() { return FLAG_harmony_regexp_dotall ? 6 : 5; }

  DECL_ACCESSORS(data, Object)
  DECL_ACCESSORS(flags, Object)
  DECL_ACCESSORS(last_index, Object)
  DECL_ACCESSORS(source, Object)

  V8_EXPORT_PRIVATE static MaybeHandle<JSRegExp> New(Handle<String> source,
                                                     Flags flags);
  static Handle<JSRegExp> Copy(Handle<JSRegExp> regexp);

  static MaybeHandle<JSRegExp> Initialize(Handle<JSRegExp> regexp,
                                          Handle<String> source, Flags flags);
  static MaybeHandle<JSRegExp> Initialize(Handle<JSRegExp> regexp,
                                          Handle<String> source,
                                          Handle<String> flags_string);

  inline Type TypeTag();
  // Number of captures (without the match itself).
  inline int CaptureCount();
  inline Flags GetFlags();
  inline String* Pattern();
  inline Object* CaptureNameMap();
  inline Object* DataAt(int index);
  // Set implementation data after the object has been prepared.
  inline void SetDataAt(int index, Object* value);

  inline void SetLastIndex(int index);
  inline Object* LastIndex();

  static int code_index(bool is_latin1) {
    if (is_latin1) {
      return kIrregexpLatin1CodeIndex;
    } else {
      return kIrregexpUC16CodeIndex;
    }
  }

  DECL_CAST(JSRegExp)

  // Dispatched behavior.
  DECL_PRINTER(JSRegExp)
  DECL_VERIFIER(JSRegExp)

  static const int kDataOffset = JSObject::kHeaderSize;
  static const int kSourceOffset = kDataOffset + kPointerSize;
  static const int kFlagsOffset = kSourceOffset + kPointerSize;
  static const int kSize = kFlagsOffset + kPointerSize;
  static const int kLastIndexOffset = kSize;  // In-object field.

  // Indices in the data array.
  static const int kTagIndex = 0;
  static const int kSourceIndex = kTagIndex + 1;
  static const int kFlagsIndex = kSourceIndex + 1;
  static const int kDataIndex = kFlagsIndex + 1;
  // The data fields are used in different ways depending on the
  // value of the tag.
  // Atom regexps (literal strings).
  static const int kAtomPatternIndex = kDataIndex;

  static const int kAtomDataSize = kAtomPatternIndex + 1;

  // Irregexp compiled code or bytecode for Latin1. If compilation
  // fails, this fields hold an exception object that should be
  // thrown if the regexp is used again.
  static const int kIrregexpLatin1CodeIndex = kDataIndex;
  // Irregexp compiled code or bytecode for UC16.  If compilation
  // fails, this fields hold an exception object that should be
  // thrown if the regexp is used again.
  static const int kIrregexpUC16CodeIndex = kDataIndex + 1;
  // Maximal number of registers used by either Latin1 or UC16.
  // Only used to check that there is enough stack space
  static const int kIrregexpMaxRegisterCountIndex = kDataIndex + 2;
  // Number of captures in the compiled regexp.
  static const int kIrregexpCaptureCountIndex = kDataIndex + 3;
  // Maps names of named capture groups (at indices 2i) to their corresponding
  // (1-based) capture group indices (at indices 2i + 1).
  static const int kIrregexpCaptureNameMapIndex = kDataIndex + 4;

  static const int kIrregexpDataSize = kIrregexpCaptureNameMapIndex + 1;

  // In-object fields.
  static const int kLastIndexFieldIndex = 0;
  static const int kInObjectFieldCount = 1;

  // The uninitialized value for a regexp code object.
  static const int kUninitializedValue = -1;
};

DEFINE_OPERATORS_FOR_FLAGS(JSRegExp::Flags)

class TypeFeedbackInfo : public Tuple3 {
 public:
  inline int ic_total_count();
  inline void set_ic_total_count(int count);

  inline int ic_with_type_info_count();
  inline void change_ic_with_type_info_count(int delta);

  inline int ic_generic_count();
  inline void change_ic_generic_count(int delta);

  inline void initialize_storage();

  inline void change_own_type_change_checksum();
  inline int own_type_change_checksum();

  inline void set_inlined_type_change_checksum(int checksum);
  inline bool matches_inlined_type_change_checksum(int checksum);

  DECL_CAST(TypeFeedbackInfo)

  static const int kStorage1Offset = kValue1Offset;
  static const int kStorage2Offset = kValue2Offset;
  static const int kStorage3Offset = kValue3Offset;

 private:
  static const int kTypeChangeChecksumBits = 7;

  class ICTotalCountField: public BitField<int, 0,
      kSmiValueSize - kTypeChangeChecksumBits> {};  // NOLINT
  class OwnTypeChangeChecksum: public BitField<int,
      kSmiValueSize - kTypeChangeChecksumBits,
      kTypeChangeChecksumBits> {};  // NOLINT
  class ICsWithTypeInfoCountField: public BitField<int, 0,
      kSmiValueSize - kTypeChangeChecksumBits> {};  // NOLINT
  class InlinedTypeChangeChecksum: public BitField<int,
      kSmiValueSize - kTypeChangeChecksumBits,
      kTypeChangeChecksumBits> {};  // NOLINT

  DISALLOW_IMPLICIT_CONSTRUCTORS(TypeFeedbackInfo);
};

class AllocationSite: public Struct {
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
  DECL_INT_ACCESSORS(pretenure_data)

  DECL_INT_ACCESSORS(pretenure_create_count)
  DECL_ACCESSORS(dependent_code, DependentCode)

  // heap->allocation_site_list() points to the last AllocationSite which form
  // a linked list through the weak_next property. The GC might remove elements
  // from the list by updateing weak_next.
  DECL_ACCESSORS(weak_next, Object)

  inline void Initialize();

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

  static const int kTransitionInfoOrBoilerplateOffset = HeapObject::kHeaderSize;
  static const int kNestedSiteOffset =
      kTransitionInfoOrBoilerplateOffset + kPointerSize;
  static const int kPretenureDataOffset = kNestedSiteOffset + kPointerSize;
  static const int kPretenureCreateCountOffset =
      kPretenureDataOffset + kPointerSize;
  static const int kDependentCodeOffset =
      kPretenureCreateCountOffset + kPointerSize;
  static const int kWeakNextOffset = kDependentCodeOffset + kPointerSize;
  static const int kSize = kWeakNextOffset + kPointerSize;

  // During mark compact we need to take special care for the dependent code
  // field.
  static const int kPointerFieldsBeginOffset =
      kTransitionInfoOrBoilerplateOffset;
  static const int kPointerFieldsEndOffset = kWeakNextOffset;

  // Ignores weakness.
  typedef FixedBodyDescriptor<HeapObject::kHeaderSize, kSize, kSize>
      BodyDescriptor;

  // Respects weakness.
  typedef FixedBodyDescriptor<kPointerFieldsBeginOffset,
                              kPointerFieldsEndOffset, kSize>
      BodyDescriptorWeak;

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

template <typename T>
class VectorIterator {
 public:
  VectorIterator(T* d, int l) : data_(Vector<const T>(d, l)), index_(0) { }
  explicit VectorIterator(Vector<const T> data) : data_(data), index_(0) { }
  T GetNext() { return data_[index_++]; }
  bool has_more() { return index_ < data_.length(); }
 private:
  Vector<const T> data_;
  int index_;
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
  MUST_USE_RESULT static inline Handle<Object> ToNumber(Handle<Oddball> input);

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

  inline PropertyDetails property_details();
  inline void set_property_details(PropertyDetails details);

  PropertyCellConstantType GetConstantType();

  // Computes the new type of the cell's contents for the given value, but
  // without actually modifying the details.
  static PropertyCellType UpdatedType(Handle<PropertyCell> cell,
                                      Handle<Object> value,
                                      PropertyDetails details);
  // Prepares property cell at given entry for receiving given value.
  // As a result the old cell could be invalidated and/or dependent code could
  // be deoptimized. Returns the prepared property cell.
  static Handle<PropertyCell> PrepareForValue(
      Handle<GlobalDictionary> dictionary, int entry, Handle<Object> value,
      PropertyDetails details);

  static Handle<PropertyCell> InvalidateEntry(
      Handle<GlobalDictionary> dictionary, int entry);

  static void SetValueWithInvalidation(Handle<PropertyCell> cell,
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

  DECL_ACCESSORS(next, Object)

  inline void clear_next(Object* the_hole_value);

  inline bool next_cleared();

  DECL_CAST(WeakCell)

  DECL_PRINTER(WeakCell)
  DECL_VERIFIER(WeakCell)

  // Layout description.
  static const int kValueOffset = HeapObject::kHeaderSize;
  static const int kNextOffset = kValueOffset + kPointerSize;
  static const int kSize = kNextOffset + kPointerSize;

  typedef FixedBodyDescriptor<kValueOffset, kSize, kSize> BodyDescriptor;

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(WeakCell);
};


// The JSProxy describes EcmaScript Harmony proxies
class JSProxy: public JSReceiver {
 public:
  MUST_USE_RESULT static MaybeHandle<JSProxy> New(Isolate* isolate,
                                                  Handle<Object>,
                                                  Handle<Object>);

  // [handler]: The handler property.
  DECL_ACCESSORS(handler, Object)
  // [target]: The target property.
  DECL_ACCESSORS(target, JSReceiver)
  // [hash]: The hash code property (undefined if not initialized yet).
  DECL_ACCESSORS(hash, Object)

  static MaybeHandle<Context> GetFunctionRealm(Handle<JSProxy> proxy);

  DECL_CAST(JSProxy)

  INLINE(bool IsRevoked() const);
  static void Revoke(Handle<JSProxy> proxy);

  // ES6 9.5.1
  static MaybeHandle<Object> GetPrototype(Handle<JSProxy> receiver);

  // ES6 9.5.2
  MUST_USE_RESULT static Maybe<bool> SetPrototype(Handle<JSProxy> proxy,
                                                  Handle<Object> value,
                                                  bool from_javascript,
                                                  ShouldThrow should_throw);
  // ES6 9.5.3
  MUST_USE_RESULT static Maybe<bool> IsExtensible(Handle<JSProxy> proxy);

  // ES6, #sec-isarray.  NOT to be confused with %_IsArray.
  MUST_USE_RESULT static Maybe<bool> IsArray(Handle<JSProxy> proxy);

  // ES6 9.5.4 (when passed DONT_THROW)
  MUST_USE_RESULT static Maybe<bool> PreventExtensions(
      Handle<JSProxy> proxy, ShouldThrow should_throw);

  // ES6 9.5.5
  MUST_USE_RESULT static Maybe<bool> GetOwnPropertyDescriptor(
      Isolate* isolate, Handle<JSProxy> proxy, Handle<Name> name,
      PropertyDescriptor* desc);

  // ES6 9.5.6
  MUST_USE_RESULT static Maybe<bool> DefineOwnProperty(
      Isolate* isolate, Handle<JSProxy> object, Handle<Object> key,
      PropertyDescriptor* desc, ShouldThrow should_throw);

  // ES6 9.5.7
  MUST_USE_RESULT static Maybe<bool> HasProperty(Isolate* isolate,
                                                 Handle<JSProxy> proxy,
                                                 Handle<Name> name);

  // ES6 9.5.8
  MUST_USE_RESULT static MaybeHandle<Object> GetProperty(
      Isolate* isolate, Handle<JSProxy> proxy, Handle<Name> name,
      Handle<Object> receiver, bool* was_found);

  // ES6 9.5.9
  MUST_USE_RESULT static Maybe<bool> SetProperty(Handle<JSProxy> proxy,
                                                 Handle<Name> name,
                                                 Handle<Object> value,
                                                 Handle<Object> receiver,
                                                 LanguageMode language_mode);

  // ES6 9.5.10 (when passed SLOPPY)
  MUST_USE_RESULT static Maybe<bool> DeletePropertyOrElement(
      Handle<JSProxy> proxy, Handle<Name> name, LanguageMode language_mode);

  // ES6 9.5.12
  MUST_USE_RESULT static Maybe<bool> OwnPropertyKeys(
      Isolate* isolate, Handle<JSReceiver> receiver, Handle<JSProxy> proxy,
      PropertyFilter filter, KeyAccumulator* accumulator);

  MUST_USE_RESULT static Maybe<PropertyAttributes> GetPropertyAttributes(
      LookupIterator* it);

  // Dispatched behavior.
  DECL_PRINTER(JSProxy)
  DECL_VERIFIER(JSProxy)

  static const int kMaxIterationLimit = 100 * 1024;

  // Layout description.
  static const int kTargetOffset = JSReceiver::kHeaderSize;
  static const int kHandlerOffset = kTargetOffset + kPointerSize;
  static const int kHashOffset = kHandlerOffset + kPointerSize;
  static const int kSize = kHashOffset + kPointerSize;

  typedef FixedBodyDescriptor<JSReceiver::kPropertiesOrHashOffset, kSize, kSize>
      BodyDescriptor;
  // No weak fields.
  typedef BodyDescriptor BodyDescriptorWeak;

  static Object* GetIdentityHash(Handle<JSProxy> receiver);

  static Smi* GetOrCreateIdentityHash(Isolate* isolate, Handle<JSProxy> proxy);

  static Maybe<bool> SetPrivateProperty(Isolate* isolate, Handle<JSProxy> proxy,
                                        Handle<Symbol> private_name,
                                        PropertyDescriptor* desc,
                                        ShouldThrow should_throw);

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(JSProxy);
};


class JSCollection : public JSObject {
 public:
  // [table]: the backing hash table
  DECL_ACCESSORS(table, Object)

  static const int kTableOffset = JSObject::kHeaderSize;
  static const int kSize = kTableOffset + kPointerSize;

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(JSCollection);
};


// The JSSet describes EcmaScript Harmony sets
// TODO(marja): When moving JSSet out of objects.h, move JSSetIterator (from
// objects/hash-table.h) into the same file.
class JSSet : public JSCollection {
 public:
  DECL_CAST(JSSet)

  static void Initialize(Handle<JSSet> set, Isolate* isolate);
  static void Clear(Handle<JSSet> set);

  // Dispatched behavior.
  DECL_PRINTER(JSSet)
  DECL_VERIFIER(JSSet)

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(JSSet);
};


// The JSMap describes EcmaScript Harmony maps
// TODO(marja): When moving JSMap out of objects.h, move JSMapIterator (from
// objects/hash-table.h) into the same file.
class JSMap : public JSCollection {
 public:
  DECL_CAST(JSMap)

  static void Initialize(Handle<JSMap> map, Isolate* isolate);
  static void Clear(Handle<JSMap> map);

  // Dispatched behavior.
  DECL_PRINTER(JSMap)
  DECL_VERIFIER(JSMap)

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(JSMap);
};

class JSArrayIterator : public JSObject {
 public:
  DECL_PRINTER(JSArrayIterator)
  DECL_VERIFIER(JSArrayIterator)

  DECL_CAST(JSArrayIterator)

  // [object]: the [[IteratedObject]] inobject property.
  DECL_ACCESSORS(object, Object)

  // [index]: The [[ArrayIteratorNextIndex]] inobject property.
  DECL_ACCESSORS(index, Object)

  // [map]: The Map of the [[IteratedObject]] field at the time the iterator is
  // allocated.
  DECL_ACCESSORS(object_map, Object)

  // Return the ElementsKind that a JSArrayIterator's [[IteratedObject]] is
  // expected to have, based on its instance type.
  static ElementsKind ElementsKindForInstanceType(InstanceType instance_type);

  static const int kIteratedObjectOffset = JSObject::kHeaderSize;
  static const int kNextIndexOffset = kIteratedObjectOffset + kPointerSize;
  static const int kIteratedObjectMapOffset = kNextIndexOffset + kPointerSize;
  static const int kSize = kIteratedObjectMapOffset + kPointerSize;

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(JSArrayIterator);
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

  // Offsets of object fields.
  static const int kSyncIteratorOffset = JSObject::kHeaderSize;
  static const int kSize = kSyncIteratorOffset + kPointerSize;

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

// Base class for both JSWeakMap and JSWeakSet
class JSWeakCollection: public JSObject {
 public:
  DECL_CAST(JSWeakCollection)

  // [table]: the backing hash table mapping keys to values.
  DECL_ACCESSORS(table, Object)

  // [next]: linked list of encountered weak maps during GC.
  DECL_ACCESSORS(next, Object)

  static void Initialize(Handle<JSWeakCollection> collection, Isolate* isolate);
  static void Set(Handle<JSWeakCollection> collection, Handle<Object> key,
                  Handle<Object> value, int32_t hash);
  static bool Delete(Handle<JSWeakCollection> collection, Handle<Object> key,
                     int32_t hash);
  static Handle<JSArray> GetEntries(Handle<JSWeakCollection> holder,
                                    int max_entries);

  static const int kTableOffset = JSObject::kHeaderSize;
  static const int kNextOffset = kTableOffset + kPointerSize;
  static const int kSize = kNextOffset + kPointerSize;

  // Visiting policy defines whether the table and next collection fields
  // should be visited or not.
  enum BodyVisitingPolicy { kIgnoreWeakness, kRespectWeakness };

  // Iterates the function object according to the visiting policy.
  template <BodyVisitingPolicy>
  class BodyDescriptorImpl;

  // Visit the whole object.
  typedef BodyDescriptorImpl<kIgnoreWeakness> BodyDescriptor;

  // Don't visit table and next collection fields.
  typedef BodyDescriptorImpl<kRespectWeakness> BodyDescriptorWeak;

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(JSWeakCollection);
};


// The JSWeakMap describes EcmaScript Harmony weak maps
class JSWeakMap: public JSWeakCollection {
 public:
  DECL_CAST(JSWeakMap)

  // Dispatched behavior.
  DECL_PRINTER(JSWeakMap)
  DECL_VERIFIER(JSWeakMap)

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(JSWeakMap);
};


// The JSWeakSet describes EcmaScript Harmony weak sets
class JSWeakSet: public JSWeakCollection {
 public:
  DECL_CAST(JSWeakSet)

  // Dispatched behavior.
  DECL_PRINTER(JSWeakSet)
  DECL_VERIFIER(JSWeakSet)

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(JSWeakSet);
};


// Whether a JSArrayBuffer is a SharedArrayBuffer or not.
enum class SharedFlag { kNotShared, kShared };


class JSArrayBuffer: public JSObject {
 public:
  // [backing_store]: backing memory for this array
  DECL_ACCESSORS(backing_store, void)

  // [byte_length]: length in bytes
  DECL_ACCESSORS(byte_length, Object)

  // [allocation_base]: the start of the memory allocation for this array,
  // normally equal to backing_store
  DECL_ACCESSORS(allocation_base, void)

  // [allocation_length]: the size of the memory allocation for this array,
  // normally equal to byte_length
  inline size_t allocation_length() const;
  inline void set_allocation_length(size_t value);

  inline uint32_t bit_field() const;
  inline void set_bit_field(uint32_t bits);

  // [is_external]: true indicates that the embedder is in charge of freeing the
  // backing_store, while is_external == false means that v8 will free the
  // memory block once all ArrayBuffers referencing it are collected by the GC.
  inline bool is_external();
  inline void set_is_external(bool value);

  inline bool is_neuterable();
  inline void set_is_neuterable(bool value);

  inline bool was_neutered();
  inline void set_was_neutered(bool value);

  inline bool is_shared();
  inline void set_is_shared(bool value);

  inline bool has_guard_region() const;
  inline void set_has_guard_region(bool value);

  // TODO(gdeepti): This flag is introduced to disable asm.js optimizations in
  // js-typer-lowering.cc, remove when the asm.js case is fixed.
  inline bool is_wasm_buffer();
  inline void set_is_wasm_buffer(bool value);

  DECL_CAST(JSArrayBuffer)

  void Neuter();

  inline ArrayBuffer::Allocator::AllocationMode allocation_mode() const;

  void FreeBackingStore();

  V8_EXPORT_PRIVATE static void Setup(
      Handle<JSArrayBuffer> array_buffer, Isolate* isolate, bool is_external,
      void* data, size_t allocated_length,
      SharedFlag shared = SharedFlag::kNotShared);

  V8_EXPORT_PRIVATE static void Setup(
      Handle<JSArrayBuffer> array_buffer, Isolate* isolate, bool is_external,
      void* allocation_base, size_t allocation_length, void* data,
      size_t byte_length, SharedFlag shared = SharedFlag::kNotShared);

  // Returns false if array buffer contents could not be allocated.
  // In this case, |array_buffer| will not be set up.
  static bool SetupAllocatingData(
      Handle<JSArrayBuffer> array_buffer, Isolate* isolate,
      size_t allocated_length, bool initialize = true,
      SharedFlag shared = SharedFlag::kNotShared) WARN_UNUSED_RESULT;

  // Dispatched behavior.
  DECL_PRINTER(JSArrayBuffer)
  DECL_VERIFIER(JSArrayBuffer)

  static const int kByteLengthOffset = JSObject::kHeaderSize;
  // The rest of the fields are not JSObjects, so they are not iterated over in
  // objects-body-descriptors-inl.h.
  static const int kBackingStoreOffset = kByteLengthOffset + kPointerSize;
  static const int kAllocationBaseOffset = kBackingStoreOffset + kPointerSize;
  static const int kAllocationLengthOffset =
      kAllocationBaseOffset + kPointerSize;
  static const int kBitFieldSlot = kAllocationLengthOffset + kSizetSize;
#if V8_TARGET_LITTLE_ENDIAN || !V8_HOST_ARCH_64_BIT
  static const int kBitFieldOffset = kBitFieldSlot;
#else
  static const int kBitFieldOffset = kBitFieldSlot + kIntSize;
#endif
  static const int kSize = kBitFieldSlot + kPointerSize;

  static const int kSizeWithEmbedderFields =
      kSize + v8::ArrayBuffer::kEmbedderFieldCount * kPointerSize;

  // Iterates all fields in the object including internal ones except
  // kBackingStoreOffset and kBitFieldSlot.
  class BodyDescriptor;
  // No weak fields.
  typedef BodyDescriptor BodyDescriptorWeak;

  class IsExternal : public BitField<bool, 1, 1> {};
  class IsNeuterable : public BitField<bool, 2, 1> {};
  class WasNeutered : public BitField<bool, 3, 1> {};
  class IsShared : public BitField<bool, 4, 1> {};
  class HasGuardRegion : public BitField<bool, 5, 1> {};
  class IsWasmBuffer : public BitField<bool, 6, 1> {};

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(JSArrayBuffer);
};


class JSArrayBufferView: public JSObject {
 public:
  // [buffer]: ArrayBuffer that this typed array views.
  DECL_ACCESSORS(buffer, Object)

  // [byte_offset]: offset of typed array in bytes.
  DECL_ACCESSORS(byte_offset, Object)

  // [byte_length]: length of typed array in bytes.
  DECL_ACCESSORS(byte_length, Object)

  DECL_CAST(JSArrayBufferView)

  DECL_VERIFIER(JSArrayBufferView)

  inline bool WasNeutered() const;

  static const int kBufferOffset = JSObject::kHeaderSize;
  static const int kByteOffsetOffset = kBufferOffset + kPointerSize;
  static const int kByteLengthOffset = kByteOffsetOffset + kPointerSize;
  static const int kViewSize = kByteLengthOffset + kPointerSize;

 private:
#ifdef VERIFY_HEAP
  DECL_ACCESSORS(raw_byte_offset, Object)
  DECL_ACCESSORS(raw_byte_length, Object)
#endif

  DISALLOW_IMPLICIT_CONSTRUCTORS(JSArrayBufferView);
};


class JSTypedArray: public JSArrayBufferView {
 public:
  // [length]: length of typed array in elements.
  DECL_ACCESSORS(length, Object)
  inline uint32_t length_value() const;

  // ES6 9.4.5.3
  MUST_USE_RESULT static Maybe<bool> DefineOwnProperty(
      Isolate* isolate, Handle<JSTypedArray> o, Handle<Object> key,
      PropertyDescriptor* desc, ShouldThrow should_throw);

  DECL_CAST(JSTypedArray)

  ExternalArrayType type();
  V8_EXPORT_PRIVATE size_t element_size();

  Handle<JSArrayBuffer> GetBuffer();

  static inline MaybeHandle<JSTypedArray> Validate(Isolate* isolate,
                                                   Handle<Object> receiver,
                                                   const char* method_name);
  // ES7 section 22.2.4.6 Create ( constructor, argumentList )
  static MaybeHandle<JSTypedArray> Create(Isolate* isolate,
                                          Handle<Object> default_ctor, int argc,
                                          Handle<Object>* argv,
                                          const char* method_name);
  // ES7 section 22.2.4.7 TypedArraySpeciesCreate ( exemplar, argumentList )
  static MaybeHandle<JSTypedArray> SpeciesCreate(Isolate* isolate,
                                                 Handle<JSTypedArray> exemplar,
                                                 int argc, Handle<Object>* argv,
                                                 const char* method_name);

  // Dispatched behavior.
  DECL_PRINTER(JSTypedArray)
  DECL_VERIFIER(JSTypedArray)

  static const int kLengthOffset = kViewSize + kPointerSize;
  static const int kSize = kLengthOffset + kPointerSize;

  static const int kSizeWithEmbedderFields =
      kSize + v8::ArrayBufferView::kEmbedderFieldCount * kPointerSize;

 private:
  static Handle<JSArrayBuffer> MaterializeArrayBuffer(
      Handle<JSTypedArray> typed_array);
#ifdef VERIFY_HEAP
  DECL_ACCESSORS(raw_length, Object)
#endif

  DISALLOW_IMPLICIT_CONSTRUCTORS(JSTypedArray);
};


class JSDataView: public JSArrayBufferView {
 public:
  DECL_CAST(JSDataView)

  // Dispatched behavior.
  DECL_PRINTER(JSDataView)
  DECL_VERIFIER(JSDataView)

  static const int kSize = kViewSize;

  static const int kSizeWithEmbedderFields =
      kSize + v8::ArrayBufferView::kEmbedderFieldCount * kPointerSize;

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(JSDataView);
};


// Foreign describes objects pointing from JavaScript to C structures.
class Foreign: public HeapObject {
 public:
  // [address]: field containing the address.
  inline Address foreign_address();
  inline void set_foreign_address(Address value);

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
  DISALLOW_IMPLICIT_CONSTRUCTORS(Foreign);
};


// The JSArray describes JavaScript Arrays
//  Such an array can be in one of two modes:
//    - fast, backing storage is a FixedArray and length <= elements.length();
//       Please note: push and pop can be used to grow and shrink the array.
//    - slow, backing storage is a HashTable with numbers as keys.
class JSArray: public JSObject {
 public:
  // [length]: The length property.
  DECL_ACCESSORS(length, Object)

  // Overload the length setter to skip write barrier when the length
  // is set to a smi. This matches the set function on FixedArray.
  inline void set_length(Smi* length);

  static bool HasReadOnlyLength(Handle<JSArray> array);
  static bool WouldChangeReadOnlyLength(Handle<JSArray> array, uint32_t index);

  // Initialize the array with the given capacity. The function may
  // fail due to out-of-memory situations, but only if the requested
  // capacity is non-zero.
  static void Initialize(Handle<JSArray> array, int capacity, int length = 0);

  // If the JSArray has fast elements, and new_length would result in
  // normalization, returns true.
  bool SetLengthWouldNormalize(uint32_t new_length);
  static inline bool SetLengthWouldNormalize(Heap* heap, uint32_t new_length);

  // Initializes the array to a certain length.
  inline bool AllowsSetLength();

  static void SetLength(Handle<JSArray> array, uint32_t length);

  // Set the content of the array to the content of storage.
  static inline void SetContent(Handle<JSArray> array,
                                Handle<FixedArrayBase> storage);

  // ES6 9.4.2.1
  MUST_USE_RESULT static Maybe<bool> DefineOwnProperty(
      Isolate* isolate, Handle<JSArray> o, Handle<Object> name,
      PropertyDescriptor* desc, ShouldThrow should_throw);

  static bool AnythingToArrayLength(Isolate* isolate,
                                    Handle<Object> length_object,
                                    uint32_t* output);
  MUST_USE_RESULT static Maybe<bool> ArraySetLength(Isolate* isolate,
                                                    Handle<JSArray> a,
                                                    PropertyDescriptor* desc,
                                                    ShouldThrow should_throw);

  // Checks whether the Array has the current realm's Array.prototype as its
  // prototype. This function is best-effort and only gives a conservative
  // approximation, erring on the side of false, in particular with respect
  // to Proxies and objects with a hidden prototype.
  inline bool HasArrayPrototype(Isolate* isolate);

  DECL_CAST(JSArray)

  // Dispatched behavior.
  DECL_PRINTER(JSArray)
  DECL_VERIFIER(JSArray)

  // Number of element slots to pre-allocate for an empty array.
  static const int kPreallocatedArrayElements = 4;

  // Layout description.
  static const int kLengthOffset = JSObject::kHeaderSize;
  static const int kSize = kLengthOffset + kPointerSize;

  // Max. number of elements being copied in Array builtins.
  static const int kMaxCopyElements = 100;

  static const int kInitialMaxFastElementArray =
      (kMaxRegularHeapObjectSize - FixedArray::kHeaderSize - kSize -
       AllocationMemento::kSize) /
      kPointerSize;

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(JSArray);
};


Handle<Object> CacheInitialJSArrayMaps(Handle<Context> native_context,
                                       Handle<Map> initial_map);


// JSRegExpResult is just a JSArray with a specific initial map.
// This initial map adds in-object properties for "index" and "input"
// properties, as assigned by RegExp.prototype.exec, which allows
// faster creation of RegExp exec results.
// This class just holds constants used when creating the result.
// After creation the result must be treated as a JSArray in all regards.
class JSRegExpResult: public JSArray {
 public:
  // Offsets of object fields.
  static const int kIndexOffset = JSArray::kSize;
  static const int kInputOffset = kIndexOffset + kPointerSize;
  static const int kSize = kInputOffset + kPointerSize;
  // Indices of in-object properties.
  static const int kIndexIndex = 0;
  static const int kInputIndex = 1;
 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(JSRegExpResult);
};


// An accessor must have a getter, but can have no setter.
//
// When setting a property, V8 searches accessors in prototypes.
// If an accessor was found and it does not have a setter,
// the request is ignored.
//
// If the accessor in the prototype has the READ_ONLY property attribute, then
// a new value is added to the derived object when the property is set.
// This shadows the accessor in the prototype.
class AccessorInfo: public Struct {
 public:
  DECL_ACCESSORS(name, Object)
  DECL_INT_ACCESSORS(flag)
  DECL_ACCESSORS(expected_receiver_type, Object)
  // This directly points at a foreign C function to be used from the runtime.
  DECL_ACCESSORS(getter, Object)
  DECL_ACCESSORS(setter, Object)
  // This either points at the same as above, or a trampoline in case we are
  // running with the simulator. Use these entries from generated code.
  DECL_ACCESSORS(js_getter, Object)
  DECL_ACCESSORS(data, Object)

  static Address redirect(Isolate* isolate, Address address,
                          AccessorComponent component);
  Address redirected_getter() const;

  // Dispatched behavior.
  DECL_PRINTER(AccessorInfo)

  inline bool all_can_read();
  inline void set_all_can_read(bool value);

  inline bool all_can_write();
  inline void set_all_can_write(bool value);

  inline bool is_special_data_property();
  inline void set_is_special_data_property(bool value);

  inline bool replace_on_access();
  inline void set_replace_on_access(bool value);

  inline bool is_sloppy();
  inline void set_is_sloppy(bool value);

  inline PropertyAttributes property_attributes();
  inline void set_property_attributes(PropertyAttributes attributes);

  // Checks whether the given receiver is compatible with this accessor.
  static bool IsCompatibleReceiverMap(Isolate* isolate,
                                      Handle<AccessorInfo> info,
                                      Handle<Map> map);
  inline bool IsCompatibleReceiver(Object* receiver);

  DECL_CAST(AccessorInfo)

  // Dispatched behavior.
  DECL_VERIFIER(AccessorInfo)

  // Append all descriptors to the array that are not already there.
  // Return number added.
  static int AppendUnique(Handle<Object> descriptors,
                          Handle<FixedArray> array,
                          int valid_descriptors);

  static const int kNameOffset = HeapObject::kHeaderSize;
  static const int kFlagOffset = kNameOffset + kPointerSize;
  static const int kExpectedReceiverTypeOffset = kFlagOffset + kPointerSize;
  static const int kSetterOffset = kExpectedReceiverTypeOffset + kPointerSize;
  static const int kGetterOffset = kSetterOffset + kPointerSize;
  static const int kJsGetterOffset = kGetterOffset + kPointerSize;
  static const int kDataOffset = kJsGetterOffset + kPointerSize;
  static const int kSize = kDataOffset + kPointerSize;


 private:
  inline bool HasExpectedReceiverType();

  // Bit positions in flag.
  static const int kAllCanReadBit = 0;
  static const int kAllCanWriteBit = 1;
  static const int kSpecialDataProperty = 2;
  static const int kIsSloppy = 3;
  static const int kReplaceOnAccess = 4;
  class AttributesField : public BitField<PropertyAttributes, 5, 3> {};

  DISALLOW_IMPLICIT_CONSTRUCTORS(AccessorInfo);
};


// Support for JavaScript accessors: A pair of a getter and a setter. Each
// accessor can either be
//   * a pointer to a JavaScript function or proxy: a real accessor
//   * undefined: considered an accessor by the spec, too, strangely enough
//   * the hole: an accessor which has not been set
//   * a pointer to a map: a transition used to ensure map sharing
class AccessorPair: public Struct {
 public:
  DECL_ACCESSORS(getter, Object)
  DECL_ACCESSORS(setter, Object)

  DECL_CAST(AccessorPair)

  static Handle<AccessorPair> Copy(Handle<AccessorPair> pair);

  inline Object* get(AccessorComponent component);
  inline void set(AccessorComponent component, Object* value);

  // Note: Returns undefined instead in case of a hole.
  static Handle<Object> GetComponent(Handle<AccessorPair> accessor_pair,
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


class AccessCheckInfo: public Struct {
 public:
  DECL_ACCESSORS(callback, Object)
  DECL_ACCESSORS(named_interceptor, Object)
  DECL_ACCESSORS(indexed_interceptor, Object)
  DECL_ACCESSORS(data, Object)

  DECL_CAST(AccessCheckInfo)

  // Dispatched behavior.
  DECL_PRINTER(AccessCheckInfo)
  DECL_VERIFIER(AccessCheckInfo)

  static AccessCheckInfo* Get(Isolate* isolate, Handle<JSObject> receiver);

  static const int kCallbackOffset = HeapObject::kHeaderSize;
  static const int kNamedInterceptorOffset = kCallbackOffset + kPointerSize;
  static const int kIndexedInterceptorOffset =
      kNamedInterceptorOffset + kPointerSize;
  static const int kDataOffset = kIndexedInterceptorOffset + kPointerSize;
  static const int kSize = kDataOffset + kPointerSize;

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(AccessCheckInfo);
};


class InterceptorInfo: public Struct {
 public:
  DECL_ACCESSORS(getter, Object)
  DECL_ACCESSORS(setter, Object)
  DECL_ACCESSORS(query, Object)
  DECL_ACCESSORS(descriptor, Object)
  DECL_ACCESSORS(deleter, Object)
  DECL_ACCESSORS(enumerator, Object)
  DECL_ACCESSORS(definer, Object)
  DECL_ACCESSORS(data, Object)
  DECL_BOOLEAN_ACCESSORS(can_intercept_symbols)
  DECL_BOOLEAN_ACCESSORS(all_can_read)
  DECL_BOOLEAN_ACCESSORS(non_masking)

  inline int flags() const;
  inline void set_flags(int flags);

  DECL_CAST(InterceptorInfo)

  // Dispatched behavior.
  DECL_PRINTER(InterceptorInfo)
  DECL_VERIFIER(InterceptorInfo)

  static const int kGetterOffset = HeapObject::kHeaderSize;
  static const int kSetterOffset = kGetterOffset + kPointerSize;
  static const int kQueryOffset = kSetterOffset + kPointerSize;
  static const int kDescriptorOffset = kQueryOffset + kPointerSize;
  static const int kDeleterOffset = kDescriptorOffset + kPointerSize;
  static const int kEnumeratorOffset = kDeleterOffset + kPointerSize;
  static const int kDefinerOffset = kEnumeratorOffset + kPointerSize;
  static const int kDataOffset = kDefinerOffset + kPointerSize;
  static const int kFlagsOffset = kDataOffset + kPointerSize;
  static const int kSize = kFlagsOffset + kPointerSize;

  static const int kCanInterceptSymbolsBit = 0;
  static const int kAllCanReadBit = 1;
  static const int kNonMasking = 2;

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(InterceptorInfo);
};

class CallHandlerInfo : public Tuple2 {
 public:
  DECL_ACCESSORS(callback, Object)
  DECL_ACCESSORS(data, Object)

  DECL_CAST(CallHandlerInfo)

  static const int kCallbackOffset = kValue1Offset;
  static const int kDataOffset = kValue2Offset;

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(CallHandlerInfo);
};


class TemplateInfo: public Struct {
 public:
  DECL_ACCESSORS(tag, Object)
  DECL_ACCESSORS(serial_number, Object)
  DECL_INT_ACCESSORS(number_of_properties)
  DECL_ACCESSORS(property_list, Object)
  DECL_ACCESSORS(property_accessors, Object)

  DECL_VERIFIER(TemplateInfo)

  DECL_CAST(TemplateInfo)

  static const int kTagOffset = HeapObject::kHeaderSize;
  static const int kSerialNumberOffset = kTagOffset + kPointerSize;
  static const int kNumberOfProperties = kSerialNumberOffset + kPointerSize;
  static const int kPropertyListOffset = kNumberOfProperties + kPointerSize;
  static const int kPropertyAccessorsOffset =
      kPropertyListOffset + kPointerSize;
  static const int kHeaderSize = kPropertyAccessorsOffset + kPointerSize;

  static const int kFastTemplateInstantiationsCacheSize = 1 * KB;

  // While we could grow the slow cache until we run out of memory, we put
  // a limit on it anyway to not crash for embedders that re-create templates
  // instead of caching them.
  static const int kSlowTemplateInstantiationsCacheSize = 1 * MB;

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(TemplateInfo);
};

// See the api-exposed FunctionTemplate for more information.
class FunctionTemplateInfo: public TemplateInfo {
 public:
  // Handler invoked when calling an instance of this FunctionTemplateInfo.
  // Either CallInfoHandler or Undefined.
  DECL_ACCESSORS(call_code, Object)

  // ObjectTemplateInfo or Undefined, used for the prototype property of the
  // resulting JSFunction instance of this FunctionTemplate.
  DECL_ACCESSORS(prototype_template, Object)

  // In the case the prototype_template is Undefined we use the
  // protoype_provider_template to retrieve the instance prototype. Either
  // contains an ObjectTemplateInfo or Undefined.
  DECL_ACCESSORS(prototype_provider_template, Object)

  // Used to create protoype chains. The parent_template's prototype is set as
  // __proto__ of this FunctionTemplate's instance prototype. Is either a
  // FunctionTemplateInfo or Undefined.
  DECL_ACCESSORS(parent_template, Object)

  // Returns an InterceptorInfo or Undefined for named properties.
  DECL_ACCESSORS(named_property_handler, Object)
  // Returns an InterceptorInfo or Undefined for indexed properties/elements.
  DECL_ACCESSORS(indexed_property_handler, Object)

  // An ObjectTemplateInfo that is used when instantiating the JSFunction
  // associated with this FunctionTemplateInfo. Contains either an
  // ObjectTemplateInfo or Undefined. A default instance_template is assigned
  // upon first instantiation if it's Undefined.
  DECL_ACCESSORS(instance_template, Object)

  DECL_ACCESSORS(class_name, Object)

  // If the signature is a FunctionTemplateInfo it is used to check whether the
  // receiver calling the associated JSFunction is a compatible receiver, i.e.
  // it is an instance of the signare FunctionTemplateInfo or any of the
  // receiver's prototypes are.
  DECL_ACCESSORS(signature, Object)

  // Either a CallHandlerInfo or Undefined. If an instance_call_handler is
  // provided the instances created from the associated JSFunction are marked as
  // callable.
  DECL_ACCESSORS(instance_call_handler, Object)

  DECL_ACCESSORS(access_check_info, Object)
  DECL_ACCESSORS(shared_function_info, Object)

  // Internal field to store a flag bitfield.
  DECL_INT_ACCESSORS(flag)

  // "length" property of the final JSFunction.
  DECL_INT_ACCESSORS(length)

  // Either the_hole or a private symbol. Used to cache the result on
  // the receiver under the the cached_property_name when this
  // FunctionTemplateInfo is used as a getter.
  DECL_ACCESSORS(cached_property_name, Object)

  // Begin flag bits ---------------------
  DECL_BOOLEAN_ACCESSORS(hidden_prototype)
  DECL_BOOLEAN_ACCESSORS(undetectable)

  // If set, object instances created by this function
  // requires access check.
  DECL_BOOLEAN_ACCESSORS(needs_access_check)

  DECL_BOOLEAN_ACCESSORS(read_only_prototype)

  // If set, do not create a prototype property for the associated
  // JSFunction. This bit implies that neither the prototype_template nor the
  // prototype_provoider_template are instantiated.
  DECL_BOOLEAN_ACCESSORS(remove_prototype)

  // If set, do not attach a serial number to this FunctionTemplate and thus do
  // not keep an instance boilerplate around.
  DECL_BOOLEAN_ACCESSORS(do_not_cache)

  // If not set an access may be performed on calling the associated JSFunction.
  DECL_BOOLEAN_ACCESSORS(accept_any_receiver)
  // End flag bits ---------------------

  DECL_CAST(FunctionTemplateInfo)

  // Dispatched behavior.
  DECL_PRINTER(FunctionTemplateInfo)
  DECL_VERIFIER(FunctionTemplateInfo)

  static const int kInvalidSerialNumber = 0;

  static const int kCallCodeOffset = TemplateInfo::kHeaderSize;
  static const int kPrototypeTemplateOffset =
      kCallCodeOffset + kPointerSize;
  static const int kPrototypeProviderTemplateOffset =
      kPrototypeTemplateOffset + kPointerSize;
  static const int kParentTemplateOffset =
      kPrototypeProviderTemplateOffset + kPointerSize;
  static const int kNamedPropertyHandlerOffset =
      kParentTemplateOffset + kPointerSize;
  static const int kIndexedPropertyHandlerOffset =
      kNamedPropertyHandlerOffset + kPointerSize;
  static const int kInstanceTemplateOffset =
      kIndexedPropertyHandlerOffset + kPointerSize;
  static const int kClassNameOffset = kInstanceTemplateOffset + kPointerSize;
  static const int kSignatureOffset = kClassNameOffset + kPointerSize;
  static const int kInstanceCallHandlerOffset = kSignatureOffset + kPointerSize;
  static const int kAccessCheckInfoOffset =
      kInstanceCallHandlerOffset + kPointerSize;
  static const int kSharedFunctionInfoOffset =
      kAccessCheckInfoOffset + kPointerSize;
  static const int kFlagOffset = kSharedFunctionInfoOffset + kPointerSize;
  static const int kLengthOffset = kFlagOffset + kPointerSize;
  static const int kCachedPropertyNameOffset = kLengthOffset + kPointerSize;
  static const int kSize = kCachedPropertyNameOffset + kPointerSize;

  static Handle<SharedFunctionInfo> GetOrCreateSharedFunctionInfo(
      Isolate* isolate, Handle<FunctionTemplateInfo> info,
      MaybeHandle<Name> maybe_name);
  // Returns parent function template or null.
  inline FunctionTemplateInfo* GetParent(Isolate* isolate);
  // Returns true if |object| is an instance of this function template.
  inline bool IsTemplateFor(JSObject* object);
  bool IsTemplateFor(Map* map);
  inline bool instantiated();

  // Helper function for cached accessors.
  static MaybeHandle<Name> TryGetCachedPropertyName(Isolate* isolate,
                                                    Handle<Object> getter);

 private:
  // Bit position in the flag, from least significant bit position.
  static const int kHiddenPrototypeBit   = 0;
  static const int kUndetectableBit      = 1;
  static const int kNeedsAccessCheckBit  = 2;
  static const int kReadOnlyPrototypeBit = 3;
  static const int kRemovePrototypeBit   = 4;
  static const int kDoNotCacheBit        = 5;
  static const int kAcceptAnyReceiver = 6;

  DISALLOW_IMPLICIT_CONSTRUCTORS(FunctionTemplateInfo);
};


class ObjectTemplateInfo: public TemplateInfo {
 public:
  DECL_ACCESSORS(constructor, Object)
  DECL_ACCESSORS(data, Object)
  DECL_INT_ACCESSORS(embedder_field_count)
  DECL_BOOLEAN_ACCESSORS(immutable_proto)

  DECL_CAST(ObjectTemplateInfo)

  // Dispatched behavior.
  DECL_PRINTER(ObjectTemplateInfo)
  DECL_VERIFIER(ObjectTemplateInfo)

  static const int kConstructorOffset = TemplateInfo::kHeaderSize;
  // LSB is for immutable_proto, higher bits for embedder_field_count
  static const int kDataOffset = kConstructorOffset + kPointerSize;
  static const int kSize = kDataOffset + kPointerSize;

  // Starting from given object template's constructor walk up the inheritance
  // chain till a function template that has an instance template is found.
  inline ObjectTemplateInfo* GetParent(Isolate* isolate);

 private:
  class IsImmutablePrototype : public BitField<bool, 0, 1> {};
  class EmbedderFieldCount
      : public BitField<int, IsImmutablePrototype::kNext, 29> {};
};

class StackFrameInfo : public Struct {
 public:
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
  DECL_ACCESSORS(stack_frame_cache, UnseededNumberDictionary)

  DECL_CAST(SourcePositionTableWithFrameCache)

  static const int kSourcePositionTableIndex = Struct::kHeaderSize;
  static const int kStackFrameCacheIndex =
      kSourcePositionTableIndex + kPointerSize;
  static const int kSize = kStackFrameCacheIndex + kPointerSize;

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(SourcePositionTableWithFrameCache);
};

// Abstract base class for visiting, and optionally modifying, the
// pointers contained in Objects. Used in GC and serialization/deserialization.
// TODO(ulan): move to src/visitors.h
class ObjectVisitor BASE_EMBEDDED {
 public:
  virtual ~ObjectVisitor() {}

  // Visits a contiguous arrays of pointers in the half-open range
  // [start, end). Any or all of the values may be modified on return.
  virtual void VisitPointers(HeapObject* host, Object** start,
                             Object** end) = 0;

  // Handy shorthand for visiting a single pointer.
  virtual void VisitPointer(HeapObject* host, Object** p) {
    VisitPointers(host, p, p + 1);
  }

  // Visit weak next_code_link in Code object.
  virtual void VisitNextCodeLink(Code* host, Object** p) {
    VisitPointers(host, p, p + 1);
  }

  // To allow lazy clearing of inline caches the visitor has
  // a rich interface for iterating over Code objects..

  // Visits a code target in the instruction stream.
  virtual void VisitCodeTarget(Code* host, RelocInfo* rinfo);

  // Visits a code entry in a JS function.
  virtual void VisitCodeEntry(JSFunction* host, Address entry_address);

  // Visits a global property cell reference in the instruction stream.
  virtual void VisitCellPointer(Code* host, RelocInfo* rinfo);

  // Visits a runtime entry in the instruction stream.
  virtual void VisitRuntimeEntry(Code* host, RelocInfo* rinfo) {}

  // Visits a debug call target in the instruction stream.
  virtual void VisitDebugTarget(Code* host, RelocInfo* rinfo);

  // Visits the byte sequence in a function's prologue that contains information
  // about the code's age.
  virtual void VisitCodeAgeSequence(Code* host, RelocInfo* rinfo);

  // Visit pointer embedded into a code object.
  virtual void VisitEmbeddedPointer(Code* host, RelocInfo* rinfo);

  // Visits an external reference embedded into a code object.
  virtual void VisitExternalReference(Code* host, RelocInfo* rinfo) {}

  // Visits an external reference.
  virtual void VisitExternalReference(Foreign* host, Address* p) {}

  // Visits an (encoded) internal reference.
  virtual void VisitInternalReference(Code* host, RelocInfo* rinfo) {}
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
