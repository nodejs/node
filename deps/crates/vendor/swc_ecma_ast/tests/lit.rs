use swc_atoms::{wtf8::Wtf8, Atom};
use swc_common::DUMMY_SP;
use swc_ecma_ast::{Str, TplElement};

/// Convert Wtf8 to a string representation, escaping invalid surrogates.
fn convert_wtf8_to_raw(s: &Wtf8) -> String {
    let mut result = String::new();
    let iter = s.code_points();

    for code_point in iter {
        if let Some(c) = code_point.to_char() {
            result.push(c);
        } else {
            result.push_str(format!("\\u{:04X}", code_point.to_u32()).as_str());
        }
    }

    result
}

/// Helper function to test `Str::from_tpl_raw`
fn test_from_tpl_raw(raw: &str, expected: &str) {
    let tpl = TplElement {
        span: DUMMY_SP,
        tail: true,
        raw: Atom::new(raw.to_string()),
        cooked: None,
    };

    let result = Str::from_tpl_raw(&tpl);
    let result_str = convert_wtf8_to_raw(&result);

    assert_eq!(result_str, expected, "Input: {raw}");
}

#[test]
fn basic_escape_sequences() {
    test_from_tpl_raw("hello world", "hello world");
}

#[test]
fn hex_escape_sequences() {
    test_from_tpl_raw("\\xff", "\u{ff}");
}

#[test]
fn emoji_literals() {
    test_from_tpl_raw("🦀", "🦀");
    test_from_tpl_raw("🚀", "🚀");
}

#[test]
fn unicode_escape_sequences() {
    test_from_tpl_raw("\\uD83E\\uDD80", "🦀");
    test_from_tpl_raw("\\u{D83E}\\u{DD80}", "🦀");
    test_from_tpl_raw("\\u{1F980}", "🦀");
    test_from_tpl_raw("\\uD800\\uDC00", "𐀀");
}
#[test]
fn surrogate_pair_boundary_cases() {
    // First supplementary plane character
    test_from_tpl_raw("\\uD800\\uDC00", "\u{10000}");

    // Last valid character
    test_from_tpl_raw("\\uDBFF\\uDFFF", "\u{10FFFF}");

    // Various combinations to ensure formula is correct
    test_from_tpl_raw("\\uD801\\uDC37", "\u{10437}"); // 𐐷
    test_from_tpl_raw("\\uD852\\uDF62", "\u{24B62}"); // 𤭢
}

#[test]
fn invalid_surrogate_pairs() {
    test_from_tpl_raw("\\u{d800}", "\\uD800");
    test_from_tpl_raw("\\u{dc00}", "\\uDC00");
    test_from_tpl_raw("\\u{dFFF}", "\\uDFFF");
}

#[test]
fn surrogate_pair_combinations() {
    test_from_tpl_raw("\\u{d800}\\uD83E\\uDD80", "\\uD800🦀");
    test_from_tpl_raw("\\uD83E-", "\\uD83E-");
}

#[test]
fn single_high_surrogate() {
    test_from_tpl_raw("\\uD83E", "\\uD83E");
}

#[test]
fn common_escape_sequences() {
    test_from_tpl_raw("\\0", "\u{0000}");
    test_from_tpl_raw("\\b", "\u{0008}");
    test_from_tpl_raw("\\f", "\u{000C}");
    test_from_tpl_raw("\\n", "\n");
    test_from_tpl_raw("\\r", "\r");
    test_from_tpl_raw("\\t", "\t");
    test_from_tpl_raw("\\v", "\u{000B}");
}

#[test]
fn escaped_template_chars() {
    test_from_tpl_raw("\\`", "`");
    test_from_tpl_raw("\\$", "$");
    test_from_tpl_raw("\\\\", "\\");
    test_from_tpl_raw("Hello \\\nworld!", "Hello world!");
}

#[test]
fn combined_escapes() {
    test_from_tpl_raw("hello\\nworld", "hello\nworld");
    test_from_tpl_raw("\\t\\tindented", "\t\tindented");
}

// Tests for octal escape sequences that should be rejected.
// These will panic because octal escapes are not allowed in template strings.
#[test]
#[should_panic]
fn should_panic_octal_01() {
    test_from_tpl_raw("\\01", "");
}

#[test]
#[should_panic]
fn should_panic_octal_2() {
    test_from_tpl_raw("\\2", "");
}

#[test]
#[should_panic]
fn should_panic_octal_7() {
    test_from_tpl_raw("\\7", "");
}
