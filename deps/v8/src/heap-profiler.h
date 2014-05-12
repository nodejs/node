// Copyright 2009-2010 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_HEAP_PROFILER_H_
#define V8_HEAP_PROFILER_H_

#include "heap-snapshot-generator-inl.h"
#include "isolate.h"
#include "smart-pointers.h"

namespace v8 {
namespace internal {

class HeapSnapshot;

class HeapProfiler {
 public:
  explicit HeapProfiler(Heap* heap);
  ~HeapProfiler();

  size_t GetMemorySizeUsedByProfiler();

  HeapSnapshot* TakeSnapshot(
      const char* name,
      v8::ActivityControl* control,
      v8::HeapProfiler::ObjectNameResolver* resolver);
  HeapSnapshot* TakeSnapshot(
      String* name,
      v8::ActivityControl* control,
      v8::HeapProfiler::ObjectNameResolver* resolver);

  void StartHeapObjectsTracking(bool track_allocations);
  void StopHeapObjectsTracking();
  AllocationTracker* allocation_tracker() const {
    return allocation_tracker_.get();
  }
  HeapObjectsMap* heap_object_map() const { return ids_.get(); }
  StringsStorage* names() const { return names_.get(); }

  SnapshotObjectId PushHeapObjectsStats(OutputStream* stream);
  int GetSnapshotsCount();
  HeapSnapshot* GetSnapshot(int index);
  SnapshotObjectId GetSnapshotObjectId(Handle<Object> obj);
  void DeleteAllSnapshots();
  void RemoveSnapshot(HeapSnapshot* snapshot);

  void ObjectMoveEvent(Address from, Address to, int size);

  void AllocationEvent(Address addr, int size);

  void UpdateObjectSizeEvent(Address addr, int size);

  void DefineWrapperClass(
      uint16_t class_id, v8::HeapProfiler::WrapperInfoCallback callback);

  v8::RetainedObjectInfo* ExecuteWrapperClassCallback(uint16_t class_id,
                                                      Object** wrapper);
  void SetRetainedObjectInfo(UniqueId id, RetainedObjectInfo* info);

  bool is_tracking_object_moves() const { return is_tracking_object_moves_; }
  bool is_tracking_allocations() const {
    return !allocation_tracker_.is_empty();
  }

  Handle<HeapObject> FindHeapObjectById(SnapshotObjectId id);
  void ClearHeapObjectMap();

 private:
  Heap* heap() const { return ids_->heap(); }

  // Mapping from HeapObject addresses to objects' uids.
  SmartPointer<HeapObjectsMap> ids_;
  List<HeapSnapshot*> snapshots_;
  SmartPointer<StringsStorage> names_;
  unsigned next_snapshot_uid_;
  List<v8::HeapProfiler::WrapperInfoCallback> wrapper_callbacks_;
  SmartPointer<AllocationTracker> allocation_tracker_;
  bool is_tracking_object_moves_;
};

} }  // namespace v8::internal

#endif  // V8_HEAP_PROFILER_H_
