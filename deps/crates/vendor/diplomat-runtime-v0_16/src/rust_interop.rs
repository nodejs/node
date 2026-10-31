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
use crate::DiplomatAbiCompatible;
use crate::DiplomatWrite;
use crate::DiplomatWriteGeneric;
use alloc::vec::Vec;
use core::borrow::Borrow;
use core::marker::PhantomData;
use core::mem::ManuallyDrop;
use core::ptr;

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

/// A [`DiplomatWriteGeneric<T>`] backed by a `Vec<T>`, for convenient use in Rust.
pub struct RustWriteVecGeneric<T: DiplomatAbiCompatible> {
    // Safety invariant: `inner.raw`'s `buf`, `len`, and `cap` correspond to a valid
    // `Vec<T>` allocation with `len = inner.raw.len / size_of::<T>()` and
    // `cap = inner.raw.cap / size_of::<T>()`.
    inner: DiplomatWriteGeneric<T>,
}

// Note: `this` must be `*mut DiplomatWrite` rather than `&mut DiplomatWrite` to match
// the `extern "C" fn(*mut DiplomatWrite, usize) -> bool` signature of `DiplomatWrite::grow`.
extern "C" fn grow_vec<T: DiplomatAbiCompatible>(
    this: *mut DiplomatWrite,
    new_cap_bytes: usize,
) -> bool {
    // SAFETY:
    // 1. `this` is a valid pointer to a `DiplomatWrite` constructed by `RustWriteVecGeneric::with_capacity`.
    // 2. `this.buf` was allocated by `Vec::<T>` with `old_len_elems` initialized elements and `old_cap_elems` capacity.
    unsafe {
        let this = this.as_mut().unwrap();
        let elem_size = core::mem::size_of::<T>();
        if elem_size == 0 {
            return true;
        }
        let new_cap_elems = new_cap_bytes.div_ceil(elem_size);
        let old_len_elems = this.len / elem_size;
        let old_cap_elems = this.cap / elem_size;

        let mut vec = Vec::<T>::from_raw_parts(this.buf as *mut T, old_len_elems, old_cap_elems);
        vec.reserve(new_cap_elems.saturating_sub(old_len_elems));
        this.cap = vec.capacity() * elem_size;
        this.buf = vec.as_mut_ptr() as *mut u8;
        core::mem::forget(vec);
    }
    true
}

impl<T: DiplomatAbiCompatible> RustWriteVecGeneric<T> {
    /// Creates a new [`RustWriteVecGeneric`] with the given initial element capacity.
    pub fn with_capacity(cap: usize) -> Self {
        extern "C" fn flush(_: *mut DiplomatWrite) {}
        let mut vec = Vec::<T>::with_capacity(cap);
        let raw = DiplomatWrite {
            context: ptr::null_mut(),
            buf: vec.as_mut_ptr() as *mut u8,
            len: 0,
            grow_failed: false,
            cap: vec.capacity() * core::mem::size_of::<T>(),
            flush,
            grow: grow_vec::<T>,
        };
        core::mem::forget(vec);
        Self {
            inner: DiplomatWriteGeneric {
                raw,
                _marker: PhantomData,
            },
        }
    }

    /// Borrows the underlying [`DiplomatWriteGeneric<T>`].
    #[allow(clippy::should_implement_trait)] // the trait is also implemented
    pub fn borrow(&self) -> &DiplomatWriteGeneric<T> {
        &self.inner
    }

    /// Mutably borrows the underlying [`DiplomatWriteGeneric<T>`].
    ///
    /// # Safety
    /// The contents of the returned reference MUST NOT be swapped with another instance
    /// of [`DiplomatWriteGeneric<T>`] that may have been created from a different source.
    pub unsafe fn borrow_mut(&mut self) -> &mut DiplomatWriteGeneric<T> {
        &mut self.inner
    }

    /// Returns a slice of the elements written so far.
    pub fn as_slice(&self) -> &[T] {
        self.inner.as_slice()
    }

    /// Consumes the wrapper and returns the filled `Vec<T>`.
    pub fn into_vec(self) -> Vec<T> {
        let this = ManuallyDrop::new(self);
        let elem_size = core::mem::size_of::<T>();
        let len_elems = this.inner.raw.len.checked_div(elem_size).unwrap_or(0);
        let cap_elems = this.inner.raw.cap.checked_div(elem_size).unwrap_or(0);
        // SAFETY:
        // 1. By `self.inner`'s safety invariant, `this.inner.raw.buf` was allocated by `Vec<T>`
        //    with `len_elems` initialized elements and `cap_elems` capacity.
        // 2. Wrapping `self` in `ManuallyDrop` prevents `Drop` from reconstructing and freeing
        //    the `Vec<T>` a second time.
        unsafe { Vec::<T>::from_raw_parts(this.inner.raw.buf as *mut T, len_elems, cap_elems) }
    }
}

impl<T: DiplomatAbiCompatible> Borrow<DiplomatWriteGeneric<T>> for RustWriteVecGeneric<T> {
    fn borrow(&self) -> &DiplomatWriteGeneric<T> {
        self.borrow()
    }
}

impl<T: DiplomatAbiCompatible> Drop for RustWriteVecGeneric<T> {
    fn drop(&mut self) {
        let elem_size = core::mem::size_of::<T>();
        let len_elems = self.inner.raw.len.checked_div(elem_size).unwrap_or(0);
        let cap_elems = self.inner.raw.cap.checked_div(elem_size).unwrap_or(0);
        // SAFETY: By `self.inner`'s safety invariant, `self.inner.raw.buf` was allocated by `Vec<T>`
        // with `len_elems` initialized elements and `cap_elems` capacity.
        unsafe {
            let _ = Vec::<T>::from_raw_parts(self.inner.raw.buf as *mut T, len_elems, cap_elems);
        }
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

    #[test]
    fn test_rust_write_generic() {
        let mut buffer = RustWriteVecGeneric::<u32>::with_capacity(2);
        // SAFETY: this is the only instance of `DiplomatWriteGeneric<u32>` in scope.
        let writer = unsafe { buffer.borrow_mut() };
        writer.push(10);
        writer.push(20);
        writer.push(30); // triggers grow
        writer.write_slice(&[40, 50]);
        writer.extend([60, 70]);
        assert_eq!(writer.len(), 7);
        assert_eq!(buffer.as_slice(), &[10, 20, 30, 40, 50, 60, 70]);
        assert_eq!(buffer.into_vec(), alloc::vec![10, 20, 30, 40, 50, 60, 70]);
    }

    #[test]
    fn test_rust_write_16() {
        let mut buffer = RustWriteVecGeneric::<u16>::with_capacity(2);
        // SAFETY: this is the only instance of `DiplomatWrite16` in scope.
        let writer: &mut crate::DiplomatWrite16 = unsafe { buffer.borrow_mut() };
        writer.write_char_as_utf16('A');
        writer.write_char_as_utf16('𐐷'); // surrogate pair: 0xD801, 0xDC37
        assert_eq!(writer.len(), 3);
        assert_eq!(buffer.as_slice(), &[0x0041, 0xD801, 0xDC37]);
        assert_eq!(
            alloc::string::String::from_utf16(buffer.as_slice()).unwrap(),
            "A𐐷"
        );
    }
}
