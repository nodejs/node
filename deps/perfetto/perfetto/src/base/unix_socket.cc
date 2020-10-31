/*
 * Copyright (C) 2017 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "perfetto/ext/base/unix_socket.h"

#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/un.h>
#include <unistd.h>

#include <algorithm>
#include <memory>

#include "perfetto/base/build_config.h"
#include "perfetto/base/logging.h"
#include "perfetto/base/task_runner.h"
#include "perfetto/ext/base/string_utils.h"
#include "perfetto/ext/base/utils.h"

#if PERFETTO_BUILDFLAG(PERFETTO_OS_MACOSX)
#include <sys/ucred.h>
#endif

namespace perfetto {
namespace base {

// The CMSG_* macros use NULL instead of nullptr.
#pragma GCC diagnostic push
#if !PERFETTO_BUILDFLAG(PERFETTO_OS_MACOSX)
#pragma GCC diagnostic ignored "-Wzero-as-null-pointer-constant"
#endif

namespace {

// MSG_NOSIGNAL is not supported on Mac OS X, but in that case the socket is
// created with SO_NOSIGPIPE (See InitializeSocket()).
#if PERFETTO_BUILDFLAG(PERFETTO_OS_MACOSX)
constexpr int kNoSigPipe = 0;
#else
constexpr int kNoSigPipe = MSG_NOSIGNAL;
#endif

// Android takes an int instead of socklen_t for the control buffer size.
#if PERFETTO_BUILDFLAG(PERFETTO_OS_ANDROID)
using CBufLenType = size_t;
#else
using CBufLenType = socklen_t;
#endif

// A wrapper around variable-size sockaddr structs.
// This is solving the following problem: when calling connect() or bind(), the
// caller needs to take care to allocate the right struct (sockaddr_un for
// AF_UNIX, sockaddr_in for AF_INET).   Those structs have different sizes and,
// more importantly, are bigger than the base struct sockaddr.
struct SockaddrAny {
  SockaddrAny() : size() {}
  SockaddrAny(const void* addr, socklen_t sz) : data(new char[sz]), size(sz) {
    memcpy(data.get(), addr, static_cast<size_t>(size));
  }

  const struct sockaddr* addr() const {
    return reinterpret_cast<const struct sockaddr*>(data.get());
  }

  std::unique_ptr<char[]> data;
  socklen_t size;
};

inline int GetSockFamily(SockFamily family) {
  switch (family) {
    case SockFamily::kUnix:
      return AF_UNIX;
    case SockFamily::kInet:
      return AF_INET;
  }
  PERFETTO_CHECK(false);  // For GCC.
}

inline int GetSockType(SockType type) {
  switch (type) {
    case SockType::kStream:
      return SOCK_STREAM;
    case SockType::kDgram:
      return SOCK_DGRAM;
    case SockType::kSeqPacket:
      return SOCK_SEQPACKET;
  }
  PERFETTO_CHECK(false);  // For GCC.
}

SockaddrAny MakeSockAddr(SockFamily family, const std::string& socket_name) {
  switch (family) {
    case SockFamily::kUnix: {
      struct sockaddr_un saddr {};
      const size_t name_len = socket_name.size();
      if (name_len >= sizeof(saddr.sun_path)) {
        errno = ENAMETOOLONG;
        return SockaddrAny();
      }
      memcpy(saddr.sun_path, socket_name.data(), name_len);
      if (saddr.sun_path[0] == '@')
        saddr.sun_path[0] = '\0';
      saddr.sun_family = AF_UNIX;
      auto size = static_cast<socklen_t>(
          __builtin_offsetof(sockaddr_un, sun_path) + name_len + 1);
      PERFETTO_CHECK(static_cast<size_t>(size) <= sizeof(saddr));
      return SockaddrAny(&saddr, size);
    }
    case SockFamily::kInet: {
      auto parts = SplitString(socket_name, ":");
      PERFETTO_CHECK(parts.size() == 2);
      struct addrinfo* addr_info = nullptr;
      struct addrinfo hints {};
      hints.ai_family = AF_INET;
      PERFETTO_CHECK(getaddrinfo(parts[0].c_str(), parts[1].c_str(), &hints,
                                 &addr_info) == 0);
      PERFETTO_CHECK(addr_info->ai_family == AF_INET);
      SockaddrAny res(addr_info->ai_addr, addr_info->ai_addrlen);
      freeaddrinfo(addr_info);
      return res;
    }
  }
  PERFETTO_CHECK(false);  // For GCC.
}

}  // namespace

// +-----------------------+
// | UnixSocketRaw methods |
// +-----------------------+

// static
void UnixSocketRaw::ShiftMsgHdr(size_t n, struct msghdr* msg) {
  using LenType = decltype(msg->msg_iovlen);  // Mac and Linux don't agree.
  for (LenType i = 0; i < msg->msg_iovlen; ++i) {
    struct iovec* vec = &msg->msg_iov[i];
    if (n < vec->iov_len) {
      // We sent a part of this iovec.
      vec->iov_base = reinterpret_cast<char*>(vec->iov_base) + n;
      vec->iov_len -= n;
      msg->msg_iov = vec;
      msg->msg_iovlen -= i;
      return;
    }
    // We sent the whole iovec.
    n -= vec->iov_len;
  }
  // We sent all the iovecs.
  PERFETTO_CHECK(n == 0);
  msg->msg_iovlen = 0;
  msg->msg_iov = nullptr;
}

// static
UnixSocketRaw UnixSocketRaw::CreateMayFail(SockFamily family, SockType type) {
  auto fd = ScopedFile(socket(GetSockFamily(family), GetSockType(type), 0));
  if (!fd) {
    return UnixSocketRaw();
  }
  return UnixSocketRaw(std::move(fd), family, type);
}

// static
std::pair<UnixSocketRaw, UnixSocketRaw> UnixSocketRaw::CreatePair(
    SockFamily family,
    SockType type) {
  int fds[2];
  if (socketpair(GetSockFamily(family), GetSockType(type), 0, fds) != 0)
    return std::make_pair(UnixSocketRaw(), UnixSocketRaw());

  return std::make_pair(UnixSocketRaw(ScopedFile(fds[0]), family, type),
                        UnixSocketRaw(ScopedFile(fds[1]), family, type));
}

UnixSocketRaw::UnixSocketRaw() = default;

UnixSocketRaw::UnixSocketRaw(SockFamily family, SockType type)
    : UnixSocketRaw(
          ScopedFile(socket(GetSockFamily(family), GetSockType(type), 0)),
          family,
          type) {}

UnixSocketRaw::UnixSocketRaw(ScopedFile fd, SockFamily family, SockType type)
    : fd_(std::move(fd)), family_(family), type_(type) {
  PERFETTO_CHECK(fd_);
#if PERFETTO_BUILDFLAG(PERFETTO_OS_MACOSX)
  const int no_sigpipe = 1;
  setsockopt(*fd_, SOL_SOCKET, SO_NOSIGPIPE, &no_sigpipe, sizeof(no_sigpipe));
#endif

  if (family == SockFamily::kInet) {
    int flag = 1;
    PERFETTO_CHECK(
        !setsockopt(*fd_, SOL_SOCKET, SO_REUSEADDR, &flag, sizeof(flag)));
  }

  // There is no reason why a socket should outlive the process in case of
  // exec() by default, this is just working around a broken unix design.
  int fcntl_res = fcntl(*fd_, F_SETFD, FD_CLOEXEC);
  PERFETTO_CHECK(fcntl_res == 0);
}

void UnixSocketRaw::SetBlocking(bool is_blocking) {
  PERFETTO_DCHECK(fd_);
  int flags = fcntl(*fd_, F_GETFL, 0);
  if (!is_blocking) {
    flags |= O_NONBLOCK;
  } else {
    flags &= ~static_cast<int>(O_NONBLOCK);
  }
  bool fcntl_res = fcntl(*fd_, F_SETFL, flags);
  PERFETTO_CHECK(fcntl_res == 0);
}

void UnixSocketRaw::RetainOnExec() {
  PERFETTO_DCHECK(fd_);
  int flags = fcntl(*fd_, F_GETFD, 0);
  flags &= ~static_cast<int>(FD_CLOEXEC);
  bool fcntl_res = fcntl(*fd_, F_SETFD, flags);
  PERFETTO_CHECK(fcntl_res == 0);
}

bool UnixSocketRaw::IsBlocking() const {
  PERFETTO_DCHECK(fd_);
  return (fcntl(*fd_, F_GETFL, 0) & O_NONBLOCK) == 0;
}

bool UnixSocketRaw::Bind(const std::string& socket_name) {
  PERFETTO_DCHECK(fd_);
  SockaddrAny addr = MakeSockAddr(family_, socket_name);
  if (addr.size == 0)
    return false;

  if (bind(*fd_, addr.addr(), addr.size)) {
    PERFETTO_DPLOG("bind(%s)", socket_name.c_str());
    return false;
  }

  return true;
}

bool UnixSocketRaw::Listen() {
  PERFETTO_DCHECK(fd_);
  PERFETTO_DCHECK(type_ == SockType::kStream || type_ == SockType::kSeqPacket);
  return listen(*fd_, SOMAXCONN) == 0;
}

bool UnixSocketRaw::Connect(const std::string& socket_name) {
  PERFETTO_DCHECK(fd_);
  SockaddrAny addr = MakeSockAddr(family_, socket_name);
  if (addr.size == 0)
    return false;

  int res = PERFETTO_EINTR(connect(*fd_, addr.addr(), addr.size));
  if (res && errno != EINPROGRESS)
    return false;

  return true;
}

void UnixSocketRaw::Shutdown() {
  shutdown(*fd_, SHUT_RDWR);
  fd_.reset();
}

// For the interested reader, Linux kernel dive to verify this is not only a
// theoretical possibility: sock_stream_sendmsg, if sock_alloc_send_pskb returns
// NULL [1] (which it does when it gets interrupted [2]), returns early with the
// amount of bytes already sent.
//
// [1]:
// https://elixir.bootlin.com/linux/v4.18.10/source/net/unix/af_unix.c#L1872
// [2]: https://elixir.bootlin.com/linux/v4.18.10/source/net/core/sock.c#L2101
ssize_t UnixSocketRaw::SendMsgAll(struct msghdr* msg) {
  // This does not make sense on non-blocking sockets.
  PERFETTO_DCHECK(fd_);

  ssize_t total_sent = 0;
  while (msg->msg_iov) {
    ssize_t sent = PERFETTO_EINTR(sendmsg(*fd_, msg, kNoSigPipe));
    if (sent <= 0) {
      if (sent == -1 && IsAgain(errno))
        return total_sent;
      return sent;
    }
    total_sent += sent;
    ShiftMsgHdr(static_cast<size_t>(sent), msg);
    // Only send the ancillary data with the first sendmsg call.
    msg->msg_control = nullptr;
    msg->msg_controllen = 0;
  }
  return total_sent;
}

ssize_t UnixSocketRaw::Send(const void* msg,
                            size_t len,
                            const int* send_fds,
                            size_t num_fds) {
  PERFETTO_DCHECK(fd_);
  msghdr msg_hdr = {};
  iovec iov = {const_cast<void*>(msg), len};
  msg_hdr.msg_iov = &iov;
  msg_hdr.msg_iovlen = 1;
  alignas(cmsghdr) char control_buf[256];

  if (num_fds > 0) {
    const auto raw_ctl_data_sz = num_fds * sizeof(int);
    const CBufLenType control_buf_len =
        static_cast<CBufLenType>(CMSG_SPACE(raw_ctl_data_sz));
    PERFETTO_CHECK(control_buf_len <= sizeof(control_buf));
    memset(control_buf, 0, sizeof(control_buf));
    msg_hdr.msg_control = control_buf;
    msg_hdr.msg_controllen = control_buf_len;  // used by CMSG_FIRSTHDR
    struct cmsghdr* cmsg = CMSG_FIRSTHDR(&msg_hdr);
    cmsg->cmsg_level = SOL_SOCKET;
    cmsg->cmsg_type = SCM_RIGHTS;
    cmsg->cmsg_len = static_cast<CBufLenType>(CMSG_LEN(raw_ctl_data_sz));
    memcpy(CMSG_DATA(cmsg), send_fds, num_fds * sizeof(int));
    // note: if we were to send multiple cmsghdr structures, then
    // msg_hdr.msg_controllen would need to be adjusted, see "man 3 cmsg".
  }

  return SendMsgAll(&msg_hdr);
}

ssize_t UnixSocketRaw::Receive(void* msg,
                               size_t len,
                               ScopedFile* fd_vec,
                               size_t max_files) {
  PERFETTO_DCHECK(fd_);
  msghdr msg_hdr = {};
  iovec iov = {msg, len};
  msg_hdr.msg_iov = &iov;
  msg_hdr.msg_iovlen = 1;
  alignas(cmsghdr) char control_buf[256];

  if (max_files > 0) {
    msg_hdr.msg_control = control_buf;
    msg_hdr.msg_controllen =
        static_cast<CBufLenType>(CMSG_SPACE(max_files * sizeof(int)));
    PERFETTO_CHECK(msg_hdr.msg_controllen <= sizeof(control_buf));
  }
  const ssize_t sz = PERFETTO_EINTR(recvmsg(*fd_, &msg_hdr, 0));
  if (sz <= 0) {
    return sz;
  }
  PERFETTO_CHECK(static_cast<size_t>(sz) <= len);

  int* fds = nullptr;
  uint32_t fds_len = 0;

  if (max_files > 0) {
    for (cmsghdr* cmsg = CMSG_FIRSTHDR(&msg_hdr); cmsg;
         cmsg = CMSG_NXTHDR(&msg_hdr, cmsg)) {
      const size_t payload_len = cmsg->cmsg_len - CMSG_LEN(0);
      if (cmsg->cmsg_level == SOL_SOCKET && cmsg->cmsg_type == SCM_RIGHTS) {
        PERFETTO_DCHECK(payload_len % sizeof(int) == 0u);
        PERFETTO_CHECK(fds == nullptr);
        fds = reinterpret_cast<int*>(CMSG_DATA(cmsg));
        fds_len = static_cast<uint32_t>(payload_len / sizeof(int));
      }
    }
  }

  if (msg_hdr.msg_flags & MSG_TRUNC || msg_hdr.msg_flags & MSG_CTRUNC) {
    for (size_t i = 0; fds && i < fds_len; ++i)
      close(fds[i]);
    errno = EMSGSIZE;
    return -1;
  }

  for (size_t i = 0; fds && i < fds_len; ++i) {
    if (i < max_files)
      fd_vec[i].reset(fds[i]);
    else
      close(fds[i]);
  }

  return sz;
}

bool UnixSocketRaw::SetTxTimeout(uint32_t timeout_ms) {
  PERFETTO_DCHECK(fd_);
  struct timeval timeout {};
  uint32_t timeout_sec = timeout_ms / 1000;
  timeout.tv_sec = static_cast<decltype(timeout.tv_sec)>(timeout_sec);
  timeout.tv_usec = static_cast<decltype(timeout.tv_usec)>(
      (timeout_ms - (timeout_sec * 1000)) * 1000);

  return setsockopt(*fd_, SOL_SOCKET, SO_SNDTIMEO,
                    reinterpret_cast<const char*>(&timeout),
                    sizeof(timeout)) == 0;
}

bool UnixSocketRaw::SetRxTimeout(uint32_t timeout_ms) {
  PERFETTO_DCHECK(fd_);
  struct timeval timeout {};
  uint32_t timeout_sec = timeout_ms / 1000;
  timeout.tv_sec = static_cast<decltype(timeout.tv_sec)>(timeout_sec);
  timeout.tv_usec = static_cast<decltype(timeout.tv_usec)>(
      (timeout_ms - (timeout_sec * 1000)) * 1000);

  return setsockopt(*fd_, SOL_SOCKET, SO_RCVTIMEO,
                    reinterpret_cast<const char*>(&timeout),
                    sizeof(timeout)) == 0;
}

#pragma GCC diagnostic pop

// +--------------------+
// | UnixSocket methods |
// +--------------------+

// TODO(primiano): Add ThreadChecker to methods of this class.

// static
std::unique_ptr<UnixSocket> UnixSocket::Listen(const std::string& socket_name,
                                               EventListener* event_listener,
                                               TaskRunner* task_runner,
                                               SockFamily sock_family,
                                               SockType sock_type) {
  auto sock_raw = UnixSocketRaw::CreateMayFail(sock_family, sock_type);
  if (!sock_raw || !sock_raw.Bind(socket_name))
    return nullptr;

  // Forward the call to the Listen() overload below.
  return Listen(sock_raw.ReleaseFd(), event_listener, task_runner, sock_family,
                sock_type);
}

// static
std::unique_ptr<UnixSocket> UnixSocket::Listen(ScopedFile fd,
                                               EventListener* event_listener,
                                               TaskRunner* task_runner,
                                               SockFamily sock_family,
                                               SockType sock_type) {
  return std::unique_ptr<UnixSocket>(
      new UnixSocket(event_listener, task_runner, std::move(fd),
                     State::kListening, sock_family, sock_type));
}

// static
std::unique_ptr<UnixSocket> UnixSocket::Connect(const std::string& socket_name,
                                                EventListener* event_listener,
                                                TaskRunner* task_runner,
                                                SockFamily sock_family,
                                                SockType sock_type) {
  std::unique_ptr<UnixSocket> sock(
      new UnixSocket(event_listener, task_runner, sock_family, sock_type));
  sock->DoConnect(socket_name);
  return sock;
}

// static
std::unique_ptr<UnixSocket> UnixSocket::AdoptConnected(
    ScopedFile fd,
    EventListener* event_listener,
    TaskRunner* task_runner,
    SockFamily sock_family,
    SockType sock_type) {
  return std::unique_ptr<UnixSocket>(
      new UnixSocket(event_listener, task_runner, std::move(fd),
                     State::kConnected, sock_family, sock_type));
}

UnixSocket::UnixSocket(EventListener* event_listener,
                       TaskRunner* task_runner,
                       SockFamily sock_family,
                       SockType sock_type)
    : UnixSocket(event_listener,
                 task_runner,
                 ScopedFile(),
                 State::kDisconnected,
                 sock_family,
                 sock_type) {}

UnixSocket::UnixSocket(EventListener* event_listener,
                       TaskRunner* task_runner,
                       ScopedFile adopt_fd,
                       State adopt_state,
                       SockFamily sock_family,
                       SockType sock_type)
    : event_listener_(event_listener),
      task_runner_(task_runner),
      weak_ptr_factory_(this) {
  state_ = State::kDisconnected;
  if (adopt_state == State::kDisconnected) {
    PERFETTO_DCHECK(!adopt_fd);
    sock_raw_ = UnixSocketRaw::CreateMayFail(sock_family, sock_type);
    if (!sock_raw_) {
      last_error_ = errno;
      return;
    }
  } else if (adopt_state == State::kConnected) {
    PERFETTO_DCHECK(adopt_fd);
    sock_raw_ = UnixSocketRaw(std::move(adopt_fd), sock_family, sock_type);
    state_ = State::kConnected;
    ReadPeerCredentials();
  } else if (adopt_state == State::kListening) {
    // We get here from Listen().

    // |adopt_fd| might genuinely be invalid if the bind() failed.
    if (!adopt_fd) {
      last_error_ = errno;
      return;
    }

    sock_raw_ = UnixSocketRaw(std::move(adopt_fd), sock_family, sock_type);
    if (!sock_raw_.Listen()) {
      last_error_ = errno;
      PERFETTO_DPLOG("listen()");
      return;
    }
    state_ = State::kListening;
  } else {
    PERFETTO_FATAL("Unexpected adopt_state");  // Unfeasible.
  }

  PERFETTO_CHECK(sock_raw_);
  last_error_ = 0;

  sock_raw_.SetBlocking(false);

  WeakPtr<UnixSocket> weak_ptr = weak_ptr_factory_.GetWeakPtr();

  task_runner_->AddFileDescriptorWatch(sock_raw_.fd(), [weak_ptr] {
    if (weak_ptr)
      weak_ptr->OnEvent();
  });
}

UnixSocket::~UnixSocket() {
  // The implicit dtor of |weak_ptr_factory_| will no-op pending callbacks.
  Shutdown(true);
}

UnixSocketRaw UnixSocket::ReleaseSocket() {
  // This will invalidate any pending calls to OnEvent.
  state_ = State::kDisconnected;
  if (sock_raw_)
    task_runner_->RemoveFileDescriptorWatch(sock_raw_.fd());

  return std::move(sock_raw_);
}

// Called only by the Connect() static constructor.
void UnixSocket::DoConnect(const std::string& socket_name) {
  PERFETTO_DCHECK(state_ == State::kDisconnected);

  // This is the only thing that can gracefully fail in the ctor.
  if (!sock_raw_)
    return NotifyConnectionState(false);

  if (!sock_raw_.Connect(socket_name)) {
    last_error_ = errno;
    return NotifyConnectionState(false);
  }

  // At this point either connect() succeeded or started asynchronously
  // (errno = EINPROGRESS).
  last_error_ = 0;
  state_ = State::kConnecting;

  // Even if the socket is non-blocking, connecting to a UNIX socket can be
  // acknowledged straight away rather than returning EINPROGRESS.
  // The decision here is to deal with the two cases uniformly, at the cost of
  // delaying the straight-away-connect() case by one task, to avoid depending
  // on implementation details of UNIX socket on the various OSes.
  // Posting the OnEvent() below emulates a wakeup of the FD watch. OnEvent(),
  // which knows how to deal with spurious wakeups, will poll the SO_ERROR and
  // evolve, if necessary, the state into either kConnected or kDisconnected.
  WeakPtr<UnixSocket> weak_ptr = weak_ptr_factory_.GetWeakPtr();
  task_runner_->PostTask([weak_ptr] {
    if (weak_ptr)
      weak_ptr->OnEvent();
  });
}

void UnixSocket::ReadPeerCredentials() {
  // Peer credentials are supported only on AF_UNIX sockets.
  if (sock_raw_.family() != SockFamily::kUnix)
    return;

#if PERFETTO_BUILDFLAG(PERFETTO_OS_LINUX) || \
    PERFETTO_BUILDFLAG(PERFETTO_OS_ANDROID)
  struct ucred user_cred;
  socklen_t len = sizeof(user_cred);
  int fd = sock_raw_.fd();
  int res = getsockopt(fd, SOL_SOCKET, SO_PEERCRED, &user_cred, &len);
  PERFETTO_CHECK(res == 0);
  peer_uid_ = user_cred.uid;
  peer_pid_ = user_cred.pid;
#else
  struct xucred user_cred;
  socklen_t len = sizeof(user_cred);
  int res = getsockopt(sock_raw_.fd(), 0, LOCAL_PEERCRED, &user_cred, &len);
  PERFETTO_CHECK(res == 0 && user_cred.cr_version == XUCRED_VERSION);
  peer_uid_ = static_cast<uid_t>(user_cred.cr_uid);
// There is no pid in the LOCAL_PEERCREDS for MacOS / FreeBSD.
#endif
}

void UnixSocket::OnEvent() {
  if (state_ == State::kDisconnected)
    return;  // Some spurious event, typically queued just before Shutdown().

  if (state_ == State::kConnected)
    return event_listener_->OnDataAvailable(this);

  if (state_ == State::kConnecting) {
    PERFETTO_DCHECK(sock_raw_);
    int sock_err = EINVAL;
    socklen_t err_len = sizeof(sock_err);
    int res =
        getsockopt(sock_raw_.fd(), SOL_SOCKET, SO_ERROR, &sock_err, &err_len);

    if (res == 0 && sock_err == EINPROGRESS)
      return;  // Not connected yet, just a spurious FD watch wakeup.
    if (res == 0 && sock_err == 0) {
      ReadPeerCredentials();
      state_ = State::kConnected;
      return event_listener_->OnConnect(this, true /* connected */);
    }
    PERFETTO_DLOG("Connection error: %s", strerror(sock_err));
    last_error_ = sock_err;
    Shutdown(false);
    return event_listener_->OnConnect(this, false /* connected */);
  }

  // New incoming connection.
  if (state_ == State::kListening) {
    // There could be more than one incoming connection behind each FD watch
    // notification. Drain'em all.
    for (;;) {
      struct sockaddr_in cli_addr {};
      socklen_t size = sizeof(cli_addr);
      ScopedFile new_fd(PERFETTO_EINTR(accept(
          sock_raw_.fd(), reinterpret_cast<sockaddr*>(&cli_addr), &size)));
      if (!new_fd)
        return;
      std::unique_ptr<UnixSocket> new_sock(new UnixSocket(
          event_listener_, task_runner_, std::move(new_fd), State::kConnected,
          sock_raw_.family(), sock_raw_.type()));
      event_listener_->OnNewIncomingConnection(this, std::move(new_sock));
    }
  }
}

bool UnixSocket::Send(const void* msg,
                      size_t len,
                      const int* send_fds,
                      size_t num_fds) {
  if (state_ != State::kConnected) {
    errno = last_error_ = ENOTCONN;
    return false;
  }

  sock_raw_.SetBlocking(true);
  const ssize_t sz = sock_raw_.Send(msg, len, send_fds, num_fds);
  int saved_errno = errno;
  sock_raw_.SetBlocking(false);

  if (sz == static_cast<ssize_t>(len)) {
    last_error_ = 0;
    return true;
  }

  // If sendmsg() succeeds but the returned size is < |len| it means that the
  // endpoint disconnected in the middle of the read, and we managed to send
  // only a portion of the buffer. In this case we should just give up.

  if (sz < 0 && (saved_errno == EAGAIN || saved_errno == EWOULDBLOCK)) {
    // A genuine out-of-buffer. The client should retry or give up.
    // Man pages specify that EAGAIN and EWOULDBLOCK have the same semantic here
    // and clients should check for both.
    last_error_ = EAGAIN;
    return false;
  }

  // Either the other endpoint disconnected (ECONNRESET) or some other error
  // happened.
  last_error_ = saved_errno;
  PERFETTO_DPLOG("sendmsg() failed");
  Shutdown(true);
  return false;
}

void UnixSocket::Shutdown(bool notify) {
  WeakPtr<UnixSocket> weak_ptr = weak_ptr_factory_.GetWeakPtr();
  if (notify) {
    if (state_ == State::kConnected) {
      task_runner_->PostTask([weak_ptr] {
        if (weak_ptr)
          weak_ptr->event_listener_->OnDisconnect(weak_ptr.get());
      });
    } else if (state_ == State::kConnecting) {
      task_runner_->PostTask([weak_ptr] {
        if (weak_ptr)
          weak_ptr->event_listener_->OnConnect(weak_ptr.get(), false);
      });
    }
  }

  if (sock_raw_) {
    task_runner_->RemoveFileDescriptorWatch(sock_raw_.fd());
    sock_raw_.Shutdown();
  }
  state_ = State::kDisconnected;
}

size_t UnixSocket::Receive(void* msg,
                           size_t len,
                           ScopedFile* fd_vec,
                           size_t max_files) {
  if (state_ != State::kConnected) {
    last_error_ = ENOTCONN;
    return 0;
  }

  const ssize_t sz = sock_raw_.Receive(msg, len, fd_vec, max_files);
  if (sz < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
    last_error_ = EAGAIN;
    return 0;
  }
  if (sz <= 0) {
    last_error_ = errno;
    Shutdown(true);
    return 0;
  }
  PERFETTO_CHECK(static_cast<size_t>(sz) <= len);
  return static_cast<size_t>(sz);
}

std::string UnixSocket::ReceiveString(size_t max_length) {
  std::unique_ptr<char[]> buf(new char[max_length + 1]);
  size_t rsize = Receive(buf.get(), max_length);
  PERFETTO_CHECK(static_cast<size_t>(rsize) <= max_length);
  buf[static_cast<size_t>(rsize)] = '\0';
  return std::string(buf.get());
}

void UnixSocket::NotifyConnectionState(bool success) {
  if (!success)
    Shutdown(false);

  WeakPtr<UnixSocket> weak_ptr = weak_ptr_factory_.GetWeakPtr();
  task_runner_->PostTask([weak_ptr, success] {
    if (weak_ptr)
      weak_ptr->event_listener_->OnConnect(weak_ptr.get(), success);
  });
}

UnixSocket::EventListener::~EventListener() {}
void UnixSocket::EventListener::OnNewIncomingConnection(
    UnixSocket*,
    std::unique_ptr<UnixSocket>) {}
void UnixSocket::EventListener::OnConnect(UnixSocket*, bool) {}
void UnixSocket::EventListener::OnDisconnect(UnixSocket*) {}
void UnixSocket::EventListener::OnDataAvailable(UnixSocket*) {}

}  // namespace base
}  // namespace perfetto
