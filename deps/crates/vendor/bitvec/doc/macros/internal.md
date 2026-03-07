# Internal Macro Implementations

The contents of this module are required to be publicly reachable from external
crates, because that is the context in which the public macros expand; however,
the contents of this module are **not** public API and `bitvec` does not support
any use of it other than within the public macros.
