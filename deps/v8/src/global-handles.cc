// Copyright 2009 the V8 project authors. All rights reserved.
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
//       copyright notice, this list of conditions and the following
//       disclaimer in the documentation and/or other materials provided
//       with the distribution.
//     * Neither the name of Google Inc. nor the names of its
//       contributors may be used to endorse or promote products derived
//       from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include "v8.h"

#include "api.h"
#include "global-handles.h"

#include "vm-state-inl.h"

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
    NORMAL,     // Normal global handle.
    WEAK,       // Flagged as weak but not yet finalized.
    PENDING,    // Has been recognized as only reachable by weak handles.
    NEAR_DEATH,  // Callback has informed the handle is near death.

    NUMBER_OF_STATES
  };

  // Maps handle location (slot) to the containing node.
  static Node* FromLocation(Object** location) {
    ASSERT(OFFSET_OF(Node, object_) == 0);
    return reinterpret_cast<Node*>(location);
  }

  Node() {
    ASSERT(OFFSET_OF(Node, class_id_) == Internals::kNodeClassIdOffset);
    ASSERT(OFFSET_OF(Node, flags_) == Internals::kNodeFlagsOffset);
    STATIC_ASSERT(static_cast<int>(NodeState::kMask) ==
                  Internals::kNodeStateMask);
    STATIC_ASSERT(WEAK == Internals::kNodeStateIsWeakValue);
    STATIC_ASSERT(PENDING == Internals::kNodeStateIsPendingValue);
    STATIC_ASSERT(NEAR_DEATH == Internals::kNodeStateIsNearDeathValue);
    STATIC_ASSERT(static_cast<int>(IsIndependent::kShift) ==
                  Internals::kNodeIsIndependentShift);
    STATIC_ASSERT(static_cast<int>(IsPartiallyDependent::kShift) ==
                  Internals::kNodeIsPartiallyDependentShift);
  }

#ifdef ENABLE_EXTRA_CHECKS
  ~Node() {
    // TODO(1428): if it's a weak handle we should have invoked its callback.
    // Zap the values for eager trapping.
    object_ = reinterpret_cast<Object*>(kGlobalHandleZapValue);
    class_id_ = v8::HeapProfiler::kPersistentHandleNoClassId;
    index_ = 0;
    set_independent(false);
    set_partially_dependent(false);
    set_in_new_space_list(false);
    parameter_or_next_free_.next_free = NULL;
    weak_reference_callback_ = NULL;
  }
#endif

  void Initialize(int index, Node* first_free) {
    index_ = static_cast<uint8_t>(index);
    ASSERT(static_cast<int>(index_) == index);
    set_state(FREE);
    set_in_new_space_list(false);
    parameter_or_next_free_.next_free = first_free;
  }

  void Acquire(Object* object) {
    ASSERT(state() == FREE);
    object_ = object;
    class_id_ = v8::HeapProfiler::kPersistentHandleNoClassId;
    set_independent(false);
    set_partially_dependent(false);
    set_state(NORMAL);
    parameter_or_next_free_.parameter = NULL;
    weak_reference_callback_ = NULL;
  }

  void Release() {
    ASSERT(state() != FREE);
    set_state(FREE);
#ifdef ENABLE_EXTRA_CHECKS
    // Zap the values for eager trapping.
    object_ = reinterpret_cast<Object*>(kGlobalHandleZapValue);
    class_id_ = v8::HeapProfiler::kPersistentHandleNoClassId;
    set_independent(false);
    set_partially_dependent(false);
    weak_reference_callback_ = NULL;
#endif
    ReleaseFromBlock();
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
    return IsPartiallyDependent::decode(flags_);
  }
  void set_partially_dependent(bool v) {
    flags_ = IsPartiallyDependent::update(flags_, v);
  }

  bool is_in_new_space_list() {
    return IsInNewSpaceList::decode(flags_);
  }
  void set_in_new_space_list(bool v) {
    flags_ = IsInNewSpaceList::update(flags_, v);
  }

  bool IsNearDeath() const {
    // Check for PENDING to ensure correct answer when processing callbacks.
    return state() == PENDING || state() == NEAR_DEATH;
  }

  bool IsWeak() const { return state() == WEAK; }

  bool IsRetainer() const { return state() != FREE; }

  bool IsStrongRetainer() const { return state() == NORMAL; }

  bool IsWeakRetainer() const {
    return state() == WEAK || state() == PENDING || state() == NEAR_DEATH;
  }

  void MarkPending() {
    ASSERT(state() == WEAK);
    set_state(PENDING);
  }

  // Independent flag accessors.
  void MarkIndependent() {
    ASSERT(state() != FREE);
    set_independent(true);
  }

  void MarkPartiallyDependent() {
    ASSERT(state() != FREE);
    if (GetGlobalHandles()->isolate()->heap()->InNewSpace(object_)) {
      set_partially_dependent(true);
    }
  }
  void clear_partially_dependent() { set_partially_dependent(false); }

  // Callback parameter accessors.
  void set_parameter(void* parameter) {
    ASSERT(state() != FREE);
    parameter_or_next_free_.parameter = parameter;
  }
  void* parameter() const {
    ASSERT(state() != FREE);
    return parameter_or_next_free_.parameter;
  }

  // Accessors for next free node in the free list.
  Node* next_free() {
    ASSERT(state() == FREE);
    return parameter_or_next_free_.next_free;
  }
  void set_next_free(Node* value) {
    ASSERT(state() == FREE);
    parameter_or_next_free_.next_free = value;
  }

  void MakeWeak(void* parameter,
                RevivableCallback weak_reference_callback) {
    ASSERT(state() != FREE);
    set_state(WEAK);
    set_parameter(parameter);
    weak_reference_callback_ = weak_reference_callback;
  }

  void ClearWeakness() {
    ASSERT(state() != FREE);
    set_state(NORMAL);
    set_parameter(NULL);
  }

  bool PostGarbageCollectionProcessing(Isolate* isolate) {
    if (state() != Node::PENDING) return false;
    if (weak_reference_callback_ == NULL) {
      Release();
      return false;
    }
    void* par = parameter();
    set_state(NEAR_DEATH);
    set_parameter(NULL);

    Object** object = location();
    {
      // Check that we are not passing a finalized external string to
      // the callback.
      ASSERT(!object_->IsExternalAsciiString() ||
             ExternalAsciiString::cast(object_)->resource() != NULL);
      ASSERT(!object_->IsExternalTwoByteString() ||
             ExternalTwoByteString::cast(object_)->resource() != NULL);
      // Leaving V8.
      VMState<EXTERNAL> state(isolate);
      HandleScope handle_scope(isolate);
      weak_reference_callback_(reinterpret_cast<v8::Isolate*>(isolate),
                               reinterpret_cast<Persistent<Value>*>(&object),
                               par);
    }
    // Absence of explicit cleanup or revival of weak handle
    // in most of the cases would lead to memory leak.
    ASSERT(state() != NEAR_DEATH);
    return true;
  }

 private:
  inline NodeBlock* FindBlock();
  inline GlobalHandles* GetGlobalHandles();
  inline void ReleaseFromBlock();

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
  class NodeState:            public BitField<State, 0, 4> {};
  class IsIndependent:        public BitField<bool,  4, 1> {};
  class IsPartiallyDependent: public BitField<bool,  5, 1> {};
  class IsInNewSpaceList:     public BitField<bool,  6, 1> {};

  uint8_t flags_;

  // Handle specific callback - might be a weak reference in disguise.
  RevivableCallback weak_reference_callback_;

  // Provided data for callback.  In FREE state, this is used for
  // the free list link.
  union {
    void* parameter;
    Node* next_free;
  } parameter_or_next_free_;

  DISALLOW_COPY_AND_ASSIGN(Node);
};


class GlobalHandles::BlockListIterator {
 public:
  explicit inline BlockListIterator(BlockList* anchor)
      : anchor_(anchor), current_(anchor->next()) {
    ASSERT(anchor->IsAnchor());
  }
  inline BlockList* block() const {
    ASSERT(!done());
    return current_;
  }
  inline bool done() const {
    ASSERT_EQ(anchor_ == current_, current_->IsAnchor());
    return anchor_ == current_;
  }
  inline void Advance() {
    ASSERT(!done());
    current_ = current_->next();
  }

 private:
  BlockList* const anchor_;
  BlockList* current_;
  DISALLOW_COPY_AND_ASSIGN(BlockListIterator);
};


GlobalHandles::BlockList::BlockList()
    : prev_block_(this),
      next_block_(this),
      first_free_(NULL),
      used_nodes_(0) {}


void GlobalHandles::BlockList::InsertAsNext(BlockList* const block) {
  ASSERT(block != this);
  ASSERT(!block->IsAnchor());
  ASSERT(block->IsDetached());
  block->prev_block_ = this;
  block->next_block_ = next_block_;
  next_block_->prev_block_ = block;
  next_block_ = block;
  ASSERT(!IsDetached());
  ASSERT(!block->IsDetached());
}


void GlobalHandles::BlockList::Detach() {
  ASSERT(!IsAnchor());
  ASSERT(!IsDetached());
  prev_block_->next_block_ = next_block_;
  next_block_->prev_block_ = prev_block_;
  prev_block_ = this;
  next_block_ = this;
  ASSERT(IsDetached());
}


bool GlobalHandles::BlockList::HasAtLeastLength(int length) {
  ASSERT(IsAnchor());
  ASSERT(length > 0);
  for (BlockListIterator it(this); !it.done(); it.Advance()) {
    if (--length <= 0) return true;
  }
  return false;
}


#ifdef DEBUG
int GlobalHandles::BlockList::LengthOfFreeList() {
  int count = 0;
  Node* node = first_free_;
  while (node != NULL) {
    count++;
    node = node->next_free();
  }
  return count;
}
#endif


int GlobalHandles::BlockList::CompareBlocks(const void* a, const void* b) {
  const BlockList* block_a =
      *reinterpret_cast<const BlockList**>(reinterpret_cast<uintptr_t>(a));
  const BlockList* block_b =
      *reinterpret_cast<const BlockList**>(reinterpret_cast<uintptr_t>(b));
  if (block_a->used_nodes() > block_b->used_nodes()) return -1;
  if (block_a->used_nodes() == block_b->used_nodes()) return 0;
  return 1;
}


class GlobalHandles::NodeBlock : public BlockList {
 public:
  static const int kSize = 256;

  explicit NodeBlock(GlobalHandles* global_handles)
      : global_handles_(global_handles) {
    // Initialize nodes
    Node* first_free = first_free_;
    for (int i = kSize - 1; i >= 0; --i) {
      nodes_[i].Initialize(i, first_free);
      first_free = &nodes_[i];
    }
    first_free_ = first_free;
    ASSERT(!IsAnchor());
    // Link into global_handles
    ASSERT(global_handles->non_full_blocks_.IsDetached());
    global_handles->non_full_blocks_.InsertAsHead(this);
    global_handles->number_of_blocks_++;
  }

  Node* Acquire(Object* o) {
    ASSERT(used_nodes_ < kSize);
    ASSERT(first_free_ != NULL);
    ASSERT(global_handles_->non_full_blocks_.next() == this);
    // Remove from free list
    Node* node = first_free_;
    first_free_ = node->next_free();
    // Increment counters
    global_handles_->isolate()->counters()->global_handles()->Increment();
    global_handles_->number_of_global_handles_++;
    // Initialize node with value
    node->Acquire(o);
    bool now_full = ++used_nodes_ == kSize;
    ASSERT_EQ(now_full, first_free_ == NULL);
    if (now_full) {
      // Move block to tail of non_full_blocks_
      Detach();
      global_handles_->full_blocks_.InsertAsTail(this);
    }
    return node;
  }

  void Release(Node* node) {
    ASSERT(used_nodes_ > 0);
    // Add to free list
    node->set_next_free(first_free_);
    first_free_ = node;
    // Decrement counters
    global_handles_->isolate()->counters()->global_handles()->Decrement();
    global_handles_->number_of_global_handles_--;
    bool was_full = used_nodes_-- == kSize;
    ASSERT_EQ(was_full, first_free_->next_free() == NULL);
    if (was_full) {
      // Move this block to head of non_full_blocks_
      Detach();
      global_handles_->non_full_blocks_.InsertAsHead(this);
    }
  }

  Node* node_at(int index) {
    ASSERT(0 <= index && index < kSize);
    return &nodes_[index];
  }

  GlobalHandles* global_handles() { return global_handles_; }

  static NodeBlock* Cast(BlockList* block_list) {
    ASSERT(!block_list->IsAnchor());
    return static_cast<NodeBlock*>(block_list);
  }

  static NodeBlock* From(Node* node, uint8_t index) {
    uintptr_t ptr = reinterpret_cast<uintptr_t>(node - index);
    ptr -= OFFSET_OF(NodeBlock, nodes_);
    NodeBlock* block = reinterpret_cast<NodeBlock*>(ptr);
    ASSERT(block->node_at(index) == node);
    return block;
  }

 private:
  Node nodes_[kSize];
  GlobalHandles* global_handles_;
};


void GlobalHandles::BlockList::SortBlocks(GlobalHandles* global_handles,
                                          bool prune) {
  // Always sort at least 2 blocks
  if (!global_handles->non_full_blocks_.HasAtLeastLength(2)) return;
  // build a vector that could contain the upper bound of the block count
  int number_of_blocks = global_handles->block_count();
  // Build array of blocks and update number_of_blocks to actual count
  ScopedVector<BlockList*> blocks(number_of_blocks);
  {
    int i = 0;
    BlockList* anchor = &global_handles->non_full_blocks_;
    for (BlockListIterator it(anchor); !it.done(); it.Advance()) {
      blocks[i++] = it.block();
    }
    number_of_blocks = i;
  }
  // Nothing to do.
  if (number_of_blocks <= 1) return;
  // Sort blocks
  qsort(blocks.start(), number_of_blocks, sizeof(blocks[0]), CompareBlocks);
  // Prune empties
  if (prune) {
    static const double kUnusedPercentage = 0.30;
    static const double kUsedPercentage = 1.30;
    int total_slots = global_handles->number_of_blocks_ * NodeBlock::kSize;
    const int total_used = global_handles->number_of_global_handles_;
    const int target_unused = static_cast<int>(Max(
        total_used * kUsedPercentage,
        total_slots * kUnusedPercentage));
    // Reverse through empty blocks. Note: always leave one block free.
    int blocks_deleted = 0;
    for (int i = number_of_blocks - 1; i > 0 && blocks[i]->IsUnused(); i--) {
      // Not worth deleting
      if (total_slots - total_used < target_unused) break;
      blocks[i]->Detach();
      delete blocks[i];
      blocks_deleted++;
      total_slots -= NodeBlock::kSize;
    }
    global_handles->number_of_blocks_ -= blocks_deleted;
    number_of_blocks -= blocks_deleted;
  }
  // Relink all blocks
  for (int i = 0; i < number_of_blocks; i++) {
    blocks[i]->Detach();
    global_handles->non_full_blocks_.InsertAsTail(blocks[i]);
  }
#ifdef DEBUG
  // Check sorting
  BlockList* anchor = &global_handles->non_full_blocks_;
  int last_size = NodeBlock::kSize;
  for (BlockListIterator it(anchor); !it.done(); it.Advance()) {
    ASSERT(it.block()->used_nodes() <= last_size);
    last_size = it.block()->used_nodes();
  }
#endif
}


#ifdef DEBUG
void GlobalHandles::VerifyBlockInvariants() {
  int number_of_blocks = 0;
  int number_of_handles = 0;
  for (int i = 0; i < kAllAnchorsSize; i++) {
    for (BlockListIterator it(all_anchors_[i]); !it.done(); it.Advance()) {
      BlockList* block = it.block();
      number_of_blocks++;
      int used_nodes = block->used_nodes();
      number_of_handles += used_nodes;
      int unused_nodes = block->LengthOfFreeList();
      ASSERT_EQ(used_nodes + unused_nodes, NodeBlock::kSize);
      if (all_anchors_[i] == &full_blocks_) {
        ASSERT_EQ(NodeBlock::kSize, used_nodes);
      } else {
        ASSERT_NE(NodeBlock::kSize, used_nodes);
      }
    }
  }
  ASSERT_EQ(number_of_handles, number_of_global_handles_);
  ASSERT_EQ(number_of_blocks, number_of_blocks_);
}
#endif


void GlobalHandles::SortBlocks(bool shouldPrune) {
#ifdef DEBUG
  VerifyBlockInvariants();
#endif
  BlockList::SortBlocks(this, shouldPrune);
#ifdef DEBUG
  VerifyBlockInvariants();
#endif
}


GlobalHandles* GlobalHandles::Node::GetGlobalHandles() {
  return FindBlock()->global_handles();
}


GlobalHandles::NodeBlock* GlobalHandles::Node::FindBlock() {
  return NodeBlock::From(this, index_);
}


void GlobalHandles::Node::ReleaseFromBlock() {
  FindBlock()->Release(this);
}


class GlobalHandles::NodeIterator {
 public:
  explicit NodeIterator(GlobalHandles* global_handles)
      : all_anchors_(global_handles->all_anchors_),
        block_(all_anchors_[0]),
        anchor_index_(0),
        node_index_(0) {
    AdvanceBlock();
  }

  bool done() const {
    return anchor_index_ == kAllAnchorsSize;
  }

  Node* node() const {
    ASSERT(!done());
    return NodeBlock::Cast(block_)->node_at(node_index_);
  }

  void Advance() {
    ASSERT(!done());
    if (++node_index_ < NodeBlock::kSize) return;
    node_index_ = 0;
    AdvanceBlock();
  }

  typedef int CountArray[Node::NUMBER_OF_STATES];
  static int CollectStats(GlobalHandles* global_handles, CountArray counts);

 private:
  void AdvanceBlock() {
    ASSERT(!done());
    while (true) {
      block_ = block_->next();
      // block is valid
      if (block_ != all_anchors_[anchor_index_]) {
        ASSERT(!done());
        ASSERT(!block_->IsAnchor());
        // skip empty blocks
        if (block_->IsUnused()) continue;
        return;
      }
      // jump lists
      anchor_index_++;
      if (anchor_index_ == kAllAnchorsSize) break;
      block_ = all_anchors_[anchor_index_];
    }
    ASSERT(done());
  }

  BlockList* const * const all_anchors_;
  BlockList* block_;
  int anchor_index_;
  int node_index_;

  DISALLOW_COPY_AND_ASSIGN(NodeIterator);
};


int GlobalHandles::NodeIterator::CollectStats(GlobalHandles* global_handles,
                                              CountArray counts) {
  static const int kSize = Node::NUMBER_OF_STATES;
  for (int i = 0; i < kSize; i++) {
    counts[i] = 0;
  }
  int total = 0;
  for (NodeIterator it(global_handles); !it.done(); it.Advance()) {
    total++;
    Node::State state = it.node()->state();
    ASSERT(state >= 0 && state < kSize);
    counts[state]++;
  }
  // NodeIterator skips empty blocks
  int skipped = global_handles->number_of_blocks_ * NodeBlock::kSize - total;
  total += skipped;
  counts[Node::FREE] += total;
  return total;
}


GlobalHandles::GlobalHandles(Isolate* isolate)
    : isolate_(isolate),
      number_of_blocks_(0),
      number_of_global_handles_(0),
      post_gc_processing_count_(0),
      object_group_connections_(kObjectGroupConnectionsCapacity) {
  all_anchors_[0] = &full_blocks_;
  all_anchors_[1] = &non_full_blocks_;
}


GlobalHandles::~GlobalHandles() {
  for (int i = 0; i < kAllAnchorsSize; i++) {
    BlockList* block = all_anchors_[i]->next();
    while (block != all_anchors_[i]) {
      BlockList* tmp = block->next();
      block->Detach();
      delete NodeBlock::Cast(block);
      block = tmp;
    }
  }
}


Handle<Object> GlobalHandles::Create(Object* value) {
  if (non_full_blocks_.IsDetached()) {
    new NodeBlock(this);
    ASSERT(!non_full_blocks_.IsDetached());
  }
  ASSERT(non_full_blocks_.IsAnchor());
  ASSERT(!non_full_blocks_.next()->IsAnchor());
  Node* result = NodeBlock::Cast(non_full_blocks_.next())->Acquire(value);
  if (isolate_->heap()->InNewSpace(value) &&
      !result->is_in_new_space_list()) {
    new_space_nodes_.Add(result);
    result->set_in_new_space_list(true);
  }
  return result->handle();
}


void GlobalHandles::Destroy(Object** location) {
  if (location != NULL) Node::FromLocation(location)->Release();
}


void GlobalHandles::MakeWeak(Object** location,
                             void* parameter,
                             RevivableCallback weak_reference_callback) {
  ASSERT(weak_reference_callback != NULL);
  Node::FromLocation(location)->MakeWeak(parameter, weak_reference_callback);
}


void GlobalHandles::ClearWeakness(Object** location) {
  Node::FromLocation(location)->ClearWeakness();
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
    if (it.node()->IsWeakRetainer()) v->VisitPointer(it.node()->location());
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
    if (node->IsStrongRetainer() ||
        (node->IsWeakRetainer() && !node->is_independent() &&
         !node->is_partially_dependent())) {
        v->VisitPointer(node->location());
    }
  }
}


void GlobalHandles::IdentifyNewSpaceWeakIndependentHandles(
    WeakSlotCallbackWithHeap f) {
  for (int i = 0; i < new_space_nodes_.length(); ++i) {
    Node* node = new_space_nodes_[i];
    ASSERT(node->is_in_new_space_list());
    if ((node->is_independent() || node->is_partially_dependent()) &&
        node->IsWeak() && f(isolate_->heap(), node->location())) {
      node->MarkPending();
    }
  }
}


void GlobalHandles::IterateNewSpaceWeakIndependentRoots(ObjectVisitor* v) {
  for (int i = 0; i < new_space_nodes_.length(); ++i) {
    Node* node = new_space_nodes_[i];
    ASSERT(node->is_in_new_space_list());
    if ((node->is_independent() || node->is_partially_dependent()) &&
        node->IsWeakRetainer()) {
      v->VisitPointer(node->location());
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
    ASSERT(entry != NULL);

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


bool GlobalHandles::PostGarbageCollectionProcessing(
    GarbageCollector collector, GCTracer* tracer) {
  // Process weak global handle callbacks. This must be done after the
  // GC is completely done, because the callbacks may invoke arbitrary
  // API functions.
  ASSERT(isolate_->heap()->gc_state() == Heap::NOT_IN_GC);
  const int initial_post_gc_processing_count = ++post_gc_processing_count_;
  bool next_gc_likely_to_collect_more = false;
  if (collector == SCAVENGER) {
    for (int i = 0; i < new_space_nodes_.length(); ++i) {
      Node* node = new_space_nodes_[i];
      ASSERT(node->is_in_new_space_list());
      if (!node->IsRetainer()) {
        // Free nodes do not have weak callbacks. Do not use them to compute
        // the next_gc_likely_to_collect_more.
        continue;
      }
      // Skip dependent handles. Their weak callbacks might expect to be
      // called between two global garbage collection callbacks which
      // are not called for minor collections.
      if (!node->is_independent() && !node->is_partially_dependent()) {
        continue;
      }
      node->clear_partially_dependent();
      if (node->PostGarbageCollectionProcessing(isolate_)) {
        if (initial_post_gc_processing_count != post_gc_processing_count_) {
          // Weak callback triggered another GC and another round of
          // PostGarbageCollection processing.  The current node might
          // have been deleted in that round, so we need to bail out (or
          // restart the processing).
          return next_gc_likely_to_collect_more;
        }
      }
      if (!node->IsRetainer()) {
        next_gc_likely_to_collect_more = true;
      }
    }
  } else {
    // Must cache all blocks, as NodeIterator can't survive mutation.
    List<NodeBlock*> blocks(number_of_blocks_);
    for (int i = 0; i < kAllAnchorsSize; i++) {
      for (BlockListIterator it(all_anchors_[i]); !it.done(); it.Advance()) {
        blocks.Add(NodeBlock::Cast(it.block()));
      }
    }
    for (int block_index = 0; block_index < blocks.length(); block_index++) {
      NodeBlock* block = blocks[block_index];
      for (int node_index = 0; node_index < NodeBlock::kSize; node_index++) {
        Node* node = block->node_at(node_index);
        if (!node->IsRetainer()) {
          // Free nodes do not have weak callbacks. Do not use them to compute
          // the next_gc_likely_to_collect_more.
          continue;
        }
        node->clear_partially_dependent();
        if (node->PostGarbageCollectionProcessing(isolate_)) {
          if (initial_post_gc_processing_count != post_gc_processing_count_) {
            // See the comment above.
            return next_gc_likely_to_collect_more;
          }
        }
        if (!node->IsRetainer()) {
          next_gc_likely_to_collect_more = true;
        }
      }
    }
  }
  // Update the list of new space nodes.
  int last = 0;
  for (int i = 0; i < new_space_nodes_.length(); ++i) {
    Node* node = new_space_nodes_[i];
    ASSERT(node->is_in_new_space_list());
    if (node->IsRetainer()) {
      if (isolate_->heap()->InNewSpace(node->object())) {
        new_space_nodes_[last++] = node;
        tracer->increment_nodes_copied_in_new_space();
      } else {
        node->set_in_new_space_list(false);
        tracer->increment_nodes_promoted();
      }
    } else {
      node->set_in_new_space_list(false);
      tracer->increment_nodes_died_in_new_space();
    }
  }
  new_space_nodes_.Rewind(last);
  bool shouldPruneBlocks = collector != SCAVENGER;
  SortBlocks(shouldPruneBlocks);
  return next_gc_likely_to_collect_more;
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
  NodeIterator::CountArray counts;
  int total = NodeIterator::CollectStats(this, counts);
  *stats->global_handle_count = total;
  *stats->weak_global_handle_count = counts[Node::WEAK];
  *stats->pending_global_handle_count = counts[Node::PENDING];
  *stats->near_death_global_handle_count = counts[Node::NEAR_DEATH];
  *stats->free_global_handle_count = counts[Node::FREE];
}


#ifdef DEBUG

void GlobalHandles::PrintStats() {
  NodeIterator::CountArray counts;
  int total = NodeIterator::CollectStats(this, counts);
  size_t total_consumed = sizeof(NodeBlock) * number_of_blocks_;
  PrintF("Global Handle Statistics:\n");
  PrintF("  allocated blocks = %d\n", number_of_blocks_);
  PrintF("  allocated memory = %" V8_PTR_PREFIX "dB\n", total_consumed);
  PrintF("  # normal     = %d\n", counts[Node::NORMAL]);
  PrintF("  # weak       = %d\n", counts[Node::WEAK]);
  PrintF("  # pending    = %d\n", counts[Node::PENDING]);
  PrintF("  # near_death = %d\n", counts[Node::NEAR_DEATH]);
  PrintF("  # free       = %d\n", counts[Node::FREE]);
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
    ASSERT(!Node::FromLocation(handles[i])->is_independent());
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


void GlobalHandles::AddImplicitReferences(HeapObject** parent,
                                          Object*** children,
                                          size_t length) {
#ifdef DEBUG
  ASSERT(!Node::FromLocation(BitCast<Object**>(parent))->is_independent());
  for (size_t i = 0; i < length; ++i) {
    ASSERT(!Node::FromLocation(children[i])->is_independent());
  }
#endif
  if (length == 0) return;
  ImplicitRefGroup* group = new ImplicitRefGroup(parent, length);
  for (size_t i = 0; i < length; ++i)
    group->children[i] = children[i];
  implicit_ref_groups_.Add(group);
}


void GlobalHandles::SetReferenceFromGroup(UniqueId id, Object** child) {
  ASSERT(!Node::FromLocation(child)->is_independent());
  implicit_ref_connections_.Add(ObjectGroupConnection(id, child));
}


void GlobalHandles::SetReference(HeapObject** parent, Object** child) {
  ASSERT(!Node::FromLocation(child)->is_independent());
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
  STATIC_ASSERT(v8::kUninitializedEternalIndex == kInvalidIndex);
  for (unsigned i = 0; i < ARRAY_SIZE(singleton_handles_); i++) {
    singleton_handles_[i] = kInvalidIndex;
  }
}


EternalHandles::~EternalHandles() {
  for (int i = 0; i < blocks_.length(); i++) delete[] blocks_[i];
}


void EternalHandles::IterateAllRoots(ObjectVisitor* visitor) {
  int limit = size_;
  for (int i = 0; i < blocks_.length(); i++) {
    ASSERT(limit > 0);
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


int EternalHandles::Create(Isolate* isolate, Object* object) {
  if (object == NULL) return kInvalidIndex;
  ASSERT_NE(isolate->heap()->the_hole_value(), object);
  int block = size_ >> kShift;
  int offset = size_ & kMask;
  // need to resize
  if (offset == 0) {
    Object** next_block = new Object*[kSize];
    Object* the_hole = isolate->heap()->the_hole_value();
    MemsetPointer(next_block, the_hole, kSize);
    blocks_.Add(next_block);
  }
  ASSERT_EQ(isolate->heap()->the_hole_value(), blocks_[block][offset]);
  blocks_[block][offset] = object;
  if (isolate->heap()->InNewSpace(object)) {
    new_space_indices_.Add(size_);
  }
  return size_++;
}


} }  // namespace v8::internal
