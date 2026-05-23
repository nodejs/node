#pragma once

#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#include <base_object.h>
#include <env.h>
#include <memory_tracker.h>
#include <nghttp3/nghttp3.h>
#include <ngtcp2/ngtcp2.h>
#include <ngtcp2/ngtcp2_crypto.h>
#include <node.h>
#include <node_mem.h>
#include <uv.h>
#include <v8.h>
#include <functional>
#include <memory>
#include <unordered_map>
#include <vector>
#include "defs.h"

namespace node::quic {

class Endpoint;
class Packet;
class Session;
class SessionManager;

// ============================================================================

// The FunctionTemplates the BindingData will store for us.
#define QUIC_CONSTRUCTORS(V)                                                   \
  V(endpoint)                                                                  \
  V(session)                                                                   \
  V(stream)                                                                    \
  V(udp)

// The callbacks are persistent v8::Function references that are set in the
// quic::BindingState used to communicate data and events back out to the JS
// environment. They are set once from the JavaScript side when the
// internalBinding('quic') is first loaded.
#define QUIC_JS_CALLBACKS(V)                                                   \
  V(endpoint_close, EndpointClose)                                             \
  V(session_close, SessionClose)                                               \
  V(session_application, SessionApplication)                                   \
  V(session_early_data_rejected, SessionEarlyDataRejected)                     \
  V(session_goaway, SessionGoaway)                                             \
  V(session_datagram, SessionDatagram)                                         \
  V(session_datagram_status, SessionDatagramStatus)                            \
  V(session_handshake, SessionHandshake)                                       \
  V(session_keylog, SessionKeyLog)                                             \
  V(session_qlog, SessionQlog)                                                 \
  V(session_new, SessionNew)                                                   \
  V(session_new_token, SessionNewToken)                                        \
  V(session_origin, SessionOrigin)                                             \
  V(session_path_validation, SessionPathValidation)                            \
  V(session_ticket, SessionTicket)                                             \
  V(session_version_negotiation, SessionVersionNegotiation)                    \
  V(stream_blocked, StreamBlocked)                                             \
  V(stream_close, StreamClose)                                                 \
  V(stream_created, StreamCreated)                                             \
  V(stream_drain, StreamDrain)                                                 \
  V(stream_headers, StreamHeaders)                                             \
  V(stream_reset, StreamReset)                                                 \
  V(stream_trailers, StreamTrailers)

// The various JS strings the implementation uses.
#define QUIC_STRINGS(V)                                                        \
  V(abandoned, "abandoned")                                                    \
  V(aborted, "aborted")                                                        \
  V(acknowledged, "acknowledged")                                              \
  V(ack_delay_exponent, "ackDelayExponent")                                    \
  V(active_connection_id_limit, "activeConnectionIDLimit")                     \
  V(address_lru_size, "addressLRUSize")                                        \
  V(allow, "allow")                                                            \
  V(application, "application")                                                \
  V(authoritative, "authoritative")                                            \
  V(bbr, "bbr")                                                                \
  V(ca, "ca")                                                                  \
  V(cc_algorithm, "cc")                                                        \
  V(certificate_compression, "certificateCompression")                         \
  V(certs, "certs")                                                            \
  V(code, "code")                                                              \
  V(ciphers, "ciphers")                                                        \
  V(crl, "crl")                                                                \
  V(cubic, "cubic")                                                            \
  V(datagram_drop_policy, "datagramDropPolicy")                                \
  V(deny, "deny")                                                              \
  V(disable_stateless_reset, "disableStatelessReset")                          \
  V(draining_period_multiplier, "drainingPeriodMultiplier")                    \
  V(enable_connect_protocol, "enableConnectProtocol")                          \
  V(enable_early_data, "enableEarlyData")                                      \
  V(enable_datagrams, "enableDatagrams")                                       \
  V(enable_webtransport, "enableWebtransport")                                 \
  V(enable_tls_trace, "tlsTrace")                                              \
  V(endpoint, "Endpoint")                                                      \
  V(endpoint_udp, "Endpoint::UDP")                                             \
  V(failure, "failure")                                                        \
  V(groups, "groups")                                                          \
  V(handshake_timeout, "handshakeTimeout")                                     \
  V(http3_alpn, &NGHTTP3_ALPN_H3[1])                                           \
  V(initial_rtt, "initialRtt")                                                 \
  V(keep_alive_timeout, "keepAlive")                                           \
  V(initial_max_data, "initialMaxData")                                        \
  V(initial_max_stream_data_bidi_local, "initialMaxStreamDataBidiLocal")       \
  V(initial_max_stream_data_bidi_remote, "initialMaxStreamDataBidiRemote")     \
  V(initial_max_stream_data_uni, "initialMaxStreamDataUni")                    \
  V(initial_max_streams_bidi, "initialMaxStreamsBidi")                         \
  V(initial_max_streams_uni, "initialMaxStreamsUni")                           \
  V(ipv6_only, "ipv6Only")                                                     \
  V(reuse_port, "reusePort")                                                   \
  V(keylog, "keylog")                                                          \
  V(keys, "keys")                                                              \
  V(lost, "lost")                                                              \
  V(max_ack_delay, "maxAckDelay")                                              \
  V(max_connections_per_host, "maxConnectionsPerHost")                         \
  V(max_connections_total, "maxConnectionsTotal")                              \
  V(max_datagram_frame_size, "maxDatagramFrameSize")                           \
  V(max_datagram_send_attempts, "maxDatagramSendAttempts")                     \
  V(stream_idle_timeout, "streamIdleTimeout")                                  \
  V(max_field_section_size, "maxFieldSectionSize")                             \
  V(max_header_length, "maxHeaderLength")                                      \
  V(max_header_pairs, "maxHeaderPairs")                                        \
  V(idle_timeout, "idleTimeout")                                               \
  V(max_idle_timeout, "maxIdleTimeout")                                        \
  V(max_payload_size, "maxPayloadSize")                                        \
  V(retry_rate, "retryRate")                                                   \
  V(retry_burst, "retryBurst")                                                 \
  V(stateless_reset_rate, "statelessResetRate")                                \
  V(stateless_reset_burst, "statelessResetBurst")                              \
  V(version_negotiation_rate, "versionNegotiationRate")                        \
  V(version_negotiation_burst, "versionNegotiationBurst")                      \
  V(immediate_close_rate, "immediateCloseRate")                                \
  V(immediate_close_burst, "immediateCloseBurst")                              \
  V(session_creation_rate, "sessionCreationRate")                              \
  V(session_creation_burst, "sessionCreationBurst")                            \
  V(block_list, "blockList")                                                   \
  V(block_list_policy, "blockListPolicy")                                      \
  V(max_stream_window, "maxStreamWindow")                                      \
  V(max_window, "maxWindow")                                                   \
  V(min_version, "minVersion")                                                 \
  V(port, "port")                                                              \
  V(preferred_address_ipv4, "preferredAddressIpv4")                            \
  V(preferred_address_ipv6, "preferredAddressIpv6")                            \
  V(preferred_address_strategy, "preferredAddressPolicy")                      \
  V(alpn, "alpn")                                                              \
  V(qlog, "qlog")                                                              \
  V(qpack_blocked_streams, "qpackBlockedStreams")                              \
  V(qpack_encoder_max_dtable_capacity, "qpackEncoderMaxDTableCapacity")        \
  V(qpack_max_dtable_capacity, "qpackMaxDTableCapacity")                       \
  V(reason, "reason")                                                          \
  V(reject_unauthorized, "rejectUnauthorized")                                 \
  V(reno, "reno")                                                              \
  V(reset_token_secret, "resetTokenSecret")                                    \
  V(retry_token_expiration, "retryTokenExpiration")                            \
  V(rx_loss, "rxDiagnosticLoss")                                               \
  V(servername, "servername")                                                  \
  V(session, "Session")                                                        \
  V(sni, "sni")                                                                \
  V(stream, "Stream")                                                          \
  V(success, "success")                                                        \
  V(tls_options, "tls")                                                        \
  V(token, "token")                                                            \
  V(token_expiration, "tokenExpiration")                                       \
  V(token_secret, "tokenSecret")                                               \
  V(transport, "transport")                                                    \
  V(transport_params, "transportParams")                                       \
  V(type, "type")                                                              \
  V(tx_loss, "txDiagnosticLoss")                                               \
  V(udp_receive_buffer_size, "udpReceiveBufferSize")                           \
  V(udp_send_buffer_size, "udpSendBufferSize")                                 \
  V(udp_ttl, "udpTTL")                                                         \
  V(unacknowledged_packet_threshold, "unacknowledgedPacketThreshold")          \
  V(validate_address, "validateAddress")                                       \
  V(verify_client, "verifyClient")                                             \
  V(verify_hostname, "verifyHostname")                                         \
  V(verify_peer_strict, "verifyPeerStrict")                                    \
  V(verify_private_key, "verifyPrivateKey")                                    \
  V(version, "version")

// =============================================================================
// Lightweight wrappers around uv_check_t that ensure safe handle closure.
// The check handle is embedded in a heap-allocated CheckWrap whose destruction
// is deferred until the uv_close callback fires, preventing use-after-free
// when the owning object is destroyed before libuv finishes closing the handle.
// Follows the same two-layer pattern as TimerWrap / TimerWrapHandle
// (see timer_wrap.h).
// TODO(@jasnell): Consider moving it out to a separate file like timer_wrap.h.
class CheckWrap final : public MemoryRetainer {
 public:
  using CheckCb = std::function<void()>;

  template <typename... Args>
  explicit CheckWrap(Environment* env, Args&&... args)
      : env_(env), fn_(std::forward<Args>(args)...) {
    uv_check_init(env->event_loop(), &check_);
    check_.data = this;
  }

  DISALLOW_COPY_AND_MOVE(CheckWrap)

  inline Environment* env() const { return env_; }

  void Start();
  void Stop();
  void Close();
  void Ref();
  void Unref();

  SET_NO_MEMORY_INFO()
  SET_MEMORY_INFO_NAME(CheckWrap)
  SET_SELF_SIZE(CheckWrap)

 private:
  static void OnCheck(uv_check_t* check);
  static void CheckClosedCb(uv_handle_t* handle);
  ~CheckWrap() = default;

  Environment* env_;
  CheckCb fn_;
  uv_check_t check_;

  friend std::unique_ptr<CheckWrap>::deleter_type;
};

class CheckWrapHandle : public MemoryRetainer {
 public:
  template <typename... Args>
  explicit CheckWrapHandle(Environment* env, Args&&... args)
      : check_(new CheckWrap(env, std::forward<Args>(args)...)) {
    env->AddCleanupHook(CleanupHook, this);
  }

  DISALLOW_COPY_AND_MOVE(CheckWrapHandle)

  ~CheckWrapHandle() { Close(); }

  inline operator bool() const { return check_ != nullptr; }

  void Start();
  void Stop();
  void Close();
  void Ref();
  void Unref();

  void MemoryInfo(node::MemoryTracker* tracker) const override;

  SET_MEMORY_INFO_NAME(CheckWrapHandle)
  SET_SELF_SIZE(CheckWrapHandle)

 private:
  static void CleanupHook(void* data);
  CheckWrap* check_;
};

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
  static void InitPerContext(Realm* realm, v8::Local<v8::Object> target);
  static void RegisterExternalReferences(ExternalReferenceRegistry* registry);

  static BindingData& Get(Environment* env);
  static inline BindingData& Get(Realm* realm) { return Get(realm->env()); }

  BindingData(Realm* realm, v8::Local<v8::Object> object);
  ~BindingData() override;
  DISALLOW_COPY_AND_MOVE(BindingData)

  void MemoryInfo(MemoryTracker* tracker) const override;
  SET_MEMORY_INFO_NAME(BindingData)
  SET_SELF_SIZE(BindingData)

  // NgLibMemoryManager — the base class provides CheckAllocatedSize,
  // IncreaseAllocatedSize, DecreaseAllocatedSize, and StopTrackingMemory.
  // Actual allocations go through the thread-local allocators below.
  void CheckAllocatedSize(size_t previous_size) const;
  void IncreaseAllocatedSize(size_t size);
  void DecreaseAllocatedSize(size_t size);

  // Thread-local allocators that outlive BindingData destruction.
  // Both ngtcp2 and nghttp3 store the allocator pointer inside every
  // object they allocate; some of those objects (e.g., nghttp3 rcbufs
  // backing V8 external strings) can be freed after BindingData is gone.
  ngtcp2_mem* ngtcp2_allocator();
  nghttp3_mem* nghttp3_allocator();

  // Installs the set of JavaScript callback functions that are used to
  // bridge out to the JS API.
  JS_METHOD(SetCallbacks);

  // Lazily-created per-Realm SessionManager. Centralizes CID -> Session
  // routing so that any endpoint can route packets to any session.
  SessionManager& session_manager();

  // Schedule a session for deferred SendPendingData. Sessions are accumulated
  // during the I/O poll phase (via Endpoint::Receive -> Session::ReadPacket)
  // and flushed in a uv_check callback immediately after poll completes.
  // This batches multiple received packets before generating responses,
  // allowing ngtcp2 to make better ACK coalescing decisions.
  void ScheduleSessionFlush(const BaseObjectPtr<Session>& session);

  std::unordered_map<Endpoint*, BaseObjectPtr<BaseObject>> listening_endpoints;

  v8::Local<v8::String> error_name_string(const char* name);

  size_t current_ngtcp2_memory_ = 0;

  // The following set up various storage and accessors for common strings,
  // construction templates, and callbacks stored on the BindingData. These
  // are all defined in defs.h

#define V(name)                                                                \
  void set_##name##_constructor_template(                                      \
      v8::Local<v8::FunctionTemplate> tmpl);                                   \
  v8::Local<v8::FunctionTemplate> name##_constructor_template() const;
  QUIC_CONSTRUCTORS(V)
#undef V

  void set_transport_params_template(v8::Local<v8::DictionaryTemplate> tmpl);
  v8::Local<v8::DictionaryTemplate> transport_params_template() const;

  void set_application_options_template(v8::Local<v8::DictionaryTemplate> tmpl);
  v8::Local<v8::DictionaryTemplate> application_options_template() const;

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

#define V(name) v8::Global<v8::FunctionTemplate> name##_constructor_template_;
  QUIC_CONSTRUCTORS(V)
#undef V

  v8::Global<v8::DictionaryTemplate> transport_params_template_;
  v8::Global<v8::DictionaryTemplate> application_options_template_;

#define V(name, _) v8::Global<v8::Function> name##_callback_;
  QUIC_JS_CALLBACKS(V)
#undef V

#define V(name, _) mutable v8::Eternal<v8::String> name##_string_;
  QUIC_STRINGS(V)
#undef V

#define V(name, _) mutable v8::Eternal<v8::String> on_##name##_string_;
  QUIC_JS_CALLBACKS(V)
#undef V

  // Lazy cache backing error_name_string()
  std::unordered_map<const char*, v8::Eternal<v8::String>> error_name_strings_;

  std::unique_ptr<SessionManager> session_manager_;

  // Type-erased arena storage. The concrete AliasedStructArena<T> types
  // are only complete in the .cc files where Stream::State etc. are defined.
  // Each .cc file provides typed accessor methods. The deleters are set
  // when the arenas are created so that ~BindingData destroys them correctly.
  using ArenaDeleter = void (*)(void*);
  using ArenaPtr = std::unique_ptr<void, ArenaDeleter>;
  ArenaPtr stream_state_arena_{nullptr, +[](void*) {}};
  ArenaPtr stream_stats_arena_{nullptr, +[](void*) {}};
  ArenaPtr session_state_arena_{nullptr, +[](void*) {}};
  ArenaPtr session_stats_arena_{nullptr, +[](void*) {}};
  ArenaPtr endpoint_state_arena_{nullptr, +[](void*) {}};
  ArenaPtr endpoint_stats_arena_{nullptr, +[](void*) {}};

  // Deferred send flush state. The CheckWrapHandle fires immediately after
  // the I/O poll phase in the same event loop tick, allowing batched
  // receive processing: all packets are read during poll, then
  // SendPendingData is called once per dirty session in the check callback.
  CheckWrapHandle flush_check_;
  std::vector<BaseObjectPtr<Session>> pending_flush_sessions_;
  bool flush_check_started_ = false;

  void OnFlushCheck();
};

JS_METHOD_IMPL(IllegalConstructor);

// The ngtcp2 and nghttp3 callbacks have certain restrictions
// that forbid re-entry. We provide the following scopes for
// use in those to help protect against it.
// These callback scopes are per-session, not per-environment. This ensures
// that one session's ngtcp2/nghttp3 callback does not block an unrelated
// session from sending packets. A BaseObjectPtr prevents the Session from
// being prematurely freed while the scope is alive on the stack.
struct NgTcp2CallbackScope final {
  BaseObjectPtr<Session> session;
  explicit NgTcp2CallbackScope(Session* session);
  DISALLOW_COPY_AND_MOVE(NgTcp2CallbackScope)
  ~NgTcp2CallbackScope();
};

struct NgHttp3CallbackScope final {
  BaseObjectPtr<Session> session;
  explicit NgHttp3CallbackScope(Session* session);
  DISALLOW_COPY_AND_MOVE(NgHttp3CallbackScope)
  ~NgHttp3CallbackScope();
};

struct CallbackScopeBase {
  Environment* env;
  v8::Context::Scope context_scope;
  v8::TryCatch try_catch;

  explicit CallbackScopeBase(Environment* env);
  DISALLOW_COPY_AND_MOVE(CallbackScopeBase)
  ~CallbackScopeBase();
};

// Maintains a strong reference to BaseObject type ptr to keep it alive during
// a MakeCallback during which it might be destroyed.
template <typename T>
struct CallbackScope final : public CallbackScopeBase {
  BaseObjectPtr<T> ref;
  explicit CallbackScope(const T* ptr)
      : CallbackScopeBase(ptr->env()), ref(ptr) {}
  DISALLOW_COPY_AND_MOVE(CallbackScope)
  explicit CallbackScope(T* ptr) : CallbackScopeBase(ptr->env()), ref(ptr) {}
};

}  // namespace node::quic

#endif  // defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS
