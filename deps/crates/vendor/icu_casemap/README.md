# icu_casemap [![crates.io](https://img.shields.io/crates/v/icu_casemap)](https://crates.io/crates/icu_casemap)

<!-- cargo-rdme start -->

Case mapping for Unicode characters and strings.

This module is published as its own crate ([`icu_casemap`](https://docs.rs/icu_casemap/latest/icu_casemap/))
and as part of the [`icu`](https://docs.rs/icu/latest/icu/) crate. See the latter for more details on the ICU4X project.

## Examples

```rust
use icu::casemap::CaseMapper;
use icu::locale::langid;

let cm = CaseMapper::new();

assert_eq!(
    cm.uppercase_to_string("hello world", &langid!("und")),
    "HELLO WORLD"
);
assert_eq!(
    cm.lowercase_to_string("Γειά σου Κόσμε", &langid!("und")),
    "γειά σου κόσμε"
);
```

[`ICU4X`]: ../icu/index.html

<!-- cargo-rdme end -->

## More Information

For more information on development, authorship, contributing etc. please visit [`ICU4X home page`](https://github.com/unicode-org/icu4x).
