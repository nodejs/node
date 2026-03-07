#![doc(hidden)]
#![doc = include_str!("../../doc/macros/internal.md")]

//  Provide known mount-points of dependency crates.

#[doc(hidden)]
pub use core;

#[doc(hidden)]
pub use funty;

#[doc(hidden)]
#[macro_export]
#[doc = include_str!("../../doc/macros/encode_bits.md")]
macro_rules! __encode_bits {
	/* ENTRY POINTS
	 *
	 * These arms match the syntax provided by the public macros, and dispatch
	 * by storage type width.
	 */

	(u8, $ord:tt; $($val:expr),*) => {
		$crate::__encode_bits!(u8 as u8, $ord; $($val),*)
	};
	(Cell<u8>, $ord:tt; $($val:expr),*) => {
		$crate::__encode_bits!(Cell<u8> as u8, $ord; $($val),*)
	};
	(AtomicU8, $ord:tt; $($val:expr),*) => {
		$crate::__encode_bits!(AtomicU8 as u8, $ord; $($val),*)
	};
	(RadiumU8, $ord:tt; $($val:expr),*) => {
		$crate::__encode_bits!(RadiumU8 as u8, $ord; $($val),*)
	};

	(u16, $ord:tt; $($val:expr),*) => {
		$crate::__encode_bits!(u16 as u16, $ord; $($val),*)
	};
	(Cell<u16>, $ord:tt; $($val:expr),*) => {
		$crate::__encode_bits!(Cell<u16> as u16, $ord; $($val),*)
	};
	(AtomicU16, $ord:tt; $($val:expr),*) => {
		$crate::__encode_bits!(AtomicU16 as u16, $ord; $($val),*)
	};
	(RadiumU16, $ord:tt; $($val:expr),*) => {
		$crate::__encode_bits!(RadiumU16 as u16, $ord; $($val),*)
	};

	(u32, $ord:tt; $($val:expr),*) => {
		$crate::__encode_bits!(u32 as u32, $ord; $($val),*)
	};
	(Cell<u32>, $ord:tt; $($val:expr),*) => {
		$crate::__encode_bits!(Cell<u32> as u32, $ord; $($val),*)
	};
	(AtomicU32, $ord:tt; $($val:expr),*) => {
		$crate::__encode_bits!(AtomicU32 as u32, $ord; $($val),*)
	};
	(RadiumU32, $ord:tt; $($val:expr),*) => {
		$crate::__encode_bits!(RadiumU32 as u32, $ord; $($val),*)
	};

	(u64, $ord:tt; $($val:expr),*) => {
		$crate::__encode_bits!(u64 as u64, $ord; $($val),*)
	};
	(Cell<u64>, $ord:tt; $($val:expr),*) => {
		$crate::__encode_bits!(Cell<u64> as u64, $ord; $($val),*)
	};
	(AtomicU64, $ord:tt; $($val:expr),*) => {
		$crate::__encode_bits!(AtomicU64 as u64, $ord; $($val),*)
	};
	(RadiumU64, $ord:tt; $($val:expr),*) => {
		$crate::__encode_bits!(RadiumU64 as u64, $ord; $($val),*)
	};

	(usize, $ord:tt; $($val:expr),*) => {
		$crate::__encode_bits!(usize as usize, $ord; $($val),*)
	};
	(Cell<usize>, $ord:tt; $($val:expr),*) => {
		$crate::__encode_bits!(Cell<usize> as usize, $ord; $($val),*)
	};
	(AtomicUsize, $ord:tt; $($val:expr),*) => {
		$crate::__encode_bits!(AtomicUsize as usize, $ord; $($val),*)
	};
	(RadiumUsize, $ord:tt; $($val:expr),*) => {
		$crate::__encode_bits!(RadiumUsize as usize, $ord; $($val),*)
	};

	//  This arm routes `usize` into `u32` or `u64`, depending on target, and
	//  marks them to return to `usize` after chunking.
	($typ:ty as usize, $ord:tt; $($val:expr),*) => {{
		const LEN: usize = $crate::__count_elts!(usize; $($val),*);

		let out: [$typ; LEN];

		#[cfg(target_pointer_width = "32")]
		{
			out = $crate::__encode_bits!($typ as u32 as usize, $ord; $($val),*);
		}

		#[cfg(target_pointer_width = "64")]
		{
			out = $crate::__encode_bits!($typ as u64 as usize, $ord; $($val),*);
		}

		out
	}};

	//  ZERO EXTENSION: Supply literal `0, ` tokens to ensure that elements can
	//  be completely filled with bits.
	($typ:ty as $uint:ident $(as $usz:ident)?, $ord:tt; $($val:expr),*) => {
		$crate::__encode_bits!(
			$typ as $uint $(as $usz)?, $ord; []; $($val,)*
			0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // 16
			0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // 32
			0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // 48
			0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // 64
		)
	};

	/* EXIT POINT.
	 *
	 * This arm enters once the only remaining bit-expression tokens are the
	 * literal `0, `s provided above. It does not enter while any opaque
	 * user-provided bit expressions remain, and matching falls through to the
	 * chunkers, below.
	 *
	 * Once entered, this converts each chunk of bit expressions into the
	 * requested storage element, then emits an array of the encoded elements.
	 * This array is the final value of the originally-invoked macro. The
	 * invoker is responsible for turning the array into a `bitvec` type.
	 */
	(
		$typ:ty as $uint:ident as usize, $ord:tt;
		[$([$($bit:tt,)+],)*]; $(0,)*
	) => {
		[$($crate::__make_elem!($typ as $uint as usize, $ord; $($bit,)+),)*]
	};
	(
		$typ:ty as $uint:ident, $ord:tt;
		[$([$($bit:tt,)+],)*]; $(0,)*
	) => {
		[$($crate::__make_elem!($typ as $uint, $ord; $($bit,)+),)*]
	};

	/* CHUNKERS
	 *
	 * These arms munch through the token stream, creating a sequence of chunks
	 * of bits. Each chunk contains bits to exactly fill one element, and gets
	 * passed into `__make_elem!` for final encoding.
	 */

	(
		$typ:ty as u8, $ord:tt; [$($elem:tt)*];
		$a0:tt, $b0:tt, $c0:tt, $d0:tt, $e0:tt, $f0:tt, $g0:tt, $h0:tt,
		$($t:tt)*
	) => {
		$crate::__encode_bits!(
			$typ as u8, $ord; [$($elem)* [
				$a0, $b0, $c0, $d0, $e0, $f0, $g0, $h0,
			],]; $($t)*
		)
	};

	(
		$typ:ty as u16, $ord:tt; [$($elem:tt)*];
		$a0:tt, $b0:tt, $c0:tt, $d0:tt, $e0:tt, $f0:tt, $g0:tt, $h0:tt,
		$a1:tt, $b1:tt, $c1:tt, $d1:tt, $e1:tt, $f1:tt, $g1:tt, $h1:tt,
		$($t:tt)*
	) => {
		$crate::__encode_bits!(
			$typ as u16, $ord; [$($elem)* [
				$a0, $b0, $c0, $d0, $e0, $f0, $g0, $h0,
				$a1, $b1, $c1, $d1, $e1, $f1, $g1, $h1,
			],]; $($t)*
		)
	};

	(
		$typ:ty as u32 $(as $usz:ident)?, $ord:tt; [$($elem:tt)*];
		$a0:tt, $b0:tt, $c0:tt, $d0:tt, $e0:tt, $f0:tt, $g0:tt, $h0:tt,
		$a1:tt, $b1:tt, $c1:tt, $d1:tt, $e1:tt, $f1:tt, $g1:tt, $h1:tt,
		$a2:tt, $b2:tt, $c2:tt, $d2:tt, $e2:tt, $f2:tt, $g2:tt, $h2:tt,
		$a3:tt, $b3:tt, $c3:tt, $d3:tt, $e3:tt, $f3:tt, $g3:tt, $h3:tt,
		$($t:tt)*
	) => {
		$crate::__encode_bits!(
			$typ as u32 $(as $usz)?, $ord; [$($elem)* [
				$a0, $b0, $c0, $d0, $e0, $f0, $g0, $h0,
				$a1, $b1, $c1, $d1, $e1, $f1, $g1, $h1,
				$a2, $b2, $c2, $d2, $e2, $f2, $g2, $h2,
				$a3, $b3, $c3, $d3, $e3, $f3, $g3, $h3,
			],]; $($t)*
		)
	};

	(
		$typ:ty as u64 $(as $usz:ident)?, $ord:tt; [$($elem:tt)*];
		$a0:tt, $b0:tt, $c0:tt, $d0:tt, $e0:tt, $f0:tt, $g0:tt, $h0:tt,
		$a1:tt, $b1:tt, $c1:tt, $d1:tt, $e1:tt, $f1:tt, $g1:tt, $h1:tt,
		$a2:tt, $b2:tt, $c2:tt, $d2:tt, $e2:tt, $f2:tt, $g2:tt, $h2:tt,
		$a3:tt, $b3:tt, $c3:tt, $d3:tt, $e3:tt, $f3:tt, $g3:tt, $h3:tt,
		$a4:tt, $b4:tt, $c4:tt, $d4:tt, $e4:tt, $f4:tt, $g4:tt, $h4:tt,
		$a5:tt, $b5:tt, $c5:tt, $d5:tt, $e5:tt, $f5:tt, $g5:tt, $h5:tt,
		$a6:tt, $b6:tt, $c6:tt, $d6:tt, $e6:tt, $f6:tt, $g6:tt, $h6:tt,
		$a7:tt, $b7:tt, $c7:tt, $d7:tt, $e7:tt, $f7:tt, $g7:tt, $h7:tt,
		$($t:tt)*
	) => {
		$crate::__encode_bits!(
			$typ as u64 $(as $usz)?, $ord; [$($elem)* [
				$a0, $b0, $c0, $d0, $e0, $f0, $g0, $h0,
				$a1, $b1, $c1, $d1, $e1, $f1, $g1, $h1,
				$a2, $b2, $c2, $d2, $e2, $f2, $g2, $h2,
				$a3, $b3, $c3, $d3, $e3, $f3, $g3, $h3,
				$a4, $b4, $c4, $d4, $e4, $f4, $g4, $h4,
				$a5, $b5, $c5, $d5, $e5, $f5, $g5, $h5,
				$a6, $b6, $c6, $d6, $e6, $f6, $g6, $h6,
				$a7, $b7, $c7, $d7, $e7, $f7, $g7, $h7,
			],]; $($t)*
		)
	};
}

/// Counts the number of expression tokens in a repetition sequence.
#[doc(hidden)]
#[macro_export]
macro_rules! __count {
	(@ $val:expr) => { 1 };
	($($val:expr),* $(,)?) => {{
		const LEN: usize = 0 $(+ $crate::__count!(@ $val))*;
		LEN
	}};
}

/// Counts the number of storage elements needed to store a bit sequence.
#[doc(hidden)]
#[macro_export]
macro_rules! __count_elts {
	($t:ty; $($val:expr),*) => {
		$crate::mem::elts::<$t>($crate::__count!($($val),*))
	};
}

#[doc(hidden)]
#[macro_export]
#[doc = include_str!("../../doc/macros/make_elem.md")]
macro_rules! __make_elem {
	//  Token-matching ordering names can use specialized work.
	($typ:ty as $uint:ident $(as $usz:ident)?, Lsb0; $(
		$a:expr, $b:expr, $c:expr, $d:expr,
		$e:expr, $f:expr, $g:expr, $h:expr,
	)*) => {{
		const ELEM: $uint = $crate::__ty_from_bytes!(
			$uint, Lsb0, [$($crate::macros::internal::u8_from_le_bits(
				$a != 0, $b != 0, $c != 0, $d != 0,
				$e != 0, $f != 0, $g != 0, $h != 0,
			)),*]
		);
		$crate::mem::BitElement::<$typ>::new(ELEM $(as $usz)?).elem
	}};
	($typ:ty as $uint:ident $(as $usz:ident)?, Msb0; $(
		$a:expr, $b:expr, $c:expr, $d:expr,
		$e:expr, $f:expr, $g:expr, $h:expr,
	)*) => {{
		const ELEM: $uint = $crate::__ty_from_bytes!(
			$uint, Msb0, [$($crate::macros::internal::u8_from_be_bits(
				$a != 0, $b != 0, $c != 0, $d != 0,
				$e != 0, $f != 0, $g != 0, $h != 0,
			)),*]
		);
		$crate::mem::BitElement::<$typ>::new(ELEM $(as $usz)?).elem
	}};
	($typ:ty as $uint:ident $(as $usz:ident)?, LocalBits; $(
		$a:expr, $b:expr, $c:expr, $d:expr,
		$e:expr, $f:expr, $g:expr, $h:expr,
	)*) => {{
		const ELEM: $uint = $crate::__ty_from_bytes!(
			$uint, LocalBits, [$($crate::macros::internal::u8_from_ne_bits(
				$a != 0, $b != 0, $c != 0, $d != 0,
				$e != 0, $f != 0, $g != 0, $h != 0,
			)),*]
		);
		$crate::mem::BitElement::<$typ>::new(ELEM $(as $usz)?).elem
	}};
	//  Otherwise, invoke `BitOrder` for each bit and accumulate.
	($typ:ty as $uint:ident $(as $usz:ident)?, $ord:tt; $($bit:expr),* $(,)?) => {{
		let mut tmp: $uint = 0;
		let _bits = $crate::slice::BitSlice::<$uint, $ord>::from_element_mut(
			&mut tmp
		);
		let mut _idx = 0;
		$( _bits.set(_idx, $bit != 0); _idx += 1; )*
		$crate::mem::BitElement::<$typ>::new(tmp $(as $usz)?).elem
	}};
}

/// Translates `false` into `0` and `true` into `!0`.
#[doc(hidden)]
#[macro_export]
macro_rules! __extend_bool {
	($val:expr, $typ:tt) => {{
		type Mem = <$typ as $crate::store::BitStore>::Mem;
		if $val != 0 {
			<Mem as $crate::mem::BitRegister>::ALL
		}
		else {
			<Mem as $crate::macros::internal::funty::Integral>::ZERO
		}
	}};
}

/// Constructs an unsigned integer from a list of *bytes*.
#[doc(hidden)]
#[macro_export]
macro_rules! __ty_from_bytes {
	(u8, Msb0, [$($byte:expr),*]) => {
		u8::from_be_bytes([$($byte),*])
	};
	(u8, Lsb0, [$($byte:expr),*]) => {
		u8::from_le_bytes([$($byte),*])
	};
	(u8, LocalBits, [$($byte:expr),*]) => {
		u8::from_ne_bytes([$($byte),*])
	};
	(u16, Msb0, [$($byte:expr),*]) => {
		u16::from_be_bytes([$($byte),*])
	};
	(u16, Lsb0, [$($byte:expr),*]) => {
		u16::from_le_bytes([$($byte),*])
	};
	(u16, LocalBits, [$($byte:expr),*]) => {
		u16::from_ne_bytes([$($byte),*])
	};
	(u32, Msb0, [$($byte:expr),*]) => {
		u32::from_be_bytes([$($byte),*])
	};
	(u32, Lsb0, [$($byte:expr),*]) => {
		u32::from_le_bytes([$($byte),*])
	};
	(u32, LocalBits, [$($byte:expr),*]) => {
		u32::from_ne_bytes([$($byte),*])
	};
	(u64, Msb0, [$($byte:expr),*]) => {
		u64::from_be_bytes([$($byte),*])
	};
	(u64, Lsb0, [$($byte:expr),*]) => {
		u64::from_le_bytes([$($byte),*])
	};
	(u64, LocalBits, [$($byte:expr),*]) => {
		u64::from_ne_bytes([$($byte),*])
	};
	(usize, Msb0, [$($byte:expr),*]) => {
		usize::from_be_bytes([$($byte),*])
	};
	(usizeLsb0, , [$($byte:expr),*]) => {
		usize::from_le_bytes([$($byte),*])
	};
	(usize, LocalBits, [$($byte:expr),*]) => {
		usize::from_ne_bytes([$($byte),*])
	};
}

/// Constructs a `u8` from bits applied in `Lsb0` order (`a` low, `h` high).
#[doc(hidden)]
#[inline(always)]
#[cfg(not(tarpaulin_include))]
pub const fn u8_from_le_bits(
	a: bool,
	b: bool,
	c: bool,
	d: bool,
	e: bool,
	f: bool,
	g: bool,
	h: bool,
) -> u8 {
	(a as u8)
		| ((b as u8) << 1)
		| ((c as u8) << 2)
		| ((d as u8) << 3)
		| ((e as u8) << 4)
		| ((f as u8) << 5)
		| ((g as u8) << 6)
		| ((h as u8) << 7)
}

/// Constructs a `u8` from bits applied in `Msb0` order (`a` high, `h` low).
#[doc(hidden)]
#[inline(always)]
#[cfg(not(tarpaulin_include))]
pub const fn u8_from_be_bits(
	a: bool,
	b: bool,
	c: bool,
	d: bool,
	e: bool,
	f: bool,
	g: bool,
	h: bool,
) -> u8 {
	(h as u8)
		| ((g as u8) << 1)
		| ((f as u8) << 2)
		| ((e as u8) << 3)
		| ((d as u8) << 4)
		| ((c as u8) << 5)
		| ((b as u8) << 6)
		| ((a as u8) << 7)
}

#[doc(hidden)]
#[cfg(target_endian = "big")]
pub use self::u8_from_be_bits as u8_from_ne_bits;
#[doc(hidden)]
#[cfg(target_endian = "little")]
pub use self::u8_from_le_bits as u8_from_ne_bits;
