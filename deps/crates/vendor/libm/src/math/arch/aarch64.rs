//! Architecture-specific support for aarch64 with neon.

use core::arch::asm;

pub fn fma(mut x: f64, y: f64, z: f64) -> f64 {
    // SAFETY: `fmadd` is available with neon and has no side effects.
    unsafe {
        asm!(
            "fmadd {x:d}, {x:d}, {y:d}, {z:d}",
            x = inout(vreg) x,
            y = in(vreg) y,
            z = in(vreg) z,
            options(nomem, nostack, pure)
        );
    }
    x
}

pub fn fmaf(mut x: f32, y: f32, z: f32) -> f32 {
    // SAFETY: `fmadd` is available with neon and has no side effects.
    unsafe {
        asm!(
            "fmadd {x:s}, {x:s}, {y:s}, {z:s}",
            x = inout(vreg) x,
            y = in(vreg) y,
            z = in(vreg) z,
            options(nomem, nostack, pure)
        );
    }
    x
}

pub fn rint(mut x: f64) -> f64 {
    // SAFETY: `frintn` is available with neon and has no side effects.
    //
    // `frintn` is always round-to-nearest which does not match the C specification, but Rust does
    // not support rounding modes.
    unsafe {
        asm!(
            "frintn {x:d}, {x:d}",
            x = inout(vreg) x,
            options(nomem, nostack, pure)
        );
    }
    x
}

pub fn rintf(mut x: f32) -> f32 {
    // SAFETY: `frintn` is available with neon and has no side effects.
    //
    // `frintn` is always round-to-nearest which does not match the C specification, but Rust does
    // not support rounding modes.
    unsafe {
        asm!(
            "frintn {x:s}, {x:s}",
            x = inout(vreg) x,
            options(nomem, nostack, pure)
        );
    }
    x
}

#[cfg(all(f16_enabled, target_feature = "fp16"))]
pub fn rintf16(mut x: f16) -> f16 {
    // SAFETY: `frintn` is available for `f16` with `fp16` (implies `neon`) and has no side effects.
    //
    // `frintn` is always round-to-nearest which does not match the C specification, but Rust does
    // not support rounding modes.
    unsafe {
        asm!(
            "frintn {x:h}, {x:h}",
            x = inout(vreg) x,
            options(nomem, nostack, pure)
        );
    }
    x
}

pub fn sqrt(mut x: f64) -> f64 {
    // SAFETY: `fsqrt` is available with neon and has no side effects.
    unsafe {
        asm!(
            "fsqrt {x:d}, {x:d}",
            x = inout(vreg) x,
            options(nomem, nostack, pure)
        );
    }
    x
}

pub fn sqrtf(mut x: f32) -> f32 {
    // SAFETY: `fsqrt` is available with neon and has no side effects.
    unsafe {
        asm!(
            "fsqrt {x:s}, {x:s}",
            x = inout(vreg) x,
            options(nomem, nostack, pure)
        );
    }
    x
}

#[cfg(all(f16_enabled, target_feature = "fp16"))]
pub fn sqrtf16(mut x: f16) -> f16 {
    // SAFETY: `fsqrt` is available for `f16` with `fp16` (implies `neon`) and has no
    // side effects.
    unsafe {
        asm!(
            "fsqrt {x:h}, {x:h}",
            x = inout(vreg) x,
            options(nomem, nostack, pure)
        );
    }
    x
}
