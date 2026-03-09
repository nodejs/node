//! Unit tests for bit-pointers.

#![cfg(test)]

use core::cmp;

use crate::{
	index::BitIdx,
	prelude::*,
	ptr::{
		self as bv_ptr,
		AddressExt,
		BitSpan,
		BitSpanError,
		Mut,
	},
};

#[test]
fn free_functions() {
	let mut a = [0u8, !0];
	let mut b = 255u16;

	let one = BitPtr::<Mut, u8, Lsb0>::from_slice_mut(&mut a[..]);
	let two = one.wrapping_add(8);
	let three = BitPtr::<Mut, u16, Msb0>::from_mut(&mut b);
	let four = three.wrapping_add(8);

	unsafe {
		bv_ptr::copy(two.to_const(), one, 8);
	}
	assert_eq!(a[0], !0);
	unsafe {
		bv_ptr::copy(three.to_const(), one, 8);
	}
	assert_eq!(a[0], 0);

	assert!(!bv_ptr::eq(two.to_const(), one.to_const()));

	unsafe {
		bv_ptr::swap_nonoverlapping(two, three, 8);
	}
	assert_eq!(a[1], 0);
	assert_eq!(b, !0);

	unsafe {
		bv_ptr::write_bits(four, false, 8);
	}
	assert_eq!(b, 0xFF00);
}

#[test]
fn alignment() {
	let data = 0u16;
	let a = unsafe { (&data).into_address() };
	let b = a.cast::<u8>().wrapping_add(1).cast::<u16>();

	assert!(bv_ptr::check_alignment(a).is_ok());
	assert!(bv_ptr::check_alignment(b).is_err());
}

#[test]
fn proxy() {
	let mut data = 0u8;
	{
		let bits = data.view_bits_mut::<Lsb0>();
		let (mut a, rest) = bits.split_first_mut().unwrap();
		let (mut b, _) = rest.split_first_mut().unwrap();
		assert!(!a.replace(true));
		a.swap(&mut b);
		assert!(*b);
		a.set(true);
	}

	assert_eq!(data, 3);
}

#[test]
fn range() {
	let data = 0u8;
	let mut bpr = data.view_bits::<Lsb0>().as_bitptr_range();

	let range = bpr.clone().into_range();
	let bpr2 = range.into();
	assert_eq!(bpr, bpr2);

	assert!(bpr.nth_back(9).is_none());
}

#[test]
#[allow(deprecated)]
fn single() {
	let mut data = 1u16;
	let bp = data.view_bits_mut::<Lsb0>().as_mut_bitptr();

	assert!(!bp.is_null());
	let bp2 = bp.wrapping_add(9);
	assert_ne!(bp2.pointer().cast::<u8>(), bp2.cast::<u8>().pointer());

	assert!(unsafe { bp.read_volatile() });
	assert!(unsafe { bp.read_unaligned() });

	assert_eq!(bp.align_offset(2), 0);
	assert_eq!(bp2.align_offset(2), 7);

	unsafe {
		bp.write_volatile(false);
		bp.swap(bp2);
		bp2.write_unaligned(true);
	}

	assert_eq!(bp.cmp(&bp2), cmp::Ordering::Less);
	assert_ne!(bp, bp.cast::<u8>());
	assert!(bp.partial_cmp(&bp.cast::<u8>()).is_none());
}

#[test]
fn span() {
	let mut data = [0u32; 2];
	let addr = unsafe { data.as_mut_ptr().into_address() };

	let too_long = BitSpan::<Mut, u32, Lsb0>::REGION_MAX_BITS + 1;
	assert!(matches!(
		BitSpan::<_, _, Lsb0>::new(addr, BitIdx::MIN, too_long),
		Err(BitSpanError::TooLong(ct)) if ct == too_long));

	let bp = data.view_bits_mut::<Lsb0>().as_mut_bitptr();
	let bs = bp.cast::<u8>().wrapping_add(8).span(32).unwrap();
	let (l, c, r) = unsafe { bs.align_to::<u16>() };
	assert_eq!(l.len(), 8);
	assert_eq!(c.len(), 16);
	assert_eq!(r.len(), 8);

	let bs2 = bp.cast::<u8>().wrapping_add(3).span(3).unwrap();
	assert_eq!(
		unsafe { bs2.align_to::<u16>() },
		(bs2, BitSpan::EMPTY, BitSpan::EMPTY)
	);
}

#[test]
#[cfg(feature = "alloc")]
fn format() {
	#[cfg(not(feature = "std"))]
	use alloc::format;
	use core::any;

	let data = 1u8;
	let bits = data.view_bits::<Lsb0>();

	let bit = bits.first().unwrap();
	let render = format!("{:?}", bit);
	assert!(render.starts_with("BitRef<u8,"));
	assert!(render.ends_with("bits: 1, bit: true }"));

	let bitptr = bits.as_bitptr();
	let render = format!("{:?}", bitptr);
	assert!(render.starts_with("*const Bit<u8,"));
	assert!(render.ends_with(", 000)"), "{}", render);

	let bitspan = bitptr.wrapping_add(2).span(3).unwrap();
	let render = format!("{:?}", bitspan);
	let expected = format!(
		"BitSpan<u8, {}> {{ addr: {:p}, head: 010, bits: 3 }}",
		any::type_name::<Lsb0>(),
		bitspan.address(),
	);
	assert_eq!(render, expected);
	let render = format!("{:p}", bitspan);
	let expected = format!("{:p}(010)[3]", bitspan.address());
	assert_eq!(render, expected);
}
