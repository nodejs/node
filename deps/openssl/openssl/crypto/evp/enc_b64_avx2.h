#ifndef OSSL_CRYPTO_EVP_B64_AVX2_H
#define OSSL_CRYPTO_EVP_B64_AVX2_H

#include <openssl/evp.h>
#include <stddef.h>

#if defined(__clang__)
#define HAVE_AVX2_INTRINSICS 1
#elif defined(__GNUC__) && (__GNUC__ >= 8) && !defined(__MINGW32__)
#define HAVE_AVX2_INTRINSICS 1
#elif defined(_MSC_VER) && (_MSC_VER >= 1920) /* MSVC 2019 */
#define HAVE_AVX2_INTRINSICS 1
#endif

#if defined(__x86_64) || defined(__x86_64__) || defined(_M_AMD64) || defined(_M_X64) || defined(__e2k__)
#if !defined(_M_ARM64EC)
#if defined(HAVE_AVX2_INTRINSICS)
size_t encode_base64_avx2(EVP_ENCODE_CTX *ctx,
    unsigned char *out, const unsigned char *src, int srclen,
    int newlines, int *wrap_cnt);
#endif /* defined(HAVE_AVX2_INTRINSICS) */
#endif /* !defined(_M_ARM64EC) */
#endif

#endif
