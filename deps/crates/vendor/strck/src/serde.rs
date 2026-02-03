//! Implement [`Serialize`] and [`Deserialize`] for [`Check`] and [`Ck`].

use crate::{Check, Ck, Invariant};
use serde::de::{Deserialize, Deserializer, Error};
use serde::ser::{Serialize, Serializer};

impl<I: Invariant, B: AsRef<str>> Serialize for Check<I, B> {
    fn serialize<S>(&self, serializer: S) -> Result<S::Ok, S::Error>
    where
        S: Serializer,
    {
        serializer.serialize_str(self.as_str())
    }
}

impl<I: Invariant> Serialize for Ck<I> {
    fn serialize<S>(&self, serializer: S) -> Result<S::Ok, S::Error>
    where
        S: Serializer,
    {
        serializer.serialize_str(self.as_str())
    }
}

impl<'de, I, B> Deserialize<'de> for Check<I, B>
where
    I: Invariant,
    B: AsRef<str> + Deserialize<'de>,
{
    fn deserialize<D>(deserializer: D) -> Result<Self, D::Error>
    where
        D: Deserializer<'de>,
    {
        let buf = Deserialize::deserialize(deserializer)?;

        Check::from_buf(buf).map_err(Error::custom)
    }
}

impl<'de: 'a, 'a, I: Invariant> Deserialize<'de> for &'a Ck<I> {
    fn deserialize<D>(deserializer: D) -> Result<Self, D::Error>
    where
        D: Deserializer<'de>,
    {
        let slice = Deserialize::deserialize(deserializer)?;

        Ck::from_slice(slice).map_err(Error::custom)
    }
}

#[cfg(all(test))]
mod tests {
    use crate::{ident::unicode::Ident, IntoCk};
    use serde::{Deserialize, Serialize};

    #[derive(Serialize, Deserialize, Debug, PartialEq)]
    struct Player<'a> {
        #[serde(borrow)]
        username: &'a Ident,
        level: u32,
    }

    #[test]
    fn test_zero_copy_deserialization() {
        let qnn = Player {
            username: "qnn".ck().unwrap(),
            level: 100,
        };

        fn get() -> Player<'static> {
            let ser = r#"{"username":"qnn","level":100}"#;
            serde_json::from_str(ser).unwrap()
        }

        let de = get();
        assert_eq!(qnn, de);
    }
}
