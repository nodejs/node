// Copyright 2012 the V8 project authors. All rights reserved.
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
//       copyright notice, this list of conditions and the following
//       disclaimer in the documentation and/or other materials provided
//       with the distribution.
//     * Neither the name of Google Inc. nor the names of its
//       contributors may be used to endorse or promote products derived
//       from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#ifndef V8_OBJECTS_H_
#define V8_OBJECTS_H_

#include "allocation.h"
#include "assert-scope.h"
#include "builtins.h"
#include "elements-kind.h"
#include "flags.h"
#include "list.h"
#include "property-details.h"
#include "smart-pointers.h"
#include "unicode-inl.h"
#if V8_TARGET_ARCH_A64
#include "a64/constants-a64.h"
#elif V8_TARGET_ARCH_ARM
#include "arm/constants-arm.h"
#elif V8_TARGET_ARCH_MIPS
#include "mips/constants-mips.h"
#endif
#include "v8checks.h"
#include "zone.h"


//
// Most object types in the V8 JavaScript are described in this file.
//
// Inheritance hierarchy:
// - MaybeObject    (an object or a failure)
//   - Failure      (immediate for marking failed operation)
//   - Object
//     - Smi          (immediate small integer)
//     - HeapObject   (superclass for everything allocated in the heap)
//       - JSReceiver  (suitable for property access)
//         - JSObject
//           - JSArray
//           - JSArrayBuffer
//           - JSArrayBufferView
//             - JSTypedArray
//             - JSDataView
//           - JSSet
//           - JSMap
//           - JSWeakCollection
//             - JSWeakMap
//             - JSWeakSet
//           - JSRegExp
//           - JSFunction
//           - JSGeneratorObject
//           - JSModule
//           - GlobalObject
//             - JSGlobalObject
//             - JSBuiltinsObject
//           - JSGlobalProxy
//           - JSValue
//             - JSDate
//           - JSMessageObject
//         - JSProxy
//           - JSFunctionProxy
//       - FixedArrayBase
//         - ByteArray
//         - FixedArray
//           - DescriptorArray
//           - HashTable
//             - Dictionary
//             - StringTable
//             - CompilationCacheTable
//             - CodeCacheHashTable
//             - MapCache
//           - Context
//           - JSFunctionResultCache
//           - ScopeInfo
//           - TransitionArray
//         - FixedDoubleArray
//         - ExternalArray
//           - ExternalUint8ClampedArray
//           - ExternalInt8Array
//           - ExternalUint8Array
//           - ExternalInt16Array
//           - ExternalUint16Array
//           - ExternalInt32Array
//           - ExternalUint32Array
//           - ExternalFloat32Array
//       - Name
//         - String
//           - SeqString
//             - SeqOneByteString
//             - SeqTwoByteString
//           - SlicedString
//           - ConsString
//           - ExternalString
//             - ExternalAsciiString
//             - ExternalTwoByteString
//           - InternalizedString
//             - SeqInternalizedString
//               - SeqOneByteInternalizedString
//               - SeqTwoByteInternalizedString
//             - ConsInternalizedString
//             - ExternalInternalizedString
//               - ExternalAsciiInternalizedString
//               - ExternalTwoByteInternalizedString
//         - Symbol
//       - HeapNumber
//       - Cell
//         - PropertyCell
//       - Code
//       - Map
//       - Oddball
//       - Foreign
//       - SharedFunctionInfo
//       - Struct
//         - Box
//         - DeclaredAccessorDescriptor
//         - AccessorInfo
//           - DeclaredAccessorInfo
//           - ExecutableAccessorInfo
//         - AccessorPair
//         - AccessCheckInfo
//         - InterceptorInfo
//         - CallHandlerInfo
//         - TemplateInfo
//           - FunctionTemplateInfo
//           - ObjectTemplateInfo
//         - Script
//         - SignatureInfo
//         - TypeSwitchInfo
//         - DebugInfo
//         - BreakPointInfo
//         - CodeCache
//
// Formats of Object*:
//  Smi:        [31 bit signed int] 0
//  HeapObject: [32 bit direct pointer] (4 byte aligned) | 01
//  Failure:    [30 bit signed int] 11

namespace v8 {
namespace internal {

enum KeyedAccessStoreMode {
  STANDARD_STORE,
  STORE_TRANSITION_SMI_TO_OBJECT,
  STORE_TRANSITION_SMI_TO_DOUBLE,
  STORE_TRANSITION_DOUBLE_TO_OBJECT,
  STORE_TRANSITION_HOLEY_SMI_TO_OBJECT,
  STORE_TRANSITION_HOLEY_SMI_TO_DOUBLE,
  STORE_TRANSITION_HOLEY_DOUBLE_TO_OBJECT,
  STORE_AND_GROW_NO_TRANSITION,
  STORE_AND_GROW_TRANSITION_SMI_TO_OBJECT,
  STORE_AND_GROW_TRANSITION_SMI_TO_DOUBLE,
  STORE_AND_GROW_TRANSITION_DOUBLE_TO_OBJECT,
  STORE_AND_GROW_TRANSITION_HOLEY_SMI_TO_OBJECT,
  STORE_AND_GROW_TRANSITION_HOLEY_SMI_TO_DOUBLE,
  STORE_AND_GROW_TRANSITION_HOLEY_DOUBLE_TO_OBJECT,
  STORE_NO_TRANSITION_IGNORE_OUT_OF_BOUNDS,
  STORE_NO_TRANSITION_HANDLE_COW
};


enum ContextualMode {
  NOT_CONTEXTUAL,
  CONTEXTUAL
};


static const int kGrowICDelta = STORE_AND_GROW_NO_TRANSITION -
    STANDARD_STORE;
STATIC_ASSERT(STANDARD_STORE == 0);
STATIC_ASSERT(kGrowICDelta ==
              STORE_AND_GROW_TRANSITION_SMI_TO_OBJECT -
              STORE_TRANSITION_SMI_TO_OBJECT);
STATIC_ASSERT(kGrowICDelta ==
              STORE_AND_GROW_TRANSITION_SMI_TO_DOUBLE -
              STORE_TRANSITION_SMI_TO_DOUBLE);
STATIC_ASSERT(kGrowICDelta ==
              STORE_AND_GROW_TRANSITION_DOUBLE_TO_OBJECT -
              STORE_TRANSITION_DOUBLE_TO_OBJECT);


static inline KeyedAccessStoreMode GetGrowStoreMode(
    KeyedAccessStoreMode store_mode) {
  if (store_mode < STORE_AND_GROW_NO_TRANSITION) {
    store_mode = static_cast<KeyedAccessStoreMode>(
        static_cast<int>(store_mode) + kGrowICDelta);
  }
  return store_mode;
}


static inline bool IsTransitionStoreMode(KeyedAccessStoreMode store_mode) {
  return store_mode > STANDARD_STORE &&
      store_mode <= STORE_AND_GROW_TRANSITION_HOLEY_DOUBLE_TO_OBJECT &&
      store_mode != STORE_AND_GROW_NO_TRANSITION;
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
      store_mode <= STORE_AND_GROW_TRANSITION_HOLEY_DOUBLE_TO_OBJECT;
}


// Setter that skips the write barrier if mode is SKIP_WRITE_BARRIER.
enum WriteBarrierMode { SKIP_WRITE_BARRIER, UPDATE_WRITE_BARRIER };


// Indicates whether a value can be loaded as a constant.
enum StoreMode {
  ALLOW_AS_CONSTANT,
  FORCE_FIELD
};


// PropertyNormalizationMode is used to specify whether to keep
// inobject properties when normalizing properties of a JSObject.
enum PropertyNormalizationMode {
  CLEAR_INOBJECT_PROPERTIES,
  KEEP_INOBJECT_PROPERTIES
};


// NormalizedMapSharingMode is used to specify whether a map may be shared
// by different objects with normalized properties.
enum NormalizedMapSharingMode {
  UNIQUE_NORMALIZED_MAP,
  SHARED_NORMALIZED_MAP
};


// Indicates whether transitions can be added to a source map or not.
enum TransitionFlag {
  INSERT_TRANSITION,
  OMIT_TRANSITION
};


enum DebugExtraICState {
  DEBUG_BREAK,
  DEBUG_PREPARE_STEP_IN
};


// Indicates whether the transition is simple: the target map of the transition
// either extends the current map with a new property, or it modifies the
// property that was added last to the current map.
enum SimpleTransitionFlag {
  SIMPLE_TRANSITION,
  FULL_TRANSITION
};


// Indicates whether we are only interested in the descriptors of a particular
// map, or in all descriptors in the descriptor array.
enum DescriptorFlag {
  ALL_DESCRIPTORS,
  OWN_DESCRIPTORS
};

// The GC maintains a bit of information, the MarkingParity, which toggles
// from odd to even and back every time marking is completed. Incremental
// marking can visit an object twice during a marking phase, so algorithms that
// that piggy-back on marking can use the parity to ensure that they only
// perform an operation on an object once per marking phase: they record the
// MarkingParity when they visit an object, and only re-visit the object when it
// is marked again and the MarkingParity changes.
enum MarkingParity {
  NO_MARKING_PARITY,
  ODD_MARKING_PARITY,
  EVEN_MARKING_PARITY
};

// ICs store extra state in a Code object. The default extra state is
// kNoExtraICState.
typedef int ExtraICState;
static const ExtraICState kNoExtraICState = 0;

// Instance size sentinel for objects of variable size.
const int kVariableSizeSentinel = 0;

const int kStubMajorKeyBits = 7;
const int kStubMinorKeyBits = kBitsPerInt - kSmiTagSize - kStubMajorKeyBits;

// All Maps have a field instance_type containing a InstanceType.
// It describes the type of the instances.
//
// As an example, a JavaScript object is a heap object and its map
// instance_type is JS_OBJECT_TYPE.
//
// The names of the string instance types are intended to systematically
// mirror their encoding in the instance_type field of the map.  The default
// encoding is considered TWO_BYTE.  It is not mentioned in the name.  ASCII
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
  V(STRING_TYPE)                                                               \
  V(ASCII_STRING_TYPE)                                                         \
  V(CONS_STRING_TYPE)                                                          \
  V(CONS_ASCII_STRING_TYPE)                                                    \
  V(SLICED_STRING_TYPE)                                                        \
  V(SLICED_ASCII_STRING_TYPE)                                                  \
  V(EXTERNAL_STRING_TYPE)                                                      \
  V(EXTERNAL_ASCII_STRING_TYPE)                                                \
  V(EXTERNAL_STRING_WITH_ONE_BYTE_DATA_TYPE)                                   \
  V(SHORT_EXTERNAL_STRING_TYPE)                                                \
  V(SHORT_EXTERNAL_ASCII_STRING_TYPE)                                          \
  V(SHORT_EXTERNAL_STRING_WITH_ONE_BYTE_DATA_TYPE)                             \
                                                                               \
  V(INTERNALIZED_STRING_TYPE)                                                  \
  V(ASCII_INTERNALIZED_STRING_TYPE)                                            \
  V(CONS_INTERNALIZED_STRING_TYPE)                                             \
  V(CONS_ASCII_INTERNALIZED_STRING_TYPE)                                       \
  V(EXTERNAL_INTERNALIZED_STRING_TYPE)                                         \
  V(EXTERNAL_ASCII_INTERNALIZED_STRING_TYPE)                                   \
  V(EXTERNAL_INTERNALIZED_STRING_WITH_ONE_BYTE_DATA_TYPE)                      \
  V(SHORT_EXTERNAL_INTERNALIZED_STRING_TYPE)                                   \
  V(SHORT_EXTERNAL_ASCII_INTERNALIZED_STRING_TYPE)                             \
  V(SHORT_EXTERNAL_INTERNALIZED_STRING_WITH_ONE_BYTE_DATA_TYPE)                \
                                                                               \
  V(SYMBOL_TYPE)                                                               \
                                                                               \
  V(MAP_TYPE)                                                                  \
  V(CODE_TYPE)                                                                 \
  V(ODDBALL_TYPE)                                                              \
  V(CELL_TYPE)                                                                 \
  V(PROPERTY_CELL_TYPE)                                                        \
                                                                               \
  V(HEAP_NUMBER_TYPE)                                                          \
  V(FOREIGN_TYPE)                                                              \
  V(BYTE_ARRAY_TYPE)                                                           \
  V(FREE_SPACE_TYPE)                                                           \
  /* Note: the order of these external array */                                \
  /* types is relied upon in */                                                \
  /* Object::IsExternalArray(). */                                             \
  V(EXTERNAL_INT8_ARRAY_TYPE)                                                  \
  V(EXTERNAL_UINT8_ARRAY_TYPE)                                                 \
  V(EXTERNAL_INT16_ARRAY_TYPE)                                                 \
  V(EXTERNAL_UINT16_ARRAY_TYPE)                                                \
  V(EXTERNAL_INT32_ARRAY_TYPE)                                                 \
  V(EXTERNAL_UINT32_ARRAY_TYPE)                                                \
  V(EXTERNAL_FLOAT32_ARRAY_TYPE)                                               \
  V(EXTERNAL_FLOAT64_ARRAY_TYPE)                                               \
  V(EXTERNAL_UINT8_CLAMPED_ARRAY_TYPE)                                         \
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
  V(FILLER_TYPE)                                                               \
                                                                               \
  V(DECLARED_ACCESSOR_DESCRIPTOR_TYPE)                                         \
  V(DECLARED_ACCESSOR_INFO_TYPE)                                               \
  V(EXECUTABLE_ACCESSOR_INFO_TYPE)                                             \
  V(ACCESSOR_PAIR_TYPE)                                                        \
  V(ACCESS_CHECK_INFO_TYPE)                                                    \
  V(INTERCEPTOR_INFO_TYPE)                                                     \
  V(CALL_HANDLER_INFO_TYPE)                                                    \
  V(FUNCTION_TEMPLATE_INFO_TYPE)                                               \
  V(OBJECT_TEMPLATE_INFO_TYPE)                                                 \
  V(SIGNATURE_INFO_TYPE)                                                       \
  V(TYPE_SWITCH_INFO_TYPE)                                                     \
  V(ALLOCATION_MEMENTO_TYPE)                                                   \
  V(ALLOCATION_SITE_TYPE)                                                      \
  V(SCRIPT_TYPE)                                                               \
  V(CODE_CACHE_TYPE)                                                           \
  V(POLYMORPHIC_CODE_CACHE_TYPE)                                               \
  V(TYPE_FEEDBACK_INFO_TYPE)                                                   \
  V(ALIASED_ARGUMENTS_ENTRY_TYPE)                                              \
  V(BOX_TYPE)                                                                  \
                                                                               \
  V(FIXED_ARRAY_TYPE)                                                          \
  V(FIXED_DOUBLE_ARRAY_TYPE)                                                   \
  V(CONSTANT_POOL_ARRAY_TYPE)                                                  \
  V(SHARED_FUNCTION_INFO_TYPE)                                                 \
                                                                               \
  V(JS_MESSAGE_OBJECT_TYPE)                                                    \
                                                                               \
  V(JS_VALUE_TYPE)                                                             \
  V(JS_DATE_TYPE)                                                              \
  V(JS_OBJECT_TYPE)                                                            \
  V(JS_CONTEXT_EXTENSION_OBJECT_TYPE)                                          \
  V(JS_GENERATOR_OBJECT_TYPE)                                                  \
  V(JS_MODULE_TYPE)                                                            \
  V(JS_GLOBAL_OBJECT_TYPE)                                                     \
  V(JS_BUILTINS_OBJECT_TYPE)                                                   \
  V(JS_GLOBAL_PROXY_TYPE)                                                      \
  V(JS_ARRAY_TYPE)                                                             \
  V(JS_ARRAY_BUFFER_TYPE)                                                      \
  V(JS_TYPED_ARRAY_TYPE)                                                       \
  V(JS_DATA_VIEW_TYPE)                                                         \
  V(JS_PROXY_TYPE)                                                             \
  V(JS_SET_TYPE)                                                               \
  V(JS_MAP_TYPE)                                                               \
  V(JS_WEAK_MAP_TYPE)                                                          \
  V(JS_WEAK_SET_TYPE)                                                          \
  V(JS_REGEXP_TYPE)                                                            \
                                                                               \
  V(JS_FUNCTION_TYPE)                                                          \
  V(JS_FUNCTION_PROXY_TYPE)                                                    \
  V(DEBUG_INFO_TYPE)                                                           \
  V(BREAK_POINT_INFO_TYPE)


// Since string types are not consecutive, this macro is used to
// iterate over them.
#define STRING_TYPE_LIST(V)                                                    \
  V(STRING_TYPE,                                                               \
    kVariableSizeSentinel,                                                     \
    string,                                                                    \
    String)                                                                    \
  V(ASCII_STRING_TYPE,                                                         \
    kVariableSizeSentinel,                                                     \
    ascii_string,                                                              \
    AsciiString)                                                               \
  V(CONS_STRING_TYPE,                                                          \
    ConsString::kSize,                                                         \
    cons_string,                                                               \
    ConsString)                                                                \
  V(CONS_ASCII_STRING_TYPE,                                                    \
    ConsString::kSize,                                                         \
    cons_ascii_string,                                                         \
    ConsAsciiString)                                                           \
  V(SLICED_STRING_TYPE,                                                        \
    SlicedString::kSize,                                                       \
    sliced_string,                                                             \
    SlicedString)                                                              \
  V(SLICED_ASCII_STRING_TYPE,                                                  \
    SlicedString::kSize,                                                       \
    sliced_ascii_string,                                                       \
    SlicedAsciiString)                                                         \
  V(EXTERNAL_STRING_TYPE,                                                      \
    ExternalTwoByteString::kSize,                                              \
    external_string,                                                           \
    ExternalString)                                                            \
  V(EXTERNAL_ASCII_STRING_TYPE,                                                \
    ExternalAsciiString::kSize,                                                \
    external_ascii_string,                                                     \
    ExternalAsciiString)                                                       \
  V(EXTERNAL_STRING_WITH_ONE_BYTE_DATA_TYPE,                                   \
    ExternalTwoByteString::kSize,                                              \
    external_string_with_one_byte_data,                                        \
    ExternalStringWithOneByteData)                                             \
  V(SHORT_EXTERNAL_STRING_TYPE,                                                \
    ExternalTwoByteString::kShortSize,                                         \
    short_external_string,                                                     \
    ShortExternalString)                                                       \
  V(SHORT_EXTERNAL_ASCII_STRING_TYPE,                                          \
    ExternalAsciiString::kShortSize,                                           \
    short_external_ascii_string,                                               \
    ShortExternalAsciiString)                                                  \
  V(SHORT_EXTERNAL_STRING_WITH_ONE_BYTE_DATA_TYPE,                             \
    ExternalTwoByteString::kShortSize,                                         \
    short_external_string_with_one_byte_data,                                  \
    ShortExternalStringWithOneByteData)                                        \
                                                                               \
  V(INTERNALIZED_STRING_TYPE,                                                  \
    kVariableSizeSentinel,                                                     \
    internalized_string,                                                       \
    InternalizedString)                                                        \
  V(ASCII_INTERNALIZED_STRING_TYPE,                                            \
    kVariableSizeSentinel,                                                     \
    ascii_internalized_string,                                                 \
    AsciiInternalizedString)                                                   \
  V(CONS_INTERNALIZED_STRING_TYPE,                                             \
    ConsString::kSize,                                                         \
    cons_internalized_string,                                                  \
    ConsInternalizedString)                                                    \
  V(CONS_ASCII_INTERNALIZED_STRING_TYPE,                                       \
    ConsString::kSize,                                                         \
    cons_ascii_internalized_string,                                            \
    ConsAsciiInternalizedString)                                               \
  V(EXTERNAL_INTERNALIZED_STRING_TYPE,                                         \
    ExternalTwoByteString::kSize,                                              \
    external_internalized_string,                                              \
    ExternalInternalizedString)                                                \
  V(EXTERNAL_ASCII_INTERNALIZED_STRING_TYPE,                                   \
    ExternalAsciiString::kSize,                                                \
    external_ascii_internalized_string,                                        \
    ExternalAsciiInternalizedString)                                           \
  V(EXTERNAL_INTERNALIZED_STRING_WITH_ONE_BYTE_DATA_TYPE,                      \
    ExternalTwoByteString::kSize,                                              \
    external_internalized_string_with_one_byte_data,                           \
    ExternalInternalizedStringWithOneByteData)                                 \
  V(SHORT_EXTERNAL_INTERNALIZED_STRING_TYPE,                                   \
    ExternalTwoByteString::kShortSize,                                         \
    short_external_internalized_string,                                        \
    ShortExternalInternalizedString)                                           \
  V(SHORT_EXTERNAL_ASCII_INTERNALIZED_STRING_TYPE,                             \
    ExternalAsciiString::kShortSize,                                           \
    short_external_ascii_internalized_string,                                  \
    ShortExternalAsciiInternalizedString)                                      \
  V(SHORT_EXTERNAL_INTERNALIZED_STRING_WITH_ONE_BYTE_DATA_TYPE,                \
    ExternalTwoByteString::kShortSize,                                         \
    short_external_internalized_string_with_one_byte_data,                     \
    ShortExternalInternalizedStringWithOneByteData)                            \

// A struct is a simple object a set of object-valued fields.  Including an
// object type in this causes the compiler to generate most of the boilerplate
// code for the class including allocation and garbage collection routines,
// casts and predicates.  All you need to define is the class, methods and
// object verification routines.  Easy, no?
//
// Note that for subtle reasons related to the ordering or numerical values of
// type tags, elements in this list have to be added to the INSTANCE_TYPE_LIST
// manually.
#define STRUCT_LIST_ALL(V)                                                     \
  V(BOX, Box, box)                                                             \
  V(DECLARED_ACCESSOR_DESCRIPTOR,                                              \
    DeclaredAccessorDescriptor,                                                \
    declared_accessor_descriptor)                                              \
  V(DECLARED_ACCESSOR_INFO, DeclaredAccessorInfo, declared_accessor_info)      \
  V(EXECUTABLE_ACCESSOR_INFO, ExecutableAccessorInfo, executable_accessor_info)\
  V(ACCESSOR_PAIR, AccessorPair, accessor_pair)                                \
  V(ACCESS_CHECK_INFO, AccessCheckInfo, access_check_info)                     \
  V(INTERCEPTOR_INFO, InterceptorInfo, interceptor_info)                       \
  V(CALL_HANDLER_INFO, CallHandlerInfo, call_handler_info)                     \
  V(FUNCTION_TEMPLATE_INFO, FunctionTemplateInfo, function_template_info)      \
  V(OBJECT_TEMPLATE_INFO, ObjectTemplateInfo, object_template_info)            \
  V(SIGNATURE_INFO, SignatureInfo, signature_info)                             \
  V(TYPE_SWITCH_INFO, TypeSwitchInfo, type_switch_info)                        \
  V(SCRIPT, Script, script)                                                    \
  V(ALLOCATION_SITE, AllocationSite, allocation_site)                          \
  V(ALLOCATION_MEMENTO, AllocationMemento, allocation_memento)                 \
  V(CODE_CACHE, CodeCache, code_cache)                                         \
  V(POLYMORPHIC_CODE_CACHE, PolymorphicCodeCache, polymorphic_code_cache)      \
  V(TYPE_FEEDBACK_INFO, TypeFeedbackInfo, type_feedback_info)                  \
  V(ALIASED_ARGUMENTS_ENTRY, AliasedArgumentsEntry, aliased_arguments_entry)

#ifdef ENABLE_DEBUGGER_SUPPORT
#define STRUCT_LIST_DEBUGGER(V)                                                \
  V(DEBUG_INFO, DebugInfo, debug_info)                                         \
  V(BREAK_POINT_INFO, BreakPointInfo, break_point_info)
#else
#define STRUCT_LIST_DEBUGGER(V)
#endif

#define STRUCT_LIST(V)                                                         \
  STRUCT_LIST_ALL(V)                                                           \
  STRUCT_LIST_DEBUGGER(V)

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
STATIC_ASSERT((kSeqStringTag & kIsIndirectStringMask) == 0);
STATIC_ASSERT((kExternalStringTag & kIsIndirectStringMask) == 0);
STATIC_ASSERT(
    (kConsStringTag & kIsIndirectStringMask) == kIsIndirectStringTag);
STATIC_ASSERT(
    (kSlicedStringTag & kIsIndirectStringMask) == kIsIndirectStringTag);

// Use this mask to distinguish between cons and slice only after making
// sure that the string is one of the two (an indirect string).
const uint32_t kSlicedNotConsMask = kSlicedStringTag & ~kConsStringTag;
STATIC_ASSERT(IS_POWER_OF_TWO(kSlicedNotConsMask) && kSlicedNotConsMask != 0);

// If bit 7 is clear, then bit 3 indicates whether this two-byte
// string actually contains one byte data.
const uint32_t kOneByteDataHintMask = 0x08;
const uint32_t kOneByteDataHintTag = 0x08;

// If bit 7 is clear and string representation indicates an external string,
// then bit 4 indicates whether the data pointer is cached.
const uint32_t kShortExternalStringMask = 0x10;
const uint32_t kShortExternalStringTag = 0x10;


// A ConsString with an empty string as the right side is a candidate
// for being shortcut by the garbage collector unless it is internalized.
// It's not common to have non-flat internalized strings, so we do not
// shortcut them thereby avoiding turning internalized strings into strings.
// See heap.cc and mark-compact.cc.
const uint32_t kShortcutTypeMask =
    kIsNotStringMask |
    kIsNotInternalizedMask |
    kStringRepresentationMask;
const uint32_t kShortcutTypeTag = kConsStringTag | kNotInternalizedTag;


enum InstanceType {
  // String types.
  INTERNALIZED_STRING_TYPE = kTwoByteStringTag | kSeqStringTag
      | kInternalizedTag,
  ASCII_INTERNALIZED_STRING_TYPE = kOneByteStringTag | kSeqStringTag
      | kInternalizedTag,
  CONS_INTERNALIZED_STRING_TYPE = kTwoByteStringTag | kConsStringTag
      | kInternalizedTag,
  CONS_ASCII_INTERNALIZED_STRING_TYPE = kOneByteStringTag | kConsStringTag
      | kInternalizedTag,
  EXTERNAL_INTERNALIZED_STRING_TYPE = kTwoByteStringTag | kExternalStringTag
      | kInternalizedTag,
  EXTERNAL_ASCII_INTERNALIZED_STRING_TYPE = kOneByteStringTag
      | kExternalStringTag | kInternalizedTag,
  EXTERNAL_INTERNALIZED_STRING_WITH_ONE_BYTE_DATA_TYPE =
      EXTERNAL_INTERNALIZED_STRING_TYPE | kOneByteDataHintTag
      | kInternalizedTag,
  SHORT_EXTERNAL_INTERNALIZED_STRING_TYPE =
      EXTERNAL_INTERNALIZED_STRING_TYPE | kShortExternalStringTag
      | kInternalizedTag,
  SHORT_EXTERNAL_ASCII_INTERNALIZED_STRING_TYPE =
      EXTERNAL_ASCII_INTERNALIZED_STRING_TYPE | kShortExternalStringTag
      | kInternalizedTag,
  SHORT_EXTERNAL_INTERNALIZED_STRING_WITH_ONE_BYTE_DATA_TYPE =
      EXTERNAL_INTERNALIZED_STRING_WITH_ONE_BYTE_DATA_TYPE
      | kShortExternalStringTag | kInternalizedTag,

  STRING_TYPE = INTERNALIZED_STRING_TYPE | kNotInternalizedTag,
  ASCII_STRING_TYPE = ASCII_INTERNALIZED_STRING_TYPE | kNotInternalizedTag,
  CONS_STRING_TYPE = CONS_INTERNALIZED_STRING_TYPE | kNotInternalizedTag,
  CONS_ASCII_STRING_TYPE =
      CONS_ASCII_INTERNALIZED_STRING_TYPE | kNotInternalizedTag,

  SLICED_STRING_TYPE =
      kTwoByteStringTag | kSlicedStringTag | kNotInternalizedTag,
  SLICED_ASCII_STRING_TYPE =
      kOneByteStringTag | kSlicedStringTag | kNotInternalizedTag,
  EXTERNAL_STRING_TYPE =
  EXTERNAL_INTERNALIZED_STRING_TYPE | kNotInternalizedTag,
  EXTERNAL_ASCII_STRING_TYPE =
  EXTERNAL_ASCII_INTERNALIZED_STRING_TYPE | kNotInternalizedTag,
  EXTERNAL_STRING_WITH_ONE_BYTE_DATA_TYPE =
      EXTERNAL_INTERNALIZED_STRING_WITH_ONE_BYTE_DATA_TYPE
      | kNotInternalizedTag,
  SHORT_EXTERNAL_STRING_TYPE =
      SHORT_EXTERNAL_INTERNALIZED_STRING_TYPE | kNotInternalizedTag,
  SHORT_EXTERNAL_ASCII_STRING_TYPE =
      SHORT_EXTERNAL_ASCII_INTERNALIZED_STRING_TYPE | kNotInternalizedTag,
  SHORT_EXTERNAL_STRING_WITH_ONE_BYTE_DATA_TYPE =
      SHORT_EXTERNAL_INTERNALIZED_STRING_WITH_ONE_BYTE_DATA_TYPE
      | kNotInternalizedTag,

  // Non-string names
  SYMBOL_TYPE = kNotStringTag,  // FIRST_NONSTRING_TYPE, LAST_NAME_TYPE

  // Objects allocated in their own spaces (never in new space).
  MAP_TYPE,
  CODE_TYPE,
  ODDBALL_TYPE,
  CELL_TYPE,
  PROPERTY_CELL_TYPE,

  // "Data", objects that cannot contain non-map-word pointers to heap
  // objects.
  HEAP_NUMBER_TYPE,
  FOREIGN_TYPE,
  BYTE_ARRAY_TYPE,
  FREE_SPACE_TYPE,

  EXTERNAL_INT8_ARRAY_TYPE,  // FIRST_EXTERNAL_ARRAY_TYPE
  EXTERNAL_UINT8_ARRAY_TYPE,
  EXTERNAL_INT16_ARRAY_TYPE,
  EXTERNAL_UINT16_ARRAY_TYPE,
  EXTERNAL_INT32_ARRAY_TYPE,
  EXTERNAL_UINT32_ARRAY_TYPE,
  EXTERNAL_FLOAT32_ARRAY_TYPE,
  EXTERNAL_FLOAT64_ARRAY_TYPE,
  EXTERNAL_UINT8_CLAMPED_ARRAY_TYPE,  // LAST_EXTERNAL_ARRAY_TYPE

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
  DECLARED_ACCESSOR_DESCRIPTOR_TYPE,
  DECLARED_ACCESSOR_INFO_TYPE,
  EXECUTABLE_ACCESSOR_INFO_TYPE,
  ACCESSOR_PAIR_TYPE,
  ACCESS_CHECK_INFO_TYPE,
  INTERCEPTOR_INFO_TYPE,
  CALL_HANDLER_INFO_TYPE,
  FUNCTION_TEMPLATE_INFO_TYPE,
  OBJECT_TEMPLATE_INFO_TYPE,
  SIGNATURE_INFO_TYPE,
  TYPE_SWITCH_INFO_TYPE,
  ALLOCATION_SITE_TYPE,
  ALLOCATION_MEMENTO_TYPE,
  SCRIPT_TYPE,
  CODE_CACHE_TYPE,
  POLYMORPHIC_CODE_CACHE_TYPE,
  TYPE_FEEDBACK_INFO_TYPE,
  ALIASED_ARGUMENTS_ENTRY_TYPE,
  BOX_TYPE,
  // The following two instance types are only used when ENABLE_DEBUGGER_SUPPORT
  // is defined. However as include/v8.h contain some of the instance type
  // constants always having them avoids them getting different numbers
  // depending on whether ENABLE_DEBUGGER_SUPPORT is defined or not.
  DEBUG_INFO_TYPE,
  BREAK_POINT_INFO_TYPE,

  FIXED_ARRAY_TYPE,
  CONSTANT_POOL_ARRAY_TYPE,
  SHARED_FUNCTION_INFO_TYPE,

  JS_MESSAGE_OBJECT_TYPE,

  // All the following types are subtypes of JSReceiver, which corresponds to
  // objects in the JS sense. The first and the last type in this range are
  // the two forms of function. This organization enables using the same
  // compares for checking the JS_RECEIVER/SPEC_OBJECT range and the
  // NONCALLABLE_JS_OBJECT range.
  JS_FUNCTION_PROXY_TYPE,  // FIRST_JS_RECEIVER_TYPE, FIRST_JS_PROXY_TYPE
  JS_PROXY_TYPE,  // LAST_JS_PROXY_TYPE

  JS_VALUE_TYPE,  // FIRST_JS_OBJECT_TYPE
  JS_DATE_TYPE,
  JS_OBJECT_TYPE,
  JS_CONTEXT_EXTENSION_OBJECT_TYPE,
  JS_GENERATOR_OBJECT_TYPE,
  JS_MODULE_TYPE,
  JS_GLOBAL_OBJECT_TYPE,
  JS_BUILTINS_OBJECT_TYPE,
  JS_GLOBAL_PROXY_TYPE,
  JS_ARRAY_TYPE,
  JS_ARRAY_BUFFER_TYPE,
  JS_TYPED_ARRAY_TYPE,
  JS_DATA_VIEW_TYPE,
  JS_SET_TYPE,
  JS_MAP_TYPE,
  JS_WEAK_MAP_TYPE,
  JS_WEAK_SET_TYPE,

  JS_REGEXP_TYPE,

  JS_FUNCTION_TYPE,  // LAST_JS_OBJECT_TYPE, LAST_JS_RECEIVER_TYPE

  // Pseudo-types
  FIRST_TYPE = 0x0,
  LAST_TYPE = JS_FUNCTION_TYPE,
  FIRST_NAME_TYPE = FIRST_TYPE,
  LAST_NAME_TYPE = SYMBOL_TYPE,
  FIRST_UNIQUE_NAME_TYPE = INTERNALIZED_STRING_TYPE,
  LAST_UNIQUE_NAME_TYPE = SYMBOL_TYPE,
  FIRST_NONSTRING_TYPE = SYMBOL_TYPE,
  // Boundaries for testing for an external array.
  FIRST_EXTERNAL_ARRAY_TYPE = EXTERNAL_INT8_ARRAY_TYPE,
  LAST_EXTERNAL_ARRAY_TYPE = EXTERNAL_UINT8_CLAMPED_ARRAY_TYPE,
  // Boundaries for testing for a fixed typed array.
  FIRST_FIXED_TYPED_ARRAY_TYPE = FIXED_INT8_ARRAY_TYPE,
  LAST_FIXED_TYPED_ARRAY_TYPE = FIXED_UINT8_CLAMPED_ARRAY_TYPE,
  // Boundary for promotion to old data space/old pointer space.
  LAST_DATA_TYPE = FILLER_TYPE,
  // Boundary for objects represented as JSReceiver (i.e. JSObject or JSProxy).
  // Note that there is no range for JSObject or JSProxy, since their subtypes
  // are not continuous in this enum! The enum ranges instead reflect the
  // external class names, where proxies are treated as either ordinary objects,
  // or functions.
  FIRST_JS_RECEIVER_TYPE = JS_FUNCTION_PROXY_TYPE,
  LAST_JS_RECEIVER_TYPE = LAST_TYPE,
  // Boundaries for testing the types represented as JSObject
  FIRST_JS_OBJECT_TYPE = JS_VALUE_TYPE,
  LAST_JS_OBJECT_TYPE = LAST_TYPE,
  // Boundaries for testing the types represented as JSProxy
  FIRST_JS_PROXY_TYPE = JS_FUNCTION_PROXY_TYPE,
  LAST_JS_PROXY_TYPE = JS_PROXY_TYPE,
  // Boundaries for testing whether the type is a JavaScript object.
  FIRST_SPEC_OBJECT_TYPE = FIRST_JS_RECEIVER_TYPE,
  LAST_SPEC_OBJECT_TYPE = LAST_JS_RECEIVER_TYPE,
  // Boundaries for testing the types for which typeof is "object".
  FIRST_NONCALLABLE_SPEC_OBJECT_TYPE = JS_PROXY_TYPE,
  LAST_NONCALLABLE_SPEC_OBJECT_TYPE = JS_REGEXP_TYPE,
  // Note that the types for which typeof is "function" are not continuous.
  // Define this so that we can put assertions on discrete checks.
  NUM_OF_CALLABLE_SPEC_OBJECT_TYPES = 2
};

const int kExternalArrayTypeCount =
    LAST_EXTERNAL_ARRAY_TYPE - FIRST_EXTERNAL_ARRAY_TYPE + 1;

STATIC_CHECK(JS_OBJECT_TYPE == Internals::kJSObjectType);
STATIC_CHECK(FIRST_NONSTRING_TYPE == Internals::kFirstNonstringType);
STATIC_CHECK(ODDBALL_TYPE == Internals::kOddballType);
STATIC_CHECK(FOREIGN_TYPE == Internals::kForeignType);


#define FIXED_ARRAY_SUB_INSTANCE_TYPE_LIST(V) \
  V(FAST_ELEMENTS_SUB_TYPE)                   \
  V(DICTIONARY_ELEMENTS_SUB_TYPE)             \
  V(FAST_PROPERTIES_SUB_TYPE)                 \
  V(DICTIONARY_PROPERTIES_SUB_TYPE)           \
  V(MAP_CODE_CACHE_SUB_TYPE)                  \
  V(SCOPE_INFO_SUB_TYPE)                      \
  V(STRING_TABLE_SUB_TYPE)                    \
  V(DESCRIPTOR_ARRAY_SUB_TYPE)                \
  V(TRANSITION_ARRAY_SUB_TYPE)

enum FixedArraySubInstanceType {
#define DEFINE_FIXED_ARRAY_SUB_INSTANCE_TYPE(name) name,
  FIXED_ARRAY_SUB_INSTANCE_TYPE_LIST(DEFINE_FIXED_ARRAY_SUB_INSTANCE_TYPE)
#undef DEFINE_FIXED_ARRAY_SUB_INSTANCE_TYPE
  LAST_FIXED_ARRAY_SUB_TYPE = TRANSITION_ARRAY_SUB_TYPE
};


enum CompareResult {
  LESS      = -1,
  EQUAL     =  0,
  GREATER   =  1,

  NOT_EQUAL = GREATER
};


#define DECL_BOOLEAN_ACCESSORS(name)   \
  inline bool name();                  \
  inline void set_##name(bool value);  \


#define DECL_ACCESSORS(name, type)                                      \
  inline type* name();                                                  \
  inline void set_##name(type* value,                                   \
                         WriteBarrierMode mode = UPDATE_WRITE_BARRIER); \

class AccessorPair;
class AllocationSite;
class AllocationSiteCreationContext;
class AllocationSiteUsageContext;
class DictionaryElementsAccessor;
class ElementsAccessor;
class Failure;
class FixedArrayBase;
class GlobalObject;
class ObjectVisitor;
class StringStream;
// We cannot just say "class HeapType;" if it is created from a template... =8-?
template<class> class TypeImpl;
struct HeapTypeConfig;
typedef TypeImpl<HeapTypeConfig> HeapType;


// A template-ized version of the IsXXX functions.
template <class C> inline bool Is(Object* obj);

#ifdef VERIFY_HEAP
#define DECLARE_VERIFIER(Name) void Name##Verify();
#else
#define DECLARE_VERIFIER(Name)
#endif

#ifdef OBJECT_PRINT
#define DECLARE_PRINTER(Name) void Name##Print(FILE* out = stdout);
#else
#define DECLARE_PRINTER(Name)
#endif

class MaybeObject BASE_EMBEDDED {
 public:
  inline bool IsFailure();
  inline bool IsRetryAfterGC();
  inline bool IsOutOfMemory();
  inline bool IsException();
  INLINE(bool IsTheHole());
  INLINE(bool IsUninitialized());
  inline bool ToObject(Object** obj) {
    if (IsFailure()) return false;
    *obj = reinterpret_cast<Object*>(this);
    return true;
  }
  inline Failure* ToFailureUnchecked() {
    ASSERT(IsFailure());
    return reinterpret_cast<Failure*>(this);
  }
  inline Object* ToObjectUnchecked() {
    // TODO(jkummerow): Turn this back into an ASSERT when we can be certain
    // that it never fires in Release mode in the wild.
    CHECK(!IsFailure());
    return reinterpret_cast<Object*>(this);
  }
  inline Object* ToObjectChecked() {
    CHECK(!IsFailure());
    return reinterpret_cast<Object*>(this);
  }

  template<typename T>
  inline bool To(T** obj) {
    if (IsFailure()) return false;
    *obj = T::cast(reinterpret_cast<Object*>(this));
    return true;
  }

  template<typename T>
    inline bool ToHandle(Handle<T>* obj, Isolate* isolate) {
    if (IsFailure()) return false;
    *obj = handle(T::cast(reinterpret_cast<Object*>(this)), isolate);
    return true;
  }

#ifdef OBJECT_PRINT
  // Prints this object with details.
  void Print();
  void Print(FILE* out);
  void PrintLn();
  void PrintLn(FILE* out);
#endif
#ifdef VERIFY_HEAP
  // Verifies the object.
  void Verify();
#endif
};


#define OBJECT_TYPE_LIST(V)                    \
  V(Smi)                                       \
  V(HeapObject)                                \
  V(Number)                                    \

#define HEAP_OBJECT_TYPE_LIST(V)               \
  V(HeapNumber)                                \
  V(Name)                                      \
  V(UniqueName)                                \
  V(String)                                    \
  V(SeqString)                                 \
  V(ExternalString)                            \
  V(ConsString)                                \
  V(SlicedString)                              \
  V(ExternalTwoByteString)                     \
  V(ExternalAsciiString)                       \
  V(SeqTwoByteString)                          \
  V(SeqOneByteString)                          \
  V(InternalizedString)                        \
  V(Symbol)                                    \
                                               \
  V(ExternalArray)                             \
  V(ExternalInt8Array)                         \
  V(ExternalUint8Array)                        \
  V(ExternalInt16Array)                        \
  V(ExternalUint16Array)                       \
  V(ExternalInt32Array)                        \
  V(ExternalUint32Array)                       \
  V(ExternalFloat32Array)                      \
  V(ExternalFloat64Array)                      \
  V(ExternalUint8ClampedArray)                 \
  V(FixedTypedArrayBase)                       \
  V(FixedUint8Array)                           \
  V(FixedInt8Array)                            \
  V(FixedUint16Array)                          \
  V(FixedInt16Array)                           \
  V(FixedUint32Array)                          \
  V(FixedInt32Array)                           \
  V(FixedFloat32Array)                         \
  V(FixedFloat64Array)                         \
  V(FixedUint8ClampedArray)                    \
  V(ByteArray)                                 \
  V(FreeSpace)                                 \
  V(JSReceiver)                                \
  V(JSObject)                                  \
  V(JSContextExtensionObject)                  \
  V(JSGeneratorObject)                         \
  V(JSModule)                                  \
  V(Map)                                       \
  V(DescriptorArray)                           \
  V(TransitionArray)                           \
  V(DeoptimizationInputData)                   \
  V(DeoptimizationOutputData)                  \
  V(DependentCode)                             \
  V(FixedArray)                                \
  V(FixedDoubleArray)                          \
  V(ConstantPoolArray)                         \
  V(Context)                                   \
  V(NativeContext)                             \
  V(ScopeInfo)                                 \
  V(JSFunction)                                \
  V(Code)                                      \
  V(Oddball)                                   \
  V(SharedFunctionInfo)                        \
  V(JSValue)                                   \
  V(JSDate)                                    \
  V(JSMessageObject)                           \
  V(StringWrapper)                             \
  V(Foreign)                                   \
  V(Boolean)                                   \
  V(JSArray)                                   \
  V(JSArrayBuffer)                             \
  V(JSArrayBufferView)                         \
  V(JSTypedArray)                              \
  V(JSDataView)                                \
  V(JSProxy)                                   \
  V(JSFunctionProxy)                           \
  V(JSSet)                                     \
  V(JSMap)                                     \
  V(JSWeakCollection)                          \
  V(JSWeakMap)                                 \
  V(JSWeakSet)                                 \
  V(JSRegExp)                                  \
  V(HashTable)                                 \
  V(Dictionary)                                \
  V(StringTable)                               \
  V(JSFunctionResultCache)                     \
  V(NormalizedMapCache)                        \
  V(CompilationCacheTable)                     \
  V(CodeCacheHashTable)                        \
  V(PolymorphicCodeCacheHashTable)             \
  V(MapCache)                                  \
  V(Primitive)                                 \
  V(GlobalObject)                              \
  V(JSGlobalObject)                            \
  V(JSBuiltinsObject)                          \
  V(JSGlobalProxy)                             \
  V(UndetectableObject)                        \
  V(AccessCheckNeeded)                         \
  V(Cell)                                      \
  V(PropertyCell)                              \
  V(ObjectHashTable)                           \
  V(WeakHashTable)


#define ERROR_MESSAGES_LIST(V) \
  V(kNoReason, "no reason")                                                   \
                                                                              \
  V(k32BitValueInRegisterIsNotZeroExtended,                                   \
    "32 bit value in register is not zero-extended")                          \
  V(kAlignmentMarkerExpected, "Alignment marker expected")                    \
  V(kAllocationIsNotDoubleAligned, "Allocation is not double aligned")        \
  V(kAPICallReturnedInvalidObject, "API call returned invalid object")        \
  V(kArgumentsObjectValueInATestContext,                                      \
    "Arguments object value in a test context")                               \
  V(kArrayBoilerplateCreationFailed, "Array boilerplate creation failed")     \
  V(kArrayIndexConstantValueTooBig, "Array index constant value too big")     \
  V(kAssignmentToArguments, "Assignment to arguments")                        \
  V(kAssignmentToLetVariableBeforeInitialization,                             \
    "Assignment to let variable before initialization")                       \
  V(kAssignmentToLOOKUPVariable, "Assignment to LOOKUP variable")             \
  V(kAssignmentToParameterFunctionUsesArgumentsObject,                        \
    "Assignment to parameter, function uses arguments object")                \
  V(kAssignmentToParameterInArgumentsObject,                                  \
    "Assignment to parameter in arguments object")                            \
  V(kAttemptToUseUndefinedCache, "Attempt to use undefined cache")            \
  V(kBadValueContextForArgumentsObjectValue,                                  \
    "Bad value context for arguments object value")                           \
  V(kBadValueContextForArgumentsValue,                                        \
    "Bad value context for arguments value")                                  \
  V(kBailedOutDueToDependencyChange, "Bailed out due to dependency change")   \
  V(kBailoutWasNotPrepared, "Bailout was not prepared")                       \
  V(kBinaryStubGenerateFloatingPointCode,                                     \
    "BinaryStub_GenerateFloatingPointCode")                                   \
  V(kBothRegistersWereSmisInSelectNonSmi,                                     \
    "Both registers were smis in SelectNonSmi")                               \
  V(kCallToAJavaScriptRuntimeFunction,                                        \
    "Call to a JavaScript runtime function")                                  \
  V(kCannotTranslatePositionInChangedArea,                                    \
    "Cannot translate position in changed area")                              \
  V(kCodeGenerationFailed, "Code generation failed")                          \
  V(kCodeObjectNotProperlyPatched, "Code object not properly patched")        \
  V(kCompoundAssignmentToLookupSlot, "Compound assignment to lookup slot")    \
  V(kContextAllocatedArguments, "Context-allocated arguments")                \
  V(kCopyBuffersOverlap, "Copy buffers overlap")                              \
  V(kCouldNotGenerateZero, "Could not generate +0.0")                         \
  V(kCouldNotGenerateNegativeZero, "Could not generate -0.0")                 \
  V(kDebuggerIsActive, "Debugger is active")                                  \
  V(kDebuggerStatement, "DebuggerStatement")                                  \
  V(kDeclarationInCatchContext, "Declaration in catch context")               \
  V(kDeclarationInWithContext, "Declaration in with context")                 \
  V(kDefaultNaNModeNotSet, "Default NaN mode not set")                        \
  V(kDeleteWithGlobalVariable, "Delete with global variable")                 \
  V(kDeleteWithNonGlobalVariable, "Delete with non-global variable")          \
  V(kDestinationOfCopyNotAligned, "Destination of copy not aligned")          \
  V(kDontDeleteCellsCannotContainTheHole,                                     \
    "DontDelete cells can't contain the hole")                                \
  V(kDoPushArgumentNotImplementedForDoubleType,                               \
    "DoPushArgument not implemented for double type")                         \
  V(kEliminatedBoundsCheckFailed, "Eliminated bounds check failed")           \
  V(kEmitLoadRegisterUnsupportedDoubleImmediate,                              \
    "EmitLoadRegister: Unsupported double immediate")                         \
  V(kEval, "eval")                                                            \
  V(kExpected0AsASmiSentinel, "Expected 0 as a Smi sentinel")                 \
  V(kExpectedAlignmentMarker, "Expected alignment marker")                    \
  V(kExpectedAllocationSite, "Expected allocation site")                      \
  V(kExpectedFunctionObject, "Expected function object in register")          \
  V(kExpectedHeapNumber, "Expected HeapNumber")                               \
  V(kExpectedNativeContext, "Expected native context")                        \
  V(kExpectedNonIdenticalObjects, "Expected non-identical objects")           \
  V(kExpectedNonNullContext, "Expected non-null context")                     \
  V(kExpectedPositiveZero, "Expected +0.0")                                   \
  V(kExpectedAllocationSiteInCell,                                            \
    "Expected AllocationSite in property cell")                               \
  V(kExpectedFixedArrayInFeedbackVector,                                      \
    "Expected fixed array in feedback vector")                                \
  V(kExpectedFixedArrayInRegisterA2,                                          \
    "Expected fixed array in register a2")                                    \
  V(kExpectedFixedArrayInRegisterEbx,                                         \
    "Expected fixed array in register ebx")                                   \
  V(kExpectedFixedArrayInRegisterR2,                                          \
    "Expected fixed array in register r2")                                    \
  V(kExpectedFixedArrayInRegisterRbx,                                         \
    "Expected fixed array in register rbx")                                   \
  V(kExpectedSmiOrHeapNumber, "Expected smi or HeapNumber")                   \
  V(kExpectingAlignmentForCopyBytes,                                          \
    "Expecting alignment for CopyBytes")                                      \
  V(kExportDeclaration, "Export declaration")                                 \
  V(kExternalStringExpectedButNotFound,                                       \
    "External string expected, but not found")                                \
  V(kFailedBailedOutLastTime, "Failed/bailed out last time")                  \
  V(kForInStatementIsNotFastCase, "ForInStatement is not fast case")          \
  V(kForInStatementOptimizationIsDisabled,                                    \
    "ForInStatement optimization is disabled")                                \
  V(kForInStatementWithNonLocalEachVariable,                                  \
    "ForInStatement with non-local each variable")                            \
  V(kForOfStatement, "ForOfStatement")                                        \
  V(kFrameIsExpectedToBeAligned, "Frame is expected to be aligned")           \
  V(kFunctionCallsEval, "Function calls eval")                                \
  V(kFunctionIsAGenerator, "Function is a generator")                         \
  V(kFunctionWithIllegalRedeclaration, "Function with illegal redeclaration") \
  V(kGeneratedCodeIsTooLarge, "Generated code is too large")                  \
  V(kGeneratorFailedToResume, "Generator failed to resume")                   \
  V(kGenerator, "Generator")                                                  \
  V(kGlobalFunctionsMustHaveInitialMap,                                       \
    "Global functions must have initial map")                                 \
  V(kHeapNumberMapRegisterClobbered, "HeapNumberMap register clobbered")      \
  V(kHydrogenFilter, "Optimization disabled by filter")                       \
  V(kImportDeclaration, "Import declaration")                                 \
  V(kImproperObjectOnPrototypeChainForStore,                                  \
    "Improper object on prototype chain for store")                           \
  V(kIndexIsNegative, "Index is negative")                                    \
  V(kIndexIsTooLarge, "Index is too large")                                   \
  V(kInlinedRuntimeFunctionClassOf, "Inlined runtime function: ClassOf")      \
  V(kInlinedRuntimeFunctionFastAsciiArrayJoin,                                \
    "Inlined runtime function: FastAsciiArrayJoin")                           \
  V(kInlinedRuntimeFunctionGeneratorNext,                                     \
    "Inlined runtime function: GeneratorNext")                                \
  V(kInlinedRuntimeFunctionGeneratorThrow,                                    \
    "Inlined runtime function: GeneratorThrow")                               \
  V(kInlinedRuntimeFunctionGetFromCache,                                      \
    "Inlined runtime function: GetFromCache")                                 \
  V(kInlinedRuntimeFunctionIsNonNegativeSmi,                                  \
    "Inlined runtime function: IsNonNegativeSmi")                             \
  V(kInlinedRuntimeFunctionIsStringWrapperSafeForDefaultValueOf,              \
    "Inlined runtime function: IsStringWrapperSafeForDefaultValueOf")         \
  V(kInliningBailedOut, "Inlining bailed out")                                \
  V(kInputGPRIsExpectedToHaveUpper32Cleared,                                  \
    "Input GPR is expected to have upper32 cleared")                          \
  V(kInputStringTooLong, "Input string too long")                             \
  V(kInstanceofStubUnexpectedCallSiteCacheCheck,                              \
    "InstanceofStub unexpected call site cache (check)")                      \
  V(kInstanceofStubUnexpectedCallSiteCacheCmp1,                               \
    "InstanceofStub unexpected call site cache (cmp 1)")                      \
  V(kInstanceofStubUnexpectedCallSiteCacheCmp2,                               \
    "InstanceofStub unexpected call site cache (cmp 2)")                      \
  V(kInstanceofStubUnexpectedCallSiteCacheMov,                                \
    "InstanceofStub unexpected call site cache (mov)")                        \
  V(kInteger32ToSmiFieldWritingToNonSmiLocation,                              \
    "Integer32ToSmiField writing to non-smi location")                        \
  V(kInvalidCaptureReferenced, "Invalid capture referenced")                  \
  V(kInvalidElementsKindForInternalArrayOrInternalPackedArray,                \
    "Invalid ElementsKind for InternalArray or InternalPackedArray")          \
  V(kInvalidFullCodegenState, "invalid full-codegen state")                   \
  V(kInvalidHandleScopeLevel, "Invalid HandleScope level")                    \
  V(kInvalidLeftHandSideInAssignment, "Invalid left-hand side in assignment") \
  V(kInvalidLhsInCompoundAssignment, "Invalid lhs in compound assignment")    \
  V(kInvalidLhsInCountOperation, "Invalid lhs in count operation")            \
  V(kInvalidMinLength, "Invalid min_length")                                  \
  V(kJSGlobalObjectNativeContextShouldBeANativeContext,                       \
    "JSGlobalObject::native_context should be a native context")              \
  V(kJSGlobalProxyContextShouldNotBeNull,                                     \
    "JSGlobalProxy::context() should not be null")                            \
  V(kJSObjectWithFastElementsMapHasSlowElements,                              \
    "JSObject with fast elements map has slow elements")                      \
  V(kLetBindingReInitialization, "Let binding re-initialization")             \
  V(kLhsHasBeenClobbered, "lhs has been clobbered")                           \
  V(kLiveBytesCountOverflowChunkSize, "Live Bytes Count overflow chunk size") \
  V(kLiveEditFrameDroppingIsNotSupportedOnA64,                                \
    "LiveEdit frame dropping is not supported on a64")                        \
  V(kLiveEditFrameDroppingIsNotSupportedOnArm,                                \
    "LiveEdit frame dropping is not supported on arm")                        \
  V(kLiveEditFrameDroppingIsNotSupportedOnMips,                               \
    "LiveEdit frame dropping is not supported on mips")                       \
  V(kLiveEdit, "LiveEdit")                                                    \
  V(kLookupVariableInCountOperation,                                          \
    "Lookup variable in count operation")                                     \
  V(kMapIsNoLongerInEax, "Map is no longer in eax")                           \
  V(kModuleDeclaration, "Module declaration")                                 \
  V(kModuleLiteral, "Module literal")                                         \
  V(kModulePath, "Module path")                                               \
  V(kModuleStatement, "Module statement")                                     \
  V(kModuleVariable, "Module variable")                                       \
  V(kModuleUrl, "Module url")                                                 \
  V(kNativeFunctionLiteral, "Native function literal")                        \
  V(kNoCasesLeft, "No cases left")                                            \
  V(kNoEmptyArraysHereInEmitFastAsciiArrayJoin,                               \
    "No empty arrays here in EmitFastAsciiArrayJoin")                         \
  V(kNonInitializerAssignmentToConst,                                         \
    "Non-initializer assignment to const")                                    \
  V(kNonSmiIndex, "Non-smi index")                                            \
  V(kNonSmiKeyInArrayLiteral, "Non-smi key in array literal")                 \
  V(kNonSmiValue, "Non-smi value")                                            \
  V(kNonObject, "Non-object value")                                           \
  V(kNotEnoughVirtualRegistersForValues,                                      \
    "Not enough virtual registers for values")                                \
  V(kNotEnoughSpillSlotsForOsr,                                               \
    "Not enough spill slots for OSR")                                         \
  V(kNotEnoughVirtualRegistersRegalloc,                                       \
    "Not enough virtual registers (regalloc)")                                \
  V(kObjectFoundInSmiOnlyArray, "Object found in smi-only array")             \
  V(kObjectLiteralWithComplexProperty,                                        \
    "Object literal with complex property")                                   \
  V(kOddballInStringTableIsNotUndefinedOrTheHole,                             \
    "Oddball in string table is not undefined or the hole")                   \
  V(kOffsetOutOfRange, "Offset out of range")                                 \
  V(kOperandIsASmiAndNotAName, "Operand is a smi and not a name")             \
  V(kOperandIsASmiAndNotAString, "Operand is a smi and not a string")         \
  V(kOperandIsASmi, "Operand is a smi")                                       \
  V(kOperandIsNotAName, "Operand is not a name")                              \
  V(kOperandIsNotANumber, "Operand is not a number")                          \
  V(kOperandIsNotASmi, "Operand is not a smi")                                \
  V(kOperandIsNotAString, "Operand is not a string")                          \
  V(kOperandIsNotSmi, "Operand is not smi")                                   \
  V(kOperandNotANumber, "Operand not a number")                               \
  V(kOptimizationDisabled, "Optimization is disabled")                        \
  V(kOptimizedTooManyTimes, "Optimized too many times")                       \
  V(kOutOfVirtualRegistersWhileTryingToAllocateTempRegister,                  \
    "Out of virtual registers while trying to allocate temp register")        \
  V(kParseScopeError, "Parse/scope error")                                    \
  V(kPossibleDirectCallToEval, "Possible direct call to eval")                \
  V(kPreconditionsWereNotMet, "Preconditions were not met")                   \
  V(kPropertyAllocationCountFailed, "Property allocation count failed")       \
  V(kReceivedInvalidReturnAddress, "Received invalid return address")         \
  V(kReferenceToAVariableWhichRequiresDynamicLookup,                          \
    "Reference to a variable which requires dynamic lookup")                  \
  V(kReferenceToGlobalLexicalVariable,                                        \
    "Reference to global lexical variable")                                   \
  V(kReferenceToUninitializedVariable, "Reference to uninitialized variable") \
  V(kRegisterDidNotMatchExpectedRoot, "Register did not match expected root") \
  V(kRegisterWasClobbered, "Register was clobbered")                          \
  V(kRememberedSetPointerInNewSpace, "Remembered set pointer is in new space") \
  V(kReturnAddressNotFoundInFrame, "Return address not found in frame")       \
  V(kRhsHasBeenClobbered, "Rhs has been clobbered")                           \
  V(kScopedBlock, "ScopedBlock")                                              \
  V(kSmiAdditionOverflow, "Smi addition overflow")                            \
  V(kSmiSubtractionOverflow, "Smi subtraction overflow")                      \
  V(kStackAccessBelowStackPointer, "Stack access below stack pointer")        \
  V(kStackFrameTypesMustMatch, "Stack frame types must match")                \
  V(kSwitchStatementMixedOrNonLiteralSwitchLabels,                            \
    "SwitchStatement: mixed or non-literal switch labels")                    \
  V(kSwitchStatementTooManyClauses, "SwitchStatement: too many clauses")      \
  V(kTheCurrentStackPointerIsBelowCsp,                                        \
    "The current stack pointer is below csp")                                 \
  V(kTheInstructionShouldBeALui, "The instruction should be a lui")           \
  V(kTheInstructionShouldBeAnOri, "The instruction should be an ori")         \
  V(kTheInstructionToPatchShouldBeALoadFromPc,                                \
    "The instruction to patch should be a load from pc")                      \
  V(kTheInstructionToPatchShouldBeAnLdrLiteral,                               \
    "The instruction to patch should be a ldr literal")                       \
  V(kTheInstructionToPatchShouldBeALui,                                       \
    "The instruction to patch should be a lui")                               \
  V(kTheInstructionToPatchShouldBeAnOri,                                      \
    "The instruction to patch should be an ori")                              \
  V(kTheSourceAndDestinationAreTheSame,                                       \
    "The source and destination are the same")                                \
  V(kTheStackWasCorruptedByMacroAssemblerCall,                                \
    "The stack was corrupted by MacroAssembler::Call()")                      \
  V(kTooManyParametersLocals, "Too many parameters/locals")                   \
  V(kTooManyParameters, "Too many parameters")                                \
  V(kTooManySpillSlotsNeededForOSR, "Too many spill slots needed for OSR")    \
  V(kToOperand32UnsupportedImmediate, "ToOperand32 unsupported immediate.")   \
  V(kToOperandIsDoubleRegisterUnimplemented,                                  \
    "ToOperand IsDoubleRegister unimplemented")                               \
  V(kToOperandUnsupportedDoubleImmediate,                                     \
    "ToOperand Unsupported double immediate")                                 \
  V(kTryCatchStatement, "TryCatchStatement")                                  \
  V(kTryFinallyStatement, "TryFinallyStatement")                              \
  V(kUnableToEncodeValueAsSmi, "Unable to encode value as smi")               \
  V(kUnalignedAllocationInNewSpace, "Unaligned allocation in new space")      \
  V(kUnalignedCellInWriteBarrier, "Unaligned cell in write barrier")          \
  V(kUndefinedValueNotLoaded, "Undefined value not loaded")                   \
  V(kUndoAllocationOfNonAllocatedMemory,                                      \
    "Undo allocation of non allocated memory")                                \
  V(kUnexpectedAllocationTop, "Unexpected allocation top")                    \
  V(kUnexpectedColorFound, "Unexpected color bit pattern found")              \
  V(kUnexpectedElementsKindInArrayConstructor,                                \
    "Unexpected ElementsKind in array constructor")                           \
  V(kUnexpectedFallthroughFromCharCodeAtSlowCase,                             \
    "Unexpected fallthrough from CharCodeAt slow case")                       \
  V(kUnexpectedFallthroughFromCharFromCodeSlowCase,                           \
    "Unexpected fallthrough from CharFromCode slow case")                     \
  V(kUnexpectedFallThroughFromStringComparison,                               \
    "Unexpected fall-through from string comparison")                         \
  V(kUnexpectedFallThroughInBinaryStubGenerateFloatingPointCode,              \
    "Unexpected fall-through in BinaryStub_GenerateFloatingPointCode")        \
  V(kUnexpectedFallthroughToCharCodeAtSlowCase,                               \
    "Unexpected fallthrough to CharCodeAt slow case")                         \
  V(kUnexpectedFallthroughToCharFromCodeSlowCase,                             \
    "Unexpected fallthrough to CharFromCode slow case")                       \
  V(kUnexpectedFPUStackDepthAfterInstruction,                                 \
    "Unexpected FPU stack depth after instruction")                           \
  V(kUnexpectedInitialMapForArrayFunction1,                                   \
    "Unexpected initial map for Array function (1)")                          \
  V(kUnexpectedInitialMapForArrayFunction2,                                   \
    "Unexpected initial map for Array function (2)")                          \
  V(kUnexpectedInitialMapForArrayFunction,                                    \
    "Unexpected initial map for Array function")                              \
  V(kUnexpectedInitialMapForInternalArrayFunction,                            \
    "Unexpected initial map for InternalArray function")                      \
  V(kUnexpectedLevelAfterReturnFromApiCall,                                   \
    "Unexpected level after return from api call")                            \
  V(kUnexpectedNegativeValue, "Unexpected negative value")                    \
  V(kUnexpectedNumberOfPreAllocatedPropertyFields,                            \
    "Unexpected number of pre-allocated property fields")                     \
  V(kUnexpectedSmi, "Unexpected smi value")                                   \
  V(kUnexpectedStringFunction, "Unexpected String function")                  \
  V(kUnexpectedStringType, "Unexpected string type")                          \
  V(kUnexpectedStringWrapperInstanceSize,                                     \
    "Unexpected string wrapper instance size")                                \
  V(kUnexpectedTypeForRegExpDataFixedArrayExpected,                           \
    "Unexpected type for RegExp data, FixedArray expected")                   \
  V(kUnexpectedValue, "Unexpected value")                                     \
  V(kUnexpectedUnusedPropertiesOfStringWrapper,                               \
    "Unexpected unused properties of string wrapper")                         \
  V(kUnimplemented, "unimplemented")                                          \
  V(kUninitializedKSmiConstantRegister, "Uninitialized kSmiConstantRegister") \
  V(kUnknown, "Unknown")                                                      \
  V(kUnsupportedConstCompoundAssignment,                                      \
    "Unsupported const compound assignment")                                  \
  V(kUnsupportedCountOperationWithConst,                                      \
    "Unsupported count operation with const")                                 \
  V(kUnsupportedDoubleImmediate, "Unsupported double immediate")              \
  V(kUnsupportedLetCompoundAssignment, "Unsupported let compound assignment") \
  V(kUnsupportedLookupSlotInDeclaration,                                      \
    "Unsupported lookup slot in declaration")                                 \
  V(kUnsupportedNonPrimitiveCompare, "Unsupported non-primitive compare")     \
  V(kUnsupportedPhiUseOfArguments, "Unsupported phi use of arguments")        \
  V(kUnsupportedPhiUseOfConstVariable,                                        \
    "Unsupported phi use of const variable")                                  \
  V(kUnsupportedTaggedImmediate, "Unsupported tagged immediate")              \
  V(kVariableResolvedToWithContext, "Variable resolved to with context")      \
  V(kWeShouldNotHaveAnEmptyLexicalContext,                                    \
    "We should not have an empty lexical context")                            \
  V(kWithStatement, "WithStatement")                                          \
  V(kWrongAddressOrValuePassedToRecordWrite,                                  \
    "Wrong address or value passed to RecordWrite")                           \
  V(kYield, "Yield")


#define ERROR_MESSAGES_CONSTANTS(C, T) C,
enum BailoutReason {
  ERROR_MESSAGES_LIST(ERROR_MESSAGES_CONSTANTS)
  kLastErrorMessage
};
#undef ERROR_MESSAGES_CONSTANTS


const char* GetBailoutReason(BailoutReason reason);


// Object is the abstract superclass for all classes in the
// object hierarchy.
// Object does not use any virtual functions to avoid the
// allocation of the C++ vtable.
// Since Smi and Failure are subclasses of Object no
// data members can be present in Object.
class Object : public MaybeObject {
 public:
  // Type testing.
  bool IsObject() { return true; }

#define IS_TYPE_FUNCTION_DECL(type_)  inline bool Is##type_();
  OBJECT_TYPE_LIST(IS_TYPE_FUNCTION_DECL)
  HEAP_OBJECT_TYPE_LIST(IS_TYPE_FUNCTION_DECL)
#undef IS_TYPE_FUNCTION_DECL

  inline bool IsFixedArrayBase();
  inline bool IsExternal();
  inline bool IsAccessorInfo();

  inline bool IsStruct();
#define DECLARE_STRUCT_PREDICATE(NAME, Name, name) inline bool Is##Name();
  STRUCT_LIST(DECLARE_STRUCT_PREDICATE)
#undef DECLARE_STRUCT_PREDICATE

  INLINE(bool IsSpecObject());
  INLINE(bool IsSpecFunction());
  bool IsCallable();

  // Oddball testing.
  INLINE(bool IsUndefined());
  INLINE(bool IsNull());
  INLINE(bool IsTheHole());  // Shadows MaybeObject's implementation.
  INLINE(bool IsUninitialized());
  INLINE(bool IsTrue());
  INLINE(bool IsFalse());
  inline bool IsArgumentsMarker();
  inline bool NonFailureIsHeapObject();

  // Filler objects (fillers and free space objects).
  inline bool IsFiller();

  // Extract the number.
  inline double Number();
  inline bool IsNaN();
  bool ToInt32(int32_t* value);
  bool ToUint32(uint32_t* value);

  // Indicates whether OptimalRepresentation can do its work, or whether it
  // always has to return Representation::Tagged().
  enum ValueType {
    OPTIMAL_REPRESENTATION,
    FORCE_TAGGED
  };

  inline Representation OptimalRepresentation(
      ValueType type = OPTIMAL_REPRESENTATION) {
    if (!FLAG_track_fields) return Representation::Tagged();
    if (type == FORCE_TAGGED) return Representation::Tagged();
    if (IsSmi()) {
      return Representation::Smi();
    } else if (FLAG_track_double_fields && IsHeapNumber()) {
      return Representation::Double();
    } else if (FLAG_track_computed_fields && IsUninitialized()) {
      return Representation::None();
    } else if (FLAG_track_heap_object_fields) {
      ASSERT(IsHeapObject());
      return Representation::HeapObject();
    } else {
      return Representation::Tagged();
    }
  }

  inline bool FitsRepresentation(Representation representation) {
    if (FLAG_track_fields && representation.IsNone()) {
      return false;
    } else if (FLAG_track_fields && representation.IsSmi()) {
      return IsSmi();
    } else if (FLAG_track_double_fields && representation.IsDouble()) {
      return IsNumber();
    } else if (FLAG_track_heap_object_fields && representation.IsHeapObject()) {
      return IsHeapObject();
    }
    return true;
  }

  inline MaybeObject* AllocateNewStorageFor(Heap* heap,
                                            Representation representation);

  // Returns true if the object is of the correct type to be used as a
  // implementation of a JSObject's elements.
  inline bool HasValidElements();

  inline bool HasSpecificClassOf(String* name);

  MUST_USE_RESULT MaybeObject* ToObject(Isolate* isolate);  // ECMA-262 9.9.
  bool BooleanValue();                                      // ECMA-262 9.2.

  // Convert to a JSObject if needed.
  // native_context is used when creating wrapper object.
  MUST_USE_RESULT MaybeObject* ToObject(Context* native_context);

  // Converts this to a Smi if possible.
  // Failure is returned otherwise.
  MUST_USE_RESULT inline MaybeObject* ToSmi();

  void Lookup(Name* name, LookupResult* result);

  // Property access.
  MUST_USE_RESULT inline MaybeObject* GetProperty(Name* key);
  MUST_USE_RESULT inline MaybeObject* GetProperty(
      Name* key,
      PropertyAttributes* attributes);

  // TODO(yangguo): this should eventually replace the non-handlified version.
  static Handle<Object> GetPropertyWithReceiver(Handle<Object> object,
                                                Handle<Object> receiver,
                                                Handle<Name> name,
                                                PropertyAttributes* attributes);
  MUST_USE_RESULT MaybeObject* GetPropertyWithReceiver(
      Object* receiver,
      Name* key,
      PropertyAttributes* attributes);

  static Handle<Object> GetProperty(Handle<Object> object,
                                    Handle<Name> key);
  static Handle<Object> GetProperty(Handle<Object> object,
                                    Handle<Object> receiver,
                                    LookupResult* result,
                                    Handle<Name> key,
                                    PropertyAttributes* attributes);

  MUST_USE_RESULT static MaybeObject* GetPropertyOrFail(
      Handle<Object> object,
      Handle<Object> receiver,
      LookupResult* result,
      Handle<Name> key,
      PropertyAttributes* attributes);

  MUST_USE_RESULT MaybeObject* GetProperty(Object* receiver,
                                           LookupResult* result,
                                           Name* key,
                                           PropertyAttributes* attributes);

  MUST_USE_RESULT MaybeObject* GetPropertyWithDefinedGetter(Object* receiver,
                                                            JSReceiver* getter);

  static Handle<Object> GetElement(Isolate* isolate,
                                   Handle<Object> object,
                                   uint32_t index);
  MUST_USE_RESULT inline MaybeObject* GetElement(Isolate* isolate,
                                                 uint32_t index);
  // For use when we know that no exception can be thrown.
  inline Object* GetElementNoExceptionThrown(Isolate* isolate, uint32_t index);
  MUST_USE_RESULT MaybeObject* GetElementWithReceiver(Isolate* isolate,
                                                      Object* receiver,
                                                      uint32_t index);

  // Return the object's prototype (might be Heap::null_value()).
  Object* GetPrototype(Isolate* isolate);
  Map* GetMarkerMap(Isolate* isolate);

  // Returns the permanent hash code associated with this object. May return
  // undefined if not yet created.
  Object* GetHash();

  // Returns the permanent hash code associated with this object depending on
  // the actual object type. May create and store a hash code if needed and none
  // exists.
  // TODO(rafaelw): Remove isolate parameter when objects.cc is fully
  // handlified.
  static Handle<Object> GetOrCreateHash(Handle<Object> object,
                                        Isolate* isolate);

  // Checks whether this object has the same value as the given one.  This
  // function is implemented according to ES5, section 9.12 and can be used
  // to implement the Harmony "egal" function.
  bool SameValue(Object* other);

  // Tries to convert an object to an array index.  Returns true and sets
  // the output parameter if it succeeds.
  inline bool ToArrayIndex(uint32_t* index);

  // Returns true if this is a JSValue containing a string and the index is
  // < the length of the string.  Used to implement [] on strings.
  inline bool IsStringObjectWithCharacterAt(uint32_t index);

#ifdef VERIFY_HEAP
  // Verify a pointer is a valid object pointer.
  static void VerifyPointer(Object* p);
#endif

  inline void VerifyApiCallResultType();

  // Prints this object without details.
  void ShortPrint(FILE* out = stdout);

  // Prints this object without details to a message accumulator.
  void ShortPrint(StringStream* accumulator);

  // Casting: This cast is only needed to satisfy macros in objects-inl.h.
  static Object* cast(Object* value) { return value; }

  // Layout description.
  static const int kHeaderSize = 0;  // Object does not take up any space.

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(Object);
};


// Smi represents integer Numbers that can be stored in 31 bits.
// Smis are immediate which means they are NOT allocated in the heap.
// The this pointer has the following format: [31 bit signed int] 0
// For long smis it has the following format:
//     [32 bit signed int] [31 bits zero padding] 0
// Smi stands for small integer.
class Smi: public Object {
 public:
  // Returns the integer value.
  inline int value();

  // Convert a value to a Smi object.
  static inline Smi* FromInt(int value);

  static inline Smi* FromIntptr(intptr_t value);

  // Returns whether value can be represented in a Smi.
  static inline bool IsValid(intptr_t value);

  // Casting.
  static inline Smi* cast(Object* object);

  // Dispatched behavior.
  void SmiPrint(FILE* out = stdout);
  void SmiPrint(StringStream* accumulator);

  DECLARE_VERIFIER(Smi)

  static const int kMinValue =
      (static_cast<unsigned int>(-1)) << (kSmiValueSize - 1);
  static const int kMaxValue = -(kMinValue + 1);

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(Smi);
};


// Failure is used for reporting out of memory situations and
// propagating exceptions through the runtime system.  Failure objects
// are transient and cannot occur as part of the object graph.
//
// Failures are a single word, encoded as follows:
// +-------------------------+---+--+--+
// |.........unused..........|sss|tt|11|
// +-------------------------+---+--+--+
//                          7 6 4 32 10
//
//
// The low two bits, 0-1, are the failure tag, 11.  The next two bits,
// 2-3, are a failure type tag 'tt' with possible values:
//   00 RETRY_AFTER_GC
//   01 EXCEPTION
//   10 INTERNAL_ERROR
//   11 OUT_OF_MEMORY_EXCEPTION
//
// The next three bits, 4-6, are an allocation space tag 'sss'.  The
// allocation space tag is 000 for all failure types except
// RETRY_AFTER_GC.  For RETRY_AFTER_GC, the possible values are the
// allocation spaces (the encoding is found in globals.h).

// Failure type tag info.
const int kFailureTypeTagSize = 2;
const int kFailureTypeTagMask = (1 << kFailureTypeTagSize) - 1;

class Failure: public MaybeObject {
 public:
  // RuntimeStubs assumes EXCEPTION = 1 in the compiler-generated code.
  enum Type {
    RETRY_AFTER_GC = 0,
    EXCEPTION = 1,       // Returning this marker tells the real exception
                         // is in Isolate::pending_exception.
    INTERNAL_ERROR = 2,
    OUT_OF_MEMORY_EXCEPTION = 3
  };

  inline Type type() const;

  // Returns the space that needs to be collected for RetryAfterGC failures.
  inline AllocationSpace allocation_space() const;

  inline bool IsInternalError() const;
  inline bool IsOutOfMemoryException() const;

  static inline Failure* RetryAfterGC(AllocationSpace space);
  static inline Failure* RetryAfterGC();  // NEW_SPACE
  static inline Failure* Exception();
  static inline Failure* InternalError();
  // TODO(jkummerow): The value is temporary instrumentation. Remove it
  // when it has served its purpose.
  static inline Failure* OutOfMemoryException(intptr_t value);
  // Casting.
  static inline Failure* cast(MaybeObject* object);

  // Dispatched behavior.
  void FailurePrint(FILE* out = stdout);
  void FailurePrint(StringStream* accumulator);

  DECLARE_VERIFIER(Failure)

 private:
  inline intptr_t value() const;
  static inline Failure* Construct(Type type, intptr_t value = 0);

  DISALLOW_IMPLICIT_CONSTRUCTORS(Failure);
};


// Heap objects typically have a map pointer in their first word.  However,
// during GC other data (e.g. mark bits, forwarding addresses) is sometimes
// encoded in the first word.  The class MapWord is an abstraction of the
// value in a heap object's first word.
class MapWord BASE_EMBEDDED {
 public:
  // Normal state: the map word contains a map pointer.

  // Create a map word from a map pointer.
  static inline MapWord FromMap(Map* map);

  // View this map word as a map pointer.
  inline Map* ToMap();


  // Scavenge collection: the map word of live objects in the from space
  // contains a forwarding address (a heap object pointer in the to space).

  // True if this map word is a forwarding address for a scavenge
  // collection.  Only valid during a scavenge collection (specifically,
  // when all map words are heap object pointers, i.e. not during a full GC).
  inline bool IsForwardingAddress();

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
  inline Map* map();
  inline void set_map(Map* value);
  // The no-write-barrier version.  This is OK if the object is white and in
  // new space, or if the value is an immortal immutable object, like the maps
  // of primitive (non-JS) objects like strings, heap numbers etc.
  inline void set_map_no_write_barrier(Map* value);

  // During garbage collection, the map word of a heap object does not
  // necessarily contain a map pointer.
  inline MapWord map_word();
  inline void set_map_word(MapWord map_word);

  // The Heap the object was allocated in. Used also to access Isolate.
  inline Heap* GetHeap();

  // Convenience method to get current isolate.
  inline Isolate* GetIsolate();

  // Converts an address to a HeapObject pointer.
  static inline HeapObject* FromAddress(Address address);

  // Returns the address of this HeapObject.
  inline Address address();

  // Iterates over pointers contained in the object (including the Map)
  void Iterate(ObjectVisitor* v);

  // Iterates over all pointers contained in the object except the
  // first map pointer.  The object type is given in the first
  // parameter. This function does not access the map pointer in the
  // object, and so is safe to call while the map pointer is modified.
  void IterateBody(InstanceType type, int object_size, ObjectVisitor* v);

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

  // Casting.
  static inline HeapObject* cast(Object* obj);

  // Return the write barrier mode for this. Callers of this function
  // must be able to present a reference to an DisallowHeapAllocation
  // object as a sign that they are not going to use this function
  // from code that allocates and thus invalidates the returned write
  // barrier mode.
  inline WriteBarrierMode GetWriteBarrierMode(
      const DisallowHeapAllocation& promise);

  // Dispatched behavior.
  void HeapObjectShortPrint(StringStream* accumulator);
#ifdef OBJECT_PRINT
  void PrintHeader(FILE* out, const char* id);
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

  // Layout description.
  // First field in a heap object is map.
  static const int kMapOffset = Object::kHeaderSize;
  static const int kHeaderSize = kMapOffset + kPointerSize;

  STATIC_CHECK(kMapOffset == Internals::kHeapObjectMapOffset);

 protected:
  // helpers for calling an ObjectVisitor to iterate over pointers in the
  // half-open range [start, end) specified as integer offsets
  inline void IteratePointers(ObjectVisitor* v, int start, int end);
  // as above, for the single element at "offset"
  inline void IteratePointer(ObjectVisitor* v, int offset);

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(HeapObject);
};


// This class describes a body of an object of a fixed size
// in which all pointer fields are located in the [start_offset, end_offset)
// interval.
template<int start_offset, int end_offset, int size>
class FixedBodyDescriptor {
 public:
  static const int kStartOffset = start_offset;
  static const int kEndOffset = end_offset;
  static const int kSize = size;

  static inline void IterateBody(HeapObject* obj, ObjectVisitor* v);

  template<typename StaticVisitor>
  static inline void IterateBody(HeapObject* obj) {
    StaticVisitor::VisitPointers(HeapObject::RawField(obj, start_offset),
                                 HeapObject::RawField(obj, end_offset));
  }
};


// This class describes a body of an object of a variable size
// in which all pointer fields are located in the [start_offset, object_size)
// interval.
template<int start_offset>
class FlexibleBodyDescriptor {
 public:
  static const int kStartOffset = start_offset;

  static inline void IterateBody(HeapObject* obj,
                                 int object_size,
                                 ObjectVisitor* v);

  template<typename StaticVisitor>
  static inline void IterateBody(HeapObject* obj, int object_size) {
    StaticVisitor::VisitPointers(HeapObject::RawField(obj, start_offset),
                                 HeapObject::RawField(obj, object_size));
  }
};


// The HeapNumber class describes heap allocated numbers that cannot be
// represented in a Smi (small integer)
class HeapNumber: public HeapObject {
 public:
  // [value]: number value.
  inline double value();
  inline void set_value(double value);

  // Casting.
  static inline HeapNumber* cast(Object* obj);

  // Dispatched behavior.
  bool HeapNumberBooleanValue();

  void HeapNumberPrint(FILE* out = stdout);
  void HeapNumberPrint(StringStream* accumulator);
  DECLARE_VERIFIER(HeapNumber)

  inline int get_exponent();
  inline int get_sign();

  // Layout description.
  static const int kValueOffset = HeapObject::kHeaderSize;
  // IEEE doubles are two 32 bit words.  The first is just mantissa, the second
  // is a mixture of sign, exponent and mantissa.  Our current platforms are all
  // little endian apart from non-EABI arm which is little endian with big
  // endian floating point word ordering!
  static const int kMantissaOffset = kValueOffset;
  static const int kExponentOffset = kValueOffset + 4;

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


// Indicates whether a property should be set or (re)defined.  Setting of a
// property causes attributes to remain unchanged, writability to be checked
// and callbacks to be called.  Defining of a property causes attributes to
// be updated and callbacks to be overridden.
enum SetPropertyMode {
  SET_PROPERTY,
  DEFINE_PROPERTY
};


// Indicator for one component of an AccessorPair.
enum AccessorComponent {
  ACCESSOR_GETTER,
  ACCESSOR_SETTER
};


// JSReceiver includes types on which properties can be defined, i.e.,
// JSObject and JSProxy.
class JSReceiver: public HeapObject {
 public:
  enum DeleteMode {
    NORMAL_DELETION,
    STRICT_DELETION,
    FORCE_DELETION
  };

  // A non-keyed store is of the form a.x = foo or a["x"] = foo whereas
  // a keyed store is of the form a[expression] = foo.
  enum StoreFromKeyed {
    MAY_BE_STORE_FROM_KEYED,
    CERTAINLY_NOT_STORE_FROM_KEYED
  };

  // Internal properties (e.g. the hidden properties dictionary) might
  // be added even though the receiver is non-extensible.
  enum ExtensibilityCheck {
    PERFORM_EXTENSIBILITY_CHECK,
    OMIT_EXTENSIBILITY_CHECK
  };

  // Casting.
  static inline JSReceiver* cast(Object* obj);

  // Implementation of [[Put]], ECMA-262 5th edition, section 8.12.5.
  static Handle<Object> SetProperty(Handle<JSReceiver> object,
                                    Handle<Name> key,
                                    Handle<Object> value,
                                    PropertyAttributes attributes,
                                    StrictModeFlag strict_mode,
                                    StoreFromKeyed store_mode =
                                        MAY_BE_STORE_FROM_KEYED);
  static Handle<Object> SetElement(Handle<JSReceiver> object,
                                   uint32_t index,
                                   Handle<Object> value,
                                   PropertyAttributes attributes,
                                   StrictModeFlag strict_mode);

  // Implementation of [[HasProperty]], ECMA-262 5th edition, section 8.12.6.
  static inline bool HasProperty(Handle<JSReceiver> object, Handle<Name> name);
  static inline bool HasLocalProperty(Handle<JSReceiver>, Handle<Name> name);
  static inline bool HasElement(Handle<JSReceiver> object, uint32_t index);
  static inline bool HasLocalElement(Handle<JSReceiver> object, uint32_t index);

  // Implementation of [[Delete]], ECMA-262 5th edition, section 8.12.7.
  static Handle<Object> DeleteProperty(Handle<JSReceiver> object,
                                       Handle<Name> name,
                                       DeleteMode mode = NORMAL_DELETION);
  static Handle<Object> DeleteElement(Handle<JSReceiver> object,
                                      uint32_t index,
                                      DeleteMode mode = NORMAL_DELETION);

  // Tests for the fast common case for property enumeration.
  bool IsSimpleEnum();

  // Returns the class name ([[Class]] property in the specification).
  String* class_name();

  // Returns the constructor name (the name (possibly, inferred name) of the
  // function that was used to instantiate the object).
  String* constructor_name();

  inline PropertyAttributes GetPropertyAttribute(Name* name);
  PropertyAttributes GetPropertyAttributeWithReceiver(JSReceiver* receiver,
                                                      Name* name);
  PropertyAttributes GetLocalPropertyAttribute(Name* name);

  inline PropertyAttributes GetElementAttribute(uint32_t index);
  inline PropertyAttributes GetLocalElementAttribute(uint32_t index);

  // Return the object's prototype (might be Heap::null_value()).
  inline Object* GetPrototype();

  // Return the constructor function (may be Heap::null_value()).
  inline Object* GetConstructor();

  // Retrieves a permanent object identity hash code. The undefined value might
  // be returned in case no hash was created yet.
  inline Object* GetIdentityHash();

  // Retrieves a permanent object identity hash code. May create and store a
  // hash code if needed and none exists.
  inline static Handle<Object> GetOrCreateIdentityHash(
      Handle<JSReceiver> object);

  // Lookup a property.  If found, the result is valid and has
  // detailed information.
  void LocalLookup(Name* name, LookupResult* result,
                   bool search_hidden_prototypes = false);
  void Lookup(Name* name, LookupResult* result);

 protected:
  Smi* GenerateIdentityHash();

  static Handle<Object> SetPropertyWithDefinedSetter(Handle<JSReceiver> object,
                                                     Handle<JSReceiver> setter,
                                                     Handle<Object> value);

 private:
  PropertyAttributes GetPropertyAttributeForResult(JSReceiver* receiver,
                                                   LookupResult* result,
                                                   Name* name,
                                                   bool continue_search);

  static Handle<Object> SetProperty(Handle<JSReceiver> receiver,
                                    LookupResult* result,
                                    Handle<Name> key,
                                    Handle<Object> value,
                                    PropertyAttributes attributes,
                                    StrictModeFlag strict_mode,
                                    StoreFromKeyed store_from_keyed);

  DISALLOW_IMPLICIT_CONSTRUCTORS(JSReceiver);
};

// Forward declaration for JSObject::GetOrCreateHiddenPropertiesHashTable.
class ObjectHashTable;

// The JSObject describes real heap allocated JavaScript objects with
// properties.
// Note that the map of JSObject changes during execution to enable inline
// caching.
class JSObject: public JSReceiver {
 public:
  // [properties]: Backing storage for properties.
  // properties is a FixedArray in the fast case and a Dictionary in the
  // slow case.
  DECL_ACCESSORS(properties, FixedArray)  // Get and set fast properties.
  inline void initialize_properties();
  inline bool HasFastProperties();
  inline NameDictionary* property_dictionary();  // Gets slow properties.

  // [elements]: The elements (properties with names that are integers).
  //
  // Elements can be in two general modes: fast and slow. Each mode
  // corrensponds to a set of object representations of elements that
  // have something in common.
  //
  // In the fast mode elements is a FixedArray and so each element can
  // be quickly accessed. This fact is used in the generated code. The
  // elements array can have one of three maps in this mode:
  // fixed_array_map, non_strict_arguments_elements_map or
  // fixed_cow_array_map (for copy-on-write arrays). In the latter case
  // the elements array may be shared by a few objects and so before
  // writing to any element the array must be copied. Use
  // EnsureWritableFastElements in this case.
  //
  // In the slow mode the elements is either a NumberDictionary, an
  // ExternalArray, or a FixedArray parameter map for a (non-strict)
  // arguments object.
  DECL_ACCESSORS(elements, FixedArrayBase)
  inline void initialize_elements();
  MUST_USE_RESULT inline MaybeObject* ResetElements();
  inline ElementsKind GetElementsKind();
  inline ElementsAccessor* GetElementsAccessor();
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
  inline bool HasNonStrictArgumentsElements();
  inline bool HasDictionaryElements();

  inline bool HasExternalUint8ClampedElements();
  inline bool HasExternalArrayElements();
  inline bool HasExternalInt8Elements();
  inline bool HasExternalUint8Elements();
  inline bool HasExternalInt16Elements();
  inline bool HasExternalUint16Elements();
  inline bool HasExternalInt32Elements();
  inline bool HasExternalUint32Elements();
  inline bool HasExternalFloat32Elements();
  inline bool HasExternalFloat64Elements();

  inline bool HasFixedTypedArrayElements();

  bool HasFastArgumentsElements();
  bool HasDictionaryArgumentsElements();
  inline SeededNumberDictionary* element_dictionary();  // Gets slow elements.

  inline void set_map_and_elements(
      Map* map,
      FixedArrayBase* value,
      WriteBarrierMode mode = UPDATE_WRITE_BARRIER);

  // Requires: HasFastElements().
  static Handle<FixedArray> EnsureWritableFastElements(
      Handle<JSObject> object);
  MUST_USE_RESULT inline MaybeObject* EnsureWritableFastElements();

  // Collects elements starting at index 0.
  // Undefined values are placed after non-undefined values.
  // Returns the number of non-undefined values.
  static Handle<Object> PrepareElementsForSort(Handle<JSObject> object,
                                               uint32_t limit);
  // As PrepareElementsForSort, but only on objects where elements is
  // a dictionary, and it will stay a dictionary.
  static Handle<Object> PrepareSlowElementsForSort(Handle<JSObject> object,
                                                   uint32_t limit);
  MUST_USE_RESULT MaybeObject* PrepareSlowElementsForSort(uint32_t limit);

  static Handle<Object> GetPropertyWithCallback(Handle<JSObject> object,
                                                Handle<Object> receiver,
                                                Handle<Object> structure,
                                                Handle<Name> name);

  static Handle<Object> SetPropertyWithCallback(
      Handle<JSObject> object,
      Handle<Object> structure,
      Handle<Name> name,
      Handle<Object> value,
      Handle<JSObject> holder,
      StrictModeFlag strict_mode);

  static Handle<Object> SetPropertyWithInterceptor(
      Handle<JSObject> object,
      Handle<Name> name,
      Handle<Object> value,
      PropertyAttributes attributes,
      StrictModeFlag strict_mode);

  static Handle<Object> SetPropertyForResult(
      Handle<JSObject> object,
      LookupResult* result,
      Handle<Name> name,
      Handle<Object> value,
      PropertyAttributes attributes,
      StrictModeFlag strict_mode,
      StoreFromKeyed store_mode = MAY_BE_STORE_FROM_KEYED);

  static Handle<Object> SetLocalPropertyIgnoreAttributes(
      Handle<JSObject> object,
      Handle<Name> key,
      Handle<Object> value,
      PropertyAttributes attributes,
      ValueType value_type = OPTIMAL_REPRESENTATION,
      StoreMode mode = ALLOW_AS_CONSTANT,
      ExtensibilityCheck extensibility_check = PERFORM_EXTENSIBILITY_CHECK);

  static inline Handle<String> ExpectedTransitionKey(Handle<Map> map);
  static inline Handle<Map> ExpectedTransitionTarget(Handle<Map> map);

  // Try to follow an existing transition to a field with attributes NONE. The
  // return value indicates whether the transition was successful.
  static inline Handle<Map> FindTransitionToField(Handle<Map> map,
                                                  Handle<Name> key);

  // Extend the receiver with a single fast property appeared first in the
  // passed map. This also extends the property backing store if necessary.
  static void AllocateStorageForMap(Handle<JSObject> object, Handle<Map> map);

  // Migrates the given object to a map whose field representations are the
  // lowest upper bound of all known representations for that field.
  static void MigrateInstance(Handle<JSObject> instance);

  // Migrates the given object only if the target map is already available,
  // or returns an empty handle if such a map is not yet available.
  static Handle<Object> TryMigrateInstance(Handle<JSObject> instance);

  // Retrieve a value in a normalized object given a lookup result.
  // Handles the special representation of JS global objects.
  Object* GetNormalizedProperty(const LookupResult* result);

  // Sets the property value in a normalized object given a lookup result.
  // Handles the special representation of JS global objects.
  static void SetNormalizedProperty(Handle<JSObject> object,
                                    const LookupResult* result,
                                    Handle<Object> value);

  // Sets the property value in a normalized object given (key, value, details).
  // Handles the special representation of JS global objects.
  static void SetNormalizedProperty(Handle<JSObject> object,
                                    Handle<Name> key,
                                    Handle<Object> value,
                                    PropertyDetails details);

  static void OptimizeAsPrototype(Handle<JSObject> object);

  // Retrieve interceptors.
  InterceptorInfo* GetNamedInterceptor();
  InterceptorInfo* GetIndexedInterceptor();

  // Used from JSReceiver.
  PropertyAttributes GetPropertyAttributePostInterceptor(JSObject* receiver,
                                                         Name* name,
                                                         bool continue_search);
  PropertyAttributes GetPropertyAttributeWithInterceptor(JSObject* receiver,
                                                         Name* name,
                                                         bool continue_search);
  PropertyAttributes GetPropertyAttributeWithFailedAccessCheck(
      Object* receiver,
      LookupResult* result,
      Name* name,
      bool continue_search);
  PropertyAttributes GetElementAttributeWithReceiver(JSReceiver* receiver,
                                                     uint32_t index,
                                                     bool continue_search);

  // Retrieves an AccessorPair property from the given object. Might return
  // undefined if the property doesn't exist or is of a different kind.
  static Handle<Object> GetAccessor(Handle<JSObject> object,
                                    Handle<Name> name,
                                    AccessorComponent component);

  // Defines an AccessorPair property on the given object.
  // TODO(mstarzinger): Rename to SetAccessor() and return empty handle on
  // exception instead of letting callers check for scheduled exception.
  static void DefineAccessor(Handle<JSObject> object,
                             Handle<Name> name,
                             Handle<Object> getter,
                             Handle<Object> setter,
                             PropertyAttributes attributes,
                             v8::AccessControl access_control = v8::DEFAULT);

  // Defines an AccessorInfo property on the given object.
  static Handle<Object> SetAccessor(Handle<JSObject> object,
                                    Handle<AccessorInfo> info);

  static Handle<Object> GetPropertyWithInterceptor(
      Handle<JSObject> object,
      Handle<Object> receiver,
      Handle<Name> name,
      PropertyAttributes* attributes);
  static Handle<Object> GetPropertyPostInterceptor(
      Handle<JSObject> object,
      Handle<Object> receiver,
      Handle<Name> name,
      PropertyAttributes* attributes);
  MUST_USE_RESULT MaybeObject* GetLocalPropertyPostInterceptor(
      Object* receiver,
      Name* name,
      PropertyAttributes* attributes);

  // Returns true if this is an instance of an api function and has
  // been modified since it was created.  May give false positives.
  bool IsDirty();

  // Accessors for hidden properties object.
  //
  // Hidden properties are not local properties of the object itself.
  // Instead they are stored in an auxiliary structure kept as a local
  // property with a special name Heap::hidden_string(). But if the
  // receiver is a JSGlobalProxy then the auxiliary object is a property
  // of its prototype, and if it's a detached proxy, then you can't have
  // hidden properties.

  // Sets a hidden property on this object. Returns this object if successful,
  // undefined if called on a detached proxy.
  static Handle<Object> SetHiddenProperty(Handle<JSObject> object,
                                          Handle<Name> key,
                                          Handle<Object> value);
  // Gets the value of a hidden property with the given key. Returns the hole
  // if the property doesn't exist (or if called on a detached proxy),
  // otherwise returns the value set for the key.
  Object* GetHiddenProperty(Name* key);
  // Deletes a hidden property. Deleting a non-existing property is
  // considered successful.
  static void DeleteHiddenProperty(Handle<JSObject> object,
                                   Handle<Name> key);
  // Returns true if the object has a property with the hidden string as name.
  bool HasHiddenProperties();

  static void SetIdentityHash(Handle<JSObject> object, Handle<Smi> hash);

  inline void ValidateElements();

  // Makes sure that this object can contain HeapObject as elements.
  static inline void EnsureCanContainHeapObjectElements(Handle<JSObject> obj);

  // Makes sure that this object can contain the specified elements.
  MUST_USE_RESULT inline MaybeObject* EnsureCanContainElements(
      Object** elements,
      uint32_t count,
      EnsureElementsMode mode);
  MUST_USE_RESULT inline MaybeObject* EnsureCanContainElements(
      FixedArrayBase* elements,
      uint32_t length,
      EnsureElementsMode mode);
  MUST_USE_RESULT MaybeObject* EnsureCanContainElements(
      Arguments* arguments,
      uint32_t first_arg,
      uint32_t arg_count,
      EnsureElementsMode mode);

  // Do we want to keep the elements in fast case when increasing the
  // capacity?
  bool ShouldConvertToSlowElements(int new_capacity);
  // Returns true if the backing storage for the slow-case elements of
  // this object takes up nearly as much space as a fast-case backing
  // storage would.  In that case the JSObject should have fast
  // elements.
  bool ShouldConvertToFastElements();
  // Returns true if the elements of JSObject contains only values that can be
  // represented in a FixedDoubleArray and has at least one value that can only
  // be represented as a double and not a Smi.
  bool ShouldConvertToFastDoubleElements(bool* has_smi_only_elements);

  // Computes the new capacity when expanding the elements of a JSObject.
  static int NewElementsCapacity(int old_capacity) {
    // (old_capacity + 50%) + 16
    return old_capacity + (old_capacity >> 1) + 16;
  }

  // These methods do not perform access checks!
  AccessorPair* GetLocalPropertyAccessorPair(Name* name);
  AccessorPair* GetLocalElementAccessorPair(uint32_t index);

  static Handle<Object> SetFastElement(Handle<JSObject> object, uint32_t index,
                                       Handle<Object> value,
                                       StrictModeFlag strict_mode,
                                       bool check_prototype);

  static Handle<Object> SetOwnElement(Handle<JSObject> object,
                                      uint32_t index,
                                      Handle<Object> value,
                                      StrictModeFlag strict_mode);

  // Empty handle is returned if the element cannot be set to the given value.
  static Handle<Object> SetElement(
      Handle<JSObject> object,
      uint32_t index,
      Handle<Object> value,
      PropertyAttributes attributes,
      StrictModeFlag strict_mode,
      bool check_prototype = true,
      SetPropertyMode set_mode = SET_PROPERTY);

  // Returns the index'th element.
  // The undefined object if index is out of bounds.
  MUST_USE_RESULT MaybeObject* GetElementWithInterceptor(Object* receiver,
                                                         uint32_t index);

  enum SetFastElementsCapacitySmiMode {
    kAllowSmiElements,
    kForceSmiElements,
    kDontAllowSmiElements
  };

  static Handle<FixedArray> SetFastElementsCapacityAndLength(
      Handle<JSObject> object,
      int capacity,
      int length,
      SetFastElementsCapacitySmiMode smi_mode);
  // Replace the elements' backing store with fast elements of the given
  // capacity.  Update the length for JSArrays.  Returns the new backing
  // store.
  MUST_USE_RESULT MaybeObject* SetFastElementsCapacityAndLength(
      int capacity,
      int length,
      SetFastElementsCapacitySmiMode smi_mode);
  static void SetFastDoubleElementsCapacityAndLength(
      Handle<JSObject> object,
      int capacity,
      int length);
  MUST_USE_RESULT MaybeObject* SetFastDoubleElementsCapacityAndLength(
      int capacity,
      int length);

  // Lookup interceptors are used for handling properties controlled by host
  // objects.
  inline bool HasNamedInterceptor();
  inline bool HasIndexedInterceptor();

  // Support functions for v8 api (needed for correct interceptor behavior).
  static bool HasRealNamedProperty(Handle<JSObject> object,
                                   Handle<Name> key);
  static bool HasRealElementProperty(Handle<JSObject> object, uint32_t index);
  static bool HasRealNamedCallbackProperty(Handle<JSObject> object,
                                           Handle<Name> key);

  // Get the header size for a JSObject.  Used to compute the index of
  // internal fields as well as the number of internal fields.
  inline int GetHeaderSize();

  inline int GetInternalFieldCount();
  inline int GetInternalFieldOffset(int index);
  inline Object* GetInternalField(int index);
  inline void SetInternalField(int index, Object* value);
  inline void SetInternalField(int index, Smi* value);

  // The following lookup functions skip interceptors.
  void LocalLookupRealNamedProperty(Name* name, LookupResult* result);
  void LookupRealNamedProperty(Name* name, LookupResult* result);
  void LookupRealNamedPropertyInPrototypes(Name* name, LookupResult* result);
  void LookupCallbackProperty(Name* name, LookupResult* result);

  // Returns the number of properties on this object filtering out properties
  // with the specified attributes (ignoring interceptors).
  int NumberOfLocalProperties(PropertyAttributes filter = NONE);
  // Fill in details for properties into storage starting at the specified
  // index.
  void GetLocalPropertyNames(
      FixedArray* storage, int index, PropertyAttributes filter = NONE);

  // Returns the number of properties on this object filtering out properties
  // with the specified attributes (ignoring interceptors).
  int NumberOfLocalElements(PropertyAttributes filter);
  // Returns the number of enumerable elements (ignoring interceptors).
  int NumberOfEnumElements();
  // Returns the number of elements on this object filtering out elements
  // with the specified attributes (ignoring interceptors).
  int GetLocalElementKeys(FixedArray* storage, PropertyAttributes filter);
  // Count and fill in the enumerable elements into storage.
  // (storage->length() == NumberOfEnumElements()).
  // If storage is NULL, will count the elements without adding
  // them to any storage.
  // Returns the number of enumerable elements.
  int GetEnumElementKeys(FixedArray* storage);

  // Returns a new map with all transitions dropped from the object's current
  // map and the ElementsKind set.
  static Handle<Map> GetElementsTransitionMap(Handle<JSObject> object,
                                              ElementsKind to_kind);
  inline MUST_USE_RESULT MaybeObject* GetElementsTransitionMap(
      Isolate* isolate,
      ElementsKind elements_kind);
  MUST_USE_RESULT MaybeObject* GetElementsTransitionMapSlow(
      ElementsKind elements_kind);

  static void TransitionElementsKind(Handle<JSObject> object,
                                     ElementsKind to_kind);

  MUST_USE_RESULT MaybeObject* TransitionElementsKind(ElementsKind to_kind);

  // TODO(mstarzinger): Both public because of ConvertAnsSetLocalProperty().
  static void MigrateToMap(Handle<JSObject> object, Handle<Map> new_map);
  static void GeneralizeFieldRepresentation(Handle<JSObject> object,
                                            int modify_index,
                                            Representation new_representation,
                                            StoreMode store_mode);

  // Convert the object to use the canonical dictionary
  // representation. If the object is expected to have additional properties
  // added this number can be indicated to have the backing store allocated to
  // an initial capacity for holding these properties.
  static void NormalizeProperties(Handle<JSObject> object,
                                  PropertyNormalizationMode mode,
                                  int expected_additional_properties);

  // Convert and update the elements backing store to be a
  // SeededNumberDictionary dictionary.  Returns the backing after conversion.
  static Handle<SeededNumberDictionary> NormalizeElements(
      Handle<JSObject> object);

  MUST_USE_RESULT MaybeObject* NormalizeElements();

  // Transform slow named properties to fast variants.
  static void TransformToFastProperties(Handle<JSObject> object,
                                        int unused_property_fields);

  // Access fast-case object properties at index.
  MUST_USE_RESULT inline MaybeObject* FastPropertyAt(
      Representation representation,
      int index);
  inline Object* RawFastPropertyAt(int index);
  inline void FastPropertyAtPut(int index, Object* value);

  // Access to in object properties.
  inline int GetInObjectPropertyOffset(int index);
  inline Object* InObjectPropertyAt(int index);
  inline Object* InObjectPropertyAtPut(int index,
                                       Object* value,
                                       WriteBarrierMode mode
                                       = UPDATE_WRITE_BARRIER);

  // Set the object's prototype (only JSReceiver and null are allowed values).
  static Handle<Object> SetPrototype(Handle<JSObject> object,
                                     Handle<Object> value,
                                     bool skip_hidden_prototypes = false);

  // Initializes the body after properties slot, properties slot is
  // initialized by set_properties.  Fill the pre-allocated fields with
  // pre_allocated_value and the rest with filler_value.
  // Note: this call does not update write barrier, the caller is responsible
  // to ensure that |filler_value| can be collected without WB here.
  inline void InitializeBody(Map* map,
                             Object* pre_allocated_value,
                             Object* filler_value);

  // Check whether this object references another object
  bool ReferencesObject(Object* obj);

  // Disalow further properties to be added to the object.
  static Handle<Object> PreventExtensions(Handle<JSObject> object);

  // ES5 Object.freeze
  static Handle<Object> Freeze(Handle<JSObject> object);

  // Called the first time an object is observed with ES7 Object.observe.
  static void SetObserved(Handle<JSObject> object);

  // Copy object.
  enum DeepCopyHints {
    kNoHints = 0,
    kObjectIsShallowArray = 1
  };

  static Handle<JSObject> Copy(Handle<JSObject> object);
  static Handle<JSObject> DeepCopy(Handle<JSObject> object,
                                   AllocationSiteUsageContext* site_context,
                                   DeepCopyHints hints = kNoHints);
  static Handle<JSObject> DeepWalk(Handle<JSObject> object,
                                   AllocationSiteCreationContext* site_context);

  // Casting.
  static inline JSObject* cast(Object* obj);

  // Dispatched behavior.
  void JSObjectShortPrint(StringStream* accumulator);
  DECLARE_PRINTER(JSObject)
  DECLARE_VERIFIER(JSObject)
#ifdef OBJECT_PRINT
  void PrintProperties(FILE* out = stdout);
  void PrintElements(FILE* out = stdout);
  void PrintTransitions(FILE* out = stdout);
#endif

  void PrintElementsTransition(
      FILE* file, ElementsKind from_kind, FixedArrayBase* from_elements,
      ElementsKind to_kind, FixedArrayBase* to_elements);

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

  // Maximal number of fast properties for the JSObject. Used to
  // restrict the number of map transitions to avoid an explosion in
  // the number of maps for objects used as dictionaries.
  inline bool TooManyFastProperties(
      StoreFromKeyed store_mode = MAY_BE_STORE_FROM_KEYED);

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

  // Note that Page::kMaxRegularHeapObjectSize puts a limit on
  // permissible values (see the ASSERT in heap.cc).
  static const int kInitialMaxFastElementArray = 100000;

  static const int kFastPropertiesSoftLimit = 12;
  static const int kMaxFastProperties = 64;
  static const int kMaxInstanceSize = 255 * kPointerSize;
  // When extending the backing storage for property values, we increase
  // its size by more than the 1 entry necessary, so sequentially adding fields
  // to the same object requires fewer allocations and copies.
  static const int kFieldsAdded = 3;

  // Layout description.
  static const int kPropertiesOffset = HeapObject::kHeaderSize;
  static const int kElementsOffset = kPropertiesOffset + kPointerSize;
  static const int kHeaderSize = kElementsOffset + kPointerSize;

  STATIC_CHECK(kHeaderSize == Internals::kJSObjectHeaderSize);

  class BodyDescriptor : public FlexibleBodyDescriptor<kPropertiesOffset> {
   public:
    static inline int SizeOf(Map* map, HeapObject* object);
  };

  // Enqueue change record for Object.observe. May cause GC.
  static void EnqueueChangeRecord(Handle<JSObject> object,
                                  const char* type,
                                  Handle<Name> name,
                                  Handle<Object> old_value);

 private:
  friend class DictionaryElementsAccessor;
  friend class JSReceiver;
  friend class Object;

  static void UpdateAllocationSite(Handle<JSObject> object,
                                   ElementsKind to_kind);
  MUST_USE_RESULT MaybeObject* UpdateAllocationSite(ElementsKind to_kind);

  // Used from Object::GetProperty().
  static Handle<Object> GetPropertyWithFailedAccessCheck(
      Handle<JSObject> object,
      Handle<Object> receiver,
      LookupResult* result,
      Handle<Name> name,
      PropertyAttributes* attributes);

  MUST_USE_RESULT MaybeObject* GetElementWithCallback(Object* receiver,
                                                      Object* structure,
                                                      uint32_t index,
                                                      Object* holder);
  MUST_USE_RESULT PropertyAttributes GetElementAttributeWithInterceptor(
      JSReceiver* receiver,
      uint32_t index,
      bool continue_search);
  MUST_USE_RESULT PropertyAttributes GetElementAttributeWithoutInterceptor(
      JSReceiver* receiver,
      uint32_t index,
      bool continue_search);
  static Handle<Object> SetElementWithCallback(
      Handle<JSObject> object,
      Handle<Object> structure,
      uint32_t index,
      Handle<Object> value,
      Handle<JSObject> holder,
      StrictModeFlag strict_mode);
  static Handle<Object> SetElementWithInterceptor(
      Handle<JSObject> object,
      uint32_t index,
      Handle<Object> value,
      PropertyAttributes attributes,
      StrictModeFlag strict_mode,
      bool check_prototype,
      SetPropertyMode set_mode);
  static Handle<Object> SetElementWithoutInterceptor(
      Handle<JSObject> object,
      uint32_t index,
      Handle<Object> value,
      PropertyAttributes attributes,
      StrictModeFlag strict_mode,
      bool check_prototype,
      SetPropertyMode set_mode);
  static Handle<Object> SetElementWithCallbackSetterInPrototypes(
      Handle<JSObject> object,
      uint32_t index,
      Handle<Object> value,
      bool* found,
      StrictModeFlag strict_mode);
  static Handle<Object> SetDictionaryElement(
      Handle<JSObject> object,
      uint32_t index,
      Handle<Object> value,
      PropertyAttributes attributes,
      StrictModeFlag strict_mode,
      bool check_prototype,
      SetPropertyMode set_mode = SET_PROPERTY);
  static Handle<Object> SetFastDoubleElement(
      Handle<JSObject> object,
      uint32_t index,
      Handle<Object> value,
      StrictModeFlag strict_mode,
      bool check_prototype = true);

  // Searches the prototype chain for property 'name'. If it is found and
  // has a setter, invoke it and set '*done' to true. If it is found and is
  // read-only, reject and set '*done' to true. Otherwise, set '*done' to
  // false. Can throw and return an empty handle with '*done==true'.
  static Handle<Object> SetPropertyViaPrototypes(
      Handle<JSObject> object,
      Handle<Name> name,
      Handle<Object> value,
      PropertyAttributes attributes,
      StrictModeFlag strict_mode,
      bool* done);
  static Handle<Object> SetPropertyPostInterceptor(
      Handle<JSObject> object,
      Handle<Name> name,
      Handle<Object> value,
      PropertyAttributes attributes,
      StrictModeFlag strict_mode);
  static Handle<Object> SetPropertyUsingTransition(
      Handle<JSObject> object,
      LookupResult* lookup,
      Handle<Name> name,
      Handle<Object> value,
      PropertyAttributes attributes);
  static Handle<Object> SetPropertyWithFailedAccessCheck(
      Handle<JSObject> object,
      LookupResult* result,
      Handle<Name> name,
      Handle<Object> value,
      bool check_prototype,
      StrictModeFlag strict_mode);

  // Add a property to an object.
  static Handle<Object> AddProperty(
      Handle<JSObject> object,
      Handle<Name> name,
      Handle<Object> value,
      PropertyAttributes attributes,
      StrictModeFlag strict_mode,
      StoreFromKeyed store_mode = MAY_BE_STORE_FROM_KEYED,
      ExtensibilityCheck extensibility_check = PERFORM_EXTENSIBILITY_CHECK,
      ValueType value_type = OPTIMAL_REPRESENTATION,
      StoreMode mode = ALLOW_AS_CONSTANT,
      TransitionFlag flag = INSERT_TRANSITION);

  // Add a constant function property to a fast-case object.
  // This leaves a CONSTANT_TRANSITION in the old map, and
  // if it is called on a second object with this map, a
  // normal property is added instead, with a map transition.
  // This avoids the creation of many maps with the same constant
  // function, all orphaned.
  static void AddConstantProperty(Handle<JSObject> object,
                                  Handle<Name> name,
                                  Handle<Object> constant,
                                  PropertyAttributes attributes,
                                  TransitionFlag flag);

  // Add a property to a fast-case object.
  static void AddFastProperty(Handle<JSObject> object,
                              Handle<Name> name,
                              Handle<Object> value,
                              PropertyAttributes attributes,
                              StoreFromKeyed store_mode,
                              ValueType value_type,
                              TransitionFlag flag);

  // Add a property to a fast-case object using a map transition to
  // new_map.
  static void AddFastPropertyUsingMap(Handle<JSObject> object,
                                      Handle<Map> new_map,
                                      Handle<Name> name,
                                      Handle<Object> value,
                                      int field_index,
                                      Representation representation);

  // Add a property to a slow-case object.
  static void AddSlowProperty(Handle<JSObject> object,
                              Handle<Name> name,
                              Handle<Object> value,
                              PropertyAttributes attributes);

  static Handle<Object> DeleteProperty(Handle<JSObject> object,
                                       Handle<Name> name,
                                       DeleteMode mode);
  static Handle<Object> DeletePropertyPostInterceptor(Handle<JSObject> object,
                                                      Handle<Name> name,
                                                      DeleteMode mode);
  static Handle<Object> DeletePropertyWithInterceptor(Handle<JSObject> object,
                                                      Handle<Name> name);

  // Deletes the named property in a normalized object.
  static Handle<Object> DeleteNormalizedProperty(Handle<JSObject> object,
                                                 Handle<Name> name,
                                                 DeleteMode mode);

  static Handle<Object> DeleteElement(Handle<JSObject> object,
                                      uint32_t index,
                                      DeleteMode mode);
  static Handle<Object> DeleteElementWithInterceptor(Handle<JSObject> object,
                                                     uint32_t index);

  bool ReferencesObjectFromElements(FixedArray* elements,
                                    ElementsKind kind,
                                    Object* object);

  // Returns true if most of the elements backing storage is used.
  bool HasDenseElements();

  // Gets the current elements capacity and the number of used elements.
  void GetElementsCapacityAndUsage(int* capacity, int* used);

  bool CanSetCallback(Name* name);
  static void SetElementCallback(Handle<JSObject> object,
                                 uint32_t index,
                                 Handle<Object> structure,
                                 PropertyAttributes attributes);
  static void SetPropertyCallback(Handle<JSObject> object,
                                  Handle<Name> name,
                                  Handle<Object> structure,
                                  PropertyAttributes attributes);
  static void DefineElementAccessor(Handle<JSObject> object,
                                    uint32_t index,
                                    Handle<Object> getter,
                                    Handle<Object> setter,
                                    PropertyAttributes attributes,
                                    v8::AccessControl access_control);
  static Handle<AccessorPair> CreateAccessorPairFor(Handle<JSObject> object,
                                                    Handle<Name> name);
  static void DefinePropertyAccessor(Handle<JSObject> object,
                                     Handle<Name> name,
                                     Handle<Object> getter,
                                     Handle<Object> setter,
                                     PropertyAttributes attributes,
                                     v8::AccessControl access_control);

  // Try to define a single accessor paying attention to map transitions.
  // Returns false if this was not possible and we have to use the slow case.
  static bool DefineFastAccessor(Handle<JSObject> object,
                                 Handle<Name> name,
                                 AccessorComponent component,
                                 Handle<Object> accessor,
                                 PropertyAttributes attributes);


  // Return the hash table backing store or the inline stored identity hash,
  // whatever is found.
  MUST_USE_RESULT Object* GetHiddenPropertiesHashTable();

  // Return the hash table backing store for hidden properties.  If there is no
  // backing store, allocate one.
  static Handle<ObjectHashTable> GetOrCreateHiddenPropertiesHashtable(
      Handle<JSObject> object);

  // Set the hidden property backing store to either a hash table or
  // the inline-stored identity hash.
  static Handle<Object> SetHiddenPropertiesHashTable(
      Handle<JSObject> object,
      Handle<Object> value);

  MUST_USE_RESULT Object* GetIdentityHash();

  static Handle<Object> GetOrCreateIdentityHash(Handle<JSObject> object);

  DISALLOW_IMPLICIT_CONSTRUCTORS(JSObject);
};


// Common superclass for FixedArrays that allow implementations to share
// common accessors and some code paths.
class FixedArrayBase: public HeapObject {
 public:
  // [length]: length of the array.
  inline int length();
  inline void set_length(int value);

  inline static FixedArrayBase* cast(Object* object);

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
  inline Object* get(int index);
  // Setter that uses write barrier.
  inline void set(int index, Object* value);
  inline bool is_the_hole(int index);

  // Setter that doesn't need write barrier.
  inline void set(int index, Smi* value);
  // Setter with explicit barrier mode.
  inline void set(int index, Object* value, WriteBarrierMode mode);

  // Setters for frequently used oddballs located in old space.
  inline void set_undefined(int index);
  inline void set_null(int index);
  inline void set_the_hole(int index);

  inline Object** GetFirstElementAddress();
  inline bool ContainsOnlySmisOrHoles();

  // Gives access to raw memory which stores the array's data.
  inline Object** data_start();

  // Shrink length and insert filler objects.
  void Shrink(int length);

  // Copy operations.
  MUST_USE_RESULT inline MaybeObject* Copy();
  MUST_USE_RESULT MaybeObject* CopySize(int new_length,
                                        PretenureFlag pretenure = NOT_TENURED);

  // Add the elements of a JSArray to this FixedArray.
  MUST_USE_RESULT MaybeObject* AddKeysFromJSArray(JSArray* array);

  // Compute the union of this and other.
  MUST_USE_RESULT MaybeObject* UnionOfKeys(FixedArray* other);

  // Copy a sub array from the receiver to dest.
  void CopyTo(int pos, FixedArray* dest, int dest_pos, int len);

  // Garbage collection support.
  static int SizeFor(int length) { return kHeaderSize + length * kPointerSize; }

  // Code Generation support.
  static int OffsetOfElementAt(int index) { return SizeFor(index); }

  // Garbage collection support.
  Object** RawFieldOfElementAt(int index) {
    return HeapObject::RawField(this, OffsetOfElementAt(index));
  }

  // Casting.
  static inline FixedArray* cast(Object* obj);

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

  // Swap two elements in a pair of arrays.  If this array and the
  // numbers array are the same object, the elements are only swapped
  // once.
  void SwapPairs(FixedArray* numbers, int i, int j);

  // Sort prefix of this array and the numbers array as pairs wrt. the
  // numbers.  If the numbers array and the this array are the same
  // object, the prefix of this array is sorted.
  void SortPairs(FixedArray* numbers, uint32_t len);

  class BodyDescriptor : public FlexibleBodyDescriptor<kHeaderSize> {
   public:
    static inline int SizeOf(Map* map, HeapObject* object) {
      return SizeFor(reinterpret_cast<FixedArray*>(object)->length());
    }
  };

 protected:
  // Set operation on FixedArray without using write barriers. Can
  // only be used for storing old space objects or smis.
  static inline void NoWriteBarrierSet(FixedArray* array,
                                       int index,
                                       Object* value);

  // Set operation on FixedArray without incremental write barrier. Can
  // only be used if the object is guaranteed to be white (whiteness witness
  // is present).
  static inline void NoIncrementalWriteBarrierSet(FixedArray* array,
                                                  int index,
                                                  Object* value);

 private:
  STATIC_CHECK(kHeaderSize == Internals::kFixedArrayHeaderSize);

  DISALLOW_IMPLICIT_CONSTRUCTORS(FixedArray);
};


// FixedDoubleArray describes fixed-sized arrays with element type double.
class FixedDoubleArray: public FixedArrayBase {
 public:
  // Setter and getter for elements.
  inline double get_scalar(int index);
  inline int64_t get_representation(int index);
  MUST_USE_RESULT inline MaybeObject* get(int index);
  inline void set(int index, double value);
  inline void set_the_hole(int index);

  // Checking for the hole.
  inline bool is_the_hole(int index);

  // Copy operations
  MUST_USE_RESULT inline MaybeObject* Copy();

  // Garbage collection support.
  inline static int SizeFor(int length) {
    return kHeaderSize + length * kDoubleSize;
  }

  // Gives access to raw memory which stores the array's data.
  inline double* data_start();

  // Code Generation support.
  static int OffsetOfElementAt(int index) { return SizeFor(index); }

  inline static bool is_the_hole_nan(double value);
  inline static double hole_nan_as_double();
  inline static double canonical_not_the_hole_nan_as_double();

  // Casting.
  static inline FixedDoubleArray* cast(Object* obj);

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


// ConstantPoolArray describes a fixed-sized array containing constant pool
// entires.
// The format of the pool is:
//   [0]: Field holding the first index which is a pointer entry
//   [1]: Field holding the first index which is a int32 entry
//   [2] ... [first_ptr_index() - 1]: 64 bit entries
//   [first_ptr_index()] ... [first_int32_index() - 1]: pointer entries
//   [first_int32_index()] ... [length - 1]: 32 bit entries
class ConstantPoolArray: public FixedArrayBase {
 public:
  // Getters for the field storing the first index for different type entries.
  inline int first_ptr_index();
  inline int first_int64_index();
  inline int first_int32_index();

  // Getters for counts of different type entries.
  inline int count_of_ptr_entries();
  inline int count_of_int64_entries();
  inline int count_of_int32_entries();

  // Setter and getter for pool elements.
  inline Object* get_ptr_entry(int index);
  inline int64_t get_int64_entry(int index);
  inline int32_t get_int32_entry(int index);
  inline double get_int64_entry_as_double(int index);

  inline void set(int index, Object* value);
  inline void set(int index, int64_t value);
  inline void set(int index, double value);
  inline void set(int index, int32_t value);

  // Set up initial state.
  inline void SetEntryCounts(int number_of_int64_entries,
                             int number_of_ptr_entries,
                             int number_of_int32_entries);

  // Copy operations
  MUST_USE_RESULT inline MaybeObject* Copy();

  // Garbage collection support.
  inline static int SizeFor(int number_of_int64_entries,
                            int number_of_ptr_entries,
                            int number_of_int32_entries) {
    return RoundUp(OffsetAt(number_of_int64_entries,
                            number_of_ptr_entries,
                            number_of_int32_entries),
                   kPointerSize);
  }

  // Code Generation support.
  inline int OffsetOfElementAt(int index) {
    ASSERT(index < length());
    if (index >= first_int32_index()) {
      return OffsetAt(count_of_int64_entries(), count_of_ptr_entries(),
                      index - first_int32_index());
    } else if (index >= first_ptr_index()) {
      return OffsetAt(count_of_int64_entries(), index - first_ptr_index(), 0);
    } else {
      return OffsetAt(index, 0, 0);
    }
  }

  // Casting.
  static inline ConstantPoolArray* cast(Object* obj);

  // Layout description.
  static const int kFirstPointerIndexOffset = FixedArray::kHeaderSize;
  static const int kFirstInt32IndexOffset =
      kFirstPointerIndexOffset + kPointerSize;
  static const int kFirstOffset = kFirstInt32IndexOffset + kPointerSize;

  // Dispatched behavior.
  void ConstantPoolIterateBody(ObjectVisitor* v);

  DECLARE_PRINTER(ConstantPoolArray)
  DECLARE_VERIFIER(ConstantPoolArray)

 private:
  inline void set_first_ptr_index(int value);
  inline void set_first_int32_index(int value);

  inline static int OffsetAt(int number_of_int64_entries,
                             int number_of_ptr_entries,
                             int number_of_int32_entries) {
    return kFirstOffset
        + (number_of_int64_entries * kInt64Size)
        + (number_of_ptr_entries * kPointerSize)
        + (number_of_int32_entries * kInt32Size);
  }

  DISALLOW_IMPLICIT_CONSTRUCTORS(ConstantPoolArray);
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
  // WhitenessWitness is used to prove that a descriptor array is white
  // (unmarked), so incremental write barriers can be skipped because the
  // marking invariant cannot be broken and slots pointing into evacuation
  // candidates will be discovered when the object is scanned. A witness is
  // always stack-allocated right after creating an array. By allocating a
  // witness, incremental marking is globally disabled. The witness is then
  // passed along wherever needed to statically prove that the array is known to
  // be white.
  class WhitenessWitness {
   public:
    inline explicit WhitenessWitness(FixedArray* array);
    inline ~WhitenessWitness();

   private:
    IncrementalMarking* marking_;
  };

  // Returns true for both shared empty_descriptor_array and for smis, which the
  // map uses to encode additional bit fields when the descriptor array is not
  // yet used.
  inline bool IsEmpty();

  // Returns the number of descriptors in the array.
  int number_of_descriptors() {
    ASSERT(length() >= kFirstIndex || IsEmpty());
    int len = length();
    return len == 0 ? 0 : Smi::cast(get(kDescriptorLengthIndex))->value();
  }

  int number_of_descriptors_storage() {
    int len = length();
    return len == 0 ? 0 : (len - kFirstIndex) / kDescriptorSize;
  }

  int NumberOfSlackDescriptors() {
    return number_of_descriptors_storage() - number_of_descriptors();
  }

  inline void SetNumberOfDescriptors(int number_of_descriptors);
  inline int number_of_entries() { return number_of_descriptors(); }

  bool HasEnumCache() {
    return !IsEmpty() && !get(kEnumCacheIndex)->IsSmi();
  }

  void CopyEnumCacheFrom(DescriptorArray* array) {
    set(kEnumCacheIndex, array->get(kEnumCacheIndex));
  }

  FixedArray* GetEnumCache() {
    ASSERT(HasEnumCache());
    FixedArray* bridge = FixedArray::cast(get(kEnumCacheIndex));
    return FixedArray::cast(bridge->get(kEnumCacheBridgeCacheIndex));
  }

  bool HasEnumIndicesCache() {
    if (IsEmpty()) return false;
    Object* object = get(kEnumCacheIndex);
    if (object->IsSmi()) return false;
    FixedArray* bridge = FixedArray::cast(object);
    return !bridge->get(kEnumCacheBridgeIndicesCacheIndex)->IsSmi();
  }

  FixedArray* GetEnumIndicesCache() {
    ASSERT(HasEnumIndicesCache());
    FixedArray* bridge = FixedArray::cast(get(kEnumCacheIndex));
    return FixedArray::cast(bridge->get(kEnumCacheBridgeIndicesCacheIndex));
  }

  Object** GetEnumCacheSlot() {
    ASSERT(HasEnumCache());
    return HeapObject::RawField(reinterpret_cast<HeapObject*>(this),
                                kEnumCacheOffset);
  }

  void ClearEnumCache();

  // Initialize or change the enum cache,
  // using the supplied storage for the small "bridge".
  void SetEnumCache(FixedArray* bridge_storage,
                    FixedArray* new_cache,
                    Object* new_index_cache);

  // Accessors for fetching instance descriptor at descriptor number.
  inline Name* GetKey(int descriptor_number);
  inline Object** GetKeySlot(int descriptor_number);
  inline Object* GetValue(int descriptor_number);
  inline Object** GetValueSlot(int descriptor_number);
  inline Object** GetDescriptorStartSlot(int descriptor_number);
  inline Object** GetDescriptorEndSlot(int descriptor_number);
  inline PropertyDetails GetDetails(int descriptor_number);
  inline PropertyType GetType(int descriptor_number);
  inline int GetFieldIndex(int descriptor_number);
  inline Object* GetConstant(int descriptor_number);
  inline Object* GetCallbacksObject(int descriptor_number);
  inline AccessorDescriptor* GetCallbacks(int descriptor_number);

  inline Name* GetSortedKey(int descriptor_number);
  inline int GetSortedKeyIndex(int descriptor_number);
  inline void SetSortedKey(int pointer, int descriptor_number);
  inline void InitializeRepresentations(Representation representation);
  inline void SetRepresentation(int descriptor_number,
                                Representation representation);

  // Accessor for complete descriptor.
  inline void Get(int descriptor_number, Descriptor* desc);
  inline void Set(int descriptor_number,
                  Descriptor* desc,
                  const WhitenessWitness&);
  inline void Set(int descriptor_number, Descriptor* desc);

  // Append automatically sets the enumeration index. This should only be used
  // to add descriptors in bulk at the end, followed by sorting the descriptor
  // array.
  inline void Append(Descriptor* desc, const WhitenessWitness&);
  inline void Append(Descriptor* desc);

  // Transfer a complete descriptor from the src descriptor array to this
  // descriptor array.
  void CopyFrom(int dst_index,
                DescriptorArray* src,
                int src_index,
                const WhitenessWitness&);
  static Handle<DescriptorArray> Merge(Handle<DescriptorArray> desc,
                                       int verbatim,
                                       int valid,
                                       int new_size,
                                       int modify_index,
                                       StoreMode store_mode,
                                       Handle<DescriptorArray> other);
  MUST_USE_RESULT MaybeObject* Merge(int verbatim,
                                     int valid,
                                     int new_size,
                                     int modify_index,
                                     StoreMode store_mode,
                                     DescriptorArray* other);

  bool IsMoreGeneralThan(int verbatim,
                         int valid,
                         int new_size,
                         DescriptorArray* other);

  MUST_USE_RESULT MaybeObject* CopyUpTo(int enumeration_index) {
    return CopyUpToAddAttributes(enumeration_index, NONE);
  }

  static Handle<DescriptorArray> CopyUpToAddAttributes(
      Handle<DescriptorArray> desc,
      int enumeration_index,
      PropertyAttributes attributes);
  MUST_USE_RESULT MaybeObject* CopyUpToAddAttributes(
      int enumeration_index,
      PropertyAttributes attributes);

  // Sort the instance descriptors by the hash codes of their keys.
  void Sort();

  // Search the instance descriptors for given name.
  INLINE(int Search(Name* name, int number_of_own_descriptors));

  // As the above, but uses DescriptorLookupCache and updates it when
  // necessary.
  INLINE(int SearchWithCache(Name* name, Map* map));

  // Allocates a DescriptorArray, but returns the singleton
  // empty descriptor array object if number_of_descriptors is 0.
  MUST_USE_RESULT static MaybeObject* Allocate(Isolate* isolate,
                                               int number_of_descriptors,
                                               int slack = 0);

  // Casting.
  static inline DescriptorArray* cast(Object* obj);

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

#ifdef OBJECT_PRINT
  // Print all the descriptors.
  void PrintDescriptors(FILE* out = stdout);
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

 private:
  // An entry in a DescriptorArray, represented as an (array, index) pair.
  class Entry {
   public:
    inline explicit Entry(DescriptorArray* descs, int index) :
        descs_(descs), index_(index) { }

    inline PropertyType type() { return descs_->GetType(index_); }
    inline Object* GetCallbackObject() { return descs_->GetValue(index_); }

   private:
    DescriptorArray* descs_;
    int index_;
  };

  // Conversion from descriptor number to array indices.
  static int ToKeyIndex(int descriptor_number) {
    return kFirstIndex +
           (descriptor_number * kDescriptorSize) +
           kDescriptorKey;
  }

  static int ToDetailsIndex(int descriptor_number) {
    return kFirstIndex +
           (descriptor_number * kDescriptorSize) +
           kDescriptorDetails;
  }

  static int ToValueIndex(int descriptor_number) {
    return kFirstIndex +
           (descriptor_number * kDescriptorSize) +
           kDescriptorValue;
  }

  // Swap first and second descriptor.
  inline void SwapSortedKeys(int first, int second);

  DISALLOW_IMPLICIT_CONSTRUCTORS(DescriptorArray);
};


enum SearchMode { ALL_ENTRIES, VALID_ENTRIES };

template<SearchMode search_mode, typename T>
inline int LinearSearch(T* array, Name* name, int len, int valid_entries);


template<SearchMode search_mode, typename T>
inline int Search(T* array, Name* name, int valid_entries = 0);


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
//     static inline Object* AsObject(Heap* heap, Key key);
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
    ASSERT(UsesSeed);
    return Hash(key);
  }
  static uint32_t HashForObject(Key key, Object* object) { return 0; }
  static uint32_t SeededHashForObject(Key key, uint32_t seed, Object* object) {
    ASSERT(UsesSeed);
    return HashForObject(key, object);
  }
};

template<typename Shape, typename Key>
class HashTable: public FixedArray {
 public:
  // Wrapper methods
  inline uint32_t Hash(Key key) {
    if (Shape::UsesSeed) {
      return Shape::SeededHash(key,
          GetHeap()->HashSeed());
    } else {
      return Shape::Hash(key);
    }
  }

  inline uint32_t HashForObject(Key key, Object* object) {
    if (Shape::UsesSeed) {
      return Shape::SeededHashForObject(key,
          GetHeap()->HashSeed(), object);
    } else {
      return Shape::HashForObject(key, object);
    }
  }

  // Returns the number of elements in the hash table.
  int NumberOfElements() {
    return Smi::cast(get(kNumberOfElementsIndex))->value();
  }

  // Returns the number of deleted elements in the hash table.
  int NumberOfDeletedElements() {
    return Smi::cast(get(kNumberOfDeletedElementsIndex))->value();
  }

  // Returns the capacity of the hash table.
  int Capacity() {
    return Smi::cast(get(kCapacityIndex))->value();
  }

  // ElementAdded should be called whenever an element is added to a
  // hash table.
  void ElementAdded() { SetNumberOfElements(NumberOfElements() + 1); }

  // ElementRemoved should be called whenever an element is removed from
  // a hash table.
  void ElementRemoved() {
    SetNumberOfElements(NumberOfElements() - 1);
    SetNumberOfDeletedElements(NumberOfDeletedElements() + 1);
  }
  void ElementsRemoved(int n) {
    SetNumberOfElements(NumberOfElements() - n);
    SetNumberOfDeletedElements(NumberOfDeletedElements() + n);
  }

  // Returns a new HashTable object. Might return Failure.
  MUST_USE_RESULT static MaybeObject* Allocate(
      Heap* heap,
      int at_least_space_for,
      MinimumCapacity capacity_option = USE_DEFAULT_MINIMUM_CAPACITY,
      PretenureFlag pretenure = NOT_TENURED);

  // Computes the required capacity for a table holding the given
  // number of elements. May be more than HashTable::kMaxCapacity.
  static int ComputeCapacity(int at_least_space_for);

  // Returns the key at entry.
  Object* KeyAt(int entry) { return get(EntryToIndex(entry)); }

  // Tells whether k is a real key.  The hole and undefined are not allowed
  // as keys and can be used to indicate missing or deleted elements.
  bool IsKey(Object* k) {
    return !k->IsTheHole() && !k->IsUndefined();
  }

  // Garbage collection support.
  void IteratePrefix(ObjectVisitor* visitor);
  void IterateElements(ObjectVisitor* visitor);

  // Casting.
  static inline HashTable* cast(Object* obj);

  // Compute the probe offset (quadratic probing).
  INLINE(static uint32_t GetProbeOffset(uint32_t n)) {
    return (n + n * n) >> 1;
  }

  static const int kNumberOfElementsIndex = 0;
  static const int kNumberOfDeletedElementsIndex = 1;
  static const int kCapacityIndex = 2;
  static const int kPrefixStartIndex = 3;
  static const int kElementsStartIndex =
      kPrefixStartIndex + Shape::kPrefixSize;
  static const int kEntrySize = Shape::kEntrySize;
  static const int kElementsStartOffset =
      kHeaderSize + kElementsStartIndex * kPointerSize;
  static const int kCapacityOffset =
      kHeaderSize + kCapacityIndex * kPointerSize;

  // Constant used for denoting a absent entry.
  static const int kNotFound = -1;

  // Maximal capacity of HashTable. Based on maximal length of underlying
  // FixedArray. Staying below kMaxCapacity also ensures that EntryToIndex
  // cannot overflow.
  static const int kMaxCapacity =
      (FixedArray::kMaxLength - kElementsStartOffset) / kEntrySize;

  // Find entry for key otherwise return kNotFound.
  inline int FindEntry(Key key);
  int FindEntry(Isolate* isolate, Key key);

  // Rehashes the table in-place.
  void Rehash(Key key);

 protected:
  friend class ObjectHashSet;
  friend class ObjectHashTable;

  // Find the entry at which to insert element with the given key that
  // has the given hash value.
  uint32_t FindInsertionEntry(uint32_t hash);

  // Returns the index for an entry (of the key)
  static inline int EntryToIndex(int entry) {
    return (entry * kEntrySize) + kElementsStartIndex;
  }

  // Update the number of elements in the hash table.
  void SetNumberOfElements(int nof) {
    set(kNumberOfElementsIndex, Smi::FromInt(nof));
  }

  // Update the number of deleted elements in the hash table.
  void SetNumberOfDeletedElements(int nod) {
    set(kNumberOfDeletedElementsIndex, Smi::FromInt(nod));
  }

  // Sets the capacity of the hash table.
  void SetCapacity(int capacity) {
    // To scale a computed hash code to fit within the hash table, we
    // use bit-wise AND with a mask, so the capacity must be positive
    // and non-zero.
    ASSERT(capacity > 0);
    ASSERT(capacity <= kMaxCapacity);
    set(kCapacityIndex, Smi::FromInt(capacity));
  }


  // Returns probe entry.
  static uint32_t GetProbe(uint32_t hash, uint32_t number, uint32_t size) {
    ASSERT(IsPowerOf2(size));
    return (hash + GetProbeOffset(number)) & (size - 1);
  }

  inline static uint32_t FirstProbe(uint32_t hash, uint32_t size) {
    return hash & (size - 1);
  }

  inline static uint32_t NextProbe(
      uint32_t last, uint32_t number, uint32_t size) {
    return (last + number) & (size - 1);
  }

  // Returns _expected_ if one of entries given by the first _probe_ probes is
  // equal to  _expected_. Otherwise, returns the entry given by the probe
  // number _probe_.
  uint32_t EntryForProbe(Key key, Object* k, int probe, uint32_t expected);

  void Swap(uint32_t entry1, uint32_t entry2, WriteBarrierMode mode);

  // Rehashes this hash-table into the new table.
  MUST_USE_RESULT MaybeObject* Rehash(HashTable* new_table, Key key);

  // Attempt to shrink hash table after removal of key.
  MUST_USE_RESULT MaybeObject* Shrink(Key key);

  // Ensure enough space for n additional elements.
  MUST_USE_RESULT MaybeObject* EnsureCapacity(
      int n,
      Key key,
      PretenureFlag pretenure = NOT_TENURED);
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
  // If allocations fails a failure object is returned.
  MUST_USE_RESULT virtual MaybeObject* AsObject(Heap* heap) = 0;
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
  MUST_USE_RESULT static inline MaybeObject* AsObject(Heap* heap,
                                                      HashTableKey* key) {
    return key->AsObject(heap);
  }

  static const int kPrefixSize = 0;
  static const int kEntrySize = 1;
};

class SeqOneByteString;

// StringTable.
//
// No special elements in the prefix and the element size is 1
// because only the string itself (the key) needs to be stored.
class StringTable: public HashTable<StringTableShape, HashTableKey*> {
 public:
  // Find string in the string table.  If it is not there yet, it is
  // added.  The return value is the string table which might have
  // been enlarged.  If the return value is not a failure, the string
  // pointer *s is set to the string found.
  MUST_USE_RESULT MaybeObject* LookupString(String* key, Object** s);
  MUST_USE_RESULT MaybeObject* LookupKey(HashTableKey* key, Object** s);

  // Looks up a string that is equal to the given string and returns
  // true if it is found, assigning the string to the given output
  // parameter.
  bool LookupStringIfExists(String* str, String** result);
  bool LookupTwoCharsStringIfExists(uint16_t c1, uint16_t c2, String** result);

  // Casting.
  static inline StringTable* cast(Object* obj);

 private:
  template <bool seq_ascii> friend class JsonParser;

  DISALLOW_IMPLICIT_CONSTRUCTORS(StringTable);
};


class MapCacheShape : public BaseShape<HashTableKey*> {
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

  MUST_USE_RESULT static inline MaybeObject* AsObject(Heap* heap,
                                                      HashTableKey* key) {
    return key->AsObject(heap);
  }

  static const int kPrefixSize = 0;
  static const int kEntrySize = 2;
};


// MapCache.
//
// Maps keys that are a fixed array of unique names to a map.
// Used for canonicalize maps for object literals.
class MapCache: public HashTable<MapCacheShape, HashTableKey*> {
 public:
  // Find cached value for a name key, otherwise return null.
  Object* Lookup(FixedArray* key);
  MUST_USE_RESULT MaybeObject* Put(FixedArray* key, Map* value);
  static inline MapCache* cast(Object* obj);

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(MapCache);
};


template <typename Shape, typename Key>
class Dictionary: public HashTable<Shape, Key> {
 public:
  static inline Dictionary<Shape, Key>* cast(Object* obj) {
    return reinterpret_cast<Dictionary<Shape, Key>*>(obj);
  }

  // Returns the value at entry.
  Object* ValueAt(int entry) {
    return this->get(HashTable<Shape, Key>::EntryToIndex(entry) + 1);
  }

  // Set the value for entry.
  void ValueAtPut(int entry, Object* value) {
    this->set(HashTable<Shape, Key>::EntryToIndex(entry) + 1, value);
  }

  // Returns the property details for the property at entry.
  PropertyDetails DetailsAt(int entry) {
    ASSERT(entry >= 0);  // Not found is -1, which is not caught by get().
    return PropertyDetails(
        Smi::cast(this->get(HashTable<Shape, Key>::EntryToIndex(entry) + 2)));
  }

  // Set the details for entry.
  void DetailsAtPut(int entry, PropertyDetails value) {
    this->set(HashTable<Shape, Key>::EntryToIndex(entry) + 2, value.AsSmi());
  }

  // Sorting support
  void CopyValuesTo(FixedArray* elements);

  // Delete a property from the dictionary.
  Object* DeleteProperty(int entry, JSObject::DeleteMode mode);

  // Attempt to shrink the dictionary after deletion of key.
  MUST_USE_RESULT MaybeObject* Shrink(Key key);

  // Returns the number of elements in the dictionary filtering out properties
  // with the specified attributes.
  int NumberOfElementsFilterAttributes(PropertyAttributes filter);

  // Returns the number of enumerable elements in the dictionary.
  int NumberOfEnumElements();

  enum SortMode { UNSORTED, SORTED };
  // Copies keys to preallocated fixed array.
  void CopyKeysTo(FixedArray* storage,
                  PropertyAttributes filter,
                  SortMode sort_mode);
  // Fill in details for properties into storage.
  void CopyKeysTo(FixedArray* storage,
                  int index,
                  PropertyAttributes filter,
                  SortMode sort_mode);

  // Accessors for next enumeration index.
  void SetNextEnumerationIndex(int index) {
    ASSERT(index != 0);
    this->set(kNextEnumerationIndexIndex, Smi::FromInt(index));
  }

  int NextEnumerationIndex() {
    return Smi::cast(FixedArray::get(kNextEnumerationIndexIndex))->value();
  }

  // Returns a new array for dictionary usage. Might return Failure.
  MUST_USE_RESULT static MaybeObject* Allocate(
      Heap* heap,
      int at_least_space_for,
      PretenureFlag pretenure = NOT_TENURED);

  // Ensure enough space for n additional elements.
  MUST_USE_RESULT MaybeObject* EnsureCapacity(int n, Key key);

#ifdef OBJECT_PRINT
  void Print(FILE* out = stdout);
#endif
  // Returns the key (slow).
  Object* SlowReverseLookup(Object* value);

  // Sets the entry to (key, value) pair.
  inline void SetEntry(int entry,
                       Object* key,
                       Object* value);
  inline void SetEntry(int entry,
                       Object* key,
                       Object* value,
                       PropertyDetails details);

  MUST_USE_RESULT MaybeObject* Add(Key key,
                                   Object* value,
                                   PropertyDetails details);

 protected:
  // Generic at put operation.
  MUST_USE_RESULT MaybeObject* AtPut(Key key, Object* value);

  // Add entry to dictionary.
  MUST_USE_RESULT MaybeObject* AddEntry(Key key,
                                        Object* value,
                                        PropertyDetails details,
                                        uint32_t hash);

  // Generate new enumeration indices to avoid enumeration index overflow.
  MUST_USE_RESULT MaybeObject* GenerateNewEnumerationIndices();
  static const int kMaxNumberKeyIndex =
      HashTable<Shape, Key>::kPrefixStartIndex;
  static const int kNextEnumerationIndexIndex = kMaxNumberKeyIndex + 1;
};


class NameDictionaryShape : public BaseShape<Name*> {
 public:
  static inline bool IsMatch(Name* key, Object* other);
  static inline uint32_t Hash(Name* key);
  static inline uint32_t HashForObject(Name* key, Object* object);
  MUST_USE_RESULT static inline MaybeObject* AsObject(Heap* heap,
                                                      Name* key);
  static const int kPrefixSize = 2;
  static const int kEntrySize = 3;
  static const bool kIsEnumerable = true;
};


class NameDictionary: public Dictionary<NameDictionaryShape, Name*> {
 public:
  static inline NameDictionary* cast(Object* obj) {
    ASSERT(obj->IsDictionary());
    return reinterpret_cast<NameDictionary*>(obj);
  }

  // Copies enumerable keys to preallocated fixed array.
  FixedArray* CopyEnumKeysTo(FixedArray* storage);
  static void DoGenerateNewEnumerationIndices(
      Handle<NameDictionary> dictionary);

  // For transforming properties of a JSObject.
  MUST_USE_RESULT MaybeObject* TransformPropertiesToFastFor(
      JSObject* obj,
      int unused_property_fields);

  // Find entry for key, otherwise return kNotFound. Optimized version of
  // HashTable::FindEntry.
  int FindEntry(Name* key);
};


class NumberDictionaryShape : public BaseShape<uint32_t> {
 public:
  static inline bool IsMatch(uint32_t key, Object* other);
  MUST_USE_RESULT static inline MaybeObject* AsObject(Heap* heap,
                                                      uint32_t key);
  static const int kEntrySize = 3;
  static const bool kIsEnumerable = false;
};


class SeededNumberDictionaryShape : public NumberDictionaryShape {
 public:
  static const bool UsesSeed = true;
  static const int kPrefixSize = 2;

  static inline uint32_t SeededHash(uint32_t key, uint32_t seed);
  static inline uint32_t SeededHashForObject(uint32_t key,
                                             uint32_t seed,
                                             Object* object);
};


class UnseededNumberDictionaryShape : public NumberDictionaryShape {
 public:
  static const int kPrefixSize = 0;

  static inline uint32_t Hash(uint32_t key);
  static inline uint32_t HashForObject(uint32_t key, Object* object);
};


class SeededNumberDictionary
    : public Dictionary<SeededNumberDictionaryShape, uint32_t> {
 public:
  static SeededNumberDictionary* cast(Object* obj) {
    ASSERT(obj->IsDictionary());
    return reinterpret_cast<SeededNumberDictionary*>(obj);
  }

  // Type specific at put (default NONE attributes is used when adding).
  MUST_USE_RESULT MaybeObject* AtNumberPut(uint32_t key, Object* value);
  MUST_USE_RESULT static Handle<SeededNumberDictionary> AddNumberEntry(
      Handle<SeededNumberDictionary> dictionary,
      uint32_t key,
      Handle<Object> value,
      PropertyDetails details);
  MUST_USE_RESULT MaybeObject* AddNumberEntry(uint32_t key,
                                              Object* value,
                                              PropertyDetails details);

  // Set an existing entry or add a new one if needed.
  // Return the updated dictionary.
  MUST_USE_RESULT static Handle<SeededNumberDictionary> Set(
      Handle<SeededNumberDictionary> dictionary,
      uint32_t index,
      Handle<Object> value,
      PropertyDetails details);

  MUST_USE_RESULT MaybeObject* Set(uint32_t key,
                                   Object* value,
                                   PropertyDetails details);

  void UpdateMaxNumberKey(uint32_t key);

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

  // Bit masks.
  static const int kRequiresSlowElementsMask = 1;
  static const int kRequiresSlowElementsTagSize = 1;
  static const uint32_t kRequiresSlowElementsLimit = (1 << 29) - 1;
};


class UnseededNumberDictionary
    : public Dictionary<UnseededNumberDictionaryShape, uint32_t> {
 public:
  static UnseededNumberDictionary* cast(Object* obj) {
    ASSERT(obj->IsDictionary());
    return reinterpret_cast<UnseededNumberDictionary*>(obj);
  }

  // Type specific at put (default NONE attributes is used when adding).
  MUST_USE_RESULT MaybeObject* AtNumberPut(uint32_t key, Object* value);
  MUST_USE_RESULT MaybeObject* AddNumberEntry(uint32_t key, Object* value);

  // Set an existing entry or add a new one if needed.
  // Return the updated dictionary.
  MUST_USE_RESULT static Handle<UnseededNumberDictionary> Set(
      Handle<UnseededNumberDictionary> dictionary,
      uint32_t index,
      Handle<Object> value);

  MUST_USE_RESULT MaybeObject* Set(uint32_t key, Object* value);
};


template <int entrysize>
class ObjectHashTableShape : public BaseShape<Object*> {
 public:
  static inline bool IsMatch(Object* key, Object* other);
  static inline uint32_t Hash(Object* key);
  static inline uint32_t HashForObject(Object* key, Object* object);
  MUST_USE_RESULT static inline MaybeObject* AsObject(Heap* heap,
                                                      Object* key);
  static const int kPrefixSize = 0;
  static const int kEntrySize = entrysize;
};


// ObjectHashSet holds keys that are arbitrary objects by using the identity
// hash of the key for hashing purposes.
class ObjectHashSet: public HashTable<ObjectHashTableShape<1>, Object*> {
 public:
  static inline ObjectHashSet* cast(Object* obj) {
    ASSERT(obj->IsHashTable());
    return reinterpret_cast<ObjectHashSet*>(obj);
  }

  // Looks up whether the given key is part of this hash set.
  bool Contains(Object* key);

  static Handle<ObjectHashSet> EnsureCapacity(
      Handle<ObjectHashSet> table,
      int n,
      Handle<Object> key,
      PretenureFlag pretenure = NOT_TENURED);

  // Attempt to shrink hash table after removal of key.
  static Handle<ObjectHashSet> Shrink(Handle<ObjectHashSet> table,
                                      Handle<Object> key);

  // Adds the given key to this hash set.
  static Handle<ObjectHashSet> Add(Handle<ObjectHashSet> table,
                                   Handle<Object> key);

  // Removes the given key from this hash set.
  static Handle<ObjectHashSet> Remove(Handle<ObjectHashSet> table,
                                      Handle<Object> key);
};


// ObjectHashTable maps keys that are arbitrary objects to object values by
// using the identity hash of the key for hashing purposes.
class ObjectHashTable: public HashTable<ObjectHashTableShape<2>, Object*> {
 public:
  static inline ObjectHashTable* cast(Object* obj) {
    ASSERT(obj->IsHashTable());
    return reinterpret_cast<ObjectHashTable*>(obj);
  }

  static Handle<ObjectHashTable> EnsureCapacity(
      Handle<ObjectHashTable> table,
      int n,
      Handle<Object> key,
      PretenureFlag pretenure = NOT_TENURED);

  // Attempt to shrink hash table after removal of key.
  static Handle<ObjectHashTable> Shrink(Handle<ObjectHashTable> table,
                                        Handle<Object> key);

  // Looks up the value associated with the given key. The hole value is
  // returned in case the key is not present.
  Object* Lookup(Object* key);

  // Adds (or overwrites) the value associated with the given key. Mapping a
  // key to the hole value causes removal of the whole entry.
  static Handle<ObjectHashTable> Put(Handle<ObjectHashTable> table,
                                     Handle<Object> key,
                                     Handle<Object> value);

 private:
  friend class MarkCompactCollector;

  void AddEntry(int entry, Object* key, Object* value);
  void RemoveEntry(int entry);

  // Returns the index to the value of an entry.
  static inline int EntryToValueIndex(int entry) {
    return EntryToIndex(entry) + 1;
  }
};


template <int entrysize>
class WeakHashTableShape : public BaseShape<Object*> {
 public:
  static inline bool IsMatch(Object* key, Object* other);
  static inline uint32_t Hash(Object* key);
  static inline uint32_t HashForObject(Object* key, Object* object);
  MUST_USE_RESULT static inline MaybeObject* AsObject(Heap* heap,
                                                      Object* key);
  static const int kPrefixSize = 0;
  static const int kEntrySize = entrysize;
};


// WeakHashTable maps keys that are arbitrary objects to object values.
// It is used for the global weak hash table that maps objects
// embedded in optimized code to dependent code lists.
class WeakHashTable: public HashTable<WeakHashTableShape<2>, Object*> {
 public:
  static inline WeakHashTable* cast(Object* obj) {
    ASSERT(obj->IsHashTable());
    return reinterpret_cast<WeakHashTable*>(obj);
  }

  // Looks up the value associated with the given key. The hole value is
  // returned in case the key is not present.
  Object* Lookup(Object* key);

  // Adds (or overwrites) the value associated with the given key. Mapping a
  // key to the hole value causes removal of the whole entry.
  MUST_USE_RESULT MaybeObject* Put(Object* key, Object* value);

  // This function is called when heap verification is turned on.
  void Zap(Object* value) {
    int capacity = Capacity();
    for (int i = 0; i < capacity; i++) {
      set(EntryToIndex(i), value);
      set(EntryToValueIndex(i), value);
    }
  }

 private:
  friend class MarkCompactCollector;

  void AddEntry(int entry, Object* key, Object* value);

  // Returns the index to the value of an entry.
  static inline int EntryToValueIndex(int entry) {
    return EntryToIndex(entry) + 1;
  }
};


// JSFunctionResultCache caches results of some JSFunction invocation.
// It is a fixed array with fixed structure:
//   [0]: factory function
//   [1]: finger index
//   [2]: current cache size
//   [3]: dummy field.
// The rest of array are key/value pairs.
class JSFunctionResultCache: public FixedArray {
 public:
  static const int kFactoryIndex = 0;
  static const int kFingerIndex = kFactoryIndex + 1;
  static const int kCacheSizeIndex = kFingerIndex + 1;
  static const int kDummyIndex = kCacheSizeIndex + 1;
  static const int kEntriesIndex = kDummyIndex + 1;

  static const int kEntrySize = 2;  // key + value

  static const int kFactoryOffset = kHeaderSize;
  static const int kFingerOffset = kFactoryOffset + kPointerSize;
  static const int kCacheSizeOffset = kFingerOffset + kPointerSize;

  inline void MakeZeroSize();
  inline void Clear();

  inline int size();
  inline void set_size(int size);
  inline int finger_index();
  inline void set_finger_index(int finger_index);

  // Casting
  static inline JSFunctionResultCache* cast(Object* obj);

  DECLARE_VERIFIER(JSFunctionResultCache)
};


// ScopeInfo represents information about different scopes of a source
// program  and the allocation of the scope's variables. Scope information
// is stored in a compressed form in ScopeInfo objects and is used
// at runtime (stack dumps, deoptimization, etc.).

// This object provides quick access to scope info details for runtime
// routines.
class ScopeInfo : public FixedArray {
 public:
  static inline ScopeInfo* cast(Object* object);

  // Return the type of this scope.
  ScopeType scope_type();

  // Does this scope call eval?
  bool CallsEval();

  // Return the language mode of this scope.
  LanguageMode language_mode();

  // Does this scope make a non-strict eval call?
  bool CallsNonStrictEval() {
    return CallsEval() && (language_mode() == CLASSIC_MODE);
  }

  // Return the total number of locals allocated on the stack and in the
  // context. This includes the parameters that are allocated in the context.
  int LocalCount();

  // Return the number of stack slots for code. This number consists of two
  // parts:
  //  1. One stack slot per stack allocated local.
  //  2. One stack slot for the function name if it is stack allocated.
  int StackSlotCount();

  // Return the number of context slots for code if a context is allocated. This
  // number consists of three parts:
  //  1. Size of fixed header for every context: Context::MIN_CONTEXT_SLOTS
  //  2. One context slot per context allocated local.
  //  3. One context slot for the function name if it is context allocated.
  // Parameters allocated in the context count as context allocated locals. If
  // no contexts are allocated for this scope ContextLength returns 0.
  int ContextLength();

  // Is this scope the scope of a named function expression?
  bool HasFunctionName();

  // Return if this has context allocated locals.
  bool HasHeapAllocatedLocals();

  // Return if contexts are allocated for this scope.
  bool HasContext();

  // Return the function_name if present.
  String* FunctionName();

  // Return the name of the given parameter.
  String* ParameterName(int var);

  // Return the name of the given local.
  String* LocalName(int var);

  // Return the name of the given stack local.
  String* StackLocalName(int var);

  // Return the name of the given context local.
  String* ContextLocalName(int var);

  // Return the mode of the given context local.
  VariableMode ContextLocalMode(int var);

  // Return the initialization flag of the given context local.
  InitializationFlag ContextLocalInitFlag(int var);

  // Lookup support for serialized scope info. Returns the
  // the stack slot index for a given slot name if the slot is
  // present; otherwise returns a value < 0. The name must be an internalized
  // string.
  int StackSlotIndex(String* name);

  // Lookup support for serialized scope info. Returns the
  // context slot index for a given slot name if the slot is present; otherwise
  // returns a value < 0. The name must be an internalized string.
  // If the slot is present and mode != NULL, sets *mode to the corresponding
  // mode for that variable.
  int ContextSlotIndex(String* name,
                       VariableMode* mode,
                       InitializationFlag* init_flag);

  // Lookup support for serialized scope info. Returns the
  // parameter index for a given parameter name if the parameter is present;
  // otherwise returns a value < 0. The name must be an internalized string.
  int ParameterIndex(String* name);

  // Lookup support for serialized scope info. Returns the function context
  // slot index if the function name is present and context-allocated (named
  // function expressions, only), otherwise returns a value < 0. The name
  // must be an internalized string.
  int FunctionContextSlotIndex(String* name, VariableMode* mode);


  // Copies all the context locals into an object used to materialize a scope.
  static bool CopyContextLocalsToScopeObject(Handle<ScopeInfo> scope_info,
                                             Handle<Context> context,
                                             Handle<JSObject> scope_object);


  static Handle<ScopeInfo> Create(Scope* scope, Zone* zone);

  // Serializes empty scope info.
  static ScopeInfo* Empty(Isolate* isolate);

#ifdef DEBUG
  void Print();
#endif

  // The layout of the static part of a ScopeInfo is as follows. Each entry is
  // numeric and occupies one array slot.
  // 1. A set of properties of the scope
  // 2. The number of parameters. This only applies to function scopes. For
  //    non-function scopes this is 0.
  // 3. The number of non-parameter variables allocated on the stack.
  // 4. The number of non-parameter and parameter variables allocated in the
  //    context.
#define FOR_EACH_NUMERIC_FIELD(V)          \
  V(Flags)                                 \
  V(ParameterCount)                        \
  V(StackLocalCount)                       \
  V(ContextLocalCount)

#define FIELD_ACCESSORS(name)                            \
  void Set##name(int value) {                            \
    set(k##name, Smi::FromInt(value));                   \
  }                                                      \
  int name() {                                           \
    if (length() > 0) {                                  \
      return Smi::cast(get(k##name))->value();           \
    } else {                                             \
      return 0;                                          \
    }                                                    \
  }
  FOR_EACH_NUMERIC_FIELD(FIELD_ACCESSORS)
#undef FIELD_ACCESSORS

 private:
  enum {
#define DECL_INDEX(name) k##name,
  FOR_EACH_NUMERIC_FIELD(DECL_INDEX)
#undef DECL_INDEX
#undef FOR_EACH_NUMERIC_FIELD
    kVariablePartIndex
  };

  // The layout of the variable part of a ScopeInfo is as follows:
  // 1. ParameterEntries:
  //    This part stores the names of the parameters for function scopes. One
  //    slot is used per parameter, so in total this part occupies
  //    ParameterCount() slots in the array. For other scopes than function
  //    scopes ParameterCount() is 0.
  // 2. StackLocalEntries:
  //    Contains the names of local variables that are allocated on the stack,
  //    in increasing order of the stack slot index. One slot is used per stack
  //    local, so in total this part occupies StackLocalCount() slots in the
  //    array.
  // 3. ContextLocalNameEntries:
  //    Contains the names of local variables and parameters that are allocated
  //    in the context. They are stored in increasing order of the context slot
  //    index starting with Context::MIN_CONTEXT_SLOTS. One slot is used per
  //    context local, so in total this part occupies ContextLocalCount() slots
  //    in the array.
  // 4. ContextLocalInfoEntries:
  //    Contains the variable modes and initialization flags corresponding to
  //    the context locals in ContextLocalNameEntries. One slot is used per
  //    context local, so in total this part occupies ContextLocalCount()
  //    slots in the array.
  // 5. FunctionNameEntryIndex:
  //    If the scope belongs to a named function expression this part contains
  //    information about the function variable. It always occupies two array
  //    slots:  a. The name of the function variable.
  //            b. The context or stack slot index for the variable.
  int ParameterEntriesIndex();
  int StackLocalEntriesIndex();
  int ContextLocalNameEntriesIndex();
  int ContextLocalInfoEntriesIndex();
  int FunctionNameEntryIndex();

  // Location of the function variable for named function expressions.
  enum FunctionVariableInfo {
    NONE,     // No function name present.
    STACK,    // Function
    CONTEXT,
    UNUSED
  };

  // Properties of scopes.
  class ScopeTypeField:        public BitField<ScopeType,            0, 3> {};
  class CallsEvalField:        public BitField<bool,                 3, 1> {};
  class LanguageModeField:     public BitField<LanguageMode,         4, 2> {};
  class FunctionVariableField: public BitField<FunctionVariableInfo, 6, 2> {};
  class FunctionVariableMode:  public BitField<VariableMode,         8, 3> {};

  // BitFields representing the encoded information for context locals in the
  // ContextLocalInfoEntries part.
  class ContextLocalMode:      public BitField<VariableMode,         0, 3> {};
  class ContextLocalInitFlag:  public BitField<InitializationFlag,   3, 1> {};
};


// The cache for maps used by normalized (dictionary mode) objects.
// Such maps do not have property descriptors, so a typical program
// needs very limited number of distinct normalized maps.
class NormalizedMapCache: public FixedArray {
 public:
  static const int kEntries = 64;

  static Handle<Map> Get(Handle<NormalizedMapCache> cache,
                         Handle<JSObject> object,
                         PropertyNormalizationMode mode);

  void Clear();

  // Casting
  static inline NormalizedMapCache* cast(Object* obj);

  DECLARE_VERIFIER(NormalizedMapCache)
};


// ByteArray represents fixed sized byte arrays.  Used for the relocation info
// that is attached to code objects.
class ByteArray: public FixedArrayBase {
 public:
  inline int Size() { return RoundUp(length() + kHeaderSize, kPointerSize); }

  // Setter and getter.
  inline byte get(int index);
  inline void set(int index, byte value);

  // Treat contents as an int array.
  inline int get_int(int index);

  static int SizeFor(int length) {
    return OBJECT_POINTER_ALIGN(kHeaderSize + length);
  }
  // We use byte arrays for free blocks in the heap.  Given a desired size in
  // bytes that is a multiple of the word size and big enough to hold a byte
  // array, this function returns the number of elements a byte array should
  // have.
  static int LengthFor(int size_in_bytes) {
    ASSERT(IsAligned(size_in_bytes, kPointerSize));
    ASSERT(size_in_bytes >= kHeaderSize);
    return size_in_bytes - kHeaderSize;
  }

  // Returns data start address.
  inline Address GetDataStartAddress();

  // Returns a pointer to the ByteArray object for a given data start address.
  static inline ByteArray* FromDataStartAddress(Address address);

  // Casting.
  static inline ByteArray* cast(Object* obj);

  // Dispatched behavior.
  inline int ByteArraySize() {
    return SizeFor(this->length());
  }
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


// FreeSpace represents fixed sized areas of the heap that are not currently in
// use.  Used by the heap and GC.
class FreeSpace: public HeapObject {
 public:
  // [size]: size of the free space including the header.
  inline int size();
  inline void set_size(int value);

  inline int Size() { return size(); }

  // Casting.
  static inline FreeSpace* cast(Object* obj);

  // Dispatched behavior.
  DECLARE_PRINTER(FreeSpace)
  DECLARE_VERIFIER(FreeSpace)

  // Layout description.
  // Size is smi tagged when it is stored.
  static const int kSizeOffset = HeapObject::kHeaderSize;
  static const int kHeaderSize = kSizeOffset + kPointerSize;

  static const int kAlignedSize = OBJECT_POINTER_ALIGN(kHeaderSize);

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



// An ExternalArray represents a fixed-size array of primitive values
// which live outside the JavaScript heap. Its subclasses are used to
// implement the CanvasArray types being defined in the WebGL
// specification. As of this writing the first public draft is not yet
// available, but Khronos members can access the draft at:
//   https://cvs.khronos.org/svn/repos/3dweb/trunk/doc/spec/WebGL-spec.html
//
// The semantics of these arrays differ from CanvasPixelArray.
// Out-of-range values passed to the setter are converted via a C
// cast, not clamping. Out-of-range indices cause exceptions to be
// raised rather than being silently ignored.
class ExternalArray: public FixedArrayBase {
 public:
  inline bool is_the_hole(int index) { return false; }

  // [external_pointer]: The pointer to the external memory area backing this
  // external array.
  DECL_ACCESSORS(external_pointer, void)  // Pointer to the data store.

  // Casting.
  static inline ExternalArray* cast(Object* obj);

  // Maximal acceptable length for an external array.
  static const int kMaxLength = 0x3fffffff;

  // ExternalArray headers are not quadword aligned.
  static const int kExternalPointerOffset =
      POINTER_SIZE_ALIGN(FixedArrayBase::kLengthOffset + kPointerSize);
  static const int kHeaderSize = kExternalPointerOffset + kPointerSize;
  static const int kAlignedSize = OBJECT_POINTER_ALIGN(kHeaderSize);

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(ExternalArray);
};


// A ExternalUint8ClampedArray represents a fixed-size byte array with special
// semantics used for implementing the CanvasPixelArray object. Please see the
// specification at:

// http://www.whatwg.org/specs/web-apps/current-work/
//                      multipage/the-canvas-element.html#canvaspixelarray
// In particular, write access clamps the value written to 0 or 255 if the
// value written is outside this range.
class ExternalUint8ClampedArray: public ExternalArray {
 public:
  inline uint8_t* external_uint8_clamped_pointer();

  // Setter and getter.
  inline uint8_t get_scalar(int index);
  MUST_USE_RESULT inline MaybeObject* get(int index);
  inline void set(int index, uint8_t value);

  // This accessor applies the correct conversion from Smi, HeapNumber and
  // undefined and clamps the converted value between 0 and 255.
  Object* SetValue(uint32_t index, Object* value);

  static Handle<Object> SetValue(Handle<ExternalUint8ClampedArray> array,
                                 uint32_t index,
                                 Handle<Object> value);

  // Casting.
  static inline ExternalUint8ClampedArray* cast(Object* obj);

  // Dispatched behavior.
  DECLARE_PRINTER(ExternalUint8ClampedArray)
  DECLARE_VERIFIER(ExternalUint8ClampedArray)

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(ExternalUint8ClampedArray);
};


class ExternalInt8Array: public ExternalArray {
 public:
  // Setter and getter.
  inline int8_t get_scalar(int index);
  MUST_USE_RESULT inline MaybeObject* get(int index);
  inline void set(int index, int8_t value);

  static Handle<Object> SetValue(Handle<ExternalInt8Array> array,
                                 uint32_t index,
                                 Handle<Object> value);

  // This accessor applies the correct conversion from Smi, HeapNumber
  // and undefined.
  MUST_USE_RESULT MaybeObject* SetValue(uint32_t index, Object* value);

  // Casting.
  static inline ExternalInt8Array* cast(Object* obj);

  // Dispatched behavior.
  DECLARE_PRINTER(ExternalInt8Array)
  DECLARE_VERIFIER(ExternalInt8Array)

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(ExternalInt8Array);
};


class ExternalUint8Array: public ExternalArray {
 public:
  // Setter and getter.
  inline uint8_t get_scalar(int index);
  MUST_USE_RESULT inline MaybeObject* get(int index);
  inline void set(int index, uint8_t value);

  static Handle<Object> SetValue(Handle<ExternalUint8Array> array,
                                 uint32_t index,
                                 Handle<Object> value);

  // This accessor applies the correct conversion from Smi, HeapNumber
  // and undefined.
  MUST_USE_RESULT MaybeObject* SetValue(uint32_t index, Object* value);

  // Casting.
  static inline ExternalUint8Array* cast(Object* obj);

  // Dispatched behavior.
  DECLARE_PRINTER(ExternalUint8Array)
  DECLARE_VERIFIER(ExternalUint8Array)

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(ExternalUint8Array);
};


class ExternalInt16Array: public ExternalArray {
 public:
  // Setter and getter.
  inline int16_t get_scalar(int index);
  MUST_USE_RESULT inline MaybeObject* get(int index);
  inline void set(int index, int16_t value);

  static Handle<Object> SetValue(Handle<ExternalInt16Array> array,
                                 uint32_t index,
                                 Handle<Object> value);

  // This accessor applies the correct conversion from Smi, HeapNumber
  // and undefined.
  MUST_USE_RESULT MaybeObject* SetValue(uint32_t index, Object* value);

  // Casting.
  static inline ExternalInt16Array* cast(Object* obj);

  // Dispatched behavior.
  DECLARE_PRINTER(ExternalInt16Array)
  DECLARE_VERIFIER(ExternalInt16Array)

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(ExternalInt16Array);
};


class ExternalUint16Array: public ExternalArray {
 public:
  // Setter and getter.
  inline uint16_t get_scalar(int index);
  MUST_USE_RESULT inline MaybeObject* get(int index);
  inline void set(int index, uint16_t value);

  static Handle<Object> SetValue(Handle<ExternalUint16Array> array,
                                 uint32_t index,
                                 Handle<Object> value);

  // This accessor applies the correct conversion from Smi, HeapNumber
  // and undefined.
  MUST_USE_RESULT MaybeObject* SetValue(uint32_t index, Object* value);

  // Casting.
  static inline ExternalUint16Array* cast(Object* obj);

  // Dispatched behavior.
  DECLARE_PRINTER(ExternalUint16Array)
  DECLARE_VERIFIER(ExternalUint16Array)

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(ExternalUint16Array);
};


class ExternalInt32Array: public ExternalArray {
 public:
  // Setter and getter.
  inline int32_t get_scalar(int index);
  MUST_USE_RESULT inline MaybeObject* get(int index);
  inline void set(int index, int32_t value);

  static Handle<Object> SetValue(Handle<ExternalInt32Array> array,
                                 uint32_t index,
                                 Handle<Object> value);

  // This accessor applies the correct conversion from Smi, HeapNumber
  // and undefined.
  MUST_USE_RESULT MaybeObject* SetValue(uint32_t index, Object* value);

  // Casting.
  static inline ExternalInt32Array* cast(Object* obj);

  // Dispatched behavior.
  DECLARE_PRINTER(ExternalInt32Array)
  DECLARE_VERIFIER(ExternalInt32Array)

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(ExternalInt32Array);
};


class ExternalUint32Array: public ExternalArray {
 public:
  // Setter and getter.
  inline uint32_t get_scalar(int index);
  MUST_USE_RESULT inline MaybeObject* get(int index);
  inline void set(int index, uint32_t value);

  static Handle<Object> SetValue(Handle<ExternalUint32Array> array,
                                 uint32_t index,
                                 Handle<Object> value);

  // This accessor applies the correct conversion from Smi, HeapNumber
  // and undefined.
  MUST_USE_RESULT MaybeObject* SetValue(uint32_t index, Object* value);

  // Casting.
  static inline ExternalUint32Array* cast(Object* obj);

  // Dispatched behavior.
  DECLARE_PRINTER(ExternalUint32Array)
  DECLARE_VERIFIER(ExternalUint32Array)

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(ExternalUint32Array);
};


class ExternalFloat32Array: public ExternalArray {
 public:
  // Setter and getter.
  inline float get_scalar(int index);
  MUST_USE_RESULT inline MaybeObject* get(int index);
  inline void set(int index, float value);

  static Handle<Object> SetValue(Handle<ExternalFloat32Array> array,
                                 uint32_t index,
                                 Handle<Object> value);

  // This accessor applies the correct conversion from Smi, HeapNumber
  // and undefined.
  MUST_USE_RESULT MaybeObject* SetValue(uint32_t index, Object* value);

  // Casting.
  static inline ExternalFloat32Array* cast(Object* obj);

  // Dispatched behavior.
  DECLARE_PRINTER(ExternalFloat32Array)
  DECLARE_VERIFIER(ExternalFloat32Array)

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(ExternalFloat32Array);
};


class ExternalFloat64Array: public ExternalArray {
 public:
  // Setter and getter.
  inline double get_scalar(int index);
  MUST_USE_RESULT inline MaybeObject* get(int index);
  inline void set(int index, double value);

  static Handle<Object> SetValue(Handle<ExternalFloat64Array> array,
                                 uint32_t index,
                                 Handle<Object> value);

  // This accessor applies the correct conversion from Smi, HeapNumber
  // and undefined.
  MUST_USE_RESULT MaybeObject* SetValue(uint32_t index, Object* value);

  // Casting.
  static inline ExternalFloat64Array* cast(Object* obj);

  // Dispatched behavior.
  DECLARE_PRINTER(ExternalFloat64Array)
  DECLARE_VERIFIER(ExternalFloat64Array)

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(ExternalFloat64Array);
};


class FixedTypedArrayBase: public FixedArrayBase {
 public:
  // Casting:
  static inline FixedTypedArrayBase* cast(Object* obj);

  static const int kDataOffset = kHeaderSize;

  inline int size();

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(FixedTypedArrayBase);
};


template <class Traits>
class FixedTypedArray: public FixedTypedArrayBase {
 public:
  typedef typename Traits::ElementType ElementType;
  static const InstanceType kInstanceType = Traits::kInstanceType;

  // Casting:
  static inline FixedTypedArray<Traits>* cast(Object* obj);

  static inline int ElementOffset(int index) {
    return kDataOffset + index * sizeof(ElementType);
  }

  static inline int SizeFor(int length) {
    return ElementOffset(length);
  }

  inline ElementType get_scalar(int index);
  MUST_USE_RESULT inline MaybeObject* get(int index);
  inline void set(int index, ElementType value);

  // This accessor applies the correct conversion from Smi, HeapNumber
  // and undefined.
  MUST_USE_RESULT MaybeObject* SetValue(uint32_t index, Object* value);

  static Handle<Object> SetValue(Handle<FixedTypedArray<Traits> > array,
                                 uint32_t index,
                                 Handle<Object> value);

  DECLARE_PRINTER(FixedTypedArray)
  DECLARE_VERIFIER(FixedTypedArray)

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(FixedTypedArray);
};

#define FIXED_TYPED_ARRAY_TRAITS(Type, type, TYPE, elementType, size)         \
  class Type##ArrayTraits {                                                   \
    public:                                                                   \
      typedef elementType ElementType;                                        \
      static const InstanceType kInstanceType = FIXED_##TYPE##_ARRAY_TYPE;    \
      static const char* Designator() { return #type " array"; }              \
      static inline MaybeObject* ToObject(Heap* heap, elementType scalar);    \
      static elementType defaultValue() { return 0; }                         \
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
  static const int kFirstDeoptEntryIndex = 6;

  // Offsets of deopt entry elements relative to the start of the entry.
  static const int kAstIdRawOffset = 0;
  static const int kTranslationIndexOffset = 1;
  static const int kArgumentsStackHeightOffset = 2;
  static const int kPcOffset = 3;
  static const int kDeoptEntrySize = 4;

  // Simple element accessors.
#define DEFINE_ELEMENT_ACCESSORS(name, type)      \
  type* name() {                                  \
    return type::cast(get(k##name##Index));       \
  }                                               \
  void Set##name(type* value) {                   \
    set(k##name##Index, value);                   \
  }

  DEFINE_ELEMENT_ACCESSORS(TranslationByteArray, ByteArray)
  DEFINE_ELEMENT_ACCESSORS(InlinedFunctionCount, Smi)
  DEFINE_ELEMENT_ACCESSORS(LiteralArray, FixedArray)
  DEFINE_ELEMENT_ACCESSORS(OsrAstId, Smi)
  DEFINE_ELEMENT_ACCESSORS(OsrPcOffset, Smi)
  DEFINE_ELEMENT_ACCESSORS(OptimizationId, Smi)

#undef DEFINE_ELEMENT_ACCESSORS

  // Accessors for elements of the ith deoptimization entry.
#define DEFINE_ENTRY_ACCESSORS(name, type)                       \
  type* name(int i) {                                            \
    return type::cast(get(IndexForEntry(i) + k##name##Offset));  \
  }                                                              \
  void Set##name(int i, type* value) {                           \
    set(IndexForEntry(i) + k##name##Offset, value);              \
  }

  DEFINE_ENTRY_ACCESSORS(AstIdRaw, Smi)
  DEFINE_ENTRY_ACCESSORS(TranslationIndex, Smi)
  DEFINE_ENTRY_ACCESSORS(ArgumentsStackHeight, Smi)
  DEFINE_ENTRY_ACCESSORS(Pc, Smi)

#undef DEFINE_ENTRY_ACCESSORS

  BailoutId AstId(int i) {
    return BailoutId(AstIdRaw(i)->value());
  }

  void SetAstId(int i, BailoutId value) {
    SetAstIdRaw(i, Smi::FromInt(value.ToInt()));
  }

  int DeoptCount() {
    return (length() - kFirstDeoptEntryIndex) / kDeoptEntrySize;
  }

  // Allocates a DeoptimizationInputData.
  MUST_USE_RESULT static MaybeObject* Allocate(Isolate* isolate,
                                               int deopt_entry_count,
                                               PretenureFlag pretenure);

  // Casting.
  static inline DeoptimizationInputData* cast(Object* obj);

#ifdef ENABLE_DISASSEMBLER
  void DeoptimizationInputDataPrint(FILE* out);
#endif

 private:
  static int IndexForEntry(int i) {
    return kFirstDeoptEntryIndex + (i * kDeoptEntrySize);
  }

  static int LengthFor(int entry_count) {
    return IndexForEntry(entry_count);
  }
};


// DeoptimizationOutputData is a fixed array used to hold the deoptimization
// data for code generated by the full compiler.
// The format of the these objects is
//   [i * 2]: Ast ID for ith deoptimization.
//   [i * 2 + 1]: PC and state of ith deoptimization
class DeoptimizationOutputData: public FixedArray {
 public:
  int DeoptPoints() { return length() / 2; }

  BailoutId AstId(int index) {
    return BailoutId(Smi::cast(get(index * 2))->value());
  }

  void SetAstId(int index, BailoutId id) {
    set(index * 2, Smi::FromInt(id.ToInt()));
  }

  Smi* PcAndState(int index) { return Smi::cast(get(1 + index * 2)); }
  void SetPcAndState(int index, Smi* offset) { set(1 + index * 2, offset); }

  static int LengthOfFixedArray(int deopt_points) {
    return deopt_points * 2;
  }

  // Allocates a DeoptimizationOutputData.
  MUST_USE_RESULT static MaybeObject* Allocate(Isolate* isolate,
                                               int number_of_deopt_points,
                                               PretenureFlag pretenure);

  // Casting.
  static inline DeoptimizationOutputData* cast(Object* obj);

#if defined(OBJECT_PRINT) || defined(ENABLE_DISASSEMBLER)
  void DeoptimizationOutputDataPrint(FILE* out);
#endif
};


// Forward declaration.
class Cell;
class PropertyCell;
class SafepointEntry;
class TypeFeedbackInfo;

// Code describes objects with on-the-fly generated machine code.
class Code: public HeapObject {
 public:
  // Opaque data type for encapsulating code flags like kind, inline
  // cache state, and arguments count.
  typedef uint32_t Flags;

#define NON_IC_KIND_LIST(V) \
  V(FUNCTION)               \
  V(OPTIMIZED_FUNCTION)     \
  V(STUB)                   \
  V(HANDLER)                \
  V(BUILTIN)                \
  V(REGEXP)

#define IC_KIND_LIST(V) \
  V(LOAD_IC)            \
  V(KEYED_LOAD_IC)      \
  V(STORE_IC)           \
  V(KEYED_STORE_IC)     \
  V(BINARY_OP_IC)       \
  V(COMPARE_IC)         \
  V(COMPARE_NIL_IC)     \
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

  // No more than 16 kinds. The value is currently encoded in four bits in
  // Flags.
  STATIC_ASSERT(NUMBER_OF_KINDS <= 16);

  static const char* Kind2String(Kind kind);

  // Types of stubs.
  enum StubType {
    NORMAL,
    FAST
  };

  static const int kPrologueOffsetNotSet = -1;

#ifdef ENABLE_DISASSEMBLER
  // Printing
  static const char* ICState2String(InlineCacheState state);
  static const char* StubType2String(StubType type);
  static void PrintExtraICState(FILE* out, Kind kind, ExtraICState extra);
  void Disassemble(const char* name, FILE* out = stdout);
#endif  // ENABLE_DISASSEMBLER

  // [instruction_size]: Size of the native instructions
  inline int instruction_size();
  inline void set_instruction_size(int value);

  // [relocation_info]: Code relocation information
  DECL_ACCESSORS(relocation_info, ByteArray)
  void InvalidateRelocation();
  void InvalidateEmbeddedObjects();

  // [handler_table]: Fixed array containing offsets of exception handlers.
  DECL_ACCESSORS(handler_table, FixedArray)

  // [deoptimization_data]: Array containing data for deopt.
  DECL_ACCESSORS(deoptimization_data, FixedArray)

  // [raw_type_feedback_info]: This field stores various things, depending on
  // the kind of the code object.
  //   FUNCTION           => type feedback information.
  //   STUB               => various things, e.g. a SMI
  //   OPTIMIZED_FUNCTION => the next_code_link for optimized code list.
  DECL_ACCESSORS(raw_type_feedback_info, Object)
  inline Object* type_feedback_info();
  inline void set_type_feedback_info(
      Object* value, WriteBarrierMode mode = UPDATE_WRITE_BARRIER);
  inline int stub_info();
  inline void set_stub_info(int info);

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
  inline int ic_age();

  // [prologue_offset]: Offset of the function prologue, used for aging
  // FUNCTIONs and OPTIMIZED_FUNCTIONs.
  inline int prologue_offset();
  inline void set_prologue_offset(int offset);

  // Unchecked accessors to be used during GC.
  inline ByteArray* unchecked_relocation_info();

  inline int relocation_size();

  // [flags]: Various code flags.
  inline Flags flags();
  inline void set_flags(Flags flags);

  // [flags]: Access to specific code flags.
  inline Kind kind();
  inline InlineCacheState ic_state();  // Only valid for IC stubs.
  inline ExtraICState extra_ic_state();  // Only valid for IC stubs.

  inline StubType type();  // Only valid for monomorphic IC stubs.

  // Testers for IC stub kinds.
  inline bool is_inline_cache_stub();
  inline bool is_debug_stub();
  inline bool is_handler() { return kind() == HANDLER; }
  inline bool is_load_stub() { return kind() == LOAD_IC; }
  inline bool is_keyed_load_stub() { return kind() == KEYED_LOAD_IC; }
  inline bool is_store_stub() { return kind() == STORE_IC; }
  inline bool is_keyed_store_stub() { return kind() == KEYED_STORE_IC; }
  inline bool is_binary_op_stub() { return kind() == BINARY_OP_IC; }
  inline bool is_compare_ic_stub() { return kind() == COMPARE_IC; }
  inline bool is_compare_nil_ic_stub() { return kind() == COMPARE_NIL_IC; }
  inline bool is_to_boolean_ic_stub() { return kind() == TO_BOOLEAN_IC; }
  inline bool is_keyed_stub();

  inline void set_raw_kind_specific_flags1(int value);
  inline void set_raw_kind_specific_flags2(int value);

  // [major_key]: For kind STUB or BINARY_OP_IC, the major key.
  inline int major_key();
  inline void set_major_key(int value);
  inline bool has_major_key();

  // For kind STUB or ICs, tells whether or not a code object was generated by
  // the optimizing compiler (but it may not be an optimized function).
  bool is_crankshafted();
  inline void set_is_crankshafted(bool value);

  // [optimizable]: For FUNCTION kind, tells if it is optimizable.
  inline bool optimizable();
  inline void set_optimizable(bool value);

  // [has_deoptimization_support]: For FUNCTION kind, tells if it has
  // deoptimization support.
  inline bool has_deoptimization_support();
  inline void set_has_deoptimization_support(bool value);

  // [has_debug_break_slots]: For FUNCTION kind, tells if it has
  // been compiled with debug break slots.
  inline bool has_debug_break_slots();
  inline void set_has_debug_break_slots(bool value);

  // [compiled_with_optimizing]: For FUNCTION kind, tells if it has
  // been compiled with IsOptimizing set to true.
  inline bool is_compiled_optimizable();
  inline void set_compiled_optimizable(bool value);

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

  // [stack_slots]: For kind OPTIMIZED_FUNCTION, the number of stack slots
  // reserved in the code prologue.
  inline unsigned stack_slots();
  inline void set_stack_slots(unsigned slots);

  // [safepoint_table_start]: For kind OPTIMIZED_CODE, the offset in
  // the instruction stream where the safepoint table starts.
  inline unsigned safepoint_table_offset();
  inline void set_safepoint_table_offset(unsigned offset);

  // [back_edge_table_start]: For kind FUNCTION, the offset in the
  // instruction stream where the back edge table starts.
  inline unsigned back_edge_table_offset();
  inline void set_back_edge_table_offset(unsigned offset);

  inline bool back_edges_patched_for_osr();
  inline void set_back_edges_patched_for_osr(bool value);

  // [to_boolean_foo]: For kind TO_BOOLEAN_IC tells what state the stub is in.
  inline byte to_boolean_state();

  // [has_function_cache]: For kind STUB tells whether there is a function
  // cache is passed to the stub.
  inline bool has_function_cache();
  inline void set_has_function_cache(bool flag);


  // [marked_for_deoptimization]: For kind OPTIMIZED_FUNCTION tells whether
  // the code is going to be deoptimized because of dead embedded maps.
  inline bool marked_for_deoptimization();
  inline void set_marked_for_deoptimization(bool flag);

  // [constant_pool]: The constant pool for this function.
  inline ConstantPoolArray* constant_pool();
  inline void set_constant_pool(Object* constant_pool);

  // Get the safepoint entry for the given pc.
  SafepointEntry GetSafepointEntry(Address pc);

  // Find an object in a stub with a specified map
  Object* FindNthObject(int n, Map* match_map);
  void ReplaceNthObject(int n, Map* match_map, Object* replace_with);

  // Find the first allocation site in an IC stub.
  AllocationSite* FindFirstAllocationSite();

  // Find the first map in an IC stub.
  Map* FindFirstMap();
  void FindAllMaps(MapHandleList* maps);
  void FindAllTypes(TypeHandleList* types);
  void ReplaceFirstMap(Map* replace);

  // Find the first handler in an IC stub.
  Code* FindFirstHandler();

  // Find |length| handlers and put them into |code_list|. Returns false if not
  // enough handlers can be found.
  bool FindHandlers(CodeHandleList* code_list, int length = -1);

  // Find the first name in an IC stub.
  Name* FindFirstName();

  void ReplaceNthCell(int n, Cell* replace_with);

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
      Kind kind,
      InlineCacheState ic_state = UNINITIALIZED,
      ExtraICState extra_ic_state = kNoExtraICState,
      StubType type = NORMAL,
      InlineCacheHolderFlag holder = OWN_MAP);

  static inline Flags ComputeMonomorphicFlags(
      Kind kind,
      ExtraICState extra_ic_state = kNoExtraICState,
      InlineCacheHolderFlag holder = OWN_MAP,
      StubType type = NORMAL);

  static inline Flags ComputeHandlerFlags(
      Kind handler_kind,
      StubType type = NORMAL,
      InlineCacheHolderFlag holder = OWN_MAP);

  static inline InlineCacheState ExtractICStateFromFlags(Flags flags);
  static inline StubType ExtractTypeFromFlags(Flags flags);
  static inline Kind ExtractKindFromFlags(Flags flags);
  static inline InlineCacheHolderFlag ExtractCacheHolderFromFlags(Flags flags);
  static inline ExtraICState ExtractExtraICStateFromFlags(Flags flags);

  static inline Flags RemoveTypeFromFlags(Flags flags);

  // Convert a target address into a code object.
  static inline Code* GetCodeFromTargetAddress(Address address);

  // Convert an entry address into an object.
  static inline Object* GetObjectFromEntryAddress(Address location_of_address);

  // Returns the address of the first instruction.
  inline byte* instruction_start();

  // Returns the address right after the last instruction.
  inline byte* instruction_end();

  // Returns the size of the instructions, padding, and relocation information.
  inline int body_size();

  // Returns the address of the first relocation info (read backwards!).
  inline byte* relocation_start();

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
    ASSERT_SIZE_TAG_ALIGNED(body_size);
    return RoundUp(kHeaderSize + body_size, kCodeAlignment);
  }

  // Calculate the size of the code object to report for log events. This takes
  // the layout of the code object into account.
  int ExecutableSize() {
    // Check that the assumptions about the layout of the code object holds.
    ASSERT_EQ(static_cast<int>(instruction_start() - address()),
              Code::kHeaderSize);
    return instruction_size() + Code::kHeaderSize;
  }

  // Locating source position.
  int SourcePosition(Address pc);
  int SourceStatementPosition(Address pc);

  // Casting.
  static inline Code* cast(Object* obj);

  // Dispatched behavior.
  int CodeSize() { return SizeFor(body_size()); }
  inline void CodeIterateBody(ObjectVisitor* v);

  template<typename StaticVisitor>
  inline void CodeIterateBody(Heap* heap);

  DECLARE_PRINTER(Code)
  DECLARE_VERIFIER(Code)

  void ClearInlineCaches();
  void ClearInlineCaches(Kind kind);

  void ClearTypeFeedbackInfo(Heap* heap);

  BailoutId TranslatePcOffsetToAstId(uint32_t pc_offset);
  uint32_t TranslateAstIdToPcOffset(BailoutId ast_id);

#define DECLARE_CODE_AGE_ENUM(X) k##X##CodeAge,
  enum Age {
    kNotExecutedCodeAge = -2,
    kExecutedOnceCodeAge = -1,
    kNoAgeCodeAge = 0,
    CODE_AGE_LIST(DECLARE_CODE_AGE_ENUM)
    kAfterLastCodeAge,
    kFirstCodeAge = kNotExecutedCodeAge,
    kLastCodeAge = kAfterLastCodeAge - 1,
    kCodeAgeCount = kAfterLastCodeAge - kNotExecutedCodeAge - 1,
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
  void MakeOlder(MarkingParity);
  static bool IsYoungSequence(byte* sequence);
  bool IsOld();
  Age GetAge();
  // Gets the raw code age, including psuedo code-age values such as
  // kNotExecutedCodeAge and kExecutedOnceCodeAge.
  Age GetRawAge();
  static inline Code* GetPreAgedCodeAgeStub(Isolate* isolate) {
    return GetCodeAgeStub(isolate, kNotExecutedCodeAge, NO_MARKING_PARITY);
  }

  void PrintDeoptLocation(FILE* out, int bailout_id);
  bool CanDeoptAt(Address pc);

#ifdef VERIFY_HEAP
  void VerifyEmbeddedObjectsDependency();
#endif

  static bool IsWeakEmbeddedObject(Kind kind, Object* object);

  // Max loop nesting marker used to postpose OSR. We don't take loop
  // nesting that is deeper than 5 levels into account.
  static const int kMaxLoopNestingMarker = 6;

  // Layout description.
  static const int kInstructionSizeOffset = HeapObject::kHeaderSize;
  static const int kRelocationInfoOffset = kInstructionSizeOffset + kIntSize;
  static const int kHandlerTableOffset = kRelocationInfoOffset + kPointerSize;
  static const int kDeoptimizationDataOffset =
      kHandlerTableOffset + kPointerSize;
  static const int kTypeFeedbackInfoOffset =
      kDeoptimizationDataOffset + kPointerSize;
  static const int kNextCodeLinkOffset = kTypeFeedbackInfoOffset;  // Shared.
  static const int kGCMetadataOffset = kTypeFeedbackInfoOffset + kPointerSize;
  static const int kICAgeOffset =
      kGCMetadataOffset + kPointerSize;
  static const int kFlagsOffset = kICAgeOffset + kIntSize;
  static const int kKindSpecificFlags1Offset = kFlagsOffset + kIntSize;
  static const int kKindSpecificFlags2Offset =
      kKindSpecificFlags1Offset + kIntSize;
  // Note: We might be able to squeeze this into the flags above.
  static const int kPrologueOffset = kKindSpecificFlags2Offset + kIntSize;
  static const int kConstantPoolOffset = kPrologueOffset + kPointerSize;

  static const int kHeaderPaddingStart = kConstantPoolOffset + kIntSize;

  // Add padding to align the instruction start following right after
  // the Code object header.
  static const int kHeaderSize =
      (kHeaderPaddingStart + kCodeAlignmentMask) & ~kCodeAlignmentMask;

  // Byte offsets within kKindSpecificFlags1Offset.
  static const int kOptimizableOffset = kKindSpecificFlags1Offset;

  static const int kFullCodeFlags = kOptimizableOffset + 1;
  class FullCodeFlagsHasDeoptimizationSupportField:
      public BitField<bool, 0, 1> {};  // NOLINT
  class FullCodeFlagsHasDebugBreakSlotsField: public BitField<bool, 1, 1> {};
  class FullCodeFlagsIsCompiledOptimizable: public BitField<bool, 2, 1> {};

  static const int kAllowOSRAtLoopNestingLevelOffset = kFullCodeFlags + 1;
  static const int kProfilerTicksOffset = kAllowOSRAtLoopNestingLevelOffset + 1;

  // Flags layout.  BitField<type, shift, size>.
  class ICStateField: public BitField<InlineCacheState, 0, 3> {};
  class TypeField: public BitField<StubType, 3, 1> {};
  class CacheHolderField: public BitField<InlineCacheHolderFlag, 5, 1> {};
  class KindField: public BitField<Kind, 6, 4> {};
  // TODO(bmeurer): Bit 10 is available for free use. :-)
  class ExtraICStateField: public BitField<ExtraICState, 11,
      PlatformSmiTagging::kSmiValueSize - 11 + 1> {};  // NOLINT

  // KindSpecificFlags1 layout (STUB and OPTIMIZED_FUNCTION)
  static const int kStackSlotsFirstBit = 0;
  static const int kStackSlotsBitCount = 24;
  static const int kHasFunctionCacheFirstBit =
      kStackSlotsFirstBit + kStackSlotsBitCount;
  static const int kHasFunctionCacheBitCount = 1;
  static const int kMarkedForDeoptimizationFirstBit =
      kStackSlotsFirstBit + kStackSlotsBitCount + 1;
  static const int kMarkedForDeoptimizationBitCount = 1;

  STATIC_ASSERT(kStackSlotsFirstBit + kStackSlotsBitCount <= 32);
  STATIC_ASSERT(kHasFunctionCacheFirstBit + kHasFunctionCacheBitCount <= 32);
  STATIC_ASSERT(kMarkedForDeoptimizationFirstBit +
                kMarkedForDeoptimizationBitCount <= 32);

  class StackSlotsField: public BitField<int,
      kStackSlotsFirstBit, kStackSlotsBitCount> {};  // NOLINT
  class HasFunctionCacheField: public BitField<bool,
      kHasFunctionCacheFirstBit, kHasFunctionCacheBitCount> {};  // NOLINT
  class MarkedForDeoptimizationField: public BitField<bool,
      kMarkedForDeoptimizationFirstBit,
      kMarkedForDeoptimizationBitCount> {};  // NOLINT

  // KindSpecificFlags2 layout (ALL)
  static const int kIsCrankshaftedBit = 0;
  class IsCrankshaftedField: public BitField<bool,
      kIsCrankshaftedBit, 1> {};  // NOLINT

  // KindSpecificFlags2 layout (STUB and OPTIMIZED_FUNCTION)
  static const int kStubMajorKeyFirstBit = kIsCrankshaftedBit + 1;
  static const int kSafepointTableOffsetFirstBit =
      kStubMajorKeyFirstBit + kStubMajorKeyBits;
  static const int kSafepointTableOffsetBitCount = 24;

  STATIC_ASSERT(kStubMajorKeyFirstBit + kStubMajorKeyBits <= 32);
  STATIC_ASSERT(kSafepointTableOffsetFirstBit +
                kSafepointTableOffsetBitCount <= 32);
  STATIC_ASSERT(1 + kStubMajorKeyBits +
                kSafepointTableOffsetBitCount <= 32);

  class SafepointTableOffsetField: public BitField<int,
      kSafepointTableOffsetFirstBit,
      kSafepointTableOffsetBitCount> {};  // NOLINT
  class StubMajorKeyField: public BitField<int,
      kStubMajorKeyFirstBit, kStubMajorKeyBits> {};  // NOLINT

  // KindSpecificFlags2 layout (FUNCTION)
  class BackEdgeTableOffsetField: public BitField<int,
      kIsCrankshaftedBit + 1, 29> {};  // NOLINT
  class BackEdgesPatchedForOSRField: public BitField<bool,
      kIsCrankshaftedBit + 1 + 29, 1> {};  // NOLINT

  static const int kArgumentsBits = 16;
  static const int kMaxArguments = (1 << kArgumentsBits) - 1;

  // This constant should be encodable in an ARM instruction.
  static const int kFlagsNotUsedInLookup =
      TypeField::kMask | CacheHolderField::kMask;

 private:
  friend class RelocIterator;

  void ClearInlineCaches(Kind* kind);

  // Code aging
  byte* FindCodeAgeSequence();
  static void GetCodeAgeAndParity(Code* code, Age* age,
                                  MarkingParity* parity);
  static void GetCodeAgeAndParity(byte* sequence, Age* age,
                                  MarkingParity* parity);
  static Code* GetCodeAgeStub(Isolate* isolate, Age age, MarkingParity parity);

  // Code aging -- platform-specific
  static void PatchPlatformCodeAge(Isolate* isolate,
                                   byte* sequence, Age age,
                                   MarkingParity parity);

  DISALLOW_IMPLICIT_CONSTRUCTORS(Code);
};


class CompilationInfo;

// This class describes the layout of dependent codes array of a map. The
// array is partitioned into several groups of dependent codes. Each group
// contains codes with the same dependency on the map. The array has the
// following layout for n dependency groups:
//
// +----+----+-----+----+---------+----------+-----+---------+-----------+
// | C1 | C2 | ... | Cn | group 1 |  group 2 | ... | group n | undefined |
// +----+----+-----+----+---------+----------+-----+---------+-----------+
//
// The first n elements are Smis, each of them specifies the number of codes
// in the corresponding group. The subsequent elements contain grouped code
// objects. The suffix of the array can be filled with the undefined value if
// the number of codes is less than the length of the array. The order of the
// code objects within a group is not preserved.
//
// All code indexes used in the class are counted starting from the first
// code object of the first group. In other words, code index 0 corresponds
// to array index n = kCodesStartIndex.

class DependentCode: public FixedArray {
 public:
  enum DependencyGroup {
    // Group of code that weakly embed this map and depend on being
    // deoptimized when the map is garbage collected.
    kWeaklyEmbeddedGroup,
    // Group of code that embed a transition to this map, and depend on being
    // deoptimized when the transition is replaced by a new version.
    kTransitionGroup,
    // Group of code that omit run-time prototype checks for prototypes
    // described by this map. The group is deoptimized whenever an object
    // described by this map changes shape (and transitions to a new map),
    // possibly invalidating the assumptions embedded in the code.
    kPrototypeCheckGroup,
    // Group of code that depends on elements not being added to objects with
    // this map.
    kElementsCantBeAddedGroup,
    // Group of code that depends on global property values in property cells
    // not being changed.
    kPropertyCellChangedGroup,
    // Group of code that depends on tenuring information in AllocationSites
    // not being changed.
    kAllocationSiteTenuringChangedGroup,
    // Group of code that depends on element transition information in
    // AllocationSites not being changed.
    kAllocationSiteTransitionChangedGroup,
    kGroupCount = kAllocationSiteTransitionChangedGroup + 1
  };

  // Array for holding the index of the first code object of each group.
  // The last element stores the total number of code objects.
  class GroupStartIndexes {
   public:
    explicit GroupStartIndexes(DependentCode* entries);
    void Recompute(DependentCode* entries);
    int at(int i) { return start_indexes_[i]; }
    int number_of_entries() { return start_indexes_[kGroupCount]; }
   private:
    int start_indexes_[kGroupCount + 1];
  };

  bool Contains(DependencyGroup group, Code* code);
  static Handle<DependentCode> Insert(Handle<DependentCode> entries,
                                      DependencyGroup group,
                                      Handle<Object> object);
  void UpdateToFinishedCode(DependencyGroup group,
                            CompilationInfo* info,
                            Code* code);
  void RemoveCompilationInfo(DependentCode::DependencyGroup group,
                             CompilationInfo* info);

  void DeoptimizeDependentCodeGroup(Isolate* isolate,
                                    DependentCode::DependencyGroup group);

  bool MarkCodeForDeoptimization(Isolate* isolate,
                                 DependentCode::DependencyGroup group);

  // The following low-level accessors should only be used by this class
  // and the mark compact collector.
  inline int number_of_entries(DependencyGroup group);
  inline void set_number_of_entries(DependencyGroup group, int value);
  inline bool is_code_at(int i);
  inline Code* code_at(int i);
  inline CompilationInfo* compilation_info_at(int i);
  inline void set_object_at(int i, Object* object);
  inline Object** slot_at(int i);
  inline Object* object_at(int i);
  inline void clear_at(int i);
  inline void copy(int from, int to);
  static inline DependentCode* cast(Object* object);

  static DependentCode* ForObject(Handle<HeapObject> object,
                                  DependencyGroup group);

 private:
  // Make a room at the end of the given group by moving out the first
  // code objects of the subsequent groups.
  inline void ExtendGroup(DependencyGroup group);
  static const int kCodesStartIndex = kGroupCount;
};


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

  // Count of properties allocated in the object.
  inline int inobject_properties();
  inline void set_inobject_properties(int value);

  // Count of property fields pre-allocated in the object when first allocated.
  inline int pre_allocated_property_fields();
  inline void set_pre_allocated_property_fields(int value);

  // Instance type.
  inline InstanceType instance_type();
  inline void set_instance_type(InstanceType value);

  // Tells how many unused property fields are available in the
  // instance (only used for JSObject in fast mode).
  inline int unused_property_fields();
  inline void set_unused_property_fields(int value);

  // Bit field.
  inline byte bit_field();
  inline void set_bit_field(byte value);

  // Bit field 2.
  inline byte bit_field2();
  inline void set_bit_field2(byte value);

  // Bit field 3.
  inline uint32_t bit_field3();
  inline void set_bit_field3(uint32_t bits);

  class EnumLengthBits:             public BitField<int,
      0, kDescriptorIndexBitCount> {};  // NOLINT
  class NumberOfOwnDescriptorsBits: public BitField<int,
      kDescriptorIndexBitCount, kDescriptorIndexBitCount> {};  // NOLINT
  STATIC_ASSERT(kDescriptorIndexBitCount + kDescriptorIndexBitCount == 20);
  class IsShared:                   public BitField<bool, 20,  1> {};
  class FunctionWithPrototype:      public BitField<bool, 21,  1> {};
  class DictionaryMap:              public BitField<bool, 22,  1> {};
  class OwnsDescriptors:            public BitField<bool, 23,  1> {};
  class HasInstanceCallHandler:     public BitField<bool, 24,  1> {};
  class Deprecated:                 public BitField<bool, 25,  1> {};
  class IsFrozen:                   public BitField<bool, 26,  1> {};
  class IsUnstable:                 public BitField<bool, 27,  1> {};
  class IsMigrationTarget:          public BitField<bool, 28,  1> {};

  // Tells whether the object in the prototype property will be used
  // for instances created from this function.  If the prototype
  // property is set to a value that is not a JSObject, the prototype
  // property will not be used to create instances of the function.
  // See ECMA-262, 13.2.2.
  inline void set_non_instance_prototype(bool value);
  inline bool has_non_instance_prototype();

  // Tells whether function has special prototype property. If not, prototype
  // property will not be created when accessed (will return undefined),
  // and construction from this function will not be allowed.
  inline void set_function_with_prototype(bool value);
  inline bool function_with_prototype();

  // Tells whether the instance with this map should be ignored by the
  // Object.getPrototypeOf() function and the __proto__ accessor.
  inline void set_is_hidden_prototype() {
    set_bit_field(bit_field() | (1 << kIsHiddenPrototype));
  }

  inline bool is_hidden_prototype() {
    return ((1 << kIsHiddenPrototype) & bit_field()) != 0;
  }

  // Records and queries whether the instance has a named interceptor.
  inline void set_has_named_interceptor() {
    set_bit_field(bit_field() | (1 << kHasNamedInterceptor));
  }

  inline bool has_named_interceptor() {
    return ((1 << kHasNamedInterceptor) & bit_field()) != 0;
  }

  // Records and queries whether the instance has an indexed interceptor.
  inline void set_has_indexed_interceptor() {
    set_bit_field(bit_field() | (1 << kHasIndexedInterceptor));
  }

  inline bool has_indexed_interceptor() {
    return ((1 << kHasIndexedInterceptor) & bit_field()) != 0;
  }

  // Tells whether the instance is undetectable.
  // An undetectable object is a special class of JSObject: 'typeof' operator
  // returns undefined, ToBoolean returns false. Otherwise it behaves like
  // a normal JS object.  It is useful for implementing undetectable
  // document.all in Firefox & Safari.
  // See https://bugzilla.mozilla.org/show_bug.cgi?id=248549.
  inline void set_is_undetectable() {
    set_bit_field(bit_field() | (1 << kIsUndetectable));
  }

  inline bool is_undetectable() {
    return ((1 << kIsUndetectable) & bit_field()) != 0;
  }

  // Tells whether the instance has a call-as-function handler.
  inline void set_is_observed() {
    set_bit_field(bit_field() | (1 << kIsObserved));
  }

  inline bool is_observed() {
    return ((1 << kIsObserved) & bit_field()) != 0;
  }

  inline void set_is_extensible(bool value);
  inline bool is_extensible();

  inline void set_elements_kind(ElementsKind elements_kind) {
    ASSERT(elements_kind < kElementsKindCount);
    ASSERT(kElementsKindCount <= (1 << kElementsKindBitCount));
    set_bit_field2((bit_field2() & ~kElementsKindMask) |
        (elements_kind << kElementsKindShift));
    ASSERT(this->elements_kind() == elements_kind);
  }

  inline ElementsKind elements_kind() {
    return static_cast<ElementsKind>(
        (bit_field2() & kElementsKindMask) >> kElementsKindShift);
  }

  // Tells whether the instance has fast elements that are only Smis.
  inline bool has_fast_smi_elements() {
    return IsFastSmiElementsKind(elements_kind());
  }

  // Tells whether the instance has fast elements.
  inline bool has_fast_object_elements() {
    return IsFastObjectElementsKind(elements_kind());
  }

  inline bool has_fast_smi_or_object_elements() {
    return IsFastSmiOrObjectElementsKind(elements_kind());
  }

  inline bool has_fast_double_elements() {
    return IsFastDoubleElementsKind(elements_kind());
  }

  inline bool has_fast_elements() {
    return IsFastElementsKind(elements_kind());
  }

  inline bool has_non_strict_arguments_elements() {
    return elements_kind() == NON_STRICT_ARGUMENTS_ELEMENTS;
  }

  inline bool has_external_array_elements() {
    return IsExternalArrayElementsKind(elements_kind());
  }

  inline bool has_fixed_typed_array_elements() {
    return IsFixedTypedArrayElementsKind(elements_kind());
  }

  inline bool has_dictionary_elements() {
    return IsDictionaryElementsKind(elements_kind());
  }

  inline bool has_slow_elements_kind() {
    return elements_kind() == DICTIONARY_ELEMENTS
        || elements_kind() == NON_STRICT_ARGUMENTS_ELEMENTS;
  }

  static bool IsValidElementsTransition(ElementsKind from_kind,
                                        ElementsKind to_kind);

  // Returns true if the current map doesn't have DICTIONARY_ELEMENTS but if a
  // map with DICTIONARY_ELEMENTS was found in the prototype chain.
  bool DictionaryElementsInPrototypeChainOnly();

  inline bool HasTransitionArray();
  inline bool HasElementsTransition();
  inline Map* elements_transition_map();
  MUST_USE_RESULT inline MaybeObject* set_elements_transition_map(
      Map* transitioned_map);
  inline void SetTransition(int transition_index, Map* target);
  inline Map* GetTransition(int transition_index);

  static Handle<TransitionArray> AddTransition(Handle<Map> map,
                                               Handle<Name> key,
                                               Handle<Map> target,
                                               SimpleTransitionFlag flag);

  MUST_USE_RESULT inline MaybeObject* AddTransition(Name* key,
                                                    Map* target,
                                                    SimpleTransitionFlag flag);
  DECL_ACCESSORS(transitions, TransitionArray)
  inline void ClearTransitions(Heap* heap,
                               WriteBarrierMode mode = UPDATE_WRITE_BARRIER);

  void DeprecateTransitionTree();
  void DeprecateTarget(Name* key, DescriptorArray* new_descriptors);

  Map* FindRootMap();
  Map* FindUpdatedMap(int verbatim, int length, DescriptorArray* descriptors);
  Map* FindLastMatchMap(int verbatim, int length, DescriptorArray* descriptors);

  inline int GetInObjectPropertyOffset(int index);

  int NumberOfFields();

  bool InstancesNeedRewriting(Map* target,
                              int target_number_of_fields,
                              int target_inobject,
                              int target_unused);
  static Handle<Map> GeneralizeAllFieldRepresentations(
      Handle<Map> map,
      Representation new_representation);
  static Handle<Map> GeneralizeRepresentation(
      Handle<Map> map,
      int modify_index,
      Representation new_representation,
      StoreMode store_mode);
  static Handle<Map> CopyGeneralizeAllRepresentations(
      Handle<Map> map,
      int modify_index,
      StoreMode store_mode,
      PropertyAttributes attributes,
      const char* reason);

  void PrintGeneralization(FILE* file,
                           const char* reason,
                           int modify_index,
                           int split,
                           int descriptors,
                           bool constant_to_field,
                           Representation old_representation,
                           Representation new_representation);

  // Returns the constructor name (the name (possibly, inferred name) of the
  // function that was used to instantiate the object).
  String* constructor_name();

  // Tells whether the map is attached to SharedFunctionInfo
  // (for inobject slack tracking).
  inline void set_attached_to_shared_function_info(bool value);

  inline bool attached_to_shared_function_info();

  // Tells whether the map is shared between objects that may have different
  // behavior. If true, the map should never be modified, instead a clone
  // should be created and modified.
  inline void set_is_shared(bool value);
  inline bool is_shared();

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

  // [constructor]: points back to the function responsible for this map.
  DECL_ACCESSORS(constructor, Object)

  // [instance descriptors]: describes the object.
  DECL_ACCESSORS(instance_descriptors, DescriptorArray)
  inline void InitializeDescriptors(DescriptorArray* descriptors);

  // [stub cache]: contains stubs compiled for this map.
  DECL_ACCESSORS(code_cache, Object)

  // [dependent code]: list of optimized codes that have this map embedded.
  DECL_ACCESSORS(dependent_code, DependentCode)

  // [back pointer]: points back to the parent map from which a transition
  // leads to this map. The field overlaps with prototype transitions and the
  // back pointer will be moved into the prototype transitions array if
  // required.
  inline Object* GetBackPointer();
  inline void SetBackPointer(Object* value,
                             WriteBarrierMode mode = UPDATE_WRITE_BARRIER);
  inline void init_back_pointer(Object* undefined);

  // [prototype transitions]: cache of prototype transitions.
  // Prototype transition is a transition that happens
  // when we change object's prototype to a new one.
  // Cache format:
  //    0: finger - index of the first free cell in the cache
  //    1: back pointer that overlaps with prototype transitions field.
  //    2 + 2 * i: prototype
  //    3 + 2 * i: target map
  inline FixedArray* GetPrototypeTransitions();
  MUST_USE_RESULT inline MaybeObject* SetPrototypeTransitions(
      FixedArray* prototype_transitions);
  inline bool HasPrototypeTransitions();

  inline HeapObject* UncheckedPrototypeTransitions();
  inline TransitionArray* unchecked_transition_array();

  static const int kProtoTransitionHeaderSize = 1;
  static const int kProtoTransitionNumberOfEntriesOffset = 0;
  static const int kProtoTransitionElementsPerEntry = 2;
  static const int kProtoTransitionPrototypeOffset = 0;
  static const int kProtoTransitionMapOffset = 1;

  inline int NumberOfProtoTransitions() {
    FixedArray* cache = GetPrototypeTransitions();
    if (cache->length() == 0) return 0;
    return
        Smi::cast(cache->get(kProtoTransitionNumberOfEntriesOffset))->value();
  }

  inline void SetNumberOfProtoTransitions(int value) {
    FixedArray* cache = GetPrototypeTransitions();
    ASSERT(cache->length() != 0);
    cache->set(kProtoTransitionNumberOfEntriesOffset, Smi::FromInt(value));
  }

  // Lookup in the map's instance descriptors and fill out the result
  // with the given holder if the name is found. The holder may be
  // NULL when this function is used from the compiler.
  inline void LookupDescriptor(JSObject* holder,
                               Name* name,
                               LookupResult* result);

  inline void LookupTransition(JSObject* holder,
                               Name* name,
                               LookupResult* result);

  inline PropertyDetails GetLastDescriptorDetails();

  // The size of transition arrays are limited so they do not end up in large
  // object space. Otherwise ClearNonLiveTransitions would leak memory while
  // applying in-place right trimming.
  inline bool CanHaveMoreTransitions();

  int LastAdded() {
    int number_of_own_descriptors = NumberOfOwnDescriptors();
    ASSERT(number_of_own_descriptors > 0);
    return number_of_own_descriptors - 1;
  }

  int NumberOfOwnDescriptors() {
    return NumberOfOwnDescriptorsBits::decode(bit_field3());
  }

  void SetNumberOfOwnDescriptors(int number) {
    ASSERT(number <= instance_descriptors()->number_of_descriptors());
    set_bit_field3(NumberOfOwnDescriptorsBits::update(bit_field3(), number));
  }

  inline Cell* RetrieveDescriptorsPointer();

  int EnumLength() {
    return EnumLengthBits::decode(bit_field3());
  }

  void SetEnumLength(int length) {
    if (length != kInvalidEnumCacheSentinel) {
      ASSERT(length >= 0);
      ASSERT(length == 0 || instance_descriptors()->HasEnumCache());
      ASSERT(length <= NumberOfOwnDescriptors());
    }
    set_bit_field3(EnumLengthBits::update(bit_field3(), length));
  }

  inline bool owns_descriptors();
  inline void set_owns_descriptors(bool is_shared);
  inline bool has_instance_call_handler();
  inline void set_has_instance_call_handler();
  inline void freeze();
  inline bool is_frozen();
  inline void mark_unstable();
  inline bool is_stable();
  inline void set_migration_target(bool value);
  inline bool is_migration_target();
  inline void deprecate();
  inline bool is_deprecated();
  inline bool CanBeDeprecated();
  // Returns a non-deprecated version of the input. If the input was not
  // deprecated, it is directly returned. Otherwise, the non-deprecated version
  // is found by re-transitioning from the root of the transition tree using the
  // descriptor array of the map. Returns NULL if no updated map is found.
  // This method also applies any pending migrations along the prototype chain.
  static Handle<Map> CurrentMapForDeprecated(Handle<Map> map);
  // Same as above, but does not touch the prototype chain.
  static Handle<Map> CurrentMapForDeprecatedInternal(Handle<Map> map);

  static Handle<Map> RawCopy(Handle<Map> map, int instance_size);
  MUST_USE_RESULT MaybeObject* RawCopy(int instance_size);
  MUST_USE_RESULT MaybeObject* CopyWithPreallocatedFieldDescriptors();
  static Handle<Map> CopyDropDescriptors(Handle<Map> map);
  MUST_USE_RESULT MaybeObject* CopyDropDescriptors();
  static Handle<Map> CopyReplaceDescriptors(Handle<Map> map,
                                            Handle<DescriptorArray> descriptors,
                                            TransitionFlag flag,
                                            Handle<Name> name);
  MUST_USE_RESULT MaybeObject* CopyReplaceDescriptors(
      DescriptorArray* descriptors,
      TransitionFlag flag,
      Name* name = NULL,
      SimpleTransitionFlag simple_flag = FULL_TRANSITION);
  static Handle<Map> CopyInstallDescriptors(
      Handle<Map> map,
      int new_descriptor,
      Handle<DescriptorArray> descriptors);
  MUST_USE_RESULT MaybeObject* ShareDescriptor(DescriptorArray* descriptors,
                                               Descriptor* descriptor);
  MUST_USE_RESULT MaybeObject* CopyAddDescriptor(Descriptor* descriptor,
                                                 TransitionFlag flag);
  MUST_USE_RESULT MaybeObject* CopyInsertDescriptor(Descriptor* descriptor,
                                                    TransitionFlag flag);
  MUST_USE_RESULT MaybeObject* CopyReplaceDescriptor(
      DescriptorArray* descriptors,
      Descriptor* descriptor,
      int index,
      TransitionFlag flag);
  MUST_USE_RESULT MaybeObject* AsElementsKind(ElementsKind kind);

  MUST_USE_RESULT MaybeObject* CopyAsElementsKind(ElementsKind kind,
                                                  TransitionFlag flag);

  static Handle<Map> CopyForObserved(Handle<Map> map);

  static Handle<Map> CopyNormalized(Handle<Map> map,
                                    PropertyNormalizationMode mode,
                                    NormalizedMapSharingMode sharing);

  inline void AppendDescriptor(Descriptor* desc,
                               const DescriptorArray::WhitenessWitness&);

  // Returns a copy of the map, with all transitions dropped from the
  // instance descriptors.
  static Handle<Map> Copy(Handle<Map> map);
  MUST_USE_RESULT MaybeObject* Copy();

  // Returns the next free property index (only valid for FAST MODE).
  int NextFreePropertyIndex();

  // Returns the number of properties described in instance_descriptors
  // filtering out properties with the specified attributes.
  int NumberOfDescribedProperties(DescriptorFlag which = OWN_DESCRIPTORS,
                                  PropertyAttributes filter = NONE);

  // Returns the number of slots allocated for the initial properties
  // backing storage for instances of this map.
  int InitialPropertiesLength() {
    return pre_allocated_property_fields() + unused_property_fields() -
        inobject_properties();
  }

  // Casting.
  static inline Map* cast(Object* obj);

  // Locate an accessor in the instance descriptor.
  AccessorDescriptor* FindAccessor(Name* name);

  // Code cache operations.

  // Clears the code cache.
  inline void ClearCodeCache(Heap* heap);

  // Update code cache.
  static void UpdateCodeCache(Handle<Map> map,
                              Handle<Name> name,
                              Handle<Code> code);
  MUST_USE_RESULT MaybeObject* UpdateCodeCache(Name* name, Code* code);

  // Extend the descriptor array of the map with the list of descriptors.
  // In case of duplicates, the latest descriptor is used.
  static void AppendCallbackDescriptors(Handle<Map> map,
                                        Handle<Object> descriptors);

  static void EnsureDescriptorSlack(Handle<Map> map, int slack);

  // Returns the found code or undefined if absent.
  Object* FindInCodeCache(Name* name, Code::Flags flags);

  // Returns the non-negative index of the code object if it is in the
  // cache and -1 otherwise.
  int IndexInCodeCache(Object* name, Code* code);

  // Removes a code object from the code cache at the given index.
  void RemoveFromCodeCache(Name* name, Code* code, int index);

  // Set all map transitions from this map to dead maps to null.  Also clear
  // back pointers in transition targets so that we do not process this map
  // again while following back pointers.
  void ClearNonLiveTransitions(Heap* heap);

  // Computes a hash value for this map, to be used in HashTables and such.
  int Hash();

  bool EquivalentToForTransition(Map* other);

  // Compares this map to another to see if they describe equivalent objects.
  // If |mode| is set to CLEAR_INOBJECT_PROPERTIES, |other| is treated as if
  // it had exactly zero inobject properties.
  // The "shared" flags of both this map and |other| are ignored.
  bool EquivalentToForNormalization(Map* other, PropertyNormalizationMode mode);

  // Returns the map that this map transitions to if its elements_kind
  // is changed to |elements_kind|, or NULL if no such map is cached yet.
  // |safe_to_add_transitions| is set to false if adding transitions is not
  // allowed.
  Map* LookupElementsTransitionMap(ElementsKind elements_kind);

  // Returns the transitioned map for this map with the most generic
  // elements_kind that's found in |candidates|, or null handle if no match is
  // found at all.
  Handle<Map> FindTransitionedMap(MapHandleList* candidates);
  Map* FindTransitionedMap(MapList* candidates);

  // Zaps the contents of backing data structures. Note that the
  // heap verifier (i.e. VerifyMarkingVisitor) relies on zapping of objects
  // holding weak references when incremental marking is used, because it also
  // iterates over objects that are otherwise unreachable.
  // In general we only want to call these functions in release mode when
  // heap verification is turned on.
  void ZapPrototypeTransitions();
  void ZapTransitions();

  bool CanTransition() {
    // Only JSObject and subtypes have map transitions and back pointers.
    STATIC_ASSERT(LAST_TYPE == LAST_JS_OBJECT_TYPE);
    return instance_type() >= FIRST_JS_OBJECT_TYPE;
  }

  bool IsJSObjectMap() {
    return instance_type() >= FIRST_JS_OBJECT_TYPE;
  }
  bool IsJSGlobalProxyMap() {
    return instance_type() == JS_GLOBAL_PROXY_TYPE;
  }
  bool IsJSGlobalObjectMap() {
    return instance_type() == JS_GLOBAL_OBJECT_TYPE;
  }
  bool IsGlobalObjectMap() {
    const InstanceType type = instance_type();
    return type == JS_GLOBAL_OBJECT_TYPE || type == JS_BUILTINS_OBJECT_TYPE;
  }

  // Fires when the layout of an object with a leaf map changes.
  // This includes adding transitions to the leaf map or changing
  // the descriptor array.
  inline void NotifyLeafMapLayoutChange();

  inline bool CanOmitMapChecks();

  void AddDependentCompilationInfo(DependentCode::DependencyGroup group,
                                   CompilationInfo* info);

  void AddDependentCode(DependentCode::DependencyGroup group,
                        Handle<Code> code);

  bool IsMapInArrayPrototypeChain();

  // Dispatched behavior.
  DECLARE_PRINTER(Map)
  DECLARE_VERIFIER(Map)

#ifdef VERIFY_HEAP
  void SharedMapVerify();
  void VerifyOmittedMapChecks();
#endif

  inline int visitor_id();
  inline void set_visitor_id(int visitor_id);

  typedef void (*TraverseCallback)(Map* map, void* data);

  void TraverseTransitionTree(TraverseCallback callback, void* data);

  // When you set the prototype of an object using the __proto__ accessor you
  // need a new map for the object (the prototype is stored in the map).  In
  // order not to multiply maps unnecessarily we store these as transitions in
  // the original map.  That way we can transition to the same map if the same
  // prototype is set, rather than creating a new map every time.  The
  // transitions are in the form of a map where the keys are prototype objects
  // and the values are the maps the are transitioned to.
  static const int kMaxCachedPrototypeTransitions = 256;
  static Handle<Map> GetPrototypeTransition(Handle<Map> map,
                                            Handle<Object> prototype);
  static Handle<Map> PutPrototypeTransition(Handle<Map> map,
                                            Handle<Object> prototype,
                                            Handle<Map> target_map);

  static const int kMaxPreAllocatedPropertyFields = 255;

  // Layout description.
  static const int kInstanceSizesOffset = HeapObject::kHeaderSize;
  static const int kInstanceAttributesOffset = kInstanceSizesOffset + kIntSize;
  static const int kPrototypeOffset = kInstanceAttributesOffset + kIntSize;
  static const int kConstructorOffset = kPrototypeOffset + kPointerSize;
  // Storage for the transition array is overloaded to directly contain a back
  // pointer if unused. When the map has transitions, the back pointer is
  // transferred to the transition array and accessed through an extra
  // indirection.
  static const int kTransitionsOrBackPointerOffset =
      kConstructorOffset + kPointerSize;
  static const int kDescriptorsOffset =
      kTransitionsOrBackPointerOffset + kPointerSize;
  static const int kCodeCacheOffset = kDescriptorsOffset + kPointerSize;
  static const int kDependentCodeOffset = kCodeCacheOffset + kPointerSize;
  static const int kBitField3Offset = kDependentCodeOffset + kPointerSize;
  static const int kSize = kBitField3Offset + kPointerSize;

  // Layout of pointer fields. Heap iteration code relies on them
  // being continuously allocated.
  static const int kPointerFieldsBeginOffset = Map::kPrototypeOffset;
  static const int kPointerFieldsEndOffset = kBitField3Offset + kPointerSize;

  // Byte offsets within kInstanceSizesOffset.
  static const int kInstanceSizeOffset = kInstanceSizesOffset + 0;
  static const int kInObjectPropertiesByte = 1;
  static const int kInObjectPropertiesOffset =
      kInstanceSizesOffset + kInObjectPropertiesByte;
  static const int kPreAllocatedPropertyFieldsByte = 2;
  static const int kPreAllocatedPropertyFieldsOffset =
      kInstanceSizesOffset + kPreAllocatedPropertyFieldsByte;
  static const int kVisitorIdByte = 3;
  static const int kVisitorIdOffset = kInstanceSizesOffset + kVisitorIdByte;

  // Byte offsets within kInstanceAttributesOffset attributes.
  static const int kInstanceTypeOffset = kInstanceAttributesOffset + 0;
  static const int kUnusedPropertyFieldsOffset = kInstanceAttributesOffset + 1;
  static const int kBitFieldOffset = kInstanceAttributesOffset + 2;
  static const int kBitField2Offset = kInstanceAttributesOffset + 3;

  STATIC_CHECK(kInstanceTypeOffset == Internals::kMapInstanceTypeOffset);

  // Bit positions for bit field.
  static const int kUnused = 0;  // To be used for marking recently used maps.
  static const int kHasNonInstancePrototype = 1;
  static const int kIsHiddenPrototype = 2;
  static const int kHasNamedInterceptor = 3;
  static const int kHasIndexedInterceptor = 4;
  static const int kIsUndetectable = 5;
  static const int kIsObserved = 6;
  static const int kIsAccessCheckNeeded = 7;

  // Bit positions for bit field 2
  static const int kIsExtensible = 0;
  static const int kStringWrapperSafeForDefaultValueOf = 1;
  static const int kAttachedToSharedFunctionInfo = 2;
  // No bits can be used after kElementsKindFirstBit, they are all reserved for
  // storing ElementKind.
  static const int kElementsKindShift = 3;
  static const int kElementsKindBitCount = 5;

  // Derived values from bit field 2
  static const int kElementsKindMask = (-1 << kElementsKindShift) &
      ((1 << (kElementsKindShift + kElementsKindBitCount)) - 1);
  static const int8_t kMaximumBitField2FastElementValue = static_cast<int8_t>(
      (FAST_ELEMENTS + 1) << Map::kElementsKindShift) - 1;
  static const int8_t kMaximumBitField2FastSmiElementValue =
      static_cast<int8_t>((FAST_SMI_ELEMENTS + 1) <<
                          Map::kElementsKindShift) - 1;
  static const int8_t kMaximumBitField2FastHoleyElementValue =
      static_cast<int8_t>((FAST_HOLEY_ELEMENTS + 1) <<
                          Map::kElementsKindShift) - 1;
  static const int8_t kMaximumBitField2FastHoleySmiElementValue =
      static_cast<int8_t>((FAST_HOLEY_SMI_ELEMENTS + 1) <<
                          Map::kElementsKindShift) - 1;

  typedef FixedBodyDescriptor<kPointerFieldsBeginOffset,
                              kPointerFieldsEndOffset,
                              kSize> BodyDescriptor;

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(Map);
};


// An abstract superclass, a marker class really, for simple structure classes.
// It doesn't carry much functionality but allows struct classes to be
// identified in the type system.
class Struct: public HeapObject {
 public:
  inline void InitializeBody(int object_size);
  static inline Struct* cast(Object* that);
};


// A simple one-element struct, useful where smis need to be boxed.
class Box : public Struct {
 public:
  // [value]: the boxed contents.
  DECL_ACCESSORS(value, Object)

  static inline Box* cast(Object* obj);

  // Dispatched behavior.
  DECLARE_PRINTER(Box)
  DECLARE_VERIFIER(Box)

  static const int kValueOffset = HeapObject::kHeaderSize;
  static const int kSize = kValueOffset + kPointerSize;

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(Box);
};


// Script describes a script which has been added to the VM.
class Script: public Struct {
 public:
  // Script types.
  enum Type {
    TYPE_NATIVE = 0,
    TYPE_EXTENSION = 1,
    TYPE_NORMAL = 2
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
  DECL_ACCESSORS(id, Smi)

  // [line_offset]: script line offset in resource from where it was extracted.
  DECL_ACCESSORS(line_offset, Smi)

  // [column_offset]: script column offset in resource from where it was
  // extracted.
  DECL_ACCESSORS(column_offset, Smi)

  // [data]: additional data associated with this script.
  DECL_ACCESSORS(data, Object)

  // [context_data]: context data for the context this script was compiled in.
  DECL_ACCESSORS(context_data, Object)

  // [wrapper]: the wrapper cache.
  DECL_ACCESSORS(wrapper, Foreign)

  // [type]: the script type.
  DECL_ACCESSORS(type, Smi)

  // [line_ends]: FixedArray of line ends positions.
  DECL_ACCESSORS(line_ends, Object)

  // [eval_from_shared]: for eval scripts the shared funcion info for the
  // function from which eval was called.
  DECL_ACCESSORS(eval_from_shared, Object)

  // [eval_from_instructions_offset]: the instruction offset in the code for the
  // function from which eval was called where eval was called.
  DECL_ACCESSORS(eval_from_instructions_offset, Smi)

  // [flags]: Holds an exciting bitfield.
  DECL_ACCESSORS(flags, Smi)

  // [compilation_type]: how the the script was compiled. Encoded in the
  // 'flags' field.
  inline CompilationType compilation_type();
  inline void set_compilation_type(CompilationType type);

  // [compilation_state]: determines whether the script has already been
  // compiled. Encoded in the 'flags' field.
  inline CompilationState compilation_state();
  inline void set_compilation_state(CompilationState state);

  // [is_shared_cross_origin]: An opaque boolean set by the embedder via
  // ScriptOrigin, and used by the embedder to make decisions about the
  // script's level of privilege. V8 just passes this through. Encoded in
  // the 'flags' field.
  DECL_BOOLEAN_ACCESSORS(is_shared_cross_origin)

  static inline Script* cast(Object* obj);

  // If script source is an external string, check that the underlying
  // resource is accessible. Otherwise, always return true.
  inline bool HasValidSource();

  // Dispatched behavior.
  DECLARE_PRINTER(Script)
  DECLARE_VERIFIER(Script)

  static const int kSourceOffset = HeapObject::kHeaderSize;
  static const int kNameOffset = kSourceOffset + kPointerSize;
  static const int kLineOffsetOffset = kNameOffset + kPointerSize;
  static const int kColumnOffsetOffset = kLineOffsetOffset + kPointerSize;
  static const int kDataOffset = kColumnOffsetOffset + kPointerSize;
  static const int kContextOffset = kDataOffset + kPointerSize;
  static const int kWrapperOffset = kContextOffset + kPointerSize;
  static const int kTypeOffset = kWrapperOffset + kPointerSize;
  static const int kLineEndsOffset = kTypeOffset + kPointerSize;
  static const int kIdOffset = kLineEndsOffset + kPointerSize;
  static const int kEvalFromSharedOffset = kIdOffset + kPointerSize;
  static const int kEvalFrominstructionsOffsetOffset =
      kEvalFromSharedOffset + kPointerSize;
  static const int kFlagsOffset =
      kEvalFrominstructionsOffsetOffset + kPointerSize;
  static const int kSize = kFlagsOffset + kPointerSize;

 private:
  // Bit positions in the flags field.
  static const int kCompilationTypeBit = 0;
  static const int kCompilationStateBit = 1;
  static const int kIsSharedCrossOriginBit = 2;

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
#define FUNCTIONS_WITH_ID_LIST(V)                   \
  V(Array.prototype, push, ArrayPush)               \
  V(Array.prototype, pop, ArrayPop)                 \
  V(Function.prototype, apply, FunctionApply)       \
  V(String.prototype, charCodeAt, StringCharCodeAt) \
  V(String.prototype, charAt, StringCharAt)         \
  V(String, fromCharCode, StringFromCharCode)       \
  V(Math, floor, MathFloor)                         \
  V(Math, round, MathRound)                         \
  V(Math, ceil, MathCeil)                           \
  V(Math, abs, MathAbs)                             \
  V(Math, log, MathLog)                             \
  V(Math, exp, MathExp)                             \
  V(Math, sqrt, MathSqrt)                           \
  V(Math, pow, MathPow)                             \
  V(Math, max, MathMax)                             \
  V(Math, min, MathMin)                             \
  V(Math, imul, MathImul)

enum BuiltinFunctionId {
  kArrayCode,
#define DECLARE_FUNCTION_ID(ignored1, ignore2, name)    \
  k##name,
  FUNCTIONS_WITH_ID_LIST(DECLARE_FUNCTION_ID)
#undef DECLARE_FUNCTION_ID
  // Fake id for a special case of Math.pow. Note, it continues the
  // list of math functions.
  kMathPowHalf
};


// SharedFunctionInfo describes the JSFunction information that can be
// shared by multiple instances of the function.
class SharedFunctionInfo: public HeapObject {
 public:
  // [name]: Function name.
  DECL_ACCESSORS(name, Object)

  // [code]: Function code.
  DECL_ACCESSORS(code, Code)
  inline void ReplaceCode(Code* code);

  // [optimized_code_map]: Map from native context to optimized code
  // and a shared literals array or Smi(0) if none.
  DECL_ACCESSORS(optimized_code_map, Object)

  // Returns index i of the entry with the specified context and OSR entry.
  // At position i - 1 is the context, position i the code, and i + 1 the
  // literals array.  Returns -1 when no matching entry is found.
  int SearchOptimizedCodeMap(Context* native_context, BailoutId osr_ast_id);

  // Installs optimized code from the code map on the given closure. The
  // index has to be consistent with a search result as defined above.
  FixedArray* GetLiteralsFromOptimizedCodeMap(int index);

  Code* GetCodeFromOptimizedCodeMap(int index);

  // Clear optimized code map.
  void ClearOptimizedCodeMap();

  // Removed a specific optimized code object from the optimized code map.
  void EvictFromOptimizedCodeMap(Code* optimized_code, const char* reason);

  // Trims the optimized code map after entries have been removed.
  void TrimOptimizedCodeMap(int shrink_by);

  // Add a new entry to the optimized code map.
  MUST_USE_RESULT MaybeObject* AddToOptimizedCodeMap(Context* native_context,
                                                     Code* code,
                                                     FixedArray* literals,
                                                     BailoutId osr_ast_id);
  static void AddToOptimizedCodeMap(Handle<SharedFunctionInfo> shared,
                                    Handle<Context> native_context,
                                    Handle<Code> code,
                                    Handle<FixedArray> literals,
                                    BailoutId osr_ast_id);

  // Layout description of the optimized code map.
  static const int kNextMapIndex = 0;
  static const int kEntriesStart = 1;
  static const int kContextOffset = 0;
  static const int kCachedCodeOffset = 1;
  static const int kLiteralsOffset = 2;
  static const int kOsrAstIdOffset = 3;
  static const int kEntryLength = 4;
  static const int kFirstContextSlot = FixedArray::kHeaderSize +
      (kEntriesStart + kContextOffset) * kPointerSize;
  static const int kFirstCodeSlot = FixedArray::kHeaderSize +
      (kEntriesStart + kCachedCodeOffset) * kPointerSize;
  static const int kFirstOsrAstIdSlot = FixedArray::kHeaderSize +
      (kEntriesStart + kOsrAstIdOffset) * kPointerSize;
  static const int kSecondEntryIndex = kEntryLength + kEntriesStart;
  static const int kInitialLength = kEntriesStart + kEntryLength;

  // [scope_info]: Scope info.
  DECL_ACCESSORS(scope_info, ScopeInfo)

  // [construct stub]: Code stub for constructing instances of this function.
  DECL_ACCESSORS(construct_stub, Code)

  // Returns if this function has been compiled to native code yet.
  inline bool is_compiled();

  // [length]: The function length - usually the number of declared parameters.
  // Use up to 2^30 parameters.
  inline int length();
  inline void set_length(int value);

  // [formal parameter count]: The declared number of parameters.
  inline int formal_parameter_count();
  inline void set_formal_parameter_count(int value);

  // Set the formal parameter count so the function code will be
  // called without using argument adaptor frames.
  inline void DontAdaptArguments();

  // [expected_nof_properties]: Expected number of properties for the function.
  inline int expected_nof_properties();
  inline void set_expected_nof_properties(int value);

  // Inobject slack tracking is the way to reclaim unused inobject space.
  //
  // The instance size is initially determined by adding some slack to
  // expected_nof_properties (to allow for a few extra properties added
  // after the constructor). There is no guarantee that the extra space
  // will not be wasted.
  //
  // Here is the algorithm to reclaim the unused inobject space:
  // - Detect the first constructor call for this SharedFunctionInfo.
  //   When it happens enter the "in progress" state: remember the
  //   constructor's initial_map and install a special construct stub that
  //   counts constructor calls.
  // - While the tracking is in progress create objects filled with
  //   one_pointer_filler_map instead of undefined_value. This way they can be
  //   resized quickly and safely.
  // - Once enough (kGenerousAllocationCount) objects have been created
  //   compute the 'slack' (traverse the map transition tree starting from the
  //   initial_map and find the lowest value of unused_property_fields).
  // - Traverse the transition tree again and decrease the instance size
  //   of every map. Existing objects will resize automatically (they are
  //   filled with one_pointer_filler_map). All further allocations will
  //   use the adjusted instance size.
  // - Decrease expected_nof_properties so that an allocations made from
  //   another context will use the adjusted instance size too.
  // - Exit "in progress" state by clearing the reference to the initial_map
  //   and setting the regular construct stub (generic or inline).
  //
  //  The above is the main event sequence. Some special cases are possible
  //  while the tracking is in progress:
  //
  // - GC occurs.
  //   Check if the initial_map is referenced by any live objects (except this
  //   SharedFunctionInfo). If it is, continue tracking as usual.
  //   If it is not, clear the reference and reset the tracking state. The
  //   tracking will be initiated again on the next constructor call.
  //
  // - The constructor is called from another context.
  //   Immediately complete the tracking, perform all the necessary changes
  //   to maps. This is  necessary because there is no efficient way to track
  //   multiple initial_maps.
  //   Proceed to create an object in the current context (with the adjusted
  //   size).
  //
  // - A different constructor function sharing the same SharedFunctionInfo is
  //   called in the same context. This could be another closure in the same
  //   context, or the first function could have been disposed.
  //   This is handled the same way as the previous case.
  //
  //  Important: inobject slack tracking is not attempted during the snapshot
  //  creation.

  static const int kGenerousAllocationCount = 8;

  // [construction_count]: Counter for constructor calls made during
  // the tracking phase.
  inline int construction_count();
  inline void set_construction_count(int value);

  // [initial_map]: initial map of the first function called as a constructor.
  // Saved for the duration of the tracking phase.
  // This is a weak link (GC resets it to undefined_value if no other live
  // object reference this map).
  DECL_ACCESSORS(initial_map, Object)

  // True if the initial_map is not undefined and the countdown stub is
  // installed.
  inline bool IsInobjectSlackTrackingInProgress();

  // Starts the tracking.
  // Stores the initial map and installs the countdown stub.
  // IsInobjectSlackTrackingInProgress is normally true after this call,
  // except when tracking have not been started (e.g. the map has no unused
  // properties or the snapshot is being built).
  void StartInobjectSlackTracking(Map* map);

  // Completes the tracking.
  // IsInobjectSlackTrackingInProgress is false after this call.
  void CompleteInobjectSlackTracking();

  // Invoked before pointers in SharedFunctionInfo are being marked.
  // Also clears the optimized code map.
  inline void BeforeVisitingPointers();

  // Clears the initial_map before the GC marking phase to ensure the reference
  // is weak. IsInobjectSlackTrackingInProgress is false after this call.
  void DetachInitialMap();

  // Restores the link to the initial map after the GC marking phase.
  // IsInobjectSlackTrackingInProgress is true after this call.
  void AttachInitialMap(Map* map);

  // False if there are definitely no live objects created from this function.
  // True if live objects _may_ exist (existence not guaranteed).
  // May go back from true to false after GC.
  DECL_BOOLEAN_ACCESSORS(live_objects_may_exist)

  // [instance class name]: class name for instances.
  DECL_ACCESSORS(instance_class_name, Object)

  // [function data]: This field holds some additional data for function.
  // Currently it either has FunctionTemplateInfo to make benefit the API
  // or Smi identifying a builtin function.
  // In the long run we don't want all functions to have this field but
  // we can fix that when we have a better model for storing hidden data
  // on objects.
  DECL_ACCESSORS(function_data, Object)

  inline bool IsApiFunction();
  inline FunctionTemplateInfo* get_api_func_data();
  inline bool HasBuiltinFunctionId();
  inline BuiltinFunctionId builtin_function_id();

  // [script info]: Script from which the function originates.
  DECL_ACCESSORS(script, Object)

  // [num_literals]: Number of literals used by this function.
  inline int num_literals();
  inline void set_num_literals(int value);

  // [start_position_and_type]: Field used to store both the source code
  // position, whether or not the function is a function expression,
  // and whether or not the function is a toplevel function. The two
  // least significants bit indicates whether the function is an
  // expression and the rest contains the source code position.
  inline int start_position_and_type();
  inline void set_start_position_and_type(int value);

  // [debug info]: Debug information.
  DECL_ACCESSORS(debug_info, Object)

  // [inferred name]: Name inferred from variable or property
  // assignment of this function. Used to facilitate debugging and
  // profiling of JavaScript code written in OO style, where almost
  // all functions are anonymous but are assigned to object
  // properties.
  DECL_ACCESSORS(inferred_name, String)

  // The function's name if it is non-empty, otherwise the inferred name.
  String* DebugName();

  // Position of the 'function' token in the script source.
  inline int function_token_position();
  inline void set_function_token_position(int function_token_position);

  // Position of this function in the script source.
  inline int start_position();
  inline void set_start_position(int start_position);

  // End position of this function in the script source.
  inline int end_position();
  inline void set_end_position(int end_position);

  // Is this function a function expression in the source code.
  DECL_BOOLEAN_ACCESSORS(is_expression)

  // Is this function a top-level function (scripts, evals).
  DECL_BOOLEAN_ACCESSORS(is_toplevel)

  // Bit field containing various information collected by the compiler to
  // drive optimization.
  inline int compiler_hints();
  inline void set_compiler_hints(int value);

  inline int ast_node_count();
  inline void set_ast_node_count(int count);

  inline int profiler_ticks();

  // Inline cache age is used to infer whether the function survived a context
  // disposal or not. In the former case we reset the opt_count.
  inline int ic_age();
  inline void set_ic_age(int age);

  // Indicates if this function can be lazy compiled.
  // This is used to determine if we can safely flush code from a function
  // when doing GC if we expect that the function will no longer be used.
  DECL_BOOLEAN_ACCESSORS(allows_lazy_compilation)

  // Indicates if this function can be lazy compiled without a context.
  // This is used to determine if we can force compilation without reaching
  // the function through program execution but through other means (e.g. heap
  // iteration by the debugger).
  DECL_BOOLEAN_ACCESSORS(allows_lazy_compilation_without_context)

  // Indicates whether optimizations have been disabled for this
  // shared function info. If a function is repeatedly optimized or if
  // we cannot optimize the function we disable optimization to avoid
  // spending time attempting to optimize it again.
  DECL_BOOLEAN_ACCESSORS(optimization_disabled)

  // Indicates the language mode of the function's code as defined by the
  // current harmony drafts for the next ES language standard. Possible
  // values are:
  // 1. CLASSIC_MODE - Unrestricted syntax and semantics, same as in ES5.
  // 2. STRICT_MODE - Restricted syntax and semantics, same as in ES5.
  // 3. EXTENDED_MODE - Only available under the harmony flag, not part of ES5.
  inline LanguageMode language_mode();
  inline void set_language_mode(LanguageMode language_mode);

  // Indicates whether the language mode of this function is CLASSIC_MODE.
  inline bool is_classic_mode();

  // Indicates whether the language mode of this function is EXTENDED_MODE.
  inline bool is_extended_mode();

  // False if the function definitely does not allocate an arguments object.
  DECL_BOOLEAN_ACCESSORS(uses_arguments)

  // True if the function has any duplicated parameter names.
  DECL_BOOLEAN_ACCESSORS(has_duplicate_parameters)

  // Indicates whether the function is a native function.
  // These needs special treatment in .call and .apply since
  // null passed as the receiver should not be translated to the
  // global object.
  DECL_BOOLEAN_ACCESSORS(native)

  // Indicate that this builtin needs to be inlined in crankshaft.
  DECL_BOOLEAN_ACCESSORS(inline_builtin)

  // Indicates that the function was created by the Function function.
  // Though it's anonymous, toString should treat it as if it had the name
  // "anonymous".  We don't set the name itself so that the system does not
  // see a binding for it.
  DECL_BOOLEAN_ACCESSORS(name_should_print_as_anonymous)

  // Indicates whether the function is a bound function created using
  // the bind function.
  DECL_BOOLEAN_ACCESSORS(bound)

  // Indicates that the function is anonymous (the name field can be set
  // through the API, which does not change this flag).
  DECL_BOOLEAN_ACCESSORS(is_anonymous)

  // Is this a function or top-level/eval code.
  DECL_BOOLEAN_ACCESSORS(is_function)

  // Indicates that the function cannot be optimized.
  DECL_BOOLEAN_ACCESSORS(dont_optimize)

  // Indicates that the function cannot be inlined.
  DECL_BOOLEAN_ACCESSORS(dont_inline)

  // Indicates that code for this function cannot be cached.
  DECL_BOOLEAN_ACCESSORS(dont_cache)

  // Indicates that code for this function cannot be flushed.
  DECL_BOOLEAN_ACCESSORS(dont_flush)

  // Indicates that this function is a generator.
  DECL_BOOLEAN_ACCESSORS(is_generator)

  // Indicates whether or not the code in the shared function support
  // deoptimization.
  inline bool has_deoptimization_support();

  // Enable deoptimization support through recompiled code.
  void EnableDeoptimizationSupport(Code* recompiled);

  // Disable (further) attempted optimization of all functions sharing this
  // shared function info.
  void DisableOptimization(BailoutReason reason);

  inline BailoutReason DisableOptimizationReason();

  // Lookup the bailout ID and ASSERT that it exists in the non-optimized
  // code, returns whether it asserted (i.e., always true if assertions are
  // disabled).
  bool VerifyBailoutId(BailoutId id);

  // [source code]: Source code for the function.
  bool HasSourceCode();
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
  inline int counters();

  // Stores opt_count and bailout_reason as bit-fields.
  inline void set_opt_count_and_bailout_reason(int value);
  inline int opt_count_and_bailout_reason();

  void set_bailout_reason(BailoutReason reason) {
    set_opt_count_and_bailout_reason(
        DisabledOptimizationReasonBits::update(opt_count_and_bailout_reason(),
                                               reason));
  }

  void set_dont_optimize_reason(BailoutReason reason) {
    set_bailout_reason(reason);
    set_dont_optimize(reason != kNoReason);
  }

  // Check whether or not this function is inlineable.
  bool IsInlineable();

  // Source size of this function.
  int SourceSize();

  // Calculate the instance size.
  int CalculateInstanceSize();

  // Calculate the number of in-object properties.
  int CalculateInObjectProperties();

  // Dispatched behavior.
  // Set max_length to -1 for unlimited length.
  void SourceCodePrint(StringStream* accumulator, int max_length);
  DECLARE_PRINTER(SharedFunctionInfo)
  DECLARE_VERIFIER(SharedFunctionInfo)

  void ResetForNewContext(int new_ic_age);

  // Casting.
  static inline SharedFunctionInfo* cast(Object* obj);

  // Constants.
  static const int kDontAdaptArgumentsSentinel = -1;

  // Layout description.
  // Pointer fields.
  static const int kNameOffset = HeapObject::kHeaderSize;
  static const int kCodeOffset = kNameOffset + kPointerSize;
  static const int kOptimizedCodeMapOffset = kCodeOffset + kPointerSize;
  static const int kScopeInfoOffset = kOptimizedCodeMapOffset + kPointerSize;
  static const int kConstructStubOffset = kScopeInfoOffset + kPointerSize;
  static const int kInstanceClassNameOffset =
      kConstructStubOffset + kPointerSize;
  static const int kFunctionDataOffset =
      kInstanceClassNameOffset + kPointerSize;
  static const int kScriptOffset = kFunctionDataOffset + kPointerSize;
  static const int kDebugInfoOffset = kScriptOffset + kPointerSize;
  static const int kInferredNameOffset = kDebugInfoOffset + kPointerSize;
  static const int kInitialMapOffset =
      kInferredNameOffset + kPointerSize;
  // ast_node_count is a Smi field. It could be grouped with another Smi field
  // into a PSEUDO_SMI_ACCESSORS pair (on x64), if one becomes available.
  static const int kAstNodeCountOffset =
      kInitialMapOffset + kPointerSize;
#if V8_HOST_ARCH_32_BIT
  // Smi fields.
  static const int kLengthOffset =
      kAstNodeCountOffset + kPointerSize;
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

  // Total size.
  static const int kSize = kCountersOffset + kPointerSize;
#else
  // The only reason to use smi fields instead of int fields
  // is to allow iteration without maps decoding during
  // garbage collections.
  // To avoid wasting space on 64-bit architectures we use
  // the following trick: we group integer fields into pairs
  // First integer in each pair is shifted left by 1.
  // By doing this we guarantee that LSB of each kPointerSize aligned
  // word is not set and thus this word cannot be treated as pointer
  // to HeapObject during old space traversal.
  static const int kLengthOffset =
      kAstNodeCountOffset + kPointerSize;
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

  // Total size.
  static const int kSize = kCountersOffset + kIntSize;

#endif

  // The construction counter for inobject slack tracking is stored in the
  // most significant byte of compiler_hints which is otherwise unused.
  // Its offset depends on the endian-ness of the architecture.
#if __BYTE_ORDER == __LITTLE_ENDIAN
  static const int kConstructionCountOffset = kCompilerHintsOffset + 3;
#elif __BYTE_ORDER == __BIG_ENDIAN
  static const int kConstructionCountOffset = kCompilerHintsOffset + 0;
#else
#error Unknown byte ordering
#endif

  static const int kAlignedSize = POINTER_SIZE_ALIGN(kSize);

  typedef FixedBodyDescriptor<kNameOffset,
                              kInitialMapOffset + kPointerSize,
                              kSize> BodyDescriptor;

  // Bit positions in start_position_and_type.
  // The source code start position is in the 30 most significant bits of
  // the start_position_and_type field.
  static const int kIsExpressionBit    = 0;
  static const int kIsTopLevelBit      = 1;
  static const int kStartPositionShift = 2;
  static const int kStartPositionMask  = ~((1 << kStartPositionShift) - 1);

  // Bit positions in compiler_hints.
  enum CompilerHints {
    kAllowLazyCompilation,
    kAllowLazyCompilationWithoutContext,
    kLiveObjectsMayExist,
    kOptimizationDisabled,
    kStrictModeFunction,
    kExtendedModeFunction,
    kUsesArguments,
    kHasDuplicateParameters,
    kNative,
    kInlineBuiltin,
    kBoundFunction,
    kIsAnonymous,
    kNameShouldPrintAsAnonymous,
    kIsFunction,
    kDontOptimize,
    kDontInline,
    kDontCache,
    kDontFlush,
    kIsGenerator,
    kCompilerHintsCount  // Pseudo entry
  };

  class DeoptCountBits: public BitField<int, 0, 4> {};
  class OptReenableTriesBits: public BitField<int, 4, 18> {};
  class ICAgeBits: public BitField<int, 22, 8> {};

  class OptCountBits: public BitField<int, 0, 22> {};
  class DisabledOptimizationReasonBits: public BitField<int, 22, 8> {};

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

  STATIC_ASSERT(SharedFunctionInfo::kCompilerHintsCount <=
                SharedFunctionInfo::kCompilerHintsSize * kBitsPerByte);

 public:
  // Constants for optimizing codegen for strict mode function and
  // native tests.
  // Allows to use byte-width instructions.
  static const int kStrictModeBitWithinByte =
      (kStrictModeFunction + kCompilerHintsSmiTagSize) % kBitsPerByte;

  static const int kExtendedModeBitWithinByte =
      (kExtendedModeFunction + kCompilerHintsSmiTagSize) % kBitsPerByte;

  static const int kNativeBitWithinByte =
      (kNative + kCompilerHintsSmiTagSize) % kBitsPerByte;

#if __BYTE_ORDER == __LITTLE_ENDIAN
  static const int kStrictModeByteOffset = kCompilerHintsOffset +
      (kStrictModeFunction + kCompilerHintsSmiTagSize) / kBitsPerByte;
  static const int kExtendedModeByteOffset = kCompilerHintsOffset +
      (kExtendedModeFunction + kCompilerHintsSmiTagSize) / kBitsPerByte;
  static const int kNativeByteOffset = kCompilerHintsOffset +
      (kNative + kCompilerHintsSmiTagSize) / kBitsPerByte;
#elif __BYTE_ORDER == __BIG_ENDIAN
  static const int kStrictModeByteOffset = kCompilerHintsOffset +
      (kCompilerHintsSize - 1) -
      ((kStrictModeFunction + kCompilerHintsSmiTagSize) / kBitsPerByte);
  static const int kExtendedModeByteOffset = kCompilerHintsOffset +
      (kCompilerHintsSize - 1) -
      ((kExtendedModeFunction + kCompilerHintsSmiTagSize) / kBitsPerByte);
  static const int kNativeByteOffset = kCompilerHintsOffset +
      (kCompilerHintsSize - 1) -
      ((kNative + kCompilerHintsSmiTagSize) / kBitsPerByte);
#else
#error Unknown byte ordering
#endif

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(SharedFunctionInfo);
};


class JSGeneratorObject: public JSObject {
 public:
  // [function]: The function corresponding to this generator object.
  DECL_ACCESSORS(function, JSFunction)

  // [context]: The context of the suspended computation.
  DECL_ACCESSORS(context, Context)

  // [receiver]: The receiver of the suspended computation.
  DECL_ACCESSORS(receiver, Object)

  // [continuation]: Offset into code of continuation.
  //
  // A positive offset indicates a suspended generator.  The special
  // kGeneratorExecuting and kGeneratorClosed values indicate that a generator
  // cannot be resumed.
  inline int continuation();
  inline void set_continuation(int continuation);

  // [operand_stack]: Saved operand stack.
  DECL_ACCESSORS(operand_stack, FixedArray)

  // [stack_handler_index]: Index of first stack handler in operand_stack, or -1
  // if the captured activation had no stack handler.
  inline int stack_handler_index();
  inline void set_stack_handler_index(int stack_handler_index);

  // Casting.
  static inline JSGeneratorObject* cast(Object* obj);

  // Dispatched behavior.
  DECLARE_PRINTER(JSGeneratorObject)
  DECLARE_VERIFIER(JSGeneratorObject)

  // Magic sentinel values for the continuation.
  static const int kGeneratorExecuting = -1;
  static const int kGeneratorClosed = 0;

  // Layout description.
  static const int kFunctionOffset = JSObject::kHeaderSize;
  static const int kContextOffset = kFunctionOffset + kPointerSize;
  static const int kReceiverOffset = kContextOffset + kPointerSize;
  static const int kContinuationOffset = kReceiverOffset + kPointerSize;
  static const int kOperandStackOffset = kContinuationOffset + kPointerSize;
  static const int kStackHandlerIndexOffset =
      kOperandStackOffset + kPointerSize;
  static const int kSize = kStackHandlerIndexOffset + kPointerSize;

  // Resume mode, for use by runtime functions.
  enum ResumeMode { NEXT, THROW };

  // Yielding from a generator returns an object with the following inobject
  // properties.  See Context::generator_result_map() for the map.
  static const int kResultValuePropertyIndex = 0;
  static const int kResultDonePropertyIndex = 1;
  static const int kResultPropertyCount = 2;

  static const int kResultValuePropertyOffset = JSObject::kHeaderSize;
  static const int kResultDonePropertyOffset =
      kResultValuePropertyOffset + kPointerSize;
  static const int kResultSize = kResultDonePropertyOffset + kPointerSize;

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(JSGeneratorObject);
};


// Representation for module instance objects.
class JSModule: public JSObject {
 public:
  // [context]: the context holding the module's locals, or undefined if none.
  DECL_ACCESSORS(context, Object)

  // [scope_info]: Scope info.
  DECL_ACCESSORS(scope_info, ScopeInfo)

  // Casting.
  static inline JSModule* cast(Object* obj);

  // Dispatched behavior.
  DECLARE_PRINTER(JSModule)
  DECLARE_VERIFIER(JSModule)

  // Layout description.
  static const int kContextOffset = JSObject::kHeaderSize;
  static const int kScopeInfoOffset = kContextOffset + kPointerSize;
  static const int kSize = kScopeInfoOffset + kPointerSize;

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(JSModule);
};


// JSFunction describes JavaScript functions.
class JSFunction: public JSObject {
 public:
  // [prototype_or_initial_map]:
  DECL_ACCESSORS(prototype_or_initial_map, Object)

  // [shared]: The information about the function that
  // can be shared by instances.
  DECL_ACCESSORS(shared, SharedFunctionInfo)

  // [context]: The context for this function.
  inline Context* context();
  inline void set_context(Object* context);

  // [code]: The generated code object for this function.  Executed
  // when the function is invoked, e.g. foo() or new foo(). See
  // [[Call]] and [[Construct]] description in ECMA-262, section
  // 8.6.2, page 27.
  inline Code* code();
  inline void set_code(Code* code);
  inline void set_code_no_write_barrier(Code* code);
  inline void ReplaceCode(Code* code);

  // Tells whether this function is builtin.
  inline bool IsBuiltin();

  // Tells whether or not the function needs arguments adaption.
  inline bool NeedsArgumentsAdaption();

  // Tells whether or not this function has been optimized.
  inline bool IsOptimized();

  // Tells whether or not this function can be optimized.
  inline bool IsOptimizable();

  // Mark this function for lazy recompilation. The function will be
  // recompiled the next time it is executed.
  void MarkForOptimization();
  void MarkForConcurrentOptimization();
  void MarkInOptimizationQueue();

  static bool CompileOptimized(Handle<JSFunction> function,
                               ClearExceptionFlag flag);

  // Tells whether or not the function is already marked for lazy
  // recompilation.
  inline bool IsMarkedForOptimization();
  inline bool IsMarkedForConcurrentOptimization();

  // Tells whether or not the function is on the concurrent recompilation queue.
  inline bool IsInOptimizationQueue();

  // [literals_or_bindings]: Fixed array holding either
  // the materialized literals or the bindings of a bound function.
  //
  // If the function contains object, regexp or array literals, the
  // literals array prefix contains the object, regexp, and array
  // function to be used when creating these literals.  This is
  // necessary so that we do not dynamically lookup the object, regexp
  // or array functions.  Performing a dynamic lookup, we might end up
  // using the functions from a new context that we should not have
  // access to.
  //
  // On bound functions, the array is a (copy-on-write) fixed-array containing
  // the function that was bound, bound this-value and any bound
  // arguments. Bound functions never contain literals.
  DECL_ACCESSORS(literals_or_bindings, FixedArray)

  inline FixedArray* literals();
  inline void set_literals(FixedArray* literals);

  inline FixedArray* function_bindings();
  inline void set_function_bindings(FixedArray* bindings);

  // The initial map for an object created by this constructor.
  inline Map* initial_map();
  inline void set_initial_map(Map* value);
  inline bool has_initial_map();
  static void EnsureHasInitialMap(Handle<JSFunction> function);

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
  void RemovePrototype();
  inline bool should_have_prototype();

  // Accessor for this function's initial map's [[class]]
  // property. This is primarily used by ECMA native functions.  This
  // method sets the class_name field of this function's initial map
  // to a given value. It creates an initial map if this function does
  // not have one. Note that this method does not copy the initial map
  // if it has one already, but simply replaces it with the new value.
  // Instances created afterwards will have a map whose [[class]] is
  // set to 'value', but there is no guarantees on instances created
  // before.
  void SetInstanceClassName(String* name);

  // Returns if this function has been compiled to native code yet.
  inline bool is_compiled();

  // [next_function_link]: Links functions into various lists, e.g. the list
  // of optimized functions hanging off the native_context. The CodeFlusher
  // uses this link to chain together flushing candidates. Treated weakly
  // by the garbage collector.
  DECL_ACCESSORS(next_function_link, Object)

  // Prints the name of the function using PrintF.
  void PrintName(FILE* out = stdout);

  // Casting.
  static inline JSFunction* cast(Object* obj);

  // Iterates the objects, including code objects indirectly referenced
  // through pointers to the first instruction in the code object.
  void JSFunctionIterateBody(int object_size, ObjectVisitor* v);

  // Dispatched behavior.
  DECLARE_PRINTER(JSFunction)
  DECLARE_VERIFIER(JSFunction)

  // Returns the number of allocated literals.
  inline int NumberOfLiterals();

  // Retrieve the native context from a function's literal array.
  static Context* NativeContextFromLiterals(FixedArray* literals);

  // Used for flags such as --hydrogen-filter.
  bool PassesFilter(const char* raw_filter);

  // Layout descriptors. The last property (from kNonWeakFieldsEndOffset to
  // kSize) is weak and has special handling during garbage collection.
  static const int kCodeEntryOffset = JSObject::kHeaderSize;
  static const int kPrototypeOrInitialMapOffset =
      kCodeEntryOffset + kPointerSize;
  static const int kSharedFunctionInfoOffset =
      kPrototypeOrInitialMapOffset + kPointerSize;
  static const int kContextOffset = kSharedFunctionInfoOffset + kPointerSize;
  static const int kLiteralsOffset = kContextOffset + kPointerSize;
  static const int kNonWeakFieldsEndOffset = kLiteralsOffset + kPointerSize;
  static const int kNextFunctionLinkOffset = kNonWeakFieldsEndOffset;
  static const int kSize = kNextFunctionLinkOffset + kPointerSize;

  // Layout of the literals array.
  static const int kLiteralsPrefixSize = 1;
  static const int kLiteralNativeContextIndex = 0;

  // Layout of the bound-function binding array.
  static const int kBoundFunctionIndex = 0;
  static const int kBoundThisIndex = 1;
  static const int kBoundArgumentsStartIndex = 2;

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

  // Casting.
  static inline JSGlobalProxy* cast(Object* obj);

  inline bool IsDetachedFrom(GlobalObject* global);

  // Dispatched behavior.
  DECLARE_PRINTER(JSGlobalProxy)
  DECLARE_VERIFIER(JSGlobalProxy)

  // Layout description.
  static const int kNativeContextOffset = JSObject::kHeaderSize;
  static const int kSize = kNativeContextOffset + kPointerSize;

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(JSGlobalProxy);
};


// Forward declaration.
class JSBuiltinsObject;

// Common super class for JavaScript global objects and the special
// builtins global objects.
class GlobalObject: public JSObject {
 public:
  // [builtins]: the object holding the runtime routines written in JS.
  DECL_ACCESSORS(builtins, JSBuiltinsObject)

  // [native context]: the natives corresponding to this global object.
  DECL_ACCESSORS(native_context, Context)

  // [global context]: the most recent (i.e. innermost) global context.
  DECL_ACCESSORS(global_context, Context)

  // [global receiver]: the global receiver object of the context
  DECL_ACCESSORS(global_receiver, JSObject)

  // Retrieve the property cell used to store a property.
  PropertyCell* GetPropertyCell(LookupResult* result);

  // This is like GetProperty, but is used when you know the lookup won't fail
  // by throwing an exception.  This is for the debug and builtins global
  // objects, where it is known which properties can be expected to be present
  // on the object.
  Object* GetPropertyNoExceptionThrown(Name* key) {
    Object* answer = GetProperty(key)->ToObjectUnchecked();
    return answer;
  }

  // Casting.
  static inline GlobalObject* cast(Object* obj);

  // Layout description.
  static const int kBuiltinsOffset = JSObject::kHeaderSize;
  static const int kNativeContextOffset = kBuiltinsOffset + kPointerSize;
  static const int kGlobalContextOffset = kNativeContextOffset + kPointerSize;
  static const int kGlobalReceiverOffset = kGlobalContextOffset + kPointerSize;
  static const int kHeaderSize = kGlobalReceiverOffset + kPointerSize;

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(GlobalObject);
};


// JavaScript global object.
class JSGlobalObject: public GlobalObject {
 public:
  // Casting.
  static inline JSGlobalObject* cast(Object* obj);

  // Ensure that the global object has a cell for the given property name.
  static Handle<PropertyCell> EnsurePropertyCell(Handle<JSGlobalObject> global,
                                                 Handle<Name> name);

  inline bool IsDetached();

  // Dispatched behavior.
  DECLARE_PRINTER(JSGlobalObject)
  DECLARE_VERIFIER(JSGlobalObject)

  // Layout description.
  static const int kSize = GlobalObject::kHeaderSize;

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(JSGlobalObject);
};


// Builtins global object which holds the runtime routines written in
// JavaScript.
class JSBuiltinsObject: public GlobalObject {
 public:
  // Accessors for the runtime routines written in JavaScript.
  inline Object* javascript_builtin(Builtins::JavaScript id);
  inline void set_javascript_builtin(Builtins::JavaScript id, Object* value);

  // Accessors for code of the runtime routines written in JavaScript.
  inline Code* javascript_builtin_code(Builtins::JavaScript id);
  inline void set_javascript_builtin_code(Builtins::JavaScript id, Code* value);

  // Casting.
  static inline JSBuiltinsObject* cast(Object* obj);

  // Dispatched behavior.
  DECLARE_PRINTER(JSBuiltinsObject)
  DECLARE_VERIFIER(JSBuiltinsObject)

  // Layout description.  The size of the builtins object includes
  // room for two pointers per runtime routine written in javascript
  // (function and code object).
  static const int kJSBuiltinsCount = Builtins::id_count;
  static const int kJSBuiltinsOffset = GlobalObject::kHeaderSize;
  static const int kJSBuiltinsCodeOffset =
      GlobalObject::kHeaderSize + (kJSBuiltinsCount * kPointerSize);
  static const int kSize =
      kJSBuiltinsCodeOffset + (kJSBuiltinsCount * kPointerSize);

  static int OffsetOfFunctionWithId(Builtins::JavaScript id) {
    return kJSBuiltinsOffset + id * kPointerSize;
  }

  static int OffsetOfCodeWithId(Builtins::JavaScript id) {
    return kJSBuiltinsCodeOffset + id * kPointerSize;
  }

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(JSBuiltinsObject);
};


// Representation for JS Wrapper objects, String, Number, Boolean, etc.
class JSValue: public JSObject {
 public:
  // [value]: the object being wrapped.
  DECL_ACCESSORS(value, Object)

  // Casting.
  static inline JSValue* cast(Object* obj);

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
  // moment when local fields were cached.
  DECL_ACCESSORS(cache_stamp, Object)

  // Casting.
  static inline JSDate* cast(Object* obj);

  // Returns the date field with the specified index.
  // See FieldIndex for the list of date fields.
  static Object* GetField(Object* date, Smi* index);

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
  inline void SetLocalFields(int64_t local_time_ms, DateCache* date_cache);


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
  DECL_ACCESSORS(type, String)

  // [arguments]: the arguments for formatting the error message.
  DECL_ACCESSORS(arguments, JSArray)

  // [script]: the script from which the error message originated.
  DECL_ACCESSORS(script, Object)

  // [stack_frames]: an array of stack frames for this error object.
  DECL_ACCESSORS(stack_frames, Object)

  // [start_position]: the start position in the script for the error message.
  inline int start_position();
  inline void set_start_position(int value);

  // [end_position]: the end position in the script for the error message.
  inline int end_position();
  inline void set_end_position(int value);

  // Casting.
  static inline JSMessageObject* cast(Object* obj);

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
  static const int kSize = kEndPositionOffset + kPointerSize;

  typedef FixedBodyDescriptor<HeapObject::kMapOffset,
                              kStackFramesOffset + kPointerSize,
                              kSize> BodyDescriptor;
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
// - a reference to code for ASCII inputs (bytecode or compiled), or a smi
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
  // IRREGEXP_NATIVE: Compiled to native code with Irregexp.
  enum Type { NOT_COMPILED, ATOM, IRREGEXP };
  enum Flag { NONE = 0, GLOBAL = 1, IGNORE_CASE = 2, MULTILINE = 4 };

  class Flags {
   public:
    explicit Flags(uint32_t value) : value_(value) { }
    bool is_global() { return (value_ & GLOBAL) != 0; }
    bool is_ignore_case() { return (value_ & IGNORE_CASE) != 0; }
    bool is_multiline() { return (value_ & MULTILINE) != 0; }
    uint32_t value() { return value_; }
   private:
    uint32_t value_;
  };

  DECL_ACCESSORS(data, Object)

  inline Type TypeTag();
  inline int CaptureCount();
  inline Flags GetFlags();
  inline String* Pattern();
  inline Object* DataAt(int index);
  // Set implementation data after the object has been prepared.
  inline void SetDataAt(int index, Object* value);

  static int code_index(bool is_ascii) {
    if (is_ascii) {
      return kIrregexpASCIICodeIndex;
    } else {
      return kIrregexpUC16CodeIndex;
    }
  }

  static int saved_code_index(bool is_ascii) {
    if (is_ascii) {
      return kIrregexpASCIICodeSavedIndex;
    } else {
      return kIrregexpUC16CodeSavedIndex;
    }
  }

  static inline JSRegExp* cast(Object* obj);

  // Dispatched behavior.
  DECLARE_VERIFIER(JSRegExp)

  static const int kDataOffset = JSObject::kHeaderSize;
  static const int kSize = kDataOffset + kPointerSize;

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

  // Irregexp compiled code or bytecode for ASCII. If compilation
  // fails, this fields hold an exception object that should be
  // thrown if the regexp is used again.
  static const int kIrregexpASCIICodeIndex = kDataIndex;
  // Irregexp compiled code or bytecode for UC16.  If compilation
  // fails, this fields hold an exception object that should be
  // thrown if the regexp is used again.
  static const int kIrregexpUC16CodeIndex = kDataIndex + 1;

  // Saved instance of Irregexp compiled code or bytecode for ASCII that
  // is a potential candidate for flushing.
  static const int kIrregexpASCIICodeSavedIndex = kDataIndex + 2;
  // Saved instance of Irregexp compiled code or bytecode for UC16 that is
  // a potential candidate for flushing.
  static const int kIrregexpUC16CodeSavedIndex = kDataIndex + 3;

  // Maximal number of registers used by either ASCII or UC16.
  // Only used to check that there is enough stack space
  static const int kIrregexpMaxRegisterCountIndex = kDataIndex + 4;
  // Number of captures in the compiled regexp.
  static const int kIrregexpCaptureCountIndex = kDataIndex + 5;

  static const int kIrregexpDataSize = kIrregexpCaptureCountIndex + 1;

  // Offsets directly into the data fixed array.
  static const int kDataTagOffset =
      FixedArray::kHeaderSize + kTagIndex * kPointerSize;
  static const int kDataAsciiCodeOffset =
      FixedArray::kHeaderSize + kIrregexpASCIICodeIndex * kPointerSize;
  static const int kDataUC16CodeOffset =
      FixedArray::kHeaderSize + kIrregexpUC16CodeIndex * kPointerSize;
  static const int kIrregexpCaptureCountOffset =
      FixedArray::kHeaderSize + kIrregexpCaptureCountIndex * kPointerSize;

  // In-object fields.
  static const int kSourceFieldIndex = 0;
  static const int kGlobalFieldIndex = 1;
  static const int kIgnoreCaseFieldIndex = 2;
  static const int kMultilineFieldIndex = 3;
  static const int kLastIndexFieldIndex = 4;
  static const int kInObjectFieldCount = 5;

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

  MUST_USE_RESULT static MaybeObject* AsObject(Heap* heap,
                                               HashTableKey* key) {
    return key->AsObject(heap);
  }

  static const int kPrefixSize = 0;
  static const int kEntrySize = 2;
};


class CompilationCacheTable: public HashTable<CompilationCacheShape,
                                              HashTableKey*> {
 public:
  // Find cached value for a string key, otherwise return null.
  Object* Lookup(String* src, Context* context);
  Object* LookupEval(String* src,
                     Context* context,
                     LanguageMode language_mode,
                     int scope_position);
  Object* LookupRegExp(String* source, JSRegExp::Flags flags);
  MUST_USE_RESULT MaybeObject* Put(String* src,
                                   Context* context,
                                   Object* value);
  MUST_USE_RESULT MaybeObject* PutEval(String* src,
                                       Context* context,
                                       SharedFunctionInfo* value,
                                       int scope_position);
  MUST_USE_RESULT MaybeObject* PutRegExp(String* src,
                                         JSRegExp::Flags flags,
                                         FixedArray* value);

  // Remove given value from cache.
  void Remove(Object* value);

  static inline CompilationCacheTable* cast(Object* obj);

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(CompilationCacheTable);
};


class CodeCache: public Struct {
 public:
  DECL_ACCESSORS(default_cache, FixedArray)
  DECL_ACCESSORS(normal_type_cache, Object)

  // Add the code object to the cache.
  MUST_USE_RESULT MaybeObject* Update(Name* name, Code* code);

  // Lookup code object in the cache. Returns code object if found and undefined
  // if not.
  Object* Lookup(Name* name, Code::Flags flags);

  // Get the internal index of a code object in the cache. Returns -1 if the
  // code object is not in that cache. This index can be used to later call
  // RemoveByIndex. The cache cannot be modified between a call to GetIndex and
  // RemoveByIndex.
  int GetIndex(Object* name, Code* code);

  // Remove an object from the cache with the provided internal index.
  void RemoveByIndex(Object* name, Code* code, int index);

  static inline CodeCache* cast(Object* obj);

  // Dispatched behavior.
  DECLARE_PRINTER(CodeCache)
  DECLARE_VERIFIER(CodeCache)

  static const int kDefaultCacheOffset = HeapObject::kHeaderSize;
  static const int kNormalTypeCacheOffset =
      kDefaultCacheOffset + kPointerSize;
  static const int kSize = kNormalTypeCacheOffset + kPointerSize;

 private:
  MUST_USE_RESULT MaybeObject* UpdateDefaultCache(Name* name, Code* code);
  MUST_USE_RESULT MaybeObject* UpdateNormalTypeCache(Name* name, Code* code);
  Object* LookupDefaultCache(Name* name, Code::Flags flags);
  Object* LookupNormalTypeCache(Name* name, Code::Flags flags);

  // Code cache layout of the default cache. Elements are alternating name and
  // code objects for non normal load/store/call IC's.
  static const int kCodeCacheEntrySize = 2;
  static const int kCodeCacheEntryNameOffset = 0;
  static const int kCodeCacheEntryCodeOffset = 1;

  DISALLOW_IMPLICIT_CONSTRUCTORS(CodeCache);
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

  MUST_USE_RESULT static MaybeObject* AsObject(Heap* heap,
                                               HashTableKey* key) {
    return key->AsObject(heap);
  }

  static const int kPrefixSize = 0;
  static const int kEntrySize = 2;
};


class CodeCacheHashTable: public HashTable<CodeCacheHashTableShape,
                                           HashTableKey*> {
 public:
  Object* Lookup(Name* name, Code::Flags flags);
  MUST_USE_RESULT MaybeObject* Put(Name* name, Code* code);

  int GetIndex(Name* name, Code::Flags flags);
  void RemoveByIndex(int index);

  static inline CodeCacheHashTable* cast(Object* obj);

  // Initial size of the fixed array backing the hash table.
  static const int kInitialSize = 64;

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(CodeCacheHashTable);
};


class PolymorphicCodeCache: public Struct {
 public:
  DECL_ACCESSORS(cache, Object)

  static void Update(Handle<PolymorphicCodeCache> cache,
                     MapHandleList* maps,
                     Code::Flags flags,
                     Handle<Code> code);

  MUST_USE_RESULT MaybeObject* Update(MapHandleList* maps,
                                      Code::Flags flags,
                                      Code* code);

  // Returns an undefined value if the entry is not found.
  Handle<Object> Lookup(MapHandleList* maps, Code::Flags flags);

  static inline PolymorphicCodeCache* cast(Object* obj);

  // Dispatched behavior.
  DECLARE_PRINTER(PolymorphicCodeCache)
  DECLARE_VERIFIER(PolymorphicCodeCache)

  static const int kCacheOffset = HeapObject::kHeaderSize;
  static const int kSize = kCacheOffset + kPointerSize;

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(PolymorphicCodeCache);
};


class PolymorphicCodeCacheHashTable
    : public HashTable<CodeCacheHashTableShape, HashTableKey*> {
 public:
  Object* Lookup(MapHandleList* maps, int code_kind);

  MUST_USE_RESULT MaybeObject* Put(MapHandleList* maps,
                                   int code_kind,
                                   Code* code);

  static inline PolymorphicCodeCacheHashTable* cast(Object* obj);

  static const int kInitialSize = 64;
 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(PolymorphicCodeCacheHashTable);
};


class TypeFeedbackInfo: public Struct {
 public:
  inline int ic_total_count();
  inline void set_ic_total_count(int count);

  inline int ic_with_type_info_count();
  inline void change_ic_with_type_info_count(int count);

  inline void initialize_storage();

  inline void change_own_type_change_checksum();
  inline int own_type_change_checksum();

  inline void set_inlined_type_change_checksum(int checksum);
  inline bool matches_inlined_type_change_checksum(int checksum);

  DECL_ACCESSORS(feedback_vector, FixedArray)

  static inline TypeFeedbackInfo* cast(Object* obj);

  // Dispatched behavior.
  DECLARE_PRINTER(TypeFeedbackInfo)
  DECLARE_VERIFIER(TypeFeedbackInfo)

  static const int kStorage1Offset = HeapObject::kHeaderSize;
  static const int kStorage2Offset = kStorage1Offset + kPointerSize;
  static const int kFeedbackVectorOffset =
      kStorage2Offset + kPointerSize;
  static const int kSize = kFeedbackVectorOffset + kPointerSize;

  // The object that indicates an uninitialized cache.
  static inline Handle<Object> UninitializedSentinel(Isolate* isolate);

  // The object that indicates a cache in pre-monomorphic state.
  static inline Handle<Object> PremonomorphicSentinel(Isolate* isolate);

  // The object that indicates a megamorphic state.
  static inline Handle<Object> MegamorphicSentinel(Isolate* isolate);

  // The object that indicates a monomorphic state of Array with
  // ElementsKind
  static inline Handle<Object> MonomorphicArraySentinel(Isolate* isolate,
      ElementsKind elements_kind);

  // A raw version of the uninitialized sentinel that's safe to read during
  // garbage collection (e.g., for patching the cache).
  static inline Object* RawUninitializedSentinel(Heap* heap);

  static const int kForInFastCaseMarker = 0;
  static const int kForInSlowCaseMarker = 1;

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


enum AllocationSiteMode {
  DONT_TRACK_ALLOCATION_SITE,
  TRACK_ALLOCATION_SITE,
  LAST_ALLOCATION_SITE_MODE = TRACK_ALLOCATION_SITE
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
    kTenure = 2,
    kZombie = 3,
    kLastPretenureDecisionValue = kZombie
  };

  DECL_ACCESSORS(transition_info, Object)
  // nested_site threads a list of sites that represent nested literals
  // walked in a particular order. So [[1, 2], 1, 2] will have one
  // nested_site, but [[1, 2], 3, [4]] will have a list of two.
  DECL_ACCESSORS(nested_site, Object)
  DECL_ACCESSORS(pretenure_data, Smi)
  DECL_ACCESSORS(pretenure_create_count, Smi)
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
  class MementoFoundCountBits:  public BitField<int,               0, 27> {};
  class PretenureDecisionBits:  public BitField<PretenureDecision, 27, 2> {};
  class DeoptDependentCodeBit:  public BitField<bool,              29, 1> {};
  STATIC_ASSERT(PretenureDecisionBits::kMax >= kLastPretenureDecisionValue);

  // Increments the mementos found counter and returns true when the first
  // memento was found for a given allocation site.
  inline bool IncrementMementoFoundCount();

  inline void IncrementMementoCreateCount();

  PretenureFlag GetPretenureMode();

  void ResetPretenureDecision();

  PretenureDecision pretenure_decision() {
    int value = pretenure_data()->value();
    return PretenureDecisionBits::decode(value);
  }

  void set_pretenure_decision(PretenureDecision decision) {
    int value = pretenure_data()->value();
    set_pretenure_data(
        Smi::FromInt(PretenureDecisionBits::update(value, decision)),
        SKIP_WRITE_BARRIER);
  }

  bool deopt_dependent_code() {
    int value = pretenure_data()->value();
    return DeoptDependentCodeBit::decode(value);
  }

  void set_deopt_dependent_code(bool deopt) {
    int value = pretenure_data()->value();
    set_pretenure_data(
        Smi::FromInt(DeoptDependentCodeBit::update(value, deopt)),
        SKIP_WRITE_BARRIER);
  }

  int memento_found_count() {
    int value = pretenure_data()->value();
    return MementoFoundCountBits::decode(value);
  }

  inline void set_memento_found_count(int count);

  int memento_create_count() {
    return pretenure_create_count()->value();
  }

  void set_memento_create_count(int count) {
    set_pretenure_create_count(Smi::FromInt(count), SKIP_WRITE_BARRIER);
  }

  // The pretenuring decision is made during gc, and the zombie state allows
  // us to recognize when an allocation site is just being kept alive because
  // a later traversal of new space may discover AllocationMementos that point
  // to this AllocationSite.
  bool IsZombie() {
    return pretenure_decision() == kZombie;
  }

  inline void MarkZombie();

  inline bool DigestPretenuringFeedback();

  ElementsKind GetElementsKind() {
    ASSERT(!SitePointsToLiteral());
    int value = Smi::cast(transition_info())->value();
    return ElementsKindBits::decode(value);
  }

  void SetElementsKind(ElementsKind kind) {
    int value = Smi::cast(transition_info())->value();
    set_transition_info(Smi::FromInt(ElementsKindBits::update(value, kind)),
                        SKIP_WRITE_BARRIER);
  }

  bool CanInlineCall() {
    int value = Smi::cast(transition_info())->value();
    return DoNotInlineBit::decode(value) == 0;
  }

  void SetDoNotInlineCall() {
    int value = Smi::cast(transition_info())->value();
    set_transition_info(Smi::FromInt(DoNotInlineBit::update(value, true)),
                        SKIP_WRITE_BARRIER);
  }

  bool SitePointsToLiteral() {
    // If transition_info is a smi, then it represents an ElementsKind
    // for a constructed array. Otherwise, it must be a boilerplate
    // for an object or array literal.
    return transition_info()->IsJSArray() || transition_info()->IsJSObject();
  }

  MaybeObject* DigestTransitionFeedback(ElementsKind to_kind);

  enum Reason {
    TENURING,
    TRANSITIONS
  };

  static void AddDependentCompilationInfo(Handle<AllocationSite> site,
                                          Reason reason,
                                          CompilationInfo* info);

  DECLARE_PRINTER(AllocationSite)
  DECLARE_VERIFIER(AllocationSite)

  static inline AllocationSite* cast(Object* obj);
  static inline AllocationSiteMode GetMode(
      ElementsKind boilerplate_elements_kind);
  static inline AllocationSiteMode GetMode(ElementsKind from, ElementsKind to);
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
  static const int kPointerFieldsEndOffset = kDependentCodeOffset;

  // For other visitors, use the fixed body descriptor below.
  typedef FixedBodyDescriptor<HeapObject::kHeaderSize,
                              kDependentCodeOffset + kPointerSize,
                              kSize> BodyDescriptor;

 private:
  inline DependentCode::DependencyGroup ToDependencyGroup(Reason reason);
  bool PretenuringDecisionMade() {
    return pretenure_decision() != kUndecided;
  }

  DISALLOW_IMPLICIT_CONSTRUCTORS(AllocationSite);
};


class AllocationMemento: public Struct {
 public:
  static const int kAllocationSiteOffset = HeapObject::kHeaderSize;
  static const int kSize = kAllocationSiteOffset + kPointerSize;

  DECL_ACCESSORS(allocation_site, Object)

  bool IsValid() {
    return allocation_site()->IsAllocationSite() &&
        !AllocationSite::cast(allocation_site())->IsZombie();
  }
  AllocationSite* GetAllocationSite() {
    ASSERT(IsValid());
    return AllocationSite::cast(allocation_site());
  }

  DECLARE_PRINTER(AllocationMemento)
  DECLARE_VERIFIER(AllocationMemento)

  static inline AllocationMemento* cast(Object* obj);

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(AllocationMemento);
};


// Representation of a slow alias as part of a non-strict arguments objects.
// For fast aliases (if HasNonStrictArgumentsElements()):
// - the parameter map contains an index into the context
// - all attributes of the element have default values
// For slow aliases (if HasDictionaryArgumentsElements()):
// - the parameter map contains no fast alias mapping (i.e. the hole)
// - this struct (in the slow backing store) contains an index into the context
// - all attributes are available as part if the property details
class AliasedArgumentsEntry: public Struct {
 public:
  inline int aliased_context_slot();
  inline void set_aliased_context_slot(int count);

  static inline AliasedArgumentsEntry* cast(Object* obj);

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


class StringHasher {
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
  inline explicit StringShape(String* s);
  inline explicit StringShape(Map* s);
  inline explicit StringShape(InstanceType t);
  inline bool IsSequential();
  inline bool IsExternal();
  inline bool IsCons();
  inline bool IsSliced();
  inline bool IsIndirect();
  inline bool IsExternalAscii();
  inline bool IsExternalTwoByte();
  inline bool IsSequentialAscii();
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

  // Conversion.
  inline bool AsArrayIndex(uint32_t* index);

  // Casting.
  static inline Name* cast(Object* obj);

  bool IsCacheable(Isolate* isolate);

  DECLARE_PRINTER(Name)

  // Layout description.
  static const int kHashFieldOffset = HeapObject::kHeaderSize;
  static const int kSize = kHashFieldOffset + kPointerSize;

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

  // For strings which are array indexes the hash value has the string length
  // mixed into the hash, mainly to avoid a hash value of zero which would be
  // the case for the string '0'. 24 bits are used for the array index value.
  static const int kArrayIndexValueBits = 24;
  static const int kArrayIndexLengthBits =
      kBitsPerInt - kArrayIndexValueBits - kNofHashBitFields;

  STATIC_CHECK((kArrayIndexLengthBits > 0));

  static const int kArrayIndexHashLengthShift =
      kArrayIndexValueBits + kNofHashBitFields;

  static const int kArrayIndexHashMask = (1 << kArrayIndexHashLengthShift) - 1;

  static const int kArrayIndexValueMask =
      ((1 << kArrayIndexValueBits) - 1) << kHashShift;

  // Check that kMaxCachedArrayIndexLength + 1 is a power of two so we
  // could use a mask to test if the length of string is less than or equal to
  // kMaxCachedArrayIndexLength.
  STATIC_CHECK(IS_POWER_OF_TWO(kMaxCachedArrayIndexLength + 1));

  static const unsigned int kContainsCachedArrayIndexMask =
      (~kMaxCachedArrayIndexLength << kArrayIndexHashLengthShift) |
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
  // [name]: the print name of a symbol, or undefined if none.
  DECL_ACCESSORS(name, Object)

  DECL_ACCESSORS(flags, Smi)

  // [is_private]: whether this is a private symbol.
  DECL_BOOLEAN_ACCESSORS(is_private)

  // Casting.
  static inline Symbol* cast(Object* obj);

  // Dispatched behavior.
  DECLARE_PRINTER(Symbol)
  DECLARE_VERIFIER(Symbol)

  // Layout description.
  static const int kNameOffset = Name::kSize;
  static const int kFlagsOffset = kNameOffset + kPointerSize;
  static const int kSize = kFlagsOffset + kPointerSize;

  typedef FixedBodyDescriptor<kNameOffset, kFlagsOffset, kSize> BodyDescriptor;

 private:
  static const int kPrivateBit = 0;

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

  // Representation of the flat content of a String.
  // A non-flat string doesn't have flat content.
  // A flat string has content that's encoded as a sequence of either
  // ASCII chars or two-byte UC16.
  // Returned by String::GetFlatContent().
  class FlatContent {
   public:
    // Returns true if the string is flat and this structure contains content.
    bool IsFlat() { return state_ != NON_FLAT; }
    // Returns true if the structure contains ASCII content.
    bool IsAscii() { return state_ == ASCII; }
    // Returns true if the structure contains two-byte content.
    bool IsTwoByte() { return state_ == TWO_BYTE; }

    // Return the one byte content of the string. Only use if IsAscii() returns
    // true.
    Vector<const uint8_t> ToOneByteVector() {
      ASSERT_EQ(ASCII, state_);
      return buffer_;
    }
    // Return the two-byte content of the string. Only use if IsTwoByte()
    // returns true.
    Vector<const uc16> ToUC16Vector() {
      ASSERT_EQ(TWO_BYTE, state_);
      return Vector<const uc16>::cast(buffer_);
    }

   private:
    enum State { NON_FLAT, ASCII, TWO_BYTE };

    // Constructors only used by String::GetFlatContent().
    explicit FlatContent(Vector<const uint8_t> chars)
        : buffer_(chars),
          state_(ASCII) { }
    explicit FlatContent(Vector<const uc16> chars)
        : buffer_(Vector<const byte>::cast(chars)),
          state_(TWO_BYTE) { }
    FlatContent() : buffer_(), state_(NON_FLAT) { }

    Vector<const uint8_t> buffer_;
    State state_;

    friend class String;
  };

  // Get and set the length of the string.
  inline int length();
  inline void set_length(int value);

  // Returns whether this string has only ASCII chars, i.e. all of them can
  // be ASCII encoded.  This might be the case even if the string is
  // two-byte.  Such strings may appear when the embedder prefers
  // two-byte external representations even for ASCII data.
  inline bool IsOneByteRepresentation();
  inline bool IsTwoByteRepresentation();

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

  // Try to flatten the string.  Checks first inline to see if it is
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
  //
  // Use FlattenString from Handles.cc to flatten even in case an
  // allocation failure happens.
  inline MaybeObject* TryFlatten(PretenureFlag pretenure = NOT_TENURED);

  // Convenience function.  Has exactly the same behavior as
  // TryFlatten(), except in the case of failure returns the original
  // string.
  inline String* TryFlattenGetString(PretenureFlag pretenure = NOT_TENURED);

  // Tries to return the content of a flat string as a structure holding either
  // a flat vector of char or of uc16.
  // If the string isn't flat, and therefore doesn't have flat content, the
  // returned structure will report so, and can't provide a vector of either
  // kind.
  FlatContent GetFlatContent();

  // Returns the parent of a sliced string or first part of a flat cons string.
  // Requires: StringShape(this).IsIndirect() && this->IsFlat()
  inline String* GetUnderlying();

  // Mark the string as an undetectable object. It only applies to
  // ASCII and two byte string types.
  bool MarkAsUndetectable();

  // String equality operations.
  inline bool Equals(String* other);
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
  SmartArrayPointer<char> ToCString(AllowNullsFlag allow_nulls,
                                    RobustnessFlag robustness_flag,
                                    int offset,
                                    int length,
                                    int* length_output = 0);
  SmartArrayPointer<char> ToCString(
      AllowNullsFlag allow_nulls = DISALLOW_NULLS,
      RobustnessFlag robustness_flag = FAST_STRING_TRAVERSAL,
      int* length_output = 0);

  // Return a 16 bit Unicode representation of the string.
  // The string should be nearly flat, otherwise the performance of
  // of this method may be very bad.  Setting robustness_flag to
  // ROBUST_STRING_TRAVERSAL invokes behaviour that is robust  This means it
  // handles unexpected data without causing assert failures and it does not
  // do any heap allocations.  This is useful when printing stack traces.
  SmartArrayPointer<uc16> ToWideCString(
      RobustnessFlag robustness_flag = FAST_STRING_TRAVERSAL);

  bool ComputeArrayIndex(uint32_t* index);

  // Externalization.
  bool MakeExternal(v8::String::ExternalStringResource* resource);
  bool MakeExternal(v8::String::ExternalAsciiStringResource* resource);

  // Conversion.
  inline bool AsArrayIndex(uint32_t* index);

  // Casting.
  static inline String* cast(Object* obj);

  void PrintOn(FILE* out);

  // For use during stack traces.  Performs rudimentary sanity check.
  bool LooksValid();

  // Dispatched behavior.
  void StringShortPrint(StringStream* accumulator);
#ifdef OBJECT_PRINT
  char* ToAsciiArray();
#endif
  DECLARE_PRINTER(String)
  DECLARE_VERIFIER(String)

  inline bool IsFlat();

  // Layout description.
  static const int kLengthOffset = Name::kSize;
  static const int kSize = kLengthOffset + kPointerSize;

  // Maximum number of characters to consider when trying to convert a string
  // value into an array index.
  static const int kMaxArrayIndexSize = 10;
  STATIC_CHECK(kMaxArrayIndexSize < (1 << kArrayIndexLengthBits));

  // Max char codes.
  static const int32_t kMaxOneByteCharCode = unibrow::Latin1::kMaxChar;
  static const uint32_t kMaxOneByteCharCodeU = unibrow::Latin1::kMaxChar;
  static const int kMaxUtf16CodeUnit = 0xffff;

  // Value of hash field containing computed hash equal to zero.
  static const int kEmptyStringHash = kIsNotArrayIndexMask;

  // Maximal string length.
  static const int kMaxLength = (1 << (32 - 2)) - 1;

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

  // The return value may point to the first aligned word containing the
  // first non-ascii character, rather than directly to the non-ascii character.
  // If the return value is >= the passed length, the entire string was ASCII.
  static inline int NonAsciiStart(const char* chars, int length) {
    const char* start = chars;
    const char* limit = chars + length;
#ifdef V8_HOST_CAN_READ_UNALIGNED
    ASSERT(unibrow::Utf8::kMaxOneByteChar == 0x7F);
    const uintptr_t non_ascii_mask = kUintptrAllBitsSet / 0xFF * 0x80;
    while (chars + sizeof(uintptr_t) <= limit) {
      if (*reinterpret_cast<const uintptr_t*>(chars) & non_ascii_mask) {
        return static_cast<int>(chars - start);
      }
      chars += sizeof(uintptr_t);
    }
#endif
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

  // TODO(dcarney): Replace all instances of this with VisitFlat.
  template<class Visitor, class ConsOp>
  static inline void Visit(String* string,
                           unsigned offset,
                           Visitor& visitor,
                           ConsOp& cons_op,
                           int32_t type,
                           unsigned length);

  template<class Visitor>
  static inline ConsString* VisitFlat(Visitor* visitor,
                                      String* string,
                                      int offset,
                                      int length,
                                      int32_t type);

  template<class Visitor>
  static inline ConsString* VisitFlat(Visitor* visitor,
                                      String* string,
                                      int offset = 0) {
    int32_t type = string->map()->instance_type();
    return VisitFlat(visitor, string, offset, string->length(), type);
  }

 private:
  friend class Name;

  // Try to flatten the top level ConsString that is hiding behind this
  // string.  This is a no-op unless the string is a ConsString.  Flatten
  // mutates the ConsString and might return a failure.
  MUST_USE_RESULT MaybeObject* SlowTryFlatten(PretenureFlag pretenure);

  // Slow case of String::Equals.  This implementation works on any strings
  // but it is most efficient on strings that are almost flat.
  bool SlowEquals(String* other);

  // Slow case of AsArrayIndex.
  bool SlowAsArrayIndex(uint32_t* index);

  // Compute and set the hash code.
  uint32_t ComputeAndSetHash();

  DISALLOW_IMPLICIT_CONSTRUCTORS(String);
};


// The SeqString abstract class captures sequential string values.
class SeqString: public String {
 public:
  // Casting.
  static inline SeqString* cast(Object* obj);

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


// The AsciiString class captures sequential ASCII string objects.
// Each character in the AsciiString is an ASCII character.
class SeqOneByteString: public SeqString {
 public:
  static const bool kHasAsciiEncoding = true;

  // Dispatched behavior.
  inline uint16_t SeqOneByteStringGet(int index);
  inline void SeqOneByteStringSet(int index, uint16_t value);

  // Get the address of the characters in this string.
  inline Address GetCharsAddress();

  inline uint8_t* GetChars();

  // Casting
  static inline SeqOneByteString* cast(Object* obj);

  // Garbage collection support.  This method is called by the
  // garbage collector to compute the actual size of an AsciiString
  // instance.
  inline int SeqOneByteStringSize(InstanceType instance_type);

  // Computes the size for an AsciiString instance of a given length.
  static int SizeFor(int length) {
    return OBJECT_POINTER_ALIGN(kHeaderSize + length * kCharSize);
  }

  // Maximal memory usage for a single sequential ASCII string.
  static const int kMaxSize = 512 * MB - 1;
  // Maximal length of a single sequential ASCII string.
  // Q.v. String::kMaxLength which is the maximal size of concatenated strings.
  static const int kMaxLength = (kMaxSize - kHeaderSize);

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(SeqOneByteString);
};


// The TwoByteString class captures sequential unicode string objects.
// Each character in the TwoByteString is a two-byte uint16_t.
class SeqTwoByteString: public SeqString {
 public:
  static const bool kHasAsciiEncoding = false;

  // Dispatched behavior.
  inline uint16_t SeqTwoByteStringGet(int index);
  inline void SeqTwoByteStringSet(int index, uint16_t value);

  // Get the address of the characters in this string.
  inline Address GetCharsAddress();

  inline uc16* GetChars();

  // For regexp code.
  const uint16_t* SeqTwoByteStringGetData(unsigned start);

  // Casting
  static inline SeqTwoByteString* cast(Object* obj);

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
  // Maximal length of a single sequential two-byte string.
  // Q.v. String::kMaxLength which is the maximal size of concatenated strings.
  static const int kMaxLength = (kMaxSize - kHeaderSize) / sizeof(uint16_t);

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
  uint16_t ConsStringGet(int index);

  // Casting.
  static inline ConsString* cast(Object* obj);

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
  inline int offset();
  inline void set_offset(int offset);

  // Dispatched behavior.
  uint16_t SlicedStringGet(int index);

  // Casting.
  static inline SlicedString* cast(Object* obj);

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
  // Casting
  static inline ExternalString* cast(Object* obj);

  // Layout description.
  static const int kResourceOffset = POINTER_SIZE_ALIGN(String::kSize);
  static const int kShortSize = kResourceOffset + kPointerSize;
  static const int kResourceDataOffset = kResourceOffset + kPointerSize;
  static const int kSize = kResourceDataOffset + kPointerSize;

  static const int kMaxShortLength =
      (kShortSize - SeqString::kHeaderSize) / kCharSize;

  // Return whether external string is short (data pointer is not cached).
  inline bool is_short();

  STATIC_CHECK(kResourceOffset == Internals::kStringResourceOffset);

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(ExternalString);
};


// The ExternalAsciiString class is an external string backed by an
// ASCII string.
class ExternalAsciiString: public ExternalString {
 public:
  static const bool kHasAsciiEncoding = true;

  typedef v8::String::ExternalAsciiStringResource Resource;

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
  inline uint16_t ExternalAsciiStringGet(int index);

  // Casting.
  static inline ExternalAsciiString* cast(Object* obj);

  // Garbage collection support.
  inline void ExternalAsciiStringIterateBody(ObjectVisitor* v);

  template<typename StaticVisitor>
  inline void ExternalAsciiStringIterateBody();

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(ExternalAsciiString);
};


// The ExternalTwoByteString class is an external string backed by a UTF-16
// encoded string.
class ExternalTwoByteString: public ExternalString {
 public:
  static const bool kHasAsciiEncoding = false;

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

  // Casting.
  static inline ExternalTwoByteString* cast(Object* obj);

  // Garbage collection support.
  inline void ExternalTwoByteStringIterateBody(ObjectVisitor* v);

  template<typename StaticVisitor>
  inline void ExternalTwoByteStringIterateBody();

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
  int length() { return length_; }
 private:
  String** str_;
  bool is_ascii_;
  int length_;
  const void* start_;
};


// A ConsStringOp that returns null.
// Useful when the operation to apply on a ConsString
// requires an expensive data structure.
class ConsStringNullOp {
 public:
  inline ConsStringNullOp() {}
  static inline String* Operate(String*, unsigned*, int32_t*, unsigned*);
 private:
  DISALLOW_COPY_AND_ASSIGN(ConsStringNullOp);
};


// This maintains an off-stack representation of the stack frames required
// to traverse a ConsString, allowing an entirely iterative and restartable
// traversal of the entire string
// Note: this class is not GC-safe.
class ConsStringIteratorOp {
 public:
  inline ConsStringIteratorOp() {}
  String* Operate(String* string,
                  unsigned* offset_out,
                  int32_t* type_out,
                  unsigned* length_out);
  inline String* ContinueOperation(int32_t* type_out, unsigned* length_out);
  inline void Reset();
  inline bool HasMore();

 private:
  // TODO(dcarney): Templatize this out for different stack sizes.
  static const unsigned kStackSize = 32;
  // Use a mask instead of doing modulo operations for stack wrapping.
  static const unsigned kDepthMask = kStackSize-1;
  STATIC_ASSERT(IS_POWER_OF_TWO(kStackSize));
  static inline unsigned OffsetForDepth(unsigned depth);

  inline void PushLeft(ConsString* string);
  inline void PushRight(ConsString* string);
  inline void AdjustMaximumDepth();
  inline void Pop();
  String* NextLeaf(bool* blew_stack, int32_t* type_out, unsigned* length_out);
  String* Search(unsigned* offset_out,
                 int32_t* type_out,
                 unsigned* length_out);

  unsigned depth_;
  unsigned maximum_depth_;
  // Stack must always contain only frames for which right traversal
  // has not yet been performed.
  ConsString* frames_[kStackSize];
  unsigned consumed_;
  ConsString* root_;
  DISALLOW_COPY_AND_ASSIGN(ConsStringIteratorOp);
};


// Note: this class is not GC-safe.
class StringCharacterStream {
 public:
  inline StringCharacterStream(String* string,
                               ConsStringIteratorOp* op,
                               unsigned offset = 0);
  inline uint16_t GetNext();
  inline bool HasMore();
  inline void Reset(String* string, unsigned offset = 0);
  inline void VisitOneByteString(const uint8_t* chars, unsigned length);
  inline void VisitTwoByteString(const uint16_t* chars, unsigned length);

 private:
  bool is_one_byte_;
  union {
    const uint8_t* buffer8_;
    const uint16_t* buffer16_;
  };
  const uint8_t* end_;
  ConsStringIteratorOp* op_;
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
  // [to_string]: Cached to_string computed at startup.
  DECL_ACCESSORS(to_string, String)

  // [to_number]: Cached to_number computed at startup.
  DECL_ACCESSORS(to_number, Object)

  inline byte kind();
  inline void set_kind(byte kind);

  // Casting.
  static inline Oddball* cast(Object* obj);

  // Dispatched behavior.
  DECLARE_VERIFIER(Oddball)

  // Initialize the fields.
  MUST_USE_RESULT MaybeObject* Initialize(Heap* heap,
                                          const char* to_string,
                                          Object* to_number,
                                          byte kind);

  // Layout description.
  static const int kToStringOffset = HeapObject::kHeaderSize;
  static const int kToNumberOffset = kToStringOffset + kPointerSize;
  static const int kKindOffset = kToNumberOffset + kPointerSize;
  static const int kSize = kKindOffset + kPointerSize;

  static const byte kFalse = 0;
  static const byte kTrue = 1;
  static const byte kNotBooleanMask = ~1;
  static const byte kTheHole = 2;
  static const byte kNull = 3;
  static const byte kArgumentMarker = 4;
  static const byte kUndefined = 5;
  static const byte kUninitialized = 6;
  static const byte kOther = 7;

  typedef FixedBodyDescriptor<kToStringOffset,
                              kToNumberOffset + kPointerSize,
                              kSize> BodyDescriptor;

  STATIC_CHECK(kKindOffset == Internals::kOddballKindOffset);
  STATIC_CHECK(kNull == Internals::kNullOddballKind);
  STATIC_CHECK(kUndefined == Internals::kUndefinedOddballKind);

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(Oddball);
};


class Cell: public HeapObject {
 public:
  // [value]: value of the global property.
  DECL_ACCESSORS(value, Object)

  // Casting.
  static inline Cell* cast(Object* obj);

  static inline Cell* FromValueAddress(Address value) {
    Object* result = FromAddress(value - kValueOffset);
    ASSERT(result->IsCell() || result->IsPropertyCell());
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


class PropertyCell: public Cell {
 public:
  // [type]: type of the global property.
  HeapType* type();
  void set_type(HeapType* value, WriteBarrierMode mode = UPDATE_WRITE_BARRIER);

  // [dependent_code]: dependent code that depends on the type of the global
  // property.
  DECL_ACCESSORS(dependent_code, DependentCode)

  // Sets the value of the cell and updates the type field to be the union
  // of the cell's current type and the value's type. If the change causes
  // a change of the type of the cell's contents, code dependent on the cell
  // will be deoptimized.
  static void SetValueInferType(Handle<PropertyCell> cell,
                                Handle<Object> value);

  // Computes the new type of the cell's contents for the given value, but
  // without actually modifying the 'type' field.
  static Handle<HeapType> UpdatedType(Handle<PropertyCell> cell,
                                      Handle<Object> value);

  void AddDependentCompilationInfo(CompilationInfo* info);

  void AddDependentCode(Handle<Code> code);

  // Casting.
  static inline PropertyCell* cast(Object* obj);

  inline Address TypeAddress() {
    return address() + kTypeOffset;
  }

  // Dispatched behavior.
  DECLARE_PRINTER(PropertyCell)
  DECLARE_VERIFIER(PropertyCell)

  // Layout description.
  static const int kTypeOffset = kValueOffset + kPointerSize;
  static const int kDependentCodeOffset = kTypeOffset + kPointerSize;
  static const int kSize = kDependentCodeOffset + kPointerSize;

  static const int kPointerFieldsBeginOffset = kValueOffset;
  static const int kPointerFieldsEndOffset = kDependentCodeOffset;

  typedef FixedBodyDescriptor<kValueOffset,
                              kSize,
                              kSize> BodyDescriptor;

 private:
  DECL_ACCESSORS(type_raw, Object)
  DISALLOW_IMPLICIT_CONSTRUCTORS(PropertyCell);
};


// The JSProxy describes EcmaScript Harmony proxies
class JSProxy: public JSReceiver {
 public:
  // [handler]: The handler property.
  DECL_ACCESSORS(handler, Object)

  // [hash]: The hash code property (undefined if not initialized yet).
  DECL_ACCESSORS(hash, Object)

  // Casting.
  static inline JSProxy* cast(Object* obj);

  MUST_USE_RESULT MaybeObject* GetPropertyWithHandler(
      Object* receiver,
      Name* name);
  MUST_USE_RESULT MaybeObject* GetElementWithHandler(
      Object* receiver,
      uint32_t index);

  // If the handler defines an accessor property with a setter, invoke it.
  // If it defines an accessor property without a setter, or a data property
  // that is read-only, throw. In all these cases set '*done' to true,
  // otherwise set it to false.
  static Handle<Object> SetPropertyViaPrototypesWithHandler(
      Handle<JSProxy> proxy,
      Handle<JSReceiver> receiver,
      Handle<Name> name,
      Handle<Object> value,
      PropertyAttributes attributes,
      StrictModeFlag strict_mode,
      bool* done);

  MUST_USE_RESULT PropertyAttributes GetPropertyAttributeWithHandler(
      JSReceiver* receiver,
      Name* name);
  MUST_USE_RESULT PropertyAttributes GetElementAttributeWithHandler(
      JSReceiver* receiver,
      uint32_t index);

  // Turn the proxy into an (empty) JSObject.
  static void Fix(Handle<JSProxy> proxy);

  // Initializes the body after the handler slot.
  inline void InitializeBody(int object_size, Object* value);

  // Invoke a trap by name. If the trap does not exist on this's handler,
  // but derived_trap is non-NULL, invoke that instead.  May cause GC.
  Handle<Object> CallTrap(const char* name,
                          Handle<Object> derived_trap,
                          int argc,
                          Handle<Object> args[]);

  // Dispatched behavior.
  DECLARE_PRINTER(JSProxy)
  DECLARE_VERIFIER(JSProxy)

  // Layout description. We add padding so that a proxy has the same
  // size as a virgin JSObject. This is essential for becoming a JSObject
  // upon freeze.
  static const int kHandlerOffset = HeapObject::kHeaderSize;
  static const int kHashOffset = kHandlerOffset + kPointerSize;
  static const int kPaddingOffset = kHashOffset + kPointerSize;
  static const int kSize = JSObject::kHeaderSize;
  static const int kHeaderSize = kPaddingOffset;
  static const int kPaddingSize = kSize - kPaddingOffset;

  STATIC_CHECK(kPaddingSize >= 0);

  typedef FixedBodyDescriptor<kHandlerOffset,
                              kPaddingOffset,
                              kSize> BodyDescriptor;

 private:
  friend class JSReceiver;

  static Handle<Object> SetPropertyWithHandler(Handle<JSProxy> proxy,
                                               Handle<JSReceiver> receiver,
                                               Handle<Name> name,
                                               Handle<Object> value,
                                               PropertyAttributes attributes,
                                               StrictModeFlag strict_mode);
  static Handle<Object> SetElementWithHandler(Handle<JSProxy> proxy,
                                              Handle<JSReceiver> receiver,
                                              uint32_t index,
                                              Handle<Object> value,
                                              StrictModeFlag strict_mode);

  static bool HasPropertyWithHandler(Handle<JSProxy> proxy, Handle<Name> name);
  static bool HasElementWithHandler(Handle<JSProxy> proxy, uint32_t index);

  static Handle<Object> DeletePropertyWithHandler(Handle<JSProxy> proxy,
                                                  Handle<Name> name,
                                                  DeleteMode mode);
  static Handle<Object> DeleteElementWithHandler(Handle<JSProxy> proxy,
                                                 uint32_t index,
                                                 DeleteMode mode);

  MUST_USE_RESULT Object* GetIdentityHash();

  static Handle<Object> GetOrCreateIdentityHash(Handle<JSProxy> proxy);

  DISALLOW_IMPLICIT_CONSTRUCTORS(JSProxy);
};


class JSFunctionProxy: public JSProxy {
 public:
  // [call_trap]: The call trap.
  DECL_ACCESSORS(call_trap, Object)

  // [construct_trap]: The construct trap.
  DECL_ACCESSORS(construct_trap, Object)

  // Casting.
  static inline JSFunctionProxy* cast(Object* obj);

  // Dispatched behavior.
  DECLARE_PRINTER(JSFunctionProxy)
  DECLARE_VERIFIER(JSFunctionProxy)

  // Layout description.
  static const int kCallTrapOffset = JSProxy::kPaddingOffset;
  static const int kConstructTrapOffset = kCallTrapOffset + kPointerSize;
  static const int kPaddingOffset = kConstructTrapOffset + kPointerSize;
  static const int kSize = JSFunction::kSize;
  static const int kPaddingSize = kSize - kPaddingOffset;

  STATIC_CHECK(kPaddingSize >= 0);

  typedef FixedBodyDescriptor<kHandlerOffset,
                              kConstructTrapOffset + kPointerSize,
                              kSize> BodyDescriptor;

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(JSFunctionProxy);
};


// The JSSet describes EcmaScript Harmony sets
class JSSet: public JSObject {
 public:
  // [set]: the backing hash set containing keys.
  DECL_ACCESSORS(table, Object)

  // Casting.
  static inline JSSet* cast(Object* obj);

  // Dispatched behavior.
  DECLARE_PRINTER(JSSet)
  DECLARE_VERIFIER(JSSet)

  static const int kTableOffset = JSObject::kHeaderSize;
  static const int kSize = kTableOffset + kPointerSize;

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(JSSet);
};


// The JSMap describes EcmaScript Harmony maps
class JSMap: public JSObject {
 public:
  // [table]: the backing hash table mapping keys to values.
  DECL_ACCESSORS(table, Object)

  // Casting.
  static inline JSMap* cast(Object* obj);

  // Dispatched behavior.
  DECLARE_PRINTER(JSMap)
  DECLARE_VERIFIER(JSMap)

  static const int kTableOffset = JSObject::kHeaderSize;
  static const int kSize = kTableOffset + kPointerSize;

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(JSMap);
};


// Base class for both JSWeakMap and JSWeakSet
class JSWeakCollection: public JSObject {
 public:
  // [table]: the backing hash table mapping keys to values.
  DECL_ACCESSORS(table, Object)

  // [next]: linked list of encountered weak maps during GC.
  DECL_ACCESSORS(next, Object)

  static const int kTableOffset = JSObject::kHeaderSize;
  static const int kNextOffset = kTableOffset + kPointerSize;
  static const int kSize = kNextOffset + kPointerSize;

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(JSWeakCollection);
};


// The JSWeakMap describes EcmaScript Harmony weak maps
class JSWeakMap: public JSWeakCollection {
 public:
  // Casting.
  static inline JSWeakMap* cast(Object* obj);

  // Dispatched behavior.
  DECLARE_PRINTER(JSWeakMap)
  DECLARE_VERIFIER(JSWeakMap)

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(JSWeakMap);
};


// The JSWeakSet describes EcmaScript Harmony weak sets
class JSWeakSet: public JSWeakCollection {
 public:
  // Casting.
  static inline JSWeakSet* cast(Object* obj);

  // Dispatched behavior.
  DECLARE_PRINTER(JSWeakSet)
  DECLARE_VERIFIER(JSWeakSet)

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(JSWeakSet);
};


class JSArrayBuffer: public JSObject {
 public:
  // [backing_store]: backing memory for this array
  DECL_ACCESSORS(backing_store, void)

  // [byte_length]: length in bytes
  DECL_ACCESSORS(byte_length, Object)

  // [flags]
  DECL_ACCESSORS(flag, Smi)

  inline bool is_external();
  inline void set_is_external(bool value);

  inline bool should_be_freed();
  inline void set_should_be_freed(bool value);

  // [weak_next]: linked list of array buffers.
  DECL_ACCESSORS(weak_next, Object)

  // [weak_first_array]: weak linked list of views.
  DECL_ACCESSORS(weak_first_view, Object)

  // Casting.
  static inline JSArrayBuffer* cast(Object* obj);

  // Neutering. Only neuters the buffer, not associated typed arrays.
  void Neuter();

  // Dispatched behavior.
  DECLARE_PRINTER(JSArrayBuffer)
  DECLARE_VERIFIER(JSArrayBuffer)

  static const int kBackingStoreOffset = JSObject::kHeaderSize;
  static const int kByteLengthOffset = kBackingStoreOffset + kPointerSize;
  static const int kFlagOffset = kByteLengthOffset + kPointerSize;
  static const int kWeakNextOffset = kFlagOffset + kPointerSize;
  static const int kWeakFirstViewOffset = kWeakNextOffset + kPointerSize;
  static const int kSize = kWeakFirstViewOffset + kPointerSize;

  static const int kSizeWithInternalFields =
      kSize + v8::ArrayBuffer::kInternalFieldCount * kPointerSize;

 private:
  // Bit position in a flag
  static const int kIsExternalBit = 0;
  static const int kShouldBeFreed = 1;

  DISALLOW_IMPLICIT_CONSTRUCTORS(JSArrayBuffer);
};


class JSArrayBufferView: public JSObject {
 public:
  // [buffer]: ArrayBuffer that this typed array views.
  DECL_ACCESSORS(buffer, Object)

  // [byte_length]: offset of typed array in bytes.
  DECL_ACCESSORS(byte_offset, Object)

  // [byte_length]: length of typed array in bytes.
  DECL_ACCESSORS(byte_length, Object)

  // [weak_next]: linked list of typed arrays over the same array buffer.
  DECL_ACCESSORS(weak_next, Object)

  // Casting.
  static inline JSArrayBufferView* cast(Object* obj);

  DECLARE_VERIFIER(JSArrayBufferView)

  static const int kBufferOffset = JSObject::kHeaderSize;
  static const int kByteOffsetOffset = kBufferOffset + kPointerSize;
  static const int kByteLengthOffset = kByteOffsetOffset + kPointerSize;
  static const int kWeakNextOffset = kByteLengthOffset + kPointerSize;
  static const int kViewSize = kWeakNextOffset + kPointerSize;

 protected:
  void NeuterView();

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(JSArrayBufferView);
};


class JSTypedArray: public JSArrayBufferView {
 public:
  // [length]: length of typed array in elements.
  DECL_ACCESSORS(length, Object)

  // Neutering. Only neuters this typed array.
  void Neuter();

  // Casting.
  static inline JSTypedArray* cast(Object* obj);

  ExternalArrayType type();
  size_t element_size();

  // Dispatched behavior.
  DECLARE_PRINTER(JSTypedArray)
  DECLARE_VERIFIER(JSTypedArray)

  static const int kLengthOffset = kViewSize + kPointerSize;
  static const int kSize = kLengthOffset + kPointerSize;

  static const int kSizeWithInternalFields =
      kSize + v8::ArrayBufferView::kInternalFieldCount * kPointerSize;

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(JSTypedArray);
};


class JSDataView: public JSArrayBufferView {
 public:
  // Only neuters this DataView
  void Neuter();

  // Casting.
  static inline JSDataView* cast(Object* obj);

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
// Since they cannot contain references to JS HeapObjects they can be
// placed in old_data_space.
class Foreign: public HeapObject {
 public:
  // [address]: field containing the address.
  inline Address foreign_address();
  inline void set_foreign_address(Address value);

  // Casting.
  static inline Foreign* cast(Object* obj);

  // Dispatched behavior.
  inline void ForeignIterateBody(ObjectVisitor* v);

  template<typename StaticVisitor>
  inline void ForeignIterateBody();

  // Dispatched behavior.
  DECLARE_PRINTER(Foreign)
  DECLARE_VERIFIER(Foreign)

  // Layout description.

  static const int kForeignAddressOffset = HeapObject::kHeaderSize;
  static const int kSize = kForeignAddressOffset + kPointerSize;

  STATIC_CHECK(kForeignAddressOffset == Internals::kForeignAddressOffset);

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

  static void JSArrayUpdateLengthFromIndex(Handle<JSArray> array,
                                           uint32_t index,
                                           Handle<Object> value);

  MUST_USE_RESULT MaybeObject* JSArrayUpdateLengthFromIndex(uint32_t index,
                                                            Object* value);

  // Initialize the array with the given capacity. The function may
  // fail due to out-of-memory situations, but only if the requested
  // capacity is non-zero.
  MUST_USE_RESULT MaybeObject* Initialize(int capacity, int length = 0);

  // Initializes the array to a certain length.
  inline bool AllowsSetElementsLength();
  // Can cause GC.
  MUST_USE_RESULT MaybeObject* SetElementsLength(Object* length);

  // Set the content of the array to the content of storage.
  MUST_USE_RESULT inline MaybeObject* SetContent(FixedArrayBase* storage);

  // Casting.
  static inline JSArray* cast(Object* obj);

  // Uses handles.  Ensures that the fixed array backing the JSArray has at
  // least the stated size.
  inline void EnsureSize(int minimum_size_of_backing_fixed_array);

  // Dispatched behavior.
  DECLARE_PRINTER(JSArray)
  DECLARE_VERIFIER(JSArray)

  // Number of element slots to pre-allocate for an empty array.
  static const int kPreallocatedArrayElements = 4;

  // Layout description.
  static const int kLengthOffset = JSObject::kHeaderSize;
  static const int kSize = kLengthOffset + kPointerSize;

 private:
  // Expand the fixed array backing of a fast-case JSArray to at least
  // the requested size.
  void Expand(int minimum_size_of_backing_fixed_array);

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


class AccessorInfo: public Struct {
 public:
  DECL_ACCESSORS(name, Object)
  DECL_ACCESSORS(flag, Smi)
  DECL_ACCESSORS(expected_receiver_type, Object)

  inline bool all_can_read();
  inline void set_all_can_read(bool value);

  inline bool all_can_write();
  inline void set_all_can_write(bool value);

  inline bool prohibits_overwriting();
  inline void set_prohibits_overwriting(bool value);

  inline PropertyAttributes property_attributes();
  inline void set_property_attributes(PropertyAttributes attributes);

  // Checks whether the given receiver is compatible with this accessor.
  inline bool IsCompatibleReceiver(Object* receiver);

  static inline AccessorInfo* cast(Object* obj);

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
  static const int kSize = kExpectedReceiverTypeOffset + kPointerSize;

 private:
  // Bit positions in flag.
  static const int kAllCanReadBit = 0;
  static const int kAllCanWriteBit = 1;
  static const int kProhibitsOverwritingBit = 2;
  class AttributesField: public BitField<PropertyAttributes, 3, 3> {};

  DISALLOW_IMPLICIT_CONSTRUCTORS(AccessorInfo);
};


enum AccessorDescriptorType {
  kDescriptorBitmaskCompare,
  kDescriptorPointerCompare,
  kDescriptorPrimitiveValue,
  kDescriptorObjectDereference,
  kDescriptorPointerDereference,
  kDescriptorPointerShift,
  kDescriptorReturnObject
};


struct BitmaskCompareDescriptor {
  uint32_t bitmask;
  uint32_t compare_value;
  uint8_t size;  // Must be in {1,2,4}.
};


struct PointerCompareDescriptor {
  void* compare_value;
};


struct PrimitiveValueDescriptor {
  v8::DeclaredAccessorDescriptorDataType data_type;
  uint8_t bool_offset;  // Must be in [0,7], used for kDescriptorBoolType.
};


struct ObjectDerefenceDescriptor {
  uint8_t internal_field;
};


struct PointerShiftDescriptor {
  int16_t byte_offset;
};


struct DeclaredAccessorDescriptorData {
  AccessorDescriptorType type;
  union {
    struct BitmaskCompareDescriptor bitmask_compare_descriptor;
    struct PointerCompareDescriptor pointer_compare_descriptor;
    struct PrimitiveValueDescriptor primitive_value_descriptor;
    struct ObjectDerefenceDescriptor object_dereference_descriptor;
    struct PointerShiftDescriptor pointer_shift_descriptor;
  };
};


class DeclaredAccessorDescriptor;


class DeclaredAccessorDescriptorIterator {
 public:
  explicit DeclaredAccessorDescriptorIterator(
      DeclaredAccessorDescriptor* descriptor);
  const DeclaredAccessorDescriptorData* Next();
  bool Complete() const { return length_ == offset_; }
 private:
  uint8_t* array_;
  const int length_;
  int offset_;
  DISALLOW_IMPLICIT_CONSTRUCTORS(DeclaredAccessorDescriptorIterator);
};


class DeclaredAccessorDescriptor: public Struct {
 public:
  DECL_ACCESSORS(serialized_data, ByteArray)

  static inline DeclaredAccessorDescriptor* cast(Object* obj);

  static Handle<DeclaredAccessorDescriptor> Create(
      Isolate* isolate,
      const DeclaredAccessorDescriptorData& data,
      Handle<DeclaredAccessorDescriptor> previous);

  // Dispatched behavior.
  DECLARE_PRINTER(DeclaredAccessorDescriptor)
  DECLARE_VERIFIER(DeclaredAccessorDescriptor)

  static const int kSerializedDataOffset = HeapObject::kHeaderSize;
  static const int kSize = kSerializedDataOffset + kPointerSize;

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(DeclaredAccessorDescriptor);
};


class DeclaredAccessorInfo: public AccessorInfo {
 public:
  DECL_ACCESSORS(descriptor, DeclaredAccessorDescriptor)

  static inline DeclaredAccessorInfo* cast(Object* obj);

  // Dispatched behavior.
  DECLARE_PRINTER(DeclaredAccessorInfo)
  DECLARE_VERIFIER(DeclaredAccessorInfo)

  static const int kDescriptorOffset = AccessorInfo::kSize;
  static const int kSize = kDescriptorOffset + kPointerSize;

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(DeclaredAccessorInfo);
};


// An accessor must have a getter, but can have no setter.
//
// When setting a property, V8 searches accessors in prototypes.
// If an accessor was found and it does not have a setter,
// the request is ignored.
//
// If the accessor in the prototype has the READ_ONLY property attribute, then
// a new value is added to the local object when the property is set.
// This shadows the accessor in the prototype.
class ExecutableAccessorInfo: public AccessorInfo {
 public:
  DECL_ACCESSORS(getter, Object)
  DECL_ACCESSORS(setter, Object)
  DECL_ACCESSORS(data, Object)

  static inline ExecutableAccessorInfo* cast(Object* obj);

  // Dispatched behavior.
  DECLARE_PRINTER(ExecutableAccessorInfo)
  DECLARE_VERIFIER(ExecutableAccessorInfo)

  static const int kGetterOffset = AccessorInfo::kSize;
  static const int kSetterOffset = kGetterOffset + kPointerSize;
  static const int kDataOffset = kSetterOffset + kPointerSize;
  static const int kSize = kDataOffset + kPointerSize;

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(ExecutableAccessorInfo);
};


// Support for JavaScript accessors: A pair of a getter and a setter. Each
// accessor can either be
//   * a pointer to a JavaScript function or proxy: a real accessor
//   * undefined: considered an accessor by the spec, too, strangely enough
//   * the hole: an accessor which has not been set
//   * a pointer to a map: a transition used to ensure map sharing
// access_flags provides the ability to override access checks on access check
// failure.
class AccessorPair: public Struct {
 public:
  DECL_ACCESSORS(getter, Object)
  DECL_ACCESSORS(setter, Object)
  DECL_ACCESSORS(access_flags, Smi)

  inline void set_access_flags(v8::AccessControl access_control);
  inline bool all_can_read();
  inline bool all_can_write();
  inline bool prohibits_overwriting();

  static inline AccessorPair* cast(Object* obj);

  static Handle<AccessorPair> Copy(Handle<AccessorPair> pair);

  Object* get(AccessorComponent component) {
    return component == ACCESSOR_GETTER ? getter() : setter();
  }

  void set(AccessorComponent component, Object* value) {
    if (component == ACCESSOR_GETTER) {
      set_getter(value);
    } else {
      set_setter(value);
    }
  }

  // Note: Returns undefined instead in case of a hole.
  Object* GetComponent(AccessorComponent component);

  // Set both components, skipping arguments which are a JavaScript null.
  void SetComponents(Object* getter, Object* setter) {
    if (!getter->IsNull()) set_getter(getter);
    if (!setter->IsNull()) set_setter(setter);
  }

  bool ContainsAccessor() {
    return IsJSAccessor(getter()) || IsJSAccessor(setter());
  }

  // Dispatched behavior.
  DECLARE_PRINTER(AccessorPair)
  DECLARE_VERIFIER(AccessorPair)

  static const int kGetterOffset = HeapObject::kHeaderSize;
  static const int kSetterOffset = kGetterOffset + kPointerSize;
  static const int kAccessFlagsOffset = kSetterOffset + kPointerSize;
  static const int kSize = kAccessFlagsOffset + kPointerSize;

 private:
  static const int kAllCanReadBit = 0;
  static const int kAllCanWriteBit = 1;
  static const int kProhibitsOverwritingBit = 2;

  // Strangely enough, in addition to functions and harmony proxies, the spec
  // requires us to consider undefined as a kind of accessor, too:
  //    var obj = {};
  //    Object.defineProperty(obj, "foo", {get: undefined});
  //    assertTrue("foo" in obj);
  bool IsJSAccessor(Object* obj) {
    return obj->IsSpecFunction() || obj->IsUndefined();
  }

  DISALLOW_IMPLICIT_CONSTRUCTORS(AccessorPair);
};


class AccessCheckInfo: public Struct {
 public:
  DECL_ACCESSORS(named_callback, Object)
  DECL_ACCESSORS(indexed_callback, Object)
  DECL_ACCESSORS(data, Object)

  static inline AccessCheckInfo* cast(Object* obj);

  // Dispatched behavior.
  DECLARE_PRINTER(AccessCheckInfo)
  DECLARE_VERIFIER(AccessCheckInfo)

  static const int kNamedCallbackOffset   = HeapObject::kHeaderSize;
  static const int kIndexedCallbackOffset = kNamedCallbackOffset + kPointerSize;
  static const int kDataOffset = kIndexedCallbackOffset + kPointerSize;
  static const int kSize = kDataOffset + kPointerSize;

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(AccessCheckInfo);
};


class InterceptorInfo: public Struct {
 public:
  DECL_ACCESSORS(getter, Object)
  DECL_ACCESSORS(setter, Object)
  DECL_ACCESSORS(query, Object)
  DECL_ACCESSORS(deleter, Object)
  DECL_ACCESSORS(enumerator, Object)
  DECL_ACCESSORS(data, Object)

  static inline InterceptorInfo* cast(Object* obj);

  // Dispatched behavior.
  DECLARE_PRINTER(InterceptorInfo)
  DECLARE_VERIFIER(InterceptorInfo)

  static const int kGetterOffset = HeapObject::kHeaderSize;
  static const int kSetterOffset = kGetterOffset + kPointerSize;
  static const int kQueryOffset = kSetterOffset + kPointerSize;
  static const int kDeleterOffset = kQueryOffset + kPointerSize;
  static const int kEnumeratorOffset = kDeleterOffset + kPointerSize;
  static const int kDataOffset = kEnumeratorOffset + kPointerSize;
  static const int kSize = kDataOffset + kPointerSize;

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(InterceptorInfo);
};


class CallHandlerInfo: public Struct {
 public:
  DECL_ACCESSORS(callback, Object)
  DECL_ACCESSORS(data, Object)

  static inline CallHandlerInfo* cast(Object* obj);

  // Dispatched behavior.
  DECLARE_PRINTER(CallHandlerInfo)
  DECLARE_VERIFIER(CallHandlerInfo)

  static const int kCallbackOffset = HeapObject::kHeaderSize;
  static const int kDataOffset = kCallbackOffset + kPointerSize;
  static const int kSize = kDataOffset + kPointerSize;

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(CallHandlerInfo);
};


class TemplateInfo: public Struct {
 public:
  DECL_ACCESSORS(tag, Object)
  DECL_ACCESSORS(property_list, Object)
  DECL_ACCESSORS(property_accessors, Object)

  DECLARE_VERIFIER(TemplateInfo)

  static const int kTagOffset = HeapObject::kHeaderSize;
  static const int kPropertyListOffset = kTagOffset + kPointerSize;
  static const int kPropertyAccessorsOffset =
      kPropertyListOffset + kPointerSize;
  static const int kHeaderSize = kPropertyAccessorsOffset + kPointerSize;

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(TemplateInfo);
};


class FunctionTemplateInfo: public TemplateInfo {
 public:
  DECL_ACCESSORS(serial_number, Object)
  DECL_ACCESSORS(call_code, Object)
  DECL_ACCESSORS(prototype_template, Object)
  DECL_ACCESSORS(parent_template, Object)
  DECL_ACCESSORS(named_property_handler, Object)
  DECL_ACCESSORS(indexed_property_handler, Object)
  DECL_ACCESSORS(instance_template, Object)
  DECL_ACCESSORS(class_name, Object)
  DECL_ACCESSORS(signature, Object)
  DECL_ACCESSORS(instance_call_handler, Object)
  DECL_ACCESSORS(access_check_info, Object)
  DECL_ACCESSORS(flag, Smi)

  inline int length();
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

  static inline FunctionTemplateInfo* cast(Object* obj);

  // Dispatched behavior.
  DECLARE_PRINTER(FunctionTemplateInfo)
  DECLARE_VERIFIER(FunctionTemplateInfo)

  static const int kSerialNumberOffset = TemplateInfo::kHeaderSize;
  static const int kCallCodeOffset = kSerialNumberOffset + kPointerSize;
  static const int kPrototypeTemplateOffset =
      kCallCodeOffset + kPointerSize;
  static const int kParentTemplateOffset =
      kPrototypeTemplateOffset + kPointerSize;
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
  static const int kFlagOffset = kAccessCheckInfoOffset + kPointerSize;
  static const int kLengthOffset = kFlagOffset + kPointerSize;
  static const int kSize = kLengthOffset + kPointerSize;

  // Returns true if |object| is an instance of this function template.
  bool IsTemplateFor(Object* object);
  bool IsTemplateFor(Map* map);

 private:
  // Bit position in the flag, from least significant bit position.
  static const int kHiddenPrototypeBit   = 0;
  static const int kUndetectableBit      = 1;
  static const int kNeedsAccessCheckBit  = 2;
  static const int kReadOnlyPrototypeBit = 3;
  static const int kRemovePrototypeBit   = 4;
  static const int kDoNotCacheBit        = 5;

  DISALLOW_IMPLICIT_CONSTRUCTORS(FunctionTemplateInfo);
};


class ObjectTemplateInfo: public TemplateInfo {
 public:
  DECL_ACCESSORS(constructor, Object)
  DECL_ACCESSORS(internal_field_count, Object)

  static inline ObjectTemplateInfo* cast(Object* obj);

  // Dispatched behavior.
  DECLARE_PRINTER(ObjectTemplateInfo)
  DECLARE_VERIFIER(ObjectTemplateInfo)

  static const int kConstructorOffset = TemplateInfo::kHeaderSize;
  static const int kInternalFieldCountOffset =
      kConstructorOffset + kPointerSize;
  static const int kSize = kInternalFieldCountOffset + kPointerSize;
};


class SignatureInfo: public Struct {
 public:
  DECL_ACCESSORS(receiver, Object)
  DECL_ACCESSORS(args, Object)

  static inline SignatureInfo* cast(Object* obj);

  // Dispatched behavior.
  DECLARE_PRINTER(SignatureInfo)
  DECLARE_VERIFIER(SignatureInfo)

  static const int kReceiverOffset = Struct::kHeaderSize;
  static const int kArgsOffset     = kReceiverOffset + kPointerSize;
  static const int kSize           = kArgsOffset + kPointerSize;

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(SignatureInfo);
};


class TypeSwitchInfo: public Struct {
 public:
  DECL_ACCESSORS(types, Object)

  static inline TypeSwitchInfo* cast(Object* obj);

  // Dispatched behavior.
  DECLARE_PRINTER(TypeSwitchInfo)
  DECLARE_VERIFIER(TypeSwitchInfo)

  static const int kTypesOffset = Struct::kHeaderSize;
  static const int kSize        = kTypesOffset + kPointerSize;
};


#ifdef ENABLE_DEBUGGER_SUPPORT
// The DebugInfo class holds additional information for a function being
// debugged.
class DebugInfo: public Struct {
 public:
  // The shared function info for the source being debugged.
  DECL_ACCESSORS(shared, SharedFunctionInfo)
  // Code object for the original code.
  DECL_ACCESSORS(original_code, Code)
  // Code object for the patched code. This code object is the code object
  // currently active for the function.
  DECL_ACCESSORS(code, Code)
  // Fixed array holding status information for each active break point.
  DECL_ACCESSORS(break_points, FixedArray)

  // Check if there is a break point at a code position.
  bool HasBreakPoint(int code_position);
  // Get the break point info object for a code position.
  Object* GetBreakPointInfo(int code_position);
  // Clear a break point.
  static void ClearBreakPoint(Handle<DebugInfo> debug_info,
                              int code_position,
                              Handle<Object> break_point_object);
  // Set a break point.
  static void SetBreakPoint(Handle<DebugInfo> debug_info, int code_position,
                            int source_position, int statement_position,
                            Handle<Object> break_point_object);
  // Get the break point objects for a code position.
  Object* GetBreakPointObjects(int code_position);
  // Find the break point info holding this break point object.
  static Object* FindBreakPointInfo(Handle<DebugInfo> debug_info,
                                    Handle<Object> break_point_object);
  // Get the number of break points for this function.
  int GetBreakPointCount();

  static inline DebugInfo* cast(Object* obj);

  // Dispatched behavior.
  DECLARE_PRINTER(DebugInfo)
  DECLARE_VERIFIER(DebugInfo)

  static const int kSharedFunctionInfoIndex = Struct::kHeaderSize;
  static const int kOriginalCodeIndex = kSharedFunctionInfoIndex + kPointerSize;
  static const int kPatchedCodeIndex = kOriginalCodeIndex + kPointerSize;
  static const int kActiveBreakPointsCountIndex =
      kPatchedCodeIndex + kPointerSize;
  static const int kBreakPointsStateIndex =
      kActiveBreakPointsCountIndex + kPointerSize;
  static const int kSize = kBreakPointsStateIndex + kPointerSize;

 private:
  static const int kNoBreakPointInfo = -1;

  // Lookup the index in the break_points array for a code position.
  int GetBreakPointInfoIndex(int code_position);

  DISALLOW_IMPLICIT_CONSTRUCTORS(DebugInfo);
};


// The BreakPointInfo class holds information for break points set in a
// function. The DebugInfo object holds a BreakPointInfo object for each code
// position with one or more break points.
class BreakPointInfo: public Struct {
 public:
  // The position in the code for the break point.
  DECL_ACCESSORS(code_position, Smi)
  // The position in the source for the break position.
  DECL_ACCESSORS(source_position, Smi)
  // The position in the source for the last statement before this break
  // position.
  DECL_ACCESSORS(statement_position, Smi)
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
  // Get the number of break points for this code position.
  int GetBreakPointCount();

  static inline BreakPointInfo* cast(Object* obj);

  // Dispatched behavior.
  DECLARE_PRINTER(BreakPointInfo)
  DECLARE_VERIFIER(BreakPointInfo)

  static const int kCodePositionIndex = Struct::kHeaderSize;
  static const int kSourcePositionIndex = kCodePositionIndex + kPointerSize;
  static const int kStatementPositionIndex =
      kSourcePositionIndex + kPointerSize;
  static const int kBreakPointObjectsIndex =
      kStatementPositionIndex + kPointerSize;
  static const int kSize = kBreakPointObjectsIndex + kPointerSize;

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(BreakPointInfo);
};
#endif  // ENABLE_DEBUGGER_SUPPORT


#undef DECL_BOOLEAN_ACCESSORS
#undef DECL_ACCESSORS
#undef DECLARE_VERIFIER

#define VISITOR_SYNCHRONIZATION_TAGS_LIST(V)                            \
  V(kStringTable, "string_table", "(Internalized strings)")             \
  V(kExternalStringsTable, "external_strings_table", "(External strings)") \
  V(kStrongRootList, "strong_root_list", "(Strong roots)")              \
  V(kSmiRootList, "smi_root_list", "(Smi roots)")                       \
  V(kInternalizedString, "internalized_string", "(Internal string)")    \
  V(kBootstrapper, "bootstrapper", "(Bootstrapper)")                    \
  V(kTop, "top", "(Isolate)")                                           \
  V(kRelocatable, "relocatable", "(Relocatable)")                       \
  V(kDebug, "debug", "(Debugger)")                                      \
  V(kCompilationCache, "compilationcache", "(Compilation cache)")       \
  V(kHandleScope, "handlescope", "(Handle scope)")                      \
  V(kBuiltins, "builtins", "(Builtins)")                                \
  V(kGlobalHandles, "globalhandles", "(Global handles)")                \
  V(kEternalHandles, "eternalhandles", "(Eternal handles)")             \
  V(kThreadManager, "threadmanager", "(Thread manager)")                \
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

  // Visits the resource of an ASCII or two-byte string.
  virtual void VisitExternalAsciiString(
      v8::String::ExternalAsciiStringResource** resource) {}
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

  // Visits an external reference. The value may be modified on return.
  virtual void VisitExternalReference(Address* p) {}

  // Visits a handle that has an embedder-assigned class ID.
  virtual void VisitEmbedderReference(Object** p, uint16_t class_id) {}

  // Intended for serialization/deserialization checking: insert, or
  // check for the presence of, a tag at this position in the stream.
  // Also used for marking up GC roots in heap snapshots.
  virtual void Synchronize(VisitorSynchronization::SyncTag tag) {}
};


class StructBodyDescriptor : public
  FlexibleBodyDescriptor<HeapObject::kHeaderSize> {
 public:
  static inline int SizeOf(Map* map, HeapObject* object) {
    return map->instance_size();
  }
};


// BooleanBit is a helper class for setting and getting a bit in an
// integer or Smi.
class BooleanBit : public AllStatic {
 public:
  static inline bool get(Smi* smi, int bit_position) {
    return get(smi->value(), bit_position);
  }

  static inline bool get(int value, int bit_position) {
    return (value & (1 << bit_position)) != 0;
  }

  static inline Smi* set(Smi* smi, int bit_position, bool v) {
    return Smi::FromInt(set(smi->value(), bit_position, v));
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

} }  // namespace v8::internal

#endif  // V8_OBJECTS_H_
