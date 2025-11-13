# Library Architecture

TODO: FFI docs

This doc provides an overview of the layout of `temporal_rs`.

We will go over the Temporal Date/Time builtins, general primitives, and
utiltity crates.

## `temporal_rs` design considerations

`temporal_rs` is first and foremost designed to be a fully spec
compliant implementation of ECMAScript's Temporal date/time builtins.

As such, the main design consideration of `temporal_rs` is that it needs
to be able to service language interpreters / engines.

Thus, `temporal_rs` aims to provide an API along with tools to implement
Temporal while minimizing issue with integrating Temporal into engines.

## Date/Time builtins

The primary date & time builtins/components are located in the
`builtins` directory.

These builtins are then reexported from `lib.rs` to be available from
`temporal_rs`'s root module.

### Core vs. Compiled

`temporal_rs`'s builtins are split in two distinct directories `core`
and `compiled`. The core implementation contains the core implementation
of the Temporal builtins; meanwhile, the `compiled` implementation provides
method wrappers around the `core` methods that simplify some "lower" level
date/time APIs that may not be necessary for a general use case.

### Core implementation

The core implementation is always publicly available, but may not be
available to import from the `temporal_rs`'s root.

The core implementation exposes the Provider API that allows the user to
supply a "provider", or any type that implements the `TimeZoneProvider`
trait, for time zone data that the library can use to complete it's
calculations. This is useful from an engine / implementor perspective
because it allows the engine to source time zone data in their preferred
manner without locking them into a library specific implementation that
may or may not have side effects.

A `TimeZoneProvider` API on a core builtin will look like the below.

```rust
impl ZonedDateTime {
    pub fn day_with_provider(&self, provider: &impl TimeZoneProvider) -> TemporalResult<u8> {
        // Code goes here.
    }
}
```

### Compiled implementation

The native implementation is only available via the "compiled" default
feature flag.

For the same reason that the Provider API is useful for language
implementors, it is a deterent from a general use case perspective. Most
people using a datetime library, outside of the self-proclaimed time
zone nerds, probably won't care from where their time zone data is being
sourced.

The native Rust wrapper of the core implementation provides a default
provider implementation to remove the need of the user to think or deal
with `TimeZoneProvider`.

```rust
impl ZonedDateTime {
    pub fn day(&self) -> TemporalResult<u8> {
        // Code goes here.
    }
}
```

This greatly simplifies the API for general use cases.

## Primitives

<!-- Should IsoDate, IsoTime, and IsoDateTime be considered primitives? -->

`temporal_rs` has a primitive number implementation `FiniteF64` along
with a few date and time primitives: `IsoDate`, `IsoTime`,
`IsoDateTime`, and `EpochNanoseconds`.

`FiniteF64` allows an interface to translate between ECMAScript's number
type vs. `temporal_rs`'s strictly typed API.

Meanwhile the Date and Time primitives allow certain invariants to be
enforced on their records.

## Utiltiies

`temporal_rs` provides one implementation of the `TimeZoneProvider`
trait: `FsTzdbProvider`.

`FsTzdbProvider` reads from the file systems' tzdb and when not
available on the system, `FsTzdbProvider` relies on a prepackaged
`tzdb`.

<!-- TODO: add some more about parsers -->
