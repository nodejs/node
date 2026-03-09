use core::mem;

pub use core::arch::x86_64::*;

pub const fn _MM_SHUFFLE(z: u32, y: u32, x: u32, w: u32) -> i32 {
    ((z << 6) | (y << 4) | (x << 2) | w) as i32
}

pub const fn _mm_set_epi64x(e1: i64, e0: i64) -> __m128i {
    unsafe { mem::transmute([e0, e1]) }
}

pub const fn _mm_set_epi32(e3: i32, e2: i32, e1: i32, e0: i32) -> __m128i {
    unsafe { mem::transmute([e0, e1, e2, e3]) }
}

pub const fn _mm_set_epi16(
    e7: i16,
    e6: i16,
    e5: i16,
    e4: i16,
    e3: i16,
    e2: i16,
    e1: i16,
    e0: i16,
) -> __m128i {
    unsafe { mem::transmute([e0, e1, e2, e3, e4, e5, e6, e7]) }
}

pub const fn _mm_set_epi8(
    e15: i8,
    e14: i8,
    e13: i8,
    e12: i8,
    e11: i8,
    e10: i8,
    e9: i8,
    e8: i8,
    e7: i8,
    e6: i8,
    e5: i8,
    e4: i8,
    e3: i8,
    e2: i8,
    e1: i8,
    e0: i8,
) -> __m128i {
    unsafe {
        mem::transmute([
            e0, e1, e2, e3, e4, e5, e6, e7, e8, e9, e10, e11, e12, e13, e14, e15,
        ])
    }
}

pub const fn _mm_set1_epi64x(a: i64) -> __m128i {
    _mm_set_epi64x(a, a)
}

pub const fn _mm_set1_epi32(a: i32) -> __m128i {
    _mm_set_epi32(a, a, a, a)
}

pub const fn _mm_set1_epi16(a: i16) -> __m128i {
    _mm_set_epi16(a, a, a, a, a, a, a, a)
}
