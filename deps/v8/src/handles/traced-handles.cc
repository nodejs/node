// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/handles/traced-handles.h"

#include <iterator>
#include <limits>

#include "include/v8-internal.h"
#include "include/v8-traced-handle.h"
#include "src/base/logging.h"
#include "src/base/platform/memory.h"
#include "src/base/sanitizer/asan.h"
#include "src/common/globals.h"
#include "src/handles/handles.h"
#include "src/heap/heap-write-barrier-inl.h"
#include "src/objects/objects.h"
#include "src/objects/visitors.h"

namespace v8::internal {

class TracedHandlesImpl;
namespace {

class TracedNodeBlock;

class TracedNode final {
 public:
#ifdef V8_HOST_ARCH_64_BIT
  using IndexType = uint16_t;
#else   // !V8_HOST_ARCH_64_BIT
  using IndexType = uint8_t;
#endif  // !V8_HOST_ARCH_64_BIT

  static TracedNode* FromLocation(Address* location) {
    return reinterpret_cast<TracedNode*>(location);
  }

  static const TracedNode* FromLocation(const Address* location) {
    return reinterpret_cast<const TracedNode*>(location);
  }

  TracedNode(IndexType, IndexType);

  IndexType index() const { return index_; }

  bool is_root() const { return IsRoot::decode(flags_); }
  void set_root(bool v) { flags_ = IsRoot::update(flags_, v); }

  template <AccessMode access_mode = AccessMode::NON_ATOMIC>
  bool is_in_use() const {
    if constexpr (access_mode == AccessMode::NON_ATOMIC) {
      return IsInUse::decode(flags_);
    }
    const auto flags =
        reinterpret_cast<const std::atomic<uint8_t>&>(flags_).load(
            std::memory_order_relaxed);
    return IsInUse::decode(flags);
  }
  void set_is_in_use(bool v) { flags_ = IsInUse::update(flags_, v); }

  bool is_in_young_list() const { return IsInYoungList::decode(flags_); }
  void set_is_in_young_list(bool v) {
    flags_ = IsInYoungList::update(flags_, v);
  }

  IndexType next_free() const { return next_free_index_; }
  void set_next_free(IndexType next_free_index) {
    next_free_index_ = next_free_index;
  }
  void set_class_id(uint16_t class_id) { class_id_ = class_id; }

  template <AccessMode access_mode = AccessMode::NON_ATOMIC>
  void set_markbit() {
    if constexpr (access_mode == AccessMode::NON_ATOMIC) {
      flags_ = Markbit::update(flags_, true);
      return;
    }
    std::atomic<uint8_t>& atomic_flags =
        reinterpret_cast<std::atomic<uint8_t>&>(flags_);
    const uint8_t new_value =
        Markbit::update(atomic_flags.load(std::memory_order_relaxed), true);
    atomic_flags.fetch_or(new_value, std::memory_order_relaxed);
  }

  template <AccessMode access_mode = AccessMode::NON_ATOMIC>
  bool markbit() const {
    if constexpr (access_mode == AccessMode::NON_ATOMIC) {
      return Markbit::decode(flags_);
    }
    const auto flags =
        reinterpret_cast<const std::atomic<uint8_t>&>(flags_).load(
            std::memory_order_relaxed);
    return Markbit::decode(flags);
  }

  void clear_markbit() { flags_ = Markbit::update(flags_, false); }

  bool has_old_host() const { return HasOldHost::decode(flags_); }
  void set_has_old_host(bool v) { flags_ = HasOldHost::update(flags_, v); }

  template <AccessMode access_mode = AccessMode::NON_ATOMIC>
  void set_raw_object(Address value) {
    if constexpr (access_mode == AccessMode::NON_ATOMIC) {
      object_ = value;
    } else {
      reinterpret_cast<std::atomic<Address>*>(&object_)->store(
          value, std::memory_order_relaxed);
    }
  }
  Address raw_object() const { return object_; }
  Tagged<Object> object() const { return Tagged<Object>(object_); }
  Handle<Object> handle() { return Handle<Object>(&object_); }
  FullObjectSlot location() { return FullObjectSlot(&object_); }

  Handle<Object> Publish(Tagged<Object> object, bool needs_young_bit_update,
                         bool needs_black_allocation, bool has_old_host);
  void Release();

 private:
  using IsInUse = base::BitField8<bool, 0, 1>;
  using IsInYoungList = IsInUse::Next<bool, 1>;
  using IsRoot = IsInYoungList::Next<bool, 1>;
  // The markbit is the exception as it can be set from the main and marker
  // threads at the same time.
  using Markbit = IsRoot::Next<bool, 1>;
  using HasOldHost = Markbit::Next<bool, 1>;

  Address object_ = kNullAddress;
  union {
    // When a node is in use, the user can specify a class id.
    uint16_t class_id_;
    // When a node is not in use, this index is used to build the free list.
    IndexType next_free_index_;
  };
  IndexType index_;
  uint8_t flags_ = 0;
};

TracedNode::TracedNode(IndexType index, IndexType next_free_index)
    : next_free_index_(next_free_index), index_(index) {
  static_assert(offsetof(TracedNode, class_id_) ==
                Internals::kTracedNodeClassIdOffset);
  // TracedNode size should stay within 2 words.
  static_assert(sizeof(TracedNode) <= (2 * kSystemPointerSize));
  DCHECK(!is_in_use());
  DCHECK(!is_in_young_list());
  DCHECK(!is_root());
  DCHECK(!markbit());
  DCHECK(!has_old_host());
}

// Publishes all internal state to be consumed by other threads.
Handle<Object> TracedNode::Publish(Tagged<Object> object,
                                   bool needs_young_bit_update,
                                   bool needs_black_allocation,
                                   bool has_old_host) {
  DCHECK(!is_in_use());
  DCHECK(!is_root());
  DCHECK(!markbit());
  set_class_id(0);
  if (needs_young_bit_update) {
    set_is_in_young_list(true);
  }
  if (needs_black_allocation) {
    set_markbit();
  }
  if (has_old_host) {
    DCHECK(is_in_young_list());
    set_has_old_host(true);
  }
  set_root(true);
  set_is_in_use(true);
  reinterpret_cast<std::atomic<Address>*>(&object_)->store(
      object.ptr(), std::memory_order_release);
  return Handle<Object>(&object_);
}

void TracedNode::Release() {
  DCHECK(is_in_use());
  // Only preserve the in-young-list bit which is used to avoid duplicates in
  // TracedHandlesImpl::young_nodes_;
  flags_ &= IsInYoungList::encode(true);
  DCHECK(!is_in_use());
  DCHECK(!is_root());
  DCHECK(!markbit());
  DCHECK(!has_old_host());
  set_raw_object(kGlobalHandleZapValue);
}

template <typename T, typename NodeAccessor>
class DoublyLinkedList final {
  template <typename U>
  class IteratorImpl final
      : public base::iterator<std::forward_iterator_tag, U> {
   public:
    explicit IteratorImpl(U* object) : object_(object) {}
    IteratorImpl(const IteratorImpl& other) V8_NOEXCEPT
        : object_(other.object_) {}
    U* operator*() { return object_; }
    bool operator==(const IteratorImpl& rhs) const {
      return rhs.object_ == object_;
    }
    bool operator!=(const IteratorImpl& rhs) const { return !(*this == rhs); }
    inline IteratorImpl& operator++() {
      object_ = ListNodeFor(object_)->next;
      return *this;
    }
    inline IteratorImpl operator++(int) {
      IteratorImpl tmp(*this);
      operator++();
      return tmp;
    }

   private:
    U* object_;
  };

 public:
  using Iterator = IteratorImpl<T>;
  using ConstIterator = IteratorImpl<const T>;

  struct ListNode {
    T* prev = nullptr;
    T* next = nullptr;
  };

  T* Front() { return front_; }

  void PushFront(T* object) {
    DCHECK(!Contains(object));
    ListNodeFor(object)->next = front_;
    if (front_) {
      ListNodeFor(front_)->prev = object;
    }
    front_ = object;
    size_++;
  }

  void PopFront() {
    DCHECK(!Empty());
    if (ListNodeFor(front_)->next) {
      ListNodeFor(ListNodeFor(front_)->next)->prev = nullptr;
    }
    front_ = ListNodeFor(front_)->next;
    size_--;
  }

  void Remove(T* object) {
    DCHECK(Contains(object));
    auto& next_object = ListNodeFor(object)->next;
    auto& prev_object = ListNodeFor(object)->prev;
    if (front_ == object) {
      front_ = next_object;
    }
    if (next_object) {
      ListNodeFor(next_object)->prev = prev_object;
    }
    if (prev_object) {
      ListNodeFor(prev_object)->next = next_object;
    }
    next_object = nullptr;
    prev_object = nullptr;
    size_--;
  }

  bool Contains(T* object) const {
    if (front_ == object) return true;
    auto* list_node = ListNodeFor(object);
    return list_node->prev || list_node->next;
  }

  size_t Size() const { return size_; }
  bool Empty() const { return size_ == 0; }

  Iterator begin() { return Iterator(front_); }
  Iterator end() { return Iterator(nullptr); }
  ConstIterator begin() const { return ConstIterator(front_); }
  ConstIterator end() const { return ConstIterator(nullptr); }

 private:
  static ListNode* ListNodeFor(T* object) {
    return NodeAccessor::GetListNode(object);
  }
  static const ListNode* ListNodeFor(const T* object) {
    return NodeAccessor::GetListNode(const_cast<T*>(object));
  }

  T* front_ = nullptr;
  size_t size_ = 0;
};

class TracedNodeBlock final {
  struct OverallListNode {
    static auto* GetListNode(TracedNodeBlock* block) {
      return &block->overall_list_node_;
    }
  };

  struct UsableListNode {
    static auto* GetListNode(TracedNodeBlock* block) {
      return &block->usable_list_node_;
    }
  };

  class NodeIteratorImpl final
      : public base::iterator<std::forward_iterator_tag, TracedNode> {
   public:
    explicit NodeIteratorImpl(TracedNodeBlock* block) : block_(block) {}
    NodeIteratorImpl(TracedNodeBlock* block,
                     TracedNode::IndexType current_index)
        : block_(block), current_index_(current_index) {}
    NodeIteratorImpl(const NodeIteratorImpl& other) V8_NOEXCEPT
        : block_(other.block_),
          current_index_(other.current_index_) {}

    TracedNode* operator*() { return block_->at(current_index_); }
    bool operator==(const NodeIteratorImpl& rhs) const {
      return rhs.block_ == block_ && rhs.current_index_ == current_index_;
    }
    bool operator!=(const NodeIteratorImpl& rhs) const {
      return !(*this == rhs);
    }
    inline NodeIteratorImpl& operator++() {
      current_index_++;
      return *this;
    }
    inline NodeIteratorImpl operator++(int) {
      NodeIteratorImpl tmp(*this);
      operator++();
      return tmp;
    }

   private:
    TracedNodeBlock* block_;
    TracedNode::IndexType current_index_ = 0;
  };

 public:
  using OverallList = DoublyLinkedList<TracedNodeBlock, OverallListNode>;
  using UsableList = DoublyLinkedList<TracedNodeBlock, UsableListNode>;
  using Iterator = NodeIteratorImpl;

#if defined(V8_USE_ADDRESS_SANITIZER)
  static constexpr size_t kMinCapacity = 1;
  static constexpr size_t kMaxCapacity = 1;
#else  // !defined(V8_USE_ADDRESS_SANITIZER)
#ifdef V8_HOST_ARCH_64_BIT
  static constexpr size_t kMinCapacity = 256;
#else   // !V8_HOST_ARCH_64_BIT
  static constexpr size_t kMinCapacity = 128;
#endif  // !V8_HOST_ARCH_64_BIT
  static constexpr size_t kMaxCapacity =
      std::numeric_limits<TracedNode::IndexType>::max() - 1;
#endif  // !defined(V8_USE_ADDRESS_SANITIZER)

  static constexpr TracedNode::IndexType kInvalidFreeListNodeIndex = -1;

  static_assert(kMinCapacity <= kMaxCapacity);
  static_assert(kInvalidFreeListNodeIndex > kMaxCapacity);

  static TracedNodeBlock* Create(TracedHandlesImpl&, OverallList&, UsableList&);
  static void Delete(TracedNodeBlock*);

  static TracedNodeBlock& From(TracedNode& node);
  static const TracedNodeBlock& From(const TracedNode& node);

  TracedNode* AllocateNode();
  void FreeNode(TracedNode*);

  TracedNode* at(TracedNode::IndexType index) {
    return &(reinterpret_cast<TracedNode*>(this + 1)[index]);
  }
  const TracedNode* at(TracedNode::IndexType index) const {
    return const_cast<TracedNodeBlock*>(this)->at(index);
  }

  const void* nodes_begin_address() const { return at(0); }
  const void* nodes_end_address() const { return at(capacity_); }

  TracedHandlesImpl& traced_handles() const { return traced_handles_; }

  Iterator begin() { return Iterator(this); }
  Iterator end() { return Iterator(this, capacity_); }

  bool IsFull() const { return used_ == capacity_; }
  bool IsEmpty() const { return used_ == 0; }

  size_t size_bytes() const {
    return sizeof(*this) + capacity_ * sizeof(TracedNode);
  }

 private:
  TracedNodeBlock(TracedHandlesImpl&, OverallList&, UsableList&,
                  TracedNode::IndexType);

  OverallList::ListNode overall_list_node_;
  UsableList::ListNode usable_list_node_;
  TracedHandlesImpl& traced_handles_;
  TracedNode::IndexType used_ = 0;
  const TracedNode::IndexType capacity_ = 0;
  TracedNode::IndexType first_free_node_ = 0;
};

// static
TracedNodeBlock* TracedNodeBlock::Create(TracedHandlesImpl& traced_handles,
                                         OverallList& overall_list,
                                         UsableList& usable_list) {
  static_assert(alignof(TracedNodeBlock) >= alignof(TracedNode));
  static_assert(sizeof(TracedNodeBlock) % alignof(TracedNode) == 0,
                "TracedNodeBlock size is used to auto-align node FAM storage.");
  const size_t min_wanted_size =
      sizeof(TracedNodeBlock) +
      sizeof(TracedNode) * TracedNodeBlock::kMinCapacity;
  const auto raw_result = v8::base::AllocateAtLeast<char>(min_wanted_size);
  const size_t capacity = std::min(
      (raw_result.count - sizeof(TracedNodeBlock)) / sizeof(TracedNode),
      kMaxCapacity);
  CHECK_LT(capacity, std::numeric_limits<TracedNode::IndexType>::max());
  const auto result = std::make_pair(raw_result.ptr, capacity);
  return new (result.first)
      TracedNodeBlock(traced_handles, overall_list, usable_list,
                      static_cast<TracedNode::IndexType>(result.second));
}

// static
void TracedNodeBlock::Delete(TracedNodeBlock* block) { free(block); }

TracedNodeBlock::TracedNodeBlock(TracedHandlesImpl& traced_handles,
                                 OverallList& overall_list,
                                 UsableList& usable_list,
                                 TracedNode::IndexType capacity)
    : traced_handles_(traced_handles), capacity_(capacity) {
  for (TracedNode::IndexType i = 0; i < (capacity_ - 1); i++) {
    new (at(i)) TracedNode(i, i + 1);
  }
  new (at(capacity_ - 1)) TracedNode(capacity_ - 1, kInvalidFreeListNodeIndex);
  overall_list.PushFront(this);
  usable_list.PushFront(this);
}

// static
TracedNodeBlock& TracedNodeBlock::From(TracedNode& node) {
  TracedNode* first_node = &node - node.index();
  return *reinterpret_cast<TracedNodeBlock*>(
      reinterpret_cast<uintptr_t>(first_node) - sizeof(TracedNodeBlock));
}

// static
const TracedNodeBlock& TracedNodeBlock::From(const TracedNode& node) {
  return From(const_cast<TracedNode&>(node));
}

TracedNode* TracedNodeBlock::AllocateNode() {
  if (used_ == capacity_) {
    DCHECK_EQ(first_free_node_, kInvalidFreeListNodeIndex);
    return nullptr;
  }

  DCHECK_NE(first_free_node_, kInvalidFreeListNodeIndex);
  auto* node = at(first_free_node_);
  first_free_node_ = node->next_free();
  used_++;
  DCHECK(!node->is_in_use());
  return node;
}

void TracedNodeBlock::FreeNode(TracedNode* node) {
  DCHECK(node->is_in_use());
  node->Release();
  DCHECK(!node->is_in_use());
  node->set_next_free(first_free_node_);
  first_free_node_ = node->index();
  used_--;
}

CppHeap* GetCppHeapIfUnifiedYoungGC(Isolate* isolate) {
  // TODO(v8:13475) Consider removing this check when unified-young-gen becomes
  // default.
  if (!v8_flags.cppgc_young_generation) return nullptr;
  auto* cpp_heap = CppHeap::From(isolate->heap()->cpp_heap());
  if (cpp_heap && cpp_heap->generational_gc_supported()) return cpp_heap;
  return nullptr;
}

bool IsCppGCHostOld(CppHeap& cpp_heap, Address host) {
  DCHECK(host);
  DCHECK(cpp_heap.generational_gc_supported());
  auto* host_ptr = reinterpret_cast<void*>(host);
  auto* page = cppgc::internal::BasePage::FromInnerAddress(&cpp_heap, host_ptr);
  // TracedReference may be created on stack, in which case assume it's young
  // and doesn't need to be remembered, since it'll anyway be scanned.
  if (!page) return false;
  return !page->ObjectHeaderFromInnerAddress(host_ptr).IsYoung();
}

void SetSlotThreadSafe(Address** slot, Address* val) {
  reinterpret_cast<std::atomic<Address*>*>(slot)->store(
      val, std::memory_order_relaxed);
}

}  // namespace

class TracedHandlesImpl final {
 public:
  explicit TracedHandlesImpl(Isolate*);
  ~TracedHandlesImpl();

  Handle<Object> Create(Address value, Address* slot,
                        GlobalHandleStoreMode store_mode);
  void Destroy(TracedNodeBlock& node_block, TracedNode& node);
  void Copy(const TracedNode& from_node, Address** to);
  void Move(TracedNode& from_node, Address** from, Address** to);

  void SetIsMarking(bool);
  void SetIsSweepingOnMutatorThread(bool);

  const TracedHandles::NodeBounds GetNodeBounds() const;

  void UpdateListOfYoungNodes();
  void ClearListOfYoungNodes();

  void DeleteEmptyBlocks();

  void ResetDeadNodes(WeakSlotCallbackWithHeap should_reset_handle);
  void ResetYoungDeadNodes(WeakSlotCallbackWithHeap should_reset_handle);

  void ComputeWeaknessForYoungObjects(WeakSlotCallback is_unmodified);
  void ProcessYoungObjects(RootVisitor* visitor,
                           WeakSlotCallbackWithHeap should_reset_handle);

  void Iterate(RootVisitor* visitor);
  void IterateYoung(RootVisitor* visitor);
  void IterateYoungRoots(RootVisitor* visitor);
  void IterateAndMarkYoungRootsWithOldHosts(RootVisitor* visitor);
  void IterateYoungRootsWithOldHostsForTesting(RootVisitor* visitor);

  size_t used_node_count() const { return used_nodes_; }
  size_t used_size_bytes() const { return sizeof(TracedNode) * used_nodes_; }
  size_t total_size_bytes() const { return block_size_bytes_; }

  bool HasYoung() const { return !young_nodes_.empty(); }

 private:
  TracedNode* AllocateNode();
  void FreeNode(TracedNode*);

  bool NeedsToBeRemembered(Tagged<Object> value, TracedNode* node,
                           Address* slot,
                           GlobalHandleStoreMode store_mode) const;

  TracedNodeBlock::OverallList blocks_;
  TracedNodeBlock::UsableList usable_blocks_;
  // List of young nodes. May refer to nodes in `blocks_`, `usable_blocks_`, and
  // `empty_block_candidates_`.
  std::vector<TracedNode*> young_nodes_;
  // Empty blocks that are still referred to from `young_nodes_`.
  std::vector<TracedNodeBlock*> empty_block_candidates_;
  // Fully empty blocks that are neither referenced from any stale references in
  // destructors nor from young nodes.
  std::vector<TracedNodeBlock*> empty_blocks_;
  Isolate* isolate_;
  bool is_marking_ = false;
  bool is_sweeping_on_mutator_thread_ = false;
  size_t used_nodes_ = 0;
  size_t block_size_bytes_ = 0;
};

TracedNode* TracedHandlesImpl::AllocateNode() {
  auto* block = usable_blocks_.Front();
  if (!block) {
    if (empty_blocks_.empty() && empty_block_candidates_.empty()) {
      block = TracedNodeBlock::Create(*this, blocks_, usable_blocks_);
      block_size_bytes_ += block->size_bytes();
    } else {
      // Pick a block from candidates first as such blocks may anyways still be
      // referred to from young nodes and thus are not eligible for freeing.
      auto& block_source = empty_block_candidates_.empty()
                               ? empty_blocks_
                               : empty_block_candidates_;
      block = block_source.back();
      block_source.pop_back();
      DCHECK(block->IsEmpty());
      usable_blocks_.PushFront(block);
      blocks_.PushFront(block);
    }
    DCHECK_EQ(block, usable_blocks_.Front());
  }
  auto* node = block->AllocateNode();
  if (node) {
    used_nodes_++;
    return node;
  }

  usable_blocks_.Remove(block);
  return AllocateNode();
}

void TracedHandlesImpl::FreeNode(TracedNode* node) {
  auto& block = TracedNodeBlock::From(*node);
  if (block.IsFull() && !usable_blocks_.Contains(&block)) {
    usable_blocks_.PushFront(&block);
  }
  block.FreeNode(node);
  if (block.IsEmpty()) {
    usable_blocks_.Remove(&block);
    blocks_.Remove(&block);
    empty_block_candidates_.push_back(&block);
  }
  used_nodes_--;
}

TracedHandlesImpl::TracedHandlesImpl(Isolate* isolate) : isolate_(isolate) {}

TracedHandlesImpl::~TracedHandlesImpl() {
  size_t block_size_bytes = 0;
  while (!blocks_.Empty()) {
    auto* block = blocks_.Front();
    blocks_.PopFront();
    block_size_bytes += block->size_bytes();
    TracedNodeBlock::Delete(block);
  }
  for (auto* block : empty_block_candidates_) {
    block_size_bytes += block->size_bytes();
    TracedNodeBlock::Delete(block);
  }
  for (auto* block : empty_blocks_) {
    block_size_bytes += block->size_bytes();
    TracedNodeBlock::Delete(block);
  }
  USE(block_size_bytes);
  DCHECK_EQ(block_size_bytes, block_size_bytes_);
}

namespace {
bool NeedsTrackingInYoungNodes(Tagged<Object> object, TracedNode* node) {
  return ObjectInYoungGeneration(object) && !node->is_in_young_list();
}
}  // namespace

bool TracedHandlesImpl::NeedsToBeRemembered(
    Tagged<Object> object, TracedNode* node, Address* slot,
    GlobalHandleStoreMode store_mode) const {
  DCHECK(!node->has_old_host());
  if (store_mode == GlobalHandleStoreMode::kInitializingStore) {
    // Don't record initializing stores.
    return false;
  }
  if (is_marking_) {
    // If marking is in progress, the marking barrier will be issued later.
    return false;
  }
  auto* cpp_heap = GetCppHeapIfUnifiedYoungGC(isolate_);
  if (!cpp_heap) return false;

  if (!ObjectInYoungGeneration(object)) return false;
  return IsCppGCHostOld(*cpp_heap, reinterpret_cast<Address>(slot));
}

Handle<Object> TracedHandlesImpl::Create(Address value, Address* slot,
                                         GlobalHandleStoreMode store_mode) {
  Tagged<Object> object(value);
  auto* node = AllocateNode();
  bool needs_young_bit_update = false;
  if (NeedsTrackingInYoungNodes(object, node)) {
    needs_young_bit_update = true;
    young_nodes_.push_back(node);
  }

  const bool has_old_host = NeedsToBeRemembered(object, node, slot, store_mode);
  bool needs_black_allocation = false;
  if (is_marking_ && store_mode != GlobalHandleStoreMode::kInitializingStore) {
    needs_black_allocation = true;
    WriteBarrier::MarkingFromGlobalHandle(object);
  }
  return node->Publish(object, needs_young_bit_update, needs_black_allocation,
                       has_old_host);
}

void TracedHandlesImpl::Destroy(TracedNodeBlock& node_block, TracedNode& node) {
  DCHECK_IMPLIES(is_marking_, !is_sweeping_on_mutator_thread_);
  DCHECK_IMPLIES(is_sweeping_on_mutator_thread_, !is_marking_);

  // If sweeping on the mutator thread is running then the handle destruction
  // may be a result of a Reset() call from a destructor. The node will be
  // reclaimed on the next cycle.
  //
  // This allows v8::TracedReference::Reset() calls from destructors on
  // objects that may be used from stack and heap.
  if (is_sweeping_on_mutator_thread_) {
    return;
  }

  if (is_marking_) {
    // Incremental/concurrent marking is running. This also covers the scavenge
    // case which prohibits eagerly reclaiming nodes when marking is on during a
    // scavenge.
    //
    // On-heap traced nodes are released in the atomic pause in
    // `IterateWeakRootsForPhantomHandles()` when they are discovered as not
    // marked. Eagerly clear out the object here to avoid needlessly marking it
    // from this point on. The node will be reclaimed on the next cycle.
    node.set_raw_object<AccessMode::ATOMIC>(kNullAddress);
    return;
  }

  // In case marking and sweeping are off, the handle may be freed immediately.
  // Note that this includes also the case when invoking the first pass
  // callbacks during the atomic pause which requires releasing a node fully.
  FreeNode(&node);
}

void TracedHandlesImpl::Copy(const TracedNode& from_node, Address** to) {
  DCHECK_NE(kGlobalHandleZapValue, from_node.raw_object());
  Handle<Object> o =
      Create(from_node.raw_object(), reinterpret_cast<Address*>(to),
             GlobalHandleStoreMode::kAssigningStore);
  SetSlotThreadSafe(to, o.location());
#ifdef VERIFY_HEAP
  if (v8_flags.verify_heap) {
    Object::ObjectVerify(Tagged<Object>(**to), isolate_);
  }
#endif  // VERIFY_HEAP
}

void TracedHandlesImpl::Move(TracedNode& from_node, Address** from,
                             Address** to) {
  DCHECK(from_node.is_in_use());

  // Deal with old "to".
  auto* to_node = TracedNode::FromLocation(*to);
  DCHECK_IMPLIES(*to, to_node->is_in_use());
  DCHECK_IMPLIES(*to, kGlobalHandleZapValue != to_node->raw_object());
  DCHECK_NE(kGlobalHandleZapValue, from_node.raw_object());
  if (*to) {
    auto& to_node_block = TracedNodeBlock::From(*to_node);
    Destroy(to_node_block, *to_node);
  }

  // Set "to" to "from".
  SetSlotThreadSafe(to, *from);
  to_node = &from_node;

  // Deal with new "to"
  DCHECK_NOT_NULL(*to);
  DCHECK_EQ(*from, *to);
  if (is_marking_) {
    // Write barrier needs to cover node as well as object.
    to_node->set_markbit<AccessMode::ATOMIC>();
    WriteBarrier::MarkingFromGlobalHandle(to_node->object());
  } else if (auto* cpp_heap = GetCppHeapIfUnifiedYoungGC(isolate_)) {
    const bool object_is_young_and_not_yet_recorded =
        !from_node.has_old_host() &&
        ObjectInYoungGeneration(from_node.object());
    if (object_is_young_and_not_yet_recorded &&
        IsCppGCHostOld(*cpp_heap, reinterpret_cast<Address>(to))) {
      DCHECK(from_node.is_in_young_list());
      from_node.set_has_old_host(true);
    }
  }
  SetSlotThreadSafe(from, nullptr);
}

void TracedHandlesImpl::SetIsMarking(bool value) {
  DCHECK_EQ(is_marking_, !value);
  is_marking_ = value;
}

void TracedHandlesImpl::SetIsSweepingOnMutatorThread(bool value) {
  DCHECK_EQ(is_sweeping_on_mutator_thread_, !value);
  is_sweeping_on_mutator_thread_ = value;
}

const TracedHandles::NodeBounds TracedHandlesImpl::GetNodeBounds() const {
  TracedHandles::NodeBounds block_bounds;
  block_bounds.reserve(blocks_.Size());
  for (const auto* block : blocks_) {
    block_bounds.push_back(
        {block->nodes_begin_address(), block->nodes_end_address()});
  }
  std::sort(block_bounds.begin(), block_bounds.end(),
            [](const auto& pair1, const auto& pair2) {
              return pair1.first < pair2.first;
            });
  return block_bounds;
}

void TracedHandlesImpl::UpdateListOfYoungNodes() {
  size_t last = 0;
  const bool needs_to_mark_as_old =
      static_cast<bool>(GetCppHeapIfUnifiedYoungGC(isolate_));
  for (auto* node : young_nodes_) {
    DCHECK(node->is_in_young_list());
    if (node->is_in_use() && ObjectInYoungGeneration(node->object())) {
      young_nodes_[last++] = node;
      // The node was discovered through a cppgc object, which will be
      // immediately promoted. Remember the object.
      if (needs_to_mark_as_old) node->set_has_old_host(true);
    } else {
      node->set_is_in_young_list(false);
      node->set_has_old_host(false);
    }
  }
  DCHECK_LE(last, young_nodes_.size());
  young_nodes_.resize(last);
  young_nodes_.shrink_to_fit();
  empty_blocks_.insert(empty_blocks_.end(), empty_block_candidates_.begin(),
                       empty_block_candidates_.end());
  empty_block_candidates_.clear();
  empty_block_candidates_.shrink_to_fit();
}

void TracedHandlesImpl::ClearListOfYoungNodes() {
  for (auto* node : young_nodes_) {
    DCHECK(node->is_in_young_list());
    // Nodes in use and not in use can have this bit set to false.
    node->set_is_in_young_list(false);
    node->set_has_old_host(false);
  }
  young_nodes_.clear();
  young_nodes_.shrink_to_fit();
  empty_blocks_.insert(empty_blocks_.end(), empty_block_candidates_.begin(),
                       empty_block_candidates_.end());
  empty_block_candidates_.clear();
  empty_block_candidates_.shrink_to_fit();
}

void TracedHandlesImpl::DeleteEmptyBlocks() {
  // Keep one node block around for fast allocation/deallocation patterns.
  if (empty_blocks_.size() <= 1) return;

  for (size_t i = 1; i < empty_blocks_.size(); i++) {
    auto* block = empty_blocks_[i];
    DCHECK(block->IsEmpty());
    DCHECK_GE(block_size_bytes_, block->size_bytes());
    block_size_bytes_ -= block->size_bytes();
    TracedNodeBlock::Delete(block);
  }
  empty_blocks_.resize(1);
  empty_blocks_.shrink_to_fit();
}

void TracedHandlesImpl::ResetDeadNodes(
    WeakSlotCallbackWithHeap should_reset_handle) {
  // Manual iteration as the block may be deleted in `FreeNode()`.
  for (auto it = blocks_.begin(); it != blocks_.end();) {
    auto* block = *(it++);
    for (auto* node : *block) {
      if (!node->is_in_use()) continue;

      // Detect unreachable nodes first.
      if (!node->markbit()) {
        FreeNode(node);
        continue;
      }

      // Node was reachable. Clear the markbit for the next GC.
      node->clear_markbit();
      // TODO(v8:13141): Turn into a DCHECK after some time.
      CHECK(!should_reset_handle(isolate_->heap(), node->location()));
    }
  }
}

void TracedHandlesImpl::ResetYoungDeadNodes(
    WeakSlotCallbackWithHeap should_reset_handle) {
  for (auto* node : young_nodes_) {
    DCHECK(node->is_in_young_list());
    DCHECK_IMPLIES(node->has_old_host(), node->markbit());

    if (!node->is_in_use()) continue;

    if (!node->markbit()) {
      FreeNode(node);
      continue;
    }

    // Node was reachable. Clear the markbit for the next GC.
    node->clear_markbit();
    // TODO(v8:13141): Turn into a DCHECK after some time.
    CHECK(!should_reset_handle(isolate_->heap(), node->location()));
  }
}

void TracedHandlesImpl::ComputeWeaknessForYoungObjects(
    WeakSlotCallback is_unmodified) {
  if (!v8_flags.reclaim_unmodified_wrappers) return;

  // Treat all objects as roots during incremental marking to avoid corrupting
  // marking worklists.
  if (is_marking_) return;

  auto* const handler = isolate_->heap()->GetEmbedderRootsHandler();
  if (!handler) return;

  for (TracedNode* node : young_nodes_) {
    if (node->is_in_use()) {
      DCHECK(node->is_root());
      if (is_unmodified(node->location())) {
        v8::Value* value = ToApi<v8::Value>(node->handle());
        bool r = handler->IsRoot(
            *reinterpret_cast<v8::TracedReference<v8::Value>*>(&value));
        node->set_root(r);
      }
    }
  }
}

void TracedHandlesImpl::ProcessYoungObjects(
    RootVisitor* visitor, WeakSlotCallbackWithHeap should_reset_handle) {
  if (!v8_flags.reclaim_unmodified_wrappers) return;

  auto* const handler = isolate_->heap()->GetEmbedderRootsHandler();
  if (!handler) return;

  // If CppGC is attached, since the embeeder may trigger allocations in
  // ResetRoot().
  if (auto* cpp_heap = CppHeap::From(isolate_->heap()->cpp_heap())) {
    cpp_heap->EnterNoGCScope();
  }

  for (TracedNode* node : young_nodes_) {
    if (!node->is_in_use()) continue;

    bool should_reset = should_reset_handle(isolate_->heap(), node->location());
    CHECK_IMPLIES(node->is_root(), !should_reset);
    if (should_reset) {
      CHECK(!is_marking_);
      v8::Value* value = ToApi<v8::Value>(node->handle());
      handler->ResetRoot(
          *reinterpret_cast<v8::TracedReference<v8::Value>*>(&value));
      // We cannot check whether a node is in use here as the reset behavior
      // depends on whether incremental marking is running when reclaiming
      // young objects.
    } else {
      if (!node->is_root()) {
        node->set_root(true);
        if (visitor) {
          visitor->VisitRootPointer(Root::kGlobalHandles, nullptr,
                                    node->location());
        }
      }
    }
  }

  if (auto* cpp_heap = CppHeap::From(isolate_->heap()->cpp_heap())) {
    cpp_heap->LeaveNoGCScope();
  }
}

void TracedHandlesImpl::Iterate(RootVisitor* visitor) {
  for (auto* block : blocks_) {
    for (auto* node : *block) {
      if (!node->is_in_use()) continue;

      visitor->VisitRootPointer(Root::kTracedHandles, nullptr,
                                node->location());
    }
  }
}

void TracedHandlesImpl::IterateYoung(RootVisitor* visitor) {
  for (auto* node : young_nodes_) {
    if (!node->is_in_use()) continue;

    visitor->VisitRootPointer(Root::kTracedHandles, nullptr, node->location());
  }
}

void TracedHandlesImpl::IterateYoungRoots(RootVisitor* visitor) {
  for (auto* node : young_nodes_) {
    if (!node->is_in_use()) continue;

    CHECK_IMPLIES(is_marking_, node->is_root());

    if (!node->is_root()) continue;

    visitor->VisitRootPointer(Root::kTracedHandles, nullptr, node->location());
  }
}

void TracedHandlesImpl::IterateAndMarkYoungRootsWithOldHosts(
    RootVisitor* visitor) {
  for (auto* node : young_nodes_) {
    if (!node->is_in_use()) continue;
    if (!node->has_old_host()) continue;

    CHECK_IMPLIES(is_marking_, node->is_root());

    if (!node->is_root()) continue;

    node->set_markbit();
    CHECK(ObjectInYoungGeneration(node->object()));
    visitor->VisitRootPointer(Root::kTracedHandles, nullptr, node->location());
  }
}

void TracedHandlesImpl::IterateYoungRootsWithOldHostsForTesting(
    RootVisitor* visitor) {
  for (auto* node : young_nodes_) {
    if (!node->is_in_use()) continue;
    if (!node->has_old_host()) continue;

    CHECK_IMPLIES(is_marking_, node->is_root());

    if (!node->is_root()) continue;

    visitor->VisitRootPointer(Root::kTracedHandles, nullptr, node->location());
  }
}

TracedHandles::TracedHandles(Isolate* isolate)
    : impl_(std::make_unique<TracedHandlesImpl>(isolate)) {}

TracedHandles::~TracedHandles() = default;

Handle<Object> TracedHandles::Create(Address value, Address* slot,
                                     GlobalHandleStoreMode store_mode) {
  return impl_->Create(value, slot, store_mode);
}

void TracedHandles::SetIsMarking(bool value) { impl_->SetIsMarking(value); }

void TracedHandles::SetIsSweepingOnMutatorThread(bool value) {
  impl_->SetIsSweepingOnMutatorThread(value);
}

const TracedHandles::NodeBounds TracedHandles::GetNodeBounds() const {
  return impl_->GetNodeBounds();
}

void TracedHandles::UpdateListOfYoungNodes() {
  impl_->UpdateListOfYoungNodes();
}

void TracedHandles::ClearListOfYoungNodes() { impl_->ClearListOfYoungNodes(); }

void TracedHandles::DeleteEmptyBlocks() { impl_->DeleteEmptyBlocks(); }

void TracedHandles::ResetDeadNodes(
    WeakSlotCallbackWithHeap should_reset_handle) {
  impl_->ResetDeadNodes(should_reset_handle);
}

void TracedHandles::ResetYoungDeadNodes(
    WeakSlotCallbackWithHeap should_reset_handle) {
  impl_->ResetYoungDeadNodes(should_reset_handle);
}

void TracedHandles::ComputeWeaknessForYoungObjects(
    WeakSlotCallback is_unmodified) {
  impl_->ComputeWeaknessForYoungObjects(is_unmodified);
}

void TracedHandles::ProcessYoungObjects(
    RootVisitor* visitor, WeakSlotCallbackWithHeap should_reset_handle) {
  impl_->ProcessYoungObjects(visitor, should_reset_handle);
}

void TracedHandles::Iterate(RootVisitor* visitor) { impl_->Iterate(visitor); }

void TracedHandles::IterateYoung(RootVisitor* visitor) {
  impl_->IterateYoung(visitor);
}

void TracedHandles::IterateYoungRoots(RootVisitor* visitor) {
  impl_->IterateYoungRoots(visitor);
}

void TracedHandles::IterateAndMarkYoungRootsWithOldHosts(RootVisitor* visitor) {
  impl_->IterateAndMarkYoungRootsWithOldHosts(visitor);
}

void TracedHandles::IterateYoungRootsWithOldHostsForTesting(
    RootVisitor* visitor) {
  impl_->IterateYoungRootsWithOldHostsForTesting(visitor);
}

size_t TracedHandles::used_node_count() const {
  return impl_->used_node_count();
}

size_t TracedHandles::total_size_bytes() const {
  return impl_->total_size_bytes();
}

size_t TracedHandles::used_size_bytes() const {
  return impl_->used_size_bytes();
}

// static
void TracedHandles::Destroy(Address* location) {
  if (!location) return;

  auto* node = TracedNode::FromLocation(location);
  auto& node_block = TracedNodeBlock::From(*node);
  auto& traced_handles = node_block.traced_handles();
  traced_handles.Destroy(node_block, *node);
}

// static
void TracedHandles::Copy(const Address* const* from, Address** to) {
  DCHECK_NOT_NULL(*from);
  DCHECK_NULL(*to);

  const TracedNode* from_node = TracedNode::FromLocation(*from);
  const auto& node_block = TracedNodeBlock::From(*from_node);
  auto& traced_handles = node_block.traced_handles();
  traced_handles.Copy(*from_node, to);
}

// static
void TracedHandles::Move(Address** from, Address** to) {
  // Fast path for moving from an empty reference.
  if (!*from) {
    Destroy(*to);
    SetSlotThreadSafe(to, nullptr);
    return;
  }

  TracedNode* from_node = TracedNode::FromLocation(*from);
  auto& node_block = TracedNodeBlock::From(*from_node);
  auto& traced_handles = node_block.traced_handles();
  traced_handles.Move(*from_node, from, to);
}

namespace {
Tagged<Object> MarkObject(Tagged<Object> obj, TracedNode& node,
                          TracedHandles::MarkMode mark_mode) {
  if (mark_mode == TracedHandles::MarkMode::kOnlyYoung &&
      !node.is_in_young_list())
    return Smi::zero();
  node.set_markbit<AccessMode::ATOMIC>();
  // Being in the young list, the node may still point to an old object, in
  // which case we want to keep the node marked, but not follow the reference.
  if (mark_mode == TracedHandles::MarkMode::kOnlyYoung &&
      !ObjectInYoungGeneration(obj))
    return Smi::zero();
  return obj;
}
}  // namespace

// static
Tagged<Object> TracedHandles::Mark(Address* location, MarkMode mark_mode) {
  // The load synchronizes internal bitfields that are also read atomically
  // from the concurrent marker. The counterpart is `TracedNode::Publish()`.
  Tagged<Object> object =
      Tagged<Object>(reinterpret_cast<std::atomic<Address>*>(location)->load(
          std::memory_order_acquire));
  auto* node = TracedNode::FromLocation(location);
  DCHECK(node->is_in_use<AccessMode::ATOMIC>());
  return MarkObject(object, *node, mark_mode);
}

// static
Tagged<Object> TracedHandles::MarkConservatively(
    Address* inner_location, Address* traced_node_block_base,
    MarkMode mark_mode) {
  // Compute the `TracedNode` address based on its inner pointer.
  const ptrdiff_t delta = reinterpret_cast<uintptr_t>(inner_location) -
                          reinterpret_cast<uintptr_t>(traced_node_block_base);
  const auto index = delta / sizeof(TracedNode);
  TracedNode& node =
      reinterpret_cast<TracedNode*>(traced_node_block_base)[index];
  // `MarkConservatively()` runs concurrently with marking code. Reading
  // state concurrently to setting the markbit is safe.
  if (!node.is_in_use<AccessMode::ATOMIC>()) return Smi::zero();
  return MarkObject(node.object(), node, mark_mode);
}

bool TracedHandles::HasYoung() const { return impl_->HasYoung(); }

}  // namespace v8::internal
