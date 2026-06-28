// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

use crate::operands::PluralOperands;
use crate::provider::rules::reference::ast;

/// Function used to test [`Condition`] against [`PluralOperands`] to identify
/// the appropriate [`PluralCategory`].
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
/// use icu::plurals::provider::rules::reference::parse_condition;
/// use icu::plurals::provider::rules::reference::test_condition;
/// use icu::plurals::PluralOperands;
///
/// let operands = PluralOperands::from(5_usize);
/// let condition =
///     parse_condition(b"i = 4..6").expect("Failed to parse a rule.");
///
/// assert!(test_condition(&condition, &operands));
/// ```
///
/// [`PluralCategory`]: crate::PluralCategory
/// [`PluralOperands`]: crate::PluralOperands
/// [`Condition`]: super::ast::Condition
pub fn test_condition(condition: &ast::Condition, operands: &PluralOperands) -> bool {
    condition.0.is_empty() || condition.0.iter().any(|c| test_and_condition(c, operands))
}

fn test_and_condition(condition: &ast::AndCondition, operands: &PluralOperands) -> bool {
    condition.0.iter().all(|r| test_relation(r, operands))
}

fn test_relation(relation: &ast::Relation, operands: &PluralOperands) -> bool {
    calculate_expression(&relation.expression, operands)
        .is_some_and(|exp| test_range(&relation.range_list, exp, relation.operator))
}

// UTS 35 Part 2 Section 5.1 specifies that CLDR rules contain only integer values.
// See: https://unicode.org/reports/tr35/tr35-numbers.html#Plural_rules_syntax
//
// If a rule is asking for `n` it will compare it to a list of integers,
// and expect it to not match if there is any fractional part.
//
// That allows us to play a trick - we already have an integer portion of the number as operand `i`.
//
// In result, if we are asked to calculate an operand `n` and it contains a fractional part,
// we know that it will not match the value, which must be an integer without a fractional part.
//
// If that happens, we'll return `None`, and the matching will return `false`.
fn calculate_expression(expression: &ast::Expression, operands: &PluralOperands) -> Option<u64> {
    let value = match expression.operand {
        ast::Operand::N => {
            if operands.w == 0 {
                operands.i
            } else {
                return None;
            }
        }
        ast::Operand::I => operands.i,
        ast::Operand::F => operands.f,
        ast::Operand::V => operands.v as u64,
        ast::Operand::W => operands.w as u64,
        ast::Operand::T => operands.t,
        ast::Operand::C | ast::Operand::E => operands.c as u64,
    };
    if let Some(modulus) = &expression.modulus {
        value.checked_rem_euclid(modulus.0)
    } else {
        Some(value)
    }
}

fn test_range(range: &ast::RangeList, value: u64, operator: ast::Operator) -> bool {
    let connect = match operator {
        ast::Operator::Eq => Iterator::any,
        ast::Operator::NotEq => Iterator::all,
    };
    connect(&mut range.0.iter(), |item| {
        test_range_item(item, value, operator)
    })
}

fn test_range_item(item: &ast::RangeListItem, value: u64, operator: ast::Operator) -> bool {
    let value = match item {
        ast::RangeListItem::Value(n) => n.0 == value,
        ast::RangeListItem::Range(range) => range.contains(&ast::Value(value)),
    };
    match operator {
        ast::Operator::Eq => value,
        ast::Operator::NotEq => !value,
    }
}
