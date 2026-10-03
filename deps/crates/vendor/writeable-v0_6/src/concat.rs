// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

use crate::TryWriteable;
use crate::Writeable;
use core::fmt;

/// A [`Writeable`] that efficiently concatenates two other [`Writeable`]s.
///
/// See the [`concat_writeable!`] macro for a convenient way to make one of these.
///
/// # Examples
///
/// ```
/// use writeable::adapters::Concat;
/// use writeable::assert_writeable_eq;
///
/// assert_writeable_eq!(Concat("Number: ", 25), "Number: 25");
/// ```
///
/// With [`TryWriteable`]:
///
/// ```
/// use writeable::adapters::Concat;
/// use writeable::assert_try_writeable_eq;
/// use writeable::TryWriteable;
///
/// struct AlwaysPanic;
///
/// impl TryWriteable for AlwaysPanic {
///     type Error = &'static str;
///     fn try_write_to_parts<W: writeable::PartsWrite + ?Sized>(
///         &self,
///         _sink: &mut W,
///     ) -> Result<Result<(), Self::Error>, core::fmt::Error> {
///         // Unreachable panic: the first Writeable errors,
///         // so the second Writeable is not evaluated.
///         panic!("this is a test to demonstrate unreachable code")
///     }
/// }
///
/// let writeable0: Result<&str, &str> = Err("error message");
/// let writeable1 = AlwaysPanic;
///
/// assert_try_writeable_eq!(
///     Concat(writeable0, writeable1),
///     "error message",
///     Err("error message"),
/// )
/// ```
#[derive(Debug)]
#[allow(clippy::exhaustive_structs)] // designed for nesting
pub struct Concat<A, B>(pub A, pub B);

impl<A, B> Writeable for Concat<A, B>
where
    A: Writeable,
    B: Writeable,
{
    #[inline]
    fn write_to<W: fmt::Write + ?Sized>(&self, sink: &mut W) -> fmt::Result {
        self.0.write_to(sink)?;
        self.1.write_to(sink)
    }
    #[inline]
    fn write_to_parts<S: crate::PartsWrite + ?Sized>(&self, sink: &mut S) -> fmt::Result {
        self.0.write_to_parts(sink)?;
        self.1.write_to_parts(sink)
    }
    #[inline]
    fn writeable_length_hint(&self) -> crate::LengthHint {
        self.0.writeable_length_hint() + self.1.writeable_length_hint()
    }
}

impl<A, B, E> TryWriteable for Concat<A, B>
where
    A: TryWriteable<Error = E>,
    B: TryWriteable<Error = E>,
{
    type Error = E;
    #[inline]
    fn try_write_to<W: fmt::Write + ?Sized>(
        &self,
        sink: &mut W,
    ) -> Result<Result<(), Self::Error>, fmt::Error> {
        if let Err(e) = self.0.try_write_to(sink)? {
            return Ok(Err(e));
        }
        self.1.try_write_to(sink)
    }
    #[inline]
    fn try_write_to_parts<S: crate::PartsWrite + ?Sized>(
        &self,
        sink: &mut S,
    ) -> Result<Result<(), Self::Error>, fmt::Error> {
        if let Err(e) = self.0.try_write_to_parts(sink)? {
            return Ok(Err(e));
        }
        self.1.try_write_to_parts(sink)
    }
    #[inline]
    fn writeable_length_hint(&self) -> crate::LengthHint {
        self.0.writeable_length_hint() + self.1.writeable_length_hint()
    }
}

impl<A, B> fmt::Display for Concat<A, B>
where
    A: Writeable,
    B: Writeable,
{
    #[inline]
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        self.0.write_to(f)?;
        self.1.write_to(f)
    }
}

/// Returns a [`Writeable`] concatenating any number of [`Writeable`]s.
///
/// The macro resolves to a nested [`Concat`].
///
/// # Examples
///
/// ```
/// use writeable::assert_writeable_eq;
///
/// let concatenated = writeable::concat_writeable!("Health: ", 5, '/', 8);
///
/// assert_writeable_eq!(concatenated, "Health: 5/8");
/// ```
#[macro_export]
#[doc(hidden)] // macro
macro_rules! __concat_writeable {
    // Base case:
    ($x:expr) => ($x);
    // `$x` followed by at least one `$y,`
    ($x:expr, $($y:expr),+) => (
        // Call `concat_writeable!` recursively on the tail `$y`
        $crate::adapters::Concat($x, $crate::concat_writeable!($($y),+))
    )
}
#[doc(inline)]
pub use __concat_writeable as concat_writeable;
