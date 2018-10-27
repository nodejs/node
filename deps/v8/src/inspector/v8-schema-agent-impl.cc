// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/inspector/v8-schema-agent-impl.h"

#include "src/inspector/protocol/Protocol.h"
#include "src/inspector/v8-inspector-session-impl.h"

namespace v8_inspector {

V8SchemaAgentImpl::V8SchemaAgentImpl(V8InspectorSessionImpl* session,
                                     protocol::FrontendChannel* frontendChannel,
                                     protocol::DictionaryValue* state)
    : m_session(session), m_frontend(frontendChannel) {}

V8SchemaAgentImpl::~V8SchemaAgentImpl() = default;

Response V8SchemaAgentImpl::getDomains(
    std::unique_ptr<protocol::Array<protocol::Schema::Domain>>* result) {
  std::vector<std::unique_ptr<protocol::Schema::Domain>> domains =
      m_session->supportedDomainsImpl();
  *result = protocol::Array<protocol::Schema::Domain>::create();
  for (size_t i = 0; i < domains.size(); ++i)
    (*result)->addItem(std::move(domains[i]));
  return Response::OK();
}

}  // namespace v8_inspector
