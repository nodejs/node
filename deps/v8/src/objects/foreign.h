// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_FOREIGN_H_
#define V8_OBJECTS_FOREIGN_H_

#include "src/objects/heap-object.h"

// Has to be the last include (doesn't have include guards):
#include "src/objects/object-macros.h"

namespace v8 {
namespace internal {

#include "torque-generated/src/objects/foreign-tq.inc"

// Foreign describes objects pointing from JavaScript to C structures.
class Foreign : public TorqueGeneratedForeign<Foreign, HeapObject> {
 public:
  // [foreign_address]: field containing the address.
  DECL_EXTERNAL_POINTER_ACCESSORS(foreign_address, Address)

  // Dispatched behavior.
  DECL_PRINTER(Foreign)

#ifdef V8_COMPRESS_POINTERS
  // TODO(ishell, v8:8875): When pointer compression is enabled the
  // kForeignAddressOffset is only kTaggedSize aligned but we can keep using
  // unaligned access since both x64 and arm64 architectures (where pointer
  // compression is supported) allow unaligned access to full words.
  static_assert(IsAligned(kForeignAddressOffset, kTaggedSize));
#else
  static_assert(IsAligned(kForeignAddressOffset, kExternalPointerSlotSize));
#endif

  class BodyDescriptor;

 private:
  TQ_OBJECT_CONSTRUCTORS(Foreign)
};

}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_FOREIGN_H_
