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
#include "src/builtins/builtins.h"
#include "src/checks.h"
#include "src/elements-kind.h"
#include "src/field-index.h"
#include "src/flags.h"
#include "src/list.h"
#include "src/messages.h"
#include "src/property-details.h"
#include "src/unicode-decoder.h"
#include "src/unicode.h"
#include "src/zone/zone.h"

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
//       - JSProxy
//     - FixedArrayBase
//       - ByteArray
//       - BytecodeArray
//       - FixedArray
//         - DescriptorArray
//         - FrameArray
//         - LiteralsArray
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
//       - FixedDoubleArray
//     - Name
//       - String
//         - SeqString
//           - SeqOneByteString
//           - SeqTwoByteString
//         - SlicedString
//         - ConsString
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
//     - Simd128Value
//       - Float32x4
//       - Int32x4
//       - Uint32x4
//       - Bool32x4
//       - Int16x8
//       - Uint16x8
//       - Bool16x8
//       - Int8x16
//       - Uint8x16
//       - Bool8x16
//     - Cell
//     - PropertyCell
//     - Code
//     - AbstractCode, a wrapper around Code or BytecodeArray
//     - Map
//     - Oddball
//     - Foreign
//     - SharedFunctionInfo
//     - Struct
//       - Box
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
//       - CodeCache
//       - PrototypeInfo
//       - Module
//       - ModuleInfoEntry
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


enum ExternalArrayType {
  kExternalInt8Array = 1,
  kExternalUint8Array,
  kExternalInt16Array,
  kExternalUint16Array,
  kExternalInt32Array,
  kExternalUint32Array,
  kExternalFloat32Array,
  kExternalFloat64Array,
  kExternalUint8ClampedArray,
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


// Indicates how aggressively the prototype should be optimized. FAST_PROTOTYPE
// will give the fastest result by tailoring the map to the prototype, but that
// will cause polymorphism with other objects. REGULAR_PROTOTYPE is to be used
// (at least for now) when dynamically modifying the prototype chain of an
// object using __proto__ or Object.setPrototypeOf.
enum PrototypeOptimizationMode { REGULAR_PROTOTYPE, FAST_PROTOTYPE };


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
#define INSTANCE_TYPE_LIST(V)                                   \
  V(INTERNALIZED_STRING_TYPE)                                   \
  V(EXTERNAL_INTERNALIZED_STRING_TYPE)                          \
  V(ONE_BYTE_INTERNALIZED_STRING_TYPE)                          \
  V(EXTERNAL_ONE_BYTE_INTERNALIZED_STRING_TYPE)                 \
  V(EXTERNAL_INTERNALIZED_STRING_WITH_ONE_BYTE_DATA_TYPE)       \
  V(SHORT_EXTERNAL_INTERNALIZED_STRING_TYPE)                    \
  V(SHORT_EXTERNAL_ONE_BYTE_INTERNALIZED_STRING_TYPE)           \
  V(SHORT_EXTERNAL_INTERNALIZED_STRING_WITH_ONE_BYTE_DATA_TYPE) \
  V(STRING_TYPE)                                                \
  V(CONS_STRING_TYPE)                                           \
  V(EXTERNAL_STRING_TYPE)                                       \
  V(SLICED_STRING_TYPE)                                         \
  V(ONE_BYTE_STRING_TYPE)                                       \
  V(CONS_ONE_BYTE_STRING_TYPE)                                  \
  V(EXTERNAL_ONE_BYTE_STRING_TYPE)                              \
  V(SLICED_ONE_BYTE_STRING_TYPE)                                \
  V(EXTERNAL_STRING_WITH_ONE_BYTE_DATA_TYPE)                    \
  V(SHORT_EXTERNAL_STRING_TYPE)                                 \
  V(SHORT_EXTERNAL_ONE_BYTE_STRING_TYPE)                        \
  V(SHORT_EXTERNAL_STRING_WITH_ONE_BYTE_DATA_TYPE)              \
                                                                \
  V(SYMBOL_TYPE)                                                \
  V(HEAP_NUMBER_TYPE)                                           \
  V(SIMD128_VALUE_TYPE)                                         \
  V(ODDBALL_TYPE)                                               \
                                                                \
  V(MAP_TYPE)                                                   \
  V(CODE_TYPE)                                                  \
  V(MUTABLE_HEAP_NUMBER_TYPE)                                   \
  V(FOREIGN_TYPE)                                               \
  V(BYTE_ARRAY_TYPE)                                            \
  V(BYTECODE_ARRAY_TYPE)                                        \
  V(FREE_SPACE_TYPE)                                            \
                                                                \
  V(FIXED_INT8_ARRAY_TYPE)                                      \
  V(FIXED_UINT8_ARRAY_TYPE)                                     \
  V(FIXED_INT16_ARRAY_TYPE)                                     \
  V(FIXED_UINT16_ARRAY_TYPE)                                    \
  V(FIXED_INT32_ARRAY_TYPE)                                     \
  V(FIXED_UINT32_ARRAY_TYPE)                                    \
  V(FIXED_FLOAT32_ARRAY_TYPE)                                   \
  V(FIXED_FLOAT64_ARRAY_TYPE)                                   \
  V(FIXED_UINT8_CLAMPED_ARRAY_TYPE)                             \
                                                                \
  V(FIXED_DOUBLE_ARRAY_TYPE)                                    \
  V(FILLER_TYPE)                                                \
                                                                \
  V(ACCESSOR_INFO_TYPE)                                         \
  V(ACCESSOR_PAIR_TYPE)                                         \
  V(ACCESS_CHECK_INFO_TYPE)                                     \
  V(INTERCEPTOR_INFO_TYPE)                                      \
  V(CALL_HANDLER_INFO_TYPE)                                     \
  V(FUNCTION_TEMPLATE_INFO_TYPE)                                \
  V(OBJECT_TEMPLATE_INFO_TYPE)                                  \
  V(ALLOCATION_SITE_TYPE)                                       \
  V(ALLOCATION_MEMENTO_TYPE)                                    \
  V(SCRIPT_TYPE)                                                \
  V(TYPE_FEEDBACK_INFO_TYPE)                                    \
  V(ALIASED_ARGUMENTS_ENTRY_TYPE)                               \
  V(BOX_TYPE)                                                   \
  V(PROMISE_RESOLVE_THENABLE_JOB_INFO_TYPE)                     \
  V(PROMISE_REACTION_JOB_INFO_TYPE)                             \
  V(DEBUG_INFO_TYPE)                                            \
  V(BREAK_POINT_INFO_TYPE)                                      \
  V(PROTOTYPE_INFO_TYPE)                                        \
  V(TUPLE2_TYPE)                                                \
  V(TUPLE3_TYPE)                                                \
  V(CONTEXT_EXTENSION_TYPE)                                     \
  V(CONSTANT_ELEMENTS_PAIR_TYPE)                                \
  V(MODULE_TYPE)                                                \
  V(MODULE_INFO_ENTRY_TYPE)                                     \
  V(FIXED_ARRAY_TYPE)                                           \
  V(TRANSITION_ARRAY_TYPE)                                      \
  V(SHARED_FUNCTION_INFO_TYPE)                                  \
  V(CELL_TYPE)                                                  \
  V(WEAK_CELL_TYPE)                                             \
  V(PROPERTY_CELL_TYPE)                                         \
                                                                \
  V(JS_PROXY_TYPE)                                              \
  V(JS_GLOBAL_OBJECT_TYPE)                                      \
  V(JS_GLOBAL_PROXY_TYPE)                                       \
  V(JS_SPECIAL_API_OBJECT_TYPE)                                 \
  V(JS_VALUE_TYPE)                                              \
  V(JS_MESSAGE_OBJECT_TYPE)                                     \
  V(JS_DATE_TYPE)                                               \
  V(JS_API_OBJECT_TYPE)                                         \
  V(JS_OBJECT_TYPE)                                             \
  V(JS_ARGUMENTS_TYPE)                                          \
  V(JS_CONTEXT_EXTENSION_OBJECT_TYPE)                           \
  V(JS_GENERATOR_OBJECT_TYPE)                                   \
  V(JS_MODULE_NAMESPACE_TYPE)                                   \
  V(JS_ARRAY_TYPE)                                              \
  V(JS_ARRAY_BUFFER_TYPE)                                       \
  V(JS_TYPED_ARRAY_TYPE)                                        \
  V(JS_DATA_VIEW_TYPE)                                          \
  V(JS_SET_TYPE)                                                \
  V(JS_MAP_TYPE)                                                \
  V(JS_SET_ITERATOR_TYPE)                                       \
  V(JS_MAP_ITERATOR_TYPE)                                       \
  V(JS_WEAK_MAP_TYPE)                                           \
  V(JS_WEAK_SET_TYPE)                                           \
  V(JS_PROMISE_CAPABILITY_TYPE)                                 \
  V(JS_PROMISE_TYPE)                                            \
  V(JS_REGEXP_TYPE)                                             \
  V(JS_ERROR_TYPE)                                              \
  V(JS_STRING_ITERATOR_TYPE)                                    \
                                                                \
  V(JS_TYPED_ARRAY_KEY_ITERATOR_TYPE)                           \
  V(JS_FAST_ARRAY_KEY_ITERATOR_TYPE)                            \
  V(JS_GENERIC_ARRAY_KEY_ITERATOR_TYPE)                         \
                                                                \
  V(JS_UINT8_ARRAY_KEY_VALUE_ITERATOR_TYPE)                     \
  V(JS_INT8_ARRAY_KEY_VALUE_ITERATOR_TYPE)                      \
  V(JS_UINT16_ARRAY_KEY_VALUE_ITERATOR_TYPE)                    \
  V(JS_INT16_ARRAY_KEY_VALUE_ITERATOR_TYPE)                     \
  V(JS_UINT32_ARRAY_KEY_VALUE_ITERATOR_TYPE)                    \
  V(JS_INT32_ARRAY_KEY_VALUE_ITERATOR_TYPE)                     \
  V(JS_FLOAT32_ARRAY_KEY_VALUE_ITERATOR_TYPE)                   \
  V(JS_FLOAT64_ARRAY_KEY_VALUE_ITERATOR_TYPE)                   \
  V(JS_UINT8_CLAMPED_ARRAY_KEY_VALUE_ITERATOR_TYPE)             \
                                                                \
  V(JS_FAST_SMI_ARRAY_KEY_VALUE_ITERATOR_TYPE)                  \
  V(JS_FAST_HOLEY_SMI_ARRAY_KEY_VALUE_ITERATOR_TYPE)            \
  V(JS_FAST_ARRAY_KEY_VALUE_ITERATOR_TYPE)                      \
  V(JS_FAST_HOLEY_ARRAY_KEY_VALUE_ITERATOR_TYPE)                \
  V(JS_FAST_DOUBLE_ARRAY_KEY_VALUE_ITERATOR_TYPE)               \
  V(JS_FAST_HOLEY_DOUBLE_ARRAY_KEY_VALUE_ITERATOR_TYPE)         \
  V(JS_GENERIC_ARRAY_KEY_VALUE_ITERATOR_TYPE)                   \
                                                                \
  V(JS_UINT8_ARRAY_VALUE_ITERATOR_TYPE)                         \
  V(JS_INT8_ARRAY_VALUE_ITERATOR_TYPE)                          \
  V(JS_UINT16_ARRAY_VALUE_ITERATOR_TYPE)                        \
  V(JS_INT16_ARRAY_VALUE_ITERATOR_TYPE)                         \
  V(JS_UINT32_ARRAY_VALUE_ITERATOR_TYPE)                        \
  V(JS_INT32_ARRAY_VALUE_ITERATOR_TYPE)                         \
  V(JS_FLOAT32_ARRAY_VALUE_ITERATOR_TYPE)                       \
  V(JS_FLOAT64_ARRAY_VALUE_ITERATOR_TYPE)                       \
  V(JS_UINT8_CLAMPED_ARRAY_VALUE_ITERATOR_TYPE)                 \
                                                                \
  V(JS_FAST_SMI_ARRAY_VALUE_ITERATOR_TYPE)                      \
  V(JS_FAST_HOLEY_SMI_ARRAY_VALUE_ITERATOR_TYPE)                \
  V(JS_FAST_ARRAY_VALUE_ITERATOR_TYPE)                          \
  V(JS_FAST_HOLEY_ARRAY_VALUE_ITERATOR_TYPE)                    \
  V(JS_FAST_DOUBLE_ARRAY_VALUE_ITERATOR_TYPE)                   \
  V(JS_FAST_HOLEY_DOUBLE_ARRAY_VALUE_ITERATOR_TYPE)             \
  V(JS_GENERIC_ARRAY_VALUE_ITERATOR_TYPE)                       \
                                                                \
  V(JS_BOUND_FUNCTION_TYPE)                                     \
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
    ShortExternalInternalizedStringWithOneByteData)

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
  V(CALL_HANDLER_INFO, CallHandlerInfo, call_handler_info)                   \
  V(FUNCTION_TEMPLATE_INFO, FunctionTemplateInfo, function_template_info)    \
  V(OBJECT_TEMPLATE_INFO, ObjectTemplateInfo, object_template_info)          \
  V(ALLOCATION_SITE, AllocationSite, allocation_site)                        \
  V(ALLOCATION_MEMENTO, AllocationMemento, allocation_memento)               \
  V(SCRIPT, Script, script)                                                  \
  V(TYPE_FEEDBACK_INFO, TypeFeedbackInfo, type_feedback_info)                \
  V(ALIASED_ARGUMENTS_ENTRY, AliasedArgumentsEntry, aliased_arguments_entry) \
  V(BOX, Box, box)                                                           \
  V(PROMISE_RESOLVE_THENABLE_JOB_INFO, PromiseResolveThenableJobInfo,        \
    promise_resolve_thenable_job_info)                                       \
  V(PROMISE_REACTION_JOB_INFO, PromiseReactionJobInfo,                       \
    promise_reaction_job_info)                                               \
  V(DEBUG_INFO, DebugInfo, debug_info)                                       \
  V(BREAK_POINT_INFO, BreakPointInfo, break_point_info)                      \
  V(PROTOTYPE_INFO, PrototypeInfo, prototype_info)                           \
  V(TUPLE2, Tuple2, tuple2)                                                  \
  V(TUPLE3, Tuple3, tuple3)                                                  \
  V(CONTEXT_EXTENSION, ContextExtension, context_extension)                  \
  V(CONSTANT_ELEMENTS_PAIR, ConstantElementsPair, constant_elements_pair)    \
  V(MODULE, Module, module)                                                  \
  V(MODULE_INFO_ENTRY, ModuleInfoEntry, module_info_entry)

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

// If bit 7 is clear then bit 2 indicates whether the string consists of
// two-byte characters or one-byte characters.
const uint32_t kStringEncodingMask = 0x4;
const uint32_t kTwoByteStringTag = 0x0;
const uint32_t kOneByteStringTag = 0x4;

// If bit 7 is clear, the low-order 2 bits indicate the representation
// of the string.
const uint32_t kStringRepresentationMask = 0x03;
enum StringRepresentationTag {
  kSeqStringTag = 0x0,
  kConsStringTag = 0x1,
  kExternalStringTag = 0x2,
  kSlicedStringTag = 0x3
};
const uint32_t kIsIndirectStringMask = 0x1;
const uint32_t kIsIndirectStringTag = 0x1;
STATIC_ASSERT((kSeqStringTag & kIsIndirectStringMask) == 0);  // NOLINT
STATIC_ASSERT((kExternalStringTag & kIsIndirectStringMask) == 0);  // NOLINT
STATIC_ASSERT((kConsStringTag &
               kIsIndirectStringMask) == kIsIndirectStringTag);  // NOLINT
STATIC_ASSERT((kSlicedStringTag &
               kIsIndirectStringMask) == kIsIndirectStringTag);  // NOLINT

// Use this mask to distinguish between cons and slice only after making
// sure that the string is one of the two (an indirect string).
const uint32_t kSlicedNotConsMask = kSlicedStringTag & ~kConsStringTag;
STATIC_ASSERT(IS_POWER_OF_TWO(kSlicedNotConsMask));

// If bit 7 is clear, then bit 3 indicates whether this two-byte
// string actually contains one byte data.
const uint32_t kOneByteDataHintMask = 0x08;
const uint32_t kOneByteDataHintTag = 0x08;

// If bit 7 is clear and string representation indicates an external string,
// then bit 4 indicates whether the data pointer is cached.
const uint32_t kShortExternalStringMask = 0x10;
const uint32_t kShortExternalStringTag = 0x10;

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

enum InstanceType {
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

  // Non-string names
  SYMBOL_TYPE = kNotStringTag,  // FIRST_NONSTRING_TYPE, LAST_NAME_TYPE

  // Other primitives (cannot contain non-map-word pointers to heap objects).
  HEAP_NUMBER_TYPE,
  SIMD128_VALUE_TYPE,
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
  CALL_HANDLER_INFO_TYPE,
  FUNCTION_TEMPLATE_INFO_TYPE,
  OBJECT_TEMPLATE_INFO_TYPE,
  ALLOCATION_SITE_TYPE,
  ALLOCATION_MEMENTO_TYPE,
  SCRIPT_TYPE,
  TYPE_FEEDBACK_INFO_TYPE,
  ALIASED_ARGUMENTS_ENTRY_TYPE,
  BOX_TYPE,
  PROMISE_RESOLVE_THENABLE_JOB_INFO_TYPE,
  PROMISE_REACTION_JOB_INFO_TYPE,
  DEBUG_INFO_TYPE,
  BREAK_POINT_INFO_TYPE,
  PROTOTYPE_INFO_TYPE,
  TUPLE2_TYPE,
  TUPLE3_TYPE,
  CONTEXT_EXTENSION_TYPE,
  CONSTANT_ELEMENTS_PAIR_TYPE,
  MODULE_TYPE,
  MODULE_INFO_ENTRY_TYPE,
  FIXED_ARRAY_TYPE,
  TRANSITION_ARRAY_TYPE,
  SHARED_FUNCTION_INFO_TYPE,
  CELL_TYPE,
  WEAK_CELL_TYPE,
  PROPERTY_CELL_TYPE,

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
  JS_MODULE_NAMESPACE_TYPE,
  JS_ARRAY_TYPE,
  JS_ARRAY_BUFFER_TYPE,
  JS_TYPED_ARRAY_TYPE,
  JS_DATA_VIEW_TYPE,
  JS_SET_TYPE,
  JS_MAP_TYPE,
  JS_SET_ITERATOR_TYPE,
  JS_MAP_ITERATOR_TYPE,
  JS_WEAK_MAP_TYPE,
  JS_WEAK_SET_TYPE,
  JS_PROMISE_CAPABILITY_TYPE,
  JS_PROMISE_TYPE,
  JS_REGEXP_TYPE,
  JS_ERROR_TYPE,
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
  V(FAST_ELEMENTS_SUB_TYPE)                      \
  V(FAST_PROPERTIES_SUB_TYPE)                    \
  V(FAST_TEMPLATE_INSTANTIATIONS_CACHE_SUB_TYPE) \
  V(HANDLER_TABLE_SUB_TYPE)                      \
  V(JS_COLLECTION_SUB_TYPE)                      \
  V(JS_WEAK_COLLECTION_SUB_TYPE)                 \
  V(LITERALS_ARRAY_SUB_TYPE)                     \
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
class AllocationSiteCreationContext;
class AllocationSiteUsageContext;
class Cell;
class ConsString;
class ElementsAccessor;
class FixedArrayBase;
class FunctionLiteral;
class JSGlobalObject;
class KeyAccumulator;
class LayoutDescriptor;
class LiteralsArray;
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
class SafepointEntry;
class SharedFunctionInfo;
class StringStream;
class TypeFeedbackInfo;
class FeedbackMetadata;
class FeedbackVector;
class WeakCell;
class TransitionArray;
class TemplateList;

// A template-ized version of the IsXXX functions.
template <class C> inline bool Is(Object* obj);

#ifdef OBJECT_PRINT
#define DECLARE_PRINTER(Name) void Name##Print(std::ostream& os);  // NOLINT
#else
#define DECLARE_PRINTER(Name)
#endif

#define OBJECT_TYPE_LIST(V) \
  V(Smi)                    \
  V(LayoutDescriptor)       \
  V(HeapObject)             \
  V(Primitive)              \
  V(Number)

#define HEAP_OBJECT_TYPE_LIST(V) \
  V(HeapNumber)                  \
  V(MutableHeapNumber)           \
  V(Simd128Value)                \
  V(Float32x4)                   \
  V(Int32x4)                     \
  V(Uint32x4)                    \
  V(Bool32x4)                    \
  V(Int16x8)                     \
  V(Uint16x8)                    \
  V(Bool16x8)                    \
  V(Int8x16)                     \
  V(Uint8x16)                    \
  V(Bool8x16)                    \
  V(Name)                        \
  V(UniqueName)                  \
  V(String)                      \
  V(SeqString)                   \
  V(ExternalString)              \
  V(ConsString)                  \
  V(SlicedString)                \
  V(ExternalTwoByteString)       \
  V(ExternalOneByteString)       \
  V(SeqTwoByteString)            \
  V(SeqOneByteString)            \
  V(InternalizedString)          \
  V(Symbol)                      \
                                 \
  V(FixedTypedArrayBase)         \
  V(FixedUint8Array)             \
  V(FixedInt8Array)              \
  V(FixedUint16Array)            \
  V(FixedInt16Array)             \
  V(FixedUint32Array)            \
  V(FixedInt32Array)             \
  V(FixedFloat32Array)           \
  V(FixedFloat64Array)           \
  V(FixedUint8ClampedArray)      \
  V(ByteArray)                   \
  V(BytecodeArray)               \
  V(FreeSpace)                   \
  V(JSReceiver)                  \
  V(JSObject)                    \
  V(JSArgumentsObject)           \
  V(JSContextExtensionObject)    \
  V(JSGeneratorObject)           \
  V(JSModuleNamespace)           \
  V(Map)                         \
  V(DescriptorArray)             \
  V(FrameArray)                  \
  V(TransitionArray)             \
  V(LiteralsArray)               \
  V(FeedbackMetadata)            \
  V(FeedbackVector)              \
  V(DeoptimizationInputData)     \
  V(DeoptimizationOutputData)    \
  V(DependentCode)               \
  V(HandlerTable)                \
  V(FixedArray)                  \
  V(FixedDoubleArray)            \
  V(WeakFixedArray)              \
  V(ArrayList)                   \
  V(RegExpMatchInfo)             \
  V(Context)                     \
  V(ScriptContextTable)          \
  V(NativeContext)               \
  V(ScopeInfo)                   \
  V(ModuleInfo)                  \
  V(JSBoundFunction)             \
  V(JSFunction)                  \
  V(Code)                        \
  V(AbstractCode)                \
  V(Oddball)                     \
  V(SharedFunctionInfo)          \
  V(JSValue)                     \
  V(JSDate)                      \
  V(JSMessageObject)             \
  V(StringWrapper)               \
  V(Foreign)                     \
  V(Boolean)                     \
  V(JSArray)                     \
  V(JSArrayBuffer)               \
  V(JSArrayBufferView)           \
  V(JSCollection)                \
  V(JSTypedArray)                \
  V(JSArrayIterator)             \
  V(JSDataView)                  \
  V(JSProxy)                     \
  V(JSError)                     \
  V(JSPromiseCapability)         \
  V(JSPromise)                   \
  V(JSStringIterator)            \
  V(JSSet)                       \
  V(JSMap)                       \
  V(JSSetIterator)               \
  V(JSMapIterator)               \
  V(JSWeakCollection)            \
  V(JSWeakMap)                   \
  V(JSWeakSet)                   \
  V(JSRegExp)                    \
  V(HashTable)                   \
  V(Dictionary)                  \
  V(UnseededNumberDictionary)    \
  V(StringTable)                 \
  V(StringSet)                   \
  V(NormalizedMapCache)          \
  V(CompilationCacheTable)       \
  V(CodeCacheHashTable)          \
  V(MapCache)                    \
  V(JSGlobalObject)              \
  V(JSGlobalProxy)               \
  V(Undetectable)                \
  V(AccessCheckNeeded)           \
  V(Callable)                    \
  V(Function)                    \
  V(Constructor)                 \
  V(TemplateInfo)                \
  V(Filler)                      \
  V(FixedArrayBase)              \
  V(External)                    \
  V(Struct)                      \
  V(Cell)                        \
  V(TemplateList)                \
  V(PropertyCell)                \
  V(WeakCell)                    \
  V(ObjectHashTable)             \
  V(ObjectHashSet)               \
  V(WeakHashTable)               \
  V(OrderedHashTable)

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

#define DECLARE_STRUCT_PREDICATE(NAME, Name, name) \
  INLINE(bool Is##Name() const);
  STRUCT_LIST(DECLARE_STRUCT_PREDICATE)
#undef DECLARE_STRUCT_PREDICATE

  // ES6, section 7.2.2 IsArray.  NOT to be confused with %_IsArray.
  MUST_USE_RESULT static Maybe<bool> IsArray(Handle<Object> object);

  INLINE(bool IsNameDictionary() const);
  INLINE(bool IsGlobalDictionary() const);
  INLINE(bool IsSeededNumberDictionary() const);
  INLINE(bool IsOrderedHashSet() const);
  INLINE(bool IsOrderedHashMap() const);

  // Extract the number.
  inline double Number() const;
  INLINE(bool IsNaN() const);
  INLINE(bool IsMinusZero() const);
  bool ToInt32(int32_t* value);
  inline bool ToUint32(uint32_t* value);

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

  // Convert to a JSObject if needed.
  // native_context is used when creating wrapper object.
  MUST_USE_RESULT static inline MaybeHandle<JSReceiver> ToObject(
      Isolate* isolate, Handle<Object> object);
  MUST_USE_RESULT static MaybeHandle<JSReceiver> ToObject(
      Isolate* isolate, Handle<Object> object, Handle<Context> context);

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

  // Tries to convert an object to an array length. Returns true and sets the
  // output parameter if it succeeds.
  inline bool ToArrayLength(uint32_t* index);

  // Tries to convert an object to an array index. Returns true and sets the
  // output parameter if it succeeds. Equivalent to ToArrayLength, but does not
  // allow kMaxUInt32.
  inline bool ToArrayIndex(uint32_t* index);

  // Returns true if the result of iterating over the object is the same
  // (including observable effects) as simply accessing the properties between 0
  // and length.
  bool IterationHasObservableEffects();

  DECLARE_VERIFIER(Object)
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

  DECLARE_CAST(Object)

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
  Map* GetPrototypeChainRootMap(Isolate* isolate);

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
bool Object::IsHeapObject() const { return Internals::HasHeapObjectTag(this); }


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

  // Returns whether value can be represented in a Smi.
  static inline bool IsValid(intptr_t value) {
    bool result = Internals::IsValidSmi(value);
    DCHECK_EQ(result, value >= kMinValue && value <= kMaxValue);
    return result;
  }

  DECLARE_CAST(Smi)

  // Dispatched behavior.
  V8_EXPORT_PRIVATE void SmiPrint(std::ostream& os) const;  // NOLINT
  DECLARE_VERIFIER(Smi)

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
  inline Map* ToMap();


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
  // The no-write-barrier version.  This is OK if the object is white and in
  // new space, or if the value is an immortal immutable object, like the maps
  // of primitive (non-JS) objects like strings, heap numbers etc.
  inline void set_map_no_write_barrier(Map* value);

  // Get the map using acquire load.
  inline Map* synchronized_map();
  inline MapWord synchronized_map_word() const;

  // Set the map using release store
  inline void synchronized_set_map(Map* value);
  inline void synchronized_set_map_no_write_barrier(Map* value);
  inline void synchronized_set_map_word(MapWord map_word);

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

#define DECLARE_STRUCT_PREDICATE(NAME, Name, name) \
  INLINE(bool Is##Name() const);
  STRUCT_LIST(DECLARE_STRUCT_PREDICATE)
#undef DECLARE_STRUCT_PREDICATE

  // Converts an address to a HeapObject pointer.
  static inline HeapObject* FromAddress(Address address) {
    DCHECK_TAG_ALIGNED(address);
    return reinterpret_cast<HeapObject*>(address + kHeapObjectTag);
  }

  // Returns the address of this HeapObject.
  inline Address address() {
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
  inline int Size();

  // Given a heap object's map pointer, returns the heap size in bytes
  // Useful when the map pointer field is used for other purposes.
  // GC internal.
  inline int SizeFromMap(Map* map);

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

  DECLARE_CAST(HeapObject)

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
  DECLARE_PRINTER(HeapObject)
  DECLARE_VERIFIER(HeapObject)
#ifdef VERIFY_HEAP
  inline void VerifyObjectField(int offset);
  inline void VerifySmiField(int offset);

  // Verify a pointer is a valid HeapObject pointer that points to object
  // areas in the heap.
  static void VerifyHeapPointer(Object* p);
#endif

  inline AllocationAlignment RequiredAlignment();

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

  DECLARE_CAST(HeapNumber)

  // Dispatched behavior.
  bool HeapNumberBooleanValue();

  V8_EXPORT_PRIVATE void HeapNumberPrint(std::ostream& os);  // NOLINT
  DECLARE_VERIFIER(HeapNumber)

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


// The Simd128Value class describes heap allocated 128 bit SIMD values.
class Simd128Value : public HeapObject {
 public:
  DECLARE_CAST(Simd128Value)

  DECLARE_PRINTER(Simd128Value)
  DECLARE_VERIFIER(Simd128Value)

  static Handle<String> ToString(Handle<Simd128Value> input);

  // Equality operations.
  inline bool Equals(Simd128Value* that);
  static inline bool Equals(Handle<Simd128Value> one, Handle<Simd128Value> two);

  // Checks that another instance is bit-wise equal.
  bool BitwiseEquals(const Simd128Value* other) const;
  // Computes a hash from the 128 bit value, viewed as 4 32-bit integers.
  uint32_t Hash() const;
  // Copies the 16 bytes of SIMD data to the destination address.
  void CopyBits(void* destination) const;

  // Layout description.
  static const int kValueOffset = HeapObject::kHeaderSize;
  static const int kSize = kValueOffset + kSimd128Size;

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(Simd128Value);
};


// V has parameters (TYPE, Type, type, lane count, lane type)
#define SIMD128_TYPES(V)                       \
  V(FLOAT32X4, Float32x4, float32x4, 4, float) \
  V(INT32X4, Int32x4, int32x4, 4, int32_t)     \
  V(UINT32X4, Uint32x4, uint32x4, 4, uint32_t) \
  V(BOOL32X4, Bool32x4, bool32x4, 4, bool)     \
  V(INT16X8, Int16x8, int16x8, 8, int16_t)     \
  V(UINT16X8, Uint16x8, uint16x8, 8, uint16_t) \
  V(BOOL16X8, Bool16x8, bool16x8, 8, bool)     \
  V(INT8X16, Int8x16, int8x16, 16, int8_t)     \
  V(UINT8X16, Uint8x16, uint8x16, 16, uint8_t) \
  V(BOOL8X16, Bool8x16, bool8x16, 16, bool)

#define SIMD128_VALUE_CLASS(TYPE, Type, type, lane_count, lane_type) \
  class Type final : public Simd128Value {                           \
   public:                                                           \
    inline lane_type get_lane(int lane) const;                       \
    inline void set_lane(int lane, lane_type value);                 \
                                                                     \
    DECLARE_CAST(Type)                                               \
                                                                     \
    DECLARE_PRINTER(Type)                                            \
                                                                     \
    static Handle<String> ToString(Handle<Type> input);              \
                                                                     \
    inline bool Equals(Type* that);                                  \
                                                                     \
   private:                                                          \
    DISALLOW_IMPLICIT_CONSTRUCTORS(Type);                            \
  };
SIMD128_TYPES(SIMD128_VALUE_CLASS)
#undef SIMD128_VALUE_CLASS


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
  // [properties]: Backing storage for properties.
  // properties is a FixedArray in the fast case and a Dictionary in the
  // slow case.
  DECL_ACCESSORS(properties, FixedArray)  // Get and set fast properties.
  inline void initialize_properties();
  inline bool HasFastProperties();
  // Gets slow properties for non-global objects.
  inline NameDictionary* property_dictionary();

  // Deletes an existing named property in a normalized object.
  static void DeleteNormalizedProperty(Handle<JSReceiver> object,
                                       Handle<Name> name, int entry);

  DECLARE_CAST(JSReceiver)

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

  // Reads all enumerable own properties of source and adds them to target,
  // using either Set or CreateDataProperty depending on the use_set argument.
  MUST_USE_RESULT static Maybe<bool> SetOrCopyDataProperties(
      Isolate* isolate, Handle<JSReceiver> target, Handle<Object> source,
      bool use_set);

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
      ShouldThrow should_throw, Handle<Name> property_name = Handle<Name>());

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
  String* class_name();

  // Returns the constructor name (the name (possibly, inferred name) of the
  // function that was used to instantiate the object).
  static Handle<String> GetConstructorName(Handle<JSReceiver> receiver);

  Context* GetCreationContext();

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
  static const int kPropertiesOffset = HeapObject::kHeaderSize;
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
  static MUST_USE_RESULT MaybeHandle<JSObject> New(
      Handle<JSFunction> constructor, Handle<JSReceiver> new_target,
      Handle<AllocationSite> site = Handle<AllocationSite>::null());

  // Gets global object properties.
  inline GlobalDictionary* global_dictionary();

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
  static void ResetElements(Handle<JSObject> object);
  static inline void SetMapAndElements(Handle<JSObject> object,
                                       Handle<Map> map,
                                       Handle<FixedArrayBase> elements);
  inline ElementsKind GetElementsKind();
  ElementsAccessor* GetElementsAccessor();
  // Returns true if an object has elements of FAST_SMI_ELEMENTS ElementsKind.
  inline bool HasFastSmiElements();
  // Returns true if an object has elements of FAST_ELEMENTS ElementsKind.
  inline bool HasFastObjectElements();
  // Returns true if an object has elements of FAST_ELEMENTS or
  // FAST_SMI_ONLY_ELEMENTS.
  inline bool HasFastSmiOrObjectElements();
  // Returns true if an object has any of the fast elements kinds.
  inline bool HasFastElements();
  // Returns true if an object has elements of FAST_DOUBLE_ELEMENTS
  // ElementsKind.
  inline bool HasFastDoubleElements();
  // Returns true if an object has elements of FAST_HOLEY_*_ELEMENTS
  // ElementsKind.
  inline bool HasFastHoleyElements();
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

  // Collects elements starting at index 0.
  // Undefined values are placed after non-undefined values.
  // Returns the number of non-undefined values.
  static Handle<Object> PrepareElementsForSort(Handle<JSObject> object,
                                               uint32_t limit);
  // As PrepareElementsForSort, but only on objects where elements is
  // a dictionary, and it will stay a dictionary.  Collates undefined and
  // unexisting elements below limit from position zero of the elements.
  static Handle<Object> PrepareSlowElementsForSort(Handle<JSObject> object,
                                                   uint32_t limit);

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

  static void OptimizeAsPrototype(Handle<JSObject> object,
                                  PrototypeOptimizationMode mode);
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

  static void ValidateElements(Handle<JSObject> object);

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

  // Computes the new capacity when expanding the elements of a JSObject.
  static uint32_t NewElementsCapacity(uint32_t old_capacity) {
    // (old_capacity + 50%) + 16
    return old_capacity + (old_capacity >> 1) + 16;
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
  // internal fields as well as the number of internal fields.
  static inline int GetHeaderSize(InstanceType instance_type);
  inline int GetHeaderSize();

  static inline int GetInternalFieldCount(Map* map);
  inline int GetInternalFieldCount();
  inline int GetInternalFieldOffset(int index);
  inline Object* GetInternalField(int index);
  inline void SetInternalField(int index, Object* value);
  inline void SetInternalField(int index, Smi* value);
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

  inline void FastPropertyAtPut(FieldIndex index, Object* value);
  inline void RawFastPropertyAtPut(FieldIndex index, Object* value);
  inline void RawFastDoublePropertyAtPut(FieldIndex index, double value);
  inline void WriteToField(int descriptor, PropertyDetails details,
                           Object* value);
  inline void WriteToField(int descriptor, Object* value);

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

  MUST_USE_RESULT static Maybe<bool> PreventExtensions(
      Handle<JSObject> object, ShouldThrow should_throw);

  static bool IsExtensible(Handle<JSObject> object);

  // Copy object.
  enum DeepCopyHints { kNoHints = 0, kObjectIsShallow = 1 };

  MUST_USE_RESULT static MaybeHandle<JSObject> DeepCopy(
      Handle<JSObject> object,
      AllocationSiteUsageContext* site_context,
      DeepCopyHints hints = kNoHints);
  MUST_USE_RESULT static MaybeHandle<JSObject> DeepWalk(
      Handle<JSObject> object,
      AllocationSiteCreationContext* site_context);

  DECLARE_CAST(JSObject)

  // Dispatched behavior.
  void JSObjectShortPrint(StringStream* accumulator);
  DECLARE_PRINTER(JSObject)
  DECLARE_VERIFIER(JSObject)
#ifdef OBJECT_PRINT
  bool PrintProperties(std::ostream& os);  // NOLINT
  bool PrintElements(std::ostream& os);    // NOLINT
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

  typedef FlexibleBodyDescriptor<JSReceiver::kPropertiesOffset> BodyDescriptor;

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


// Common superclass for JSSloppyArgumentsObject and JSStrictArgumentsObject.
class JSArgumentsObject: public JSObject {
 public:
  // Offsets of object fields.
  static const int kLengthOffset = JSObject::kHeaderSize;
  static const int kHeaderSize = kLengthOffset + kPointerSize;
  // Indices of in-object properties.
  static const int kLengthIndex = 0;

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(JSArgumentsObject);
};


// JSSloppyArgumentsObject is just a JSObject with specific initial map.
// This initial map adds in-object properties for "length" and "callee".
class JSSloppyArgumentsObject: public JSArgumentsObject {
 public:
  // Offsets of object fields.
  static const int kCalleeOffset = JSArgumentsObject::kHeaderSize;
  static const int kSize = kCalleeOffset + kPointerSize;
  // Indices of in-object properties.
  static const int kCalleeIndex = 1;

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(JSSloppyArgumentsObject);
};


// JSStrictArgumentsObject is just a JSObject with specific initial map.
// This initial map adds an in-object property for "length".
class JSStrictArgumentsObject: public JSArgumentsObject {
 public:
  // Offsets of object fields.
  static const int kSize = JSArgumentsObject::kHeaderSize;

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(JSStrictArgumentsObject);
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

  DECLARE_CAST(FixedArrayBase)

  static int GetMaxLengthForNewSpaceAllocation(ElementsKind kind);

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
  void CopyTo(int pos, FixedArray* dest, int dest_pos, int len);

  // Garbage collection support.
  static int SizeFor(int length) { return kHeaderSize + length * kPointerSize; }

  // Code Generation support.
  static int OffsetOfElementAt(int index) { return SizeFor(index); }

  // Garbage collection support.
  inline Object** RawFieldOfElementAt(int index);

  DECLARE_CAST(FixedArray)

  // Maximal allowed size, in bytes, of a single FixedArray.
  // Prevents overflowing size computations, as well as extreme memory
  // consumption.
  static const int kMaxSize = 128 * MB * kPointerSize;
  // Maximally allowed length of a FixedArray.
  static const int kMaxLength = (kMaxSize - kHeaderSize) / kPointerSize;

  // Dispatched behavior.
  DECLARE_PRINTER(FixedArray)
  DECLARE_VERIFIER(FixedArray)
#ifdef DEBUG
  // Checks if two FixedArrays have identical contents.
  bool IsEqualTo(FixedArray* other);
#endif

  typedef FlexibleBodyDescriptor<kHeaderSize> BodyDescriptor;

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

  DECLARE_CAST(FixedDoubleArray)

  // Maximal allowed size, in bytes, of a single FixedDoubleArray.
  // Prevents overflowing size computations, as well as extreme memory
  // consumption.
  static const int kMaxSize = 512 * MB;
  // Maximally allowed length of a FixedArray.
  static const int kMaxLength = (kMaxSize - kHeaderSize) / kDoubleSize;

  // Dispatched behavior.
  DECLARE_PRINTER(FixedDoubleArray)
  DECLARE_VERIFIER(FixedDoubleArray)

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

  DECLARE_CAST(WeakFixedArray)

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
  inline int Length();
  inline void SetLength(int length);
  inline Object* Get(int index);
  inline Object** Slot(int index);
  inline void Set(int index, Object* obj,
                  WriteBarrierMode mode = UPDATE_WRITE_BARRIER);
  inline void Clear(int index, Object* undefined);
  bool IsFull();
  DECLARE_CAST(ArrayList)

 private:
  static Handle<ArrayList> EnsureSpace(Handle<ArrayList> array, int length);
  static const int kLengthIndex = 0;
  static const int kFirstIndex = 1;
  DISALLOW_IMPLICIT_CONSTRUCTORS(ArrayList);
};

// The property RegExpMatchInfo includes the matchIndices
// array of the last successful regexp match (an array of start/end index
// pairs for the match and all the captured substrings), the invariant is
// that there are at least two capture indices.  The array also contains
// the subject string for the last successful match.
// After creation the result must be treated as a FixedArray in all regards.
class V8_EXPORT_PRIVATE RegExpMatchInfo : NON_EXPORTED_BASE(public FixedArray) {
 public:
  // Returns the number of captures, which is defined as the length of the
  // matchIndices objects of the last match. matchIndices contains two indices
  // for each capture (including the match itself), i.e. 2 * #captures + 2.
  inline int NumberOfCaptureRegisters();
  inline void SetNumberOfCaptureRegisters(int value);

  // Returns the subject string of the last match.
  inline String* LastSubject();
  inline void SetLastSubject(String* value);

  // Like LastSubject, but modifiable by the user.
  inline Object* LastInput();
  inline void SetLastInput(Object* value);

  // Returns the i'th capture index, 0 <= i < NumberOfCaptures(). Capture(0) and
  // Capture(1) determine the start- and endpoint of the match itself.
  inline int Capture(int i);
  inline void SetCapture(int i, int value);

  // Reserves space for captures.
  static Handle<RegExpMatchInfo> ReserveCaptures(
      Handle<RegExpMatchInfo> match_info, int capture_count);

  DECLARE_CAST(RegExpMatchInfo)

  static const int kNumberOfCapturesIndex = 0;
  static const int kLastSubjectIndex = 1;
  static const int kLastInputIndex = 2;
  static const int kFirstCaptureIndex = 3;
  static const int kLastMatchOverhead = kFirstCaptureIndex;

  static const int kNumberOfCapturesOffset = FixedArray::kHeaderSize;
  static const int kLastSubjectOffset = kNumberOfCapturesOffset + kPointerSize;
  static const int kLastInputOffset = kLastSubjectOffset + kPointerSize;
  static const int kFirstCaptureOffset = kLastInputOffset + kPointerSize;

  // Every match info is guaranteed to have enough space to store two captures.
  static const int kInitialCaptureIndices = 2;

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(RegExpMatchInfo);
};

#define FRAME_ARRAY_FIELD_LIST(V) \
  V(WasmInstance, Object)         \
  V(WasmFunctionIndex, Smi)       \
  V(Receiver, Object)             \
  V(Function, JSFunction)         \
  V(Code, AbstractCode)           \
  V(Offset, Smi)                  \
  V(Flags, Smi)

// Container object for data collected during simple stack trace captures.
class FrameArray : public FixedArray {
 public:
#define DECLARE_FRAME_ARRAY_ACCESSORS(name, type) \
  inline type* name(int frame_ix) const;          \
  inline void Set##name(int frame_ix, type* value);
  FRAME_ARRAY_FIELD_LIST(DECLARE_FRAME_ARRAY_ACCESSORS)
#undef DECLARE_FRAME_ARRAY_ACCESSORS

  inline bool IsWasmFrame(int frame_ix) const;
  inline bool IsAsmJsWasmFrame(int frame_ix) const;
  inline int FrameCount() const;

  void ShrinkToFit();

  // Flags.
  static const int kIsWasmFrame = 1 << 0;
  static const int kIsAsmJsWasmFrame = 1 << 1;
  static const int kIsStrict = 1 << 2;
  static const int kForceConstructor = 1 << 3;
  static const int kAsmJsAtNumberConversion = 1 << 4;

  static Handle<FrameArray> AppendJSFrame(Handle<FrameArray> in,
                                          Handle<Object> receiver,
                                          Handle<JSFunction> function,
                                          Handle<AbstractCode> code, int offset,
                                          int flags);
  static Handle<FrameArray> AppendWasmFrame(Handle<FrameArray> in,
                                            Handle<Object> wasm_instance,
                                            int wasm_function_index,
                                            Handle<AbstractCode> code,
                                            int offset, int flags);

  DECLARE_CAST(FrameArray)

 private:
  // The underlying fixed array embodies a captured stack trace. Frame i
  // occupies indices
  //
  // kFirstIndex + 1 + [i * kElementsPerFrame, (i + 1) * kElementsPerFrame[,
  //
  // with internal offsets as below:

  static const int kWasmInstanceOffset = 0;
  static const int kWasmFunctionIndexOffset = 1;

  static const int kReceiverOffset = 0;
  static const int kFunctionOffset = 1;

  static const int kCodeOffset = 2;
  static const int kOffsetOffset = 3;

  static const int kFlagsOffset = 4;

  static const int kElementsPerFrame = 5;

  // Array layout indices.

  static const int kFrameCountIndex = 0;
  static const int kFirstIndex = 1;

  static int LengthFor(int frame_count) {
    return kFirstIndex + frame_count * kElementsPerFrame;
  }

  static Handle<FrameArray> EnsureSpace(Handle<FrameArray> array, int length);

  friend class Factory;
  DISALLOW_IMPLICIT_CONSTRUCTORS(FrameArray);
};

// DescriptorArrays are fixed arrays used to hold instance descriptors.
// The format of the these objects is:
//   [0]: Number of descriptors
//   [1]: Either Smi(0) if uninitialized, or a pointer to small fixed array:
//          [0]: pointer to fixed array with enum cache
//          [1]: either Smi(0) or pointer to fixed array with indices
//   [2]: first key
//   [2 + number of descriptors * kDescriptorSize]: start of slack
class DescriptorArray: public FixedArray {
 public:
  // Returns true for both shared empty_descriptor_array and for smis, which the
  // map uses to encode additional bit fields when the descriptor array is not
  // yet used.
  inline bool IsEmpty();

  // Returns the number of descriptors in the array.
  inline int number_of_descriptors();

  inline int number_of_descriptors_storage();

  inline int NumberOfSlackDescriptors();

  inline void SetNumberOfDescriptors(int number_of_descriptors);
  inline int number_of_entries();

  inline bool HasEnumCache();

  inline void CopyEnumCacheFrom(DescriptorArray* array);

  inline FixedArray* GetEnumCache();

  inline bool HasEnumIndicesCache();

  inline FixedArray* GetEnumIndicesCache();

  inline Object** GetEnumCacheSlot();

  void ClearEnumCache();

  // Initialize or change the enum cache,
  // using the supplied storage for the small "bridge".
  static void SetEnumCache(Handle<DescriptorArray> descriptors,
                           Isolate* isolate, Handle<FixedArray> new_cache,
                           Handle<FixedArray> new_index_cache);

  // Accessors for fetching instance descriptor at descriptor number.
  inline Name* GetKey(int descriptor_number);
  inline Object** GetKeySlot(int descriptor_number);
  inline Object* GetValue(int descriptor_number);
  inline void SetValue(int descriptor_number, Object* value);
  inline Object** GetValueSlot(int descriptor_number);
  static inline int GetValueOffset(int descriptor_number);
  inline Object** GetDescriptorStartSlot(int descriptor_number);
  inline Object** GetDescriptorEndSlot(int descriptor_number);
  inline PropertyDetails GetDetails(int descriptor_number);
  inline int GetFieldIndex(int descriptor_number);
  inline FieldType* GetFieldType(int descriptor_number);

  inline Name* GetSortedKey(int descriptor_number);
  inline int GetSortedKeyIndex(int descriptor_number);
  inline void SetSortedKey(int pointer, int descriptor_number);

  // Accessor for complete descriptor.
  inline void Get(int descriptor_number, Descriptor* desc);
  inline void Set(int descriptor_number, Descriptor* desc);
  inline void Set(int descriptor_number, Name* key, Object* value,
                  PropertyDetails details);
  void Replace(int descriptor_number, Descriptor* descriptor);

  // Generalizes representation and field type of all field descriptors.
  void GeneralizeAllFields();

  // Append automatically sets the enumeration index. This should only be used
  // to add descriptors in bulk at the end, followed by sorting the descriptor
  // array.
  inline void Append(Descriptor* desc);

  static Handle<DescriptorArray> CopyUpTo(Handle<DescriptorArray> desc,
                                          int enumeration_index,
                                          int slack = 0);

  static Handle<DescriptorArray> CopyUpToAddAttributes(
      Handle<DescriptorArray> desc,
      int enumeration_index,
      PropertyAttributes attributes,
      int slack = 0);

  // Sort the instance descriptors by the hash codes of their keys.
  void Sort();

  // Search the instance descriptors for given name.
  INLINE(int Search(Name* name, int number_of_own_descriptors));

  // As the above, but uses DescriptorLookupCache and updates it when
  // necessary.
  INLINE(int SearchWithCache(Isolate* isolate, Name* name, Map* map));

  bool IsEqualUpTo(DescriptorArray* desc, int nof_descriptors);

  // Allocates a DescriptorArray, but returns the singleton
  // empty descriptor array object if number_of_descriptors is 0.
  static Handle<DescriptorArray> Allocate(
      Isolate* isolate, int number_of_descriptors, int slack,
      PretenureFlag pretenure = NOT_TENURED);

  DECLARE_CAST(DescriptorArray)

  // Constant for denoting key was not found.
  static const int kNotFound = -1;

  static const int kDescriptorLengthIndex = 0;
  static const int kEnumCacheIndex = 1;
  static const int kFirstIndex = 2;

  // The length of the "bridge" to the enum cache.
  static const int kEnumCacheBridgeLength = 2;
  static const int kEnumCacheBridgeCacheIndex = 0;
  static const int kEnumCacheBridgeIndicesCacheIndex = 1;

  // Layout description.
  static const int kDescriptorLengthOffset = FixedArray::kHeaderSize;
  static const int kEnumCacheOffset = kDescriptorLengthOffset + kPointerSize;
  static const int kFirstOffset = kEnumCacheOffset + kPointerSize;

  // Layout description for the bridge array.
  static const int kEnumCacheBridgeCacheOffset = FixedArray::kHeaderSize;

  // Layout of descriptor.
  static const int kDescriptorKey = 0;
  static const int kDescriptorDetails = 1;
  static const int kDescriptorValue = 2;
  static const int kDescriptorSize = 3;

#if defined(DEBUG) || defined(OBJECT_PRINT)
  // For our gdb macros, we should perhaps change these in the future.
  void Print();

  // Print all the descriptors.
  void PrintDescriptors(std::ostream& os);  // NOLINT

  void PrintDescriptorDetails(std::ostream& os, int descriptor,
                              PropertyDetails::PrintMode mode);
#endif

#ifdef DEBUG
  // Is the descriptor array sorted and without duplicates?
  bool IsSortedNoDuplicates(int valid_descriptors = -1);

  // Is the descriptor array consistent with the back pointers in targets?
  bool IsConsistentWithBackPointers(Map* current_map);

  // Are two DescriptorArrays equal?
  bool IsEqualTo(DescriptorArray* other);
#endif

  // Returns the fixed array length required to hold number_of_descriptors
  // descriptors.
  static int LengthFor(int number_of_descriptors) {
    return ToKeyIndex(number_of_descriptors);
  }

  static int ToDetailsIndex(int descriptor_number) {
    return kFirstIndex + (descriptor_number * kDescriptorSize) +
           kDescriptorDetails;
  }

  // Conversion from descriptor number to array indices.
  static int ToKeyIndex(int descriptor_number) {
    return kFirstIndex + (descriptor_number * kDescriptorSize) + kDescriptorKey;
  }

  static int ToValueIndex(int descriptor_number) {
    return kFirstIndex + (descriptor_number * kDescriptorSize) +
           kDescriptorValue;
  }

 private:
  // Transfer a complete descriptor from the src descriptor array to this
  // descriptor array.
  void CopyFrom(int index, DescriptorArray* src);

  // Swap first and second descriptor.
  inline void SwapSortedKeys(int first, int second);

  DISALLOW_IMPLICIT_CONSTRUCTORS(DescriptorArray);
};


enum SearchMode { ALL_ENTRIES, VALID_ENTRIES };

template <SearchMode search_mode, typename T>
inline int Search(T* array, Name* name, int valid_entries = 0,
                  int* out_insertion_index = NULL);


// HashTable is a subclass of FixedArray that implements a hash table
// that uses open addressing and quadratic probing.
//
// In order for the quadratic probing to work, elements that have not
// yet been used and elements that have been deleted are
// distinguished.  Probing continues when deleted elements are
// encountered and stops when unused elements are encountered.
//
// - Elements with key == undefined have not been used yet.
// - Elements with key == the_hole have been deleted.
//
// The hash table class is parameterized with a Shape and a Key.
// Shape must be a class with the following interface:
//   class ExampleShape {
//    public:
//      // Tells whether key matches other.
//     static bool IsMatch(Key key, Object* other);
//     // Returns the hash value for key.
//     static uint32_t Hash(Key key);
//     // Returns the hash value for object.
//     static uint32_t HashForObject(Key key, Object* object);
//     // Convert key to an object.
//     static inline Handle<Object> AsHandle(Isolate* isolate, Key key);
//     // The prefix size indicates number of elements in the beginning
//     // of the backing storage.
//     static const int kPrefixSize = ..;
//     // The Element size indicates number of elements per entry.
//     static const int kEntrySize = ..;
//   };
// The prefix size indicates an amount of memory in the
// beginning of the backing storage that can be used for non-element
// information by subclasses.

template<typename Key>
class BaseShape {
 public:
  static const bool UsesSeed = false;
  static uint32_t Hash(Key key) { return 0; }
  static uint32_t SeededHash(Key key, uint32_t seed) {
    DCHECK(UsesSeed);
    return Hash(key);
  }
  static uint32_t HashForObject(Key key, Object* object) { return 0; }
  static uint32_t SeededHashForObject(Key key, uint32_t seed, Object* object) {
    DCHECK(UsesSeed);
    return HashForObject(key, object);
  }
  static inline Map* GetMap(Isolate* isolate);
};


class HashTableBase : public FixedArray {
 public:
  // Returns the number of elements in the hash table.
  inline int NumberOfElements();

  // Returns the number of deleted elements in the hash table.
  inline int NumberOfDeletedElements();

  // Returns the capacity of the hash table.
  inline int Capacity();

  // ElementAdded should be called whenever an element is added to a
  // hash table.
  inline void ElementAdded();

  // ElementRemoved should be called whenever an element is removed from
  // a hash table.
  inline void ElementRemoved();
  inline void ElementsRemoved(int n);

  // Computes the required capacity for a table holding the given
  // number of elements. May be more than HashTable::kMaxCapacity.
  static inline int ComputeCapacity(int at_least_space_for);

  // Tells whether k is a real key.  The hole and undefined are not allowed
  // as keys and can be used to indicate missing or deleted elements.
  inline bool IsKey(Object* k);
  inline bool IsKey(Isolate* isolate, Object* k);

  // Compute the probe offset (quadratic probing).
  INLINE(static uint32_t GetProbeOffset(uint32_t n)) {
    return (n + n * n) >> 1;
  }

  static const int kNumberOfElementsIndex = 0;
  static const int kNumberOfDeletedElementsIndex = 1;
  static const int kCapacityIndex = 2;
  static const int kPrefixStartIndex = 3;

  // Constant used for denoting a absent entry.
  static const int kNotFound = -1;

  // Minimum capacity for newly created hash tables.
  static const int kMinCapacity = 4;

 protected:
  // Update the number of elements in the hash table.
  inline void SetNumberOfElements(int nof);

  // Update the number of deleted elements in the hash table.
  inline void SetNumberOfDeletedElements(int nod);

  // Returns probe entry.
  static uint32_t GetProbe(uint32_t hash, uint32_t number, uint32_t size) {
    DCHECK(base::bits::IsPowerOfTwo32(size));
    return (hash + GetProbeOffset(number)) & (size - 1);
  }

  inline static uint32_t FirstProbe(uint32_t hash, uint32_t size) {
    return hash & (size - 1);
  }

  inline static uint32_t NextProbe(
      uint32_t last, uint32_t number, uint32_t size) {
    return (last + number) & (size - 1);
  }
};


template <typename Derived, typename Shape, typename Key>
class HashTable : public HashTableBase {
 public:
  typedef Shape ShapeT;

  // Wrapper methods
  inline uint32_t Hash(Key key) {
    if (Shape::UsesSeed) {
      return Shape::SeededHash(key, GetHeap()->HashSeed());
    } else {
      return Shape::Hash(key);
    }
  }

  inline uint32_t HashForObject(Key key, Object* object) {
    if (Shape::UsesSeed) {
      return Shape::SeededHashForObject(key, GetHeap()->HashSeed(), object);
    } else {
      return Shape::HashForObject(key, object);
    }
  }

  // Returns a new HashTable object.
  MUST_USE_RESULT static Handle<Derived> New(
      Isolate* isolate, int at_least_space_for,
      MinimumCapacity capacity_option = USE_DEFAULT_MINIMUM_CAPACITY,
      PretenureFlag pretenure = NOT_TENURED);

  DECLARE_CAST(HashTable)

  // Garbage collection support.
  void IteratePrefix(ObjectVisitor* visitor);
  void IterateElements(ObjectVisitor* visitor);

  // Find entry for key otherwise return kNotFound.
  inline int FindEntry(Key key);
  inline int FindEntry(Isolate* isolate, Key key, int32_t hash);
  int FindEntry(Isolate* isolate, Key key);
  inline bool Has(Isolate* isolate, Key key);
  inline bool Has(Key key);

  // Rehashes the table in-place.
  void Rehash(Key key);

  // Returns the key at entry.
  Object* KeyAt(int entry) { return get(EntryToIndex(entry) + kEntryKeyIndex); }

  static const int kElementsStartIndex = kPrefixStartIndex + Shape::kPrefixSize;
  static const int kEntrySize = Shape::kEntrySize;
  STATIC_ASSERT(kEntrySize > 0);
  static const int kEntryKeyIndex = 0;
  static const int kElementsStartOffset =
      kHeaderSize + kElementsStartIndex * kPointerSize;
  // Maximal capacity of HashTable. Based on maximal length of underlying
  // FixedArray. Staying below kMaxCapacity also ensures that EntryToIndex
  // cannot overflow.
  static const int kMaxCapacity =
      (FixedArray::kMaxLength - kElementsStartIndex) / kEntrySize;

  // Returns the index for an entry (of the key)
  static inline int EntryToIndex(int entry) {
    return (entry * kEntrySize) + kElementsStartIndex;
  }

 protected:
  friend class ObjectHashTable;

  MUST_USE_RESULT static Handle<Derived> New(Isolate* isolate, int capacity,
                                             PretenureFlag pretenure);

  // Find the entry at which to insert element with the given key that
  // has the given hash value.
  uint32_t FindInsertionEntry(uint32_t hash);

  // Attempt to shrink hash table after removal of key.
  MUST_USE_RESULT static Handle<Derived> Shrink(Handle<Derived> table, Key key);

  // Ensure enough space for n additional elements.
  MUST_USE_RESULT static Handle<Derived> EnsureCapacity(
      Handle<Derived> table,
      int n,
      Key key,
      PretenureFlag pretenure = NOT_TENURED);

  // Returns true if this table has sufficient capacity for adding n elements.
  bool HasSufficientCapacityToAdd(int number_of_additional_elements);

  // Sets the capacity of the hash table.
  void SetCapacity(int capacity) {
    // To scale a computed hash code to fit within the hash table, we
    // use bit-wise AND with a mask, so the capacity must be positive
    // and non-zero.
    DCHECK(capacity > 0);
    DCHECK(capacity <= kMaxCapacity);
    set(kCapacityIndex, Smi::FromInt(capacity));
  }

 private:
  // Returns _expected_ if one of entries given by the first _probe_ probes is
  // equal to  _expected_. Otherwise, returns the entry given by the probe
  // number _probe_.
  uint32_t EntryForProbe(Key key, Object* k, int probe, uint32_t expected);

  void Swap(uint32_t entry1, uint32_t entry2, WriteBarrierMode mode);

  // Rehashes this hash-table into the new table.
  void Rehash(Handle<Derived> new_table, Key key);
};


// HashTableKey is an abstract superclass for virtual key behavior.
class HashTableKey {
 public:
  // Returns whether the other object matches this key.
  virtual bool IsMatch(Object* other) = 0;
  // Returns the hash value for this key.
  virtual uint32_t Hash() = 0;
  // Returns the hash value for object.
  virtual uint32_t HashForObject(Object* key) = 0;
  // Returns the key object for storing into the hash table.
  MUST_USE_RESULT virtual Handle<Object> AsHandle(Isolate* isolate) = 0;
  // Required.
  virtual ~HashTableKey() {}
};


class StringTableShape : public BaseShape<HashTableKey*> {
 public:
  static inline bool IsMatch(HashTableKey* key, Object* value) {
    return key->IsMatch(value);
  }

  static inline uint32_t Hash(HashTableKey* key) {
    return key->Hash();
  }

  static inline uint32_t HashForObject(HashTableKey* key, Object* object) {
    return key->HashForObject(object);
  }

  static inline Handle<Object> AsHandle(Isolate* isolate, HashTableKey* key);

  static const int kPrefixSize = 0;
  static const int kEntrySize = 1;
};

class SeqOneByteString;

// StringTable.
//
// No special elements in the prefix and the element size is 1
// because only the string itself (the key) needs to be stored.
class StringTable: public HashTable<StringTable,
                                    StringTableShape,
                                    HashTableKey*> {
 public:
  // Find string in the string table. If it is not there yet, it is
  // added. The return value is the string found.
  V8_EXPORT_PRIVATE static Handle<String> LookupString(Isolate* isolate,
                                                       Handle<String> key);
  static Handle<String> LookupKey(Isolate* isolate, HashTableKey* key);
  static String* LookupKeyIfExists(Isolate* isolate, HashTableKey* key);

  // Tries to internalize given string and returns string handle on success
  // or an empty handle otherwise.
  MUST_USE_RESULT static MaybeHandle<String> InternalizeStringIfExists(
      Isolate* isolate,
      Handle<String> string);

  // Looks up a string that is equal to the given string and returns
  // string handle if it is found, or an empty handle otherwise.
  MUST_USE_RESULT static MaybeHandle<String> LookupStringIfExists(
      Isolate* isolate,
      Handle<String> str);
  MUST_USE_RESULT static MaybeHandle<String> LookupTwoCharsStringIfExists(
      Isolate* isolate,
      uint16_t c1,
      uint16_t c2);

  static void EnsureCapacityForDeserialization(Isolate* isolate, int expected);

  DECLARE_CAST(StringTable)

 private:
  template <bool seq_one_byte>
  friend class JsonParser;

  DISALLOW_IMPLICIT_CONSTRUCTORS(StringTable);
};

class StringSetShape : public BaseShape<String*> {
 public:
  static inline bool IsMatch(String* key, Object* value);
  static inline uint32_t Hash(String* key);
  static inline uint32_t HashForObject(String* key, Object* object);

  static const int kPrefixSize = 0;
  static const int kEntrySize = 1;
};

class StringSet : public HashTable<StringSet, StringSetShape, String*> {
 public:
  static Handle<StringSet> New(Isolate* isolate);
  static Handle<StringSet> Add(Handle<StringSet> blacklist,
                               Handle<String> name);
  bool Has(Handle<String> name);

  DECLARE_CAST(StringSet)
};

template <typename Derived, typename Shape, typename Key>
class Dictionary: public HashTable<Derived, Shape, Key> {
  typedef HashTable<Derived, Shape, Key> DerivedHashTable;

 public:
  // Returns the value at entry.
  Object* ValueAt(int entry) {
    return this->get(Derived::EntryToIndex(entry) + 1);
  }

  // Set the value for entry.
  void ValueAtPut(int entry, Object* value) {
    this->set(Derived::EntryToIndex(entry) + 1, value);
  }

  // Returns the property details for the property at entry.
  PropertyDetails DetailsAt(int entry) {
    return Shape::DetailsAt(static_cast<Derived*>(this), entry);
  }

  // Set the details for entry.
  void DetailsAtPut(int entry, PropertyDetails value) {
    Shape::DetailsAtPut(static_cast<Derived*>(this), entry, value);
  }

  // Returns true if property at given entry is deleted.
  bool IsDeleted(int entry) {
    return Shape::IsDeleted(static_cast<Derived*>(this), entry);
  }

  // Delete a property from the dictionary.
  static Handle<Object> DeleteProperty(Handle<Derived> dictionary, int entry);

  // Attempt to shrink the dictionary after deletion of key.
  MUST_USE_RESULT static inline Handle<Derived> Shrink(
      Handle<Derived> dictionary,
      Key key) {
    return DerivedHashTable::Shrink(dictionary, key);
  }

  // Sorting support
  // TODO(dcarney): templatize or move to SeededNumberDictionary
  void CopyValuesTo(FixedArray* elements);

  // Returns the number of elements in the dictionary filtering out properties
  // with the specified attributes.
  int NumberOfElementsFilterAttributes(PropertyFilter filter);

  // Returns the number of enumerable elements in the dictionary.
  int NumberOfEnumElements() {
    return NumberOfElementsFilterAttributes(ENUMERABLE_STRINGS);
  }

  enum SortMode { UNSORTED, SORTED };

  // Return the key indices sorted by its enumeration index.
  static Handle<FixedArray> IterationIndices(
      Handle<Dictionary<Derived, Shape, Key>> dictionary);

  // Collect the keys into the given KeyAccumulator, in ascending chronological
  // order of property creation.
  static void CollectKeysTo(Handle<Dictionary<Derived, Shape, Key>> dictionary,
                            KeyAccumulator* keys);

  // Copies enumerable keys to preallocated fixed array.
  static void CopyEnumKeysTo(Handle<Dictionary<Derived, Shape, Key>> dictionary,
                             Handle<FixedArray> storage, KeyCollectionMode mode,
                             KeyAccumulator* accumulator);

  // Accessors for next enumeration index.
  void SetNextEnumerationIndex(int index) {
    DCHECK(index != 0);
    this->set(kNextEnumerationIndexIndex, Smi::FromInt(index));
  }

  int NextEnumerationIndex() {
    return Smi::cast(this->get(kNextEnumerationIndexIndex))->value();
  }

  // Creates a new dictionary.
  MUST_USE_RESULT static Handle<Derived> New(
      Isolate* isolate, int at_least_space_for,
      PretenureFlag pretenure = NOT_TENURED,
      MinimumCapacity capacity_option = USE_DEFAULT_MINIMUM_CAPACITY);

  // Creates an dictionary with minimal possible capacity.
  MUST_USE_RESULT static Handle<Derived> NewEmpty(
      Isolate* isolate, PretenureFlag pretenure = NOT_TENURED);

  // Ensures that a new dictionary is created when the capacity is checked.
  void SetRequiresCopyOnCapacityChange();

  // Ensure enough space for n additional elements.
  static Handle<Derived> EnsureCapacity(Handle<Derived> obj, int n, Key key);

#ifdef OBJECT_PRINT
  // For our gdb macros, we should perhaps change these in the future.
  void Print();

  void Print(std::ostream& os);  // NOLINT
#endif
  // Returns the key (slow).
  Object* SlowReverseLookup(Object* value);

  // Sets the entry to (key, value) pair.
  inline void SetEntry(int entry,
                       Handle<Object> key,
                       Handle<Object> value);
  inline void SetEntry(int entry,
                       Handle<Object> key,
                       Handle<Object> value,
                       PropertyDetails details);

  MUST_USE_RESULT static Handle<Derived> Add(Handle<Derived> dictionary,
                                             Key key, Handle<Object> value,
                                             PropertyDetails details,
                                             int* entry_out = nullptr);

  static const int kMaxNumberKeyIndex = DerivedHashTable::kPrefixStartIndex;
  static const int kNextEnumerationIndexIndex = kMaxNumberKeyIndex + 1;

  static const bool kIsEnumerable = Shape::kIsEnumerable;

 protected:
  // Generic at put operation.
  MUST_USE_RESULT static Handle<Derived> AtPut(
      Handle<Derived> dictionary,
      Key key,
      Handle<Object> value);
  // Add entry to dictionary. Returns entry value.
  static int AddEntry(Handle<Derived> dictionary, Key key, Handle<Object> value,
                      PropertyDetails details, uint32_t hash);
  // Generate new enumeration indices to avoid enumeration index overflow.
  // Returns iteration indices array for the |dictionary|.
  static Handle<FixedArray> GenerateNewEnumerationIndices(
      Handle<Derived> dictionary);
};


template <typename Derived, typename Shape>
class NameDictionaryBase : public Dictionary<Derived, Shape, Handle<Name> > {
  typedef Dictionary<Derived, Shape, Handle<Name> > DerivedDictionary;

 public:
  // Find entry for key, otherwise return kNotFound. Optimized version of
  // HashTable::FindEntry.
  int FindEntry(Handle<Name> key);
};


template <typename Key>
class BaseDictionaryShape : public BaseShape<Key> {
 public:
  template <typename Dictionary>
  static inline PropertyDetails DetailsAt(Dictionary* dict, int entry) {
    STATIC_ASSERT(Dictionary::kEntrySize == 3);
    DCHECK(entry >= 0);  // Not found is -1, which is not caught by get().
    return PropertyDetails(Smi::cast(dict->get(
        Dictionary::EntryToIndex(entry) + Dictionary::kEntryDetailsIndex)));
  }

  template <typename Dictionary>
  static inline void DetailsAtPut(Dictionary* dict, int entry,
                                  PropertyDetails value) {
    STATIC_ASSERT(Dictionary::kEntrySize == 3);
    dict->set(Dictionary::EntryToIndex(entry) + Dictionary::kEntryDetailsIndex,
              value.AsSmi());
  }

  template <typename Dictionary>
  static bool IsDeleted(Dictionary* dict, int entry) {
    return false;
  }

  template <typename Dictionary>
  static inline void SetEntry(Dictionary* dict, int entry, Handle<Object> key,
                              Handle<Object> value, PropertyDetails details);
};


class NameDictionaryShape : public BaseDictionaryShape<Handle<Name> > {
 public:
  static inline bool IsMatch(Handle<Name> key, Object* other);
  static inline uint32_t Hash(Handle<Name> key);
  static inline uint32_t HashForObject(Handle<Name> key, Object* object);
  static inline Handle<Object> AsHandle(Isolate* isolate, Handle<Name> key);
  static const int kPrefixSize = 2;
  static const int kEntrySize = 3;
  static const int kEntryValueIndex = 1;
  static const int kEntryDetailsIndex = 2;
  static const bool kIsEnumerable = true;
};


class NameDictionary
    : public NameDictionaryBase<NameDictionary, NameDictionaryShape> {
  typedef NameDictionaryBase<NameDictionary, NameDictionaryShape>
      DerivedDictionary;

 public:
  DECLARE_CAST(NameDictionary)

  inline static Handle<FixedArray> DoGenerateNewEnumerationIndices(
      Handle<NameDictionary> dictionary);

  static const int kEntryValueIndex = 1;
  static const int kEntryDetailsIndex = 2;
  static const int kInitialCapacity = 2;
};


class GlobalDictionaryShape : public NameDictionaryShape {
 public:
  static const int kEntrySize = 2;  // Overrides NameDictionaryShape::kEntrySize

  template <typename Dictionary>
  static inline PropertyDetails DetailsAt(Dictionary* dict, int entry);

  template <typename Dictionary>
  static inline void DetailsAtPut(Dictionary* dict, int entry,
                                  PropertyDetails value);

  template <typename Dictionary>
  static bool IsDeleted(Dictionary* dict, int entry);

  template <typename Dictionary>
  static inline void SetEntry(Dictionary* dict, int entry, Handle<Object> key,
                              Handle<Object> value, PropertyDetails details);
};


class GlobalDictionary
    : public NameDictionaryBase<GlobalDictionary, GlobalDictionaryShape> {
 public:
  DECLARE_CAST(GlobalDictionary)

  static const int kEntryValueIndex = 1;
};


class NumberDictionaryShape : public BaseDictionaryShape<uint32_t> {
 public:
  static inline bool IsMatch(uint32_t key, Object* other);
  static inline Handle<Object> AsHandle(Isolate* isolate, uint32_t key);
  static const bool kIsEnumerable = false;
};


class SeededNumberDictionaryShape : public NumberDictionaryShape {
 public:
  static const bool UsesSeed = true;
  static const int kPrefixSize = 2;
  static const int kEntrySize = 3;

  static inline uint32_t SeededHash(uint32_t key, uint32_t seed);
  static inline uint32_t SeededHashForObject(uint32_t key,
                                             uint32_t seed,
                                             Object* object);
};


class UnseededNumberDictionaryShape : public NumberDictionaryShape {
 public:
  static const int kPrefixSize = 0;
  static const int kEntrySize = 2;

  static inline uint32_t Hash(uint32_t key);
  static inline uint32_t HashForObject(uint32_t key, Object* object);

  template <typename Dictionary>
  static inline PropertyDetails DetailsAt(Dictionary* dict, int entry) {
    UNREACHABLE();
    return PropertyDetails::Empty();
  }

  template <typename Dictionary>
  static inline void DetailsAtPut(Dictionary* dict, int entry,
                                  PropertyDetails value) {
    UNREACHABLE();
  }

  static inline Map* GetMap(Isolate* isolate);
};


class SeededNumberDictionary
    : public Dictionary<SeededNumberDictionary,
                        SeededNumberDictionaryShape,
                        uint32_t> {
 public:
  DECLARE_CAST(SeededNumberDictionary)

  // Type specific at put (default NONE attributes is used when adding).
  MUST_USE_RESULT static Handle<SeededNumberDictionary> AtNumberPut(
      Handle<SeededNumberDictionary> dictionary, uint32_t key,
      Handle<Object> value, Handle<JSObject> dictionary_holder);
  MUST_USE_RESULT static Handle<SeededNumberDictionary> AddNumberEntry(
      Handle<SeededNumberDictionary> dictionary, uint32_t key,
      Handle<Object> value, PropertyDetails details,
      Handle<JSObject> dictionary_holder);

  // Set an existing entry or add a new one if needed.
  // Return the updated dictionary.
  MUST_USE_RESULT static Handle<SeededNumberDictionary> Set(
      Handle<SeededNumberDictionary> dictionary, uint32_t key,
      Handle<Object> value, PropertyDetails details,
      Handle<JSObject> dictionary_holder);

  void UpdateMaxNumberKey(uint32_t key, Handle<JSObject> dictionary_holder);

  // Returns true if the dictionary contains any elements that are non-writable,
  // non-configurable, non-enumerable, or have getters/setters.
  bool HasComplexElements();

  // If slow elements are required we will never go back to fast-case
  // for the elements kept in this dictionary.  We require slow
  // elements if an element has been added at an index larger than
  // kRequiresSlowElementsLimit or set_requires_slow_elements() has been called
  // when defining a getter or setter with a number key.
  inline bool requires_slow_elements();
  inline void set_requires_slow_elements();

  // Get the value of the max number key that has been added to this
  // dictionary.  max_number_key can only be called if
  // requires_slow_elements returns false.
  inline uint32_t max_number_key();

  static const int kEntryValueIndex = 1;
  static const int kEntryDetailsIndex = 2;

  // Bit masks.
  static const int kRequiresSlowElementsMask = 1;
  static const int kRequiresSlowElementsTagSize = 1;
  static const uint32_t kRequiresSlowElementsLimit = (1 << 29) - 1;
};


class UnseededNumberDictionary
    : public Dictionary<UnseededNumberDictionary,
                        UnseededNumberDictionaryShape,
                        uint32_t> {
 public:
  DECLARE_CAST(UnseededNumberDictionary)

  // Type specific at put (default NONE attributes is used when adding).
  MUST_USE_RESULT static Handle<UnseededNumberDictionary> AtNumberPut(
      Handle<UnseededNumberDictionary> dictionary,
      uint32_t key,
      Handle<Object> value);
  MUST_USE_RESULT static Handle<UnseededNumberDictionary> AddNumberEntry(
      Handle<UnseededNumberDictionary> dictionary,
      uint32_t key,
      Handle<Object> value);
  static Handle<UnseededNumberDictionary> DeleteKey(
      Handle<UnseededNumberDictionary> dictionary, uint32_t key);

  // Set an existing entry or add a new one if needed.
  // Return the updated dictionary.
  MUST_USE_RESULT static Handle<UnseededNumberDictionary> Set(
      Handle<UnseededNumberDictionary> dictionary,
      uint32_t key,
      Handle<Object> value);

  static const int kEntryValueIndex = 1;
  static const int kEntryDetailsIndex = 2;
};


class ObjectHashTableShape : public BaseShape<Handle<Object> > {
 public:
  static inline bool IsMatch(Handle<Object> key, Object* other);
  static inline uint32_t Hash(Handle<Object> key);
  static inline uint32_t HashForObject(Handle<Object> key, Object* object);
  static inline Handle<Object> AsHandle(Isolate* isolate, Handle<Object> key);
  static const int kPrefixSize = 0;
  static const int kEntrySize = 2;
};


// ObjectHashTable maps keys that are arbitrary objects to object values by
// using the identity hash of the key for hashing purposes.
class ObjectHashTable: public HashTable<ObjectHashTable,
                                        ObjectHashTableShape,
                                        Handle<Object> > {
  typedef HashTable<
      ObjectHashTable, ObjectHashTableShape, Handle<Object> > DerivedHashTable;
 public:
  DECLARE_CAST(ObjectHashTable)

  // Attempt to shrink hash table after removal of key.
  MUST_USE_RESULT static inline Handle<ObjectHashTable> Shrink(
      Handle<ObjectHashTable> table,
      Handle<Object> key);

  // Looks up the value associated with the given key. The hole value is
  // returned in case the key is not present.
  Object* Lookup(Handle<Object> key);
  Object* Lookup(Handle<Object> key, int32_t hash);
  Object* Lookup(Isolate* isolate, Handle<Object> key, int32_t hash);

  // Returns the value at entry.
  Object* ValueAt(int entry);

  // Adds (or overwrites) the value associated with the given key.
  static Handle<ObjectHashTable> Put(Handle<ObjectHashTable> table,
                                     Handle<Object> key,
                                     Handle<Object> value);
  static Handle<ObjectHashTable> Put(Handle<ObjectHashTable> table,
                                     Handle<Object> key, Handle<Object> value,
                                     int32_t hash);

  // Returns an ObjectHashTable (possibly |table|) where |key| has been removed.
  static Handle<ObjectHashTable> Remove(Handle<ObjectHashTable> table,
                                        Handle<Object> key,
                                        bool* was_present);
  static Handle<ObjectHashTable> Remove(Handle<ObjectHashTable> table,
                                        Handle<Object> key, bool* was_present,
                                        int32_t hash);

 protected:
  friend class MarkCompactCollector;

  void AddEntry(int entry, Object* key, Object* value);
  void RemoveEntry(int entry);

  // Returns the index to the value of an entry.
  static inline int EntryToValueIndex(int entry) {
    return EntryToIndex(entry) + 1;
  }
};

class ObjectHashSetShape : public ObjectHashTableShape {
 public:
  static const int kPrefixSize = 0;
  static const int kEntrySize = 1;
};

class ObjectHashSet
    : public HashTable<ObjectHashSet, ObjectHashSetShape, Handle<Object>> {
 public:
  static Handle<ObjectHashSet> Add(Handle<ObjectHashSet> set,
                                   Handle<Object> key);

  inline bool Has(Isolate* isolate, Handle<Object> key, int32_t hash);
  inline bool Has(Isolate* isolate, Handle<Object> key);

  DECLARE_CAST(ObjectHashSet)
};

// OrderedHashTable is a HashTable with Object keys that preserves
// insertion order. There are Map and Set interfaces (OrderedHashMap
// and OrderedHashTable, below). It is meant to be used by JSMap/JSSet.
//
// Only Object* keys are supported, with Object::SameValueZero() used as the
// equality operator and Object::GetHash() for the hash function.
//
// Based on the "Deterministic Hash Table" as described by Jason Orendorff at
// https://wiki.mozilla.org/User:Jorend/Deterministic_hash_tables
// Originally attributed to Tyler Close.
//
// Memory layout:
//   [0]: bucket count
//   [1]: element count
//   [2]: deleted element count
//   [3..(3 + NumberOfBuckets() - 1)]: "hash table", where each item is an
//                            offset into the data table (see below) where the
//                            first item in this bucket is stored.
//   [3 + NumberOfBuckets()..length]: "data table", an array of length
//                            Capacity() * kEntrySize, where the first entrysize
//                            items are handled by the derived class and the
//                            item at kChainOffset is another entry into the
//                            data table indicating the next entry in this hash
//                            bucket.
//
// When we transition the table to a new version we obsolete it and reuse parts
// of the memory to store information how to transition an iterator to the new
// table:
//
// Memory layout for obsolete table:
//   [0]: bucket count
//   [1]: Next newer table
//   [2]: Number of removed holes or -1 when the table was cleared.
//   [3..(3 + NumberOfRemovedHoles() - 1)]: The indexes of the removed holes.
//   [3 + NumberOfRemovedHoles()..length]: Not used
//
template<class Derived, class Iterator, int entrysize>
class OrderedHashTable: public FixedArray {
 public:
  // Returns an OrderedHashTable with a capacity of at least |capacity|.
  static Handle<Derived> Allocate(
      Isolate* isolate, int capacity, PretenureFlag pretenure = NOT_TENURED);

  // Returns an OrderedHashTable (possibly |table|) with enough space
  // to add at least one new element.
  static Handle<Derived> EnsureGrowable(Handle<Derived> table);

  // Returns an OrderedHashTable (possibly |table|) that's shrunken
  // if possible.
  static Handle<Derived> Shrink(Handle<Derived> table);

  // Returns a new empty OrderedHashTable and records the clearing so that
  // existing iterators can be updated.
  static Handle<Derived> Clear(Handle<Derived> table);

  // Returns a true if the OrderedHashTable contains the key
  static bool HasKey(Handle<Derived> table, Handle<Object> key);

  int NumberOfElements() {
    return Smi::cast(get(kNumberOfElementsIndex))->value();
  }

  int NumberOfDeletedElements() {
    return Smi::cast(get(kNumberOfDeletedElementsIndex))->value();
  }

  // Returns the number of contiguous entries in the data table, starting at 0,
  // that either are real entries or have been deleted.
  int UsedCapacity() { return NumberOfElements() + NumberOfDeletedElements(); }

  int NumberOfBuckets() {
    return Smi::cast(get(kNumberOfBucketsIndex))->value();
  }

  // Returns an index into |this| for the given entry.
  int EntryToIndex(int entry) {
    return kHashTableStartIndex + NumberOfBuckets() + (entry * kEntrySize);
  }

  int HashToBucket(int hash) { return hash & (NumberOfBuckets() - 1); }

  int HashToEntry(int hash) {
    int bucket = HashToBucket(hash);
    Object* entry = this->get(kHashTableStartIndex + bucket);
    return Smi::cast(entry)->value();
  }

  int KeyToFirstEntry(Isolate* isolate, Object* key) {
    Object* hash = key->GetHash();
    // If the object does not have an identity hash, it was never used as a key
    if (hash->IsUndefined(isolate)) return kNotFound;
    return HashToEntry(Smi::cast(hash)->value());
  }

  int NextChainEntry(int entry) {
    Object* next_entry = get(EntryToIndex(entry) + kChainOffset);
    return Smi::cast(next_entry)->value();
  }

  // use KeyAt(i)->IsTheHole(isolate) to determine if this is a deleted entry.
  Object* KeyAt(int entry) {
    DCHECK_LT(entry, this->UsedCapacity());
    return get(EntryToIndex(entry));
  }

  bool IsObsolete() {
    return !get(kNextTableIndex)->IsSmi();
  }

  // The next newer table. This is only valid if the table is obsolete.
  Derived* NextTable() {
    return Derived::cast(get(kNextTableIndex));
  }

  // When the table is obsolete we store the indexes of the removed holes.
  int RemovedIndexAt(int index) {
    return Smi::cast(get(kRemovedHolesIndex + index))->value();
  }

  static const int kNotFound = -1;
  static const int kMinCapacity = 4;

  static const int kNumberOfBucketsIndex = 0;
  static const int kNumberOfElementsIndex = kNumberOfBucketsIndex + 1;
  static const int kNumberOfDeletedElementsIndex = kNumberOfElementsIndex + 1;
  static const int kHashTableStartIndex = kNumberOfDeletedElementsIndex + 1;
  static const int kNextTableIndex = kNumberOfElementsIndex;

  static const int kNumberOfBucketsOffset =
      kHeaderSize + kNumberOfBucketsIndex * kPointerSize;
  static const int kNumberOfElementsOffset =
      kHeaderSize + kNumberOfElementsIndex * kPointerSize;
  static const int kNumberOfDeletedElementsOffset =
      kHeaderSize + kNumberOfDeletedElementsIndex * kPointerSize;
  static const int kHashTableStartOffset =
      kHeaderSize + kHashTableStartIndex * kPointerSize;
  static const int kNextTableOffset =
      kHeaderSize + kNextTableIndex * kPointerSize;

  static const int kEntrySize = entrysize + 1;
  static const int kChainOffset = entrysize;

  static const int kLoadFactor = 2;

  // NumberOfDeletedElements is set to kClearedTableSentinel when
  // the table is cleared, which allows iterator transitions to
  // optimize that case.
  static const int kClearedTableSentinel = -1;

 protected:
  static Handle<Derived> Rehash(Handle<Derived> table, int new_capacity);

  void SetNumberOfBuckets(int num) {
    set(kNumberOfBucketsIndex, Smi::FromInt(num));
  }

  void SetNumberOfElements(int num) {
    set(kNumberOfElementsIndex, Smi::FromInt(num));
  }

  void SetNumberOfDeletedElements(int num) {
    set(kNumberOfDeletedElementsIndex, Smi::FromInt(num));
  }

  // Returns the number elements that can fit into the allocated buffer.
  int Capacity() {
    return NumberOfBuckets() * kLoadFactor;
  }

  void SetNextTable(Derived* next_table) {
    set(kNextTableIndex, next_table);
  }

  void SetRemovedIndexAt(int index, int removed_index) {
    return set(kRemovedHolesIndex + index, Smi::FromInt(removed_index));
  }

  static const int kRemovedHolesIndex = kHashTableStartIndex;

  static const int kMaxCapacity =
      (FixedArray::kMaxLength - kHashTableStartIndex)
      / (1 + (kEntrySize * kLoadFactor));
};


class JSSetIterator;


class OrderedHashSet: public OrderedHashTable<
    OrderedHashSet, JSSetIterator, 1> {
 public:
  DECLARE_CAST(OrderedHashSet)

  static Handle<OrderedHashSet> Add(Handle<OrderedHashSet> table,
                                    Handle<Object> value);
  static Handle<FixedArray> ConvertToKeysArray(Handle<OrderedHashSet> table,
                                               GetKeysConversion convert);
};


class JSMapIterator;


class OrderedHashMap
    : public OrderedHashTable<OrderedHashMap, JSMapIterator, 2> {
 public:
  DECLARE_CAST(OrderedHashMap)

  inline Object* ValueAt(int entry);

  static const int kValueOffset = 1;
};


template <int entrysize>
class WeakHashTableShape : public BaseShape<Handle<Object> > {
 public:
  static inline bool IsMatch(Handle<Object> key, Object* other);
  static inline uint32_t Hash(Handle<Object> key);
  static inline uint32_t HashForObject(Handle<Object> key, Object* object);
  static inline Handle<Object> AsHandle(Isolate* isolate, Handle<Object> key);
  static const int kPrefixSize = 0;
  static const int kEntrySize = entrysize;
};


// WeakHashTable maps keys that are arbitrary heap objects to heap object
// values. The table wraps the keys in weak cells and store values directly.
// Thus it references keys weakly and values strongly.
class WeakHashTable: public HashTable<WeakHashTable,
                                      WeakHashTableShape<2>,
                                      Handle<Object> > {
  typedef HashTable<
      WeakHashTable, WeakHashTableShape<2>, Handle<Object> > DerivedHashTable;
 public:
  DECLARE_CAST(WeakHashTable)

  // Looks up the value associated with the given key. The hole value is
  // returned in case the key is not present.
  Object* Lookup(Handle<HeapObject> key);

  // Adds (or overwrites) the value associated with the given key. Mapping a
  // key to the hole value causes removal of the whole entry.
  MUST_USE_RESULT static Handle<WeakHashTable> Put(Handle<WeakHashTable> table,
                                                   Handle<HeapObject> key,
                                                   Handle<HeapObject> value);

  static Handle<FixedArray> GetValues(Handle<WeakHashTable> table);

 private:
  friend class MarkCompactCollector;

  void AddEntry(int entry, Handle<WeakCell> key, Handle<HeapObject> value);

  // Returns the index to the value of an entry.
  static inline int EntryToValueIndex(int entry) {
    return EntryToIndex(entry) + 1;
  }
};


// The cache for maps used by normalized (dictionary mode) objects.
// Such maps do not have property descriptors, so a typical program
// needs very limited number of distinct normalized maps.
class NormalizedMapCache: public FixedArray {
 public:
  static Handle<NormalizedMapCache> New(Isolate* isolate);

  MUST_USE_RESULT MaybeHandle<Map> Get(Handle<Map> fast_map,
                                       PropertyNormalizationMode mode);
  void Set(Handle<Map> fast_map, Handle<Map> normalized_map);

  void Clear();

  DECLARE_CAST(NormalizedMapCache)

  static inline bool IsNormalizedMapCache(const HeapObject* obj);

  DECLARE_VERIFIER(NormalizedMapCache)
 private:
  static const int kEntries = 64;

  static inline int GetIndex(Handle<Map> map);

  // The following declarations hide base class methods.
  Object* get(int index);
  void set(int index, Object* value);
};

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

  DECLARE_CAST(HandlerTable)

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
  inline byte get(int index);
  inline void set(int index, byte value);

  // Copy in / copy out whole byte slices.
  inline void copy_out(int index, byte* buffer, int length);
  inline void copy_in(int index, const byte* buffer, int length);

  // Treat contents as an int array.
  inline int get_int(int index);
  inline void set_int(int index, int value);

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

  // Returns a pointer to the ByteArray object for a given data start address.
  static inline ByteArray* FromDataStartAddress(Address address);

  DECLARE_CAST(ByteArray)

  // Dispatched behavior.
  inline int ByteArraySize();
  DECLARE_PRINTER(ByteArray)
  DECLARE_VERIFIER(ByteArray)

  // Layout description.
  static const int kAlignedSize = OBJECT_POINTER_ALIGN(kHeaderSize);

  // Maximal memory consumption for a single ByteArray.
  static const int kMaxSize = 512 * MB;
  // Maximal length of a single ByteArray.
  static const int kMaxLength = kMaxSize - kHeaderSize;

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
  DECLARE_CAST(PodArray<T>)

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(PodArray<T>);
};

// BytecodeArray represents a sequence of interpreter bytecodes.
class BytecodeArray : public FixedArrayBase {
 public:
#define DECLARE_BYTECODE_AGE_ENUM(X) k##X##BytecodeAge,
  enum Age {
    kNoAgeBytecodeAge = 0,
    CODE_AGE_LIST(DECLARE_BYTECODE_AGE_ENUM) kAfterLastBytecodeAge,
    kFirstBytecodeAge = kNoAgeBytecodeAge,
    kLastBytecodeAge = kAfterLastBytecodeAge - 1,
    kBytecodeAgeCount = kAfterLastBytecodeAge - kFirstBytecodeAge - 1,
    kIsOldBytecodeAge = kSexagenarianBytecodeAge
  };
#undef DECLARE_BYTECODE_AGE_ENUM

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
  // offset and source position.
  DECL_ACCESSORS(source_position_table, ByteArray)

  DECLARE_CAST(BytecodeArray)

  // Dispatched behavior.
  inline int BytecodeArraySize();

  inline int instruction_size();

  // Returns the size of bytecode and its metadata. This includes the size of
  // bytecode, constant pool, source position table, and handler table.
  inline int SizeIncludingMetadata();

  int SourcePosition(int offset);
  int SourceStatementPosition(int offset);

  DECLARE_PRINTER(BytecodeArray)
  DECLARE_VERIFIER(BytecodeArray)

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

  typedef FixedBodyDescriptor<kPointerFieldsBeginOffset,
                              kPointerFieldsEndOffset, kHeaderSize>
      MarkingBodyDescriptor;

  class BodyDescriptor;

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

  inline int nobarrier_size() const;
  inline void nobarrier_set_size(int value);

  inline int Size();

  // Accessors for the next field.
  inline FreeSpace* next();
  inline void set_next(FreeSpace* next);

  inline static FreeSpace* cast(HeapObject* obj);

  // Dispatched behavior.
  DECLARE_PRINTER(FreeSpace)
  DECLARE_VERIFIER(FreeSpace)

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
  DECLARE_CAST(FixedTypedArrayBase)

  static const int kBasePointerOffset = FixedArrayBase::kHeaderSize;
  static const int kExternalPointerOffset = kBasePointerOffset + kPointerSize;
  static const int kHeaderSize =
      DOUBLE_POINTER_ALIGN(kExternalPointerOffset + kPointerSize);

  static const int kDataOffset = kHeaderSize;

  class BodyDescriptor;

  inline int size();

  static inline int TypedArraySize(InstanceType type, int length);
  inline int TypedArraySize(InstanceType type);

  // Use with care: returns raw pointer into heap.
  inline void* DataPtr();

  inline int DataSize();

 private:
  static inline int ElementSize(InstanceType type);

  inline int DataSize(InstanceType type);

  DISALLOW_IMPLICIT_CONSTRUCTORS(FixedTypedArrayBase);
};


template <class Traits>
class FixedTypedArray: public FixedTypedArrayBase {
 public:
  typedef typename Traits::ElementType ElementType;
  static const InstanceType kInstanceType = Traits::kInstanceType;

  DECLARE_CAST(FixedTypedArray<Traits>)

  inline ElementType get_scalar(int index);
  static inline Handle<Object> get(FixedTypedArray* array, int index);
  inline void set(int index, ElementType value);

  static inline ElementType from_int(int value);
  static inline ElementType from_double(double value);

  // This accessor applies the correct conversion from Smi, HeapNumber
  // and undefined.
  inline void SetValue(uint32_t index, Object* value);

  DECLARE_PRINTER(FixedTypedArray)
  DECLARE_VERIFIER(FixedTypedArray)

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(FixedTypedArray);
};

#define FIXED_TYPED_ARRAY_TRAITS(Type, type, TYPE, elementType, size)         \
  class Type##ArrayTraits {                                                   \
   public:   /* NOLINT */                                                     \
    typedef elementType ElementType;                                          \
    static const InstanceType kInstanceType = FIXED_##TYPE##_ARRAY_TYPE;      \
    static const char* Designator() { return #type " array"; }                \
    static inline Handle<Object> ToHandle(Isolate* isolate,                   \
                                          elementType scalar);                \
    static inline elementType defaultValue();                                 \
  };                                                                          \
                                                                              \
  typedef FixedTypedArray<Type##ArrayTraits> Fixed##Type##Array;

TYPED_ARRAYS(FIXED_TYPED_ARRAY_TRAITS)

#undef FIXED_TYPED_ARRAY_TRAITS

// DeoptimizationInputData is a fixed array used to hold the deoptimization
// data for code generated by the Hydrogen/Lithium compiler.  It also
// contains information about functions that were inlined.  If N different
// functions were inlined then first N elements of the literal array will
// contain these functions.
//
// It can be empty.
class DeoptimizationInputData: public FixedArray {
 public:
  // Layout description.  Indices in the array.
  static const int kTranslationByteArrayIndex = 0;
  static const int kInlinedFunctionCountIndex = 1;
  static const int kLiteralArrayIndex = 2;
  static const int kOsrAstIdIndex = 3;
  static const int kOsrPcOffsetIndex = 4;
  static const int kOptimizationIdIndex = 5;
  static const int kSharedFunctionInfoIndex = 6;
  static const int kWeakCellCacheIndex = 7;
  static const int kInliningPositionsIndex = 8;
  static const int kFirstDeoptEntryIndex = 9;

  // Offsets of deopt entry elements relative to the start of the entry.
  static const int kAstIdRawOffset = 0;
  static const int kTranslationIndexOffset = 1;
  static const int kArgumentsStackHeightOffset = 2;
  static const int kPcOffset = 3;
  static const int kDeoptEntrySize = 4;

  // Simple element accessors.
#define DECLARE_ELEMENT_ACCESSORS(name, type) \
  inline type* name();                        \
  inline void Set##name(type* value);

  DECLARE_ELEMENT_ACCESSORS(TranslationByteArray, ByteArray)
  DECLARE_ELEMENT_ACCESSORS(InlinedFunctionCount, Smi)
  DECLARE_ELEMENT_ACCESSORS(LiteralArray, FixedArray)
  DECLARE_ELEMENT_ACCESSORS(OsrAstId, Smi)
  DECLARE_ELEMENT_ACCESSORS(OsrPcOffset, Smi)
  DECLARE_ELEMENT_ACCESSORS(OptimizationId, Smi)
  DECLARE_ELEMENT_ACCESSORS(SharedFunctionInfo, Object)
  DECLARE_ELEMENT_ACCESSORS(WeakCellCache, Object)
  DECLARE_ELEMENT_ACCESSORS(InliningPositions, PodArray<InliningPosition>)

#undef DECLARE_ELEMENT_ACCESSORS

  // Accessors for elements of the ith deoptimization entry.
#define DECLARE_ENTRY_ACCESSORS(name, type) \
  inline type* name(int i);                 \
  inline void Set##name(int i, type* value);

  DECLARE_ENTRY_ACCESSORS(AstIdRaw, Smi)
  DECLARE_ENTRY_ACCESSORS(TranslationIndex, Smi)
  DECLARE_ENTRY_ACCESSORS(ArgumentsStackHeight, Smi)
  DECLARE_ENTRY_ACCESSORS(Pc, Smi)

#undef DECLARE_ENTRY_ACCESSORS

  inline BailoutId AstId(int i);

  inline void SetAstId(int i, BailoutId value);

  inline int DeoptCount();

  static const int kNotInlinedIndex = -1;

  // Returns the inlined function at the given position in LiteralArray, or the
  // outer function if index == kNotInlinedIndex.
  class SharedFunctionInfo* GetInlinedFunction(int index);

  // Allocates a DeoptimizationInputData.
  static Handle<DeoptimizationInputData> New(Isolate* isolate,
                                             int deopt_entry_count,
                                             PretenureFlag pretenure);

  DECLARE_CAST(DeoptimizationInputData)

#ifdef ENABLE_DISASSEMBLER
  void DeoptimizationInputDataPrint(std::ostream& os);  // NOLINT
#endif

 private:
  static int IndexForEntry(int i) {
    return kFirstDeoptEntryIndex + (i * kDeoptEntrySize);
  }


  static int LengthFor(int entry_count) { return IndexForEntry(entry_count); }
};

// DeoptimizationOutputData is a fixed array used to hold the deoptimization
// data for code generated by the full compiler.
// The format of the these objects is
//   [i * 2]: Ast ID for ith deoptimization.
//   [i * 2 + 1]: PC and state of ith deoptimization
class DeoptimizationOutputData: public FixedArray {
 public:
  inline int DeoptPoints();

  inline BailoutId AstId(int index);

  inline void SetAstId(int index, BailoutId id);

  inline Smi* PcAndState(int index);
  inline void SetPcAndState(int index, Smi* offset);

  static int LengthOfFixedArray(int deopt_points) {
    return deopt_points * 2;
  }

  // Allocates a DeoptimizationOutputData.
  static Handle<DeoptimizationOutputData> New(Isolate* isolate,
                                              int number_of_deopt_points,
                                              PretenureFlag pretenure);

  DECLARE_CAST(DeoptimizationOutputData)

#ifdef ENABLE_DISASSEMBLER
  void DeoptimizationOutputDataPrint(std::ostream& os);  // NOLINT
#endif
};


// A literals array contains the literals for a JSFunction. It also holds
// the type feedback vector.
class LiteralsArray : public FixedArray {
 public:
  static const int kVectorIndex = 0;
  static const int kFirstLiteralIndex = 1;
  V8_EXPORT_PRIVATE static const int kFeedbackVectorOffset;
  static const int kOffsetToFirstLiteral;

  static int OffsetOfLiteralAt(int index) {
    return OffsetOfElementAt(index + kFirstLiteralIndex);
  }

  inline FeedbackVector* feedback_vector() const;
  inline void set_feedback_vector(FeedbackVector* vector);
  inline Object* literal(int literal_index) const;
  inline void set_literal(int literal_index, Object* literal);
  inline void set_literal_undefined(int literal_index);
  inline int literals_count() const;

  static Handle<LiteralsArray> New(Isolate* isolate,
                                   Handle<FeedbackVector> vector,
                                   int number_of_literals,
                                   PretenureFlag pretenure = TENURED);

  DECLARE_CAST(LiteralsArray)

 private:
  inline Object* get(int index) const;
  inline void set(int index, Object* value);
  inline void set(int index, Smi* value);
  inline void set(int index, Object* value, WriteBarrierMode mode);
};


class TemplateList : public FixedArray {
 public:
  static Handle<TemplateList> New(Isolate* isolate, int size);
  inline int length() const;
  inline Object* get(int index) const;
  inline void set(int index, Object* value);
  static Handle<TemplateList> Add(Isolate* isolate, Handle<TemplateList> list,
                                  Handle<Object> value);
  DECLARE_CAST(TemplateList)
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
  V(CALL_IC)            \
  V(STORE_IC)           \
  V(KEYED_STORE_IC)     \
  V(BINARY_OP_IC)       \
  V(COMPARE_IC)         \
  V(TO_BOOLEAN_IC)

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

  // [source_position_table]: ByteArray for the source positions table.
  DECL_ACCESSORS(source_position_table, ByteArray)

  // [protected_instructions]: Fixed array containing protected instruction and
  // corresponding landing pad offsets.
  DECL_ACCESSORS(protected_instructions, FixedArray)

  // [raw_type_feedback_info]: This field stores various things, depending on
  // the kind of the code object.
  //   FUNCTION           => type feedback information.
  //   STUB and ICs       => major/minor key as Smi.
  DECL_ACCESSORS(raw_type_feedback_info, Object)
  inline Object* type_feedback_info();
  inline void set_type_feedback_info(
      Object* value, WriteBarrierMode mode = UPDATE_WRITE_BARRIER);
  inline uint32_t stub_key();
  inline void set_stub_key(uint32_t key);

  // [next_code_link]: Link for lists of optimized or deoptimized code.
  // Note that storage for this field is overlapped with typefeedback_info.
  DECL_ACCESSORS(next_code_link, Object)

  // [gc_metadata]: Field used to hold GC related metadata. The contents of this
  // field does not have to be traced during garbage collection since
  // it is only used by the garbage collector itself.
  DECL_ACCESSORS(gc_metadata, Object)

  // [ic_age]: Inline caching age: the value of the Heap::global_ic_age
  // at the moment when this object was created.
  inline void set_ic_age(int count);
  inline int ic_age() const;

  // [prologue_offset]: Offset of the function prologue, used for aging
  // FUNCTIONs and OPTIMIZED_FUNCTIONs.
  inline int prologue_offset() const;
  inline void set_prologue_offset(int offset);

  // [constant_pool offset]: Offset of the constant pool.
  // Valid for FLAG_enable_embedded_constant_pool only
  inline int constant_pool_offset() const;
  inline void set_constant_pool_offset(int offset);

  // Unchecked accessors to be used during GC.
  inline ByteArray* unchecked_relocation_info();

  inline int relocation_size();

  // [flags]: Various code flags.
  inline Flags flags();
  inline void set_flags(Flags flags);

  // [flags]: Access to specific code flags.
  inline Kind kind();
  inline ExtraICState extra_ic_state();  // Only valid for IC stubs.

  // Testers for IC stub kinds.
  inline bool is_inline_cache_stub();
  inline bool is_debug_stub();
  inline bool is_handler();
  inline bool is_call_stub();
  inline bool is_binary_op_stub();
  inline bool is_compare_ic_stub();
  inline bool is_to_boolean_ic_stub();
  inline bool is_optimized_code();
  inline bool is_wasm_code();

  inline bool IsCodeStubOrIC();

  inline void set_raw_kind_specific_flags1(int value);
  inline void set_raw_kind_specific_flags2(int value);

  // Testers for interpreter builtins.
  inline bool is_interpreter_trampoline_builtin();

  // [is_crankshafted]: For kind STUB or ICs, tells whether or not a code
  // object was generated by either the hydrogen or the TurboFan optimizing
  // compiler (but it may not be an optimized function).
  inline bool is_crankshafted();
  inline bool is_hydrogen_stub();  // Crankshafted, but not a function.
  inline void set_is_crankshafted(bool value);

  // [is_turbofanned]: For kind STUB or OPTIMIZED_FUNCTION, tells whether the
  // code object was generated by the TurboFan optimizing compiler.
  inline bool is_turbofanned();
  inline void set_is_turbofanned(bool value);

  // [can_have_weak_objects]: For kind OPTIMIZED_FUNCTION, tells whether the
  // embedded objects in code should be treated weakly.
  inline bool can_have_weak_objects();
  inline void set_can_have_weak_objects(bool value);

  // [is_construct_stub]: For kind BUILTIN, tells whether the code object
  // represents a hand-written construct stub
  // (e.g., NumberConstructor_ConstructStub).
  inline bool is_construct_stub();
  inline void set_is_construct_stub(bool value);

  // [has_deoptimization_support]: For FUNCTION kind, tells if it has
  // deoptimization support.
  inline bool has_deoptimization_support();
  inline void set_has_deoptimization_support(bool value);

  // [has_debug_break_slots]: For FUNCTION kind, tells if it has
  // been compiled with debug break slots.
  inline bool has_debug_break_slots();
  inline void set_has_debug_break_slots(bool value);

  // [has_reloc_info_for_serialization]: For FUNCTION kind, tells if its
  // reloc info includes runtime and external references to support
  // serialization/deserialization.
  inline bool has_reloc_info_for_serialization();
  inline void set_has_reloc_info_for_serialization(bool value);

  // [allow_osr_at_loop_nesting_level]: For FUNCTION kind, tells for
  // how long the function has been marked for OSR and therefore which
  // level of loop nesting we are willing to do on-stack replacement
  // for.
  inline void set_allow_osr_at_loop_nesting_level(int level);
  inline int allow_osr_at_loop_nesting_level();

  // [profiler_ticks]: For FUNCTION kind, tells for how many profiler ticks
  // the code object was seen on the stack with no IC patching going on.
  inline int profiler_ticks();
  inline void set_profiler_ticks(int ticks);

  // [builtin_index]: For builtins, tells which builtin index the code object
  // has. Note that builtins can have a code kind other than BUILTIN. The
  // builtin index is a non-negative integer for builtins, and -1 otherwise.
  inline int builtin_index();
  inline void set_builtin_index(int id);

  // [stack_slots]: For kind OPTIMIZED_FUNCTION, the number of stack slots
  // reserved in the code prologue.
  inline unsigned stack_slots();
  inline void set_stack_slots(unsigned slots);

  // [safepoint_table_start]: For kind OPTIMIZED_FUNCTION, the offset in
  // the instruction stream where the safepoint table starts.
  inline unsigned safepoint_table_offset();
  inline void set_safepoint_table_offset(unsigned offset);

  // [back_edge_table_start]: For kind FUNCTION, the offset in the
  // instruction stream where the back edge table starts.
  inline unsigned back_edge_table_offset();
  inline void set_back_edge_table_offset(unsigned offset);

  inline bool back_edges_patched_for_osr();

  // [to_boolean_foo]: For kind TO_BOOLEAN_IC tells what state the stub is in.
  inline uint16_t to_boolean_state();

  // [marked_for_deoptimization]: For kind OPTIMIZED_FUNCTION tells whether
  // the code is going to be deoptimized because of dead embedded maps.
  inline bool marked_for_deoptimization();
  inline void set_marked_for_deoptimization(bool flag);

  // [is_promise_rejection]: For kind BUILTIN tells whether the exception
  // thrown by the code will lead to promise rejection.
  inline bool is_promise_rejection();
  inline void set_is_promise_rejection(bool flag);

  // [is_exception_caught]: For kind BUILTIN tells whether the exception
  // thrown by the code will be caught internally.
  inline bool is_exception_caught();
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

  class FindAndReplacePattern;
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
      Kind kind, ExtraICState extra_ic_state = kNoExtraICState,
      CacheHolderFlag holder = kCacheOnReceiver);

  static inline Flags ComputeHandlerFlags(
      Kind handler_kind, CacheHolderFlag holder = kCacheOnReceiver);

  static inline CacheHolderFlag ExtractCacheHolderFromFlags(Flags flags);
  static inline Kind ExtractKindFromFlags(Flags flags);
  static inline ExtraICState ExtractExtraICStateFromFlags(Flags flags);

  static inline Flags RemoveHolderFromFlags(Flags flags);

  // Convert a target address into a code object.
  static inline Code* GetCodeFromTargetAddress(Address address);

  // Convert an entry address into an object.
  static inline Object* GetObjectFromEntryAddress(Address location_of_address);

  // Returns the address of the first instruction.
  inline byte* instruction_start();

  // Returns the address right after the last instruction.
  inline byte* instruction_end();

  // Returns the size of the instructions, padding, relocation and unwinding
  // information.
  inline int body_size();

  // Returns the size of code and its metadata. This includes the size of code
  // relocation information, deoptimization data and handler table.
  inline int SizeIncludingMetadata();

  // Returns the address of the first relocation info (read backwards!).
  inline byte* relocation_start();

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
  inline byte* unwinding_info_start();

  // Returns the address right after the end of the unwinding information.
  inline byte* unwinding_info_end();

  // Code entry point.
  inline byte* entry();

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
  inline int ExecutableSize();

  DECLARE_CAST(Code)

  // Dispatched behavior.
  inline int CodeSize();

  DECLARE_PRINTER(Code)
  DECLARE_VERIFIER(Code)

  void ClearInlineCaches();

  BailoutId TranslatePcOffsetToAstId(uint32_t pc_offset);
  uint32_t TranslateAstIdToPcOffset(BailoutId ast_id);

#define DECLARE_CODE_AGE_ENUM(X) k##X##CodeAge,
  enum Age {
    kToBeExecutedOnceCodeAge = -3,
    kNotExecutedCodeAge = -2,
    kExecutedOnceCodeAge = -1,
    kNoAgeCodeAge = 0,
    CODE_AGE_LIST(DECLARE_CODE_AGE_ENUM)
    kAfterLastCodeAge,
    kFirstCodeAge = kToBeExecutedOnceCodeAge,
    kLastCodeAge = kAfterLastCodeAge - 1,
    kCodeAgeCount = kAfterLastCodeAge - kFirstCodeAge - 1,
    kIsOldCodeAge = kSexagenarianCodeAge,
    kPreAgedCodeAge = kIsOldCodeAge - 1
  };
#undef DECLARE_CODE_AGE_ENUM

  // Code aging.  Indicates how many full GCs this code has survived without
  // being entered through the prologue.  Used to determine when it is
  // relatively safe to flush this code object and replace it with the lazy
  // compilation stub.
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
  static const int kProtectedInstructionOffset =
      kTypeFeedbackInfoOffset + kPointerSize;
  static const int kNextCodeLinkOffset =
      kProtectedInstructionOffset + kPointerSize;
  static const int kGCMetadataOffset = kNextCodeLinkOffset + kPointerSize;
  static const int kInstructionSizeOffset = kGCMetadataOffset + kPointerSize;
  static const int kICAgeOffset = kInstructionSizeOffset + kIntSize;
  static const int kFlagsOffset = kICAgeOffset + kIntSize;
  static const int kKindSpecificFlags1Offset = kFlagsOffset + kIntSize;
  static const int kKindSpecificFlags2Offset =
      kKindSpecificFlags1Offset + kIntSize;
  // Note: We might be able to squeeze this into the flags above.
  static const int kPrologueOffset = kKindSpecificFlags2Offset + kIntSize;
  static const int kConstantPoolOffset = kPrologueOffset + kIntSize;
  static const int kBuiltinIndexOffset =
      kConstantPoolOffset + kConstantPoolSize;
  static const int kHeaderPaddingStart = kBuiltinIndexOffset + kIntSize;

  enum TrapFields { kTrapCodeOffset, kTrapLandingOffset, kTrapDataSize };


  // Add padding to align the instruction start following right after
  // the Code object header.
  static const int kHeaderSize =
      (kHeaderPaddingStart + kCodeAlignmentMask) & ~kCodeAlignmentMask;

  inline int GetUnwindingInfoSizeOffset() const;

  class BodyDescriptor;

  // Byte offsets within kKindSpecificFlags1Offset.
  static const int kFullCodeFlags = kKindSpecificFlags1Offset;
  class FullCodeFlagsHasDeoptimizationSupportField:
      public BitField<bool, 0, 1> {};  // NOLINT
  class FullCodeFlagsHasDebugBreakSlotsField: public BitField<bool, 1, 1> {};
  class FullCodeFlagsHasRelocInfoForSerialization
      : public BitField<bool, 2, 1> {};
  // Bit 3 in this bitfield is unused.
  class ProfilerTicksField : public BitField<int, 4, 28> {};

  // Flags layout.  BitField<type, shift, size>.
  class HasUnwindingInfoField : public BitField<bool, 0, 1> {};
  class CacheHolderField
      : public BitField<CacheHolderFlag, HasUnwindingInfoField::kNext, 2> {};
  class KindField : public BitField<Kind, CacheHolderField::kNext, 5> {};
  STATIC_ASSERT(NUMBER_OF_KINDS <= KindField::kMax);
  class ExtraICStateField
      : public BitField<ExtraICState, KindField::kNext,
                        PlatformSmiTagging::kSmiValueSize - KindField::kNext> {
  };

  // KindSpecificFlags1 layout (STUB, BUILTIN and OPTIMIZED_FUNCTION)
  static const int kStackSlotsFirstBit = 0;
  static const int kStackSlotsBitCount = 24;
  static const int kMarkedForDeoptimizationBit =
      kStackSlotsFirstBit + kStackSlotsBitCount;
  static const int kIsTurbofannedBit = kMarkedForDeoptimizationBit + 1;
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
  class IsCrankshaftedField: public BitField<bool,
      kIsCrankshaftedBit, 1> {};  // NOLINT

  // KindSpecificFlags2 layout (STUB and OPTIMIZED_FUNCTION)
  static const int kSafepointTableOffsetFirstBit = kIsCrankshaftedBit + 1;
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

  // This constant should be encodable in an ARM instruction.
  static const int kFlagsNotUsedInLookup = CacheHolderField::kMask;

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

  // Returns the size of instructions and the metadata.
  inline int SizeIncludingMetadata();

  // Returns true if pc is inside this object's instructions.
  inline bool contains(byte* pc);

  // Returns the AbstractCode::Kind of the code.
  inline Kind kind();

  // Calculate the size of the code object to report for log events. This takes
  // the layout of the code object into account.
  inline int ExecutableSize();

  DECLARE_CAST(AbstractCode)
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
  DECLARE_CAST(DependentCode)

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


// All heap objects have a Map that describes their structure.
//  A Map contains information about:
//  - Size information about the object
//  - How to iterate over an object (for garbage collection)
class Map: public HeapObject {
 public:
  // Instance size.
  // Size in bytes or kVariableSizeSentinel if instances do not have
  // a fixed size.
  inline int instance_size();
  inline void set_instance_size(int value);

  // Only to clear an unused byte, remove once byte is used.
  inline void clear_unused();

  // [inobject_properties_or_constructor_function_index]: Provides access
  // to the inobject properties in case of JSObject maps, or the constructor
  // function index in case of primitive maps.
  inline int inobject_properties_or_constructor_function_index();
  inline void set_inobject_properties_or_constructor_function_index(int value);
  // Count of properties allocated in the object (JSObject only).
  inline int GetInObjectProperties();
  inline void SetInObjectProperties(int value);
  // Index of the constructor function in the native context (primitives only),
  // or the special sentinel value to indicate that there is no object wrapper
  // for the primitive (i.e. in case of null or undefined).
  static const int kNoConstructorFunctionIndex = 0;
  inline int GetConstructorFunctionIndex();
  inline void SetConstructorFunctionIndex(int value);
  static MaybeHandle<JSFunction> GetConstructorFunction(
      Handle<Map> map, Handle<Context> native_context);

  // Retrieve interceptors.
  inline InterceptorInfo* GetNamedInterceptor();
  inline InterceptorInfo* GetIndexedInterceptor();

  // Instance type.
  inline InstanceType instance_type();
  inline void set_instance_type(InstanceType value);

  // Tells how many unused property fields are available in the
  // instance (only used for JSObject in fast mode).
  inline int unused_property_fields();
  inline void set_unused_property_fields(int value);

  // Bit field.
  inline byte bit_field() const;
  inline void set_bit_field(byte value);

  // Bit field 2.
  inline byte bit_field2() const;
  inline void set_bit_field2(byte value);

  // Bit field 3.
  inline uint32_t bit_field3() const;
  inline void set_bit_field3(uint32_t bits);

  class EnumLengthBits:             public BitField<int,
      0, kDescriptorIndexBitCount> {};  // NOLINT
  class NumberOfOwnDescriptorsBits: public BitField<int,
      kDescriptorIndexBitCount, kDescriptorIndexBitCount> {};  // NOLINT
  STATIC_ASSERT(kDescriptorIndexBitCount + kDescriptorIndexBitCount == 20);
  class DictionaryMap : public BitField<bool, 20, 1> {};
  class OwnsDescriptors : public BitField<bool, 21, 1> {};
  class HasHiddenPrototype : public BitField<bool, 22, 1> {};
  class Deprecated : public BitField<bool, 23, 1> {};
  class IsUnstable : public BitField<bool, 24, 1> {};
  class IsMigrationTarget : public BitField<bool, 25, 1> {};
  class ImmutablePrototype : public BitField<bool, 26, 1> {};
  class NewTargetIsBase : public BitField<bool, 27, 1> {};
  // Bit 28 is free.

  // Keep this bit field at the very end for better code in
  // Builtins::kJSConstructStubGeneric stub.
  // This counter is used for in-object slack tracking.
  // The in-object slack tracking is considered enabled when the counter is
  // non zero. The counter only has a valid count for initial maps. For
  // transitioned maps only kNoSlackTracking has a meaning, namely that inobject
  // slack tracking already finished for the transition tree. Any other value
  // indicates that either inobject slack tracking is still in progress, or that
  // the map isn't part of the transition tree anymore.
  class ConstructionCounter : public BitField<int, 29, 3> {};
  static const int kSlackTrackingCounterStart = 7;
  static const int kSlackTrackingCounterEnd = 1;
  static const int kNoSlackTracking = 0;
  STATIC_ASSERT(kSlackTrackingCounterStart <= ConstructionCounter::kMax);


  // Inobject slack tracking is the way to reclaim unused inobject space.
  //
  // The instance size is initially determined by adding some slack to
  // expected_nof_properties (to allow for a few extra properties added
  // after the constructor). There is no guarantee that the extra space
  // will not be wasted.
  //
  // Here is the algorithm to reclaim the unused inobject space:
  // - Detect the first constructor call for this JSFunction.
  //   When it happens enter the "in progress" state: initialize construction
  //   counter in the initial_map.
  // - While the tracking is in progress initialize unused properties of a new
  //   object with one_pointer_filler_map instead of undefined_value (the "used"
  //   part is initialized with undefined_value as usual). This way they can
  //   be resized quickly and safely.
  // - Once enough objects have been created  compute the 'slack'
  //   (traverse the map transition tree starting from the
  //   initial_map and find the lowest value of unused_property_fields).
  // - Traverse the transition tree again and decrease the instance size
  //   of every map. Existing objects will resize automatically (they are
  //   filled with one_pointer_filler_map). All further allocations will
  //   use the adjusted instance size.
  // - SharedFunctionInfo's expected_nof_properties left unmodified since
  //   allocations made using different closures could actually create different
  //   kind of objects (see prototype inheritance pattern).
  //
  //  Important: inobject slack tracking is not attempted during the snapshot
  //  creation.

  static const int kGenerousAllocationCount =
      kSlackTrackingCounterStart - kSlackTrackingCounterEnd + 1;

  // Starts the tracking by initializing object constructions countdown counter.
  void StartInobjectSlackTracking();

  // True if the object constructions countdown counter is a range
  // [kSlackTrackingCounterEnd, kSlackTrackingCounterStart].
  inline bool IsInobjectSlackTrackingInProgress();

  // Does the tracking step.
  inline void InobjectSlackTrackingStep();

  // Completes inobject slack tracking for the transition tree starting at this
  // initial map.
  void CompleteInobjectSlackTracking();

  // Tells whether the object in the prototype property will be used
  // for instances created from this function.  If the prototype
  // property is set to a value that is not a JSObject, the prototype
  // property will not be used to create instances of the function.
  // See ECMA-262, 13.2.2.
  inline void set_non_instance_prototype(bool value);
  inline bool has_non_instance_prototype();

  // Tells whether the instance has a [[Construct]] internal method.
  // This property is implemented according to ES6, section 7.2.4.
  inline void set_is_constructor(bool value);
  inline bool is_constructor() const;

  // Tells whether the instance with this map has a hidden prototype.
  inline void set_has_hidden_prototype(bool value);
  inline bool has_hidden_prototype() const;

  // Records and queries whether the instance has a named interceptor.
  inline void set_has_named_interceptor();
  inline bool has_named_interceptor();

  // Records and queries whether the instance has an indexed interceptor.
  inline void set_has_indexed_interceptor();
  inline bool has_indexed_interceptor();

  // Tells whether the instance is undetectable.
  // An undetectable object is a special class of JSObject: 'typeof' operator
  // returns undefined, ToBoolean returns false. Otherwise it behaves like
  // a normal JS object.  It is useful for implementing undetectable
  // document.all in Firefox & Safari.
  // See https://bugzilla.mozilla.org/show_bug.cgi?id=248549.
  inline void set_is_undetectable();
  inline bool is_undetectable();

  // Tells whether the instance has a [[Call]] internal method.
  // This property is implemented according to ES6, section 7.2.3.
  inline void set_is_callable();
  inline bool is_callable() const;

  inline void set_new_target_is_base(bool value);
  inline bool new_target_is_base();
  inline void set_is_extensible(bool value);
  inline bool is_extensible();
  inline void set_is_prototype_map(bool value);
  inline bool is_prototype_map() const;

  inline void set_elements_kind(ElementsKind elements_kind);
  inline ElementsKind elements_kind();

  // Tells whether the instance has fast elements that are only Smis.
  inline bool has_fast_smi_elements();

  // Tells whether the instance has fast elements.
  inline bool has_fast_object_elements();
  inline bool has_fast_smi_or_object_elements();
  inline bool has_fast_double_elements();
  inline bool has_fast_elements();
  inline bool has_sloppy_arguments_elements();
  inline bool has_fast_sloppy_arguments_elements();
  inline bool has_fast_string_wrapper_elements();
  inline bool has_fixed_typed_array_elements();
  inline bool has_dictionary_elements();

  static bool IsValidElementsTransition(ElementsKind from_kind,
                                        ElementsKind to_kind);

  // Returns true if the current map doesn't have DICTIONARY_ELEMENTS but if a
  // map with DICTIONARY_ELEMENTS was found in the prototype chain.
  bool DictionaryElementsInPrototypeChainOnly();

  inline Map* ElementsTransitionMap();

  inline FixedArrayBase* GetInitialElements();

  // [raw_transitions]: Provides access to the transitions storage field.
  // Don't call set_raw_transitions() directly to overwrite transitions, use
  // the TransitionArray::ReplaceTransitions() wrapper instead!
  DECL_ACCESSORS(raw_transitions, Object)
  // [prototype_info]: Per-prototype metadata. Aliased with transitions
  // (which prototype maps don't have).
  DECL_ACCESSORS(prototype_info, Object)
  // PrototypeInfo is created lazily using this helper (which installs it on
  // the given prototype's map).
  static Handle<PrototypeInfo> GetOrCreatePrototypeInfo(
      Handle<JSObject> prototype, Isolate* isolate);
  static Handle<PrototypeInfo> GetOrCreatePrototypeInfo(
      Handle<Map> prototype_map, Isolate* isolate);
  inline bool should_be_fast_prototype_map() const;
  static void SetShouldBeFastPrototypeMap(Handle<Map> map, bool value,
                                          Isolate* isolate);

  // [prototype chain validity cell]: Associated with a prototype object,
  // stored in that object's map's PrototypeInfo, indicates that prototype
  // chains through this object are currently valid. The cell will be
  // invalidated and replaced when the prototype chain changes.
  static Handle<Cell> GetOrCreatePrototypeChainValidityCell(Handle<Map> map,
                                                            Isolate* isolate);
  static const int kPrototypeChainValid = 0;
  static const int kPrototypeChainInvalid = 1;

  // Return the map of the root of object's prototype chain.
  Map* GetPrototypeChainRootMap(Isolate* isolate);

  // Returns a WeakCell object containing given prototype. The cell is cached
  // in PrototypeInfo which is created lazily.
  static Handle<WeakCell> GetOrCreatePrototypeWeakCell(
      Handle<JSObject> prototype, Isolate* isolate);

  Map* FindRootMap();
  Map* FindFieldOwner(int descriptor);

  inline int GetInObjectPropertyOffset(int index);

  int NumberOfFields();

  // TODO(ishell): candidate with JSObject::MigrateToMap().
  bool InstancesNeedRewriting(Map* target);
  bool InstancesNeedRewriting(Map* target, int target_number_of_fields,
                              int target_inobject, int target_unused,
                              int* old_number_of_fields);
  // TODO(ishell): moveit!
  static Handle<Map> GeneralizeAllFields(Handle<Map> map);
  MUST_USE_RESULT static Handle<FieldType> GeneralizeFieldType(
      Representation rep1, Handle<FieldType> type1, Representation rep2,
      Handle<FieldType> type2, Isolate* isolate);
  static void GeneralizeField(Handle<Map> map, int modify_index,
                              Representation new_representation,
                              Handle<FieldType> new_field_type);

  static Handle<Map> ReconfigureProperty(Handle<Map> map, int modify_index,
                                         PropertyKind new_kind,
                                         PropertyAttributes new_attributes,
                                         Representation new_representation,
                                         Handle<FieldType> new_field_type);

  static Handle<Map> ReconfigureElementsKind(Handle<Map> map,
                                             ElementsKind new_elements_kind);

  static Handle<Map> PrepareForDataProperty(Handle<Map> old_map,
                                            int descriptor_number,
                                            Handle<Object> value);

  static Handle<Map> Normalize(Handle<Map> map, PropertyNormalizationMode mode,
                               const char* reason);

  // Tells whether the map is used for JSObjects in dictionary mode (ie
  // normalized objects, ie objects for which HasFastProperties returns false).
  // A map can never be used for both dictionary mode and fast mode JSObjects.
  // False by default and for HeapObjects that are not JSObjects.
  inline void set_dictionary_map(bool value);
  inline bool is_dictionary_map();

  // Tells whether the instance needs security checks when accessing its
  // properties.
  inline void set_is_access_check_needed(bool access_check_needed);
  inline bool is_access_check_needed();

  // Returns true if map has a non-empty stub code cache.
  inline bool has_code_cache();

  // [prototype]: implicit prototype object.
  DECL_ACCESSORS(prototype, Object)
  // TODO(jkummerow): make set_prototype private.
  static void SetPrototype(
      Handle<Map> map, Handle<Object> prototype,
      PrototypeOptimizationMode proto_mode = FAST_PROTOTYPE);

  // [constructor]: points back to the function responsible for this map.
  // The field overlaps with the back pointer. All maps in a transition tree
  // have the same constructor, so maps with back pointers can walk the
  // back pointer chain until they find the map holding their constructor.
  DECL_ACCESSORS(constructor_or_backpointer, Object)
  inline Object* GetConstructor() const;
  inline void SetConstructor(Object* constructor,
                             WriteBarrierMode mode = UPDATE_WRITE_BARRIER);
  // [back pointer]: points back to the parent map from which a transition
  // leads to this map. The field overlaps with the constructor (see above).
  inline Object* GetBackPointer();
  inline void SetBackPointer(Object* value,
                             WriteBarrierMode mode = UPDATE_WRITE_BARRIER);

  // [instance descriptors]: describes the object.
  DECL_ACCESSORS(instance_descriptors, DescriptorArray)

  // [layout descriptor]: describes the object layout.
  DECL_ACCESSORS(layout_descriptor, LayoutDescriptor)
  // |layout descriptor| accessor which can be used from GC.
  inline LayoutDescriptor* layout_descriptor_gc_safe();
  inline bool HasFastPointerLayout() const;

  // |layout descriptor| accessor that is safe to call even when
  // FLAG_unbox_double_fields is disabled (in this case Map does not contain
  // |layout_descriptor| field at all).
  inline LayoutDescriptor* GetLayoutDescriptor();

  inline void UpdateDescriptors(DescriptorArray* descriptors,
                                LayoutDescriptor* layout_descriptor);
  inline void InitializeDescriptors(DescriptorArray* descriptors,
                                    LayoutDescriptor* layout_descriptor);

  // [stub cache]: contains stubs compiled for this map.
  DECL_ACCESSORS(code_cache, FixedArray)

  // [dependent code]: list of optimized codes that weakly embed this map.
  DECL_ACCESSORS(dependent_code, DependentCode)

  // [weak cell cache]: cache that stores a weak cell pointing to this map.
  DECL_ACCESSORS(weak_cell_cache, Object)

  inline PropertyDetails GetLastDescriptorDetails();

  inline int LastAdded();

  inline int NumberOfOwnDescriptors();
  inline void SetNumberOfOwnDescriptors(int number);

  inline Cell* RetrieveDescriptorsPointer();

  // Checks whether all properties are stored either in the map or on the object
  // (inobject, properties, or elements backing store), requiring no special
  // checks.
  bool OnlyHasSimpleProperties();
  inline int EnumLength();
  inline void SetEnumLength(int length);

  inline bool owns_descriptors();
  inline void set_owns_descriptors(bool owns_descriptors);
  inline void mark_unstable();
  inline bool is_stable();
  inline void set_migration_target(bool value);
  inline bool is_migration_target();
  inline void set_immutable_proto(bool value);
  inline bool is_immutable_proto();
  inline void set_construction_counter(int value);
  inline int construction_counter();
  inline void deprecate();
  inline bool is_deprecated();
  inline bool CanBeDeprecated();
  // Returns a non-deprecated version of the input. If the input was not
  // deprecated, it is directly returned. Otherwise, the non-deprecated version
  // is found by re-transitioning from the root of the transition tree using the
  // descriptor array of the map. Returns MaybeHandle<Map>() if no updated map
  // is found.
  static MaybeHandle<Map> TryUpdate(Handle<Map> map) WARN_UNUSED_RESULT;

  // Returns a non-deprecated version of the input. This method may deprecate
  // existing maps along the way if encodings conflict. Not for use while
  // gathering type feedback. Use TryUpdate in those cases instead.
  static Handle<Map> Update(Handle<Map> map);

  static inline Handle<Map> CopyInitialMap(Handle<Map> map);
  static Handle<Map> CopyInitialMap(Handle<Map> map, int instance_size,
                                    int in_object_properties,
                                    int unused_property_fields);
  static Handle<Map> CopyDropDescriptors(Handle<Map> map);
  static Handle<Map> CopyInsertDescriptor(Handle<Map> map,
                                          Descriptor* descriptor,
                                          TransitionFlag flag);

  static Handle<Object> WrapFieldType(Handle<FieldType> type);
  static FieldType* UnwrapFieldType(Object* wrapped_type);

  MUST_USE_RESULT static MaybeHandle<Map> CopyWithField(
      Handle<Map> map, Handle<Name> name, Handle<FieldType> type,
      PropertyAttributes attributes, Representation representation,
      TransitionFlag flag);

  MUST_USE_RESULT static MaybeHandle<Map> CopyWithConstant(
      Handle<Map> map,
      Handle<Name> name,
      Handle<Object> constant,
      PropertyAttributes attributes,
      TransitionFlag flag);

  // Returns a new map with all transitions dropped from the given map and
  // the ElementsKind set.
  static Handle<Map> TransitionElementsTo(Handle<Map> map,
                                          ElementsKind to_kind);

  static Handle<Map> AsElementsKind(Handle<Map> map, ElementsKind kind);

  static Handle<Map> CopyAsElementsKind(Handle<Map> map,
                                        ElementsKind kind,
                                        TransitionFlag flag);

  static Handle<Map> AsLanguageMode(Handle<Map> initial_map,
                                    LanguageMode language_mode,
                                    FunctionKind kind);


  static Handle<Map> CopyForPreventExtensions(Handle<Map> map,
                                              PropertyAttributes attrs_to_add,
                                              Handle<Symbol> transition_marker,
                                              const char* reason);

  static Handle<Map> FixProxy(Handle<Map> map, InstanceType type, int size);


  // Maximal number of fast properties. Used to restrict the number of map
  // transitions to avoid an explosion in the number of maps for objects used as
  // dictionaries.
  inline bool TooManyFastProperties(StoreFromKeyed store_mode);
  static Handle<Map> TransitionToDataProperty(Handle<Map> map,
                                              Handle<Name> name,
                                              Handle<Object> value,
                                              PropertyAttributes attributes,
                                              StoreFromKeyed store_mode);
  static Handle<Map> TransitionToAccessorProperty(
      Isolate* isolate, Handle<Map> map, Handle<Name> name, int descriptor,
      Handle<Object> getter, Handle<Object> setter,
      PropertyAttributes attributes);
  static Handle<Map> ReconfigureExistingProperty(Handle<Map> map,
                                                 int descriptor,
                                                 PropertyKind kind,
                                                 PropertyAttributes attributes);

  inline void AppendDescriptor(Descriptor* desc);

  // Returns a copy of the map, prepared for inserting into the transition
  // tree (if the |map| owns descriptors then the new one will share
  // descriptors with |map|).
  static Handle<Map> CopyForTransition(Handle<Map> map, const char* reason);

  // Returns a copy of the map, with all transitions dropped from the
  // instance descriptors.
  static Handle<Map> Copy(Handle<Map> map, const char* reason);
  static Handle<Map> Create(Isolate* isolate, int inobject_properties);

  // Returns the next free property index (only valid for FAST MODE).
  int NextFreePropertyIndex();

  // Returns the number of properties described in instance_descriptors
  // filtering out properties with the specified attributes.
  int NumberOfDescribedProperties(DescriptorFlag which = OWN_DESCRIPTORS,
                                  PropertyFilter filter = ALL_PROPERTIES);

  DECLARE_CAST(Map)

  // Code cache operations.

  // Clears the code cache.
  inline void ClearCodeCache(Heap* heap);

  // Update code cache.
  static void UpdateCodeCache(Handle<Map> map,
                              Handle<Name> name,
                              Handle<Code> code);

  // Extend the descriptor array of the map with the list of descriptors.
  // In case of duplicates, the latest descriptor is used.
  static void AppendCallbackDescriptors(Handle<Map> map,
                                        Handle<Object> descriptors);

  static inline int SlackForArraySize(int old_size, int size_limit);

  static void EnsureDescriptorSlack(Handle<Map> map, int slack);

  Code* LookupInCodeCache(Name* name, Code::Flags code);

  static Handle<Map> GetObjectCreateMap(Handle<HeapObject> prototype);

  // Computes a hash value for this map, to be used in HashTables and such.
  int Hash();

  // Returns the transitioned map for this map with the most generic
  // elements_kind that's found in |candidates|, or |nullptr| if no match is
  // found at all.
  Map* FindElementsKindTransitionedMap(MapHandleList* candidates);

  inline bool CanTransition();

  inline bool IsBooleanMap();
  inline bool IsPrimitiveMap();
  inline bool IsJSReceiverMap();
  inline bool IsJSObjectMap();
  inline bool IsJSArrayMap();
  inline bool IsJSFunctionMap();
  inline bool IsStringMap();
  inline bool IsJSProxyMap();
  inline bool IsModuleMap();
  inline bool IsJSGlobalProxyMap();
  inline bool IsJSGlobalObjectMap();
  inline bool IsJSTypedArrayMap();
  inline bool IsJSDataViewMap();

  inline bool IsSpecialReceiverMap();

  inline bool CanOmitMapChecks();

  static void AddDependentCode(Handle<Map> map,
                               DependentCode::DependencyGroup group,
                               Handle<Code> code);

  bool IsMapInArrayPrototypeChain();

  static Handle<WeakCell> WeakCellForMap(Handle<Map> map);

  // Dispatched behavior.
  DECLARE_PRINTER(Map)
  DECLARE_VERIFIER(Map)

#ifdef VERIFY_HEAP
  void DictionaryMapVerify();
  void VerifyOmittedMapChecks();
#endif

  inline int visitor_id();
  inline void set_visitor_id(int visitor_id);

  static Handle<Map> TransitionToPrototype(Handle<Map> map,
                                           Handle<Object> prototype,
                                           PrototypeOptimizationMode mode);

  static Handle<Map> TransitionToImmutableProto(Handle<Map> map);

  static const int kMaxPreAllocatedPropertyFields = 255;

  // Layout description.
  static const int kInstanceSizesOffset = HeapObject::kHeaderSize;
  static const int kInstanceAttributesOffset = kInstanceSizesOffset + kIntSize;
  static const int kBitField3Offset = kInstanceAttributesOffset + kIntSize;
  static const int kPrototypeOffset = kBitField3Offset + kPointerSize;
  static const int kConstructorOrBackPointerOffset =
      kPrototypeOffset + kPointerSize;
  // When there is only one transition, it is stored directly in this field;
  // otherwise a transition array is used.
  // For prototype maps, this slot is used to store this map's PrototypeInfo
  // struct.
  static const int kTransitionsOrPrototypeInfoOffset =
      kConstructorOrBackPointerOffset + kPointerSize;
  static const int kDescriptorsOffset =
      kTransitionsOrPrototypeInfoOffset + kPointerSize;
#if V8_DOUBLE_FIELDS_UNBOXING
  static const int kLayoutDescriptorOffset = kDescriptorsOffset + kPointerSize;
  static const int kCodeCacheOffset = kLayoutDescriptorOffset + kPointerSize;
#else
  static const int kLayoutDescriptorOffset = 1;  // Must not be ever accessed.
  static const int kCodeCacheOffset = kDescriptorsOffset + kPointerSize;
#endif
  static const int kDependentCodeOffset = kCodeCacheOffset + kPointerSize;
  static const int kWeakCellCacheOffset = kDependentCodeOffset + kPointerSize;
  static const int kSize = kWeakCellCacheOffset + kPointerSize;

  // Layout of pointer fields. Heap iteration code relies on them
  // being continuously allocated.
  static const int kPointerFieldsBeginOffset = Map::kPrototypeOffset;
  static const int kPointerFieldsEndOffset = kSize;

  // Byte offsets within kInstanceSizesOffset.
  static const int kInstanceSizeOffset = kInstanceSizesOffset + 0;
  static const int kInObjectPropertiesOrConstructorFunctionIndexByte = 1;
  static const int kInObjectPropertiesOrConstructorFunctionIndexOffset =
      kInstanceSizesOffset + kInObjectPropertiesOrConstructorFunctionIndexByte;
  // Note there is one byte available for use here.
  static const int kUnusedByte = 2;
  static const int kUnusedOffset = kInstanceSizesOffset + kUnusedByte;
  static const int kVisitorIdByte = 3;
  static const int kVisitorIdOffset = kInstanceSizesOffset + kVisitorIdByte;

  // Byte offsets within kInstanceAttributesOffset attributes.
#if V8_TARGET_LITTLE_ENDIAN
  // Order instance type and bit field together such that they can be loaded
  // together as a 16-bit word with instance type in the lower 8 bits regardless
  // of endianess. Also provide endian-independent offset to that 16-bit word.
  static const int kInstanceTypeOffset = kInstanceAttributesOffset + 0;
  static const int kBitFieldOffset = kInstanceAttributesOffset + 1;
#else
  static const int kBitFieldOffset = kInstanceAttributesOffset + 0;
  static const int kInstanceTypeOffset = kInstanceAttributesOffset + 1;
#endif
  static const int kInstanceTypeAndBitFieldOffset =
      kInstanceAttributesOffset + 0;
  static const int kBitField2Offset = kInstanceAttributesOffset + 2;
  static const int kUnusedPropertyFieldsByte = 3;
  static const int kUnusedPropertyFieldsOffset = kInstanceAttributesOffset + 3;

  STATIC_ASSERT(kInstanceTypeAndBitFieldOffset ==
                Internals::kMapInstanceTypeAndBitFieldOffset);

  // Bit positions for bit field.
  static const int kHasNonInstancePrototype = 0;
  static const int kIsCallable = 1;
  static const int kHasNamedInterceptor = 2;
  static const int kHasIndexedInterceptor = 3;
  static const int kIsUndetectable = 4;
  static const int kIsAccessCheckNeeded = 5;
  static const int kIsConstructor = 6;
  // Bit 7 is free.

  // Bit positions for bit field 2
  static const int kIsExtensible = 0;
  // Bit 1 is free.
  class IsPrototypeMapBits : public BitField<bool, 2, 1> {};
  class ElementsKindBits: public BitField<ElementsKind, 3, 5> {};

  // Derived values from bit field 2
  static const int8_t kMaximumBitField2FastElementValue = static_cast<int8_t>(
      (FAST_ELEMENTS + 1) << Map::ElementsKindBits::kShift) - 1;
  static const int8_t kMaximumBitField2FastSmiElementValue =
      static_cast<int8_t>((FAST_SMI_ELEMENTS + 1) <<
                          Map::ElementsKindBits::kShift) - 1;
  static const int8_t kMaximumBitField2FastHoleyElementValue =
      static_cast<int8_t>((FAST_HOLEY_ELEMENTS + 1) <<
                          Map::ElementsKindBits::kShift) - 1;
  static const int8_t kMaximumBitField2FastHoleySmiElementValue =
      static_cast<int8_t>((FAST_HOLEY_SMI_ELEMENTS + 1) <<
                          Map::ElementsKindBits::kShift) - 1;

  typedef FixedBodyDescriptor<kPointerFieldsBeginOffset,
                              kPointerFieldsEndOffset,
                              kSize> BodyDescriptor;

  // Compares this map to another to see if they describe equivalent objects.
  // If |mode| is set to CLEAR_INOBJECT_PROPERTIES, |other| is treated as if
  // it had exactly zero inobject properties.
  // The "shared" flags of both this map and |other| are ignored.
  bool EquivalentToForNormalization(Map* other, PropertyNormalizationMode mode);

  // Returns true if given field is unboxed double.
  inline bool IsUnboxedDoubleField(FieldIndex index);

#if TRACE_MAPS
  static void TraceTransition(const char* what, Map* from, Map* to, Name* name);
  static void TraceAllTransitions(Map* map);
#endif

  static inline Handle<Map> AddMissingTransitionsForTesting(
      Handle<Map> split_map, Handle<DescriptorArray> descriptors,
      Handle<LayoutDescriptor> full_layout_descriptor);

  // Fires when the layout of an object with a leaf map changes.
  // This includes adding transitions to the leaf map or changing
  // the descriptor array.
  inline void NotifyLeafMapLayoutChange();

 private:
  // Returns the map that this (root) map transitions to if its elements_kind
  // is changed to |elements_kind|, or |nullptr| if no such map is cached yet.
  Map* LookupElementsTransitionMap(ElementsKind elements_kind);

  // Tries to replay property transitions starting from this (root) map using
  // the descriptor array of the |map|. The |root_map| is expected to have
  // proper elements kind and therefore elements kinds transitions are not
  // taken by this function. Returns |nullptr| if matching transition map is
  // not found.
  Map* TryReplayPropertyTransitions(Map* map);

  static void ConnectTransition(Handle<Map> parent, Handle<Map> child,
                                Handle<Name> name, SimpleTransitionFlag flag);

  bool EquivalentToForTransition(Map* other);
  static Handle<Map> RawCopy(Handle<Map> map, int instance_size);
  static Handle<Map> ShareDescriptor(Handle<Map> map,
                                     Handle<DescriptorArray> descriptors,
                                     Descriptor* descriptor);
  static Handle<Map> AddMissingTransitions(
      Handle<Map> map, Handle<DescriptorArray> descriptors,
      Handle<LayoutDescriptor> full_layout_descriptor);
  static void InstallDescriptors(
      Handle<Map> parent_map, Handle<Map> child_map, int new_descriptor,
      Handle<DescriptorArray> descriptors,
      Handle<LayoutDescriptor> full_layout_descriptor);
  static Handle<Map> CopyAddDescriptor(Handle<Map> map,
                                       Descriptor* descriptor,
                                       TransitionFlag flag);
  static Handle<Map> CopyReplaceDescriptors(
      Handle<Map> map, Handle<DescriptorArray> descriptors,
      Handle<LayoutDescriptor> layout_descriptor, TransitionFlag flag,
      MaybeHandle<Name> maybe_name, const char* reason,
      SimpleTransitionFlag simple_flag);

  static Handle<Map> CopyReplaceDescriptor(Handle<Map> map,
                                           Handle<DescriptorArray> descriptors,
                                           Descriptor* descriptor,
                                           int index,
                                           TransitionFlag flag);
  static MUST_USE_RESULT MaybeHandle<Map> TryReconfigureExistingProperty(
      Handle<Map> map, int descriptor, PropertyKind kind,
      PropertyAttributes attributes, const char** reason);

  static Handle<Map> CopyNormalized(Handle<Map> map,
                                    PropertyNormalizationMode mode);

  // TODO(ishell): Move to MapUpdater.
  static Handle<Map> CopyGeneralizeAllFields(
      Handle<Map> map, ElementsKind elements_kind, int modify_index,
      PropertyKind kind, PropertyAttributes attributes, const char* reason);

  void DeprecateTransitionTree();

  void ReplaceDescriptors(DescriptorArray* new_descriptors,
                          LayoutDescriptor* new_layout_descriptor);


  // Update field type of the given descriptor to new representation and new
  // type. The type must be prepared for storing in descriptor array:
  // it must be either a simple type or a map wrapped in a weak cell.
  void UpdateFieldType(int descriptor_number, Handle<Name> name,
                       Representation new_representation,
                       Handle<Object> new_wrapped_type);

  // TODO(ishell): Move to MapUpdater.
  void PrintReconfiguration(FILE* file, int modify_index, PropertyKind kind,
                            PropertyAttributes attributes);
  // TODO(ishell): Move to MapUpdater.
  void PrintGeneralization(FILE* file, const char* reason, int modify_index,
                           int split, int descriptors, bool constant_to_field,
                           Representation old_representation,
                           Representation new_representation,
                           MaybeHandle<FieldType> old_field_type,
                           MaybeHandle<Object> old_value,
                           MaybeHandle<FieldType> new_field_type,
                           MaybeHandle<Object> new_value);
  static const int kFastPropertiesSoftLimit = 12;
  static const int kMaxFastProperties = 128;

  friend class MapUpdater;

  DISALLOW_IMPLICIT_CONSTRUCTORS(Map);
};


// An abstract superclass, a marker class really, for simple structure classes.
// It doesn't carry much functionality but allows struct classes to be
// identified in the type system.
class Struct: public HeapObject {
 public:
  inline void InitializeBody(int object_size);
  DECLARE_CAST(Struct)
};

// A container struct to hold state required for PromiseResolveThenableJob.
class PromiseResolveThenableJobInfo : public Struct {
 public:
  DECL_ACCESSORS(thenable, JSReceiver)
  DECL_ACCESSORS(then, JSReceiver)
  DECL_ACCESSORS(resolve, JSFunction)
  DECL_ACCESSORS(reject, JSFunction)

  DECL_INT_ACCESSORS(debug_id)

  DECL_ACCESSORS(context, Context)

  static const int kThenableOffset = Struct::kHeaderSize;
  static const int kThenOffset = kThenableOffset + kPointerSize;
  static const int kResolveOffset = kThenOffset + kPointerSize;
  static const int kRejectOffset = kResolveOffset + kPointerSize;
  static const int kDebugIdOffset = kRejectOffset + kPointerSize;
  static const int kContextOffset = kDebugIdOffset + kPointerSize;
  static const int kSize = kContextOffset + kPointerSize;

  DECLARE_CAST(PromiseResolveThenableJobInfo)
  DECLARE_PRINTER(PromiseResolveThenableJobInfo)
  DECLARE_VERIFIER(PromiseResolveThenableJobInfo)

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
  static const int kDebugIdOffset = kDeferredOnRejectOffset + kPointerSize;
  static const int kContextOffset = kDebugIdOffset + kPointerSize;
  static const int kSize = kContextOffset + kPointerSize;

  DECLARE_CAST(PromiseReactionJobInfo)
  DECLARE_PRINTER(PromiseReactionJobInfo)
  DECLARE_VERIFIER(PromiseReactionJobInfo)

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(PromiseReactionJobInfo);
};

// A simple one-element struct, useful where smis need to be boxed.
class Box : public Struct {
 public:
  // [value]: the boxed contents.
  DECL_ACCESSORS(value, Object)

  DECLARE_CAST(Box)

  // Dispatched behavior.
  DECLARE_PRINTER(Box)
  DECLARE_VERIFIER(Box)

  static const int kValueOffset = HeapObject::kHeaderSize;
  static const int kSize = kValueOffset + kPointerSize;

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(Box);
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

  DECLARE_CAST(PrototypeInfo)

  // Dispatched behavior.
  DECLARE_PRINTER(PrototypeInfo)
  DECLARE_VERIFIER(PrototypeInfo)

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

  DECLARE_CAST(Tuple2)

  // Dispatched behavior.
  DECLARE_PRINTER(Tuple2)
  DECLARE_VERIFIER(Tuple2)

  static const int kValue1Offset = HeapObject::kHeaderSize;
  static const int kValue2Offset = kValue1Offset + kPointerSize;
  static const int kSize = kValue2Offset + kPointerSize;

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(Tuple2);
};

class Tuple3 : public Tuple2 {
 public:
  DECL_ACCESSORS(value3, Object)

  DECLARE_CAST(Tuple3)

  // Dispatched behavior.
  DECLARE_PRINTER(Tuple3)
  DECLARE_VERIFIER(Tuple3)

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

  DECLARE_CAST(ContextExtension)

  // Dispatched behavior.
  DECLARE_PRINTER(ContextExtension)
  DECLARE_VERIFIER(ContextExtension)

  static const int kScopeInfoOffset = HeapObject::kHeaderSize;
  static const int kExtensionOffset = kScopeInfoOffset + kPointerSize;
  static const int kSize = kExtensionOffset + kPointerSize;

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(ContextExtension);
};

// Pair of {ElementsKind} and an array of constant values for {ArrayLiteral}
// expressions. Used to communicate with the runtime for literal boilerplate
// creation within the {Runtime_CreateArrayLiteral} method.
class ConstantElementsPair : public Struct {
 public:
  DECL_INT_ACCESSORS(elements_kind)
  DECL_ACCESSORS(constant_values, FixedArrayBase)

  DECLARE_CAST(ConstantElementsPair)

  // Dispatched behavior.
  DECLARE_PRINTER(ConstantElementsPair)
  DECLARE_VERIFIER(ConstantElementsPair)

  static const int kElementsKindOffset = HeapObject::kHeaderSize;
  static const int kConstantValuesOffset = kElementsKindOffset + kPointerSize;
  static const int kSize = kConstantValuesOffset + kPointerSize;

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(ConstantElementsPair);
};

// Script describes a script which has been added to the VM.
class Script: public Struct {
 public:
  // Script types.
  enum Type {
    TYPE_NATIVE = 0,
    TYPE_EXTENSION = 1,
    TYPE_NORMAL = 2,
    TYPE_WASM = 3,
    TYPE_INSPECTOR = 4
  };

  // Script compilation types.
  enum CompilationType {
    COMPILATION_TYPE_HOST = 0,
    COMPILATION_TYPE_EVAL = 1
  };

  // Script compilation state.
  enum CompilationState {
    COMPILATION_STATE_INITIAL = 0,
    COMPILATION_STATE_COMPILED = 1
  };

  // [source]: the script source.
  DECL_ACCESSORS(source, Object)

  // [name]: the script name.
  DECL_ACCESSORS(name, Object)

  // [id]: the script id.
  DECL_INT_ACCESSORS(id)

  // [line_offset]: script line offset in resource from where it was extracted.
  DECL_INT_ACCESSORS(line_offset)

  // [column_offset]: script column offset in resource from where it was
  // extracted.
  DECL_INT_ACCESSORS(column_offset)

  // [context_data]: context data for the context this script was compiled in.
  DECL_ACCESSORS(context_data, Object)

  // [wrapper]: the wrapper cache.  This is either undefined or a WeakCell.
  DECL_ACCESSORS(wrapper, HeapObject)

  // [type]: the script type.
  DECL_INT_ACCESSORS(type)

  // [line_ends]: FixedArray of line ends positions.
  DECL_ACCESSORS(line_ends, Object)

  // [eval_from_shared]: for eval scripts the shared function info for the
  // function from which eval was called.
  DECL_ACCESSORS(eval_from_shared, Object)

  // [eval_from_position]: the source position in the code for the function
  // from which eval was called, as positive integer. Or the code offset in the
  // code from which eval was called, as negative integer.
  DECL_INT_ACCESSORS(eval_from_position)

  // [shared_function_infos]: weak fixed array containing all shared
  // function infos created from this script.
  DECL_ACCESSORS(shared_function_infos, FixedArray)

  // [flags]: Holds an exciting bitfield.
  DECL_INT_ACCESSORS(flags)

  // [source_url]: sourceURL from magic comment
  DECL_ACCESSORS(source_url, Object)

  // [source_mapping_url]: sourceMappingURL magic comment
  DECL_ACCESSORS(source_mapping_url, Object)

  // [wasm_compiled_module]: the compiled wasm module this script belongs to.
  // This must only be called if the type of this script is TYPE_WASM.
  DECL_ACCESSORS(wasm_compiled_module, Object)

  // [compilation_type]: how the the script was compiled. Encoded in the
  // 'flags' field.
  inline CompilationType compilation_type();
  inline void set_compilation_type(CompilationType type);

  // [compilation_state]: determines whether the script has already been
  // compiled. Encoded in the 'flags' field.
  inline CompilationState compilation_state();
  inline void set_compilation_state(CompilationState state);

  // [origin_options]: optional attributes set by the embedder via ScriptOrigin,
  // and used by the embedder to make decisions about the script. V8 just passes
  // this through. Encoded in the 'flags' field.
  inline v8::ScriptOriginOptions origin_options();
  inline void set_origin_options(ScriptOriginOptions origin_options);

  DECLARE_CAST(Script)

  // If script source is an external string, check that the underlying
  // resource is accessible. Otherwise, always return true.
  inline bool HasValidSource();

  Object* GetNameOrSourceURL();

  // Set eval origin for stack trace formatting.
  static void SetEvalOrigin(Handle<Script> script,
                            Handle<SharedFunctionInfo> outer,
                            int eval_position);
  // Retrieve source position from where eval was called.
  int GetEvalPosition();

  // Init line_ends array with source code positions of line ends.
  static void InitLineEnds(Handle<Script> script);

  // Carries information about a source position.
  struct PositionInfo {
    PositionInfo() : line(-1), column(-1), line_start(-1), line_end(-1) {}

    int line;        // Zero-based line number.
    int column;      // Zero-based column number.
    int line_start;  // Position of first character in line.
    int line_end;    // Position of final linebreak character in line.
  };

  // Specifies whether to add offsets to position infos.
  enum OffsetFlag { NO_OFFSET = 0, WITH_OFFSET = 1 };

  // Retrieves information about the given position, optionally with an offset.
  // Returns false on failure, and otherwise writes into the given info object
  // on success.
  // The static method should is preferable for handlified callsites because it
  // initializes the line ends array, avoiding expensive recomputations.
  // The non-static version is not allocating and safe for unhandlified
  // callsites.
  static bool GetPositionInfo(Handle<Script> script, int position,
                              PositionInfo* info, OffsetFlag offset_flag);
  bool GetPositionInfo(int position, PositionInfo* info,
                       OffsetFlag offset_flag) const;

  // Wrappers for GetPositionInfo
  static int GetColumnNumber(Handle<Script> script, int code_offset);
  int GetColumnNumber(int code_pos) const;
  static int GetLineNumber(Handle<Script> script, int code_offset);
  int GetLineNumber(int code_pos) const;

  // Get the JS object wrapping the given script; create it if none exists.
  static Handle<JSObject> GetWrapper(Handle<Script> script);

  // Look through the list of existing shared function infos to find one
  // that matches the function literal.  Return empty handle if not found.
  MaybeHandle<SharedFunctionInfo> FindSharedFunctionInfo(Isolate* isolate,
                                                         FunctionLiteral* fun);

  // Iterate over all script objects on the heap.
  class Iterator {
   public:
    explicit Iterator(Isolate* isolate);
    Script* Next();

   private:
    WeakFixedArray::Iterator iterator_;
    DISALLOW_COPY_AND_ASSIGN(Iterator);
  };

  // Dispatched behavior.
  DECLARE_PRINTER(Script)
  DECLARE_VERIFIER(Script)

  static const int kSourceOffset = HeapObject::kHeaderSize;
  static const int kNameOffset = kSourceOffset + kPointerSize;
  static const int kLineOffsetOffset = kNameOffset + kPointerSize;
  static const int kColumnOffsetOffset = kLineOffsetOffset + kPointerSize;
  static const int kContextOffset = kColumnOffsetOffset + kPointerSize;
  static const int kWrapperOffset = kContextOffset + kPointerSize;
  static const int kTypeOffset = kWrapperOffset + kPointerSize;
  static const int kLineEndsOffset = kTypeOffset + kPointerSize;
  static const int kIdOffset = kLineEndsOffset + kPointerSize;
  static const int kEvalFromSharedOffset = kIdOffset + kPointerSize;
  static const int kEvalFromPositionOffset =
      kEvalFromSharedOffset + kPointerSize;
  static const int kSharedFunctionInfosOffset =
      kEvalFromPositionOffset + kPointerSize;
  static const int kFlagsOffset = kSharedFunctionInfosOffset + kPointerSize;
  static const int kSourceUrlOffset = kFlagsOffset + kPointerSize;
  static const int kSourceMappingUrlOffset = kSourceUrlOffset + kPointerSize;
  static const int kSize = kSourceMappingUrlOffset + kPointerSize;

 private:
  // Bit positions in the flags field.
  static const int kCompilationTypeBit = 0;
  static const int kCompilationStateBit = 1;
  static const int kOriginOptionsShift = 2;
  static const int kOriginOptionsSize = 3;
  static const int kOriginOptionsMask = ((1 << kOriginOptionsSize) - 1)
                                        << kOriginOptionsShift;

  DISALLOW_IMPLICIT_CONSTRUCTORS(Script);
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
  V(Function.prototype, call, FunctionCall)                 \
  V(Object.prototype, hasOwnProperty, ObjectHasOwnProperty) \
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
  V(Object, create, ObjectCreate)

#define ATOMIC_FUNCTIONS_WITH_ID_LIST(V) \
  V(Atomics, load, AtomicsLoad)          \
  V(Atomics, store, AtomicsStore)

enum BuiltinFunctionId {
  kArrayCode,
#define DECLARE_FUNCTION_ID(ignored1, ignore2, name)    \
  k##name,
  FUNCTIONS_WITH_ID_LIST(DECLARE_FUNCTION_ID)
      ATOMIC_FUNCTIONS_WITH_ID_LIST(DECLARE_FUNCTION_ID)
#undef DECLARE_FUNCTION_ID
  // Fake id for a special case of Math.pow. Note, it continues the
  // list of math functions.
  kMathPowHalf,
  // These are manually assigned to special getters during bootstrapping.
  kArrayBufferByteLength,
  kArrayEntries,
  kArrayKeys,
  kArrayValues,
  kArrayIteratorNext,
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
};

// Result of searching in an optimized code map of a SharedFunctionInfo. Note
// that both {code} and {literals} can be NULL to pass search result status.
struct CodeAndLiterals {
  Code* code;               // Cached optimized code.
  LiteralsArray* literals;  // Cached literals array.
};

// SharedFunctionInfo describes the JSFunction information that can be
// shared by multiple instances of the function.
class SharedFunctionInfo: public HeapObject {
 public:
  // [name]: Function name.
  DECL_ACCESSORS(name, Object)

  // [code]: Function code.
  DECL_ACCESSORS(code, Code)

  // Get the abstract code associated with the function, which will either be
  // a Code object or a BytecodeArray.
  inline AbstractCode* abstract_code();

  // Tells whether or not this shared function info is interpreted.
  //
  // Note: function->IsInterpreted() does not necessarily return the same value
  // as function->shared()->IsInterpreted() because the shared function info
  // could tier up to baseline via a different function closure. The interpreter
  // entry stub will "self-heal" this divergence when the function is executed.
  inline bool IsInterpreted() const;

  inline void ReplaceCode(Code* code);
  inline bool HasBaselineCode() const;

  // [optimized_code_map]: Map from native context to optimized code
  // and a shared literals array.
  DECL_ACCESSORS(optimized_code_map, FixedArray)

  // Returns entry from optimized code map for specified context and OSR entry.
  // Note that {code == nullptr, literals == nullptr} indicates no matching
  // entry has been found, whereas {code, literals == nullptr} indicates that
  // code is context-independent.
  CodeAndLiterals SearchOptimizedCodeMap(Context* native_context,
                                         BailoutId osr_ast_id);

  // Clear optimized code map.
  void ClearOptimizedCodeMap();

  // Like ClearOptimizedCodeMap, but preserves literals.
  void ClearCodeFromOptimizedCodeMap();

  // We have a special root FixedArray with the right shape and values
  // to represent the cleared optimized code map. This predicate checks
  // if that root is installed.
  inline bool OptimizedCodeMapIsCleared() const;

  // Removes a specific optimized code object from the optimized code map.
  // In case of non-OSR the code reference is cleared from the cache entry but
  // the entry itself is left in the map in order to proceed sharing literals.
  void EvictFromOptimizedCodeMap(Code* optimized_code, const char* reason);

  static Handle<LiteralsArray> FindOrCreateLiterals(
      Handle<SharedFunctionInfo> shared, Handle<Context> native_context);

  // Add or update entry in the optimized code map for context-dependent code.
  // If {code} is not given, then an existing entry's code won't be overwritten.
  static void AddToOptimizedCodeMap(Handle<SharedFunctionInfo> shared,
                                    Handle<Context> native_context,
                                    MaybeHandle<Code> code,
                                    Handle<LiteralsArray> literals,
                                    BailoutId osr_ast_id);

  // Set up the link between shared function info and the script. The shared
  // function info is added to the list on the script.
  V8_EXPORT_PRIVATE static void SetScript(Handle<SharedFunctionInfo> shared,
                                          Handle<Object> script_object);

  // Layout description of the optimized code map.
  static const int kEntriesStart = 0;
  static const int kContextOffset = 0;
  static const int kCachedCodeOffset = 1;
  static const int kLiteralsOffset = 2;
  static const int kEntryLength = 3;
  static const int kInitialLength = kEntriesStart + kEntryLength;

  static const int kNotFound = -1;

  // Helpers for assembly code that does a backwards walk of the optimized code
  // map.
  static const int kOffsetToPreviousContext =
      FixedArray::kHeaderSize + kPointerSize * (kContextOffset - kEntryLength);
  static const int kOffsetToPreviousCachedCode =
      FixedArray::kHeaderSize +
      kPointerSize * (kCachedCodeOffset - kEntryLength);
  static const int kOffsetToPreviousLiterals =
      FixedArray::kHeaderSize + kPointerSize * (kLiteralsOffset - kEntryLength);

  // [scope_info]: Scope info.
  DECL_ACCESSORS(scope_info, ScopeInfo)

  // The outer scope info for the purpose of parsing this function, or the hole
  // value if it isn't yet known.
  DECL_ACCESSORS(outer_scope_info, HeapObject)

  // [construct stub]: Code stub for constructing instances of this function.
  DECL_ACCESSORS(construct_stub, Code)

  // Sets the given code as the construct stub, and marks builtin code objects
  // as a construct stub.
  void SetConstructStub(Code* code);

  // Returns if this function has been compiled to native code yet.
  inline bool is_compiled() const;

  // [length]: The function length - usually the number of declared parameters.
  // Use up to 2^30 parameters.
  inline int length() const;
  inline void set_length(int value);

  // [internal formal parameter count]: The declared number of parameters.
  // For subclass constructors, also includes new.target.
  // The size of function's frame is internal_formal_parameter_count + 1.
  inline int internal_formal_parameter_count() const;
  inline void set_internal_formal_parameter_count(int value);

  // Set the formal parameter count so the function code will be
  // called without using argument adaptor frames.
  inline void DontAdaptArguments();

  // [expected_nof_properties]: Expected number of properties for the function.
  inline int expected_nof_properties() const;
  inline void set_expected_nof_properties(int value);

  // [feedback_metadata] - describes ast node feedback from full-codegen and
  // (increasingly) from crankshafted code where sufficient feedback isn't
  // available.
  DECL_ACCESSORS(feedback_metadata, FeedbackMetadata)

  // [function_literal_id] - uniquely identifies the FunctionLiteral this
  // SharedFunctionInfo represents within its script, or -1 if this
  // SharedFunctionInfo object doesn't correspond to a parsed FunctionLiteral.
  inline int function_literal_id() const;
  inline void set_function_literal_id(int value);

#if TRACE_MAPS
  // [unique_id] - For --trace-maps purposes, an identifier that's persistent
  // even if the GC moves this SharedFunctionInfo.
  inline int unique_id() const;
  inline void set_unique_id(int value);
#endif

  // [instance class name]: class name for instances.
  DECL_ACCESSORS(instance_class_name, Object)

  // [function data]: This field holds some additional data for function.
  // Currently it has one of:
  //  - a FunctionTemplateInfo to make benefit the API [IsApiFunction()].
  //  - a BytecodeArray for the interpreter [HasBytecodeArray()].
  //  - a FixedArray with Asm->Wasm conversion [HasAsmWasmData()].
  DECL_ACCESSORS(function_data, Object)

  inline bool IsApiFunction();
  inline FunctionTemplateInfo* get_api_func_data();
  inline void set_api_func_data(FunctionTemplateInfo* data);
  inline bool HasBytecodeArray();
  inline BytecodeArray* bytecode_array();
  inline void set_bytecode_array(BytecodeArray* bytecode);
  inline void ClearBytecodeArray();
  inline bool HasAsmWasmData();
  inline FixedArray* asm_wasm_data();
  inline void set_asm_wasm_data(FixedArray* data);
  inline void ClearAsmWasmData();

  // [function identifier]: This field holds an additional identifier for the
  // function.
  //  - a Smi identifying a builtin function [HasBuiltinFunctionId()].
  //  - a String identifying the function's inferred name [HasInferredName()].
  // The inferred_name is inferred from variable or property
  // assignment of this function. It is used to facilitate debugging and
  // profiling of JavaScript code written in OO style, where almost
  // all functions are anonymous but are assigned to object
  // properties.
  DECL_ACCESSORS(function_identifier, Object)

  inline bool HasBuiltinFunctionId();
  inline BuiltinFunctionId builtin_function_id();
  inline void set_builtin_function_id(BuiltinFunctionId id);
  inline bool HasInferredName();
  inline String* inferred_name();
  inline void set_inferred_name(String* inferred_name);

  // [script]: Script from which the function originates.
  DECL_ACCESSORS(script, Object)

  // [num_literals]: Number of literals used by this function.
  inline int num_literals() const;
  inline void set_num_literals(int value);

  // [start_position_and_type]: Field used to store both the source code
  // position, whether or not the function is a function expression,
  // and whether or not the function is a toplevel function. The two
  // least significants bit indicates whether the function is an
  // expression and the rest contains the source code position.
  inline int start_position_and_type() const;
  inline void set_start_position_and_type(int value);

  // The function is subject to debugging if a debug info is attached.
  inline bool HasDebugInfo();
  inline DebugInfo* GetDebugInfo();

  // A function has debug code if the compiled code has debug break slots.
  inline bool HasDebugCode();

  // [debug info]: Debug information.
  DECL_ACCESSORS(debug_info, Object)

  // The function's name if it is non-empty, otherwise the inferred name.
  String* DebugName();

  // The function cannot cause any side effects.
  bool HasNoSideEffect();

  // Used for flags such as --hydrogen-filter.
  bool PassesFilter(const char* raw_filter);

  // Position of the 'function' token in the script source.
  inline int function_token_position() const;
  inline void set_function_token_position(int function_token_position);

  // Position of this function in the script source.
  inline int start_position() const;
  inline void set_start_position(int start_position);

  // End position of this function in the script source.
  inline int end_position() const;
  inline void set_end_position(int end_position);

  // Is this function a named function expression in the source code.
  DECL_BOOLEAN_ACCESSORS(is_named_expression)

  // Is this function a top-level function (scripts, evals).
  DECL_BOOLEAN_ACCESSORS(is_toplevel)

  // Bit field containing various information collected by the compiler to
  // drive optimization.
  inline int compiler_hints() const;
  inline void set_compiler_hints(int value);

  inline int ast_node_count() const;
  inline void set_ast_node_count(int count);

  inline int profiler_ticks() const;
  inline void set_profiler_ticks(int ticks);

  // Inline cache age is used to infer whether the function survived a context
  // disposal or not. In the former case we reset the opt_count.
  inline int ic_age();
  inline void set_ic_age(int age);

  // Indicates if this function can be lazy compiled.
  // This is used to determine if we can safely flush code from a function
  // when doing GC if we expect that the function will no longer be used.
  DECL_BOOLEAN_ACCESSORS(allows_lazy_compilation)

  // Indicates whether optimizations have been disabled for this
  // shared function info. If a function is repeatedly optimized or if
  // we cannot optimize the function we disable optimization to avoid
  // spending time attempting to optimize it again.
  DECL_BOOLEAN_ACCESSORS(optimization_disabled)

  // Indicates the language mode.
  inline LanguageMode language_mode();
  inline void set_language_mode(LanguageMode language_mode);

  // False if the function definitely does not allocate an arguments object.
  DECL_BOOLEAN_ACCESSORS(uses_arguments)

  // Indicates that this function uses a super property (or an eval that may
  // use a super property).
  // This is needed to set up the [[HomeObject]] on the function instance.
  DECL_BOOLEAN_ACCESSORS(needs_home_object)

  // True if the function has any duplicated parameter names.
  DECL_BOOLEAN_ACCESSORS(has_duplicate_parameters)

  // Indicates whether the function is a native function.
  // These needs special treatment in .call and .apply since
  // null passed as the receiver should not be translated to the
  // global object.
  DECL_BOOLEAN_ACCESSORS(native)

  // Indicate that this function should always be inlined in optimized code.
  DECL_BOOLEAN_ACCESSORS(force_inline)

  // Indicates that the function was created by the Function function.
  // Though it's anonymous, toString should treat it as if it had the name
  // "anonymous".  We don't set the name itself so that the system does not
  // see a binding for it.
  DECL_BOOLEAN_ACCESSORS(name_should_print_as_anonymous)

  // Indicates that the function is either an anonymous expression
  // or an arrow function (the name field can be set through the API,
  // which does not change this flag).
  DECL_BOOLEAN_ACCESSORS(is_anonymous_expression)

  // Indicates that code for this function must be compiled through the
  // Ignition / TurboFan pipeline, and is unsupported by
  // FullCodegen / Crankshaft.
  DECL_BOOLEAN_ACCESSORS(must_use_ignition_turbo)

  // Indicates that code for this function cannot be flushed.
  DECL_BOOLEAN_ACCESSORS(dont_flush)

  // Indicates that this function is an asm function.
  DECL_BOOLEAN_ACCESSORS(asm_function)

  // Indicates that the the shared function info is deserialized from cache.
  DECL_BOOLEAN_ACCESSORS(deserialized)

  // Whether this function was created from a FunctionDeclaration.
  DECL_BOOLEAN_ACCESSORS(is_declaration)

  // Whether this function was marked to be tiered up.
  DECL_BOOLEAN_ACCESSORS(marked_for_tier_up)

  // Indicates that asm->wasm conversion failed and should not be re-attempted.
  DECL_BOOLEAN_ACCESSORS(is_asm_wasm_broken)

  // Indicates that the function cannot cause side-effects.
  DECL_BOOLEAN_ACCESSORS(has_no_side_effect)

  // Indicates that |has_no_side_effect| has been computed and set.
  DECL_BOOLEAN_ACCESSORS(computed_has_no_side_effect)

  inline FunctionKind kind() const;
  inline void set_kind(FunctionKind kind);

  // Indicates whether or not the code in the shared function support
  // deoptimization.
  inline bool has_deoptimization_support();

  // Enable deoptimization support through recompiled code.
  void EnableDeoptimizationSupport(Code* recompiled);

  // Disable (further) attempted optimization of all functions sharing this
  // shared function info.
  void DisableOptimization(BailoutReason reason);

  inline BailoutReason disable_optimization_reason();

  // Lookup the bailout ID and DCHECK that it exists in the non-optimized
  // code, returns whether it asserted (i.e., always true if assertions are
  // disabled).
  bool VerifyBailoutId(BailoutId id);

  // [source code]: Source code for the function.
  bool HasSourceCode() const;
  Handle<Object> GetSourceCode();

  // Number of times the function was optimized.
  inline int opt_count();
  inline void set_opt_count(int opt_count);

  // Number of times the function was deoptimized.
  inline void set_deopt_count(int value);
  inline int deopt_count();
  inline void increment_deopt_count();

  // Number of time we tried to re-enable optimization after it
  // was disabled due to high number of deoptimizations.
  inline void set_opt_reenable_tries(int value);
  inline int opt_reenable_tries();

  inline void TryReenableOptimization();

  // Stores deopt_count, opt_reenable_tries and ic_age as bit-fields.
  inline void set_counters(int value);
  inline int counters() const;

  // Stores opt_count and bailout_reason as bit-fields.
  inline void set_opt_count_and_bailout_reason(int value);
  inline int opt_count_and_bailout_reason() const;

  inline void set_disable_optimization_reason(BailoutReason reason);

  // Tells whether this function should be subject to debugging.
  inline bool IsSubjectToDebugging();

  // Whether this function is defined in user-provided JavaScript code.
  inline bool IsUserJavaScript();

  // Check whether or not this function is inlineable.
  bool IsInlineable();

  // Source size of this function.
  int SourceSize();

  // Returns `false` if formal parameters include rest parameters, optional
  // parameters, or destructuring parameters.
  // TODO(caitp): make this a flag set during parsing
  inline bool has_simple_parameters();

  // Initialize a SharedFunctionInfo from a parsed function literal.
  static void InitFromFunctionLiteral(Handle<SharedFunctionInfo> shared_info,
                                      FunctionLiteral* lit);

  // Dispatched behavior.
  DECLARE_PRINTER(SharedFunctionInfo)
  DECLARE_VERIFIER(SharedFunctionInfo)

  void ResetForNewContext(int new_ic_age);

  // Iterate over all shared function infos in a given script.
  class ScriptIterator {
   public:
    explicit ScriptIterator(Handle<Script> script);
    ScriptIterator(Isolate* isolate, Handle<FixedArray> shared_function_infos);
    SharedFunctionInfo* Next();

    // Reset the iterator to run on |script|.
    void Reset(Handle<Script> script);

   private:
    Isolate* isolate_;
    Handle<FixedArray> shared_function_infos_;
    int index_;
    DISALLOW_COPY_AND_ASSIGN(ScriptIterator);
  };

  // Iterate over all shared function infos on the heap.
  class GlobalIterator {
   public:
    explicit GlobalIterator(Isolate* isolate);
    SharedFunctionInfo* Next();

   private:
    Script::Iterator script_iterator_;
    WeakFixedArray::Iterator noscript_sfi_iterator_;
    SharedFunctionInfo::ScriptIterator sfi_iterator_;
    DisallowHeapAllocation no_gc_;
    DISALLOW_COPY_AND_ASSIGN(GlobalIterator);
  };

  DECLARE_CAST(SharedFunctionInfo)

  // Constants.
  static const int kDontAdaptArgumentsSentinel = -1;

  // Layout description.
  // Pointer fields.
  static const int kCodeOffset = HeapObject::kHeaderSize;
  static const int kNameOffset = kCodeOffset + kPointerSize;
  static const int kOptimizedCodeMapOffset = kNameOffset + kPointerSize;
  static const int kScopeInfoOffset = kOptimizedCodeMapOffset + kPointerSize;
  static const int kOuterScopeInfoOffset = kScopeInfoOffset + kPointerSize;
  static const int kConstructStubOffset = kOuterScopeInfoOffset + kPointerSize;
  static const int kInstanceClassNameOffset =
      kConstructStubOffset + kPointerSize;
  static const int kFunctionDataOffset =
      kInstanceClassNameOffset + kPointerSize;
  static const int kScriptOffset = kFunctionDataOffset + kPointerSize;
  static const int kDebugInfoOffset = kScriptOffset + kPointerSize;
  static const int kFunctionIdentifierOffset = kDebugInfoOffset + kPointerSize;
  static const int kFeedbackMetadataOffset =
      kFunctionIdentifierOffset + kPointerSize;
  static const int kFunctionLiteralIdOffset =
      kFeedbackMetadataOffset + kPointerSize;
#if TRACE_MAPS
  static const int kUniqueIdOffset = kFunctionLiteralIdOffset + kPointerSize;
  static const int kLastPointerFieldOffset = kUniqueIdOffset;
#else
  // Just to not break the postmortrem support with conditional offsets
  static const int kUniqueIdOffset = kFunctionLiteralIdOffset;
  static const int kLastPointerFieldOffset = kFunctionLiteralIdOffset;
#endif

#if V8_HOST_ARCH_32_BIT
  // Smi fields.
  static const int kLengthOffset = kLastPointerFieldOffset + kPointerSize;
  static const int kFormalParameterCountOffset = kLengthOffset + kPointerSize;
  static const int kExpectedNofPropertiesOffset =
      kFormalParameterCountOffset + kPointerSize;
  static const int kNumLiteralsOffset =
      kExpectedNofPropertiesOffset + kPointerSize;
  static const int kStartPositionAndTypeOffset =
      kNumLiteralsOffset + kPointerSize;
  static const int kEndPositionOffset =
      kStartPositionAndTypeOffset + kPointerSize;
  static const int kFunctionTokenPositionOffset =
      kEndPositionOffset + kPointerSize;
  static const int kCompilerHintsOffset =
      kFunctionTokenPositionOffset + kPointerSize;
  static const int kOptCountAndBailoutReasonOffset =
      kCompilerHintsOffset + kPointerSize;
  static const int kCountersOffset =
      kOptCountAndBailoutReasonOffset + kPointerSize;
  static const int kAstNodeCountOffset =
      kCountersOffset + kPointerSize;
  static const int kProfilerTicksOffset =
      kAstNodeCountOffset + kPointerSize;

  // Total size.
  static const int kSize = kProfilerTicksOffset + kPointerSize;
#else
// The only reason to use smi fields instead of int fields is to allow
// iteration without maps decoding during garbage collections.
// To avoid wasting space on 64-bit architectures we use the following trick:
// we group integer fields into pairs
// The least significant integer in each pair is shifted left by 1.  By doing
// this we guarantee that LSB of each kPointerSize aligned word is not set and
// thus this word cannot be treated as pointer to HeapObject during old space
// traversal.
#if V8_TARGET_LITTLE_ENDIAN
  static const int kLengthOffset = kLastPointerFieldOffset + kPointerSize;
  static const int kFormalParameterCountOffset =
      kLengthOffset + kIntSize;

  static const int kExpectedNofPropertiesOffset =
      kFormalParameterCountOffset + kIntSize;
  static const int kNumLiteralsOffset =
      kExpectedNofPropertiesOffset + kIntSize;

  static const int kEndPositionOffset =
      kNumLiteralsOffset + kIntSize;
  static const int kStartPositionAndTypeOffset =
      kEndPositionOffset + kIntSize;

  static const int kFunctionTokenPositionOffset =
      kStartPositionAndTypeOffset + kIntSize;
  static const int kCompilerHintsOffset =
      kFunctionTokenPositionOffset + kIntSize;

  static const int kOptCountAndBailoutReasonOffset =
      kCompilerHintsOffset + kIntSize;
  static const int kCountersOffset =
      kOptCountAndBailoutReasonOffset + kIntSize;

  static const int kAstNodeCountOffset =
      kCountersOffset + kIntSize;
  static const int kProfilerTicksOffset =
      kAstNodeCountOffset + kIntSize;

  // Total size.
  static const int kSize = kProfilerTicksOffset + kIntSize;

#elif V8_TARGET_BIG_ENDIAN
  static const int kFormalParameterCountOffset =
      kLastPointerFieldOffset + kPointerSize;
  static const int kLengthOffset = kFormalParameterCountOffset + kIntSize;

  static const int kNumLiteralsOffset = kLengthOffset + kIntSize;
  static const int kExpectedNofPropertiesOffset = kNumLiteralsOffset + kIntSize;

  static const int kStartPositionAndTypeOffset =
      kExpectedNofPropertiesOffset + kIntSize;
  static const int kEndPositionOffset = kStartPositionAndTypeOffset + kIntSize;

  static const int kCompilerHintsOffset = kEndPositionOffset + kIntSize;
  static const int kFunctionTokenPositionOffset =
      kCompilerHintsOffset + kIntSize;

  static const int kCountersOffset = kFunctionTokenPositionOffset + kIntSize;
  static const int kOptCountAndBailoutReasonOffset = kCountersOffset + kIntSize;

  static const int kProfilerTicksOffset =
      kOptCountAndBailoutReasonOffset + kIntSize;
  static const int kAstNodeCountOffset = kProfilerTicksOffset + kIntSize;

  // Total size.
  static const int kSize = kAstNodeCountOffset + kIntSize;

#else
#error Unknown byte ordering
#endif  // Big endian
#endif  // 64-bit


  static const int kAlignedSize = POINTER_SIZE_ALIGN(kSize);

  typedef FixedBodyDescriptor<kCodeOffset,
                              kLastPointerFieldOffset + kPointerSize, kSize>
      BodyDescriptor;
  typedef FixedBodyDescriptor<kNameOffset,
                              kLastPointerFieldOffset + kPointerSize, kSize>
      BodyDescriptorWeakCode;

  // Bit positions in start_position_and_type.
  // The source code start position is in the 30 most significant bits of
  // the start_position_and_type field.
  static const int kIsNamedExpressionBit = 0;
  static const int kIsTopLevelBit = 1;
  static const int kStartPositionShift = 2;
  static const int kStartPositionMask = ~((1 << kStartPositionShift) - 1);

  // Bit positions in compiler_hints.
  enum CompilerHints {
    // byte 0
    kAllowLazyCompilation,
    kMarkedForTierUp,
    kOptimizationDisabled,
    kHasDuplicateParameters,
    kNative,
    kStrictModeFunction,
    kUsesArguments,
    kNeedsHomeObject,
    // byte 1
    kForceInline,
    kIsAsmFunction,
    kIsAnonymousExpression,
    kNameShouldPrintAsAnonymous,
    kMustUseIgnitionTurbo,
    kDontFlush,
    kIsDeclaration,

    kUnused,  // unused.
    // byte 2
    kFunctionKind,
    // rest of byte 2 and first two bits of byte 3 are used by FunctionKind
    // byte 3
    kDeserialized = kFunctionKind + 10,
    kIsAsmWasmBroken,
    kHasNoSideEffect,
    kComputedHasNoSideEffect,
    kCompilerHintsCount,  // Pseudo entry
  };
  // kFunctionKind has to be byte-aligned
  STATIC_ASSERT((kFunctionKind % kBitsPerByte) == 0);

  class FunctionKindBits : public BitField<FunctionKind, kFunctionKind, 10> {};

  class DeoptCountBits : public BitField<int, 0, 4> {};
  class OptReenableTriesBits : public BitField<int, 4, 18> {};
  class ICAgeBits : public BitField<int, 22, 8> {};

  class OptCountBits : public BitField<int, 0, 22> {};
  class DisabledOptimizationReasonBits : public BitField<int, 22, 8> {};

 private:
#if V8_HOST_ARCH_32_BIT
  // On 32 bit platforms, compiler hints is a smi.
  static const int kCompilerHintsSmiTagSize = kSmiTagSize;
  static const int kCompilerHintsSize = kPointerSize;
#else
  // On 64 bit platforms, compiler hints is not a smi, see comment above.
  static const int kCompilerHintsSmiTagSize = 0;
  static const int kCompilerHintsSize = kIntSize;
#endif

  STATIC_ASSERT(SharedFunctionInfo::kCompilerHintsCount +
                    SharedFunctionInfo::kCompilerHintsSmiTagSize <=
                SharedFunctionInfo::kCompilerHintsSize * kBitsPerByte);

 public:
  // Constants for optimizing codegen for strict mode function and
  // native tests when using integer-width instructions.
  static const int kStrictModeBit =
      kStrictModeFunction + kCompilerHintsSmiTagSize;
  static const int kNativeBit = kNative + kCompilerHintsSmiTagSize;
  static const int kHasDuplicateParametersBit =
      kHasDuplicateParameters + kCompilerHintsSmiTagSize;

  static const int kFunctionKindShift =
      kFunctionKind + kCompilerHintsSmiTagSize;
  static const int kAllFunctionKindBitsMask = FunctionKindBits::kMask
                                              << kCompilerHintsSmiTagSize;

  static const int kMarkedForTierUpBit =
      kMarkedForTierUp + kCompilerHintsSmiTagSize;

  // Constants for optimizing codegen for strict mode function and
  // native tests.
  // Allows to use byte-width instructions.
  static const int kStrictModeBitWithinByte = kStrictModeBit % kBitsPerByte;
  static const int kNativeBitWithinByte = kNativeBit % kBitsPerByte;
  static const int kHasDuplicateParametersBitWithinByte =
      kHasDuplicateParametersBit % kBitsPerByte;

  static const int kClassConstructorBitsWithinByte =
      FunctionKind::kClassConstructor << kCompilerHintsSmiTagSize;
  STATIC_ASSERT(kClassConstructorBitsWithinByte < (1 << kBitsPerByte));

  static const int kMarkedForTierUpBitWithinByte =
      kMarkedForTierUpBit % kBitsPerByte;

#if defined(V8_TARGET_LITTLE_ENDIAN)
#define BYTE_OFFSET(compiler_hint) \
  kCompilerHintsOffset +           \
      (compiler_hint + kCompilerHintsSmiTagSize) / kBitsPerByte
#elif defined(V8_TARGET_BIG_ENDIAN)
#define BYTE_OFFSET(compiler_hint)                  \
  kCompilerHintsOffset + (kCompilerHintsSize - 1) - \
      ((compiler_hint + kCompilerHintsSmiTagSize) / kBitsPerByte)
#else
#error Unknown byte ordering
#endif
  static const int kStrictModeByteOffset = BYTE_OFFSET(kStrictModeFunction);
  static const int kNativeByteOffset = BYTE_OFFSET(kNative);
  static const int kFunctionKindByteOffset = BYTE_OFFSET(kFunctionKind);
  static const int kHasDuplicateParametersByteOffset =
      BYTE_OFFSET(kHasDuplicateParameters);
  static const int kMarkedForTierUpByteOffset = BYTE_OFFSET(kMarkedForTierUp);
#undef BYTE_OFFSET

 private:
  // Returns entry from optimized code map for specified context.
  // The result is either kNotFound, or a start index of the context-dependent
  // entry.
  int SearchOptimizedCodeMapEntry(Context* native_context);

  DISALLOW_IMPLICIT_CONSTRUCTORS(SharedFunctionInfo);
};


// Printing support.
struct SourceCodeOf {
  explicit SourceCodeOf(SharedFunctionInfo* v, int max = -1)
      : value(v), max_length(max) {}
  const SharedFunctionInfo* value;
  int max_length;
};


std::ostream& operator<<(std::ostream& os, const SourceCodeOf& v);


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

  DECLARE_CAST(JSGeneratorObject)

  // Dispatched behavior.
  DECLARE_VERIFIER(JSGeneratorObject)

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

// When importing a module namespace (import * as foo from "bar"), a
// JSModuleNamespace object (representing module "bar") is created and bound to
// the declared variable (foo).  A module can have at most one namespace object.
class JSModuleNamespace : public JSObject {
 public:
  DECLARE_CAST(JSModuleNamespace)
  DECLARE_PRINTER(JSModuleNamespace)
  DECLARE_VERIFIER(JSModuleNamespace)

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
  DECLARE_CAST(Module)
  DECLARE_VERIFIER(Module)
  DECLARE_PRINTER(Module)

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

  // The namespace object (or undefined).
  DECL_ACCESSORS(module_namespace, HeapObject)

  // Modules imported or re-exported by this module.
  // Corresponds 1-to-1 to the module specifier strings in
  // ModuleInfo::module_requests.
  DECL_ACCESSORS(requested_modules, FixedArray)

  // Get the ModuleInfo associated with the code.
  inline ModuleInfo* info() const;

  inline bool instantiated() const;
  inline bool evaluated() const;
  inline void set_evaluated();

  // Implementation of spec operation ModuleDeclarationInstantiation.
  // Returns false if an exception occurred during instantiation, true
  // otherwise.
  static MUST_USE_RESULT bool Instantiate(Handle<Module> module,
                                          v8::Local<v8::Context> context,
                                          v8::Module::ResolveCallback callback);

  // Implementation of spec operation ModuleEvaluation.
  static MUST_USE_RESULT MaybeHandle<Object> Evaluate(Handle<Module> module);

  static Handle<Object> LoadVariable(Handle<Module> module, int cell_index);
  static void StoreVariable(Handle<Module> module, int cell_index,
                            Handle<Object> value);

  // Get the namespace object for [module_request] of [module].  If it doesn't
  // exist yet, it is created.
  static Handle<JSModuleNamespace> GetModuleNamespace(Handle<Module> module,
                                                      int module_request);

  static const int kCodeOffset = HeapObject::kHeaderSize;
  static const int kExportsOffset = kCodeOffset + kPointerSize;
  static const int kRegularExportsOffset = kExportsOffset + kPointerSize;
  static const int kRegularImportsOffset = kRegularExportsOffset + kPointerSize;
  static const int kHashOffset = kRegularImportsOffset + kPointerSize;
  static const int kModuleNamespaceOffset = kHashOffset + kPointerSize;
  static const int kRequestedModulesOffset =
      kModuleNamespaceOffset + kPointerSize;
  static const int kSize = kRequestedModulesOffset + kPointerSize;

 private:
  static void CreateExport(Handle<Module> module, int cell_index,
                           Handle<FixedArray> names);
  static void CreateIndirectExport(Handle<Module> module, Handle<String> name,
                                   Handle<ModuleInfoEntry> entry);

  // Get the namespace object for [module].  If it doesn't exist yet, it is
  // created.
  static Handle<JSModuleNamespace> GetModuleNamespace(Handle<Module> module);

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

  DISALLOW_IMPLICIT_CONSTRUCTORS(Module);
};

// JSBoundFunction describes a bound function exotic object.
class JSBoundFunction : public JSObject {
 public:
  // [bound_target_function]: The wrapped function object.
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

  DECLARE_CAST(JSBoundFunction)

  // Dispatched behavior.
  DECLARE_PRINTER(JSBoundFunction)
  DECLARE_VERIFIER(JSBoundFunction)

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
  // as function->shared()->IsInterpreted() because the shared function info
  // could tier up to baseline via a different function closure. The interpreter
  // entry stub will "self-heal" this divergence when the function is executed.
  inline bool IsInterpreted();

  // Tells whether or not this function has been optimized.
  inline bool IsOptimized();

  // Mark this function for lazy recompilation. The function will be recompiled
  // the next time it is executed.
  void MarkForBaseline();
  void MarkForOptimization();
  void AttemptConcurrentOptimization();

  // Tells whether or not the function is already marked for lazy recompilation.
  inline bool IsMarkedForBaseline();
  inline bool IsMarkedForOptimization();
  inline bool IsMarkedForConcurrentOptimization();

  // Tells whether or not the function is on the concurrent recompilation queue.
  inline bool IsInOptimizationQueue();

  // Completes inobject slack tracking on initial map if it is active.
  inline void CompleteInobjectSlackTrackingIfActive();

  // [literals]: Fixed array holding the materialized literals.
  //
  // If the function contains object, regexp or array literals, the
  // literals array prefix contains the object, regexp, and array
  // function to be used when creating these literals.  This is
  // necessary so that we do not dynamically lookup the object, regexp
  // or array functions.  Performing a dynamic lookup, we might end up
  // using the functions from a new context that we should not have
  // access to. For API objects we store the boilerplate in the literal array.
  DECL_ACCESSORS(literals, LiteralsArray)

  static void EnsureLiterals(Handle<JSFunction> function);
  inline FeedbackVector* feedback_vector();

  // Unconditionally clear the type feedback vector (including vector ICs).
  void ClearTypeFeedbackInfo();

  // Clear the type feedback vector with a more subtle policy at GC time.
  void ClearTypeFeedbackInfoAtGCTime();

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
  static void SetInstancePrototype(Handle<JSFunction> function,
                                   Handle<Object> value);

  // After prototype is removed, it will not be created when accessed, and
  // [[Construct]] from this function will not be allowed.
  bool RemovePrototype();

  // Returns if this function has been compiled to native code yet.
  inline bool is_compiled();

  // [next_function_link]: Links functions into various lists, e.g. the list
  // of optimized functions hanging off the native_context. The CodeFlusher
  // uses this link to chain together flushing candidates. Treated weakly
  // by the garbage collector.
  DECL_ACCESSORS(next_function_link, Object)

  // Prints the name of the function using PrintF.
  void PrintName(FILE* out = stdout);

  DECLARE_CAST(JSFunction)

  // Calculate the instance size and in-object properties count.
  void CalculateInstanceSize(InstanceType instance_type,
                             int requested_internal_fields, int* instance_size,
                             int* in_object_properties);
  void CalculateInstanceSizeForDerivedClass(InstanceType instance_type,
                                            int requested_internal_fields,
                                            int* instance_size,
                                            int* in_object_properties);
  static void CalculateInstanceSizeHelper(InstanceType instance_type,
                                          int requested_internal_fields,
                                          int requested_in_object_properties,
                                          int* instance_size,
                                          int* in_object_properties);
  // Visiting policy flags define whether the code entry or next function
  // should be visited or not.
  enum BodyVisitingPolicy {
    kVisitCodeEntry = 1 << 0,
    kVisitNextFunction = 1 << 1,

    kSkipCodeEntryAndNextFunction = 0,
    kVisitCodeEntryAndNextFunction = kVisitCodeEntry | kVisitNextFunction
  };
  // Iterates the function object according to the visiting policy.
  template <BodyVisitingPolicy>
  class BodyDescriptorImpl;

  // Visit the whole object.
  typedef BodyDescriptorImpl<kVisitCodeEntryAndNextFunction> BodyDescriptor;

  // Don't visit next function.
  typedef BodyDescriptorImpl<kVisitCodeEntry> BodyDescriptorStrongCode;
  typedef BodyDescriptorImpl<kSkipCodeEntryAndNextFunction>
      BodyDescriptorWeakCode;

  // Dispatched behavior.
  DECLARE_PRINTER(JSFunction)
  DECLARE_VERIFIER(JSFunction)

  // The function's name if it is configured, otherwise shared function info
  // debug name.
  static Handle<String> GetName(Handle<JSFunction> function);

  // ES6 section 9.2.11 SetFunctionName
  // Because of the way this abstract operation is used in the spec,
  // it should never fail.
  static void SetName(Handle<JSFunction> function, Handle<Name> name,
                      Handle<String> prefix);

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
  static const int kLiteralsOffset = kContextOffset + kPointerSize;
  static const int kNonWeakFieldsEndOffset = kLiteralsOffset + kPointerSize;
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

  DECLARE_CAST(JSGlobalProxy)

  inline bool IsDetachedFrom(JSGlobalObject* global) const;

  static int SizeWithInternalFields(int internal_field_count);

  // Dispatched behavior.
  DECLARE_PRINTER(JSGlobalProxy)
  DECLARE_VERIFIER(JSGlobalProxy)

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


  static void InvalidatePropertyCell(Handle<JSGlobalObject> object,
                                     Handle<Name> name);
  // Ensure that the global object has a cell for the given property name.
  static Handle<PropertyCell> EnsureEmptyPropertyCell(
      Handle<JSGlobalObject> global, Handle<Name> name,
      PropertyCellType cell_type, int* entry_out = nullptr);

  DECLARE_CAST(JSGlobalObject)

  inline bool IsDetached();

  // Dispatched behavior.
  DECLARE_PRINTER(JSGlobalObject)
  DECLARE_VERIFIER(JSGlobalObject)

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

  DECLARE_CAST(JSValue)

  // Dispatched behavior.
  DECLARE_PRINTER(JSValue)
  DECLARE_VERIFIER(JSValue)

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

  DECLARE_CAST(JSDate)

  // Returns the time value (UTC) identifying the current time.
  static double CurrentTimeValue(Isolate* isolate);

  // Returns the date field with the specified index.
  // See FieldIndex for the list of date fields.
  static Object* GetField(Object* date, Smi* index);

  static Handle<Object> SetValue(Handle<JSDate> date, double v);

  void SetValue(Object* value, bool is_value_nan);

  // Dispatched behavior.
  DECLARE_PRINTER(JSDate)
  DECLARE_VERIFIER(JSDate)

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

  DECLARE_CAST(JSMessageObject)

  // Dispatched behavior.
  DECLARE_PRINTER(JSMessageObject)
  DECLARE_VERIFIER(JSMessageObject)

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
};

class JSPromise;

// TODO(caitp): Make this a Struct once properties are no longer accessed from
// JS
class JSPromiseCapability : public JSObject {
 public:
  DECLARE_CAST(JSPromiseCapability)

  DECLARE_VERIFIER(JSPromiseCapability)

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
  // 2) Object -- There is a single Callable directly attached to the
  // fulfill_reactions, reject_reactions and the deferred fields are
  // directly attached to the slots. In this state, deferred_promise
  // is a JSReceiver and deferred_on_{resolve, reject} are Callables.
  //
  // 3) FixedArray -- There is more than one callback and deferred
  // fields attached to a FixedArray.
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

  DECLARE_CAST(JSPromise)

  // Dispatched behavior.
  DECLARE_PRINTER(JSPromise)
  DECLARE_VERIFIER(JSPromise)

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
// used for tracking the last usage (used for code flushing).
// - a reference to code for UC16 inputs (bytecode or compiled), or a smi
// used for tracking the last usage (used for code flushing)..
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
  };
  typedef base::Flags<Flag> Flags;

  DECL_ACCESSORS(data, Object)
  DECL_ACCESSORS(flags, Object)
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
  inline int CaptureCount();
  inline Flags GetFlags();
  inline String* Pattern();
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

  static int saved_code_index(bool is_latin1) {
    if (is_latin1) {
      return kIrregexpLatin1CodeSavedIndex;
    } else {
      return kIrregexpUC16CodeSavedIndex;
    }
  }

  DECLARE_CAST(JSRegExp)

  // Dispatched behavior.
  DECLARE_PRINTER(JSRegExp)
  DECLARE_VERIFIER(JSRegExp)

  static const int kDataOffset = JSObject::kHeaderSize;
  static const int kSourceOffset = kDataOffset + kPointerSize;
  static const int kFlagsOffset = kSourceOffset + kPointerSize;
  static const int kSize = kFlagsOffset + kPointerSize;

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

  // Saved instance of Irregexp compiled code or bytecode for Latin1 that
  // is a potential candidate for flushing.
  static const int kIrregexpLatin1CodeSavedIndex = kDataIndex + 2;
  // Saved instance of Irregexp compiled code or bytecode for UC16 that is
  // a potential candidate for flushing.
  static const int kIrregexpUC16CodeSavedIndex = kDataIndex + 3;

  // Maximal number of registers used by either Latin1 or UC16.
  // Only used to check that there is enough stack space
  static const int kIrregexpMaxRegisterCountIndex = kDataIndex + 4;
  // Number of captures in the compiled regexp.
  static const int kIrregexpCaptureCountIndex = kDataIndex + 5;
  // Maps names of named capture groups (at indices 2i) to their corresponding
  // capture group indices (at indices 2i + 1).
  static const int kIrregexpCaptureNameMapIndex = kDataIndex + 6;

  static const int kIrregexpDataSize = kIrregexpCaptureNameMapIndex + 1;

  // Offsets directly into the data fixed array.
  static const int kDataTagOffset =
      FixedArray::kHeaderSize + kTagIndex * kPointerSize;
  static const int kDataOneByteCodeOffset =
      FixedArray::kHeaderSize + kIrregexpLatin1CodeIndex * kPointerSize;
  static const int kDataUC16CodeOffset =
      FixedArray::kHeaderSize + kIrregexpUC16CodeIndex * kPointerSize;
  static const int kIrregexpCaptureCountOffset =
      FixedArray::kHeaderSize + kIrregexpCaptureCountIndex * kPointerSize;

  // In-object fields.
  static const int kLastIndexFieldIndex = 0;
  static const int kInObjectFieldCount = 1;

  // The uninitialized value for a regexp code object.
  static const int kUninitializedValue = -1;

  // The compilation error value for the regexp code object. The real error
  // object is in the saved code field.
  static const int kCompilationErrorValue = -2;

  // When we store the sweep generation at which we moved the code from the
  // code index to the saved code index we mask it of to be in the [0:255]
  // range.
  static const int kCodeAgeMask = 0xff;
};

DEFINE_OPERATORS_FOR_FLAGS(JSRegExp::Flags)


class CompilationCacheShape : public BaseShape<HashTableKey*> {
 public:
  static inline bool IsMatch(HashTableKey* key, Object* value) {
    return key->IsMatch(value);
  }

  static inline uint32_t Hash(HashTableKey* key) {
    return key->Hash();
  }

  static inline uint32_t HashForObject(HashTableKey* key, Object* object) {
    return key->HashForObject(object);
  }

  static inline Handle<Object> AsHandle(Isolate* isolate, HashTableKey* key);

  static const int kPrefixSize = 0;
  static const int kEntrySize = 2;
};


// This cache is used in two different variants. For regexp caching, it simply
// maps identifying info of the regexp to the cached regexp object. Scripts and
// eval code only gets cached after a second probe for the code object. To do
// so, on first "put" only a hash identifying the source is entered into the
// cache, mapping it to a lifetime count of the hash. On each call to Age all
// such lifetimes get reduced, and removed once they reach zero. If a second put
// is called while such a hash is live in the cache, the hash gets replaced by
// an actual cache entry. Age also removes stale live entries from the cache.
// Such entries are identified by SharedFunctionInfos pointing to either the
// recompilation stub, or to "old" code. This avoids memory leaks due to
// premature caching of scripts and eval strings that are never needed later.
class CompilationCacheTable: public HashTable<CompilationCacheTable,
                                              CompilationCacheShape,
                                              HashTableKey*> {
 public:
  // Find cached value for a string key, otherwise return null.
  Handle<Object> Lookup(
      Handle<String> src, Handle<Context> context, LanguageMode language_mode);
  Handle<Object> LookupEval(
      Handle<String> src, Handle<SharedFunctionInfo> shared,
      LanguageMode language_mode, int scope_position);
  Handle<Object> LookupRegExp(Handle<String> source, JSRegExp::Flags flags);
  static Handle<CompilationCacheTable> Put(
      Handle<CompilationCacheTable> cache, Handle<String> src,
      Handle<Context> context, LanguageMode language_mode,
      Handle<Object> value);
  static Handle<CompilationCacheTable> PutEval(
      Handle<CompilationCacheTable> cache, Handle<String> src,
      Handle<SharedFunctionInfo> context, Handle<SharedFunctionInfo> value,
      int scope_position);
  static Handle<CompilationCacheTable> PutRegExp(
      Handle<CompilationCacheTable> cache, Handle<String> src,
      JSRegExp::Flags flags, Handle<FixedArray> value);
  void Remove(Object* value);
  void Age();
  static const int kHashGenerations = 10;

  DECLARE_CAST(CompilationCacheTable)

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(CompilationCacheTable);
};


class CodeCacheHashTableShape : public BaseShape<HashTableKey*> {
 public:
  static inline bool IsMatch(HashTableKey* key, Object* value) {
    return key->IsMatch(value);
  }

  static inline uint32_t Hash(HashTableKey* key) {
    return key->Hash();
  }

  static inline uint32_t HashForObject(HashTableKey* key, Object* object) {
    return key->HashForObject(object);
  }

  static inline Handle<Object> AsHandle(Isolate* isolate, HashTableKey* key);

  static const int kPrefixSize = 0;
  // The both the key (name + flags) and value (code object) can be derived from
  // the fixed array that stores both the name and code.
  // TODO(verwaest): Don't allocate a fixed array but inline name and code.
  // Rewrite IsMatch to get table + index as input rather than just the raw key.
  static const int kEntrySize = 1;
};


class CodeCacheHashTable: public HashTable<CodeCacheHashTable,
                                           CodeCacheHashTableShape,
                                           HashTableKey*> {
 public:
  static Handle<CodeCacheHashTable> Put(
      Handle<CodeCacheHashTable> table,
      Handle<Name> name,
      Handle<Code> code);

  Code* Lookup(Name* name, Code::Flags flags);

  DECLARE_CAST(CodeCacheHashTable)

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(CodeCacheHashTable);
};


class TypeFeedbackInfo: public Struct {
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

  DECLARE_CAST(TypeFeedbackInfo)

  // Dispatched behavior.
  DECLARE_PRINTER(TypeFeedbackInfo)
  DECLARE_VERIFIER(TypeFeedbackInfo)

  static const int kStorage1Offset = HeapObject::kHeaderSize;
  static const int kStorage2Offset = kStorage1Offset + kPointerSize;
  static const int kStorage3Offset = kStorage2Offset + kPointerSize;
  static const int kSize = kStorage3Offset + kPointerSize;

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

  DECL_ACCESSORS(transition_info, Object)
  // nested_site threads a list of sites that represent nested literals
  // walked in a particular order. So [[1, 2], 1, 2] will have one
  // nested_site, but [[1, 2], 3, [4]] will have a list of two.
  DECL_ACCESSORS(nested_site, Object)
  DECL_INT_ACCESSORS(pretenure_data)
  DECL_INT_ACCESSORS(pretenure_create_count)
  DECL_ACCESSORS(dependent_code, DependentCode)
  DECL_ACCESSORS(weak_next, Object)

  inline void Initialize();

  // This method is expensive, it should only be called for reporting.
  bool IsNestedSite();

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

  PretenureFlag GetPretenureMode();

  void ResetPretenureDecision();

  inline PretenureDecision pretenure_decision();
  inline void set_pretenure_decision(PretenureDecision decision);

  inline bool deopt_dependent_code();
  inline void set_deopt_dependent_code(bool deopt);

  inline int memento_found_count();
  inline void set_memento_found_count(int count);

  inline int memento_create_count();
  inline void set_memento_create_count(int count);

  // The pretenuring decision is made during gc, and the zombie state allows
  // us to recognize when an allocation site is just being kept alive because
  // a later traversal of new space may discover AllocationMementos that point
  // to this AllocationSite.
  inline bool IsZombie();

  inline bool IsMaybeTenure();

  inline void MarkZombie();

  inline bool MakePretenureDecision(PretenureDecision current_decision,
                                    double ratio,
                                    bool maximum_size_scavenge);

  inline bool DigestPretenuringFeedback(bool maximum_size_scavenge);

  inline ElementsKind GetElementsKind();
  inline void SetElementsKind(ElementsKind kind);

  inline bool CanInlineCall();
  inline void SetDoNotInlineCall();

  inline bool SitePointsToLiteral();

  template <AllocationSiteUpdateMode update_or_check =
                AllocationSiteUpdateMode::kUpdate>
  static bool DigestTransitionFeedback(Handle<AllocationSite> site,
                                       ElementsKind to_kind);

  DECLARE_PRINTER(AllocationSite)
  DECLARE_VERIFIER(AllocationSite)

  DECLARE_CAST(AllocationSite)
  static inline AllocationSiteMode GetMode(
      ElementsKind boilerplate_elements_kind);
  static AllocationSiteMode GetMode(ElementsKind from, ElementsKind to);
  static inline bool CanTrack(InstanceType type);

  static const int kTransitionInfoOffset = HeapObject::kHeaderSize;
  static const int kNestedSiteOffset = kTransitionInfoOffset + kPointerSize;
  static const int kPretenureDataOffset = kNestedSiteOffset + kPointerSize;
  static const int kPretenureCreateCountOffset =
      kPretenureDataOffset + kPointerSize;
  static const int kDependentCodeOffset =
      kPretenureCreateCountOffset + kPointerSize;
  static const int kWeakNextOffset = kDependentCodeOffset + kPointerSize;
  static const int kSize = kWeakNextOffset + kPointerSize;

  // During mark compact we need to take special care for the dependent code
  // field.
  static const int kPointerFieldsBeginOffset = kTransitionInfoOffset;
  static const int kPointerFieldsEndOffset = kWeakNextOffset;

  typedef FixedBodyDescriptor<kPointerFieldsBeginOffset,
                              kPointerFieldsEndOffset, kSize>
      MarkingBodyDescriptor;

  // For other visitors, use the fixed body descriptor below.
  typedef FixedBodyDescriptor<HeapObject::kHeaderSize, kSize, kSize>
      BodyDescriptor;

 private:
  inline bool PretenuringDecisionMade();

  DISALLOW_IMPLICIT_CONSTRUCTORS(AllocationSite);
};


class AllocationMemento: public Struct {
 public:
  static const int kAllocationSiteOffset = HeapObject::kHeaderSize;
  static const int kSize = kAllocationSiteOffset + kPointerSize;

  DECL_ACCESSORS(allocation_site, Object)

  inline bool IsValid();
  inline AllocationSite* GetAllocationSite();
  inline Address GetAllocationSiteUnchecked();

  DECLARE_PRINTER(AllocationMemento)
  DECLARE_VERIFIER(AllocationMemento)

  DECLARE_CAST(AllocationMemento)

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(AllocationMemento);
};


// Representation of a slow alias as part of a sloppy arguments objects.
// For fast aliases (if HasSloppyArgumentsElements()):
// - the parameter map contains an index into the context
// - all attributes of the element have default values
// For slow aliases (if HasDictionaryArgumentsElements()):
// - the parameter map contains no fast alias mapping (i.e. the hole)
// - this struct (in the slow backing store) contains an index into the context
// - all attributes are available as part if the property details
class AliasedArgumentsEntry: public Struct {
 public:
  inline int aliased_context_slot() const;
  inline void set_aliased_context_slot(int count);

  DECLARE_CAST(AliasedArgumentsEntry)

  // Dispatched behavior.
  DECLARE_PRINTER(AliasedArgumentsEntry)
  DECLARE_VERIFIER(AliasedArgumentsEntry)

  static const int kAliasedContextSlot = HeapObject::kHeaderSize;
  static const int kSize = kAliasedContextSlot + kPointerSize;

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(AliasedArgumentsEntry);
};


enum AllowNullsFlag {ALLOW_NULLS, DISALLOW_NULLS};
enum RobustnessFlag {ROBUST_STRING_TRAVERSAL, FAST_STRING_TRAVERSAL};

class V8_EXPORT_PRIVATE StringHasher {
 public:
  explicit inline StringHasher(int length, uint32_t seed);

  template <typename schar>
  static inline uint32_t HashSequentialString(const schar* chars,
                                              int length,
                                              uint32_t seed);

  // Reads all the data, even for long strings and computes the utf16 length.
  static uint32_t ComputeUtf8Hash(Vector<const char> chars,
                                  uint32_t seed,
                                  int* utf16_length_out);

  // Calculated hash value for a string consisting of 1 to
  // String::kMaxArrayIndexSize digits with no leading zeros (except "0").
  // value is represented decimal value.
  static uint32_t MakeArrayIndexHash(uint32_t value, int length);

  // No string is allowed to have a hash of zero.  That value is reserved
  // for internal properties.  If the hash calculation yields zero then we
  // use 27 instead.
  static const int kZeroHash = 27;

  // Reusable parts of the hashing algorithm.
  INLINE(static uint32_t AddCharacterCore(uint32_t running_hash, uint16_t c));
  INLINE(static uint32_t GetHashCore(uint32_t running_hash));
  INLINE(static uint32_t ComputeRunningHash(uint32_t running_hash,
                                            const uc16* chars, int length));
  INLINE(static uint32_t ComputeRunningHashOneByte(uint32_t running_hash,
                                                   const char* chars,
                                                   int length));

 protected:
  // Returns the value to store in the hash field of a string with
  // the given length and contents.
  uint32_t GetHashField();
  // Returns true if the hash of this string can be computed without
  // looking at the contents.
  inline bool has_trivial_hash();
  // Adds a block of characters to the hash.
  template<typename Char>
  inline void AddCharacters(const Char* chars, int len);

 private:
  // Add a character to the hash.
  inline void AddCharacter(uint16_t c);
  // Update index. Returns true if string is still an index.
  inline bool UpdateIndex(uint16_t c);

  int length_;
  uint32_t raw_running_hash_;
  uint32_t array_index_;
  bool is_array_index_;
  bool is_first_char_;
  DISALLOW_COPY_AND_ASSIGN(StringHasher);
};


class IteratingStringHasher : public StringHasher {
 public:
  static inline uint32_t Hash(String* string, uint32_t seed);
  inline void VisitOneByteString(const uint8_t* chars, int length);
  inline void VisitTwoByteString(const uint16_t* chars, int length);

 private:
  inline IteratingStringHasher(int len, uint32_t seed);
  void VisitConsString(ConsString* cons_string);
  DISALLOW_COPY_AND_ASSIGN(IteratingStringHasher);
};


// The characteristics of a string are stored in its map.  Retrieving these
// few bits of information is moderately expensive, involving two memory
// loads where the second is dependent on the first.  To improve efficiency
// the shape of the string is given its own class so that it can be retrieved
// once and used for several string operations.  A StringShape is small enough
// to be passed by value and is immutable, but be aware that flattening a
// string can potentially alter its shape.  Also be aware that a GC caused by
// something else can alter the shape of a string due to ConsString
// shortcutting.  Keeping these restrictions in mind has proven to be error-
// prone and so we no longer put StringShapes in variables unless there is a
// concrete performance benefit at that particular point in the code.
class StringShape BASE_EMBEDDED {
 public:
  inline explicit StringShape(const String* s);
  inline explicit StringShape(Map* s);
  inline explicit StringShape(InstanceType t);
  inline bool IsSequential();
  inline bool IsExternal();
  inline bool IsCons();
  inline bool IsSliced();
  inline bool IsIndirect();
  inline bool IsExternalOneByte();
  inline bool IsExternalTwoByte();
  inline bool IsSequentialOneByte();
  inline bool IsSequentialTwoByte();
  inline bool IsInternalized();
  inline StringRepresentationTag representation_tag();
  inline uint32_t encoding_tag();
  inline uint32_t full_representation_tag();
  inline uint32_t size_tag();
#ifdef DEBUG
  inline uint32_t type() { return type_; }
  inline void invalidate() { valid_ = false; }
  inline bool valid() { return valid_; }
#else
  inline void invalidate() { }
#endif

 private:
  uint32_t type_;
#ifdef DEBUG
  inline void set_valid() { valid_ = true; }
  bool valid_;
#else
  inline void set_valid() { }
#endif
};


// The Name abstract class captures anything that can be used as a property
// name, i.e., strings and symbols.  All names store a hash value.
class Name: public HeapObject {
 public:
  // Get and set the hash field of the name.
  inline uint32_t hash_field();
  inline void set_hash_field(uint32_t value);

  // Tells whether the hash code has been computed.
  inline bool HasHashCode();

  // Returns a hash value used for the property table
  inline uint32_t Hash();

  // Equality operations.
  inline bool Equals(Name* other);
  inline static bool Equals(Handle<Name> one, Handle<Name> two);

  // Conversion.
  inline bool AsArrayIndex(uint32_t* index);

  // If the name is private, it can only name own properties.
  inline bool IsPrivate();

  inline bool IsUniqueName() const;

  // Return a string version of this name that is converted according to the
  // rules described in ES6 section 9.2.11.
  MUST_USE_RESULT static MaybeHandle<String> ToFunctionName(Handle<Name> name);
  MUST_USE_RESULT static MaybeHandle<String> ToFunctionName(
      Handle<Name> name, Handle<String> prefix);

  DECLARE_CAST(Name)

  DECLARE_PRINTER(Name)
#if TRACE_MAPS
  void NameShortPrint();
  int NameShortPrint(Vector<char> str);
#endif

  // Layout description.
  static const int kHashFieldSlot = HeapObject::kHeaderSize;
#if V8_TARGET_LITTLE_ENDIAN || !V8_HOST_ARCH_64_BIT
  static const int kHashFieldOffset = kHashFieldSlot;
#else
  static const int kHashFieldOffset = kHashFieldSlot + kIntSize;
#endif
  static const int kSize = kHashFieldSlot + kPointerSize;

  // Mask constant for checking if a name has a computed hash code
  // and if it is a string that is an array index.  The least significant bit
  // indicates whether a hash code has been computed.  If the hash code has
  // been computed the 2nd bit tells whether the string can be used as an
  // array index.
  static const int kHashNotComputedMask = 1;
  static const int kIsNotArrayIndexMask = 1 << 1;
  static const int kNofHashBitFields = 2;

  // Shift constant retrieving hash code from hash field.
  static const int kHashShift = kNofHashBitFields;

  // Only these bits are relevant in the hash, since the top two are shifted
  // out.
  static const uint32_t kHashBitMask = 0xffffffffu >> kHashShift;

  // Array index strings this short can keep their index in the hash field.
  static const int kMaxCachedArrayIndexLength = 7;

  // Maximum number of characters to consider when trying to convert a string
  // value into an array index.
  static const int kMaxArrayIndexSize = 10;

  // For strings which are array indexes the hash value has the string length
  // mixed into the hash, mainly to avoid a hash value of zero which would be
  // the case for the string '0'. 24 bits are used for the array index value.
  static const int kArrayIndexValueBits = 24;
  static const int kArrayIndexLengthBits =
      kBitsPerInt - kArrayIndexValueBits - kNofHashBitFields;

  STATIC_ASSERT(kArrayIndexLengthBits > 0);
  STATIC_ASSERT(kMaxArrayIndexSize < (1 << kArrayIndexLengthBits));

  class ArrayIndexValueBits : public BitField<unsigned int, kNofHashBitFields,
      kArrayIndexValueBits> {};  // NOLINT
  class ArrayIndexLengthBits : public BitField<unsigned int,
      kNofHashBitFields + kArrayIndexValueBits,
      kArrayIndexLengthBits> {};  // NOLINT

  // Check that kMaxCachedArrayIndexLength + 1 is a power of two so we
  // could use a mask to test if the length of string is less than or equal to
  // kMaxCachedArrayIndexLength.
  STATIC_ASSERT(IS_POWER_OF_TWO(kMaxCachedArrayIndexLength + 1));

  static const unsigned int kContainsCachedArrayIndexMask =
      (~static_cast<unsigned>(kMaxCachedArrayIndexLength)
       << ArrayIndexLengthBits::kShift) |
      kIsNotArrayIndexMask;

  // Value of empty hash field indicating that the hash is not computed.
  static const int kEmptyHashField =
      kIsNotArrayIndexMask | kHashNotComputedMask;

 protected:
  static inline bool IsHashFieldComputed(uint32_t field);

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(Name);
};


// ES6 symbols.
class Symbol: public Name {
 public:
  // [name]: The print name of a symbol, or undefined if none.
  DECL_ACCESSORS(name, Object)

  DECL_INT_ACCESSORS(flags)

  // [is_private]: Whether this is a private symbol.  Private symbols can only
  // be used to designate own properties of objects.
  DECL_BOOLEAN_ACCESSORS(is_private)

  // [is_well_known_symbol]: Whether this is a spec-defined well-known symbol,
  // or not. Well-known symbols do not throw when an access check fails during
  // a load.
  DECL_BOOLEAN_ACCESSORS(is_well_known_symbol)

  // [is_public]: Whether this is a symbol created by Symbol.for. Calling
  // Symbol.keyFor on such a symbol simply needs to return the attached name.
  DECL_BOOLEAN_ACCESSORS(is_public)

  DECLARE_CAST(Symbol)

  // Dispatched behavior.
  DECLARE_PRINTER(Symbol)
  DECLARE_VERIFIER(Symbol)

  // Layout description.
  static const int kNameOffset = Name::kSize;
  static const int kFlagsOffset = kNameOffset + kPointerSize;
  static const int kSize = kFlagsOffset + kPointerSize;

  // Flags layout.
  static const int kPrivateBit = 0;
  static const int kWellKnownSymbolBit = 1;
  static const int kPublicBit = 2;

  typedef FixedBodyDescriptor<kNameOffset, kFlagsOffset, kSize> BodyDescriptor;

  void SymbolShortPrint(std::ostream& os);

 private:
  const char* PrivateSymbolToName() const;

#if TRACE_MAPS
  friend class Name;  // For PrivateSymbolToName.
#endif

  DISALLOW_IMPLICIT_CONSTRUCTORS(Symbol);
};


class ConsString;

// The String abstract class captures JavaScript string values:
//
// Ecma-262:
//  4.3.16 String Value
//    A string value is a member of the type String and is a finite
//    ordered sequence of zero or more 16-bit unsigned integer values.
//
// All string values have a length field.
class String: public Name {
 public:
  enum Encoding { ONE_BYTE_ENCODING, TWO_BYTE_ENCODING };

  class SubStringRange {
   public:
    explicit inline SubStringRange(String* string, int first = 0,
                                   int length = -1);
    class iterator;
    inline iterator begin();
    inline iterator end();

   private:
    String* string_;
    int first_;
    int length_;
  };

  // Representation of the flat content of a String.
  // A non-flat string doesn't have flat content.
  // A flat string has content that's encoded as a sequence of either
  // one-byte chars or two-byte UC16.
  // Returned by String::GetFlatContent().
  class FlatContent {
   public:
    // Returns true if the string is flat and this structure contains content.
    bool IsFlat() const { return state_ != NON_FLAT; }
    // Returns true if the structure contains one-byte content.
    bool IsOneByte() const { return state_ == ONE_BYTE; }
    // Returns true if the structure contains two-byte content.
    bool IsTwoByte() const { return state_ == TWO_BYTE; }

    // Return the one byte content of the string. Only use if IsOneByte()
    // returns true.
    Vector<const uint8_t> ToOneByteVector() const {
      DCHECK_EQ(ONE_BYTE, state_);
      return Vector<const uint8_t>(onebyte_start, length_);
    }
    // Return the two-byte content of the string. Only use if IsTwoByte()
    // returns true.
    Vector<const uc16> ToUC16Vector() const {
      DCHECK_EQ(TWO_BYTE, state_);
      return Vector<const uc16>(twobyte_start, length_);
    }

    uc16 Get(int i) const {
      DCHECK(i < length_);
      DCHECK(state_ != NON_FLAT);
      if (state_ == ONE_BYTE) return onebyte_start[i];
      return twobyte_start[i];
    }

    bool UsesSameString(const FlatContent& other) const {
      return onebyte_start == other.onebyte_start;
    }

   private:
    enum State { NON_FLAT, ONE_BYTE, TWO_BYTE };

    // Constructors only used by String::GetFlatContent().
    explicit FlatContent(const uint8_t* start, int length)
        : onebyte_start(start), length_(length), state_(ONE_BYTE) {}
    explicit FlatContent(const uc16* start, int length)
        : twobyte_start(start), length_(length), state_(TWO_BYTE) { }
    FlatContent() : onebyte_start(NULL), length_(0), state_(NON_FLAT) { }

    union {
      const uint8_t* onebyte_start;
      const uc16* twobyte_start;
    };
    int length_;
    State state_;

    friend class String;
    friend class IterableSubString;
  };

  template <typename Char>
  INLINE(Vector<const Char> GetCharVector());

  // Get and set the length of the string.
  inline int length() const;
  inline void set_length(int value);

  // Get and set the length of the string using acquire loads and release
  // stores.
  inline int synchronized_length() const;
  inline void synchronized_set_length(int value);

  // Returns whether this string has only one-byte chars, i.e. all of them can
  // be one-byte encoded.  This might be the case even if the string is
  // two-byte.  Such strings may appear when the embedder prefers
  // two-byte external representations even for one-byte data.
  inline bool IsOneByteRepresentation() const;
  inline bool IsTwoByteRepresentation() const;

  // Cons and slices have an encoding flag that may not represent the actual
  // encoding of the underlying string.  This is taken into account here.
  // Requires: this->IsFlat()
  inline bool IsOneByteRepresentationUnderneath();
  inline bool IsTwoByteRepresentationUnderneath();

  // NOTE: this should be considered only a hint.  False negatives are
  // possible.
  inline bool HasOnlyOneByteChars();

  // Get and set individual two byte chars in the string.
  inline void Set(int index, uint16_t value);
  // Get individual two byte char in the string.  Repeated calls
  // to this method are not efficient unless the string is flat.
  INLINE(uint16_t Get(int index));

  // ES6 section 7.1.3.1 ToNumber Applied to the String Type
  static Handle<Object> ToNumber(Handle<String> subject);

  // Flattens the string.  Checks first inline to see if it is
  // necessary.  Does nothing if the string is not a cons string.
  // Flattening allocates a sequential string with the same data as
  // the given string and mutates the cons string to a degenerate
  // form, where the first component is the new sequential string and
  // the second component is the empty string.  If allocation fails,
  // this function returns a failure.  If flattening succeeds, this
  // function returns the sequential string that is now the first
  // component of the cons string.
  //
  // Degenerate cons strings are handled specially by the garbage
  // collector (see IsShortcutCandidate).

  static inline Handle<String> Flatten(Handle<String> string,
                                       PretenureFlag pretenure = NOT_TENURED);

  // Tries to return the content of a flat string as a structure holding either
  // a flat vector of char or of uc16.
  // If the string isn't flat, and therefore doesn't have flat content, the
  // returned structure will report so, and can't provide a vector of either
  // kind.
  FlatContent GetFlatContent();

  // Returns the parent of a sliced string or first part of a flat cons string.
  // Requires: StringShape(this).IsIndirect() && this->IsFlat()
  inline String* GetUnderlying();

  // String relational comparison, implemented according to ES6 section 7.2.11
  // Abstract Relational Comparison (step 5): The comparison of Strings uses a
  // simple lexicographic ordering on sequences of code unit values. There is no
  // attempt to use the more complex, semantically oriented definitions of
  // character or string equality and collating order defined in the Unicode
  // specification. Therefore String values that are canonically equal according
  // to the Unicode standard could test as unequal. In effect this algorithm
  // assumes that both Strings are already in normalized form. Also, note that
  // for strings containing supplementary characters, lexicographic ordering on
  // sequences of UTF-16 code unit values differs from that on sequences of code
  // point values.
  MUST_USE_RESULT static ComparisonResult Compare(Handle<String> x,
                                                  Handle<String> y);

  // Perform ES6 21.1.3.8, including checking arguments.
  static Object* IndexOf(Isolate* isolate, Handle<Object> receiver,
                         Handle<Object> search, Handle<Object> position);
  // Perform string match of pattern on subject, starting at start index.
  // Caller must ensure that 0 <= start_index <= sub->length(), as this does not
  // check any arguments.
  static int IndexOf(Isolate* isolate, Handle<String> receiver,
                     Handle<String> search, int start_index);

  static Object* LastIndexOf(Isolate* isolate, Handle<Object> receiver,
                             Handle<Object> search, Handle<Object> position);

  // Encapsulates logic related to a match and its capture groups as required
  // by GetSubstitution.
  class Match {
   public:
    virtual Handle<String> GetMatch() = 0;
    virtual MaybeHandle<String> GetCapture(int i, bool* capture_exists) = 0;
    virtual Handle<String> GetPrefix() = 0;
    virtual Handle<String> GetSuffix() = 0;
    virtual int CaptureCount() = 0;
    virtual ~Match() {}
  };

  // ES#sec-getsubstitution
  // GetSubstitution(matched, str, position, captures, replacement)
  // Expand the $-expressions in the string and return a new string with
  // the result.
  MUST_USE_RESULT static MaybeHandle<String> GetSubstitution(
      Isolate* isolate, Match* match, Handle<String> replacement);

  // String equality operations.
  inline bool Equals(String* other);
  inline static bool Equals(Handle<String> one, Handle<String> two);
  bool IsUtf8EqualTo(Vector<const char> str, bool allow_prefix_match = false);
  bool IsOneByteEqualTo(Vector<const uint8_t> str);
  bool IsTwoByteEqualTo(Vector<const uc16> str);

  // Return a UTF8 representation of the string.  The string is null
  // terminated but may optionally contain nulls.  Length is returned
  // in length_output if length_output is not a null pointer  The string
  // should be nearly flat, otherwise the performance of this method may
  // be very slow (quadratic in the length).  Setting robustness_flag to
  // ROBUST_STRING_TRAVERSAL invokes behaviour that is robust  This means it
  // handles unexpected data without causing assert failures and it does not
  // do any heap allocations.  This is useful when printing stack traces.
  std::unique_ptr<char[]> ToCString(AllowNullsFlag allow_nulls,
                                    RobustnessFlag robustness_flag, int offset,
                                    int length, int* length_output = 0);
  std::unique_ptr<char[]> ToCString(
      AllowNullsFlag allow_nulls = DISALLOW_NULLS,
      RobustnessFlag robustness_flag = FAST_STRING_TRAVERSAL,
      int* length_output = 0);

  bool ComputeArrayIndex(uint32_t* index);

  // Externalization.
  bool MakeExternal(v8::String::ExternalStringResource* resource);
  bool MakeExternal(v8::String::ExternalOneByteStringResource* resource);

  // Conversion.
  inline bool AsArrayIndex(uint32_t* index);
  uint32_t inline ToValidIndex(Object* number);

  // Trimming.
  enum TrimMode { kTrim, kTrimLeft, kTrimRight };
  static Handle<String> Trim(Handle<String> string, TrimMode mode);

  DECLARE_CAST(String)

  void PrintOn(FILE* out);

  // For use during stack traces.  Performs rudimentary sanity check.
  bool LooksValid();

  // Dispatched behavior.
  void StringShortPrint(StringStream* accumulator, bool show_details = true);
  void PrintUC16(std::ostream& os, int start = 0, int end = -1);  // NOLINT
#if defined(DEBUG) || defined(OBJECT_PRINT)
  char* ToAsciiArray();
#endif
  DECLARE_PRINTER(String)
  DECLARE_VERIFIER(String)

  inline bool IsFlat();

  // Layout description.
  static const int kLengthOffset = Name::kSize;
  static const int kSize = kLengthOffset + kPointerSize;

  // Max char codes.
  static const int32_t kMaxOneByteCharCode = unibrow::Latin1::kMaxChar;
  static const uint32_t kMaxOneByteCharCodeU = unibrow::Latin1::kMaxChar;
  static const int kMaxUtf16CodeUnit = 0xffff;
  static const uint32_t kMaxUtf16CodeUnitU = kMaxUtf16CodeUnit;
  static const uc32 kMaxCodePoint = 0x10ffff;

  // Maximal string length.
  static const int kMaxLength = (1 << 28) - 16;

  // Max length for computing hash. For strings longer than this limit the
  // string length is used as the hash value.
  static const int kMaxHashCalcLength = 16383;

  // Limit for truncation in short printing.
  static const int kMaxShortPrintLength = 1024;

  // Support for regular expressions.
  const uc16* GetTwoByteData(unsigned start);

  // Helper function for flattening strings.
  template <typename sinkchar>
  static void WriteToFlat(String* source,
                          sinkchar* sink,
                          int from,
                          int to);

  // The return value may point to the first aligned word containing the first
  // non-one-byte character, rather than directly to the non-one-byte character.
  // If the return value is >= the passed length, the entire string was
  // one-byte.
  static inline int NonAsciiStart(const char* chars, int length) {
    const char* start = chars;
    const char* limit = chars + length;

    if (length >= kIntptrSize) {
      // Check unaligned bytes.
      while (!IsAligned(reinterpret_cast<intptr_t>(chars), sizeof(uintptr_t))) {
        if (static_cast<uint8_t>(*chars) > unibrow::Utf8::kMaxOneByteChar) {
          return static_cast<int>(chars - start);
        }
        ++chars;
      }
      // Check aligned words.
      DCHECK(unibrow::Utf8::kMaxOneByteChar == 0x7F);
      const uintptr_t non_one_byte_mask = kUintptrAllBitsSet / 0xFF * 0x80;
      while (chars + sizeof(uintptr_t) <= limit) {
        if (*reinterpret_cast<const uintptr_t*>(chars) & non_one_byte_mask) {
          return static_cast<int>(chars - start);
        }
        chars += sizeof(uintptr_t);
      }
    }
    // Check remaining unaligned bytes.
    while (chars < limit) {
      if (static_cast<uint8_t>(*chars) > unibrow::Utf8::kMaxOneByteChar) {
        return static_cast<int>(chars - start);
      }
      ++chars;
    }

    return static_cast<int>(chars - start);
  }

  static inline bool IsAscii(const char* chars, int length) {
    return NonAsciiStart(chars, length) >= length;
  }

  static inline bool IsAscii(const uint8_t* chars, int length) {
    return
        NonAsciiStart(reinterpret_cast<const char*>(chars), length) >= length;
  }

  static inline int NonOneByteStart(const uc16* chars, int length) {
    const uc16* limit = chars + length;
    const uc16* start = chars;
    while (chars < limit) {
      if (*chars > kMaxOneByteCharCodeU) return static_cast<int>(chars - start);
      ++chars;
    }
    return static_cast<int>(chars - start);
  }

  static inline bool IsOneByte(const uc16* chars, int length) {
    return NonOneByteStart(chars, length) >= length;
  }

  template<class Visitor>
  static inline ConsString* VisitFlat(Visitor* visitor,
                                      String* string,
                                      int offset = 0);

  static Handle<FixedArray> CalculateLineEnds(Handle<String> string,
                                              bool include_ending_line);

  // Use the hash field to forward to the canonical internalized string
  // when deserializing an internalized string.
  inline void SetForwardedInternalizedString(String* string);
  inline String* GetForwardedInternalizedString();

 private:
  friend class Name;
  friend class StringTableInsertionKey;

  static Handle<String> SlowFlatten(Handle<ConsString> cons,
                                    PretenureFlag tenure);

  // Slow case of String::Equals.  This implementation works on any strings
  // but it is most efficient on strings that are almost flat.
  bool SlowEquals(String* other);

  static bool SlowEquals(Handle<String> one, Handle<String> two);

  // Slow case of AsArrayIndex.
  V8_EXPORT_PRIVATE bool SlowAsArrayIndex(uint32_t* index);

  // Compute and set the hash code.
  uint32_t ComputeAndSetHash();

  DISALLOW_IMPLICIT_CONSTRUCTORS(String);
};


// The SeqString abstract class captures sequential string values.
class SeqString: public String {
 public:
  DECLARE_CAST(SeqString)

  // Layout description.
  static const int kHeaderSize = String::kSize;

  // Truncate the string in-place if possible and return the result.
  // In case of new_length == 0, the empty string is returned without
  // truncating the original string.
  MUST_USE_RESULT static Handle<String> Truncate(Handle<SeqString> string,
                                                 int new_length);
 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(SeqString);
};


// The OneByteString class captures sequential one-byte string objects.
// Each character in the OneByteString is an one-byte character.
class SeqOneByteString: public SeqString {
 public:
  static const bool kHasOneByteEncoding = true;

  // Dispatched behavior.
  inline uint16_t SeqOneByteStringGet(int index);
  inline void SeqOneByteStringSet(int index, uint16_t value);

  // Get the address of the characters in this string.
  inline Address GetCharsAddress();

  inline uint8_t* GetChars();

  DECLARE_CAST(SeqOneByteString)

  // Garbage collection support.  This method is called by the
  // garbage collector to compute the actual size of an OneByteString
  // instance.
  inline int SeqOneByteStringSize(InstanceType instance_type);

  // Computes the size for an OneByteString instance of a given length.
  static int SizeFor(int length) {
    return OBJECT_POINTER_ALIGN(kHeaderSize + length * kCharSize);
  }

  // Maximal memory usage for a single sequential one-byte string.
  static const int kMaxSize = 512 * MB - 1;
  STATIC_ASSERT((kMaxSize - kHeaderSize) >= String::kMaxLength);

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(SeqOneByteString);
};


// The TwoByteString class captures sequential unicode string objects.
// Each character in the TwoByteString is a two-byte uint16_t.
class SeqTwoByteString: public SeqString {
 public:
  static const bool kHasOneByteEncoding = false;

  // Dispatched behavior.
  inline uint16_t SeqTwoByteStringGet(int index);
  inline void SeqTwoByteStringSet(int index, uint16_t value);

  // Get the address of the characters in this string.
  inline Address GetCharsAddress();

  inline uc16* GetChars();

  // For regexp code.
  const uint16_t* SeqTwoByteStringGetData(unsigned start);

  DECLARE_CAST(SeqTwoByteString)

  // Garbage collection support.  This method is called by the
  // garbage collector to compute the actual size of a TwoByteString
  // instance.
  inline int SeqTwoByteStringSize(InstanceType instance_type);

  // Computes the size for a TwoByteString instance of a given length.
  static int SizeFor(int length) {
    return OBJECT_POINTER_ALIGN(kHeaderSize + length * kShortSize);
  }

  // Maximal memory usage for a single sequential two-byte string.
  static const int kMaxSize = 512 * MB - 1;
  STATIC_ASSERT(static_cast<int>((kMaxSize - kHeaderSize)/sizeof(uint16_t)) >=
               String::kMaxLength);

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(SeqTwoByteString);
};


// The ConsString class describes string values built by using the
// addition operator on strings.  A ConsString is a pair where the
// first and second components are pointers to other string values.
// One or both components of a ConsString can be pointers to other
// ConsStrings, creating a binary tree of ConsStrings where the leaves
// are non-ConsString string values.  The string value represented by
// a ConsString can be obtained by concatenating the leaf string
// values in a left-to-right depth-first traversal of the tree.
class ConsString: public String {
 public:
  // First string of the cons cell.
  inline String* first();
  // Doesn't check that the result is a string, even in debug mode.  This is
  // useful during GC where the mark bits confuse the checks.
  inline Object* unchecked_first();
  inline void set_first(String* first,
                        WriteBarrierMode mode = UPDATE_WRITE_BARRIER);

  // Second string of the cons cell.
  inline String* second();
  // Doesn't check that the result is a string, even in debug mode.  This is
  // useful during GC where the mark bits confuse the checks.
  inline Object* unchecked_second();
  inline void set_second(String* second,
                         WriteBarrierMode mode = UPDATE_WRITE_BARRIER);

  // Dispatched behavior.
  V8_EXPORT_PRIVATE uint16_t ConsStringGet(int index);

  DECLARE_CAST(ConsString)

  // Layout description.
  static const int kFirstOffset = POINTER_SIZE_ALIGN(String::kSize);
  static const int kSecondOffset = kFirstOffset + kPointerSize;
  static const int kSize = kSecondOffset + kPointerSize;

  // Minimum length for a cons string.
  static const int kMinLength = 13;

  typedef FixedBodyDescriptor<kFirstOffset, kSecondOffset + kPointerSize, kSize>
          BodyDescriptor;

  DECLARE_VERIFIER(ConsString)

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(ConsString);
};


// The Sliced String class describes strings that are substrings of another
// sequential string.  The motivation is to save time and memory when creating
// a substring.  A Sliced String is described as a pointer to the parent,
// the offset from the start of the parent string and the length.  Using
// a Sliced String therefore requires unpacking of the parent string and
// adding the offset to the start address.  A substring of a Sliced String
// are not nested since the double indirection is simplified when creating
// such a substring.
// Currently missing features are:
//  - handling externalized parent strings
//  - external strings as parent
//  - truncating sliced string to enable otherwise unneeded parent to be GC'ed.
class SlicedString: public String {
 public:
  inline String* parent();
  inline void set_parent(String* parent,
                         WriteBarrierMode mode = UPDATE_WRITE_BARRIER);
  inline int offset() const;
  inline void set_offset(int offset);

  // Dispatched behavior.
  V8_EXPORT_PRIVATE uint16_t SlicedStringGet(int index);

  DECLARE_CAST(SlicedString)

  // Layout description.
  static const int kParentOffset = POINTER_SIZE_ALIGN(String::kSize);
  static const int kOffsetOffset = kParentOffset + kPointerSize;
  static const int kSize = kOffsetOffset + kPointerSize;

  // Minimum length for a sliced string.
  static const int kMinLength = 13;

  typedef FixedBodyDescriptor<kParentOffset,
                              kOffsetOffset + kPointerSize, kSize>
          BodyDescriptor;

  DECLARE_VERIFIER(SlicedString)

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(SlicedString);
};


// The ExternalString class describes string values that are backed by
// a string resource that lies outside the V8 heap.  ExternalStrings
// consist of the length field common to all strings, a pointer to the
// external resource.  It is important to ensure (externally) that the
// resource is not deallocated while the ExternalString is live in the
// V8 heap.
//
// The API expects that all ExternalStrings are created through the
// API.  Therefore, ExternalStrings should not be used internally.
class ExternalString: public String {
 public:
  DECLARE_CAST(ExternalString)

  // Layout description.
  static const int kResourceOffset = POINTER_SIZE_ALIGN(String::kSize);
  static const int kShortSize = kResourceOffset + kPointerSize;
  static const int kResourceDataOffset = kResourceOffset + kPointerSize;
  static const int kSize = kResourceDataOffset + kPointerSize;

  // Return whether external string is short (data pointer is not cached).
  inline bool is_short();

  STATIC_ASSERT(kResourceOffset == Internals::kStringResourceOffset);

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(ExternalString);
};


// The ExternalOneByteString class is an external string backed by an
// one-byte string.
class ExternalOneByteString : public ExternalString {
 public:
  static const bool kHasOneByteEncoding = true;

  typedef v8::String::ExternalOneByteStringResource Resource;

  // The underlying resource.
  inline const Resource* resource();
  inline void set_resource(const Resource* buffer);

  // Update the pointer cache to the external character array.
  // The cached pointer is always valid, as the external character array does =
  // not move during lifetime.  Deserialization is the only exception, after
  // which the pointer cache has to be refreshed.
  inline void update_data_cache();

  inline const uint8_t* GetChars();

  // Dispatched behavior.
  inline uint16_t ExternalOneByteStringGet(int index);

  DECLARE_CAST(ExternalOneByteString)

  class BodyDescriptor;

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(ExternalOneByteString);
};


// The ExternalTwoByteString class is an external string backed by a UTF-16
// encoded string.
class ExternalTwoByteString: public ExternalString {
 public:
  static const bool kHasOneByteEncoding = false;

  typedef v8::String::ExternalStringResource Resource;

  // The underlying string resource.
  inline const Resource* resource();
  inline void set_resource(const Resource* buffer);

  // Update the pointer cache to the external character array.
  // The cached pointer is always valid, as the external character array does =
  // not move during lifetime.  Deserialization is the only exception, after
  // which the pointer cache has to be refreshed.
  inline void update_data_cache();

  inline const uint16_t* GetChars();

  // Dispatched behavior.
  inline uint16_t ExternalTwoByteStringGet(int index);

  // For regexp code.
  inline const uint16_t* ExternalTwoByteStringGetData(unsigned start);

  DECLARE_CAST(ExternalTwoByteString)

  class BodyDescriptor;

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(ExternalTwoByteString);
};


// Utility superclass for stack-allocated objects that must be updated
// on gc.  It provides two ways for the gc to update instances, either
// iterating or updating after gc.
class Relocatable BASE_EMBEDDED {
 public:
  explicit inline Relocatable(Isolate* isolate);
  inline virtual ~Relocatable();
  virtual void IterateInstance(ObjectVisitor* v) { }
  virtual void PostGarbageCollection() { }

  static void PostGarbageCollectionProcessing(Isolate* isolate);
  static int ArchiveSpacePerThread();
  static char* ArchiveState(Isolate* isolate, char* to);
  static char* RestoreState(Isolate* isolate, char* from);
  static void Iterate(Isolate* isolate, ObjectVisitor* v);
  static void Iterate(ObjectVisitor* v, Relocatable* top);
  static char* Iterate(ObjectVisitor* v, char* t);

 private:
  Isolate* isolate_;
  Relocatable* prev_;
};


// A flat string reader provides random access to the contents of a
// string independent of the character width of the string.  The handle
// must be valid as long as the reader is being used.
class FlatStringReader : public Relocatable {
 public:
  FlatStringReader(Isolate* isolate, Handle<String> str);
  FlatStringReader(Isolate* isolate, Vector<const char> input);
  void PostGarbageCollection();
  inline uc32 Get(int index);
  template <typename Char>
  inline Char Get(int index);
  int length() { return length_; }
 private:
  String** str_;
  bool is_one_byte_;
  int length_;
  const void* start_;
};


// This maintains an off-stack representation of the stack frames required
// to traverse a ConsString, allowing an entirely iterative and restartable
// traversal of the entire string
class ConsStringIterator {
 public:
  inline ConsStringIterator() {}
  inline explicit ConsStringIterator(ConsString* cons_string, int offset = 0) {
    Reset(cons_string, offset);
  }
  inline void Reset(ConsString* cons_string, int offset = 0) {
    depth_ = 0;
    // Next will always return NULL.
    if (cons_string == NULL) return;
    Initialize(cons_string, offset);
  }
  // Returns NULL when complete.
  inline String* Next(int* offset_out) {
    *offset_out = 0;
    if (depth_ == 0) return NULL;
    return Continue(offset_out);
  }

 private:
  static const int kStackSize = 32;
  // Use a mask instead of doing modulo operations for stack wrapping.
  static const int kDepthMask = kStackSize-1;
  STATIC_ASSERT(IS_POWER_OF_TWO(kStackSize));
  static inline int OffsetForDepth(int depth);

  inline void PushLeft(ConsString* string);
  inline void PushRight(ConsString* string);
  inline void AdjustMaximumDepth();
  inline void Pop();
  inline bool StackBlown() { return maximum_depth_ - depth_ == kStackSize; }
  void Initialize(ConsString* cons_string, int offset);
  String* Continue(int* offset_out);
  String* NextLeaf(bool* blew_stack);
  String* Search(int* offset_out);

  // Stack must always contain only frames for which right traversal
  // has not yet been performed.
  ConsString* frames_[kStackSize];
  ConsString* root_;
  int depth_;
  int maximum_depth_;
  int consumed_;
  DISALLOW_COPY_AND_ASSIGN(ConsStringIterator);
};


class StringCharacterStream {
 public:
  inline StringCharacterStream(String* string,
                               int offset = 0);
  inline uint16_t GetNext();
  inline bool HasMore();
  inline void Reset(String* string, int offset = 0);
  inline void VisitOneByteString(const uint8_t* chars, int length);
  inline void VisitTwoByteString(const uint16_t* chars, int length);

 private:
  ConsStringIterator iter_;
  bool is_one_byte_;
  union {
    const uint8_t* buffer8_;
    const uint16_t* buffer16_;
  };
  const uint8_t* end_;
  DISALLOW_COPY_AND_ASSIGN(StringCharacterStream);
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

  DECLARE_CAST(Oddball)

  // Dispatched behavior.
  DECLARE_VERIFIER(Oddball)

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
  static const byte kNotBooleanMask = ~1;
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

  DECLARE_CAST(Cell)

  static inline Cell* FromValueAddress(Address value) {
    Object* result = FromAddress(value - kValueOffset);
    return static_cast<Cell*>(result);
  }

  inline Address ValueAddress() {
    return address() + kValueOffset;
  }

  // Dispatched behavior.
  DECLARE_PRINTER(Cell)
  DECLARE_VERIFIER(Cell)

  // Layout description.
  static const int kValueOffset = HeapObject::kHeaderSize;
  static const int kSize = kValueOffset + kPointerSize;

  typedef FixedBodyDescriptor<kValueOffset,
                              kValueOffset + kPointerSize,
                              kSize> BodyDescriptor;

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(Cell);
};


class PropertyCell : public HeapObject {
 public:
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

  DECLARE_CAST(PropertyCell)

  // Dispatched behavior.
  DECLARE_PRINTER(PropertyCell)
  DECLARE_VERIFIER(PropertyCell)

  // Layout description.
  static const int kDetailsOffset = HeapObject::kHeaderSize;
  static const int kValueOffset = kDetailsOffset + kPointerSize;
  static const int kDependentCodeOffset = kValueOffset + kPointerSize;
  static const int kSize = kDependentCodeOffset + kPointerSize;

  typedef FixedBodyDescriptor<kValueOffset,
                              kSize,
                              kSize> BodyDescriptor;

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

  DECLARE_CAST(WeakCell)

  DECLARE_PRINTER(WeakCell)
  DECLARE_VERIFIER(WeakCell)

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

  DECLARE_CAST(JSProxy)

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
  DECLARE_PRINTER(JSProxy)
  DECLARE_VERIFIER(JSProxy)

  // Layout description.
  static const int kTargetOffset = JSReceiver::kHeaderSize;
  static const int kHandlerOffset = kTargetOffset + kPointerSize;
  static const int kHashOffset = kHandlerOffset + kPointerSize;
  static const int kSize = kHashOffset + kPointerSize;

  typedef FixedBodyDescriptor<JSReceiver::kPropertiesOffset, kSize, kSize>
      BodyDescriptor;

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
class JSSet : public JSCollection {
 public:
  DECLARE_CAST(JSSet)

  static void Initialize(Handle<JSSet> set, Isolate* isolate);
  static void Clear(Handle<JSSet> set);

  // Dispatched behavior.
  DECLARE_PRINTER(JSSet)
  DECLARE_VERIFIER(JSSet)

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(JSSet);
};


// The JSMap describes EcmaScript Harmony maps
class JSMap : public JSCollection {
 public:
  DECLARE_CAST(JSMap)

  static void Initialize(Handle<JSMap> map, Isolate* isolate);
  static void Clear(Handle<JSMap> map);

  // Dispatched behavior.
  DECLARE_PRINTER(JSMap)
  DECLARE_VERIFIER(JSMap)

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(JSMap);
};

class JSArrayIterator : public JSObject {
 public:
  DECLARE_PRINTER(JSArrayIterator)
  DECLARE_VERIFIER(JSArrayIterator)

  DECLARE_CAST(JSArrayIterator)

  // [object]: the [[IteratedObject]] internal field.
  DECL_ACCESSORS(object, Object)

  // [index]: The [[ArrayIteratorNextIndex]] internal field.
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

class JSStringIterator : public JSObject {
 public:
  // Dispatched behavior.
  DECLARE_PRINTER(JSStringIterator)
  DECLARE_VERIFIER(JSStringIterator)

  DECLARE_CAST(JSStringIterator)

  // [string]: the [[IteratedString]] internal field.
  DECL_ACCESSORS(string, String)

  // [index]: The [[StringIteratorNextIndex]] internal field.
  inline int index() const;
  inline void set_index(int value);

  static const int kStringOffset = JSObject::kHeaderSize;
  static const int kNextIndexOffset = kStringOffset + kPointerSize;
  static const int kSize = kNextIndexOffset + kPointerSize;

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(JSStringIterator);
};

// OrderedHashTableIterator is an iterator that iterates over the keys and
// values of an OrderedHashTable.
//
// The iterator has a reference to the underlying OrderedHashTable data,
// [table], as well as the current [index] the iterator is at.
//
// When the OrderedHashTable is rehashed it adds a reference from the old table
// to the new table as well as storing enough data about the changes so that the
// iterator [index] can be adjusted accordingly.
//
// When the [Next] result from the iterator is requested, the iterator checks if
// there is a newer table that it needs to transition to.
template<class Derived, class TableType>
class OrderedHashTableIterator: public JSObject {
 public:
  // [table]: the backing hash table mapping keys to values.
  DECL_ACCESSORS(table, Object)

  // [index]: The index into the data table.
  DECL_ACCESSORS(index, Object)

  // [kind]: The kind of iteration this is. One of the [Kind] enum values.
  DECL_ACCESSORS(kind, Object)

#ifdef OBJECT_PRINT
  void OrderedHashTableIteratorPrint(std::ostream& os);  // NOLINT
#endif

  static const int kTableOffset = JSObject::kHeaderSize;
  static const int kIndexOffset = kTableOffset + kPointerSize;
  static const int kKindOffset = kIndexOffset + kPointerSize;
  static const int kSize = kKindOffset + kPointerSize;

  enum Kind {
    kKindKeys = 1,
    kKindValues = 2,
    kKindEntries = 3
  };

  // Whether the iterator has more elements. This needs to be called before
  // calling |CurrentKey| and/or |CurrentValue|.
  bool HasMore();

  // Move the index forward one.
  void MoveNext() {
    set_index(Smi::FromInt(Smi::cast(index())->value() + 1));
  }

  // Populates the array with the next key and value and then moves the iterator
  // forward.
  // This returns the |kind| or 0 if the iterator is already at the end.
  Smi* Next(JSArray* value_array);

  // Returns the current key of the iterator. This should only be called when
  // |HasMore| returns true.
  inline Object* CurrentKey();

 private:
  // Transitions the iterator to the non obsolete backing store. This is a NOP
  // if the [table] is not obsolete.
  void Transition();

  DISALLOW_IMPLICIT_CONSTRUCTORS(OrderedHashTableIterator);
};


class JSSetIterator: public OrderedHashTableIterator<JSSetIterator,
                                                     OrderedHashSet> {
 public:
  // Dispatched behavior.
  DECLARE_PRINTER(JSSetIterator)
  DECLARE_VERIFIER(JSSetIterator)

  DECLARE_CAST(JSSetIterator)

  // Called by |Next| to populate the array. This allows the subclasses to
  // populate the array differently.
  inline void PopulateValueArray(FixedArray* array);

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(JSSetIterator);
};


class JSMapIterator: public OrderedHashTableIterator<JSMapIterator,
                                                     OrderedHashMap> {
 public:
  // Dispatched behavior.
  DECLARE_PRINTER(JSMapIterator)
  DECLARE_VERIFIER(JSMapIterator)

  DECLARE_CAST(JSMapIterator)

  // Called by |Next| to populate the array. This allows the subclasses to
  // populate the array differently.
  inline void PopulateValueArray(FixedArray* array);

 private:
  // Returns the current value of the iterator. This should only be called when
  // |HasMore| returns true.
  inline Object* CurrentValue();

  DISALLOW_IMPLICIT_CONSTRUCTORS(JSMapIterator);
};


// Base class for both JSWeakMap and JSWeakSet
class JSWeakCollection: public JSObject {
 public:
  DECLARE_CAST(JSWeakCollection)

  // [table]: the backing hash table mapping keys to values.
  DECL_ACCESSORS(table, Object)

  // [next]: linked list of encountered weak maps during GC.
  DECL_ACCESSORS(next, Object)

  static void Initialize(Handle<JSWeakCollection> collection, Isolate* isolate);
  static void Set(Handle<JSWeakCollection> collection, Handle<Object> key,
                  Handle<Object> value, int32_t hash);
  static bool Delete(Handle<JSWeakCollection> collection, Handle<Object> key,
                     int32_t hash);

  static const int kTableOffset = JSObject::kHeaderSize;
  static const int kNextOffset = kTableOffset + kPointerSize;
  static const int kSize = kNextOffset + kPointerSize;

  // Visiting policy defines whether the table and next collection fields
  // should be visited or not.
  enum BodyVisitingPolicy { kVisitStrong, kVisitWeak };

  // Iterates the function object according to the visiting policy.
  template <BodyVisitingPolicy>
  class BodyDescriptorImpl;

  // Visit the whole object.
  typedef BodyDescriptorImpl<kVisitStrong> BodyDescriptor;

  // Don't visit table and next collection fields.
  typedef BodyDescriptorImpl<kVisitWeak> BodyDescriptorWeak;

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(JSWeakCollection);
};


// The JSWeakMap describes EcmaScript Harmony weak maps
class JSWeakMap: public JSWeakCollection {
 public:
  DECLARE_CAST(JSWeakMap)

  // Dispatched behavior.
  DECLARE_PRINTER(JSWeakMap)
  DECLARE_VERIFIER(JSWeakMap)

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(JSWeakMap);
};


// The JSWeakSet describes EcmaScript Harmony weak sets
class JSWeakSet: public JSWeakCollection {
 public:
  DECLARE_CAST(JSWeakSet)

  // Dispatched behavior.
  DECLARE_PRINTER(JSWeakSet)
  DECLARE_VERIFIER(JSWeakSet)

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

  inline uint32_t bit_field() const;
  inline void set_bit_field(uint32_t bits);

  inline bool is_external();
  inline void set_is_external(bool value);

  inline bool is_neuterable();
  inline void set_is_neuterable(bool value);

  inline bool was_neutered();
  inline void set_was_neutered(bool value);

  inline bool is_shared();
  inline void set_is_shared(bool value);

  inline bool has_guard_region();
  inline void set_has_guard_region(bool value);

  DECLARE_CAST(JSArrayBuffer)

  void Neuter();

  V8_EXPORT_PRIVATE static void Setup(
      Handle<JSArrayBuffer> array_buffer, Isolate* isolate, bool is_external,
      void* data, size_t allocated_length,
      SharedFlag shared = SharedFlag::kNotShared);

  static bool SetupAllocatingData(Handle<JSArrayBuffer> array_buffer,
                                  Isolate* isolate, size_t allocated_length,
                                  bool initialize = true,
                                  SharedFlag shared = SharedFlag::kNotShared);

  // Dispatched behavior.
  DECLARE_PRINTER(JSArrayBuffer)
  DECLARE_VERIFIER(JSArrayBuffer)

  static const int kByteLengthOffset = JSObject::kHeaderSize;
  static const int kBackingStoreOffset = kByteLengthOffset + kPointerSize;
  static const int kBitFieldSlot = kBackingStoreOffset + kPointerSize;
#if V8_TARGET_LITTLE_ENDIAN || !V8_HOST_ARCH_64_BIT
  static const int kBitFieldOffset = kBitFieldSlot;
#else
  static const int kBitFieldOffset = kBitFieldSlot + kIntSize;
#endif
  static const int kSize = kBitFieldSlot + kPointerSize;

  static const int kSizeWithInternalFields =
      kSize + v8::ArrayBuffer::kInternalFieldCount * kPointerSize;

  // Iterates all fields in the object including internal ones except
  // kBackingStoreOffset and kBitFieldSlot.
  class BodyDescriptor;

  class IsExternal : public BitField<bool, 1, 1> {};
  class IsNeuterable : public BitField<bool, 2, 1> {};
  class WasNeutered : public BitField<bool, 3, 1> {};
  class IsShared : public BitField<bool, 4, 1> {};
  class HasGuardRegion : public BitField<bool, 5, 1> {};

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

  DECLARE_CAST(JSArrayBufferView)

  DECLARE_VERIFIER(JSArrayBufferView)

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

  DECLARE_CAST(JSTypedArray)

  ExternalArrayType type();
  V8_EXPORT_PRIVATE size_t element_size();

  Handle<JSArrayBuffer> GetBuffer();

  // Dispatched behavior.
  DECLARE_PRINTER(JSTypedArray)
  DECLARE_VERIFIER(JSTypedArray)

  static const int kLengthOffset = kViewSize + kPointerSize;
  static const int kSize = kLengthOffset + kPointerSize;

  static const int kSizeWithInternalFields =
      kSize + v8::ArrayBufferView::kInternalFieldCount * kPointerSize;

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
  DECLARE_CAST(JSDataView)

  // Dispatched behavior.
  DECLARE_PRINTER(JSDataView)
  DECLARE_VERIFIER(JSDataView)

  static const int kSize = kViewSize;

  static const int kSizeWithInternalFields =
      kSize + v8::ArrayBufferView::kInternalFieldCount * kPointerSize;

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(JSDataView);
};


// Foreign describes objects pointing from JavaScript to C structures.
class Foreign: public HeapObject {
 public:
  // [address]: field containing the address.
  inline Address foreign_address();
  inline void set_foreign_address(Address value);

  DECLARE_CAST(Foreign)

  // Dispatched behavior.
  DECLARE_PRINTER(Foreign)
  DECLARE_VERIFIER(Foreign)

  // Layout description.

  static const int kForeignAddressOffset = HeapObject::kHeaderSize;
  static const int kSize = kForeignAddressOffset + kPointerSize;

  STATIC_ASSERT(kForeignAddressOffset == Internals::kForeignAddressOffset);

  class BodyDescriptor;

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

  DECLARE_CAST(JSArray)

  // Dispatched behavior.
  DECLARE_PRINTER(JSArray)
  DECLARE_VERIFIER(JSArray)

  // Number of element slots to pre-allocate for an empty array.
  static const int kPreallocatedArrayElements = 4;

  // Layout description.
  static const int kLengthOffset = JSObject::kHeaderSize;
  static const int kSize = kLengthOffset + kPointerSize;

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
  DECLARE_PRINTER(AccessorInfo)

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

  DECLARE_CAST(AccessorInfo)

  // Dispatched behavior.
  DECLARE_VERIFIER(AccessorInfo)

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

  DECLARE_CAST(AccessorPair)

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
  DECLARE_PRINTER(AccessorPair)
  DECLARE_VERIFIER(AccessorPair)

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

  DECLARE_CAST(AccessCheckInfo)

  // Dispatched behavior.
  DECLARE_PRINTER(AccessCheckInfo)
  DECLARE_VERIFIER(AccessCheckInfo)

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

  DECLARE_CAST(InterceptorInfo)

  // Dispatched behavior.
  DECLARE_PRINTER(InterceptorInfo)
  DECLARE_VERIFIER(InterceptorInfo)

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


class CallHandlerInfo: public Struct {
 public:
  DECL_ACCESSORS(callback, Object)
  DECL_ACCESSORS(data, Object)
  DECL_ACCESSORS(fast_handler, Object)

  DECLARE_CAST(CallHandlerInfo)

  // Dispatched behavior.
  DECLARE_PRINTER(CallHandlerInfo)
  DECLARE_VERIFIER(CallHandlerInfo)

  static const int kCallbackOffset = HeapObject::kHeaderSize;
  static const int kDataOffset = kCallbackOffset + kPointerSize;
  static const int kFastHandlerOffset = kDataOffset + kPointerSize;
  static const int kSize = kFastHandlerOffset + kPointerSize;

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

  DECLARE_VERIFIER(TemplateInfo)

  DECLARE_CAST(TemplateInfo)

  static const int kTagOffset = HeapObject::kHeaderSize;
  static const int kSerialNumberOffset = kTagOffset + kPointerSize;
  static const int kNumberOfProperties = kSerialNumberOffset + kPointerSize;
  static const int kPropertyListOffset = kNumberOfProperties + kPointerSize;
  static const int kPropertyAccessorsOffset =
      kPropertyListOffset + kPointerSize;
  static const int kHeaderSize = kPropertyAccessorsOffset + kPointerSize;

  static const int kFastTemplateInstantiationsCacheSize = 1 * KB;

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(TemplateInfo);
};


class FunctionTemplateInfo: public TemplateInfo {
 public:
  DECL_ACCESSORS(call_code, Object)
  DECL_ACCESSORS(prototype_template, Object)
  DECL_ACCESSORS(prototype_provider_template, Object)
  DECL_ACCESSORS(parent_template, Object)
  DECL_ACCESSORS(named_property_handler, Object)
  DECL_ACCESSORS(indexed_property_handler, Object)
  DECL_ACCESSORS(instance_template, Object)
  DECL_ACCESSORS(class_name, Object)
  DECL_ACCESSORS(signature, Object)
  DECL_ACCESSORS(instance_call_handler, Object)
  DECL_ACCESSORS(access_check_info, Object)
  DECL_ACCESSORS(shared_function_info, Object)
  DECL_ACCESSORS(js_function, Object)
  DECL_INT_ACCESSORS(flag)

  inline int length() const;
  inline void set_length(int value);

  // Following properties use flag bits.
  DECL_BOOLEAN_ACCESSORS(hidden_prototype)
  DECL_BOOLEAN_ACCESSORS(undetectable)
  // If the bit is set, object instances created by this function
  // requires access check.
  DECL_BOOLEAN_ACCESSORS(needs_access_check)
  DECL_BOOLEAN_ACCESSORS(read_only_prototype)
  DECL_BOOLEAN_ACCESSORS(remove_prototype)
  DECL_BOOLEAN_ACCESSORS(do_not_cache)
  DECL_BOOLEAN_ACCESSORS(accept_any_receiver)

  DECL_ACCESSORS(cached_property_name, Object)

  DECLARE_CAST(FunctionTemplateInfo)

  // Dispatched behavior.
  DECLARE_PRINTER(FunctionTemplateInfo)
  DECLARE_VERIFIER(FunctionTemplateInfo)

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
      Isolate* isolate, Handle<FunctionTemplateInfo> info);
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
  DECL_INT_ACCESSORS(internal_field_count)
  DECL_BOOLEAN_ACCESSORS(immutable_proto)

  DECLARE_CAST(ObjectTemplateInfo)

  // Dispatched behavior.
  DECLARE_PRINTER(ObjectTemplateInfo)
  DECLARE_VERIFIER(ObjectTemplateInfo)

  static const int kConstructorOffset = TemplateInfo::kHeaderSize;
  // LSB is for immutable_proto, higher bits for internal_field_count
  static const int kDataOffset = kConstructorOffset + kPointerSize;
  static const int kSize = kDataOffset + kPointerSize;

  // Starting from given object template's constructor walk up the inheritance
  // chain till a function template that has an instance template is found.
  inline ObjectTemplateInfo* GetParent(Isolate* isolate);

 private:
  class IsImmutablePrototype : public BitField<bool, 0, 1> {};
  class InternalFieldCount
      : public BitField<int, IsImmutablePrototype::kNext, 29> {};
};


// The DebugInfo class holds additional information for a function being
// debugged.
class DebugInfo: public Struct {
 public:
  // The shared function info for the source being debugged.
  DECL_ACCESSORS(shared, SharedFunctionInfo)

  DECL_ACCESSORS(debug_bytecode_array, Object)
  // Fixed array holding status information for each active break point.
  DECL_ACCESSORS(break_points, FixedArray)

  // Check if there is a break point at a source position.
  bool HasBreakPoint(int source_position);
  // Attempt to clear a break point. Return true if successful.
  static bool ClearBreakPoint(Handle<DebugInfo> debug_info,
                              Handle<Object> break_point_object);
  // Set a break point.
  static void SetBreakPoint(Handle<DebugInfo> debug_info, int source_position,
                            Handle<Object> break_point_object);
  // Get the break point objects for a source position.
  Handle<Object> GetBreakPointObjects(int source_position);
  // Find the break point info holding this break point object.
  static Handle<Object> FindBreakPointInfo(Handle<DebugInfo> debug_info,
                                           Handle<Object> break_point_object);
  // Get the number of break points for this function.
  int GetBreakPointCount();

  static Smi* uninitialized() { return Smi::kZero; }

  inline bool HasDebugBytecodeArray();
  inline bool HasDebugCode();

  inline BytecodeArray* OriginalBytecodeArray();
  inline BytecodeArray* DebugBytecodeArray();
  inline Code* DebugCode();

  DECLARE_CAST(DebugInfo)

  // Dispatched behavior.
  DECLARE_PRINTER(DebugInfo)
  DECLARE_VERIFIER(DebugInfo)

  static const int kSharedFunctionInfoIndex = Struct::kHeaderSize;
  static const int kDebugBytecodeArrayIndex =
      kSharedFunctionInfoIndex + kPointerSize;
  static const int kBreakPointsStateIndex =
      kDebugBytecodeArrayIndex + kPointerSize;
  static const int kSize = kBreakPointsStateIndex + kPointerSize;

  static const int kEstimatedNofBreakPointsInFunction = 4;

 private:
  // Get the break point info object for a source position.
  Object* GetBreakPointInfo(int source_position);

  DISALLOW_IMPLICIT_CONSTRUCTORS(DebugInfo);
};


// The BreakPointInfo class holds information for break points set in a
// function. The DebugInfo object holds a BreakPointInfo object for each code
// position with one or more break points.
class BreakPointInfo: public Struct {
 public:
  // The position in the source for the break position.
  DECL_INT_ACCESSORS(source_position)
  // List of related JavaScript break points.
  DECL_ACCESSORS(break_point_objects, Object)

  // Removes a break point.
  static void ClearBreakPoint(Handle<BreakPointInfo> info,
                              Handle<Object> break_point_object);
  // Set a break point.
  static void SetBreakPoint(Handle<BreakPointInfo> info,
                            Handle<Object> break_point_object);
  // Check if break point info has this break point object.
  static bool HasBreakPointObject(Handle<BreakPointInfo> info,
                                  Handle<Object> break_point_object);
  // Get the number of break points for this code offset.
  int GetBreakPointCount();

  int GetStatementPosition(Handle<DebugInfo> debug_info);

  DECLARE_CAST(BreakPointInfo)

  // Dispatched behavior.
  DECLARE_PRINTER(BreakPointInfo)
  DECLARE_VERIFIER(BreakPointInfo)

  static const int kSourcePositionIndex = Struct::kHeaderSize;
  static const int kBreakPointObjectsIndex =
      kSourcePositionIndex + kPointerSize;
  static const int kSize = kBreakPointObjectsIndex + kPointerSize;

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(BreakPointInfo);
};


#define VISITOR_SYNCHRONIZATION_TAGS_LIST(V)                               \
  V(kStringTable, "string_table", "(Internalized strings)")                \
  V(kExternalStringsTable, "external_strings_table", "(External strings)") \
  V(kStrongRootList, "strong_root_list", "(Strong roots)")                 \
  V(kSmiRootList, "smi_root_list", "(Smi roots)")                          \
  V(kBootstrapper, "bootstrapper", "(Bootstrapper)")                       \
  V(kTop, "top", "(Isolate)")                                              \
  V(kRelocatable, "relocatable", "(Relocatable)")                          \
  V(kDebug, "debug", "(Debugger)")                                         \
  V(kCompilationCache, "compilationcache", "(Compilation cache)")          \
  V(kHandleScope, "handlescope", "(Handle scope)")                         \
  V(kDispatchTable, "dispatchtable", "(Dispatch table)")                   \
  V(kBuiltins, "builtins", "(Builtins)")                                   \
  V(kGlobalHandles, "globalhandles", "(Global handles)")                   \
  V(kEternalHandles, "eternalhandles", "(Eternal handles)")                \
  V(kThreadManager, "threadmanager", "(Thread manager)")                   \
  V(kStrongRoots, "strong roots", "(Strong roots)")                        \
  V(kExtensions, "Extensions", "(Extensions)")

class VisitorSynchronization : public AllStatic {
 public:
#define DECLARE_ENUM(enum_item, ignore1, ignore2) enum_item,
  enum SyncTag {
    VISITOR_SYNCHRONIZATION_TAGS_LIST(DECLARE_ENUM)
    kNumberOfSyncTags
  };
#undef DECLARE_ENUM

  static const char* const kTags[kNumberOfSyncTags];
  static const char* const kTagNames[kNumberOfSyncTags];
};

// Abstract base class for visiting, and optionally modifying, the
// pointers contained in Objects. Used in GC and serialization/deserialization.
class ObjectVisitor BASE_EMBEDDED {
 public:
  virtual ~ObjectVisitor() {}

  // Visits a contiguous arrays of pointers in the half-open range
  // [start, end). Any or all of the values may be modified on return.
  virtual void VisitPointers(Object** start, Object** end) = 0;

  // Handy shorthand for visiting a single pointer.
  virtual void VisitPointer(Object** p) { VisitPointers(p, p + 1); }

  // Visit weak next_code_link in Code object.
  virtual void VisitNextCodeLink(Object** p) { VisitPointers(p, p + 1); }

  // To allow lazy clearing of inline caches the visitor has
  // a rich interface for iterating over Code objects..

  // Visits a code target in the instruction stream.
  virtual void VisitCodeTarget(RelocInfo* rinfo);

  // Visits a code entry in a JS function.
  virtual void VisitCodeEntry(Address entry_address);

  // Visits a global property cell reference in the instruction stream.
  virtual void VisitCell(RelocInfo* rinfo);

  // Visits a runtime entry in the instruction stream.
  virtual void VisitRuntimeEntry(RelocInfo* rinfo) {}

  // Visits the resource of an one-byte or two-byte string.
  virtual void VisitExternalOneByteString(
      v8::String::ExternalOneByteStringResource** resource) {}
  virtual void VisitExternalTwoByteString(
      v8::String::ExternalStringResource** resource) {}

  // Visits a debug call target in the instruction stream.
  virtual void VisitDebugTarget(RelocInfo* rinfo);

  // Visits the byte sequence in a function's prologue that contains information
  // about the code's age.
  virtual void VisitCodeAgeSequence(RelocInfo* rinfo);

  // Visit pointer embedded into a code object.
  virtual void VisitEmbeddedPointer(RelocInfo* rinfo);

  // Visits an external reference embedded into a code object.
  virtual void VisitExternalReference(RelocInfo* rinfo);

  // Visits an external reference.
  virtual void VisitExternalReference(Address* p) {}

  // Visits an (encoded) internal reference.
  virtual void VisitInternalReference(RelocInfo* rinfo) {}

  // Visits a handle that has an embedder-assigned class ID.
  virtual void VisitEmbedderReference(Object** p, uint16_t class_id) {}

  // Intended for serialization/deserialization checking: insert, or
  // check for the presence of, a tag at this position in the stream.
  // Also used for marking up GC roots in heap snapshots.
  virtual void Synchronize(VisitorSynchronization::SyncTag tag) {}
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
