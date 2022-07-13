// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/heap/cppgc/process-heap-statistics.h"

namespace cppgc {

std::atomic_size_t ProcessHeapStatistics::total_allocated_space_{0};
std::atomic_size_t ProcessHeapStatistics::total_allocated_object_size_{0};

}  // namespace cppgc
