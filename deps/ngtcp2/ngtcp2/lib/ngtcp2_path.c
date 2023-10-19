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
  dest->user_data = src->user_data;
}

int ngtcp2_path_eq(const ngtcp2_path *a, const ngtcp2_path *b) {
  return ngtcp2_addr_eq(&a->local, &b->local) &&
         ngtcp2_addr_eq(&a->remote, &b->remote);
}

void ngtcp2_path_storage_init(ngtcp2_path_storage *ps,
                              const ngtcp2_sockaddr *local_addr,
                              ngtcp2_socklen local_addrlen,
                              const ngtcp2_sockaddr *remote_addr,
                              ngtcp2_socklen remote_addrlen, void *user_data) {
  ngtcp2_addr_init(&ps->path.local, (const ngtcp2_sockaddr *)&ps->local_addrbuf,
                   0);
  ngtcp2_addr_init(&ps->path.remote,
                   (const ngtcp2_sockaddr *)&ps->remote_addrbuf, 0);

  ngtcp2_addr_copy_byte(&ps->path.local, local_addr, local_addrlen);
  ngtcp2_addr_copy_byte(&ps->path.remote, remote_addr, remote_addrlen);

  ps->path.user_data = user_data;
}

void ngtcp2_path_storage_init2(ngtcp2_path_storage *ps,
                               const ngtcp2_path *path) {
  ngtcp2_path_storage_init(ps, path->local.addr, path->local.addrlen,
                           path->remote.addr, path->remote.addrlen,
                           path->user_data);
}

void ngtcp2_path_storage_zero(ngtcp2_path_storage *ps) {
  ngtcp2_addr_init(&ps->path.local, (const ngtcp2_sockaddr *)&ps->local_addrbuf,
                   0);
  ngtcp2_addr_init(&ps->path.remote,
                   (const ngtcp2_sockaddr *)&ps->remote_addrbuf, 0);
  ps->path.user_data = NULL;
}
