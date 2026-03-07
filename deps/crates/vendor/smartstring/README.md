# smartstring

Compact inlined strings.

## tl;dr

String type that's source compatible with `std::string::String`, uses exactly the same amount of
space, doesn't heap allocate for short strings (up to 23 bytes on 64-bit archs) by storing them in
the space a `String` would have taken up on the stack, making strings go faster overall.

## Overview

This crate provides a wrapper for Rust's standard `String` which uses the space a `String` occupies
on the stack to store inline string data, automatically promoting it to a `String` when it grows
beyond the inline capacity. This has the advantage of avoiding heap allocations for short strings as
well as improving performance thanks to keeping the strings on the stack.

This is all accomplished without the need for an external discriminant, so a `SmartString` is
exactly the same size as a `String` on the stack, regardless of whether it's inlined or not.
Converting a heap allocated `SmartString` into a `String` and vice versa is also a zero cost
operation, as one will reuse the allocated memory of the other.

## Documentation

-   [API docs](https://docs.rs/smartstring)

## Licence

Copyright 2020 Bodil Stokke

This software is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL
was not distributed with this file, You can obtain one at <http://mozilla.org/MPL/2.0/>.

## Code of Conduct

Please note that this project is released with a [Contributor Code of Conduct][coc]. By
participating in this project you agree to abide by its terms.

[coc]: https://github.com/bodil/sized-chunks/blob/master/CODE_OF_CONDUCT.md
