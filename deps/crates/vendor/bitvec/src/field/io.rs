#![cfg(feature = "std")]
#![doc = include_str!("../../doc/field/io.md")]

use core::mem;
use std::io::{
	self,
	Read,
	Write,
};

use super::BitField;
use crate::{
	mem::bits_of,
	order::BitOrder,
	slice::BitSlice,
	store::BitStore,
	vec::BitVec,
};

#[doc = include_str!("../../doc/field/io/Read_BitSlice.md")]
impl<T, O> Read for &BitSlice<T, O>
where
	T: BitStore,
	O: BitOrder,
	BitSlice<T, O>: BitField,
{
	#[inline]
	fn read(&mut self, buf: &mut [u8]) -> io::Result<usize> {
		let mut count = 0;
		self.chunks_exact(bits_of::<u8>())
			.zip(buf.iter_mut())
			.for_each(|(byte, slot)| {
				*slot = byte.load_be();
				count += 1;
			});
		*self = unsafe { self.get_unchecked(count * bits_of::<u8>() ..) };
		Ok(count)
	}
}

#[doc = include_str!("../../doc/field/io/Write_BitSlice.md")]
impl<T, O> Write for &mut BitSlice<T, O>
where
	T: BitStore,
	O: BitOrder,
	BitSlice<T, O>: BitField,
{
	#[inline]
	fn write(&mut self, buf: &[u8]) -> io::Result<usize> {
		let mut count = 0;
		unsafe { self.chunks_exact_mut(bits_of::<u8>()).remove_alias() }
			.zip(buf.iter().copied())
			.for_each(|(slot, byte)| {
				slot.store_be(byte);
				count += 1;
			});
		*self = unsafe {
			mem::take(self).get_unchecked_mut(count * bits_of::<u8>() ..)
		};
		Ok(count)
	}

	#[inline]
	#[cfg(not(tarpaulin_include))]
	fn flush(&mut self) -> io::Result<()> {
		Ok(())
	}
}

#[doc = include_str!("../../doc/field/io/Read_BitVec.md")]
impl<T, O> Read for BitVec<T, O>
where
	T: BitStore,
	O: BitOrder,
	BitSlice<T, O>: BitField,
{
	#[inline]
	fn read(&mut self, buf: &mut [u8]) -> io::Result<usize> {
		let bytes_read = self.as_bitslice().read(buf)?;
		let bits = bytes_read * bits_of::<u8>();
		self.shift_left(bits);
		self.truncate(self.len() - bits);
		Ok(bytes_read)
	}
}

#[doc = include_str!("../../doc/field/io/Write_BitVec.md")]
impl<T, O> Write for BitVec<T, O>
where
	O: BitOrder,
	T: BitStore,
	BitSlice<T, O>: BitField,
{
	#[inline]
	fn write(&mut self, buf: &[u8]) -> io::Result<usize> {
		let len = self.len();
		self.resize(len + buf.len() * bits_of::<u8>(), false);
		unsafe { self.get_unchecked_mut(len ..) }.write(buf)
	}

	#[inline]
	#[cfg(not(tarpaulin_include))]
	fn flush(&mut self) -> io::Result<()> {
		Ok(())
	}
}
