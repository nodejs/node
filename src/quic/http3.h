#pragma once

#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#include <memory>
#include <optional>
#include "application.h"
#include "session.h"

namespace node::quic {

// Create an HTTP/3 Application implementation for the given session.
// Uses the Application_Options from the session's config for HTTP/3
// specific settings (qpack, max header length, etc.).
std::unique_ptr<Session::Application> CreateHttp3Application(
    Session* session, const Session::Application_Options& options);

// Parse HTTP/3 specific session ticket app data. Called from
// Application::ParseTicketData() when the type byte is HTTP3.
// The data includes the type byte prefix.
std::optional<PendingTicketAppData> ParseHttp3TicketData(const uv_buf_t& data);

}  // namespace node::quic

#endif  // defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS
