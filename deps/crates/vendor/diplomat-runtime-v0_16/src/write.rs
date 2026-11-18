use alloc::boxed::Box;
use alloc::vec::Vec;
use core::ffi::c_void;
use core::{fmt, ptr};

/// An object that can one can write UTF-8 strings to
///
/// This allows the C API to write to arbitrary kinds of objects, for example a
/// C++ std::string or a char buffer.
///
/// The way to use this object is to fill out the `buf`, `len`, `cap` fields with
/// appropriate values for the buffer, its current length, and its current capacity,
/// and `flush` and `grow` with appropriate callbacks (using `context` to reference any
/// state they need). This object will be passed by mutable reference to the Rust side,
/// and Rust will write to it, calling `grow()` as necessary. Once done, it will call `flush()`
/// to update any state on `context` (e.g. adding a null terminator, updating the length).
/// The object on the foreign side will be directly usable after this, the foreign side
/// need not perform additional state updates after passing an [`DiplomatWrite`] to
/// a function.
///
/// May be extended in the future to support further invariants
///
/// DiplomatWrite will not perform any cleanup on `context` or `buf`, these are logically
/// "borrows" from the FFI side.
///
/// # DiplomatWrite is Polymorphic
///
/// Instances of [`DiplomatWrite`] can be created from multiple different sources.
/// There are two constructors available in `diplomat_runtime`:
///
/// 1. [`diplomat_simple_write()`] to write to a fixed-size buffer.
/// 2. [`diplomat_buffer_write_create()`] to write to a Vec allocated by Rust.
///    A wrapper is provided: [`RustWriteVec`](crate::rust_interop::RustWriteVec).
///
/// Backends may have additional constructors for writing to various shapes of buffer.
///
/// ⚠️ Because [`DiplomatWrite`] is polymorphic, the destructor must know how the instance
/// was constructed. It is therefore unsound to use functions such as [`core::mem::swap`]
/// on instances of [`DiplomatWrite`] with potentially different sources. For example,
/// the following code is not safe:
///
/// ```no_run
/// use diplomat_runtime::DiplomatWrite;
/// fn bad(write: &mut DiplomatWrite) {
///   let mut some_other_write: DiplomatWrite = unimplemented!();
///   // Not safe! The two `DiplomatWrite`s have different invariants
///   core::mem::swap(write, &mut some_other_write);
/// }
/// ```
///
/// As a result, any function that returns an owned `DiplomatWrite` or a `&mut DiplomatWrite`
/// must be `unsafe`. For an example, see [`RustWriteVec::borrow_mut`](crate::rust_interop::RustWriteVec::borrow_mut).
///
/// Diplomat backends guarantee they will only ever hand the same type of `DiplomatWrite` object to Rust
/// code; this is only something you need to worry about if using [`RustWriteVec`](crate::rust_interop::RustWriteVec),
/// or `DiplomatWrite` objects manually created in Rust via APIs like `diplomat_simple_write`.
///
/// # Safety invariants:
///  - `flush()` and `grow()` will be passed `self` including `context` and it should always be safe to do so.
///    `context` may be  null, however `flush()` and `grow()` must then be ready to receive it as such.
///  - `buf` must be a valid pointer to `cap` bytes of memory
///  - `buf` must point to `len` consecutive properly initialized bytes
///  - `cap` must be less than or equal to isize::MAX
///  - `grow()` must either return false or update `buf` and `cap` for a valid buffer
///    of at least the requested buffer size.
///  - If grow_failed is true all safety invariants on buf/cap/len MUST still hold.
///  - `DiplomatWrite::flush()` will be automatically called by Diplomat. `flush()` might also be called
///    (erroneously) on the Rust side (it's a public method), so it must be idempotent.
#[repr(C)]
pub struct DiplomatWrite {
    /// Context pointer for additional data needed by `grow()` and `flush()`. May be `null`.
    ///
    /// The pointer may reference structured data on the foreign side,
    /// such as C++ std::string, used to reallocate buf.
    pub(crate) context: *mut c_void,
    /// The raw string buffer, which will be mutated on the Rust side.
    pub(crate) buf: *mut u8,
    /// The current filled size of the buffer
    pub(crate) len: usize,
    /// The current capacity of the buffer
    pub(crate) cap: usize,
    /// Set to true if `grow` ever fails.
    pub(crate) grow_failed: bool,
    /// Called by Rust to indicate that there is no more data to write.
    ///
    /// May be called multiple times.
    ///
    /// Arguments:
    /// - `self` (`*mut DiplomatWrite`): This `DiplomatWrite`
    pub(crate) flush: extern "C" fn(*mut DiplomatWrite),
    /// Called by Rust to request more capacity in the buffer. The implementation should allocate a new
    /// buffer and copy the contents of the old buffer into the new buffer, updating `self.buf` and `self.cap`
    ///
    /// Arguments:
    /// - `self` (`*mut DiplomatWrite`): This `DiplomatWrite`
    /// - `capacity` (`usize`): The requested capacity.
    ///
    /// Returns: `true` if the allocation succeeded. Should not update any state if it failed.
    pub(crate) grow: extern "C" fn(*mut DiplomatWrite, usize) -> bool,
}

impl DiplomatWrite {
    /// Call this function before releasing the buffer to C
    pub fn flush(&mut self) {
        (self.flush)(self);
    }

    /// Returns a pointer to the buffer's bytes.
    ///
    /// If growth has failed, this returns what has been written so far.
    pub fn as_bytes(&self) -> &[u8] {
        if self.buf.is_null() {
            return &[];
        }
        debug_assert!(self.len <= self.cap);
        // Safety checklist, assuming this struct's safety invariants:
        // 1. `buf` is a valid pointer and not null
        // 2. `buf` points to `len` consecutive properly initialized bytes
        // 3. `buf` won't be mutated because it is only directly accessible via
        //    `diplomat_buffer_write_get_bytes`, whose safety invariant states
        //    that the bytes cannot be mutated while borrowed
        //    can only be dereferenced using unsafe code
        // 4. `buf`'s total size is no larger than isize::MAX
        unsafe { core::slice::from_raw_parts(self.buf, self.len) }
    }
}
impl fmt::Write for DiplomatWrite {
    fn write_str(&mut self, s: &str) -> Result<(), fmt::Error> {
        if self.grow_failed {
            return Ok(());
        }
        let needed_len = self.len + s.len();
        if needed_len > self.cap {
            let success = (self.grow)(self, needed_len);
            if !success {
                self.grow_failed = true;
                return Ok(());
            }
        }
        debug_assert!(needed_len <= self.cap);
        unsafe {
            ptr::copy_nonoverlapping(s.as_bytes().as_ptr(), self.buf.add(self.len), s.len());
        }
        self.len = needed_len;
        Ok(())
    }
}

/// Create an `DiplomatWrite` that can write to a fixed-length stack allocated `u8` buffer.
///
/// Once done, this will append a null terminator to the written string.
///
/// This is largely used internally by Diplomat-generated FFI code, and should not need to be constructed
/// manually outside of that context. See [`RustWriteVec`](crate::rust_interop::RustWriteVec) if you need this in Rust.
///
/// # Safety
///
///  - `buf` must be a valid pointer to a region of memory that can hold at `buf_size` bytes
#[no_mangle]
pub unsafe extern "C" fn diplomat_simple_write(buf: *mut u8, buf_size: usize) -> DiplomatWrite {
    extern "C" fn grow(_this: *mut DiplomatWrite, _cap: usize) -> bool {
        false
    }
    extern "C" fn flush(this: *mut DiplomatWrite) {
        unsafe {
            debug_assert!((*this).len <= (*this).cap);
            let buf = (*this).buf;
            ptr::write(buf.add((*this).len), 0)
        }
    }
    DiplomatWrite {
        context: ptr::null_mut(),
        buf,
        len: 0,
        grow_failed: false,
        // keep an extra byte in our pocket for the null terminator
        cap: buf_size - 1,
        flush,
        grow,
    }
}

/// Create an [`DiplomatWrite`] that can write to a dynamically allocated buffer managed by Rust.
///
/// Use [`diplomat_buffer_write_destroy()`] to free the writable and its underlying buffer.
/// The pointer is valid until that function is called.
///
/// This is largely used internally by Diplomat-generated FFI code, and should not need to be constructed
/// manually outside of that context. See [`RustWriteVec`](crate::rust_interop::RustWriteVec) if you need this in Rust.
///
/// The grow impl never sets `grow_failed`, although it is possible for it to panic.
#[no_mangle]
pub extern "C" fn diplomat_buffer_write_create(cap: usize) -> *mut DiplomatWrite {
    extern "C" fn grow(this: *mut DiplomatWrite, new_cap: usize) -> bool {
        unsafe {
            let this = this.as_mut().unwrap();
            let mut vec = Vec::from_raw_parts(this.buf, 0, this.cap);
            vec.reserve(new_cap);
            this.cap = vec.capacity();
            this.buf = vec.as_mut_ptr();
            core::mem::forget(vec);
        }
        true
    }

    extern "C" fn flush(_: *mut DiplomatWrite) {}

    let mut vec = Vec::<u8>::with_capacity(cap);
    let ret = DiplomatWrite {
        context: ptr::null_mut(),
        buf: vec.as_mut_ptr(),
        len: 0,
        grow_failed: false,
        cap,
        flush,
        grow,
    };

    core::mem::forget(vec);
    Box::into_raw(Box::new(ret))
}

/// Grabs a pointer to the underlying buffer of a writable.
///
/// Returns null if there was an allocation error during the write construction.
///
/// # Safety
/// - The returned pointer is valid until the passed writable is destroyed.
/// - The returned pointer is valid for both reads and writes, however Rust code
///   may not write to it if `this` is being accessed by other methods simultaneously.
/// - `this` must be a pointer to a valid [`DiplomatWrite`] constructed by
///   [`diplomat_buffer_write_create()`].
#[no_mangle]
pub extern "C" fn diplomat_buffer_write_get_bytes(this: *mut DiplomatWrite) -> *mut u8 {
    let this = unsafe { &*this };
    if this.grow_failed {
        core::ptr::null_mut()
    } else {
        this.buf
    }
}

/// Gets the length in bytes of the content written to the writable.
///
/// Returns 0 if there was an allocation error during the write construction.
///
/// # Safety
/// - `this` must be a pointer to a valid [`DiplomatWrite`] constructed by
///   [`diplomat_buffer_write_create()`].
#[no_mangle]
pub extern "C" fn diplomat_buffer_write_len(this: &DiplomatWrite) -> usize {
    if this.grow_failed {
        0
    } else {
        this.len
    }
}

/// Destructor for Rust-memory backed writables.
///
/// # Safety
/// - `this` must be a pointer to a valid [`DiplomatWrite`] constructed by
///   [`diplomat_buffer_write_create()`].
#[no_mangle]
pub unsafe extern "C" fn diplomat_buffer_write_destroy(this: *mut DiplomatWrite) {
    let this = Box::from_raw(this);
    let vec = Vec::from_raw_parts(this.buf, 0, this.cap);
    drop(vec);
    drop(this);
}

mod sealed {
    pub trait Sealed {}
}

/// Marker trait for types that have a stable, shared C ABI layout and can be written
/// directly into a [`DiplomatWriteGeneric`].
///
/// This trait is currently sealed and implemented only for scalar primitive types.
///
/// # Safety
/// Implementors must:
/// - Be `Copy` and Plain Old Data (POD).
/// - Have a fixed, shared layout across the FFI boundary.
/// - Contain no pointers, references, or custom `Drop` glue.
pub unsafe trait DiplomatAbiCompatible: Copy + sealed::Sealed {}

macro_rules! impl_abi_compatible {
    ($($ty:ty),* $(,)?) => {
        $(
            impl sealed::Sealed for $ty {}
            // SAFETY: Scalar primitives are Copy, POD, have no pointers or Drop glue,
            // and share a fixed C ABI layout across FFI.
            unsafe impl DiplomatAbiCompatible for $ty {}
        )*
    };
}

impl_abi_compatible!(i8, u8, i16, u16, i32, u32, i64, u64, i128, u128, isize, usize, f32, f64);

/// An object that can write a sequence of primitive values of type `T` across FFI.
///
/// This wraps an underlying [`DiplomatWrite`] with a type-safe API for pushing elements,
/// writing slices, and extending from iterators.
///
/// # Safety invariants
/// - All safety invariants of [`DiplomatWrite`] hold on `self.raw`.
/// - `self.raw.len` and `self.raw.cap` are measured in bytes, and `self.raw.len` is always
///   a multiple of `size_of::<T>()`.
/// - `self.raw.buf` points to `self.raw.len / size_of::<T>()` consecutive properly initialized
///   values of `T`.
#[repr(transparent)]
pub struct DiplomatWriteGeneric<T: DiplomatAbiCompatible> {
    pub(crate) raw: DiplomatWrite,
    pub(crate) _marker: core::marker::PhantomData<T>,
}

impl<T: DiplomatAbiCompatible> DiplomatWriteGeneric<T> {
    /// Pushes a single element into the buffer.
    #[inline]
    pub fn push(&mut self, val: T) {
        if self.raw.grow_failed {
            return;
        }
        let elem_size = core::mem::size_of::<T>();
        if elem_size == 0 {
            return;
        }
        let Some(needed_bytes) = self.raw.len.checked_add(elem_size) else {
            self.raw.grow_failed = true;
            return;
        };
        if needed_bytes > self.raw.cap {
            let success = (self.raw.grow)(&mut self.raw, needed_bytes);
            if !success {
                self.raw.grow_failed = true;
                return;
            }
        }
        debug_assert!(needed_bytes <= self.raw.cap);
        // SAFETY:
        // 1. `self.raw.buf` is valid for at least `needed_bytes` bytes (`<= self.raw.cap`).
        // 2. `write_unaligned` is used so no alignment assumption on `self.raw.buf` is required.
        unsafe {
            ptr::write_unaligned(self.raw.buf.add(self.raw.len) as *mut T, val);
        }
        // Maintains safety invariant: `self.raw.len <= self.raw.cap` and `len` is a multiple of `size_of::<T>()`.
        self.raw.len = needed_bytes;
    }

    /// Appends a contiguous slice of elements into the buffer.
    #[inline]
    pub fn write_slice(&mut self, slice: &[T]) {
        if self.raw.grow_failed || slice.is_empty() {
            return;
        }
        let elem_size = core::mem::size_of::<T>();
        if elem_size == 0 {
            return;
        }
        let Some(byte_len) = slice.len().checked_mul(elem_size) else {
            self.raw.grow_failed = true;
            return;
        };
        let Some(needed_bytes) = self.raw.len.checked_add(byte_len) else {
            self.raw.grow_failed = true;
            return;
        };
        if needed_bytes > self.raw.cap {
            let success = (self.raw.grow)(&mut self.raw, needed_bytes);
            if !success {
                self.raw.grow_failed = true;
                return;
            }
        }
        debug_assert!(needed_bytes <= self.raw.cap);
        // SAFETY:
        // 1. `slice.as_ptr()` is valid for `byte_len` bytes.
        // 2. `self.raw.buf.add(self.raw.len)` is valid for `byte_len` bytes (`needed_bytes <= self.raw.cap`).
        // 3. The two memory regions do not overlap because `&mut self` exclusively borrows the write buffer.
        unsafe {
            ptr::copy_nonoverlapping(
                slice.as_ptr() as *const u8,
                self.raw.buf.add(self.raw.len),
                byte_len,
            );
        }
        // Maintains safety invariant: `self.raw.len <= self.raw.cap` and `len` is a multiple of `size_of::<T>()`.
        self.raw.len = needed_bytes;
    }

    /// Extends the buffer with elements from an iterator.
    pub fn extend<I: IntoIterator<Item = T>>(&mut self, iter: I) {
        let iter = iter.into_iter();
        let (lower, _) = iter.size_hint();
        if lower > 0 {
            self.reserve(lower);
        }
        for item in iter {
            self.push(item);
        }
    }

    /// Reserves capacity for at least `additional` more elements of type `T`.
    ///
    /// Returns `true` if the reservation succeeded, or `false` if allocation failed.
    pub fn reserve(&mut self, additional: usize) -> bool {
        if self.raw.grow_failed {
            return false;
        }
        let elem_size = core::mem::size_of::<T>();
        let Some(additional_bytes) = additional.checked_mul(elem_size) else {
            self.raw.grow_failed = true;
            return false;
        };
        let Some(needed_bytes) = self.raw.len.checked_add(additional_bytes) else {
            self.raw.grow_failed = true;
            return false;
        };
        if needed_bytes > self.raw.cap {
            let success = (self.raw.grow)(&mut self.raw, needed_bytes);
            if !success {
                self.raw.grow_failed = true;
                return false;
            }
        }
        true
    }

    /// Returns the number of elements of type `T` written so far.
    #[inline]
    pub fn len(&self) -> usize {
        self.raw
            .len
            .checked_div(core::mem::size_of::<T>())
            .unwrap_or(0)
    }

    /// Returns `true` if no elements have been written.
    #[inline]
    pub fn is_empty(&self) -> bool {
        self.len() == 0
    }

    /// Returns the current capacity in elements of type `T`.
    #[inline]
    pub fn capacity(&self) -> usize {
        self.raw
            .cap
            .checked_div(core::mem::size_of::<T>())
            .unwrap_or(0)
    }

    /// Returns a slice of the elements written so far if the underlying buffer is aligned for `T`.
    ///
    /// If growth has failed, this returns what has been written so far.
    pub fn as_slice(&self) -> &[T] {
        if self.raw.buf.is_null() || self.raw.len == 0 {
            return &[];
        }
        let elem_size = core::mem::size_of::<T>();
        if elem_size == 0 {
            return &[];
        }
        let count = self.raw.len / elem_size;
        if !(self.raw.buf as usize).is_multiple_of(core::mem::align_of::<T>()) {
            return &[];
        }
        debug_assert!(self.raw.len <= self.raw.cap);
        // SAFETY:
        // 1. `self.raw.buf` is non-null and aligned to `align_of::<T>()` (checked above).
        // 2. By `DiplomatWriteGeneric<T>`'s safety invariants, `self.raw.buf` points to `count`
        //    consecutive properly initialized values of `T` with total size `<= isize::MAX`.
        // 3. The buffer will not be mutated while `&self` is borrowed.
        unsafe { core::slice::from_raw_parts(self.raw.buf as *const T, count) }
    }

    /// Call this function before releasing the buffer to C.
    pub fn flush(&mut self) {
        self.raw.flush();
    }
}

impl<T: DiplomatAbiCompatible> Extend<T> for DiplomatWriteGeneric<T> {
    fn extend<I: IntoIterator<Item = T>>(&mut self, iter: I) {
        DiplomatWriteGeneric::extend(self, iter);
    }
}

/// An object that can one-shot write a UTF-16 string.
///
/// Over FFI, this acts just like [`DiplomatWrite`], except that the underlying
/// buffer contains `u16` code units and is converted into a string by backends
/// that support `utf16_writeables`.
pub type DiplomatWrite16 = DiplomatWriteGeneric<u16>;

impl DiplomatWriteGeneric<u16> {
    /// Encodes `c` as UTF-16 (1 or 2 `u16` code units) and appends it to the buffer.
    #[inline]
    pub fn write_char_as_utf16(&mut self, c: char) {
        let mut buf = [0u16; 2];
        self.write_slice(c.encode_utf16(&mut buf));
    }
}
