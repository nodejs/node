use serde::{Deserialize, Serialize};

use crate::merge::Merge;

/// You can create this type like `true.into()` or `false.into()`.
#[derive(
    Debug, Default, Clone, Copy, PartialEq, Eq, PartialOrd, Ord, Hash, Serialize, Deserialize,
)]
pub struct BoolConfig<const DEFAULT: bool>(#[serde(default)] Option<bool>);

impl<const DEFAULT: bool> BoolConfig<DEFAULT> {
    /// Creates a new `BoolConfig` with the given value.
    #[inline]
    pub fn new(value: Option<bool>) -> Self {
        Self(value)
    }

    /// Returns the value specified by the user or the default value.
    #[inline]
    pub fn into_bool(self) -> bool {
        self.into()
    }
}

impl<const DEFAULT: bool> From<BoolConfig<DEFAULT>> for bool {
    #[inline]
    fn from(v: BoolConfig<DEFAULT>) -> Self {
        match v.0 {
            Some(v) => v,
            _ => DEFAULT,
        }
    }
}

impl<const DEFAULT: bool> From<Option<bool>> for BoolConfig<DEFAULT> {
    #[inline]
    fn from(v: Option<bool>) -> Self {
        Self(v)
    }
}

impl<const DEFAULT: bool> From<bool> for BoolConfig<DEFAULT> {
    #[inline]
    fn from(v: bool) -> Self {
        Self(Some(v))
    }
}

impl<const DEFAULT: bool> Merge for BoolConfig<DEFAULT> {
    #[inline]
    fn merge(&mut self, other: Self) {
        self.0.merge(other.0);
    }
}
