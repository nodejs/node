// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

use crate::operands::PluralOperands;
use crate::provider::rules::runtime::ast;

/// <div class="stab unstable">
/// 🚧 This code is considered unstable; it may change at any time, in breaking or non-breaking ways,
/// including in SemVer minor releases. In particular, the `DataProvider` implementations are only
/// guaranteed to match with this version's `*_unstable` providers. Use with caution.
/// </div>
#[inline]
pub fn test_rule(rule: &ast::Rule, operands: &PluralOperands) -> bool {
    // This algorithm is a simple non-recursive interpreter of the
    // [UTS #35: Language Plural Rules].
    //
    // The algorithm exploits the fact that plural rules syntax is a simple
    // logical operator expression composition with maximum depth of one
    // level of `OR` expression.
    //
    // That means that any `AND` expression accumulates to a single boolean
    // result which either results in a test passing, or the next set
    // of `AND` relations is evaluated after the `OR`.
    //
    // To achieve that, the algorithm traverses the relations from left to right
    // collecting all matching relations into a temporary `left` variable for
    // as long as they are followed by the `AND` operator.
    //
    // If any relation fails to match, the `left` variable becomes `false` and the
    // interpreter skips to the first `OR` operator, rejects the left side, and
    // evaluates the right as a candidate and so on.
    //
    // [UTS #35: Language Plural Rules]: https://unicode.org/reports/tr35/tr35-numbers.html#Language_Plural_Rules

    let mut left = true;

    for relation in rule.0.iter() {
        let relation = relation.as_relation();
        if left && relation.aopo.and_or == ast::AndOr::Or {
            return true;
        }
        if left || relation.aopo.and_or == ast::AndOr::Or {
            left = test_relation(&relation, operands);
        }
    }
    left
}

#[inline]
fn test_relation(relation: &ast::Relation, operands: &PluralOperands) -> bool {
    let result = if let Some(value) = get_value(relation, operands) {
        relation.range_list.iter().any(|range| match range {
            ast::RangeOrValue::Value(v) => u64::from(v) == value,
            ast::RangeOrValue::Range(min, max) => {
                value >= u64::from(min) && value <= u64::from(max)
            }
        })
    } else {
        false
    };
    match relation.aopo.polarity {
        ast::Polarity::Negative => !result,
        ast::Polarity::Positive => result,
    }
}

#[inline]
fn get_value(relation: &ast::Relation, operands: &PluralOperands) -> Option<u64> {
    let value = match relation.aopo.operand {
        ast::Operand::N if operands.w == 0 => operands.i,
        ast::Operand::N => return None,
        ast::Operand::I => operands.i,
        ast::Operand::F => operands.f,
        ast::Operand::V => operands.v as u64,
        ast::Operand::W => operands.w as u64,
        ast::Operand::T => operands.t,
        ast::Operand::C | ast::Operand::E => operands.c as u64,
    };
    if relation.modulo > 0 {
        value.checked_rem_euclid(relation.modulo.into())
    } else {
        Some(value)
    }
}
