/*! Trait-level `co`nst/`mu`table tracking.

This module provides a system of marker types that can be used to encode write
permissions into type parameters rather than duplicate structures.
!*/

//  This module has no compute logic of its own; it only exists in the
//  type-system and to forward to the standard library.
#![cfg(not(tarpaulin_include))]

use core::{
	cmp,
	convert::TryFrom,
	fmt::{
		self,
		Debug,
		Display,
		Formatter,
		Pointer,
	},
	hash::{
		Hash,
		Hasher,
	},
	ops::Deref,
	ptr::NonNull,
	slice,
};

use tap::Pipe;

/// A basic `const` marker.
#[derive(Clone, Copy, Debug, Default, Eq, Hash, Ord, PartialEq, PartialOrd)]
pub struct Const;

/// A basic `mut` marker.
#[derive(Clone, Copy, Debug, Default, Eq, Hash, Ord, PartialEq, PartialOrd)]
pub struct Mut;

/// A frozen wrapper over some other `Mutability` marker.
#[derive(Clone, Copy, Debug, Default, Eq, Hash, Ord, PartialEq, PartialOrd)]
pub struct Frozen<Inner>
where Inner: Mutability
{
	inner: Inner,
}

/** Generalized mutability permissions.

This trait enables referent structures to be generic over the write permissions
of their referent data. As an example, the standard library defines `*const T`
and `*mut T` as two duplicate type families, that cannot share any logic at all.

An equivalent library implementation might be `Ptr<T, M: Mutability>`, where
shared logic can be placed in an `impl<T, M> Ptr<T, M>` block, but unique logic
(such as freezing a `Mut` pointer, or unfreezing a `Frozen<Mut>`) can be placed
in specialized `impl<T> Ptr<T, Mut>` blocks.
**/
pub trait Mutability: 'static + Copy + Sized + self::seal::Sealed {
	/// Marks whether this type contains mutability permissions within it.
	///
	/// This is `false` for `Const` and `true` for `Mut`. `Frozen` wrappers
	/// atop either of these types inherit their interior marker.
	const CONTAINS_MUTABILITY: bool = false;

	/// Counts the layers of `Frozen<>` wrapping around a base `Const` or `Mut`.
	const PEANO_NUMBER: usize = 0;

	/// Allow instances to be constructed generically.
	const SELF: Self;

	/// One of `*const` or `*mut`.
	const RENDER: &'static str;

	/// Freeze this type, wrapping it in a `const` marker that may later be
	/// removed to thaw it.
	fn freeze(self) -> Frozen<Self> {
		Frozen { inner: self }
	}

	/// Thaw a previously-frozen type, removing its `Frozen` marker and
	/// restoring it to `Self`.
	fn thaw(Frozen { inner }: Frozen<Self>) -> Self {
		inner
	}
}

impl Mutability for Const {
	const RENDER: &'static str = "*const";
	const SELF: Self = Self;
}

impl self::seal::Sealed for Const {
}

impl<Inner> Mutability for Frozen<Inner>
where Inner: Mutability + Sized
{
	const CONTAINS_MUTABILITY: bool = Inner::CONTAINS_MUTABILITY;
	const PEANO_NUMBER: usize = 1 + Inner::PEANO_NUMBER;
	const RENDER: &'static str = Inner::RENDER;
	const SELF: Self = Self { inner: Inner::SELF };
}

impl<Inner> self::seal::Sealed for Frozen<Inner> where Inner: Mutability + Sized
{
}

impl Mutability for Mut {
	const CONTAINS_MUTABILITY: bool = true;
	const RENDER: &'static str = "*mut";
	const SELF: Self = Self;
}

impl self::seal::Sealed for Mut {
}

/** A generic non-null pointer with type-system mutability tracking.

# Type Parameters

- `M`: The mutability permissions of the source pointer.
- `T`: The referent type of the source pointer.
**/
pub struct Address<M, T>
where
	M: Mutability,
	T: ?Sized,
{
	/// The address value.
	inner: NonNull<T>,
	/// The mutability permissions.
	comu: M,
}

impl<M, T> Address<M, T>
where M: Mutability
{
	/// The dangling pointer.
	pub const DANGLING: Self = Self {
		inner: NonNull::dangling(),
		comu: M::SELF,
	};
}

impl<M, T> Address<M, T>
where
	M: Mutability,
	T: ?Sized,
{
	/// Constructs a new `Address` over some pointer value.
	///
	/// You are responsible for selecting the correct `Mutability` marker.
	#[inline(always)]
	pub fn new(addr: NonNull<T>) -> Self {
		Self {
			inner: addr,
			comu: M::SELF,
		}
	}

	/// Permanently converts an `Address<_>` into an `Address<Const>`.
	///
	/// You should generally prefer [`Address::freeze`].
	#[inline(always)]
	pub fn immut(self) -> Address<Const, T> {
		Address {
			inner: self.inner,
			comu: Const,
		}
	}

	/// Force an `Address<Const>` to be `Address<Mut>`.
	///
	/// ## Safety
	///
	/// You should only call this on addresses you know to have been created
	/// with `Mut`able permissions and previously removed by [`Address::immut`].
	///
	/// You should prefer using [`Address::freeze`] for temporary, trackable,
	/// immutability constraints instead.
	#[inline(always)]
	pub unsafe fn assert_mut(self) -> Address<Mut, T> {
		Address {
			inner: self.inner,
			comu: Mut,
		}
	}

	/// Freezes the `Address` so that it is read-only.
	#[inline(always)]
	pub fn freeze(self) -> Address<Frozen<M>, T> {
		let Self { inner, comu } = self;
		Address {
			inner,
			comu: comu.freeze(),
		}
	}

	/// Removes the `Address` type marker, returning the original pointer.
	#[inline(always)]
	pub fn into_inner(self) -> NonNull<T> {
		self.inner
	}

	/// Gets the address as a read-only pointer.
	#[inline(always)]
	pub fn to_const(self) -> *const T {
		self.inner.as_ptr() as *const T
	}
}

impl<T> Address<Mut, T> {
	/// Gets the address as a write-capable pointer.
	#[inline(always)]
	#[allow(clippy::wrong_self_convention)]
	pub fn to_mut(self) -> *mut T {
		self.inner.as_ptr()
	}
}

impl<M, T> Address<Frozen<M>, T>
where
	M: Mutability,
	T: ?Sized,
{
	/// Thaws the `Address` to its original mutability permission.
	#[inline(always)]
	pub fn thaw(self) -> Address<M, T> {
		let Self { inner, comu } = self;
		Address {
			inner,
			comu: Mutability::thaw(comu),
		}
	}
}

/// Implement `*T -> *T` functions as `Address<T> -> Address<T>`.
macro_rules! fwd {
	($(
		$(@$unsafe:ident)?
		$name:ident
		$(<
			$($lt:lifetime),*
			$($typaram:ident$(: $($bound:ident),+ $(,)?)?),*
			$(,)*
		>)?
		$(, $arg:ident: $ty:ty)*
		$(=> $ret:ty)?
	);+ $(;)?) => { $(
		#[doc = concat!("Applies `<*T>::", stringify!($name), "`.")]
		///
		/// See [original documentation][orig].
		///
		#[doc = concat!("[orig]: https://doc.rust-lang.org/std/primitive.pointer.html#method.", stringify!($name))]
		pub $($unsafe)? fn $name$(<
			$($lt,)* $($typaram$(: $($bound),+)?,)*
		>)?(self$(, $arg: $ty)*) $(-> $ret)? {
			self.with_ptr(|ptr| ptr.$name($($arg),*))
		}
	)+ };
}

/// Implement all other pointer functions.
macro_rules! map {
	($(
		$(@$unsafe:ident)?
		$name:ident
		$(<
			$($lt:lifetime),*
			$($typaram:ident$(: $($bound:ident),+ $(,)?)?),*
			$(,)?
		>)?
		$(, $arg:ident: $ty:ty $(as $map:expr)?)*
		$(=> $ret:ty)?
	);+ $(;)?) => { $(
		#[doc = concat!("Applies `<*T>::", stringify!($name), "`.")]
		///
		/// See [original documentation][orig].
		///
		#[doc = concat!("[orig]: https://doc.rust-lang.org/std/primitive.pointer.html#method.", stringify!($name))]
		pub $($unsafe)? fn $name$(<
			$($lt,)* $($typaram$(: $($bound),+)?,)*
		>)?(self$(, $arg: $ty)*) $(-> $ret)? {
			self.inner.as_ptr().$name($($arg$(.pipe($map))?),*)
		}
	)+ };
}

/// Port of the pointer inherent methods on `Address`es of `Sized` types.
#[allow(clippy::missing_safety_doc)]
impl<M, T> Address<M, T>
where M: Mutability
{
	fwd! {
		cast<U> => Address<M, U>;
		@unsafe offset, count: isize => Self;
		@unsafe add, count: usize => Self;
		@unsafe sub, count: usize => Self;
		wrapping_offset, count: isize => Self;
		wrapping_add, count: usize => Self;
		wrapping_sub, count: usize => Self;
	}

	map! {
		@unsafe offset_from, origin: Self as |orig| orig.to_const() as *mut T => isize;
		@unsafe read => T;
		@unsafe read_volatile => T;
		@unsafe read_unaligned => T;
		@unsafe copy_to, dest: Address<Mut, T> as Address::to_mut, count: usize;
		@unsafe copy_to_nonoverlapping, dest: Address<Mut, T> as Address::to_mut, count: usize;
		align_offset, align: usize => usize;
	}
}

/// Port of the pointer inherent methods on `Address`es of any type.
impl<M, T> Address<M, T>
where
	M: Mutability,
	T: ?Sized,
{
	map! {
		@unsafe as_ref<'a> => Option<&'a T>;
	}

	/// Applies a pointer -> pointer function within an Address -> Address.
	#[track_caller]
	fn with_ptr<U>(self, func: impl FnOnce(*mut T) -> *mut U) -> Address<M, U> {
		self.inner
			.as_ptr()
			.pipe(func)
			.pipe(NonNull::new)
			.unwrap()
			.pipe(Address::new)
	}
}

/// Port of pointer inherent methods on mutable `Address`es of sized types.
impl<T> Address<Mut, T> {
	map! {
		@unsafe copy_from<M2: Mutability>, src: Address<M2, T> as Address::to_const, count: usize;
		@unsafe copy_from_nonoverlapping<M2: Mutability>, src: Address<M2, T> as Address::to_const, count: usize;
		@unsafe write, value: T;
		@unsafe write_volatile, value: T;
		@unsafe write_unaligned, value: T;
		@unsafe replace, src: T => T;
		@unsafe swap, with: Self as Self::to_mut;
	}
}

/// Port of pointer inherent methods on mutable `Address`es of any type.
impl<T> Address<Mut, T>
where T: ?Sized
{
	map! {
		@unsafe as_mut<'a> => Option<&'a mut T>;
		@unsafe drop_in_place;
	}
}

impl<M, T> Clone for Address<M, T>
where
	M: Mutability,
	T: ?Sized,
{
	#[inline(always)]
	fn clone(&self) -> Self {
		*self
	}
}

impl<T> TryFrom<*const T> for Address<Const, T>
where T: ?Sized
{
	type Error = NullPtrError;

	#[inline(always)]
	fn try_from(elem: *const T) -> Result<Self, Self::Error> {
		NonNull::new(elem as *mut T)
			.ok_or(NullPtrError)
			.map(Self::new)
	}
}

impl<T> From<&T> for Address<Const, T>
where T: ?Sized
{
	#[inline(always)]
	fn from(elem: &T) -> Self {
		Self::new(elem.into())
	}
}

impl<T> TryFrom<*mut T> for Address<Mut, T>
where T: ?Sized
{
	type Error = NullPtrError;

	#[inline(always)]
	fn try_from(elem: *mut T) -> Result<Self, Self::Error> {
		NonNull::new(elem).ok_or(NullPtrError).map(Self::new)
	}
}

impl<T> From<&mut T> for Address<Mut, T>
where T: ?Sized
{
	#[inline(always)]
	fn from(elem: &mut T) -> Self {
		Self::new(elem.into())
	}
}

impl<M, T> Eq for Address<M, T> where M: Mutability
{
}

impl<M1, M2, T1, T2> PartialEq<Address<M2, T2>> for Address<M1, T1>
where
	M1: Mutability,
	M2: Mutability,
{
	#[inline]
	fn eq(&self, other: &Address<M2, T2>) -> bool {
		self.inner.as_ptr() as usize == other.inner.as_ptr() as usize
	}
}

impl<M, T> Ord for Address<M, T>
where M: Mutability
{
	#[inline]
	fn cmp(&self, other: &Self) -> cmp::Ordering {
		self.partial_cmp(other)
			.expect("Addresses have a total ordering")
	}
}

impl<M1, M2, T1, T2> PartialOrd<Address<M2, T2>> for Address<M1, T1>
where
	M1: Mutability,
	M2: Mutability,
{
	#[inline]
	fn partial_cmp(&self, other: &Address<M2, T2>) -> Option<cmp::Ordering> {
		(self.inner.as_ptr() as usize)
			.partial_cmp(&(other.inner.as_ptr() as usize))
	}
}

impl<M, T> Debug for Address<M, T>
where
	M: Mutability,
	T: ?Sized,
{
	#[inline(always)]
	fn fmt(&self, fmt: &mut Formatter) -> fmt::Result {
		Debug::fmt(&self.to_const(), fmt)
	}
}

impl<M, T> Pointer for Address<M, T>
where
	M: Mutability,
	T: ?Sized,
{
	#[inline(always)]
	fn fmt(&self, fmt: &mut Formatter) -> fmt::Result {
		Pointer::fmt(&self.to_const(), fmt)
	}
}

impl<M, T> Hash for Address<M, T>
where
	M: Mutability,
	T: ?Sized,
{
	#[inline(always)]
	fn hash<H>(&self, state: &mut H)
	where H: Hasher {
		self.inner.hash(state)
	}
}

impl<M, T> Copy for Address<M, T>
where
	M: Mutability,
	T: ?Sized,
{
}

impl<M, T> self::seal::Sealed for Address<M, T>
where
	M: Mutability,
	T: ?Sized,
{
}

/// Allows an `Address` to produce an ordinary reference.
pub trait Referential<'a>: self::seal::Sealed {
	/// The created reference type. Must be one of `&T` or `&mut T`.
	type Ref: 'a + Deref;

	/// Converts the `Address` to a reference.
	///
	/// ## Safety
	///
	/// The caller is responsible for ensuring that the memory location that the
	/// `Address` describes contains an initialized value, and that the produced
	/// reference abides by the Rust `&`/`&mut` exclusion rules.
	unsafe fn to_ref(self) -> Self::Ref;

	/// Converts a reference back into an `Address`.
	fn from_ref(this: Self::Ref) -> Self;
}

impl<'a, T> Referential<'a> for Address<Const, T>
where T: 'a + ?Sized
{
	type Ref = &'a T;

	unsafe fn to_ref(self) -> Self::Ref {
		self.inner.as_ref()
	}

	fn from_ref(this: Self::Ref) -> Self {
		this.into()
	}
}

impl<'a, T> Referential<'a> for Address<Mut, T>
where T: 'a + ?Sized
{
	type Ref = &'a mut T;

	unsafe fn to_ref(mut self) -> Self::Ref {
		self.inner.as_mut()
	}

	fn from_ref(this: Self::Ref) -> Self {
		this.into()
	}
}

impl<'a, M, T> Referential<'a> for Address<Frozen<M>, T>
where
	M: Mutability,
	T: 'a + ?Sized,
{
	type Ref = &'a T;

	unsafe fn to_ref(self) -> Self::Ref {
		self.inner.as_ref()
	}

	fn from_ref(this: Self::Ref) -> Self {
		Self::new(NonNull::from(this))
	}
}

/// A generically-mutable reference.
pub type Reference<'a, M, T> = <Address<M, T> as Referential<'a>>::Ref;

/// Allows an `Address<M, [T]>` to produce an ordinary slice reference.
pub trait SliceReferential<'a>: Referential<'a> + self::seal::Sealed {
	/// The type of the element pointer.
	type ElementAddr;

	/// Constructs an ordinary slice reference from a base-address and a length.
	///
	/// ## Parameters
	///
	/// - `ptr`: The address of the base element in the slice.
	/// - `len`: The number of elements, beginning at `ptr`, in the slice.
	///
	/// ## Safety
	///
	/// The base address and the element count must describe a valid region of
	/// memory.
	unsafe fn from_raw_parts(ptr: Self::ElementAddr, len: usize) -> Self::Ref;
}

impl<'a, T> SliceReferential<'a> for Address<Const, [T]>
where T: 'a
{
	type ElementAddr = Address<Const, T>;

	unsafe fn from_raw_parts(ptr: Self::ElementAddr, len: usize) -> Self::Ref {
		slice::from_raw_parts(ptr.to_const(), len)
	}
}

impl<'a, M, T> SliceReferential<'a> for Address<Frozen<M>, [T]>
where
	M: Mutability,
	T: 'a,
{
	type ElementAddr = Address<Frozen<M>, T>;

	unsafe fn from_raw_parts(ptr: Self::ElementAddr, len: usize) -> Self::Ref {
		slice::from_raw_parts(ptr.to_const(), len)
	}
}

impl<'a, T> SliceReferential<'a> for Address<Mut, [T]>
where T: 'a
{
	type ElementAddr = Address<Mut, T>;

	unsafe fn from_raw_parts(ptr: Self::ElementAddr, len: usize) -> Self::Ref {
		slice::from_raw_parts_mut(ptr.to_mut(), len)
	}
}

/// [`Address`] cannot be constructed over null pointers.
#[derive(Clone, Copy, Debug, Default, Eq, Hash, Ord, PartialEq, PartialOrd)]
pub struct NullPtrError;

impl Display for NullPtrError {
	#[inline]
	fn fmt(&self, fmt: &mut Formatter) -> fmt::Result {
		write!(fmt, "wyz::Address cannot contain a null pointer")
	}
}

#[cfg(feature = "std")]
impl std::error::Error for NullPtrError {
}

#[doc(hidden)]
mod seal {
	#[doc(hidden)]
	pub trait Sealed {}
}
