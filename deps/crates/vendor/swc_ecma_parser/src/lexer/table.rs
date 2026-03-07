//! Lookup table for byte handlers.
//!
//! Idea is taken from ratel.
//!
//! https://github.com/ratel-rust/ratel-core/blob/e55a1310ba69a3f5ce2a9a6eef643feced02ac08/ratel/src/lexer/mod.rs#L665

use either::Either;
use swc_common::input::Input;

use super::{pos_span, LexResult, Lexer};
use crate::{
    error::SyntaxError,
    lexer::{
        char_ext::CharExt,
        token::{Token, TokenValue},
    },
};

pub(super) type ByteHandler = fn(&mut Lexer<'_>) -> LexResult<Token>;

/// Lookup table mapping any incoming byte to a handler function defined below.
#[rustfmt::skip]
pub(super) static BYTE_HANDLERS: [ByteHandler; 256] = [
//   0    1    2    3    4    5    6    7    8    9    A    B    C    D    E    F   //
    ERR, ERR, ERR, ERR, ERR, ERR, ERR, ERR, ERR, ERR, ERR, ERR, ERR, ERR, ERR, ERR, // 0
    ERR, ERR, ERR, ERR, ERR, ERR, ERR, ERR, ERR, ERR, ERR, ERR, ERR, ERR, ERR, ERR, // 1
    ERR, EXL, QOT, HSH, IDN, PRC, AMP, QOT, PNO, PNC, ATR, PLS, COM, MIN, PRD, SLH, // 2
    ZER, DIG, DIG, DIG, DIG, DIG, DIG, DIG, DIG, DIG, COL, SEM, LSS, EQL, MOR, QST, // 3
    AT_, IDN, IDN, IDN, IDN, IDN, IDN, IDN, IDN, IDN, IDN, IDN, IDN, IDN, IDN, IDN, // 4
    IDN, IDN, IDN, IDN, IDN, IDN, IDN, IDN, IDN, IDN, IDN, BTO, IDN, BTC, CRT, IDN, // 5
    TPL, L_A, L_B, L_C, L_D, L_E, L_F, L_G, L_H, L_I, L_J, L_K, L_L, L_M, L_N, L_O, // 6
    L_P, L_Q, L_R, L_S, L_T, L_U, L_V, L_W, L_X, L_Y, L_Z, BEO, PIP, BEC, TLD, ERR, // 7
    UNI, UNI, UNI, UNI, UNI, UNI, UNI, UNI, UNI, UNI, UNI, UNI, UNI, UNI, UNI, UNI, // 8
    UNI, UNI, UNI, UNI, UNI, UNI, UNI, UNI, UNI, UNI, UNI, UNI, UNI, UNI, UNI, UNI, // 9
    UNI, UNI, UNI, UNI, UNI, UNI, UNI, UNI, UNI, UNI, UNI, UNI, UNI, UNI, UNI, UNI, // A
    UNI, UNI, UNI, UNI, UNI, UNI, UNI, UNI, UNI, UNI, UNI, UNI, UNI, UNI, UNI, UNI, // B
    UNI, UNI, UNI, UNI, UNI, UNI, UNI, UNI, UNI, UNI, UNI, UNI, UNI, UNI, UNI, UNI, // C
    UNI, UNI, UNI, UNI, UNI, UNI, UNI, UNI, UNI, UNI, UNI, UNI, UNI, UNI, UNI, UNI, // D
    UNI, UNI, UNI, UNI, UNI, UNI, UNI, UNI, UNI, UNI, UNI, UNI, UNI, UNI, UNI, UNI, // E
    UNI, UNI, UNI, UNI, UNI, UNI, UNI, UNI, UNI, UNI, UNI, UNI, UNI, UNI, UNI, UNI, // F
];

const ERR: ByteHandler = |lexer| {
    let c = unsafe {
        // Safety: Byte handler is only called for non-last characters
        // Get the char representation for error messages
        lexer.input.cur_as_char().unwrap_unchecked()
    };

    let start = lexer.cur_pos();
    lexer.bump(c.len_utf8());
    lexer.error_span(pos_span(start), SyntaxError::UnexpectedChar { c })?
};

/// Identifier and we know that this cannot be a keyword or known ident.
const IDN: ByteHandler = |lexer| lexer.read_ident_unknown();

const L_A: ByteHandler = |lexer| {
    lexer.read_keyword_with(&|s| match s {
        "abstract" => Some(Token::Abstract),
        "as" => Some(Token::As),
        "await" => Some(Token::Await),
        "async" => Some(Token::Async),
        "assert" => Some(Token::Assert),
        "asserts" => Some(Token::Asserts),
        "any" => Some(Token::Any),
        "accessor" => Some(Token::Accessor),
        _ => None,
    })
};

const L_B: ByteHandler = |lexer| {
    lexer.read_keyword_with(&|s| match s {
        "break" => Some(Token::Break),
        "boolean" => Some(Token::Boolean),
        "bigint" => Some(Token::Bigint),
        _ => None,
    })
};

const L_C: ByteHandler = |lexer| {
    lexer.read_keyword_with(&|s| match s {
        "case" => Some(Token::Case),
        "catch" => Some(Token::Catch),
        "class" => Some(Token::Class),
        "const" => Some(Token::Const),
        "continue" => Some(Token::Continue),
        _ => None,
    })
};

const L_D: ByteHandler = |lexer| {
    lexer.read_keyword_with(&|s| match s {
        "debugger" => Some(Token::Debugger),
        "default" => Some(Token::Default),
        "delete" => Some(Token::Delete),
        "do" => Some(Token::Do),
        "declare" => Some(Token::Declare),
        _ => None,
    })
};

const L_E: ByteHandler = |lexer| {
    lexer.read_keyword_with(&|s| match s {
        "else" => Some(Token::Else),
        "enum" => Some(Token::Enum),
        "export" => Some(Token::Export),
        "extends" => Some(Token::Extends),
        _ => None,
    })
};

const L_F: ByteHandler = |lexer| {
    lexer.read_keyword_with(&|s| match s {
        "false" => Some(Token::False),
        "finally" => Some(Token::Finally),
        "for" => Some(Token::For),
        "function" => Some(Token::Function),
        "from" => Some(Token::From),
        _ => None,
    })
};

const L_G: ByteHandler = |lexer| {
    lexer.read_keyword_with(&|s| match s {
        "global" => Some(Token::Global),
        "get" => Some(Token::Get),
        _ => None,
    })
};

const L_H: ByteHandler = IDN;

const L_I: ByteHandler = |lexer| {
    lexer.read_keyword_with(&|s| match s {
        "if" => Some(Token::If),
        "import" => Some(Token::Import),
        "in" => Some(Token::In),
        "instanceof" => Some(Token::InstanceOf),
        "is" => Some(Token::Is),
        "infer" => Some(Token::Infer),
        "interface" => Some(Token::Interface),
        "implements" => Some(Token::Implements),
        "intrinsic" => Some(Token::Intrinsic),
        _ => None,
    })
};

const L_J: ByteHandler = IDN;

const L_K: ByteHandler = |lexer| {
    lexer.read_keyword_with(&|s| match s {
        "keyof" => Some(Token::Keyof),
        _ => None,
    })
};

const L_L: ByteHandler = |lexer| {
    lexer.read_keyword_with(&|s| match s {
        "let" => Some(Token::Let),
        _ => None,
    })
};

const L_M: ByteHandler = |lexer| {
    lexer.read_keyword_with(&|s| match s {
        "meta" => Some(Token::Meta),
        _ => None,
    })
};

const L_N: ByteHandler = |lexer| {
    lexer.read_keyword_with(&|s| match s {
        "new" => Some(Token::New),
        "null" => Some(Token::Null),
        "number" => Some(Token::Number),
        "never" => Some(Token::Never),
        "namespace" => Some(Token::Namespace),
        _ => None,
    })
};

const L_O: ByteHandler = |lexer| {
    lexer.read_keyword_with(&|s| match s {
        "of" => Some(Token::Of),
        "object" => Some(Token::Object),
        "out" => Some(Token::Out),
        "override" => Some(Token::Override),
        _ => None,
    })
};

const L_P: ByteHandler = |lexer| {
    lexer.read_keyword_with(&|s| match s {
        "public" => Some(Token::Public),
        "package" => Some(Token::Package),
        "protected" => Some(Token::Protected),
        "private" => Some(Token::Private),
        _ => None,
    })
};

const L_Q: ByteHandler = IDN;

const L_R: ByteHandler = |lexer| {
    lexer.read_keyword_with(&|s| match s {
        "return" => Some(Token::Return),
        "readonly" => Some(Token::Readonly),
        "require" => Some(Token::Require),
        _ => None,
    })
};

const L_S: ByteHandler = |lexer| {
    lexer.read_keyword_with(&|s| match s {
        "super" => Some(Token::Super),
        "static" => Some(Token::Static),
        "switch" => Some(Token::Switch),
        "symbol" => Some(Token::Symbol),
        "set" => Some(Token::Set),
        "string" => Some(Token::String),
        "satisfies" => Some(Token::Satisfies),
        _ => None,
    })
};

const L_T: ByteHandler = |lexer| {
    lexer.read_keyword_with(&|s| match s {
        "this" => Some(Token::This),
        "throw" => Some(Token::Throw),
        "true" => Some(Token::True),
        "typeof" => Some(Token::TypeOf),
        "try" => Some(Token::Try),
        "type" => Some(Token::Type),
        "target" => Some(Token::Target),
        _ => None,
    })
};

const L_U: ByteHandler = |lexer| {
    lexer.read_keyword_with(&|s| match s {
        "using" => Some(Token::Using),
        "unique" => Some(Token::Unique),
        "undefined" => Some(Token::Undefined),
        "unknown" => Some(Token::Unknown),
        _ => None,
    })
};

const L_V: ByteHandler = |lexer| {
    lexer.read_keyword_with(&|s| match s {
        "var" => Some(Token::Var),
        "void" => Some(Token::Void),
        _ => None,
    })
};

const L_W: ByteHandler = |lexer| {
    lexer.read_keyword_with(&|s| match s {
        "while" => Some(Token::While),
        "with" => Some(Token::With),
        _ => None,
    })
};

const L_X: ByteHandler = IDN;

const L_Y: ByteHandler = |lexer| {
    lexer.read_keyword_with(&|s| match s {
        "yield" => Some(Token::Yield),
        _ => None,
    })
};

const L_Z: ByteHandler = IDN;

/// `0`
const ZER: ByteHandler = |lexer| lexer.read_token_zero();

/// Numbers
const DIG: ByteHandler = |lexer| {
    debug_assert!(lexer.cur().is_some_and(|cur| cur != b'0'));
    lexer.read_number::<false, false>().map(|v| match v {
        Either::Left(value) => {
            lexer.state.set_token_value(TokenValue::Num(value));
            Token::Num
        }
        Either::Right(value) => {
            lexer.state.set_token_value(TokenValue::BigInt(value));
            Token::BigInt
        }
    })
};

/// String literals with `'` or `"`
const QOT: ByteHandler = |lexer| lexer.read_str_lit();

/// Unicode - handles multi-byte UTF-8 sequences
const UNI: ByteHandler = |lexer| {
    let c = unsafe {
        // Safety: Byte handler is only called for non-last characters
        // For non-ASCII bytes, we need the full char
        lexer.input.cur_as_char().unwrap_unchecked()
    };

    // Identifier or keyword. '\uXXXX' sequences are allowed in
    // identifiers, so '\' also dispatches to that.
    if c == '\\' || c.is_ident_start() {
        return lexer.read_ident_unknown();
    }

    let start = lexer.cur_pos();
    lexer.bump(c.len_utf8());
    lexer.error_span(pos_span(start), SyntaxError::UnexpectedChar { c })?
};

/// `:`
const COL: ByteHandler = |lexer| lexer.read_token_colon();

/// `%`
const PRC: ByteHandler = |lexer| lexer.read_token_mul_mod::<false>();

/// `*`
const ATR: ByteHandler = |lexer| lexer.read_token_mul_mod::<true>();

/// `?`
const QST: ByteHandler = |lexer| lexer.read_token_question_mark();

/// `&`
const AMP: ByteHandler = |lexer| lexer.read_token_logical::<b'&'>();

/// `|`
const PIP: ByteHandler = |lexer| lexer.read_token_logical::<b'|'>();

macro_rules! single_char {
    ($name:ident, $c:literal, $token:ident) => {
        const $name: ByteHandler = |lexer| {
            unsafe {
                lexer.input.bump_bytes(1);
            }
            Ok(Token::$token)
        };
    };
}

single_char!(SEM, b';', Semi);
single_char!(COM, b',', Comma);

/// `\``
const TPL: ByteHandler = |lexer| lexer.read_token_back_quote();

single_char!(TLD, b'~', Tilde);
single_char!(AT_, b'@', At);

single_char!(PNO, b'(', LParen);
single_char!(PNC, b')', RParen);

single_char!(BTO, b'[', LBracket);
single_char!(BTC, b']', RBracket);

single_char!(BEO, b'{', LBrace);
single_char!(BEC, b'}', RBrace);

/// `^`
const CRT: ByteHandler = |lexer| {
    // Bitwise xor
    unsafe {
        lexer.input.bump_bytes(1);
    }
    Ok(if lexer.input.cur() == Some(b'=') {
        unsafe {
            lexer.input.bump_bytes(1);
        }
        Token::BitXorEq
    } else {
        Token::Caret
    })
};

/// `+`
const PLS: ByteHandler = |lexer| lexer.read_token_plus_minus::<b'+'>();

/// `-`
const MIN: ByteHandler = |lexer| lexer.read_token_plus_minus::<b'-'>();

/// `!`
const EXL: ByteHandler = |lexer| lexer.read_token_bang_or_eq::<b'!'>();

/// `=`
const EQL: ByteHandler = |lexer| lexer.read_token_bang_or_eq::<b'='>();

/// `.`
const PRD: ByteHandler = |lexer| lexer.read_token_dot();

/// `<`
const LSS: ByteHandler = |lexer| lexer.read_token_lt_gt::<b'<'>();

/// `>`
const MOR: ByteHandler = |lexer| lexer.read_token_lt_gt::<b'>'>();

/// `/`
const SLH: ByteHandler = |lexer| lexer.read_slash();

/// `#`
const HSH: ByteHandler = |lexer| lexer.read_token_number_sign();
