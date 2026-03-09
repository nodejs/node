use core::marker::PhantomData;
use core::mem;
use core::mem::ManuallyDrop;
use core::ops::Deref;
use core::panic::{RefUnwindSafe, UnwindSafe};
use core::ptr::NonNull;

use super::Arc;

/// A "borrowed `Arc`". This is a pointer to
/// a T that is known to have been allocated within an
/// `Arc`.
///
/// This is equivalent in guarantees to `&Arc<T>`, however it is
/// a bit more flexible. To obtain an `&Arc<T>` you must have
/// an `Arc<T>` instance somewhere pinned down until we're done with it.
/// It's also a direct pointer to `T`, so using this involves less pointer-chasing
///
/// However, C++ code may hand us refcounted things as pointers to T directly,
/// so we have to conjure up a temporary `Arc` on the stack each time. The
/// same happens for when the object is managed by a `OffsetArc`.
///
/// `ArcBorrow` lets us deal with borrows of known-refcounted objects
/// without needing to worry about where the `Arc<T>` is.
#[derive(Debug, Eq, PartialEq)]
#[repr(transparent)]
pub struct ArcBorrow<'a, T: ?Sized + 'a>(pub(crate) NonNull<T>, pub(crate) PhantomData<&'a T>);

unsafe impl<'a, T: ?Sized + Sync + Send> Send for ArcBorrow<'a, T> {}
unsafe impl<'a, T: ?Sized + Sync + Send> Sync for ArcBorrow<'a, T> {}

impl<'a, T: RefUnwindSafe + ?Sized> UnwindSafe for ArcBorrow<'a, T> {}

impl<'a, T> Copy for ArcBorrow<'a, T> {}
impl<'a, T> Clone for ArcBorrow<'a, T> {
    #[inline]
    fn clone(&self) -> Self {
        *self
    }
}

impl<'a, T> ArcBorrow<'a, T> {
    /// Clone this as an `Arc<T>`. This bumps the refcount.
    #[inline]
    pub fn clone_arc(&self) -> Arc<T> {
        let arc = unsafe { Arc::from_raw(self.0.as_ptr()) };
        // addref it!
        mem::forget(arc.clone());
        arc
    }

    /// For constructing from a pointer known to be Arc-backed,
    /// e.g. if we obtain such a pointer over FFI
    ///
    // TODO: should from_ptr be relaxed to unsized types? It can't be
    // converted back to an Arc right now for unsized types.
    //
    /// # Safety
    /// - The pointer to `T` must have come from a Triomphe `Arc`, `UniqueArc`, or `ArcBorrow`.
    /// - The pointer to `T` must have full provenance over the `Arc`, `UniqueArc`, or `ArcBorrow`,
    ///   in particular it must not have been derived from a `&T` reference, as references immediately
    ///   loose all provenance over the adjacent reference counts. As of this writing,
    ///   of the 3 types, only Trimphe's `Arc` offers a direct API for obtaining such a pointer:
    ///   [`Arc::as_ptr`].
    #[inline]
    pub unsafe fn from_ptr(ptr: *const T) -> Self {
        unsafe { ArcBorrow(NonNull::new_unchecked(ptr as *mut T), PhantomData) }
    }

    /// Compare two `ArcBorrow`s via pointer equality. Will only return
    /// true if they come from the same allocation
    #[inline]
    pub fn ptr_eq(this: &Self, other: &Self) -> bool {
        this.0 == other.0
    }

    /// The reference count of the underlying `Arc`.
    ///
    /// The number does not include borrowed pointers,
    /// or temporary `Arc` pointers created with functions like
    /// [`ArcBorrow::with_arc`].
    ///
    /// The function is called `strong_count` to mirror `std::sync::Arc::strong_count`,
    /// however `triomphe::Arc` does not support weak references.
    #[inline]
    pub fn strong_count(this: &Self) -> usize {
        Self::with_arc(this, |arc| Arc::strong_count(arc))
    }

    /// Temporarily converts |self| into a bonafide Arc and exposes it to the
    /// provided callback. The refcount is not modified.
    #[inline]
    pub fn with_arc<F, U>(&self, f: F) -> U
    where
        F: FnOnce(&Arc<T>) -> U,
    {
        // Synthesize transient Arc, which never touches the refcount.
        let transient = unsafe { ManuallyDrop::new(Arc::from_raw(self.0.as_ptr())) };

        // Expose the transient Arc to the callback, which may clone it if it wants
        // and forward the result to the user
        f(&transient)
    }

    /// Similar to deref, but uses the lifetime |a| rather than the lifetime of
    /// self, which is incompatible with the signature of the Deref trait.
    #[inline]
    pub fn get(&self) -> &'a T {
        unsafe { &*self.0.as_ptr() }
    }
}

impl<'a, T> Deref for ArcBorrow<'a, T> {
    type Target = T;

    #[inline]
    fn deref(&self) -> &T {
        self.get()
    }
}

// Safety:
// This implementation must guarantee that it is sound to call replace_ptr with an unsized variant
// of the pointer retuned in `as_sized_ptr`. We leverage unsizing the contained reference. This
// continues to point to the data of an ArcInner. The reference count remains untouched which is
// correct since the number of owners did not change. This implies the returned instance fulfills
// its safety invariants.
#[cfg(feature = "unsize")]
unsafe impl<'lt, T: 'lt, U: ?Sized + 'lt> unsize::CoerciblePtr<U> for ArcBorrow<'lt, T> {
    type Pointee = T;
    type Output = ArcBorrow<'lt, U>;

    fn as_sized_ptr(&mut self) -> *mut T {
        self.0.as_ptr()
    }

    unsafe fn replace_ptr(self, new: *mut U) -> ArcBorrow<'lt, U> {
        let inner = ManuallyDrop::new(self);
        // Safety: backed by the same Arc that backed `self`.
        ArcBorrow(inner.0.replace_ptr(new), PhantomData)
    }
}

#[test]
fn clone_arc_borrow() {
    let x = Arc::new(42);
    let b: ArcBorrow<'_, i32> = x.borrow_arc();
    let y = b.clone_arc();
    assert_eq!(x, y);
}
