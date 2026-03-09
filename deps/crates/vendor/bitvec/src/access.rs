#![doc = include_str!("../doc/access.md")]

use core::sync::atomic::Ordering;

use funty::Integral;
use radium::Radium;

use crate::{
	index::{
		BitIdx,
		BitMask,
	},
	mem::BitRegister,
	order::BitOrder,
};

#[doc = include_str!("../doc/access/BitAccess.md")]
pub trait BitAccess: Radium
where <Self as Radium>::Item: BitRegister
{
	/// Clears bits within a memory element to `0`.
	///
	/// The mask provided to this method must be constructed from indices that
	/// are valid in the caller’s context. As the mask is already computed by
	/// the caller, this does not take an ordering type parameter.
	///
	/// ## Parameters
	///
	/// - `mask`: A mask of any number of bits. This is a selection mask: all
	///   bits in the mask that are set to `1` will set the corresponding bit in
	///   `*self` to `0`.
	///
	/// ## Returns
	///
	/// The prior value of the memory element.
	///
	/// ## Effects
	///
	/// All bits in `*self` corresponding to `1` bits in the `mask` are cleared
	/// to `0`; all others retain their original value.
	///
	/// Do not invert the `mask` prior to calling this function. [`BitMask`] is
	/// a selection type, not a bitwise-operation argument.
	///
	/// [`BitMask`]: crate::index::BitMask
	#[inline]
	fn clear_bits(&self, mask: BitMask<Self::Item>) -> Self::Item {
		self.fetch_and(!mask.into_inner(), Ordering::Relaxed)
	}

	/// Sets bits within a memory element to `1`.
	///
	/// The mask provided to this method must be constructed from indices that
	/// are valid in the caller’s context. As the mask is already computed by
	/// the caller, this does not take an ordering type parameter.
	///
	/// ## Parameters
	///
	/// - `mask`: A mask of any number of bits. This is a selection mask: all
	///   bits in the mask that are set to `1` will set the corresponding bit in
	///   `*self` to `1`.
	///
	/// ## Returns
	///
	/// The prior value of the memory element.
	///
	/// ## Effects
	///
	/// All bits in `*self` corresponding to `1` bits in the `mask` are set to
	/// `1`; all others retain their original value.
	#[inline]
	fn set_bits(&self, mask: BitMask<Self::Item>) -> Self::Item {
		self.fetch_or(mask.into_inner(), Ordering::Relaxed)
	}

	/// Inverts bits within a memory element.
	///
	/// The mask provided to this method must be constructed from indices that
	/// are valid in the caller’s context. As the mask is already computed by
	/// the caller, this does not take an ordering type parameter.
	///
	/// ## Parameters
	///
	/// - `mask`: A mask of any number of bits. This is a selection mask: all
	///   bits in the mask that are set to `1` will invert the corresponding bit
	///   in `*self`.
	///
	/// ## Returns
	///
	/// The prior value of the memory element.
	///
	/// ## Effects
	///
	/// All bits in `*self` corresponding to `1` bits in the `mask` are
	/// inverted; all others retain their original value.
	#[inline]
	fn invert_bits(&self, mask: BitMask<Self::Item>) -> Self::Item {
		self.fetch_xor(mask.into_inner(), Ordering::Relaxed)
	}

	/// Writes a value to one bit in a memory element, returning the previous
	/// value.
	///
	/// ## Type Parameters
	///
	/// - `O`: An ordering of bits in a memory element that translates the
	///   `index` into a real position.
	///
	/// ## Parameters
	///
	/// - `index`: The semantic index of the bit in `*self` to modify.
	/// - `value`: The new bit value to write into `*self` at the `index`.
	///
	/// ## Returns
	///
	/// The bit previously stored in `*self` at `index`. These operations are
	/// required to load the `*self` value from memory in order to operate, and
	/// so always have the prior value available for use. This can reduce
	/// spurious loads throughout the crate.
	///
	/// ## Effects
	///
	/// `*self` is updated with the bit at `index` set to `value`; all other
	/// bits remain unchanged.
	#[inline]
	fn write_bit<O>(&self, index: BitIdx<Self::Item>, value: bool) -> bool
	where O: BitOrder {
		let select = index.select::<O>().into_inner();
		select
			& if value {
				self.fetch_or(select, Ordering::Relaxed)
			}
			else {
				self.fetch_and(!select, Ordering::Relaxed)
			} != <Self::Item>::ZERO
	}

	/// Gets the function that will write `value` into all bits under a mask.
	///
	/// This is useful for preparing bulk operations that all write the same
	/// data into memory, and only need to provide the shape of memory to write.
	///
	/// ## Parameters
	///
	/// - `value`: The bit that will be written by the returned function.
	///
	/// ## Returns
	///
	/// A function which writes `value` into memory at a given address and under
	/// a given mask. If `value` is `false`, then this produces [`clear_bits`];
	/// if it is `true`, then this produces [`set_bits`].
	///
	/// [`clear_bits`]: Self::clear_bits
	/// [`set_bits`]: Self::set_bits
	#[inline]
	fn get_writers(
		value: bool,
	) -> for<'a> fn(&'a Self, BitMask<Self::Item>) -> Self::Item {
		if value {
			Self::set_bits
		}
		else {
			Self::clear_bits
		}
	}
}

impl<A> BitAccess for A
where
	A: Radium,
	A::Item: BitRegister,
{
}

#[doc = include_str!("../doc/access/BitSafe.md")]
pub trait BitSafe {
	/// The element type being guarded against improper mutation.
	///
	/// This is only present as an extra proof that the type graph has a
	/// consistent view of the underlying memory.
	type Mem: BitRegister;

	/// The memory-access type this guards.
	///
	/// This is exposed as an associated type so that `BitStore` can name it
	/// without having to re-select it based on crate configuration.
	type Rad: Radium<Item = Self::Mem>;

	/// The zero constant.
	const ZERO: Self;

	/// Loads the value from memory, allowing for the possibility that other
	/// handles have write permissions to it.
	fn load(&self) -> Self::Mem;
}

/// Constructs a shared-mutable guard type that disallows mutation *through it*.
macro_rules! safe {
	($($t:ident => $w:ident => $r:ty);+ $(;)?) => { $(
		#[derive(Debug)]
		#[repr(transparent)]
		#[doc = include_str!("../doc/access/impl_BitSafe.md")]
		pub struct $w {
			inner: <Self as BitSafe>::Rad,
		}

		impl $w {
			/// Allow construction of the safed value by forwarding to its
			/// interior constructor.
			///
			/// This type is not public API, and general use has no reason to
			/// construct values of it directly. It is provided for convenience
			/// as a crate internal.
			pub(crate) const fn new(value: $t) -> Self {
				Self { inner: <<Self as BitSafe>::Rad>::new(value) }
			}
		}

		impl BitSafe for $w {
			type Mem = $t;

			#[cfg(feature = "atomic")]
			type Rad = $r;

			#[cfg(not(feature = "atomic"))]
			type Rad = core::cell::Cell<$t>;

			const ZERO: Self = Self::new(0);

			#[inline]
			fn load(&self) -> Self::Mem {
				self.inner.load(Ordering::Relaxed)
			}
		}
	)+ };
}

safe! {
	u8 => BitSafeU8 => radium::types::RadiumU8;
	u16 => BitSafeU16 => radium::types::RadiumU16;
	u32 => BitSafeU32 => radium::types::RadiumU32;
}

#[cfg(target_pointer_width = "64")]
safe!(u64 => BitSafeU64 => radium::types::RadiumU64);

safe!(usize => BitSafeUsize => radium::types::RadiumUsize);

#[cfg(test)]
mod tests {
	use core::cell::Cell;

	use super::*;
	use crate::prelude::*;

	#[test]
	fn touch_memory() {
		let data = Cell::new(0u8);
		let accessor = &data;
		let aliased = unsafe { &*(&data as *const _ as *const BitSafeU8) };

		assert!(!BitAccess::write_bit::<Lsb0>(
			accessor,
			BitIdx::new(1).unwrap(),
			true
		));
		assert_eq!(aliased.load(), 2);
		assert!(BitAccess::write_bit::<Lsb0>(
			accessor,
			BitIdx::new(1).unwrap(),
			false
		));
		assert_eq!(aliased.load(), 0);
	}

	#[test]
	#[cfg(not(miri))]
	fn sanity_check_prefetch() {
		use core::cell::Cell;
		assert_eq!(
			<Cell<u8> as BitAccess>::get_writers(false) as *const (),
			<Cell<u8> as BitAccess>::clear_bits as *const ()
		);

		assert_eq!(
			<Cell<u8> as BitAccess>::get_writers(true) as *const (),
			<Cell<u8> as BitAccess>::set_bits as *const ()
		);
	}
}
