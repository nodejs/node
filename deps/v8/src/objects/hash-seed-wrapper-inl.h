// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_HASH_SEED_WRAPPER_INL_H_
#define V8_OBJECTS_HASH_SEED_WRAPPER_INL_H_

#include "src/objects/hash-seed-wrapper.h"
#include "src/objects/heap-object-inl.h"

// Has to be the last include (doesn't have include guards):
#include "src/objects/object-macros.h"

namespace v8 {
namespace internal {

const HashSeed::Data& HashSeedWrapper::data() const { return *data_.address(); }

void HashSeedWrapper::set_data(const HashSeed::Data& value) {
  data_.set_value(value);
}

}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_HASH_SEED_WRAPPER_INL_H_
