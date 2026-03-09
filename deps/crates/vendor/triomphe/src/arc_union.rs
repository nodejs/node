use core::fmt;
use core::marker::PhantomData;
use core::panic::{RefUnwindSafe, UnwindSafe};
use core::ptr;

use super::{Arc, ArcBorrow};

/// A tagged union that can represent `Arc<A>` or `Arc<B>` while only consuming a
/// single word. The type is also `NonNull`, and thus can be stored in an Option
/// without increasing size.
///
/// This is functionally equivalent to
/// `enum ArcUnion<A, B> { First(Arc<A>), Second(Arc<B>)` but only takes up
/// up a single word of stack space.
///
/// This could probably be extended to support four types if necessary.
pub struct ArcUnion<A, B> {
    p: ptr::NonNull<()>,
    phantom_a: PhantomData<A>,
    phantom_b: PhantomData<B>,
}

unsafe impl<A: Sync + Send, B: Send + Sync> Send for ArcUnion<A, B> {}
unsafe impl<A: Sync + Send, B: Send + Sync> Sync for ArcUnion<A, B> {}

impl<A: RefUnwindSafe, B: RefUnwindSafe> UnwindSafe for ArcUnion<A, B> {}

impl<A: PartialEq, B: PartialEq> PartialEq for ArcUnion<A, B> {
    fn eq(&self, other: &Self) -> bool {
        use crate::ArcUnionBorrow::*;
        match (self.borrow(), other.borrow()) {
            (First(x), First(y)) => x == y,
            (Second(x), Second(y)) => x == y,
            (_, _) => false,
        }
    }
}

/// This represents a borrow of an `ArcUnion`.
#[derive(Debug)]
pub enum ArcUnionBorrow<'a, A: 'a, B: 'a> {
    First(ArcBorrow<'a, A>),
    Second(ArcBorrow<'a, B>),
}

impl<A, B> ArcUnion<A, B> {
    unsafe fn new(ptr: *mut ()) -> Self {
        ArcUnion {
            p: ptr::NonNull::new_unchecked(ptr),
            phantom_a: PhantomData,
            phantom_b: PhantomData,
        }
    }

    /// Returns true if the two values are pointer-equal.
    #[inline]
    pub fn ptr_eq(this: &Self, other: &Self) -> bool {
        this.p == other.p
    }

    /// Reference count.
    #[inline]
    pub fn strong_count(this: &Self) -> usize {
        ArcUnionBorrow::strong_count(&this.borrow())
    }

    /// Returns an enum representing a borrow of either A or B.
    pub fn borrow(&self) -> ArcUnionBorrow<'_, A, B> {
        if self.is_first() {
            let ptr = self.p.as_ptr() as *const A;
            let borrow = unsafe { ArcBorrow::from_ptr(ptr) };
            ArcUnionBorrow::First(borrow)
        } else {
            let ptr = ((self.p.as_ptr() as usize) & !0x1) as *const B;
            let borrow = unsafe { ArcBorrow::from_ptr(ptr) };
            ArcUnionBorrow::Second(borrow)
        }
    }

    /// Creates an `ArcUnion` from an instance of the first type.
    #[inline]
    pub fn from_first(other: Arc<A>) -> Self {
        unsafe { Self::new(Arc::into_raw(other) as *mut _) }
    }

    /// Creates an `ArcUnion` from an instance of the second type.
    #[inline]
    pub fn from_second(other: Arc<B>) -> Self {
        unsafe { Self::new(((Arc::into_raw(other) as usize) | 0x1) as *mut _) }
    }

    /// Returns true if this `ArcUnion` contains the first type.
    #[inline]
    pub fn is_first(&self) -> bool {
        self.p.as_ptr() as usize & 0x1 == 0
    }

    /// Returns true if this `ArcUnion` contains the second type.
    #[inline]
    pub fn is_second(&self) -> bool {
        !self.is_first()
    }

    /// Returns a borrow of the first type if applicable, otherwise `None`.
    pub fn as_first(&self) -> Option<ArcBorrow<'_, A>> {
        match self.borrow() {
            ArcUnionBorrow::First(x) => Some(x),
            ArcUnionBorrow::Second(_) => None,
        }
    }

    /// Returns a borrow of the second type if applicable, otherwise None.
    pub fn as_second(&self) -> Option<ArcBorrow<'_, B>> {
        match self.borrow() {
            ArcUnionBorrow::First(_) => None,
            ArcUnionBorrow::Second(x) => Some(x),
        }
    }
}

impl<A, B> Clone for ArcUnion<A, B> {
    fn clone(&self) -> Self {
        match self.borrow() {
            ArcUnionBorrow::First(x) => ArcUnion::from_first(x.clone_arc()),
            ArcUnionBorrow::Second(x) => ArcUnion::from_second(x.clone_arc()),
        }
    }
}

impl<A, B> Drop for ArcUnion<A, B> {
    fn drop(&mut self) {
        match self.borrow() {
            ArcUnionBorrow::First(x) => unsafe {
                let _ = Arc::from_raw(&*x);
            },
            ArcUnionBorrow::Second(x) => unsafe {
                let _ = Arc::from_raw(&*x);
            },
        }
    }
}

impl<A: fmt::Debug, B: fmt::Debug> fmt::Debug for ArcUnion<A, B> {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        fmt::Debug::fmt(&self.borrow(), f)
    }
}

impl<'a, A, B> ArcUnionBorrow<'a, A, B> {
    /// The reference count of this `Arc`.
    ///
    /// The number does not include borrowed pointers,
    /// or temporary `Arc` pointers created with functions like
    /// [`ArcBorrow::with_arc`].
    ///
    /// The function is called `strong_count` to mirror `std::sync::Arc::strong_count`,
    /// however `triomphe::Arc` does not support weak references.
    #[inline]
    pub fn strong_count(this: &Self) -> usize {
        match this {
            ArcUnionBorrow::First(arc) => ArcBorrow::strong_count(arc),
            ArcUnionBorrow::Second(arc) => ArcBorrow::strong_count(arc),
        }
    }
}
