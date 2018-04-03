// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/inspector/v8-stack-trace-impl.h"

#include <algorithm>

#include "src/inspector/v8-debugger.h"
#include "src/inspector/wasm-translation.h"

namespace v8_inspector {

namespace {

static const v8::StackTrace::StackTraceOptions stackTraceOptions =
    static_cast<v8::StackTrace::StackTraceOptions>(
        v8::StackTrace::kDetailed |
        v8::StackTrace::kExposeFramesAcrossSecurityOrigins);

std::vector<std::shared_ptr<StackFrame>> toFramesVector(
    V8Debugger* debugger, v8::Local<v8::StackTrace> v8StackTrace,
    int maxStackSize) {
  DCHECK(debugger->isolate()->InContext());
  int frameCount = std::min(v8StackTrace->GetFrameCount(), maxStackSize);
  std::vector<std::shared_ptr<StackFrame>> frames;
  for (int i = 0; i < frameCount; ++i) {
    frames.push_back(debugger->symbolize(v8StackTrace->GetFrame(i)));
  }
  return frames;
}

void calculateAsyncChain(V8Debugger* debugger, int contextGroupId,
                         std::shared_ptr<AsyncStackTrace>* asyncParent,
                         V8StackTraceId* externalParent, int* maxAsyncDepth) {
  *asyncParent = debugger->currentAsyncParent();
  *externalParent = debugger->currentExternalParent();
  DCHECK(externalParent->IsInvalid() || !*asyncParent);
  if (maxAsyncDepth) *maxAsyncDepth = debugger->maxAsyncCallChainDepth();

  // Do not accidentally append async call chain from another group. This should
  // not happen if we have proper instrumentation, but let's double-check to be
  // safe.
  if (contextGroupId && *asyncParent &&
      (*asyncParent)->externalParent().IsInvalid() &&
      (*asyncParent)->contextGroupId() != contextGroupId) {
    asyncParent->reset();
    *externalParent = V8StackTraceId();
    if (maxAsyncDepth) *maxAsyncDepth = 0;
    return;
  }

  // Only the top stack in the chain may be empty, so ensure that second stack
  // is non-empty (it's the top of appended chain).
  if (*asyncParent && (*asyncParent)->isEmpty()) {
    *asyncParent = (*asyncParent)->parent().lock();
  }
}

std::unique_ptr<protocol::Runtime::StackTrace> buildInspectorObjectCommon(
    V8Debugger* debugger,
    const std::vector<std::shared_ptr<StackFrame>>& frames,
    const String16& description,
    const std::shared_ptr<AsyncStackTrace>& asyncParent,
    const V8StackTraceId& externalParent, int maxAsyncDepth) {
  if (asyncParent && frames.empty() &&
      description == asyncParent->description()) {
    return asyncParent->buildInspectorObject(debugger, maxAsyncDepth);
  }

  std::unique_ptr<protocol::Array<protocol::Runtime::CallFrame>>
      inspectorFrames = protocol::Array<protocol::Runtime::CallFrame>::create();
  for (size_t i = 0; i < frames.size(); i++) {
    inspectorFrames->addItem(frames[i]->buildInspectorObject());
  }
  std::unique_ptr<protocol::Runtime::StackTrace> stackTrace =
      protocol::Runtime::StackTrace::create()
          .setCallFrames(std::move(inspectorFrames))
          .build();
  if (!description.isEmpty()) stackTrace->setDescription(description);
  if (asyncParent) {
    if (maxAsyncDepth > 0) {
      stackTrace->setParent(
          asyncParent->buildInspectorObject(debugger, maxAsyncDepth - 1));
    } else if (debugger) {
      stackTrace->setParentId(
          protocol::Runtime::StackTraceId::create()
              .setId(stackTraceIdToString(
                  AsyncStackTrace::store(debugger, asyncParent)))
              .build());
    }
  }
  if (!externalParent.IsInvalid()) {
    stackTrace->setParentId(
        protocol::Runtime::StackTraceId::create()
            .setId(stackTraceIdToString(externalParent.id))
            .setDebuggerId(debuggerIdToString(externalParent.debugger_id))
            .build());
  }
  return stackTrace;
}

}  //  namespace

V8StackTraceId::V8StackTraceId() : id(0), debugger_id(std::make_pair(0, 0)) {}

V8StackTraceId::V8StackTraceId(uintptr_t id,
                               const std::pair<int64_t, int64_t> debugger_id)
    : id(id), debugger_id(debugger_id) {}

bool V8StackTraceId::IsInvalid() const { return !id; }

StackFrame::StackFrame(v8::Local<v8::StackFrame> v8Frame)
    : m_functionName(toProtocolString(v8Frame->GetFunctionName())),
      m_scriptId(String16::fromInteger(v8Frame->GetScriptId())),
      m_sourceURL(toProtocolString(v8Frame->GetScriptNameOrSourceURL())),
      m_lineNumber(v8Frame->GetLineNumber() - 1),
      m_columnNumber(v8Frame->GetColumn() - 1) {
  DCHECK_NE(v8::Message::kNoLineNumberInfo, m_lineNumber + 1);
  DCHECK_NE(v8::Message::kNoColumnInfo, m_columnNumber + 1);
}

void StackFrame::translate(WasmTranslation* wasmTranslation) {
  wasmTranslation->TranslateWasmScriptLocationToProtocolLocation(
      &m_scriptId, &m_lineNumber, &m_columnNumber);
}

const String16& StackFrame::functionName() const { return m_functionName; }

const String16& StackFrame::scriptId() const { return m_scriptId; }

const String16& StackFrame::sourceURL() const { return m_sourceURL; }

int StackFrame::lineNumber() const { return m_lineNumber; }

int StackFrame::columnNumber() const { return m_columnNumber; }

std::unique_ptr<protocol::Runtime::CallFrame> StackFrame::buildInspectorObject()
    const {
  return protocol::Runtime::CallFrame::create()
      .setFunctionName(m_functionName)
      .setScriptId(m_scriptId)
      .setUrl(m_sourceURL)
      .setLineNumber(m_lineNumber)
      .setColumnNumber(m_columnNumber)
      .build();
}

bool StackFrame::isEqual(StackFrame* frame) const {
  return m_scriptId == frame->m_scriptId &&
         m_lineNumber == frame->m_lineNumber &&
         m_columnNumber == frame->m_columnNumber;
}

// static
void V8StackTraceImpl::setCaptureStackTraceForUncaughtExceptions(
    v8::Isolate* isolate, bool capture) {
  isolate->SetCaptureStackTraceForUncaughtExceptions(
      capture, V8StackTraceImpl::maxCallStackSizeToCapture);
}

// static
std::unique_ptr<V8StackTraceImpl> V8StackTraceImpl::create(
    V8Debugger* debugger, int contextGroupId,
    v8::Local<v8::StackTrace> v8StackTrace, int maxStackSize) {
  DCHECK(debugger);

  v8::Isolate* isolate = debugger->isolate();
  v8::HandleScope scope(isolate);

  std::vector<std::shared_ptr<StackFrame>> frames;
  if (!v8StackTrace.IsEmpty() && v8StackTrace->GetFrameCount()) {
    frames = toFramesVector(debugger, v8StackTrace, maxStackSize);
  }

  int maxAsyncDepth = 0;
  std::shared_ptr<AsyncStackTrace> asyncParent;
  V8StackTraceId externalParent;
  calculateAsyncChain(debugger, contextGroupId, &asyncParent, &externalParent,
                      &maxAsyncDepth);
  if (frames.empty() && !asyncParent && externalParent.IsInvalid())
    return nullptr;
  return std::unique_ptr<V8StackTraceImpl>(new V8StackTraceImpl(
      std::move(frames), maxAsyncDepth, asyncParent, externalParent));
}

// static
std::unique_ptr<V8StackTraceImpl> V8StackTraceImpl::capture(
    V8Debugger* debugger, int contextGroupId, int maxStackSize) {
  DCHECK(debugger);
  v8::Isolate* isolate = debugger->isolate();
  v8::HandleScope handleScope(isolate);
  v8::Local<v8::StackTrace> v8StackTrace;
  if (isolate->InContext()) {
    v8StackTrace = v8::StackTrace::CurrentStackTrace(isolate, maxStackSize,
                                                     stackTraceOptions);
  }
  return V8StackTraceImpl::create(debugger, contextGroupId, v8StackTrace,
                                  maxStackSize);
}

V8StackTraceImpl::V8StackTraceImpl(
    std::vector<std::shared_ptr<StackFrame>> frames, int maxAsyncDepth,
    std::shared_ptr<AsyncStackTrace> asyncParent,
    const V8StackTraceId& externalParent)
    : m_frames(std::move(frames)),
      m_maxAsyncDepth(maxAsyncDepth),
      m_asyncParent(asyncParent),
      m_externalParent(externalParent) {}

V8StackTraceImpl::~V8StackTraceImpl() {}

std::unique_ptr<V8StackTrace> V8StackTraceImpl::clone() {
  return std::unique_ptr<V8StackTrace>(new V8StackTraceImpl(
      m_frames, 0, std::shared_ptr<AsyncStackTrace>(), V8StackTraceId()));
}

bool V8StackTraceImpl::isEmpty() const { return m_frames.empty(); }

StringView V8StackTraceImpl::topSourceURL() const {
  return toStringView(m_frames[0]->sourceURL());
}

int V8StackTraceImpl::topLineNumber() const {
  return m_frames[0]->lineNumber() + 1;
}

int V8StackTraceImpl::topColumnNumber() const {
  return m_frames[0]->columnNumber() + 1;
}

StringView V8StackTraceImpl::topScriptId() const {
  return toStringView(m_frames[0]->scriptId());
}

StringView V8StackTraceImpl::topFunctionName() const {
  return toStringView(m_frames[0]->functionName());
}

std::unique_ptr<protocol::Runtime::StackTrace>
V8StackTraceImpl::buildInspectorObjectImpl(V8Debugger* debugger) const {
  return buildInspectorObjectCommon(debugger, m_frames, String16(),
                                    m_asyncParent.lock(), m_externalParent,
                                    m_maxAsyncDepth);
}

std::unique_ptr<protocol::Runtime::API::StackTrace>
V8StackTraceImpl::buildInspectorObject() const {
  return buildInspectorObjectImpl(nullptr);
}

std::unique_ptr<StringBuffer> V8StackTraceImpl::toString() const {
  String16Builder stackTrace;
  for (size_t i = 0; i < m_frames.size(); ++i) {
    const StackFrame& frame = *m_frames[i];
    stackTrace.append("\n    at " + (frame.functionName().length()
                                         ? frame.functionName()
                                         : "(anonymous function)"));
    stackTrace.append(" (");
    stackTrace.append(frame.sourceURL());
    stackTrace.append(':');
    stackTrace.append(String16::fromInteger(frame.lineNumber() + 1));
    stackTrace.append(':');
    stackTrace.append(String16::fromInteger(frame.columnNumber() + 1));
    stackTrace.append(')');
  }
  String16 string = stackTrace.toString();
  return StringBufferImpl::adopt(string);
}

bool V8StackTraceImpl::isEqualIgnoringTopFrame(
    V8StackTraceImpl* stackTrace) const {
  StackFrameIterator current(this);
  StackFrameIterator target(stackTrace);

  current.next();
  target.next();
  while (!current.done() && !target.done()) {
    if (!current.frame()->isEqual(target.frame())) {
      return false;
    }
    current.next();
    target.next();
  }
  return current.done() == target.done();
}

V8StackTraceImpl::StackFrameIterator::StackFrameIterator(
    const V8StackTraceImpl* stackTrace)
    : m_currentIt(stackTrace->m_frames.begin()),
      m_currentEnd(stackTrace->m_frames.end()),
      m_parent(stackTrace->m_asyncParent.lock().get()) {}

void V8StackTraceImpl::StackFrameIterator::next() {
  if (m_currentIt == m_currentEnd) return;
  ++m_currentIt;
  while (m_currentIt == m_currentEnd && m_parent) {
    const std::vector<std::shared_ptr<StackFrame>>& frames = m_parent->frames();
    m_currentIt = frames.begin();
    if (m_parent->description() == "async function") ++m_currentIt;
    m_currentEnd = frames.end();
    m_parent = m_parent->parent().lock().get();
  }
}

bool V8StackTraceImpl::StackFrameIterator::done() {
  return m_currentIt == m_currentEnd;
}

StackFrame* V8StackTraceImpl::StackFrameIterator::frame() {
  return m_currentIt->get();
}

// static
std::shared_ptr<AsyncStackTrace> AsyncStackTrace::capture(
    V8Debugger* debugger, int contextGroupId, const String16& description,
    int maxStackSize) {
  DCHECK(debugger);

  v8::Isolate* isolate = debugger->isolate();
  v8::HandleScope handleScope(isolate);

  std::vector<std::shared_ptr<StackFrame>> frames;
  if (isolate->InContext()) {
    v8::Local<v8::StackTrace> v8StackTrace = v8::StackTrace::CurrentStackTrace(
        isolate, maxStackSize, stackTraceOptions);
    frames = toFramesVector(debugger, v8StackTrace, maxStackSize);
  }

  std::shared_ptr<AsyncStackTrace> asyncParent;
  V8StackTraceId externalParent;
  calculateAsyncChain(debugger, contextGroupId, &asyncParent, &externalParent,
                      nullptr);

  if (frames.empty() && !asyncParent && externalParent.IsInvalid())
    return nullptr;

  // When async call chain is empty but doesn't contain useful schedule stack
  // but doesn't synchronous we can merge them together. e.g. Promise
  // ThenableJob.
  if (asyncParent && frames.empty() &&
      (asyncParent->m_description == description || description.isEmpty())) {
    return asyncParent;
  }

  DCHECK(contextGroupId || asyncParent || !externalParent.IsInvalid());
  if (!contextGroupId && asyncParent) {
    contextGroupId = asyncParent->m_contextGroupId;
  }

  return std::shared_ptr<AsyncStackTrace>(
      new AsyncStackTrace(contextGroupId, description, std::move(frames),
                          asyncParent, externalParent));
}

AsyncStackTrace::AsyncStackTrace(
    int contextGroupId, const String16& description,
    std::vector<std::shared_ptr<StackFrame>> frames,
    std::shared_ptr<AsyncStackTrace> asyncParent,
    const V8StackTraceId& externalParent)
    : m_contextGroupId(contextGroupId),
      m_id(0),
      m_description(description),
      m_frames(std::move(frames)),
      m_asyncParent(asyncParent),
      m_externalParent(externalParent) {
  DCHECK(m_contextGroupId || (!externalParent.IsInvalid() && m_frames.empty()));
}

std::unique_ptr<protocol::Runtime::StackTrace>
AsyncStackTrace::buildInspectorObject(V8Debugger* debugger,
                                      int maxAsyncDepth) const {
  return buildInspectorObjectCommon(debugger, m_frames, m_description,
                                    m_asyncParent.lock(), m_externalParent,
                                    maxAsyncDepth);
}

int AsyncStackTrace::contextGroupId() const { return m_contextGroupId; }

uintptr_t AsyncStackTrace::store(V8Debugger* debugger,
                                 std::shared_ptr<AsyncStackTrace> stack) {
  if (stack->m_id) return stack->m_id;
  stack->m_id = debugger->storeStackTrace(stack);
  return stack->m_id;
}

const String16& AsyncStackTrace::description() const { return m_description; }

std::weak_ptr<AsyncStackTrace> AsyncStackTrace::parent() const {
  return m_asyncParent;
}

bool AsyncStackTrace::isEmpty() const { return m_frames.empty(); }

}  // namespace v8_inspector
