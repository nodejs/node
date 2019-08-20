// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_FOREIGN_INL_H_
#define V8_OBJECTS_FOREIGN_INL_H_

#include "src/objects/foreign.h"

#include "src/heap/heap-write-barrier-inl.h"
#include "src/objects/objects-inl.h"

// Has to be the last include (doesn't have include guards):
#include "src/objects/object-macros.h"

namespace v8 {
namespace internal {

OBJECT_CONSTRUCTORS_IMPL(Foreign, HeapObject)

CAST_ACCESSOR(Foreign)

// static
bool Foreign::IsNormalized(Object value) {
  if (value == Smi::kZero) return true;
  return Foreign::cast(value).foreign_address() != kNullAddress;
}

Address Foreign::foreign_address() {
  return ReadField<Address>(kForeignAddressOffset);
}

void Foreign::set_foreign_address(Address value) {
  WriteField<Address>(kForeignAddressOffset, value);
}

}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_FOREIGN_INL_H_
