// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_INSTANCE_TYPE_INL_H_
#define V8_OBJECTS_INSTANCE_TYPE_INL_H_

#include "src/base/bounds.h"
#include "src/execution/isolate-utils-inl.h"
#include "src/objects/instance-type.h"
#include "src/objects/map-inl.h"

// Has to be the last include (doesn't have include guards):
#include "src/objects/object-macros.h"

namespace v8 {
namespace internal {

namespace InstanceTypeChecker {

// Define type checkers for classes with single instance type.
#define INSTANCE_TYPE_CHECKER(type, forinstancetype)              \
  V8_INLINE constexpr bool Is##type(InstanceType instance_type) { \
    return instance_type == forinstancetype;                      \
  }

INSTANCE_TYPE_CHECKERS_SINGLE(INSTANCE_TYPE_CHECKER)
#undef INSTANCE_TYPE_CHECKER

// Checks if value is in range [lower_limit, higher_limit] using a single
// branch. Assumes that the input instance type is valid.
template <InstanceType lower_limit, InstanceType upper_limit>
struct InstanceRangeChecker {
  static constexpr bool Check(InstanceType value) {
    return base::IsInRange(value, lower_limit, upper_limit);
  }
};
template <InstanceType upper_limit>
struct InstanceRangeChecker<FIRST_TYPE, upper_limit> {
  static constexpr bool Check(InstanceType value) {
    DCHECK_LE(FIRST_TYPE, value);
    return value <= upper_limit;
  }
};
template <InstanceType lower_limit>
struct InstanceRangeChecker<lower_limit, LAST_TYPE> {
  static constexpr bool Check(InstanceType value) {
    DCHECK_GE(LAST_TYPE, value);
    return value >= lower_limit;
  }
};

// Define type checkers for classes with ranges of instance types.
#define INSTANCE_TYPE_CHECKER_RANGE(type, first_instance_type,             \
                                    last_instance_type)                    \
  V8_INLINE constexpr bool Is##type(InstanceType instance_type) {          \
    return InstanceRangeChecker<first_instance_type,                       \
                                last_instance_type>::Check(instance_type); \
  }
INSTANCE_TYPE_CHECKERS_RANGE(INSTANCE_TYPE_CHECKER_RANGE)
#undef INSTANCE_TYPE_CHECKER_RANGE

V8_INLINE constexpr bool IsHeapObject(InstanceType instance_type) {
  return true;
}

V8_INLINE constexpr bool IsInternalizedString(InstanceType instance_type) {
  STATIC_ASSERT(kNotInternalizedTag != 0);
  return (instance_type & (kIsNotStringMask | kIsNotInternalizedMask)) ==
         (kStringTag | kInternalizedTag);
}

V8_INLINE constexpr bool IsExternalString(InstanceType instance_type) {
  return (instance_type & (kIsNotStringMask | kStringRepresentationMask)) ==
         kExternalStringTag;
}

V8_INLINE constexpr bool IsThinString(InstanceType instance_type) {
  return (instance_type & kStringRepresentationMask) == kThinStringTag;
}

V8_INLINE constexpr bool IsFreeSpaceOrFiller(InstanceType instance_type) {
  return instance_type == FREE_SPACE_TYPE || instance_type == FILLER_TYPE;
}

}  // namespace InstanceTypeChecker

// INSTANCE_TYPE_CHECKERS macro defines some "types" that do not have
// respective C++ classes (see TypedArrayConstructor, FixedArrayExact) or
// the respective C++ counterpart is actually a template (see HashTable).
// So in order to be able to customize IsType() implementations for specific
// types, we declare a parallel set of "types" that can be compared using
// std::is_same<>.
namespace InstanceTypeTraits {

#define DECL_TYPE(type, ...) class type;
INSTANCE_TYPE_CHECKERS(DECL_TYPE)
TORQUE_INSTANCE_CHECKERS_MULTIPLE_FULLY_DEFINED(DECL_TYPE)
TORQUE_INSTANCE_CHECKERS_MULTIPLE_ONLY_DECLARED(DECL_TYPE)
HEAP_OBJECT_TYPE_LIST(DECL_TYPE)
#undef DECL_TYPE

}  // namespace InstanceTypeTraits

#define TYPE_CHECKER(type, ...)                                               \
  bool HeapObject::Is##type() const {                                         \
    PtrComprCageBase cage_base = GetPtrComprCageBase(*this);                  \
    return HeapObject::Is##type(cage_base);                                   \
  }                                                                           \
  /* The cage_base passed here is supposed to be the base of the pointer */   \
  /* compression cage where the Map space is allocated. */                    \
  /* However when external code space is enabled it's not always the case */  \
  /* yet and the predicate has to work if the cage_base corresponds to the */ \
  /* cage containing external code space.  */                                 \
  /* TODO(v8:11880): Ensure that the cage_base value always corresponds to */ \
  /* the main pointer compression cage. */                                    \
  bool HeapObject::Is##type(PtrComprCageBase cage_base) const {               \
    if (V8_EXTERNAL_CODE_SPACE_BOOL) {                                        \
      if (IsCodeObject(*this)) {                                              \
        /* Code space contains only Code objects and free space fillers. */   \
        if (std::is_same<InstanceTypeTraits::type,                            \
                         InstanceTypeTraits::Code>::value ||                  \
            std::is_same<InstanceTypeTraits::type,                            \
                         InstanceTypeTraits::FreeSpace>::value ||             \
            std::is_same<InstanceTypeTraits::type,                            \
                         InstanceTypeTraits::FreeSpaceOrFiller>::value) {     \
          /* Code space objects are never read-only, so it's safe to query */ \
          /* heap value in order to compute proper cage base. */              \
          Heap* heap = GetHeapFromWritableObject(*this);                      \
          Map map_object = map(Isolate::FromHeap(heap));                      \
          return InstanceTypeChecker::Is##type(map_object.instance_type());   \
        }                                                                     \
        /* For all the other queries we can return false. */                  \
        return false;                                                         \
      }                                                                       \
      /* Fallback to checking map instance type. */                           \
    }                                                                         \
    Map map_object = map(cage_base);                                          \
    return InstanceTypeChecker::Is##type(map_object.instance_type());         \
  }

// TODO(v8:7786): For instance types that have a single map instance on the
// roots, and when that map is a embedded in the binary, compare against the map
// pointer rather than looking up the instance type.
INSTANCE_TYPE_CHECKERS(TYPE_CHECKER)
#undef TYPE_CHECKER

}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_INSTANCE_TYPE_INL_H_
