// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/v8_inspector/V8ProfilerAgentImpl.h"

#include "platform/v8_inspector/Atomics.h"
#include "platform/v8_inspector/V8InspectorImpl.h"
#include "platform/v8_inspector/V8InspectorSessionImpl.h"
#include "platform/v8_inspector/V8StackTraceImpl.h"
#include "platform/v8_inspector/V8StringUtil.h"
#include <v8-profiler.h>

#include <vector>

#define ENSURE_V8_VERSION(major, minor) \
    (V8_MAJOR_VERSION * 1000 + V8_MINOR_VERSION >= (major) * 1000 + (minor))

namespace v8_inspector {

namespace ProfilerAgentState {
static const char samplingInterval[] = "samplingInterval";
static const char userInitiatedProfiling[] = "userInitiatedProfiling";
static const char profilerEnabled[] = "profilerEnabled";
}

namespace {

std::unique_ptr<protocol::Array<protocol::Profiler::PositionTickInfo>> buildInspectorObjectForPositionTicks(const v8::CpuProfileNode* node)
{
    unsigned lineCount = node->GetHitLineCount();
    if (!lineCount)
        return nullptr;
    auto array = protocol::Array<protocol::Profiler::PositionTickInfo>::create();
    std::vector<v8::CpuProfileNode::LineTick> entries(lineCount);
    if (node->GetLineTicks(&entries[0], lineCount)) {
        for (unsigned i = 0; i < lineCount; i++) {
            std::unique_ptr<protocol::Profiler::PositionTickInfo> line = protocol::Profiler::PositionTickInfo::create()
                .setLine(entries[i].line)
                .setTicks(entries[i].hit_count).build();
            array->addItem(std::move(line));
        }
    }
    return array;
}

std::unique_ptr<protocol::Profiler::CPUProfileNode> buildInspectorObjectFor(v8::Isolate* isolate, const v8::CpuProfileNode* node)
{
    v8::HandleScope handleScope(isolate);
    auto callFrame = protocol::Runtime::CallFrame::create()
        .setFunctionName(toProtocolString(node->GetFunctionName()))
        .setScriptId(String16::fromInteger(node->GetScriptId()))
        .setUrl(toProtocolString(node->GetScriptResourceName()))
        .setLineNumber(node->GetLineNumber() - 1)
        .setColumnNumber(node->GetColumnNumber() - 1)
        .build();
    auto result = protocol::Profiler::CPUProfileNode::create()
        .setCallFrame(std::move(callFrame))
        .setHitCount(node->GetHitCount())
        .setId(node->GetNodeId()).build();

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
    if (positionTicks)
        result->setPositionTicks(std::move(positionTicks));

    return result;
}

std::unique_ptr<protocol::Array<int>> buildInspectorObjectForSamples(v8::CpuProfile* v8profile)
{
    auto array = protocol::Array<int>::create();
    int count = v8profile->GetSamplesCount();
    for (int i = 0; i < count; i++)
        array->addItem(v8profile->GetSample(i)->GetNodeId());
    return array;
}

std::unique_ptr<protocol::Array<int>> buildInspectorObjectForTimestamps(v8::CpuProfile* v8profile)
{
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

void flattenNodesTree(v8::Isolate* isolate, const v8::CpuProfileNode* node, protocol::Array<protocol::Profiler::CPUProfileNode>* list)
{
    list->addItem(buildInspectorObjectFor(isolate, node));
    const int childrenCount = node->GetChildrenCount();
    for (int i = 0; i < childrenCount; i++)
        flattenNodesTree(isolate, node->GetChild(i), list);
}

std::unique_ptr<protocol::Profiler::CPUProfile> createCPUProfile(v8::Isolate* isolate, v8::CpuProfile* v8profile)
{
    auto nodes = protocol::Array<protocol::Profiler::CPUProfileNode>::create();
    flattenNodesTree(isolate, v8profile->GetTopDownRoot(), nodes.get());

    auto profile = protocol::Profiler::CPUProfile::create()
        .setNodes(std::move(nodes))
        .setStartTime(static_cast<double>(v8profile->GetStartTime()))
        .setEndTime(static_cast<double>(v8profile->GetEndTime())).build();
    profile->setSamples(buildInspectorObjectForSamples(v8profile));
    profile->setTimestampDeltas(buildInspectorObjectForTimestamps(v8profile));
    return profile;
}

std::unique_ptr<protocol::Debugger::Location> currentDebugLocation(V8InspectorImpl* inspector)
{
    std::unique_ptr<V8StackTrace> callStack = inspector->captureStackTrace(1);
    auto location = protocol::Debugger::Location::create()
        .setScriptId(callStack->topScriptId())
        .setLineNumber(callStack->topLineNumber()).build();
    location->setColumnNumber(callStack->topColumnNumber());
    return location;
}

volatile int s_lastProfileId = 0;

} // namespace

class V8ProfilerAgentImpl::ProfileDescriptor {
public:
    ProfileDescriptor(const String16& id, const String16& title)
        : m_id(id)
        , m_title(title) { }
    String16 m_id;
    String16 m_title;
};

V8ProfilerAgentImpl::V8ProfilerAgentImpl(V8InspectorSessionImpl* session, protocol::FrontendChannel* frontendChannel, protocol::DictionaryValue* state)
    : m_session(session)
    , m_isolate(m_session->inspector()->isolate())
    , m_profiler(nullptr)
    , m_state(state)
    , m_frontend(frontendChannel)
    , m_enabled(false)
    , m_recordingCPUProfile(false)
{
}

V8ProfilerAgentImpl::~V8ProfilerAgentImpl()
{
#if ENSURE_V8_VERSION(5, 4)
    if (m_profiler)
        m_profiler->Dispose();
#endif
}

void V8ProfilerAgentImpl::consoleProfile(const String16& title)
{
    if (!m_enabled)
        return;
    String16 id = nextProfileId();
    m_startedProfiles.push_back(ProfileDescriptor(id, title));
    startProfiling(id);
    m_frontend.consoleProfileStarted(id, currentDebugLocation(m_session->inspector()), title);
}

void V8ProfilerAgentImpl::consoleProfileEnd(const String16& title)
{
    if (!m_enabled)
        return;
    String16 id;
    String16 resolvedTitle;
    // Take last started profile if no title was passed.
    if (title.isEmpty()) {
        if (m_startedProfiles.empty())
            return;
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
        if (id.isEmpty())
            return;
    }
    std::unique_ptr<protocol::Profiler::CPUProfile> profile = stopProfiling(id, true);
    if (!profile)
        return;
    std::unique_ptr<protocol::Debugger::Location> location = currentDebugLocation(m_session->inspector());
    m_frontend.consoleProfileFinished(id, std::move(location), std::move(profile), resolvedTitle);
}

void V8ProfilerAgentImpl::enable(ErrorString*)
{
    if (m_enabled)
        return;
    m_enabled = true;
#if ENSURE_V8_VERSION(5, 4)
    DCHECK(!m_profiler);
    m_profiler = v8::CpuProfiler::New(m_isolate);
#endif
    m_state->setBoolean(ProfilerAgentState::profilerEnabled, true);
}

void V8ProfilerAgentImpl::disable(ErrorString* errorString)
{
    if (!m_enabled)
        return;
    for (size_t i = m_startedProfiles.size(); i > 0; --i)
        stopProfiling(m_startedProfiles[i - 1].m_id, false);
    m_startedProfiles.clear();
    stop(nullptr, nullptr);
#if ENSURE_V8_VERSION(5, 4)
    m_profiler->Dispose();
    m_profiler = nullptr;
#endif
    m_enabled = false;
    m_state->setBoolean(ProfilerAgentState::profilerEnabled, false);
}

void V8ProfilerAgentImpl::setSamplingInterval(ErrorString* error, int interval)
{
    if (m_recordingCPUProfile) {
        *error = "Cannot change sampling interval when profiling.";
        return;
    }
    m_state->setInteger(ProfilerAgentState::samplingInterval, interval);
    profiler()->SetSamplingInterval(interval);
}

void V8ProfilerAgentImpl::restore()
{
    DCHECK(!m_enabled);
    if (!m_state->booleanProperty(ProfilerAgentState::profilerEnabled, false))
        return;
    m_enabled = true;
#if ENSURE_V8_VERSION(5, 4)
    DCHECK(!m_profiler);
    m_profiler = v8::CpuProfiler::New(m_isolate);
#endif
    int interval = 0;
    m_state->getInteger(ProfilerAgentState::samplingInterval, &interval);
    if (interval)
        profiler()->SetSamplingInterval(interval);
    if (m_state->booleanProperty(ProfilerAgentState::userInitiatedProfiling, false)) {
        ErrorString error;
        start(&error);
    }
}

void V8ProfilerAgentImpl::start(ErrorString* error)
{
    if (m_recordingCPUProfile)
        return;
    if (!m_enabled) {
        *error = "Profiler is not enabled";
        return;
    }
    m_recordingCPUProfile = true;
    m_frontendInitiatedProfileId = nextProfileId();
    startProfiling(m_frontendInitiatedProfileId);
    m_state->setBoolean(ProfilerAgentState::userInitiatedProfiling, true);
}

void V8ProfilerAgentImpl::stop(ErrorString* errorString, std::unique_ptr<protocol::Profiler::CPUProfile>* profile)
{
    if (!m_recordingCPUProfile) {
        if (errorString)
            *errorString = "No recording profiles found";
        return;
    }
    m_recordingCPUProfile = false;
    std::unique_ptr<protocol::Profiler::CPUProfile> cpuProfile = stopProfiling(m_frontendInitiatedProfileId, !!profile);
    if (profile) {
        *profile = std::move(cpuProfile);
        if (!profile->get() && errorString)
            *errorString = "Profile is not found";
    }
    m_frontendInitiatedProfileId = String16();
    m_state->setBoolean(ProfilerAgentState::userInitiatedProfiling, false);
}

String16 V8ProfilerAgentImpl::nextProfileId()
{
    return String16::fromInteger(atomicIncrement(&s_lastProfileId));
}

void V8ProfilerAgentImpl::startProfiling(const String16& title)
{
    v8::HandleScope handleScope(m_isolate);
    profiler()->StartProfiling(toV8String(m_isolate, title), true);
}

std::unique_ptr<protocol::Profiler::CPUProfile> V8ProfilerAgentImpl::stopProfiling(const String16& title, bool serialize)
{
    v8::HandleScope handleScope(m_isolate);
    v8::CpuProfile* profile = profiler()->StopProfiling(toV8String(m_isolate, title));
    if (!profile)
        return nullptr;
    std::unique_ptr<protocol::Profiler::CPUProfile> result;
    if (serialize)
        result = createCPUProfile(m_isolate, profile);
    profile->Delete();
    return result;
}

bool V8ProfilerAgentImpl::isRecording() const
{
    return m_recordingCPUProfile || !m_startedProfiles.empty();
}

v8::CpuProfiler* V8ProfilerAgentImpl::profiler()
{
#if ENSURE_V8_VERSION(5, 4)
    return m_profiler;
#else
    return m_isolate->GetCpuProfiler();
#endif
}

} // namespace v8_inspector
