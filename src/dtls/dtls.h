#pragma once

#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#if HAVE_OPENSSL && HAVE_DTLS

#include <env.h>
#include <node_errors.h>
#include <uv.h>
#include <v8.h>

#include <cstdint>

namespace node::dtls {

// Utilities for updating stats maintained in an AliasedStruct.
template <typename Stats, uint64_t Stats::*member>
void IncrementStat(Stats* stats, uint64_t amt = 1) {
  stats->*member += amt;
}

template <typename Stats, uint64_t Stats::*member>
void RecordTimestampStat(Stats* stats) {
  stats->*member = uv_hrtime();
}

#define DTLS_STAT_INCREMENT(Type, name)                                        \
  IncrementStat<Type, &Type::name>(stats_.Data())
#define DTLS_STAT_INCREMENT_N(Type, name, amt)                                 \
  IncrementStat<Type, &Type::name>(stats_.Data(), amt)
#define DTLS_STAT_RECORD_TIMESTAMP(Type, name)                                 \
  RecordTimestampStat<Type, &Type::name>(stats_.Data())

#define DTLS_STAT_FIELD(_, name) uint64_t name;

// ============================================================================
// Stats X-macros: V(ENUM_NAME, field_name)

#define DTLS_ENDPOINT_STATS(V)                                                 \
  V(CREATED_AT, created_at)                                                    \
  V(DESTROYED_AT, destroyed_at)                                                \
  V(BYTES_RECEIVED, bytes_received)                                            \
  V(BYTES_SENT, bytes_sent)                                                    \
  V(PACKETS_RECEIVED, packets_received)                                        \
  V(PACKETS_SENT, packets_sent)                                                \
  V(SERVER_SESSIONS, server_sessions)                                          \
  V(CLIENT_SESSIONS, client_sessions)                                          \
  V(SERVER_BUSY_COUNT, server_busy_count)

#define DTLS_SESSION_STATS(V)                                                  \
  V(CREATED_AT, created_at)                                                    \
  V(DESTROYED_AT, destroyed_at)                                                \
  V(CLOSING_AT, closing_at)                                                    \
  V(HANDSHAKE_COMPLETED_AT, handshake_completed_at)                            \
  V(BYTES_RECEIVED, bytes_received)                                            \
  V(BYTES_SENT, bytes_sent)                                                    \
  V(MESSAGES_RECEIVED, messages_received)                                      \
  V(MESSAGES_SENT, messages_sent)                                              \
  V(RETRANSMIT_COUNT, retransmit_count)

// State "indices" shared between C++ and JS via AliasedStruct/DataView. These
// are BYTE OFFSETS into the state struct, not sequential indices: session_count
// is a uint32, so `busy`, which follows it, sits at byte offset 8. The
// static_asserts in dtls_endpoint.cc pin these to the actual struct layout.
// Keep in sync with lib/internal/dtls/state.js.
enum DTLSEndpointStateIndex {
  IDX_ENDPOINT_STATE_BOUND = 0,
  IDX_ENDPOINT_STATE_LISTENING = 1,
  IDX_ENDPOINT_STATE_CLOSING = 2,
  IDX_ENDPOINT_STATE_DESTROYED = 3,
  IDX_ENDPOINT_STATE_SESSION_COUNT = 4,
  IDX_ENDPOINT_STATE_BUSY = 8,
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
