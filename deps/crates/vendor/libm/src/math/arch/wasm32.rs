//! Wasm has builtins for simple float operations. Use the unstable `core::arch` intrinsics which
//! are significantly faster than soft float operations.

pub fn ceil(x: f64) -> f64 {
    core::arch::wasm32::f64_ceil(x)
}

pub fn ceilf(x: f32) -> f32 {
    core::arch::wasm32::f32_ceil(x)
}

pub fn fabs(x: f64) -> f64 {
    x.abs()
}

pub fn fabsf(x: f32) -> f32 {
    x.abs()
}

pub fn floor(x: f64) -> f64 {
    core::arch::wasm32::f64_floor(x)
}

pub fn floorf(x: f32) -> f32 {
    core::arch::wasm32::f32_floor(x)
}

pub fn rint(x: f64) -> f64 {
    core::arch::wasm32::f64_nearest(x)
}

pub fn rintf(x: f32) -> f32 {
    core::arch::wasm32::f32_nearest(x)
}

pub fn sqrt(x: f64) -> f64 {
    core::arch::wasm32::f64_sqrt(x)
}

pub fn sqrtf(x: f32) -> f32 {
    core::arch::wasm32::f32_sqrt(x)
}

pub fn trunc(x: f64) -> f64 {
    core::arch::wasm32::f64_trunc(x)
}

pub fn truncf(x: f32) -> f32 {
    core::arch::wasm32::f32_trunc(x)
}
