use alloc::alloc::Layout;
use alloc::boxed::Box;
use alloc::string::String;
use alloc::vec::Vec;
use core::cmp::Ordering;
use core::iter::{ExactSizeIterator, Iterator};
use core::marker::PhantomData;
use core::mem::{self, ManuallyDrop};
use core::ptr::{self, addr_of_mut};

use super::{Arc, ArcInner};

/// Structure to allow Arc-managing some fixed-sized data and a variably-sized
/// slice in a single allocation.
#[derive(Debug, Copy, Clone, Eq, PartialEq, Hash, PartialOrd, Ord)]
#[repr(C)]
pub struct HeaderSlice<H, T: ?Sized> {
    /// The fixed-sized data.
    pub header: H,

    /// The dynamically-sized data.
    pub slice: T,
}

impl<H, T> Arc<HeaderSlice<H, [T]>> {
    /// Creates an Arc for a HeaderSlice using the given header struct and
    /// iterator to generate the slice. The resulting Arc will be fat.
    pub fn from_header_and_iter<I>(header: H, mut items: I) -> Self
    where
        I: Iterator<Item = T> + ExactSizeIterator,
    {
        assert_ne!(mem::size_of::<T>(), 0, "Need to think about ZST");

        let num_items = items.len();

        let inner = Arc::allocate_for_header_and_slice(num_items);

        unsafe {
            // Write the data.
            //
            // Note that any panics here (i.e. from the iterator) are safe, since
            // we'll just leak the uninitialized memory.
            ptr::write(&mut ((*inner.as_ptr()).data.header), header);
            let mut current = (*inner.as_ptr()).data.slice.as_mut_ptr();
            for _ in 0..num_items {
                ptr::write(
                    current,
                    items
                        .next()
                        .expect("ExactSizeIterator over-reported length"),
                );
                current = current.offset(1);
            }
            assert!(
                items.next().is_none(),
                "ExactSizeIterator under-reported length"
            );
        }

        // Safety: ptr is valid & the inner structure is fully initialized
        Arc {
            p: inner,
            phantom: PhantomData,
        }
    }

    /// Creates an Arc for a HeaderSlice using the given header struct and
    /// iterator to generate the slice. The resulting Arc will be fat.
    pub fn from_header_and_slice(header: H, items: &[T]) -> Self
    where
        T: Copy,
    {
        assert_ne!(mem::size_of::<T>(), 0, "Need to think about ZST");

        let num_items = items.len();

        let inner = Arc::allocate_for_header_and_slice(num_items);

        unsafe {
            // Write the data.
            ptr::write(&mut ((*inner.as_ptr()).data.header), header);
            let dst = (*inner.as_ptr()).data.slice.as_mut_ptr();
            ptr::copy_nonoverlapping(items.as_ptr(), dst, num_items);
        }

        // Safety: ptr is valid & the inner structure is fully initialized
        Arc {
            p: inner,
            phantom: PhantomData,
        }
    }

    /// Creates an Arc for a HeaderSlice using the given header struct and
    /// vec to generate the slice. The resulting Arc will be fat.
    pub fn from_header_and_vec(header: H, mut v: Vec<T>) -> Self {
        let len = v.len();

        let inner = Arc::allocate_for_header_and_slice(len);

        unsafe {
            // Safety: inner is a valid pointer, so this can't go out of bounds
            let dst = addr_of_mut!((*inner.as_ptr()).data.header);

            // Safety: `dst` is valid for writes (just allocated)
            ptr::write(dst, header);
        }

        unsafe {
            let src = v.as_mut_ptr();

            // Safety: inner is a valid pointer, so this can't go out of bounds
            let dst = addr_of_mut!((*inner.as_ptr()).data.slice) as *mut T;

            // Safety:
            // - `src` is valid for reads for `len` (got from `Vec`)
            // - `dst` is valid for writes for `len` (just allocated, with layout for appropriate slice)
            // - `src` and `dst` don't overlap (separate allocations)
            ptr::copy_nonoverlapping(src, dst, len);

            // Deallocate vec without dropping `T`
            //
            // Safety: 0..0 elements are always initialized, 0 <= cap for any cap
            v.set_len(0);
        }

        // Safety: ptr is valid & the inner structure is fully initialized
        Arc {
            p: inner,
            phantom: PhantomData,
        }
    }
}

impl<H> Arc<HeaderSlice<H, str>> {
    /// Creates an Arc for a HeaderSlice using the given header struct and
    /// a str slice to generate the slice. The resulting Arc will be fat.
    pub fn from_header_and_str(header: H, string: &str) -> Self {
        let bytes = Arc::from_header_and_slice(header, string.as_bytes());

        // Safety: `ArcInner` and `HeaderSlice` are `repr(C)`, `str` has the same layout as `[u8]`,
        //         thus it's ok to "transmute" between `Arc<HeaderSlice<H, [u8]>>` and `Arc<HeaderSlice<H, str>>`.
        //
        //         `bytes` are a valid string since we've just got them from a valid `str`.
        unsafe { Arc::from_raw_inner(Arc::into_raw_inner(bytes) as _) }
    }
}

/// Header data with an inline length. Consumers that use HeaderWithLength as the
/// Header type in HeaderSlice can take advantage of ThinArc.
#[derive(Debug, Copy, Clone, Eq, PartialEq, Hash)]
#[repr(C)]
pub struct HeaderWithLength<H> {
    /// The fixed-sized data.
    pub header: H,

    /// The slice length.
    pub length: usize,
}

impl<H> HeaderWithLength<H> {
    /// Creates a new HeaderWithLength.
    #[inline]
    pub fn new(header: H, length: usize) -> Self {
        HeaderWithLength { header, length }
    }
}

impl<T: ?Sized> From<Arc<HeaderSlice<(), T>>> for Arc<T> {
    fn from(this: Arc<HeaderSlice<(), T>>) -> Self {
        debug_assert_eq!(
            Layout::for_value::<HeaderSlice<(), T>>(&this),
            Layout::for_value::<T>(&this.slice)
        );

        // Safety: `HeaderSlice<(), T>` and `T` has the same layout
        unsafe { Arc::from_raw_inner(Arc::into_raw_inner(this) as _) }
    }
}

impl<T: ?Sized> From<Arc<T>> for Arc<HeaderSlice<(), T>> {
    fn from(this: Arc<T>) -> Self {
        // Safety: `T` and `HeaderSlice<(), T>` has the same layout
        unsafe { Arc::from_raw_inner(Arc::into_raw_inner(this) as _) }
    }
}

impl<T: Copy> From<&[T]> for Arc<[T]> {
    fn from(slice: &[T]) -> Self {
        Arc::from_header_and_slice((), slice).into()
    }
}

impl From<&str> for Arc<str> {
    fn from(s: &str) -> Self {
        Arc::from_header_and_str((), s).into()
    }
}

impl From<String> for Arc<str> {
    fn from(s: String) -> Self {
        Self::from(&s[..])
    }
}

// FIXME: once `pointer::with_metadata_of` is stable or
//        implementable on stable without assuming ptr layout
//        this will be able to accept `T: ?Sized`.
impl<T> From<Box<T>> for Arc<T> {
    fn from(b: Box<T>) -> Self {
        let layout = Layout::for_value::<T>(&b);

        // Safety: the closure only changes the type of the pointer
        let inner = unsafe { Self::allocate_for_layout(layout, |mem| mem as *mut ArcInner<T>) };

        unsafe {
            let src = Box::into_raw(b);

            // Safety: inner is a valid pointer, so this can't go out of bounds
            let dst = addr_of_mut!((*inner.as_ptr()).data);

            // Safety:
            // - `src` is valid for reads (got from `Box`)
            // - `dst` is valid for writes (just allocated)
            // - `src` and `dst` don't overlap (separate allocations)
            ptr::copy_nonoverlapping(src, dst, 1);

            // Deallocate box without dropping `T`
            //
            // Safety:
            // - `src` has been got from `Box::into_raw`
            // - `ManuallyDrop<T>` is guaranteed to have the same layout as `T`
            drop(Box::<ManuallyDrop<T>>::from_raw(src as _));
        }

        Arc {
            p: inner,
            phantom: PhantomData,
        }
    }
}

impl<T> From<Vec<T>> for Arc<[T]> {
    fn from(v: Vec<T>) -> Self {
        Arc::from_header_and_vec((), v).into()
    }
}

/// A type wrapping `HeaderSlice<HeaderWithLength<H>, T>` that is used internally in `ThinArc`.
///
/// # Safety
///
/// Safety-usable invariants:
///
/// - This is guaranteed to have the same representation as `HeaderSlice<HeaderWithLength<H>, [T]>`
/// - The header length (`.length()`) is checked to be the slice length
#[derive(Debug, Hash, Eq, PartialEq, Ord, PartialOrd)]
#[repr(transparent)]
pub struct HeaderSliceWithLengthProtected<H, T> {
    // Invariant: the header's length field must be the slice length
    inner: HeaderSliceWithLengthUnchecked<H, T>,
}

pub(crate) type HeaderSliceWithLengthUnchecked<H, T> = HeaderSlice<HeaderWithLength<H>, [T]>;

impl<H, T> HeaderSliceWithLengthProtected<H, T> {
    pub fn header(&self) -> &H {
        &self.inner.header.header
    }
    pub fn header_mut(&mut self) -> &mut H {
        // Safety: only the length is unsafe to mutate
        &mut self.inner.header.header
    }
    pub fn length(&self) -> usize {
        self.inner.header.length
    }

    pub fn slice(&self) -> &[T] {
        &self.inner.slice
    }
    pub fn slice_mut(&mut self) -> &mut [T] {
        // Safety: only the length is unsafe to mutate
        &mut self.inner.slice
    }
    pub(crate) fn inner(&self) -> &HeaderSliceWithLengthUnchecked<H, T> {
        // This is safe in an immutable context
        &self.inner
    }
}

impl<H: PartialOrd, T: ?Sized + PartialOrd> PartialOrd for HeaderSlice<HeaderWithLength<H>, T> {
    fn partial_cmp(&self, other: &Self) -> Option<Ordering> {
        (&self.header.header, &self.slice).partial_cmp(&(&other.header.header, &other.slice))
    }
}

impl<H: Ord, T: ?Sized + Ord> Ord for HeaderSlice<HeaderWithLength<H>, T> {
    fn cmp(&self, other: &Self) -> Ordering {
        (&self.header.header, &self.slice).cmp(&(&other.header.header, &other.slice))
    }
}

#[cfg(test)]
mod tests {
    use alloc::boxed::Box;
    use alloc::string::String;
    use alloc::vec;
    use core::iter;

    use crate::{Arc, HeaderSlice};

    #[test]
    fn from_header_and_iter_smoke() {
        let arc = Arc::from_header_and_iter(
            (42u32, 17u8),
            IntoIterator::into_iter([1u16, 2, 3, 4, 5, 6, 7]),
        );

        assert_eq!(arc.header, (42, 17));
        assert_eq!(arc.slice, [1, 2, 3, 4, 5, 6, 7]);
    }

    #[test]
    fn from_header_and_slice_smoke() {
        let arc = Arc::from_header_and_slice((42u32, 17u8), &[1u16, 2, 3, 4, 5, 6, 7]);

        assert_eq!(arc.header, (42, 17));
        assert_eq!(arc.slice, [1u16, 2, 3, 4, 5, 6, 7]);
    }

    #[test]
    fn from_header_and_vec_smoke() {
        let arc = Arc::from_header_and_vec((42u32, 17u8), vec![1u16, 2, 3, 4, 5, 6, 7]);

        assert_eq!(arc.header, (42, 17));
        assert_eq!(arc.slice, [1u16, 2, 3, 4, 5, 6, 7]);
    }

    #[test]
    fn from_header_and_iter_empty() {
        let arc = Arc::from_header_and_iter((42u32, 17u8), iter::empty::<u16>());

        assert_eq!(arc.header, (42, 17));
        assert_eq!(arc.slice, []);
    }

    #[test]
    fn from_header_and_slice_empty() {
        let arc = Arc::from_header_and_slice((42u32, 17u8), &[1u16; 0]);

        assert_eq!(arc.header, (42, 17));
        assert_eq!(arc.slice, []);
    }

    #[test]
    fn from_header_and_vec_empty() {
        let arc = Arc::from_header_and_vec((42u32, 17u8), vec![1u16; 0]);

        assert_eq!(arc.header, (42, 17));
        assert_eq!(arc.slice, []);
    }

    #[test]
    fn issue_13_empty() {
        crate::Arc::from_header_and_iter((), iter::empty::<usize>());
    }

    #[test]
    fn issue_13_consumption() {
        let s: &[u8] = &[0u8; 255];
        crate::Arc::from_header_and_iter((), s.iter().copied());
    }

    #[test]
    fn from_header_and_str_smoke() {
        let a = Arc::from_header_and_str(
            42,
            "The answer to the ultimate question of life, the universe, and everything",
        );
        assert_eq!(a.header, 42);
        assert_eq!(
            &a.slice,
            "The answer to the ultimate question of life, the universe, and everything"
        );

        let empty = Arc::from_header_and_str((), "");
        assert_eq!(&empty.slice, "");
    }

    #[test]
    fn erase_and_create_from_thin_air_header() {
        let a: Arc<HeaderSlice<(), [u32]>> = Arc::from_header_and_slice((), &[12, 17, 16]);
        let b: Arc<[u32]> = a.into();

        assert_eq!(&*b, [12, 17, 16]);

        let c: Arc<HeaderSlice<(), [u32]>> = b.into();

        assert_eq!(&c.slice, [12, 17, 16]);
    }

    #[test]
    fn from_box_and_vec() {
        let b = Box::new(String::from("xxx"));
        let b = Arc::<String>::from(b);
        assert_eq!(&*b, "xxx");

        let v = vec![String::from("1"), String::from("2"), String::from("3")];
        let v = Arc::<[_]>::from(v);
        assert_eq!(
            &*v,
            [String::from("1"), String::from("2"), String::from("3")]
        );

        let mut v = vec![String::from("1"), String::from("2"), String::from("3")];
        v.reserve(10);
        let v = Arc::<[_]>::from(v);
        assert_eq!(
            &*v,
            [String::from("1"), String::from("2"), String::from("3")]
        );
    }

    /// It’s possible to make a generic `Arc` wrapper that supports both:
    ///
    /// * `T: !Sized`
    /// * `Arc::make_mut` if `T: Sized`
    #[test]
    fn dst_and_make_mut() {
        struct MyArc<T: ?Sized>(Arc<HeaderSlice<MyHeader, T>>);

        #[derive(Clone)]
        struct MyHeader {
            // Very interesting things go here
        }

        // MyArc<str> is possible
        let dst: MyArc<str> = MyArc(Arc::from_header_and_str(MyHeader {}, "example"));
        assert_eq!(&dst.0.slice, "example");

        // `make_mut` is still available when `T: Sized`
        let mut answer: MyArc<u32> = MyArc(Arc::new(HeaderSlice {
            header: MyHeader {},
            // Not actually a slice in this case,
            // but `HeaderSlice` is required to use `from_header_and_str`
            // and we want the same `MyArc` to support both cases.
            slice: 6 * 9,
        }));
        let mut_ref = Arc::make_mut(&mut answer.0);
        mut_ref.slice = 42;
    }
}
