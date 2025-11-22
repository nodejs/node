
# Zoneinfo_rs

**NOTE:** This crate is experimental and should be considered unstable.

`zoneinfo_rs` provides basic parsing and compilation of IANA's time zone database
zoneinfo files.

```rust
use std::path::Path;
use zoneinfo_rs::{ZoneInfoData, ZoneInfoCompiler};
// Below assumes we are in the parent directory of `tzdata`
let zoneinfo_filepath = Path::new("./tzdata/");
let parsed_data = ZoneInfoData::from_zoneinfo_directory(zoneinfo_filepath)?;
let _compiled_data = ZoneInfoCompiler::new(parsed_data).build();
```

## Extra notes

Currently, parsing only supports parsing of zoneinfo files and none of the preprocessing
typically completed by `awk` scripts in tzdata.

It is recommended to still use the baseline awk script until preprocessing is supported.

## IANA tzdb repository

The latest version of the time zone database can be found [here](https://www.iana.org/time-zones)

