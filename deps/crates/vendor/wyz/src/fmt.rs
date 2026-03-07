/*! Format forwarding

This module provides wrapper types for each formatting trait other than `Debug`
which, when `Debug`-formatted, forward to the original trait instead of `Debug`.

Each wrapper type is a tuple struct so that it can be used as a named
constructor, such as in `.map(FmtDisplay)`. In addition, a blanket trait adds
extension methods `.fmt_<trait_name>>()` to provide the corresponding wrap.

Any modifiers in the format template string or struct modifier are passed
through to the desired trait implementation unchanged. The only effect of the
forwarding types in this module is to change the `?` template character to one
of the other trait signifiers.
!*/

use core::{
	fmt::{
		self,
		Binary,
		Debug,
		Display,
		Formatter,
		LowerExp,
		LowerHex,
		Octal,
		Pointer,
		UpperExp,
		UpperHex,
	},
	ops::{
		Deref,
		DerefMut,
	},
};

/// Wraps any value with a format-forward to `Debug`.
#[cfg(not(tarpaulin_include))]
pub trait FmtForward: Sized {
	/// Causes `self` to use its `Binary` implementation when `Debug`-formatted.
	#[inline(always)]
	fn fmt_binary(self) -> FmtBinary<Self>
	where Self: Binary {
		FmtBinary(self)
	}

	/// Causes `self` to use its `Display` implementation when
	/// `Debug`-formatted.
	#[inline(always)]
	fn fmt_display(self) -> FmtDisplay<Self>
	where Self: Display {
		FmtDisplay(self)
	}

	/// Causes `self` to use its `LowerExp` implementation when
	/// `Debug`-formatted.
	#[inline(always)]
	fn fmt_lower_exp(self) -> FmtLowerExp<Self>
	where Self: LowerExp {
		FmtLowerExp(self)
	}

	/// Causes `self` to use its `LowerHex` implementation when
	/// `Debug`-formatted.
	#[inline(always)]
	fn fmt_lower_hex(self) -> FmtLowerHex<Self>
	where Self: LowerHex {
		FmtLowerHex(self)
	}

	/// Causes `self` to use its `Octal` implementation when `Debug`-formatted.
	#[inline(always)]
	fn fmt_octal(self) -> FmtOctal<Self>
	where Self: Octal {
		FmtOctal(self)
	}

	/// Causes `self` to use its `Pointer` implementation when
	/// `Debug`-formatted.
	#[inline(always)]
	fn fmt_pointer(self) -> FmtPointer<Self>
	where Self: Pointer {
		FmtPointer(self)
	}

	/// Causes `self` to use its `UpperExp` implementation when
	/// `Debug`-formatted.
	#[inline(always)]
	fn fmt_upper_exp(self) -> FmtUpperExp<Self>
	where Self: UpperExp {
		FmtUpperExp(self)
	}

	/// Causes `self` to use its `UpperHex` implementation when
	/// `Debug`-formatted.
	#[inline(always)]
	fn fmt_upper_hex(self) -> FmtUpperHex<Self>
	where Self: UpperHex {
		FmtUpperHex(self)
	}

	/// Formats each item in a sequence.
	///
	/// This wrapper structure conditionally implements all of the formatting
	/// traits when `self` can be viewed as an iterator whose *items* implement
	/// them. It iterates over `&self` and prints each item according to the
	/// formatting specifier provided.
	#[inline(always)]
	fn fmt_list(self) -> FmtList<Self>
	where for<'a> &'a Self: IntoIterator {
		FmtList(self)
	}
}

impl<T: Sized> FmtForward for T {
}

/// Forwards a type’s `Binary` formatting implementation to `Debug`.
#[repr(transparent)]
pub struct FmtBinary<T: Binary>(pub T);

/// Forwards a type’s `Display` formatting implementation to `Debug`.
#[repr(transparent)]
pub struct FmtDisplay<T: Display>(pub T);

/// Renders each element of a stream into a list.
#[repr(transparent)]
pub struct FmtList<T>(pub T)
where for<'a> &'a T: IntoIterator;

/// Forwards a type’s `LowerExp` formatting implementation to `Debug`.
#[repr(transparent)]
pub struct FmtLowerExp<T: LowerExp>(pub T);

/// Forwards a type’s `LowerHex` formatting implementation to `Debug`.
#[repr(transparent)]
pub struct FmtLowerHex<T: LowerHex>(pub T);

/// Forwards a type’s `Octal` formatting implementation to `Debug`.
#[repr(transparent)]
pub struct FmtOctal<T: Octal>(pub T);

/// Forwards a type’s `Pointer` formatting implementation to `Debug`.
#[repr(transparent)]
pub struct FmtPointer<T: Pointer>(pub T);

/// Forwards a type’s `UpperExp` formatting implementation to `Debug`.
#[repr(transparent)]
pub struct FmtUpperExp<T: UpperExp>(pub T);

/// Forwards a type’s `UpperHex` formatting implementation to `Debug`.
#[repr(transparent)]
pub struct FmtUpperHex<T: UpperHex>(pub T);

macro_rules! fmt {
	($($w:ty => $t:ident),* $(,)?) => { $(
		#[cfg(not(tarpaulin_include))]
		impl<T: $t + Binary> Binary for $w {
			#[inline(always)]
			fn fmt(&self, fmt: &mut Formatter) -> fmt::Result {
				Binary::fmt(&self.0, fmt)
			}
		}

		impl<T: $t> Debug for $w {
			#[inline(always)]
			fn fmt(&self, fmt: &mut Formatter) -> fmt::Result {
				<T as $t>::fmt(&self.0, fmt)
			}
		}

		#[cfg(not(tarpaulin_include))]
		impl<T: $t + Display> Display for $w {
			#[inline(always)]
			fn fmt(&self, fmt: &mut Formatter) -> fmt::Result {
				Display::fmt(&self.0, fmt)
			}
		}

		#[cfg(not(tarpaulin_include))]
		impl<T: $t + LowerExp> LowerExp for $w {
			#[inline(always)]
			fn fmt(&self, fmt: &mut Formatter) -> fmt::Result {
				LowerExp::fmt(&self.0, fmt)
			}
		}

		#[cfg(not(tarpaulin_include))]
		impl<T: $t + LowerHex> LowerHex for $w {
			#[inline(always)]
			fn fmt(&self, fmt: &mut Formatter) -> fmt::Result {
				LowerHex::fmt(&self.0, fmt)
			}
		}

		#[cfg(not(tarpaulin_include))]
		impl<T: $t + Octal> Octal for $w {
			#[inline(always)]
			fn fmt(&self, fmt: &mut Formatter) -> fmt::Result {
				Octal::fmt(&self.0, fmt)
			}
		}

		#[cfg(not(tarpaulin_include))]
		impl<T: $t + Pointer> Pointer for $w {
			#[inline(always)]
			fn fmt(&self, fmt: &mut Formatter) -> fmt::Result {
				Pointer::fmt(&self.0, fmt)
			}
		}

		#[cfg(not(tarpaulin_include))]
		impl<T: $t + UpperExp> UpperExp for $w {
			#[inline(always)]
			fn fmt(&self, fmt: &mut Formatter) -> fmt::Result {
				UpperExp::fmt(&self.0, fmt)
			}
		}

		#[cfg(not(tarpaulin_include))]
		impl<T: $t + UpperHex> UpperHex for $w {
			#[inline(always)]
			fn fmt(&self, fmt: &mut Formatter) -> fmt::Result {
				UpperHex::fmt(&self.0, fmt)
			}
		}

		#[cfg(not(tarpaulin_include))]
		impl<T: $t> Deref for $w {
			type Target = T;

			#[inline(always)]
			fn deref(&self) -> &Self::Target {
				&self.0
			}
		}

		#[cfg(not(tarpaulin_include))]
		impl<T: $t> DerefMut for $w {
			#[inline(always)]
			fn deref_mut(&mut self) -> &mut Self::Target {
				&mut self.0
			}
		}

		#[cfg(not(tarpaulin_include))]
		impl<T: $t> AsRef<T> for $w {
			#[inline(always)]
			fn as_ref(&self) -> &T {
				&self.0
			}
		}

		#[cfg(not(tarpaulin_include))]
		impl<T: $t> AsMut<T> for $w {
			#[inline(always)]
			fn as_mut(&mut self) -> &mut T {
				&mut self.0
			}
		}
	)* };
}

fmt!(
	FmtBinary<T> => Binary,
	FmtDisplay<T> => Display,
	FmtLowerExp<T> => LowerExp,
	FmtLowerHex<T> => LowerHex,
	FmtOctal<T> => Octal,
	FmtPointer<T> => Pointer,
	FmtUpperExp<T> => UpperExp,
	FmtUpperHex<T> => UpperHex,
);

impl<T> Binary for FmtList<T>
where
	for<'a> &'a T: IntoIterator,
	for<'a> <&'a T as IntoIterator>::Item: Binary,
{
	fn fmt(&self, fmt: &mut Formatter) -> fmt::Result {
		fmt.debug_list()
			.entries((&self.0).into_iter().map(FmtBinary))
			.finish()
	}
}

impl<T> Debug for FmtList<T>
where
	for<'a> &'a T: IntoIterator,
	for<'a> <&'a T as IntoIterator>::Item: Debug,
{
	fn fmt(&self, fmt: &mut Formatter) -> fmt::Result {
		fmt.debug_list().entries((&self.0).into_iter()).finish()
	}
}

impl<T> Display for FmtList<T>
where
	for<'a> &'a T: IntoIterator,
	for<'a> <&'a T as IntoIterator>::Item: Display,
{
	fn fmt(&self, fmt: &mut Formatter) -> fmt::Result {
		fmt.debug_list()
			.entries((&self.0).into_iter().map(FmtDisplay))
			.finish()
	}
}

impl<T> LowerExp for FmtList<T>
where
	for<'a> &'a T: IntoIterator,
	for<'a> <&'a T as IntoIterator>::Item: LowerExp,
{
	fn fmt(&self, fmt: &mut Formatter) -> fmt::Result {
		fmt.debug_list()
			.entries((&self.0).into_iter().map(FmtLowerExp))
			.finish()
	}
}

impl<T> LowerHex for FmtList<T>
where
	for<'a> &'a T: IntoIterator,
	for<'a> <&'a T as IntoIterator>::Item: LowerHex,
{
	fn fmt(&self, fmt: &mut Formatter) -> fmt::Result {
		fmt.debug_list()
			.entries((&self.0).into_iter().map(FmtLowerHex))
			.finish()
	}
}

impl<T> Octal for FmtList<T>
where
	for<'a> &'a T: IntoIterator,
	for<'a> <&'a T as IntoIterator>::Item: Octal,
{
	fn fmt(&self, fmt: &mut Formatter) -> fmt::Result {
		fmt.debug_list()
			.entries((&self.0).into_iter().map(FmtOctal))
			.finish()
	}
}

impl<T> UpperExp for FmtList<T>
where
	for<'a> &'a T: IntoIterator,
	for<'a> <&'a T as IntoIterator>::Item: UpperExp,
{
	fn fmt(&self, fmt: &mut Formatter) -> fmt::Result {
		fmt.debug_list()
			.entries((&self.0).into_iter().map(FmtUpperExp))
			.finish()
	}
}

impl<T> UpperHex for FmtList<T>
where
	for<'a> &'a T: IntoIterator,
	for<'a> <&'a T as IntoIterator>::Item: UpperHex,
{
	fn fmt(&self, fmt: &mut Formatter) -> fmt::Result {
		fmt.debug_list()
			.entries((&self.0).into_iter().map(FmtUpperHex))
			.finish()
	}
}

#[cfg(not(tarpaulin_include))]
impl<T> Deref for FmtList<T>
where for<'a> &'a T: IntoIterator
{
	type Target = T;

	#[inline(always)]
	fn deref(&self) -> &Self::Target {
		&self.0
	}
}

#[cfg(not(tarpaulin_include))]
impl<T> DerefMut for FmtList<T>
where for<'a> &'a T: IntoIterator
{
	#[inline(always)]
	fn deref_mut(&mut self) -> &mut Self::Target {
		&mut self.0
	}
}

#[cfg(not(tarpaulin_include))]
impl<T> AsRef<T> for FmtList<T>
where for<'a> &'a T: IntoIterator
{
	#[inline(always)]
	fn as_ref(&self) -> &T {
		&self.0
	}
}

#[cfg(not(tarpaulin_include))]
impl<T> AsMut<T> for FmtList<T>
where for<'a> &'a T: IntoIterator
{
	#[inline(always)]
	fn as_mut(&mut self) -> &mut T {
		&mut self.0
	}
}

#[cfg(all(test, feature = "alloc"))]
mod tests {
	#[cfg(not(feature = "std"))]
	use alloc::format;

	#[cfg(feature = "std")]
	use std::format;

	use super::*;

	#[test]
	fn render_item() {
		let num = 29;

		assert_eq!(format!("{:?}", num.fmt_binary()), "11101");
		assert_eq!(format!("{:?}", num.fmt_display()), "29");
		assert_eq!(format!("{:?}", num.fmt_upper_hex()), "1D");
		assert_eq!(format!("{:?}", num.fmt_octal()), "35");
		assert_eq!(format!("{:?}", num.fmt_lower_hex()), "1d");

		let num = 53.7;
		assert_eq!(format!("{:?}", num.fmt_lower_exp()), "5.37e1");
		assert_eq!(format!("{:?}", num.fmt_upper_exp()), "5.37E1");
	}

	#[test]
	fn render_list() {
		let list = [0, 1, 2, 3];
		assert_eq!(format!("{:02b}", list.fmt_list()), "[00, 01, 10, 11]");
		assert_eq!(format!("{:01?}", list.fmt_list()), "[0, 1, 2, 3]");
		assert_eq!(format!("{:01}", list.fmt_list()), "[0, 1, 2, 3]");

		let list = [-51.0, -1.2, 1.3, 54.0];
		assert_eq!(
			format!("{:e}", list.fmt_list()),
			"[-5.1e1, -1.2e0, 1.3e0, 5.4e1]"
		);
		assert_eq!(
			format!("{:E}", list.fmt_list()),
			"[-5.1E1, -1.2E0, 1.3E0, 5.4E1]"
		);

		let list = [0, 10, 20, 30];
		assert_eq!(format!("{:02x}", list.fmt_list()), "[00, 0a, 14, 1e]");
		assert_eq!(format!("{:02o}", list.fmt_list()), "[00, 12, 24, 36]");
		assert_eq!(format!("{:02X}", list.fmt_list()), "[00, 0A, 14, 1E]");
	}
}
