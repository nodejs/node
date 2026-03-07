#![doc=include_str!("../../doc/serdes/slice.md")]

#[cfg(feature = "alloc")]
use alloc::vec::Vec;
use core::{
	any,
	fmt::{
		self,
		Formatter,
	},
	marker::PhantomData,
};

use serde::{
	de::{
		Deserialize,
		Deserializer,
		Error,
		MapAccess,
		SeqAccess,
		Visitor,
	},
	ser::{
		Serialize,
		SerializeStruct,
		Serializer,
	},
};
use wyz::comu::Const;

use super::{
	utils::TypeName,
	Field,
	FIELDS,
};
#[cfg(feature = "alloc")]
use crate::{
	boxed::BitBox,
	vec::BitVec,
};
use crate::{
	index::BitIdx,
	mem::bits_of,
	order::BitOrder,
	ptr::{
		AddressExt,
		BitSpan,
		BitSpanError,
	},
	slice::BitSlice,
	store::BitStore,
};

impl<T, O> Serialize for BitSlice<T, O>
where
	T: BitStore,
	O: BitOrder,
	T::Mem: Serialize,
{
	#[inline]
	fn serialize<S>(&self, serializer: S) -> super::Result<S>
	where S: Serializer {
		let head = self.as_bitspan().head();
		let mut state = serializer.serialize_struct("BitSeq", FIELDS.len())?;

		state.serialize_field("order", &any::type_name::<O>())?;
		state.serialize_field("head", &head)?;
		state.serialize_field("bits", &(self.len() as u64))?;
		state.serialize_field("data", &self.domain())?;

		state.end()
	}
}

#[cfg(feature = "alloc")]
impl<T, O> Serialize for BitBox<T, O>
where
	T: BitStore,
	O: BitOrder,
	BitSlice<T, O>: Serialize,
{
	#[inline]
	fn serialize<S>(&self, serializer: S) -> super::Result<S>
	where S: Serializer {
		self.as_bitslice().serialize(serializer)
	}
}

#[cfg(feature = "alloc")]
impl<T, O> Serialize for BitVec<T, O>
where
	T: BitStore,
	O: BitOrder,
	BitSlice<T, O>: Serialize,
{
	#[inline]
	fn serialize<S>(&self, serializer: S) -> super::Result<S>
	where S: Serializer {
		self.as_bitslice().serialize(serializer)
	}
}

impl<'de, O> Deserialize<'de> for &'de BitSlice<u8, O>
where O: BitOrder
{
	#[inline]
	fn deserialize<D>(deserializer: D) -> Result<Self, D::Error>
	where D: Deserializer<'de> {
		deserializer.deserialize_struct(
			"BitSeq",
			FIELDS,
			BitSeqVisitor::<u8, O, &'de [u8], Self, _>::new(
				|data, head, bits| unsafe {
					BitSpan::new(data.as_ptr().into_address(), head, bits)
						.map(|span| BitSpan::into_bitslice_ref(span))
				},
			),
		)
	}
}

#[cfg(feature = "alloc")]
impl<'de, T, O> Deserialize<'de> for BitBox<T, O>
where
	T: BitStore,
	O: BitOrder,
	Vec<T>: Deserialize<'de>,
{
	#[inline]
	fn deserialize<D>(deserializer: D) -> Result<Self, D::Error>
	where D: Deserializer<'de> {
		<BitVec<T, O> as Deserialize<'de>>::deserialize(deserializer)
			.map(BitVec::into_boxed_bitslice)
	}
}

#[cfg(feature = "alloc")]
impl<'de, T, O> Deserialize<'de> for BitVec<T, O>
where
	T: BitStore,
	O: BitOrder,
	Vec<T>: Deserialize<'de>,
{
	#[inline]
	fn deserialize<D>(deserializer: D) -> Result<Self, D::Error>
	where D: Deserializer<'de> {
		deserializer.deserialize_struct(
			"BitSeq",
			FIELDS,
			BitSeqVisitor::<T, O, Vec<T>, Self, _>::new(
				|vec, head, bits| unsafe {
					let addr = vec.as_ptr().into_address();
					let mut bv = BitVec::try_from_vec(vec).map_err(|_| {
						BitSpan::<Const, T, O>::new(addr, head, bits)
							.unwrap_err()
					})?;
					bv.set_head(head);
					bv.set_len(bits);
					Ok(bv)
				},
			),
		)
	}
}

/// Assists in deserialization of a dynamic `BitSeq`.
struct BitSeqVisitor<T, O, In, Out, Func>
where
	T: BitStore,
	O: BitOrder,
	Func: FnOnce(In, BitIdx<T::Mem>, usize) -> Result<Out, BitSpanError<T>>,
{
	/// As well as a final output value.
	out:   PhantomData<Result<Out, BitSpanError<T>>>,
	/// The deserialized bit-ordering string.
	order: Option<TypeName<O>>,
	/// The deserialized head-bit index.
	head:  Option<BitIdx<T::Mem>>,
	/// The deserialized bit-count.
	bits:  Option<u64>,
	/// The deserialized data buffer.
	data:  Option<In>,
	/// A functor responsible for final transformation of the deserialized
	/// components into the output value.
	func:  Func,
}

impl<'de, T, O, In, Out, Func> BitSeqVisitor<T, O, In, Out, Func>
where
	T: 'de + BitStore,
	O: BitOrder,
	In: Deserialize<'de>,
	Func: FnOnce(In, BitIdx<T::Mem>, usize) -> Result<Out, BitSpanError<T>>,
{
	/// Creates a new visitor with a given transform functor.
	#[inline]
	fn new(func: Func) -> Self {
		Self {
			out: PhantomData,
			order: None,
			head: None,
			bits: None,
			data: None,
			func,
		}
	}

	/// Attempts to assemble deserialized components into an output value.
	#[inline]
	fn assemble<E>(mut self) -> Result<Out, E>
	where E: Error {
		self.order.take().ok_or_else(|| E::missing_field("order"))?;
		let head = self.head.take().ok_or_else(|| E::missing_field("head"))?;
		let bits = self.bits.take().ok_or_else(|| E::missing_field("bits"))?;
		let data = self.data.take().ok_or_else(|| E::missing_field("data"))?;

		(self.func)(data, head, bits as usize).map_err(|_| todo!())
	}
}

impl<'de, T, O, In, Out, Func> Visitor<'de>
	for BitSeqVisitor<T, O, In, Out, Func>
where
	T: 'de + BitStore,
	O: BitOrder,
	In: Deserialize<'de>,
	Func: FnOnce(In, BitIdx<T::Mem>, usize) -> Result<Out, BitSpanError<T>>,
{
	type Value = Out;

	#[inline]
	fn expecting(&self, fmt: &mut Formatter) -> fmt::Result {
		write!(
			fmt,
			"a `BitSlice<u{}, {}>`",
			bits_of::<T::Mem>(),
			any::type_name::<O>(),
		)
	}

	#[inline]
	fn visit_seq<V>(mut self, mut seq: V) -> Result<Self::Value, V::Error>
	where V: SeqAccess<'de> {
		self.order = Some(
			seq.next_element()?
				.ok_or_else(|| <V::Error>::invalid_length(0, &self))?,
		);
		self.head = Some(
			seq.next_element()?
				.ok_or_else(|| <V::Error>::invalid_length(1, &self))?,
		);
		self.bits = Some(
			seq.next_element()?
				.ok_or_else(|| <V::Error>::invalid_length(2, &self))?,
		);
		self.data = Some(
			seq.next_element()?
				.ok_or_else(|| <V::Error>::invalid_length(3, &self))?,
		);

		self.assemble()
	}

	#[inline]
	fn visit_map<V>(mut self, mut map: V) -> Result<Self::Value, V::Error>
	where V: MapAccess<'de> {
		while let Some(key) = map.next_key()? {
			match key {
				Field::Order => {
					if self.order.replace(map.next_value()?).is_some() {
						return Err(<V::Error>::duplicate_field("order"));
					}
				},
				Field::Head => {
					if self.head.replace(map.next_value()?).is_some() {
						return Err(<V::Error>::duplicate_field("head"));
					}
				},
				Field::Bits => {
					if self.bits.replace(map.next_value()?).is_some() {
						return Err(<V::Error>::duplicate_field("bits"));
					}
				},
				Field::Data => {
					if self.data.replace(map.next_value()?).is_some() {
						return Err(<V::Error>::duplicate_field("data"));
					}
				},
			}
		}

		self.assemble()
	}
}

#[cfg(test)]
mod tests {
	#[cfg(all(feature = "alloc", not(feature = "std")))]
	use alloc::format;
	use core::any;

	use serde_test::{
		assert_de_tokens,
		assert_de_tokens_error,
		assert_ser_tokens,
		Token,
	};

	use crate::prelude::*;

	#[test]
	#[cfg(feature = "alloc")]
	fn roundtrip() -> Result<(), alloc::boxed::Box<bincode::ErrorKind>> {
		let bits = bits![u8, Msb0; 1, 0, 1, 1, 0];
		let encoded = bincode::serialize(&bits)?;
		let bits2 = bincode::deserialize::<&BitSlice<u8, Msb0>>(&encoded)?;
		assert_eq!(bits, bits2);
		Ok(())
	}

	#[test]
	fn tokens() {
		let slice = bits![u8, Lsb0; 0, 1, 0, 0, 1];
		let tokens = &mut [
			Token::Struct {
				name: "BitSeq",
				len:  4,
			},
			Token::Str("order"),
			Token::Str(any::type_name::<Lsb0>()),
			Token::Str("head"),
			Token::Struct {
				name: "BitIdx",
				len:  2,
			},
			Token::Str("width"),
			Token::U8(8),
			Token::Str("index"),
			Token::U8(0),
			Token::StructEnd,
			Token::Str("bits"),
			Token::U64(5),
			Token::Str("data"),
			Token::Seq { len: Some(1) },
			Token::U8(18),
			Token::SeqEnd,
			Token::StructEnd,
		];
		assert_ser_tokens(&slice, tokens);
		tokens[8] = Token::U8(1);
		tokens[11] = Token::U64(4);
		assert_ser_tokens(&&slice[1 ..], tokens);

		let tokens = &[
			Token::Seq { len: Some(4) },
			Token::BorrowedStr(any::type_name::<Lsb0>()),
			Token::Seq { len: Some(2) },
			Token::U8(8),
			Token::U8(0),
			Token::SeqEnd,
			Token::U64(5),
			Token::BorrowedBytes(&[18]),
			Token::SeqEnd,
		];
		assert_de_tokens(&slice, tokens);
	}

	#[test]
	#[cfg(feature = "alloc")]
	fn errors() {
		assert_de_tokens_error::<&BitSlice<u8, Msb0>>(
			&[
				Token::Seq { len: Some(4) },
				Token::BorrowedStr(any::type_name::<Lsb0>()),
			],
			&format!(
				"invalid value: string \"{}\", expected the string \"{}\"",
				any::type_name::<Lsb0>(),
				any::type_name::<Msb0>(),
			),
		);

		assert_de_tokens_error::<&BitSlice<u8, Msb0>>(
			&[
				Token::Struct {
					name: "BitSeq",
					len:  1,
				},
				Token::BorrowedStr("unknown"),
			],
			&format!(
				"unknown field `unknown`, expected one of `{}`",
				super::FIELDS.join("`, `"),
			),
		);

		assert_de_tokens_error::<&BitSlice<u8, Msb0>>(
			&[
				Token::Struct {
					name: "BitSeq",
					len:  2,
				},
				Token::BorrowedStr("order"),
				Token::BorrowedStr(any::type_name::<Msb0>()),
				Token::BorrowedStr("order"),
				Token::BorrowedStr(any::type_name::<Msb0>()),
				Token::StructEnd,
			],
			"duplicate field `order`",
		);
		assert_de_tokens_error::<&BitSlice<u8, Msb0>>(
			&[
				Token::Struct {
					name: "BitSeq",
					len:  2,
				},
				Token::BorrowedStr("head"),
				Token::Seq { len: Some(2) },
				Token::U8(8),
				Token::U8(0),
				Token::SeqEnd,
				Token::BorrowedStr("head"),
				Token::Seq { len: Some(2) },
				Token::U8(8),
				Token::U8(0),
				Token::SeqEnd,
				Token::StructEnd,
			],
			"duplicate field `head`",
		);
		assert_de_tokens_error::<&BitSlice<u8, Msb0>>(
			&[
				Token::Struct {
					name: "BitSeq",
					len:  2,
				},
				Token::BorrowedStr("bits"),
				Token::U64(10),
				Token::BorrowedStr("bits"),
				Token::U64(10),
				Token::StructEnd,
			],
			"duplicate field `bits`",
		);
		assert_de_tokens_error::<&BitSlice<u8, Msb0>>(
			&[
				Token::Struct {
					name: "BitSeq",
					len:  2,
				},
				Token::BorrowedStr("data"),
				Token::BorrowedBytes(&[0x3C, 0xA5]),
				Token::BorrowedStr("data"),
				Token::BorrowedBytes(&[0x3C, 0xA5]),
				Token::StructEnd,
			],
			"duplicate field `data`",
		);
	}
}
