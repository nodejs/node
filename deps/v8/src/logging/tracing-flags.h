// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_LOGGING_TRACING_FLAGS_H_
#define V8_LOGGING_TRACING_FLAGS_H_

#include <atomic>

#include "src/base/macros.h"

namespace v8 {
namespace internal {

// This struct contains a set of flags that can be modified from multiple
// threads at runtime unlike the normal FLAG_-like flags which are not modified
// after V8 instance is initialized.

struct TracingFlags {
  static V8_EXPORT_PRIVATE std::atomic_uint runtime_stats;
  static V8_EXPORT_PRIVATE std::atomic_uint gc;
  static V8_EXPORT_PRIVATE std::atomic_uint gc_stats;
  static V8_EXPORT_PRIVATE std::atomic_uint ic_stats;
  static V8_EXPORT_PRIVATE std::atomic_uint zone_stats;

  static bool is_runtime_stats_enabled() {
    return runtime_stats.load(std::memory_order_relaxed) != 0;
  }

  static bool is_gc_enabled() {
    return gc.load(std::memory_order_relaxed) != 0;
  }

  static bool is_gc_stats_enabled() {
    return gc_stats.load(std::memory_order_relaxed) != 0;
  }

  static bool is_ic_stats_enabled() {
    return ic_stats.load(std::memory_order_relaxed) != 0;
  }

  static bool is_zone_stats_enabled() {
    return zone_stats.load(std::memory_order_relaxed) != 0;
  }
};

}  // namespace internal
}  // namespace v8

#endif  // V8_LOGGING_TRACING_FLAGS_H_
