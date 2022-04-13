/*
 * Copyright 2020-2021 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the Apache License 2.0 (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

#ifndef OSSL_CRYPTO_DECODER_H
# define OSSL_CRYPTO_DECODER_H
# pragma once

# include <openssl/decoder.h>

OSSL_DECODER *ossl_decoder_fetch_by_number(OSSL_LIB_CTX *libctx,
                                                     int id,
                                                     const char *properties);

/*
 * These are specially made for the 'file:' provider-native loader, which
 * uses this to install a DER to anything decoder, which doesn't do much
 * except read a DER blob and pass it on as a provider object abstraction
 * (provider-object(7)).
 */
void *ossl_decoder_from_algorithm(int id, const OSSL_ALGORITHM *algodef,
                                  OSSL_PROVIDER *prov);

OSSL_DECODER_INSTANCE *
ossl_decoder_instance_new(OSSL_DECODER *decoder, void *decoderctx);
void ossl_decoder_instance_free(OSSL_DECODER_INSTANCE *decoder_inst);
int ossl_decoder_ctx_add_decoder_inst(OSSL_DECODER_CTX *ctx,
                                      OSSL_DECODER_INSTANCE *di);

int ossl_decoder_ctx_setup_for_pkey(OSSL_DECODER_CTX *ctx,
                                    EVP_PKEY **pkey, const char *keytype,
                                    OSSL_LIB_CTX *libctx,
                                    const char *propquery);

int ossl_decoder_get_number(const OSSL_DECODER *encoder);

#endif
