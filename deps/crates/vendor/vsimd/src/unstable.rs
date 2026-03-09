use core::simd::*;

#[inline(always)]
pub fn splat<T, const N: usize>(x: T) -> Simd<T, N>
where
    T: SimdElement,
    LaneCount<N>: SupportedLaneCount,
{
    Simd::splat(x)
}
