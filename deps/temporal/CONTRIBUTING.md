# Contributing to Temporal in Rust

We welcome contributions, feel free to checkout our open issues. If you
find an issue you're interested in, please feel free to ask to be assigned.

If you're interested in helping out but don't see an issue that's for
you, please feel free to contact us on `Boa`'s Matrix server.

## Contributor Information

The Temporal proposal is a new date/time API that is being developed and proposed
for the ECMAScript specification. This library aims to be a Rust
implementation of that specification.

Due to the nature of the material and this library, we would advise anyone
interested in contributing to familiarize themselves with the Temporal
specification.

Also, always feel free to reach out for any advice or feedback as needed.

## Testing and debugging

For more information on testing and debugging `temporal_rs`. Please see
the [testing overview](./docs/testing.md).

## Diplomat and `temporal_capi`

If changes are made to `temporal_capi` that affect the public API, the
FFI bindings will need to be regenerated / updated.

To update the bindings, run:

```bash
cargo run -p diplomat-gen
```

## Baked data

To regenerate baked data, run:

```bash
cargo run -p bakeddata
```

## Dependency check

To check the dependencies, run:

```bash
cargo run -p depcheck
```
