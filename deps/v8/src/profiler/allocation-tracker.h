// Copyright 2013 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_PROFILER_ALLOCATION_TRACKER_H_
#define V8_PROFILER_ALLOCATION_TRACKER_H_

#include <map>
#include <vector>

#include "include/v8-profiler.h"
#include "src/base/hashmap.h"
#include "src/handles.h"
#include "src/vector.h"

namespace v8 {
namespace internal {

// Forward declarations.
class AllocationTraceTree;
class AllocationTracker;
class HeapObjectsMap;
class SharedFunctionInfo;
class StringsStorage;

class AllocationTraceNode {
 public:
  AllocationTraceNode(AllocationTraceTree* tree,
                      unsigned function_info_index);
  ~AllocationTraceNode();
  AllocationTraceNode* FindChild(unsigned function_info_index);
  AllocationTraceNode* FindOrAddChild(unsigned function_info_index);
  void AddAllocation(unsigned size);

  unsigned function_info_index() const { return function_info_index_; }
  unsigned allocation_size() const { return total_size_; }
  unsigned allocation_count() const { return allocation_count_; }
  unsigned id() const { return id_; }
  const std::vector<AllocationTraceNode*>& children() const {
    return children_;
  }

  void Print(int indent, AllocationTracker* tracker);

 private:
  AllocationTraceTree* tree_;
  unsigned function_info_index_;
  unsigned total_size_;
  unsigned allocation_count_;
  unsigned id_;
  std::vector<AllocationTraceNode*> children_;

  DISALLOW_COPY_AND_ASSIGN(AllocationTraceNode);
};


class AllocationTraceTree {
 public:
  AllocationTraceTree();
  ~AllocationTraceTree() = default;
  AllocationTraceNode* AddPathFromEnd(const Vector<unsigned>& path);
  AllocationTraceNode* root() { return &root_; }
  unsigned next_node_id() { return next_node_id_++; }
  V8_EXPORT_PRIVATE void Print(AllocationTracker* tracker);

 private:
  unsigned next_node_id_;
  AllocationTraceNode root_;

  DISALLOW_COPY_AND_ASSIGN(AllocationTraceTree);
};

class V8_EXPORT_PRIVATE AddressToTraceMap {
 public:
  void AddRange(Address addr, int size, unsigned node_id);
  unsigned GetTraceNodeId(Address addr);
  void MoveObject(Address from, Address to, int size);
  void Clear();
  size_t size() { return ranges_.size(); }
  void Print();

 private:
  struct RangeStack {
    RangeStack(Address start, unsigned node_id)
        : start(start), trace_node_id(node_id) {}
    Address start;
    unsigned trace_node_id;
  };
  // [start, end) -> trace
  typedef std::map<Address, RangeStack> RangeMap;

  void RemoveRange(Address start, Address end);

  RangeMap ranges_;
};

class AllocationTracker {
 public:
  struct FunctionInfo {
    FunctionInfo();
    const char* name;
    SnapshotObjectId function_id;
    const char* script_name;
    int script_id;
    int line;
    int column;
  };

  AllocationTracker(HeapObjectsMap* ids, StringsStorage* names);
  ~AllocationTracker();

  V8_EXPORT_PRIVATE void PrepareForSerialization();
  void AllocationEvent(Address addr, int size);

  AllocationTraceTree* trace_tree() { return &trace_tree_; }
  const std::vector<FunctionInfo*>& function_info_list() const {
    return function_info_list_;
  }
  AddressToTraceMap* address_to_trace() { return &address_to_trace_; }

 private:
  unsigned AddFunctionInfo(SharedFunctionInfo info, SnapshotObjectId id);
  unsigned functionInfoIndexForVMState(StateTag state);

  class UnresolvedLocation {
   public:
    UnresolvedLocation(Script script, int start, FunctionInfo* info);
    ~UnresolvedLocation();
    void Resolve();

   private:
    static void HandleWeakScript(const v8::WeakCallbackInfo<void>& data);

    Handle<Script> script_;
    int start_position_;
    FunctionInfo* info_;
  };

  static const int kMaxAllocationTraceLength = 64;
  HeapObjectsMap* ids_;
  StringsStorage* names_;
  AllocationTraceTree trace_tree_;
  unsigned allocation_trace_buffer_[kMaxAllocationTraceLength];
  std::vector<FunctionInfo*> function_info_list_;
  base::HashMap id_to_function_info_index_;
  std::vector<UnresolvedLocation*> unresolved_locations_;
  unsigned info_index_for_other_state_;
  AddressToTraceMap address_to_trace_;

  DISALLOW_COPY_AND_ASSIGN(AllocationTracker);
};

}  // namespace internal
}  // namespace v8

#endif  // V8_PROFILER_ALLOCATION_TRACKER_H_
