// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_INSTANCE_TYPE_H_
#define V8_OBJECTS_INSTANCE_TYPE_H_

#include "src/objects/elements-kind.h"
#include "src/objects/objects-definitions.h"

// Has to be the last include (doesn't have include guards):
#include "src/objects/object-macros.h"
#include "torque-generated/instance-types.h"

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
STATIC_ASSERT((kSeqStringTag & kIsIndirectStringMask) == 0);
STATIC_ASSERT((kExternalStringTag & kIsIndirectStringMask) == 0);
STATIC_ASSERT((kConsStringTag & kIsIndirectStringMask) == kIsIndirectStringTag);
STATIC_ASSERT((kSlicedStringTag & kIsIndirectStringMask) ==
              kIsIndirectStringTag);
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
  BIGINT_TYPE = BIG_INT_BASE_TYPE,
};

// This constant is defined outside of the InstanceType enum because the
// string instance types are sparse and there's no such string instance type.
// But it's still useful for range checks to have such a value.
constexpr InstanceType LAST_STRING_TYPE =
    static_cast<InstanceType>(FIRST_NONSTRING_TYPE - 1);

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
// clang-format off
#define CHECK_INSTANCE_TYPE(TYPE)                                          \
  STATIC_ASSERT((TYPE >= FIRST_JS_RECEIVER_TYPE &&                         \
                 TYPE <= LAST_SPECIAL_RECEIVER_TYPE) ==                    \
                (IF_WASM(EXPAND, TYPE == WASM_STRUCT_TYPE ||               \
                                 TYPE == WASM_ARRAY_TYPE ||)               \
                 TYPE == JS_PROXY_TYPE || TYPE == JS_GLOBAL_OBJECT_TYPE || \
                 TYPE == JS_GLOBAL_PROXY_TYPE ||                           \
                 TYPE == JS_MODULE_NAMESPACE_TYPE ||                       \
                 TYPE == JS_SPECIAL_API_OBJECT_TYPE));                     \
  STATIC_ASSERT((TYPE >= FIRST_JS_RECEIVER_TYPE &&                         \
                 TYPE <= LAST_CUSTOM_ELEMENTS_RECEIVER) ==                 \
                (IF_WASM(EXPAND, TYPE == WASM_STRUCT_TYPE ||               \
                                 TYPE == WASM_ARRAY_TYPE ||)               \
                 TYPE == JS_PROXY_TYPE || TYPE == JS_GLOBAL_OBJECT_TYPE || \
                 TYPE == JS_GLOBAL_PROXY_TYPE ||                           \
                 TYPE == JS_MODULE_NAMESPACE_TYPE ||                       \
                 TYPE == JS_SPECIAL_API_OBJECT_TYPE ||                     \
                 TYPE == JS_PRIMITIVE_WRAPPER_TYPE));
// clang-format on
TORQUE_ASSIGNED_INSTANCE_TYPE_LIST(CHECK_INSTANCE_TYPE)
#undef CHECK_INSTANCE_TYPE

// Make sure it doesn't matter whether we sign-extend or zero-extend these
// values, because Torque treats InstanceType as signed.
STATIC_ASSERT(LAST_TYPE < 1 << 15);

V8_EXPORT_PRIVATE std::ostream& operator<<(std::ostream& os,
                                           InstanceType instance_type);

// List of object types that have a single unique instance type.
#define INSTANCE_TYPE_CHECKERS_SINGLE(V)           \
  TORQUE_INSTANCE_CHECKERS_SINGLE_FULLY_DEFINED(V) \
  TORQUE_INSTANCE_CHECKERS_SINGLE_ONLY_DECLARED(V) \
  V(BigInt, BIGINT_TYPE)                           \
  V(FixedArrayExact, FIXED_ARRAY_TYPE)

#define INSTANCE_TYPE_CHECKERS_RANGE(V)           \
  TORQUE_INSTANCE_CHECKERS_RANGE_FULLY_DEFINED(V) \
  TORQUE_INSTANCE_CHECKERS_RANGE_ONLY_DECLARED(V)

#define INSTANCE_TYPE_CHECKERS_CUSTOM(V) \
  V(ExternalString)                      \
  V(InternalizedString)

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

#undef IS_TYPE_FUNCTION_DECL
}  // namespace InstanceTypeChecker

// This list must contain only maps that are shared by all objects of their
// instance type.
#define UNIQUE_INSTANCE_TYPE_MAP_LIST_GENERATOR(V, _)                          \
  V(_, AccessorInfoMap, accessor_info_map, AccessorInfo)                       \
  V(_, AccessorPairMap, accessor_pair_map, AccessorPair)                       \
  V(_, AllocationMementoMap, allocation_memento_map, AllocationMemento)        \
  V(_, ArrayBoilerplateDescriptionMap, array_boilerplate_description_map,      \
    ArrayBoilerplateDescription)                                               \
  V(_, BreakPointMap, break_point_map, BreakPoint)                             \
  V(_, BreakPointInfoMap, break_point_info_map, BreakPointInfo)                \
  V(_, CachedTemplateObjectMap, cached_template_object_map,                    \
    CachedTemplateObject)                                                      \
  V(_, CellMap, cell_map, Cell)                                                \
  V(_, WeakCellMap, weak_cell_map, WeakCell)                                   \
  V(_, CodeMap, code_map, Code)                                                \
  V(_, CodeDataContainerMap, code_data_container_map, CodeDataContainer)       \
  V(_, CoverageInfoMap, coverage_info_map, CoverageInfo)                       \
  V(_, DebugInfoMap, debug_info_map, DebugInfo)                                \
  V(_, FeedbackVectorMap, feedback_vector_map, FeedbackVector)                 \
  V(_, FixedDoubleArrayMap, fixed_double_array_map, FixedDoubleArray)          \
  V(_, FunctionTemplateInfoMap, function_template_info_map,                    \
    FunctionTemplateInfo)                                                      \
  V(_, HeapNumberMap, heap_number_map, HeapNumber)                             \
  V(_, MegaDomHandlerMap, mega_dom_handler_map, MegaDomHandler)                \
  V(_, MetaMap, meta_map, Map)                                                 \
  V(_, PreparseDataMap, preparse_data_map, PreparseData)                       \
  V(_, PrototypeInfoMap, prototype_info_map, PrototypeInfo)                    \
  V(_, SharedFunctionInfoMap, shared_function_info_map, SharedFunctionInfo)    \
  V(_, SmallOrderedHashSetMap, small_ordered_hash_set_map,                     \
    SmallOrderedHashSet)                                                       \
  V(_, SmallOrderedHashMapMap, small_ordered_hash_map_map,                     \
    SmallOrderedHashMap)                                                       \
  V(_, SmallOrderedNameDictionaryMap, small_ordered_name_dictionary_map,       \
    SmallOrderedNameDictionary)                                                \
  V(_, SwissNameDictionaryMap, swiss_name_dictionary_map, SwissNameDictionary) \
  V(_, SymbolMap, symbol_map, Symbol)                                          \
  V(_, TransitionArrayMap, transition_array_map, TransitionArray)              \
  V(_, Tuple2Map, tuple2_map, Tuple2)                                          \
  V(_, WeakFixedArrayMap, weak_fixed_array_map, WeakFixedArray)                \
  TORQUE_DEFINED_MAP_CSA_LIST_GENERATOR(V, _)

}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_INSTANCE_TYPE_H_
