# Constructor Macros

This module provides macros that can be used to create `bitvec` data buffers at
compile time. Each data structure has a corresponding macro:

- `BitSlice` has [`bits!`]
- `BitArray` has [`bitarr!`] (and [`BitArr!`] to produce type expressions)
- `BitBox` has [`bitbox!`]
- `BitVec` has [`bitvec!`]

These macros take a sequence of bit literals, as well as some optional control
prefixes, and expand to code that is generally solvable at compile-time. The
provided bit-orderings `Lsb0` and `Msb0` have implementations that can be used
in `const` contexts, while third-party user-provided orderings cannot be used in
`const` contexts but almost certainly *can* be const-folded by LLVM.

The sequences are encoded into element literals during compilation, and will be
correctly encoded into the target binary. This is even true for targets with
differing byte-endianness than the host compiler.

See each macro for documentation on its invocation syntax. The general pattern
is `[modifier] [T, O;] bits…`. The modifiers influence the nature of the
produced binding, the `[T, O;]` pair provides type parameters when the default
is undesirable, and the `bits…` provides the actual contents of the data
buffer.

[`BitArr!`]: macro@crate::BitArr
[`bitarr!`]: macro@crate::bitarr
[`bitbox!`]: macro@crate::bitbox
[`bits!`]: macro@crate::bits
[`bitvec!`]: macro@crate::bitvec
