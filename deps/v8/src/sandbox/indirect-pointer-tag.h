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
// the sandbox is enabled, this tag is used to ensure type-safe access to
// objects referenced via indirect pointers. As IndirectPointerTags are derived
// from instance types, conversion between the two types is possible and
// supported through routines defined in this file.

constexpr int kIndirectPointerTagShift = 48;
constexpr uint64_t kIndirectPointerTagMask = 0xffff000000000000;

#define INDIRECT_POINTER_TAG_LIST(V)                           \
  V(kCodeIndirectPointerTag, CODE_TYPE)                        \
  V(kBytecodeArrayIndirectPointerTag, BYTECODE_ARRAY_TYPE)     \
  V(kInterpreterDataIndirectPointerTag, INTERPRETER_DATA_TYPE) \
  IF_WASM(V, kWasmTrustedInstanceDataIndirectPointerTag,       \
          WASM_TRUSTED_INSTANCE_DATA_TYPE)

#define MAKE_TAG(instance_type) \
  (uint64_t{instance_type} << kIndirectPointerTagShift)

// TODO(saelo): consider renaming this to something like TypeTag or
// InstanceTypeTag since that better captures what this represents.
enum IndirectPointerTag : uint64_t {
  // The null tag. Usually used to express the lack of a valid tag, for example
  // in non-sandbox builds.
  kIndirectPointerNullTag = 0,

  // This tag can be used when an indirect pointer field can legitimately refer
  // to objects of different types.
  // NOTE: this tag effectively disables the built-in type-checking mechanism.
  // As such, in virtually all cases the caller needs to perform runtime-type
  // checks (i.e. IsXyzObject(obj))` afterwards which need to be able to
  // correctly handle unexpected types. The last point is worth stressing
  // further. As an example, the following code is NOT correct:
  //
  //     auto obj = LoadTrustedPointerField<kUnknownIndirectPointerTag>(...);
  //     if (IsFoo(obj)) {
  //         Foo::cast(obj)->foo();
  //     } else if (IsBar(obj)) {
  //         Bar::cast(obj)->bar();
  //     } else {
  //         // Potential type confusion here!
  //         Baz::cast(obj)->baz();
  //     }
  //
  // This is because an attacker can swap trusted pointers and thereby cause an
  // object of a different/unexpected type to be returned. Instead, in this
  // case a CHECK can for example be used to make the code correct:
  //
  //     // ...
  //     } else {
  //         // Must be a Baz object
  //         CHECK(IsBaz(obj));
  //         Baz::cast(obj)->baz();
  //    }
  //
  kUnknownIndirectPointerTag = kIndirectPointerTagMask,

// "Regular" tags. One per supported instance type.
#define INDIRECT_POINTER_TAG_ENUM_DECL(name, instance_type) \
  name = MAKE_TAG(instance_type),
  INDIRECT_POINTER_TAG_LIST(INDIRECT_POINTER_TAG_ENUM_DECL)
#undef INDIRECT_POINTER_TAG_ENUM_DECL
};

V8_INLINE constexpr bool IsValidIndirectPointerTag(IndirectPointerTag tag) {
#define VALID_INDIRECT_POINTER_TAG_CASE(tag, instance_type) case tag:
  switch (tag) {
    INDIRECT_POINTER_TAG_LIST(VALID_INDIRECT_POINTER_TAG_CASE)
    return true;
    default:
      return false;
  }
#undef VALID_INDIRECT_POINTER_TAG_CASE
}

// Migrating objects into trusted space is typically performed in multiple
// steps, where all references to the object from inside the sandbox are first
// changed to indirect pointers before actually moving the object out of the
// sandbox. As we have CHECKs that trusted pointer table entries point outside
// of the sandbox, we need this helper function to disable that CHECK for
// objects that are in the process of being migrated into trusted space.
V8_INLINE constexpr bool IsTrustedSpaceMigrationInProgressForObjectsWithTag(
    IndirectPointerTag tag) {
  return tag == kInterpreterDataIndirectPointerTag;
}

// The null tag is also considered an invalid tag since no indirect pointer
// field should be using this tag.
static_assert(!IsValidIndirectPointerTag(kIndirectPointerNullTag));

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
