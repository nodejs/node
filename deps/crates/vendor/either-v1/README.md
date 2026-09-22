# Either

[![Either crate](https://img.shields.io/crates/v/either.svg)](https://crates.io/crates/either)
[![Either documentation](https://docs.rs/either/badge.svg)](https://docs.rs/either)
![minimum rustc 1.63](https://img.shields.io/badge/rustc-1.63+-red.svg)
[![build status](https://github.com/rayon-rs/either/actions/workflows/ci.yml/badge.svg)](https://github.com/rayon-rs/either/actions)

The enum `Either` with variants `Left` and `Right` is a general purpose sum
type with two cases.

`Either` has methods that are similar to `Option` and `Result`, and it also
implements traits like `Iterator`, `Read`, and `Write`.

The crate includes macros `try_left!()` and `try_right!()` to use for
short-circuiting logic, similar to how the `?` operator is used with `Result`.
Note that `Either` is general purpose and unopinionated about the meaning of
`Left` and `Right`. For describing success or error, use the regular `Result`.

Please read the [API documentation here](https://docs.rs/either/), and see
[RELEASES.md](RELEASES.md) for the version history.

How to use with cargo:

```toml
[dependencies]
either = "1"
```

## License

Dual-licensed to be compatible with the Rust project.

Licensed under the Apache License, Version 2.0
<https://www.apache.org/licenses/LICENSE-2.0> or the MIT license
<https://opensource.org/licenses/MIT>, at your option. This file may not
be copied, modified, or distributed except according to those terms.
