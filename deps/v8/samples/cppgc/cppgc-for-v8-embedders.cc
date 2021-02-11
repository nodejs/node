// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <include/cppgc/allocation.h>
#include <include/cppgc/default-platform.h>
#include <include/cppgc/garbage-collected.h>
#include <include/cppgc/heap.h>
#include <include/cppgc/member.h>
#include <include/cppgc/platform.h>
#include <include/cppgc/visitor.h>
#include <include/v8.h>

#include <iostream>
#include <memory>
#include <string>

/**
 * This sample program shows how to set up a stand-alone cppgc heap as an
 * embedder of V8. Most importantly, this example shows how to reuse V8's
 * platform for cppgc.
 */

/**
 * Simple string rope to illustrate allocation and garbage collection below. The
 * rope keeps the next parts alive via regular managed reference.
 */
class Rope final : public cppgc::GarbageCollected<Rope> {
 public:
  explicit Rope(std::string part, Rope* next = nullptr)
      : part_(part), next_(next) {}

  void Trace(cppgc::Visitor* visitor) const { visitor->Trace(next_); }

 private:
  std::string part_;
  cppgc::Member<Rope> next_;

  friend std::ostream& operator<<(std::ostream& os, const Rope& rope);
};

std::ostream& operator<<(std::ostream& os, const Rope& rope) {
  os << rope.part_;
  if (rope.next_) {
    os << *rope.next_;
  }
  return os;
}

int main(int argc, char* argv[]) {
  // Create a platform that is used by cppgc::Heap for execution and backend
  // allocation.
  auto cppgc_platform = std::make_shared<cppgc::DefaultPlatform>();
  // Initialize the process. This must happen before any cppgc::Heap::Create()
  // calls.
  cppgc::InitializeProcess(cppgc_platform->GetPageAllocator());
  // Create a managed heap.
  std::unique_ptr<cppgc::Heap> heap = cppgc::Heap::Create(cppgc_platform);
  // Allocate a string rope on the managed heap.
  auto* greeting = cppgc::MakeGarbageCollected<Rope>(
      heap->GetAllocationHandle(), "Hello ",
      cppgc::MakeGarbageCollected<Rope>(heap->GetAllocationHandle(), "World!"));
  // Manually trigger garbage collection. The object greeting is held alive
  // through conservative stack scanning.
  heap->ForceGarbageCollectionSlow("V8 embedders example", "Testing");
  std::cout << *greeting << std::endl;
  // Gracefully shutdown the process.
  cppgc::ShutdownProcess();
  return 0;
}
