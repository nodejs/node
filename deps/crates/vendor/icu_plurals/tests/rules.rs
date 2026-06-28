// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

mod fixtures;

use icu_plurals::provider::rules::{
    reference::test_condition,
    reference::{parse, parse_condition, serialize, Lexer},
};
use icu_plurals::PluralOperands;

#[test]
fn test_parsing_operands() {
    let test_set: fixtures::RuleTestSet = serde_json::from_str(include_str!("fixtures/rules.json"))
        .expect("Failed to read a fixture");

    for test in test_set.0 {
        match test.output {
            fixtures::RuleTestOutput::Value(val) => {
                // Test that lexer completes.
                let lex = Lexer::new(test.rule.as_bytes());
                lex.count();

                // Test that rule matches test.
                let ast = parse_condition(test.rule.as_bytes()).expect("Failed to parse.");
                let operands: PluralOperands = test.input.into();

                if val {
                    assert!(
                        test_condition(&ast, &operands),
                        "\nExpected true\n\
                            AST: {ast:#?}\n\
                            Operands: {operands:#?}\n"
                    );
                } else {
                    assert!(
                        !test_condition(&ast, &operands),
                        "\nExpected false\n\
                            AST: {ast:#?}\n\
                            Operands: {operands:#?}\n"
                    );
                }

                // Test that parse/serialize roundtrip completes.
                let ast = parse(test.rule.as_bytes()).expect("Failed to parse.");
                let mut string = String::new();
                assert!(serialize(&ast, &mut string).is_ok());
                assert_eq!(string, test.rule);
            }
            fixtures::RuleTestOutput::Error(val) => {
                let err = parse(test.rule.as_bytes()).unwrap_err();
                assert_eq!(format!("{err:?}"), val);
            }
        }
    }
}
