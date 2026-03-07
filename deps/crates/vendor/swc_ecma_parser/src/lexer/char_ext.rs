/// Implemented for `u8` and `char` - operates on bytes for performance.
pub trait CharExt: Copy {
    fn to_char(self) -> Option<char>;

    /// Test whether a given byte/character starts an identifier.
    ///
    /// https://tc39.github.io/ecma262/#prod-IdentifierStart
    #[inline]
    fn is_ident_start(self) -> bool {
        let c = match self.to_char() {
            Some(c) => c,
            None => return false,
        };
        swc_ecma_ast::Ident::is_valid_start(c)
    }

    /// Test whether a given byte/character is part of an identifier.
    #[inline]
    fn is_ident_part(self) -> bool {
        let c = match self.to_char() {
            Some(c) => c,
            None => return false,
        };
        swc_ecma_ast::Ident::is_valid_continue(c)
    }

    /// See https://tc39.github.io/ecma262/#sec-line-terminators
    #[inline]
    fn is_line_terminator(self) -> bool {
        let c = match self.to_char() {
            Some(c) => c,
            None => return false,
        };
        matches!(c, '\r' | '\n' | '\u{2028}' | '\u{2029}')
    }
}

impl CharExt for u8 {
    #[inline(always)]
    fn to_char(self) -> Option<char> {
        // For ASCII bytes, this is a fast path
        if self <= 0x7f {
            Some(self as char)
        } else {
            // For non-ASCII bytes, we can't convert a single byte to a char
            // The caller should use cur_as_char() on the Input trait instead
            None
        }
    }
}

impl CharExt for char {
    #[inline(always)]
    fn to_char(self) -> Option<char> {
        Some(self)
    }
}
