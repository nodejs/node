use std::{collections::HashMap, path::PathBuf};

use indexmap::IndexMap;
pub use swc_config_macro::Merge;

/// Deriavable trait for overrding configurations.
///
/// Typically, correct implementation of this trait for a struct is calling
/// merge for all fields, and `#[derive(Merge)]` will do it for you.
pub trait Merge: Sized {
    /// `self` has higher priority.
    fn merge(&mut self, other: Self);
}

/// Modifies `self` iff `self` is [None]
impl<T> Merge for Option<T> {
    #[inline]
    fn merge(&mut self, other: Self) {
        if self.is_none() {
            *self = other;
        }
    }
}

impl<T> Merge for Box<T>
where
    T: Merge,
{
    #[inline]
    fn merge(&mut self, other: Self) {
        (**self).merge(*other);
    }
}

/// Modifies `self` iff `self` is empty.
impl<T> Merge for Vec<T> {
    #[inline]
    fn merge(&mut self, other: Self) {
        if self.is_empty() {
            *self = other;
        }
    }
}

/// Modifies `self` iff `self` is empty.
impl<K, V, S> Merge for HashMap<K, V, S> {
    #[inline]
    fn merge(&mut self, other: Self) {
        if self.is_empty() {
            *self = other;
        }
    }
}

/// Modifies `self` iff `self` is empty.
impl<K, V, S> Merge for IndexMap<K, V, S> {
    #[inline]
    fn merge(&mut self, other: Self) {
        if self.is_empty() {
            *self = other;
        }
    }
}

/// Modifies `self` iff `self` is empty.
impl Merge for String {
    #[inline]
    fn merge(&mut self, other: Self) {
        if self.is_empty() {
            *self = other;
        }
    }
}

/// Modifies `self` iff `self` is empty.
impl Merge for PathBuf {
    #[inline]
    fn merge(&mut self, other: Self) {
        if self.as_os_str().is_empty() {
            *self = other;
        }
    }
}
