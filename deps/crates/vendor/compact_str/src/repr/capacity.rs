use crate::repr::HEAP_MASK;

// how many bytes a `usize` occupies
const USIZE_SIZE: usize = core::mem::size_of::<usize>();

/// Used to generate [`CAPACITY_IS_ON_THE_HEAP`]
#[allow(non_snake_case)]
const fn CAP_ON_HEAP_FLAG() -> [u8; USIZE_SIZE] {
    // all bytes 255, with the last being HEAP_MASK
    let mut flag = [255; USIZE_SIZE];
    flag[USIZE_SIZE - 1] = HEAP_MASK;
    flag
}

/// State that describes the capacity as being stored on the heap.
///
/// All bytes `255`, with the last being [`HEAP_MASK`], using the same amount of bytes as `usize`
/// Example (64-bit): `[255, 255, 255, 255, 255, 255, 255, 254]`
const CAPACITY_IS_ON_THE_HEAP: [u8; USIZE_SIZE] = CAP_ON_HEAP_FLAG();

// how many bytes we can use for capacity
const SPACE_FOR_CAPACITY: usize = USIZE_SIZE - 1;
// the maximum value we're able to store, e.g. on 64-bit arch this is 2^56 - 2
//
// note: Preferably we'd used usize.pow(..) here, but that's not a `const fn`, so we need to use
// bitshift operators, and there's a lint against using them in this pattern, which IMO isn't a
// great lint
pub const MAX_VALUE: usize = 2usize.pow(SPACE_FOR_CAPACITY as u32 * 8) - 2;

/// An integer type that uses `core::mem::size_of::<usize>() - 1` bytes to store the capacity of
/// a heap buffer.
///
/// Assumming a 64-bit arch, a [`super::BoxString`] uses 8 bytes for a pointer, 8 bytes for a
/// length, and then needs 1 byte for a discriminant. We need to store the capacity somewhere, and
/// we could store it on the heap, but we also have 7 unused bytes. [`Capacity`] handles storing a
/// value in these 7 bytes, returning an error if it's not possible, at which point we'll store the
/// capacity on the heap.
///
/// # Max Values
/// * __64-bit:__ `(2 ^ (7 * 8)) - 2 = 72_057_594_037_927_934 ~= 64 petabytes`
/// * __32-bit:__ `(2 ^ (3 * 8)) - 2 = 16_777_214             ~= 16 megabytes`
///
/// Practically speaking, on a 64-bit architecture we'll never need to store the capacity on the
/// heap, because with it's impossible to create a string that is 64 petabytes or larger. But for
/// 32-bit architectures we need to be able to store a capacity larger than 16 megabytes, since a
/// string larger than 16 megabytes probably isn't that uncommon.
#[derive(Copy, Clone, Debug, PartialEq, Eq)]
#[cfg_attr(target_pointer_width = "64", repr(align(8)))]
#[cfg_attr(target_pointer_width = "32", repr(align(4)))]
pub struct Capacity([u8; USIZE_SIZE]);

static_assertions::assert_eq_size!(Capacity, usize);
static_assertions::assert_eq_align!(Capacity, usize);

impl Capacity {
    #[inline]
    pub const fn new(capacity: usize) -> Self {
        cfg_if::cfg_if! {
            if #[cfg(target_pointer_width = "64")] {
                // on 64-bit arches we can always fit the capacity inline
                debug_assert!(capacity <= MAX_VALUE);

                let mut bytes = capacity.to_le_bytes();
                bytes[core::mem::size_of::<usize>() - 1] = HEAP_MASK;
                Capacity(bytes)
            } else if #[cfg(target_pointer_width = "32")] {
                // on 32-bit arches we might need to store the capacity on the heap
                if capacity > MAX_VALUE {
                    // if we need the last byte to encode this capacity then we need to put the capacity on
                    // the heap. return an Error so `BoxString` can do the right thing
                    Capacity(CAPACITY_IS_ON_THE_HEAP)
                } else {
                    // otherwise, we can store this capacity inline! Set the last byte to be our `HEAP_MASK`
                    // for our discriminant, using the leading bytes to store the actual value
                    let mut bytes = capacity.to_le_bytes();
                    bytes[core::mem::size_of::<usize>() - 1] = HEAP_MASK;
                    Capacity(bytes)
                }
            } else {
                compile_error!("Unsupported target_pointer_width");
            }
        }
    }

    /// Re-interprets a [`Capacity`] as a `usize`
    ///
    /// # SAFETY:
    /// * `self` must be less than or equal to [`MAX_VALUE`]
    #[inline(always)]
    pub unsafe fn as_usize(&self) -> usize {
        let mut usize_buf = [0u8; USIZE_SIZE];
        // SAFETY:
        // * `src` is valid for reads of `SPACE_FOR_CAPACITY` because it is less than `USIZE_SIZE`
        // * `dst` is valid for reads of `SPACE_FOR_CAPACITY` because it is less than `USIZE_SIZE`
        // * `src` and `dst` do not overlap because we created `usize_buf`
        core::ptr::copy_nonoverlapping(self.0.as_ptr(), usize_buf.as_mut_ptr(), SPACE_FOR_CAPACITY);
        usize::from_le_bytes(usize_buf)
    }

    /// Returns whether or not this [`Capacity`] has a value that indicates the capacity is being
    /// stored on the heap
    #[inline(always)]
    pub fn is_heap(&self) -> bool {
        self.0 == CAPACITY_IS_ON_THE_HEAP
    }
}

#[cfg(test)]
mod tests {
    use rayon::prelude::*;

    use super::Capacity;

    #[test]
    fn test_zero_roundtrips() {
        let og = 0;
        let cap = Capacity::new(og);
        let after = unsafe { cap.as_usize() };

        assert_eq!(og, after);
    }

    #[test]
    fn test_max_value() {
        let available_bytes = (core::mem::size_of::<usize>() - 1) as u32;
        let max_value = 2usize.pow(available_bytes * 8) - 2;

        #[cfg(target_pointer_width = "64")]
        assert_eq!(max_value, 72057594037927934);
        #[cfg(target_pointer_width = "32")]
        assert_eq!(max_value, 16777214);

        let cap = Capacity::new(max_value);
        let after = unsafe { cap.as_usize() };

        assert_eq!(max_value, after);
    }

    #[cfg(target_pointer_width = "32")]
    #[test]

    fn test_invalid_value() {
        let invalid_val = usize::MAX;
        let cap = Capacity::new(invalid_val);
        let after = unsafe { cap.as_usize() };

        // anything greater than or equal to 16777215, should "resolve" to 16777215
        assert_eq!(16777215, after);
    }

    #[test]
    #[cfg_attr(miri, ignore)]
    fn test_all_valid_32bit_values() {
        #[cfg(target_pointer_width = "32")]
        assert_eq!(16_777_214, super::MAX_VALUE);

        (0..=16_777_214).into_par_iter().for_each(|i| {
            let cap = Capacity::new(i);
            let val = unsafe { cap.as_usize() };

            assert_eq!(val, i, "value roundtriped to wrong value?");
        });
    }
}
