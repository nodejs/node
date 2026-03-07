//! Best-effort atomic types
//!
//! This module exports `RadiumType` aliases that map to the `AtomicType` on
//! targets that have it, or `Cell<type>` on targets that do not. This alias can
//! be used as a consistent name for crates that need portable names for
//! non-portable types.

/// Best-effort atomic `bool` type.
pub type RadiumBool = if_atomic! {
    if atomic(bool) { core::sync::atomic::AtomicBool }
    else { core::cell::Cell<bool> }
};

/// Best-effort atomic `i8` type.
pub type RadiumI8 = if_atomic! {
    if atomic(8) { core::sync::atomic::AtomicI8 }
    else { core::cell::Cell<i8> }
};

/// Best-effort atomic `u8` type.
pub type RadiumU8 = if_atomic! {
    if atomic(8) { core::sync::atomic::AtomicU8 }
    else { core::cell::Cell<u8> }
};

/// Best-effort atomic `i16` type.
pub type RadiumI16 = if_atomic! {
    if atomic(16) { core::sync::atomic::AtomicI16 }
    else { core::cell::Cell<i16> }
};

/// Best-effort atomic `u16` type.
pub type RadiumU16 = if_atomic! {
    if atomic(16) { core::sync::atomic::AtomicU16 }
    else { core::cell::Cell<u16> }
};

/// Best-effort atomic `i32` type.
pub type RadiumI32 = if_atomic! {
    if atomic(32) { core::sync::atomic::AtomicI32 }
    else { core::cell::Cell<i32> }
};

/// Best-effort atomic `u32` type.
pub type RadiumU32 = if_atomic! {
    if atomic(32) { core::sync::atomic::AtomicU32 }
    else { core::cell::Cell<u32> }
};

/// Best-effort atomic `i64` type.
pub type RadiumI64 = if_atomic! {
    if atomic(64) { core::sync::atomic::AtomicI64 }
    else { core::cell::Cell<i64> }
};

/// Best-effort atomic `u64` type.
pub type RadiumU64 = if_atomic! {
    if atomic(64) { core::sync::atomic::AtomicU64 }
    else { core::cell::Cell<u64> }
};

/// Best-effort atomic `isize` type.
pub type RadiumIsize = if_atomic! {
    if atomic(size) { core::sync::atomic::AtomicIsize }
    else { core::cell::Cell<isize> }
};

/// Best-effort atomic `usize` type.
pub type RadiumUsize = if_atomic! {
    if atomic(size) { core::sync::atomic::AtomicUsize }
    else { core::cell::Cell<usize> }
};

/// Best-effort atomic pointer type.
pub type RadiumPtr<T> = if_atomic! {
    if atomic(ptr) { core::sync::atomic::AtomicPtr<T> }
    else { core::cell::Cell<*mut T> }
};
