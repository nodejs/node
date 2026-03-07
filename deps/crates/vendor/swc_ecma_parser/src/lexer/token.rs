use std::fmt::Display;

use swc_atoms::{Atom, Wtf8Atom};
use swc_common::{BytePos, Span};
use swc_ecma_ast::AssignOp;

use super::LexResult;
use crate::{
    error::Error,
    input::{Buffer, Tokens},
    Context, Lexer,
};

#[derive(Debug, Clone)]
pub enum TokenValue {
    /// unknown ident, jsx name and shebang
    Word(Atom),
    Template(LexResult<Wtf8Atom>),
    // string
    Str(Wtf8Atom),
    // jsx text
    JsxText(Atom),
    // regexp
    Regex(BytePos),
    Num(f64),
    BigInt(Box<num_bigint::BigInt>),
    Error(crate::error::Error),
}

#[derive(Clone, Copy, PartialEq, Eq)]
#[repr(u8)]
pub enum Token {
    // Single character tokens
    /// `(`
    LParen,
    /// `)`
    RParen,
    /// `{`
    LBrace,
    /// `}`
    RBrace,
    /// `[`
    LBracket,
    /// `]`
    RBracket,
    /// `;`
    Semi,
    /// `,`
    Comma,
    /// `.`
    Dot,
    /// `:`
    Colon,
    /// `?`
    QuestionMark,
    /// `!`
    Bang,
    /// `~`
    Tilde,
    /// `+`
    Plus,
    /// `-`
    Minus,
    /// `*`
    Asterisk,
    /// `/`
    Slash,
    /// `%`
    Percent,
    /// `<`
    Lt,
    /// `>`
    Gt,
    /// `|`
    Pipe,
    /// `^`
    Caret,
    /// `&`
    Ampersand,
    /// `=`
    Eq,
    /// `@`
    At,
    /// `#`
    Hash,
    /// '`'
    BackQuote,
    /// `=>`
    Arrow,
    /// `...`
    DotDotDot,

    // Compound operators
    /// `++`
    PlusPlus,
    /// `--`
    MinusMinus,
    /// `+=`
    PlusEq,
    /// `-=`
    MinusEq,

    // More compound operators and keywords
    /// `*=`
    MulEq,
    /// `/=`
    DivEq,
    /// `%=`
    ModEq,
    /// `<<=`
    LShiftEq,
    /// `>>=`
    RShiftEq,
    /// `>>>=`
    ZeroFillRShiftEq,
    /// `|=`
    BitOrEq,
    /// `^=`
    BitXorEq,
    /// `&=`
    BitAndEq,
    /// `**=`
    ExpEq,
    /// `||=`
    LogicalOrEq,
    /// `&&=`
    LogicalAndEq,
    /// `??=`
    NullishEq,
    /// `?.`
    OptionalChain,

    /// `==`
    EqEq,
    /// `!=`
    NotEq,
    /// `===`
    EqEqEq,
    /// `!==`
    NotEqEq,

    /// `<=`
    LtEq,
    /// `>=`
    GtEq,
    /// `<<`
    LShift,
    /// `>>`
    RShift,
    /// `>>>`
    ZeroFillRShift,

    /// `**`
    Exp,
    /// `||`
    LogicalOr,
    /// `&&`
    LogicalAnd,
    /// `??`
    NullishCoalescing,

    /// `</`
    LessSlash,
    /// `${`
    DollarLBrace,

    // JSX-related tokens
    JSXTagStart,
    JSXTagEnd,

    // Literals
    Str,
    Num,
    BigInt,
    Regex,
    Template,
    NoSubstitutionTemplateLiteral,
    TemplateHead,
    TemplateMiddle,
    TemplateTail,
    JSXName,
    JSXText,
    // Identifiers and keyword
    Ident,
    // Reserved keyword tokens
    Await,
    Break,
    Case,
    Catch,
    Class,
    Const,
    Continue,
    Debugger,
    Default,
    Delete,
    Do,
    Else,
    Export,
    Extends,
    False,
    Finally,
    For,
    Function,
    If,
    Import,
    In,
    InstanceOf,
    Let,
    New,
    Null,
    Return,
    Super,
    Switch,
    This,
    Throw,
    True,
    Try,
    TypeOf,
    Var,
    Void,
    While,
    With,
    Yield,
    Module,

    // TypeScript-related keywords
    Abstract,
    Any,
    As,
    Asserts,
    Assert,
    Async,
    Bigint,
    Boolean,
    Constructor,
    Declare,
    Enum,
    From,
    Get,
    Global,
    Implements,
    Interface,
    Intrinsic,
    Is,
    Keyof,
    Namespace,
    Never,
    Number,
    Object,
    Of,
    Out,
    Override,
    Package,
    Private,
    Protected,
    Public,
    Readonly,
    Require,
    Set,
    Static,
    String,
    Symbol,
    Type,
    Undefined,
    Unique,
    Unknown,
    Using,
    Accessor,
    Infer,
    Satisfies,
    Meta,
    Target,

    // Special tokens
    Shebang,
    Error,
    Eof,
}

impl Token {
    #[inline(always)]
    pub const fn is_ident_ref(self, ctx: Context) -> bool {
        self.is_word() && !self.is_reserved(ctx)
    }

    pub const fn is_other_and_before_expr_is_false(self) -> bool {
        !self.is_keyword()
            && !self.is_bin_op()
            && !self.before_expr()
            && !matches!(
                self,
                Token::Template
                    | Token::Dot
                    | Token::Colon
                    | Token::LBrace
                    | Token::RParen
                    | Token::Semi
                    | Token::JSXName
                    | Token::JSXText
                    | Token::JSXTagStart
                    | Token::JSXTagEnd
                    | Token::Arrow
            )
    }

    #[inline(always)]
    pub const fn is_other_and_can_have_trailing_comment(self) -> bool {
        matches!(
            self,
            Token::Num
                | Token::Str
                | Token::Ident
                | Token::DollarLBrace
                | Token::Regex
                | Token::BigInt
                | Token::JSXText
                | Token::RBrace
        ) || self.is_known_ident()
    }
}

impl<'a> Token {
    #[inline(always)]
    pub fn jsx_name(name: &str, lexer: &mut crate::Lexer) -> Self {
        let name = lexer.atoms.atom(name);
        lexer.set_token_value(Some(TokenValue::Word(name)));
        Token::JSXName
    }

    #[inline(always)]
    pub fn take_jsx_name<I: Tokens>(self, buffer: &mut Buffer<I>) -> Atom {
        buffer.expect_word_token_value()
    }

    #[inline(always)]
    pub fn str(value: Wtf8Atom, lexer: &mut crate::Lexer<'a>) -> Self {
        lexer.set_token_value(Some(TokenValue::Str(value)));
        Token::Str
    }

    #[inline(always)]
    pub fn template(cooked: LexResult<Wtf8Atom>, lexer: &mut crate::Lexer<'a>) -> Self {
        lexer.set_token_value(Some(TokenValue::Template(cooked)));
        Token::Template
    }

    #[inline(always)]
    pub fn regexp(exp_end: BytePos, lexer: &mut crate::Lexer<'a>) -> Self {
        lexer.set_token_value(Some(TokenValue::Regex(exp_end)));
        Token::Regex
    }

    #[inline(always)]
    pub fn num(value: f64, lexer: &mut crate::Lexer<'a>) -> Self {
        lexer.set_token_value(Some(TokenValue::Num(value)));
        Self::Num
    }

    #[inline(always)]
    pub fn bigint(value: Box<num_bigint::BigInt>, lexer: &mut crate::Lexer<'a>) -> Self {
        lexer.set_token_value(Some(TokenValue::BigInt(value)));
        Self::BigInt
    }

    #[inline(always)]
    pub fn unknown_ident(value: Atom, lexer: &mut crate::Lexer<'a>) -> Self {
        lexer.set_token_value(Some(TokenValue::Word(value)));
        Token::Ident
    }

    #[inline(always)]
    pub fn take_error<I: Tokens>(self, buffer: &mut Buffer<I>) -> Error {
        buffer.expect_error_token_value()
    }

    #[inline]
    pub fn take_word<I: Tokens>(self, buffer: &Buffer<I>) -> Atom {
        if self == Token::Ident {
            let value = buffer.get_token_value();
            let Some(TokenValue::Word(word)) = value else {
                unreachable!("{:#?}", value)
            };
            return word.clone();
        }

        let span = buffer.cur.span;
        let atom = Atom::new(buffer.iter.read_string(span));
        atom
    }

    #[inline(always)]
    pub fn take_known_ident<I: Tokens>(self, buffer: &Buffer<I>) -> Atom {
        let span = buffer.cur.span;
        let atom = Atom::new(buffer.iter.read_string(span));
        atom
    }

    #[inline(always)]
    pub fn take_unknown_ident_ref<'b, I: Tokens>(&'b self, buffer: &'b Buffer<I>) -> &'b Atom {
        buffer.expect_word_token_value_ref()
    }

    #[inline(always)]
    pub fn shebang(value: Atom, lexer: &mut Lexer) -> Self {
        lexer.set_token_value(Some(TokenValue::Word(value)));
        Token::Shebang
    }

    #[inline(always)]
    pub fn take_shebang<I: Tokens>(self, buffer: &mut Buffer<I>) -> Atom {
        buffer.expect_word_token_value()
    }
}

impl std::fmt::Debug for Token {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        let s = match self {
            Token::Str => "<string literal>",
            Token::Num => "<number literal>",
            Token::BigInt => "<bigint literal>",
            Token::Regex => "<regexp literal>",
            Token::Template | Token::NoSubstitutionTemplateLiteral => "<template literal>",
            Token::TemplateHead => "<template head `...${ >",
            Token::TemplateMiddle => "<template middle ...${ >",
            Token::TemplateTail => "<template tail ` >",
            Token::JSXName => "<jsx name>",
            Token::JSXText => "<jsx text>",
            Token::Ident => "<identifier>",
            Token::Error => "<error>",
            _ => &self.to_string(),
        };
        f.write_str(s)
    }
}

impl Token {
    pub const fn is_reserved(self, ctx: Context) -> bool {
        match self {
            Token::Let | Token::Static => ctx.contains(Context::Strict),
            Token::Await => {
                ctx.contains(Context::InAsync)
                    || ctx.contains(Context::InStaticBlock)
                    || ctx.contains(Context::Strict)
            }
            Token::Yield => ctx.contains(Context::InGenerator) || ctx.contains(Context::Strict),

            Token::Null
            | Token::True
            | Token::False
            | Token::Break
            | Token::Case
            | Token::Catch
            | Token::Continue
            | Token::Debugger
            | Token::Default
            | Token::Do
            | Token::Export
            | Token::Else
            | Token::Finally
            | Token::For
            | Token::Function
            | Token::If
            | Token::Return
            | Token::Switch
            | Token::Throw
            | Token::Try
            | Token::Var
            | Token::Const
            | Token::While
            | Token::With
            | Token::New
            | Token::This
            | Token::Super
            | Token::Class
            | Token::Extends
            | Token::Import
            | Token::In
            | Token::InstanceOf
            | Token::TypeOf
            | Token::Void
            | Token::Delete => true,

            // Future reserved word
            Token::Enum => true,

            Token::Implements
            | Token::Package
            | Token::Protected
            | Token::Interface
            | Token::Private
            | Token::Public
                if ctx.contains(Context::Strict) =>
            {
                true
            }

            _ => false,
        }
    }

    #[inline(always)]
    pub const fn is_keyword(self) -> bool {
        let t = self as u8;
        t >= Token::Await as u8 && t <= Token::Module as u8
    }

    #[inline(always)]
    pub const fn is_known_ident(self) -> bool {
        let t = self as u8;
        t >= Token::Abstract as u8 && t <= Token::Target as u8
    }

    pub const fn is_word(self) -> bool {
        matches!(
            self,
            Token::Null | Token::True | Token::False | Token::Ident
        ) || self.is_known_ident()
            || self.is_keyword()
    }

    #[inline(always)]
    pub const fn is_bin_op(self) -> bool {
        let t = self as u8;
        (t >= Token::EqEq as u8 && t <= Token::NullishCoalescing as u8)
            || (t >= Token::Plus as u8 && t <= Token::Ampersand as u8)
    }

    pub const fn as_bin_op(self) -> Option<swc_ecma_ast::BinaryOp> {
        match self {
            Token::EqEq => Some(swc_ecma_ast::BinaryOp::EqEq),
            Token::NotEq => Some(swc_ecma_ast::BinaryOp::NotEq),
            Token::EqEqEq => Some(swc_ecma_ast::BinaryOp::EqEqEq),
            Token::NotEqEq => Some(swc_ecma_ast::BinaryOp::NotEqEq),
            Token::Lt => Some(swc_ecma_ast::BinaryOp::Lt),
            Token::LtEq => Some(swc_ecma_ast::BinaryOp::LtEq),
            Token::Gt => Some(swc_ecma_ast::BinaryOp::Gt),
            Token::GtEq => Some(swc_ecma_ast::BinaryOp::GtEq),
            Token::LShift => Some(swc_ecma_ast::BinaryOp::LShift),
            Token::RShift => Some(swc_ecma_ast::BinaryOp::RShift),
            Token::ZeroFillRShift => Some(swc_ecma_ast::BinaryOp::ZeroFillRShift),
            Token::Plus => Some(swc_ecma_ast::BinaryOp::Add),
            Token::Minus => Some(swc_ecma_ast::BinaryOp::Sub),
            Token::Asterisk => Some(swc_ecma_ast::BinaryOp::Mul),
            Token::Slash => Some(swc_ecma_ast::BinaryOp::Div),
            Token::Percent => Some(swc_ecma_ast::BinaryOp::Mod),
            Token::Pipe => Some(swc_ecma_ast::BinaryOp::BitOr),
            Token::Caret => Some(swc_ecma_ast::BinaryOp::BitXor),
            Token::Ampersand => Some(swc_ecma_ast::BinaryOp::BitAnd),
            Token::LogicalOr => Some(swc_ecma_ast::BinaryOp::LogicalOr),
            Token::LogicalAnd => Some(swc_ecma_ast::BinaryOp::LogicalAnd),
            // Token::In => Some(swc_ecma_ast::BinaryOp::In),
            // Token::InstanceOf => Some(swc_ecma_ast::BinaryOp::InstanceOf),
            Token::Exp => Some(swc_ecma_ast::BinaryOp::Exp),
            Token::NullishCoalescing => Some(swc_ecma_ast::BinaryOp::NullishCoalescing),
            _ => None,
        }
    }

    #[inline(always)]
    pub const fn is_assign_op(self) -> bool {
        let t = self as u8;
        matches!(self, Token::Eq) || (t >= Token::PlusEq as u8 && t <= Token::NullishEq as u8)
    }

    pub fn as_assign_op(self) -> Option<swc_ecma_ast::AssignOp> {
        match self {
            Self::Eq => Some(AssignOp::Assign),
            Self::PlusEq => Some(AssignOp::AddAssign),
            Self::MinusEq => Some(AssignOp::SubAssign),
            Self::MulEq => Some(AssignOp::MulAssign),
            Self::DivEq => Some(AssignOp::DivAssign),
            Self::ModEq => Some(AssignOp::ModAssign),
            Self::LShiftEq => Some(AssignOp::LShiftAssign),
            Self::RShiftEq => Some(AssignOp::RShiftAssign),
            Self::ZeroFillRShiftEq => Some(AssignOp::ZeroFillRShiftAssign),
            Self::BitOrEq => Some(AssignOp::BitOrAssign),
            Self::BitXorEq => Some(AssignOp::BitXorAssign),
            Self::BitAndEq => Some(AssignOp::BitAndAssign),
            Self::ExpEq => Some(AssignOp::ExpAssign),
            Self::LogicalAndEq => Some(AssignOp::AndAssign),
            Self::LogicalOrEq => Some(AssignOp::OrAssign),
            Self::NullishEq => Some(AssignOp::NullishAssign),
            _ => None,
        }
    }

    #[inline(always)]
    pub const fn before_expr(self) -> bool {
        match self {
            Self::Await
            | Self::Case
            | Self::Default
            | Self::Do
            | Self::Else
            | Self::Return
            | Self::Throw
            | Self::New
            | Self::Extends
            | Self::Yield
            | Self::In
            | Self::InstanceOf
            | Self::TypeOf
            | Self::Void
            | Self::Delete
            | Self::Arrow
            | Self::DotDotDot
            | Self::Bang
            | Self::LParen
            | Self::LBrace
            | Self::LBracket
            | Self::Semi
            | Self::Comma
            | Self::Colon
            | Self::DollarLBrace
            | Self::QuestionMark
            | Self::PlusPlus
            | Self::MinusMinus
            | Self::Tilde
            | Self::JSXText => true,
            _ => self.is_bin_op() || self.is_assign_op(),
        }
    }

    #[inline(always)]
    pub const fn starts_expr(self) -> bool {
        matches!(
            self,
            Self::Ident
                | Self::JSXName
                | Self::Plus
                | Self::Minus
                | Self::Bang
                | Self::LParen
                | Self::LBrace
                | Self::LBracket
                | Self::TemplateHead
                | Self::NoSubstitutionTemplateLiteral
                | Self::DollarLBrace
                | Self::PlusPlus
                | Self::MinusMinus
                | Self::Tilde
                | Self::Str
                | Self::Regex
                | Self::Num
                | Self::BigInt
                | Self::JSXTagStart
                | Self::Await
                | Self::Function
                | Self::Throw
                | Self::New
                | Self::This
                | Self::Super
                | Self::Class
                | Self::Import
                | Self::Yield
                | Self::TypeOf
                | Self::Void
                | Self::Delete
                | Self::Null
                | Self::True
                | Self::False
                | Self::BackQuote
        ) || self.is_known_ident()
    }

    #[inline(always)]
    pub const fn follows_keyword_let(self) -> bool {
        match self {
            Token::Let
            | Token::LBrace
            | Token::LBracket
            | Token::Ident
            | Token::Yield
            | Token::Await => true,
            _ if self.is_known_ident() => true,
            _ => false,
        }
    }

    #[inline(always)]
    pub const fn should_rescan_into_gt_in_jsx(self) -> bool {
        matches!(
            self,
            Token::GtEq
                | Token::RShift
                | Token::RShiftEq
                | Token::ZeroFillRShift
                | Token::ZeroFillRShiftEq
        )
    }
}

impl Display for Token {
    #[cold]
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        let s = match self {
            Token::LParen => "(",
            Token::RParen => ")",
            Token::LBrace => "{",
            Token::RBrace => "}",
            Token::LBracket => "[",
            Token::RBracket => "]",
            Token::Semi => ";",
            Token::Comma => ",",
            Token::Dot => ".",
            Token::Colon => ":",
            Token::QuestionMark => "?",
            Token::Bang => "!",
            Token::Tilde => "~",
            Token::Plus => "+",
            Token::Minus => "-",
            Token::Asterisk => "*",
            Token::Slash => "/",
            Token::Percent => "%",
            Token::Lt => "<",
            Token::Gt => ">",
            Token::Pipe => "|",
            Token::Caret => "^",
            Token::Ampersand => "&",
            Token::Eq => "=",
            Token::At => "@",
            Token::Hash => "#",
            Token::BackQuote => "`",
            Token::Arrow => "=>",
            Token::DotDotDot => "...",
            Token::PlusPlus => "++",
            Token::MinusMinus => "--",
            Token::PlusEq => "+=",
            Token::MinusEq => "-=",
            Token::MulEq => "*",
            Token::DivEq => "/=",
            Token::ModEq => "%=",
            Token::LShiftEq => "<<=",
            Token::RShiftEq => ">>=",
            Token::ZeroFillRShiftEq => ">>>=",
            Token::BitOrEq => "|=",
            Token::BitXorEq => "^=",
            Token::BitAndEq => "&=",
            Token::ExpEq => "**=",
            Token::LogicalOrEq => "||=",
            Token::LogicalAndEq => "&&=",
            Token::NullishEq => "??=",
            Token::OptionalChain => "?.",
            Token::EqEq => "==",
            Token::NotEq => "!=",
            Token::EqEqEq => "===",
            Token::NotEqEq => "!==",
            Token::LtEq => "<=",
            Token::GtEq => ">=",
            Token::LShift => "<<",
            Token::RShift => ">>",
            Token::ZeroFillRShift => ">>>",
            Token::Exp => "**",
            Token::LogicalOr => "||",
            Token::LogicalAnd => "&&",
            Token::NullishCoalescing => "??",
            Token::DollarLBrace => "${",
            Token::JSXTagStart => "jsx tag start",
            Token::JSXTagEnd => "jsx tag end",
            Token::JSXText => "jsx text",
            Token::Str => "string literal",
            Token::Num => "numeric literal",
            Token::BigInt => "bigint literal",
            Token::Regex => "regexp literal",
            Token::Template => "template token",
            Token::JSXName => "jsx name",
            Token::Error => "<lexing error>",
            Token::Ident => "ident",
            Token::NoSubstitutionTemplateLiteral => "no substitution template literal",
            Token::TemplateHead => "template head",
            Token::TemplateMiddle => "template middle",
            Token::TemplateTail => "template tail",
            Token::Await => "await",
            Token::Break => "break",
            Token::Case => "case",
            Token::Catch => "catch",
            Token::Class => "class",
            Token::Const => "const",
            Token::Continue => "continue",
            Token::Debugger => "debugger",
            Token::Default => "default",
            Token::Delete => "delete",
            Token::Do => "do",
            Token::Else => "else",
            Token::Export => "export",
            Token::Extends => "extends",
            Token::False => "false",
            Token::Finally => "finally",
            Token::For => "for",
            Token::Function => "function",
            Token::If => "if",
            Token::Import => "import",
            Token::In => "in",
            Token::InstanceOf => "instanceOf",
            Token::Let => "let",
            Token::New => "new",
            Token::Null => "null",
            Token::Return => "return",
            Token::Super => "super",
            Token::Switch => "switch",
            Token::This => "this",
            Token::Throw => "throw",
            Token::True => "true",
            Token::Try => "try",
            Token::TypeOf => "typeOf",
            Token::Var => "var",
            Token::Void => "void",
            Token::While => "while",
            Token::With => "with",
            Token::Yield => "yield",
            Token::Module => "module",
            Token::Abstract => "abstract",
            Token::Any => "any",
            Token::As => "as",
            Token::Asserts => "asserts",
            Token::Assert => "assert",
            Token::Async => "async",
            Token::Bigint => "bigint",
            Token::Boolean => "boolean",
            Token::Constructor => "constructor",
            Token::Declare => "declare",
            Token::Enum => "enum",
            Token::From => "from",
            Token::Get => "get",
            Token::Global => "global",
            Token::Implements => "implements",
            Token::Interface => "interface",
            Token::Intrinsic => "intrinsic",
            Token::Is => "is",
            Token::Keyof => "keyof",
            Token::Namespace => "namespace",
            Token::Never => "never",
            Token::Number => "number",
            Token::Object => "object",
            Token::Of => "of",
            Token::Out => "out",
            Token::Override => "override",
            Token::Package => "package",
            Token::Private => "private",
            Token::Protected => "protected",
            Token::Public => "public",
            Token::Readonly => "readonly",
            Token::Require => "require",
            Token::Set => "set",
            Token::Static => "static",
            Token::String => "string",
            Token::Symbol => "symbol",
            Token::Type => "type",
            Token::Undefined => "undefined",
            Token::Unique => "unique",
            Token::Unknown => "unknown",
            Token::Using => "using",
            Token::Accessor => "accessor",
            Token::Infer => "infer",
            Token::Satisfies => "satisfies",
            Token::Meta => "meta",
            Token::Target => "target",
            Token::Shebang => "#!",
            Token::LessSlash => "</",
            Token::Eof => "<eof>",
        };
        f.write_str(s)
    }
}

#[derive(Clone, Copy, Debug)]
pub struct TokenAndSpan {
    pub token: Token,
    /// Had a line break before this token?
    pub had_line_break: bool,
    pub span: Span,
}

impl TokenAndSpan {
    #[inline(always)]
    pub fn new(token: Token, span: Span, had_line_break: bool) -> Self {
        Self {
            token,
            had_line_break,
            span,
        }
    }
}

#[derive(Clone)]
pub struct NextTokenAndSpan {
    pub token_and_span: TokenAndSpan,
    pub value: Option<TokenValue>,
}

impl NextTokenAndSpan {
    #[inline(always)]
    pub fn token(&self) -> Token {
        self.token_and_span.token
    }

    #[inline(always)]
    pub fn span(&self) -> Span {
        self.token_and_span.span
    }

    #[inline(always)]
    pub fn had_line_break(&self) -> bool {
        self.token_and_span.had_line_break
    }
}
