# icu_list [![crates.io](https://img.shields.io/crates/v/icu_list)](https://crates.io/crates/icu_list)

<!-- cargo-rdme start -->

Formatting lists in a locale-sensitive way.

This module is published as its own crate ([`icu_list`](https://docs.rs/icu_list/latest/icu_list/))
and as part of the [`icu`](https://docs.rs/icu/latest/icu/) crate. See the latter for more details on the ICU4X project.

## Examples

### Formatting *and* lists in Spanish

```rust
let list_formatter = ListFormatter::try_new_and(
    locale!("es").into(),
    ListFormatterOptions::default().with_length(ListLength::Wide),
)
.expect("locale should be present");

assert_writeable_eq!(
    list_formatter.format(["España", "Suiza"].iter()),
    "España y Suiza",
);

// The Spanish 'y' sometimes becomes an 'e':
assert_writeable_eq!(
    list_formatter.format(["España", "Suiza", "Italia"].iter()),
    "España, Suiza e Italia",
);
```

### Formatting *or* lists in Thai

```rust
let list_formatter = ListFormatter::try_new_or(
    locale!("th").into(),
    ListFormatterOptions::default().with_length(ListLength::Short),
)
.expect("locale should be present");

// We can use any Writeables as inputs
assert_writeable_eq!(list_formatter.format(1..=3), "1, 2 หรือ 3",);
```

### Formatting unit lists in English

```rust
let list_formatter = ListFormatter::try_new_unit(
    locale!("en").into(),
    ListFormatterOptions::default().with_length(ListLength::Wide),
)
.expect("locale should be present");

assert_writeable_eq!(
    list_formatter.format(["1ft", "2in"].iter()),
    "1ft, 2in",
);
```
Note: this last example is not fully internationalized. See [icu4x/2192](https://github.com/unicode-org/icu4x/issues/2192)
for full unit handling.

<!-- cargo-rdme end -->

## More Information

For more information on development, authorship, contributing etc. please visit [`ICU4X home page`](https://github.com/unicode-org/icu4x).
