/* crypto/bn/bn_mul.c */
/* Copyright (C) 1995-1998 Eric Young (eay@cryptsoft.com)
 * All rights reserved.
 *
 * This package is an SSL implementation written
 * by Eric Young (eay@cryptsoft.com).
 * The implementation was written so as to conform with Netscapes SSL.
 *
 * This library is free for commercial and non-commercial use as long as
 * the following conditions are aheared to.  The following conditions
 * apply to all code found in this distribution, be it the RC4, RSA,
 * lhash, DES, etc., code; not just the SSL code.  The SSL documentation
 * included with this distribution is covered by the same copyright terms
 * except that the holder is Tim Hudson (tjh@cryptsoft.com).
 *
 * Copyright remains Eric Young's, and as such any Copyright notices in
 * the code are not to be removed.
 * If this package is used in a product, Eric Young should be given attribution
 * as the author of the parts of the library used.
 * This can be in the form of a textual message at program startup or
 * in documentation (online or textual) provided with the package.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *    "This product includes cryptographic software written by
 *     Eric Young (eay@cryptsoft.com)"
 *    The word 'cryptographic' can be left out if the rouines from the library
 *    being used are not cryptographic related :-).
 * 4. If you include any Windows specific code (or a derivative thereof) from
 *    the apps directory (application code) you must include an acknowledgement:
 *    "This product includes software written by Tim Hudson (tjh@cryptsoft.com)"
 *
 * THIS SOFTWARE IS PROVIDED BY ERIC YOUNG ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * The licence and distribution terms for any publically available version or
 * derivative of this code cannot be changed.  i.e. this code cannot simply be
 * copied and put under another distribution licence
 * [including the GNU Public Licence.]
 */

#ifndef BN_DEBUG
# undef NDEBUG                  /* avoid conflicting definitions */
# define NDEBUG
#endif

#include <stdio.h>
#include <assert.h>
#include "cryptlib.h"
#include "bn_lcl.h"

#if defined(OPENSSL_NO_ASM) || !defined(OPENSSL_BN_ASM_PART_WORDS)
/*
 * Here follows specialised variants of bn_add_words() and bn_sub_words().
 * They have the property performing operations on arrays of different sizes.
 * The sizes of those arrays is expressed through cl, which is the common
 * length ( basicall, min(len(a),len(b)) ), and dl, which is the delta
 * between the two lengths, calculated as len(a)-len(b). All lengths are the
 * number of BN_ULONGs...  For the operations that require a result array as
 * parameter, it must have the length cl+abs(dl). These functions should
 * probably end up in bn_asm.c as soon as there are assembler counterparts
 * for the systems that use assembler files.
 */

BN_ULONG bn_sub_part_words(BN_ULONG *r,
                           const BN_ULONG *a, const BN_ULONG *b,
                           int cl, int dl)
{
    BN_ULONG c, t;

    assert(cl >= 0);
    c = bn_sub_words(r, a, b, cl);

    if (dl == 0)
        return c;

    r += cl;
    a += cl;
    b += cl;

    if (dl < 0) {
# ifdef BN_COUNT
        fprintf(stderr, "  bn_sub_part_words %d + %d (dl < 0, c = %d)\n", cl,
                dl, c);
# endif
        for (;;) {
            t = b[0];
            r[0] = (0 - t - c) & BN_MASK2;
            if (t != 0)
                c = 1;
            if (++dl >= 0)
                break;

            t = b[1];
            r[1] = (0 - t - c) & BN_MASK2;
            if (t != 0)
                c = 1;
            if (++dl >= 0)
                break;

            t = b[2];
            r[2] = (0 - t - c) & BN_MASK2;
            if (t != 0)
                c = 1;
            if (++dl >= 0)
                break;

            t = b[3];
            r[3] = (0 - t - c) & BN_MASK2;
            if (t != 0)
                c = 1;
            if (++dl >= 0)
                break;

            b += 4;
            r += 4;
        }
    } else {
        int save_dl = dl;
# ifdef BN_COUNT
        fprintf(stderr, "  bn_sub_part_words %d + %d (dl > 0, c = %d)\n", cl,
                dl, c);
# endif
        while (c) {
            t = a[0];
            r[0] = (t - c) & BN_MASK2;
            if (t != 0)
                c = 0;
            if (--dl <= 0)
                break;

            t = a[1];
            r[1] = (t - c) & BN_MASK2;
            if (t != 0)
                c = 0;
            if (--dl <= 0)
                break;

            t = a[2];
            r[2] = (t - c) & BN_MASK2;
            if (t != 0)
                c = 0;
            if (--dl <= 0)
                break;

            t = a[3];
            r[3] = (t - c) & BN_MASK2;
            if (t != 0)
                c = 0;
            if (--dl <= 0)
                break;

            save_dl = dl;
            a += 4;
            r += 4;
        }
        if (dl > 0) {
# ifdef BN_COUNT
            fprintf(stderr, "  bn_sub_part_words %d + %d (dl > 0, c == 0)\n",
                    cl, dl);
# endif
            if (save_dl > dl) {
                switch (save_dl - dl) {
                case 1:
                    r[1] = a[1];
                    if (--dl <= 0)
                        break;
                case 2:
                    r[2] = a[2];
                    if (--dl <= 0)
                        break;
                case 3:
                    r[3] = a[3];
                    if (--dl <= 0)
                        break;
                }
                a += 4;
                r += 4;
            }
        }
        if (dl > 0) {
# ifdef BN_COUNT
            fprintf(stderr, "  bn_sub_part_words %d + %d (dl > 0, copy)\n",
                    cl, dl);
# endif
            for (;;) {
                r[0] = a[0];
                if (--dl <= 0)
                    break;
                r[1] = a[1];
                if (--dl <= 0)
                    break;
                r[2] = a[2];
                if (--dl <= 0)
                    break;
                r[3] = a[3];
                if (--dl <= 0)
                    break;

                a += 4;
                r += 4;
            }
        }
    }
    return c;
}
#endif

BN_ULONG bn_add_part_words(BN_ULONG *r,
                           const BN_ULONG *a, const BN_ULONG *b,
                           int cl, int dl)
{
    BN_ULONG c, l, t;

    assert(cl >= 0);
    c = bn_add_words(r, a, b, cl);

    if (dl == 0)
        return c;

    r += cl;
    a += cl;
    b += cl;

    if (dl < 0) {
        int save_dl = dl;
#ifdef BN_COUNT
        fprintf(stderr, "  bn_add_part_words %d + %d (dl < 0, c = %d)\n", cl,
                dl, c);
#endif
        while (c) {
            l = (c + b[0]) & BN_MASK2;
            c = (l < c);
            r[0] = l;
            if (++dl >= 0)
                break;

            l = (c + b[1]) & BN_MASK2;
            c = (l < c);
            r[1] = l;
            if (++dl >= 0)
                break;

            l = (c + b[2]) & BN_MASK2;
            c = (l < c);
            r[2] = l;
            if (++dl >= 0)
                break;

            l = (c + b[3]) & BN_MASK2;
            c = (l < c);
            r[3] = l;
            if (++dl >= 0)
                break;

            save_dl = dl;
            b += 4;
            r += 4;
        }
        if (dl < 0) {
#ifdef BN_COUNT
            fprintf(stderr, "  bn_add_part_words %d + %d (dl < 0, c == 0)\n",
                    cl, dl);
#endif
            if (save_dl < dl) {
                switch (dl - save_dl) {
                case 1:
                    r[1] = b[1];
                    if (++dl >= 0)
                        break;
                case 2:
                    r[2] = b[2];
                    if (++dl >= 0)
                        break;
                case 3:
                    r[3] = b[3];
                    if (++dl >= 0)
                        break;
                }
                b += 4;
                r += 4;
            }
        }
        if (dl < 0) {
#ifdef BN_COUNT
            fprintf(stderr, "  bn_add_part_words %d + %d (dl < 0, copy)\n",
                    cl, dl);
#endif
            for (;;) {
                r[0] = b[0];
                if (++dl >= 0)
                    break;
                r[1] = b[1];
                if (++dl >= 0)
                    break;
                r[2] = b[2];
                if (++dl >= 0)
                    break;
                r[3] = b[3];
                if (++dl >= 0)
                    break;

                b += 4;
                r += 4;
            }
        }
    } else {
        int save_dl = dl;
#ifdef BN_COUNT
        fprintf(stderr, "  bn_add_part_words %d + %d (dl > 0)\n", cl, dl);
#endif
        while (c) {
            t = (a[0] + c) & BN_MASK2;
            c = (t < c);
            r[0] = t;
            if (--dl <= 0)
                break;

            t = (a[1] + c) & BN_MASK2;
            c = (t < c);
            r[1] = t;
            if (--dl <= 0)
                break;

            t = (a[2] + c) & BN_MASK2;
            c = (t < c);
            r[2] = t;
            if (--dl <= 0)
                break;

            t = (a[3] + c) & BN_MASK2;
            c = (t < c);
            r[3] = t;
            if (--dl <= 0)
                break;

            save_dl = dl;
            a += 4;
            r += 4;
        }
#ifdef BN_COUNT
        fprintf(stderr, "  bn_add_part_words %d + %d (dl > 0, c == 0)\n", cl,
                dl);
#endif
        if (dl > 0) {
            if (save_dl > dl) {
                switch (save_dl - dl) {
                case 1:
                    r[1] = a[1];
                    if (--dl <= 0)
                        break;
                case 2:
                    r[2] = a[2];
                    if (--dl <= 0)
                        break;
                case 3:
                    r[3] = a[3];
                    if (--dl <= 0)
                        break;
                }
                a += 4;
                r += 4;
            }
        }
        if (dl > 0) {
#ifdef BN_COUNT
            fprintf(stderr, "  bn_add_part_words %d + %d (dl > 0, copy)\n",
                    cl, dl);
#endif
            for (;;) {
                r[0] = a[0];
                if (--dl <= 0)
                    break;
                r[1] = a[1];
                if (--dl <= 0)
                    break;
                r[2] = a[2];
                if (--dl <= 0)
                    break;
                r[3] = a[3];
                if (--dl <= 0)
                    break;

                a += 4;
                r += 4;
            }
        }
    }
    return c;
}

#ifdef BN_RECURSION
/*
 * Karatsuba recursive multiplication algorithm (cf. Knuth, The Art of
 * Computer Programming, Vol. 2)
 */

/*-
 * r is 2*n2 words in size,
 * a and b are both n2 words in size.
 * n2 must be a power of 2.
 * We multiply and return the result.
 * t must be 2*n2 words in size
 * We calculate
 * a[0]*b[0]
 * a[0]*b[0]+a[1]*b[1]+(a[0]-a[1])*(b[1]-b[0])
 * a[1]*b[1]
 */
/* dnX may not be positive, but n2/2+dnX has to be */
void bn_mul_recursive(BN_ULONG *r, BN_ULONG *a, BN_ULONG *b, int n2,
                      int dna, int dnb, BN_ULONG *t)
{
    int n = n2 / 2, c1, c2;
    int tna = n + dna, tnb = n + dnb;
    unsigned int neg, zero;
    BN_ULONG ln, lo, *p;

# ifdef BN_COUNT
    fprintf(stderr, " bn_mul_recursive %d%+d * %d%+d\n", n2, dna, n2, dnb);
# endif
# ifdef BN_MUL_COMBA
#  if 0
    if (n2 == 4) {
        bn_mul_comba4(r, a, b);
        return;
    }
#  endif
    /*
     * Only call bn_mul_comba 8 if n2 == 8 and the two arrays are complete
     * [steve]
     */
    if (n2 == 8 && dna == 0 && dnb == 0) {
        bn_mul_comba8(r, a, b);
        return;
    }
# endif                         /* BN_MUL_COMBA */
    /* Else do normal multiply */
    if (n2 < BN_MUL_RECURSIVE_SIZE_NORMAL) {
        bn_mul_normal(r, a, n2 + dna, b, n2 + dnb);
        if ((dna + dnb) < 0)
            memset(&r[2 * n2 + dna + dnb], 0,
                   sizeof(BN_ULONG) * -(dna + dnb));
        return;
    }
    /* r=(a[0]-a[1])*(b[1]-b[0]) */
    c1 = bn_cmp_part_words(a, &(a[n]), tna, n - tna);
    c2 = bn_cmp_part_words(&(b[n]), b, tnb, tnb - n);
    zero = neg = 0;
    switch (c1 * 3 + c2) {
    case -4:
        bn_sub_part_words(t, &(a[n]), a, tna, tna - n); /* - */
        bn_sub_part_words(&(t[n]), b, &(b[n]), tnb, n - tnb); /* - */
        break;
    case -3:
        zero = 1;
        break;
    case -2:
        bn_sub_part_words(t, &(a[n]), a, tna, tna - n); /* - */
        bn_sub_part_words(&(t[n]), &(b[n]), b, tnb, tnb - n); /* + */
        neg = 1;
        break;
    case -1:
    case 0:
    case 1:
        zero = 1;
        break;
    case 2:
        bn_sub_part_words(t, a, &(a[n]), tna, n - tna); /* + */
        bn_sub_part_words(&(t[n]), b, &(b[n]), tnb, n - tnb); /* - */
        neg = 1;
        break;
    case 3:
        zero = 1;
        break;
    case 4:
        bn_sub_part_words(t, a, &(a[n]), tna, n - tna);
        bn_sub_part_words(&(t[n]), &(b[n]), b, tnb, tnb - n);
        break;
    }

# ifdef BN_MUL_COMBA
    if (n == 4 && dna == 0 && dnb == 0) { /* XXX: bn_mul_comba4 could take
                                           * extra args to do this well */
        if (!zero)
            bn_mul_comba4(&(t[n2]), t, &(t[n]));
        else
            memset(&(t[n2]), 0, 8 * sizeof(BN_ULONG));

        bn_mul_comba4(r, a, b);
        bn_mul_comba4(&(r[n2]), &(a[n]), &(b[n]));
    } else if (n == 8 && dna == 0 && dnb == 0) { /* XXX: bn_mul_comba8 could
                                                  * take extra args to do
                                                  * this well */
        if (!zero)
            bn_mul_comba8(&(t[n2]), t, &(t[n]));
        else
            memset(&(t[n2]), 0, 16 * sizeof(BN_ULONG));

        bn_mul_comba8(r, a, b);
        bn_mul_comba8(&(r[n2]), &(a[n]), &(b[n]));
    } else
# endif                         /* BN_MUL_COMBA */
    {
        p = &(t[n2 * 2]);
        if (!zero)
            bn_mul_recursive(&(t[n2]), t, &(t[n]), n, 0, 0, p);
        else
            memset(&(t[n2]), 0, n2 * sizeof(BN_ULONG));
        bn_mul_recursive(r, a, b, n, 0, 0, p);
        bn_mul_recursive(&(r[n2]), &(a[n]), &(b[n]), n, dna, dnb, p);
    }

    /*-
     * t[32] holds (a[0]-a[1])*(b[1]-b[0]), c1 is the sign
     * r[10] holds (a[0]*b[0])
     * r[32] holds (b[1]*b[1])
     */

    c1 = (int)(bn_add_words(t, r, &(r[n2]), n2));

    if (neg) {                  /* if t[32] is negative */
        c1 -= (int)(bn_sub_words(&(t[n2]), t, &(t[n2]), n2));
    } else {
        /* Might have a carry */
        c1 += (int)(bn_add_words(&(t[n2]), &(t[n2]), t, n2));
    }

    /*-
     * t[32] holds (a[0]-a[1])*(b[1]-b[0])+(a[0]*b[0])+(a[1]*b[1])
     * r[10] holds (a[0]*b[0])
     * r[32] holds (b[1]*b[1])
     * c1 holds the carry bits
     */
    c1 += (int)(bn_add_words(&(r[n]), &(r[n]), &(t[n2]), n2));
    if (c1) {
        p = &(r[n + n2]);
        lo = *p;
        ln = (lo + c1) & BN_MASK2;
        *p = ln;

        /*
         * The overflow will stop before we over write words we should not
         * overwrite
         */
        if (ln < (BN_ULONG)c1) {
            do {
                p++;
                lo = *p;
                ln = (lo + 1) & BN_MASK2;
                *p = ln;
            } while (ln == 0);
        }
    }
}

/*
 * n+tn is the word length t needs to be n*4 is size, as does r
 */
/* tnX may not be negative but less than n */
void bn_mul_part_recursive(BN_ULONG *r, BN_ULONG *a, BN_ULONG *b, int n,
                           int tna, int tnb, BN_ULONG *t)
{
    int i, j, n2 = n * 2;
    int c1, c2, neg;
    BN_ULONG ln, lo, *p;

# ifdef BN_COUNT
    fprintf(stderr, " bn_mul_part_recursive (%d%+d) * (%d%+d)\n",
            n, tna, n, tnb);
# endif
    if (n < 8) {
        bn_mul_normal(r, a, n + tna, b, n + tnb);
        return;
    }

    /* r=(a[0]-a[1])*(b[1]-b[0]) */
    c1 = bn_cmp_part_words(a, &(a[n]), tna, n - tna);
    c2 = bn_cmp_part_words(&(b[n]), b, tnb, tnb - n);
    neg = 0;
    switch (c1 * 3 + c2) {
    case -4:
        bn_sub_part_words(t, &(a[n]), a, tna, tna - n); /* - */
        bn_sub_part_words(&(t[n]), b, &(b[n]), tnb, n - tnb); /* - */
        break;
    case -3:
        /* break; */
    case -2:
        bn_sub_part_words(t, &(a[n]), a, tna, tna - n); /* - */
        bn_sub_part_words(&(t[n]), &(b[n]), b, tnb, tnb - n); /* + */
        neg = 1;
        break;
    case -1:
    case 0:
    case 1:
        /* break; */
    case 2:
        bn_sub_part_words(t, a, &(a[n]), tna, n - tna); /* + */
        bn_sub_part_words(&(t[n]), b, &(b[n]), tnb, n - tnb); /* - */
        neg = 1;
        break;
    case 3:
        /* break; */
    case 4:
        bn_sub_part_words(t, a, &(a[n]), tna, n - tna);
        bn_sub_part_words(&(t[n]), &(b[n]), b, tnb, tnb - n);
        break;
    }
    /*
     * The zero case isn't yet implemented here. The speedup would probably
     * be negligible.
     */
# if 0
    if (n == 4) {
        bn_mul_comba4(&(t[n2]), t, &(t[n]));
        bn_mul_comba4(r, a, b);
        bn_mul_normal(&(r[n2]), &(a[n]), tn, &(b[n]), tn);
        memset(&(r[n2 + tn * 2]), 0, sizeof(BN_ULONG) * (n2 - tn * 2));
    } else
# endif
    if (n == 8) {
        bn_mul_comba8(&(t[n2]), t, &(t[n]));
        bn_mul_comba8(r, a, b);
        bn_mul_normal(&(r[n2]), &(a[n]), tna, &(b[n]), tnb);
        memset(&(r[n2 + tna + tnb]), 0, sizeof(BN_ULONG) * (n2 - tna - tnb));
    } else {
        p = &(t[n2 * 2]);
        bn_mul_recursive(&(t[n2]), t, &(t[n]), n, 0, 0, p);
        bn_mul_recursive(r, a, b, n, 0, 0, p);
        i = n / 2;
        /*
         * If there is only a bottom half to the number, just do it
         */
        if (tna > tnb)
            j = tna - i;
        else
            j = tnb - i;
        if (j == 0) {
            bn_mul_recursive(&(r[n2]), &(a[n]), &(b[n]),
                             i, tna - i, tnb - i, p);
            memset(&(r[n2 + i * 2]), 0, sizeof(BN_ULONG) * (n2 - i * 2));
        } else if (j > 0) {     /* eg, n == 16, i == 8 and tn == 11 */
            bn_mul_part_recursive(&(r[n2]), &(a[n]), &(b[n]),
                                  i, tna - i, tnb - i, p);
            memset(&(r[n2 + tna + tnb]), 0,
                   sizeof(BN_ULONG) * (n2 - tna - tnb));
        } else {                /* (j < 0) eg, n == 16, i == 8 and tn == 5 */

            memset(&(r[n2]), 0, sizeof(BN_ULONG) * n2);
            if (tna < BN_MUL_RECURSIVE_SIZE_NORMAL
                && tnb < BN_MUL_RECURSIVE_SIZE_NORMAL) {
                bn_mul_normal(&(r[n2]), &(a[n]), tna, &(b[n]), tnb);
            } else {
                for (;;) {
                    i /= 2;
                    /*
                     * these simplified conditions work exclusively because
                     * difference between tna and tnb is 1 or 0
                     */
                    if (i < tna || i < tnb) {
                        bn_mul_part_recursive(&(r[n2]),
                                              &(a[n]), &(b[n]),
                                              i, tna - i, tnb - i, p);
                        break;
                    } else if (i == tna || i == tnb) {
                        bn_mul_recursive(&(r[n2]),
                                         &(a[n]), &(b[n]),
                                         i, tna - i, tnb - i, p);
                        break;
                    }
                }
            }
        }
    }

    /*-
     * t[32] holds (a[0]-a[1])*(b[1]-b[0]), c1 is the sign
     * r[10] holds (a[0]*b[0])
     * r[32] holds (b[1]*b[1])
     */

    c1 = (int)(bn_add_words(t, r, &(r[n2]), n2));

    if (neg) {                  /* if t[32] is negative */
        c1 -= (int)(bn_sub_words(&(t[n2]), t, &(t[n2]), n2));
    } else {
        /* Might have a carry */
        c1 += (int)(bn_add_words(&(t[n2]), &(t[n2]), t, n2));
    }

    /*-
     * t[32] holds (a[0]-a[1])*(b[1]-b[0])+(a[0]*b[0])+(a[1]*b[1])
     * r[10] holds (a[0]*b[0])
     * r[32] holds (b[1]*b[1])
     * c1 holds the carry bits
     */
    c1 += (int)(bn_add_words(&(r[n]), &(r[n]), &(t[n2]), n2));
    if (c1) {
        p = &(r[n + n2]);
        lo = *p;
        ln = (lo + c1) & BN_MASK2;
        *p = ln;

        /*
         * The overflow will stop before we over write words we should not
         * overwrite
         */
        if (ln < (BN_ULONG)c1) {
            do {
                p++;
                lo = *p;
                ln = (lo + 1) & BN_MASK2;
                *p = ln;
            } while (ln == 0);
        }
    }
}

/*-
 * a and b must be the same size, which is n2.
 * r needs to be n2 words and t needs to be n2*2
 */
void bn_mul_low_recursive(BN_ULONG *r, BN_ULONG *a, BN_ULONG *b, int n2,
                          BN_ULONG *t)
{
    int n = n2 / 2;

# ifdef BN_COUNT
    fprintf(stderr, " bn_mul_low_recursive %d * %d\n", n2, n2);
# endif

    bn_mul_recursive(r, a, b, n, 0, 0, &(t[0]));
    if (n >= BN_MUL_LOW_RECURSIVE_SIZE_NORMAL) {
        bn_mul_low_recursive(&(t[0]), &(a[0]), &(b[n]), n, &(t[n2]));
        bn_add_words(&(r[n]), &(r[n]), &(t[0]), n);
        bn_mul_low_recursive(&(t[0]), &(a[n]), &(b[0]), n, &(t[n2]));
        bn_add_words(&(r[n]), &(r[n]), &(t[0]), n);
    } else {
        bn_mul_low_normal(&(t[0]), &(a[0]), &(b[n]), n);
        bn_mul_low_normal(&(t[n]), &(a[n]), &(b[0]), n);
        bn_add_words(&(r[n]), &(r[n]), &(t[0]), n);
        bn_add_words(&(r[n]), &(r[n]), &(t[n]), n);
    }
}

/*-
 * a and b must be the same size, which is n2.
 * r needs to be n2 words and t needs to be n2*2
 * l is the low words of the output.
 * t needs to be n2*3
 */
void bn_mul_high(BN_ULONG *r, BN_ULONG *a, BN_ULONG *b, BN_ULONG *l, int n2,
                 BN_ULONG *t)
{
    int i, n;
    int c1, c2;
    int neg, oneg, zero;
    BN_ULONG ll, lc, *lp, *mp;

# ifdef BN_COUNT
    fprintf(stderr, " bn_mul_high %d * %d\n", n2, n2);
# endif
    n = n2 / 2;

    /* Calculate (al-ah)*(bh-bl) */
    neg = zero = 0;
    c1 = bn_cmp_words(&(a[0]), &(a[n]), n);
    c2 = bn_cmp_words(&(b[n]), &(b[0]), n);
    switch (c1 * 3 + c2) {
    case -4:
        bn_sub_words(&(r[0]), &(a[n]), &(a[0]), n);
        bn_sub_words(&(r[n]), &(b[0]), &(b[n]), n);
        break;
    case -3:
        zero = 1;
        break;
    case -2:
        bn_sub_words(&(r[0]), &(a[n]), &(a[0]), n);
        bn_sub_words(&(r[n]), &(b[n]), &(b[0]), n);
        neg = 1;
        break;
    case -1:
    case 0:
    case 1:
        zero = 1;
        break;
    case 2:
        bn_sub_words(&(r[0]), &(a[0]), &(a[n]), n);
        bn_sub_words(&(r[n]), &(b[0]), &(b[n]), n);
        neg = 1;
        break;
    case 3:
        zero = 1;
        break;
    case 4:
        bn_sub_words(&(r[0]), &(a[0]), &(a[n]), n);
        bn_sub_words(&(r[n]), &(b[n]), &(b[0]), n);
        break;
    }

    oneg = neg;
    /* t[10] = (a[0]-a[1])*(b[1]-b[0]) */
    /* r[10] = (a[1]*b[1]) */
# ifdef BN_MUL_COMBA
    if (n == 8) {
        bn_mul_comba8(&(t[0]), &(r[0]), &(r[n]));
        bn_mul_comba8(r, &(a[n]), &(b[n]));
    } else
# endif
    {
        bn_mul_recursive(&(t[0]), &(r[0]), &(r[n]), n, 0, 0, &(t[n2]));
        bn_mul_recursive(r, &(a[n]), &(b[n]), n, 0, 0, &(t[n2]));
    }

    /*-
     * s0 == low(al*bl)
     * s1 == low(ah*bh)+low((al-ah)*(bh-bl))+low(al*bl)+high(al*bl)
     * We know s0 and s1 so the only unknown is high(al*bl)
     * high(al*bl) == s1 - low(ah*bh+s0+(al-ah)*(bh-bl))
     * high(al*bl) == s1 - (r[0]+l[0]+t[0])
     */
    if (l != NULL) {
        lp = &(t[n2 + n]);
        c1 = (int)(bn_add_words(lp, &(r[0]), &(l[0]), n));
    } else {
        c1 = 0;
        lp = &(r[0]);
    }

    if (neg)
        neg = (int)(bn_sub_words(&(t[n2]), lp, &(t[0]), n));
    else {
        bn_add_words(&(t[n2]), lp, &(t[0]), n);
        neg = 0;
    }

    if (l != NULL) {
        bn_sub_words(&(t[n2 + n]), &(l[n]), &(t[n2]), n);
    } else {
        lp = &(t[n2 + n]);
        mp = &(t[n2]);
        for (i = 0; i < n; i++)
            lp[i] = ((~mp[i]) + 1) & BN_MASK2;
    }

    /*-
     * s[0] = low(al*bl)
     * t[3] = high(al*bl)
     * t[10] = (a[0]-a[1])*(b[1]-b[0]) neg is the sign
     * r[10] = (a[1]*b[1])
     */
    /*-
     * R[10] = al*bl
     * R[21] = al*bl + ah*bh + (a[0]-a[1])*(b[1]-b[0])
     * R[32] = ah*bh
     */
    /*-
     * R[1]=t[3]+l[0]+r[0](+-)t[0] (have carry/borrow)
     * R[2]=r[0]+t[3]+r[1](+-)t[1] (have carry/borrow)
     * R[3]=r[1]+(carry/borrow)
     */
    if (l != NULL) {
        lp = &(t[n2]);
        c1 = (int)(bn_add_words(lp, &(t[n2 + n]), &(l[0]), n));
    } else {
        lp = &(t[n2 + n]);
        c1 = 0;
    }
    c1 += (int)(bn_add_words(&(t[n2]), lp, &(r[0]), n));
    if (oneg)
        c1 -= (int)(bn_sub_words(&(t[n2]), &(t[n2]), &(t[0]), n));
    else
        c1 += (int)(bn_add_words(&(t[n2]), &(t[n2]), &(t[0]), n));

    c2 = (int)(bn_add_words(&(r[0]), &(r[0]), &(t[n2 + n]), n));
    c2 += (int)(bn_add_words(&(r[0]), &(r[0]), &(r[n]), n));
    if (oneg)
        c2 -= (int)(bn_sub_words(&(r[0]), &(r[0]), &(t[n]), n));
    else
        c2 += (int)(bn_add_words(&(r[0]), &(r[0]), &(t[n]), n));

    if (c1 != 0) {              /* Add starting at r[0], could be +ve or -ve */
        i = 0;
        if (c1 > 0) {
            lc = c1;
            do {
                ll = (r[i] + lc) & BN_MASK2;
                r[i++] = ll;
                lc = (lc > ll);
            } while (lc);
        } else {
            lc = -c1;
            do {
                ll = r[i];
                r[i++] = (ll - lc) & BN_MASK2;
                lc = (lc > ll);
            } while (lc);
        }
    }
    if (c2 != 0) {              /* Add starting at r[1] */
        i = n;
        if (c2 > 0) {
            lc = c2;
            do {
                ll = (r[i] + lc) & BN_MASK2;
                r[i++] = ll;
                lc = (lc > ll);
            } while (lc);
        } else {
            lc = -c2;
            do {
                ll = r[i];
                r[i++] = (ll - lc) & BN_MASK2;
                lc = (lc > ll);
            } while (lc);
        }
    }
}
#endif                          /* BN_RECURSION */

int BN_mul(BIGNUM *r, const BIGNUM *a, const BIGNUM *b, BN_CTX *ctx)
{
    int ret = 0;
    int top, al, bl;
    BIGNUM *rr;
#if defined(BN_MUL_COMBA) || defined(BN_RECURSION)
    int i;
#endif
#ifdef BN_RECURSION
    BIGNUM *t = NULL;
    int j = 0, k;
#endif

#ifdef BN_COUNT
    fprintf(stderr, "BN_mul %d * %d\n", a->top, b->top);
#endif

    bn_check_top(a);
    bn_check_top(b);
    bn_check_top(r);

    al = a->top;
    bl = b->top;

    if ((al == 0) || (bl == 0)) {
        BN_zero(r);
        return (1);
    }
    top = al + bl;

    BN_CTX_start(ctx);
    if ((r == a) || (r == b)) {
        if ((rr = BN_CTX_get(ctx)) == NULL)
            goto err;
    } else
        rr = r;
    rr->neg = a->neg ^ b->neg;

#if defined(BN_MUL_COMBA) || defined(BN_RECURSION)
    i = al - bl;
#endif
#ifdef BN_MUL_COMBA
    if (i == 0) {
# if 0
        if (al == 4) {
            if (bn_wexpand(rr, 8) == NULL)
                goto err;
            rr->top = 8;
            bn_mul_comba4(rr->d, a->d, b->d);
            goto end;
        }
# endif
        if (al == 8) {
            if (bn_wexpand(rr, 16) == NULL)
                goto err;
            rr->top = 16;
            bn_mul_comba8(rr->d, a->d, b->d);
            goto end;
        }
    }
#endif                          /* BN_MUL_COMBA */
#ifdef BN_RECURSION
    if ((al >= BN_MULL_SIZE_NORMAL) && (bl >= BN_MULL_SIZE_NORMAL)) {
        if (i >= -1 && i <= 1) {
            /*
             * Find out the power of two lower or equal to the longest of the
             * two numbers
             */
            if (i >= 0) {
                j = BN_num_bits_word((BN_ULONG)al);
            }
            if (i == -1) {
                j = BN_num_bits_word((BN_ULONG)bl);
            }
            j = 1 << (j - 1);
            assert(j <= al || j <= bl);
            k = j + j;
            t = BN_CTX_get(ctx);
            if (t == NULL)
                goto err;
            if (al > j || bl > j) {
                if (bn_wexpand(t, k * 4) == NULL)
                    goto err;
                if (bn_wexpand(rr, k * 4) == NULL)
                    goto err;
                bn_mul_part_recursive(rr->d, a->d, b->d,
                                      j, al - j, bl - j, t->d);
            } else {            /* al <= j || bl <= j */

                if (bn_wexpand(t, k * 2) == NULL)
                    goto err;
                if (bn_wexpand(rr, k * 2) == NULL)
                    goto err;
                bn_mul_recursive(rr->d, a->d, b->d, j, al - j, bl - j, t->d);
            }
            rr->top = top;
            goto end;
        }
# if 0
        if (i == 1 && !BN_get_flags(b, BN_FLG_STATIC_DATA)) {
            BIGNUM *tmp_bn = (BIGNUM *)b;
            if (bn_wexpand(tmp_bn, al) == NULL)
                goto err;
            tmp_bn->d[bl] = 0;
            bl++;
            i--;
        } else if (i == -1 && !BN_get_flags(a, BN_FLG_STATIC_DATA)) {
            BIGNUM *tmp_bn = (BIGNUM *)a;
            if (bn_wexpand(tmp_bn, bl) == NULL)
                goto err;
            tmp_bn->d[al] = 0;
            al++;
            i++;
        }
        if (i == 0) {
            /* symmetric and > 4 */
            /* 16 or larger */
            j = BN_num_bits_word((BN_ULONG)al);
            j = 1 << (j - 1);
            k = j + j;
            t = BN_CTX_get(ctx);
            if (al == j) {      /* exact multiple */
                if (bn_wexpand(t, k * 2) == NULL)
                    goto err;
                if (bn_wexpand(rr, k * 2) == NULL)
                    goto err;
                bn_mul_recursive(rr->d, a->d, b->d, al, t->d);
            } else {
                if (bn_wexpand(t, k * 4) == NULL)
                    goto err;
                if (bn_wexpand(rr, k * 4) == NULL)
                    goto err;
                bn_mul_part_recursive(rr->d, a->d, b->d, al - j, j, t->d);
            }
            rr->top = top;
            goto end;
        }
# endif
    }
#endif                          /* BN_RECURSION */
    if (bn_wexpand(rr, top) == NULL)
        goto err;
    rr->top = top;
    bn_mul_normal(rr->d, a->d, al, b->d, bl);

#if defined(BN_MUL_COMBA) || defined(BN_RECURSION)
 end:
#endif
    bn_correct_top(rr);
    if (r != rr && BN_copy(r, rr) == NULL)
        goto err;

    ret = 1;
 err:
    bn_check_top(r);
    BN_CTX_end(ctx);
    return (ret);
}

void bn_mul_normal(BN_ULONG *r, BN_ULONG *a, int na, BN_ULONG *b, int nb)
{
    BN_ULONG *rr;

#ifdef BN_COUNT
    fprintf(stderr, " bn_mul_normal %d * %d\n", na, nb);
#endif

    if (na < nb) {
        int itmp;
        BN_ULONG *ltmp;

        itmp = na;
        na = nb;
        nb = itmp;
        ltmp = a;
        a = b;
        b = ltmp;

    }
    rr = &(r[na]);
    if (nb <= 0) {
        (void)bn_mul_words(r, a, na, 0);
        return;
    } else
        rr[0] = bn_mul_words(r, a, na, b[0]);

    for (;;) {
        if (--nb <= 0)
            return;
        rr[1] = bn_mul_add_words(&(r[1]), a, na, b[1]);
        if (--nb <= 0)
            return;
        rr[2] = bn_mul_add_words(&(r[2]), a, na, b[2]);
        if (--nb <= 0)
            return;
        rr[3] = bn_mul_add_words(&(r[3]), a, na, b[3]);
        if (--nb <= 0)
            return;
        rr[4] = bn_mul_add_words(&(r[4]), a, na, b[4]);
        rr += 4;
        r += 4;
        b += 4;
    }
}

void bn_mul_low_normal(BN_ULONG *r, BN_ULONG *a, BN_ULONG *b, int n)
{
#ifdef BN_COUNT
    fprintf(stderr, " bn_mul_low_normal %d * %d\n", n, n);
#endif
    bn_mul_words(r, a, n, b[0]);

    for (;;) {
        if (--n <= 0)
            return;
        bn_mul_add_words(&(r[1]), a, n, b[1]);
        if (--n <= 0)
            return;
        bn_mul_add_words(&(r[2]), a, n, b[2]);
        if (--n <= 0)
            return;
        bn_mul_add_words(&(r[3]), a, n, b[3]);
        if (--n <= 0)
            return;
        bn_mul_add_words(&(r[4]), a, n, b[4]);
        r += 4;
        b += 4;
    }
}
