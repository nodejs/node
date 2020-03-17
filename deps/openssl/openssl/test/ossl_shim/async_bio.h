/*
 * Copyright 1995-2016 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the OpenSSL license (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

#ifndef OSSL_TEST_SHIM_ASYNC_BIO_H
#define OSSL_TEST_SHIM_ASYNC_BIO_H

#include <openssl/base.h>
#include <openssl/bio.h>


// AsyncBioCreate creates a filter BIO for testing asynchronous state
// machines which consume a stream socket. Reads and writes will fail
// and return EAGAIN unless explicitly allowed. Each async BIO has a
// read quota and a write quota. Initially both are zero. As each is
// incremented, bytes are allowed to flow through the BIO.
bssl::UniquePtr<BIO> AsyncBioCreate();

// AsyncBioCreateDatagram creates a filter BIO for testing for
// asynchronous state machines which consume datagram sockets. The read
// and write quota count in packets rather than bytes.
bssl::UniquePtr<BIO> AsyncBioCreateDatagram();

// AsyncBioAllowRead increments |bio|'s read quota by |count|.
void AsyncBioAllowRead(BIO *bio, size_t count);

// AsyncBioAllowWrite increments |bio|'s write quota by |count|.
void AsyncBioAllowWrite(BIO *bio, size_t count);

// AsyncBioEnforceWriteQuota configures where |bio| enforces its write quota.
void AsyncBioEnforceWriteQuota(BIO *bio, bool enforce);


#endif  // OSSL_TEST_SHIM_ASYNC_BIO_H
