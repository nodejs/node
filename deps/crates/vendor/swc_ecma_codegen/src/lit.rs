use std::{fmt::Write, io, str};

use ascii::AsciiChar;
use compact_str::CompactString;
use swc_atoms::wtf8::{CodePoint, Wtf8};
use swc_common::{Spanned, DUMMY_SP};
use swc_ecma_ast::*;
use swc_ecma_codegen_macros::node_impl;

#[cfg(swc_ast_unknown)]
use crate::unknown_error;
use crate::{text_writer::WriteJs, CowStr, Emitter, SourceMapperExt};

#[node_impl]
impl MacroNode for Lit {
    fn emit(&mut self, emitter: &mut Macro) -> Result {
        emitter.emit_leading_comments_of_span(self.span(), false)?;

        srcmap!(emitter, self, true);

        match self {
            Lit::Bool(Bool { value, .. }) => {
                if *value {
                    keyword!(emitter, "true")
                } else {
                    keyword!(emitter, "false")
                }
            }
            Lit::Null(Null { .. }) => keyword!(emitter, "null"),
            Lit::Str(ref s) => emit!(s),
            Lit::BigInt(ref s) => emit!(s),
            Lit::Num(ref n) => emit!(n),
            Lit::Regex(ref n) => {
                punct!(emitter, "/");
                // Encode non-ASCII characters in regex pattern when ascii_only is enabled
                let encoded_exp = encode_regex_for_ascii(&n.exp, emitter.cfg.ascii_only);
                emitter.wr.write_str(&encoded_exp)?;
                punct!(emitter, "/");
                emitter.wr.write_str(&n.flags)?;
            }
            Lit::JSXText(ref n) => emit!(n),
            #[cfg(swc_ast_unknown)]
            _ => return Err(unknown_error()),
        }

        Ok(())
    }
}

#[node_impl]
impl MacroNode for Str {
    fn emit(&mut self, emitter: &mut Macro) -> Result {
        emitter.wr.commit_pending_semi()?;

        emitter.emit_leading_comments_of_span(self.span(), false)?;

        srcmap!(emitter, self, true);

        if &*self.value == "use strict"
            && self.raw.is_some()
            && self.raw.as_ref().unwrap().contains('\\')
            && (!emitter.cfg.inline_script || !self.raw.as_ref().unwrap().contains("script"))
        {
            emitter
                .wr
                .write_str_lit(DUMMY_SP, self.raw.as_ref().unwrap())?;

            srcmap!(emitter, self, false);

            return Ok(());
        }

        let target = emitter.cfg.target;

        if !emitter.cfg.minify {
            if let Some(raw) = &self.raw {
                let es5_safe = match emitter.cfg.target {
                    EsVersion::Es3 | EsVersion::Es5 => {
                        // Block raw strings containing ES6+ Unicode escapes (\u{...}) for ES3/ES5
                        // targets
                        !raw.contains("\\u{")
                    }
                    _ => true,
                };

                if es5_safe
                    && (!emitter.cfg.ascii_only || raw.is_ascii())
                    && (!emitter.cfg.inline_script
                        || !self.raw.as_ref().unwrap().contains("script"))
                {
                    emitter.wr.write_str_lit(DUMMY_SP, raw)?;
                    return Ok(());
                }
            }
        }

        let (quote_char, mut value) = get_quoted_utf16(&self.value, emitter.cfg.ascii_only, target);

        if emitter.cfg.inline_script {
            value = CowStr::Owned(
                replace_close_inline_script(&value)
                    .replace("\x3c!--", "\\x3c!--")
                    .replace("--\x3e", "--\\x3e")
                    .into(),
            );
        }

        let quote_str = [quote_char.as_byte()];
        let quote_str = unsafe {
            // Safety: quote_char is valid ascii
            str::from_utf8_unchecked(&quote_str)
        };

        emitter.wr.write_str(quote_str)?;
        emitter.wr.write_str_lit(DUMMY_SP, &value)?;
        emitter.wr.write_str(quote_str)?;

        // srcmap!(emitter,self, false);

        Ok(())
    }
}

#[node_impl]
impl MacroNode for Number {
    fn emit(&mut self, emitter: &mut Macro) -> Result {
        emitter.emit_num_lit_internal(self, false)?;

        Ok(())
    }
}

#[node_impl]
impl MacroNode for BigInt {
    fn emit(&mut self, emitter: &mut Macro) -> Result {
        emitter.emit_leading_comments_of_span(self.span, false)?;

        if emitter.cfg.minify {
            let value = if *self.value >= 10000000000000000_i64.into() {
                format!("0x{}", self.value.to_str_radix(16))
            } else if *self.value <= (-10000000000000000_i64).into() {
                format!("-0x{}", (-*self.value.clone()).to_str_radix(16))
            } else {
                self.value.to_string()
            };
            emitter.wr.write_lit(self.span, &value)?;
            emitter.wr.write_lit(self.span, "n")?;
        } else {
            match &self.raw {
                Some(raw) => {
                    if raw.len() > 2 && emitter.cfg.target < EsVersion::Es2021 && raw.contains('_')
                    {
                        emitter.wr.write_str_lit(self.span, &raw.replace('_', ""))?;
                    } else {
                        emitter.wr.write_str_lit(self.span, raw)?;
                    }
                }
                _ => {
                    emitter.wr.write_lit(self.span, &self.value.to_string())?;
                    emitter.wr.write_lit(self.span, "n")?;
                }
            }
        }

        Ok(())
    }
}

#[node_impl]
impl MacroNode for Bool {
    fn emit(&mut self, emitter: &mut Macro) -> Result {
        emitter.emit_leading_comments_of_span(self.span(), false)?;

        if self.value {
            keyword!(emitter, self.span, "true")
        } else {
            keyword!(emitter, self.span, "false")
        }

        Ok(())
    }
}

pub fn replace_close_inline_script(raw: &str) -> CowStr {
    let chars = raw.as_bytes();
    let pattern_len = 8; // </script>

    let mut matched_indexes = chars
        .iter()
        .enumerate()
        .filter(|(index, byte)| {
            byte == &&b'<'
                && index + pattern_len < chars.len()
                && chars[index + 1..index + pattern_len].eq_ignore_ascii_case(b"/script")
                && matches!(
                    chars[index + pattern_len],
                    b'>' | b' ' | b'\t' | b'\n' | b'\x0C' | b'\r'
                )
        })
        .map(|(index, _)| index)
        .peekable();

    if matched_indexes.peek().is_none() {
        return CowStr::Borrowed(raw);
    }

    let mut result = CompactString::new(raw);

    for (offset, i) in matched_indexes.enumerate() {
        result.insert(i + 1 + offset, '\\');
    }

    CowStr::Owned(result)
}

impl<W, S: swc_common::SourceMapper> Emitter<'_, W, S>
where
    W: WriteJs,
    S: SourceMapperExt,
{
    /// `1.toString` is an invalid property access,
    /// should emit a dot after the literal if return true
    pub fn emit_num_lit_internal(
        &mut self,
        num: &Number,
        mut detect_dot: bool,
    ) -> std::result::Result<bool, io::Error> {
        self.wr.commit_pending_semi()?;

        self.emit_leading_comments_of_span(num.span(), false)?;

        // Handle infinity
        if num.value.is_infinite() && num.raw.is_none() {
            self.wr.write_str_lit(num.span, &num.value.print())?;

            return Ok(false);
        }

        let mut striped_raw = None;
        let mut value = String::default();

        srcmap!(self, num, true);

        if self.cfg.minify {
            if num.value.is_infinite() && num.raw.is_some() {
                self.wr.write_str_lit(DUMMY_SP, num.raw.as_ref().unwrap())?;
            } else {
                value = minify_number(num.value, &mut detect_dot);
                self.wr.write_str_lit(DUMMY_SP, &value)?;
            }
        } else {
            match &num.raw {
                Some(raw) => {
                    if raw.len() > 2 && self.cfg.target < EsVersion::Es2015 && {
                        let slice = &raw.as_bytes()[..2];
                        slice == b"0b" || slice == b"0o" || slice == b"0B" || slice == b"0O"
                    } {
                        if num.value.is_infinite() && num.raw.is_some() {
                            self.wr.write_str_lit(DUMMY_SP, num.raw.as_ref().unwrap())?;
                        } else {
                            value = num.value.print();
                            self.wr.write_str_lit(DUMMY_SP, &value)?;
                        }
                    } else if raw.len() > 2
                        && self.cfg.target < EsVersion::Es2021
                        && raw.contains('_')
                    {
                        let value = raw.replace('_', "");
                        self.wr.write_str_lit(DUMMY_SP, &value)?;

                        striped_raw = Some(value);
                    } else {
                        self.wr.write_str_lit(DUMMY_SP, raw)?;

                        if !detect_dot {
                            return Ok(false);
                        }

                        striped_raw = Some(raw.replace('_', ""));
                    }
                }
                _ => {
                    value = num.value.print();
                    self.wr.write_str_lit(DUMMY_SP, &value)?;
                }
            }
        }

        // fast return
        if !detect_dot {
            return Ok(false);
        }

        Ok(striped_raw
            .map(|raw| {
                if raw.bytes().all(|c| c.is_ascii_digit()) {
                    // Maybe legacy octal
                    // Do we really need to support pre es5?
                    let slice = raw.as_bytes();
                    if slice.len() >= 2 && slice[0] == b'0' {
                        return false;
                    }

                    return true;
                }

                false
            })
            .unwrap_or_else(|| {
                let bytes = value.as_bytes();

                if !bytes.contains(&b'.') && !bytes.contains(&b'e') {
                    return true;
                }

                false
            }))
    }
}

/// Encodes non-ASCII characters in regex patterns when ascii_only is enabled.
///
/// This function converts non-ASCII characters to their Unicode escape
/// sequences to ensure the regex can be safely used in ASCII-only contexts.
///
/// # Arguments
/// * `pattern` - The regex pattern string to encode
/// * `ascii_only` - Whether to encode non-ASCII characters
///
/// # Returns
/// A string with non-ASCII characters encoded as Unicode escapes
pub fn encode_regex_for_ascii(pattern: &str, ascii_only: bool) -> CowStr {
    if !ascii_only || pattern.is_ascii() {
        return CowStr::Borrowed(pattern);
    }

    let mut buf = CompactString::with_capacity(pattern.len());

    for c in pattern.chars() {
        match c {
            // ASCII characters are preserved as-is
            '\x00'..='\x7e' => buf.push(c),
            // Characters in the \x7f to \xff range use \xHH format
            '\u{7f}'..='\u{ff}' => {
                buf.push_str("\\x");
                write!(&mut buf, "{:02x}", c as u8).unwrap();
            }
            // Line/paragraph separators need escaping in all contexts
            '\u{2028}' => buf.push_str("\\u2028"),
            '\u{2029}' => buf.push_str("\\u2029"),
            // Characters above \xff use \uHHHH format
            _ => {
                if c > '\u{FFFF}' {
                    // Characters beyond BMP are encoded as surrogate pairs for compatibility
                    let code_point = c as u32;
                    let h = ((code_point - 0x10000) / 0x400) + 0xd800;
                    let l = ((code_point - 0x10000) % 0x400) + 0xdc00;
                    write!(&mut buf, "\\u{h:04x}\\u{l:04x}").unwrap();
                } else {
                    write!(&mut buf, "\\u{:04x}", c as u16).unwrap();
                }
            }
        }
    }

    CowStr::Owned(buf)
}

macro_rules! cp {
    ($c:expr) => {
        unsafe { CodePoint::from_u32_unchecked($c as u32) }
    };
}

const DOUBLE_QUOTE: CodePoint = cp!('"');
const SINGLE_QUOTE: CodePoint = cp!('\'');
const NULL_CHAR: CodePoint = cp!('\x00');
const BACKSPACE: CodePoint = cp!('\u{0008}');
const FORM_FEED: CodePoint = cp!('\u{000c}');
const LINE_FEED: CodePoint = cp!('\n');
const CARRIAGE_RETURN: CodePoint = cp!('\r');
const VERTICAL_TAB: CodePoint = cp!('\u{000b}');
const TAB: CodePoint = cp!('\t');
const BACKSLASH: CodePoint = cp!('\\');
const CTRL_START_1: CodePoint = cp!('\x01');
const CTRL_END_1: CodePoint = cp!('\x0f');
const CTRL_START_2: CodePoint = cp!('\x10');
const CTRL_END_2: CodePoint = cp!('\x1f');
const PRINTABLE_START: CodePoint = cp!('\x20');
const PRINTABLE_END: CodePoint = cp!('\x7e');
const DEL_START: CodePoint = cp!('\u{7f}');
const DEL_END: CodePoint = cp!('\u{ff}');
const LINE_SEPARATOR: CodePoint = cp!('\u{2028}');
const PARAGRAPH_SEPARATOR: CodePoint = cp!('\u{2029}');
const ZERO_WIDTH_NO_BREAK_SPACE: CodePoint = cp!('\u{FEFF}');

const SURROGATE_START: CodePoint = cp!(0xd800);
const SURROGATE_END: CodePoint = cp!(0xdfff);

/// Returns `(quote_char, value)`
pub fn get_quoted_utf16(v: &Wtf8, ascii_only: bool, target: EsVersion) -> (AsciiChar, CowStr) {
    // Fast path: If the string is ASCII and doesn't need escaping, we can avoid
    // allocation
    if v.is_ascii() {
        let mut needs_escaping = false;
        let mut single_quote_count = 0;
        let mut double_quote_count = 0;

        for &b in v.as_bytes() {
            match b {
                b'\'' => single_quote_count += 1,
                b'"' => double_quote_count += 1,
                // Control characters and backslash need escaping
                0..=0x1f | b'\\' => {
                    needs_escaping = true;
                    break;
                }
                _ => {}
            }
        }

        if !needs_escaping {
            let quote_char = if double_quote_count > single_quote_count {
                AsciiChar::Apostrophe
            } else {
                AsciiChar::Quotation
            };

            // If there are no quotes to escape, we can return the original string
            if (quote_char == AsciiChar::Apostrophe && single_quote_count == 0)
                || (quote_char == AsciiChar::Quotation && double_quote_count == 0)
            {
                return (
                    quote_char,
                    // SAFETY: We have checked that the string is ASCII. So it does not contain any
                    // unpaired surrogate.
                    CowStr::Borrowed(v.as_str().unwrap()),
                );
            }
        }
    }

    // Slow path: Original implementation for strings that need processing
    // Count quotes first to determine which quote character to use
    let (mut single_quote_count, mut double_quote_count) = (0, 0);
    for c in v.code_points() {
        match c {
            SINGLE_QUOTE => single_quote_count += 1,
            DOUBLE_QUOTE => double_quote_count += 1,
            _ => {}
        }
    }

    // Pre-calculate capacity to avoid reallocations
    let quote_char = if double_quote_count > single_quote_count {
        AsciiChar::Apostrophe
    } else {
        AsciiChar::Quotation
    };
    let escape_char = if quote_char == AsciiChar::Apostrophe {
        AsciiChar::Apostrophe
    } else {
        AsciiChar::Quotation
    };
    let escape_count = if quote_char == AsciiChar::Apostrophe {
        single_quote_count
    } else {
        double_quote_count
    };

    // Add 1 for each escaped quote
    let capacity = v.len() + escape_count;
    let mut buf = CompactString::with_capacity(capacity);

    let mut iter = v.code_points().peekable();
    while let Some(c) = iter.next() {
        match c {
            NULL_CHAR => {
                if target < EsVersion::Es5
                    || matches!(iter.peek(), Some(x) if *x >= cp!('0') && *x <= cp!('9'))
                {
                    buf.push_str("\\x00");
                } else {
                    buf.push_str("\\0");
                }
            }
            BACKSPACE => buf.push_str("\\b"),
            FORM_FEED => buf.push_str("\\f"),
            LINE_FEED => buf.push_str("\\n"),
            CARRIAGE_RETURN => buf.push_str("\\r"),
            VERTICAL_TAB => buf.push_str("\\v"),
            TAB => buf.push('\t'),
            BACKSLASH => buf.push_str("\\\\"),
            c if matches!(c.to_char(), Some(c) if c == escape_char) => {
                buf.push('\\');
                // SAFETY: `escape_char` is a valid ASCII character.
                buf.push(c.to_char().unwrap());
            }
            c if c >= CTRL_START_1 && c <= CTRL_END_1 => {
                buf.push_str("\\x0");
                write!(&mut buf, "{:x}", c.to_u32() as u8).unwrap();
            }
            c if c >= CTRL_START_2 && c <= CTRL_END_2 => {
                buf.push_str("\\x");
                write!(&mut buf, "{:x}", c.to_u32() as u8).unwrap();
            }
            c if c >= PRINTABLE_START && c <= PRINTABLE_END => {
                // SAFETY: c is a valid ASCII character.
                buf.push(c.to_char().unwrap())
            }
            c if c >= DEL_START && c <= DEL_END => {
                if ascii_only || target <= EsVersion::Es5 {
                    buf.push_str("\\x");
                    write!(&mut buf, "{:x}", c.to_u32() as u8).unwrap();
                } else {
                    // SAFETY: c is a valid Rust char.
                    buf.push(c.to_char().unwrap());
                }
            }
            LINE_SEPARATOR => buf.push_str("\\u2028"),
            PARAGRAPH_SEPARATOR => buf.push_str("\\u2029"),
            ZERO_WIDTH_NO_BREAK_SPACE => buf.push_str("\\uFEFF"),
            c => {
                if c.is_ascii() {
                    // SAFETY: c is a valid ASCII character.
                    buf.push(c.to_char().unwrap());
                } else if c > cp!('\u{FFFF}') {
                    if target <= EsVersion::Es5 {
                        let h = ((c.to_u32() - 0x10000) / 0x400) + 0xd800;
                        let l = (c.to_u32() - 0x10000) % 0x400 + 0xdc00;
                        write!(&mut buf, "\\u{h:04X}\\u{l:04X}").unwrap();
                    } else if ascii_only {
                        write!(&mut buf, "\\u{{{:04X}}}", c.to_u32()).unwrap();
                    } else {
                        // SAFETY: c is a valid Rust char. (> U+FFFF && <= U+10FFFF)
                        // The latter condition is guaranteed by CodePoint.
                        buf.push(c.to_char().unwrap());
                    }
                } else if c >= SURROGATE_START && c <= SURROGATE_END {
                    // Unparied Surrogate
                    // Escape as \uXXXX
                    write!(&mut buf, "\\u{:04X}", c.to_u32()).unwrap();
                } else if ascii_only {
                    write!(&mut buf, "\\u{:04X}", c.to_u32() as u16).unwrap();
                } else {
                    // SAFETY: c is a valid Rust char. (>= U+0080 && <= U+FFFF, excluding
                    // surrogates)
                    buf.push(c.to_char().unwrap());
                }
            }
        }
    }

    (quote_char, CowStr::Owned(buf))
}

pub fn minify_number(num: f64, detect_dot: &mut bool) -> String {
    // ddddd -> 0xhhhh
    // len(0xhhhh) == len(ddddd)
    // 10000000 <= num <= 0xffffff
    'hex: {
        if num.fract() == 0.0 && num.abs() <= u64::MAX as f64 {
            let int = num.abs() as u64;

            if int < 10000000 {
                break 'hex;
            }

            // use scientific notation
            if int % 1000 == 0 {
                break 'hex;
            }

            *detect_dot = false;
            return format!(
                "{}{:#x}",
                if num.is_sign_negative() { "-" } else { "" },
                int
            );
        }
    }

    let mut num = num.to_string();

    if num.contains(".") {
        *detect_dot = false;
    }

    if let Some(num) = num.strip_prefix("0.") {
        let cnt = clz(num);
        if cnt > 2 {
            return format!("{}e-{}", &num[cnt..], num.len());
        }
        return format!(".{num}");
    }

    if let Some(num) = num.strip_prefix("-0.") {
        let cnt = clz(num);
        if cnt > 2 {
            return format!("-{}e-{}", &num[cnt..], num.len());
        }
        return format!("-.{num}");
    }

    if num.ends_with("000") {
        *detect_dot = false;

        let cnt = num
            .as_bytes()
            .iter()
            .rev()
            .skip(3)
            .take_while(|&&c| c == b'0')
            .count()
            + 3;

        num.truncate(num.len() - cnt);
        num.push('e');
        num.push_str(&cnt.to_string());
    }

    num
}

fn clz(s: &str) -> usize {
    s.as_bytes().iter().take_while(|&&c| c == b'0').count()
}

pub trait Print {
    fn print(&self) -> String;
}

impl Print for f64 {
    fn print(&self) -> String {
        // preserve -0.0
        if *self == 0.0 {
            return self.to_string();
        }

        let mut buffer = dragonbox_ecma::Buffer::new();
        buffer.format(*self).to_string()
    }
}
