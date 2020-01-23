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
  INTERNALIZED_STRING_TYPE =
      kTwoByteStringTag | kSeqStringTag | kInternalizedTag,
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

// Most instance types are defined in Torque, with the exception of the string
// types above. They are ordered by inheritance hierarchy so that we can easily
// use range checks to determine whether an object is an instance of a subclass
// of any type. There are a few more constraints specified in the Torque type
// definitions:
// - Some instance types are exposed in v8.h, so they are locked to specific
//   values to not unnecessarily change the ABI.
// - JSSpecialObject and JSCustomElementsObject are aligned with the beginning
//   of the JSObject range, so that we can use a larger range check from
//   FIRST_JS_RECEIVER_TYPE to the end of those ranges and include JSProxy too.
// - JSFunction is last, meaning we can use a single inequality check to
//   determine whether an instance type is within the range for any class in the
//   inheritance hierarchy of JSFunction. This includes commonly-checked classes
//   JSObject and JSReceiver.
#define MAKE_TORQUE_INSTANCE_TYPE(TYPE, value) TYPE = value,
  TORQUE_ASSIGNED_INSTANCE_TYPES(MAKE_TORQUE_INSTANCE_TYPE)
#undef MAKE_TORQUE_INSTANCE_TYPE

  // Pseudo-types
  FIRST_UNIQUE_NAME_TYPE = INTERNALIZED_STRING_TYPE,
  LAST_UNIQUE_NAME_TYPE = SYMBOL_TYPE,
  FIRST_NONSTRING_TYPE = SYMBOL_TYPE,
  // Boundary for testing JSReceivers that need special property lookup handling
  LAST_SPECIAL_RECEIVER_TYPE = LAST_JS_SPECIAL_OBJECT_TYPE,
  // Boundary case for testing JSReceivers that may have elements while having
  // an empty fixed array as elements backing store. This is true for string
  // wrappers.
  LAST_CUSTOM_ELEMENTS_RECEIVER = LAST_JS_CUSTOM_ELEMENTS_OBJECT_TYPE,

  // Convenient names for things where the generated name is awkward:
  FIRST_TYPE = FIRST_HEAP_OBJECT_TYPE,
  LAST_TYPE = LAST_HEAP_OBJECT_TYPE,
  FIRST_FUNCTION_TYPE = FIRST_JS_FUNCTION_OR_BOUND_FUNCTION_TYPE,
  LAST_FUNCTION_TYPE = LAST_JS_FUNCTION_OR_BOUND_FUNCTION_TYPE,
  BIGINT_TYPE = BIG_INT_BASE_TYPE,
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

// Verify that string types are all less than other types.
#define CHECK_STRING_RANGE(TYPE, ...) \
  STATIC_ASSERT(TYPE < FIRST_NONSTRING_TYPE);
STRING_TYPE_LIST(CHECK_STRING_RANGE)
#undef CHECK_STRING_RANGE
#define CHECK_NONSTRING_RANGE(TYPE) STATIC_ASSERT(TYPE >= FIRST_NONSTRING_TYPE);
TORQUE_ASSIGNED_INSTANCE_TYPE_LIST(CHECK_NONSTRING_RANGE)
#undef CHECK_NONSTRING_RANGE

// Two ranges don't cleanly follow the inheritance hierarchy. Here we ensure
// that only expected types fall within these ranges.
// - From FIRST_JS_RECEIVER_TYPE to LAST_SPECIAL_RECEIVER_TYPE should correspond
//   to the union type JSProxy | JSSpecialObject.
// - From FIRST_JS_RECEIVER_TYPE to LAST_CUSTOM_ELEMENTS_RECEIVER should
//   correspond to the union type JSProxy | JSCustomElementsObject.
// Note in particular that these ranges include all subclasses of JSReceiver
// that are not also subclasses of JSObject (currently only JSProxy).
#define CHECK_INSTANCE_TYPE(TYPE)                                          \
  STATIC_ASSERT((TYPE >= FIRST_JS_RECEIVER_TYPE &&                         \
                 TYPE <= LAST_SPECIAL_RECEIVER_TYPE) ==                    \
                (TYPE == JS_PROXY_TYPE || TYPE == JS_GLOBAL_OBJECT_TYPE || \
                 TYPE == JS_GLOBAL_PROXY_TYPE ||                           \
                 TYPE == JS_MODULE_NAMESPACE_TYPE ||                       \
                 TYPE == JS_SPECIAL_API_OBJECT_TYPE));                     \
  STATIC_ASSERT((TYPE >= FIRST_JS_RECEIVER_TYPE &&                         \
                 TYPE <= LAST_CUSTOM_ELEMENTS_RECEIVER) ==                 \
                (TYPE == JS_PROXY_TYPE || TYPE == JS_GLOBAL_OBJECT_TYPE || \
                 TYPE == JS_GLOBAL_PROXY_TYPE ||                           \
                 TYPE == JS_MODULE_NAMESPACE_TYPE ||                       \
                 TYPE == JS_SPECIAL_API_OBJECT_TYPE ||                     \
                 TYPE == JS_PRIMITIVE_WRAPPER_TYPE));
TORQUE_ASSIGNED_INSTANCE_TYPE_LIST(CHECK_INSTANCE_TYPE)
#undef CHECK_INSTANCE_TYPE

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
  V(JSArgumentsObject, JS_ARGUMENTS_OBJECT_TYPE)                             \
  V(JSArgumentsObjectWithLength, JS_ARGUMENTS_OBJECT_TYPE)                   \
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
  V(JSRegExp, JS_REG_EXP_TYPE)                                               \
  V(JSRegExpResult, JS_ARRAY_TYPE)                                           \
  V(JSRegExpResultIndices, JS_ARRAY_TYPE)                                    \
  V(JSRegExpStringIterator, JS_REG_EXP_STRING_ITERATOR_TYPE)                 \
  V(JSSet, JS_SET_TYPE)                                                      \
  V(JSStringIterator, JS_STRING_ITERATOR_TYPE)                               \
  V(JSTypedArray, JS_TYPED_ARRAY_TYPE)                                       \
  V(JSWeakMap, JS_WEAK_MAP_TYPE)                                             \
  V(JSWeakRef, JS_WEAK_REF_TYPE)                                             \
  V(JSWeakSet, JS_WEAK_SET_TYPE)                                             \
  V(LoadHandler, LOAD_HANDLER_TYPE)                                          \
  V(Map, MAP_TYPE)                                                           \
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
  V(WasmExceptionObject, WASM_EXCEPTION_OBJECT_TYPE)                         \
  V(WasmGlobalObject, WASM_GLOBAL_OBJECT_TYPE)                               \
  V(WasmInstanceObject, WASM_INSTANCE_OBJECT_TYPE)                           \
  V(WasmMemoryObject, WASM_MEMORY_OBJECT_TYPE)                               \
  V(WasmModuleObject, WASM_MODULE_OBJECT_TYPE)                               \
  V(WasmTableObject, WASM_TABLE_OBJECT_TYPE)                                 \
  V(WeakArrayList, WEAK_ARRAY_LIST_TYPE)                                     \
  V(WeakCell, WEAK_CELL_TYPE)
#ifdef V8_INTL_SUPPORT

#define INSTANCE_TYPE_CHECKERS_SINGLE(V)                \
  INSTANCE_TYPE_CHECKERS_SINGLE_BASE(V)                 \
  V(JSV8BreakIterator, JS_V8_BREAK_ITERATOR_TYPE)       \
  V(JSCollator, JS_COLLATOR_TYPE)                       \
  V(JSDateTimeFormat, JS_DATE_TIME_FORMAT_TYPE)         \
  V(JSListFormat, JS_LIST_FORMAT_TYPE)                  \
  V(JSLocale, JS_LOCALE_TYPE)                           \
  V(JSNumberFormat, JS_NUMBER_FORMAT_TYPE)              \
  V(JSPluralRules, JS_PLURAL_RULES_TYPE)                \
  V(JSRelativeTimeFormat, JS_RELATIVE_TIME_FORMAT_TYPE) \
  V(JSSegmentIterator, JS_SEGMENT_ITERATOR_TYPE)        \
  V(JSSegmenter, JS_SEGMENTER_TYPE)

#else

#define INSTANCE_TYPE_CHECKERS_SINGLE(V) INSTANCE_TYPE_CHECKERS_SINGLE_BASE(V)

#endif  // V8_INTL_SUPPORT

#define INSTANCE_TYPE_CHECKERS_RANGE(V)                                   \
  V(Context, FIRST_CONTEXT_TYPE, LAST_CONTEXT_TYPE)                       \
  V(FixedArray, FIRST_FIXED_ARRAY_TYPE, LAST_FIXED_ARRAY_TYPE)            \
  V(HashTable, FIRST_HASH_TABLE_TYPE, LAST_HASH_TABLE_TYPE)               \
  V(JSCustomElementsObject, FIRST_JS_CUSTOM_ELEMENTS_OBJECT_TYPE,         \
    LAST_JS_CUSTOM_ELEMENTS_OBJECT_TYPE)                                  \
  V(JSFunctionOrBoundFunction, FIRST_FUNCTION_TYPE, LAST_FUNCTION_TYPE)   \
  V(JSMapIterator, FIRST_JS_MAP_ITERATOR_TYPE, LAST_JS_MAP_ITERATOR_TYPE) \
  V(JSSetIterator, FIRST_JS_SET_ITERATOR_TYPE, LAST_JS_SET_ITERATOR_TYPE) \
  V(JSSpecialObject, FIRST_JS_SPECIAL_OBJECT_TYPE,                        \
    LAST_JS_SPECIAL_OBJECT_TYPE)                                          \
  V(Microtask, FIRST_MICROTASK_TYPE, LAST_MICROTASK_TYPE)                 \
  V(Module, FIRST_MODULE_TYPE, LAST_MODULE_TYPE)                          \
  V(Name, FIRST_NAME_TYPE, LAST_NAME_TYPE)                                \
  V(PrimitiveHeapObject, FIRST_PRIMITIVE_HEAP_OBJECT_TYPE,                \
    LAST_PRIMITIVE_HEAP_OBJECT_TYPE)                                      \
  V(String, FIRST_STRING_TYPE, LAST_STRING_TYPE)                          \
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
