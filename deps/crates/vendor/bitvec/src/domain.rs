#![doc = include_str!("../doc/domain.md")]

use core::{
	any,
	convert::{
		TryFrom,
		TryInto,
	},
	fmt::{
		self,
		Binary,
		Debug,
		Display,
		Formatter,
		LowerHex,
		Octal,
		UpperHex,
	},
	hash::{
		Hash,
		Hasher,
	},
	iter::FusedIterator,
	marker::PhantomData,
};

use tap::{
	Conv,
	Pipe,
	Tap,
};
use wyz::{
	comu::{
		Address,
		Const,
		Mut,
		Mutability,
		Reference,
		Referential,
		SliceReferential,
	},
	fmt::FmtForward,
};

use crate::{
	access::BitAccess,
	index::{
		BitEnd,
		BitIdx,
		BitMask,
	},
	order::{
		BitOrder,
		Lsb0,
	},
	ptr::BitSpan,
	slice::BitSlice,
	store::BitStore,
};

#[doc = include_str!("../doc/domain/BitDomain.md")]
pub enum BitDomain<'a, M = Const, T = usize, O = Lsb0>
where
	M: Mutability,
	T: 'a + BitStore,
	O: BitOrder,
	Address<M, BitSlice<T, O>>: Referential<'a>,
	Address<M, BitSlice<T::Unalias, O>>: Referential<'a>,
{
	/// Indicates that a bit-slice’s contents are entirely in the interior
	/// indices of a single memory element.
	///
	/// The contained value is always the bit-slice that created this view.
	Enclave(Reference<'a, M, BitSlice<T, O>>),
	/// Indicates that a bit-slice’s contents touch an element edge.
	///
	/// This splits the bit-slice into three partitions, each of which may be
	/// empty: two partially-occupied edge elements, with their original type
	/// status, and one interior span, which is known to not have any other
	/// aliases derived from the bit-slice that created this view.
	Region {
		/// Any bits that partially-fill the first element of the underlying
		/// storage region.
		///
		/// This does not modify its aliasing status, as it will already be
		/// appropriately marked before this view is constructed.
		head: Reference<'a, M, BitSlice<T, O>>,
		/// Any bits that wholly-fill elements in the interior of the bit-slice.
		///
		/// This is marked as unaliased, because it is statically impossible for
		/// any other handle derived from the source bit-slice to have
		/// conflicting access to the region of memory it describes. As such,
		/// even a bit-slice that was marked as `::Alias` can revert this
		/// protection on the known-unaliased interior.
		///
		/// Proofs:
		///
		/// - Rust’s `&`/`&mut` exclusion rules universally apply. If a
		///   reference exists, no other reference has unsynchronized write
		///   capability.
		/// - `BitStore::Unalias` only modifies unsynchronized types. `Cell` and
		///   atomic types unalias to themselves, and retain their original
		///   behavior.
		body: Reference<'a, M, BitSlice<T::Unalias, O>>,
		/// Any bits that partially-fill the last element of the underlying
		/// storage region.
		///
		/// This does not modify its aliasing status, as it will already be
		/// appropriately marked before this view is constructed.
		tail: Reference<'a, M, BitSlice<T, O>>,
	},
}

impl<'a, M, T, O> BitDomain<'a, M, T, O>
where
	M: Mutability,
	T: 'a + BitStore,
	O: BitOrder,
	Address<M, BitSlice<T, O>>: Referential<'a>,
	Address<M, BitSlice<T::Unalias, O>>: Referential<'a>,
{
	/// Attempts to unpack the bit-domain as an [`Enclave`] variant. This is
	/// just a shorthand for explicit destructuring.
	///
	/// [`Enclave`]: Self::Enclave
	#[inline]
	pub fn enclave(self) -> Option<Reference<'a, M, BitSlice<T, O>>> {
		match self {
			Self::Enclave(bits) => Some(bits),
			_ => None,
		}
	}

	/// Attempts to unpack the bit-domain as a [`Region`] variant. This is just
	/// a shorthand for explicit destructuring.
	///
	/// [`Region`]: Self::Region
	#[inline]
	pub fn region(
		self,
	) -> Option<(
		Reference<'a, M, BitSlice<T, O>>,
		Reference<'a, M, BitSlice<T::Unalias, O>>,
		Reference<'a, M, BitSlice<T, O>>,
	)> {
		match self {
			Self::Region { head, body, tail } => Some((head, body, tail)),
			_ => None,
		}
	}
}

impl<'a, M, T, O> Default for BitDomain<'a, M, T, O>
where
	M: Mutability,
	T: 'a + BitStore,
	O: BitOrder,
	Address<M, BitSlice<T, O>>: Referential<'a>,
	Address<M, BitSlice<T::Unalias, O>>: Referential<'a>,
	Reference<'a, M, BitSlice<T, O>>: Default,
	Reference<'a, M, BitSlice<T::Unalias, O>>: Default,
{
	#[inline]
	fn default() -> Self {
		Self::Region {
			head: Default::default(),
			body: Default::default(),
			tail: Default::default(),
		}
	}
}

impl<'a, M, T, O> Debug for BitDomain<'a, M, T, O>
where
	M: Mutability,
	T: 'a + BitStore,
	O: BitOrder,
	Address<M, BitSlice<T, O>>: Referential<'a>,
	Address<M, BitSlice<T::Unalias, O>>: Referential<'a>,
	Reference<'a, M, BitSlice<T, O>>: Debug,
	Reference<'a, M, BitSlice<T::Unalias, O>>: Debug,
{
	#[inline]
	fn fmt(&self, fmt: &mut Formatter) -> fmt::Result {
		write!(
			fmt,
			"BitDomain::<{} {}, {}>::",
			M::RENDER,
			any::type_name::<T::Mem>(),
			any::type_name::<O>(),
		)?;
		match self {
			Self::Enclave(elem) => {
				fmt.debug_tuple("Enclave").field(elem).finish()
			},
			Self::Region { head, body, tail } => fmt
				.debug_struct("Region")
				.field("head", head)
				.field("body", body)
				.field("tail", tail)
				.finish(),
		}
	}
}

#[cfg(not(tarpaulin_include))]
impl<T, O> Clone for BitDomain<'_, Const, T, O>
where
	T: BitStore,
	O: BitOrder,
{
	#[inline]
	fn clone(&self) -> Self {
		*self
	}
}

impl<T, O> Copy for BitDomain<'_, Const, T, O>
where
	T: BitStore,
	O: BitOrder,
{
}

#[doc = include_str!("../doc/domain/Domain.md")]
pub enum Domain<'a, M = Const, T = usize, O = Lsb0>
where
	M: Mutability,
	T: 'a + BitStore,
	O: BitOrder,
	Address<M, T>: Referential<'a>,
	Address<M, [T::Unalias]>: SliceReferential<'a>,
{
	/// Indicates that a bit-slice’s contents are entirely in the interior
	/// indices of a single memory element.
	///
	/// The contained reference is only able to observe the bits governed by the
	/// generating bit-slice. Other handles to the element may exist, and may
	/// write to bits outside the range that this reference can observe.
	Enclave(PartialElement<'a, M, T, O>),
	/// Indicates that a bit-slice’s contents touch an element edge.
	///
	/// This splits the bit-slice into three partitions, each of which may be
	/// empty: two partially-occupied edge elements, with their original type
	/// status, and one interior span, which is known not to have any other
	/// aliases derived from the bit-slice that created this view.
	Region {
		/// The first element in the bit-slice’s underlying storage, if it is
		/// only partially used.
		head: Option<PartialElement<'a, M, T, O>>,
		/// All fully-used elements in the bit-slice’s underlying storage.
		///
		/// This is marked as unaliased, because it is statically impossible for
		/// any other handle derived from the source bit-slice to have
		/// conflicting access to the region of memory it describes. As such,
		/// even a bit-slice that was marked as `::Alias` can revert this
		/// protection on the known-unaliased interior.
		body: Reference<'a, M, [T::Unalias]>,
		/// The last element in the bit-slice’s underlying storage, if it is
		/// only partially used.
		tail: Option<PartialElement<'a, M, T, O>>,
	},
}

impl<'a, M, T, O> Domain<'a, M, T, O>
where
	M: Mutability,
	T: 'a + BitStore,
	O: BitOrder,
	Address<M, T>: Referential<'a>,
	Address<M, [T::Unalias]>: SliceReferential<'a>,
{
	/// Attempts to unpack the bit-domain as an [`Enclave`] variant. This is
	/// just a shorthand for explicit destructuring.
	///
	/// [`Enclave`]: Self::Enclave
	#[inline]
	pub fn enclave(self) -> Option<PartialElement<'a, M, T, O>> {
		match self {
			Self::Enclave(elem) => Some(elem),
			_ => None,
		}
	}

	/// Attempts to unpack the bit-domain as a [`Region`] variant. This is just
	/// a shorthand for explicit destructuring.
	///
	/// [`Region`]: Self::Region
	#[inline]
	pub fn region(
		self,
	) -> Option<(
		Option<PartialElement<'a, M, T, O>>,
		Reference<'a, M, [T::Unalias]>,
		Option<PartialElement<'a, M, T, O>>,
	)> {
		match self {
			Self::Region { head, body, tail } => Some((head, body, tail)),
			_ => None,
		}
	}

	/// Converts the element-wise `Domain` into the equivalent `BitDomain`.
	///
	/// This transform replaces each memory reference with an equivalent
	/// `BitSlice` reference.
	#[inline]
	pub fn into_bit_domain(self) -> BitDomain<'a, M, T, O>
	where
		Address<M, BitSlice<T, O>>: Referential<'a>,
		Address<M, BitSlice<T::Unalias, O>>: Referential<'a>,
		Reference<'a, M, BitSlice<T, O>>: Default,
		Reference<'a, M, BitSlice<T::Unalias, O>>:
			TryFrom<Reference<'a, M, [T::Unalias]>>,
	{
		match self {
			Self::Enclave(elem) => BitDomain::Enclave(elem.into_bitslice()),
			Self::Region { head, body, tail } => BitDomain::Region {
				head: head.map_or_else(
					Default::default,
					PartialElement::into_bitslice,
				),
				body: body.try_into().unwrap_or_else(|_| {
					match option_env!("CARGO_PKG_REPOSITORY") {
						Some(env) => unreachable!(
							"Construction of a slice with length {} should not \
							 be possible. If this assumption is outdated, \
							 please file an issue at {}",
							(isize::MIN as usize) >> 3,
							env,
						),
						None => unreachable!(
							"Construction of a slice with length {} should not \
							 be possible. If this assumption is outdated, \
							 please consider filing an issue",
							(isize::MIN as usize) >> 3
						),
					}
				}),
				tail: tail.map_or_else(
					Default::default,
					PartialElement::into_bitslice,
				),
			},
		}
	}
}

/** Domain constructors.

Only `Domain<Const>` and `Domain<Mut>` are ever constructed, and they of course
are only constructed from `&BitSlice` and `&mut BitSlice`, respectively.

However, the Rust trait system does not have a way to express a closed set, so
**/
impl<'a, M, T, O> Domain<'a, M, T, O>
where
	M: Mutability,
	T: 'a + BitStore,
	O: BitOrder,
	Address<M, T>: Referential<'a>,
	Address<M, [T::Unalias]>:
		SliceReferential<'a, ElementAddr = Address<M, T::Unalias>>,
	Address<M, BitSlice<T, O>>: Referential<'a>,
	Reference<'a, M, [T::Unalias]>: Default,
{
	/// Creates a new `Domain` over a bit-slice.
	///
	/// ## Parameters
	///
	/// - `bits`: Either a `&BitSlice` or `&mut BitSlice` reference, depending
	///   on whether a `Domain<Const>` or `Domain<Mut>` is being produced.
	///
	/// ## Returns
	///
	/// A `Domain` description of the raw memory governed by `bits`.
	pub(crate) fn new(bits: Reference<'a, M, BitSlice<T, O>>) -> Self
	where BitSpan<M, T, O>: From<Reference<'a, M, BitSlice<T, O>>> {
		let bitspan = bits.conv::<BitSpan<M, T, O>>();
		let (head, elts, tail) =
			(bitspan.head(), bitspan.elements(), bitspan.tail());
		let base = bitspan.address();
		let (min, max) = (BitIdx::<T::Mem>::MIN, BitEnd::<T::Mem>::MAX);
		let ctor = match (head, elts, tail) {
			(_, 0, _) => Self::empty,
			(h, _, t) if h == min && t == max => Self::spanning,
			(_, _, t) if t == max => Self::partial_head,
			(h, ..) if h == min => Self::partial_tail,
			(_, 1, _) => Self::minor,
			_ => Self::major,
		};
		ctor(base, elts, head, tail)
	}

	/// Produces the canonical empty `Domain`.
	#[inline]
	fn empty(
		_: Address<M, T>,
		_: usize,
		_: BitIdx<T::Mem>,
		_: BitEnd<T::Mem>,
	) -> Self {
		Default::default()
	}

	/// Produces a `Domain::Region` that contains both `head` and `tail` partial
	/// elements as well as a `body` slice (which may be empty).
	#[inline]
	fn major(
		addr: Address<M, T>,
		elts: usize,
		head: BitIdx<T::Mem>,
		tail: BitEnd<T::Mem>,
	) -> Self {
		let h_elem = addr;
		let t_elem = unsafe { addr.add(elts - 1) };
		let body = unsafe {
			Address::<M, [T::Unalias]>::from_raw_parts(
				addr.add(1).cast::<T::Unalias>(),
				elts - 2,
			)
		};
		Self::Region {
			head: Some(PartialElement::new(h_elem, head, None)),
			body,
			tail: Some(PartialElement::new(t_elem, None, tail)),
		}
	}

	/// Produces a `Domain::Enclave`.
	#[inline]
	fn minor(
		addr: Address<M, T>,
		_: usize,
		head: BitIdx<T::Mem>,
		tail: BitEnd<T::Mem>,
	) -> Self {
		let elem = addr;
		Self::Enclave(PartialElement::new(elem, head, tail))
	}

	/// Produces a `Domain::Region` with a partial `head` and a `body`, but no
	/// `tail`.
	#[inline]
	fn partial_head(
		addr: Address<M, T>,
		elts: usize,
		head: BitIdx<T::Mem>,
		_: BitEnd<T::Mem>,
	) -> Self {
		let elem = addr;
		let body = unsafe {
			Address::<M, [T::Unalias]>::from_raw_parts(
				addr.add(1).cast::<T::Unalias>(),
				elts - 1,
			)
		};
		Self::Region {
			head: Some(PartialElement::new(elem, head, None)),
			body,
			tail: None,
		}
	}

	/// Produces a `Domain::Region` with a partial `tail` and a `body`, but no
	/// `head`.
	#[inline]
	fn partial_tail(
		addr: Address<M, T>,
		elts: usize,
		_: BitIdx<T::Mem>,
		tail: BitEnd<T::Mem>,
	) -> Self {
		let elem = unsafe { addr.add(elts - 1) };
		let body = unsafe {
			Address::<M, [T::Unalias]>::from_raw_parts(
				addr.cast::<T::Unalias>(),
				elts - 1,
			)
		};
		Self::Region {
			head: None,
			body,
			tail: Some(PartialElement::new(elem, None, tail)),
		}
	}

	/// Produces a `Domain::Region` with neither `head` nor `tail`, but only a
	/// `body`.
	#[inline]
	fn spanning(
		addr: Address<M, T>,
		elts: usize,
		_: BitIdx<T::Mem>,
		_: BitEnd<T::Mem>,
	) -> Self {
		Self::Region {
			head: None,
			body: unsafe {
				<Address<M, [T::Unalias]> as SliceReferential>::from_raw_parts(
					addr.cast::<T::Unalias>(),
					elts,
				)
			},
			tail: None,
		}
	}
}

impl<'a, M, T, O> Default for Domain<'a, M, T, O>
where
	M: Mutability,
	T: 'a + BitStore,
	O: BitOrder,
	Address<M, T>: Referential<'a>,
	Address<M, [T::Unalias]>: SliceReferential<'a>,
	Reference<'a, M, [T::Unalias]>: Default,
{
	#[inline]
	fn default() -> Self {
		Self::Region {
			head: None,
			body: Reference::<M, [T::Unalias]>::default(),
			tail: None,
		}
	}
}

impl<'a, M, T, O> Debug for Domain<'a, M, T, O>
where
	M: Mutability,
	T: 'a + BitStore,
	O: BitOrder,
	Address<M, T>: Referential<'a>,
	Address<M, [T::Unalias]>: SliceReferential<'a>,
	Reference<'a, M, [T::Unalias]>: Debug,
{
	#[inline]
	fn fmt(&self, fmt: &mut Formatter) -> fmt::Result {
		write!(
			fmt,
			"Domain::<{} {}, {}>::",
			M::RENDER,
			any::type_name::<T>(),
			any::type_name::<O>(),
		)?;
		match self {
			Self::Enclave(elem) => {
				fmt.debug_tuple("Enclave").field(elem).finish()
			},
			Self::Region { head, body, tail } => fmt
				.debug_struct("Region")
				.field("head", head)
				.field("body", body)
				.field("tail", tail)
				.finish(),
		}
	}
}

#[cfg(not(tarpaulin_include))]
impl<T, O> Clone for Domain<'_, Const, T, O>
where
	T: BitStore,
	O: BitOrder,
{
	#[inline]
	fn clone(&self) -> Self {
		*self
	}
}

impl<T, O> Iterator for Domain<'_, Const, T, O>
where
	T: BitStore,
	O: BitOrder,
{
	type Item = T::Mem;

	#[inline]
	fn next(&mut self) -> Option<Self::Item> {
		match self {
			Self::Enclave(elem) => {
				elem.load_value().tap(|_| *self = Default::default()).into()
			},
			Self::Region { head, body, tail } => {
				if let Some(elem) = head.take() {
					return elem.load_value().into();
				}
				if let Some((elem, rest)) = body.split_first() {
					*body = rest;
					return elem.load_value().into();
				}
				if let Some(elem) = tail.take() {
					return elem.load_value().into();
				}
				None
			},
		}
	}
}

impl<T, O> DoubleEndedIterator for Domain<'_, Const, T, O>
where
	T: BitStore,
	O: BitOrder,
{
	#[inline]
	fn next_back(&mut self) -> Option<Self::Item> {
		match self {
			Self::Enclave(elem) => {
				elem.load_value().tap(|_| *self = Default::default()).into()
			},
			Self::Region { head, body, tail } => {
				if let Some(elem) = tail.take() {
					return elem.load_value().into();
				}
				if let Some((elem, rest)) = body.split_last() {
					*body = rest;
					return elem.load_value().into();
				}
				if let Some(elem) = head.take() {
					return elem.load_value().into();
				}
				None
			},
		}
	}
}

impl<T, O> ExactSizeIterator for Domain<'_, Const, T, O>
where
	T: BitStore,
	O: BitOrder,
{
	#[inline]
	fn len(&self) -> usize {
		match self {
			Self::Enclave(_) => 1,
			Self::Region { head, body, tail } => {
				head.is_some() as usize + body.len() + tail.is_some() as usize
			},
		}
	}
}

impl<T, O> FusedIterator for Domain<'_, Const, T, O>
where
	T: BitStore,
	O: BitOrder,
{
}

impl<T, O> Copy for Domain<'_, Const, T, O>
where
	T: BitStore,
	O: BitOrder,
{
}

/// Implements numeric formatting by rendering each element.
macro_rules! fmt {
	($($fmt:ty => $fwd:ident),+ $(,)?) => { $(
		impl<'a, T, O> $fmt for Domain<'a, Const, T, O>
		where
			O: BitOrder,
			T: BitStore,
		{
			#[inline]
			fn fmt(&self, fmt: &mut Formatter) -> fmt::Result {
				fmt.debug_list()
					.entries(self.into_iter().map(FmtForward::$fwd))
					.finish()
			}
		}
	)+ };
}

fmt! {
	Binary => fmt_binary,
	Display => fmt_display,
	LowerHex => fmt_lower_hex,
	Octal => fmt_octal,
	UpperHex => fmt_upper_hex,
}

#[doc = include_str!("../doc/domain/PartialElement.md")]
pub struct PartialElement<'a, M, T, O>
where
	M: Mutability,
	T: 'a + BitStore,
	O: BitOrder,
{
	/// The address of the memory element being partially viewed.
	///
	/// This must be stored as a pointer, not a reference, because it must
	/// retain mutability permissions but cannot have an `&mut` reference to
	/// a shared element.
	///
	/// Similarly, it must remain typed as `T`, not `T::Access`, to allow the
	/// `<Const, uN>` case not to inappropriately produce a `<Const, Cell<uN>>`
	/// even if no write is performed.
	elem: Address<M, T>,
	/// Cache the selector mask, so it never needs to be recomputed.
	mask: BitMask<T::Mem>,
	/// The starting index.
	head: BitIdx<T::Mem>,
	/// The ending index.
	tail: BitEnd<T::Mem>,
	/// Preserve the originating bit-order
	_ord: PhantomData<O>,
	/// This type acts as-if it were a shared-mutable reference.
	_ref: PhantomData<&'a T::Access>,
}

impl<'a, M, T, O> PartialElement<'a, M, T, O>
where
	M: Mutability,
	T: 'a + BitStore,
	O: BitOrder,
{
	/// Constructs a new partial-element guarded reference.
	///
	/// ## Parameters
	///
	/// - `elem`: the element to which this partially points.
	/// - `head`: the index at which the partial region begins.
	/// - `tail`: the index at which the partial region ends.
	#[inline]
	fn new(
		elem: Address<M, T>,
		head: impl Into<Option<BitIdx<T::Mem>>>,
		tail: impl Into<Option<BitEnd<T::Mem>>>,
	) -> Self {
		let (head, tail) = (
			head.into().unwrap_or(BitIdx::MIN),
			tail.into().unwrap_or(BitEnd::MAX),
		);
		Self {
			elem,
			mask: O::mask(head, tail),
			head,
			tail,
			_ord: PhantomData,
			_ref: PhantomData,
		}
	}

	/// Fetches the value stored through `self` and masks away extra bits.
	///
	/// ## Returns
	///
	/// A bit-map containing any bits set to `1` in the governed bits. All other
	/// bits are cleared to `0`.
	#[inline]
	pub fn load_value(&self) -> T::Mem {
		self.elem
			.pipe(|addr| unsafe { &*addr.to_const() })
			.load_value()
			& self.mask.into_inner()
	}

	/// Gets the starting index of the live bits in the element.
	#[inline]
	#[cfg(not(tarpaulin_include))]
	pub fn head(&self) -> BitIdx<T::Mem> {
		self.head
	}

	/// Gets the ending index of the live bits in the element.
	#[inline]
	#[cfg(not(tarpaulin_include))]
	pub fn tail(&self) -> BitEnd<T::Mem> {
		self.tail
	}

	/// Gets the semantic head and tail indices that constrain which bits of the
	/// referent element may be accessed.
	#[inline]
	#[cfg(not(tarpaulin_include))]
	pub fn bounds(&self) -> (BitIdx<T::Mem>, BitEnd<T::Mem>) {
		(self.head, self.tail)
	}

	/// Gets the bit-mask over all accessible bits.
	#[inline]
	#[cfg(not(tarpaulin_include))]
	pub fn mask(&self) -> BitMask<T::Mem> {
		self.mask
	}

	/// Converts the partial element into a bit-slice over its governed bits.
	#[inline]
	pub fn into_bitslice(self) -> Reference<'a, M, BitSlice<T, O>>
	where Address<M, BitSlice<T, O>>: Referential<'a> {
		unsafe {
			BitSpan::new_unchecked(
				self.elem,
				self.head,
				(self.tail.into_inner() - self.head.into_inner()) as usize,
			)
		}
		.to_bitslice()
	}
}

impl<'a, T, O> PartialElement<'a, Mut, T, O>
where
	T: BitStore,
	O: BitOrder,
	Address<Mut, T>: Referential<'a>,
{
	/// Stores a value through `self` after masking away extra bits.
	///
	/// ## Parameters
	///
	/// - `&mut self`
	/// - `value`: A bit-map which will be written into the governed bits. This
	///   is a bit-map store, not an integer store; the value will not be
	///   shifted into position and will only be masked directly against the
	///   bits that this partial-element governs.
	///
	/// ## Returns
	///
	/// The previous value of the governed bits.
	#[inline]
	pub fn store_value(&mut self, value: T::Mem) -> T::Mem {
		let this = self.access();
		let prev = this.clear_bits(self.mask);
		this.set_bits(self.mask & value);
		prev & self.mask.into_inner()
	}

	/// Inverts the value of each bit governed by the partial-element.
	///
	/// ## Returns
	///
	/// The previous value of the governed bits.
	#[inline]
	#[cfg(not(tarpaulin_include))]
	pub fn invert(&mut self) -> T::Mem {
		self.access().invert_bits(self.mask) & self.mask.into_inner()
	}

	/// Clears all bits governed by the partial-element to `0`.
	///
	/// ## Returns
	///
	/// The previous value of the governed bits.
	#[inline]
	#[cfg(not(tarpaulin_include))]
	pub fn clear(&mut self) -> T::Mem {
		self.access().clear_bits(self.mask) & self.mask.into_inner()
	}

	/// Sets all bits governed by the partial-element to `1`.
	///
	/// ## Returns
	///
	/// The previous value of the governed bits.
	#[inline]
	#[cfg(not(tarpaulin_include))]
	pub fn set(&mut self) -> T::Mem {
		self.access().set_bits(self.mask) & self.mask.into_inner()
	}

	/// Produces a reference capable of tolerating other handles viewing the
	/// same *memory element*.
	#[inline]
	fn access(&self) -> &T::Access {
		unsafe { &*self.elem.to_const().cast::<T::Access>() }
	}
}

impl<'a, M, T, O> PartialElement<'a, M, T, O>
where
	M: Mutability,
	O: BitOrder,
	T: 'a + BitStore + radium::Radium,
{
	/// Performs a store operation on a partial-element whose bits might be
	/// observed by another handle.
	#[inline]
	pub fn store_value_aliased(&self, value: T::Mem) -> T::Mem {
		let this = unsafe { &*self.elem.to_const().cast::<T::Access>() };
		let prev = this.clear_bits(self.mask);
		this.set_bits(self.mask & value);
		prev & self.mask.into_inner()
	}
}

#[cfg(not(tarpaulin_include))]
impl<'a, T, O> Clone for PartialElement<'a, Const, T, O>
where
	T: BitStore,
	O: BitOrder,
	Address<Const, T>: Referential<'a>,
{
	#[inline]
	fn clone(&self) -> Self {
		*self
	}
}

impl<'a, M, T, O> Debug for PartialElement<'a, M, T, O>
where
	M: Mutability,
	T: 'a + BitStore,
	O: BitOrder,
{
	#[inline]
	fn fmt(&self, fmt: &mut Formatter) -> fmt::Result {
		write!(
			fmt,
			"PartialElement<{} {}, {}>",
			M::RENDER,
			any::type_name::<T>(),
			any::type_name::<O>(),
		)?;
		fmt.debug_struct("")
			.field("elem", &self.load_value())
			.field("mask", &self.mask.fmt_display())
			.field("head", &self.head.fmt_display())
			.field("tail", &self.tail.fmt_display())
			.finish()
	}
}

#[cfg(not(tarpaulin_include))]
impl<'a, M, T, O> Hash for PartialElement<'a, M, T, O>
where
	M: Mutability,
	T: 'a + BitStore,
	O: BitOrder,
{
	#[inline]
	fn hash<H>(&self, hasher: &mut H)
	where H: Hasher {
		self.load_value().hash(hasher);
		self.mask.hash(hasher);
		self.head.hash(hasher);
		self.tail.hash(hasher);
	}
}

impl<T, O> Copy for PartialElement<'_, Const, T, O>
where
	T: BitStore,
	O: BitOrder,
{
}

#[cfg(test)]
mod tests {
	use rand::random;

	use super::*;
	use crate::prelude::*;

	#[test]
	fn bit_domain() {
		let data = BitArray::<[u32; 3], Msb0>::new(random());

		let bd = data.bit_domain();
		assert!(bd.enclave().is_none());
		let (head, body, tail) = bd.region().unwrap();
		assert_eq!(data, body);
		assert!(head.is_empty());
		assert!(tail.is_empty());

		let bd = data[2 ..].bit_domain();
		let (head, body, tail) = bd.region().unwrap();
		assert_eq!(head, &data[2 .. 32]);
		assert_eq!(body, &data[32 ..]);
		assert!(tail.is_empty());

		let bd = data[.. 94].bit_domain();
		let (head, body, tail) = bd.region().unwrap();
		assert!(head.is_empty());
		assert_eq!(body, &data[.. 64]);
		assert_eq!(tail, &data[64 .. 94]);

		let bd = data[2 .. 94].bit_domain();
		let (head, body, tail) = bd.region().unwrap();
		assert_eq!(head, &data[2 .. 32]);
		assert_eq!(body, &data[32 .. 64]);
		assert_eq!(tail, &data[64 .. 94]);

		let bd = data[34 .. 62].bit_domain();
		assert!(bd.region().is_none());
		assert_eq!(bd.enclave().unwrap(), data[34 .. 62]);

		let (head, body, tail) =
			BitDomain::<Const, usize, Lsb0>::default().region().unwrap();
		assert!(head.is_empty());
		assert!(body.is_empty());
		assert!(tail.is_empty());
	}

	#[test]
	fn domain() {
		let data: [u32; 3] = random();
		let bits = data.view_bits::<Msb0>();

		let d = bits.domain();
		assert!(d.enclave().is_none());
		let (head, body, tail) = d.region().unwrap();
		assert!(head.is_none());
		assert!(tail.is_none());
		assert_eq!(body, data);

		let d = bits[2 ..].domain();
		let (head, body, tail) = d.region().unwrap();
		assert_eq!(head.unwrap().load_value(), (data[0] << 2) >> 2);
		assert_eq!(body, &data[1 ..]);
		assert!(tail.is_none());

		let d = bits[.. 94].domain();
		let (head, body, tail) = d.region().unwrap();
		assert!(head.is_none());
		assert_eq!(body, &data[.. 2]);
		assert_eq!(tail.unwrap().load_value(), (data[2] >> 2) << 2);

		let d = bits[2 .. 94].domain();
		let (head, body, tail) = d.region().unwrap();
		assert_eq!(head.unwrap().load_value(), (data[0] << 2) >> 2);
		assert_eq!(body, &data[1 .. 2]);
		assert_eq!(tail.unwrap().load_value(), (data[2] >> 2) << 2);

		let d = bits[34 .. 62].domain();
		assert!(d.region().is_none());
		assert_eq!(
			d.enclave().unwrap().load_value(),
			((data[1] << 2) >> 4) << 2,
		);

		assert!(matches!(bits![].domain(), Domain::Region {
			head: None,
			body: &[],
			tail: None,
		}));

		assert!(matches!(
			Domain::<Const, usize, Lsb0>::default(),
			Domain::Region {
				head: None,
				body: &[],
				tail: None,
			},
		));

		let data = core::cell::Cell::new(0u8);
		let partial =
			data.view_bits::<Lsb0>()[2 .. 6].domain().enclave().unwrap();
		assert_eq!(partial.store_value_aliased(!0), 0);
		assert_eq!(data.get(), 0b00_1111_00);
	}

	#[test]
	fn iter() {
		let bits = [0x12u8, 0x34, 0x56].view_bits::<Lsb0>();
		let mut domain = bits[4 .. 12].domain();
		assert_eq!(domain.len(), 2);
		assert_eq!(domain.next().unwrap(), 0x10);
		assert_eq!(domain.next_back().unwrap(), 0x04);

		assert!(domain.next().is_none());
		assert!(domain.next_back().is_none());

		assert_eq!(bits[2 .. 6].domain().len(), 1);
		assert_eq!(bits[18 .. 22].domain().next_back().unwrap(), 0b00_0101_00);

		let mut domain = bits[4 .. 20].domain();
		assert_eq!(domain.next_back().unwrap(), 0x06);
		assert_eq!(domain.next_back().unwrap(), 0x34);
		assert_eq!(domain.next_back().unwrap(), 0x10);
	}

	#[test]
	#[cfg(feature = "alloc")]
	fn render() {
		#[cfg(not(feature = "std"))]
		use alloc::format;

		let data = BitArray::<u32, Msb0>::new(random());

		let render = format!("{:?}", data.bit_domain());
		let expected = format!(
			"BitDomain::<*const u32, {}>::Region {{ head: {:?}, body: {:?}, \
			 tail: {:?} }}",
			any::type_name::<Msb0>(),
			BitSlice::<u32, Msb0>::empty(),
			data.as_bitslice(),
			BitSlice::<u32, Msb0>::empty(),
		);
		assert_eq!(render, expected);

		let render = format!("{:?}", data[2 .. 30].bit_domain());
		let expected = format!(
			"BitDomain::<*const u32, {}>::Enclave({:?})",
			any::type_name::<Msb0>(),
			&data[2 .. 30],
		);
		assert_eq!(render, expected);

		let render = format!("{:?}", data.domain());
		let expected = format!(
			"Domain::<*const u32, {}>::Region {{ head: None, body: {:?}, tail: \
			 None }}",
			any::type_name::<Msb0>(),
			data.as_raw_slice(),
		);
		assert_eq!(render, expected);

		let render = format!("{:?}", data[2 .. 30].domain());
		let expected = format!(
			"Domain::<*const u32, {}>::Enclave",
			any::type_name::<Msb0>(),
		);
		assert!(render.starts_with(&expected));

		let partial = 0x3Cu8.view_bits::<Lsb0>()[2 .. 6]
			.domain()
			.enclave()
			.unwrap();
		let render = format!("{:?}", partial);
		assert_eq!(
			render,
			format!(
				"PartialElement<*const u8, {}> {{ elem: 60, mask: {}, head: \
				 {}, tail: {} }}",
				any::type_name::<Lsb0>(),
				partial.mask,
				partial.head,
				partial.tail,
			),
		);
	}
}
