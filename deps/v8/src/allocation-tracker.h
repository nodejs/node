// Copyright 2013 the V8 project authors. All rights reserved.
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

#ifndef V8_ALLOCATION_TRACKER_H_
#define V8_ALLOCATION_TRACKER_H_

#include <map>

namespace v8 {
namespace internal {

class HeapObjectsMap;

class AllocationTraceTree;

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
  Vector<AllocationTraceNode*> children() const { return children_.ToVector(); }

  void Print(int indent, AllocationTracker* tracker);

 private:
  AllocationTraceTree* tree_;
  unsigned function_info_index_;
  unsigned total_size_;
  unsigned allocation_count_;
  unsigned id_;
  List<AllocationTraceNode*> children_;

  DISALLOW_COPY_AND_ASSIGN(AllocationTraceNode);
};


class AllocationTraceTree {
 public:
  AllocationTraceTree();
  ~AllocationTraceTree();
  AllocationTraceNode* AddPathFromEnd(const Vector<unsigned>& path);
  AllocationTraceNode* root() { return &root_; }
  unsigned next_node_id() { return next_node_id_++; }
  void Print(AllocationTracker* tracker);

 private:
  unsigned next_node_id_;
  AllocationTraceNode root_;

  DISALLOW_COPY_AND_ASSIGN(AllocationTraceTree);
};


class AddressToTraceMap {
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

  void PrepareForSerialization();
  void AllocationEvent(Address addr, int size);

  AllocationTraceTree* trace_tree() { return &trace_tree_; }
  const List<FunctionInfo*>& function_info_list() const {
    return function_info_list_;
  }
  AddressToTraceMap* address_to_trace() { return &address_to_trace_; }

 private:
  unsigned AddFunctionInfo(SharedFunctionInfo* info, SnapshotObjectId id);
  static void DeleteFunctionInfo(FunctionInfo** info);
  unsigned functionInfoIndexForVMState(StateTag state);

  class UnresolvedLocation {
   public:
    UnresolvedLocation(Script* script, int start, FunctionInfo* info);
    ~UnresolvedLocation();
    void Resolve();

   private:
    static void HandleWeakScript(
        const v8::WeakCallbackData<v8::Value, void>& data);

    Handle<Script> script_;
    int start_position_;
    FunctionInfo* info_;
  };
  static void DeleteUnresolvedLocation(UnresolvedLocation** location);

  static const int kMaxAllocationTraceLength = 64;
  HeapObjectsMap* ids_;
  StringsStorage* names_;
  AllocationTraceTree trace_tree_;
  unsigned allocation_trace_buffer_[kMaxAllocationTraceLength];
  List<FunctionInfo*> function_info_list_;
  HashMap id_to_function_info_index_;
  List<UnresolvedLocation*> unresolved_locations_;
  unsigned info_index_for_other_state_;
  AddressToTraceMap address_to_trace_;

  DISALLOW_COPY_AND_ASSIGN(AllocationTracker);
};

} }  // namespace v8::internal

#endif  // V8_ALLOCATION_TRACKER_H_
