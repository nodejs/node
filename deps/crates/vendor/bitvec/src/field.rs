#![doc = include_str!("../doc/field.md")]

use core::{
	mem,
	ptr,
};

use funty::Integral;
use tap::Pipe;
use wyz::comu::{
	Const,
	Mut,
};

use crate::{
	array::BitArray,
	devel as dvl,
	domain::{
		Domain,
		PartialElement,
	},
	mem::bits_of,
	order::{
		BitOrder,
		Lsb0,
		Msb0,
	},
	slice::BitSlice,
	store::BitStore,
	view::BitViewSized,
};
#[cfg(feature = "alloc")]
use crate::{
	boxed::BitBox,
	vec::BitVec,
};

mod io;
mod tests;

#[doc = include_str!("../doc/field/BitField.md")]
pub trait BitField {
	#[inline]
	#[cfg(not(tarpaulin_include))]
	#[doc = include_str!("../doc/field/BitField_load.md")]
	fn load<I>(&self) -> I
	where I: Integral {
		if cfg!(target_endian = "little") {
			self.load_le::<I>()
		}
		else if cfg!(target_endian = "big") {
			self.load_be::<I>()
		}
		else {
			match option_env!("CARGO_PKG_REPOSITORY") {
				Some(env) => unreachable!(
					"This architecture is not supported! Please consider \
					 filing an issue at {}",
					env
				),
				None => unreachable!(
					"This architecture is not supported! Please consider \
					 filing an issue"
				),
			}
		}
	}

	#[inline]
	#[cfg(not(tarpaulin_include))]
	#[doc = include_str!("../doc/field/BitField_store.md")]
	fn store<I>(&mut self, value: I)
	where I: Integral {
		if cfg!(target_endian = "little") {
			self.store_le::<I>(value);
		}
		else if cfg!(target_endian = "big") {
			self.store_be::<I>(value);
		}
		else {
			match option_env!("CARGO_PKG_REPOSITORY") {
				Some(env) => unreachable!(
					"This architecture is not supported! Please consider \
					 filing an issue at {}",
					env
				),
				None => unreachable!(
					"This architecture is not supported! Please consider \
					 filing an issue"
				),
			}
		}
	}

	#[doc = include_str!("../doc/field/BitField_load_le.md")]
	fn load_le<I>(&self) -> I
	where I: Integral;

	#[doc = include_str!("../doc/field/BitField_load_be.md")]
	fn load_be<I>(&self) -> I
	where I: Integral;

	#[doc = include_str!("../doc/field/BitField_store_le.md")]
	fn store_le<I>(&mut self, value: I)
	where I: Integral;

	#[doc = include_str!("../doc/field/BitField_store_be.md")]
	fn store_be<I>(&mut self, value: I)
	where I: Integral;
}

#[doc = include_str!("../doc/field/BitField_Lsb0.md")]
impl<T> BitField for BitSlice<T, Lsb0>
where T: BitStore
{
	#[inline]
	#[doc = include_str!("../doc/field/BitField_Lsb0_load_le.md")]
	fn load_le<I>(&self) -> I
	where I: Integral {
		let len = self.len();
		check::<I>("load", len);

		match self.domain() {
			//  In Lsb0, the head counts distance from LSedge to first live bit.
			Domain::Enclave(elem) => get(elem, elem.head().into_inner()),
			Domain::Region { head, body, tail } => {
				let mut accum = I::ZERO;

				if let Some(elem) = tail {
					accum = get(elem, 0);
				}

				for elem in body.iter().rev().map(BitStore::load_value) {
					maybe_shift_left(&mut accum, bits_of::<T>());
					accum |= resize::<T::Mem, I>(elem);
				}

				if let Some(elem) = head {
					let shamt = elem.head().into_inner();
					maybe_shift_left(
						&mut accum,
						bits_of::<T>() - shamt as usize,
					);
					accum |= get::<_, _, I>(elem, shamt);
				}

				accum
			},
		}
		.pipe(|elem| sign(elem, len))
	}

	#[inline]
	#[doc = include_str!("../doc/field/BitField_Lsb0_load_be.md")]
	fn load_be<I>(&self) -> I
	where I: Integral {
		let len = self.len();
		check::<I>("load", len);

		match self.domain() {
			Domain::Enclave(elem) => get(elem, elem.head().into_inner()),
			Domain::Region { head, body, tail } => {
				let mut accum = I::ZERO;

				if let Some(elem) = head {
					accum = get(elem, elem.head().into_inner());
				}

				for elem in body.iter().map(BitStore::load_value) {
					maybe_shift_left(&mut accum, bits_of::<T>());
					accum |= resize::<T::Mem, I>(elem);
				}

				if let Some(elem) = tail {
					let shamt = elem.tail().into_inner() as usize;
					maybe_shift_left(&mut accum, shamt);
					accum |= get::<_, _, I>(elem, 0);
				}

				accum
			},
		}
		.pipe(|elem| sign(elem, len))
	}

	#[inline]
	#[doc = include_str!("../doc/field/BitField_Lsb0_store_le.md")]
	fn store_le<I>(&mut self, mut value: I)
	where I: Integral {
		check::<I>("store", self.len());

		match self.domain_mut() {
			Domain::Enclave(elem) => {
				let shamt = elem.head().into_inner();
				set(elem, value, shamt);
			},
			Domain::Region { head, body, tail } => {
				if let Some(elem) = head {
					let shamt = elem.head().into_inner();
					set(elem, value, shamt);
					let rshamt = bits_of::<T>() - shamt as usize;
					maybe_shift_right(&mut value, rshamt);
				}

				for elem in body.iter_mut() {
					elem.store_value(resize(value));
					maybe_shift_right(&mut value, bits_of::<T>());
				}

				if let Some(elem) = tail {
					set(elem, value, 0);
				}
			},
		}
	}

	#[inline]
	#[doc = include_str!("../doc/field/BitField_Lsb0_store_be.md")]
	fn store_be<I>(&mut self, mut value: I)
	where I: Integral {
		check::<I>("store", self.len());

		match self.domain_mut() {
			Domain::Enclave(elem) => {
				let shamt = elem.head().into_inner();
				set(elem, value, shamt);
			},
			Domain::Region { head, body, tail } => {
				if let Some(elem) = tail {
					let shamt = elem.tail().into_inner() as usize;
					set(elem, value, 0);
					maybe_shift_right(&mut value, shamt);
				}

				for elem in body.iter_mut().rev() {
					elem.store_value(resize(value));
					maybe_shift_right(&mut value, bits_of::<T>());
				}

				if let Some(elem) = head {
					let shamt = elem.head().into_inner();
					set(elem, value, shamt);
				}
			},
		}
	}
}

#[doc = include_str!("../doc/field/BitField_Msb0.md")]
impl<T> BitField for BitSlice<T, Msb0>
where T: BitStore
{
	#[inline]
	#[doc = include_str!("../doc/field/BitField_Msb0_load_le.md")]
	fn load_le<I>(&self) -> I
	where I: Integral {
		let len = self.len();
		check::<I>("load", len);

		match self.domain() {
			Domain::Enclave(elem) => {
				let shamt = bits_of::<T>() as u8 - elem.tail().into_inner();
				get(elem, shamt)
			},
			Domain::Region { head, body, tail } => {
				let mut accum = I::ZERO;

				if let Some(elem) = tail {
					let shamt = bits_of::<T>() as u8 - elem.tail().into_inner();
					accum = get(elem, shamt);
				}

				for elem in body.iter().rev().map(BitStore::load_value) {
					maybe_shift_left(&mut accum, bits_of::<T>());
					accum |= resize::<T::Mem, I>(elem);
				}

				if let Some(elem) = head {
					let shamt =
						bits_of::<T>() - elem.head().into_inner() as usize;
					maybe_shift_left(&mut accum, shamt);
					accum |= get::<_, _, I>(elem, 0);
				}

				accum
			},
		}
		.pipe(|elem| sign(elem, len))
	}

	#[inline]
	#[doc = include_str!("../doc/field/BitField_Msb0_load_be.md")]
	fn load_be<I>(&self) -> I
	where I: Integral {
		let len = self.len();
		check::<I>("load", len);

		match self.domain() {
			Domain::Enclave(elem) => {
				let shamt = bits_of::<T>() as u8 - elem.tail().into_inner();
				get(elem, shamt)
			},
			Domain::Region { head, body, tail } => {
				let mut accum = I::ZERO;

				if let Some(elem) = head {
					accum = get(elem, 0);
				}

				for elem in body.iter().map(BitStore::load_value) {
					maybe_shift_left(&mut accum, bits_of::<T>());
					accum |= resize::<T::Mem, I>(elem);
				}

				if let Some(elem) = tail {
					let shamt = elem.tail().into_inner();
					maybe_shift_left(&mut accum, shamt as usize);
					accum |= get::<_, _, I>(elem, bits_of::<T>() as u8 - shamt);
				}

				accum
			},
		}
		.pipe(|elem| sign(elem, len))
	}

	#[inline]
	#[doc = include_str!("../doc/field/BitField_Msb0_store_le.md")]
	fn store_le<I>(&mut self, mut value: I)
	where I: Integral {
		check::<I>("store", self.len());

		match self.domain_mut() {
			Domain::Enclave(elem) => {
				let shamt = bits_of::<T>() as u8 - elem.tail().into_inner();
				set(elem, value, shamt);
			},
			Domain::Region { head, body, tail } => {
				if let Some(elem) = head {
					let shamt =
						bits_of::<T>() - elem.head().into_inner() as usize;
					set(elem, value, 0);
					maybe_shift_right(&mut value, shamt);
				}

				for elem in body.iter_mut() {
					elem.store_value(resize(value));
					maybe_shift_right(&mut value, bits_of::<T>());
				}

				if let Some(elem) = tail {
					let shamt = bits_of::<T>() as u8 - elem.tail().into_inner();
					set(elem, value, shamt);
				}
			},
		}
	}

	#[inline]
	#[doc = include_str!("../doc/field/BitField_Msb0_store_be.md")]
	fn store_be<I>(&mut self, mut value: I)
	where I: Integral {
		check::<I>("store", self.len());

		match self.domain_mut() {
			Domain::Enclave(elem) => {
				let shamt = bits_of::<T>() as u8 - elem.tail().into_inner();
				set(elem, value, shamt);
			},
			Domain::Region { head, body, tail } => {
				if let Some(elem) = tail {
					let tail = elem.tail().into_inner() as usize;
					let shamt = bits_of::<T>() - tail;
					set(elem, value, shamt as u8);
					maybe_shift_right(&mut value, tail);
				}

				for elem in body.iter_mut().rev() {
					elem.store_value(resize(value));
					maybe_shift_right(&mut value, bits_of::<T>());
				}

				if let Some(elem) = head {
					set(elem, value, 0);
				}
			},
		}
	}
}

#[doc = include_str!("../doc/field/impl_BitArray.md")]
impl<A, O> BitField for BitArray<A, O>
where
	O: BitOrder,
	A: BitViewSized,
	BitSlice<A::Store, O>: BitField,
{
	#[inline(always)]
	fn load_le<I>(&self) -> I
	where I: Integral {
		let mut accum = I::ZERO;

		for elem in self.as_raw_slice().iter().map(BitStore::load_value).rev() {
			maybe_shift_left(&mut accum, bits_of::<A::Store>());
			accum |= resize::<_, I>(elem);
		}

		sign(accum, self.len())
	}

	#[inline(always)]
	fn load_be<I>(&self) -> I
	where I: Integral {
		let mut accum = I::ZERO;

		for elem in self.as_raw_slice().iter().map(BitStore::load_value) {
			maybe_shift_left(&mut accum, bits_of::<A::Store>());
			accum |= resize::<_, I>(elem);
		}

		sign(accum, self.len())
	}

	#[inline(always)]
	fn store_le<I>(&mut self, mut value: I)
	where I: Integral {
		for slot in self.as_raw_mut_slice() {
			slot.store_value(resize(value));
			maybe_shift_right(&mut value, bits_of::<A::Store>());
		}
	}

	#[inline(always)]
	fn store_be<I>(&mut self, mut value: I)
	where I: Integral {
		for slot in self.as_raw_mut_slice().iter_mut().rev() {
			slot.store_value(resize(value));
			maybe_shift_right(&mut value, bits_of::<A::Store>());
		}
	}
}

#[cfg(feature = "alloc")]
#[cfg(not(tarpaulin_include))]
impl<T, O> BitField for BitBox<T, O>
where
	T: BitStore,
	O: BitOrder,
	BitSlice<T, O>: BitField,
{
	#[inline(always)]
	fn load_le<I>(&self) -> I
	where I: Integral {
		self.as_bitslice().load_le()
	}

	#[inline(always)]
	fn load_be<I>(&self) -> I
	where I: Integral {
		self.as_bitslice().load_be()
	}

	#[inline(always)]
	fn store_le<I>(&mut self, value: I)
	where I: Integral {
		self.as_mut_bitslice().store_le(value)
	}

	#[inline(always)]
	fn store_be<I>(&mut self, value: I)
	where I: Integral {
		self.as_mut_bitslice().store_be(value)
	}
}

#[cfg(feature = "alloc")]
#[cfg(not(tarpaulin_include))]
impl<T, O> BitField for BitVec<T, O>
where
	T: BitStore,
	O: BitOrder,
	BitSlice<T, O>: BitField,
{
	#[inline(always)]
	fn load_le<I>(&self) -> I
	where I: Integral {
		self.as_bitslice().load_le()
	}

	#[inline(always)]
	fn load_be<I>(&self) -> I
	where I: Integral {
		self.as_bitslice().load_be()
	}

	#[inline(always)]
	fn store_le<I>(&mut self, value: I)
	where I: Integral {
		self.as_mut_bitslice().store_le(value)
	}

	#[inline(always)]
	fn store_be<I>(&mut self, value: I)
	where I: Integral {
		self.as_mut_bitslice().store_be(value)
	}
}

/** Asserts that a bit-slice is not longer than a memory element.

## Type Parameters

- `I`: The integer type being stored into or loaded out of a bit-slice.

## Parameters

- `action`: the verb being performed. One of `"load"` or `"store"`.
- `len`: the length of the bit-slice under test.

## Panics

This panics if `len` is not in `1 ..= U::BITS`.
**/
fn check<I>(action: &'static str, len: usize)
where I: Integral {
	assert!(
		(1 ..= bits_of::<I>()).contains(&len),
		"cannot {} {} bits from a {}-bit region",
		action,
		bits_of::<I>(),
		len,
	);
}

/// Shifts a value to the left, if it can support the shift amount.
fn maybe_shift_left<T: Integral>(elem: &mut T, shamt: usize) {
	if bits_of::<T>() > shamt {
		*elem <<= shamt;
	}
}

/// Shifts a value to the right, if it can support the shift amount.
fn maybe_shift_right<T: Integral>(elem: &mut T, shamt: usize) {
	if bits_of::<T>() > shamt {
		*elem >>= shamt;
	}
}

#[doc = include_str!("../doc/field/get.md")]
fn get<T, O, I>(elem: PartialElement<Const, T, O>, shamt: u8) -> I
where
	T: BitStore,
	O: BitOrder,
	I: Integral,
{
	resize::<T::Mem, I>(elem.load_value() >> shamt)
}

#[doc = include_str!("../doc/field/set.md")]
fn set<T, O, I>(mut elem: PartialElement<Mut, T, O>, value: I, shamt: u8)
where
	T: BitStore,
	O: BitOrder,
	I: Integral,
{
	elem.store_value(resize::<I, T::Mem>(value) << shamt);
}

#[doc = include_str!("../doc/field/sign.md")]
fn sign<I>(elem: I, width: usize) -> I
where I: Integral {
	if dvl::is_unsigned::<I>() {
		return elem;
	}
	//  Find the number of high bits that are not loaded.
	let shamt = bits_of::<I>() - width;
	//  Shift left, so that the highest loaded bit is now in the sign position.
	let shl: I = elem << shamt;
	//  Shift right with sign extension back to the original place.
	shl >> shamt
}

#[doc = include_str!("../doc/field/resize.md")]
fn resize<T, U>(value: T) -> U
where
	T: Integral,
	U: Integral,
{
	let mut out = U::ZERO;
	let size_t = mem::size_of::<T>();
	let size_u = mem::size_of::<U>();

	unsafe {
		resize_inner::<T, U>(&value, &mut out, size_t, size_u);
	}

	out
}

/// Performs little-endian byte-order register resizing.
#[cfg(target_endian = "little")]
unsafe fn resize_inner<T, U>(
	src: &T,
	dst: &mut U,
	size_t: usize,
	size_u: usize,
) {
	//  In LE, the least-significant byte is the base address, so resizing is
	//  just a `memmove` into a zeroed slot, taking only the lesser width.
	ptr::copy_nonoverlapping(
		src as *const T as *const u8,
		dst as *mut U as *mut u8,
		size_t.min(size_u),
	);
}

/// Performs big-endian byte-order register resizing.
#[cfg(target_endian = "big")]
unsafe fn resize_inner<T, U>(
	src: &T,
	dst: &mut U,
	size_t: usize,
	size_u: usize,
) {
	let src = src as *const T as *const u8;
	let dst = dst as *mut U as *mut u8;

	//  In BE, shrinking a value requires moving the source base-pointer up in
	//  memory (to a higher address, lower significance),
	if size_t > size_u {
		ptr::copy_nonoverlapping(src.add(size_t - size_u), dst, size_u);
	}
	//  While expanding a value requires moving the *destination* base-pointer
	//  up (and leaving the lower address, higher significance bytes zeroed).
	else {
		ptr::copy_nonoverlapping(src, dst.add(size_u - size_t), size_t);
	}
}
