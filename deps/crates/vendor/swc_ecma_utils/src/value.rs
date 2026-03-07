use std::ops::Not;

use self::Value::{Known, Unknown};

/// Runtime value.
#[derive(Debug, Clone, Copy, PartialEq, Eq, PartialOrd, Ord, Hash)]
pub enum Value<T> {
    Known(T),
    /// Not determined at compile time.`
    Unknown,
}

/// Type of value.
#[derive(Debug, Clone, Copy, PartialEq, Eq, PartialOrd, Ord, Hash)]
pub enum Type {
    Undefined,
    Null,
    Bool,
    Str,
    Symbol,
    Num,
    Obj,
}

impl Value<Type> {
    pub fn casted_to_number_on_add(self) -> bool {
        match self {
            Known(Type::Bool) | Known(Type::Null) | Known(Type::Num) | Known(Type::Undefined) => {
                true
            }
            _ => false,
        }
    }
}

/// Value could not be determined
pub struct UnknownError;

// impl<T> Try for Value<T> {
//     type Ok = T;
//     type Error = UnknownError;
//     fn from_ok(t: T) -> Self {
//         Known(t)
//     }
//     fn from_error(_: UnknownError) -> Self {
//         Unknown
//     }
//     fn into_result(self) -> Result<T, UnknownError> {
//         match self {
//             Known(t) => Ok(t),
//             Unknown => Err(UnknownError),
//         }
//     }
// }

impl<T> Value<T> {
    pub fn into_result(self) -> Result<T, UnknownError> {
        match self {
            Known(v) => Ok(v),
            Unknown => Err(UnknownError),
        }
    }
}

impl<T> Value<T> {
    /// Returns true if the value is not known.
    pub fn is_unknown(&self) -> bool {
        matches!(*self, Unknown)
    }

    /// Returns true if the value is known.
    pub fn is_known(&self) -> bool {
        matches!(*self, Known(..))
    }
}

impl Value<bool> {
    pub fn and(self, other: Self) -> Self {
        match self {
            Known(true) => other,
            Known(false) => Known(false),
            Unknown => match other {
                Known(false) => Known(false),
                _ => Unknown,
            },
        }
    }

    pub fn or(self, other: Self) -> Self {
        match self {
            Known(true) => Known(true),
            Known(false) => other,
            Unknown => match other {
                Known(true) => Known(true),
                _ => Unknown,
            },
        }
    }
}

impl Not for Value<bool> {
    type Output = Self;

    fn not(self) -> Self {
        match self {
            Value::Known(b) => Value::Known(!b),
            Value::Unknown => Value::Unknown,
        }
    }
}

pub trait Merge {
    fn merge(&mut self, rhs: Self);
}

impl Merge for Option<Value<Type>> {
    fn merge(&mut self, rhs: Self) {
        *self = match (*self, rhs) {
            (None, None) => None,
            (None, Some(ty)) | (Some(ty), None) => Some(ty),
            (Some(ty1), Some(ty2)) if ty1 == ty2 => Some(ty1),
            _ => Some(Unknown),
        }
    }
}
