// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef INCLUDE_CPPGC_COMMON_H_
#define INCLUDE_CPPGC_COMMON_H_

// TODO(chromium:1056170): Remove dependency on v8.
#include "v8config.h"  // NOLINT(build/include_directory)

namespace cppgc {

// Indicator for the stack state of the embedder.
enum class EmbedderStackState {
  kMayContainHeapPointers,
  kNoHeapPointers,
  kUnknown V8_ENUM_DEPRECATE_SOON("Use kMayContainHeapPointers") =
      kMayContainHeapPointers,
  kNonEmpty V8_ENUM_DEPRECATE_SOON("Use kMayContainHeapPointers") =
      kMayContainHeapPointers,
  kEmpty V8_ENUM_DEPRECATE_SOON("Use kNoHeapPointers") = kNoHeapPointers,
};

}  // namespace cppgc

#endif  // INCLUDE_CPPGC_COMMON_H_
