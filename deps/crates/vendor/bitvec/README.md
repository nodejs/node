<div style="text-align: center;" align="center">

# `bitvec`

## A Magnifying Glass for Memory <!-- omit in toc -->

[![Crate][crate_img]][crate]
[![Documentation][docs_img]][docs]
[![License][license_img]][license_file]

[![Crate Downloads][downloads_img]][crate]
[![Project Size][loc_img]][loc]

</div>

1. [Summary](#summary)
1. [Introduction](#introduction)
1. [Highlights](#highlights)
1. [Usage](#usage)
1. [Examples](#examples)
1. [User Stories](#user-stories)
   1. [Bit Collections](#bit-collections)
   1. [Bit-Field Memory Access](#bit-field-memory-access)
   1. [Transport Protocols](#transport-protocols)
1. [Feature Flags](#feature-flags)
1. [Deeper Reading](#deeper-reading)

## Summary

`bitvec` provides a foundational API for bitfields in Rust. It specializes
standard-library data structures (slices, arrays, and vectors of `bool`) to use
one-bit-per-`bool` storage, similar to [`std::bitset<N>`] and
[`std::vector<bool>`] in C++.

Additionally, it allows a memory region to be divided into arbitrary regions of
integer storage, like [binaries][erl_bit] in Erlang.

If you need to view memory as bit-addressed instead of byte-addressed, then
`bitvec` is the fastest, most complete, and Rust-idiomatic crate for you.

## Introduction

Computers do not operate on bits. The memory bus is byte-addressed, and
processors operate on register words, which are typically four to eight bytes,
or even wider. This means that when programmers wish to operate on individual
bits within a byte of memory or a word of register, they have to do so manually,
using shift and mask operations that are likely familiar to anyone who has done
this before.

`bitvec` brings the capabilities of C++’s compact `bool` storage and Erlang’s
decomposable bit-streams to Rust, in a package that fits in with your existing
Rust idioms and in the most capable, performant, implementation possible. The
bit-stream behavior provides the logic necessary for C-style structural
bitfields, and syntax sugar for it can be found in [`deku`].

`bitvec` enables you to write code for bit-addressed memory that is simple,
easy, and fast. It compiles to the same, or even better, object code than you
would get from writing shift/mask instructions manually. It leverages Rust’s
powerful reference and type systems to create a system that seamlessly bridges
single-bit addressing, precise control of in-memory layout, and Rust-native
ownership and borrowing mechanisms.

## Highlights

`bitvec` has a number of unique capabilities related to its place as a Rust
library and as a bit-addressing system.

- It supports arbitrary bit-addressing, and its bit slices can be munched from
  the front.
- `BitSlice` is a region type equivalent to `[bool]`, and can be described by
  Rust references and thus fit into reference-based APIs.
- Type parameters enable users to select the precise memory representation they
  desire.
- A memory model accounts for element-level aliasing and is safe for concurrent
  use. In particular, the “Beware Bitfields” bug described in
  [this Mozilla report][moz] is simply impossible to produce.
- Native support for atomic integers as bit-field storage.
- Users can supply their own translation layer for memory representation if the
  built-in translations are insufficient.

However, it does also have some small costs associated with its capabilities:

- `BitSlice` cannot be used as a referent type in pointers, such as `Box`, `Rc`,
  or `Arc`.
- `BitSlice` cannot implement `IndexMut`, so `bitslice[index] = true;` does not
  work.

## Usage

**Minimum Supported Rust Version**: 1.56.0

`bitvec` strives to follow the sequence APIs in the standard library. However,
as most of its functionality is a reïmplementation that does not require the
standard library to actually have the symbols present, doing so may not require
an MSRV raise.

Now that `bitvec` is at 1.0, it will only raise MSRV in minor-edition releases.
If you have a pinned Rust toolchain, you should depend on `bitvec` with a
limiting minor-version constraint like `"~1.0"`.

First, depend on it in your Cargo manifest:

```toml
[dependencies]
bitvec = "1"
```

> Note: `bitvec` supports `#![no_std]` targets. If you do not have `std`,
> disable the default features, and explicitly restore any features that you do
> have:
>
> ```toml
> [dependencies.bitvec]
> version = "1"
> default-features = false
> features = ["atomic", "alloc"]
> ```

Once Cargo knows about it, bring its prelude into scope:

```rust
use bitvec::prelude::*;
```

You can read the [prelude reëxports][prelude] to see exactly which symbols are
being imported. The prelude brings in many symbols, and while name collisions
are not likely, you may wish to instead import the prelude *module* rather than
its contents:

```rust
use bitvec::prelude as bv;
```

You should almost certainly use type aliases to make names for specific
instantiations of `bitvec` type parameters, and use that rather than attempting
to remain generic over an `<T: BitStore, O: BitOrder>` pair throughout your
project.

## Examples

```rust
use bitvec::prelude::*;

// All data-types have macro
// constructors.
let arr = bitarr![u32, Lsb0; 0; 80];
let bits = bits![u16, Msb0; 0; 40];

// Unsigned integers (scalar, array,
// and slice) can be borrowed.
let data = 0x2021u16;
let bits = data.view_bits::<Msb0>();
let data = [0xA5u8, 0x3C];
let bits = data.view_bits::<Lsb0>();

// Bit-slices can split anywhere.
let (head, rest) = bits.split_at(4);
assert_eq!(head, bits[.. 4]);
assert_eq!(rest, bits[4 ..]);

// And they are writable!
let mut data = [0u8; 2];
let bits = data.view_bits_mut::<Lsb0>();
// l and r each own one byte.
let (l, r) = bits.split_at_mut(8);

// but now a, b, c, and d own a nibble!
let ((a, b), (c, d)) = (
  l.split_at_mut(4),
  r.split_at_mut(4),
);

// and all four of them are writable.
a.set(0, true);
b.set(1, true);
c.set(2, true);
d.set(3, true);

assert!(bits[0]);  // a[0]
assert!(bits[5]);  // b[1]
assert!(bits[10]); // c[2]
assert!(bits[15]); // d[3]

// `BitSlice` is accessed by reference,
// which means it respects NLL styles.
assert_eq!(data, [0x21u8, 0x84]);

// Furthermore, bit-slices can store
// ordinary integers:
let eight = [0u8, 4, 8, 12, 16, 20, 24, 28];
//           a    b  c  d   e   f   g   h
let mut five = [0u8; 5];
for (slot, byte) in five
  .view_bits_mut::<Msb0>()
  .chunks_mut(5)
  .zip(eight.iter().copied())
{
  slot.store_be(byte);
  assert_eq!(slot.load_be::<u8>(), byte);
}

assert_eq!(five, [
  0b00000_001,
//  aaaaa bbb
  0b00_01000_0,
//  bb ccccc d
  0b1100_1000,
//  dddd eeee
  0b0_10100_11,
//  e fffff gg
  0b000_11100,
//  ggg hhhhh
]);
```

The `BitSlice` type is a view that alters the behavior of a borrowed memory
region. It is never held directly, but only by references (created by borrowing
integer memory) or the `BitArray` value type. In addition, the presence of a
dynamic allocator enables the `BitBox` and `BitVec` buffer types, which can be
used for more advanced buffer manipulation:

```rust
#[cfg(feature = "alloc")]
fn main() {

use bitvec::prelude::*;

let mut bv = bitvec![u8, Msb0;];
bv.push(false);
bv.push(true);
bv.extend([false; 4].iter());
bv.extend(&15u8.view_bits::<Lsb0>()[.. 4]);

assert_eq!(bv.as_raw_slice(), &[
  0b01_0000_11, 0b11_000000
//                   ^ dead
]);

}
```

While place expressions like `bits[index] = value;` are not available, `bitvec`
instead provides a proxy structure that can be used as *nearly* an `&mut bit`
reference:

```rust
use bitvec::prelude::*;

let bits = bits![mut 0];
// `bit` is not a reference, so
// it must be bound with `mut`.
let mut bit = bits.get_mut(0).unwrap();
assert!(!*bit);
*bit = true;
assert!(*bit);
// `bit` is not a reference,
// so NLL rules do not apply.
drop(bit);
assert!(bits[0]);
```

The `bitvec` data types implement a complete replacement for their
standard-library counterparts, including all of the inherent methods, traits,
and operator behaviors.

## User Stories

Uses of `bitvec` generally fall into three major genres.

- compact, fast, `usize => bit` collections
- truncated integer storage
- precise control of memory layout

### Bit Collections

At its most basic, `bitvec` provides sequence types analogous to the standard
library’s `bool` collections. The default behavior is optimized for fast memory
access and simple codegen, and can compact `[bool]` or `Vec<bool>` with minimal
overhead.

While `bitvec` does not attempt to take advantage of SIMD or other vectorized
instructions in its default work, its codegen should be a good candidate for
autovectorization in LLVM. If explicit vectorization is important to you, please
[file an issue][issue].

Example uses might be implementing a Sieve of Eratosthenes to store primes, or
other collections that test a yes/no property of a number; or replacing
`Vec<Option<T>>` with `(BitVec, Vec<MaybeUninit<T>>`).

To get started, you can perform basic text replacement on your project.
Translate any existing types as follows:

- `[bool; N]` becomes `BitArray`
- `[bool]` becomes `BitSlice`
- `Vec<bool>` becomes `BitVec`
- `Box<[bool]>` becomes `BitBox`

and then follow any compiler errors that arise.

### Bit-Field Memory Access

A single bit of information has very few uses. `bitvec` also enables you to
store integers wider than a single bit, by selecting a bit-slice and using the
[`BitField`] trait on it. You can store and retrieve both unsigned and signed
integers, as long as the ordering type parameter is [`Lsb0`] or [`Msb0`].

If your bit-field storage buffers are never serialized for exchange between
machines, then you can get away with using the default type parameters and
unadorned load/store methods. While the in-memory layout of stored integers may
be surprising if directly inspected, the overall behavior should be optimal for
your target.

Remember: `bitvec` only provides array place expressions, using integer start
and end points. You can use [`deku`] if you want C-style named structural fields
with bit-field memory storage.

However, if you are de/serializing buffers for transport, then you fall into the
third category.

### Transport Protocols

Many protocols use sub-element fields in order to save space in transport; for
example, TCP headers have single-bit and 4-bit fields in order to pack all the
needed information into a desirable amount of space. In C or Erlang, these TCP
protocol fields could be mapped by record fields in the language. In Rust, they
can be mapped by indexing into a bit-slice.

When using `bitvec` to manage protocol buffers, you will need to select the
exact type parameters that match your memory layout. For instance, TCP uses
`<u8, Msb0>`, while IPv6 on a little-endian machine uses `<u32, Lsb0>`. Once you
have done this, you can replace all of your `(memory & mask) >> shift` or
`memory |= (value & mask) << shift` expressions with `memory[start .. end]`.

As a direct example, the Itanium instruction set IA-64 uses very-long
instruction words containing three 41-bit fields in a `[u8; 16]`. One IA-64
disassembler replaced its manual shift/mask implementation with `bitvec` range
indexing, taking the bit numbers directly from the datasheet, and observed that
their code was both easier to maintain and also had better performance as a
result!

## Feature Flags

`bitvec` has a few Cargo features that govern its API surface. The default
feature set is:

```toml
[dependencies.bitvec]
version = "1"
features = [
  "alloc",
  "atomic",
  # "serde",
  "std",
]
```

Use `default-features = false` to disable all of them, then `features = []` to
restore the ones you need.

- `alloc`: This links against the `alloc` distribution crate, and provides the
  `BitVec` and `BitBox` types. It can be used on `#![no_std]` targets that
  possess a dynamic allocator but not an operating system.

- `atomic`: This controls whether atomic instructions can be used for aliased
  memory. `bitvec` uses the [`radium`] crate to perform automatic detection of
  atomic capability, and targets that do not possess atomic instructions can
  still function with this feature *enabled*. Its only effect is that targets
  which do have atomic instructions may choose to disable it and enforce
  single-threaded behavior that never incurs atomic synchronization.

- `serde`: This enables the de/serialization of `bitvec` buffers through the
  `serde` system. This can be useful if you need to transmit `usize => bool`
  collections.

- `std`: This provides some `std::io::{Read,Write}` implementations, as well as
  `std::error::Error` for the various error types. It is otherwise unnecessary.

## Deeper Reading

The [API Documentation][docsrs] explores `bitvec`’s usage and implementation in
great detail. In particular, you should read the documentation for the
[`order`], [`store`], and [`field`] modules, as well as the [`BitSlice`] and
[`BitArray`] types.

In addition, the [user guide][guide] explores the philosophical and academic
concepts behind `bitvec`’s construction, its goals, and the more intricate parts
of its behavior.

While you should be able to get started with `bitvec` with only dropping it into
your code and using the same habits you have with the standard library, both of
these resources contain all of the information needed to understand what it
does, how it works, and how it can be useful to you.

<!-- Badges -->
[crate]: https://crates.io/crates/bitvec "Crate listing"
[crate_img]: https://img.shields.io/crates/v/bitvec.svg?logo=rust&style=for-the-badge "Crate badge"
[docs]: https://docs.rs/bitvec/latest/bitvec "Crate documentation"
[docs_img]: https://img.shields.io/docsrs/bitvec/latest.svg?style=for-the-badge "Documentation badge"
[downloads_img]: https://img.shields.io/crates/dv/bitvec.svg?logo=rust&style=for-the-badge "Crate downloads"
[license_file]: https://github.com/bitvecto-rs/bitvec/blob/main/LICENSE.txt "Project license"
[license_img]: https://img.shields.io/crates/l/bitvec.svg?style=for-the-badge "License badge"
[loc]: https://github.com/bitvecto-rs/bitvec "Project repository"
[loc_img]: https://img.shields.io/tokei/lines/github/bitvecto-rs/bitvec?category=code&style=for-the-badge "Project size"

<!-- Documentation -->
[`BitArray`]: https://docs.rs/bitvec/latest/bitvec/array/struct.BitArray.html
[`BitField`]: https://docs.rs/bitvec/latest/bitvec/field/trait.BitField.html
[`BitSlice`]: https://docs.rs/bitvec/latest/bitvec/slice/struct.BitSlice.html
[`Lsb0`]: https://docs.rs/bitvec/latest/bitvec/order/struct.Lsb0.html
[`Msb0`]: https://docs.rs/bitvec/latest/bitvec/order/struct.Msb0.html
[`field`]: https://docs.rs/bitvec/latest/bitvec/field/index.html
[`order`]: https://docs.rs/bitvec/latest/bitvec/order/index.html
[`store`]: https://docs.rs/bitvec/latest/bitvec/store/index.html
[layout]: https://bitvecto-rs.github.io/bitvec/memory-representation
[prelude]: https://docs.rs/bitvec/latest/bitvec/prelude

<!-- External References -->
[`deku`]: https://crates.io/crates/deku
[docsrs]: https://docs.rs/bitvec/latest/bitvec
[erl_bit]: https://www.erlang.org/doc/programming_examples/bit_syntax.html
[guide]: https://bitvecto-rs.github.io/bitvec/
[issue]: https://github.com/bitvecto-rs/bitvec/issues/new
[moz]: https://hacks.mozilla.org/2021/04/eliminating-data-races-in-firefox-a-technical-report/ "Mozilla Hacks article describing various concurrency bugs in FireFox"
[`radium`]: https://crates.io/crates/radium
[`std::bitset<N>`]: https://en.cppreference.com/w/cpp/utility/bitset
[`std::vector<bool>`]: https://en.cppreference.com/w/cpp/container/vector_bool
