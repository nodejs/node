# zoneinfo64 [![crates.io](https://img.shields.io/crates/v/zoneinfo64)](https://crates.io/crates/zoneinfo64)

<!-- cargo-rdme start -->

This crate contains utilities for working with ICU4C's zoneinfo64 format

```rust

// Needs to be u32-aligned
let resb = resb::include_bytes_as_u32!("./data/zoneinfo64.res");
// Then we parse the data
let zoneinfo = ZoneInfo64::try_from_u32s(resb)
           .expect("Error processing resource bundle file");

let pacific = zoneinfo.get("America/Los_Angeles").unwrap();
// Calculate the timezone offset for 2024-01-01
let offset = pacific.for_timestamp(1704067200000);
let offset_seven = UtcOffset::from_seconds(-7 * 3600);
assert_eq!(offset.offset, offset_seven);

// Calculate possible offsets at 2025-11-02T01:00:00
// This is during a DST switchover and is ambiguous
let possible = pacific.for_date_time(2025, 11, 2, 1, 0, 0);
let offset_eight = UtcOffset::from_seconds(-8 * 3600);
assert_eq!(possible, PossibleOffset::Ambiguous {
    before: Offset { offset: offset_seven, rule_applies: true },
    after: Offset { offset: offset_eight, rule_applies: false },
    transition: 1762074000,
});
```

<!-- cargo-rdme end -->

## More Information

For more information on development, authorship, contributing etc. please visit [`ICU4X home page`](https://github.com/unicode-org/icu4x).
