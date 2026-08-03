// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <iostream>
#include <memory>
#include <string>

#include "include/cppgc/allocation.h"
#include "include/cppgc/default-platform.h"
#include "include/cppgc/garbage-collected.h"
#include "include/cppgc/heap.h"
#include "include/cppgc/member.h"
#include "include/cppgc/visitor.h"

#if !CPPGC_IS_STANDALONE
#include "include/v8-initialization.h"
#endif  // !CPPGC_IS_STANDALONE

/**
 * This sample program shows how to set up a stand-alone cppgc heap.
 */

/**
 * Simple string rope to illustrate allocation and garbage collection below.
 * The rope keeps the next parts alive via regular managed reference.
 */
class Rope final : public cppgc::GarbageCollected<Rope> {
 public:
  explicit Rope(std::string part, Rope* next = nullptr)
      : part_(std::move(part)), next_(next) {}

  void Trace(cppgc::Visitor* visitor) const { visitor->Trace(next_); }

 private:
  const std::string part_;
  const cppgc::Member<Rope> next_;

  friend std::ostream& operator<<(std::ostream& os, const Rope& rope) {
    os << rope.part_;
    if (rope.next_) {
      os << *rope.next_;
    }
    return os;
  }
};

int main(int argc, char* argv[]) {
  // Create a default platform that is used by cppgc::Heap for execution and
  // backend allocation.
  auto cppgc_platform = std::make_shared<cppgc::DefaultPlatform>();
#if !CPPGC_IS_STANDALONE
  // When initializing a stand-alone cppgc heap in a regular V8 build, the
  // internal V8 platform will be reused. Reusing the V8 platform requires
  // initializing it properly.
  v8::V8::InitializePlatform(cppgc_platform->GetV8Platform());
#endif  // !CPPGC_IS_STANDALONE
  // Initialize the process. This must happen before any cppgc::Heap::Create()
  // calls.
  cppgc::InitializeProcess(cppgc_platform->GetPageAllocator());
  {
    // Create a managed heap.
    std::unique_ptr<cppgc::Heap> heap = cppgc::Heap::Create(cppgc_platform);
    // Allocate a string rope on the managed heap.
    Rope* greeting = cppgc::MakeGarbageCollected<Rope>(
        heap->GetAllocationHandle(), "Hello ",
        cppgc::MakeGarbageCollected<Rope>(heap->GetAllocationHandle(),
                                          "World!"));
    // Manually trigger garbage collection. The object greeting is held alive
    // through conservative stack scanning.
    heap->ForceGarbageCollectionSlow("CppGC example", "Testing");
    std::cout << *greeting << std::endl;
  }
  // Gracefully shutdown the process.
  cppgc::ShutdownProcess();
  return 0;
}
