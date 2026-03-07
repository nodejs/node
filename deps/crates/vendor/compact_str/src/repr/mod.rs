use core::str::Utf8Error;
use core::{
    mem,
    ptr,
};
use std::borrow::Cow;

#[cfg(feature = "bytes")]
mod bytes;
#[cfg(feature = "smallvec")]
mod smallvec;

mod capacity;
mod heap;
mod inline;
mod iter;
mod nonmax;
mod num;
mod traits;

use capacity::Capacity;
use heap::HeapBuffer;
use inline::InlineBuffer;
use nonmax::NonMaxU8;
pub use traits::IntoRepr;

/// The max size of a string we can fit inline
pub const MAX_SIZE: usize = std::mem::size_of::<String>();
/// Used as a discriminant to identify different variants
pub const HEAP_MASK: u8 = 0b11111110;
/// When our string is stored inline, we represent the length of the string in the last byte, offset
/// by `LENGTH_MASK`
pub const LENGTH_MASK: u8 = 0b11000000;

const EMPTY: Repr = Repr::new_inline("");

#[repr(C)]
pub struct Repr(
    // We have a pointer in the repesentation to properly carry provenance
    *const (),
    // Then we need two `usize`s (aka WORDs) of data, for the first we just define a `usize`...
    usize,
    // ...but the second we breakup into multiple pieces...
    #[cfg(target_pointer_width = "64")] u32,
    u16,
    u8,
    // ...so that the last byte can be a NonMax, which allows the compiler to see a niche value
    NonMaxU8,
);

unsafe impl Send for Repr {}
unsafe impl Sync for Repr {}

impl Repr {
    #[inline]
    pub fn new(text: &str) -> Self {
        let len = text.len();

        if len == 0 {
            EMPTY
        } else if len <= MAX_SIZE {
            // SAFETY: We checked that the length of text is less than or equal to MAX_SIZE
            let inline = unsafe { InlineBuffer::new(text) };
            Repr::from_inline(inline)
        } else {
            let heap = HeapBuffer::new(text);
            Repr::from_heap(heap)
        }
    }

    #[inline]
    pub const fn new_inline(text: &str) -> Self {
        let len = text.len();

        if len <= MAX_SIZE {
            let inline = InlineBuffer::new_const(text);
            Repr::from_inline(inline)
        } else {
            panic!("Inline string was too long, max length is `std::mem::size_of::<CompactString>()` bytes");
        }
    }

    /// Create a [`Repr`] with the provided `capacity`
    #[inline]
    pub fn with_capacity(capacity: usize) -> Self {
        if capacity <= MAX_SIZE {
            EMPTY
        } else {
            let heap = HeapBuffer::with_capacity(capacity);
            Repr::from_heap(heap)
        }
    }

    /// Create a [`Repr`] from a slice of bytes that is UTF-8
    #[inline]
    pub fn from_utf8<B: AsRef<[u8]>>(buf: B) -> Result<Self, Utf8Error> {
        // Get a &str from the Vec, failing if it's not valid UTF-8
        let s = core::str::from_utf8(buf.as_ref())?;
        // Construct a Repr from the &str
        Ok(Self::new(s))
    }

    /// Create a [`Repr`] from a slice of bytes that is UTF-8, without validating that it is indeed
    /// UTF-8
    ///
    /// # Safety
    /// * The caller must guarantee that `buf` is valid UTF-8
    #[inline]
    pub unsafe fn from_utf8_unchecked<B: AsRef<[u8]>>(buf: B) -> Self {
        let bytes = buf.as_ref();
        let bytes_len = bytes.len();

        // Create a Repr with enough capacity for the entire buffer
        let mut repr = Repr::with_capacity(bytes_len);

        // There's an edge case where the final byte of this buffer == `HEAP_MASK`, which is
        // invalid UTF-8, but would result in us creating an inline variant, that identifies as
        // a heap variant. If a user ever tried to reference the data at all, we'd incorrectly
        // try and read data from an invalid memory address, causing undefined behavior.
        if bytes_len == MAX_SIZE {
            let last_byte = bytes[bytes_len - 1];
            // If we hit the edge case, reserve additional space to make the repr becomes heap
            // allocated, which prevents us from writing this last byte inline
            if last_byte >= 0b11000000 {
                repr.reserve(MAX_SIZE + 1);
            }
        }

        // SAFETY: The caller is responsible for making sure the provided buffer is UTF-8. This
        // invariant is documented in the public API
        let slice = repr.as_mut_buf();
        // write the chunk into the Repr
        slice[..bytes_len].copy_from_slice(bytes);

        // Set the length of the Repr
        // SAFETY: We just wrote the entire `buf` into the Repr
        repr.set_len(bytes_len);

        repr
    }

    /// Create a [`Repr`] from a [`String`], in `O(1)` time. We'll attempt to inline the string
    /// if `should_inline` is `true`
    ///
    /// Note: If the provided [`String`] is >16 MB and we're on a 32-bit arch, we'll copy the
    /// `String`.
    #[inline]
    pub fn from_string(s: String, should_inline: bool) -> Self {
        let og_cap = s.capacity();
        let cap = Capacity::new(og_cap);

        #[cold]
        fn capacity_on_heap(s: String) -> Repr {
            let heap = HeapBuffer::new(s.as_str());
            Repr::from_heap(heap)
        }

        #[cold]
        fn empty() -> Repr {
            EMPTY
        }

        if cap.is_heap() {
            // We only hit this case if the provided String is > 16MB and we're on a 32-bit arch. We
            // expect it to be unlikely, thus we hint that to the compiler
            capacity_on_heap(s)
        } else if og_cap == 0 {
            // We don't expect converting from an empty String often, so we make this code path cold
            empty()
        } else if should_inline && s.len() <= MAX_SIZE {
            // SAFETY: Checked to make sure the string would fit inline
            let inline = unsafe { InlineBuffer::new(s.as_str()) };
            Repr::from_inline(inline)
        } else {
            let mut s = mem::ManuallyDrop::new(s.into_bytes());
            let len = s.len();
            let raw_ptr = s.as_mut_ptr();

            let ptr = ptr::NonNull::new(raw_ptr).expect("string with capacity has null ptr?");
            let heap = HeapBuffer { ptr, len, cap };

            Repr::from_heap(heap)
        }
    }

    /// Converts a [`Repr`] into a [`String`], in `O(1)` time, if possible
    #[inline]
    pub fn into_string(self) -> String {
        let last_byte = self.last_byte();

        #[cold]
        fn into_string_heap(this: HeapBuffer) -> String {
            // SAFETY: We know pointer is valid for `length` bytes
            let slice = unsafe { core::slice::from_raw_parts(this.ptr.as_ptr(), this.len) };
            // SAFETY: A `Repr` contains valid UTF-8
            let s = unsafe { core::str::from_utf8_unchecked(slice) };

            String::from(s)
        }

        if last_byte == HEAP_MASK {
            // SAFTEY: we just checked that the discriminant indicates we're a HeapBuffer
            let heap_buffer = unsafe { self.into_heap() };

            if heap_buffer.cap.is_heap() {
                // We don't expect capacity to be on the heap often, so we mark it as cold
                into_string_heap(heap_buffer)
            } else {
                // Wrap the BoxString in a ManuallyDrop so the underlying buffer doesn't get freed
                let this = mem::ManuallyDrop::new(heap_buffer);

                // SAFETY: We checked above to make sure capacity is valid
                let cap = unsafe { this.cap.as_usize() };

                // SAFETY:
                // * The memory in `ptr` was previously allocated by the same allocator the standard
                //   library uses, with a required alignment of exactly 1.
                // * `length` is less than or equal to capacity, due to internal invaraints.
                // * `capacity` is correctly maintained internally.
                // * `BoxString` only ever contains valid UTF-8.
                unsafe { String::from_raw_parts(this.ptr.as_ptr(), this.len, cap) }
            }
        } else {
            let pointer = &self as *const _ as *const u8;
            let length = core::cmp::min((last_byte.wrapping_sub(LENGTH_MASK)) as usize, MAX_SIZE);

            // SAFETY: We know pointer is valid for `length` bytes
            let slice = unsafe { core::slice::from_raw_parts(pointer, length) };
            // SAFETY: A `Repr` contains valid UTF-8
            let s = unsafe { core::str::from_utf8_unchecked(slice) };

            String::from(s)
        }
    }

    /// Reserves at least `additional` bytes. If there is already enough capacity to store
    /// `additional` bytes this is a no-op
    #[inline]
    pub fn reserve(&mut self, additional: usize) {
        let len = self.len();
        let needed_capacity = len
            .checked_add(additional)
            .expect("Attempted to reserve more than 'usize' bytes");

        if needed_capacity < self.capacity() {
            // we already have enough space, no-op
            return;
        }

        if needed_capacity <= MAX_SIZE {
            // It's possible to have a `Repr` that is heap allocated with a capacity less than
            // MAX_SIZE, if that `Repr` was created From a String or Box<str>
            //
            // SAFTEY: Our needed_capacity is >= our length, which is <= than MAX_SIZE
            let inline = unsafe { InlineBuffer::new(self.as_str()) };
            *self = Repr::from_inline(inline);
        } else if !self.is_heap_allocated() {
            // We're not heap allocated, but need to be, create a HeapBuffer
            let heap = HeapBuffer::with_additional(self.as_str(), additional);
            *self = Repr::from_heap(heap);
        } else {
            // We're already heap allocated, but we need more capacity
            //
            // SAFETY: We checked above to see if we're heap allocated
            let heap_buffer = unsafe { self.as_mut_heap() };

            // To reduce allocations, we amortize our growth
            let amortized_capacity = heap::amortized_growth(len, additional);
            // Attempt to grow our capacity, allocating a new HeapBuffer on failure
            if heap_buffer.realloc(amortized_capacity).is_err() {
                // Create a new HeapBuffer
                let heap = HeapBuffer::with_additional(self.as_str(), additional);
                *self = Repr::from_heap(heap);
            }
        }
    }

    pub fn shrink_to(&mut self, min_capacity: usize) {
        let last_byte = self.last_byte();

        // Note: We can't shrink the inline variant since it's buffer is a fixed size, so we only
        // take action here if our string is heap allocated
        if last_byte == HEAP_MASK {
            // SAFETY: We just checked the discriminant to make sure we're heap allocated
            let heap = unsafe { self.as_mut_heap() };

            let old_capacity = heap.capacity();
            let new_capacity = heap.len.max(min_capacity);

            if new_capacity <= MAX_SIZE {
                // String can be inlined.
                let mut inline = InlineBuffer::empty();
                // SAFETY: Our src is on the heap, so it does not overlap with our new inline
                // buffer, and the src is a `Repr` so we can assume it's valid UTF-8
                unsafe {
                    inline
                        .0
                        .as_mut_ptr()
                        .copy_from_nonoverlapping(heap.ptr.as_ptr(), heap.len)
                };
                // SAFETY: The src we wrote from was a `Repr` which we can assume is valid UTF-8
                unsafe { inline.set_len(heap.len) }
                *self = Repr::from_inline(inline);
            } else if new_capacity < old_capacity {
                // String can be shrunk.
                // We can ignore the result. The string keeps its old capacity, but that's okay.
                let _ = heap.realloc(new_capacity);
            }
        }
    }

    #[inline]
    pub fn push_str(&mut self, s: &str) {
        let len = self.len();
        let str_len = s.len();

        // Reserve at least enough space to fit `s`
        self.reserve(str_len);

        // SAFTEY: `s` which we're appending to the buffer, is valid UTF-8
        let slice = unsafe { self.as_mut_buf() };
        let push_buffer = &mut slice[len..len + str_len];

        debug_assert_eq!(push_buffer.len(), s.as_bytes().len());

        // Copy the string into our buffer
        push_buffer.copy_from_slice(s.as_bytes());

        // Increment the length of our string
        //
        // SAFETY: We appened `s` which is valid UTF-8, and if our size became greater than
        // MAX_SIZE, our call to reserve would make us heap allocated
        unsafe { self.set_len(len + str_len) };
    }

    #[inline]
    pub fn pop(&mut self) -> Option<char> {
        let ch = self.as_str().chars().rev().next()?;

        // SAFETY: We know this is is a valid length which falls on a char boundary
        unsafe { self.set_len(self.len() - ch.len_utf8()) };

        Some(ch)
    }

    /// Returns the string content, and only the string content, as a slice of bytes.
    #[inline]
    pub fn as_slice(&self) -> &[u8] {
        // the last byte stores our discriminant and stack length
        let last_byte = self.last_byte();

        // initially has the value of the stack pointer, conditionally becomes the heap pointer
        let mut pointer = self as *const Self as *const u8;
        let heap_pointer = self.0 as *const u8;

        // initially has the value of the stack length, conditionally becomes the heap length
        let mut length = core::cmp::min((last_byte.wrapping_sub(LENGTH_MASK)) as usize, MAX_SIZE);
        let heap_length = self.1;

        // our discriminant is stored in the last byte and denotes stack vs heap
        //
        // Note: We should never add an `else` statement here, keeping the conditional simple allows
        // the compiler to optimize this to a conditional-move instead of a branch
        if last_byte == HEAP_MASK {
            pointer = heap_pointer;
            length = heap_length;
        }

        // SAFETY: We know the data is valid, aligned, and part of the same contiguous allocated
        // chunk. It's also valid for the lifetime of self
        unsafe { core::slice::from_raw_parts(pointer, length) }
    }

    #[inline]
    pub fn as_str(&self) -> &str {
        // SAFETY: A `Repr` contains valid UTF-8
        unsafe { core::str::from_utf8_unchecked(self.as_slice()) }
    }

    /// Returns the length of the string that we're storing
    #[allow(clippy::len_without_is_empty)] // is_empty exists on CompactString
    #[inline]
    pub fn len(&self) -> usize {
        // the last byte stores our discriminant and stack length
        let last_byte = self.last_byte();

        // initially has the value of the stack length, conditionally becomes the heap length
        let mut length = core::cmp::min((last_byte.wrapping_sub(LENGTH_MASK)) as usize, MAX_SIZE);
        let heap_length = self.1;
        let length_ref = &mut length;

        // our discriminant is stored in the last byte and denotes stack vs heap
        //
        // Note: We should never add an `else` statement here, keeping the conditional simple allows
        // the compiler to optimize this to a conditional-move instead of a branch
        if last_byte == HEAP_MASK {
            *length_ref = heap_length;
        }

        *length_ref
    }

    /// Returns the overall capacity of the underlying buffer
    #[inline]
    pub fn capacity(&self) -> usize {
        // the last byte stores our discriminant and stack length
        let last_byte = self.last_byte();

        #[cold]
        fn heap_capacity(this: &Repr) -> usize {
            // SAFETY: We just checked the discriminant to make sure we're heap allocated
            let heap_buffer = unsafe { this.as_heap() };
            heap_buffer.capacity()
        }

        if last_byte == HEAP_MASK {
            heap_capacity(self)
        } else {
            MAX_SIZE
        }
    }

    #[inline(always)]
    pub fn is_heap_allocated(&self) -> bool {
        let last_byte = self.last_byte();
        last_byte == HEAP_MASK
    }

    /// Return a mutable reference to the entirely underlying buffer
    ///
    /// # Safety
    /// * Callers must guarantee that any modifications made to the buffer are valid UTF-8
    pub unsafe fn as_mut_buf(&mut self) -> &mut [u8] {
        // the last byte stores our discriminant and stack length
        let last_byte = self.last_byte();

        let (ptr, cap) = if last_byte == HEAP_MASK {
            // SAFETY: We just checked the discriminant to make sure we're heap allocated
            let heap_buffer = self.as_heap();
            let ptr = heap_buffer.ptr.as_ptr();
            let cap = heap_buffer.capacity();

            (ptr, cap)
        } else {
            let ptr = self as *mut Self as *mut u8;
            (ptr, MAX_SIZE)
        };

        // SAFETY: Our data is valid for `cap` bytes, and is initialized
        core::slice::from_raw_parts_mut(ptr, cap)
    }

    /// Sets the length of the string that our underlying buffer contains
    ///
    /// # Safety
    /// * `len` bytes in the buffer must be valid UTF-8
    /// * If the underlying buffer is stored inline, `len` must be <= MAX_SIZE
    pub unsafe fn set_len(&mut self, len: usize) {
        let last_byte = self.last_byte();

        if last_byte == HEAP_MASK {
            // SAFETY: We just checked the discriminant to make sure we're heap allocated
            let heap_buffer = self.as_mut_heap();
            // SAFETY: The caller guarantees that `len` bytes is valid UTF-8
            heap_buffer.set_len(len);
        } else {
            // SAFETY: We just checked the discriminant to make sure we're an InlineBuffer
            let inline_buffer = self.as_mut_inline();
            // SAFETY: The caller guarantees that len <= MAX_SIZE, and `len` bytes is valid UTF-8
            inline_buffer.set_len(len);
        }
    }

    /// Returns the last byte that's on the stack.
    ///
    /// The last byte stores the discriminant that indicates whether the string is on the stack or
    /// on the heap. When the string is on the stack the last byte also stores the length
    #[inline(always)]
    const fn last_byte(&self) -> u8 {
        cfg_if::cfg_if! {
            if #[cfg(target_pointer_width = "64")] {
                let last_byte = self.5;
            } else if #[cfg(target_pointer_width = "32")] {
                let last_byte = self.4;
            } else {
                compile_error!("Unsupported target_pointer_width");
            }
        };
        last_byte as u8
    }

    /// Reinterprets an [`InlineBuffer`] into a [`Repr`]
    ///
    /// Note: This is safe because [`InlineBuffer`] and [`Repr`] are the same size. We used to
    /// define [`Repr`] as a `union` which implicitly transmuted between the two types, but that
    /// prevented us from defining a "niche" value to make `Option<CompactString>` the same size as
    /// just `CompactString`
    #[inline(always)]
    const fn from_inline(inline: InlineBuffer) -> Self {
        // SAFETY: An `InlineBuffer` and `Repr` have the same size
        unsafe { core::mem::transmute(inline) }
    }

    /// Reinterprets a [`HeapBuffer`] into a [`Repr`]
    ///
    /// Note: This is safe because [`HeapBuffer`] and [`Repr`] are the same size. We used to define
    /// [`Repr`] as a `union` which implicitly transmuted between the two types, but that prevented
    /// us from defining a "niche" value to make `Option<CompactString>` the same size as just
    /// `CompactString`
    #[inline(always)]
    const fn from_heap(heap: HeapBuffer) -> Self {
        // SAFETY: A `HeapBuffer` and `Repr` have the same size
        unsafe { core::mem::transmute(heap) }
    }

    /// Reinterprets a [`Repr`] as a [`HeapBuffer`]
    ///
    /// # SAFETY
    /// * The caller must guarantee that the provided [`Repr`] is actually a [`HeapBuffer`] by
    /// checking the discriminant
    ///
    /// Note: We used to define [`Repr`] as a `union` which implicitly transmuted between the two
    /// types, but that prevented us from defining a "niche" value to make `Option<CompactString>`
    /// the same size as just `CompactString`
    #[inline(always)]
    const unsafe fn into_heap(self) -> HeapBuffer {
        core::mem::transmute(self)
    }

    /// Reinterprets a `&mut Repr` as a `&mut HeapBuffer`
    ///
    /// # SAFETY
    /// * The caller must guarantee that the provided [`Repr`] is actually a [`HeapBuffer`] by
    /// checking the discriminant
    ///
    /// Note: We used to define [`Repr`] as a `union` which implicitly transmuted between the two
    /// types, but that prevented us from defining a "niche" value to make `Option<CompactString>`
    /// the same size as just `CompactString`
    #[inline(always)]
    unsafe fn as_mut_heap(&mut self) -> &mut HeapBuffer {
        // SAFETY: A `HeapBuffer` and `Repr` have the same size
        &mut *(self as *mut _ as *mut HeapBuffer)
    }

    /// Reinterprets a `&Repr` as a `&HeapBuffer`
    ///
    /// # SAFETY
    /// * The caller must guarantee that the provided [`Repr`] is actually a [`HeapBuffer`] by
    /// checking the discriminant
    ///
    /// Note: We used to define [`Repr`] as a `union` which implicitly transmuted between the two
    /// types, but that prevented us from defining a "niche" value to make `Option<CompactString>`
    /// the same size as just `CompactString`
    #[inline(always)]
    unsafe fn as_heap(&self) -> &HeapBuffer {
        // SAFETY: A `HeapBuffer` and `Repr` have the same size
        &*(self as *const _ as *const HeapBuffer)
    }

    /// Reinterprets a [`Repr`] as an [`InlineBuffer`]
    ///
    /// # SAFETY
    /// * The caller must guarantee that the provided [`Repr`] is actually an [`InlineBuffer`] by
    /// checking the discriminant
    ///
    /// Note: We used to define [`Repr`] as a `union` which implicitly transmuted between the two
    /// types, but that prevented us from defining a "niche" value to make `Option<CompactString>`
    /// the same size as just `CompactString`
    #[inline(always)]
    #[cfg(feature = "smallvec")]
    const unsafe fn into_inline(self) -> InlineBuffer {
        core::mem::transmute(self)
    }

    /// Reinterprets a `&mut Repr` as an `&mut InlineBuffer`
    ///
    /// # SAFETY
    /// * The caller must guarantee that the provided [`Repr`] is actually an [`InlineBuffer`] by
    /// checking the discriminant
    ///
    /// Note: We used to define [`Repr`] as a `union` which implicitly transmuted between the two
    /// types, but that prevented us from defining a "niche" value to make `Option<CompactString>`
    /// the same size as just `CompactString`
    #[inline(always)]
    unsafe fn as_mut_inline(&mut self) -> &mut InlineBuffer {
        // SAFETY: An `InlineBuffer` and `Repr` have the same size
        &mut *(self as *mut _ as *mut InlineBuffer)
    }

    /// Reinterprets a `&Repr` as an `&InlineBuffer`
    ///
    /// # SAFETY
    /// * The caller must guarantee that the provided [`Repr`] is actually an [`InlineBuffer`] by
    /// checking the discriminant
    ///
    /// Note: We used to define [`Repr`] as a `union` which implicitly transmuted between the two
    /// types, but that prevented us from defining a "niche" value to make `Option<CompactString>`
    /// the same size as just `CompactString`
    #[inline(always)]
    unsafe fn as_inline(&self) -> &InlineBuffer {
        // SAFETY: An `InlineBuffer` and `Repr` have the same size
        &*(self as *const _ as *const InlineBuffer)
    }
}

impl Clone for Repr {
    #[inline]
    fn clone(&self) -> Self {
        let last_byte = self.last_byte();

        #[cold]
        fn clone_heap(this: &Repr) -> Repr {
            // SAFETY: We just checked the discriminant to make sure we're heap allocated
            let heap = unsafe { this.as_heap() };

            // If the contained string is small enough, we will inline it instead of allocating
            if heap.len <= MAX_SIZE {
                // SAFETY: Checked to make sure the length is <= MAX_SIZE
                let inline = unsafe { InlineBuffer::new(this.as_str()) };
                Repr::from_inline(inline)
            } else {
                let new = heap.clone();
                Repr::from_heap(new)
            }
        }

        if last_byte == HEAP_MASK {
            clone_heap(self)
        } else {
            // SAFETY: We checked above that the discriminant indicates we're inline
            let inline = unsafe { self.as_inline() };
            Repr::from_inline(inline.copy())
        }
    }
}

impl Drop for Repr {
    #[inline]
    fn drop(&mut self) {
        // By "outlining" the actual Drop code and only calling it if we're a heap variant, it
        // allows dropping an inline variant to be as cheap as possible.
        if self.is_heap_allocated() {
            outlined_drop(self)
        }

        #[cold]
        fn outlined_drop(this: &mut Repr) {
            // SAFETY: We just checked the discriminant to make sure we're heap allocated
            let heap_buffer = unsafe { this.as_mut_heap() };
            heap_buffer.dealloc();
        }
    }
}

impl Extend<char> for Repr {
    #[inline]
    fn extend<T: IntoIterator<Item = char>>(&mut self, iter: T) {
        let mut iterator = iter.into_iter().peekable();

        // if the iterator is empty, no work needs to be done!
        if iterator.peek().is_none() {
            return;
        }
        let (lower_bound, _) = iterator.size_hint();

        self.reserve(lower_bound);
        iterator.for_each(|c| self.push_str(c.encode_utf8(&mut [0; 4])));
    }
}

impl<'a> Extend<&'a char> for Repr {
    fn extend<T: IntoIterator<Item = &'a char>>(&mut self, iter: T) {
        self.extend(iter.into_iter().copied());
    }
}

impl<'a> Extend<&'a str> for Repr {
    fn extend<T: IntoIterator<Item = &'a str>>(&mut self, iter: T) {
        iter.into_iter().for_each(|s| self.push_str(s));
    }
}

impl Extend<Box<str>> for Repr {
    fn extend<T: IntoIterator<Item = Box<str>>>(&mut self, iter: T) {
        iter.into_iter().for_each(move |s| self.push_str(&s));
    }
}

impl<'a> Extend<Cow<'a, str>> for Repr {
    fn extend<T: IntoIterator<Item = Cow<'a, str>>>(&mut self, iter: T) {
        iter.into_iter().for_each(move |s| self.push_str(&s));
    }
}

impl Extend<String> for Repr {
    fn extend<T: IntoIterator<Item = String>>(&mut self, iter: T) {
        iter.into_iter().for_each(move |s| self.push_str(&s));
    }
}

#[cfg(test)]
mod tests {
    use quickcheck_macros::quickcheck;
    use test_case::test_case;

    use super::{
        Repr,
        MAX_SIZE,
    };

    const EIGHTEEN_MB: usize = 18 * 1024 * 1024;
    const EIGHTEEN_MB_STR: &'static str =
        unsafe { core::str::from_utf8_unchecked(&[42; EIGHTEEN_MB]) };

    #[test_case("hello world!"; "inline")]
    #[test_case("this is a long string that should be stored on the heap"; "heap")]
    fn test_create(s: &'static str) {
        let repr = Repr::new(s);
        assert_eq!(repr.as_str(), s);
        assert_eq!(repr.len(), s.len());
    }

    #[quickcheck]
    #[cfg_attr(miri, ignore)]
    fn quickcheck_create(s: String) {
        let repr = Repr::new(&s);
        assert_eq!(repr.as_str(), s);
        assert_eq!(repr.len(), s.len());
    }

    #[test_case(0; "empty")]
    #[test_case(10; "short")]
    #[test_case(64; "long")]
    #[test_case(EIGHTEEN_MB; "huge")]
    fn test_with_capacity(cap: usize) {
        let r = Repr::with_capacity(cap);
        assert!(r.capacity() >= MAX_SIZE);
        assert_eq!(r.len(), 0);
    }

    #[test_case(""; "empty")]
    #[test_case("abc"; "short")]
    #[test_case("hello world! I am a longer string 🦀"; "long")]
    fn test_from_utf8_valid(s: &'static str) {
        let bytes = s.as_bytes();
        let r = Repr::from_utf8(bytes).expect("valid UTF-8");

        assert_eq!(r.as_str(), s);
        assert_eq!(r.len(), s.len());
    }

    #[quickcheck]
    #[cfg_attr(miri, ignore)]
    fn quickcheck_from_utf8(buf: Vec<u8>) {
        match (core::str::from_utf8(&buf), Repr::from_utf8(&buf)) {
            (Ok(s), Ok(r)) => {
                assert_eq!(r.as_str(), s);
                assert_eq!(r.len(), s.len());
            }
            (Err(e), Err(r)) => assert_eq!(e, r),
            _ => panic!("core::str and Repr differ on what is valid UTF-8!"),
        }
    }

    #[test_case(String::new(), true; "empty should inline")]
    #[test_case(String::new(), false; "empty not inline")]
    #[test_case(String::with_capacity(10), true ; "empty with small capacity inline")]
    #[test_case(String::with_capacity(10), false ; "empty with small capacity not inline")]
    #[test_case(String::with_capacity(128), true ; "empty with large capacity inline")]
    #[test_case(String::with_capacity(128), false ; "empty with large capacity not inline")]
    #[test_case(String::from("nyc 🗽"), true; "short should inline")]
    #[test_case(String::from("nyc 🗽"), false ; "short not inline")]
    #[test_case(String::from("this is a really long string, which is intended"), true; "long")]
    #[test_case(String::from("this is a really long string, which is intended"), false; "long not inline")]
    #[test_case(EIGHTEEN_MB_STR.to_string(), true ; "huge should inline")]
    #[test_case(EIGHTEEN_MB_STR.to_string(), false ; "huge not inline")]
    fn test_from_string(s: String, try_to_inline: bool) {
        // note: when cloning a String it truncates capacity, which is why we measure these values
        // before cloning the string
        let s_len = s.len();
        let s_cap = s.capacity();
        let s_str = s.clone();

        let r = Repr::from_string(s, try_to_inline);

        assert_eq!(r.len(), s_len);
        assert_eq!(r.as_str(), s_str.as_str());

        if s_cap == 0 {
            // we should always inline the string, if the length of the source string is 0
            assert!(!r.is_heap_allocated());
        } else if try_to_inline && s_len <= MAX_SIZE {
            // we should inline the string, if we were asked to, and the length of the string would
            // fit inline, meaning we would truncate capacity
            assert!(!r.is_heap_allocated());
        } else {
            assert!(r.is_heap_allocated());
        }
    }

    #[quickcheck]
    #[cfg_attr(miri, ignore)]
    fn quickcheck_from_string(s: String, try_to_inline: bool) {
        let r = Repr::from_string(s.clone(), try_to_inline);

        assert_eq!(r.len(), s.len());
        assert_eq!(r.as_str(), s.as_str());

        if s.capacity() == 0 {
            // we should always inline the string, if the length of the source string is 0
            assert!(!r.is_heap_allocated());
        } else if s.capacity() <= MAX_SIZE {
            // we should inline the string, if we were asked to
            assert_eq!(!r.is_heap_allocated(), try_to_inline);
        } else {
            assert!(r.is_heap_allocated());
        }
    }

    #[test_case(""; "empty")]
    #[test_case("nyc 🗽"; "short")]
    #[test_case("this is a really long string, which is intended"; "long")]
    fn test_into_string(control: &'static str) {
        let r = Repr::new(control);
        let s = r.into_string();

        assert_eq!(control.len(), s.len());
        assert_eq!(control, s.as_str());
    }

    #[quickcheck]
    #[cfg_attr(miri, ignore)]
    fn quickcheck_into_string(control: String) {
        let r = Repr::new(&control);
        let s = r.into_string();

        assert_eq!(control.len(), s.len());
        assert_eq!(control, s.as_str());
    }

    #[test_case("", "a", false; "empty")]
    #[test_case("", "🗽", false; "empty_emoji")]
    #[test_case("abc", "🗽🙂🦀🌈👏🐶", true; "inline_to_heap")]
    #[test_case("i am a long string that will be on the heap", "extra", true; "heap_to_heap")]
    fn test_push_str(control: &'static str, append: &'static str, is_heap: bool) {
        let mut r = Repr::new(control);
        let mut c = String::from(control);

        r.push_str(append);
        c.push_str(append);

        assert_eq!(r.as_str(), c.as_str());
        assert_eq!(r.len(), c.len());

        assert_eq!(r.is_heap_allocated(), is_heap);
    }

    #[quickcheck]
    #[cfg_attr(miri, ignore)]
    fn quickcheck_push_str(control: String, append: String) {
        let mut r = Repr::new(&control);
        let mut c = control;

        r.push_str(&append);
        c.push_str(&append);

        assert_eq!(r.as_str(), c.as_str());
        assert_eq!(r.len(), c.len());
    }

    #[test_case(&[42; 0], &[42; EIGHTEEN_MB]; "empty_to_heap_capacity")]
    #[test_case(&[42; 8], &[42; EIGHTEEN_MB]; "inline_to_heap_capacity")]
    #[test_case(&[42; 128], &[42; EIGHTEEN_MB]; "heap_inline_to_heap_capacity")]
    #[test_case(&[42; EIGHTEEN_MB], &[42; 64]; "heap_capacity_to_heap_capacity")]
    fn test_push_str_from_buf(buf: &[u8], append: &[u8]) {
        // The goal of this test is to exercise the scenario when our capacity is stored on the heap

        let control = unsafe { core::str::from_utf8_unchecked(buf) };
        let append = unsafe { core::str::from_utf8_unchecked(append) };

        let mut r = Repr::new(control);
        let mut c = String::from(control);

        r.push_str(append);
        c.push_str(append);

        assert_eq!(r.as_str(), c.as_str());
        assert_eq!(r.len(), c.len());

        assert!(r.is_heap_allocated());
    }

    #[test_case("", 0, false; "empty_zero")]
    #[test_case("", 10, false; "empty_small")]
    #[test_case("", 64, true; "empty_large")]
    #[test_case("abc", 0, false; "short_zero")]
    #[test_case("abc", 8, false; "short_small")]
    #[test_case("abc", 64, true; "short_large")]
    #[test_case("I am a long string that will be on the heap", 0, true; "large_zero")]
    #[test_case("I am a long string that will be on the heap", 10, true; "large_small")]
    #[test_case("I am a long string that will be on the heap", EIGHTEEN_MB, true; "large_huge")]
    fn test_reserve(initial: &'static str, additional: usize, is_heap: bool) {
        let mut r = Repr::new(initial);
        r.reserve(additional);

        assert!(r.capacity() >= initial.len() + additional);
        assert_eq!(r.is_heap_allocated(), is_heap);
    }

    #[test]
    #[should_panic(expected = "Attempted to reserve more than 'usize' bytes")]
    fn test_reserve_overflow() {
        let mut r = Repr::new("abc");
        r.reserve(usize::MAX);
    }

    #[test_case(""; "empty")]
    #[test_case("abc"; "short")]
    #[test_case("i am a longer string that will be on the heap"; "long")]
    #[test_case(EIGHTEEN_MB_STR; "huge")]
    fn test_clone(initial: &'static str) {
        let r_a = Repr::new(initial);
        let r_b = r_a.clone();

        assert_eq!(r_a.as_str(), initial);
        assert_eq!(r_a.len(), initial.len());

        assert_eq!(r_a.as_str(), r_b.as_str());
        assert_eq!(r_a.len(), r_b.len());
        assert_eq!(r_a.capacity(), r_b.capacity());
        assert_eq!(r_a.is_heap_allocated(), r_b.is_heap_allocated());
    }

    #[quickcheck]
    #[cfg_attr(miri, ignore)]
    fn quickcheck_clone(initial: String) {
        let r_a = Repr::new(&initial);
        let r_b = r_a.clone();

        assert_eq!(r_a.as_str(), initial);
        assert_eq!(r_a.len(), initial.len());

        assert_eq!(r_a.as_str(), r_b.as_str());
        assert_eq!(r_a.len(), r_b.len());
        assert_eq!(r_a.capacity(), r_b.capacity());
        assert_eq!(r_a.is_heap_allocated(), r_b.is_heap_allocated());
    }
}
