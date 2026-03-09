`seq!`
======

[<img alt="github" src="https://img.shields.io/badge/github-dtolnay/seq--macro-8da0cb?style=for-the-badge&labelColor=555555&logo=github" height="20">](https://github.com/dtolnay/seq-macro)
[<img alt="crates.io" src="https://img.shields.io/crates/v/seq-macro.svg?style=for-the-badge&color=fc8d62&logo=rust" height="20">](https://crates.io/crates/seq-macro)
[<img alt="docs.rs" src="https://img.shields.io/badge/docs.rs-seq--macro-66c2a5?style=for-the-badge&labelColor=555555&logo=docs.rs" height="20">](https://docs.rs/seq-macro)
[<img alt="build status" src="https://img.shields.io/github/actions/workflow/status/dtolnay/seq-macro/ci.yml?branch=master&style=for-the-badge" height="20">](https://github.com/dtolnay/seq-macro/actions?query=branch%3Amaster)

A `seq!` macro to repeat a fragment of source code and substitute into each
repetition a sequential numeric counter.

```toml
[dependencies]
seq-macro = "0.3"
```

```rust
use seq_macro::seq;

fn main() {
    let tuple = (1000, 100, 10);
    let mut sum = 0;

    // Expands to:
    //
    //     sum += tuple.0;
    //     sum += tuple.1;
    //     sum += tuple.2;
    //
    // This cannot be written using an ordinary for-loop because elements of
    // a tuple can only be accessed by their integer literal index, not by a
    // variable.
    seq!(N in 0..=2 {
        sum += tuple.N;
    });

    assert_eq!(sum, 1110);
}
```

- If the input tokens contain a section surrounded by `#(` ... `)*` then only
  that part is repeated.

- The numeric counter can be pasted onto the end of some prefix to form
  sequential identifiers.

```rust
use seq_macro::seq;

seq!(N in 64..=127 {
    #[derive(Debug)]
    enum Demo {
        // Expands to Variant64, Variant65, ...
        #(
            Variant~N,
        )*
    }
});

fn main() {
    assert_eq!("Variant99", format!("{:?}", Demo::Variant99));
}
```

- Byte and character ranges are supported: `b'a'..=b'z'`, `'a'..='z'`.

- If the range bounds are written in binary, octal, hex, or with zero padding,
  those features are preserved in any generated tokens.

```rust
use seq_macro::seq;

seq!(P in 0x000..=0x00F {
    // expands to structs Pin000, ..., Pin009, Pin00A, ..., Pin00F
    struct Pin~P;
});
```

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
