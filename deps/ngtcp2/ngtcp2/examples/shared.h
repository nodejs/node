/*
 * ngtcp2
 *
 * Copyright (c) 2017 ngtcp2 contributors
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
 * LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
 * OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
 * WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */
#ifndef SHARED_H
#define SHARED_H

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif // defined(HAVE_CONFIG_H)

#include <optional>
#include <string_view>
#include <span>
#include <print>
#include <expected>

#include <ngtcp2/ngtcp2.h>

#include "network.h"

using namespace std::literals;

namespace ngtcp2 {

enum class Error {
  // the generic errors that are not covered by more specific error
  // codes.
  INTERNAL,
  // function arguments are invalid
  INVALID_ARGUMENT,
  // integer overflow error
  INTEGER_OVERFLOW,
  // file I/O error
  IO,
  // function is not implemented yet
  NOT_IMPLEMENTED,
  // the operation is not supported
  UNSUPPORTED,
  // entity is not found (e.g., file not found)
  NOT_FOUND,
  // crypto related error (e.g., error from TLS stack)
  CRYPTO,
  // system call error
  SYSCALL,
  // C library error (e.g., error from getaddrinfo)
  LIBC,
  // HTTP3 library error (e.g., error from nghttp3 API)
  HTTP3,
  // QUIC library error (e.g., error from ngtcp2 API)
  QUIC,
  // sending packet is blocked by kernel
  SEND_BLOCKED,
  // QUIC connection is in close-wait.
  CLOSE_WAIT,
  // QUIC connection should be retried.
  RETRY_CONN,
  // QUIC connection should be dropped.
  DROP_CONN,
  // Retry token is unreadable, and should be ignored.
  UNREADABLE_TOKEN,
};

enum class AppProtocol {
  H3,
  HQ,
};

template <size_t N>
consteval std::span<const uint8_t> span_from_lit(const uint8_t (&s)[N]) {
  return {s, N - 1};
}

inline constexpr uint8_t RAW_HQ_ALPN[] = "\xAhq-interop";
inline constexpr auto HQ_ALPN = span_from_lit(RAW_HQ_ALPN);
inline constexpr auto HQ_ALPN_V1 = span_from_lit(RAW_HQ_ALPN);

inline constexpr uint8_t RAW_H3_ALPN[] = "\x2h3";
inline constexpr auto H3_ALPN = span_from_lit(RAW_H3_ALPN);
inline constexpr auto H3_ALPN_V1 = span_from_lit(RAW_H3_ALPN);

inline constexpr uint32_t TLS_ALERT_ECH_REQUIRED = 121;

inline constexpr size_t MAX_RECV_PKTS = 64;

// msghdr_get_ecn gets ECN bits from |msg|.  |family| is the address
// family from which packet is received.
uint8_t msghdr_get_ecn(msghdr *msg, int family);

// fd_set_recv_ecn sets socket option to |fd| so that it can receive
// ECN bits.
void fd_set_recv_ecn(int fd, int family);

// fd_set_ip_mtu_discover sets IP(V6)_MTU_DISCOVER socket option to
// |fd|.
void fd_set_ip_mtu_discover(int fd, int family);

// fd_set_ip_dontfrag sets IP(V6)_DONTFRAG socket option to |fd|.
void fd_set_ip_dontfrag(int fd, int family);

// fd_set_udp_gro sets UDP_GRO socket option to |fd|.
void fd_set_udp_gro(int fd);

std::expected<Address, Error> msghdr_get_local_addr(msghdr *msg, int family);

// msghdr_get_udp_gro returns UDP_GRO value from |msg|.  If UDP_GRO is
// not found, or UDP_GRO is not supported, this function returns 0.
size_t msghdr_get_udp_gro(msghdr *msg);

// get_local_addr returns the preferred local address (interface
// address) for a given destination address |remote_addr|.
std::expected<InAddr, Error> get_local_addr(const Address &remote_addr);

// addreq returns true if |addr| and |ia| contain the same address.
bool addreq(const Address &addr, const InAddr &ia);

// in_addr_get_ptr returns the pointer to the stored address in |ia|.
// It is undefined if |ia| contains std::monostate.
const void *in_addr_get_ptr(const InAddr &ia);

// in_addr_empty returns true if |ia| if it does not contain any
// meaningful address.
bool in_addr_empty(const InAddr &ia);

// as_sockaddr returns the pointer to the stored address casted to
// const sockaddr *.
[[nodiscard]] const sockaddr *as_sockaddr(const Sockaddr &skaddr);
[[nodiscard]] sockaddr *as_sockaddr(Sockaddr &skaddr);

// sockaddr_family returns the address family.
[[nodiscard]] int sockaddr_family(const Sockaddr &skaddr);

// sockaddr_port returns the port.
[[nodiscard]] uint16_t sockaddr_port(const Sockaddr &skaddr);

// sockaddr_port sets |port| to |skaddr|.
void sockaddr_port(Sockaddr &skaddr, uint16_t port);

// sockaddr_set stores |sa| to |skaddr|.  The address family is
// determined by |sa|->sa_family, and |sa| must point to the memory
// that contains valid object which is either sockaddr_in or
// sockaddr_in6.
void sockaddr_set(Sockaddr &skaddr, const sockaddr *sa);

// sockaddr_size returns the size of the stored address.
[[nodiscard]] socklen_t sockaddr_size(const Sockaddr &skaddr);

// sockaddr_empty returns true if |skaddr| does not contain any
// meaningful address.
[[nodiscard]] bool sockaddr_empty(const Sockaddr &skaddr);

[[nodiscard]] inline ngtcp2_addr as_ngtcp2_addr(const Address &addr) {
  return {
    .addr = const_cast<sockaddr *>(addr.as_sockaddr()),
    .addrlen = addr.size(),
  };
}

[[nodiscard]] inline ngtcp2_addr as_ngtcp2_addr(Address &addr) {
  return {
    .addr = addr.as_sockaddr(),
    .addrlen = addr.size(),
  };
}

} // namespace ngtcp2

template <>
struct std::formatter<ngtcp2::Error> : public std::formatter<std::string_view> {
  auto format(ngtcp2::Error e, format_context &ctx) const {
    auto s = "unknown"sv;

    switch (e) {
    case ngtcp2::Error::INTERNAL:
      s = "internal"sv;
      break;
    case ngtcp2::Error::INVALID_ARGUMENT:
      s = "invalid argument"sv;
      break;
    case ngtcp2::Error::INTEGER_OVERFLOW:
      s = "integer overflow"sv;
      break;
    case ngtcp2::Error::IO:
      s = "I/O"sv;
      break;
    case ngtcp2::Error::NOT_IMPLEMENTED:
      s = "not implemented"sv;
      break;
    case ngtcp2::Error::UNSUPPORTED:
      s = "unsupported"sv;
      break;
    case ngtcp2::Error::NOT_FOUND:
      s = "not found"sv;
      break;
    case ngtcp2::Error::CRYPTO:
      s = "crypto"sv;
      break;
    case ngtcp2::Error::SYSCALL:
      s = "syscall"sv;
      break;
    case ngtcp2::Error::LIBC:
      s = "libc"sv;
      break;
    case ngtcp2::Error::HTTP3:
      s = "HTTP3"sv;
      break;
    case ngtcp2::Error::QUIC:
      s = "QUIC"sv;
      break;
    case ngtcp2::Error::SEND_BLOCKED:
      s = "send blocked"sv;
      break;
    case ngtcp2::Error::CLOSE_WAIT:
      s = "close wait"sv;
      break;
    case ngtcp2::Error::RETRY_CONN:
      s = "retry connection"sv;
      break;
    case ngtcp2::Error::DROP_CONN:
      s = "drop connection"sv;
      break;
    case ngtcp2::Error::UNREADABLE_TOKEN:
      s = "unreadable token"sv;
      break;
    }

    return std::formatter<std::string_view>::format(s, ctx);
  }
};

#endif // !defined(SHARED_H)
