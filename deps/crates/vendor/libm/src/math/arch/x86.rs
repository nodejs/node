//! Architecture-specific support for x86-32 and x86-64 with SSE2

mod detect;
mod fma;

pub use fma::{fma, fmaf};

pub fn sqrtf(mut x: f32) -> f32 {
    // SAFETY: `sqrtss` is part of `sse2`, which this module is gated behind. It has no memory
    // access or side effects.
    unsafe {
        core::arch::asm!(
            "sqrtss {x}, {x}",
            x = inout(xmm_reg) x,
            options(nostack, nomem, pure),
        )
    };
    x
}

pub fn sqrt(mut x: f64) -> f64 {
    // SAFETY: `sqrtsd` is part of `sse2`, which this module is gated behind. It has no memory
    // access or side effects.
    unsafe {
        core::arch::asm!(
            "sqrtsd {x}, {x}",
            x = inout(xmm_reg) x,
            options(nostack, nomem, pure),
        )
    };
    x
}
