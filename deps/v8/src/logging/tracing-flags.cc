// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/logging/tracing-flags.h"

namespace v8 {
namespace internal {

std::atomic_uint TracingFlags::runtime_stats{0};
std::atomic_uint TracingFlags::gc{0};
std::atomic_uint TracingFlags::gc_stats{0};
std::atomic_uint TracingFlags::ic_stats{0};
std::atomic_uint TracingFlags::zone_stats{0};

}  // namespace internal
}  // namespace v8
