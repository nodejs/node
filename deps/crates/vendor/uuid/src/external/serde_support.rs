// Copyright 2013-2014 The Rust Project Developers.
// Copyright 2018 The Uuid Project Developers.
//
// See the COPYRIGHT file at the top-level directory of this distribution.
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

use core::marker::PhantomData;

use crate::{
    error::*,
    fmt::{Braced, Hyphenated, Simple, Urn},
    non_nil::NonNilUuid,
    std::fmt,
    Bytes, Uuid,
};
use serde_core::{
    de::{self, Error as _},
    Deserialize, Deserializer, Serialize, Serializer,
};

impl Serialize for Uuid {
    fn serialize<S: Serializer>(&self, serializer: S) -> Result<S::Ok, S::Error> {
        if serializer.is_human_readable() {
            serializer.serialize_str(self.hyphenated().encode_lower(&mut Uuid::encode_buffer()))
        } else {
            serializer.serialize_bytes(self.as_bytes())
        }
    }
}

impl Serialize for NonNilUuid {
    fn serialize<S>(&self, serializer: S) -> Result<S::Ok, S::Error>
    where
        S: serde_core::Serializer,
    {
        Uuid::from(*self).serialize(serializer)
    }
}

impl Serialize for Hyphenated {
    fn serialize<S: Serializer>(&self, serializer: S) -> Result<S::Ok, S::Error> {
        serializer.serialize_str(self.encode_lower(&mut Uuid::encode_buffer()))
    }
}

impl Serialize for Simple {
    fn serialize<S: Serializer>(&self, serializer: S) -> Result<S::Ok, S::Error> {
        serializer.serialize_str(self.encode_lower(&mut Uuid::encode_buffer()))
    }
}

impl Serialize for Urn {
    fn serialize<S: Serializer>(&self, serializer: S) -> Result<S::Ok, S::Error> {
        serializer.serialize_str(self.encode_lower(&mut Uuid::encode_buffer()))
    }
}

impl Serialize for Braced {
    fn serialize<S: Serializer>(&self, serializer: S) -> Result<S::Ok, S::Error> {
        serializer.serialize_str(self.encode_lower(&mut Uuid::encode_buffer()))
    }
}

struct UuidReadableVisitor<T> {
    expecting: &'static str,
    _marker: PhantomData<T>,
}

impl<'vi, T: UuidDeserialize> de::Visitor<'vi> for UuidReadableVisitor<T> {
    type Value = T;

    fn expecting(&self, formatter: &mut fmt::Formatter<'_>) -> fmt::Result {
        formatter.write_str(self.expecting)
    }

    fn visit_str<E: de::Error>(self, value: &str) -> Result<T, E> {
        T::from_str(value).map_err(de_error)
    }

    fn visit_bytes<E: de::Error>(self, value: &[u8]) -> Result<T, E> {
        T::from_slice(value).map_err(de_error)
    }

    fn visit_seq<A>(self, mut seq: A) -> Result<T, A::Error>
    where
        A: de::SeqAccess<'vi>,
    {
        #[rustfmt::skip]
        let bytes = [
            match seq.next_element()? { Some(e) => e, None => return Err(A::Error::invalid_length(0, &self)) },
            match seq.next_element()? { Some(e) => e, None => return Err(A::Error::invalid_length(1, &self)) },
            match seq.next_element()? { Some(e) => e, None => return Err(A::Error::invalid_length(2, &self)) },
            match seq.next_element()? { Some(e) => e, None => return Err(A::Error::invalid_length(3, &self)) },
            match seq.next_element()? { Some(e) => e, None => return Err(A::Error::invalid_length(4, &self)) },
            match seq.next_element()? { Some(e) => e, None => return Err(A::Error::invalid_length(5, &self)) },
            match seq.next_element()? { Some(e) => e, None => return Err(A::Error::invalid_length(6, &self)) },
            match seq.next_element()? { Some(e) => e, None => return Err(A::Error::invalid_length(7, &self)) },
            match seq.next_element()? { Some(e) => e, None => return Err(A::Error::invalid_length(8, &self)) },
            match seq.next_element()? { Some(e) => e, None => return Err(A::Error::invalid_length(9, &self)) },
            match seq.next_element()? { Some(e) => e, None => return Err(A::Error::invalid_length(10, &self)) },
            match seq.next_element()? { Some(e) => e, None => return Err(A::Error::invalid_length(11, &self)) },
            match seq.next_element()? { Some(e) => e, None => return Err(A::Error::invalid_length(12, &self)) },
            match seq.next_element()? { Some(e) => e, None => return Err(A::Error::invalid_length(13, &self)) },
            match seq.next_element()? { Some(e) => e, None => return Err(A::Error::invalid_length(14, &self)) },
            match seq.next_element()? { Some(e) => e, None => return Err(A::Error::invalid_length(15, &self)) },
        ];

        T::from_bytes(bytes).map_err(de_error)
    }
}

struct UuidBytesVisitor<T> {
    _marker: PhantomData<T>,
}

impl<'vi, T: UuidDeserialize> de::Visitor<'vi> for UuidBytesVisitor<T> {
    type Value = T;

    fn expecting(&self, formatter: &mut fmt::Formatter<'_>) -> fmt::Result {
        write!(formatter, "a 16 byte array")
    }

    fn visit_bytes<E: de::Error>(self, value: &[u8]) -> Result<T, E> {
        T::from_slice(value).map_err(de_error)
    }
}

fn de_error<E: de::Error>(e: Error) -> E {
    E::custom(format_args!("UUID parsing failed: {}", e))
}

trait UuidDeserialize {
    fn from_str(formatted: &str) -> Result<Self, Error>
    where
        Self: Sized;
    fn from_slice(bytes: &[u8]) -> Result<Self, Error>
    where
        Self: Sized;
    fn from_bytes(bytes: Bytes) -> Result<Self, Error>
    where
        Self: Sized;
}

impl UuidDeserialize for Uuid {
    fn from_str(formatted: &str) -> Result<Self, Error> {
        formatted.parse()
    }

    fn from_slice(bytes: &[u8]) -> Result<Self, Error> {
        Uuid::from_slice(bytes)
    }

    fn from_bytes(bytes: Bytes) -> Result<Self, Error> {
        Ok(Uuid::from_bytes(bytes))
    }
}

impl<'de> Deserialize<'de> for Uuid {
    fn deserialize<D: Deserializer<'de>>(deserializer: D) -> Result<Self, D::Error> {
        if deserializer.is_human_readable() {
            deserializer.deserialize_str(UuidReadableVisitor {
                expecting: "a formatted UUID string",
                _marker: PhantomData::<Uuid>,
            })
        } else {
            deserializer.deserialize_bytes(UuidBytesVisitor {
                _marker: PhantomData::<Uuid>,
            })
        }
    }
}

impl UuidDeserialize for Braced {
    fn from_str(formatted: &str) -> Result<Self, Error> {
        formatted.parse()
    }

    fn from_slice(bytes: &[u8]) -> Result<Self, Error> {
        Ok(Uuid::from_slice(bytes)?.into())
    }

    fn from_bytes(bytes: Bytes) -> Result<Self, Error> {
        Ok(Uuid::from_bytes(bytes).into())
    }
}

impl<'de> Deserialize<'de> for Braced {
    fn deserialize<D: Deserializer<'de>>(deserializer: D) -> Result<Self, D::Error> {
        deserializer.deserialize_str(UuidReadableVisitor {
            expecting: "a UUID string in the braced format",
            _marker: PhantomData::<Braced>,
        })
    }
}

impl UuidDeserialize for Hyphenated {
    fn from_str(formatted: &str) -> Result<Self, Error> {
        formatted.parse()
    }

    fn from_slice(bytes: &[u8]) -> Result<Self, Error> {
        Ok(Uuid::from_slice(bytes)?.into())
    }

    fn from_bytes(bytes: Bytes) -> Result<Self, Error> {
        Ok(Uuid::from_bytes(bytes).into())
    }
}

impl<'de> Deserialize<'de> for Hyphenated {
    fn deserialize<D: Deserializer<'de>>(deserializer: D) -> Result<Self, D::Error> {
        deserializer.deserialize_str(UuidReadableVisitor {
            expecting: "a UUID string in the hyphenated format",
            _marker: PhantomData::<Hyphenated>,
        })
    }
}

impl UuidDeserialize for Simple {
    fn from_str(formatted: &str) -> Result<Self, Error> {
        formatted.parse()
    }

    fn from_slice(bytes: &[u8]) -> Result<Self, Error> {
        Ok(Uuid::from_slice(bytes)?.into())
    }

    fn from_bytes(bytes: Bytes) -> Result<Self, Error> {
        Ok(Uuid::from_bytes(bytes).into())
    }
}

impl<'de> Deserialize<'de> for Simple {
    fn deserialize<D: Deserializer<'de>>(deserializer: D) -> Result<Self, D::Error> {
        deserializer.deserialize_str(UuidReadableVisitor {
            expecting: "a UUID string in the simple format",
            _marker: PhantomData::<Simple>,
        })
    }
}

impl UuidDeserialize for Urn {
    fn from_str(formatted: &str) -> Result<Self, Error> {
        formatted.parse()
    }

    fn from_slice(bytes: &[u8]) -> Result<Self, Error> {
        Ok(Uuid::from_slice(bytes)?.into())
    }

    fn from_bytes(bytes: Bytes) -> Result<Self, Error> {
        Ok(Uuid::from_bytes(bytes).into())
    }
}

impl<'de> Deserialize<'de> for Urn {
    fn deserialize<D: Deserializer<'de>>(deserializer: D) -> Result<Self, D::Error> {
        deserializer.deserialize_str(UuidReadableVisitor {
            expecting: "a UUID string in the URN format",
            _marker: PhantomData::<Urn>,
        })
    }
}

impl<'de> Deserialize<'de> for NonNilUuid {
    fn deserialize<D>(deserializer: D) -> Result<Self, D::Error>
    where
        D: serde_core::Deserializer<'de>,
    {
        let uuid = Uuid::deserialize(deserializer)?;

        NonNilUuid::try_from(uuid).map_err(|_| {
            de::Error::invalid_value(de::Unexpected::Other("nil UUID"), &"a non-nil UUID")
        })
    }
}

pub mod compact {
    //! Serialize a [`Uuid`] as a `[u8; 16]`.
    //!
    //! [`Uuid`]: ../../struct.Uuid.html

    /// Serialize from a [`Uuid`] as a `[u8; 16]`
    ///
    /// [`Uuid`]: ../../struct.Uuid.html
    pub fn serialize<S>(u: &crate::Uuid, serializer: S) -> Result<S::Ok, S::Error>
    where
        S: serde_core::Serializer,
    {
        serde_core::Serialize::serialize(u.as_bytes(), serializer)
    }

    /// Deserialize a `[u8; 16]` as a [`Uuid`]
    ///
    /// [`Uuid`]: ../../struct.Uuid.html
    pub fn deserialize<'de, D>(deserializer: D) -> Result<crate::Uuid, D::Error>
    where
        D: serde_core::Deserializer<'de>,
    {
        let bytes: [u8; 16] = serde_core::Deserialize::deserialize(deserializer)?;

        Ok(crate::Uuid::from_bytes(bytes))
    }

    #[cfg(test)]
    mod tests {
        use serde_derive::*;
        use serde_test::Configure;

        #[test]
        fn test_serialize_compact() {
            #[derive(Serialize, Debug, Deserialize, PartialEq)]
            struct UuidContainer {
                #[serde(with = "crate::serde::compact")]
                u: crate::Uuid,
            }

            let uuid_bytes = b"F9168C5E-CEB2-4F";
            let container = UuidContainer {
                u: crate::Uuid::from_slice(uuid_bytes).unwrap(),
            };

            // more complex because of the struct wrapping the actual UUID
            // serialization
            serde_test::assert_tokens(
                &container.compact(),
                &[
                    serde_test::Token::Struct {
                        name: "UuidContainer",
                        len: 1,
                    },
                    serde_test::Token::Str("u"),
                    serde_test::Token::Tuple { len: 16 },
                    serde_test::Token::U8(uuid_bytes[0]),
                    serde_test::Token::U8(uuid_bytes[1]),
                    serde_test::Token::U8(uuid_bytes[2]),
                    serde_test::Token::U8(uuid_bytes[3]),
                    serde_test::Token::U8(uuid_bytes[4]),
                    serde_test::Token::U8(uuid_bytes[5]),
                    serde_test::Token::U8(uuid_bytes[6]),
                    serde_test::Token::U8(uuid_bytes[7]),
                    serde_test::Token::U8(uuid_bytes[8]),
                    serde_test::Token::U8(uuid_bytes[9]),
                    serde_test::Token::U8(uuid_bytes[10]),
                    serde_test::Token::U8(uuid_bytes[11]),
                    serde_test::Token::U8(uuid_bytes[12]),
                    serde_test::Token::U8(uuid_bytes[13]),
                    serde_test::Token::U8(uuid_bytes[14]),
                    serde_test::Token::U8(uuid_bytes[15]),
                    serde_test::Token::TupleEnd,
                    serde_test::Token::StructEnd,
                ],
            )
        }
    }
}

/// Serialize from a [`Uuid`] as a `uuid::fmt::Simple`
///
/// [`Uuid`]: ../../struct.Uuid.html
///
/// ## Example
///
/// ```rust
/// #[derive(serde_derive::Serialize, serde_derive::Deserialize)]
/// struct StructA {
///     // This will change both serailization and deserialization
///     #[serde(with = "uuid::serde::simple")]
///     id: uuid::Uuid,
/// }
///
/// #[derive(serde_derive::Serialize, serde_derive::Deserialize)]
/// struct StructB {
///     // This will be serialized as uuid::fmt::Simple and deserialize from all valid formats
///     #[serde(serialize_with = "uuid::serde::simple::serialize")]
///     id: uuid::Uuid,
/// }
/// ```
pub mod simple {
    use super::*;

    /// Serialize from a [`Uuid`] as a `uuid::fmt::Simple`
    ///
    /// [`Uuid`]: ../../struct.Uuid.html
    ///
    /// # Example
    ///
    /// ```rust
    /// #[derive(serde_derive::Serialize)]
    /// struct Struct {
    ///     // This will be serialize as uuid::fmt::Simple
    ///     #[serde(serialize_with = "uuid::serde::simple::serialize")]
    ///     id: uuid::Uuid,
    /// }
    ///
    /// ```
    pub fn serialize<S>(u: &crate::Uuid, serializer: S) -> Result<S::Ok, S::Error>
    where
        S: serde_core::Serializer,
    {
        serde_core::Serialize::serialize(u.as_simple(), serializer)
    }

    /// Deserialize a simple Uuid string as a [`Uuid`]
    ///
    /// [`Uuid`]: ../../struct.Uuid.html
    pub fn deserialize<'de, D>(deserializer: D) -> Result<Uuid, D::Error>
    where
        D: serde_core::Deserializer<'de>,
    {
        Ok(Simple::deserialize(deserializer)?.into())
    }

    #[cfg(test)]
    mod tests {
        use serde::de::{self, Error};
        use serde_test::{Readable, Token};

        use super::*;

        const HYPHENATED_UUID_STR: &str = "f9168c5e-ceb2-4faa-b6bf-329bf39fa1e4";
        const SIMPLE_UUID_STR: &str = "f9168c5eceb24faab6bf329bf39fa1e4";

        #[test]
        fn test_serialize_as_simple() {
            #[derive(serde_derive::Serialize)]
            struct Struct(#[serde(with = "super")] crate::Uuid);

            let u = Struct(Uuid::parse_str(HYPHENATED_UUID_STR).unwrap());
            serde_test::assert_ser_tokens(
                &u,
                &[
                    Token::NewtypeStruct { name: "Struct" },
                    Token::Str(SIMPLE_UUID_STR),
                ],
            );
        }

        #[test]
        fn test_de_from_simple() {
            #[derive(PartialEq, Debug, serde_derive::Deserialize)]
            struct Struct(#[serde(with = "super")] crate::Uuid);
            let s = Struct(HYPHENATED_UUID_STR.parse().unwrap());
            serde_test::assert_de_tokens::<Struct>(
                &s,
                &[
                    Token::TupleStruct {
                        name: "Struct",
                        len: 1,
                    },
                    Token::BorrowedStr(SIMPLE_UUID_STR),
                    Token::TupleStructEnd,
                ],
            );
        }

        #[test]
        fn test_de_reject_hypenated() {
            #[derive(PartialEq, Debug, serde_derive::Deserialize)]
            struct Struct(#[serde(with = "super")] crate::Uuid);
            serde_test::assert_de_tokens_error::<Readable<Struct>>(
                &[
                    Token::TupleStruct {
                        name: "Struct",
                        len: 1,
                    },
                    Token::BorrowedStr(HYPHENATED_UUID_STR),
                    Token::TupleStructEnd,
                ],
                &format!("{}", de::value::Error::custom("UUID parsing failed: invalid group length in group 4: expected 12, found 12")),
            );
        }
    }
}

/// Serialize from a [`Uuid`] as a `uuid::fmt::Braced`
///
/// [`Uuid`]: ../../struct.Uuid.html
///
/// ## Example
///
/// ```rust
/// #[derive(serde_derive::Serialize, serde_derive::Deserialize)]
/// struct StructA {
///     // This will change both serailization and deserialization
///     #[serde(with = "uuid::serde::braced")]
///     id: uuid::Uuid,
/// }
///
/// #[derive(serde_derive::Serialize, serde_derive::Deserialize)]
/// struct StructB {
///     // This will be serialized as uuid::fmt::Urn and deserialize from all valid formats
///     #[serde(serialize_with = "uuid::serde::braced::serialize")]
///     id: uuid::Uuid,
/// }
/// ```
pub mod braced {
    use super::*;

    /// Serialize from a [`Uuid`] as a `uuid::fmt::Braced`
    ///
    /// [`Uuid`]: ../../struct.Uuid.html
    ///
    /// # Example
    ///
    /// ```rust
    /// #[derive(serde_derive::Serialize)]
    /// struct Struct {
    ///     // This will be serialize as uuid::fmt::Braced
    ///     #[serde(serialize_with = "uuid::serde::braced::serialize")]
    ///     id: uuid::Uuid,
    /// }
    ///
    /// ```
    pub fn serialize<S>(u: &crate::Uuid, serializer: S) -> Result<S::Ok, S::Error>
    where
        S: serde_core::Serializer,
    {
        serde_core::Serialize::serialize(u.as_braced(), serializer)
    }

    /// Deserialize a braced Uuid string as a [`Uuid`]
    ///
    /// [`Uuid`]: ../../struct.Uuid.html
    pub fn deserialize<'de, D>(deserializer: D) -> Result<crate::Uuid, D::Error>
    where
        D: serde_core::Deserializer<'de>,
    {
        Ok(Braced::deserialize(deserializer)?.into())
    }

    #[cfg(test)]
    mod tests {

        use serde::de::{self, Error};
        use serde_test::{Readable, Token};

        use super::*;

        const HYPHENATED_UUID_STR: &str = "f9168c5e-ceb2-4faa-b6bf-329bf39fa1e4";
        const BRACED_UUID_STR: &str = "{f9168c5e-ceb2-4faa-b6bf-329bf39fa1e4}";

        #[test]
        fn test_serialize_as_braced() {
            #[derive(serde_derive::Serialize)]
            struct Struct(#[serde(with = "super")] crate::Uuid);

            let u = Struct(Uuid::parse_str(HYPHENATED_UUID_STR).unwrap());
            serde_test::assert_ser_tokens(
                &u,
                &[
                    Token::NewtypeStruct { name: "Struct" },
                    Token::Str(BRACED_UUID_STR),
                ],
            );
        }

        #[test]
        fn test_de_from_braced() {
            #[derive(PartialEq, Debug, serde_derive::Deserialize)]
            struct Struct(#[serde(with = "super")] crate::Uuid);
            let s = Struct(HYPHENATED_UUID_STR.parse().unwrap());
            serde_test::assert_de_tokens::<Struct>(
                &s,
                &[
                    Token::TupleStruct {
                        name: "Struct",
                        len: 1,
                    },
                    Token::BorrowedStr(BRACED_UUID_STR),
                    Token::TupleStructEnd,
                ],
            );
        }

        #[test]
        fn test_de_reject_hypenated() {
            #[derive(PartialEq, Debug, serde_derive::Deserialize)]
            struct Struct(#[serde(with = "super")] crate::Uuid);
            serde_test::assert_de_tokens_error::<Readable<Struct>>(
                &[
                    Token::TupleStruct {
                        name: "Struct",
                        len: 1,
                    },
                    Token::BorrowedStr(HYPHENATED_UUID_STR),
                    Token::TupleStructEnd,
                ],
                &format!("{}", de::value::Error::custom("UUID parsing failed: invalid group length in group 4: expected 12, found 12")),
            );
        }
    }
}

/// Serialize from a [`Uuid`] as a `uuid::fmt::Urn`
///
/// [`Uuid`]: ../../struct.Uuid.html
///
/// ## Example
///
/// ```rust
/// #[derive(serde_derive::Serialize, serde_derive::Deserialize)]
/// struct StructA {
///     // This will change both serailization and deserialization
///     #[serde(with = "uuid::serde::urn")]
///     id: uuid::Uuid,
/// }
///
/// #[derive(serde_derive::Serialize, serde_derive::Deserialize)]
/// struct StructB {
///     // This will be serialized as uuid::fmt::Urn and deserialize from all valid formats
///     #[serde(serialize_with = "uuid::serde::urn::serialize")]
///     id: uuid::Uuid,
/// }
/// ```
pub mod urn {
    use super::*;

    /// Serialize from a [`Uuid`] as a `uuid::fmt::Urn`
    ///
    /// [`Uuid`]: ../../struct.Uuid.html
    ///
    /// # Example
    ///
    /// ```rust
    /// #[derive(serde_derive::Serialize)]
    /// struct Struct {
    ///     // This will be serialize as uuid::fmt::Urn
    ///     #[serde(serialize_with = "uuid::serde::urn::serialize")]
    ///     id: uuid::Uuid,
    /// }
    ///
    /// ```
    pub fn serialize<S>(u: &crate::Uuid, serializer: S) -> Result<S::Ok, S::Error>
    where
        S: serde_core::Serializer,
    {
        serde_core::Serialize::serialize(u.as_urn(), serializer)
    }

    /// Deserialize a urn Uuid string as a [`Uuid`]
    ///
    /// [`Uuid`]: ../../struct.Uuid.html
    pub fn deserialize<'de, D>(deserializer: D) -> Result<crate::Uuid, D::Error>
    where
        D: serde_core::Deserializer<'de>,
    {
        Ok(Urn::deserialize(deserializer)?.into())
    }

    #[cfg(test)]
    mod tests {
        use serde::de::{self, Error};
        use serde_test::{Readable, Token};

        use super::*;

        const HYPHENATED_UUID_STR: &str = "f9168c5e-ceb2-4faa-b6bf-329bf39fa1e4";
        const URN_UUID_STR: &str = "urn:uuid:f9168c5e-ceb2-4faa-b6bf-329bf39fa1e4";

        #[test]
        fn test_serialize_as_urn() {
            #[derive(serde_derive::Serialize)]
            struct Struct(#[serde(with = "super")] crate::Uuid);

            let u = Struct(Uuid::parse_str(HYPHENATED_UUID_STR).unwrap());
            serde_test::assert_ser_tokens(
                &u,
                &[
                    Token::NewtypeStruct { name: "Struct" },
                    Token::Str(URN_UUID_STR),
                ],
            );
        }

        #[test]
        fn test_de_from_urn() {
            #[derive(PartialEq, Debug, serde_derive::Deserialize)]
            struct Struct(#[serde(with = "super")] crate::Uuid);
            let s = Struct(HYPHENATED_UUID_STR.parse().unwrap());
            serde_test::assert_de_tokens::<Struct>(
                &s,
                &[
                    Token::TupleStruct {
                        name: "Struct",
                        len: 1,
                    },
                    Token::BorrowedStr(URN_UUID_STR),
                    Token::TupleStructEnd,
                ],
            );
        }

        #[test]
        fn test_de_reject_hypenated() {
            #[derive(PartialEq, Debug, serde_derive::Deserialize)]
            struct Struct(#[serde(with = "super")] crate::Uuid);
            serde_test::assert_de_tokens_error::<Readable<Struct>>(
                &[
                    Token::TupleStruct {
                        name: "Struct",
                        len: 1,
                    },
                    Token::BorrowedStr(HYPHENATED_UUID_STR),
                    Token::TupleStructEnd,
                ],
                &format!("{}", de::value::Error::custom("UUID parsing failed: invalid group length in group 4: expected 12, found 12")),
            );
        }
    }
}

#[cfg(test)]
mod serde_tests {
    use super::*;

    use serde_test::{Compact, Configure, Readable, Token};

    #[test]
    fn test_serialize_readable_string() {
        let uuid_str = "f9168c5e-ceb2-4faa-b6bf-329bf39fa1e4";
        let u = Uuid::parse_str(uuid_str).unwrap();
        serde_test::assert_tokens(&u.readable(), &[Token::Str(uuid_str)]);
    }

    #[test]
    fn test_deserialize_readable_compact() {
        let uuid_bytes = b"F9168C5E-CEB2-4F";
        let u = Uuid::from_slice(uuid_bytes).unwrap();

        serde_test::assert_de_tokens(
            &u.readable(),
            &[
                serde_test::Token::Tuple { len: 16 },
                serde_test::Token::U8(uuid_bytes[0]),
                serde_test::Token::U8(uuid_bytes[1]),
                serde_test::Token::U8(uuid_bytes[2]),
                serde_test::Token::U8(uuid_bytes[3]),
                serde_test::Token::U8(uuid_bytes[4]),
                serde_test::Token::U8(uuid_bytes[5]),
                serde_test::Token::U8(uuid_bytes[6]),
                serde_test::Token::U8(uuid_bytes[7]),
                serde_test::Token::U8(uuid_bytes[8]),
                serde_test::Token::U8(uuid_bytes[9]),
                serde_test::Token::U8(uuid_bytes[10]),
                serde_test::Token::U8(uuid_bytes[11]),
                serde_test::Token::U8(uuid_bytes[12]),
                serde_test::Token::U8(uuid_bytes[13]),
                serde_test::Token::U8(uuid_bytes[14]),
                serde_test::Token::U8(uuid_bytes[15]),
                serde_test::Token::TupleEnd,
            ],
        );
    }

    #[test]
    fn test_deserialize_readable_bytes() {
        let uuid_bytes = b"F9168C5E-CEB2-4F";
        let u = Uuid::from_slice(uuid_bytes).unwrap();

        serde_test::assert_de_tokens(&u.readable(), &[serde_test::Token::Bytes(uuid_bytes)]);
    }

    #[test]
    fn test_serialize_hyphenated() {
        let uuid_str = "f9168c5e-ceb2-4faa-b6bf-329bf39fa1e4";
        let u = Uuid::parse_str(uuid_str).unwrap();
        serde_test::assert_ser_tokens(&u.hyphenated(), &[Token::Str(uuid_str)]);
        serde_test::assert_de_tokens(&u.hyphenated(), &[Token::Str(uuid_str)]);
    }

    #[test]
    fn test_serialize_simple() {
        let uuid_str = "f9168c5eceb24faab6bf329bf39fa1e4";
        let u = Uuid::parse_str(uuid_str).unwrap();
        serde_test::assert_ser_tokens(&u.simple(), &[Token::Str(uuid_str)]);
        serde_test::assert_de_tokens(&u.simple(), &[Token::Str(uuid_str)]);
    }

    #[test]
    fn test_serialize_urn() {
        let uuid_str = "urn:uuid:f9168c5e-ceb2-4faa-b6bf-329bf39fa1e4";
        let u = Uuid::parse_str(uuid_str).unwrap();
        serde_test::assert_ser_tokens(&u.urn(), &[Token::Str(uuid_str)]);
        serde_test::assert_de_tokens(&u.urn(), &[Token::Str(uuid_str)]);
    }

    #[test]
    fn test_serialize_braced() {
        let uuid_str = "{f9168c5e-ceb2-4faa-b6bf-329bf39fa1e4}";
        let u = Uuid::parse_str(uuid_str).unwrap();
        serde_test::assert_ser_tokens(&u.braced(), &[Token::Str(uuid_str)]);
        serde_test::assert_de_tokens(&u.braced(), &[Token::Str(uuid_str)]);
    }

    #[test]
    fn test_serialize_non_human_readable() {
        let uuid_bytes = b"F9168C5E-CEB2-4F";
        let u = Uuid::from_slice(uuid_bytes).unwrap();
        serde_test::assert_tokens(
            &u.compact(),
            &[serde_test::Token::Bytes(&[
                70, 57, 49, 54, 56, 67, 53, 69, 45, 67, 69, 66, 50, 45, 52, 70,
            ])],
        );
    }

    #[test]
    fn test_de_failure() {
        serde_test::assert_de_tokens_error::<Readable<Uuid>>(
            &[Token::Str("hello_world")],
            "UUID parsing failed: invalid character: expected an optional prefix of `urn:uuid:` followed by [0-9a-fA-F-], found `h` at 1",
        );

        serde_test::assert_de_tokens_error::<Compact<Uuid>>(
            &[Token::Bytes(b"hello_world")],
            "UUID parsing failed: invalid length: expected 16 bytes, found 11",
        );
    }

    #[test]
    fn test_serde_non_nil_uuid() {
        let uuid_str = "f9168c5e-ceb2-4faa-b6bf-329bf39fa1e4";
        let uuid = Uuid::parse_str(uuid_str).unwrap();
        let non_nil_uuid = NonNilUuid::try_from(uuid).unwrap();

        serde_test::assert_ser_tokens(&non_nil_uuid.readable(), &[Token::Str(uuid_str)]);
        serde_test::assert_de_tokens(&non_nil_uuid.readable(), &[Token::Str(uuid_str)]);
    }
}
