// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_INSPECTOR_V8HEAPPROFILERAGENTIMPL_H_
#define V8_INSPECTOR_V8HEAPPROFILERAGENTIMPL_H_

#include "src/base/macros.h"
#include "src/inspector/protocol/Forward.h"
#include "src/inspector/protocol/HeapProfiler.h"

#include "include/v8.h"

namespace v8_inspector {

class V8InspectorSessionImpl;

using protocol::ErrorString;
using protocol::Maybe;

class V8HeapProfilerAgentImpl : public protocol::HeapProfiler::Backend {
 public:
  V8HeapProfilerAgentImpl(V8InspectorSessionImpl*, protocol::FrontendChannel*,
                          protocol::DictionaryValue* state);
  ~V8HeapProfilerAgentImpl() override;
  void restore();

  void collectGarbage(ErrorString*) override;

  void enable(ErrorString*) override;
  void startTrackingHeapObjects(ErrorString*,
                                const Maybe<bool>& trackAllocations) override;
  void stopTrackingHeapObjects(ErrorString*,
                               const Maybe<bool>& reportProgress) override;

  void disable(ErrorString*) override;

  void takeHeapSnapshot(ErrorString*,
                        const Maybe<bool>& reportProgress) override;

  void getObjectByHeapObjectId(
      ErrorString*, const String16& heapSnapshotObjectId,
      const Maybe<String16>& objectGroup,
      std::unique_ptr<protocol::Runtime::RemoteObject>* result) override;
  void addInspectedHeapObject(ErrorString*,
                              const String16& inspectedHeapObjectId) override;
  void getHeapObjectId(ErrorString*, const String16& objectId,
                       String16* heapSnapshotObjectId) override;

  void startSampling(ErrorString*,
                     const Maybe<double>& samplingInterval) override;
  void stopSampling(
      ErrorString*,
      std::unique_ptr<protocol::HeapProfiler::SamplingHeapProfile>*) override;

 private:
  void startTrackingHeapObjectsInternal(bool trackAllocations);
  void stopTrackingHeapObjectsInternal();
  void requestHeapStatsUpdate();
  static void onTimer(void*);

  V8InspectorSessionImpl* m_session;
  v8::Isolate* m_isolate;
  protocol::HeapProfiler::Frontend m_frontend;
  protocol::DictionaryValue* m_state;
  bool m_hasTimer;

  DISALLOW_COPY_AND_ASSIGN(V8HeapProfilerAgentImpl);
};

}  // namespace v8_inspector

#endif  // V8_INSPECTOR_V8HEAPPROFILERAGENTIMPL_H_
