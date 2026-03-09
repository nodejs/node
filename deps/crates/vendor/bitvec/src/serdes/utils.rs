#![doc=include_str!("../../doc/serdes/utils.md")]

use core::{
	any,
	fmt::{
		self,
		Formatter,
	},
	marker::PhantomData,
	mem::MaybeUninit,
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
		SerializeSeq,
		SerializeStruct,
		SerializeTuple,
		Serializer,
	},
};
use wyz::comu::Const;

use crate::{
	domain::Domain,
	index::BitIdx,
	mem::{
		bits_of,
		BitRegister,
	},
	order::BitOrder,
	store::BitStore,
	view::BitViewSized,
};

/// A zero-sized type that deserializes from any string as long as it is equal
/// to `any::type_name::<T>()`.
pub(super) struct TypeName<T>(PhantomData<T>);

impl<T> TypeName<T> {
	/// Creates a type-name ghost for any type.
	fn new() -> Self {
		TypeName(PhantomData)
	}
}

impl<'de, T> Deserialize<'de> for TypeName<T> {
	fn deserialize<D>(deserializer: D) -> Result<Self, D::Error>
	where D: Deserializer<'de> {
		deserializer.deserialize_str(Self::new())
	}
}

impl<'de, T> Visitor<'de> for TypeName<T> {
	type Value = Self;

	fn expecting(&self, fmt: &mut Formatter) -> fmt::Result {
		write!(fmt, "the string {:?}", any::type_name::<T>())
	}

	fn visit_str<E>(self, value: &str) -> Result<Self::Value, E>
	where E: serde::de::Error {
		if value == any::type_name::<T>() {
			Ok(self)
		}
		else {
			Err(serde::de::Error::invalid_value(
				Unexpected::Str(value),
				&self,
			))
		}
	}
}

/// Fields used in the `BitIdx` transport format.
static FIELDS: &[&str] = &["width", "index"];

/// The components of a bit-idx in wire format.
enum Field {
	/// Denotes the maximum allowable value of the bit-idx.
	Width,
	/// Denotes the value of the bit-idx.
	Index,
}

/// Visits field tokens of a bit-idx wire format.
struct FieldVisitor;

impl<'de> Deserialize<'de> for Field {
	fn deserialize<D>(deserializer: D) -> Result<Self, D::Error>
	where D: Deserializer<'de> {
		deserializer.deserialize_identifier(FieldVisitor)
	}
}

impl<'de> Visitor<'de> for FieldVisitor {
	type Value = Field;

	fn expecting(&self, fmt: &mut Formatter) -> fmt::Result {
		fmt.write_str("field identifier")
	}

	fn visit_str<E>(self, value: &str) -> Result<Self::Value, E>
	where E: serde::de::Error {
		match value {
			"width" => Ok(Field::Width),
			"index" => Ok(Field::Index),
			_ => Err(serde::de::Error::unknown_field(value, FIELDS)),
		}
	}
}

impl<R> Serialize for BitIdx<R>
where R: BitRegister
{
	#[inline]
	fn serialize<S>(&self, serializer: S) -> super::Result<S>
	where S: Serializer {
		let mut state = serializer.serialize_struct("BitIdx", FIELDS.len())?;

		//  Emit the bit-width of the `R` type.
		state.serialize_field(FIELDS[0], &(bits_of::<R>() as u8))?;
		//  Emit the actual head-bit index.
		state.serialize_field(FIELDS[1], &self.into_inner())?;

		state.end()
	}
}

impl<'de, R> Deserialize<'de> for BitIdx<R>
where R: BitRegister
{
	#[inline]
	fn deserialize<D>(deserializer: D) -> Result<Self, D::Error>
	where D: Deserializer<'de> {
		deserializer.deserialize_struct(
			"BitIdx",
			FIELDS,
			BitIdxVisitor::<R>::THIS,
		)
	}
}

impl<T, O> Serialize for Domain<'_, Const, T, O>
where
	T: BitStore,
	O: BitOrder,
	T::Mem: Serialize,
{
	#[inline]
	fn serialize<S>(&self, serializer: S) -> super::Result<S>
	where S: Serializer {
		//  Domain<T> is functionally equivalent to `[T::Mem]`.
		let mut state = serializer.serialize_seq(Some(self.len()))?;
		for elem in *self {
			state.serialize_element(&elem)?;
		}
		state.end()
	}
}

/** `serde` only provides implementations for `[T; 0 ..= 32]`. This wrapper
provides the same de/ser logic, but allows it to be used on arrays of any size.
**/
#[repr(transparent)]
#[derive(Clone, Copy, Debug, Eq, Hash, Ord, PartialEq, PartialOrd)]
pub(super) struct Array<T, const N: usize>
where T: BitStore
{
	/// The data buffer being transported.
	pub(super) inner: [T; N],
}

impl<T, const N: usize> Array<T, N>
where T: BitStore
{
	/// Constructs a `&Array` reference from an `&[T; N]` reference.
	///
	/// ## Safety
	///
	/// `Array` is `#[repr(transparent)]`, so this address transformation is
	/// always sound.
	pub(super) fn from_ref(arr: &[T; N]) -> &Self {
		unsafe { &*(arr as *const [T; N] as *const Self) }
	}
}

impl<T, const N: usize> Serialize for Array<T, N>
where
	T: BitStore,
	T::Mem: Serialize,
{
	#[inline]
	fn serialize<S>(&self, serializer: S) -> super::Result<S>
	where S: Serializer {
		//  `serde` serializes arrays as a tuple, so that transport formats can
		//  safely choose to keep or discard the length counter.
		let mut state = serializer.serialize_tuple(N)?;
		for elem in self.inner.as_raw_slice().iter().map(BitStore::load_value) {
			state.serialize_element(&elem)?
		}
		state.end()
	}
}

impl<'de, T, const N: usize> Deserialize<'de> for Array<T, N>
where
	T: BitStore,
	T::Mem: Deserialize<'de>,
{
	#[inline]
	fn deserialize<D>(deserializer: D) -> Result<Self, D::Error>
	where D: Deserializer<'de> {
		deserializer.deserialize_tuple(N, ArrayVisitor::<T, N>::THIS)
	}
}

/// Assists in deserialization of a static `[T; N]` for any `N`.
struct ArrayVisitor<T, const N: usize>
where T: BitStore
{
	/// This produces an array during its work.
	inner: PhantomData<[T; N]>,
}

impl<T, const N: usize> ArrayVisitor<T, N>
where T: BitStore
{
	/// A blank visitor in its ready state.
	const THIS: Self = Self { inner: PhantomData };
}

impl<'de, T, const N: usize> Visitor<'de> for ArrayVisitor<T, N>
where
	T: BitStore,
	T::Mem: Deserialize<'de>,
{
	type Value = Array<T, N>;

	#[inline]
	fn expecting(&self, fmt: &mut Formatter) -> fmt::Result {
		write!(fmt, "a [{}; {}]", any::type_name::<T>(), N)
	}

	#[inline]
	fn visit_seq<V>(self, mut seq: V) -> Result<Self::Value, V::Error>
	where V: SeqAccess<'de> {
		let mut uninit = [MaybeUninit::<T::Mem>::uninit(); N];
		for (idx, slot) in uninit.iter_mut().enumerate() {
			slot.write(
				seq.next_element::<T::Mem>()?
					.ok_or_else(|| <V::Error>::invalid_length(idx, &self))?,
			);
		}
		Ok(Array {
			inner: uninit
				.map(|elem| unsafe { MaybeUninit::assume_init(elem) })
				.map(BitStore::new),
		})
	}
}

/// Assists in deserialization of a `BitIdx` value.
struct BitIdxVisitor<R>
where R: BitRegister
{
	/// This requires carrying the register type information.
	inner: PhantomData<R>,
}

impl<R> BitIdxVisitor<R>
where R: BitRegister
{
	/// A blank visitor in its ready state.
	const THIS: Self = Self { inner: PhantomData };

	/// Attempts to assemble deserialized components into an output value.
	#[inline]
	fn assemble<E>(self, width: u8, index: u8) -> Result<BitIdx<R>, E>
	where E: Error {
		//  Fail if the transported type width does not match the destination.
		if width != bits_of::<R>() as u8 {
			return Err(E::invalid_type(
				Unexpected::Unsigned(width as u64),
				&self,
			));
		}

		//  Capture an invalid index value and route it to the error handler.
		BitIdx::<R>::new(index).map_err(|_| {
			E::invalid_value(Unexpected::Unsigned(index as u64), &self)
		})
	}
}

impl<'de, R> Visitor<'de> for BitIdxVisitor<R>
where R: BitRegister
{
	type Value = BitIdx<R>;

	#[inline]
	fn expecting(&self, fmt: &mut Formatter) -> fmt::Result {
		write!(fmt, "a valid `BitIdx<u{}>`", bits_of::<R>())
	}

	#[inline]
	fn visit_seq<V>(self, mut seq: V) -> Result<Self::Value, V::Error>
	where V: SeqAccess<'de> {
		let width = seq
			.next_element::<u8>()?
			.ok_or_else(|| <V::Error>::invalid_length(0, &self))?;
		let index = seq
			.next_element::<u8>()?
			.ok_or_else(|| <V::Error>::invalid_length(1, &self))?;

		self.assemble(width, index)
	}

	#[inline]
	fn visit_map<V>(self, mut map: V) -> Result<Self::Value, V::Error>
	where V: MapAccess<'de> {
		let mut width = None;
		let mut index = None;

		while let Some(key) = map.next_key()? {
			match key {
				Field::Width => {
					if width.replace(map.next_value::<u8>()?).is_some() {
						return Err(<V::Error>::duplicate_field("width"));
					}
				},
				Field::Index => {
					if index.replace(map.next_value::<u8>()?).is_some() {
						return Err(<V::Error>::duplicate_field("index"));
					}
				},
			}
		}

		let width = width.ok_or_else(|| <V::Error>::missing_field("width"))?;
		let index = index.ok_or_else(|| <V::Error>::missing_field("index"))?;

		self.assemble(width, index)
	}
}

#[cfg(test)]
mod tests {
	use serde_test::{
		assert_de_tokens,
		assert_de_tokens_error,
		assert_ser_tokens,
		Token,
	};

	use super::*;

	#[test]
	fn array_wrapper() {
		let array = Array { inner: [0u8; 40] };
		#[rustfmt::skip]
		let tokens = &[
			Token::Tuple { len: 40 },
			Token::U8(0), Token::U8(0), Token::U8(0), Token::U8(0), Token::U8(0),
			Token::U8(0), Token::U8(0), Token::U8(0), Token::U8(0), Token::U8(0),
			Token::U8(0), Token::U8(0), Token::U8(0), Token::U8(0), Token::U8(0),
			Token::U8(0), Token::U8(0), Token::U8(0), Token::U8(0), Token::U8(0),
			Token::U8(0), Token::U8(0), Token::U8(0), Token::U8(0), Token::U8(0),
			Token::U8(0), Token::U8(0), Token::U8(0), Token::U8(0), Token::U8(0),
			Token::U8(0), Token::U8(0), Token::U8(0), Token::U8(0), Token::U8(0),
			Token::U8(0), Token::U8(0), Token::U8(0), Token::U8(0), Token::U8(0),
			Token::TupleEnd,
		];
		assert_ser_tokens(&array, tokens);
		assert_de_tokens(&array, tokens);

		let tokens = &[Token::Tuple { len: 1 }, Token::U32(0), Token::TupleEnd];
		assert_de_tokens_error::<Array<u32, 2>>(
			tokens,
			"invalid length 1, expected a [u32; 2]",
		);
	}

	#[test]
	fn bit_idx() {
		let idx = BitIdx::<u32>::new(20).unwrap();
		let tokens = &mut [
			Token::Struct {
				name: "BitIdx",
				len:  2,
			},
			Token::Str("width"),
			Token::U8(32),
			Token::Str("index"),
			Token::U8(20),
			Token::StructEnd,
		];
		assert_ser_tokens(&idx, tokens);
		tokens[1] = Token::BorrowedStr("width");
		tokens[3] = Token::BorrowedStr("index");
		assert_de_tokens(&idx, tokens);

		let idx = BitIdx::<u16>::new(10).unwrap();
		let tokens = &[
			Token::Seq { len: Some(2) },
			Token::U8(16),
			Token::U8(10),
			Token::SeqEnd,
		];
		assert_de_tokens(&idx, tokens);

		assert_de_tokens_error::<BitIdx<u16>>(
			&[
				Token::Seq { len: Some(2) },
				Token::U8(8),
				Token::U8(0),
				Token::SeqEnd,
			],
			"invalid type: integer `8`, expected a valid `BitIdx<u16>`",
		);
		assert_de_tokens_error::<BitIdx<u16>>(
			&[
				Token::Seq { len: Some(2) },
				Token::U8(16),
				Token::U8(16),
				Token::SeqEnd,
			],
			"invalid value: integer `16`, expected a valid `BitIdx<u16>`",
		);
		assert_de_tokens_error::<BitIdx<u8>>(
			&[
				Token::Struct {
					name: "BitIdx",
					len:  1,
				},
				Token::BorrowedStr("unknown"),
			],
			"unknown field `unknown`, expected `width` or `index`",
		);
		assert_de_tokens_error::<BitIdx<u8>>(
			&[
				Token::Struct {
					name: "BitIdx",
					len:  2,
				},
				Token::BorrowedStr("width"),
				Token::U8(8),
				Token::BorrowedStr("width"),
				Token::U8(8),
				Token::StructEnd,
			],
			"duplicate field `width`",
		);
		assert_de_tokens_error::<BitIdx<u8>>(
			&[
				Token::Struct {
					name: "BitIdx",
					len:  2,
				},
				Token::BorrowedStr("index"),
				Token::U8(7),
				Token::BorrowedStr("index"),
				Token::U8(7),
				Token::StructEnd,
			],
			"duplicate field `index`",
		);
	}
}
