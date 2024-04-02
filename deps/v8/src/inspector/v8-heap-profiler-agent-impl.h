// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_INSPECTOR_V8_HEAP_PROFILER_AGENT_IMPL_H_
#define V8_INSPECTOR_V8_HEAP_PROFILER_AGENT_IMPL_H_

#include <memory>

#include "src/base/macros.h"
#include "src/inspector/protocol/Forward.h"
#include "src/inspector/protocol/HeapProfiler.h"

namespace v8 {
class Isolate;
}

namespace v8_inspector {

class V8InspectorSessionImpl;

using protocol::Maybe;
using protocol::Response;

class V8HeapProfilerAgentImpl : public protocol::HeapProfiler::Backend {
 public:
  V8HeapProfilerAgentImpl(V8InspectorSessionImpl*, protocol::FrontendChannel*,
                          protocol::DictionaryValue* state);
  ~V8HeapProfilerAgentImpl() override;
  V8HeapProfilerAgentImpl(const V8HeapProfilerAgentImpl&) = delete;
  V8HeapProfilerAgentImpl& operator=(const V8HeapProfilerAgentImpl&) = delete;
  void restore();

  void collectGarbage(
      std::unique_ptr<CollectGarbageCallback> callback) override;

  Response enable() override;
  Response startTrackingHeapObjects(Maybe<bool> trackAllocations) override;
  Response stopTrackingHeapObjects(Maybe<bool> reportProgress,
                                   Maybe<bool> treatGlobalObjectsAsRoots,
                                   Maybe<bool> captureNumericValue) override;

  Response disable() override;

  Response takeHeapSnapshot(Maybe<bool> reportProgress,
                            Maybe<bool> treatGlobalObjectsAsRoots,
                            Maybe<bool> captureNumericValue) override;

  Response getObjectByHeapObjectId(
      const String16& heapSnapshotObjectId, Maybe<String16> objectGroup,
      std::unique_ptr<protocol::Runtime::RemoteObject>* result) override;
  Response addInspectedHeapObject(
      const String16& inspectedHeapObjectId) override;
  Response getHeapObjectId(const String16& objectId,
                           String16* heapSnapshotObjectId) override;

  Response startSampling(Maybe<double> samplingInterval) override;
  Response stopSampling(
      std::unique_ptr<protocol::HeapProfiler::SamplingHeapProfile>*) override;
  Response getSamplingProfile(
      std::unique_ptr<protocol::HeapProfiler::SamplingHeapProfile>*) override;

 private:
  struct AsyncGC;
  class GCTask;

  void startTrackingHeapObjectsInternal(bool trackAllocations);
  void stopTrackingHeapObjectsInternal();
  void requestHeapStatsUpdate();
  static void onTimer(void*);

  V8InspectorSessionImpl* m_session;
  v8::Isolate* m_isolate;
  protocol::HeapProfiler::Frontend m_frontend;
  protocol::DictionaryValue* m_state;
  bool m_hasTimer;
  std::shared_ptr<AsyncGC> m_async_gc;
};

}  // namespace v8_inspector

#endif  // V8_INSPECTOR_V8_HEAP_PROFILER_AGENT_IMPL_H_
