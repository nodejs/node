#pragma once

#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS
#if HAVE_OPENSSL && NODE_OPENSSL_HAS_QUIC

#include <base_object.h>
#include <env.h>
#include <memory_tracker.h>
#include <nghttp3/nghttp3.h>
#include <ngtcp2/ngtcp2.h>
#include <ngtcp2/ngtcp2_crypto.h>
#include <node.h>
#include <node_mem.h>
#include <v8.h>
#include <vector>

namespace node {
namespace quic {

class Endpoint;
class Packet;

enum class Side {
  CLIENT = NGTCP2_CRYPTO_SIDE_CLIENT,
  SERVER = NGTCP2_CRYPTO_SIDE_SERVER,
};

constexpr size_t kDefaultMaxPacketLength = NGTCP2_MAX_UDP_PAYLOAD_SIZE;

// ============================================================================

// The FunctionTemplates the BindingData will store for us.
#define QUIC_CONSTRUCTORS(V)                                                   \
  V(endpoint)                                                                  \
  V(logstream)                                                                 \
  V(packet)                                                                    \
  V(session)                                                                   \
  V(stream)                                                                    \
  V(udp)

// The callbacks are persistent v8::Function references that are set in the
// quic::BindingState used to communicate data and events back out to the JS
// environment. They are set once from the JavaScript side when the
// internalBinding('quic') is first loaded.
#define QUIC_JS_CALLBACKS(V)                                                   \
  V(endpoint_close, EndpointClose)                                             \
  V(endpoint_error, EndpointError)                                             \
  V(session_new, SessionNew)                                                   \
  V(session_close, SessionClose)                                               \
  V(session_error, SessionError)                                               \
  V(session_datagram, SessionDatagram)                                         \
  V(session_datagram_status, SessionDatagramStatus)                            \
  V(session_handshake, SessionHandshake)                                       \
  V(session_ticket, SessionTicket)                                             \
  V(session_version_negotiation, SessionVersionNegotiation)                    \
  V(session_path_validation, SessionPathValidation)                            \
  V(stream_close, StreamClose)                                                 \
  V(stream_error, StreamError)                                                 \
  V(stream_created, StreamCreated)                                             \
  V(stream_reset, StreamReset)                                                 \
  V(stream_headers, StreamHeaders)                                             \
  V(stream_blocked, StreamBlocked)                                             \
  V(stream_trailers, StreamTrailers)

// The various JS strings the implementation uses.
#define QUIC_STRINGS(V)                                                        \
  V(ack_delay_exponent, "ackDelayExponent")                                    \
  V(active_connection_id_limit, "activeConnectionIDLimit")                     \
  V(alpn, "alpn")                                                              \
  V(ca, "ca")                                                                  \
  V(certs, "certs")                                                            \
  V(crl, "crl")                                                                \
  V(ciphers, "ciphers")                                                        \
  V(disable_active_migration, "disableActiveMigration")                        \
  V(enable_tls_trace, "tlsTrace")                                              \
  V(endpoint, "Endpoint")                                                      \
  V(endpoint_udp, "Endpoint::UDP")                                             \
  V(groups, "groups")                                                          \
  V(hostname, "hostname")                                                      \
  V(http3_alpn, &NGHTTP3_ALPN_H3[1])                                           \
  V(initial_max_data, "initialMaxData")                                        \
  V(initial_max_stream_data_bidi_local, "initialMaxStreamDataBidiLocal")       \
  V(initial_max_stream_data_bidi_remote, "initialMaxStreamDataBidiRemote")     \
  V(initial_max_stream_data_uni, "initialMaxStreamDataUni")                    \
  V(initial_max_streams_bidi, "initialMaxStreamsBidi")                         \
  V(initial_max_streams_uni, "initialMaxStreamsUni")                           \
  V(keylog, "keylog")                                                          \
  V(keys, "keys")                                                              \
  V(logstream, "LogStream")                                                    \
  V(max_ack_delay, "maxAckDelay")                                              \
  V(max_datagram_frame_size, "maxDatagramFrameSize")                           \
  V(max_idle_timeout, "maxIdleTimeout")                                        \
  V(packetwrap, "PacketWrap")                                                  \
  V(reject_unauthorized, "rejectUnauthorized")                                 \
  V(request_peer_certificate, "requestPeerCertificate")                        \
  V(session, "Session")                                                        \
  V(session_id_ctx, "sessionIDContext")                                        \
  V(stream, "Stream")                                                          \
  V(verify_hostname_identity, "verifyHostnameIdentity")

// =============================================================================
// The BindingState object holds state for the internalBinding('quic') binding
// instance. It is mostly used to hold the persistent constructors, strings, and
// callback references used for the rest of the implementation.
//
// TODO(@jasnell): Make this snapshotable?
class BindingData final
    : public BaseObject,
      public mem::NgLibMemoryManager<BindingData, ngtcp2_mem> {
 public:
  SET_BINDING_ID(quic_binding_data)
  static void Initialize(Environment* env, v8::Local<v8::Object> target);
  static void RegisterExternalReferences(ExternalReferenceRegistry* registry);

  static BindingData& Get(Environment* env);

  BindingData(Realm* realm, v8::Local<v8::Object> object);

  void MemoryInfo(MemoryTracker* tracker) const override;
  SET_MEMORY_INFO_NAME(BindingData)
  SET_SELF_SIZE(BindingData)

  // NgLibMemoryManager
  operator ngtcp2_mem();
  operator nghttp3_mem();
  void CheckAllocatedSize(size_t previous_size) const;
  void IncreaseAllocatedSize(size_t size);
  void DecreaseAllocatedSize(size_t size);

  // Installs the set of JavaScript callback functions that are used to
  // bridge out to the JS API.
  static void SetCallbacks(const v8::FunctionCallbackInfo<v8::Value>& args);

  std::vector<BaseObjectPtr<BaseObject>> packet_freelist;

  // Purge the packet free list to free up memory.
  static void FlushPacketFreelist(
      const v8::FunctionCallbackInfo<v8::Value>& args);

  bool in_ngtcp2_callback_scope = false;
  bool in_nghttp3_callback_scope = false;

  // The following set up various storage and accessors for common strings,
  // construction templates, and callbacks stored on the BindingData. These
  // are all defined in defs.h

#define V(name)                                                                \
  void set_##name##_constructor_template(                                      \
      v8::Local<v8::FunctionTemplate> tmpl);                                   \
  v8::Local<v8::FunctionTemplate> name##_constructor_template() const;
  QUIC_CONSTRUCTORS(V)
#undef V

#define V(name, _)                                                             \
  void set_##name##_callback(v8::Local<v8::Function> fn);                      \
  v8::Local<v8::Function> name##_callback() const;
  QUIC_JS_CALLBACKS(V)
#undef V

#define V(name, _) v8::Local<v8::String> name##_string() const;
  QUIC_STRINGS(V)
#undef V

#define V(name, _) v8::Local<v8::String> on_##name##_string() const;
  QUIC_JS_CALLBACKS(V)
#undef V

  size_t current_ngtcp2_memory_ = 0;

#define V(name) v8::Global<v8::FunctionTemplate> name##_constructor_template_;
  QUIC_CONSTRUCTORS(V)
#undef V

#define V(name, _) v8::Global<v8::Function> name##_callback_;
  QUIC_JS_CALLBACKS(V)
#undef V

#define V(name, _) mutable v8::Eternal<v8::String> name##_string_;
  QUIC_STRINGS(V)
#undef V

#define V(name, _) mutable v8::Eternal<v8::String> on_##name##_string_;
  QUIC_JS_CALLBACKS(V)
#undef V
};

void IllegalConstructor(const v8::FunctionCallbackInfo<v8::Value>& args);

// The ngtcp2 and nghttp3 callbacks have certain restrictions
// that forbid re-entry. We provide the following scopes for
// use in those to help protect against it.
struct NgTcp2CallbackScope {
  Environment* env;
  explicit NgTcp2CallbackScope(Environment* env);
  ~NgTcp2CallbackScope();
  static bool in_ngtcp2_callback(Environment* env);
};

struct NgHttp3CallbackScope {
  Environment* env;
  explicit NgHttp3CallbackScope(Environment* env);
  ~NgHttp3CallbackScope();
  static bool in_nghttp3_callback(Environment* env);
};

}  // namespace quic
}  // namespace node

#endif  // HAVE_OPENSSL && NODE_OPENSSL_HAS_QUIC
#endif  // defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS
