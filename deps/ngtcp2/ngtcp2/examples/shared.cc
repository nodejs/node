/*
 * ngtcp2
 *
 * Copyright (c) 2019 ngtcp2 contributors
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
#include "shared.h"

#include <nghttp3/nghttp3.h>

#include <cstring>
#include <cassert>
#include <iostream>

#include <unistd.h>
#ifdef HAVE_NETINET_IN_H
#  include <netinet/in.h>
#endif // defined(HAVE_NETINET_IN_H)
#ifdef HAVE_NETINET_UDP_H
#  include <netinet/udp.h>
#endif // defined(HAVE_NETINET_UDP_H)
#ifdef HAVE_NETINET_IP_H
#  include <netinet/ip.h>
#endif // defined(HAVE_NETINET_IP_H)
#ifdef HAVE_ASM_TYPES_H
#  include <asm/types.h>
#endif // defined(HAVE_ASM_TYPES_H)
#ifdef HAVE_LINUX_NETLINK_H
#  include <linux/netlink.h>
#endif // defined(HAVE_LINUX_NETLINK_H)
#ifdef HAVE_LINUX_RTNETLINK_H
#  include <linux/rtnetlink.h>
#endif // defined(HAVE_LINUX_RTNETLINK_H)

#include "template.h"

namespace ngtcp2 {

uint8_t msghdr_get_ecn(msghdr *msg, int family) {
  switch (family) {
  case AF_INET:
    for (auto cmsg = CMSG_FIRSTHDR(msg); cmsg; cmsg = CMSG_NXTHDR(msg, cmsg)) {
      if (cmsg->cmsg_level == IPPROTO_IP &&
#ifdef __APPLE__
          cmsg->cmsg_type == IP_RECVTOS
#else  // !defined(__APPLE__)
          cmsg->cmsg_type == IP_TOS
#endif // !defined(__APPLE__)
          && cmsg->cmsg_len) {
        return *reinterpret_cast<uint8_t *>(CMSG_DATA(cmsg)) & IPTOS_ECN_MASK;
      }
    }
    break;
  case AF_INET6:
    for (auto cmsg = CMSG_FIRSTHDR(msg); cmsg; cmsg = CMSG_NXTHDR(msg, cmsg)) {
      if (cmsg->cmsg_level == IPPROTO_IPV6 && cmsg->cmsg_type == IPV6_TCLASS &&
          cmsg->cmsg_len) {
        unsigned int tos;

        memcpy(&tos, CMSG_DATA(cmsg), sizeof(int));

        return tos & IPTOS_ECN_MASK;
      }
    }
    break;
  }

  return 0;
}

void fd_set_recv_ecn(int fd, int family) {
  unsigned int tos = 1;
  switch (family) {
  case AF_INET:
    if (setsockopt(fd, IPPROTO_IP, IP_RECVTOS, &tos,
                   static_cast<socklen_t>(sizeof(tos))) == -1) {
      std::cerr << "setsockopt: " << strerror(errno) << std::endl;
    }
    break;
  case AF_INET6:
    if (setsockopt(fd, IPPROTO_IPV6, IPV6_RECVTCLASS, &tos,
                   static_cast<socklen_t>(sizeof(tos))) == -1) {
      std::cerr << "setsockopt: " << strerror(errno) << std::endl;
    }
    break;
  }
}

void fd_set_ip_mtu_discover(int fd, int family) {
#if defined(IP_MTU_DISCOVER) && defined(IPV6_MTU_DISCOVER)
  int val;

  switch (family) {
  case AF_INET:
    val = IP_PMTUDISC_PROBE;
    if (setsockopt(fd, IPPROTO_IP, IP_MTU_DISCOVER, &val,
                   static_cast<socklen_t>(sizeof(val))) == -1) {
      std::cerr << "setsockopt: IP_MTU_DISCOVER: " << strerror(errno)
                << std::endl;
    }
    break;
  case AF_INET6:
    val = IPV6_PMTUDISC_PROBE;
    if (setsockopt(fd, IPPROTO_IPV6, IPV6_MTU_DISCOVER, &val,
                   static_cast<socklen_t>(sizeof(val))) == -1) {
      std::cerr << "setsockopt: IPV6_MTU_DISCOVER: " << strerror(errno)
                << std::endl;
    }
    break;
  }
#endif // defined(IP_MTU_DISCOVER) && defined(IPV6_MTU_DISCOVER)
}

void fd_set_ip_dontfrag(int fd, int family) {
#if defined(IP_DONTFRAG) && defined(IPV6_DONTFRAG)
  int val = 1;

  switch (family) {
  case AF_INET:
    if (setsockopt(fd, IPPROTO_IP, IP_DONTFRAG, &val,
                   static_cast<socklen_t>(sizeof(val))) == -1) {
      std::cerr << "setsockopt: IP_DONTFRAG: " << strerror(errno) << std::endl;
    }
    break;
  case AF_INET6:
    if (setsockopt(fd, IPPROTO_IPV6, IPV6_DONTFRAG, &val,
                   static_cast<socklen_t>(sizeof(val))) == -1) {
      std::cerr << "setsockopt: IPV6_DONTFRAG: " << strerror(errno)
                << std::endl;
    }
    break;
  }
#endif // defined(IP_DONTFRAG) && defined(IPV6_DONTFRAG)
}

void fd_set_udp_gro(int fd) {
#ifdef UDP_GRO
  int val = 1;

  if (setsockopt(fd, IPPROTO_UDP, UDP_GRO, &val,
                 static_cast<socklen_t>(sizeof(val))) == -1) {
    std::cerr << "setsockopt: UDP_GRO: " << strerror(errno) << std::endl;
  }
#endif // defined(UDP_GRO)
}

std::optional<Address> msghdr_get_local_addr(msghdr *msg, int family) {
  switch (family) {
  case AF_INET:
    for (auto cmsg = CMSG_FIRSTHDR(msg); cmsg; cmsg = CMSG_NXTHDR(msg, cmsg)) {
      if (cmsg->cmsg_level == IPPROTO_IP && cmsg->cmsg_type == IP_PKTINFO) {
        in_pktinfo pktinfo;
        memcpy(&pktinfo, CMSG_DATA(cmsg), sizeof(pktinfo));
        Address res{
          .ifindex = static_cast<uint32_t>(pktinfo.ipi_ifindex),
        };
        auto &sa = res.skaddr.emplace<sockaddr_in>();
        sa.sin_family = AF_INET;
        sa.sin_addr = pktinfo.ipi_addr;
        return res;
      }
    }
    return {};
  case AF_INET6:
    for (auto cmsg = CMSG_FIRSTHDR(msg); cmsg; cmsg = CMSG_NXTHDR(msg, cmsg)) {
      if (cmsg->cmsg_level == IPPROTO_IPV6 && cmsg->cmsg_type == IPV6_PKTINFO) {
        in6_pktinfo pktinfo;
        memcpy(&pktinfo, CMSG_DATA(cmsg), sizeof(pktinfo));
        Address res{
          .ifindex = static_cast<uint32_t>(pktinfo.ipi6_ifindex),
        };
        auto &sa = res.skaddr.emplace<sockaddr_in6>();
        sa.sin6_family = AF_INET6;
        sa.sin6_addr = pktinfo.ipi6_addr;
        return res;
      }
    }
    return {};
  }
  return {};
}

size_t msghdr_get_udp_gro(msghdr *msg) {
  int gso_size = 0;

#ifdef UDP_GRO
  for (auto cmsg = CMSG_FIRSTHDR(msg); cmsg; cmsg = CMSG_NXTHDR(msg, cmsg)) {
    if (cmsg->cmsg_level == SOL_UDP && cmsg->cmsg_type == UDP_GRO) {
      memcpy(&gso_size, CMSG_DATA(cmsg), sizeof(gso_size));

      break;
    }
  }
#endif // defined(UDP_GRO)

  return static_cast<size_t>(gso_size);
}

#ifdef HAVE_LINUX_RTNETLINK_H

struct nlmsg {
  nlmsghdr hdr;
  rtmsg msg;
  rtattr dst;
  uint8_t dst_addr[sizeof(sockaddr_storage)];
};

namespace {
int send_netlink_msg(int fd, const Address &remote_addr, uint32_t seq) {
  nlmsg nlmsg{
    .hdr{
      .nlmsg_type = RTM_GETROUTE,
      .nlmsg_flags = NLM_F_REQUEST | NLM_F_ACK,
      .nlmsg_seq = seq,
    },
    .msg{
      .rtm_family = static_cast<unsigned char>(remote_addr.family()),
      .rtm_protocol = RTPROT_KERNEL,
    },
    .dst{
      .rta_type = RTA_DST,
    },
  };

  std::visit(
    [&nlmsg](auto &&arg) {
      using T = std::decay_t<decltype(arg)>;

      if constexpr (std::is_same_v<T, sockaddr_in>) {
        nlmsg.dst.rta_len = RTA_LENGTH(sizeof(arg.sin_addr));
        memcpy(RTA_DATA(&nlmsg.dst), &arg.sin_addr, sizeof(arg.sin_addr));
        return;
      }

      if constexpr (std::is_same_v<T, sockaddr_in6>) {
        nlmsg.dst.rta_len = RTA_LENGTH(sizeof(arg.sin6_addr));
        memcpy(RTA_DATA(&nlmsg.dst), &arg.sin6_addr, sizeof(arg.sin6_addr));
        return;
      }

      assert(0);
      abort();
    },
    remote_addr.skaddr);

  nlmsg.hdr.nlmsg_len = NLMSG_LENGTH(sizeof(nlmsg.msg) + nlmsg.dst.rta_len);

  sockaddr_nl sa{
    .nl_family = AF_NETLINK,
  };

  iovec iov{
    .iov_base = &nlmsg,
    .iov_len = nlmsg.hdr.nlmsg_len,
  };
  msghdr msg{
    .msg_name = &sa,
    .msg_namelen = sizeof(sa),
    .msg_iov = &iov,
    .msg_iovlen = 1,
  };

  ssize_t nwrite;

  do {
    nwrite = sendmsg(fd, &msg, 0);
  } while (nwrite == -1 && errno == EINTR);

  if (nwrite == -1) {
    std::cerr << "sendmsg: Could not write netlink message: " << strerror(errno)
              << std::endl;
    return -1;
  }

  return 0;
}
} // namespace

namespace {
int recv_netlink_msg(InAddr &ia, int fd, uint32_t seq) {
  std::array<uint8_t, 8192> buf;
  iovec iov = {
    .iov_base = buf.data(),
    .iov_len = buf.size(),
  };
  sockaddr_nl sa{};
  msghdr msg{
    .msg_name = &sa,
    .msg_namelen = sizeof(sa),
    .msg_iov = &iov,
    .msg_iovlen = 1,
  };
  ssize_t nread;

  do {
    nread = recvmsg(fd, &msg, 0);
  } while (nread == -1 && errno == EINTR);

  if (nread == -1) {
    std::cerr << "recvmsg: Could not receive netlink message: "
              << strerror(errno) << std::endl;
    return -1;
  }

  for (auto hdr = reinterpret_cast<nlmsghdr *>(buf.data());
       NLMSG_OK(hdr, nread); hdr = NLMSG_NEXT(hdr, nread)) {
    if (seq != hdr->nlmsg_seq) {
      std::cerr << "netlink: unexpected sequence number " << hdr->nlmsg_seq
                << " while expecting " << seq << std::endl;
      return -1;
    }

    if (hdr->nlmsg_flags & NLM_F_MULTI) {
      std::cerr << "netlink: unexpected NLM_F_MULTI flag set" << std::endl;
      return -1;
    }

    switch (hdr->nlmsg_type) {
    case NLMSG_DONE:
      std::cerr << "netlink: unexpected NLMSG_DONE" << std::endl;
      return -1;
    case NLMSG_NOOP:
      continue;
    case NLMSG_ERROR:
      std::cerr << "netlink: "
                << strerror(-static_cast<nlmsgerr *>(NLMSG_DATA(hdr))->error)
                << std::endl;
      return -1;
    }

    auto attrlen = hdr->nlmsg_len - NLMSG_SPACE(sizeof(rtmsg));

    for (auto rta = reinterpret_cast<rtattr *>(
           static_cast<uint8_t *>(NLMSG_DATA(hdr)) + sizeof(rtmsg));
         RTA_OK(rta, attrlen); rta = RTA_NEXT(rta, attrlen)) {
      if (rta->rta_type != RTA_PREFSRC) {
        continue;
      }

      switch (static_cast<rtmsg *>(NLMSG_DATA(hdr))->rtm_family) {
      case AF_INET: {
        constexpr auto in_addrlen = sizeof(in_addr);
        if (RTA_LENGTH(in_addrlen) != rta->rta_len) {
          return -1;
        }

        in_addr addr;
        memcpy(&addr, RTA_DATA(rta), in_addrlen);

        ia.emplace<in_addr>(addr);

        break;
      }
      case AF_INET6: {
        constexpr auto in_addrlen = sizeof(in6_addr);
        if (RTA_LENGTH(in_addrlen) != rta->rta_len) {
          return -1;
        }

        in6_addr addr;
        memcpy(&addr, RTA_DATA(rta), in_addrlen);

        ia.emplace<in6_addr>(addr);

        break;
      }
      default:
        assert(0);
        abort();
      }

      break;
    }
  }

  if (in_addr_empty(ia)) {
    return -1;
  }

  // Read ACK
  sa = {};
  msg = {};

  msg.msg_name = &sa;
  msg.msg_namelen = sizeof(sa);
  msg.msg_iov = &iov;
  msg.msg_iovlen = 1;

  int error = -1;

  do {
    nread = recvmsg(fd, &msg, 0);
  } while (nread == -1 && errno == EINTR);

  if (nread == -1) {
    std::cerr << "recvmsg: Could not receive netlink message: "
              << strerror(errno) << std::endl;
    return -1;
  }

  error = -1;

  for (auto hdr = reinterpret_cast<nlmsghdr *>(buf.data());
       NLMSG_OK(hdr, nread); hdr = NLMSG_NEXT(hdr, nread)) {
    if (seq != hdr->nlmsg_seq) {
      std::cerr << "netlink: unexpected sequence number " << hdr->nlmsg_seq
                << " while expecting " << seq << std::endl;
      return -1;
    }

    if (hdr->nlmsg_flags & NLM_F_MULTI) {
      std::cerr << "netlink: unexpected NLM_F_MULTI flag set" << std::endl;
      return -1;
    }

    switch (hdr->nlmsg_type) {
    case NLMSG_DONE:
      std::cerr << "netlink: unexpected NLMSG_DONE" << std::endl;
      return -1;
    case NLMSG_NOOP:
      continue;
    case NLMSG_ERROR:
      error = -static_cast<nlmsgerr *>(NLMSG_DATA(hdr))->error;
      if (error == 0) {
        break;
      }

      std::cerr << "netlink: " << strerror(error) << std::endl;

      return -1;
    }
  }

  if (error != 0) {
    return -1;
  }

  return 0;
}
} // namespace

int get_local_addr(InAddr &ia, const Address &remote_addr) {
  sockaddr_nl sa{
    .nl_family = AF_NETLINK,
  };

  auto fd = socket(AF_NETLINK, SOCK_RAW, NETLINK_ROUTE);
  if (fd == -1) {
    std::cerr << "socket: Could not create netlink socket: " << strerror(errno)
              << std::endl;
    return -1;
  }

  auto fd_d = defer([fd] { close(fd); });

  if (bind(fd, reinterpret_cast<sockaddr *>(&sa), sizeof(sa)) == -1) {
    std::cerr << "bind: Could not bind netlink socket: " << strerror(errno)
              << std::endl;
    return -1;
  }

  uint32_t seq = 1;

  if (send_netlink_msg(fd, remote_addr, seq) != 0) {
    return -1;
  }

  return recv_netlink_msg(ia, fd, seq);
}

#endif // defined(HAVE_LINUX_NETLINK_H)

bool addreq(const Address &addr, const InAddr &ia) {
  return std::visit(
    [&ia](auto &&arg) -> bool {
      using T = std::decay_t<decltype(arg)>;

      if constexpr (std::is_same_v<T, sockaddr_in>) {
        auto rhs = std::get_if<in_addr>(&ia);

        return rhs && memcmp(&arg.sin_addr, rhs, sizeof(*rhs)) == 0;
      }

      if constexpr (std::is_same_v<T, sockaddr_in6>) {
        auto rhs = std::get_if<in6_addr>(&ia);

        return rhs && memcmp(&arg.sin6_addr, rhs, sizeof(*rhs)) == 0;
      }

      assert(0);
      abort();
    },
    addr.skaddr);
}

const void *in_addr_get_ptr(const InAddr &ia) {
  return std::visit(
    [](auto &&arg) {
      if constexpr (std::is_same_v<std::decay_t<decltype(arg)>,
                                   std::monostate>) {
        assert(0);
        abort();
      }

      return reinterpret_cast<const void *>(&arg);
    },
    ia);
}

bool in_addr_empty(const InAddr &ia) {
  return std::holds_alternative<std::monostate>(ia);
}

const sockaddr *as_sockaddr(const Sockaddr &skaddr) {
  return std::visit(
    [](auto &&arg) {
      if constexpr (std::is_same_v<std::decay_t<decltype(arg)>,
                                   std::monostate>) {
        assert(0);
        abort();
      }

      return reinterpret_cast<const sockaddr *>(&arg);
    },
    skaddr);
}

sockaddr *as_sockaddr(Sockaddr &skaddr) {
  return std::visit(
    [](auto &&arg) {
      if constexpr (std::is_same_v<std::decay_t<decltype(arg)>,
                                   std::monostate>) {
        assert(0);
        abort();
      }

      return reinterpret_cast<sockaddr *>(&arg);
    },
    skaddr);
}

int sockaddr_family(const Sockaddr &skaddr) {
  return as_sockaddr(skaddr)->sa_family;
}

uint16_t sockaddr_port(const Sockaddr &skaddr) {
  return std::visit(
    [](auto &&arg) -> uint16_t {
      using T = std::decay_t<decltype(arg)>;

      if constexpr (std::is_same_v<T, sockaddr_in>) {
        return ntohs(arg.sin_port);
      }

      if constexpr (std::is_same_v<T, sockaddr_in6>) {
        return ntohs(arg.sin6_port);
      }

      assert(0);
      abort();
    },
    skaddr);
}

void sockaddr_port(Sockaddr &skaddr, uint16_t port) {
  std::visit(
    [port](auto &&arg) {
      using T = std::decay_t<decltype(arg)>;

      if constexpr (std::is_same_v<T, sockaddr_in>) {
        arg.sin_port = htons(port);
        return;
      }

      if constexpr (std::is_same_v<T, sockaddr_in6>) {
        arg.sin6_port = htons(port);
        return;
      }

      assert(0);
      abort();
    },
    skaddr);
}

void sockaddr_set(Sockaddr &skaddr, const sockaddr *sa) {
  switch (sa->sa_family) {
  case AF_INET:
    skaddr.emplace<sockaddr_in>(*reinterpret_cast<const sockaddr_in *>(sa));
    return;
  case AF_INET6:
    skaddr.emplace<sockaddr_in6>(*reinterpret_cast<const sockaddr_in6 *>(sa));
    return;
  default:
    assert(0);
    abort();
  }
}

socklen_t sockaddr_size(const Sockaddr &skaddr) {
  return std::visit(
    [](auto &&arg) { return static_cast<socklen_t>(sizeof(arg)); }, skaddr);
}

bool sockaddr_empty(const Sockaddr &skaddr) {
  return std::holds_alternative<std::monostate>(skaddr);
}

const sockaddr *Address::as_sockaddr() const {
  return ngtcp2::as_sockaddr(skaddr);
}

sockaddr *Address::as_sockaddr() { return ngtcp2::as_sockaddr(skaddr); }

int Address::family() const { return sockaddr_family(skaddr); }

uint16_t Address::port() const { return sockaddr_port(skaddr); }

void Address::port(uint16_t port) { sockaddr_port(skaddr, port); }

void Address::set(const sockaddr *sa) { sockaddr_set(skaddr, sa); }

socklen_t Address::size() const { return sockaddr_size(skaddr); }

bool Address::empty() const { return sockaddr_empty(skaddr); }

} // namespace ngtcp2
