# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](http://keepachangelog.com/en/1.0.0/) and this project
adheres to [Semantic Versioning](http://semver.org/spec/v2.0.0.html).

## [1.0.1] - 2022-03-24

### FIXED

-   To avoid an issue where allocated heap memory may be deallocated with a different layout
    alignment than it was officially allocated with when converting between `std::string::String`
    and `SmartString`, even if otherwise correctly aligned, the respective `From` implementations
    now use `std::alloc::Allocator::grow()` to re-align the heap data as necessary. An unfortunate
    consequence of this is that because the `std::alloc::Allocator` API hasn't been stabilised yet,
    unless you're on nightly or some future stable rustc version after `allocator_api` has been
    stabilised, converting between `String` and `SmartString` will always reallocate and copy
    (making it always O(n) rather than O(1) when correctly aligned and O(n) otherwise).
    ([#28](https://github.com/bodil/smartstring/issues/28))

## [1.0.0] - 2022-02-24

### CHANGED

-   `smartstring` now implements its own boxed string type rather than deferring directly to
    `String`, so it no longer makes assumptions it shouldn't be making about the layout of the
    `String` struct.

    This also allows us to organise the boxed struct in a way that will let us rely only on our
    basic assumption that heap memory is word aligned on both big and little endian architectures.
    The most immediate consequence of this is that `smartstring` will now compile on 32-bit big
    endian architectures such as `mips`.

    We are now also explicitly allocating heap memory aligned for `u16` rather than `u8`, ensuring
    the assumption about pointer alignment becomes an invariant.

    In short: `smartstring` no longer relies on undefined behaviour, and should be safe to use
    anywhere.

-   The above means that the boxed `SmartString` is no longer pointer compatible with `String`, so
    if you were relying on that despite the documentation urging you not to, you'll really have to
    stop it now. Converting between `SmartString` and `String` using `From` and `Into` traits is
    still efficient and allocation free.

-   The minimum supported rustc version is now 1.57.0.

-   The `smartstring::validate()` function has been removed, as it's no longer needed.

## [0.2.10] - 2022-02-20

### CHANGED

-   The minimum supported rustc version has been increased to 1.56.0, and the `rust-version` field
    has been added to the crate's `Cargo.toml` to indicate the MSRV. (The `rust-version` field
    itself was introduced in version 1.56, hence the bump.)
-   Dependencies have been bumped, most notably to `arbitrary` version 1.

## [0.2.9] - 2021-07-27

### ADDED

-   You can (and should) now call `smartstring::validate()` from your own code or test suite to
    validate `SmartString`'s memory layout assumptions.

## [0.2.8] - 2021-07-26

### CHANGED

-   The minimum supported rustc version has been increased to 1.46.0.

### ADDED

-   There are now `const fn new_const()` constructors for `SmartString<Compact>` and
    `SmartString<LazyCompact>`, added as a temporary measure because const functions can't yet take
    trait bounds on type arguments, so we can't simply make `SmartString::new()` const.

    Please note that when rustc catches up, the plan is to deprecate `new_const()` in favour of
    `new()`. (#21)

## [0.2.7] - 2021-07-01

### FIXED

-   `no_std` builds have been fixed. (#18)

## [0.2.6] - 2020-12-19

### ADDED

-   `SmartString` now implements `PartialEq<&str>`.

## [0.2.5] - 2020-09-24

### ADDED

-   `From` implementations from `Cow<'_, str>` and `&mut str` were added. (#12)

## [0.2.4] - 2020-09-05

### ADDED

-   `smartstring` is now `no_std` if you disable the `std` feature flag (which is enabled by
    default). (#10)

### FIXED

-   `smartstring` will now refuse to compile on 32-bit big-endian architectures, where assuming that
    the high bit of a pointer is always empty is going to be a very bad idea.

## [0.2.3] - 2020-07-07

### ADDED

-   `SmartString` now implements `Display`. (#6)
-   `SmartString` now implements `FromIterator<char>`.
-   Support for [`serde`](https://serde.rs/) behind the `serde` feature flag. (#2)
-   Support for [`arbitrary`](https://crates.io/crates/arbitrary) behind the `arbitrary` feature
    flag.
-   Support for [`proptest`](https://crates.io/crates/proptest) behind the `proptest` feature flag.

### FIXED

-   `SmartString::push_str` would previously trigger two heap allocations while promoting an inline
    string to a boxed string, one of which was unnecessary. It now only makes the one strictly
    necessary allocation. (#5)
-   Fixed a bug where `SmartString::remove` would panic if you tried to remove the last index in an
    inline string.

## [0.2.2] - 2020-07-05

### FIXED

-   Calling `shrink_to_fit()` on a string with `LazyCompact` layout will now inline it and
    deallocate the heap allocation if the string is short enough to be inlined.

## [0.2.1] - 2020-07-04

### FIXED

-   The type alias `smartstring::alias::String` was incorrectly pointing at the `Compact` variant.
    It is now pointing at `LazyCompact`, as the documentation describes.

## [0.2.0] - 2020-07-04

### REMOVED

-   The `Prefixed` variant has been removed, as it comes with significant code complexity for very
    dubious gains.

### CHANGED

-   The type alias `smartstring::alias::String` now refers to `LazyCompact` instead of `Compact`,
    the idea being that the obvious drop-in replacement for `String` shouldn't have any unexpected
    performance differences, which `Compact` can have because it aggressively re-inlines strings to
    keep them as local as possible. `LazyCompact` instead heap allocates once when the string is in
    excess of the inline capacity and keeps the allocation from then on, so there are no surprises.

### ADDED

-   There's a new layout variant, `LazyCompact`, which works like `Compact` except it never
    re-inlines strings once they have been moved to the heap.
-   As the alias `String` has changed, there is now a new type alias
    `smartstring::alias::CompactString`, referring to strings with `Compact` layout.

### FIXED

-   Fixed a bug where `SmartString::drain()` would remove twice the drained content from the string.

## [0.1.0] - 2020-05-15

Initial release.
