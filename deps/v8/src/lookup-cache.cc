// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/lookup-cache.h"

namespace v8 {
namespace internal {

void DescriptorLookupCache::Clear() {
  for (int index = 0; index < kLength; index++) keys_[index].source = Map();
}

}  // namespace internal
}  // namespace v8
