#ifndef SRC_NODE_SOCKADDR_INL_H_
#define SRC_NODE_SOCKADDR_INL_H_

#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#include "node.h"
#include "node_internals.h"
#include "node_sockaddr.h"
#include "util-inl.h"

#include <string>

namespace node {

// Fun hash combine trick based on a variadic template that
// I came across a while back but can't remember where. Will add an attribution
// if I can find the source.
inline void hash_combine(size_t* seed) { }

template <typename T, typename... Args>
inline void hash_combine(size_t* seed, const T& value, Args... rest) {
    *seed ^= std::hash<T>{}(value) + 0x9e3779b9 + (*seed << 6) + (*seed >> 2);
    hash_combine(seed, rest...);
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

bool SocketAddress::Compare::operator()(
    const sockaddr* laddr,
    const sockaddr* raddr) const {
  CHECK(laddr->sa_family == AF_INET || laddr->sa_family == AF_INET6);
  return memcmp(laddr, raddr, GetLength(laddr)) == 0;
}

bool SocketAddress::is_numeric_host(const char* hostname) {
  return is_numeric_host(hostname, AF_INET) ||
         is_numeric_host(hostname, AF_INET6);
}

bool SocketAddress::is_numeric_host(const char* hostname, int family) {
  std::array<uint8_t, sizeof(struct in6_addr)> dst;
  return inet_pton(family, hostname, dst.data()) == 1;
}

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

int SocketAddress::GetPort(const sockaddr* addr) {
  return ntohs(addr->sa_family == AF_INET ?
      reinterpret_cast<const sockaddr_in*>(addr)->sin_port :
      reinterpret_cast<const sockaddr_in6*>(addr)->sin6_port);
}

int SocketAddress::GetPort(const sockaddr_storage* addr) {
  return GetPort(reinterpret_cast<const sockaddr*>(addr));
}

std::string SocketAddress::GetAddress(const sockaddr* addr) {
  char host[INET6_ADDRSTRLEN];
  const void* src = addr->sa_family == AF_INET ?
      static_cast<const void*>(
          &(reinterpret_cast<const sockaddr_in*>(addr)->sin_addr)) :
      static_cast<const void*>(
          &(reinterpret_cast<const sockaddr_in6*>(addr)->sin6_addr));
  uv_inet_ntop(addr->sa_family, src, host, INET6_ADDRSTRLEN);
  return std::string(host);
}

std::string SocketAddress::GetAddress(const sockaddr_storage* addr) {
  return GetAddress(reinterpret_cast<const sockaddr*>(addr));
}

size_t SocketAddress::GetLength(const sockaddr* addr) {
  return
      addr->sa_family == AF_INET6 ?
          sizeof(sockaddr_in6) :
          sizeof(sockaddr_in);
}

size_t SocketAddress::GetLength(const sockaddr_storage* addr) {
  return GetLength(reinterpret_cast<const sockaddr*>(addr));
}

SocketAddress::SocketAddress(const sockaddr* addr) {
  memcpy(&address_, addr, GetLength(addr));
}

SocketAddress::SocketAddress(const SocketAddress& addr) {
  memcpy(&address_, &addr.address_, addr.GetLength());
}

SocketAddress& SocketAddress::operator=(const sockaddr* addr) {
  memcpy(&address_, addr, GetLength(addr));
  return *this;
}

SocketAddress& SocketAddress::operator=(const SocketAddress& addr) {
  memcpy(&address_, &addr.address_, addr.GetLength());
  return *this;
}

const sockaddr& SocketAddress::operator*() const {
  return *this->data();
}

const sockaddr* SocketAddress::operator->() const {
  return this->data();
}

const sockaddr* SocketAddress::data() const {
  return reinterpret_cast<const sockaddr*>(&address_);
}

size_t SocketAddress::GetLength() const {
  return GetLength(&address_);
}

int SocketAddress::GetFamily() const {
  return address_.ss_family;
}

std::string SocketAddress::GetAddress() const {
  return GetAddress(&address_);
}

int SocketAddress::GetPort() const {
  return GetPort(&address_);
}

void SocketAddress::Update(uint8_t* data, size_t len) {
  memcpy(&address_, data, len);
}

template <typename T, typename F>
SocketAddress* SocketAddress::FromUVHandle(
    F fn,
    T* handle,
    SocketAddress* addr) {
  if (addr == nullptr)
    addr = new SocketAddress();
  int len = sizeof(sockaddr_storage);
  fn(handle, reinterpret_cast<sockaddr*>(&addr->address_), &len);
  return addr;
}

template <typename T, typename F>
SocketAddress* SocketAddress::FromUVHandle(
    F fn,
    const T* handle,
    SocketAddress* addr) {
  if (addr == nullptr)
    addr = new SocketAddress();
  int len = sizeof(sockaddr_storage);
  fn(handle, reinterpret_cast<sockaddr*>(&addr->address_), &len);
  return addr;
}

SocketAddress* SocketAddress::FromSockName(
    const uv_tcp_t* handle,
    SocketAddress* addr) {
  return FromUVHandle<uv_tcp_t>(uv_tcp_getsockname, handle, addr);
}

SocketAddress* SocketAddress::FromSockName(
    const uv_tcp_t& handle,
    SocketAddress* addr) {
  return FromUVHandle<uv_tcp_t>(uv_tcp_getsockname, &handle, addr);
}

SocketAddress* SocketAddress::FromSockName(
    const uv_udp_t* handle,
    SocketAddress* addr) {
  return FromUVHandle<uv_udp_t>(uv_udp_getsockname, handle, addr);
}

SocketAddress* SocketAddress::FromSockName(
    const uv_udp_t& handle,
    SocketAddress* addr) {
  return FromUVHandle<uv_udp_t>(uv_udp_getsockname, &handle, addr);
}

SocketAddress* SocketAddress::FromPeerName(
    const uv_tcp_t* handle,
    SocketAddress* addr) {
  return FromUVHandle<uv_tcp_t>(uv_tcp_getpeername, handle, addr);
}

SocketAddress* SocketAddress::FromPeerName(
    const uv_tcp_t& handle,
    SocketAddress* addr) {
  return FromUVHandle<uv_tcp_t>(uv_tcp_getpeername, &handle, addr);
}

SocketAddress* SocketAddress::FromPeerName(
    const uv_udp_t* handle,
    SocketAddress* addr) {
  return FromUVHandle<uv_udp_t>(uv_udp_getpeername, handle, addr);
}

SocketAddress* SocketAddress::FromPeerName(
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

v8::Local<v8::Object> SocketAddress::ToJS(
    Environment* env,
    v8::Local<v8::Object> info) const {
  return AddressToJS(env, this->data(), info);
}

}  // namespace node

#endif  // NODE_WANT_INTERNALS
#endif  // SRC_NODE_SOCKADDR_INL_H_
