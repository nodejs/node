use crate::isa::InstructionSet;
use crate::pod::POD;
use crate::vector::{V128, V256};
use crate::{mask::*, unified};
use crate::{SIMD128, SIMD256};

pub unsafe trait Scalable<V: POD>: InstructionSet {
    #[inline(always)]
    fn and(self, a: V, b: V) -> V {
        unified::and(self, a, b)
    }

    #[inline(always)]
    fn or(self, a: V, b: V) -> V {
        unified::or(self, a, b)
    }

    #[inline(always)]
    fn xor(self, a: V, b: V) -> V {
        unified::xor(self, a, b)
    }

    #[inline(always)]
    fn andnot(self, a: V, b: V) -> V {
        unified::andnot(self, a, b)
    }

    #[inline(always)]
    fn u8xn_splat(self, x: u8) -> V {
        unified::splat::<_, u8, _>(self, x)
    }

    #[inline(always)]
    fn i8xn_splat(self, x: i8) -> V {
        unified::splat::<_, i8, _>(self, x)
    }

    #[inline(always)]
    fn u32xn_splat(self, x: u32) -> V {
        unified::splat::<_, u32, _>(self, x)
    }

    #[inline(always)]
    fn u8xn_add(self, a: V, b: V) -> V {
        unified::add::<_, u8, _>(self, a, b)
    }

    #[inline(always)]
    fn u8xn_sub(self, a: V, b: V) -> V {
        unified::sub::<_, u8, _>(self, a, b)
    }

    #[inline(always)]
    fn u32xn_sub(self, a: V, b: V) -> V {
        unified::sub::<_, u32, _>(self, a, b)
    }

    #[inline(always)]
    fn u8xn_add_sat(self, a: V, b: V) -> V {
        unified::add_sat::<_, u8, _>(self, a, b)
    }

    #[inline(always)]
    fn i8xn_add_sat(self, a: V, b: V) -> V {
        unified::add_sat::<_, i8, _>(self, a, b)
    }

    #[inline(always)]
    fn u8xn_sub_sat(self, a: V, b: V) -> V {
        unified::sub_sat::<_, u8, _>(self, a, b)
    }

    #[inline(always)]
    fn u8xn_eq(self, a: V, b: V) -> V {
        unified::eq::<_, u8, _>(self, a, b)
    }

    #[inline(always)]
    fn i8xn_lt(self, a: V, b: V) -> V {
        unified::lt::<_, i8, _>(self, a, b)
    }

    #[inline(always)]
    fn u32xn_lt(self, a: V, b: V) -> V {
        unified::lt::<_, u32, _>(self, a, b)
    }

    #[inline(always)]
    fn u32xn_max(self, a: V, b: V) -> V {
        unified::max::<_, u32, _>(self, a, b)
    }

    fn u16xn_shl<const IMM8: i32>(self, a: V) -> V;

    fn u16xn_shr<const IMM8: i32>(self, a: V) -> V;
    fn u32xn_shr<const IMM8: i32>(self, a: V) -> V;

    fn u8xn_avgr(self, a: V, b: V) -> V;

    fn u8x16xn_swizzle(self, a: V, b: V) -> V;

    fn all_zero(self, a: V) -> bool;

    fn mask8xn_all(self, a: V) -> bool;
    fn mask8xn_any(self, a: V) -> bool;

    fn u8xn_highbit_all(self, a: V) -> bool;
    fn u8xn_highbit_any(self, a: V) -> bool;

    fn u16xn_bswap(self, a: V) -> V;
    fn u32xn_bswap(self, a: V) -> V;
    fn u64xn_bswap(self, a: V) -> V;
}

unsafe impl<S> Scalable<V128> for S
where
    S: SIMD128,
{
    #[inline(always)]
    fn u16xn_shl<const IMM8: i32>(self, a: V128) -> V128 {
        self.u16x8_shl::<IMM8>(a)
    }

    #[inline(always)]
    fn u16xn_shr<const IMM8: i32>(self, a: V128) -> V128 {
        self.u16x8_shr::<IMM8>(a)
    }

    #[inline(always)]
    fn u32xn_shr<const IMM8: i32>(self, a: V128) -> V128 {
        self.u32x4_shr::<IMM8>(a)
    }

    #[inline(always)]
    fn u8xn_avgr(self, a: V128, b: V128) -> V128 {
        self.u8x16_avgr(a, b)
    }

    #[inline(always)]
    fn u8x16xn_swizzle(self, a: V128, b: V128) -> V128 {
        self.u8x16_swizzle(a, b)
    }

    #[inline(always)]
    fn all_zero(self, a: V128) -> bool {
        self.v128_all_zero(a)
    }

    #[inline(always)]
    fn mask8xn_all(self, a: V128) -> bool {
        mask8x16_all(self, a)
    }

    #[inline(always)]
    fn mask8xn_any(self, a: V128) -> bool {
        mask8x16_any(self, a)
    }

    #[inline(always)]
    fn u8xn_highbit_all(self, a: V128) -> bool {
        u8x16_highbit_all(self, a)
    }

    #[inline(always)]
    fn u8xn_highbit_any(self, a: V128) -> bool {
        u8x16_highbit_any(self, a)
    }

    #[inline(always)]
    fn u16xn_bswap(self, a: V128) -> V128 {
        self.u16x8_bswap(a)
    }

    #[inline(always)]
    fn u32xn_bswap(self, a: V128) -> V128 {
        self.u32x4_bswap(a)
    }

    #[inline(always)]
    fn u64xn_bswap(self, a: V128) -> V128 {
        self.u64x2_bswap(a)
    }
}

unsafe impl<S> Scalable<V256> for S
where
    S: SIMD256,
{
    #[inline(always)]
    fn u16xn_shl<const IMM8: i32>(self, a: V256) -> V256 {
        self.u16x16_shl::<IMM8>(a)
    }

    #[inline(always)]
    fn u16xn_shr<const IMM8: i32>(self, a: V256) -> V256 {
        self.u16x16_shr::<IMM8>(a)
    }

    #[inline(always)]
    fn u32xn_shr<const IMM8: i32>(self, a: V256) -> V256 {
        self.u32x8_shr::<IMM8>(a)
    }

    #[inline(always)]
    fn u8xn_avgr(self, a: V256, b: V256) -> V256 {
        self.u8x32_avgr(a, b)
    }

    #[inline(always)]
    fn u8x16xn_swizzle(self, a: V256, b: V256) -> V256 {
        self.u8x16x2_swizzle(a, b)
    }

    #[inline(always)]
    fn all_zero(self, a: V256) -> bool {
        self.v256_all_zero(a)
    }

    #[inline(always)]
    fn mask8xn_all(self, a: V256) -> bool {
        mask8x32_all(self, a)
    }

    #[inline(always)]
    fn mask8xn_any(self, a: V256) -> bool {
        mask8x32_any(self, a)
    }

    #[inline(always)]
    fn u8xn_highbit_all(self, a: V256) -> bool {
        u8x32_highbit_all(self, a)
    }

    #[inline(always)]
    fn u8xn_highbit_any(self, a: V256) -> bool {
        u8x32_highbit_any(self, a)
    }

    #[inline(always)]
    fn u16xn_bswap(self, a: V256) -> V256 {
        self.u16x16_bswap(a)
    }

    #[inline(always)]
    fn u32xn_bswap(self, a: V256) -> V256 {
        self.u32x8_bswap(a)
    }

    #[inline(always)]
    fn u64xn_bswap(self, a: V256) -> V256 {
        self.u64x4_bswap(a)
    }
}
