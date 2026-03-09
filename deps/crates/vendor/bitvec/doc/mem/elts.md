# Bit Storage Calculator

Computes the number of `T` elements required to store some number of bits. `T`
must be an unsigned integer type and cannot have padding bits, but this
restriction cannot be placed on `const fn`s yet.

## Parameters

- `bits`: The number of bits being stored in a `[T]` array.

## Returns

A minimal `N` in `[T; N]` that is not less than `bits`.

As this is a `const` function, when `bits` is also a `const` expression, it can
be used to compute the size of an array type, such as
`[u32; elts::<u32>(BITS)]`.

## Examples

```rust
use bitvec::mem as bv_mem;

assert_eq!(bv_mem::elts::<u8>(10), 2);
assert_eq!(bv_mem::elts::<u8>(16), 2);

let arr: [u16; bv_mem::elts::<u16>(20)] = [0; 2];
```
