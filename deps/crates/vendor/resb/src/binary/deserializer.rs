// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

use core::fmt;

use crate::MASK_28_BIT;

use super::{
    get_subslice, header::BinHeader, read_u16, BinIndex, BinaryDeserializerError, CharsetFamily,
    FormatVersion, ResDescriptor, ResourceReprType,
};

extern crate alloc;
use alloc::string::String;

use serde_core::{de, forward_to_deserialize_any, Deserialize};

/// The character set family of the current system.
///
/// Systems using EBCDIC are not supported at this time.
const SYSTEM_CHARSET_FAMILY: CharsetFamily = CharsetFamily::Ascii;

/// Deserializes an instance of type `T` from bytes representing a binary ICU
/// resource bundle.
pub fn from_words<'a, T>(input: &'a [u32]) -> Result<T, BinaryDeserializerError>
where
    T: Deserialize<'a>,
{
    let mut deserializer = ResourceTreeDeserializer::from_bytes(input)?;
    let t = T::deserialize(&mut deserializer)?;

    Ok(t)
}

/// The `ResourceTreeDeserializer` struct processes an ICU binary resource
/// bundle by walking the resource tree (as represented by [`ResDescriptor`]s).
struct ResourceTreeDeserializer<'de> {
    /// The current position in the input, represented as a slice beginning at
    /// the next byte to be read and ending at the end of input.
    ///
    /// As an invariant of the deserializer, `input` should always begin
    /// immediately before a resource descriptor.
    input: &'de [u8],

    /// The format version of the input.
    ///
    /// This is currently unused, but support for other format versions will be
    /// incorporated in later versions.
    _format_version: FormatVersion,

    /// The 16-bit data block, represented as a slice beginning at the start of
    /// the block. This is `None` in format versions below 2.0.
    data_16_bit: Option<&'de [u8]>,

    /// The keys block, represented as a slice beginning at the start of the
    /// block.
    keys: &'de [u8],

    /// The input body, represented as a slice beginning at the start of the
    /// block.
    body: &'de [u8],
}

impl<'de> ResourceTreeDeserializer<'de> {
    /// Creates a new deserializer from the header and index of the resource
    /// bundle.
    fn from_bytes(input: &'de [u32]) -> Result<Self, BinaryDeserializerError> {
        let input = unsafe {
            core::slice::from_raw_parts(input.as_ptr() as *const u8, core::mem::size_of_val(input))
        };

        let header = BinHeader::try_from(input)?;

        // Verify that the representation in the resource bundle is one we're
        // prepared to read.
        if header.repr_info.charset_family != SYSTEM_CHARSET_FAMILY {
            return Err(BinaryDeserializerError::unsupported_format(
                "bundle and system character set families do not match",
            ));
        }

        if header.repr_info.size_of_char != 2 {
            return Err(BinaryDeserializerError::unsupported_format(
                "characters of size other than 2 are not supported",
            ));
        }

        if header.repr_info.format_version != FormatVersion::V2_0 {
            // Support for other versions can be added at a later time, but for
            // now we can only deal with 2.0.
            return Err(BinaryDeserializerError::unsupported_format(
                "format versions other than 2.0 are not supported at this time",
            ));
        }

        let body = get_subslice(input, header.size as usize..)?;

        // Skip the root resource descriptor and get the index area.
        let index = get_subslice(body, core::mem::size_of::<u32>()..)?;
        let index = BinIndex::try_from(index)?;

        // Keys begin at the start of the body.
        let keys = get_subslice(
            body,
            ..(index.keys_end as usize) * core::mem::size_of::<u32>(),
        )?;

        let data_16_bit = if header.repr_info.format_version < FormatVersion::V2_0 {
            // The 16-bit data area was not introduced until format version 2.0.
            None
        } else if let Some(data_16_bit_end) = index.data_16_bit_end {
            let data_16_bit = get_subslice(
                body,
                (index.keys_end as usize) * core::mem::size_of::<u32>()
                    ..(data_16_bit_end as usize) * core::mem::size_of::<u32>(),
            )?;
            Some(data_16_bit)
        } else {
            return Err(BinaryDeserializerError::invalid_data(
                "offset to the end of 16-bit data not specified",
            ));
        };

        Ok(Self {
            input: body,
            _format_version: header.repr_info.format_version,
            data_16_bit,
            keys,
            body,
        })
    }

    /// Reads the next resource descriptor without updating the input position.
    fn peek_next_resource_descriptor(&self) -> Result<ResDescriptor, BinaryDeserializerError> {
        let input = get_subslice(self.input, 0..4)?;
        let descriptor = match input.try_into() {
            Ok(value) => value,
            Err(_) => {
                return Err(BinaryDeserializerError::invalid_data(
                    "unable to read resource descriptor",
                ))
            }
        };
        let descriptor = u32::from_le_bytes(descriptor);

        ResDescriptor::try_from(descriptor)
    }

    /// Reads the next resource descriptor.
    fn get_next_resource_descriptor(&mut self) -> Result<ResDescriptor, BinaryDeserializerError> {
        let result = self.peek_next_resource_descriptor();

        // Pop resource descriptor from input.
        self.input = get_subslice(self.input, core::mem::size_of::<u32>()..)?;

        result
    }

    /// Reads a 28-bit integer resource as a signed value.
    fn parse_signed(&mut self) -> Result<i32, BinaryDeserializerError> {
        let descriptor = self.get_next_resource_descriptor()?;
        match descriptor.resource_type() {
            // Since integers in the resource bundle are 28-bit, we need to
            // shift left and shift back right in order to get sign extension.
            // Per https://doc.rust-lang.org/reference/expressions/operator-expr.html#arithmetic-and-logical-binary-operators,
            // `>>` is arithmetic right shift on signed ints, so it gives us the
            // desired behavior.
            ResourceReprType::Int => Ok(descriptor.value_as_signed_int()),
            _ => Err(BinaryDeserializerError::resource_type_mismatch(
                "expected integer resource",
            )),
        }
    }

    /// Reads a 28-bit integer resource as an unsigned value.
    fn parse_unsigned(&mut self) -> Result<u32, BinaryDeserializerError> {
        let descriptor = self.get_next_resource_descriptor()?;
        match descriptor.resource_type() {
            ResourceReprType::Int => Ok(descriptor.value_as_unsigned_int()),
            _ => Err(BinaryDeserializerError::resource_type_mismatch(
                "expected integer resource",
            )),
        }
    }
}

impl<'de> de::Deserializer<'de> for &mut ResourceTreeDeserializer<'de> {
    type Error = BinaryDeserializerError;

    fn deserialize_any<V>(self, visitor: V) -> Result<V::Value, Self::Error>
    where
        V: de::Visitor<'de>,
    {
        let descriptor = self.peek_next_resource_descriptor()?;
        match descriptor.resource_type() {
            ResourceReprType::_String | ResourceReprType::StringV2 => {
                self.deserialize_string(visitor)
            }
            ResourceReprType::Binary => self.deserialize_bytes(visitor),
            ResourceReprType::Table | ResourceReprType::Table16 | ResourceReprType::_Table32 => {
                self.deserialize_map(visitor)
            }
            ResourceReprType::_Alias => todo!(),
            ResourceReprType::Int => self.deserialize_u32(visitor),
            ResourceReprType::Array | ResourceReprType::Array16 | ResourceReprType::IntVector => {
                self.deserialize_seq(visitor)
            }
        }
    }

    fn deserialize_bool<V>(self, visitor: V) -> Result<V::Value, Self::Error>
    where
        V: de::Visitor<'de>,
    {
        let value = self.parse_unsigned()?;
        let value = match value {
            0 => false,
            1 => true,
            _ => {
                return Err(BinaryDeserializerError::resource_type_mismatch(
                    "expected integer resource representable as boolean",
                ))
            }
        };

        visitor.visit_bool(value)
    }

    fn deserialize_i8<V>(self, visitor: V) -> Result<V::Value, Self::Error>
    where
        V: de::Visitor<'de>,
    {
        self.deserialize_i32(visitor)
    }

    fn deserialize_i16<V>(self, visitor: V) -> Result<V::Value, Self::Error>
    where
        V: de::Visitor<'de>,
    {
        self.deserialize_i32(visitor)
    }

    fn deserialize_i32<V>(self, visitor: V) -> Result<V::Value, Self::Error>
    where
        V: de::Visitor<'de>,
    {
        visitor.visit_i32(self.parse_signed()?)
    }

    fn deserialize_i64<V>(self, visitor: V) -> Result<V::Value, Self::Error>
    where
        V: de::Visitor<'de>,
    {
        visitor.visit_i64(self.parse_signed()? as i64)
    }

    fn deserialize_u8<V>(self, visitor: V) -> Result<V::Value, Self::Error>
    where
        V: de::Visitor<'de>,
    {
        self.deserialize_u32(visitor)
    }

    fn deserialize_u16<V>(self, visitor: V) -> Result<V::Value, Self::Error>
    where
        V: de::Visitor<'de>,
    {
        self.deserialize_u32(visitor)
    }

    fn deserialize_u32<V>(self, visitor: V) -> Result<V::Value, Self::Error>
    where
        V: de::Visitor<'de>,
    {
        visitor.visit_u32(self.parse_unsigned()?)
    }

    fn deserialize_u64<V>(self, visitor: V) -> Result<V::Value, Self::Error>
    where
        V: de::Visitor<'de>,
    {
        visitor.visit_u64(self.parse_unsigned()? as u64)
    }

    fn deserialize_f32<V>(self, _visitor: V) -> Result<V::Value, Self::Error>
    where
        V: de::Visitor<'de>,
    {
        // Resource bundles have no native concept of floating point numbers and
        // no examples of storing them have been encountered.
        unimplemented!()
    }

    fn deserialize_f64<V>(self, _visitor: V) -> Result<V::Value, Self::Error>
    where
        V: de::Visitor<'de>,
    {
        // Resource bundles have no native concept of floating point numbers and
        // no examples of storing them have been encountered.
        unimplemented!()
    }

    fn deserialize_char<V>(self, _visitor: V) -> Result<V::Value, Self::Error>
    where
        V: de::Visitor<'de>,
    {
        // Resource bundles have no native concept of single characters and no
        // examples of storing them have been encountered.
        unimplemented!()
    }

    fn deserialize_str<V>(self, visitor: V) -> Result<V::Value, Self::Error>
    where
        V: de::Visitor<'de>,
    {
        // Strings in resource bundles are stored as UTF-16 and can't be
        // borrowed.
        self.deserialize_string(visitor)
    }

    fn deserialize_string<V>(self, visitor: V) -> Result<V::Value, Self::Error>
    where
        V: de::Visitor<'de>,
    {
        let descriptor = self.get_next_resource_descriptor()?;
        match descriptor.resource_type() {
            ResourceReprType::_String => todo!(),
            ResourceReprType::StringV2 => {
                if let Some(data_16_bit) = self.data_16_bit {
                    if descriptor.is_empty() {
                        return visitor.visit_str("");
                    }

                    let input = get_subslice(data_16_bit, descriptor.value_as_16_bit_offset()..)?;
                    let de = Resource16BitDeserializer::new(input);
                    de.deserialize_string(visitor)
                } else {
                    Err(BinaryDeserializerError::invalid_data(
                        "StringV2 resource without 16-bit data block",
                    ))
                }
            }
            _ => Err(BinaryDeserializerError::resource_type_mismatch(
                "expected string resource",
            )),
        }
    }

    fn deserialize_bytes<V>(self, visitor: V) -> Result<V::Value, Self::Error>
    where
        V: de::Visitor<'de>,
    {
        let descriptor = self.get_next_resource_descriptor()?;
        let value = match descriptor.resource_type() {
            ResourceReprType::Binary => {
                // Binary resources are, by definition, a sequence of arbitrary
                // bytes and can be borrowed as such.
                if descriptor.is_empty() {
                    // Handle empty descriptors per-type so we don't miss a type
                    // mismatch.
                    return visitor.visit_borrowed_bytes(&[]);
                }

                let input = get_subslice(self.body, descriptor.value_as_32_bit_offset()..)?;
                let (length, input) = read_u32(input)?;

                get_subslice(input, 0..length as usize)?
            }
            ResourceReprType::IntVector => {
                // Int vector resources are stored as a sequence of 32-bit
                // integers in the bundle's native endian. For zero-copy, it may
                // be desirable to simply borrow as bytes.
                if descriptor.is_empty() {
                    // Handle empty descriptors per-type so we don't miss a type
                    // mismatch.
                    return visitor.visit_borrowed_bytes(&[]);
                }

                let input = get_subslice(self.body, descriptor.value_as_32_bit_offset()..)?;
                let (length, input) = read_u32(input)?;

                get_subslice(input, ..(length as usize) * core::mem::size_of::<u32>())?
            }
            ResourceReprType::StringV2 => {
                // String resources are stored as UTF-16 strings in the bundle's
                // native endian. In situations where treatment as strings may
                // not be needed or performance would benefit from lazy
                // interpretation, allow for zero-copy.
                if let Some(data_16_bit) = self.data_16_bit {
                    if descriptor.is_empty() {
                        // Handle empty descriptors per-type so we don't miss a
                        // type mismatch.
                        return visitor.visit_borrowed_bytes(&[]);
                    }

                    let input = get_subslice(data_16_bit, descriptor.value_as_16_bit_offset()..)?;
                    let (length, input) = get_length_and_start_of_utf16_string(input)?;
                    get_subslice(input, ..length * core::mem::size_of::<u16>())?
                } else {
                    return Err(BinaryDeserializerError::invalid_data(
                        "StringV2 resource without 16-bit data block",
                    ));
                }
            }
            _ => {
                return Err(BinaryDeserializerError::resource_type_mismatch(
                    "expected binary data resource",
                ))
            }
        };

        visitor.visit_borrowed_bytes(value)
    }

    fn deserialize_byte_buf<V>(self, visitor: V) -> Result<V::Value, Self::Error>
    where
        V: de::Visitor<'de>,
    {
        self.deserialize_bytes(visitor)
    }

    fn deserialize_option<V>(self, visitor: V) -> Result<V::Value, Self::Error>
    where
        V: de::Visitor<'de>,
    {
        visitor.visit_some(self)
    }

    fn deserialize_unit<V>(self, _visitor: V) -> Result<V::Value, Self::Error>
    where
        V: de::Visitor<'de>,
    {
        // There's no concept of `null` or any other unit type in resource
        // bundles.
        unimplemented!()
    }

    fn deserialize_unit_struct<V>(
        self,
        _name: &'static str,
        _visitor: V,
    ) -> Result<V::Value, Self::Error>
    where
        V: de::Visitor<'de>,
    {
        // There's no concept of `null` or any other unit type in resource
        // bundles.
        unimplemented!()
    }

    fn deserialize_newtype_struct<V>(
        self,
        _name: &'static str,
        visitor: V,
    ) -> Result<V::Value, Self::Error>
    where
        V: de::Visitor<'de>,
    {
        // Resource bundles have no concept of newtypes, so just pass through
        // and let the visitor ask for what it expects.
        visitor.visit_newtype_struct(self)
    }

    fn deserialize_seq<V>(self, visitor: V) -> Result<V::Value, Self::Error>
    where
        V: de::Visitor<'de>,
    {
        let descriptor = self.get_next_resource_descriptor()?;
        match descriptor.resource_type() {
            ResourceReprType::Array => {
                if descriptor.is_empty() {
                    // Handle empty descriptors per-type so we don't miss a type
                    // mismatch.
                    return visitor.visit_seq(EmptySeqAccess);
                }

                let input = get_subslice(self.body, descriptor.value_as_32_bit_offset()..)?;
                let (length, offsets) = read_u32(input)?;

                visitor.visit_seq(ArraySeqAccess {
                    de: self,
                    descriptors: offsets,
                    remaining: length as usize,
                })
            }
            ResourceReprType::Array16 => {
                if descriptor.is_empty() {
                    // Handle empty descriptors per-type so we don't miss a type
                    // mismatch.
                    return visitor.visit_seq(EmptySeqAccess);
                }

                if let Some(data_16_bit) = self.data_16_bit {
                    let input = get_subslice(data_16_bit, descriptor.value_as_16_bit_offset()..)?;
                    let (length, offsets) = read_u16(input)?;

                    let result = visitor.visit_seq(Array16SeqAccess {
                        data_16_bit,
                        offsets,
                        remaining: length as usize,
                    });

                    result
                } else {
                    Err(BinaryDeserializerError::invalid_data(
                        "StringV2 resource with no 16-bit data",
                    ))
                }
            }
            ResourceReprType::IntVector => {
                if descriptor.is_empty() {
                    // Handle empty descriptors per-type so we don't miss a type
                    // mismatch.
                    return visitor.visit_seq(EmptySeqAccess);
                }

                let input = get_subslice(self.body, descriptor.value_as_32_bit_offset()..)?;
                let (length, values) = read_u32(input)?;

                let result = visitor.visit_seq(IntVectorSeqAccess {
                    values,
                    remaining: length as usize,
                });

                result
            }
            _ => Err(BinaryDeserializerError::resource_type_mismatch(
                "expected array resource",
            )),
        }
    }

    fn deserialize_tuple<V>(self, _len: usize, visitor: V) -> Result<V::Value, Self::Error>
    where
        V: de::Visitor<'de>,
    {
        // We copy `serde_json` in handling tuples as sequences.
        self.deserialize_seq(visitor)
    }

    fn deserialize_tuple_struct<V>(
        self,
        _name: &'static str,
        _len: usize,
        visitor: V,
    ) -> Result<V::Value, Self::Error>
    where
        V: de::Visitor<'de>,
    {
        // We copy `serde_json` in handling tuples as sequences.
        self.deserialize_seq(visitor)
    }

    fn deserialize_map<V>(self, visitor: V) -> Result<V::Value, Self::Error>
    where
        V: de::Visitor<'de>,
    {
        let descriptor = self.get_next_resource_descriptor()?;
        match descriptor.resource_type() {
            ResourceReprType::Table => {
                if descriptor.is_empty() {
                    // Handle empty descriptors per-type so we don't miss a type
                    // mismatch.
                    return visitor.visit_map(EmptyMapAccess);
                }

                let input = get_subslice(self.body, descriptor.value_as_32_bit_offset()..)?;
                let (length, keys) = read_u16(input)?;

                // Most values in the file are 32-bit aligned, so sequences of
                // 16-bit values may be padded.
                let length_with_padding = (length + ((length + 1) % 2)) as usize;

                let values_offset = length_with_padding * core::mem::size_of::<u16>();
                let values = get_subslice(keys, values_offset..)?;

                visitor.visit_map(TableMapAccess {
                    de: self,
                    keys,
                    values,
                    remaining: length as usize,
                })
            }
            ResourceReprType::_Table32 => todo!(),
            ResourceReprType::Table16 => todo!(),
            _ => Err(BinaryDeserializerError::resource_type_mismatch(
                "expected table resource",
            )),
        }
    }

    fn deserialize_struct<V>(
        self,
        _name: &'static str,
        _fields: &'static [&'static str],
        visitor: V,
    ) -> Result<V::Value, Self::Error>
    where
        V: de::Visitor<'de>,
    {
        self.deserialize_map(visitor)
    }

    fn deserialize_enum<V>(
        self,
        _name: &'static str,
        _variants: &'static [&'static str],
        _visitor: V,
    ) -> Result<V::Value, Self::Error>
    where
        V: de::Visitor<'de>,
    {
        // Resource bundles have no concept of an enum and it's unclear how to
        // handle untagged heterogeneous values.
        todo!()
    }

    fn deserialize_identifier<V>(self, _visitor: V) -> Result<V::Value, Self::Error>
    where
        V: de::Visitor<'de>,
    {
        unimplemented!()
    }

    fn deserialize_ignored_any<V>(self, visitor: V) -> Result<V::Value, Self::Error>
    where
        V: de::Visitor<'de>,
    {
        let (_, input) = read_u32(self.input)?;
        self.input = input;

        visitor.visit_none()
    }

    #[inline]
    fn is_human_readable(&self) -> bool {
        false
    }
}

/// The `Array16SeqAccess` struct provides deserialization for resources of type
/// `Array16`.
///
/// See [`ResourceReprType`] for more details.
struct Array16SeqAccess<'de> {
    data_16_bit: &'de [u8],
    offsets: &'de [u8],
    remaining: usize,
}

impl<'de> de::SeqAccess<'de> for Array16SeqAccess<'de> {
    type Error = BinaryDeserializerError;

    fn next_element_seed<T>(&mut self, seed: T) -> Result<Option<T::Value>, Self::Error>
    where
        T: de::DeserializeSeed<'de>,
    {
        if self.remaining == 0 {
            return Ok(None);
        }

        // Elements are stored as a sequence of `u16` offsets. Pop one and
        // deserialize the corresponding resource.
        let (offset, rest) = read_u16(self.offsets)?;
        self.offsets = rest;
        self.remaining -= 1;

        let input = get_subslice(
            self.data_16_bit,
            (offset as usize) * core::mem::size_of::<u16>()..,
        )?;
        let de = Resource16BitDeserializer::new(input);
        seed.deserialize(de).map(Some)
    }

    fn size_hint(&self) -> Option<usize> {
        Some(self.remaining)
    }
}

/// The `ArraySeqAccess` struct provides deserialization for resources of type
/// `Array`.
///
/// See [`ResourceReprType`] for more details.
struct ArraySeqAccess<'a, 'de: 'a> {
    de: &'a mut ResourceTreeDeserializer<'de>,
    descriptors: &'de [u8],
    remaining: usize,
}

impl<'de> de::SeqAccess<'de> for ArraySeqAccess<'_, 'de> {
    type Error = BinaryDeserializerError;

    fn next_element_seed<T>(&mut self, seed: T) -> Result<Option<T::Value>, Self::Error>
    where
        T: de::DeserializeSeed<'de>,
    {
        if self.remaining == 0 {
            return Ok(None);
        }

        // Elements are stored as a sequence of resource descriptors. Pop one
        // and deserialize the corresponding resource.
        let input = self.descriptors;
        self.descriptors = get_subslice(self.descriptors, core::mem::size_of::<u32>()..)?;
        self.remaining -= 1;

        // Input must always start at a resource descriptor. The rest of the
        // input is immaterial in this case, as we will return to this function
        // for the next descriptor.
        self.de.input = input;
        seed.deserialize(&mut *self.de).map(Some)
    }

    fn size_hint(&self) -> Option<usize> {
        Some(self.remaining)
    }
}

/// The `IntVectorSeqAccess` struct provides deserialization for resources of
/// type `IntVector`.
///
/// See [`ResourceReprType`] for more details.
struct IntVectorSeqAccess<'de> {
    values: &'de [u8],
    remaining: usize,
}

impl<'de> de::SeqAccess<'de> for IntVectorSeqAccess<'de> {
    type Error = BinaryDeserializerError;

    fn next_element_seed<T>(&mut self, seed: T) -> Result<Option<T::Value>, Self::Error>
    where
        T: de::DeserializeSeed<'de>,
    {
        if self.remaining == 0 {
            return Ok(None);
        }

        // Elements are stored as a sequence of 32-bit integers. Pop one and
        // feed it into the specialized int vector deserializer.
        let input = self.values;
        self.values = get_subslice(self.values, core::mem::size_of::<u32>()..)?;
        self.remaining -= 1;

        let de = IntVectorDeserializer::new(input);
        seed.deserialize(de).map(Some)
    }

    fn size_hint(&self) -> Option<usize> {
        Some(self.remaining)
    }
}

/// The `EmptySeqAccess` struct provides for deserialization of any empty
/// array resource, including `IntVector` and string types.
struct EmptySeqAccess;

impl<'de> de::SeqAccess<'de> for EmptySeqAccess {
    type Error = BinaryDeserializerError;

    fn next_element_seed<T>(&mut self, _seed: T) -> Result<Option<T::Value>, Self::Error>
    where
        T: de::DeserializeSeed<'de>,
    {
        Ok(None)
    }

    fn size_hint(&self) -> Option<usize> {
        Some(0)
    }
}

/// The `EmptyMapAccess` struct provides for deserialization of any empty
/// table resource.
struct EmptyMapAccess;

impl<'de> de::MapAccess<'de> for EmptyMapAccess {
    type Error = BinaryDeserializerError;

    fn next_key_seed<K>(&mut self, _seed: K) -> Result<Option<K::Value>, Self::Error>
    where
        K: de::DeserializeSeed<'de>,
    {
        Ok(None)
    }

    #[expect(clippy::panic)]
    fn next_value_seed<V>(&mut self, _seed: V) -> Result<V::Value, Self::Error>
    where
        V: de::DeserializeSeed<'de>,
    {
        // We should never reach this code unless serde has violated the
        // invariant of never calling `next_value_seed()` if `next_key_seed()`
        // returns `None`. We have no reasonable value to return here, so our
        // only choice is to panic.
        panic!("Unable to process value for empty map. This is likely a `serde` bug.");
    }

    fn size_hint(&self) -> Option<usize> {
        Some(0)
    }
}

/// The `TableMapAccess` struct provides deserialization for resources of type
/// `Table`.
///
/// See [`ResourceReprType`] for more details.
struct TableMapAccess<'de, 'a> {
    de: &'a mut ResourceTreeDeserializer<'de>,
    keys: &'de [u8],
    values: &'de [u8],
    remaining: usize,
}

impl<'de> de::MapAccess<'de> for TableMapAccess<'de, '_> {
    type Error = BinaryDeserializerError;

    fn next_key_seed<K>(&mut self, seed: K) -> Result<Option<K::Value>, Self::Error>
    where
        K: de::DeserializeSeed<'de>,
    {
        if self.remaining == 0 {
            return Ok(None);
        }

        // Keys are stored as a sequence of byte offsets into the key block. Pop
        // one and feed it into the specialized key deserializer.
        let (key, keys) = read_u16(self.keys)?;
        self.keys = keys;
        self.remaining -= 1;

        let input = get_subslice(self.de.keys, key as usize..).or(Err(
            BinaryDeserializerError::invalid_data("unexpected end of data while deserializing key"),
        ))?;

        let de = KeyDeserializer::new(input);
        seed.deserialize(de).map(Some)
    }

    fn next_value_seed<V>(&mut self, seed: V) -> Result<V::Value, Self::Error>
    where
        V: de::DeserializeSeed<'de>,
    {
        // Values are stored as a sequence of resource descriptors. Pop one and
        // deserialize the corresponding resource.
        let value = self.values;
        self.values = get_subslice(self.values, core::mem::size_of::<u32>()..)?;

        self.de.input = value;
        seed.deserialize(&mut *self.de)
    }

    fn size_hint(&self) -> Option<usize> {
        Some(self.remaining)
    }
}

/// The `Resource16BitDeserializer` struct processes resources which are a part
/// of the 16-bit data block of the resource bundle. A resource will be in the
/// 16-bit data block if and only if it is a `StringV2`.
pub struct Resource16BitDeserializer<'de> {
    input: &'de [u8],
}

impl<'de> Resource16BitDeserializer<'de> {
    fn new(input: &'de [u8]) -> Self {
        Self { input }
    }

    /// Reads a UTF-16 string from the 16-bit data block.
    fn read_string_v2(self) -> Result<String, BinaryDeserializerError> {
        let (length, input) = get_length_and_start_of_utf16_string(self.input)?;

        let byte_slices = input.chunks_exact(2).take(length);
        if byte_slices.len() != length {
            // `take()` will silently return fewer elements than requested if
            // the input is too short, but that's an error during deserialize.
            return Err(BinaryDeserializerError::invalid_data(
                "unexpected end of input while reading string",
            ));
        }

        let units = byte_slices.map(|bytes| {
            // We can safely unwrap as we guarantee above that this chunk is
            // exactly 2 bytes.
            #[expect(clippy::unwrap_used)]
            let bytes = <[u8; 2]>::try_from(bytes).unwrap();
            u16::from_le_bytes(bytes)
        });

        char::decode_utf16(units)
            .collect::<Result<String, _>>()
            .map_err(|_| {
                BinaryDeserializerError::invalid_data("string resource is not valid UTF-16")
            })
    }
}

impl<'de> de::Deserializer<'de> for Resource16BitDeserializer<'de> {
    type Error = BinaryDeserializerError;

    fn deserialize_any<V>(self, visitor: V) -> Result<V::Value, Self::Error>
    where
        V: de::Visitor<'de>,
    {
        // The only type which can be wholly represented in the 16-bit data
        // block is `StringV2`.
        self.deserialize_string(visitor)
    }

    forward_to_deserialize_any! {
        bool i8 i16 i32 i64 i128 u8 u16 u32 u64 u128 f32 f64 char str
        byte_buf option unit unit_struct newtype_struct seq tuple
        tuple_struct map struct enum identifier ignored_any
    }

    fn deserialize_string<V>(self, visitor: V) -> Result<V::Value, Self::Error>
    where
        V: de::Visitor<'de>,
    {
        // Because `StringV2` is stored as UTF-16, we can't borrow it as a
        // `&str`. If zero-copy is desired, an appropriate data structure such
        // as `ZeroVec` will be needed.
        visitor.visit_string(self.read_string_v2()?)
    }

    fn deserialize_bytes<V>(self, visitor: V) -> Result<V::Value, Self::Error>
    where
        V: de::Visitor<'de>,
    {
        // `StringV2` is a contiguous sequence of native-endian `u16`, so we can
        // zero-copy deserialize it if the visitor supports it.
        let (length, input) = get_length_and_start_of_utf16_string(self.input)?;
        let bytes = get_subslice(input, 0..length * core::mem::size_of::<u16>())?;

        visitor.visit_borrowed_bytes(bytes)
    }

    fn is_human_readable(&self) -> bool {
        false
    }
}

/// The `IntVectorDeserializer` struct processes an `IntVector` resource,
/// consisting of 32-bit integers.
pub struct IntVectorDeserializer<'de> {
    input: &'de [u8],
}

impl<'de> IntVectorDeserializer<'de> {
    fn new(input: &'de [u8]) -> Self {
        Self { input }
    }

    /// Reads a 32-bit integer from the current input as signed.
    fn read_signed(mut self) -> Result<i32, BinaryDeserializerError> {
        let (value, next) = read_u32(self.input)?;
        self.input = next;

        Ok(value as i32)
    }

    /// Reads a 32-bit integer from the current input as unsigned.
    fn read_unsigned(mut self) -> Result<u32, BinaryDeserializerError> {
        let (value, next) = read_u32(self.input)?;
        self.input = next;

        Ok(value)
    }
}

impl<'de> de::Deserializer<'de> for IntVectorDeserializer<'de> {
    type Error = BinaryDeserializerError;

    fn deserialize_any<V>(self, visitor: V) -> Result<V::Value, Self::Error>
    where
        V: de::Visitor<'de>,
    {
        // The contents of `IntVector`s are always 32-bit integers. We can
        // safely generalize to `u32`, as there's no special handling needed
        // for signed integers.
        self.deserialize_u32(visitor)
    }

    forward_to_deserialize_any! {
        bool f32 f64 char str string
        bytes byte_buf option unit unit_struct newtype_struct seq tuple
        tuple_struct map struct enum identifier ignored_any
    }

    fn deserialize_i8<V>(self, visitor: V) -> Result<V::Value, Self::Error>
    where
        V: de::Visitor<'de>,
    {
        self.deserialize_i32(visitor)
    }

    fn deserialize_i16<V>(self, visitor: V) -> Result<V::Value, Self::Error>
    where
        V: de::Visitor<'de>,
    {
        self.deserialize_i32(visitor)
    }

    fn deserialize_i32<V>(self, visitor: V) -> Result<V::Value, Self::Error>
    where
        V: de::Visitor<'de>,
    {
        visitor.visit_i32(self.read_signed()?)
    }

    fn deserialize_i64<V>(self, visitor: V) -> Result<V::Value, Self::Error>
    where
        V: de::Visitor<'de>,
    {
        visitor.visit_i64(self.read_signed()? as i64)
    }

    fn deserialize_u8<V>(self, visitor: V) -> Result<V::Value, Self::Error>
    where
        V: de::Visitor<'de>,
    {
        self.deserialize_u32(visitor)
    }

    fn deserialize_u16<V>(self, visitor: V) -> Result<V::Value, Self::Error>
    where
        V: de::Visitor<'de>,
    {
        self.deserialize_u32(visitor)
    }

    fn deserialize_u32<V>(self, visitor: V) -> Result<V::Value, Self::Error>
    where
        V: de::Visitor<'de>,
    {
        visitor.visit_u32(self.read_unsigned()?)
    }

    fn deserialize_u64<V>(self, visitor: V) -> Result<V::Value, Self::Error>
    where
        V: de::Visitor<'de>,
    {
        visitor.visit_u64(self.read_unsigned()? as u64)
    }
}

pub struct KeyDeserializer<'de> {
    input: &'de [u8],
}

impl<'de> KeyDeserializer<'de> {
    fn new(input: &'de [u8]) -> Self {
        Self { input }
    }

    /// Reads a key from the current input.
    fn read_key(self) -> Result<&'de str, BinaryDeserializerError> {
        // Keys are stored as null-terminated UTF-8 strings. Locate the
        // terminating byte and return as a borrowed string.
        let terminator_pos = self.input.iter().position(|&byte| byte == 0).ok_or(
            BinaryDeserializerError::invalid_data("unterminated key string"),
        )?;

        let input = get_subslice(self.input, 0..terminator_pos)?;
        core::str::from_utf8(input)
            .map_err(|_| BinaryDeserializerError::invalid_data("key string is not valid UTF-8"))
    }
}

impl<'de> de::Deserializer<'de> for KeyDeserializer<'de> {
    type Error = BinaryDeserializerError;

    fn deserialize_any<V>(self, visitor: V) -> Result<V::Value, Self::Error>
    where
        V: de::Visitor<'de>,
    {
        self.deserialize_str(visitor)
    }

    forward_to_deserialize_any! {
        bool i8 i16 i32 i64 i128 u8 u16 u32 u64 u128 f32 f64 char string
        bytes byte_buf option unit unit_struct newtype_struct seq tuple
        tuple_struct map struct enum identifier ignored_any
    }

    fn deserialize_str<V>(self, visitor: V) -> Result<V::Value, Self::Error>
    where
        V: de::Visitor<'de>,
    {
        visitor.visit_borrowed_str(self.read_key()?)
    }
}

/// Determines the length in units of a serialized UTF-16 string.
///
/// Returns the length of the string and a slice beginning at the first unit.
fn get_length_and_start_of_utf16_string(
    input: &[u8],
) -> Result<(usize, &[u8]), BinaryDeserializerError> {
    let (first, rest) = read_u16(input)?;

    let (length, rest) = if (0xdc00..0xdfef).contains(&first) {
        // The unit is the entire length marker.
        ((first & 0x03ff) as usize, rest)
    } else if (0xdfef..0xdfff).contains(&first) {
        // The unit is the first of a 2-unit length marker.
        let (second, rest) = read_u16(rest)?;

        (((first as usize - 0xdfef) << 16) | second as usize, rest)
    } else if first == 0xdfff {
        // The unit is the first of a 3-unit length marker.
        let (second, rest) = read_u16(rest)?;
        let (third, rest) = read_u16(rest)?;

        (((second as usize) << 16) | third as usize, rest)
    } else {
        // The string has implicit length. These are strings of at least 1
        // and at most 40 units.
        let length = rest
            .chunks_exact(2)
            .take(40)
            .position(|chunk| chunk == [0, 0])
            .ok_or(BinaryDeserializerError::invalid_data(
                "unterminated string with implicit length",
            ))?
            + 1;

        (length, input)
    };

    Ok((length, rest))
}

impl de::StdError for BinaryDeserializerError {}

impl de::Error for BinaryDeserializerError {
    fn custom<T>(_msg: T) -> Self
    where
        T: fmt::Display,
    {
        #[cfg(feature = "logging")]
        log::warn!("Error during resource bundle deserialization: {_msg}");

        BinaryDeserializerError::unknown("error during deserialization; see logs")
    }
}

impl TryFrom<&[u8]> for BinIndex {
    type Error = BinaryDeserializerError;

    fn try_from(value: &[u8]) -> Result<Self, Self::Error> {
        let (field_count, value) = read_u32(value)?;

        if field_count < 5 {
            // We shouldn't call into this function in format version 1.0, as
            // there is no index count field, and format version 1.1 and up all
            // specify at least five fields.
            return Err(BinaryDeserializerError::invalid_data(
                "invalid index field count",
            ));
        }

        let (keys_end, value) = read_u32(value)?;
        let (resources_end, value) = read_u32(value)?;
        let (bundle_end, value) = read_u32(value)?;
        let (largest_table_entry_count, value) = read_u32(value)?;

        // The following fields may or may not be present, depending on format
        // version and whether or not the file is or uses a pool bundle.
        let (bundle_attributes, data_16_bit_end, pool_checksum) = if field_count >= 6 {
            let (bundle_attributes, value) = read_u32(value)?;

            let (data_16_bit_end, pool_checksum) = if field_count >= 7 {
                let (data_16_bit_end, value) = read_u32(value)?;

                let pool_checksum = if field_count >= 8 {
                    let (pool_checksum, _) = read_u32(value)?;

                    Some(pool_checksum)
                } else {
                    None
                };

                (Some(data_16_bit_end), pool_checksum)
            } else {
                (None, None)
            };

            (Some(bundle_attributes), data_16_bit_end, pool_checksum)
        } else {
            (None, None, None)
        };

        Ok(Self {
            field_count,
            keys_end,
            resources_end,
            bundle_end,
            largest_table_entry_count,
            bundle_attributes,
            data_16_bit_end,
            pool_checksum,
        })
    }
}

impl TryFrom<&[u8]> for FormatVersion {
    type Error = BinaryDeserializerError;

    fn try_from(value: &[u8]) -> Result<Self, Self::Error> {
        let value = match value {
            [1, 0, 0, 0] => FormatVersion::V1_0,
            [1, 1, 0, 0] => FormatVersion::V1_1,
            [1, 2, 0, 0] => FormatVersion::V1_2,
            [1, 3, 0, 0] => FormatVersion::V1_3,
            [2, 0, 0, 0] => FormatVersion::V2_0,
            [3, 0, 0, 0] => FormatVersion::V3_0,
            _ => {
                return Err(BinaryDeserializerError::invalid_data(
                    "unrecognized format version",
                ))
            }
        };

        Ok(value)
    }
}

impl TryFrom<u32> for ResDescriptor {
    type Error = BinaryDeserializerError;

    fn try_from(value: u32) -> Result<Self, Self::Error> {
        let resource_type = ResourceReprType::try_from((value >> 28) as u16)?;

        Ok(Self::new(resource_type, value & MASK_28_BIT))
    }
}

/// Reads the first four bytes of the input and interprets them as a `u32` with
/// native endianness.
///
/// Returns the `u32` and a slice containing all input after the interpreted
/// bytes. Returns an error if the input is of insufficient length.
fn read_u32(input: &[u8]) -> Result<(u32, &[u8]), BinaryDeserializerError> {
    // Safe to unwrap at the end of this because `try_into()` for arrays will
    // only fail if the slice is the wrong size.
    #[expect(clippy::unwrap_used)]
    let bytes = input
        .get(0..core::mem::size_of::<u32>())
        .ok_or(BinaryDeserializerError::invalid_data(
            "unexpected end of input",
        ))?
        .try_into()
        .unwrap();
    let value = u32::from_le_bytes(bytes);

    let rest =
        input
            .get(core::mem::size_of::<u32>()..)
            .ok_or(BinaryDeserializerError::invalid_data(
                "unexpected end of input",
            ))?;
    Ok((value, rest))
}
