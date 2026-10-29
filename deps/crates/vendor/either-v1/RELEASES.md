- 1.18.0 - 2026-08-20
  - Add trait impls akin to `Itertools::partition_map`, by @cuviper (#144)
    - `Extend<Either<L, R>> for (A, B)`
    - `FromIterator<Either<L, R>> for (A, B)`

- 1.17.0 - 2026-07-24
  - Add implementations for all `std::fmt` traits, by @msrd0 (#141)

- 1.16.0 - 2026-05-20
  - Add many new methods dealing with each side, by @A4-Tacks:
    - `inspect_left` and `inspect_right` (#124)
    - `left_and` and `right_and` (#125)
    - `is_left_and` and `is_right_and` (#126)
    - `map_left_or` and `map_right_or` (#127)
  - Add a version of `for_both!` with a single `ident`, by @A4-Tacks (#128)
  - Add a `map_both!` macro, by @JohnScience (#109) and @ronnodas (#137)

- 1.15.0 - 2025-03-05
  - Fix `serde` support when building without `std`, by @klkvr (#119)
  - Use a more common `std` feature for default enablement, deprecating the
    `use_std` feature as a mere alias of the new name.

- 1.14.0 - 2025-02-23
  - **MSRV**: `either` now requires Rust 1.63 or later.
  - Implement `fmt::Write` for `Either`, by @yotamofek (#113)
  - Replace `Into<Result> for Either` with `From<Either> for Result`, by
    @cuviper (#118)

- 1.13.0 - 2024-06-25
  - Add new methods `.cloned()` and `.copied()`, by @ColonelThirtyTwo (#107)

- 1.12.0 - 2024-05-16
  - **MSRV**: `either` now requires Rust 1.37 or later.
  - Specialize `nth_back` for `Either` and `IterEither`, by @cuviper (#106)

- 1.11.0 - 2024-04-13
  - Add new trait `IntoEither` that is useful to convert to `Either` in method
    chains, by @SFM61319 (#101)

- 1.10.0 - 2024-02-10
  - Add new methods `.factor_iter()`, `.factor_iter_mut()`, and
    `.factor_into_iter()` that return `Either` items, plus `.iter()` and
    `.iter_mut()` to convert to direct reference iterators; by @aj-bagwell and
    @cuviper (#91)

- 1.9.0 - 2023-07-22
  - Add new methods `.map_either()` and `.map_either_with()`, by @nasadorian
    (#82)

- 1.8.1 - 2023-01-26
  - Clarified that the multiple licenses are combined with OR.

- 1.8.0 - 2022-08-17
  - **MSRV**: `either` now requires Rust 1.36 or later.
  - Add new methods `.as_pin_ref()` and `.as_pin_mut()` to project a pinned
    `Either` as inner `Pin` variants, by @cuviper (#77)
  - Implement the `Future` trait, by @cuviper (#77)
  - Specialize more methods of the `io` traits, by @Kixunil and @cuviper (#75)

- 1.7.0 - 2022-06-29
  - **MSRV**: `either` now requires Rust 1.31 or later.
  - Export the macro `for_both!`, by @thomaseizinger (#58)
  - Implement the `io::Seek` trait, by @Kerollmops (#60)
  - Add new method `.either_into()` for `Into` conversion, by
    @TonalidadeHidrica (#63)
  - Add new methods `.factor_ok()`, `.factor_err()`, and `.factor_none()`, by
    @zachs18 (#67)
  - Specialize `source` in the `Error` implementation, by @thomaseizinger (#69)
  - Specialize more iterator methods and implement the `FusedIterator` trait,
    by @Ten0 (#66) and @cuviper (#71)
  - Specialize `Clone::clone_from`, by @cuviper (#72)

- 1.6.1 - 2020-09-16
  - Add new methods `.expect_left()`, `.unwrap_left()`, and equivalents on the
    right, by @spenserblack (#51)

- 1.6.0 - 2020-08-10
  - Add new modules `serde_untagged` and `serde_untagged_optional` to customize
    how `Either` fields are serialized in other types, by @MikailBag (#49)

- 1.5.3 - 2019-09-13
  - Add new method `.map()` for `Either<T, T>` by @nvzqz (#40).

- 1.5.2 - 2019-04-02
  - Add new methods `.left_or()`, `.left_or_default()`, `.left_or_else()`, and
    equivalents on the right, by @DCjanus (#36)

- 1.5.1 - 2019-02-21
  - Add `AsRef` and `AsMut` implementations for common unsized types: `str`,
    `[T]`, `CStr`, `OsStr`, and `Path`, by @mexus (#29)

- 1.5.0 - 2018-03-25
  - Add new methods `.factor_first()`, `.factor_second()` and `.into_inner()`
    by @mathstuf (#19)

- 1.4.0 - 2017-11-14
  - Add inherent method `.into_iter()` by @cuviper (#12)

- 1.3.0 - 2017-10-15
  - Add opt-in `serde` support by @hcpl

- 1.2.0 - 2017-09-28
  - Add method `.either_with()` by @Twey (#13)

- 1.1.0 - 2017-03-25
  - Add methods `left_and_then`, `right_and_then` by @rampantmonkey
  - Include license files in the repository and released crate

- 1.0.3 - 2017-02-10
  - Add crate categories

- 1.0.2 - 2016-11-13
  - Forward more `Iterator` methods
  - Implement `Extend` for `Either<L, R>` if `L, R` do.

- 1.0.1 - 2016-10-21
  - Fix `Iterator` impl for `Either` to forward `.fold()`.

- 1.0.0 - 2016-09-23
  - Add default crate feature `use_std` so that you can opt out of
    linking to std.

- 0.1.7 - 2016-09-03
  - Add methods `.map_left()`, `.map_right()` and `.either()`.
  - Add more documentation

- 0.1.3 - 2015-09-25
  - Implement `Display`, `Error`

- 0.1.2 - 2015-09-20
  - Add macros `try_left!` and `try_right!`.

- 0.1.1 - 2015-09-16
  - Implement `Deref`, `DerefMut`

- 0.1.0 - 2015-09-14
  - Initial release
  - Support `Iterator`, `Read`, `Write`
