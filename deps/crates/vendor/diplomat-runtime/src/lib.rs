#![cfg_attr(not(any(target_arch = "wasm32")), no_std)]

//! The [Diplomat](https://rust-diplomat.github.io/diplomat/) runtime crate.
//!
//! This crate provides Diplomat-specific types for crates writing `#[diplomat::bridge]` modules.
//! Include this in any crate that also depends on `diplomat`, since `#[diplomat::bridge]`
//! will generate code that relies on types from here.
//!
//! This crate contains a fair number of `DiplomatFoo` types. Some, like
//! [`DiplomatOption`], are FFI-safe versions of Rust types that can be put in
//! a struct and passed safely over FFI. Others, like [`DiplomatChar`], are simple
//! type aliases that signal to the Diplomat tool that particular semantics are desired
//! on the C++ side.
//!
//! # Features
//!
//! The `log` feature enables logging support, currently enabled via the wasm-only `diplomat_init()`.
//!
//! The `jvm-callback-support` feature should be enabled if building Diplomat for use in the JVM, for
//! a Diplomat-based library that uses callbacks.
extern crate alloc;

use alloc::alloc::Layout;

#[cfg(all(target_arch = "wasm32", target_os = "unknown"))]
mod wasm_glue;

mod write;
pub use write::DiplomatWrite;
pub use write::{diplomat_buffer_write_create, diplomat_buffer_write_destroy};
mod slices;
pub use slices::{
    DiplomatOwnedSlice, DiplomatOwnedStr16Slice, DiplomatOwnedStrSlice, DiplomatOwnedUTF8StrSlice,
    DiplomatSlice, DiplomatSliceMut, DiplomatStr16Slice, DiplomatStrSlice, DiplomatUtf8StrSlice,
};

mod callback;
pub use callback::DiplomatCallback;

mod result;
pub use result::{DiplomatOption, DiplomatResult};

pub mod rust_interop;

/// Like [`char`], but unvalidated.
///
/// This type will usually map to some character type in the target language, and
/// you will not need to worry about the safety of mismatched string invariants.
pub type DiplomatChar = u32;

/// Like [`str`], but unvalidated.
///
/// This is a dynamically sized type, it should be used behind an `&` or a `Box<T>`
///
/// This type will usually map to some string type in the target language, and
/// you will not need to worry about the safety of mismatched string invariants.
pub type DiplomatStr = [u8];

/// Like `Wstr`, but unvalidated.
///
/// This is a dynamically sized type, it should be used behind an `&` or a `Box<T>`
///
/// This type will usually map to some string type in the target language, and
/// you will not need to worry about the safety of mismatched string invariants.
pub type DiplomatStr16 = [u16];

/// Like [`u8`], but interpreted explicitly as a raw byte as opposed to a numerical value.
///
/// This matters for languages like JavaScript or Dart, where there's only a single numeric
/// type, but special types for byte buffers.
pub type DiplomatByte = u8;

/// Allocates a buffer of a given size in Rust's memory.
///
/// Primarily to be called by generated FFI bindings, not Rust code, but is available if needed.
///
/// # Safety
/// - The allocated buffer must be freed with [`diplomat_free()`].
#[no_mangle]
pub unsafe extern "C" fn diplomat_alloc(size: usize, align: usize) -> *mut u8 {
    alloc::alloc::alloc(Layout::from_size_align(size, align).unwrap())
}

/// Frees a buffer that was allocated in Rust's memory.
///
/// Primarily to be called by generated FFI bindings, not Rust code, but is available if needed.
///
/// # Safety
/// - `ptr` must be a pointer to a valid buffer allocated by [`diplomat_alloc()`].
#[no_mangle]
pub unsafe extern "C" fn diplomat_free(ptr: *mut u8, size: usize, align: usize) {
    alloc::alloc::dealloc(ptr, Layout::from_size_align(size, align).unwrap())
}

/// Whether a `&[u8]` is a `&str`.
///
/// Primarily to be called by generated FFI bindings, not Rust code, but is available if needed.
///
/// # Safety
/// - `ptr` and `size` must be a valid `&[u8]`
#[no_mangle]
pub unsafe extern "C" fn diplomat_is_str(ptr: *const u8, size: usize) -> bool {
    core::str::from_utf8(core::slice::from_raw_parts(ptr, size)).is_ok()
}
