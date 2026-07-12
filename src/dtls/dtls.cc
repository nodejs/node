#include "dtls.h"

#if HAVE_OPENSSL && HAVE_DTLS

#include "dtls_context.h"
#include "dtls_endpoint.h"
#include "dtls_session.h"

#include <env-inl.h>
#include <node_external_reference.h>
#include <node_sockaddr-inl.h>
#include <util-inl.h>
#include <v8.h>

namespace node {

using v8::Context;
using v8::Local;
using v8::Object;
using v8::ObjectTemplate;
using v8::Value;

namespace dtls {

void CreatePerContextProperties(Local<Object> target,
                                Local<Value> unused,
                                Local<Context> context,
                                void* priv) {
  Environment* env = Environment::GetCurrent(context);

  // Register constructors.
  DTLSContext::InitPerContext(target, context, env);
  DTLSEndpoint::InitPerContext(target, context, env);
  DTLSSession::InitPerContext(target, context, env);

  // Endpoint state indices
  NODE_DEFINE_CONSTANT(target, IDX_ENDPOINT_STATE_BOUND);
  NODE_DEFINE_CONSTANT(target, IDX_ENDPOINT_STATE_LISTENING);
  NODE_DEFINE_CONSTANT(target, IDX_ENDPOINT_STATE_CLOSING);
  NODE_DEFINE_CONSTANT(target, IDX_ENDPOINT_STATE_DESTROYED);
  NODE_DEFINE_CONSTANT(target, IDX_ENDPOINT_STATE_SESSION_COUNT);
  NODE_DEFINE_CONSTANT(target, IDX_ENDPOINT_STATE_BUSY);

  // Session state indices
  NODE_DEFINE_CONSTANT(target, IDX_SESSION_STATE_HANDSHAKING);
  NODE_DEFINE_CONSTANT(target, IDX_SESSION_STATE_OPEN);
  NODE_DEFINE_CONSTANT(target, IDX_SESSION_STATE_CLOSING);
  NODE_DEFINE_CONSTANT(target, IDX_SESSION_STATE_DESTROYED);
  NODE_DEFINE_CONSTANT(target, IDX_SESSION_STATE_HAS_MESSAGE_LISTENER);

  // Endpoint stats indices (for BigUint64Array access from JS)
#define V(name, _) IDX_STATS_ENDPOINT_##name,
  enum IDX_STATS_ENDPOINT { DTLS_ENDPOINT_STATS(V) IDX_STATS_ENDPOINT_COUNT };
#undef V
#define V(name, _) NODE_DEFINE_CONSTANT(target, IDX_STATS_ENDPOINT_##name);
  DTLS_ENDPOINT_STATS(V);
#undef V
  NODE_DEFINE_CONSTANT(target, IDX_STATS_ENDPOINT_COUNT);

  // Session stats indices
#define V(name, _) IDX_STATS_SESSION_##name,
  enum IDX_STATS_SESSION { DTLS_SESSION_STATS(V) IDX_STATS_SESSION_COUNT };
#undef V
#define V(name, _) NODE_DEFINE_CONSTANT(target, IDX_STATS_SESSION_##name);
  DTLS_SESSION_STATS(V);
#undef V
  NODE_DEFINE_CONSTANT(target, IDX_STATS_SESSION_COUNT);

  // SSL verify mode constants
  constexpr auto SSL_VERIFY_NONE_VALUE = SSL_VERIFY_NONE;
  constexpr auto SSL_VERIFY_PEER_VALUE = SSL_VERIFY_PEER;
  constexpr auto SSL_VERIFY_FAIL_IF_NO_PEER_CERT_VALUE =
      SSL_VERIFY_FAIL_IF_NO_PEER_CERT;
  NODE_DEFINE_CONSTANT(target, SSL_VERIFY_NONE_VALUE);
  NODE_DEFINE_CONSTANT(target, SSL_VERIFY_PEER_VALUE);
  NODE_DEFINE_CONSTANT(target, SSL_VERIFY_FAIL_IF_NO_PEER_CERT_VALUE);
}

void CreatePerIsolateProperties(IsolateData* isolate_data,
                                Local<ObjectTemplate> target) {
  // Per-isolate initialization (currently none needed).
}

void RegisterExternalReferences(ExternalReferenceRegistry* registry) {
  DTLSContext::RegisterExternalReferences(registry);
  DTLSEndpoint::RegisterExternalReferences(registry);
  DTLSSession::RegisterExternalReferences(registry);
}

}  // namespace dtls
}  // namespace node

NODE_BINDING_CONTEXT_AWARE_INTERNAL(dtls,
                                    node::dtls::CreatePerContextProperties)
NODE_BINDING_PER_ISOLATE_INIT(dtls, node::dtls::CreatePerIsolateProperties)
NODE_BINDING_EXTERNAL_REFERENCE(dtls, node::dtls::RegisterExternalReferences)

#endif  // HAVE_OPENSSL && HAVE_DTLS
