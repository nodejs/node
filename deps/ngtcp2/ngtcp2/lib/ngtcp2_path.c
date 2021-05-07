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
#include "ngtcp2_path.h"

#include <string.h>

#include "ngtcp2_addr.h"

void ngtcp2_path_init(ngtcp2_path *path, const ngtcp2_addr *local,
                      const ngtcp2_addr *remote) {
  path->local = *local;
  path->remote = *remote;
}

void ngtcp2_path_copy(ngtcp2_path *dest, const ngtcp2_path *src) {
  ngtcp2_addr_copy(&dest->local, &src->local);
  ngtcp2_addr_copy(&dest->remote, &src->remote);
}

int ngtcp2_path_eq(const ngtcp2_path *a, const ngtcp2_path *b) {
  return ngtcp2_addr_eq(&a->local, &b->local) &&
         ngtcp2_addr_eq(&a->remote, &b->remote);
}

void ngtcp2_path_storage_init(ngtcp2_path_storage *ps,
                              const struct sockaddr *local_addr,
                              size_t local_addrlen, void *local_user_data,
                              const struct sockaddr *remote_addr,
                              size_t remote_addrlen, void *remote_user_data) {
  ngtcp2_addr_init(&ps->path.local, (const struct sockaddr *)&ps->local_addrbuf,
                   0, local_user_data);
  ngtcp2_addr_init(&ps->path.remote,
                   (const struct sockaddr *)&ps->remote_addrbuf, 0,
                   remote_user_data);

  ngtcp2_addr_copy_byte(&ps->path.local, local_addr, local_addrlen);
  ngtcp2_addr_copy_byte(&ps->path.remote, remote_addr, remote_addrlen);
}

void ngtcp2_path_storage_init2(ngtcp2_path_storage *ps,
                               const ngtcp2_path *path) {
  ngtcp2_path_storage_init(ps, path->local.addr, path->local.addrlen,
                           path->local.user_data, path->remote.addr,
                           path->remote.addrlen, path->remote.user_data);
}

void ngtcp2_path_storage_zero(ngtcp2_path_storage *ps) {
  ngtcp2_addr_init(&ps->path.local, (const struct sockaddr *)&ps->local_addrbuf,
                   0, NULL);
  ngtcp2_addr_init(&ps->path.remote,
                   (const struct sockaddr *)&ps->remote_addrbuf, 0, NULL);
}
