use crate::support::{
    CastFrom, CastInto, DFloat, Float, FpResult, HFloat, IntTy, MinInt, Round, Status,
};

/// Fma implementation when a hardware-backed larger float type is available. For `f32` and `f64`,
/// `f64` has enough precision to represent the `f32` in its entirety, except for double rounding.
#[inline]
pub fn fma_wide_round<F, B>(x: F, y: F, z: F, round: Round) -> FpResult<F>
where
    F: Float + HFloat<D = B>,
    B: Float + DFloat<H = F>,
    B::Int: CastInto<i32>,
    i32: CastFrom<i32>,
{
    let one = IntTy::<B>::ONE;

    let xy: B = x.widen() * y.widen();
    let mut result: B = xy + z.widen();
    let mut ui: B::Int = result.to_bits();
    let re = result.ex();
    let zb: B = z.widen();

    let prec_diff = B::SIG_BITS - F::SIG_BITS;
    let excess_prec = ui & ((one << prec_diff) - one);
    let halfway = one << (prec_diff - 1);

    // Common case: the larger precision is fine if...
    // This is not a halfway case
    if excess_prec != halfway
        // Or the result is NaN
        || re == B::EXP_SAT
        // Or the result is exact
        || (result - xy == zb && result - zb == xy)
        // Or the mode is something other than round to nearest
        || round != Round::Nearest
    {
        let min_inexact_exp = (B::EXP_BIAS as i32 + F::EXP_MIN_SUBNORM) as u32;
        let max_inexact_exp = (B::EXP_BIAS as i32 + F::EXP_MIN) as u32;

        let mut status = Status::OK;

        if (min_inexact_exp..max_inexact_exp).contains(&re) && status.inexact() {
            // This branch is never hit; requires previous operations to set a status
            status.set_inexact(false);

            result = xy + z.widen();
            if status.inexact() {
                status.set_underflow(true);
            } else {
                status.set_inexact(true);
            }
        }

        return FpResult {
            val: result.narrow(),
            status,
        };
    }

    let neg = ui >> (B::BITS - 1) != IntTy::<B>::ZERO;
    let err = if neg == (zb > xy) {
        xy - result + zb
    } else {
        zb - result + xy
    };
    if neg == (err < B::ZERO) {
        ui += one;
    } else {
        ui -= one;
    }

    FpResult::ok(B::from_bits(ui).narrow())
}
