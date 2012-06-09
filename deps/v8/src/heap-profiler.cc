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

#include "heap-profiler.h"
#include "profile-generator.h"

namespace v8 {
namespace internal {


HeapProfiler::HeapProfiler()
    : snapshots_(new HeapSnapshotsCollection()),
      next_snapshot_uid_(1) {
}


HeapProfiler::~HeapProfiler() {
  delete snapshots_;
}


void HeapProfiler::ResetSnapshots() {
  delete snapshots_;
  snapshots_ = new HeapSnapshotsCollection();
}


void HeapProfiler::SetUp() {
  Isolate* isolate = Isolate::Current();
  if (isolate->heap_profiler() == NULL) {
    isolate->set_heap_profiler(new HeapProfiler());
  }
}


void HeapProfiler::TearDown() {
  Isolate* isolate = Isolate::Current();
  delete isolate->heap_profiler();
  isolate->set_heap_profiler(NULL);
}


HeapSnapshot* HeapProfiler::TakeSnapshot(const char* name,
                                         int type,
                                         v8::ActivityControl* control) {
  ASSERT(Isolate::Current()->heap_profiler() != NULL);
  return Isolate::Current()->heap_profiler()->TakeSnapshotImpl(name,
                                                               type,
                                                               control);
}


HeapSnapshot* HeapProfiler::TakeSnapshot(String* name,
                                         int type,
                                         v8::ActivityControl* control) {
  ASSERT(Isolate::Current()->heap_profiler() != NULL);
  return Isolate::Current()->heap_profiler()->TakeSnapshotImpl(name,
                                                               type,
                                                               control);
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


HeapSnapshot* HeapProfiler::TakeSnapshotImpl(const char* name,
                                             int type,
                                             v8::ActivityControl* control) {
  HeapSnapshot::Type s_type = static_cast<HeapSnapshot::Type>(type);
  HeapSnapshot* result =
      snapshots_->NewSnapshot(s_type, name, next_snapshot_uid_++);
  bool generation_completed = true;
  switch (s_type) {
    case HeapSnapshot::kFull: {
      HeapSnapshotGenerator generator(result, control);
      generation_completed = generator.GenerateSnapshot();
      break;
    }
    default:
      UNREACHABLE();
  }
  if (!generation_completed) {
    delete result;
    result = NULL;
  }
  snapshots_->SnapshotGenerationFinished(result);
  return result;
}


HeapSnapshot* HeapProfiler::TakeSnapshotImpl(String* name,
                                             int type,
                                             v8::ActivityControl* control) {
  return TakeSnapshotImpl(snapshots_->names()->GetName(name), type, control);
}


int HeapProfiler::GetSnapshotsCount() {
  HeapProfiler* profiler = Isolate::Current()->heap_profiler();
  ASSERT(profiler != NULL);
  return profiler->snapshots_->snapshots()->length();
}


HeapSnapshot* HeapProfiler::GetSnapshot(int index) {
  HeapProfiler* profiler = Isolate::Current()->heap_profiler();
  ASSERT(profiler != NULL);
  return profiler->snapshots_->snapshots()->at(index);
}


HeapSnapshot* HeapProfiler::FindSnapshot(unsigned uid) {
  HeapProfiler* profiler = Isolate::Current()->heap_profiler();
  ASSERT(profiler != NULL);
  return profiler->snapshots_->GetSnapshot(uid);
}


void HeapProfiler::DeleteAllSnapshots() {
  HeapProfiler* profiler = Isolate::Current()->heap_profiler();
  ASSERT(profiler != NULL);
  profiler->ResetSnapshots();
}


void HeapProfiler::ObjectMoveEvent(Address from, Address to) {
  snapshots_->ObjectMoveEvent(from, to);
}


} }  // namespace v8::internal
