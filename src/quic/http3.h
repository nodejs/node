#pragma once

#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

namespace node::quic {

// Registers the HTTP/3 application factory (creation, settings parsing,
// and session-ticket hooks) under the name "http3". Called once at
// binding initialization; a session installs the application only when
// its options request that name explicitly (set by node:http3).
void RegisterHttp3Application();

}  // namespace node::quic

#endif  // defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS
