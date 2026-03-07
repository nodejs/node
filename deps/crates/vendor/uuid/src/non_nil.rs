//! A wrapper type for nil UUIDs that provides a more memory-efficient
//! `Option<NonNilUuid>` representation.

use std::{cmp::Ordering, fmt, num::NonZeroU128};

use crate::{
    error::{Error, ErrorKind},
    Uuid,
};

/// A UUID that is guaranteed not to be the [nil UUID](https://www.ietf.org/rfc/rfc9562.html#name-nil-uuid).
///
/// This is useful for representing optional UUIDs more efficiently, as `Option<NonNilUuid>`
/// takes up the same space as `Uuid`.
///
/// Note that `Uuid`s created by the following methods are guaranteed to be non-nil:
///
/// - [`Uuid::new_v1`]
/// - [`Uuid::now_v1`]
/// - [`Uuid::new_v3`]
/// - [`Uuid::new_v4`]
/// - [`Uuid::new_v5`]
/// - [`Uuid::new_v6`]
/// - [`Uuid::now_v6`]
/// - [`Uuid::new_v7`]
/// - [`Uuid::now_v7`]
/// - [`Uuid::new_v8`]
///
/// # ABI
///
/// The `NonNilUuid` type does not yet have a stable ABI. Its representation or alignment
/// may change. It is currently only guaranteed that `NonNilUuid` and `Option<NonNilUuid>`
/// are the same size as `Uuid`.
#[repr(transparent)]
#[derive(Copy, Clone, PartialEq, Eq, Hash, PartialOrd, Ord)]
pub struct NonNilUuid(NonZeroU128);

impl fmt::Debug for NonNilUuid {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        fmt::Debug::fmt(&Uuid::from(*self), f)
    }
}

impl fmt::Display for NonNilUuid {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        fmt::Display::fmt(&Uuid::from(*self), f)
    }
}

impl PartialEq<Uuid> for NonNilUuid {
    fn eq(&self, other: &Uuid) -> bool {
        self.get() == *other
    }
}

impl PartialEq<NonNilUuid> for Uuid {
    fn eq(&self, other: &NonNilUuid) -> bool {
        *self == other.get()
    }
}

impl PartialOrd<Uuid> for NonNilUuid {
    fn partial_cmp(&self, other: &Uuid) -> Option<Ordering> {
        self.get().partial_cmp(other)
    }
}

impl PartialOrd<NonNilUuid> for Uuid {
    fn partial_cmp(&self, other: &NonNilUuid) -> Option<Ordering> {
        self.partial_cmp(&other.get())
    }
}

impl NonNilUuid {
    /// Creates a non-nil UUID if the value is non-nil.
    pub const fn new(uuid: Uuid) -> Option<Self> {
        match NonZeroU128::new(uuid.as_u128()) {
            Some(non_nil) => Some(NonNilUuid(non_nil)),
            None => None,
        }
    }

    /// Creates a non-nil without checking whether the value is non-nil. This results in undefined behavior if the value is nil.
    ///
    /// # Safety
    ///
    /// The value must not be nil.
    pub const unsafe fn new_unchecked(uuid: Uuid) -> Self {
        NonNilUuid(unsafe { NonZeroU128::new_unchecked(uuid.as_u128()) })
    }

    /// Get the underlying [`Uuid`] value.
    #[inline]
    pub const fn get(self) -> Uuid {
        Uuid::from_u128(self.0.get())
    }
}

impl From<NonNilUuid> for Uuid {
    /// Converts a [`NonNilUuid`] back into a [`Uuid`].
    ///
    /// # Examples
    /// ```
    /// # use uuid::{NonNilUuid, Uuid};
    /// let uuid = Uuid::from_u128(0x0123456789abcdef0123456789abcdef);
    /// let non_nil = NonNilUuid::try_from(uuid).unwrap();
    /// let uuid_again = Uuid::from(non_nil);
    ///
    /// assert_eq!(uuid, uuid_again);
    /// ```
    fn from(non_nil: NonNilUuid) -> Self {
        Uuid::from_u128(non_nil.0.get())
    }
}

impl TryFrom<Uuid> for NonNilUuid {
    type Error = Error;

    /// Attempts to convert a [`Uuid`] into a [`NonNilUuid`].
    ///
    /// # Examples
    /// ```
    /// # use uuid::{NonNilUuid, Uuid};
    /// let uuid = Uuid::from_u128(0x0123456789abcdef0123456789abcdef);
    /// let non_nil = NonNilUuid::try_from(uuid).unwrap();
    /// ```
    fn try_from(uuid: Uuid) -> Result<Self, Self::Error> {
        NonZeroU128::new(uuid.as_u128())
            .map(Self)
            .ok_or(Error(ErrorKind::Nil))
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_non_nil_with_option_size() {
        assert_eq!(
            std::mem::size_of::<Option<NonNilUuid>>(),
            std::mem::size_of::<Uuid>()
        );
    }

    #[test]
    fn test_non_nil() {
        let uuid = Uuid::from_u128(0x0123456789abcdef0123456789abcdef);

        assert_eq!(Uuid::from(NonNilUuid::try_from(uuid).unwrap()), uuid);
        assert_eq!(NonNilUuid::new(uuid).unwrap(), uuid);
        assert_eq!(unsafe { NonNilUuid::new_unchecked(uuid) }, uuid);

        assert!(NonNilUuid::try_from(Uuid::nil()).is_err());
        assert!(NonNilUuid::new(Uuid::nil()).is_none());
    }

    #[test]
    fn test_non_nil_formatting() {
        let uuid = Uuid::from_u128(0x0123456789abcdef0123456789abcdef);
        let non_nil = NonNilUuid::try_from(uuid).unwrap();

        assert_eq!(format!("{uuid}"), format!("{non_nil}"));
        assert_eq!(format!("{uuid:?}"), format!("{non_nil:?}"));
    }

    #[test]
    fn test_non_nil_ord() {
        let uuid1 = Uuid::from_u128(0x0123456789abcdef0123456789abcdef);
        let uuid2 = Uuid::from_u128(0x0123456789abcdef0123456789abcdf0);

        let non_nil1 = NonNilUuid::try_from(uuid1).unwrap();
        let non_nil2 = NonNilUuid::try_from(uuid2).unwrap();

        // Ordering between NonNilUuid instances
        assert!(non_nil1 < non_nil2);
        assert!(non_nil2 > non_nil1);

        // Ordering between NonNilUuid and Uuid
        assert!(non_nil1 < uuid2);
        assert!(non_nil2 > uuid1);

        // Ordering between Uuid and NonNilUuid
        assert!(uuid1 < non_nil2);
        assert!(uuid2 > non_nil1);

        // Equality
        let non_nil1_copy = NonNilUuid::try_from(uuid1).unwrap();
        assert_eq!(non_nil1, non_nil1_copy);
        assert!(non_nil1 <= non_nil1_copy);
        assert!(non_nil1 >= non_nil1_copy);

        // Equality between NonNilUuid and Uuid
        assert_eq!(non_nil1, uuid1);
        assert_eq!(uuid1, non_nil1);
        assert!(non_nil1 >= uuid1);
        assert!(non_nil1 <= uuid1);
        assert!(uuid1 >= non_nil1);
        assert!(uuid1 <= non_nil1);
    }
}
