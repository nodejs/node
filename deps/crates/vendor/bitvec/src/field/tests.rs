#![cfg(test)]

#[cfg(feature = "std")]
use std::io;

use rand::prelude::*;

use crate::prelude::*;

#[test]
fn lsb0_u8_any_u5() {
	let mut bits = BitArray::<u8, Lsb0>::ZERO;

	let val = random::<u8>() & 0x1Fu8;
	bits[2 .. 7].store_le(val);
	assert_eq!(
		bits.as_raw_slice()[0],
		val << 2,
		"{:08b} != {:08b}",
		bits.as_raw_slice()[0],
		val << 2,
	);
	assert_eq!(bits[2 .. 7].load_le::<u8>(), val);

	let neg = val | 0xF0;
	bits[2 .. 7].store_le(neg);
	assert_eq!(bits[2 .. 7].load_le::<i8>(), neg as i8);

	let val = random::<u8>() & 0x1Fu8;
	bits[2 .. 7].store_be(val);
	assert_eq!(
		bits.as_raw_slice()[0],
		val << 2,
		"{:08b} != {:08b}",
		bits.as_raw_slice()[0],
		val << 2,
	);
	assert_eq!(bits[2 .. 7].load_be::<u8>(), val);

	let neg = val | 0xF0;
	bits[2 .. 7].store_be(neg);
	assert_eq!(bits[2 .. 7].load_be::<i8>(), neg as i8);
}

#[test]
fn lsb0_u8_le_u20() {
	let mut bits = BitArray::<[u8; 3], Lsb0>::ZERO;

	let val = random::<u32>() & 0x00_0F_FF_FFu32;
	let bytes = (val << 2).to_le_bytes();
	bits[2 .. 22].store_le(val);
	assert_eq!(bits.as_raw_slice(), &bytes[.. 3]);
	assert_eq!(bits[2 .. 22].load_le::<u32>(), val);

	let neg = val | 0xFF_F8_00_00u32;
	bits[2 .. 22].store_le(neg);
	assert_eq!(
		bits[2 .. 22].load_le::<i32>(),
		neg as i32,
		"{:08x} != {:08x}",
		bits[2 .. 22].load_le::<i32>(),
		neg as i32,
	);
}

#[test]
fn lsb0_u8_be_u20() {
	let mut bits = BitArray::<[u8; 3], Lsb0>::ZERO;

	let val = random::<u32>() & 0x00_0F_FF_FFu32;
	let mut bytes = (val << 2).to_be_bytes();
	// Lsb0 _be has *weird* effects in raw memory.
	bytes[1] <<= 2;
	bytes[3] >>= 2;
	bits[2 .. 22].store_be(val);
	assert_eq!(bits.as_raw_slice(), &bytes[1 ..]);
	assert_eq!(bits[2 .. 22].load_be::<u32>(), val);

	let neg = val | 0xFF_F8_00_00u32;
	bits[2 .. 22].store_be(neg);
	assert_eq!(
		bits[2 .. 22].load_be::<i32>(),
		neg as i32,
		"{:08x} != {:08x}",
		bits[2 .. 22].load_le::<i32>(),
		neg as i32,
	);
}

#[test]
fn msb0_u8_any_u5() {
	let mut bits = BitArray::<u8, Msb0>::ZERO;

	let val = random::<u8>() & 0x1Fu8;
	bits[2 .. 7].store_le(val);
	assert_eq!(
		bits.as_raw_slice()[0],
		val << 1,
		"{:08b} != {:08b}",
		bits.as_raw_slice()[0],
		val << 1,
	);
	assert_eq!(bits[2 .. 7].load_le::<u8>(), val);

	let neg = val | 0xF0;
	bits[2 .. 7].store_le(neg);
	assert_eq!(bits[2 .. 7].load_le::<i8>(), neg as i8);

	let val = random::<u8>() & 0x1Fu8;
	bits[2 .. 7].store_be(val);
	assert_eq!(
		bits.as_raw_slice()[0],
		val << 1,
		"{:08b} != {:08b}",
		bits.as_raw_slice()[0],
		val << 1,
	);
	assert_eq!(bits[2 .. 7].load_be::<u8>(), val);

	let neg = val | 0xF0;
	bits[2 .. 7].store_be(neg);
	assert_eq!(bits[2 .. 7].load_be::<i8>(), neg as i8);
}

#[test]
fn msb0_u8_le_u20() {
	let mut bits = BitArray::<[u8; 3], Msb0>::ZERO;

	let val = random::<u32>() & 0x00_0F_FF_FFu32;
	let mut bytes = (val << 2).to_le_bytes();
	// Msb0 _le has *weird* effects in raw memory.
	bytes[0] >>= 2;
	bytes[2] <<= 2;
	bits[2 .. 22].store_le(val);
	assert_eq!(bits.as_raw_slice(), &bytes[.. 3]);
	assert_eq!(bits[2 .. 22].load_le::<u32>(), val);

	let neg = val | 0xFF_F8_00_00u32;
	bits[2 .. 22].store_le(neg);
	assert_eq!(
		bits[2 .. 22].load_le::<i32>(),
		neg as i32,
		"{:08x} != {:08x}",
		bits[2 .. 22].load_le::<i32>(),
		neg as i32,
	);
}

#[test]
fn msb0_u8_be_u20() {
	let mut bits = BitArray::<[u8; 3], Msb0>::ZERO;

	let val = random::<u32>() & 0x00_0F_FF_FFu32;
	let bytes = (val << 2).to_be_bytes();
	bits[2 .. 22].store_be(val);
	assert_eq!(bits.as_raw_slice(), &bytes[1 ..]);
	assert_eq!(bits[2 .. 22].load_be::<u32>(), val);

	let neg = val | 0xFF_F8_00_00u32;
	bits[2 .. 22].store_be(neg);
	assert_eq!(
		bits[2 .. 22].load_be::<i32>(),
		neg as i32,
		"{:08x} != {:08x}",
		bits[2 .. 22].load_le::<i32>(),
		neg as i32,
	);
}

#[test]
fn lsb0_u8_le_u24() {
	let mut bits = BitArray::<[u8; 3], Lsb0>::ZERO;

	let val = random::<u32>() & 0x00_FF_FF_FFu32;
	let bytes = val.to_le_bytes();
	bits.store_le(val);
	assert_eq!(bits.as_raw_slice(), &bytes[.. 3]);
	assert_eq!(
		bits.load_le::<u32>(),
		val,
		"{:08x} != {:08x}",
		bits.load_le::<i32>(),
		val,
	);

	let neg = val | 0xFF_80_00_00u32;
	bits.store_le(neg);
	assert_eq!(
		bits.load_le::<i32>(),
		neg as i32,
		"{:08x} != {:08x}",
		bits.load_le::<i32>(),
		neg as i32,
	);
}

#[test]
fn lsb0_u8_be_u24() {
	let mut bits = BitArray::<[u8; 3], Lsb0>::ZERO;

	let val = random::<u32>() & 0x00_FF_FF_FFu32;
	let bytes = val.to_be_bytes();
	bits.store_be(val);
	assert_eq!(bits.as_raw_slice(), &bytes[1 ..]);
	assert_eq!(bits.load_be::<u32>(), val);

	let neg = val | 0xFF_80_00_00u32;
	bits.store_be(neg);
	assert_eq!(
		bits.load_be::<i32>(),
		neg as i32,
		"{:08x} != {:08x}",
		bits.load_be::<i32>(),
		neg as i32,
	);
}

#[test]
fn msb0_u8_le_u24() {
	let mut bits = BitArray::<[u8; 3], Msb0>::ZERO;

	let val = random::<u32>() & 0x00_FF_FF_FFu32;
	let bytes = val.to_le_bytes();
	bits.store_le(val);
	assert_eq!(bits.as_raw_slice(), &bytes[.. 3]);
	assert_eq!(bits.load_le::<u32>(), val);

	let neg = val | 0xFF_80_00_00u32;
	bits.store_le(neg);
	assert_eq!(
		bits.load_le::<i32>(),
		neg as i32,
		"{:08x} != {:08x}",
		bits.load_le::<i32>(),
		neg as i32,
	);
}

#[test]
fn msb0_u8_be_u24() {
	let mut bits = BitArray::<[u8; 3], Msb0>::ZERO;

	let val = random::<u32>() & 0x00_FF_FF_FFu32;
	let bytes = val.to_be_bytes();
	bits.store_be(val);
	assert_eq!(bits.as_raw_slice(), &bytes[1 ..]);
	assert_eq!(bits.load_be::<u32>(), val);

	let neg = val | 0xFF_80_00_00u32;
	bits.store_be(neg);
	assert_eq!(
		bits.load_be::<i32>(),
		neg as i32,
		"{:08x} != {:08x}",
		bits.load_be::<i32>(),
		neg as i32,
	);
}

#[test]
#[cfg(feature = "std")]
fn read_bits() {
	let data = [0x136Cu16, 0x8C63];
	let base = data.view_bits::<Msb0>().as_bitptr();
	let mut bits = &data.view_bits::<Msb0>()[4 ..];

	assert_eq!(unsafe { bits.as_bitptr().offset_from(base) }, 4);
	assert_eq!(bits.len(), 28);

	let mut transfer = [0u8; 4];
	let last_ptr = &mut transfer[3] as *mut _;
	let mut transfer_handle = &mut transfer[..];

	assert_eq!(io::copy(&mut bits, &mut transfer_handle).unwrap(), 3);
	assert_eq!(unsafe { bits.as_bitptr().offset_from(base) }, 28);
	assert_eq!(transfer_handle.as_mut_ptr() as *mut _, last_ptr);
	assert_eq!(transfer[.. 3], [0x36, 0xC8, 0xC6][..]);

	let mut bv = data.view_bits::<Msb0>()[4 ..].to_bitvec();
	let mut transfer = [0u8; 3];
	assert_eq!(io::copy(&mut bv, &mut &mut transfer[..]).unwrap(), 3);
	assert_eq!(bv, bits![0, 0, 1, 1]);
	assert_eq!(transfer, [0x36, 0xC8, 0xC6]);
}

#[test]
#[cfg(feature = "std")]
fn write_bits() {
	let mut bv = bitvec![usize, Msb0; 0; 4];
	assert_eq!(
		io::copy(&mut &[0xC3u8, 0xF0, 0x69][..], &mut bv).unwrap(),
		3,
	);

	assert_eq!(bv, bits![
		0, 0, 0, 0, // original
		1, 1, 0, 0, 0, 0, 1, 1, // byte 0
		1, 1, 1, 1, 0, 0, 0, 0, // byte 1
		0, 1, 1, 0, 1, 0, 0, 1, // byte 2
	]);

	let mut data = [0u8; 4];
	let base = data.view_bits_mut::<Lsb0>().as_mut_bitptr();
	let mut bits = &mut data.view_bits_mut::<Lsb0>()[4 ..];
	assert_eq!(unsafe { bits.as_mut_bitptr().offset_from(base) }, 4);
	assert_eq!(bits.len(), 28);
	assert_eq!(
		io::copy(&mut &[0xA5u8, 0xB4, 0x3C][..], &mut bits).unwrap(),
		3,
	);
	assert_eq!(unsafe { bits.as_mut_bitptr().offset_from(base) }, 28);
	assert_eq!(bits.len(), 4);

	assert_eq!(data, [0b1010_0000, 0b1011_0101, 0b0011_0100, 0b0000_1100]);
}
