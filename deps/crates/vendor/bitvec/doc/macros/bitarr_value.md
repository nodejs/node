# Bit-Array Value Constructor

This macro provides a bit-initializer syntax for [`BitArray`] values. It takes a
superset of the [`vec!`] arguments, and is capable of producing bit-arrays in
`const` contexts (for known type parameters).

Like `vec!`, it can accept a sequence of comma-separated bit values, or a
semicolon-separated pair of a bit value and a repetition counter. Bit values may
be any integer or name of a `const` integer, but *should* only be `0` or `1`.

## Argument Syntax

It accepts zero, one, or three prefix arguments:

- `const`: If the first argument to the macro is the keyword `const`, separated
  from remaining arguments by a space, then the macro expands to a
  `const`-expression that can be used in any appropriate context (initializing
  a `static`, a `const`, or passed to a `const fn`). This only works when the
  bit-ordering argument is either implicit, or one of the three tokens that
  `bitvec` can recognize.
- `$order ,`: When this is one of the three literal tokens `LocalBits`, `Lsb0`,
  or `Msb0`, then the macro is able to compute the encoded bit-array contents at
  compile time, including in `const` contexts. When it is anything else, the
  encoding must take place at runtime. The name or path chosen must be in scope
  at the macro invocation site.

  When not provided, this defaults to `Lbs0`.
- `$store ;`: This must be one of `uTYPE`, `Cell<uTYPE>`, `AtomicUTYPE`, or
  `RadiumUTYPE` where `TYPE` is one of `8`, `16`, `32`, `64`, or `size`. The
  macro recognizes this token textually, and does not have access to the type
  system resolver, so it will not accept aliases or qualified paths.

  When not provided, this defaults to `usize`.

The `const` argument can be present or absent independently of the
type-parameter pair. The pair must be either both absent or both present
together.

> Previous versions of `bitvec` supported `$order`-only arguments. This has been
> removed for clarity of use and ease of implementation.

## Examples

```rust
use bitvec::prelude::*;
use core::{cell::Cell, mem};
use radium::types::*;

let a: BitArray = bitarr![0, 1, 0, 0, 1];

let b: BitArray = bitarr![1; 5];
assert_eq!(b.len(), mem::size_of::<usize>() * 8);

let c = bitarr![u16, Lsb0; 0, 1, 0, 0, 1];
let d = bitarr![Cell<u16>, Msb0; 1; 10];
const E: BitArray<[u32; 1], LocalBits> = bitarr![u32, LocalBits; 1; 15];
let f = bitarr![RadiumU32, Msb0; 1; 20];
```

[`BitArray`]: crate::array::BitArray
[`vec!`]: macro@alloc::vec
