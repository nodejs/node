pub const EOF: char = '\0';

// 11.1 Unicode Format-Control Characters

/// U+200C ZERO WIDTH NON-JOINER, abbreviated in the spec as `<ZWNJ>`.
/// Specially permitted in identifiers.
pub const ZWNJ: char = '\u{200c}';

/// U+200D ZERO WIDTH JOINER, abbreviated as `<ZWJ>`.
/// Specially permitted in identifiers.
pub const ZWJ: char = '\u{200d}';

/// U+FEFF ZERO WIDTH NO-BREAK SPACE, abbreviated `<ZWNBSP>`.
/// Considered a whitespace character in JS.
pub const ZWNBSP: char = '\u{feff}';

// 11.2 White Space
/// U+0009 CHARACTER TABULATION, abbreviated `<TAB>`.
pub const TAB: char = '\u{9}';

/// U+000B VERTICAL TAB, abbreviated `<VT>`.
pub const VT: char = '\u{b}';

/// U+000C FORM FEED, abbreviated `<FF>`.
pub const FF: char = '\u{c}';

/// U+0020 SPACE, abbreviated `<SP>`.
pub const SP: char = '\u{20}';

/// U+00A0 NON-BREAKING SPACE, abbreviated `<NBSP>`.
pub const NBSP: char = '\u{a0}';

// U+0085 NEXT LINE, abbreviated `<NEL>`.
const NEL: char = '\u{85}';

const OGHAM_SPACE_MARK: char = '\u{1680}';

const EN_QUAD: char = '\u{2000}';

// U+200B ZERO WIDTH SPACE, abbreviated `<ZWSP>`.
const ZWSP: char = '\u{200b}';

// Narrow NO-BREAK SPACE, abbreviated `<NNBSP>`.
const NNBSP: char = '\u{202f}';

// U+205F MEDIUM MATHEMATICAL SPACE, abbreviated `<MMSP>`.
const MMSP: char = '\u{205f}';

const IDEOGRAPHIC_SPACE: char = '\u{3000}';

// https://github.com/microsoft/TypeScript/blob/9e20e032effad965567d4a1e1c30d5433b0a3332/src/compiler/scanner.ts#L562-L571
#[rustfmt::skip]
pub fn is_irregular_whitespace(c: char) -> bool {
    matches!(
        c,
        | VT
        | FF
        | NBSP
        | NEL
        | OGHAM_SPACE_MARK
        | EN_QUAD..=ZWSP
        | NNBSP
        | MMSP
        | IDEOGRAPHIC_SPACE
        | ZWNBSP
    )
}

// https://github.com/microsoft/TypeScript/blob/9e20e032effad965567d4a1e1c30d5433b0a3332/src/compiler/scanner.ts#L557-L572
pub fn is_white_space_single_line(c: char) -> bool {
    // Note: nextLine is in the Zs space, and should be considered to be a
    // whitespace.
    // It is explicitly not a line-break as it isn't in the exact set specified by
    // EcmaScript.
    matches!(c, SP | TAB) || is_irregular_whitespace(c)
}

// 11.3 Line Terminators

///  U+000A LINE FEED, abbreviated in the spec as `<LF>`.
pub const LF: char = '\u{a}';

/// U+000D CARRIAGE RETURN, abbreviated in the spec as `<CR>`.
pub const CR: char = '\u{d}';

/// U+2028 LINE SEPARATOR, abbreviated `<LS>`.
pub const LS: char = '\u{2028}';

/// U+2029 PARAGRAPH SEPARATOR, abbreviated `<PS>`.
pub const PS: char = '\u{2029}';

pub fn is_regular_line_terminator(c: char) -> bool {
    matches!(c, LF | CR)
}

pub fn is_irregular_line_terminator(c: char) -> bool {
    matches!(c, LS | PS)
}

// https://github.com/microsoft/TypeScript/blob/9e20e032effad965567d4a1e1c30d5433b0a3332/src/compiler/scanner.ts#L574-L590
pub fn is_line_terminator(c: char) -> bool {
    is_regular_line_terminator(c) || is_irregular_line_terminator(c)
}
