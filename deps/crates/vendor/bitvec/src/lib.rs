#![doc = include_str!("../README.md")]
#![cfg_attr(not(feature = "std"), no_std)]
#![cfg_attr(
	debug_assertions,
	warn(missing_docs, clippy::missing_docs_in_private_items)
)]
#![cfg_attr(
	not(debug_assertions),
	deny(missing_docs, clippy::missing_docs_in_private_items)
)]
#![deny(unconditional_recursion)]
#![allow(
	clippy::declare_interior_mutable_const,
	clippy::type_complexity,
	unknown_lints
)]

#[cfg(feature = "alloc")]
extern crate alloc;

#[macro_use]
mod devel;

#[macro_use]
pub mod macros;

pub mod access;
pub mod array;
pub mod boxed;
pub mod domain;
pub mod field;
pub mod index;
pub mod mem;
pub mod order;
pub mod ptr;
mod serdes;
pub mod slice;
pub mod store;
pub mod vec;
pub mod view;

#[doc = include_str!("../doc/prelude.md")]
pub mod prelude {
	pub use crate::{
		array::BitArray,
		bitarr,
		bits,
		field::BitField as _,
		order::{
			BitOrder,
			LocalBits,
			Lsb0,
			Msb0,
		},
		ptr::{
			BitPtr,
			BitPtrRange,
			BitRef,
		},
		slice::BitSlice,
		store::BitStore,
		view::{
			AsBits,
			AsMutBits,
			BitView as _,
			BitViewSized as _,
		},
		BitArr,
	};
	#[cfg(feature = "alloc")]
	pub use crate::{
		bitbox,
		bitvec,
		boxed::BitBox,
		vec::BitVec,
	};
}
