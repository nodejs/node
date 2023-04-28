// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/inspector/v8-heap-profiler-agent-impl.h"

#include "include/v8-context.h"
#include "include/v8-inspector.h"
#include "include/v8-platform.h"
#include "include/v8-profiler.h"
#include "include/v8-version.h"
#include "src/base/platform/mutex.h"
#include "src/inspector/injected-script.h"
#include "src/inspector/inspected-context.h"
#include "src/inspector/protocol/Protocol.h"
#include "src/inspector/string-util.h"
#include "src/inspector/v8-debugger.h"
#include "src/inspector/v8-inspector-impl.h"
#include "src/inspector/v8-inspector-session-impl.h"

namespace v8_inspector {

namespace {

namespace HeapProfilerAgentState {
static const char heapProfilerEnabled[] = "heapProfilerEnabled";
static const char heapObjectsTrackingEnabled[] = "heapObjectsTrackingEnabled";
static const char allocationTrackingEnabled[] = "allocationTrackingEnabled";
static const char samplingHeapProfilerEnabled[] = "samplingHeapProfilerEnabled";
static const char samplingHeapProfilerInterval[] =
    "samplingHeapProfilerInterval";
static const char samplingHeapProfilerFlags[] = "samplingHeapProfilerFlags";
}  // namespace HeapProfilerAgentState

class HeapSnapshotProgress final : public v8::ActivityControl {
 public:
  explicit HeapSnapshotProgress(protocol::HeapProfiler::Frontend* frontend)
      : m_frontend(frontend) {}
  ControlOption ReportProgressValue(uint32_t done, uint32_t total) override {
    m_frontend->reportHeapSnapshotProgress(done, total,
                                           protocol::Maybe<bool>());
    if (done >= total) {
      m_frontend->reportHeapSnapshotProgress(total, total, true);
    }
    m_frontend->flush();
    return kContinue;
  }

 private:
  protocol::HeapProfiler::Frontend* m_frontend;
};

class GlobalObjectNameResolver final
    : public v8::HeapProfiler::ObjectNameResolver {
 public:
  explicit GlobalObjectNameResolver(V8InspectorSessionImpl* session)
      : m_offset(0), m_strings(10000), m_session(session) {}

  const char* GetName(v8::Local<v8::Object> object) override {
    v8::Local<v8::Context> creationContext;
    if (!object->GetCreationContext().ToLocal(&creationContext)) {
      return "";
    }
    InspectedContext* context = m_session->inspector()->getContext(
        m_session->contextGroupId(),
        InspectedContext::contextId(creationContext));
    if (!context) return "";
    String16 name = context->origin();
    size_t length = name.length();
    if (m_offset + length + 1 >= m_strings.size()) return "";
    for (size_t i = 0; i < length; ++i) {
      UChar ch = name[i];
      m_strings[m_offset + i] = ch > 0xFF ? '?' : static_cast<char>(ch);
    }
    m_strings[m_offset + length] = '\0';
    char* result = &*m_strings.begin() + m_offset;
    m_offset += length + 1;
    return result;
  }

 private:
  size_t m_offset;
  std::vector<char> m_strings;
  V8InspectorSessionImpl* m_session;
};

class HeapSnapshotOutputStream final : public v8::OutputStream {
 public:
  explicit HeapSnapshotOutputStream(protocol::HeapProfiler::Frontend* frontend)
      : m_frontend(frontend) {}
  void EndOfStream() override {}
  int GetChunkSize() override { return 102400; }
  WriteResult WriteAsciiChunk(char* data, int size) override {
    m_frontend->addHeapSnapshotChunk(String16(data, size));
    m_frontend->flush();
    return kContinue;
  }

 private:
  protocol::HeapProfiler::Frontend* m_frontend;
};

v8::Local<v8::Object> objectByHeapObjectId(v8::Isolate* isolate, int id) {
  v8::HeapProfiler* profiler = isolate->GetHeapProfiler();
  v8::Local<v8::Value> value = profiler->FindObjectById(id);
  if (value.IsEmpty() || !value->IsObject()) return v8::Local<v8::Object>();
  return value.As<v8::Object>();
}

class InspectableHeapObject final : public V8InspectorSession::Inspectable {
 public:
  explicit InspectableHeapObject(int heapObjectId)
      : m_heapObjectId(heapObjectId) {}
  v8::Local<v8::Value> get(v8::Local<v8::Context> context) override {
    return objectByHeapObjectId(context->GetIsolate(), m_heapObjectId);
  }

 private:
  int m_heapObjectId;
};

class HeapStatsStream final : public v8::OutputStream {
 public:
  explicit HeapStatsStream(protocol::HeapProfiler::Frontend* frontend)
      : m_frontend(frontend) {}

  void EndOfStream() override {}

  WriteResult WriteAsciiChunk(char* data, int size) override {
    DCHECK(false);
    return kAbort;
  }

  WriteResult WriteHeapStatsChunk(v8::HeapStatsUpdate* updateData,
                                  int count) override {
    DCHECK_GT(count, 0);
    auto statsDiff = std::make_unique<protocol::Array<int>>();
    for (int i = 0; i < count; ++i) {
      statsDiff->emplace_back(updateData[i].index);
      statsDiff->emplace_back(updateData[i].count);
      statsDiff->emplace_back(updateData[i].size);
    }
    m_frontend->heapStatsUpdate(std::move(statsDiff));
    return kContinue;
  }

 private:
  protocol::HeapProfiler::Frontend* m_frontend;
};

}  // namespace

struct V8HeapProfilerAgentImpl::AsyncGC {
  v8::base::Mutex m_mutex;
  bool m_canceled = false;
  bool m_pending = false;
  std::vector<std::unique_ptr<CollectGarbageCallback>> m_pending_callbacks;
};

class V8HeapProfilerAgentImpl::GCTask : public v8::Task {
 public:
  GCTask(v8::Isolate* isolate, std::shared_ptr<AsyncGC> async_gc)
      : m_isolate(isolate), m_async_gc(async_gc) {}

  void Run() override {
    std::shared_ptr<AsyncGC> async_gc = m_async_gc.lock();
    if (!async_gc) return;
    v8::base::MutexGuard lock(&async_gc->m_mutex);
    if (async_gc->m_canceled) return;
    v8::debug::ForceGarbageCollection(m_isolate,
                                      v8::StackState::kNoHeapPointers);
    for (auto& callback : async_gc->m_pending_callbacks) {
      callback->sendSuccess();
    }
    async_gc->m_pending_callbacks.clear();
  }

 private:
  v8::Isolate* m_isolate;
  std::weak_ptr<AsyncGC> m_async_gc;
};

V8HeapProfilerAgentImpl::V8HeapProfilerAgentImpl(
    V8InspectorSessionImpl* session, protocol::FrontendChannel* frontendChannel,
    protocol::DictionaryValue* state)
    : m_session(session),
      m_isolate(session->inspector()->isolate()),
      m_frontend(frontendChannel),
      m_state(state),
      m_hasTimer(false),
      m_async_gc(std::make_shared<AsyncGC>()) {}

V8HeapProfilerAgentImpl::~V8HeapProfilerAgentImpl() {
  v8::base::MutexGuard lock(&m_async_gc->m_mutex);
  m_async_gc->m_canceled = true;
  m_async_gc->m_pending_callbacks.clear();
}

void V8HeapProfilerAgentImpl::restore() {
  if (m_state->booleanProperty(HeapProfilerAgentState::heapProfilerEnabled,
                               false))
    m_frontend.resetProfiles();
  if (m_state->booleanProperty(
          HeapProfilerAgentState::heapObjectsTrackingEnabled, false))
    startTrackingHeapObjectsInternal(m_state->booleanProperty(
        HeapProfilerAgentState::allocationTrackingEnabled, false));
  if (m_state->booleanProperty(
          HeapProfilerAgentState::samplingHeapProfilerEnabled, false)) {
    double samplingInterval = m_state->doubleProperty(
        HeapProfilerAgentState::samplingHeapProfilerInterval, -1);
    DCHECK_GE(samplingInterval, 0);
    int flags = m_state->integerProperty(
        HeapProfilerAgentState::samplingHeapProfilerFlags, 0);
    startSampling(
        Maybe<double>(samplingInterval),
        Maybe<bool>(
            flags &
            v8::HeapProfiler::kSamplingIncludeObjectsCollectedByMajorGC),
        Maybe<bool>(
            flags &
            v8::HeapProfiler::kSamplingIncludeObjectsCollectedByMinorGC));
  }
}

void V8HeapProfilerAgentImpl::collectGarbage(
    std::unique_ptr<CollectGarbageCallback> callback) {
  v8::base::MutexGuard lock(&m_async_gc->m_mutex);
  m_async_gc->m_pending_callbacks.push_back(std::move(callback));
  if (!m_async_gc->m_pending) {
    v8::debug::GetCurrentPlatform()
        ->GetForegroundTaskRunner(m_isolate)
        ->PostNonNestableTask(std::make_unique<GCTask>(m_isolate, m_async_gc));
  }
}

Response V8HeapProfilerAgentImpl::startTrackingHeapObjects(
    Maybe<bool> trackAllocations) {
  m_state->setBoolean(HeapProfilerAgentState::heapObjectsTrackingEnabled, true);
  bool allocationTrackingEnabled = trackAllocations.fromMaybe(false);
  m_state->setBoolean(HeapProfilerAgentState::allocationTrackingEnabled,
                      allocationTrackingEnabled);
  startTrackingHeapObjectsInternal(allocationTrackingEnabled);
  return Response::Success();
}

Response V8HeapProfilerAgentImpl::stopTrackingHeapObjects(
    Maybe<bool> reportProgress, Maybe<bool> treatGlobalObjectsAsRoots,
    Maybe<bool> captureNumericValue, Maybe<bool> exposeInternals) {
  requestHeapStatsUpdate();
  takeHeapSnapshot(std::move(reportProgress),
                   std::move(treatGlobalObjectsAsRoots),
                   std::move(captureNumericValue), std::move(exposeInternals));
  stopTrackingHeapObjectsInternal();
  return Response::Success();
}

Response V8HeapProfilerAgentImpl::enable() {
  m_state->setBoolean(HeapProfilerAgentState::heapProfilerEnabled, true);
  return Response::Success();
}

Response V8HeapProfilerAgentImpl::disable() {
  stopTrackingHeapObjectsInternal();
  if (m_state->booleanProperty(
          HeapProfilerAgentState::samplingHeapProfilerEnabled, false)) {
    v8::HeapProfiler* profiler = m_isolate->GetHeapProfiler();
    if (profiler) profiler->StopSamplingHeapProfiler();
  }
  m_isolate->GetHeapProfiler()->ClearObjectIds();
  m_state->setBoolean(HeapProfilerAgentState::heapProfilerEnabled, false);
  return Response::Success();
}

Response V8HeapProfilerAgentImpl::takeHeapSnapshot(
    Maybe<bool> reportProgress, Maybe<bool> treatGlobalObjectsAsRoots,
    Maybe<bool> captureNumericValue, Maybe<bool> exposeInternals) {
  v8::HeapProfiler* profiler = m_isolate->GetHeapProfiler();
  if (!profiler) return Response::ServerError("Cannot access v8 heap profiler");
  std::unique_ptr<HeapSnapshotProgress> progress;
  if (reportProgress.fromMaybe(false))
    progress.reset(new HeapSnapshotProgress(&m_frontend));

  GlobalObjectNameResolver resolver(m_session);
  v8::HeapProfiler::HeapSnapshotOptions options;
  options.global_object_name_resolver = &resolver;
  options.control = progress.get();
  options.snapshot_mode =
      exposeInternals.fromMaybe(false) ||
              // Not treating global objects as roots results into exposing
              // internals.
              !treatGlobalObjectsAsRoots.fromMaybe(true)
          ? v8::HeapProfiler::HeapSnapshotMode::kExposeInternals
          : v8::HeapProfiler::HeapSnapshotMode::kRegular;
  options.numerics_mode =
      captureNumericValue.fromMaybe(false)
          ? v8::HeapProfiler::NumericsMode::kExposeNumericValues
          : v8::HeapProfiler::NumericsMode::kHideNumericValues;
  const v8::HeapSnapshot* snapshot = profiler->TakeHeapSnapshot(options);
  if (!snapshot) return Response::ServerError("Failed to take heap snapshot");
  HeapSnapshotOutputStream stream(&m_frontend);
  snapshot->Serialize(&stream);
  const_cast<v8::HeapSnapshot*>(snapshot)->Delete();
  return Response::Success();
}

Response V8HeapProfilerAgentImpl::getObjectByHeapObjectId(
    const String16& heapSnapshotObjectId, Maybe<String16> objectGroup,
    std::unique_ptr<protocol::Runtime::RemoteObject>* result) {
  bool ok;
  int id = heapSnapshotObjectId.toInteger(&ok);
  if (!ok) return Response::ServerError("Invalid heap snapshot object id");

  v8::HandleScope handles(m_isolate);
  v8::Local<v8::Object> heapObject = objectByHeapObjectId(m_isolate, id);
  if (heapObject.IsEmpty())
    return Response::ServerError("Object is not available");

  if (!m_session->inspector()->client()->isInspectableHeapObject(heapObject))
    return Response::ServerError("Object is not available");

  v8::Local<v8::Context> creationContext;
  if (!heapObject->GetCreationContext().ToLocal(&creationContext)) {
    return Response::ServerError("Object is not available");
  }
  *result = m_session->wrapObject(creationContext, heapObject,
                                  objectGroup.fromMaybe(""), false);
  if (!*result) return Response::ServerError("Object is not available");
  return Response::Success();
}

Response V8HeapProfilerAgentImpl::addInspectedHeapObject(
    const String16& inspectedHeapObjectId) {
  bool ok;
  int id = inspectedHeapObjectId.toInteger(&ok);
  if (!ok) return Response::ServerError("Invalid heap snapshot object id");

  v8::HandleScope handles(m_isolate);
  v8::Local<v8::Object> heapObject = objectByHeapObjectId(m_isolate, id);
  if (heapObject.IsEmpty())
    return Response::ServerError("Object is not available");

  if (!m_session->inspector()->client()->isInspectableHeapObject(heapObject))
    return Response::ServerError("Object is not available");
  m_session->addInspectedObject(
      std::unique_ptr<InspectableHeapObject>(new InspectableHeapObject(id)));
  return Response::Success();
}

Response V8HeapProfilerAgentImpl::getHeapObjectId(
    const String16& objectId, String16* heapSnapshotObjectId) {
  v8::HandleScope handles(m_isolate);
  v8::Local<v8::Value> value;
  v8::Local<v8::Context> context;
  Response response =
      m_session->unwrapObject(objectId, &value, &context, nullptr);
  if (!response.IsSuccess()) return response;
  if (value->IsUndefined()) return Response::InternalError();

  v8::SnapshotObjectId id = m_isolate->GetHeapProfiler()->GetObjectId(value);
  *heapSnapshotObjectId = String16::fromInteger(static_cast<size_t>(id));
  return Response::Success();
}

void V8HeapProfilerAgentImpl::requestHeapStatsUpdate() {
  HeapStatsStream stream(&m_frontend);
  v8::SnapshotObjectId lastSeenObjectId =
      m_isolate->GetHeapProfiler()->GetHeapStats(&stream);
  m_frontend.lastSeenObjectId(
      lastSeenObjectId, m_session->inspector()->client()->currentTimeMS());
}

// static
void V8HeapProfilerAgentImpl::onTimer(void* data) {
  reinterpret_cast<V8HeapProfilerAgentImpl*>(data)->requestHeapStatsUpdate();
}

void V8HeapProfilerAgentImpl::startTrackingHeapObjectsInternal(
    bool trackAllocations) {
  m_isolate->GetHeapProfiler()->StartTrackingHeapObjects(trackAllocations);
  if (!m_hasTimer) {
    m_hasTimer = true;
    m_session->inspector()->client()->startRepeatingTimer(
        0.05, &V8HeapProfilerAgentImpl::onTimer, reinterpret_cast<void*>(this));
  }
}

void V8HeapProfilerAgentImpl::stopTrackingHeapObjectsInternal() {
  if (m_hasTimer) {
    m_session->inspector()->client()->cancelTimer(
        reinterpret_cast<void*>(this));
    m_hasTimer = false;
  }
  m_isolate->GetHeapProfiler()->StopTrackingHeapObjects();
  m_state->setBoolean(HeapProfilerAgentState::heapObjectsTrackingEnabled,
                      false);
  m_state->setBoolean(HeapProfilerAgentState::allocationTrackingEnabled, false);
}

Response V8HeapProfilerAgentImpl::startSampling(
    Maybe<double> samplingInterval,
    Maybe<bool> includeObjectsCollectedByMajorGC,
    Maybe<bool> includeObjectsCollectedByMinorGC) {
  v8::HeapProfiler* profiler = m_isolate->GetHeapProfiler();
  if (!profiler) return Response::ServerError("Cannot access v8 heap profiler");
  const unsigned defaultSamplingInterval = 1 << 15;
  double samplingIntervalValue =
      samplingInterval.fromMaybe(defaultSamplingInterval);
  if (samplingIntervalValue <= 0.0) {
    return Response::ServerError("Invalid sampling interval");
  }
  m_state->setDouble(HeapProfilerAgentState::samplingHeapProfilerInterval,
                     samplingIntervalValue);
  m_state->setBoolean(HeapProfilerAgentState::samplingHeapProfilerEnabled,
                      true);
  int flags = v8::HeapProfiler::kSamplingForceGC;
  if (includeObjectsCollectedByMajorGC.fromMaybe(false)) {
    flags |= v8::HeapProfiler::kSamplingIncludeObjectsCollectedByMajorGC;
  }
  if (includeObjectsCollectedByMinorGC.fromMaybe(false)) {
    flags |= v8::HeapProfiler::kSamplingIncludeObjectsCollectedByMinorGC;
  }
  m_state->setInteger(HeapProfilerAgentState::samplingHeapProfilerFlags, flags);
  profiler->StartSamplingHeapProfiler(
      static_cast<uint64_t>(samplingIntervalValue), 128,
      static_cast<v8::HeapProfiler::SamplingFlags>(flags));
  return Response::Success();
}

namespace {
std::unique_ptr<protocol::HeapProfiler::SamplingHeapProfileNode>
buildSampingHeapProfileNode(v8::Isolate* isolate,
                            const v8::AllocationProfile::Node* node) {
  auto children = std::make_unique<
      protocol::Array<protocol::HeapProfiler::SamplingHeapProfileNode>>();
  for (const auto* child : node->children)
    children->emplace_back(buildSampingHeapProfileNode(isolate, child));
  size_t selfSize = 0;
  for (const auto& allocation : node->allocations)
    selfSize += allocation.size * allocation.count;
  std::unique_ptr<protocol::Runtime::CallFrame> callFrame =
      protocol::Runtime::CallFrame::create()
          .setFunctionName(toProtocolString(isolate, node->name))
          .setScriptId(String16::fromInteger(node->script_id))
          .setUrl(toProtocolString(isolate, node->script_name))
          .setLineNumber(node->line_number - 1)
          .setColumnNumber(node->column_number - 1)
          .build();
  std::unique_ptr<protocol::HeapProfiler::SamplingHeapProfileNode> result =
      protocol::HeapProfiler::SamplingHeapProfileNode::create()
          .setCallFrame(std::move(callFrame))
          .setSelfSize(selfSize)
          .setChildren(std::move(children))
          .setId(node->node_id)
          .build();
  return result;
}
}  // namespace

Response V8HeapProfilerAgentImpl::stopSampling(
    std::unique_ptr<protocol::HeapProfiler::SamplingHeapProfile>* profile) {
  Response result = getSamplingProfile(profile);
  if (result.IsSuccess()) {
    m_isolate->GetHeapProfiler()->StopSamplingHeapProfiler();
    m_state->setBoolean(HeapProfilerAgentState::samplingHeapProfilerEnabled,
                        false);
  }
  return result;
}

Response V8HeapProfilerAgentImpl::getSamplingProfile(
    std::unique_ptr<protocol::HeapProfiler::SamplingHeapProfile>* profile) {
  v8::HeapProfiler* profiler = m_isolate->GetHeapProfiler();
  // Need a scope as v8::AllocationProfile contains Local handles.
  v8::HandleScope scope(m_isolate);
  std::unique_ptr<v8::AllocationProfile> v8Profile(
      profiler->GetAllocationProfile());
  if (!v8Profile)
    return Response::ServerError("V8 sampling heap profiler was not started.");
  v8::AllocationProfile::Node* root = v8Profile->GetRootNode();
  auto samples = std::make_unique<
      protocol::Array<protocol::HeapProfiler::SamplingHeapProfileSample>>();
  for (const auto& sample : v8Profile->GetSamples()) {
    samples->emplace_back(
        protocol::HeapProfiler::SamplingHeapProfileSample::create()
            .setSize(sample.size * sample.count)
            .setNodeId(sample.node_id)
            .setOrdinal(static_cast<double>(sample.sample_id))
            .build());
  }
  *profile = protocol::HeapProfiler::SamplingHeapProfile::create()
                 .setHead(buildSampingHeapProfileNode(m_isolate, root))
                 .setSamples(std::move(samples))
                 .build();
  return Response::Success();
}

}  // namespace v8_inspector
