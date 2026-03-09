#![doc = include_str!("../doc/order.md")]

use crate::{
	index::{
		BitEnd,
		BitIdx,
		BitMask,
		BitPos,
		BitSel,
	},
	mem::{
		bits_of,
		BitRegister,
	},
};

#[doc = include_str!("../doc/order/BitOrder.md")]
pub unsafe trait BitOrder: 'static {
	/// Translates a semantic bit index into a real bit position.
	///
	/// This function is the basis of the trait, and must adhere to a number of
	/// requirements in order for an implementation to be correct.
	///
	/// ## Type Parameters
	///
	/// - `R`: The memory element type that the index and position govern.
	///
	/// ## Parameters
	///
	/// - `index`: A semantic bit-index within some `R` element.
	///
	/// ## Returns
	///
	/// The real position of the indexed bit within an `R` element. See the
	/// `BitPos` documentation for what these positions are considered to mean.
	///
	/// ## Requirements
	///
	/// This function must satisfy the following requirements for all possible
	/// input and output values, for all possible `R` type parameters:
	///
	/// - Totality: The implementation must be able to accept every input in
	///   [`BitIdx::<R>::range_all()`], and produce some `BitPos` value for
	///   each.
	/// - Bijection: There must be an exactly one-to-one correspondence between
	///   input and output values. No input index may choose its output from a
	///   set of more than one position, and no output position may be produced
	///   by more than one input index.
	/// - Purity: The translation from index to position must be consistent for
	///   the lifetime of *at least* all data structures in the program. This
	///   function *may* refer to global state, but that state **must** be
	///   immutable while any `bitvec` data structures exist, and must not be
	///   used to violate the totality or bijection requirements.
	/// - Validity: The produced `BitPos` value must be within the valid range
	///   of its type. This is enforced by [`BitPos::new`], but not by the
	///   unsafe constructor [`BitPos::new_unchecked`].
	///
	/// [`BitIdx::<R>::range_all()`]: crate::index::BitIdx::range_all
	/// [`BitPos::new`]: crate::index::BitPos::new
	/// [`BitPos::new_unchecked`]: crate::index::BitPos::new_unchecked
	fn at<R>(index: BitIdx<R>) -> BitPos<R>
	where R: BitRegister;

	/// Produces a single-bit selection mask from a bit-index.
	///
	/// This is an optional function: it is implemented as, and must always be
	/// exactly identical to, `BitOrder::at(index).select()`. If your ordering
	/// has a faster implementation, you may provide it, but it must be exactly
	/// numerically equivalent.
	#[inline]
	fn select<R>(index: BitIdx<R>) -> BitSel<R>
	where R: BitRegister {
		Self::at::<R>(index).select()
	}

	/// Produces a multi-bit selection mask from a range of bit-indices.
	///
	/// This is an optional function: it is implemented as, and must always be
	/// exactly identical to,
	/// `BitIdx::range(from, upto).map(BitOrder::select).sum()`. If your
	/// ordering has a faster implementation, you may provide it, but it must be
	/// exactly numerically equivalent.
	///
	/// ## Parameters
	///
	/// - `from`: The inclusive starting value of the indices being selected.
	///   Defaults to [`BitIdx::MIN`].
	/// - `upto`: The exclusive ending value of the indices being selected.
	///   Defaults to [`BitEnd::MAX`].
	///
	/// ## Returns
	///
	/// A selection mask with all bit-positions corresponding to `from .. upto`
	/// selected.
	///
	/// [`BitEnd::MAX`]: crate::index::BitEnd::MAX
	/// [`BitIdx::MIN`]: crate::index::BitIdx::MIN
	#[inline]
	fn mask<R>(
		from: impl Into<Option<BitIdx<R>>>,
		upto: impl Into<Option<BitEnd<R>>>,
	) -> BitMask<R>
	where
		R: BitRegister,
	{
		let (from, upto) = match (from.into(), upto.into()) {
			(None, None) => return BitMask::ALL,
			(Some(from), None) => (from, BitEnd::MAX),
			(None, Some(upto)) => (BitIdx::MIN, upto),
			(Some(from), Some(upto)) => (from, upto),
		};
		from.range(upto).map(Self::select::<R>).sum()
	}
}

#[doc = include_str!("../doc/order/Lsb0.md")]
#[derive(Clone, Copy, Debug, Default, Eq, Hash, Ord, PartialEq, PartialOrd)]
pub struct Lsb0;

#[doc = include_str!("../doc/order/Msb0.md")]
#[derive(Clone, Copy, Debug, Default, Eq, Hash, Ord, PartialEq, PartialOrd)]
pub struct Msb0;

unsafe impl BitOrder for Lsb0 {
	#[inline]
	fn at<R>(index: BitIdx<R>) -> BitPos<R>
	where R: BitRegister {
		unsafe { BitPos::new_unchecked(index.into_inner()) }
	}

	#[inline]
	fn select<R>(index: BitIdx<R>) -> BitSel<R>
	where R: BitRegister {
		unsafe { BitSel::new_unchecked(R::ONE << index.into_inner()) }
	}

	#[inline]
	fn mask<R>(
		from: impl Into<Option<BitIdx<R>>>,
		upto: impl Into<Option<BitEnd<R>>>,
	) -> BitMask<R>
	where
		R: BitRegister,
	{
		let from = from.into().unwrap_or(BitIdx::MIN).into_inner();
		let upto = upto.into().unwrap_or(BitEnd::MAX).into_inner();
		debug_assert!(
			from <= upto,
			"Ranges must run from low index ({}) to high ({})",
			from,
			upto,
		);
		let ct = upto - from;
		if ct == bits_of::<R>() as u8 {
			return BitMask::ALL;
		}
		/* This expression does the following work:
		 * 1. Set all bits in the mask to `1`.
		 * 2. Shift left by the number of bits in the mask. The mask bits are
		 *    now at LSedge and `0`.
		 * 3. Invert the mask. The mask bits are now at LSedge and `1`; all
		 *    else are `0`.
		 * 4. Shift left by the `from` distance from LSedge. The mask bits now
		 *    begin at `from` left of LSedge and extend to `upto` left of
		 *    LSedge.
		 */
		BitMask::new(!(R::ALL << ct) << from)
	}
}

unsafe impl BitOrder for Msb0 {
	#[inline]
	fn at<R>(index: BitIdx<R>) -> BitPos<R>
	where R: BitRegister {
		unsafe { BitPos::new_unchecked(R::MASK - index.into_inner()) }
	}

	#[inline]
	fn select<R>(index: BitIdx<R>) -> BitSel<R>
	where R: BitRegister {
		/* Shift the MSbit down by the index count. This is not equivalent to
		 * the expression `1 << (mask - index)`, because that is required to
		 * perform a runtime subtraction before the shift, while this produces
		 * a constant that is shifted.
		 */
		let msbit: R = R::ONE << R::MASK;
		unsafe { BitSel::new_unchecked(msbit >> index.into_inner()) }
	}

	#[inline]
	fn mask<R>(
		from: impl Into<Option<BitIdx<R>>>,
		upto: impl Into<Option<BitEnd<R>>>,
	) -> BitMask<R>
	where
		R: BitRegister,
	{
		let from = from.into().unwrap_or(BitIdx::MIN).into_inner();
		let upto = upto.into().unwrap_or(BitEnd::MAX).into_inner();
		debug_assert!(
			from <= upto,
			"ranges must run from low index ({}) to high ({})",
			from,
			upto,
		);
		let ct = upto - from;
		if ct == bits_of::<R>() as u8 {
			return BitMask::ALL;
		}
		/* This expression does the following work:
		 * 1. Set all bits in the mask to `1`.
		 * 2. Shift right by the number of bits in the mask. The mask bits are
		 *    now at MSedge and `0`.
		 * 3. Invert the mask. The mask bits are now at MSedge and `1`; all
		 *    else are `0`.
		 * 4. Shift right by the `from` distance from MSedge. The mask bits
		 *    now begin at `from` right of MSedge and extend to `upto` right
		 *    of MSedge.
		 */
		BitMask::new(!(R::ALL >> ct) >> from)
	}
}

#[cfg(target_endian = "little")]
#[doc = include_str!("../doc/order/LocalBits.md")]
pub use self::Lsb0 as LocalBits;
#[cfg(target_endian = "big")]
#[doc = include_str!("../doc/order/LocalBits.md")]
pub use self::Msb0 as LocalBits;

#[cfg(not(any(target_endian = "big", target_endian = "little")))]
compile_fail!(
	"This architecture is not supported! Please consider filing an issue"
);

#[inline]
#[cfg(not(tarpaulin_include))]
#[doc = include_str!("../doc/order/verify.md")]
pub fn verify<O>(verbose: bool)
where O: BitOrder {
	verify_for_type::<u8, O>(verbose);
	verify_for_type::<u16, O>(verbose);
	verify_for_type::<u32, O>(verbose);
	verify_for_type::<usize, O>(verbose);

	#[cfg(target_pointer_width = "64")]
	verify_for_type::<u64, O>(verbose);
}

/// Verification does not access memory, and is both useless and slow in Miri.
#[cfg(miri)]
pub fn verify_for_type<R, O>(_: bool)
where
	R: BitRegister,
	O: BitOrder,
{
}

#[cfg(not(miri))]
#[doc = include_str!("../doc/order/verify_for_type.md")]
pub fn verify_for_type<R, O>(verbose: bool)
where
	R: BitRegister,
	O: BitOrder,
{
	use core::any::type_name;
	let mut accum = BitMask::<R>::ZERO;

	let ord_name = type_name::<O>();
	let reg_name = type_name::<R>();

	for n in 0 .. bits_of::<R>() as u8 {
		//  Wrap the counter as an index.
		let idx = unsafe { BitIdx::<R>::new_unchecked(n) };

		//  Compute the bit position for the index.
		let pos = O::at::<R>(idx);
		if verbose {
			#[cfg(feature = "std")]
			println!(
				"`<{} as BitOrder>::at::<{}>({})` produces {}",
				ord_name,
				reg_name,
				n,
				pos.into_inner(),
			);
		}

		//  If the computed position exceeds the valid range, fail.
		assert!(
			pos.into_inner() < bits_of::<R>() as u8,
			"Error when verifying the implementation of `BitOrder` for `{}`: \
			 Index {} produces a bit position ({}) that exceeds the type width \
			 {}",
			ord_name,
			n,
			pos.into_inner(),
			bits_of::<R>(),
		);

		//  Check `O`’s implementation of `select`
		let sel = O::select::<R>(idx);
		if verbose {
			#[cfg(feature = "std")]
			println!(
				"`<{} as BitOrder>::select::<{}>({})` produces {:b}",
				ord_name, reg_name, n, sel,
			);
		}

		//  If the selector bit is not one-hot, fail.
		assert_eq!(
			sel.into_inner().count_ones(),
			1,
			"Error when verifying the implementation of `BitOrder` for `{}`: \
			 Index {} produces a bit selector ({:b}) that is not a one-hot mask",
			ord_name,
			n,
			sel,
		);

		//  Check that the selection computed from the index matches the
		//  selection computed from the position.
		let shl = pos.select();
		//  If `O::select(idx)` does not produce `1 << pos`, fail.
		assert_eq!(
			sel,
			shl,
			"Error when verifying the implementation of `BitOrder` for `{}`: \
			 Index {} produces a bit selector ({:b}) that is not equal to `1 \
			 << {}` ({:b})",
			ord_name,
			n,
			sel,
			pos.into_inner(),
			shl,
		);

		//  Check that the produced selector bit has not already been added to
		//  the accumulator.
		assert!(
			!accum.test(sel),
			"Error when verifying the implementation of `BitOrder` for `{}`: \
			 Index {} produces a bit position ({}) that has already been \
			 produced by a prior index",
			ord_name,
			n,
			pos.into_inner(),
		);
		accum.insert(sel);
		if verbose {
			#[cfg(feature = "std")]
			println!(
				"`<{} as BitOrder>::at::<{}>({})` accumulates  {:b}",
				ord_name, reg_name, n, accum,
			);
		}
	}

	//  Check that all indices produced all positions.
	assert_eq!(
		accum,
		BitMask::ALL,
		"Error when verifying the implementation of `BitOrder` for `{}`: The \
		 bit positions marked with a `0` here were never produced from an \
		 index, despite all possible indices being passed in for translation: \
		 {:b}",
		ord_name,
		accum,
	);

	//  Check that `O::mask` is correct for all range combinations.
	for from in BitIdx::<R>::range_all() {
		for upto in BitEnd::<R>::range_from(from) {
			let mask = O::mask(from, upto);
			let check = from
				.range(upto)
				.map(O::at)
				.map(BitPos::select)
				.sum::<BitMask<R>>();
			assert_eq!(
				mask,
				check,
				"Error when verifying the implementation of `BitOrder` for \
				 `{o}`: `{o}::mask::<{m}>({f}, {u})` produced {bad:b}, but \
				 expected {good:b}",
				o = ord_name,
				m = reg_name,
				f = from,
				u = upto,
				bad = mask,
				good = check,
			);
		}
	}
}

/// An ordering that does not provide a contiguous index map or `BitField`
/// acceleration.
#[cfg(test)]
pub struct HiLo;

#[cfg(test)]
unsafe impl BitOrder for HiLo {
	fn at<R>(index: BitIdx<R>) -> BitPos<R>
	where R: BitRegister {
		BitPos::new(index.into_inner() ^ 4).unwrap()
	}
}

#[cfg(test)]
mod tests {
	use super::*;

	#[test]
	fn default_impl() {
		assert_eq!(Lsb0::mask(None, None), BitMask::<u8>::ALL);
		assert_eq!(Msb0::mask(None, None), BitMask::<u8>::ALL);
		assert_eq!(HiLo::mask(None, None), BitMask::<u8>::ALL);

		assert_eq!(
			HiLo::mask(None, BitEnd::<u8>::new(3).unwrap()),
			BitMask::new(0b0111_0000),
		);
		assert_eq!(
			HiLo::mask(BitIdx::<u8>::new(3).unwrap(), None),
			BitMask::new(0b1000_1111),
		);
	}

	//  Split these out into individual test functions so they can parallelize.

	mod lsb0 {
		use super::*;

		#[test]
		fn verify_u8() {
			verify_for_type::<u8, Lsb0>(cfg!(feature = "verbose"));
		}

		#[test]
		#[cfg(not(tarpaulin))]
		fn verify_u16() {
			verify_for_type::<u16, Lsb0>(cfg!(feature = "verbose"));
		}

		#[test]
		#[cfg(not(tarpaulin))]
		fn verify_u32() {
			verify_for_type::<u32, Lsb0>(cfg!(feature = "verbose"));
		}

		#[test]
		#[cfg(all(target_pointer_width = "64", not(tarpaulin)))]
		fn verify_u64() {
			verify_for_type::<u64, Lsb0>(cfg!(feature = "verbose"));
		}

		#[test]
		#[cfg(not(tarpaulin))]
		fn verify_usize() {
			verify_for_type::<usize, Lsb0>(cfg!(feature = "verbose"));
		}
	}

	mod msb0 {
		use super::*;

		#[test]
		fn verify_u8() {
			verify_for_type::<u8, Msb0>(cfg!(feature = "verbose"));
		}

		#[test]
		#[cfg(not(tarpaulin))]
		fn verify_u16() {
			verify_for_type::<u16, Msb0>(cfg!(feature = "verbose"));
		}

		#[test]
		#[cfg(not(tarpaulin))]
		fn verify_u32() {
			verify_for_type::<u32, Msb0>(cfg!(feature = "verbose"));
		}

		#[test]
		#[cfg(all(target_pointer_width = "64", not(tarpaulin)))]
		fn verify_u64() {
			verify_for_type::<u64, Msb0>(cfg!(feature = "verbose"));
		}

		#[test]
		#[cfg(not(tarpaulin))]
		fn verify_usize() {
			verify_for_type::<usize, Msb0>(cfg!(feature = "verbose"));
		}
	}

	mod hilo {
		use super::*;

		#[test]
		fn verify_u8() {
			verify_for_type::<u8, HiLo>(cfg!(feature = "verbose"));
		}

		#[test]
		#[cfg(not(tarpaulin))]
		fn verify_u16() {
			verify_for_type::<u16, HiLo>(cfg!(feature = "verbose"));
		}

		#[test]
		#[cfg(not(tarpaulin))]
		fn verify_u32() {
			verify_for_type::<u32, HiLo>(cfg!(feature = "verbose"));
		}

		#[test]
		#[cfg(all(target_pointer_width = "64", not(tarpaulin)))]
		fn verify_u64() {
			verify_for_type::<u64, HiLo>(cfg!(feature = "verbose"));
		}

		#[test]
		#[cfg(not(tarpaulin))]
		fn verify_usize() {
			verify_for_type::<usize, HiLo>(cfg!(feature = "verbose"));
		}
	}
}
