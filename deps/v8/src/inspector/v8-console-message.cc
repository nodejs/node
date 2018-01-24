// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/inspector/v8-console-message.h"

#include "src/debug/debug-interface.h"
#include "src/inspector/inspected-context.h"
#include "src/inspector/protocol/Protocol.h"
#include "src/inspector/string-util.h"
#include "src/inspector/v8-console-agent-impl.h"
#include "src/inspector/v8-inspector-impl.h"
#include "src/inspector/v8-inspector-session-impl.h"
#include "src/inspector/v8-runtime-agent-impl.h"
#include "src/inspector/v8-stack-trace-impl.h"

#include "include/v8-inspector.h"

namespace v8_inspector {

namespace {

String16 consoleAPITypeValue(ConsoleAPIType type) {
  switch (type) {
    case ConsoleAPIType::kLog:
      return protocol::Runtime::ConsoleAPICalled::TypeEnum::Log;
    case ConsoleAPIType::kDebug:
      return protocol::Runtime::ConsoleAPICalled::TypeEnum::Debug;
    case ConsoleAPIType::kInfo:
      return protocol::Runtime::ConsoleAPICalled::TypeEnum::Info;
    case ConsoleAPIType::kError:
      return protocol::Runtime::ConsoleAPICalled::TypeEnum::Error;
    case ConsoleAPIType::kWarning:
      return protocol::Runtime::ConsoleAPICalled::TypeEnum::Warning;
    case ConsoleAPIType::kClear:
      return protocol::Runtime::ConsoleAPICalled::TypeEnum::Clear;
    case ConsoleAPIType::kDir:
      return protocol::Runtime::ConsoleAPICalled::TypeEnum::Dir;
    case ConsoleAPIType::kDirXML:
      return protocol::Runtime::ConsoleAPICalled::TypeEnum::Dirxml;
    case ConsoleAPIType::kTable:
      return protocol::Runtime::ConsoleAPICalled::TypeEnum::Table;
    case ConsoleAPIType::kTrace:
      return protocol::Runtime::ConsoleAPICalled::TypeEnum::Trace;
    case ConsoleAPIType::kStartGroup:
      return protocol::Runtime::ConsoleAPICalled::TypeEnum::StartGroup;
    case ConsoleAPIType::kStartGroupCollapsed:
      return protocol::Runtime::ConsoleAPICalled::TypeEnum::StartGroupCollapsed;
    case ConsoleAPIType::kEndGroup:
      return protocol::Runtime::ConsoleAPICalled::TypeEnum::EndGroup;
    case ConsoleAPIType::kAssert:
      return protocol::Runtime::ConsoleAPICalled::TypeEnum::Assert;
    case ConsoleAPIType::kTimeEnd:
      return protocol::Runtime::ConsoleAPICalled::TypeEnum::TimeEnd;
    case ConsoleAPIType::kCount:
      return protocol::Runtime::ConsoleAPICalled::TypeEnum::Count;
  }
  return protocol::Runtime::ConsoleAPICalled::TypeEnum::Log;
}

const unsigned maxConsoleMessageCount = 1000;
const int maxConsoleMessageV8Size = 10 * 1024 * 1024;
const unsigned maxArrayItemsLimit = 10000;
const unsigned maxStackDepthLimit = 32;

class V8ValueStringBuilder {
 public:
  static String16 toString(v8::Local<v8::Value> value,
                           v8::Local<v8::Context> context) {
    V8ValueStringBuilder builder(context);
    if (!builder.append(value)) return String16();
    return builder.toString();
  }

 private:
  enum {
    IgnoreNull = 1 << 0,
    IgnoreUndefined = 1 << 1,
  };

  explicit V8ValueStringBuilder(v8::Local<v8::Context> context)
      : m_arrayLimit(maxArrayItemsLimit),
        m_isolate(context->GetIsolate()),
        m_tryCatch(context->GetIsolate()),
        m_context(context) {}

  bool append(v8::Local<v8::Value> value, unsigned ignoreOptions = 0) {
    if (value.IsEmpty()) return true;
    if ((ignoreOptions & IgnoreNull) && value->IsNull()) return true;
    if ((ignoreOptions & IgnoreUndefined) && value->IsUndefined()) return true;
    if (value->IsString()) return append(v8::Local<v8::String>::Cast(value));
    if (value->IsStringObject())
      return append(v8::Local<v8::StringObject>::Cast(value)->ValueOf());
    if (value->IsSymbol()) return append(v8::Local<v8::Symbol>::Cast(value));
    if (value->IsSymbolObject())
      return append(v8::Local<v8::SymbolObject>::Cast(value)->ValueOf());
    if (value->IsNumberObject()) {
      m_builder.append(String16::fromDouble(
          v8::Local<v8::NumberObject>::Cast(value)->ValueOf(), 6));
      return true;
    }
    if (value->IsBooleanObject()) {
      m_builder.append(v8::Local<v8::BooleanObject>::Cast(value)->ValueOf()
                           ? "true"
                           : "false");
      return true;
    }
    if (value->IsArray()) return append(v8::Local<v8::Array>::Cast(value));
    if (value->IsProxy()) {
      m_builder.append("[object Proxy]");
      return true;
    }
    if (value->IsObject() && !value->IsDate() && !value->IsFunction() &&
        !value->IsNativeError() && !value->IsRegExp()) {
      v8::Local<v8::Object> object = v8::Local<v8::Object>::Cast(value);
      v8::Local<v8::String> stringValue;
      if (object->ObjectProtoToString(m_isolate->GetCurrentContext())
              .ToLocal(&stringValue))
        return append(stringValue);
    }
    v8::Local<v8::String> stringValue;
    if (!value->ToString(m_isolate->GetCurrentContext()).ToLocal(&stringValue))
      return false;
    return append(stringValue);
  }

  bool append(v8::Local<v8::Array> array) {
    for (const auto& it : m_visitedArrays) {
      if (it == array) return true;
    }
    uint32_t length = array->Length();
    if (length > m_arrayLimit) return false;
    if (m_visitedArrays.size() > maxStackDepthLimit) return false;

    bool result = true;
    m_arrayLimit -= length;
    m_visitedArrays.push_back(array);
    for (uint32_t i = 0; i < length; ++i) {
      if (i) m_builder.append(',');
      v8::Local<v8::Value> value;
      if (!array->Get(m_context, i).ToLocal(&value)) continue;
      if (!append(value, IgnoreNull | IgnoreUndefined)) {
        result = false;
        break;
      }
    }
    m_visitedArrays.pop_back();
    return result;
  }

  bool append(v8::Local<v8::Symbol> symbol) {
    m_builder.append("Symbol(");
    bool result = append(symbol->Name(), IgnoreUndefined);
    m_builder.append(')');
    return result;
  }

  bool append(v8::Local<v8::String> string) {
    if (m_tryCatch.HasCaught()) return false;
    if (!string.IsEmpty()) m_builder.append(toProtocolString(string));
    return true;
  }

  String16 toString() {
    if (m_tryCatch.HasCaught()) return String16();
    return m_builder.toString();
  }

  uint32_t m_arrayLimit;
  v8::Isolate* m_isolate;
  String16Builder m_builder;
  std::vector<v8::Local<v8::Array>> m_visitedArrays;
  v8::TryCatch m_tryCatch;
  v8::Local<v8::Context> m_context;
};

}  // namespace

V8ConsoleMessage::V8ConsoleMessage(V8MessageOrigin origin, double timestamp,
                                   const String16& message)
    : m_origin(origin),
      m_timestamp(timestamp),
      m_message(message),
      m_lineNumber(0),
      m_columnNumber(0),
      m_scriptId(0),
      m_contextId(0),
      m_type(ConsoleAPIType::kLog),
      m_exceptionId(0),
      m_revokedExceptionId(0) {}

V8ConsoleMessage::~V8ConsoleMessage() {}

void V8ConsoleMessage::setLocation(const String16& url, unsigned lineNumber,
                                   unsigned columnNumber,
                                   std::unique_ptr<V8StackTraceImpl> stackTrace,
                                   int scriptId) {
  m_url = url;
  m_lineNumber = lineNumber;
  m_columnNumber = columnNumber;
  m_stackTrace = std::move(stackTrace);
  m_scriptId = scriptId;
}

void V8ConsoleMessage::reportToFrontend(
    protocol::Console::Frontend* frontend) const {
  DCHECK_EQ(V8MessageOrigin::kConsole, m_origin);
  String16 level = protocol::Console::ConsoleMessage::LevelEnum::Log;
  if (m_type == ConsoleAPIType::kDebug || m_type == ConsoleAPIType::kCount ||
      m_type == ConsoleAPIType::kTimeEnd)
    level = protocol::Console::ConsoleMessage::LevelEnum::Debug;
  else if (m_type == ConsoleAPIType::kError ||
           m_type == ConsoleAPIType::kAssert)
    level = protocol::Console::ConsoleMessage::LevelEnum::Error;
  else if (m_type == ConsoleAPIType::kWarning)
    level = protocol::Console::ConsoleMessage::LevelEnum::Warning;
  else if (m_type == ConsoleAPIType::kInfo)
    level = protocol::Console::ConsoleMessage::LevelEnum::Info;
  std::unique_ptr<protocol::Console::ConsoleMessage> result =
      protocol::Console::ConsoleMessage::create()
          .setSource(protocol::Console::ConsoleMessage::SourceEnum::ConsoleApi)
          .setLevel(level)
          .setText(m_message)
          .build();
  result->setLine(static_cast<int>(m_lineNumber));
  result->setColumn(static_cast<int>(m_columnNumber));
  result->setUrl(m_url);
  frontend->messageAdded(std::move(result));
}

std::unique_ptr<protocol::Array<protocol::Runtime::RemoteObject>>
V8ConsoleMessage::wrapArguments(V8InspectorSessionImpl* session,
                                bool generatePreview) const {
  V8InspectorImpl* inspector = session->inspector();
  int contextGroupId = session->contextGroupId();
  int contextId = m_contextId;
  if (!m_arguments.size() || !contextId) return nullptr;
  InspectedContext* inspectedContext =
      inspector->getContext(contextGroupId, contextId);
  if (!inspectedContext) return nullptr;

  v8::Isolate* isolate = inspectedContext->isolate();
  v8::HandleScope handles(isolate);
  v8::Local<v8::Context> context = inspectedContext->context();

  std::unique_ptr<protocol::Array<protocol::Runtime::RemoteObject>> args =
      protocol::Array<protocol::Runtime::RemoteObject>::create();
  if (m_type == ConsoleAPIType::kTable && generatePreview) {
    v8::Local<v8::Value> table = m_arguments[0]->Get(isolate);
    v8::Local<v8::Value> columns = m_arguments.size() > 1
                                       ? m_arguments[1]->Get(isolate)
                                       : v8::Local<v8::Value>();
    std::unique_ptr<protocol::Runtime::RemoteObject> wrapped =
        session->wrapTable(context, table, columns);
    inspectedContext = inspector->getContext(contextGroupId, contextId);
    if (!inspectedContext) return nullptr;
    if (wrapped)
      args->addItem(std::move(wrapped));
    else
      args = nullptr;
  } else {
    for (size_t i = 0; i < m_arguments.size(); ++i) {
      std::unique_ptr<protocol::Runtime::RemoteObject> wrapped =
          session->wrapObject(context, m_arguments[i]->Get(isolate), "console",
                              generatePreview);
      inspectedContext = inspector->getContext(contextGroupId, contextId);
      if (!inspectedContext) return nullptr;
      if (!wrapped) {
        args = nullptr;
        break;
      }
      args->addItem(std::move(wrapped));
    }
  }
  return args;
}

void V8ConsoleMessage::reportToFrontend(protocol::Runtime::Frontend* frontend,
                                        V8InspectorSessionImpl* session,
                                        bool generatePreview) const {
  int contextGroupId = session->contextGroupId();
  V8InspectorImpl* inspector = session->inspector();

  if (m_origin == V8MessageOrigin::kException) {
    std::unique_ptr<protocol::Runtime::RemoteObject> exception =
        wrapException(session, generatePreview);
    if (!inspector->hasConsoleMessageStorage(contextGroupId)) return;
    std::unique_ptr<protocol::Runtime::ExceptionDetails> exceptionDetails =
        protocol::Runtime::ExceptionDetails::create()
            .setExceptionId(m_exceptionId)
            .setText(exception ? m_message : m_detailedMessage)
            .setLineNumber(m_lineNumber ? m_lineNumber - 1 : 0)
            .setColumnNumber(m_columnNumber ? m_columnNumber - 1 : 0)
            .build();
    if (m_scriptId)
      exceptionDetails->setScriptId(String16::fromInteger(m_scriptId));
    if (!m_url.isEmpty()) exceptionDetails->setUrl(m_url);
    if (m_stackTrace) {
      exceptionDetails->setStackTrace(
          m_stackTrace->buildInspectorObjectImpl(inspector->debugger()));
    }
    if (m_contextId) exceptionDetails->setExecutionContextId(m_contextId);
    if (exception) exceptionDetails->setException(std::move(exception));
    frontend->exceptionThrown(m_timestamp, std::move(exceptionDetails));
    return;
  }
  if (m_origin == V8MessageOrigin::kRevokedException) {
    frontend->exceptionRevoked(m_message, m_revokedExceptionId);
    return;
  }
  if (m_origin == V8MessageOrigin::kConsole) {
    std::unique_ptr<protocol::Array<protocol::Runtime::RemoteObject>>
        arguments = wrapArguments(session, generatePreview);
    if (!inspector->hasConsoleMessageStorage(contextGroupId)) return;
    if (!arguments) {
      arguments = protocol::Array<protocol::Runtime::RemoteObject>::create();
      if (!m_message.isEmpty()) {
        std::unique_ptr<protocol::Runtime::RemoteObject> messageArg =
            protocol::Runtime::RemoteObject::create()
                .setType(protocol::Runtime::RemoteObject::TypeEnum::String)
                .build();
        messageArg->setValue(protocol::StringValue::create(m_message));
        arguments->addItem(std::move(messageArg));
      }
    }
    Maybe<String16> consoleContext;
    if (!m_consoleContext.isEmpty()) consoleContext = m_consoleContext;
    frontend->consoleAPICalled(
        consoleAPITypeValue(m_type), std::move(arguments), m_contextId,
        m_timestamp,
        m_stackTrace
            ? m_stackTrace->buildInspectorObjectImpl(inspector->debugger())
            : nullptr,
        std::move(consoleContext));
    return;
  }
  UNREACHABLE();
}

std::unique_ptr<protocol::Runtime::RemoteObject>
V8ConsoleMessage::wrapException(V8InspectorSessionImpl* session,
                                bool generatePreview) const {
  if (!m_arguments.size() || !m_contextId) return nullptr;
  DCHECK_EQ(1u, m_arguments.size());
  InspectedContext* inspectedContext =
      session->inspector()->getContext(session->contextGroupId(), m_contextId);
  if (!inspectedContext) return nullptr;

  v8::Isolate* isolate = inspectedContext->isolate();
  v8::HandleScope handles(isolate);
  // TODO(dgozman): should we use different object group?
  return session->wrapObject(inspectedContext->context(),
                             m_arguments[0]->Get(isolate), "console",
                             generatePreview);
}

V8MessageOrigin V8ConsoleMessage::origin() const { return m_origin; }

ConsoleAPIType V8ConsoleMessage::type() const { return m_type; }

// static
std::unique_ptr<V8ConsoleMessage> V8ConsoleMessage::createForConsoleAPI(
    v8::Local<v8::Context> v8Context, int contextId, int groupId,
    V8InspectorImpl* inspector, double timestamp, ConsoleAPIType type,
    const std::vector<v8::Local<v8::Value>>& arguments,
    const String16& consoleContext,
    std::unique_ptr<V8StackTraceImpl> stackTrace) {
  v8::Isolate* isolate = v8Context->GetIsolate();

  std::unique_ptr<V8ConsoleMessage> message(
      new V8ConsoleMessage(V8MessageOrigin::kConsole, timestamp, String16()));
  if (stackTrace && !stackTrace->isEmpty()) {
    message->m_url = toString16(stackTrace->topSourceURL());
    message->m_lineNumber = stackTrace->topLineNumber();
    message->m_columnNumber = stackTrace->topColumnNumber();
  }
  message->m_stackTrace = std::move(stackTrace);
  message->m_consoleContext = consoleContext;
  message->m_type = type;
  message->m_contextId = contextId;
  for (size_t i = 0; i < arguments.size(); ++i) {
    message->m_arguments.push_back(std::unique_ptr<v8::Global<v8::Value>>(
        new v8::Global<v8::Value>(isolate, arguments.at(i))));
    message->m_v8Size +=
        v8::debug::EstimatedValueSize(isolate, arguments.at(i));
  }
  if (arguments.size())
    message->m_message =
        V8ValueStringBuilder::toString(arguments[0], v8Context);

  v8::Isolate::MessageErrorLevel clientLevel = v8::Isolate::kMessageInfo;
  if (type == ConsoleAPIType::kDebug || type == ConsoleAPIType::kCount ||
      type == ConsoleAPIType::kTimeEnd) {
    clientLevel = v8::Isolate::kMessageDebug;
  } else if (type == ConsoleAPIType::kError ||
             type == ConsoleAPIType::kAssert) {
    clientLevel = v8::Isolate::kMessageError;
  } else if (type == ConsoleAPIType::kWarning) {
    clientLevel = v8::Isolate::kMessageWarning;
  } else if (type == ConsoleAPIType::kInfo || type == ConsoleAPIType::kLog) {
    clientLevel = v8::Isolate::kMessageInfo;
  }

  if (type != ConsoleAPIType::kClear) {
    inspector->client()->consoleAPIMessage(
        groupId, clientLevel, toStringView(message->m_message),
        toStringView(message->m_url), message->m_lineNumber,
        message->m_columnNumber, message->m_stackTrace.get());
  }

  return message;
}

// static
std::unique_ptr<V8ConsoleMessage> V8ConsoleMessage::createForException(
    double timestamp, const String16& detailedMessage, const String16& url,
    unsigned lineNumber, unsigned columnNumber,
    std::unique_ptr<V8StackTraceImpl> stackTrace, int scriptId,
    v8::Isolate* isolate, const String16& message, int contextId,
    v8::Local<v8::Value> exception, unsigned exceptionId) {
  std::unique_ptr<V8ConsoleMessage> consoleMessage(
      new V8ConsoleMessage(V8MessageOrigin::kException, timestamp, message));
  consoleMessage->setLocation(url, lineNumber, columnNumber,
                              std::move(stackTrace), scriptId);
  consoleMessage->m_exceptionId = exceptionId;
  consoleMessage->m_detailedMessage = detailedMessage;
  if (contextId && !exception.IsEmpty()) {
    consoleMessage->m_contextId = contextId;
    consoleMessage->m_arguments.push_back(
        std::unique_ptr<v8::Global<v8::Value>>(
            new v8::Global<v8::Value>(isolate, exception)));
    consoleMessage->m_v8Size +=
        v8::debug::EstimatedValueSize(isolate, exception);
  }
  return consoleMessage;
}

// static
std::unique_ptr<V8ConsoleMessage> V8ConsoleMessage::createForRevokedException(
    double timestamp, const String16& messageText,
    unsigned revokedExceptionId) {
  std::unique_ptr<V8ConsoleMessage> message(new V8ConsoleMessage(
      V8MessageOrigin::kRevokedException, timestamp, messageText));
  message->m_revokedExceptionId = revokedExceptionId;
  return message;
}

void V8ConsoleMessage::contextDestroyed(int contextId) {
  if (contextId != m_contextId) return;
  m_contextId = 0;
  if (m_message.isEmpty()) m_message = "<message collected>";
  Arguments empty;
  m_arguments.swap(empty);
  m_v8Size = 0;
}

// ------------------------ V8ConsoleMessageStorage ----------------------------

V8ConsoleMessageStorage::V8ConsoleMessageStorage(V8InspectorImpl* inspector,
                                                 int contextGroupId)
    : m_inspector(inspector), m_contextGroupId(contextGroupId) {}

V8ConsoleMessageStorage::~V8ConsoleMessageStorage() { clear(); }

void V8ConsoleMessageStorage::addMessage(
    std::unique_ptr<V8ConsoleMessage> message) {
  int contextGroupId = m_contextGroupId;
  V8InspectorImpl* inspector = m_inspector;
  if (message->type() == ConsoleAPIType::kClear) clear();

  inspector->forEachSession(
      contextGroupId, [&message](V8InspectorSessionImpl* session) {
        if (message->origin() == V8MessageOrigin::kConsole)
          session->consoleAgent()->messageAdded(message.get());
        session->runtimeAgent()->messageAdded(message.get());
      });
  if (!inspector->hasConsoleMessageStorage(contextGroupId)) return;

  DCHECK(m_messages.size() <= maxConsoleMessageCount);
  if (m_messages.size() == maxConsoleMessageCount) {
    m_estimatedSize -= m_messages.front()->estimatedSize();
    m_messages.pop_front();
  }
  while (m_estimatedSize + message->estimatedSize() > maxConsoleMessageV8Size &&
         !m_messages.empty()) {
    m_estimatedSize -= m_messages.front()->estimatedSize();
    m_messages.pop_front();
  }

  m_messages.push_back(std::move(message));
  m_estimatedSize += m_messages.back()->estimatedSize();
}

void V8ConsoleMessageStorage::clear() {
  m_messages.clear();
  m_estimatedSize = 0;
  m_inspector->forEachSession(m_contextGroupId,
                              [](V8InspectorSessionImpl* session) {
                                session->releaseObjectGroup("console");
                              });
  m_data.clear();
}

bool V8ConsoleMessageStorage::shouldReportDeprecationMessage(
    int contextId, const String16& method) {
  std::set<String16>& reportedDeprecationMessages =
      m_data[contextId].m_reportedDeprecationMessages;
  auto it = reportedDeprecationMessages.find(method);
  if (it != reportedDeprecationMessages.end()) return false;
  reportedDeprecationMessages.insert(it, method);
  return true;
}

int V8ConsoleMessageStorage::count(int contextId, const String16& id) {
  return ++m_data[contextId].m_count[id];
}

void V8ConsoleMessageStorage::time(int contextId, const String16& id) {
  m_data[contextId].m_time[id] = m_inspector->client()->currentTimeMS();
}

double V8ConsoleMessageStorage::timeEnd(int contextId, const String16& id) {
  std::map<String16, double>& time = m_data[contextId].m_time;
  auto it = time.find(id);
  if (it == time.end()) return 0.0;
  double elapsed = m_inspector->client()->currentTimeMS() - it->second;
  time.erase(it);
  return elapsed;
}

void V8ConsoleMessageStorage::contextDestroyed(int contextId) {
  m_estimatedSize = 0;
  for (size_t i = 0; i < m_messages.size(); ++i) {
    m_messages[i]->contextDestroyed(contextId);
    m_estimatedSize += m_messages[i]->estimatedSize();
  }
  auto it = m_data.find(contextId);
  if (it != m_data.end()) m_data.erase(contextId);
}

}  // namespace v8_inspector
