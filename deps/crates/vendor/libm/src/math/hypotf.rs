use core::f32;

use super::sqrtf;

#[cfg_attr(all(test, assert_no_panic), no_panic::no_panic)]
pub fn hypotf(mut x: f32, mut y: f32) -> f32 {
    let x1p90 = f32::from_bits(0x6c800000); // 0x1p90f === 2 ^ 90
    let x1p_90 = f32::from_bits(0x12800000); // 0x1p-90f === 2 ^ -90

    let mut uxi = x.to_bits();
    let mut uyi = y.to_bits();
    let uti;
    let mut z: f32;

    uxi &= -1i32 as u32 >> 1;
    uyi &= -1i32 as u32 >> 1;
    if uxi < uyi {
        uti = uxi;
        uxi = uyi;
        uyi = uti;
    }

    x = f32::from_bits(uxi);
    y = f32::from_bits(uyi);
    if uyi == 0xff << 23 {
        return y;
    }
    if uxi >= 0xff << 23 || uyi == 0 || uxi - uyi >= 25 << 23 {
        return x + y;
    }

    z = 1.;
    if uxi >= (0x7f + 60) << 23 {
        z = x1p90;
        x *= x1p_90;
        y *= x1p_90;
    } else if uyi < (0x7f - 60) << 23 {
        z = x1p_90;
        x *= x1p90;
        y *= x1p90;
    }
    z * sqrtf((x as f64 * x as f64 + y as f64 * y as f64) as f32)
}
