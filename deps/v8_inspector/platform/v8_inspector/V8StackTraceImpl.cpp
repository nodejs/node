// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/v8_inspector/V8StackTraceImpl.h"

#include "platform/inspector_protocol/String16.h"
#include "platform/v8_inspector/V8DebuggerAgentImpl.h"
#include "platform/v8_inspector/V8DebuggerImpl.h"
#include "platform/v8_inspector/V8StringUtil.h"
#include "wtf/PtrUtil.h"

#include <v8-debug.h>
#include <v8-profiler.h>
#include <v8-version.h>

namespace blink {

namespace {

V8StackTraceImpl::Frame toFrame(v8::Local<v8::StackFrame> frame)
{
    String16 scriptId = String16::number(frame->GetScriptId());
    String16 sourceName;
    v8::Local<v8::String> sourceNameValue(frame->GetScriptNameOrSourceURL());
    if (!sourceNameValue.IsEmpty())
        sourceName = toProtocolString(sourceNameValue);

    String16 functionName;
    v8::Local<v8::String> functionNameValue(frame->GetFunctionName());
    if (!functionNameValue.IsEmpty())
        functionName = toProtocolString(functionNameValue);

    int sourceLineNumber = frame->GetLineNumber();
    int sourceColumn = frame->GetColumn();
    return V8StackTraceImpl::Frame(functionName, scriptId, sourceName, sourceLineNumber, sourceColumn);
}

void toFramesVector(v8::Local<v8::StackTrace> stackTrace, protocol::Vector<V8StackTraceImpl::Frame>& frames, size_t maxStackSize, v8::Isolate* isolate)
{
    DCHECK(isolate->InContext());
    int frameCount = stackTrace->GetFrameCount();
    if (frameCount > static_cast<int>(maxStackSize))
        frameCount = maxStackSize;
    for (int i = 0; i < frameCount; i++) {
        v8::Local<v8::StackFrame> stackFrame = stackTrace->GetFrame(i);
        frames.append(toFrame(stackFrame));
    }
}

} //  namespace

V8StackTraceImpl::Frame::Frame()
    : m_functionName("undefined")
    , m_scriptId("")
    , m_scriptName("undefined")
    , m_lineNumber(0)
    , m_columnNumber(0)
{
}

V8StackTraceImpl::Frame::Frame(const String16& functionName, const String16& scriptId, const String16& scriptName, int lineNumber, int column)
    : m_functionName(functionName)
    , m_scriptId(scriptId)
    , m_scriptName(scriptName)
    , m_lineNumber(lineNumber)
    , m_columnNumber(column)
{
}

V8StackTraceImpl::Frame::~Frame()
{
}

// buildInspectorObject() and ScriptCallStack's toTracedValue() should set the same fields.
// If either of them is modified, the other should be also modified.
std::unique_ptr<protocol::Runtime::CallFrame> V8StackTraceImpl::Frame::buildInspectorObject() const
{
    return protocol::Runtime::CallFrame::create()
        .setFunctionName(m_functionName)
        .setScriptId(m_scriptId)
        .setUrl(m_scriptName)
        .setLineNumber(m_lineNumber)
        .setColumnNumber(m_columnNumber)
        .build();
}

std::unique_ptr<V8StackTraceImpl> V8StackTraceImpl::create(V8DebuggerAgentImpl* agent, v8::Local<v8::StackTrace> stackTrace, size_t maxStackSize, const String16& description)
{
    v8::Isolate* isolate = v8::Isolate::GetCurrent();
    v8::HandleScope scope(isolate);
    protocol::Vector<V8StackTraceImpl::Frame> frames;
    if (!stackTrace.IsEmpty())
        toFramesVector(stackTrace, frames, maxStackSize, isolate);

    int maxAsyncCallChainDepth = 1;
    V8StackTraceImpl* asyncCallChain = nullptr;
    if (agent && maxStackSize > 1) {
        asyncCallChain = agent->currentAsyncCallChain();
        maxAsyncCallChainDepth = agent->maxAsyncCallChainDepth();
    }

    // Only the top stack in the chain may be empty, so ensure that second stack is non-empty (it's the top of appended chain).
    if (asyncCallChain && asyncCallChain->isEmpty())
        asyncCallChain = asyncCallChain->m_parent.get();

    if (stackTrace.IsEmpty() && !asyncCallChain)
        return nullptr;

    std::unique_ptr<V8StackTraceImpl> result(new V8StackTraceImpl(description, frames, asyncCallChain ? asyncCallChain->clone() : nullptr));

    // Crop to not exceed maxAsyncCallChainDepth.
    V8StackTraceImpl* deepest = result.get();
    while (deepest && maxAsyncCallChainDepth) {
        deepest = deepest->m_parent.get();
        maxAsyncCallChainDepth--;
    }
    if (deepest)
        deepest->m_parent.reset();

    return result;
}

std::unique_ptr<V8StackTraceImpl> V8StackTraceImpl::capture(V8DebuggerAgentImpl* agent, size_t maxStackSize, const String16& description)
{
    v8::Isolate* isolate = v8::Isolate::GetCurrent();
    v8::HandleScope handleScope(isolate);
    v8::Local<v8::StackTrace> stackTrace;
    if (isolate->InContext()) {
#if V8_MAJOR_VERSION >= 5
        isolate->GetCpuProfiler()->CollectSample();
#endif
        stackTrace = v8::StackTrace::CurrentStackTrace(isolate, maxStackSize, stackTraceOptions);
    }
    return V8StackTraceImpl::create(agent, stackTrace, maxStackSize, description);
}

std::unique_ptr<V8StackTraceImpl> V8StackTraceImpl::clone()
{
    protocol::Vector<Frame> framesCopy(m_frames);
    return wrapUnique(new V8StackTraceImpl(m_description, framesCopy, m_parent ? m_parent->clone() : nullptr));
}

V8StackTraceImpl::V8StackTraceImpl(const String16& description, protocol::Vector<Frame>& frames, std::unique_ptr<V8StackTraceImpl> parent)
    : m_description(description)
    , m_parent(std::move(parent))
{
    m_frames.swap(frames);
}

V8StackTraceImpl::~V8StackTraceImpl()
{
}

String16 V8StackTraceImpl::topSourceURL() const
{
    DCHECK(m_frames.size());
    return m_frames[0].m_scriptName;
}

int V8StackTraceImpl::topLineNumber() const
{
    DCHECK(m_frames.size());
    return m_frames[0].m_lineNumber;
}

int V8StackTraceImpl::topColumnNumber() const
{
    DCHECK(m_frames.size());
    return m_frames[0].m_columnNumber;
}

String16 V8StackTraceImpl::topFunctionName() const
{
    DCHECK(m_frames.size());
    return m_frames[0].m_functionName;
}

String16 V8StackTraceImpl::topScriptId() const
{
    DCHECK(m_frames.size());
    return m_frames[0].m_scriptId;
}

std::unique_ptr<protocol::Runtime::StackTrace> V8StackTraceImpl::buildInspectorObject() const
{
    std::unique_ptr<protocol::Array<protocol::Runtime::CallFrame>> frames = protocol::Array<protocol::Runtime::CallFrame>::create();
    for (size_t i = 0; i < m_frames.size(); i++)
        frames->addItem(m_frames.at(i).buildInspectorObject());

    std::unique_ptr<protocol::Runtime::StackTrace> stackTrace = protocol::Runtime::StackTrace::create()
        .setCallFrames(std::move(frames)).build();
    if (!m_description.isEmpty())
        stackTrace->setDescription(m_description);
    if (m_parent)
        stackTrace->setParent(m_parent->buildInspectorObject());
    return stackTrace;
}

std::unique_ptr<protocol::Runtime::StackTrace> V8StackTraceImpl::buildInspectorObjectForTail(V8DebuggerAgentImpl* agent) const
{
    v8::HandleScope handleScope(v8::Isolate::GetCurrent());
    // Next call collapses possible empty stack and ensures maxAsyncCallChainDepth.
    std::unique_ptr<V8StackTraceImpl> fullChain = V8StackTraceImpl::create(agent, v8::Local<v8::StackTrace>(), V8StackTrace::maxCallStackSizeToCapture);
    if (!fullChain || !fullChain->m_parent)
        return nullptr;
    return fullChain->m_parent->buildInspectorObject();
}

String16 V8StackTraceImpl::toString() const
{
    String16Builder stackTrace;
    for (size_t i = 0; i < m_frames.size(); ++i) {
        const Frame& frame = m_frames[i];
        stackTrace.append("\n    at " + (frame.functionName().length() ? frame.functionName() : "(anonymous function)"));
        stackTrace.append(" (");
        stackTrace.append(frame.sourceURL());
        stackTrace.append(':');
        stackTrace.appendNumber(frame.lineNumber());
        stackTrace.append(':');
        stackTrace.appendNumber(frame.columnNumber());
        stackTrace.append(')');
    }
    return stackTrace.toString();
}

} // namespace blink
