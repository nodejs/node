// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_FOREIGN_H_
#define V8_OBJECTS_FOREIGN_H_

#include "src/objects/heap-object.h"
#include "torque-generated/class-definitions-from-dsl.h"

// Has to be the last include (doesn't have include guards):
#include "src/objects/object-macros.h"

namespace v8 {
namespace internal {

// Foreign describes objects pointing from JavaScript to C structures.
class Foreign : public HeapObject {
 public:
  // [address]: field containing the address.
  inline Address foreign_address();

  static inline bool IsNormalized(Object object);

  DECL_CAST(Foreign)

  // Dispatched behavior.
  DECL_PRINTER(Foreign)
  DECL_VERIFIER(Foreign)

  DEFINE_FIELD_OFFSET_CONSTANTS(HeapObject::kHeaderSize,
                                TORQUE_GENERATED_FOREIGN_FIELDS)

#ifdef V8_COMPRESS_POINTERS
  // TODO(ishell, v8:8875): When pointer compression is enabled the
  // kForeignAddressOffset is only kTaggedSize aligned but we can keep using
  // unaligned access since both x64 and arm64 architectures (where pointer
  // compression is supported) allow unaligned access to full words.
  STATIC_ASSERT(IsAligned(kForeignAddressOffset, kTaggedSize));
#else
  STATIC_ASSERT(IsAligned(kForeignAddressOffset, kSystemPointerSize));
#endif

  STATIC_ASSERT(kForeignAddressOffset == Internals::kForeignAddressOffset);

  class BodyDescriptor;

 private:
  friend class Factory;
  friend class SerializerDeserializer;
  friend class StartupSerializer;

  inline void set_foreign_address(Address value);

  OBJECT_CONSTRUCTORS(Foreign, HeapObject);
};

}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_FOREIGN_H_
