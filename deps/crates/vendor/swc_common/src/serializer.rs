#![allow(deprecated)]

use serde::Deserialize;

#[derive(Deserialize)]
#[deprecated = "Not used by swc, and this will be removed with next breaking change"]
pub struct Node<T> {
    #[serde(default, rename = "type")]
    pub ty: String,
    #[serde(flatten)]
    pub node: T,
}

#[derive(Deserialize)]
pub struct Type {
    #[serde(rename = "type")]
    pub ty: String,
}

#[cfg(feature = "encoding-impl")]
pub struct WithChar<T>(pub T);

#[cfg(feature = "encoding-impl")]
impl cbor4ii::core::enc::Encode for WithChar<&char> {
    fn encode<W: cbor4ii::core::enc::Write>(
        &self,
        writer: &mut W,
    ) -> Result<(), cbor4ii::core::enc::Error<W::Error>> {
        u32::from(*self.0).encode(writer)
    }
}

#[cfg(feature = "encoding-impl")]
impl<'de> cbor4ii::core::dec::Decode<'de> for WithChar<char> {
    fn decode<R: cbor4ii::core::dec::Read<'de>>(
        reader: &mut R,
    ) -> Result<Self, cbor4ii::core::dec::Error<R::Error>> {
        let n = u32::decode(reader)?;
        let value = char::from_u32(n).ok_or_else(|| cbor4ii::core::dec::Error::Mismatch {
            name: &"WithChar",
            found: 0,
        })?;
        Ok(WithChar(value))
    }
}

#[cfg(feature = "encoding-impl")]
pub struct ArrayOption<T>(pub T);

#[cfg(feature = "encoding-impl")]
impl<T: cbor4ii::core::enc::Encode> cbor4ii::core::enc::Encode for ArrayOption<&'_ Vec<Option<T>>> {
    fn encode<W: cbor4ii::core::enc::Write>(
        &self,
        writer: &mut W,
    ) -> Result<(), cbor4ii::core::enc::Error<W::Error>> {
        <cbor4ii::core::types::Array<()>>::bounded(self.0.len(), writer)?;
        for item in self.0.iter() {
            cbor4ii::core::types::Maybe(item).encode(writer)?;
        }
        Ok(())
    }
}

#[cfg(feature = "encoding-impl")]
impl<'de, T: cbor4ii::core::dec::Decode<'de>> cbor4ii::core::dec::Decode<'de>
    for ArrayOption<Vec<Option<T>>>
{
    fn decode<R: cbor4ii::core::dec::Read<'de>>(
        reader: &mut R,
    ) -> Result<Self, cbor4ii::core::dec::Error<R::Error>> {
        let len = <cbor4ii::core::types::Array<()>>::len(reader)?;
        let Some(len) = len else {
            return Err(cbor4ii::core::error::DecodeError::RequireLength {
                name: &"array-option",
                found: len
                    .map(cbor4ii::core::error::Len::new)
                    .unwrap_or(cbor4ii::core::error::Len::Indefinite),
            });
        };
        let mut q = Vec::with_capacity(std::cmp::min(len, 128));
        for _ in 0..len {
            q.push(<cbor4ii::core::types::Maybe<Option<T>>>::decode(reader)?.0);
        }
        Ok(ArrayOption(q))
    }
}
