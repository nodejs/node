// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DispatcherBase_h
#define DispatcherBase_h

#include "platform/inspector_protocol/BackendCallback.h"
#include "platform/inspector_protocol/Collections.h"
#include "platform/inspector_protocol/ErrorSupport.h"
#include "platform/inspector_protocol/Platform.h"
#include "platform/inspector_protocol/String16.h"
#include "platform/inspector_protocol/Values.h"

namespace blink {
namespace protocol {

class FrontendChannel;
class WeakPtr;

class PLATFORM_EXPORT DispatcherBase {
    PROTOCOL_DISALLOW_COPY(DispatcherBase);
public:
    static const char kInvalidRequest[];
    class PLATFORM_EXPORT WeakPtr {
    public:
        explicit WeakPtr(DispatcherBase*);
        ~WeakPtr();
        DispatcherBase* get() { return m_dispatcher; }
        void dispose() { m_dispatcher = nullptr; }

    private:
        DispatcherBase* m_dispatcher;
    };

    class PLATFORM_EXPORT Callback : public protocol::BackendCallback {
    public:
        Callback(std::unique_ptr<WeakPtr> backendImpl, int callId);
        virtual ~Callback();
        void dispose();

    protected:
        void sendIfActive(std::unique_ptr<protocol::DictionaryValue> partialMessage, const ErrorString& invocationError);

    private:
        std::unique_ptr<WeakPtr> m_backendImpl;
        int m_callId;
    };

    explicit DispatcherBase(FrontendChannel*);
    virtual ~DispatcherBase();

    enum CommonErrorCode {
        ParseError = -32700,
        InvalidRequest = -32600,
        MethodNotFound = -32601,
        InvalidParams = -32602,
        InternalError = -32603,
        ServerError = -32000,
    };

    static bool getCommandName(const String16& message, String16* result);

    virtual void dispatch(int callId, const String16& method, std::unique_ptr<protocol::DictionaryValue> messageObject) = 0;

    void sendResponse(int callId, const ErrorString&, ErrorSupport*, std::unique_ptr<protocol::DictionaryValue> result);
    void sendResponse(int callId, const ErrorString&, std::unique_ptr<protocol::DictionaryValue> result);
    void sendResponse(int callId, const ErrorString&);

    void reportProtocolError(int callId, CommonErrorCode, const String16& errorMessage, ErrorSupport* errors);
    void clearFrontend();

    std::unique_ptr<WeakPtr> weakPtr();

private:
    FrontendChannel* m_frontendChannel;
    protocol::HashSet<WeakPtr*> m_weakPtrs;
};

class PLATFORM_EXPORT UberDispatcher {
    PROTOCOL_DISALLOW_COPY(UberDispatcher);
public:
    explicit UberDispatcher(FrontendChannel*);
    void registerBackend(const String16& name, std::unique_ptr<protocol::DispatcherBase>);
    void dispatch(const String16& message);
    FrontendChannel* channel() { return m_frontendChannel; }
    virtual ~UberDispatcher();

private:
    FrontendChannel* m_frontendChannel;
    protocol::HashMap<String16, std::unique_ptr<protocol::DispatcherBase>> m_dispatchers;
};

} // namespace platform
} // namespace blink

#endif // !defined(DispatcherBase_h)
