// Vendored from rustc-literal-escaper v0.0.5
// https://github.com/rust-lang/literal-escaper/tree/v0.0.5

//! Utilities for validating (raw) string, char, and byte literals and
//! turning escape sequences into the values they represent.

use crate::num::NonZeroChar;
use core::ffi::CStr;
use core::num::NonZeroU8;
use core::ops::Range;
use core::str::Chars;

/// Errors and warnings that can occur during string, char, and byte unescaping.
///
/// Mostly relating to malformed escape sequences, but also a few other problems.
#[derive(Debug, PartialEq, Eq)]
pub enum EscapeError {
    /// Expected 1 char, but 0 were found.
    ZeroChars,
    /// Expected 1 char, but more than 1 were found.
    MoreThanOneChar,

    /// Escaped '\' character without continuation.
    LoneSlash,
    /// Invalid escape character (e.g. '\z').
    InvalidEscape,
    /// Raw '\r' encountered.
    BareCarriageReturn,
    /// Raw '\r' encountered in raw string.
    BareCarriageReturnInRawString,
    /// Unescaped character that was expected to be escaped (e.g. raw '\t').
    EscapeOnlyChar,

    /// Numeric character escape is too short (e.g. '\x1').
    TooShortHexEscape,
    /// Invalid character in numeric escape (e.g. '\xz')
    InvalidCharInHexEscape,
    /// Character code in numeric escape is non-ascii (e.g. '\xFF').
    OutOfRangeHexEscape,

    /// '\u' not followed by '{'.
    NoBraceInUnicodeEscape,
    /// Non-hexadecimal value in '\u{..}'.
    InvalidCharInUnicodeEscape,
    /// '\u{}'
    EmptyUnicodeEscape,
    /// No closing brace in '\u{..}', e.g. '\u{12'.
    UnclosedUnicodeEscape,
    /// '\u{_12}'
    LeadingUnderscoreUnicodeEscape,
    /// More than 6 characters in '\u{..}', e.g. '\u{10FFFF_FF}'
    OverlongUnicodeEscape,
    /// Invalid in-bound unicode character code, e.g. '\u{DFFF}'.
    LoneSurrogateUnicodeEscape,
    /// Out of bounds unicode character code, e.g. '\u{FFFFFF}'.
    OutOfRangeUnicodeEscape,

    /// Unicode escape code in byte literal.
    UnicodeEscapeInByte,
    /// Non-ascii character in byte literal, byte string literal, or raw byte string literal.
    NonAsciiCharInByte,

    /// `\0` in a C string literal.
    NulInCStr,

    /// After a line ending with '\', the next line contains whitespace
    /// characters that are not skipped.
    UnskippedWhitespaceWarning,

    /// After a line ending with '\', multiple lines are skipped.
    MultipleSkippedLinesWarning,
}

impl EscapeError {
    /// Returns true for actual errors, as opposed to warnings.
    pub fn is_fatal(&self) -> bool {
        !matches!(
            self,
            EscapeError::UnskippedWhitespaceWarning | EscapeError::MultipleSkippedLinesWarning
        )
    }
}

/// Check a raw string literal for validity
///
/// Takes the contents of a raw string literal (without quotes)
/// and produces a sequence of characters or errors,
/// which are returned by invoking `callback`.
/// NOTE: Does no escaping, but produces errors for bare carriage return ('\r').
pub fn check_raw_str(src: &str, callback: impl FnMut(Range<usize>, Result<char, EscapeError>)) {
    str::check_raw(src, callback);
}

/// Check a raw byte string literal for validity
///
/// Takes the contents of a raw byte string literal (without quotes)
/// and produces a sequence of bytes or errors,
/// which are returned by invoking `callback`.
/// NOTE: Does no escaping, but produces errors for bare carriage return ('\r').
pub fn check_raw_byte_str(src: &str, callback: impl FnMut(Range<usize>, Result<u8, EscapeError>)) {
    <[u8]>::check_raw(src, callback);
}

/// Check a raw C string literal for validity
///
/// Takes the contents of a raw C string literal (without quotes)
/// and produces a sequence of characters or errors,
/// which are returned by invoking `callback`.
/// NOTE: Does no escaping, but produces errors for bare carriage return ('\r').
pub fn check_raw_c_str(
    src: &str,
    callback: impl FnMut(Range<usize>, Result<NonZeroChar, EscapeError>),
) {
    CStr::check_raw(src, callback);
}

/// Trait for checking raw string literals for validity
trait CheckRaw {
    /// Unit type of the implementing string type (`char` for string, `u8` for byte string)
    type RawUnit;

    /// Converts chars to the unit type of the literal type
    fn char2raw_unit(c: char) -> Result<Self::RawUnit, EscapeError>;

    /// Takes the contents of a raw literal (without quotes)
    /// and produces a sequence of `Result<Self::RawUnit, EscapeError>`
    /// which are returned via `callback`.
    ///
    /// NOTE: Does no escaping, but produces errors for bare carriage return ('\r').
    fn check_raw(
        src: &str,
        mut callback: impl FnMut(Range<usize>, Result<Self::RawUnit, EscapeError>),
    ) {
        let mut chars = src.chars();
        while let Some(c) = chars.next() {
            let start = src.len() - chars.as_str().len() - c.len_utf8();
            let res = match c {
                '\r' => Err(EscapeError::BareCarriageReturnInRawString),
                _ => Self::char2raw_unit(c),
            };
            let end = src.len() - chars.as_str().len();
            callback(start..end, res);
        }

        // Unfortunately, it is a bit unclear whether the following equivalent code is slower or faster: bug 141855
        // src.char_indices().for_each(|(pos, c)| {
        //     callback(
        //         pos..pos + c.len_utf8(),
        //         if c == '\r' {
        //             Err(EscapeError::BareCarriageReturnInRawString)
        //         } else {
        //             Self::char2raw_unit(c)
        //         },
        //     );
        // });
    }
}

impl CheckRaw for str {
    type RawUnit = char;

    #[inline]
    fn char2raw_unit(c: char) -> Result<Self::RawUnit, EscapeError> {
        Ok(c)
    }
}

impl CheckRaw for [u8] {
    type RawUnit = u8;

    #[inline]
    fn char2raw_unit(c: char) -> Result<Self::RawUnit, EscapeError> {
        char2byte(c)
    }
}

/// Turn an ascii char into a byte
#[inline]
fn char2byte(c: char) -> Result<u8, EscapeError> {
    // do NOT do: c.try_into().ok_or(EscapeError::NonAsciiCharInByte)
    if c.is_ascii() {
        Ok(c as u8)
    } else {
        Err(EscapeError::NonAsciiCharInByte)
    }
}

impl CheckRaw for CStr {
    type RawUnit = NonZeroChar;

    #[inline]
    fn char2raw_unit(c: char) -> Result<Self::RawUnit, EscapeError> {
        NonZeroChar::new(c).ok_or(EscapeError::NulInCStr)
    }
}

/// Unescape a char literal
///
/// Takes the contents of a char literal (without quotes),
/// and returns an unescaped char or an error.
#[inline]
pub fn unescape_char(src: &str) -> Result<char, EscapeError> {
    str::unescape_single(&mut src.chars())
}

/// Unescape a byte literal
///
/// Takes the contents of a byte literal (without quotes),
/// and returns an unescaped byte or an error.
#[inline]
pub fn unescape_byte(src: &str) -> Result<u8, EscapeError> {
    <[u8]>::unescape_single(&mut src.chars())
}

/// Unescape a string literal
///
/// Takes the contents of a string literal (without quotes)
/// and produces a sequence of escaped characters or errors,
/// which are returned by invoking `callback`.
pub fn unescape_str(src: &str, callback: impl FnMut(Range<usize>, Result<char, EscapeError>)) {
    str::unescape(src, callback)
}

/// Unescape a byte string literal
///
/// Takes the contents of a byte string literal (without quotes)
/// and produces a sequence of escaped bytes or errors,
/// which are returned by invoking `callback`.
pub fn unescape_byte_str(src: &str, callback: impl FnMut(Range<usize>, Result<u8, EscapeError>)) {
    <[u8]>::unescape(src, callback)
}

/// Unescape a C string literal
///
/// Takes the contents of a C string literal (without quotes)
/// and produces a sequence of escaped MixedUnits or errors,
/// which are returned by invoking `callback`.
pub fn unescape_c_str(
    src: &str,
    callback: impl FnMut(Range<usize>, Result<MixedUnit, EscapeError>),
) {
    CStr::unescape(src, callback)
}

/// Enum representing either a char or a byte
///
/// Used for mixed utf8 string literals, i.e. those that allow both unicode
/// chars and high bytes.
#[derive(Copy, Clone, Debug, PartialEq, Eq)]
pub enum MixedUnit {
    /// Used for ASCII chars (written directly or via `\x00`..`\x7f` escapes)
    /// and Unicode chars (written directly or via `\u` escapes).
    ///
    /// For example, if '¥' appears in a string it is represented here as
    /// `MixedUnit::Char('¥')`, and it will be appended to the relevant byte
    /// string as the two-byte UTF-8 sequence `[0xc2, 0xa5]`
    Char(NonZeroChar),

    /// Used for high bytes (`\x80`..`\xff`).
    ///
    /// For example, if `\xa5` appears in a string it is represented here as
    /// `MixedUnit::HighByte(0xa5)`, and it will be appended to the relevant
    /// byte string as the single byte `0xa5`.
    HighByte(NonZeroU8),
}

impl From<NonZeroChar> for MixedUnit {
    #[inline]
    fn from(c: NonZeroChar) -> Self {
        MixedUnit::Char(c)
    }
}

impl From<NonZeroU8> for MixedUnit {
    #[inline]
    fn from(byte: NonZeroU8) -> Self {
        if byte.get().is_ascii() {
            MixedUnit::Char(NonZeroChar::new(byte.get() as char).unwrap())
        } else {
            MixedUnit::HighByte(byte)
        }
    }
}

impl TryFrom<char> for MixedUnit {
    type Error = EscapeError;

    #[inline]
    fn try_from(c: char) -> Result<Self, EscapeError> {
        NonZeroChar::new(c)
            .map(MixedUnit::Char)
            .ok_or(EscapeError::NulInCStr)
    }
}

impl TryFrom<u8> for MixedUnit {
    type Error = EscapeError;

    #[inline]
    fn try_from(byte: u8) -> Result<Self, EscapeError> {
        NonZeroU8::new(byte)
            .map(From::from)
            .ok_or(EscapeError::NulInCStr)
    }
}

/// Trait for unescaping escape sequences in strings
trait Unescape {
    /// Unit type of the implementing string type (`char` for string, `u8` for byte string)
    type Unit;

    /// Result of unescaping the zero char ('\0')
    const ZERO_RESULT: Result<Self::Unit, EscapeError>;

    /// Converts non-zero bytes to the unit type
    fn nonzero_byte2unit(b: NonZeroU8) -> Self::Unit;

    /// Converts chars to the unit type
    fn char2unit(c: char) -> Result<Self::Unit, EscapeError>;

    /// Converts the byte of a hex escape to the unit type
    fn hex2unit(b: u8) -> Result<Self::Unit, EscapeError>;

    /// Converts the result of a unicode escape to the unit type
    fn unicode2unit(r: Result<char, EscapeError>) -> Result<Self::Unit, EscapeError>;

    /// Unescape a single unit (single quote syntax)
    fn unescape_single(chars: &mut Chars<'_>) -> Result<Self::Unit, EscapeError> {
        let res = match chars.next().ok_or(EscapeError::ZeroChars)? {
            '\\' => Self::unescape_1(chars),
            '\n' | '\t' | '\'' => Err(EscapeError::EscapeOnlyChar),
            '\r' => Err(EscapeError::BareCarriageReturn),
            c => Self::char2unit(c),
        }?;
        if chars.next().is_some() {
            return Err(EscapeError::MoreThanOneChar);
        }
        Ok(res)
    }

    /// Unescape the first unit of a string (double quoted syntax)
    fn unescape_1(chars: &mut Chars<'_>) -> Result<Self::Unit, EscapeError> {
        // Previous character was '\\', unescape what follows.
        let c = chars.next().ok_or(EscapeError::LoneSlash)?;
        if c == '0' {
            Self::ZERO_RESULT
        } else {
            simple_escape(c)
                .map(|b| Self::nonzero_byte2unit(b))
                .or_else(|c| match c {
                    'x' => Self::hex2unit(hex_escape(chars)?),
                    'u' => Self::unicode2unit({
                        let value = unicode_escape(chars)?;
                        if value > char::MAX as u32 {
                            Err(EscapeError::OutOfRangeUnicodeEscape)
                        } else {
                            char::from_u32(value).ok_or(EscapeError::LoneSurrogateUnicodeEscape)
                        }
                    }),
                    _ => Err(EscapeError::InvalidEscape),
                })
        }
    }

    /// Unescape a string literal
    ///
    /// Takes the contents of a raw string literal (without quotes)
    /// and produces a sequence of `Result<Self::Unit, EscapeError>`
    /// which are returned via `callback`.
    fn unescape(
        src: &str,
        mut callback: impl FnMut(Range<usize>, Result<Self::Unit, EscapeError>),
    ) {
        let mut chars = src.chars();
        while let Some(c) = chars.next() {
            let start = src.len() - chars.as_str().len() - c.len_utf8();
            let res = match c {
                '\\' => {
                    if let Some(b'\n') = chars.as_str().as_bytes().first() {
                        let _ = chars.next();
                        // skip whitespace for backslash newline, see [Rust language reference]
                        // (https://doc.rust-lang.org/reference/tokens.html#string-literals).
                        let callback_err = |range, err| callback(range, Err(err));
                        skip_ascii_whitespace(&mut chars, start, callback_err);
                        continue;
                    } else {
                        Self::unescape_1(&mut chars)
                    }
                }
                '"' => Err(EscapeError::EscapeOnlyChar),
                '\r' => Err(EscapeError::BareCarriageReturn),
                c => Self::char2unit(c),
            };
            let end = src.len() - chars.as_str().len();
            callback(start..end, res);
        }
    }
}

/// Interpret a non-nul ASCII escape
///
/// Parses the character of an ASCII escape (except nul) without the leading backslash.
#[inline] // single use in Unescape::unescape_1
fn simple_escape(c: char) -> Result<NonZeroU8, char> {
    // Previous character was '\\', unescape what follows.
    Ok(NonZeroU8::new(match c {
        '"' => b'"',
        'n' => b'\n',
        'r' => b'\r',
        't' => b'\t',
        '\\' => b'\\',
        '\'' => b'\'',
        _ => Err(c)?,
    })
    .unwrap())
}

/// Interpret a hexadecimal escape
///
/// Parses the two hexadecimal characters of a hexadecimal escape without the leading r"\x".
#[inline] // single use in Unescape::unescape_1
fn hex_escape(chars: &mut impl Iterator<Item = char>) -> Result<u8, EscapeError> {
    let hi = chars.next().ok_or(EscapeError::TooShortHexEscape)?;
    let hi = hi.to_digit(16).ok_or(EscapeError::InvalidCharInHexEscape)?;

    let lo = chars.next().ok_or(EscapeError::TooShortHexEscape)?;
    let lo = lo.to_digit(16).ok_or(EscapeError::InvalidCharInHexEscape)?;

    Ok((hi * 16 + lo) as u8)
}

/// Interpret a unicode escape
///
/// Parse the braces with hexadecimal characters (and underscores) part of a unicode escape.
/// This r"{...}" normally comes after r"\u" and cannot start with an underscore.
#[inline] // single use in Unescape::unescape_1
fn unicode_escape(chars: &mut impl Iterator<Item = char>) -> Result<u32, EscapeError> {
    if chars.next() != Some('{') {
        return Err(EscapeError::NoBraceInUnicodeEscape);
    }

    // First character must be a hexadecimal digit.
    let mut value: u32 = match chars.next().ok_or(EscapeError::UnclosedUnicodeEscape)? {
        '_' => return Err(EscapeError::LeadingUnderscoreUnicodeEscape),
        '}' => return Err(EscapeError::EmptyUnicodeEscape),
        c => c
            .to_digit(16)
            .ok_or(EscapeError::InvalidCharInUnicodeEscape)?,
    };

    // First character is valid, now parse the rest of the number
    // and closing brace.
    let mut n_digits = 1;
    loop {
        match chars.next() {
            None => return Err(EscapeError::UnclosedUnicodeEscape),
            Some('_') => continue,
            Some('}') => {
                // Incorrect syntax has higher priority for error reporting
                // than unallowed value for a literal.
                return if n_digits > 6 {
                    Err(EscapeError::OverlongUnicodeEscape)
                } else {
                    Ok(value)
                };
            }
            Some(c) => {
                let digit: u32 = c
                    .to_digit(16)
                    .ok_or(EscapeError::InvalidCharInUnicodeEscape)?;
                n_digits += 1;
                if n_digits > 6 {
                    // Stop updating value since we're sure that it's incorrect already.
                    continue;
                }
                value = value * 16 + digit;
            }
        };
    }
}

/// Interpret a string continuation escape (https://doc.rust-lang.org/reference/expressions/literal-expr.html#string-continuation-escapes)
///
/// Skip ASCII whitespace, except for the formfeed character
/// (see [this issue](https://github.com/rust-lang/rust/issues/136600)).
/// Warns on unescaped newline and following non-ASCII whitespace.
#[inline] // single use in Unescape::unescape
fn skip_ascii_whitespace(
    chars: &mut Chars<'_>,
    start: usize,
    mut callback: impl FnMut(Range<usize>, EscapeError),
) {
    let rest = chars.as_str();
    let first_non_space = rest
        .bytes()
        .position(|b| b != b' ' && b != b'\t' && b != b'\n' && b != b'\r')
        .unwrap_or(rest.len());
    let (space, rest) = rest.split_at(first_non_space);
    // backslash newline adds 2 bytes
    let end = start + 2 + first_non_space;
    if space.contains('\n') {
        callback(start..end, EscapeError::MultipleSkippedLinesWarning);
    }
    *chars = rest.chars();
    if let Some(c) = chars.clone().next() {
        if c.is_whitespace() {
            // for error reporting, include the character that was not skipped in the span
            callback(
                start..end + c.len_utf8(),
                EscapeError::UnskippedWhitespaceWarning,
            );
        }
    }
}

impl Unescape for str {
    type Unit = char;

    const ZERO_RESULT: Result<Self::Unit, EscapeError> = Ok('\0');

    #[inline]
    fn nonzero_byte2unit(b: NonZeroU8) -> Self::Unit {
        b.get().into()
    }

    #[inline]
    fn char2unit(c: char) -> Result<Self::Unit, EscapeError> {
        Ok(c)
    }

    #[inline]
    fn hex2unit(b: u8) -> Result<Self::Unit, EscapeError> {
        if b.is_ascii() {
            Ok(b as char)
        } else {
            Err(EscapeError::OutOfRangeHexEscape)
        }
    }

    #[inline]
    fn unicode2unit(r: Result<char, EscapeError>) -> Result<Self::Unit, EscapeError> {
        r
    }
}

impl Unescape for [u8] {
    type Unit = u8;

    const ZERO_RESULT: Result<Self::Unit, EscapeError> = Ok(b'\0');

    #[inline]
    fn nonzero_byte2unit(b: NonZeroU8) -> Self::Unit {
        b.get()
    }

    #[inline]
    fn char2unit(c: char) -> Result<Self::Unit, EscapeError> {
        char2byte(c)
    }

    #[inline]
    fn hex2unit(b: u8) -> Result<Self::Unit, EscapeError> {
        Ok(b)
    }

    #[inline]
    fn unicode2unit(_r: Result<char, EscapeError>) -> Result<Self::Unit, EscapeError> {
        Err(EscapeError::UnicodeEscapeInByte)
    }
}

impl Unescape for CStr {
    type Unit = MixedUnit;

    const ZERO_RESULT: Result<Self::Unit, EscapeError> = Err(EscapeError::NulInCStr);

    #[inline]
    fn nonzero_byte2unit(b: NonZeroU8) -> Self::Unit {
        b.into()
    }

    #[inline]
    fn char2unit(c: char) -> Result<Self::Unit, EscapeError> {
        c.try_into()
    }

    #[inline]
    fn hex2unit(byte: u8) -> Result<Self::Unit, EscapeError> {
        byte.try_into()
    }

    #[inline]
    fn unicode2unit(r: Result<char, EscapeError>) -> Result<Self::Unit, EscapeError> {
        Self::char2unit(r?)
    }
}

/// Enum of the different kinds of literal
#[derive(Debug, Clone, Copy, PartialEq)]
pub enum Mode {
    /// `'a'`
    Char,

    /// `b'a'`
    Byte,

    /// `"hello"`
    Str,
    /// `r"hello"`
    RawStr,

    /// `b"hello"`
    ByteStr,
    /// `br"hello"`
    RawByteStr,

    /// `c"hello"`
    CStr,
    /// `cr"hello"`
    RawCStr,
}

impl Mode {
    pub fn in_double_quotes(self) -> bool {
        match self {
            Mode::Str
            | Mode::RawStr
            | Mode::ByteStr
            | Mode::RawByteStr
            | Mode::CStr
            | Mode::RawCStr => true,
            Mode::Char | Mode::Byte => false,
        }
    }

    pub fn prefix_noraw(self) -> &'static str {
        match self {
            Mode::Char | Mode::Str | Mode::RawStr => "",
            Mode::Byte | Mode::ByteStr | Mode::RawByteStr => "b",
            Mode::CStr | Mode::RawCStr => "c",
        }
    }
}

/// Check a literal only for errors
///
/// Takes the contents of a literal (without quotes)
/// and produces a sequence of only errors,
/// which are returned by invoking `error_callback`.
///
/// NB Does not produce any output other than errors
pub fn check_for_errors(
    src: &str,
    mode: Mode,
    mut error_callback: impl FnMut(Range<usize>, EscapeError),
) {
    match mode {
        Mode::Char => {
            let mut chars = src.chars();
            if let Err(e) = str::unescape_single(&mut chars) {
                error_callback(0..(src.len() - chars.as_str().len()), e);
            }
        }
        Mode::Byte => {
            let mut chars = src.chars();
            if let Err(e) = <[u8]>::unescape_single(&mut chars) {
                error_callback(0..(src.len() - chars.as_str().len()), e);
            }
        }
        Mode::Str => unescape_str(src, |range, res| {
            if let Err(e) = res {
                error_callback(range, e);
            }
        }),
        Mode::ByteStr => unescape_byte_str(src, |range, res| {
            if let Err(e) = res {
                error_callback(range, e);
            }
        }),
        Mode::CStr => unescape_c_str(src, |range, res| {
            if let Err(e) = res {
                error_callback(range, e);
            }
        }),
        Mode::RawStr => check_raw_str(src, |range, res| {
            if let Err(e) = res {
                error_callback(range, e);
            }
        }),
        Mode::RawByteStr => check_raw_byte_str(src, |range, res| {
            if let Err(e) = res {
                error_callback(range, e);
            }
        }),
        Mode::RawCStr => check_raw_c_str(src, |range, res| {
            if let Err(e) = res {
                error_callback(range, e);
            }
        }),
    }
}
