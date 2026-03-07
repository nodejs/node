use std::{
    borrow::Cow,
    fmt::{self, Display, Formatter},
    hash::{Hash, Hasher},
};

use is_macro::Is;
use num_bigint::BigInt as BigIntValue;
use swc_atoms::{
    wtf8::{CodePoint, Wtf8Buf},
    Atom, Wtf8Atom,
};
use swc_common::{ast_node, errors::HANDLER, util::take::Take, EqIgnoreSpan, Span, DUMMY_SP};

use crate::{jsx::JSXText, TplElement};

#[ast_node]
#[derive(Eq, Hash, EqIgnoreSpan, Is)]
#[cfg_attr(feature = "arbitrary", derive(arbitrary::Arbitrary))]
#[cfg_attr(feature = "shrink-to-fit", derive(shrink_to_fit::ShrinkToFit))]
pub enum Lit {
    #[tag("StringLiteral")]
    Str(Str),

    #[tag("BooleanLiteral")]
    Bool(Bool),

    #[tag("NullLiteral")]
    Null(Null),

    #[tag("NumericLiteral")]
    Num(Number),

    #[tag("BigIntLiteral")]
    BigInt(BigInt),

    #[tag("RegExpLiteral")]
    Regex(Regex),

    #[tag("JSXText")]
    JSXText(JSXText),
}

macro_rules! bridge_lit_from {
    ($bridge: ty, $src:ty) => {
        bridge_expr_from!(crate::Lit, $src);
        bridge_from!(Lit, $bridge, $src);
    };
}

bridge_expr_from!(Lit, Str);
bridge_expr_from!(Lit, Bool);
bridge_expr_from!(Lit, Number);
bridge_expr_from!(Lit, BigInt);
bridge_expr_from!(Lit, Regex);
bridge_expr_from!(Lit, Null);
bridge_expr_from!(Lit, JSXText);

bridge_lit_from!(Str, &'_ str);
bridge_lit_from!(Str, Atom);
bridge_lit_from!(Str, Wtf8Atom);
bridge_lit_from!(Str, Cow<'_, str>);
bridge_lit_from!(Str, String);
bridge_lit_from!(Bool, bool);
bridge_lit_from!(Number, f64);
bridge_lit_from!(Number, usize);
bridge_lit_from!(BigInt, BigIntValue);

impl Lit {
    pub fn set_span(&mut self, span: Span) {
        match self {
            Lit::Str(s) => s.span = span,
            Lit::Bool(b) => b.span = span,
            Lit::Null(n) => n.span = span,
            Lit::Num(n) => n.span = span,
            Lit::BigInt(n) => n.span = span,
            Lit::Regex(n) => n.span = span,
            Lit::JSXText(n) => n.span = span,
            #[cfg(all(swc_ast_unknown, feature = "encoding-impl"))]
            _ => swc_common::unknown!(),
        }
    }
}

#[ast_node("BigIntLiteral")]
#[derive(Eq, Hash)]
pub struct BigInt {
    pub span: Span,
    #[cfg_attr(any(feature = "rkyv-impl"), rkyv(with = EncodeBigInt))]
    #[cfg_attr(feature = "encoding-impl", encoding(with = "EncodeBigInt2"))]
    pub value: Box<BigIntValue>,

    /// Use `None` value only for transformations to avoid recalculate
    /// characters in big integer
    #[cfg_attr(
        feature = "encoding-impl",
        encoding(with = "cbor4ii::core::types::Maybe")
    )]
    pub raw: Option<Atom>,
}

#[cfg(feature = "shrink-to-fit")]
impl shrink_to_fit::ShrinkToFit for BigInt {
    #[inline(always)]
    fn shrink_to_fit(&mut self) {}
}

impl EqIgnoreSpan for BigInt {
    fn eq_ignore_span(&self, other: &Self) -> bool {
        self.value == other.value
    }
}

#[cfg(feature = "encoding-impl")]
struct EncodeBigInt2<T>(T);

#[cfg(feature = "encoding-impl")]
impl cbor4ii::core::enc::Encode for EncodeBigInt2<&'_ Box<BigIntValue>> {
    #[inline]
    fn encode<W: cbor4ii::core::enc::Write>(
        &self,
        writer: &mut W,
    ) -> Result<(), cbor4ii::core::enc::Error<W::Error>> {
        cbor4ii::core::types::Bytes(self.0.to_signed_bytes_le().as_slice()).encode(writer)
    }
}

#[cfg(feature = "encoding-impl")]
impl<'de> cbor4ii::core::dec::Decode<'de> for EncodeBigInt2<Box<BigIntValue>> {
    #[inline]
    fn decode<R: cbor4ii::core::dec::Read<'de>>(
        reader: &mut R,
    ) -> Result<Self, cbor4ii::core::dec::Error<R::Error>> {
        let buf = <cbor4ii::core::types::Bytes<&'de [u8]>>::decode(reader)?;
        Ok(EncodeBigInt2(Box::new(BigIntValue::from_signed_bytes_le(
            buf.0,
        ))))
    }
}

#[cfg(feature = "rkyv-impl")]
#[derive(Debug, Clone, Copy)]
#[cfg_attr(feature = "rkyv-impl", derive(bytecheck::CheckBytes))]
#[cfg_attr(feature = "rkyv-impl", repr(C))]
pub struct EncodeBigInt;

#[cfg(feature = "rkyv-impl")]
impl rkyv::with::ArchiveWith<Box<BigIntValue>> for EncodeBigInt {
    type Archived = rkyv::Archived<String>;
    type Resolver = rkyv::Resolver<String>;

    fn resolve_with(
        field: &Box<BigIntValue>,
        resolver: Self::Resolver,
        out: rkyv::Place<Self::Archived>,
    ) {
        use rkyv::Archive;

        let s = field.to_string();
        s.resolve(resolver, out);
    }
}

#[cfg(feature = "rkyv-impl")]
impl<S> rkyv::with::SerializeWith<Box<BigIntValue>, S> for EncodeBigInt
where
    S: ?Sized + rancor::Fallible + rkyv::ser::Writer,
    S::Error: rancor::Source,
{
    fn serialize_with(
        field: &Box<BigIntValue>,
        serializer: &mut S,
    ) -> Result<Self::Resolver, S::Error> {
        let field = field.to_string();
        rkyv::string::ArchivedString::serialize_from_str(&field, serializer)
    }
}

#[cfg(feature = "rkyv-impl")]
impl<D> rkyv::with::DeserializeWith<rkyv::Archived<String>, Box<BigIntValue>, D> for EncodeBigInt
where
    D: ?Sized + rancor::Fallible,
{
    fn deserialize_with(
        field: &rkyv::Archived<String>,
        deserializer: &mut D,
    ) -> Result<Box<BigIntValue>, D::Error> {
        use rkyv::Deserialize;

        let s: String = field.deserialize(deserializer)?;

        Ok(Box::new(s.parse().unwrap()))
    }
}

#[cfg(feature = "arbitrary")]
#[cfg_attr(docsrs, doc(cfg(feature = "arbitrary")))]
impl<'a> arbitrary::Arbitrary<'a> for BigInt {
    fn arbitrary(u: &mut arbitrary::Unstructured<'_>) -> arbitrary::Result<Self> {
        let span = u.arbitrary()?;
        let value = Box::new(u.arbitrary::<usize>()?.into());
        let raw = Some(u.arbitrary::<String>()?.into());

        Ok(Self { span, value, raw })
    }
}

impl From<BigIntValue> for BigInt {
    #[inline]
    fn from(value: BigIntValue) -> Self {
        BigInt {
            span: DUMMY_SP,
            value: Box::new(value),
            raw: None,
        }
    }
}

/// A string literal.
#[ast_node("StringLiteral")]
#[derive(Eq, Hash)]
#[cfg_attr(feature = "shrink-to-fit", derive(shrink_to_fit::ShrinkToFit))]
pub struct Str {
    pub span: Span,

    pub value: Wtf8Atom,

    /// Use `None` value only for transformations to avoid recalculate escaped
    /// characters in strings
    #[cfg_attr(
        feature = "encoding-impl",
        encoding(with = "cbor4ii::core::types::Maybe")
    )]
    pub raw: Option<Atom>,
}

impl Take for Str {
    fn dummy() -> Self {
        Str {
            span: DUMMY_SP,
            value: Wtf8Atom::default(),
            raw: None,
        }
    }
}

#[cfg(feature = "arbitrary")]
#[cfg_attr(docsrs, doc(cfg(feature = "arbitrary")))]
impl<'a> arbitrary::Arbitrary<'a> for Str {
    fn arbitrary(u: &mut arbitrary::Unstructured<'_>) -> arbitrary::Result<Self> {
        let span = u.arbitrary()?;
        let value = u.arbitrary::<Wtf8Atom>()?.into();
        let raw = Some(u.arbitrary::<String>()?.into());

        Ok(Self { span, value, raw })
    }
}

fn emit_span_error(span: Span, msg: &str) {
    HANDLER.with(|handler| {
        handler.struct_span_err(span, msg).emit();
    });
}

impl Str {
    #[inline]
    pub fn is_empty(&self) -> bool {
        self.value.is_empty()
    }

    pub fn from_tpl_raw(tpl: &TplElement) -> Wtf8Atom {
        let tpl_raw = &tpl.raw;
        let span = tpl.span;
        let mut buf: Wtf8Buf = Wtf8Buf::with_capacity(tpl_raw.len());
        let mut iter = tpl_raw.chars();
        // prev_result can only be less than 0xdc00
        // so init with 0xdc00 as no prev result
        const NO_PREV_RESULT: u32 = 0xdc00;
        let mut prev_result: u32 = NO_PREV_RESULT;
        while let Some(c) = iter.next() {
            match c {
                '\\' => {
                    if let Some(c) = iter.next() {
                        match c {
                            '`' | '$' | '\\' => {
                                buf.push_char(c);
                            }
                            'b' => {
                                buf.push_char('\u{0008}');
                            }
                            'f' => {
                                buf.push_char('\u{000C}');
                            }
                            'n' => {
                                buf.push_char('\n');
                            }
                            'r' => {
                                buf.push_char('\r');
                            }
                            't' => {
                                buf.push_char('\t');
                            }
                            'v' => {
                                buf.push_char('\u{000B}');
                            }
                            '\r' => {
                                let mut next_iter = iter.clone();
                                if let Some('\n') = next_iter.next() {
                                    iter = next_iter;
                                }
                            }
                            '\n' | '\u{2028}' | '\u{2029}' => {}
                            'u' | 'x' => {
                                let mut count: u8 = 0;
                                // result is a 4 digit hex value
                                let mut result: u32 = 0;
                                let mut max_len = if c == 'u' { 4 } else { 2 };
                                for c in &mut iter {
                                    match c {
                                        '{' if max_len == 4 && count == 0 => {
                                            max_len = 6;
                                            continue;
                                        }
                                        '}' if max_len == 6 => {
                                            break;
                                        }
                                        '0'..='9' => {
                                            result = (result << 4) | (c as u32 - '0' as u32);
                                            count += 1;
                                        }
                                        'a'..='f' => {
                                            result = (result << 4) | (c as u32 - 'a' as u32 + 10);
                                            count += 1;
                                        }
                                        'A'..='F' => {
                                            result = (result << 4) | (c as u32 - 'A' as u32 + 10);
                                            count += 1;
                                        }
                                        _ => emit_span_error(
                                            span,
                                            "Uncaught SyntaxError: Invalid Unicode escape sequence",
                                        ),
                                    }
                                    if count >= max_len {
                                        if result > 0x10ffff {
                                            emit_span_error(
                                                span,
                                                "Uncaught SyntaxError: Undefined Unicode \
                                                 code-point",
                                            )
                                        } else {
                                            break;
                                        }
                                    }
                                }
                                if max_len == 2 && max_len != count {
                                    emit_span_error(
                                        span,
                                        "Uncaught SyntaxError: Invalid hexadecimal escape sequence",
                                    );
                                }
                                if (0xd800..=0xdfff).contains(&result) {
                                    // Handle UTF-16 surrogate pair
                                    if result < 0xdc00 {
                                        // High surrogate pair
                                        if prev_result != NO_PREV_RESULT {
                                            // If the previous result is a high surrogate
                                            // We can be sure `prev_result` is less than 0xdc00
                                            buf.push(unsafe {
                                                CodePoint::from_u32_unchecked(prev_result)
                                            });
                                        }
                                        let mut iter = iter.clone();
                                        if let Some('\\') = iter.next() {
                                            if let Some('u') = iter.next() {
                                                // less than 0xdc00
                                                prev_result = result;
                                                continue;
                                            }
                                        }
                                    } else if prev_result != NO_PREV_RESULT {
                                        // Low surrogate pair
                                        // Decode to supplementary plane code point
                                        // (0x10000-0x10FFFF)
                                        result = 0x10000
                                            + ((result & 0x3ff) | ((prev_result & 0x3ff) << 10));
                                        // We can be sure result is a valid code point here
                                        buf.push(unsafe { CodePoint::from_u32_unchecked(result) });
                                        prev_result = NO_PREV_RESULT;
                                        continue;
                                    }
                                }
                                if prev_result != NO_PREV_RESULT {
                                    // Could not find a valid low surrogate pair
                                    // We can be sure `prev_result` is less than 0xdc00
                                    buf.push(unsafe { CodePoint::from_u32_unchecked(prev_result) });
                                    prev_result = NO_PREV_RESULT;
                                }
                                if result <= 0x10ffff {
                                    // We can be sure result is a valid code point here
                                    buf.push(unsafe { CodePoint::from_u32_unchecked(result) });
                                } else {
                                    emit_span_error(
                                        span,
                                        "Uncaught SyntaxError: Undefined Unicode code-point",
                                    );
                                }
                            }
                            '0'..='7' => {
                                let next = iter.clone().next();
                                if c == '0' {
                                    match next {
                                        Some(next) => {
                                            if !next.is_digit(8) {
                                                buf.push_char('\u{0000}');
                                                continue;
                                            }
                                        }
                                        // \0 is not an octal literal nor decimal literal.
                                        _ => {
                                            buf.push_char('\u{0000}');
                                            continue;
                                        }
                                    }
                                }
                                emit_span_error(
                                    span,
                                    "Uncaught SyntaxError: Octal escape sequences are not allowed \
                                     in template strings.",
                                );
                            }
                            _ => {
                                // output raw value when this is not supported
                                buf.push_char(c);
                            }
                        }
                    }
                }

                c => {
                    buf.push_char(c);
                }
            }
        }

        buf.into()
    }
}

impl EqIgnoreSpan for Str {
    fn eq_ignore_span(&self, other: &Self) -> bool {
        self.value == other.value
    }
}

impl From<Atom> for Str {
    #[inline]
    fn from(value: Atom) -> Self {
        Str {
            span: DUMMY_SP,
            value: value.into(),
            raw: None,
        }
    }
}

impl From<Wtf8Atom> for Str {
    #[inline]
    fn from(value: Wtf8Atom) -> Self {
        Str {
            span: DUMMY_SP,
            value,
            raw: None,
        }
    }
}

bridge_from!(Str, Atom, &'_ str);
bridge_from!(Str, Atom, String);
bridge_from!(Str, Atom, Cow<'_, str>);

/// A boolean literal.
///
///
/// # Creation
///
/// If you are creating a boolean literal with a dummy span, please use
/// `true.into()` or `false.into()`, instead of creating this struct directly.
///
/// All of `Box<Expr>`, `Expr`, `Lit`, `Bool` implements `From<bool>`.
#[ast_node("BooleanLiteral")]
#[derive(Copy, Eq, Hash, EqIgnoreSpan)]
#[cfg_attr(feature = "arbitrary", derive(arbitrary::Arbitrary))]
#[cfg_attr(feature = "shrink-to-fit", derive(shrink_to_fit::ShrinkToFit))]
pub struct Bool {
    pub span: Span,
    pub value: bool,
}

impl Take for Bool {
    fn dummy() -> Self {
        Bool {
            span: DUMMY_SP,
            value: false,
        }
    }
}

impl From<bool> for Bool {
    #[inline]
    fn from(value: bool) -> Self {
        Bool {
            span: DUMMY_SP,
            value,
        }
    }
}

#[ast_node("NullLiteral")]
#[derive(Copy, Eq, Hash, EqIgnoreSpan)]
#[cfg_attr(feature = "arbitrary", derive(arbitrary::Arbitrary))]
#[cfg_attr(feature = "shrink-to-fit", derive(shrink_to_fit::ShrinkToFit))]
pub struct Null {
    pub span: Span,
}

impl Take for Null {
    fn dummy() -> Self {
        Null { span: DUMMY_SP }
    }
}

#[ast_node("RegExpLiteral")]
#[derive(Eq, Hash, EqIgnoreSpan)]
#[cfg_attr(feature = "shrink-to-fit", derive(shrink_to_fit::ShrinkToFit))]
pub struct Regex {
    pub span: Span,

    #[cfg_attr(feature = "serde-impl", serde(rename = "pattern"))]
    pub exp: Atom,

    #[cfg_attr(feature = "serde-impl", serde(default))]
    pub flags: Atom,
}

impl Take for Regex {
    fn dummy() -> Self {
        Self {
            span: DUMMY_SP,
            exp: Default::default(),
            flags: Default::default(),
        }
    }
}

#[cfg(feature = "arbitrary")]
#[cfg_attr(docsrs, doc(cfg(feature = "arbitrary")))]
impl<'a> arbitrary::Arbitrary<'a> for Regex {
    fn arbitrary(u: &mut arbitrary::Unstructured<'_>) -> arbitrary::Result<Self> {
        use swc_atoms::atom;

        let span = u.arbitrary()?;
        let exp = u.arbitrary::<String>()?.into();
        let flags = atom!(""); // TODO

        Ok(Self { span, exp, flags })
    }
}

/// A numeric literal.
///
///
/// # Creation
///
/// If you are creating a numeric literal with a dummy span, please use
/// `literal.into()`, instead of creating this struct directly.
///
/// All of `Box<Expr>`, `Expr`, `Lit`, `Number` implements `From<64>` and
/// `From<usize>`.

#[ast_node("NumericLiteral")]
#[cfg_attr(feature = "shrink-to-fit", derive(shrink_to_fit::ShrinkToFit))]
pub struct Number {
    pub span: Span,
    /// **Note**: This should not be `NaN`. Use [crate::Ident] to represent NaN.
    ///
    /// If you store `NaN` in this field, a hash map will behave strangely.
    pub value: f64,

    /// Use `None` value only for transformations to avoid recalculate
    /// characters in number literal
    #[cfg_attr(
        feature = "encoding-impl",
        encoding(with = "cbor4ii::core::types::Maybe")
    )]
    pub raw: Option<Atom>,
}

impl Eq for Number {}

impl EqIgnoreSpan for Number {
    fn eq_ignore_span(&self, other: &Self) -> bool {
        self.value == other.value && self.value.is_sign_positive() == other.value.is_sign_positive()
    }
}

#[allow(clippy::derived_hash_with_manual_eq)]
#[allow(clippy::transmute_float_to_int)]
impl Hash for Number {
    fn hash<H: Hasher>(&self, state: &mut H) {
        fn integer_decode(val: f64) -> (u64, i16, i8) {
            let bits: u64 = val.to_bits();
            let sign: i8 = if bits >> 63 == 0 { 1 } else { -1 };
            let mut exponent: i16 = ((bits >> 52) & 0x7ff) as i16;
            let mantissa = if exponent == 0 {
                (bits & 0xfffffffffffff) << 1
            } else {
                (bits & 0xfffffffffffff) | 0x10000000000000
            };

            exponent -= 1023 + 52;
            (mantissa, exponent, sign)
        }

        self.span.hash(state);
        integer_decode(self.value).hash(state);
        self.raw.hash(state);
    }
}

impl Display for Number {
    fn fmt(&self, f: &mut Formatter<'_>) -> fmt::Result {
        if self.value.is_infinite() {
            if self.value.is_sign_positive() {
                Display::fmt("Infinity", f)
            } else {
                Display::fmt("-Infinity", f)
            }
        } else {
            Display::fmt(&self.value, f)
        }
    }
}

#[cfg(feature = "arbitrary")]
#[cfg_attr(docsrs, doc(cfg(feature = "arbitrary")))]
impl<'a> arbitrary::Arbitrary<'a> for Number {
    fn arbitrary(u: &mut arbitrary::Unstructured<'_>) -> arbitrary::Result<Self> {
        let span = u.arbitrary()?;
        let value = u.arbitrary::<f64>()?;
        let raw = Some(u.arbitrary::<String>()?.into());

        Ok(Self { span, value, raw })
    }
}

impl From<f64> for Number {
    #[inline]
    fn from(value: f64) -> Self {
        Number {
            span: DUMMY_SP,
            value,
            raw: None,
        }
    }
}

impl From<usize> for Number {
    #[inline]
    fn from(value: usize) -> Self {
        Number {
            span: DUMMY_SP,
            value: value as _,
            raw: None,
        }
    }
}
