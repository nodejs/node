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
#include "src/inspector/v8-value-utils.h"

#include "include/v8-inspector.h"

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
        m_isolate(inspector->isolate()),
        m_context(m_isolate->GetCurrentContext()),
        m_inspector(inspector),
        m_contextId(InspectedContext::contextId(m_context)),
        m_groupId(m_inspector->contextGroupId(m_contextId)) {}

  int contextId() const { return m_contextId; }
  int groupId() const { return m_groupId; }

  InjectedScript* injectedScript(int sessionId) {
    InspectedContext* context = m_inspector->getContext(m_groupId, m_contextId);
    if (!context) return nullptr;
    return context->getInjectedScript(sessionId);
  }

  V8InspectorSessionImpl* session(int sessionId) {
    return m_inspector->sessionById(m_groupId, sessionId);
  }

  V8ConsoleMessageStorage* consoleMessageStorage() {
    return m_inspector->ensureConsoleMessageStorage(m_groupId);
  }

  void reportCall(ConsoleAPIType type) {
    if (!m_info.Length()) return;
    std::vector<v8::Local<v8::Value>> arguments;
    arguments.reserve(m_info.Length());
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

  void reportCallAndReplaceFirstArgument(ConsoleAPIType type,
                                         const String16& message) {
    std::vector<v8::Local<v8::Value>> arguments;
    arguments.push_back(toV8String(m_isolate, message));
    for (int i = 1; i < m_info.Length(); ++i) arguments.push_back(m_info[i]);
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
            consoleContextToString(m_isolate, m_consoleContext),
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
    return m_info[0]->BooleanValue(m_context->GetIsolate());
  }

  String16 firstArgToString(const String16& defaultValue,
                            bool allowUndefined = true) {
    if (m_info.Length() < 1 || (!allowUndefined && m_info[0]->IsUndefined())) {
      return defaultValue;
    }
    v8::Local<v8::String> titleValue;
    v8::TryCatch tryCatch(m_context->GetIsolate());
    if (!m_info[0]->ToString(m_context).ToLocal(&titleValue))
      return defaultValue;
    return toProtocolString(m_context->GetIsolate(), titleValue);
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

  void forEachSession(std::function<void(V8InspectorSessionImpl*)> callback) {
    m_inspector->forEachSession(m_groupId, std::move(callback));
  }

 private:
  const v8::debug::ConsoleCallArguments& m_info;
  const v8::debug::ConsoleContext& m_consoleContext;
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

void createBoundFunctionProperty(
    v8::Local<v8::Context> context, v8::Local<v8::Object> console,
    v8::Local<v8::Value> data, const char* name, v8::FunctionCallback callback,
    const char* description = nullptr,
    v8::SideEffectType side_effect_type = v8::SideEffectType::kHasSideEffect) {
  v8::Local<v8::String> funcName =
      toV8StringInternalized(context->GetIsolate(), name);
  v8::Local<v8::Function> func;
  if (!v8::Function::New(context, callback, data, 0,
                         v8::ConstructorBehavior::kThrow, side_effect_type)
           .ToLocal(&func))
    return;
  func->SetName(funcName);
  if (description) {
    v8::Local<v8::String> returnValue =
        toV8String(context->GetIsolate(), description);
    v8::Local<v8::Function> toStringFunction;
    if (v8::Function::New(context, returnDataCallback, returnValue, 0,
                          v8::ConstructorBehavior::kThrow,
                          v8::SideEffectType::kHasNoSideEffect)
            .ToLocal(&toStringFunction))
      createDataProperty(context, func, toV8StringInternalized(
                                            context->GetIsolate(), "toString"),
                         toStringFunction);
  }
  createDataProperty(context, console, funcName, func);
}

enum InspectRequest { kRegular, kCopyToClipboard, kQueryObjects };

}  // namespace

V8Console::V8Console(V8InspectorImpl* inspector) : m_inspector(inspector) {}

void V8Console::Debug(const v8::debug::ConsoleCallArguments& info,
                      const v8::debug::ConsoleContext& consoleContext) {
  ConsoleHelper(info, consoleContext, m_inspector)
      .reportCall(ConsoleAPIType::kDebug);
}

void V8Console::Error(const v8::debug::ConsoleCallArguments& info,
                      const v8::debug::ConsoleContext& consoleContext) {
  ConsoleHelper(info, consoleContext, m_inspector)
      .reportCall(ConsoleAPIType::kError);
}

void V8Console::Info(const v8::debug::ConsoleCallArguments& info,
                     const v8::debug::ConsoleContext& consoleContext) {
  ConsoleHelper(info, consoleContext, m_inspector)
      .reportCall(ConsoleAPIType::kInfo);
}

void V8Console::Log(const v8::debug::ConsoleCallArguments& info,
                    const v8::debug::ConsoleContext& consoleContext) {
  ConsoleHelper(info, consoleContext, m_inspector)
      .reportCall(ConsoleAPIType::kLog);
}

void V8Console::Warn(const v8::debug::ConsoleCallArguments& info,
                     const v8::debug::ConsoleContext& consoleContext) {
  ConsoleHelper(info, consoleContext, m_inspector)
      .reportCall(ConsoleAPIType::kWarning);
}

void V8Console::Dir(const v8::debug::ConsoleCallArguments& info,
                    const v8::debug::ConsoleContext& consoleContext) {
  ConsoleHelper(info, consoleContext, m_inspector)
      .reportCall(ConsoleAPIType::kDir);
}

void V8Console::DirXml(const v8::debug::ConsoleCallArguments& info,
                       const v8::debug::ConsoleContext& consoleContext) {
  ConsoleHelper(info, consoleContext, m_inspector)
      .reportCall(ConsoleAPIType::kDirXML);
}

void V8Console::Table(const v8::debug::ConsoleCallArguments& info,
                      const v8::debug::ConsoleContext& consoleContext) {
  ConsoleHelper(info, consoleContext, m_inspector)
      .reportCall(ConsoleAPIType::kTable);
}

void V8Console::Trace(const v8::debug::ConsoleCallArguments& info,
                      const v8::debug::ConsoleContext& consoleContext) {
  ConsoleHelper(info, consoleContext, m_inspector)
      .reportCallWithDefaultArgument(ConsoleAPIType::kTrace,
                                     String16("console.trace"));
}

void V8Console::Group(const v8::debug::ConsoleCallArguments& info,
                      const v8::debug::ConsoleContext& consoleContext) {
  ConsoleHelper(info, consoleContext, m_inspector)
      .reportCallWithDefaultArgument(ConsoleAPIType::kStartGroup,
                                     String16("console.group"));
}

void V8Console::GroupCollapsed(
    const v8::debug::ConsoleCallArguments& info,
    const v8::debug::ConsoleContext& consoleContext) {
  ConsoleHelper(info, consoleContext, m_inspector)
      .reportCallWithDefaultArgument(ConsoleAPIType::kStartGroupCollapsed,
                                     String16("console.groupCollapsed"));
}

void V8Console::GroupEnd(const v8::debug::ConsoleCallArguments& info,
                         const v8::debug::ConsoleContext& consoleContext) {
  ConsoleHelper(info, consoleContext, m_inspector)
      .reportCallWithDefaultArgument(ConsoleAPIType::kEndGroup,
                                     String16("console.groupEnd"));
}

void V8Console::Clear(const v8::debug::ConsoleCallArguments& info,
                      const v8::debug::ConsoleContext& consoleContext) {
  ConsoleHelper helper(info, consoleContext, m_inspector);
  if (!helper.groupId()) return;
  m_inspector->client()->consoleClear(helper.groupId());
  helper.reportCallWithDefaultArgument(ConsoleAPIType::kClear,
                                       String16("console.clear"));
}

static String16 identifierFromTitleOrStackTrace(
    const String16& title, const ConsoleHelper& helper,
    const v8::debug::ConsoleContext& consoleContext,
    V8InspectorImpl* inspector) {
  String16 identifier;
  if (title.isEmpty()) {
    std::unique_ptr<V8StackTraceImpl> stackTrace =
        V8StackTraceImpl::capture(inspector->debugger(), helper.groupId(), 1);
    if (stackTrace && !stackTrace->isEmpty()) {
      identifier = toString16(stackTrace->topSourceURL()) + ":" +
                   String16::fromInteger(stackTrace->topLineNumber());
    }
  } else {
    identifier = title + "@";
  }
  identifier = consoleContextToString(inspector->isolate(), consoleContext) +
               "@" + identifier;

  return identifier;
}

void V8Console::Count(const v8::debug::ConsoleCallArguments& info,
                      const v8::debug::ConsoleContext& consoleContext) {
  ConsoleHelper helper(info, consoleContext, m_inspector);
  String16 title = helper.firstArgToString(String16("default"), false);
  String16 identifier = identifierFromTitleOrStackTrace(
      title, helper, consoleContext, m_inspector);

  int count =
      helper.consoleMessageStorage()->count(helper.contextId(), identifier);
  String16 countString = String16::fromInteger(count);
  helper.reportCallWithArgument(
      ConsoleAPIType::kCount,
      title.isEmpty() ? countString : (title + ": " + countString));
}

void V8Console::CountReset(const v8::debug::ConsoleCallArguments& info,
                           const v8::debug::ConsoleContext& consoleContext) {
  ConsoleHelper helper(info, consoleContext, m_inspector);
  String16 title = helper.firstArgToString(String16("default"), false);
  String16 identifier = identifierFromTitleOrStackTrace(
      title, helper, consoleContext, m_inspector);

  if (!helper.consoleMessageStorage()->countReset(helper.contextId(),
                                                  identifier)) {
    helper.reportCallWithArgument(ConsoleAPIType::kWarning,
                                  "Count for '" + title + "' does not exist");
  }
}

void V8Console::Assert(const v8::debug::ConsoleCallArguments& info,
                       const v8::debug::ConsoleContext& consoleContext) {
  ConsoleHelper helper(info, consoleContext, m_inspector);
  DCHECK(!helper.firstArgToBoolean(false));

  std::vector<v8::Local<v8::Value>> arguments;
  for (int i = 1; i < info.Length(); ++i) arguments.push_back(info[i]);
  if (info.Length() < 2)
    arguments.push_back(
        toV8String(m_inspector->isolate(), String16("console.assert")));
  helper.reportCall(ConsoleAPIType::kAssert, arguments);
  m_inspector->debugger()->breakProgramOnAssert(helper.groupId());
}

void V8Console::Profile(const v8::debug::ConsoleCallArguments& info,
                        const v8::debug::ConsoleContext& consoleContext) {
  ConsoleHelper helper(info, consoleContext, m_inspector);
  helper.forEachSession([&helper](V8InspectorSessionImpl* session) {
    session->profilerAgent()->consoleProfile(
        helper.firstArgToString(String16()));
  });
}

void V8Console::ProfileEnd(const v8::debug::ConsoleCallArguments& info,
                           const v8::debug::ConsoleContext& consoleContext) {
  ConsoleHelper helper(info, consoleContext, m_inspector);
  helper.forEachSession([&helper](V8InspectorSessionImpl* session) {
    session->profilerAgent()->consoleProfileEnd(
        helper.firstArgToString(String16()));
  });
}

static void timeFunction(const v8::debug::ConsoleCallArguments& info,
                         const v8::debug::ConsoleContext& consoleContext,
                         bool timelinePrefix, V8InspectorImpl* inspector) {
  ConsoleHelper helper(info, consoleContext, inspector);
  String16 protocolTitle = helper.firstArgToString("default", false);
  if (timelinePrefix) protocolTitle = "Timeline '" + protocolTitle + "'";
  const String16& timerId =
      protocolTitle + "@" +
      consoleContextToString(inspector->isolate(), consoleContext);
  if (helper.consoleMessageStorage()->hasTimer(helper.contextId(), timerId)) {
    helper.reportCallWithArgument(
        ConsoleAPIType::kWarning,
        "Timer '" + protocolTitle + "' already exists");
    return;
  }
  inspector->client()->consoleTime(toStringView(protocolTitle));
  helper.consoleMessageStorage()->time(helper.contextId(), timerId);
}

static void timeEndFunction(const v8::debug::ConsoleCallArguments& info,
                            const v8::debug::ConsoleContext& consoleContext,
                            bool timeLog, V8InspectorImpl* inspector) {
  ConsoleHelper helper(info, consoleContext, inspector);
  String16 protocolTitle = helper.firstArgToString("default", false);
  const String16& timerId =
      protocolTitle + "@" +
      consoleContextToString(inspector->isolate(), consoleContext);
  if (!helper.consoleMessageStorage()->hasTimer(helper.contextId(), timerId)) {
    helper.reportCallWithArgument(
        ConsoleAPIType::kWarning,
        "Timer '" + protocolTitle + "' does not exist");
    return;
  }
  inspector->client()->consoleTimeEnd(toStringView(protocolTitle));
  String16 title = protocolTitle + "@" +
                   consoleContextToString(inspector->isolate(), consoleContext);
  double elapsed;
  if (timeLog) {
    elapsed =
        helper.consoleMessageStorage()->timeLog(helper.contextId(), title);
  } else {
    elapsed =
        helper.consoleMessageStorage()->timeEnd(helper.contextId(), title);
  }
  String16 message =
      protocolTitle + ": " + String16::fromDouble(elapsed) + "ms";
  if (timeLog)
    helper.reportCallAndReplaceFirstArgument(ConsoleAPIType::kLog, message);
  else
    helper.reportCallWithArgument(ConsoleAPIType::kTimeEnd, message);
}

void V8Console::Time(const v8::debug::ConsoleCallArguments& info,
                     const v8::debug::ConsoleContext& consoleContext) {
  timeFunction(info, consoleContext, false, m_inspector);
}

void V8Console::TimeLog(const v8::debug::ConsoleCallArguments& info,
                        const v8::debug::ConsoleContext& consoleContext) {
  timeEndFunction(info, consoleContext, true, m_inspector);
}

void V8Console::TimeEnd(const v8::debug::ConsoleCallArguments& info,
                        const v8::debug::ConsoleContext& consoleContext) {
  timeEndFunction(info, consoleContext, false, m_inspector);
}

void V8Console::TimeStamp(const v8::debug::ConsoleCallArguments& info,
                          const v8::debug::ConsoleContext& consoleContext) {
  ConsoleHelper helper(info, consoleContext, m_inspector);
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

void V8Console::keysCallback(const v8::FunctionCallbackInfo<v8::Value>& info,
                             int sessionId) {
  v8::Isolate* isolate = info.GetIsolate();
  info.GetReturnValue().Set(v8::Array::New(isolate));

  v8::debug::ConsoleCallArguments args(info);
  ConsoleHelper helper(args, v8::debug::ConsoleContext(), m_inspector);
  v8::Local<v8::Object> obj;
  if (!helper.firstArgAsObject().ToLocal(&obj)) return;
  v8::Local<v8::Array> names;
  if (!obj->GetOwnPropertyNames(isolate->GetCurrentContext()).ToLocal(&names))
    return;
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
  if (!name->IsString() || !v8::Local<v8::String>::Cast(name)->Length())
    name = function->GetInferredName();
  String16 functionName =
      toProtocolStringWithTypeCheck(info.GetIsolate(), name);
  String16Builder builder;
  builder.append("console.log(\"function ");
  if (functionName.isEmpty())
    builder.append("(anonymous function)");
  else
    builder.append(functionName);
  builder.append(
      " called\" + (arguments.length > 0 ? \" with arguments: \" + "
      "Array.prototype.join.call(arguments, \", \") : \"\")) && false");
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
  InjectedScript* injectedScript = helper.injectedScript(sessionId);
  if (!injectedScript) return;
  info.GetReturnValue().Set(injectedScript->lastEvaluationResult());
}

static void inspectImpl(const v8::FunctionCallbackInfo<v8::Value>& info,
                        v8::Local<v8::Value> value, int sessionId,
                        InspectRequest request, V8InspectorImpl* inspector) {
  if (request == kRegular) info.GetReturnValue().Set(value);

  v8::debug::ConsoleCallArguments args(info);
  ConsoleHelper helper(args, v8::debug::ConsoleContext(), inspector);
  InjectedScript* injectedScript = helper.injectedScript(sessionId);
  if (!injectedScript) return;
  std::unique_ptr<protocol::Runtime::RemoteObject> wrappedObject;
  protocol::Response response = injectedScript->wrapObject(
      value, "", WrapMode::kNoPreview, &wrappedObject);
  if (!response.isSuccess()) return;

  std::unique_ptr<protocol::DictionaryValue> hints =
      protocol::DictionaryValue::create();
  if (request == kCopyToClipboard) {
    hints->setBoolean("copyToClipboard", true);
  } else if (request == kQueryObjects) {
    hints->setBoolean("queryObjects", true);
  }
  if (V8InspectorSessionImpl* session = helper.session(sessionId)) {
    session->runtimeAgent()->inspect(std::move(wrappedObject),
                                     std::move(hints));
  }
}

void V8Console::inspectCallback(const v8::FunctionCallbackInfo<v8::Value>& info,
                                int sessionId) {
  if (info.Length() < 1) return;
  inspectImpl(info, info[0], sessionId, kRegular, m_inspector);
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
    if (object)
      info.GetReturnValue().Set(object->get(isolate->GetCurrentContext()));
    else
      info.GetReturnValue().Set(v8::Undefined(isolate));
  }
}

void V8Console::installMemoryGetter(v8::Local<v8::Context> context,
                                    v8::Local<v8::Object> console) {
  v8::Isolate* isolate = context->GetIsolate();
  v8::Local<v8::External> data = v8::External::New(isolate, this);
  console->SetAccessorProperty(
      toV8StringInternalized(isolate, "memory"),
      v8::Function::New(
          context, &V8Console::call<&V8Console::memoryGetterCallback>, data, 0,
          v8::ConstructorBehavior::kThrow, v8::SideEffectType::kHasNoSideEffect)
          .ToLocalChecked(),
      v8::Function::New(context,
                        &V8Console::call<&V8Console::memorySetterCallback>,
                        data, 0, v8::ConstructorBehavior::kThrow)
          .ToLocalChecked(),
      static_cast<v8::PropertyAttribute>(v8::None), v8::DEFAULT);
}

v8::Local<v8::Object> V8Console::createCommandLineAPI(
    v8::Local<v8::Context> context, int sessionId) {
  v8::Isolate* isolate = context->GetIsolate();
  v8::MicrotasksScope microtasksScope(isolate,
                                      v8::MicrotasksScope::kDoNotRunMicrotasks);

  v8::Local<v8::Object> commandLineAPI = v8::Object::New(isolate);
  bool success =
      commandLineAPI->SetPrototype(context, v8::Null(isolate)).FromMaybe(false);
  DCHECK(success);
  USE(success);

  v8::Local<v8::ArrayBuffer> data =
      v8::ArrayBuffer::New(isolate, sizeof(CommandLineAPIData));
  *static_cast<CommandLineAPIData*>(data->GetContents().Data()) =
      CommandLineAPIData(this, sessionId);
  createBoundFunctionProperty(context, commandLineAPI, data, "dir",
                              &V8Console::call<&V8Console::Dir>,
                              "function dir(value) { [Command Line API] }");
  createBoundFunctionProperty(context, commandLineAPI, data, "dirxml",
                              &V8Console::call<&V8Console::DirXml>,
                              "function dirxml(value) { [Command Line API] }");
  createBoundFunctionProperty(context, commandLineAPI, data, "profile",
                              &V8Console::call<&V8Console::Profile>,
                              "function profile(title) { [Command Line API] }");
  createBoundFunctionProperty(
      context, commandLineAPI, data, "profileEnd",
      &V8Console::call<&V8Console::ProfileEnd>,
      "function profileEnd(title) { [Command Line API] }");
  createBoundFunctionProperty(context, commandLineAPI, data, "clear",
                              &V8Console::call<&V8Console::Clear>,
                              "function clear() { [Command Line API] }");
  createBoundFunctionProperty(
      context, commandLineAPI, data, "table",
      &V8Console::call<&V8Console::Table>,
      "function table(data, [columns]) { [Command Line API] }");

  createBoundFunctionProperty(context, commandLineAPI, data, "keys",
                              &V8Console::call<&V8Console::keysCallback>,
                              "function keys(object) { [Command Line API] }",
                              v8::SideEffectType::kHasNoSideEffect);
  createBoundFunctionProperty(context, commandLineAPI, data, "values",
                              &V8Console::call<&V8Console::valuesCallback>,
                              "function values(object) { [Command Line API] }",
                              v8::SideEffectType::kHasNoSideEffect);
  createBoundFunctionProperty(
      context, commandLineAPI, data, "debug",
      &V8Console::call<&V8Console::debugFunctionCallback>,
      "function debug(function, condition) { [Command Line API] }");
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
      context, commandLineAPI, data, "queryObjects",
      &V8Console::call<&V8Console::queryObjectsCallback>,
      "function queryObjects(constructor) { [Command Line API] }");
  createBoundFunctionProperty(
      context, commandLineAPI, data, "$_",
      &V8Console::call<&V8Console::lastEvaluationResultCallback>, nullptr,
      v8::SideEffectType::kHasNoSideEffect);
  createBoundFunctionProperty(context, commandLineAPI, data, "$0",
                              &V8Console::call<&V8Console::inspectedObject0>,
                              nullptr, v8::SideEffectType::kHasNoSideEffect);
  createBoundFunctionProperty(context, commandLineAPI, data, "$1",
                              &V8Console::call<&V8Console::inspectedObject1>,
                              nullptr, v8::SideEffectType::kHasNoSideEffect);
  createBoundFunctionProperty(context, commandLineAPI, data, "$2",
                              &V8Console::call<&V8Console::inspectedObject2>,
                              nullptr, v8::SideEffectType::kHasNoSideEffect);
  createBoundFunctionProperty(context, commandLineAPI, data, "$3",
                              &V8Console::call<&V8Console::inspectedObject3>,
                              nullptr, v8::SideEffectType::kHasNoSideEffect);
  createBoundFunctionProperty(context, commandLineAPI, data, "$4",
                              &V8Console::call<&V8Console::inspectedObject4>,
                              nullptr, v8::SideEffectType::kHasNoSideEffect);

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
  if (isCommandLineAPIGetter(
          toProtocolStringWithTypeCheck(info.GetIsolate(), name))) {
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
  v8::MicrotasksScope microtasksScope(context->GetIsolate(),
                                      v8::MicrotasksScope::kDoNotRunMicrotasks);
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
                           externalThis, v8::DEFAULT, v8::DontEnum,
                           v8::SideEffectType::kHasNoSideEffect)
             .FromMaybe(false)) {
      bool removed = m_installedMethods->Delete(context, name).FromMaybe(false);
      DCHECK(removed);
      USE(removed);
      continue;
    }
  }
}

V8Console::CommandLineAPIScope::~CommandLineAPIScope() {
  v8::MicrotasksScope microtasksScope(m_context->GetIsolate(),
                                      v8::MicrotasksScope::kDoNotRunMicrotasks);
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
