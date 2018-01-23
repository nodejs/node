// Copyright 2009 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/global-handles.h"

#include "src/api.h"
#include "src/cancelable-task.h"
#include "src/objects-inl.h"
#include "src/v8.h"
#include "src/visitors.h"
#include "src/vm-state-inl.h"

namespace v8 {
namespace internal {

class GlobalHandles::Node {
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
  static Node* FromLocation(Object** location) {
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
    STATIC_ASSERT(static_cast<int>(IsActive::kShift) ==
                  Internals::kNodeIsActiveShift);
  }

#ifdef ENABLE_HANDLE_ZAPPING
  ~Node() {
    // TODO(1428): if it's a weak handle we should have invoked its callback.
    // Zap the values for eager trapping.
    object_ = reinterpret_cast<Object*>(kGlobalHandleZapValue);
    class_id_ = v8::HeapProfiler::kPersistentHandleNoClassId;
    index_ = 0;
    set_active(false);
    set_in_new_space_list(false);
    parameter_or_next_free_.next_free = nullptr;
    weak_callback_ = nullptr;
  }
#endif

  void Initialize(int index, Node** first_free) {
    object_ = reinterpret_cast<Object*>(kGlobalHandleZapValue);
    index_ = static_cast<uint8_t>(index);
    DCHECK(static_cast<int>(index_) == index);
    set_state(FREE);
    set_in_new_space_list(false);
    parameter_or_next_free_.next_free = *first_free;
    *first_free = this;
  }

  void Acquire(Object* object) {
    DCHECK(state() == FREE);
    object_ = object;
    class_id_ = v8::HeapProfiler::kPersistentHandleNoClassId;
    set_active(false);
    set_state(NORMAL);
    parameter_or_next_free_.parameter = nullptr;
    weak_callback_ = nullptr;
    IncreaseBlockUses();
  }

  void Zap() {
    DCHECK(IsInUse());
    // Zap the values for eager trapping.
    object_ = reinterpret_cast<Object*>(kGlobalHandleZapValue);
  }

  void Release() {
    DCHECK(IsInUse());
    set_state(FREE);
    // Zap the values for eager trapping.
    object_ = reinterpret_cast<Object*>(kGlobalHandleZapValue);
    class_id_ = v8::HeapProfiler::kPersistentHandleNoClassId;
    set_active(false);
    weak_callback_ = nullptr;
    DecreaseBlockUses();
  }

  // Object slot accessors.
  Object* object() const { return object_; }
  Object** location() { return &object_; }
  Handle<Object> handle() { return Handle<Object>(location()); }

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
    parameter_or_next_free_.parameter = parameter;
  }
  void* parameter() const {
    DCHECK(IsInUse());
    return parameter_or_next_free_.parameter;
  }

  // Accessors for next free node in the free list.
  Node* next_free() {
    DCHECK(state() == FREE);
    return parameter_or_next_free_.next_free;
  }
  void set_next_free(Node* value) {
    DCHECK(state() == FREE);
    parameter_or_next_free_.next_free = value;
  }

  void MakeWeak(void* parameter,
                WeakCallbackInfo<void>::Callback phantom_callback,
                v8::WeakCallbackType type) {
    DCHECK_NOT_NULL(phantom_callback);
    DCHECK(IsInUse());
    CHECK_NE(object_, reinterpret_cast<Object*>(kGlobalHandleZapValue));
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

  void MakeWeak(Object*** location_addr) {
    DCHECK(IsInUse());
    CHECK_NE(object_, reinterpret_cast<Object*>(kGlobalHandleZapValue));
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

  void CollectPhantomCallbackData(
      Isolate* isolate,
      std::vector<PendingPhantomCallback>* pending_phantom_callbacks) {
    DCHECK(weakness_type() == PHANTOM_WEAK ||
           weakness_type() == PHANTOM_WEAK_2_EMBEDDER_FIELDS);
    DCHECK(state() == PENDING);
    DCHECK_NOT_NULL(weak_callback_);

    void* embedder_fields[v8::kEmbedderFieldsInWeakCallback] = {nullptr,
                                                                nullptr};
    if (weakness_type() != PHANTOM_WEAK && object()->IsJSObject()) {
      auto jsobject = JSObject::cast(object());
      int field_count = jsobject->GetEmbedderFieldCount();
      for (int i = 0; i < v8::kEmbedderFieldsInWeakCallback; ++i) {
        if (field_count == i) break;
        auto field = jsobject->GetEmbedderField(i);
        if (field->IsSmi()) embedder_fields[i] = field;
      }
    }

    // Zap with something dangerous.
    *location() = reinterpret_cast<Object*>(0x6057CA11);

    typedef v8::WeakCallbackInfo<void> Data;
    auto callback = reinterpret_cast<Data::Callback>(weak_callback_);
    pending_phantom_callbacks->push_back(
        PendingPhantomCallback(this, callback, parameter(), embedder_fields));
    DCHECK(IsInUse());
    set_state(NEAR_DEATH);
  }

  void ResetPhantomHandle() {
    DCHECK(weakness_type() == PHANTOM_WEAK_RESET_HANDLE);
    DCHECK(state() == PENDING);
    DCHECK_NULL(weak_callback_);
    Object*** handle = reinterpret_cast<Object***>(parameter());
    *handle = nullptr;
    Release();
  }

  bool PostGarbageCollectionProcessing(Isolate* isolate) {
    // Handles only weak handles (not phantom) that are dying.
    if (state() != Node::PENDING) return false;
    if (weak_callback_ == nullptr) {
      Release();
      return false;
    }
    set_state(NEAR_DEATH);

    // Check that we are not passing a finalized external string to
    // the callback.
    DCHECK(!object_->IsExternalOneByteString() ||
           ExternalOneByteString::cast(object_)->resource() != nullptr);
    DCHECK(!object_->IsExternalTwoByteString() ||
           ExternalTwoByteString::cast(object_)->resource() != nullptr);
    if (weakness_type() != FINALIZER_WEAK) {
      return false;
    }

    // Leaving V8.
    VMState<EXTERNAL> vmstate(isolate);
    HandleScope handle_scope(isolate);
    void* embedder_fields[v8::kEmbedderFieldsInWeakCallback] = {nullptr,
                                                                nullptr};
    v8::WeakCallbackInfo<void> data(reinterpret_cast<v8::Isolate*>(isolate),
                                    parameter(), embedder_fields, nullptr);
    weak_callback_(data);

    // Absence of explicit cleanup or revival of weak handle
    // in most of the cases would lead to memory leak.
    CHECK(state() != NEAR_DEATH);
    return true;
  }

  inline GlobalHandles* GetGlobalHandles();

 private:
  inline NodeBlock* FindBlock();
  inline void IncreaseBlockUses();
  inline void DecreaseBlockUses();

  // Storage for object pointer.
  // Placed first to avoid offset computation.
  Object* object_;

  // Next word stores class_id, index, and state.
  // Note: the most aligned fields should go first.

  // Wrapper class ID.
  uint16_t class_id_;

  // Index in the containing handle block.
  uint8_t index_;

  class NodeState : public BitField<State, 0, 3> {};
  // The following two fields are mutually exclusive
  class IsActive : public BitField<bool, 4, 1> {};
  class IsInNewSpaceList : public BitField<bool, 5, 1> {};
  class NodeWeaknessType : public BitField<WeaknessType, 6, 2> {};

  uint8_t flags_;

  // Handle specific callback - might be a weak reference in disguise.
  WeakCallbackInfo<void>::Callback weak_callback_;

  // Provided data for callback.  In FREE state, this is used for
  // the free list link.
  union {
    void* parameter;
    Node* next_free;
  } parameter_or_next_free_;

  DISALLOW_COPY_AND_ASSIGN(Node);
};


class GlobalHandles::NodeBlock {
 public:
  static const int kSize = 256;

  explicit NodeBlock(GlobalHandles* global_handles, NodeBlock* next)
      : next_(next),
        used_nodes_(0),
        next_used_(nullptr),
        prev_used_(nullptr),
        global_handles_(global_handles) {}

  void PutNodesOnFreeList(Node** first_free) {
    for (int i = kSize - 1; i >= 0; --i) {
      nodes_[i].Initialize(i, first_free);
    }
  }

  Node* node_at(int index) {
    DCHECK(0 <= index && index < kSize);
    return &nodes_[index];
  }

  void IncreaseUses() {
    DCHECK_LT(used_nodes_, kSize);
    if (used_nodes_++ == 0) {
      NodeBlock* old_first = global_handles_->first_used_block_;
      global_handles_->first_used_block_ = this;
      next_used_ = old_first;
      prev_used_ = nullptr;
      if (old_first == nullptr) return;
      old_first->prev_used_ = this;
    }
  }

  void DecreaseUses() {
    DCHECK_GT(used_nodes_, 0);
    if (--used_nodes_ == 0) {
      if (next_used_ != nullptr) next_used_->prev_used_ = prev_used_;
      if (prev_used_ != nullptr) prev_used_->next_used_ = next_used_;
      if (this == global_handles_->first_used_block_) {
        global_handles_->first_used_block_ = next_used_;
      }
    }
  }

  GlobalHandles* global_handles() { return global_handles_; }

  // Next block in the list of all blocks.
  NodeBlock* next() const { return next_; }

  // Next/previous block in the list of blocks with used nodes.
  NodeBlock* next_used() const { return next_used_; }
  NodeBlock* prev_used() const { return prev_used_; }

 private:
  Node nodes_[kSize];
  NodeBlock* const next_;
  int used_nodes_;
  NodeBlock* next_used_;
  NodeBlock* prev_used_;
  GlobalHandles* global_handles_;
};


GlobalHandles* GlobalHandles::Node::GetGlobalHandles() {
  return FindBlock()->global_handles();
}


GlobalHandles::NodeBlock* GlobalHandles::Node::FindBlock() {
  intptr_t ptr = reinterpret_cast<intptr_t>(this);
  ptr = ptr - index_ * sizeof(Node);
  NodeBlock* block = reinterpret_cast<NodeBlock*>(ptr);
  DCHECK(block->node_at(index_) == this);
  return block;
}


void GlobalHandles::Node::IncreaseBlockUses() {
  NodeBlock* node_block = FindBlock();
  node_block->IncreaseUses();
  GlobalHandles* global_handles = node_block->global_handles();
  global_handles->isolate()->counters()->global_handles()->Increment();
  global_handles->number_of_global_handles_++;
}


void GlobalHandles::Node::DecreaseBlockUses() {
  NodeBlock* node_block = FindBlock();
  GlobalHandles* global_handles = node_block->global_handles();
  parameter_or_next_free_.next_free = global_handles->first_free_;
  global_handles->first_free_ = this;
  node_block->DecreaseUses();
  global_handles->isolate()->counters()->global_handles()->Decrement();
  global_handles->number_of_global_handles_--;
}


class GlobalHandles::NodeIterator {
 public:
  explicit NodeIterator(GlobalHandles* global_handles)
      : block_(global_handles->first_used_block_),
        index_(0) {}

  bool done() const { return block_ == nullptr; }

  Node* node() const {
    DCHECK(!done());
    return block_->node_at(index_);
  }

  void Advance() {
    DCHECK(!done());
    if (++index_ < NodeBlock::kSize) return;
    index_ = 0;
    block_ = block_->next_used();
  }

 private:
  NodeBlock* block_;
  int index_;

  DISALLOW_COPY_AND_ASSIGN(NodeIterator);
};

class GlobalHandles::PendingPhantomCallbacksSecondPassTask
    : public v8::internal::CancelableTask {
 public:
  // Takes ownership of the contents of pending_phantom_callbacks, leaving it in
  // the same state it would be after a call to Clear().
  PendingPhantomCallbacksSecondPassTask(
      std::vector<PendingPhantomCallback>* pending_phantom_callbacks,
      Isolate* isolate)
      : CancelableTask(isolate), isolate_(isolate) {
    pending_phantom_callbacks_.swap(*pending_phantom_callbacks);
  }

  void RunInternal() override {
    TRACE_EVENT0("v8", "V8.GCPhantomHandleProcessingCallback");
    isolate()->heap()->CallGCPrologueCallbacks(
        GCType::kGCTypeProcessWeakCallbacks, kNoGCCallbackFlags);
    InvokeSecondPassPhantomCallbacks(&pending_phantom_callbacks_, isolate());
    isolate()->heap()->CallGCEpilogueCallbacks(
        GCType::kGCTypeProcessWeakCallbacks, kNoGCCallbackFlags);
  }

  Isolate* isolate() { return isolate_; }

 private:
  Isolate* isolate_;
  std::vector<PendingPhantomCallback> pending_phantom_callbacks_;

  DISALLOW_COPY_AND_ASSIGN(PendingPhantomCallbacksSecondPassTask);
};

GlobalHandles::GlobalHandles(Isolate* isolate)
    : isolate_(isolate),
      number_of_global_handles_(0),
      first_block_(nullptr),
      first_used_block_(nullptr),
      first_free_(nullptr),
      post_gc_processing_count_(0),
      number_of_phantom_handle_resets_(0) {}

GlobalHandles::~GlobalHandles() {
  NodeBlock* block = first_block_;
  while (block != nullptr) {
    NodeBlock* tmp = block->next();
    delete block;
    block = tmp;
  }
  first_block_ = nullptr;
}


Handle<Object> GlobalHandles::Create(Object* value) {
  if (first_free_ == nullptr) {
    first_block_ = new NodeBlock(this, first_block_);
    first_block_->PutNodesOnFreeList(&first_free_);
  }
  DCHECK_NOT_NULL(first_free_);
  // Take the first node in the free list.
  Node* result = first_free_;
  first_free_ = result->next_free();
  result->Acquire(value);
  if (isolate_->heap()->InNewSpace(value) &&
      !result->is_in_new_space_list()) {
    new_space_nodes_.push_back(result);
    result->set_in_new_space_list(true);
  }
  return result->handle();
}


Handle<Object> GlobalHandles::CopyGlobal(Object** location) {
  DCHECK_NOT_NULL(location);
  return Node::FromLocation(location)->GetGlobalHandles()->Create(*location);
}


void GlobalHandles::Destroy(Object** location) {
  if (location != nullptr) Node::FromLocation(location)->Release();
}


typedef v8::WeakCallbackInfo<void>::Callback GenericCallback;


void GlobalHandles::MakeWeak(Object** location, void* parameter,
                             GenericCallback phantom_callback,
                             v8::WeakCallbackType type) {
  Node::FromLocation(location)->MakeWeak(parameter, phantom_callback, type);
}

void GlobalHandles::MakeWeak(Object*** location_addr) {
  Node::FromLocation(*location_addr)->MakeWeak(location_addr);
}

void* GlobalHandles::ClearWeakness(Object** location) {
  return Node::FromLocation(location)->ClearWeakness();
}

bool GlobalHandles::IsNearDeath(Object** location) {
  return Node::FromLocation(location)->IsNearDeath();
}


bool GlobalHandles::IsWeak(Object** location) {
  return Node::FromLocation(location)->IsWeak();
}

DISABLE_CFI_PERF
void GlobalHandles::IterateWeakRootsForFinalizers(RootVisitor* v) {
  for (NodeIterator it(this); !it.done(); it.Advance()) {
    Node* node = it.node();
    if (node->IsWeakRetainer() && node->state() == Node::PENDING) {
      DCHECK(!node->IsPhantomCallback());
      DCHECK(!node->IsPhantomResetHandle());
      // Finalizers need to survive.
      v->VisitRootPointer(Root::kGlobalHandles, node->location());
    }
  }
}

DISABLE_CFI_PERF
void GlobalHandles::IterateWeakRootsForPhantomHandles(
    WeakSlotCallback should_reset_handle) {
  for (NodeIterator it(this); !it.done(); it.Advance()) {
    Node* node = it.node();
    if (node->IsWeakRetainer() && should_reset_handle(node->location())) {
      if (node->IsPhantomResetHandle()) {
        node->MarkPending();
        node->ResetPhantomHandle();
        ++number_of_phantom_handle_resets_;
      } else if (node->IsPhantomCallback()) {
        node->MarkPending();
        node->CollectPhantomCallbackData(isolate(),
                                         &pending_phantom_callbacks_);
      }
    }
  }
}

void GlobalHandles::IdentifyWeakHandles(WeakSlotCallback should_reset_handle) {
  for (NodeIterator it(this); !it.done(); it.Advance()) {
    Node* node = it.node();
    if (node->IsWeak() && should_reset_handle(node->location())) {
      if (!node->IsPhantomCallback() && !node->IsPhantomResetHandle()) {
        node->MarkPending();
      }
    }
  }
}

void GlobalHandles::IterateNewSpaceStrongAndDependentRoots(RootVisitor* v) {
  for (Node* node : new_space_nodes_) {
    if (node->IsStrongRetainer() ||
        (node->IsWeakRetainer() && node->is_active())) {
      v->VisitRootPointer(Root::kGlobalHandles, node->location());
    }
  }
}

void GlobalHandles::IterateNewSpaceStrongAndDependentRootsAndIdentifyUnmodified(
    RootVisitor* v, size_t start, size_t end) {
  for (size_t i = start; i < end; ++i) {
    Node* node = new_space_nodes_[i];
    if (node->IsWeak() && !JSObject::IsUnmodifiedApiObject(node->location())) {
      node->set_active(true);
    }
    if (node->IsStrongRetainer() ||
        (node->IsWeakRetainer() && node->is_active())) {
      v->VisitRootPointer(Root::kGlobalHandles, node->location());
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
    if (node->IsWeak() && is_dead(isolate_->heap(), node->location())) {
      DCHECK(!node->is_active());
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
    if (!node->is_active() && node->IsWeakRetainer() &&
        (node->state() == Node::PENDING)) {
      DCHECK(!node->IsPhantomCallback());
      DCHECK(!node->IsPhantomResetHandle());
      // Finalizers need to survive.
      v->VisitRootPointer(Root::kGlobalHandles, node->location());
    }
  }
}

void GlobalHandles::IterateNewSpaceWeakUnmodifiedRootsForPhantomHandles(
    RootVisitor* v, WeakSlotCallbackWithHeap should_reset_handle) {
  for (Node* node : new_space_nodes_) {
    DCHECK(node->is_in_new_space_list());
    if (!node->is_active() && node->IsWeakRetainer() &&
        (node->state() != Node::PENDING)) {
      DCHECK(node->IsPhantomResetHandle() || node->IsPhantomCallback());
      if (should_reset_handle(isolate_->heap(), node->location())) {
        if (node->IsPhantomResetHandle()) {
          node->MarkPending();
          node->ResetPhantomHandle();
          ++number_of_phantom_handle_resets_;

        } else if (node->IsPhantomCallback()) {
          node->MarkPending();
          node->CollectPhantomCallbackData(isolate(),
                                           &pending_phantom_callbacks_);
        } else {
          UNREACHABLE();
        }
      } else {
        // Node survived and needs to be visited.
        v->VisitRootPointer(Root::kGlobalHandles, node->location());
      }
    }
  }
}

void GlobalHandles::InvokeSecondPassPhantomCallbacks(
    std::vector<PendingPhantomCallback>* callbacks, Isolate* isolate) {
  while (!callbacks->empty()) {
    auto callback = callbacks->back();
    callbacks->pop_back();
    DCHECK_NULL(callback.node());
    // Fire second pass callback
    callback.Invoke(isolate);
  }
}


int GlobalHandles::PostScavengeProcessing(
    const int initial_post_gc_processing_count) {
  int freed_nodes = 0;
  for (Node* node : new_space_nodes_) {
    DCHECK(node->is_in_new_space_list());
    if (!node->IsRetainer()) {
      // Free nodes do not have weak callbacks. Do not use them to compute
      // the freed_nodes.
      continue;
    }

    // Active nodes are kept alive, so no further processing is requires.
    if (node->is_active()) {
      node->set_active(false);
      continue;
    }

    if (node->PostGarbageCollectionProcessing(isolate_)) {
      if (initial_post_gc_processing_count != post_gc_processing_count_) {
        // Weak callback triggered another GC and another round of
        // PostGarbageCollection processing.  The current node might
        // have been deleted in that round, so we need to bail out (or
        // restart the processing).
        return freed_nodes;
      }
    }

    if (!node->IsRetainer()) {
      freed_nodes++;
    }
  }
  return freed_nodes;
}


int GlobalHandles::PostMarkSweepProcessing(
    const int initial_post_gc_processing_count) {
  int freed_nodes = 0;
  for (NodeIterator it(this); !it.done(); it.Advance()) {
    if (!it.node()->IsRetainer()) {
      // Free nodes do not have weak callbacks. Do not use them to compute
      // the freed_nodes.
      continue;
    }
    it.node()->set_active(false);
    if (it.node()->PostGarbageCollectionProcessing(isolate_)) {
      if (initial_post_gc_processing_count != post_gc_processing_count_) {
        // See the comment above.
        return freed_nodes;
      }
    }
    if (!it.node()->IsRetainer()) {
      freed_nodes++;
    }
  }
  return freed_nodes;
}


void GlobalHandles::UpdateListOfNewSpaceNodes() {
  size_t last = 0;
  for (Node* node : new_space_nodes_) {
    DCHECK(node->is_in_new_space_list());
    if (node->IsRetainer()) {
      if (isolate_->heap()->InNewSpace(node->object())) {
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


int GlobalHandles::DispatchPendingPhantomCallbacks(
    bool synchronous_second_pass) {
  int freed_nodes = 0;
  std::vector<PendingPhantomCallback> second_pass_callbacks;
  {
    // The initial pass callbacks must simply clear the nodes.
    for (auto callback : pending_phantom_callbacks_) {
      // Skip callbacks that have already been processed once.
      if (callback.node() == nullptr) continue;
      callback.Invoke(isolate());
      if (callback.callback()) second_pass_callbacks.push_back(callback);
      freed_nodes++;
    }
  }
  pending_phantom_callbacks_.clear();
  if (!second_pass_callbacks.empty()) {
    if (FLAG_optimize_for_size || FLAG_predictable || synchronous_second_pass) {
      isolate()->heap()->CallGCPrologueCallbacks(
          GCType::kGCTypeProcessWeakCallbacks, kNoGCCallbackFlags);
      InvokeSecondPassPhantomCallbacks(&second_pass_callbacks, isolate());
      isolate()->heap()->CallGCEpilogueCallbacks(
          GCType::kGCTypeProcessWeakCallbacks, kNoGCCallbackFlags);
    } else {
      auto task = new PendingPhantomCallbacksSecondPassTask(
          &second_pass_callbacks, isolate());
      V8::GetCurrentPlatform()->CallOnForegroundThread(
          reinterpret_cast<v8::Isolate*>(isolate()), task);
    }
  }
  return freed_nodes;
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
    // Transition to second pass state.
    DCHECK(node_->state() == Node::FREE);
    node_ = nullptr;
  }
}


int GlobalHandles::PostGarbageCollectionProcessing(
    GarbageCollector collector, const v8::GCCallbackFlags gc_callback_flags) {
  // Process weak global handle callbacks. This must be done after the
  // GC is completely done, because the callbacks may invoke arbitrary
  // API functions.
  DCHECK(isolate_->heap()->gc_state() == Heap::NOT_IN_GC);
  const int initial_post_gc_processing_count = ++post_gc_processing_count_;
  int freed_nodes = 0;
  bool synchronous_second_pass =
      (gc_callback_flags &
       (kGCCallbackFlagForced | kGCCallbackFlagCollectAllAvailableGarbage |
        kGCCallbackFlagSynchronousPhantomCallbackProcessing)) != 0;
  freed_nodes += DispatchPendingPhantomCallbacks(synchronous_second_pass);
  if (initial_post_gc_processing_count != post_gc_processing_count_) {
    // If the callbacks caused a nested GC, then return.  See comment in
    // PostScavengeProcessing.
    return freed_nodes;
  }
  if (Heap::IsYoungGenerationCollector(collector)) {
    freed_nodes += PostScavengeProcessing(initial_post_gc_processing_count);
  } else {
    freed_nodes += PostMarkSweepProcessing(initial_post_gc_processing_count);
  }
  if (initial_post_gc_processing_count != post_gc_processing_count_) {
    // If the callbacks caused a nested GC, then return.  See comment in
    // PostScavengeProcessing.
    return freed_nodes;
  }
  if (initial_post_gc_processing_count == post_gc_processing_count_) {
    UpdateListOfNewSpaceNodes();
  }
  return freed_nodes;
}

void GlobalHandles::IterateStrongRoots(RootVisitor* v) {
  for (NodeIterator it(this); !it.done(); it.Advance()) {
    if (it.node()->IsStrongRetainer()) {
      v->VisitRootPointer(Root::kGlobalHandles, it.node()->location());
    }
  }
}


DISABLE_CFI_PERF
void GlobalHandles::IterateAllRoots(RootVisitor* v) {
  for (NodeIterator it(this); !it.done(); it.Advance()) {
    if (it.node()->IsRetainer()) {
      v->VisitRootPointer(Root::kGlobalHandles, it.node()->location());
    }
  }
}

DISABLE_CFI_PERF
void GlobalHandles::IterateAllNewSpaceRoots(RootVisitor* v) {
  for (Node* node : new_space_nodes_) {
    if (node->IsRetainer()) {
      v->VisitRootPointer(Root::kGlobalHandles, node->location());
    }
  }
}

DISABLE_CFI_PERF
void GlobalHandles::IterateNewSpaceRoots(RootVisitor* v, size_t start,
                                         size_t end) {
  for (size_t i = start; i < end; ++i) {
    Node* node = new_space_nodes_[i];
    if (node->IsRetainer()) {
      v->VisitRootPointer(Root::kGlobalHandles, node->location());
    }
  }
}

DISABLE_CFI_PERF
void GlobalHandles::ApplyPersistentHandleVisitor(
    v8::PersistentHandleVisitor* visitor, GlobalHandles::Node* node) {
  v8::Value* value = ToApi<v8::Value>(Handle<Object>(node->location()));
  visitor->VisitPersistentHandle(
      reinterpret_cast<v8::Persistent<v8::Value>*>(&value),
      node->wrapper_class_id());
}

DISABLE_CFI_PERF
void GlobalHandles::IterateAllRootsWithClassIds(
    v8::PersistentHandleVisitor* visitor) {
  for (NodeIterator it(this); !it.done(); it.Advance()) {
    if (it.node()->IsRetainer() && it.node()->has_wrapper_class_id()) {
      ApplyPersistentHandleVisitor(visitor, it.node());
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
  for (NodeIterator it(this); !it.done(); it.Advance()) {
    *stats->global_handle_count += 1;
    if (it.node()->state() == Node::WEAK) {
      *stats->weak_global_handle_count += 1;
    } else if (it.node()->state() == Node::PENDING) {
      *stats->pending_global_handle_count += 1;
    } else if (it.node()->state() == Node::NEAR_DEATH) {
      *stats->near_death_global_handle_count += 1;
    } else if (it.node()->state() == Node::FREE) {
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

  for (NodeIterator it(this); !it.done(); it.Advance()) {
    total++;
    if (it.node()->state() == Node::WEAK) weak++;
    if (it.node()->state() == Node::PENDING) pending++;
    if (it.node()->state() == Node::NEAR_DEATH) near_death++;
    if (it.node()->state() == Node::FREE) destroyed++;
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
  for (NodeIterator it(this); !it.done(); it.Advance()) {
    PrintF("  handle %p to %p%s\n",
           reinterpret_cast<void*>(it.node()->location()),
           reinterpret_cast<void*>(it.node()->object()),
           it.node()->IsWeak() ? " (weak)" : "");
  }
}

#endif

void GlobalHandles::TearDown() {}

EternalHandles::EternalHandles() : size_(0) {
  for (unsigned i = 0; i < arraysize(singleton_handles_); i++) {
    singleton_handles_[i] = kInvalidIndex;
  }
}


EternalHandles::~EternalHandles() {
  for (Object** block : blocks_) delete[] block;
}

void EternalHandles::IterateAllRoots(RootVisitor* visitor) {
  int limit = size_;
  for (Object** block : blocks_) {
    DCHECK_GT(limit, 0);
    visitor->VisitRootPointers(Root::kEternalHandles, block,
                               block + Min(limit, kSize));
    limit -= kSize;
  }
}

void EternalHandles::IterateNewSpaceRoots(RootVisitor* visitor) {
  for (int index : new_space_indices_) {
    visitor->VisitRootPointer(Root::kEternalHandles, GetLocation(index));
  }
}


void EternalHandles::PostGarbageCollectionProcessing(Heap* heap) {
  size_t last = 0;
  for (int index : new_space_indices_) {
    if (heap->InNewSpace(*GetLocation(index))) {
      new_space_indices_[last++] = index;
    }
  }
  DCHECK_LE(last, new_space_indices_.size());
  new_space_indices_.resize(last);
}


void EternalHandles::Create(Isolate* isolate, Object* object, int* index) {
  DCHECK_EQ(kInvalidIndex, *index);
  if (object == nullptr) return;
  DCHECK_NE(isolate->heap()->the_hole_value(), object);
  int block = size_ >> kShift;
  int offset = size_ & kMask;
  // need to resize
  if (offset == 0) {
    Object** next_block = new Object*[kSize];
    Object* the_hole = isolate->heap()->the_hole_value();
    MemsetPointer(next_block, the_hole, kSize);
    blocks_.push_back(next_block);
  }
  DCHECK_EQ(isolate->heap()->the_hole_value(), blocks_[block][offset]);
  blocks_[block][offset] = object;
  if (isolate->heap()->InNewSpace(object)) {
    new_space_indices_.push_back(size_);
  }
  *index = size_++;
}


}  // namespace internal
}  // namespace v8
