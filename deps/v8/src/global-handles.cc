// Copyright 2009 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/global-handles.h"

#include "src/api-inl.h"
#include "src/base/compiler-specific.h"
#include "src/cancelable-task.h"
#include "src/objects-inl.h"
#include "src/objects/slots.h"
#include "src/task-utils.h"
#include "src/v8.h"
#include "src/visitors.h"
#include "src/vm-state-inl.h"

namespace v8 {
namespace internal {

namespace {

constexpr size_t kBlockSize = 256;

}  // namespace

template <class _NodeType>
class GlobalHandles::NodeBlock final {
 public:
  using BlockType = NodeBlock<_NodeType>;
  using NodeType = _NodeType;

  V8_INLINE static NodeBlock* From(NodeType* node);

  NodeBlock(GlobalHandles* global_handles,
            GlobalHandles::NodeSpace<NodeType>* space,
            NodeBlock* next) V8_NOEXCEPT : next_(next),
                                           global_handles_(global_handles),
                                           space_(space) {}

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

 private:
  NodeType nodes_[kBlockSize];
  NodeBlock* const next_;
  GlobalHandles* const global_handles_;
  GlobalHandles::NodeSpace<NodeType>* const space_;
  NodeBlock* next_used_ = nullptr;
  NodeBlock* prev_used_ = nullptr;
  uint32_t used_nodes_ = 0;

  DISALLOW_COPY_AND_ASSIGN(NodeBlock);
};

template <class NodeType>
GlobalHandles::NodeBlock<NodeType>* GlobalHandles::NodeBlock<NodeType>::From(
    NodeType* node) {
  uintptr_t ptr =
      reinterpret_cast<uintptr_t>(node) - sizeof(NodeType) * node->index();
  BlockType* block = reinterpret_cast<BlockType*>(ptr);
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

  DISALLOW_COPY_AND_ASSIGN(NodeIterator);
};

template <class NodeType>
class GlobalHandles::NodeSpace final {
 public:
  using BlockType = NodeBlock<NodeType>;
  using iterator = NodeIterator<BlockType>;

  static NodeSpace* From(NodeType* node);
  static void Release(NodeType* node);

  explicit NodeSpace(GlobalHandles* global_handles) V8_NOEXCEPT
      : global_handles_(global_handles) {}
  ~NodeSpace();

  V8_INLINE NodeType* Acquire(Object object);

  iterator begin() { return iterator(first_used_block_); }
  iterator end() { return iterator(nullptr); }

 private:
  void PutNodesOnFreeList(BlockType* block);
  V8_INLINE void Free(NodeType* node);

  GlobalHandles* const global_handles_;
  BlockType* first_block_ = nullptr;
  BlockType* first_used_block_ = nullptr;
  NodeType* first_free_ = nullptr;
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
NodeType* GlobalHandles::NodeSpace<NodeType>::Acquire(Object object) {
  if (first_free_ == nullptr) {
    first_block_ = new BlockType(global_handles_, this, first_block_);
    PutNodesOnFreeList(first_block_);
  }
  DCHECK_NOT_NULL(first_free_);
  NodeType* node = first_free_;
  first_free_ = first_free_->next_free();
  node->Acquire(object);
  BlockType* block = BlockType::From(node);
  if (block->IncreaseUsage()) {
    block->ListAdd(&first_used_block_);
  }
  global_handles_->isolate()->counters()->global_handles()->Increment();
  global_handles_->handles_count_++;
  DCHECK(node->IsInUse());
  return node;
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
  global_handles_->handles_count_--;
}

class GlobalHandles::Node final {
 public:
  // State transition diagram:
  // FREE -> NORMAL <-> WEAK -> PENDING -> NEAR_DEATH -> { NORMAL, WEAK, FREE }
  enum State {
    FREE = 0,
    NORMAL,      // Normal global handle.
    WEAK,        // Flagged as weak but not yet finalized.
    PENDING,     // Has been recognized as only reachable by weak handles.
    NEAR_DEATH,  // Callback has informed the handle is near death.
    NUMBER_OF_NODE_STATES
  };

  // Maps handle location (slot) to the containing node.
  static Node* FromLocation(Address* location) {
    DCHECK_EQ(offsetof(Node, object_), 0);
    return reinterpret_cast<Node*>(location);
  }

  Node() {
    DCHECK_EQ(offsetof(Node, class_id_), Internals::kNodeClassIdOffset);
    DCHECK_EQ(offsetof(Node, flags_), Internals::kNodeFlagsOffset);
    STATIC_ASSERT(static_cast<int>(NodeState::kMask) ==
                  Internals::kNodeStateMask);
    STATIC_ASSERT(WEAK == Internals::kNodeStateIsWeakValue);
    STATIC_ASSERT(PENDING == Internals::kNodeStateIsPendingValue);
    STATIC_ASSERT(NEAR_DEATH == Internals::kNodeStateIsNearDeathValue);
    STATIC_ASSERT(static_cast<int>(IsIndependent::kShift) ==
                  Internals::kNodeIsIndependentShift);
    STATIC_ASSERT(static_cast<int>(IsActive::kShift) ==
                  Internals::kNodeIsActiveShift);
    set_in_new_space_list(false);
  }

#ifdef ENABLE_HANDLE_ZAPPING
  ~Node() {
    ClearFields();
    data_.next_free = nullptr;
    index_ = 0;
  }
#endif

  void Free(Node* free_list) {
    ClearFields();
    set_state(FREE);
    data_.next_free = free_list;
  }

  void Acquire(Object object) {
    DCHECK(!IsInUse());
    CheckFieldsAreCleared();
    object_ = object.ptr();
    set_state(NORMAL);
    data_.parameter = nullptr;
    DCHECK(IsInUse());
  }

  void Release(Node* free_list) {
    DCHECK(IsInUse());
    Free(free_list);
    DCHECK(!IsInUse());
  }

  void Zap() {
    DCHECK(IsInUse());
    // Zap the values for eager trapping.
    object_ = kGlobalHandleZapValue;
  }

  // Object slot accessors.
  Object object() const { return Object(object_); }
  FullObjectSlot location() { return FullObjectSlot(&object_); }
  const char* label() { return state() == NORMAL ? data_.label : nullptr; }
  Handle<Object> handle() { return Handle<Object>(&object_); }

  // Wrapper class ID accessors.
  bool has_wrapper_class_id() const {
    return class_id_ != v8::HeapProfiler::kPersistentHandleNoClassId;
  }
  uint16_t wrapper_class_id() const { return class_id_; }

  // State and flag accessors.

  State state() const {
    return NodeState::decode(flags_);
  }
  void set_state(State state) {
    flags_ = NodeState::update(flags_, state);
  }

  bool is_independent() { return IsIndependent::decode(flags_); }
  void set_independent(bool v) { flags_ = IsIndependent::update(flags_, v); }

  bool is_active() {
    return IsActive::decode(flags_);
  }
  void set_active(bool v) {
    flags_ = IsActive::update(flags_, v);
  }

  bool is_in_new_space_list() {
    return IsInNewSpaceList::decode(flags_);
  }
  void set_in_new_space_list(bool v) {
    flags_ = IsInNewSpaceList::update(flags_, v);
  }

  WeaknessType weakness_type() const {
    return NodeWeaknessType::decode(flags_);
  }
  void set_weakness_type(WeaknessType weakness_type) {
    flags_ = NodeWeaknessType::update(flags_, weakness_type);
  }

  bool IsNearDeath() const {
    // Check for PENDING to ensure correct answer when processing callbacks.
    return state() == PENDING || state() == NEAR_DEATH;
  }

  bool IsWeak() const { return state() == WEAK; }

  bool IsInUse() const { return state() != FREE; }

  bool IsPhantomCallback() const {
    return weakness_type() == PHANTOM_WEAK ||
           weakness_type() == PHANTOM_WEAK_2_EMBEDDER_FIELDS;
  }

  bool IsPhantomResetHandle() const {
    return weakness_type() == PHANTOM_WEAK_RESET_HANDLE;
  }

  bool IsPendingPhantomCallback() const {
    return state() == PENDING && IsPhantomCallback();
  }

  bool IsPendingPhantomResetHandle() const {
    return state() == PENDING && IsPhantomResetHandle();
  }

  bool IsPendingFinalizer() const {
    return state() == PENDING && weakness_type() == FINALIZER_WEAK;
  }

  bool IsPending() const { return state() == PENDING; }

  bool IsRetainer() const {
    return state() != FREE &&
           !(state() == NEAR_DEATH && weakness_type() != FINALIZER_WEAK);
  }

  bool IsStrongRetainer() const { return state() == NORMAL; }

  bool IsWeakRetainer() const {
    return state() == WEAK || state() == PENDING ||
           (state() == NEAR_DEATH && weakness_type() == FINALIZER_WEAK);
  }

  void MarkPending() {
    DCHECK(state() == WEAK);
    set_state(PENDING);
  }

  // Callback parameter accessors.
  void set_parameter(void* parameter) {
    DCHECK(IsInUse());
    data_.parameter = parameter;
  }
  void* parameter() const {
    DCHECK(IsInUse());
    return data_.parameter;
  }

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
        set_weakness_type(PHANTOM_WEAK);
        break;
      case v8::WeakCallbackType::kInternalFields:
        set_weakness_type(PHANTOM_WEAK_2_EMBEDDER_FIELDS);
        break;
      case v8::WeakCallbackType::kFinalizer:
        set_weakness_type(FINALIZER_WEAK);
        break;
    }
    set_parameter(parameter);
    weak_callback_ = phantom_callback;
  }

  void MakeWeak(Address** location_addr) {
    DCHECK(IsInUse());
    CHECK_NE(object_, kGlobalHandleZapValue);
    set_state(WEAK);
    set_weakness_type(PHANTOM_WEAK_RESET_HANDLE);
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
    data_.label = label;
  }

  void CollectPhantomCallbackData(
      std::vector<PendingPhantomCallback>* pending_phantom_callbacks) {
    DCHECK(weakness_type() == PHANTOM_WEAK ||
           weakness_type() == PHANTOM_WEAK_2_EMBEDDER_FIELDS);
    DCHECK(state() == PENDING);
    DCHECK_NOT_NULL(weak_callback_);

    void* embedder_fields[v8::kEmbedderFieldsInWeakCallback] = {nullptr,
                                                                nullptr};
    if (weakness_type() != PHANTOM_WEAK && object()->IsJSObject()) {
      JSObject jsobject = JSObject::cast(object());
      int field_count = jsobject->GetEmbedderFieldCount();
      for (int i = 0; i < v8::kEmbedderFieldsInWeakCallback; ++i) {
        if (field_count == i) break;
        void* pointer;
        if (EmbedderDataSlot(jsobject, i).ToAlignedPointer(&pointer)) {
          embedder_fields[i] = pointer;
        }
      }
    }

    // Zap with something dangerous.
    location().store(Object(0x6057CA11));

    pending_phantom_callbacks->push_back(PendingPhantomCallback(
        this, weak_callback_, parameter(), embedder_fields));
    DCHECK(IsInUse());
    set_state(NEAR_DEATH);
  }

  void ResetPhantomHandle() {
    DCHECK(weakness_type() == PHANTOM_WEAK_RESET_HANDLE);
    DCHECK(state() == PENDING);
    DCHECK_NULL(weak_callback_);
    Address** handle = reinterpret_cast<Address**>(parameter());
    *handle = nullptr;
    NodeSpace<Node>::Release(this);
  }

  void PostGarbageCollectionProcessing(Isolate* isolate) {
    // This method invokes a finalizer. Updating the method name would require
    // adjusting CFI blacklist as weak_callback_ is invoked on the wrong type.
    CHECK(IsPendingFinalizer());
    CHECK(!is_active());
    set_state(NEAR_DEATH);
    // Check that we are not passing a finalized external string to
    // the callback.
    DCHECK(!object()->IsExternalOneByteString() ||
           ExternalOneByteString::cast(object())->resource() != nullptr);
    DCHECK(!object()->IsExternalTwoByteString() ||
           ExternalTwoByteString::cast(object())->resource() != nullptr);
    // Leaving V8.
    VMState<EXTERNAL> vmstate(isolate);
    HandleScope handle_scope(isolate);
    void* embedder_fields[v8::kEmbedderFieldsInWeakCallback] = {nullptr,
                                                                nullptr};
    v8::WeakCallbackInfo<void> data(reinterpret_cast<v8::Isolate*>(isolate),
                                    parameter(), embedder_fields, nullptr);
    weak_callback_(data);
    // For finalizers the handle must have either been reset or made strong.
    // Both cases reset the state.
    CHECK_NE(NEAR_DEATH, state());
  }

  inline GlobalHandles* GetGlobalHandles();

  uint8_t index() const { return index_; }
  void set_index(uint8_t value) { index_ = value; }

 private:
  // Fields that are not used for managing node memory.
  void ClearFields() {
    // Zap the values for eager trapping.
    object_ = kGlobalHandleZapValue;
    class_id_ = v8::HeapProfiler::kPersistentHandleNoClassId;
    set_independent(false);
    set_active(false);
    weak_callback_ = nullptr;
  }

  void CheckFieldsAreCleared() {
    DCHECK_EQ(kGlobalHandleZapValue, object_);
    DCHECK_EQ(v8::HeapProfiler::kPersistentHandleNoClassId, class_id_);
    DCHECK(!is_independent());
    DCHECK(!is_active());
    DCHECK_EQ(nullptr, weak_callback_);
  }

  // Storage for object pointer.
  //
  // Placed first to avoid offset computation. The stored data is equivalent to
  // an Object. It is stored as a plain Address for convenience (smallest number
  // of casts), and because it is a private implementation detail: the public
  // interface provides type safety.
  Address object_;

  // Next word stores class_id, index, state, and independent.
  // Note: the most aligned fields should go first.

  // Wrapper class ID.
  uint16_t class_id_;

  // Index in the containing handle block.
  uint8_t index_;

  // This stores three flags (independent, partially_dependent and
  // in_new_space_list) and a State.
  class NodeState : public BitField<State, 0, 3> {};
  class IsIndependent : public BitField<bool, 3, 1> {};
  // The following two fields are mutually exclusive
  class IsActive : public BitField<bool, 4, 1> {};
  class IsInNewSpaceList : public BitField<bool, 5, 1> {};
  class NodeWeaknessType : public BitField<WeaknessType, 6, 2> {};

  uint8_t flags_;

  // Handle specific callback - might be a weak reference in disguise.
  WeakCallbackInfo<void>::Callback weak_callback_;

  // The meaning of this field depends on node state:
  // state == FREE: it stores the next free node pointer.
  // state == NORMAL: it stores the strong retainer label.
  // otherwise: it stores the parameter for the weak callback.
  union {
    Node* next_free;
    const char* label;
    void* parameter;
  } data_;

  DISALLOW_COPY_AND_ASSIGN(Node);
};

GlobalHandles* GlobalHandles::Node::GetGlobalHandles() {
  return NodeBlock<Node>::From(this)->global_handles();
}

GlobalHandles::GlobalHandles(Isolate* isolate)
    : isolate_(isolate),
      regular_nodes_(new NodeSpace<GlobalHandles::Node>(this)) {}

GlobalHandles::~GlobalHandles() { regular_nodes_.reset(nullptr); }

Handle<Object> GlobalHandles::Create(Object value) {
  GlobalHandles::Node* result = regular_nodes_->Acquire(value);
  if (Heap::InNewSpace(value) && !result->is_in_new_space_list()) {
    new_space_nodes_.push_back(result);
    result->set_in_new_space_list(true);
  }
  return result->handle();
}

Handle<Object> GlobalHandles::Create(Address value) {
  return Create(Object(value));
}

Handle<Object> GlobalHandles::CopyGlobal(Address* location) {
  DCHECK_NOT_NULL(location);
  GlobalHandles* global_handles =
      Node::FromLocation(location)->GetGlobalHandles();
#ifdef VERIFY_HEAP
  if (i::FLAG_verify_heap) {
    Object(*location)->ObjectVerify(global_handles->isolate());
  }
#endif  // VERIFY_HEAP
  return global_handles->Create(*location);
}

void GlobalHandles::Destroy(Address* location) {
  if (location != nullptr) {
    NodeSpace<Node>::Release(Node::FromLocation(location));
  }
}

typedef v8::WeakCallbackInfo<void>::Callback GenericCallback;


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

bool GlobalHandles::IsNearDeath(Address* location) {
  return Node::FromLocation(location)->IsNearDeath();
}

bool GlobalHandles::IsWeak(Address* location) {
  return Node::FromLocation(location)->IsWeak();
}

DISABLE_CFI_PERF
void GlobalHandles::IterateWeakRootsForFinalizers(RootVisitor* v) {
  for (Node* node : *regular_nodes_) {
    if (node->IsWeakRetainer() && node->state() == Node::PENDING) {
      DCHECK(!node->IsPhantomCallback());
      DCHECK(!node->IsPhantomResetHandle());
      // Finalizers need to survive.
      v->VisitRootPointer(Root::kGlobalHandles, node->label(),
                          node->location());
    }
  }
}

DISABLE_CFI_PERF
void GlobalHandles::IterateWeakRootsForPhantomHandles(
    WeakSlotCallbackWithHeap should_reset_handle) {
  for (Node* node : *regular_nodes_) {
    if (node->IsWeakRetainer() &&
        should_reset_handle(isolate()->heap(), node->location())) {
      if (node->IsPhantomResetHandle()) {
        node->MarkPending();
        node->ResetPhantomHandle();
        ++number_of_phantom_handle_resets_;
      } else if (node->IsPhantomCallback()) {
        node->MarkPending();
        node->CollectPhantomCallbackData(&pending_phantom_callbacks_);
      }
    }
  }
}

void GlobalHandles::IdentifyWeakHandles(
    WeakSlotCallbackWithHeap should_reset_handle) {
  for (Node* node : *regular_nodes_) {
    if (node->IsWeak() &&
        should_reset_handle(isolate()->heap(), node->location())) {
      if (!node->IsPhantomCallback() && !node->IsPhantomResetHandle()) {
        node->MarkPending();
      }
    }
  }
}

void GlobalHandles::IterateNewSpaceStrongAndDependentRoots(RootVisitor* v) {
  for (Node* node : new_space_nodes_) {
    if (node->IsStrongRetainer() ||
        (node->IsWeakRetainer() && !node->is_independent() &&
         node->is_active())) {
      v->VisitRootPointer(Root::kGlobalHandles, node->label(),
                          node->location());
    }
  }
}

void GlobalHandles::IdentifyWeakUnmodifiedObjects(
    WeakSlotCallback is_unmodified) {
  for (Node* node : new_space_nodes_) {
    if (node->IsWeak() && !is_unmodified(node->location())) {
      node->set_active(true);
    }
  }
}

void GlobalHandles::MarkNewSpaceWeakUnmodifiedObjectsPending(
    WeakSlotCallbackWithHeap is_dead) {
  for (Node* node : new_space_nodes_) {
    DCHECK(node->is_in_new_space_list());
    if ((node->is_independent() || !node->is_active()) && node->IsWeak() &&
        is_dead(isolate_->heap(), node->location())) {
      if (!node->IsPhantomCallback() && !node->IsPhantomResetHandle()) {
        node->MarkPending();
      }
    }
  }
}

void GlobalHandles::IterateNewSpaceWeakUnmodifiedRootsForFinalizers(
    RootVisitor* v) {
  for (Node* node : new_space_nodes_) {
    DCHECK(node->is_in_new_space_list());
    if ((node->is_independent() || !node->is_active()) &&
        node->IsWeakRetainer() && (node->state() == Node::PENDING)) {
      DCHECK(!node->IsPhantomCallback());
      DCHECK(!node->IsPhantomResetHandle());
      // Finalizers need to survive.
      v->VisitRootPointer(Root::kGlobalHandles, node->label(),
                          node->location());
    }
  }
}

void GlobalHandles::IterateNewSpaceWeakUnmodifiedRootsForPhantomHandles(
    RootVisitor* v, WeakSlotCallbackWithHeap should_reset_handle) {
  for (Node* node : new_space_nodes_) {
    DCHECK(node->is_in_new_space_list());
    if ((node->is_independent() || !node->is_active()) &&
        node->IsWeakRetainer() && (node->state() != Node::PENDING)) {
      if (should_reset_handle(isolate_->heap(), node->location())) {
        DCHECK(node->IsPhantomResetHandle() || node->IsPhantomCallback());
        if (node->IsPhantomResetHandle()) {
          node->MarkPending();
          node->ResetPhantomHandle();
          ++number_of_phantom_handle_resets_;

        } else if (node->IsPhantomCallback()) {
          node->MarkPending();
          node->CollectPhantomCallbackData(&pending_phantom_callbacks_);
        } else {
          UNREACHABLE();
        }
      } else {
        // Node survived and needs to be visited.
        v->VisitRootPointer(Root::kGlobalHandles, node->label(),
                            node->location());
      }
    }
  }
}

void GlobalHandles::InvokeSecondPassPhantomCallbacksFromTask() {
  DCHECK(second_pass_callbacks_task_posted_);
  second_pass_callbacks_task_posted_ = false;
  TRACE_EVENT0("v8", "V8.GCPhantomHandleProcessingCallback");
  isolate()->heap()->CallGCPrologueCallbacks(
      GCType::kGCTypeProcessWeakCallbacks, kNoGCCallbackFlags);
  InvokeSecondPassPhantomCallbacks();
  isolate()->heap()->CallGCEpilogueCallbacks(
      GCType::kGCTypeProcessWeakCallbacks, kNoGCCallbackFlags);
}

void GlobalHandles::InvokeSecondPassPhantomCallbacks() {
  while (!second_pass_callbacks_.empty()) {
    auto callback = second_pass_callbacks_.back();
    second_pass_callbacks_.pop_back();
    DCHECK_NULL(callback.node());
    // Fire second pass callback
    callback.Invoke(isolate());
  }
}

size_t GlobalHandles::PostScavengeProcessing(unsigned post_processing_count) {
  size_t freed_nodes = 0;
  for (Node* node : new_space_nodes_) {
    // Filter free nodes.
    if (!node->IsRetainer()) continue;

    // Reset active state for all affected nodes.
    node->set_active(false);

    if (node->IsPending()) {
      DCHECK(node->has_callback());
      DCHECK(node->IsPendingFinalizer());
      node->PostGarbageCollectionProcessing(isolate_);
    }
    if (InRecursiveGC(post_processing_count)) return freed_nodes;

    if (!node->IsRetainer()) freed_nodes++;
  }
  return freed_nodes;
}

size_t GlobalHandles::PostMarkSweepProcessing(unsigned post_processing_count) {
  size_t freed_nodes = 0;
  for (Node* node : *regular_nodes_) {
    // Filter free nodes.
    if (!node->IsRetainer()) continue;

    // Reset active state for all affected nodes.
    node->set_active(false);

    if (node->IsPending()) {
      DCHECK(node->has_callback());
      DCHECK(node->IsPendingFinalizer());
      node->PostGarbageCollectionProcessing(isolate_);
    }
    if (InRecursiveGC(post_processing_count)) return freed_nodes;

    if (!node->IsRetainer()) freed_nodes++;
  }
  return freed_nodes;
}

void GlobalHandles::UpdateListOfNewSpaceNodes() {
  size_t last = 0;
  for (Node* node : new_space_nodes_) {
    DCHECK(node->is_in_new_space_list());
    if (node->IsRetainer()) {
      if (Heap::InNewSpace(node->object())) {
        new_space_nodes_[last++] = node;
        isolate_->heap()->IncrementNodesCopiedInNewSpace();
      } else {
        node->set_in_new_space_list(false);
        isolate_->heap()->IncrementNodesPromoted();
      }
    } else {
      node->set_in_new_space_list(false);
      isolate_->heap()->IncrementNodesDiedInNewSpace();
    }
  }
  DCHECK_LE(last, new_space_nodes_.size());
  new_space_nodes_.resize(last);
  new_space_nodes_.shrink_to_fit();
}

size_t GlobalHandles::InvokeFirstPassWeakCallbacks() {
  size_t freed_nodes = 0;
  std::vector<PendingPhantomCallback> pending_phantom_callbacks;
  pending_phantom_callbacks.swap(pending_phantom_callbacks_);
  {
    // The initial pass callbacks must simply clear the nodes.
    for (auto callback : pending_phantom_callbacks) {
      // Skip callbacks that have already been processed once.
      if (callback.node() == nullptr) continue;
      callback.Invoke(isolate());
      if (callback.callback()) second_pass_callbacks_.push_back(callback);
      freed_nodes++;
    }
  }
  return freed_nodes;
}

void GlobalHandles::InvokeOrScheduleSecondPassPhantomCallbacks(
    bool synchronous_second_pass) {
  if (!second_pass_callbacks_.empty()) {
    if (FLAG_optimize_for_size || FLAG_predictable || synchronous_second_pass) {
      isolate()->heap()->CallGCPrologueCallbacks(
          GCType::kGCTypeProcessWeakCallbacks, kNoGCCallbackFlags);
      InvokeSecondPassPhantomCallbacks();
      isolate()->heap()->CallGCEpilogueCallbacks(
          GCType::kGCTypeProcessWeakCallbacks, kNoGCCallbackFlags);
    } else if (!second_pass_callbacks_task_posted_) {
      second_pass_callbacks_task_posted_ = true;
      auto taskrunner = V8::GetCurrentPlatform()->GetForegroundTaskRunner(
          reinterpret_cast<v8::Isolate*>(isolate()));
      taskrunner->PostTask(MakeCancelableTask(
          isolate(), [this] { InvokeSecondPassPhantomCallbacksFromTask(); }));
    }
  }
}

void GlobalHandles::PendingPhantomCallback::Invoke(Isolate* isolate) {
  Data::Callback* callback_addr = nullptr;
  if (node_ != nullptr) {
    // Initialize for first pass callback.
    DCHECK(node_->state() == Node::NEAR_DEATH);
    callback_addr = &callback_;
  }
  Data data(reinterpret_cast<v8::Isolate*>(isolate), parameter_,
            embedder_fields_, callback_addr);
  Data::Callback callback = callback_;
  callback_ = nullptr;
  callback(data);
  if (node_ != nullptr) {
    // Transition to second pass. It is required that the first pass callback
    // resets the handle using |v8::PersistentBase::Reset|. Also see comments on
    // |v8::WeakCallbackInfo|.
    CHECK_WITH_MSG(Node::FREE == node_->state(),
                   "Handle not reset in first callback. See comments on "
                   "|v8::WeakCallbackInfo|.");
    node_ = nullptr;
  }
}

bool GlobalHandles::InRecursiveGC(unsigned gc_processing_counter) {
  return gc_processing_counter != post_gc_processing_count_;
}

size_t GlobalHandles::PostGarbageCollectionProcessing(
    GarbageCollector collector, const v8::GCCallbackFlags gc_callback_flags) {
  // Process weak global handle callbacks. This must be done after the
  // GC is completely done, because the callbacks may invoke arbitrary
  // API functions.
  DCHECK_EQ(Heap::NOT_IN_GC, isolate_->heap()->gc_state());
  const unsigned post_processing_count = ++post_gc_processing_count_;
  size_t freed_nodes = 0;
  bool synchronous_second_pass =
      isolate_->heap()->IsTearingDown() ||
      (gc_callback_flags &
       (kGCCallbackFlagForced | kGCCallbackFlagCollectAllAvailableGarbage |
        kGCCallbackFlagSynchronousPhantomCallbackProcessing)) != 0;
  InvokeOrScheduleSecondPassPhantomCallbacks(synchronous_second_pass);
  if (InRecursiveGC(post_processing_count)) return freed_nodes;

  freed_nodes += Heap::IsYoungGenerationCollector(collector)
                     ? PostScavengeProcessing(post_processing_count)
                     : PostMarkSweepProcessing(post_processing_count);
  if (InRecursiveGC(post_processing_count)) return freed_nodes;

  UpdateListOfNewSpaceNodes();
  return freed_nodes;
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
}

DISABLE_CFI_PERF
void GlobalHandles::IterateAllRoots(RootVisitor* v) {
  for (Node* node : *regular_nodes_) {
    if (node->IsRetainer()) {
      v->VisitRootPointer(Root::kGlobalHandles, node->label(),
                          node->location());
    }
  }
}

DISABLE_CFI_PERF
void GlobalHandles::IterateAllNewSpaceRoots(RootVisitor* v) {
  for (Node* node : new_space_nodes_) {
    if (node->IsRetainer()) {
      v->VisitRootPointer(Root::kGlobalHandles, node->label(),
                          node->location());
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

DISABLE_CFI_PERF
void GlobalHandles::IterateAllRootsWithClassIds(
    v8::PersistentHandleVisitor* visitor) {
  for (Node* node : *regular_nodes_) {
    if (node->IsRetainer() && node->has_wrapper_class_id()) {
      ApplyPersistentHandleVisitor(visitor, node);
    }
  }
}


DISABLE_CFI_PERF
void GlobalHandles::IterateAllRootsInNewSpaceWithClassIds(
    v8::PersistentHandleVisitor* visitor) {
  for (Node* node : new_space_nodes_) {
    if (node->IsRetainer() && node->has_wrapper_class_id()) {
      ApplyPersistentHandleVisitor(visitor, node);
    }
  }
}


DISABLE_CFI_PERF
void GlobalHandles::IterateWeakRootsInNewSpaceWithClassIds(
    v8::PersistentHandleVisitor* visitor) {
  for (Node* node : new_space_nodes_) {
    if (node->has_wrapper_class_id() && node->IsWeak()) {
      ApplyPersistentHandleVisitor(visitor, node);
    }
  }
}

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
    } else if (node->state() == Node::PENDING) {
      *stats->pending_global_handle_count += 1;
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
  int pending = 0;
  int near_death = 0;
  int destroyed = 0;

  for (Node* node : *regular_nodes_) {
    total++;
    if (node->state() == Node::WEAK) weak++;
    if (node->state() == Node::PENDING) pending++;
    if (node->state() == Node::NEAR_DEATH) near_death++;
    if (node->state() == Node::FREE) destroyed++;
  }

  PrintF("Global Handle Statistics:\n");
  PrintF("  allocated memory = %" PRIuS "B\n", total * sizeof(Node));
  PrintF("  # weak       = %d\n", weak);
  PrintF("  # pending    = %d\n", pending);
  PrintF("  # near_death = %d\n", near_death);
  PrintF("  # free       = %d\n", destroyed);
  PrintF("  # total      = %d\n", total);
}


void GlobalHandles::Print() {
  PrintF("Global handles:\n");
  for (Node* node : *regular_nodes_) {
    PrintF("  handle %p to %p%s\n", node->location().ToVoidPtr(),
           reinterpret_cast<void*>(node->object()->ptr()),
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
    visitor->VisitRootPointers(Root::kEternalHandles, nullptr,
                               FullObjectSlot(block),
                               FullObjectSlot(block + Min(limit, kSize)));
    limit -= kSize;
  }
}

void EternalHandles::IterateNewSpaceRoots(RootVisitor* visitor) {
  for (int index : new_space_indices_) {
    visitor->VisitRootPointer(Root::kEternalHandles, nullptr,
                              FullObjectSlot(GetLocation(index)));
  }
}

void EternalHandles::PostGarbageCollectionProcessing() {
  size_t last = 0;
  for (int index : new_space_indices_) {
    if (Heap::InNewSpace(Object(*GetLocation(index)))) {
      new_space_indices_[last++] = index;
    }
  }
  DCHECK_LE(last, new_space_indices_.size());
  new_space_indices_.resize(last);
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
  DCHECK_EQ(the_hole->ptr(), blocks_[block][offset]);
  blocks_[block][offset] = object->ptr();
  if (Heap::InNewSpace(object)) {
    new_space_indices_.push_back(size_);
  }
  *index = size_++;
}

}  // namespace internal
}  // namespace v8
