use std::fmt;

use serde::{
    de::{Unexpected, Visitor},
    Deserialize, Deserializer, Serialize, Serializer,
};

use crate::merge::Merge;

#[derive(Clone, Debug, Copy, PartialEq, Eq, PartialOrd, Ord, Hash)]
pub enum IsModule {
    Bool(bool),
    Unknown,
    CommonJS,
}

impl Default for IsModule {
    fn default() -> Self {
        IsModule::Bool(true)
    }
}

impl Merge for IsModule {
    fn merge(&mut self, other: Self) {
        if *self == Default::default() {
            *self = other;
        }
    }
}

impl Serialize for IsModule {
    fn serialize<S>(&self, serializer: S) -> Result<S::Ok, S::Error>
    where
        S: Serializer,
    {
        match *self {
            IsModule::Bool(ref b) => b.serialize(serializer),
            IsModule::Unknown => "unknown".serialize(serializer),
            IsModule::CommonJS => "commonjs".serialize(serializer),
        }
    }
}

struct IsModuleVisitor;

impl Visitor<'_> for IsModuleVisitor {
    type Value = IsModule;

    fn expecting(&self, formatter: &mut fmt::Formatter) -> fmt::Result {
        formatter.write_str("a boolean or the string 'unknown'")
    }

    fn visit_bool<E>(self, b: bool) -> Result<Self::Value, E>
    where
        E: serde::de::Error,
    {
        Ok(IsModule::Bool(b))
    }

    fn visit_str<E>(self, s: &str) -> Result<Self::Value, E>
    where
        E: serde::de::Error,
    {
        match s {
            "unknown" => Ok(IsModule::Unknown),
            "commonjs" => Ok(IsModule::CommonJS),
            _ => Err(serde::de::Error::invalid_value(Unexpected::Str(s), &self)),
        }
    }
}

impl<'de> Deserialize<'de> for IsModule {
    fn deserialize<D>(deserializer: D) -> Result<Self, D::Error>
    where
        D: Deserializer<'de>,
    {
        deserializer.deserialize_any(IsModuleVisitor)
    }
}
