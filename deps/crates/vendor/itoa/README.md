itoa
====

[<img alt="github" src="https://img.shields.io/badge/github-dtolnay/itoa-8da0cb?style=for-the-badge&labelColor=555555&logo=github" height="20">](https://github.com/dtolnay/itoa)
[<img alt="crates.io" src="https://img.shields.io/crates/v/itoa.svg?style=for-the-badge&color=fc8d62&logo=rust" height="20">](https://crates.io/crates/itoa)
[<img alt="docs.rs" src="https://img.shields.io/badge/docs.rs-itoa-66c2a5?style=for-the-badge&labelColor=555555&logo=docs.rs" height="20">](https://docs.rs/itoa)
[<img alt="build status" src="https://img.shields.io/github/actions/workflow/status/dtolnay/itoa/ci.yml?branch=master&style=for-the-badge" height="20">](https://github.com/dtolnay/itoa/actions?query=branch%3Amaster)

This crate provides a fast conversion of integer primitives to decimal strings.
The implementation comes straight from [libcore] but avoids the performance
penalty of going through [`core::fmt::Formatter`].

See also [`zmij`] for printing floating point primitives.

[libcore]: https://github.com/rust-lang/rust/blob/1.92.0/library/core/src/fmt/num.rs#L190-L253
[`core::fmt::Formatter`]: https://doc.rust-lang.org/std/fmt/struct.Formatter.html
[`zmij`]: https://github.com/dtolnay/zmij

```toml
[dependencies]
itoa = "1.0"
```

<br>

## Example

```rust
fn main() {
    let mut buffer = itoa::Buffer::new();
    let printed = buffer.format(128u64);
    assert_eq!(printed, "128");
}
```

<br>

## Performance

The [itoa-benchmark] compares this library and other Rust integer formatting
implementations across a range of integer sizes. The vertical axis in this chart
shows nanoseconds taken by a single execution of
`itoa::Buffer::new().format(value)` so a lower result indicates a faster
library.

[itoa-benchmark]: https://github.com/dtolnay/itoa-benchmark

![performance](https://raw.githubusercontent.com/dtolnay/itoa/master/itoa-benchmark.png)

<br>

#### License

<sup>
Licensed under either of <a href="LICENSE-APACHE">Apache License, Version
2.0</a> or <a href="LICENSE-MIT">MIT license</a> at your option.
</sup>

<br>

<sub>
Unless you explicitly state otherwise, any contribution intentionally submitted
for inclusion in this crate by you, as defined in the Apache-2.0 license, shall
be dual licensed as above, without any additional terms or conditions.
</sub>
