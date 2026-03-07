use crate::pod::POD;
use crate::vector::{V128, V256};
use crate::SIMD256;

pub(crate) const SHUFFLE_U16X8: V128 = V128::from_bytes([
    0x01, 0x00, 0x03, 0x02, 0x05, 0x04, 0x07, 0x06, //
    0x09, 0x08, 0x0b, 0x0a, 0x0d, 0x0c, 0x0f, 0x0e, //
]);

pub(crate) const SHUFFLE_U32X4: V128 = V128::from_bytes([
    0x03, 0x02, 0x01, 0x00, 0x07, 0x06, 0x05, 0x04, //
    0x0b, 0x0a, 0x09, 0x08, 0x0f, 0x0e, 0x0d, 0x0c, //
]);

pub(crate) const SHUFFLE_U64X2: V128 = V128::from_bytes([
    0x07, 0x06, 0x05, 0x04, 0x03, 0x02, 0x01, 0x00, //
    0x0f, 0x0e, 0x0d, 0x0c, 0x0b, 0x0a, 0x09, 0x08, //
]);

pub(crate) const SHUFFLE_U16X16: V256 = SHUFFLE_U16X8.x2();

pub(crate) const SHUFFLE_U32X8: V256 = SHUFFLE_U32X4.x2();

pub(crate) const SHUFFLE_U64X4: V256 = SHUFFLE_U64X2.x2();

pub unsafe trait BSwap: POD {
    const LANES: usize;
    fn swap_single(x: Self) -> Self;
    fn swap_simd<S: SIMD256>(s: S, a: V256) -> V256;
}

unsafe impl BSwap for u16 {
    const LANES: usize = 16;

    #[inline(always)]
    fn swap_single(x: Self) -> Self {
        x.swap_bytes()
    }

    #[inline(always)]
    fn swap_simd<S: SIMD256>(s: S, a: V256) -> V256 {
        s.u16x16_bswap(a)
    }
}

unsafe impl BSwap for u32 {
    const LANES: usize = 8;

    #[inline(always)]
    fn swap_single(x: Self) -> Self {
        x.swap_bytes()
    }

    #[inline(always)]
    fn swap_simd<S: SIMD256>(s: S, a: V256) -> V256 {
        s.u32x8_bswap(a)
    }
}

unsafe impl BSwap for u64 {
    const LANES: usize = 4;

    #[inline(always)]
    fn swap_single(x: Self) -> Self {
        x.swap_bytes()
    }

    #[inline(always)]
    fn swap_simd<S: SIMD256>(s: S, a: V256) -> V256 {
        s.u64x4_bswap(a)
    }
}

#[inline(always)]
pub unsafe fn bswap_fallback<T>(mut src: *const T, len: usize, mut dst: *mut T)
where
    T: BSwap,
{
    let end = src.add(len);
    while src < end {
        let x = src.read();
        let y = <T as BSwap>::swap_single(x);
        dst.write(y);
        src = src.add(1);
        dst = dst.add(1);
    }
}

#[inline(always)]
pub unsafe fn bswap_simd<S: SIMD256, T>(s: S, mut src: *const T, mut len: usize, mut dst: *mut T)
where
    T: BSwap,
{
    let end = src.add(len / T::LANES * T::LANES);
    while src < end {
        let x = s.v256_load_unaligned(src.cast());
        let y = <T as BSwap>::swap_simd(s, x);
        s.v256_store_unaligned(dst.cast(), y);
        src = src.add(T::LANES);
        dst = dst.add(T::LANES);
    }
    len %= T::LANES;

    bswap_fallback(src, len, dst);
}
