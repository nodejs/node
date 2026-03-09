# Read-Only Semivolatile Handle

This trait describes views of memory that are not permitted to modify the value
they reference, but must tolerate external modification to that value.
Implementors must tolerate shared-mutability behaviors, but are not allowed to
expose shared mutation APIs. They are permitted to modify the referent only
under `&mut` exclusive references.

This behavior enables an important aspect of the `bitvec` memory model when
working with memory elements that multiple [`&mut BitSlice`][0] references
touch: each `BitSlice` needs to be able to give the caller a view of the memory
element, but they also need to prevent modification of bits outside of their
span. This trait enables callers to view raw underlying memory without
improperly modifying memory that *other* `&mut BitSlice`s expect to be stable.

[0]: crate::slice::BitSlice
