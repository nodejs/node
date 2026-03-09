use crate::isa::{NEON, SSE2, SSE41, WASM128};
use crate::unified;
use crate::vector::V128;
use crate::SIMD64;

#[cfg(any(target_arch = "x86", target_arch = "x86_64"))]
use crate::isa::SSSE3;

#[cfg(any(
    any(target_arch = "x86", target_arch = "x86_64"),
    any(all(feature = "unstable", target_arch = "arm"), target_arch = "aarch64"),
    target_arch = "wasm32"
))]
use core::mem::transmute as t;

#[cfg(target_arch = "x86")]
use core::arch::x86::*;

#[cfg(target_arch = "x86_64")]
use core::arch::x86_64::*;

#[cfg(all(feature = "unstable", target_arch = "arm"))]
use core::arch::arm::*;

#[cfg(target_arch = "aarch64")]
use core::arch::aarch64::*;

#[cfg(target_arch = "wasm32")]
use core::arch::wasm32::*;

pub unsafe trait SIMD128: SIMD64 {
    /// T1: SSE2, NEON, WASM128
    #[inline(always)]
    unsafe fn v128_load(self, addr: *const u8) -> V128 {
        debug_assert_ptr_align!(addr, 16);

        #[cfg(any(target_arch = "x86", target_arch = "x86_64"))]
        if matches_isa!(Self, SSE2) {
            return t(_mm_load_si128(addr.cast()));
        }
        #[cfg(any(all(feature = "unstable", target_arch = "arm"), target_arch = "aarch64"))]
        if matches_isa!(Self, NEON) {
            return self.v128_load_unaligned(addr);
        }
        #[cfg(target_arch = "wasm32")]
        if matches_isa!(Self, WASM128) {
            return self.v128_load_unaligned(addr);
        }
        {
            let _ = addr;
            unreachable!()
        }
    }

    /// T1: SSE2, NEON, WASM128
    #[inline(always)]
    unsafe fn v128_load_unaligned(self, addr: *const u8) -> V128 {
        #[cfg(any(target_arch = "x86", target_arch = "x86_64"))]
        if matches_isa!(Self, SSE2) {
            return t(_mm_loadu_si128(addr.cast()));
        }
        #[cfg(any(all(feature = "unstable", target_arch = "arm"), target_arch = "aarch64"))]
        if matches_isa!(Self, NEON) {
            return t(vld1q_u8(addr.cast()));
        }
        #[cfg(target_arch = "wasm32")]
        if matches_isa!(Self, WASM128) {
            return t(v128_load(addr.cast()));
        }
        {
            let _ = addr;
            unreachable!()
        }
    }

    /// T1: SSE2, NEON, WASM128
    #[inline(always)]
    unsafe fn v128_store(self, addr: *mut u8, a: V128) {
        debug_assert_ptr_align!(addr, 16);

        #[cfg(any(target_arch = "x86", target_arch = "x86_64"))]
        if matches_isa!(Self, SSE2) {
            return _mm_store_si128(addr.cast(), t(a));
        }
        #[cfg(any(all(feature = "unstable", target_arch = "arm"), target_arch = "aarch64"))]
        if matches_isa!(Self, NEON) {
            return self.v128_store_unaligned(addr, a);
        }
        #[cfg(target_arch = "wasm32")]
        if matches_isa!(Self, WASM128) {
            return self.v128_store_unaligned(addr, a);
        }
        {
            let _ = (addr, a);
            unreachable!()
        }
    }

    /// T1: SSE2, NEON, WASM128
    #[inline(always)]
    unsafe fn v128_store_unaligned(self, addr: *mut u8, a: V128) {
        if cfg!(miri) {
            return addr.cast::<V128>().write_unaligned(a);
        }
        #[cfg(any(target_arch = "x86", target_arch = "x86_64"))]
        if matches_isa!(Self, SSE2) {
            return _mm_storeu_si128(addr.cast(), t(a));
        }
        #[cfg(any(all(feature = "unstable", target_arch = "arm"), target_arch = "aarch64"))]
        if matches_isa!(Self, NEON) {
            return vst1q_u8(addr.cast(), t(a));
        }
        #[cfg(target_arch = "wasm32")]
        if matches_isa!(Self, WASM128) {
            return v128_store(addr.cast(), t(a));
        }
        {
            let _ = (addr, a);
            unreachable!()
        }
    }

    /// T1: SSE2, NEON, WASM128
    #[inline(always)]
    fn v128_create_zero(self) -> V128 {
        #[cfg(any(target_arch = "x86", target_arch = "x86_64"))]
        if matches_isa!(Self, SSE2) {
            return unsafe { t(_mm_setzero_si128()) };
        }
        #[cfg(any(all(feature = "unstable", target_arch = "arm"), target_arch = "aarch64"))]
        if matches_isa!(Self, NEON) {
            return unsafe { t(vdupq_n_u8(0)) };
        }
        #[cfg(target_arch = "wasm32")]
        if matches_isa!(Self, WASM128) {
            return unsafe { t(u8x16_splat(0)) };
        }
        {
            unreachable!()
        }
    }

    /// T1: NEON, WASM128
    ///
    /// T2: SSE2
    #[inline(always)]
    fn v128_not(self, a: V128) -> V128 {
        #[cfg(any(target_arch = "x86", target_arch = "x86_64"))]
        if matches_isa!(Self, SSE2) {
            return unsafe {
                let a = t(a);
                t(_mm_xor_si128(a, _mm_cmpeq_epi8(a, a)))
            };
        }
        #[cfg(any(all(feature = "unstable", target_arch = "arm"), target_arch = "aarch64"))]
        if matches_isa!(Self, NEON) {
            return unsafe { t(vmvnq_u8(t(a))) };
        }
        #[cfg(target_arch = "wasm32")]
        if matches_isa!(Self, WASM128) {
            return unsafe { t(v128_not(t(a))) };
        }
        {
            let _ = a;
            unreachable!()
        }
    }

    /// T1: SSE2, NEON, WASM128
    #[inline(always)]
    fn v128_and(self, a: V128, b: V128) -> V128 {
        unified::and(self, a, b)
    }

    /// T1: SSE2, NEON, WASM128
    #[inline(always)]
    fn v128_or(self, a: V128, b: V128) -> V128 {
        unified::or(self, a, b)
    }

    /// T1: SSE2, NEON, WASM128
    #[inline(always)]
    fn v128_xor(self, a: V128, b: V128) -> V128 {
        unified::xor(self, a, b)
    }

    /// T1: SSE2, NEON, WASM128
    #[inline(always)]
    fn v128_andnot(self, a: V128, b: V128) -> V128 {
        unified::andnot(self, a, b)
    }

    /// T1: SSE41, NEON-A64, WASM128
    ///
    /// T2: NEON-A32
    #[inline(always)]
    fn v128_all_zero(self, a: V128) -> bool {
        #[cfg(any(target_arch = "x86", target_arch = "x86_64"))]
        if matches_isa!(Self, SSE41) {
            return unsafe {
                let a = t(a);
                _mm_testz_si128(a, a) != 0
            };
        }
        #[cfg(all(feature = "unstable", target_arch = "arm"))]
        if matches_isa!(Self, NEON) {
            return unsafe {
                let a1: uint32x2x2_t = t(a);
                let a2: uint32x2_t = vorr_u32(a1.0, a1.1);
                vget_lane_u32::<0>(vpmax_u32(a2, a2)) == 0
            };
        }
        #[cfg(target_arch = "aarch64")]
        if matches_isa!(Self, NEON) {
            return unsafe { vmaxvq_u8(t(a)) == 0 };
        }
        #[cfg(target_arch = "wasm32")]
        if matches_isa!(Self, WASM128) {
            return unsafe { !v128_any_true(t(a)) };
        }
        {
            let _ = a;
            unreachable!()
        }
    }

    /// T1: SSE2, NEON, WASM128
    #[inline(always)]
    fn u8x16_splat(self, x: u8) -> V128 {
        unified::splat(self, x)
    }

    /// T1: SSE2, NEON, WASM128
    #[inline(always)]
    fn u16x8_splat(self, x: u16) -> V128 {
        unified::splat(self, x)
    }

    /// T1: SSE2, NEON, WASM128
    #[inline(always)]
    fn u32x4_splat(self, x: u32) -> V128 {
        unified::splat(self, x)
    }

    /// T1: SSE2, NEON, WASM128
    #[inline(always)]
    fn u64x2_splat(self, x: u64) -> V128 {
        unified::splat(self, x)
    }

    /// T1: SSE2, NEON, WASM128
    #[inline(always)]
    fn i8x16_splat(self, x: i8) -> V128 {
        unified::splat(self, x)
    }

    /// T1: SSE2, NEON, WASM128
    #[inline(always)]
    fn i16x8_splat(self, x: i16) -> V128 {
        unified::splat(self, x)
    }

    /// T1: SSE2, NEON, WASM128
    #[inline(always)]
    fn i32x4_splat(self, x: i32) -> V128 {
        unified::splat(self, x)
    }

    /// T1: SSE2, NEON, WASM128
    #[inline(always)]
    fn i64x2_splat(self, x: i64) -> V128 {
        unified::splat(self, x)
    }

    /// T1: SSE2, NEON, WASM128
    #[inline(always)]
    fn u8x16_add(self, a: V128, b: V128) -> V128 {
        unified::add::<_, u8, _>(self, a, b)
    }

    /// T1: SSE2, NEON, WASM128
    #[inline(always)]
    fn u16x8_add(self, a: V128, b: V128) -> V128 {
        unified::add::<_, u16, _>(self, a, b)
    }

    /// T1: SSE2, NEON, WASM128
    #[inline(always)]
    fn u32x4_add(self, a: V128, b: V128) -> V128 {
        unified::add::<_, u32, _>(self, a, b)
    }

    /// T1: SSE2, NEON, WASM128
    #[inline(always)]
    fn u64x2_add(self, a: V128, b: V128) -> V128 {
        unified::add::<_, u64, _>(self, a, b)
    }

    /// T1: SSE2, NEON, WASM128
    #[inline(always)]
    fn u8x16_sub(self, a: V128, b: V128) -> V128 {
        unified::sub::<_, u8, _>(self, a, b)
    }

    /// T1: SSE2, NEON, WASM128
    #[inline(always)]
    fn u16x8_sub(self, a: V128, b: V128) -> V128 {
        unified::sub::<_, u16, _>(self, a, b)
    }

    /// T1: SSE2, NEON, WASM128
    #[inline(always)]
    fn u32x4_sub(self, a: V128, b: V128) -> V128 {
        unified::sub::<_, u32, _>(self, a, b)
    }

    /// T1: SSE2, NEON, WASM128
    #[inline(always)]
    fn u64x2_sub(self, a: V128, b: V128) -> V128 {
        unified::sub::<_, u64, _>(self, a, b)
    }

    /// T1: SSE2, NEON, WASM128
    #[inline(always)]
    fn u8x16_sub_sat(self, a: V128, b: V128) -> V128 {
        unified::sub_sat::<_, u8, _>(self, a, b)
    }

    /// T1: SSE2, NEON, WASM128
    #[inline(always)]
    fn u16x8_sub_sat(self, a: V128, b: V128) -> V128 {
        unified::sub_sat::<_, u16, _>(self, a, b)
    }

    /// T1: SSE2, NEON, WASM128
    #[inline(always)]
    fn i8x16_sub_sat(self, a: V128, b: V128) -> V128 {
        unified::sub_sat::<_, i8, _>(self, a, b)
    }

    /// T1: SSE2, NEON, WASM128
    #[inline(always)]
    fn i16x8_sub_sat(self, a: V128, b: V128) -> V128 {
        unified::sub_sat::<_, i16, _>(self, a, b)
    }

    /// T1: SSE2, NEON, WASM128
    #[inline(always)]
    fn i16x8_mul_lo(self, a: V128, b: V128) -> V128 {
        #[cfg(any(target_arch = "x86", target_arch = "x86_64"))]
        if matches_isa!(Self, SSE2) {
            return unsafe { t(_mm_mullo_epi16(t(a), t(b))) };
        }
        #[cfg(any(all(feature = "unstable", target_arch = "arm"), target_arch = "aarch64"))]
        if matches_isa!(Self, NEON) {
            return unsafe { t(vmulq_s16(t(a), t(b))) };
        }
        #[cfg(target_arch = "wasm32")]
        if matches_isa!(Self, WASM128) {
            return unsafe { t(i16x8_mul(t(a), t(b))) };
        }
        {
            let _ = (a, b);
            unreachable!()
        }
    }

    /// T1: SSE41, NEON, WASM128
    #[inline(always)]
    fn i32x4_mul_lo(self, a: V128, b: V128) -> V128 {
        #[cfg(any(target_arch = "x86", target_arch = "x86_64"))]
        if matches_isa!(Self, SSE41) {
            return unsafe { t(_mm_mullo_epi32(t(a), t(b))) };
        }
        #[cfg(any(all(feature = "unstable", target_arch = "arm"), target_arch = "aarch64"))]
        if matches_isa!(Self, NEON) {
            return unsafe { t(vmulq_s32(t(a), t(b))) };
        }
        #[cfg(target_arch = "wasm32")]
        if matches_isa!(Self, WASM128) {
            return unsafe { t(i32x4_mul(t(a), t(b))) };
        }
        {
            let _ = (a, b);
            unreachable!()
        }
    }

    /// T1: SSE2, NEON, WASM128
    #[inline(always)]
    fn u16x8_shl<const IMM8: i32>(self, a: V128) -> V128 {
        if cfg!(miri) {
            return crate::simulation::u16x8_shl(a, IMM8 as u8);
        }
        #[cfg(any(target_arch = "x86", target_arch = "x86_64"))]
        if matches_isa!(Self, SSE2) {
            return unsafe { t(_mm_slli_epi16::<IMM8>(t(a))) };
        }
        #[cfg(any(all(feature = "unstable", target_arch = "arm"), target_arch = "aarch64"))]
        if matches_isa!(Self, NEON) {
            return unsafe { t(vshlq_n_u16::<IMM8>(t(a))) };
        }
        #[cfg(target_arch = "wasm32")]
        if matches_isa!(Self, WASM128) {
            return unsafe { t(u16x8_shl(t(a), IMM8 as u32)) };
        }
        {
            let _ = a;
            unreachable!()
        }
    }

    /// T1: SSE2, NEON, WASM128
    #[inline(always)]
    fn u32x4_shl<const IMM8: i32>(self, a: V128) -> V128 {
        #[cfg(any(target_arch = "x86", target_arch = "x86_64"))]
        if matches_isa!(Self, SSE2) {
            return unsafe { t(_mm_slli_epi32::<IMM8>(t(a))) };
        }
        #[cfg(any(all(feature = "unstable", target_arch = "arm"), target_arch = "aarch64"))]
        if matches_isa!(Self, NEON) {
            return unsafe { t(vshlq_n_u32::<IMM8>(t(a))) };
        }
        #[cfg(target_arch = "wasm32")]
        if matches_isa!(Self, WASM128) {
            return unsafe { t(u32x4_shl(t(a), IMM8 as u32)) };
        }
        {
            let _ = a;
            unreachable!()
        }
    }

    /// T1: SSE2, NEON, WASM128
    #[inline(always)]
    fn u16x8_shr<const IMM8: i32>(self, a: V128) -> V128 {
        if cfg!(miri) {
            return crate::simulation::u16x8_shr(a, IMM8 as u8);
        }
        #[cfg(any(target_arch = "x86", target_arch = "x86_64"))]
        if matches_isa!(Self, SSE2) {
            return unsafe { t(_mm_srli_epi16::<IMM8>(t(a))) };
        }
        #[cfg(any(all(feature = "unstable", target_arch = "arm"), target_arch = "aarch64"))]
        if matches_isa!(Self, NEON) {
            return unsafe { t(vshrq_n_u16::<IMM8>(t(a))) };
        }
        #[cfg(target_arch = "wasm32")]
        if matches_isa!(Self, WASM128) {
            return unsafe { t(u16x8_shr(t(a), IMM8 as u32)) };
        }
        {
            let _ = a;
            unreachable!()
        }
    }

    /// T1: SSE2, NEON, WASM128
    #[inline(always)]
    fn u32x4_shr<const IMM8: i32>(self, a: V128) -> V128 {
        #[cfg(any(target_arch = "x86", target_arch = "x86_64"))]
        if matches_isa!(Self, SSE2) {
            return unsafe { t(_mm_srli_epi32::<IMM8>(t(a))) };
        }
        #[cfg(any(all(feature = "unstable", target_arch = "arm"), target_arch = "aarch64"))]
        if matches_isa!(Self, NEON) {
            return unsafe { t(vshrq_n_u32::<IMM8>(t(a))) };
        }
        #[cfg(target_arch = "wasm32")]
        if matches_isa!(Self, WASM128) {
            return unsafe { t(u32x4_shr(t(a), IMM8 as u32)) };
        }
        {
            let _ = a;
            unreachable!()
        }
    }

    /// T1: SSE2, NEON, WASM128
    #[inline(always)]
    fn u8x16_eq(self, a: V128, b: V128) -> V128 {
        unified::eq::<_, u8, _>(self, a, b)
    }

    /// T1: SSE2, NEON, WASM128
    #[inline(always)]
    fn u16x8_eq(self, a: V128, b: V128) -> V128 {
        unified::eq::<_, u16, _>(self, a, b)
    }

    /// T1: SSE2, NEON, WASM128
    #[inline(always)]
    fn u32x4_eq(self, a: V128, b: V128) -> V128 {
        unified::eq::<_, u32, _>(self, a, b)
    }

    /// T1: NEON, WASM128
    ///
    /// T2: SSE2
    #[inline(always)]
    fn u8x16_lt(self, a: V128, b: V128) -> V128 {
        unified::lt::<_, u8, _>(self, a, b)
    }

    /// T1: NEON, WASM128
    ///
    /// T2: SSE2
    #[inline(always)]
    fn u16x8_lt(self, a: V128, b: V128) -> V128 {
        unified::lt::<_, u16, _>(self, a, b)
    }

    /// T1: NEON, WASM128
    ///
    /// T2: SSE2
    #[inline(always)]
    fn u32x4_lt(self, a: V128, b: V128) -> V128 {
        unified::lt::<_, u32, _>(self, a, b)
    }

    /// T1: SSE2, NEON, WASM128
    #[inline(always)]
    fn i8x16_lt(self, a: V128, b: V128) -> V128 {
        unified::lt::<_, i8, _>(self, a, b)
    }

    /// T1: SSE2, NEON, WASM128
    #[inline(always)]
    fn i16x8_lt(self, a: V128, b: V128) -> V128 {
        unified::lt::<_, i16, _>(self, a, b)
    }

    /// T1: SSE2, NEON, WASM128
    #[inline(always)]
    fn i32x4_lt(self, a: V128, b: V128) -> V128 {
        unified::lt::<_, i32, _>(self, a, b)
    }

    /// T1: SSE2, NEON, WASM128
    #[inline(always)]
    fn u8x16_max(self, a: V128, b: V128) -> V128 {
        unified::max::<_, u8, _>(self, a, b)
    }

    /// T1: SSE41, NEON, WASM128
    #[inline(always)]
    fn u16x8_max(self, a: V128, b: V128) -> V128 {
        unified::max::<_, u16, _>(self, a, b)
    }

    /// T1: SSE41, NEON, WASM128
    #[inline(always)]
    fn u32x4_max(self, a: V128, b: V128) -> V128 {
        unified::max::<_, u32, _>(self, a, b)
    }

    /// T1: SSE41, NEON, WASM128
    #[inline(always)]
    fn i8x16_max(self, a: V128, b: V128) -> V128 {
        unified::max::<_, i8, _>(self, a, b)
    }

    /// T1: SSE2, NEON, WASM128
    #[inline(always)]
    fn i16x8_max(self, a: V128, b: V128) -> V128 {
        unified::max::<_, i16, _>(self, a, b)
    }

    /// T1: SSE41, NEON, WASM128
    #[inline(always)]
    fn i32x4_max(self, a: V128, b: V128) -> V128 {
        unified::max::<_, i32, _>(self, a, b)
    }

    /// T1: SSE2, NEON, WASM128
    #[inline(always)]
    fn u8x16_min(self, a: V128, b: V128) -> V128 {
        unified::min::<_, u8, _>(self, a, b)
    }

    /// T1: SSE41, NEON, WASM128
    #[inline(always)]
    fn u16x8_min(self, a: V128, b: V128) -> V128 {
        unified::min::<_, u16, _>(self, a, b)
    }

    /// T1: SSE41, NEON, WASM128
    #[inline(always)]
    fn u32x4_min(self, a: V128, b: V128) -> V128 {
        unified::min::<_, u32, _>(self, a, b)
    }

    /// T1: SSE41, NEON, WASM128
    #[inline(always)]
    fn i8x16_min(self, a: V128, b: V128) -> V128 {
        unified::min::<_, i8, _>(self, a, b)
    }

    /// T1: SSE2, NEON, WASM128
    #[inline(always)]
    fn i16x8_min(self, a: V128, b: V128) -> V128 {
        unified::min::<_, i16, _>(self, a, b)
    }

    /// T1: SSE41, NEON, WASM128
    #[inline(always)]
    fn i32x4_min(self, a: V128, b: V128) -> V128 {
        unified::min::<_, i32, _>(self, a, b)
    }

    /// T1: SSSE3, NEON-A64, WASM128
    ///
    /// T2: NEON-A32
    #[inline(always)]
    fn u8x16_swizzle(self, a: V128, b: V128) -> V128 {
        #[cfg(any(target_arch = "x86", target_arch = "x86_64"))]
        if matches_isa!(Self, SSSE3) {
            return unsafe { t(_mm_shuffle_epi8(t(a), t(b))) };
        }
        #[cfg(all(feature = "unstable", target_arch = "arm"))]
        if matches_isa!(Self, NEON) {
            return unsafe {
                let (a, b) = (t(a), t(b));
                let a = uint8x8x2_t(vget_low_u8(a), vget_high_u8(a));
                let b = (vget_low_u8(b), vget_high_u8(b));
                let c = (vtbl2_u8(a, b.0), vtbl2_u8(a, b.1));
                t([c.0, c.1])
            };
        }
        #[cfg(target_arch = "aarch64")]
        if matches_isa!(Self, NEON) {
            return unsafe { t(vqtbl1q_u8(t(a), t(b))) };
        }
        #[cfg(target_arch = "wasm32")]
        if matches_isa!(Self, WASM128) {
            return unsafe { t(u8x16_swizzle(t(a), t(b))) };
        }
        {
            let _ = (a, b);
            unreachable!()
        }
    }

    /// T1: SSE41, NEON, WASM128
    #[inline(always)]
    fn u16x8_bswap(self, a: V128) -> V128 {
        if matches_isa!(Self, SSE41 | WASM128) {
            return self.u8x16_swizzle(a, crate::bswap::SHUFFLE_U16X8);
        }

        #[cfg(any(all(feature = "unstable", target_arch = "arm"), target_arch = "aarch64"))]
        if matches_isa!(Self, NEON) {
            return unsafe { t(vrev16q_u8(t(a))) };
        }

        {
            let _ = a;
            unreachable!()
        }
    }

    /// T1: SSE41, NEON, WASM128
    #[inline(always)]
    fn u32x4_bswap(self, a: V128) -> V128 {
        if matches_isa!(Self, SSE41 | WASM128) {
            return self.u8x16_swizzle(a, crate::bswap::SHUFFLE_U32X4);
        }

        #[cfg(any(all(feature = "unstable", target_arch = "arm"), target_arch = "aarch64"))]
        if matches_isa!(Self, NEON) {
            return unsafe { t(vrev32q_u8(t(a))) };
        }

        {
            let _ = a;
            unreachable!()
        }
    }

    /// T1: SSE41, NEON, WASM128
    #[inline(always)]
    fn u64x2_bswap(self, a: V128) -> V128 {
        if matches_isa!(Self, SSE41 | WASM128) {
            return self.u8x16_swizzle(a, crate::bswap::SHUFFLE_U64X2);
        }

        #[cfg(any(all(feature = "unstable", target_arch = "arm"), target_arch = "aarch64"))]
        if matches_isa!(Self, NEON) {
            return unsafe { t(vrev64q_u8(t(a))) };
        }

        {
            let _ = a;
            unreachable!()
        }
    }

    /// T1: NEON-A64, WASM128
    ///
    /// T2: SSE2, NEON-A32
    #[inline(always)]
    fn u8x16_any_zero(self, a: V128) -> bool {
        #[cfg(any(target_arch = "x86", target_arch = "x86_64"))]
        if matches_isa!(Self, SSE2) {
            let is_zero = self.u8x16_eq(a, self.v128_create_zero());
            return self.u8x16_bitmask(is_zero) != 0;
        }
        #[cfg(all(feature = "unstable", target_arch = "arm"))]
        if matches_isa!(Self, NEON) {
            return unsafe {
                let a: uint8x8x2_t = t(a);
                let a = vpmin_u8(a.0, a.1);
                let m: u64 = t(vtst_u8(a, a));
                m != u64::MAX
            };
        }
        #[cfg(target_arch = "aarch64")]
        if matches_isa!(Self, NEON) {
            return unsafe { vminvq_u8(t(a)) == 0 };
        }
        #[cfg(target_arch = "wasm32")]
        if matches_isa!(Self, WASM128) {
            return unsafe { !u8x16_all_true(t(a)) };
        }
        {
            let _ = a;
            unreachable!()
        }
    }

    /// T1: SSE2, WASM128
    #[inline(always)]
    fn u8x16_bitmask(self, a: V128) -> u16 {
        if cfg!(miri) {
            return crate::simulation::u8x16_bitmask(a);
        }
        #[cfg(any(target_arch = "x86", target_arch = "x86_64"))]
        if matches_isa!(Self, SSE2) {
            return unsafe { _mm_movemask_epi8(t(a)) as u16 };
        }
        #[cfg(any(all(feature = "unstable", target_arch = "arm"), target_arch = "aarch64"))]
        if matches_isa!(Self, NEON) {
            unimplemented!()
        }
        #[cfg(target_arch = "wasm32")]
        if matches_isa!(Self, WASM128) {
            return unsafe { u8x16_bitmask(t(a)) };
        }
        {
            let _ = a;
            unreachable!()
        }
    }

    /// T1: NEON-A64
    #[inline(always)]
    fn u8x16_reduce_max(self, a: V128) -> u8 {
        if matches_isa!(Self, SSE41 | WASM128) {
            unimplemented!()
        }
        #[cfg(all(feature = "unstable", target_arch = "arm"))]
        if matches_isa!(Self, NEON) {
            unimplemented!()
        }
        #[cfg(target_arch = "aarch64")]
        if matches_isa!(Self, NEON) {
            return unsafe { vmaxvq_u8(t(a)) };
        }
        {
            let _ = a;
            unreachable!()
        }
    }

    /// T1: NEON-A64
    #[inline(always)]
    fn u8x16_reduce_min(self, a: V128) -> u8 {
        if matches_isa!(Self, SSE41 | WASM128) {
            unimplemented!()
        }
        #[cfg(all(feature = "unstable", target_arch = "arm"))]
        if matches_isa!(Self, NEON) {
            unimplemented!()
        }
        #[cfg(target_arch = "aarch64")]
        if matches_isa!(Self, NEON) {
            return unsafe { vminvq_u8(t(a)) };
        }
        {
            let _ = a;
            unreachable!()
        }
    }

    /// T1: NEON
    ///
    /// T2: SSE2, WASM128
    #[inline(always)]
    fn v128_bsl(self, a: V128, b: V128, c: V128) -> V128 {
        if matches_isa!(Self, SSE2 | WASM128) {
            return self.v128_xor(self.v128_and(self.v128_xor(b, c), a), c);
        }
        #[cfg(any(all(feature = "unstable", target_arch = "arm"), target_arch = "aarch64"))]
        if matches_isa!(Self, NEON) {
            return unsafe { t(vbslq_u8(t(a), t(b), t(c))) };
        }
        {
            let _ = (a, b, c);
            unreachable!()
        }
    }

    /// T1: SSE2, NEON, WASM128
    #[inline(always)]
    fn u8x16_zip_lo(self, a: V128, b: V128) -> V128 {
        #[cfg(any(target_arch = "x86", target_arch = "x86_64"))]
        if matches_isa!(Self, SSE2) {
            return unsafe { t(_mm_unpacklo_epi8(t(a), t(b))) };
        }
        #[cfg(all(feature = "unstable", target_arch = "arm"))]
        if matches_isa!(Self, NEON) {
            return unsafe { t(vzipq_u8(t(a), t(b)).0) };
        }
        #[cfg(target_arch = "aarch64")]
        if matches_isa!(Self, NEON) {
            return unsafe { t(vzip1q_u8(t(a), t(b))) };
        }
        #[cfg(target_arch = "wasm32")]
        if matches_isa!(Self, WASM128) {
            let (a, b) = unsafe { (t(a), t(b)) };
            let ans = u8x16_shuffle::<0, 16, 1, 17, 2, 18, 3, 19, 4, 20, 5, 21, 6, 22, 7, 23>(a, b);
            return unsafe { t(ans) };
        }
        {
            let _ = (a, b);
            unreachable!()
        }
    }

    /// T1: SSE2, NEON, WASM128
    #[inline(always)]
    fn u8x16_zip_hi(self, a: V128, b: V128) -> V128 {
        #[cfg(any(target_arch = "x86", target_arch = "x86_64"))]
        if matches_isa!(Self, SSE2) {
            return unsafe { t(_mm_unpackhi_epi8(t(a), t(b))) };
        }
        #[cfg(all(feature = "unstable", target_arch = "arm"))]
        if matches_isa!(Self, NEON) {
            return unsafe { t(vzipq_u8(t(a), t(b)).1) };
        }
        #[cfg(target_arch = "aarch64")]
        if matches_isa!(Self, NEON) {
            return unsafe { t(vzip2q_u8(t(a), t(b))) };
        }
        #[cfg(target_arch = "wasm32")]
        if matches_isa!(Self, WASM128) {
            let (a, b) = unsafe { (t(a), t(b)) };
            let ans = u8x16_shuffle::<8, 24, 9, 25, 10, 26, 11, 27, 12, 28, 13, 29, 14, 30, 15, 31>(a, b);
            return unsafe { t(ans) };
        }
        {
            let _ = (a, b);
            unreachable!()
        }
    }

    /// T1: SSE2, NEON, WASM128
    #[inline(always)]
    fn u16x8_zip_lo(self, a: V128, b: V128) -> V128 {
        #[cfg(any(target_arch = "x86", target_arch = "x86_64"))]
        if matches_isa!(Self, SSE2) {
            return unsafe { t(_mm_unpacklo_epi16(t(a), t(b))) };
        }
        #[cfg(all(feature = "unstable", target_arch = "arm"))]
        if matches_isa!(Self, NEON) {
            return unsafe { t(vzipq_u16(t(a), t(b)).0) };
        }
        #[cfg(target_arch = "aarch64")]
        if matches_isa!(Self, NEON) {
            return unsafe { t(vzip1q_u16(t(a), t(b))) };
        }
        #[cfg(target_arch = "wasm32")]
        if matches_isa!(Self, WASM128) {
            let (a, b) = unsafe { (t(a), t(b)) };
            let ans = u16x8_shuffle::<0, 8, 1, 9, 2, 10, 3, 11>(a, b);
            return unsafe { t(ans) };
        }
        {
            let _ = (a, b);
            unreachable!()
        }
    }

    /// T1: SSE2, NEON, WASM128
    #[inline(always)]
    fn u16x8_zip_hi(self, a: V128, b: V128) -> V128 {
        #[cfg(any(target_arch = "x86", target_arch = "x86_64"))]
        if matches_isa!(Self, SSE2) {
            return unsafe { t(_mm_unpackhi_epi16(t(a), t(b))) };
        }
        #[cfg(all(feature = "unstable", target_arch = "arm"))]
        if matches_isa!(Self, NEON) {
            return unsafe { t(vzipq_u16(t(a), t(b)).1) };
        }
        #[cfg(target_arch = "aarch64")]
        if matches_isa!(Self, NEON) {
            return unsafe { t(vzip2q_u16(t(a), t(b))) };
        }
        #[cfg(target_arch = "wasm32")]
        if matches_isa!(Self, WASM128) {
            let (a, b) = unsafe { (t(a), t(b)) };
            let ans = u16x8_shuffle::<4, 12, 5, 13, 6, 14, 7, 15>(a, b);
            return unsafe { t(ans) };
        }
        {
            let _ = (a, b);
            unreachable!()
        }
    }

    /// T1: SSE2, NEON, WASM128
    #[inline(always)]
    fn u32x4_zip_lo(self, a: V128, b: V128) -> V128 {
        #[cfg(any(target_arch = "x86", target_arch = "x86_64"))]
        if matches_isa!(Self, SSE2) {
            return unsafe { t(_mm_unpacklo_epi32(t(a), t(b))) };
        }
        #[cfg(all(feature = "unstable", target_arch = "arm"))]
        if matches_isa!(Self, NEON) {
            return unsafe { t(vzipq_u32(t(a), t(b)).0) };
        }
        #[cfg(target_arch = "aarch64")]
        if matches_isa!(Self, NEON) {
            return unsafe { t(vzip1q_u32(t(a), t(b))) };
        }
        #[cfg(target_arch = "wasm32")]
        if matches_isa!(Self, WASM128) {
            let (a, b) = unsafe { (t(a), t(b)) };
            let ans = u32x4_shuffle::<0, 4, 1, 5>(a, b);
            return unsafe { t(ans) };
        }
        {
            let _ = (a, b);
            unreachable!()
        }
    }

    /// T1: SSE2, NEON, WASM128
    #[inline(always)]
    fn u32x4_zip_hi(self, a: V128, b: V128) -> V128 {
        #[cfg(any(target_arch = "x86", target_arch = "x86_64"))]
        if matches_isa!(Self, SSE2) {
            return unsafe { t(_mm_unpackhi_epi32(t(a), t(b))) };
        }
        #[cfg(all(feature = "unstable", target_arch = "arm"))]
        if matches_isa!(Self, NEON) {
            return unsafe { t(vzipq_u32(t(a), t(b)).1) };
        }
        #[cfg(target_arch = "aarch64")]
        if matches_isa!(Self, NEON) {
            return unsafe { t(vzip2q_u32(t(a), t(b))) };
        }
        #[cfg(target_arch = "wasm32")]
        if matches_isa!(Self, WASM128) {
            let (a, b) = unsafe { (t(a), t(b)) };
            let ans = u32x4_shuffle::<2, 6, 3, 7>(a, b);
            return unsafe { t(ans) };
        }
        {
            let _ = (a, b);
            unreachable!()
        }
    }

    /// T1: SSE2, NEON, WASM128
    #[inline(always)]
    fn u64x2_zip_lo(self, a: V128, b: V128) -> V128 {
        #[cfg(any(target_arch = "x86", target_arch = "x86_64"))]
        if matches_isa!(Self, SSE2) {
            return unsafe { t(_mm_unpacklo_epi64(t(a), t(b))) };
        }
        #[cfg(any(all(feature = "unstable", target_arch = "arm"), target_arch = "aarch64"))]
        if matches_isa!(Self, NEON) {
            return unsafe {
                let (a, b): ([u64; 2], [u64; 2]) = (t(a), t(b));
                t([a[0], b[0]])
            };
        }
        #[cfg(target_arch = "wasm32")]
        if matches_isa!(Self, WASM128) {
            let (a, b) = unsafe { (t(a), t(b)) };
            let ans = u64x2_shuffle::<0, 2>(a, b);
            return unsafe { t(ans) };
        }
        {
            let _ = (a, b);
            unreachable!()
        }
    }

    /// T1: SSE2, NEON, WASM128
    #[inline(always)]
    fn u64x2_zip_hi(self, a: V128, b: V128) -> V128 {
        #[cfg(any(target_arch = "x86", target_arch = "x86_64"))]
        if matches_isa!(Self, SSE2) {
            return unsafe { t(_mm_unpackhi_epi64(t(a), t(b))) };
        }
        #[cfg(any(all(feature = "unstable", target_arch = "arm"), target_arch = "aarch64"))]
        if matches_isa!(Self, NEON) {
            return unsafe {
                let (a, b): ([u64; 2], [u64; 2]) = (t(a), t(b));
                t([a[1], b[1]])
            };
        }
        #[cfg(target_arch = "wasm32")]
        if matches_isa!(Self, WASM128) {
            let (a, b) = unsafe { (t(a), t(b)) };
            let ans = u64x2_shuffle::<1, 3>(a, b);
            return unsafe { t(ans) };
        }
        {
            let _ = (a, b);
            unreachable!()
        }
    }

    /// T1: NEON, WASM128
    #[inline(always)]
    fn u8x16_unzip_even(self, a: V128, b: V128) -> V128 {
        #[cfg(any(target_arch = "x86", target_arch = "x86_64"))]
        if matches_isa!(Self, SSE2) {
            unimplemented!()
        }
        #[cfg(all(feature = "unstable", target_arch = "arm"))]
        if matches_isa!(Self, NEON) {
            return unsafe { t(vuzpq_u8(t(a), t(b)).0) };
        }
        #[cfg(target_arch = "aarch64")]
        if matches_isa!(Self, NEON) {
            return unsafe { t(vuzp1q_u8(t(a), t(b))) };
        }
        #[cfg(target_arch = "wasm32")]
        if matches_isa!(Self, WASM128) {
            let (a, b) = unsafe { (t(a), t(b)) };
            let ans = u8x16_shuffle::<0, 2, 4, 6, 8, 10, 12, 14, 16, 18, 20, 22, 24, 26, 28, 30>(a, b);
            return unsafe { t(ans) };
        }
        {
            let _ = (a, b);
            unreachable!()
        }
    }

    /// T1: NEON, WASM128
    #[inline(always)]
    fn u8x16_unzip_odd(self, a: V128, b: V128) -> V128 {
        #[cfg(any(target_arch = "x86", target_arch = "x86_64"))]
        if matches_isa!(Self, SSE2) {
            unimplemented!()
        }
        #[cfg(all(feature = "unstable", target_arch = "arm"))]
        if matches_isa!(Self, NEON) {
            return unsafe { t(vuzpq_u8(t(a), t(b)).1) };
        }
        #[cfg(target_arch = "aarch64")]
        if matches_isa!(Self, NEON) {
            return unsafe { t(vuzp2q_u8(t(a), t(b))) };
        }
        #[cfg(target_arch = "wasm32")]
        if matches_isa!(Self, WASM128) {
            let (a, b) = unsafe { (t(a), t(b)) };
            let ans = u8x16_shuffle::<1, 3, 5, 7, 9, 11, 13, 15, 17, 19, 21, 23, 25, 27, 29, 31>(a, b);
            return unsafe { t(ans) };
        }
        {
            let _ = (a, b);
            unreachable!()
        }
    }

    /// T1: SSE2
    #[inline(always)]
    fn u16x8_mul_hi(self, a: V128, b: V128) -> V128 {
        #[cfg(any(target_arch = "x86", target_arch = "x86_64"))]
        if matches_isa!(Self, SSE2) {
            return unsafe { t(_mm_mulhi_epu16(t(a), t(b))) };
        }
        if matches_isa!(Self, NEON | WASM128) {
            unimplemented!()
        }
        {
            let _ = (a, b);
            unreachable!()
        }
    }

    /// T1: SSE2
    #[inline(always)]
    fn i16x8_mul_hi(self, a: V128, b: V128) -> V128 {
        #[cfg(any(target_arch = "x86", target_arch = "x86_64"))]
        if matches_isa!(Self, SSE2) {
            return unsafe { t(_mm_mulhi_epi16(t(a), t(b))) };
        }
        if matches_isa!(Self, NEON | WASM128) {
            unimplemented!()
        }
        {
            let _ = (a, b);
            unreachable!()
        }
    }

    /// T1: SSSE3
    #[inline(always)]
    fn i16x8_maddubs(self, a: V128, b: V128) -> V128 {
        #[cfg(any(target_arch = "x86", target_arch = "x86_64"))]
        if matches_isa!(Self, SSSE3) {
            return unsafe { t(_mm_maddubs_epi16(t(a), t(b))) };
        }
        if matches_isa!(Self, NEON | WASM128) {
            unimplemented!()
        }
        {
            let _ = (a, b);
            unreachable!()
        }
    }

    /// T1: SSE41
    #[inline(always)]
    fn u16x8_blend<const IMM8: i32>(self, a: V128, b: V128) -> V128 {
        #[cfg(any(target_arch = "x86", target_arch = "x86_64"))]
        if matches_isa!(Self, SSE41) {
            return unsafe { t(_mm_blend_epi16::<IMM8>(t(a), t(b))) };
        }
        if matches_isa!(Self, NEON | WASM128) {
            unimplemented!()
        }
        {
            let _ = (a, b);
            unreachable!()
        }
    }

    /// if highbit(c) { b } else { a }
    ///
    /// T1: SSE41
    #[inline(always)]
    fn u8x16_blendv(self, a: V128, b: V128, c: V128) -> V128 {
        #[cfg(any(target_arch = "x86", target_arch = "x86_64"))]
        if matches_isa!(Self, SSE41) {
            return unsafe { t(_mm_blendv_epi8(t(a), t(b), t(c))) };
        }
        if matches_isa!(Self, NEON | WASM128) {
            unimplemented!()
        }
        {
            let _ = (a, b, c);
            unreachable!()
        }
    }

    /// T1: SSE2
    #[inline(always)]
    fn i16x8_madd(self, a: V128, b: V128) -> V128 {
        #[cfg(any(target_arch = "x86", target_arch = "x86_64"))]
        if matches_isa!(Self, SSE2) {
            return unsafe { t(_mm_madd_epi16(t(a), t(b))) };
        }
        if matches_isa!(Self, NEON | WASM128) {
            unimplemented!()
        }
        {
            let _ = (a, b);
            unreachable!()
        }
    }

    /// T1: SSE2, NEON, WASM128
    #[inline(always)]
    fn u8x16_avgr(self, a: V128, b: V128) -> V128 {
        #[cfg(any(target_arch = "x86", target_arch = "x86_64"))]
        if matches_isa!(Self, SSE2) {
            return unsafe { t(_mm_avg_epu8(t(a), t(b))) };
        }
        #[cfg(any(all(feature = "unstable", target_arch = "arm"), target_arch = "aarch64"))]
        if matches_isa!(Self, NEON) {
            return unsafe { t(vrhaddq_u8(t(a), t(b))) };
        }
        #[cfg(target_arch = "wasm32")]
        if matches_isa!(Self, WASM128) {
            return unsafe { t(u8x16_avgr(t(a), t(b))) };
        }
        {
            let _ = (a, b);
            unreachable!()
        }
    }

    /// T1: SSE2, NEON, WASM128
    #[inline(always)]
    fn i8x16_add_sat(self, a: V128, b: V128) -> V128 {
        unified::add_sat::<_, i8, _>(self, a, b)
    }

    /// T1: SSE2, NEON, WASM128
    #[inline(always)]
    fn u8x16_add_sat(self, a: V128, b: V128) -> V128 {
        unified::add_sat::<_, u8, _>(self, a, b)
    }

    /// T1: SSE2
    #[inline(always)]
    fn i16x8_packus(self, a: V128, b: V128) -> V128 {
        if cfg!(miri) {
            return crate::simulation::i16x8_packus(a, b);
        }
        #[cfg(any(target_arch = "x86", target_arch = "x86_64"))]
        if matches_isa!(Self, SSE2) {
            return unsafe { t(_mm_packus_epi16(t(a), t(b))) };
        }
        {
            let _ = (a, b);
            unreachable!()
        }
    }
}
