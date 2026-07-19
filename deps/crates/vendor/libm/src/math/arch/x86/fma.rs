//! Use assembly fma if the `fma` or `fma4` feature is detected at runtime.

use core::arch::asm;

use super::super::super::generic;
use super::detect::{cpu_flags, get_cpu_features};
use crate::support::Round;
use crate::support::feature_detect::select_once;

pub fn fma(x: f64, y: f64, z: f64) -> f64 {
    select_once! {
        sig: fn(x: f64, y: f64, z: f64) -> f64,
        init: || {
            let features = get_cpu_features();
            if features.contains(cpu_flags::FMA) {
                fma_with_fma
            } else if features.contains(cpu_flags::FMA4) {
               fma_with_fma4
            } else {
                fma_fallback as Func
            }
        },
        // SAFETY: `fn_ptr` is the result of `init`, preconditions have been checked.
        call: |fn_ptr: Func| unsafe { fn_ptr(x, y, z) },
    }
}

pub fn fmaf(x: f32, y: f32, z: f32) -> f32 {
    select_once! {
        sig: fn(x: f32, y: f32, z: f32) -> f32,
        init: || {
            let features = get_cpu_features();
            if features.contains(cpu_flags::FMA) {
                fmaf_with_fma
            } else if features.contains(cpu_flags::FMA4) {
                fmaf_with_fma4
            } else {
                fmaf_fallback as Func
            }
        },
        // SAFETY: `fn_ptr` is the result of `init`, preconditions have been checked.
        call: |fn_ptr: Func| unsafe { fn_ptr(x, y, z) },
    }
}

/// # Safety
///
/// Must have +fma available.
unsafe fn fma_with_fma(mut x: f64, y: f64, z: f64) -> f64 {
    debug_assert!(get_cpu_features().contains(cpu_flags::FMA));

    // SAFETY: fma is asserted available by precondition, which provides the instruction. No
    // memory access or side effects.
    unsafe {
        asm!(
            "vfmadd213sd {x}, {y}, {z}",
            x = inout(xmm_reg) x,
            y = in(xmm_reg) y,
            z = in(xmm_reg) z,
            options(nostack, nomem, pure),
        );
    }
    x
}

/// # Safety
///
/// Must have +fma available.
unsafe fn fmaf_with_fma(mut x: f32, y: f32, z: f32) -> f32 {
    debug_assert!(get_cpu_features().contains(cpu_flags::FMA));

    // SAFETY: fma is asserted available by precondition, which provides the instruction. No
    // memory access or side effects.
    unsafe {
        asm!(
            "vfmadd213ss {x}, {y}, {z}",
            x = inout(xmm_reg) x,
            y = in(xmm_reg) y,
            z = in(xmm_reg) z,
            options(nostack, nomem, pure),
        );
    }
    x
}

/// # Safety
///
/// Must have +fma4 available.
unsafe fn fma_with_fma4(mut x: f64, y: f64, z: f64) -> f64 {
    debug_assert!(get_cpu_features().contains(cpu_flags::FMA4));

    // SAFETY: fma4 is asserted available by precondition, which provides the instruction. No
    // memory access or side effects.
    unsafe {
        asm!(
            "vfmaddsd {x}, {x}, {y}, {z}",
            x = inout(xmm_reg) x,
            y = in(xmm_reg) y,
            z = in(xmm_reg) z,
            options(nostack, nomem, pure),
        );
    }
    x
}

/// # Safety
///
/// Must have +fma4 available.
unsafe fn fmaf_with_fma4(mut x: f32, y: f32, z: f32) -> f32 {
    debug_assert!(get_cpu_features().contains(cpu_flags::FMA4));

    // SAFETY: fma4 is asserted available by precondition, which provides the instruction. No
    // memory access or side effects.
    unsafe {
        asm!(
            "vfmaddss {x}, {x}, {y}, {z}",
            x = inout(xmm_reg) x,
            y = in(xmm_reg) y,
            z = in(xmm_reg) z,
            options(nostack, nomem, pure),
        );
    }
    x
}

// FIXME: the `select_implementation` macro should handle arch implementations that want
// to use the fallback, so we don't need to recreate the body.

fn fma_fallback(x: f64, y: f64, z: f64) -> f64 {
    generic::fma_round(x, y, z, Round::Nearest).val
}

fn fmaf_fallback(x: f32, y: f32, z: f32) -> f32 {
    generic::fma_wide_round(x, y, z, Round::Nearest).val
}
