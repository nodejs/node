// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

//! 🚧 \[Unstable\] APIs and Data Structures for Plural Rules
//!
//! A single Plural Rule is an expression which tests the value of [`PluralOperands`]
//! against a condition. If the condition is truthful, then the [`PluralCategory`]
//! to which the Rule is assigned should be used.
//!
//! <div class="stab unstable">
//! 🚧 This code is considered unstable; it may change at any time, in breaking or non-breaking ways,
//! including in SemVer minor releases. In particular, the `DataProvider` implementations are only
//! guaranteed to match with this version's `*_unstable` providers. Use with caution.
//! </div>
//!
//! # Examples
//!
//! In this example we're going to examine the AST, parsing and resolving of a
//! set of English Cardinal Plural Rules.
//!
//! A CLDR JSON source contains the following entry:
//!
//! ```json
//! {
//!   "one": "i = 1 and v = 0 @integer 1",
//!   "other": " @integer 0, 2~16, 100, 1000, 10000, 100000, 1000000, … @decimal 0.0~1.5, 10.0, 100.0, 1000.0, 10000.0, 100000.0, 1000000.0, …"
//! }
//! ```
//!
//! When the user provides a number for which the [`PluralCategory`] is to be selected,
//! the system will examine a rule for each category in order, and stop on the first
//! category which matches.
//!
//! In our example, the user provided an input value `1`.
//! That value expanded into [`PluralOperands`] might look something like this, in its
//! internal representation of plural operand values, or something logically equivalent:
//!
//! ```text
//! PluralOperands {
//!     i: 1,
//!     v: 0,
//!     w: 0,
//!     f: 0,
//!     t: 0,
//!     c: 0,
//! };
//! ```
//!
//! Now, the system will parse the first rule, assigned to [`PluralCategory::One`], and
//! test if it matches.
//!
//! The value of the rule is:
//!
//! ```text
//! i = 1 and v = 0 @integer 1
//! ```
//!
//! The [`Rule`] contains a [`Condition`] `i = 1 and v = 0` and a [`Sample`] `@integer 1`.
//!
//! When parsed, the resulting [`AST`] will look like this:
//!
//! ```
//! use icu::plurals::provider::rules::reference::ast::*;
//! use icu::plurals::provider::rules::reference::parse_condition;
//!
//! let input = "i = 1 and v = 0 @integer 1";
//!
//! let ast = parse_condition(input.as_bytes()).expect("Parsing failed.");
//! assert_eq!(
//!     ast,
//!     Condition(vec![AndCondition(vec![
//!         Relation {
//!             expression: Expression {
//!                 operand: Operand::I,
//!                 modulus: None,
//!             },
//!             operator: Operator::Eq,
//!             range_list: RangeList(vec![RangeListItem::Value(Value(1))])
//!         },
//!         Relation {
//!             expression: Expression {
//!                 operand: Operand::V,
//!                 modulus: None,
//!             },
//!             operator: Operator::Eq,
//!             range_list: RangeList(vec![RangeListItem::Value(Value(0))])
//!         },
//!     ]),])
//! );
//! ```
//!
//! Finally, we can pass this [`AST`] (in fact, just the [`Condition`] node),
//! to a resolver alongside the [`PluralOperands`] to test if the Rule
//! matches:
//!
//! ```
//! use icu::plurals::provider::rules::reference::parse_condition;
//! use icu::plurals::provider::rules::reference::test_condition;
//! use icu::plurals::PluralOperands;
//!
//! let input = "i = 1 and v = 0 @integer 1";
//!
//! let operands = PluralOperands::from(1_u32);
//!
//! let ast = parse_condition(input.as_bytes()).expect("Parsing failed.");
//!
//! assert!(test_condition(&ast, &operands));
//! ```
//!
//! Since the rule for [`PluralCategory::One`] matches, we will return this category.
//! Otherwise, we'd test the next rule, in this case [`PluralCategory::Other`], which has an
//! empty [`Condition`], meaning that it'll match all operands.
//!
//! # Summary
//!
//! For [`PluralRuleType::Cardinal`] in English, we can restate the rule's logic as:
//!
//! When the `PluralOperands::i` is `1` and `PluralOperands::v` is `0` (or equivalent thereof), [`PluralCategory::One`]
//! should be used, otherwise [`PluralCategory::Other`] should be used.
//!
//! For other locales, there are different/more [`PluralCategories`] defined in the `PluralRules` (see [`PluralRules::categories`]),
//! and possibly more complicated [`Rules`] therein.
//!
//! # Difference between Category and Number
//!
//! While in English [`PluralCategory::One`] overlaps with an integer value `1`,
//! in other languages, the category may be used for other numbers as well.
//!
//! For example, in Russian [`PluralCategory::One`] matches numbers such as `11`, `21`, `121` etc.
//!
//! # Runtime vs. Resolver Rules
//!
//! `ICU4X` provides two sets of rules AST and APIs to manage it:
//!  * `reference` is the canonical implementation of the specification intended for
//!    tooling and testing to use.
//!    This module provides APIs for parsing, editing and serialization of the rules.
//!  * `runtime` is a non-public, non-mutable runtime version optimized for
//!    performance and low memory overhead. This version provides
//!    runtime resolver used by the `PluralRules` itself.
//!
//! [`PluralCategory`]: super::PluralCategory
//! [`PluralCategories`]: super::PluralCategory
//! [`PluralCategory::One`]: super::PluralCategory::One
//! [`PluralCategory::Other`]: super::PluralCategory::Other
//! [`PluralOperands`]: super::PluralOperands
//! [`PluralRules::categories`]: super::PluralRules::categories
//! [`PluralRuleType::Cardinal`]: crate::PluralRuleType::Cardinal
//! [`Rule`]: super::rules::reference::ast::Rule
//! [`Rules`]: super::rules::reference::ast::Rule
//! [`Condition`]: super::rules::reference::ast::Condition
//! [`Sample`]: super::rules::reference::ast::Samples
//! [`AST`]: super::rules::reference::ast

pub mod runtime;

// The reference module is used internally, but is only needed externally in datagen mode
pub mod reference;
