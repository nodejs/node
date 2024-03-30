// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_TURBOSHAFT_LATE_LOAD_ELIMINATION_REDUCER_H_
#define V8_COMPILER_TURBOSHAFT_LATE_LOAD_ELIMINATION_REDUCER_H_

#include "src/base/doubly-threaded-list.h"
#include "src/compiler/turboshaft/analyzer-iterator.h"
#include "src/compiler/turboshaft/assembler.h"
#include "src/compiler/turboshaft/graph.h"
#include "src/compiler/turboshaft/loop-finder.h"
#include "src/compiler/turboshaft/phase.h"
#include "src/compiler/turboshaft/snapshot-table-opindex.h"
#include "src/compiler/turboshaft/utils.h"
#include "src/zone/zone.h"

namespace v8::internal::compiler::turboshaft {

#include "src/compiler/turboshaft/define-assembler-macros.inc"

// Design doc:
// https://docs.google.com/document/d/1AEl4dATNLu8GlLyUBQFXJoCxoAT5BeG7RCWxoEtIBJE/edit?usp=sharing

// Load Elimination removes redundant loads. Loads can be redundant because:
//
//   - they follow a store to the same address. For instance:
//
//       x.a = 42;
//       y = x.a;
//
//   - or, they follow the same load. For instance:
//
//       y = x.a;
//       z = x.a;
//
// The "annoying" part of load elimination is that object can alias, and stores
// to dynamically computed indices tend to invalidate the whole state. For
// instance, if we don't know anything about aliasing regarding `a` and `b`,
// then, in this situation:
//
//     x.a = 42
//     y.a = 25
//     z = x.a
//
// We can't load-eliminate `z = x.a`, since `y` could alias with `x`, and `y.a =
// 25` could have overwritten `x.a`. Similarly, if we have something like:
//
//     x[0] = 42
//     y[i] = 25
//     z = x[0]
//
// We can't load-eliminate `z = x[0]`, since `y` could alias with `x`, and
// `y[i]` thus have overwritten `x[0]`.
//
//
// Implementation:
//
//   - In a `MemoryContentTable` (a SnapshotTable), we keep track of known
//     memory values.
//     * When we visit a Store:
//       + if it's to a constant offset, we invalidate all of the known values
//         at the same offset (for all bases).
//       + if it's to a dynamic index, we invalidate everything (because things
//         could alias).
//       We then update the known value at the address of the store.
//     * When we visit a Call, we invalidate everything (since the function
//       called could change any memory through aliases).
//     * When we visit a Load:
//       + if there is a known value at the address, we replace the Load by this
//         value.
//       + otherwise, the result of the Load becomes the known value at the load
//         address.
//
//   - We keep track (using a SparseOpIndexSnapshotTable) of some objects that
//     are known to not alias with anything: freshly allocated objects, until
//     they are passed to a function call, stored in an object, or flow in a
//     Phi. When storing in a fresh object, we only need to invalidate things in
//     the same object, leaving the rest of the state untouched. When storing in
//     a non-fresh object, we don't invalidate the state for fresh objects.
//
//   - We keep track (using a SparseOpIndexSnapshotTable) of the maps of some
//     objects (which we get from AssumeMap operations, which are inserted when
//     lowering CheckMaps). We use them to know if some objects can alias or
//     not: 2 objects with different maps cannot alias.
//
//   - When a loop contains a Store or a Call, it could invalidate previously
//     eliminated loads in the beginning of the loop. Thus, once we reach the
//     end of a loop, we recompute the header's snapshot using {header,
//     backedge} as predecessors, and if anything is invalidated by the
//     backedge, we revisit the loop.
//
// How we "keep track" of objects:
//
// We need the following operation:
//     1. Load the value for a {base, index, offset}.
//     2. Store that {base, index, offset} = value
//     3. Invalidate everything at a given offset + everything at an index (for
//        when storing to a base that could alias with other things).
//     4. Invalidate everything in a base (for when said base is passed to a
//        function, or when there is an indexed store in this base).
//     5. Invalidate everything (for an indexed store into an arbitrary base)
//
// To have 1. in constant time, we maintain a global hashmap (`all_keys`) from
// MemoryAddress (= {base, index, offset, element_size_log2, size}) to Keys, and
// from these Keys, we have constant-time lookup in the SnapshotTable.
// To have 3. efficiently, we maintain a Map from offsets to lists of every
// MemoryAddress at this offset (`offset_keys_`).
// To have 4. efficiently, we have a similar map from bases to lists of every
// MemoryAddress at this base (`base_keys_`).
// For 5., we can use either `offset_keys_` or `base_keys_`. In practice, we use
// the latter because it allows us to efficiently skip bases that are known to
// have no aliases.

// MapMask and related functions are an attempt to avoid having to store sets of
// maps for each AssumeMap that we encounter by compressing all of the maps into
// a single uint64_t.
//
// For each object, we keep in a MapMaskAndOr the "minimum" and "maximum" of
// all of its potential maps, where
//   - "or_" is computed using the union (logical or) of all of its potential
//     maps.
//   - "and_" is computed using the intersection (logical and) of all of its
//     potential maps.
//
// Then, given two objects A and B, if A.and_ has a bit set that isn't set in
// B.or_, it means that all of the maps of A have a bit that none of the maps of
// B have, ie, A and B are guaranteed to not have a map in common.
using MapMask = uint64_t;
struct MapMaskAndOr {
  MapMask or_ = 0;
  MapMask and_ = -1ull;

  bool operator==(const MapMaskAndOr& other) const {
    return or_ == other.or_ && and_ == other.and_;
  }

  bool operator!=(const MapMaskAndOr& other) const { return !(*this == other); }
};
inline bool is_empty(MapMaskAndOr minmax) {
  return minmax.or_ == 0 && minmax.and_ == -1ull;
}
inline MapMask ComputeMapHash(MapRef map) {
  // `map.hash_value()` is probably not a good enough hash, since most user maps
  // will have the same upper bits, so we re-hash. We're using xorshift64* (from
  // "An experimental exploration of Marsaglia’s xorshift generators, scrambled"
  // by Vigna in ACM Transactions on Mathematical Software, Volume 42).
  MapMask hash = map.hash_value();
  hash ^= hash >> 12;
  hash ^= hash << 25;
  hash ^= hash >> 27;
  return hash * 0x2545f4914f6cdd1d;
}
inline MapMaskAndOr ComputeMinMaxHash(ZoneRefSet<Map> maps) {
  MapMaskAndOr minmax;
  for (MapRef map : maps) {
    MapMask hash = ComputeMapHash(map);
    minmax.or_ |= hash;
    minmax.and_ &= hash;
  }
  return minmax;
}
inline MapMaskAndOr CombineMinMax(MapMaskAndOr a, MapMaskAndOr b) {
  return {a.or_ | b.or_, a.and_ & b.and_};
}
// Returns true if {a} and {b} could have a map in common.
inline bool CouldHaveSameMap(MapMaskAndOr a, MapMaskAndOr b) {
  return ((a.and_ & b.or_) == a.and_) || ((b.and_ & a.or_) == b.and_);
}

struct MemoryAddress {
  OpIndex base;
  OptionalOpIndex index;
  int32_t offset;
  uint8_t element_size_log2;
  uint8_t size;

  bool operator==(const MemoryAddress& other) const {
    return base == other.base && index == other.index &&
           offset == other.offset &&
           element_size_log2 == other.element_size_log2 && size == other.size;
  }

  template <typename H>
  friend H AbslHashValue(H h, const MemoryAddress& mem) {
    return H::combine(std::move(h), mem.base, mem.index, mem.offset,
                      mem.element_size_log2, mem.size);
  }
};

inline size_t hash_value(MemoryAddress const& mem) {
  return fast_hash_combine(mem.base, mem.index, mem.offset,
                           mem.element_size_log2, mem.size);
}

struct KeyData {
  using Key = SnapshotTableKey<OpIndex, KeyData>;
  MemoryAddress mem;
  // Pointers to the previous and the next Keys at the same base.
  Key* prev_same_base = nullptr;
  Key next_same_base = {};
  // Pointers to either the next/previous Keys at the same offset.
  Key* prev_same_offset = nullptr;
  Key next_same_offset = {};
};

struct OffsetListTraits {
  using T = SnapshotTable<OpIndex, KeyData>::Key;
  static T** prev(T t) { return &(t.data().prev_same_offset); }
  static T* next(T t) { return &(t.data().next_same_offset); }
  static bool non_empty(T t) { return t.valid(); }
};

struct BaseListTraits {
  using T = SnapshotTable<OpIndex, KeyData>::Key;
  static T** prev(T t) { return &(t.data().prev_same_base); }
  static T* next(T t) { return &(t.data().next_same_base); }
  static bool non_empty(T t) { return t.valid(); }
};

struct BaseData {
  using Key = SnapshotTable<OpIndex, KeyData>::Key;
  // List of every value at this base that has an offset rather than an index.
  v8::base::DoublyThreadedList<Key, BaseListTraits> with_offsets;
  // List of every value at this base that has a valid index.
  v8::base::DoublyThreadedList<Key, BaseListTraits> with_indices;
};

class MemoryContentTable
    : public ChangeTrackingSnapshotTable<MemoryContentTable, OpIndex, KeyData> {
 public:
  explicit MemoryContentTable(
      Zone* zone, SparseOpIndexSnapshotTable<bool>& non_aliasing_objects,
      SparseOpIndexSnapshotTable<MapMaskAndOr>& object_maps,
      FixedOpIndexSidetable<OpIndex>& replacements)
      : ChangeTrackingSnapshotTable(zone),
        non_aliasing_objects_(non_aliasing_objects),
        object_maps_(object_maps),
        replacements_(replacements),
        all_keys_(zone),
        base_keys_(zone),
        offset_keys_(zone) {}

  void OnNewKey(Key key, OpIndex value) {
    if (value.valid()) {
      AddKeyInBaseOffsetMaps(key);
    }
  }

  void OnValueChange(Key key, OpIndex old_value, OpIndex new_value) {
    DCHECK_NE(old_value, new_value);
    if (old_value.valid() && !new_value.valid()) {
      RemoveKeyFromBaseOffsetMaps(key);
    } else if (new_value.valid() && !old_value.valid()) {
      AddKeyInBaseOffsetMaps(key);
    } else {
      DCHECK_EQ(new_value.valid(), old_value.valid());
    }
  }

  // Invalidate all previous known memory that could alias with {store}.
  void Invalidate(const StoreOp& store) {
    Invalidate(store.base(), store.index(), store.offset);
  }

  void Invalidate(OpIndex base, OptionalOpIndex index, int32_t offset) {
    base = ResolveBase(base);

    if (non_aliasing_objects_.Get(base)) {
      // Since {base} is non-aliasing, it's enough to just iterate the values at
      // this base.
      auto base_keys = base_keys_.find(base);
      if (base_keys == base_keys_.end()) return;
      for (auto it = base_keys->second.with_offsets.begin();
           it != base_keys->second.with_offsets.end();) {
        Key key = *it;
        DCHECK_EQ(key.data().mem.base, base);
        DCHECK(!key.data().mem.index.valid());
        if (index.valid() || offset == key.data().mem.offset) {
          // Overwrites {key}.
          it = base_keys->second.with_offsets.RemoveAt(it);
          Set(key, OpIndex::Invalid());
        } else {
          ++it;
        }
      }
      // Invalidating all of the value with valid Index at base {base}.
      for (auto it = base_keys->second.with_indices.begin();
           it != base_keys->second.with_indices.end();) {
        Key key = *it;
        DCHECK(key.data().mem.index.valid());
        it = base_keys->second.with_indices.RemoveAt(it);
        Set(key, OpIndex::Invalid());
      }
    } else {
      // {base} could alias with other things, so we iterate the whole state.
      if (index.valid()) {
        // {index} could be anything, so we invalidate everything.
        return InvalidateMaybeAliasing();
      }

      // Invalidating all of the values with valid Index.
      // TODO(dmercadier): we could keep keys that don't alias here, but that
      // would require doing a map lookup on the base of each key. A better
      // alternative would probably be to have 2 {non_alias_index_keys_} and
      // {maybe_alias_index_keys_} tables instead of just {index_keys_}. This
      // has the downside that when a base stops being non-alias, all of its
      // indexed memory cells have to be moved. This could be worked around by
      // having these 2 tables contain BaseData.with_indices values instead of
      // Keys, so that a whole BaseData.with_indices can be removed in a single
      // operation from the global {non_alias_index_keys_}.
      for (auto it = index_keys_.begin(); it != index_keys_.end();) {
        Key key = *it;
        it = index_keys_.RemoveAt(it);
        Set(key, OpIndex::Invalid());
      }

      InvalidateAtOffset(offset, base);
    }
  }

  // Invalidates all Keys that are not known as non-aliasing.
  void InvalidateMaybeAliasing() {
    // We find current active keys through {base_keys_} so that we can bail out
    // for whole buckets non-aliasing bases (if we had gone through
    // {offset_keys_} instead, then for each key we would've had to check
    // whether it was non-aliasing or not).
    for (auto& base_keys : base_keys_) {
      OpIndex base = base_keys.first;
      if (non_aliasing_objects_.Get(base)) continue;
      for (auto it = base_keys.second.with_offsets.begin();
           it != base_keys.second.with_offsets.end();) {
        Key key = *it;
        // It's important to remove with RemoveAt before Setting the key to
        // invalid, otherwise OnKeyChange will remove {key} from {base_keys},
        // which will invalidate {it}.
        it = base_keys.second.with_offsets.RemoveAt(it);
        Set(key, OpIndex::Invalid());
      }
      for (auto it = base_keys.second.with_indices.begin();
           it != base_keys.second.with_indices.end();) {
        Key key = *it;
        it = base_keys.second.with_indices.RemoveAt(it);
        Set(key, OpIndex::Invalid());
      }
    }
  }

  OpIndex Find(const LoadOp& load) {
    OpIndex base = ResolveBase(load.base());
    OptionalOpIndex index = load.index();
    int32_t offset = load.offset;
    uint8_t element_size_log2 = index.valid() ? load.element_size_log2 : 0;
    uint8_t size = load.loaded_rep.SizeInBytes();

    MemoryAddress mem{base, index, offset, element_size_log2, size};
    auto key = all_keys_.find(mem);
    if (key == all_keys_.end()) return OpIndex::Invalid();
    return Get(key->second);
  }

  void Insert(const StoreOp& store) {
    OpIndex base = ResolveBase(store.base());
    OptionalOpIndex index = store.index();
    int32_t offset = store.offset;
    uint8_t element_size_log2 = index.valid() ? store.element_size_log2 : 0;
    OpIndex value = store.value();
    uint8_t size = store.stored_rep.SizeInBytes();

    if (store.kind.is_immutable) {
      InsertImmutable(base, index, offset, element_size_log2, size, value);
    } else {
      Insert(base, index, offset, element_size_log2, size, value);
    }
  }

  void Insert(const LoadOp& load, OpIndex load_idx) {
    OpIndex base = ResolveBase(load.base());
    OptionalOpIndex index = load.index();
    int32_t offset = load.offset;
    uint8_t element_size_log2 = index.valid() ? load.element_size_log2 : 0;
    uint8_t size = load.loaded_rep.SizeInBytes();

    if (load.kind.is_immutable) {
      InsertImmutable(base, index, offset, element_size_log2, size, load_idx);
    } else {
      Insert(base, index, offset, element_size_log2, size, load_idx);
    }
  }

#ifdef DEBUG
  void Print() {
    std::cout << "MemoryContentTable:\n";
    for (const auto& base_keys : base_keys_) {
      for (Key key : base_keys.second.with_offsets) {
        std::cout << "  * " << key.data().mem.base << " - "
                  << key.data().mem.index << " - " << key.data().mem.offset
                  << " - " << key.data().mem.element_size_log2 << " ==> "
                  << Get(key) << "\n";
      }
      for (Key key : base_keys.second.with_indices) {
        std::cout << "  * " << key.data().mem.base << " - "
                  << key.data().mem.index << " - " << key.data().mem.offset
                  << " - " << key.data().mem.element_size_log2 << " ==> "
                  << Get(key) << "\n";
      }
    }
  }
#endif

 private:
  // To avoid pathological execution times, we cap the maximum number of
  // keys we track. This is safe, because *not* tracking objects (even
  // though we could) only makes us miss out on possible optimizations.
  // TODO(dmercadier/jkummerow): Find a more elegant solution to keep
  // execution time in check. One example of a test case can be found in
  // crbug.com/v8/14370.
  static constexpr size_t kMaxKeys = 10000;

  void Insert(OpIndex base, OptionalOpIndex index, int32_t offset,
              uint8_t element_size_log2, uint8_t size, OpIndex value) {
    DCHECK_EQ(base, ResolveBase(base));

    MemoryAddress mem{base, index, offset, element_size_log2, size};
    auto existing_key = all_keys_.find(mem);
    if (existing_key != all_keys_.end()) {
      Set(existing_key->second, value);
      return;
    }

    if (all_keys_.size() > kMaxKeys) return;

    // Creating a new key.
    Key key = NewKey({mem});
    all_keys_.insert({mem, key});
    Set(key, value);
  }

  void InsertImmutable(OpIndex base, OptionalOpIndex index, int32_t offset,
                       uint8_t element_size_log2, uint8_t size, OpIndex value) {
    DCHECK_EQ(base, ResolveBase(base));

    MemoryAddress mem{base, index, offset, element_size_log2, size};
    auto existing_key = all_keys_.find(mem);
    if (existing_key != all_keys_.end()) {
      SetNoNotify(existing_key->second, value);
      return;
    }

    if (all_keys_.size() > kMaxKeys) return;

    // Creating a new key.
    Key key = NewKey({mem});
    all_keys_.insert({mem, key});
    // Call `SetNoNotify` to avoid calls to `OnNewKey` and `OnValueChanged`.
    SetNoNotify(key, value);
  }

  void InvalidateAtOffset(int32_t offset, OpIndex base) {
    MapMaskAndOr base_maps = object_maps_.Get(base);
    auto offset_keys = offset_keys_.find(offset);
    if (offset_keys == offset_keys_.end()) return;
    for (auto it = offset_keys->second.begin();
         it != offset_keys->second.end();) {
      Key key = *it;
      DCHECK_EQ(offset, key.data().mem.offset);
      // It can overwrite previous stores to any base (except non-aliasing
      // ones).
      if (non_aliasing_objects_.Get(key.data().mem.base)) {
        ++it;
        continue;
      }
      MapMaskAndOr this_maps = key.data().mem.base == base
                                   ? base_maps
                                   : object_maps_.Get(key.data().mem.base);
      if (!is_empty(base_maps) && !is_empty(this_maps) &&
          !CouldHaveSameMap(base_maps, this_maps)) {
        ++it;
        continue;
      }
      it = offset_keys->second.RemoveAt(it);
      Set(key, OpIndex::Invalid());
    }
  }

  OpIndex ResolveBase(OpIndex base) {
    while (replacements_[base] != OpIndex::Invalid()) {
      base = replacements_[base];
    }
    return base;
  }

  void AddKeyInBaseOffsetMaps(Key key) {
    // Inserting in {base_keys_}.
    OpIndex base = key.data().mem.base;
    auto base_keys = base_keys_.find(base);
    if (base_keys != base_keys_.end()) {
      if (key.data().mem.index.valid()) {
        base_keys->second.with_indices.PushFront(key);
      } else {
        base_keys->second.with_offsets.PushFront(key);
      }
    } else {
      BaseData data;
      if (key.data().mem.index.valid()) {
        data.with_indices.PushFront(key);
      } else {
        data.with_offsets.PushFront(key);
      }
      base_keys_.insert({base, std::move(data)});
    }

    if (key.data().mem.index.valid()) {
      // Inserting in {index_keys_}.
      index_keys_.PushFront(key);
    } else {
      // Inserting in {offset_keys_}.
      int offset = key.data().mem.offset;
      auto offset_keys = offset_keys_.find(offset);
      if (offset_keys != offset_keys_.end()) {
        offset_keys->second.PushFront(key);
      } else {
        v8::base::DoublyThreadedList<Key, OffsetListTraits> list;
        list.PushFront(key);
        offset_keys_.insert({offset, std::move(list)});
      }
    }
  }

  void RemoveKeyFromBaseOffsetMaps(Key key) {
    // Removing from {base_keys_}.
    v8::base::DoublyThreadedList<Key, BaseListTraits>::Remove(key);
    v8::base::DoublyThreadedList<Key, OffsetListTraits>::Remove(key);
  }

  SparseOpIndexSnapshotTable<bool>& non_aliasing_objects_;
  SparseOpIndexSnapshotTable<MapMaskAndOr>& object_maps_;
  FixedOpIndexSidetable<OpIndex>& replacements_;

  // A map containing all of the keys, for fast lookup of a specific
  // MemoryAddress.
  ZoneAbslFlatHashMap<MemoryAddress, Key> all_keys_;
  // Map from base OpIndex to keys associated with this base.
  ZoneAbslFlatHashMap<OpIndex, BaseData> base_keys_;
  // Map from offsets to keys associated with this offset.
  ZoneAbslFlatHashMap<int, v8::base::DoublyThreadedList<Key, OffsetListTraits>>
      offset_keys_;

  // List of all of the keys that have a valid index.
  v8::base::DoublyThreadedList<Key, OffsetListTraits> index_keys_;
};

class LateLoadEliminationAnalyzer {
 public:
  using AliasTable = SparseOpIndexSnapshotTable<bool>;
  using AliasKey = AliasTable::Key;
  using AliasSnapshot = AliasTable::Snapshot;

  using MapTable = SparseOpIndexSnapshotTable<MapMaskAndOr>;
  using MapKey = MapTable::Key;
  using MapSnapshot = MapTable::Snapshot;

  using MemoryKey = MemoryContentTable::Key;
  using MemorySnapshot = MemoryContentTable::Snapshot;

  enum class RawBaseAssumption {
    kNoInnerPointer,
    kMaybeInnerPointer,
  };

  LateLoadEliminationAnalyzer(Graph& graph, Zone* phase_zone,
                              JSHeapBroker* broker,
                              RawBaseAssumption raw_base_assumption)
      : graph_(graph),
        phase_zone_(phase_zone),
        broker_(broker),
        raw_base_assumption_(raw_base_assumption),
        replacements_(graph.op_id_count(), phase_zone, &graph),
        non_aliasing_objects_(phase_zone),
        object_maps_(phase_zone),
        memory_(phase_zone, non_aliasing_objects_, object_maps_, replacements_),
        block_to_snapshot_mapping_(graph.block_count(), phase_zone),
        predecessor_alias_snapshots_(phase_zone),
        predecessor_maps_snapshots_(phase_zone),
        predecessor_memory_snapshots_(phase_zone) {}

  void Run() {
    LoopFinder loop_finder(phase_zone_, &graph_);
    AnalyzerIterator iterator(phase_zone_, graph_, loop_finder);

    bool compute_start_snapshot = true;
    while (iterator.HasNext()) {
      const Block* block = iterator.Next();

      ProcessBlock(*block, compute_start_snapshot);
      compute_start_snapshot = true;

      // Consider re-processing for loops.
      if (const GotoOp* last = block->LastOperation(graph_).TryCast<GotoOp>()) {
        if (last->destination->IsLoop() &&
            last->destination->LastPredecessor() == block) {
          const Block* loop_header = last->destination;
          // {block} is the backedge of a loop. We recompute the loop header's
          // initial snapshots, and if they differ from its original snapshot,
          // then we revisit the loop.
          if (BeginBlock<true>(loop_header)) {
            // We set the snapshot of the loop's 1st predecessor to the newly
            // computed snapshot. It's not quite correct, but this predecessor
            // is guaranteed to end with a Goto, and we are now visiting the
            // loop, which means that we don't really care about this
            // predecessor anymore.
            // The reason for saving this snapshot is to prevent infinite
            // looping, since the next time we reach this point, the backedge
            // snapshot could still invalidate things from the forward edge
            // snapshot. By restricting the forward edge snapshot, we prevent
            // this.
            const Block* loop_1st_pred =
                loop_header->LastPredecessor()->NeighboringPredecessor();
            FinishBlock(loop_1st_pred);
            // And we start a new fresh snapshot from this predecessor.
            auto pred_snapshots =
                block_to_snapshot_mapping_[loop_1st_pred->index()];
            non_aliasing_objects_.StartNewSnapshot(
                pred_snapshots->alias_snapshot);
            object_maps_.StartNewSnapshot(pred_snapshots->maps_snapshot);
            memory_.StartNewSnapshot(pred_snapshots->memory_snapshot);

            iterator.MarkLoopForRevisit();
            compute_start_snapshot = false;
          } else {
            SealAndDiscard();
          }
        }
      }
    }
  }

  OpIndex Replacement(OpIndex index) {
    DCHECK(graph_.Get(index).Is<LoadOp>());
    return replacements_[index];
  }

 private:
  void ProcessBlock(const Block& block, bool compute_start_snapshot);
  void ProcessLoad(OpIndex op_idx, const LoadOp& op);
  void ProcessStore(OpIndex op_idx, const StoreOp& op);
  void ProcessAllocate(OpIndex op_idx, const AllocateOp& op);
  void ProcessCall(OpIndex op_idx, const CallOp& op);
  void ProcessPhi(OpIndex op_idx, const PhiOp& op);
  void ProcessAssumeMap(OpIndex op_idx, const AssumeMapOp& op);

  // BeginBlock initializes the various SnapshotTables for {block}, and returns
  // true if {block} is a loop that should be revisited.
  template <bool for_loop_revisit = false>
  bool BeginBlock(const Block* block);
  void FinishBlock(const Block* block);
  // Seals the current snapshot, but discards it. This is used when considering
  // whether a loop should be revisited or not: we recompute the loop header's
  // snapshots, and then revisit the loop if the snapshots contain
  // modifications. If the snapshots are unchanged, we discard them and don't
  // revisit the loop.
  void SealAndDiscard();
  void StoreLoopSnapshotInForwardPredecessor(const Block& loop_header);

  // Returns true if the loop's backedge already has snapshot data (meaning that
  // it was already visited).
  bool BackedgeHasSnapshot(const Block& loop_header) const;

  void InvalidateIfAlias(OpIndex op_idx);

  Graph& graph_;
  Zone* phase_zone_;
  JSHeapBroker* broker_;
  RawBaseAssumption raw_base_assumption_;

#if V8_ENABLE_WEBASSEMBLY
  bool is_wasm_ = PipelineData::Get().is_wasm();
#endif

  FixedOpIndexSidetable<OpIndex> replacements_;

  // TODO(dmercadier): {non_aliasing_objects_} tends to be weak for
  // backing-stores, because they are often stored into an object right after
  // being created, and often don't have other aliases throughout their
  // lifetime. It would be more useful to have a more precise tracking of
  // aliases. Storing a non-aliasing object into a potentially-aliasing one
  // probably always means that the former becomes potentially-aliasing.
  // However, storing a non-aliasing object into another non-aliasing object
  // should be reasonably not-too-hard to track.
  AliasTable non_aliasing_objects_;
  MapTable object_maps_;
  MemoryContentTable memory_;

  struct Snapshot {
    AliasSnapshot alias_snapshot;
    MapSnapshot maps_snapshot;
    MemorySnapshot memory_snapshot;
  };
  FixedBlockSidetable<base::Optional<Snapshot>> block_to_snapshot_mapping_;

  // {predecessor_alias_napshots_}, {predecessor_maps_snapshots_} and
  // {predecessor_memory_snapshots_} are used as temporary vectors when starting
  // to process a block. We store them as members to avoid reallocation.
  ZoneVector<AliasSnapshot> predecessor_alias_snapshots_;
  ZoneVector<MapSnapshot> predecessor_maps_snapshots_;
  ZoneVector<MemorySnapshot> predecessor_memory_snapshots_;
};

template <class Next>
class LateLoadEliminationReducer : public Next {
 public:
  TURBOSHAFT_REDUCER_BOILERPLATE()

  void Analyze() {
    if (is_wasm_ || v8_flags.turboshaft_load_elimination) {
      DCHECK(AllowHandleDereference::IsAllowed());
      analyzer_.Run();
    }
    Next::Analyze();
  }

  OpIndex REDUCE_INPUT_GRAPH(Load)(OpIndex ig_index, const LoadOp& load) {
    if (is_wasm_ || v8_flags.turboshaft_load_elimination) {
      OpIndex ig_replacement_index = analyzer_.Replacement(ig_index);
      if (ig_replacement_index.valid()) {
        OpIndex replacement = Asm().MapToNewGraph(ig_replacement_index);
        DCHECK(Asm()
                   .output_graph()
                   .Get(replacement)
                   .outputs_rep()[0]
                   .AllowImplicitRepresentationChangeTo(load.outputs_rep()[0]));
        return replacement;
      }
    }
    return Next::ReduceInputGraphLoad(ig_index, load);
  }

  OpIndex REDUCE(AssumeMap)(OpIndex, ZoneRefSet<Map>) {
    // AssumeMaps are currently not used after Load Elimination. We thus remove
    // them now. If they ever become needed for later optimizations, we could
    // consider leaving them in the graph and just ignoring them in the
    // Instruction Selector.
    return OpIndex::Invalid();
  }

 private:
  const bool is_wasm_ = PipelineData::Get().is_wasm();
  using RawBaseAssumption = LateLoadEliminationAnalyzer::RawBaseAssumption;
  RawBaseAssumption raw_base_assumption_ =
      PipelineData::Get().pipeline_kind() == TurboshaftPipelineKind::kCSA
          ? RawBaseAssumption::kMaybeInnerPointer
          : RawBaseAssumption::kNoInnerPointer;
  LateLoadEliminationAnalyzer analyzer_{
      Asm().modifiable_input_graph(), Asm().phase_zone(),
      PipelineData::Get().broker(), raw_base_assumption_};
};

#include "src/compiler/turboshaft/undef-assembler-macros.inc"

}  // namespace v8::internal::compiler::turboshaft

#endif  // V8_COMPILER_TURBOSHAFT_LATE_LOAD_ELIMINATION_REDUCER_H_
