// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/inspector/v8-profiler-agent-impl.h"

#include <vector>

#include "src/base/atomicops.h"
#include "src/debug/debug-interface.h"
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
static const char preciseCoverageDetailed[] = "preciseCoverageDetailed";
static const char typeProfileStarted[] = "typeProfileStarted";
}

namespace {

String16 resourceNameToUrl(V8InspectorImpl* inspector,
                           v8::Local<v8::String> v8Name) {
  String16 name = toProtocolString(inspector->isolate(), v8Name);
  if (!inspector) return name;
  std::unique_ptr<StringBuffer> url =
      inspector->client()->resourceNameToUrl(toStringView(name));
  return url ? toString16(url->string()) : name;
}

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
    V8InspectorImpl* inspector, const v8::CpuProfileNode* node) {
  v8::Isolate* isolate = inspector->isolate();
  v8::HandleScope handleScope(isolate);
  auto callFrame =
      protocol::Runtime::CallFrame::create()
          .setFunctionName(toProtocolString(isolate, node->GetFunctionName()))
          .setScriptId(String16::fromInteger(node->GetScriptId()))
          .setUrl(resourceNameToUrl(inspector, node->GetScriptResourceName()))
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

void flattenNodesTree(V8InspectorImpl* inspector,
                      const v8::CpuProfileNode* node,
                      protocol::Array<protocol::Profiler::ProfileNode>* list) {
  list->addItem(buildInspectorObjectFor(inspector, node));
  const int childrenCount = node->GetChildrenCount();
  for (int i = 0; i < childrenCount; i++)
    flattenNodesTree(inspector, node->GetChild(i), list);
}

std::unique_ptr<protocol::Profiler::Profile> createCPUProfile(
    V8InspectorImpl* inspector, v8::CpuProfile* v8profile) {
  auto nodes = protocol::Array<protocol::Profiler::ProfileNode>::create();
  flattenNodesTree(inspector, v8profile->GetTopDownRoot(), nodes.get());
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
    bool detailed = m_state->booleanProperty(
        ProfilerAgentState::preciseCoverageDetailed, false);
    startPreciseCoverage(Maybe<bool>(callCount), Maybe<bool>(detailed));
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

Response V8ProfilerAgentImpl::startPreciseCoverage(Maybe<bool> callCount,
                                                   Maybe<bool> detailed) {
  if (!m_enabled) return Response::Error("Profiler is not enabled");
  bool callCountValue = callCount.fromMaybe(false);
  bool detailedValue = detailed.fromMaybe(false);
  m_state->setBoolean(ProfilerAgentState::preciseCoverageStarted, true);
  m_state->setBoolean(ProfilerAgentState::preciseCoverageCallCount,
                      callCountValue);
  m_state->setBoolean(ProfilerAgentState::preciseCoverageDetailed,
                      detailedValue);
  // BlockCount is a superset of PreciseCount. It includes block-granularity
  // coverage data if it exists (at the time of writing, that's the case for
  // each function recompiled after the BlockCount mode has been set); and
  // function-granularity coverage data otherwise.
  typedef v8::debug::Coverage C;
  typedef v8::debug::CoverageMode Mode;
  Mode mode = callCountValue
                  ? (detailedValue ? Mode::kBlockCount : Mode::kPreciseCount)
                  : (detailedValue ? Mode::kBlockBinary : Mode::kPreciseBinary);
  C::SelectMode(m_isolate, mode);
  return Response::OK();
}

Response V8ProfilerAgentImpl::stopPreciseCoverage() {
  if (!m_enabled) return Response::Error("Profiler is not enabled");
  m_state->setBoolean(ProfilerAgentState::preciseCoverageStarted, false);
  m_state->setBoolean(ProfilerAgentState::preciseCoverageCallCount, false);
  m_state->setBoolean(ProfilerAgentState::preciseCoverageDetailed, false);
  v8::debug::Coverage::SelectMode(m_isolate,
                                  v8::debug::CoverageMode::kBestEffort);
  return Response::OK();
}

namespace {
std::unique_ptr<protocol::Profiler::CoverageRange> createCoverageRange(
    int start, int end, int count) {
  return protocol::Profiler::CoverageRange::create()
      .setStartOffset(start)
      .setEndOffset(end)
      .setCount(count)
      .build();
}

Response coverageToProtocol(
    V8InspectorImpl* inspector, const v8::debug::Coverage& coverage,
    std::unique_ptr<protocol::Array<protocol::Profiler::ScriptCoverage>>*
        out_result) {
  std::unique_ptr<protocol::Array<protocol::Profiler::ScriptCoverage>> result =
      protocol::Array<protocol::Profiler::ScriptCoverage>::create();
  v8::Isolate* isolate = inspector->isolate();
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

      // Add function range.
      ranges->addItem(createCoverageRange(function_data.StartOffset(),
                                          function_data.EndOffset(),
                                          function_data.Count()));

      // Process inner blocks.
      for (size_t k = 0; k < function_data.BlockCount(); k++) {
        v8::debug::Coverage::BlockData block_data =
            function_data.GetBlockData(k);
        ranges->addItem(createCoverageRange(block_data.StartOffset(),
                                            block_data.EndOffset(),
                                            block_data.Count()));
      }

      functions->addItem(
          protocol::Profiler::FunctionCoverage::create()
              .setFunctionName(toProtocolString(
                  isolate,
                  function_data.Name().FromMaybe(v8::Local<v8::String>())))
              .setRanges(std::move(ranges))
              .setIsBlockCoverage(function_data.HasBlockCoverage())
              .build());
    }
    String16 url;
    v8::Local<v8::String> name;
    if (script->SourceURL().ToLocal(&name) && name->Length()) {
      url = toProtocolString(isolate, name);
    } else if (script->Name().ToLocal(&name) && name->Length()) {
      url = resourceNameToUrl(inspector, name);
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
  return coverageToProtocol(m_session->inspector(), coverage, out_result);
}

Response V8ProfilerAgentImpl::getBestEffortCoverage(
    std::unique_ptr<protocol::Array<protocol::Profiler::ScriptCoverage>>*
        out_result) {
  v8::HandleScope handle_scope(m_isolate);
  v8::debug::Coverage coverage =
      v8::debug::Coverage::CollectBestEffort(m_isolate);
  return coverageToProtocol(m_session->inspector(), coverage, out_result);
}

namespace {
std::unique_ptr<protocol::Array<protocol::Profiler::ScriptTypeProfile>>
typeProfileToProtocol(V8InspectorImpl* inspector,
                      const v8::debug::TypeProfile& type_profile) {
  std::unique_ptr<protocol::Array<protocol::Profiler::ScriptTypeProfile>>
      result = protocol::Array<protocol::Profiler::ScriptTypeProfile>::create();
  v8::Isolate* isolate = inspector->isolate();
  for (size_t i = 0; i < type_profile.ScriptCount(); i++) {
    v8::debug::TypeProfile::ScriptData script_data =
        type_profile.GetScriptData(i);
    v8::Local<v8::debug::Script> script = script_data.GetScript();
    std::unique_ptr<protocol::Array<protocol::Profiler::TypeProfileEntry>>
        entries =
            protocol::Array<protocol::Profiler::TypeProfileEntry>::create();

    for (const auto& entry : script_data.Entries()) {
      std::unique_ptr<protocol::Array<protocol::Profiler::TypeObject>> types =
          protocol::Array<protocol::Profiler::TypeObject>::create();
      for (const auto& type : entry.Types()) {
        types->addItem(
            protocol::Profiler::TypeObject::create()
                .setName(toProtocolString(
                    isolate, type.FromMaybe(v8::Local<v8::String>())))
                .build());
      }
      entries->addItem(protocol::Profiler::TypeProfileEntry::create()
                           .setOffset(entry.SourcePosition())
                           .setTypes(std::move(types))
                           .build());
    }
    String16 url;
    v8::Local<v8::String> name;
    if (script->SourceURL().ToLocal(&name) && name->Length()) {
      url = toProtocolString(isolate, name);
    } else if (script->Name().ToLocal(&name) && name->Length()) {
      url = resourceNameToUrl(inspector, name);
    }
    result->addItem(protocol::Profiler::ScriptTypeProfile::create()
                        .setScriptId(String16::fromInteger(script->Id()))
                        .setUrl(url)
                        .setEntries(std::move(entries))
                        .build());
  }
  return result;
}
}  // anonymous namespace

Response V8ProfilerAgentImpl::startTypeProfile() {
  m_state->setBoolean(ProfilerAgentState::typeProfileStarted, true);
  v8::debug::TypeProfile::SelectMode(m_isolate,
                                     v8::debug::TypeProfileMode::kCollect);
  return Response::OK();
}

Response V8ProfilerAgentImpl::stopTypeProfile() {
  m_state->setBoolean(ProfilerAgentState::typeProfileStarted, false);
  v8::debug::TypeProfile::SelectMode(m_isolate,
                                     v8::debug::TypeProfileMode::kNone);
  return Response::OK();
}

Response V8ProfilerAgentImpl::takeTypeProfile(
    std::unique_ptr<protocol::Array<protocol::Profiler::ScriptTypeProfile>>*
        out_result) {
  if (!m_state->booleanProperty(ProfilerAgentState::typeProfileStarted,
                                false)) {
    return Response::Error("Type profile has not been started.");
  }
  v8::HandleScope handle_scope(m_isolate);
  v8::debug::TypeProfile type_profile =
      v8::debug::TypeProfile::Collect(m_isolate);
  *out_result = typeProfileToProtocol(m_session->inspector(), type_profile);
  return Response::OK();
}

String16 V8ProfilerAgentImpl::nextProfileId() {
  return String16::fromInteger(
      v8::base::Relaxed_AtomicIncrement(&s_lastProfileId, 1));
}

void V8ProfilerAgentImpl::startProfiling(const String16& title) {
  v8::HandleScope handleScope(m_isolate);
  if (!m_startedProfilesCount) {
    DCHECK(!m_profiler);
    m_profiler = v8::CpuProfiler::New(m_isolate);
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
    if (serialize) result = createCPUProfile(m_session->inspector(), profile);
    profile->Delete();
  }
  --m_startedProfilesCount;
  if (!m_startedProfilesCount) {
    m_profiler->Dispose();
    m_profiler = nullptr;
  }
  return result;
}

}  // namespace v8_inspector
