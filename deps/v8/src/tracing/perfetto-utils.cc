// Copyright 2024 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/tracing/perfetto-utils.h"

#include <algorithm>
#include <cstdint>
#include <vector>

#include "src/base/logging.h"
#include "src/objects/string-inl.h"

namespace v8 {
namespace internal {

PerfettoV8String::PerfettoV8String(Tagged<String> string) {
  SharedStringAccessGuardIfNeeded access_guard(string);
  String::VisitFlat(&visitor_, string);
}

}  // namespace internal
}  // namespace v8
