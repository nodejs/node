use alloc::vec::Vec;
use alloc::{alloc::Layout, boxed::Box};
use core::convert::TryFrom;
use core::iter::FromIterator;
use core::marker::PhantomData;
use core::mem::{ManuallyDrop, MaybeUninit};
use core::ops::{Deref, DerefMut};
use core::ptr::{self, addr_of_mut, NonNull};
use core::sync::atomic::AtomicUsize;

#[cfg(feature = "serde")]
use serde::{Deserialize, Serialize};

use crate::iterator_as_exact_size_iterator::IteratorAsExactSizeIterator;
use crate::HeaderSlice;

use super::{Arc, ArcInner};

/// An `Arc` that is known to be uniquely owned
///
/// When `Arc`s are constructed, they are known to be
/// uniquely owned. In such a case it is safe to mutate
/// the contents of the `Arc`. Normally, one would just handle
/// this by mutating the data on the stack before allocating the
/// `Arc`, however it's possible the data is large or unsized
/// and you need to heap-allocate it earlier in such a way
/// that it can be freely converted into a regular `Arc` once you're
/// done.
///
/// `UniqueArc` exists for this purpose, when constructed it performs
/// the same allocations necessary for an `Arc`, however it allows mutable access.
/// Once the mutation is finished, you can call `.shareable()` and get a regular `Arc`
/// out of it.
///
/// ```rust
/// # use triomphe::UniqueArc;
/// let data = [1, 2, 3, 4, 5];
/// let mut x = UniqueArc::new(data);
/// x[4] = 7; // mutate!
/// let y = x.shareable(); // y is an Arc<T>
/// ```
#[repr(transparent)]
pub struct UniqueArc<T: ?Sized>(Arc<T>);

// Uniquene ownership means that we can support weaker bounds than `T: Send + Sync`.
// Thus, these impls can follow the precedent of std's `Box`, not `Arc`.
unsafe impl<T: ?Sized + Send> Send for UniqueArc<T> {}
unsafe impl<T: ?Sized + Sync> Sync for UniqueArc<T> {}

impl<T> UniqueArc<T> {
    #[inline]
    /// Construct a new UniqueArc
    pub fn new(data: T) -> Self {
        UniqueArc(Arc::new(data))
    }

    /// Construct an uninitialized arc
    #[inline]
    pub fn new_uninit() -> UniqueArc<MaybeUninit<T>> {
        unsafe {
            let layout = Layout::new::<ArcInner<MaybeUninit<T>>>();
            let ptr = alloc::alloc::alloc(layout);
            let mut p = NonNull::new(ptr)
                .unwrap_or_else(|| alloc::alloc::handle_alloc_error(layout))
                .cast::<ArcInner<MaybeUninit<T>>>();
            ptr::write(&mut p.as_mut().count, AtomicUsize::new(1));

            UniqueArc(Arc {
                p,
                phantom: PhantomData,
            })
        }
    }

    /// Gets the inner value of the unique arc
    pub fn into_inner(this: Self) -> T {
        // Wrap the Arc in a `ManuallyDrop` so that its drop routine never runs
        let this = ManuallyDrop::new(this.0);
        debug_assert!(
            this.is_unique(),
            "attempted to call `.into_inner()` on a `UniqueArc` with a non-zero ref count",
        );

        // Safety: We have exclusive access to the inner data and the
        //         arc will not perform its drop routine since we've
        //         wrapped it in a `ManuallyDrop`
        unsafe { Box::from_raw(this.ptr()).data }
    }
}

impl<T: ?Sized> UniqueArc<T> {
    /// Convert to a shareable `Arc<T>` once we're done mutating it
    #[inline]
    pub fn shareable(self) -> Arc<T> {
        self.0
    }

    /// Creates a new [`UniqueArc`] from the given [`Arc`].
    ///
    /// An unchecked alternative to `Arc::try_unique()`
    ///
    /// # Safety
    ///
    /// The given `Arc` must have a reference count of exactly one
    ///
    pub(crate) unsafe fn from_arc(arc: Arc<T>) -> Self {
        debug_assert_eq!(Arc::count(&arc), 1);
        Self(arc)
    }

    /// Creates a new `&mut `[`UniqueArc`] from the given `&mut `[`Arc`].
    ///
    /// An unchecked alternative to `Arc::try_as_unique()`
    ///
    /// # Safety
    ///
    /// The given `Arc` must have a reference count of exactly one
    pub(crate) unsafe fn from_arc_ref(arc: &mut Arc<T>) -> &mut Self {
        debug_assert_eq!(Arc::count(arc), 1);

        // Safety: caller guarantees that `arc` is unique,
        //         `UniqueArc` is `repr(transparent)`
        &mut *(arc as *mut Arc<T> as *mut UniqueArc<T>)
    }
}

impl<T> UniqueArc<MaybeUninit<T>> {
    /// Calls `MaybeUninit::write` on the contained value.
    pub fn write(&mut self, val: T) -> &mut T {
        unsafe {
            // Casting *mut MaybeUninit<T> -> *mut T is always fine
            let ptr = self.as_mut_ptr() as *mut T;

            // Safety: We have exclusive access to the inner data
            ptr.write(val);

            // Safety: the pointer was just written to
            &mut *ptr
        }
    }

    /// Obtain a mutable pointer to the stored `MaybeUninit<T>`.
    pub fn as_mut_ptr(&mut self) -> *mut MaybeUninit<T> {
        unsafe { &mut (*self.0.ptr()).data }
    }

    /// Convert to an initialized Arc.
    ///
    /// # Safety
    ///
    /// This function is equivalent to `MaybeUninit::assume_init` and has the
    /// same safety requirements. You are responsible for ensuring that the `T`
    /// has actually been initialized before calling this method.
    #[inline]
    pub unsafe fn assume_init(this: Self) -> UniqueArc<T> {
        UniqueArc(Arc {
            p: ManuallyDrop::new(this).0.p.cast(),
            phantom: PhantomData,
        })
    }
}

impl<T> UniqueArc<[MaybeUninit<T>]> {
    /// Create an Arc contains an array `[MaybeUninit<T>]` of `len`.
    pub fn new_uninit_slice(len: usize) -> Self {
        // Safety (although no unsafe is required):
        // - `ArcInner` is properly allocated and initialized.
        //   - `()` and `[MaybeUninit<T>]` do not require special initialization
        // - The `Arc` is just created and so -- unique.
        let arc: Arc<HeaderSlice<(), [MaybeUninit<T>]>> =
            UniqueArc::from_header_and_uninit_slice((), len).0;
        let arc: Arc<[MaybeUninit<T>]> = arc.into();
        UniqueArc(arc)
    }

    /// # Safety
    ///
    /// Must initialize all fields before calling this function.
    #[inline]
    pub unsafe fn assume_init_slice(Self(this): Self) -> UniqueArc<[T]> {
        UniqueArc(this.assume_init())
    }
}

impl<H, T> UniqueArc<HeaderSlice<H, [MaybeUninit<T>]>> {
    /// Creates an Arc for a HeaderSlice using the given header struct and allocated space
    /// for an unitialized slice of length `len`.
    #[inline]
    pub fn from_header_and_uninit_slice(header: H, len: usize) -> Self {
        let inner = Arc::<HeaderSlice<H, [MaybeUninit<T>]>>::allocate_for_header_and_slice(len);

        unsafe {
            // Safety: inner is a valid pointer, so this can't go out of bounds
            let dst = addr_of_mut!((*inner.as_ptr()).data.header);

            // Safety: `dst` is valid for writes (just allocated)
            ptr::write(dst, header);
        }

        // Safety: ptr is valid & the inner structure is initialized.
        // We wrote the header above and the slice can stay unitialized as it's [MaybeUninit<T>]
        Self(Arc {
            p: inner,
            phantom: PhantomData,
        })
    }

    /// # Safety
    ///
    /// Must initialize all fields before calling this function.
    #[inline]
    pub unsafe fn assume_init_slice_with_header(self) -> UniqueArc<HeaderSlice<H, [T]>> {
        unsafe { core::mem::transmute(self) }
    }
}

impl<T: ?Sized> TryFrom<Arc<T>> for UniqueArc<T> {
    type Error = Arc<T>;

    fn try_from(arc: Arc<T>) -> Result<Self, Self::Error> {
        Arc::try_unique(arc)
    }
}

impl<T: ?Sized> Deref for UniqueArc<T> {
    type Target = T;

    #[inline]
    fn deref(&self) -> &T {
        &self.0
    }
}

impl<T: ?Sized> DerefMut for UniqueArc<T> {
    #[inline]
    fn deref_mut(&mut self) -> &mut T {
        // We know this to be uniquely owned
        unsafe { &mut (*self.0.ptr()).data }
    }
}

impl<A> FromIterator<A> for UniqueArc<[A]> {
    fn from_iter<T: IntoIterator<Item = A>>(iter: T) -> Self {
        let iter = iter.into_iter();
        let (lower, upper) = iter.size_hint();
        let arc: Arc<[A]> = if Some(lower) == upper {
            let iter = IteratorAsExactSizeIterator::new(iter);
            Arc::from_header_and_iter((), iter).into()
        } else {
            let vec = iter.collect::<Vec<_>>();
            Arc::from(vec)
        };
        // Safety: We just created an `Arc`, so it's unique.
        unsafe { UniqueArc::from_arc(arc) }
    }
}

// Safety:
// This leverages the correctness of Arc's CoerciblePtr impl. Additionally, we must ensure that
// this can not be used to violate the safety invariants of UniqueArc, which require that we can not
// duplicate the Arc, such that replace_ptr returns a valid instance. This holds since it consumes
// a unique owner of the contained ArcInner.
#[cfg(feature = "unsize")]
unsafe impl<T, U: ?Sized> unsize::CoerciblePtr<U> for UniqueArc<T> {
    type Pointee = T;
    type Output = UniqueArc<U>;

    fn as_sized_ptr(&mut self) -> *mut T {
        // Dispatch to the contained field.
        unsize::CoerciblePtr::<U>::as_sized_ptr(&mut self.0)
    }

    unsafe fn replace_ptr(self, new: *mut U) -> UniqueArc<U> {
        // Dispatch to the contained field, work around conflict of destructuring and Drop.
        let inner = ManuallyDrop::new(self);
        UniqueArc(ptr::read(&inner.0).replace_ptr(new))
    }
}

#[cfg(feature = "serde")]
impl<'de, T: Deserialize<'de>> Deserialize<'de> for UniqueArc<T> {
    fn deserialize<D>(deserializer: D) -> Result<UniqueArc<T>, D::Error>
    where
        D: ::serde::de::Deserializer<'de>,
    {
        T::deserialize(deserializer).map(UniqueArc::new)
    }
}

#[cfg(feature = "serde")]
impl<T: Serialize> Serialize for UniqueArc<T> {
    fn serialize<S>(&self, serializer: S) -> Result<S::Ok, S::Error>
    where
        S: ::serde::ser::Serializer,
    {
        (**self).serialize(serializer)
    }
}

#[cfg(test)]
mod tests {
    use crate::{Arc, HeaderSliceWithLengthUnchecked, HeaderWithLength, UniqueArc};
    use core::{convert::TryFrom, mem::MaybeUninit};

    #[test]
    fn unique_into_inner() {
        let unique = UniqueArc::new(10u64);
        assert_eq!(UniqueArc::into_inner(unique), 10);
    }

    #[test]
    fn try_from_arc() {
        let x = Arc::new(10_000);
        let y = x.clone();

        assert!(UniqueArc::try_from(x).is_err());
        assert_eq!(
            UniqueArc::into_inner(UniqueArc::try_from(y).unwrap()),
            10_000,
        );
    }

    #[test]
    #[allow(deprecated)]
    fn maybeuninit_smoke() {
        let mut arc: UniqueArc<MaybeUninit<_>> = UniqueArc::new_uninit();
        arc.write(999);

        let arc = unsafe { UniqueArc::assume_init(arc) };
        assert_eq!(*arc, 999);
    }

    #[test]
    fn from_header_and_uninit_slice() {
        let mut uarc: UniqueArc<HeaderSliceWithLengthUnchecked<u8, MaybeUninit<u16>>> =
            UniqueArc::from_header_and_uninit_slice(HeaderWithLength::new(1, 3), 3);
        uarc.slice.fill(MaybeUninit::new(2));
        let arc = unsafe { uarc.assume_init_slice_with_header() }.shareable();
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
        // Similar for ThinArc
        let thin = Arc::into_thin(arc.clone());
        assert_eq!(7, Arc::count(&arc));
        // If the layout is not compatible, then the data might be corrupted.
        assert_eq!(arc.header.header, 1);
        assert_eq!(&arc.slice, [2, 2, 2]);
        assert_eq!(thin.header.header, 1);
        assert_eq!(&thin.slice, [2, 2, 2]);

        // Drop the arcs and check the count and the content to
        // make sure it isn't corrupted.
        drop(arcs);
        drop(thin);
        assert!(arc.is_unique());
        assert_eq!(arc.header.header, 1);
        assert_eq!(&arc.slice, [2, 2, 2]);
    }
}
