// Copyright 2009-2010 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_PROFILER_HEAP_PROFILER_H_
#define V8_PROFILER_HEAP_PROFILER_H_

#include <memory>

#include "src/isolate.h"
#include "src/list.h"

namespace v8 {
namespace internal {

// Forward declarations.
class AllocationTracker;
class HeapObjectsMap;
class HeapSnapshot;
class SamplingHeapProfiler;
class StringsStorage;

class HeapProfiler {
 public:
  explicit HeapProfiler(Heap* heap);
  ~HeapProfiler();

  size_t GetMemorySizeUsedByProfiler();

  HeapSnapshot* TakeSnapshot(
      v8::ActivityControl* control,
      v8::HeapProfiler::ObjectNameResolver* resolver);

  bool StartSamplingHeapProfiler(uint64_t sample_interval, int stack_depth,
                                 v8::HeapProfiler::SamplingFlags);
  void StopSamplingHeapProfiler();
  bool is_sampling_allocations() { return !!sampling_heap_profiler_; }
  AllocationProfile* GetAllocationProfile();

  void StartHeapObjectsTracking(bool track_allocations);
  void StopHeapObjectsTracking();
  AllocationTracker* allocation_tracker() const {
    return allocation_tracker_.get();
  }
  HeapObjectsMap* heap_object_map() const { return ids_.get(); }
  StringsStorage* names() const { return names_.get(); }

  SnapshotObjectId PushHeapObjectsStats(OutputStream* stream,
                                        int64_t* timestamp_us);
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

  void SetGetRetainerInfosCallback(
      v8::HeapProfiler::GetRetainerInfosCallback callback);

  v8::HeapProfiler::RetainerInfos GetRetainerInfos(Isolate* isolate);

  bool is_tracking_object_moves() const { return is_tracking_object_moves_; }
  bool is_tracking_allocations() const { return !!allocation_tracker_; }

  Handle<HeapObject> FindHeapObjectById(SnapshotObjectId id);
  void ClearHeapObjectMap();

  Isolate* isolate() const { return heap()->isolate(); }

 private:
  Heap* heap() const;

  // Mapping from HeapObject addresses to objects' uids.
  std::unique_ptr<HeapObjectsMap> ids_;
  List<HeapSnapshot*> snapshots_;
  std::unique_ptr<StringsStorage> names_;
  List<v8::HeapProfiler::WrapperInfoCallback> wrapper_callbacks_;
  std::unique_ptr<AllocationTracker> allocation_tracker_;
  bool is_tracking_object_moves_;
  base::Mutex profiler_mutex_;
  std::unique_ptr<SamplingHeapProfiler> sampling_heap_profiler_;
  v8::HeapProfiler::GetRetainerInfosCallback get_retainer_infos_callback_;

  DISALLOW_COPY_AND_ASSIGN(HeapProfiler);
};

}  // namespace internal
}  // namespace v8

#endif  // V8_PROFILER_HEAP_PROFILER_H_
