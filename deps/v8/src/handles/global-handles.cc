// Copyright 2009 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/handles/global-handles.h"

#include "src/api/api-inl.h"
#include "src/base/compiler-specific.h"
#include "src/execution/vm-state-inl.h"
#include "src/heap/embedder-tracing.h"
#include "src/heap/heap-write-barrier-inl.h"
#include "src/init/v8.h"
#include "src/logging/counters.h"
#include "src/objects/objects-inl.h"
#include "src/objects/slots.h"
#include "src/objects/visitors.h"
#include "src/tasks/cancelable-task.h"
#include "src/tasks/task-utils.h"

namespace v8 {
namespace internal {

namespace {

// Specifies whether V8 expects the holder memory of a global handle to be live
// or dead.
enum class HandleHolder { kLive, kDead };

constexpr size_t kBlockSize = 256;

}  // namespace

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
const GlobalHandles::NodeBlock<NodeType>*
GlobalHandles::NodeBlock<NodeType>::From(const NodeType* node) {
  uintptr_t ptr = reinterpret_cast<const uintptr_t>(node) -
                  sizeof(NodeType) * node->index();
  const BlockType* block = reinterpret_cast<const BlockType*>(ptr);
  DCHECK_EQ(node, block->at(node->index()));
  return block;
}

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

  void Acquire(Object object) {
    DCHECK(!AsChild()->IsInUse());
    CheckFieldsAreCleared();
    object_ = object.ptr();
    AsChild()->MarkAsUsed();
    data_.parameter = nullptr;
    DCHECK(AsChild()->IsInUse());
  }

  void Release(Child* free_list) {
    DCHECK(AsChild()->IsInUse());
    Free(free_list);
    DCHECK(!AsChild()->IsInUse());
  }

  Object object() const { return Object(object_); }
  FullObjectSlot location() { return FullObjectSlot(&object_); }
  Handle<Object> handle() { return Handle<Object>(&object_); }

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

 protected:
  Child* AsChild() { return reinterpret_cast<Child*>(this); }
  const Child* AsChild() const { return reinterpret_cast<const Child*>(this); }

  void ClearFields() {
    // Zap the values for eager trapping.
    object_ = kGlobalHandleZapValue;
    class_id_ = v8::HeapProfiler::kPersistentHandleNoClassId;
    AsChild()->ClearImplFields();
  }

  void CheckFieldsAreCleared() {
    DCHECK_EQ(kGlobalHandleZapValue, object_);
    DCHECK_EQ(v8::HeapProfiler::kPersistentHandleNoClassId, class_id_);
    AsChild()->CheckImplFieldsAreCleared();
  }

  // Storage for object pointer.
  //
  // Placed first to avoid offset computation. The stored data is equivalent to
  // an Object. It is stored as a plain Address for convenience (smallest number
  // of casts), and because it is a private implementation detail: the public
  // interface provides type safety.
  Address object_;

  // Class id set by the embedder.
  uint16_t class_id_;

  // Index in the containing handle block.
  uint8_t index_;

  uint8_t flags_;

  // The meaning of this field depends on node state:
  // - Node in free list: Stores next free node pointer.
  // - Otherwise, specific to the node implementation.
  union {
    Child* next_free;
    void* parameter;
  } data_;
};

namespace {

void ExtractInternalFields(JSObject jsobject, void** embedder_fields, int len) {
  int field_count = jsobject.GetEmbedderFieldCount();
  for (int i = 0; i < len; ++i) {
    if (field_count == i) break;
    void* pointer;
    if (EmbedderDataSlot(jsobject, i).ToAlignedPointer(&pointer)) {
      embedder_fields[i] = pointer;
    }
  }
}

}  // namespace

class GlobalHandles::Node final : public NodeBase<GlobalHandles::Node> {
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

  Node() {
    STATIC_ASSERT(static_cast<int>(NodeState::kMask) ==
                  Internals::kNodeStateMask);
    STATIC_ASSERT(WEAK == Internals::kNodeStateIsWeakValue);
    STATIC_ASSERT(PENDING == Internals::kNodeStateIsPendingValue);
    set_in_young_list(false);
  }

  void Zap() {
    DCHECK(IsInUse());
    // Zap the values for eager trapping.
    object_ = kGlobalHandleZapValue;
  }

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

  bool IsPhantomCallback() const {
    return weakness_type() == PHANTOM_WEAK ||
           weakness_type() == PHANTOM_WEAK_2_EMBEDDER_FIELDS;
  }

  bool IsPhantomResetHandle() const {
    return weakness_type() == PHANTOM_WEAK_RESET_HANDLE;
  }

  bool IsFinalizerHandle() const { return weakness_type() == FINALIZER_WEAK; }

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
    data_.parameter = const_cast<char*>(label);
  }

  void CollectPhantomCallbackData(
      std::vector<std::pair<Node*, PendingPhantomCallback>>*
          pending_phantom_callbacks) {
    DCHECK(weakness_type() == PHANTOM_WEAK ||
           weakness_type() == PHANTOM_WEAK_2_EMBEDDER_FIELDS);
    DCHECK(state() == PENDING);
    DCHECK_NOT_NULL(weak_callback_);

    void* embedder_fields[v8::kEmbedderFieldsInWeakCallback] = {nullptr,
                                                                nullptr};
    if (weakness_type() != PHANTOM_WEAK && object().IsJSObject()) {
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

  void ResetPhantomHandle(HandleHolder handle_holder) {
    DCHECK_EQ(HandleHolder::kLive, handle_holder);
    DCHECK_EQ(PHANTOM_WEAK_RESET_HANDLE, weakness_type());
    DCHECK_EQ(PENDING, state());
    DCHECK_NULL(weak_callback_);
    Address** handle = reinterpret_cast<Address**>(parameter());
    *handle = nullptr;
    NodeSpace<Node>::Release(this);
  }

  void PostGarbageCollectionProcessing(Isolate* isolate) {
    // This method invokes a finalizer. Updating the method name would require
    // adjusting CFI blacklist as weak_callback_ is invoked on the wrong type.
    CHECK(IsPendingFinalizer());
    set_state(NEAR_DEATH);
    // Check that we are not passing a finalized external string to
    // the callback.
    DCHECK(!object().IsExternalOneByteString() ||
           ExternalOneByteString::cast(object()).resource() != nullptr);
    DCHECK(!object().IsExternalTwoByteString() ||
           ExternalTwoByteString::cast(object()).resource() != nullptr);
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

  void MarkAsFree() { set_state(FREE); }
  void MarkAsUsed() { set_state(NORMAL); }

  GlobalHandles* global_handles() {
    return NodeBlock<Node>::From(this)->global_handles();
  }

 private:
  // Fields that are not used for managing node memory.
  void ClearImplFields() { weak_callback_ = nullptr; }

  void CheckImplFieldsAreCleared() { DCHECK_EQ(nullptr, weak_callback_); }

  // This stores three flags (independent, partially_dependent and
  // in_young_list) and a State.
  using NodeState = BitField8<State, 0, 3>;
  using IsInYoungList = NodeState::Next<bool, 1>;
  using NodeWeaknessType = IsInYoungList::Next<WeaknessType, 2>;

  // Handle specific callback - might be a weak reference in disguise.
  WeakCallbackInfo<void>::Callback weak_callback_;

  friend class NodeBase<Node>;

  DISALLOW_COPY_AND_ASSIGN(Node);
};

class GlobalHandles::TracedNode final
    : public NodeBase<GlobalHandles::TracedNode> {
 public:
  TracedNode() { set_in_young_list(false); }

  enum State { FREE = 0, NORMAL, NEAR_DEATH };

  State state() const { return NodeState::decode(flags_); }
  void set_state(State state) { flags_ = NodeState::update(flags_, state); }

  void MarkAsFree() { set_state(FREE); }
  void MarkAsUsed() { set_state(NORMAL); }
  bool IsInUse() const { return state() != FREE; }
  bool IsRetainer() const { return state() == NORMAL; }
  bool IsPhantomResetHandle() const { return callback_ == nullptr; }

  bool is_in_young_list() const { return IsInYoungList::decode(flags_); }
  void set_in_young_list(bool v) { flags_ = IsInYoungList::update(flags_, v); }

  bool is_root() const { return IsRoot::decode(flags_); }
  void set_root(bool v) { flags_ = IsRoot::update(flags_, v); }

  bool has_destructor() const { return HasDestructor::decode(flags_); }
  void set_has_destructor(bool v) { flags_ = HasDestructor::update(flags_, v); }

  void SetFinalizationCallback(void* parameter,
                               WeakCallbackInfo<void>::Callback callback) {
    set_parameter(parameter);
    callback_ = callback;
  }
  bool HasFinalizationCallback() const { return callback_ != nullptr; }

  void CollectPhantomCallbackData(
      std::vector<std::pair<TracedNode*, PendingPhantomCallback>>*
          pending_phantom_callbacks) {
    DCHECK(IsInUse());
    DCHECK_NOT_NULL(callback_);

    void* embedder_fields[v8::kEmbedderFieldsInWeakCallback] = {nullptr,
                                                                nullptr};
    ExtractInternalFields(JSObject::cast(object()), embedder_fields,
                          v8::kEmbedderFieldsInWeakCallback);

    // Zap with something dangerous.
    location().store(Object(0xCA11));

    pending_phantom_callbacks->push_back(std::make_pair(
        this, PendingPhantomCallback(callback_, parameter(), embedder_fields)));
    set_state(NEAR_DEATH);
  }

  void ResetPhantomHandle(HandleHolder handle_holder) {
    DCHECK(IsInUse());
    if (handle_holder == HandleHolder::kLive) {
      Address** handle = reinterpret_cast<Address**>(data_.parameter);
      *handle = nullptr;
    }
    NodeSpace<TracedNode>::Release(this);
    DCHECK(!IsInUse());
  }

 protected:
  using NodeState = BitField8<State, 0, 2>;
  using IsInYoungList = NodeState::Next<bool, 1>;
  using IsRoot = IsInYoungList::Next<bool, 1>;
  using HasDestructor = IsRoot::Next<bool, 1>;

  void ClearImplFields() {
    set_root(true);
    callback_ = nullptr;
  }

  void CheckImplFieldsAreCleared() const {
    DCHECK(is_root());
    DCHECK_NULL(callback_);
  }

  WeakCallbackInfo<void>::Callback callback_;

  friend class NodeBase<GlobalHandles::TracedNode>;

  DISALLOW_COPY_AND_ASSIGN(TracedNode);
};

GlobalHandles::GlobalHandles(Isolate* isolate)
    : isolate_(isolate),
      regular_nodes_(new NodeSpace<GlobalHandles::Node>(this)),
      traced_nodes_(new NodeSpace<GlobalHandles::TracedNode>(this)) {}

GlobalHandles::~GlobalHandles() { regular_nodes_.reset(nullptr); }

Handle<Object> GlobalHandles::Create(Object value) {
  GlobalHandles::Node* result = regular_nodes_->Acquire(value);
  if (ObjectInYoungGeneration(value) && !result->is_in_young_list()) {
    young_nodes_.push_back(result);
    result->set_in_young_list(true);
  }
  return result->handle();
}

Handle<Object> GlobalHandles::Create(Address value) {
  return Create(Object(value));
}

Handle<Object> GlobalHandles::CreateTraced(Object value, Address* slot,
                                           bool has_destructor) {
  GlobalHandles::TracedNode* result = traced_nodes_->Acquire(value);
  if (ObjectInYoungGeneration(value) && !result->is_in_young_list()) {
    traced_young_nodes_.push_back(result);
    result->set_in_young_list(true);
  }
  result->set_parameter(slot);
  result->set_has_destructor(has_destructor);
  return result->handle();
}

Handle<Object> GlobalHandles::CreateTraced(Address value, Address* slot,
                                           bool has_destructor) {
  return CreateTraced(Object(value), slot, has_destructor);
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

// static
void GlobalHandles::CopyTracedGlobal(const Address* const* from, Address** to) {
  DCHECK_NOT_NULL(*from);
  DCHECK_NULL(*to);
  const TracedNode* node = TracedNode::FromLocation(*from);
  // Copying a traced handle with finalization callback is prohibited because
  // the callback may require knowing about multiple copies of the traced
  // handle.
  CHECK(!node->HasFinalizationCallback());
  GlobalHandles* global_handles =
      NodeBlock<TracedNode>::From(node)->global_handles();
  Handle<Object> o = global_handles->CreateTraced(
      node->object(), reinterpret_cast<Address*>(to), node->has_destructor());
  *to = o.location();
#ifdef VERIFY_HEAP
  if (i::FLAG_verify_heap) {
    Object(**to).ObjectVerify(global_handles->isolate());
  }
#endif  // VERIFY_HEAP
}

void GlobalHandles::MoveGlobal(Address** from, Address** to) {
  DCHECK_NOT_NULL(*from);
  DCHECK_NOT_NULL(*to);
  DCHECK_EQ(*from, *to);
  Node* node = Node::FromLocation(*from);
  if (node->IsWeak() && node->IsPhantomResetHandle()) {
    node->set_parameter(to);
  }

  // - Strong handles do not require fixups.
  // - Weak handles with finalizers and callbacks are too general to fix up. For
  //   those the callers need to ensure consistency.
}

void GlobalHandles::MoveTracedGlobal(Address** from, Address** to) {
  DCHECK_NOT_NULL(*from);
  DCHECK_NOT_NULL(*to);
  DCHECK_EQ(*from, *to);
  TracedNode* node = TracedNode::FromLocation(*from);
  // Only set the backpointer for clearing a phantom handle when there is no
  // finalization callback attached. As soon as a callback is attached to a node
  // the embedder is on its own when resetting a handle.
  if (!node->HasFinalizationCallback()) {
    node->set_parameter(to);
  }
}

void GlobalHandles::Destroy(Address* location) {
  if (location != nullptr) {
    NodeSpace<Node>::Release(Node::FromLocation(location));
  }
}

void GlobalHandles::DestroyTraced(Address* location) {
  if (location != nullptr) {
    NodeSpace<TracedNode>::Release(TracedNode::FromLocation(location));
  }
}

void GlobalHandles::SetFinalizationCallbackForTraced(
    Address* location, void* parameter,
    WeakCallbackInfo<void>::Callback callback) {
  TracedNode::FromLocation(location)->SetFinalizationCallback(parameter,
                                                              callback);
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
        node->ResetPhantomHandle(HandleHolder::kLive);
        ++number_of_phantom_handle_resets_;
      } else if (node->IsPhantomCallback()) {
        node->MarkPending();
        node->CollectPhantomCallbackData(&regular_pending_phantom_callbacks_);
      }
    }
  }
  for (TracedNode* node : *traced_nodes_) {
    if (node->IsInUse() &&
        should_reset_handle(isolate()->heap(), node->location())) {
      if (node->IsPhantomResetHandle()) {
        node->ResetPhantomHandle(node->has_destructor() ? HandleHolder::kLive
                                                        : HandleHolder::kDead);
        ++number_of_phantom_handle_resets_;
      } else {
        node->CollectPhantomCallbackData(&traced_pending_phantom_callbacks_);
      }
    }
  }
}

void GlobalHandles::IterateWeakRootsIdentifyFinalizers(
    WeakSlotCallbackWithHeap should_reset_handle) {
  for (Node* node : *regular_nodes_) {
    if (node->IsWeak() &&
        should_reset_handle(isolate()->heap(), node->location())) {
      if (node->IsFinalizerHandle()) {
        node->MarkPending();
      }
    }
  }
}

void GlobalHandles::IdentifyWeakUnmodifiedObjects(
    WeakSlotCallback is_unmodified) {
  LocalEmbedderHeapTracer* const tracer =
      isolate()->heap()->local_embedder_heap_tracer();
  for (TracedNode* node : traced_young_nodes_) {
    if (node->IsInUse()) {
      DCHECK(node->is_root());
      if (is_unmodified(node->location())) {
        v8::Value* value = ToApi<v8::Value>(node->handle());
        node->set_root(tracer->IsRootForNonTracingGC(
            *reinterpret_cast<v8::TracedGlobal<v8::Value>*>(&value)));
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

void GlobalHandles::MarkYoungWeakUnmodifiedObjectsPending(
    WeakSlotCallbackWithHeap is_dead) {
  for (Node* node : young_nodes_) {
    DCHECK(node->is_in_young_list());
    if (node->IsWeak() && is_dead(isolate_->heap(), node->location())) {
      if (!node->IsPhantomCallback() && !node->IsPhantomResetHandle()) {
        node->MarkPending();
      }
    }
  }
}

void GlobalHandles::IterateYoungWeakUnmodifiedRootsForFinalizers(
    RootVisitor* v) {
  for (Node* node : young_nodes_) {
    DCHECK(node->is_in_young_list());
    if (node->IsWeakRetainer() && (node->state() == Node::PENDING)) {
      DCHECK(!node->IsPhantomCallback());
      DCHECK(!node->IsPhantomResetHandle());
      // Finalizers need to survive.
      v->VisitRootPointer(Root::kGlobalHandles, node->label(),
                          node->location());
    }
  }
}

void GlobalHandles::IterateYoungWeakUnmodifiedRootsForPhantomHandles(
    RootVisitor* v, WeakSlotCallbackWithHeap should_reset_handle) {
  for (Node* node : young_nodes_) {
    DCHECK(node->is_in_young_list());
    if (node->IsWeakRetainer() && (node->state() != Node::PENDING)) {
      if (should_reset_handle(isolate_->heap(), node->location())) {
        DCHECK(node->IsPhantomResetHandle() || node->IsPhantomCallback());
        if (node->IsPhantomResetHandle()) {
          node->MarkPending();
          node->ResetPhantomHandle(HandleHolder::kLive);
          ++number_of_phantom_handle_resets_;
        } else if (node->IsPhantomCallback()) {
          node->MarkPending();
          node->CollectPhantomCallbackData(&regular_pending_phantom_callbacks_);
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

  LocalEmbedderHeapTracer* const tracer =
      isolate()->heap()->local_embedder_heap_tracer();
  for (TracedNode* node : traced_young_nodes_) {
    if (!node->IsInUse()) continue;

    DCHECK_IMPLIES(node->is_root(),
                   !should_reset_handle(isolate_->heap(), node->location()));
    if (should_reset_handle(isolate_->heap(), node->location())) {
      if (node->IsPhantomResetHandle()) {
        if (node->has_destructor()) {
          // For handles with destructor it is guaranteed that the embedder
          // memory is still alive as the destructor would have otherwise
          // removed the memory.
          node->ResetPhantomHandle(HandleHolder::kLive);
        } else {
          v8::Value* value = ToApi<v8::Value>(node->handle());
          tracer->ResetHandleInNonTracingGC(
              *reinterpret_cast<v8::TracedGlobal<v8::Value>*>(&value));
          DCHECK(!node->IsInUse());
        }

        ++number_of_phantom_handle_resets_;
      } else {
        node->CollectPhantomCallbackData(&traced_pending_phantom_callbacks_);
      }
    } else {
      if (!node->is_root()) {
        node->set_root(true);
        v->VisitRootPointer(Root::kGlobalHandles, nullptr, node->location());
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
  // The callbacks may execute JS, which in turn may lead to another GC run.
  // If we are already processing the callbacks, we do not want to start over
  // from within the inner GC. Newly added callbacks will always be run by the
  // outermost GC run only.
  if (running_second_pass_callbacks_) return;
  running_second_pass_callbacks_ = true;

  AllowJavascriptExecution allow_js(isolate());
  while (!second_pass_callbacks_.empty()) {
    auto callback = second_pass_callbacks_.back();
    second_pass_callbacks_.pop_back();
    callback.Invoke(isolate(), PendingPhantomCallback::kSecondPass);
  }
  running_second_pass_callbacks_ = false;
}

size_t GlobalHandles::PostScavengeProcessing(unsigned post_processing_count) {
  size_t freed_nodes = 0;
  for (Node* node : young_nodes_) {
    // Filter free nodes.
    if (!node->IsRetainer()) continue;

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

template <typename T>
void GlobalHandles::UpdateAndCompactListOfYoungNode(
    std::vector<T*>* node_list) {
  size_t last = 0;
  for (T* node : *node_list) {
    DCHECK(node->is_in_young_list());
    if (node->IsInUse()) {
      if (ObjectInYoungGeneration(node->object())) {
        (*node_list)[last++] = node;
        isolate_->heap()->IncrementNodesCopiedInNewSpace();
      } else {
        node->set_in_young_list(false);
        isolate_->heap()->IncrementNodesPromoted();
      }
    } else {
      node->set_in_young_list(false);
      isolate_->heap()->IncrementNodesDiedInNewSpace();
    }
  }
  DCHECK_LE(last, node_list->size());
  node_list->resize(last);
  node_list->shrink_to_fit();
}

void GlobalHandles::UpdateListOfYoungNodes() {
  UpdateAndCompactListOfYoungNode(&young_nodes_);
  UpdateAndCompactListOfYoungNode(&traced_young_nodes_);
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
  return InvokeFirstPassWeakCallbacks(&regular_pending_phantom_callbacks_) +
         InvokeFirstPassWeakCallbacks(&traced_pending_phantom_callbacks_);
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

  UpdateListOfYoungNodes();
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
  for (TracedNode* node : *traced_nodes_) {
    if (node->IsInUse()) {
      v->VisitRootPointer(Root::kGlobalHandles, nullptr, node->location());
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
  for (TracedNode* node : *traced_nodes_) {
    if (node->IsRetainer()) {
      v->VisitRootPointer(Root::kGlobalHandles, nullptr, node->location());
    }
  }
}

DISABLE_CFI_PERF
void GlobalHandles::IterateAllYoungRoots(RootVisitor* v) {
  for (Node* node : young_nodes_) {
    if (node->IsRetainer()) {
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
void GlobalHandles::IterateTracedNodes(
    v8::EmbedderHeapTracer::TracedGlobalHandleVisitor* visitor) {
  for (TracedNode* node : *traced_nodes_) {
    if (node->IsInUse()) {
      v8::Value* value = ToApi<v8::Value>(node->handle());
      visitor->VisitTracedGlobalHandle(
          *reinterpret_cast<v8::TracedGlobal<v8::Value>*>(&value));
    }
  }
}

DISABLE_CFI_PERF
void GlobalHandles::IterateAllYoungRootsWithClassIds(
    v8::PersistentHandleVisitor* visitor) {
  for (Node* node : young_nodes_) {
    if (node->IsRetainer() && node->has_wrapper_class_id()) {
      ApplyPersistentHandleVisitor(visitor, node);
    }
  }
}

DISABLE_CFI_PERF
void GlobalHandles::IterateYoungWeakRootsWithClassIds(
    v8::PersistentHandleVisitor* visitor) {
  for (Node* node : young_nodes_) {
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
  PrintF("  allocated memory = %zuB\n", total * sizeof(Node));
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
    visitor->VisitRootPointers(Root::kEternalHandles, nullptr,
                               FullObjectSlot(block),
                               FullObjectSlot(block + Min(limit, kSize)));
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
