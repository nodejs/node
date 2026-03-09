# Żmij

[<img alt="github" src="https://img.shields.io/badge/github-dtolnay/zmij-8da0cb?style=for-the-badge&labelColor=555555&logo=github" height="20">](https://github.com/dtolnay/zmij)
[<img alt="crates.io" src="https://img.shields.io/crates/v/zmij.svg?style=for-the-badge&color=fc8d62&logo=rust" height="20">](https://crates.io/crates/zmij)
[<img alt="docs.rs" src="https://img.shields.io/badge/docs.rs-zmij-66c2a5?style=for-the-badge&labelColor=555555&logo=docs.rs" height="20">](https://docs.rs/zmij)
[<img alt="build status" src="https://img.shields.io/github/actions/workflow/status/dtolnay/zmij/ci.yml?branch=master&style=for-the-badge" height="20">](https://github.com/dtolnay/zmij/actions?query=branch%3Amaster)

Pure Rust implementation of Żmij, an algorithm to quickly convert floating point
numbers to decimal strings.

This Rust implementation is a line-by-line port of Victor Zverovich's
implementation in C++, [https://github.com/vitaut/zmij][upstream].

[upstream]: https://github.com/vitaut/zmij/tree/b35b64a2ba63da9ea7c0a72fd2651e89743966c9

## Example

```rust
fn main() {
    let mut buffer = zmij::Buffer::new();
    let printed = buffer.format(1.234);
    assert_eq!(printed, "1.234");
}
```

## Performance

The [dtoa-benchmark] compares this library and other Rust floating point
formatting implementations across a range of precisions. The vertical axis in
this chart shows nanoseconds taken by a single execution of
`zmij::Buffer::new().format_finite(value)` so a lower result indicates a faster
library.

[dtoa-benchmark]: https://github.com/dtolnay/dtoa-benchmark

![performance](https://raw.githubusercontent.com/dtolnay/zmij/master/dtoa-benchmark.png)

<br>

#### License

<a href="LICENSE-MIT">MIT license</a>.
