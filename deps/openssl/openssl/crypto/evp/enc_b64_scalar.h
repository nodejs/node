#ifndef OSSL_CRYPTO_EVP_B64_SCALAR_H
#define OSSL_CRYPTO_EVP_B64_SCALAR_H
#include <openssl/evp.h>
#include <stddef.h>

size_t evp_encodeblock_int(EVP_ENCODE_CTX *ctx, unsigned char *t,
    const unsigned char *f, int dlen, int *wrap_cnt);

#endif
