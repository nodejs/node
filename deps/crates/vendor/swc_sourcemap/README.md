# sourcemap

This library implements basic processing of JavaScript sourcemaps.

## Installation

The crate is called sourcemap and you can depend on it via cargo:

```toml
[dependencies]
sourcemap = "*"
```

If you want to use the git version:

```toml
[dependencies.sourcemap]
git = "https://github.com/getsentry/rust-sourcemap.git"
```

## Basic Operation

This crate can load JavaScript sourcemaps from JSON files.  It uses
`serde` for parsing of the JSON data.  Due to the nature of sourcemaps
the entirety of the file must be loaded into memory which can be quite
memory intensive.

Usage:

```rust
use swc_sourcemap::SourceMap;
let input: &[_] = b"{
    \"version\":3,
    \"sources\":[\"coolstuff.js\"],
    \"names\":[\"x\",\"alert\"],
    \"mappings\":\"AAAA,GAAIA,GAAI,EACR,IAAIA,GAAK,EAAG,CACVC,MAAM\"
}";
let sm = SourceMap::from_reader(input).unwrap();
let token = sm.lookup_token(0, 0).unwrap(); // line-number and column
println!("token: {}", token);
```

## Features

Functionality of the crate can be turned on and off by feature flags.  This is the
current list of feature flags:

* `ram_bundle`: turns on RAM bundle support


License: BSD-3-Clause
