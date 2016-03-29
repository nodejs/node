// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_LIVENESS_ANAYZER_H_
#define V8_COMPILER_LIVENESS_ANAYZER_H_

#include "src/bit-vector.h"
#include "src/compiler/node.h"
#include "src/zone-containers.h"

namespace v8 {
namespace internal {
namespace compiler {

class LivenessAnalyzerBlock;
class Node;
class StateValuesCache;


class NonLiveFrameStateSlotReplacer {
 public:
  void ClearNonLiveFrameStateSlots(Node* frame_state, BitVector* liveness);
  NonLiveFrameStateSlotReplacer(StateValuesCache* state_values_cache,
                                Node* replacement, size_t local_count,
                                Zone* local_zone)
      : replacement_node_(replacement),
        state_values_cache_(state_values_cache),
        local_zone_(local_zone),
        permanently_live_(local_count == 0 ? 1 : static_cast<int>(local_count),
                          local_zone),
        inputs_buffer_(local_zone) {}

  void MarkPermanentlyLive(int var) { permanently_live_.Add(var); }

 private:
  Node* ClearNonLiveStateValues(Node* frame_state, BitVector* liveness);

  StateValuesCache* state_values_cache() { return state_values_cache_; }
  Zone* local_zone() { return local_zone_; }

  // Node that replaces dead values.
  Node* replacement_node_;
  // Reference to state values cache so that we can create state values
  // nodes.
  StateValuesCache* state_values_cache_;

  Zone* local_zone_;
  BitVector permanently_live_;
  NodeVector inputs_buffer_;
};


class LivenessAnalyzer {
 public:
  LivenessAnalyzer(size_t local_count, Zone* zone);

  LivenessAnalyzerBlock* NewBlock();
  LivenessAnalyzerBlock* NewBlock(LivenessAnalyzerBlock* predecessor);

  void Run(NonLiveFrameStateSlotReplacer* relaxer);

  Zone* zone() { return zone_; }

  void Print(std::ostream& os);

  size_t local_count() { return local_count_; }

 private:
  void Queue(LivenessAnalyzerBlock* block);

  Zone* zone_;
  ZoneDeque<LivenessAnalyzerBlock*> blocks_;
  size_t local_count_;

  ZoneQueue<LivenessAnalyzerBlock*> queue_;
};


class LivenessAnalyzerBlock {
 public:
  friend class LivenessAnalyzer;

  void Lookup(int var) { entries_.push_back(Entry(Entry::kLookup, var)); }
  void Bind(int var) { entries_.push_back(Entry(Entry::kBind, var)); }
  void Checkpoint(Node* node) { entries_.push_back(Entry(node)); }
  void AddPredecessor(LivenessAnalyzerBlock* b) { predecessors_.push_back(b); }
  LivenessAnalyzerBlock* GetPredecessor() {
    DCHECK(predecessors_.size() == 1);
    return predecessors_[0];
  }

 private:
  class Entry {
   public:
    enum Kind { kBind, kLookup, kCheckpoint };

    Kind kind() const { return kind_; }
    Node* node() const {
      DCHECK(kind() == kCheckpoint);
      return node_;
    }
    int var() const {
      DCHECK(kind() != kCheckpoint);
      return var_;
    }

    explicit Entry(Node* node) : kind_(kCheckpoint), var_(-1), node_(node) {}
    Entry(Kind kind, int var) : kind_(kind), var_(var), node_(nullptr) {
      DCHECK(kind != kCheckpoint);
    }

   private:
    Kind kind_;
    int var_;
    Node* node_;
  };

  LivenessAnalyzerBlock(size_t id, size_t local_count, Zone* zone);
  void Process(BitVector* result, NonLiveFrameStateSlotReplacer* relaxer);
  bool UpdateLive(BitVector* working_area);

  void SetQueued() { queued_ = true; }
  bool IsQueued() { return queued_; }

  ZoneDeque<LivenessAnalyzerBlock*>::const_iterator pred_begin() {
    return predecessors_.begin();
  }
  ZoneDeque<LivenessAnalyzerBlock*>::const_iterator pred_end() {
    return predecessors_.end();
  }

  size_t id() { return id_; }
  void Print(std::ostream& os);

  ZoneDeque<Entry> entries_;
  ZoneDeque<LivenessAnalyzerBlock*> predecessors_;

  BitVector live_;
  bool queued_;

  size_t id_;
};


}  // namespace compiler
}  // namespace internal
}  // namespace v8

#endif  // V8_COMPILER_AST_GRAPH_BUILDER_H_
