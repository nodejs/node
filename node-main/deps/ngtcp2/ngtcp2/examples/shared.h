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

#include <ngtcp2/ngtcp2.h>

#include "network.h"

using namespace std::literals;

namespace ngtcp2 {

enum class AppProtocol {
  H3,
  HQ,
};

template <size_t N>
consteval std::span<const uint8_t> as_uint8_span(const uint8_t (&s)[N]) {
  return {s, N - 1};
}

constexpr uint8_t RAW_HQ_ALPN[] = "\xahq-interop";
constexpr auto HQ_ALPN = as_uint8_span(RAW_HQ_ALPN);
constexpr auto HQ_ALPN_V1 = as_uint8_span(RAW_HQ_ALPN);

constexpr uint8_t RAW_H3_ALPN[] = "\x2h3";
constexpr auto H3_ALPN = as_uint8_span(RAW_H3_ALPN);
constexpr auto H3_ALPN_V1 = as_uint8_span(RAW_H3_ALPN);

constexpr uint32_t TLS_ALERT_ECH_REQUIRED = 121;

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

std::optional<Address> msghdr_get_local_addr(msghdr *msg, int family);

// msghdr_get_udp_gro returns UDP_GRO value from |msg|.  If UDP_GRO is
// not found, or UDP_GRO is not supported, this function returns 0.
size_t msghdr_get_udp_gro(msghdr *msg);

void set_port(Address &dst, const Address &src);

// get_local_addr stores preferred local address (interface address)
// in |iau| for a given destination address |remote_addr|.
int get_local_addr(in_addr_union &iau, const Address &remote_addr);

// addreq returns true if |sa| and |iau| contain the same address.
bool addreq(const sockaddr *sa, const in_addr_union &iau);

} // namespace ngtcp2

#endif // !defined(SHARED_H)
