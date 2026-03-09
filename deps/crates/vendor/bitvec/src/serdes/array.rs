#![doc=include_str!("../../doc/serdes/array.md")]

use core::{
	any,
	fmt::{
		self,
		Formatter,
	},
};

use serde::{
	de::{
		Deserialize,
		Deserializer,
		Error,
		MapAccess,
		SeqAccess,
		Unexpected,
		Visitor,
	},
	ser::{
		Serialize,
		SerializeStruct,
		Serializer,
	},
};

use super::{
	utils::{
		Array,
		TypeName,
	},
	Field,
	FIELDS,
};
use crate::{
	array::BitArray,
	index::BitIdx,
	mem::bits_of,
	order::BitOrder,
	store::BitStore,
};

impl<T, O> Serialize for BitArray<T, O>
where
	T: BitStore,
	O: BitOrder,
	T::Mem: Serialize,
{
	#[inline]
	fn serialize<S>(&self, serializer: S) -> super::Result<S>
	where S: Serializer {
		let mut state = serializer.serialize_struct("BitArr", FIELDS.len())?;

		state.serialize_field("order", &any::type_name::<O>())?;
		state.serialize_field("head", &BitIdx::<T::Mem>::MIN)?;
		state.serialize_field("bits", &(self.len() as u64))?;
		state.serialize_field(
			"data",
			Array::from_ref(core::array::from_ref(&self.data)),
		)?;

		state.end()
	}
}

impl<T, O, const N: usize> Serialize for BitArray<[T; N], O>
where
	T: BitStore,
	O: BitOrder,
	T::Mem: Serialize,
{
	#[inline]
	fn serialize<S>(&self, serializer: S) -> super::Result<S>
	where S: Serializer {
		let mut state = serializer.serialize_struct("BitArr", FIELDS.len())?;

		state.serialize_field("order", &any::type_name::<O>())?;
		state.serialize_field("head", &BitIdx::<T::Mem>::MIN)?;
		state.serialize_field("bits", &(self.len() as u64))?;
		state.serialize_field("data", Array::from_ref(&self.data))?;

		state.end()
	}
}

impl<'de, T, O> Deserialize<'de> for BitArray<T, O>
where
	T: BitStore,
	O: BitOrder,
	T::Mem: Deserialize<'de>,
{
	#[inline]
	fn deserialize<D>(deserializer: D) -> Result<Self, D::Error>
	where D: Deserializer<'de> {
		deserializer
			.deserialize_struct("BitArr", FIELDS, BitArrVisitor::<T, O, 1>::THIS)
			.map(|BitArray { data: [elem], .. }| BitArray::new(elem))
	}
}

impl<'de, T, O, const N: usize> Deserialize<'de> for BitArray<[T; N], O>
where
	T: BitStore,
	O: BitOrder,
	T::Mem: Deserialize<'de>,
{
	#[inline]
	fn deserialize<D>(deserializer: D) -> Result<Self, D::Error>
	where D: Deserializer<'de> {
		deserializer.deserialize_struct(
			"BitArr",
			FIELDS,
			BitArrVisitor::<T, O, N>::THIS,
		)
	}
}

/// Assists in deserialization of a static `BitArr`.
struct BitArrVisitor<T, O, const N: usize>
where
	T: BitStore,
	O: BitOrder,
{
	/// The deserialized bit-ordering string.
	order: Option<TypeName<O>>,
	/// The deserialized head-bit index. This must be zero; it is used for
	/// consistency with `BitSeq` and to carry `T::Mem` information.
	head:  Option<BitIdx<T::Mem>>,
	/// The deserialized bit-count. It must be `bits_of::<[T::Mem; N]>()`.
	bits:  Option<u64>,
	/// The deserialized data buffer.
	data:  Option<Array<T, N>>,
}

impl<'de, T, O, const N: usize> BitArrVisitor<T, O, N>
where
	T: BitStore,
	O: BitOrder,
	Array<T, N>: Deserialize<'de>,
{
	/// A new visitor in its ready condition.
	const THIS: Self = Self {
		order: None,
		head:  None,
		bits:  None,
		data:  None,
	};

	/// Attempts to assemble deserialized components into an output value.
	#[inline]
	fn assemble<E>(mut self) -> Result<BitArray<[T; N], O>, E>
	where E: Error {
		self.order.take().ok_or_else(|| E::missing_field("order"))?;
		let head = self.head.take().ok_or_else(|| E::missing_field("head"))?;
		let bits = self.bits.take().ok_or_else(|| E::missing_field("bits"))?;
		let data = self.data.take().ok_or_else(|| E::missing_field("data"))?;

		if head != BitIdx::MIN {
			return Err(E::invalid_value(
				Unexpected::Unsigned(head.into_inner() as u64),
				&"`BitArray` must have a head-bit of `0`",
			));
		}
		let bits = bits as usize;
		if bits != bits_of::<[T; N]>() {
			return Err(E::invalid_length(bits, &self));
		}

		Ok(BitArray::new(data.inner))
	}
}

impl<'de, T, O, const N: usize> Visitor<'de> for BitArrVisitor<T, O, N>
where
	T: BitStore,
	O: BitOrder,
	Array<T, N>: Deserialize<'de>,
{
	type Value = BitArray<[T; N], O>;

	#[inline]
	fn expecting(&self, fmt: &mut Formatter) -> fmt::Result {
		write!(
			fmt,
			"a `BitArray<[u{}; {}], {}>`",
			bits_of::<T::Mem>(),
			N,
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
	#[cfg(feature = "std")]
	fn roundtrip() -> Result<(), Box<dyn std::error::Error>> {
		type BA = BitArr!(for 16, in u8, Msb0);
		let array = [0x3Cu8, 0xA5].into_bitarray::<Msb0>();

		let bytes = bincode::serialize(&array)?;
		let array2 = bincode::deserialize::<BA>(&bytes)?;
		assert_eq!(array, array2);

		let json = serde_json::to_string(&array)?;
		let array3 = serde_json::from_str::<BA>(&json)?;
		assert_eq!(array, array3);

		let json_value = serde_json::to_value(&array)?;
		let array4 = serde_json::from_value::<BA>(json_value)?;
		assert_eq!(array, array4);

		type BA2 = BitArray<u16, Msb0>;
		let array = BA2::new(44203);

		let bytes = bincode::serialize(&array)?;
		let array2 = bincode::deserialize::<BA2>(&bytes)?;
		assert_eq!(array, array2);

		let json = serde_json::to_string(&array)?;
		let array3 = serde_json::from_str::<BA2>(&json)?;
		assert_eq!(array, array3);

		let json_value = serde_json::to_value(&array)?;
		let array4 = serde_json::from_value::<BA2>(json_value)?;
		assert_eq!(array, array4);

		Ok(())
	}

	#[test]
	fn tokens() {
		let array = [0x3Cu8, 0xA5].into_bitarray::<Msb0>();
		let tokens = &mut [
			Token::Struct {
				name: "BitArr",
				len:  4,
			},
			Token::Str("order"),
			Token::Str(any::type_name::<Msb0>()),
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
			Token::U64(16),
			Token::Str("data"),
			Token::Tuple { len: 2 },
			Token::U8(0x3C),
			Token::U8(0xA5),
			Token::TupleEnd,
			Token::StructEnd,
		];

		assert_ser_tokens(&array, tokens);

		tokens[1 .. 4].copy_from_slice(&[
			Token::BorrowedStr("order"),
			Token::BorrowedStr(any::type_name::<Msb0>()),
			Token::BorrowedStr("head"),
		]);
		tokens[5] = Token::BorrowedStr("width");
		tokens[7] = Token::BorrowedStr("index");
		tokens[10] = Token::BorrowedStr("bits");
		tokens[12] = Token::BorrowedStr("data");
		assert_de_tokens(&array, tokens);
	}

	#[test]
	#[cfg(feature = "alloc")]
	fn errors() {
		type BA = BitArr!(for 8, in u8, Msb0);
		let mut tokens = vec![
			Token::Seq { len: Some(4) },
			Token::BorrowedStr(any::type_name::<Msb0>()),
		];

		assert_de_tokens_error::<BitArr!(for 8, in u8, Lsb0)>(
			&tokens,
			&format!(
				"invalid value: string \"{}\", expected the string \"{}\"",
				any::type_name::<Msb0>(),
				any::type_name::<Lsb0>(),
			),
		);

		tokens.extend([
			Token::Seq { len: Some(2) },
			Token::U8(8),
			Token::U8(0),
			Token::SeqEnd,
			Token::U64(8),
			Token::Tuple { len: 1 },
			Token::U8(0),
			Token::TupleEnd,
			Token::SeqEnd,
		]);

		tokens[6] = Token::U64(7);
		assert_de_tokens_error::<BA>(
			&tokens,
			"invalid length 7, expected a `BitArray<[u8; 1], \
			 bitvec::order::Msb0>`",
		);

		tokens[4] = Token::U8(1);
		assert_de_tokens_error::<BA>(
			&tokens,
			"invalid value: integer `1`, expected `BitArray` must have a \
			 head-bit of `0`",
		);

		assert_de_tokens_error::<BA>(
			&[
				Token::Struct {
					name: "BitArr",
					len:  2,
				},
				Token::BorrowedStr("placeholder"),
			],
			&format!(
				"unknown field `placeholder`, expected one of `{}`",
				super::FIELDS.join("`, `"),
			),
		);
		assert_de_tokens_error::<BA>(
			&[
				Token::Struct {
					name: "BitArr",
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
		assert_de_tokens_error::<BA>(
			&[
				Token::Struct {
					name: "BitArr",
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
		assert_de_tokens_error::<BA>(
			&[
				Token::Struct {
					name: "BitArr",
					len:  2,
				},
				Token::BorrowedStr("bits"),
				Token::U64(8),
				Token::BorrowedStr("bits"),
				Token::U64(8),
				Token::StructEnd,
			],
			"duplicate field `bits`",
		);
		assert_de_tokens_error::<BA>(
			&[
				Token::Struct {
					name: "BitArr",
					len:  2,
				},
				Token::BorrowedStr("data"),
				Token::Tuple { len: 1 },
				Token::U8(0),
				Token::TupleEnd,
				Token::BorrowedStr("data"),
				Token::Tuple { len: 1 },
				Token::U8(1),
				Token::TupleEnd,
				Token::StructEnd,
			],
			"duplicate field `data`",
		);
	}
}
