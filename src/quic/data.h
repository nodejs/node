#pragma once

#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#include <node_sockaddr.h>
#include <ngtcp2/ngtcp2.h>

namespace node {
namespace quic {

struct Path final : public ngtcp2_path {
  Path(const SocketAddress& local, const SocketAddress& remote);
};

struct PathStorage final: public ngtcp2_path_storage {
  PathStorage();
  operator ngtcp2_path();
};

}  // namespace quic
}  // namespace node

#endif  // defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS
