# Bit-Addressable Memory

A slice of individual bits, anywhere in memory.

`BitSlice<T, O>` is an unsized region type; you interact with it through
`&BitSlice<T, O>` and `&mut BitSlice<T, O>` references, which work exactly like
all other Rust references. As with the standard slice’s relationship to arrays
and vectors, this is `bitvec`’s primary working type, but you will probably
hold it through one of the provided [`BitArray`], [`BitBox`], or [`BitVec`]
containers.

`BitSlice` is conceptually a `[bool]` slice, and provides a nearly complete
mirror of `[bool]`’s API.

Every bit-vector crate can give you an opaque type that hides shift/mask
calculations from you. `BitSlice` does far more than this: it offers you the
full Rust guarantees about reference behavior, including lifetime tracking,
mutability and aliasing awareness, and explicit memory control, *as well as* the
full set of tools and APIs available to the standard `[bool]` slice type.
`BitSlice` can arbitrarily split and subslice, just like `[bool]`. You can write
a linear consuming function and keep the patterns you already know.

For example, to trim all the bits off either edge that match a condition, you
could write

```rust
use bitvec::prelude::*;

fn trim<T: BitStore, O: BitOrder>(
  bits: &BitSlice<T, O>,
  to_trim: bool,
) -> &BitSlice<T, O> {
  let stop = |b: bool| b != to_trim;
  let front = bits.iter()
    .by_vals()
    .position(stop)
    .unwrap_or(0);
  let back = bits.iter()
    .by_vals()
    .rposition(stop)
    .map_or(0, |p| p + 1);
  &bits[front .. back]
}
# assert_eq!(trim(bits![0, 0, 1, 1, 0, 1, 0], false), bits![1, 1, 0, 1]);
```

to get behavior something like
`trim(&BitSlice[0, 0, 1, 1, 0, 1, 0], false) == &BitSlice[1, 1, 0, 1]`.

## Documentation

All APIs that mirror something in the standard library will have an `Original`
section linking to the corresponding item. All APIs that have a different
signature or behavior than the original will have an `API Differences` section
explaining what has changed, and how to adapt your existing code to the change.

These sections look like this:

## Original

[`[bool]`](https://doc.rust-lang.org/stable/std/primitive.slice.html)

## API Differences

The slice type `[bool]` has no type parameters. `BitSlice<T, O>` has two: one
for the integer type used as backing storage, and one for the order of bits
within that integer type.

`&BitSlice<T, O>` is capable of producing `&bool` references to read bits out
of its memory, but is not capable of producing `&mut bool` references to write
bits *into* its memory. Any `[bool]` API that would produce a `&mut bool` will
instead produce a [`BitRef<Mut, T, O>`] proxy reference.

## Behavior

`BitSlice` is a wrapper over `[T]`. It describes a region of memory, and must be
handled indirectly. This is most commonly done through the reference types
`&BitSlice` and `&mut BitSlice`, which borrow memory owned by some other value
in the program. These buffers can be directly owned by the sibling types
[`BitBox`], which behaves like [`Box<[T]>`](alloc::boxed::Box), and [`BitVec`],
which behaves like [`Vec<T>`]. It cannot be used as the type parameter to a
pointer type such as `Box`, `Rc`, `Arc`, or any other indirection.

The `BitSlice` region provides access to each individual bit in the region, as
if each bit had a memory address that you could use to dereference it. It packs
each logical bit into exactly one bit of storage memory, just like
[`std::bitset`] and [`std::vector<bool>`] in C++.

## Type Parameters

`BitSlice` has two type parameters which propagate through nearly every public
API in the crate. These are very important to its operation, and your choice
of type arguments informs nearly every part of this library’s behavior.

### `T: BitStore`

[`BitStore`] is the simpler of the two parameters. It refers to the integer type
used to hold bits. It must be one of the Rust unsigned integer fundamentals:
`u8`, `u16`, `u32`, `usize`, and on 64-bit systems only, `u64`. In addition, it
can also be an alias-safe wrapper over them (see the [`access`] module) in
order to permit bit-slices to share underlying memory without interfering with
each other.

`BitSlice` references can only be constructed over the integers, not over their
aliasing wrappers. `BitSlice` will only use aliasing types in its `T` slots when
you invoke APIs that produce them, such as [`.split_at_mut()`].

The default type argument is `usize`.

The argument you choose is used as the basis of a `[T]` slice, over which the
`BitSlice` view is produced. `BitSlice<T, _>` is subject to all of the rules
about alignment that `[T]` is. If you are working with in-memory representation
formats, chances are that you already have a `T` type with which you’ve been
working, and should use it here.

If you are only using this crate to discard the seven wasted bits per `bool`
in a collection of `bool`s, and are not too concerned about the in-memory
representation, then you should use the default type argument of `usize`. This
is because most processors work best when moving an entire `usize` between
memory and the processor itself, and using a smaller type may cause it to slow
down. Additionally, processor instructions are typically optimized for the whole
register, and the processor might need to do additional clearing work for
narrower types.

### `O: BitOrder`

[`BitOrder`] is the more complex parameter. It has a default argument which,
like `usize`, is a good baseline choice when you do not explicitly need to
control the representation of bits in memory.

This parameter determines how `bitvec` indexes the bits within a single `T`
memory element. Computers all agree that in a slice of `T` elements, the element
with the lower index has a lower memory address than the element with the higher
index. But the individual bits within an element do not have addresses, and so
there is no uniform standard of which bit is the zeroth, which is the first,
which is the penultimate, and which is the last.

To make matters even more confusing, there are two predominant ideas of
in-element ordering that often *correlate* with the in-element *byte* ordering
of integer types, but are in fact wholly unrelated! `bitvec` provides these two
main orderings as types for you, and if you need a different one, it also
provides the tools you need to write your own.

#### Least Significant Bit Comes First

This ordering, named the [`Lsb0`] type, indexes bits within an element by
placing the `0` index at the least significant bit (numeric value `1`) and the
final index at the most significant bit (numeric value [`T::MIN`][minval] for
signed integers on most machines).

For example, this is the ordering used by most C compilers to lay out bit-field
struct members on little-endian **byte**-ordered machines.

#### Most Significant Bit Comes First

This ordering, named the [`Msb0`] type, indexes bits within an element by
placing the `0` index at the most significant bit (numeric value
[`T::MIN`][minval] for most signed integers) and the final index at the least
significant bit (numeric value `1`).

For example, this is the ordering used by the [TCP wire format][tcp], and by
most C compilers to lay out bit-field struct members on big-endian
**byte**-ordered machines.

#### Default Ordering

The default ordering is [`Lsb0`], as it typically produces shorter object code
than [`Msb0`] does. If you are implementing a collection, then `Lsb0` will
likely give you better performance; if you are implementing a buffer protocol,
then your choice of ordering is dictated by the protocol definition.

## Safety

`BitSlice` is designed to never introduce new memory unsafety that you did not
provide yourself, either before or during the use of this crate. However, safety
bugs have been identified before, and you are welcome to submit any discovered
flaws as a defect report.

The `&BitSlice` reference type uses a private encoding scheme to hold all of the
information needed in its stack value. This encoding is **not** part of the
public API of the library, and is not binary-compatible with `&[T]`.
Furthermore, in order to satisfy Rust’s requirements about alias conditions,
`BitSlice` performs type transformations on the `T` parameter to ensure that it
never creates the potential for undefined behavior or data races.

You must never attempt to type-cast a reference to `BitSlice` in any way. You
must not use [`mem::transmute`] with `BitSlice` anywhere in its type arguments.
You must not use `as`-casting to convert between `*BitSlice` and any other type.
You must not attempt to modify the binary representation of a `&BitSlice`
reference value. These actions will all lead to runtime memory unsafety, are
(hopefully) likely to induce a program crash, and may possibly cause undefined
behavior at compile-time.

Everything in the `BitSlice` public API, even the `unsafe` parts, are guaranteed
to have no more unsafety than their equivalent items in the standard library.
All `unsafe` APIs will have documentation explicitly detailing what the API
requires you to uphold in order for it to function safely and correctly. All
safe APIs will do so themselves.

## Performance

Like the standard library’s `[T]` slice, `BitSlice` is designed to be very easy
to use safely, while supporting `unsafe` usage when necessary. Rust has a
powerful optimizing engine, and `BitSlice` will frequently be compiled to have
zero runtime cost. Where it is slower, it will not be significantly slower than
a manual replacement.

As the machine instructions operate on registers rather than bits, your choice
of [`T: BitStore`] type parameter can influence your bits-slice’s performance.
Using larger register types means that bit-slices can gallop over
completely-used interior elements faster, while narrower register types permit
more graceful handling of subslicing and aliased splits.

## Construction

`BitSlice` views of memory can be constructed over borrowed data in a number of
ways. As this is a reference-only type, it can only ever be built by borrowing
an existing memory buffer and taking temporary control of your program’s view of
the region.

### Macro Constructor

`BitSlice` buffers can be constructed at compile-time through the [`bits!`]
macro. This macro accepts a superset of the [`vec!`] arguments, and creates an
appropriate buffer in the local scope. The macro expands to a borrowed
[`BitArray`] temporary, which will live for the duration of the bound name.

```rust
use bitvec::prelude::*;

let immut = bits![u8, Lsb0; 0, 1, 0, 0, 1, 0, 0, 1];
let mutable: &mut BitSlice<_, _> = bits![mut u8, Msb0; 0; 8];

assert_ne!(immut, mutable);
mutable.clone_from_bitslice(immut);
assert_eq!(immut, mutable);
```

### Borrowing Constructors

You may borrow existing elements or slices with the following functions:

- [`from_element`] and [`from_element_mut`],
- [`from_slice`] and [`from_slice_mut`],
- [`try_from_slice`] and [`try_from_slice_mut`]

These take references to existing memory and construct `BitSlice` references
from them. These are the most basic ways to borrow memory and view it as bits;
however, you should prefer the [`BitView`] trait methods instead.

```rust
use bitvec::prelude::*;

let data = [0u16; 3];
let local_borrow = BitSlice::<_, Lsb0>::from_slice(&data);

let mut data = [0u8; 5];
let local_mut = BitSlice::<_, Lsb0>::from_slice_mut(&mut data);
```

### Trait Method Constructors

The [`BitView`] trait implements [`.view_bits::<O>()`] and
[`.view_bits_mut::<O>()`] methods on elements, arrays, and slices. This trait,
imported in the crate prelude, is *probably* the easiest way for you to borrow
memory as bits.

```rust
use bitvec::prelude::*;

let data = [0u32; 5];
let trait_view = data.view_bits::<Lsb0>();

let mut data = 0usize;
let trait_mut = data.view_bits_mut::<Msb0>();
```

### Owned Bit Slices

If you wish to take ownership of a memory region and enforce that it is always
viewed as a `BitSlice` by default, you can use one of the [`BitArray`],
[`BitBox`], or [`BitVec`] types, rather than pairing ordinary buffer types with
the borrowing constructors.

```rust
use bitvec::prelude::*;

let slice = bits![0; 27];
let array = bitarr![u8, LocalBits; 0; 10];
# #[cfg(feature = "alloc")] fn allocates() {
let boxed = bitbox![0; 10];
let vec = bitvec![0; 20];
# } #[cfg(feature = "alloc")] allocates();

// arrays always round up
assert_eq!(array.as_bitslice(), slice[.. 16]);
# #[cfg(feature = "alloc")] fn allocates2() {
# let slice = bits![0; 27];
# let boxed = bitbox![0; 10];
# let vec = bitvec![0; 20];
assert_eq!(boxed.as_bitslice(), slice[.. 10]);
assert_eq!(vec.as_bitslice(), slice[.. 20]);
# } #[cfg(feature = "alloc")] allocates2();
```

## Usage

`BitSlice` implements the full standard-library `[bool]` API. The documentation
for these API surfaces is intentionally sparse, and forwards to the standard
library rather than try to replicate it.

`BitSlice` also has a great deal of novel API surfaces. These are broken into
separate `impl` blocks below. A short summary:

- Since there is no `BitSlice` literal, the constructor functions `::empty()`,
  `::from_element()`, `::from_slice()`, and `::try_from_slice()`, and their
  `_mut` counterparts, create bit-slices as needed.
- Since `bits[idx] = value` does not exist, you can use `.set()` or `.replace()`
  (as well as their `_unchecked` and `_aliased` counterparts) to write into a
  bit-slice.
- Raw memory can be inspected with `.domain()` and `.domain_mut()`, and a
  bit-slice can be split on aliasing lines with `.bit_domain()` and
  `.bit_domain_mut()`.
- The population can be queried for which indices have `0` or `1` bits by
  iterating across all such indices, counting them, or counting leading or
  trailing blocks. Additionally, `.any()`, `.all()`, `.not_any()`, `.not_all()`,
  and `.some()` test whether bit-slices satisfy aggregate Boolean qualities.
- Buffer contents can be relocated internally by shifting or rotating to the
  left or right.

## Trait Implementations

`BitSlice` adds trait implementations that `[bool]` and `[T]` do not necessarily
have, including numeric formatting and Boolean arithmetic operators.
Additionally, the [`BitField`] trait allows bit-slices to act as a buffer for
wide-value storage.

[minval]: https://doc.rust-lang.org/stable/std/primitive.usize.html#associatedconstant.MIN
[tcp]: https://en.wikipedia.org/wiki/Transmission_Control_Protocol#TCP_segment_structure

[`BitArray`]: crate::array::BitArray
[`BitBox`]: crate::boxed::BitBox
[`BitField`]: crate::field::BitField
[`BitRef<Mut, T, O>`]: crate::ptr::BitRef
[`BitOrder`]: crate::order::BitOrder
[`BitStore`]: crate::store::BitStore
[`BitVec`]: crate::vec::BitVec
[`BitView`]: crate::view::BitView
[`Cell<T>`]: core::cell::Cell
[`Lsb0`]: crate::order::Lsb0
[`Msb0`]: crate::order::Msb0
[`T: BitStore`]: crate::store::BitStore
[`Vec<T>`]: alloc::vec::Vec

[`access`]: crate::access
[`bits!`]: macro@crate::bits
[`bitvec::prelude::LocalBits`]: crate::order::LocalBits
[`from_element`]: Self::from_element
[`from_element_mut`]: Self::from_element_mut
[`from_slice`]: Self::from_slice
[`from_slice_mut`]: Self::from_slice_mut
[`mem::transmute`]: core::mem::transmute
[`std::bitset`]: https://en.cppreference.com/w/cpp/utility/bitset
[`std::vector<bool>`]: https://en.cppreference.com/w/cpp/container/vector_bool
[`try_from_slice`]: Self::try_from_slice
[`try_from_slice_mut`]: Self::try_from_slice_mut
[`vec!`]: macro@alloc::vec

[`.split_at_mut()`]: Self::split_at_mut
[`.view_bits::<O>()`]: crate::view::BitView::view_bits
[`.view_bits_mut::<O>()`]: crate::view::BitView::view_bits_mut
