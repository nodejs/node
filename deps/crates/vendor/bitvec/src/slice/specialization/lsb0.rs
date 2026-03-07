//! Specializations for `BitSlice<_, Lsb0>.

use core::iter;

use funty::Integral;
use wyz::{
	bidi::BidiIterator,
	range::RangeExt,
};

use super::{
	has_one,
	has_zero,
	WORD_BITS,
};
use crate::{
	domain::Domain,
	field::BitField,
	mem::bits_of,
	order::Lsb0,
	slice::BitSlice,
	store::BitStore,
};

impl<T> BitSlice<T, Lsb0>
where T: BitStore
{
	/// Accelerates Boolean arithmetic.
	///
	/// This applies a Boolean-arithmetic function across all the bits in a
	/// pair. The secondary bit-slice is zero-extended if it expires before
	/// `self` does.
	///
	/// Because the two bit-slices share the same types, this is able to
	/// batch-load `usize` chunks from each, apply the arithmetic to them, and
	/// write the result back into `self`. Any leftover bits are handled
	/// individually.
	pub(crate) fn sp_bitop_assign(
		&mut self,
		rhs: &Self,
		word_op: fn(usize, usize) -> usize,
		bool_op: fn(bool, bool) -> bool,
	) {
		let (mut this, mut that) = (self, rhs);
		while this.len() >= WORD_BITS && that.len() >= WORD_BITS {
			unsafe {
				let (l, left) = this.split_at_unchecked_mut_noalias(WORD_BITS);
				let (r, right) = that.split_at_unchecked(WORD_BITS);
				this = left;
				that = right;
				let (a, b) = (l.load_le::<usize>(), r.load_le::<usize>());
				l.store_le(word_op(a, b));
			}
		}
		//  Note: it might actually be possible to do a partial-word load/store
		//  to exhaust the shorter bit-slice. Investigate further.
		for (l, r) in this
			.as_mut_bitptr_range()
			.zip(that.iter().by_vals().chain(iter::repeat(false)))
		{
			unsafe {
				l.write(bool_op(l.read(), r));
			}
		}
	}

	/// Accelerates copies between disjoint bit-slices with batch loads.
	pub(crate) fn sp_copy_from_bitslice(&mut self, src: &Self) {
		assert_eq!(
			self.len(),
			src.len(),
			"copying between bit-slices requires equal lengths",
		);

		for (to, from) in unsafe { self.chunks_mut(WORD_BITS).remove_alias() }
			.zip(src.chunks(WORD_BITS))
		{
			to.store_le::<usize>(from.load_le::<usize>());
		}
	}

	/// Accelerates possibly-overlapping copies within a single bit-slice with
	/// batch loads.
	pub(crate) unsafe fn sp_copy_within_unchecked(
		&mut self,
		src: impl RangeExt<usize>,
		dest: usize,
	) {
		let source = src.normalize(None, self.len());
		let rev = source.contains(&dest);
		let dest = dest .. dest + source.len();

		let this = self.as_accessor();
		let from = this
			.get_unchecked(source)
			.chunks(WORD_BITS)
			.map(|bits| bits as *const BitSlice<T::Access, Lsb0>);
		let to = this.get_unchecked(dest).chunks(WORD_BITS).map(|bits| {
			bits as *const BitSlice<T::Access, Lsb0>
				as *mut BitSlice<T::Access, Lsb0>
		});
		for (from, to) in from.zip(to).bidi(rev) {
			let value = (*from).load_le::<usize>();
			(*to).store_le::<usize>(value);
		}
	}

	/// Accelerates equality checking with batch loads.
	pub(crate) fn sp_eq(&self, other: &Self) -> bool {
		self.len() == other.len()
			&& self
				.chunks(WORD_BITS)
				.zip(other.chunks(WORD_BITS))
				.all(|(a, b)| a.load_le::<usize>() == b.load_le::<usize>())
	}

	/// Seeks the index of the first `1` bit in the bit-slice.
	pub(crate) fn sp_first_one(&self) -> Option<usize> {
		let mut accum = 0;

		match self.domain() {
			Domain::Enclave(elem) => {
				let val = elem.load_value();
				if has_one(val, elem.mask().into_inner()) {
					accum += val.trailing_zeros() as usize
						- elem.head().into_inner() as usize;
					return Some(accum);
				}
				None
			},
			Domain::Region { head, body, tail } => {
				if let Some(elem) = head {
					let val = elem.load_value();
					accum += val.trailing_zeros() as usize
						- elem.head().into_inner() as usize;
					if has_one(val, elem.mask().into_inner()) {
						return Some(accum);
					}
				}

				for val in body.iter().map(BitStore::load_value) {
					accum += val.trailing_zeros() as usize;
					if has_one(val, !<T::Mem as Integral>::ZERO) {
						return Some(accum);
					}
				}

				if let Some(elem) = tail {
					let val = elem.load_value();
					if has_one(val, elem.mask().into_inner()) {
						accum += val.trailing_zeros() as usize;
						return Some(accum);
					}
				}

				None
			},
		}
	}

	/// Seeks the index of the last `1` bit in the bit-slice.
	pub(crate) fn sp_last_one(&self) -> Option<usize> {
		let mut out = self.len();

		match self.domain() {
			Domain::Enclave(elem) => {
				let val = elem.load_value();
				let dead_bits =
					bits_of::<T::Mem>() - elem.tail().into_inner() as usize;
				if has_one(val, elem.mask().into_inner()) {
					out -= val.leading_zeros() as usize - dead_bits as usize;
					return Some(out - 1);
				}
				None
			},
			Domain::Region { head, body, tail } => {
				if let Some(elem) = tail {
					let val = elem.load_value();
					let dead_bits =
						bits_of::<T::Mem>() - elem.tail().into_inner() as usize;
					out -= val.leading_zeros() as usize - dead_bits;
					if has_one(val, elem.mask().into_inner()) {
						return Some(out - 1);
					}
				}

				for val in body.iter().map(BitStore::load_value).rev() {
					out -= val.leading_zeros() as usize;
					if has_one(val, !<T::Mem as Integral>::ZERO) {
						return Some(out - 1);
					}
				}

				if let Some(elem) = head {
					let val = elem.load_value();
					if has_one(val, elem.mask().into_inner()) {
						out -= val.leading_zeros() as usize;
						return Some(out - 1);
					}
				}

				None
			},
		}
	}

	/// Seeks the index of the first `0` bit in the bit-slice.
	pub(crate) fn sp_first_zero(&self) -> Option<usize> {
		let mut accum = 0;

		match self.domain() {
			Domain::Enclave(elem) => {
				let val = elem.load_value() | !elem.mask().into_inner();
				accum += val.trailing_ones() as usize
					- elem.head().into_inner() as usize;
				if has_zero(val, elem.mask().into_inner()) {
					return Some(accum);
				}
				None
			},
			Domain::Region { head, body, tail } => {
				if let Some(elem) = head {
					let val = elem.load_value() | !elem.mask().into_inner();

					accum += val.trailing_ones() as usize
						- elem.head().into_inner() as usize;
					if has_zero(val, elem.mask().into_inner()) {
						return Some(accum);
					}
				}

				for val in body.iter().map(BitStore::load_value) {
					accum += val.trailing_ones() as usize;
					if has_zero(val, !<T::Mem as Integral>::ZERO) {
						return Some(accum);
					}
				}

				if let Some(elem) = tail {
					let val = elem.load_value() | !elem.mask().into_inner();
					accum += val.trailing_ones() as usize;
					if has_zero(val, elem.mask().into_inner()) {
						return Some(accum);
					}
				}

				None
			},
		}
	}

	/// Seeks the index of the last `0` bit in the bit-slice.
	pub(crate) fn sp_last_zero(&self) -> Option<usize> {
		let mut out = self.len();

		match self.domain() {
			Domain::Enclave(elem) => {
				let val = elem.load_value() | !elem.mask().into_inner();
				let dead_bits =
					bits_of::<T::Mem>() - elem.tail().into_inner() as usize;
				if has_zero(val, elem.mask().into_inner()) {
					out -= val.leading_ones() as usize - dead_bits as usize;
					return Some(out - 1);
				}
				None
			},
			Domain::Region { head, body, tail } => {
				if let Some(elem) = tail {
					let val = elem.load_value() | !elem.mask().into_inner();
					let dead_bits =
						bits_of::<T::Mem>() - elem.tail().into_inner() as usize;
					out -= val.leading_ones() as usize - dead_bits;
					if has_zero(val, elem.mask().into_inner()) {
						return Some(out - 1);
					}
				}

				for val in body.iter().map(BitStore::load_value).rev() {
					out -= val.leading_ones() as usize;
					if has_zero(val, !<T::Mem as Integral>::ZERO) {
						return Some(out - 1);
					}
				}

				if let Some(elem) = head {
					let val = elem.load_value() | !elem.mask().into_inner();
					if has_zero(val, elem.mask().into_inner()) {
						out -= val.leading_ones() as usize;
						return Some(out - 1);
					}
				}

				None
			},
		}
	}

	/// Accelerates swapping memory.
	pub(crate) fn sp_swap_with_bitslice(&mut self, other: &mut Self) {
		for (this, that) in unsafe {
			self.chunks_mut(WORD_BITS)
				.remove_alias()
				.zip(other.chunks_mut(WORD_BITS).remove_alias())
		} {
			let (a, b) = (this.load_le::<usize>(), that.load_le::<usize>());
			this.store_le(b);
			that.store_le(a);
		}
	}
}
