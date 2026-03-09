//! This module contains type aliases for C's platform-specific types
//! and fixed-width integer types.
//!
//! The platform-specific types definitions were taken from rust-lang/rust in
//! library/core/src/ffi/primitives.rs
//!
//! The fixed-width integer aliases are deprecated: use the Rust types instead.

pub type c_schar = i8;
pub type c_uchar = u8;
pub type c_short = i16;
pub type c_ushort = u16;

pub type c_longlong = i64;
pub type c_ulonglong = u64;

pub type c_float = f32;
pub type c_double = f64;

cfg_if! {
    if #[cfg(all(
        not(windows),
        not(target_vendor = "apple"),
        not(target_os = "vita"),
        any(
            target_arch = "aarch64",
            target_arch = "arm",
            target_arch = "csky",
            target_arch = "hexagon",
            target_arch = "msp430",
            target_arch = "powerpc",
            target_arch = "powerpc64",
            target_arch = "riscv32",
            target_arch = "riscv64",
            target_arch = "s390x",
            target_arch = "xtensa",
        )
    ))] {
        pub type c_char = u8;
    } else {
        // On every other target, c_char is signed.
        pub type c_char = i8;
    }
}

cfg_if! {
    if #[cfg(any(target_arch = "avr", target_arch = "msp430"))] {
        pub type c_int = i16;
        pub type c_uint = u16;
    } else {
        pub type c_int = i32;
        pub type c_uint = u32;
    }
}

cfg_if! {
    if #[cfg(all(target_pointer_width = "64", not(windows)))] {
        pub type c_long = i64;
        pub type c_ulong = u64;
    } else {
        // The minimal size of `long` in the C standard is 32 bits
        pub type c_long = i32;
        pub type c_ulong = u32;
    }
}

#[deprecated(since = "0.2.55", note = "Use i8 instead.")]
pub type int8_t = i8;
#[deprecated(since = "0.2.55", note = "Use i16 instead.")]
pub type int16_t = i16;
#[deprecated(since = "0.2.55", note = "Use i32 instead.")]
pub type int32_t = i32;
#[deprecated(since = "0.2.55", note = "Use i64 instead.")]
pub type int64_t = i64;
#[deprecated(since = "0.2.55", note = "Use u8 instead.")]
pub type uint8_t = u8;
#[deprecated(since = "0.2.55", note = "Use u16 instead.")]
pub type uint16_t = u16;
#[deprecated(since = "0.2.55", note = "Use u32 instead.")]
pub type uint32_t = u32;
#[deprecated(since = "0.2.55", note = "Use u64 instead.")]
pub type uint64_t = u64;

cfg_if! {
    if #[cfg(all(target_arch = "aarch64", not(target_os = "windows")))] {
        /// C `__int128` (a GCC extension that's part of many ABIs)
        pub type __int128 = i128;
        /// C `unsigned __int128` (a GCC extension that's part of many ABIs)
        pub type __uint128 = u128;
        /// C __int128_t (alternate name for [__int128][])
        pub type __int128_t = i128;
        /// C __uint128_t (alternate name for [__uint128][])
        pub type __uint128_t = u128;
    }
}
