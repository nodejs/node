// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_TURBOSHAFT_SNAPSHOT_TABLE_H_
#define V8_COMPILER_TURBOSHAFT_SNAPSHOT_TABLE_H_

#include <iostream>
#include <limits>

#include "src/base/iterator.h"
#include "src/base/small-vector.h"
#include "src/zone/zone-containers.h"

// A `SnapshotTable` stores a mapping from keys to values and creates snapshots,
// which capture the current state efficiently and allow us to return to a
// previous snapshot later. It is opimized for the case where we switch between
// similar snapshots with a closeby common ancestor.
//
// Complexity:
//   creating a scope   linear in the number of `Set` operations between the
//                      current state and the common ancestor of all
//                      predecessors and the current state, plus the `Set`
//                      operations from the common ancestor to all predecessors.
//   Scope::Get()       O(1)
//   Scope::Set()       O(1) + operator== for Value
//   Scope::Seal()      O(1)
//   NewKey()           O(1)
namespace v8::internal::compiler::turboshaft {

struct NoKeyData {};

template <class Value, class KeyData = NoKeyData>
class SnapshotTable {
 private:
  struct TableEntry;
  struct LogEntry;
  struct SnapshotData;

 public:
  // A `Key` identifies an entry in the `SnapshotTable`. For better performance,
  // keys always have identity. The template parameter `KeyData` can be used to
  // embed additional data in the keys.
  // A Key is implemented as a pointer into the table, which also contains the
  // `KeyData`. Therefore, keys have pointer-size and are cheap to copy.
  class Key {
   public:
    bool operator==(Key other) const { return entry_ == other.entry_; }
    const KeyData& data() const { return *entry_; }
    KeyData& data() { return *entry_; }

   private:
    friend SnapshotTable;
    TableEntry* entry_;
    explicit Key(TableEntry& entry) : entry_(&entry) {}
  };

  // A `Snapshot` captures the state of the `SnapshotTable`. Using a `Scope`,
  // the state of the table can be reset to a given snapshot.
  // A `Snapshot` is implemented as a pointer to internal data and is therefore
  // cheap to copy.
  class Snapshot {
   public:
    bool operator==(Snapshot other) const { return data_ == other.data_; }

   private:
    friend SnapshotTable;
    SnapshotData* data_;
    explicit Snapshot(SnapshotData& data) : data_(&data) {}
  };

  // All modifications to the table need to be performed through a `Scope`.
  // There can only be a single active scope for a table at a time. A new scope
  // is based on a list of predecessor snapshots. If no predecessor is given,
  // the scope is based on the initial state of the table. A single predecessor
  // snapshot resets the table to exactly this snapshot. In the case of multiple
  // snapshots, a merge function is used to unify values that were set since the
  // last common ancestor snapshot.
  // The previous scope needs to be closed using Seal() before another one can
  // be created.
  class Scope {
   public:
    // These overloads move to a new snapshot based on the common ancestor,
    // without merging different values from the predecessors.
    Scope(SnapshotTable& table, base::Vector<const Snapshot> predecessors)
        : snapshot_table_(table),
          snapshot_(&table.MoveToNewSnapshot(predecessors)) {}
    explicit Scope(SnapshotTable& table,
                   std::initializer_list<Snapshot> predecessors = {})
        : Scope(table, base::VectorOf(predecessors)) {}
    Scope(SnapshotTable& table, Snapshot parent) : Scope(table, {parent}) {}
    // These overloads merge different values from the predecessors using the
    // given function.
    template <class F>
    Scope(SnapshotTable& table, base::Vector<const Snapshot> predecessors,
          F merge_fun)
        : Scope(table, predecessors) {
      table.MergePredecessors(predecessors, merge_fun);
    }
    template <class F>
    Scope(SnapshotTable& table, std::initializer_list<Snapshot> predecessors,
          F merge_fun)
        : Scope(table, base::VectorOf(predecessors), merge_fun) {}

    Scope(const Scope&) = delete;
    Scope& operator=(const Scope&) = delete;

    const Value& Get(Key key) {
      DCHECK_EQ(snapshot_, snapshot_table_.current_snapshot_);
      return key.entry_->value;
    }

    void Set(Key key, Value new_value) {
      DCHECK(!snapshot_->IsSealed());
      snapshot_table_.Set(key, new_value);
    }

    // Sealing the current scope means that no more modifications are possible.
    // Produces a new snapshot which represents the current state.
    Snapshot Seal() {
      snapshot_->Seal(snapshot_table_.log_.size());
      // Optimization: If nothing changed in the new snapshot, we discard it and
      // use its parent instead.
      if (snapshot_->log_begin == snapshot_->log_end) {
        SnapshotData* parent = snapshot_->parent;
        snapshot_table_.current_snapshot_ = parent;
        DCHECK_EQ(snapshot_, &snapshot_table_.snapshots_.back());
        snapshot_table_.snapshots_.pop_back();
        return Snapshot{*parent};
      }
      return Snapshot{*snapshot_};
    }

    ~Scope() {
      // Seal() should have been used to obtain the new snapshot.
      DCHECK(snapshot_->IsSealed());
    }

   private:
    SnapshotTable& snapshot_table_;
    SnapshotData* snapshot_;
  };

  explicit SnapshotTable(Zone* zone) : zone_(zone) {
    root_snapshot_ = &NewSnapshot(nullptr);
    root_snapshot_->Seal(0);
    current_snapshot_ = root_snapshot_;
  }

  // The initial value is independent of the snapshot mechanism. Creating a key
  // with a certain initial value later has the same effect as creating the key
  // before all modifications to the table.
  // Keys have identity, and the data embedded in the key is mutable.
  Key NewKey(KeyData data, Value initial_value = Value{}) {
    return Key{table_.emplace_back(
        TableEntry{std::move(initial_value), std::move(data)})};
  }
  Key NewKey(Value initial_value = Value{}) {
    return NewKey(KeyData{}, initial_value);
  }

 private:
  Zone* zone_;
  ZoneDeque<TableEntry> table_{zone_};
  ZoneDeque<SnapshotData> snapshots_{zone_};
  // While logically each snapshot has its own log, we allocate the memory as a
  // single global log with each snapshot pointing to a section of it to reduce
  // the number of allocations.
  ZoneVector<LogEntry> log_{zone_};
  SnapshotData* root_snapshot_;
  SnapshotData* current_snapshot_;

  // The following members are only used during a merge operation. They are
  // declared here to recycle the memory, avoiding repeated Zone-allocation.
  ZoneVector<TableEntry*> merging_entries_{zone_};
  ZoneVector<Value> merge_values_{zone_};

  SnapshotData& NewSnapshot(SnapshotData* parent) {
    return snapshots_.emplace_back(parent, log_.size());
  }

  base::Vector<LogEntry> LogEntries(SnapshotData* s) {
    return base::VectorOf(&log_[s->log_begin], s->log_end - s->log_begin);
  }

  void RevertCurrentSnapshot() {
    DCHECK(current_snapshot_->IsSealed());
    base::Vector<LogEntry> log_entries = LogEntries(current_snapshot_);
    for (const LogEntry& entry : base::Reversed(log_entries)) {
      DCHECK_EQ(entry.table_entry.value, entry.new_value);
      entry.table_entry.value = entry.old_value;
    }
    current_snapshot_ = current_snapshot_->parent;
    DCHECK_NOT_NULL(current_snapshot_);
  }

  void ReplaySnapshot(SnapshotData* snapshot) {
    DCHECK_EQ(snapshot->parent, current_snapshot_);
    for (const LogEntry& entry : LogEntries(snapshot)) {
      DCHECK_EQ(entry.table_entry.value, entry.old_value);
      entry.table_entry.value = entry.new_value;
    }
    current_snapshot_ = snapshot;
  }

  void Set(Key key, Value new_value) {
    if (key.entry_->value == new_value) return;
    log_.push_back(LogEntry{*key.entry_, key.entry_->value, new_value});
    key.entry_->value = new_value;
  }

  void RecordMergeValue(TableEntry& entry, const Value& value,
                        uint32_t predecessor_index, uint32_t predecessor_count);
  SnapshotData& MoveToNewSnapshot(base::Vector<const Snapshot> predecessors);
  template <class F>
  void MergePredecessors(base::Vector<const Snapshot> predecessors,
                         F merge_fun);

  static constexpr uint32_t kNoMergeOffset =
      std::numeric_limits<uint32_t>::max();
  static constexpr uint32_t kNoMergedPredecessor =
      std::numeric_limits<uint32_t>::max();
};

// Place `KeyData` in a superclass to benefit from empty-base optimization.
template <class Value, class KeyData>
struct SnapshotTable<Value, KeyData>::TableEntry : KeyData {
  Value value;
  //  Used during merging: the offset in `merge_values_` where we store the
  //  merged values.
  uint32_t merge_offset = kNoMergeOffset;
  // Used during merging: the index of the predecessor for which we last
  // recorded a value. This allows us to only use the last value for a given
  // predecessor and skip over all earlier ones.
  uint32_t last_merged_predecessor = kNoMergedPredecessor;

  explicit TableEntry(Value value, KeyData data)
      : KeyData(std::move(data)), value(std::move(value)) {}
};

template <class Value, class KeyData>
struct SnapshotTable<Value, KeyData>::LogEntry {
  TableEntry& table_entry;
  Value old_value;
  Value new_value;
};

template <class Value, class KeyData>
struct SnapshotTable<Value, KeyData>::SnapshotData {
  SnapshotData* parent;

  const uint32_t depth = parent ? parent->depth + 1 : 0;
  size_t log_begin;
  size_t log_end = kInvalidOffset;

  static constexpr size_t kInvalidOffset = std::numeric_limits<size_t>::max();

  SnapshotData(SnapshotData* parent, size_t log_begin)
      : parent(parent), log_begin(log_begin) {}

  SnapshotData* CommonAncestor(SnapshotData* other) {
    SnapshotData* self = this;
    while (other->depth > self->depth) other = other->parent;
    while (self->depth > other->depth) self = self->parent;
    while (other != self) {
      self = self->parent;
      other = other->parent;
    }
    return self;
  }
  void Seal(size_t log_end) {
    DCHECK_WITH_MSG(!IsSealed(), "A scope can only be sealed once.");
    this->log_end = log_end;
  }

  bool IsSealed() const { return log_end != kInvalidOffset; }
};

template <class Value, class KeyData>
void SnapshotTable<Value, KeyData>::RecordMergeValue(
    TableEntry& entry, const Value& value, uint32_t predecessor_index,
    uint32_t predecessor_count) {
  if (predecessor_index == entry.last_merged_predecessor) {
    DCHECK_NE(entry.merge_offset, kNoMergeOffset);
    // We already recorded a later value for this predecessor, so we should skip
    // earlier values.
    return;
  }
  if (entry.merge_offset == kNoMergeOffset) {
    // Allocate space for the merge values. All the merge values are initialized
    // to the value from the parent snapshot. This way, we get the right value
    // for predecessors that did not change the value.
    DCHECK_EQ(entry.last_merged_predecessor, kNoMergedPredecessor);
    CHECK_LE(merge_values_.size() + predecessor_count,
             std::numeric_limits<uint32_t>::max());
    entry.merge_offset = static_cast<uint32_t>(merge_values_.size());
    merging_entries_.push_back(&entry);
    for (size_t i = 0; i < predecessor_count; ++i) {
      merge_values_.push_back(entry.value);
    }
  }
  merge_values_[entry.merge_offset + predecessor_index] = value;
  entry.last_merged_predecessor = predecessor_index;
}

// This function prepares the SnapshotTable to start a new snapshot/scope whose
// predecessors are `predecessors`. To do this, it resets and replay snapshots
// in between the `current_snapshot_` and the position of the new snapshot. For
// instance:
//
//        S0
//      /    \
//     S1      S3
//     |         \
//     S2         S4
//               /  \
//              S5   S6
// If `predecessors` are S5 and S6, and `current_snapshot_` is S2, we:
//
// - First find the common ancestor of S5 and S6 (it's S4). This will be the
//   parent snapshot of the new snapshot.
// - Find the common ancestor of S4 and the current snapshot S2 (it's S0).
// - Roll back S2 and S1 to reach S0
// - Replay S3 and S4 go be in the state of S4 (the common ancestor of
//   `predecessors`).
// - Start creating a new snapshot with parent S4.
template <class Value, class KeyData>
typename SnapshotTable<Value, KeyData>::SnapshotData&
SnapshotTable<Value, KeyData>::MoveToNewSnapshot(
    base::Vector<const Snapshot> predecessors) {
  DCHECK_WITH_MSG(
      current_snapshot_->IsSealed(),
      "A new scope was opened before the previous scope was sealed.");

  SnapshotData* common_ancestor;
  if (predecessors.empty()) {
    common_ancestor = root_snapshot_;
  } else {
    common_ancestor = predecessors.first().data_;
    for (Snapshot s : predecessors.SubVectorFrom(1)) {
      common_ancestor = common_ancestor->CommonAncestor(s.data_);
    }
  }
  SnapshotData* go_back_to = common_ancestor->CommonAncestor(current_snapshot_);
  while (current_snapshot_ != go_back_to) {
    RevertCurrentSnapshot();
  }
  {
    // Replay to common_ancestor.
    base::SmallVector<SnapshotData*, 16> path;
    for (SnapshotData* s = common_ancestor; s != go_back_to; s = s->parent) {
      path.push_back(s);
    }
    for (SnapshotData* s : base::Reversed(path)) {
      ReplaySnapshot(s);
    }
  }

  DCHECK_EQ(current_snapshot_, common_ancestor);
  SnapshotData& new_snapshot = NewSnapshot(common_ancestor);
  current_snapshot_ = &new_snapshot;
  return new_snapshot;
}

// Merges all entries modified in `predecessors` since the last common ancestor
// by adding them to the current scope.
template <class Value, class KeyData>
template <class F>
void SnapshotTable<Value, KeyData>::MergePredecessors(
    base::Vector<const Snapshot> predecessors, F merge_fun) {
  CHECK_LE(predecessors.size(), std::numeric_limits<uint32_t>::max());
  uint32_t predecessor_count = static_cast<uint32_t>(predecessors.size());
  if (predecessor_count < 1) return;

  // The merging works by reserving `predecessor_count` many slots in
  // `merge_values_` for every key that we find while going through the
  // predecessor logs. There, we place the values of the corresponding
  // predecessors, so that we can finally call the `merge_fun` by creating a
  // `base::Vector` pointing to the collected values inside of `merge_values_`.
  DCHECK(merge_values_.empty());
  DCHECK(merging_entries_.empty());
  SnapshotData* common_ancestor = current_snapshot_->parent;

  // Collect all the entries that require merging. For this, we walk the logs of
  // the predecessors backwards until reaching the common ancestor.
  for (uint32_t i = 0; i < predecessor_count; ++i) {
    for (SnapshotData* predecessor = predecessors[i].data_;
         predecessor != common_ancestor; predecessor = predecessor->parent) {
      base::Vector<LogEntry> log_entries = LogEntries(predecessor);
      for (const LogEntry& entry : base::Reversed(log_entries)) {
        RecordMergeValue(entry.table_entry, entry.new_value, i,
                         predecessor_count);
      }
    }
  }
  // Actually perform the merging by calling the merge function and modifying
  // the table.
  for (TableEntry* entry : merging_entries_) {
    Key key{*entry};
    Set(key, merge_fun(key, base::VectorOf(&merge_values_[entry->merge_offset],
                                           predecessor_count)));
    entry->last_merged_predecessor = kNoMergedPredecessor;
    entry->merge_offset = kNoMergeOffset;
  }
  merge_values_.clear();
  merging_entries_.clear();
}

}  // namespace v8::internal::compiler::turboshaft

#endif  // V8_COMPILER_TURBOSHAFT_SNAPSHOT_TABLE_H_
