use alloc::alloc::handle_alloc_error;
use alloc::boxed::Box;
use core::alloc::Layout;
use core::borrow;
use core::cmp::Ordering;
use core::convert::From;
use core::ffi::c_void;
use core::fmt;
use core::hash::{Hash, Hasher};
use core::iter::FromIterator;
use core::marker::PhantomData;
use core::mem::{ManuallyDrop, MaybeUninit};
use core::ops::Deref;
use core::panic::{RefUnwindSafe, UnwindSafe};
use core::ptr::{self, NonNull};
use core::sync::atomic;
use core::sync::atomic::Ordering::{AcqRel, Acquire, Relaxed, Release};

#[cfg(feature = "serde")]
use serde::{Deserialize, Serialize};
#[cfg(feature = "stable_deref_trait")]
use stable_deref_trait::{CloneStableDeref, StableDeref};

use crate::{abort, ArcBorrow, HeaderSlice, OffsetArc, UniqueArc};

/// A soft limit on the amount of references that may be made to an `Arc`.
///
/// Going above this limit will abort your program (although not
/// necessarily) at _exactly_ `MAX_REFCOUNT + 1` references.
const MAX_REFCOUNT: usize = (isize::MAX) as usize;

/// The object allocated by an `Arc<T>`
#[repr(C)]
pub(crate) struct ArcInner<T: ?Sized> {
    pub(crate) count: atomic::AtomicUsize,
    pub(crate) data: T,
}

unsafe impl<T: ?Sized + Sync + Send> Send for ArcInner<T> {}
unsafe impl<T: ?Sized + Sync + Send> Sync for ArcInner<T> {}

impl<T: ?Sized> ArcInner<T> {
    /// Compute the offset of the `data` field within `ArcInner<T>`.
    ///
    /// # Safety
    ///
    /// - The pointer must be created from `Arc::into_raw` or similar functions
    /// - The pointee must be initialized (`&*value` must not be UB).
    ///   That happens automatically if the pointer comes from `Arc` and type was not changed.
    ///   This is **not** the case, for example, when `Arc` was uninitialized `MaybeUninit<T>`
    ///   and the pointer was cast to `*const T`.
    unsafe fn offset_of_data(value: *const T) -> usize {
        // We can use `Layout::for_value_raw` when it is stable.
        let value = &*value;

        let layout = Layout::new::<atomic::AtomicUsize>();
        let (_, offset) = layout.extend(Layout::for_value(value)).unwrap();
        offset
    }
}

/// An atomically reference counted shared pointer
///
/// See the documentation for [`Arc`] in the standard library. Unlike the
/// standard library `Arc`, this `Arc` does not support weak reference counting.
///
/// [`Arc`]: https://doc.rust-lang.org/stable/std/sync/struct.Arc.html
#[repr(transparent)]
pub struct Arc<T: ?Sized> {
    pub(crate) p: ptr::NonNull<ArcInner<T>>,
    pub(crate) phantom: PhantomData<T>,
}

unsafe impl<T: ?Sized + Sync + Send> Send for Arc<T> {}
unsafe impl<T: ?Sized + Sync + Send> Sync for Arc<T> {}

impl<T: ?Sized + RefUnwindSafe> UnwindSafe for Arc<T> {}

impl<T> Arc<T> {
    /// Construct an `Arc<T>`
    #[inline]
    pub fn new(data: T) -> Self {
        let ptr = Box::into_raw(Box::new(ArcInner {
            count: atomic::AtomicUsize::new(1),
            data,
        }));

        unsafe {
            Arc {
                p: ptr::NonNull::new_unchecked(ptr),
                phantom: PhantomData,
            }
        }
    }

    /// Temporarily converts |self| into a bonafide OffsetArc and exposes it to the
    /// provided callback. The refcount is not modified.
    #[inline(always)]
    pub fn with_raw_offset_arc<F, U>(&self, f: F) -> U
    where
        F: FnOnce(&OffsetArc<T>) -> U,
    {
        // Synthesize transient Arc, which never touches the refcount of the ArcInner.
        // Store transient in `ManuallyDrop`, to leave the refcount untouched.
        let transient = unsafe { ManuallyDrop::new(Arc::into_raw_offset(ptr::read(self))) };

        // Expose the transient Arc to the callback, which may clone it if it wants.
        f(&transient)
    }

    /// Converts an `Arc` into a `OffsetArc`. This consumes the `Arc`, so the refcount
    /// is not modified.
    #[inline]
    pub fn into_raw_offset(a: Self) -> OffsetArc<T> {
        unsafe {
            OffsetArc {
                ptr: ptr::NonNull::new_unchecked(Arc::into_raw(a) as *mut T),
                phantom: PhantomData,
            }
        }
    }

    /// Converts a `OffsetArc` into an `Arc`. This consumes the `OffsetArc`, so the refcount
    /// is not modified.
    #[inline]
    pub fn from_raw_offset(a: OffsetArc<T>) -> Self {
        let a = ManuallyDrop::new(a);
        let ptr = a.ptr.as_ptr();
        unsafe { Arc::from_raw(ptr) }
    }

    /// Returns the inner value, if the [`Arc`] has exactly one strong reference.
    ///
    /// Otherwise, an [`Err`] is returned with the same [`Arc`] that was
    /// passed in.
    ///
    /// # Examples
    ///
    /// ```
    /// use triomphe::Arc;
    ///
    /// let x = Arc::new(3);
    /// assert_eq!(Arc::try_unwrap(x), Ok(3));
    ///
    /// let x = Arc::new(4);
    /// let _y = Arc::clone(&x);
    /// assert_eq!(*Arc::try_unwrap(x).unwrap_err(), 4);
    /// ```
    pub fn try_unwrap(this: Self) -> Result<T, Self> {
        Self::try_unique(this).map(UniqueArc::into_inner)
    }

    /// Converts the `Arc` to `UniqueArc` if the `Arc` has exactly one strong reference.
    ///
    /// Otherwise, `None` is returned and the `Arc` is dropped.
    ///
    /// If `Arc::into_unique` is called on every clone of this `Arc`, it is guaranteed that exactly one of the calls
    /// returns a `UniqueArc`. This means in particular that the inner data is not dropped. This can be useful when
    /// it is desirable to recover the inner value in a way that does not require coordination amongst the various
    /// copies of `Arc`.
    ///
    /// `Arc::try_unique` is conceptually similar to `Arc::into_unique`, but it is meant for different use-cases. If
    /// used as a direct replacement for `Arc::into_unique`, such as with the expression `Arc::try_unique(this).ok()`,
    /// then it does not give the same guarantee as described in the previous paragraph.
    ///
    /// For more information, see the examples below and read the documentation of `Arc::try_unique`.
    ///
    /// # Examples
    ///
    /// ```
    /// use triomphe::Arc;
    ///
    /// let x = Arc::new(3);
    /// let y = Arc::clone(&x);
    ///
    /// // Two threads calling `Arc::into_inner` on both clones of an `Arc`:
    /// let x_thread = std::thread::spawn(|| Arc::into_unique(x));
    /// let y_thread = std::thread::spawn(|| Arc::into_unique(y));
    ///
    /// let x_unique = x_thread.join().unwrap();
    /// let y_unique = y_thread.join().unwrap();
    ///
    /// // One of the threads is guaranteed to receive the inner value:
    /// assert!((x_unique.is_some() && y_unique.is_none()) || (x_unique.is_none() && y_unique.is_some()));
    /// // The result could also be `(None, None)` if the threads called
    /// // `Arc::try_unique(x).ok()` and `Arc::try_unique(y).ok()` instead.
    /// ```
    pub fn into_unique(this: Self) -> Option<UniqueArc<T>> {
        // Prevent ourselves from being dropped in avoid clashing with the existing drop logic.
        let this = ManuallyDrop::new(this);

        // Update the reference count by decrementing by one, and if we are the last holder of this `Arc` (previous
        // value was one), then we know we now can reconstitute this `Arc` into a `UniqueArc`. Otherwise, there's
        // nothing else for us to do and we return `None` to signal that we weren't the last holder.
        //
        // Unlike `drop_inner`, we use AcqRel ordering on `fetch_sub` (instead of `fetch_sub(Release)` followed by
        // `load(Acquire)`) to end up with the same outcome, just with a single atomic operation instead. This _is_
        // strictly stronger (in terms of synchronization) but is not materially different: we're simply ensuring that
        // any subsequent mutation of the data through `UniqueArc` cannot be ordered _before_ the reference count is
        // updated, which could allow for other threads to see the data in an inconsistent state, ultimately leading to
        // a data race.
        if this.inner().count.fetch_sub(1, AcqRel) != 1 {
            return None;
        }

        // Update the reference count _back_ to one, which upholds the reference count invariant of `UniqueArc`.
        //
        // Since we know we are the only thread accessing this `Arc` at this point, we have no special ordering needs.
        this.inner().count.store(1, Relaxed);

        // SAFETY: The reference count is guaranteed to be one at this point.
        Some(unsafe { UniqueArc::from_arc(ManuallyDrop::into_inner(this)) })
    }
}

impl<T> Arc<[T]> {
    /// Reconstruct the `Arc<[T]>` from a raw pointer obtained from `into_raw()`.
    ///
    /// [`Arc::from_raw`] should accept unsized types, but this is not trivial to do correctly
    /// until the feature [`pointer_bytes_offsets`](https://github.com/rust-lang/rust/issues/96283)
    /// is stabilized. This is stopgap solution for slices.
    ///
    ///  # Safety
    /// - The given pointer must be a valid pointer to `[T]` that came from [`Arc::into_raw`].
    /// - After `from_raw_slice`, the pointer must not be accessed.
    pub unsafe fn from_raw_slice(ptr: *const [T]) -> Self {
        Arc::from_raw(ptr)
    }
}

impl<T: ?Sized> Arc<T> {
    /// Convert the `Arc<T>` to a raw pointer, suitable for use across FFI
    ///
    /// Note: This returns a pointer to the data T, which is offset in the allocation.
    ///
    /// It is recommended to use OffsetArc for this.
    #[inline]
    pub fn into_raw(this: Self) -> *const T {
        let this = ManuallyDrop::new(this);
        this.as_ptr()
    }

    /// Reconstruct the `Arc<T>` from a raw pointer obtained from into_raw()
    ///
    /// Note: This raw pointer will be offset in the allocation and must be preceded
    /// by the atomic count.
    ///
    /// It is recommended to use OffsetArc for this
    ///
    ///  # Safety
    /// - The given pointer must be a valid pointer to `T` that came from [`Arc::into_raw`].
    /// - After `from_raw`, the pointer must not be accessed.
    #[inline]
    pub unsafe fn from_raw(ptr: *const T) -> Self {
        // To find the corresponding pointer to the `ArcInner` we need
        // to subtract the offset of the `data` field from the pointer.

        // SAFETY: `ptr` comes from `ArcInner.data`, so it must be initialized.
        let offset_of_data = ArcInner::<T>::offset_of_data(ptr);

        // SAFETY: `from_raw_inner` expects a pointer to the beginning of the allocation,
        //   not a pointer to data part.
        //  `ptr` points to `ArcInner.data`, so subtraction results
        //   in the beginning of the `ArcInner`, which is the beginning of the allocation.
        let arc_inner_ptr = ptr.byte_sub(offset_of_data);
        Arc::from_raw_inner(arc_inner_ptr as *mut ArcInner<T>)
    }

    /// Returns the raw pointer.
    ///
    /// Same as into_raw except `self` isn't consumed.
    #[inline]
    pub fn as_ptr(&self) -> *const T {
        // SAFETY: This cannot go through a reference to `data`, because this method
        // is used to implement `into_raw`. To reconstruct the full `Arc` from this
        // pointer, it needs to maintain its full provenance, and not be reduced to
        // just the contained `T`.
        unsafe { ptr::addr_of_mut!((*self.ptr()).data) }
    }

    /// Produce a pointer to the data that can be converted back
    /// to an Arc. This is basically an `&Arc<T>`, without the extra indirection.
    /// It has the benefits of an `&T` but also knows about the underlying refcount
    /// and can be converted into more `Arc<T>`s if necessary.
    #[inline]
    pub fn borrow_arc(&self) -> ArcBorrow<'_, T> {
        unsafe { ArcBorrow(NonNull::new_unchecked(self.as_ptr() as *mut T), PhantomData) }
    }

    /// Returns the address on the heap of the Arc itself -- not the T within it -- for memory
    /// reporting.
    pub fn heap_ptr(&self) -> *const c_void {
        self.p.as_ptr() as *const ArcInner<T> as *const c_void
    }

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
        this.inner().count.load(Relaxed)
    }

    #[inline]
    pub(super) fn into_raw_inner(this: Self) -> *mut ArcInner<T> {
        let this = ManuallyDrop::new(this);
        this.ptr()
    }

    /// Construct an `Arc` from an allocated `ArcInner`.
    /// # Safety
    /// The `ptr` must point to a valid instance, allocated by an `Arc`. The reference could will
    /// not be modified.
    pub(super) unsafe fn from_raw_inner(ptr: *mut ArcInner<T>) -> Self {
        Arc {
            p: ptr::NonNull::new_unchecked(ptr),
            phantom: PhantomData,
        }
    }

    #[inline]
    pub(super) fn inner(&self) -> &ArcInner<T> {
        // This unsafety is ok because while this arc is alive we're guaranteed
        // that the inner pointer is valid. Furthermore, we know that the
        // `ArcInner` structure itself is `Sync` because the inner data is
        // `Sync` as well, so we're ok loaning out an immutable pointer to these
        // contents.
        unsafe { &*self.ptr() }
    }

    // Non-inlined part of `drop`. Just invokes the destructor.
    #[inline(never)]
    unsafe fn drop_slow(&mut self) {
        let _ = Box::from_raw(self.ptr());
    }

    /// Returns `true` if the two `Arc`s point to the same allocation in a vein similar to
    /// [`ptr::eq`]. This function ignores the metadata of  `dyn Trait` pointers.
    #[inline]
    pub fn ptr_eq(this: &Self, other: &Self) -> bool {
        ptr::addr_eq(this.ptr(), other.ptr())
    }

    pub(crate) fn ptr(&self) -> *mut ArcInner<T> {
        self.p.as_ptr()
    }

    /// Allocates an `ArcInner<T>` with sufficient space for
    /// a possibly-unsized inner value where the value has the layout provided.
    ///
    /// The function `mem_to_arcinner` is called with the data pointer
    /// and must return back a (potentially fat)-pointer for the `ArcInner<T>`.
    ///
    /// ## Safety
    ///
    /// `mem_to_arcinner` must return the same pointer, the only things that can change are
    /// - its type
    /// - its metadata
    ///
    /// `value_layout` must be correct for `T`.
    #[allow(unused_unsafe)]
    pub(super) unsafe fn allocate_for_layout(
        value_layout: Layout,
        mem_to_arcinner: impl FnOnce(*mut u8) -> *mut ArcInner<T>,
    ) -> NonNull<ArcInner<T>> {
        let layout = Layout::new::<ArcInner<()>>()
            .extend(value_layout)
            .unwrap()
            .0
            .pad_to_align();

        // Safety: we propagate safety requirements to the caller
        unsafe {
            Arc::try_allocate_for_layout(value_layout, mem_to_arcinner)
                .unwrap_or_else(|_| handle_alloc_error(layout))
        }
    }

    /// Allocates an `ArcInner<T>` with sufficient space for
    /// a possibly-unsized inner value where the value has the layout provided,
    /// returning an error if allocation fails.
    ///
    /// The function `mem_to_arcinner` is called with the data pointer
    /// and must return back a (potentially fat)-pointer for the `ArcInner<T>`.
    ///
    /// ## Safety
    ///
    /// `mem_to_arcinner` must return the same pointer, the only things that can change are
    /// - its type
    /// - its metadata
    ///
    /// `value_layout` must be correct for `T`.
    #[allow(unused_unsafe)]
    unsafe fn try_allocate_for_layout(
        value_layout: Layout,
        mem_to_arcinner: impl FnOnce(*mut u8) -> *mut ArcInner<T>,
    ) -> Result<NonNull<ArcInner<T>>, ()> {
        let layout = Layout::new::<ArcInner<()>>()
            .extend(value_layout)
            .unwrap()
            .0
            .pad_to_align();

        let ptr = NonNull::new(alloc::alloc::alloc(layout)).ok_or(())?;

        // Initialize the ArcInner
        let inner = mem_to_arcinner(ptr.as_ptr());
        debug_assert_eq!(unsafe { Layout::for_value(&*inner) }, layout);

        unsafe {
            ptr::write(&mut (*inner).count, atomic::AtomicUsize::new(1));
        }

        // Safety: `ptr` is checked to be non-null,
        //         `inner` is the same as `ptr` (per the safety requirements of this function)
        unsafe { Ok(NonNull::new_unchecked(inner)) }
    }
}

impl<H, T> Arc<HeaderSlice<H, [T]>> {
    pub(super) fn allocate_for_header_and_slice(
        len: usize,
    ) -> NonNull<ArcInner<HeaderSlice<H, [T]>>> {
        let layout = Layout::new::<H>()
            .extend(Layout::array::<T>(len).unwrap())
            .unwrap()
            .0
            .pad_to_align();

        unsafe {
            // Safety:
            // - the provided closure does not change the pointer (except for meta & type)
            // - the provided layout is valid for `HeaderSlice<H, [T]>`
            Arc::allocate_for_layout(layout, |mem| {
                // Synthesize the fat pointer. We do this by claiming we have a direct
                // pointer to a [T], and then changing the type of the borrow. The key
                // point here is that the length portion of the fat pointer applies
                // only to the number of elements in the dynamically-sized portion of
                // the type, so the value will be the same whether it points to a [T]
                // or something else with a [T] as its last member.
                let fake_slice = ptr::slice_from_raw_parts_mut(mem as *mut T, len);
                fake_slice as *mut ArcInner<HeaderSlice<H, [T]>>
            })
        }
    }
}

impl<T> Arc<MaybeUninit<T>> {
    /// Create an Arc contains an `MaybeUninit<T>`.
    pub fn new_uninit() -> Self {
        Arc::new(MaybeUninit::<T>::uninit())
    }

    /// Calls `MaybeUninit::write` on the value contained.
    ///
    /// ## Panics
    ///
    /// If the `Arc` is not unique.
    #[deprecated(
        since = "0.1.7",
        note = "this function previously was UB and now panics for non-unique `Arc`s. Use `UniqueArc::write` instead."
    )]
    #[track_caller]
    pub fn write(&mut self, val: T) -> &mut T {
        UniqueArc::write(must_be_unique(self), val)
    }

    /// Obtain a mutable pointer to the stored `MaybeUninit<T>`.
    pub fn as_mut_ptr(&mut self) -> *mut MaybeUninit<T> {
        unsafe { &mut (*self.ptr()).data }
    }

    /// # Safety
    ///
    /// Must initialize all fields before calling this function.
    #[inline]
    pub unsafe fn assume_init(self) -> Arc<T> {
        Arc::from_raw_inner(ManuallyDrop::new(self).ptr().cast())
    }
}

impl<T> Arc<[MaybeUninit<T>]> {
    /// Create an Arc contains an array `[MaybeUninit<T>]` of `len`.
    pub fn new_uninit_slice(len: usize) -> Self {
        UniqueArc::new_uninit_slice(len).shareable()
    }

    /// Obtain a mutable slice to the stored `[MaybeUninit<T>]`.
    #[deprecated(
        since = "0.1.8",
        note = "this function previously was UB and now panics for non-unique `Arc`s. Use `UniqueArc` or `get_mut` instead."
    )]
    #[track_caller]
    pub fn as_mut_slice(&mut self) -> &mut [MaybeUninit<T>] {
        must_be_unique(self)
    }

    /// # Safety
    ///
    /// Must initialize all fields before calling this function.
    #[inline]
    pub unsafe fn assume_init(self) -> Arc<[T]> {
        Arc::from_raw_inner(ManuallyDrop::new(self).ptr() as _)
    }
}

impl<T: ?Sized> Clone for Arc<T> {
    #[inline]
    fn clone(&self) -> Self {
        // Using a relaxed ordering is alright here, as knowledge of the
        // original reference prevents other threads from erroneously deleting
        // the object.
        //
        // As explained in the [Boost documentation][1], Increasing the
        // reference counter can always be done with memory_order_relaxed: New
        // references to an object can only be formed from an existing
        // reference, and passing an existing reference from one thread to
        // another must already provide any required synchronization.
        //
        // [1]: (www.boost.org/doc/libs/1_55_0/doc/html/atomic/usage_examples.html)
        let old_size = self.inner().count.fetch_add(1, Relaxed);

        // However we need to guard against massive refcounts in case someone
        // is `mem::forget`ing Arcs. If we don't do this the count can overflow
        // and users will use-after free. We racily saturate to `isize::MAX` on
        // the assumption that there aren't ~2 billion threads incrementing
        // the reference count at once. This branch will never be taken in
        // any realistic program.
        //
        // We abort because such a program is incredibly degenerate, and we
        // don't care to support it.
        if old_size > MAX_REFCOUNT {
            abort();
        }

        unsafe {
            Arc {
                p: ptr::NonNull::new_unchecked(self.ptr()),
                phantom: PhantomData,
            }
        }
    }
}

impl<T: ?Sized> Deref for Arc<T> {
    type Target = T;

    #[inline]
    fn deref(&self) -> &T {
        &self.inner().data
    }
}

impl<T: Clone> Arc<T> {
    /// Makes a mutable reference to the `Arc`, cloning if necessary
    ///
    /// This is functionally equivalent to [`Arc::make_mut`][mm] from the standard library.
    ///
    /// If this `Arc` is uniquely owned, `make_mut()` will provide a mutable
    /// reference to the contents. If not, `make_mut()` will create a _new_ `Arc`
    /// with a copy of the contents, update `this` to point to it, and provide
    /// a mutable reference to its contents.
    ///
    /// This is useful for implementing copy-on-write schemes where you wish to
    /// avoid copying things if your `Arc` is not shared.
    ///
    /// [mm]: https://doc.rust-lang.org/stable/std/sync/struct.Arc.html#method.make_mut
    #[inline]
    pub fn make_mut(this: &mut Self) -> &mut T {
        if !this.is_unique() {
            // Another pointer exists; clone
            *this = Arc::new(T::clone(this));
        }

        unsafe {
            // This unsafety is ok because we're guaranteed that the pointer
            // returned is the *only* pointer that will ever be returned to T. Our
            // reference count is guaranteed to be 1 at this point, and we required
            // the Arc itself to be `mut`, so we're returning the only possible
            // reference to the inner data.
            &mut (*this.ptr()).data
        }
    }

    /// Makes a `UniqueArc` from an `Arc`, cloning if necessary.
    ///
    /// If this `Arc` is uniquely owned, `make_unique()` will provide a `UniqueArc`
    /// containing `this`. If not, `make_unique()` will create a _new_ `Arc`
    /// with a copy of the contents, update `this` to point to it, and provide
    /// a `UniqueArc` to it.
    ///
    /// This is useful for implementing copy-on-write schemes where you wish to
    /// avoid copying things if your `Arc` is not shared.
    #[inline]
    pub fn make_unique(this: &mut Self) -> &mut UniqueArc<T> {
        if !this.is_unique() {
            // Another pointer exists; clone
            *this = Arc::new(T::clone(this));
        }

        unsafe {
            // Safety: this is either unique or just created (which is also unique)
            UniqueArc::from_arc_ref(this)
        }
    }

    /// If we have the only reference to `T` then unwrap it. Otherwise, clone `T` and return the clone.
    ///
    /// Assuming `arc_t` is of type `Arc<T>`, this function is functionally equivalent to `(*arc_t).clone()`, but will avoid cloning the inner value where possible.
    pub fn unwrap_or_clone(this: Arc<T>) -> T {
        Self::try_unwrap(this).unwrap_or_else(|this| T::clone(&this))
    }
}

impl<T: ?Sized> Arc<T> {
    /// Provides mutable access to the contents _if_ the `Arc` is uniquely owned.
    #[inline]
    pub fn get_mut(this: &mut Self) -> Option<&mut T> {
        if this.is_unique() {
            unsafe {
                // See make_mut() for documentation of the threadsafety here.
                Some(&mut (*this.ptr()).data)
            }
        } else {
            None
        }
    }

    /// Provides unique access to the arc _if_ the `Arc` is uniquely owned.
    pub fn get_unique(this: &mut Self) -> Option<&mut UniqueArc<T>> {
        Self::try_as_unique(this).ok()
    }

    /// Whether or not the `Arc` is uniquely owned (is the refcount 1?).
    pub fn is_unique(&self) -> bool {
        // See the extensive discussion in [1] for why this needs to be Acquire.
        //
        // [1] https://github.com/servo/servo/issues/21186
        Self::count(self) == 1
    }

    /// Gets the number of [`Arc`] pointers to this allocation
    pub fn count(this: &Self) -> usize {
        this.inner().count.load(Acquire)
    }

    /// Returns a [`UniqueArc`] if the [`Arc`] has exactly one strong reference.
    ///
    /// Otherwise, an [`Err`] is returned with the same [`Arc`] that was
    /// passed in.
    ///
    /// # Examples
    ///
    /// ```
    /// use triomphe::{Arc, UniqueArc};
    ///
    /// let x = Arc::new(3);
    /// assert_eq!(UniqueArc::into_inner(Arc::try_unique(x).unwrap()), 3);
    ///
    /// let x = Arc::new(4);
    /// let _y = Arc::clone(&x);
    /// assert_eq!(
    ///     *Arc::try_unique(x).map(UniqueArc::into_inner).unwrap_err(),
    ///     4,
    /// );
    /// ```
    pub fn try_unique(this: Self) -> Result<UniqueArc<T>, Self> {
        if this.is_unique() {
            // Safety: The current arc is unique and making a `UniqueArc`
            //         from it is sound
            unsafe { Ok(UniqueArc::from_arc(this)) }
        } else {
            Err(this)
        }
    }

    pub(crate) fn try_as_unique(this: &mut Self) -> Result<&mut UniqueArc<T>, &mut Self> {
        if this.is_unique() {
            // Safety: The current arc is unique and making a `UniqueArc`
            //         from it is sound
            unsafe { Ok(UniqueArc::from_arc_ref(this)) }
        } else {
            Err(this)
        }
    }

    fn drop_inner(&mut self) {
        // Because `fetch_sub` is already atomic, we do not need to synchronize
        // with other threads unless we are going to delete the object.
        if self.inner().count.fetch_sub(1, Release) != 1 {
            return;
        }

        // This fence is needed to prevent reordering of use of the data and
        // deletion of the data. Because it is marked `Release`, the decreasing
        // of the reference count synchronizes with this `Acquire` fence. This
        // means that use of the data happens before decreasing the reference
        // count, which happens before this fence, which happens before the
        // deletion of the data.
        //
        // As explained in the [Boost documentation][1],
        //
        // > It is important to enforce any possible access to the object in one
        // > thread (through an existing reference) to *happen before* deleting
        // > the object in a different thread. This is achieved by a "release"
        // > operation after dropping a reference (any access to the object
        // > through this reference must obviously happened before), and an
        // > "acquire" operation before deleting the object.
        //
        // In particular, while the contents of an Arc are usually immutable, it's
        // possible to have interior writes to something like a Mutex<T>. Since a
        // Mutex is not acquired when it is deleted, we can't rely on its
        // synchronization logic to make writes in thread A visible to a destructor
        // running in thread B.
        //
        // [1]: (www.boost.org/doc/libs/1_55_0/doc/html/atomic/usage_examples.html)
        atomic::fence(Acquire);

        unsafe {
            self.drop_slow();
        }
    }
}

#[cfg(not(feature = "unstable_dropck_eyepatch"))]
impl<T: ?Sized> Drop for Arc<T> {
    #[inline]
    fn drop(&mut self) {
        self.drop_inner();
    }
}

// SAFETY: We do not access the inner `T`, so we are fine to drop Arc with an already dropped T.
#[cfg(feature = "unstable_dropck_eyepatch")]
unsafe impl<#[may_dangle] T: ?Sized> Drop for Arc<T> {
    #[inline]
    fn drop(&mut self) {
        self.drop_inner();
    }
}

impl<T: ?Sized + PartialEq> PartialEq for Arc<T> {
    fn eq(&self, other: &Arc<T>) -> bool {
        // TODO: pointer equality is incorrect if `T` is not `Eq`.
        Self::ptr_eq(self, other) || *(*self) == *(*other)
    }

    #[allow(clippy::partialeq_ne_impl)]
    fn ne(&self, other: &Arc<T>) -> bool {
        !Self::ptr_eq(self, other) && *(*self) != *(*other)
    }
}

impl<T: ?Sized + PartialOrd> PartialOrd for Arc<T> {
    fn partial_cmp(&self, other: &Arc<T>) -> Option<Ordering> {
        (**self).partial_cmp(&**other)
    }

    fn lt(&self, other: &Arc<T>) -> bool {
        *(*self) < *(*other)
    }

    fn le(&self, other: &Arc<T>) -> bool {
        *(*self) <= *(*other)
    }

    fn gt(&self, other: &Arc<T>) -> bool {
        *(*self) > *(*other)
    }

    fn ge(&self, other: &Arc<T>) -> bool {
        *(*self) >= *(*other)
    }
}

impl<T: ?Sized + Ord> Ord for Arc<T> {
    fn cmp(&self, other: &Arc<T>) -> Ordering {
        (**self).cmp(&**other)
    }
}

impl<T: ?Sized + Eq> Eq for Arc<T> {}

impl<T: ?Sized + fmt::Display> fmt::Display for Arc<T> {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        fmt::Display::fmt(&**self, f)
    }
}

impl<T: ?Sized + fmt::Debug> fmt::Debug for Arc<T> {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        fmt::Debug::fmt(&**self, f)
    }
}

impl<T: ?Sized> fmt::Pointer for Arc<T> {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        fmt::Pointer::fmt(&self.ptr(), f)
    }
}

impl<T: Default> Default for Arc<T> {
    #[inline]
    fn default() -> Arc<T> {
        Arc::new(Default::default())
    }
}

impl<T: ?Sized + Hash> Hash for Arc<T> {
    fn hash<H: Hasher>(&self, state: &mut H) {
        (**self).hash(state)
    }
}

impl<T> From<T> for Arc<T> {
    #[inline]
    fn from(t: T) -> Self {
        Arc::new(t)
    }
}

impl<A> FromIterator<A> for Arc<[A]> {
    fn from_iter<T: IntoIterator<Item = A>>(iter: T) -> Self {
        UniqueArc::from_iter(iter).shareable()
    }
}

impl<T: ?Sized> borrow::Borrow<T> for Arc<T> {
    #[inline]
    fn borrow(&self) -> &T {
        self
    }
}

impl<T: ?Sized> AsRef<T> for Arc<T> {
    #[inline]
    fn as_ref(&self) -> &T {
        self
    }
}

#[cfg(feature = "stable_deref_trait")]
unsafe impl<T: ?Sized> StableDeref for Arc<T> {}
#[cfg(feature = "stable_deref_trait")]
unsafe impl<T: ?Sized> CloneStableDeref for Arc<T> {}

#[cfg(feature = "serde")]
impl<'de, T: Deserialize<'de>> Deserialize<'de> for Arc<T> {
    fn deserialize<D>(deserializer: D) -> Result<Arc<T>, D::Error>
    where
        D: ::serde::de::Deserializer<'de>,
    {
        T::deserialize(deserializer).map(Arc::new)
    }
}

#[cfg(feature = "serde")]
impl<T: Serialize> Serialize for Arc<T> {
    fn serialize<S>(&self, serializer: S) -> Result<S::Ok, S::Error>
    where
        S: ::serde::ser::Serializer,
    {
        (**self).serialize(serializer)
    }
}

// Safety:
// This implementation must guarantee that it is sound to call replace_ptr with an unsized variant
// of the pointer retuned in `as_sized_ptr`. The basic property of Unsize coercion is that safety
// variants and layout is unaffected. The Arc does not rely on any other property of T. This makes
// any unsized ArcInner valid for being shared with the sized variant.
// This does _not_ mean that any T can be unsized into an U, but rather than if such unsizing is
// possible then it can be propagated into the Arc<T>.
#[cfg(feature = "unsize")]
unsafe impl<T, U: ?Sized> unsize::CoerciblePtr<U> for Arc<T> {
    type Pointee = T;
    type Output = Arc<U>;

    fn as_sized_ptr(&mut self) -> *mut T {
        // Returns a pointer to the complete inner. The unsizing itself won't care about the
        // pointer value and promises not to offset it.
        self.p.as_ptr() as *mut T
    }

    unsafe fn replace_ptr(self, new: *mut U) -> Arc<U> {
        // Fix the provenance by ensuring that of `self` is used.
        let inner = ManuallyDrop::new(self);
        let p = inner.p.as_ptr() as *mut T;
        // Safety: This points to an ArcInner of the previous self and holds shared ownership since
        // the old pointer never decremented the reference count. The caller upholds that `new` is
        // an unsized version of the previous ArcInner. This assumes that unsizing to the fat
        // pointer tag of an `ArcInner<U>` and `U` is isomorphic under a direct pointer cast since
        // in reality we unsized *mut T to *mut U at the address of the ArcInner. This is the case
        // for all currently envisioned unsized types where the tag of T and ArcInner<T> are simply
        // the same.
        Arc::from_raw_inner(p.replace_ptr(new) as *mut ArcInner<U>)
    }
}

#[track_caller]
fn must_be_unique<T: ?Sized>(arc: &mut Arc<T>) -> &mut UniqueArc<T> {
    match Arc::try_as_unique(arc) {
        Ok(unique) => unique,
        Err(this) => panic!("`Arc` must be unique in order for this operation to be safe, there are currently {} copies", Arc::count(this)),
    }
}

#[cfg(test)]
mod tests {
    use crate::arc::Arc;
    use alloc::borrow::ToOwned;
    use alloc::string::String;
    use alloc::vec::Vec;
    use core::iter::FromIterator;
    use core::mem::MaybeUninit;
    #[cfg(feature = "unsize")]
    use unsize::{CoerceUnsize, Coercion};

    #[test]
    fn try_unwrap() {
        let x = Arc::new(100usize);
        let y = x.clone();

        // The count should be two so `try_unwrap()` should fail
        assert_eq!(Arc::count(&x), 2);
        assert!(Arc::try_unwrap(x).is_err());

        // Since `x` has now been dropped, the count should be 1
        // and `try_unwrap()` should succeed
        assert_eq!(Arc::count(&y), 1);
        assert_eq!(Arc::try_unwrap(y), Ok(100));
    }

    #[test]
    #[cfg(feature = "unsize")]
    fn coerce_to_slice() {
        let x = Arc::new([0u8; 4]);
        let y: Arc<[u8]> = x.clone().unsize(Coercion::to_slice());
        assert_eq!((*x).as_ptr(), (*y).as_ptr());
    }

    #[test]
    #[cfg(feature = "unsize")]
    fn coerce_to_dyn() {
        let x: Arc<_> = Arc::new(|| 42u32);
        let x: Arc<_> = x.unsize(Coercion::<_, dyn Fn() -> u32>::to_fn());
        assert_eq!((*x)(), 42);
    }

    #[test]
    #[allow(deprecated)]
    fn maybeuninit() {
        let mut arc: Arc<MaybeUninit<_>> = Arc::new_uninit();
        arc.write(999);

        let arc = unsafe { arc.assume_init() };
        assert_eq!(*arc, 999);
    }

    #[test]
    #[allow(deprecated)]
    #[should_panic = "`Arc` must be unique in order for this operation to be safe"]
    fn maybeuninit_ub_to_proceed() {
        let mut uninit = Arc::new_uninit();
        let clone = uninit.clone();

        let x: &MaybeUninit<String> = &clone;

        // This write invalidates `x` reference
        uninit.write(String::from("nonononono"));

        // Read invalidated reference to trigger UB
        let _read = &*x;
    }

    #[test]
    #[allow(deprecated)]
    #[should_panic = "`Arc` must be unique in order for this operation to be safe"]
    fn maybeuninit_slice_ub_to_proceed() {
        let mut uninit = Arc::new_uninit_slice(13);
        let clone = uninit.clone();

        let x: &[MaybeUninit<String>] = &clone;

        // This write invalidates `x` reference
        uninit.as_mut_slice()[0].write(String::from("nonononono"));

        // Read invalidated reference to trigger UB
        let _read = &*x;
    }

    #[test]
    fn maybeuninit_array() {
        let mut arc: Arc<[MaybeUninit<_>]> = Arc::new_uninit_slice(5);
        assert!(arc.is_unique());
        #[allow(deprecated)]
        for (uninit, index) in arc.as_mut_slice().iter_mut().zip(0..5) {
            let ptr = uninit.as_mut_ptr();
            unsafe { core::ptr::write(ptr, index) };
        }

        let arc = unsafe { arc.assume_init() };
        assert!(arc.is_unique());
        // Using clone to that the layout generated in new_uninit_slice is compatible
        // with ArcInner.
        let arcs = [
            arc.clone(),
            arc.clone(),
            arc.clone(),
            arc.clone(),
            arc.clone(),
        ];
        assert_eq!(6, Arc::count(&arc));
        // If the layout is not compatible, then the data might be corrupted.
        assert_eq!(*arc, [0, 1, 2, 3, 4]);

        // Drop the arcs and check the count and the content to
        // make sure it isn't corrupted.
        drop(arcs);
        assert!(arc.is_unique());
        assert_eq!(*arc, [0, 1, 2, 3, 4]);
    }

    #[test]
    fn roundtrip() {
        let arc: Arc<usize> = Arc::new(0usize);
        let ptr = Arc::into_raw(arc);
        unsafe {
            let _arc = Arc::from_raw(ptr);
        }
    }

    #[test]
    fn from_iterator_exact_size() {
        let arc = Arc::from_iter(Vec::from_iter(["ololo".to_owned(), "trololo".to_owned()]));
        assert_eq!(1, Arc::count(&arc));
        assert_eq!(["ololo".to_owned(), "trololo".to_owned()], *arc);
    }

    #[test]
    fn from_iterator_unknown_size() {
        let arc = Arc::from_iter(
            Vec::from_iter(["ololo".to_owned(), "trololo".to_owned()])
                .into_iter()
                // Filter is opaque to iterators, so the resulting iterator
                // will report lower bound of 0.
                .filter(|_| true),
        );
        assert_eq!(1, Arc::count(&arc));
        assert_eq!(["ololo".to_owned(), "trololo".to_owned()], *arc);
    }

    #[test]
    fn roundtrip_slice() {
        let arc = Arc::from(Vec::from_iter([17, 19]));
        let ptr = Arc::into_raw(arc);
        let arc = unsafe { Arc::from_raw_slice(ptr) };
        assert_eq!([17, 19], *arc);
        assert_eq!(1, Arc::count(&arc));
    }

    #[test]
    fn arc_eq_and_cmp() {
        [
            [("*", &b"AB"[..]), ("*", &b"ab"[..])],
            [("*", &b"AB"[..]), ("*", &b"a"[..])],
            [("*", &b"A"[..]), ("*", &b"ab"[..])],
            [("A", &b"*"[..]), ("a", &b"*"[..])],
            [("a", &b"*"[..]), ("A", &b"*"[..])],
            [("AB", &b"*"[..]), ("a", &b"*"[..])],
            [("A", &b"*"[..]), ("ab", &b"*"[..])],
        ]
        .iter()
        .for_each(|[lt @ (lh, ls), rt @ (rh, rs)]| {
            let l = Arc::from_header_and_slice(lh, ls);
            let r = Arc::from_header_and_slice(rh, rs);

            assert_eq!(l, l);
            assert_eq!(r, r);

            assert_ne!(l, r);
            assert_ne!(r, l);

            assert_eq!(l <= l, lt <= lt, "{lt:?} <= {lt:?}");
            assert_eq!(l >= l, lt >= lt, "{lt:?} >= {lt:?}");

            assert_eq!(l < l, lt < lt, "{lt:?} < {lt:?}");
            assert_eq!(l > l, lt > lt, "{lt:?} > {lt:?}");

            assert_eq!(r <= r, rt <= rt, "{rt:?} <= {rt:?}");
            assert_eq!(r >= r, rt >= rt, "{rt:?} >= {rt:?}");

            assert_eq!(r < r, rt < rt, "{rt:?} < {rt:?}");
            assert_eq!(r > r, rt > rt, "{rt:?} > {rt:?}");

            assert_eq!(l < r, lt < rt, "{lt:?} < {rt:?}");
            assert_eq!(r > l, rt > lt, "{rt:?} > {lt:?}");
        })
    }

    #[test]
    fn arc_eq_and_partial_cmp() {
        [
            [(0.0, &[0.0, 0.0][..]), (1.0, &[0.0, 0.0][..])],
            [(1.0, &[0.0, 0.0][..]), (0.0, &[0.0, 0.0][..])],
            [(0.0, &[0.0][..]), (0.0, &[0.0, 0.0][..])],
            [(0.0, &[0.0, 0.0][..]), (0.0, &[0.0][..])],
            [(0.0, &[1.0, 2.0][..]), (0.0, &[10.0, 20.0][..])],
        ]
        .iter()
        .for_each(|[lt @ (lh, ls), rt @ (rh, rs)]| {
            let l = Arc::from_header_and_slice(lh, ls);
            let r = Arc::from_header_and_slice(rh, rs);

            assert_eq!(l, l);
            assert_eq!(r, r);

            assert_ne!(l, r);
            assert_ne!(r, l);

            assert_eq!(l <= l, lt <= lt, "{lt:?} <= {lt:?}");
            assert_eq!(l >= l, lt >= lt, "{lt:?} >= {lt:?}");

            assert_eq!(l < l, lt < lt, "{lt:?} < {lt:?}");
            assert_eq!(l > l, lt > lt, "{lt:?} > {lt:?}");

            assert_eq!(r <= r, rt <= rt, "{rt:?} <= {rt:?}");
            assert_eq!(r >= r, rt >= rt, "{rt:?} >= {rt:?}");

            assert_eq!(r < r, rt < rt, "{rt:?} < {rt:?}");
            assert_eq!(r > r, rt > rt, "{rt:?} > {rt:?}");

            assert_eq!(l < r, lt < rt, "{lt:?} < {rt:?}");
            assert_eq!(r > l, rt > lt, "{rt:?} > {lt:?}");
        })
    }

    #[test]
    fn test_strong_count() {
        let arc = Arc::new(17);
        assert_eq!(1, Arc::strong_count(&arc));
        let arc2 = arc.clone();
        assert_eq!(2, Arc::strong_count(&arc));
        drop(arc);
        assert_eq!(1, Arc::strong_count(&arc2));
    }

    #[test]
    fn test_partial_eq_bug() {
        let float = f32::NAN;
        assert_ne!(float, float);
        let arc = Arc::new(f32::NAN);
        // TODO: this is a bug.
        assert_eq!(arc, arc);
    }

    #[test]
    fn test_into_raw_from_raw_dst() {
        trait AnInteger {
            fn get_me_an_integer(&self) -> u64;
        }

        impl AnInteger for u32 {
            fn get_me_an_integer(&self) -> u64 {
                *self as u64
            }
        }

        let arc = Arc::<u32>::new(19);
        let data = Arc::into_raw(arc);
        let data: *const dyn AnInteger = data as *const _;
        let arc: Arc<dyn AnInteger> = unsafe { Arc::from_raw(data) };
        assert_eq!(19, arc.get_me_an_integer());
    }

    #[test]
    fn into_unique() {
        let arc = Arc::new(42);
        assert_eq!(1, Arc::count(&arc));

        let arc2 = Arc::clone(&arc);

        assert_eq!(2, Arc::count(&arc));

        let arc2_unique = Arc::into_unique(arc2);
        assert!(arc2_unique.is_none());
        assert_eq!(1, Arc::count(&arc));

        let arc_unique = Arc::into_unique(arc).unwrap();
        assert_eq!(42, *arc_unique);
    }

    #[cfg(feature = "std")]
    #[test]
    fn into_unique_data_race_no_sleep() {
        // Exists to be exercised by Miri to check for data races.
        let a = Arc::new(0);
        let b = a.clone();
        std::thread::spawn(move || {
            let _value = *b;
        });
        std::thread::spawn(move || {
            *Arc::into_unique(a).unwrap() += 1;
        });
    }

    #[cfg(feature = "std")]
    #[test]
    fn into_unique_data_race_sleep() {
        // Exists to be exercised by Miri to check for data races.
        let a = Arc::new(0);
        let b = a.clone();
        let t1 = std::thread::spawn(move || {
            let _value = *b;
        });
        let t2 = std::thread::spawn(move || {
            std::thread::sleep(std::time::Duration::from_millis(100));
            if let Some(mut u) = Arc::into_unique(a) {
                *u += 1
            }
        });
        t1.join().unwrap();
        t2.join().unwrap();
    }

    #[allow(dead_code)]
    const fn is_partial_ord<T: ?Sized + PartialOrd>() {}

    #[allow(dead_code)]
    const fn is_ord<T: ?Sized + Ord>() {}

    // compile-time check that PartialOrd/Ord is correctly derived
    const _: () = is_partial_ord::<Arc<f64>>();
    const _: () = is_ord::<Arc<u64>>();
}
