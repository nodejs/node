// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/inspector/v8-console.h"

#include <array>
#include <atomic>
#include <span>

#include "include/cppgc/allocation.h"
#include "include/v8-container.h"
#include "include/v8-context.h"
#include "include/v8-cppgc.h"
#include "include/v8-function.h"
#include "include/v8-inspector.h"
#include "include/v8-microtask-queue.h"
#include "include/v8-object.h"
#include "include/v8-profiler.h"
#include "include/v8-sandbox.h"
#include "src/base/lazy-instance.h"
#include "src/base/macros.h"
#include "src/base/platform/time.h"
#include "src/debug/debug-interface.h"
#include "src/inspector/injected-script.h"
#include "src/inspector/inspected-context.h"
#include "src/inspector/string-util.h"
#include "src/inspector/v8-console-message.h"
#include "src/inspector/v8-debugger-agent-impl.h"
#include "src/inspector/v8-inspector-impl.h"
#include "src/inspector/v8-inspector-session-impl.h"
#include "src/inspector/v8-profiler-agent-impl.h"
#include "src/inspector/v8-runtime-agent-impl.h"
#include "src/inspector/v8-stack-trace-impl.h"
#include "src/inspector/v8-value-utils.h"
#include "src/tracing/trace-event.h"
#include "src/tracing/trace-id.h"

namespace v8_inspector {

namespace {

String16 consoleContextToString(
    v8::Isolate* isolate, const v8::debug::ConsoleContext& consoleContext) {
  if (consoleContext.id() == 0) return String16();
  return toProtocolString(isolate, consoleContext.name()) + "#" +
         String16::fromInteger(consoleContext.id());
}

class ConsoleHelper {
 public:
  ConsoleHelper(const v8::debug::ConsoleCallArguments& info,
                const v8::debug::ConsoleContext& consoleContext,
                V8InspectorImpl* inspector)
      : m_info(info),
        m_consoleContext(consoleContext),
        m_inspector(inspector) {}

  ConsoleHelper(const ConsoleHelper&) = delete;
  ConsoleHelper& operator=(const ConsoleHelper&) = delete;

  v8::Isolate* isolate() const { return m_inspector->isolate(); }
  v8::Local<v8::Context> context() const {
    return isolate()->GetCurrentContext();
  }
  int contextId() const { return InspectedContext::contextId(context()); }
  int groupId() const { return m_inspector->contextGroupId(contextId()); }

  std::shared_ptr<InjectedScript> injectedScript(int sessionId) {
    m_inspectedContext = m_inspector->getContext(groupId(), contextId());
    if (!m_inspectedContext) return nullptr;
    return m_inspectedContext->getInjectedScript(sessionId);
  }

  V8InspectorSessionImpl* session(int sessionId) {
    return m_inspector->sessionById(groupId(), sessionId);
  }

  // Returns the console message storage for the current context group, or
  // nullptr if the current context is not (or no longer) associated with any
  // context group. The latter can happen for example when user JavaScript
  // (i.e. a `toString()` method on a label argument) causes the embedder to
  // destroy the current context. In that case we must not create a storage
  // for the (invalid) context group 0, since that would never be cleaned up.
  V8ConsoleMessageStorage* consoleMessageStorage() {
    int contextGroupId = groupId();
    if (!contextGroupId) return nullptr;
    return m_inspector->ensureConsoleMessageStorage(contextGroupId);
  }

  void reportCall(ConsoleAPIType type) {
    if (!m_info.Length()) return;
    v8::LocalVector<v8::Value> arguments(isolate());
    arguments.reserve(m_info.Length());
    for (int i = 0; i < m_info.Length(); ++i) arguments.push_back(m_info[i]);
    reportCall(type, arguments);
  }

  void reportCallWithDefaultArgument(ConsoleAPIType type,
                                     const String16& message) {
    v8::LocalVector<v8::Value> arguments(isolate());
    arguments.reserve(m_info.Length());
    for (int i = 0; i < m_info.Length(); ++i) arguments.push_back(m_info[i]);
    if (!m_info.Length()) arguments.push_back(toV8String(isolate(), message));
    reportCall(type, arguments);
  }

  void reportCallAndReplaceFirstArgument(ConsoleAPIType type,
                                         const String16& message) {
    v8::LocalVector<v8::Value> arguments(isolate());
    arguments.push_back(toV8String(isolate(), message));
    for (int i = 1; i < m_info.Length(); ++i) arguments.push_back(m_info[i]);
    reportCall(type, arguments);
  }

  void reportCallWithArgument(ConsoleAPIType type, const String16& message) {
    auto arguments =
        std::to_array<v8::Local<v8::Value>>({toV8String(isolate(), message)});
    reportCall(type, arguments);
  }

  void reportCall(ConsoleAPIType type,
                  std::span<const v8::Local<v8::Value>> arguments) {
    if (!groupId()) return;
    // Depending on the type of the console message, we capture only parts of
    // the stack trace, or no stack trace at all.
    std::unique_ptr<V8StackTraceImpl> stackTrace;
    switch (type) {
      case ConsoleAPIType::kTrace:
        // The purpose of `console.trace()` is to output a stack trace to the
        // developer tools console, therefore we should always strive to
        // capture a full stack trace, even before any debugger is attached.
        stackTrace = m_inspector->debugger()->captureStackTrace(true);
        break;

      case ConsoleAPIType::kTimeEnd:
        // The `console.time()` and `console.timeEnd()` APIs are meant for
        // performance investigations, and therefore it's important to reduce
        // the total overhead of these calls, but also make sure these APIs
        // have consistent performance overhead. In order to guarantee that,
        // we always capture only the top frame, otherwise the performance
        // characteristics of `console.timeEnd()` would differ based on the
        // current call depth, which would skew the results.
        //
        // See https://crbug.com/41433391 for more information.
        stackTrace = V8StackTraceImpl::capture(m_inspector->debugger(), 1);
        break;

      default:
        // All other APIs get a full stack trace only when the debugger is
        // attached, otherwise record only the top frame.
        stackTrace = m_inspector->debugger()->captureStackTrace(false);
        break;
    }
    std::unique_ptr<V8ConsoleMessage> message =
        V8ConsoleMessage::createForConsoleAPI(
            context(), contextId(), groupId(), m_inspector,
            m_inspector->client()->currentTimeMS(), type, arguments,
            consoleContextToString(isolate(), m_consoleContext),
            std::move(stackTrace));
    if (!groupId()) return;
    consoleMessageStorage()->addMessage(std::move(message));
  }

  void reportDeprecatedCall(const char* id, const String16& message) {
    V8ConsoleMessageStorage* storage = consoleMessageStorage();
    if (!storage || !storage->shouldReportDeprecationMessage(contextId(), id)) {
      return;
    }
    auto arguments =
        std::to_array<v8::Local<v8::Value>>({toV8String(isolate(), message)});
    reportCall(ConsoleAPIType::kWarning, arguments);
  }

  bool firstArgToBoolean(bool defaultValue) {
    if (m_info.Length() < 1) return defaultValue;
    if (m_info[0]->IsBoolean()) return m_info[0].As<v8::Boolean>()->Value();
    return m_info[0]->BooleanValue(m_inspector->isolate());
  }

  // Returns the first argument converted to a string, or "default" if the
  // first argument is missing or undefined. Returns an empty MaybeLocal if
  // the conversion throws an exception or terminates execution.
  v8::MaybeLocal<v8::String> firstArgToString() {
    if (V8_LIKELY(m_info.Length() > 0)) {
      v8::Local<v8::Value> arg = m_info[0];
      if (V8_LIKELY(arg->IsString())) {
        return arg.As<v8::String>();
      }
      if (!arg->IsUndefined()) return arg->ToString(context());
    }
    return toV8StringInternalized(isolate(), "default");
  }

  v8::MaybeLocal<v8::Object> firstArgAsObject() {
    if (m_info.Length() < 1 || !m_info[0]->IsObject()) {
      return v8::MaybeLocal<v8::Object>();
    }
    return m_info[0].As<v8::Object>();
  }

  v8::MaybeLocal<v8::Function> firstArgAsFunction() {
    if (m_info.Length() < 1 || !m_info[0]->IsFunction()) {
      return v8::MaybeLocal<v8::Function>();
    }
    v8::Local<v8::Function> func = m_info[0].As<v8::Function>();
    while (func->GetBoundFunction()->IsFunction()) {
      func = func->GetBoundFunction().As<v8::Function>();
    }
    return func;
  }

  void forEachSession(std::function<void(V8InspectorSessionImpl*)> callback) {
    m_inspector->forEachSession(groupId(), std::move(callback));
  }

 private:
  const v8::debug::ConsoleCallArguments& m_info;
  const v8::debug::ConsoleContext& m_consoleContext;
  V8InspectorImpl* m_inspector;
  std::shared_ptr<InspectedContext> m_inspectedContext;
};

void createBoundFunctionProperty(
    v8::Local<v8::Context> context, v8::Local<v8::Object> console,
    v8::Local<v8::Data> data, const char* name, v8::FunctionCallback callback,
    v8::SideEffectType side_effect_type = v8::SideEffectType::kHasSideEffect) {
  v8::Local<v8::String> funcName =
      toV8StringInternalized(v8::Isolate::GetCurrent(), name);
  v8::Local<v8::Function> func;
  if (!v8::Function::New(context, callback, data, 0,
                         v8::ConstructorBehavior::kThrow, side_effect_type)
           .ToLocal(&func)) {
    return;
  }
  func->SetName(funcName);
  createDataProperty(context, console, funcName, func);
}

enum InspectRequest { kRegular, kCopyToClipboard, kQueryObjects };

}  // namespace

V8Console::V8Console(V8InspectorImpl* inspector) : m_inspector(inspector) {}

void V8Console::Debug(const v8::debug::ConsoleCallArguments& info,
                      const v8::debug::ConsoleContext& consoleContext) {
  TRACE_EVENT(TRACE_DISABLED_BY_DEFAULT("v8.inspector"), "V8Console::Debug");
  ConsoleHelper(info, consoleContext, m_inspector)
      .reportCall(ConsoleAPIType::kDebug);
}

void V8Console::Error(const v8::debug::ConsoleCallArguments& info,
                      const v8::debug::ConsoleContext& consoleContext) {
  TRACE_EVENT(TRACE_DISABLED_BY_DEFAULT("v8.inspector"), "V8Console::Error");
  ConsoleHelper(info, consoleContext, m_inspector)
      .reportCall(ConsoleAPIType::kError);
}

void V8Console::Info(const v8::debug::ConsoleCallArguments& info,
                     const v8::debug::ConsoleContext& consoleContext) {
  TRACE_EVENT(TRACE_DISABLED_BY_DEFAULT("v8.inspector"), "V8Console::Info");
  ConsoleHelper(info, consoleContext, m_inspector)
      .reportCall(ConsoleAPIType::kInfo);
}

void V8Console::Log(const v8::debug::ConsoleCallArguments& info,
                    const v8::debug::ConsoleContext& consoleContext) {
  TRACE_EVENT(TRACE_DISABLED_BY_DEFAULT("v8.inspector"), "V8Console::Log");
  ConsoleHelper(info, consoleContext, m_inspector)
      .reportCall(ConsoleAPIType::kLog);
}

void V8Console::Warn(const v8::debug::ConsoleCallArguments& info,
                     const v8::debug::ConsoleContext& consoleContext) {
  TRACE_EVENT(TRACE_DISABLED_BY_DEFAULT("v8.inspector"), "V8Console::Warn");
  ConsoleHelper(info, consoleContext, m_inspector)
      .reportCall(ConsoleAPIType::kWarning);
}

void V8Console::Dir(const v8::debug::ConsoleCallArguments& info,
                    const v8::debug::ConsoleContext& consoleContext) {
  TRACE_EVENT(TRACE_DISABLED_BY_DEFAULT("v8.inspector"), "V8Console::Dir");
  ConsoleHelper(info, consoleContext, m_inspector)
      .reportCall(ConsoleAPIType::kDir);
}

void V8Console::DirXml(const v8::debug::ConsoleCallArguments& info,
                       const v8::debug::ConsoleContext& consoleContext) {
  TRACE_EVENT(TRACE_DISABLED_BY_DEFAULT("v8.inspector"), "V8Console::DirXml");
  ConsoleHelper(info, consoleContext, m_inspector)
      .reportCall(ConsoleAPIType::kDirXML);
}

void V8Console::Table(const v8::debug::ConsoleCallArguments& info,
                      const v8::debug::ConsoleContext& consoleContext) {
  TRACE_EVENT(TRACE_DISABLED_BY_DEFAULT("v8.inspector"), "V8Console::Table");
  ConsoleHelper(info, consoleContext, m_inspector)
      .reportCall(ConsoleAPIType::kTable);
}

void V8Console::Trace(const v8::debug::ConsoleCallArguments& info,
                      const v8::debug::ConsoleContext& consoleContext) {
  TRACE_EVENT(TRACE_DISABLED_BY_DEFAULT("v8.inspector"), "V8Console::Trace");
  ConsoleHelper(info, consoleContext, m_inspector)
      .reportCallWithDefaultArgument(ConsoleAPIType::kTrace,
                                     String16("console.trace"));
}

void V8Console::Group(const v8::debug::ConsoleCallArguments& info,
                      const v8::debug::ConsoleContext& consoleContext) {
  TRACE_EVENT(TRACE_DISABLED_BY_DEFAULT("v8.inspector"), "V8Console::Group");
  ConsoleHelper(info, consoleContext, m_inspector)
      .reportCallWithDefaultArgument(ConsoleAPIType::kStartGroup,
                                     String16("console.group"));
}

void V8Console::GroupCollapsed(
    const v8::debug::ConsoleCallArguments& info,
    const v8::debug::ConsoleContext& consoleContext) {
  TRACE_EVENT(TRACE_DISABLED_BY_DEFAULT("v8.inspector"),
              "V8Console::GroupCollapsed");
  ConsoleHelper(info, consoleContext, m_inspector)
      .reportCallWithDefaultArgument(ConsoleAPIType::kStartGroupCollapsed,
                                     String16("console.groupCollapsed"));
}

void V8Console::GroupEnd(const v8::debug::ConsoleCallArguments& info,
                         const v8::debug::ConsoleContext& consoleContext) {
  TRACE_EVENT(TRACE_DISABLED_BY_DEFAULT("v8.inspector"), "V8Console::GroupEnd");
  ConsoleHelper(info, consoleContext, m_inspector)
      .reportCallWithDefaultArgument(ConsoleAPIType::kEndGroup,
                                     String16("console.groupEnd"));
}

void V8Console::Clear(const v8::debug::ConsoleCallArguments& info,
                      const v8::debug::ConsoleContext& consoleContext) {
  TRACE_EVENT(TRACE_DISABLED_BY_DEFAULT("v8.inspector"), "V8Console::Clear");
  ConsoleHelper helper(info, consoleContext, m_inspector);
  if (!helper.groupId()) return;
  m_inspector->client()->consoleClear(helper.groupId());
  helper.reportCallWithDefaultArgument(ConsoleAPIType::kClear,
                                       String16("console.clear"));
}

void V8Console::Count(const v8::debug::ConsoleCallArguments& info,
                      const v8::debug::ConsoleContext& consoleContext) {
  TRACE_EVENT_BEGIN(TRACE_DISABLED_BY_DEFAULT("v8.inspector"),
                    "V8Console::Count");
  ConsoleHelper helper(info, consoleContext, m_inspector);
  v8::Local<v8::String> v8Label;
  if (!helper.firstArgToString().ToLocal(&v8Label)) {
    TRACE_EVENT_END(TRACE_DISABLED_BY_DEFAULT("v8.inspector"));
    return;
  }
  String16 label = toProtocolString(m_inspector->isolate(), v8Label);
  // Converting the label to a string above can run arbitrary JavaScript,
  // which might destroy the current context.
  int count = 0;
  if (V8ConsoleMessageStorage* storage = helper.consoleMessageStorage()) {
    count = storage->count(helper.contextId(), consoleContext.id(), label);
    helper.reportCallWithArgument(ConsoleAPIType::kCount,
                                  label + ": " + String16::fromInteger(count));
  }
  TRACE_EVENT_END(TRACE_DISABLED_BY_DEFAULT("v8.inspector"), "label",
                  label.utf8().c_str(), "count", count);
}

void V8Console::CountReset(const v8::debug::ConsoleCallArguments& info,
                           const v8::debug::ConsoleContext& consoleContext) {
  TRACE_EVENT_BEGIN(TRACE_DISABLED_BY_DEFAULT("v8.inspector"),
                    "V8Console::CountReset");
  ConsoleHelper helper(info, consoleContext, m_inspector);
  v8::Local<v8::String> v8Label;
  if (!helper.firstArgToString().ToLocal(&v8Label)) {
    TRACE_EVENT_END(TRACE_DISABLED_BY_DEFAULT("v8.inspector"));
    return;
  }
  String16 label = toProtocolString(m_inspector->isolate(), v8Label);
  // Converting the label to a string above can run arbitrary JavaScript,
  // which might destroy the current context.
  V8ConsoleMessageStorage* storage = helper.consoleMessageStorage();
  if (storage &&
      !storage->countReset(helper.contextId(), consoleContext.id(), label)) {
    helper.reportCallWithArgument(ConsoleAPIType::kWarning,
                                  "Count for '" + label + "' does not exist");
  }
  TRACE_EVENT_END(TRACE_DISABLED_BY_DEFAULT("v8.inspector"), "label",
                  label.utf8().c_str());
}

void V8Console::Assert(const v8::debug::ConsoleCallArguments& info,
                       const v8::debug::ConsoleContext& consoleContext) {
  TRACE_EVENT(TRACE_DISABLED_BY_DEFAULT("v8.inspector"), "V8Console::Assert");
  ConsoleHelper helper(info, consoleContext, m_inspector);
  DCHECK(!helper.firstArgToBoolean(false));

  v8::Isolate* isolate = m_inspector->isolate();
  v8::LocalVector<v8::Value> arguments(isolate);
  for (int i = 1; i < info.Length(); ++i) arguments.push_back(info[i]);
  if (info.Length() < 2) {
    arguments.push_back(toV8String(isolate, String16("console.assert")));
  }
  helper.reportCall(ConsoleAPIType::kAssert, arguments);
  if (!helper.groupId() || isolate->IsExecutionTerminating()) return;
  m_inspector->debugger()->breakProgramOnAssert(helper.groupId());
}

void V8Console::Profile(const v8::debug::ConsoleCallArguments& info,
                        const v8::debug::ConsoleContext& consoleContext) {
  TRACE_EVENT_BEGIN(TRACE_DISABLED_BY_DEFAULT("v8.inspector"),
                    "V8Console::Profile");
  ConsoleHelper helper(info, consoleContext, m_inspector);
  v8::Local<v8::String> v8Title;
  if (!helper.firstArgToString().ToLocal(&v8Title)) {
    TRACE_EVENT_END(TRACE_DISABLED_BY_DEFAULT("v8.inspector"));
    return;
  }
  String16 title = toProtocolString(m_inspector->isolate(), v8Title);
  helper.forEachSession([&title](V8InspectorSessionImpl* session) {
    if (session->profilerAgent()) {
      session->profilerAgent()->consoleProfile(title);
    }
  });
  TRACE_EVENT_END(TRACE_DISABLED_BY_DEFAULT("v8.inspector"), "title",
                  title.utf8().c_str());
}

void V8Console::ProfileEnd(const v8::debug::ConsoleCallArguments& info,
                           const v8::debug::ConsoleContext& consoleContext) {
  TRACE_EVENT_BEGIN(TRACE_DISABLED_BY_DEFAULT("v8.inspector"),
                    "V8Console::ProfileEnd");
  ConsoleHelper helper(info, consoleContext, m_inspector);
  v8::Local<v8::String> v8Title;
  if (!helper.firstArgToString().ToLocal(&v8Title)) {
    TRACE_EVENT_END(TRACE_DISABLED_BY_DEFAULT("v8.inspector"));
    return;
  }
  String16 title = toProtocolString(m_inspector->isolate(), v8Title);
  helper.forEachSession([&title](V8InspectorSessionImpl* session) {
    if (session->profilerAgent()) {
      session->profilerAgent()->consoleProfileEnd(title);
    }
  });
  TRACE_EVENT_END(TRACE_DISABLED_BY_DEFAULT("v8.inspector"), "title",
                  title.utf8().c_str());
}

void V8Console::Time(const v8::debug::ConsoleCallArguments& info,
                     const v8::debug::ConsoleContext& consoleContext) {
  TRACE_EVENT(TRACE_DISABLED_BY_DEFAULT("v8.inspector"), "V8Console::Time");
  ConsoleHelper helper(info, consoleContext, m_inspector);
  v8::Local<v8::String> label;
  if (!helper.firstArgToString().ToLocal(&label)) return;
  String16 protocolLabel = toProtocolString(m_inspector->isolate(), label);
  // Converting the label to a string above can run arbitrary JavaScript,
  // which might destroy the current context.
  V8ConsoleMessageStorage* storage = helper.consoleMessageStorage();
  if (!storage) return;
  if (!storage->time(helper.contextId(), consoleContext.id(), protocolLabel)) {
    helper.reportCallWithArgument(
        ConsoleAPIType::kWarning,
        "Timer '" + protocolLabel + "' already exists");
    return;
  }
  m_inspector->client()->consoleTime(m_inspector->isolate(), label);
}

void V8Console::TimeLog(const v8::debug::ConsoleCallArguments& info,
                        const v8::debug::ConsoleContext& consoleContext) {
  TRACE_EVENT(TRACE_DISABLED_BY_DEFAULT("v8.inspector"), "V8Console::TimeLog");
  ConsoleHelper helper(info, consoleContext, m_inspector);
  v8::Local<v8::String> label;
  if (!helper.firstArgToString().ToLocal(&label)) return;
  String16 protocolLabel = toProtocolString(m_inspector->isolate(), label);
  // Converting the label to a string above can run arbitrary JavaScript,
  // which might destroy the current context.
  V8ConsoleMessageStorage* storage = helper.consoleMessageStorage();
  if (!storage) return;
  std::optional<double> elapsed =
      storage->timeLog(helper.contextId(), consoleContext.id(), protocolLabel);
  if (!elapsed.has_value()) {
    helper.reportCallWithArgument(
        ConsoleAPIType::kWarning,
        "Timer '" + protocolLabel + "' does not exist");
    return;
  }
  String16 message =
      protocolLabel + ": " + String16::fromDouble(elapsed.value()) + " ms";
  helper.reportCallAndReplaceFirstArgument(ConsoleAPIType::kLog, message);
}

void V8Console::TimeEnd(const v8::debug::ConsoleCallArguments& info,
                        const v8::debug::ConsoleContext& consoleContext) {
  TRACE_EVENT(TRACE_DISABLED_BY_DEFAULT("v8.inspector"), "V8Console::TimeEnd");
  ConsoleHelper helper(info, consoleContext, m_inspector);
  v8::Local<v8::String> label;
  if (!helper.firstArgToString().ToLocal(&label)) return;
  String16 protocolLabel = toProtocolString(m_inspector->isolate(), label);
  // Converting the label to a string above can run arbitrary JavaScript,
  // which might destroy the current context.
  V8ConsoleMessageStorage* storage = helper.consoleMessageStorage();
  if (!storage) return;
  std::optional<double> elapsed =
      storage->timeEnd(helper.contextId(), consoleContext.id(), protocolLabel);
  if (!elapsed.has_value()) {
    helper.reportCallWithArgument(
        ConsoleAPIType::kWarning,
        "Timer '" + protocolLabel + "' does not exist");
    return;
  }
  m_inspector->client()->consoleTimeEnd(m_inspector->isolate(), label);
  String16 message =
      protocolLabel + ": " + String16::fromDouble(elapsed.value()) + " ms";
  helper.reportCallWithArgument(ConsoleAPIType::kTimeEnd, message);
}

void V8Console::TimeStamp(const v8::debug::ConsoleCallArguments& info,
                          const v8::debug::ConsoleContext& consoleContext) {
  TRACE_EVENT(TRACE_DISABLED_BY_DEFAULT("v8.inspector"),
              "V8Console::TimeStamp");
  ConsoleHelper helper(info, consoleContext, m_inspector);
  v8::Local<v8::String> label;
  if (!helper.firstArgToString().ToLocal(&label)) return;

  v8::Isolate* isolate = m_inspector->isolate();
  v8::LocalVector<v8::Value> args(isolate);
  args.reserve(info.Length());
  for (int i = 0; i < info.Length(); i++) {
    args.push_back(info[i]);
  }

  m_inspector->client()->consoleTimeStampWithArgs(isolate, label, args);
}

void V8Console::memoryGetterCallback(
    const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Value> memoryValue;
  if (!m_inspector->client()
           ->memoryInfo(info.GetIsolate(),
                        info.GetIsolate()->GetCurrentContext())
           .ToLocal(&memoryValue)) {
    return;
  }
  info.GetReturnValue().Set(memoryValue);
}

void V8Console::memorySetterCallback(
    const v8::FunctionCallbackInfo<v8::Value>& info) {
  // We can't make the attribute readonly as it breaks existing code that relies
  // on being able to assign to console.memory in strict mode. Instead, the
  // setter just ignores the passed value.  http://crbug.com/468611
}

void V8Console::createTask(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();

  v8::debug::RecordAsyncStackTaggingCreateTaskCall(isolate);

  if (info.Length() < 1 || !info[0]->IsString() ||
      !info[0].As<v8::String>()->Length()) {
    isolate->ThrowError("First argument must be a non-empty string.");
    return;
  }

  v8::Local<v8::Object> task = taskTemplate()
                                   ->NewInstance(isolate->GetCurrentContext())
                                   .ToLocalChecked();

  auto* taskInfo = cppgc::MakeGarbageCollected<TaskInfo>(
      isolate->GetCppHeap()->GetAllocationHandle(), isolate, this);
  void* taskId = taskInfo->Id();
  v8::Object::Wrap<TaskInfo::kPointerTag>(isolate, task, taskInfo);

  String16 nameArgument = toProtocolString(isolate, info[0].As<v8::String>());
  StringView taskName =
      StringView(nameArgument.characters16(), nameArgument.length());
  m_inspector->asyncTaskScheduled(taskName, taskId, /* recurring */ true);

  info.GetReturnValue().Set(task);
}

v8::Local<v8::ObjectTemplate> V8Console::taskTemplate() {
  v8::Isolate* isolate = m_inspector->isolate();
  if (!m_taskTemplate.IsEmpty()) {
    return m_taskTemplate.Get(isolate);
  }

  v8::Local<v8::FunctionTemplate> consTemplate =
      v8::FunctionTemplate::New(isolate);
  v8::Local<v8::ObjectTemplate> taskTemplate = consTemplate->InstanceTemplate();
  v8::Local<v8::Signature> signature =
      v8::Signature::New(isolate, consTemplate);
  v8::Local<v8::FunctionTemplate> funcTemplate = v8::FunctionTemplate::New(
      isolate, &TaskInfo::runTask, v8::Local<v8::Value>(), signature);
  taskTemplate->Set(isolate, "run", funcTemplate);

  m_taskTemplate.Reset(isolate, taskTemplate);
  return taskTemplate;
}

TaskInfo::TaskInfo(v8::Isolate* isolate, V8Console* console)
    : m_isolate(isolate), m_console(console), m_id(new char(0)) {}

TaskInfo::~TaskInfo() {
  auto* inspector =
      static_cast<V8InspectorImpl*>(v8::debug::GetInspector(m_isolate));
  if (inspector) {
    inspector->asyncTaskCanceled(Id());
  }
}

void TaskInfo::Trace(cppgc::Visitor* visitor) const {
  visitor->Trace(m_console);
  v8::Object::Wrappable::Trace(visitor);
}

void TaskInfo::runTask(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  if (info.Length() < 1 || !info[0]->IsFunction()) {
    isolate->ThrowError("First argument must be a function.");
    return;
  }
  v8::Local<v8::Function> function = info[0].As<v8::Function>();

  v8::Local<v8::Object> task = info.This();
  TaskInfo* taskInfo =
      v8::Object::Unwrap<TaskInfo::kPointerTag, TaskInfo>(isolate, task);
  if (!taskInfo) {
    isolate->ThrowError("'run' called with illegal receiver.");
    return;
  }

  V8Console* console = taskInfo->m_console.Get();
  CHECK_NOT_NULL(console);
  if (!console->m_inspector) {
    // Inspector has been cleared, so we're in shutdown.
    return;
  }
  console->m_inspector->asyncTaskStarted(taskInfo->Id());
  {
#ifdef V8_USE_PERFETTO
    TRACE_EVENT(TRACE_DISABLED_BY_DEFAULT("v8.inspector"), "V8Console::runTask",
                "data", ([&](perfetto::TracedValue context) {
                  uint64_t trace_id = v8::tracing::TraceId();
                  auto dict = std::move(context).WriteDictionary();
                  v8::CpuProfiler::CpuProfiler::CollectSample(isolate,
                                                              trace_id);
                  dict.Add("sampleTraceId", trace_id);
                }));
#endif  // V8_USE_PERFETTO

    v8::Local<v8::Value> result;
    if (function
            ->Call(isolate->GetCurrentContext(), v8::Undefined(isolate), 0, {})
            .ToLocal(&result)) {
      info.GetReturnValue().Set(result);
    }
  }
  console->m_inspector->asyncTaskFinished(taskInfo->Id());
}

void V8Console::CommandLineAPIData::Trace(cppgc::Visitor* visitor) const {
  visitor->Trace(m_console);
}

void V8Console::Trace(cppgc::Visitor* visitor) const {
  visitor->Trace(m_taskTemplate);
  v8::debug::ConsoleDelegate::Trace(visitor);
}

void V8Console::keysCallback(const v8::FunctionCallbackInfo<v8::Value>& info,
                             int sessionId) {
  v8::Isolate* isolate = info.GetIsolate();
  info.GetReturnValue().Set(v8::Array::New(isolate));

  v8::debug::ConsoleCallArguments args(info);
  ConsoleHelper helper(args, v8::debug::ConsoleContext(), m_inspector);
  v8::Local<v8::Object> obj;
  if (!helper.firstArgAsObject().ToLocal(&obj)) return;
  v8::Local<v8::Array> names;
  if (!obj->GetOwnPropertyNames(isolate->GetCurrentContext()).ToLocal(&names)) {
    return;
  }
  info.GetReturnValue().Set(names);
}

void V8Console::valuesCallback(const v8::FunctionCallbackInfo<v8::Value>& info,
                               int sessionId) {
  v8::Isolate* isolate = info.GetIsolate();
  info.GetReturnValue().Set(v8::Array::New(isolate));

  v8::debug::ConsoleCallArguments args(info);
  ConsoleHelper helper(args, v8::debug::ConsoleContext(), m_inspector);
  v8::Local<v8::Object> obj;
  if (!helper.firstArgAsObject().ToLocal(&obj)) return;
  v8::Local<v8::Array> names;
  v8::Local<v8::Context> context = isolate->GetCurrentContext();
  if (!obj->GetOwnPropertyNames(context).ToLocal(&names)) return;
  v8::Local<v8::Array> values = v8::Array::New(isolate, names->Length());
  for (uint32_t i = 0; i < names->Length(); ++i) {
    v8::Local<v8::Value> key;
    if (!names->Get(context, i).ToLocal(&key)) continue;
    v8::Local<v8::Value> value;
    if (!obj->Get(context, key).ToLocal(&value)) continue;
    createDataProperty(context, values, i, value);
  }
  info.GetReturnValue().Set(values);
}

static void setFunctionBreakpoint(ConsoleHelper& helper, int sessionId,
                                  v8::Local<v8::Function> function,
                                  V8DebuggerAgentImpl::BreakpointSource source,
                                  v8::Local<v8::String> condition,
                                  bool enable) {
  V8InspectorSessionImpl* session = helper.session(sessionId);
  if (session == nullptr) return;
  if (!session->debuggerAgent()->enabled()) return;
  if (enable) {
    session->debuggerAgent()->setBreakpointFor(function, condition, source);
  } else {
    session->debuggerAgent()->removeBreakpointFor(function, source);
  }
}

void V8Console::debugFunctionCallback(
    const v8::FunctionCallbackInfo<v8::Value>& info, int sessionId) {
  v8::debug::ConsoleCallArguments args(info);
  ConsoleHelper helper(args, v8::debug::ConsoleContext(), m_inspector);
  v8::Local<v8::Function> function;
  v8::Local<v8::String> condition;
  if (!helper.firstArgAsFunction().ToLocal(&function)) return;
  if (args.Length() > 1 && args[1]->IsString()) {
    condition = args[1].As<v8::String>();
  }
  setFunctionBreakpoint(helper, sessionId, function,
                        V8DebuggerAgentImpl::DebugCommandBreakpointSource,
                        condition, true);
}

void V8Console::undebugFunctionCallback(
    const v8::FunctionCallbackInfo<v8::Value>& info, int sessionId) {
  v8::debug::ConsoleCallArguments args(info);
  ConsoleHelper helper(args, v8::debug::ConsoleContext(), m_inspector);
  v8::Local<v8::Function> function;
  if (!helper.firstArgAsFunction().ToLocal(&function)) return;
  setFunctionBreakpoint(helper, sessionId, function,
                        V8DebuggerAgentImpl::DebugCommandBreakpointSource,
                        v8::Local<v8::String>(), false);
}

void V8Console::monitorFunctionCallback(
    const v8::FunctionCallbackInfo<v8::Value>& info, int sessionId) {
  v8::debug::ConsoleCallArguments args(info);
  ConsoleHelper helper(args, v8::debug::ConsoleContext(), m_inspector);
  v8::Local<v8::Function> function;
  if (!helper.firstArgAsFunction().ToLocal(&function)) return;
  v8::Local<v8::Value> name = function->GetName();
  if (!name->IsString() || !name.As<v8::String>()->Length()) {
    name = function->GetInferredName();
  }
  String16 functionName =
      toProtocolStringWithTypeCheck(info.GetIsolate(), name);
  String16Builder builder;
  builder.append("console.log(\"function ");
  if (functionName.isEmpty()) {
    builder.append("(anonymous function)");
  } else {
    builder.append(functionName);
  }
  builder.append(
      " called\" + (typeof arguments !== \"undefined\" && arguments.length > 0 "
      "? \" with arguments: \" + Array.prototype.join.call(arguments, \", \") "
      ": \"\")) && false");
  setFunctionBreakpoint(helper, sessionId, function,
                        V8DebuggerAgentImpl::MonitorCommandBreakpointSource,
                        toV8String(info.GetIsolate(), builder.toString()),
                        true);
}

void V8Console::unmonitorFunctionCallback(
    const v8::FunctionCallbackInfo<v8::Value>& info, int sessionId) {
  v8::debug::ConsoleCallArguments args(info);
  ConsoleHelper helper(args, v8::debug::ConsoleContext(), m_inspector);
  v8::Local<v8::Function> function;
  if (!helper.firstArgAsFunction().ToLocal(&function)) return;
  setFunctionBreakpoint(helper, sessionId, function,
                        V8DebuggerAgentImpl::MonitorCommandBreakpointSource,
                        v8::Local<v8::String>(), false);
}

void V8Console::lastEvaluationResultCallback(
    const v8::FunctionCallbackInfo<v8::Value>& info, int sessionId) {
  v8::debug::ConsoleCallArguments args(info);
  ConsoleHelper helper(args, v8::debug::ConsoleContext(), m_inspector);
  std::shared_ptr<InjectedScript> injectedScript =
      helper.injectedScript(sessionId);
  if (!injectedScript) return;
  info.GetReturnValue().Set(injectedScript->lastEvaluationResult());
}

static void inspectImpl(const v8::FunctionCallbackInfo<v8::Value>& info,
                        v8::Local<v8::Value> value, int sessionId,
                        InspectRequest request, V8InspectorImpl* inspector,
                        bool omitFocus = false) {
  if (request == kRegular) info.GetReturnValue().Set(value);

  v8::debug::ConsoleCallArguments args(info);
  ConsoleHelper helper(args, v8::debug::ConsoleContext(), inspector);
  std::shared_ptr<InjectedScript> injectedScript =
      helper.injectedScript(sessionId);
  if (!injectedScript) return;
  std::unique_ptr<protocol::Runtime::RemoteObject> wrappedObject;
  protocol::Response response = injectedScript->wrapObject(
      value, "", WrapOptions({WrapMode::kIdOnly}), &wrappedObject);
  if (!response.IsSuccess()) return;

  std::unique_ptr<protocol::DictionaryValue> hints =
      protocol::DictionaryValue::create();
  if (request == kCopyToClipboard) {
    hints->setBoolean("copyToClipboard", true);
  } else if (request == kQueryObjects) {
    hints->setBoolean("queryObjects", true);
  }
  if (omitFocus) {
    hints->setBoolean("omitFocus", true);
  }
  if (V8InspectorSessionImpl* session = helper.session(sessionId)) {
    session->runtimeAgent()->inspect(std::move(wrappedObject), std::move(hints),
                                     helper.contextId());
  }
}

void V8Console::inspectCallback(const v8::FunctionCallbackInfo<v8::Value>& info,
                                int sessionId) {
  if (info.Length() < 1) return;
  bool omitFocus = false;
  if (info.Length() >= 2 && info[1]->IsObject()) {
    v8::Isolate* isolate = info.GetIsolate();
    v8::Local<v8::Context> context = isolate->GetCurrentContext();
    v8::Local<v8::Object> options = info[1].As<v8::Object>();
    v8::Local<v8::Value> focusValue;
    if (options->Get(context, toV8StringInternalized(isolate, "focus"))
            .ToLocal(&focusValue) &&
        focusValue->IsFalse()) {
      omitFocus = true;
    }
  }
  inspectImpl(info, info[0], sessionId, kRegular, m_inspector, omitFocus);
}

void V8Console::copyCallback(const v8::FunctionCallbackInfo<v8::Value>& info,
                             int sessionId) {
  if (info.Length() < 1) return;
  inspectImpl(info, info[0], sessionId, kCopyToClipboard, m_inspector);
}

void V8Console::queryObjectsCallback(
    const v8::FunctionCallbackInfo<v8::Value>& info, int sessionId) {
  if (info.Length() < 1) return;
  v8::Local<v8::Value> arg = info[0];
  if (arg->IsFunction()) {
    v8::Isolate* isolate = info.GetIsolate();
    v8::TryCatch tryCatch(isolate);
    v8::Local<v8::Value> prototype;
    if (arg.As<v8::Function>()
            ->Get(isolate->GetCurrentContext(),
                  toV8StringInternalized(isolate, "prototype"))
            .ToLocal(&prototype) &&
        prototype->IsObject()) {
      arg = prototype;
    }
    if (tryCatch.HasCaught()) {
      tryCatch.ReThrow();
      return;
    }
  }
  inspectImpl(info, arg, sessionId, kQueryObjects, m_inspector);
}

void V8Console::inspectedObject(const v8::FunctionCallbackInfo<v8::Value>& info,
                                int sessionId, unsigned num) {
  DCHECK_GT(V8InspectorSessionImpl::kInspectedObjectBufferSize, num);
  v8::debug::ConsoleCallArguments args(info);
  ConsoleHelper helper(args, v8::debug::ConsoleContext(), m_inspector);
  if (V8InspectorSessionImpl* session = helper.session(sessionId)) {
    V8InspectorSession::Inspectable* object = session->inspectedObject(num);
    v8::Isolate* isolate = info.GetIsolate();
    if (object) {
      info.GetReturnValue().Set(object->get(isolate->GetCurrentContext()));
    } else {
      info.GetReturnValue().Set(v8::Undefined(isolate));
    }
  }
}

void V8Console::installMemoryGetter(v8::Local<v8::Context> context,
                                    v8::Local<v8::Object> console) {
  v8::Isolate* isolate = v8::Isolate::GetCurrent();
  v8::Local<v8::Data> data =
      v8::CppHeapExternal::New<V8Console>(isolate, this, kPointerTag);
  console->SetAccessorProperty(
      toV8StringInternalized(isolate, "memory"),
      v8::Function::New(
          context, &V8Console::call<&V8Console::memoryGetterCallback>, data, 0,
          v8::ConstructorBehavior::kThrow, v8::SideEffectType::kHasNoSideEffect)
          .ToLocalChecked(),
      v8::Function::New(context,
                        &V8Console::call<&V8Console::memorySetterCallback>,
                        data, 1, v8::ConstructorBehavior::kThrow)
          .ToLocalChecked(),
      static_cast<v8::PropertyAttribute>(v8::None));
}

void V8Console::installAsyncStackTaggingAPI(v8::Local<v8::Context> context,
                                            v8::Local<v8::Object> console) {
  v8::Isolate* isolate = v8::Isolate::GetCurrent();
  v8::Local<v8::Data> data =
      v8::CppHeapExternal::New<V8Console>(isolate, this, kPointerTag);

  v8::MicrotasksScope microtasksScope(context,
                                      v8::MicrotasksScope::kDoNotRunMicrotasks);

  createBoundFunctionProperty(context, console, data, "createTask",
                              &V8Console::call<&V8Console::createTask>);
}

v8::Local<v8::Object> V8Console::createCommandLineAPI(
    v8::Local<v8::Context> context, int sessionId) {
  v8::Isolate* isolate = v8::Isolate::GetCurrent();
  v8::MicrotasksScope microtasksScope(context,
                                      v8::MicrotasksScope::kDoNotRunMicrotasks);

  v8::Local<v8::Object> commandLineAPI = v8::Object::New(isolate);
  bool success =
      commandLineAPI->SetPrototype(context, v8::Null(isolate)).FromMaybe(false);
  DCHECK(success);
  USE(success);

  v8::Local<v8::Data> consoleData =
      v8::CppHeapExternal::New<V8Console>(isolate, this, kPointerTag);

  createBoundFunctionProperty(context, commandLineAPI, consoleData, "dir",
                              &V8Console::call<&V8Console::Dir>);
  createBoundFunctionProperty(context, commandLineAPI, consoleData, "dirxml",
                              &V8Console::call<&V8Console::DirXml>);
  createBoundFunctionProperty(context, commandLineAPI, consoleData, "profile",
                              &V8Console::call<&V8Console::Profile>);
  createBoundFunctionProperty(context, commandLineAPI, consoleData,
                              "profileEnd",
                              &V8Console::call<&V8Console::ProfileEnd>);
  createBoundFunctionProperty(context, commandLineAPI, consoleData, "clear",
                              &V8Console::call<&V8Console::Clear>);
  createBoundFunctionProperty(context, commandLineAPI, consoleData, "table",
                              &V8Console::call<&V8Console::Table>);

  auto* sessionData = cppgc::MakeGarbageCollected<CommandLineAPIData>(
      isolate->GetCppHeap()->GetAllocationHandle(), this, sessionId);
  v8::Local<v8::Data> data = v8::CppHeapExternal::New<CommandLineAPIData>(
      isolate, sessionData, CommandLineAPIData::kPointerTag);

  createBoundFunctionProperty(context, commandLineAPI, data, "keys",
                              &V8Console::call<&V8Console::keysCallback>,
                              v8::SideEffectType::kHasNoSideEffect);
  createBoundFunctionProperty(context, commandLineAPI, data, "values",
                              &V8Console::call<&V8Console::valuesCallback>,
                              v8::SideEffectType::kHasNoSideEffect);
  createBoundFunctionProperty(
      context, commandLineAPI, data, "debug",
      &V8Console::call<&V8Console::debugFunctionCallback>);
  createBoundFunctionProperty(
      context, commandLineAPI, data, "undebug",
      &V8Console::call<&V8Console::undebugFunctionCallback>);
  createBoundFunctionProperty(
      context, commandLineAPI, data, "monitor",
      &V8Console::call<&V8Console::monitorFunctionCallback>);
  createBoundFunctionProperty(
      context, commandLineAPI, data, "unmonitor",
      &V8Console::call<&V8Console::unmonitorFunctionCallback>);
  createBoundFunctionProperty(context, commandLineAPI, data, "inspect",
                              &V8Console::call<&V8Console::inspectCallback>);
  createBoundFunctionProperty(context, commandLineAPI, data, "copy",
                              &V8Console::call<&V8Console::copyCallback>);
  createBoundFunctionProperty(
      context, commandLineAPI, data, "queryObjects",
      &V8Console::call<&V8Console::queryObjectsCallback>);
  createBoundFunctionProperty(
      context, commandLineAPI, data, "$_",
      &V8Console::call<&V8Console::lastEvaluationResultCallback>,
      v8::SideEffectType::kHasNoSideEffect);
  createBoundFunctionProperty(context, commandLineAPI, data, "$0",
                              &V8Console::call<&V8Console::inspectedObject0>,
                              v8::SideEffectType::kHasNoSideEffect);
  createBoundFunctionProperty(context, commandLineAPI, data, "$1",
                              &V8Console::call<&V8Console::inspectedObject1>,
                              v8::SideEffectType::kHasNoSideEffect);
  createBoundFunctionProperty(context, commandLineAPI, data, "$2",
                              &V8Console::call<&V8Console::inspectedObject2>,
                              v8::SideEffectType::kHasNoSideEffect);
  createBoundFunctionProperty(context, commandLineAPI, data, "$3",
                              &V8Console::call<&V8Console::inspectedObject3>,
                              v8::SideEffectType::kHasNoSideEffect);
  createBoundFunctionProperty(context, commandLineAPI, data, "$4",
                              &V8Console::call<&V8Console::inspectedObject4>,
                              v8::SideEffectType::kHasNoSideEffect);

  m_inspector->client()->installAdditionalCommandLineAPI(context,
                                                         commandLineAPI);
  return commandLineAPI;
}

static bool isCommandLineAPIGetter(const String16& name) {
  if (name.length() != 2) return false;
  // $0 ... $4, $_
  return name[0] == '$' &&
         ((name[1] >= '0' && name[1] <= '4') || name[1] == '_');
}

void V8Console::CommandLineAPIScope::Data::Trace(
    cppgc::Visitor* visitor) const {
  visitor->Trace(m_commandLineAPI);
  visitor->Trace(m_installedMethods);
}

void V8Console::CommandLineAPIScope::accessorGetterCallback(
    v8::Local<v8::Name> name, const v8::PropertyCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  v8::Local<v8::Context> context = isolate->GetCurrentContext();
  Data* data = info.DataV2().As<v8::CppHeapExternal>()->Value<Data>(
      isolate, {Data::kPointerTag, Data::kPointerTag});
  CHECK_NOT_NULL(data);
  v8::Local<v8::Object> commandLineAPI = data->commandLineAPI(isolate);
  if (commandLineAPI.IsEmpty()) {
    USE(info.Holder()->Delete(context, name).FromMaybe(false));
    return;
  }

  v8::Local<v8::Value> value;
  if (!commandLineAPI->Get(context, name).ToLocal(&value)) return;
  if (isCommandLineAPIGetter(
          toProtocolStringWithTypeCheck(info.GetIsolate(), name))) {
    DCHECK(value->IsFunction());
    v8::MicrotasksScope microtasks(context,
                                   v8::MicrotasksScope::kDoNotRunMicrotasks);
    if (value.As<v8::Function>()
            ->Call(context, commandLineAPI, 0, nullptr)
            .ToLocal(&value)) {
      info.GetReturnValue().Set(value);
    }
  } else {
    info.GetReturnValue().Set(value);
  }
}

void V8Console::CommandLineAPIScope::accessorSetterCallback(
    v8::Local<v8::Name> name, v8::Local<v8::Value> value,
    const v8::PropertyCallbackInfo<v8::Boolean>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  v8::Local<v8::Context> context = isolate->GetCurrentContext();
  Data* data = info.DataV2().As<v8::CppHeapExternal>()->Value<Data>(
      isolate, {Data::kPointerTag, Data::kPointerTag});
  CHECK_NOT_NULL(data);
  if (data->commandLineAPI(isolate).IsEmpty()) return;

  if (!info.Holder()->Delete(context, name).FromMaybe(false)) return;
  if (!info.Holder()
           ->CreateDataProperty(context, name, value)
           .FromMaybe(false)) {
    return;
  }

  v8::Local<v8::PrimitiveArray> methods = data->installedMethods(isolate);
  for (int i = 0; i < methods->Length(); ++i) {
    v8::Local<v8::Primitive> methodName = methods->Get(isolate, i);
    if (!name->StrictEquals(methodName)) continue;
    methods->Set(isolate, i, v8::Undefined(isolate));
    break;
  }
}

namespace {

// "get"-ting these functions from the global proxy is considered a side-effect.
// Otherwise, malicious sites could stash references to these functions through
// previews / ValueMirror and use them across origin isolation.
DEFINE_LAZY_LEAKY_OBJECT_GETTER(std::set<std::string_view>,
                                UnsafeCommandLineAPIFns,
                                std::initializer_list<std::string_view>{
                                    "debug", "undebug", "monitor", "unmonitor",
                                    "inspect", "copy", "queryObjects"})

bool IsUnsafeCommandLineAPIFn(v8::Local<v8::Context> context,
                              v8::Local<v8::Object> commandLineAPI,
                              v8::Local<v8::Name> name, v8::Isolate* isolate) {
  std::string nameStr = toProtocolStringWithTypeCheck(isolate, name).utf8();
  if (UnsafeCommandLineAPIFns()->count(nameStr) > 0) return true;

  v8::Local<v8::Value> descriptor_val;
  if (!commandLineAPI->GetOwnPropertyDescriptor(context, name)
           .ToLocal(&descriptor_val) ||
      !descriptor_val->IsObject()) {
    return false;
  }
  v8::Local<v8::Object> descriptor = descriptor_val.As<v8::Object>();
  v8::Local<v8::Value> value;
  if (!descriptor
           ->Get(context, v8::String::NewFromUtf8Literal(isolate, "value"))
           .ToLocal(&value)) {
    return false;
  }

  return v8::debug::IsAPIFunctionWithSideEffects(value);
}

}  // namespace

V8Console::CommandLineAPIScope::CommandLineAPIScope(
    v8::Local<v8::Context> context, v8::Local<v8::Object> commandLineAPI,
    v8::Local<v8::Object> global)
    : m_isolate(v8::Isolate::GetCurrent()),
      m_context(m_isolate, context),
      m_global(m_isolate, global),
      m_data(cppgc::MakeGarbageCollected<Data>(
          m_isolate->GetCppHeap()->GetAllocationHandle(), m_isolate,
          commandLineAPI)) {
  v8::MicrotasksScope microtasksScope(context,
                                      v8::MicrotasksScope::kDoNotRunMicrotasks);
  v8::Local<v8::Array> names;
  if (!commandLineAPI->GetOwnPropertyNames(context).ToLocal(&names)) return;
  v8::Local<v8::PrimitiveArray> installedMethods =
      v8::PrimitiveArray::New(m_isolate, names->Length());
  m_data->setInstalledMethods(m_isolate, installedMethods);
  v8::Local<v8::Data> externalData = v8::CppHeapExternal::New<Data>(
      m_isolate, m_data.Get(), Data::kPointerTag);

  for (uint32_t i = 0; i < names->Length(); ++i) {
    v8::Local<v8::Value> name;
    if (!names->Get(context, i).ToLocal(&name) || !name->IsName()) continue;
    if (global->HasRealNamedProperty(context, name.As<v8::Name>())
            .FromMaybe(true)) {
      continue;
    }

    const v8::SideEffectType get_accessor_side_effect_type =
        IsUnsafeCommandLineAPIFn(context, commandLineAPI, name.As<v8::Name>(),
                                 isolate())
            ? v8::SideEffectType::kHasSideEffect
            : v8::SideEffectType::kHasNoSideEffect;
    if (!global
             ->SetNativeDataProperty(
                 context, name.As<v8::Name>(),
                 CommandLineAPIScope::accessorGetterCallback,
                 CommandLineAPIScope::accessorSetterCallback, externalData,
                 v8::DontEnum, get_accessor_side_effect_type)
             .FromMaybe(false)) {
      continue;
    }
    installedMethods->Set(m_isolate, i, name.As<v8::Name>());
  }
}

V8Console::CommandLineAPIScope::~CommandLineAPIScope() {
  m_data->clearCommandLineAPI();
  if (m_isolate->IsExecutionTerminating()) return;
  v8::MicrotasksScope microtasksScope(context(),
                                      v8::MicrotasksScope::kDoNotRunMicrotasks);
  v8::Local<v8::PrimitiveArray> names = m_data->installedMethods(m_isolate);
  if (names.IsEmpty()) return;
  for (int i = 0; i < names->Length(); ++i) {
    v8::Local<v8::Primitive> name = names->Get(m_isolate, i);
    if (name->IsName()) {
      v8::Local<v8::Value> descriptor;
      bool success = global()
                         ->GetOwnPropertyDescriptor(context(),
                                                    name.As<v8::Name>())
                         .ToLocal(&descriptor);
      USE(success);
    }
  }
}

}  // namespace v8_inspector
