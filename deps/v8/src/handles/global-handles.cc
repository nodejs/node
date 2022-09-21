// Copyright 2009 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/handles/global-handles.h"

#include <algorithm>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <map>

#include "include/v8-traced-handle.h"
#include "src/api/api-inl.h"
#include "src/base/compiler-specific.h"
#include "src/base/logging.h"
#include "src/base/sanitizer/asan.h"
#include "src/common/globals.h"
#include "src/execution/vm-state-inl.h"
#include "src/heap/base/stack.h"
#include "src/heap/embedder-tracing.h"
#include "src/heap/heap-inl.h"
#include "src/heap/heap-write-barrier-inl.h"
#include "src/heap/heap-write-barrier.h"
#include "src/init/v8.h"
#include "src/logging/counters.h"
#include "src/objects/objects-inl.h"
#include "src/objects/slots.h"
#include "src/objects/visitors.h"
#include "src/tasks/cancelable-task.h"
#include "src/tasks/task-utils.h"
#include "src/utils/utils.h"

namespace v8 {
namespace internal {

namespace {

constexpr size_t kBlockSize = 256;

}  // namespace

// Various internal weakness types for Persistent and Global handles.
enum class WeaknessType {
  // Weakness with custom callback and an embedder-provided parameter.
  kCallback,
  // Weakness with custom callback and an embedder-provided parameter. In
  // addition the first two embedder fields are passed along. Note that the
  // internal fields must contain aligned non-V8 pointers. Getting pointers to
  // V8 objects through this interface would be GC unsafe so in that case the
  // embedder gets a null pointer instead.
  kCallbackWithTwoEmbedderFields,
  // Weakness where the handle is automatically reset in the garbage collector
  // when the object is no longer reachable.
  kNoCallback,
};

template <class _NodeType>
class GlobalHandles::NodeBlock final {
 public:
  using BlockType = NodeBlock<_NodeType>;
  using NodeType = _NodeType;

  V8_INLINE static const NodeBlock* From(const NodeType* node);
  V8_INLINE static NodeBlock* From(NodeType* node);

  NodeBlock(GlobalHandles* global_handles,
            GlobalHandles::NodeSpace<NodeType>* space,
            NodeBlock* next) V8_NOEXCEPT : next_(next),
                                           global_handles_(global_handles),
                                           space_(space) {}

  NodeBlock(const NodeBlock&) = delete;
  NodeBlock& operator=(const NodeBlock&) = delete;

  NodeType* at(size_t index) { return &nodes_[index]; }
  const NodeType* at(size_t index) const { return &nodes_[index]; }
  GlobalHandles::NodeSpace<NodeType>* space() const { return space_; }
  GlobalHandles* global_handles() const { return global_handles_; }

  V8_INLINE bool IncreaseUsage();
  V8_INLINE bool DecreaseUsage();

  V8_INLINE void ListAdd(NodeBlock** top);
  V8_INLINE void ListRemove(NodeBlock** top);

  NodeBlock* next() const { return next_; }
  NodeBlock* next_used() const { return next_used_; }

  const void* begin_address() const { return nodes_; }
  const void* end_address() const { return &nodes_[kBlockSize]; }

 private:
  NodeType nodes_[kBlockSize];
  NodeBlock* const next_;
  GlobalHandles* const global_handles_;
  GlobalHandles::NodeSpace<NodeType>* const space_;
  NodeBlock* next_used_ = nullptr;
  NodeBlock* prev_used_ = nullptr;
  uint32_t used_nodes_ = 0;
};

template <class NodeType>
const GlobalHandles::NodeBlock<NodeType>*
GlobalHandles::NodeBlock<NodeType>::From(const NodeType* node) {
  const NodeType* firstNode = node - node->index();
  const BlockType* block = reinterpret_cast<const BlockType*>(firstNode);
  DCHECK_EQ(node, block->at(node->index()));
  return block;
}

template <class NodeType>
GlobalHandles::NodeBlock<NodeType>* GlobalHandles::NodeBlock<NodeType>::From(
    NodeType* node) {
  NodeType* firstNode = node - node->index();
  BlockType* block = reinterpret_cast<BlockType*>(firstNode);
  DCHECK_EQ(node, block->at(node->index()));
  return block;
}

template <class NodeType>
bool GlobalHandles::NodeBlock<NodeType>::IncreaseUsage() {
  DCHECK_LT(used_nodes_, kBlockSize);
  return used_nodes_++ == 0;
}

template <class NodeType>
void GlobalHandles::NodeBlock<NodeType>::ListAdd(BlockType** top) {
  BlockType* old_top = *top;
  *top = this;
  next_used_ = old_top;
  prev_used_ = nullptr;
  if (old_top != nullptr) {
    old_top->prev_used_ = this;
  }
}

template <class NodeType>
bool GlobalHandles::NodeBlock<NodeType>::DecreaseUsage() {
  DCHECK_GT(used_nodes_, 0);
  return --used_nodes_ == 0;
}

template <class NodeType>
void GlobalHandles::NodeBlock<NodeType>::ListRemove(BlockType** top) {
  if (next_used_ != nullptr) next_used_->prev_used_ = prev_used_;
  if (prev_used_ != nullptr) prev_used_->next_used_ = next_used_;
  if (this == *top) {
    *top = next_used_;
  }
}

template <class BlockType>
class GlobalHandles::NodeIterator final {
 public:
  using NodeType = typename BlockType::NodeType;

  // Iterator traits.
  using iterator_category = std::forward_iterator_tag;
  using difference_type = std::ptrdiff_t;
  using value_type = NodeType*;
  using reference = value_type;
  using pointer = value_type*;

  explicit NodeIterator(BlockType* block) V8_NOEXCEPT : block_(block) {}
  NodeIterator(NodeIterator&& other) V8_NOEXCEPT : block_(other.block_),
                                                   index_(other.index_) {}

  NodeIterator(const NodeIterator&) = delete;
  NodeIterator& operator=(const NodeIterator&) = delete;

  bool operator==(const NodeIterator& other) const {
    return block_ == other.block_;
  }
  bool operator!=(const NodeIterator& other) const {
    return block_ != other.block_;
  }

  NodeIterator& operator++() {
    if (++index_ < kBlockSize) return *this;
    index_ = 0;
    block_ = block_->next_used();
    return *this;
  }

  NodeType* operator*() { return block_->at(index_); }
  NodeType* operator->() { return block_->at(index_); }

 private:
  BlockType* block_ = nullptr;
  size_t index_ = 0;
};

template <class NodeType>
class GlobalHandles::NodeSpace final {
 public:
  using BlockType = NodeBlock<NodeType>;
  using iterator = NodeIterator<BlockType>;
  using NodeBounds = GlobalHandles::NodeBounds;

  static NodeSpace* From(NodeType* node);
  static void Release(NodeType* node);

  explicit NodeSpace(GlobalHandles* global_handles) V8_NOEXCEPT
      : global_handles_(global_handles) {}
  ~NodeSpace();

  V8_INLINE NodeType* Allocate();

  iterator begin() { return iterator(first_used_block_); }
  iterator end() { return iterator(nullptr); }

  size_t TotalSize() const { return blocks_ * sizeof(NodeType) * kBlockSize; }
  size_t handles_count() const { return handles_count_; }

  NodeBounds GetNodeBlockBounds() const;

 private:
  void PutNodesOnFreeList(BlockType* block);
  V8_INLINE void Free(NodeType* node);

  GlobalHandles* const global_handles_;
  BlockType* first_block_ = nullptr;
  BlockType* first_used_block_ = nullptr;
  NodeType* first_free_ = nullptr;
  size_t blocks_ = 0;
  size_t handles_count_ = 0;
};

template <class NodeType>
GlobalHandles::NodeSpace<NodeType>::~NodeSpace() {
  auto* block = first_block_;
  while (block != nullptr) {
    auto* tmp = block->next();
    delete block;
    block = tmp;
  }
}

template <class NodeType>
NodeType* GlobalHandles::NodeSpace<NodeType>::Allocate() {
  if (first_free_ == nullptr) {
    first_block_ = new BlockType(global_handles_, this, first_block_);
    blocks_++;
    PutNodesOnFreeList(first_block_);
  }
  DCHECK_NOT_NULL(first_free_);
  NodeType* node = first_free_;
  first_free_ = first_free_->next_free();
  BlockType* block = BlockType::From(node);
  if (block->IncreaseUsage()) {
    block->ListAdd(&first_used_block_);
  }
  global_handles_->isolate()->counters()->global_handles()->Increment();
  handles_count_++;
  node->CheckNodeIsFreeNode();
  return node;
}

template <class NodeType>
typename GlobalHandles::NodeSpace<NodeType>::NodeBounds
GlobalHandles::NodeSpace<NodeType>::GetNodeBlockBounds() const {
  NodeBounds block_bounds;
  for (BlockType* current = first_used_block_; current;
       current = current->next_used()) {
    block_bounds.push_back({current->begin_address(), current->end_address()});
  }
  std::sort(block_bounds.begin(), block_bounds.end(),
            [](const auto& pair1, const auto& pair2) {
              return pair1.first < pair2.first;
            });
  return block_bounds;
}

template <class NodeType>
void GlobalHandles::NodeSpace<NodeType>::PutNodesOnFreeList(BlockType* block) {
  for (int32_t i = kBlockSize - 1; i >= 0; --i) {
    NodeType* node = block->at(i);
    const uint8_t index = static_cast<uint8_t>(i);
    DCHECK_EQ(i, index);
    node->set_index(index);
    node->Free(first_free_);
    first_free_ = node;
  }
}

template <class NodeType>
void GlobalHandles::NodeSpace<NodeType>::Release(NodeType* node) {
  BlockType* block = BlockType::From(node);
  block->space()->Free(node);
}

template <class NodeType>
void GlobalHandles::NodeSpace<NodeType>::Free(NodeType* node) {
  node->Release(first_free_);
  first_free_ = node;
  BlockType* block = BlockType::From(node);
  if (block->DecreaseUsage()) {
    block->ListRemove(&first_used_block_);
  }
  global_handles_->isolate()->counters()->global_handles()->Decrement();
  handles_count_--;
}

template <class Child>
class NodeBase {
 public:
  static const Child* FromLocation(const Address* location) {
    return reinterpret_cast<const Child*>(location);
  }

  static Child* FromLocation(Address* location) {
    return reinterpret_cast<Child*>(location);
  }

  NodeBase() {
    DCHECK_EQ(offsetof(NodeBase, object_), 0);
    DCHECK_EQ(offsetof(NodeBase, class_id_), Internals::kNodeClassIdOffset);
    DCHECK_EQ(offsetof(NodeBase, flags_), Internals::kNodeFlagsOffset);
  }

#ifdef ENABLE_HANDLE_ZAPPING
  ~NodeBase() {
    ClearFields();
    data_.next_free = nullptr;
    index_ = 0;
  }
#endif

  void Free(Child* free_list) {
    ClearFields();
    AsChild()->MarkAsFree();
    data_.next_free = free_list;
  }

  // Publishes all internal state to be consumed by other threads.
  Handle<Object> Publish(Object object) {
    DCHECK(!AsChild()->IsInUse());
    data_.parameter = nullptr;
    AsChild()->MarkAsUsed();
    reinterpret_cast<std::atomic<Address>*>(&object_)->store(
        object.ptr(), std::memory_order_release);
    DCHECK(AsChild()->IsInUse());
    return handle();
  }

  void Release(Child* free_list) {
    DCHECK(AsChild()->IsInUse());
    Free(free_list);
    DCHECK(!AsChild()->IsInUse());
  }

  Object object() const { return Object(object_); }
  FullObjectSlot location() { return FullObjectSlot(&object_); }
  Handle<Object> handle() { return Handle<Object>(&object_); }
  Address raw_object() const { return object_; }

  uint8_t index() const { return index_; }
  void set_index(uint8_t value) { index_ = value; }

  uint16_t wrapper_class_id() const { return class_id_; }
  bool has_wrapper_class_id() const {
    return class_id_ != v8::HeapProfiler::kPersistentHandleNoClassId;
  }

  // Accessors for next free node in the free list.
  Child* next_free() {
    DCHECK(!AsChild()->IsInUse());
    return data_.next_free;
  }

  void set_parameter(void* parameter) {
    DCHECK(AsChild()->IsInUse());
    data_.parameter = parameter;
  }
  void* parameter() const {
    DCHECK(AsChild()->IsInUse());
    return data_.parameter;
  }

  void CheckNodeIsFreeNode() const {
    DCHECK_EQ(kGlobalHandleZapValue, object_);
    DCHECK_EQ(v8::HeapProfiler::kPersistentHandleNoClassId, class_id_);
    AsChild()->CheckNodeIsFreeNodeImpl();
  }

 protected:
  Child* AsChild() { return reinterpret_cast<Child*>(this); }
  const Child* AsChild() const { return reinterpret_cast<const Child*>(this); }

  void ClearFields() {
    // Zap the values for eager trapping.
    object_ = kGlobalHandleZapValue;
    class_id_ = v8::HeapProfiler::kPersistentHandleNoClassId;
    AsChild()->ClearImplFields();
  }

  // Storage for object pointer.
  //
  // Placed first to avoid offset computation. The stored data is equivalent to
  // an Object. It is stored as a plain Address for convenience (smallest number
  // of casts), and because it is a private implementation detail: the public
  // interface provides type safety.
  Address object_ = kNullAddress;

  // Class id set by the embedder.
  uint16_t class_id_ = 0;

  // Index in the containing handle block.
  uint8_t index_ = 0;

  uint8_t flags_ = 0;

  // The meaning of this field depends on node state:
  // - Node in free list: Stores next free node pointer.
  // - Otherwise, specific to the node implementation.
  union {
    Child* next_free = nullptr;
    void* parameter;
  } data_;
};

namespace {

void ExtractInternalFields(JSObject jsobject, void** embedder_fields, int len) {
  int field_count = jsobject.GetEmbedderFieldCount();
  Isolate* isolate = GetIsolateForSandbox(jsobject);
  for (int i = 0; i < len; ++i) {
    if (field_count == i) break;
    void* pointer;
    if (EmbedderDataSlot(jsobject, i).ToAlignedPointer(isolate, &pointer)) {
      embedder_fields[i] = pointer;
    }
  }
}

}  // namespace

class GlobalHandles::Node final : public NodeBase<GlobalHandles::Node> {
 public:
  // State transition diagram:
  // FREE -> NORMAL <-> WEAK -> {NEAR_DEATH, FREE} -> FREE
  enum State {
    FREE = 0,
    // Strong global handle.
    NORMAL,
    // Flagged as weak and still considered as live.
    WEAK,
    // Temporary state used in GC to sanity check that handles are reset in
    // their first pass callback.
    NEAR_DEATH,
  };

  Node() {
    static_assert(static_cast<int>(NodeState::kMask) ==
                  Internals::kNodeStateMask);
    static_assert(WEAK == Internals::kNodeStateIsWeakValue);
    set_in_young_list(false);
  }

  Node(const Node&) = delete;
  Node& operator=(const Node&) = delete;

  const char* label() const {
    return state() == NORMAL ? reinterpret_cast<char*>(data_.parameter)
                             : nullptr;
  }

  // State and flag accessors.

  State state() const { return NodeState::decode(flags_); }
  void set_state(State state) { flags_ = NodeState::update(flags_, state); }

  bool is_in_young_list() const { return IsInYoungList::decode(flags_); }
  void set_in_young_list(bool v) { flags_ = IsInYoungList::update(flags_, v); }

  WeaknessType weakness_type() const {
    return NodeWeaknessType::decode(flags_);
  }
  void set_weakness_type(WeaknessType weakness_type) {
    flags_ = NodeWeaknessType::update(flags_, weakness_type);
  }

  bool IsWeak() const { return state() == WEAK; }

  bool IsInUse() const { return state() != FREE; }

  bool IsPhantomResetHandle() const {
    return weakness_type() == WeaknessType::kNoCallback;
  }

  bool IsWeakOrStrongRetainer() const {
    return state() == NORMAL || state() == WEAK;
  }

  bool IsStrongRetainer() const { return state() == NORMAL; }

  bool IsWeakRetainer() const { return state() == WEAK; }

  bool has_callback() const { return weak_callback_ != nullptr; }

  // Accessors for next free node in the free list.
  Node* next_free() {
    DCHECK_EQ(FREE, state());
    return data_.next_free;
  }

  void MakeWeak(void* parameter,
                WeakCallbackInfo<void>::Callback phantom_callback,
                v8::WeakCallbackType type) {
    DCHECK_NOT_NULL(phantom_callback);
    DCHECK(IsInUse());
    CHECK_NE(object_, kGlobalHandleZapValue);
    set_state(WEAK);
    switch (type) {
      case v8::WeakCallbackType::kParameter:
        set_weakness_type(WeaknessType::kCallback);
        break;
      case v8::WeakCallbackType::kInternalFields:
        set_weakness_type(WeaknessType::kCallbackWithTwoEmbedderFields);
        break;
    }
    set_parameter(parameter);
    weak_callback_ = phantom_callback;
  }

  void MakeWeak(Address** location_addr) {
    DCHECK(IsInUse());
    CHECK_NE(object_, kGlobalHandleZapValue);
    set_state(WEAK);
    set_weakness_type(WeaknessType::kNoCallback);
    set_parameter(location_addr);
    weak_callback_ = nullptr;
  }

  void* ClearWeakness() {
    DCHECK(IsInUse());
    void* p = parameter();
    set_state(NORMAL);
    set_parameter(nullptr);
    return p;
  }

  void AnnotateStrongRetainer(const char* label) {
    DCHECK_EQ(state(), NORMAL);
    data_.parameter = const_cast<char*>(label);
  }

  void CollectPhantomCallbackData(
      std::vector<std::pair<Node*, PendingPhantomCallback>>*
          pending_phantom_callbacks) {
    DCHECK(weakness_type() == WeaknessType::kCallback ||
           weakness_type() == WeaknessType::kCallbackWithTwoEmbedderFields);
    DCHECK_NOT_NULL(weak_callback_);

    void* embedder_fields[v8::kEmbedderFieldsInWeakCallback] = {nullptr,
                                                                nullptr};
    if (weakness_type() == WeaknessType::kCallbackWithTwoEmbedderFields &&
        object().IsJSObject()) {
      ExtractInternalFields(JSObject::cast(object()), embedder_fields,
                            v8::kEmbedderFieldsInWeakCallback);
    }

    // Zap with something dangerous.
    location().store(Object(0xCA11));

    pending_phantom_callbacks->push_back(std::make_pair(
        this,
        PendingPhantomCallback(weak_callback_, parameter(), embedder_fields)));
    DCHECK(IsInUse());
    set_state(NEAR_DEATH);
  }

  void ResetPhantomHandle() {
    DCHECK_EQ(WeaknessType::kNoCallback, weakness_type());
    DCHECK_NULL(weak_callback_);
    Address** handle = reinterpret_cast<Address**>(parameter());
    *handle = nullptr;
    NodeSpace<Node>::Release(this);
  }

  void MarkAsFree() { set_state(FREE); }
  void MarkAsUsed() { set_state(NORMAL); }

  GlobalHandles* global_handles() {
    return NodeBlock<Node>::From(this)->global_handles();
  }

 private:
  // Fields that are not used for managing node memory.
  void ClearImplFields() { weak_callback_ = nullptr; }

  void CheckNodeIsFreeNodeImpl() const {
    DCHECK_EQ(nullptr, weak_callback_);
    DCHECK(!IsInUse());
  }

  // This stores three flags (independent, partially_dependent and
  // in_young_list) and a State.
  using NodeState = base::BitField8<State, 0, 2>;
  // Tracks whether the node is contained in the set of young nodes. This bit
  // persists across allocating and freeing a node as it's only cleaned up
  // when young nodes are proccessed.
  using IsInYoungList = NodeState::Next<bool, 1>;
  using NodeWeaknessType = IsInYoungList::Next<WeaknessType, 2>;

  // Handle specific callback - might be a weak reference in disguise.
  WeakCallbackInfo<void>::Callback weak_callback_;

  friend class NodeBase<Node>;
};

class GlobalHandles::TracedNode final
    : public NodeBase<GlobalHandles::TracedNode> {
 public:
  TracedNode() {
    DCHECK(!is_in_young_list());
    DCHECK(!markbit());
  }

  // Copy and move ctors are used when constructing a TracedNode when recording
  // a node for on-stack data structures. (Older compilers may refer to copy
  // instead of move ctor.)
  TracedNode(TracedNode&& other) V8_NOEXCEPT = default;
  TracedNode(const TracedNode& other) V8_NOEXCEPT = default;

  enum State { FREE = 0, NORMAL, NEAR_DEATH };

  State state() const { return NodeState::decode(flags_); }
  void set_state(State state) { flags_ = NodeState::update(flags_, state); }

  void MarkAsFree() { set_state(FREE); }
  void MarkAsUsed() { set_state(NORMAL); }
  bool IsInUse() const { return state() != FREE; }
  bool IsRetainer() const { return state() == NORMAL; }

  bool is_in_young_list() const { return IsInYoungList::decode(flags_); }
  void set_in_young_list(bool v) { flags_ = IsInYoungList::update(flags_, v); }

  bool is_root() const { return IsRoot::decode(flags_); }
  void set_root(bool v) { flags_ = IsRoot::update(flags_, v); }

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

  void clear_object() {
    reinterpret_cast<std::atomic<Address>*>(&object_)->store(
        kNullAddress, std::memory_order_relaxed);
  }

  void CopyObjectReference(const TracedNode& other) {
    reinterpret_cast<std::atomic<Address>*>(&object_)->store(
        other.object_, std::memory_order_relaxed);
  }

  void ResetPhantomHandle() {
    DCHECK(IsInUse());
    NodeSpace<TracedNode>::Release(this);
    DCHECK(!IsInUse());
  }

  static void Verify(const Address* const* slot);

 protected:
  // Various state is managed in a bit field where some of the state is managed
  // concurrently, whereas other state is managed only on the main thread when
  // no concurrent thread has access to flags, e.g., in the atomic pause of the
  // garbage collector. All state is made available to other threads using
  // `Publish()`.
  //
  // The following state is modified only on the main thread.
  using NodeState = base::BitField8<State, 0, 2>;
  using IsInYoungList = NodeState::Next<bool, 1>;
  using IsRoot = IsInYoungList::Next<bool, 1>;
  // The markbit is the exception as it can be set from the main and marker
  // threads at the same time.
  using Markbit = IsRoot::Next<bool, 1>;

  void ClearImplFields() { set_root(true); }

  void CheckNodeIsFreeNodeImpl() const {
    DCHECK(is_root());
    DCHECK(!markbit());
    DCHECK(!IsInUse());
  }

  friend class NodeBase<GlobalHandles::TracedNode>;
};

// static
void GlobalHandles::EnableMarkingBarrier(Isolate* isolate) {
  auto* global_handles = isolate->global_handles();
  DCHECK(!global_handles->is_marking_);
  global_handles->is_marking_ = true;
}

// static
void GlobalHandles::DisableMarkingBarrier(Isolate* isolate) {
  auto* global_handles = isolate->global_handles();
  DCHECK(global_handles->is_marking_);
  global_handles->is_marking_ = false;
}

// static
void GlobalHandles::TracedNode::Verify(const Address* const* slot) {
#ifdef DEBUG
  const TracedNode* node = FromLocation(*slot);
  auto* global_handles = GlobalHandles::From(node);
  DCHECK(node->IsInUse());
  auto* incremental_marking =
      global_handles->isolate()->heap()->incremental_marking();
  if (incremental_marking && incremental_marking->IsMarking()) {
    Object object = node->object();
    if (object.IsHeapObject()) {
      DCHECK_IMPLIES(node->markbit<AccessMode::ATOMIC>(),
                     !incremental_marking->marking_state()->IsWhite(
                         HeapObject::cast(object)));
    }
  }
  DCHECK_IMPLIES(ObjectInYoungGeneration(node->object()),
                 node->is_in_young_list());
  const bool in_young_list =
      std::find(global_handles->traced_young_nodes_.begin(),
                global_handles->traced_young_nodes_.end(),
                node) != global_handles->traced_young_nodes_.end();
  DCHECK_EQ(in_young_list, node->is_in_young_list());
#endif  // DEBUG
}

size_t GlobalHandles::TotalSize() const {
  return regular_nodes_->TotalSize() + traced_nodes_->TotalSize();
}

size_t GlobalHandles::UsedSize() const {
  return regular_nodes_->handles_count() * sizeof(Node) +
         traced_nodes_->handles_count() * sizeof(TracedNode);
}

size_t GlobalHandles::handles_count() const {
  return regular_nodes_->handles_count() + traced_nodes_->handles_count();
}

GlobalHandles::GlobalHandles(Isolate* isolate)
    : isolate_(isolate),
      regular_nodes_(std::make_unique<NodeSpace<GlobalHandles::Node>>(this)),
      traced_nodes_(
          std::make_unique<NodeSpace<GlobalHandles::TracedNode>>(this)) {}

GlobalHandles::~GlobalHandles() = default;

namespace {

template <typename NodeType>
bool NeedsTrackingInYoungNodes(Object value, NodeType* node) {
  return ObjectInYoungGeneration(value) && !node->is_in_young_list();
}

}  // namespace

Handle<Object> GlobalHandles::Create(Object value) {
  GlobalHandles::Node* node = regular_nodes_->Allocate();
  if (NeedsTrackingInYoungNodes(value, node)) {
    young_nodes_.push_back(node);
    node->set_in_young_list(true);
  }
  return node->Publish(value);
}

Handle<Object> GlobalHandles::Create(Address value) {
  return Create(Object(value));
}

Handle<Object> GlobalHandles::CreateTraced(Object value, Address* slot,
                                           GlobalHandleStoreMode store_mode) {
  GlobalHandles::TracedNode* node = traced_nodes_->Allocate();
  if (NeedsTrackingInYoungNodes(value, node)) {
    traced_young_nodes_.push_back(node);
    node->set_in_young_list(true);
  }
  if (is_marking_ && store_mode != GlobalHandleStoreMode::kInitializingStore) {
    node->set_markbit();
    WriteBarrier::MarkingFromGlobalHandle(value);
  }
  return node->Publish(value);
}

Handle<Object> GlobalHandles::CreateTraced(Address value, Address* slot,
                                           GlobalHandleStoreMode store_mode) {
  return CreateTraced(Object(value), slot, store_mode);
}

Handle<Object> GlobalHandles::CopyGlobal(Address* location) {
  DCHECK_NOT_NULL(location);
  GlobalHandles* global_handles =
      Node::FromLocation(location)->global_handles();
#ifdef VERIFY_HEAP
  if (i::FLAG_verify_heap) {
    Object(*location).ObjectVerify(global_handles->isolate());
  }
#endif  // VERIFY_HEAP
  return global_handles->Create(*location);
}

namespace {
void SetSlotThreadSafe(Address** slot, Address* val) {
  reinterpret_cast<std::atomic<Address*>*>(slot)->store(
      val, std::memory_order_relaxed);
}
}  // namespace

// static
void GlobalHandles::CopyTracedReference(const Address* const* from,
                                        Address** to) {
  DCHECK_NOT_NULL(*from);
  DCHECK_NULL(*to);
  const TracedNode* from_node = TracedNode::FromLocation(*from);
  DCHECK_NE(kGlobalHandleZapValue, from_node->raw_object());
  GlobalHandles* global_handles =
      GlobalHandles::From(const_cast<TracedNode*>(from_node));
  Handle<Object> o = global_handles->CreateTraced(
      from_node->object(), reinterpret_cast<Address*>(to),
      GlobalHandleStoreMode::kAssigningStore);
  SetSlotThreadSafe(to, o.location());
  TracedNode::Verify(from);
  TracedNode::Verify(to);
#ifdef VERIFY_HEAP
  if (i::FLAG_verify_heap) {
    Object(**to).ObjectVerify(global_handles->isolate());
  }
#endif  // VERIFY_HEAP
}

// static
void GlobalHandles::MoveGlobal(Address** from, Address** to) {
  DCHECK_NOT_NULL(*from);
  DCHECK_NOT_NULL(*to);
  DCHECK_EQ(*from, *to);
  Node* node = Node::FromLocation(*from);
  if (node->IsWeak() && node->IsPhantomResetHandle()) {
    node->set_parameter(to);
  }
  // Strong handles do not require fixups.
}

// static
void GlobalHandles::MoveTracedReference(Address** from, Address** to) {
  // Fast path for moving from an empty reference.
  if (!*from) {
    DestroyTracedReference(*to);
    SetSlotThreadSafe(to, nullptr);
    return;
  }

  // Determining whether from or to are on stack.
  TracedNode* from_node = TracedNode::FromLocation(*from);
  DCHECK(from_node->IsInUse());
  TracedNode* to_node = TracedNode::FromLocation(*to);
  // Pure heap move.
  DCHECK_IMPLIES(*to, to_node->IsInUse());
  DCHECK_IMPLIES(*to, kGlobalHandleZapValue != to_node->raw_object());
  DCHECK_NE(kGlobalHandleZapValue, from_node->raw_object());
  DestroyTracedReference(*to);
  SetSlotThreadSafe(to, *from);
  to_node = from_node;
  DCHECK_NOT_NULL(*from);
  DCHECK_NOT_NULL(*to);
  DCHECK_EQ(*from, *to);
  if (GlobalHandles::From(to_node)->is_marking_) {
    // Write barrier needs to cover node as well as object.
    to_node->set_markbit<AccessMode::ATOMIC>();
    WriteBarrier::MarkingFromGlobalHandle(to_node->object());
  }
  SetSlotThreadSafe(from, nullptr);
  TracedNode::Verify(to);
}

// static
GlobalHandles* GlobalHandles::From(const TracedNode* node) {
  return NodeBlock<TracedNode>::From(node)->global_handles();
}

// static
void GlobalHandles::MarkTraced(Address* location) {
  TracedNode* node = TracedNode::FromLocation(location);
  DCHECK(node->IsInUse());
  node->set_markbit<AccessMode::ATOMIC>();
}

// static
Object GlobalHandles::MarkTracedConservatively(
    Address* inner_location, Address* traced_node_block_base) {
  // Compute the `TracedNode` address based on its inner pointer.
  const ptrdiff_t delta = reinterpret_cast<uintptr_t>(inner_location) -
                          reinterpret_cast<uintptr_t>(traced_node_block_base);
  const auto index = delta / sizeof(TracedNode);
  TracedNode& node =
      reinterpret_cast<TracedNode*>(traced_node_block_base)[index];
  if (!node.IsInUse()) return Smi::zero();
  node.set_markbit<AccessMode::ATOMIC>();
  return node.object();
}

void GlobalHandles::Destroy(Address* location) {
  if (location != nullptr) {
    NodeSpace<Node>::Release(Node::FromLocation(location));
  }
}

// static
void GlobalHandles::DestroyTracedReference(Address* location) {
  if (location != nullptr) {
    TracedNode* node = TracedNode::FromLocation(location);
    auto* global_handles = GlobalHandles::From(node);
    // When marking is off the handle may be freed immediately. Note that this
    // includes also the case when invoking the first pass callbacks during the
    // atomic pause which requires releasing a node fully.
    if (!global_handles->is_marking_) {
      NodeSpace<TracedNode>::Release(node);
      return;
    }

    // Incremental marking is on. This also covers the scavenge case which
    // prohibits eagerly reclaiming nodes when marking is on during a scavenge.
    //
    // On-heap traced nodes are released in the atomic pause in
    // `IterateWeakRootsForPhantomHandles()` when they are discovered as not
    // marked. Eagerly clear out the object here to avoid needlessly marking it
    // from this point on. The node will be reclaimed on the next cycle.
    node->clear_object();
  }
}

using GenericCallback = v8::WeakCallbackInfo<void>::Callback;

void GlobalHandles::MakeWeak(Address* location, void* parameter,
                             GenericCallback phantom_callback,
                             v8::WeakCallbackType type) {
  Node::FromLocation(location)->MakeWeak(parameter, phantom_callback, type);
}

void GlobalHandles::MakeWeak(Address** location_addr) {
  Node::FromLocation(*location_addr)->MakeWeak(location_addr);
}

void* GlobalHandles::ClearWeakness(Address* location) {
  return Node::FromLocation(location)->ClearWeakness();
}

void GlobalHandles::AnnotateStrongRetainer(Address* location,
                                           const char* label) {
  Node::FromLocation(location)->AnnotateStrongRetainer(label);
}

bool GlobalHandles::IsWeak(Address* location) {
  return Node::FromLocation(location)->IsWeak();
}

V8_INLINE bool GlobalHandles::ResetWeakNodeIfDead(
    Node* node, WeakSlotCallbackWithHeap should_reset_handle) {
  DCHECK(node->IsWeakRetainer());

  if (!should_reset_handle(isolate()->heap(), node->location())) return false;

  switch (node->weakness_type()) {
    case WeaknessType::kNoCallback:
      node->ResetPhantomHandle();
      break;
    case WeaknessType::kCallback:
      V8_FALLTHROUGH;
    case WeaknessType::kCallbackWithTwoEmbedderFields:
      node->CollectPhantomCallbackData(&regular_pending_phantom_callbacks_);
      break;
  }
  return true;
}

DISABLE_CFI_PERF
void GlobalHandles::IterateWeakRootsForPhantomHandles(
    WeakSlotCallbackWithHeap should_reset_handle) {
  for (Node* node : *regular_nodes_) {
    if (node->IsWeakRetainer()) ResetWeakNodeIfDead(node, should_reset_handle);
  }
  for (TracedNode* node : *traced_nodes_) {
    if (!node->IsInUse()) continue;
    // Detect unreachable nodes first.
    if (!node->markbit()) {
      // The handle itself is unreachable. We can clear it even if the target V8
      // object is alive.
      node->ResetPhantomHandle();
      continue;
    }
    // Clear the markbit for the next GC.
    node->clear_markbit();
    DCHECK(node->IsInUse());
    // TODO(v8:13141): Turn into a DCHECK after some time.
    CHECK(!should_reset_handle(isolate()->heap(), node->location()));
  }
}

void GlobalHandles::ComputeWeaknessForYoungObjects(
    WeakSlotCallback is_unmodified) {
  if (!FLAG_reclaim_unmodified_wrappers) return;

  // Treat all objects as roots during incremental marking to avoid corrupting
  // marking worklists.
  if (isolate()->heap()->incremental_marking()->IsMarking()) return;

  auto* const handler = isolate()->heap()->GetEmbedderRootsHandler();
  for (TracedNode* node : traced_young_nodes_) {
    if (node->IsInUse()) {
      DCHECK(node->is_root());
      if (is_unmodified(node->location())) {
        v8::Value* value = ToApi<v8::Value>(node->handle());
        node->set_root(handler->IsRoot(
            *reinterpret_cast<v8::TracedReference<v8::Value>*>(&value)));
      }
    }
  }
}

void GlobalHandles::IterateYoungStrongAndDependentRoots(RootVisitor* v) {
  for (Node* node : young_nodes_) {
    if (node->IsStrongRetainer()) {
      v->VisitRootPointer(Root::kGlobalHandles, node->label(),
                          node->location());
    }
  }
  for (TracedNode* node : traced_young_nodes_) {
    if (node->IsInUse() && node->is_root()) {
      v->VisitRootPointer(Root::kGlobalHandles, nullptr, node->location());
    }
  }
}

void GlobalHandles::ProcessWeakYoungObjects(
    RootVisitor* v, WeakSlotCallbackWithHeap should_reset_handle) {
  for (Node* node : young_nodes_) {
    DCHECK(node->is_in_young_list());

    if (node->IsWeakRetainer() &&
        !ResetWeakNodeIfDead(node, should_reset_handle)) {
      // Node is weak and alive, so it should be passed onto the visitor.
      v->VisitRootPointer(Root::kGlobalHandles, node->label(),
                          node->location());
    }
  }

  if (!FLAG_reclaim_unmodified_wrappers) return;

  auto* const handler = isolate()->heap()->GetEmbedderRootsHandler();
  for (TracedNode* node : traced_young_nodes_) {
    if (!node->IsInUse()) continue;

    DCHECK_IMPLIES(node->is_root(),
                   !should_reset_handle(isolate_->heap(), node->location()));
    if (should_reset_handle(isolate_->heap(), node->location())) {
      v8::Value* value = ToApi<v8::Value>(node->handle());
      handler->ResetRoot(
          *reinterpret_cast<v8::TracedReference<v8::Value>*>(&value));
      // We cannot check whether a node is in use here as the reset behavior
      // depends on whether incremental marking is running when reclaiming
      // young objects.
    } else {
      if (!node->is_root()) {
        node->set_root(true);
        v->VisitRootPointer(Root::kGlobalHandles, nullptr, node->location());
      }
    }
  }
}

void GlobalHandles::InvokeSecondPassPhantomCallbacks() {
  if (second_pass_callbacks_.empty()) return;

  GCCallbacksScope scope(isolate()->heap());
  // The callbacks may execute JS, which in turn may lead to another GC run.
  // If we are already processing the callbacks, we do not want to start over
  // from within the inner GC. Newly added callbacks will always be run by the
  // outermost GC run only.
  if (scope.CheckReenter()) {
    TRACE_EVENT0("v8", "V8.GCPhantomHandleProcessingCallback");
    isolate()->heap()->CallGCPrologueCallbacks(
        GCType::kGCTypeProcessWeakCallbacks, kNoGCCallbackFlags);
    {
      AllowJavascriptExecution allow_js(isolate());
      while (!second_pass_callbacks_.empty()) {
        auto callback = second_pass_callbacks_.back();
        second_pass_callbacks_.pop_back();
        callback.Invoke(isolate(), PendingPhantomCallback::kSecondPass);
      }
    }
    isolate()->heap()->CallGCEpilogueCallbacks(
        GCType::kGCTypeProcessWeakCallbacks, kNoGCCallbackFlags);
  }
}

namespace {

template <typename T>
void UpdateListOfYoungNodesImpl(Isolate* isolate, std::vector<T*>* node_list) {
  size_t last = 0;
  for (T* node : *node_list) {
    DCHECK(node->is_in_young_list());
    if (node->IsInUse() && node->state() != T::NEAR_DEATH) {
      if (ObjectInYoungGeneration(node->object())) {
        (*node_list)[last++] = node;
        isolate->heap()->IncrementNodesCopiedInNewSpace();
      } else {
        node->set_in_young_list(false);
        isolate->heap()->IncrementNodesPromoted();
      }
    } else {
      node->set_in_young_list(false);
      isolate->heap()->IncrementNodesDiedInNewSpace(1);
    }
  }
  DCHECK_LE(last, node_list->size());
  node_list->resize(last);
  node_list->shrink_to_fit();
}

template <typename T>
void ClearListOfYoungNodesImpl(Isolate* isolate, std::vector<T*>* node_list) {
  for (T* node : *node_list) {
    DCHECK(node->is_in_young_list());
    node->set_in_young_list(false);
    DCHECK_IMPLIES(node->IsInUse() && node->state() != T::NEAR_DEATH,
                   !ObjectInYoungGeneration(node->object()));
  }
  isolate->heap()->IncrementNodesDiedInNewSpace(
      static_cast<int>(node_list->size()));
  node_list->clear();
  node_list->shrink_to_fit();
}

}  // namespace

void GlobalHandles::UpdateListOfYoungNodes() {
  UpdateListOfYoungNodesImpl(isolate_, &young_nodes_);
  UpdateListOfYoungNodesImpl(isolate_, &traced_young_nodes_);
}

void GlobalHandles::ClearListOfYoungNodes() {
  ClearListOfYoungNodesImpl(isolate_, &young_nodes_);
  ClearListOfYoungNodesImpl(isolate_, &traced_young_nodes_);
}

template <typename T>
size_t GlobalHandles::InvokeFirstPassWeakCallbacks(
    std::vector<std::pair<T*, PendingPhantomCallback>>* pending) {
  size_t freed_nodes = 0;
  std::vector<std::pair<T*, PendingPhantomCallback>> pending_phantom_callbacks;
  pending_phantom_callbacks.swap(*pending);
  {
    // The initial pass callbacks must simply clear the nodes.
    for (auto& pair : pending_phantom_callbacks) {
      T* node = pair.first;
      DCHECK_EQ(T::NEAR_DEATH, node->state());
      pair.second.Invoke(isolate(), PendingPhantomCallback::kFirstPass);

      // Transition to second pass. It is required that the first pass callback
      // resets the handle using |v8::PersistentBase::Reset|. Also see comments
      // on |v8::WeakCallbackInfo|.
      CHECK_WITH_MSG(T::FREE == node->state(),
                     "Handle not reset in first callback. See comments on "
                     "|v8::WeakCallbackInfo|.");

      if (pair.second.callback()) second_pass_callbacks_.push_back(pair.second);
      freed_nodes++;
    }
  }
  return freed_nodes;
}

size_t GlobalHandles::InvokeFirstPassWeakCallbacks() {
  return InvokeFirstPassWeakCallbacks(&regular_pending_phantom_callbacks_);
}

void GlobalHandles::PendingPhantomCallback::Invoke(Isolate* isolate,
                                                   InvocationType type) {
  Data::Callback* callback_addr = nullptr;
  if (type == kFirstPass) {
    callback_addr = &callback_;
  }
  Data data(reinterpret_cast<v8::Isolate*>(isolate), parameter_,
            embedder_fields_, callback_addr);
  Data::Callback callback = callback_;
  callback_ = nullptr;
  callback(data);
}

void GlobalHandles::PostGarbageCollectionProcessing(
    GarbageCollector collector, const v8::GCCallbackFlags gc_callback_flags) {
  // Process weak global handle callbacks. This must be done after the
  // GC is completely done, because the callbacks may invoke arbitrary
  // API functions.
  DCHECK_EQ(Heap::NOT_IN_GC, isolate_->heap()->gc_state());

  const bool synchronous_second_pass =
      FLAG_optimize_for_size || FLAG_predictable ||
      isolate_->heap()->IsTearingDown() ||
      (gc_callback_flags &
       (kGCCallbackFlagForced | kGCCallbackFlagCollectAllAvailableGarbage |
        kGCCallbackFlagSynchronousPhantomCallbackProcessing)) != 0;

  if (synchronous_second_pass) {
    InvokeSecondPassPhantomCallbacks();
    return;
  }

  if (second_pass_callbacks_.empty() || second_pass_callbacks_task_posted_)
    return;

  second_pass_callbacks_task_posted_ = true;
  V8::GetCurrentPlatform()
      ->GetForegroundTaskRunner(reinterpret_cast<v8::Isolate*>(isolate()))
      ->PostTask(MakeCancelableTask(isolate(), [this] {
        DCHECK(second_pass_callbacks_task_posted_);
        second_pass_callbacks_task_posted_ = false;
        InvokeSecondPassPhantomCallbacks();
      }));
}

void GlobalHandles::IterateStrongRoots(RootVisitor* v) {
  for (Node* node : *regular_nodes_) {
    if (node->IsStrongRetainer()) {
      v->VisitRootPointer(Root::kGlobalHandles, node->label(),
                          node->location());
    }
  }
}

void GlobalHandles::IterateWeakRoots(RootVisitor* v) {
  for (Node* node : *regular_nodes_) {
    if (node->IsWeak()) {
      v->VisitRootPointer(Root::kGlobalHandles, node->label(),
                          node->location());
    }
  }
  for (TracedNode* node : *traced_nodes_) {
    if (node->IsInUse()) {
      v->VisitRootPointer(Root::kGlobalHandles, nullptr, node->location());
    }
  }
}

DISABLE_CFI_PERF
void GlobalHandles::IterateAllRoots(RootVisitor* v) {
  for (Node* node : *regular_nodes_) {
    if (node->IsWeakOrStrongRetainer()) {
      v->VisitRootPointer(Root::kGlobalHandles, node->label(),
                          node->location());
    }
  }
  for (TracedNode* node : *traced_nodes_) {
    if (node->IsRetainer()) {
      v->VisitRootPointer(Root::kGlobalHandles, nullptr, node->location());
    }
  }
}

DISABLE_CFI_PERF
void GlobalHandles::IterateAllYoungRoots(RootVisitor* v) {
  for (Node* node : young_nodes_) {
    if (node->IsWeakOrStrongRetainer()) {
      v->VisitRootPointer(Root::kGlobalHandles, node->label(),
                          node->location());
    }
  }
  for (TracedNode* node : traced_young_nodes_) {
    if (node->IsRetainer()) {
      v->VisitRootPointer(Root::kGlobalHandles, nullptr, node->location());
    }
  }
}

DISABLE_CFI_PERF
void GlobalHandles::ApplyPersistentHandleVisitor(
    v8::PersistentHandleVisitor* visitor, GlobalHandles::Node* node) {
  v8::Value* value = ToApi<v8::Value>(node->handle());
  visitor->VisitPersistentHandle(
      reinterpret_cast<v8::Persistent<v8::Value>*>(&value),
      node->wrapper_class_id());
}

void GlobalHandles::IterateAllRootsForTesting(
    v8::PersistentHandleVisitor* visitor) {
  for (Node* node : *regular_nodes_) {
    if (node->IsWeakOrStrongRetainer()) {
      ApplyPersistentHandleVisitor(visitor, node);
    }
  }
}

GlobalHandles::NodeBounds GlobalHandles::GetTracedNodeBounds() const {
  return traced_nodes_->GetNodeBlockBounds();
}

START_ALLOW_USE_DEPRECATED()

DISABLE_CFI_PERF void GlobalHandles::IterateTracedNodes(
    v8::EmbedderHeapTracer::TracedGlobalHandleVisitor* visitor) {
  for (TracedNode* node : *traced_nodes_) {
    if (node->IsInUse()) {
      v8::Value* value = ToApi<v8::Value>(node->handle());
      visitor->VisitTracedReference(
          *reinterpret_cast<v8::TracedReference<v8::Value>*>(&value));
    }
  }
}

END_ALLOW_USE_DEPRECATED()

void GlobalHandles::RecordStats(HeapStats* stats) {
  *stats->global_handle_count = 0;
  *stats->weak_global_handle_count = 0;
  *stats->pending_global_handle_count = 0;
  *stats->near_death_global_handle_count = 0;
  *stats->free_global_handle_count = 0;
  for (Node* node : *regular_nodes_) {
    *stats->global_handle_count += 1;
    if (node->state() == Node::WEAK) {
      *stats->weak_global_handle_count += 1;
    } else if (node->state() == Node::NEAR_DEATH) {
      *stats->near_death_global_handle_count += 1;
    } else if (node->state() == Node::FREE) {
      *stats->free_global_handle_count += 1;
    }
  }
}

#ifdef DEBUG

void GlobalHandles::PrintStats() {
  int total = 0;
  int weak = 0;
  int near_death = 0;
  int destroyed = 0;

  for (Node* node : *regular_nodes_) {
    total++;
    if (node->state() == Node::WEAK) weak++;
    if (node->state() == Node::NEAR_DEATH) near_death++;
    if (node->state() == Node::FREE) destroyed++;
  }

  PrintF("Global Handle Statistics:\n");
  PrintF("  allocated memory = %zuB\n", total * sizeof(Node));
  PrintF("  # weak       = %d\n", weak);
  PrintF("  # near_death = %d\n", near_death);
  PrintF("  # free       = %d\n", destroyed);
  PrintF("  # total      = %d\n", total);
}

void GlobalHandles::Print() {
  PrintF("Global handles:\n");
  for (Node* node : *regular_nodes_) {
    PrintF("  handle %p to %p%s\n", node->location().ToVoidPtr(),
           reinterpret_cast<void*>(node->object().ptr()),
           node->IsWeak() ? " (weak)" : "");
  }
}

#endif

EternalHandles::~EternalHandles() {
  for (Address* block : blocks_) delete[] block;
}

void EternalHandles::IterateAllRoots(RootVisitor* visitor) {
  int limit = size_;
  for (Address* block : blocks_) {
    DCHECK_GT(limit, 0);
    visitor->VisitRootPointers(
        Root::kEternalHandles, nullptr, FullObjectSlot(block),
        FullObjectSlot(block + std::min({limit, kSize})));
    limit -= kSize;
  }
}

void EternalHandles::IterateYoungRoots(RootVisitor* visitor) {
  for (int index : young_node_indices_) {
    visitor->VisitRootPointer(Root::kEternalHandles, nullptr,
                              FullObjectSlot(GetLocation(index)));
  }
}

void EternalHandles::PostGarbageCollectionProcessing() {
  size_t last = 0;
  for (int index : young_node_indices_) {
    if (ObjectInYoungGeneration(Object(*GetLocation(index)))) {
      young_node_indices_[last++] = index;
    }
  }
  DCHECK_LE(last, young_node_indices_.size());
  young_node_indices_.resize(last);
}

void EternalHandles::Create(Isolate* isolate, Object object, int* index) {
  DCHECK_EQ(kInvalidIndex, *index);
  if (object == Object()) return;
  Object the_hole = ReadOnlyRoots(isolate).the_hole_value();
  DCHECK_NE(the_hole, object);
  int block = size_ >> kShift;
  int offset = size_ & kMask;
  // Need to resize.
  if (offset == 0) {
    Address* next_block = new Address[kSize];
    MemsetPointer(FullObjectSlot(next_block), the_hole, kSize);
    blocks_.push_back(next_block);
  }
  DCHECK_EQ(the_hole.ptr(), blocks_[block][offset]);
  blocks_[block][offset] = object.ptr();
  if (ObjectInYoungGeneration(object)) {
    young_node_indices_.push_back(size_);
  }
  *index = size_++;
}

}  // namespace internal
}  // namespace v8
