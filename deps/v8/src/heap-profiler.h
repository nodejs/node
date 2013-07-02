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

#ifndef V8_HEAP_PROFILER_H_
#define V8_HEAP_PROFILER_H_

#include "heap-snapshot-generator-inl.h"
#include "isolate.h"

namespace v8 {
namespace internal {

class HeapSnapshot;
class HeapSnapshotsCollection;

#define HEAP_PROFILE(heap, call)                                             \
  do {                                                                       \
    v8::internal::HeapProfiler* profiler = heap->isolate()->heap_profiler(); \
    if (profiler != NULL && profiler->is_profiling()) {                      \
      profiler->call;                                                        \
    }                                                                        \
  } while (false)

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

  void StartHeapObjectsTracking();
  void StopHeapObjectsTracking();
  SnapshotObjectId PushHeapObjectsStats(OutputStream* stream);
  int GetSnapshotsCount();
  HeapSnapshot* GetSnapshot(int index);
  SnapshotObjectId GetSnapshotObjectId(Handle<Object> obj);
  void DeleteAllSnapshots();

  void ObjectMoveEvent(Address from, Address to);

  void DefineWrapperClass(
      uint16_t class_id, v8::HeapProfiler::WrapperInfoCallback callback);

  v8::RetainedObjectInfo* ExecuteWrapperClassCallback(uint16_t class_id,
                                                      Object** wrapper);
  INLINE(bool is_profiling()) {
    return snapshots_->is_tracking_objects();
  }

  void SetRetainedObjectInfo(UniqueId id, RetainedObjectInfo* info);

 private:
  Heap* heap() const { return snapshots_->heap(); }

  HeapSnapshotsCollection* snapshots_;
  unsigned next_snapshot_uid_;
  List<v8::HeapProfiler::WrapperInfoCallback> wrapper_callbacks_;
};

} }  // namespace v8::internal

#endif  // V8_HEAP_PROFILER_H_
