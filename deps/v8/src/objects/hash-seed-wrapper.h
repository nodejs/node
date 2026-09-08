// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_HASH_SEED_WRAPPER_H_
#define V8_OBJECTS_HASH_SEED_WRAPPER_H_

#include "src/numbers/hash-seed.h"
#include "src/objects/heap-object.h"
#include "src/objects/tagged-field.h"

// Has to be the last include (doesn't have include guards):
#include "src/objects/object-macros.h"

namespace v8 {
namespace internal {

V8_OBJECT class HashSeedWrapper : public HeapObject {
 public:
  inline const HashSeed::Data& data() const;
  inline void set_data(const HashSeed::Data& value);

  DECL_PRINTER(HashSeedWrapper)
  DECL_VERIFIER(HashSeedWrapper)

  class BodyDescriptor;

 public:
  UnalignedValueMember<HashSeed::Data> data_;
} V8_OBJECT_END;

}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_HASH_SEED_WRAPPER_H_
