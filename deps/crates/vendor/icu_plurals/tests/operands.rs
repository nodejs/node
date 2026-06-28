// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

mod fixtures;

use fixed_decimal::Decimal;
use icu_plurals::PluralOperands;
#[cfg(feature = "unstable")]
use icu_plurals::RawPluralOperands;

#[test]
fn test_parsing_operands() {
    let test_set: fixtures::OperandsTestSet =
        serde_json::from_str(include_str!("fixtures/operands.json"))
            .expect("Failed to read a fixture");

    for test in test_set.string {
        let operands: PluralOperands = test.input.parse().expect("Failed to parse to operands.");
        assert_eq!(operands, test.output.into());
    }

    for test in test_set.int {
        let operands: PluralOperands = test.input.into();
        assert_eq!(operands, test.output.clone().into());

        if test.input.is_positive() {
            let x = test.input as usize;
            let operands: PluralOperands = x.into();
            assert_eq!(operands, test.output.into());
        }
    }

    #[cfg(feature = "unstable")]
    for test in test_set.floats {
        let t = test.clone();
        let operands: PluralOperands = t.output.into();
        let raw_operands = RawPluralOperands::from(operands);
        let expected: f64 = t.input.abs();
        let fraction = raw_operands.t as f64 / 10_f64.powi(raw_operands.v as i32);
        let n = raw_operands.i as f64 + fraction;
        let actual = (n - expected).abs();
        assert!(
            actual < f64::EPSILON,
            "actual: {}, for test: {:?}",
            actual,
            &test
        );
    }
}

#[test]
fn test_parsing_operand_errors() {
    let operands: Result<PluralOperands, _> = "".parse();
    assert!(operands.is_err());
}

#[test]
fn test_from_fixed_decimals() {
    let test_set: fixtures::OperandsTestSet =
        serde_json::from_str(include_str!("fixtures/operands.json"))
            .expect("Failed to read a fixture");
    for test in test_set.from_test {
        let input: Decimal = Decimal::from(&test.input);
        let actual: PluralOperands = PluralOperands::from(&input);
        let expected: PluralOperands = PluralOperands::from(test.expected.clone());
        assert_eq!(
            expected, actual,
            "\n\t(expected==left; actual==right)\n\t\t{:?}",
            &test
        );
    }
}
