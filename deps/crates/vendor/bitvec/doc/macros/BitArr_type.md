# Bit-Array Type Definition

Because `BitArray<T, O, const BITS: usize>` is not expressible in stable Rust,
this macro serves the purpose of creating a type definition that expands to a
suitable `BitArray`. It creates the correct, rounded-up, `BitArray` to hold a
requested number of bits in a requested set of ordering/storage parameters.

The macro takes a minimum number of bits to store, and an optional set of
bit-order and bit-store type names, and creates a `BitArray` that satisfies the
request. As this macro is only usable in type position, it is named with
`PascalCase` rather than `snake_case`.

## Examples

You must provide a bit-count; you may optionally provide a storage type, or a
bit-ordering *and* a storage type, as subsequent arguments. When elided, the
type parameters are set to the crate defaut type parameters of `Lsb0` and
`usize`.

```rust
use bitvec::prelude::*;
use core::cell::Cell;

let a: BitArr!(for 100) = BitArray::ZERO;
let b: BitArr!(for 100, in u32) = BitArray::<_>::ZERO;
let c: BitArr!(for 100, in Cell<u16>, Msb0) = BitArray::<_, _>::ZERO;
```

The length expression must be `const`. It may be a literal, a named `const`
item, or a `const` expression, as long as it evaluates to a `usize`. The type
arguments have no restrictions, as long as they are in-scope at the invocation
site and are implementors of [`BitOrder`] and [`BitStore`].

[`BitOrder`]: crate::order::BitOrder
[`BitStore`]: crate::store::BitStore
