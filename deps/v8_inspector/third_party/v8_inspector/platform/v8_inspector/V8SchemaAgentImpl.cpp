// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/v8_inspector/V8SchemaAgentImpl.h"

#include "platform/v8_inspector/V8InspectorSessionImpl.h"

namespace v8_inspector {

V8SchemaAgentImpl::V8SchemaAgentImpl(V8InspectorSessionImpl* session, protocol::FrontendChannel* frontendChannel, protocol::DictionaryValue* state)
    : m_session(session)
    , m_frontend(frontendChannel)
{
}

V8SchemaAgentImpl::~V8SchemaAgentImpl()
{
}

void V8SchemaAgentImpl::getDomains(ErrorString*, std::unique_ptr<protocol::Array<protocol::Schema::Domain>>* result)
{
    std::vector<std::unique_ptr<protocol::Schema::Domain>> domains = m_session->supportedDomainsImpl();
    *result = protocol::Array<protocol::Schema::Domain>::create();
    for (size_t i = 0; i < domains.size(); ++i)
        (*result)->addItem(std::move(domains[i]));
}

} // namespace v8_inspector
