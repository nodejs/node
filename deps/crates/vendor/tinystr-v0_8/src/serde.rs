// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

use crate::TinyAsciiStr;
use core::fmt;
use core::marker::PhantomData;
use core::ops::Deref;
use serde_core::de::{Error, SeqAccess, Visitor};
use serde_core::ser::SerializeTuple;
use serde_core::{Deserialize, Deserializer, Serialize, Serializer};

impl<const N: usize> Serialize for TinyAsciiStr<N> {
    #[inline]
    fn serialize<S>(&self, serializer: S) -> Result<S::Ok, S::Error>
    where
        S: Serializer,
    {
        if serializer.is_human_readable() {
            self.deref().serialize(serializer)
        } else {
            let mut seq = serializer.serialize_tuple(N)?;
            for byte in self.all_bytes() {
                seq.serialize_element(byte)?;
            }
            seq.end()
        }
    }
}

struct TinyAsciiStrVisitor<const N: usize> {
    marker: PhantomData<TinyAsciiStr<N>>,
}

impl<const N: usize> TinyAsciiStrVisitor<N> {
    fn new() -> Self {
        TinyAsciiStrVisitor {
            marker: PhantomData,
        }
    }
}

impl<'de, const N: usize> Visitor<'de> for TinyAsciiStrVisitor<N> {
    type Value = TinyAsciiStr<N>;

    fn expecting(&self, formatter: &mut fmt::Formatter) -> fmt::Result {
        write!(formatter, "a TinyAsciiStr<{N}>")
    }

    #[inline]
    fn visit_seq<A>(self, mut seq: A) -> Result<Self::Value, A::Error>
    where
        A: SeqAccess<'de>,
    {
        let mut bytes = [0u8; N];
        let mut zeroes = false;
        for out in &mut bytes.iter_mut().take(N) {
            let byte = seq
                .next_element()?
                .ok_or_else(|| Error::invalid_length(N, &self))?;
            if byte == 0 {
                zeroes = true;
            } else if zeroes {
                return Err(Error::custom("TinyAsciiStr cannot contain null bytes"));
            }

            if byte >= 0x80 {
                return Err(Error::custom("TinyAsciiStr cannot contain non-ascii bytes"));
            }
            *out = byte;
        }

        Ok(unsafe { TinyAsciiStr::from_utf8_unchecked(bytes) })
    }
}

impl<'de, const N: usize> Deserialize<'de> for TinyAsciiStr<N> {
    fn deserialize<D>(deserializer: D) -> Result<Self, D::Error>
    where
        D: Deserializer<'de>,
    {
        if deserializer.is_human_readable() {
            struct HumanVisitor<const N: usize>;
            impl<'de, const M: usize> Visitor<'de> for HumanVisitor<M> {
                type Value = TinyAsciiStr<M>;

                fn expecting(&self, formatter: &mut fmt::Formatter) -> fmt::Result {
                    write!(formatter, "a TinyAsciiStr<{M}>")
                }

                fn visit_str<E>(self, v: &str) -> Result<Self::Value, E>
                where
                    E: Error,
                {
                    TinyAsciiStr::try_from_str(v).map_err(|_| Error::custom("invalid str"))
                }
            }
            deserializer.deserialize_str(HumanVisitor::<N>)
        } else {
            deserializer.deserialize_tuple(N, TinyAsciiStrVisitor::<N>::new())
        }
    }
}
