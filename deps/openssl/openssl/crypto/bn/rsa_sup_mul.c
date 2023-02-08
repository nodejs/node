#include <openssl/e_os2.h>
#include <stddef.h>
#include <sys/types.h>
#include <string.h>
#include <openssl/bn.h>
#include <openssl/err.h>
#include <openssl/rsaerr.h>
#include "internal/numbers.h"
#include "internal/constant_time.h"
#include "bn_local.h"

# if BN_BYTES == 8
typedef uint64_t limb_t;
#  if defined(__SIZEOF_INT128__) && __SIZEOF_INT128__ == 16
/* nonstandard; implemented by gcc on 64-bit platforms */
typedef __uint128_t limb2_t;
#   define HAVE_LIMB2_T
#  endif
#  define LIMB_BIT_SIZE 64
#  define LIMB_BYTE_SIZE 8
# elif BN_BYTES == 4
typedef uint32_t limb_t;
typedef uint64_t limb2_t;
#  define LIMB_BIT_SIZE 32
#  define LIMB_BYTE_SIZE 4
#  define HAVE_LIMB2_T
# else
#  error "Not supported"
# endif

/*
 * For multiplication we're using schoolbook multiplication,
 * so if we have two numbers, each with 6 "digits" (words)
 * the multiplication is calculated as follows:
 *                        A B C D E F
 *                     x  I J K L M N
 *                     --------------
 *                                N*F
 *                              N*E
 *                            N*D
 *                          N*C
 *                        N*B
 *                      N*A
 *                              M*F
 *                            M*E
 *                          M*D
 *                        M*C
 *                      M*B
 *                    M*A
 *                            L*F
 *                          L*E
 *                        L*D
 *                      L*C
 *                    L*B
 *                  L*A
 *                          K*F
 *                        K*E
 *                      K*D
 *                    K*C
 *                  K*B
 *                K*A
 *                        J*F
 *                      J*E
 *                    J*D
 *                  J*C
 *                J*B
 *              J*A
 *                      I*F
 *                    I*E
 *                  I*D
 *                I*C
 *              I*B
 *         +  I*A
 *         ==========================
 *                        N*B N*D N*F
 *                    + N*A N*C N*E
 *                    + M*B M*D M*F
 *                  + M*A M*C M*E
 *                  + L*B L*D L*F
 *                + L*A L*C L*E
 *                + K*B K*D K*F
 *              + K*A K*C K*E
 *              + J*B J*D J*F
 *            + J*A J*C J*E
 *            + I*B I*D I*F
 *          + I*A I*C I*E
 *
 *                1+1 1+3 1+5
 *              1+0 1+2 1+4
 *              0+1 0+3 0+5
 *            0+0 0+2 0+4
 *
 *            0 1 2 3 4 5 6
 * which requires n^2 multiplications and 2n full length additions
 * as we can keep every other result of limb multiplication in two separate
 * limbs
 */

#if defined HAVE_LIMB2_T
static ossl_inline void _mul_limb(limb_t *hi, limb_t *lo, limb_t a, limb_t b)
{
    limb2_t t;
    /*
     * this is idiomatic code to tell compiler to use the native mul
     * those three lines will actually compile to single instruction
     */

    t = (limb2_t)a * b;
    *hi = t >> LIMB_BIT_SIZE;
    *lo = (limb_t)t;
}
#elif (BN_BYTES == 8) && (defined _MSC_VER)
/* https://learn.microsoft.com/en-us/cpp/intrinsics/umul128?view=msvc-170 */
#pragma intrinsic(_umul128)
static ossl_inline void _mul_limb(limb_t *hi, limb_t *lo, limb_t a, limb_t b)
{
    *lo = _umul128(a, b, hi);
}
#else
/*
 * if the compiler doesn't have either a 128bit data type nor a "return
 * high 64 bits of multiplication"
 */
static ossl_inline void _mul_limb(limb_t *hi, limb_t *lo, limb_t a, limb_t b)
{
    limb_t a_low = (limb_t)(uint32_t)a;
    limb_t a_hi = a >> 32;
    limb_t b_low = (limb_t)(uint32_t)b;
    limb_t b_hi = b >> 32;

    limb_t p0 = a_low * b_low;
    limb_t p1 = a_low * b_hi;
    limb_t p2 = a_hi * b_low;
    limb_t p3 = a_hi * b_hi;

    uint32_t cy = (uint32_t)(((p0 >> 32) + (uint32_t)p1 + (uint32_t)p2) >> 32);

    *lo = p0 + (p1 << 32) + (p2 << 32);
    *hi = p3 + (p1 >> 32) + (p2 >> 32) + cy;
}
#endif

/* add two limbs with carry in, return carry out */
static ossl_inline limb_t _add_limb(limb_t *ret, limb_t a, limb_t b, limb_t carry)
{
    limb_t carry1, carry2, t;
    /*
     * `c = a + b; if (c < a)` is idiomatic code that makes compilers
     * use add with carry on assembly level
     */

    *ret = a + carry;
    if (*ret < a)
        carry1 = 1;
    else
        carry1 = 0;

    t = *ret;
    *ret = t + b;
    if (*ret < t)
        carry2 = 1;
    else
        carry2 = 0;

    return carry1 + carry2;
}

/*
 * add two numbers of the same size, return overflow
 *
 * add a to b, place result in ret; all arrays need to be n limbs long
 * return overflow from addition (0 or 1)
 */
static ossl_inline limb_t add(limb_t *ret, limb_t *a, limb_t *b, size_t n)
{
    limb_t c = 0;
    ossl_ssize_t i;

    for(i = n - 1; i > -1; i--)
        c = _add_limb(&ret[i], a[i], b[i], c);

    return c;
}

/*
 * return number of limbs necessary for temporary values
 * when multiplying numbers n limbs large
 */
static ossl_inline size_t mul_limb_numb(size_t n)
{
    return  2 * n * 2;
}

/*
 * multiply two numbers of the same size
 *
 * multiply a by b, place result in ret; a and b need to be n limbs long
 * ret needs to be 2*n limbs long, tmp needs to be mul_limb_numb(n) limbs
 * long
 */
static void limb_mul(limb_t *ret, limb_t *a, limb_t *b, size_t n, limb_t *tmp)
{
    limb_t *r_odd, *r_even;
    size_t i, j, k;

    r_odd = tmp;
    r_even = &tmp[2 * n];

    memset(ret, 0, 2 * n * sizeof(limb_t));

    for (i = 0; i < n; i++) {
        for (k = 0; k < i + n + 1; k++) {
            r_even[k] = 0;
            r_odd[k] = 0;
        }
        for (j = 0; j < n; j++) {
            /*
             * place results from even and odd limbs in separate arrays so that
             * we don't have to calculate overflow every time we get individual
             * limb multiplication result
             */
            if (j % 2 == 0)
                _mul_limb(&r_even[i + j], &r_even[i + j + 1], a[i], b[j]);
            else
                _mul_limb(&r_odd[i + j], &r_odd[i + j + 1], a[i], b[j]);
        }
        /*
         * skip the least significant limbs when adding multiples of
         * more significant limbs (they're zero anyway)
         */
        add(ret, ret, r_even, n + i + 1);
        add(ret, ret, r_odd, n + i + 1);
    }
}

/* modifies the value in place by performing a right shift by one bit */
static ossl_inline void rshift1(limb_t *val, size_t n)
{
    limb_t shift_in = 0, shift_out = 0;
    size_t i;

    for (i = 0; i < n; i++) {
        shift_out = val[i] & 1;
        val[i] = shift_in << (LIMB_BIT_SIZE - 1) | (val[i] >> 1);
        shift_in = shift_out;
    }
}

/* extend the LSB of flag to all bits of limb */
static ossl_inline limb_t mk_mask(limb_t flag)
{
    flag |= flag << 1;
    flag |= flag << 2;
    flag |= flag << 4;
    flag |= flag << 8;
    flag |= flag << 16;
#if (LIMB_BYTE_SIZE == 8)
    flag |= flag << 32;
#endif
    return flag;
}

/*
 * copy from either a or b to ret based on flag
 * when flag == 0, then copies from b
 * when flag == 1, then copies from a
 */
static ossl_inline void cselect(limb_t flag, limb_t *ret, limb_t *a, limb_t *b, size_t n)
{
    /*
     * would be more efficient with non volatile mask, but then gcc
     * generates code with jumps
     */
    volatile limb_t mask;
    size_t i;

    mask = mk_mask(flag);
    for (i = 0; i < n; i++) {
#if (LIMB_BYTE_SIZE == 8)
        ret[i] = constant_time_select_64(mask, a[i], b[i]);
#else
        ret[i] = constant_time_select_32(mask, a[i], b[i]);
#endif
    }
}

static limb_t _sub_limb(limb_t *ret, limb_t a, limb_t b, limb_t borrow)
{
    limb_t borrow1, borrow2, t;
    /*
     * while it doesn't look constant-time, this is idiomatic code
     * to tell compilers to use the carry bit from subtraction
     */

    *ret = a - borrow;
    if (*ret > a)
        borrow1 = 1;
    else
        borrow1 = 0;

    t = *ret;
    *ret = t - b;
    if (*ret > t)
        borrow2 = 1;
    else
        borrow2 = 0;

    return borrow1 + borrow2;
}

/*
 * place the result of a - b into ret, return the borrow bit.
 * All arrays need to be n limbs long
 */
static limb_t sub(limb_t *ret, limb_t *a, limb_t *b, size_t n)
{
    limb_t borrow = 0;
    ossl_ssize_t i;

    for (i = n - 1; i > -1; i--)
        borrow = _sub_limb(&ret[i], a[i], b[i], borrow);

    return borrow;
}

/* return the number of limbs necessary to allocate for the mod() tmp operand */
static ossl_inline size_t mod_limb_numb(size_t anum, size_t modnum)
{
    return (anum + modnum) * 3;
}

/*
 * calculate a % mod, place the result in ret
 * size of a is defined by anum, size of ret and mod is modnum,
 * size of tmp is returned by mod_limb_numb()
 */
static void mod(limb_t *ret, limb_t *a, size_t anum, limb_t *mod,
               size_t modnum, limb_t *tmp)
{
    limb_t *atmp, *modtmp, *rettmp;
    limb_t res;
    size_t i;

    memset(tmp, 0, mod_limb_numb(anum, modnum) * LIMB_BYTE_SIZE);

    atmp = tmp;
    modtmp = &tmp[anum + modnum];
    rettmp = &tmp[(anum + modnum) * 2];

    for (i = modnum; i <modnum + anum; i++)
        atmp[i] = a[i-modnum];

    for (i = 0; i < modnum; i++)
        modtmp[i] = mod[i];

    for (i = 0; i < anum * LIMB_BIT_SIZE; i++) {
        rshift1(modtmp, anum + modnum);
        res = sub(rettmp, atmp, modtmp, anum+modnum);
        cselect(res, atmp, atmp, rettmp, anum+modnum);
    }

    memcpy(ret, &atmp[anum], sizeof(limb_t) * modnum);
}

/* necessary size of tmp for a _mul_add_limb() call with provided anum */
static ossl_inline size_t _mul_add_limb_numb(size_t anum)
{
    return 2 * (anum + 1);
}

/* multiply a by m, add to ret, return carry */
static limb_t _mul_add_limb(limb_t *ret, limb_t *a, size_t anum,
                           limb_t m, limb_t *tmp)
{
    limb_t carry = 0;
    limb_t *r_odd, *r_even;
    size_t i;

    memset(tmp, 0, sizeof(limb_t) * (anum + 1) * 2);

    r_odd = tmp;
    r_even = &tmp[anum + 1];

    for (i = 0; i < anum; i++) {
        /*
         * place the results from even and odd limbs in separate arrays
         * so that we have to worry about carry just once
         */
        if (i % 2 == 0)
            _mul_limb(&r_even[i], &r_even[i + 1], a[i], m);
        else
            _mul_limb(&r_odd[i], &r_odd[i + 1], a[i], m);
    }
    /* assert: add() carry here will be equal zero */
    add(r_even, r_even, r_odd, anum + 1);
    /*
     * while here it will not overflow as the max value from multiplication
     * is -2 while max overflow from addition is 1, so the max value of
     * carry is -1 (i.e. max int)
     */
    carry = add(ret, ret, &r_even[1], anum) + r_even[0];

    return carry;
}

static ossl_inline size_t mod_montgomery_limb_numb(size_t modnum)
{
    return modnum * 2 + _mul_add_limb_numb(modnum);
}

/*
 * calculate a % mod, place result in ret
 * assumes that a is in Montgomery form with the R (Montgomery modulus) being
 * smallest power of two big enough to fit mod and that's also a power
 * of the count of number of bits in limb_t (B).
 * For calculation, we also need n', such that mod * n' == -1 mod B.
 * anum must be <= 2 * modnum
 * ret needs to be modnum words long
 * tmp needs to be mod_montgomery_limb_numb(modnum) limbs long
 */
static void mod_montgomery(limb_t *ret, limb_t *a, size_t anum, limb_t *mod,
                          size_t modnum, limb_t ni0, limb_t *tmp)
{
    limb_t carry, v;
    limb_t *res, *rp, *tmp2;
    ossl_ssize_t i;

    res = tmp;
    /*
     * for intermediate result we need an integer twice as long as modulus
     * but keep the input in the least significant limbs
     */
    memset(res, 0, sizeof(limb_t) * (modnum * 2));
    memcpy(&res[modnum * 2 - anum], a, sizeof(limb_t) * anum);
    rp = &res[modnum];
    tmp2 = &res[modnum * 2];

    carry = 0;

    /* add multiples of the modulus to the value until R divides it cleanly */
    for (i = modnum; i > 0; i--, rp--) {
        v = _mul_add_limb(rp, mod, modnum, rp[modnum - 1] * ni0, tmp2);
        v = v + carry + rp[-1];
        carry |= (v != rp[-1]);
        carry &= (v <= rp[-1]);
        rp[-1] = v;
    }

    /* perform the final reduction by mod... */
    carry -= sub(ret, rp, mod, modnum);

    /* ...conditionally */
    cselect(carry, ret, rp, ret, modnum);
}

/* allocated buffer should be freed afterwards */
static void BN_to_limb(const BIGNUM *bn, limb_t *buf, size_t limbs)
{
    int i;
    int real_limbs = (BN_num_bytes(bn) + LIMB_BYTE_SIZE - 1) / LIMB_BYTE_SIZE;
    limb_t *ptr = buf + (limbs - real_limbs);

    for (i = 0; i < real_limbs; i++)
         ptr[i] = bn->d[real_limbs - i - 1];
}

#if LIMB_BYTE_SIZE == 8
static ossl_inline uint64_t be64(uint64_t host)
{
    const union {
        long one;
        char little;
    } is_endian = { 1 };

    if (is_endian.little) {
        uint64_t big = 0;

        big |= (host & 0xff00000000000000) >> 56;
        big |= (host & 0x00ff000000000000) >> 40;
        big |= (host & 0x0000ff0000000000) >> 24;
        big |= (host & 0x000000ff00000000) >>  8;
        big |= (host & 0x00000000ff000000) <<  8;
        big |= (host & 0x0000000000ff0000) << 24;
        big |= (host & 0x000000000000ff00) << 40;
        big |= (host & 0x00000000000000ff) << 56;
        return big;
    } else {
        return host;
    }
}

#else
/* Not all platforms have htobe32(). */
static ossl_inline uint32_t be32(uint32_t host)
{
    const union {
        long one;
        char little;
    } is_endian = { 1 };

    if (is_endian.little) {
        uint32_t big = 0;

        big |= (host & 0xff000000) >> 24;
        big |= (host & 0x00ff0000) >> 8;
        big |= (host & 0x0000ff00) << 8;
        big |= (host & 0x000000ff) << 24;
        return big;
    } else {
        return host;
    }
}
#endif

/*
 * We assume that intermediate, possible_arg2, blinding, and ctx are used
 * similar to BN_BLINDING_invert_ex() arguments.
 * to_mod is RSA modulus.
 * buf and num is the serialization buffer and its length.
 *
 * Here we use classic/Montgomery multiplication and modulo. After the calculation finished
 * we serialize the new structure instead of BIGNUMs taking endianness into account.
 */
int ossl_bn_rsa_do_unblind(const BIGNUM *intermediate,
                           const BN_BLINDING *blinding,
                           const BIGNUM *possible_arg2,
                           const BIGNUM *to_mod, BN_CTX *ctx,
                           unsigned char *buf, int num)
{
    limb_t *l_im = NULL, *l_mul = NULL, *l_mod = NULL;
    limb_t *l_ret = NULL, *l_tmp = NULL, l_buf;
    size_t l_im_count = 0, l_mul_count = 0, l_size = 0, l_mod_count = 0;
    size_t l_tmp_count = 0;
    int ret = 0;
    size_t i;
    unsigned char *tmp;
    const BIGNUM *arg1 = intermediate;
    const BIGNUM *arg2 = (possible_arg2 == NULL) ? blinding->Ai : possible_arg2;

    l_im_count  = (BN_num_bytes(arg1)   + LIMB_BYTE_SIZE - 1) / LIMB_BYTE_SIZE;
    l_mul_count = (BN_num_bytes(arg2)   + LIMB_BYTE_SIZE - 1) / LIMB_BYTE_SIZE;
    l_mod_count = (BN_num_bytes(to_mod) + LIMB_BYTE_SIZE - 1) / LIMB_BYTE_SIZE;

    l_size = l_im_count > l_mul_count ? l_im_count : l_mul_count;
    l_im  = OPENSSL_zalloc(l_size * LIMB_BYTE_SIZE);
    l_mul = OPENSSL_zalloc(l_size * LIMB_BYTE_SIZE);
    l_mod = OPENSSL_zalloc(l_mod_count * LIMB_BYTE_SIZE);

    if ((l_im == NULL) || (l_mul == NULL) || (l_mod == NULL))
        goto err;

    BN_to_limb(arg1,   l_im,  l_size);
    BN_to_limb(arg2,   l_mul, l_size);
    BN_to_limb(to_mod, l_mod, l_mod_count);

    l_ret = OPENSSL_malloc(2 * l_size * LIMB_BYTE_SIZE);

    if (blinding->m_ctx != NULL) {
        l_tmp_count = mul_limb_numb(l_size) > mod_montgomery_limb_numb(l_mod_count) ?
                      mul_limb_numb(l_size) : mod_montgomery_limb_numb(l_mod_count);
        l_tmp = OPENSSL_malloc(l_tmp_count * LIMB_BYTE_SIZE);
    } else {
        l_tmp_count = mul_limb_numb(l_size) > mod_limb_numb(2 * l_size, l_mod_count) ?
                      mul_limb_numb(l_size) : mod_limb_numb(2 * l_size, l_mod_count);
        l_tmp = OPENSSL_malloc(l_tmp_count * LIMB_BYTE_SIZE);
    }

    if ((l_ret == NULL) || (l_tmp == NULL))
        goto err;

    if (blinding->m_ctx != NULL) {
        limb_mul(l_ret, l_im, l_mul, l_size, l_tmp);
        mod_montgomery(l_ret, l_ret, 2 * l_size, l_mod, l_mod_count,
                       blinding->m_ctx->n0[0], l_tmp);
    } else {
        limb_mul(l_ret, l_im, l_mul, l_size, l_tmp);
        mod(l_ret, l_ret, 2 * l_size, l_mod, l_mod_count, l_tmp);
    }

    /* modulus size in bytes can be equal to num but after limbs conversion it becomes bigger */
    if (num < BN_num_bytes(to_mod)) {
        BNerr(BN_F_OSSL_BN_RSA_DO_UNBLIND, ERR_R_PASSED_INVALID_ARGUMENT);
        goto err;
    }

    memset(buf, 0, num);
    tmp = buf + num - BN_num_bytes(to_mod);
    for (i = 0; i < l_mod_count; i++) {
#if LIMB_BYTE_SIZE == 8
        l_buf = be64(l_ret[i]);
#else
        l_buf = be32(l_ret[i]);
#endif
        if (i == 0) {
            int delta = LIMB_BYTE_SIZE - ((l_mod_count * LIMB_BYTE_SIZE) - num);

            memcpy(tmp, ((char *)&l_buf) + LIMB_BYTE_SIZE - delta, delta);
            tmp += delta;
        } else {
            memcpy(tmp, &l_buf, LIMB_BYTE_SIZE);
            tmp += LIMB_BYTE_SIZE;
        }
    }
    ret = num;

 err:
    OPENSSL_free(l_im);
    OPENSSL_free(l_mul);
    OPENSSL_free(l_mod);
    OPENSSL_free(l_tmp);
    OPENSSL_free(l_ret);

    return ret;
}
