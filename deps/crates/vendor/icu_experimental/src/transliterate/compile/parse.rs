// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

use super::*;
use crate::unicodeset_parse::{self as icu_unicodeset_parse, VariableMap, VariableValue};
use alloc::borrow::Cow;
use alloc::boxed::Box;
use alloc::string::ToString;
use alloc::vec;
use core::fmt::{Display, Formatter};
use core::{iter::Peekable, str::CharIndices};
use icu_collections::codepointinvlist::CodePointInversionList;
use icu_collections::codepointinvliststringlist::CodePointInversionListAndStringList;

type Result<T> = core::result::Result<T, CompileError>;

/// An element that can appear in a rule. Used for error reporting in [`CompileError`].
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub(crate) enum ElementKind {
    /// A literal string: `abc 'abc'`.
    Literal,
    /// A variable reference: `$var`.
    VariableReference,
    /// A backreference to a segment: `$1`.
    BackReference,
    /// A quantifier of any sort: `c*`, `c+`, `c?`.
    Quantifier,
    /// A segment: `(abc)`.
    Segment,
    /// A `UnicodeSet`: `[a-z]`.
    UnicodeSet,
    /// A function call: `&[a-z] Remove(...)`.
    FunctionCall,
    /// A cursor: `|`.
    Cursor,
    /// A start anchor: `^`.
    AnchorStart,
    /// An end anchor: `$`.
    AnchorEnd,
}

impl ElementKind {
    // returns true if the element has no effect in the location. this is not equivalent to
    // syntactically being allowed in that location.
    pub(crate) fn skipped_in(self, location: ElementLocation) -> bool {
        #[expect(clippy::match_like_matches_macro)] // I think the explicit match is clearer here
        match (location, self) {
            (ElementLocation::Source, Self::Cursor) => true,
            (ElementLocation::Target, Self::AnchorStart | Self::AnchorEnd) => true,
            _ => false,
        }
    }

    pub(crate) fn debug_str(self) -> &'static str {
        match self {
            ElementKind::Literal => "literal",
            ElementKind::VariableReference => "variable reference",
            ElementKind::BackReference => "back reference",
            ElementKind::Quantifier => "quantifier",
            ElementKind::Segment => "segment",
            ElementKind::UnicodeSet => "unicodeset",
            ElementKind::FunctionCall => "function call",
            ElementKind::Cursor => "cursor",
            ElementKind::AnchorStart => "start anchor",
            ElementKind::AnchorEnd => "end anchor",
        }
    }
}

/// The location in which an element can appear. Used for error reporting in [`CompileError`].
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub(crate) enum ElementLocation {
    /// The element appears on the source side of a rule (i.e., the side _not_ pointed at
    /// by the arrow).
    Source,
    /// The element appears on the target side of a rule (i.e., the side pointed at by the arrow).
    Target,
    /// The element appears inside a variable definition.
    VariableDefinition,
}

// the only UnicodeSets used in this crate are parsed, and thus 'static.
pub(crate) type UnicodeSet = CodePointInversionListAndStringList<'static>;
// UnicodeSets that are used as filters may not contain strings.
pub(crate) type FilterSet = CodePointInversionList<'static>;

#[derive(Debug, Clone, Copy)]
pub(crate) enum QuantifierKind {
    // ?
    ZeroOrOne,
    // *
    ZeroOrMore,
    // +
    OneOrMore,
}

// source-target/variant
#[derive(Debug, Clone, Hash, PartialEq, Eq)]
pub(crate) struct BasicId {
    pub(crate) source: String,
    pub(crate) target: String,
    pub(crate) variant: Option<String>,
}

impl BasicId {
    pub(crate) fn reverse(self) -> Self {
        let source = self.source.to_lowercase();
        let target = self.target.to_lowercase();
        let (new_source, new_target) = match (source.as_str(), target.as_str()) {
            // hardcoded inverses
            ("any", "lower") => (self.source, "Upper".to_string()),
            ("any", "upper") => (self.source, "Lower".to_string()),
            ("any", "nfc") => (self.source, "NFD".to_string()),
            ("any", "nfd") => (self.source, "NFC".to_string()),
            ("any", "nfkc") => (self.source, "NFKD".to_string()),
            ("any", "nfkd") => (self.source, "NFKC".to_string()),
            // no-ops
            ("any", "remove" | "null") => (self.source, self.target),
            // default inverse swaps source and target
            _ => (self.target, self.source),
        };

        Self {
            source: new_source,
            target: new_target,
            variant: self.variant,
        }
    }
}

impl Default for BasicId {
    fn default() -> Self {
        Self {
            source: "Any".to_string(),
            target: "Null".to_string(),
            variant: None,
        }
    }
}

impl Display for BasicId {
    fn fmt(&self, f: &mut Formatter<'_>) -> core::fmt::Result {
        write!(
            f,
            "{}-{}",
            self.source.to_ascii_lowercase(),
            self.target.to_ascii_lowercase()
        )?;
        if let Some(variant) = &self.variant {
            write!(f, "/{}", variant.to_ascii_lowercase())?;
        }
        Ok(())
    }
}

// [set] source-target/variant
#[derive(Debug, Clone)]
pub(crate) struct SingleId {
    pub(crate) filter: Option<FilterSet>,
    pub(crate) basic_id: BasicId,
}

impl SingleId {
    pub(crate) fn reverse(self) -> Self {
        Self {
            basic_id: self.basic_id.reverse(),
            ..self
        }
    }
}

#[derive(Debug, Clone)]
pub(crate) enum Element {
    // Examples:
    //  - hello\ world
    //  - 'hello world'
    Literal(String),
    // Example: $my_var
    VariableRef(String),
    // Example: $12
    BackRef(u32),
    // Examples:
    //  - <element>?
    //  - <element>*
    //  - <element>+
    // note: Box<Element> instead of Section, because a quantifier only ever refers to the immediately preceding element.
    // segments or variable refs are used to group multiple elements together.
    Quantifier(QuantifierKind, Box<Element>),
    // Example: (<element> <element> ...)
    Segment(Section),
    // Example: [:^L:]
    UnicodeSet(UnicodeSet),
    // Example: &[a-z] Any-Remove(<element> <element> ...)
    // single id, function arguments
    FunctionCall(SingleId, Section),
    // Example: @@@@ |, |@@@@
    Cursor(u32, u32),
    // '^'
    AnchorStart,
    // '$'
    AnchorEnd,
}

impl Element {
    pub(crate) fn kind(&self) -> ElementKind {
        match self {
            Element::Literal(..) => ElementKind::Literal,
            Element::VariableRef(..) => ElementKind::VariableReference,
            Element::BackRef(..) => ElementKind::BackReference,
            Element::Quantifier(..) => ElementKind::Quantifier,
            Element::Segment(..) => ElementKind::Segment,
            Element::UnicodeSet(..) => ElementKind::UnicodeSet,
            Element::FunctionCall(..) => ElementKind::FunctionCall,
            Element::Cursor(..) => ElementKind::Cursor,
            Element::AnchorStart => ElementKind::AnchorStart,
            Element::AnchorEnd => ElementKind::AnchorEnd,
        }
    }
}

pub(crate) type Section = Vec<Element>;

#[derive(Debug, Clone)]
pub(crate) struct HalfRule {
    pub(crate) ante: Section,
    pub(crate) key: Section,
    pub(crate) post: Section,
}

#[derive(Debug, Clone)]
pub(crate) enum Rule {
    GlobalFilter(FilterSet),
    GlobalInverseFilter(FilterSet),
    // forward and backward IDs.
    // "A (B)" is Transform(A, Some(B)),
    // "(B)" is Transform(Null, Some(B)),
    // "A" is Transform(A, None), which indicates an auto-computed reverse ID,
    // "A ()" is Transform(A, Some(Null))
    Transform(SingleId, Option<SingleId>),
    Conversion(HalfRule, Direction, HalfRule),
    VariableDefinition(String, Section),
}

pub(crate) struct Parser<'a, P: ?Sized> {
    iter: Peekable<CharIndices<'a>>,
    source: &'a str,
    // flattened variable map specifically for unicodesets, i.e., only contains variables that
    // are chars, strings, or UnicodeSets when all variables are inlined.
    variable_map: VariableMap<'static>,
    // cached set for the special set .
    dot_set: Option<UnicodeSet>,
    // for variable identifiers (XID Start, XID Continue)
    xid_start: &'a CodePointInversionList<'a>,
    xid_continue: &'a CodePointInversionList<'a>,
    // for skipped whitespace (Pattern White Space)
    pat_ws: &'a CodePointInversionList<'a>,
    property_provider: &'a P,
}

impl<'a, P> Parser<'a, P>
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
    // initiates a line comment
    const COMMENT: char = '#';
    // terminates a line comment
    const COMMENT_END: char = '\n';
    // terminates a rule
    const RULE_END: char = ';';
    // initiates a filter or transform rule, as part of '::'
    const SPECIAL_START: char = ':';
    // initiates a UnicodeSet
    const SET_START: char = '[';
    // equivalent to the UnicodeSet [^[:Zp:][:Zl:]\r\n$]
    const DOT: char = '.';
    const DOT_SET: &'static str = r"[^[:Zp:][:Zl:]\r\n$]";
    // matches the beginning of the input
    const ANCHOR_START: char = '^';
    // initiates a segment or the reverse portion of an ID
    const OPEN_PAREN: char = '(';
    // terminates a segment or the reverse portion of an ID
    const CLOSE_PAREN: char = ')';
    // separates source and target of an ID
    const ID_SEP: char = '-';
    // separates variant from ID
    const VARIANT_SEP: char = '/';
    // variable reference prefix, and anchor end character
    const VAR_PREFIX: char = '$';
    // variable definition operator
    const VAR_DEF_OP: char = '=';
    // left context
    const LEFT_CONTEXT: char = '{';
    // right context
    const RIGHT_CONTEXT: char = '}';
    // optional quantifier
    const OPTIONAL: char = '?';
    // zero or more quantifier
    const ZERO_OR_MORE: char = '*';
    // one or more quantifier
    const ONE_OR_MORE: char = '+';
    // function prefix
    const FUNCTION_PREFIX: char = '&';
    // quoted literals
    const QUOTE: char = '\'';
    // escape character
    const ESCAPE: char = '\\';
    // cursor
    const CURSOR: char = '|';
    // before or after a cursor
    const CURSOR_PLACEHOLDER: char = '@';

    pub(super) fn run(
        source: &'a str,
        xid_start: &'a CodePointInversionList<'a>,
        xid_continue: &'a CodePointInversionList<'a>,
        pat_ws: &'a CodePointInversionList<'a>,
        provider: &'a P,
    ) -> Result<Vec<Rule>> {
        let mut s = Self {
            iter: source.char_indices().peekable(),
            source,
            variable_map: Default::default(),
            dot_set: None,
            xid_start,
            xid_continue,
            pat_ws,
            property_provider: provider,
        };

        let mut rules = Vec::new();
        s.skip_icu_pragma();

        loop {
            s.skip_whitespace();
            if s.iter.peek().is_none() {
                break;
            }
            // we skipped whitespace and comments, so any other chars must be part of a rule
            rules.push(s.parse_rule()?);
        }

        Ok(rules)
    }

    // skips ICU4C pragmas of the form `use variable range 0xFFFF 0xFFFF ;`
    fn skip_icu_pragma(&mut self) {
        loop {
            self.skip_whitespace();
            if let Some(start) = self.peek_index() {
                let start_source = &self.source[start..];
                if start_source.starts_with("use variable range 0x") {
                    let conv_idx = start_source.find(['>', '<', '→', '←', '↔']);
                    let end_idx = start_source.find(Self::RULE_END);
                    // avoid cases like `use variable range 0x70 0x72 > b ;` which not pragmas
                    match (end_idx, conv_idx) {
                        (Some(end_idx), Some(conv_idx)) if conv_idx < end_idx => break,
                        (None, Some(_)) => break,
                        _ => {}
                    }
                    self.skip_until(Self::RULE_END);
                    // search for another pragma
                    continue;
                }
            }
            break;
        }
    }

    // expects a rule
    fn parse_rule(&mut self) -> Result<Rule> {
        match self.must_peek_char()? {
            Self::SPECIAL_START => self.parse_filter_or_transform_rule(),
            // must be a conversion or variable rule
            _ => self.parse_conversion_or_variable_rule(),
        }
    }

    // any rules starting with '::'
    fn parse_filter_or_transform_rule(&mut self) -> Result<Rule> {
        // Syntax:
        // '::' <unicodeset> ';'                  # global filter
        // '::' '(' <unicodeset> ')' ';'          # global inverse filter
        // '::' <single-id> (<single-id>)? ';'    # transform rule

        self.consume(Self::SPECIAL_START)?;
        self.consume(Self::SPECIAL_START)?;

        // because all three options can start with a UnicodeSet, we just try to parse everything
        // into options, and assemble at the end

        let (forward_filter, forward_basic_id, reverse_filter, reverse_basic_id, has_reverse) =
            self.parse_filter_or_transform_rule_parts()?;

        self.skip_whitespace();

        // the offset of ';'
        let meta_err_offset = self.must_peek_index()?;
        self.consume(Self::RULE_END)?;

        // try to assemble the rule
        // first try global filters
        match (
            forward_filter.is_some(),
            forward_basic_id.is_some(),
            reverse_filter.is_some(),
            reverse_basic_id.is_some(),
        ) {
            (true, false, false, false) => {
                // by match, forward_filter.is_some() is true
                #[expect(clippy::unwrap_used)]
                return Ok(Rule::GlobalFilter(forward_filter.unwrap()));
            }
            (false, false, true, false) => {
                // by match, reverse_filter.is_some() is true
                #[expect(clippy::unwrap_used)]
                return Ok(Rule::GlobalInverseFilter(reverse_filter.unwrap()));
            }
            _ => {}
        }

        // if this is not a global (inverse) filter rule, this must be a transform rule

        // either forward_basic_id or reverse_basic_id must be nonempty
        if forward_basic_id.is_none() && reverse_basic_id.is_none() {
            return Err(CompileErrorKind::InvalidId.with_offset(meta_err_offset));
        }

        if !has_reverse {
            // we must have a forward id due to:
            //  1. !has_reverse implying reverse_basic_id.is_none()
            //  2. the above none checks implying forward_basic_id.is_some()
            // because this is difficult to verify, returning a CompileErrorKind::Internal anyway
            // instead of unwrapping, despite technically being unnecessary
            let forward_basic_id =
                forward_basic_id.ok_or(CompileErrorKind::Internal("transform rule logic error"))?;
            return Ok(Rule::Transform(
                SingleId {
                    basic_id: forward_basic_id,
                    filter: forward_filter,
                },
                None,
            ));
        }

        if forward_filter.is_some() && forward_basic_id.is_none()
            || reverse_filter.is_some() && reverse_basic_id.is_none()
        {
            // cannot have a filter without a basic id
            return Err(CompileErrorKind::InvalidId.with_offset(meta_err_offset));
        }

        // an empty forward rule, such as ":: (R) ;" is equivalent to ":: Any-Null (R) ;"
        let forward_basic_id = forward_basic_id.unwrap_or_else(BasicId::default);
        // an empty reverse rule, such as ":: F () ;" is equivalent to ":: F (Any-Null) ;"
        let reverse_basic_id = reverse_basic_id.unwrap_or_else(BasicId::default);

        let forward_single_id = SingleId {
            basic_id: forward_basic_id,
            filter: forward_filter,
        };
        let reverse_single_id = SingleId {
            basic_id: reverse_basic_id,
            filter: reverse_filter,
        };

        Ok(Rule::Transform(forward_single_id, Some(reverse_single_id)))
    }

    // consumes everything between '::' and ';', exclusive.
    #[expect(clippy::type_complexity)] // used internally in one place only
    fn parse_filter_or_transform_rule_parts(
        &mut self,
    ) -> Result<(
        Option<FilterSet>,
        Option<BasicId>,
        Option<FilterSet>,
        Option<BasicId>,
        bool,
    )> {
        // parse forward things, i.e., everything until Self::OPEN_PAREN
        self.skip_whitespace();
        let forward_filter = self.try_parse_filter_set()?;
        self.skip_whitespace();
        let forward_basic_id = self.try_parse_basic_id()?;
        self.skip_whitespace();

        let has_reverse = match self.must_peek_char()? {
            // initiates a reverse id
            Self::OPEN_PAREN => true,
            // we're done parsing completely, no reverse id
            Self::RULE_END => false,
            _ => return self.unexpected_char_here(),
        };

        let reverse_filter;
        let reverse_basic_id;

        if has_reverse {
            // if we have a reverse, parse it
            self.consume(Self::OPEN_PAREN)?;
            self.skip_whitespace();
            reverse_filter = self.try_parse_filter_set()?;
            self.skip_whitespace();
            reverse_basic_id = self.try_parse_basic_id()?;
            self.skip_whitespace();
            self.consume(Self::CLOSE_PAREN)?;
        } else {
            reverse_filter = None;
            reverse_basic_id = None;
        }

        Ok((
            forward_filter,
            forward_basic_id,
            reverse_filter,
            reverse_basic_id,
            has_reverse,
        ))
    }

    fn parse_conversion_or_variable_rule(&mut self) -> Result<Rule> {
        // Syntax:
        // <variable_ref> '=' <section> ';'           # variable rule
        // <half-rule> <direction> <half-rule> ';'    # conversion rule

        // try parsing into a variable rule
        let first_elt = if Self::VAR_PREFIX == self.must_peek_char()? {
            let elt = self.parse_variable_or_backref_or_anchor_end()?;
            self.skip_whitespace();
            if Self::VAR_DEF_OP == self.must_peek_char()? {
                // must be variable ref
                let var_name = match elt {
                    Element::VariableRef(var_name) => var_name,
                    _ => return self.unexpected_char_here(),
                };
                self.iter.next();
                let section = self.parse_section(None)?;
                let err_offset = self.must_peek_index()?;
                self.consume(Self::RULE_END)?;
                self.add_variable(var_name.clone(), section.clone(), err_offset)?;
                return Ok(Rule::VariableDefinition(var_name, section));
            }
            Some(elt)
        } else {
            None
        };

        // must be conversion rule
        // passing down first_elt that was already parsed for the variable rule check
        let first_half = self.parse_half_rule(first_elt)?;

        let dir = self.parse_direction()?;

        let second_half = self.parse_half_rule(None)?;
        self.consume(Self::RULE_END)?;
        Ok(Rule::Conversion(first_half, dir, second_half))
    }

    fn parse_single_id(&mut self) -> Result<SingleId> {
        // Syntax:
        // <unicodeset>? <basic-id>

        self.skip_whitespace();
        let filter = self.try_parse_filter_set()?;
        self.skip_whitespace();
        let basic_id = self.parse_basic_id()?;
        Ok(SingleId { filter, basic_id })
    }

    fn try_parse_basic_id(&mut self) -> Result<Option<BasicId>> {
        if let Some(c) = self.peek_char() {
            if self.xid_start.contains(c) {
                return Ok(Some(self.parse_basic_id()?));
            }
        }
        Ok(None)
    }

    // TODO(#3736): factor this out for runtime ID parsing?
    fn parse_basic_id(&mut self) -> Result<BasicId> {
        // Syntax:
        // <identifier> ('-' <identifier>)? ('/' <identifier>)?

        // we must have at least one identifier. the implicit "Null" id is only allowed
        // in a '::'-rule, which is handled explicitly.
        let first_id = self.parse_unicode_identifier()?;

        self.skip_whitespace();
        let second_id = self.try_parse_sep_and_unicode_identifier(Self::ID_SEP)?;
        self.skip_whitespace();
        let variant_id = self.try_parse_sep_and_unicode_identifier(Self::VARIANT_SEP)?;

        let (source, target) = match second_id {
            None => ("Any".to_string(), first_id),
            Some(second_id) => (first_id, second_id),
        };

        Ok(BasicId {
            source,
            target,
            variant: variant_id,
        })
    }

    fn try_parse_sep_and_unicode_identifier(&mut self, sep: char) -> Result<Option<String>> {
        if Some(sep) == self.peek_char() {
            self.iter.next();
            self.skip_whitespace();
            // at this point we must be parsing a identifier
            return Ok(Some(self.parse_unicode_identifier()?));
        }
        Ok(None)
    }

    // parses an XID-based identifier
    fn parse_unicode_identifier(&mut self) -> Result<String> {
        // Syntax:
        // <xid_start> (<xid_continue>)*

        let mut id = String::new();

        let (first_offset, first_c) = self.must_peek()?;
        if !self.xid_start.contains(first_c) {
            return Err(CompileErrorKind::UnexpectedChar(first_c).with_offset(first_offset));
        }
        self.iter.next();
        id.push(first_c);

        loop {
            let c = self.must_peek_char()?;
            if !self.xid_continue.contains(c) {
                break;
            }
            id.push(c);
            self.iter.next();
        }

        Ok(id)
    }

    fn parse_half_rule(&mut self, prev_elt: Option<Element>) -> Result<HalfRule> {
        // Syntax:
        // (<section> '{')? <section> ('}' <section>)?

        let ante;
        let key;
        let post;
        let first = self.parse_section(prev_elt)?;
        if Self::LEFT_CONTEXT == self.must_peek_char()? {
            self.iter.next();
            ante = first;
            key = self.parse_section(None)?;
        } else {
            ante = vec![];
            key = first;
        }
        if Self::RIGHT_CONTEXT == self.must_peek_char()? {
            self.iter.next();
            post = self.parse_section(None)?;
        } else {
            post = vec![];
        }

        Ok(HalfRule { ante, key, post })
    }

    fn parse_direction(&mut self) -> Result<Direction> {
        // Syntax:
        // '<' | '>' | '<>' | '→' | '←' | '↔'

        match self.must_peek_char()? {
            '>' | '→' => {
                self.iter.next();
                Ok(Direction::Forward)
            }
            '↔' => {
                self.iter.next();
                Ok(Direction::Both)
            }
            '←' => {
                self.iter.next();
                Ok(Direction::Reverse)
            }
            '<' => {
                self.iter.next();
                match self.must_peek_char()? {
                    '>' => {
                        self.iter.next();
                        Ok(Direction::Both)
                    }
                    _ => Ok(Direction::Reverse),
                }
            }
            _ => self.unexpected_char_here(),
        }
    }

    // whitespace before and after is consumed
    fn parse_section(&mut self, prev_elt: Option<Element>) -> Result<Section> {
        let mut section = Section::new();
        let mut prev_elt = prev_elt;

        loop {
            self.skip_whitespace();
            let c = self.must_peek_char()?;
            if self.is_section_end(c) {
                if let Some(elt) = prev_elt.take() {
                    section.push(elt);
                }
                break;
            }

            let next_elt = self.parse_element(&mut prev_elt)?;

            if let Some(elt) = prev_elt {
                section.push(elt);
            }
            prev_elt = Some(next_elt);
        }

        Ok(section)
    }

    fn parse_quantifier_kind(&mut self) -> Result<QuantifierKind> {
        match self.must_peek_char()? {
            Self::OPTIONAL => {
                self.iter.next();
                Ok(QuantifierKind::ZeroOrOne)
            }
            Self::ZERO_OR_MORE => {
                self.iter.next();
                Ok(QuantifierKind::ZeroOrMore)
            }
            Self::ONE_OR_MORE => {
                self.iter.next();
                Ok(QuantifierKind::OneOrMore)
            }
            _ => self.unexpected_char_here(),
        }
    }

    fn parse_element(&mut self, prev_elt: &mut Option<Element>) -> Result<Element> {
        match self.must_peek_char()? {
            Self::VAR_PREFIX => self.parse_variable_or_backref_or_anchor_end(),
            Self::ANCHOR_START => {
                self.iter.next();
                Ok(Element::AnchorStart)
            }
            Self::OPEN_PAREN => self.parse_segment(),
            Self::DOT => {
                self.iter.next();
                Ok(Element::UnicodeSet(self.get_dot_set()?))
            }
            Self::OPTIONAL | Self::ZERO_OR_MORE | Self::ONE_OR_MORE => {
                let quantifier = self.parse_quantifier_kind()?;
                if let Some(elt) = prev_elt.take() {
                    Ok(Element::Quantifier(quantifier, Box::new(elt)))
                } else {
                    self.unexpected_char_here()
                }
            }
            Self::FUNCTION_PREFIX => self.parse_function_call(),
            Self::CURSOR_PLACEHOLDER | Self::CURSOR => self.parse_cursor(),
            Self::QUOTE => Ok(Element::Literal(self.parse_quoted_literal()?)),
            _ if self.peek_is_unicode_set_start() => {
                let (_, set) = self.parse_unicode_set()?;
                Ok(Element::UnicodeSet(set))
            }
            c if self.is_valid_unquoted_literal(c) => Ok(Element::Literal(self.parse_literal()?)),
            _ => self.unexpected_char_here(),
        }
    }

    fn parse_variable_or_backref_or_anchor_end(&mut self) -> Result<Element> {
        self.consume(Self::VAR_PREFIX)?;

        match self.must_peek_char()? {
            c if c.is_ascii_digit() => {
                // we have a backref
                let num = self.parse_number()?;
                Ok(Element::BackRef(num))
            }
            c if self.xid_start.contains(c) => {
                // we have a variable
                let variable_id = self.parse_unicode_identifier()?;
                Ok(Element::VariableRef(variable_id))
            }
            _ => {
                // this was an anchor end
                Ok(Element::AnchorEnd)
            }
        }
    }

    fn parse_number(&mut self) -> Result<u32> {
        let (first_offset, first_c) = self.must_next()?;
        if !matches!(first_c, '1'..='9') {
            return Err(CompileErrorKind::UnexpectedChar(first_c).with_offset(first_offset));
        }
        // inclusive end offset
        let mut end_offset = first_offset;

        loop {
            let (offset, c) = self.must_peek()?;
            if !c.is_ascii_digit() {
                break;
            }
            self.iter.next();
            end_offset = offset;
        }

        // first_offset is valid by `Chars`, and the inclusive end_offset
        // is valid because we only set it to the indices of ASCII chars,
        // which are all exactly 1 UTF-8 byte
        self.source[first_offset..=end_offset]
            .parse()
            .map_err(|_| CompileErrorKind::InvalidNumber.with_offset(end_offset))
    }

    fn parse_literal(&mut self) -> Result<String> {
        let mut buf = String::new();
        loop {
            let c = self.must_peek_char()?;
            if c == Self::ESCAPE {
                self.parse_escaped_char_into_buf(&mut buf)?;
                continue;
            }
            if !self.is_valid_unquoted_literal(c) {
                break;
            }
            self.iter.next();
            buf.push(c);
        }
        Ok(buf)
    }

    fn parse_quoted_literal(&mut self) -> Result<String> {
        // Syntax:
        // \' [^']* \'

        let mut buf = String::new();
        self.consume(Self::QUOTE)?;
        loop {
            let c = self.must_next_char()?;
            if c == Self::QUOTE {
                break;
            }
            buf.push(c);
        }
        if buf.is_empty() {
            // '' is the escaped version of a quote
            buf.push(Self::QUOTE);
        }
        Ok(buf)
    }

    // parses all supported escapes. code is somewhat duplicated from icu_unicodeset_parse
    // might want to deduplicate this with unicodeset_parse somehow
    fn parse_escaped_char_into_buf(&mut self, buf: &mut String) -> Result<()> {
        self.consume(Self::ESCAPE)?;

        let (offset, next_char) = self.must_next()?;

        match next_char {
            'u' | 'x' if self.peek_char() == Some('{') => {
                // bracketedHex
                self.iter.next();

                // the first codepoint is mandatory
                self.skip_whitespace();
                let c = self.parse_hex_digits_into_char(1, 6)?;
                buf.push(c);

                loop {
                    let skipped = self.skip_whitespace();
                    let next_char = self.must_peek_char()?;
                    if next_char == '}' {
                        self.iter.next();
                        break;
                    }
                    if skipped == 0 {
                        // multiple code points must be separated in multi escapes
                        return self.unexpected_char_here();
                    }

                    let c = self.parse_hex_digits_into_char(1, 6)?;
                    buf.push(c);
                }
            }
            'u' => {
                // 'u' hex{4}
                let c = self.parse_hex_digits_into_char(4, 4)?;
                buf.push(c);
            }
            'x' => {
                // 'x' hex{2}
                let c = self.parse_hex_digits_into_char(2, 2)?;
                buf.push(c);
            }
            'U' => {
                // 'U00' ('0' hex{5} | '10' hex{4})
                let c = self.parse_hex_digits_into_char(6, 6)?;
                buf.push(c);
            }
            'N' => {
                // parse code point with name in {}
                // tracking issue: https://github.com/unicode-org/icu4x/issues/1397
                return Err(CompileErrorKind::Unimplemented.with_offset(offset));
            }
            'a' => buf.push('\u{0007}'),
            'b' => buf.push('\u{0008}'),
            't' => buf.push('\u{0009}'),
            'n' => buf.push('\u{000A}'),
            'v' => buf.push('\u{000B}'),
            'f' => buf.push('\u{000C}'),
            'r' => buf.push('\u{000D}'),
            _ => buf.push(next_char),
        }
        Ok(())
    }

    fn parse_hex_digits_into_char(&mut self, min: usize, max: usize) -> Result<char> {
        let first_offset = self.must_peek_index()?;
        let end_offset = self.validate_hex_digits(min, max)?;

        // validate_hex_digits ensures that chars (including the last one) are ascii hex digits,
        // which are all exactly one UTF-8 byte long, so slicing on these offsets always respects char boundaries
        let hex_source = &self.source[first_offset..=end_offset];
        let num = u32::from_str_radix(hex_source, 16)
            .map_err(|_| CompileErrorKind::Internal("expected valid hex escape"))?;
        char::try_from(num).map_err(|_| CompileErrorKind::InvalidEscape.with_offset(end_offset))
    }

    // validates [0-9a-fA-F]{min,max}, returns the offset of the last digit, consuming everything in the process
    fn validate_hex_digits(&mut self, min: usize, max: usize) -> Result<usize> {
        let mut last_offset = 0;
        for count in 0..max {
            let (offset, c) = self.must_peek()?;
            if !c.is_ascii_hexdigit() {
                if count < min {
                    return self.unexpected_char_here();
                } else {
                    break;
                }
            }
            self.iter.next();
            last_offset = offset;
        }
        Ok(last_offset)
    }

    fn parse_segment(&mut self) -> Result<Element> {
        self.consume(Self::OPEN_PAREN)?;
        let elt = Element::Segment(self.parse_section(None)?);
        self.consume(Self::CLOSE_PAREN)?;
        Ok(elt)
    }

    fn try_parse_filter_set(&mut self) -> Result<Option<FilterSet>> {
        if self.peek_is_unicode_set_start() {
            let (offset, set) = self.parse_unicode_set()?;
            if set.has_strings() {
                return Err(CompileErrorKind::GlobalFilterWithStrings.with_offset(offset));
            }
            return Ok(Some(set.code_points().clone()));
        }
        Ok(None)
    }

    fn parse_unicode_set(&mut self) -> Result<(usize, UnicodeSet)> {
        let pre_offset = self.must_peek_index()?;
        // pre_offset is a valid index because self.iter (used in must_peek_index)
        // was created from self.source
        let set_source = &self.source[pre_offset..];
        let (set, consumed_bytes) = icu_unicodeset_parse::parse_unstable_with_variables(
            set_source,
            &self.variable_map,
            self.property_provider,
        )
        .map_err(|e| CompileErrorKind::UnicodeSetError(e).with_offset(pre_offset))?;

        let mut last_offset = pre_offset;
        // advance self.iter consumed_bytes bytes
        while let Some(offset) = self.peek_index() {
            // we can use equality because unicodeset_parse also lexes on char boundaries
            // note: we must not consume this final token because it is the first non-consumed char
            if offset == pre_offset + consumed_bytes {
                break;
            }
            last_offset = offset;
            self.iter.next();
        }

        Ok((last_offset, set))
    }

    fn get_dot_set(&mut self) -> Result<UnicodeSet> {
        match &self.dot_set {
            Some(set) => Ok(set.clone()),
            None => {
                let (set, _) =
                    icu_unicodeset_parse::parse_unstable(Self::DOT_SET, self.property_provider)
                        .map_err(|_| CompileErrorKind::Internal("dot set syntax not valid"))?;
                self.dot_set = Some(set.clone());
                Ok(set)
            }
        }
    }

    fn parse_function_call(&mut self) -> Result<Element> {
        self.consume(Self::FUNCTION_PREFIX)?;

        // parse single-id
        let single_id = self.parse_single_id()?;
        self.skip_whitespace();
        self.consume(Self::OPEN_PAREN)?;
        let section = self.parse_section(None)?;
        self.consume(Self::CLOSE_PAREN)?;

        Ok(Element::FunctionCall(single_id, section))
    }

    fn parse_cursor(&mut self) -> Result<Element> {
        // Syntax:
        // '@'* '|' '@'*

        let mut num_pre = 0;
        let mut num_post = 0;
        // parse pre
        loop {
            self.skip_whitespace();
            match self.must_peek_char()? {
                Self::CURSOR_PLACEHOLDER => {
                    self.iter.next();
                    num_pre += 1;
                }
                Self::CURSOR => {
                    self.iter.next();
                    break;
                }
                _ => return self.unexpected_char_here(),
            }
        }
        // parse post
        loop {
            self.skip_whitespace();
            match self.must_peek_char()? {
                Self::CURSOR_PLACEHOLDER => {
                    self.iter.next();
                    num_post += 1;
                }
                _ => break,
            }
        }

        Ok(Element::Cursor(num_pre, num_post))
    }

    fn add_variable(&mut self, name: String, value: Section, offset: usize) -> Result<()> {
        if let Some(uset_value) = self.try_uset_flatten_section(&value) {
            self.variable_map
                .insert(name.to_string(), uset_value)
                .map_err(|_| CompileErrorKind::DuplicateVariable.with_offset(offset))?;
        }
        Ok(())
    }

    fn try_uset_flatten_section(&self, section: &Section) -> Option<VariableValue<'static>> {
        // note: could avoid some clones here if the VariableMap stored &T's (or both), but that is
        // quite the edge case in transliterator source files

        // is this just a unicode set?
        if let [Element::UnicodeSet(set)] = &section[..] {
            return Some(VariableValue::UnicodeSet(set.clone()));
        }
        // if it's just a variable that is already a valid uset variable, we return that
        if let [Element::VariableRef(name)] = &section[..] {
            if let Some(value) = self.variable_map.get(name) {
                return Some(value.clone());
            }
            return None;
        }

        // if not, must be a string literal
        let mut combined_literal = String::new();
        for elt in section {
            match elt {
                Element::Literal(s) => combined_literal.push_str(s),
                Element::VariableRef(name) => match self.variable_map.get(name) {
                    Some(VariableValue::String(s)) => combined_literal.push_str(s),
                    Some(VariableValue::Char(c)) => combined_literal.push(*c),
                    _ => return None,
                },
                _ => return None,
            }
        }
        Some(VariableValue::String(Cow::Owned(combined_literal)))
    }

    fn consume(&mut self, expected: char) -> Result<()> {
        match self.must_next()? {
            (offset, c) if c != expected => {
                Err(CompileErrorKind::UnexpectedChar(c).with_offset(offset))
            }
            _ => Ok(()),
        }
    }

    // skips whitespace and comments, returns the number of skipped chars
    fn skip_whitespace(&mut self) -> usize {
        let mut count = 0;
        while let Some(c) = self.peek_char() {
            if c == Self::COMMENT {
                count += self.skip_until(Self::COMMENT_END);
                continue;
            }
            if !self.pat_ws.contains(c) {
                break;
            }
            self.iter.next();
            count += 1;
        }
        count
    }

    // skips until the next occurrence of c, which is also consumed
    // returns the number of skipped chars
    fn skip_until(&mut self, end: char) -> usize {
        let mut count = 0;
        for (_, c) in self.iter.by_ref() {
            count += 1;
            if c == end {
                break;
            }
        }
        count
    }

    fn peek_is_unicode_set_start(&mut self) -> bool {
        match self.peek_char() {
            Some(Self::SET_START) => true,
            Some(Self::ESCAPE) => {
                let mut it = self.iter.clone();
                // skip past the ESCAPE
                it.next();
                matches!(it.next(), Some((_, 'p' | 'P')))
            }
            _ => false,
        }
    }

    fn peek_char(&mut self) -> Option<char> {
        self.iter.peek().map(|(_, c)| *c)
    }

    fn peek_index(&mut self) -> Option<usize> {
        self.iter.peek().map(|(idx, _)| *idx)
    }

    // use this whenever an empty iterator would imply an Eof error
    fn must_next(&mut self) -> Result<(usize, char)> {
        self.iter
            .next()
            .ok_or(CompileErrorKind::Eof.without_offset())
    }

    // see must_next
    fn must_next_char(&mut self) -> Result<char> {
        self.must_next().map(|(_, c)| c)
    }

    // use this whenever an empty iterator would imply an Eof error
    fn must_peek(&mut self) -> Result<(usize, char)> {
        self.iter
            .peek()
            .copied()
            .ok_or(CompileErrorKind::Eof.without_offset())
    }

    // see must_peek
    fn must_peek_char(&mut self) -> Result<char> {
        self.must_peek().map(|(_, c)| c)
    }

    // see must_peek
    fn must_peek_index(&mut self) -> Result<usize> {
        self.must_peek().map(|(idx, _)| idx)
    }

    fn unexpected_char_here<T>(&mut self) -> Result<T> {
        let (offset, char) = self.must_peek()?;
        Err(CompileErrorKind::UnexpectedChar(char).with_offset(offset))
    }

    fn is_section_end(&self, c: char) -> bool {
        matches!(
            c,
            Self::RULE_END
                | Self::CLOSE_PAREN
                | Self::RIGHT_CONTEXT
                | Self::LEFT_CONTEXT
                | Self::VAR_DEF_OP
                | '<'
                | '>'
                | '→'
                | '←'
                | '↔'
        )
    }

    fn is_valid_unquoted_literal(&self, c: char) -> bool {
        // allowing \ since it's used for escapes, which are allowed in an unquoted context
        c.is_ascii() && (c.is_ascii_alphanumeric() || c == '\\')
            || (!c.is_ascii() && c != '→' && c != '←' && c != '↔')
    }
}

#[cfg(test)]
pub(super) fn parse(source: &str) -> Result<Vec<Rule>> {
    use icu_properties::CodePointSetData;
    Parser::run(
        source,
        &CodePointSetData::new::<XidStart>()
            .static_to_owned()
            .to_code_point_inversion_list(),
        &CodePointSetData::new::<XidContinue>()
            .static_to_owned()
            .to_code_point_inversion_list(),
        &CodePointSetData::new::<PatternWhiteSpace>()
            .static_to_owned()
            .to_code_point_inversion_list(),
        &icu_properties::provider::Baked,
    )
}

#[test]
fn test_full() {
    let source = r"

    # these are skipped:
    use variable range 0x70 0x72 ;
    use variable range 0x1 0x2 ;

    :: [a-z\]] ; :: [b-z] Latin/BGN ;
    :: Source-Target/Variant () ;::([b-z]Target-Source/Variant) ;
    :: [a-z] Any ([b-z] Target-Source/Variant);

    $my_var = an arbitrary section ',' some quantifiers *+? 'and other variables: $var' $var  ;
    $innerMinus = '-' ;
    $minus = $innerMinus ;
    $good_set = [a $minus z] ;

    ^ (start) { key ' key '+ $good_set } > $102 }  post\-context$;
    # contexts are optional
    target < source [{set\ with\ string}];
    # contexts can be empty
    { 'source-or-target' } <> { 'target-or-source' } ;

    (nested (sections)+ are () so fun) > ;

    . > ;

    :: ([inverse-filter]) ;
    ";

    parse(source).map_err(|e| e.explain(source)).unwrap();
}

#[test]
fn test_conversion_rules_ok() {
    let sources = [
        r"a > b ;",
        r"a < b ;",
        r"a <> b ;",
        r"a → b ;",
        r"a ← b ;",
        r"a ↔ b ;",
        r"a \> > b ;",
        r"a \→ > b ;",
        r"{ a > b ;",
        r"a {  > b ;",
        r"{ a } > b ;",
        r"{ a } > { b ;",
        r"{ a } > { b } ;",
        r"^ pre [a-z] { a } post [$] $ > ^ [$] pre { b [b-z] } post $ ;",
        r"[äöü] > ;",
        r"([äöü]) > &Remove($1) ;",
        r"[äöü] { ([äöü]+) > &Remove($1) ;",
        r"|@@@ a <> b @@@@  @ | ;",
        r"|a <> b ;",
    ];

    for source in sources {
        parse(source).map_err(|e| e.explain(source)).unwrap();
    }
}

#[test]
fn test_conversion_rules_err() {
    let sources = [
        r"a > > b ;",
        r"a >< b ;",
        r"(a > b) > b ;",
        r"a \← b ;",
        r"a ↔ { b > } ;",
        r"a ↔ { b > } ;",
        r"a > b",
        r"@ a > b ;",
        r"a ( {  > b ;",
        r"a ( { )  > b ;",
        r"a } + > b ;",
        r"a (+?*) > b ;",
        r"+?* > b ;",
        r"+ > b ;",
        r"* > b ;",
        r"? > b ;",
        r"use variable range 0x71 > 2 ; use variable range 0x71 ;",
    ];

    for source in sources {
        parse(source).unwrap_err();
    }
}

#[test]
fn test_variable_rules_ok() {
    let sources = [
        r" $my_var = [a-z] ;",
        r"$my_var = äüöÜ ;",
        r"$my_var = [a-z] literal ; $other_var = [A-Z] [b-z];",
        r"$my_var = [a-z] ; $other_var = [A-Z] [b-z];",
        r"$my_var = [a-z] ; $other_var = $my_var + $2222;",
        r"$my_var = [a-z] ; $other_var = $my_var \+\ \$2222 \\ 'hello\';",
        r"
        $innerMinus = '-' ;
        $minus = $innerMinus ;
        $good_set = [a $minus z] ;
        ",
    ];

    for source in sources {
        parse(source).map_err(|e| e.explain(source)).unwrap();
    }
}

#[test]
fn test_variable_rules_err() {
    let sources = [
        r" $ my_var = a ;",
        r" $my_var = a_2 ;",
        r"$my_var 2 = [a-z] literal ;",
        r"$my_var = [$doesnt_exist] ;",
    ];

    for source in sources {
        if let Ok(rules) = parse(source) {
            panic!("Parsed invalid source {source:?}: {rules:?}");
        }
    }
}

#[test]
fn test_global_filters_ok() {
    let sources = [
        r":: [^\[$] ;",
        r":: \p{L} ;",
        r":: [^\[{[}$] ;",
        r":: [^\[{]}$] ;",
        r":: [^\[{]\}]}$] ;",
        r":: ([^\[$]) ;",
        r":: ( [^\[$] ) ;",
        r":: [^[a-z[]][]] ;",
        r":: [^[a-z\[\]]\]] ;",
        r":: [^\]] ;",
    ];

    for source in sources {
        parse(source).map_err(|e| e.explain(source)).unwrap();
    }
}

#[test]
fn test_global_filters_err() {
    let sources = [
        r":: [^\[$ ;",
        r":: \p{L  ;",
        r":: [^[$] ;",
        r":: [^\[$]) ;",
        r":: ( [^\[$]  ;",
        r":: [^[a-z[]][]] [] ;",
        r":: [^[a-z\[\]]\]] ([a-z]);",
        r":: [a$-^\]] ;",
        r":: ( [] [] ) ;",
        r":: () [] ;",
        r":: [{string}];",
        r":: ([{string}]);",
    ];

    for source in sources {
        if let Ok(rules) = parse(source) {
            panic!("Parsed invalid source {source:?}: {rules:?}");
        }
    }
}

#[test]
fn test_function_calls_ok() {
    let sources = [
        r"$fn = & Any-Any/Variant ($var literal 'quoted literal' $1) ;",
        r"$fn = &[a-z] Any-Any/Variant ($var literal 'quoted literal' $1) ;",
        r"$fn = &[a-z]Any-Any/Variant ($var literal 'quoted literal' $1) ;",
        r"$fn = &[a-z]Any/Variant ($var literal 'quoted literal' $1) ;",
        r"$fn = &Any/Variant ($var literal 'quoted literal' $1) ;",
        r"$fn = &[a-z]Any ($var literal 'quoted literal' $1) ;",
        r"$fn = &Any($var literal 'quoted literal' $1) ;",
    ];

    for source in sources {
        parse(source).map_err(|e| e.explain(source)).unwrap();
    }
}

#[test]
fn test_function_calls_err() {
    let sources = [
        r"$fn = &[a-z]($var literal 'quoted literal' $1) ;",
        r"$fn = &[a-z] ($var literal 'quoted literal' $1) ;",
        r"$fn = &($var literal 'quoted literal' $1) ;",
    ];

    for source in sources {
        if let Ok(rules) = parse(source) {
            panic!("Parsed invalid source {source:?}: {rules:?}");
        }
    }
}

#[test]
fn test_transform_rules_ok() {
    let sources = [
        ":: NFD; :: NFKC;",
        ":: Latin ;",
        ":: any - Latin;",
        ":: any - Latin/bgn;",
        ":: any - Latin/bgn ();",
        ":: any - Latin/bgn ([a-z] a-z);",
        ":: ([a-z] a-z);",
        ":: (a-z);",
        ":: (a-z / variant);",
        ":: [a-z] latin/variant (a-z / variant);",
        ":: [a-z] latin/variant (a-z / variant) ;",
        ":: [a-z] latin (  );",
        ":: [a-z] latin ;",
        "::[];",
    ];

    for source in sources {
        parse(source).map_err(|e| e.explain(source)).unwrap();
    }
}

#[test]
fn test_transform_rules_err() {
    let sources = [
        r":: a a ;",
        r":: (a a) ;",
        r":: a - z - b ;",
        r":: ( a - z - b) ;",
        r":: [] ( a - z) ;",
        r":: a-z ( [] ) ;",
        r":: a-z / ( [] a-z ) ;",
        r":: Latin-ASCII/BGN Arab-Greek/UNGEGN ;",
        r":: (Latin-ASCII/BGN Arab-Greek/UNGEGN) ;",
        r":: [a-z{string}] Remove ;",
    ];

    for source in sources {
        if let Ok(rules) = parse(source) {
            panic!("Parsed invalid source {source:?}: {rules:?}");
        }
    }
}
