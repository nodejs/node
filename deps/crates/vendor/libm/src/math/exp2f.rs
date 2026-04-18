// origin: FreeBSD /usr/src/lib/msun/src/s_exp2f.c
//-
// Copyright (c) 2005 David Schultz <das@FreeBSD.ORG>
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions
// are met:
// 1. Redistributions of source code must retain the above copyright
//    notice, this list of conditions and the following disclaimer.
// 2. Redistributions in binary form must reproduce the above copyright
//    notice, this list of conditions and the following disclaimer in the
//    documentation and/or other materials provided with the distribution.
//
// THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
// ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
// FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
// DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
// OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
// HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
// LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
// OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
// SUCH DAMAGE.

const TBLSIZE: usize = 16;

static EXP2FT: [u64; TBLSIZE] = [
    0x3fe6a09e667f3bcd,
    0x3fe7a11473eb0187,
    0x3fe8ace5422aa0db,
    0x3fe9c49182a3f090,
    0x3feae89f995ad3ad,
    0x3fec199bdd85529c,
    0x3fed5818dcfba487,
    0x3feea4afa2a490da,
    0x3ff0000000000000,
    0x3ff0b5586cf9890f,
    0x3ff172b83c7d517b,
    0x3ff2387a6e756238,
    0x3ff306fe0a31b715,
    0x3ff3dea64c123422,
    0x3ff4bfdad5362a27,
    0x3ff5ab07dd485429,
];

// exp2f(x): compute the base 2 exponential of x
//
// Accuracy: Peak error < 0.501 ulp; location of peak: -0.030110927.
//
// Method: (equally-spaced tables)
//
//   Reduce x:
//     x = k + y, for integer k and |y| <= 1/2.
//     Thus we have exp2f(x) = 2**k * exp2(y).
//
//   Reduce y:
//     y = i/TBLSIZE + z for integer i near y * TBLSIZE.
//     Thus we have exp2(y) = exp2(i/TBLSIZE) * exp2(z),
//     with |z| <= 2**-(TBLSIZE+1).
//
//   We compute exp2(i/TBLSIZE) via table lookup and exp2(z) via a
//   degree-4 minimax polynomial with maximum error under 1.4 * 2**-33.
//   Using double precision for everything except the reduction makes
//   roundoff error insignificant and simplifies the scaling step.
//
//   This method is due to Tang, but I do not use his suggested parameters:
//
//      Tang, P.  Table-driven Implementation of the Exponential Function
//      in IEEE Floating-Point Arithmetic.  TOMS 15(2), 144-157 (1989).

/// Exponential, base 2 (f32)
///
/// Calculate `2^x`, that is, 2 raised to the power `x`.
#[cfg_attr(all(test, assert_no_panic), no_panic::no_panic)]
pub fn exp2f(mut x: f32) -> f32 {
    let redux = f32::from_bits(0x4b400000) / TBLSIZE as f32;
    let p1 = f32::from_bits(0x3f317218);
    let p2 = f32::from_bits(0x3e75fdf0);
    let p3 = f32::from_bits(0x3d6359a4);
    let p4 = f32::from_bits(0x3c1d964e);

    // double_t t, r, z;
    // uint32_t ix, i0, k;

    let x1p127 = f32::from_bits(0x7f000000);

    /* Filter out exceptional cases. */
    let ui = f32::to_bits(x);
    let ix = ui & 0x7fffffff;
    if ix > 0x42fc0000 {
        /* |x| > 126 */
        if ix > 0x7f800000 {
            /* NaN */
            return x;
        }
        if (0x43000000..0x80000000).contains(&ui) {
            /* x >= 128 */
            x *= x1p127;
            return x;
        }
        if ui >= 0x80000000 {
            /* x < -126 */
            if ui >= 0xc3160000 || (ui & 0x0000ffff != 0) {
                force_eval!(f32::from_bits(0x80000001) / x);
            }
            if ui >= 0xc3160000 {
                /* x <= -150 */
                return 0.0;
            }
        }
    } else if ix <= 0x33000000 {
        /* |x| <= 0x1p-25 */
        return 1.0 + x;
    }

    /* Reduce x, computing z, i0, and k. */
    let ui = f32::to_bits(x + redux);
    let mut i0 = ui;
    i0 += TBLSIZE as u32 / 2;
    let k = i0 / TBLSIZE as u32;
    let ukf = f64::from_bits(((0x3ff + k) as u64) << 52);
    i0 &= TBLSIZE as u32 - 1;
    let mut uf = f32::from_bits(ui);
    uf -= redux;
    let z: f64 = (x - uf) as f64;
    /* Compute r = exp2(y) = exp2ft[i0] * p(z). */
    let r: f64 = f64::from_bits(i!(EXP2FT, i0 as usize));
    let t: f64 = r * z;
    let r: f64 = r + t * (p1 as f64 + z * p2 as f64) + t * (z * z) * (p3 as f64 + z * p4 as f64);

    /* Scale by 2**k */
    (r * ukf) as f32
}
