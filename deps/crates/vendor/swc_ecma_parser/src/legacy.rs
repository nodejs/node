/// This module should only be used by [swc_ecma_lexer] for compatibility.
pub mod token {
    use std::{
        borrow::Cow,
        fmt::{self, Debug, Display, Formatter},
    };

    use swc_atoms::{atom, Atom, AtomStore, Wtf8Atom};
    use swc_common::{Span, Spanned};
    use swc_ecma_ast::{AssignOp, BigIntValue, BinaryOp};

    use self::{Keyword::*, Token::*};
    use crate::{error::Error, lexer::LexResult};

    macro_rules! define_known_ident {
    (
        $(
            $name:ident => $value:tt,
        )*
    ) => {
        #[derive(Debug, Clone, Copy, PartialEq, Eq, PartialOrd, Ord, Hash)]
        #[non_exhaustive]
        pub enum KnownIdent {
            $(
                $name
            ),*
        }

        #[macro_export]
        macro_rules! known_ident_token {
            $(
                ($value) => {
                    $crate::token::TokenKind::Word($crate::token::WordKind::Ident(
                        $crate::token::IdentKind::Known($crate::token::KnownIdent::$name),
                    ))
                };
            )*
        }

        #[macro_export]
        macro_rules! known_ident {
            $(
                ($value) => {
                    $crate::token::KnownIdent::$name
                };
            )*
        }
        #[macro_export]
        macro_rules! ident_like {
            $(
                ($value) => {
                    $crate::token::IdentLike::Known(
                        $crate::token::KnownIdent::$name
                    )
                };
            )*
        }

        static STR_TO_KNOWN_IDENT: phf::Map<&'static str, KnownIdent> = phf::phf_map! {
            $(
                $value => KnownIdent::$name,
            )*
        };

        impl From<KnownIdent> for swc_atoms::Atom {

            fn from(s: KnownIdent) -> Self {
                match s {
                    $(
                        KnownIdent::$name => atom!($value),
                    )*
                }
            }
        }
        impl From<KnownIdent> for &'static str {

            fn from(s: KnownIdent) -> Self {
                match s {
                    $(
                        KnownIdent::$name => $value,
                    )*
                }
            }
        }
    };
}

    define_known_ident!(
        Abstract => "abstract",
        As => "as",
        Async => "async",
        From => "from",
        Of => "of",
        Type => "type",
        Global => "global",
        Static => "static",
        Using => "using",
        Readonly => "readonly",
        Unique => "unique",
        Keyof => "keyof",
        Declare => "declare",
        Enum => "enum",
        Is => "is",
        Infer => "infer",
        Symbol => "symbol",
        Undefined => "undefined",
        Interface => "interface",
        Implements => "implements",
        Asserts => "asserts",
        Require => "require",
        Get => "get",
        Set => "set",
        Any => "any",
        Intrinsic => "intrinsic",
        Unknown => "unknown",
        String => "string",
        Object => "object",
        Number => "number",
        Bigint => "bigint",
        Boolean => "boolean",
        Never => "never",
        Assert => "assert",
        Namespace => "namespace",
        Accessor => "accessor",
        Meta => "meta",
        Target => "target",
        Satisfies => "satisfies",
        Package => "package",
        Protected => "protected",
        Private => "private",
        Public => "public",
    );

    impl std::str::FromStr for KnownIdent {
        type Err = ();

        fn from_str(s: &str) -> Result<Self, Self::Err> {
            STR_TO_KNOWN_IDENT.get(s).cloned().ok_or(())
        }
    }

    #[derive(Debug, Copy, Clone, PartialEq, Eq, Hash)]
    pub enum WordKind {
        Keyword(Keyword),

        Null,
        True,
        False,

        Ident(IdentKind),
    }

    impl From<Keyword> for WordKind {
        #[inline(always)]
        fn from(kwd: Keyword) -> Self {
            Self::Keyword(kwd)
        }
    }

    #[derive(Debug, Copy, Clone, PartialEq, Eq, Hash)]
    pub enum IdentKind {
        Known(KnownIdent),
        Other,
    }

    #[derive(Debug, Copy, Clone, PartialEq, Eq, Hash)]
    pub enum TokenKind {
        Word(WordKind),
        Arrow,
        Hash,
        At,
        Dot,
        DotDotDot,
        Bang,
        LParen,
        RParen,
        LBracket,
        RBracket,
        LBrace,
        RBrace,
        Semi,
        Comma,
        BackQuote,
        Template,
        Colon,
        BinOp(BinOpToken),
        AssignOp(AssignOp),
        DollarLBrace,
        QuestionMark,
        PlusPlus,
        MinusMinus,
        Tilde,
        Str,
        /// We abuse `token.raw` for flags
        Regex,
        Num,
        BigInt,

        JSXName,
        JSXText,
        JSXTagStart,
        JSXTagEnd,

        Shebang,
        Error,
    }

    #[derive(Clone, PartialEq)]
    pub enum Token {
        /// Identifier, "null", "true", "false".
        ///
        /// Contains `null` and ``
        Word(Word),

        /// '=>'
        Arrow,

        /// '#'
        Hash,

        /// '@'
        At,
        /// '.'
        Dot,

        /// '...'
        DotDotDot,
        /// '!'
        Bang,

        /// '('
        LParen,
        /// ')'
        RParen,
        /// `[`
        LBracket,
        /// ']'
        RBracket,
        /// '{'
        LBrace,
        /// '}'
        RBrace,

        /// ';'
        Semi,
        /// ','
        Comma,

        /// '`'
        BackQuote,
        Template {
            raw: Atom,
            cooked: LexResult<Wtf8Atom>,
        },
        /// ':'
        Colon,
        BinOp(BinOpToken),
        AssignOp(AssignOp),

        /// '${'
        DollarLBrace,

        /// '?'
        QuestionMark,

        /// `++`
        PlusPlus,
        /// `--`
        MinusMinus,

        /// `~`
        Tilde,

        /// String literal. Span of this token contains quote.
        Str {
            value: Wtf8Atom,
            raw: Atom,
        },

        /// Regexp literal.
        Regex(Atom, Atom),

        /// TODO: Make Num as enum and separate decimal, binary, ..etc
        Num {
            value: f64,
            raw: Atom,
        },

        BigInt {
            value: Box<BigIntValue>,
            raw: Atom,
        },

        JSXName {
            name: Atom,
        },
        JSXText {
            value: Atom,
            raw: Atom,
        },
        JSXTagStart,
        JSXTagEnd,

        Shebang(Atom),
        Error(Error),
        Eof,
    }

    impl Token {
        pub fn kind(&self) -> TokenKind {
            match self {
                Self::Arrow => TokenKind::Arrow,
                Self::Hash => TokenKind::Hash,
                Self::At => TokenKind::At,
                Self::Dot => TokenKind::Dot,
                Self::DotDotDot => TokenKind::DotDotDot,
                Self::Bang => TokenKind::Bang,
                Self::LParen => TokenKind::LParen,
                Self::RParen => TokenKind::RParen,
                Self::LBracket => TokenKind::LBracket,
                Self::RBracket => TokenKind::RBracket,
                Self::LBrace => TokenKind::LBrace,
                Self::RBrace => TokenKind::RBrace,
                Self::Semi => TokenKind::Semi,
                Self::Comma => TokenKind::Comma,
                Self::BackQuote => TokenKind::BackQuote,
                Self::Template { .. } => TokenKind::Template,
                Self::Colon => TokenKind::Colon,
                Self::BinOp(op) => TokenKind::BinOp(*op),
                Self::AssignOp(op) => TokenKind::AssignOp(*op),
                Self::DollarLBrace => TokenKind::DollarLBrace,
                Self::QuestionMark => TokenKind::QuestionMark,
                Self::PlusPlus => TokenKind::PlusPlus,
                Self::MinusMinus => TokenKind::MinusMinus,
                Self::Tilde => TokenKind::Tilde,
                Self::Str { .. } => TokenKind::Str,
                Self::Regex(..) => TokenKind::Regex,
                Self::Num { .. } => TokenKind::Num,
                Self::BigInt { .. } => TokenKind::BigInt,
                Self::JSXName { .. } => TokenKind::JSXName,
                Self::JSXText { .. } => TokenKind::JSXText,
                Self::JSXTagStart => TokenKind::JSXTagStart,
                Self::JSXTagEnd => TokenKind::JSXTagEnd,
                Self::Shebang(..) => TokenKind::Shebang,
                Self::Error(..) => TokenKind::Error,
                Self::Eof => TokenKind::Error,
                Self::Word(w) => TokenKind::Word(w.kind()),
            }
        }
    }

    impl TokenKind {
        pub const fn before_expr(self) -> bool {
            match self {
                Self::Word(w) => w.before_expr(),
                Self::BinOp(w) => w.before_expr(),
                Self::Arrow
                | Self::DotDotDot
                | Self::Bang
                | Self::LParen
                | Self::LBrace
                | Self::LBracket
                | Self::Semi
                | Self::Comma
                | Self::Colon
                | Self::AssignOp(..)
                | Self::DollarLBrace
                | Self::QuestionMark
                | Self::PlusPlus
                | Self::MinusMinus
                | Self::Tilde
                | Self::JSXText { .. } => true,
                _ => false,
            }
        }

        pub const fn starts_expr(self) -> bool {
            match self {
                Self::Word(w) => w.starts_expr(),
                Self::BinOp(w) => w.starts_expr(),
                Self::Bang
                | Self::LParen
                | Self::LBrace
                | Self::LBracket
                | Self::BackQuote
                | Self::DollarLBrace
                | Self::PlusPlus
                | Self::MinusMinus
                | Self::Tilde
                | Self::Str
                | Self::Regex
                | Self::Num
                | Self::BigInt
                | Self::JSXTagStart => true,
                _ => false,
            }
        }
    }

    #[derive(Debug, Clone, Copy, Eq, PartialEq, Hash)]
    pub enum BinOpToken {
        /// `==`
        EqEq,
        /// `!=`
        NotEq,
        /// `===`
        EqEqEq,
        /// `!==`
        NotEqEq,
        /// `<`
        Lt,
        /// `<=`
        LtEq,
        /// `>`
        Gt,
        /// `>=`
        GtEq,
        /// `<<`
        LShift,
        /// `>>`
        RShift,
        /// `>>>`
        ZeroFillRShift,

        /// `+`
        Add,
        /// `-`
        Sub,
        /// `*`
        Mul,
        /// `/`
        Div,
        /// `%`
        Mod,

        /// `|`
        BitOr,
        /// `^`
        BitXor,
        /// `&`
        BitAnd,

        // /// `in`
        // #[kind(precedence = "7")]
        // In,
        // /// `instanceof`
        // #[kind(precedence = "7")]
        // InstanceOf,
        /// `**`
        Exp,

        /// `||`
        LogicalOr,
        /// `&&`
        LogicalAnd,

        /// `??`
        NullishCoalescing,
    }

    impl BinOpToken {
        pub(crate) const fn starts_expr(self) -> bool {
            matches!(self, Self::Add | Self::Sub)
        }

        pub const fn before_expr(self) -> bool {
            true
        }
    }

    #[derive(Debug, Clone, PartialEq)]
    pub struct TokenAndSpan {
        pub token: Token,
        /// Had a line break before this token?
        pub had_line_break: bool,
        pub span: Span,
    }

    impl Spanned for TokenAndSpan {
        #[inline]
        fn span(&self) -> Span {
            self.span
        }
    }

    #[derive(Clone, PartialEq, Eq, Hash)]
    pub enum Word {
        Keyword(Keyword),

        Null,
        True,
        False,

        Ident(IdentLike),
    }

    #[derive(Clone, PartialEq, Eq, Hash)]
    pub enum IdentLike {
        Known(KnownIdent),
        Other(Atom),
    }

    impl From<&'_ str> for IdentLike {
        fn from(s: &str) -> Self {
            s.parse::<KnownIdent>()
                .map(Self::Known)
                .unwrap_or_else(|_| Self::Other(s.into()))
        }
    }

    impl IdentLike {
        pub(crate) fn from_str(atoms: &mut AtomStore, s: &str) -> IdentLike {
            s.parse::<KnownIdent>()
                .map(Self::Known)
                .unwrap_or_else(|_| Self::Other(atoms.atom(s)))
        }
    }

    impl Word {
        pub fn from_str(atoms: &mut AtomStore, s: &str) -> Self {
            match s {
                "null" => Word::Null,
                "true" => Word::True,
                "false" => Word::False,
                "await" => Await.into(),
                "break" => Break.into(),
                "case" => Case.into(),
                "catch" => Catch.into(),
                "continue" => Continue.into(),
                "debugger" => Debugger.into(),
                "default" => Default_.into(),
                "do" => Do.into(),
                "export" => Export.into(),
                "else" => Else.into(),
                "finally" => Finally.into(),
                "for" => For.into(),
                "function" => Function.into(),
                "if" => If.into(),
                "return" => Return.into(),
                "switch" => Switch.into(),
                "throw" => Throw.into(),
                "try" => Try.into(),
                "var" => Var.into(),
                "let" => Let.into(),
                "const" => Const.into(),
                "while" => While.into(),
                "with" => With.into(),
                "new" => New.into(),
                "this" => This.into(),
                "super" => Super.into(),
                "class" => Class.into(),
                "extends" => Extends.into(),
                "import" => Import.into(),
                "yield" => Yield.into(),
                "in" => In.into(),
                "instanceof" => InstanceOf.into(),
                "typeof" => TypeOf.into(),
                "void" => Void.into(),
                "delete" => Delete.into(),
                _ => Word::Ident(IdentLike::from_str(atoms, s)),
            }
        }

        pub(crate) fn kind(&self) -> WordKind {
            match self {
                Word::Keyword(k) => WordKind::Keyword(*k),
                Word::Null => WordKind::Null,
                Word::True => WordKind::True,
                Word::False => WordKind::False,
                Word::Ident(IdentLike::Known(i)) => WordKind::Ident(IdentKind::Known(*i)),
                Word::Ident(IdentLike::Other(..)) => WordKind::Ident(IdentKind::Other),
            }
        }
    }

    impl WordKind {
        pub(crate) const fn before_expr(self) -> bool {
            match self {
                Self::Keyword(k) => k.before_expr(),
                _ => false,
            }
        }

        pub(crate) const fn starts_expr(self) -> bool {
            match self {
                Self::Keyword(k) => k.starts_expr(),
                _ => true,
            }
        }
    }

    impl AsRef<str> for IdentLike {
        fn as_ref(&self) -> &str {
            match self {
                IdentLike::Known(k) => (*k).into(),
                IdentLike::Other(s) => s.as_ref(),
            }
        }
    }

    impl From<Keyword> for Word {
        fn from(kwd: Keyword) -> Self {
            Word::Keyword(kwd)
        }
    }

    impl From<Word> for Atom {
        fn from(w: Word) -> Self {
            match w {
                Word::Keyword(k) => match k {
                    Await => "await",
                    Break => "break",
                    Case => "case",
                    Catch => "catch",
                    Continue => "continue",
                    Debugger => "debugger",
                    Default_ => "default",
                    Do => "do",
                    Else => "else",

                    Finally => "finally",
                    For => "for",

                    Function => "function",

                    If => "if",

                    Return => "return",

                    Switch => "switch",

                    Throw => "throw",

                    Try => "try",
                    Var => "var",
                    Let => "let",
                    Const => "const",
                    While => "while",
                    With => "with",

                    New => "new",
                    This => "this",
                    Super => "super",

                    Class => "class",

                    Extends => "extends",

                    Export => "export",
                    Import => "import",

                    Yield => "yield",

                    In => "in",
                    InstanceOf => "instanceof",

                    TypeOf => "typeof",

                    Void => "void",

                    Delete => "delete",
                }
                .into(),

                Word::Null => atom!("null"),
                Word::True => atom!("true"),
                Word::False => atom!("false"),

                Word::Ident(w) => w.into(),
            }
        }
    }

    impl From<IdentLike> for Atom {
        fn from(i: IdentLike) -> Self {
            match i {
                IdentLike::Known(i) => i.into(),
                IdentLike::Other(i) => i,
            }
        }
    }

    impl Debug for Word {
        fn fmt(&self, f: &mut Formatter<'_>) -> fmt::Result {
            match *self {
                Word::Ident(ref s) => Display::fmt(s, f),
                _ => {
                    let s: Atom = self.clone().into();
                    Display::fmt(&s, f)
                }
            }
        }
    }

    impl Display for IdentLike {
        fn fmt(&self, f: &mut Formatter<'_>) -> fmt::Result {
            match *self {
                IdentLike::Known(ref s) => Display::fmt(s, f),
                IdentLike::Other(ref s) => Display::fmt(s, f),
            }
        }
    }

    impl Display for KnownIdent {
        fn fmt(&self, f: &mut Formatter<'_>) -> fmt::Result {
            let s: &'static str = (*self).into();

            Display::fmt(s, f)
        }
    }

    #[macro_export]
    macro_rules! declare_keyword {
    ($(
        $name:ident => $value:tt,
    )*) => {
        impl Keyword {
            pub fn into_atom(self) -> Atom {
                match self {
                    $(Keyword::$name => atom!($value),)*
                }
            }
        }
    };
}

    declare_keyword!(
        Await => "await",
        Break => "break",
        Case => "case",
        Catch => "catch",
        Continue => "continue",
        Debugger => "debugger",
        Default_ => "default",
        Do => "do",
        Else => "else",

        Finally => "finally",
        For => "for",

        Function => "function",

        If => "if",

        Return => "return",

        Switch => "switch",

        Throw => "throw",

        Try => "try",
        Var => "var",
        Let => "let",
        Const => "const",
        While => "while",
        With => "with",

        New => "new",
        This => "this",
        Super => "super",

        Class => "class",

        Extends => "extends",

        Export => "export",
        Import => "import",

        Yield => "yield",

        In => "in",
        InstanceOf => "instanceof",

        TypeOf => "typeof",

        Void => "void",

        Delete => "delete",
    );

    /// Keywords
    #[derive(Clone, Copy, PartialEq, Eq, Hash)]
    pub enum Keyword {
        /// Spec says this might be identifier.
        Await,
        Break,
        Case,
        Catch,
        Continue,
        Debugger,
        Default_,
        Do,
        Else,

        Finally,
        For,

        Function,

        If,

        Return,

        Switch,

        Throw,

        Try,
        Var,
        Let,
        Const,
        While,
        With,

        New,
        This,
        Super,

        Class,

        Extends,

        Export,
        Import,

        /// Spec says this might be identifier.
        Yield,

        In,
        InstanceOf,
        TypeOf,
        Void,
        Delete,
    }

    impl Keyword {
        pub const fn before_expr(self) -> bool {
            matches!(
                self,
                Self::Await
                    | Self::Case
                    | Self::Default_
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
            )
        }

        pub(crate) const fn starts_expr(self) -> bool {
            matches!(
                self,
                Self::Await
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
            )
        }
    }

    impl Debug for Keyword {
        fn fmt(&self, f: &mut Formatter<'_>) -> fmt::Result {
            write!(f, "keyword '{}'", self.into_atom())?;

            Ok(())
        }
    }

    impl From<BinOpToken> for BinaryOp {
        fn from(t: BinOpToken) -> Self {
            use self::BinaryOp::*;
            match t {
                BinOpToken::EqEq => EqEq,
                BinOpToken::NotEq => NotEq,
                BinOpToken::EqEqEq => EqEqEq,
                BinOpToken::NotEqEq => NotEqEq,
                BinOpToken::Lt => Lt,
                BinOpToken::LtEq => LtEq,
                BinOpToken::Gt => Gt,
                BinOpToken::GtEq => GtEq,
                BinOpToken::LShift => LShift,
                BinOpToken::RShift => RShift,
                BinOpToken::ZeroFillRShift => ZeroFillRShift,
                BinOpToken::Add => Add,
                BinOpToken::Sub => Sub,
                BinOpToken::Mul => Mul,
                BinOpToken::Div => Div,
                BinOpToken::Mod => Mod,
                BinOpToken::BitOr => BitOr,
                BinOpToken::BitXor => BitXor,
                BinOpToken::BitAnd => BitAnd,
                BinOpToken::LogicalOr => LogicalOr,
                BinOpToken::LogicalAnd => LogicalAnd,
                BinOpToken::Exp => Exp,
                BinOpToken::NullishCoalescing => NullishCoalescing,
            }
        }
    }

    impl TokenKind {
        /// Returns true if `self` can follow keyword let.
        ///
        /// e.g. `let a = xx;`, `let {a:{}} = 1`
        pub fn follows_keyword_let(self, _strict: bool) -> bool {
            match self {
                Self::Word(WordKind::Keyword(Keyword::Let))
                | TokenKind::LBrace
                | TokenKind::LBracket
                | Self::Word(WordKind::Ident(..))
                | TokenKind::Word(WordKind::Keyword(Keyword::Yield))
                | TokenKind::Word(WordKind::Keyword(Keyword::Await)) => true,
                _ => false,
            }
        }
    }

    impl Word {
        pub fn cow(&self) -> Cow<Atom> {
            match self {
                Word::Keyword(k) => Cow::Owned(k.into_atom()),
                Word::Ident(IdentLike::Known(w)) => Cow::Owned((*w).into()),
                Word::Ident(IdentLike::Other(w)) => Cow::Borrowed(w),
                Word::False => Cow::Owned(atom!("false")),
                Word::True => Cow::Owned(atom!("true")),
                Word::Null => Cow::Owned(atom!("null")),
            }
        }
    }

    impl Debug for Token {
        /// This method is called only in the case of parsing failure.
        #[cold]
        #[inline(never)]
        fn fmt(&self, f: &mut Formatter<'_>) -> fmt::Result {
            match self {
                Word(w) => write!(f, "{w:?}")?,
                Arrow => write!(f, "=>")?,
                Hash => write!(f, "#")?,
                At => write!(f, "@")?,
                Dot => write!(f, ".")?,
                DotDotDot => write!(f, "...")?,
                Bang => write!(f, "!")?,
                LParen => write!(f, "(")?,
                RParen => write!(f, ")")?,
                LBracket => write!(f, "[")?,
                RBracket => write!(f, "]")?,
                LBrace => write!(f, "{{")?,
                RBrace => write!(f, "}}")?,
                Semi => write!(f, ";")?,
                Comma => write!(f, ",")?,
                BackQuote => write!(f, "`")?,
                Template { raw, .. } => write!(f, "template token ({raw})")?,
                Colon => write!(f, ":")?,
                BinOp(op) => write!(f, "{}", BinaryOp::from(*op).as_str())?,
                AssignOp(op) => write!(f, "{}", op.as_str())?,
                DollarLBrace => write!(f, "${{")?,
                QuestionMark => write!(f, "?")?,
                PlusPlus => write!(f, "++")?,
                MinusMinus => write!(f, "--")?,
                Tilde => write!(f, "~")?,
                Str { value, raw } => write!(f, "string literal ({value:?}, {raw})")?,
                Regex(exp, flags) => write!(f, "regexp literal ({exp}, {flags})")?,
                Num { value, raw, .. } => write!(f, "numeric literal ({value}, {raw})")?,
                BigInt { value, raw } => write!(f, "bigint literal ({value}, {raw})")?,
                JSXName { name } => write!(f, "jsx name ({name})")?,
                JSXText { raw, .. } => write!(f, "jsx text ({raw})")?,
                JSXTagStart => write!(f, "< (jsx tag start)")?,
                JSXTagEnd => write!(f, "> (jsx tag end)")?,
                Shebang(_) => write!(f, "#!")?,
                Error(e) => write!(f, "<lexing error: {e:?}>")?,
                Eof => write!(f, "<eof>")?,
            }

            Ok(())
        }
    }
}
