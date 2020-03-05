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
#define INSTANCE_TYPE_CHECKERS_SINGLE(V)           \
  TORQUE_INSTANCE_CHECKERS_SINGLE_FULLY_DEFINED(V) \
  TORQUE_INSTANCE_CHECKERS_SINGLE_ONLY_DECLARED(V) \
  V(BigInt, BIGINT_TYPE)                           \
  V(CoverageInfo, FIXED_ARRAY_TYPE)                \
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

}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_INSTANCE_TYPE_H_
