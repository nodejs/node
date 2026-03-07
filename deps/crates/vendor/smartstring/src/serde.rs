// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

use crate::{SmartString, SmartStringMode};
use alloc::string::String;
use core::{fmt, marker::PhantomData};

use serde::{
    de::{Error, Visitor},
    Deserialize, Deserializer, Serialize, Serializer,
};

impl<T: SmartStringMode> Serialize for SmartString<T> {
    fn serialize<S>(&self, serializer: S) -> Result<S::Ok, S::Error>
    where
        S: Serializer,
    {
        serializer.serialize_str(self)
    }
}

impl<'de, T: SmartStringMode> Deserialize<'de> for SmartString<T> {
    fn deserialize<D>(deserializer: D) -> Result<Self, D::Error>
    where
        D: Deserializer<'de>,
    {
        deserializer
            .deserialize_string(SmartStringVisitor(PhantomData))
            .map(SmartString::from)
    }
}

struct SmartStringVisitor<T: SmartStringMode>(PhantomData<*const T>);

impl<'de, T: SmartStringMode> Visitor<'de> for SmartStringVisitor<T> {
    type Value = SmartString<T>;

    fn expecting(&self, formatter: &mut fmt::Formatter<'_>) -> fmt::Result {
        formatter.write_str("a string")
    }

    fn visit_string<E>(self, v: String) -> Result<Self::Value, E>
    where
        E: Error,
    {
        Ok(SmartString::from(v))
    }

    fn visit_str<E>(self, v: &str) -> Result<Self::Value, E>
    where
        E: Error,
    {
        Ok(SmartString::from(v))
    }
}

#[cfg(test)]
mod test {
    use super::*;
    use crate::Compact;

    #[test]
    fn test_ser_de() {
        use serde_test::{assert_tokens, Token};

        let strings = [
            "",
            "small test",
            "longer than inline string for serde testing",
        ];

        for &string in strings.iter() {
            let value = SmartString::<Compact>::from(string);
            assert_tokens(&value, &[Token::String(string)]);
        }
    }
}
