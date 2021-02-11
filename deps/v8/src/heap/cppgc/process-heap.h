// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_HEAP_CPPGC_PROCESS_HEAP_H_
#define V8_HEAP_CPPGC_PROCESS_HEAP_H_

#include "src/base/platform/mutex.h"

namespace cppgc {
namespace internal {

extern v8::base::LazyMutex g_process_mutex;

}  // namespace internal
}  // namespace cppgc

#endif  // V8_HEAP_CPPGC_PROCESS_HEAP_H_
