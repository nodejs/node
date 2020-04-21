// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/heap/cppgc/heap.h"

#include <memory>

#include "src/base/platform/platform.h"
#include "src/heap/cppgc/heap-object-header-inl.h"
#include "src/heap/cppgc/heap-page.h"
#include "src/heap/cppgc/stack.h"

namespace cppgc {

std::unique_ptr<Heap> Heap::Create() {
  return std::make_unique<internal::Heap>();
}

namespace internal {

namespace {

constexpr bool NeedsConservativeStackScan(Heap::GCConfig config) {
  return config.stack_state == Heap::GCConfig::StackState::kNonEmpty;
}

}  // namespace

// static
cppgc::LivenessBroker LivenessBrokerFactory::Create() {
  return cppgc::LivenessBroker();
}

// TODO(chromium:1056170): Replace with fast stack scanning once
// object are allocated actual arenas/spaces.
class StackMarker final : public StackVisitor {
 public:
  explicit StackMarker(const std::vector<HeapObjectHeader*>& objects)
      : objects_(objects) {}

  void VisitPointer(const void* address) final {
    for (auto* header : objects_) {
      if (address >= header->Payload() &&
          address < (header + header->GetSize())) {
        header->TryMarkAtomic();
      }
    }
  }

 private:
  const std::vector<HeapObjectHeader*>& objects_;
};

Heap::Heap()
    : stack_(std::make_unique<Stack>(v8::base::Stack::GetStackStart())),
      allocator_(std::make_unique<BasicAllocator>(this)),
      prefinalizer_handler_(std::make_unique<PreFinalizerHandler>()) {}

Heap::~Heap() {
  for (HeapObjectHeader* header : objects_) {
    header->Finalize();
  }
}

void Heap::CollectGarbage(GCConfig config) {
  // No GC calls when in NoGCScope.
  CHECK(!in_no_gc_scope());

  // TODO(chromium:1056170): Replace with proper mark-sweep algorithm.
  // "Marking".
  if (NeedsConservativeStackScan(config)) {
    StackMarker marker(objects_);
    stack_->IteratePointers(&marker);
  }
  // "Sweeping and finalization".
  {
    // Pre finalizers are forbidden from allocating objects
    NoAllocationScope no_allocation_scope_(this);
    prefinalizer_handler_->InvokePreFinalizers();
  }
  for (auto it = objects_.begin(); it != objects_.end();) {
    HeapObjectHeader* header = *it;
    if (header->IsMarked()) {
      header->Unmark();
      ++it;
    } else {
      header->Finalize();
      it = objects_.erase(it);
    }
  }
}

Heap::NoGCScope::NoGCScope(Heap* heap) : heap_(heap) { heap_->no_gc_scope_++; }

Heap::NoGCScope::~NoGCScope() { heap_->no_gc_scope_--; }

Heap::NoAllocationScope::NoAllocationScope(Heap* heap) : heap_(heap) {
  heap_->no_allocation_scope_++;
}
Heap::NoAllocationScope::~NoAllocationScope() { heap_->no_allocation_scope_--; }

Heap::BasicAllocator::BasicAllocator(Heap* heap) : heap_(heap) {}

Heap::BasicAllocator::~BasicAllocator() {
  for (auto* page : used_pages_) {
    NormalPage::Destroy(page);
  }
}

void Heap::BasicAllocator::GetNewPage() {
  auto* page = NormalPage::Create(heap_);
  CHECK(page);
  used_pages_.push_back(page);
  current_ = page->PayloadStart();
  limit_ = page->PayloadEnd();
}

}  // namespace internal
}  // namespace cppgc
