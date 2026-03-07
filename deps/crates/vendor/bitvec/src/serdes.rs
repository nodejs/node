#![cfg(feature = "serde")]
#![doc = include_str!("../doc/serdes.md")]

mod array;
mod slice;
mod utils;

use core::fmt::{
	self,
	Formatter,
};

use serde::de::{
	Deserialize,
	Deserializer,
	Visitor,
};

/// A result of serialization.
type Result<S> = core::result::Result<
	<S as serde::Serializer>::Ok,
	<S as serde::Serializer>::Error,
>;

/// A list of fields in the `BitSeq` and `BitArr` transport format.
static FIELDS: &[&str] = &["order", "head", "bits", "data"];

/// The components of a bit-slice in wire format.
enum Field {
	/// Denotes the `<O: BitOrder>` type parameter.
	Order,
	/// Denotes the head-bit index in the first `Data` element.
	Head,
	/// Denotes the count of all live bits in the `Data` sequence.
	Bits,
	/// Denotes the raw storage sequence.
	Data,
}

/// Visits field tokens without attempting to deserialize into real data.
struct FieldVisitor;

impl<'de> Deserialize<'de> for Field {
	fn deserialize<D>(deserializer: D) -> core::result::Result<Self, D::Error>
	where D: Deserializer<'de> {
		deserializer.deserialize_identifier(FieldVisitor)
	}
}

impl<'de> Visitor<'de> for FieldVisitor {
	type Value = Field;

	fn expecting(&self, fmt: &mut Formatter) -> fmt::Result {
		fmt.write_str("field_identifier")
	}

	fn visit_str<E>(self, value: &str) -> core::result::Result<Self::Value, E>
	where E: serde::de::Error {
		match value {
			"order" => Ok(Field::Order),
			"head" => Ok(Field::Head),
			"bits" => Ok(Field::Bits),
			"data" => Ok(Field::Data),
			_ => Err(serde::de::Error::unknown_field(value, FIELDS)),
		}
	}
}

#[cfg(test)]
mod tests {
	use serde::{
		Deserialize,
		Serialize,
	};
	use static_assertions::*;

	use crate::prelude::*;

	#[test]
	fn trait_impls() {
		use core::{
			cell::Cell,
			sync::atomic::*,
		};

		use radium::types::*;
		macro_rules! check_impl {
			($($ord:ident @ $($sto:ty),+);+ $(;)?) => {{ $( $(
				assert_impl_all!(BitSlice<$sto, $ord>: Serialize);
				assert_impl_all!(BitArray<$sto, $ord>: Serialize, Deserialize<'static>);
				assert_impl_all!(BitArray<[$sto; 32], $ord>: Serialize, Deserialize<'static>);

				#[cfg(feature = "alloc")] {
					assert_impl_all!(BitBox<$sto, $ord>: Serialize, Deserialize<'static>);
					assert_impl_all!(BitVec<$sto, $ord>: Serialize, Deserialize<'static>);
				}
			)+ )+ }};
		}

		assert_impl_all!(&BitSlice<u8, Lsb0>: Deserialize<'static>);
		assert_impl_all!(&BitSlice<u8, Msb0>: Deserialize<'static>);
		assert_impl_all!(&BitSlice<u8, LocalBits>: Deserialize<'static>);

		check_impl! {
			Lsb0 @ u8, u16, u32, usize;
			Msb0 @ u8, u16, u32, usize;
			LocalBits @ u8, u16, u32, usize;
			Lsb0 @ Cell<u8>, Cell<u16>, Cell<u32>, Cell<usize>;
			Msb0 @ Cell<u8>, Cell<u16>, Cell<u32>, Cell<usize>;
			LocalBits @ Cell<u8>, Cell<u16>, Cell<u32>, Cell<usize>;
			Lsb0 @ RadiumU8, RadiumU16, RadiumU32, RadiumUsize;
			Msb0 @ RadiumU8, RadiumU16, RadiumU32, RadiumUsize;
			LocalBits @ RadiumU8, RadiumU16, RadiumU32, RadiumUsize;
		}
		radium::if_atomic! {
			if atomic(8) {
				check_impl! {
					Lsb0 @ AtomicU8;
					Msb0 @ AtomicU8;
					LocalBits @ AtomicU8;
				}
			}
			if atomic(16) {
				check_impl! {
					Lsb0 @ AtomicU16;
					Msb0 @ AtomicU16;
					LocalBits @ AtomicU16;
				}
			}
			if atomic(32) {
				check_impl! {
					Lsb0 @ AtomicU32;
					Msb0 @ AtomicU32;
					LocalBits @ AtomicU32;
				}
			}
			if atomic(ptr) {
				check_impl! {
					Lsb0 @ AtomicUsize;
					Msb0 @ AtomicUsize;
					LocalBits @ AtomicUsize;
				}
			}
		}
		#[cfg(target_pointer_width = "64")]
		check_impl! {
			Lsb0 @ u64, RadiumU64;
			Msb0 @ u64, RadiumU64;
			LocalBits @ u64, RadiumU64;
		}
		#[cfg(target_pointer_width = "64")]
		radium::if_atomic!(if atomic(64) {
			check_impl! {
				Lsb0 @ AtomicU64;
				Msb0 @ AtomicU64;
				LocalBits @ AtomicU64;
			}
		});
	}
}
