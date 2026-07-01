// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

//! [`AST`](self) provides a set of Syntax Tree Nodes used to store
//! the output of [`parse`] method that is used in [`test_condition`] method
//! to evaluate whether a given [`PluralCategory`] should be used.
//!
//! <div class="stab unstable">
//! 🚧 This code is considered unstable; it may change at any time, in breaking or non-breaking ways,
//! including in SemVer minor releases. In particular, the `DataProvider` implementations are only
//! guaranteed to match with this version's `*_unstable` providers. Use with caution.
//! </div>
//!
//! # Examples
//!
//! ```
//! use icu::plurals::provider::rules::reference::ast::*;
//! use icu::plurals::provider::rules::reference::parse_condition;
//!
//! let input = "i = 1";
//!
//! let ast = parse_condition(input.as_bytes()).expect("Parsing failed.");
//!
//! assert_eq!(
//!     ast,
//!     Condition(vec![AndCondition(vec![Relation {
//!         expression: Expression {
//!             operand: Operand::I,
//!             modulus: None,
//!         },
//!         operator: Operator::Eq,
//!         range_list: RangeList(vec![RangeListItem::Value(Value(1))])
//!     }])])
//! );
//! ```
//!
//! [`PluralCategory`]: crate::PluralCategory
//! [`parse`]: super::parse()
//! [`test_condition`]: super::test_condition()
use alloc::string::String;
use alloc::vec::Vec;
use core::ops::RangeInclusive;

/// A complete AST representation of a plural rule.
/// Comprises a vector of [`AndConditions`] and optionally a set of [`Samples`].
///
/// <div class="stab unstable">
/// 🚧 This code is considered unstable; it may change at any time, in breaking or non-breaking ways,
/// including in SemVer minor releases. In particular, the `DataProvider` implementations are only
/// guaranteed to match with this version's `*_unstable` providers. Use with caution.
/// </div>
///
/// # Examples
///
/// ```
/// use icu::plurals::provider::rules::reference::ast::*;
/// use icu::plurals::provider::rules::reference::{parse, parse_condition};
///
/// let condition =
///     parse_condition(b"i = 5 or v = 2").expect("Parsing failed.");
///
/// let samples = Samples {
///     integer: Some(SampleList {
///         sample_ranges: vec![SampleRange {
///             lower_val: DecimalValue("2".to_string()),
///             upper_val: None,
///         }],
///         ellipsis: true,
///     }),
///     decimal: Some(SampleList {
///         sample_ranges: vec![SampleRange {
///             lower_val: DecimalValue("2.5".to_string()),
///             upper_val: None,
///         }],
///         ellipsis: false,
///     }),
/// };
///
/// let rule = Rule {
///     condition,
///     samples: Some(samples),
/// };
///
/// assert_eq!(
///     rule,
///     parse("i = 5 or v = 2 @integer 2, … @decimal 2.5".as_bytes())
///         .expect("Parsing failed")
/// )
/// ```
///
/// [`AndConditions`]: AndCondition
#[derive(Debug, Clone, PartialEq)]
#[allow(clippy::exhaustive_structs)] // this type is stable
pub struct Rule {
    /// The set of conditions that each must be satisfied for the entire `Rule` to be satisfied
    pub condition: Condition,
    /// The set of sample numerical values matching each plural category that has a rule, or `None` if not present.
    pub samples: Option<Samples>,
}

/// A complete AST representation of a plural rule's condition. Comprises a vector of [`AndConditions`].
///
/// <div class="stab unstable">
/// 🚧 This code is considered unstable; it may change at any time, in breaking or non-breaking ways,
/// including in SemVer minor releases. In particular, the `DataProvider` implementations are only
/// guaranteed to match with this version's `*_unstable` providers. Use with caution.
/// </div>
///
/// # Examples
///
/// ```
/// use icu::plurals::provider::rules::reference::ast::*;
/// use icu::plurals::provider::rules::reference::parse_condition;
///
/// let condition = Condition(vec![
///     AndCondition(vec![Relation {
///         expression: Expression {
///             operand: Operand::I,
///             modulus: None,
///         },
///         operator: Operator::Eq,
///         range_list: RangeList(vec![RangeListItem::Value(Value(5))]),
///     }]),
///     AndCondition(vec![Relation {
///         expression: Expression {
///             operand: Operand::V,
///             modulus: None,
///         },
///         operator: Operator::Eq,
///         range_list: RangeList(vec![RangeListItem::Value(Value(2))]),
///     }]),
/// ]);
///
/// assert_eq!(
///     condition,
///     parse_condition(b"i = 5 or v = 2").expect("Parsing failed")
/// )
/// ```
///
/// [`AndConditions`]: AndCondition
#[derive(Debug, Clone, PartialEq)]
#[allow(clippy::exhaustive_structs)] // this type is stable
pub struct Condition(pub Vec<AndCondition>);

/// An incomplete AST representation of a plural rule. Comprises a vector of [`Relations`].
///
/// <div class="stab unstable">
/// 🚧 This code is considered unstable; it may change at any time, in breaking or non-breaking ways,
/// including in SemVer minor releases. In particular, the `DataProvider` implementations are only
/// guaranteed to match with this version's `*_unstable` providers. Use with caution.
/// </div>
///
/// # Examples
///
/// All AST nodes can be built explicitly, as seen in the example. However, due to its complexity, it is preferred to build the
/// AST using the [`parse()`](crate::provider::rules::reference::parser::parse()) function.
///
/// ```text
/// "i = 3 and v = 0"
/// ```
///
/// Can be represented by the AST:
///
/// ```
/// use icu::plurals::provider::rules::reference::ast::*;
///
/// AndCondition(vec![
///     Relation {
///         expression: Expression {
///             operand: Operand::I,
///             modulus: None,
///         },
///         operator: Operator::Eq,
///         range_list: RangeList(vec![RangeListItem::Value(Value(5))]),
///     },
///     Relation {
///         expression: Expression {
///             operand: Operand::V,
///             modulus: None,
///         },
///         operator: Operator::NotEq,
///         range_list: RangeList(vec![RangeListItem::Value(Value(2))]),
///     },
/// ]);
/// ```
///
/// [`Relations`]: Relation
#[derive(Debug, Clone, PartialEq)]
#[allow(clippy::exhaustive_structs)] // this type is stable
pub struct AndCondition(pub Vec<Relation>);

/// An incomplete AST representation of a plural rule. Comprises an [`Expression`], an [`Operator`], and a [`RangeList`].
///
/// <div class="stab unstable">
/// 🚧 This code is considered unstable; it may change at any time, in breaking or non-breaking ways,
/// including in SemVer minor releases. In particular, the `DataProvider` implementations are only
/// guaranteed to match with this version's `*_unstable` providers. Use with caution.
/// </div>
///
/// # Examples
///
/// All AST nodes can be built explicitly, as seen in the example. However, due to its complexity, it is preferred to build the
/// AST using the [`parse()`](crate::provider::rules::reference::parser::parse()) function.
///
/// ```text
/// "i = 3"
/// ```
///
/// Can be represented by the AST:
///
/// ```
/// use icu::plurals::provider::rules::reference::ast::*;
///
/// Relation {
///     expression: Expression {
///         operand: Operand::I,
///         modulus: None,
///     },
///     operator: Operator::Eq,
///     range_list: RangeList(vec![RangeListItem::Value(Value(3))]),
/// };
/// ```
#[derive(Debug, Clone, PartialEq)]
#[allow(clippy::exhaustive_structs)] // this type is stable
pub struct Relation {
    /// The plural operand variable that optionally includes an application of modulo arithmetic.
    pub expression: Expression,
    /// The operator (equals, not equals) indicating whether the resolved expression value
    /// at runtime should match the set of possible values in `range_list`. Note: `Operator::Eq`
    /// effectively means "is contained within the set of".
    pub operator: Operator,
    /// A sequence of `RangeListItem`, each of which represents a scalar number or a numerical range,
    /// that creates the interval set within which `expression`'s resolved value should exist.
    pub range_list: RangeList,
}

/// An enum of [`Relation`] operators for plural rules.
///
/// Each Operator enumeration belongs to the corresponding symbolic operators:
///
/// | Enum Operator | Symbolic Operator | Meaning                                        |
/// | --------------| ----------------- |------------------------------------------------|
/// | `Eq`          | "="               | is contained within the following interval set |
/// | `NotEq`       | "!="              | complement of `Eq` ("is _not_ contained..."")  |
///
/// <div class="stab unstable">
/// 🚧 This code is considered unstable; it may change at any time, in breaking or non-breaking ways,
/// including in SemVer minor releases. In particular, the `DataProvider` implementations are only
/// guaranteed to match with this version's `*_unstable` providers. Use with caution.
/// </div>
#[derive(Debug, Clone, Copy, PartialEq)]
#[allow(clippy::exhaustive_enums)] // this type is stable
pub enum Operator {
    /// In a plural rule [`Relation`], represents that the plural operand [`Expression`]'s value at
    /// should be contained within the [`RangeList`] interval set.
    Eq,
    /// The opposite of `Eq` -- that the plural operand [`Expression`]'s value at
    /// _should not be contained_ within the [`RangeList`] interval set.
    NotEq,
}

/// An incomplete AST representation of a plural rule. Comprises an [`Operand`] and an optional modulo.
///
/// <div class="stab unstable">
/// 🚧 This code is considered unstable; it may change at any time, in breaking or non-breaking ways,
/// including in SemVer minor releases. In particular, the `DataProvider` implementations are only
/// guaranteed to match with this version's `*_unstable` providers. Use with caution.
/// </div>
///
/// # Examples
///
/// All AST nodes can be built explicitly, as seen in the example. However, due to its complexity, it is preferred to build the
/// AST using the [`parse()`](crate::provider::rules::reference::parser::parse()) function.
///
/// ```text
/// "i % 100"
/// ```
///
/// Can be represented by the AST:
///
/// ```
/// use icu::plurals::provider::rules::reference::ast::*;
///
/// Expression {
///     operand: Operand::I,
///     modulus: Some(Value(100)),
/// };
/// ```
#[derive(Debug, Clone, PartialEq)]
#[allow(clippy::exhaustive_structs)] // this type is stable
pub struct Expression {
    /// The plural operand under test in this expression.
    pub operand: Operand,
    /// An optional modulo arithmetic base value when modulo arithmetic should be applied to the
    /// value of `operand`, otherwise `None`.
    pub modulus: Option<Value>,
}

/// An incomplete AST representation of a plural rule. Comprises a [`char`].
///
/// <div class="stab unstable">
/// 🚧 This code is considered unstable; it may change at any time, in breaking or non-breaking ways,
/// including in SemVer minor releases. In particular, the `DataProvider` implementations are only
/// guaranteed to match with this version's `*_unstable` providers. Use with caution.
/// </div>
///
/// # Examples
///
/// All AST nodes can be built explicitly, as seen in the example. However, due to its complexity, it is preferred to build the
/// AST using the [`parse()`](crate::provider::rules::reference::parser::parse()) function.
///
/// ```text
/// "i"
/// ```
///
/// Can be represented by the AST:
///
/// ```
/// use icu::plurals::provider::rules::reference::ast::Operand;
///
/// Operand::I;
/// ```
#[derive(Debug, Clone, Copy, PartialEq)]
#[allow(clippy::exhaustive_enums)] // this type is stable
pub enum Operand {
    /// Absolute value of input
    N,
    /// An integer value of input with the fraction part truncated off
    I,
    /// Number of visible fraction digits with trailing zeros
    V,
    /// Number of visible fraction digits without trailing zeros
    W,
    /// Visible fraction digits with trailing zeros
    F,
    /// Visible fraction digits without trailing zeros
    T,
    /// Compact decimal exponent value:
    ///   exponent of the power of 10 used in compact decimal formatting
    C,
    /// Currently, synonym for ‘c’. however, may be redefined in the future
    E,
}

/// An incomplete AST representation of a plural rule. Comprises a vector of [`RangeListItems`].
///
/// <div class="stab unstable">
/// 🚧 This code is considered unstable; it may change at any time, in breaking or non-breaking ways,
/// including in SemVer minor releases. In particular, the `DataProvider` implementations are only
/// guaranteed to match with this version's `*_unstable` providers. Use with caution.
/// </div>
///
/// # Examples
///
/// All AST nodes can be built explicitly, as seen in the example. However, due to its complexity, it is preferred to build the
/// AST using the [`parse()`](crate::provider::rules::reference::parser::parse()) function.
///
/// ```text
/// "5, 7, 9"
/// ```
///
/// Can be represented by the AST:
///
/// ```
/// use icu::plurals::provider::rules::reference::ast::*;
///
/// RangeList(vec![
///     RangeListItem::Value(Value(5)),
///     RangeListItem::Value(Value(7)),
///     RangeListItem::Value(Value(9)),
/// ]);
/// ```
///
/// [`RangeListItems`]: RangeListItem
#[derive(Debug, Clone, PartialEq)]
#[allow(clippy::exhaustive_structs)] // this type is stable
pub struct RangeList(pub Vec<RangeListItem>);

/// An enum of items that appear in a [`RangeList`]: `Range` or a `Value`.
///
/// See [`RangeInclusive`] and [`Value`] for additional details.
/// A range comprises two values: an inclusive lower and upper limit.
///
/// <div class="stab unstable">
/// 🚧 This code is considered unstable; it may change at any time, in breaking or non-breaking ways,
/// including in SemVer minor releases. In particular, the `DataProvider` implementations are only
/// guaranteed to match with this version's `*_unstable` providers. Use with caution.
/// </div>
///
/// # Examples
///
/// ```text
/// 5
/// 11..15
/// ```
///
/// Can be represented by the AST:
///
/// ```
/// use icu::plurals::provider::rules::reference::ast::*;
///
/// let _ = RangeListItem::Value(Value(5));
/// let _ = RangeListItem::Range(Value(11)..=Value(15));
/// ```
#[derive(Debug, Clone, PartialEq)]
#[allow(clippy::exhaustive_enums)] // this type is stable
pub enum RangeListItem {
    /// An interval of numerical values (inclusive of both interval boundaries).
    Range(RangeInclusive<Value>),
    /// A single scalar numerical value.
    Value(Value),
}

/// An incomplete AST representation of a plural rule, representing one integer.
///
/// <div class="stab unstable">
/// 🚧 This code is considered unstable; it may change at any time, in breaking or non-breaking ways,
/// including in SemVer minor releases. In particular, the `DataProvider` implementations are only
/// guaranteed to match with this version's `*_unstable` providers. Use with caution.
/// </div>
///
/// # Examples
///
/// All AST nodes can be built explicitly, as seen in the example. However, due to its complexity, it is preferred to build the
/// AST using the [`parse()`](crate::provider::rules::reference::parser::parse()) function.
///
/// ```text
/// "99"
/// ```
///
/// Can be represented by the AST:
///
/// ```
/// use icu::plurals::provider::rules::reference::ast::*;
///
/// RangeListItem::Value(Value(99));
/// ```
#[derive(Debug, Clone, PartialEq, PartialOrd)]
#[allow(clippy::exhaustive_structs)] // this type is stable
pub struct Value(pub u64);

/// A sample of example values that match the given rule.
///
/// <div class="stab unstable">
/// 🚧 This code is considered unstable; it may change at any time, in breaking or non-breaking ways,
/// including in SemVer minor releases. In particular, the `DataProvider` implementations are only
/// guaranteed to match with this version's `*_unstable` providers. Use with caution.
/// </div>
///
/// # Examples
///
/// ```text
/// @integer 2, … @decimal 2.5
/// ```
///
/// ```
/// use icu::plurals::provider::rules::reference::ast::*;
/// Samples {
///     integer: Some(SampleList {
///         sample_ranges: vec![SampleRange {
///             lower_val: DecimalValue("2".to_string()),
///             upper_val: None,
///         }],
///         ellipsis: true,
///     }),
///     decimal: Some(SampleList {
///         sample_ranges: vec![SampleRange {
///             lower_val: DecimalValue("2.5".to_string()),
///             upper_val: None,
///         }],
///         ellipsis: false,
///     }),
/// };
/// ```
#[derive(Debug, Clone, PartialEq)]
#[allow(clippy::exhaustive_structs)] // this type is stable
pub struct Samples {
    /// The list of integer samples provided (denoted
    /// [in LDML by `@integer`](https://unicode.org/reports/tr35/tr35-numbers.html#Samples)).
    pub integer: Option<SampleList>,
    /// The list of samples with decimal fractions provided (denoted
    /// [in LDML by `@decimal`](https://unicode.org/reports/tr35/tr35-numbers.html#Samples)).
    pub decimal: Option<SampleList>,
}

/// A list of values used in samples.
///
/// <div class="stab unstable">
/// 🚧 This code is considered unstable; it may change at any time, in breaking or non-breaking ways,
/// including in SemVer minor releases. In particular, the `DataProvider` implementations are only
/// guaranteed to match with this version's `*_unstable` providers. Use with caution.
/// </div>
///
/// # Examples
///
/// ```text
/// 0.0~1.5, …
/// ```
///
/// ```
/// use icu::plurals::provider::rules::reference::ast::*;
/// SampleList {
///     sample_ranges: vec![SampleRange {
///         lower_val: DecimalValue("0.0".to_string()),
///         upper_val: Some(DecimalValue("1.5".to_string())),
///     }],
///     ellipsis: true,
/// };
/// ```
#[derive(Debug, Clone, PartialEq)]
#[allow(clippy::exhaustive_structs)] // this type is stable
pub struct SampleList {
    /// A collection of intervals in which all of the contained values (inclusive of the
    /// interval boundaries) satisfy the associated rule.
    pub sample_ranges: Vec<SampleRange>,
    /// Indicates the presence of U+2026 HORIZONTAL ELLIPSIS at the end of sample string, which
    /// represents whether an infinite set of values satisfies the rule or not.
    pub ellipsis: bool,
}

/// A value range used in samples.
///
/// <div class="stab unstable">
/// 🚧 This code is considered unstable; it may change at any time, in breaking or non-breaking ways,
/// including in SemVer minor releases. In particular, the `DataProvider` implementations are only
/// guaranteed to match with this version's `*_unstable` providers. Use with caution.
/// </div>
///
/// # Examples
///
/// ```text
/// 0.0~1.5
/// ```
///
/// ```
/// use icu::plurals::provider::rules::reference::ast::*;
/// SampleRange {
///     lower_val: DecimalValue("0.0".to_string()),
///     upper_val: Some(DecimalValue("1.5".to_string())),
/// };
/// ```
#[derive(Debug, Clone, PartialEq)]
#[allow(clippy::exhaustive_structs)] // this type is stable
pub struct SampleRange {
    /// When `upper_val` is `None`, this field represents a single sample value that satisfies
    /// the associated plural rule. When `upper_val` is `Some`, this field represents the lower
    /// bound of an interval (and is included in the interval) whose values all satisfy the rule.
    pub lower_val: DecimalValue,
    /// When this `SampleRange` represents an interval of values, this field represents the upper
    /// bound of the interval (and is included in the interval). Otherwise, this field is `None`.
    pub upper_val: Option<DecimalValue>,
}

/// A decimal value used in samples.
///
/// <div class="stab unstable">
/// 🚧 This code is considered unstable; it may change at any time, in breaking or non-breaking ways,
/// including in SemVer minor releases. In particular, the `DataProvider` implementations are only
/// guaranteed to match with this version's `*_unstable` providers. Use with caution.
/// </div>
///
/// # Examples
///
/// ```text
/// 1.00
/// ```
///
/// ```
/// use icu::plurals::provider::rules::reference::ast::*;
/// DecimalValue("1.00".to_string());
/// ```
#[derive(Debug, Clone, PartialEq)]
#[allow(clippy::exhaustive_structs)] // this type is stable
pub struct DecimalValue(pub String);
