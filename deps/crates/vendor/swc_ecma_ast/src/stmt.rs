use is_macro::Is;
use swc_common::{ast_node, util::take::Take, EqIgnoreSpan, Span, SyntaxContext, DUMMY_SP};

use crate::{
    decl::{Decl, VarDecl},
    expr::Expr,
    pat::Pat,
    Ident, Lit, Str, UsingDecl,
};

/// Use when only block statements are allowed.
#[ast_node("BlockStatement")]
#[derive(Eq, Hash, EqIgnoreSpan, Default)]
#[cfg_attr(feature = "arbitrary", derive(arbitrary::Arbitrary))]
#[cfg_attr(feature = "shrink-to-fit", derive(shrink_to_fit::ShrinkToFit))]
pub struct BlockStmt {
    /// Span including the braces.
    pub span: Span,

    pub ctxt: SyntaxContext,

    pub stmts: Vec<Stmt>,
}

impl Take for BlockStmt {
    fn dummy() -> Self {
        BlockStmt {
            span: DUMMY_SP,
            stmts: Vec::new(),
            ctxt: Default::default(),
        }
    }
}

#[ast_node(no_clone)]
#[derive(Eq, Hash, Is, EqIgnoreSpan)]
#[cfg_attr(feature = "arbitrary", derive(arbitrary::Arbitrary))]
#[cfg_attr(feature = "shrink-to-fit", derive(shrink_to_fit::ShrinkToFit))]
pub enum Stmt {
    #[tag("BlockStatement")]
    Block(BlockStmt),

    #[tag("EmptyStatement")]
    Empty(EmptyStmt),

    #[tag("DebuggerStatement")]
    Debugger(DebuggerStmt),

    #[tag("WithStatement")]
    With(WithStmt),

    #[tag("ReturnStatement")]
    #[is(name = "return_stmt")]
    Return(ReturnStmt),

    #[tag("LabeledStatement")]
    Labeled(LabeledStmt),

    #[tag("BreakStatement")]
    #[is(name = "break_stmt")]
    Break(BreakStmt),

    #[tag("ContinueStatement")]
    #[is(name = "continue_stmt")]
    Continue(ContinueStmt),

    #[tag("IfStatement")]
    #[is(name = "if_stmt")]
    If(IfStmt),

    #[tag("SwitchStatement")]
    Switch(SwitchStmt),

    #[tag("ThrowStatement")]
    Throw(ThrowStmt),

    /// A try statement. If handler is null then finalizer must be a BlockStmt.
    #[tag("TryStatement")]
    #[is(name = "try_stmt")]
    Try(Box<TryStmt>),

    #[tag("WhileStatement")]
    #[is(name = "while_stmt")]
    While(WhileStmt),

    #[tag("DoWhileStatement")]
    DoWhile(DoWhileStmt),

    #[tag("ForStatement")]
    #[is(name = "for_stmt")]
    For(ForStmt),

    #[tag("ForInStatement")]
    ForIn(ForInStmt),

    #[tag("ForOfStatement")]
    ForOf(ForOfStmt),

    #[tag("ClassDeclaration")]
    #[tag("FunctionDeclaration")]
    #[tag("VariableDeclaration")]
    #[tag("TsInterfaceDeclaration")]
    #[tag("TsTypeAliasDeclaration")]
    #[tag("TsEnumDeclaration")]
    #[tag("TsModuleDeclaration")]
    #[tag("UsingDeclaration")]
    Decl(Decl),

    #[tag("ExpressionStatement")]
    Expr(ExprStmt),
}

boxed!(Stmt, [TryStmt]);

macro_rules! stmt_from {
    ($($varant_ty:ty),*) => {
        $(
            bridge_from!(Box<crate::Stmt>, crate::Stmt, $varant_ty);
            bridge_from!(crate::ModuleItem, crate::Stmt, $varant_ty);
        )*
    };
}

stmt_from!(
    ExprStmt,
    BlockStmt,
    EmptyStmt,
    DebuggerStmt,
    WithStmt,
    ReturnStmt,
    LabeledStmt,
    BreakStmt,
    ContinueStmt,
    IfStmt,
    SwitchStmt,
    ThrowStmt,
    TryStmt,
    WhileStmt,
    DoWhileStmt,
    ForStmt,
    ForInStmt,
    ForOfStmt,
    Decl
);

impl Stmt {
    pub fn is_use_strict(&self) -> bool {
        match self {
            Stmt::Expr(expr) => match *expr.expr {
                Expr::Lit(Lit::Str(Str { ref raw, .. })) => {
                    matches!(raw, Some(value) if value == "\"use strict\"" || value == "'use strict'")
                }
                _ => false,
            },
            _ => false,
        }
    }

    /// Returns true if the statement does not prevent the directives below
    /// `self` from being directives.
    pub fn can_precede_directive(&self) -> bool {
        match self {
            Stmt::Expr(expr) => matches!(*expr.expr, Expr::Lit(Lit::Str(_))),
            _ => false,
        }
    }
}

// Memory layout depedns on the version of rustc.
// #[cfg(target_pointer_width = "64")]
// assert_eq_size!(Stmt, [u8; 56]);

// Implement Clone without inline to avoid multiple copies of the
// implementation.
impl Clone for Stmt {
    fn clone(&self) -> Self {
        use Stmt::*;
        match self {
            #[cfg(all(swc_ast_unknown, feature = "encoding-impl"))]
            Unknown(tag, v) => Unknown(*tag, v.clone()),
            Block(s) => Block(s.clone()),
            Empty(s) => Empty(s.clone()),
            Debugger(s) => Debugger(s.clone()),
            With(s) => With(s.clone()),
            Return(s) => Return(s.clone()),
            Labeled(s) => Labeled(s.clone()),
            Break(s) => Break(s.clone()),
            Continue(s) => Continue(s.clone()),
            If(s) => If(s.clone()),
            Switch(s) => Switch(s.clone()),
            Throw(s) => Throw(s.clone()),
            Try(s) => Try(s.clone()),
            While(s) => While(s.clone()),
            DoWhile(s) => DoWhile(s.clone()),
            For(s) => For(s.clone()),
            ForIn(s) => ForIn(s.clone()),
            ForOf(s) => ForOf(s.clone()),
            Decl(s) => Decl(s.clone()),
            Expr(s) => Expr(s.clone()),
        }
    }
}

impl Default for Stmt {
    fn default() -> Self {
        Self::Empty(EmptyStmt { span: DUMMY_SP })
    }
}

impl Take for Stmt {
    fn dummy() -> Self {
        Default::default()
    }
}

#[ast_node("ExpressionStatement")]
#[derive(Eq, Hash, EqIgnoreSpan, Default)]
#[cfg_attr(feature = "arbitrary", derive(arbitrary::Arbitrary))]
#[cfg_attr(feature = "shrink-to-fit", derive(shrink_to_fit::ShrinkToFit))]
pub struct ExprStmt {
    pub span: Span,
    #[cfg_attr(feature = "serde-impl", serde(rename = "expression"))]
    pub expr: Box<Expr>,
}

#[ast_node("EmptyStatement")]
#[derive(Eq, Hash, Copy, EqIgnoreSpan)]
#[cfg_attr(feature = "arbitrary", derive(arbitrary::Arbitrary))]
#[cfg_attr(feature = "shrink-to-fit", derive(shrink_to_fit::ShrinkToFit))]
pub struct EmptyStmt {
    /// Span of semicolon.
    pub span: Span,
}

#[ast_node("DebuggerStatement")]
#[derive(Eq, Hash, Copy, EqIgnoreSpan)]
#[cfg_attr(feature = "arbitrary", derive(arbitrary::Arbitrary))]
#[cfg_attr(feature = "shrink-to-fit", derive(shrink_to_fit::ShrinkToFit))]
pub struct DebuggerStmt {
    pub span: Span,
}

#[ast_node("WithStatement")]
#[derive(Eq, Hash, EqIgnoreSpan, Default)]
#[cfg_attr(feature = "arbitrary", derive(arbitrary::Arbitrary))]
#[cfg_attr(feature = "shrink-to-fit", derive(shrink_to_fit::ShrinkToFit))]
pub struct WithStmt {
    pub span: Span,
    #[cfg_attr(feature = "serde-impl", serde(rename = "object"))]
    pub obj: Box<Expr>,
    pub body: Box<Stmt>,
}

#[ast_node("ReturnStatement")]
#[derive(Eq, Hash, EqIgnoreSpan, Default)]
#[cfg_attr(feature = "arbitrary", derive(arbitrary::Arbitrary))]
#[cfg_attr(feature = "shrink-to-fit", derive(shrink_to_fit::ShrinkToFit))]
pub struct ReturnStmt {
    pub span: Span,
    #[cfg_attr(feature = "serde-impl", serde(default, rename = "argument"))]
    #[cfg_attr(
        feature = "encoding-impl",
        encoding(with = "cbor4ii::core::types::Maybe")
    )]
    pub arg: Option<Box<Expr>>,
}

#[ast_node("LabeledStatement")]
#[derive(Eq, Hash, EqIgnoreSpan, Default)]
#[cfg_attr(feature = "arbitrary", derive(arbitrary::Arbitrary))]
#[cfg_attr(feature = "shrink-to-fit", derive(shrink_to_fit::ShrinkToFit))]
pub struct LabeledStmt {
    pub span: Span,
    pub label: Ident,
    pub body: Box<Stmt>,
}

#[ast_node("BreakStatement")]
#[derive(Eq, Hash, EqIgnoreSpan, Default)]
#[cfg_attr(feature = "arbitrary", derive(arbitrary::Arbitrary))]
#[cfg_attr(feature = "shrink-to-fit", derive(shrink_to_fit::ShrinkToFit))]
pub struct BreakStmt {
    pub span: Span,
    #[cfg_attr(feature = "serde-impl", serde(default))]
    #[cfg_attr(
        feature = "encoding-impl",
        encoding(with = "cbor4ii::core::types::Maybe")
    )]
    pub label: Option<Ident>,
}

#[ast_node("ContinueStatement")]
#[derive(Eq, Hash, EqIgnoreSpan, Default)]
#[cfg_attr(feature = "arbitrary", derive(arbitrary::Arbitrary))]
#[cfg_attr(feature = "shrink-to-fit", derive(shrink_to_fit::ShrinkToFit))]
pub struct ContinueStmt {
    pub span: Span,
    #[cfg_attr(feature = "serde-impl", serde(default))]
    #[cfg_attr(
        feature = "encoding-impl",
        encoding(with = "cbor4ii::core::types::Maybe")
    )]
    pub label: Option<Ident>,
}

#[ast_node("IfStatement")]
#[derive(Eq, Hash, EqIgnoreSpan, Default)]
#[cfg_attr(feature = "arbitrary", derive(arbitrary::Arbitrary))]
#[cfg_attr(feature = "shrink-to-fit", derive(shrink_to_fit::ShrinkToFit))]
pub struct IfStmt {
    pub span: Span,
    pub test: Box<Expr>,

    #[cfg_attr(feature = "serde-impl", serde(rename = "consequent"))]
    pub cons: Box<Stmt>,

    #[cfg_attr(feature = "serde-impl", serde(default, rename = "alternate"))]
    #[cfg_attr(
        feature = "encoding-impl",
        encoding(with = "cbor4ii::core::types::Maybe")
    )]
    pub alt: Option<Box<Stmt>>,
}

#[ast_node("SwitchStatement")]
#[derive(Eq, Hash, EqIgnoreSpan, Default)]
#[cfg_attr(feature = "arbitrary", derive(arbitrary::Arbitrary))]
#[cfg_attr(feature = "shrink-to-fit", derive(shrink_to_fit::ShrinkToFit))]
pub struct SwitchStmt {
    pub span: Span,
    pub discriminant: Box<Expr>,
    pub cases: Vec<SwitchCase>,
}

#[ast_node("ThrowStatement")]
#[derive(Eq, Hash, EqIgnoreSpan, Default)]
#[cfg_attr(feature = "arbitrary", derive(arbitrary::Arbitrary))]
#[cfg_attr(feature = "shrink-to-fit", derive(shrink_to_fit::ShrinkToFit))]
pub struct ThrowStmt {
    pub span: Span,
    #[cfg_attr(feature = "serde-impl", serde(rename = "argument"))]
    pub arg: Box<Expr>,
}

#[ast_node("TryStatement")]
#[derive(Eq, Hash, EqIgnoreSpan, Default)]
#[cfg_attr(feature = "arbitrary", derive(arbitrary::Arbitrary))]
#[cfg_attr(feature = "shrink-to-fit", derive(shrink_to_fit::ShrinkToFit))]
pub struct TryStmt {
    pub span: Span,

    pub block: BlockStmt,

    #[cfg_attr(feature = "serde-impl", serde(default))]
    #[cfg_attr(
        feature = "encoding-impl",
        encoding(with = "cbor4ii::core::types::Maybe")
    )]
    pub handler: Option<CatchClause>,

    #[cfg_attr(feature = "serde-impl", serde(default))]
    #[cfg_attr(
        feature = "encoding-impl",
        encoding(with = "cbor4ii::core::types::Maybe")
    )]
    pub finalizer: Option<BlockStmt>,
}

#[ast_node("WhileStatement")]
#[derive(Eq, Hash, EqIgnoreSpan, Default)]
#[cfg_attr(feature = "arbitrary", derive(arbitrary::Arbitrary))]
#[cfg_attr(feature = "shrink-to-fit", derive(shrink_to_fit::ShrinkToFit))]
pub struct WhileStmt {
    pub span: Span,
    pub test: Box<Expr>,
    pub body: Box<Stmt>,
}

#[ast_node("DoWhileStatement")]
#[derive(Eq, Hash, EqIgnoreSpan, Default)]
#[cfg_attr(feature = "arbitrary", derive(arbitrary::Arbitrary))]
#[cfg_attr(feature = "shrink-to-fit", derive(shrink_to_fit::ShrinkToFit))]
pub struct DoWhileStmt {
    pub span: Span,
    pub test: Box<Expr>,
    pub body: Box<Stmt>,
}

#[ast_node("ForStatement")]
#[derive(Eq, Hash, EqIgnoreSpan, Default)]
#[cfg_attr(feature = "arbitrary", derive(arbitrary::Arbitrary))]
#[cfg_attr(feature = "shrink-to-fit", derive(shrink_to_fit::ShrinkToFit))]
pub struct ForStmt {
    pub span: Span,

    #[cfg_attr(feature = "serde-impl", serde(default))]
    #[cfg_attr(
        feature = "encoding-impl",
        encoding(with = "cbor4ii::core::types::Maybe")
    )]
    pub init: Option<VarDeclOrExpr>,

    #[cfg_attr(feature = "serde-impl", serde(default))]
    #[cfg_attr(
        feature = "encoding-impl",
        encoding(with = "cbor4ii::core::types::Maybe")
    )]
    pub test: Option<Box<Expr>>,

    #[cfg_attr(feature = "serde-impl", serde(default))]
    #[cfg_attr(
        feature = "encoding-impl",
        encoding(with = "cbor4ii::core::types::Maybe")
    )]
    pub update: Option<Box<Expr>>,

    pub body: Box<Stmt>,
}

#[ast_node("ForInStatement")]
#[derive(Eq, Hash, EqIgnoreSpan, Default)]
#[cfg_attr(feature = "arbitrary", derive(arbitrary::Arbitrary))]
#[cfg_attr(feature = "shrink-to-fit", derive(shrink_to_fit::ShrinkToFit))]
pub struct ForInStmt {
    pub span: Span,
    pub left: ForHead,
    pub right: Box<Expr>,
    pub body: Box<Stmt>,
}

#[ast_node("ForOfStatement")]
#[derive(Eq, Hash, EqIgnoreSpan, Default)]
#[cfg_attr(feature = "arbitrary", derive(arbitrary::Arbitrary))]
#[cfg_attr(feature = "shrink-to-fit", derive(shrink_to_fit::ShrinkToFit))]
pub struct ForOfStmt {
    pub span: Span,
    /// Span of the await token.
    ///
    /// es2018
    ///
    /// for-await-of statements, e.g., `for await (const x of xs) {`
    #[cfg_attr(feature = "serde-impl", serde(default, rename = "await"))]
    pub is_await: bool,
    pub left: ForHead,
    pub right: Box<Expr>,
    pub body: Box<Stmt>,
}

impl Take for ForOfStmt {
    fn dummy() -> Self {
        Default::default()
    }
}

#[ast_node("SwitchCase")]
#[derive(Eq, Hash, EqIgnoreSpan, Default)]
#[cfg_attr(feature = "arbitrary", derive(arbitrary::Arbitrary))]
#[cfg_attr(feature = "shrink-to-fit", derive(shrink_to_fit::ShrinkToFit))]
pub struct SwitchCase {
    pub span: Span,

    /// None for `default:`
    #[cfg_attr(feature = "serde-impl", serde(default))]
    #[cfg_attr(
        feature = "encoding-impl",
        encoding(with = "cbor4ii::core::types::Maybe")
    )]
    pub test: Option<Box<Expr>>,

    #[cfg_attr(feature = "serde-impl", serde(rename = "consequent"))]
    pub cons: Vec<Stmt>,
}

impl Take for SwitchCase {
    fn dummy() -> Self {
        Self {
            span: DUMMY_SP,
            test: None,
            cons: Vec::new(),
        }
    }
}

#[ast_node("CatchClause")]
#[derive(Eq, Hash, EqIgnoreSpan, Default)]
#[cfg_attr(feature = "arbitrary", derive(arbitrary::Arbitrary))]
#[cfg_attr(feature = "shrink-to-fit", derive(shrink_to_fit::ShrinkToFit))]
pub struct CatchClause {
    pub span: Span,
    /// es2019
    ///
    /// The param is null if the catch binding is omitted. E.g., try { foo() }
    /// catch { bar() }
    #[cfg_attr(feature = "serde-impl", serde(default))]
    #[cfg_attr(
        feature = "encoding-impl",
        encoding(with = "cbor4ii::core::types::Maybe")
    )]
    pub param: Option<Pat>,

    pub body: BlockStmt,
}

/// A head for for-in and for-of loop.
#[ast_node]
#[derive(Eq, Hash, Is, EqIgnoreSpan)]
#[cfg_attr(feature = "arbitrary", derive(arbitrary::Arbitrary))]
#[cfg_attr(feature = "shrink-to-fit", derive(shrink_to_fit::ShrinkToFit))]
pub enum ForHead {
    #[tag("VariableDeclaration")]
    VarDecl(Box<VarDecl>),

    #[tag("UsingDeclaration")]
    UsingDecl(Box<UsingDecl>),

    #[tag("*")]
    Pat(Box<Pat>),
}

bridge_from!(ForHead, Box<VarDecl>, VarDecl);
bridge_from!(ForHead, Box<Pat>, Pat);

impl Take for ForHead {
    fn dummy() -> Self {
        Default::default()
    }
}

impl Default for ForHead {
    fn default() -> Self {
        ForHead::Pat(Take::dummy())
    }
}

#[ast_node]
#[derive(Eq, Hash, Is, EqIgnoreSpan)]
#[allow(variant_size_differences)]
#[cfg_attr(feature = "arbitrary", derive(arbitrary::Arbitrary))]
#[cfg_attr(feature = "shrink-to-fit", derive(shrink_to_fit::ShrinkToFit))]
pub enum VarDeclOrExpr {
    #[tag("VariableDeclaration")]
    VarDecl(Box<VarDecl>),

    #[tag("*")]
    Expr(Box<Expr>),
}

bridge_from!(VarDeclOrExpr, Box<VarDecl>, VarDecl);
bridge_from!(VarDeclOrExpr, Box<Expr>, Expr);

impl Take for VarDeclOrExpr {
    fn dummy() -> Self {
        VarDeclOrExpr::Expr(Take::dummy())
    }
}
