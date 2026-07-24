#[cfg_attr(assert_no_panic, no_panic::no_panic)]
pub fn nextafter(x: f64, y: f64) -> f64 {
    if x.is_nan() || y.is_nan() {
        return x + y;
    }

    let mut ux_i = x.to_bits();
    let uy_i = y.to_bits();
    if ux_i == uy_i {
        return y;
    }

    let ax = ux_i & (!1_u64 / 2);
    let ay = uy_i & (!1_u64 / 2);
    if ax == 0 {
        if ay == 0 {
            return y;
        }
        ux_i = (uy_i & (1_u64 << 63)) | 1;
    } else if ax > ay || ((ux_i ^ uy_i) & (1_u64 << 63)) != 0 {
        ux_i -= 1;
    } else {
        ux_i += 1;
    }

    let e = (ux_i >> 52) & 0x7ff;
    // raise overflow if ux.f is infinite and x is finite
    if e == 0x7ff {
        force_eval!(x + x);
    }
    let ux_f = f64::from_bits(ux_i);
    // raise underflow if ux.f is subnormal or zero
    if e == 0 {
        force_eval!(x * x + ux_f * ux_f);
    }
    ux_f
}
