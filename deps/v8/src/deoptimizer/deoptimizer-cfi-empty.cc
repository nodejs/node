// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/deoptimizer/deoptimizer.h"

namespace v8 {
namespace internal {

// Dummy implementation when building mksnapshot.
bool Deoptimizer::IsValidReturnAddress(Address address) { return false; }

}  // namespace internal
}  // namespace v8
