// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_TRACING_TRACE_ID_H_
#define V8_TRACING_TRACE_ID_H_

#include "src/base/platform/platform.h"

namespace v8 {
namespace tracing {
V8_INLINE uint64_t TraceId() {
  static std::atomic<uint64_t> sequence_number(0);
  return sequence_number.fetch_add(1, std::memory_order_relaxed);
}
}  // namespace tracing
}  // namespace v8

#endif  // V8_TRACING_TRACE_ID_H_
