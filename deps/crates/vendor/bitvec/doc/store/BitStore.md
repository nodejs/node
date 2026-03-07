# Bit Storage

This trait drives `bitvec`’s ability to view memory as a collection of discrete
bits. It combines awareness of storage element width, memory-bus access
requirements, element contention, and buffer management, into a type-system
graph that the rest of the crate can use to abstract away concerns about memory
representation or access rules.

It is responsible for extending the standard Rust `&`/`&mut` shared/exclusion
rules to apply to individual bits while avoiding violating those rules when
operating on real memory so that Rust and LLVM cannot find fault with the object
code it produces.

## Implementors

This is implemented on three type families:

- all [`BitRegister`] raw integer fundamentals
- all [`Cell`] wrappers of them
- all [atomic] variants of them

The [`BitSlice`] region, and all structures composed atop it, can be built out
of regions of memory that have this trait implementation.

## Associated Types

The associated types attached to each implementation create a closed graph of
type transitions used to manage alias conditions. When a bit-slice region
determines that an aliasing or unaliasing event has occurred, it transitions
along the type graph in order to maintain correct operations in memory. The
methods that cause type transitions can be found in [`BitSlice`] and [`domain`].

[`BitRegister`]: crate::mem::BitRegister
[`BitSlice`]: crate::slice::BitSlice
[`Cell`]: core::cell::Cell
[`domain`]: crate::domain
[atomic]: core::sync::atomic
