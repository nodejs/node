#![allow(non_upper_case_globals)]
use bitflags::bitflags;

bitflags! {
    /// Represents the formatting rule for a list of nodes.
    #[derive(PartialEq, Eq, Copy, Clone)]
    pub struct ListFormat: u32 {
        /// Default value.
        const None = 0;

        // Line separators
        /// Prints the list on a single line (default).
        const SingleLine = 0;
        /// Prints the list on multiple lines.
        const MultiLine = 1 << 0;
        /// Prints the list using line preservation if possible.
        const PreserveLines = 1 << 1;
        const LinesMask = Self::MultiLine.bits() | Self::PreserveLines.bits();

        // Delimiters
        /// There is no delimiter between list items (default).
        const NotDelimited = 0;
        /// Each list item is space-and-bar (" |") delimited.
        const BarDelimited = 1 << 2;
        /// Each list item is space-and-ampersand (" &") delimited.
        const AmpersandDelimited = 1 << 3;
        /// Each list item is comma (";") delimited.
        const CommaDelimited = 1 << 4;
        const DelimitersMask = Self::BarDelimited.bits()
            | Self::AmpersandDelimited.bits()
            | Self::CommaDelimited.bits();

        // Write a trailing comma (";") if present.
        const AllowTrailingComma = 1 << 5;

        // Whitespace
        /// The list should be indented.
        const Indented = 1 << 6;
        /// Inserts a space after the opening brace and before the closing
        /// brace.
        const SpaceBetweenBraces = 1 << 7;
        /// Inserts a space between each sibling node.
        const SpaceBetweenSiblings = 1 << 8;

        // Brackets/Braces
        /// The list is surrounded by "{" and "}".
        const Braces = 1 << 9;
        /// The list is surrounded by "(" and ")".
        const Parenthesis = 1 << 10;
        /// The list is surrounded by "<" and ">".
        const AngleBrackets = 1 << 11;
        /// The list is surrounded by "[" and "]".
        const SquareBrackets = 1 << 12;
        const BracketsMask = Self::Braces.bits()
            | Self::Parenthesis.bits()
            | Self::AngleBrackets.bits()
            | Self::SquareBrackets.bits();

        /// Do not emit brackets if the list is undefined.
        const OptionalIfUndefined = 1 << 13;
        /// Do not emit brackets if the list is empty.
        const OptionalIfEmpty = 1 << 14;
        const Optional = Self::OptionalIfUndefined.bits() | Self::OptionalIfEmpty.bits();

        // Others
        /// Prefer adding a LineTerminator between synthesized nodes.
        const PreferNewLine = 1 << 15;
        /// Do not emit a trailing NewLine for a MultiLine list.
        const NoTrailingNewLine = 1 << 16;
        /// Do not emit comments between each node
        const NoInterveningComments = 1 << 17;
        /// If the literal is empty; do not add spaces between braces.
        const NoSpaceIfEmpty = 1 << 18;
        const SingleElement = 1 << 19;
        const ForceTrailingComma = 1 << 20;

        // Optimization.
        const CanSkipTrailingComma = 1 << 21;

        // Precomputed Formats
        const Modifiers = Self::SingleLine.bits()
            | Self::SpaceBetweenSiblings.bits()
            | Self::NoInterveningComments.bits();
        const HeritageClauses = Self::SingleLine.bits() | Self::SpaceBetweenSiblings.bits();
        const SingleLineTypeLiteralMembers = Self::SingleLine.bits()
            | Self::SpaceBetweenBraces.bits()
            | Self::SpaceBetweenSiblings.bits()
            | Self::Indented.bits();
        const MultiLineTypeLiteralMembers = Self::MultiLine.bits() | Self::Indented.bits();
        const TupleTypeElements = Self::CommaDelimited.bits()
            | Self::SpaceBetweenSiblings.bits()
            | Self::SingleLine.bits()
            | Self::Indented.bits();
        const UnionTypeConstituents = Self::BarDelimited.bits()
            | Self::SpaceBetweenSiblings.bits()
            | Self::SingleLine.bits();
        const IntersectionTypeConstituents = Self::AmpersandDelimited.bits()
            | Self::SpaceBetweenSiblings.bits()
            | Self::SingleLine.bits();
        const ObjectBindingPatternElements = Self::SingleLine.bits()
            | Self::SpaceBetweenBraces.bits()
            | Self::CommaDelimited.bits()
            | Self::SpaceBetweenSiblings.bits()
            | Self::NoSpaceIfEmpty.bits();
        const ArrayBindingPatternElements = Self::SingleLine.bits()
            | Self::CommaDelimited.bits()
            | Self::SpaceBetweenSiblings.bits()
            | Self::NoSpaceIfEmpty.bits();
        const ObjectLiteralExpressionProperties = Self::MultiLine.bits()
            | Self::CommaDelimited.bits()
            | Self::SpaceBetweenSiblings.bits()
            | Self::SpaceBetweenBraces.bits()
            | Self::Indented.bits()
            | Self::Braces.bits()
            | Self::NoSpaceIfEmpty.bits();
        const ArrayLiteralExpressionElements = Self::PreserveLines.bits()
            | Self::CommaDelimited.bits()
            | Self::SpaceBetweenSiblings.bits()
            | Self::Indented.bits()
            | Self::SquareBrackets.bits();
        const CommaListElements = Self::CommaDelimited.bits()
            | Self::SpaceBetweenSiblings.bits()
            | Self::SingleLine.bits();
        const CallExpressionArguments = Self::CommaDelimited.bits()
            | Self::SpaceBetweenSiblings.bits()
            | Self::SingleLine.bits()
            | Self::Parenthesis.bits();
        const NewExpressionArguments = Self::CommaDelimited.bits()
            | Self::SpaceBetweenSiblings.bits()
            | Self::SingleLine.bits()
            | Self::Parenthesis.bits()
            | Self::OptionalIfUndefined.bits();
        const TemplateExpressionSpans = Self::SingleLine.bits() | Self::NoInterveningComments.bits();
        const SingleLineBlockStatements = Self::SpaceBetweenBraces.bits()
            | Self::SpaceBetweenSiblings.bits()
            | Self::SingleLine.bits();
        const MultiLineBlockStatements = Self::Indented.bits() | Self::MultiLine.bits();
        const VariableDeclarationList = Self::CommaDelimited.bits()
            | Self::SpaceBetweenSiblings.bits()
            | Self::SingleLine.bits();
        const SingleLineFunctionBodyStatements = Self::SingleLine.bits()
            | Self::SpaceBetweenSiblings.bits()
            | Self::SpaceBetweenBraces.bits();
        const MultiLineFunctionBodyStatements = Self::MultiLine.bits();
        const ClassHeritageClauses = Self::CommaDelimited.bits() | Self::SingleLine.bits() | Self::SpaceBetweenSiblings.bits();
        const ClassMembers = Self::Indented.bits() | Self::MultiLine.bits();
        const InterfaceMembers = Self::Indented.bits() | Self::MultiLine.bits();
        const EnumMembers = Self::CommaDelimited.bits() | Self::Indented.bits() | Self::MultiLine.bits();
        const CaseBlockClauses = Self::Indented.bits() | Self::MultiLine.bits();
        const NamedImportsOrExportsElements = Self::CommaDelimited.bits()
            | Self::SpaceBetweenSiblings.bits()
            | Self::AllowTrailingComma.bits()
            | Self::SingleLine.bits()
            | Self::SpaceBetweenBraces.bits();
        const JsxElementOrFragmentChildren = Self::SingleLine.bits() | Self::NoInterveningComments.bits();
        const JsxElementAttributes = Self::SingleLine.bits()
            | Self::SpaceBetweenSiblings.bits()
            | Self::NoInterveningComments.bits();
        const CaseOrDefaultClauseStatements = Self::Indented.bits()
            | Self::MultiLine.bits()
            | Self::NoTrailingNewLine.bits()
            | Self::OptionalIfEmpty.bits();
        const HeritageClauseTypes = Self::CommaDelimited.bits()
            | Self::SpaceBetweenSiblings.bits()
            | Self::SingleLine.bits();
        const SourceFileStatements = Self::MultiLine.bits() | Self::NoTrailingNewLine.bits();
        const Decorators = Self::MultiLine.bits() | Self::Optional.bits();
        const TypeArguments = Self::CommaDelimited.bits()
            | Self::SpaceBetweenSiblings.bits()
            | Self::SingleLine.bits()
            | Self::AngleBrackets.bits()
            | Self::Optional.bits();
        const TypeParameters = Self::CommaDelimited.bits()
            | Self::SpaceBetweenSiblings.bits()
            | Self::SingleLine.bits()
            | Self::AngleBrackets.bits()
            | Self::Optional.bits();
        const Parameters = Self::CommaDelimited.bits()
            | Self::SpaceBetweenSiblings.bits()
            | Self::SingleLine.bits()
            | Self::Parenthesis.bits();
        const IndexSignatureParameters = Self::CommaDelimited.bits()
            | Self::SpaceBetweenSiblings.bits()
            | Self::SingleLine.bits()
            | Self::Indented.bits()
            | Self::SquareBrackets.bits();
    }
}

impl ListFormat {
    pub fn opening_bracket(self) -> &'static str {
        match self & ListFormat::BracketsMask {
            ListFormat::Braces => "{",
            ListFormat::Parenthesis => "(",
            ListFormat::AngleBrackets => "<",
            ListFormat::SquareBrackets => "[",
            _ => unreachable!(),
        }
    }

    pub fn closing_bracket(self) -> &'static str {
        match self & ListFormat::BracketsMask {
            ListFormat::Braces => "}",
            ListFormat::Parenthesis => ")",
            ListFormat::AngleBrackets => ">",
            ListFormat::SquareBrackets => "]",
            _ => unreachable!(),
        }
    }

    /// Returns `true` if the opening bracket requires committing a pending
    /// semicolon before it (to avoid ASI issues). This is true for `{`,
    /// `(`, and `[`, but not for `<`.
    #[inline]
    pub fn opening_bracket_requires_semi_commit(self) -> bool {
        // `<` is the only opening bracket that doesn't require committing a
        // pending semicolon
        !matches!(self & ListFormat::BracketsMask, ListFormat::AngleBrackets)
    }
}
