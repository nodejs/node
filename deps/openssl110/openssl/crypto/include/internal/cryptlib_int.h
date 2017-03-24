/*
 * Copyright 2016 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the OpenSSL license (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

#include <internal/cryptlib.h>

/* This file is not scanned by mkdef.pl, whereas cryptlib.h is */

struct thread_local_inits_st {
    int async;
    int err_state;
};

int ossl_init_thread_start(uint64_t opts);

/*
 * OPENSSL_INIT flags. The primary list of these is in crypto.h. Flags below
 * are those omitted from crypto.h because they are "reserved for internal
 * use".
 */
# define OPENSSL_INIT_ZLIB                   0x00010000L

/* OPENSSL_INIT_THREAD flags */
# define OPENSSL_INIT_THREAD_ASYNC           0x01
# define OPENSSL_INIT_THREAD_ERR_STATE       0x02

