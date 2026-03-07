#![doc = include_str!("../doc/mem.md")]

use core::{
	cell::Cell,
	mem,
};

use funty::Unsigned;
use radium::marker::BitOps;

#[doc = include_str!("../doc/mem/BitRegister.md")]
pub trait BitRegister: Unsigned + BitOps {
	/// The number of bits required to store an index in the range `0 .. BITS`.
	const INDX: u8 = bits_of::<Self>().trailing_zeros() as u8;
	/// A mask over all bits that can be used as an index within the element.
	/// This is the value with the least significant `INDX`-many bits set high.
	const MASK: u8 = bits_of::<Self>() as u8 - 1;
	/// The literal `!0`.
	const ALL: Self;
}

/// Marks certain fundamentals as processor registers.
macro_rules! register {
	($($t:ty),+ $(,)?) => { $(
		impl BitRegister for $t {
			const ALL: Self = !0;
		}
	)+ };
}

register!(u8, u16, u32);

/** `u64` can only be used as a register on processors whose word size is at
least 64 bits.

This implementation is not present on targets with 32-bit processor words.
**/
#[cfg(target_pointer_width = "64")]
impl BitRegister for u64 {
	const ALL: Self = !0;
}

register!(usize);

/// Counts the number of bits in a value of type `T`.
pub const fn bits_of<T>() -> usize {
	core::mem::size_of::<T>().saturating_mul(<u8>::BITS as usize)
}

#[doc = include_str!("../doc/mem/elts.md")]
pub const fn elts<T>(bits: usize) -> usize {
	let width = bits_of::<T>();
	if width == 0 {
		return 0;
	}
	bits / width + (bits % width != 0) as usize
}

/// Tests if a type has alignment equal to its size.
#[doc(hidden)]
#[cfg(not(tarpaulin_include))]
pub const fn aligned_to_size<T>() -> bool {
	mem::align_of::<T>() == mem::size_of::<T>()
}

/// Tests if two types have identical layouts (size and alignment are equal).
#[doc(hidden)]
#[cfg(not(tarpaulin_include))]
pub const fn layout_eq<T, U>() -> bool {
	mem::align_of::<T>() == mem::align_of::<U>()
		&& mem::size_of::<T>() == mem::size_of::<U>()
}

#[doc(hidden)]
#[repr(transparent)]
#[doc = include_str!("../doc/mem/BitElement.md")]
#[derive(Clone, Copy, Debug, Default, Eq, Hash, Ord, PartialEq, PartialOrd)]
pub struct BitElement<T = usize> {
	pub elem: T,
}

/// Creates a `BitElement` implementation for an integer and its atomic/cell
/// variants.
macro_rules! element {
	($($size:tt, $bare:ty => $atom:ident);+ $(;)?) => { $(
		impl BitElement<$bare> {
			/// Creates a new element wrapper from a raw integer.
			pub const fn new(elem: $bare) -> Self {
				Self {
					elem,
				}
			}
		}

		impl BitElement<Cell<$bare>> {
			/// Creates a new element wrapper from a raw integer.
			pub const fn new(elem: $bare) -> Self {
				Self {
					elem: Cell::new(elem),
				}
			}
		}

		radium::if_atomic!( if atomic($size) {
			use core::sync::atomic::$atom;
			impl BitElement<$atom> {
				/// Creates a new element wrapper from a raw integer.
				pub const fn new(elem: $bare) -> Self {
					Self {
						elem: <$atom>::new(elem),
					}
				}
			}
		});
	)+ };
}

element! {
	8, u8 => AtomicU8;
	16, u16 => AtomicU16;
	32, u32 => AtomicU32;
}

#[cfg(target_pointer_width = "64")]
element!(64, u64 => AtomicU64);

element!(size, usize => AtomicUsize);

#[cfg(test)]
mod tests {
	use super::*;
	use crate::access::*;

	#[test]
	fn integer_properties() {
		assert!(aligned_to_size::<u8>());
		assert!(aligned_to_size::<BitSafeU8>());
		assert!(layout_eq::<u8, BitSafeU8>());

		assert!(aligned_to_size::<u16>());
		assert!(aligned_to_size::<BitSafeU16>());
		assert!(layout_eq::<u16, BitSafeU16>());

		assert!(aligned_to_size::<u32>());
		assert!(aligned_to_size::<BitSafeU32>());
		assert!(layout_eq::<u32, BitSafeU32>());

		assert!(aligned_to_size::<usize>());
		assert!(aligned_to_size::<BitSafeUsize>());
		assert!(layout_eq::<usize, BitSafeUsize>());

		#[cfg(target_pointer_width = "64")]
		{
			assert!(aligned_to_size::<u64>());
			assert!(aligned_to_size::<BitSafeU64>());
			assert!(layout_eq::<u64, BitSafeU64>());
		}
	}
}
