use super::exp;

/* k is such that k*ln2 has minimal relative error and x - kln2 > log(FLT_MIN) */
const K: i32 = 2043;

/* expf(x)/2 for x >= log(FLT_MAX), slightly better than 0.5f*expf(x/2)*expf(x/2) */
#[cfg_attr(all(test, assert_no_panic), no_panic::no_panic)]
pub(crate) fn k_expo2(x: f64) -> f64 {
    let k_ln2 = f64::from_bits(0x40962066151add8b);
    /* note that k is odd and scale*scale overflows */
    let scale = f64::from_bits(((((0x3ff + K / 2) as u32) << 20) as u64) << 32);
    /* exp(x - k ln2) * 2**(k-1) */
    exp(x - k_ln2) * scale * scale
}
