/* SPDX-License-Identifier: MIT */
/* origin: core-math/src/binary64/cbrt/cbrt.c
 * Copyright (c) 2021-2022 Alexei Sibidanov.
 * Ported to Rust in 2025 by Trevor Gross.
 */

use super::Float;
use super::support::{FpResult, Round, cold_path};

/// Compute the cube root of the argument.
#[cfg_attr(assert_no_panic, no_panic::no_panic)]
pub fn cbrt(x: f64) -> f64 {
    cbrt_round(x, Round::Nearest).val
}

pub fn cbrt_round(x: f64, round: Round) -> FpResult<f64> {
    const ESCALE: [f64; 3] = [
        1.0,
        hf64!("0x1.428a2f98d728bp+0"), /* 2^(1/3) */
        hf64!("0x1.965fea53d6e3dp+0"), /* 2^(2/3) */
    ];

    /* the polynomial c0+c1*x+c2*x^2+c3*x^3 approximates x^(1/3) on [1,2]
    with maximal error < 9.2e-5 (attained at x=2) */
    const C: [f64; 4] = [
        hf64!("0x1.1b0babccfef9cp-1"),
        hf64!("0x1.2c9a3e94d1da5p-1"),
        hf64!("-0x1.4dc30b1a1ddbap-3"),
        hf64!("0x1.7a8d3e4ec9b07p-6"),
    ];

    let u0: f64 = hf64!("0x1.5555555555555p-2");
    let u1: f64 = hf64!("0x1.c71c71c71c71cp-3");

    let rsc = [1.0, -1.0, 0.5, -0.5, 0.25, -0.25];

    let off = [hf64!("0x1p-53"), 0.0, 0.0, 0.0];

    /* rm=0 for rounding to nearest, and other values for directed roundings */
    let hx: u64 = x.to_bits();
    let mut mant: u64 = hx & f64::SIG_MASK;
    let sign: u64 = hx >> 63;

    let mut e: u32 = (hx >> f64::SIG_BITS) as u32 & f64::EXP_SAT;

    if ((e + 1) & f64::EXP_SAT) < 2 {
        cold_path();

        let ix: u64 = hx & !f64::SIGN_MASK;

        /* 0, inf, nan: we return x + x instead of simply x,
        to that for x a signaling NaN, it correctly triggers
        the invalid exception. */
        if e == f64::EXP_SAT || ix == 0 {
            return FpResult::ok(x + x);
        }

        let nz = ix.leading_zeros() - 11; /* subnormal */
        mant <<= nz;
        mant &= f64::SIG_MASK;
        e = e.wrapping_sub(nz - 1);
    }

    e = e.wrapping_add(3072);
    let cvt1: u64 = mant | (0x3ffu64 << 52);
    let mut cvt5: u64 = cvt1;

    let et: u32 = e / 3;
    let it: u32 = e % 3;

    /* 2^(3k+it) <= x < 2^(3k+it+1), with 0 <= it <= 3 */
    cvt5 += u64::from(it) << f64::SIG_BITS;
    cvt5 |= sign << 63;
    let zz: f64 = f64::from_bits(cvt5);

    /* cbrt(x) = cbrt(zz)*2^(et-1365) where 1 <= zz < 8 */
    let mut isc: u64 = ESCALE[it as usize].to_bits(); // todo: index
    isc |= sign << 63;
    let cvt2: u64 = isc;
    let z: f64 = f64::from_bits(cvt1);

    /* cbrt(zz) = cbrt(z)*isc, where isc encodes 1, 2^(1/3) or 2^(2/3),
    and 1 <= z < 2 */
    let r: f64 = 1.0 / z;
    let rr: f64 = r * rsc[((it as usize) << 1) | sign as usize];
    let z2: f64 = z * z;
    let c0: f64 = C[0] + z * C[1];
    let c2: f64 = C[2] + z * C[3];
    let mut y: f64 = c0 + z2 * c2;
    let mut y2: f64 = y * y;

    /* y is an approximation of z^(1/3) */
    let mut h: f64 = y2 * (y * r) - 1.0;

    /* h determines the error between y and z^(1/3) */
    y -= (h * y) * (u0 - u1 * h);

    /* The correction y -= (h*y)*(u0 - u1*h) corresponds to a cubic variant
    of Newton's method, with the function f(y) = 1-z/y^3. */
    y *= f64::from_bits(cvt2);

    /* Now y is an approximation of zz^(1/3),
     * and rr an approximation of 1/zz. We now perform another iteration of
     * Newton-Raphson, this time with a linear approximation only. */
    y2 = y * y;
    let mut y2l: f64 = y.fma(y, -y2);

    /* y2 + y2l = y^2 exactly */
    let mut y3: f64 = y2 * y;
    let mut y3l: f64 = y.fma(y2, -y3) + y * y2l;

    /* y3 + y3l approximates y^3 with about 106 bits of accuracy */
    h = ((y3 - zz) + y3l) * rr;
    let mut dy: f64 = h * (y * u0);

    /* the approximation of zz^(1/3) is y - dy */
    let mut y1: f64 = y - dy;
    dy = (y - y1) - dy;

    /* the approximation of zz^(1/3) is now y1 + dy, where |dy| < 1/2 ulp(y)
     * (for rounding to nearest) */
    let mut ady: f64 = dy.abs();

    /* For directed roundings, ady0 is tiny when dy is tiny, or ady0 is near
     * from ulp(1);
     * for rounding to nearest, ady0 is tiny when dy is near from 1/2 ulp(1),
     * or from 3/2 ulp(1). */
    let mut ady0: f64 = (ady - off[round as usize]).abs();
    let mut ady1: f64 = (ady - (hf64!("0x1p-52") + off[round as usize])).abs();

    if ady0 < hf64!("0x1p-75") || ady1 < hf64!("0x1p-75") {
        cold_path();

        y2 = y1 * y1;
        y2l = y1.fma(y1, -y2);
        y3 = y2 * y1;
        y3l = y1.fma(y2, -y3) + y1 * y2l;
        h = ((y3 - zz) + y3l) * rr;
        dy = h * (y1 * u0);
        y = y1 - dy;
        dy = (y1 - y) - dy;
        y1 = y;
        ady = dy.abs();
        ady0 = (ady - off[round as usize]).abs();
        ady1 = (ady - (hf64!("0x1p-52") + off[round as usize])).abs();

        if ady0 < hf64!("0x1p-98") || ady1 < hf64!("0x1p-98") {
            cold_path();
            let azz: f64 = zz.abs();

            // ~ 0x1.79d15d0e8d59b80000000000000ffc3dp+0
            if azz == hf64!("0x1.9b78223aa307cp+1") {
                y1 = hf64!("0x1.79d15d0e8d59cp+0").copysign(zz);
            }

            // ~ 0x1.de87aa837820e80000000000001c0f08p+0
            if azz == hf64!("0x1.a202bfc89ddffp+2") {
                y1 = hf64!("0x1.de87aa837820fp+0").copysign(zz);
            }

            if round != Round::Nearest {
                let wlist = [
                    (hf64!("0x1.3a9ccd7f022dbp+0"), hf64!("0x1.1236160ba9b93p+0")), // ~ 0x1.1236160ba9b930000000000001e7e8fap+0
                    (hf64!("0x1.7845d2faac6fep+0"), hf64!("0x1.23115e657e49cp+0")), // ~ 0x1.23115e657e49c0000000000001d7a799p+0
                    (hf64!("0x1.d1ef81cbbbe71p+0"), hf64!("0x1.388fb44cdcf5ap+0")), // ~ 0x1.388fb44cdcf5a0000000000002202c55p+0
                    (hf64!("0x1.0a2014f62987cp+1"), hf64!("0x1.46bcbf47dc1e8p+0")), // ~ 0x1.46bcbf47dc1e8000000000000303aa2dp+0
                    (hf64!("0x1.fe18a044a5501p+1"), hf64!("0x1.95decfec9c904p+0")), // ~ 0x1.95decfec9c9040000000000000159e8ep+0
                    (hf64!("0x1.a6bb8c803147bp+2"), hf64!("0x1.e05335a6401dep+0")), // ~ 0x1.e05335a6401de00000000000027ca017p+0
                    (hf64!("0x1.ac8538a031cbdp+2"), hf64!("0x1.e281d87098de8p+0")), // ~ 0x1.e281d87098de80000000000000ee9314p+0
                ];

                for (a, b) in wlist {
                    if azz == a {
                        let tmp = if round as u64 + sign == 2 {
                            hf64!("0x1p-52")
                        } else {
                            0.0
                        };
                        y1 = (b + tmp).copysign(zz);
                    }
                }
            }
        }
    }

    let mut cvt3: u64 = y1.to_bits();
    cvt3 = cvt3.wrapping_add(((et.wrapping_sub(342).wrapping_sub(1023)) as u64) << 52);
    let m0: u64 = cvt3 << 30;
    let m1 = m0 >> 63;

    if (m0 ^ m1) <= (1u64 << 30) {
        cold_path();

        let mut cvt4: u64 = y1.to_bits();
        cvt4 = (cvt4 + (164 << 15)) & 0xffffffffffff0000u64;

        if ((f64::from_bits(cvt4) - y1) - dy).abs() < hf64!("0x1p-60") || (zz).abs() == 1.0 {
            cvt3 = (cvt3 + (1u64 << 15)) & 0xffffffffffff0000u64;
        }
    }

    FpResult::ok(f64::from_bits(cvt3))
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn spot_checks() {
        if !cfg!(x86_no_sse) {
            // Exposes a rounding mode problem. Ignored on i586 because of inaccurate FMA.
            assert_biteq!(
                cbrt(f64::from_bits(0xf7f792b28f600000)),
                f64::from_bits(0xd29ce68655d962f3)
            );
        }
    }
}
