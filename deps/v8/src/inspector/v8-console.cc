// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/inspector/v8-console.h"

#include "src/base/macros.h"
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
#include "src/inspector/v8-value-copier.h"

#include "include/v8-inspector.h"

namespace v8_inspector {

namespace {

class ConsoleHelper {
 public:
  explicit ConsoleHelper(const v8::FunctionCallbackInfo<v8::Value>& info,
                         V8InspectorImpl* inspector)
      : m_info(info),
        m_isolate(info.GetIsolate()),
        m_context(info.GetIsolate()->GetCurrentContext()),
        m_inspector(inspector),
        m_contextId(InspectedContext::contextId(m_context)),
        m_groupId(m_inspector->contextGroupId(m_contextId)) {}

  int contextId() const { return m_contextId; }
  int groupId() const { return m_groupId; }

  InjectedScript* injectedScript() {
    InspectedContext* context = m_inspector->getContext(m_groupId, m_contextId);
    if (!context) return nullptr;
    return context->getInjectedScript();
  }

  V8ConsoleMessageStorage* consoleMessageStorage() {
    return m_inspector->ensureConsoleMessageStorage(m_groupId);
  }

  void reportCall(ConsoleAPIType type) {
    if (!m_info.Length()) return;
    std::vector<v8::Local<v8::Value>> arguments;
    for (int i = 0; i < m_info.Length(); ++i) arguments.push_back(m_info[i]);
    reportCall(type, arguments);
  }

  void reportCallWithDefaultArgument(ConsoleAPIType type,
                                     const String16& message) {
    std::vector<v8::Local<v8::Value>> arguments;
    for (int i = 0; i < m_info.Length(); ++i) arguments.push_back(m_info[i]);
    if (!m_info.Length()) arguments.push_back(toV8String(m_isolate, message));
    reportCall(type, arguments);
  }

  void reportCallWithArgument(ConsoleAPIType type, const String16& message) {
    std::vector<v8::Local<v8::Value>> arguments(1,
                                                toV8String(m_isolate, message));
    reportCall(type, arguments);
  }

  void reportCall(ConsoleAPIType type,
                  const std::vector<v8::Local<v8::Value>>& arguments) {
    if (!m_groupId) return;
    std::unique_ptr<V8ConsoleMessage> message =
        V8ConsoleMessage::createForConsoleAPI(
            m_context, m_contextId, m_groupId, m_inspector,
            m_inspector->client()->currentTimeMS(), type, arguments,
            m_inspector->debugger()->captureStackTrace(false));
    consoleMessageStorage()->addMessage(std::move(message));
  }

  void reportDeprecatedCall(const char* id, const String16& message) {
    if (!consoleMessageStorage()->shouldReportDeprecationMessage(m_contextId,
                                                                 id)) {
      return;
    }
    std::vector<v8::Local<v8::Value>> arguments(1,
                                                toV8String(m_isolate, message));
    reportCall(ConsoleAPIType::kWarning, arguments);
  }

  bool firstArgToBoolean(bool defaultValue) {
    if (m_info.Length() < 1) return defaultValue;
    if (m_info[0]->IsBoolean()) return m_info[0].As<v8::Boolean>()->Value();
    return m_info[0]->BooleanValue(m_context).FromMaybe(defaultValue);
  }

  String16 firstArgToString(const String16& defaultValue) {
    if (m_info.Length() < 1) return defaultValue;
    v8::Local<v8::String> titleValue;
    if (m_info[0]->IsObject()) {
      if (!m_info[0].As<v8::Object>()->ObjectProtoToString(m_context).ToLocal(
              &titleValue))
        return defaultValue;
    } else {
      if (!m_info[0]->ToString(m_context).ToLocal(&titleValue))
        return defaultValue;
    }
    return toProtocolString(titleValue);
  }

  v8::MaybeLocal<v8::Object> firstArgAsObject() {
    if (m_info.Length() < 1 || !m_info[0]->IsObject())
      return v8::MaybeLocal<v8::Object>();
    return m_info[0].As<v8::Object>();
  }

  v8::MaybeLocal<v8::Function> firstArgAsFunction() {
    if (m_info.Length() < 1 || !m_info[0]->IsFunction())
      return v8::MaybeLocal<v8::Function>();
    v8::Local<v8::Function> func = m_info[0].As<v8::Function>();
    while (func->GetBoundFunction()->IsFunction())
      func = func->GetBoundFunction().As<v8::Function>();
    return func;
  }

  V8ProfilerAgentImpl* profilerAgent() {
    if (V8InspectorSessionImpl* session = currentSession()) {
      if (session && session->profilerAgent()->enabled())
        return session->profilerAgent();
    }
    return nullptr;
  }

  V8DebuggerAgentImpl* debuggerAgent() {
    if (V8InspectorSessionImpl* session = currentSession()) {
      if (session && session->debuggerAgent()->enabled())
        return session->debuggerAgent();
    }
    return nullptr;
  }

  V8InspectorSessionImpl* currentSession() {
    return m_inspector->sessionForContextGroup(m_groupId);
  }

 private:
  const v8::FunctionCallbackInfo<v8::Value>& m_info;
  v8::Isolate* m_isolate;
  v8::Local<v8::Context> m_context;
  V8InspectorImpl* m_inspector = nullptr;
  int m_contextId;
  int m_groupId;

  DISALLOW_COPY_AND_ASSIGN(ConsoleHelper);
};

void returnDataCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  info.GetReturnValue().Set(info.Data());
}

void createBoundFunctionProperty(v8::Local<v8::Context> context,
                                 v8::Local<v8::Object> console,
                                 v8::Local<v8::Value> data, const char* name,
                                 v8::FunctionCallback callback,
                                 const char* description = nullptr) {
  v8::Local<v8::String> funcName =
      toV8StringInternalized(context->GetIsolate(), name);
  v8::Local<v8::Function> func;
  if (!v8::Function::New(context, callback, data, 0,
                         v8::ConstructorBehavior::kThrow)
           .ToLocal(&func))
    return;
  func->SetName(funcName);
  if (description) {
    v8::Local<v8::String> returnValue =
        toV8String(context->GetIsolate(), description);
    v8::Local<v8::Function> toStringFunction;
    if (v8::Function::New(context, returnDataCallback, returnValue, 0,
                          v8::ConstructorBehavior::kThrow)
            .ToLocal(&toStringFunction))
      createDataProperty(context, func, toV8StringInternalized(
                                            context->GetIsolate(), "toString"),
                         toStringFunction);
  }
  createDataProperty(context, console, funcName, func);
}

}  // namespace

V8Console::V8Console(V8InspectorImpl* inspector) : m_inspector(inspector) {}

void V8Console::debugCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  ConsoleHelper(info, m_inspector).reportCall(ConsoleAPIType::kDebug);
}

void V8Console::errorCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  ConsoleHelper(info, m_inspector).reportCall(ConsoleAPIType::kError);
}

void V8Console::infoCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  ConsoleHelper(info, m_inspector).reportCall(ConsoleAPIType::kInfo);
}

void V8Console::logCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  ConsoleHelper(info, m_inspector).reportCall(ConsoleAPIType::kLog);
}

void V8Console::warnCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  ConsoleHelper(info, m_inspector).reportCall(ConsoleAPIType::kWarning);
}

void V8Console::dirCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  ConsoleHelper(info, m_inspector).reportCall(ConsoleAPIType::kDir);
}

void V8Console::dirxmlCallback(
    const v8::FunctionCallbackInfo<v8::Value>& info) {
  ConsoleHelper(info, m_inspector).reportCall(ConsoleAPIType::kDirXML);
}

void V8Console::tableCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  ConsoleHelper(info, m_inspector).reportCall(ConsoleAPIType::kTable);
}

void V8Console::traceCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  ConsoleHelper(info, m_inspector)
      .reportCallWithDefaultArgument(ConsoleAPIType::kTrace,
                                     String16("console.trace"));
}

void V8Console::groupCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  ConsoleHelper(info, m_inspector)
      .reportCallWithDefaultArgument(ConsoleAPIType::kStartGroup,
                                     String16("console.group"));
}

void V8Console::groupCollapsedCallback(
    const v8::FunctionCallbackInfo<v8::Value>& info) {
  ConsoleHelper(info, m_inspector)
      .reportCallWithDefaultArgument(ConsoleAPIType::kStartGroupCollapsed,
                                     String16("console.groupCollapsed"));
}

void V8Console::groupEndCallback(
    const v8::FunctionCallbackInfo<v8::Value>& info) {
  ConsoleHelper(info, m_inspector)
      .reportCallWithDefaultArgument(ConsoleAPIType::kEndGroup,
                                     String16("console.groupEnd"));
}

void V8Console::clearCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  ConsoleHelper helper(info, m_inspector);
  if (!helper.groupId()) return;
  m_inspector->client()->consoleClear(helper.groupId());
  helper.reportCallWithDefaultArgument(ConsoleAPIType::kClear,
                                       String16("console.clear"));
}

void V8Console::countCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  ConsoleHelper helper(info, m_inspector);
  String16 title = helper.firstArgToString(String16());
  String16 identifier;
  if (title.isEmpty()) {
    std::unique_ptr<V8StackTraceImpl> stackTrace =
        V8StackTraceImpl::capture(m_inspector->debugger(), 0, 1);
    if (stackTrace && !stackTrace->isEmpty()) {
      identifier = toString16(stackTrace->topSourceURL()) + ":" +
                   String16::fromInteger(stackTrace->topLineNumber());
    }
  } else {
    identifier = title + "@";
  }

  int count =
      helper.consoleMessageStorage()->count(helper.contextId(), identifier);
  String16 countString = String16::fromInteger(count);
  helper.reportCallWithArgument(
      ConsoleAPIType::kCount,
      title.isEmpty() ? countString : (title + ": " + countString));
}

void V8Console::assertCallback(
    const v8::FunctionCallbackInfo<v8::Value>& info) {
  ConsoleHelper helper(info, m_inspector);
  if (helper.firstArgToBoolean(false)) return;

  std::vector<v8::Local<v8::Value>> arguments;
  for (int i = 1; i < info.Length(); ++i) arguments.push_back(info[i]);
  if (info.Length() < 2)
    arguments.push_back(
        toV8String(info.GetIsolate(), String16("console.assert")));
  helper.reportCall(ConsoleAPIType::kAssert, arguments);

  if (V8DebuggerAgentImpl* debuggerAgent = helper.debuggerAgent())
    debuggerAgent->breakProgramOnException(
        protocol::Debugger::Paused::ReasonEnum::Assert, nullptr);
}

void V8Console::markTimelineCallback(
    const v8::FunctionCallbackInfo<v8::Value>& info) {
  ConsoleHelper(info, m_inspector)
      .reportDeprecatedCall("V8Console#markTimelineDeprecated",
                            "'console.markTimeline' is "
                            "deprecated. Please use "
                            "'console.timeStamp' instead.");
  timeStampCallback(info);
}

void V8Console::profileCallback(
    const v8::FunctionCallbackInfo<v8::Value>& info) {
  ConsoleHelper helper(info, m_inspector);
  if (V8ProfilerAgentImpl* profilerAgent = helper.profilerAgent())
    profilerAgent->consoleProfile(helper.firstArgToString(String16()));
}

void V8Console::profileEndCallback(
    const v8::FunctionCallbackInfo<v8::Value>& info) {
  ConsoleHelper helper(info, m_inspector);
  if (V8ProfilerAgentImpl* profilerAgent = helper.profilerAgent())
    profilerAgent->consoleProfileEnd(helper.firstArgToString(String16()));
}

static void timeFunction(const v8::FunctionCallbackInfo<v8::Value>& info,
                         bool timelinePrefix, V8InspectorImpl* inspector) {
  ConsoleHelper helper(info, inspector);
  String16 protocolTitle = helper.firstArgToString("default");
  if (timelinePrefix) protocolTitle = "Timeline '" + protocolTitle + "'";
  inspector->client()->consoleTime(toStringView(protocolTitle));
  helper.consoleMessageStorage()->time(helper.contextId(), protocolTitle);
}

static void timeEndFunction(const v8::FunctionCallbackInfo<v8::Value>& info,
                            bool timelinePrefix, V8InspectorImpl* inspector) {
  ConsoleHelper helper(info, inspector);
  String16 protocolTitle = helper.firstArgToString("default");
  if (timelinePrefix) protocolTitle = "Timeline '" + protocolTitle + "'";
  inspector->client()->consoleTimeEnd(toStringView(protocolTitle));
  double elapsed = helper.consoleMessageStorage()->timeEnd(helper.contextId(),
                                                           protocolTitle);
  String16 message =
      protocolTitle + ": " + String16::fromDouble(elapsed) + "ms";
  helper.reportCallWithArgument(ConsoleAPIType::kTimeEnd, message);
}

void V8Console::timelineCallback(
    const v8::FunctionCallbackInfo<v8::Value>& info) {
  ConsoleHelper(info, m_inspector)
      .reportDeprecatedCall("V8Console#timeline",
                            "'console.timeline' is deprecated. Please use "
                            "'console.time' instead.");
  timeFunction(info, true, m_inspector);
}

void V8Console::timelineEndCallback(
    const v8::FunctionCallbackInfo<v8::Value>& info) {
  ConsoleHelper(info, m_inspector)
      .reportDeprecatedCall("V8Console#timelineEnd",
                            "'console.timelineEnd' is "
                            "deprecated. Please use "
                            "'console.timeEnd' instead.");
  timeEndFunction(info, true, m_inspector);
}

void V8Console::timeCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  timeFunction(info, false, m_inspector);
}

void V8Console::timeEndCallback(
    const v8::FunctionCallbackInfo<v8::Value>& info) {
  timeEndFunction(info, false, m_inspector);
}

void V8Console::timeStampCallback(
    const v8::FunctionCallbackInfo<v8::Value>& info) {
  ConsoleHelper helper(info, m_inspector);
  String16 title = helper.firstArgToString(String16());
  m_inspector->client()->consoleTimeStamp(toStringView(title));
}

void V8Console::memoryGetterCallback(
    const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Value> memoryValue;
  if (!m_inspector->client()
           ->memoryInfo(info.GetIsolate(),
                        info.GetIsolate()->GetCurrentContext())
           .ToLocal(&memoryValue))
    return;
  info.GetReturnValue().Set(memoryValue);
}

void V8Console::memorySetterCallback(
    const v8::FunctionCallbackInfo<v8::Value>& info) {
  // We can't make the attribute readonly as it breaks existing code that relies
  // on being able to assign to console.memory in strict mode. Instead, the
  // setter just ignores the passed value.  http://crbug.com/468611
}

void V8Console::keysCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  info.GetReturnValue().Set(v8::Array::New(isolate));

  ConsoleHelper helper(info, m_inspector);
  v8::Local<v8::Object> obj;
  if (!helper.firstArgAsObject().ToLocal(&obj)) return;
  v8::Local<v8::Array> names;
  if (!obj->GetOwnPropertyNames(isolate->GetCurrentContext()).ToLocal(&names))
    return;
  info.GetReturnValue().Set(names);
}

void V8Console::valuesCallback(
    const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  info.GetReturnValue().Set(v8::Array::New(isolate));

  ConsoleHelper helper(info, m_inspector);
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

static void setFunctionBreakpoint(ConsoleHelper& helper,
                                  v8::Local<v8::Function> function,
                                  V8DebuggerAgentImpl::BreakpointSource source,
                                  const String16& condition, bool enable) {
  V8DebuggerAgentImpl* debuggerAgent = helper.debuggerAgent();
  if (!debuggerAgent) return;
  String16 scriptId = String16::fromInteger(function->ScriptId());
  int lineNumber = function->GetScriptLineNumber();
  int columnNumber = function->GetScriptColumnNumber();
  if (lineNumber == v8::Function::kLineOffsetNotFound ||
      columnNumber == v8::Function::kLineOffsetNotFound)
    return;
  if (enable)
    debuggerAgent->setBreakpointAt(scriptId, lineNumber, columnNumber, source,
                                   condition);
  else
    debuggerAgent->removeBreakpointAt(scriptId, lineNumber, columnNumber,
                                      source);
}

void V8Console::debugFunctionCallback(
    const v8::FunctionCallbackInfo<v8::Value>& info) {
  ConsoleHelper helper(info, m_inspector);
  v8::Local<v8::Function> function;
  if (!helper.firstArgAsFunction().ToLocal(&function)) return;
  setFunctionBreakpoint(helper, function,
                        V8DebuggerAgentImpl::DebugCommandBreakpointSource,
                        String16(), true);
}

void V8Console::undebugFunctionCallback(
    const v8::FunctionCallbackInfo<v8::Value>& info) {
  ConsoleHelper helper(info, m_inspector);
  v8::Local<v8::Function> function;
  if (!helper.firstArgAsFunction().ToLocal(&function)) return;
  setFunctionBreakpoint(helper, function,
                        V8DebuggerAgentImpl::DebugCommandBreakpointSource,
                        String16(), false);
}

void V8Console::monitorFunctionCallback(
    const v8::FunctionCallbackInfo<v8::Value>& info) {
  ConsoleHelper helper(info, m_inspector);
  v8::Local<v8::Function> function;
  if (!helper.firstArgAsFunction().ToLocal(&function)) return;
  v8::Local<v8::Value> name = function->GetName();
  if (!name->IsString() || !v8::Local<v8::String>::Cast(name)->Length())
    name = function->GetInferredName();
  String16 functionName = toProtocolStringWithTypeCheck(name);
  String16Builder builder;
  builder.append("console.log(\"function ");
  if (functionName.isEmpty())
    builder.append("(anonymous function)");
  else
    builder.append(functionName);
  builder.append(
      " called\" + (arguments.length > 0 ? \" with arguments: \" + "
      "Array.prototype.join.call(arguments, \", \") : \"\")) && false");
  setFunctionBreakpoint(helper, function,
                        V8DebuggerAgentImpl::MonitorCommandBreakpointSource,
                        builder.toString(), true);
}

void V8Console::unmonitorFunctionCallback(
    const v8::FunctionCallbackInfo<v8::Value>& info) {
  ConsoleHelper helper(info, m_inspector);
  v8::Local<v8::Function> function;
  if (!helper.firstArgAsFunction().ToLocal(&function)) return;
  setFunctionBreakpoint(helper, function,
                        V8DebuggerAgentImpl::MonitorCommandBreakpointSource,
                        String16(), false);
}

void V8Console::lastEvaluationResultCallback(
    const v8::FunctionCallbackInfo<v8::Value>& info) {
  ConsoleHelper helper(info, m_inspector);
  InjectedScript* injectedScript = helper.injectedScript();
  if (!injectedScript) return;
  info.GetReturnValue().Set(injectedScript->lastEvaluationResult());
}

static void inspectImpl(const v8::FunctionCallbackInfo<v8::Value>& info,
                        bool copyToClipboard, V8InspectorImpl* inspector) {
  if (info.Length() < 1) return;
  if (!copyToClipboard) info.GetReturnValue().Set(info[0]);

  ConsoleHelper helper(info, inspector);
  InjectedScript* injectedScript = helper.injectedScript();
  if (!injectedScript) return;
  std::unique_ptr<protocol::Runtime::RemoteObject> wrappedObject;
  protocol::Response response =
      injectedScript->wrapObject(info[0], "", false /** forceValueType */,
                                 false /** generatePreview */, &wrappedObject);
  if (!response.isSuccess()) return;

  std::unique_ptr<protocol::DictionaryValue> hints =
      protocol::DictionaryValue::create();
  if (copyToClipboard) hints->setBoolean("copyToClipboard", true);
  if (V8InspectorSessionImpl* session = helper.currentSession()) {
    session->runtimeAgent()->inspect(std::move(wrappedObject),
                                     std::move(hints));
  }
}

void V8Console::inspectCallback(
    const v8::FunctionCallbackInfo<v8::Value>& info) {
  inspectImpl(info, false, m_inspector);
}

void V8Console::copyCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  inspectImpl(info, true, m_inspector);
}

void V8Console::inspectedObject(const v8::FunctionCallbackInfo<v8::Value>& info,
                                unsigned num) {
  DCHECK(num < V8InspectorSessionImpl::kInspectedObjectBufferSize);
  ConsoleHelper helper(info, m_inspector);
  if (V8InspectorSessionImpl* session = helper.currentSession()) {
    V8InspectorSession::Inspectable* object = session->inspectedObject(num);
    v8::Isolate* isolate = info.GetIsolate();
    if (object)
      info.GetReturnValue().Set(object->get(isolate->GetCurrentContext()));
    else
      info.GetReturnValue().Set(v8::Undefined(isolate));
  }
}

v8::Local<v8::Object> V8Console::createConsole(v8::Local<v8::Context> context) {
  v8::Context::Scope contextScope(context);
  v8::Isolate* isolate = context->GetIsolate();
  v8::MicrotasksScope microtasksScope(isolate,
                                      v8::MicrotasksScope::kDoNotRunMicrotasks);

  v8::Local<v8::Object> console = v8::Object::New(isolate);
  bool success =
      console->SetPrototype(context, v8::Object::New(isolate)).FromMaybe(false);
  DCHECK(success);
  USE(success);

  v8::Local<v8::External> data = v8::External::New(isolate, this);
  createBoundFunctionProperty(context, console, data, "debug",
                              &V8Console::call<&V8Console::debugCallback>);
  createBoundFunctionProperty(context, console, data, "error",
                              &V8Console::call<&V8Console::errorCallback>);
  createBoundFunctionProperty(context, console, data, "info",
                              &V8Console::call<&V8Console::infoCallback>);
  createBoundFunctionProperty(context, console, data, "log",
                              &V8Console::call<&V8Console::logCallback>);
  createBoundFunctionProperty(context, console, data, "warn",
                              &V8Console::call<&V8Console::warnCallback>);
  createBoundFunctionProperty(context, console, data, "dir",
                              &V8Console::call<&V8Console::dirCallback>);
  createBoundFunctionProperty(context, console, data, "dirxml",
                              &V8Console::call<&V8Console::dirxmlCallback>);
  createBoundFunctionProperty(context, console, data, "table",
                              &V8Console::call<&V8Console::tableCallback>);
  createBoundFunctionProperty(context, console, data, "trace",
                              &V8Console::call<&V8Console::traceCallback>);
  createBoundFunctionProperty(context, console, data, "group",
                              &V8Console::call<&V8Console::groupCallback>);
  createBoundFunctionProperty(
      context, console, data, "groupCollapsed",
      &V8Console::call<&V8Console::groupCollapsedCallback>);
  createBoundFunctionProperty(context, console, data, "groupEnd",
                              &V8Console::call<&V8Console::groupEndCallback>);
  createBoundFunctionProperty(context, console, data, "clear",
                              &V8Console::call<&V8Console::clearCallback>);
  createBoundFunctionProperty(context, console, data, "count",
                              &V8Console::call<&V8Console::countCallback>);
  createBoundFunctionProperty(context, console, data, "assert",
                              &V8Console::call<&V8Console::assertCallback>);
  createBoundFunctionProperty(
      context, console, data, "markTimeline",
      &V8Console::call<&V8Console::markTimelineCallback>);
  createBoundFunctionProperty(context, console, data, "profile",
                              &V8Console::call<&V8Console::profileCallback>);
  createBoundFunctionProperty(context, console, data, "profileEnd",
                              &V8Console::call<&V8Console::profileEndCallback>);
  createBoundFunctionProperty(context, console, data, "timeline",
                              &V8Console::call<&V8Console::timelineCallback>);
  createBoundFunctionProperty(
      context, console, data, "timelineEnd",
      &V8Console::call<&V8Console::timelineEndCallback>);
  createBoundFunctionProperty(context, console, data, "time",
                              &V8Console::call<&V8Console::timeCallback>);
  createBoundFunctionProperty(context, console, data, "timeEnd",
                              &V8Console::call<&V8Console::timeEndCallback>);
  createBoundFunctionProperty(context, console, data, "timeStamp",
                              &V8Console::call<&V8Console::timeStampCallback>);
  return console;
}

void V8Console::installMemoryGetter(v8::Local<v8::Context> context,
                                    v8::Local<v8::Object> console) {
  v8::Isolate* isolate = context->GetIsolate();
  v8::Local<v8::External> data = v8::External::New(isolate, this);
  console->SetAccessorProperty(
      toV8StringInternalized(isolate, "memory"),
      v8::Function::New(context,
                        &V8Console::call<&V8Console::memoryGetterCallback>,
                        data, 0, v8::ConstructorBehavior::kThrow)
          .ToLocalChecked(),
      v8::Function::New(context,
                        &V8Console::call<&V8Console::memorySetterCallback>,
                        data, 0, v8::ConstructorBehavior::kThrow)
          .ToLocalChecked(),
      static_cast<v8::PropertyAttribute>(v8::None), v8::DEFAULT);
}

v8::Local<v8::Object> V8Console::createCommandLineAPI(
    v8::Local<v8::Context> context) {
  v8::Isolate* isolate = context->GetIsolate();
  v8::MicrotasksScope microtasksScope(isolate,
                                      v8::MicrotasksScope::kDoNotRunMicrotasks);

  v8::Local<v8::Object> commandLineAPI = v8::Object::New(isolate);
  bool success =
      commandLineAPI->SetPrototype(context, v8::Null(isolate)).FromMaybe(false);
  DCHECK(success);
  USE(success);

  v8::Local<v8::External> data = v8::External::New(isolate, this);
  createBoundFunctionProperty(context, commandLineAPI, data, "dir",
                              &V8Console::call<&V8Console::dirCallback>,
                              "function dir(value) { [Command Line API] }");
  createBoundFunctionProperty(context, commandLineAPI, data, "dirxml",
                              &V8Console::call<&V8Console::dirxmlCallback>,
                              "function dirxml(value) { [Command Line API] }");
  createBoundFunctionProperty(context, commandLineAPI, data, "profile",
                              &V8Console::call<&V8Console::profileCallback>,
                              "function profile(title) { [Command Line API] }");
  createBoundFunctionProperty(
      context, commandLineAPI, data, "profileEnd",
      &V8Console::call<&V8Console::profileEndCallback>,
      "function profileEnd(title) { [Command Line API] }");
  createBoundFunctionProperty(context, commandLineAPI, data, "clear",
                              &V8Console::call<&V8Console::clearCallback>,
                              "function clear() { [Command Line API] }");
  createBoundFunctionProperty(
      context, commandLineAPI, data, "table",
      &V8Console::call<&V8Console::tableCallback>,
      "function table(data, [columns]) { [Command Line API] }");

  createBoundFunctionProperty(context, commandLineAPI, data, "keys",
                              &V8Console::call<&V8Console::keysCallback>,
                              "function keys(object) { [Command Line API] }");
  createBoundFunctionProperty(context, commandLineAPI, data, "values",
                              &V8Console::call<&V8Console::valuesCallback>,
                              "function values(object) { [Command Line API] }");
  createBoundFunctionProperty(
      context, commandLineAPI, data, "debug",
      &V8Console::call<&V8Console::debugFunctionCallback>,
      "function debug(function) { [Command Line API] }");
  createBoundFunctionProperty(
      context, commandLineAPI, data, "undebug",
      &V8Console::call<&V8Console::undebugFunctionCallback>,
      "function undebug(function) { [Command Line API] }");
  createBoundFunctionProperty(
      context, commandLineAPI, data, "monitor",
      &V8Console::call<&V8Console::monitorFunctionCallback>,
      "function monitor(function) { [Command Line API] }");
  createBoundFunctionProperty(
      context, commandLineAPI, data, "unmonitor",
      &V8Console::call<&V8Console::unmonitorFunctionCallback>,
      "function unmonitor(function) { [Command Line API] }");
  createBoundFunctionProperty(
      context, commandLineAPI, data, "inspect",
      &V8Console::call<&V8Console::inspectCallback>,
      "function inspect(object) { [Command Line API] }");
  createBoundFunctionProperty(context, commandLineAPI, data, "copy",
                              &V8Console::call<&V8Console::copyCallback>,
                              "function copy(value) { [Command Line API] }");
  createBoundFunctionProperty(
      context, commandLineAPI, data, "$_",
      &V8Console::call<&V8Console::lastEvaluationResultCallback>);
  createBoundFunctionProperty(context, commandLineAPI, data, "$0",
                              &V8Console::call<&V8Console::inspectedObject0>);
  createBoundFunctionProperty(context, commandLineAPI, data, "$1",
                              &V8Console::call<&V8Console::inspectedObject1>);
  createBoundFunctionProperty(context, commandLineAPI, data, "$2",
                              &V8Console::call<&V8Console::inspectedObject2>);
  createBoundFunctionProperty(context, commandLineAPI, data, "$3",
                              &V8Console::call<&V8Console::inspectedObject3>);
  createBoundFunctionProperty(context, commandLineAPI, data, "$4",
                              &V8Console::call<&V8Console::inspectedObject4>);

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

void V8Console::CommandLineAPIScope::accessorGetterCallback(
    v8::Local<v8::Name> name, const v8::PropertyCallbackInfo<v8::Value>& info) {
  CommandLineAPIScope* scope = static_cast<CommandLineAPIScope*>(
      info.Data().As<v8::External>()->Value());
  DCHECK(scope);

  v8::Local<v8::Context> context = info.GetIsolate()->GetCurrentContext();
  if (scope->m_cleanup) {
    bool removed = info.Holder()->Delete(context, name).FromMaybe(false);
    DCHECK(removed);
    USE(removed);
    return;
  }
  v8::Local<v8::Object> commandLineAPI = scope->m_commandLineAPI;

  v8::Local<v8::Value> value;
  if (!commandLineAPI->Get(context, name).ToLocal(&value)) return;
  if (isCommandLineAPIGetter(toProtocolStringWithTypeCheck(name))) {
    DCHECK(value->IsFunction());
    v8::MicrotasksScope microtasks(info.GetIsolate(),
                                   v8::MicrotasksScope::kDoNotRunMicrotasks);
    if (value.As<v8::Function>()
            ->Call(context, commandLineAPI, 0, nullptr)
            .ToLocal(&value))
      info.GetReturnValue().Set(value);
  } else {
    info.GetReturnValue().Set(value);
  }
}

void V8Console::CommandLineAPIScope::accessorSetterCallback(
    v8::Local<v8::Name> name, v8::Local<v8::Value> value,
    const v8::PropertyCallbackInfo<void>& info) {
  CommandLineAPIScope* scope = static_cast<CommandLineAPIScope*>(
      info.Data().As<v8::External>()->Value());
  v8::Local<v8::Context> context = info.GetIsolate()->GetCurrentContext();
  if (!info.Holder()->Delete(context, name).FromMaybe(false)) return;
  if (!info.Holder()->CreateDataProperty(context, name, value).FromMaybe(false))
    return;
  bool removed =
      scope->m_installedMethods->Delete(context, name).FromMaybe(false);
  DCHECK(removed);
  USE(removed);
}

V8Console::CommandLineAPIScope::CommandLineAPIScope(
    v8::Local<v8::Context> context, v8::Local<v8::Object> commandLineAPI,
    v8::Local<v8::Object> global)
    : m_context(context),
      m_commandLineAPI(commandLineAPI),
      m_global(global),
      m_installedMethods(v8::Set::New(context->GetIsolate())),
      m_cleanup(false) {
  v8::Local<v8::Array> names;
  if (!m_commandLineAPI->GetOwnPropertyNames(context).ToLocal(&names)) return;
  v8::Local<v8::External> externalThis =
      v8::External::New(context->GetIsolate(), this);
  for (uint32_t i = 0; i < names->Length(); ++i) {
    v8::Local<v8::Value> name;
    if (!names->Get(context, i).ToLocal(&name) || !name->IsName()) continue;
    if (m_global->Has(context, name).FromMaybe(true)) continue;
    if (!m_installedMethods->Add(context, name).ToLocal(&m_installedMethods))
      continue;
    if (!m_global
             ->SetAccessor(context, v8::Local<v8::Name>::Cast(name),
                           CommandLineAPIScope::accessorGetterCallback,
                           CommandLineAPIScope::accessorSetterCallback,
                           externalThis, v8::DEFAULT, v8::DontEnum)
             .FromMaybe(false)) {
      bool removed = m_installedMethods->Delete(context, name).FromMaybe(false);
      DCHECK(removed);
      USE(removed);
      continue;
    }
  }
}

V8Console::CommandLineAPIScope::~CommandLineAPIScope() {
  m_cleanup = true;
  v8::Local<v8::Array> names = m_installedMethods->AsArray();
  for (uint32_t i = 0; i < names->Length(); ++i) {
    v8::Local<v8::Value> name;
    if (!names->Get(m_context, i).ToLocal(&name) || !name->IsName()) continue;
    if (name->IsString()) {
      v8::Local<v8::Value> descriptor;
      bool success = m_global
                         ->GetOwnPropertyDescriptor(
                             m_context, v8::Local<v8::String>::Cast(name))
                         .ToLocal(&descriptor);
      DCHECK(success);
      USE(success);
    }
  }
}

}  // namespace v8_inspector
