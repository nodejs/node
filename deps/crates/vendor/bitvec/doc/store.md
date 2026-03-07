# Storage Memory Description

This module defines the `bitvec` memory model used to interface bit-slice
regions to raw memory, and manage type-state changes as demanded by the region
descriptor.

The [`BitStore`] trait is the primary type-level description of `bitvec` views
of the memory space and provides the runtime system that drives the crate memory
model.

## Memory Model

`bitvec` considers all memory within [`BitSlice`] regions as if it were composed
of discrete bits, each divisible and independent from its neighbors, just as the
Rust memory model considers elements `T` in a slice `[T]`. Much as ordinary byte
slices `[u8]` provide an API where each byte is distinct and independent from
its neighbors, but the underlying processor silicon clusters them in words and
cachelines, both the processor silicon *and* the Rust compiler require that bits
in a `BitSlice` be grouped into memory elements, and collectively subjected to
aliasing rules within their batch.

`bitvec` manages this through the `BitStore` trait. It is implemented on three
type families available from the Rust standard libraries:

- [unsigned integers]
- [atomic] unsigned integers
- [`Cell`] wrappers of unsigned integers

`bitvec` receives memory regions typed with one of these families and wraps it
in one of its data structures based on the `BitSlice` region. The target
processor is responsible for handling any contention between `T: BitStore`
memory elements; this is irrelevant to the `bitvec` model. `bitvec` is solely
responsible for proving to the Rust compiler that all memory accesses through
its types are correctly managed according to the `&`/`&mut` shared/exclusion
reference model, and the [`UnsafeCell`] shared-mutation model.

Through `BitStore`, `bitvec` is able to demonstrate that `&mut BitSlice`
references to a region of *bits* have no other `BitSlice` references capable of
viewing those bits. However, multiple `&mut BitSlice` references may view the
same underlying memory element, which is undefined behavior in the Rust compiler
unless additional synchronization and mutual exclusion is provided to prevent
racing writes and unsynchronized reads.

As such, `BitStore` provides a closed type-system graph that the `BitSlice`
region API uses to mark events that can induce aliasing over memory locations.
When a `&mut BitSlice<_, T>` typed with an ordinary unsigned integer uses any of
the APIs that call [`.split_at_mut()`], it transitions its `BitStore` parameter
to `&mut BitSlice<_, T::Alias>`. The [`::Alias`] associated type is always a
type that manages aliasing references to a single memory location: either an
[atomic] unsigned integer `T` or a [`Cell<T>`][`Cell`]. The Rust standard
library guarantees that these types will behave correctly when multiple
references to a single location attempt to perform memory transactions.

The atomic and `Cell` types stay as themselves when [`BitSlice`] introduces
aliasing conditions, as they are already alias-aware.

Foreign implementations of `BitStore` are required to follow the conventions
used here: unsynchronized storage types must create marker newtypes over an
appropriate synchronized type for `::Alias` and uphold the “only `&mut` has
write permission” rule, while synchronized storage types do not need to perform
these transitions, but may never transition to an unsynchronized type either.

The `bitvec` memory description model as implemented in the [`domain`] module is
able to perform the inverse transition: where a `BitSlice` can demonstrate a
static awareness that the `&`/`&mut` exclusion rules are satisfied for a
particular element slice `[T]`, it may apply the [`::Unalias`] marker to undo
any `::Alias`ing, and present a type that has no more aliasing protection than
that with which the memory region was initially declared.

Namely, this means that the [atomic] and [`Cell`] wrappers will *never* be
removed from a region that had them before it was given to `bitvec`, while a
region of ordinary integers may regain the ability to be viewed without
synchrony  guards if `bitvec` can prove safety in the `domain` module.

In order to retain `bitvec`’s promise that an `&mut BitSlice<_, T>` has the sole
right of observation for all bits in its region, the unsigned integers alias to
a crate-internal wrapper over the alias-capable standard-library types. This
wrapper forbids mutation through shared references, so two [`BitSlice`]
references that alias a memory location, but do not overlap in bits, may not be
coërced to interfere with each other.

[atomic]: core::sync::atomic
[unsigned integers]: core::primitive
[`BitSlice`]: crate::slice::BitSlice
[`BitStore`]: self::BitStore
[`Cell`]: core::cell::Cell
[`UnsafeCell`]: core::cell::UnsafeCell
[`domain`]: crate::domain
[`::Alias`]: self::BitStore::Alias
[`::Unalias`]: self::BitStore::Unalias
