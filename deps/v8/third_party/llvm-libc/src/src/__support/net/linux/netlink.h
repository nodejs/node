//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Helpers for retrieving information from the kernel over NETLINK_ROUTE
/// sockets.
///
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_NET_LINUX_NETLINK_H
#define LLVM_LIBC_SRC___SUPPORT_NET_LINUX_NETLINK_H

#include "hdr/errno_macros.h"
#include "hdr/net_if_macros.h"
#include "hdr/sys_socket_macros.h"
#include "hdr/types/ssize_t.h"
#include "src/__support/CPP/functional.h"
#include "src/__support/CPP/scope.h"
#include "src/__support/CPP/span.h"
#include "src/__support/CPP/string_view.h"
#include "src/__support/CPP/unique_ptr.h"
#include "src/__support/checked_ptr.h"
#include "src/__support/common.h"
#include "src/__support/error_or.h"
#include "src/__support/libc_assert.h"
#include "src/string/string_utils.h"

#include <linux/netlink.h>
#include <linux/rtnetlink.h>

namespace LIBC_NAMESPACE_DECL {
namespace net {

/// How many times callers of NetlinkSocket::dump() should try to get a
/// consistent dump (one that was not interrupted) before giving up.
LIBC_INLINE_VAR constexpr size_t MAX_DUMP_ATTEMPTS = 3;

/// The handler which NetlinkSocket::dump() calls for every reply message of the
/// requested type. If the handler returns an error, the dump stops, and dump()
/// returns that error.
///
/// The message lives in the receive buffer, so it is only valid until the
/// handler returns. The same goes for anything pointing into it, such as the
/// attribute payloads passed to the for_each_rtattr callback, or the names
/// returned by parse_ifname. Anything that needs to outlive the handler must be
/// copied.
///
/// The message is not const, as the netlink macros (NLMSG_DATA, RTA_DATA, etc.)
/// cast it to char *, which -Wcast-qual flags for const pointers.
using NetlinkMessageHandler =
    cpp::function_ref<ErrorOr<void>(struct nlmsghdr &)>;

/// The outcome of a successful NetlinkSocket::dump().
enum class DumpResult {
  /// We have received all of the data, and it is consistent.
  Complete,
  /// The kernel reported that the data changed during the dump, so the
  /// messages passed to the handler may be inconsistent. The caller should
  /// discard them and try again.
  Interrupted,
};

/// A NETLINK_ROUTE socket, which we use to get information (e.g. the list of
/// network interfaces) from the kernel via dump requests.
template <typename Policy> class NetlinkSocket {
public:
  /// Size of the initial receive buffer. The kernel limits the size of dump
  /// datagrams to the larger of NLMSG_GOODSIZE (which is at most 8K) and the
  /// largest buffer we have passed to recvfrom, so they always fit.
  /// (RTM_GETLINK dumps requesting extended information via IFLA_EXT_MASK can
  /// be larger, but that requires an attribute in the request, and dump()
  /// never sends any.)
  static constexpr size_t NLMSG_BUFFER_SIZE = 8192;

  /// Size of the buffer we switch to if the dump gets interrupted. A larger
  /// buffer means fewer datagrams, and so less time for the data to change
  /// while we are reading it. The kernel caps the size of dump datagrams at (a
  /// bit less than) 32K, so there is no point in going any higher.
  static constexpr size_t NLMSG_LARGE_BUFFER_SIZE = 32768;

  LIBC_INLINE NetlinkSocket() = default;
  NetlinkSocket(const NetlinkSocket &) = delete;
  NetlinkSocket &operator=(const NetlinkSocket &) = delete;

  /// Closes the socket (if it is open), ignoring any errors.
  LIBC_INLINE ~NetlinkSocket() { close_ignoring_errors(); }

  /// Opens a new NETLINK_ROUTE socket. The socket must not be open already.
  LIBC_INLINE ErrorOr<void> open() {
    LIBC_ASSERT(fd < 0 && "socket is already open");
    ErrorOr<int> res =
        Policy::socket(AF_NETLINK, SOCK_RAW | SOCK_CLOEXEC, NETLINK_ROUTE);
    if (!res.has_value())
      return Error(res.error());
    fd = *res;
    return {};
  }

  /// Closes the socket, and reports any errors. The socket cannot be used
  /// afterwards. Does nothing if the socket is not open (e.g. because it has
  /// been closed by a failed dump()).
  LIBC_INLINE ErrorOr<void> close() {
    if (fd < 0)
      return {};
    // Linux releases the descriptor even if close() fails, so we must not try
    // closing it again.
    int old_fd = fd;
    fd = -1;
    if (ErrorOr<int> res = Policy::close(old_fd); !res.has_value())
      return Error(res.error());
    return {};
  }

  /// Sends a dump request of the given type, using the provided struct (e.g.
  /// struct ifinfomsg) as the payload, and calls the handler for every message
  /// of type `reply_type` in the response. Returns DumpResult::Interrupted if
  /// the kernel reported that the data changed during the dump. In that case,
  /// the following dumps on this socket use a larger receive buffer (if we
  /// manage to allocate it), to reduce the chance of another interruption.
  ///
  /// Returns an error if sending the request or receiving the response fails,
  /// if the kernel or the handler reported an error, or if the response is
  /// malformed. The handler may have been called for some of the messages by
  /// then. Since the rest of the dump may be left unread, and the kernel won't
  /// start another dump on this socket until it is, dump() also closes the
  /// socket in this case. Any further dumps then fail with EBADF, instead of
  /// receiving the leftovers of this one.
  template <typename Header>
  LIBC_INLINE ErrorOr<DumpResult> dump(uint16_t type, const Header &payload,
                                       uint16_t reply_type,
                                       NetlinkMessageHandler handler);

private:
  int fd = -1;
  /// The buffer we switch to after an interrupted dump. Once we have used it,
  /// we have to keep using it, as the kernel may now send datagrams of up to
  /// that size.
  cpp::unique_ptr<uint8_t[]> large_buf;

  /// Sends a dump request of the given type, with the given payload.
  template <typename Header>
  LIBC_INLINE ErrorOr<ssize_t> send_dump_request(uint16_t type,
                                                 const Header &payload);

  /// Parses the netlink messages in the buffer and passes the ones of type
  /// `reply_type` to the handler. Returns true if the buffer contained the
  /// NLMSG_DONE message terminating the dump, or false if more messages should
  /// follow. Returns an error if the kernel or the handler reported one, or if
  /// the data is malformed. Sets `interrupted` if the kernel indicated that the
  /// data changed during the dump.
  LIBC_INLINE static ErrorOr<bool>
  parse_netlink_messages(cpp::span<uint8_t> buf, uint16_t reply_type,
                         NetlinkMessageHandler handler, bool &interrupted);

  /// Closes the socket (if it is open), ignoring any errors.
  LIBC_INLINE void close_ignoring_errors() {
    if (fd >= 0)
      Policy::close(fd);
    fd = -1;
  }
};

template <typename Policy>
template <typename Header>
LIBC_INLINE ErrorOr<DumpResult>
NetlinkSocket<Policy>::dump(uint16_t type, const Header &payload,
                            uint16_t reply_type,
                            NetlinkMessageHandler handler) {
  if (fd < 0)
    return Error(EBADF);
  // If anything below fails, close the socket, so that the next dump doesn't
  // receive the rest of this one.
  cpp::scope_exit close_on_error([this]() { close_ignoring_errors(); });

  ErrorOr<ssize_t> send_res = send_dump_request(type, payload);
  if (!send_res.has_value())
    return Error(send_res.error());

  alignas(struct nlmsghdr) uint8_t stack_buf[NLMSG_BUFFER_SIZE];
  cpp::span<uint8_t> buf(stack_buf);
  if (large_buf)
    buf = {large_buf.get(), NLMSG_LARGE_BUFFER_SIZE};

  bool interrupted = false;
  // The dump can come in multiple datagrams. Keep reading until we get the
  // NLMSG_DONE message. We need to do that even if the dump was interrupted,
  // as the kernel won't start a new dump on this socket until this one is
  // done.
  while (true) {
    ErrorOr<ssize_t> recv_res =
        Policy::recvfrom(fd, buf.data(), buf.size(), 0, nullptr, nullptr);
    if (!recv_res.has_value())
      return Error(recv_res.error());
    // A netlink socket should never return an empty datagram. Bail out
    // instead of spinning if it does.
    if (*recv_res == 0)
      return Error(EIO);

    ErrorOr<bool> parse_res =
        parse_netlink_messages(buf.first(static_cast<size_t>(*recv_res)),
                               reply_type, handler, interrupted);
    if (!parse_res.has_value())
      return Error(parse_res.error());
    if (*parse_res)
      break;
  }
  close_on_error.release();

  if (!interrupted)
    return DumpResult::Complete;

  if (!large_buf) {
    // The caller will probably want to try again. Use a larger buffer for
    // that, to reduce the chance of another interruption. If the allocation
    // fails, we keep using the stack buffer.
    CheckedPtr<uint8_t[]> new_buf =
        make_unique_for_overwrite<uint8_t[]>(NLMSG_LARGE_BUFFER_SIZE);
    if (new_buf)
      large_buf = new_buf.take_unique();
  }
  return DumpResult::Interrupted;
}

template <typename Policy>
template <typename Header>
LIBC_INLINE ErrorOr<ssize_t>
NetlinkSocket<Policy>::send_dump_request(uint16_t type, const Header &payload) {
  struct {
    struct nlmsghdr nlh;
    Header hdr;
  } req = {};
  static_assert(sizeof(req) >= NLMSG_LENGTH(sizeof(Header)));
  req.nlh.nlmsg_len = NLMSG_LENGTH(sizeof(Header));
  req.nlh.nlmsg_type = type;
  req.nlh.nlmsg_flags = NLM_F_REQUEST | NLM_F_DUMP;
  req.hdr = payload;

  return Policy::sendto(fd, &req, req.nlh.nlmsg_len, 0, nullptr, 0);
}

template <typename Policy>
LIBC_INLINE ErrorOr<bool> NetlinkSocket<Policy>::parse_netlink_messages(
    cpp::span<uint8_t> buf, uint16_t reply_type, NetlinkMessageHandler handler,
    bool &interrupted) {
  // NLMSG_NEXT can take the length below zero if the length of the last
  // message is not aligned, so it must be signed. It is wider than int so
  // that NLMSG_OK can compare it with the unsigned nlmsg_len.
  int64_t len = static_cast<int64_t>(buf.size());
  for (auto *nh = reinterpret_cast<struct nlmsghdr *>(buf.data());
       NLMSG_OK(nh, len); nh = NLMSG_NEXT(nh, len)) {
    if (nh->nlmsg_flags & NLM_F_DUMP_INTR)
      interrupted = true;
    if (nh->nlmsg_type == NLMSG_DONE) {
      // The payload contains the result of the dump. This is how the kernel
      // reports errors which happen in the middle of the dump.
      if (nh->nlmsg_len < NLMSG_LENGTH(sizeof(int)))
        return Error(EIO);
      int error = *reinterpret_cast<int *>(NLMSG_DATA(nh));
      if (error < 0)
        return Error(-error);
      return true;
    }
    if (nh->nlmsg_type == NLMSG_ERROR) {
      if (nh->nlmsg_len < NLMSG_LENGTH(sizeof(struct nlmsgerr)))
        return Error(EINVAL);
      auto *err = reinterpret_cast<struct nlmsgerr *>(NLMSG_DATA(nh));
      if (err->error == 0) {
        // Zero means an ACK, which we shouldn't get because we didn't ask for
        // it...
        continue;
      }
      return Error(-err->error);
    }
    if (nh->nlmsg_type != reply_type)
      continue;
    if (ErrorOr<void> res = handler(*nh); !res.has_value())
      return Error(res.error());
  }
  // Anything left over is garbage or a truncated message.
  if (len != 0)
    return Error(EIO);
  return false;
}

/// Returns the fixed-size header (e.g. struct ifinfomsg) at the start of the
/// message payload, or nullptr if the message is too short to contain it.
template <typename Header>
LIBC_INLINE Header *get_nlmsg_header(struct nlmsghdr &nh) {
  if (nh.nlmsg_len < NLMSG_LENGTH(sizeof(Header)))
    return nullptr;
  return static_cast<Header *>(NLMSG_DATA(&nh));
}

/// Calls `fn(type, payload)` for every attribute (struct rtattr) in the `len`
/// bytes starting at `rta`. These should come from the macros for the type of
/// the message, e.g.:
///
///   for_each_rtattr(IFLA_RTA(ifm), IFLA_PAYLOAD(&nh), fn);
///
/// The message must be large enough to contain the fixed-size header (see
/// get_nlmsg_header), as the *_PAYLOAD macros don't check that.
template <typename Fn>
LIBC_INLINE void for_each_rtattr(struct rtattr *rta, size_t len, Fn fn) {
  // The RTA_* macros expect a signed length, as RTA_NEXT can take it below
  // zero if the message length is not aligned.
  int attr_len = static_cast<int>(len);
  for (; RTA_OK(rta, attr_len); rta = RTA_NEXT(rta, attr_len)) {
    fn(rta->rta_type,
       cpp::span<const uint8_t>(static_cast<uint8_t *>(RTA_DATA(rta)),
                                RTA_PAYLOAD(rta)));
  }
}

/// Returns the interface name stored in an IFLA_IFNAME or IFA_LABEL attribute.
/// The name ends at the first null byte (or at the end of the payload), and is
/// truncated to IF_NAMESIZE - 1 characters, so that it fits into IF_NAMESIZE
/// bytes together with a null terminator. Callers rely on this when copying it
/// into fixed-size buffers. The result points into `payload`, and is not
/// null-terminated.
LIBC_INLINE cpp::string_view parse_ifname(cpp::span<const uint8_t> payload) {
  const char *name = reinterpret_cast<const char *>(payload.data());
  size_t len = internal::strnlen(name, payload.size());
  // Defensive check: kernel should not be providing us with names that don't
  // fit.
  if (len >= IF_NAMESIZE)
    len = IF_NAMESIZE - 1;
  return cpp::string_view(name, len);
}

} // namespace net
} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC___SUPPORT_NET_LINUX_NETLINK_H
