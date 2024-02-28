// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_SANDBOX_INDIRECT_POINTER_TAG_H_
#define V8_SANDBOX_INDIRECT_POINTER_TAG_H_

#include "src/common/globals.h"
#include "src/objects/instance-type.h"

namespace v8 {
namespace internal {

// Defines the list of possible indirect pointer tags.
//
// When accessing an indirect pointer, an IndirectPointerTag must be provided
// which expresses the expected instance type of the pointed-to object. When
// the sandbox is enabled, this tag is used by the indirect pointer table to
// ensure type-safe access to objects referenced via indirect pointers. As
// IndirectPointerTags are derived from instance types, conversion between the
// two types is possible and supported through routines defined in this file.

constexpr int kIndirectPointerTagShift = 48;
constexpr uint64_t kIndirectPointerTagMask = 0xffff000000000000;
#define INDIRECT_POINTER_TAG_LIST(V) V(kCodeIndirectPointerTag, CODE_TYPE)
#define MAKE_TAG(instance_type) \
  (uint64_t{instance_type} << kIndirectPointerTagShift)

enum IndirectPointerTag : uint64_t {
  kIndirectPointerNullTag = 0,
#define INDIRECT_POINTER_TAG_ENUM_DECL(name, instance_type) \
  name = MAKE_TAG(instance_type),
  INDIRECT_POINTER_TAG_LIST(INDIRECT_POINTER_TAG_ENUM_DECL)
#undef INDIRECT_POINTER_TAG_ENUM_DECL
};

V8_INLINE bool IsValidIndirectPointerTag(IndirectPointerTag tag) {
#define VALID_INDIRECT_POINTER_TAG_CASE(tag, instance_type) case tag:
  switch (tag) {
    INDIRECT_POINTER_TAG_LIST(VALID_INDIRECT_POINTER_TAG_CASE)
    return true;
    default:
      return false;
  }
#undef VALID_INDIRECT_POINTER_TAG_CASE
}

V8_INLINE IndirectPointerTag
IndirectPointerTagFromInstanceType(InstanceType instance_type) {
  auto tag = static_cast<IndirectPointerTag>(MAKE_TAG(instance_type));
  DCHECK(IsValidIndirectPointerTag(tag));
  return tag;
}

V8_INLINE InstanceType
InstanceTypeFromIndirectPointerTag(IndirectPointerTag tag) {
  DCHECK(IsValidIndirectPointerTag(tag));
  return static_cast<InstanceType>(tag >> kIndirectPointerTagShift);
}

#undef MAKE_TAG
#undef INDIRECT_POINTER_TAG_LIST

}  // namespace internal
}  // namespace v8

#endif  // V8_SANDBOX_INDIRECT_POINTER_TAG_H_
