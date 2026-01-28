## What's Changed in v0.1.0
* Update Diplomat to 0.13.0 by @Manishearth in [#588](https://github.com/boa-dev/temporal/pull/588)
* Add TryFrom for PartialDuration to Duration by @nekevss in [#585](https://github.com/boa-dev/temporal/pull/585)
* Add missing from_epoch_nanoseconds() FFI by @linusg in [#584](https://github.com/boa-dev/temporal/pull/584)
* Add from_nanoseconds to FFI PlainDateTime by @nekevss in [#583](https://github.com/boa-dev/temporal/pull/583)
* Update diplomat to using cpp lib name by @Manishearth in [#581](https://github.com/boa-dev/temporal/pull/581)
* Fix TimeZone::get_possible_epoch_ns at date-time limits by @ptomato in [#580](https://github.com/boa-dev/temporal/pull/580)
* Rename timezone.rs to time_zone.rs by @nekevss in [#574](https://github.com/boa-dev/temporal/pull/574)
* General updates to temporal_rs's exports and docs by @nekevss in [#575](https://github.com/boa-dev/temporal/pull/575)
* Review Instant API + Duration by @nekevss in [#573](https://github.com/boa-dev/temporal/pull/573)
* Make TimeZone no longer allocate over FFI by @Manishearth in [#572](https://github.com/boa-dev/temporal/pull/572)
* Update the library introduction and README for timezone_provider by @nekevss in [#570](https://github.com/boa-dev/temporal/pull/570)
* Add a minimal README for zoneinfo_rs by @nekevss in [#569](https://github.com/boa-dev/temporal/pull/569)
* Remove passing lint allows by @Manishearth in [#571](https://github.com/boa-dev/temporal/pull/571)
* Updates to PlainYearMonth and PlainMonthDay based on review by @nekevss in [#567](https://github.com/boa-dev/temporal/pull/567)
* Update library introduction documentation and the project README.md by @nekevss in [#564](https://github.com/boa-dev/temporal/pull/564)
* Updates to PlainTime API based on review by @nekevss in [#565](https://github.com/boa-dev/temporal/pull/565)
* Make ISO getters crate private by @nekevss in [#568](https://github.com/boa-dev/temporal/pull/568)
* Handle und month codes by @Manishearth in [#563](https://github.com/boa-dev/temporal/pull/563)
* Update `ZonedDateTime` constructors by @nekevss in [#562](https://github.com/boa-dev/temporal/pull/562)
* Review and update PlainDate and PlainDateTime API by @nekevss in [#561](https://github.com/boa-dev/temporal/pull/561)
* Refactor Now to be trait based for lazy host getters by @nekevss in [#560](https://github.com/boa-dev/temporal/pull/560)
* Fix comment and organization of TimeZone::zero by @Manishearth in [#559](https://github.com/boa-dev/temporal/pull/559)
* Update ZonedDateTime module and API by @nekevss in [#557](https://github.com/boa-dev/temporal/pull/557)
* Remove yoke dep from temporal_capi by @Manishearth in [#556](https://github.com/boa-dev/temporal/pull/556)

## New Contributors
* @ptomato made their first contribution in [#580](https://github.com/boa-dev/temporal/pull/580)

**Full Changelog**: https://github.com/boa-dev/temporal/compare/v0.0.16...v0.1.0

# Changelog

All notable changes to this project will be documented in this file.

## What's Changed in v0.0.16
* Bump versions to 0.0.16
* Remove extraneous icu_time dependency
* Add TimeZone::zero() to capi by @Manishearth in [#554](https://github.com/boa-dev/temporal/pull/554)

**Full Changelog**: https://github.com/boa-dev/temporal/compare/v0.0.15...v0.0.16

## What's Changed in v0.0.15
* Bump versions
* Update zoneinfo64 by @Manishearth in [#552](https://github.com/boa-dev/temporal/pull/552)
* Remove Default impl from TimeZone; use utc() everywhere by @Manishearth in [#551](https://github.com/boa-dev/temporal/pull/551)
* Add missing header files by @Manishearth in [#550](https://github.com/boa-dev/temporal/pull/550)
* Add provider APIs to capi by @Manishearth in [#544](https://github.com/boa-dev/temporal/pull/544)
* Add new unit validation code by @Manishearth in [#542](https://github.com/boa-dev/temporal/pull/542)
* Add calendar consts for calendar construction by @nekevss in [#541](https://github.com/boa-dev/temporal/pull/541)
* Remove some unreachables by @Manishearth in [#543](https://github.com/boa-dev/temporal/pull/543)
* Move timezone tests to testing against multiple providers by @Manishearth in [#539](https://github.com/boa-dev/temporal/pull/539)
* Move TimeZone over to being Copy with TimeZoneId by @Manishearth in [#538](https://github.com/boa-dev/temporal/pull/538)
* Split TimeZoneProvider trait by @Manishearth in [#537](https://github.com/boa-dev/temporal/pull/537)
* Update compiled tzif data provider data by @nekevss in [#535](https://github.com/boa-dev/temporal/pull/535)
* Add zoneinfo64 support to temporal_provider by @Manishearth in [#533](https://github.com/boa-dev/temporal/pull/533)
* Update zoneinfo compilation in zoneinfo crate by @nekevss in [#532](https://github.com/boa-dev/temporal/pull/532)
* Complete some cleanup options module by @nekevss in [#531](https://github.com/boa-dev/temporal/pull/531)
* Make errors Copy by @Manishearth in [#528](https://github.com/boa-dev/temporal/pull/528)
* More saturating arithmetic by @Manishearth in [#527](https://github.com/boa-dev/temporal/pull/527)
* Move TimeZoneProvider to timezone_provider crate by @Manishearth in [#526](https://github.com/boa-dev/temporal/pull/526)
* Clean up TimeZoneProvider crate by @Manishearth in [#525](https://github.com/boa-dev/temporal/pull/525)

**Full Changelog**: https://github.com/boa-dev/temporal/compare/v0.0.14...v0.0.15

## What's Changed in v0.0.14
* Release 0.0.14
* Fix validity checks by @Manishearth in [#523](https://github.com/boa-dev/temporal/pull/523)

**Full Changelog**: https://github.com/boa-dev/temporal/compare/v0.0.13...v0.0.14

## What's Changed in v0.0.13
* Bump versions to 0.13
* Add validity checks on parse by @Manishearth in [#517](https://github.com/boa-dev/temporal/pull/517)
* Use correct cached offset when rounding ZDTs by @Manishearth in [#520](https://github.com/boa-dev/temporal/pull/520)
* Clamp unit indices before checking table by @Manishearth in [#516](https://github.com/boa-dev/temporal/pull/516)
* Check that year values are in safe arithmetical range by @Manishearth in [#513](https://github.com/boa-dev/temporal/pull/513)
* Use correct sign value in nudge code by @Manishearth in [#514](https://github.com/boa-dev/temporal/pull/514)
* Support fractional hour values in hoursInDay by @Manishearth in [#515](https://github.com/boa-dev/temporal/pull/515)
* Cache offsets on ZDT by @Manishearth in [#510](https://github.com/boa-dev/temporal/pull/510)
* Update duration to unsigned fields + specification updates by @nekevss in [#507](https://github.com/boa-dev/temporal/pull/507)
* Reduce (and lint-disallow) panics in main code by @Manishearth in [#506](https://github.com/boa-dev/temporal/pull/506)
* Further normalize MWDs, add Vancouver and Santiago tests by @Manishearth in [#504](https://github.com/boa-dev/temporal/pull/504)
* Duration rounding: is_date_unit(), not is_calendar_unit() by @Manishearth in [#503](https://github.com/boa-dev/temporal/pull/503)
* Use beyondDaySpan in NudgeToZonedDateTime by @Manishearth in [#501](https://github.com/boa-dev/temporal/pull/501)
* Remove use of unsafe from the main crate by @Manishearth in [#502](https://github.com/boa-dev/temporal/pull/502)
* Simplify calendar API over FFI by @Manishearth in [#498](https://github.com/boa-dev/temporal/pull/498)
* Handle week=5 MWD cases in posix TZ strings by @Manishearth in [#499](https://github.com/boa-dev/temporal/pull/499)
* Exclude tzif files from publish by @Manishearth in [#495](https://github.com/boa-dev/temporal/pull/495)

**Full Changelog**: https://github.com/boa-dev/temporal/compare/v0.0.12...v0.0.13

## What's Changed in v0.0.12
* Bump versions
* cleanup: Remove parsing functionality from PartialZDT by @Manishearth in [#493](https://github.com/boa-dev/temporal/pull/493)
* Various MonthDay fixes by @Manishearth in [#492](https://github.com/boa-dev/temporal/pull/492)
* Add `CalendarFields` concept and integate it with API and Partial structs by @nekevss in [#487](https://github.com/boa-dev/temporal/pull/487)
* Add support for non-ISO yearmonths and monthdays by @Manishearth in [#490](https://github.com/boa-dev/temporal/pull/490)
* Add separate parsed variants of all types by @Manishearth in [#486](https://github.com/boa-dev/temporal/pull/486)
* Update icu_calendar, fix self-consistency test by @Manishearth in [#488](https://github.com/boa-dev/temporal/pull/488)
* Add builder pattern `with_xxx` methods for PartialDuration by @nekevss in [#485](https://github.com/boa-dev/temporal/pull/485)
* Add CalendarFieldKeysToIgnore by @Manishearth in [#484](https://github.com/boa-dev/temporal/pull/484)
* Reuse ZDT parsing logic from PartialZDT by @Manishearth in [#483](https://github.com/boa-dev/temporal/pull/483)
* Fix hours in day calculation around gap transitions by @Manishearth in [#482](https://github.com/boa-dev/temporal/pull/482)
* Generate the baked zone info data for ZoneInfoProvider by @nekevss in [#264](https://github.com/boa-dev/temporal/pull/264)
* Precompute before/after offsets and use in disambiguate_possible_epoch_nanos by @Manishearth in [#479](https://github.com/boa-dev/temporal/pull/479)
* Add tzif-inspect tool for inspecting tzif data by @Manishearth in [#480](https://github.com/boa-dev/temporal/pull/480)
* Fix behavior of ns-to-seconds casting for negative values by @Manishearth in [#475](https://github.com/boa-dev/temporal/pull/475)
* Fix panics from calling abs on i64::MIN by @nekevss in [#474](https://github.com/boa-dev/temporal/pull/474)
* Implement get_named_tzdb_transition by @Manishearth in [#472](https://github.com/boa-dev/temporal/pull/472)
* Use ISO day of week unconditionally by @Manishearth in [#471](https://github.com/boa-dev/temporal/pull/471)
* Fix overflow when validating Duration by @nekevss in [#470](https://github.com/boa-dev/temporal/pull/470)
* Rewrite v2_estimate_tz_pair to compare local timestamps by @Manishearth in [#468](https://github.com/boa-dev/temporal/pull/468)
* Normalize, don't canonicalize time zones by @Manishearth in [#466](https://github.com/boa-dev/temporal/pull/466)
* Normalize UTC to UTC by @Manishearth in [#463](https://github.com/boa-dev/temporal/pull/463)
* Ensure that the correct year-month is range-checked during diff operations by @Manishearth in [#461](https://github.com/boa-dev/temporal/pull/461)
* Correctly handle matchBehavior for sub-minute offset strings by @Manishearth in [#462](https://github.com/boa-dev/temporal/pull/462)
* Correctly normalize timezones by @Manishearth in [#460](https://github.com/boa-dev/temporal/pull/460)
* Fix posix logic that was causing panics by @nekevss in [#459](https://github.com/boa-dev/temporal/pull/459)
* Fix float precision check by @Manishearth in [#457](https://github.com/boa-dev/temporal/pull/457)
* CalendarDateAdd takes DateDurations, not full Durations by @Manishearth in [#453](https://github.com/boa-dev/temporal/pull/453)
* Check validity where requested by the spec by @Manishearth in [#456](https://github.com/boa-dev/temporal/pull/456)
* Add .clone() to FFI, YearMonth::reference_day by @Manishearth in [#454](https://github.com/boa-dev/temporal/pull/454)
* Move nanoseconds-validity checking to being explicit by @Manishearth in [#452](https://github.com/boa-dev/temporal/pull/452)
* Add debug impl for error message by @Manishearth in [#451](https://github.com/boa-dev/temporal/pull/451)
* Rename ffi epoch_ns_for to epoch_ms_for by @Manishearth in [#443](https://github.com/boa-dev/temporal/pull/443)
* Fix panic in tzdb logic by @nekevss in [#441](https://github.com/boa-dev/temporal/pull/441)
* Support RoundNumberToIncrementAsIfPositive by @Manishearth in [#440](https://github.com/boa-dev/temporal/pull/440)
* Support sub-minute offsets in UTCOffset by @Manishearth in [#437](https://github.com/boa-dev/temporal/pull/437)
* Fix bug in `to_zoned_date_time_with_provider` by @nekevss in [#436](https://github.com/boa-dev/temporal/pull/436)
* Ensure dates are in limits after performing arithmetic by @Manishearth in [#435](https://github.com/boa-dev/temporal/pull/435)
* YearMonth::compare should compare ISO days by @Manishearth in [#433](https://github.com/boa-dev/temporal/pull/433)
* toPlainDate should use CONSTRAIN by @Manishearth in [#430](https://github.com/boa-dev/temporal/pull/430)

**Full Changelog**: https://github.com/boa-dev/temporal/compare/v0.0.11...v0.0.12

## What's Changed in v0.0.11
* Add PartialZonedDateTime::try_from_str by @Manishearth in [#420](https://github.com/boa-dev/temporal/pull/420)
* Add from_partial for YearMonth/MonthDay FFI by @Manishearth in [#351](https://github.com/boa-dev/temporal/pull/351)
* Throw error if given more than 9 duration digits by @Manishearth in [#425](https://github.com/boa-dev/temporal/pull/425)
* Fix up eras, correctly expose arithmetic year by @Manishearth in [#424](https://github.com/boa-dev/temporal/pull/424)
* Add time zone normalization by @Manishearth in [#415](https://github.com/boa-dev/temporal/pull/415)
* Allow adding date durations to PlainTime by @Manishearth in [#421](https://github.com/boa-dev/temporal/pull/421)
* Parse both zoned and unzoned date times in relativeTo by @Manishearth in [#422](https://github.com/boa-dev/temporal/pull/422)
* Add get_epoch_ns_for on YearMonth/MonthDay by @Manishearth in [#414](https://github.com/boa-dev/temporal/pull/414)
* Don't set None time in ZDT::from_partial by @Manishearth in [#416](https://github.com/boa-dev/temporal/pull/416)
* [capi] Add from_utf8/from_utf16 for OwnedRelativeTo by @Manishearth in [#375](https://github.com/boa-dev/temporal/pull/375)
* Forbid MonthDay/YearMonth formats for non-iso by @Manishearth in [#413](https://github.com/boa-dev/temporal/pull/413)
* Use track_caller in assertion errors by @Manishearth in [#417](https://github.com/boa-dev/temporal/pull/417)
* Properly respect overflow options when performing with_fallback_date/time by @Manishearth in [#407](https://github.com/boa-dev/temporal/pull/407)
* Add TimeZone::utc() to FFI by @Manishearth in [#358](https://github.com/boa-dev/temporal/pull/358)
* Add POSIX time zone string support to zoneinfo by @nekevss in [#265](https://github.com/boa-dev/temporal/pull/265)
* Fix duration validity check by @Manishearth in [#411](https://github.com/boa-dev/temporal/pull/411)
* Produce correct error types for invalid month/era codes by @Manishearth in [#405](https://github.com/boa-dev/temporal/pull/405)
* Add support for offsetBehavior == WALL by @Manishearth in [#404](https://github.com/boa-dev/temporal/pull/404)
* Update README.md - mention other usages of lib by @jasonwilliams in [#401](https://github.com/boa-dev/temporal/pull/401)
* Make TimeZone::identifier() infallible by @linusg in [#399](https://github.com/boa-dev/temporal/pull/399)
* Fix the bakedata provider path by @nekevss in [#398](https://github.com/boa-dev/temporal/pull/398)
* Include time duration in InternalDurationSign by @Manishearth in [#397](https://github.com/boa-dev/temporal/pull/397)
* Use rounded instant in ZDT::toString by @Manishearth in [#396](https://github.com/boa-dev/temporal/pull/396)
* Fix hoursInDay division constant by @nekevss in [#395](https://github.com/boa-dev/temporal/pull/395)
* Expose ParseTemporalCalendarString over FFI by @Manishearth in [#390](https://github.com/boa-dev/temporal/pull/390)
* Remove UTC designator check from time zone parsing by @nekevss in [#392](https://github.com/boa-dev/temporal/pull/392)
* Update ixdtf to icu4x main git branch by @robot-head in [#365](https://github.com/boa-dev/temporal/pull/365)
* Fix `9.5.8 AddDurationToYearMonth` abstract method by @HalidOdat in [#389](https://github.com/boa-dev/temporal/pull/389)
* Fix ZonedDateTime::nanosecond by @Manishearth in [#388](https://github.com/boa-dev/temporal/pull/388)
* Allow datetime strings for YearMonth/MonthDay/Time by @Manishearth in [#386](https://github.com/boa-dev/temporal/pull/386)
* Remove orgs/boa-dev/people from links by @nekevss in [#387](https://github.com/boa-dev/temporal/pull/387)
* Update maintainers in README.md by @jasonwilliams in [#384](https://github.com/boa-dev/temporal/pull/384)
* Re-add try_from_offset_str by @Manishearth in [#376](https://github.com/boa-dev/temporal/pull/376)
* Fix clippy lints for Rust 1.88 by @nekevss in [#377](https://github.com/boa-dev/temporal/pull/377)

## New Contributors
* @linusg made their first contribution in [#399](https://github.com/boa-dev/temporal/pull/399)

**Full Changelog**: https://github.com/boa-dev/temporal/compare/v0.0.10...v0.0.11

## What's Changed in v0.0.10
* Add documentation and doctests for builtins by @blarfoon in [#360](https://github.com/boa-dev/temporal/pull/360)
* More error enums by @Manishearth in [#373](https://github.com/boa-dev/temporal/pull/373)
* [capi] Add stringifier/cloning to timezones by @Manishearth in [#344](https://github.com/boa-dev/temporal/pull/344)
* Handle unknown timezone identifiers in FsTzdbProvider by @Manishearth in [#345](https://github.com/boa-dev/temporal/pull/345)
* [capi] Fix i128Nanoseconds by @Manishearth in [#372](https://github.com/boa-dev/temporal/pull/372)
* [capi] expose error strings by @Manishearth in [#364](https://github.com/boa-dev/temporal/pull/364)
* Consolidate tools into a single `tool` directory by @nekevss in [#368](https://github.com/boa-dev/temporal/pull/368)
* Add a new PartialYearMonth to available partial structs (#288) by @robot-head in [#342](https://github.com/boa-dev/temporal/pull/342)
* Implement zoneinfo parsing/compilation and add TZif structs by @nekevss in [#257](https://github.com/boa-dev/temporal/pull/257)
* Add ErrorMessage enum, start using it by @Manishearth in [#355](https://github.com/boa-dev/temporal/pull/355)
* [capi] Add is_valid() to I128Nanoseconds by @Manishearth in [#363](https://github.com/boa-dev/temporal/pull/363)
* [capi] Add ZonedDateTime::{equals,offset} by @Manishearth in [#362](https://github.com/boa-dev/temporal/pull/362)
* Add convenience methods for constructing FFI datetime types from milliseconds by @Manishearth in [#359](https://github.com/boa-dev/temporal/pull/359)
* [capi] Add offset_nanoseconds() to ZDT FFI by @Manishearth in [#361](https://github.com/boa-dev/temporal/pull/361)
* Update diplomat by @Manishearth in [#357](https://github.com/boa-dev/temporal/pull/357)
* Stop depending on `is_dst` for calculations by @jedel1043 in [#356](https://github.com/boa-dev/temporal/pull/356)
* Add some FAQ style docs for temporal_rs by @nekevss in [#350](https://github.com/boa-dev/temporal/pull/350)
* Add try_from_offset_str ctor for TimeZone by @Manishearth in [#348](https://github.com/boa-dev/temporal/pull/348)
* Switch compiled_data APIs to new CompiledTzdbProvider by @Manishearth in [#346](https://github.com/boa-dev/temporal/pull/346)

## New Contributors
* @blarfoon made their first contribution in [#360](https://github.com/boa-dev/temporal/pull/360)

**Full Changelog**: https://github.com/boa-dev/temporal/compare/v0.0.9...v0.0.10

## What's Changed in v0.0.9
* Cross boundary rounding fix #286 by @robot-head in [#343](https://github.com/boa-dev/temporal/pull/343)
* Implement PlainMonthDay::with functionality by @nekevss in [#335](https://github.com/boa-dev/temporal/pull/335)
* Add Writeable getters for some types, use in FFI by @Manishearth in [#340](https://github.com/boa-dev/temporal/pull/340)
* Add missing FFI APIs by @Manishearth in [#339](https://github.com/boa-dev/temporal/pull/339)
* Fill in missing Zoned APIs by @Manishearth in [#331](https://github.com/boa-dev/temporal/pull/331)
* Add stringifiers to MonthDay/YearMonth by @Manishearth in [#338](https://github.com/boa-dev/temporal/pull/338)
* Use AnyCalendarKind in PartialDate by @Manishearth in [#332](https://github.com/boa-dev/temporal/pull/332)
* Add ZonedDateTime FFI by @Manishearth in [#329](https://github.com/boa-dev/temporal/pull/329)
* Use a lock internally to FsTzdbProvider instead of externally by @Manishearth in [#327](https://github.com/boa-dev/temporal/pull/327)
* Remove create() APIs, consistently use try_new() by @Manishearth in [#326](https://github.com/boa-dev/temporal/pull/326)
* Switch FFI calendar APIs over to using AnyCalendarKind for input by @Manishearth in [#324](https://github.com/boa-dev/temporal/pull/324)
* Update abstract method `7.5.37 RoundRelativeDuration` by @HalidOdat in [#323](https://github.com/boa-dev/temporal/pull/323)
* Fix abstract method `7.5.36 BubbleRelativeDuration()` by @HalidOdat in [#322](https://github.com/boa-dev/temporal/pull/322)
* Generate FFI bindings for C by @jedel1043 in [#321](https://github.com/boa-dev/temporal/pull/321)
* Improve baked data formatting by @Manishearth in [#319](https://github.com/boa-dev/temporal/pull/319)
* Correctly validate largestUnit when constructing from DifferenceSettings by @Manishearth in [#316](https://github.com/boa-dev/temporal/pull/316)
* Align `Duration.prototype.round()` to latest specification by @HalidOdat in [#317](https://github.com/boa-dev/temporal/pull/317)

**Full Changelog**: https://github.com/boa-dev/temporal/compare/v0.0.8...v0.0.9

## What's Changed in v0.0.8
* Make duration capi getters non-optional by @Manishearth in [#314](https://github.com/boa-dev/temporal/pull/314)
* Add to_string to Duration by @Manishearth in [#312](https://github.com/boa-dev/temporal/pull/312)
* Add comparison methods to Instant by @Manishearth in [#311](https://github.com/boa-dev/temporal/pull/311)
* Add LICENSEs to timezone_provider crate by @Manishearth in [#308](https://github.com/boa-dev/temporal/pull/308)
* Prune dependencies of timezone_provider in normal mode, add depcheck by @Manishearth in [#310](https://github.com/boa-dev/temporal/pull/310)
* Add to_plain_date to PlainMonthDay and PlainYearMonth by @HenrikTennebekk in [#287](https://github.com/boa-dev/temporal/pull/287)
* Remove exclude and readme by @nekevss in [#304](https://github.com/boa-dev/temporal/pull/304)

**Full Changelog**: https://github.com/boa-dev/temporal/compare/v0.0.7...v0.0.8

## What's Changed in v0.0.7
* Bump ixdtf and complete changes for update by @nekevss in [#299](https://github.com/boa-dev/temporal/pull/299)
* A few more changes to the readme by @nekevss in [#297](https://github.com/boa-dev/temporal/pull/297)
* Implement a builder API for Now by @nekevss in [#296](https://github.com/boa-dev/temporal/pull/296)
* Some docs cleanup and updates by @nekevss in [#294](https://github.com/boa-dev/temporal/pull/294)
* Add `PlainMonthDay` method + clean up by @nekevss in [#284](https://github.com/boa-dev/temporal/pull/284)
* Add Eq, Ord impls on FiniteF64 by @sffc in [#187](https://github.com/boa-dev/temporal/pull/187)
* Allow parsers to accept unvalidated UTF8 by @HalidOdat in [#295](https://github.com/boa-dev/temporal/pull/295)
* Bump to icu_calendar 2.0 by @nekevss in [#292](https://github.com/boa-dev/temporal/pull/292)
* Add ISO specific constructors to builtins by @nekevss in [#263](https://github.com/boa-dev/temporal/pull/263)
* Rename the provider crate by @nekevss in [#289](https://github.com/boa-dev/temporal/pull/289)
* Expose equals and compare over FFI by @Magnus-Fjeldstad in [#269](https://github.com/boa-dev/temporal/pull/269)
* Impl round_with_provider for ZonedDateTIme by @sebastianjacmatt in [#278](https://github.com/boa-dev/temporal/pull/278)
* Add some compiled_data FFI APIs by @Manishearth in [#273](https://github.com/boa-dev/temporal/pull/273)
* Add string conversion functions for Temporal types by @Manishearth in [#276](https://github.com/boa-dev/temporal/pull/276)
* Make sure temporal_capi can be built no_std by @Manishearth in [#281](https://github.com/boa-dev/temporal/pull/281)
* Make iana-time-zone dep optional by @Manishearth in [#279](https://github.com/boa-dev/temporal/pull/279)
* Implementation of ZonedDateTime.prototype.with by @lockels in [#267](https://github.com/boa-dev/temporal/pull/267)
* Update Duration's inner representation from floating point to integers. by @nekevss in [#268](https://github.com/boa-dev/temporal/pull/268)
* Add all-features = true to docs.rs metadata by @Manishearth in [#271](https://github.com/boa-dev/temporal/pull/271)
* Fix instant math in capi by @Manishearth in [#270](https://github.com/boa-dev/temporal/pull/270)
* Remove the Temporal prefix from Unit, RoundingMode, and UnsignedRoundingMode by @nekevss in [#254](https://github.com/boa-dev/temporal/pull/254)
* Since until by @sebastianjacmatt in [#259](https://github.com/boa-dev/temporal/pull/259)
* Implementation of toZonedDateTimeISO for Instant by @lockels in [#258](https://github.com/boa-dev/temporal/pull/258)
* Implement toZonedDateTime in PlainDate by @JohannesHelleve in [#192](https://github.com/boa-dev/temporal/pull/192)
* Add intro documentation to ZonedDateTime and PlainDateTime by @nekevss in [#253](https://github.com/boa-dev/temporal/pull/253)
* Implement IANA normalizer baked data provider by @nekevss in [#251](https://github.com/boa-dev/temporal/pull/251)

## New Contributors
* @HalidOdat made their first contribution in [#295](https://github.com/boa-dev/temporal/pull/295)
* @JohannesHelleve made their first contribution in [#192](https://github.com/boa-dev/temporal/pull/192)

**Full Changelog**: https://github.com/boa-dev/temporal/compare/v0.0.6...v0.0.7

## What's Changed in v0.0.6
* Rename methods on `Now` and add a test by @nekevss in [#243](https://github.com/boa-dev/temporal/pull/243)
* Add licenses to `temporal_capi` and `temporal_rs` for publish by @nekevss in [#247](https://github.com/boa-dev/temporal/pull/247)
* Add with to PlainYearMonth by @sebastianjacmatt in [#231](https://github.com/boa-dev/temporal/pull/231)
* initial implementation of ToZonedDateTime, ToPlainDate, ToPlainTime by @lockels in [#244](https://github.com/boa-dev/temporal/pull/244)
* Initial implementation of Duration.prototype.total by @lockels in [#241](https://github.com/boa-dev/temporal/pull/241)

**Full Changelog**: https://github.com/boa-dev/temporal/compare/v0.0.5...v0.0.6

## What's Changed in v0.0.5
* Prepare temporal_capi for publish by @jedel1043 in [#238](https://github.com/boa-dev/temporal/pull/238)
* Adjustments to `toPlainYearMonth` and `toPlainMonthDay` methods on PlainDate by @nekevss in [#237](https://github.com/boa-dev/temporal/pull/237)
* Missed an unwrap in the README.md example by @nekevss in [#236](https://github.com/boa-dev/temporal/pull/236)
* Clean up API ergonomics for calendar methods by @nekevss in [#235](https://github.com/boa-dev/temporal/pull/235)
* Add various updates to docs by @nekevss in [#234](https://github.com/boa-dev/temporal/pull/234)
* Reject datetime when fraction digits are too large by @nekevss in [#229](https://github.com/boa-dev/temporal/pull/229)
* Fix not adjusting fraction for duration unit by @nekevss in [#228](https://github.com/boa-dev/temporal/pull/228)
* Fixes for `EpochNanosecond`s and `Offset` parsing by @nekevss in [#223](https://github.com/boa-dev/temporal/pull/223)
* Fix bugs around validating diffing units by @nekevss in [#225](https://github.com/boa-dev/temporal/pull/225)
* Add `UtcOffset` struct for `PartialZonedDateTime` by @nekevss in [#207](https://github.com/boa-dev/temporal/pull/207)
* Check in bindings, add CI for keeping them up to date by @Manishearth in [#220](https://github.com/boa-dev/temporal/pull/220)
* Fix `Instant::epoch_milliseconds` for values before Epoch by @nekevss in [#221](https://github.com/boa-dev/temporal/pull/221)
* Update ixdtf to 0.4.0 by @Manishearth in [#219](https://github.com/boa-dev/temporal/pull/219)
* Update icu4x to 2.0.0-beta2 by @Manishearth in [#218](https://github.com/boa-dev/temporal/pull/218)
* Add `GetNamedTimeZoneTransition` method to TimeZoneProvider trait by @nekevss in [#203](https://github.com/boa-dev/temporal/pull/203)
* Implement posix resolution for month-week-day by @jedel1043 in [#214](https://github.com/boa-dev/temporal/pull/214)
* implement utility methods on partial structs by @nekevss in [#206](https://github.com/boa-dev/temporal/pull/206)
* Test all combinations of features by @jedel1043 in [#212](https://github.com/boa-dev/temporal/pull/212)
* Reject non-iso calendar in `YearMonth::from_str` and `MonthDay::from_str` by @nekevss in [#211](https://github.com/boa-dev/temporal/pull/211)
* Fix not validating `MonthCode` in ISO path by @nekevss in [#210](https://github.com/boa-dev/temporal/pull/210)
* Integrate `MonthCode` into public API and related adjustments by @nekevss in [#208](https://github.com/boa-dev/temporal/pull/208)
* Implement the remaining non-ISO calendar method calls by @nekevss in [#209](https://github.com/boa-dev/temporal/pull/209)
* PlainYearMonth parsing and Calendar field resolution cleanup/fixes by @nekevss in [#205](https://github.com/boa-dev/temporal/pull/205)
* Build out stubs for remaining unimplemented methods by @nekevss in [#202](https://github.com/boa-dev/temporal/pull/202)
* Fix clippy lints by @nekevss in [#201](https://github.com/boa-dev/temporal/pull/201)
* Temporal duration compare by @sffc, @lockels, @Neelzee, @sebastianjacmatt, @Magnus-Fjeldstad and @HenrikTennebekk. in [#186](https://github.com/boa-dev/temporal/pull/186)
* Fix issues with `from_partial` method implementations by @nekevss in [#200](https://github.com/boa-dev/temporal/pull/200)
* More FFI: Finish Calendar, add Instant by @Manishearth in [#198](https://github.com/boa-dev/temporal/pull/198)
* Fix parsing bugs related to UTC Designator usage by @nekevss in [#197](https://github.com/boa-dev/temporal/pull/197)
* Update time parsing to error on dup critical calendars by @nekevss in [#196](https://github.com/boa-dev/temporal/pull/196)
* Update unit group validation to handle rounding options by @nekevss in [#194](https://github.com/boa-dev/temporal/pull/194)
* Fix handling of leap seconds in parsing by @nekevss in [#195](https://github.com/boa-dev/temporal/pull/195)
* Update time zone parsing to include other ixdtf formats by @nekevss in [#193](https://github.com/boa-dev/temporal/pull/193)
* Fix calendar parsing on `from_str` implementation by @nekevss in [#191](https://github.com/boa-dev/temporal/pull/191)
* Cleanup `Now` API in favor of system defined implementations by @nekevss in [#182](https://github.com/boa-dev/temporal/pull/182)
* Reimplement Unit Group with different approach by @nekevss in [#183](https://github.com/boa-dev/temporal/pull/183)
* Implement `ZonedDateTime::offset` and `ZonedDateTime::offset_nanoseconds` by @nekevss in [#185](https://github.com/boa-dev/temporal/pull/185)
* Small API cleanup and a couple dev docs updates by @nekevss in [#190](https://github.com/boa-dev/temporal/pull/190)
* Add Eq and Ord for PlainYearMonth + Eq for PlainMonthDay by @nekevss in [#175](https://github.com/boa-dev/temporal/pull/175)
* More FFI APIs by @Manishearth in [#178](https://github.com/boa-dev/temporal/pull/178)
* Fix the typo that's returning milliseconds instead of microseconds by @nekevss in [#184](https://github.com/boa-dev/temporal/pull/184)
* Add some FFI tests by @Manishearth in [#179](https://github.com/boa-dev/temporal/pull/179)
* Fix Instant parsing by handling order of operations and properly balance `IsoDateTime` by @nekevss in [#174](https://github.com/boa-dev/temporal/pull/174)
* Add some testing / debugging docs by @nekevss in [#176](https://github.com/boa-dev/temporal/pull/176)
* Fix logic on asserting is_time_duration by @nekevss in [#177](https://github.com/boa-dev/temporal/pull/177)
* Rework library restructure to remove wrapper types by @nekevss in [#181](https://github.com/boa-dev/temporal/pull/181)
* Restructure project to separate core provider APIs from non-provider APIs by @nekevss in [#169](https://github.com/boa-dev/temporal/pull/169)
* Fix `Duration` parsing not returning a range error. by @nekevss in [#173](https://github.com/boa-dev/temporal/pull/173)
* Set up basic diplomat workflow by @Manishearth in [#163](https://github.com/boa-dev/temporal/pull/163)
* Implement Neri-Schneider calculations by @nekevss in [#147](https://github.com/boa-dev/temporal/pull/147)
* Remove `UnitGroup` addition by @nekevss in [#171](https://github.com/boa-dev/temporal/pull/171)
* Implement `ZonedDateTime::since` and `ZonedDateTime::until` by @nekevss in [#170](https://github.com/boa-dev/temporal/pull/170)
* Implement to_string functionality and methods for `Duration`, `PlainYearMonth`, and `PlainMonthDay` by @nekevss in [#164](https://github.com/boa-dev/temporal/pull/164)
* API cleanup, visibility updates, and tech debt cleanup by @nekevss in [#168](https://github.com/boa-dev/temporal/pull/168)
* Add to_string functionality for timezone identifer by @nekevss in [#161](https://github.com/boa-dev/temporal/pull/161)
* Bug fixes to address test failures + removing unused API by @nekevss in [#162](https://github.com/boa-dev/temporal/pull/162)
* Implement correct resolving of `getStartOfDay` by @jedel1043 in [#159](https://github.com/boa-dev/temporal/pull/159)
* Add an MSRV check to CI by @nekevss in [#158](https://github.com/boa-dev/temporal/pull/158)
* Fixing panics in test262 when running in debug mode by @nekevss in [#157](https://github.com/boa-dev/temporal/pull/157)
* Fix edge case for disambiguating `ZonedDateTime`s on DSTs skipping midnight by @jedel1043 in [#156](https://github.com/boa-dev/temporal/pull/156)
* Extend implementation of `to_ixdtf_string` to more types by @nekevss in [#155](https://github.com/boa-dev/temporal/pull/155)
* Implement support for `to_string` and implement `PlainDate::to_string` by @nekevss in [#153](https://github.com/boa-dev/temporal/pull/153)
* Fix RoundingMode::truncation to UnsignedRoundingMode mapping by @nekevss in [#146](https://github.com/boa-dev/temporal/pull/146)
* Add validation logic to `from_diff_settings` by @nekevss in [#144](https://github.com/boa-dev/temporal/pull/144)
* Build out the rest of the Now methods by @nekevss in [#145](https://github.com/boa-dev/temporal/pull/145)
* Adjust `RelativeTo` according to specification and implementation by @nekevss in [#140](https://github.com/boa-dev/temporal/pull/140)
* Complete some general cleanup of `temporal_rs` by @nekevss in [#138](https://github.com/boa-dev/temporal/pull/138)
* Add ZonedDateTime functionality to `Duration::round` by @nekevss in [#134](https://github.com/boa-dev/temporal/pull/134)
* Update try_new, new, and new_with_overflow integer type by @nekevss in [#137](https://github.com/boa-dev/temporal/pull/137)
* Fix `Calendar::from_str` to be case-insensitive by @nekevss in [#135](https://github.com/boa-dev/temporal/pull/135)
* Add ToIntegerWithTruncation, ToPositiveIntegerWithTruncation, and ToIntegerIfIntegral methods to `FiniteF64` by @nekevss in [#131](https://github.com/boa-dev/temporal/pull/131)
* Add to-x methods and with-x methods to ZonedDateTime by @nekevss in [#129](https://github.com/boa-dev/temporal/pull/129)
* Fix `epoch_time_to_epoch_year` date equation bug related to `BalanceISODate` by @nekevss in [#132](https://github.com/boa-dev/temporal/pull/132)
* Bump dependencies for `ixdtf`, `tzif`, `icu_calendar`, and `tinystr`. by @nekevss in [#133](https://github.com/boa-dev/temporal/pull/133)
* Fix bug introduced by `EpochNanoseconds` + adjust tests to catch better by @nekevss in [#128](https://github.com/boa-dev/temporal/pull/128)
* Remove epochSeconds and epochMicroseconds + adjust epochMillseconds by @nekevss in [#127](https://github.com/boa-dev/temporal/pull/127)
* Adjust compilation configuration of tzdb to target_family from target_os by @nekevss in [#125](https://github.com/boa-dev/temporal/pull/125)
* Migrate repo to workspace by @jedel1043 in [#126](https://github.com/boa-dev/temporal/pull/126)
* Add now feature flag by @nekevss in [#123](https://github.com/boa-dev/temporal/pull/123)
* Add  `ZonedDateTime` calendar accessor methods by @nekevss in [#117](https://github.com/boa-dev/temporal/pull/117)
* Implement  `PartialZonedDateTime` and `from_partial` and `from_str` for `ZonedDateTime` by @nekevss in [#115](https://github.com/boa-dev/temporal/pull/115)
* Update CHANGELOG for v0.0.4 release by @nekevss in [#124](https://github.com/boa-dev/temporal/pull/124)
* Patch the now test per matrix discussion by @nekevss in [#121](https://github.com/boa-dev/temporal/pull/121)

## New Contributors
* @sffc made their first contribution in [#186](https://github.com/boa-dev/temporal/pull/186)
* @lockels made their first contribution in [#186](https://github.com/boa-dev/temporal/pull/186)
* @Neelzee made their first contribution in [#186](https://github.com/boa-dev/temporal/pull/186)
* @sebastianjacmatt made their first contribution in [#186](https://github.com/boa-dev/temporal/pull/186)
* @Magnus-Fjeldstad made their first contribution in [#186](https://github.com/boa-dev/temporal/pull/186)
* @HenrikTennebekk made their first contribution in [#186](https://github.com/boa-dev/temporal/pull/186)
* @cassioneri made their first contribution in [#147](https://github.com/boa-dev/temporal/pull/147)

**Full Changelog**: https://github.com/boa-dev/temporal/compare/v0.0.4...v0.0.5

## What's Changed in v0.0.4

* bump release by @jasonwilliams in [#120](https://github.com/boa-dev/temporal/pull/120)
* Add an `EpochNanosecond` new type by @nekevss in [#116](https://github.com/boa-dev/temporal/pull/116)
* Migrate to `web_time::SystemTime` for `wasm32-unknown-unknown` targets by @nekevss in [#118](https://github.com/boa-dev/temporal/pull/118)
* Bug fixes and more implementation by @jasonwilliams in [#110](https://github.com/boa-dev/temporal/pull/110)
* Some `Error` optimizations by @CrazyboyQCD in [#112](https://github.com/boa-dev/temporal/pull/112)
* Add `from_partial` methods to `PlainTime`, `PlainDate`, and `PlainDateTime` by @nekevss in [#106](https://github.com/boa-dev/temporal/pull/106)
* Implement `ZonedDateTime`'s add and subtract methods by @nekevss in [#102](https://github.com/boa-dev/temporal/pull/102)
* Add matrix links to README and some layout adjustments by @nekevss in [#108](https://github.com/boa-dev/temporal/pull/108)
* Stub out `tzdb` support for Windows and POSIX tz string by @nekevss in [#100](https://github.com/boa-dev/temporal/pull/100)
* Stub out tzdb support to unblock `Now` and `ZonedDateTime` by @nekevss in [#99](https://github.com/boa-dev/temporal/pull/99)
* Remove num-bigint dependency and rely on primitives by @nekevss in [#103](https://github.com/boa-dev/temporal/pull/103)
* Move to no_std by @Manishearth in [#101](https://github.com/boa-dev/temporal/pull/101)
* General API cleanup and adjustments by @nekevss in [#97](https://github.com/boa-dev/temporal/pull/97)
* Update README.md by @jasonwilliams in [#96](https://github.com/boa-dev/temporal/pull/96)
* Refactor `TemporalFields` into `CalendarFields` by @nekevss in [#95](https://github.com/boa-dev/temporal/pull/95)
* Patch for partial records by @nekevss in [#94](https://github.com/boa-dev/temporal/pull/94)
* Add `PartialTime` and `PartialDateTime` with corresponding `with` methods. by @nekevss in [#92](https://github.com/boa-dev/temporal/pull/92)
* Implement `MonthCode`, `PartialDate`, and `Date::with` by @nekevss in [#89](https://github.com/boa-dev/temporal/pull/89)
* Add is empty for partialDuration by @jasonwilliams in [#90](https://github.com/boa-dev/temporal/pull/90)
* Fix lints for rustc 1.80.0 by @jedel1043 in [#91](https://github.com/boa-dev/temporal/pull/91)
* adding methods for yearMonth and MonthDay by @jasonwilliams in [#44](https://github.com/boa-dev/temporal/pull/44)
* Implement `DateTime` round method by @nekevss in [#88](https://github.com/boa-dev/temporal/pull/88)
* Update `Duration` types to use a `FiniteF64` instead of `f64` primitive. by @nekevss in [#86](https://github.com/boa-dev/temporal/pull/86)
* Refactor `TemporalFields` interface and add `FieldsKey` enum by @nekevss in [#87](https://github.com/boa-dev/temporal/pull/87)
* Updates to instant and its methods by @nekevss in [#85](https://github.com/boa-dev/temporal/pull/85)
* Implement compare functionality and some more traits by @nekevss in [#82](https://github.com/boa-dev/temporal/pull/82)
* Implement `DateTime` diffing methods `Until` and `Since` by @nekevss in [#83](https://github.com/boa-dev/temporal/pull/83)
* Add `with_*` methods to `Date` and `DateTime` by @nekevss in [#84](https://github.com/boa-dev/temporal/pull/84)
* Add some missing trait implementations by @nekevss in [#81](https://github.com/boa-dev/temporal/pull/81)
* chore(dependabot): bump zerovec-derive from 0.10.2 to 0.10.3 by @dependabot[bot] in [#80](https://github.com/boa-dev/temporal/pull/80)
* Add prefix option to commit-message by @nekevss in [#79](https://github.com/boa-dev/temporal/pull/79)
* Add commit-message prefix to dependabot by @nekevss in [#77](https://github.com/boa-dev/temporal/pull/77)
* Bump zerovec from 0.10.2 to 0.10.4 by @dependabot[bot] in [#78](https://github.com/boa-dev/temporal/pull/78)

## New Contributors
* @jasonwilliams made their first contribution in [#120](https://github.com/boa-dev/temporal/pull/120)
* @CrazyboyQCD made their first contribution in [#112](https://github.com/boa-dev/temporal/pull/112)
* @Manishearth made their first contribution in [#101](https://github.com/boa-dev/temporal/pull/101)

**Full Changelog**: https://github.com/boa-dev/temporal/compare/v0.0.3...v0.0.4

# CHANGELOG

## What's Changed in v0.0.3

* Implement add and subtract methods for Duration by @nekevss in [#74](https://github.com/boa-dev/temporal/pull/74)
* Implement PartialEq and Eq for `Calendar`, `Date`, and `DateTime` by @nekevss in [#75](https://github.com/boa-dev/temporal/pull/75)
* Update duration validation and switch asserts to debug-asserts by @nekevss in [#73](https://github.com/boa-dev/temporal/pull/73)
* Update duration rounding to new algorithms by @nekevss in [#65](https://github.com/boa-dev/temporal/pull/65)
* Remove `CalendarProtocol` and `TimeZoneProtocol` by @jedel1043 in [#66](https://github.com/boa-dev/temporal/pull/66)
* Use groups in dependabot updates by @jedel1043 in [#69](https://github.com/boa-dev/temporal/pull/69)
* Ensure parsing throws with unknown critical annotations by @jedel1043 in [#63](https://github.com/boa-dev/temporal/pull/63)
* Reject `IsoDate` when outside the allowed range by @jedel1043 in [#62](https://github.com/boa-dev/temporal/pull/62)
* Avoid overflowing when calling `NormalizedTimeDuration::add_days` by @jedel1043 in [#61](https://github.com/boa-dev/temporal/pull/61)
* Ensure parsing throws when duplicate calendar is critical by @jedel1043 in [#58](https://github.com/boa-dev/temporal/pull/58)
* Fix rounding when the dividend is smaller than the divisor by @jedel1043 in [#57](https://github.com/boa-dev/temporal/pull/57)
* Implement the `toYearMonth`, `toMonthDay`, and `toDateTime` for `Date` component by @nekevss in [#56](https://github.com/boa-dev/temporal/pull/56)
* Update increment rounding functionality by @nekevss in [#53](https://github.com/boa-dev/temporal/pull/53)
* Patch `(un)balance_relative` to avoid panicking by @jedel1043 in [#48](https://github.com/boa-dev/temporal/pull/48)
* Cleanup rounding increment usages with new struct by @jedel1043 in [#54](https://github.com/boa-dev/temporal/pull/54)
* Add struct to encapsulate invariants of rounding increments by @jedel1043 in [#49](https://github.com/boa-dev/temporal/pull/49)
* Migrate parsing to `ixdtf` crate by @nekevss in [#50](https://github.com/boa-dev/temporal/pull/50)
* Fix method call in days_in_month by @nekevss in [#46](https://github.com/boa-dev/temporal/pull/46)
* Implement add & subtract methods for `DateTime` component by @nekevss in [#45](https://github.com/boa-dev/temporal/pull/45)
* Fix panics when no relative_to is supplied to round by @nekevss in [#40](https://github.com/boa-dev/temporal/pull/40)
* Implement Time's until and since methods by @nekevss in [#36](https://github.com/boa-dev/temporal/pull/36)
* Implements `Date`'s `add`, `subtract`, `until`, and `since` methods by @nekevss in [#35](https://github.com/boa-dev/temporal/pull/35)
* Fix clippy lints and bump bitflags version by @nekevss in [#38](https://github.com/boa-dev/temporal/pull/38)

**Full Changelog**: https://github.com/boa-dev/temporal/compare/v0.0.2...v0.0.3

## What's Changed in v0.0.2

# [0.0.2 (2024-03-04)](https://github.com/boa-dev/temporal/compare/v0.0.1...v0.0.2)

### Enhancements

* Fix loop in `diff_iso_date` by @nekevss in https://github.com/boa-dev/temporal/pull/31
* Remove unnecessary iterations by @nekevss in https://github.com/boa-dev/temporal/pull/30

**Full Changelog**: https://github.com/boa-dev/temporal/compare/v0.0.1...v0.0.2

# [0.0.1 (2024-02-25)](https://github.com/boa-dev/temporal/commits/v0.0.1)

### Enhancements
* Add blank and negated + small adjustments by @nekevss in https://github.com/boa-dev/temporal/pull/17
* Simplify Temporal APIs by @jedel1043 in https://github.com/boa-dev/temporal/pull/18
* Implement `Duration` normalization - Part 1 by @nekevss in https://github.com/boa-dev/temporal/pull/20
* Duration Normalization - Part 2 by @nekevss in https://github.com/boa-dev/temporal/pull/23
* Add `non_exhaustive` attribute to component structs by @nekevss in https://github.com/boa-dev/temporal/pull/25
* Implement `Duration::round` and some general updates/fixes by @nekevss in https://github.com/boa-dev/temporal/pull/24

### Documentation
* Adding a `docs` directory by @nekevss in https://github.com/boa-dev/temporal/pull/16
* Build out README and CONTRIBUTING docs by @nekevss in https://github.com/boa-dev/temporal/pull/21

### Other Changes
* Port `boa_temporal` to new `temporal` crate by @nekevss in https://github.com/boa-dev/temporal/pull/1
* Add CI and rename license by @jedel1043 in https://github.com/boa-dev/temporal/pull/3
* Create LICENSE-Apache by @jedel1043 in https://github.com/boa-dev/temporal/pull/6
* Setup publish CI by @jedel1043 in https://github.com/boa-dev/temporal/pull/26
* Remove keywords from Cargo.toml by @jedel1043 in https://github.com/boa-dev/temporal/pull/28

## New Contributors
* @nekevss made their first contribution in https://github.com/boa-dev/temporal/pull/1
* @jedel1043 made their first contribution in https://github.com/boa-dev/temporal/pull/3

**Full Changelog**: https://github.com/boa-dev/temporal/commits/v0.0.1