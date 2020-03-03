#include "node_sockaddr-inl.h"  // NOLINT(build/include)
#include "uv.h"

namespace node {

namespace {
template <typename T, typename F>
SocketAddress FromUVHandle(F fn, const T& handle) {
  SocketAddress addr;
  int len = sizeof(sockaddr_storage);
  if (fn(&handle, addr.storage(), &len) == 0)
    CHECK_EQ(static_cast<size_t>(len), addr.length());
  else
    addr.storage()->sa_family = 0;
  return addr;
}
}  // namespace

bool SocketAddress::ToSockAddr(
    int32_t family,
    const char* host,
    uint32_t port,
    sockaddr_storage* addr) {
  switch (family) {
    case AF_INET:
      return uv_ip4_addr(
          host,
          port,
          reinterpret_cast<sockaddr_in*>(addr)) == 0;
    case AF_INET6:
      return uv_ip6_addr(
          host,
          port,
          reinterpret_cast<sockaddr_in6*>(addr)) == 0;
    default:
      UNREACHABLE();
  }
}

bool SocketAddress::New(
    const char* host,
    uint32_t port,
    SocketAddress* addr) {
  return New(AF_INET, host, port, addr) || New(AF_INET6, host, port, addr);
}

bool SocketAddress::New(
    int32_t family,
    const char* host,
    uint32_t port,
    SocketAddress* addr) {
  return ToSockAddr(family, host, port,
                    reinterpret_cast<sockaddr_storage*>(addr->storage()));
}

size_t SocketAddress::Hash::operator()(const SocketAddress& addr) const {
  size_t hash = 0;
  switch (addr.family()) {
    case AF_INET: {
      const sockaddr_in* ipv4 =
          reinterpret_cast<const sockaddr_in*>(addr.raw());
      hash_combine(&hash, ipv4->sin_port, ipv4->sin_addr.s_addr);
      break;
    }
    case AF_INET6: {
      const sockaddr_in6* ipv6 =
          reinterpret_cast<const sockaddr_in6*>(addr.raw());
      const uint64_t* a =
          reinterpret_cast<const uint64_t*>(&ipv6->sin6_addr);
      hash_combine(&hash, ipv6->sin6_port, a[0], a[1]);
      break;
    }
    default:
      UNREACHABLE();
  }
  return hash;
}

SocketAddress SocketAddress::FromSockName(const uv_tcp_t& handle) {
  return FromUVHandle(uv_tcp_getsockname, handle);
}

SocketAddress SocketAddress::FromSockName(const uv_udp_t& handle) {
  return FromUVHandle(uv_udp_getsockname, handle);
}

SocketAddress SocketAddress::FromPeerName(const uv_tcp_t& handle) {
  return FromUVHandle(uv_tcp_getpeername, handle);
}

SocketAddress SocketAddress::FromPeerName(const uv_udp_t& handle) {
  return FromUVHandle(uv_udp_getpeername, handle);
}

}  // namespace node
