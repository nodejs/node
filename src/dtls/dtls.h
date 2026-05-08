#pragma once

#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#if HAVE_OPENSSL && HAVE_DTLS

#include <env.h>
#include <node_errors.h>
#include <v8.h>

namespace node::dtls {

// State indices shared between C++ and JS via AliasedStruct/DataView.
// Keep in sync with lib/internal/dtls/state.js.
enum DTLSEndpointStateIndex {
  IDX_ENDPOINT_STATE_BOUND = 0,
  IDX_ENDPOINT_STATE_LISTENING,
  IDX_ENDPOINT_STATE_CLOSING,
  IDX_ENDPOINT_STATE_DESTROYED,
  IDX_ENDPOINT_STATE_SESSION_COUNT,
  IDX_ENDPOINT_STATE_BUSY,
  IDX_ENDPOINT_STATE_COUNT
};

enum DTLSSessionStateIndex {
  IDX_SESSION_STATE_HANDSHAKING = 0,
  IDX_SESSION_STATE_OPEN,
  IDX_SESSION_STATE_CLOSING,
  IDX_SESSION_STATE_DESTROYED,
  IDX_SESSION_STATE_HAS_MESSAGE_LISTENER,
  IDX_SESSION_STATE_COUNT
};

// Callback indices for JS dispatch
enum DTLSCallbackIndex {
  DTLS_CB_ENDPOINT_CLOSE = 0,
  DTLS_CB_ENDPOINT_ERROR,
  DTLS_CB_SESSION_NEW,
  DTLS_CB_SESSION_CLOSE,
  DTLS_CB_SESSION_ERROR,
  DTLS_CB_SESSION_HANDSHAKE,
  DTLS_CB_SESSION_MESSAGE,
  DTLS_CB_SESSION_KEYLOG,
  DTLS_CB_SESSION_TICKET,
  DTLS_CB_COUNT
};

void CreatePerContextProperties(v8::Local<v8::Object> target,
                                v8::Local<v8::Value> unused,
                                v8::Local<v8::Context> context,
                                void* priv);
void CreatePerIsolateProperties(IsolateData* isolate_data,
                                v8::Local<v8::ObjectTemplate> target);
void RegisterExternalReferences(ExternalReferenceRegistry* registry);

}  // namespace node::dtls

#endif  // HAVE_OPENSSL && HAVE_DTLS
#endif  // defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS
