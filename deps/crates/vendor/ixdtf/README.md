# ixdtf [![crates.io](https://img.shields.io/crates/v/ixdtf)](https://crates.io/crates/ixdtf)

<!-- cargo-rdme start -->

Parsers for extended date time string and Duration parsing.

The [Internet Extended Date/Time Fmt (IXDTF)][rfc9557] is laid out by RFC 9557. RFC 9557
builds on RFC 3339's time stamp specification and ISO 8601 to provide an optional extension
syntax for date/time strings.

RFC 9557 also updates the interpretation of `Z` from RFC 3339.

## Date Time Extended Examples

- `2024-03-02T08:48:00-05:00[America/New_York]`
- `2024-03-02T08:48:00-05:00[-05:00]`
- `2024-03-02T08:48:00-05:00[u-ca=iso8601]`

### Example Usage

```rust
use ixdtf::{
    parsers::IxdtfParser,
    records::{Sign, TimeZoneRecord},
};

let ixdtf_str = "2024-03-02T08:48:00-05:00[America/New_York]";

let result = IxdtfParser::from_str(ixdtf_str).parse().unwrap();

let date = result.date.unwrap();
let time = result.time.unwrap();
let offset = result.offset.unwrap().resolve_rfc_9557();
let tz_annotation = result.tz.unwrap();

assert_eq!(date.year, 2024);
assert_eq!(date.month, 3);
assert_eq!(date.day, 2);
assert_eq!(time.hour, 8);
assert_eq!(time.minute, 48);
assert_eq!(offset.sign(), Sign::Negative);
assert_eq!(offset.hour(), 5);
assert_eq!(offset.minute(), 0);
assert_eq!(offset.second(), None);
assert_eq!(offset.fraction(), None);
assert!(!tz_annotation.critical);
assert_eq!(
    tz_annotation.tz,
    TimeZoneRecord::Name("America/New_York".as_bytes())
);
```

### Date/Time Strings

The extended suffixes laid out by RFC 9557 are optional, so the `IxdtfParser`
will also still parse any valid date time strings described by RFC3339.

Example Valid Date Time Strings:

- `2024-03-02`
- `+002024-03-02`
- `20240302`
- `+0020240302`
- `2024-03-02T08:48:00`
- `2024-03-02T08:48:00`

### Updates to Zulu interpretation from RFC 3339

RFC 3339 interpreted both `+00:00` and `Z` "UTC is the preferred reference point for the
specified time"; meanwhile, `-00:00` expressed "the time in UTC is known, but the local
time is unknown".

RFC 9557 updates the interpretation of `Z` to align with `-00:00`.

```rust
use ixdtf::{
    parsers::IxdtfParser,
    records::{Sign, TimeZoneRecord},
};

let ixdtf_str = "2024-03-02T08:48:00Z[America/New_York]";

let result = IxdtfParser::from_str(ixdtf_str).parse().unwrap();

let date = result.date.unwrap();
let time = result.time.unwrap();
let offset = result.offset.unwrap().resolve_rfc_9557();
let tz_annotation = result.tz.unwrap();

assert_eq!(date.year, 2024);
assert_eq!(date.month, 3);
assert_eq!(date.day, 2);
assert_eq!(time.hour, 8);
assert_eq!(time.minute, 48);
assert_eq!(offset.sign(), Sign::Negative);
assert_eq!(offset.hour(), 0);
assert_eq!(offset.minute(), 0);
assert_eq!(offset.second(), None);
assert_eq!(offset.fraction(), None);
assert!(!tz_annotation.critical);
assert_eq!(
    tz_annotation.tz,
    TimeZoneRecord::Name("America/New_York".as_bytes())
);
```

For more information on the update to RFC 3339, please see RFC 9557, Section 2.

For more information on `Z` along with time zone annotations, please see the Annotations
with Application Defined Behavior section below.

### IXDTF Extensions: A Deeper Look

The suffix extensions come in two primary kinds: a time zone annotation and a key-value
annotation. The suffixes may also be flagged as critical with a `!` as a leading flag
character.

#### Time Zone Annotations

Time zone annotations can be either a valid IANA time zone name or numeric
offset.

##### Valid Time Zone Annotations

- `2024-03-02T08:48:00-5:00[America/New_York]`
- `2024-03-02T08:48:00-5:00[-05:00]`
- `2024-03-02T08:48:00Z[America/New_York]`

###### Time Zone Consistency

With the update to RFC 3339, when `Z` is provided as a datetime offset along side a time zone
annotation, the IXDTF string is not considered inconsistent as `Z` does not assert any local
time. Instead, an application may decide to calculate the time with the rules of the time
zone annotation if it is provided.

```rust
use ixdtf::{
    parsers::IxdtfParser,
    records::{Sign, TimeZoneRecord},
};

let zulu_offset = "2024-03-02T08:48:00Z[!America/New_York]";

let result = IxdtfParser::from_str(zulu_offset).parse().unwrap();

let tz_annotation = result.tz.unwrap();
let offset = result.offset.unwrap().resolve_rfc_9557();

// The offset is `Z`/`-00:00`, so the application can use the rules of
// "America/New_York" to calculate the time for IXDTF string.
assert_eq!(offset.sign(), Sign::Negative);
assert_eq!(offset.hour(), 0);
assert_eq!(offset.minute(), 0);
assert_eq!(offset.second(), None);
assert_eq!(offset.fraction(), None);
assert!(tz_annotation.critical);
assert_eq!(
    tz_annotation.tz,
    TimeZoneRecord::Name("America/New_York".as_bytes())
);
```

#### Key-Value Annotations

Key-value pair annotations are any key and value string separated by a '=' character.
Key-value pairs are can include any information. Keys may be permanently registered,
provisionally registered, or unknown; however, only permanent keys are acted on by
`IxdtfParser`.

If duplicate registered keys are provided the first key will be returned, unless one
of the duplicate annotations is marked as critical, in which case an error may be
thrown by the `ixdtf` (See [Invalid Annotations](#invalid-annotations) for more
information).

##### Permanent Registered Keys

- `u-ca`

##### Valid Annotations

- (1) `2024-03-02T08:48:00-05:00[America/New_York][u-ca=iso8601]`
- (2) `2024-03-02T08:48:00-05:00[u-ca=iso8601][u-ca=japanese]`
- (3) `2024-03-02T08:48:00-05:00[u-ca=iso8601][!u-ca=iso8601]`
- (4) `2024-03-02T08:48:00-05:00[u-ca=iso8601][answer-to-universe=fortytwo]`

###### Example 1

This is a basic annotation string that has a Time Zone and calendar annotation.

###### Example 2

This example is duplicate and different calendar annotations, but neither calendar
is flagged as critical so the first calendar is returned while the second calendar
is ignored.

###### Example 3

This example is a duplicate and identical calendar annotations with one annotation flagged
as critical. As the annotations are identical values, there is no ambiguity with the use of
the critical flag that may cause an error. Thus, the first annotation is returned, and the
second is ignored (See [Annotations with Application Defined
Behavior](#annotations-with-application-defined-behavior)).

###### Example 4

This example contains an unknown annotation. The annotation is not marked as critical
so the value is ignored (See [Implementing Annotation Handlers](#implementing-annotation-handlers)).

##### Invalid Annotations

The below `ixdtf` strings have invalid annotations that will cause an error
to be thrown (NOTE: these are not to be confused with potentially invalid
annotations with application defined behavior).

- (1) `2024-03-02T08:48:00-05:00[u-ca=iso8601][America/New_York]`
- (2) `2024-03-02T08:48:00-05:00[u-ca=iso8601][!u-ca=japanese]`
- (3) `2024-03-02T08:48:00-05:00[u-ca=iso8601][!answer-to-universe=fortytwo]`

###### Example 1

This example shows a Time Zone annotation that is not currently in the correct
order with the key value. When parsing this invalid annotation, `ixdtf`
will attempt to parse the Time Zone annotation as a key-value annotation.

```rust
use ixdtf::{parsers::IxdtfParser, ParseError};

let example_one =
    "2024-03-02T08:48:00-05:00[u-ca=iso8601][America/New_York]";

let result = IxdtfParser::from_str(example_one).parse();

assert_eq!(result, Err(ParseError::AnnotationKeyLeadingChar));
```

###### Example 2

This example shows a duplicate registered key; however, in this case, one
of the registered keys is flagged as critical, which throws an error as
the ixdtf string must be treated as erroneous

```rust
use ixdtf::{parsers::IxdtfParser, ParseError};

let example_two = "2024-03-02T08:48:00-05:00[u-ca=iso8601][!u-ca=japanese]";

let result = IxdtfParser::from_str(example_two).parse();

assert_eq!(result, Err(ParseError::CriticalDuplicateCalendar));
```

###### Example 3

This example shows an unknown key flagged as critical. `ixdtf` will return an
error on an unknown flag being flagged as critical.

```rust
use ixdtf::{parsers::IxdtfParser, ParseError};

let example_three =
    "2024-03-02T08:48:00-05:00[u-ca=iso8601][!answer-to-universe=fortytwo]";

let result = IxdtfParser::from_str(example_three).parse();

assert_eq!(result, Err(ParseError::UnrecognizedCritical));
```

##### Annotations with Application Defined Behavior

The below options may be viewed as valid or invalid depending on application defined
behavior. Where user defined behavior might be required, the `ixdtf` crate applies
the logic in the least restrictive interpretation and provides optional callbacks
for the user to define stricter behavior.

- (1) `2024-03-02T08:48:00-05:00[u-ca=japanese][!u-ca=japanese]`
- (2) `2024-03-02T08:48:00+01:00[America/New_York]`

###### Example 1

This example shows a critical duplicate calendar where the annotation value is identical. RFC 9557 is
ambiguous on whether this should be rejected for inconsistency. `ixdtf` treats these values
as consistent, and, therefore, okay. However, an application may wish to handle this duplicate
critical calendar value as inconsistent (See [Implementing Annotation Handlers](#implementing-annotation-handlers)).

###### Example 2

This example shows an ambiguous Time Zone caused by a misalignment
of the offset and the Time Zone annotation. It is up to the user to handle this ambiguity
between the offset and annotation.

```rust
use ixdtf::{parsers::IxdtfParser, records::TimeZoneRecord};

let example_two = "2024-03-02T08:48:00+01:00[!America/New_York]";

let result = IxdtfParser::from_str(example_two).parse().unwrap();

let tz_annotation = result.tz.unwrap();
let offset = result.offset.unwrap().resolve_rfc_9557();

// The time zone annotation and offset conflict with each other, and must therefore be
// resolved by the user.
assert!(tz_annotation.critical);
assert_eq!(tz_annotation.tz, TimeZoneRecord::Name("America/New_York".as_bytes()));
assert_eq!(offset.hour(), 1);
```

##### Implementing Annotation Handlers

As mentioned in the prior section, there may be times where an application may
need to implement application defined behavior for user defined functionality.
In this instance, `ixdtf` provides a `*_with_annotation_handler` method that
allows to the user to provide a callback.

A handler is defined as `handler: impl FnMut(Annotation<'a>) -> Option<Annotation<'a>>`
where `ixdtf` provides visibility to an annotation to the user. The call to this callback
occurs prior to the `ixdtf`'s processing of the annotation, and will only occur if the
annotation is provided back to `ixdtf`.

If the user wishes to ignore any `ixdtf`'s errors, then they may return `None`, which
results in a no-op for that annotation.

Unless the user’s application has a specific reason to bypass action on an annotation,
such as, custom unknown key handling or superceding a calendar based on it’s critical
flag, it is recommended to return the annotation value.

###### Handler Example

A user may wish to implement a custom key in an annotation set. This can be completed
with custom handler.

```rust
use ixdtf::parsers::IxdtfParser;

let example_with_custom_key = "2024-03-02T08:48:00-05:00[u-ca=iso8601][!answer-to-universe=fortytwo]";

let mut answer = None;

let _ = IxdtfParser::from_str(example_with_custom_key).parse_with_annotation_handler(|annotation| {
    if annotation.key == "answer-to-universe".as_bytes() {
        answer.get_or_insert(annotation);
        // Found our value! We don't need `ixdtf` to handle this annotation.
        return None
    }
    // The annotation is not our custom annotation, so we return
    // the value back for regular logic.
    Some(annotation)
}).unwrap();

let answer = answer.unwrap();

assert!(answer.critical);
assert_eq!(answer.value, "fortytwo".as_bytes());
```

It is worth noting that in the above example the annotation above found is a critically flagged
unknown key. RFC 9557 and `ixdtf` considers unknown critical keys as invalid. However, handlers
allow the user to define any known keys of their own and therefore also handle the logic around
criticality.

### Additional grammar resources

Additional resources for Date and Time string grammar can be found in [RFC3339][rfc3339]
and the [Temporal proposal][temporal-grammar].

### Additional Feature

The `ixdtf` crate also implements an ISO8601 Duration parser (`IsoDurationParser`) that is available under
the `duration` feature flag. The API for `IsoDurationParser` is the same as `IxdtfParser`, but
parses duration strings over date/time strings.

[rfc9557]: https://datatracker.ietf.org/doc/rfc9557/
[rfc3339]: https://datatracker.ietf.org/doc/html/rfc3339
[temporal-grammar]: https://tc39.es/proposal-temporal/#sec-temporal-iso8601grammar

<!-- cargo-rdme end -->

## More Information

For more information on development, authorship, contributing etc. please visit [`ICU4X home page`](https://github.com/unicode-org/icu4x).
