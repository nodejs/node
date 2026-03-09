use vsimd::isa::detect;
use vsimd::isa::{NEON, SSE2, WASM128};
use vsimd::vector::V128;
use vsimd::SIMD128;

use const_str::hex;

#[cfg(not(miri))]
#[cfg_attr(not(target_arch = "wasm32"), test)]
#[cfg_attr(target_arch = "wasm32", wasm_bindgen_test::wasm_bindgen_test)]
fn native_sum() {
    use vsimd::native::Native;

    let x: u32 = rand::random::<u32>() / 2;
    let y: u32 = rand::random::<u32>() / 2;

    const N: usize = 100;
    let a = [x; N];
    let b = [y; N];
    let mut c = [0; N];

    Native::detect().exec(|| {
        assert!(a.len() == N && b.len() == N && c.len() == N);
        for i in 0..N {
            c[i] = a[i] + b[i];
        }
    });

    assert!(c.iter().copied().all(|z| z == x + y));
}

#[cfg_attr(not(target_arch = "wasm32"), test)]
#[cfg_attr(target_arch = "wasm32", wasm_bindgen_test::wasm_bindgen_test)]
fn u8x16_any_zero() {
    fn f(a: [u8; 16]) -> bool {
        let a = V128::from_bytes(a);
        if let Some(s) = detect::<SSE2>() {
            return s.u8x16_any_zero(a);
        }
        if let Some(s) = detect::<NEON>() {
            return s.u8x16_any_zero(a);
        }
        if let Some(s) = detect::<WASM128>() {
            return s.u8x16_any_zero(a);
        }
        a.as_bytes().iter().any(|&x| x == 0)
    }

    fn test(a: [u8; 16], expected: bool) {
        assert_eq!(f(a), expected);
    }

    test([0x00; 16], true);
    test([0xff; 16], false);
    test(hex!("00 01 02 03 04 05 06 07 08 09 0A 0B 0C 0D 0E 0F"), true);
    test(hex!("10 01 02 03 04 05 06 07 08 09 0A 0B 0C 0D 0E 0F"), false);
}
