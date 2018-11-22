// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/detachable-vector.h"

namespace v8 {
namespace internal {

const size_t DetachableVectorBase::kMinimumCapacity = 8;
const size_t DetachableVectorBase::kDataOffset =
    offsetof(DetachableVectorBase, data_);
const size_t DetachableVectorBase::kCapacityOffset =
    offsetof(DetachableVectorBase, capacity_);
const size_t DetachableVectorBase::kSizeOffset =
    offsetof(DetachableVectorBase, size_);

}  // namespace internal
}  // namespace v8
