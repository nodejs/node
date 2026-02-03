# `libm`

A Rust implementations of the C math library.

## Usage

`libm` provides fallback implementations for Rust's [float math functions] in
`core`, and the [`core_float_math`] feature. If what is available suits your
needs, there is no need to add `libm` as a dependency.

If more functionality is needed, this crate can also be used directly:

```toml
[dependencies]
libm = "0.2.11"
```

[float math functions]: https://doc.rust-lang.org/std/primitive.f32.html
[`core_float_math`]: https://github.com/rust-lang/rust/issues/137578

## Contributing

Please check [CONTRIBUTING.md](../CONTRIBUTING.md)

## Minimum Rust version policy

This crate supports rustc 1.63 and newer.

## License

Usage is under the MIT license, available at
<https://opensource.org/license/mit>.

### Contribution

Contributions are licensed under both the MIT license and the Apache License,
Version 2.0, available at <htps://www.apache.org/licenses/LICENSE-2.0>. Unless
you explicitly state otherwise, any contribution intentionally submitted for
inclusion in the work by you, as defined in the Apache-2.0 license, shall be
dual licensed as mentioned, without any additional terms or conditions.

See [LICENSE.txt](LICENSE.txt) for full details.
