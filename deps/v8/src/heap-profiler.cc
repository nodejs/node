// Copyright 2009-2010 the V8 project authors. All rights reserved.
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

#include "deoptimizer.h"
#include "heap-profiler.h"
#include "heap-snapshot-generator-inl.h"

namespace v8 {
namespace internal {

HeapProfiler::HeapProfiler(Heap* heap)
    : snapshots_(new HeapSnapshotsCollection(heap)),
      next_snapshot_uid_(1),
      is_tracking_allocations_(false) {
}


HeapProfiler::~HeapProfiler() {
  delete snapshots_;
}


void HeapProfiler::DeleteAllSnapshots() {
  Heap* the_heap = heap();
  delete snapshots_;
  snapshots_ = new HeapSnapshotsCollection(the_heap);
}


void HeapProfiler::DefineWrapperClass(
    uint16_t class_id, v8::HeapProfiler::WrapperInfoCallback callback) {
  ASSERT(class_id != v8::HeapProfiler::kPersistentHandleNoClassId);
  if (wrapper_callbacks_.length() <= class_id) {
    wrapper_callbacks_.AddBlock(
        NULL, class_id - wrapper_callbacks_.length() + 1);
  }
  wrapper_callbacks_[class_id] = callback;
}


v8::RetainedObjectInfo* HeapProfiler::ExecuteWrapperClassCallback(
    uint16_t class_id, Object** wrapper) {
  if (wrapper_callbacks_.length() <= class_id) return NULL;
  return wrapper_callbacks_[class_id](
      class_id, Utils::ToLocal(Handle<Object>(wrapper)));
}


HeapSnapshot* HeapProfiler::TakeSnapshot(
    const char* name,
    v8::ActivityControl* control,
    v8::HeapProfiler::ObjectNameResolver* resolver) {
  HeapSnapshot* result = snapshots_->NewSnapshot(name, next_snapshot_uid_++);
  {
    HeapSnapshotGenerator generator(result, control, resolver, heap());
    if (!generator.GenerateSnapshot()) {
      delete result;
      result = NULL;
    }
  }
  snapshots_->SnapshotGenerationFinished(result);
  return result;
}


HeapSnapshot* HeapProfiler::TakeSnapshot(
    String* name,
    v8::ActivityControl* control,
    v8::HeapProfiler::ObjectNameResolver* resolver) {
  return TakeSnapshot(snapshots_->names()->GetName(name), control, resolver);
}


void HeapProfiler::StartHeapObjectsTracking() {
  snapshots_->StartHeapObjectsTracking();
}


SnapshotObjectId HeapProfiler::PushHeapObjectsStats(OutputStream* stream) {
  return snapshots_->PushHeapObjectsStats(stream);
}


void HeapProfiler::StopHeapObjectsTracking() {
  snapshots_->StopHeapObjectsTracking();
}


size_t HeapProfiler::GetMemorySizeUsedByProfiler() {
  return snapshots_->GetUsedMemorySize();
}


int HeapProfiler::GetSnapshotsCount() {
  return snapshots_->snapshots()->length();
}


HeapSnapshot* HeapProfiler::GetSnapshot(int index) {
  return snapshots_->snapshots()->at(index);
}


SnapshotObjectId HeapProfiler::GetSnapshotObjectId(Handle<Object> obj) {
  if (!obj->IsHeapObject())
    return v8::HeapProfiler::kUnknownObjectId;
  return snapshots_->FindObjectId(HeapObject::cast(*obj)->address());
}


void HeapProfiler::ObjectMoveEvent(Address from, Address to, int size) {
  snapshots_->ObjectMoveEvent(from, to, size);
}


void HeapProfiler::NewObjectEvent(Address addr, int size) {
  snapshots_->NewObjectEvent(addr, size);
}


void HeapProfiler::UpdateObjectSizeEvent(Address addr, int size) {
  snapshots_->UpdateObjectSizeEvent(addr, size);
}


void HeapProfiler::SetRetainedObjectInfo(UniqueId id,
                                         RetainedObjectInfo* info) {
  // TODO(yurus, marja): Don't route this information through GlobalHandles.
  heap()->isolate()->global_handles()->SetRetainedObjectInfo(id, info);
}


void HeapProfiler::StartHeapAllocationsRecording() {
  StartHeapObjectsTracking();
  is_tracking_allocations_ = true;
  DropCompiledCode();
  snapshots_->UpdateHeapObjectsMap();
}


void HeapProfiler::StopHeapAllocationsRecording() {
  StopHeapObjectsTracking();
  is_tracking_allocations_ = false;
  DropCompiledCode();
}


void HeapProfiler::RecordObjectAllocationFromMasm(Isolate* isolate,
                                                  Address obj,
                                                  int size) {
  isolate->heap_profiler()->NewObjectEvent(obj, size);
}


void HeapProfiler::DropCompiledCode() {
  Isolate* isolate = heap()->isolate();
  HandleScope scope(isolate);

  if (FLAG_concurrent_recompilation) {
    isolate->optimizing_compiler_thread()->Flush();
  }

  Deoptimizer::DeoptimizeAll(isolate);

  Handle<Code> lazy_compile =
      Handle<Code>(isolate->builtins()->builtin(Builtins::kLazyCompile));

  heap()->CollectAllGarbage(Heap::kMakeHeapIterableMask,
                            "switch allocations tracking");

  DisallowHeapAllocation no_allocation;

  HeapIterator iterator(heap());
  HeapObject* obj = NULL;
  while (((obj = iterator.next()) != NULL)) {
    if (obj->IsJSFunction()) {
      JSFunction* function = JSFunction::cast(obj);
      SharedFunctionInfo* shared = function->shared();

      if (!shared->allows_lazy_compilation()) continue;
      if (!shared->script()->IsScript()) continue;

      Code::Kind kind = function->code()->kind();
      if (kind == Code::FUNCTION || kind == Code::BUILTIN) {
        function->set_code(*lazy_compile);
        shared->set_code(*lazy_compile);
      }
    }
  }
}


} }  // namespace v8::internal
