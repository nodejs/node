#pragma once

#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS
#if HAVE_OPENSSL && NODE_OPENSSL_HAS_QUIC

#include <async_wrap.h>
#include <memory_tracker.h>
#include <node_sockaddr.h>
#include <optional>
#include "bindingdata.h"
#include "cid.h"
#include "data.h"

namespace node {
namespace quic {

class Endpoint;

// TODO(@jasnell): This is a placeholder definition of Session that
// includes only the pieces needed by Endpoint right now. The full
// Session definition will be provided separately.
class Session final : public AsyncWrap {
 public:
  struct Config {
    SocketAddress local_address;
    SocketAddress remote_address;
    std::optional<CID> ocid = std::nullopt;
    std::optional<CID> retry_scid = std::nullopt;

    Config(Side side,
           const Endpoint& endpoint,
           const CID& scid,
           const SocketAddress& local_address,
           const SocketAddress& remote_address,
           uint32_t min_quic_version,
           uint32_t max_quic_version,
           const CID& ocid = CID::kInvalid);
  };
  struct Options : public MemoryRetainer {
    SET_NO_MEMORY_INFO()
    SET_MEMORY_INFO_NAME(Session::Options)
    SET_SELF_SIZE(Options)

    static v8::Maybe<Options> From(Environment* env,
                                   v8::Local<v8::Value> value);
  };

  static BaseObjectPtr<Session> Create(BaseObjectPtr<Endpoint> endpoint,
                                       const Config& config,
                                       const Options& options);

  enum class CloseMethod {
    // Roundtrip through JavaScript, causing all currently opened streams
    // to be closed. An attempt will be made to send a CONNECTION_CLOSE
    // frame to the peer. If closing while within the ngtcp2 callback scope,
    // sending the CONNECTION_CLOSE will be deferred until the scope exits.
    DEFAULT,
    // The connected peer will not be notified.
    SILENT,
    // Closing gracefully disables the ability to open or accept new streams for
    // this Session. Existing streams are allowed to close naturally on their
    // own.
    // Once called, the Session will be immediately closed once there are no
    // remaining streams. No notification is given to the connected peer that we
    // are in a graceful closing state. A CONNECTION_CLOSE will be sent only
    // once
    // Close() is called.
    GRACEFUL
  };

  Session(Environment* env, v8::Local<v8::Object> object);

  void Close(CloseMethod method = CloseMethod::DEFAULT);
  bool Receive(Store&& store,
               const SocketAddress& local_address,
               const SocketAddress& remote_address);

  bool is_destroyed() const;
  bool is_server() const;
  // The session is "wrapped" if it has been passed out to JavaScript
  // via the New Session event or returned by the connect method.
  void set_wrapped();
  SocketAddress remote_address() const;

  SET_NO_MEMORY_INFO()
  SET_MEMORY_INFO_NAME(Session)
  SET_SELF_SIZE(Session)
};

}  // namespace quic
}  // namespace node

#endif  // HAVE_OPENSSL && NODE_OPENSSL_HAS_QUIC
#endif  // defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS
