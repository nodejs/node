/*
 * Copyright 1995-2016 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the OpenSSL license (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

#include "async_bio.h"

#include <errno.h>
#include <string.h>

#include <openssl/bio.h>
#include <openssl/crypto.h>


namespace {

struct AsyncBio {
  bool datagram;
  bool enforce_write_quota;
  size_t read_quota;
  size_t write_quota;
};

AsyncBio *GetData(BIO *bio) {
  return (AsyncBio *)BIO_get_data(bio);
}

static int AsyncWrite(BIO *bio, const char *in, int inl) {
  AsyncBio *a = GetData(bio);
  if (a == NULL || BIO_next(bio) == NULL) {
    return 0;
  }

  if (!a->enforce_write_quota) {
    return BIO_write(BIO_next(bio), in, inl);
  }

  BIO_clear_retry_flags(bio);

  if (a->write_quota == 0) {
    BIO_set_retry_write(bio);
    errno = EAGAIN;
    return -1;
  }

  if (!a->datagram && (size_t)inl > a->write_quota) {
    inl = a->write_quota;
  }
  int ret = BIO_write(BIO_next(bio), in, inl);
  if (ret <= 0) {
    BIO_copy_next_retry(bio);
  } else {
    a->write_quota -= (a->datagram ? 1 : ret);
  }
  return ret;
}

static int AsyncRead(BIO *bio, char *out, int outl) {
  AsyncBio *a = GetData(bio);
  if (a == NULL || BIO_next(bio) == NULL) {
    return 0;
  }

  BIO_clear_retry_flags(bio);

  if (a->read_quota == 0) {
    BIO_set_retry_read(bio);
    errno = EAGAIN;
    return -1;
  }

  if (!a->datagram && (size_t)outl > a->read_quota) {
    outl = a->read_quota;
  }
  int ret = BIO_read(BIO_next(bio), out, outl);
  if (ret <= 0) {
    BIO_copy_next_retry(bio);
  } else {
    a->read_quota -= (a->datagram ? 1 : ret);
  }
  return ret;
}

static long AsyncCtrl(BIO *bio, int cmd, long num, void *ptr) {
  if (BIO_next(bio) == NULL) {
    return 0;
  }
  BIO_clear_retry_flags(bio);
  int ret = BIO_ctrl(BIO_next(bio), cmd, num, ptr);
  BIO_copy_next_retry(bio);
  return ret;
}

static int AsyncNew(BIO *bio) {
  AsyncBio *a = (AsyncBio *)OPENSSL_malloc(sizeof(*a));
  if (a == NULL) {
    return 0;
  }
  memset(a, 0, sizeof(*a));
  a->enforce_write_quota = true;
  BIO_set_init(bio, 1);
  BIO_set_data(bio, a);
  return 1;
}

static int AsyncFree(BIO *bio) {
  if (bio == NULL) {
    return 0;
  }

  OPENSSL_free(BIO_get_data(bio));
  BIO_set_data(bio, NULL);
  BIO_set_init(bio, 0);
  return 1;
}

static long AsyncCallbackCtrl(BIO *bio, int cmd, BIO_info_cb fp)
{
  if (BIO_next(bio) == NULL)
    return 0;
  return BIO_callback_ctrl(BIO_next(bio), cmd, fp);
}

static BIO_METHOD *g_async_bio_method = NULL;

static const BIO_METHOD *AsyncMethod(void)
{
  if (g_async_bio_method == NULL) {
    g_async_bio_method = BIO_meth_new(BIO_TYPE_FILTER, "async bio");
    if (   g_async_bio_method == NULL
        || !BIO_meth_set_write(g_async_bio_method, AsyncWrite)
        || !BIO_meth_set_read(g_async_bio_method, AsyncRead)
        || !BIO_meth_set_ctrl(g_async_bio_method, AsyncCtrl)
        || !BIO_meth_set_create(g_async_bio_method, AsyncNew)
        || !BIO_meth_set_destroy(g_async_bio_method, AsyncFree)
        || !BIO_meth_set_callback_ctrl(g_async_bio_method, AsyncCallbackCtrl))
    return NULL;
  }
  return g_async_bio_method;
}

}  // namespace

bssl::UniquePtr<BIO> AsyncBioCreate() {
  return bssl::UniquePtr<BIO>(BIO_new(AsyncMethod()));
}

bssl::UniquePtr<BIO> AsyncBioCreateDatagram() {
  bssl::UniquePtr<BIO> ret(BIO_new(AsyncMethod()));
  if (!ret) {
    return nullptr;
  }
  GetData(ret.get())->datagram = true;
  return ret;
}

void AsyncBioAllowRead(BIO *bio, size_t count) {
  AsyncBio *a = GetData(bio);
  if (a == NULL) {
    return;
  }
  a->read_quota += count;
}

void AsyncBioAllowWrite(BIO *bio, size_t count) {
  AsyncBio *a = GetData(bio);
  if (a == NULL) {
    return;
  }
  a->write_quota += count;
}

void AsyncBioEnforceWriteQuota(BIO *bio, bool enforce) {
  AsyncBio *a = GetData(bio);
  if (a == NULL) {
    return;
  }
  a->enforce_write_quota = enforce;
}
