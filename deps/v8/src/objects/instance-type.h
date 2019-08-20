// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_INSTANCE_TYPE_H_
#define V8_OBJECTS_INSTANCE_TYPE_H_

#include "src/objects/elements-kind.h"
#include "src/objects/objects-definitions.h"

// Has to be the last include (doesn't have include guards):
#include "src/objects/object-macros.h"

#include "torque-generated/instance-types-tq.h"

namespace v8 {
namespace internal {

// We use the full 16 bits of the instance_type field to encode heap object
// instance types. All the high-order bits (bits 6-15) are cleared if the object
// is a string, and contain set bits if it is not a string.
const uint32_t kIsNotStringMask = ~((1 << 6) - 1);
const uint32_t kStringTag = 0x0;

// For strings, bits 0-2 indicate the representation of the string. In
// particular, bit 0 indicates whether the string is direct or indirect.
const uint32_t kStringRepresentationMask = (1 << 3) - 1;
enum StringRepresentationTag {
  kSeqStringTag = 0x0,
  kConsStringTag = 0x1,
  kExternalStringTag = 0x2,
  kSlicedStringTag = 0x3,
  kThinStringTag = 0x5
};
const uint32_t kIsIndirectStringMask = 1 << 0;
const uint32_t kIsIndirectStringTag = 1 << 0;
// NOLINTNEXTLINE(runtime/references) (false positive)
STATIC_ASSERT((kSeqStringTag & kIsIndirectStringMask) == 0);
// NOLINTNEXTLINE(runtime/references) (false positive)
STATIC_ASSERT((kExternalStringTag & kIsIndirectStringMask) == 0);
// NOLINTNEXTLINE(runtime/references) (false positive)
STATIC_ASSERT((kConsStringTag & kIsIndirectStringMask) == kIsIndirectStringTag);
// NOLINTNEXTLINE(runtime/references) (false positive)
STATIC_ASSERT((kSlicedStringTag & kIsIndirectStringMask) ==
              kIsIndirectStringTag);
// NOLINTNEXTLINE(runtime/references) (false positive)
STATIC_ASSERT((kThinStringTag & kIsIndirectStringMask) == kIsIndirectStringTag);

// For strings, bit 3 indicates whether the string consists of two-byte
// characters or one-byte characters.
const uint32_t kStringEncodingMask = 1 << 3;
const uint32_t kTwoByteStringTag = 0;
const uint32_t kOneByteStringTag = 1 << 3;

// For strings, bit 4 indicates whether the data pointer of an external string
// is cached. Note that the string representation is expected to be
// kExternalStringTag.
const uint32_t kUncachedExternalStringMask = 1 << 4;
const uint32_t kUncachedExternalStringTag = 1 << 4;

// For strings, bit 5 indicates that the string is internalized (if not set) or
// isn't (if set).
const uint32_t kIsNotInternalizedMask = 1 << 5;
const uint32_t kNotInternalizedTag = 1 << 5;
const uint32_t kInternalizedTag = 0;

// A ConsString with an empty string as the right side is a candidate
// for being shortcut by the garbage collector. We don't allocate any
// non-flat internalized strings, so we do not shortcut them thereby
// avoiding turning internalized strings into strings. The bit-masks
// below contain the internalized bit as additional safety.
// See heap.cc, mark-compact.cc and objects-visiting.cc.
const uint32_t kShortcutTypeMask =
    kIsNotStringMask | kIsNotInternalizedMask | kStringRepresentationMask;
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
  UNCACHED_EXTERNAL_INTERNALIZED_STRING_TYPE =
      EXTERNAL_INTERNALIZED_STRING_TYPE | kUncachedExternalStringTag |
      kInternalizedTag,
  UNCACHED_EXTERNAL_ONE_BYTE_INTERNALIZED_STRING_TYPE =
      EXTERNAL_ONE_BYTE_INTERNALIZED_STRING_TYPE | kUncachedExternalStringTag |
      kInternalizedTag,
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
  UNCACHED_EXTERNAL_STRING_TYPE =
      UNCACHED_EXTERNAL_INTERNALIZED_STRING_TYPE | kNotInternalizedTag,
  UNCACHED_EXTERNAL_ONE_BYTE_STRING_TYPE =
      UNCACHED_EXTERNAL_ONE_BYTE_INTERNALIZED_STRING_TYPE | kNotInternalizedTag,
  THIN_STRING_TYPE = kTwoByteStringTag | kThinStringTag | kNotInternalizedTag,
  THIN_ONE_BYTE_STRING_TYPE =
      kOneByteStringTag | kThinStringTag | kNotInternalizedTag,

  // Non-string names
  SYMBOL_TYPE =
      1 + (kIsNotInternalizedMask | kUncachedExternalStringMask |
           kStringEncodingMask |
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
  FIXED_DOUBLE_ARRAY_TYPE,
  FEEDBACK_METADATA_TYPE,
  FILLER_TYPE,  // LAST_DATA_TYPE

  // Structs.
  ACCESS_CHECK_INFO_TYPE,
  ACCESSOR_INFO_TYPE,
  ACCESSOR_PAIR_TYPE,
  ALIASED_ARGUMENTS_ENTRY_TYPE,
  ALLOCATION_MEMENTO_TYPE,
  ARRAY_BOILERPLATE_DESCRIPTION_TYPE,
  ASM_WASM_DATA_TYPE,
  ASYNC_GENERATOR_REQUEST_TYPE,
  CLASS_POSITIONS_TYPE,
  DEBUG_INFO_TYPE,
  ENUM_CACHE_TYPE,
  FUNCTION_TEMPLATE_INFO_TYPE,
  FUNCTION_TEMPLATE_RARE_DATA_TYPE,
  INTERCEPTOR_INFO_TYPE,
  INTERPRETER_DATA_TYPE,
  OBJECT_TEMPLATE_INFO_TYPE,
  PROMISE_CAPABILITY_TYPE,
  PROMISE_REACTION_TYPE,
  PROTOTYPE_INFO_TYPE,
  SCRIPT_TYPE,
  SOURCE_POSITION_TABLE_WITH_FRAME_CACHE_TYPE,
  SOURCE_TEXT_MODULE_INFO_ENTRY_TYPE,
  STACK_FRAME_INFO_TYPE,
  STACK_TRACE_FRAME_TYPE,
  TEMPLATE_OBJECT_DESCRIPTION_TYPE,
  TUPLE2_TYPE,
  TUPLE3_TYPE,
  WASM_CAPI_FUNCTION_DATA_TYPE,
  WASM_DEBUG_INFO_TYPE,
  WASM_EXCEPTION_TAG_TYPE,
  WASM_EXPORTED_FUNCTION_DATA_TYPE,
  WASM_INDIRECT_FUNCTION_TABLE_TYPE,
  WASM_JS_FUNCTION_DATA_TYPE,

  CALLABLE_TASK_TYPE,  // FIRST_MICROTASK_TYPE
  CALLBACK_TASK_TYPE,
  PROMISE_FULFILL_REACTION_JOB_TASK_TYPE,
  PROMISE_REJECT_REACTION_JOB_TASK_TYPE,
  PROMISE_RESOLVE_THENABLE_JOB_TASK_TYPE,
  FINALIZATION_GROUP_CLEANUP_JOB_TASK_TYPE,  // LAST_MICROTASK_TYPE

#define MAKE_TORQUE_INSTANCE_TYPE(V) V,
  TORQUE_DEFINED_INSTANCE_TYPES(MAKE_TORQUE_INSTANCE_TYPE)
#undef MAKE_TORQUE_INSTANCE_TYPE

  // Modules
  SOURCE_TEXT_MODULE_TYPE,  // FIRST_MODULE_TYPE
  SYNTHETIC_MODULE_TYPE,    // LAST_MODULE_TYPE

  ALLOCATION_SITE_TYPE,
  EMBEDDER_DATA_ARRAY_TYPE,
  // FixedArrays.
  FIXED_ARRAY_TYPE,  // FIRST_FIXED_ARRAY_TYPE
  OBJECT_BOILERPLATE_DESCRIPTION_TYPE,
  CLOSURE_FEEDBACK_CELL_ARRAY_TYPE,
  HASH_TABLE_TYPE,  // FIRST_HASH_TABLE_TYPE
  ORDERED_HASH_MAP_TYPE,
  ORDERED_HASH_SET_TYPE,
  ORDERED_NAME_DICTIONARY_TYPE,
  NAME_DICTIONARY_TYPE,
  GLOBAL_DICTIONARY_TYPE,
  NUMBER_DICTIONARY_TYPE,
  SIMPLE_NUMBER_DICTIONARY_TYPE,
  STRING_TABLE_TYPE,
  EPHEMERON_HASH_TABLE_TYPE,  // LAST_HASH_TABLE_TYPE
  SCOPE_INFO_TYPE,
  SCRIPT_CONTEXT_TABLE_TYPE,  // LAST_FIXED_ARRAY_TYPE,

  // Contexts.
  AWAIT_CONTEXT_TYPE,  // FIRST_CONTEXT_TYPE
  BLOCK_CONTEXT_TYPE,
  CATCH_CONTEXT_TYPE,
  DEBUG_EVALUATE_CONTEXT_TYPE,
  EVAL_CONTEXT_TYPE,
  FUNCTION_CONTEXT_TYPE,
  MODULE_CONTEXT_TYPE,
  NATIVE_CONTEXT_TYPE,
  SCRIPT_CONTEXT_TYPE,
  WITH_CONTEXT_TYPE,  // LAST_CONTEXT_TYPE

  WEAK_FIXED_ARRAY_TYPE,  // FIRST_WEAK_FIXED_ARRAY_TYPE
  TRANSITION_ARRAY_TYPE,  // LAST_WEAK_FIXED_ARRAY_TYPE

  // Misc.
  CALL_HANDLER_INFO_TYPE,
  CELL_TYPE,
  CODE_DATA_CONTAINER_TYPE,
  DESCRIPTOR_ARRAY_TYPE,
  FEEDBACK_CELL_TYPE,
  FEEDBACK_VECTOR_TYPE,
  LOAD_HANDLER_TYPE,
  PREPARSE_DATA_TYPE,
  PROPERTY_ARRAY_TYPE,
  PROPERTY_CELL_TYPE,
  SHARED_FUNCTION_INFO_TYPE,
  SMALL_ORDERED_HASH_MAP_TYPE,
  SMALL_ORDERED_HASH_SET_TYPE,
  SMALL_ORDERED_NAME_DICTIONARY_TYPE,
  STORE_HANDLER_TYPE,
  UNCOMPILED_DATA_WITHOUT_PREPARSE_DATA_TYPE,
  UNCOMPILED_DATA_WITH_PREPARSE_DATA_TYPE,
  WEAK_ARRAY_LIST_TYPE,
  WEAK_CELL_TYPE,

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
  JS_PRIMITIVE_WRAPPER_TYPE,            // LAST_CUSTOM_ELEMENTS_RECEIVER
  // Like JS_OBJECT_TYPE, but created from API function.
  JS_API_OBJECT_TYPE = 0x0420,
  JS_OBJECT_TYPE,
  JS_ARGUMENTS_TYPE,
  JS_ARRAY_BUFFER_TYPE,
  JS_ARRAY_ITERATOR_TYPE,
  JS_ARRAY_TYPE,
  JS_ASYNC_FROM_SYNC_ITERATOR_TYPE,
  JS_ASYNC_FUNCTION_OBJECT_TYPE,
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
  JS_WEAK_REF_TYPE,
  JS_FINALIZATION_GROUP_CLEANUP_ITERATOR_TYPE,
  JS_FINALIZATION_GROUP_TYPE,
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
  JS_INTL_SEGMENT_ITERATOR_TYPE,
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
  FIRST_STRING_TYPE = FIRST_TYPE,
  FIRST_NAME_TYPE = FIRST_STRING_TYPE,
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
  LAST_FIXED_ARRAY_TYPE = SCRIPT_CONTEXT_TABLE_TYPE,
  // Boundaries for testing if given HeapObject is a subclass of HashTable
  FIRST_HASH_TABLE_TYPE = HASH_TABLE_TYPE,
  LAST_HASH_TABLE_TYPE = EPHEMERON_HASH_TABLE_TYPE,
  // Boundaries for testing if given HeapObject is a subclass of WeakFixedArray.
  FIRST_WEAK_FIXED_ARRAY_TYPE = WEAK_FIXED_ARRAY_TYPE,
  LAST_WEAK_FIXED_ARRAY_TYPE = TRANSITION_ARRAY_TYPE,
  // Boundaries for testing if given HeapObject is a Context
  FIRST_CONTEXT_TYPE = AWAIT_CONTEXT_TYPE,
  LAST_CONTEXT_TYPE = WITH_CONTEXT_TYPE,
  // Boundaries for testing if given HeapObject is a subclass of Microtask.
  FIRST_MICROTASK_TYPE = CALLABLE_TASK_TYPE,
  LAST_MICROTASK_TYPE = FINALIZATION_GROUP_CLEANUP_JOB_TASK_TYPE,
  // Boundaries of module record types
  FIRST_MODULE_TYPE = SOURCE_TEXT_MODULE_TYPE,
  LAST_MODULE_TYPE = SYNTHETIC_MODULE_TYPE,
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
  LAST_CUSTOM_ELEMENTS_RECEIVER = JS_PRIMITIVE_WRAPPER_TYPE,

  FIRST_SET_ITERATOR_TYPE = JS_SET_KEY_VALUE_ITERATOR_TYPE,
  LAST_SET_ITERATOR_TYPE = JS_SET_VALUE_ITERATOR_TYPE,

  FIRST_MAP_ITERATOR_TYPE = JS_MAP_KEY_ITERATOR_TYPE,
  LAST_MAP_ITERATOR_TYPE = JS_MAP_VALUE_ITERATOR_TYPE,
};

// This constant is defined outside of the InstanceType enum because the
// string instance types are sparce and there's no such a string instance type.
// But it's still useful for range checks to have such a value.
constexpr InstanceType LAST_STRING_TYPE =
    static_cast<InstanceType>(FIRST_NONSTRING_TYPE - 1);

// NOLINTNEXTLINE(runtime/references) (false positive)
STATIC_ASSERT((FIRST_NONSTRING_TYPE & kIsNotStringMask) != kStringTag);
STATIC_ASSERT(JS_OBJECT_TYPE == Internals::kJSObjectType);
STATIC_ASSERT(JS_API_OBJECT_TYPE == Internals::kJSApiObjectType);
STATIC_ASSERT(JS_SPECIAL_API_OBJECT_TYPE == Internals::kJSSpecialApiObjectType);
STATIC_ASSERT(FIRST_NONSTRING_TYPE == Internals::kFirstNonstringType);
STATIC_ASSERT(ODDBALL_TYPE == Internals::kOddballType);
STATIC_ASSERT(FOREIGN_TYPE == Internals::kForeignType);

// Make sure it doesn't matter whether we sign-extend or zero-extend these
// values, because Torque treats InstanceType as signed.
STATIC_ASSERT(LAST_TYPE < 1 << 15);

V8_EXPORT_PRIVATE std::ostream& operator<<(std::ostream& os,
                                           InstanceType instance_type);

// List of object types that have a single unique instance type.
#define INSTANCE_TYPE_CHECKERS_SINGLE_BASE(V)                                \
  V(AllocationSite, ALLOCATION_SITE_TYPE)                                    \
  V(BigInt, BIGINT_TYPE)                                                     \
  V(ObjectBoilerplateDescription, OBJECT_BOILERPLATE_DESCRIPTION_TYPE)       \
  V(BreakPoint, TUPLE2_TYPE)                                                 \
  V(BreakPointInfo, TUPLE2_TYPE)                                             \
  V(ByteArray, BYTE_ARRAY_TYPE)                                              \
  V(BytecodeArray, BYTECODE_ARRAY_TYPE)                                      \
  V(CallHandlerInfo, CALL_HANDLER_INFO_TYPE)                                 \
  V(Cell, CELL_TYPE)                                                         \
  V(Code, CODE_TYPE)                                                         \
  V(CachedTemplateObject, TUPLE3_TYPE)                                       \
  V(CodeDataContainer, CODE_DATA_CONTAINER_TYPE)                             \
  V(CoverageInfo, FIXED_ARRAY_TYPE)                                          \
  V(ClosureFeedbackCellArray, CLOSURE_FEEDBACK_CELL_ARRAY_TYPE)              \
  V(DescriptorArray, DESCRIPTOR_ARRAY_TYPE)                                  \
  V(EmbedderDataArray, EMBEDDER_DATA_ARRAY_TYPE)                             \
  V(EphemeronHashTable, EPHEMERON_HASH_TABLE_TYPE)                           \
  V(FeedbackCell, FEEDBACK_CELL_TYPE)                                        \
  V(FeedbackMetadata, FEEDBACK_METADATA_TYPE)                                \
  V(FeedbackVector, FEEDBACK_VECTOR_TYPE)                                    \
  V(FixedArrayExact, FIXED_ARRAY_TYPE)                                       \
  V(FixedDoubleArray, FIXED_DOUBLE_ARRAY_TYPE)                               \
  V(Foreign, FOREIGN_TYPE)                                                   \
  V(FreeSpace, FREE_SPACE_TYPE)                                              \
  V(GlobalDictionary, GLOBAL_DICTIONARY_TYPE)                                \
  V(HeapNumber, HEAP_NUMBER_TYPE)                                            \
  V(JSArgumentsObject, JS_ARGUMENTS_TYPE)                                    \
  V(JSArgumentsObjectWithLength, JS_ARGUMENTS_TYPE)                          \
  V(JSArray, JS_ARRAY_TYPE)                                                  \
  V(JSArrayBuffer, JS_ARRAY_BUFFER_TYPE)                                     \
  V(JSArrayIterator, JS_ARRAY_ITERATOR_TYPE)                                 \
  V(JSAsyncFromSyncIterator, JS_ASYNC_FROM_SYNC_ITERATOR_TYPE)               \
  V(JSAsyncFunctionObject, JS_ASYNC_FUNCTION_OBJECT_TYPE)                    \
  V(JSAsyncGeneratorObject, JS_ASYNC_GENERATOR_OBJECT_TYPE)                  \
  V(JSBoundFunction, JS_BOUND_FUNCTION_TYPE)                                 \
  V(JSContextExtensionObject, JS_CONTEXT_EXTENSION_OBJECT_TYPE)              \
  V(JSDataView, JS_DATA_VIEW_TYPE)                                           \
  V(JSDate, JS_DATE_TYPE)                                                    \
  V(JSError, JS_ERROR_TYPE)                                                  \
  V(JSFinalizationGroup, JS_FINALIZATION_GROUP_TYPE)                         \
  V(JSFinalizationGroupCleanupIterator,                                      \
    JS_FINALIZATION_GROUP_CLEANUP_ITERATOR_TYPE)                             \
  V(JSFunction, JS_FUNCTION_TYPE)                                            \
  V(JSGlobalObject, JS_GLOBAL_OBJECT_TYPE)                                   \
  V(JSGlobalProxy, JS_GLOBAL_PROXY_TYPE)                                     \
  V(JSMap, JS_MAP_TYPE)                                                      \
  V(JSMessageObject, JS_MESSAGE_OBJECT_TYPE)                                 \
  V(JSModuleNamespace, JS_MODULE_NAMESPACE_TYPE)                             \
  V(JSPrimitiveWrapper, JS_PRIMITIVE_WRAPPER_TYPE)                           \
  V(JSPromise, JS_PROMISE_TYPE)                                              \
  V(JSProxy, JS_PROXY_TYPE)                                                  \
  V(JSRegExp, JS_REGEXP_TYPE)                                                \
  V(JSRegExpResult, JS_ARRAY_TYPE)                                           \
  V(JSRegExpStringIterator, JS_REGEXP_STRING_ITERATOR_TYPE)                  \
  V(JSSet, JS_SET_TYPE)                                                      \
  V(JSStringIterator, JS_STRING_ITERATOR_TYPE)                               \
  V(JSTypedArray, JS_TYPED_ARRAY_TYPE)                                       \
  V(JSWeakMap, JS_WEAK_MAP_TYPE)                                             \
  V(JSWeakRef, JS_WEAK_REF_TYPE)                                             \
  V(JSWeakSet, JS_WEAK_SET_TYPE)                                             \
  V(LoadHandler, LOAD_HANDLER_TYPE)                                          \
  V(Map, MAP_TYPE)                                                           \
  V(MutableHeapNumber, MUTABLE_HEAP_NUMBER_TYPE)                             \
  V(NameDictionary, NAME_DICTIONARY_TYPE)                                    \
  V(NativeContext, NATIVE_CONTEXT_TYPE)                                      \
  V(NumberDictionary, NUMBER_DICTIONARY_TYPE)                                \
  V(Oddball, ODDBALL_TYPE)                                                   \
  V(OrderedHashMap, ORDERED_HASH_MAP_TYPE)                                   \
  V(OrderedHashSet, ORDERED_HASH_SET_TYPE)                                   \
  V(OrderedNameDictionary, ORDERED_NAME_DICTIONARY_TYPE)                     \
  V(PreparseData, PREPARSE_DATA_TYPE)                                        \
  V(PropertyArray, PROPERTY_ARRAY_TYPE)                                      \
  V(PropertyCell, PROPERTY_CELL_TYPE)                                        \
  V(PropertyDescriptorObject, FIXED_ARRAY_TYPE)                              \
  V(ScopeInfo, SCOPE_INFO_TYPE)                                              \
  V(ScriptContextTable, SCRIPT_CONTEXT_TABLE_TYPE)                           \
  V(SharedFunctionInfo, SHARED_FUNCTION_INFO_TYPE)                           \
  V(SimpleNumberDictionary, SIMPLE_NUMBER_DICTIONARY_TYPE)                   \
  V(SmallOrderedHashMap, SMALL_ORDERED_HASH_MAP_TYPE)                        \
  V(SmallOrderedHashSet, SMALL_ORDERED_HASH_SET_TYPE)                        \
  V(SmallOrderedNameDictionary, SMALL_ORDERED_NAME_DICTIONARY_TYPE)          \
  V(SourceTextModule, SOURCE_TEXT_MODULE_TYPE)                               \
  V(StoreHandler, STORE_HANDLER_TYPE)                                        \
  V(StringTable, STRING_TABLE_TYPE)                                          \
  V(Symbol, SYMBOL_TYPE)                                                     \
  V(SyntheticModule, SYNTHETIC_MODULE_TYPE)                                  \
  V(TransitionArray, TRANSITION_ARRAY_TYPE)                                  \
  V(UncompiledDataWithoutPreparseData,                                       \
    UNCOMPILED_DATA_WITHOUT_PREPARSE_DATA_TYPE)                              \
  V(UncompiledDataWithPreparseData, UNCOMPILED_DATA_WITH_PREPARSE_DATA_TYPE) \
  V(WasmExceptionObject, WASM_EXCEPTION_TYPE)                                \
  V(WasmGlobalObject, WASM_GLOBAL_TYPE)                                      \
  V(WasmInstanceObject, WASM_INSTANCE_TYPE)                                  \
  V(WasmMemoryObject, WASM_MEMORY_TYPE)                                      \
  V(WasmModuleObject, WASM_MODULE_TYPE)                                      \
  V(WasmTableObject, WASM_TABLE_TYPE)                                        \
  V(WeakArrayList, WEAK_ARRAY_LIST_TYPE)                                     \
  V(WeakCell, WEAK_CELL_TYPE)
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
  V(JSSegmentIterator, JS_INTL_SEGMENT_ITERATOR_TYPE)        \
  V(JSSegmenter, JS_INTL_SEGMENTER_TYPE)

#else

#define INSTANCE_TYPE_CHECKERS_SINGLE(V) INSTANCE_TYPE_CHECKERS_SINGLE_BASE(V)

#endif  // V8_INTL_SUPPORT

#define INSTANCE_TYPE_CHECKERS_RANGE(V)                             \
  V(Context, FIRST_CONTEXT_TYPE, LAST_CONTEXT_TYPE)                 \
  V(FixedArray, FIRST_FIXED_ARRAY_TYPE, LAST_FIXED_ARRAY_TYPE)      \
  V(HashTable, FIRST_HASH_TABLE_TYPE, LAST_HASH_TABLE_TYPE)         \
  V(JSMapIterator, FIRST_MAP_ITERATOR_TYPE, LAST_MAP_ITERATOR_TYPE) \
  V(JSSetIterator, FIRST_SET_ITERATOR_TYPE, LAST_SET_ITERATOR_TYPE) \
  V(Microtask, FIRST_MICROTASK_TYPE, LAST_MICROTASK_TYPE)           \
  V(Module, FIRST_MODULE_TYPE, LAST_MODULE_TYPE)                    \
  V(Name, FIRST_NAME_TYPE, LAST_NAME_TYPE)                          \
  V(String, FIRST_STRING_TYPE, LAST_STRING_TYPE)                    \
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

}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_INSTANCE_TYPE_H_
