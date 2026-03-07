use swc_common::input::Input;

use crate::{byte_search, lexer::search::SafeByteMatchTable, safe_byte_match_table, Lexer};

/// U+000B VERTICAL TAB, abbreviated `<VT>`.
const B_VT: u8 = 0x0b;

/// U+000C FORM FEED, abbreviated `<FF>`.
const B_FF: u8 = 0x0c;

// https://github.com/oxc-project/oxc/blob/ec6721c458d64c5b27b78542aa205d70b06edf9a/crates/oxc_syntax/src/identifier.rs#L70
#[inline]
pub fn is_irregular_whitespace(c: char) -> bool {
    /// U+FEFF ZERO WIDTH NO-BREAK SPACE, abbreviated `<ZWNBSP>`.
    /// Considered a whitespace character in JS.
    const ZWNBSP: char = '\u{feff}';

    /// U+000B VERTICAL TAB, abbreviated `<VT>`.
    const VT: char = '\u{b}';

    /// U+000C FORM FEED, abbreviated `<FF>`.
    const FF: char = '\u{c}';

    /// U+00A0 NON-BREAKING SPACE, abbreviated `<NBSP>`.
    const NBSP: char = '\u{a0}';

    /// U+0085 NEXT LINE, abbreviated `<NEL>`.
    const NEL: char = '\u{85}';

    const OGHAM_SPACE_MARK: char = '\u{1680}';

    const EN_QUAD: char = '\u{2000}';

    /// U+200B ZERO WIDTH SPACE, abbreviated `<ZWSP>`.
    const ZWSP: char = '\u{200b}';

    /// Narrow NO-BREAK SPACE, abbreviated `<NNBSP>`.
    const NNBSP: char = '\u{202f}';

    /// U+205F MEDIUM MATHEMATICAL SPACE, abbreviated `<MMSP>`.
    const MMSP: char = '\u{205f}';

    const IDEOGRAPHIC_SPACE: char = '\u{3000}';
    matches!(
        c,
        VT | FF | NBSP | ZWNBSP | NEL | OGHAM_SPACE_MARK | EN_QUAD
            ..=ZWSP | NNBSP | MMSP | IDEOGRAPHIC_SPACE
    )
}

// https://github.com/oxc-project/oxc/blob/ec6721c458d64c5b27b78542aa205d70b06edf9a/crates/oxc_syntax/src/identifier.rs#L102
#[inline]
pub fn is_irregular_line_terminator(c: char) -> bool {
    /// U+2028 LINE SEPARATOR, abbreviated `<LS>`.
    const LS: char = '\u{2028}';

    /// U+2029 PARAGRAPH SEPARATOR, abbreviated `<PS>`.
    const PS: char = '\u{2029}';

    matches!(c, LS | PS)
}

/// Returns true if it's done
type ByteHandler = fn(&mut Lexer<'_>) -> bool;

/// Lookup table for whitespace
#[rustfmt::skip]
static BYTE_HANDLERS: [ByteHandler; 256] = [
//   0    1    2    3    4    5    6    7    8    9    A    B    C    D    E    F   //
    ___, ___, ___, ___, ___, ___, ___, ___, ___, SPC, NLN, SPC, SPC, NLN, ___, ___, // 0
    ___, ___, ___, ___, ___, ___, ___, ___, ___, ___, ___, ___, ___, ___, ___, ___, // 1
    SPC, ___, ___, ___, ___, ___, ___, ___, ___, ___, ___, ___, ___, ___, ___, SLH, // 2
    ___, ___, ___, ___, ___, ___, ___, ___, ___, ___, ___, ___, ___, ___, ___, ___, // 3
    ___, ___, ___, ___, ___, ___, ___, ___, ___, ___, ___, ___, ___, ___, ___, ___, // 4
    ___, ___, ___, ___, ___, ___, ___, ___, ___, ___, ___, ___, ___, ___, ___, ___, // 5
    ___, ___, ___, ___, ___, ___, ___, ___, ___, ___, ___, ___, ___, ___, ___, ___, // 6
    ___, ___, ___, ___, ___, ___, ___, ___, ___, ___, ___, ___, ___, ___, ___, ___, // 7
    UNI, UNI, UNI, UNI, UNI, UNI, UNI, UNI, UNI, UNI, UNI, UNI, UNI, UNI, UNI, UNI, // 8
    UNI, UNI, UNI, UNI, UNI, UNI, UNI, UNI, UNI, UNI, UNI, UNI, UNI, UNI, UNI, UNI, // 9
    UNI, UNI, UNI, UNI, UNI, UNI, UNI, UNI, UNI, UNI, UNI, UNI, UNI, UNI, UNI, UNI, // A
    UNI, UNI, UNI, UNI, UNI, UNI, UNI, UNI, UNI, UNI, UNI, UNI, UNI, UNI, UNI, UNI, // B
    UNI, UNI, UNI, UNI, UNI, UNI, UNI, UNI, UNI, UNI, UNI, UNI, UNI, UNI, UNI, UNI, // C
    UNI, UNI, UNI, UNI, UNI, UNI, UNI, UNI, UNI, UNI, UNI, UNI, UNI, UNI, UNI, UNI, // D
    UNI, UNI, UNI, UNI, UNI, UNI, UNI, UNI, UNI, UNI, UNI, UNI, UNI, UNI, UNI, UNI, // E
    UNI, UNI, UNI, UNI, UNI, UNI, UNI, UNI, UNI, UNI, UNI, UNI, UNI, UNI, UNI, UNI, // F
];

/// Stop
const ___: ByteHandler = |_| false;

/// Newline
const NLN: ByteHandler = |lexer| {
    static NOT_REGULAR_WHITESPACE_OR_LINE_BREAK_TABLE: SafeByteMatchTable =
        safe_byte_match_table!(|b| !matches!(b, b' ' | b'\t' | B_VT | B_FF | b'\r' | b'\n'));

    lexer.state.had_line_break = true;
    byte_search! {
        lexer: lexer,
        table: NOT_REGULAR_WHITESPACE_OR_LINE_BREAK_TABLE,
        handle_eof: return false,
    };
    true
};

/// Space
const SPC: ByteHandler = |lexer| {
    static NOT_SPC: SafeByteMatchTable =
        safe_byte_match_table!(|b| !matches!(b, b' ' | b'\t' | B_VT | B_FF));

    byte_search! {
        lexer: lexer,
        table: NOT_SPC,
        handle_eof: return false,
    };
    true
};

const SLH: ByteHandler = |lexer| match lexer.peek() {
    Some(b'/') => {
        lexer.skip_line_comment(2);
        true
    }
    Some(b'*') => {
        lexer.skip_block_comment();
        true
    }
    _ => false,
};

/// Unicode - handles multi-byte UTF-8 whitespace characters
const UNI: ByteHandler = |lexer| {
    // For non-ASCII bytes, we need the full UTF-8 character
    let Some(c) = lexer.cur_as_char() else {
        return false;
    };

    if is_irregular_whitespace(c) {
        lexer.bump(c.len_utf8());
        true
    } else if is_irregular_line_terminator(c) {
        lexer.bump(c.len_utf8());
        lexer.state.had_line_break = true;
        true
    } else {
        false
    }
};

impl<'a> Lexer<'a> {
    /// Skip comments or whitespaces.
    ///
    /// See https://tc39.github.io/ecma262/#sec-white-space
    #[inline]
    pub fn skip_space(&mut self) {
        loop {
            let byte = match self.input.cur() {
                Some(v) => v,
                None => return,
            };

            let handler = unsafe { *(&BYTE_HANDLERS as *const ByteHandler).offset(byte as isize) };
            if !handler(self) {
                break;
            }
        }
    }
}
