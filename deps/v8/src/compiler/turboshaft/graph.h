// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_TURBOSHAFT_GRAPH_H_
#define V8_COMPILER_TURBOSHAFT_GRAPH_H_

#include <algorithm>
#include <iterator>
#include <limits>
#include <memory>
#include <tuple>
#include <type_traits>

#include "src/base/iterator.h"
#include "src/base/logging.h"
#include "src/base/small-vector.h"
#include "src/base/vector.h"
#include "src/codegen/source-position.h"
#include "src/compiler/turboshaft/operations.h"
#include "src/compiler/turboshaft/sidetable.h"
#include "src/compiler/turboshaft/types.h"
#include "src/zone/zone-containers.h"

namespace v8::internal::compiler::turboshaft {

template <class Reducers>
class Assembler;

// `OperationBuffer` is a growable, Zone-allocated buffer to store Turboshaft
// operations. It is part of a `Graph`.
// The buffer can be seen as an array of 8-byte `OperationStorageSlot` values.
// The structure is append-only, that is, we only add operations at the end.
// There are rare cases (i.e., loop phis) where we overwrite an existing
// operation, but only if we can guarantee that the new operation is not bigger
// than the operation we overwrite.
class OperationBuffer {
 public:
  // A `ReplaceScope` is to overwrite an existing operation.
  // It moves the end-pointer temporarily so that the next emitted operation
  // overwrites an old one.
  class ReplaceScope {
   public:
    ReplaceScope(OperationBuffer* buffer, OpIndex replaced)
        : buffer_(buffer),
          replaced_(replaced),
          old_end_(buffer->end_),
          old_slot_count_(buffer->SlotCount(replaced)) {
      buffer_->end_ = buffer_->Get(replaced);
    }
    ~ReplaceScope() {
      DCHECK_LE(buffer_->SlotCount(replaced_), old_slot_count_);
      buffer_->end_ = old_end_;
      // Preserve the original operation size in case it has become smaller.
      buffer_->operation_sizes_[replaced_.id()] = old_slot_count_;
      buffer_->operation_sizes_[OpIndex(replaced_.offset() +
                                        static_cast<uint32_t>(old_slot_count_) *
                                            sizeof(OperationStorageSlot))
                                    .id() -
                                1] = old_slot_count_;
    }

    ReplaceScope(const ReplaceScope&) = delete;
    ReplaceScope& operator=(const ReplaceScope&) = delete;

   private:
    OperationBuffer* buffer_;
    OpIndex replaced_;
    OperationStorageSlot* old_end_;
    uint16_t old_slot_count_;
  };

  explicit OperationBuffer(Zone* zone, size_t initial_capacity) : zone_(zone) {
    DCHECK_NE(initial_capacity, 0);
    begin_ = end_ = zone_->NewArray<OperationStorageSlot>(initial_capacity);
    operation_sizes_ =
        zone_->NewArray<uint16_t>((initial_capacity + 1) / kSlotsPerId);
    end_cap_ = begin_ + initial_capacity;
  }

  OperationStorageSlot* Allocate(size_t slot_count) {
    if (V8_UNLIKELY(static_cast<size_t>(end_cap_ - end_) < slot_count)) {
      Grow(capacity() + slot_count);
      DCHECK(slot_count <= static_cast<size_t>(end_cap_ - end_));
    }
    OperationStorageSlot* result = end_;
    end_ += slot_count;
    OpIndex idx = Index(result);
    // Store the size in both for the first and last id corresponding to the new
    // operation. This enables iteration in both directions. The two id's are
    // the same if the operation is small.
    operation_sizes_[idx.id()] = slot_count;
    operation_sizes_[OpIndex(idx.offset() + static_cast<uint32_t>(slot_count) *
                                                sizeof(OperationStorageSlot))
                         .id() -
                     1] = slot_count;
    return result;
  }

  void RemoveLast() {
    size_t slot_count = operation_sizes_[EndIndex().id() - 1];
    end_ -= slot_count;
    DCHECK_GE(end_, begin_);
  }

  OpIndex Index(const Operation& op) const {
    return Index(reinterpret_cast<const OperationStorageSlot*>(&op));
  }
  OpIndex Index(const OperationStorageSlot* ptr) const {
    DCHECK(begin_ <= ptr && ptr <= end_);
    return OpIndex(static_cast<uint32_t>(reinterpret_cast<Address>(ptr) -
                                         reinterpret_cast<Address>(begin_)));
  }

  OperationStorageSlot* Get(OpIndex idx) {
    DCHECK_LT(idx.offset() / sizeof(OperationStorageSlot), size());
    return reinterpret_cast<OperationStorageSlot*>(
        reinterpret_cast<Address>(begin_) + idx.offset());
  }
  uint16_t SlotCount(OpIndex idx) {
    DCHECK_LT(idx.offset() / sizeof(OperationStorageSlot), size());
    return operation_sizes_[idx.id()];
  }

  const OperationStorageSlot* Get(OpIndex idx) const {
    DCHECK_LT(idx.offset(), capacity() * sizeof(OperationStorageSlot));
    return reinterpret_cast<const OperationStorageSlot*>(
        reinterpret_cast<Address>(begin_) + idx.offset());
  }

  OpIndex Next(OpIndex idx) const {
    DCHECK_GT(operation_sizes_[idx.id()], 0);
    OpIndex result = OpIndex(idx.offset() + operation_sizes_[idx.id()] *
                                                sizeof(OperationStorageSlot));
    DCHECK_LT(0, result.offset());
    DCHECK_LE(result.offset(), capacity() * sizeof(OperationStorageSlot));
    return result;
  }
  OpIndex Previous(OpIndex idx) const {
    DCHECK_GT(idx.id(), 0);
    DCHECK_GT(operation_sizes_[idx.id() - 1], 0);
    OpIndex result = OpIndex(idx.offset() - operation_sizes_[idx.id() - 1] *
                                                sizeof(OperationStorageSlot));
    DCHECK_LE(0, result.offset());
    DCHECK_LT(result.offset(), capacity() * sizeof(OperationStorageSlot));
    return result;
  }

  // Offset of the first operation.
  OpIndex BeginIndex() const { return OpIndex(0); }
  // One-past-the-end offset.
  OpIndex EndIndex() const { return Index(end_); }

  uint32_t size() const { return static_cast<uint32_t>(end_ - begin_); }
  uint32_t capacity() const { return static_cast<uint32_t>(end_cap_ - begin_); }

  void Grow(size_t min_capacity) {
    size_t size = this->size();
    size_t capacity = this->capacity();
    size_t new_capacity = 2 * capacity;
    while (new_capacity < min_capacity) new_capacity *= 2;
    CHECK_LT(new_capacity, std::numeric_limits<uint32_t>::max() /
                               sizeof(OperationStorageSlot));

    OperationStorageSlot* new_buffer =
        zone_->NewArray<OperationStorageSlot>(new_capacity);
    memcpy(new_buffer, begin_, size * sizeof(OperationStorageSlot));

    uint16_t* new_operation_sizes =
        zone_->NewArray<uint16_t>(new_capacity / kSlotsPerId);
    memcpy(new_operation_sizes, operation_sizes_,
           size / kSlotsPerId * sizeof(uint16_t));

    begin_ = new_buffer;
    end_ = new_buffer + size;
    end_cap_ = new_buffer + new_capacity;
    operation_sizes_ = new_operation_sizes;
  }

  void Reset() { end_ = begin_; }

 private:
  Zone* zone_;
  OperationStorageSlot* begin_;
  OperationStorageSlot* end_;
  OperationStorageSlot* end_cap_;
  uint16_t* operation_sizes_;
};

template <class Derived>
class DominatorForwardTreeNode;
template <class Derived>
class RandomAccessStackDominatorNode;

template <class Derived>
class DominatorForwardTreeNode {
  // A class storing a forward representation of the dominator tree, since the
  // regular dominator tree is represented as pointers from the children to
  // parents rather than parents to children.
 public:
  void AddChild(Derived* next) {
    DCHECK_EQ(static_cast<Derived*>(this)->len_ + 1, next->len_);
    next->neighboring_child_ = last_child_;
    last_child_ = next;
  }

  Derived* LastChild() const { return last_child_; }
  Derived* NeighboringChild() const { return neighboring_child_; }
  bool HasChildren() const { return last_child_ != nullptr; }

  base::SmallVector<Derived*, 8> Children() const {
    base::SmallVector<Derived*, 8> result;
    for (Derived* child = last_child_; child != nullptr;
         child = child->neighboring_child_) {
      result.push_back(child);
    }
    std::reverse(result.begin(), result.end());
    return result;
  }

 private:
#ifdef DEBUG
  friend class RandomAccessStackDominatorNode<Derived>;
#endif
  Derived* neighboring_child_ = nullptr;
  Derived* last_child_ = nullptr;
};

template <class Derived>
class RandomAccessStackDominatorNode
    : public DominatorForwardTreeNode<Derived> {
  // This class represents a node of a dominator tree implemented using Myers'
  // Random-Access Stack (see
  // https://publications.mpi-cbg.de/Myers_1983_6328.pdf). This datastructure
  // enables searching for a predecessor of a node in log(h) time, where h is
  // the height of the dominator tree.
 public:
  void SetDominator(Derived* dominator);
  void SetAsDominatorRoot();
  Derived* GetDominator() { return nxt_; }

  // Returns the lowest common dominator of {this} and {other}.
  Derived* GetCommonDominator(
      RandomAccessStackDominatorNode<Derived>* other) const;

  bool IsDominatedBy(const Derived* other) const {
    // TODO(dmercadier): we don't have to call GetCommonDominator and could
    // determine quicker that {this} isn't dominated by {other}.
    return GetCommonDominator(other) == other;
  }

  int Depth() const { return len_; }

 private:
  friend class DominatorForwardTreeNode<Derived>;
#ifdef DEBUG
  friend class Block;
#endif

  // Myers' original datastructure requires to often check jmp_->len_, which is
  // not so great on modern computers (memory access, caches & co). To speed up
  // things a bit, we store here jmp_len_.
  int jmp_len_ = 0;

  int len_ = 0;
  Derived* nxt_ = nullptr;
  Derived* jmp_ = nullptr;
};

// A basic block
class Block : public RandomAccessStackDominatorNode<Block> {
 public:
  enum class Kind : uint8_t { kMerge, kLoopHeader, kBranchTarget };

  bool IsLoopOrMerge() const { return IsLoop() || IsMerge(); }
  bool IsLoop() const { return kind_ == Kind::kLoopHeader; }
  bool IsMerge() const { return kind_ == Kind::kMerge; }
  bool IsBranchTarget() const { return kind_ == Kind::kBranchTarget; }
  bool IsHandler() const { return false; }
  bool IsSwitchCase() const { return false; }

  Kind kind() const { return kind_; }
  void SetKind(Kind kind) { kind_ = kind; }

  BlockIndex index() const { return index_; }

  bool Contains(OpIndex op_idx) const {
    return begin_ <= op_idx && op_idx < end_;
  }

  bool IsBound() const { return index_ != BlockIndex::Invalid(); }

  base::SmallVector<Block*, 8> Predecessors() const {
    base::SmallVector<Block*, 8> result;
    for (Block* pred = last_predecessor_; pred != nullptr;
         pred = pred->neighboring_predecessor_) {
      result.push_back(pred);
    }
    std::reverse(result.begin(), result.end());
    return result;
  }

  // TODO(dmercadier): we should store predecessor count in the Blocks directly
  // (or in the Graph, or in the Assembler), to avoid this O(n) PredecessorCount
  // method.
  int PredecessorCount() const {
    int count = 0;
    for (Block* pred = last_predecessor_; pred != nullptr;
         pred = pred->neighboring_predecessor_) {
      count++;
    }
    return count;
  }

  // Returns the index of {target} in the predecessors of the current Block.
  // If {target} is not a direct predecessor, returns -1.
  int GetPredecessorIndex(const Block* target) const {
    int pred_count = 0;
    int pred_reverse_index = -1;
    for (Block* pred = last_predecessor_; pred != nullptr;
         pred = pred->neighboring_predecessor_) {
      if (pred == target) {
        DCHECK_EQ(pred_reverse_index, -1);
        pred_reverse_index = pred_count;
      }
      pred_count++;
    }
    if (pred_reverse_index == -1) {
      return -1;
    }
    return pred_count - pred_reverse_index - 1;
  }

  // HasExactlyNPredecessors(n) returns the same result as
  // `PredecessorCount() == n`, but stops early and iterates at most the first
  // {n} predecessors.
  bool HasExactlyNPredecessors(unsigned int n) const {
    Block* current_pred = last_predecessor_;
    while (current_pred != nullptr && n != 0) {
      current_pred = current_pred->neighboring_predecessor_;
      n--;
    }
    return n == 0 && current_pred == nullptr;
  }

  Block* LastPredecessor() const { return last_predecessor_; }
  Block* NeighboringPredecessor() const { return neighboring_predecessor_; }
  bool HasPredecessors() const { return last_predecessor_ != nullptr; }
  void ResetLastPredecessor() { last_predecessor_ = nullptr; }

  void SetMappingToNextGraph(Block* next_graph_block) {
    DCHECK_NULL(next_graph_mapping_);
    DCHECK_NOT_NULL(next_graph_block);
    next_graph_mapping_ = next_graph_block;
    next_graph_block->SetOrigin(this);
  }
  Block* MapToNextGraph() const {
    DCHECK_NOT_NULL(next_graph_mapping_);
    return next_graph_mapping_;
  }
  // The block from the previous graph which produced the current block. This
  // has to be updated to be the last block that contributed operations to the
  // current block to ensure that phi nodes are created correctly.git cl
  void SetOrigin(const Block* origin) {
    DCHECK_IMPLIES(origin != nullptr,
                   origin->graph_generation_ + 1 == graph_generation_);
    origin_ = origin;
  }
  // The block from the input graph that is equivalent as a predecessor. It is
  // only available for bound blocks and it does *not* refer to an equivalent
  // block as a branch destination.
  const Block* OriginForBlockEnd() const {
    DCHECK(IsBound());
    return origin_;
  }
  // The block from the input graph that corresponds to the current block as a
  // branch destination. Such a block might not exist, and this function uses a
  // trick to compute such a block in almost all cases, but might rarely fail
  // and return `nullptr` instead.
  const Block* OriginForBlockStart() const {
    // Check that `origin_` is still valid as a block start and was not changed
    // to a semantically different block when inlining blocks.
    if (origin_ && origin_->MapToNextGraph() == this) {
      return origin_;
    }
    return nullptr;
  }

  OpIndex begin() const {
    DCHECK(begin_.valid());
    return begin_;
  }
  OpIndex end() const {
    DCHECK(end_.valid());
    return end_;
  }

  // Might return nullptr if the first operation is invalid.
  const Operation& FirstOperation(const Graph& graph) const;
  const Operation& LastOperation(const Graph& graph) const;

  bool EndsWithBranchingOp(const Graph& graph) const {
    switch (LastOperation(graph).opcode) {
      case Opcode::kBranch:
      case Opcode::kSwitch:
      case Opcode::kCallAndCatchException:
        return true;
      default:
        return false;
    }
  }

  bool HasPhis(const Graph& graph) const;

  // Computes the dominators of the this block, assuming that the dominators of
  // its predecessors are already computed. Returns the depth of the current
  // block in the dominator tree.
  uint32_t ComputeDominator();

  void PrintDominatorTree(
      std::vector<const char*> tree_symbols = std::vector<const char*>(),
      bool has_next = false) const;

  explicit Block(Kind kind) : kind_(kind) {}

  uint32_t& custom_data() { return custom_data_; }
  const uint32_t& custom_data() const { return custom_data_; }

 private:
  // AddPredecessor should never be called directly except from Assembler's
  // AddPredecessor and SplitEdge methods, which takes care of maintaining
  // split-edge form.
  void AddPredecessor(Block* predecessor) {
    DCHECK(!IsBound() ||
           (Predecessors().size() == 1 && kind_ == Kind::kLoopHeader));
    DCHECK_EQ(predecessor->neighboring_predecessor_, nullptr);
    predecessor->neighboring_predecessor_ = last_predecessor_;
    last_predecessor_ = predecessor;
  }

  friend class Graph;
  template <class Reducers>
  friend class Assembler;

  Kind kind_;
  OpIndex begin_ = OpIndex::Invalid();
  OpIndex end_ = OpIndex::Invalid();
  BlockIndex index_ = BlockIndex::Invalid();
  Block* last_predecessor_ = nullptr;
  Block* neighboring_predecessor_ = nullptr;
  const Block* origin_ = nullptr;
  Block* next_graph_mapping_ = nullptr;
  // The {custom_data_} field can be used by algorithms to temporarily store
  // block-specific data. This field is not preserved when constructing a new
  // output graph and algorithms cannot rely on this field being properly reset
  // after previous uses.
  uint32_t custom_data_ = 0;
#ifdef DEBUG
  size_t graph_generation_ = 0;
#endif
};

std::ostream& operator<<(std::ostream& os, const Block* b);

class Graph {
 public:
  // A big initial capacity prevents many growing steps. It also makes sense
  // because the graph and its memory is recycled for following phases.
  explicit Graph(Zone* graph_zone, size_t initial_capacity = 2048)
      : operations_(graph_zone, initial_capacity),
        bound_blocks_(graph_zone),
        all_blocks_(graph_zone),
        graph_zone_(graph_zone),
        source_positions_(graph_zone),
        operation_origins_(graph_zone),
        operation_types_(graph_zone)
#ifdef DEBUG
        ,
        block_type_refinement_(graph_zone)
#endif
  {
  }

  // Reset the graph to recycle its memory.
  void Reset() {
    operations_.Reset();
    bound_blocks_.clear();
    source_positions_.Reset();
    operation_origins_.Reset();
    operation_types_.Reset();
    next_block_ = 0;
    dominator_tree_depth_ = 0;
#ifdef DEBUG
    block_type_refinement_.Reset();
#endif
  }

  V8_INLINE const Operation& Get(OpIndex i) const {
    // `Operation` contains const fields and can be overwritten with placement
    // new. Therefore, std::launder is necessary to avoid undefined behavior.
    const Operation* ptr =
        std::launder(reinterpret_cast<const Operation*>(operations_.Get(i)));
    // Detect invalid memory by checking if opcode is valid.
    DCHECK_LT(OpcodeIndex(ptr->opcode), kNumberOfOpcodes);
    return *ptr;
  }
  V8_INLINE Operation& Get(OpIndex i) {
    // `Operation` contains const fields and can be overwritten with placement
    // new. Therefore, std::launder is necessary to avoid undefined behavior.
    Operation* ptr =
        std::launder(reinterpret_cast<Operation*>(operations_.Get(i)));
    // Detect invalid memory by checking if opcode is valid.
    DCHECK_LT(OpcodeIndex(ptr->opcode), kNumberOfOpcodes);
    return *ptr;
  }

  void MarkAsUnused(OpIndex i) { Get(i).saturated_use_count = 0; }

  const Block& StartBlock() const { return Get(BlockIndex(0)); }

  Block& Get(BlockIndex i) {
    DCHECK_LT(i.id(), bound_blocks_.size());
    return *bound_blocks_[i.id()];
  }
  const Block& Get(BlockIndex i) const {
    DCHECK_LT(i.id(), bound_blocks_.size());
    return *bound_blocks_[i.id()];
  }

  OpIndex Index(const Operation& op) const { return operations_.Index(op); }
  BlockIndex BlockOf(OpIndex index) const {
    auto it = std::upper_bound(
        bound_blocks_.begin(), bound_blocks_.end(), index,
        [](OpIndex value, const Block* b) { return value < b->begin_; });
    DCHECK_NE(it, bound_blocks_.begin());
    --it;
    return (*it)->index();
  }

  OpIndex NextIndex(const OpIndex idx) const { return operations_.Next(idx); }
  OpIndex PreviousIndex(const OpIndex idx) const {
    return operations_.Previous(idx);
  }

  OperationStorageSlot* Allocate(size_t slot_count) {
    return operations_.Allocate(slot_count);
  }

  void RemoveLast() {
    DecrementInputUses(*AllOperations().rbegin());
    operations_.RemoveLast();
  }

  template <class Op, class... Args>
  V8_INLINE Op& Add(Args... args) {
#ifdef DEBUG
    OpIndex result = next_operation_index();
#endif  // DEBUG
    Op& op = Op::New(this, args...);
    IncrementInputUses(op);

    if (op.Properties().is_required_when_unused) {
      // Once the graph is built, an operation with a `saturated_use_count` of 0
      // is guaranteed to be unused and can be removed. Thus, to avoid removing
      // operations that never have uses (such as Goto or Branch), we set the
      // `saturated_use_count` of Operations that are `required_when_unused`
      // to 1.
      op.saturated_use_count = 1;
    }

    DCHECK_EQ(result, Index(op));
#ifdef DEBUG
    for (OpIndex input : op.inputs()) {
      DCHECK_LT(input, result);
    }
#endif  // DEBUG
    return op;
  }

  template <class Op, class... Args>
  void Replace(OpIndex replaced, Args... args) {
    static_assert((std::is_base_of<Operation, Op>::value));
    static_assert(std::is_trivially_destructible<Op>::value);

    const Operation& old_op = Get(replaced);
    DecrementInputUses(old_op);
    auto old_uses = old_op.saturated_use_count;
    Op* new_op;
    {
      OperationBuffer::ReplaceScope replace_scope(&operations_, replaced);
      new_op = &Op::New(this, args...);
    }
    new_op->saturated_use_count = old_uses;
    IncrementInputUses(*new_op);
  }

  V8_INLINE Block* NewLoopHeader() {
    return NewBlock(Block::Kind::kLoopHeader);
  }
  V8_INLINE Block* NewBlock() { return NewBlock(Block::Kind::kMerge); }
  V8_INLINE Block* NewMappedBlock(Block* origin) {
    Block* new_block = NewBlock(origin->IsLoop() ? Block::Kind::kLoopHeader
                                                 : Block::Kind::kMerge);
    origin->SetMappingToNextGraph(new_block);
    return new_block;
  }

  V8_INLINE bool Add(Block* block) {
    DCHECK_EQ(block->graph_generation_, generation_);
    if (!bound_blocks_.empty() && !block->HasPredecessors()) return false;

    DCHECK(!block->begin_.valid());
    block->begin_ = next_operation_index();
    DCHECK_EQ(block->index_, BlockIndex::Invalid());
    block->index_ = next_block_index();
    bound_blocks_.push_back(block);
    uint32_t depth = block->ComputeDominator();
    dominator_tree_depth_ = std::max<uint32_t>(dominator_tree_depth_, depth);

    return true;
  }

  void Finalize(Block* block) {
    DCHECK(!block->end_.valid());
    block->end_ = next_operation_index();
  }

  void TurnLoopIntoMerge(Block* loop) {
    DCHECK(loop->IsLoop());
    DCHECK_EQ(loop->PredecessorCount(), 1);
    loop->kind_ = Block::Kind::kMerge;
    for (Operation& op : operations(*loop)) {
      if (auto* pending_phi = op.TryCast<PendingLoopPhiOp>()) {
        Replace<PhiOp>(Index(*pending_phi),
                       base::VectorOf({pending_phi->first()}),
                       pending_phi->rep);
      }
    }
  }

  OpIndex next_operation_index() const { return operations_.EndIndex(); }
  BlockIndex next_block_index() const {
    return BlockIndex(static_cast<uint32_t>(bound_blocks_.size()));
  }

  Zone* graph_zone() const { return graph_zone_; }
  uint32_t block_count() const {
    return static_cast<uint32_t>(bound_blocks_.size());
  }
  uint32_t op_id_count() const {
    return (operations_.size() + (kSlotsPerId - 1)) / kSlotsPerId;
  }
  uint32_t op_id_capacity() const {
    return operations_.capacity() / kSlotsPerId;
  }

  class OpIndexIterator
      : public base::iterator<std::bidirectional_iterator_tag, OpIndex> {
   public:
    using value_type = OpIndex;

    explicit OpIndexIterator(OpIndex index, const Graph* graph)
        : index_(index), graph_(graph) {}
    value_type& operator*() { return index_; }
    OpIndexIterator& operator++() {
      index_ = graph_->operations_.Next(index_);
      return *this;
    }
    OpIndexIterator& operator--() {
      index_ = graph_->operations_.Previous(index_);
      return *this;
    }
    bool operator!=(OpIndexIterator other) const {
      DCHECK_EQ(graph_, other.graph_);
      return index_ != other.index_;
    }
    bool operator==(OpIndexIterator other) const { return !(*this != other); }

   private:
    OpIndex index_;
    const Graph* const graph_;
  };

  template <class OperationT, typename GraphT>
  class OperationIterator
      : public base::iterator<std::bidirectional_iterator_tag, OperationT> {
   public:
    static_assert(std::is_same_v<std::remove_const_t<OperationT>, Operation> &&
                  std::is_same_v<std::remove_const_t<GraphT>, Graph>);
    using value_type = OperationT;

    explicit OperationIterator(OpIndex index, GraphT* graph)
        : index_(index), graph_(graph) {}
    value_type& operator*() { return graph_->Get(index_); }
    OperationIterator& operator++() {
      index_ = graph_->operations_.Next(index_);
      return *this;
    }
    OperationIterator& operator--() {
      index_ = graph_->operations_.Previous(index_);
      return *this;
    }
    bool operator!=(OperationIterator other) const {
      DCHECK_EQ(graph_, other.graph_);
      return index_ != other.index_;
    }
    bool operator==(OperationIterator other) const { return !(*this != other); }

   private:
    OpIndex index_;
    GraphT* const graph_;
  };

  using MutableOperationIterator = OperationIterator<Operation, Graph>;
  using ConstOperationIterator =
      OperationIterator<const Operation, const Graph>;

  base::iterator_range<MutableOperationIterator> AllOperations() {
    return operations(operations_.BeginIndex(), operations_.EndIndex());
  }
  base::iterator_range<ConstOperationIterator> AllOperations() const {
    return operations(operations_.BeginIndex(), operations_.EndIndex());
  }

  base::iterator_range<OpIndexIterator> AllOperationIndices() const {
    return OperationIndices(operations_.BeginIndex(), operations_.EndIndex());
  }

  base::iterator_range<MutableOperationIterator> operations(
      const Block& block) {
    return operations(block.begin_, block.end_);
  }
  base::iterator_range<ConstOperationIterator> operations(
      const Block& block) const {
    return operations(block.begin_, block.end_);
  }

  base::iterator_range<OpIndexIterator> OperationIndices(
      const Block& block) const {
    return OperationIndices(block.begin_, block.end_);
  }

  base::iterator_range<ConstOperationIterator> operations(OpIndex begin,
                                                          OpIndex end) const {
    DCHECK(begin.valid());
    DCHECK(end.valid());
    return {ConstOperationIterator(begin, this),
            ConstOperationIterator(end, this)};
  }
  base::iterator_range<MutableOperationIterator> operations(OpIndex begin,
                                                            OpIndex end) {
    DCHECK(begin.valid());
    DCHECK(end.valid());
    return {MutableOperationIterator(begin, this),
            MutableOperationIterator(end, this)};
  }

  base::iterator_range<OpIndexIterator> OperationIndices(OpIndex begin,
                                                         OpIndex end) const {
    DCHECK(begin.valid());
    DCHECK(end.valid());
    return {OpIndexIterator(begin, this), OpIndexIterator(end, this)};
  }

  base::iterator_range<base::DerefPtrIterator<Block>> blocks() {
    return {base::DerefPtrIterator<Block>(bound_blocks_.data()),
            base::DerefPtrIterator<Block>(bound_blocks_.data() +
                                          bound_blocks_.size())};
  }
  base::iterator_range<base::DerefPtrIterator<const Block>> blocks() const {
    return {base::DerefPtrIterator<const Block>(bound_blocks_.data()),
            base::DerefPtrIterator<const Block>(bound_blocks_.data() +
                                                bound_blocks_.size())};
  }

  bool IsLoopBackedge(const GotoOp& op) const {
    DCHECK(op.destination->IsBound());
    return op.destination->begin() <= Index(op);
  }

  bool IsValid(OpIndex i) const { return i < next_operation_index(); }

  const GrowingSidetable<SourcePosition>& source_positions() const {
    return source_positions_;
  }
  GrowingSidetable<SourcePosition>& source_positions() {
    return source_positions_;
  }

  const GrowingSidetable<OpIndex>& operation_origins() const {
    return operation_origins_;
  }
  GrowingSidetable<OpIndex>& operation_origins() { return operation_origins_; }

  uint32_t DominatorTreeDepth() const { return dominator_tree_depth_; }
  const GrowingSidetable<Type>& operation_types() const {
    return operation_types_;
  }
  GrowingSidetable<Type>& operation_types() { return operation_types_; }
#ifdef DEBUG
  // Store refined types per block here for --trace-turbo printing.
  // TODO(nicohartmann@): Remove this once we have a proper way to print
  // type information inside the reducers.
  using TypeRefinements = std::vector<std::pair<OpIndex, Type>>;
  const GrowingBlockSidetable<TypeRefinements>& block_type_refinement() const {
    return block_type_refinement_;
  }
  GrowingBlockSidetable<TypeRefinements>& block_type_refinement() {
    return block_type_refinement_;
  }
#endif  // DEBUG

  Graph& GetOrCreateCompanion() {
    if (!companion_) {
      companion_ = std::make_unique<Graph>(graph_zone_, operations_.size());
#ifdef DEBUG
      companion_->generation_ = generation_ + 1;
#endif  // DEBUG
    }
    return *companion_;
  }

  // Swap the graph with its companion graph to turn the output of one phase
  // into the input of the next phase.
  void SwapWithCompanion() {
    Graph& companion = GetOrCreateCompanion();
    std::swap(operations_, companion.operations_);
    std::swap(bound_blocks_, companion.bound_blocks_);
    std::swap(all_blocks_, companion.all_blocks_);
    std::swap(next_block_, companion.next_block_);
    std::swap(graph_zone_, companion.graph_zone_);
    std::swap(source_positions_, companion.source_positions_);
    std::swap(operation_origins_, companion.operation_origins_);
    std::swap(operation_types_, companion.operation_types_);
#ifdef DEBUG
    std::swap(block_type_refinement_, companion.block_type_refinement_);
    // Update generation index.
    DCHECK_EQ(generation_ + 1, companion.generation_);
    generation_ = companion.generation_++;
#endif  // DEBUG
  }

#ifdef DEBUG
  size_t generation() const { return generation_; }
#endif  // DEBUG

 private:
  bool InputsValid(const Operation& op) const {
    for (OpIndex i : op.inputs()) {
      if (!IsValid(i)) return false;
    }
    return true;
  }

  template <class Op>
  void IncrementInputUses(const Op& op) {
    for (OpIndex input : op.inputs()) {
      Operation& input_op = Get(input);
      auto uses = input_op.saturated_use_count;
      if (V8_LIKELY(uses != Operation::kUnknownUseCount)) {
        input_op.saturated_use_count = uses + 1;
      }
    }
  }

  template <class Op>
  void DecrementInputUses(const Op& op) {
    for (OpIndex input : op.inputs()) {
      Operation& input_op = Get(input);
      auto uses = input_op.saturated_use_count;
      DCHECK_GT(uses, 0);
      // Do not decrement if we already reached the threshold. In this case, we
      // don't know the exact number of uses anymore and shouldn't assume
      // anything.
      if (V8_LIKELY(uses != Operation::kUnknownUseCount)) {
        input_op.saturated_use_count = uses - 1;
      }
    }
  }

  V8_INLINE Block* NewBlock(Block::Kind kind) {
    if (V8_UNLIKELY(next_block_ == all_blocks_.size())) {
      constexpr size_t new_block_count = 64;
      base::Vector<Block> blocks =
          graph_zone_->NewVector<Block>(new_block_count, Block(kind));
      for (size_t i = 0; i < new_block_count; ++i) {
        all_blocks_.push_back(&blocks[i]);
      }
    }
    Block* result = all_blocks_[next_block_++];
    *result = Block(kind);
#ifdef DEBUG
    result->graph_generation_ = generation_;
#endif
    return result;
  }

  OperationBuffer operations_;
  ZoneVector<Block*> bound_blocks_;
  ZoneVector<Block*> all_blocks_;
  size_t next_block_ = 0;
  Zone* graph_zone_;
  GrowingSidetable<SourcePosition> source_positions_;
  GrowingSidetable<OpIndex> operation_origins_;
  uint32_t dominator_tree_depth_ = 0;
  GrowingSidetable<Type> operation_types_;
#ifdef DEBUG
  GrowingBlockSidetable<TypeRefinements> block_type_refinement_;
#endif

  std::unique_ptr<Graph> companion_ = {};
#ifdef DEBUG
  size_t generation_ = 1;
#endif  // DEBUG
};

V8_INLINE OperationStorageSlot* AllocateOpStorage(Graph* graph,
                                                  size_t slot_count) {
  return graph->Allocate(slot_count);
}

V8_INLINE const Operation& Get(const Graph& graph, OpIndex index) {
  return graph.Get(index);
}

V8_INLINE const Operation& Block::FirstOperation(const Graph& graph) const {
  DCHECK_EQ(graph_generation_, graph.generation());
  DCHECK(begin_.valid());
  DCHECK(end_.valid());
  return graph.Get(begin_);
}

V8_INLINE const Operation& Block::LastOperation(const Graph& graph) const {
  DCHECK_EQ(graph_generation_, graph.generation());
  return graph.Get(graph.PreviousIndex(end()));
}

V8_INLINE bool Block::HasPhis(const Graph& graph) const {
  DCHECK_EQ(graph_generation_, graph.generation());
#ifdef DEBUG
  // Verify that only Phis/FrameStates are found, then all other Phis/
  // FrameStateOps in the block come consecutively.
  bool starts_with_phi = false;
  bool finished_phis = false;
  for (const auto& op : graph.operations(*this)) {
    if (op.Is<PhiOp>()) {
      DCHECK(!finished_phis);
      starts_with_phi = true;
    }
    if (!op.Is<PhiOp>() && !op.Is<FrameStateOp>()) {
      finished_phis = true;
    }
  }
  return starts_with_phi;
#else   // DEBUG
  for (const auto& op : graph.operations(*this)) {
    if (op.Is<PhiOp>()) {
      return true;
    } else if (op.Is<FrameStateOp>()) {
      continue;
    } else {
      return false;
    }
  }
  return false;
#endif  // DEBUG
}

struct PrintAsBlockHeader {
  const Block& block;
  BlockIndex block_id = block.index();
};
std::ostream& operator<<(std::ostream& os, PrintAsBlockHeader block);
std::ostream& operator<<(std::ostream& os, const Graph& graph);
std::ostream& operator<<(std::ostream& os, const Block::Kind& kind);

inline uint32_t Block::ComputeDominator() {
  if (V8_UNLIKELY(LastPredecessor() == nullptr)) {
    // If the block has no predecessors, then it's the start block. We create a
    // jmp_ edge to itself, so that the SetDominator algorithm does not need a
    // special case for when the start block is reached.
    SetAsDominatorRoot();
  } else {
    // If the block has one or more predecessors, the dominator is the lowest
    // common ancestor (LCA) of all of the predecessors.

    // Note that for BranchTarget, there is a single predecessor. This doesn't
    // change the logic: the loop won't be entered, and the first (and only)
    // predecessor is set as the dominator.
    // Similarly, since we compute dominators on the fly, when we reach a
    // kLoopHeader, we haven't visited its body yet, and it should only have one
    // predecessor (the backedge is not here yet), which is its dominator.
    DCHECK_IMPLIES(kind_ == Block::Kind::kLoopHeader, PredecessorCount() == 1);

    Block* dominator = LastPredecessor();
    for (Block* pred = dominator->NeighboringPredecessor(); pred != nullptr;
         pred = pred->NeighboringPredecessor()) {
      dominator = dominator->GetCommonDominator(pred);
    }
    SetDominator(dominator);
  }
  DCHECK_NE(jmp_, nullptr);
  DCHECK_IMPLIES(nxt_ == nullptr, LastPredecessor() == nullptr);
  DCHECK_IMPLIES(len_ == 0, LastPredecessor() == nullptr);
  return Depth();
}

template <class Derived>
inline void RandomAccessStackDominatorNode<Derived>::SetAsDominatorRoot() {
  jmp_ = static_cast<Derived*>(this);
  nxt_ = nullptr;
  len_ = 0;
  jmp_len_ = 0;
}

template <class Derived>
inline void RandomAccessStackDominatorNode<Derived>::SetDominator(
    Derived* dominator) {
  DCHECK_NOT_NULL(dominator);
  DCHECK_NULL(static_cast<Block*>(this)->neighboring_child_);
  DCHECK_NULL(static_cast<Block*>(this)->last_child_);
  // Determining the jmp pointer
  Derived* t = dominator->jmp_;
  if (dominator->len_ - t->len_ == t->len_ - t->jmp_len_) {
    t = t->jmp_;
  } else {
    t = dominator;
  }
  // Initializing fields
  nxt_ = dominator;
  jmp_ = t;
  len_ = dominator->len_ + 1;
  jmp_len_ = jmp_->len_;
  dominator->AddChild(static_cast<Derived*>(this));
}

template <class Derived>
inline Derived* RandomAccessStackDominatorNode<Derived>::GetCommonDominator(
    RandomAccessStackDominatorNode<Derived>* other) const {
  const RandomAccessStackDominatorNode* a = this;
  const RandomAccessStackDominatorNode* b = other;
  if (b->len_ > a->len_) {
    // Swapping |a| and |b| so that |a| always has a greater length.
    std::swap(a, b);
  }
  DCHECK_GE(a->len_, 0);
  DCHECK_GE(b->len_, 0);

  // Going up the dominators of |a| in order to reach the level of |b|.
  while (a->len_ != b->len_) {
    DCHECK_GE(a->len_, 0);
    if (a->jmp_len_ >= b->len_) {
      a = a->jmp_;
    } else {
      a = a->nxt_;
    }
  }

  // Going up the dominators of |a| and |b| simultaneously until |a| == |b|
  while (a != b) {
    DCHECK_EQ(a->len_, b->len_);
    DCHECK_GE(a->len_, 0);
    if (a->jmp_ == b->jmp_) {
      // We found a common dominator, but we actually want to find the smallest
      // one, so we go down in the current subtree.
      a = a->nxt_;
      b = b->nxt_;
    } else {
      a = a->jmp_;
      b = b->jmp_;
    }
  }

  return static_cast<Derived*>(
      const_cast<RandomAccessStackDominatorNode<Derived>*>(a));
}

}  // namespace v8::internal::compiler::turboshaft

#endif  // V8_COMPILER_TURBOSHAFT_GRAPH_H_
