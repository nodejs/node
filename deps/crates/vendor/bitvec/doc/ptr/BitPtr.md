# Single-Bit Pointer

This structure defines a pointer to exactly one bit in a memory element. It is a
structure, rather than an encoding of a `*Bit` raw pointer, because it contains
more information than can be packed into such a pointer. Furthermore, it can
uphold the same requirements and guarantees that the rest of the crate demands,
whereäs a raw pointer cannot.

## Original

[`*bool`](https://doc.rust-lang.org/std/primitive.pointer.html) and
[`NonNull<bool>`](core::ptr::NonNull)

## API Differences

Since raw pointers are not sufficient in space or guarantees, and are limited by
not being marked `#[fundamental]`, this is an ordinary `struct`. Because it
cannot use the `*const`/`*mut` distinction that raw pointers and references can,
this encodes mutability in a type parameter instead.

In order to be consistent with the rest of the crate, particularly the
`*BitSlice` encoding, this enforces that all `T` element addresses are
well-aligned to `T` and non-null. While this type is used in the API as an
analogue of raw pointers, it is restricted in value to only contain the values
of valid *references* to memory, not arbitrary pointers.

## ABI Differences

This is aligned to `1`, rather than the processor word, in order to enable some
crate-internal space optimizations.

## Type Parameters

- `M`: Marks whether the pointer has mutability permissions to the referent
  memory. Only `Mut` pointers can be used to create `&mut` references.
- `T`: A memory type used to select both the register width and the bus behavior
  when performing memory accesses.
- `O`: The ordering of bits within a memory element.

## Usage

This structure is used as the `bitvec` equivalent to `*bool`. It is used in all
raw-pointer APIs and provides behavior to emulate raw pointers. It cannot be
directly dereferenced, as it is not a pointer; it can only be transformed back
into higher referential types, or used in functions that accept it.

These pointers can never be null or misaligned.

## Safety

Rust and LLVM **do not** have a concept of bit-level initialization yet.
Furthermore, the underlying foundational code that this type uses to manipulate
individual bits in memory relies on construction of **shared references** to
memory, which means that unlike standard pointers, the `T` element to which
`BitPtr` values point must always be **already initialized** in your program
context.

`bitvec` is not able to detect or enforce this requirement, and is currently not
able to avoid it. See [`BitAccess`] for more information.

[`BitAccess`]: crate::access::BitAccess
