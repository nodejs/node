use serde::{Deserialize, Serialize};

use crate::merge::Merge;

/// You can create this type like `Some(...).into()` or `None.into()`.
#[derive(Debug, Default, Clone, Copy, PartialEq, Eq, Hash, Serialize, Deserialize)]
pub struct MergingOption<T>(Option<T>)
where
    T: Merge + Default;

impl<T> MergingOption<T>
where
    T: Merge + Default,
{
    pub fn into_inner(self) -> Option<T> {
        self.0
    }

    pub fn as_ref(&self) -> Option<&T> {
        self.0.as_ref()
    }
}

impl<T> From<Option<T>> for MergingOption<T>
where
    T: Merge + Default,
{
    #[inline]
    fn from(v: Option<T>) -> Self {
        Self(v)
    }
}

impl<T> Merge for MergingOption<T>
where
    T: Merge + Default,
{
    fn merge(&mut self, other: Self) {
        if let Some(other) = other.0 {
            if self.0.is_none() {
                self.0 = Some(Default::default());
            }

            self.0.as_mut().unwrap().merge(other);
        }
    }
}
