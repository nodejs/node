// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8SchemaAgentImpl_h
#define V8SchemaAgentImpl_h

#include "platform/inspector_protocol/InspectorProtocol.h"
#include "platform/v8_inspector/protocol/Schema.h"

namespace v8_inspector {

class V8InspectorSessionImpl;

namespace protocol = blink::protocol;

class V8SchemaAgentImpl : public protocol::Schema::Backend {
    PROTOCOL_DISALLOW_COPY(V8SchemaAgentImpl);
public:
    V8SchemaAgentImpl(V8InspectorSessionImpl*, protocol::FrontendChannel*, protocol::DictionaryValue* state);
    ~V8SchemaAgentImpl() override;

    void getDomains(ErrorString*, std::unique_ptr<protocol::Array<protocol::Schema::Domain>>*) override;

private:
    V8InspectorSessionImpl* m_session;
    protocol::Schema::Frontend m_frontend;
};

} // namespace v8_inspector


#endif // !defined(V8SchemaAgentImpl_h)
