# In-Element Bit Ordering

The `bitvec` memory model is designed to separate the semantic ordering of bits
in an abstract memory space from the electrical ordering of latches in real
memory. This module provides the bridge between the two domains with the
[`BitOrder`] trait and implementations of it.

The `BitOrder` trait bridges semantic indices (marked by the [`BitIdx`] type) to
electrical position counters (morked by the [`BitPos`] type) or selection masks
(marked by the [`BitSel`] and [`BitMask`] types).

Because `BitOrder` is open for client crates to implement, this module also
provides verification functions for the test suite that ensure a given
`BitOrder` implementation is correct for all the register types that it will
govern. See the [`verify_for_type`] or [`verify`] functions for more
information.

[`BitIdx`]: crate::index::BitIdx
[`BitMask`]: crate::index::BitMask
[`BitOrder`]: self::BitOrder
[`BitPos`]: crate::index::BitPos
[`BitSel`]: crate::index::BitSel
[`verify`]: self::verify
[`verify_for_type`]: self::verify_for_type
