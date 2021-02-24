// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cppgc/allocation.h>
#include <cppgc/default-platform.h>
#include <cppgc/garbage-collected.h>
#include <cppgc/heap.h>
#include <cppgc/member.h>
#include <cppgc/visitor.h>

#include <iostream>
#include <memory>
#include <string>

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
      : part_(part), next_(next) {}

  void Trace(cppgc::Visitor* visitor) const { visitor->Trace(next_); }

 private:
  std::string part_;
  cppgc::Member<Rope> next_;

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
  // Initialize the process. This must happen before any cppgc::Heap::Create()
  // calls. cppgc::DefaultPlatform::InitializeProcess initializes both cppgc
  // and v8 (if cppgc is not used as a standalone) as needed.
  // If using a platform other than cppgc::DefaultPlatform, should call
  // cppgc::InitializeProcess (for standalone builds) or
  // v8::V8::InitializePlatform (for non-standalone builds) directly.
  cppgc::DefaultPlatform::InitializeProcess(cppgc_platform.get());
  // Create a managed heap.
  std::unique_ptr<cppgc::Heap> heap = cppgc::Heap::Create(cppgc_platform);
  // Allocate a string rope on the managed heap.
  auto* greeting = cppgc::MakeGarbageCollected<Rope>(
      heap->GetAllocationHandle(), "Hello ",
      cppgc::MakeGarbageCollected<Rope>(heap->GetAllocationHandle(), "World!"));
  // Manually trigger garbage collection. The object greeting is held alive
  // through conservative stack scanning.
  heap->ForceGarbageCollectionSlow("CppGC example", "Testing");
  std::cout << *greeting << std::endl;
  // Gracefully shutdown the process.
  cppgc::ShutdownProcess();
  return 0;
}
