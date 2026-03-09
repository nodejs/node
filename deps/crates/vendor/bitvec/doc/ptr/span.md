# Encoded Bit-Span Pointer

This module implements the logic used to encode and operate on values of
`*BitSlice`. It is the core operational module of the library.

## Theory

Rust is slowly experimenting with allowing user-provided types to define
metadata structures attached to raw-pointers and references in a structured
manner. However, this is a fairly recent endeavour, much newer than `bitvec`’s
work in the same area, so `bitvec` does not attempt to use it.

The problem with bit-addressable memory is that it takes three more bits to
select a *bit* than it does a *byte*. While AMD64 specifies (and AArch64 likely
follows by fiat) that pointers are 64 bits wide but only contain 48 (or more
recently, 57) bits of information, leaving the remainder available to store
userspace information (as long as it is canonicalized before dereferencing),
x86 and Arm32 have no such luxury space in their pointers.

Since `bitvec` supports 32-bit targets, it instead opts to place the three
bit-selector bits outside the pointer address. The only other space available in
Rust pointers is in the length field of slice pointers. As such, `bitvec`
encodes its span description information into `*BitSlice` and, by extension,
`&/mut BitSlice`. The value underlying these language fundamentals is well-known
(though theoretically opaque), and the standard library provides APIs that it
promises will always be valid to manipulate them. Through careful use of these
APIs, and following type-system rules to prevent undefined behavior, `bitvec` is
able to define its span descriptions within the language fundamentals and appear
fully idiomatic and compliant with existing Rust patterns.

See the [`BitSpan`] type documentation for details on the encoding scheme used.

[`BitSpan`]: self::BitSpan
