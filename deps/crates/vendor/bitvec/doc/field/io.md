# Bit-Field I/O Protocols

This module defines the standard-library `io::{Read, Write}` byte-oriented
protocols on `bitvec` structures that are capable of operating on bytes through
the `BitField` trait.

Note that calling [`BitField`] methods in a loop imposes a non-trivial, and
irremovable, performance penalty on each invocation. The `.read()` and
`.write()` methods implemented in this module are going to suffer this cost, and
you should prefer to operate directly on the underlying buffer if possible.
