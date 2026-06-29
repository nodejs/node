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
          .len = sizeof(res.su.in),
          .ifindex = static_cast<uint32_t>(pktinfo.ipi_ifindex),
        };
        auto &sa = res.su.in;
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
          .len = sizeof(res.su.in6),
          .ifindex = static_cast<uint32_t>(pktinfo.ipi6_ifindex),
        };
        auto &sa = res.su.in6;
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

void set_port(Address &dst, const Address &src) {
  switch (dst.su.storage.ss_family) {
  case AF_INET:
    assert(AF_INET == src.su.storage.ss_family);
    dst.su.in.sin_port = src.su.in.sin_port;
    return;
  case AF_INET6:
    assert(AF_INET6 == src.su.storage.ss_family);
    dst.su.in6.sin6_port = src.su.in6.sin6_port;
    return;
  default:
    assert(0);
  }
}

#ifdef HAVE_LINUX_RTNETLINK_H

struct nlmsg {
  nlmsghdr hdr;
  rtmsg msg;
  rtattr dst;
  in_addr_union dst_addr;
};

namespace {
int send_netlink_msg(int fd, const Address &remote_addr, uint32_t seq) {
  nlmsg nlmsg{
    .hdr =
      {
        .nlmsg_type = RTM_GETROUTE,
        .nlmsg_flags = NLM_F_REQUEST | NLM_F_ACK,
        .nlmsg_seq = seq,
      },
    .msg =
      {
        .rtm_family = static_cast<unsigned char>(remote_addr.su.sa.sa_family),
        .rtm_protocol = RTPROT_KERNEL,
      },
    .dst =
      {
        .rta_type = RTA_DST,
      },
  };

  switch (remote_addr.su.sa.sa_family) {
  case AF_INET:
    nlmsg.dst.rta_len = RTA_LENGTH(sizeof(remote_addr.su.in.sin_addr));
    memcpy(RTA_DATA(&nlmsg.dst), &remote_addr.su.in.sin_addr,
           sizeof(remote_addr.su.in.sin_addr));
    break;
  case AF_INET6:
    nlmsg.dst.rta_len = RTA_LENGTH(sizeof(remote_addr.su.in6.sin6_addr));
    memcpy(RTA_DATA(&nlmsg.dst), &remote_addr.su.in6.sin6_addr,
           sizeof(remote_addr.su.in6.sin6_addr));
    break;
  default:
    assert(0);
  }

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
int recv_netlink_msg(in_addr_union &iau, int fd, uint32_t seq) {
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

  size_t in_addrlen = 0;

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
      case AF_INET:
        in_addrlen = sizeof(in_addr);
        break;
      case AF_INET6:
        in_addrlen = sizeof(in6_addr);
        break;
      default:
        assert(0);
        abort();
      }

      if (RTA_LENGTH(in_addrlen) != rta->rta_len) {
        return -1;
      }

      memcpy(&iau, RTA_DATA(rta), in_addrlen);

      break;
    }
  }

  if (in_addrlen == 0) {
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

int get_local_addr(in_addr_union &iau, const Address &remote_addr) {
  sockaddr_nl sa{
    .nl_family = AF_NETLINK,
  };

  auto fd = socket(AF_NETLINK, SOCK_RAW, NETLINK_ROUTE);
  if (fd == -1) {
    std::cerr << "socket: Could not create netlink socket: " << strerror(errno)
              << std::endl;
    return -1;
  }

  auto fd_d = defer(close, fd);

  if (bind(fd, reinterpret_cast<sockaddr *>(&sa), sizeof(sa)) == -1) {
    std::cerr << "bind: Could not bind netlink socket: " << strerror(errno)
              << std::endl;
    return -1;
  }

  uint32_t seq = 1;

  if (send_netlink_msg(fd, remote_addr, seq) != 0) {
    return -1;
  }

  return recv_netlink_msg(iau, fd, seq);
}

#endif // defined(HAVE_LINUX_NETLINK_H)

bool addreq(const sockaddr *sa, const in_addr_union &iau) {
  switch (sa->sa_family) {
  case AF_INET:
    return memcmp(&reinterpret_cast<const sockaddr_in *>(sa)->sin_addr, &iau.in,
                  sizeof(iau.in)) == 0;
  case AF_INET6:
    return memcmp(&reinterpret_cast<const sockaddr_in6 *>(sa)->sin6_addr,
                  &iau.in6, sizeof(iau.in6)) == 0;
  default:
    assert(0);
    abort();
  }
}

} // namespace ngtcp2
