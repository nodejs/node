use core::{
	cmp,
	convert::TryFrom,
};

use static_assertions::*;

use crate::prelude::*;

#[test]
fn core_impls() {
	use core::{
		cmp::{
			Eq,
			Ord,
		},
		fmt::Debug,
		hash::Hash,
		ops::{
			Index,
			Range,
		},
		panic::{
			RefUnwindSafe,
			UnwindSafe,
		},
	};

	assert_impl_all!(BitSlice<usize, Lsb0>:
		AsRef<BitSlice<usize, Lsb0>>,
		AsMut<BitSlice<usize, Lsb0>>,
		Debug,
		Eq,
		Hash,
		Index<usize>,
		Index<Range<usize>>,
		Ord,
		PartialEq<BitSlice<u32, Msb0>>,
		PartialOrd<BitSlice<u32, Msb0>>,
		Send,
		Sync,
		Unpin,
		UnwindSafe,
		RefUnwindSafe,
	);
	assert_impl_all!(&BitSlice<usize, Lsb0>:
		Default,
		IntoIterator,
	);
	assert_impl_all!(&mut BitSlice<usize, Lsb0>:
		Default,
		IntoIterator,
	);
}

#[test]
#[cfg(feature = "alloc")]
fn alloc_impls() {
	use alloc::borrow::ToOwned;

	assert_impl_all!(BitSlice<usize, Lsb0>:
		ToOwned,
	);
}

#[test]
#[allow(deprecated)]
#[cfg(feature = "std")]
fn std_impls() {
	use std::{
		ascii::AsciiExt,
		io::{
			BufRead,
			Read,
			Write,
		},
	};

	assert_impl_all!(&BitSlice<usize, Lsb0>:
		Read,
	);
	assert_impl_all!(&mut BitSlice<usize, Lsb0>:
		Write,
	);
	assert_not_impl_any!(BitSlice<u8, Lsb0>:
		AsciiExt,
		BufRead,
	);
}

#[test]
fn cmp() {
	let a = bits![0, 1];
	let b = bits![1, 0];
	let c = bits![u8, Msb0; 1, 0];
	let d = bits![u8, Msb0; 1, 1];

	assert_eq!(a.cmp(b), cmp::Ordering::Less);
	assert_ne!(a, b);
	assert_eq!(b, c);
	assert_ne!(c, d);
}

#[test]
fn conv() -> Result<(), ()> {
	let mut a = [0u8, 1, 2, 3];
	let _ = <&BitSlice<_, Lsb0>>::try_from(&a[..]).map_err(drop)?;
	let _ = <&mut BitSlice<_, Lsb0>>::try_from(&mut a[..]).map_err(drop)?;
	Ok(())
}

#[cfg(feature = "alloc")]
mod format {
	#[cfg(not(feature = "std"))]
	use alloc::format;

	use crate::prelude::*;

	#[test]
	fn binary() {
		let data = [0u8, 0x0F, !0];
		let bits = data.view_bits::<Msb0>();

		assert_eq!(format!("{:b}", &bits[.. 0]), "[]");
		assert_eq!(format!("{:#b}", &bits[.. 0]), "[]");

		assert_eq!(format!("{:b}", &bits[9 .. 15]), "[000111]");
		assert_eq!(
			format!("{:#b}", &bits[9 .. 15]),
			"[
    0b000111,
]"
		);

		assert_eq!(format!("{:b}", &bits[4 .. 20]), "[0000, 00001111, 1111]");
		assert_eq!(
			format!("{:#b}", &bits[4 .. 20]),
			"[
    0b0000,
    0b00001111,
    0b1111,
]"
		);

		assert_eq!(format!("{:b}", &bits[4 ..]), "[0000, 00001111, 11111111]");
		assert_eq!(
			format!("{:#b}", &bits[4 ..]),
			"[
    0b0000,
    0b00001111,
    0b11111111,
]"
		);

		assert_eq!(format!("{:b}", &bits[.. 20]), "[00000000, 00001111, 1111]");
		assert_eq!(
			format!("{:#b}", &bits[.. 20]),
			"[
    0b00000000,
    0b00001111,
    0b1111,
]"
		);

		assert_eq!(format!("{:b}", bits), "[00000000, 00001111, 11111111]");
		assert_eq!(
			format!("{:#b}", bits),
			"[
    0b00000000,
    0b00001111,
    0b11111111,
]"
		);
	}

	#[test]
	fn octal() {
		let data = [0u8, 0x0F, !0];
		let bits = data.view_bits::<Msb0>();

		assert_eq!(format!("{:o}", &bits[.. 0]), "[]");
		assert_eq!(format!("{:#o}", &bits[.. 0]), "[]");

		assert_eq!(format!("{:o}", &bits[9 .. 15]), "[07]");
		assert_eq!(
			format!("{:#o}", &bits[9 .. 15]),
			"[
    0o07,
]"
		);

		//  …0_000 00_001_111 1_111…
		assert_eq!(format!("{:o}", &bits[4 .. 20]), "[00, 017, 17]");
		assert_eq!(
			format!("{:#o}", &bits[4 .. 20]),
			"[
    0o00,
    0o017,
    0o17,
]"
		);

		assert_eq!(format!("{:o}", &bits[4 ..]), "[00, 017, 377]");
		assert_eq!(
			format!("{:#o}", &bits[4 ..]),
			"[
    0o00,
    0o017,
    0o377,
]"
		);

		assert_eq!(format!("{:o}", &bits[.. 20]), "[000, 017, 17]");
		assert_eq!(
			format!("{:#o}", &bits[.. 20]),
			"[
    0o000,
    0o017,
    0o17,
]"
		);

		assert_eq!(format!("{:o}", bits), "[000, 017, 377]");
		assert_eq!(
			format!("{:#o}", bits),
			"[
    0o000,
    0o017,
    0o377,
]"
		);
	}

	#[test]
	fn hex_lower() {
		let data = [0u8, 0x0F, !0];
		let bits = data.view_bits::<Msb0>();

		assert_eq!(format!("{:x}", &bits[.. 0]), "[]");
		assert_eq!(format!("{:#x}", &bits[.. 0]), "[]");

		//  …00_0111 …
		assert_eq!(format!("{:x}", &bits[9 .. 15]), "[07]");
		assert_eq!(
			format!("{:#x}", &bits[9 .. 15]),
			"[
    0x07,
]"
		);

		//  …0000 00001111 1111…
		assert_eq!(format!("{:x}", &bits[4 .. 20]), "[0, 0f, f]");
		assert_eq!(
			format!("{:#x}", &bits[4 .. 20]),
			"[
    0x0,
    0x0f,
    0xf,
]"
		);

		assert_eq!(format!("{:x}", &bits[4 ..]), "[0, 0f, ff]");
		assert_eq!(
			format!("{:#x}", &bits[4 ..]),
			"[
    0x0,
    0x0f,
    0xff,
]"
		);

		assert_eq!(format!("{:x}", &bits[.. 20]), "[00, 0f, f]");
		assert_eq!(
			format!("{:#x}", &bits[.. 20]),
			"[
    0x00,
    0x0f,
    0xf,
]"
		);

		assert_eq!(format!("{:x}", bits), "[00, 0f, ff]");
		assert_eq!(
			format!("{:#x}", bits),
			"[
    0x00,
    0x0f,
    0xff,
]"
		);
	}

	#[test]
	fn hex_upper() {
		let data = [0u8, 0x0F, !0];
		let bits = data.view_bits::<Msb0>();

		assert_eq!(format!("{:X}", &bits[.. 0]), "[]");
		assert_eq!(format!("{:#X}", &bits[.. 0]), "[]");

		assert_eq!(format!("{:X}", &bits[9 .. 15]), "[07]");
		assert_eq!(
			format!("{:#X}", &bits[9 .. 15]),
			"[
    0x07,
]"
		);

		assert_eq!(format!("{:X}", &bits[4 .. 20]), "[0, 0F, F]");
		assert_eq!(
			format!("{:#X}", &bits[4 .. 20]),
			"[
    0x0,
    0x0F,
    0xF,
]"
		);

		assert_eq!(format!("{:X}", &bits[4 ..]), "[0, 0F, FF]");
		assert_eq!(
			format!("{:#X}", &bits[4 ..]),
			"[
    0x0,
    0x0F,
    0xFF,
]"
		);

		assert_eq!(format!("{:X}", &bits[.. 20]), "[00, 0F, F]");
		assert_eq!(
			format!("{:#X}", &bits[.. 20]),
			"[
    0x00,
    0x0F,
    0xF,
]"
		);

		assert_eq!(format!("{:X}", bits), "[00, 0F, FF]");
		assert_eq!(
			format!("{:#X}", bits),
			"[
    0x00,
    0x0F,
    0xFF,
]"
		);
	}
}
