#pragma once

#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#include <nghttp3/nghttp3.h>
#include <ngtcp2/ngtcp2.h>
#include <util.h>

namespace node {
namespace quic {

// many ngtcp2 functions return 0 to indicate success and non-zero to indicate
// failure. Most of the time, for such functions we don't care about the
// specific return value so we simplify using a macro.
#define NGTCP2_ERR(V) (V != 0)
#define NGTCP2_OK(V) (V == 0)
#define NGTCP2_SUCCESS 0

#define NGTCP2_VERSION_NEGOTIATION_ERROR NGTCP2_VERSION_NEGOTIATION_ERROR_DRAFT
#define HTTP3_ERR_NO_ERROR NGHTTP3_H3_NO_ERROR

#define QUIC_CC_ALGO_CUBIC NGTCP2_CC_ALGO_CUBIC
#define QUIC_CC_ALGO_RENO NGTCP2_CC_ALGO_RENO
#define QUIC_CC_ALGO_BBR NGTCP2_CC_ALGO_BBR
#define QUIC_CC_ALGO_BBR2 NGTCP2_CC_ALGO_BBR2
#define QUIC_MAX_CIDLEN NGTCP2_MAX_CIDLEN

#define QUIC_HEADERS_KIND_INFO Session::Application::HeadersKind::INFO
#define QUIC_HEADERS_KIND_INITIAL Session::Application::HeadersKind::INITIAL
#define QUIC_HEADERS_KIND_TRAILING Session::Application::HeadersKind::TRAILING
#define QUIC_HEADERS_FLAGS_NONE Session::Application::HeadersFlags::NONE
#define QUIC_HEADERS_FLAGS_TERMINAL Session::Application::HeadersFlags::TERMINAL

#define QUIC_STREAM_PRIORITY_DEFAULT                                           \
  Session::Application::StreamPriority::DEFAULT
#define QUIC_STREAM_PRIORITY_LOW Session::Application::StreamPriority::LOW
#define QUIC_STREAM_PRIORITY_HIGH Session::Application::StreamPriority::HIGH
#define QUIC_STREAM_PRIORITY_FLAGS_NONE                                        \
  Session::Application::StreamPriorityFlags::NONE
#define QUIC_STREAM_PRIORITY_FLAGS_NON_INCREMENTAL                             \
  Session::Application::StreamPriorityFlags::NON_INCREMENTAL

#define QUIC_ERROR_TYPE_TRANSPORT QuicError::Type::TRANSPORT
#define QUIC_ERROR_TYPE_APPLICATION QuicError::Type::APPLICATION
#define QUIC_ERROR_TYPE_VERSION_NEGOTIATION QuicError::Type::VERSION_NEGOTIATION
#define QUIC_ERROR_TYPE_IDLE_CLOSE QuicError::Type::IDLE_CLOSE

#define QUIC_ENDPOINT_CLOSE_CONTEXT_CLOSE Endpoint::CloseContext::CLOSE
#define QUIC_ENDPOINT_CLOSE_CONTEXT_BIND_FAILURE                               \
  Endpoint::CloseContext::BIND_FAILURE
#define QUIC_ENDPOINT_CLOSE_CONTEXT_START_FAILURE                              \
  Endpoint::CloseContext::START_FAILURE
#define QUIC_ENDPOINT_CLOSE_CONTEXT_RECEIVE_FAILURE                            \
  Endpoint::CloseContext::RECEIVE_FAILURE
#define QUIC_ENDPOINT_CLOSE_CONTEXT_SEND_FAILURE                               \
  Endpoint::CloseContext::SEND_FAILURE
#define QUIC_ENDPOINT_CLOSE_CONTEXT_LISTEN_FAILURE                             \
  Endpoint::CloseContext::LISTEN_FAILURE

#define QUIC_TRANSPORT_ERRORS(V)                                               \
  V(NO_ERROR)                                                                  \
  V(INTERNAL_ERROR)                                                            \
  V(CONNECTION_REFUSED)                                                        \
  V(FLOW_CONTROL_ERROR)                                                        \
  V(STREAM_LIMIT_ERROR)                                                        \
  V(STREAM_STATE_ERROR)                                                        \
  V(FINAL_SIZE_ERROR)                                                          \
  V(FRAME_ENCODING_ERROR)                                                      \
  V(TRANSPORT_PARAMETER_ERROR)                                                 \
  V(CONNECTION_ID_LIMIT_ERROR)                                                 \
  V(PROTOCOL_VIOLATION)                                                        \
  V(INVALID_TOKEN)                                                             \
  V(APPLICATION_ERROR)                                                         \
  V(CRYPTO_BUFFER_EXCEEDED)                                                    \
  V(KEY_UPDATE_ERROR)                                                          \
  V(AEAD_LIMIT_REACHED)                                                        \
  V(NO_VIABLE_PATH)                                                            \
  V(CRYPTO_ERROR)                                                              \
  V(VERSION_NEGOTIATION_ERROR_DRAFT)

#define HTTP3_APPLICATION_ERRORS(V)                                            \
  V(H3_GENERAL_PROTOCOL_ERROR)                                                 \
  V(H3_INTERNAL_ERROR)                                                         \
  V(H3_STREAM_CREATION_ERROR)                                                  \
  V(H3_CLOSED_CRITICAL_STREAM)                                                 \
  V(H3_FRAME_UNEXPECTED)                                                       \
  V(H3_FRAME_ERROR)                                                            \
  V(H3_EXCESSIVE_LOAD)                                                         \
  V(H3_ID_ERROR)                                                               \
  V(H3_SETTINGS_ERROR)                                                         \
  V(H3_MISSING_SETTINGS)                                                       \
  V(H3_REQUEST_REJECTED)                                                       \
  V(H3_REQUEST_CANCELLED)                                                      \
  V(H3_REQUEST_INCOMPLETE)                                                     \
  V(H3_MESSAGE_ERROR)                                                          \
  V(H3_CONNECT_ERROR)                                                          \
  V(H3_VERSION_FALLBACK)                                                       \
  V(QPACK_DECOMPRESSION_FAILED)                                                \
  V(QPACK_ENCODER_STREAM_ERROR)                                                \
  V(QPACK_DECODER_STREAM_ERROR)

#define QUIC_FLAG(Name)                                                        \
  ((flags & Name) == Name) /* NOLINT(runtime/references) */

#define QUIC_NO_COPY_OR_MOVE(Name)                                             \
  Name(Name&) = delete;                                                        \
  Name(const Name&) = delete;                                                  \
  Name(const Name&&) = delete;                                                 \
  Name& operator=(Name&) = delete;                                             \
  Name& operator=(const Name&) = delete;                                       \
  Name& operator=(Name&&) = delete;

#define QUIC_MOVE_NO_COPY(Name)                                                \
  Name(Name&&) = default;                                                      \
  Name(Name&) = delete;                                                        \
  Name(const Name&) = delete;                                                  \
  Name& operator=(Name&&) = default;                                           \
  Name& operator=(Name&) = delete;                                             \
  Name& operator=(const Name&) = delete;

#define QUIC_COPY_NO_MOVE(Name)                                                \
  Name(Name&&) = delete;                                                       \
  Name(Name&) = default;                                                       \
  Name(const Name&) = default;                                                 \
  Name& operator=(Name&&) = delete;                                            \
  Name& operator=(Name&) = default;                                            \
  Name& operator=(const Name&) = default;

// The constructors are v8::FunctionTemplates that are stored persistently in
// the quic::BindingState class. These are used for creating instances of the
// various objects, as well as for performing HasInstance type checks. We choose
// to store these on the BindingData instead of the Environment in order to keep
// like-things together and to reduce the additional memory overhead on the
// Environment when QUIC is not being used.
#define QUIC_CONSTRUCTORS(V)                                                   \
  V(arraybufferviewsource)                                                     \
  V(blobsource)                                                                \
  V(endpoint)                                                                  \
  V(endpoint_config)                                                           \
  V(http3_options)                                                             \
  V(jsquicbufferconsumer)                                                      \
  V(logstream)                                                                 \
  V(nullsource)                                                                \
  V(random_connection_id_strategy)                                             \
  V(send_wrap)                                                                 \
  V(session)                                                                   \
  V(session_options)                                                           \
  V(stream)                                                                    \
  V(streamsource)                                                              \
  V(streambasesource)                                                          \
  V(udp)                                                                       \
  V(cidfactorybase)                                                            \
  V(sessionticket)

// The callbacks are persistent v8::Function references that are set in the
// quic::BindingState used to communicate data and events back out to the JS
// environment. They are set once from the JavaScript side when the
// internalBinding('quic') is first loaded.
//
// The corresponding implementations of the callbacks can be found in
// lib/internal/quic/binding.js
#define QUIC_JS_CALLBACKS(V)                                                   \
  V(endpoint_done, EndpointDone)                                               \
  V(endpoint_error, EndpointError)                                             \
  V(session_new, SessionNew)                                                   \
  V(session_client_hello, SessionClientHello)                                  \
  V(session_close, SessionClose)                                               \
  V(session_error, SessionError)                                               \
  V(session_datagram, SessionDatagram)                                         \
  V(session_datagram_ack, SessionDatagramAcknowledged)                         \
  V(session_datagram_lost, SessionDatagramLost)                                \
  V(session_handshake, SessionHandshake)                                       \
  V(session_ocsp_request, SessionOcspRequest)                                  \
  V(session_ocsp_response, SessionOcspResponse)                                \
  V(session_ticket, SessionTicket)                                             \
  V(session_version_negotiation, SessionVersionNegotiation)                    \
  V(session_path_validation, SessionPathValidation)                            \
  V(stream_close, StreamClose)                                                 \
  V(stream_error, StreamError)                                                 \
  V(stream_data, StreamData)                                                   \
  V(stream_created, StreamCreated)                                             \
  V(stream_reset, StreamReset)                                                 \
  V(stream_headers, StreamHeaders)                                             \
  V(stream_blocked, StreamBlocked)                                             \
  V(stream_trailers, StreamTrailers)

// The strings are persistent/eternal v8::Strings that are set in the
// quic::BindingState.
#define QUIC_STRINGS(V)                                                        \
  V(http3_alpn, &NGHTTP3_ALPN_H3[1])                                           \
  V(aborted, "aborted")                                                        \
  V(ack_delay_exponent, "ackDelayExponent")                                    \
  V(active_connection_id_limit, "activeConnectionIdLimit")                     \
  V(address_lru_size, "addressLRUSize")                                        \
  V(arraybufferviewsource, "ArrayBufferViewSource")                            \
  V(blobsource, "BlobSource")                                                  \
  V(ca, "ca")                                                                  \
  V(cc_algorithm, "ccAlgorithm")                                               \
  V(certs, "certs")                                                            \
  V(cidfactorybase, "CIDFactoryBase")                                          \
  V(ciphers, "ciphers")                                                        \
  V(client_hello, "clientHello")                                               \
  V(crl, "crl")                                                                \
  V(disable_active_migration, "disableActiveMigration")                        \
  V(disable_stateless_reset, "disableStatelessReset")                          \
  V(enable_tls_trace, "enableTLSTrace")                                        \
  V(endpoint, "Endpoint")                                                      \
  V(endpoint_udp, "Endpoint::UDP")                                             \
  V(endpoint_options, "Endpoint::Options")                                     \
  V(err_stream_closed, "ERR_QUIC_STREAM_CLOSED")                               \
  V(failure, "failure")                                                        \
  V(groups, "groups")                                                          \
  V(http3_application_options, "Http3ApplicationOptions")                      \
  V(initial_max_stream_data_bidi_local, "initialMaxStreamDataBidiLocal")       \
  V(initial_max_stream_data_bidi_remote, "initialMaxStreamDataBidiRemote")     \
  V(initial_max_stream_data_uni, "initialMaxStreamDataUni")                    \
  V(initial_max_data, "initialMaxData")                                        \
  V(initial_max_streams_bidi, "initialMaxStreamsBidi")                         \
  V(initial_max_streams_uni, "initialMaxStreamsUni")                           \
  V(ipv6_only, "ipv6Only")                                                     \
  V(jsquicbufferconsumer, "JSQuicBufferConsumer")                              \
  V(keys, "keys")                                                              \
  V(keylog, "keylog")                                                          \
  V(logstream, "LogStream")                                                    \
  V(max_ack_delay, "maxAckDelay")                                              \
  V(max_connections_per_host, "maxConnectionsPerHost")                         \
  V(max_connections_total, "maxConnectionsTotal")                              \
  V(max_datagram_frame_size, "maxDatagramFrameSize")                           \
  V(max_field_section_size, "maxFieldSectionSize")                             \
  V(max_header_pairs, "maxHeaderPairs")                                        \
  V(max_header_length, "maxHeaderLength")                                      \
  V(max_idle_timeout, "maxIdleTimeout")                                        \
  V(max_payload_size, "maxPayloadSize")                                        \
  V(max_stateless_resets, "maxStatelessResets")                                \
  V(max_stream_window_override, "maxStreamWindowOverride")                     \
  V(max_window_override, "maxWindowOverride")                                  \
  V(nullsource, "NullSource")                                                  \
  V(ocsp, "ocsp")                                                              \
  V(packetwrap, "PacketWrap")                                                  \
  V(pskcallback, "pskCallback")                                                \
  V(qlog, "qlog")                                                              \
  V(qpack_blocked_streams, "qpackBlockedStreams")                              \
  V(qpack_max_table_capacity, "qpackMaxTableCapacity")                         \
  V(qpack_encoder_max_dtable_capacity, "qpackEncoderMaxTableCapacity")         \
  V(reject_unauthorized, "rejectUnauthorized")                                 \
  V(request_peer_certificate, "requestPeerCertificate")                        \
  V(retry_limit, "retryLimit")                                                 \
  V(retry_token_expiration, "retryTokenExpiration")                            \
  V(rx_packet_loss, "rxPacketLoss")                                            \
  V(session, "Session")                                                        \
  V(sessionticket, "SessionTicket")                                            \
  V(session_id, "sessionID")                                                   \
  V(session_options, "Session::Options")                                       \
  V(stream, "Stream")                                                          \
  V(streamsource, "StreamSource")                                              \
  V(streambasesource, "StreamBaseSource")                                      \
  V(success, "success")                                                        \
  V(token_expiration, "tokenExpiration")                                       \
  V(tx_packet_loss, "txPacketLoss")                                            \
  V(udp_receive_buffer_size, "receiveBufferSize")                              \
  V(udp_send_buffer_size, "sendBufferSize")                                    \
  V(udp_ttl, "ttl")                                                            \
  V(unacknowledged_packet_threshold, "unacknowledgedPacketThreshold")          \
  V(verify_hostname_identity, "verifyHostnameIdentity")                        \
  V(validate_address, "validateAddress")

#define SESSION_STATS(V)                                                       \
  V(CREATED_AT, created_at, "Created at")                                      \
  V(HANDSHAKE_COMPLETED_AT, handshake_completed_at, "Handshake completed")     \
  V(HANDSHAKE_CONFIRMED_AT, handshake_confirmed_at, "Handshake confirmed")     \
  V(GRACEFUL_CLOSING_AT, graceful_closing_at, "Graceful Closing")              \
  V(CLOSING_AT, closing_at, "Closing")                                         \
  V(DESTROYED_AT, destroyed_at, "Destroyed at")                                \
  V(BYTES_RECEIVED, bytes_received, "Bytes received")                          \
  V(BYTES_SENT, bytes_sent, "Bytes sent")                                      \
  V(BIDI_STREAM_COUNT, bidi_stream_count, "Bidi stream count")                 \
  V(UNI_STREAM_COUNT, uni_stream_count, "Uni stream count")                    \
  V(STREAMS_IN_COUNT, streams_in_count, "Streams in count")                    \
  V(STREAMS_OUT_COUNT, streams_out_count, "Streams out count")                 \
  V(KEYUPDATE_COUNT, keyupdate_count, "Key update count")                      \
  V(LOSS_RETRANSMIT_COUNT, loss_retransmit_count, "Loss retransmit count")     \
  V(MAX_BYTES_IN_FLIGHT, max_bytes_in_flight, "Max bytes in flight")           \
  V(BLOCK_COUNT, block_count, "Block count")                                   \
  V(BYTES_IN_FLIGHT, bytes_in_flight, "Bytes in flight")                       \
  V(CONGESTION_RECOVERY_START_TS,                                              \
    congestion_recovery_start_ts,                                              \
    "Congestion recovery start time")                                          \
  V(CWND, cwnd, "Size of the congestion window")                               \
  V(DELIVERY_RATE_SEC, delivery_rate_sec, "Delivery bytes/sec")                \
  V(FIRST_RTT_SAMPLE_TS, first_rtt_sample_ts, "First RTT sample time")         \
  V(INITIAL_RTT, initial_rtt, "Initial RTT")                                   \
  V(LAST_TX_PKT_TS, last_tx_pkt_ts, "Last TX packet time")                     \
  V(LATEST_RTT, latest_rtt, "Latest RTT")                                      \
  V(LOSS_DETECTION_TIMER,                                                      \
    loss_detection_timer,                                                      \
    "Loss detection timer deadline")                                           \
  V(LOSS_TIME, loss_time, "Loss time")                                         \
  V(MAX_UDP_PAYLOAD_SIZE, max_udp_payload_size, "Max UDP payload size")        \
  V(MIN_RTT, min_rtt, "Minimum RTT so far")                                    \
  V(PTO_COUNT, pto_count, "PTO count")                                         \
  V(RTTVAR, rttvar, "Mean deviation of observed RTT")                          \
  V(SMOOTHED_RTT, smoothed_rtt, "Smoothed RTT")                                \
  V(SSTHRESH, ssthresh, "Slow start threshold")

#define ENDPOINT_STATS(V)                                                      \
  V(CREATED_AT, created_at, "Created at")                                      \
  V(DESTROYED_AT, destroyed_at, "Destroyed at")                                \
  V(BYTES_RECEIVED, bytes_received, "Bytes received")                          \
  V(BYTES_SENT, bytes_sent, "Bytes sent")                                      \
  V(PACKETS_RECEIVED, packets_received, "Packets received")                    \
  V(PACKETS_SENT, packets_sent, "Packets sent")                                \
  V(SERVER_SESSIONS, server_sessions, "Server sessions")                       \
  V(CLIENT_SESSIONS, client_sessions, "Client sessions")                       \
  V(SERVER_BUSY_COUNT, server_busy_count, "Server busy count")

#define STREAM_STATS(V)                                                        \
  V(CREATED_AT, created_at, "Created At")                                      \
  V(RECEIVED_AT, received_at, "Last Received At")                              \
  V(ACKED_AT, acked_at, "Last Acknowledged At")                                \
  V(CLOSING_AT, closing_at, "Closing At")                                      \
  V(DESTROYED_AT, destroyed_at, "Destroyed At")                                \
  V(BYTES_RECEIVED, bytes_received, "Bytes Received")                          \
  V(BYTES_SENT, bytes_sent, "Bytes Sent")                                      \
  V(MAX_OFFSET, max_offset, "Max Offset")                                      \
  V(MAX_OFFSET_ACK, max_offset_ack, "Max Acknowledged Offset")                 \
  V(MAX_OFFSET_RECV, max_offset_received, "Max Received Offset")               \
  V(FINAL_SIZE, final_size, "Final Size")

#define SESSION_STATE(V)                                                       \
  /* Set if the JavaScript wrapper has a path-validation event listener */     \
  V(PATH_VALIDATION, path_validation, uint8_t)                                 \
  /* Set if the JavaScript wrapper has a version-negotiation event listener */ \
  V(VERSION_NEGOTIATION, version_negotiation, uint8_t)                         \
  /* Set if the JavaScript wrapper has a datagram event listener */            \
  V(DATAGRAM, datagram, uint8_t)                                               \
  /* Set if the JavaScript wrapper has a session-ticket event listener */      \
  V(SESSION_TICKET, session_ticket, uint8_t)                                   \
  /* Set if the JavaScript wrapper has a client-hello event listener */        \
  V(CLIENT_HELLO, client_hello, uint8_t)                                       \
  /* Set when the client-hello event has been responded to */                  \
  V(CLIENT_HELLO_DONE, client_hello_done, uint8_t)                             \
  /* Set when the JavaScript wrapper has a ocsp event listener */              \
  V(OCSP, ocsp, uint8_t)                                                       \
  /* Set when the ocsp event has been responded to */                          \
  V(OCSP_DONE, ocsp_done, uint8_t)                                             \
  V(CLOSING, closing, uint8_t)                                                 \
  V(CONNECTION_CLOSE_SCOPE, in_connection_close_scope, uint8_t)                \
  V(DESTROYED, destroyed, uint8_t)                                             \
  V(GRACEFUL_CLOSING, graceful_closing, uint8_t)                               \
  V(HANDSHAKE_COMPLETED, handshake_completed, uint8_t)                         \
  V(HANDSHAKE_CONFIRMED, handshake_confirmed, uint8_t)                         \
  V(NGTCP2_CALLBACK, in_ngtcp2_callback, uint8_t)                              \
  V(STATELESS_RESET, stateless_reset, uint8_t)                                 \
  V(SILENT_CLOSE, silent_close, uint8_t)                                       \
  V(STREAM_OPEN_ALLOWED, stream_open_allowed, uint8_t)                         \
  V(USING_PREFERRED_ADDRESS, using_preferred_address, uint8_t)                 \
  V(PRIORITY_SUPPORTED, priority_supported, uint8_t)                           \
  V(WRAPPED, wrapped, uint8_t)

#define ENDPOINT_STATE(V)                                                      \
  /* Bound to the UDP port */                                                  \
  V(BOUND, bound, uint8_t)                                                     \
  /* Receiving packets on the UDP port */                                      \
  V(RECEIVING, receiving, uint8_t)                                             \
  /* Listening as a QUIC server */                                             \
  V(LISTENING, listening, uint8_t)                                             \
  /* In the process of closing down */                                         \
  V(CLOSING, closing, uint8_t)                                                 \
  /* In the process of closing down, waiting for pending send callbacks */     \
  V(WAITING_FOR_CALLBACKS, waiting_for_callbacks, uint8_t)                     \
  /* Temporarily paused serving new initial requests */                        \
  V(BUSY, busy, uint8_t)                                                       \
  /* The number of pending send callbacks */                                   \
  V(PENDING_CALLBACKS, pending_callbacks, size_t)

#define STREAM_STATE(V)                                                        \
  V(FIN_SENT, fin_sent, uint8_t)                                               \
  V(FIN_RECEIVED, fin_received, uint8_t)                                       \
  V(READ_ENDED, read_ended, uint8_t)                                           \
  V(TRAILERS, trailers, uint8_t)                                               \
  V(DESTROYED, destroyed, uint8_t)                                             \
  /* Set when the JavaScript wrapper has a data event listener */              \
  V(DATA, data, uint8_t)                                                       \
  V(PAUSED, paused, uint8_t)                                                   \
  V(RESET, reset, uint8_t)                                                     \
  V(ID, id, int64_t)

#define SOURCE_STATE(V)                                                        \
  V(EOS, eos, uint8_t)                                                         \
  V(CLOSED, closed, uint8_t)

#define V(name, _, __) IDX_STATS_SESSION_##name,
enum SessionStatsIdx : int { SESSION_STATS(V) IDX_STATS_SESSION_COUNT };
#undef V

#define V(name, _, __) IDX_STATE_SESSION_##name,
enum SessionStateIdx { SESSION_STATE(V) IDX_STATE_SESSION_COUNT };
#undef V

#define V(name, _, __) IDX_STATS_ENDPOINT_##name,
enum EndpointStatsIdx { ENDPOINT_STATS(V) IDX_STATS_ENDPOINT_COUNT };
#undef V

#define V(name, _, __) IDX_STATE_ENDPOINT_##name,
enum EndpointStateIdx { ENDPOINT_STATE(V) IDX_STATE_ENDPOINT_COUNT };
#undef V

#define V(name, _, __) IDX_STATS_STREAM_##name,
enum StreamStatsIdx { STREAM_STATS(V) IDX_STATS_STREAM_COUNT };
#undef V

#define V(name, _, __) IDX_STATE_STREAM_##name,
enum StreamStateIdx { STREAM_STATE(V) IDX_STATE_STREAM_COUNT };
#undef V

#define V(name, _, __) IDX_STATE_STREAM_##name,
enum SourceStateIdx { SOURCE_STATE(V) IDX_STATE_SOURCE_COUNT };
#undef V

using error_code = uint64_t;
using quic_version = uint32_t;
using datagram_id = uint64_t;
using stream_id = int64_t;

// The kNNN-style constants are used internally only.
// The ALLCAPS-style constants are intended to be exposed to JavaScript.

constexpr uint64_t kMaxUint64 = std::numeric_limits<int64_t>::max();
constexpr size_t kMaxSizeT = std::numeric_limits<size_t>::max();
constexpr size_t kMaxVectorCount = 16;
constexpr size_t kDefaultMaxPacketLength = NGTCP2_MAX_UDP_PAYLOAD_SIZE;
constexpr size_t kTokenSecretLen = 16;
constexpr uint64_t kSocketAddressInfoTimeout = 10000000000;  // 10 seconds
constexpr uint8_t kRetryTokenMagic = 0xb6;
constexpr uint8_t kTokenMagic = 0x36;
constexpr uint8_t kCryptoTokenKeylen = 16;
constexpr uint8_t kCryptoTokenIvlen = 12;
constexpr uint8_t kTokenRandLen = 16;
constexpr uint8_t kMaxRetryTokenLen =
    1 + sizeof(uint64_t) + NGTCP2_MAX_CIDLEN + 16 + kTokenRandLen;
constexpr uint8_t kMinRetryTokenLen = 1 + kTokenRandLen;
constexpr uint8_t kMaxTokenLen = 1 + sizeof(uint64_t) + 16 + kTokenRandLen;

constexpr uint64_t DEFAULT_ACTIVE_CONNECTION_ID_LIMIT = 2;
constexpr uint64_t DEFAULT_MAX_STREAM_DATA_BIDI_LOCAL = 256 * 1024;
constexpr uint64_t DEFAULT_MAX_STREAM_DATA_BIDI_REMOTE = 256 * 1024;
constexpr uint64_t DEFAULT_MAX_STREAM_DATA_UNI = 256 * 1024;
constexpr uint64_t DEFAULT_MAX_STREAMS_BIDI = 100;
constexpr uint64_t DEFAULT_MAX_STREAMS_UNI = 3;
constexpr uint64_t DEFAULT_MAX_DATA = 1 * 1024 * 1024;
constexpr uint64_t DEFAULT_MAX_IDLE_TIMEOUT = 10;  // seconds
constexpr size_t DEFAULT_MAX_CONNECTIONS =
    std::min<size_t>(kMaxSizeT, static_cast<size_t>(kMaxSafeJsInteger));
constexpr size_t DEFAULT_MAX_CONNECTIONS_PER_HOST = 100;
constexpr size_t DEFAULT_MAX_SOCKETADDRESS_LRU_SIZE =
    (DEFAULT_MAX_CONNECTIONS_PER_HOST * 10);
constexpr size_t DEFAULT_MAX_STATELESS_RESETS = 10;
constexpr size_t DEFAULT_MAX_RETRY_LIMIT = 10;
constexpr uint64_t DEFAULT_RETRYTOKEN_EXPIRATION = 10;  // seconds
constexpr uint64_t DEFAULT_TOKEN_EXPIRATION = 10;       // seconds
constexpr uint64_t NGTCP2_APP_NOERROR = 65280;

constexpr auto DEFAULT_CIPHERS = "TLS_AES_128_GCM_SHA256:"
                                 "TLS_AES_256_GCM_SHA384:"
                                 "TLS_CHACHA20_POLY1305_"
                                 "SHA256:TLS_AES_128_CCM_SHA256";
constexpr auto DEFAULT_GROUPS = "X25519:P-256:P-384:P-521";

enum class EndpointLabel {
  LOCAL,
  REMOTE,
};

enum class Direction {
  BIDIRECTIONAL,
  UNIDIRECTIONAL,
};

// Used for routable CIDs if we implement those
// constexpr size_t kMaxDynamicServerIDLength = 7;
// constexpr size_t kMinDynamicServerIDLength = 1;

static constexpr size_t kCryptoTokenSecretlen = 32;
static constexpr size_t kCryptoKeylen = 64;
static constexpr size_t kCryptoIvlen = 64;
static constexpr char kQuicClientEarlyTrafficSecret[] =
    "QUIC_CLIENT_EARLY_TRAFFIC_SECRET";
static constexpr char kQuicClientHandshakeTrafficSecret[] =
    "QUIC_CLIENT_HANDSHAKE_TRAFFIC_SECRET";
static constexpr char kQuicClientTrafficSecret0[] =
    "QUIC_CLIENT_TRAFFIC_SECRET_0";
static constexpr char kQuicServerHandshakeTrafficSecret[] =
    "QUIC_SERVER_HANDSHAKE_TRAFFIC_SECRET";
static constexpr char kQuicServerTrafficSecret[] =
    "QUIC_SERVER_TRAFFIC_SECRET_0";

#define HAS_INSTANCE()                                                         \
  static inline bool HasInstance(Environment* env,                             \
                                 v8::Local<v8::Value> value) {                 \
    return !value.IsEmpty() && value->IsObject() &&                            \
           GetConstructorTemplate(env)->HasInstance(value);                    \
  }

#define DEBUG(self, message) Debug(self, message);

#define DEBUG_ARGS(self, message, ...) Debug(self, message, __VA_ARGS__);

}  // namespace quic
}  // namespace node

#endif  // defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS
