// Copyright 2017 The Abseil Authors.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

// GraphCycles provides incremental cycle detection on a dynamic
// graph using the following algorithm:
//
// A dynamic topological sort algorithm for directed acyclic graphs
// David J. Pearce, Paul H. J. Kelly
// Journal of Experimental Algorithmics (JEA) JEA Homepage archive
// Volume 11, 2006, Article No. 1.7
//
// Brief summary of the algorithm:
//
// (1) Maintain a rank for each node that is consistent
//     with the topological sort of the graph. I.e., path from x to y
//     implies rank[x] < rank[y].
// (2) When a new edge (x->y) is inserted, do nothing if rank[x] < rank[y].
// (3) Otherwise: adjust ranks in the neighborhood of x and y.
//
// Unlike in the paper, ranks are spaced apart, which often lets step (3) move
// just x or just y (see "Rank allocation" below).

#include "absl/base/internal/low_level_alloc.h"  // IWYU pragma: keep
// This file is a no-op if the required LowLevelAlloc support is missing.
#ifndef ABSL_LOW_LEVEL_ALLOC_MISSING

#include <algorithm>
#include <array>
#include <cinttypes>
#include <cstddef>
#include <cstdint>
#include <limits>

#include "absl/algorithm/container.h"
#include "absl/base/attributes.h"
#include "absl/base/config.h"
#include "absl/base/internal/hide_ptr.h"
#include "absl/base/internal/raw_logging.h"
#include "absl/base/internal/scheduling_mode.h"
#include "absl/base/internal/spinlock.h"
#include "absl/base/macros.h"
#include "absl/synchronization/internal/graphcycles.h"

// Do not use STL.   This module does not use standard memory allocation.

namespace absl {
ABSL_NAMESPACE_BEGIN
namespace synchronization_internal {

namespace {

// Avoid LowLevelAlloc's default arena since it calls malloc hooks in
// which people are doing things like acquiring Mutexes.
ABSL_CONST_INIT static absl::base_internal::SpinLock arena_mu(
    base_internal::SCHEDULE_KERNEL_ONLY);
ABSL_CONST_INIT static base_internal::LowLevelAlloc::Arena* arena;

static void InitArenaIfNecessary() {
  base_internal::SpinLockHolder l(arena_mu);
  if (arena == nullptr) {
    arena = base_internal::LowLevelAlloc::NewArena(0);
  }
}

// Number of inlined elements in Vec.  Hash table implementation
// relies on this being a power of two.
static const uint32_t kInline = 8;

// A simple LowLevelAlloc based resizable vector with inlined storage
// for a few elements.  T must be a plain type since constructor
// and destructor are not run on elements of type T managed by Vec.
template <typename T>
class Vec {
 public:
  Vec() { Init(); }
  ~Vec() { Discard(); }

  void clear() {
    Discard();
    Init();
  }

  bool empty() const { return size_ == 0; }
  uint32_t size() const { return size_; }
  T* begin() { return ptr_; }
  T* end() { return ptr_ + size_; }
  const T& operator[](uint32_t i) const { return ptr_[i]; }
  T& operator[](uint32_t i) { return ptr_[i]; }
  const T& back() const { return ptr_[size_ - 1]; }
  void pop_back() { size_--; }

  void push_back(const T& v) {
    if (size_ == capacity_) Grow(size_ + 1);
    ptr_[size_] = v;
    size_++;
  }

  void resize(uint32_t n) {
    if (n > capacity_) Grow(n);
    size_ = n;
  }

  void fill(const T& val) {
    for (uint32_t i = 0; i < size(); i++) {
      ptr_[i] = val;
    }
  }

  // Guarantees src is empty at end.
  // Provided for the hash table resizing code below.
  void MoveFrom(Vec<T>* src) {
    if (src->ptr_ == src->space_) {
      // Need to actually copy
      resize(src->size_);
      std::copy_n(src->ptr_, src->size_, ptr_);
      src->size_ = 0;
    } else {
      Discard();
      ptr_ = src->ptr_;
      size_ = src->size_;
      capacity_ = src->capacity_;
      src->Init();
    }
  }

 private:
  T* ptr_;
  T space_[kInline];
  uint32_t size_;
  uint32_t capacity_;

  void Init() {
    ptr_ = space_;
    size_ = 0;
    capacity_ = kInline;
  }

  void Discard() {
    if (ptr_ != space_) base_internal::LowLevelAlloc::Free(ptr_);
  }

  void Grow(uint32_t n) {
    while (capacity_ < n) {
      capacity_ *= 2;
    }
    size_t request = static_cast<size_t>(capacity_) * sizeof(T);
    T* copy = static_cast<T*>(
        base_internal::LowLevelAlloc::AllocWithArena(request, arena));
    std::copy_n(ptr_, size_, copy);
    Discard();
    ptr_ = copy;
  }

  Vec(const Vec&) = delete;
  Vec& operator=(const Vec&) = delete;
};

// A hash set of non-negative int32_t that uses Vec for its underlying storage.
class NodeSet {
 public:
  NodeSet() { Init(); }

  void clear() { Init(); }
  bool contains(int32_t v) const { return table_[FindIndex(v)] == v; }

  bool insert(int32_t v) {
    uint32_t i = FindIndex(v);
    if (table_[i] == v) {
      return false;
    }
    if (table_[i] == kEmpty) {
      // Only inserting over an empty cell increases the number of occupied
      // slots.
      occupied_++;
    }
    table_[i] = v;
    // Double when 75% full.
    if (occupied_ >= table_.size() - table_.size() / 4) Grow();
    return true;
  }

  void erase(int32_t v) {
    uint32_t i = FindIndex(v);
    if (table_[i] == v) {
      table_[i] = kDel;
    }
  }

  // Iteration: is done via HASH_FOR_EACH
  // Example:
  //    HASH_FOR_EACH(elem, node->out) { ... }
#define HASH_FOR_EACH(elem, eset) \
  for (int32_t elem, _cursor = 0; (eset).Next(&_cursor, &elem);)
  bool Next(int32_t* cursor, int32_t* elem) {
    while (static_cast<uint32_t>(*cursor) < table_.size()) {
      int32_t v = table_[static_cast<uint32_t>(*cursor)];
      (*cursor)++;
      if (v >= 0) {
        *elem = v;
        return true;
      }
    }
    return false;
  }

 private:
  enum : int32_t { kEmpty = -1, kDel = -2 };
  Vec<int32_t> table_;
  uint32_t occupied_;  // Count of non-empty slots (includes deleted slots)

  static uint32_t Hash(int32_t a) { return static_cast<uint32_t>(a) * 41; }

  // Return index for storing v.  May return an empty index or deleted index
  uint32_t FindIndex(int32_t v) const {
    // Search starting at hash index.
    const uint32_t mask = table_.size() - 1;
    uint32_t i = Hash(v) & mask;
    uint32_t deleted_index = 0;  // index of first deleted element we see
    bool seen_deleted_element = false;
    while (true) {
      int32_t e = table_[i];
      if (v == e) {
        return i;
      } else if (e == kEmpty) {
        // Return any previously encountered deleted slot.
        return seen_deleted_element ? deleted_index : i;
      } else if (e == kDel && !seen_deleted_element) {
        // Keep searching since v might be present later.
        deleted_index = i;
        seen_deleted_element = true;
      }
      i = (i + 1) & mask;  // Linear probing; quadratic is slightly slower.
    }
  }

  void Init() {
    table_.clear();
    table_.resize(kInline);
    table_.fill(kEmpty);
    occupied_ = 0;
  }

  void Grow() {
    Vec<int32_t> copy;
    copy.MoveFrom(&table_);
    occupied_ = 0;
    table_.resize(copy.size() * 2);
    table_.fill(kEmpty);

    for (const auto& e : copy) {
      if (e >= 0) insert(e);
    }
  }

  NodeSet(const NodeSet&) = delete;
  NodeSet& operator=(const NodeSet&) = delete;
};

// We encode a node index and a node version in GraphId.  The version
// number is incremented when the GraphId is freed which automatically
// invalidates all copies of the GraphId.

inline GraphId MakeId(int32_t index, uint32_t version) {
  GraphId g;
  g.handle =
      (static_cast<uint64_t>(version) << 32) | static_cast<uint32_t>(index);
  return g;
}

inline int32_t NodeIndex(GraphId id) { return static_cast<int32_t>(id.handle); }

inline uint32_t NodeVersion(GraphId id) {
  return static_cast<uint32_t>(id.handle >> 32);
}

struct Node {
  int64_t rank;          // Unique rank (see "Rank allocation")
  int32_t prev_slot;     // Slot with the next lower rank, or -1 if none
  int32_t next_slot;     // Slot with the next higher rank, or -1 if none
  uint32_t version;      // Current version number
  int32_t next_hash;     // Next entry in hash table
  bool visited;          // Temporary marker used by depth-first-search
  uintptr_t masked_ptr;  // User-supplied pointer
  NodeSet in;            // List of immediate predecessor nodes in graph
  NodeSet out;           // List of immediate successor nodes in graph
  int priority;          // Priority of recorded stack trace.
  int nstack;            // Depth of recorded stack trace.
  void* stack[40];       // stack[0,nstack-1] holds stack trace for node.
};

// Hash table for pointer to node index lookups.
class PointerMap {
 public:
  explicit PointerMap(const Vec<Node*>* nodes) : nodes_(nodes) {
    table_.fill(-1);
  }

  int32_t Find(void* ptr) {
    auto masked = base_internal::HidePtr(ptr);
    for (int32_t i = table_[Hash(ptr)]; i != -1;) {
      Node* n = (*nodes_)[static_cast<uint32_t>(i)];
      if (n->masked_ptr == masked) return i;
      i = n->next_hash;
    }
    return -1;
  }

  void Add(void* ptr, int32_t i) {
    int32_t* head = &table_[Hash(ptr)];
    (*nodes_)[static_cast<uint32_t>(i)]->next_hash = *head;
    *head = i;
  }

  int32_t Remove(void* ptr) {
    // Advance through linked list while keeping track of the
    // predecessor slot that points to the current entry.
    auto masked = base_internal::HidePtr(ptr);
    for (int32_t* slot = &table_[Hash(ptr)]; *slot != -1;) {
      int32_t index = *slot;
      Node* n = (*nodes_)[static_cast<uint32_t>(index)];
      if (n->masked_ptr == masked) {
        *slot = n->next_hash;  // Remove n from linked list
        n->next_hash = -1;
        return index;
      }
      slot = &n->next_hash;
    }
    return -1;
  }

 private:
  // Number of buckets in hash table for pointer lookups.
  static constexpr uint32_t kHashTableSize = 262139;  // should be prime

  const Vec<Node*>* nodes_;
  std::array<int32_t, kHashTableSize> table_;

  static uint32_t Hash(void* ptr) {
    return reinterpret_cast<uintptr_t>(ptr) % kHashTableSize;
  }
};

}  // namespace

// Rank allocation
//
// Ranks are unique int64_t values. All slots, including free slots, are kept in
// a doubly linked list sorted by rank (Node::prev_slot and Node::next_slot,
// from Rep::lowest_slot to Rep::highest_slot). Free slots keep their rank and
// their place in the list, which are reused along with the slot. New slots get
// ranks kRankStride above the highest rank.
//
// When a new edge x->y violates the rank order, InsertEdge first tries to move
// just x below y, or just y above x (see TryMoveXBelowY()): either to the
// midpoint between two ranks that are adjacent in the list, or kRankStride
// beyond the lowest or highest rank. No other slot can have such a rank, so
// ranks stay unique.
//
// kRankStride leaves room for about 32 moves, one after another, into the gap
// between two adjacent new slots. When a gap has no room left, Respace() first
// spreads out the ranks around it. Ranks beyond the lowest and highest ones
// stay within [kMinRank, kMaxRank], which leaves room for about 2^30 moves at
// each end before all slots are respaced.
static constexpr int64_t kRankStride = int64_t{1} << 32;
static constexpr int64_t kMinRank = -(int64_t{1} << 62);
static constexpr int64_t kMaxRank = int64_t{1} << 62;
// Respace() spreads out ranks until they can be at least this far apart.
static constexpr uint64_t kMinRespacedGap = uint64_t{1} << 16;
// Maximum number of in-neighbors of x (or out-neighbors of y) that
// TryMoveXBelowY() (or TryMoveYAboveX()) will examine.
static constexpr int kMaxSingleNodeDegree = 16;

struct GraphCycles::Rep {
  Vec<Node*> nodes_;
  Vec<int32_t> free_nodes_;  // Indices for unused entries in nodes_
  PointerMap ptrmap_;
  int32_t lowest_slot = -1;   // Slot with the lowest rank, or -1 if none
  int32_t highest_slot = -1;  // Slot with the highest rank, or -1 if none
  int64_t rank_stride = kRankStride;  // See TestOnlySetRankStride()

  // Temporary state.
  Vec<int32_t> deltaf_;  // Results of forward DFS
  Vec<int32_t> deltab_;  // Results of backward DFS
  Vec<int32_t> list_;    // All nodes to reprocess
  Vec<int32_t> stack_;   // Emulates recursion stack for depth-first searches
  // Used by Reorder(): the slots to reorder, sorted by their current ranks, and
  // the rank, prev_slot and next_slot of each.
  Vec<int32_t> sorted_slots;
  Vec<int64_t> sorted_ranks;
  Vec<int32_t> old_prev_slots;
  Vec<int32_t> old_next_slots;

  Rep() : ptrmap_(&nodes_) {}
};

static Node* FindNode(GraphCycles::Rep* rep, GraphId id) {
  Node* n = rep->nodes_[static_cast<uint32_t>(NodeIndex(id))];
  return (n->version == NodeVersion(id)) ? n : nullptr;
}

static Node* NodeAt(GraphCycles::Rep* r, int32_t slot) {
  return r->nodes_[static_cast<uint32_t>(slot)];
}

// Makes next_slot follow prev_slot in the rank list. prev_slot == -1 makes
// next_slot the lowest slot, and next_slot == -1 makes prev_slot the highest.
static void Link(GraphCycles::Rep* r, int32_t prev_slot, int32_t next_slot) {
  if (prev_slot == -1) {
    r->lowest_slot = next_slot;
  } else {
    NodeAt(r, prev_slot)->next_slot = next_slot;
  }
  if (next_slot == -1) {
    r->highest_slot = prev_slot;
  } else {
    NodeAt(r, next_slot)->prev_slot = prev_slot;
  }
}

// Removes slot from the rank list.
static void Unlink(GraphCycles::Rep* r, int32_t slot) {
  Link(r, NodeAt(r, slot)->prev_slot, NodeAt(r, slot)->next_slot);
}

// Returns upper - lower, for lower <= upper, without overflow.
static uint64_t RankDistance(int64_t lower, int64_t upper) {
  return static_cast<uint64_t>(upper) - static_cast<uint64_t>(lower);
}

// Gives the run of adjacent slots from first_slot to last_slot in the rank list
// evenly spaced ranks, keeping their order. First grows the run on both sides
// until its slots can be at least kMinRespacedGap apart between the ranks just
// outside it (kMinRank and kMaxRank beyond the ends of the list), or until it
// covers the whole list. The whole list is spaced across
// [kMinRank / 2, kMaxRank / 2], which leaves room beyond both of its ends.
static void Respace(GraphCycles::Rep* r, int32_t first_slot,
                    int32_t last_slot) {
  uint64_t count = 1;
  for (int32_t slot = first_slot; slot != last_slot;
       slot = NodeAt(r, slot)->next_slot) {
    count++;
  }
  int64_t lower;
  int64_t upper;
  while (true) {
    const int32_t before_slot = NodeAt(r, first_slot)->prev_slot;
    const int32_t after_slot = NodeAt(r, last_slot)->next_slot;
    if (before_slot == -1 && after_slot == -1) {
      lower = kMinRank / 2;
      upper = kMaxRank / 2;
      break;
    }
    lower = before_slot == -1 ? kMinRank : NodeAt(r, before_slot)->rank;
    upper = after_slot == -1 ? kMaxRank : NodeAt(r, after_slot)->rank;
    if (RankDistance(lower, upper) / (count + 1) >= kMinRespacedGap) break;
    // Double the size of the run, as far as the ends of the list allow.
    for (const uint64_t target = 2 * count; count < target;) {
      const int32_t prev_slot = NodeAt(r, first_slot)->prev_slot;
      const int32_t next_slot = NodeAt(r, last_slot)->next_slot;
      if (prev_slot == -1 && next_slot == -1) break;
      if (prev_slot != -1) {
        first_slot = prev_slot;
        count++;
      }
      if (next_slot != -1) {
        last_slot = next_slot;
        count++;
      }
    }
  }
  const uint64_t step = RankDistance(lower, upper) / (count + 1);
  int64_t rank = lower;
  for (int32_t slot = first_slot;; slot = NodeAt(r, slot)->next_slot) {
    rank += static_cast<int64_t>(step);
    NodeAt(r, slot)->rank = rank;
    if (slot == last_slot) return;
  }
}

// Links slot, which is not in the rank list, between prev_slot and next_slot,
// which are adjacent in it (-1 beyond its ends), with a rank between theirs:
// rank_stride (kRankStride by default) beyond prev_slot or next_slot at the
// ends of the list, and halfway between them otherwise. If there is no room
// for such a rank, respaces ranks first.
static void LinkBetween(GraphCycles::Rep* r, int32_t slot, int32_t prev_slot,
                        int32_t next_slot) {
  int64_t rank;
  if (prev_slot == -1 && next_slot == -1) {
    rank = 0;
  } else if (prev_slot == -1) {
    if (NodeAt(r, next_slot)->rank < kMinRank + r->rank_stride) {
      Respace(r, r->lowest_slot, r->highest_slot);
    }
    rank = NodeAt(r, next_slot)->rank - r->rank_stride;
  } else if (next_slot == -1) {
    if (NodeAt(r, prev_slot)->rank > kMaxRank - r->rank_stride) {
      Respace(r, r->lowest_slot, r->highest_slot);
    }
    rank = NodeAt(r, prev_slot)->rank + r->rank_stride;
  } else {
    const Node* prev_node = NodeAt(r, prev_slot);
    const Node* next_node = NodeAt(r, next_slot);
    if (RankDistance(prev_node->rank, next_node->rank) < 2) {
      Respace(r, prev_slot, next_slot);
    }
    const uint64_t gap = RankDistance(prev_node->rank, next_node->rank);
    rank = prev_node->rank + static_cast<int64_t>(gap / 2);
  }
  NodeAt(r, slot)->rank = rank;
  Link(r, prev_slot, slot);
  Link(r, slot, next_slot);
}

void GraphCycles::TestOnlyAddNodes(uint32_t n) {
  uint32_t old_size = rep_->nodes_.size();
  rep_->nodes_.resize(n);
  for (auto i = old_size; i < n; ++i) {
    rep_->nodes_[i] = nullptr;
  }
}

void GraphCycles::TestOnlySetRankStride(int64_t stride) {
  // After Respace() of the whole list, there is room for a stride of 2^61
  // beyond each end.
  ABSL_RAW_CHECK(stride >= 1 && stride <= kMaxRank / 2, "Bad rank stride");
  rep_->rank_stride = stride;
}

GraphCycles::GraphCycles() {
  InitArenaIfNecessary();
  rep_ = new (base_internal::LowLevelAlloc::AllocWithArena(sizeof(Rep), arena))
      Rep;
}

GraphCycles::~GraphCycles() {
  for (auto* node : rep_->nodes_) {
    if (node == nullptr) {
      continue;
    }
    node->Node::~Node();
    base_internal::LowLevelAlloc::Free(node);
  }
  rep_->Rep::~Rep();
  base_internal::LowLevelAlloc::Free(rep_);
}

bool GraphCycles::CheckInvariants() const {
  Rep* r = rep_;
  for (uint32_t x = 0; x < r->nodes_.size(); x++) {
    Node* nx = r->nodes_[x];
    void* ptr = base_internal::UnhidePtr<void>(nx->masked_ptr);
    if (ptr != nullptr && static_cast<uint32_t>(r->ptrmap_.Find(ptr)) != x) {
      ABSL_RAW_LOG(FATAL, "Did not find live node in hash table %" PRIu32 " %p",
                   x, ptr);
    }
    if (nx->visited) {
      ABSL_RAW_LOG(FATAL, "Did not clear visited marker on node %" PRIu32, x);
    }
    HASH_FOR_EACH(y, nx->out) {
      Node* ny = r->nodes_[static_cast<uint32_t>(y)];
      if (nx->rank >= ny->rank) {
        ABSL_RAW_LOG(FATAL,
                     "Edge %" PRIu32 " ->%" PRId32
                     " has bad rank assignment %" PRId64 "->%" PRId64,
                     x, y, nx->rank, ny->rank);
      }
    }
  }
  // The rank list holds each slot once, in increasing rank order, so ranks are
  // unique.
  const uint32_t num_slots = r->nodes_.size();
  uint32_t num_listed = 0;
  int32_t prev_slot = -1;
  for (int32_t slot = r->lowest_slot; slot != -1;
       slot = NodeAt(r, slot)->next_slot) {
    if (NodeAt(r, slot)->prev_slot != prev_slot) {
      ABSL_RAW_LOG(FATAL,
                   "Slot %" PRId32 " has prev_slot %" PRId32
                   " instead of %" PRId32,
                   slot, NodeAt(r, slot)->prev_slot, prev_slot);
    }
    if (prev_slot != -1 &&
        NodeAt(r, prev_slot)->rank >= NodeAt(r, slot)->rank) {
      ABSL_RAW_LOG(
          FATAL, "Rank of slot %" PRId32 " is not above rank of slot %" PRId32,
          slot, prev_slot);
    }
    if (++num_listed > num_slots) {
      ABSL_RAW_LOG(FATAL, "Rank list has more entries than there are slots");
    }
    prev_slot = slot;
  }
  if (prev_slot != r->highest_slot || num_listed != num_slots) {
    ABSL_RAW_LOG(FATAL, "Rank list has %" PRIu32 " of %" PRIu32 " slots",
                 num_listed, num_slots);
  }
  return true;
}

GraphId GraphCycles::GetId(void* ptr) {
  int32_t i = rep_->ptrmap_.Find(ptr);
  if (i != -1) {
    return MakeId(i, rep_->nodes_[static_cast<uint32_t>(i)]->version);
  } else if (rep_->free_nodes_.empty()) {
    const int32_t slot = static_cast<int32_t>(rep_->nodes_.size());
    Node* n =
        new (base_internal::LowLevelAlloc::AllocWithArena(sizeof(Node), arena))
            Node;
    n->version = 1;  // Avoid 0 since it is used by InvalidGraphId()
    n->visited = false;
    n->masked_ptr = base_internal::HidePtr(ptr);
    n->nstack = 0;
    n->priority = 0;
    rep_->nodes_.push_back(n);
    LinkBetween(rep_, slot, rep_->highest_slot, /*next_slot=*/-1);
    rep_->ptrmap_.Add(ptr, slot);
    return MakeId(slot, n->version);
  } else {
    // The slot keeps its rank (see "Rank allocation").
    int32_t slot = rep_->free_nodes_.back();
    rep_->free_nodes_.pop_back();
    Node* n = rep_->nodes_[static_cast<uint32_t>(slot)];
    n->masked_ptr = base_internal::HidePtr(ptr);
    n->nstack = 0;
    n->priority = 0;
    rep_->ptrmap_.Add(ptr, slot);
    return MakeId(slot, n->version);
  }
}

void GraphCycles::RemoveNode(void* ptr) {
  int32_t i = rep_->ptrmap_.Remove(ptr);
  if (i == -1) {
    return;
  }
  Node* x = rep_->nodes_[static_cast<uint32_t>(i)];
  HASH_FOR_EACH(y, x->out) {
    rep_->nodes_[static_cast<uint32_t>(y)]->in.erase(i);
  }
  HASH_FOR_EACH(y, x->in) {
    rep_->nodes_[static_cast<uint32_t>(y)]->out.erase(i);
  }
  x->in.clear();
  x->out.clear();
  x->masked_ptr = base_internal::HidePtr<void>(nullptr);
  if (x->version == std::numeric_limits<uint32_t>::max()) {
    // Cannot use x any more
  } else {
    x->version++;  // Invalidates all copies of node.
    rep_->free_nodes_.push_back(i);
  }
}

void* GraphCycles::Ptr(GraphId id) {
  Node* n = FindNode(rep_, id);
  return n == nullptr ? nullptr : base_internal::UnhidePtr<void>(n->masked_ptr);
}

bool GraphCycles::HasNode(GraphId node) {
  return FindNode(rep_, node) != nullptr;
}

bool GraphCycles::HasEdge(GraphId x, GraphId y) const {
  Node* xn = FindNode(rep_, x);
  return xn && FindNode(rep_, y) && xn->out.contains(NodeIndex(y));
}

void GraphCycles::RemoveEdge(GraphId x, GraphId y) {
  Node* xn = FindNode(rep_, x);
  Node* yn = FindNode(rep_, y);
  if (xn && yn) {
    xn->out.erase(NodeIndex(y));
    yn->in.erase(NodeIndex(x));
    // No need to update the rank assignment since a previous valid
    // rank assignment remains valid after an edge deletion.
  }
}

static bool ForwardDFS(GraphCycles::Rep* r, int32_t n, int64_t upper_bound);
static void BackwardDFS(GraphCycles::Rep* r, int32_t n, int64_t lower_bound);
static void Reorder(GraphCycles::Rep* r);
static void Sort(const Vec<Node*>&, Vec<int32_t>* delta);
static void MoveToList(GraphCycles::Rep* r, Vec<int32_t>* src,
                       Vec<int32_t>* dst);

// TryMoveXBelowY() and TryMoveYAboveX() are called by InsertEdge(x, y) when
// rank(x) > rank(y). They try to restore the rank order by moving only x below
// y, or only y above x, which is much cheaper than the Pearce-Kelly search and
// reorder.
//
// x can move below y if all of x's in-neighbors rank below y. This also shows
// that the new edge does not close a cycle: every node reachable from y ranks
// at or above y, so a path from y to x would have to enter x from such a node.
// Similarly, y can move above x if all of y's out-neighbors rank above x.
//
// Each returns false, changing nothing, if the move is not possible or if
// there are more than kMaxSingleNodeDegree neighbors to check.
static bool TryMoveXBelowY(GraphCycles::Rep* r, int32_t x, int32_t y) {
  Node* nx = NodeAt(r, x);
  Node* ny = NodeAt(r, y);
  int degree = 0;
  HASH_FOR_EACH(u, nx->in) {
    if (++degree > kMaxSingleNodeDegree) return false;
    if (NodeAt(r, u)->rank >= ny->rank) return false;
  }
  Unlink(r, x);
  if (degree == 0) {
    // Move x below all other nodes.
    LinkBetween(r, x, /*prev_slot=*/-1, r->lowest_slot);
  } else {
    // Move x right below y. Its in-neighbors rank below y, so they rank no
    // higher than the node right below y.
    LinkBetween(r, x, ny->prev_slot, y);
  }
  return true;
}

static bool TryMoveYAboveX(GraphCycles::Rep* r, int32_t x, int32_t y) {
  Node* nx = NodeAt(r, x);
  Node* ny = NodeAt(r, y);
  int degree = 0;
  HASH_FOR_EACH(w, ny->out) {
    if (++degree > kMaxSingleNodeDegree) return false;
    if (NodeAt(r, w)->rank <= nx->rank) return false;
  }
  Unlink(r, y);
  if (degree == 0) {
    // Move y above all other nodes.
    LinkBetween(r, y, r->highest_slot, /*next_slot=*/-1);
  } else {
    // Move y right above x. Its out-neighbors rank above x, so they rank no
    // lower than the node right above x.
    LinkBetween(r, y, x, nx->next_slot);
  }
  return true;
}

bool GraphCycles::InsertEdge(GraphId idx, GraphId idy) {
  Rep* r = rep_;
  const int32_t x = NodeIndex(idx);
  const int32_t y = NodeIndex(idy);
  Node* nx = FindNode(r, idx);
  Node* ny = FindNode(r, idy);
  if (nx == nullptr || ny == nullptr) return true;  // Expired ids

  if (nx == ny) return false;  // Self edge
  if (!nx->out.insert(y)) {
    // Edge already exists.
    return true;
  }

  ny->in.insert(x);

  if (nx->rank <= ny->rank) {
    // New edge is consistent with existing rank assignment.
    return true;
  }

  // Try moving just x or just y.
  if (TryMoveXBelowY(r, x, y) || TryMoveYAboveX(r, x, y)) return true;

  // Current rank assignments are incompatible with the new edge.  Recompute.
  // We only need to consider nodes that fall in the range [ny->rank,nx->rank].
  if (!ForwardDFS(r, y, nx->rank)) {
    // Found a cycle.  Undo the insertion and tell caller.
    nx->out.erase(y);
    ny->in.erase(x);
    // Since we do not call Reorder() on this path, clear any visited
    // markers left by ForwardDFS.
    for (const auto& d : r->deltaf_) {
      r->nodes_[static_cast<uint32_t>(d)]->visited = false;
    }
    return false;
  }
  BackwardDFS(r, x, ny->rank);
  Reorder(r);
  return true;
}

static bool ForwardDFS(GraphCycles::Rep* r, int32_t n, int64_t upper_bound) {
  // Avoid recursion since stack space might be limited.
  // We instead keep a stack of nodes to visit.
  r->deltaf_.clear();
  r->stack_.clear();
  r->stack_.push_back(n);
  while (!r->stack_.empty()) {
    n = r->stack_.back();
    r->stack_.pop_back();
    Node* nn = r->nodes_[static_cast<uint32_t>(n)];
    if (nn->visited) continue;

    nn->visited = true;
    r->deltaf_.push_back(n);

    HASH_FOR_EACH(w, nn->out) {
      Node* nw = r->nodes_[static_cast<uint32_t>(w)];
      if (nw->rank == upper_bound) {
        return false;  // Cycle
      }
      if (!nw->visited && nw->rank < upper_bound) {
        r->stack_.push_back(w);
      }
    }
  }
  return true;
}

static void BackwardDFS(GraphCycles::Rep* r, int32_t n, int64_t lower_bound) {
  r->deltab_.clear();
  r->stack_.clear();
  r->stack_.push_back(n);
  while (!r->stack_.empty()) {
    n = r->stack_.back();
    r->stack_.pop_back();
    Node* nn = r->nodes_[static_cast<uint32_t>(n)];
    if (nn->visited) continue;

    nn->visited = true;
    r->deltab_.push_back(n);

    HASH_FOR_EACH(w, nn->in) {
      Node* nw = r->nodes_[static_cast<uint32_t>(w)];
      if (!nw->visited && lower_bound < nw->rank) {
        r->stack_.push_back(w);
      }
    }
  }
}

static void Reorder(GraphCycles::Rep* r) {
  Sort(r->nodes_, &r->deltab_);
  Sort(r->nodes_, &r->deltaf_);

  // The slots to reorder, sorted by their current ranks. In their new order
  // (list_), they take over the same ranks and places in the rank list.
  const uint32_t count = r->deltab_.size() + r->deltaf_.size();
  r->sorted_slots.resize(count);
  std::merge(r->deltab_.begin(), r->deltab_.end(), r->deltaf_.begin(),
             r->deltaf_.end(), r->sorted_slots.begin(),
             [r](int32_t a, int32_t b) {
               return NodeAt(r, a)->rank < NodeAt(r, b)->rank;
             });

  // Adds contents of delta lists to list_ (backwards deltas first).
  r->list_.clear();
  MoveToList(r, &r->deltab_, &r->list_);
  MoveToList(r, &r->deltaf_, &r->list_);

  // Record the rank, prev_slot and next_slot of each place before changing
  // them.
  r->sorted_ranks.resize(count);
  r->old_prev_slots.resize(count);
  r->old_next_slots.resize(count);
  for (uint32_t i = 0; i < count; i++) {
    const Node* p = NodeAt(r, r->sorted_slots[i]);
    r->sorted_ranks[i] = p->rank;
    r->old_prev_slots[i] = p->prev_slot;
    r->old_next_slots[i] = p->next_slot;
  }

  // Assign the ranks and places in order to the collected list. A prev_slot
  // (or next_slot) that is also being reordered must be the previous (or next)
  // place in sorted_slots, so link to the slot that now takes that place
  // instead.
  for (uint32_t i = 0; i < count; i++) {
    const int32_t slot = r->list_[i];
    NodeAt(r, slot)->rank = r->sorted_ranks[i];
    if (i > 0 && r->old_prev_slots[i] == r->sorted_slots[i - 1]) {
      Link(r, r->list_[i - 1], slot);
    } else {
      Link(r, r->old_prev_slots[i], slot);
    }
    if (i + 1 == count || r->old_next_slots[i] != r->sorted_slots[i + 1]) {
      Link(r, slot, r->old_next_slots[i]);
    }
  }
}

static void Sort(const Vec<Node*>& nodes, Vec<int32_t>* delta) {
  struct ByRank {
    const Vec<Node*>* nodes;
    bool operator()(int32_t a, int32_t b) const {
      return (*nodes)[static_cast<uint32_t>(a)]->rank <
             (*nodes)[static_cast<uint32_t>(b)]->rank;
    }
  };
  ByRank cmp;
  cmp.nodes = &nodes;
  absl::c_sort(*delta, cmp);
}

static void MoveToList(GraphCycles::Rep* r, Vec<int32_t>* src,
                       Vec<int32_t>* dst) {
  for (int32_t w : *src) {
    // Prepare for future DFS calls
    r->nodes_[static_cast<uint32_t>(w)]->visited = false;
    dst->push_back(w);
  }
}

int GraphCycles::FindPath(GraphId idx, GraphId idy, int max_path_len,
                          GraphId path[]) const {
  Rep* r = rep_;
  if (FindNode(r, idx) == nullptr || FindNode(r, idy) == nullptr) return 0;
  const int32_t x = NodeIndex(idx);
  const int32_t y = NodeIndex(idy);

  // Forward depth first search starting at x until we hit y.
  // As we descend into a node, we push it onto the path.
  // As we leave a node, we remove it from the path.
  int path_len = 0;

  NodeSet seen;
  r->stack_.clear();
  r->stack_.push_back(x);
  while (!r->stack_.empty()) {
    int32_t n = r->stack_.back();
    r->stack_.pop_back();
    if (n < 0) {
      // Marker to indicate that we are leaving a node
      path_len--;
      continue;
    }

    if (path_len < max_path_len) {
      path[path_len] =
          MakeId(n, rep_->nodes_[static_cast<uint32_t>(n)]->version);
    }
    path_len++;
    r->stack_.push_back(-1);  // Will remove tentative path entry

    if (n == y) {
      return path_len;
    }

    HASH_FOR_EACH(w, r->nodes_[static_cast<uint32_t>(n)]->out) {
      if (seen.insert(w)) {
        r->stack_.push_back(w);
      }
    }
  }

  return 0;
}

bool GraphCycles::IsReachable(GraphId x, GraphId y) const {
  return FindPath(x, y, 0, nullptr) > 0;
}

void GraphCycles::UpdateStackTrace(GraphId id, int priority,
                                   int (*get_stack_trace)(void** stack, int)) {
  Node* n = FindNode(rep_, id);
  if (n == nullptr || n->priority >= priority) {
    return;
  }
  n->nstack =
      (*get_stack_trace)(n->stack, static_cast<int>(std::size(n->stack)));
  n->priority = priority;
}

int GraphCycles::GetStackTrace(GraphId id, void*** ptr) {
  Node* n = FindNode(rep_, id);
  if (n == nullptr) {
    *ptr = nullptr;
    return 0;
  } else {
    *ptr = n->stack;
    return n->nstack;
  }
}

}  // namespace synchronization_internal
ABSL_NAMESPACE_END
}  // namespace absl

#endif  // ABSL_LOW_LEVEL_ALLOC_MISSING
