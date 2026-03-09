/*! # Method-Directed Type Conversion

The `std::convert` module provides traits for converting values from one type to
another. The first of these, [`From<T>`], provides an associated function
[`from(orig: T) -> Self`]. This function can only be called in prefix-position,
as it does not have a `self` receiver. The second, [`Into<T>`], provides a
method [`into(self) -> T`] which *can* be called in suffix-position; due to
intractable problems in the type solver, this method cannot have any *further*
method calls attached to it. It must be bound directly into a `let` or function
call.

The [`TryFrom<T>`] and [`TryInto<T>`] traits have the same properties, but
permit failure.

This module provides traits that place the conversion type parameter in the
method, rather than in the trait, so that users can write `.conv::<T>()` to
convert the preceding expression into `T`, without causing any failures in the
type solver. These traits are blanket-implemented on all types that have an
`Into<T>` implementation, which covers both the blanket implementation of `Into`
for types with `From`, and manual implementations of `Into`.

[`From<T>`]: https://doc.rust-lang.org/std/convert/trait.From.html
[`Into<T>`]: https://doc.rust-lang.org/std/convert/trait.Into.html
[`TryFrom<T>`]: https://doc.rust-lang.org/std/convert/trait.TryFrom.html
[`TryInto<T>`]: https://doc.rust-lang.org/std/convert/trait.TryInto.html
[`from(orig: T) -> Self`]: https://doc.rust-lang.org/std/convert/trait.From.html#tymethod.from
[`into(self) -> T`]: https://doc.rust-lang.org/std/convert/trait.Into.html#tymethod.into
!*/

use core::convert::TryInto;

/// Wraps `Into::<T>::into` as a method that can be placed in pipelines.
pub trait Conv
where
	Self: Sized,
{
	/// Converts `self` into `T` using `Into<T>`.
	///
	/// # Examples
	///
	/// ```rust
	/// use tap::conv::Conv;
	///
	/// let len = "Saluton, mondo!"
	///   .conv::<String>()
	///   .len();
	/// ```
	#[inline(always)]
	fn conv<T>(self) -> T
	where
		Self: Into<T>,
		T: Sized,
	{
		Into::<T>::into(self)
	}
}

impl<T> Conv for T {}

/// Wraps `TryInto::<T>::try_into` as a method that can be placed in pipelines.
pub trait TryConv
where
	Self: Sized,
{
	/// Attempts to convert `self` into `T` using `TryInto<T>`.
	///
	/// # Examples
	///
	/// ```rust
	/// use tap::conv::TryConv;
	///
	/// let len = "Saluton, mondo!"
	///   .try_conv::<String>()
	///   .unwrap()
	///   .len();
	/// ```
	#[inline(always)]
	fn try_conv<T>(self) -> Result<T, Self::Error>
	where
		Self: TryInto<T>,
		T: Sized,
	{
		TryInto::<T>::try_into(self)
	}
}

impl<T> TryConv for T {}
