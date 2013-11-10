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

namespace v8 {
namespace internal {

class HeapObjectsMap;

class AllocationTraceTree;

class AllocationTraceNode {
 public:
  AllocationTraceNode(AllocationTraceTree* tree,
                      SnapshotObjectId shared_function_info_id);
  ~AllocationTraceNode();
  AllocationTraceNode* FindChild(SnapshotObjectId shared_function_info_id);
  AllocationTraceNode* FindOrAddChild(SnapshotObjectId shared_function_info_id);
  void AddAllocation(unsigned size);

  SnapshotObjectId function_id() const { return function_id_; }
  unsigned allocation_size() const { return total_size_; }
  unsigned allocation_count() const { return allocation_count_; }
  unsigned id() const { return id_; }
  Vector<AllocationTraceNode*> children() const { return children_.ToVector(); }

  void Print(int indent, AllocationTracker* tracker);

 private:
  AllocationTraceTree* tree_;
  SnapshotObjectId function_id_;
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
  AllocationTraceNode* AddPathFromEnd(const Vector<SnapshotObjectId>& path);
  AllocationTraceNode* root() { return &root_; }
  unsigned next_node_id() { return next_node_id_++; }
  void Print(AllocationTracker* tracker);

 private:
  unsigned next_node_id_;
  AllocationTraceNode root_;

  DISALLOW_COPY_AND_ASSIGN(AllocationTraceTree);
};


class AllocationTracker {
 public:
  struct FunctionInfo {
    FunctionInfo();
    const char* name;
    const char* script_name;
    int script_id;
    int line;
    int column;
  };

  AllocationTracker(HeapObjectsMap* ids, StringsStorage* names);
  ~AllocationTracker();

  void PrepareForSerialization();
  void NewObjectEvent(Address addr, int size);

  AllocationTraceTree* trace_tree() { return &trace_tree_; }
  HashMap* id_to_function_info() { return &id_to_function_info_; }
  FunctionInfo* GetFunctionInfo(SnapshotObjectId id);

 private:
  void AddFunctionInfo(SharedFunctionInfo* info, SnapshotObjectId id);

  class UnresolvedLocation {
   public:
    UnresolvedLocation(Script* script, int start, FunctionInfo* info);
    ~UnresolvedLocation();
    void Resolve();

   private:
    static void HandleWeakScript(v8::Isolate* isolate,
                                 v8::Persistent<v8::Value>* obj,
                                 void* data);
    Handle<Script> script_;
    int start_position_;
    FunctionInfo* info_;
  };
  static void DeleteUnresolvedLocation(UnresolvedLocation** location);

  static const int kMaxAllocationTraceLength = 64;
  HeapObjectsMap* ids_;
  StringsStorage* names_;
  AllocationTraceTree trace_tree_;
  SnapshotObjectId allocation_trace_buffer_[kMaxAllocationTraceLength];
  HashMap id_to_function_info_;
  List<UnresolvedLocation*> unresolved_locations_;

  DISALLOW_COPY_AND_ASSIGN(AllocationTracker);
};

} }  // namespace v8::internal

#endif  // V8_ALLOCATION_TRACKER_H_

