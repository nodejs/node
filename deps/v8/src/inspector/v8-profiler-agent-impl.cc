// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/inspector/v8-profiler-agent-impl.h"

#include <vector>

#include "src/base/atomicops.h"
#include "src/inspector/protocol/Protocol.h"
#include "src/inspector/string-util.h"
#include "src/inspector/v8-debugger.h"
#include "src/inspector/v8-inspector-impl.h"
#include "src/inspector/v8-inspector-session-impl.h"
#include "src/inspector/v8-stack-trace-impl.h"

#include "include/v8-profiler.h"

namespace v8_inspector {

namespace ProfilerAgentState {
static const char samplingInterval[] = "samplingInterval";
static const char userInitiatedProfiling[] = "userInitiatedProfiling";
static const char profilerEnabled[] = "profilerEnabled";
static const char preciseCoverageStarted[] = "preciseCoverageStarted";
static const char preciseCoverageCallCount[] = "preciseCoverageCallCount";
}

namespace {

std::unique_ptr<protocol::Array<protocol::Profiler::PositionTickInfo>>
buildInspectorObjectForPositionTicks(const v8::CpuProfileNode* node) {
  unsigned lineCount = node->GetHitLineCount();
  if (!lineCount) return nullptr;
  auto array = protocol::Array<protocol::Profiler::PositionTickInfo>::create();
  std::vector<v8::CpuProfileNode::LineTick> entries(lineCount);
  if (node->GetLineTicks(&entries[0], lineCount)) {
    for (unsigned i = 0; i < lineCount; i++) {
      std::unique_ptr<protocol::Profiler::PositionTickInfo> line =
          protocol::Profiler::PositionTickInfo::create()
              .setLine(entries[i].line)
              .setTicks(entries[i].hit_count)
              .build();
      array->addItem(std::move(line));
    }
  }
  return array;
}

std::unique_ptr<protocol::Profiler::ProfileNode> buildInspectorObjectFor(
    v8::Isolate* isolate, const v8::CpuProfileNode* node) {
  v8::HandleScope handleScope(isolate);
  auto callFrame =
      protocol::Runtime::CallFrame::create()
          .setFunctionName(toProtocolString(node->GetFunctionName()))
          .setScriptId(String16::fromInteger(node->GetScriptId()))
          .setUrl(toProtocolString(node->GetScriptResourceName()))
          .setLineNumber(node->GetLineNumber() - 1)
          .setColumnNumber(node->GetColumnNumber() - 1)
          .build();
  auto result = protocol::Profiler::ProfileNode::create()
                    .setCallFrame(std::move(callFrame))
                    .setHitCount(node->GetHitCount())
                    .setId(node->GetNodeId())
                    .build();

  const int childrenCount = node->GetChildrenCount();
  if (childrenCount) {
    auto children = protocol::Array<int>::create();
    for (int i = 0; i < childrenCount; i++)
      children->addItem(node->GetChild(i)->GetNodeId());
    result->setChildren(std::move(children));
  }

  const char* deoptReason = node->GetBailoutReason();
  if (deoptReason && deoptReason[0] && strcmp(deoptReason, "no reason"))
    result->setDeoptReason(deoptReason);

  auto positionTicks = buildInspectorObjectForPositionTicks(node);
  if (positionTicks) result->setPositionTicks(std::move(positionTicks));

  return result;
}

std::unique_ptr<protocol::Array<int>> buildInspectorObjectForSamples(
    v8::CpuProfile* v8profile) {
  auto array = protocol::Array<int>::create();
  int count = v8profile->GetSamplesCount();
  for (int i = 0; i < count; i++)
    array->addItem(v8profile->GetSample(i)->GetNodeId());
  return array;
}

std::unique_ptr<protocol::Array<int>> buildInspectorObjectForTimestamps(
    v8::CpuProfile* v8profile) {
  auto array = protocol::Array<int>::create();
  int count = v8profile->GetSamplesCount();
  uint64_t lastTime = v8profile->GetStartTime();
  for (int i = 0; i < count; i++) {
    uint64_t ts = v8profile->GetSampleTimestamp(i);
    array->addItem(static_cast<int>(ts - lastTime));
    lastTime = ts;
  }
  return array;
}

void flattenNodesTree(v8::Isolate* isolate, const v8::CpuProfileNode* node,
                      protocol::Array<protocol::Profiler::ProfileNode>* list) {
  list->addItem(buildInspectorObjectFor(isolate, node));
  const int childrenCount = node->GetChildrenCount();
  for (int i = 0; i < childrenCount; i++)
    flattenNodesTree(isolate, node->GetChild(i), list);
}

std::unique_ptr<protocol::Profiler::Profile> createCPUProfile(
    v8::Isolate* isolate, v8::CpuProfile* v8profile) {
  auto nodes = protocol::Array<protocol::Profiler::ProfileNode>::create();
  flattenNodesTree(isolate, v8profile->GetTopDownRoot(), nodes.get());
  return protocol::Profiler::Profile::create()
      .setNodes(std::move(nodes))
      .setStartTime(static_cast<double>(v8profile->GetStartTime()))
      .setEndTime(static_cast<double>(v8profile->GetEndTime()))
      .setSamples(buildInspectorObjectForSamples(v8profile))
      .setTimeDeltas(buildInspectorObjectForTimestamps(v8profile))
      .build();
}

std::unique_ptr<protocol::Debugger::Location> currentDebugLocation(
    V8InspectorImpl* inspector) {
  std::unique_ptr<V8StackTraceImpl> callStack =
      inspector->debugger()->captureStackTrace(false /* fullStack */);
  auto location = protocol::Debugger::Location::create()
                      .setScriptId(toString16(callStack->topScriptId()))
                      .setLineNumber(callStack->topLineNumber())
                      .build();
  location->setColumnNumber(callStack->topColumnNumber());
  return location;
}

volatile int s_lastProfileId = 0;

}  // namespace

class V8ProfilerAgentImpl::ProfileDescriptor {
 public:
  ProfileDescriptor(const String16& id, const String16& title)
      : m_id(id), m_title(title) {}
  String16 m_id;
  String16 m_title;
};

V8ProfilerAgentImpl::V8ProfilerAgentImpl(
    V8InspectorSessionImpl* session, protocol::FrontendChannel* frontendChannel,
    protocol::DictionaryValue* state)
    : m_session(session),
      m_isolate(m_session->inspector()->isolate()),
      m_state(state),
      m_frontend(frontendChannel) {}

V8ProfilerAgentImpl::~V8ProfilerAgentImpl() {
  if (m_profiler) m_profiler->Dispose();
}

void V8ProfilerAgentImpl::consoleProfile(const String16& title) {
  if (!m_enabled) return;
  String16 id = nextProfileId();
  m_startedProfiles.push_back(ProfileDescriptor(id, title));
  startProfiling(id);
  m_frontend.consoleProfileStarted(
      id, currentDebugLocation(m_session->inspector()), title);
}

void V8ProfilerAgentImpl::consoleProfileEnd(const String16& title) {
  if (!m_enabled) return;
  String16 id;
  String16 resolvedTitle;
  // Take last started profile if no title was passed.
  if (title.isEmpty()) {
    if (m_startedProfiles.empty()) return;
    id = m_startedProfiles.back().m_id;
    resolvedTitle = m_startedProfiles.back().m_title;
    m_startedProfiles.pop_back();
  } else {
    for (size_t i = 0; i < m_startedProfiles.size(); i++) {
      if (m_startedProfiles[i].m_title == title) {
        resolvedTitle = title;
        id = m_startedProfiles[i].m_id;
        m_startedProfiles.erase(m_startedProfiles.begin() + i);
        break;
      }
    }
    if (id.isEmpty()) return;
  }
  std::unique_ptr<protocol::Profiler::Profile> profile =
      stopProfiling(id, true);
  if (!profile) return;
  std::unique_ptr<protocol::Debugger::Location> location =
      currentDebugLocation(m_session->inspector());
  m_frontend.consoleProfileFinished(id, std::move(location), std::move(profile),
                                    resolvedTitle);
}

Response V8ProfilerAgentImpl::enable() {
  if (m_enabled) return Response::OK();
  m_enabled = true;
  m_state->setBoolean(ProfilerAgentState::profilerEnabled, true);
  return Response::OK();
}

Response V8ProfilerAgentImpl::disable() {
  if (!m_enabled) return Response::OK();
  for (size_t i = m_startedProfiles.size(); i > 0; --i)
    stopProfiling(m_startedProfiles[i - 1].m_id, false);
  m_startedProfiles.clear();
  stop(nullptr);
  stopPreciseCoverage();
  DCHECK(!m_profiler);
  m_enabled = false;
  m_state->setBoolean(ProfilerAgentState::profilerEnabled, false);
  return Response::OK();
}

Response V8ProfilerAgentImpl::setSamplingInterval(int interval) {
  if (m_profiler) {
    return Response::Error("Cannot change sampling interval when profiling.");
  }
  m_state->setInteger(ProfilerAgentState::samplingInterval, interval);
  return Response::OK();
}

void V8ProfilerAgentImpl::restore() {
  DCHECK(!m_enabled);
  if (!m_state->booleanProperty(ProfilerAgentState::profilerEnabled, false))
    return;
  m_enabled = true;
  DCHECK(!m_profiler);
  if (m_state->booleanProperty(ProfilerAgentState::userInitiatedProfiling,
                               false)) {
    start();
  }
  if (m_state->booleanProperty(ProfilerAgentState::preciseCoverageStarted,
                               false)) {
    bool callCount = m_state->booleanProperty(
        ProfilerAgentState::preciseCoverageCallCount, false);
    startPreciseCoverage(Maybe<bool>(callCount));
  }
}

Response V8ProfilerAgentImpl::start() {
  if (m_recordingCPUProfile) return Response::OK();
  if (!m_enabled) return Response::Error("Profiler is not enabled");
  m_recordingCPUProfile = true;
  m_frontendInitiatedProfileId = nextProfileId();
  startProfiling(m_frontendInitiatedProfileId);
  m_state->setBoolean(ProfilerAgentState::userInitiatedProfiling, true);
  return Response::OK();
}

Response V8ProfilerAgentImpl::stop(
    std::unique_ptr<protocol::Profiler::Profile>* profile) {
  if (!m_recordingCPUProfile) {
    return Response::Error("No recording profiles found");
  }
  m_recordingCPUProfile = false;
  std::unique_ptr<protocol::Profiler::Profile> cpuProfile =
      stopProfiling(m_frontendInitiatedProfileId, !!profile);
  if (profile) {
    *profile = std::move(cpuProfile);
    if (!profile->get()) return Response::Error("Profile is not found");
  }
  m_frontendInitiatedProfileId = String16();
  m_state->setBoolean(ProfilerAgentState::userInitiatedProfiling, false);
  return Response::OK();
}

Response V8ProfilerAgentImpl::startPreciseCoverage(Maybe<bool> callCount) {
  if (!m_enabled) return Response::Error("Profiler is not enabled");
  bool callCountValue = callCount.fromMaybe(false);
  m_state->setBoolean(ProfilerAgentState::preciseCoverageStarted, true);
  m_state->setBoolean(ProfilerAgentState::preciseCoverageCallCount,
                      callCountValue);
  v8::debug::Coverage::SelectMode(
      m_isolate, callCountValue ? v8::debug::Coverage::kPreciseCount
                                : v8::debug::Coverage::kPreciseBinary);
  return Response::OK();
}

Response V8ProfilerAgentImpl::stopPreciseCoverage() {
  if (!m_enabled) return Response::Error("Profiler is not enabled");
  m_state->setBoolean(ProfilerAgentState::preciseCoverageStarted, false);
  m_state->setBoolean(ProfilerAgentState::preciseCoverageCallCount, false);
  v8::debug::Coverage::SelectMode(m_isolate, v8::debug::Coverage::kBestEffort);
  return Response::OK();
}

namespace {
Response coverageToProtocol(
    v8::Isolate* isolate, const v8::debug::Coverage& coverage,
    std::unique_ptr<protocol::Array<protocol::Profiler::ScriptCoverage>>*
        out_result) {
  std::unique_ptr<protocol::Array<protocol::Profiler::ScriptCoverage>> result =
      protocol::Array<protocol::Profiler::ScriptCoverage>::create();
  for (size_t i = 0; i < coverage.ScriptCount(); i++) {
    v8::debug::Coverage::ScriptData script_data = coverage.GetScriptData(i);
    v8::Local<v8::debug::Script> script = script_data.GetScript();
    std::unique_ptr<protocol::Array<protocol::Profiler::FunctionCoverage>>
        functions =
            protocol::Array<protocol::Profiler::FunctionCoverage>::create();
    for (size_t j = 0; j < script_data.FunctionCount(); j++) {
      v8::debug::Coverage::FunctionData function_data =
          script_data.GetFunctionData(j);
      std::unique_ptr<protocol::Array<protocol::Profiler::CoverageRange>>
          ranges = protocol::Array<protocol::Profiler::CoverageRange>::create();
      // At this point we only have per-function coverage data, so there is
      // only one range per function.
      ranges->addItem(protocol::Profiler::CoverageRange::create()
                          .setStartOffset(function_data.StartOffset())
                          .setEndOffset(function_data.EndOffset())
                          .setCount(function_data.Count())
                          .build());
      functions->addItem(
          protocol::Profiler::FunctionCoverage::create()
              .setFunctionName(toProtocolString(
                  function_data.Name().FromMaybe(v8::Local<v8::String>())))
              .setRanges(std::move(ranges))
              .build());
    }
    String16 url;
    v8::Local<v8::String> name;
    if (script->Name().ToLocal(&name) || script->SourceURL().ToLocal(&name)) {
      url = toProtocolString(name);
    }
    result->addItem(protocol::Profiler::ScriptCoverage::create()
                        .setScriptId(String16::fromInteger(script->Id()))
                        .setUrl(url)
                        .setFunctions(std::move(functions))
                        .build());
  }
  *out_result = std::move(result);
  return Response::OK();
}
}  // anonymous namespace

Response V8ProfilerAgentImpl::takePreciseCoverage(
    std::unique_ptr<protocol::Array<protocol::Profiler::ScriptCoverage>>*
        out_result) {
  if (!m_state->booleanProperty(ProfilerAgentState::preciseCoverageStarted,
                                false)) {
    return Response::Error("Precise coverage has not been started.");
  }
  v8::HandleScope handle_scope(m_isolate);
  v8::debug::Coverage coverage = v8::debug::Coverage::CollectPrecise(m_isolate);
  return coverageToProtocol(m_isolate, coverage, out_result);
}

Response V8ProfilerAgentImpl::getBestEffortCoverage(
    std::unique_ptr<protocol::Array<protocol::Profiler::ScriptCoverage>>*
        out_result) {
  v8::HandleScope handle_scope(m_isolate);
  v8::debug::Coverage coverage =
      v8::debug::Coverage::CollectBestEffort(m_isolate);
  return coverageToProtocol(m_isolate, coverage, out_result);
}

String16 V8ProfilerAgentImpl::nextProfileId() {
  return String16::fromInteger(
      v8::base::NoBarrier_AtomicIncrement(&s_lastProfileId, 1));
}

void V8ProfilerAgentImpl::startProfiling(const String16& title) {
  v8::HandleScope handleScope(m_isolate);
  if (!m_startedProfilesCount) {
    DCHECK(!m_profiler);
    m_profiler = v8::CpuProfiler::New(m_isolate);
    m_profiler->SetIdle(m_idle);
    int interval =
        m_state->integerProperty(ProfilerAgentState::samplingInterval, 0);
    if (interval) m_profiler->SetSamplingInterval(interval);
  }
  ++m_startedProfilesCount;
  m_profiler->StartProfiling(toV8String(m_isolate, title), true);
}

std::unique_ptr<protocol::Profiler::Profile> V8ProfilerAgentImpl::stopProfiling(
    const String16& title, bool serialize) {
  v8::HandleScope handleScope(m_isolate);
  v8::CpuProfile* profile =
      m_profiler->StopProfiling(toV8String(m_isolate, title));
  std::unique_ptr<protocol::Profiler::Profile> result;
  if (profile) {
    if (serialize) result = createCPUProfile(m_isolate, profile);
    profile->Delete();
  }
  --m_startedProfilesCount;
  if (!m_startedProfilesCount) {
    m_profiler->Dispose();
    m_profiler = nullptr;
  }
  return result;
}

bool V8ProfilerAgentImpl::idleStarted() {
  m_idle = true;
  if (m_profiler) m_profiler->SetIdle(m_idle);
  return m_profiler;
}

bool V8ProfilerAgentImpl::idleFinished() {
  m_idle = false;
  if (m_profiler) m_profiler->SetIdle(m_idle);
  return m_profiler;
}

}  // namespace v8_inspector
