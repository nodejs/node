#include <immintrin.h>
#include "chromiumbase64.h"

static inline void
enc_loop_avx512 (const char* src, size_t slen, char* dst, size_t* dlen) {
    size_t dlen_encoded = 0;
    // compliant with current simd base64 impl which only supports NORMAL mode
    static char base64_table[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                                 "abcdefghijklmnopqrstuvwxyz"
                                 "0123456789+/";
    const char* lookup_tbl = base64_table;
    // 32-bit input
    // [ 0  0  0  0  0  0  0  0|c1 c0 d5 d4 d3 d2 d1 d0|
    //  b3 b2 b1 b0 c5 c4 c3 c2|a5 a4 a3 a2 a1 a0 b5 b4]
    // output order  [1, 2, 0, 1]
    // [b3 b2 b1 b0 c5 c4 c3 c2|c1 c0 d5 d4 d3 d2 d1 d0|
    //  a5 a4 a3 a2 a1 a0 b5 b4|b3 b2 b1 b0 c3 c2 c1 c0]

    const __m512i shuffle_input = _mm512_setr_epi32(0x01020001,
                                                    0x04050304,
                                                    0x07080607,
                                                    0x0a0b090a,
                                                    0x0d0e0c0d,
                                                    0x10110f10,
                                                    0x13141213,
                                                    0x16171516,
                                                    0x191a1819,
                                                    0x1c1d1b1c,
                                                    0x1f201e1f,
                                                    0x22232122,
                                                    0x25262425,
                                                    0x28292728,
                                                    0x2b2c2a2b,
                                                    0x2e2f2d2e);
    const __m512i lookup = _mm512_loadu_si512(lookup_tbl);

    while (slen >= 64) {
    const __m512i v = _mm512_loadu_si512(src);

    // Reorder bytes
    // [b3 b2 b1 b0 c5 c4 c3 c2|c1 c0 d5 d4 d3 d2 d1 d0|
    //  a5 a4 a3 a2 a1 a0 b5 b4|b3 b2 b1 b0 c3 c2 c1 c0]
    const __m512i in = _mm512_permutexvar_epi8(shuffle_input, v);

    // After multishift a single 32-bit lane has following layout
    // [c1 c0 d5 d4 d3 d2 d1 d0|b1 b0 c5 c4 c3 c2 c1 c0|
    //  a1 a0 b5 b4 b3 b2 b1 b0|d1 d0 a5 a4 a3 a2 a1 a0]
    // (a = [10:17], b = [4:11], c = [22:27], d = [16:21])

    // 48, 54, 36, 42, 16, 22, 4, 10
    const __m512i shifts = _mm512_set1_epi64(0x3036242a1016040alu);
    const __m512i indices = _mm512_multishift_epi64_epi8(shifts, in);

    // Note: the two higher bits of each indices' byte have garbage
    // but the following permutexvar instruction masks them out

    // Translation 6-bit values to ASCII.
    const __m512i result = _mm512_permutexvar_epi8(indices, lookup);

    _mm512_storeu_si512(dst, result);

    dlen_encoded +=64;
    dst += 64;
    src += 48;
    slen -= 48;
    }

    // Fallback to a fast Base64 encoding used in Chromium project
    if (slen > 0) {
        dlen_encoded += chromium_base64_encode(dst, src, slen);
    }

    *dlen = dlen_encoded;
}