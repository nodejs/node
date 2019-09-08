// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/codegen/register.h"
#include "src/codegen/register-arch.h"

namespace v8 {
namespace internal {

bool ShouldPadArguments(int argument_count) {
  return kPadArguments && (argument_count % 2 != 0);
}

}  // namespace internal
}  // namespace v8
