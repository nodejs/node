// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/heap/cppgc/process-heap.h"

namespace cppgc {
namespace internal {

v8::base::LazyMutex g_process_mutex = LAZY_MUTEX_INITIALIZER;

}  // namespace internal
}  // namespace cppgc
