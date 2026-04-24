// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_FOREIGN_H_
#define V8_OBJECTS_FOREIGN_H_

#include "src/objects/heap-object.h"
#include "src/objects/objects-body-descriptors.h"

// Has to be the last include (doesn't have include guards):
#include "src/objects/object-macros.h"

namespace v8::internal {

#include "torque-generated/src/objects/foreign-tq.inc"

// Foreign describes objects pointing from JavaScript to C structures.
V8_OBJECT class Foreign : public HeapObjectLayout {
 public:
  // [foreign_address]: field containing the address.
  template <ExternalPointerTag tag>
  inline Address foreign_address(IsolateForSandbox isolate) const;
  // Deprecated. Prefer to use the variant above with an isolate parameter.
  template <ExternalPointerTag tag>
  inline Address foreign_address() const;
  template <ExternalPointerTag tag>
  inline void set_foreign_address(IsolateForSandbox isolate,
                                  const Address value);
  template <ExternalPointerTag tag>
  inline void init_foreign_address(IsolateForSandbox isolate,
                                   const Address initial_value);

  // Load the address without performing a type check. Only use this when the
  // returned pointer will not be dereferenced.
  inline Address foreign_address_unchecked(IsolateForSandbox isolate) const;
  inline Address foreign_address_unchecked() const;

  // Get the tag of this foreign from the external pointer table. Non-sandbox
  // builds will always return {kAnyExternalPointerTag}.
  inline ExternalPointerTag GetTag() const;

  class BodyDescriptor;

  DECL_PRINTER(Foreign)
  DECL_VERIFIER(Foreign)

 public:
  ExternalPointerMember<kAnyForeignExternalPointerTagRange> foreign_address_;
} V8_OBJECT_END;

// TrustedForeign is similar to Foreign but lives in trusted space.
V8_OBJECT class TrustedForeign : public TrustedObjectLayout {
 public:
  // [foreign_address]: field containing the address of a C object.
  inline Address foreign_address() const;
  inline void set_foreign_address(Address value);

  DECL_PRINTER(TrustedForeign)
  DECL_VERIFIER(TrustedForeign)

 public:
  UnalignedValueMember<Address> foreign_address_;
} V8_OBJECT_END;

template <>
struct ObjectTraits<TrustedForeign> {
  using BodyDescriptor =
      FixedBodyDescriptor<sizeof(TrustedForeign), sizeof(TrustedForeign),
                          sizeof(TrustedForeign)>;
};

}  // namespace v8::internal

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_FOREIGN_H_
