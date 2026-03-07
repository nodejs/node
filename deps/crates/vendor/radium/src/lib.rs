//! `radium` provides a series of helpers for a uniform API over both atomic
//! types like [`AtomicUsize`], and non-atomic types like [`Cell<T>`].
//!
//! This crate is `#![no_std]`-compatible, and uses no non-core types.
//!
//! For details, see the documentation for [`Radium`].
//!
//! The `types` module provides type names that are atomic where the target
//! supports it, and fall back to `Cell` when the target does not.
//!
//! The `if_atomic!` macro provides a means of conditional compilation based on
//! the presence of atomic instructions. It is a substitute for the
//! `cfg(target_has_atomic)` or `cfg(accessible)` attribute tests, which are not
//! yet stabilized.
//!
//! ---
//!
//! **@kneecaw** - <https://twitter.com/kneecaw/status/1132695060812849154>
//! > Feelin' lazy: Has someone already written a helper trait abstracting
//! > operations over `AtomicUsize` and `Cell<usize>` for generic code which may
//! > not care about atomicity?
//!
//! **@ManishEarth** - <https://twitter.com/ManishEarth/status/1132706585300496384>
//! > no but call the crate radium
//! >
//! > (since people didn't care that it was radioactive and used it in everything)
//!
//! [`AtomicUsize`]: core::sync::atomic::AtomicUsize
//! [`Cell<T>`]: core::cell::Cell

#![no_std]
#![deny(unconditional_recursion)]

#[macro_use]
mod macros;

pub mod types;

use core::cell::Cell;
use core::sync::atomic::Ordering;

if_atomic! {
    if atomic(8) {
        use core::sync::atomic::{AtomicBool, AtomicI8, AtomicU8};
    }
    if atomic(16) {
        use core::sync::atomic::{AtomicI16, AtomicU16};
    }
    if atomic(32) {
        use core::sync::atomic::{AtomicI32, AtomicU32};
    }
    if atomic(64) {
        use core::sync::atomic::{AtomicI64, AtomicU64};
    }
    if atomic(ptr) {
        use core::sync::atomic::{AtomicIsize, AtomicPtr, AtomicUsize};
    }
}

/// A maybe-atomic shared mutable fundamental type `T`.
///
/// This trait is implemented by both the [atomic wrapper] type for `T`, and by
/// [`Cell<T>`], providing a consistent interface for interacting with the two
/// types.
///
/// This trait provides methods predicated on marker traits for the underlying
/// fundamental. Only types which can be viewed as sequences of bits may use the
/// functions for bit-wise arithmetic, and only types which can be used as
/// integers may use the functions for numeric arithmetic. Use of these methods
/// on insufficient underlying types (for example, `Radium::fetch_and` on an
/// atomic or cell-wrapped pointer) will cause a compiler error.
///
/// [atomic wrapper]: core::sync::atomic
/// [`Cell<T>`]: core::cell::Cell
pub trait Radium {
    type Item;
    /// Creates a new value of this type.
    fn new(value: Self::Item) -> Self;

    /// If the underlying value is atomic, calls [`fence`] with the given
    /// [`Ordering`]. Otherwise, does nothing.
    ///
    /// [`Ordering`]: core::sync::atomic::Ordering
    /// [`fence`]: core::sync::atomic::fence
    fn fence(order: Ordering);

    /// Returns a mutable reference to the underlying value.
    ///
    /// This is safe because the mutable reference to `self` guarantees that no
    /// other references exist to this value.
    fn get_mut(&mut self) -> &mut Self::Item;

    /// Consumes the wrapper and returns the contained value.
    ///
    /// This is safe as passing by value ensures no other references exist.
    fn into_inner(self) -> Self::Item;

    /// Load a value from this object.
    ///
    /// Ordering values are ignored by non-atomic types.
    ///
    /// See also: [`AtomicUsize::load`].
    ///
    /// [`AtomicUsize::load`]: core::sync::atomic::AtomicUsize::load
    fn load(&self, order: Ordering) -> Self::Item;

    /// Store a value in this object.
    ///
    /// Ordering arguments are ignored by non-atomic types.
    ///
    /// See also: [`AtomicUsize::store`].
    ///
    /// [`AtomicUsize::store`]: core::sync::atomic::AtomicUsize::store
    fn store(&self, value: Self::Item, order: Ordering);

    /// Swap with the value stored in this object.
    ///
    /// Ordering arguments are ignored by non-atomic types.
    ///
    /// See also: [`AtomicUsize::swap`].
    ///
    /// [`AtomicUsize::swap`]: core::sync::atomic::AtomicUsize::swap
    fn swap(&self, value: Self::Item, order: Ordering) -> Self::Item;

    /// Stores a value into this object if the currently-stored value is the
    /// same as the `current` value.
    ///
    /// The return value is always the previously-stored value. If it is equal to
    /// `current`, then the value was updated with `new`.
    ///
    /// Ordering arguments are ignored by non-atomic types.
    ///
    /// See also: [`AtomicUsize::compare_and_swap`].
    ///
    /// [`AtomicUsize::compare_and_swap`]: core::sync::atomic::AtomicUsize::compare_and_swap
    #[deprecated = "Use `compare_exchange` or `compare_exchange_weak` instead"]
    fn compare_and_swap(&self, current: Self::Item, new: Self::Item, order: Ordering)
        -> Self::Item;

    /// Stores a value into this object if the currently-stored value is the
    /// same as the `current` value.
    ///
    /// The return value is a `Result` indicating whether the new value was
    /// written, and containing the previously-stored value. On success, this
    /// value is guaranteed to be equal to `current`.
    ///
    /// Ordering arguments are ignored by non-atomic types.
    ///
    /// See also: [`AtomicUsize::compare_exchange`].
    ///
    /// [`AtomicUsize::compare_exchange`]: core::sync::atomic::AtomicUsize::compare_exchange
    fn compare_exchange(
        &self,
        current: Self::Item,
        new: Self::Item,
        success: Ordering,
        failure: Ordering,
    ) -> Result<Self::Item, Self::Item>;

    /// Stores a value into this object if the currently-stored value is the
    /// same as the `current` value.
    ///
    /// Unlike `compare_exchange`, this function is allowed to spuriously fail
    /// even when the comparison succeeds, which can result in more efficient
    /// code on some platforms. The return value is a `Result` indicating
    /// whether the new value was written, and containing the previously-stored
    /// value.
    ///
    /// Ordering arguments are ignored by non-atomic types.
    ///
    /// See also: [`AtomicUsize::compare_exchange_weak`].
    ///
    /// [`AtomicUsize::compare_exchange_weak`]: core::sync::atomic::AtomicUsize::compare_exchange_weak
    fn compare_exchange_weak(
        &self,
        current: Self::Item,
        new: Self::Item,
        success: Ordering,
        failure: Ordering,
    ) -> Result<Self::Item, Self::Item>;

    /// Performs a bitwise "and" on the currently-stored value and the argument
    /// `value`, and stores the result in `self`.
    ///
    /// Returns the previously-stored value.
    ///
    /// Ordering arguments are ignored by non-atomic types.
    ///
    /// See also: [`AtomicUsize::fetch_and`].
    ///
    /// [`AtomicUsize::fetch_and`]: core::sync::atomic::AtomicUsize::fetch_and
    fn fetch_and(&self, value: Self::Item, order: Ordering) -> Self::Item
    where
        Self::Item: marker::BitOps;

    /// Performs a bitwise "nand" on the currently-stored value and the argument
    /// `value`, and stores the result in `self`.
    ///
    /// Returns the previously-stored value.
    ///
    /// Ordering arguments are ignored by non-atomic types.
    ///
    /// See also: [`AtomicUsize::fetch_nand`].
    ///
    /// [`AtomicUsize::fetch_nand`]: core::sync::atomic::AtomicUsize::fetch_nand
    fn fetch_nand(&self, value: Self::Item, order: Ordering) -> Self::Item
    where
        Self::Item: marker::BitOps;

    /// Performs a bitwise "or" on the currently-stored value and the argument
    /// `value`, and stores the result in `self`.
    ///
    /// Returns the previously-stored value.
    ///
    /// Ordering arguments are ignored by non-atomic types.
    ///
    /// See also: [`AtomicUsize::fetch_or`].
    ///
    /// [`AtomicUsize::fetch_or`]: core::sync::atomic::AtomicUsize::fetch_or
    fn fetch_or(&self, value: Self::Item, order: Ordering) -> Self::Item
    where
        Self::Item: marker::BitOps;

    /// Performs a bitwise "xor" on the currently-stored value and the argument
    /// `value`, and stores the result in `self`.
    ///
    /// Returns the previously-stored value.
    ///
    /// Ordering arguments are ignored by non-atomic types.
    ///
    /// See also: [`AtomicUsize::fetch_xor`].
    ///
    /// [`AtomicUsize::fetch_xor`]: core::sync::atomic::AtomicUsize::fetch_xor
    fn fetch_xor(&self, value: Self::Item, order: Ordering) -> Self::Item
    where
        Self::Item: marker::BitOps;

    /// Adds `value` to the currently-stored value, wrapping on overflow, and
    /// stores the result in `self`.
    ///
    /// Returns the previously-stored value.
    ///
    /// Ordering arguments are ignored by non-atomic types.
    ///
    /// See also: [`AtomicUsize::fetch_add`].
    ///
    /// [`AtomicUsize::fetch_add`]: core::sync::atomic::AtomicUsize::fetch_add
    fn fetch_add(&self, value: Self::Item, order: Ordering) -> Self::Item
    where
        Self::Item: marker::NumericOps;

    /// Subtracts `value` from the currently-stored value, wrapping on
    /// underflow, and stores the result in `self`.
    ///
    /// Returns the previously-stored value.
    ///
    /// Ordering arguments are ignored by non-atomic types.
    ///
    /// See also: [`AtomicUsize::fetch_sub`].
    ///
    /// [`AtomicUsize::fetch_sub`]: core::sync::atomic::AtomicUsize::fetch_sub
    fn fetch_sub(&self, value: Self::Item, order: Ordering) -> Self::Item
    where
        Self::Item: marker::NumericOps;

    /// Fetches the value, and applies a function to it that returns an
    /// optional new value.
    ///
    /// Note: This may call the function multiple times if the value has been
    /// changed from other threads in the meantime, as long as the function
    /// returns `Some(_)`, but the function will have been applied only once to
    /// the stored value.
    ///
    /// Returns a `Result` of `Ok(previous_value)` if the function returned
    /// `Some(_)`, else `Err(previous_value)`.
    ///
    /// Ordering arguments are ignored by non-atomic types.
    ///
    /// See also: [`AtomicUsize::fetch_update`].
    ///
    /// [`AtomicUsize::fetch_update`]: core::sync::atomic::AtomicUsize::fetch_update
    fn fetch_update<F>(
        &self,
        set_order: Ordering,
        fetch_order: Ordering,
        f: F,
    ) -> Result<Self::Item, Self::Item>
    where
        F: FnMut(Self::Item) -> Option<Self::Item>;
}

/// Marker traits used by [`Radium`].
pub mod marker {
    /// Types supporting maybe-atomic bitwise operations.
    ///
    /// Types implementing this trait support the [`fetch_and`], [`fetch_nand`],
    /// [`fetch_or`], and [`fetch_xor`] maybe-atomic operations.
    ///
    /// [`fetch_and`]: crate::Radium::fetch_and
    /// [`fetch_nand`]: crate::Radium::fetch_nand
    /// [`fetch_or`]: crate::Radium::fetch_or
    /// [`fetch_xor`]: crate::Radium::fetch_xor
    ///
    /// `bool` and all integer fundamental types implement this.
    ///
    /// ```rust
    /// # use core::sync::atomic::*;
    /// # use radium::Radium;
    /// let num: AtomicUsize = AtomicUsize::new(0);
    /// Radium::fetch_or(&num, 2, Ordering::Relaxed);
    /// ```
    ///
    /// Pointers do not. This will cause a compiler error.
    ///
    /// ```rust,compile_fail
    /// # use core::sync::atomic::*;
    /// # use radium::Radium;
    /// # use core::ptr;
    /// let ptr: AtomicPtr<usize> = Default::default();
    /// Radium::fetch_or(&ptr, ptr::null_mut(), Ordering::Relaxed);
    /// ```
    pub trait BitOps {}

    /// Types supporting maybe-atomic arithmetic operations.
    ///
    /// Types implementing this trait support the [`fetch_add`] and
    /// [`fetch_sub`] maybe-atomic operations.
    ///
    /// [`fetch_add`]: crate::Radium::fetch_add
    /// [`fetch_sub`]: crate::Radium::fetch_sub
    ///
    /// The integer types, such as `usize` and `i32`, implement this trait.
    ///
    /// ```rust
    /// # use core::sync::atomic::*;
    /// # use radium::Radium;
    /// let num: AtomicUsize = AtomicUsize::new(2);
    /// Radium::fetch_add(&num, 2, Ordering::Relaxed);
    /// ```
    ///
    /// `bool` and pointers do not. This will cause a compiler error.
    ///
    /// ```rust,compile_fail
    /// # use core::sync::atomic::*;
    /// # use radium::Radium;
    /// let bit: AtomicBool = AtomicBool::new(false);
    /// Radium::fetch_add(&bit, true, Ordering::Relaxed);
    /// ```
    pub trait NumericOps: BitOps {}
}

macro_rules! radium {
    // Emit the universal `Radium` trait function bodies for atomic types.
    ( atom $base:ty ) => {
        #[inline]
        fn new(value: $base) -> Self {
            Self::new(value)
        }

        #[inline]
        fn fence(order: Ordering) {
            core::sync::atomic::fence(order);
        }

        #[inline]
        fn get_mut(&mut self) -> &mut $base {
            self.get_mut()
        }

        #[inline]
        fn into_inner(self) -> $base {
            self.into_inner()
        }

        #[inline]
        fn load(&self, order: Ordering) -> $base {
            self.load(order)
        }

        #[inline]
        fn store(&self, value: $base, order: Ordering) {
            self.store(value, order);
        }

        #[inline]
        fn swap(&self, value: $base, order: Ordering) -> $base {
            self.swap(value, order)
        }

        #[inline]
        #[allow(deprecated)]
        fn compare_and_swap(&self, current: $base, new: $base, order: Ordering) -> $base {
            self.compare_and_swap(current, new, order)
        }

        #[inline]
        fn compare_exchange(
            &self,
            current: $base,
            new: $base,
            success: Ordering,
            failure: Ordering,
        ) -> Result<$base, $base> {
            self.compare_exchange(current, new, success, failure)
        }

        #[inline]
        fn compare_exchange_weak(
            &self,
            current: $base,
            new: $base,
            success: Ordering,
            failure: Ordering,
        ) -> Result<$base, $base> {
            self.compare_exchange_weak(current, new, success, failure)
        }

        #[inline]
        fn fetch_update<F>(
            &self,
            set_order: Ordering,
            fetch_order: Ordering,
            f: F,
        ) -> Result<$base, $base>
        where
            F: FnMut($base) -> Option<$base>,
        {
            self.fetch_update(set_order, fetch_order, f)
        }
    };

    // Emit the `Radium` trait function bodies for bit-wise types.
    ( atom_bit $base:ty ) => {
        #[inline]
        fn fetch_and(&self, value: $base, order: Ordering) -> $base {
            self.fetch_and(value, order)
        }

        #[inline]
        fn fetch_nand(&self, value: $base, order: Ordering) -> $base {
            self.fetch_nand(value, order)
        }

        #[inline]
        fn fetch_or(&self, value: $base, order: Ordering) -> $base {
            self.fetch_or(value, order)
        }

        #[inline]
        fn fetch_xor(&self, value: $base, order: Ordering) -> $base {
            self.fetch_xor(value, order)
        }
    };

    // Emit the `Radium` trait function bodies for integral types.
    ( atom_int $base:ty ) => {
        #[inline]
        fn fetch_add(&self, value: $base, order: Ordering) -> $base {
            self.fetch_add(value, order)
        }

        #[inline]
        fn fetch_sub(&self, value: $base, order: Ordering) -> $base {
            self.fetch_sub(value, order)
        }
    };

    // Emit the universal `Radium` trait function bodies for `Cell<_>`.
    ( cell $base:ty ) => {
        #[inline]
        fn new(value: $base) -> Self {
            Cell::new(value)
        }

        #[inline]
        fn fence(_: Ordering) {}

        #[inline]
        fn get_mut(&mut self) -> &mut $base {
            self.get_mut()
        }

        #[inline]
        fn into_inner(self) -> $base {
            self.into_inner()
        }

        #[inline]
        fn load(&self, _: Ordering) -> $base {
            self.get()
        }

        #[inline]
        fn store(&self, value: $base, _: Ordering) {
            self.set(value);
        }

        #[inline]
        fn swap(&self, value: $base, _: Ordering) -> $base {
            self.replace(value)
        }

        #[inline]
        fn compare_and_swap(&self, current: $base, new: $base, _: Ordering) -> $base {
            if self.get() == current {
                self.replace(new)
            } else {
                self.get()
            }
        }

        #[inline]
        fn compare_exchange(
            &self,
            current: $base,
            new: $base,
            _: Ordering,
            _: Ordering,
        ) -> Result<$base, $base> {
            if self.get() == current {
                Ok(self.replace(new))
            } else {
                Err(self.get())
            }
        }

        #[inline]
        fn compare_exchange_weak(
            &self,
            current: $base,
            new: $base,
            success: Ordering,
            failure: Ordering,
        ) -> Result<$base, $base> {
            Radium::compare_exchange(self, current, new, success, failure)
        }

        #[inline]
        fn fetch_update<F>(&self, _: Ordering, _: Ordering, mut f: F) -> Result<$base, $base>
        where
            F: FnMut($base) -> Option<$base>,
        {
            match f(self.get()) {
                Some(x) => Ok(self.replace(x)),
                None => Err(self.get()),
            }
        }
    };

    // Emit the `Radium` trait function bodies for bit-wise types.
    ( cell_bit $base:ty ) => {
        #[inline]
        fn fetch_and(&self, value: $base, _: Ordering) -> $base {
            self.replace(self.get() & value)
        }

        #[inline]
        fn fetch_nand(&self, value: $base, _: Ordering) -> $base {
            self.replace(!(self.get() & value))
        }

        #[inline]
        fn fetch_or(&self, value: $base, _: Ordering) -> $base {
            self.replace(self.get() | value)
        }

        #[inline]
        fn fetch_xor(&self, value: $base, _: Ordering) -> $base {
            self.replace(self.get() ^ value)
        }
    };

    // Emit the `Radium` trait function bodies for integral types.
    ( cell_int $base:ty ) => {
        #[inline]
        fn fetch_add(&self, value: $base, _: Ordering) -> $base {
            self.replace(self.get().wrapping_add(value))
        }

        #[inline]
        fn fetch_sub(&self, value: $base, _: Ordering) -> $base {
            self.replace(self.get().wrapping_sub(value))
        }
    };
}

macro_rules! radium_int {
    ( $( $width:tt: $base:ty , $atom:ty ; )* ) => { $(
        impl marker::BitOps for $base {}
        impl marker::NumericOps for $base {}

        if_atomic!(if atomic($width) {
            impl Radium for $atom {
                type Item = $base;

                radium!(atom $base);
                radium!(atom_bit $base);
                radium!(atom_int $base);
            }
        });

        impl Radium for Cell<$base> {
            type Item = $base;

            radium!(cell $base);
            radium!(cell_bit $base);
            radium!(cell_int $base);
        }
    )* };
}

radium_int! {
    8: i8, AtomicI8;
    8: u8, AtomicU8;
    16: i16, AtomicI16;
    16: u16, AtomicU16;
    32: i32, AtomicI32;
    32: u32, AtomicU32;
    64: i64, AtomicI64;
    64: u64, AtomicU64;
    size: isize, AtomicIsize;
    size: usize, AtomicUsize;
}

impl marker::BitOps for bool {}

if_atomic!(if atomic(bool) {
    impl Radium for AtomicBool {
        type Item = bool;

        radium!(atom bool);
        radium!(atom_bit bool);

        /// ```compile_fail
        /// # use std::{ptr, sync::atomic::*, cell::*};
        /// # use radium::*;
        /// let atom = AtomicBool::new(false);
        /// Radium::fetch_add(&atom, true, Ordering::Relaxed);
        /// ```
        #[doc(hidden)]
        fn fetch_add(&self, _value: bool, _order: Ordering) -> bool {
            unreachable!("This method statically cannot be called")
        }

        /// ```compile_fail
        /// # use std::{ptr, sync::atomic::*, cell::*};
        /// # use radium::*;
        /// let atom = AtomicBool::new(false);
        /// Radium::fetch_sub(&atom, true, Ordering::Relaxed);
        /// ```
        #[doc(hidden)]
        fn fetch_sub(&self, _value: bool, _order: Ordering) -> bool {
            unreachable!("This method statically cannot be called")
        }
    }
});

impl Radium for Cell<bool> {
    type Item = bool;

    radium!(cell bool);
    radium!(cell_bit bool);

    /// ```compile_fail
    /// # use std::{ptr, sync::atomic::*, cell::*};
    /// # use radium::*;
    /// let cell = Cell::<bool>::new(false);
    /// Radium::fetch_add(&cell, true, Ordering::Relaxed);
    /// ```
    #[doc(hidden)]
    fn fetch_add(&self, _value: bool, _order: Ordering) -> bool {
        unreachable!("This method statically cannot be called")
    }

    /// ```compile_fail
    /// # use std::{ptr, sync::atomic::*, cell::*};
    /// # use radium::*;
    /// let cell = Cell::<bool>::new(false);
    /// Radium::fetch_sub(&cell, true, Ordering::Relaxed);
    /// ```
    #[doc(hidden)]
    fn fetch_sub(&self, _value: bool, _order: Ordering) -> bool {
        unreachable!("This method statically cannot be called")
    }
}

if_atomic!(if atomic(ptr) {
    impl<T> Radium for AtomicPtr<T> {
        type Item = *mut T;

        radium!(atom *mut T);

        /// ```compile_fail
        /// # use std::{ptr, sync::atomic::*, cell::*};
        /// # use radium::*;
        /// let atom = AtomicPtr::<u8>::new(ptr::null_mut());
        /// Radium::fetch_and(&atom, ptr::null_mut(), Ordering::Relaxed);
        /// ```
        #[doc(hidden)]
        fn fetch_and(&self, _value: *mut T, _order: Ordering) -> *mut T {
            unreachable!("This method statically cannot be called")
        }

        /// ```compile_fail
        /// # use std::{ptr, sync::atomic::*, cell::*};
        /// # use radium::*;
        /// let atom = AtomicPtr::<u8>::new(ptr::null_mut());
        /// Radium::fetch_nand(&atom, ptr::null_mut(), Ordering::Relaxed);
        /// ```
        #[doc(hidden)]
        fn fetch_nand(&self, _value: *mut T, _order: Ordering) -> *mut T {
            unreachable!("This method statically cannot be called")
        }

        /// ```compile_fail
        /// # use std::{ptr, sync::atomic::*, cell::*};
        /// # use radium::*;
        /// let atom = AtomicPtr::<u8>::new(ptr::null_mut());
        /// Radium::fetch_or(&atom, ptr::null_mut(), Ordering::Relaxed);
        /// ```
        #[doc(hidden)]
        fn fetch_or(&self, _value: *mut T, _order: Ordering) -> *mut T {
            unreachable!("This method statically cannot be called")
        }

        /// ```compile_fail
        /// # use std::{ptr, sync::atomic::*, cell::*};
        /// # use radium::*;
        /// let atom = AtomicPtr::<u8>::new(ptr::null_mut());
        /// Radium::fetch_xor(&atom, ptr::null_mut(), Ordering::Relaxed);
        /// ```
        #[doc(hidden)]
        fn fetch_xor(&self, _value: *mut T, _order: Ordering) -> *mut T {
            unreachable!("This method statically cannot be called")
        }

        /// ```compile_fail
        /// # use std::{ptr, sync::atomic::*, cell::*};
        /// # use radium::*;
        /// let atom = AtomicPtr::<u8>::new(ptr::null_mut());
        /// Radium::fetch_add(&atom, ptr::null_mut(), Ordering::Relaxed);
        /// ```
        #[doc(hidden)]
        fn fetch_add(&self, _value: *mut T, _order: Ordering) -> *mut T {
            unreachable!("This method statically cannot be called")
        }

        /// ```compile_fail
        /// # use std::{ptr, sync::atomic::*, cell::*};
        /// # use radium::*;
        /// let atom = AtomicPtr::<u8>::new(ptr::null_mut());
        /// Radium::fetch_sub(&atom, ptr::null_mut(), Ordering::Relaxed);
        /// ```
        #[doc(hidden)]
        fn fetch_sub(&self, _value: *mut T, _order: Ordering) -> *mut T {
            unreachable!("This method statically cannot be called")
        }
    }
});

impl<T> Radium for Cell<*mut T> {
    type Item = *mut T;

    radium!(cell *mut T);

    /// ```compile_fail
    /// # use std::{ptr, sync::atomic::*, cell::*};
    /// # use radium::*;
    /// let cell = Cell::<*mut u8>::new(ptr::null_mut());
    /// Radium::fetch_and(&cell, ptr::null_mut(), Ordering::Relaxed);
    /// ```
    #[doc(hidden)]
    fn fetch_and(&self, _value: *mut T, _order: Ordering) -> *mut T {
        unreachable!("This method statically cannot be called")
    }

    /// ```compile_fail
    /// # use std::{ptr, sync::atomic::*, cell::*};
    /// # use radium::*;
    /// let cell = Cell::<*mut u8>::new(ptr::null_mut());
    /// Radium::fetch_nand(&cell, ptr::null_mut(), Ordering::Relaxed);
    /// ```
    #[doc(hidden)]
    fn fetch_nand(&self, _value: *mut T, _order: Ordering) -> *mut T {
        unreachable!("This method statically cannot be called")
    }

    /// ```compile_fail
    /// # use std::{ptr, sync::atomic::*, cell::*};
    /// # use radium::*;
    /// let cell = Cell::<*mut u8>::new(ptr::null_mut());
    /// Radium::fetch_or(&cell, ptr::null_mut(), Ordering::Relaxed);
    /// ```
    #[doc(hidden)]
    fn fetch_or(&self, _value: *mut T, _order: Ordering) -> *mut T {
        unreachable!("This method statically cannot be called")
    }

    /// ```compile_fail
    /// # use std::{ptr, sync::atomic::*, cell::*};
    /// # use radium::*;
    /// let cell = Cell::<*mut u8>::new(ptr::null_mut());
    /// Radium::fetch_xor(&cell, ptr::null_mut(), Ordering::Relaxed);
    /// ```
    #[doc(hidden)]
    fn fetch_xor(&self, _value: *mut T, _order: Ordering) -> *mut T {
        unreachable!("This method statically cannot be called")
    }

    /// ```compile_fail
    /// # use std::{ptr, sync::atomic::*, cell::*};
    /// # use radium::*;
    /// let cell = Cell::<*mut u8>::new(ptr::null_mut());
    /// Radium::fetch_add(&cell, ptr::null_mut(), Ordering::Relaxed);
    /// ```
    #[doc(hidden)]
    fn fetch_add(&self, _value: *mut T, _order: Ordering) -> *mut T {
        unreachable!("This method statically cannot be called")
    }

    /// ```compile_fail
    /// # use std::{ptr, sync::atomic::*, cell::*};
    /// # use radium::*;
    /// let cell = Cell::<*mut u8>::new(ptr::null_mut());
    /// Radium::fetch_sub(&cell, ptr::null_mut(), Ordering::Relaxed);
    /// ```
    #[doc(hidden)]
    fn fetch_sub(&self, _value: *mut T, _order: Ordering) -> *mut T {
        unreachable!("This method statically cannot be called")
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use core::cell::Cell;

    #[test]
    fn absent_traits() {
        static_assertions::assert_not_impl_any!(bool: marker::NumericOps);
        static_assertions::assert_not_impl_any!(*mut u8: marker::BitOps, marker::NumericOps);
    }

    #[test]
    fn present_traits() {
        static_assertions::assert_impl_all!(bool: marker::BitOps);
        static_assertions::assert_impl_all!(usize: marker::BitOps, marker::NumericOps);
    }

    #[test]
    fn always_cell() {
        static_assertions::assert_impl_all!(Cell<bool>: Radium<Item = bool>);
        static_assertions::assert_impl_all!(Cell<i8>: Radium<Item = i8>);
        static_assertions::assert_impl_all!(Cell<u8>: Radium<Item = u8>);
        static_assertions::assert_impl_all!(Cell<i16>: Radium<Item = i16>);
        static_assertions::assert_impl_all!(Cell<u16>: Radium<Item = u16>);
        static_assertions::assert_impl_all!(Cell<i32>: Radium<Item = i32>);
        static_assertions::assert_impl_all!(Cell<u32>: Radium<Item = u32>);
        static_assertions::assert_impl_all!(Cell<i64>: Radium<Item = i64>);
        static_assertions::assert_impl_all!(Cell<u64>: Radium<Item = u64>);
        static_assertions::assert_impl_all!(Cell<isize>: Radium<Item = isize>);
        static_assertions::assert_impl_all!(Cell<usize>: Radium<Item = usize>);
        static_assertions::assert_impl_all!(Cell<*mut ()>: Radium<Item = *mut ()>);
    }

    #[test]
    fn always_alias() {
        static_assertions::assert_impl_all!(types::RadiumBool: Radium<Item = bool>);
        static_assertions::assert_impl_all!(types::RadiumI8: Radium<Item = i8>);
        static_assertions::assert_impl_all!(types::RadiumU8: Radium<Item = u8>);
        static_assertions::assert_impl_all!(types::RadiumI16: Radium<Item = i16>);
        static_assertions::assert_impl_all!(types::RadiumU16: Radium<Item = u16>);
        static_assertions::assert_impl_all!(types::RadiumI32: Radium<Item = i32>);
        static_assertions::assert_impl_all!(types::RadiumU32: Radium<Item = u32>);
        static_assertions::assert_impl_all!(types::RadiumI64: Radium<Item = i64>);
        static_assertions::assert_impl_all!(types::RadiumU64: Radium<Item = u64>);
        static_assertions::assert_impl_all!(types::RadiumIsize: Radium<Item = isize>);
        static_assertions::assert_impl_all!(types::RadiumUsize: Radium<Item = usize>);
        static_assertions::assert_impl_all!(types::RadiumPtr<()>: Radium<Item = *mut ()>);
    }

    #[test]
    fn maybe_atom() {
        if_atomic! {
            if atomic(bool) {
                use core::sync::atomic::*;
                static_assertions::assert_impl_all!(AtomicBool: Radium<Item = bool>);
            }
            if atomic(8) {
                static_assertions::assert_impl_all!(AtomicI8: Radium<Item = i8>);
                static_assertions::assert_impl_all!(AtomicU8: Radium<Item = u8>);
            }
            if atomic(16) {
                static_assertions::assert_impl_all!(AtomicI16: Radium<Item = i16>);
                static_assertions::assert_impl_all!(AtomicU16: Radium<Item = u16>);
            }
            if atomic(32) {
                static_assertions::assert_impl_all!(AtomicI32: Radium<Item = i32>);
                static_assertions::assert_impl_all!(AtomicU32: Radium<Item = u32>);
            }
            if atomic(64) {
                static_assertions::assert_impl_all!(AtomicI64: Radium<Item = i64>);
                static_assertions::assert_impl_all!(AtomicU64: Radium<Item = u64>);
            }
            if atomic(size) {
                static_assertions::assert_impl_all!(AtomicIsize: Radium<Item = isize>);
                static_assertions::assert_impl_all!(AtomicUsize: Radium<Item = usize>);
            }
            if atomic(ptr) {
                static_assertions::assert_impl_all!(AtomicPtr<()>: Radium<Item = *mut ()>);
            }
        }
    }
}
