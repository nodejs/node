// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

use crate::provider::rules::reference;
use alloc::vec::Vec;
use core::{
    convert::{TryFrom, TryInto},
    fmt, num,
};
use icu_provider::prelude::*;
use zerovec::{
    ule::{tuple::Tuple2ULE, AsULE, UleError, ULE},
    {VarZeroVec, ZeroVec},
};

/// <div class="stab unstable">
/// 🚧 This code is considered unstable; it may change at any time, in breaking or non-breaking ways,
/// including in SemVer minor releases. In particular, the `DataProvider` implementations are only
/// guaranteed to match with this version's `*_unstable` providers. Use with caution.
/// </div>
#[derive(yoke::Yokeable, zerofrom::ZeroFrom, Clone, PartialEq, Debug)]
#[cfg_attr(feature = "datagen", derive(databake::Bake))]
#[cfg_attr(feature = "datagen", databake(path = icu_plurals::provider::rules::runtime::ast))]
#[allow(clippy::exhaustive_structs)] // Reference AST is non-public and this type is stable
pub struct Rule<'data>(pub VarZeroVec<'data, RelationULE>);

#[derive(Copy, Clone, Eq, PartialEq, Ord, PartialOrd, Debug)]
#[repr(u8)]
pub(crate) enum AndOr {
    Or,
    And,
}

#[derive(Copy, Clone, Eq, PartialEq, Ord, PartialOrd, Debug)]
#[repr(u8)]
pub(crate) enum Polarity {
    Negative,
    Positive,
}

#[derive(Copy, Clone, Debug, PartialEq, Eq, Ord, PartialOrd)]
#[repr(u8)]
#[zerovec::make_ule(OperandULE)]
pub(crate) enum Operand {
    N = 0,
    I = 1,
    V = 2,
    W = 3,
    F = 4,
    T = 5,
    C = 6,
    E = 7,
}

#[derive(Clone, Copy, Debug, PartialEq, Eq, Ord, PartialOrd)]
pub(crate) enum RangeOrValue {
    Range(u32, u32),
    Value(u32),
}

/// Represent a a single "relation" in a plural rule
#[derive(Clone, Debug, PartialEq, Eq, Ord, PartialOrd)]
#[zerovec::make_varule(RelationULE)]
pub struct Relation<'data> {
    pub(crate) aopo: AndOrPolarityOperand,
    pub(crate) modulo: u32,
    pub(crate) range_list: ZeroVec<'data, RangeOrValue>,
}

#[derive(Copy, Clone, Debug, PartialEq, Eq, Ord, PartialOrd)]
pub(crate) struct AndOrPolarityOperand {
    pub(crate) and_or: AndOr,
    pub(crate) polarity: Polarity,
    pub(crate) operand: Operand,
}

impl fmt::Debug for RelationULE {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        self.as_relation().fmt(f)
    }
}
/////

impl TryFrom<&reference::ast::Rule> for Rule<'_> {
    type Error = num::TryFromIntError;

    fn try_from(input: &reference::ast::Rule) -> Result<Self, Self::Error> {
        let mut relations: Vec<Relation> = alloc::vec![];

        for (i_or, and_condition) in input.condition.0.iter().enumerate() {
            for (i_and, relation) in and_condition.0.iter().enumerate() {
                let range_list = relation
                    .range_list
                    .0
                    .iter()
                    .map(|rov| rov.try_into())
                    .collect::<Result<Vec<_>, _>>()?;

                let and_or = if i_or > 0 && i_and == 0 {
                    AndOr::Or
                } else {
                    AndOr::And
                };

                let aopo = AndOrPolarityOperand {
                    and_or,
                    polarity: relation.operator.into(),
                    operand: relation.expression.operand.into(),
                };

                relations.push(Relation {
                    aopo,
                    modulo: get_modulo(&relation.expression.modulus)?,
                    range_list: ZeroVec::alloc_from_slice(&range_list),
                })
            }
        }

        Ok(Self(VarZeroVec::from(relations.as_slice())))
    }
}

impl From<&Rule<'_>> for reference::ast::Rule {
    fn from(input: &Rule<'_>) -> Self {
        let mut or_conditions = alloc::vec![];
        let mut and_conditions = alloc::vec![];
        for rel in input.0.iter() {
            let rel = rel.as_relation();
            let list = rel.range_list.iter().map(Into::into).collect();
            let relation = reference::ast::Relation {
                expression: (rel.aopo.operand, rel.modulo).into(),
                operator: rel.aopo.polarity.into(),
                range_list: reference::ast::RangeList(list),
            };

            if rel.aopo.and_or == AndOr::And {
                and_conditions.push(relation);
            } else {
                or_conditions.push(reference::ast::AndCondition(and_conditions));
                and_conditions = alloc::vec![relation];
            }
        }

        if !and_conditions.is_empty() {
            or_conditions.push(reference::ast::AndCondition(and_conditions));
        }

        Self {
            condition: reference::ast::Condition(or_conditions),
            samples: None,
        }
    }
}

impl From<reference::ast::Operator> for Polarity {
    fn from(op: reference::ast::Operator) -> Self {
        match op {
            reference::ast::Operator::Eq => Polarity::Positive,
            reference::ast::Operator::NotEq => Polarity::Negative,
        }
    }
}

impl From<Polarity> for reference::ast::Operator {
    fn from(pol: Polarity) -> Self {
        match pol {
            Polarity::Negative => reference::ast::Operator::NotEq,
            Polarity::Positive => reference::ast::Operator::Eq,
        }
    }
}

impl From<reference::ast::Operand> for Operand {
    fn from(op: reference::ast::Operand) -> Self {
        match op {
            reference::ast::Operand::N => Self::N,
            reference::ast::Operand::I => Self::I,
            reference::ast::Operand::V => Self::V,
            reference::ast::Operand::W => Self::W,
            reference::ast::Operand::F => Self::F,
            reference::ast::Operand::T => Self::T,
            reference::ast::Operand::C => Self::C,
            reference::ast::Operand::E => Self::E,
        }
    }
}

impl From<Operand> for reference::ast::Operand {
    fn from(op: Operand) -> Self {
        match op {
            Operand::N => Self::N,
            Operand::I => Self::I,
            Operand::V => Self::V,
            Operand::W => Self::W,
            Operand::F => Self::F,
            Operand::T => Self::T,
            Operand::C => Self::C,
            Operand::E => Self::E,
        }
    }
}

impl From<(Operand, u32)> for reference::ast::Expression {
    fn from(input: (Operand, u32)) -> Self {
        Self {
            operand: input.0.into(),
            modulus: get_modulus(input.1),
        }
    }
}

fn get_modulo(op: &Option<reference::ast::Value>) -> Result<u32, num::TryFromIntError> {
    if let Some(op) = op {
        u32::try_from(op)
    } else {
        Ok(0)
    }
}

fn get_modulus(input: u32) -> Option<reference::ast::Value> {
    if input == 0 {
        None
    } else {
        Some(input.into())
    }
}

impl TryFrom<&reference::ast::Value> for u32 {
    type Error = num::TryFromIntError;

    fn try_from(v: &reference::ast::Value) -> Result<Self, Self::Error> {
        v.0.try_into()
    }
}

impl From<u32> for reference::ast::Value {
    fn from(input: u32) -> Self {
        Self(input.into())
    }
}

impl TryFrom<&reference::ast::RangeListItem> for RangeOrValue {
    type Error = num::TryFromIntError;

    fn try_from(item: &reference::ast::RangeListItem) -> Result<Self, Self::Error> {
        Ok(match item {
            reference::ast::RangeListItem::Range(range) => {
                RangeOrValue::Range(range.start().try_into()?, range.end().try_into()?)
            }
            reference::ast::RangeListItem::Value(value) => RangeOrValue::Value(value.try_into()?),
        })
    }
}

impl From<RangeOrValue> for reference::ast::RangeListItem {
    fn from(item: RangeOrValue) -> Self {
        match item {
            RangeOrValue::Range(min, max) => Self::Range(min.into()..=max.into()),
            RangeOrValue::Value(value) => Self::Value(value.into()),
        }
    }
}

#[cfg(feature = "datagen")]
impl core::str::FromStr for Rule<'_> {
    type Err = reference::parser::ParseError;

    fn from_str(s: &str) -> Result<Self, Self::Err> {
        let rule = reference::parser::parse(s.as_bytes())?;
        Rule::try_from(&rule).map_err(|_| reference::parser::ParseError::ValueTooLarge)
    }
}

impl fmt::Display for Rule<'_> {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        let reference = reference::ast::Rule::from(self);
        reference::serialize(&reference, f)
    }
}

impl RelationULE {
    /// Convert to a Relation
    #[inline]
    pub fn as_relation(&self) -> Relation<'_> {
        zerofrom::ZeroFrom::zero_from(self)
    }
}

#[derive(Copy, Clone, Hash, PartialEq, Eq, Debug)]
#[repr(transparent)]
pub(crate) struct AndOrPolarityOperandULE(u8);

// Safety (based on the safety checklist on the ULE trait):
//  1. [`AndOrPolarityOperandULE`] does not include any uninitialized or padding bytes
//     (achieved by `#[repr(transparent)]` on a type that satisfies this invariant)
/// 2. [`AndOrPolarityOperandULE`] is aligned to 1 byte
//     (achieved by `#[repr(transparent)]` on a type that satisfies this invariant)
//  3. The impl of `validate_bytes()` returns an error if any byte is not valid.
//  4. The impl of `validate_bytes()` returns an error if there are extra bytes
//     (impossible since it is of size 1 byte)
//  5 The other ULE methods use the default impl.
//  6. [`AndOrPolarityOperandULE`] byte equality is semantic equality.
unsafe impl ULE for AndOrPolarityOperandULE {
    fn validate_bytes(bytes: &[u8]) -> Result<(), UleError> {
        for byte in bytes {
            Operand::new_from_u8(byte & 0b0011_1111).ok_or_else(UleError::parse::<Self>)?;
        }
        Ok(())
    }
}

impl AsULE for AndOrPolarityOperand {
    type ULE = AndOrPolarityOperandULE;
    fn to_unaligned(self) -> AndOrPolarityOperandULE {
        let encoded_operand = self.operand.to_unaligned().0;
        debug_assert!(encoded_operand <= 0b0011_1111);
        AndOrPolarityOperandULE(
            (((self.and_or == AndOr::And) as u8) << 7)
                + (((self.polarity == Polarity::Positive) as u8) << 6)
                + encoded_operand,
        )
    }

    fn from_unaligned(other: AndOrPolarityOperandULE) -> Self {
        let encoded = other.0;
        let and_or = if encoded & 0b1000_0000 != 0 {
            AndOr::And
        } else {
            AndOr::Or
        };

        let polarity = if encoded & 0b0100_0000 != 0 {
            Polarity::Positive
        } else {
            Polarity::Negative
        };

        // note that this is unsafe, since OperandULE has its own
        // safety requirements
        // we can guarantee safety here since these bits can only come
        // from validated OperandULEs
        let operand = OperandULE(encoded & 0b0011_1111);
        Self {
            and_or,
            polarity,
            operand: Operand::from_unaligned(operand),
        }
    }
}

type RangeOrValueULE = Tuple2ULE<<u32 as AsULE>::ULE, <u32 as AsULE>::ULE>;

impl AsULE for RangeOrValue {
    type ULE = RangeOrValueULE;

    #[inline]
    fn to_unaligned(self) -> Self::ULE {
        match self {
            Self::Range(start, end) => Tuple2ULE(start.to_unaligned(), end.to_unaligned()),
            Self::Value(idx) => Tuple2ULE(idx.to_unaligned(), idx.to_unaligned()),
        }
    }

    #[inline]
    fn from_unaligned(unaligned: Self::ULE) -> Self {
        let start = u32::from_unaligned(unaligned.0);
        let end = u32::from_unaligned(unaligned.1);
        if start == end {
            Self::Value(start)
        } else {
            Self::Range(start, end)
        }
    }
}

#[cfg(feature = "serde")]
mod serde {
    use super::*;
    use ::serde::{de, ser, Deserialize, Deserializer, Serialize};
    use alloc::{
        format,
        string::{String, ToString},
    };

    impl Serialize for Rule<'_> {
        fn serialize<S>(&self, serializer: S) -> Result<S::Ok, S::Error>
        where
            S: ser::Serializer,
        {
            if serializer.is_human_readable() {
                let string: String = self.to_string();
                serializer.serialize_str(&string)
            } else {
                serializer.serialize_bytes(self.0.as_bytes())
            }
        }
    }

    struct DeserializeRule;

    impl<'de> de::Visitor<'de> for DeserializeRule {
        type Value = Rule<'de>;

        fn expecting(&self, formatter: &mut fmt::Formatter) -> fmt::Result {
            write!(formatter, "a valid rule.")
        }

        fn visit_borrowed_str<E>(self, rule_string: &'de str) -> Result<Self::Value, E>
        where
            E: de::Error,
        {
            fn from_str(s: &str) -> Result<Rule<'_>, reference::parser::ParseError> {
                let rule = reference::parser::parse(s.as_bytes())?;
                Rule::try_from(&rule).map_err(|_| reference::parser::ParseError::ValueTooLarge)
            }

            from_str(rule_string).map_err(|err| {
                de::Error::invalid_value(
                    de::Unexpected::Other(&format!("{err}")),
                    &"a valid UTS 35 rule string",
                )
            })
        }

        fn visit_borrowed_bytes<E>(self, rule_bytes: &'de [u8]) -> Result<Self::Value, E>
        where
            E: de::Error,
        {
            let rule = VarZeroVec::parse_bytes(rule_bytes).map_err(|err| {
                de::Error::invalid_value(
                    de::Unexpected::Other(&format!("{err}")),
                    &"a valid UTS 35 rule byte slice",
                )
            })?;
            Ok(Rule(rule))
        }
    }

    impl<'de: 'data, 'data> Deserialize<'de> for Rule<'data> {
        fn deserialize<D>(deserializer: D) -> Result<Self, D::Error>
        where
            D: Deserializer<'de>,
        {
            if deserializer.is_human_readable() {
                deserializer.deserialize_str(DeserializeRule)
            } else {
                deserializer.deserialize_bytes(DeserializeRule)
            }
        }
    }
}

#[cfg(test)]
mod test {
    use super::*;
    use crate::provider::rules::reference;
    use crate::provider::rules::runtime::test_rule;
    use crate::PluralOperands;

    #[test]
    fn simple_rule_test() {
        use reference::ast;

        let input = "i = 1";
        let full_ast = reference::parse(input.as_bytes()).expect("Failed to convert Rule");
        assert_eq!(
            full_ast,
            ast::Rule {
                condition: ast::Condition(vec![ast::AndCondition(vec![ast::Relation {
                    expression: ast::Expression {
                        operand: ast::Operand::I,
                        modulus: None,
                    },
                    operator: ast::Operator::Eq,
                    range_list: ast::RangeList(vec![ast::RangeListItem::Value(ast::Value(1))])
                }])]),
                samples: None,
            }
        );

        let rule = Rule::try_from(&full_ast).expect("Failed to convert Rule");
        let relation = rule
            .0
            .iter()
            .next()
            .expect("Should have a relation")
            .as_relation();
        assert_eq!(
            relation,
            Relation {
                aopo: AndOrPolarityOperand {
                    and_or: AndOr::And,
                    polarity: Polarity::Positive,
                    operand: Operand::I,
                },
                modulo: 0,
                range_list: ZeroVec::new_borrowed(&[RangeOrValue::Value(1).to_unaligned()])
            }
        );

        let fd = fixed_decimal::Decimal::from(1);
        let operands = PluralOperands::from(&fd);
        assert!(test_rule(&rule, &operands),);
    }

    #[test]
    fn complex_rule_test() {
        let input = "n % 10 = 3..4, 9 and n % 100 != 10..19, 70..79, 90..99 or n = 0";
        let ref_rule = reference::parse(input.as_bytes()).expect("Failed to parse Rule");
        let rule = Rule::try_from(&ref_rule).expect("Failed to convert Rule");

        let fd = fixed_decimal::Decimal::from(0);
        let operands = PluralOperands::from(&fd);
        assert!(test_rule(&rule, &operands),);

        let fd = fixed_decimal::Decimal::from(13);
        let operands = PluralOperands::from(&fd);
        assert!(!test_rule(&rule, &operands),);

        let fd = fixed_decimal::Decimal::from(103);
        let operands = PluralOperands::from(&fd);
        assert!(test_rule(&rule, &operands),);

        let fd = fixed_decimal::Decimal::from(113);
        let operands = PluralOperands::from(&fd);
        assert!(!test_rule(&rule, &operands),);

        let fd = fixed_decimal::Decimal::from(178);
        let operands = PluralOperands::from(&fd);
        assert!(!test_rule(&rule, &operands),);

        let fd = fixed_decimal::Decimal::from(0);
        let operands = PluralOperands::from(&fd);
        assert!(test_rule(&rule, &operands),);
    }

    #[test]
    fn complex_rule_ule_roundtrip_test() {
        let input = "n % 10 = 3..4, 9 and n % 100 != 10..19, 70..79, 90..99 or n = 0";

        let ref_rule = reference::parse(input.as_bytes()).unwrap();

        // Create a ZVZ backed Rule from the reference one.
        let rule = Rule::try_from(&ref_rule).expect("Failed to convert Rule");

        // Convert it back to reference Rule and compare.
        assert_eq!(ref_rule, reference::ast::Rule::from(&rule));

        // Verify that the stringified output matches the input.
        assert_eq!(input, rule.to_string(),);
    }

    #[test]
    fn range_or_value_ule_test() {
        let rov = RangeOrValue::Value(1);
        let ule = rov.to_unaligned();
        let ref_bytes = &[1, 0, 0, 0, 1, 0, 0, 0];
        assert_eq!(ULE::slice_as_bytes(&[ule]), *ref_bytes);

        let rov = RangeOrValue::Range(2, 4);
        let ule = rov.to_unaligned();
        let ref_bytes = &[2, 0, 0, 0, 4, 0, 0, 0];
        assert_eq!(ULE::slice_as_bytes(&[ule]), *ref_bytes);
    }

    #[test]
    fn relation_ule_test() {
        let rov = RangeOrValue::Value(1);
        let aopo = AndOrPolarityOperand {
            and_or: AndOr::And,
            polarity: Polarity::Positive,
            operand: Operand::N,
        };
        let relation = Relation {
            aopo,
            modulo: 0,
            range_list: ZeroVec::alloc_from_slice(&[rov]),
        };
        let relations = alloc::vec![relation];
        let vzv = VarZeroVec::<_>::from(relations.as_slice());
        assert_eq!(
            vzv.as_bytes(),
            &[1, 0, 192, 0, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0]
        );
    }
}
