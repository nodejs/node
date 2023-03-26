#include "data.h"
#include <node_sockaddr-inl.h>
#include "ngtcp2/ngtcp2.h"

namespace node {
namespace quic {

Path::Path(const SocketAddress& local, const SocketAddress& remote) {
  ngtcp2_addr_init(&this->local, local.data(), local.length());
  ngtcp2_addr_init(&this->remote, remote.data(), remote.length());
}

PathStorage::PathStorage() { ngtcp2_path_storage_zero(this); }
PathStorage::operator ngtcp2_path() { return path; }

}  // namespace quic
}  // namespace node
