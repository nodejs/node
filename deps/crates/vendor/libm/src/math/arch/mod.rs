//! Architecture-specific routines and operations.
//!
//! LLVM will already optimize calls to some of these in cases that there are hardware
//! instructions. Providing an implementation here just ensures that the faster implementation
//! is used when calling the function directly. This helps anyone who uses `libm` directly, as
//! well as improving things when these routines are called as part of other implementations.

// Most implementations should be defined here, to ensure they are not made available when
// soft floats are required.
#[cfg(arch_enabled)]
cfg_if! {
    if #[cfg(all(target_arch = "wasm32", intrinsics_enabled))] {
        mod wasm32;
        pub use wasm32::{
            ceil, ceilf, fabs, fabsf, floor, floorf, rint, rintf, sqrt, sqrtf, trunc, truncf,
        };
    } else if #[cfg(target_feature = "sse2")] {
        mod x86;
        pub use x86::{sqrt, sqrtf, fma, fmaf};
    } else if #[cfg(all(
        any(target_arch = "aarch64", target_arch = "arm64ec"),
        target_feature = "neon"
    ))] {
        mod aarch64;

        pub use aarch64::{
            fma,
            fmaf,
            rint,
            rintf,
            sqrt,
            sqrtf,
        };

        #[cfg(all(f16_enabled, target_feature = "fp16"))]
        pub use aarch64::{
            rintf16,
            sqrtf16,
        };
    }
}

// There are certain architecture-specific implementations that are needed for correctness
// even with `force-soft-float`. These are configured here.
cfg_if! {
    if #[cfg(all(target_arch = "x86", not(target_feature = "sse2")))] {
        mod i586;
        pub use i586::{ceil, floor};
    }
}
