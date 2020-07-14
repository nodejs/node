// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_FOREIGN_INL_H_
#define V8_OBJECTS_FOREIGN_INL_H_

#include "src/common/globals.h"
#include "src/objects/foreign.h"

#include "src/common/external-pointer-inl.h"
#include "src/heap/heap-write-barrier-inl.h"
#include "src/objects/objects-inl.h"

// Has to be the last include (doesn't have include guards):
#include "src/objects/object-macros.h"

namespace v8 {
namespace internal {

TQ_OBJECT_CONSTRUCTORS_IMPL(Foreign)

// static
bool Foreign::IsNormalized(Object value) {
  if (value == Smi::zero()) return true;
  return Foreign::cast(value).foreign_address() != kNullAddress;
}

DEF_GETTER(Foreign, foreign_address, Address) {
  ExternalPointer_t encoded_value =
      ReadField<ExternalPointer_t>(kForeignAddressOffset);
  Address value = DecodeExternalPointer(isolate, encoded_value);
  return value;
}

void Foreign::set_foreign_address(Isolate* isolate, Address value) {
  ExternalPointer_t encoded_value = EncodeExternalPointer(isolate, value);
  WriteField<ExternalPointer_t>(kForeignAddressOffset, encoded_value);
}

}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_FOREIGN_INL_H_
