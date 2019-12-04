// Copyright 2009-2010 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_PROFILER_HEAP_PROFILER_H_
#define V8_PROFILER_HEAP_PROFILER_H_

#include <memory>
#include <vector>

#include "include/v8-profiler.h"
#include "src/base/platform/mutex.h"
#include "src/common/globals.h"
#include "src/debug/debug-interface.h"
#include "src/heap/heap.h"

namespace v8 {
namespace internal {

// Forward declarations.
class AllocationTracker;
class HeapObjectsMap;
class HeapSnapshot;
class SamplingHeapProfiler;
class StringsStorage;

class HeapProfiler : public HeapObjectAllocationTracker {
 public:
  explicit HeapProfiler(Heap* heap);
  ~HeapProfiler() override;

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
  SnapshotObjectId GetSnapshotObjectId(NativeObject obj);
  void DeleteAllSnapshots();
  void RemoveSnapshot(HeapSnapshot* snapshot);

  void ObjectMoveEvent(Address from, Address to, int size);

  void AllocationEvent(Address addr, int size) override;

  void UpdateObjectSizeEvent(Address addr, int size) override;

  void AddBuildEmbedderGraphCallback(
      v8::HeapProfiler::BuildEmbedderGraphCallback callback, void* data);
  void RemoveBuildEmbedderGraphCallback(
      v8::HeapProfiler::BuildEmbedderGraphCallback callback, void* data);
  void BuildEmbedderGraph(Isolate* isolate, v8::EmbedderGraph* graph);
  bool HasBuildEmbedderGraphCallback() {
    return !build_embedder_graph_callbacks_.empty();
  }

  bool is_tracking_object_moves() const { return is_tracking_object_moves_; }

  Handle<HeapObject> FindHeapObjectById(SnapshotObjectId id);
  void ClearHeapObjectMap();

  Isolate* isolate() const;

  void QueryObjects(Handle<Context> context,
                    debug::QueryObjectPredicate* predicate,
                    v8::PersistentValueVector<v8::Object>* objects);

 private:
  void MaybeClearStringsStorage();

  Heap* heap() const;

  // Mapping from HeapObject addresses to objects' uids.
  std::unique_ptr<HeapObjectsMap> ids_;
  std::vector<std::unique_ptr<HeapSnapshot>> snapshots_;
  std::unique_ptr<StringsStorage> names_;
  std::unique_ptr<AllocationTracker> allocation_tracker_;
  bool is_tracking_object_moves_;
  base::Mutex profiler_mutex_;
  std::unique_ptr<SamplingHeapProfiler> sampling_heap_profiler_;
  std::vector<std::pair<v8::HeapProfiler::BuildEmbedderGraphCallback, void*>>
      build_embedder_graph_callbacks_;

  DISALLOW_COPY_AND_ASSIGN(HeapProfiler);
};

}  // namespace internal
}  // namespace v8

#endif  // V8_PROFILER_HEAP_PROFILER_H_
