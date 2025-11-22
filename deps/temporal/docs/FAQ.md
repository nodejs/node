# FAQ

## Why should I use `temporal_rs`?

`temporal_rs` implements the Temporal API in Rust.

The Temporal API is designed to be a calendar and time zone aware
date/time API.

`temporal_rs` may fit your use case if any of the below are true:

- You are implementing the Temporal API in a JavaScript engine or any
  other language.
- You have internationalization date/time needs for different calendars
  that do not involve the ISO / proleptic Gregorian calendar.
- You really like the JavaScript Temporal built-in and would like to use
  a similar API that you are familiar with.
- Idk, are you a big [ECMA402][ecma402-spec] fan? Than we might be your jam.

## Why not use `jiff` to implement Temporal?

There are a few reasons why `jiff` was not used for Boa's Temporal
implementation.

Primary reasons:

- `temporal_rs` is older than `jiff`; so, it was not an option when the
  work on `temporal_rs` began
- `jiff` is inspired by Temporal, while `temporal_rs` aims to be a 100%
  compliant implementation. `jiff` can break from the specification if
  it would like where `temporal_rs` is bound to the specification
  behavior.

Other concerns:

- Time zones are designed around the concept of BYOP (Bring your own
  provider). This is VERY important for engines to be able to define
  their own time zone providers.
- Without feature flags, `temporal_rs`'s `Now` does not ship with a
  clock as this is left to JavaScript engines.
- Calendar support is first class in `temporal_rs`. The library aims to
  support all ECMA402 defined calendars. `jiff` primarily implements the
  ISO calendar with some support for other calendars through `jiff_icu`.

## Why not use other date/time Rust crates `chrono`, `time` and `hifitime`?

These crates provide fantastic APIs for their intended goal, but most
are designed for use with the proleptic Gregorian calendar.

[ecma402-spec]: https://tc39.es/ecma402/