// Copyright 2009 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/global-handles.h"

#include "src/api.h"
#include "src/v8.h"
#include "src/vm-state-inl.h"

namespace v8 {
namespace internal {


ObjectGroup::~ObjectGroup() {
  if (info != NULL) info->Dispose();
  delete[] objects;
}


ImplicitRefGroup::~ImplicitRefGroup() {
  delete[] children;
}


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
    DCHECK(offsetof(Node, object_) == 0);
    return reinterpret_cast<Node*>(location);
  }

  Node() {
    DCHECK(offsetof(Node, class_id_) == Internals::kNodeClassIdOffset);
    DCHECK(offsetof(Node, flags_) == Internals::kNodeFlagsOffset);
    STATIC_ASSERT(static_cast<int>(NodeState::kMask) ==
                  Internals::kNodeStateMask);
    STATIC_ASSERT(WEAK == Internals::kNodeStateIsWeakValue);
    STATIC_ASSERT(PENDING == Internals::kNodeStateIsPendingValue);
    STATIC_ASSERT(NEAR_DEATH == Internals::kNodeStateIsNearDeathValue);
    STATIC_ASSERT(static_cast<int>(IsIndependent::kShift) ==
                  Internals::kNodeIsIndependentShift);
    STATIC_ASSERT(static_cast<int>(IsPartiallyDependent::kShift) ==
                  Internals::kNodeIsPartiallyDependentShift);
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
    set_independent(false);
    if (FLAG_scavenge_reclaim_unmodified_objects) {
      set_active(false);
    } else {
      set_partially_dependent(false);
    }
    set_in_new_space_list(false);
    parameter_or_next_free_.next_free = NULL;
    weak_callback_ = NULL;
  }
#endif

  void Initialize(int index, Node** first_free) {
    object_ = reinterpret_cast<Object*>(kGlobalHandleZapValue);
    index_ = static_cast<uint8_t>(index);
    DCHECK(static_cast<int>(index_) == index);
    set_state(FREE);
    set_weakness_type(NORMAL_WEAK);
    set_in_new_space_list(false);
    parameter_or_next_free_.next_free = *first_free;
    *first_free = this;
  }

  void Acquire(Object* object) {
    DCHECK(state() == FREE);
    object_ = object;
    class_id_ = v8::HeapProfiler::kPersistentHandleNoClassId;
    set_independent(false);
    if (FLAG_scavenge_reclaim_unmodified_objects) {
      set_active(false);
    } else {
      set_partially_dependent(false);
    }
    set_state(NORMAL);
    parameter_or_next_free_.parameter = NULL;
    weak_callback_ = NULL;
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
    set_independent(false);
    if (FLAG_scavenge_reclaim_unmodified_objects) {
      set_active(false);
    } else {
      set_partially_dependent(false);
    }
    weak_callback_ = NULL;
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

  bool is_independent() {
    return IsIndependent::decode(flags_);
  }
  void set_independent(bool v) {
    flags_ = IsIndependent::update(flags_, v);
  }

  bool is_partially_dependent() {
    CHECK(!FLAG_scavenge_reclaim_unmodified_objects);
    return IsPartiallyDependent::decode(flags_);
  }
  void set_partially_dependent(bool v) {
    CHECK(!FLAG_scavenge_reclaim_unmodified_objects);
    flags_ = IsPartiallyDependent::update(flags_, v);
  }

  bool is_active() {
    CHECK(FLAG_scavenge_reclaim_unmodified_objects);
    return IsActive::decode(flags_);
  }
  void set_active(bool v) {
    CHECK(FLAG_scavenge_reclaim_unmodified_objects);
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

  bool IsRetainer() const {
    return state() != FREE &&
           !(state() == NEAR_DEATH && weakness_type() != NORMAL_WEAK);
  }

  bool IsStrongRetainer() const { return state() == NORMAL; }

  bool IsWeakRetainer() const {
    return state() == WEAK || state() == PENDING ||
           (state() == NEAR_DEATH && weakness_type() == NORMAL_WEAK);
  }

  void MarkPending() {
    DCHECK(state() == WEAK);
    set_state(PENDING);
  }

  // Independent flag accessors.
  void MarkIndependent() {
    DCHECK(IsInUse());
    set_independent(true);
  }

  void MarkPartiallyDependent() {
    DCHECK(IsInUse());
    if (GetGlobalHandles()->isolate()->heap()->InNewSpace(object_)) {
      set_partially_dependent(true);
    }
  }
  void clear_partially_dependent() { set_partially_dependent(false); }

  // Callback accessor.
  // TODO(svenpanne) Re-enable or nuke later.
  // WeakReferenceCallback callback() { return callback_; }

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

  void MakeWeak(void* parameter, WeakCallback weak_callback) {
    DCHECK(weak_callback != nullptr);
    DCHECK(IsInUse());
    CHECK_NE(object_, reinterpret_cast<Object*>(kGlobalHandleZapValue));
    set_state(WEAK);
    set_weakness_type(NORMAL_WEAK);
    set_parameter(parameter);
    weak_callback_ = weak_callback;
  }

  void MakeWeak(void* parameter,
                WeakCallbackInfo<void>::Callback phantom_callback,
                v8::WeakCallbackType type) {
    DCHECK(phantom_callback != nullptr);
    DCHECK(IsInUse());
    CHECK_NE(object_, reinterpret_cast<Object*>(kGlobalHandleZapValue));
    set_state(WEAK);
    switch (type) {
      case v8::WeakCallbackType::kParameter:
        set_weakness_type(PHANTOM_WEAK);
        break;
      case v8::WeakCallbackType::kInternalFields:
      set_weakness_type(PHANTOM_WEAK_2_INTERNAL_FIELDS);
      break;
    }
    set_parameter(parameter);
    weak_callback_ = reinterpret_cast<WeakCallback>(phantom_callback);
  }

  void* ClearWeakness() {
    DCHECK(IsInUse());
    void* p = parameter();
    set_state(NORMAL);
    set_parameter(NULL);
    return p;
  }

  void CollectPhantomCallbackData(
      Isolate* isolate,
      List<PendingPhantomCallback>* pending_phantom_callbacks) {
    DCHECK(weakness_type() == PHANTOM_WEAK ||
           weakness_type() == PHANTOM_WEAK_2_INTERNAL_FIELDS);
    DCHECK(state() == PENDING);

    void* internal_fields[v8::kInternalFieldsInWeakCallback] = {nullptr,
                                                                nullptr};
    if (weakness_type() != PHANTOM_WEAK && object()->IsJSObject()) {
      auto jsobject = JSObject::cast(object());
      int field_count = jsobject->GetInternalFieldCount();
      for (int i = 0; i < v8::kInternalFieldsInWeakCallback; ++i) {
        if (field_count == i) break;
        auto field = jsobject->GetInternalField(i);
        if (field->IsSmi()) internal_fields[i] = field;
      }
    }

    // Zap with something dangerous.
    *location() = reinterpret_cast<Object*>(0x6057ca11);

    typedef v8::WeakCallbackInfo<void> Data;
    auto callback = reinterpret_cast<Data::Callback>(weak_callback_);
    pending_phantom_callbacks->Add(
        PendingPhantomCallback(this, callback, parameter(), internal_fields));
    DCHECK(IsInUse());
    set_state(NEAR_DEATH);
  }

  bool PostGarbageCollectionProcessing(Isolate* isolate) {
    // Handles only weak handles (not phantom) that are dying.
    if (state() != Node::PENDING) return false;
    if (weak_callback_ == NULL) {
      Release();
      return false;
    }
    set_state(NEAR_DEATH);

    // Check that we are not passing a finalized external string to
    // the callback.
    DCHECK(!object_->IsExternalOneByteString() ||
           ExternalOneByteString::cast(object_)->resource() != NULL);
    DCHECK(!object_->IsExternalTwoByteString() ||
           ExternalTwoByteString::cast(object_)->resource() != NULL);
    if (weakness_type() != NORMAL_WEAK) return false;

    // Leaving V8.
    VMState<EXTERNAL> vmstate(isolate);
    HandleScope handle_scope(isolate);
    Object** object = location();
    Handle<Object> handle(*object, isolate);
    v8::WeakCallbackData<v8::Value, void> data(
        reinterpret_cast<v8::Isolate*>(isolate), parameter(),
        v8::Utils::ToLocal(handle));
    set_parameter(NULL);
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
  class IsPartiallyDependent : public BitField<bool, 4, 1> {};
  class IsInNewSpaceList : public BitField<bool, 5, 1> {};
  class NodeWeaknessType : public BitField<WeaknessType, 6, 2> {};

  uint8_t flags_;

  // Handle specific callback - might be a weak reference in disguise.
  WeakCallback weak_callback_;

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
        next_used_(NULL),
        prev_used_(NULL),
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
    DCHECK(used_nodes_ < kSize);
    if (used_nodes_++ == 0) {
      NodeBlock* old_first = global_handles_->first_used_block_;
      global_handles_->first_used_block_ = this;
      next_used_ = old_first;
      prev_used_ = NULL;
      if (old_first == NULL) return;
      old_first->prev_used_ = this;
    }
  }

  void DecreaseUses() {
    DCHECK(used_nodes_ > 0);
    if (--used_nodes_ == 0) {
      if (next_used_ != NULL) next_used_->prev_used_ = prev_used_;
      if (prev_used_ != NULL) prev_used_->next_used_ = next_used_;
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

  bool done() const { return block_ == NULL; }

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
      List<PendingPhantomCallback>* pending_phantom_callbacks, Isolate* isolate)
      : CancelableTask(isolate) {
    pending_phantom_callbacks_.Swap(pending_phantom_callbacks);
  }

  void RunInternal() override {
    isolate()->heap()->CallGCPrologueCallbacks(
        GCType::kGCTypeProcessWeakCallbacks, kNoGCCallbackFlags);
    InvokeSecondPassPhantomCallbacks(&pending_phantom_callbacks_, isolate());
    isolate()->heap()->CallGCEpilogueCallbacks(
        GCType::kGCTypeProcessWeakCallbacks, kNoGCCallbackFlags);
  }

 private:
  List<PendingPhantomCallback> pending_phantom_callbacks_;

  DISALLOW_COPY_AND_ASSIGN(PendingPhantomCallbacksSecondPassTask);
};


GlobalHandles::GlobalHandles(Isolate* isolate)
    : isolate_(isolate),
      number_of_global_handles_(0),
      first_block_(NULL),
      first_used_block_(NULL),
      first_free_(NULL),
      post_gc_processing_count_(0),
      object_group_connections_(kObjectGroupConnectionsCapacity) {}


GlobalHandles::~GlobalHandles() {
  NodeBlock* block = first_block_;
  while (block != NULL) {
    NodeBlock* tmp = block->next();
    delete block;
    block = tmp;
  }
  first_block_ = NULL;
}


Handle<Object> GlobalHandles::Create(Object* value) {
  if (first_free_ == NULL) {
    first_block_ = new NodeBlock(this, first_block_);
    first_block_->PutNodesOnFreeList(&first_free_);
  }
  DCHECK(first_free_ != NULL);
  // Take the first node in the free list.
  Node* result = first_free_;
  first_free_ = result->next_free();
  result->Acquire(value);
  if (isolate_->heap()->InNewSpace(value) &&
      !result->is_in_new_space_list()) {
    new_space_nodes_.Add(result);
    result->set_in_new_space_list(true);
  }
  return result->handle();
}


Handle<Object> GlobalHandles::CopyGlobal(Object** location) {
  DCHECK(location != NULL);
  return Node::FromLocation(location)->GetGlobalHandles()->Create(*location);
}


void GlobalHandles::Destroy(Object** location) {
  if (location != NULL) Node::FromLocation(location)->Release();
}


void GlobalHandles::MakeWeak(Object** location, void* parameter,
                             WeakCallback weak_callback) {
  Node::FromLocation(location)->MakeWeak(parameter, weak_callback);
}


typedef v8::WeakCallbackInfo<void>::Callback GenericCallback;


void GlobalHandles::MakeWeak(Object** location, void* parameter,
                             GenericCallback phantom_callback,
                             v8::WeakCallbackType type) {
  Node::FromLocation(location)->MakeWeak(parameter, phantom_callback, type);
}


void* GlobalHandles::ClearWeakness(Object** location) {
  return Node::FromLocation(location)->ClearWeakness();
}


void GlobalHandles::MarkIndependent(Object** location) {
  Node::FromLocation(location)->MarkIndependent();
}


void GlobalHandles::MarkPartiallyDependent(Object** location) {
  Node::FromLocation(location)->MarkPartiallyDependent();
}


bool GlobalHandles::IsIndependent(Object** location) {
  return Node::FromLocation(location)->is_independent();
}


bool GlobalHandles::IsNearDeath(Object** location) {
  return Node::FromLocation(location)->IsNearDeath();
}


bool GlobalHandles::IsWeak(Object** location) {
  return Node::FromLocation(location)->IsWeak();
}


void GlobalHandles::IterateWeakRoots(ObjectVisitor* v) {
  for (NodeIterator it(this); !it.done(); it.Advance()) {
    Node* node = it.node();
    if (node->IsWeakRetainer()) {
      // Pending weak phantom handles die immediately. Everything else survives.
      if (node->state() == Node::PENDING &&
          node->weakness_type() != NORMAL_WEAK) {
          node->CollectPhantomCallbackData(isolate(),
                                           &pending_phantom_callbacks_);
      } else {
        v->VisitPointer(node->location());
      }
    }
  }
}


void GlobalHandles::IdentifyWeakHandles(WeakSlotCallback f) {
  for (NodeIterator it(this); !it.done(); it.Advance()) {
    if (it.node()->IsWeak() && f(it.node()->location())) {
      it.node()->MarkPending();
    }
  }
}


void GlobalHandles::IterateNewSpaceStrongAndDependentRoots(ObjectVisitor* v) {
  for (int i = 0; i < new_space_nodes_.length(); ++i) {
    Node* node = new_space_nodes_[i];
    if (FLAG_scavenge_reclaim_unmodified_objects) {
      if (node->IsStrongRetainer() ||
          (node->IsWeakRetainer() && !node->is_independent() &&
           node->is_active())) {
        v->VisitPointer(node->location());
      }
    } else {
      if (node->IsStrongRetainer() ||
          (node->IsWeakRetainer() && !node->is_independent() &&
           !node->is_partially_dependent())) {
        v->VisitPointer(node->location());
      }
    }
  }
}


void GlobalHandles::IdentifyNewSpaceWeakIndependentHandles(
    WeakSlotCallbackWithHeap f) {
  for (int i = 0; i < new_space_nodes_.length(); ++i) {
    Node* node = new_space_nodes_[i];
    DCHECK(node->is_in_new_space_list());
    if ((node->is_independent() || node->is_partially_dependent()) &&
        node->IsWeak() && f(isolate_->heap(), node->location())) {
      node->MarkPending();
    }
  }
}


void GlobalHandles::IterateNewSpaceWeakIndependentRoots(ObjectVisitor* v) {
  for (int i = 0; i < new_space_nodes_.length(); ++i) {
    Node* node = new_space_nodes_[i];
    DCHECK(node->is_in_new_space_list());
    if ((node->is_independent() || node->is_partially_dependent()) &&
        node->IsWeakRetainer()) {
      // Pending weak phantom handles die immediately. Everything else survives.
      if (node->state() == Node::PENDING &&
          node->weakness_type() != NORMAL_WEAK) {
        node->CollectPhantomCallbackData(isolate(),
                                         &pending_phantom_callbacks_);
      } else {
        v->VisitPointer(node->location());
      }
    }
  }
}


void GlobalHandles::IdentifyWeakUnmodifiedObjects(
    WeakSlotCallback is_unmodified) {
  for (int i = 0; i < new_space_nodes_.length(); ++i) {
    Node* node = new_space_nodes_[i];
    if (node->IsWeak() && !is_unmodified(node->location())) {
      node->set_active(true);
    }
  }
}


void GlobalHandles::MarkNewSpaceWeakUnmodifiedObjectsPending(
    WeakSlotCallbackWithHeap is_unscavenged) {
  for (int i = 0; i < new_space_nodes_.length(); ++i) {
    Node* node = new_space_nodes_[i];
    DCHECK(node->is_in_new_space_list());
    if ((node->is_independent() || !node->is_active()) && node->IsWeak() &&
        is_unscavenged(isolate_->heap(), node->location())) {
      node->MarkPending();
    }
  }
}


void GlobalHandles::IterateNewSpaceWeakUnmodifiedRoots(ObjectVisitor* v) {
  for (int i = 0; i < new_space_nodes_.length(); ++i) {
    Node* node = new_space_nodes_[i];
    DCHECK(node->is_in_new_space_list());
    if ((node->is_independent() || !node->is_active()) &&
        node->IsWeakRetainer()) {
      // Pending weak phantom handles die immediately. Everything else survives.
      if (node->state() == Node::PENDING &&
          node->weakness_type() != NORMAL_WEAK) {
        node->CollectPhantomCallbackData(isolate(),
                                         &pending_phantom_callbacks_);
      } else {
        v->VisitPointer(node->location());
      }
    }
  }
}


bool GlobalHandles::IterateObjectGroups(ObjectVisitor* v,
                                        WeakSlotCallbackWithHeap can_skip) {
  ComputeObjectGroupsAndImplicitReferences();
  int last = 0;
  bool any_group_was_visited = false;
  for (int i = 0; i < object_groups_.length(); i++) {
    ObjectGroup* entry = object_groups_.at(i);
    DCHECK(entry != NULL);

    Object*** objects = entry->objects;
    bool group_should_be_visited = false;
    for (size_t j = 0; j < entry->length; j++) {
      Object* object = *objects[j];
      if (object->IsHeapObject()) {
        if (!can_skip(isolate_->heap(), &object)) {
          group_should_be_visited = true;
          break;
        }
      }
    }

    if (!group_should_be_visited) {
      object_groups_[last++] = entry;
      continue;
    }

    // An object in the group requires visiting, so iterate over all
    // objects in the group.
    for (size_t j = 0; j < entry->length; ++j) {
      Object* object = *objects[j];
      if (object->IsHeapObject()) {
        v->VisitPointer(&object);
        any_group_was_visited = true;
      }
    }

    // Once the entire group has been iterated over, set the object
    // group to NULL so it won't be processed again.
    delete entry;
    object_groups_.at(i) = NULL;
  }
  object_groups_.Rewind(last);
  return any_group_was_visited;
}


void GlobalHandles::InvokeSecondPassPhantomCallbacks(
    List<PendingPhantomCallback>* callbacks, Isolate* isolate) {
  while (callbacks->length() != 0) {
    auto callback = callbacks->RemoveLast();
    DCHECK(callback.node() == nullptr);
    // No second pass callback required.
    if (callback.callback() == nullptr) continue;
    // Fire second pass callback
    callback.Invoke(isolate);
  }
}


int GlobalHandles::PostScavengeProcessing(
    const int initial_post_gc_processing_count) {
  int freed_nodes = 0;
  for (int i = 0; i < new_space_nodes_.length(); ++i) {
    Node* node = new_space_nodes_[i];
    DCHECK(node->is_in_new_space_list());
    if (!node->IsRetainer()) {
      // Free nodes do not have weak callbacks. Do not use them to compute
      // the freed_nodes.
      continue;
    }
    // Skip dependent or unmodified handles. Their weak callbacks might expect
    // to be
    // called between two global garbage collection callbacks which
    // are not called for minor collections.
    if (FLAG_scavenge_reclaim_unmodified_objects) {
      if (!node->is_independent() && (node->is_active())) {
        node->set_active(false);
        continue;
      }
      node->set_active(false);
    } else {
      if (!node->is_independent() && !node->is_partially_dependent()) {
        continue;
      }
      node->clear_partially_dependent();
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
    if (FLAG_scavenge_reclaim_unmodified_objects) {
      it.node()->set_active(false);
    } else {
      it.node()->clear_partially_dependent();
    }
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
  int last = 0;
  for (int i = 0; i < new_space_nodes_.length(); ++i) {
    Node* node = new_space_nodes_[i];
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
  new_space_nodes_.Rewind(last);
  new_space_nodes_.Trim();
}


int GlobalHandles::DispatchPendingPhantomCallbacks(
    bool synchronous_second_pass) {
  int freed_nodes = 0;
  {
    // The initial pass callbacks must simply clear the nodes.
    for (auto i = pending_phantom_callbacks_.begin();
         i != pending_phantom_callbacks_.end(); ++i) {
      auto callback = i;
      // Skip callbacks that have already been processed once.
      if (callback->node() == nullptr) continue;
      callback->Invoke(isolate());
      freed_nodes++;
    }
  }
  if (pending_phantom_callbacks_.length() > 0) {
    if (FLAG_optimize_for_size || FLAG_predictable || synchronous_second_pass) {
      isolate()->heap()->CallGCPrologueCallbacks(
          GCType::kGCTypeProcessWeakCallbacks, kNoGCCallbackFlags);
      InvokeSecondPassPhantomCallbacks(&pending_phantom_callbacks_, isolate());
      isolate()->heap()->CallGCEpilogueCallbacks(
          GCType::kGCTypeProcessWeakCallbacks, kNoGCCallbackFlags);
    } else {
      auto task = new PendingPhantomCallbacksSecondPassTask(
          &pending_phantom_callbacks_, isolate());
      V8::GetCurrentPlatform()->CallOnForegroundThread(
          reinterpret_cast<v8::Isolate*>(isolate()), task);
    }
  }
  pending_phantom_callbacks_.Clear();
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
            internal_fields_, callback_addr);
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
       (kGCCallbackFlagForced |
        kGCCallbackFlagSynchronousPhantomCallbackProcessing)) != 0;
  freed_nodes += DispatchPendingPhantomCallbacks(synchronous_second_pass);
  if (initial_post_gc_processing_count != post_gc_processing_count_) {
    // If the callbacks caused a nested GC, then return.  See comment in
    // PostScavengeProcessing.
    return freed_nodes;
  }
  if (collector == SCAVENGER) {
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


void GlobalHandles::IterateStrongRoots(ObjectVisitor* v) {
  for (NodeIterator it(this); !it.done(); it.Advance()) {
    if (it.node()->IsStrongRetainer()) {
      v->VisitPointer(it.node()->location());
    }
  }
}


void GlobalHandles::IterateAllRoots(ObjectVisitor* v) {
  for (NodeIterator it(this); !it.done(); it.Advance()) {
    if (it.node()->IsRetainer()) {
      v->VisitPointer(it.node()->location());
    }
  }
}


void GlobalHandles::IterateAllRootsWithClassIds(ObjectVisitor* v) {
  for (NodeIterator it(this); !it.done(); it.Advance()) {
    if (it.node()->IsRetainer() && it.node()->has_wrapper_class_id()) {
      v->VisitEmbedderReference(it.node()->location(),
                                it.node()->wrapper_class_id());
    }
  }
}


void GlobalHandles::IterateAllRootsInNewSpaceWithClassIds(ObjectVisitor* v) {
  for (int i = 0; i < new_space_nodes_.length(); ++i) {
    Node* node = new_space_nodes_[i];
    if (node->IsRetainer() && node->has_wrapper_class_id()) {
      v->VisitEmbedderReference(node->location(),
                                node->wrapper_class_id());
    }
  }
}


void GlobalHandles::IterateWeakRootsInNewSpaceWithClassIds(ObjectVisitor* v) {
  for (int i = 0; i < new_space_nodes_.length(); ++i) {
    Node* node = new_space_nodes_[i];
    if (node->has_wrapper_class_id() && node->IsWeak()) {
      v->VisitEmbedderReference(node->location(), node->wrapper_class_id());
    }
  }
}


int GlobalHandles::NumberOfWeakHandles() {
  int count = 0;
  for (NodeIterator it(this); !it.done(); it.Advance()) {
    if (it.node()->IsWeakRetainer()) {
      count++;
    }
  }
  return count;
}


int GlobalHandles::NumberOfGlobalObjectWeakHandles() {
  int count = 0;
  for (NodeIterator it(this); !it.done(); it.Advance()) {
    if (it.node()->IsWeakRetainer() &&
        it.node()->object()->IsJSGlobalObject()) {
      count++;
    }
  }
  return count;
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
  PrintF("  allocated memory = %" V8_PTR_PREFIX "dB\n", sizeof(Node) * total);
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



void GlobalHandles::AddObjectGroup(Object*** handles,
                                   size_t length,
                                   v8::RetainedObjectInfo* info) {
#ifdef DEBUG
  for (size_t i = 0; i < length; ++i) {
    DCHECK(!Node::FromLocation(handles[i])->is_independent());
  }
#endif
  if (length == 0) {
    if (info != NULL) info->Dispose();
    return;
  }
  ObjectGroup* group = new ObjectGroup(length);
  for (size_t i = 0; i < length; ++i)
    group->objects[i] = handles[i];
  group->info = info;
  object_groups_.Add(group);
}


void GlobalHandles::SetObjectGroupId(Object** handle,
                                     UniqueId id) {
  object_group_connections_.Add(ObjectGroupConnection(id, handle));
}


void GlobalHandles::SetRetainedObjectInfo(UniqueId id,
                                          RetainedObjectInfo* info) {
  retainer_infos_.Add(ObjectGroupRetainerInfo(id, info));
}


void GlobalHandles::SetReferenceFromGroup(UniqueId id, Object** child) {
  DCHECK(!Node::FromLocation(child)->is_independent());
  implicit_ref_connections_.Add(ObjectGroupConnection(id, child));
}


void GlobalHandles::SetReference(HeapObject** parent, Object** child) {
  DCHECK(!Node::FromLocation(child)->is_independent());
  ImplicitRefGroup* group = new ImplicitRefGroup(parent, 1);
  group->children[0] = child;
  implicit_ref_groups_.Add(group);
}


void GlobalHandles::RemoveObjectGroups() {
  for (int i = 0; i < object_groups_.length(); i++)
    delete object_groups_.at(i);
  object_groups_.Clear();
  for (int i = 0; i < retainer_infos_.length(); ++i)
    retainer_infos_[i].info->Dispose();
  retainer_infos_.Clear();
  object_group_connections_.Clear();
  object_group_connections_.Initialize(kObjectGroupConnectionsCapacity);
}


void GlobalHandles::RemoveImplicitRefGroups() {
  for (int i = 0; i < implicit_ref_groups_.length(); i++) {
    delete implicit_ref_groups_.at(i);
  }
  implicit_ref_groups_.Clear();
  implicit_ref_connections_.Clear();
}


void GlobalHandles::TearDown() {
  // TODO(1428): invoke weak callbacks.
}


void GlobalHandles::ComputeObjectGroupsAndImplicitReferences() {
  if (object_group_connections_.length() == 0) {
    for (int i = 0; i < retainer_infos_.length(); ++i)
      retainer_infos_[i].info->Dispose();
    retainer_infos_.Clear();
    implicit_ref_connections_.Clear();
    return;
  }

  object_group_connections_.Sort();
  retainer_infos_.Sort();
  implicit_ref_connections_.Sort();

  int info_index = 0;  // For iterating retainer_infos_.
  UniqueId current_group_id(0);
  int current_group_start = 0;

  int current_implicit_refs_start = 0;
  int current_implicit_refs_end = 0;
  for (int i = 0; i <= object_group_connections_.length(); ++i) {
    if (i == 0)
      current_group_id = object_group_connections_[i].id;
    if (i == object_group_connections_.length() ||
        current_group_id != object_group_connections_[i].id) {
      // Group detected: objects in indices [current_group_start, i[.

      // Find out which implicit references are related to this group. (We want
      // to ignore object groups which only have 1 object, but that object is
      // needed as a representative object for the implicit refrerence group.)
      while (current_implicit_refs_start < implicit_ref_connections_.length() &&
             implicit_ref_connections_[current_implicit_refs_start].id <
                 current_group_id)
        ++current_implicit_refs_start;
      current_implicit_refs_end = current_implicit_refs_start;
      while (current_implicit_refs_end < implicit_ref_connections_.length() &&
             implicit_ref_connections_[current_implicit_refs_end].id ==
                 current_group_id)
        ++current_implicit_refs_end;

      if (current_implicit_refs_end > current_implicit_refs_start) {
        // Find a representative object for the implicit references.
        HeapObject** representative = NULL;
        for (int j = current_group_start; j < i; ++j) {
          Object** object = object_group_connections_[j].object;
          if ((*object)->IsHeapObject()) {
            representative = reinterpret_cast<HeapObject**>(object);
            break;
          }
        }
        if (representative) {
          ImplicitRefGroup* group = new ImplicitRefGroup(
              representative,
              current_implicit_refs_end - current_implicit_refs_start);
          for (int j = current_implicit_refs_start;
               j < current_implicit_refs_end;
               ++j) {
            group->children[j - current_implicit_refs_start] =
                implicit_ref_connections_[j].object;
          }
          implicit_ref_groups_.Add(group);
        }
        current_implicit_refs_start = current_implicit_refs_end;
      }

      // Find a RetainedObjectInfo for the group.
      RetainedObjectInfo* info = NULL;
      while (info_index < retainer_infos_.length() &&
             retainer_infos_[info_index].id < current_group_id) {
        retainer_infos_[info_index].info->Dispose();
        ++info_index;
      }
      if (info_index < retainer_infos_.length() &&
          retainer_infos_[info_index].id == current_group_id) {
        // This object group has an associated ObjectGroupRetainerInfo.
        info = retainer_infos_[info_index].info;
        ++info_index;
      }

      // Ignore groups which only contain one object.
      if (i > current_group_start + 1) {
        ObjectGroup* group = new ObjectGroup(i - current_group_start);
        for (int j = current_group_start; j < i; ++j) {
          group->objects[j - current_group_start] =
              object_group_connections_[j].object;
        }
        group->info = info;
        object_groups_.Add(group);
      } else if (info) {
        info->Dispose();
      }

      if (i < object_group_connections_.length()) {
        current_group_id = object_group_connections_[i].id;
        current_group_start = i;
      }
    }
  }
  object_group_connections_.Clear();
  object_group_connections_.Initialize(kObjectGroupConnectionsCapacity);
  retainer_infos_.Clear();
  implicit_ref_connections_.Clear();
}


EternalHandles::EternalHandles() : size_(0) {
  for (unsigned i = 0; i < arraysize(singleton_handles_); i++) {
    singleton_handles_[i] = kInvalidIndex;
  }
}


EternalHandles::~EternalHandles() {
  for (int i = 0; i < blocks_.length(); i++) delete[] blocks_[i];
}


void EternalHandles::IterateAllRoots(ObjectVisitor* visitor) {
  int limit = size_;
  for (int i = 0; i < blocks_.length(); i++) {
    DCHECK(limit > 0);
    Object** block = blocks_[i];
    visitor->VisitPointers(block, block + Min(limit, kSize));
    limit -= kSize;
  }
}


void EternalHandles::IterateNewSpaceRoots(ObjectVisitor* visitor) {
  for (int i = 0; i < new_space_indices_.length(); i++) {
    visitor->VisitPointer(GetLocation(new_space_indices_[i]));
  }
}


void EternalHandles::PostGarbageCollectionProcessing(Heap* heap) {
  int last = 0;
  for (int i = 0; i < new_space_indices_.length(); i++) {
    int index = new_space_indices_[i];
    if (heap->InNewSpace(*GetLocation(index))) {
      new_space_indices_[last++] = index;
    }
  }
  new_space_indices_.Rewind(last);
}


void EternalHandles::Create(Isolate* isolate, Object* object, int* index) {
  DCHECK_EQ(kInvalidIndex, *index);
  if (object == NULL) return;
  DCHECK_NE(isolate->heap()->the_hole_value(), object);
  int block = size_ >> kShift;
  int offset = size_ & kMask;
  // need to resize
  if (offset == 0) {
    Object** next_block = new Object*[kSize];
    Object* the_hole = isolate->heap()->the_hole_value();
    MemsetPointer(next_block, the_hole, kSize);
    blocks_.Add(next_block);
  }
  DCHECK_EQ(isolate->heap()->the_hole_value(), blocks_[block][offset]);
  blocks_[block][offset] = object;
  if (isolate->heap()->InNewSpace(object)) {
    new_space_indices_.Add(size_);
  }
  *index = size_++;
}


}  // namespace internal
}  // namespace v8
