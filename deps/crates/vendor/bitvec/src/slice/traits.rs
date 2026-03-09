#![doc = include_str!("../../doc/slice/traits.md")]

#[cfg(feature = "alloc")]
use alloc::borrow::ToOwned;
use core::{
	cmp,
	convert::TryFrom,
	fmt::{
		self,
		Binary,
		Debug,
		Display,
		Formatter,
		LowerHex,
		Octal,
		Pointer,
		UpperHex,
	},
	hash::{
		Hash,
		Hasher,
	},
	str,
};

use wyz::fmt::FmtForward;

use super::BitSlice;
#[cfg(feature = "alloc")]
use crate::vec::BitVec;
use crate::{
	domain::Domain,
	mem,
	order::{
		BitOrder,
		Lsb0,
		Msb0,
	},
	store::BitStore,
	view::BitView,
};

/// [Original](https://doc.rust-lang.org/std/primitive.slice.html#impl-AsRef%3C%5BT%5D%3E)
impl<T, O> AsRef<Self> for BitSlice<T, O>
where
	T: BitStore,
	O: BitOrder,
{
	#[inline]
	fn as_ref(&self) -> &Self {
		self
	}
}

/// [Original](https://doc.rust-lang.org/std/primitive.slice.html#impl-AsMut%3C%5BT%5D%3E)
impl<T, O> AsMut<Self> for BitSlice<T, O>
where
	T: BitStore,
	O: BitOrder,
{
	#[inline]
	fn as_mut(&mut self) -> &mut Self {
		self
	}
}

/// [Original](https://doc.rust-lang.org/std/primitive.slice.html#impl-Eq)
impl<T, O> Eq for BitSlice<T, O>
where
	T: BitStore,
	O: BitOrder,
{
}

/// [Original](https://doc.rust-lang.org/std/primitive.slice.html#impl-Ord)
impl<T, O> Ord for BitSlice<T, O>
where
	T: BitStore,
	O: BitOrder,
{
	#[inline]
	fn cmp(&self, rhs: &Self) -> cmp::Ordering {
		self.partial_cmp(rhs)
			.expect("BitSlice has a total ordering")
	}
}

/** Tests if two `BitSlice`s are semantically — not representationally — equal.

It is valid to compare slices of different ordering or memory types.

The equality condition requires that they have the same length and that at each
index, the two slices have the same bit value.

[Original](https://doc.rust-lang.org/std/primitive.slice.html#impl-PartialEq%3C%5BB%5D%3E)
**/
impl<T1, T2, O1, O2> PartialEq<BitSlice<T2, O2>> for BitSlice<T1, O1>
where
	T1: BitStore,
	T2: BitStore,
	O1: BitOrder,
	O2: BitOrder,
{
	#[inline]
	fn eq(&self, rhs: &BitSlice<T2, O2>) -> bool {
		if let (Some(this), Some(that)) =
			(self.coerce::<T1, Lsb0>(), rhs.coerce::<T1, Lsb0>())
		{
			this.sp_eq(that)
		}
		else if let (Some(this), Some(that)) =
			(self.coerce::<T1, Msb0>(), rhs.coerce::<T1, Msb0>())
		{
			this.sp_eq(that)
		}
		else {
			self.len() == rhs.len()
				&& self
					.iter()
					.by_vals()
					.zip(rhs.iter().by_vals())
					.all(|(l, r)| l == r)
		}
	}
}

//  ref-to-val equality

impl<T1, T2, O1, O2> PartialEq<BitSlice<T2, O2>> for &BitSlice<T1, O1>
where
	T1: BitStore,
	T2: BitStore,
	O1: BitOrder,
	O2: BitOrder,
{
	#[inline]
	fn eq(&self, rhs: &BitSlice<T2, O2>) -> bool {
		**self == rhs
	}
}

impl<T1, T2, O1, O2> PartialEq<BitSlice<T2, O2>> for &mut BitSlice<T1, O1>
where
	T1: BitStore,
	T2: BitStore,
	O1: BitOrder,
	O2: BitOrder,
{
	#[inline]
	fn eq(&self, rhs: &BitSlice<T2, O2>) -> bool {
		**self == rhs
	}
}

//  val-to-ref equality

impl<T1, T2, O1, O2> PartialEq<&BitSlice<T2, O2>> for BitSlice<T1, O1>
where
	T1: BitStore,
	T2: BitStore,
	O1: BitOrder,
	O2: BitOrder,
{
	#[inline]
	fn eq(&self, rhs: &&BitSlice<T2, O2>) -> bool {
		*self == **rhs
	}
}

impl<T1, T2, O1, O2> PartialEq<&mut BitSlice<T2, O2>> for BitSlice<T1, O1>
where
	T1: BitStore,
	T2: BitStore,
	O1: BitOrder,
	O2: BitOrder,
{
	#[inline]
	fn eq(&self, rhs: &&mut BitSlice<T2, O2>) -> bool {
		*self == **rhs
	}
}

/** Compares two `BitSlice`s by semantic — not representational — ordering.

The comparison sorts by testing at each index if one slice has a high bit where
the other has a low. At the first index where the slices differ, the slice with
the high bit is greater. If the slices are equal until at least one terminates,
then they are compared by length.

[Original](https://doc.rust-lang.org/std/primitive.slice.html#impl-PartialOrd%3C%5BT%5D%3E)
**/
impl<T1, T2, O1, O2> PartialOrd<BitSlice<T2, O2>> for BitSlice<T1, O1>
where
	T1: BitStore,
	T2: BitStore,
	O1: BitOrder,
	O2: BitOrder,
{
	#[inline]
	fn partial_cmp(&self, rhs: &BitSlice<T2, O2>) -> Option<cmp::Ordering> {
		for (l, r) in self.iter().by_vals().zip(rhs.iter().by_vals()) {
			match (l, r) {
				(true, false) => return Some(cmp::Ordering::Greater),
				(false, true) => return Some(cmp::Ordering::Less),
				_ => continue,
			}
		}
		self.len().partial_cmp(&rhs.len())
	}
}

//  ref-to-val ordering

impl<T1, T2, O1, O2> PartialOrd<BitSlice<T2, O2>> for &BitSlice<T1, O1>
where
	T1: BitStore,
	T2: BitStore,
	O1: BitOrder,
	O2: BitOrder,
{
	#[inline]
	fn partial_cmp(&self, rhs: &BitSlice<T2, O2>) -> Option<cmp::Ordering> {
		(*self).partial_cmp(rhs)
	}
}

impl<T1, T2, O1, O2> PartialOrd<BitSlice<T2, O2>> for &mut BitSlice<T1, O1>
where
	T1: BitStore,
	T2: BitStore,
	O1: BitOrder,
	O2: BitOrder,
{
	#[inline]
	fn partial_cmp(&self, rhs: &BitSlice<T2, O2>) -> Option<cmp::Ordering> {
		(**self).partial_cmp(rhs)
	}
}

//  val-to-ref ordering

impl<T1, T2, O1, O2> PartialOrd<&BitSlice<T2, O2>> for BitSlice<T1, O1>
where
	T1: BitStore,
	T2: BitStore,
	O1: BitOrder,
	O2: BitOrder,
{
	#[inline]
	fn partial_cmp(&self, rhs: &&BitSlice<T2, O2>) -> Option<cmp::Ordering> {
		(*self).partial_cmp(&**rhs)
	}
}

impl<T1, T2, O1, O2> PartialOrd<&mut BitSlice<T2, O2>> for BitSlice<T1, O1>
where
	T1: BitStore,
	T2: BitStore,
	O1: BitOrder,
	O2: BitOrder,
{
	#[inline]
	fn partial_cmp(&self, rhs: &&mut BitSlice<T2, O2>) -> Option<cmp::Ordering> {
		(*self).partial_cmp(&**rhs)
	}
}

//  &mut-to-& ordering

impl<T1, T2, O1, O2> PartialOrd<&mut BitSlice<T2, O2>> for &BitSlice<T1, O1>
where
	T1: BitStore,
	T2: BitStore,
	O1: BitOrder,
	O2: BitOrder,
{
	#[inline]
	fn partial_cmp(&self, rhs: &&mut BitSlice<T2, O2>) -> Option<cmp::Ordering> {
		(**self).partial_cmp(&**rhs)
	}
}

impl<T1, T2, O1, O2> PartialOrd<&BitSlice<T2, O2>> for &mut BitSlice<T1, O1>
where
	T1: BitStore,
	T2: BitStore,
	O1: BitOrder,
	O2: BitOrder,
{
	#[inline]
	fn partial_cmp(&self, rhs: &&BitSlice<T2, O2>) -> Option<cmp::Ordering> {
		(**self).partial_cmp(&**rhs)
	}
}

/** Calls [`BitSlice::try_from_slice`], but returns the original Rust slice on
error instead of the failure event.

This only fails if `slice.len()` exceeds `BitSlice::MAX_ELTS`.

[`BitSlice::try_from_slice`]: crate::slice::BitSlice::try_from_slice
**/
impl<'a, T, O> TryFrom<&'a [T]> for &'a BitSlice<T, O>
where
	T: BitStore,
	O: BitOrder,
{
	type Error = &'a [T];

	#[inline]
	fn try_from(slice: &'a [T]) -> Result<Self, Self::Error> {
		BitSlice::try_from_slice(slice).map_err(|_| slice)
	}
}

/** Calls [`BitSlice::try_from_slice_mut`], but returns the original Rust slice
on error instead of the failure event.

This only fails if `slice.len()` exceeds `BitSlice::MAX_ELTS`.

[`BitSlice::try_from_slice_mut`]: crate::slice::BitSlice::try_from_slice_mut
**/
impl<'a, T, O> TryFrom<&'a mut [T]> for &'a mut BitSlice<T, O>
where
	T: BitStore,
	O: BitOrder,
{
	type Error = &'a mut [T];

	#[inline]
	fn try_from(slice: &'a mut [T]) -> Result<Self, Self::Error> {
		let slice_ptr = slice as *mut [T];
		BitSlice::try_from_slice_mut(slice)
			.map_err(|_| unsafe { &mut *slice_ptr })
	}
}

/// [Original](https://doc.rust-lang.org/std/primitive.slice.html#impl-Default-1)
impl<T, O> Default for &BitSlice<T, O>
where
	T: BitStore,
	O: BitOrder,
{
	#[inline]
	fn default() -> Self {
		BitSlice::empty()
	}
}

/// [Original](https://doc.rust-lang.org/std/primitive.slice.html#impl-Default)
impl<T, O> Default for &mut BitSlice<T, O>
where
	T: BitStore,
	O: BitOrder,
{
	#[inline]
	fn default() -> Self {
		BitSlice::empty_mut()
	}
}

/// [Original](https://doc.rust-lang.org/std/primitive.slice.html#impl-Debug)
impl<T, O> Debug for BitSlice<T, O>
where
	T: BitStore,
	O: BitOrder,
{
	#[inline]
	fn fmt(&self, fmt: &mut Formatter) -> fmt::Result {
		self.as_bitspan().render(fmt, "Slice", None)?;
		fmt.write_str(" ")?;
		Display::fmt(self, fmt)
	}
}

impl<T, O> Display for BitSlice<T, O>
where
	T: BitStore,
	O: BitOrder,
{
	#[inline]
	fn fmt(&self, fmt: &mut Formatter) -> fmt::Result {
		fmt.debug_list()
			.entries(self.iter().by_vals().map(|b| if b { 1 } else { 0 }))
			.finish()
	}
}

impl<T, O> Pointer for BitSlice<T, O>
where
	T: BitStore,
	O: BitOrder,
{
	#[inline]
	fn fmt(&self, fmt: &mut Formatter) -> fmt::Result {
		Pointer::fmt(&self.as_bitspan(), fmt)
	}
}

/// Encodes a short bit-slice into an ASCII b36 value.
#[inline(always)]
fn bits_to_ascii<T, O>(bits: &BitSlice<T, O>, alpha: u8) -> u8
where
	T: BitStore,
	O: BitOrder,
{
	let mut val = 0u8;
	for bit in bits.iter().by_vals() {
		val <<= 1;
		val |= bit as u8;
	}
	match val {
		v @ 0 ..= 9 => b'0' + v,
		v @ 10 ..= 35 => alpha - 10 + v,
		_ => unreachable!(
			"bit-slices wider than five bits cannot be rendered to ASCII b36"
		),
	}
}

/** Encodes an arbitrary bit-slice into an ASCII b36 string.

## Parameters

- `bits`: the bit-slice to encode.
- `into`: a provided buffer into which the bit-slice is encoded.
- `radix`: the bit width of each digit (log2 of its radix).
- `skip`: the number of bytes to skip before beginning the write.
- `alpha`: one of `b'a'` or `b'A'`.

## Returns

A subset of `into` that is now initialized to the ASCII encoding.
**/
#[inline(always)]
fn encode_ascii<'a, T, O>(
	bits: &BitSlice<T, O>,
	into: &'a mut [u8],
	radix: usize,
	mut skip: usize,
	alpha: u8,
) -> &'a str
where
	T: BitStore,
	O: BitOrder,
{
	for (chunk, slot) in
		bits.rchunks(radix).rev().zip(into.iter_mut().skip(skip))
	{
		*slot = bits_to_ascii(chunk, alpha);
		skip += 1;
	}
	unsafe { str::from_utf8_unchecked(&into[.. skip]) }
}

/// Constructs the numeric formatting implementations.
macro_rules! fmt {
	($($trait:ident: $alpha:expr, $pfx:expr, $radix:expr;)+) => { $(
		#[doc = include_str!("../../doc/slice/format.md")]
		impl<T, O> $trait for BitSlice<T, O>
		where
			T: BitStore,
			O: BitOrder,
		{
			#[inline]
			#[allow(clippy::modulo_one)] // I know what I’m doing.
			//  TODO(myrrlyn): See if Binary codegen ditches the loops.
			fn fmt(&self, fmt: &mut Formatter) -> fmt::Result {
				const D: usize = mem::bits_of::<usize>() / $radix;
				const M: usize = mem::bits_of::<usize>() % $radix;
				const W: usize = D + (M != 0) as usize;
				let mut txt: [u8; W + 2] = [b'0'; W + 2];
				txt[1] = $pfx;

				let start = if fmt.alternate() { 0 } else { 2 };
				let mut seq = fmt.debug_list();
				match self.domain() {
					Domain::Enclave(elem) => {
						seq.entry(&encode_ascii(
							elem.into_bitslice(),
							&mut txt[start ..],
							$radix,
							2 - start,
							$alpha,
						).fmt_display());
					},
					Domain::Region { head, body, tail } => {
						if let Some(elem) = head {
							seq.entry(&encode_ascii(
								elem.into_bitslice(),
								&mut txt[start ..],
								$radix,
								2 - start,
								$alpha,
							).fmt_display());
						}
						for elem in body.iter().map(BitStore::load_value) {
							seq.entry(&encode_ascii(
								elem.view_bits::<O>(),
								&mut txt[start ..],
								$radix,
								2 - start,
								$alpha,
							).fmt_display());
						}
						if let Some(elem) = tail {
							seq.entry(&encode_ascii(
								elem.into_bitslice(),
								&mut txt[start ..],
								$radix,
								2 - start,
								$alpha,
							).fmt_display());
						}
					},
				}
				seq.finish()
			}
		}
	)+ };
}

fmt! {
	Binary: b'0', b'b', 1;
	Octal: b'0', b'o', 3;
	LowerHex: b'a', b'x', 4;
	UpperHex: b'A', b'x', 4;
}

/// [Original](https://doc.rust-lang.org/std/primitive.slice.html#impl-Hash)
#[cfg(not(tarpaulin_include))]
impl<T, O> Hash for BitSlice<T, O>
where
	T: BitStore,
	O: BitOrder,
{
	#[inline]
	fn hash<H>(&self, hasher: &mut H)
	where H: Hasher {
		self.iter().by_vals().for_each(|bit| bit.hash(hasher));
	}
}

#[doc = include_str!("../../doc/slice/threadsafe.md")]
unsafe impl<T, O> Send for BitSlice<T, O>
where
	T: BitStore + Sync,
	O: BitOrder,
{
}

#[doc = include_str!("../../doc/slice/threadsafe.md")]
unsafe impl<T, O> Sync for BitSlice<T, O>
where
	T: BitStore + Sync,
	O: BitOrder,
{
}

/// [Original](https://doc.rust-lang.org/std/primitive.slice.html#impl-Unpin)
impl<T, O> Unpin for BitSlice<T, O>
where
	T: BitStore,
	O: BitOrder,
{
}

/// [Original](https://doc.rust-lang.org/std/primitive.slice.html#impl-ToOwned)
#[cfg(feature = "alloc")]
#[cfg(not(tarpaulin_include))]
impl<T, O> ToOwned for BitSlice<T, O>
where
	T: BitStore,
	O: BitOrder,
{
	type Owned = BitVec<T, O>;

	#[inline]
	fn to_owned(&self) -> Self::Owned {
		BitVec::from_bitslice(self)
	}
}
