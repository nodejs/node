// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_INSTANCE_TYPE_INL_H_
#define V8_OBJECTS_INSTANCE_TYPE_INL_H_

#include "src/objects/map-inl.h"
#include "src/utils/utils.h"

// Has to be the last include (doesn't have include guards):
#include "src/objects/object-macros.h"

namespace v8 {
namespace internal {

namespace InstanceTypeChecker {

// Define type checkers for classes with single instance type.
INSTANCE_TYPE_CHECKERS_SINGLE(INSTANCE_TYPE_CHECKER)

#define STRUCT_INSTANCE_TYPE_CHECKER(TYPE, Name, name) \
  INSTANCE_TYPE_CHECKER(Name, TYPE)
STRUCT_LIST(STRUCT_INSTANCE_TYPE_CHECKER)
#undef STRUCT_INSTANCE_TYPE_CHECKER

// Define type checkers for classes with ranges of instance types.
#define INSTANCE_TYPE_CHECKER_RANGE(type, first_instance_type,                \
                                    last_instance_type)                       \
  V8_INLINE bool Is##type(InstanceType instance_type) {                       \
    return IsInRange(instance_type, first_instance_type, last_instance_type); \
  }
INSTANCE_TYPE_CHECKERS_RANGE(INSTANCE_TYPE_CHECKER_RANGE)
#undef INSTANCE_TYPE_CHECKER_RANGE

V8_INLINE bool IsFixedArrayBase(InstanceType instance_type) {
  return IsFixedArray(instance_type) || IsFixedDoubleArray(instance_type) ||
         IsByteArray(instance_type) || IsBytecodeArray(instance_type);
}

V8_INLINE bool IsHeapObject(InstanceType instance_type) { return true; }

V8_INLINE bool IsInternalizedString(InstanceType instance_type) {
  STATIC_ASSERT(kNotInternalizedTag != 0);
  return (instance_type & (kIsNotStringMask | kIsNotInternalizedMask)) ==
         (kStringTag | kInternalizedTag);
}

V8_INLINE bool IsJSObject(InstanceType instance_type) {
  STATIC_ASSERT(LAST_TYPE == LAST_JS_OBJECT_TYPE);
  return instance_type >= FIRST_JS_OBJECT_TYPE;
}

V8_INLINE bool IsJSReceiver(InstanceType instance_type) {
  STATIC_ASSERT(LAST_TYPE == LAST_JS_RECEIVER_TYPE);
  return instance_type >= FIRST_JS_RECEIVER_TYPE;
}

}  // namespace InstanceTypeChecker

// TODO(v8:7786): For instance types that have a single map instance on the
// roots, and when that map is a embedded in the binary, compare against the map
// pointer rather than looking up the instance type.
INSTANCE_TYPE_CHECKERS(TYPE_CHECKER)

}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_INSTANCE_TYPE_INL_H_
