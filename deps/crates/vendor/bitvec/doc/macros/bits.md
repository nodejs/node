# Bit-Slice Region Constructor

This macro provides a bit-initializer syntax for [`BitSlice`] reference values.
It takes a superset of the [`vec!`] arguments, and is capable of producing
bit-slices in `const` contexts (for known type parameters).

Like `vec!`, it can accept a sequence of comma-separated bit values, or a
semicolon-separated pair of a bit value and a repetition counter. Bit values may
be any integer or name of a `const` integer, but *should* only be `0` or `1`.

## Argument Syntax

It accepts two modifier prefixes, zero or two type parameters, and the bit
expressions described above.

The modifier prefixes are separated from the remaining arguments by clearspace.

- `static`: If the first argument is the keyword `static`, then this produces a
  `&'static BitSlice` reference bound into a (hidden, unnameable)
  `static BitArray` item. If not, then it produces a stack temporary that the
  Rust compiler automatically extends to have the lifetime of the returned
  reference. Note that non-`static` invocations rely on the compiler’s escape
  analysis, and you should typically not try to move them up the call stack.
- `mut`: If the first argument is the keyword `mut`, then this produces a `&mut`
  writable `BitSlice`.
- `static mut`: These can be combined to create a `&'static mut BitSlice`. It is
  always safe to use this reference, because the `static mut BitArray` it
  creates is concealed and unreachable by any other codepath, and so the
  produced reference is always the sole handle that can reach it.

The next possible arguments are a pair of `BitOrder`/`BitStore` type parameters.

- `$order ,`: When this is one of the three literal tokens `LocalBits`, `Lsb0`,
  or `Msb0`, then the macro is able to compute the encoded bit-array contents at
  compile time, including in `const` contexts. When it is anything else, the
  encoding must take place at runtime. The name or path chosen must be in scope
  at the macro invocation site.

  When not provided, this defaults to `Lsb0`.
- `$store ;`: This must be one of `uTYPE`, `Cell<uTYPE>`, `AtomicUTYPE`, or
  `RadiumUTYPE` where `TYPE` is one of `8`, `16`, `32`, `64`, or `size`. The
  macro recognizes this token textually, and does not have access to the type
  system resolver, so it will not accept aliases or qualified paths.

  When not provided, this defaults to `usize`.

The `static`/`mut` modifiers may be individually present or absent independently
of the type-parameter pair. The pair must be either both absent or both present
together.

> Previous versions of `bitvec` supported $order`-only arguments. This has been
> removed for clarity of use and ease of implementation.

## Safety

Rust considers all `static mut` bindings to be `unsafe` to use. While `bits!`
can prevent *some* of this unsafety by preventing direct access to the created
`static mut` buffer, there are still ways to create multiple names referring to
the same underlying buffer.

```rust,ignore
use bitvec::prelude::*;

fn unsound() -> &'static mut BitSlice<usize, Lsb0> {
  unsafe { bits![static mut 0; 64] }
}

let a = unsound();
let b = unsound();
```

The two names `a` and `b` can be used to produce aliasing `&mut [usize]`
references.

**You must not invoke `bits![static mut …]` in a context where it can be used**
**to create multiple escaping names**. This, and only this, argument combination
of the macro produces a value that requires a call-site `unsafe` block to use.

If you do not use this behavior to create multiple names over the same
underlying buffer, then the macro’s expansion is safe to use, as `bitvec`’s
existing alias-protection behavior suffices.

## Examples

```rust
use bitvec::prelude::*;
use core::cell::Cell;
use radium::types::*;

let a: &BitSlice = bits![0, 1, 0, 0, 1];

let b: &BitSlice = bits![1; 5];
assert_eq!(b.len(), 5);

let c = bits![u16, Lsb0; 0, 1, 0, 0, 1];
let d = bits![static Cell<u16>, Msb0; 1; 10];
let e = unsafe { bits![static mut u32, LocalBits; 0; 15] };
let f = bits![RadiumU32, Msb0; 1; 20];
```

[`BitSlice`]: crate::slice::BitSlice
[`vec!`]: macro@alloc::vec
