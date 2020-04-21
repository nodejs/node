// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_HEAP_CPPGC_HEAP_H_
#define V8_HEAP_CPPGC_HEAP_H_

#include <memory>
#include <vector>

#include "include/cppgc/heap.h"
#include "include/cppgc/internal/gc-info.h"
#include "include/cppgc/internal/persistent-node.h"
#include "include/cppgc/liveness-broker.h"
#include "src/heap/cppgc/heap-object-header.h"
#include "src/heap/cppgc/prefinalizer-handler.h"

namespace cppgc {
namespace internal {

class NormalPage;
class Stack;

class V8_EXPORT_PRIVATE LivenessBrokerFactory {
 public:
  static LivenessBroker Create();
};

class V8_EXPORT_PRIVATE Heap final : public cppgc::Heap {
 public:
  class V8_EXPORT_PRIVATE NoGCScope final {
   public:
    explicit NoGCScope(Heap* heap);
    ~NoGCScope();

   private:
    Heap* heap_;
  };

  // The NoAllocationScope class is used in debug mode to catch unwanted
  // allocations. E.g. allocations during GC.
  class V8_EXPORT_PRIVATE NoAllocationScope final {
    CPPGC_STACK_ALLOCATED();

   public:
    explicit NoAllocationScope(Heap* heap);
    ~NoAllocationScope();

   private:
    Heap* const heap_;

    DISALLOW_COPY_AND_ASSIGN(NoAllocationScope);
  };

  struct GCConfig {
    enum class StackState : uint8_t {
      kEmpty,
      kNonEmpty,
    };

    static GCConfig Default() { return {StackState::kNonEmpty}; }

    StackState stack_state = StackState::kNonEmpty;
  };

  static Heap* From(cppgc::Heap* heap) { return static_cast<Heap*>(heap); }

  Heap();
  ~Heap() final;

  inline void* Allocate(size_t size, GCInfoIndex index);

  void CollectGarbage(GCConfig config = GCConfig::Default());

  PreFinalizerHandler* prefinalizer_handler() {
    return prefinalizer_handler_.get();
  }

  PersistentRegion& GetStrongPersistentRegion() {
    return strong_persistent_region_;
  }
  const PersistentRegion& GetStrongPersistentRegion() const {
    return strong_persistent_region_;
  }
  PersistentRegion& GetWeakPersistentRegion() {
    return weak_persistent_region_;
  }
  const PersistentRegion& GetWeakPersistentRegion() const {
    return weak_persistent_region_;
  }

 private:
  // TODO(chromium:1056170): Remove as soon as arenas are available for
  // allocation.
  //
  // This basic allocator just gets a page from the backend and uses bump
  // pointer allocation in the payload to allocate objects. No memory is
  // reused across GC calls.
  class BasicAllocator final {
   public:
    explicit BasicAllocator(Heap* heap);
    ~BasicAllocator();
    inline void* Allocate(size_t);

   private:
    void GetNewPage();

    Heap* heap_;
    Address current_ = nullptr;
    Address limit_ = nullptr;
    std::vector<NormalPage*> used_pages_;
  };

  bool in_no_gc_scope() { return no_gc_scope_ > 0; }

  bool is_allocation_allowed() { return no_allocation_scope_ == 0; }

  std::unique_ptr<Stack> stack_;
  std::unique_ptr<BasicAllocator> allocator_;
  std::vector<HeapObjectHeader*> objects_;
  std::unique_ptr<PreFinalizerHandler> prefinalizer_handler_;

  PersistentRegion strong_persistent_region_;
  PersistentRegion weak_persistent_region_;

  size_t no_gc_scope_ = 0;
  size_t no_allocation_scope_ = 0;
};

}  // namespace internal
}  // namespace cppgc

#endif  // V8_HEAP_CPPGC_HEAP_H_
