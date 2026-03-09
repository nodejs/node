#![doc = include_str!("../doc/ptr.md")]

use core::hash::{
	Hash,
	Hasher,
};

use wyz::bidi::BidiIterator;

use crate::{
	devel as dvl,
	order::BitOrder,
	slice::BitSlice,
	store::BitStore,
};

mod addr;
mod proxy;
mod range;
mod single;
mod span;
mod tests;

pub use wyz::comu::{
	Const,
	Mut,
	Mutability,
};

pub(crate) use self::{
	addr::AddressExt,
	span::BitSpan,
};
pub use self::{
	addr::{
		check_alignment,
		MisalignError,
	},
	proxy::BitRef,
	range::BitPtrRange,
	single::{
		BitPtr,
		BitPtrError,
	},
	span::BitSpanError,
};

#[inline]
#[doc = include_str!("../doc/ptr/copy.md")]
pub unsafe fn copy<T1, T2, O1, O2>(
	src: BitPtr<Const, T1, O1>,
	dst: BitPtr<Mut, T2, O2>,
	count: usize,
) where
	O1: BitOrder,
	O2: BitOrder,
	T1: BitStore,
	T2: BitStore,
{
	//  Overlap is only defined if the orderings are identical.
	if dvl::match_order::<O1, O2>() {
		let (addr, head) = dst.raw_parts();
		let dst = BitPtr::<Mut, T2, O1>::new_unchecked(addr, head);
		let src_pair = src.range(count);

		let rev = src_pair.contains(&dst);
		for (from, to) in src_pair.zip(dst.range(count)).bidi(rev) {
			to.write(from.read());
		}
	}
	else {
		copy_nonoverlapping(src, dst, count);
	}
}

#[inline]
#[doc = include_str!("../doc/ptr/copy_nonoverlapping.md")]
pub unsafe fn copy_nonoverlapping<T1, T2, O1, O2>(
	src: BitPtr<Const, T1, O1>,
	dst: BitPtr<Mut, T2, O2>,
	count: usize,
) where
	O1: BitOrder,
	O2: BitOrder,
	T1: BitStore,
	T2: BitStore,
{
	for (from, to) in src.range(count).zip(dst.range(count)) {
		to.write(from.read());
	}
}

#[inline]
#[doc = include_str!("../doc/ptr/drop_in_place.md")]
#[deprecated = "this has no effect, and should not be called"]
pub unsafe fn drop_in_place<T, O>(_: BitPtr<Mut, T, O>)
where
	T: BitStore,
	O: BitOrder,
{
}

#[doc = include_str!("../doc/ptr/eq.md")]
#[inline]
pub fn eq<T1, T2, O>(
	this: BitPtr<Const, T1, O>,
	that: BitPtr<Const, T2, O>,
) -> bool
where
	T1: BitStore,
	T2: BitStore,
	O: BitOrder,
{
	this == that
}

#[inline]
#[cfg(not(tarpaulin_include))]
#[doc = include_str!("../doc/ptr/hash.md")]
pub fn hash<T, O, S>(ptr: BitPtr<Const, T, O>, into: &mut S)
where
	T: BitStore,
	O: BitOrder,
	S: Hasher,
{
	ptr.hash(into);
}

#[inline]
#[cfg(not(tarpaulin_include))]
#[doc = include_str!("../doc/ptr/null.md")]
pub fn null<T, O>() -> BitPtr<Const, T, O>
where
	T: BitStore,
	O: BitOrder,
{
	BitPtr::DANGLING
}

#[inline]
#[cfg(not(tarpaulin_include))]
#[doc = include_str!("../doc/ptr/null_mut.md")]
pub fn null_mut<T, O>() -> BitPtr<Mut, T, O>
where
	T: BitStore,
	O: BitOrder,
{
	BitPtr::DANGLING
}

#[inline]
#[cfg(not(tarpaulin_include))]
#[doc = include_str!("../doc/ptr/read.md")]
pub unsafe fn read<T, O>(src: BitPtr<Const, T, O>) -> bool
where
	T: BitStore,
	O: BitOrder,
{
	src.read()
}

#[inline]
#[allow(deprecated)]
#[cfg(not(tarpaulin_include))]
#[doc = include_str!("../doc/ptr/read_unaligned.md")]
#[deprecated = "`BitPtr` does not have unaligned addresses"]
pub unsafe fn read_unaligned<T, O>(src: BitPtr<Const, T, O>) -> bool
where
	T: BitStore,
	O: BitOrder,
{
	src.read_unaligned()
}

#[inline]
#[cfg(not(tarpaulin_include))]
#[doc = include_str!("../doc/ptr/read_volatile.md")]
pub unsafe fn read_volatile<T, O>(src: BitPtr<Const, T, O>) -> bool
where
	T: BitStore,
	O: BitOrder,
{
	src.read_volatile()
}

#[inline]
#[cfg(not(tarpaulin_include))]
#[doc = include_str!("../doc/ptr/replace.md")]
pub unsafe fn replace<T, O>(dst: BitPtr<Mut, T, O>, src: bool) -> bool
where
	T: BitStore,
	O: BitOrder,
{
	dst.replace(src)
}

#[inline]
#[cfg(not(tarpaulin_include))]
#[doc = include_str!("../doc/ptr/slice_from_raw_parts.md")]
pub fn slice_from_raw_parts<T, O>(
	ptr: BitPtr<Const, T, O>,
	len: usize,
) -> *const BitSlice<T, O>
where
	T: BitStore,
	O: BitOrder,
{
	bitslice_from_raw_parts(ptr, len)
}

#[inline]
#[cfg(not(tarpaulin_include))]
#[doc = include_str!("../doc/ptr/slice_from_raw_parts_mut.md")]
pub fn slice_from_raw_parts_mut<T, O>(
	ptr: BitPtr<Mut, T, O>,
	len: usize,
) -> *mut BitSlice<T, O>
where
	T: BitStore,
	O: BitOrder,
{
	bitslice_from_raw_parts_mut(ptr, len)
}

#[inline]
#[doc = include_str!("../doc/ptr/swap.md")]
pub unsafe fn swap<T1, T2, O1, O2>(
	one: BitPtr<Mut, T1, O1>,
	two: BitPtr<Mut, T2, O2>,
) where
	T1: BitStore,
	T2: BitStore,
	O1: BitOrder,
	O2: BitOrder,
{
	one.write(two.replace(one.read()));
}

#[inline]
#[doc = include_str!("../doc/ptr/swap_nonoverlapping.md")]
pub unsafe fn swap_nonoverlapping<T1, T2, O1, O2>(
	mut one: BitPtr<Mut, T1, O1>,
	mut two: BitPtr<Mut, T2, O2>,
	count: usize,
) where
	O1: BitOrder,
	O2: BitOrder,
	T1: BitStore,
	T2: BitStore,
{
	//  Note: compare codegen with `one.range(count).zip(two.range(count))`.
	for _ in 0 .. count {
		swap(one, two);
		one = one.add(1);
		two = two.add(1);
	}
}

#[inline]
#[cfg(not(tarpaulin_include))]
#[doc = include_str!("../doc/ptr/write.md")]
pub unsafe fn write<T, O>(dst: BitPtr<Mut, T, O>, value: bool)
where
	T: BitStore,
	O: BitOrder,
{
	dst.write(value);
}

#[inline]
#[cfg(not(tarpaulin_include))]
#[deprecated = "use `write_bits()` instead"]
#[doc = include_str!("../doc/ptr/write_bytes.md")]
pub unsafe fn write_bytes<T, O>(
	dst: BitPtr<Mut, T, O>,
	value: bool,
	count: usize,
) where
	T: BitStore,
	O: BitOrder,
{
	write_bits(dst, value, count)
}

#[inline]
#[allow(deprecated)]
#[cfg(not(tarpaulin_include))]
#[doc = include_str!("../doc/ptr/write_unaligned.md")]
#[deprecated = "`BitPtr` does not have unaligned addresses"]
pub unsafe fn write_unaligned<T, O>(dst: BitPtr<Mut, T, O>, value: bool)
where
	T: BitStore,
	O: BitOrder,
{
	dst.write_unaligned(value);
}

#[inline]
#[cfg(not(tarpaulin_include))]
#[doc = include_str!("../doc/ptr/write_volatile.md")]
pub unsafe fn write_volatile<T, O>(dst: BitPtr<Mut, T, O>, value: bool)
where
	T: BitStore,
	O: BitOrder,
{
	dst.write_volatile(value);
}

//  Renamed variants.

#[inline]
#[cfg(not(tarpaulin_include))]
#[doc = include_str!("../doc/ptr/bitslice_from_raw_parts.md")]
pub fn bitslice_from_raw_parts<T, O>(
	ptr: BitPtr<Const, T, O>,
	len: usize,
) -> *const BitSlice<T, O>
where
	T: BitStore,
	O: BitOrder,
{
	ptr.span(len).unwrap().into_bitslice_ptr()
}

#[inline]
#[cfg(not(tarpaulin_include))]
#[doc = include_str!("../doc/ptr/bitslice_from_raw_parts_mut.md")]
pub fn bitslice_from_raw_parts_mut<T, O>(
	ptr: BitPtr<Mut, T, O>,
	len: usize,
) -> *mut BitSlice<T, O>
where
	T: BitStore,
	O: BitOrder,
{
	ptr.span(len).unwrap().into_bitslice_ptr_mut()
}

#[inline]
#[doc = include_str!("../doc/ptr/write_bits.md")]
pub unsafe fn write_bits<T, O>(dst: BitPtr<Mut, T, O>, value: bool, count: usize)
where
	T: BitStore,
	O: BitOrder,
{
	for bit in dst.range(count) {
		bit.write(value);
	}
}
