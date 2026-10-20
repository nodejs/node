# Milo Parser

This folder contains the milo main parser.

To build milo, simply run:

```bash
makers
```

this will generate `cpp` and `wasm` build in `dist` folder.
These files can be copied in the source code to embed milo.

## How to use the API

See the main [README.md](../README.md) in the parent folder.

## Verify compiled code and macro substitution

Once [cargo-expand](https://github.com/dtolnay/cargo-expand), is installed, run:

```bash
cargo expand > compiled.rs
```

This will generate a file which can be analyzed to see what is the final compiled code after all macros have been evaluated.
