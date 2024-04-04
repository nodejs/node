// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_TRUSTED_OBJECT_H_
#define V8_OBJECTS_TRUSTED_OBJECT_H_

#include "src/objects/heap-object.h"

// Has to be the last include (doesn't have include guards):
#include "src/objects/object-macros.h"

namespace v8 {
namespace internal {

#include "torque-generated/src/objects/trusted-object-tq.inc"

// An object that is trusted to not have been modified in a malicious way.
//
// Typical examples of trusted objects are containers for bytecode or code
// metadata, which often allow an attacker to corrupt (for example) stack
// memory when manipulated. When the sandbox is enabled, trusted objects are
// located outside of the sandbox (in one of the trusted heap spaces) so that
// attackers cannot corrupt these objects and use them to escape from the
// sandbox. When the sandbox is disabled, trusted objects are treated like any
// other objects since in that case, many other types of objects (for example
// ArrayBuffers) can be used to corrupt memory outside of V8's heaps as well.
//
// Trusted objects cannot directly be referenced from untrusted objects as this
// would be unsafe: an attacker could corrupt any (direct) pointer to these
// objects stored inside the sandbox. However, ExposedTrustedObject can be
// referenced via indirect pointers, which guarantee memory-safe access.
class TrustedObject : public HeapObject {
 public:
  DECL_CAST(TrustedObject)
  DECL_VERIFIER(TrustedObject)

  // Protected pointers, i.e. pointers between objects in trusted space, must
  // only be used to reference a trusted object from another trusted object.
  // Using them from inside the sandbox would not be safe (they would not be
  // "protected"). As such, the slot accessor for these slots only exists on
  // TrustedObjects but not on any other HeapObjects.
  inline ProtectedPointerSlot RawProtectedPointerField(int byte_offset) const;

  static constexpr int kHeaderSize = HeapObject::kHeaderSize;
  static constexpr int kize = kHeaderSize;

  OBJECT_CONSTRUCTORS(TrustedObject, HeapObject);
};

// A trusted object that can safely be referenced from untrusted objects.
//
// These objects live in trusted space but are "exposed" to untrusted objects
// living inside the sandbox. They still cannot be referenced through "direct"
// pointers (these can be corrupted by an attacker), but instead they must be
// referenced through "indirect pointers": an index into a pointer table that
// contains the actual pointer as well as a type tag. This mechanism then
// guarantees memory-safe access.
//
// We want to have one pointer table entry per referenced object, *not* per
// reference. As such, there must be a way to obtain an existing table entry
// for a given (exposed) object. This base class provides that table entry in
// the form of the 'self' indirect pointer.
//
// The need to inherit from this base class to make a trusted object accessible
// means that it is not possible to expose existing utility objects such as
// hash tables or fixed arrays. Instead, those would need to be "wrapped" by
// another ExposedTrustedObject. This limitation is by design: if we were to
// create such an exposed utility object, it would likely weaken the
// type-safety mechanism of indirect pointers because indirect pointers are
// (effectively) tagged with the target's instance type. As such, if the same
// object type is used in different contexts, they would both use the same type
// tag, allowing an attacker to perform a "substitution attack". As a concrete
// example, consider the case of a trusted, exposed byte array. If such a byte
// array is used (a) to hold some sort of bytecode for an interpreter and (b)
// some sort of trusted metadata, then an attacker can take a trusted byte
// array from context (a) and use it in context (b) or vice versa. This would
// effectively result in a type confusion and likely lead to an escape from the
// sandbox. This problem goes away if (a) and (b) each use a dedicated object
// with a unique instance type. It is of course still possible to build new
// utility objects on top of this class, but hopefully this comment serves to
// document the potential pitfalls when doing so.
class ExposedTrustedObject : public TrustedObject {
 public:
  // Initializes this object by creating its pointer table entry.
  inline void init_self_indirect_pointer(IsolateForSandbox isolate);

  // Returns the 'self' indirect pointer of this object.
  // This indirect pointer references a pointer table entry (either in the
  // trusted pointer table or the code pointer table for Code objects) through
  // which this object can be referenced from inside the sandbox.
  inline IndirectPointerHandle self_indirect_pointer_handle() const;

  DECL_CAST(ExposedTrustedObject)
  DECL_VERIFIER(ExposedTrustedObject)

#ifdef V8_ENABLE_SANDBOX
  // The 'self' indirect pointer is only available when the sandbox is enabled.
  // Otherwise, these objects are referenced through direct pointers.
#define FIELD_LIST(V)                                                   \
  V(kSelfIndirectPointerOffset, kIndirectPointerSize)                   \
  V(kUnalignedHeaderSize, OBJECT_POINTER_PADDING(kUnalignedHeaderSize)) \
  V(kHeaderSize, 0)                                                     \
  V(kSize, 0)

  DEFINE_FIELD_OFFSET_CONSTANTS(TrustedObject::kHeaderSize, FIELD_LIST)
#undef FIELD_LIST
#else   // V8_ENABLE_SANDBOX
  static constexpr int kHeaderSize = TrustedObject::kHeaderSize;
#endif  // V8_ENABLE_SANDBOX

  OBJECT_CONSTRUCTORS(ExposedTrustedObject, TrustedObject);
};

}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_TRUSTED_OBJECT_H_
