# Temporal in Rust

Temporal is a calendar and timezone aware date/time builtin currently
proposed for addition to the ECMAScript specification.

`temporal_rs` is an implementation of Temporal in Rust that aims to be
100% test compliant. While initially developed for [Boa][boa-repo], the
crate has been externalized and is being used in other engines such as [V8](https://v8.dev) and [Kiesel](https://codeberg.org/kiesel-js/kiesel).

For more information on `temporal_rs`'s general position in the Rust
date/time library ecoystem, see our [FAQ](./docs/FAQ.md).


Temporal is an API for working with date and time in a calendar
and time zone aware manner.

temporal_rs is designed with ECMAScript implementations and general
purpose Rust usage in mind, meaning that temporal_rs can be used to implement
the Temporal built-ins in an ECMAScript implementation or generally
used as a date and time library in a Rust project.

temporal_rs is the primary library for the Temporal API implementation in Boa, Kiesel,
and V8. Each of these engines pass the large ECMAScript conformance test suite for
the specification.

## Why use temporal_rs?

As previously mentioned, Temporal is an API for working with date and time in
a calendar and time zone aware manner. This means that calendar and time zone support
are first class in Temporal as well as in temporal_rs.

For instance, converting between calendars is as simple as providing the calendar as
shown below.

```rust
use temporal_rs::{PlainDate, Calendar};
use tinystr::tinystr;
use core::str::FromStr;

// Create a date with an ISO calendar
let iso8601_date = PlainDate::try_new_iso(2025, 3, 3).unwrap();

// Create a new date with the japanese calendar
let japanese_date = iso8601_date.with_calendar(Calendar::JAPANESE);
assert_eq!(japanese_date.era(), Some(tinystr!(16, "reiwa")));
assert_eq!(japanese_date.era_year(), Some(7));
assert_eq!(japanese_date.month(), 3)
```

Beyond the general calendar use case, temporal_rs has robust support for
time zones which can generally by applied to a `PlainDate` via [`ZonedDateTime`].

**Important Note:** The below API is enabled with the
`compiled_data` feature flag.

```rust
use temporal_rs::{ZonedDateTime, TimeZone};
use temporal_rs::options::{Disambiguation, OffsetDisambiguation};

let zdt = ZonedDateTime::from_utf8(
b"2025-03-01T11:16:10Z[America/Chicago][u-ca=iso8601]",
Disambiguation::Compatible,
OffsetDisambiguation::Reject
).unwrap();
assert_eq!(zdt.year(), 2025);
assert_eq!(zdt.month(), 3);
assert_eq!(zdt.day(), 1);
// Using Z and a timezone projects a UTC datetime into the timezone.
assert_eq!(zdt.hour(), 5);
assert_eq!(zdt.minute(), 16);
assert_eq!(zdt.second(), 10);

// You can also update a time zone easily.
let zurich_zone = TimeZone::try_from_str("Europe/Zurich").unwrap();
let zdt_zurich = zdt.with_timezone(zurich_zone).unwrap();
assert_eq!(zdt_zurich.year(), 2025);
assert_eq!(zdt_zurich.month(), 3);
assert_eq!(zdt_zurich.day(), 1);
assert_eq!(zdt_zurich.hour(), 12);
assert_eq!(zdt_zurich.minute(), 16);
assert_eq!(zdt_zurich.second(), 10);

```

## Overview

temporal_rs provides 8 core types for working with date and time. The core types are:

- [PlainDate]
- [PlainTime]
- [PlainDateTime]
- [ZonedDateTime]
- [Instant]
- [Duration]
- [PlainYearMonth]
- [PlainMonthDay]

In addition to these types, there are the [`Calendar`] and [`TimeZone`] type that
support the calendars or time zones. The specific support for calendars and time
zones per type are as follows.

| Temporal type  | Category                             | Calendar support   | Time zone support  |
|----------------|--------------------------------------|--------------------|--------------------|
| PlainDate      | Calendar date                        |        yes         |         no         |
| PlainTime      | Wall-clock time                      |        no          |         no         |
| PlainDateTime  | Calendar date and wall-clock time    |        yes         |         no         |
| ZonedDateTime  | Calendar date and exact time         |        yes         |        yes         |
| Instant        | Exact time                           |        no          |         no         |
| Duration       | None                                 |        no          |         no         |
| PlainYearMonth | Calendar date                        |        yes         |         no         |
| PlainMonthDay  | Calendar date                        |        yes         |         no         |

There is also the [`Now`][now::Now], which provides access to the current host system
time. This can then be used to map to any of the above Temporal types.

**Important Note:** the below example is only available with the `sys` and
`compiled_data` feature flag enabled.

```rust
use core::cmp::Ordering;
use temporal_rs::{Temporal, Calendar, ZonedDateTime};
let current_instant = Temporal::now().instant().unwrap();
let current_zoned_date_time = Temporal::now().zoned_date_time_iso(None).unwrap();

/// Create a `ZonedDateTime` from the requested instant.
let zoned_date_time_from_instant = ZonedDateTime::try_new(
current_instant.as_i128(),
*current_zoned_date_time.time_zone(),
Calendar::ISO,
).unwrap();

// The two `ZonedDateTime` will be equal down to the second.
assert_eq!(current_zoned_date_time.year(), zoned_date_time_from_instant.year());
assert_eq!(current_zoned_date_time.month(), zoned_date_time_from_instant.month());
assert_eq!(current_zoned_date_time.day(), zoned_date_time_from_instant.day());
assert_eq!(current_zoned_date_time.hour(), zoned_date_time_from_instant.hour());
assert_eq!(current_zoned_date_time.minute(), zoned_date_time_from_instant.minute());
assert_eq!(current_zoned_date_time.second(), zoned_date_time_from_instant.second());

// The `Instant` reading that occurred first will be less than the ZonedDateTime
// reading
assert_eq!(
zoned_date_time_from_instant.compare_instant(&current_zoned_date_time),
Ordering::Less
);
```

## General design

While temporal_rs can be used in native Rust programs, the library is -- first and
foremost -- designed for use in ECMAScript implementations. This is not to detract
from temporal_rs's use in a native Rust program, but it is important information to
understand in order to understand the library's architecture and general API design.

Without default feature flags, temporal_rs does not have with access to the host
environment and it does not embed any time zone data. This is important from an
interpreter perspective, because access to the host environment and time zone data
comes from the interpreter's agent, not from a dependency.

Instead, temporal_rs provides the [`HostHooks`][host::HostHooks] and [`TimeZoneProvider`][provider::TimeZoneProvider]
traits that can be implemented and provided as function arguments that temporal_rs will
use to access the host system or time zone data. temporal_rs also provides some baseline
implementations of the traits that can be selected from depending on application needs.

That being said, this does not mean that everyone must implement their own trait
implementations for that functionality to exist, but the APIs are there for power
users who may need a custom host system or time zone data implementation.

A default host system and time zone provider have been implemented and are automatically
active as default features. 

### A quick walkthrough

For instance, the examples thus far have been using the general usage Rust API with
the `sys` and `compiled_data` feature.

For instance, let's manually write our [`Now`][now::Now] implementation instead of using
[`Temporal::now()`] with an empty host system implementation.

```rust
use temporal_rs::{Instant, now::Now, host::EmptyHostSystem};

// The empty host system is a system implementation HostHooks that always
// returns the UNIX_EPOCH and the "+00:00" time zone.
let now = Now::new(EmptyHostSystem);
let time_zone = now.time_zone().unwrap();
assert_eq!(time_zone.identifier().unwrap(), "+00:00");
let now = Now::new(EmptyHostSystem);
assert_eq!(now.instant(), Instant::try_new(0));
```

However, even in our above example, we cheated a bit. We were still relying on the
`compiled_data` feature flag that provided time zone data for us. Let's try again,
but this time without the feature flag.

```rust
use temporal_rs::{Instant, now::Now, host::EmptyHostSystem};
use timezone_provider::tzif::CompiledTzdbProvider;

let provider = CompiledTzdbProvider::default();

// The empty host system is a system implementation HostHooks that always
// returns the UNIX_EPOCH and the "+00:00" time zone.
let now = Now::new(EmptyHostSystem);
let time_zone = now.time_zone_with_provider(&provider).unwrap();
assert_eq!(time_zone.identifier_with_provider(&provider).unwrap(), "+00:00");

let now = Now::new(EmptyHostSystem);
assert_eq!(now.instant(), Instant::try_new(0));
```

Now -- pun only partially intended -- we've successfully written a no-default-features
example with temporal_rs!

### What have we learned going over this all this?

First, any API that has the suffix `_with_provider` is a power user API for supplying
a custom or specific time zone data provider. Furthermore, any API that has a
`_with_provider` suffix will also have a version without the suffix that automagically
provides time zone data for you.

Finally, sourcing time zone data is a very scary (but fun!) business. If you're interested
in learning more, feel free to check out the `timezone_provider` crate!

With any luck, this also highlights the general design of temporal_rs. It provides a
general usage API that aligns with the Temporal specification while also being
flexible enough to provide an power user to take control of their host system access
and time zone data sourcing as needed.

## Formatting

temporal_rs adheres to Temporal grammar, which is a strict version of
[RFC9557's IXDTF][ixdtf]. RFC9557 is an update to RFC3339 that adds
extensions to the format. 

## More information

[`Temporal`][proposal] is the Stage 3 proposal for ECMAScript that
provides new JS objects and functions for working with dates and
times that fully supports time zones and non-gregorian calendars.

This library's primary development source is the Temporal
Proposal [specification][spec].

## Temporal proposal

Relevant links and information regarding Temporal can be found below.

- [Temporal MDN](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Temporal)
- [Temporal Documentation](https://tc39.es/proposal-temporal/docs/)
- [Temporal Proposal Specification](https://tc39.es/proposal-temporal/)
- [Temporal Proposal Repository](https://github.com/tc39/proposal-temporal)

## Core maintainers

- [Kevin Ness](https://github.com/nekevss)
- [Manish Goregaokar](https://github.com/Manishearth)
- [José Julián Espina](https://github.com/jedel1043)
- [Jason Williams](https://github.com/jasonwilliams)
- [Haled Odat](https://github.com/HalidOdat)
- [Boa Developers](https://github.com/orgs/boa-dev/people)

## Contributing

This project is open source and welcomes anyone interested to
participate. Please see [CONTRIBUTING.md](./CONTRIBUTING.md) for more
information.

## Test262 Conformance

<!-- TODO: Potentially update with tests if a runner can be implemented -->

The `temporal_rs`'s current conformance results can be viewed on Boa's
[test262 conformance page](https://boajs.dev/conformance).

## FFI

`temporal_rs` currently has bindings for C++, available via the
`temporal_capi` crate.

## Communication

Feel free to contact us on
[Matrix](https://matrix.to/#/#boa:matrix.org).

## License

This project is licensed under the [Apache](./LICENSE-Apache) or
[MIT](./LICENSE-MIT) licenses, at your option.

[boa-repo]: https://github.com/boa-dev/boa
[ixdtf]: https://www.rfc-editor.org/rfc/rfc9557.txt
[proposal]: https://github.com/tc39/proposal-temporal
[spec]: https://tc39.es/proposal-temporal/

