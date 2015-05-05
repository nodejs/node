// Copyright 2009-2010 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/v8.h"

#include "src/heap-profiler.h"

#include "src/allocation-tracker.h"
#include "src/heap-snapshot-generator-inl.h"

namespace v8 {
namespace internal {

HeapProfiler::HeapProfiler(Heap* heap)
    : ids_(new HeapObjectsMap(heap)),
      names_(new StringsStorage(heap)),
      is_tracking_object_moves_(false) {
}


static void DeleteHeapSnapshot(HeapSnapshot** snapshot_ptr) {
  delete *snapshot_ptr;
}


HeapProfiler::~HeapProfiler() {
  snapshots_.Iterate(DeleteHeapSnapshot);
  snapshots_.Clear();
}


void HeapProfiler::DeleteAllSnapshots() {
  snapshots_.Iterate(DeleteHeapSnapshot);
  snapshots_.Clear();
  names_.Reset(new StringsStorage(heap()));
}


void HeapProfiler::RemoveSnapshot(HeapSnapshot* snapshot) {
  snapshots_.RemoveElement(snapshot);
}


void HeapProfiler::DefineWrapperClass(
    uint16_t class_id, v8::HeapProfiler::WrapperInfoCallback callback) {
  DCHECK(class_id != v8::HeapProfiler::kPersistentHandleNoClassId);
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
    v8::ActivityControl* control,
    v8::HeapProfiler::ObjectNameResolver* resolver) {
  HeapSnapshot* result = new HeapSnapshot(this);
  {
    HeapSnapshotGenerator generator(result, control, resolver, heap());
    if (!generator.GenerateSnapshot()) {
      delete result;
      result = NULL;
    } else {
      snapshots_.Add(result);
    }
  }
  ids_->RemoveDeadEntries();
  is_tracking_object_moves_ = true;
  return result;
}


void HeapProfiler::StartHeapObjectsTracking(bool track_allocations) {
  ids_->UpdateHeapObjectsMap();
  is_tracking_object_moves_ = true;
  DCHECK(!is_tracking_allocations());
  if (track_allocations) {
    allocation_tracker_.Reset(new AllocationTracker(ids_.get(), names_.get()));
    heap()->DisableInlineAllocation();
  }
}


SnapshotObjectId HeapProfiler::PushHeapObjectsStats(OutputStream* stream,
                                                    int64_t* timestamp_us) {
  return ids_->PushHeapObjectsStats(stream, timestamp_us);
}


void HeapProfiler::StopHeapObjectsTracking() {
  ids_->StopHeapObjectsTracking();
  if (is_tracking_allocations()) {
    allocation_tracker_.Reset(NULL);
    heap()->EnableInlineAllocation();
  }
}


size_t HeapProfiler::GetMemorySizeUsedByProfiler() {
  size_t size = sizeof(*this);
  size += names_->GetUsedMemorySize();
  size += ids_->GetUsedMemorySize();
  size += GetMemoryUsedByList(snapshots_);
  for (int i = 0; i < snapshots_.length(); ++i) {
    size += snapshots_[i]->RawSnapshotSize();
  }
  return size;
}


int HeapProfiler::GetSnapshotsCount() {
  return snapshots_.length();
}


HeapSnapshot* HeapProfiler::GetSnapshot(int index) {
  return snapshots_.at(index);
}


SnapshotObjectId HeapProfiler::GetSnapshotObjectId(Handle<Object> obj) {
  if (!obj->IsHeapObject())
    return v8::HeapProfiler::kUnknownObjectId;
  return ids_->FindEntry(HeapObject::cast(*obj)->address());
}


void HeapProfiler::ObjectMoveEvent(Address from, Address to, int size) {
  bool known_object = ids_->MoveObject(from, to, size);
  if (!known_object && !allocation_tracker_.is_empty()) {
    allocation_tracker_->address_to_trace()->MoveObject(from, to, size);
  }
}


void HeapProfiler::AllocationEvent(Address addr, int size) {
  DisallowHeapAllocation no_allocation;
  if (!allocation_tracker_.is_empty()) {
    allocation_tracker_->AllocationEvent(addr, size);
  }
}


void HeapProfiler::UpdateObjectSizeEvent(Address addr, int size) {
  ids_->UpdateObjectSize(addr, size);
}


void HeapProfiler::SetRetainedObjectInfo(UniqueId id,
                                         RetainedObjectInfo* info) {
  // TODO(yurus, marja): Don't route this information through GlobalHandles.
  heap()->isolate()->global_handles()->SetRetainedObjectInfo(id, info);
}


Handle<HeapObject> HeapProfiler::FindHeapObjectById(SnapshotObjectId id) {
  HeapObject* object = NULL;
  HeapIterator iterator(heap(), HeapIterator::kFilterUnreachable);
  // Make sure that object with the given id is still reachable.
  for (HeapObject* obj = iterator.next();
       obj != NULL;
       obj = iterator.next()) {
    if (ids_->FindEntry(obj->address()) == id) {
      DCHECK(object == NULL);
      object = obj;
      // Can't break -- kFilterUnreachable requires full heap traversal.
    }
  }
  return object != NULL ? Handle<HeapObject>(object) : Handle<HeapObject>();
}


void HeapProfiler::ClearHeapObjectMap() {
  ids_.Reset(new HeapObjectsMap(heap()));
  if (!is_tracking_allocations()) is_tracking_object_moves_ = false;
}


} }  // namespace v8::internal
