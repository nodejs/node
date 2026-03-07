// Copyright 2017 The Rust Project Developers. See the COPYRIGHT
// file at the top-level directory of this distribution and at
// http://rust-lang.org/COPYRIGHT.
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

//! This module defines types which are thread safe if `cfg!(feature =
//! "concurrent")` is true.
//!
//! `Lrc` is an alias of either Rc or Arc.
//!
//! `Lock` is a mutex.
//! It internally uses `parking_lot::Mutex` if cfg!(parallel_queries) is true,
//! `RefCell` otherwise.
//!
//! `RwLock` is a read-write lock.
//! It internally uses `parking_lot::RwLock` if cfg!(parallel_queries) is true,
//! `RefCell` otherwise.
//!
//! `LockCell` is a thread safe version of `Cell`, with `set` and `get`
//! operations. It can never deadlock. It uses `Cell` when
//! cfg!(parallel_queries) is false, otherwise it is a `Lock`.
//!
//! `MTLock` is a mutex which disappears if cfg!(parallel_queries) is false.
//!
//! `MTRef` is a immutable reference if cfg!(parallel_queries), and an mutable
//! reference otherwise.
//!
//! `rustc_erase_owner!` erases a OwningRef owner into Erased or Erased + Send +
//! Sync depending on the value of cfg!(parallel_queries).

#[cfg(not(feature = "concurrent"))]
use std::cell::{RefCell as InnerRwLock, RefCell as InnerLock};
use std::{
    cmp::Ordering,
    collections::HashMap,
    fmt,
    fmt::{Debug, Formatter},
    hash::{BuildHasher, Hash},
};

#[cfg(feature = "concurrent")]
use parking_lot::{Mutex as InnerLock, RwLock as InnerRwLock};

#[cfg(feature = "concurrent")]
pub use self::concurrent::*;
#[cfg(not(feature = "concurrent"))]
pub use self::single::*;

#[cfg(feature = "concurrent")]
mod concurrent {
    pub use std::{
        marker::{Send, Sync},
        sync::Arc as Lrc,
    };

    pub use once_cell::sync::{Lazy, OnceCell};
    pub use parking_lot::{
        MappedMutexGuard as MappedLockGuard, MappedRwLockReadGuard as MappedReadGuard,
        MappedRwLockWriteGuard as MappedWriteGuard, MutexGuard as LockGuard,
        RwLockReadGuard as ReadGuard, RwLockWriteGuard as WriteGuard,
    };
}

#[cfg(not(feature = "concurrent"))]
mod single {
    pub use once_cell::unsync::{Lazy, OnceCell};
    /// Dummy trait because swc_common is in single thread mode.
    pub trait Send {}
    /// Dummy trait because swc_common is in single thread mode.
    pub trait Sync {}

    impl<T> Send for T where T: ?Sized {}
    impl<T> Sync for T where T: ?Sized {}

    pub use std::{
        cell::{
            Ref as ReadGuard, RefMut as WriteGuard, RefMut as MappedWriteGuard,
            RefMut as LockGuard, RefMut as MappedLockGuard,
        },
        rc::{Rc as Lrc, Weak},
    };
}

#[derive(Debug)]
pub struct Lock<T>(InnerLock<T>);

impl<T> Lock<T> {
    #[inline(always)]
    pub fn new(inner: T) -> Self {
        Lock(InnerLock::new(inner))
    }

    // #[inline(always)]
    // pub fn into_inner(self) -> T {
    //     self.0.into_inner()
    // }
    //
    // #[inline(always)]
    // pub fn get_mut(&mut self) -> &mut T {
    //     self.0.get_mut()
    // }

    // #[cfg(feature = "concurrent")]
    // #[inline(always)]
    // pub fn try_lock(&self) -> Option<LockGuard<'_, T>> {
    //     self.0.try_lock()
    // }
    //
    // #[cfg(not(feature = "concurrent"))]
    // #[inline(always)]
    // pub fn try_lock(&self) -> Option<LockGuard<'_, T>> {
    //     self.0.try_borrow_mut().ok()
    // }

    #[cfg(feature = "concurrent")]
    #[inline(always)]
    pub fn lock(&self) -> LockGuard<'_, T> {
        self.0.lock()
    }

    #[cfg(not(feature = "concurrent"))]
    #[inline(always)]
    pub fn lock(&self) -> LockGuard<'_, T> {
        self.0.borrow_mut()
    }

    // #[inline(always)]
    // pub fn with_lock<F: FnOnce(&mut T) -> R, R>(&self, f: F) -> R {
    //     f(&mut *self.lock())
    // }

    #[inline(always)]
    pub fn borrow(&self) -> LockGuard<'_, T> {
        self.lock()
    }

    #[inline(always)]
    pub fn borrow_mut(&self) -> LockGuard<'_, T> {
        self.lock()
    }
}

impl<T: Default> Default for Lock<T> {
    #[inline]
    fn default() -> Self {
        Lock::new(T::default())
    }
}

impl<T> LockCell<T> {
    #[inline(always)]
    pub fn new(inner: T) -> Self {
        LockCell(Lock::new(inner))
    }

    #[inline(always)]
    pub fn set(&self, new_inner: T) {
        *self.0.lock() = new_inner;
    }

    #[inline(always)]
    pub fn get(&self) -> T
    where
        T: Copy,
    {
        *self.0.lock()
    }
}

pub trait HashMapExt<K, V> {
    /// Same as HashMap::insert, but it may panic if there's already an
    /// entry for `key` with a value not equal to `value`
    fn insert_same(&mut self, key: K, value: V);
}

impl<K: Eq + Hash, V: Eq, S: BuildHasher> HashMapExt<K, V> for HashMap<K, V, S> {
    fn insert_same(&mut self, key: K, value: V) {
        self.entry(key)
            .and_modify(|old| assert!(*old == value))
            .or_insert(value);
    }
}

impl<T: Copy + Debug> Debug for LockCell<T> {
    fn fmt(&self, f: &mut Formatter<'_>) -> fmt::Result {
        f.debug_struct("LockCell")
            .field("value", &self.get())
            .finish()
    }
}

impl<T: Default> Default for LockCell<T> {
    /// Creates a `LockCell<T>`, with the `Default` value for T.
    #[inline]
    fn default() -> LockCell<T> {
        LockCell::new(Default::default())
    }
}

impl<T: PartialEq + Copy> PartialEq for LockCell<T> {
    #[inline]
    fn eq(&self, other: &LockCell<T>) -> bool {
        self.get() == other.get()
    }
}

impl<T: Eq + Copy> Eq for LockCell<T> {}

impl<T: PartialOrd + Copy> PartialOrd for LockCell<T> {
    #[inline]
    fn partial_cmp(&self, other: &LockCell<T>) -> Option<Ordering> {
        self.get().partial_cmp(&other.get())
    }

    #[inline]
    fn lt(&self, other: &LockCell<T>) -> bool {
        self.get() < other.get()
    }

    #[inline]
    fn le(&self, other: &LockCell<T>) -> bool {
        self.get() <= other.get()
    }

    #[inline]
    fn gt(&self, other: &LockCell<T>) -> bool {
        self.get() > other.get()
    }

    #[inline]
    fn ge(&self, other: &LockCell<T>) -> bool {
        self.get() >= other.get()
    }
}

impl<T: Ord + Copy> Ord for LockCell<T> {
    #[inline]
    fn cmp(&self, other: &LockCell<T>) -> Ordering {
        self.get().cmp(&other.get())
    }
}

#[derive(Debug, Default)]
pub struct RwLock<T>(InnerRwLock<T>);

impl<T> RwLock<T> {
    #[inline(always)]
    pub fn new(inner: T) -> Self {
        RwLock(InnerRwLock::new(inner))
    }

    #[cfg(not(feature = "concurrent"))]
    #[inline(always)]
    pub fn read(&self) -> ReadGuard<'_, T> {
        self.0.borrow()
    }

    #[cfg(feature = "concurrent")]
    #[inline(always)]
    pub fn read(&self) -> ReadGuard<'_, T> {
        self.0.read()
    }

    #[inline(always)]
    pub fn borrow(&self) -> ReadGuard<'_, T> {
        self.read()
    }

    #[inline(always)]
    pub fn get_mut(&mut self) -> &mut T {
        self.0.get_mut()
    }

    #[inline(always)]
    pub fn with_read_lock<F: FnOnce(&T) -> R, R>(&self, f: F) -> R {
        f(&*self.read())
    }

    #[allow(clippy::result_unit_err)]
    #[cfg(not(feature = "concurrent"))]
    #[inline(always)]
    pub fn try_write(&self) -> Result<WriteGuard<'_, T>, ()> {
        self.0.try_borrow_mut().map_err(|_| ())
    }

    #[allow(clippy::result_unit_err)]
    #[cfg(feature = "concurrent")]
    #[inline(always)]
    pub fn try_write(&self) -> Result<WriteGuard<'_, T>, ()> {
        self.0.try_write().ok_or(())
    }

    #[cfg(not(feature = "concurrent"))]
    #[inline(always)]
    pub fn write(&self) -> WriteGuard<'_, T> {
        self.0.borrow_mut()
    }

    #[cfg(feature = "concurrent")]
    #[inline(always)]
    pub fn write(&self) -> WriteGuard<'_, T> {
        self.0.write()
    }

    #[inline(always)]
    pub fn with_write_lock<F: FnOnce(&mut T) -> R, R>(&self, f: F) -> R {
        f(&mut *self.write())
    }

    #[inline(always)]
    pub fn borrow_mut(&self) -> WriteGuard<'_, T> {
        self.write()
    }
}

// FIXME: Probably a bad idea
impl<T: Clone> Clone for RwLock<T> {
    #[inline]
    fn clone(&self) -> Self {
        RwLock::new(self.borrow().clone())
    }
}

pub struct LockCell<T>(Lock<T>);
