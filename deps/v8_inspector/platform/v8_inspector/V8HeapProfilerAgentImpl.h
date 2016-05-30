// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8HeapProfilerAgentImpl_h
#define V8HeapProfilerAgentImpl_h

#include "platform/inspector_protocol/Allocator.h"
#include "platform/v8_inspector/public/V8HeapProfilerAgent.h"

namespace blink {

class V8InspectorSessionImpl;

using protocol::Maybe;

class V8HeapProfilerAgentImpl : public V8HeapProfilerAgent {
    PROTOCOL_DISALLOW_COPY(V8HeapProfilerAgentImpl);
public:
    explicit V8HeapProfilerAgentImpl(V8InspectorSessionImpl*);
    ~V8HeapProfilerAgentImpl() override;

    void setInspectorState(protocol::DictionaryValue* state) override { m_state = state; }
    void setFrontend(protocol::Frontend::HeapProfiler* frontend) override { m_frontend = frontend; }
    void clearFrontend() override;
    void restore() override;

    void collectGarbage(ErrorString*) override;

    void enable(ErrorString*) override;
    void startTrackingHeapObjects(ErrorString*, const Maybe<bool>& trackAllocations) override;
    void stopTrackingHeapObjects(ErrorString*, const Maybe<bool>& reportProgress) override;

    void disable(ErrorString*) override;

    void takeHeapSnapshot(ErrorString*, const Maybe<bool>& reportProgress) override;

    void getObjectByHeapObjectId(ErrorString*, const String16& heapSnapshotObjectId, const Maybe<String16>& objectGroup, std::unique_ptr<protocol::Runtime::RemoteObject>* result) override;
    void addInspectedHeapObject(ErrorString*, const String16& inspectedHeapObjectId) override;
    void getHeapObjectId(ErrorString*, const String16& objectId, String16* heapSnapshotObjectId) override;

    void startSampling(ErrorString*, const Maybe<double>& samplingInterval) override;
    void stopSampling(ErrorString*, std::unique_ptr<protocol::HeapProfiler::SamplingHeapProfile>*) override;

private:
    void startTrackingHeapObjectsInternal(bool trackAllocations);
    void stopTrackingHeapObjectsInternal();
    void requestHeapStatsUpdate();
    static void onTimer(void*);

    V8InspectorSessionImpl* m_session;
    v8::Isolate* m_isolate;
    protocol::Frontend::HeapProfiler* m_frontend;
    protocol::DictionaryValue* m_state;
    bool m_hasTimer;
};

} // namespace blink

#endif // !defined(V8HeapProfilerAgentImpl_h)
