// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

//! This module contains helpers for zero-copy deserialization of slices other than `&[u8]`.

use potential_utf::PotentialUtf16;
use serde_core::de::*;

/// TODO
pub fn option_utf_16<'de, D: Deserializer<'de>>(
    deserializer: D,
) -> Result<Option<&'de PotentialUtf16>, D::Error> {
    let Some(bytes) = <Option<&[u8]>>::deserialize(deserializer)? else {
        return Ok(None);
    };
    // Safety: all byte representations are valid u16s
    unsafe { cast_bytes_to_slice(bytes) }
        .map(PotentialUtf16::from_slice)
        .map(Some)
}

/// TODO
pub fn vec_utf_16<'de, D: Deserializer<'de>>(
    deserializer: D,
) -> Result<alloc::vec::Vec<&'de PotentialUtf16>, D::Error> {
    struct Utf16Visitor;

    impl<'de> Visitor<'de> for Utf16Visitor {
        type Value = alloc::vec::Vec<&'de PotentialUtf16>;

        fn expecting(&self, formatter: &mut alloc::fmt::Formatter) -> alloc::fmt::Result {
            write!(formatter, "a sequence of UTF-16 slices")
        }

        fn visit_seq<A>(self, mut seq: A) -> Result<Self::Value, A::Error>
        where
            A: SeqAccess<'de>,
        {
            let mut vec = alloc::vec::Vec::with_capacity(seq.size_hint().unwrap_or_default());
            while let Some(bytes) = seq.next_element::<&[u8]>()? {
                vec.push(PotentialUtf16::from_slice(
                    // Safety: all byte representations are valid u16s
                    unsafe { cast_bytes_to_slice(bytes) }?,
                ));
            }
            Ok(vec)
        }
    }

    deserializer.deserialize_seq(Utf16Visitor)
}

/// TODO
pub fn option_i32<'de, D: Deserializer<'de>>(
    deserializer: D,
) -> Result<Option<&'de [i32]>, D::Error> {
    let Some(bytes) = <Option<&[u8]>>::deserialize(deserializer)? else {
        return Ok(None);
    };
    // Safety: all byte representations are valid i32s
    unsafe { cast_bytes_to_slice(bytes) }.map(Some)
}

/// TODO
pub fn option_u32<'de, D: Deserializer<'de>>(
    deserializer: D,
) -> Result<Option<&'de [u32]>, D::Error> {
    let Some(bytes) = <Option<&[u8]>>::deserialize(deserializer)? else {
        return Ok(None);
    };
    // Safety: all byte representations are valid u32s
    unsafe { cast_bytes_to_slice(bytes) }.map(Some)
}

/// TODO
pub fn i32_tuple<'de, D: Deserializer<'de>>(
    deserializer: D,
) -> Result<&'de [(i32, i32)], D::Error> {
    let bytes = <&[u8]>::deserialize(deserializer)?;
    // Safety: all byte representations are valid (i32, i32)
    unsafe { cast_bytes_to_slice(bytes) }
}

/// TODO
#[expect(clippy::type_complexity)] // serde...
pub fn option_i32_tuple<'de, D: Deserializer<'de>>(
    deserializer: D,
) -> Result<Option<&'de [(i32, i32)]>, D::Error> {
    let Some(bytes) = <Option<&[u8]>>::deserialize(deserializer)? else {
        return Ok(None);
    };
    // Safety: all byte representations are valid (i32, i32)
    unsafe { cast_bytes_to_slice(bytes) }.map(Some)
}

/// Casts a slice of byte to a slice of T
///
/// # Safety
/// Alignment and length are checked, however the caller has to guarantee that the byte representation is valid for type T.
pub unsafe fn cast_bytes_to_slice<T, E: serde_core::de::Error>(bytes: &[u8]) -> Result<&[T], E> {
    if bytes.as_ptr().align_offset(core::mem::align_of::<T>()) != 0
        && bytes.len() % core::mem::size_of::<T>() != 0
    {
        return Err(E::custom("Wrong length or align"));
    }

    // Safety: The check gurantees length and alignment
    Ok(unsafe {
        core::slice::from_raw_parts(
            bytes.as_ptr() as *const T,
            bytes.len() / core::mem::size_of::<T>(),
        )
    })
}
