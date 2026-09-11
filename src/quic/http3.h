#pragma once

#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#include <v8.h>
#include <memory>
#include "application.h"
#include "session.h"

namespace node {
class ExternalReferenceRegistry;
class Realm;
namespace quic {

// Create an HTTP/3 Application implementation for the given session.
std::unique_ptr<Session::Application> CreateHttp3Application(Session* session);

Session::Application_Options Http3SettingsFromHandle(const Session& session);

void InitHttp3PerContext(Realm* realm, v8::Local<v8::Object> target);

}  // namespace quic
}  // namespace node

#endif  // defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS
