// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

use crate::provider::rules::reference::ast;
use core::fmt;
use core::ops::RangeInclusive;

/// Unicode Plural Rule serializer converts an [`AST`] to a [`String`].
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
/// use icu::plurals::provider::rules::reference::ast;
/// use icu::plurals::provider::rules::reference::parse;
/// use icu::plurals::provider::rules::reference::serialize;
///
/// let input = "i = 0 or n = 1 @integer 0, 1 @decimal 0.0~1.0, 0.00~0.04";
///
/// let ast = parse(input.as_bytes()).expect("Parsing failed.");
///
/// assert_eq!(ast.condition.0[0].0[0].expression.operand, ast::Operand::I);
/// assert_eq!(ast.condition.0[1].0[0].expression.operand, ast::Operand::N);
///
/// let mut result = String::new();
/// serialize(&ast, &mut result).expect("Serialization failed.");
///
/// assert_eq!(input, result);
/// ```
///
/// [`AST`]: crate::provider::rules::reference::ast
pub fn serialize(rule: &ast::Rule, w: &mut impl fmt::Write) -> fmt::Result {
    serialize_condition(&rule.condition, w)?;
    if let Some(samples) = &rule.samples {
        serialize_samples(samples, w)?;
    }
    Ok(())
}

/// <div class="stab unstable">
/// 🚧 This code is considered unstable; it may change at any time, in breaking or non-breaking ways,
/// including in SemVer minor releases. In particular, the `DataProvider` implementations are only
/// guaranteed to match with this version's `*_unstable` providers. Use with caution.
/// </div>
pub fn serialize_condition(cond: &ast::Condition, w: &mut impl fmt::Write) -> fmt::Result {
    let mut first = true;

    for cond in cond.0.iter() {
        if first {
            first = false;
        } else {
            w.write_str(" or ")?;
        }
        serialize_andcondition(cond, w)?;
    }
    Ok(())
}

fn serialize_andcondition(cond: &ast::AndCondition, w: &mut impl fmt::Write) -> fmt::Result {
    let mut first = true;

    for relation in cond.0.iter() {
        if first {
            first = false;
        } else {
            w.write_str(" and ")?;
        }
        serialize_relation(relation, w)?;
    }
    Ok(())
}

fn serialize_relation(relation: &ast::Relation, w: &mut impl fmt::Write) -> fmt::Result {
    serialize_expression(&relation.expression, w)?;
    w.write_char(' ')?;
    serialize_operator(relation.operator, w)?;
    w.write_char(' ')?;
    serialize_rangelist(&relation.range_list, w)
}

fn serialize_expression(exp: &ast::Expression, w: &mut impl fmt::Write) -> fmt::Result {
    serialize_operand(exp.operand, w)?;
    if let Some(modulus) = &exp.modulus {
        w.write_str(" % ")?;
        serialize_value(modulus, w)?;
    }
    Ok(())
}

fn serialize_operator(operator: ast::Operator, w: &mut impl fmt::Write) -> fmt::Result {
    match operator {
        ast::Operator::Eq => w.write_char('='),
        ast::Operator::NotEq => w.write_str("!="),
    }
}

fn serialize_operand(operand: ast::Operand, w: &mut impl fmt::Write) -> fmt::Result {
    match operand {
        ast::Operand::N => w.write_char('n'),
        ast::Operand::I => w.write_char('i'),
        ast::Operand::V => w.write_char('v'),
        ast::Operand::W => w.write_char('w'),
        ast::Operand::F => w.write_char('f'),
        ast::Operand::T => w.write_char('t'),
        ast::Operand::C => w.write_char('c'),
        ast::Operand::E => w.write_char('e'),
    }
}

fn serialize_rangelist(rl: &ast::RangeList, w: &mut impl fmt::Write) -> fmt::Result {
    let mut first = true;

    for rli in rl.0.iter() {
        if first {
            first = false;
        } else {
            w.write_str(", ")?;
        }
        serialize_rangelistitem(rli, w)?
    }
    Ok(())
}

fn serialize_rangelistitem(rli: &ast::RangeListItem, w: &mut impl fmt::Write) -> fmt::Result {
    match rli {
        ast::RangeListItem::Range(range) => serialize_range(range, w),
        ast::RangeListItem::Value(v) => serialize_value(v, w),
    }
}

fn serialize_range(range: &RangeInclusive<ast::Value>, w: &mut impl fmt::Write) -> fmt::Result {
    serialize_value(range.start(), w)?;
    w.write_str("..")?;
    serialize_value(range.end(), w)?;
    Ok(())
}

fn serialize_value(value: &ast::Value, w: &mut impl fmt::Write) -> fmt::Result {
    write!(w, "{}", value.0)
}

/// <div class="stab unstable">
/// 🚧 This code is considered unstable; it may change at any time, in breaking or non-breaking ways,
/// including in SemVer minor releases. In particular, the `DataProvider` implementations are only
/// guaranteed to match with this version's `*_unstable` providers. Use with caution.
/// </div>
pub fn serialize_samples(samples: &ast::Samples, w: &mut impl fmt::Write) -> fmt::Result {
    if let Some(sample_list) = &samples.integer {
        w.write_str(" @integer ")?;
        serialize_sample_list(sample_list, w)?;
    }
    if let Some(sample_list) = &samples.decimal {
        w.write_str(" @decimal ")?;
        serialize_sample_list(sample_list, w)?;
    }
    Ok(())
}

/// <div class="stab unstable">
/// 🚧 This code is considered unstable; it may change at any time, in breaking or non-breaking ways,
/// including in SemVer minor releases. In particular, the `DataProvider` implementations are only
/// guaranteed to match with this version's `*_unstable` providers. Use with caution.
/// </div>
pub fn serialize_sample_list(samples: &ast::SampleList, w: &mut impl fmt::Write) -> fmt::Result {
    let mut first = true;

    for sample_range in samples.sample_ranges.iter() {
        if first {
            first = false;
        } else {
            w.write_str(", ")?;
        }
        serialize_sample_range(sample_range, w)?;
    }

    if samples.ellipsis {
        w.write_str(", …")?;
    }
    Ok(())
}

/// <div class="stab unstable">
/// 🚧 This code is considered unstable; it may change at any time, in breaking or non-breaking ways,
/// including in SemVer minor releases. In particular, the `DataProvider` implementations are only
/// guaranteed to match with this version's `*_unstable` providers. Use with caution.
/// </div>
pub fn serialize_sample_range(
    sample_range: &ast::SampleRange,
    w: &mut impl fmt::Write,
) -> fmt::Result {
    serialize_decimal_value(&sample_range.lower_val, w)?;
    if let Some(upper_val) = &sample_range.upper_val {
        w.write_char('~')?;
        serialize_decimal_value(upper_val, w)?;
    }
    Ok(())
}

/// <div class="stab unstable">
/// 🚧 This code is considered unstable; it may change at any time, in breaking or non-breaking ways,
/// including in SemVer minor releases. In particular, the `DataProvider` implementations are only
/// guaranteed to match with this version's `*_unstable` providers. Use with caution.
/// </div>
pub fn serialize_decimal_value(val: &ast::DecimalValue, w: &mut impl fmt::Write) -> fmt::Result {
    w.write_str(&val.0)
}
