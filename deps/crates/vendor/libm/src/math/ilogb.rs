const FP_ILOGBNAN: i32 = -1 - 0x7fffffff;
const FP_ILOGB0: i32 = FP_ILOGBNAN;

#[cfg_attr(all(test, assert_no_panic), no_panic::no_panic)]
pub fn ilogb(x: f64) -> i32 {
    let mut i: u64 = x.to_bits();
    let e = ((i >> 52) & 0x7ff) as i32;

    if e == 0 {
        i <<= 12;
        if i == 0 {
            force_eval!(0.0 / 0.0);
            return FP_ILOGB0;
        }
        /* subnormal x */
        let mut e = -0x3ff;
        while (i >> 63) == 0 {
            e -= 1;
            i <<= 1;
        }
        e
    } else if e == 0x7ff {
        force_eval!(0.0 / 0.0);
        if (i << 12) != 0 {
            FP_ILOGBNAN
        } else {
            i32::MAX
        }
    } else {
        e - 0x3ff
    }
}
