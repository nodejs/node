#pragma once

#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#include <env.h>
#include <memory_tracker.h>
#include <nghttp3/nghttp3.h>
#include <ngtcp2/ngtcp2.h>
#include <node_internals.h>
#include <node_realm.h>
#include <node_sockaddr.h>
#include <v8.h>
#include <concepts>
#include <string>
#include "defs.h"

namespace node::quic {

template <typename T>
concept OneByteType = sizeof(T) == 1;

struct Path final : public ngtcp2_path {
  explicit Path(const SocketAddress& local, const SocketAddress& remote);
  Path(Path&& other) noexcept = default;
  Path& operator=(Path&& other) noexcept = default;
  DISALLOW_COPY(Path)
  std::string ToString() const;
};

struct PathStorage final : public ngtcp2_path_storage {
  explicit PathStorage();
  PathStorage(PathStorage&& other) noexcept = default;
  PathStorage& operator=(PathStorage&& other) noexcept = default;
  DISALLOW_COPY(PathStorage)

  void Reset();
  void CopyTo(PathStorage* path) const;

  bool operator==(const PathStorage& other) const;
  bool operator!=(const PathStorage& other) const;
};

// Store acts as a wrapper around a v8::BackingStore, providing a convenient
// abstraction to map it to various buffer types used in QUIC, HTTP/3, and
// libuv, taking care of the necessary adjustments for length and offset.
class Store final : public MemoryRetainer {
 public:
  Store() = default;

  Store(std::shared_ptr<v8::BackingStore> store,
        size_t length,
        size_t offset = 0);
  Store(std::unique_ptr<v8::BackingStore> store,
        size_t length,
        size_t offset = 0);
  Store(Store&& other) noexcept = default;
  Store& operator=(Store&& other) noexcept = default;
  DISALLOW_COPY(Store)

  // Creates a Store from the contents of an ArrayBuffer, always detaching
  // it in the process. An empty Maybe will be returned if the ArrayBuffer
  // is not detachable or detaching failed (likely due to a detach key
  // mismatch).
  static v8::Maybe<Store> From(
    v8::Local<v8::ArrayBuffer> buffer,
    v8::Local<v8::Value> detach_key = v8::Local<v8::Value>());

  // Creates a Store from the contents of an ArrayBufferView, always detaching
  // it in the process. An empty Maybe will be returned if the ArrayBuffer
  // is not detachable or detaching failed (likely due to a detach key
  // mismatch).
  static v8::Maybe<Store> From(
    v8::Local<v8::ArrayBufferView> view,
    v8::Local<v8::Value> detach_key = v8::Local<v8::Value>());

  v8::Local<v8::Uint8Array> ToUint8Array(Environment* env) const;
  inline v8::Local<v8::Uint8Array> ToUint8Array(Realm* realm) const {
    return ToUint8Array(realm->env());
  }

  operator uv_buf_t() const;
  operator ngtcp2_vec() const;
  operator nghttp3_vec() const;
  operator bool() const;
  size_t length() const;

  // Returns the total length of the underlying store, not just the
  // length of the view. This is useful for memory accounting.
  size_t total_length() const;

  void MemoryInfo(MemoryTracker* tracker) const override;
  SET_MEMORY_INFO_NAME(Store)
  SET_SELF_SIZE(Store)

 private:
  template <typename T, OneByteType N>
  T convert() const;
  std::shared_ptr<v8::BackingStore> store_;
  size_t length_ = 0;
  size_t offset_ = 0;

  // Because Store holds the v8::BackingStore and not the v8::ArrayBuffer,
  // etc, the memory held by the Store is not directly, automatically
  // associated with a v8::Isolate, and is therefore not accounted for
  // as external memory. It is the responsibility of the owner of the
  // Store instance to ensure the memory remains accounted for.
};

// Periodically, these need to be updated to match the latest ngtcp2 defs.
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
  V(VERSION_NEGOTIATION_ERROR)

// Periodically, these need to be updated to match the latest nghttp3 defs.
#define HTTP3_APPLICATION_ERRORS(V)                                            \
  V(H3_NO_ERROR)                                                               \
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

class QuicError final : public MemoryRetainer {
 public:
  // The known error codes for the transport namespace.
  enum class TransportError : error_code {
#define V(name) name = NGTCP2_##name,
    QUIC_TRANSPORT_ERRORS(V)
#undef V
  };

  // Every QUIC application defines its own error codes in the application
  // namespace. These are managed independently of each other and may overlap
  // with other applications and even the transport namespace. The only way
  // to correctly interpret an application error code is to know which
  // application is being used. For convenience, we define constants for the
  // known HTTP/3 application error codes here.
  enum class Http3Error : error_code {
#define V(name) name = NGHTTP3_##name,
    HTTP3_APPLICATION_ERRORS(V)
#undef V
  };

  static constexpr error_code QUIC_NO_ERROR = NGTCP2_NO_ERROR;
  static constexpr error_code HTTP3_NO_ERROR_CODE = NGHTTP3_H3_NO_ERROR;

  // In QUIC, Errors are represented as namespaced 64-bit error codes.
  // The error code only has meaning within the context of a specific
  // namespace. The QuicError::Type enum defines the available namespaces.
  // There are essentially two namespaces: transport and application, with
  // a few additional special-cases that are variants of the transport
  // namespace.
  enum class Type {
    TRANSPORT = NGTCP2_CCERR_TYPE_TRANSPORT,
    APPLICATION = NGTCP2_CCERR_TYPE_APPLICATION,

    // These are special cases of transport errors.
    VERSION_NEGOTIATION = NGTCP2_CCERR_TYPE_VERSION_NEGOTIATION,
    IDLE_CLOSE = NGTCP2_CCERR_TYPE_IDLE_CLOSE,
    DROP_CONNECTION = NGTCP2_CCERR_TYPE_DROP_CONN,
    RETRY = NGTCP2_CCERR_TYPE_RETRY,
  };

  // Do not use the constructors directly in regular use. Use the static
  // factory methods instead. Those will ensure that the underlying
  // ngtcp2_ccerr is initialized correctly based on the type of error
  // being created.
  explicit QuicError(const std::string& reason = "");
  explicit QuicError(const ngtcp2_ccerr* ptr);
  explicit QuicError(const ngtcp2_ccerr& error);
  QuicError(QuicError&& other) noexcept = default;
  QuicError& operator=(QuicError&& other) noexcept = default;
  DISALLOW_COPY(QuicError)

  Type type() const;
  error_code code() const;
  const std::string_view reason() const;
  uint64_t frame_type() const;

  operator const ngtcp2_ccerr&() const;
  operator const ngtcp2_ccerr*() const;

  // Crypto errors are a subset of transport errors. The error code includes
  // the TLS alert code embedded within it.
  bool is_crypto_error() const;
  std::optional<int> get_crypto_error() const;

  // Note that since application errors are application-specific and we
  // don't know which application is being used here, it is possible that
  // the comparing two different QuicError instances from different applications
  // will return true even if they are not semantically equivalent. This should
  // not be a problem in practice.
  bool operator==(const QuicError& other) const;
  bool operator!=(const QuicError& other) const;

  void MemoryInfo(MemoryTracker* tracker) const override;
  SET_MEMORY_INFO_NAME(QuicError)
  SET_SELF_SIZE(QuicError)

  std::string ToString() const;

  // Returns an array containing [type, code, reason], where type is a string
  // representation of the error type, code is a bigint representation of the
  // error code, and reason is a string representation of the error reason, or
  // undefined if the reason is the empty string. This is expected to be used
  // to construct a JS error object from the information in JS.
  v8::MaybeLocal<v8::Value> ToV8Value(Environment* env) const;
  inline v8::MaybeLocal<v8::Value> ToV8Value(Realm* realm) const {
    return ToV8Value(realm->env());
  }

  // Utility functions for getting the default error reason strings for
  // internal error codes returned by the underlying ngtcp2/nghttp3 libraries.
  static std::string reason_for_liberr(int liberr);
  static std::string reason_for_h3_liberr(int liberr);

  // Utility functions for checking if the given internal error codes are
  // considered to be fatal by the underlying ngtcp2/nghttp3 libraries.
  static bool is_fatal_liberror(int liberr);
  static bool is_fatal_h3_liberror(int liberr);

  // Utility functions for converting between ngtcp2/nghttp3 internal error
  // codes to the corresponding QUIC error codes.
  static error_code liberr_to_code(int liberr);
  static error_code h3_liberr_to_code(int liberr);

  // Utility functions for creating QuicError instances.
  // The reason is expected to always be UTF-8 encoded.
  static const QuicError ForTransport(TransportError code,
                                      std::string reason = "");
  static const QuicError ForTransport(error_code code, std::string reason = "");
  static const QuicError ForApplication(Http3Error code,
                                        std::string reason = "");
  static const QuicError ForApplication(error_code code,
                                        std::string reason = "");
  static const QuicError ForVersionNegotiation(std::string reason = "");
  static const QuicError ForIdleClose(std::string reason = "");
  static const QuicError ForDropConnection(std::string reason = "");
  static const QuicError ForRetry(std::string reason = "");
  static const QuicError ForNgtcp2Error(int code, std::string reason = "");
  static const QuicError ForTlsAlert(int code, std::string reason = "");

  static const QuicError FromConnectionClose(ngtcp2_conn* session);

#define V(name) static const QuicError TRANSPORT_##name;
  QUIC_TRANSPORT_ERRORS(V)
#undef V
  static const QuicError HTTP3_NO_ERROR;
  static const QuicError VERSION_NEGOTIATION;
  static const QuicError IDLE_CLOSE;
  static const QuicError DROP_CONNECTION;
  static const QuicError RETRY;
  static const QuicError INTERNAL_ERROR;

 private:
  const uint8_t* reason_c_str() const;

  std::string reason_;
  ngtcp2_ccerr error_;
  const ngtcp2_ccerr* ptr_ = nullptr;
};

// Marked maybe_unused because these are used in the tests but not in the
// production code.
[[maybe_unused]] static bool operator==(const QuicError::TransportError& lhs,
                                        error_code rhs) {
  return static_cast<error_code>(lhs) == rhs;
}
[[maybe_unused]] static bool operator==(const QuicError::Http3Error& lhs,
                                        error_code rhs) {
  return static_cast<error_code>(lhs) == rhs;
}

}  // namespace node::quic

#endif  // defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS
