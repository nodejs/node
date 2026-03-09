# Memory Bus Access Management

`bitvec` allows a program to produce handles over memory that do not *logically*
alias their bits, but *may* alias their hardware locations. This module provides
a unified interface for memory accesses that can be specialized to handle such
aliased and unaliased events.

The [`BitAccess`] trait provides capabilities to access individual or clustered
bits in memory elements through shared, maybe-aliased, references. Its
implementations are responsible for coördinating synchronization and contention
as needed.

The [`BitSafe`] trait guards [`Radium`] types in order to forbid writing through
shared-only references, and require access to an `&mut` exclusive reference for
modification. This permits other components in the crate that do *not* have
`BitSafe` reference guards to safely mutate a referent element that a `BitSafe`d
reference can observe, while preventing that reference from emitting mutations
of its own.

[`BitAccess`]: self::BitAccess
[`BitSafe`]: self::BitSafe
[`Radium`]: radium::Radium
