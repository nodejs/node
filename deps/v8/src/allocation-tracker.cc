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

#include "v8.h"

#include "allocation-tracker.h"

#include "heap-snapshot-generator.h"
#include "frames-inl.h"

namespace v8 {
namespace internal {

AllocationTraceNode::AllocationTraceNode(
    AllocationTraceTree* tree, SnapshotObjectId shared_function_info_id)
    : tree_(tree),
      function_id_(shared_function_info_id),
      total_size_(0),
      allocation_count_(0),
      id_(tree->next_node_id()) {
}


AllocationTraceNode::~AllocationTraceNode() {
  for (int i = 0; i < children_.length(); i++) delete children_[i];
}


AllocationTraceNode* AllocationTraceNode::FindChild(SnapshotObjectId id) {
  for (int i = 0; i < children_.length(); i++) {
    AllocationTraceNode* node = children_[i];
    if (node->function_id() == id) return node;
  }
  return NULL;
}


AllocationTraceNode* AllocationTraceNode::FindOrAddChild(SnapshotObjectId id) {
  AllocationTraceNode* child = FindChild(id);
  if (child == NULL) {
    child = new AllocationTraceNode(tree_, id);
    children_.Add(child);
  }
  return child;
}


void AllocationTraceNode::AddAllocation(unsigned size) {
  total_size_ += size;
  ++allocation_count_;
}


void AllocationTraceNode::Print(int indent, AllocationTracker* tracker) {
  OS::Print("%10u %10u %*c", total_size_, allocation_count_, indent, ' ');
  if (tracker != NULL) {
    const char* name = "<unknown function>";
    if (function_id_ != 0) {
      AllocationTracker::FunctionInfo* info =
          tracker->GetFunctionInfo(function_id_);
      if (info != NULL) {
        name = info->name;
      }
    }
    OS::Print("%s #%u", name, id_);
  } else {
    OS::Print("%u #%u", function_id_, id_);
  }
  OS::Print("\n");
  indent += 2;
  for (int i = 0; i < children_.length(); i++) {
    children_[i]->Print(indent, tracker);
  }
}


AllocationTraceTree::AllocationTraceTree()
    : next_node_id_(1),
      root_(this, 0) {
}


AllocationTraceTree::~AllocationTraceTree() {
}


AllocationTraceNode* AllocationTraceTree::AddPathFromEnd(
    const Vector<SnapshotObjectId>& path) {
  AllocationTraceNode* node = root();
  for (SnapshotObjectId* entry = path.start() + path.length() - 1;
       entry != path.start() - 1;
       --entry) {
    node = node->FindOrAddChild(*entry);
  }
  return node;
}


void AllocationTraceTree::Print(AllocationTracker* tracker) {
  OS::Print("[AllocationTraceTree:]\n");
  OS::Print("Total size | Allocation count | Function id | id\n");
  root()->Print(0, tracker);
}

void AllocationTracker::DeleteUnresolvedLocation(
    UnresolvedLocation** location) {
  delete *location;
}


AllocationTracker::FunctionInfo::FunctionInfo()
    : name(""),
      script_name(""),
      script_id(0),
      line(-1),
      column(-1) {
}


static bool AddressesMatch(void* key1, void* key2) {
  return key1 == key2;
}


AllocationTracker::AllocationTracker(
    HeapObjectsMap* ids, StringsStorage* names)
    : ids_(ids),
      names_(names),
      id_to_function_info_(AddressesMatch) {
}


AllocationTracker::~AllocationTracker() {
  unresolved_locations_.Iterate(DeleteUnresolvedLocation);
  for (HashMap::Entry* p = id_to_function_info_.Start();
       p != NULL;
       p = id_to_function_info_.Next(p)) {
    delete reinterpret_cast<AllocationTracker::FunctionInfo* >(p->value);
  }
}


void AllocationTracker::PrepareForSerialization() {
  List<UnresolvedLocation*> copy(unresolved_locations_.length());
  copy.AddAll(unresolved_locations_);
  unresolved_locations_.Clear();
  for (int i = 0; i < copy.length(); i++) {
    copy[i]->Resolve();
    delete copy[i];
  }
}


void AllocationTracker::AllocationEvent(Address addr, int size) {
  DisallowHeapAllocation no_allocation;
  Heap* heap = ids_->heap();

  // Mark the new block as FreeSpace to make sure the heap is iterable
  // while we are capturing stack trace.
  FreeListNode::FromAddress(addr)->set_size(heap, size);
  ASSERT_EQ(HeapObject::FromAddress(addr)->Size(), size);
  ASSERT(FreeListNode::IsFreeListNode(HeapObject::FromAddress(addr)));

  Isolate* isolate = heap->isolate();
  int length = 0;
  StackTraceFrameIterator it(isolate);
  while (!it.done() && length < kMaxAllocationTraceLength) {
    JavaScriptFrame* frame = it.frame();
    SharedFunctionInfo* shared = frame->function()->shared();
    SnapshotObjectId id = ids_->FindOrAddEntry(
        shared->address(), shared->Size(), false);
    allocation_trace_buffer_[length++] = id;
    AddFunctionInfo(shared, id);
    it.Advance();
  }
  AllocationTraceNode* top_node = trace_tree_.AddPathFromEnd(
      Vector<SnapshotObjectId>(allocation_trace_buffer_, length));
  top_node->AddAllocation(size);
}


static uint32_t SnapshotObjectIdHash(SnapshotObjectId id) {
  return ComputeIntegerHash(static_cast<uint32_t>(id),
                            v8::internal::kZeroHashSeed);
}


AllocationTracker::FunctionInfo* AllocationTracker::GetFunctionInfo(
    SnapshotObjectId id) {
  HashMap::Entry* entry = id_to_function_info_.Lookup(
      reinterpret_cast<void*>(id), SnapshotObjectIdHash(id), false);
  if (entry == NULL) {
    return NULL;
  }
  return reinterpret_cast<FunctionInfo*>(entry->value);
}


void AllocationTracker::AddFunctionInfo(SharedFunctionInfo* shared,
                                        SnapshotObjectId id) {
  HashMap::Entry* entry = id_to_function_info_.Lookup(
      reinterpret_cast<void*>(id), SnapshotObjectIdHash(id), true);
  if (entry->value == NULL) {
    FunctionInfo* info = new FunctionInfo();
    info->name = names_->GetFunctionName(shared->DebugName());
    if (shared->script()->IsScript()) {
      Script* script = Script::cast(shared->script());
      if (script->name()->IsName()) {
        Name* name = Name::cast(script->name());
        info->script_name = names_->GetName(name);
      }
      info->script_id = script->id()->value();
      // Converting start offset into line and column may cause heap
      // allocations so we postpone them until snapshot serialization.
      unresolved_locations_.Add(new UnresolvedLocation(
          script,
          shared->start_position(),
          info));
    }
    entry->value = info;
  }
}


AllocationTracker::UnresolvedLocation::UnresolvedLocation(
    Script* script, int start, FunctionInfo* info)
    : start_position_(start),
      info_(info) {
  script_ = Handle<Script>::cast(
      script->GetIsolate()->global_handles()->Create(script));
  GlobalHandles::MakeWeak(reinterpret_cast<Object**>(script_.location()),
                          this,
                          &HandleWeakScript);
}


AllocationTracker::UnresolvedLocation::~UnresolvedLocation() {
  if (!script_.is_null()) {
    GlobalHandles::Destroy(reinterpret_cast<Object**>(script_.location()));
  }
}


void AllocationTracker::UnresolvedLocation::Resolve() {
  if (script_.is_null()) return;
  info_->line = GetScriptLineNumber(script_, start_position_);
  info_->column = GetScriptColumnNumber(script_, start_position_);
}


void AllocationTracker::UnresolvedLocation::HandleWeakScript(
    const v8::WeakCallbackData<v8::Value, void>& data) {
  UnresolvedLocation* loc =
      reinterpret_cast<UnresolvedLocation*>(data.GetParameter());
  GlobalHandles::Destroy(reinterpret_cast<Object**>(loc->script_.location()));
  loc->script_ = Handle<Script>::null();
}


} }  // namespace v8::internal
