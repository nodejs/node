use crate::to_chars;
use crate::{Buffer, Float};
use core::mem::MaybeUninit;
use core::slice;
use core::str;

impl Buffer {
    /// This is a cheap operation; you don't need to worry about reusing buffers
    /// for efficiency.
    #[inline]
    pub fn new() -> Self {
        let bytes = [MaybeUninit::<u8>::uninit(); to_chars::MAX_OUTPUT_STRING_LENGTH];
        Buffer { bytes }
    }

    /// Print a floating point number into this buffer and return a reference to
    /// its string representation within the buffer.
    ///
    /// # Special cases
    ///
    /// This function formats NaN as the string "NaN", positive infinity as
    /// "inf", and negative infinity as "-inf" to match std::fmt.
    ///
    /// If your input is known to be finite, you may get better performance by
    /// calling the `format_finite` method instead of `format` to avoid the
    /// checks for special cases.
    pub fn format<F: Float>(&mut self, f: F) -> &str {
        if f.is_nonfinite() {
            f.format_nonfinite()
        } else {
            self.format_finite(f)
        }
    }

    /// Print a floating point number into this buffer and return a reference to
    /// its string representation within the buffer.
    ///
    /// # Special cases
    ///
    /// This function **does not** check for NaN or infinity. If the input
    /// number is not a finite float, the printed representation will be some
    /// correctly formatted but unspecified numerical value.
    ///
    /// Please check [`is_finite`] yourself before calling this function, or
    /// check [`is_nan`] and [`is_infinite`] and handle those cases yourself.
    ///
    /// [`is_finite`]: f64::is_finite
    /// [`is_nan`]: f64::is_nan
    /// [`is_infinite`]: f64::is_infinite
    #[inline]
    pub fn format_finite<F: Float>(&mut self, f: F) -> &str {
        unsafe {
            let n = f.write_to_dragonbox_buffer(self.bytes.as_mut_ptr().cast::<u8>());
            debug_assert!(n <= self.bytes.len());
            let slice = slice::from_raw_parts(self.bytes.as_ptr().cast::<u8>(), n);
            str::from_utf8_unchecked(slice)
        }
    }
}

impl Copy for Buffer {}

#[allow(clippy::non_canonical_clone_impl)]
impl Clone for Buffer {
    #[inline]
    fn clone(&self) -> Self {
        Buffer::new()
    }
}

impl Default for Buffer {
    #[inline]
    fn default() -> Self {
        Buffer::new()
    }
}

impl Float for f64 {}

const NAN: &str = "NaN";
#[cfg(not(feature = "ecma"))]
const INFINITY: &str = "inf";
#[cfg(feature = "ecma")]
const INFINITY: &str = "Infinity";
#[cfg(not(feature = "ecma"))]
const NEG_INFINITY: &str = "-inf";
#[cfg(feature = "ecma")]
const NEG_INFINITY: &str = "-Infinity";

pub trait Sealed: Copy {
    fn is_nonfinite(self) -> bool;
    fn format_nonfinite(self) -> &'static str;
    unsafe fn write_to_dragonbox_buffer(self, result: *mut u8) -> usize;
}

impl Sealed for f64 {
    #[inline]
    fn is_nonfinite(self) -> bool {
        const EXP_MASK: u64 = 0x7ff0000000000000;
        let bits = self.to_bits();
        bits & EXP_MASK == EXP_MASK
    }

    #[cold]
    fn format_nonfinite(self) -> &'static str {
        const MANTISSA_MASK: u64 = 0x000fffffffffffff;
        const SIGN_MASK: u64 = 0x8000000000000000;
        let bits = self.to_bits();
        if bits & MANTISSA_MASK != 0 {
            NAN
        } else if bits & SIGN_MASK != 0 {
            NEG_INFINITY
        } else {
            INFINITY
        }
    }

    #[inline]
    unsafe fn write_to_dragonbox_buffer(self, buffer: *mut u8) -> usize {
        let end = crate::to_chars::to_chars(self, buffer);
        end.offset_from(buffer) as usize
    }
}
