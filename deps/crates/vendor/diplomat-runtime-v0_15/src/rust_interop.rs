//! Types for interfacing with Diplomat FFI APIs from Rust.
//!
//! By and large, Diplomat helps you produce a Rust-written library that can be called from other
//! languages. You write the library in Rust, but interact with the Diplomat-wrapped library
//! in a non-Rust language.
//!
//! However, either for debugging purposes, or for extending the library in custom ways, you may find
//! yourself wanting to work with the Diplomat-wrapped library from Rust. This module contains
//! utilities for doing that.

use crate::diplomat_buffer_write_create;
use crate::diplomat_buffer_write_destroy;
use crate::DiplomatWrite;
use core::borrow::Borrow;

/// A [`DiplomatWrite`] backed by a `Vec`, for convenient use in Rust.
pub struct RustWriteVec {
    /// Safety Invariant: must have been created by diplomat_buffer_write_create()
    ptr: *mut DiplomatWrite,
}

impl RustWriteVec {
    /// Creates a new [`RustWriteVec`] with the given initial buffer capacity.
    pub fn with_capacity(cap: usize) -> Self {
        Self {
            ptr: diplomat_buffer_write_create(cap),
        }
    }

    /// Borrows the underlying [`DiplomatWrite`].
    #[allow(clippy::should_implement_trait)] // the trait is also implemented
    pub fn borrow(&self) -> &DiplomatWrite {
        // Safety: the pointer is valid because the Drop impl hasn't been called yet.
        unsafe { &*self.ptr }
    }

    /// Mutably borrows the underlying [`DiplomatWrite`].
    ///
    /// # Safety
    /// The contents of the returned pointer MUST NOT be swapped with another instance
    /// of [`DiplomatWrite`] that may have been created from a different source. This
    /// requirement is satisfied if this is the only instance of [`DiplomatWrite`]
    /// in scope.
    ///
    /// For more information, see [`DiplomatWrite`].
    pub unsafe fn borrow_mut(&mut self) -> &mut DiplomatWrite {
        // Safety: the pointer is valid because the Drop impl hasn't been called yet.
        unsafe { &mut *self.ptr }
    }
}

impl Borrow<DiplomatWrite> for RustWriteVec {
    fn borrow(&self) -> &DiplomatWrite {
        self.borrow()
    }
}

impl Drop for RustWriteVec {
    fn drop(&mut self) {
        // Safety: by invariant, ptr was created by diplomat_buffer_write_create()
        unsafe { diplomat_buffer_write_destroy(self.ptr) }
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use core::fmt::Write;

    #[test]
    fn test_rust_write() {
        let mut buffer = RustWriteVec::with_capacity(5);
        // Safety: this is the only instance of `DiplomatWrite` in scope.
        unsafe { buffer.borrow_mut() }
            .write_str("Hello World")
            .unwrap();
        assert_eq!(buffer.borrow().as_bytes(), b"Hello World");
    }
}
