/*
 * Copyright 1995-2016 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the OpenSSL license (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

#ifndef HEADER_HMAC_LCL_H
# define HEADER_HMAC_LCL_H

#ifdef  __cplusplus
extern "C" {
#endif
#if 0                            /* emacs indentation fix */
}
#endif

struct hmac_ctx_st {
    const EVP_MD *md;
    EVP_MD_CTX *md_ctx;
    EVP_MD_CTX *i_ctx;
    EVP_MD_CTX *o_ctx;
    unsigned int key_length;
    unsigned char key[HMAC_MAX_MD_CBLOCK];
};

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif
