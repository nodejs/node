use alloc::string::String;

/// Returns whether a character is a valid JS identifier start character.
///
/// This is only ever-so-slightly different from `XID_Start` in a few edge
/// cases, so we handle those edge cases manually and delegate everything else
/// to `unicode-ident`.
fn is_id_start(c: char) -> bool {
    match c {
        '\u{037A}' | '\u{0E33}' | '\u{0EB3}' | '\u{309B}' | '\u{309C}' | '\u{FC5E}'
        | '\u{FC5F}' | '\u{FC60}' | '\u{FC61}' | '\u{FC62}' | '\u{FC63}' | '\u{FDFA}'
        | '\u{FDFB}' | '\u{FE70}' | '\u{FE72}' | '\u{FE74}' | '\u{FE76}' | '\u{FE78}'
        | '\u{FE7A}' | '\u{FE7C}' | '\u{FE7E}' | '\u{FF9E}' | '\u{FF9F}' => true,
        '$' | '_' => true,
        _ => unicode_ident::is_xid_start(c),
    }
}

/// Returns whether a character is a valid JS identifier continue character.
///
/// This is only ever-so-slightly different from `XID_Continue` in a few edge
/// cases, so we handle those edge cases manually and delegate everything else
/// to `unicode-ident`.
fn is_id_continue(c: char) -> bool {
    match c {
        '\u{037A}' | '\u{309B}' | '\u{309C}' | '\u{FC5E}' | '\u{FC5F}' | '\u{FC60}'
        | '\u{FC61}' | '\u{FC62}' | '\u{FC63}' | '\u{FDFA}' | '\u{FDFB}' | '\u{FE70}'
        | '\u{FE72}' | '\u{FE74}' | '\u{FE76}' | '\u{FE78}' | '\u{FE7A}' | '\u{FE7C}'
        | '\u{FE7E}' => true,
        '$' | '\u{200C}' | '\u{200D}' => true,
        _ => unicode_ident::is_xid_continue(c),
    }
}

fn maybe_valid_chars(name: &str) -> impl Iterator<Item = Option<char>> + '_ {
    let mut chars = name.chars();
    // Always emit at least one `None` item - that way `is_valid_ident` can fail without
    // a separate check for empty strings, and `to_valid_ident` will always produce at least
    // one underscore.
    core::iter::once(chars.next().filter(|&c| is_id_start(c))).chain(chars.map(|c| {
        if is_id_continue(c) {
            Some(c)
        } else {
            None
        }
    }))
}

/// Javascript keywords.
///
/// Note that some of these keywords are only reserved in strict mode. Since we
/// generate strict mode JS code, we treat all of these as reserved.
///
/// https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Lexical_grammar#reserved_words
const JS_KEYWORDS: [&str; 47] = [
    "arguments",
    "break",
    "case",
    "catch",
    "class",
    "const",
    "continue",
    "debugger",
    "default",
    "delete",
    "do",
    "else",
    "enum",
    "eval",
    "export",
    "extends",
    "false",
    "finally",
    "for",
    "function",
    "if",
    "implements",
    "import",
    "in",
    "instanceof",
    "interface",
    "let",
    "new",
    "null",
    "package",
    "private",
    "protected",
    "public",
    "return",
    "static",
    "super",
    "switch",
    "this",
    "throw",
    "true",
    "try",
    "typeof",
    "var",
    "void",
    "while",
    "with",
    "yield",
];

/// Javascript keywords that behave like values in that they can be called like
/// functions or have properties accessed on them.
///
/// Naturally, this list is a subset of `JS_KEYWORDS`.
const VALUE_LIKE_JS_KEYWORDS: [&str; 7] = [
    "eval",   // eval is a function-like keyword, so e.g. `eval(...)` is valid
    "false",  // false resolves to a boolean value, so e.g. `false.toString()` is valid
    "import", // import.meta and import()
    "new",    // new.target
    "super", // super can be used for a function call (`super(...)`) or property lookup (`super.prop`)
    "this",  // this obviously can be used as a value
    "true",  // true resolves to a boolean value, so e.g. `false.toString()` is valid
];

/// Returns whether the given string is a JS keyword.
pub fn is_js_keyword(keyword: &str) -> bool {
    JS_KEYWORDS.contains(&keyword)
}
/// Returns whether the given string is a JS keyword that does NOT behave like
/// a value.
///
/// Value-like keywords can be called like functions or have properties
/// accessed, which makes it possible to use them in imports. In general,
/// imports should use this function to check for reserved keywords.
pub fn is_non_value_js_keyword(keyword: &str) -> bool {
    JS_KEYWORDS.contains(&keyword) && !VALUE_LIKE_JS_KEYWORDS.contains(&keyword)
}

/// Returns whether a string is a valid JavaScript identifier.
/// Defined at https://tc39.es/ecma262/#prod-IdentifierName.
pub fn is_valid_ident(name: &str) -> bool {
    maybe_valid_chars(name).all(|opt| opt.is_some())
}

/// Converts a string to a valid JavaScript identifier by replacing invalid
/// characters with underscores.
pub fn to_valid_ident(name: &str) -> String {
    let result: String = maybe_valid_chars(name)
        .map(|opt| opt.unwrap_or('_'))
        .collect();

    if is_js_keyword(&result) || is_non_value_js_keyword(&result) {
        alloc::format!("_{result}")
    } else {
        result
    }
}
