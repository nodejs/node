#pragma once

#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#include <v8.h>

namespace node {
class ExternalReferenceRegistry;
namespace quic {

// Registers the HTTP/3 application factory (creation, settings parsing,
// and session-ticket hooks) under the name "http3". Called once at
// binding initialization; a session installs the application only when
// its options request that name explicitly (set by node:http3).
void RegisterHttp3Application();

void CreateHttp3Handle(const v8::FunctionCallbackInfo<v8::Value>& args);

void RegisterHttp3ExternalReferences(ExternalReferenceRegistry* registry);

void InitHttp3PerContext(v8::Local<v8::Object> target);

}  // namespace quic
}  // namespace node

#endif  // defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS
