// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

use crate::LanguageIdentifier;
use serde::{Deserialize, Deserializer, Serialize, Serializer};

impl Serialize for LanguageIdentifier {
    fn serialize<S>(&self, serializer: S) -> Result<S::Ok, S::Error>
    where
        S: Serializer,
    {
        serializer.serialize_str(&self.to_string())
    }
}

impl<'de> Deserialize<'de> for LanguageIdentifier {
    fn deserialize<D>(deserializer: D) -> Result<Self, D::Error>
    where
        D: Deserializer<'de>,
    {
        struct LanguageIdentifierVisitor;

        impl serde::de::Visitor<'_> for LanguageIdentifierVisitor {
            type Value = LanguageIdentifier;

            fn expecting(&self, formatter: &mut core::fmt::Formatter<'_>) -> core::fmt::Result {
                write!(formatter, "a valid Unicode Language Identifier")
            }

            fn visit_str<E>(self, s: &str) -> Result<Self::Value, E>
            where
                E: serde::de::Error,
            {
                s.parse::<LanguageIdentifier>()
                    .map_err(serde::de::Error::custom)
            }
        }

        deserializer.deserialize_string(LanguageIdentifierVisitor)
    }
}

#[test]
fn json() {
    use crate::langid;
    use crate::subtags::{Language, Region, Script};

    assert_eq!(
        serde_json::to_string(&langid!("en-US")).unwrap(),
        r#""en-US""#
    );
    assert_eq!(
        serde_json::from_str::<LanguageIdentifier>(r#""en-US""#).unwrap(),
        langid!("en-US")
    );
    assert!(serde_json::from_str::<LanguageIdentifier>(r#""2Xs""#).is_err());

    assert_eq!(
        serde_json::to_string(&"fr".parse::<Language>().unwrap()).unwrap(),
        r#""fr""#
    );
    assert_eq!(
        serde_json::from_str::<Language>(r#""fr""#).unwrap(),
        "fr".parse::<Language>().unwrap()
    );
    assert!(serde_json::from_str::<Language>(r#""2Xs""#).is_err());

    assert_eq!(
        serde_json::to_string(&"Latn".parse::<Script>().unwrap()).unwrap(),
        r#""Latn""#
    );
    assert_eq!(
        serde_json::from_str::<Script>(r#""Latn""#).unwrap(),
        "Latn".parse::<Script>().unwrap()
    );
    assert!(serde_json::from_str::<Script>(r#""2Xs""#).is_err());

    assert_eq!(
        serde_json::to_string(&"US".parse::<Region>().unwrap()).unwrap(),
        r#""US""#
    );
    assert_eq!(
        serde_json::from_str::<Region>(r#""US""#).unwrap(),
        "US".parse::<Region>().unwrap()
    );
    assert!(serde_json::from_str::<Region>(r#""2Xs""#).is_err());
}

#[test]
fn postcard() {
    use crate::langid;
    use crate::subtags::{Language, Region, Script};

    assert_eq!(
        postcard::to_stdvec(&langid!("en-US")).unwrap(),
        &[5, b'e', b'n', b'-', b'U', b'S']
    );
    assert_eq!(
        postcard::from_bytes::<LanguageIdentifier>(&[5, b'e', b'n', b'-', b'U', b'S']).unwrap(),
        langid!("en-US")
    );
    assert!(postcard::from_bytes::<LanguageIdentifier>(&[3, b'2', b'X', b's']).is_err());

    assert_eq!(
        postcard::to_stdvec(&"fr".parse::<Language>().unwrap()).unwrap(),
        b"fr\0"
    );
    assert_eq!(
        postcard::from_bytes::<Language>(b"fr\0").unwrap(),
        "fr".parse::<Language>().unwrap()
    );
    assert!(postcard::from_bytes::<Language>(b"2Xs").is_err());

    assert_eq!(
        postcard::to_stdvec(&"Latn".parse::<Script>().unwrap()).unwrap(),
        b"Latn"
    );
    assert_eq!(
        postcard::from_bytes::<Script>(b"Latn").unwrap(),
        "Latn".parse::<Script>().unwrap()
    );
    assert!(postcard::from_bytes::<Script>(b"2Xss").is_err());

    assert_eq!(
        postcard::to_stdvec(&"US".parse::<Region>().unwrap()).unwrap(),
        b"US\0"
    );
    assert_eq!(
        postcard::from_bytes::<Region>(b"US\0").unwrap(),
        "US".parse::<Region>().unwrap()
    );
    assert!(postcard::from_bytes::<Region>(b"2Xs").is_err());
}
