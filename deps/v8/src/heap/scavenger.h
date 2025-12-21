// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_HEAP_SCAVENGER_H_
#define V8_HEAP_SCAVENGER_H_

#include <memory>

namespace v8::internal {

class Heap;

// A semi-space copying garbage collector.
class ScavengerCollector {
 public:
  explicit ScavengerCollector(Heap* heap);
  ~ScavengerCollector();

  // Performs synchronous parallel garbage collection based on semi-space
  // copying algorithm.
  void CollectGarbage();

  // Pages may be left in a quarantined state after garbage collection. Objects
  // on those pages are not actually moving and as such the page has to be
  // swept which generally happens concurrently. The call here finishes
  // sweeping, possibly synchronously sweeping such pages as well.
  void CompleteSweepingQuarantinedPagesIfNeeded();

 private:
  class QuarantinedPageSweeper;

  Heap* const heap_;
  std::unique_ptr<QuarantinedPageSweeper> quarantined_page_sweeper_;
};

}  // namespace v8::internal

#endif  // V8_HEAP_SCAVENGER_H_
