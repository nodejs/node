# Time zone providers

<!-- cargo-rdme start -->

Providers for time zone data

Let's talk about time zone data everyone!

At a high level, the `timezone_provider` crate provides a set of traits along with a few
implementations of those traits. The general intention here is to make providing time zone
data as agnostic and easy as possible.

This crate is fairly "low level" at least as far as date and time needs are concerned. So
we'll cover the basic overview of the trait and some of the general implementations of
those traits, and then we will go on a bit further of a dive for the power users that
are interested in implementing their own provider or is just really curious about what
is going on.

### Available providers

Below is a list of currently available time zone providers.

- `ZoneInfo64TzdbProvider`: a provider using ICU4C's zoneinfo64 resource bundle (enable with `zoneinfo64` features flag)
- `FsTzdbProvider`: a provider that reads and parses tzdata at runtime from the host file system's
TZif files (enable with `tzif` feature flag)
- `CompiledTzdbProvider`: a provider that reads and parses tzdata at runtime from TZif's compiled
into the application (enable with `tzif` feature flag)

Coming soon (hopefully), a zero copy compiled tzdb provider (see `experimental_tzif` for more).

### Time zone provider traits

This crate provides three primary traits for working with time zone data.

- [`TimeZoneProvider`][crate::provider::TimeZoneProvider]
- [`TimeZoneNormalizer`][crate::provider::TimeZoneNormalizer]
- [`TimeZoneResolver`][crate::provider::TimeZoneResolver]

The first trait `TimeZoneProvider` is the primary interface for a time zone provider used by `temporal_rs`.

Meanwhile, the two other traits, `TimeZoneNormalizer` and `TimeZoneResolver`, are secondary
traits that can be used to implement the core `TimeZoneProvider`. Once implemented, this
crate providers a default type for creating a `TimeZoneProvider` by mixing and matching objects that implement the secondary
traits, `NormalizerAndResolver`.

#### Why two secondary traits?

Well that's because `TimeZoneProvider` handles two different concerns: fetching and
formatting normalized and canonicalized time zone identifiers, and resolving time
zone data requests. This functionality typically requires two different sets of data,
each of which may be in a variety of formats.

#### Why not just have the two secondary traits without `TimeZoneProvider`?

Well while the functionality typically requires two sets of data. Those sets are not
necessarily completely unique. The time zone database updates potentially multiple times a
year so having your formatting in 2025a while your data is in 2025b could cause some
desync. So in order to better represent this `TimeZoneProvider` is used on top of them.

**NOTE:** you CAN always just directly use `TimeZoneNormalizer` and
`TimeZoneResolver` together if you want. We just wouldn't recommemnd it.

<!-- cargo-rdme end -->
