use super::{combine_words, exp};

/* exp(x)/2 for x >= log(DBL_MAX), slightly better than 0.5*exp(x/2)*exp(x/2) */
#[cfg_attr(all(test, assert_no_panic), no_panic::no_panic)]
pub(crate) fn expo2(x: f64) -> f64 {
    /* k is such that k*ln2 has minimal relative error and x - kln2 > log(DBL_MIN) */
    const K: i32 = 2043;
    let kln2 = f64::from_bits(0x40962066151add8b);

    /* note that k is odd and scale*scale overflows */
    let scale = combine_words(((0x3ff + K / 2) as u32) << 20, 0);
    /* exp(x - k ln2) * 2**(k-1) */
    exp(x - kln2) * scale * scale
}
