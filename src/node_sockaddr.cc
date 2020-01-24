#include "node_sockaddr-inl.h"  // NOLINT(build/include)
#include "uv.h"

namespace node {

namespace {
template <typename T, typename F>
SocketAddress* FromUVHandle(
    F fn,
    const T* handle,
    SocketAddress* addr) {
  if (addr == nullptr)
    addr = new SocketAddress();
  int len = sizeof(sockaddr_storage);
  fn(handle, addr->storage(), &len);
  return addr;
}
}  // namespace

sockaddr_storage* SocketAddress::ToSockAddr(
    int32_t family,
    const char* host,
    uint32_t port,
    sockaddr_storage* addr) {
  switch (family) {
    case AF_INET:
      return uv_ip4_addr(
          host,
          port,
          reinterpret_cast<sockaddr_in*>(addr)) == 0 ?
              addr : nullptr;
    case AF_INET6:
      return uv_ip6_addr(
          host,
          port,
          reinterpret_cast<sockaddr_in6*>(addr)) == 0 ?
              addr : nullptr;
    default:
      UNREACHABLE();
  }
}

bool SocketAddress::Compare::operator()(
    const SocketAddress& laddr,
    const SocketAddress& raddr) const {
  return Compare()(laddr.data(), raddr.data());
}

bool SocketAddress::Compare::operator()(
    const sockaddr* laddr,
    const sockaddr* raddr) const {
  CHECK(laddr->sa_family == AF_INET || laddr->sa_family == AF_INET6);
  return memcmp(laddr, raddr, GetLength(laddr)) == 0;
}

size_t SocketAddress::Hash::operator()(const SocketAddress& addr) const {
  return Hash()(addr.data());
}

size_t SocketAddress::Hash::operator()(const sockaddr* addr) const {
  size_t hash = 0;
  switch (addr->sa_family) {
    case AF_INET: {
      const sockaddr_in* ipv4 =
          reinterpret_cast<const sockaddr_in*>(addr);
      hash_combine(&hash, ipv4->sin_port, ipv4->sin_addr.s_addr);
      break;
    }
    case AF_INET6: {
      const sockaddr_in6* ipv6 =
          reinterpret_cast<const sockaddr_in6*>(addr);
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

template <>
SocketAddress* SocketAddress::FromSockName<uv_tcp_t>(
    const uv_tcp_t* handle,
    SocketAddress* addr) {
  return FromUVHandle<uv_tcp_t>(uv_tcp_getsockname, handle, addr);
}

template <>
SocketAddress* SocketAddress::FromSockName<uv_udp_t>(
    const uv_udp_t* handle,
    SocketAddress* addr) {
  return FromUVHandle<uv_udp_t>(uv_udp_getsockname, handle, addr);
}

template <>
SocketAddress* SocketAddress::FromSockName<uv_tcp_t>(
    const uv_tcp_t& handle,
    SocketAddress* addr) {
  return FromUVHandle<uv_tcp_t>(uv_tcp_getsockname, &handle, addr);
}

template <>
SocketAddress* SocketAddress::FromSockName<uv_udp_t>(
    const uv_udp_t& handle,
    SocketAddress* addr) {
  return FromUVHandle<uv_udp_t>(uv_udp_getsockname, &handle, addr);
}

template <>
SocketAddress* SocketAddress::FromPeerName<uv_tcp_t>(
    const uv_tcp_t* handle,
    SocketAddress* addr) {
  return FromUVHandle<uv_tcp_t>(uv_tcp_getpeername, handle, addr);
}

template <>
SocketAddress* SocketAddress::FromPeerName<uv_tcp_t>(
    const uv_tcp_t& handle,
    SocketAddress* addr) {
  return FromUVHandle<uv_tcp_t>(uv_tcp_getpeername, &handle, addr);
}

template <>
SocketAddress* SocketAddress::FromPeerName<uv_udp_t>(
    const uv_udp_t* handle,
    SocketAddress* addr) {
  return FromUVHandle<uv_udp_t>(uv_udp_getpeername, handle, addr);
}

template <>
SocketAddress* SocketAddress::FromPeerName<uv_udp_t>(
    const uv_udp_t& handle,
    SocketAddress* addr) {
  return FromUVHandle<uv_udp_t>(uv_udp_getpeername, &handle, addr);
}

SocketAddress* SocketAddress::New(
    const char* host,
    uint32_t port,
    int32_t family,
    SocketAddress* addr) {
  if (addr == nullptr)
    addr = new SocketAddress();
  switch (family) {
    case AF_INET:
      return uv_ip4_addr(
          host,
          port,
          reinterpret_cast<sockaddr_in*>(&addr->address_)) == 0 ?
              addr : nullptr;
    case AF_INET6:
      return uv_ip6_addr(
          host,
          port,
          reinterpret_cast<sockaddr_in6*>(&addr->address_)) == 0 ?
              addr : nullptr;
    default:
      UNREACHABLE();
  }
}

}  // namespace node
