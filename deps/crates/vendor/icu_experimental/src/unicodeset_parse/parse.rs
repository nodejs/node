// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

use alloc::borrow::Cow;
use alloc::collections::{BTreeMap, BTreeSet};
use alloc::format;
use alloc::string::{String, ToString};
use alloc::vec::Vec;
use core::fmt::Display;
use core::{iter::Peekable, str::CharIndices};

use icu_collections::{
    codepointinvlist::{CodePointInversionList, CodePointInversionListBuilder},
    codepointinvliststringlist::CodePointInversionListAndStringList,
};
use icu_properties::script::ScriptWithExtensions;
use icu_properties::{
    props::{
        CanonicalCombiningClass, EnumeratedProperty, GeneralCategory, GeneralCategoryGroup,
        GraphemeClusterBreak, LineBreak, Script, SentenceBreak, WordBreak,
    },
    CodePointMapData,
};
use icu_properties::{
    props::{PatternWhiteSpace, XidContinue, XidStart},
    CodePointSetData,
};
use icu_properties::{provider::*, PropertyParser};
use icu_provider::prelude::*;

/// The kind of error that occurred.
#[derive(Debug, Clone, Copy, PartialEq, Eq, displaydoc::Display)]
#[non_exhaustive]
pub enum ParseErrorKind {
    /// An unexpected character was encountered.
    ///
    /// This variant implies the other variants
    /// (notably `UnknownProperty` and `Unimplemented`) do not apply.
    #[displaydoc("An unexpected character was encountered")]
    UnexpectedChar(char),
    /// The property name or value is unknown.
    ///
    /// For property names, make sure you use the spelling
    /// defined in [ECMA-262](https://tc39.es/ecma262/#table-nonbinary-unicode-properties).
    #[displaydoc("The property name or value is unknown")]
    UnknownProperty,
    /// A reference to an unknown variable.
    UnknownVariable,
    /// A variable of a certain type occurring in an unexpected context.
    UnexpectedVariable,
    /// The source is an incomplete unicode set.
    Eof,
    /// Something unexpected went wrong with our code. Please file a bug report on GitHub.
    Internal,
    /// The provided syntax is not supported by us.
    ///
    /// Note that unknown properties will return the
    /// `UnknownProperty` variant, not this one.
    #[displaydoc("The provided syntax is not supported by us.")]
    Unimplemented,
    /// The provided escape sequence is not a valid Unicode code point or represents too many code points.
    InvalidEscape,
}
use zerovec::VarZeroVec;
use ParseErrorKind as PEK;

impl ParseErrorKind {
    fn with_offset(self, offset: usize) -> ParseError {
        ParseError {
            offset: Some(offset),
            kind: self,
        }
    }
}

impl From<ParseErrorKind> for ParseError {
    fn from(kind: ParseErrorKind) -> Self {
        ParseError { offset: None, kind }
    }
}

/// The error type returned by the `parse` functions in this crate.
///
/// See [`ParseError::fmt_with_source`] for pretty-printing and [`ParseErrorKind`] of the
/// different types of errors represented by this struct.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub struct ParseError {
    // offset is the index to an arbitrary byte in the last character in the source that makes sense
    // to display as location for the error, e.g., the unexpected character itself or
    // for an unknown property name the last character of the name.
    offset: Option<usize>,
    kind: ParseErrorKind,
}

type Result<T, E = ParseError> = core::result::Result<T, E>;

impl ParseError {
    /// Pretty-prints this error and if applicable, shows where the error occurred in the source.
    ///
    /// Must be called with the same source that was used to parse the set.
    ///
    /// # Examples
    ///
    /// ```
    /// use icu::experimental::unicodeset_parse::*;
    ///
    /// let source = "[[abc]-x]";
    /// let set = parse(source);
    /// assert!(set.is_err());
    /// let err = set.unwrap_err();
    /// assert_eq!(
    ///     err.fmt_with_source(source).to_string(),
    ///     "[[abc]-x← error: unexpected character 'x'"
    /// );
    /// ```
    ///
    /// ```
    /// use icu::experimental::unicodeset_parse::*;
    ///
    /// let source = r"[\N{LATIN CAPITAL LETTER A}]";
    /// let set = parse(source);
    /// assert!(set.is_err());
    /// let err = set.unwrap_err();
    /// assert_eq!(
    ///     err.fmt_with_source(source).to_string(),
    ///     r"[\N← error: unimplemented"
    /// );
    /// ```
    pub fn fmt_with_source(&self, source: &str) -> impl Display {
        let ParseError { offset, kind } = *self;

        if kind == ParseErrorKind::Eof {
            return format!("{source}← error: unexpected end of input");
        }
        let mut s = String::new();
        if let Some(offset) = offset {
            if offset < source.len() {
                // offset points to any byte of the last character we want to display.
                // in the case of ASCII, this is easy - we just display bytes [..=offset].
                // however, if the last character is more than one byte in UTF-8
                // we cannot use ..=offset, because that would potentially include only partial
                // bytes of last character in our string. hence we must find the start of the
                // following character and use that as the (exclusive) end of our string.

                // offset points into the last character we want to include, hence the start of the
                // first character we want to exclude is at least offset + 1.
                let mut exclusive_end = offset + 1;
                // TODO: replace this loop with str::ceil_char_boundary once stable
                for _ in 0..3 {
                    // is_char_boundary returns true at the latest once exclusive_end == source.len()
                    if source.is_char_boundary(exclusive_end) {
                        break;
                    }
                    exclusive_end += 1;
                }

                // exclusive_end is at most source.len() due to str::is_char_boundary and at least 0 by type
                s.push_str(&source[..exclusive_end]);
                s.push_str("← ");
            }
        }
        s.push_str("error: ");
        match kind {
            ParseErrorKind::UnexpectedChar(c) => {
                s.push_str(&format!("unexpected character '{}'", c.escape_debug()));
            }
            ParseErrorKind::UnknownProperty => {
                s.push_str("unknown property");
            }
            ParseErrorKind::UnknownVariable => {
                s.push_str("unknown variable");
            }
            ParseErrorKind::UnexpectedVariable => {
                s.push_str("unexpected variable");
            }
            ParseErrorKind::Eof => {
                s.push_str("unexpected end of input");
            }
            ParseErrorKind::Internal => {
                s.push_str("internal error");
            }
            ParseErrorKind::Unimplemented => {
                s.push_str("unimplemented");
            }
            ParseErrorKind::InvalidEscape => {
                s.push_str("invalid escape sequence");
            }
        }

        s
    }

    /// Returns the [`ParseErrorKind`] of this error.
    pub fn kind(&self) -> ParseErrorKind {
        self.kind
    }

    /// Returns the offset of this error in the source string, if it was specified.
    pub fn offset(&self) -> Option<usize> {
        self.offset
    }

    fn or_with_offset(self, offset: usize) -> Self {
        match self.offset {
            Some(_) => self,
            None => ParseError {
                offset: Some(offset),
                ..self
            },
        }
    }
}

/// The value of a variable in a `UnicodeSet`. Used as value type in [`VariableMap`].
#[derive(Debug, Clone)]
#[non_exhaustive]
pub enum VariableValue<'a> {
    /// A `UnicodeSet`, represented as a [`CodePointInversionListAndStringList`](CodePointInversionListAndStringList).
    UnicodeSet(CodePointInversionListAndStringList<'a>),
    // in theory, a one-code-point string is always the same as a char, but we might want to keep
    // this variant for efficiency?
    /// A single code point.
    Char(char),
    /// A string. It is guaranteed that when returned from a `VariableMap`, this variant contains never exactly one code point.
    String(Cow<'a, str>),
}

/// The map used for parsing `UnicodeSets` with variable support. See [`parse_with_variables`].
#[derive(Debug, Clone, Default)]
pub struct VariableMap<'a>(BTreeMap<String, VariableValue<'a>>);

impl<'a> VariableMap<'a> {
    /// Creates a new empty map.
    pub fn new() -> Self {
        Self::default()
    }

    /// Removes a key from the map, returning the value at the key if the key
    /// was previously in the map.
    pub fn remove(&mut self, key: &str) -> Option<VariableValue<'a>> {
        self.0.remove(key)
    }

    /// Get a reference to the value associated with this key, if it exists.
    pub fn get(&self, key: &str) -> Option<&VariableValue<'a>> {
        self.0.get(key)
    }

    /// Insert a `VariableValue` into the `VariableMap`.
    ///
    /// Returns `Err` with the old value, if it exists, and does not update the map.
    pub fn insert(
        &mut self,
        key: String,
        value: VariableValue<'a>,
    ) -> Result<(), &VariableValue<'_>> {
        // borrow-checker shenanigans, otherwise we could use if let
        if self.0.contains_key(&key) {
            // we just checked that this key exists
            #[expect(clippy::indexing_slicing)]
            return Err(&self.0[&key]);
        }

        if let VariableValue::String(s) = &value {
            let mut chars = s.chars();
            if let (Some(c), None) = (chars.next(), chars.next()) {
                self.0.insert(key, VariableValue::Char(c));
                return Ok(());
            };
        }

        self.0.insert(key, value);
        Ok(())
    }

    /// Insert a `char` into the `VariableMap`.    
    ///
    /// Returns `Err` with the old value, if it exists, and does not update the map.
    pub fn insert_char(&mut self, key: String, c: char) -> Result<(), &VariableValue<'_>> {
        // borrow-checker shenanigans, otherwise we could use if let
        if self.0.contains_key(&key) {
            // we just checked that this key exists
            #[expect(clippy::indexing_slicing)]
            return Err(&self.0[&key]);
        }

        self.0.insert(key, VariableValue::Char(c));
        Ok(())
    }

    /// Insert a `String` of any length into the `VariableMap`.
    ///
    /// Returns `Err` with the old value, if it exists, and does not update the map.
    pub fn insert_string(&mut self, key: String, s: String) -> Result<(), &VariableValue<'_>> {
        // borrow-checker shenanigans, otherwise we could use if let
        if self.0.contains_key(&key) {
            // we just checked that this key exists
            #[expect(clippy::indexing_slicing)]
            return Err(&self.0[&key]);
        }

        let mut chars = s.chars();
        let val = match (chars.next(), chars.next()) {
            (Some(c), None) => VariableValue::Char(c),
            _ => VariableValue::String(Cow::Owned(s)),
        };

        self.0.insert(key, val);
        Ok(())
    }

    /// Insert a `&str` of any length into the `VariableMap`.
    ///
    /// Returns `Err` with the old value, if it exists, and does not update the map.
    pub fn insert_str(&mut self, key: String, s: &'a str) -> Result<(), &VariableValue<'_>> {
        // borrow-checker shenanigans, otherwise we could use if let
        if self.0.contains_key(&key) {
            // we just checked that this key exists
            #[expect(clippy::indexing_slicing)]
            return Err(&self.0[&key]);
        }

        let mut chars = s.chars();
        let val = match (chars.next(), chars.next()) {
            (Some(c), None) => VariableValue::Char(c),
            _ => VariableValue::String(Cow::Borrowed(s)),
        };

        self.0.insert(key, val);
        Ok(())
    }

    /// Insert a [`CodePointInversionListAndStringList`](CodePointInversionListAndStringList) into the `VariableMap`.
    ///
    /// Returns `Err` with the old value, if it exists, and does not update the map.
    pub fn insert_set(
        &mut self,
        key: String,
        set: CodePointInversionListAndStringList<'a>,
    ) -> Result<(), &VariableValue<'_>> {
        // borrow-checker shenanigans, otherwise we could use if let
        if self.0.contains_key(&key) {
            // we just checked that this key exists
            #[expect(clippy::indexing_slicing)]
            return Err(&self.0[&key]);
        }
        self.0.insert(key, VariableValue::UnicodeSet(set));
        Ok(())
    }
}

// this ignores the ambiguity between \-escapes and \p{} perl properties. it assumes it is in a context where \p is just 'p'
// returns whether the provided char signifies the start of a literal char (raw or escaped - so \ is a legal char start)
// important: assumes c is not pattern_white_space
fn legal_char_start(c: char) -> bool {
    !(c == '&' || c == '-' || c == '$' || c == '^' || c == '[' || c == ']' || c == '{')
}

// same as `legal_char_start` but adapted to the charInString nonterminal. \ is allowed due to escapes.
// important: assumes c is not pattern_white_space
fn legal_char_in_string_start(c: char) -> bool {
    c != '}'
}

#[derive(Debug)]
enum SingleOrMultiChar {
    Single(char),
    // Multi is a marker that indicates parsing was paused and needs to be resumed using parse_multi_escape* when
    // this token is consumed. The contained char is the first char of the multi sequence.
    Multi(char),
}

// A char or a string. The Vec<char> represents multi-escapes in the 2+ case.
// invariant: a String is either zero or 2+ chars long, a one-char-string is equivalent to a single char.
// invariant: a char is 1+ chars long
#[derive(Debug)]
enum Literal {
    String(String),
    CharKind(SingleOrMultiChar),
}

#[derive(Debug)]
enum MainToken<'data> {
    // to be interpreted as value
    Literal(Literal),
    // inner set
    UnicodeSet(CodePointInversionListAndStringList<'data>),
    // anchor, only at the end of a set ([... $])
    DollarSign,
    // intersection operator, only inbetween two sets ([[...] & [...]])
    Ampersand,
    // difference operator, only inbetween two sets ([[...] - [...]])
    // or
    // range operator, only inbetween two chars ([a-z], [a-{z}])
    Minus,
    // ] to indicate the end of a set
    ClosingBracket,
}

impl<'data> MainToken<'data> {
    fn from_variable_value(val: VariableValue<'data>) -> Self {
        match val {
            VariableValue::Char(c) => {
                MainToken::Literal(Literal::CharKind(SingleOrMultiChar::Single(c)))
            }
            VariableValue::String(s) => {
                // we know that the VariableMap only contains non-length-1 Strings.
                MainToken::Literal(Literal::String(s.into_owned()))
            }
            VariableValue::UnicodeSet(set) => MainToken::UnicodeSet(set),
        }
    }
}

#[derive(Debug, Clone, Copy)]
enum Operation {
    Union,
    Difference,
    Intersection,
}

// this builds the set on-the-fly while parsing it
struct UnicodeSetBuilder<'a, 'b, P: ?Sized> {
    single_set: CodePointInversionListBuilder,
    string_set: BTreeSet<String>,
    iter: &'a mut Peekable<CharIndices<'b>>,
    source: &'b str,
    inverted: bool,
    variable_map: &'a VariableMap<'a>,
    xid_start: &'a CodePointInversionList<'a>,
    xid_continue: &'a CodePointInversionList<'a>,
    pat_ws: &'a CodePointInversionList<'a>,
    property_provider: &'a P,
}

impl<'a, 'b, P> UnicodeSetBuilder<'a, 'b, P>
where
    P: ?Sized
        + DataProvider<PropertyBinaryAlphabeticV1>
        + DataProvider<PropertyBinaryAsciiHexDigitV1>
        + DataProvider<PropertyBinaryBidiControlV1>
        + DataProvider<PropertyBinaryBidiMirroredV1>
        + DataProvider<PropertyBinaryCasedV1>
        + DataProvider<PropertyBinaryCaseIgnorableV1>
        + DataProvider<PropertyBinaryChangesWhenCasefoldedV1>
        + DataProvider<PropertyBinaryChangesWhenCasemappedV1>
        + DataProvider<PropertyBinaryChangesWhenLowercasedV1>
        + DataProvider<PropertyBinaryChangesWhenNfkcCasefoldedV1>
        + DataProvider<PropertyBinaryChangesWhenTitlecasedV1>
        + DataProvider<PropertyBinaryChangesWhenUppercasedV1>
        + DataProvider<PropertyBinaryDashV1>
        + DataProvider<PropertyBinaryDefaultIgnorableCodePointV1>
        + DataProvider<PropertyBinaryDeprecatedV1>
        + DataProvider<PropertyBinaryDiacriticV1>
        + DataProvider<PropertyBinaryEmojiComponentV1>
        + DataProvider<PropertyBinaryEmojiModifierBaseV1>
        + DataProvider<PropertyBinaryEmojiModifierV1>
        + DataProvider<PropertyBinaryEmojiPresentationV1>
        + DataProvider<PropertyBinaryEmojiV1>
        + DataProvider<PropertyBinaryExtendedPictographicV1>
        + DataProvider<PropertyBinaryExtenderV1>
        + DataProvider<PropertyBinaryGraphemeBaseV1>
        + DataProvider<PropertyBinaryGraphemeExtendV1>
        + DataProvider<PropertyBinaryHexDigitV1>
        + DataProvider<PropertyBinaryIdContinueV1>
        + DataProvider<PropertyBinaryIdeographicV1>
        + DataProvider<PropertyBinaryIdsBinaryOperatorV1>
        + DataProvider<PropertyBinaryIdStartV1>
        + DataProvider<PropertyBinaryIdsTrinaryOperatorV1>
        + DataProvider<PropertyBinaryJoinControlV1>
        + DataProvider<PropertyBinaryLogicalOrderExceptionV1>
        + DataProvider<PropertyBinaryLowercaseV1>
        + DataProvider<PropertyBinaryMathV1>
        + DataProvider<PropertyBinaryNoncharacterCodePointV1>
        + DataProvider<PropertyBinaryPatternSyntaxV1>
        + DataProvider<PropertyBinaryPatternWhiteSpaceV1>
        + DataProvider<PropertyBinaryQuotationMarkV1>
        + DataProvider<PropertyBinaryRadicalV1>
        + DataProvider<PropertyBinaryRegionalIndicatorV1>
        + DataProvider<PropertyBinarySentenceTerminalV1>
        + DataProvider<PropertyBinarySoftDottedV1>
        + DataProvider<PropertyBinaryTerminalPunctuationV1>
        + DataProvider<PropertyBinaryUnifiedIdeographV1>
        + DataProvider<PropertyBinaryUppercaseV1>
        + DataProvider<PropertyBinaryVariationSelectorV1>
        + DataProvider<PropertyBinaryWhiteSpaceV1>
        + DataProvider<PropertyBinaryXidContinueV1>
        + DataProvider<PropertyBinaryXidStartV1>
        + DataProvider<PropertyEnumCanonicalCombiningClassV1>
        + DataProvider<PropertyEnumGeneralCategoryV1>
        + DataProvider<PropertyEnumGraphemeClusterBreakV1>
        + DataProvider<PropertyEnumLineBreakV1>
        + DataProvider<PropertyEnumScriptV1>
        + DataProvider<PropertyEnumSentenceBreakV1>
        + DataProvider<PropertyEnumWordBreakV1>
        + DataProvider<PropertyNameParseCanonicalCombiningClassV1>
        + DataProvider<PropertyNameParseGeneralCategoryMaskV1>
        + DataProvider<PropertyNameParseGraphemeClusterBreakV1>
        + DataProvider<PropertyNameParseLineBreakV1>
        + DataProvider<PropertyNameParseScriptV1>
        + DataProvider<PropertyNameParseSentenceBreakV1>
        + DataProvider<PropertyNameParseWordBreakV1>
        + DataProvider<PropertyScriptWithExtensionsV1>,
{
    fn new_internal(
        iter: &'a mut Peekable<CharIndices<'b>>,
        source: &'b str,
        variable_map: &'a VariableMap<'a>,
        xid_start: &'a CodePointInversionList<'a>,
        xid_continue: &'a CodePointInversionList<'a>,
        pat_ws: &'a CodePointInversionList<'a>,
        provider: &'a P,
    ) -> Self {
        UnicodeSetBuilder {
            single_set: CodePointInversionListBuilder::new(),
            string_set: Default::default(),
            iter,
            source,
            inverted: false,
            variable_map,
            xid_start,
            xid_continue,
            pat_ws,
            property_provider: provider,
        }
    }

    // the entry point, parses a full UnicodeSet. ignores remaining input
    fn parse_unicode_set(&mut self) -> Result<()> {
        match self.must_peek_char()? {
            '\\' => self.parse_property_perl(),
            '[' => {
                self.iter.next();
                if let Some(':') = self.peek_char() {
                    self.parse_property_posix()
                } else {
                    self.parse_unicode_set_inner()
                }
            }
            '$' => {
                // must be variable ref to a UnicodeSet
                let (offset, v) = self.parse_variable()?;
                match v {
                    Some(VariableValue::UnicodeSet(s)) => {
                        self.single_set.add_set(s.code_points());
                        self.string_set
                            .extend(s.strings().iter().map(ToString::to_string));
                        Ok(())
                    }
                    Some(_) => Err(PEK::UnexpectedVariable.with_offset(offset)),
                    None => Err(PEK::UnexpectedChar('$').with_offset(offset)),
                }
            }
            c => self.error_here(PEK::UnexpectedChar(c)),
        }
    }

    // beginning [ is already consumed
    fn parse_unicode_set_inner(&mut self) -> Result<()> {
        // special cases for the first chars after [
        if self.must_peek_char()? == '^' {
            self.iter.next();
            self.inverted = true;
        }
        // whitespace allowed between ^ and - in `[^ - ....]`
        self.skip_whitespace();
        if self.must_peek_char()? == '-' {
            self.iter.next();
            self.single_set.add_char('-');
        }

        // repeatedly parse the following:
        // char
        // char-char
        // {string}
        // unicodeset
        // & and - operators, but only between unicodesets
        // $variables in place of strings, chars, or unicodesets

        #[derive(Debug, Clone, Copy)]
        enum State {
            // a state equivalent to the beginning
            Begin,
            // a state after a char. implies `prev_char` is Some(_), because we need to buffer it
            // in case it is part of a range, e.g., a-z
            Char,
            // in the middle of parsing a range. implies `prev_char` is Some(_), and the next
            // element must be a char as well
            CharMinus,
            // state directly after parsing a recursive unicode set. operators are only allowed
            // in this state
            AfterUnicodeSet,
            // state directly after parsing an operator. forces the next element to be a recursive
            // unicode set
            AfterOp,
            // state after parsing a $ (that was not a variable reference)
            // the only valid next option is a closing bracket
            AfterDollar,
            // state after parsing a - in an otherwise invalid position
            // the only valid next option is a closing bracket
            AfterMinus,
        }
        use State::*;

        const DEFAULT_OP: Operation = Operation::Union;

        let mut state = Begin;
        let mut prev_char = None;
        let mut operation = Operation::Union;

        loop {
            self.skip_whitespace();

            // for error messages
            let (immediate_offset, immediate_char) = self.must_peek()?;

            let (tok_offset, from_var, tok) = self.parse_main_token()?;
            // warning: self.iter should not be advanced any more after this point on any path to
            // MT::Literal(Literal::CharKind(SingleOrMultiChar::Multi)), because that variant
            // expects a certain self.iter state

            use MainToken as MT;
            use SingleOrMultiChar as SMC;
            match (state, tok) {
                // the end of this unicode set
                (
                    Begin | Char | CharMinus | AfterUnicodeSet | AfterDollar | AfterMinus,
                    MT::ClosingBracket,
                ) => {
                    if let Some(prev) = prev_char.take() {
                        self.single_set.add_char(prev);
                    }
                    if matches!(state, CharMinus) {
                        self.single_set.add_char('-');
                    }

                    return Ok(());
                }
                // special case ends for -
                // [[a-z]-]
                (AfterOp, MT::ClosingBracket) if matches!(operation, Operation::Difference) => {
                    self.single_set.add_char('-');
                    return Ok(());
                }
                (Begin, MT::Minus) => {
                    self.single_set.add_char('-');
                    state = AfterMinus;
                }
                // inner unicode set
                (Begin | Char | AfterUnicodeSet | AfterOp, MT::UnicodeSet(set)) => {
                    if let Some(prev) = prev_char.take() {
                        self.single_set.add_char(prev);
                    }

                    self.process_chars(operation, set.code_points().clone());
                    self.process_strings(
                        operation,
                        set.strings().iter().map(ToString::to_string).collect(),
                    );

                    operation = DEFAULT_OP;
                    state = AfterUnicodeSet;
                }
                // a literal char (either individually or as the start of a range if char)
                (
                    Begin | Char | AfterUnicodeSet,
                    MT::Literal(Literal::CharKind(SMC::Single(c))),
                ) => {
                    if let Some(prev) = prev_char.take() {
                        self.single_set.add_char(prev);
                    }
                    prev_char = Some(c);
                    state = Char;
                }
                // a bunch of literal chars as part of a multi-escape sequence
                (
                    Begin | Char | AfterUnicodeSet,
                    MT::Literal(Literal::CharKind(SMC::Multi(first_c))),
                ) => {
                    if let Some(prev) = prev_char.take() {
                        self.single_set.add_char(prev);
                    }
                    self.single_set.add_char(first_c);
                    self.parse_multi_escape_into_set()?;

                    // Note we cannot go to the Char state, because a multi-escape sequence of
                    // length > 1 cannot initiate a range
                    state = Begin;
                }
                // a literal string (length != 1, by CharOrString invariant)
                (Begin | Char | AfterUnicodeSet, MT::Literal(Literal::String(s))) => {
                    if let Some(prev) = prev_char.take() {
                        self.single_set.add_char(prev);
                    }

                    self.string_set.insert(s);
                    state = Begin;
                }
                // parse a literal char as the end of a range
                (CharMinus, MT::Literal(Literal::CharKind(SMC::Single(c)))) => {
                    let start = prev_char.ok_or(ParseError {
                        offset: Some(tok_offset),
                        kind: PEK::Internal,
                    })?;
                    let end = c;
                    if start > end {
                        // TODO(#3558): Better error message (e.g., "start greater than end in range")?
                        return Err(PEK::UnexpectedChar(end).with_offset(tok_offset));
                    }

                    self.single_set.add_range(start..=end);
                    prev_char = None;
                    state = Begin;
                }
                // start parsing a char range
                (Char, MT::Minus) => {
                    state = CharMinus;
                }
                // start parsing a unicode set difference
                (AfterUnicodeSet, MT::Minus) => {
                    operation = Operation::Difference;
                    state = AfterOp;
                }
                // start parsing a unicode set difference
                (AfterUnicodeSet, MT::Ampersand) => {
                    operation = Operation::Intersection;
                    state = AfterOp;
                }
                (Begin | Char | AfterUnicodeSet, MT::DollarSign) => {
                    if let Some(prev) = prev_char.take() {
                        self.single_set.add_char(prev);
                    }
                    self.single_set.add_char('\u{FFFF}');
                    state = AfterDollar;
                }
                _ => {
                    // TODO(#3558): We have precise knowledge about the following MainToken here,
                    //  should we make use of that?

                    if from_var {
                        // otherwise we get error messages such as
                        // [$a-$← error: unexpected character '$'
                        // for input [$a-$b], $a = 'a', $b = "string" ;
                        return Err(PEK::UnexpectedVariable.with_offset(tok_offset));
                    }
                    return Err(PEK::UnexpectedChar(immediate_char).with_offset(immediate_offset));
                }
            }
        }
    }

    fn parse_main_token(&mut self) -> Result<(usize, bool, MainToken<'a>)> {
        let (initial_offset, first) = self.must_peek()?;
        if first == ']' {
            self.iter.next();
            return Ok((initial_offset, false, MainToken::ClosingBracket));
        }
        let (_, second) = self.must_peek_double()?;
        match (first, second) {
            // variable or anchor
            ('$', _) => {
                let (offset, var_or_anchor) = self.parse_variable()?;
                match var_or_anchor {
                    None => Ok((offset, false, MainToken::DollarSign)),
                    Some(v) => Ok((offset, true, MainToken::from_variable_value(v.clone()))),
                }
            }
            // string
            ('{', _) => self
                .parse_string()
                .map(|(offset, l)| (offset, false, MainToken::Literal(l))),
            // inner set
            ('\\', 'p' | 'P') | ('[', _) => {
                let mut inner_builder = UnicodeSetBuilder::new_internal(
                    self.iter,
                    self.source,
                    self.variable_map,
                    self.xid_start,
                    self.xid_continue,
                    self.pat_ws,
                    self.property_provider,
                );
                inner_builder.parse_unicode_set()?;
                let (single, string_set) = inner_builder.finalize();
                // note: offset - 1, because we already consumed full set
                let offset = self.must_peek_index()? - 1;
                let mut strings = string_set.into_iter().collect::<Vec<_>>();
                strings.sort();
                let cpilasl = CodePointInversionListAndStringList::try_from(
                    single.build(),
                    VarZeroVec::from(&strings),
                )
                .map_err(|_| PEK::Internal.with_offset(offset))?;
                Ok((offset, false, MainToken::UnicodeSet(cpilasl)))
            }
            // note: c cannot be a whitespace, because we called skip_whitespace just before
            // (in the main parse loop), so it's safe to call this guard function
            (c, _) if legal_char_start(c) => self
                .parse_char()
                .map(|(offset, c)| (offset, false, MainToken::Literal(Literal::CharKind(c)))),
            ('-', _) => {
                self.iter.next();
                Ok((initial_offset, false, MainToken::Minus))
            }
            ('&', _) => {
                self.iter.next();
                Ok((initial_offset, false, MainToken::Ampersand))
            }
            (c, _) => Err(PEK::UnexpectedChar(c).with_offset(initial_offset)),
        }
    }

    // parses a variable or an anchor. expects '$' as next token.
    // if this is a single $ (eg `[... $ ]` or the invalid `$ a`), then this function returns Ok(None),
    // otherwise Ok(Some(variable_value)).
    fn parse_variable(&mut self) -> Result<(usize, Option<&'a VariableValue<'a>>)> {
        self.consume('$')?;

        let mut res = String::new();
        let (mut var_offset, first_c) = self.must_peek()?;

        if !self.xid_start.contains(first_c) {
            // -1 because we already consumed the '$'
            return Ok((var_offset - 1, None));
        }

        res.push(first_c);
        self.iter.next();
        // important: if we are parsing a root unicodeset as a variable, we might reach EOF as
        // a valid end of the variable name, so we cannot use must_peek here.
        while let Some(&(offset, c)) = self.iter.peek() {
            if !self.xid_continue.contains(c) {
                break;
            }
            // only update the offset if we're adding a new char to our variable
            var_offset = offset;
            self.iter.next();
            res.push(c);
        }

        if let Some(v) = self.variable_map.0.get(&res) {
            return Ok((var_offset, Some(v)));
        }

        Err(PEK::UnknownVariable.with_offset(var_offset))
    }

    // parses and consumes: '{' (s charInString)* s '}'
    fn parse_string(&mut self) -> Result<(usize, Literal)> {
        self.consume('{')?;

        let mut buffer = String::new();
        let mut last_offset;

        loop {
            self.skip_whitespace();
            last_offset = self.must_peek_index()?;
            match self.must_peek_char()? {
                '}' => {
                    self.iter.next();
                    break;
                }
                // note: c cannot be a whitespace, because we called skip_whitespace just before,
                // so it's safe to call this guard function
                c if legal_char_in_string_start(c) => {
                    // don't need the offset, because '}' will always be the last char
                    let (_, c) = self.parse_char()?;
                    match c {
                        SingleOrMultiChar::Single(c) => buffer.push(c),
                        SingleOrMultiChar::Multi(first) => {
                            buffer.push(first);
                            self.parse_multi_escape_into_string(&mut buffer)?;
                        }
                    }
                }
                c => return self.error_here(PEK::UnexpectedChar(c)),
            }
        }

        let mut chars = buffer.chars();
        let literal = match (chars.next(), chars.next()) {
            (Some(c), None) => Literal::CharKind(SingleOrMultiChar::Single(c)),
            _ => Literal::String(buffer),
        };
        Ok((last_offset, literal))
    }

    // finishes a partial multi escape parse. in case of a parse error, self.single_set
    // may be left in an inconsistent state
    fn parse_multi_escape_into_set(&mut self) -> Result<()> {
        // note: would be good to somehow merge the two multi_escape methods. splitting up the UnicodeSetBuilder into a more
        // conventional parser + lexer combo might allow this.
        // issue is that we cannot pass this method an argument that somehow mutates `self` in the current architecture.
        // self.lexer.parse_multi_into_charappendable(&mut self.single_set) should work because the lifetimes are separate

        // whitespace before first char of this loop (ie, second char in this multi_escape) must be
        // enforced when creating the SingleOrMultiChar::Multi.
        let mut first = true;
        loop {
            let skipped = self.skip_whitespace();
            match self.must_peek_char()? {
                '}' => {
                    self.iter.next();
                    return Ok(());
                }
                initial_c => {
                    if skipped == 0 && !first {
                        // bracketed hex code points must be separated by whitespace
                        return self.error_here(PEK::UnexpectedChar(initial_c));
                    }
                    first = false;

                    let (_, c) = self.parse_hex_digits_into_char(1, 6)?;
                    self.single_set.add_char(c);
                }
            }
        }
    }

    // finishes a partial multi escape parse. in case of a parse error, the caller must clean up the
    // string if necessary.
    fn parse_multi_escape_into_string(&mut self, s: &mut String) -> Result<()> {
        // whitespace before first char of this loop (ie, second char in this multi_escape) must be
        // enforced when creating the SingleOrMultiChar::Multi.
        let mut first = true;
        loop {
            let skipped = self.skip_whitespace();
            match self.must_peek_char()? {
                '}' => {
                    self.iter.next();
                    return Ok(());
                }
                initial_c => {
                    if skipped == 0 && !first {
                        // bracketed hex code points must be separated by whitespace
                        return self.error_here(PEK::UnexpectedChar(initial_c));
                    }
                    first = false;

                    let (_, c) = self.parse_hex_digits_into_char(1, 6)?;
                    s.push(c);
                }
            }
        }
    }

    // starts with \ and consumes the whole escape sequence if a single
    // char is escaped, otherwise pauses the parse after the first char
    fn parse_escaped_char(&mut self) -> Result<(usize, SingleOrMultiChar)> {
        self.consume('\\')?;

        let (offset, next_char) = self.must_next()?;

        match next_char {
            'u' | 'x' if self.peek_char() == Some('{') => {
                // bracketedHex
                self.iter.next();

                self.skip_whitespace();
                let (_, first_c) = self.parse_hex_digits_into_char(1, 6)?;
                let skipped = self.skip_whitespace();

                match self.must_peek()? {
                    (offset, '}') => {
                        self.iter.next();
                        Ok((offset, SingleOrMultiChar::Single(first_c)))
                    }
                    // note: enforcing whitespace after the first char here, because the parse_multi_escape functions
                    // won't have access to this information anymore
                    (offset, c) if c.is_ascii_hexdigit() && skipped > 0 => {
                        Ok((offset, SingleOrMultiChar::Multi(first_c)))
                    }
                    (_, c) => self.error_here(PEK::UnexpectedChar(c)),
                }
            }
            'u' => {
                // 'u' hex{4}
                self.parse_hex_digits_into_char(4, 4)
                    .map(|(offset, c)| (offset, SingleOrMultiChar::Single(c)))
            }
            'x' => {
                // 'x' hex{2}
                self.parse_hex_digits_into_char(2, 2)
                    .map(|(offset, c)| (offset, SingleOrMultiChar::Single(c)))
            }
            'U' => {
                // 'U00' ('0' hex{5} | '10' hex{4})
                self.consume('0')?;
                self.consume('0')?;
                self.parse_hex_digits_into_char(6, 6)
                    .map(|(offset, c)| (offset, SingleOrMultiChar::Single(c)))
            }
            'N' => {
                // parse code point with name in {}
                // tracking issue: https://github.com/unicode-org/icu4x/issues/1397
                Err(PEK::Unimplemented.with_offset(offset))
            }
            'a' => Ok((offset, SingleOrMultiChar::Single('\u{0007}'))),
            'b' => Ok((offset, SingleOrMultiChar::Single('\u{0008}'))),
            't' => Ok((offset, SingleOrMultiChar::Single('\u{0009}'))),
            'n' => Ok((offset, SingleOrMultiChar::Single('\u{000A}'))),
            'v' => Ok((offset, SingleOrMultiChar::Single('\u{000B}'))),
            'f' => Ok((offset, SingleOrMultiChar::Single('\u{000C}'))),
            'r' => Ok((offset, SingleOrMultiChar::Single('\u{000D}'))),
            _ => Ok((offset, SingleOrMultiChar::Single(next_char))),
        }
    }

    // starts with :, consumes the trailing :]
    fn parse_property_posix(&mut self) -> Result<()> {
        self.consume(':')?;
        if self.must_peek_char()? == '^' {
            self.inverted = true;
            self.iter.next();
        }

        self.parse_property_inner(':')?;

        self.consume(']')?;

        Ok(())
    }

    // starts with \p{ or \P{, consumes the trailing }
    fn parse_property_perl(&mut self) -> Result<()> {
        self.consume('\\')?;
        match self.must_next()? {
            (_, 'p') => {}
            (_, 'P') => self.inverted = true,
            (offset, c) => return Err(PEK::UnexpectedChar(c).with_offset(offset)),
        }
        self.consume('{')?;

        self.parse_property_inner('}')?;

        Ok(())
    }

    fn parse_property_inner(&mut self, end: char) -> Result<()> {
        // UnicodeSet spec ignores whitespace, '-', and '_',
        // but ECMA-262 requires '_', so we'll allow that.
        // TODO(#3559): support loose matching on property names (e.g., "AS  -_-  CII_Hex_ D-igit")
        // TODO(#3559): support more properties than ECMA-262

        let property_offset;

        let mut key_buffer = String::new();
        let mut value_buffer = String::new();

        enum State {
            // initial state, nothing parsed yet
            Begin,
            // non-empty property name
            PropertyName,
            // property name parsed, '=' or '≠' parsed, no value parsed yet
            PropertyValueBegin,
            // non-empty property name, non-empty property value
            PropertyValue,
        }
        use State::*;

        let mut state = Begin;
        // whether '=' (true) or '≠' (false) was parsed
        let mut equality = true;

        loop {
            self.skip_whitespace();
            match (state, self.must_peek_char()?) {
                // parse the end of the property expression
                (PropertyName | PropertyValue, c) if c == end => {
                    // byte index of (full) property name/value is one back
                    property_offset = self.must_peek_index()? - 1;
                    self.iter.next();
                    break;
                }
                // parse the property name
                // NOTE: this might be too strict, because in the case of e.g. [:value:], we might want to
                // allow [:lower-case-letter:] ([:gc=lower-case-letter:] works)
                (Begin | PropertyName, c) if c.is_ascii_alphanumeric() || c == '_' => {
                    key_buffer.push(c);
                    self.iter.next();
                    state = PropertyName;
                }
                // parse the name-value separator
                (PropertyName, c @ ('=' | '≠')) => {
                    equality = c == '=';
                    self.iter.next();
                    state = PropertyValueBegin;
                }
                // parse the property value
                (PropertyValue | PropertyValueBegin, c) if c != end => {
                    value_buffer.push(c);
                    self.iter.next();
                    state = PropertyValue;
                }
                (_, c) => return self.error_here(PEK::UnexpectedChar(c)),
            }
        }

        if !equality {
            self.inverted = !self.inverted;
        }

        let inverted = self
            .load_property_codepoints(&key_buffer, &value_buffer)
            // any error that does not already have an offset should use the appropriate property offset
            .map_err(|e| e.or_with_offset(property_offset))?;
        if inverted {
            self.inverted = !self.inverted;
        }

        Ok(())
    }

    // returns whether the set needs to be inverted or not
    fn load_property_codepoints(&mut self, key: &str, value: &str) -> Result<bool> {
        // we support:
        // [:gc = value:]
        // [:sc = value:]
        // [:scx = value:]
        // [:Grapheme_Cluster_Break = value:]
        // [:Sentence_Break = value:]
        // [:Word_Break = value:]
        // [:value:] - looks up value in gc, sc
        // [:prop:] - binary property, returns codepoints that have the property
        // [:prop = truthy/falsy:] - same as above

        let mut inverted = false;

        // contains a value for the General_Category property that needs to be tried
        let mut try_gc = Err(PEK::UnknownProperty.into());
        // contains a value for the Script property that needs to be tried
        let mut try_sc = Err(PEK::UnknownProperty.into());
        // contains a value for the Script_Extensions property that needs to be tried
        let mut try_scx = Err(PEK::UnknownProperty.into());
        // contains a value for the Grapheme_Cluster_Break property that needs to be tried
        let mut try_gcb = Err(PEK::UnknownProperty.into());
        // contains a value for the Line_Break property that needs to be tried
        let mut try_lb = Err(PEK::UnknownProperty.into());
        // contains a value for the Sentence_Break property that needs to be tried
        let mut try_sb = Err(PEK::UnknownProperty.into());
        // contains a value for the Word_Break property that needs to be tried
        let mut try_wb = Err(PEK::UnknownProperty.into());
        // contains a supposed binary property name that needs to be tried
        let mut try_binary = Err(PEK::UnknownProperty.into());
        // contains a supposed canonical combining class property name that needs to be tried
        let mut try_ccc: Result<&str, ParseError> = Err(PEK::UnknownProperty.into());
        // contains a supposed block property name that needs to be tried
        let mut try_block: Result<&str, ParseError> = Err(PEK::UnknownProperty.into());

        if !value.is_empty() {
            // key is gc, sc, scx, grapheme cluster break, sentence break, word break
            // value is a property value
            // OR
            // key is a binary property and value is a truthy/falsy value

            match key.as_bytes() {
                GeneralCategory::NAME | GeneralCategory::SHORT_NAME => try_gc = Ok(value),
                GraphemeClusterBreak::NAME | GraphemeClusterBreak::SHORT_NAME => {
                    try_gcb = Ok(value)
                }
                LineBreak::NAME | LineBreak::SHORT_NAME => try_lb = Ok(value),
                Script::NAME | Script::SHORT_NAME => try_sc = Ok(value),
                SentenceBreak::NAME | SentenceBreak::SHORT_NAME => try_sb = Ok(value),
                WordBreak::NAME | WordBreak::SHORT_NAME => try_wb = Ok(value),
                CanonicalCombiningClass::NAME | CanonicalCombiningClass::SHORT_NAME => {
                    try_ccc = Ok(value)
                }
                b"Script_Extensions" | b"scx" => try_scx = Ok(value),
                b"Block" | b"blk" => try_block = Ok(value),
                _ => {
                    let normalized_value = value.to_ascii_lowercase();
                    let truthy = matches!(normalized_value.as_str(), "true" | "t" | "yes" | "y");
                    let falsy = matches!(normalized_value.as_str(), "false" | "f" | "no" | "n");
                    // value must either match truthy or falsy
                    if truthy == falsy {
                        return Err(PEK::UnknownProperty.into());
                    }
                    // correctness: if we reach this point, only `try_binary` can be Ok, hence
                    // it does not matter that further down we unconditionally return `inverted`,
                    // because only `try_binary` can enter that code path.
                    inverted = falsy;
                    try_binary = Ok(key);
                }
            }
        } else {
            // key is binary property
            // OR a value of gc, sc (only gc or sc are supported as implicit keys by UTS35!)
            try_gc = Ok(key);
            try_sc = Ok(key);
            try_binary = Ok(key);
        }

        try_gc
            .and_then(|value| self.try_load_general_category_set(value))
            .or_else(|_| try_sc.and_then(|value| self.try_load_script_set(value)))
            .or_else(|_| try_scx.and_then(|value| self.try_load_script_extensions_set(value)))
            .or_else(|_| try_binary.and_then(|value| self.try_load_ecma262_binary_set(value)))
            .or_else(|_| try_gcb.and_then(|value| self.try_load_grapheme_cluster_break_set(value)))
            .or_else(|_| try_lb.and_then(|value| self.try_load_line_break_set(value)))
            .or_else(|_| try_sb.and_then(|value| self.try_load_sentence_break_set(value)))
            .or_else(|_| try_wb.and_then(|value| self.try_load_word_break_set(value)))
            .or_else(|_| try_ccc.and_then(|value| self.try_load_ccc_set(value)))
            .or_else(|_| try_block.and_then(|value| self.try_load_block_set(value)))?;
        Ok(inverted)
    }

    fn finalize(mut self) -> (CodePointInversionListBuilder, BTreeSet<String>) {
        if self.inverted {
            // code point inversion; removes all strings
            #[cfg(feature = "log")]
            {
                let single_set = self.single_set.clone().build();
                if !self
                    .string_set
                    .iter()
                    // if the string starts with a cp in the cp-set, then it'll be rejected through that
                    .all(|s| s.chars().next().is_some_and(|c| single_set.contains(c)))
                {
                    log::info!(
                        "Inverting a unicode set with strings. This removes all strings entirely."
                    );
                }
            }
            self.string_set.clear();
            self.single_set.complement();
        }

        (self.single_set, self.string_set)
    }

    // parses either a raw char or an escaped char. all chars are allowed, the caller must make sure to handle
    // cases where some characters are not allowed
    fn parse_char(&mut self) -> Result<(usize, SingleOrMultiChar)> {
        let (offset, c) = self.must_peek()?;
        match c {
            '\\' => self.parse_escaped_char(),
            _ => {
                self.iter.next();
                Ok((offset, SingleOrMultiChar::Single(c)))
            }
        }
    }

    // note: could turn this from the current two-pass approach into a one-pass approach
    // by manually parsing the digits instead of using u32::from_str_radix.
    fn parse_hex_digits_into_char(&mut self, min: usize, max: usize) -> Result<(usize, char)> {
        let first_offset = self.must_peek_index()?;
        let end_offset = self.validate_hex_digits(min, max)?;

        // validate_hex_digits ensures that chars (including the last one) are ascii hex digits,
        // which are all exactly one UTF-8 byte long, so slicing on these offsets always respects char boundaries
        let hex_source = &self.source[first_offset..=end_offset];
        let num = u32::from_str_radix(hex_source, 16).map_err(|_| PEK::Internal)?;
        char::try_from(num)
            .map(|c| (end_offset, c))
            .map_err(|_| PEK::InvalidEscape.with_offset(end_offset))
    }

    // validates [0-9a-fA-F]{min,max}, returns the offset of the last digit, consuming everything in the process
    fn validate_hex_digits(&mut self, min: usize, max: usize) -> Result<usize> {
        let mut last_offset = 0;
        for count in 0..max {
            let (offset, c) = self.must_peek()?;
            if !c.is_ascii_hexdigit() {
                if count < min {
                    return Err(PEK::UnexpectedChar(c).with_offset(offset));
                } else {
                    break;
                }
            }
            self.iter.next();
            last_offset = offset;
        }
        Ok(last_offset)
    }

    // returns the number of skipped whitespace chars
    fn skip_whitespace(&mut self) -> usize {
        let mut num = 0;
        while let Some(c) = self.peek_char() {
            if !self.pat_ws.contains(c) {
                break;
            }
            self.iter.next();
            num += 1;
        }
        num
    }

    fn consume(&mut self, expected: char) -> Result<()> {
        match self.must_next()? {
            (offset, c) if c != expected => Err(PEK::UnexpectedChar(c).with_offset(offset)),
            _ => Ok(()),
        }
    }

    // use this whenever an empty iterator would imply an Eof error
    fn must_next(&mut self) -> Result<(usize, char)> {
        self.iter.next().ok_or(ParseError {
            offset: None,
            kind: PEK::Eof,
        })
    }

    // use this whenever an empty iterator would imply an Eof error
    fn must_peek(&mut self) -> Result<(usize, char)> {
        self.iter.peek().copied().ok_or(ParseError {
            offset: None,
            kind: PEK::Eof,
        })
    }

    // must_peek, but looks two chars ahead. use sparingly
    fn must_peek_double(&mut self) -> Result<(usize, char)> {
        let mut copy = self.iter.clone();
        copy.next();
        copy.next().ok_or(ParseError {
            offset: None,
            kind: PEK::Eof,
        })
    }

    // see must_peek
    fn must_peek_char(&mut self) -> Result<char> {
        self.must_peek().map(|(_, c)| c)
    }

    // see must_peek
    fn must_peek_index(&mut self) -> Result<usize> {
        self.must_peek().map(|(idx, _)| idx)
    }

    fn peek_char(&mut self) -> Option<char> {
        self.iter.peek().map(|&(_, c)| c)
    }

    // TODO: return Result<!> once ! is stable
    #[inline]
    fn error_here<T>(&mut self, kind: ParseErrorKind) -> Result<T> {
        match self.iter.peek() {
            None => Err(kind.into()),
            Some(&(offset, _)) => Err(kind.with_offset(offset)),
        }
    }

    fn process_strings(&mut self, op: Operation, other_strings: BTreeSet<String>) {
        match op {
            Operation::Union => self.string_set.extend(other_strings),
            Operation::Difference => {
                self.string_set = self
                    .string_set
                    .difference(&other_strings)
                    .cloned()
                    .collect()
            }
            Operation::Intersection => {
                self.string_set = self
                    .string_set
                    .intersection(&other_strings)
                    .cloned()
                    .collect()
            }
        }
    }

    fn process_chars(&mut self, op: Operation, other_chars: CodePointInversionList) {
        match op {
            Operation::Union => self.single_set.add_set(&other_chars),
            Operation::Difference => self.single_set.remove_set(&other_chars),
            Operation::Intersection => self.single_set.retain_set(&other_chars),
        }
    }

    fn try_load_general_category_set(&mut self, name: &str) -> Result<()> {
        // TODO(#3550): This could be cached; does not depend on name.
        let name_map =
            PropertyParser::<GeneralCategoryGroup>::try_new_unstable(self.property_provider)
                .map_err(|_| PEK::Internal)?;
        let gc_value = name_map
            .as_borrowed()
            .get_loose(name)
            .ok_or(PEK::UnknownProperty)?;
        // TODO(#3550): This could be cached; does not depend on name.
        let set = CodePointMapData::<GeneralCategory>::try_new_unstable(self.property_provider)
            .map_err(|_| PEK::Internal)?
            .as_borrowed()
            .get_set_for_value_group(gc_value);
        self.single_set.add_set(&set.to_code_point_inversion_list());
        Ok(())
    }

    fn try_get_script(&self, name: &str) -> Result<Script> {
        // TODO(#3550): This could be cached; does not depend on name.
        let name_map = PropertyParser::<Script>::try_new_unstable(self.property_provider)
            .map_err(|_| PEK::Internal)?;
        name_map.as_borrowed().get_loose(name).ok_or(ParseError {
            offset: None,
            kind: PEK::UnknownProperty,
        })
    }

    fn try_load_script_set(&mut self, name: &str) -> Result<()> {
        let sc_value = self.try_get_script(name)?;
        // TODO(#3550): This could be cached; does not depend on name.
        let property_map = CodePointMapData::<Script>::try_new_unstable(self.property_provider)
            .map_err(|_| PEK::Internal)?;
        let set = property_map.as_borrowed().get_set_for_value(sc_value);
        self.single_set.add_set(&set.to_code_point_inversion_list());
        Ok(())
    }

    fn try_load_script_extensions_set(&mut self, name: &str) -> Result<()> {
        // TODO(#3550): This could be cached; does not depend on name.
        let scx = ScriptWithExtensions::try_new_unstable(self.property_provider)
            .map_err(|_| PEK::Internal)?;
        let sc_value = self.try_get_script(name)?;
        let set = scx.as_borrowed().get_script_extensions_set(sc_value);
        self.single_set.add_set(&set);
        Ok(())
    }

    fn try_load_ecma262_binary_set(&mut self, name: &str) -> Result<()> {
        let set =
            CodePointSetData::try_new_for_ecma262_unstable(self.property_provider, name.as_bytes())
                .ok_or(PEK::UnknownProperty)?
                .map_err(|_data_error| PEK::Internal)?;
        self.single_set.add_set(&set.to_code_point_inversion_list());
        Ok(())
    }

    fn try_load_grapheme_cluster_break_set(&mut self, name: &str) -> Result<()> {
        let parser =
            PropertyParser::<GraphemeClusterBreak>::try_new_unstable(self.property_provider)
                .map_err(|_| PEK::Internal)?;
        let gcb_value = parser
            .as_borrowed()
            .get_loose(name)
            .ok_or(PEK::UnknownProperty)?;
        // TODO(#3550): This could be cached; does not depend on name.
        let property_map =
            CodePointMapData::<GraphemeClusterBreak>::try_new_unstable(self.property_provider)
                .map_err(|_| PEK::Internal)?;
        let set = property_map.as_borrowed().get_set_for_value(gcb_value);
        self.single_set.add_set(&set.to_code_point_inversion_list());
        Ok(())
    }

    fn try_load_line_break_set(&mut self, name: &str) -> Result<()> {
        let parser = PropertyParser::<LineBreak>::try_new_unstable(self.property_provider)
            .map_err(|_| PEK::Internal)?;
        let lb_value = parser
            .as_borrowed()
            .get_loose(name)
            .ok_or(PEK::UnknownProperty)?;
        // TODO(#3550): This could be cached; does not depend on name.
        let property_map = CodePointMapData::<LineBreak>::try_new_unstable(self.property_provider)
            .map_err(|_| PEK::Internal)?;
        let set = property_map.as_borrowed().get_set_for_value(lb_value);
        self.single_set.add_set(&set.to_code_point_inversion_list());
        Ok(())
    }

    fn try_load_sentence_break_set(&mut self, name: &str) -> Result<()> {
        let parser = PropertyParser::<SentenceBreak>::try_new_unstable(self.property_provider)
            .map_err(|_| PEK::Internal)?;
        let sb_value = parser
            .as_borrowed()
            .get_loose(name)
            .ok_or(PEK::UnknownProperty)?;
        // TODO(#3550): This could be cached; does not depend on name.
        let property_map =
            CodePointMapData::<SentenceBreak>::try_new_unstable(self.property_provider)
                .map_err(|_| PEK::Internal)?;
        let set = property_map.as_borrowed().get_set_for_value(sb_value);
        self.single_set.add_set(&set.to_code_point_inversion_list());
        Ok(())
    }

    fn try_load_word_break_set(&mut self, name: &str) -> Result<()> {
        let parser = PropertyParser::<WordBreak>::try_new_unstable(self.property_provider)
            .map_err(|_| PEK::Internal)?;
        let wb_value = parser
            .as_borrowed()
            .get_loose(name)
            .ok_or(PEK::UnknownProperty)?;
        // TODO(#3550): This could be cached; does not depend on name.
        let property_map = CodePointMapData::<WordBreak>::try_new_unstable(self.property_provider)
            .map_err(|_| PEK::Internal)?;
        let set = property_map.as_borrowed().get_set_for_value(wb_value);
        self.single_set.add_set(&set.to_code_point_inversion_list());
        Ok(())
    }

    fn try_load_ccc_set(&mut self, name: &str) -> Result<()> {
        let parser =
            PropertyParser::<CanonicalCombiningClass>::try_new_unstable(self.property_provider)
                .map_err(|_| PEK::Internal)?;
        let value = parser
            .as_borrowed()
            .get_loose(name)
            // TODO: make the property parser do this
            .or_else(|| {
                name.parse()
                    .ok()
                    .map(CanonicalCombiningClass::from_icu4c_value)
            })
            .ok_or(PEK::UnknownProperty)?;
        // TODO(#3550): This could be cached; does not depend on name.
        let property_map =
            CodePointMapData::<CanonicalCombiningClass>::try_new_unstable(self.property_provider)
                .map_err(|_| PEK::Internal)?;
        let set = property_map.as_borrowed().get_set_for_value(value);
        self.single_set.add_set(&set.to_code_point_inversion_list());
        Ok(())
    }

    fn try_load_block_set(&mut self, name: &str) -> Result<()> {
        // TODO: source these from properties
        self.single_set
            .add_range(match name.to_ascii_lowercase().as_str() {
                "arabic" => '\u{0600}'..'\u{06FF}',
                "thaana" => '\u{0780}'..'\u{07BF}',
                _ => {
                    #[cfg(feature = "log")]
                    log::warn!("Skipping :block={name}:");
                    return Err(PEK::Unimplemented.into());
                }
            });
        Ok(())
    }
}

/// Parses a `UnicodeSet` pattern and returns a `UnicodeSet` in the form of a [`CodePointInversionListAndStringList`](CodePointInversionListAndStringList),
/// as well as the number of bytes consumed from the source string.
///
/// Supports `UnicodeSets` as described in [UTS #35 - Unicode Sets](https://unicode.org/reports/tr35/#Unicode_Sets).
///
/// The error type of the returned Result can be pretty-printed with [`ParseError::fmt_with_source`].
///
/// # Variables
///
/// If you need support for variables inside `UnicodeSets` (e.g., `[$start-$end]`), use [`parse_with_variables`].
///
/// # Limitations
///
/// * Currently, we only support the [ECMA-262 properties](https://tc39.es/ecma262/#table-nonbinary-unicode-properties).
///   The property names must match the exact spelling listed in ECMA-262. Note that we do support UTS35 syntax for elided `General_Category`
///   and `Script` property names, i.e., `[:Latn:]` and `[:Ll:]` are both valid, with the former implying the `Script` property, and the latter the
///   `General_Category` property.
/// * We do not support `\N{Unicode code point name}` character escaping. Use any other escape method described in UTS35.
///
/// ✨ *Enabled with the `compiled_data` Cargo feature.*
///
/// [📚 Help choosing a constructor](icu_provider::constructors)
///
/// # Examples
///
/// Parse ranges
/// ```
/// use icu::experimental::unicodeset_parse::parse;
///
/// let source = "[a-zA-Z0-9]";
/// let (set, consumed) = parse(source).unwrap();
/// let code_points = set.code_points();
///
/// assert!(code_points.contains_range('a'..='z'));
/// assert!(code_points.contains_range('A'..='Z'));
/// assert!(code_points.contains_range('0'..='9'));
/// assert_eq!(consumed, source.len());
/// ```
///
/// Parse properties, set operations, inner sets
/// ```
/// use icu::experimental::unicodeset_parse::parse;
///
/// let (set, _) =
///     parse("[[:^ll:]-[^][:gc = Lowercase Letter:]&[^[[^]-[a-z]]]]").unwrap();
/// assert!(set.code_points().contains_range('a'..='z'));
/// assert_eq!(('a'..='z').count(), set.size());
/// ```
///
/// Inversions remove strings
/// ```
/// use icu::experimental::unicodeset_parse::parse;
///
/// let (set, _) =
///     parse(r"[[a-z{hello\ world}]&[^a-y{hello\ world}]]").unwrap();
/// assert!(set.contains('z'));
/// assert_eq!(set.size(), 1);
/// assert!(!set.has_strings());
/// ```
///
/// Set operators (including the implicit union) have the same precedence and are left-associative
/// ```
/// use icu::experimental::unicodeset_parse::parse;
///
/// let (set, _) = parse("[[ace][bdf] - [abc][def]]").unwrap();
/// assert!(set.code_points().contains_range('d'..='f'));
/// assert_eq!(set.size(), ('d'..='f').count());
/// ```
///
/// Supports partial parses
/// ```
/// use icu::experimental::unicodeset_parse::parse;
///
/// let (set, consumed) = parse("[a-c][x-z]").unwrap();
/// let code_points = set.code_points();
/// assert!(code_points.contains_range('a'..='c'));
/// assert!(!code_points.contains_range('x'..='z'));
/// assert_eq!(set.size(), ('a'..='c').count());
/// // only the first UnicodeSet is parsed
/// assert_eq!(consumed, "[a-c]".len());
/// ```
#[cfg(feature = "compiled_data")]
pub fn parse(source: &str) -> Result<(CodePointInversionListAndStringList<'static>, usize)> {
    parse_unstable(source, &Baked)
}

/// Parses a `UnicodeSet` pattern with support for variables enabled.
///
/// See [`parse`] for more information.
///
/// # Examples
///
/// ```
/// use icu::experimental::unicodeset_parse::*;
///
/// let (my_set, _) = parse("[abc]").unwrap();
///
/// let mut variable_map = VariableMap::new();
/// variable_map.insert_char("start".into(), 'a').unwrap();
/// variable_map.insert_char("end".into(), 'z').unwrap();
/// variable_map.insert_string("str".into(), "Hello World".into()).unwrap();
/// variable_map.insert_set("the_set".into(), my_set).unwrap();
///
/// // If a variable already exists, `Err` is returned, and the map is not updated.
/// variable_map.insert_char("end".into(), 'Ω').unwrap_err();
///
/// let source = "[[$start-$end]-$the_set $str]";
/// let (set, consumed) = parse_with_variables(source, &variable_map).unwrap();
/// assert_eq!(consumed, source.len());
/// assert!(set.code_points().contains_range('d'..='z'));
/// assert!(set.contains_str("Hello World"));
/// assert_eq!(set.size(), 1 + ('d'..='z').count());
#[cfg(feature = "compiled_data")]
pub fn parse_with_variables(
    source: &str,
    variable_map: &VariableMap<'_>,
) -> Result<(CodePointInversionListAndStringList<'static>, usize)> {
    parse_unstable_with_variables(source, variable_map, &Baked)
}

#[doc = icu_provider::gen_buffer_unstable_docs!(UNSTABLE, parse_with_variables)]
pub fn parse_unstable_with_variables<P>(
    source: &str,
    variable_map: &VariableMap<'_>,
    provider: &P,
) -> Result<(CodePointInversionListAndStringList<'static>, usize)>
where
    P: ?Sized
        + DataProvider<PropertyBinaryAlphabeticV1>
        + DataProvider<PropertyBinaryAsciiHexDigitV1>
        + DataProvider<PropertyBinaryBidiControlV1>
        + DataProvider<PropertyBinaryBidiMirroredV1>
        + DataProvider<PropertyBinaryCasedV1>
        + DataProvider<PropertyBinaryCaseIgnorableV1>
        + DataProvider<PropertyBinaryChangesWhenCasefoldedV1>
        + DataProvider<PropertyBinaryChangesWhenCasemappedV1>
        + DataProvider<PropertyBinaryChangesWhenLowercasedV1>
        + DataProvider<PropertyBinaryChangesWhenNfkcCasefoldedV1>
        + DataProvider<PropertyBinaryChangesWhenTitlecasedV1>
        + DataProvider<PropertyBinaryChangesWhenUppercasedV1>
        + DataProvider<PropertyBinaryDashV1>
        + DataProvider<PropertyBinaryDefaultIgnorableCodePointV1>
        + DataProvider<PropertyBinaryDeprecatedV1>
        + DataProvider<PropertyBinaryDiacriticV1>
        + DataProvider<PropertyBinaryEmojiComponentV1>
        + DataProvider<PropertyBinaryEmojiModifierBaseV1>
        + DataProvider<PropertyBinaryEmojiModifierV1>
        + DataProvider<PropertyBinaryEmojiPresentationV1>
        + DataProvider<PropertyBinaryEmojiV1>
        + DataProvider<PropertyBinaryExtendedPictographicV1>
        + DataProvider<PropertyBinaryExtenderV1>
        + DataProvider<PropertyBinaryGraphemeBaseV1>
        + DataProvider<PropertyBinaryGraphemeExtendV1>
        + DataProvider<PropertyBinaryHexDigitV1>
        + DataProvider<PropertyBinaryIdContinueV1>
        + DataProvider<PropertyBinaryIdeographicV1>
        + DataProvider<PropertyBinaryIdsBinaryOperatorV1>
        + DataProvider<PropertyBinaryIdStartV1>
        + DataProvider<PropertyBinaryIdsTrinaryOperatorV1>
        + DataProvider<PropertyBinaryJoinControlV1>
        + DataProvider<PropertyBinaryLogicalOrderExceptionV1>
        + DataProvider<PropertyBinaryLowercaseV1>
        + DataProvider<PropertyBinaryMathV1>
        + DataProvider<PropertyBinaryNoncharacterCodePointV1>
        + DataProvider<PropertyBinaryPatternSyntaxV1>
        + DataProvider<PropertyBinaryPatternWhiteSpaceV1>
        + DataProvider<PropertyBinaryQuotationMarkV1>
        + DataProvider<PropertyBinaryRadicalV1>
        + DataProvider<PropertyBinaryRegionalIndicatorV1>
        + DataProvider<PropertyBinarySentenceTerminalV1>
        + DataProvider<PropertyBinarySoftDottedV1>
        + DataProvider<PropertyBinaryTerminalPunctuationV1>
        + DataProvider<PropertyBinaryUnifiedIdeographV1>
        + DataProvider<PropertyBinaryUppercaseV1>
        + DataProvider<PropertyBinaryVariationSelectorV1>
        + DataProvider<PropertyBinaryWhiteSpaceV1>
        + DataProvider<PropertyBinaryXidContinueV1>
        + DataProvider<PropertyBinaryXidStartV1>
        + DataProvider<PropertyEnumCanonicalCombiningClassV1>
        + DataProvider<PropertyEnumGeneralCategoryV1>
        + DataProvider<PropertyEnumGraphemeClusterBreakV1>
        + DataProvider<PropertyEnumLineBreakV1>
        + DataProvider<PropertyEnumScriptV1>
        + DataProvider<PropertyEnumSentenceBreakV1>
        + DataProvider<PropertyEnumWordBreakV1>
        + DataProvider<PropertyNameParseCanonicalCombiningClassV1>
        + DataProvider<PropertyNameParseGeneralCategoryMaskV1>
        + DataProvider<PropertyNameParseGraphemeClusterBreakV1>
        + DataProvider<PropertyNameParseLineBreakV1>
        + DataProvider<PropertyNameParseScriptV1>
        + DataProvider<PropertyNameParseSentenceBreakV1>
        + DataProvider<PropertyNameParseWordBreakV1>
        + DataProvider<PropertyScriptWithExtensionsV1>,
{
    // TODO(#3550): Add function "parse_overescaped" that uses a custom iterator to de-overescape (i.e., maps \\ to \) on-the-fly?
    // ^ will likely need a different iterator type on UnicodeSetBuilder

    let mut iter = source.char_indices().peekable();

    let xid_start =
        CodePointSetData::try_new_unstable::<XidStart>(provider).map_err(|_| PEK::Internal)?;
    let xid_start_list = xid_start.to_code_point_inversion_list();
    let xid_continue =
        CodePointSetData::try_new_unstable::<XidContinue>(provider).map_err(|_| PEK::Internal)?;
    let xid_continue_list = xid_continue.to_code_point_inversion_list();

    let pat_ws = CodePointSetData::try_new_unstable::<PatternWhiteSpace>(provider)
        .map_err(|_| PEK::Internal)?;
    let pat_ws_list = pat_ws.to_code_point_inversion_list();

    let mut builder = UnicodeSetBuilder::new_internal(
        &mut iter,
        source,
        variable_map,
        &xid_start_list,
        &xid_continue_list,
        &pat_ws_list,
        provider,
    );

    builder.parse_unicode_set()?;
    let (single, string_set) = builder.finalize();
    let built_single = single.build();

    let mut strings = string_set.into_iter().collect::<Vec<_>>();
    strings.sort();
    let zerovec = (&strings).into();

    let cpinvlistandstrlist = CodePointInversionListAndStringList::try_from(built_single, zerovec)
        .map_err(|_| PEK::Internal)?;

    let parsed_bytes = match iter.peek().copied() {
        None => source.len(),
        Some((offset, _)) => offset,
    };

    Ok((cpinvlistandstrlist, parsed_bytes))
}

#[doc = icu_provider::gen_buffer_unstable_docs!(UNSTABLE, parse)]
pub fn parse_unstable<P>(
    source: &str,
    provider: &P,
) -> Result<(CodePointInversionListAndStringList<'static>, usize)>
where
    P: ?Sized
        + DataProvider<PropertyBinaryAlphabeticV1>
        + DataProvider<PropertyBinaryAsciiHexDigitV1>
        + DataProvider<PropertyBinaryBidiControlV1>
        + DataProvider<PropertyBinaryBidiMirroredV1>
        + DataProvider<PropertyBinaryCasedV1>
        + DataProvider<PropertyBinaryCaseIgnorableV1>
        + DataProvider<PropertyBinaryChangesWhenCasefoldedV1>
        + DataProvider<PropertyBinaryChangesWhenCasemappedV1>
        + DataProvider<PropertyBinaryChangesWhenLowercasedV1>
        + DataProvider<PropertyBinaryChangesWhenNfkcCasefoldedV1>
        + DataProvider<PropertyBinaryChangesWhenTitlecasedV1>
        + DataProvider<PropertyBinaryChangesWhenUppercasedV1>
        + DataProvider<PropertyBinaryDashV1>
        + DataProvider<PropertyBinaryDefaultIgnorableCodePointV1>
        + DataProvider<PropertyBinaryDeprecatedV1>
        + DataProvider<PropertyBinaryDiacriticV1>
        + DataProvider<PropertyBinaryEmojiComponentV1>
        + DataProvider<PropertyBinaryEmojiModifierBaseV1>
        + DataProvider<PropertyBinaryEmojiModifierV1>
        + DataProvider<PropertyBinaryEmojiPresentationV1>
        + DataProvider<PropertyBinaryEmojiV1>
        + DataProvider<PropertyBinaryExtendedPictographicV1>
        + DataProvider<PropertyBinaryExtenderV1>
        + DataProvider<PropertyBinaryGraphemeBaseV1>
        + DataProvider<PropertyBinaryGraphemeExtendV1>
        + DataProvider<PropertyBinaryHexDigitV1>
        + DataProvider<PropertyBinaryIdContinueV1>
        + DataProvider<PropertyBinaryIdeographicV1>
        + DataProvider<PropertyBinaryIdsBinaryOperatorV1>
        + DataProvider<PropertyBinaryIdStartV1>
        + DataProvider<PropertyBinaryIdsTrinaryOperatorV1>
        + DataProvider<PropertyBinaryJoinControlV1>
        + DataProvider<PropertyBinaryLogicalOrderExceptionV1>
        + DataProvider<PropertyBinaryLowercaseV1>
        + DataProvider<PropertyBinaryMathV1>
        + DataProvider<PropertyBinaryNoncharacterCodePointV1>
        + DataProvider<PropertyBinaryPatternSyntaxV1>
        + DataProvider<PropertyBinaryPatternWhiteSpaceV1>
        + DataProvider<PropertyBinaryQuotationMarkV1>
        + DataProvider<PropertyBinaryRadicalV1>
        + DataProvider<PropertyBinaryRegionalIndicatorV1>
        + DataProvider<PropertyBinarySentenceTerminalV1>
        + DataProvider<PropertyBinarySoftDottedV1>
        + DataProvider<PropertyBinaryTerminalPunctuationV1>
        + DataProvider<PropertyBinaryUnifiedIdeographV1>
        + DataProvider<PropertyBinaryUppercaseV1>
        + DataProvider<PropertyBinaryVariationSelectorV1>
        + DataProvider<PropertyBinaryWhiteSpaceV1>
        + DataProvider<PropertyBinaryXidContinueV1>
        + DataProvider<PropertyBinaryXidStartV1>
        + DataProvider<PropertyEnumCanonicalCombiningClassV1>
        + DataProvider<PropertyEnumGeneralCategoryV1>
        + DataProvider<PropertyEnumGraphemeClusterBreakV1>
        + DataProvider<PropertyEnumLineBreakV1>
        + DataProvider<PropertyEnumScriptV1>
        + DataProvider<PropertyEnumSentenceBreakV1>
        + DataProvider<PropertyEnumWordBreakV1>
        + DataProvider<PropertyNameParseCanonicalCombiningClassV1>
        + DataProvider<PropertyNameParseGeneralCategoryMaskV1>
        + DataProvider<PropertyNameParseGraphemeClusterBreakV1>
        + DataProvider<PropertyNameParseLineBreakV1>
        + DataProvider<PropertyNameParseScriptV1>
        + DataProvider<PropertyNameParseSentenceBreakV1>
        + DataProvider<PropertyNameParseWordBreakV1>
        + DataProvider<PropertyScriptWithExtensionsV1>,
{
    let dummy = Default::default();
    parse_unstable_with_variables(source, &dummy, provider)
}

#[cfg(test)]
mod tests {
    use core::ops::RangeInclusive;
    use std::collections::HashSet;

    use super::*;

    // "aabxzz" => [a..=a, b..=x, z..=z]
    fn range_iter_from_str(s: &str) -> impl Iterator<Item = RangeInclusive<u32>> {
        debug_assert_eq!(
            s.chars().count() % 2,
            0,
            "string \"{}\" does not contain an even number of code points",
            s.escape_debug()
        );
        let mut res = vec![];
        let mut skip = false;
        for (a, b) in s.chars().zip(s.chars().skip(1)) {
            if skip {
                skip = false;
                continue;
            }
            let a = a as u32;
            let b = b as u32;
            res.push(a..=b);
            skip = true;
        }

        res.into_iter()
    }

    fn assert_set_equality<'a>(
        source: &str,
        cpinvlistandstrlist: &CodePointInversionListAndStringList,
        single: impl Iterator<Item = RangeInclusive<u32>>,
        strings: impl Iterator<Item = &'a str>,
    ) {
        let expected_ranges: HashSet<_> = single.collect();
        let actual_ranges: HashSet<_> = cpinvlistandstrlist.code_points().iter_ranges().collect();
        assert_eq!(
            actual_ranges,
            expected_ranges,
            "got unexpected ranges {:?}, expected {:?} for parsed set \"{}\"",
            actual_ranges,
            expected_ranges,
            source.escape_debug()
        );
        let mut expected_size = cpinvlistandstrlist.code_points().size();
        for s in strings {
            expected_size += 1;
            assert!(
                cpinvlistandstrlist.contains_str(s),
                "missing string \"{}\" from parsed set \"{}\"",
                s.escape_debug(),
                source.escape_debug()
            );
        }
        let actual_size = cpinvlistandstrlist.size();
        assert_eq!(
            actual_size,
            expected_size,
            "got unexpected size {}, expected {} for parsed set \"{}\"",
            actual_size,
            expected_size,
            source.escape_debug()
        );
    }

    fn assert_is_error_and_message_eq(source: &str, expected_err: &str, vm: &VariableMap<'_>) {
        let result = parse_with_variables(source, vm);
        assert!(result.is_err(), "{source} does not cause an error!");
        let err = result.unwrap_err();
        assert_eq!(err.fmt_with_source(source).to_string(), expected_err);
    }

    #[test]
    fn test_semantics_with_variables() {
        let mut map_char_char = VariableMap::default();
        map_char_char.insert_char("a".to_string(), 'a').unwrap();
        map_char_char.insert_char("var2".to_string(), 'z').unwrap();

        let mut map_headache = VariableMap::default();
        map_headache.insert_char("hehe".to_string(), '-').unwrap();

        let mut map_char_string = VariableMap::default();
        map_char_string.insert_char("a".to_string(), 'a').unwrap();
        map_char_string
            .insert_string("var2".to_string(), "abc".to_string())
            .unwrap();

        let (set, _) = parse(r"[a-z {Hello,\ World!}]").unwrap();
        let mut map_char_set = VariableMap::default();
        map_char_set.insert_char("a".to_string(), 'a').unwrap();
        map_char_set.insert_set("set".to_string(), set).unwrap();

        let cases: Vec<(_, _, _, Vec<&str>)> = vec![
            // simple
            (&map_char_char, "[$a]", "aa", vec![]),
            (&map_char_char, "[ $a ]", "aa", vec![]),
            (&map_char_char, "[$a$]", "aa\u{ffff}\u{ffff}", vec![]),
            (&map_char_char, "[$a$ ]", "aa\u{ffff}\u{ffff}", vec![]),
            (&map_char_char, "[$a$var2]", "aazz", vec![]),
            (&map_char_char, "[$a - $var2]", "az", vec![]),
            (&map_char_char, "[$a-$var2]", "az", vec![]),
            (&map_headache, "[a $hehe z]", "aazz--", vec![]),
            (
                &map_char_char,
                "[[$]var2]",
                "\u{ffff}\u{ffff}vvaarr22",
                vec![],
            ),
            // variable prefix escaping
            (&map_char_char, r"[\$var2]", "$$vvaarr22", vec![]),
            (&map_char_char, r"[\\$var2]", r"\\zz", vec![]),
            // no variable dereferencing in strings
            (&map_char_char, "[{$a}]", "", vec!["$a"]),
            // set operations
            (&map_char_set, "[$set & [b-z]]", "bz", vec![]),
            (&map_char_set, "[[a-z]-[b-z]]", "aa", vec![]),
            (&map_char_set, "[$set-[b-z]]", "aa", vec!["Hello, World!"]),
            (&map_char_set, "[$set-$set]", "", vec![]),
            (&map_char_set, "[[a-zA]-$set]", "AA", vec![]),
            (&map_char_set, "[$set[b-z]]", "az", vec!["Hello, World!"]),
            (&map_char_set, "[[a-a]$set]", "az", vec!["Hello, World!"]),
            (&map_char_set, "$set", "az", vec!["Hello, World!"]),
            // strings
            (&map_char_string, "[$var2]", "", vec!["abc"]),
        ];
        for (variable_map, source, single, strings) in cases {
            let parsed = parse_with_variables(source, variable_map);
            if let Err(err) = parsed {
                panic!(
                    "{source} results in an error: {}",
                    err.fmt_with_source(source)
                );
            }
            let (set, consumed) = parsed.unwrap();
            assert_eq!(consumed, source.len(), "{source:?} is not fully consumed");
            assert_set_equality(
                source,
                &set,
                range_iter_from_str(single),
                strings.into_iter(),
            );
        }
    }

    #[test]
    fn test_semantics() {
        const ALL_CHARS: &str = "\x00\u{10FFFF}";
        let cases: Vec<(_, _, Vec<&str>)> = vec![
            // simple
            ("[a]", "aa", vec![]),
            ("[]", "", vec![]),
            ("[qax]", "aaqqxx", vec![]),
            ("[a-z]", "az", vec![]),
            ("[--]", "--", vec![]),
            ("[a-b-]", "ab--", vec![]),
            ("[[a-b]-]", "ab--", vec![]),
            ("[{ab}-]", "--", vec!["ab"]),
            ("[-a-b]", "ab--", vec![]),
            ("[-a]", "--aa", vec![]),
            // whitespace escaping
            (r"[\n]", "\n\n", vec![]),
            ("[\\\n]", "\n\n", vec![]),
            // empty - whitespace is skipped
            ("[\n]", "", vec![]),
            ("[\u{9}]", "", vec![]),
            ("[\u{A}]", "", vec![]),
            ("[\u{B}]", "", vec![]),
            ("[\u{C}]", "", vec![]),
            ("[\u{D}]", "", vec![]),
            ("[\u{20}]", "", vec![]),
            ("[\u{85}]", "", vec![]),
            ("[\u{200E}]", "", vec![]),
            ("[\u{200F}]", "", vec![]),
            ("[\u{2028}]", "", vec![]),
            ("[\u{2029}]", "", vec![]),
            // whitespace significance:
            ("[^[^$]]", "\u{ffff}\u{ffff}", vec![]),
            ("[^[^ $]]", "\u{ffff}\u{ffff}", vec![]),
            ("[^[^ $ ]]", "\u{ffff}\u{ffff}", vec![]),
            ("[^[^a$]]", "aa\u{ffff}\u{ffff}", vec![]),
            ("[^[^a$ ]]", "aa\u{ffff}\u{ffff}", vec![]),
            ("[-]", "--", vec![]),
            ("[  -  ]", "--", vec![]),
            ("[  - -  ]", "--", vec![]),
            ("[ a-b -  ]", "ab--", vec![]),
            ("[ -a]", "--aa", vec![]),
            ("[a-]", "--aa", vec![]),
            ("[a- ]", "--aa", vec![]),
            ("[ :]", "::", vec![]),
            ("[ :L:]", "::LL", vec![]),
            // but not all "whitespace", only Pattern_White_Space:
            ("[\u{A0}]", "\u{A0}\u{A0}", vec![]), // non-breaking space
            // anchor
            ("[$]", "\u{ffff}\u{ffff}", vec![]),
            (r"[\$]", "$$", vec![]),
            ("[{$}]", "$$", vec![]),
            // set operations
            ("[[a-z]&[b-z]]", "bz", vec![]),
            ("[[a-z]-[b-z]]", "aa", vec![]),
            ("[[a-z][b-z]]", "az", vec![]),
            ("[[a-a][b-z]]", "az", vec![]),
            ("[[a-z{abc}]&[b-z{abc}{abx}]]", "bz", vec!["abc"]),
            ("[[{abx}a-z{abc}]&[b-z{abc}]]", "bz", vec!["abc"]),
            ("[[a-z{abx}]-[{abx}b-z{abc}]]", "aa", vec![]),
            ("[[a-z{abx}{abc}]-[{abx}b-z]]", "aa", vec!["abc"]),
            ("[[a-z{abc}][b-z{abx}]]", "az", vec!["abc", "abx"]),
            // strings
            ("[{this is a minus -}]", "", vec!["thisisaminus-"]),
            // associativity
            ("[[a-a][b-z] - [a-d][e-z]]", "ez", vec![]),
            ("[[a-a][b-z] - [a-d]&[e-z]]", "ez", vec![]),
            ("[[a-a][b-z] - [a-z][]]", "", vec![]),
            ("[[a-a][b-z] - [a-z]&[]]", "", vec![]),
            ("[[a-a][b-z] & [a-z]-[]]", "az", vec![]),
            ("[[a-a][b-z] & []-[a-z]]", "", vec![]),
            ("[[a-a][b-z] & [a-b][x-z]]", "abxz", vec![]),
            ("[[a-z]-[a-b]-[y-z]]", "cx", vec![]),
            // escape tests
            (r"[\x61-\x63]", "ac", vec![]),
            (r"[a-\x63]", "ac", vec![]),
            (r"[\x61-c]", "ac", vec![]),
            (r"[\u0061-\x63]", "ac", vec![]),
            (r"[\U00000061-\x63]", "ac", vec![]),
            (r"[\x{61}-\x63]", "ac", vec![]),
            (r"[\u{61}-\x63]", "ac", vec![]),
            (r"[\u{61}{hello\ world}]", "aa", vec!["hello world"]),
            (r"[{hello\ world}\u{61}]", "aa", vec!["hello world"]),
            (r"[{h\u{65}llo\ world}]", "", vec!["hello world"]),
            // complement tests
            (r"[^]", ALL_CHARS, vec![]),
            (r"[[^]-[^a-z]]", "az", vec![]),
            (r"[^{h\u{65}llo\ world}]", ALL_CHARS, vec![]),
            (
                r"[^[{h\u{65}llo\ world}]-[{hello\ world}]]",
                ALL_CHARS,
                vec![],
            ),
            (
                r"[^[\x00-\U0010FFFF]-[\u0100-\U0010FFFF]]",
                "\u{100}\u{10FFFF}",
                vec![],
            ),
            (r"[^[^a-z]]", "az", vec![]),
            (r"[^[^\^]]", "^^", vec![]),
            (r"[{\x{61 0062   063}}]", "", vec!["abc"]),
            (r"[\x{61 0062   063}]", "ac", vec![]),
            // binary properties
            (r"[:AHex:]", "09afAF", vec![]),
            (r"[:AHex=True:]", "09afAF", vec![]),
            (r"[:AHex=T:]", "09afAF", vec![]),
            (r"[:AHex=Yes:]", "09afAF", vec![]),
            (r"[:AHex=Y:]", "09afAF", vec![]),
            (r"[:^AHex≠True:]", "09afAF", vec![]),
            (r"[:AHex≠False:]", "09afAF", vec![]),
            (r"[[:^AHex≠False:]&[\x00-\x10]]", "\0\x10", vec![]),
            (r"\p{AHex}", "09afAF", vec![]),
            (r"\p{AHex=True}", "09afAF", vec![]),
            (r"\p{AHex=T}", "09afAF", vec![]),
            (r"\p{AHex=Yes}", "09afAF", vec![]),
            (r"\p{AHex=Y}", "09afAF", vec![]),
            (r"\P{AHex≠True}", "09afAF", vec![]),
            (r"\p{AHex≠False}", "09afAF", vec![]),
            // general category
            (r"[[:gc=lower-case-letter:]&[a-zA-Z]]", "az", vec![]),
            (r"[[:lower case letter:]&[a-zA-Z]]", "az", vec![]),
            // general category groups
            // equivalence between L and the union of all the L* categories
            (
                r"[[[:L:]-[\p{Ll}\p{Lt}\p{Lu}\p{Lo}\p{Lm}]][[\p{Ll}\p{Lt}\p{Lu}\p{Lo}\p{Lm}]-[:L:]]]",
                "",
                vec![],
            ),
            // script
            (r"[[:sc=latn:]&[a-zA-Z]]", "azAZ", vec![]),
            (r"[[:sc=Latin:]&[a-zA-Z]]", "azAZ", vec![]),
            (r"[[:Latin:]&[a-zA-Z]]", "azAZ", vec![]),
            (r"[[:latn:]&[a-zA-Z]]", "azAZ", vec![]),
            // script extensions
            (r"[[:scx=latn:]&[a-zA-Z]]", "azAZ", vec![]),
            (r"[[:scx=Latin:]&[a-zA-Z]]", "azAZ", vec![]),
            (r"[[:scx=Hira:]&[\u30FC]]", "\u{30FC}\u{30FC}", vec![]),
            (r"[[:sc=Hira:]&[\u30FC]]", "", vec![]),
            (r"[[:scx=Kana:]&[\u30FC]]", "\u{30FC}\u{30FC}", vec![]),
            (r"[[:sc=Kana:]&[\u30FC]]", "", vec![]),
            (r"[[:sc=Common:]&[\u30FC]]", "\u{30FC}\u{30FC}", vec![]),
            // grapheme cluster break
            (
                r"\p{Grapheme_Cluster_Break=ZWJ}",
                "\u{200D}\u{200D}",
                vec![],
            ),
            // sentence break
            (
                r"\p{Sentence_Break=ATerm}",
                "\u{002E}\u{002E}\u{2024}\u{2024}\u{FE52}\u{FE52}\u{FF0E}\u{FF0E}",
                vec![],
            ),
            // word break
            (r"\p{Word_Break=Single_Quote}", "\u{0027}\u{0027}", vec![]),
            // more syntax edge cases from UTS35 directly
            (r"[\^a]", "^^aa", vec![]),
            (r"[{{}]", "{{", vec![]),
            (r"[{}}]", "}}", vec![""]),
            (r"[}]", "}}", vec![]),
            (r"[{$var}]", "", vec!["$var"]),
            (r"[{[a-z}]", "", vec!["[a-z"]),
            (r"[ { [ a - z } ]", "", vec!["[a-z"]),
            // TODO(#3556): Add more tests (specifically conformance tests if they exist)
        ];
        for (source, single, strings) in cases {
            let parsed = parse(source);
            if let Err(err) = parsed {
                panic!(
                    "{source} results in an error: {}",
                    err.fmt_with_source(source)
                );
            }
            let (set, consumed) = parsed.unwrap();
            assert_eq!(consumed, source.len());
            assert_set_equality(
                source,
                &set,
                range_iter_from_str(single),
                strings.into_iter(),
            );
        }
    }

    #[test]
    fn test_error_messages_with_variables() {
        let mut map_char_char = VariableMap::default();
        map_char_char.insert_char("a".to_string(), 'a').unwrap();
        map_char_char.insert_char("var2".to_string(), 'z').unwrap();

        let mut map_char_string = VariableMap::default();
        map_char_string.insert_char("a".to_string(), 'a').unwrap();
        map_char_string
            .insert_string("var2".to_string(), "abc".to_string())
            .unwrap();

        let (set, _) = parse(r"[a-z {Hello,\ World!}]").unwrap();
        let mut map_char_set = VariableMap::default();
        map_char_set.insert_char("a".to_string(), 'a').unwrap();
        map_char_set.insert_set("set".to_string(), set).unwrap();

        let cases = [
            (&map_char_char, "[$$a]", r"[$$a← error: unexpected variable"),
            (
                &map_char_char,
                "[$ a]",
                r"[$ a← error: unexpected character 'a'",
            ),
            (&map_char_char, "$a", r"$a← error: unexpected variable"),
            (&map_char_char, "$", r"$← error: unexpected end of input"),
            (
                &map_char_string,
                "[$var2-$a]",
                r"[$var2-$a← error: unexpected variable",
            ),
            (
                &map_char_string,
                "[$a-$var2]",
                r"[$a-$var2← error: unexpected variable",
            ),
            (
                &map_char_set,
                "[$a-$set]",
                r"[$a-$set← error: unexpected variable",
            ),
            (
                &map_char_set,
                "[$set-$a]",
                r"[$set-$a← error: unexpected variable",
            ),
            (
                &map_char_set,
                "[$=]",
                "[$=← error: unexpected character '='",
            ),
        ];
        for (variable_map, source, expected_err) in cases {
            assert_is_error_and_message_eq(source, expected_err, variable_map);
        }
    }

    #[test]
    fn test_error_messages() {
        let cases = [
            (r"[a-z[\]]", r"[a-z[\]]← error: unexpected end of input"),
            (r"", r"← error: unexpected end of input"),
            (r"[{]", r"[{]← error: unexpected end of input"),
            // we match ECMA-262 strictly, so case matters
            (
                r"[:general_category:]",
                r"[:general_category← error: unknown property",
            ),
            (r"[:ll=true:]", r"[:ll=true← error: unknown property"),
            (r"[:=", r"[:=← error: unexpected character '='"),
            // property names may not be empty
            (r"[::]", r"[::← error: unexpected character ':'"),
            (r"[:=hello:]", r"[:=← error: unexpected character '='"),
            // property values may not be empty
            (r"[:gc=:]", r"[:gc=:← error: unexpected character ':'"),
            (r"[\xag]", r"[\xag← error: unexpected character 'g'"),
            (r"[a-b-z]", r"[a-b-z← error: unexpected character 'z'"),
            // TODO(#3558): Might be better as "[a-\p← error: unexpected character 'p'"?
            (r"[a-\p{ll}]", r"[a-\← error: unexpected character '\\'"),
            (r"[a-&]", r"[a-&← error: unexpected character '&'"),
            (r"[a&b]", r"[a&← error: unexpected character '&'"),
            (r"[[set]&b]", r"[[set]&b← error: unexpected character 'b'"),
            (r"[[set]&]", r"[[set]&]← error: unexpected character ']'"),
            (r"[a-\x60]", r"[a-\x60← error: unexpected character '`'"),
            (r"[a-`]", r"[a-`← error: unexpected character '`'"),
            (r"[\x{6g}]", r"[\x{6g← error: unexpected character 'g'"),
            (r"[\x{g}]", r"[\x{g← error: unexpected character 'g'"),
            (r"[\x{}]", r"[\x{}← error: unexpected character '}'"),
            (
                r"[\x{dabeef}]",
                r"[\x{dabeef← error: invalid escape sequence",
            ),
            (
                r"[\x{10ffff0}]",
                r"[\x{10ffff0← error: unexpected character '0'",
            ),
            (
                r"[\x{11ffff}]",
                r"[\x{11ffff← error: invalid escape sequence",
            ),
            (
                r"[\x{10ffff 1 10ffff0}]",
                r"[\x{10ffff 1 10ffff0← error: unexpected character '0'",
            ),
            // > 1 byte in UTF-8 edge case
            (r"ä", r"ä← error: unexpected character 'ä'"),
            (r"\p{gc=ä}", r"\p{gc=ä← error: unknown property"),
            (r"\p{gc=ä}", r"\p{gc=ä← error: unknown property"),
            (
                r"[\xe5-\xe4]",
                r"[\xe5-\xe4← error: unexpected character 'ä'",
            ),
            (r"[\xe5-ä]", r"[\xe5-ä← error: unexpected character 'ä'"),
            // whitespace significance
            (r"[ ^]", r"[ ^← error: unexpected character '^'"),
            (r"[:]", r"[:]← error: unexpected character ']'"),
            (r"[:L]", r"[:L]← error: unexpected character ']'"),
            (r"\p {L}", r"\p ← error: unexpected character ' '"),
            // multi-escapes are not allowed in ranges
            (
                r"[\x{61 62}-d]",
                r"[\x{61 62}-d← error: unexpected character 'd'",
            ),
            (
                r"[\x{61 63}-\x{62 64}]",
                r"[\x{61 63}-\← error: unexpected character '\\'",
            ),
            // TODO(#3558): This is a bad error message.
            (r"[a-\x{62 64}]", r"[a-\← error: unexpected character '\\'"),
        ];
        let vm = Default::default();
        for (source, expected_err) in cases {
            assert_is_error_and_message_eq(source, expected_err, &vm);
        }
    }

    #[test]
    fn test_consumed() {
        let cases = [
            (r"[a-z\]{[}]".len(), r"[a-z\]{[}][]"),
            (r"[a-z\]{[}]".len(), r"[a-z\]{[}] []"),
            (r"[a-z\]{[}]".len(), r"[a-z\]{]}] []"),
            (r"[a-z\]{{[}]".len(), r"[a-z\]{{]}] []"),
            (r"[a-z\]{[}]".len(), r"[a-z\]{]}]\p{L}"),
            (r"[a-z\]{[}]".len(), r"[a-z\]{]}]$var"),
        ];

        let vm = Default::default();
        for (expected_consumed, source) in cases {
            let (_, consumed) = parse(source).unwrap();
            assert_eq!(expected_consumed, consumed);
            let (_, consumed) = parse_with_variables(source, &vm).unwrap();
            assert_eq!(expected_consumed, consumed);
        }
    }
}
