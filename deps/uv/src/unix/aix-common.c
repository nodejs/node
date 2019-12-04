/* Copyright libuv project contributors. All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

#include "uv.h"
#include "internal.h"

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <errno.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <netinet/in.h>
#include <netinet/in6_var.h>
#include <arpa/inet.h>

#include <sys/time.h>
#include <unistd.h>
#include <fcntl.h>
#include <utmp.h>
#include <libgen.h>

#include <sys/protosw.h>
#include <procinfo.h>
#include <sys/proc.h>
#include <sys/procfs.h>

#include <sys/poll.h>

#include <sys/pollset.h>
#include <ctype.h>

#include <sys/mntctl.h>
#include <sys/vmount.h>
#include <limits.h>
#include <strings.h>
#include <sys/vnode.h>

uint64_t uv__hrtime(uv_clocktype_t type) {
  uint64_t G = 1000000000;
  timebasestruct_t t;
  read_wall_time(&t, TIMEBASE_SZ);
  time_base_to_time(&t, TIMEBASE_SZ);
  return (uint64_t) t.tb_high * G + t.tb_low;
}


/*
 * We could use a static buffer for the path manipulations that we need outside
 * of the function, but this function could be called by multiple consumers and
 * we don't want to potentially create a race condition in the use of snprintf.
 * There is no direct way of getting the exe path in AIX - either through /procfs
 * or through some libc APIs. The below approach is to parse the argv[0]'s pattern
 * and use it in conjunction with PATH environment variable to craft one.
 */
int uv_exepath(char* buffer, size_t* size) {
  int res;
  char args[PATH_MAX];
  char abspath[PATH_MAX];
  size_t abspath_size;
  struct procsinfo pi;

  if (buffer == NULL || size == NULL || *size == 0)
    return UV_EINVAL;

  pi.pi_pid = getpid();
  res = getargs(&pi, sizeof(pi), args, sizeof(args));
  if (res < 0)
    return UV_EINVAL;

  /*
   * Possibilities for args:
   * i) an absolute path such as: /home/user/myprojects/nodejs/node
   * ii) a relative path such as: ./node or ../myprojects/nodejs/node
   * iii) a bare filename such as "node", after exporting PATH variable
   *     to its location.
   */

  /* Case i) and ii) absolute or relative paths */
  if (strchr(args, '/') != NULL) {
    if (realpath(args, abspath) != abspath)
      return UV__ERR(errno);

    abspath_size = strlen(abspath);

    *size -= 1;
    if (*size > abspath_size)
      *size = abspath_size;

    memcpy(buffer, abspath, *size);
    buffer[*size] = '\0';

    return 0;
  } else {
    /* Case iii). Search PATH environment variable */
    char trypath[PATH_MAX];
    char *clonedpath = NULL;
    char *token = NULL;
    char *path = getenv("PATH");

    if (path == NULL)
      return UV_EINVAL;

    clonedpath = uv__strdup(path);
    if (clonedpath == NULL)
      return UV_ENOMEM;

    token = strtok(clonedpath, ":");
    while (token != NULL) {
      snprintf(trypath, sizeof(trypath) - 1, "%s/%s", token, args);
      if (realpath(trypath, abspath) == abspath) {
        /* Check the match is executable */
        if (access(abspath, X_OK) == 0) {
          abspath_size = strlen(abspath);

          *size -= 1;
          if (*size > abspath_size)
            *size = abspath_size;

          memcpy(buffer, abspath, *size);
          buffer[*size] = '\0';

          uv__free(clonedpath);
          return 0;
        }
      }
      token = strtok(NULL, ":");
    }
    uv__free(clonedpath);

    /* Out of tokens (path entries), and no match found */
    return UV_EINVAL;
  }
}


int uv_interface_addresses(uv_interface_address_t** addresses, int* count) {
  uv_interface_address_t* address;
  int sockfd, sock6fd, inet6, i, r, size = 1;
  struct ifconf ifc;
  struct ifreq *ifr, *p, flg;
  struct in6_ifreq if6;
  struct sockaddr_dl* sa_addr;

  ifc.ifc_req = NULL;
  sock6fd = -1;
  r = 0;
  *count = 0;
  *addresses = NULL;

  if (0 > (sockfd = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP))) {
    r = UV__ERR(errno);
    goto cleanup;
  }

  if (0 > (sock6fd = socket(AF_INET6, SOCK_DGRAM, IPPROTO_IP))) {
    r = UV__ERR(errno);
    goto cleanup;
  }

  if (ioctl(sockfd, SIOCGSIZIFCONF, &size) == -1) {
    r = UV__ERR(errno);
    goto cleanup;
  }

  ifc.ifc_req = (struct ifreq*)uv__malloc(size);
  if (ifc.ifc_req == NULL) {
    r = UV_ENOMEM;
    goto cleanup;
  }
  ifc.ifc_len = size;
  if (ioctl(sockfd, SIOCGIFCONF, &ifc) == -1) {
    r = UV__ERR(errno);
    goto cleanup;
  }

#define ADDR_SIZE(p) MAX((p).sa_len, sizeof(p))

  /* Count all up and running ipv4/ipv6 addresses */
  ifr = ifc.ifc_req;
  while ((char*)ifr < (char*)ifc.ifc_req + ifc.ifc_len) {
    p = ifr;
    ifr = (struct ifreq*)
      ((char*)ifr + sizeof(ifr->ifr_name) + ADDR_SIZE(ifr->ifr_addr));

    if (!(p->ifr_addr.sa_family == AF_INET6 ||
          p->ifr_addr.sa_family == AF_INET))
      continue;

    memcpy(flg.ifr_name, p->ifr_name, sizeof(flg.ifr_name));
    if (ioctl(sockfd, SIOCGIFFLAGS, &flg) == -1) {
      r = UV__ERR(errno);
      goto cleanup;
    }

    if (!(flg.ifr_flags & IFF_UP && flg.ifr_flags & IFF_RUNNING))
      continue;

    (*count)++;
  }

  if (*count == 0)
    goto cleanup;

  /* Alloc the return interface structs */
  *addresses = uv__calloc(*count, sizeof(**addresses));
  if (!(*addresses)) {
    r = UV_ENOMEM;
    goto cleanup;
  }
  address = *addresses;

  ifr = ifc.ifc_req;
  while ((char*)ifr < (char*)ifc.ifc_req + ifc.ifc_len) {
    p = ifr;
    ifr = (struct ifreq*)
      ((char*)ifr + sizeof(ifr->ifr_name) + ADDR_SIZE(ifr->ifr_addr));

    if (!(p->ifr_addr.sa_family == AF_INET6 ||
          p->ifr_addr.sa_family == AF_INET))
      continue;

    inet6 = (p->ifr_addr.sa_family == AF_INET6);

    memcpy(flg.ifr_name, p->ifr_name, sizeof(flg.ifr_name));
    if (ioctl(sockfd, SIOCGIFFLAGS, &flg) == -1)
      goto syserror;

    if (!(flg.ifr_flags & IFF_UP && flg.ifr_flags & IFF_RUNNING))
      continue;

    /* All conditions above must match count loop */

    address->name = uv__strdup(p->ifr_name);

    if (inet6)
      address->address.address6 = *((struct sockaddr_in6*) &p->ifr_addr);
    else
      address->address.address4 = *((struct sockaddr_in*) &p->ifr_addr);

    if (inet6) {
      memset(&if6, 0, sizeof(if6));
      r = uv__strscpy(if6.ifr_name, p->ifr_name, sizeof(if6.ifr_name));
      if (r == UV_E2BIG)
        goto cleanup;
      r = 0;
      memcpy(&if6.ifr_Addr, &p->ifr_addr, sizeof(if6.ifr_Addr));
      if (ioctl(sock6fd, SIOCGIFNETMASK6, &if6) == -1)
        goto syserror;
      address->netmask.netmask6 = *((struct sockaddr_in6*) &if6.ifr_Addr);
      /* Explicitly set family as the ioctl call appears to return it as 0. */
      address->netmask.netmask6.sin6_family = AF_INET6;
    } else {
      if (ioctl(sockfd, SIOCGIFNETMASK, p) == -1)
        goto syserror;
      address->netmask.netmask4 = *((struct sockaddr_in*) &p->ifr_addr);
      /* Explicitly set family as the ioctl call appears to return it as 0. */
      address->netmask.netmask4.sin_family = AF_INET;
    }

    address->is_internal = flg.ifr_flags & IFF_LOOPBACK ? 1 : 0;

    address++;
  }

  /* Fill in physical addresses. */
  ifr = ifc.ifc_req;
  while ((char*)ifr < (char*)ifc.ifc_req + ifc.ifc_len) {
    p = ifr;
    ifr = (struct ifreq*)
      ((char*)ifr + sizeof(ifr->ifr_name) + ADDR_SIZE(ifr->ifr_addr));

    if (p->ifr_addr.sa_family != AF_LINK)
      continue;

    address = *addresses;
    for (i = 0; i < *count; i++) {
      if (strcmp(address->name, p->ifr_name) == 0) {
        sa_addr = (struct sockaddr_dl*) &p->ifr_addr;
        memcpy(address->phys_addr, LLADDR(sa_addr), sizeof(address->phys_addr));
      }
      address++;
    }
  }

#undef ADDR_SIZE
  goto cleanup;

syserror:
  uv_free_interface_addresses(*addresses, *count);
  *addresses = NULL;
  *count = 0;
  r = UV_ENOSYS;

cleanup:
  if (sockfd != -1)
    uv__close(sockfd);
  if (sock6fd != -1)
    uv__close(sock6fd);
  uv__free(ifc.ifc_req);
  return r;
}


void uv_free_interface_addresses(uv_interface_address_t* addresses,
  int count) {
  int i;

  for (i = 0; i < count; ++i) {
    uv__free(addresses[i].name);
  }

  uv__free(addresses);
}
