// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_HEAP_CPPGC_GARBAGE_COLLECTOR_H_
#define V8_HEAP_CPPGC_GARBAGE_COLLECTOR_H_

#include "include/cppgc/common.h"
#include "src/heap/cppgc/heap-config.h"

namespace cppgc {
namespace internal {

// GC interface that allows abstraction over the actual GC invocation. This is
// needed to mock/fake GC for testing.
class GarbageCollector {
 public:
  // Executes a garbage collection specified in config.
  virtual void CollectGarbage(GCConfig) = 0;
  virtual void StartIncrementalGarbageCollection(GCConfig) = 0;

  // The current epoch that the GC maintains. The epoch is increased on every
  // GC invocation.
  virtual size_t epoch() const = 0;

  // Returns a non-null state if the stack state if overriden.
  virtual const EmbedderStackState* override_stack_state() const = 0;
};

}  // namespace internal
}  // namespace cppgc

#endif  // V8_HEAP_CPPGC_GARBAGE_COLLECTOR_H_
