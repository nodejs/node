// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8ConsoleMessage_h
#define V8ConsoleMessage_h

#include "platform/inspector_protocol/InspectorProtocol.h"
#include "platform/v8_inspector/protocol/Console.h"
#include "platform/v8_inspector/protocol/Runtime.h"
#include <deque>
#include <v8.h>

namespace v8_inspector {

class InspectedContext;
class V8InspectorImpl;
class V8InspectorSessionImpl;
class V8StackTraceImpl;

namespace protocol = blink::protocol;

enum class V8MessageOrigin { kConsole, kException, kRevokedException };

enum class ConsoleAPIType { kLog, kDebug, kInfo, kError, kWarning, kDir, kDirXML, kTable, kTrace, kStartGroup, kStartGroupCollapsed, kEndGroup, kClear, kAssert, kTimeEnd, kCount };

class V8ConsoleMessage {
public:
    ~V8ConsoleMessage();

    static std::unique_ptr<V8ConsoleMessage> createForConsoleAPI(
        double timestamp,
        ConsoleAPIType,
        const std::vector<v8::Local<v8::Value>>& arguments,
        std::unique_ptr<V8StackTraceImpl>,
        InspectedContext*);

    static std::unique_ptr<V8ConsoleMessage> createForException(
        double timestamp,
        const String16& detailedMessage,
        const String16& url,
        unsigned lineNumber,
        unsigned columnNumber,
        std::unique_ptr<V8StackTraceImpl>,
        int scriptId,
        v8::Isolate*,
        const String16& message,
        int contextId,
        v8::Local<v8::Value> exception,
        unsigned exceptionId);

    static std::unique_ptr<V8ConsoleMessage> createForRevokedException(
        double timestamp,
        const String16& message,
        unsigned revokedExceptionId);

    V8MessageOrigin origin() const;
    void reportToFrontend(protocol::Console::Frontend*) const;
    void reportToFrontend(protocol::Runtime::Frontend*, V8InspectorSessionImpl*, bool generatePreview) const;
    ConsoleAPIType type() const;
    void contextDestroyed(int contextId);

private:
    V8ConsoleMessage(V8MessageOrigin, double timestamp, const String16& message);

    using Arguments = std::vector<std::unique_ptr<v8::Global<v8::Value>>>;
    std::unique_ptr<protocol::Array<protocol::Runtime::RemoteObject>> wrapArguments(V8InspectorSessionImpl*, bool generatePreview) const;
    std::unique_ptr<protocol::Runtime::RemoteObject> wrapException(V8InspectorSessionImpl*, bool generatePreview) const;
    void setLocation(const String16& url, unsigned lineNumber, unsigned columnNumber, std::unique_ptr<V8StackTraceImpl>, int scriptId);

    V8MessageOrigin m_origin;
    double m_timestamp;
    String16 m_message;
    String16 m_url;
    unsigned m_lineNumber;
    unsigned m_columnNumber;
    std::unique_ptr<V8StackTraceImpl> m_stackTrace;
    int m_scriptId;
    int m_contextId;
    ConsoleAPIType m_type;
    unsigned m_exceptionId;
    unsigned m_revokedExceptionId;
    Arguments m_arguments;
    String16 m_detailedMessage;
};

class V8ConsoleMessageStorage {
public:
    V8ConsoleMessageStorage(V8InspectorImpl*, int contextGroupId);
    ~V8ConsoleMessageStorage();

    int contextGroupId() { return m_contextGroupId; }
    int expiredCount() { return m_expiredCount; }
    const std::deque<std::unique_ptr<V8ConsoleMessage>>& messages() const { return m_messages; }

    void addMessage(std::unique_ptr<V8ConsoleMessage>);
    void contextDestroyed(int contextId);
    void clear();

private:
    V8InspectorImpl* m_inspector;
    int m_contextGroupId;
    int m_expiredCount;
    std::deque<std::unique_ptr<V8ConsoleMessage>> m_messages;
};

} // namespace v8_inspector

#endif // V8ConsoleMessage_h
