#![doc = include_str!("../../doc/ptr/addr.md")]

use core::{
	any,
	fmt::{
		self,
		Debug,
		Display,
		Formatter,
		Pointer,
	},
	mem,
	ptr::NonNull,
};

use tap::{
	Pipe,
	TryConv,
};
use wyz::{
	comu::{
		Address,
		Const,
		Mut,
		Mutability,
	},
	fmt::FmtForward,
};

/// Ensures that an address is well-aligned to its referent type width.
#[inline]
pub fn check_alignment<M, T>(
	addr: Address<M, T>,
) -> Result<Address<M, T>, MisalignError<T>>
where M: Mutability {
	let ptr = addr.to_const();
	let mask = mem::align_of::<T>() - 1;
	if ptr as usize & mask != 0 {
		Err(MisalignError { ptr })
	}
	else {
		Ok(addr)
	}
}

/// Extension methods for raw pointers.
pub(crate) trait AddressExt {
	/// Tracks the original mutation capability of the source pointer.
	type Permission: Mutability;
	/// The type to which the pointer points.
	type Referent: Sized;

	/// Forcibly wraps a raw pointer as an `Address`, without handling errors.
	///
	/// In debug builds, this panics on null or misaligned pointers. In release
	/// builds, it is permitted to remove the error-handling codepaths and
	/// assume these invariants are upheld by the caller.
	///
	/// ## Safety
	///
	/// The caller must ensure that this is only called on non-null,
	/// well-aligned pointers. Pointers derived from Rust references or calls to
	/// the Rust allocator API will always satisfy this.
	unsafe fn into_address(self) -> Address<Self::Permission, Self::Referent>;
}

#[cfg(not(tarpaulin_include))]
impl<T> AddressExt for *const T {
	type Permission = Const;
	type Referent = T;

	unsafe fn into_address(self) -> Address<Const, T> {
		if cfg!(debug_assertions) {
			self.try_conv::<Address<_, _>>()
				.unwrap_or_else(|err| panic!("{}", err))
				.pipe(check_alignment)
				.unwrap_or_else(|err| panic!("{}", err))
		}
		else {
			Address::new(NonNull::new_unchecked(self as *mut T))
		}
	}
}

#[cfg(not(tarpaulin_include))]
impl<T> AddressExt for *mut T {
	type Permission = Mut;
	type Referent = T;

	unsafe fn into_address(self) -> Address<Mut, T> {
		if cfg!(debug_assertions) {
			self.try_conv::<Address<_, _>>()
				.unwrap_or_else(|err| panic!("{}", err))
				.pipe(check_alignment)
				.unwrap_or_else(|err| panic!("{}", err))
		}
		else {
			Address::new(NonNull::new_unchecked(self))
		}
	}
}

#[cfg(not(tarpaulin_include))]
impl<T> AddressExt for &T {
	type Permission = Const;
	type Referent = T;

	unsafe fn into_address(self) -> Address<Self::Permission, Self::Referent> {
		self.into()
	}
}

#[cfg(not(tarpaulin_include))]
impl<T> AddressExt for &mut T {
	type Permission = Mut;
	type Referent = T;

	unsafe fn into_address(self) -> Address<Self::Permission, Self::Referent> {
		self.into()
	}
}

/// The error produced when an address is insufficiently aligned to the width of
/// its type.
#[derive(Clone, Copy, Eq, Hash, Ord, PartialEq, PartialOrd)]
pub struct MisalignError<T> {
	/// The misaligned pointer.
	ptr: *const T,
}

impl<T> MisalignError<T> {
	/// The minimum address alignment of `T` values.
	const ALIGN: usize = mem::align_of::<T>();
	/// The number of least-significant-bits of an address that must be `0` in
	/// order for it to be validly aligned for `T`.
	const CTTZ: usize = Self::ALIGN.trailing_zeros() as usize;
}

#[cfg(not(tarpaulin_include))]
impl<T> Debug for MisalignError<T> {
	#[inline]
	fn fmt(&self, fmt: &mut Formatter) -> fmt::Result {
		fmt.debug_tuple("MisalignError")
			.field(&self.ptr.fmt_pointer())
			.field(&Self::ALIGN)
			.finish()
	}
}

#[cfg(not(tarpaulin_include))]
impl<T> Display for MisalignError<T> {
	#[inline]
	fn fmt(&self, fmt: &mut Formatter) -> fmt::Result {
		write!(
			fmt,
			"Type {} requires {}-byte alignment: address ",
			any::type_name::<T>(),
			Self::ALIGN,
		)?;
		Pointer::fmt(&self.ptr, fmt)?;
		write!(fmt, " must clear its least {} bits", Self::CTTZ)
	}
}

unsafe impl<T> Send for MisalignError<T> {}

unsafe impl<T> Sync for MisalignError<T> {}

#[cfg(feature = "std")]
impl<T> std::error::Error for MisalignError<T> {}
