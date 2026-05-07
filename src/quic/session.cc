#if HAVE_OPENSSL && HAVE_QUIC
#include "guard.h"
#ifndef OPENSSL_NO_QUIC
#include <aliased_struct-inl.h>
#include <async_wrap-inl.h>
#include <crypto/crypto_util.h>
#include <debug_utils-inl.h>
#include <env-inl.h>
#include <memory_tracker-inl.h>
#include <ngtcp2/ngtcp2.h>
#include <node_bob-inl.h>
#include <node_errors.h>
#include <node_http_common-inl.h>
#include <node_sockaddr-inl.h>
#include <req_wrap-inl.h>
#include <timer_wrap-inl.h>
#include <util-inl.h>
#include <uv.h>
#include <v8.h>
#include "application.h"
#include "bindingdata.h"
#include "cid.h"
#include "data.h"
#include "defs.h"
#include "endpoint.h"
#include "http3.h"
#include "ncrypto.h"
#include "packet.h"
#include "preferredaddress.h"
#include "session.h"
#include "session_manager.h"
#include "sessionticket.h"
#include "streams.h"
#include "tlscontext.h"
#include "transportparams.h"

namespace node {

using v8::Array;
using v8::ArrayBufferView;
using v8::BigInt;
using v8::Boolean;
using v8::FunctionCallbackInfo;
using v8::HandleScope;
using v8::Int32;
using v8::Integer;
using v8::Just;
using v8::Local;
using v8::LocalVector;
using v8::Maybe;
using v8::MaybeLocal;
using v8::Nothing;
using v8::Number;
using v8::Object;
using v8::ObjectTemplate;
using v8::String;
using v8::Undefined;
using v8::Value;

namespace quic {

// Listener flags are packed into a single uint32_t bitfield to reduce
// the size of the shared state buffer. Each bit indicates whether a
// corresponding JS callback is registered.
enum class SessionListenerFlags : uint32_t {
  PATH_VALIDATION = 1 << 0,
  DATAGRAM = 1 << 1,
  DATAGRAM_STATUS = 1 << 2,
  SESSION_TICKET = 1 << 3,
  NEW_TOKEN = 1 << 4,
  ORIGIN = 1 << 5,
};

inline SessionListenerFlags operator|(SessionListenerFlags a,
                                      SessionListenerFlags b) {
  return static_cast<SessionListenerFlags>(static_cast<uint32_t>(a) |
                                           static_cast<uint32_t>(b));
}

inline SessionListenerFlags operator&(SessionListenerFlags a,
                                      SessionListenerFlags b) {
  return static_cast<SessionListenerFlags>(static_cast<uint32_t>(a) &
                                           static_cast<uint32_t>(b));
}

inline SessionListenerFlags operator&(uint32_t a, SessionListenerFlags b) {
  return static_cast<SessionListenerFlags>(a & static_cast<uint32_t>(b));
}

inline bool operator!(SessionListenerFlags a) {
  return static_cast<uint32_t>(a) == 0;
}

inline bool HasListenerFlag(uint32_t flags, SessionListenerFlags flag) {
  return !!(flags & flag);
}

// Compute the maximum datagram payload that fits within the peer's
// max_datagram_frame_size transport parameter. The DATAGRAM frame has
// overhead of 1 byte (frame type) + variable-length integer encoding
// of the payload length. This mirrors the check in ngtcp2's
// ngtcp2_pkt_datagram_framelen (1 + varint_len(payload) + payload).
uint64_t MaxDatagramPayload(uint64_t max_frame_size) {
  // A DATAGRAM frame needs at least 1 (type) + 1 (varint) + 0 (data).
  if (max_frame_size < 2) return 0;
  // QUIC variable-length integer encoding sizes (RFC 9000 Section 16).
  auto varint_len = [](uint64_t n) -> uint64_t {
    if (n < 64) return 1;
    if (n < 16384) return 2;
    if (n < 1073741824) return 4;
    return 8;
  };
  // Start with the optimistic payload assuming minimum varint (1 byte).
  uint64_t payload = max_frame_size - 2;
  // If the payload requires a larger varint, the overhead increases.
  // Recompute with the actual varint length of the candidate payload.
  uint64_t overhead = 1 + varint_len(payload);
  if (overhead + payload > max_frame_size) {
    payload = max_frame_size - 1 - varint_len(max_frame_size - 3);
  }
  return payload;
}

#define SESSION_STATE(V)                                                       \
  V(LISTENER_FLAGS, listener_flags, uint32_t)                                  \
  V(CLOSING, closing, uint8_t)                                                 \
  V(GRACEFUL_CLOSE, graceful_close, uint8_t)                                   \
  V(SILENT_CLOSE, silent_close, uint8_t)                                       \
  V(STATELESS_RESET, stateless_reset, uint8_t)                                 \
  V(HANDSHAKE_COMPLETED, handshake_completed, uint8_t)                         \
  V(HANDSHAKE_CONFIRMED, handshake_confirmed, uint8_t)                         \
  V(STREAM_OPEN_ALLOWED, stream_open_allowed, uint8_t)                         \
  V(PRIORITY_SUPPORTED, priority_supported, uint8_t)                           \
  V(HEADERS_SUPPORTED, headers_supported, uint8_t)                             \
  V(WRAPPED, wrapped, uint8_t)                                                 \
  V(APPLICATION_TYPE, application_type, uint8_t)                               \
  V(NO_ERROR_CODE, no_error_code, error_code)                                  \
  V(INTERNAL_ERROR_CODE, internal_error_code, error_code)                      \
  V(MAX_DATAGRAM_SIZE, max_datagram_size, uint16_t)                            \
  V(LAST_DATAGRAM_ID, last_datagram_id, datagram_id)                           \
  V(MAX_PENDING_DATAGRAMS, max_pending_datagrams, uint16_t)

#define SESSION_STATS(V)                                                       \
  V(CREATED_AT, created_at)                                                    \
  V(DESTROYED_AT, destroyed_at)                                                \
  V(CLOSING_AT, closing_at)                                                    \
  V(HANDSHAKE_COMPLETED_AT, handshake_completed_at)                            \
  V(HANDSHAKE_CONFIRMED_AT, handshake_confirmed_at)                            \
  V(BYTES_RECEIVED, bytes_received)                                            \
  V(BIDI_IN_STREAM_COUNT, bidi_in_stream_count)                                \
  V(BIDI_OUT_STREAM_COUNT, bidi_out_stream_count)                              \
  V(UNI_IN_STREAM_COUNT, uni_in_stream_count)                                  \
  V(UNI_OUT_STREAM_COUNT, uni_out_stream_count)                                \
  V(MAX_BYTES_IN_FLIGHT, max_bytes_in_flight)                                  \
  V(BYTES_IN_FLIGHT, bytes_in_flight)                                          \
  V(BLOCK_COUNT, block_count)                                                  \
  V(CWND, cwnd)                                                                \
  V(LATEST_RTT, latest_rtt)                                                    \
  V(MIN_RTT, min_rtt)                                                          \
  V(RTTVAR, rttvar)                                                            \
  V(SMOOTHED_RTT, smoothed_rtt)                                                \
  V(SSTHRESH, ssthresh)                                                        \
  V(PKT_SENT, pkt_sent)                                                        \
  V(BYTES_SENT, bytes_sent)                                                    \
  V(PKT_RECV, pkt_recv)                                                        \
  V(BYTES_RECV, bytes_recv)                                                    \
  V(PKT_LOST, pkt_lost)                                                        \
  V(BYTES_LOST, bytes_lost)                                                    \
  V(PING_RECV, ping_recv)                                                      \
  V(PKT_DISCARDED, pkt_discarded)                                              \
  V(DATAGRAMS_RECEIVED, datagrams_received)                                    \
  V(DATAGRAMS_SENT, datagrams_sent)                                            \
  V(DATAGRAMS_ACKNOWLEDGED, datagrams_acknowledged)                            \
  V(DATAGRAMS_LOST, datagrams_lost)

#define NO_SIDE_EFFECT true
#define SIDE_EFFECT false

#define SESSION_JS_METHODS(V)                                                  \
  V(Destroy, destroy, SIDE_EFFECT)                                             \
  V(GetRemoteAddress, getRemoteAddress, NO_SIDE_EFFECT)                        \
  V(GetLocalAddress, getLocalAddress, NO_SIDE_EFFECT)                          \
  V(GetCertificate, getCertificate, NO_SIDE_EFFECT)                            \
  V(GetEphemeralKeyInfo, getEphemeralKey, NO_SIDE_EFFECT)                      \
  V(GetPeerCertificate, getPeerCertificate, NO_SIDE_EFFECT)                    \
  V(GracefulClose, gracefulClose, SIDE_EFFECT)                                 \
  V(SilentClose, silentClose, SIDE_EFFECT)                                     \
  V(UpdateKey, updateKey, SIDE_EFFECT)                                         \
  V(OpenStream, openStream, SIDE_EFFECT)                                       \
  V(SendDatagram, sendDatagram, SIDE_EFFECT)

struct Session::State final {
#define V(_, name, type) type name;
  SESSION_STATE(V)
#undef V
};

STAT_STRUCT(Session, SESSION)

// ============================================================================

class Http3Application;

namespace {
constexpr std::string to_string(PreferredAddress::Policy policy) {
  switch (policy) {
    case PreferredAddress::Policy::USE_PREFERRED:
      return "use";
    case PreferredAddress::Policy::IGNORE_PREFERRED:
      return "ignore";
  }
  return "<unknown>";
}

constexpr std::string to_string(Side side) {
  switch (side) {
    case Side::CLIENT:
      return "client";
    case Side::SERVER:
      return "server";
  }
  return "<unknown>";
}

constexpr std::string to_string(ngtcp2_encryption_level level) {
  switch (level) {
    case NGTCP2_ENCRYPTION_LEVEL_1RTT:
      return "1rtt";
    case NGTCP2_ENCRYPTION_LEVEL_0RTT:
      return "0rtt";
    case NGTCP2_ENCRYPTION_LEVEL_HANDSHAKE:
      return "handshake";
    case NGTCP2_ENCRYPTION_LEVEL_INITIAL:
      return "initial";
  }
  return "<unknown>";
}

constexpr std::string to_string(ngtcp2_cc_algo cc_algorithm) {
#define V(name, label)                                                         \
  case NGTCP2_CC_ALGO_##name:                                                  \
    return #label;
  switch (cc_algorithm) { /* NOLINT(whitespace/newline) */
    CC_ALGOS(V)
  }
  return "<unknown>";
#undef V
}

Maybe<ngtcp2_cc_algo> getAlgoFromString(Environment* env, Local<String> input) {
  auto& state = BindingData::Get(env);
#define V(name, str)                                                           \
  if (input->StringEquals(state.str##_string())) {                             \
    return Just(NGTCP2_CC_ALGO_##name);                                        \
  }

  CC_ALGOS(V)

#undef V
  return Nothing<ngtcp2_cc_algo>();
}

// Qlog is a JSON-based logging format that is being standardized for low-level
// debug logging of QUIC connections and dataflows. The qlog output is generated
// optionally by ngtcp2 for us. The on_qlog_write callback is registered with
// ngtcp2 to emit the qlog information. Every Session will have it's own qlog
// stream.
void on_qlog_write(void* user_data,
                   uint32_t flags,
                   const void* data,
                   size_t len) {
  static_cast<Session*>(user_data)->EmitQlog(
      flags, std::string_view(static_cast<const char*>(data), len));
}

// Forwards detailed(verbose) debugging information from ngtcp2. Enabled using
// the NODE_DEBUG_NATIVE=NGTCP2 category.
void ngtcp2_debug_log(void* user_data, const char* fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  std::string format(fmt, strlen(fmt) + 1);
  format[strlen(fmt)] = '\n';
  // Debug() does not work with the va_list here. So we use vfprintf
  // directly instead. Ngtcp2DebugLog is only enabled when the debug
  // category is enabled. The thread ID prefix helps distinguish output
  // from concurrent sessions across worker threads.
  fprintf(stderr, "ngtcp2 ");
  vfprintf(stderr, format.c_str(), ap);
  va_end(ap);
}

template <typename Opt, PreferredAddress::Policy Opt::*member>
bool SetOption(Environment* env,
               Opt* options,
               const Local<Object>& object,
               const Local<String>& name) {
  Local<Value> value;
  PreferredAddress::Policy policy = PreferredAddress::Policy::USE_PREFERRED;
  if (!object->Get(env->context(), name).ToLocal(&value) ||
      !PreferredAddress::tryGetPolicy(env, value).To(&policy)) {
    return false;
  }
  options->*member = policy;
  return true;
}

template <typename Opt, TLSContext::Options Opt::*member>
bool SetOption(Environment* env,
               Opt* options,
               const Local<Object>& object,
               const Local<String>& name) {
  Local<Value> value;
  TLSContext::Options opts;
  if (!object->Get(env->context(), name).ToLocal(&value) ||
      !TLSContext::Options::From(env, value).To(&opts)) {
    return false;
  }
  options->*member = opts;
  return true;
}

template <typename Opt, TransportParams::Options Opt::*member>
bool SetOption(Environment* env,
               Opt* options,
               const Local<Object>& object,
               const Local<String>& name) {
  Local<Value> value;
  TransportParams::Options opts;
  if (!object->Get(env->context(), name).ToLocal(&value) ||
      !TransportParams::Options::From(env, value).To(&opts)) {
    return false;
  }
  options->*member = opts;
  return true;
}

template <typename Opt, ngtcp2_cc_algo Opt::*member>
bool SetOption(Environment* env,
               Opt* options,
               const Local<Object>& object,
               const Local<String>& name) {
  Local<Value> value;
  if (!object->Get(env->context(), name).ToLocal(&value)) return false;
  if (!value->IsUndefined()) {
    ngtcp2_cc_algo algo;
    if (value->IsString()) {
      if (!getAlgoFromString(env, value.As<String>()).To(&algo)) {
        THROW_ERR_INVALID_ARG_VALUE(env, "The cc_algorithm option is invalid");
        return false;
      }
    } else {
      if (!value->IsInt32()) {
        THROW_ERR_INVALID_ARG_VALUE(
            env, "The cc_algorithm option must be a string or an integer");
        return false;
      }
      Local<Int32> num;
      if (!value->ToInt32(env->context()).ToLocal(&num)) {
        THROW_ERR_INVALID_ARG_VALUE(env, "The cc_algorithm option is invalid");
        return false;
      }
      switch (num->Value()) {
#define V(name, _)                                                             \
  case NGTCP2_CC_ALGO_##name:                                                  \
    break;
        CC_ALGOS(V)
#undef V
        default:
          THROW_ERR_INVALID_ARG_VALUE(env,
                                      "The cc_algorithm option is invalid");
          return false;
      }
      algo = FromV8Value<ngtcp2_cc_algo>(num);
    }
    options->*member = algo;
  }
  return true;
}

template <typename Opt, uint8_t Opt::*member>
bool SetOption(Environment* env,
               Opt* options,
               const v8::Local<v8::Object>& object,
               const v8::Local<v8::String>& name) {
  v8::Local<v8::Value> value;
  if (!object->Get(env->context(), name).ToLocal(&value)) return false;
  if (!value->IsUndefined()) {
    if (!value->IsUint32()) {
      Utf8Value nameStr(env->isolate(), name);
      THROW_ERR_INVALID_ARG_VALUE(
          env, "The %s option must be an uint8", *nameStr);
      return false;
    }
    uint32_t val = value.As<v8::Uint32>()->Value();
    if (val > 255) {
      Utf8Value nameStr(env->isolate(), name);
      THROW_ERR_INVALID_ARG_VALUE(
          env, "The %s option must be <= 255", *nameStr);
      return false;
    }
    options->*member = static_cast<uint8_t>(val);
  }
  return true;
}
}  // namespace

// ============================================================================
Session::Config::Config(Environment* env,
                        Side side,
                        const Options& options,
                        uint32_t version,
                        const SocketAddress& local_address,
                        const SocketAddress& remote_address,
                        const CID& dcid,
                        const CID& scid,
                        const CID& ocid)
    : side(side),
      options(options),
      version(version),
      local_address(local_address),
      remote_address(remote_address),
      dcid(dcid),
      scid(scid),
      ocid(ocid) {
  ngtcp2_settings_default(&settings);
  settings.initial_ts = uv_hrtime();

  // Advertise all versions ngtcp2 supports for compatible version
  // negotiation (RFC 9368). The preferred list orders the newest
  // version first so that negotiation upgrades when possible. The
  // initial packet version (options.version) defaults to V1 for
  // maximum compatibility with peers that don't support version
  // negotiation.
  static const uint32_t kSupportedVersions[] = {NGTCP2_PROTO_VER_V2,
                                                NGTCP2_PROTO_VER_V1};

  settings.preferred_versions = kSupportedVersions;
  settings.preferred_versionslen = std::size(kSupportedVersions);
  settings.available_versions = kSupportedVersions;
  settings.available_versionslen = std::size(kSupportedVersions);

  // TODO(@jasnell): Path MTU Discovery is disabled because libuv does not
  // currently expose the IP_DONTFRAG / IP_MTU_DISCOVER socket options
  // needed for PMTUD probes to work correctly. Revisit when libuv adds
  // support or if we bypass libuv for the UDP socket options.
  settings.no_pmtud = 1;
  // Per the ngtcp2 documentation, when no_tx_udp_payload_size_shaping is set
  // to a non-zero value, it tells ngtcp2 not to limit the UDP payload size to
  // NGTCP2_MAX_UDP_PAYLOAD_SIZE` and will instead "use the minimum size among
  // the given buffer size, :member:`max_tx_udp_payload_size`, and the
  // received max_udp_payload_size QUIC transport parameter." For now, this
  // works for us, especially since we do not implement Path MTU discovery.
  settings.no_tx_udp_payload_size_shaping = 1;
  settings.max_tx_udp_payload_size = options.max_payload_size;

  settings.tokenlen = 0;
  settings.token = nullptr;

  if (options.qlog) {
    settings.qlog_write = on_qlog_write;
  }

  if (env->enabled_debug_list()->enabled(DebugCategory::NGTCP2)) {
    settings.log_printf = ngtcp2_debug_log;
  }

  settings.handshake_timeout = options.handshake_timeout;
  settings.max_stream_window = options.max_stream_window;
  settings.max_window = options.max_window;
  settings.ack_thresh = options.unacknowledged_packet_threshold;
  settings.cc_algo = options.cc_algorithm;

  if (side == Side::CLIENT && options.token.has_value()) {
    ngtcp2_vec vec = options.token.value();
    set_token(vec.base, vec.len, NGTCP2_TOKEN_TYPE_NEW_TOKEN);
  }
}

Session::Config::Config(Environment* env,
                        const Options& options,
                        const SocketAddress& local_address,
                        const SocketAddress& remote_address,
                        const CID& ocid)
    : Config(env,
             Side::CLIENT,
             options,
             options.version,
             local_address,
             remote_address,
             CID::Factory::random().Generate(NGTCP2_MIN_INITIAL_DCIDLEN),
             options.cid_factory->Generate(),
             ocid) {}

void Session::Config::MemoryInfo(MemoryTracker* tracker) const {
  tracker->TrackField("options", options);
  tracker->TrackField("local_address", local_address);
  tracker->TrackField("remote_address", remote_address);
  tracker->TrackField("dcid", dcid);
  tracker->TrackField("scid", scid);
  tracker->TrackField("ocid", ocid);
  tracker->TrackField("retry_scid", retry_scid);
}

void Session::Config::set_token(const uint8_t* token,
                                size_t len,
                                ngtcp2_token_type type) {
  settings.token = token;
  settings.tokenlen = len;
  settings.token_type = type;
}

void Session::Config::set_token(const RetryToken& token) {
  ngtcp2_vec vec = token;
  set_token(vec.base, vec.len, NGTCP2_TOKEN_TYPE_RETRY);
}

void Session::Config::set_token(const RegularToken& token) {
  ngtcp2_vec vec = token;
  set_token(vec.base, vec.len, NGTCP2_TOKEN_TYPE_NEW_TOKEN);
}

std::string Session::Config::ToString() const {
  DebugIndentScope indent;
  auto prefix = indent.Prefix();
  std::string res("{");
  res += prefix + "side: " + to_string(side);
  res += prefix + "options: " + options.ToString();
  res += prefix + "version: " + std::to_string(version);
  res += prefix + "local address: " + local_address.ToString();
  res += prefix + "remote address: " + remote_address.ToString();
  res += prefix + "dcid: " + dcid.ToString();
  res += prefix + "scid: " + scid.ToString();
  res += prefix + "ocid: " + ocid.ToString();
  res += prefix + "retry scid: " + retry_scid.ToString();
  res += prefix + "preferred address cid: " + preferred_address_cid.ToString();
  res += indent.Close();
  return res;
}

// ============================================================================

Maybe<Session::Options> Session::Options::From(Environment* env,
                                               Local<Value> value) {
  if (value.IsEmpty() || !value->IsObject()) {
    THROW_ERR_INVALID_ARG_TYPE(env, "options must be an object");
    return Nothing<Options>();
  }

  auto& state = BindingData::Get(env);
  auto params = value.As<Object>();
  Options options;

#define SET(name)                                                              \
  SetOption<Session::Options, &Session::Options::name>(                        \
      env, &options, params, state.name##_string())

  if (!SET(version) || !SET(min_version) || !SET(preferred_address_strategy) ||
      !SET(transport_params) || !SET(tls_options) || !SET(qlog) ||
      !SET(handshake_timeout) || !SET(keep_alive_timeout) ||
      !SET(max_stream_window) || !SET(max_window) || !SET(max_payload_size) ||
      !SET(unacknowledged_packet_threshold) || !SET(cc_algorithm) ||
      !SET(draining_period_multiplier) || !SET(max_datagram_send_attempts)) {
    return Nothing<Options>();
  }

#undef SET

  // RFC 9000 Section 10.2 requires the draining period to be at least 3x PTO.
  static const uint8_t kMinDrainingPeriodMultiplier = 3;
  options.draining_period_multiplier = std::max(
      options.draining_period_multiplier, kMinDrainingPeriodMultiplier);

  // At least 1 send attempt is required.
  options.max_datagram_send_attempts =
      std::max(options.max_datagram_send_attempts, static_cast<uint8_t>(1));

  // Parse the datagram drop policy from a string option.
  {
    Local<Value> policy_val;
    if (params->Get(env->context(), state.datagram_drop_policy_string())
            .ToLocal(&policy_val) &&
        !policy_val->IsUndefined()) {
      Utf8Value policy_str(env->isolate(), policy_val);
      if (strcmp(*policy_str, "drop-newest") == 0) {
        options.datagram_drop_policy = DatagramDropPolicy::DROP_NEWEST;
      }
      // Default is DROP_OLDEST, no need to check for "drop-oldest".
    }
  }

  // Parse the application-specific options (HTTP/3 qpack settings, etc.).
  // These are used if the negotiated ALPN selects Http3ApplicationImpl.
  {
    Local<Value> app_val;
    if (params->Get(env->context(), state.application_string())
            .ToLocal(&app_val) &&
        !app_val->IsUndefined()) {
      if (!Application_Options::From(env, app_val)
               .To(&options.application_options)) {
        return Nothing<Options>();
      }
    }
  }

  // Parse the SNI map from the tls options.
  {
    Local<Value> tls_val;
    if (params->Get(env->context(), state.tls_options_string())
            .ToLocal(&tls_val) &&
        tls_val->IsObject()) {
      Local<Value> sni_val;
      if (tls_val.As<Object>()
              ->Get(env->context(), state.sni_string())
              .ToLocal(&sni_val) &&
          sni_val->IsObject()) {
        auto sni_obj = sni_val.As<Object>();
        Local<Array> hostnames;
        if (sni_obj->GetOwnPropertyNames(env->context()).ToLocal(&hostnames)) {
          for (uint32_t i = 0; i < hostnames->Length(); i++) {
            Local<Value> key;
            Local<Value> entry_val;
            if (!hostnames->Get(env->context(), i).ToLocal(&key) ||
                !key->IsString() ||
                !sni_obj->Get(env->context(), key).ToLocal(&entry_val)) {
              continue;
            }
            Utf8Value hostname(env->isolate(), key);
            auto entry_options = TLSContext::Options::From(env, entry_val);
            if (entry_options.IsNothing()) {
              return Nothing<Options>();
            }
            options.sni[std::string(*hostname, hostname.length())] =
                entry_options.FromJust();
          }
        }
      }
    }
  }

  // TODO(@jasnell): Later we will also support setting the CID::Factory.
  // For now, we're just using the default random factory.

  // Parse the optional NEW_TOKEN for address validation on reconnection.
  Local<Value> token_val;
  if (params->Get(env->context(), state.token_string()).ToLocal(&token_val) &&
      token_val->IsArrayBufferView()) {
    Store token_store;
    if (Store::From(token_val.As<ArrayBufferView>()).To(&token_store)) {
      options.token = std::move(token_store);
    }
  }

  return Just<Options>(options);
}

void Session::Options::MemoryInfo(MemoryTracker* tracker) const {
  tracker->TrackField("transport_params", transport_params);
  tracker->TrackField("crypto_options", tls_options);
  tracker->TrackField("cid_factory_ref", cid_factory_ref);
}

std::string Session::Options::ToString() const {
  DebugIndentScope indent;
  auto prefix = indent.Prefix();
  std::string res("{");
  res += prefix + "version: " + std::to_string(version);
  res += prefix + "min version: " + std::to_string(min_version);
  res += prefix +
         "preferred address policy: " + to_string(preferred_address_strategy);
  res += prefix + "transport params: " + transport_params.ToString();
  res += prefix + "crypto options: " + tls_options.ToString();
  if (qlog) {
    res += prefix + "qlog: yes";
  }
  if (handshake_timeout == UINT64_MAX) {
    res += prefix + "handshake timeout: <none>";
  } else {
    res += prefix + "handshake timeout: " + std::to_string(handshake_timeout) +
           " nanoseconds";
  }
  res += prefix + "max stream window: " + std::to_string(max_stream_window);
  res += prefix + "max window: " + std::to_string(max_window);
  res += prefix + "max payload size: " + std::to_string(max_payload_size);
  if (unacknowledged_packet_threshold != 0) {
    res += prefix + "unacknowledged packet threshold: " +
           std::to_string(unacknowledged_packet_threshold);
  } else {
    res += prefix + "unacknowledged packet threshold: <default>";
  }
  res += prefix + "cc algorithm: " + to_string(cc_algorithm);
  res += indent.Close();
  return res;
}

// ============================================================================
// ngtcp2 static callback functions

// Utility used only within Session::Impl to reduce boilerplate
// Resolves the Session* from ngtcp2 callback arguments. The
// NgTcp2CallbackScope is NOT created here — it is placed at the
// ngtcp2 entry points (Receive, OnTimeout) so that the deferred
// destroy only fires after all callbacks for that call have completed.
#define NGTCP2_CALLBACK_SCOPE(name)                                            \
  auto name = Impl::From(conn, user_data);                                     \
  if (name == nullptr) return NGTCP2_ERR_CALLBACK_FAILURE;

// Session::Impl maintains most of the internal state of an active Session.
struct Session::Impl final : public MemoryRetainer {
  Session* session_;
  AliasedStruct<Stats> stats_;
  AliasedStruct<State> state_;
  BaseObjectWeakPtr<Endpoint> endpoint_;
  Config config_;
  SocketAddress local_address_;
  SocketAddress remote_address_;
  std::unique_ptr<Application> application_;
  StreamsMap streams_;
  TimerWrapHandle timer_;
  size_t send_scope_depth_ = 0;
  QuicError last_error_;

  // Datagrams queued for sending. Serialized into packets by
  // SendPendingData alongside stream data.
  std::deque<Session::PendingDatagram> pending_datagrams_;
  PendingStream::PendingStreamQueue pending_bidi_stream_queue_;
  PendingStream::PendingStreamQueue pending_uni_stream_queue_;

  // Session ticket app data parsed before ALPN negotiation.
  // Validated and applied in SetApplication() after ALPN selects
  // the application type.
  std::optional<PendingTicketAppData> pending_ticket_data_;

  // When true, the handshake is deferred until the first stream or
  // datagram is sent. This is set for client sessions with a session
  // ticket, enabling 0-RTT: the first send triggers the handshake
  // and the stream/datagram data is included in the 0-RTT flight.
  bool handshake_deferred_ = false;

  Impl(Session* session, Endpoint* endpoint, const Config& config)
      : session_(session),
        stats_(env()->isolate()),
        state_(env()->isolate()),
        endpoint_(endpoint),
        config_(config),
        local_address_(config.local_address),
        remote_address_(config.remote_address),
        timer_(session_->env(), [this] { session_->OnTimeout(); }) {
    timer_.Unref();
  }
  DISALLOW_COPY_AND_MOVE(Impl)

  inline bool is_closing() const { return state_->closing; }

  ~Impl() {
    // Ensure that Close() was called before dropping
    DCHECK(is_closing());
    DCHECK(endpoint_);

    // Removing the session from the endpoint may cause the endpoint to be
    // destroyed if it is waiting on the last session to be destroyed. Let's
    // grab a reference just to be safe for the rest of the function.
    BaseObjectPtr<Endpoint> endpoint = endpoint_;
    endpoint_.reset();

    MaybeStackBuffer<ngtcp2_cid, 10> cids(
        ngtcp2_conn_get_scid(*session_, nullptr));
    ngtcp2_conn_get_scid(*session_, cids.out());

    MaybeStackBuffer<ngtcp2_cid_token2, 10> tokens(
        ngtcp2_conn_get_active_dcid2(*session_, nullptr));
    ngtcp2_conn_get_active_dcid2(*session_, tokens.out());

    endpoint->DisassociateCID(config_.dcid);
    endpoint->DisassociateCID(config_.preferred_address_cid);

    for (size_t n = 0; n < cids.length(); n++) {
      endpoint->DisassociateCID(CID(cids[n]));
    }

    for (size_t n = 0; n < tokens.length(); n++) {
      if (tokens[n].token_present) {
        endpoint->DisassociateStatelessResetToken(
            StatelessResetToken(&tokens[n].token));
      }
    }

    endpoint->RemoveSession(config_.scid, remote_address_);
  }

  void MemoryInfo(MemoryTracker* tracker) const override {
    tracker->TrackField("config", config_);
    tracker->TrackField("endpoint", endpoint_);
    tracker->TrackField("streams", streams_);
    tracker->TrackField("local_address", local_address_);
    tracker->TrackField("remote_address", remote_address_);
    tracker->TrackField("application", application_);
    tracker->TrackField("timer", timer_);
  }
  SET_SELF_SIZE(Impl)
  SET_MEMORY_INFO_NAME(Session::Impl)

  Environment* env() const { return session_->env(); }

  // Gets the Session pointer from the user_data void pointer
  // provided by ngtcp2.
  static Session* From(ngtcp2_conn* conn, void* user_data) {
    if (user_data == nullptr) [[unlikely]] {
      return nullptr;
    }
    auto session = static_cast<Session*>(user_data);
    if (session->is_destroyed()) [[unlikely]] {
      return nullptr;
    }
    return session;
  }

  // JavaScript APIs

  // TODO(@jasnell): Fast API alternatives for each of these

  // Parse optional close error code options: { code, type, reason }
  // Returns true on success (including when no options were provided).
  // Returns false on validation error (exception thrown).
  // Sets *did_set to true if an error code was actually applied.
  static bool MaybeSetCloseError(const FunctionCallbackInfo<Value>& args,
                                 int options_index,
                                 Session* session,
                                 bool* did_set = nullptr) {
    if (did_set) *did_set = false;
    auto env = Environment::GetCurrent(args);
    if (args.Length() <= options_index || args[options_index]->IsUndefined()) {
      return true;
    }
    if (!args[options_index]->IsObject()) {
      THROW_ERR_INVALID_ARG_TYPE(env, "options must be an object");
      return false;
    }
    auto options = args[options_index].As<Object>();
    auto& state = BindingData::Get(env);
    auto context = env->context();

    // code: bigint (optional)
    Local<Value> code_val;
    if (!options->Get(context, state.code_string()).ToLocal(&code_val)) {
      return false;
    }
    if (code_val->IsUndefined()) return true;

    uint64_t code;
    if (code_val->IsBigInt()) {
      bool lossless;
      code = code_val.As<BigInt>()->Uint64Value(&lossless);
      if (!lossless) {
        THROW_ERR_INVALID_ARG_VALUE(env, "options.code is too large");
        return false;
      }
    } else if (code_val->IsNumber()) {
      code = static_cast<uint64_t>(code_val.As<Number>()->Value());
    } else {
      THROW_ERR_INVALID_ARG_TYPE(env,
                                 "options.code must be a bigint or number");
      return false;
    }

    // type: string (optional, default 'transport')
    Local<Value> type_val;
    if (!options->Get(context, state.type_string()).ToLocal(&type_val)) {
      return false;
    }
    bool is_application = false;
    if (!type_val->IsUndefined()) {
      if (type_val->StrictEquals(state.application_string())) {
        is_application = true;
      } else if (!type_val->StrictEquals(state.transport_string())) {
        THROW_ERR_INVALID_ARG_VALUE(
            env, "options.type must be 'transport' or 'application'");
        return false;
      }
    }

    // reason: string (optional)
    std::string reason;
    Local<Value> reason_val;
    if (!options->Get(context, state.reason_string()).ToLocal(&reason_val)) {
      return false;
    }
    if (!reason_val->IsUndefined()) {
      Utf8Value reason_str(env->isolate(), reason_val);
      reason = std::string(*reason_str, reason_str.length());
    }

    if (is_application) {
      session->SetLastError(QuicError::ForApplication(code, std::move(reason)));
    } else {
      session->SetLastError(QuicError::ForTransport(code, std::move(reason)));
    }
    if (did_set) *did_set = true;
    return true;
  }

  JS_METHOD(Destroy) {
    auto env = Environment::GetCurrent(args);
    Session* session;
    ASSIGN_OR_RETURN_UNWRAP(&session, args.This());
    if (session->is_destroyed()) {
      // At this layer, Destroy should only be called once. At the
      // JavaScript layer calling destroy() multiple times should be
      // an idempotent operation. Be sure to check for that there
      // as we strictly enforce it here.
      return THROW_ERR_INVALID_STATE(env, "Session is destroyed");
    }
    // args[0] is the optional close error options object.
    bool has_close_options = false;
    if (!MaybeSetCloseError(args, 0, session, &has_close_options)) return;
    // If an error code was provided by the caller, send CONNECTION_CLOSE
    // with that code before destroying. SendConnectionClose writes the
    // packet and hands it to the endpoint — it doesn't wait for ack.
    if (has_close_options) {
      session->SendConnectionClose();
    }
    session->Destroy();
  }

  JS_METHOD(GetRemoteAddress) {
    auto env = Environment::GetCurrent(args);
    Session* session;
    ASSIGN_OR_RETURN_UNWRAP(&session, args.This());

    if (session->is_destroyed()) {
      return THROW_ERR_INVALID_STATE(env, "Session is destroyed");
    }

    auto address = session->remote_address();
    args.GetReturnValue().Set(
        SocketAddressBase::Create(env, std::make_shared<SocketAddress>(address))
            ->object());
  }

  JS_METHOD(GetLocalAddress) {
    auto env = Environment::GetCurrent(args);
    Session* session;
    ASSIGN_OR_RETURN_UNWRAP(&session, args.This());

    if (session->is_destroyed()) {
      return THROW_ERR_INVALID_STATE(env, "Session is destroyed");
    }

    auto address = session->local_address();
    args.GetReturnValue().Set(
        SocketAddressBase::Create(env, std::make_shared<SocketAddress>(address))
            ->object());
  }

  JS_METHOD(GetCertificate) {
    auto env = Environment::GetCurrent(args);
    Session* session;
    ASSIGN_OR_RETURN_UNWRAP(&session, args.This());

    if (session->is_destroyed()) {
      return THROW_ERR_INVALID_STATE(env, "Session is destroyed");
    }

    Local<Value> ret;
    if (session->tls_session().cert(env).ToLocal(&ret))
      args.GetReturnValue().Set(ret);
  }

  JS_METHOD(GetEphemeralKeyInfo) {
    auto env = Environment::GetCurrent(args);
    Session* session;
    ASSIGN_OR_RETURN_UNWRAP(&session, args.This());

    if (session->is_destroyed()) {
      return THROW_ERR_INVALID_STATE(env, "Session is destroyed");
    }

    Local<Object> ret;
    if (!session->is_server() &&
        session->tls_session().ephemeral_key(env).ToLocal(&ret))
      args.GetReturnValue().Set(ret);
  }

  JS_METHOD(GetPeerCertificate) {
    auto env = Environment::GetCurrent(args);
    Session* session;
    ASSIGN_OR_RETURN_UNWRAP(&session, args.This());

    if (session->is_destroyed()) {
      return THROW_ERR_INVALID_STATE(env, "Session is destroyed");
    }

    Local<Value> ret;
    if (session->tls_session().peer_cert(env).ToLocal(&ret))
      args.GetReturnValue().Set(ret);
  }

  JS_METHOD(GracefulClose) {
    auto env = Environment::GetCurrent(args);
    Session* session;
    ASSIGN_OR_RETURN_UNWRAP(&session, args.This());

    if (session->is_destroyed()) {
      return THROW_ERR_INVALID_STATE(env, "Session is destroyed");
    }

    // args[0] is the optional close error options object.
    if (!MaybeSetCloseError(args, 0, session)) return;
    session->Close(CloseMethod::GRACEFUL);
  }

  JS_METHOD(SilentClose) {
    // This is exposed for testing purposes only!
    auto env = Environment::GetCurrent(args);
    Session* session;
    ASSIGN_OR_RETURN_UNWRAP(&session, args.This());

    if (session->is_destroyed()) {
      return THROW_ERR_INVALID_STATE(env, "Session is destroyed");
    }

    session->Close(CloseMethod::SILENT);
  }

  JS_METHOD(UpdateKey) {
    auto env = Environment::GetCurrent(args);
    Session* session;
    ASSIGN_OR_RETURN_UNWRAP(&session, args.This());

    if (session->is_destroyed()) {
      return THROW_ERR_INVALID_STATE(env, "Session is destroyed");
    }

    // Initiating a key update may fail if it is done too early (either
    // before the TLS handshake has been confirmed or while a previous
    // key update is being processed). When it fails, InitiateKeyUpdate()
    // will return false.
    SendPendingDataScope send_scope(session);
    args.GetReturnValue().Set(session->tls_session().InitiateKeyUpdate());
  }

  JS_METHOD(OpenStream) {
    auto env = Environment::GetCurrent(args);
    Session* session;
    ASSIGN_OR_RETURN_UNWRAP(&session, args.This());

    if (session->is_destroyed()) [[unlikely]] {
      return THROW_ERR_INVALID_STATE(env, "Session is destroyed");
    }

    DCHECK(args[0]->IsUint32());

    // GetDataQueueFromSource handles type validation.
    std::shared_ptr<DataQueue> data_source;
    if (!Stream::GetDataQueueFromSource(env, args[1]).To(&data_source))
        [[unlikely]] {
      return THROW_ERR_INVALID_ARG_VALUE(env, "Invalid data source");
    }

    session->impl_->handshake_deferred_ = false;
    SendPendingDataScope send_scope(session);
    auto direction = FromV8Value<Direction>(args[0]);
    Local<Object> stream;
    if (session->OpenStream(direction, std::move(data_source)).ToLocal(&stream))
        [[likely]] {
      args.GetReturnValue().Set(stream);
    }
  }

  JS_METHOD(SendDatagram) {
    auto env = Environment::GetCurrent(args);
    Session* session;
    ASSIGN_OR_RETURN_UNWRAP(&session, args.This());

    if (session->is_destroyed()) {
      return THROW_ERR_INVALID_STATE(env, "Session is destroyed");
    }

    DCHECK(args[0]->IsArrayBufferView());
    session->impl_->handshake_deferred_ = false;
    SendPendingDataScope send_scope(session);

    Store store;
    if (!Store::From(args[0].As<ArrayBufferView>()).To(&store)) {
      return;
    }

    args.GetReturnValue().Set(
        BigInt::New(env->isolate(), session->SendDatagram(std::move(store))));
  }

  // Internal ngtcp2 callbacks

  static int on_acknowledge_stream_data_offset(ngtcp2_conn* conn,
                                               stream_id stream_id,
                                               uint64_t offset,
                                               uint64_t datalen,
                                               void* user_data,
                                               void* stream_user_data) {
    NGTCP2_CALLBACK_SCOPE(session)
    // The callback will be invoked with datalen 0 if a zero-length
    // stream frame with fin flag set is received. In that case, let's
    // just ignore it.
    // Per ngtcp2, the range of bytes that are being acknowledged here
    // are `[offset, offset + datalen]` but we only really care about
    // the datalen as our accounting does not track the offset and
    // acknowledges should never come out of order here.
    if (datalen == 0) return NGTCP2_SUCCESS;
    return session->application().AcknowledgeStreamData(stream_id, datalen)
               ? NGTCP2_SUCCESS
               : NGTCP2_ERR_CALLBACK_FAILURE;
  }

  static int on_acknowledge_datagram(ngtcp2_conn* conn,
                                     datagram_id dgram_id,
                                     void* user_data) {
    NGTCP2_CALLBACK_SCOPE(session)
    session->DatagramStatus(dgram_id, DatagramStatus::ACKNOWLEDGED);
    return NGTCP2_SUCCESS;
  }

  static int on_cid_status(ngtcp2_conn* conn,
                           ngtcp2_connection_id_status_type type,
                           uint64_t seq,
                           const ngtcp2_cid* cid,
                           const ngtcp2_stateless_reset_token* token,
                           void* user_data) {
    NGTCP2_CALLBACK_SCOPE(session)
    std::optional<StatelessResetToken> maybe_reset_token;
    if (token != nullptr) maybe_reset_token.emplace(token);
    auto& endpoint = session->endpoint();
    switch (type) {
      case NGTCP2_CONNECTION_ID_STATUS_TYPE_ACTIVATE: {
        if (token != nullptr) {
          endpoint.AssociateStatelessResetToken(StatelessResetToken(token),
                                                session);
        }
        break;
      }
      case NGTCP2_CONNECTION_ID_STATUS_TYPE_DEACTIVATE: {
        if (token != nullptr) {
          endpoint.DisassociateStatelessResetToken(StatelessResetToken(token));
        }
        break;
      }
    }
    return NGTCP2_SUCCESS;
  }

  static int on_extend_max_remote_streams_bidi(ngtcp2_conn* conn,
                                               uint64_t max_streams,
                                               void* user_data) {
    NGTCP2_CALLBACK_SCOPE(session)
    Debug(
        session, "Max remote bidi streams increased to %" PRIu64, max_streams);
    return NGTCP2_SUCCESS;
  }

  static int on_extend_max_remote_streams_uni(ngtcp2_conn* conn,
                                              uint64_t max_streams,
                                              void* user_data) {
    NGTCP2_CALLBACK_SCOPE(session)
    Debug(session, "Max remote uni streams increased to %" PRIu64, max_streams);
    return NGTCP2_SUCCESS;
  }

  static int on_extend_max_streams_bidi(ngtcp2_conn* conn,
                                        uint64_t max_streams,
                                        void* user_data) {
    NGTCP2_CALLBACK_SCOPE(session)
    session->ProcessPendingBidiStreams();
    return NGTCP2_SUCCESS;
  }

  static int on_extend_max_streams_uni(ngtcp2_conn* conn,
                                       uint64_t max_streams,
                                       void* user_data) {
    NGTCP2_CALLBACK_SCOPE(session)
    session->ProcessPendingUniStreams();
    return NGTCP2_SUCCESS;
  }

  static int on_extend_max_stream_data(ngtcp2_conn* conn,
                                       stream_id stream_id,
                                       uint64_t max_data,
                                       void* user_data,
                                       void* stream_user_data) {
    NGTCP2_CALLBACK_SCOPE(session)
    if (auto* stream = Stream::From(stream_user_data)) {
      session->application().ExtendMaxStreamData(stream, max_data);
    }
    return NGTCP2_SUCCESS;
  }

  static int on_get_new_cid(ngtcp2_conn* conn,
                            ngtcp2_cid* cid,
                            ngtcp2_stateless_reset_token* token,
                            size_t cidlen,
                            void* user_data) {
    NGTCP2_CALLBACK_SCOPE(session)
    session->GenerateNewConnectionId(cid, cidlen, token);
    return NGTCP2_SUCCESS;
  }

  static int on_handshake_completed(ngtcp2_conn* conn, void* user_data) {
    NGTCP2_CALLBACK_SCOPE(session)
    return session->HandshakeCompleted() ? NGTCP2_SUCCESS
                                         : NGTCP2_ERR_CALLBACK_FAILURE;
  }

  static int on_handshake_confirmed(ngtcp2_conn* conn, void* user_data) {
    NGTCP2_CALLBACK_SCOPE(session)
    session->HandshakeConfirmed();
    return NGTCP2_SUCCESS;
  }

  static int on_lost_datagram(ngtcp2_conn* conn,
                              datagram_id dgram_id,
                              void* user_data) {
    NGTCP2_CALLBACK_SCOPE(session)
    session->DatagramStatus(dgram_id, DatagramStatus::LOST);
    return NGTCP2_SUCCESS;
  }

  static int on_path_validation(ngtcp2_conn* conn,
                                uint32_t flags,
                                const ngtcp2_path* path,
                                const ngtcp2_path* old_path,
                                ngtcp2_path_validation_result res,
                                void* user_data) {
    NGTCP2_CALLBACK_SCOPE(session)
    bool flag_preferred_address =
        flags & NGTCP2_PATH_VALIDATION_FLAG_PREFERRED_ADDR;
    ValidatedPath newValidatedPath{
        std::make_shared<SocketAddress>(path->local.addr),
        std::make_shared<SocketAddress>(path->remote.addr)};
    std::optional<ValidatedPath> oldValidatedPath = std::nullopt;
    if (old_path != nullptr) {
      oldValidatedPath =
          ValidatedPath{std::make_shared<SocketAddress>(old_path->local.addr),
                        std::make_shared<SocketAddress>(old_path->remote.addr)};
    }
    session->EmitPathValidation(static_cast<PathValidationResult>(res),
                                PathValidationFlags{flag_preferred_address},
                                newValidatedPath,
                                oldValidatedPath);
    return NGTCP2_SUCCESS;
  }

  static int on_receive_datagram(ngtcp2_conn* conn,
                                 uint32_t flags,
                                 const uint8_t* data,
                                 size_t datalen,
                                 void* user_data) {
    NGTCP2_CALLBACK_SCOPE(session)
    session->DatagramReceived(
        data,
        datalen,
        DatagramReceivedFlags{
            .early = (flags & NGTCP2_DATAGRAM_FLAG_0RTT) ==
                     NGTCP2_DATAGRAM_FLAG_0RTT,
        });
    return NGTCP2_SUCCESS;
  }

  static int on_receive_new_token(ngtcp2_conn* conn,
                                  const uint8_t* token,
                                  size_t tokenlen,
                                  void* user_data) {
    NGTCP2_CALLBACK_SCOPE(session)
    Debug(session, "Received NEW_TOKEN (%zu bytes)", tokenlen);
    session->EmitNewToken(token, tokenlen);
    return NGTCP2_SUCCESS;
  }

  static int on_receive_rx_key(ngtcp2_conn* conn,
                               ngtcp2_encryption_level level,
                               void* user_data) {
    NGTCP2_CALLBACK_SCOPE(session)
    CHECK(!session->is_server());

    if (level != NGTCP2_ENCRYPTION_LEVEL_1RTT) return NGTCP2_SUCCESS;

    // If the application was already started via on_receive_tx_key
    // (0-RTT path), this is a no-op.
    if (session->application().is_started()) return NGTCP2_SUCCESS;

    Debug(session,
          "Receiving RX key for level %s for dcid %s",
          to_string(level),
          session->config().dcid);

    return session->application().Start() ? NGTCP2_SUCCESS
                                          : NGTCP2_ERR_CALLBACK_FAILURE;
  }

  static int on_receive_stateless_reset(ngtcp2_conn* conn,
                                        const ngtcp2_pkt_stateless_reset2* sr,
                                        void* user_data) {
    NGTCP2_CALLBACK_SCOPE(session)
    Debug(session, "Received stateless reset from peer");
    // This callback is informational. ngtcp2 has already set the
    // connection state to NGTCP2_CS_DRAINING before invoking this
    // callback, and ngtcp2_conn_read_pkt will return
    // NGTCP2_ERR_DRAINING. The actual close handling happens in
    // Session::Receive when it processes that return value and
    // checks this flag.
    session->impl_->state_->stateless_reset = 1;
    return NGTCP2_SUCCESS;
  }

  static int on_receive_stream_data(ngtcp2_conn* conn,
                                    uint32_t flags,
                                    stream_id stream_id,
                                    uint64_t offset,
                                    const uint8_t* data,
                                    size_t datalen,
                                    void* user_data,
                                    void* stream_user_data) {
    NGTCP2_CALLBACK_SCOPE(session)
    Stream::ReceiveDataFlags data_flags{
        // The fin flag indicates that this is the last chunk of data we will
        // receive on this stream.
        .fin = (flags & NGTCP2_STREAM_DATA_FLAG_FIN) ==
               NGTCP2_STREAM_DATA_FLAG_FIN,
        // Stream data is early if it is received before the TLS handshake is
        // complete.
        .early = (flags & NGTCP2_STREAM_DATA_FLAG_0RTT) ==
                 NGTCP2_STREAM_DATA_FLAG_0RTT,
    };

    // We received data for a stream! What we don't know yet at this point
    // is whether the application wants us to treat this as a control stream
    // data (something the application will handle on its own) or a user stream
    // data (something that we should create a Stream handle for that is passed
    // out to JavaScript). HTTP3, for instance, will generally create three
    // control stream in either direction and we want to make sure those are
    // never exposed to users and that we don't waste time creating Stream
    // handles for them. So, what we do here is pass the stream data on to the
    // application for processing. If it ends up being a user stream, the
    // application will handle creating the Stream handle and passing that off
    // to the JavaScript side.
    if (!session->application().ReceiveStreamData(
            stream_id, data, datalen, data_flags, stream_user_data)) {
      return NGTCP2_ERR_CALLBACK_FAILURE;
    }

    return NGTCP2_SUCCESS;
  }

  static int on_receive_tx_key(ngtcp2_conn* conn,
                               ngtcp2_encryption_level level,
                               void* user_data) {
    NGTCP2_CALLBACK_SCOPE(session);

    // For SERVER: fires at 1RTT — start the application after handshake.
    // For CLIENT: fires at 0RTT — start the application early so that
    //   HTTP/3 control/QPACK streams are bound before 0-RTT requests.
    //   Without this, nghttp3_conn_submit_request asserts because the
    //   QPACK encoder stream isn't bound yet.
    if (session->is_server()) {
      if (level != NGTCP2_ENCRYPTION_LEVEL_1RTT) return NGTCP2_SUCCESS;
    } else {
      if (level != NGTCP2_ENCRYPTION_LEVEL_0RTT) return NGTCP2_SUCCESS;
    }

    // application_ may be null if ALPN selection hasn't happened yet
    // (e.g., ALPN mismatch causes the handshake to fail during key
    // installation). Without an application, we can't start.
    if (!session->impl_->application_) return NGTCP2_ERR_CALLBACK_FAILURE;

    Debug(session,
          "Receiving TX key for level %s for dcid %s",
          to_string(level),
          session->config().dcid);
    return session->application().Start() ? NGTCP2_SUCCESS
                                          : NGTCP2_ERR_CALLBACK_FAILURE;
  }

  static int on_receive_version_negotiation(ngtcp2_conn* conn,
                                            const ngtcp2_pkt_hd* hd,
                                            const uint32_t* sv,
                                            size_t nsv,
                                            void* user_data) {
    NGTCP2_CALLBACK_SCOPE(session)
    session->EmitVersionNegotiation(*hd, sv, nsv);
    return NGTCP2_SUCCESS;
  }

  static int on_remove_connection_id(ngtcp2_conn* conn,
                                     const ngtcp2_cid* cid,
                                     void* user_data) {
    NGTCP2_CALLBACK_SCOPE(session)
    session->endpoint().DisassociateCID(CID(cid));
    return NGTCP2_SUCCESS;
  }

  static int on_select_preferred_address(ngtcp2_conn* conn,
                                         ngtcp2_path* dest,
                                         const ngtcp2_preferred_addr* paddr,
                                         void* user_data) {
    NGTCP2_CALLBACK_SCOPE(session)
    PreferredAddress preferred_address(dest, paddr);
    session->SelectPreferredAddress(&preferred_address);
    return NGTCP2_SUCCESS;
  }

  static int on_stream_close(ngtcp2_conn* conn,
                             uint32_t flags,
                             stream_id stream_id,
                             error_code app_error_code,
                             void* user_data,
                             void* stream_user_data) {
    NGTCP2_CALLBACK_SCOPE(session)
    auto* stream = Stream::From(stream_user_data);
    if (stream == nullptr) return NGTCP2_SUCCESS;
    if (flags & NGTCP2_STREAM_CLOSE_FLAG_APP_ERROR_CODE_SET) {
      session->application().ReceiveStreamClose(
          stream, QuicError::ForApplication(app_error_code));
    } else {
      session->application().ReceiveStreamClose(stream);
    }
    return NGTCP2_SUCCESS;
  }

  static int on_stream_open(ngtcp2_conn* conn, stream_id id, void* user_data) {
    NGTCP2_CALLBACK_SCOPE(session)
    if (!session->application().ReceiveStreamOpen(id)) {
      return NGTCP2_ERR_CALLBACK_FAILURE;
    }
    return NGTCP2_SUCCESS;
  }

  static int on_stream_reset(ngtcp2_conn* conn,
                             stream_id stream_id,
                             uint64_t final_size,
                             error_code app_error_code,
                             void* user_data,
                             void* stream_user_data) {
    NGTCP2_CALLBACK_SCOPE(session)
    auto* stream = Stream::From(stream_user_data);
    if (stream == nullptr) return NGTCP2_SUCCESS;
    session->application().ReceiveStreamReset(
        stream, final_size, QuicError::ForApplication(app_error_code));
    return NGTCP2_SUCCESS;
  }

  static int on_stream_stop_sending(ngtcp2_conn* conn,
                                    stream_id stream_id,
                                    error_code app_error_code,
                                    void* user_data,
                                    void* stream_user_data) {
    NGTCP2_CALLBACK_SCOPE(session)
    auto* stream = Stream::From(stream_user_data);
    if (stream == nullptr) return NGTCP2_SUCCESS;
    session->application().ReceiveStreamStopSending(
        stream, QuicError::ForApplication(app_error_code));
    return NGTCP2_SUCCESS;
  }

  static void on_rand(uint8_t* dest,
                      size_t destlen,
                      const ngtcp2_rand_ctx* rand_ctx) {
    CHECK(ncrypto::CSPRNG(dest, destlen));
  }

  static int on_early_data_rejected(ngtcp2_conn* conn, void* user_data) {
    auto session = Impl::From(conn, user_data);
    if (session == nullptr) return NGTCP2_ERR_CALLBACK_FAILURE;
    Debug(session, "Early data was rejected");
    if (session->impl_->application_) {
      session->application().EarlyDataRejected();
    }
    return NGTCP2_SUCCESS;
  }

  static int on_begin_path_validation(ngtcp2_conn* conn,
                                      uint32_t flags,
                                      const ngtcp2_path* path,
                                      const ngtcp2_path* fallback_path,
                                      void* user_data) {
    NGTCP2_CALLBACK_SCOPE(session)
    Debug(session, "Path validation started");
    return NGTCP2_SUCCESS;
  }

  static constexpr ngtcp2_callbacks CLIENT = {
      ngtcp2_crypto_client_initial_cb,
      nullptr,
      ngtcp2_crypto_recv_crypto_data_cb,
      on_handshake_completed,
      on_receive_version_negotiation,
      ngtcp2_crypto_encrypt_cb,
      ngtcp2_crypto_decrypt_cb,
      ngtcp2_crypto_hp_mask_cb,
      on_receive_stream_data,
      on_acknowledge_stream_data_offset,
      on_stream_open,
      on_stream_close,
      nullptr,  // recv_stateless_reset (deprecated, use v2 below)
      ngtcp2_crypto_recv_retry_cb,
      on_extend_max_streams_bidi,
      on_extend_max_streams_uni,
      on_rand,
      nullptr,  // get_new_connection_id (deprecated, use v2 below)
      on_remove_connection_id,
      ngtcp2_crypto_update_key_cb,
      on_path_validation,
      on_select_preferred_address,
      on_stream_reset,
      on_extend_max_remote_streams_bidi,
      on_extend_max_remote_streams_uni,
      on_extend_max_stream_data,
      nullptr,  // dcid_status (deprecated, use v2 below)
      on_handshake_confirmed,
      on_receive_new_token,
      ngtcp2_crypto_delete_crypto_aead_ctx_cb,
      ngtcp2_crypto_delete_crypto_cipher_ctx_cb,
      on_receive_datagram,
      on_acknowledge_datagram,
      on_lost_datagram,
      nullptr,  // get_path_challenge_data (deprecated, use v2 below)
      on_stream_stop_sending,
      ngtcp2_crypto_version_negotiation_cb,
      on_receive_rx_key,
      on_receive_tx_key,
      on_early_data_rejected,
      on_begin_path_validation,
      on_receive_stateless_reset,
      on_get_new_cid,
      on_cid_status,
      ngtcp2_crypto_get_path_challenge_data2_cb};

  static constexpr ngtcp2_callbacks SERVER = {
      nullptr,
      ngtcp2_crypto_recv_client_initial_cb,
      ngtcp2_crypto_recv_crypto_data_cb,
      on_handshake_completed,
      nullptr,
      ngtcp2_crypto_encrypt_cb,
      ngtcp2_crypto_decrypt_cb,
      ngtcp2_crypto_hp_mask_cb,
      on_receive_stream_data,
      on_acknowledge_stream_data_offset,
      on_stream_open,
      on_stream_close,
      nullptr,  // recv_stateless_reset (deprecated, use v2 below)
      nullptr,
      on_extend_max_streams_bidi,
      on_extend_max_streams_uni,
      on_rand,
      nullptr,  // get_new_connection_id (deprecated, use v2 below)
      on_remove_connection_id,
      ngtcp2_crypto_update_key_cb,
      on_path_validation,
      nullptr,
      on_stream_reset,
      on_extend_max_remote_streams_bidi,
      on_extend_max_remote_streams_uni,
      on_extend_max_stream_data,
      nullptr,  // dcid_status (deprecated, use v2 below)
      nullptr,
      nullptr,
      ngtcp2_crypto_delete_crypto_aead_ctx_cb,
      ngtcp2_crypto_delete_crypto_cipher_ctx_cb,
      on_receive_datagram,
      on_acknowledge_datagram,
      on_lost_datagram,
      nullptr,  // get_path_challenge_data (deprecated, use v2 below)
      on_stream_stop_sending,
      ngtcp2_crypto_version_negotiation_cb,
      nullptr,
      on_receive_tx_key,
      on_early_data_rejected,
      on_begin_path_validation,
      on_receive_stateless_reset,
      on_get_new_cid,
      on_cid_status,
      ngtcp2_crypto_get_path_challenge_data2_cb};
};

#undef NGTCP2_CALLBACK_SCOPE

// ============================================================================
Session::SendPendingDataScope::SendPendingDataScope(Session* session)
    : session(session) {
  CHECK_NOT_NULL(session);
  CHECK(!session->is_destroyed());
  ++session->impl_->send_scope_depth_;
}

Session::SendPendingDataScope::SendPendingDataScope(
    const BaseObjectPtr<Session>& session)
    : session(session.get()) {
  CHECK_NOT_NULL(session);
  CHECK(!session->is_destroyed());
  ++session->impl_->send_scope_depth_;
}

Session::SendPendingDataScope::~SendPendingDataScope() {
  if (session->is_destroyed()) return;
  DCHECK_GE(session->impl_->send_scope_depth_, 1);
  Debug(session, "Send Scope Depth %zu", session->impl_->send_scope_depth_);
  if (--session->impl_->send_scope_depth_ == 0 &&
      session->impl_->application_ && !session->impl_->handshake_deferred_) {
    session->application().SendPendingData();
  }
}

// ============================================================================
BaseObjectPtr<Session> Session::Create(
    Endpoint* endpoint,
    const Config& config,
    TLSContext* tls_context,
    const std::optional<SessionTicket>& ticket) {
  JS_NEW_INSTANCE_OR_RETURN(endpoint->env(), obj, {});
  return MakeDetachedBaseObject<Session>(
      endpoint, obj, config, tls_context, ticket);
}

Session::Session(Endpoint* endpoint,
                 Local<Object> object,
                 const Config& config,
                 TLSContext* tls_context,
                 const std::optional<SessionTicket>& session_ticket)
    : AsyncWrap(endpoint->env(), object, PROVIDER_QUIC_SESSION),
      side_(config.side),
      allocator_(BindingData::Get(env()).ngtcp2_allocator()),
      impl_(std::make_unique<Impl>(this, endpoint, config)),
      connection_(InitConnection()),
      tls_session_(tls_context->NewSession(this, session_ticket)) {
  DCHECK(impl_);
  {
    auto& stats_ = impl_->stats_;
    STAT_RECORD_TIMESTAMP(Stats, created_at);
  }

  // For clients, select the Application immediately — the ALPN is
  // known upfront from the options. For servers, application_ stays
  // null until OnSelectAlpn fires during the TLS handshake.
  if (config.side == Side::CLIENT) {
    auto app =
        SelectApplicationFromAlpn(DecodeAlpn(config.options.tls_options.alpn));
    if (app) SetApplication(std::move(app));
  }

  // For client sessions with a session ticket and early data enabled,
  // defer the handshake until the first stream or datagram is sent.
  // This enables 0-RTT: the stream/datagram data is included in the
  // first flight alongside the ClientHello. When early data is
  // disabled, the handshake starts immediately (no 0-RTT attempt).
  if (config.side == Side::CLIENT && session_ticket.has_value() &&
      config.options.tls_options.enable_early_data) {
    impl_->handshake_deferred_ = true;
  }

  if (config.options.keep_alive_timeout > 0) {
    ngtcp2_conn_set_keep_alive_timeout(
        *this, config.options.keep_alive_timeout * NGTCP2_MILLISECONDS);
  }

  MakeWeak();
  Debug(this, "Session created.");
  auto& binding = BindingData::Get(env());

  JS_DEFINE_READONLY_PROPERTY(
      env(), object, env()->stats_string(), impl_->stats_.GetArrayBuffer());
  JS_DEFINE_READONLY_PROPERTY(
      env(), object, env()->state_string(), impl_->state_.GetArrayBuffer());

  UpdateDataStats();
}

Session::~Session() {
  // Double check that Destroy() was called first.
  CHECK(is_destroyed());
}

Session::QuicConnectionPointer Session::InitConnection() {
  ngtcp2_conn* conn;
  Path path(config().local_address, config().remote_address);
  TransportParams::Config tp_config(side_, config().ocid, config().retry_scid);
  TransportParams transport_params(tp_config,
                                   config().options.transport_params);
  transport_params.GenerateSessionTokens(this);

  switch (side_) {
    case Side::SERVER: {
      // On the server side there are certain transport parameters that are
      // required to be sent. Let's make sure they are set.
      const ngtcp2_transport_params& params = transport_params;
      CHECK_EQ(params.original_dcid_present, 1);
      CHECK_EQ(ngtcp2_conn_server_new(&conn,
                                      config().dcid,
                                      config().scid,
                                      &path,
                                      config().version,
                                      &Impl::SERVER,
                                      &config().settings,
                                      transport_params,
                                      allocator_,
                                      this),
               0);
      break;
    }
    case Side::CLIENT: {
      CHECK_EQ(ngtcp2_conn_client_new(&conn,
                                      config().dcid,
                                      config().scid,
                                      &path,
                                      config().version,
                                      &Impl::CLIENT,
                                      &config().settings,
                                      transport_params,
                                      allocator_,
                                      this),
               0);
      break;
    }
  }
  return QuicConnectionPointer(conn);
}

Session::operator ngtcp2_conn*() const {
  return connection_.get();
}

bool Session::is_server() const {
  return side_ == Side::SERVER;
}

bool Session::is_destroyed() const {
  return !impl_ || destroy_deferred_;
}

bool Session::is_destroyed_or_closing() const {
  return !impl_ || impl_->state_->closing;
}

void Session::Close(CloseMethod method) {
  if (is_destroyed()) return;
  auto& stats_ = impl_->stats_;

  // If the handshake was deferred (0-RTT client that never sent),
  // no packets were ever transmitted. Close silently since there is
  // nothing to communicate to the peer.
  if (impl_->handshake_deferred_) {
    impl_->handshake_deferred_ = false;
    method = CloseMethod::SILENT;
  }

  if (impl_->last_error_) {
    Debug(this, "Closing with error: %s", impl_->last_error_);
  }

  STAT_RECORD_TIMESTAMP(Stats, closing_at);
  impl_->state_->closing = 1;

  // With both the DEFAULT and SILENT options, we will proceed to closing
  // the session immediately. All open streams will be immediately destroyed
  // with whatever is set as the last error. The session will then be destroyed
  // with a possible roundtrip to JavaScript to emit a close event and clean up
  // any JavaScript side state. Importantly, these operations are all done
  // synchronously, so the session will be destroyed once FinishClose returns.
  //
  // With the graceful option, we will wait for all streams to close on their
  // own before proceeding with the FinishClose operation. New streams will
  // be rejected, however.

  switch (method) {
    case CloseMethod::DEFAULT: {
      Debug(this, "Immediately closing session");
      impl_->state_->silent_close = 0;
      return FinishClose();
    }
    case CloseMethod::SILENT: {
      Debug(this, "Immediately closing session silently");
      impl_->state_->silent_close = 1;
      return FinishClose();
    }
    case CloseMethod::GRACEFUL: {
      // If we are already closing gracefully, do nothing.
      if (impl_->state_->graceful_close) [[unlikely]] {
        return;
      }
      impl_->state_->graceful_close = 1;

      // application_ may be null for server sessions if close() is called
      // before the TLS handshake selects the ALPN. Without an application
      // we cannot do a graceful shutdown (GOAWAY, CONNECTION_CLOSE etc.),
      // so fall through to a silent close.
      if (!impl_->application_) {
        impl_->state_->silent_close = 1;
        return FinishClose();
      }

      // The SendPendingDataScope ensures that the GOAWAY packet queued
      // by BeginShutdown is actually sent. Without it, the GOAWAY sits
      // in nghttp3's outq until the next Receive() triggers a send.
      SendPendingDataScope send_scope(this);

      // Signal application-level graceful shutdown (e.g., HTTP/3 GOAWAY).
      // BeginShutdown can trigger callbacks that re-enter JS and destroy
      // this session, so check is_destroyed() after it returns.
      application().BeginShutdown();
      if (is_destroyed()) return;

      // If there are no open streams, then we can close immediately and
      // not worry about waiting around.
      if (impl_->streams_.empty()) {
        impl_->state_->silent_close = 0;
        return FinishClose();
      }

      // Shut down the writable side of streams whose readable side is
      // already ended (e.g., peer called resetStream or sent FIN). Without
      // this, such half-closed streams will never fire on_stream_close and
      // the graceful close hangs. Streams still actively receiving data
      // are left alone to complete naturally.
      //
      // When the application manages stream FIN (HTTP/3), skip this — a
      // writable stream with a closed read side is the normal request/
      // response pattern (server received full request, still sending
      // response). The application protocol handles stream completion.
      if (!application().stream_fin_managed_by_application()) {
        Session::SendPendingDataScope send_scope(this);
        for (auto& [id, stream] : impl_->streams_) {
          if (stream->is_writable() && !stream->is_readable()) {
            stream->EndWritable();
            ngtcp2_conn_shutdown_stream_write(*this, 0, id, 0);
          }
        }
      }
      // The SendPendingDataScope destructor can trigger callbacks that
      // re-enter JS and destroy this session.
      if (is_destroyed()) return;

      Debug(this,
            "Gracefully closing session (waiting on %zu streams)",
            impl_->streams_.size());
      return;
    }
  }
  UNREACHABLE();
}

void Session::FinishClose() {
  // FinishClose() should be called only after, and as a result of, Close()
  // being called first. However, re-entrancy through MakeCallback or timer
  // callbacks can cause impl_ to be destroyed at any point during this
  // method. We must check is_destroyed() after every operation that could
  // trigger MakeCallback (stream destruction, pending queue rejection,
  // SendConnectionClose, EmitClose).
  if (is_destroyed()) return;
  DCHECK(impl_->state_->closing);

  // Clear the graceful_close flag to prevent RemoveStream() from
  // re-entering FinishClose() when we destroy streams below.
  impl_->state_->graceful_close = 0;

  // Destroy all open streams immediately. We copy the map because
  // streams remove themselves during destruction. Each Destroy() call
  // triggers MakeCallback which can destroy impl_ via JS re-entrancy.
  StreamsMap streams = impl_->streams_;
  for (auto& stream : streams) {
    if (is_destroyed()) return;
    stream.second->Destroy(impl_->last_error_);
  }
  if (is_destroyed()) return;

  // Clear pending stream queues.
  while (!impl_->pending_bidi_stream_queue_.IsEmpty()) {
    impl_->pending_bidi_stream_queue_.PopFront()->reject(impl_->last_error_);
  }
  while (!impl_->pending_uni_stream_queue_.IsEmpty()) {
    impl_->pending_uni_stream_queue_.PopFront()->reject(impl_->last_error_);
  }

  // Send final application-level shutdown and CONNECTION_CLOSE
  // unless this is a silent close.
  if (!impl_->state_->silent_close) {
    if (impl_->application_) {
      application().CompleteShutdown();
    }
    SendConnectionClose();
  }
  if (is_destroyed()) return;

  impl_->timer_.Close();

  // If the session was passed to JavaScript, we need to round-trip
  // through JS so it can clean up before we destroy. The JS side
  // will synchronously call destroy(), which calls Session::Destroy().
  if (impl_->state_->wrapped) {
    EmitClose(impl_->last_error_);
  } else {
    Destroy();
  }
}

void Session::Destroy() {
  DCHECK(impl_);
  // Ensure the closing flag is set for the ~Impl() DCHECK. Normally
  // this is set by Session::Close(), but JS destroy() can be called
  // directly without going through Close() first.
  impl_->state_->closing = 1;

  // If we're inside a ngtcp2 or nghttp3 callback scope, we cannot
  // destroy impl_ now because the callback is executing methods on
  // objects owned by impl_ (e.g., the Application). Defer the
  // destruction until the scope exits.
  if (in_ngtcp2_callback_scope_ || in_nghttp3_callback_scope_) {
    Debug(this, "Session destroy deferred (in callback scope)");
    destroy_deferred_ = true;
    return;
  }

  Debug(this, "Session destroyed");
  {
    auto& stats_ = impl_->stats_;
    STAT_RECORD_TIMESTAMP(Stats, destroyed_at);
  }
  impl_.reset();
}

PendingStream::PendingStreamQueue& Session::pending_bidi_stream_queue() const {
  DCHECK(!is_destroyed());
  return impl_->pending_bidi_stream_queue_;
}

PendingStream::PendingStreamQueue& Session::pending_uni_stream_queue() const {
  DCHECK(!is_destroyed());
  return impl_->pending_uni_stream_queue_;
}

size_t Session::max_packet_size() const {
  DCHECK(!is_destroyed());
  return ngtcp2_conn_get_max_tx_udp_payload_size(*this);
}

uint32_t Session::version() const {
  DCHECK(!is_destroyed());
  return impl_->config_.version;
}

Endpoint& Session::endpoint() const {
  DCHECK(!is_destroyed());
  return *impl_->endpoint_;
}

TLSSession& Session::tls_session() const {
  DCHECK(!is_destroyed());
  return *tls_session_;
}

Session::Application& Session::application() const {
  DCHECK(!is_destroyed());
  DCHECK(impl_->application_);
  return *impl_->application_;
}

std::string_view Session::DecodeAlpn(std::string_view wire) {
  // ALPN wire format is length-prefixed: [len][name]. Extract the first entry.
  if (wire.size() >= 2) {
    uint8_t len = static_cast<uint8_t>(wire[0]);
    if (len > 0 && static_cast<size_t>(len + 1) <= wire.size()) {
      return wire.substr(1, len);
    }
  }
  return {};
}

std::unique_ptr<Session::Application> Session::SelectApplicationFromAlpn(
    std::string_view alpn) {
  // h3 and h3-XX variants use Http3ApplicationImpl.
  // Everything else uses DefaultApplication.
  if (alpn == "h3" || (alpn.size() > 3 && alpn.substr(0, 3) == "h3-")) {
    return CreateHttp3Application(this, config().options.application_options);
  }
  return CreateDefaultApplication(this, config().options.application_options);
}

void Session::SetApplication(std::unique_ptr<Application> app) {
  DCHECK(!impl_->application_);
  // If we have pending ticket data from a session ticket that was
  // parsed before ALPN negotiation, validate it against the selected
  // application now. If the type doesn't match or the application
  // rejects the data, the handshake will fail (application_ stays null
  // and the caller returns an error).
  if (impl_->pending_ticket_data_.has_value()) {
    auto data = std::move(*impl_->pending_ticket_data_);
    impl_->pending_ticket_data_.reset();
    if (!app->ApplySessionTicketData(data)) {
      Debug(this, "Session ticket app data rejected by application");
      return;
    }
  }
  impl_->state_->application_type = static_cast<uint8_t>(app->type());
  impl_->state_->headers_supported = static_cast<uint8_t>(
      app->SupportsHeaders() ? HeadersSupportState::SUPPORTED
                             : HeadersSupportState::UNSUPPORTED);
  // Surface the application's "no error" and "internal error" codes via
  // session state so that JS-side code (e.g. the stream writer's fail()
  // path) can resolve the right wire code for the negotiated ALPN
  // without duplicating the per-application table.
  impl_->state_->no_error_code = app->GetNoErrorCode();
  impl_->state_->internal_error_code = app->GetInternalErrorCode();
  impl_->application_ = std::move(app);
}

const SocketAddress& Session::remote_address() const {
  DCHECK(!is_destroyed());
  return impl_->remote_address_;
}

const SocketAddress& Session::local_address() const {
  DCHECK(!is_destroyed());
  return impl_->local_address_;
}

std::string Session::diagnostic_name() const {
  const auto get_type = [&] { return is_server() ? "server" : "client"; };

  return std::string("Session (") + get_type() + "," +
         std::to_string(env()->thread_id()) + ":" +
         std::to_string(static_cast<int64_t>(get_async_id())) + ")";
}

const Session::Config& Session::config() const {
  DCHECK(!is_destroyed());
  return impl_->config_;
}

Session::Config& Session::config() {
  DCHECK(!is_destroyed());
  return impl_->config_;
}

const Session::Options& Session::options() const {
  DCHECK(!is_destroyed());
  return impl_->config_.options;
}

void Session::EmitQlog(uint32_t flags, std::string_view data) {
  if (!env()->can_call_into_js()) return;

  bool fin = (flags & NGTCP2_QLOG_WRITE_FLAG_FIN) != 0;

  // Fun fact... ngtcp2 does not emit the final qlog statement until the
  // ngtcp2_conn object is destroyed. That means this method is called
  // synchronously during impl_.reset() in Session::Destroy(), at which
  // point is_destroyed() is true. We cannot use MakeCallback here because
  // it can trigger microtask processing and re-entrancy while the
  // ngtcp2_conn is mid-destruction. Defer the final chunk via SetImmediate.
  if (is_destroyed()) {
    auto isolate = env()->isolate();
    v8::Global<v8::Object> recv(isolate, object());
    v8::Global<v8::Function> cb(
        isolate, BindingData::Get(env()).session_qlog_callback());
    std::string buf(data);
    env()->SetImmediate([recv = std::move(recv),
                         cb = std::move(cb),
                         buf = std::move(buf),
                         fin](Environment* env) {
      HandleScope handle_scope(env->isolate());
      auto context = env->context();
      Local<Value> argv[] = {
          Undefined(env->isolate()),
          Boolean::New(env->isolate(), fin),
      };
      if (!ToV8Value(context, buf).ToLocal(&argv[0])) return;
      USE(cb.Get(env->isolate())
              ->Call(context, recv.Get(env->isolate()), arraysize(argv), argv));
    });
    return;
  }

  auto isolate = env()->isolate();
  Local<Value> argv[] = {Undefined(isolate), Boolean::New(isolate, fin)};
  if (!ToV8Value(env()->context(), data).ToLocal(&argv[0])) {
    Debug(this, "Failed to convert qlog data to V8 string");
    return;
  }

  Debug(this, "Emitting qlog data");
  MakeCallback(
      BindingData::Get(env()).session_qlog_callback(), arraysize(argv), argv);
}

const TransportParams Session::local_transport_params() const {
  DCHECK(!is_destroyed());
  return ngtcp2_conn_get_local_transport_params(*this);
}

const TransportParams Session::remote_transport_params() const {
  DCHECK(!is_destroyed());
  return ngtcp2_conn_get_remote_transport_params(*this);
}

void Session::SetLastError(QuicError&& error) {
  DCHECK(!is_destroyed());
  Debug(this, "Setting last error to %s", error);
  impl_->last_error_ = std::move(error);
}

bool Session::Receive(Store&& store,
                      const SocketAddress& local_address,
                      const SocketAddress& remote_address) {
  DCHECK(!is_destroyed());
  impl_->remote_address_ = remote_address;

  // When we are done processing this packet, we arrange to send any
  // pending data for this session.
  SendPendingDataScope send_scope(this);

  ngtcp2_vec vec = store;
  Path path(local_address, remote_address);

  Debug(this,
        "Session is receiving %zu-byte packet received along path %s",
        vec.len,
        path);

  // It is important to understand that reading the packet will cause
  // callback functions to be invoked, any one of which could lead to
  // the Session being closed/destroyed synchronously. The callback scope
  // ensures that any deferred destroy waits until all callbacks for this
  // packet have completed. After calling ngtcp2_conn_read_pkt here, we
  // will need to double check that the session is not destroyed before
  // we try doing anything with it (like updating stats, sending pending
  // data, etc).
  int err;
  {
    NgTcp2CallbackScope callback_scope(this);
    err = ngtcp2_conn_read_pkt(*this,
                               &path,
                               // TODO(@jasnell): ECN pkt_info blocked on libuv
                               nullptr,
                               vec.base,
                               vec.len,
                               uv_hrtime());
  }
  if (is_destroyed()) return false;

  Debug(this, "Session receiving %zu-byte packet with result %d", vec.len, err);

  switch (err) {
    case 0: {
      Debug(this, "Session successfully received %zu-byte packet", vec.len);
      if (!is_destroyed()) [[likely]] {
        auto& stats_ = impl_->stats_;
        STAT_INCREMENT_N(Stats, bytes_received, vec.len);
        // Process deferred operations that couldn't run inside callback
        // scopes (e.g., HTTP/3 GOAWAY handling that calls into JS).
        application().PostReceive();
      }
      return true;
    }
    case NGTCP2_ERR_REQUIRED_TRANSPORT_PARAM: {
      Debug(this,
            "Receiving packet failed: "
            "Remote peer failed to send a required transport parameter");
      if (!is_destroyed()) [[likely]] {
        SetLastError(QuicError::ForTransport(err));
        Close();
      }
      return false;
    }
    case NGTCP2_ERR_DRAINING: {
      // Connection has entered the draining state, no further data
      // should be sent. This can happen for two reasons:
      //
      // 1. The remote peer sent a CONNECTION_CLOSE. In this case we
      //    start the draining timer and let OnTimeout handle the
      //    close, extracting the peer's error via FromConnectionClose.
      //
      // 2. The remote peer sent a stateless reset. ngtcp2 set the
      //    draining state internally and invoked our informational
      //    on_receive_stateless_reset callback (which set the flag).
      //    There is no point in waiting for a draining period — the
      //    peer has no state. Close immediately with an error.
      if (!is_destroyed()) [[likely]] {
        if (impl_->state_->stateless_reset) {
          Debug(this, "Session received stateless reset, closing");
          SetLastError(QuicError::ForNgtcp2Error(NGTCP2_ERR_DRAINING));
          Close(CloseMethod::SILENT);
        } else {
          Debug(this, "Session is draining, starting draining timer");
          UpdateTimer();
        }
      }
      return false;
    }
    case NGTCP2_ERR_CLOSING: {
      // Connection has entered the closing state, no further data should be
      // sent. This happens when the local peer has called
      // ngtcp2_conn_write_connection_close.
      Debug(this, "Receiving packet failed: Session is closing");
      return false;
    }
    case NGTCP2_ERR_CRYPTO: {
      Debug(this, "Receiving packet failed: Crypto error");
      // Crypto error happened! Set the last error to the tls alert
      if (!is_destroyed()) [[likely]] {
        SetLastError(QuicError::ForTlsAlert(ngtcp2_conn_get_tls_alert(*this)));
        Close();
      }
      return false;
    }
    case NGTCP2_ERR_RETRY: {
      // This should only ever happen on the server. We have to send a path
      // validation challenge in the form of a RETRY packet to the peer and
      // drop the connection.
      DCHECK(is_server());
      Debug(this, "Receiving packet failed: Server must send a retry packet");
      if (!is_destroyed()) {
        endpoint().SendRetry(PathDescriptor{
            version(),
            config().dcid,
            config().scid,
            impl_->local_address_,
            impl_->remote_address_,
        });
        Close(CloseMethod::SILENT);
      }
      return false;
    }
    case NGTCP2_ERR_DROP_CONN: {
      // There's nothing else to do but drop the connection state.
      Debug(this, "Receiving packet failed: Session must drop the connection");
      if (!is_destroyed()) {
        Close(CloseMethod::SILENT);
      }
      return false;
    }
  }

  // Shouldn't happen but just in case... handle other unknown errors
  Debug(this,
        "Receiving packet failed: "
        "Unexpected error %d while receiving packet",
        err);
  if (!is_destroyed()) {
    SetLastError(QuicError::ForNgtcp2Error(err));
    Close();
  }
  return false;
}

void Session::Send(Packet::Ptr packet) {
  // Sending a Packet is generally best effort. If we're not in a state
  // where we can send a packet, it's ok to drop it on the floor. The
  // packet loss mechanisms will cause the packet data to be resent later
  // if appropriate (and possible).

  // That said, we should never be trying to send a packet when we're in
  // a draining period.
  DCHECK(!is_destroyed());
  DCHECK(!is_in_draining_period());

  if (!can_send_packets()) [[unlikely]] {
    // Ptr destructor releases the packet back to the pool.
    return;
  }

  Debug(this, "Session is sending %s", packet->ToString());
  endpoint().Send(std::move(packet));
}

void Session::Send(Packet::Ptr packet, const PathStorage& path) {
  DCHECK(!is_destroyed());
  DCHECK(!is_in_draining_period());
  UpdatePath(path);

  // Check if ngtcp2 wants this packet sent on a different path than the
  // primary endpoint. This happens during path validation for preferred
  // address or connection migration — e.g., a PATH_RESPONSE needs to be
  // sent from the preferred address endpoint, not the primary.
  if (path.path.local.addrlen > 0) {
    SocketAddress local_addr(path.path.local.addr);
    auto& mgr = BindingData::Get(env()).session_manager();
    Endpoint* target = mgr.FindEndpointForAddress(local_addr);
    if (target != nullptr && target != &endpoint()) {
      // Redirect the packet to the target endpoint. This updates the
      // listener (for pending_callbacks accounting in the ArenaPool
      // completion callback) and the destination address.
      SocketAddress remote_addr(path.path.remote.addr);
      packet->Redirect(static_cast<Packet::Listener*>(target), remote_addr);
      if (can_send_packets()) [[likely]] {
        Debug(this,
              "Sending via non-primary endpoint for path %s",
              local_addr.ToString());
        target->Send(std::move(packet));
      }
      return;
    }
  }

  Send(std::move(packet));
}

datagram_id Session::SendDatagram(Store&& data) {
  DCHECK(!is_destroyed());

  // Sending a datagram is best effort. If we cannot send it for any reason,
  // we just return 0 to indicate that the datagram was not sent an the
  // data is dropped on the floor.

  // If the session is destroyed, draining, or closing, we cannot send.
  if (is_destroyed() || is_in_draining_period() || is_in_closing_period()) {
    return 0;
  }

  const ngtcp2_transport_params* tp = remote_transport_params();
  uint64_t max_datagram_size = MaxDatagramPayload(tp->max_datagram_frame_size);

  // These size and length checks should have been caught by the JavaScript
  // side, but handle it gracefully here just in case. We might have some future
  // case where datagram frames are sent from C++ code directly, so it's good to
  // have these checks as a backstop regardless.

  if (max_datagram_size == 0) {
    Debug(this, "Datagrams are disabled");
    return 0;
  }

  if (data.length() > max_datagram_size) [[unlikely]] {
    Debug(this, "Ignoring oversized datagram");
    return 0;
  }

  if (data.length() == 0) [[unlikely]] {
    Debug(this, "Ignoring empty datagram");
    return 0;
  }

  // Assign the datagram ID.
  datagram_id did = ++impl_->state_->last_datagram_id;

  // Check queue capacity. Apply the drop policy when full.
  auto max_pending = impl_->state_->max_pending_datagrams;
  if (max_pending > 0 && impl_->pending_datagrams_.size() >= max_pending) {
    auto drop_policy = impl_->config_.options.datagram_drop_policy;
    if (drop_policy == DatagramDropPolicy::DROP_OLDEST) {
      auto& oldest = impl_->pending_datagrams_.front();
      Debug(this,
            "Datagram queue full, dropping oldest datagram %" PRIu64,
            oldest.id);
      DatagramStatus(oldest.id, DatagramStatus::ABANDONED);
      impl_->pending_datagrams_.pop_front();
    } else {
      // DROP_NEWEST: reject the incoming datagram.
      Debug(
          this, "Datagram queue full, dropping newest datagram %" PRIu64, did);
      DatagramStatus(did, DatagramStatus::ABANDONED);
      return did;
    }
  }

  // Queue the datagram. It will be serialized into packets by
  // SendPendingData alongside stream data.
  Debug(this, "Queuing %zu-byte datagram %" PRIu64, data.length(), did);
  impl_->pending_datagrams_.push_back({did, std::move(data)});

  return did;
}

void Session::UpdatePacketTxTime() {
  DCHECK(!is_destroyed());
  ngtcp2_conn_update_pkt_tx_time(*this, uv_hrtime());
}

void Session::UpdatePath(const PathStorage& storage) {
  DCHECK(!is_destroyed());
  impl_->remote_address_.Update(storage.path.remote.addr,
                                storage.path.remote.addrlen);
  impl_->local_address_.Update(storage.path.local.addr,
                               storage.path.local.addrlen);
  Debug(this,
        "path updated. local %s, remote %s",
        impl_->local_address_,
        impl_->remote_address_);
}

BaseObjectPtr<Stream> Session::FindStream(stream_id id) const {
  if (is_destroyed()) return {};
  auto it = impl_->streams_.find(id);
  if (it == std::end(impl_->streams_)) return {};
  return it->second;
}

Session::StreamsMap Session::streams() const {
  if (is_destroyed()) return {};
  return impl_->streams_;
}

BaseObjectPtr<Stream> Session::CreateStream(
    stream_id id,
    CreateStreamOption option,
    std::shared_ptr<DataQueue> data_source) {
  if (!can_create_streams()) [[unlikely]]
    return {};
  if (auto stream = Stream::Create(this, id, std::move(data_source)))
      [[likely]] {
    AddStream(stream, option);
    return stream;
  }
  return {};
}

MaybeLocal<Object> Session::OpenStream(Direction direction,
                                       std::shared_ptr<DataQueue> data_source) {
  // If can_create_streams() returns false, we are not able to open a stream
  // at all now, even in a pending state. The implication is that that session
  // is destroyed or closing.
  if (!can_create_streams()) [[unlikely]]
    return {};

  // If can_open_streams() returns false, we are able to create streams but
  // they will remain in a pending state. The implication is that the session
  // TLS handshake is still progressing. Note that when a pending stream is
  // created, it will not be listed in the streams list.
  if (!can_open_streams()) {
    if (auto stream = Stream::Create(this, direction, std::move(data_source)))
        [[likely]] {
      return stream->object();
    }
    return {};
  }

  stream_id id = -1;
  auto open = [&] {
    switch (direction) {
      case Direction::BIDIRECTIONAL: {
        Debug(this, "Opening bidirectional stream");
        return ngtcp2_conn_open_bidi_stream(*this, &id, nullptr);
      }
      case Direction::UNIDIRECTIONAL: {
        Debug(this, "Opening uni-directional stream");
        return ngtcp2_conn_open_uni_stream(*this, &id, nullptr);
      }
    }
    UNREACHABLE();
  };

  switch (open()) {
    case 0: {
      // Woo! Our stream was created.
      CHECK_GE(id, 0);
      if (auto stream = CreateStream(
              id, CreateStreamOption::DO_NOT_NOTIFY, std::move(data_source)))
          [[likely]] {
        return stream->object();
      }
      break;
    }
    case NGTCP2_ERR_STREAM_ID_BLOCKED: {
      // The stream cannot yet be opened.
      // This is typically caused by the application exceeding the allowed max
      // number of concurrent streams. We will allow the stream to be created
      // in a pending state.
      if (auto stream = Stream::Create(this, direction, std::move(data_source)))
          [[likely]] {
        return stream->object();
      }
      break;
    }
  }
  return {};
}

void Session::AddStream(BaseObjectPtr<Stream> stream,
                        CreateStreamOption option) {
  DCHECK(!is_destroyed());
  DCHECK(stream);

  auto id = stream->id();
  auto direction = stream->direction();

  // Let's double check that a stream with the given id does not alreadys
  // exist. If it does, that means we've got a bug somewhere.
  DCHECK_EQ(impl_->streams_.find(id), impl_->streams_.end());

  Debug(this, "Adding stream %" PRIi64 " to session", id);

  // The streams_ map becomes the sole owner of the Stream instance.
  // We mark the stream detached so that when it is removed from
  // the session, or is the session is destroyed, the stream will
  // also be destroyed.
  impl_->streams_[id] = stream;
  stream->Detach();

  ngtcp2_conn_set_stream_user_data(*this, id, stream.get());

  // If the stream already has outbound data (body was provided at creation
  // time), resume it now that it is registered in the streams map and can
  // be found by FindStream.
  if (stream->has_outbound()) {
    ResumeStream(id);
  }

  if (option == CreateStreamOption::NOTIFY) {
    EmitStream(stream);
    // EmitStream triggers the JS onstream callback via MakeCallback.
    // If the callback throws, safeCallbackInvoke calls session.destroy()
    // which resets impl_. We must bail out if that happened.
    if (is_destroyed()) return;
  }

  // Update tracking statistics for the number of streams associated with this
  // session.
  auto& stats_ = impl_->stats_;
  if (ngtcp2_conn_is_local_stream(*this, id)) {
    switch (direction) {
      case Direction::BIDIRECTIONAL: {
        STAT_INCREMENT(Stats, bidi_out_stream_count);
        break;
      }
      case Direction::UNIDIRECTIONAL: {
        STAT_INCREMENT(Stats, uni_out_stream_count);
        break;
      }
    }
  } else {
    switch (direction) {
      case Direction::BIDIRECTIONAL: {
        STAT_INCREMENT(Stats, bidi_in_stream_count);
        break;
      }
      case Direction::UNIDIRECTIONAL: {
        STAT_INCREMENT(Stats, uni_in_stream_count);
        break;
      }
    }
  }
}

void Session::RemoveStream(stream_id id) {
  DCHECK(!is_destroyed());
  Debug(this, "Removing stream %" PRIi64 " from session", id);
  if (!is_in_draining_period() && !is_in_closing_period() &&
      !ngtcp2_conn_is_local_stream(*this, id)) {
    if (ngtcp2_is_bidi_stream(id)) {
      ngtcp2_conn_extend_max_streams_bidi(*this, 1);
    } else {
      ngtcp2_conn_extend_max_streams_uni(*this, 1);
    }
  }

  ngtcp2_conn_set_stream_user_data(*this, id, nullptr);

  // Note that removing the stream from the streams map likely releases
  // the last BaseObjectPtr holding onto the Stream instance, at which
  // point it will be freed. If there are other BaseObjectPtr instances
  // or other references to the Stream, however, freeing will be deferred.
  // In either case, we cannot assume that the stream still exists after
  // this call.
  impl_->streams_.erase(id);

  // If we are gracefully closing and there are no more streams,
  // then we can proceed to finishing the close now. Note that the
  // expectation is that the session will be destroyed once FinishClose
  // returns.
  if (impl_->state_->closing && impl_->state_->graceful_close) {
    FinishClose();
    CHECK(is_destroyed());
  }
}

void Session::ResumeStream(stream_id id) {
  DCHECK(!is_destroyed());
  SendPendingDataScope send_scope(this);
  application().ResumeStream(id);
}

void Session::ShutdownStream(stream_id id, QuicError error) {
  DCHECK(!is_destroyed());
  Debug(this, "Shutting down stream %" PRIi64 " with error %s", id, error);
  SendPendingDataScope send_scope(this);
  ngtcp2_conn_shutdown_stream(*this,
                              0,
                              id,
                              error.type() == QuicError::Type::APPLICATION
                                  ? error.code()
                                  : application().GetNoErrorCode());
}

void Session::ShutdownStreamWrite(stream_id id, QuicError code) {
  DCHECK(!is_destroyed());
  Debug(this, "Shutting down stream %" PRIi64 " write with error %s", id, code);
  SendPendingDataScope send_scope(this);
  ngtcp2_conn_shutdown_stream_write(*this,
                                    0,
                                    id,
                                    code.type() == QuicError::Type::APPLICATION
                                        ? code.code()
                                        : application().GetNoErrorCode());
}

void Session::StreamDataBlocked(stream_id id) {
  DCHECK(!is_destroyed());
  auto& stats_ = impl_->stats_;
  STAT_INCREMENT(Stats, block_count);
  application().BlockStream(id);
}

void Session::CollectSessionTicketAppData(
    SessionTicket::AppData* app_data) const {
  DCHECK(!is_destroyed());
  application().CollectSessionTicketAppData(app_data);
}

SessionTicket::AppData::Status Session::ExtractSessionTicketAppData(
    const SessionTicket::AppData& app_data, Flag flag) {
  DCHECK(!is_destroyed());
  // If the application is already selected (client side, or server after
  // ALPN), delegate directly.
  if (impl_->application_) {
    return application().ExtractSessionTicketAppData(app_data, flag);
  }
  // The application is not yet selected (server during ClientHello
  // processing, before ALPN). Parse the ticket data now while the
  // SSL_SESSION is still valid, and stash the result for validation
  // after ALPN negotiation in SetApplication().
  auto data = app_data.Get();
  if (!data.has_value() || data->len == 0) {
    // No app data in the ticket. Accept optimistically.
    return flag == Flag::STATUS_RENEW
               ? SessionTicket::AppData::Status::TICKET_USE_RENEW
               : SessionTicket::AppData::Status::TICKET_USE;
  }
  auto parsed = Application::ParseTicketData(*data);
  if (!parsed.has_value()) {
    return SessionTicket::AppData::Status::TICKET_IGNORE_RENEW;
  }
  // Pre-validate the ticket data against the current application options.
  // If the stored settings are more permissive than the current config
  // (e.g., a feature was enabled when the ticket was issued but is now
  // disabled), reject the ticket so 0-RTT is not used. This must happen
  // here (during TLS ticket processing) rather than in SetApplication,
  // because by SetApplication time the TLS layer has already accepted
  // the ticket and told the client 0-RTT is ok.
  if (!Application::ValidateTicketData(*parsed,
                                       config().options.application_options)) {
    Debug(this, "Session ticket app data incompatible with current settings");
    return SessionTicket::AppData::Status::TICKET_IGNORE_RENEW;
  }
  impl_->pending_ticket_data_ = std::move(parsed);
  return flag == Flag::STATUS_RENEW
             ? SessionTicket::AppData::Status::TICKET_USE_RENEW
             : SessionTicket::AppData::Status::TICKET_USE;
}

void Session::MemoryInfo(MemoryTracker* tracker) const {
  if (impl_) {
    tracker->TrackField("impl", impl_);
  }
  tracker->TrackField("tls_session", tls_session_);
}

bool Session::is_in_closing_period() const {
  DCHECK(!is_destroyed());
  return ngtcp2_conn_in_closing_period(*this) != 0;
}

bool Session::is_in_draining_period() const {
  DCHECK(!is_destroyed());
  return ngtcp2_conn_in_draining_period(*this) != 0;
}

bool Session::wants_session_ticket() const {
  return !is_destroyed() &&
         HasListenerFlag(impl_->state_->listener_flags,
                         SessionListenerFlags::SESSION_TICKET);
}

void Session::SetStreamOpenAllowed() {
  DCHECK(!is_destroyed());
  impl_->state_->stream_open_allowed = 1;
}

void Session::PopulateEarlyTransportParamsState() {
  DCHECK(!is_destroyed());
  const ngtcp2_transport_params* tp = remote_transport_params();
  if (tp != nullptr) {
    impl_->state_->max_datagram_size =
        MaxDatagramPayload(tp->max_datagram_frame_size);
  }
}

bool Session::can_send_packets() const {
  // We can send packets if we're not in the middle of a ngtcp2 callback
  // on THIS session, we're not destroyed, and we're not in a draining
  // or closing period. The callback scope check is per-session so that
  // one session's ngtcp2 callback does not block unrelated sessions
  // from sending.
  return !is_destroyed() && !in_ngtcp2_callback_scope_ &&
         !is_in_draining_period() && !is_in_closing_period();
}

bool Session::can_create_streams() const {
  return !is_destroyed_or_closing() && !is_in_closing_period() &&
         !is_in_draining_period();
}

bool Session::can_open_streams() const {
  return !is_destroyed() && impl_->state_->stream_open_allowed;
}

uint64_t Session::max_data_left() const {
  DCHECK(!is_destroyed());
  return ngtcp2_conn_get_max_data_left(*this);
}

uint64_t Session::max_local_streams_uni() const {
  DCHECK(!is_destroyed());
  return ngtcp2_conn_get_streams_uni_left(*this);
}

uint64_t Session::max_local_streams_bidi() const {
  DCHECK(!is_destroyed());
  return ngtcp2_conn_get_local_transport_params(*this)
      ->initial_max_streams_bidi;
}

void Session::set_wrapped() {
  DCHECK(!is_destroyed());
  impl_->state_->wrapped = 1;
}

void Session::set_priority_supported(bool on) {
  DCHECK(!is_destroyed());
  impl_->state_->priority_supported = on ? 1 : 0;
}

void Session::ExtendStreamOffset(stream_id id, size_t amount) {
  DCHECK(!is_destroyed());
  Debug(this, "Extending stream %" PRIi64 " offset by %zu bytes", id, amount);
  ngtcp2_conn_extend_max_stream_offset(*this, id, amount);
}

void Session::ExtendOffset(size_t amount) {
  DCHECK(!is_destroyed());
  Debug(this, "Extending offset by %zu bytes", amount);
  ngtcp2_conn_extend_max_offset(*this, amount);
}

bool Session::HasPendingDatagrams() const {
  return impl_ && !impl_->pending_datagrams_.empty();
}

Session::PendingDatagram& Session::PeekPendingDatagram() {
  return impl_->pending_datagrams_.front();
}

void Session::PopPendingDatagram() {
  impl_->pending_datagrams_.pop_front();
}

size_t Session::PendingDatagramCount() const {
  return impl_ ? impl_->pending_datagrams_.size() : 0;
}

void Session::DatagramSent(datagram_id id) {
  Debug(this, "Datagram %" PRIu64 " sent", id);
  auto& stats_ = impl_->stats_;
  STAT_INCREMENT(Stats, datagrams_sent);
}

void Session::UpdateDataStats() {
  if (is_destroyed()) return;
  Debug(this, "Updating data stats");
  auto& stats_ = impl_->stats_;
  ngtcp2_conn_info info;
  ngtcp2_conn_get_conn_info(*this, &info);
  STAT_SET(Stats, bytes_in_flight, info.bytes_in_flight);
  STAT_SET(Stats, cwnd, info.cwnd);
  STAT_SET(Stats, latest_rtt, info.latest_rtt);
  STAT_SET(Stats, min_rtt, info.min_rtt);
  STAT_SET(Stats, rttvar, info.rttvar);
  STAT_SET(Stats, smoothed_rtt, info.smoothed_rtt);
  STAT_SET(Stats, ssthresh, info.ssthresh);
  STAT_SET(Stats, pkt_sent, info.pkt_sent);
  STAT_SET(Stats, bytes_sent, info.bytes_sent);
  STAT_SET(Stats, pkt_recv, info.pkt_recv);
  STAT_SET(Stats, bytes_recv, info.bytes_recv);
  STAT_SET(Stats, pkt_lost, info.pkt_lost);
  STAT_SET(Stats, bytes_lost, info.bytes_lost);
  STAT_SET(Stats, ping_recv, info.ping_recv);
  STAT_SET(Stats, pkt_discarded, info.pkt_discarded);

  STAT_SET(
      Stats,
      max_bytes_in_flight,
      std::max(STAT_GET(Stats, max_bytes_in_flight), info.bytes_in_flight));
}

void Session::SendConnectionClose() {
  // Method is a non-op if the session is already destroyed or the
  // endpoint cannot send. Note: we intentionally do NOT check
  // can_send_packets() here because ngtcp2_conn_write_connection_close
  // puts the connection into the closing period, and the resulting packet
  // must still be sent to the endpoint.
  if (is_destroyed()) return;

  Debug(this, "Sending connection close packet to peer");

  auto ErrorAndSilentClose = [&] {
    Debug(this, "Failed to create connection close packet");
    SetLastError(QuicError::ForNgtcp2Error(NGTCP2_INTERNAL_ERROR));
    Close(CloseMethod::SILENT);
  };

  if (is_server()) {
    if (auto packet = Packet::CreateConnectionClosePacket(
            endpoint(), impl_->remote_address_, *this, impl_->last_error_))
        [[likely]] {
      // Send directly to endpoint, bypassing Session::Send which
      // would drop the packet because we're now in the closing period.
      return endpoint().Send(std::move(packet));
    }

    // If we are unable to create a connection close packet then
    // we are in a bad state. An internal error will be set and
    // the session will be silently closed. This is not ideal
    // because the remote peer will not know immediately that
    // the connection has terminated but there's not much else
    // we can do.
    return ErrorAndSilentClose();
  }

  auto packet = endpoint().CreatePacket(impl_->remote_address_,
                                        kDefaultMaxPacketLength,
                                        "immediate connection close (client)");
  if (!packet) [[unlikely]] {
    return ErrorAndSilentClose();
  }

  ngtcp2_vec vec = *packet;
  Path path(impl_->local_address_, impl_->remote_address_);
  ssize_t nwrite = ngtcp2_conn_write_connection_close(*this,
                                                      &path,
                                                      nullptr,
                                                      vec.base,
                                                      vec.len,
                                                      impl_->last_error_,
                                                      uv_hrtime());

  if (nwrite < 0) [[unlikely]] {
    return ErrorAndSilentClose();
  }

  packet->Truncate(nwrite);
  // Send directly to endpoint — ngtcp2 has entered the closing period
  // at this point, so Session::Send() would drop the packet.
  return endpoint().Send(std::move(packet));
}

void Session::OnTimeout() {
  if (is_destroyed()) return;
  if (!impl_->application_) return;
  HandleScope scope(env()->isolate());
  int ret;
  {
    NgTcp2CallbackScope callback_scope(this);
    ret = ngtcp2_conn_handle_expiry(*this, uv_hrtime());
  }
  // handle_expiry can trigger ngtcp2 callbacks that invoke MakeCallback,
  // which can synchronously destroy the session. Guard before proceeding.
  if (is_destroyed()) return;
  if (NGTCP2_OK(ret) && !is_in_closing_period() && !is_in_draining_period()) {
    application().SendPendingData();
    return;
  }
  if (is_destroyed()) return;

  Debug(this, "Session timed out");

  // When the draining period expires, the peer has already sent
  // CONNECTION_CLOSE. Use their close error so a clean close (code 0)
  // propagates as no-error, allowing stream.closed promises to resolve.
  if (is_in_draining_period()) {
    SetLastError(QuicError::FromConnectionClose(*this));
  } else {
    SetLastError(QuicError::ForNgtcp2Error(ret));
  }
  Close(CloseMethod::SILENT);
}

void Session::UpdateTimer() {
  DCHECK(!is_destroyed());
  // Both uv_hrtime and ngtcp2_conn_get_expiry return nanosecond units.
  uint64_t now = uv_hrtime();
  uint64_t expiry;

  if (is_in_draining_period()) {
    // RFC 9000 Section 10.2: The draining state SHOULD persist for at
    // least three times the current Probe Timeout (PTO). ngtcp2 does
    // not set a draining timer internally — the application must
    // compute it.
    ngtcp2_duration pto = ngtcp2_conn_get_pto(*this);
    uint8_t multiplier = impl_->config_.options.draining_period_multiplier;
    expiry = now + multiplier * pto;
  } else {
    expiry = ngtcp2_conn_get_expiry(*this);
  }

  Debug(
      this, "Updating timer. Expiry: %" PRIu64 ", now: %" PRIu64, expiry, now);

  if (expiry <= now) {
    // The timer has already expired.
    return OnTimeout();
  }

  auto timeout = (expiry - now) / NGTCP2_MILLISECONDS;
  Debug(this, "Updating timeout to %zu milliseconds", timeout);

  // If timeout is zero here, it means our timer is less than a millisecond
  // off from expiry. Let's bump the timer to 1.
  impl_->timer_.Update(timeout == 0 ? 1 : timeout);
}

void Session::DatagramStatus(datagram_id datagramId,
                             quic::DatagramStatus status) {
  DCHECK(!is_destroyed());
  auto& stats_ = impl_->stats_;
  switch (status) {
    case DatagramStatus::ACKNOWLEDGED: {
      Debug(this, "Datagram %" PRIu64 " was acknowledged", datagramId);
      STAT_INCREMENT(Stats, datagrams_acknowledged);
      break;
    }
    case DatagramStatus::LOST: {
      Debug(this, "Datagram %" PRIu64 " was lost", datagramId);
      STAT_INCREMENT(Stats, datagrams_lost);
      break;
    }
    case DatagramStatus::ABANDONED: {
      Debug(this, "Datagram %" PRIu64 " was abandoned", datagramId);
      STAT_INCREMENT(Stats, datagrams_lost);
      break;
    }
  }
  if (HasListenerFlag(impl_->state_->listener_flags,
                      SessionListenerFlags::DATAGRAM_STATUS)) {
    EmitDatagramStatus(datagramId, status);
  }
}

void Session::DatagramReceived(const uint8_t* data,
                               size_t datalen,
                               DatagramReceivedFlags flag) {
  DCHECK(!is_destroyed());
  // If there is nothing watching for the datagram on the JavaScript side,
  // or if the datagram is zero-length, we just drop it on the floor.
  if (!HasListenerFlag(impl_->state_->listener_flags,
                       SessionListenerFlags::DATAGRAM) ||
      datalen == 0)
    return;

  Debug(this, "Session is receiving datagram of size %zu", datalen);
  auto& stats_ = impl_->stats_;
  STAT_INCREMENT(Stats, datagrams_received);
  JS_TRY_ALLOCATE_BACKING(env(), backing, datalen)
  memcpy(backing->Data(), data, datalen);
  EmitDatagram(Store(std::move(backing), datalen), flag);
}

void Session::GenerateNewConnectionId(ngtcp2_cid* cid,
                                      size_t len,
                                      ngtcp2_stateless_reset_token* token) {
  DCHECK(!is_destroyed());
  CID cid_ = impl_->config_.options.cid_factory->GenerateInto(cid, len);
  Debug(this, "Generated new connection id %s", cid_);
  StatelessResetToken new_token(
      token, endpoint().options().reset_token_secret, cid_);
  endpoint().AssociateCID(cid_, impl_->config_.scid);
  endpoint().AssociateStatelessResetToken(new_token, this);
}

bool Session::HandshakeCompleted() {
  DCHECK(!is_destroyed());
  DCHECK(!impl_->state_->handshake_completed);

  Debug(this, "Session handshake completed");
  impl_->state_->handshake_completed = 1;
  auto& stats_ = impl_->stats_;
  STAT_RECORD_TIMESTAMP(Stats, handshake_completed_at);
  SetStreamOpenAllowed();

  // Capture the peer's max datagram frame size from the remote transport
  // parameters so JavaScript can check it without a C++ round-trip.
  const ngtcp2_transport_params* tp = remote_transport_params();
  impl_->state_->max_datagram_size =
      MaxDatagramPayload(tp->max_datagram_frame_size);

  // If early data was attempted but rejected by the server,
  // tell ngtcp2 so it can retransmit the data as 1-RTT.
  // The status of early data will only be rejected if an
  // attempt was actually made to send early data.
  if (!is_server() && tls_session().early_data_was_rejected())
    ngtcp2_conn_tls_early_data_rejected(*this);

  // When in a server session, handshake completed == handshake confirmed.
  if (is_server()) {
    HandshakeConfirmed();

    auto& ep = endpoint();

    if (!ep.is_closed() && !ep.is_closing()) {
      auto token = ep.GenerateNewToken(version(), impl_->remote_address_);
      ngtcp2_vec vec = token;
      if (NGTCP2_ERR(ngtcp2_conn_submit_new_token(*this, vec.base, vec.len))) {
        // Submitting the new token failed... In this case we're going to
        // fail because submitting the new token should only fail if we
        // ran out of memory or some other unrecoverable state.
        return false;
      }
    }
  }

  EmitHandshakeComplete();

  return true;
}

void Session::HandshakeConfirmed() {
  DCHECK(!is_destroyed());
  DCHECK(!impl_->state_->handshake_confirmed);
  Debug(this, "Session handshake confirmed");
  impl_->state_->handshake_confirmed = 1;
  auto& stats_ = impl_->stats_;
  STAT_RECORD_TIMESTAMP(Stats, handshake_confirmed_at);
}

void Session::SelectPreferredAddress(PreferredAddress* preferredAddress) {
  if (options().preferred_address_strategy ==
      PreferredAddress::Policy::IGNORE_PREFERRED) {
    Debug(this, "Ignoring preferred address");
    return;
  }

  switch (endpoint().local_address().family()) {
    case AF_INET: {
      Debug(this, "Selecting preferred address for AF_INET");
      auto ipv4 = preferredAddress->ipv4();
      if (ipv4.has_value()) {
        if (ipv4->host[0] == '\0' || ipv4->port == 0) return;
        CHECK(SocketAddress::New(
            AF_INET, ipv4->host, ipv4->port, &impl_->remote_address_));
        preferredAddress->Use(ipv4.value());
      }
      break;
    }
    case AF_INET6: {
      Debug(this, "Selecting preferred address for AF_INET6");
      auto ipv6 = preferredAddress->ipv6();
      if (ipv6.has_value()) {
        if (ipv6->host[0] == '\0' || ipv6->port == 0) return;
        CHECK(SocketAddress::New(
            AF_INET6, ipv6->host, ipv6->port, &impl_->remote_address_));
        preferredAddress->Use(ipv6.value());
      }
      break;
    }
  }
}

CID Session::new_cid(size_t len) const {
  return options().cid_factory->Generate(len);
}

void Session::ProcessPendingBidiStreams() {
  // It shouldn't be possible to get here if can_create_streams() is false.
  DCHECK(can_create_streams());

  stream_id id;

  while (!impl_->pending_bidi_stream_queue_.IsEmpty()) {
    if (ngtcp2_conn_get_streams_bidi_left(*this) == 0) {
      return;
    }

    switch (ngtcp2_conn_open_bidi_stream(*this, &id, nullptr)) {
      case 0: {
        impl_->pending_bidi_stream_queue_.PopFront()->fulfill(id);
        continue;
      }
      case NGTCP2_ERR_STREAM_ID_BLOCKED: {
        // This case really should not happen since we've checked the number
        // of bidi streams left above. However, if it does happen we'll treat
        // it the same as if the get_streams_bidi_left call returned zero.
        return;
      }
      default: {
        // We failed to open the stream for some reason other than being
        // blocked. Report the failure.
        impl_->pending_bidi_stream_queue_.PopFront()->reject(
            QuicError::ForTransport(NGTCP2_STREAM_LIMIT_ERROR));
        continue;
      }
    }
  }
}

void Session::ProcessPendingUniStreams() {
  // It shouldn't be possible to get here if can_create_streams() is false.
  DCHECK(can_create_streams());

  stream_id id;

  while (!impl_->pending_uni_stream_queue_.IsEmpty()) {
    if (ngtcp2_conn_get_streams_uni_left(*this) == 0) {
      return;
    }

    switch (ngtcp2_conn_open_uni_stream(*this, &id, nullptr)) {
      case 0: {
        impl_->pending_uni_stream_queue_.PopFront()->fulfill(id);
        continue;
      }
      case NGTCP2_ERR_STREAM_ID_BLOCKED: {
        // This case really should not happen since we've checked the number
        // of uni streams left above. However, if it does happen we'll treat
        // it the same as if the get_streams_uni_left call returned zero.
        return;
      }
      default: {
        // We failed to open the stream for some reason other than being
        // blocked. Report the failure.
        impl_->pending_uni_stream_queue_.PopFront()->reject(
            QuicError::ForTransport(NGTCP2_STREAM_LIMIT_ERROR));
        continue;
      }
    }
  }
}

// JavaScript callouts

void Session::EmitClose(const QuicError& error) {
  DCHECK(!is_destroyed());
  // When EmitClose is called, the expectation is that the JavaScript
  // side will close the loop and call destroy on the underlying session.
  // If we cannot call out into JavaScript at this point, go ahead and
  // skip to calling destroy directly.
  if (!env()->can_call_into_js()) return Destroy();

  CallbackScope<Session> cb_scope(this);

  Local<Value> argv[] = {
      Integer::New(env()->isolate(), static_cast<int>(error.type())),
      BigInt::NewFromUnsigned(env()->isolate(), error.code()),
      Undefined(env()->isolate()),
  };
  if (error.reason().length() > 0 &&
      !ToV8Value(env()->context(), error.reason()).ToLocal(&argv[2])) {
    return;
  }

  MakeCallback(
      BindingData::Get(env()).session_close_callback(), arraysize(argv), argv);

  // Importantly, the session instance itself should have been destroyed!
  CHECK(is_destroyed());
}

void Session::set_max_datagram_size(uint16_t size) {
  if (!is_destroyed()) {
    impl_->state_->max_datagram_size = size;
  }
}

void Session::EmitGoaway(stream_id last_stream_id) {
  if (is_destroyed()) return;
  if (!env()->can_call_into_js()) return;

  CallbackScope<Session> cb_scope(this);

  Local<Value> argv[] = {
      BigInt::New(env()->isolate(), last_stream_id),
  };

  MakeCallback(
      BindingData::Get(env()).session_goaway_callback(), arraysize(argv), argv);
}

void Session::EmitDatagram(Store&& datagram, DatagramReceivedFlags flag) {
  DCHECK(!is_destroyed());
  if (!env()->can_call_into_js()) return;

  CallbackScope<Session> cbv_scope(this);

  Local<Value> argv[] = {datagram.ToUint8Array(env()),
                         Boolean::New(env()->isolate(), flag.early)};

  MakeCallback(BindingData::Get(env()).session_datagram_callback(),
               arraysize(argv),
               argv);
}

void Session::EmitDatagramStatus(datagram_id id, quic::DatagramStatus status) {
  DCHECK(!is_destroyed());

  if (!env()->can_call_into_js()) return;

  CallbackScope<Session> cb_scope(this);

  auto& state = BindingData::Get(env());

  const auto status_to_string = ([&] {
    switch (status) {
      case DatagramStatus::ACKNOWLEDGED:
        return state.acknowledged_string();
      case DatagramStatus::LOST:
        return state.lost_string();
      case DatagramStatus::ABANDONED:
        return state.abandoned_string();
    }
    UNREACHABLE();
  })();

  Local<Value> argv[] = {BigInt::NewFromUnsigned(env()->isolate(), id),
                         status_to_string};

  MakeCallback(state.session_datagram_status_callback(), arraysize(argv), argv);
}

void Session::EmitHandshakeComplete() {
  DCHECK(!is_destroyed());

  if (!env()->can_call_into_js()) return;

  CallbackScope<Session> cb_scope(this);

  auto isolate = env()->isolate();

  static constexpr auto kServerName = 0;
  static constexpr auto kSelectedAlpn = 1;
  static constexpr auto kCipherName = 2;
  static constexpr auto kCipherVersion = 3;
  static constexpr auto kValidationErrorReason = 4;
  static constexpr auto kValidationErrorCode = 5;

  Local<Value> argv[] = {
      Undefined(isolate),  // The negotiated server name
      Undefined(isolate),  // The selected protocol
      Undefined(isolate),  // Cipher name
      Undefined(isolate),  // Cipher version
      Undefined(isolate),  // Validation error reason
      Undefined(isolate),  // Validation error code
      Boolean::New(isolate, tls_session().early_data_was_attempted()),
      Boolean::New(isolate, tls_session().early_data_was_accepted())};

  auto& tls = tls_session();
  auto peerVerifyError = tls.VerifyPeerIdentity(env());
  if (peerVerifyError.has_value() &&
      (!peerVerifyError->reason.ToLocal(&argv[kValidationErrorReason]) ||
       !peerVerifyError->code.ToLocal(&argv[kValidationErrorCode]))) {
    return;
  }

  if (!ToV8Value(env()->context(), tls.servername())
           .ToLocal(&argv[kServerName]) ||
      !ToV8Value(env()->context(), tls.protocol())
           .ToLocal(&argv[kSelectedAlpn]) ||
      !tls.cipher_name(env()).ToLocal(&argv[kCipherName]) ||
      !tls.cipher_version(env()).ToLocal(&argv[kCipherVersion])) {
    return;
  }

  MakeCallback(BindingData::Get(env()).session_handshake_callback(),
               arraysize(argv),
               argv);
}

void Session::EmitPathValidation(PathValidationResult result,
                                 PathValidationFlags flags,
                                 const ValidatedPath& newPath,
                                 const std::optional<ValidatedPath>& oldPath) {
  DCHECK(!is_destroyed());

  if (!env()->can_call_into_js()) return;

  if (!HasListenerFlag(impl_->state_->listener_flags,
                       SessionListenerFlags::PATH_VALIDATION)) [[likely]] {
    return;
  }

  auto isolate = env()->isolate();
  CallbackScope<Session> cb_scope(this);
  auto& state = BindingData::Get(env());

  const auto resultToString = ([&] {
    switch (result) {
      case PathValidationResult::ABORTED:
        return state.aborted_string();
      case PathValidationResult::FAILURE:
        return state.failure_string();
      case PathValidationResult::SUCCESS:
        return state.success_string();
    }
    UNREACHABLE();
  })();

  Local<Value> argv[] = {
      resultToString,
      SocketAddressBase::Create(env(), newPath.local)->object(),
      SocketAddressBase::Create(env(), newPath.remote)->object(),
      Undefined(isolate),
      Undefined(isolate),
      is_server() ? Undefined(isolate)
                  : Boolean::New(isolate, flags.preferredAddress)};

  if (oldPath.has_value()) {
    argv[3] = SocketAddressBase::Create(env(), oldPath->local)->object();
    argv[4] = SocketAddressBase::Create(env(), oldPath->remote)->object();
  }

  Debug(this, "Notifying JavaScript of path validation");
  MakeCallback(state.session_path_validation_callback(), arraysize(argv), argv);
}

void Session::EmitSessionTicket(Store&& ticket) {
  DCHECK(!is_destroyed());

  if (!env()->can_call_into_js()) return;

  // If there is nothing listening for the session ticket, don't bother
  // emitting.
  if (!HasListenerFlag(impl_->state_->listener_flags,
                       SessionListenerFlags::SESSION_TICKET)) [[likely]] {
    Debug(this, "Session ticket was discarded");
    return;
  }

  CallbackScope<Session> cb_scope(this);

  // Encode the 0-RTT transport params using ngtcp2's matched pair format.
  // This must use ngtcp2_conn_encode_0rtt_transport_params (not the
  // generic ngtcp2_transport_params_encode_versioned) so that the
  // receiver can decode with ngtcp2_conn_decode_and_set_0rtt_transport_params.
  ssize_t tp_size = ngtcp2_conn_encode_0rtt_transport_params(*this, nullptr, 0);
  if (tp_size > 0) {
    JS_TRY_ALLOCATE_BACKING(env(), tp_backing, static_cast<size_t>(tp_size))
    ssize_t tp_written = ngtcp2_conn_encode_0rtt_transport_params(
        *this,
        static_cast<uint8_t*>(tp_backing->Data()),
        static_cast<size_t>(tp_size));
    if (tp_written > 0) {
      Store transport_params(std::move(tp_backing),
                             static_cast<size_t>(tp_written));
      SessionTicket session_ticket(std::move(ticket),
                                   std::move(transport_params));
      Local<Value> argv;
      if (session_ticket.encode(env()).ToLocal(&argv)) [[likely]] {
        MakeCallback(
            BindingData::Get(env()).session_ticket_callback(), 1, &argv);
      }
    }
  }
}

void Session::DestroyAllStreams(const QuicError& error) {
  DCHECK(!is_destroyed());
  // Copy the streams map since streams remove themselves during
  // destruction. Each Destroy() call triggers MakeCallback which
  // can destroy impl_ via JS re-entrancy.
  StreamsMap streams = impl_->streams_;
  for (auto& stream : streams) {
    if (is_destroyed()) return;
    stream.second->Destroy(error);
  }
}

void Session::EmitEarlyDataRejected() {
  DCHECK(!is_destroyed());
  if (!env()->can_call_into_js()) return;

  CallbackScope<Session> cb_scope(this);
  MakeCallback(BindingData::Get(env()).session_early_data_rejected_callback(),
               0,
               nullptr);
}

void Session::EmitNewToken(const uint8_t* token, size_t len) {
  DCHECK(!is_destroyed());
  if (!HasListenerFlag(impl_->state_->listener_flags,
                       SessionListenerFlags::NEW_TOKEN))
    return;
  if (!env()->can_call_into_js()) return;

  CallbackScope<Session> cb_scope(this);

  Local<Value> argv[2];
  auto buf = Buffer::Copy(env(), reinterpret_cast<const char*>(token), len);
  if (!buf.ToLocal(&argv[0])) return;
  argv[1] = SocketAddressBase::Create(
                env(), std::make_shared<SocketAddress>(remote_address()))
                ->object();
  MakeCallback(BindingData::Get(env()).session_new_token_callback(),
               arraysize(argv),
               argv);
}

void Session::EmitStream(const BaseObjectWeakPtr<Stream>& stream) {
  DCHECK(!is_destroyed());

  if (!env()->can_call_into_js()) return;
  CallbackScope<Session> cb_scope(this);

  auto isolate = env()->isolate();
  Local<Value> argv[] = {
      stream->object(),
      Integer::NewFromUnsigned(isolate,
                               static_cast<uint32_t>(stream->direction())),
  };

  MakeCallback(
      BindingData::Get(env()).stream_created_callback(), arraysize(argv), argv);
}

void Session::EmitVersionNegotiation(const ngtcp2_pkt_hd& hd,
                                     const uint32_t* sv,
                                     size_t nsv) {
  DCHECK(!is_destroyed());
  DCHECK(!is_server());

  if (!env()->can_call_into_js()) return;

  CallbackScope<Session> cb_scope(this);
  auto& opts = options();

  // version() is the version that was actually configured for this session.
  // versions are the versions requested by the peer.
  // supported are the versions supported by Node.js.

  LocalVector<Value> versions(env()->isolate(), nsv);
  for (size_t n = 0; n < nsv; n++) {
    versions[n] = Integer::NewFromUnsigned(env()->isolate(), sv[n]);
  }

  // supported are the versions we actually support expressed as a range.
  // The first value is the minimum version, the second is the maximum.
  Local<Value> supported[] = {
      Integer::NewFromUnsigned(env()->isolate(), opts.min_version),
      Integer::NewFromUnsigned(env()->isolate(), opts.version)};

  Local<Value> argv[] = {
      // The version configured for this session.
      Integer::NewFromUnsigned(env()->isolate(), version()),
      // The versions requested.
      Array::New(env()->isolate(), versions.data(), nsv),
      // The versions we actually support.
      Array::New(env()->isolate(), supported, arraysize(supported))};

  MakeCallback(BindingData::Get(env()).session_version_negotiation_callback(),
               arraysize(argv),
               argv);
}

void Session::EmitOrigins(std::vector<std::string>&& origins) {
  DCHECK(!is_destroyed());
  if (!HasListenerFlag(impl_->state_->listener_flags,
                       SessionListenerFlags::ORIGIN))
    return;
  if (!env()->can_call_into_js()) return;

  CallbackScope<Session> cb_scope(this);

  auto isolate = env()->isolate();

  LocalVector<Value> elements(env()->isolate(), origins.size());
  for (size_t i = 0; i < origins.size(); i++) {
    Local<Value> str;
    if (!ToV8Value(env()->context(), origins[i]).ToLocal(&str)) [[unlikely]] {
      return;
    }
    elements[i] = str;
  }

  Local<Value> argv[] = {Array::New(isolate, elements.data(), elements.size())};
  MakeCallback(
      BindingData::Get(env()).session_origin_callback(), arraysize(argv), argv);
}

void Session::EmitKeylog(const char* line) {
  DCHECK(!is_destroyed());
  if (!env()->can_call_into_js()) return;

  auto str = std::string(line);
  Local<Value> argv[] = {Undefined(env()->isolate())};
  if (!ToV8Value(env()->context(), str).ToLocal(&argv[0])) {
    Debug(this, "Failed to convert keylog line to V8 string");
    return;
  }

  MakeCallback(
      BindingData::Get(env()).session_keylog_callback(), arraysize(argv), argv);
}

// ============================================================================

#define V(name, key, no_side_effect)                                           \
  if (no_side_effect) {                                                        \
    SetProtoMethodNoSideEffect(env->isolate(), tmpl, #key, Impl::name);        \
  } else {                                                                     \
    SetProtoMethod(env->isolate(), tmpl, #key, Impl::name);                    \
  }
JS_CONSTRUCTOR_IMPL(Session, session_constructor_template, {
  JS_ILLEGAL_CONSTRUCTOR();
  JS_INHERIT(AsyncWrap);
  JS_CLASS(session);
  SESSION_JS_METHODS(V)
})
#undef V

void Session::RegisterExternalReferences(ExternalReferenceRegistry* registry) {
#define V(name, _, __) registry->Register(Impl::name);
  SESSION_JS_METHODS(V)
#undef V
}

void Session::InitPerIsolate(IsolateData* data, Local<ObjectTemplate> target) {
  // TODO(@jasnell): Implement the per-isolate state
}

void Session::InitPerContext(Realm* realm, Local<Object> target) {
#define V(name, str)                                                           \
  NODE_DEFINE_CONSTANT(target, CC_ALGO_##name);                                \
  NODE_DEFINE_STRING_CONSTANT(target, "CC_ALGO_" #name "_STR", #str);
  CC_ALGOS(V)
#undef V
  // Make sure the Session constructor template is initialized.
  USE(GetConstructorTemplate(realm->env()));

  TransportParams::Initialize(realm->env(), target);
  PreferredAddress::Initialize(realm->env(), target);

  static constexpr auto STREAM_DIRECTION_BIDIRECTIONAL =
      static_cast<uint8_t>(Direction::BIDIRECTIONAL);
  static constexpr auto STREAM_DIRECTION_UNIDIRECTIONAL =
      static_cast<uint8_t>(Direction::UNIDIRECTIONAL);
  static constexpr auto QUIC_PROTO_MAX = NGTCP2_PROTO_VER_MAX;
  static constexpr auto QUIC_PROTO_MIN = NGTCP2_PROTO_VER_MIN;

  NODE_DEFINE_CONSTANT(target, STREAM_DIRECTION_BIDIRECTIONAL);
  NODE_DEFINE_CONSTANT(target, STREAM_DIRECTION_UNIDIRECTIONAL);
  NODE_DEFINE_CONSTANT(target, DEFAULT_MAX_HEADER_LIST_PAIRS);
  NODE_DEFINE_CONSTANT(target, DEFAULT_MAX_HEADER_LENGTH);
  NODE_DEFINE_CONSTANT(target, QUIC_PROTO_MAX);
  NODE_DEFINE_CONSTANT(target, QUIC_PROTO_MIN);

  NODE_DEFINE_STRING_CONSTANT(
      target, "DEFAULT_CIPHERS", TLSContext::DEFAULT_CIPHERS);
  NODE_DEFINE_STRING_CONSTANT(
      target, "DEFAULT_GROUPS", TLSContext::DEFAULT_GROUPS);

#define V(name, _) IDX_STATS_SESSION_##name,
  enum SessionStatsIdx { SESSION_STATS(V) IDX_STATS_SESSION_COUNT };
#undef V

#define V(name, key, __)                                                       \
  auto IDX_STATE_SESSION_##name = offsetof(Session::State, key);
  SESSION_STATE(V)
#undef V

#define V(name, _) NODE_DEFINE_CONSTANT(target, IDX_STATS_SESSION_##name);
  SESSION_STATS(V)
  NODE_DEFINE_CONSTANT(target, IDX_STATS_SESSION_COUNT);
#undef V
#define V(name, _, __) NODE_DEFINE_CONSTANT(target, IDX_STATE_SESSION_##name);
  SESSION_STATE(V)
#undef V
}

}  // namespace quic
}  // namespace node

#endif  // OPENSSL_NO_QUIC
#endif  // HAVE_OPENSSL && HAVE_QUIC
