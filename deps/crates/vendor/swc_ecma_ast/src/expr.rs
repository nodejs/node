#![allow(clippy::vec_box)]
use std::mem::transmute;

use is_macro::Is;
use string_enum::StringEnum;
use swc_atoms::{Atom, Wtf8Atom};
use swc_common::{
    ast_node, util::take::Take, BytePos, EqIgnoreSpan, Span, Spanned, SyntaxContext, DUMMY_SP,
};

use crate::{
    class::Class,
    function::Function,
    ident::{Ident, PrivateName},
    jsx::{JSXElement, JSXEmptyExpr, JSXFragment, JSXMemberExpr, JSXNamespacedName},
    lit::Lit,
    operators::{AssignOp, BinaryOp, UnaryOp, UpdateOp},
    pat::Pat,
    prop::Prop,
    stmt::BlockStmt,
    typescript::{
        TsAsExpr, TsConstAssertion, TsInstantiation, TsNonNullExpr, TsSatisfiesExpr, TsTypeAnn,
        TsTypeAssertion, TsTypeParamDecl, TsTypeParamInstantiation,
    },
    ArrayPat, BindingIdent, ComputedPropName, Id, IdentName, ImportPhase, Invalid, KeyValueProp,
    Number, ObjectPat, PropName, Str,
};

#[ast_node(no_clone)]
#[derive(Eq, Hash, Is, EqIgnoreSpan)]
#[cfg_attr(feature = "arbitrary", derive(arbitrary::Arbitrary))]
#[cfg_attr(feature = "shrink-to-fit", derive(shrink_to_fit::ShrinkToFit))]
pub enum Expr {
    #[tag("ThisExpression")]
    This(ThisExpr),

    #[tag("ArrayExpression")]
    Array(ArrayLit),

    #[tag("ObjectExpression")]
    Object(ObjectLit),

    #[tag("FunctionExpression")]
    #[is(name = "fn_expr")]
    Fn(FnExpr),

    #[tag("UnaryExpression")]
    Unary(UnaryExpr),

    /// `++v`, `--v`, `v++`, `v--`
    #[tag("UpdateExpression")]
    Update(UpdateExpr),

    #[tag("BinaryExpression")]
    Bin(BinExpr),

    #[tag("AssignmentExpression")]
    Assign(AssignExpr),

    //
    // Logical {
    //
    //     op: LogicalOp,
    //     left: Box<Expr>,
    //     right: Box<Expr>,
    // },
    /// A member expression. If computed is true, the node corresponds to a
    /// computed (a[b]) member expression and property is an Expression. If
    /// computed is false, the node corresponds to a static (a.b) member
    /// expression and property is an Identifier.
    #[tag("MemberExpression")]
    Member(MemberExpr),

    #[tag("SuperPropExpression")]
    SuperProp(SuperPropExpr),

    /// true ? 'a' : 'b'
    #[tag("ConditionalExpression")]
    Cond(CondExpr),

    #[tag("CallExpression")]
    Call(CallExpr),

    /// `new Cat()`
    #[tag("NewExpression")]
    New(NewExpr),

    #[tag("SequenceExpression")]
    Seq(SeqExpr),

    #[tag("Identifier")]
    Ident(Ident),

    #[tag("StringLiteral")]
    #[tag("BooleanLiteral")]
    #[tag("NullLiteral")]
    #[tag("NumericLiteral")]
    #[tag("RegExpLiteral")]
    #[tag("JSXText")]
    #[tag("BigIntLiteral")]
    Lit(Lit),

    #[tag("TemplateLiteral")]
    Tpl(Tpl),

    #[tag("TaggedTemplateExpression")]
    TaggedTpl(TaggedTpl),

    #[tag("ArrowFunctionExpression")]
    Arrow(ArrowExpr),

    #[tag("ClassExpression")]
    Class(ClassExpr),

    #[tag("YieldExpression")]
    #[is(name = "yield_expr")]
    Yield(YieldExpr),

    #[tag("MetaProperty")]
    MetaProp(MetaPropExpr),

    #[tag("AwaitExpression")]
    #[is(name = "await_expr")]
    Await(AwaitExpr),

    #[tag("ParenthesisExpression")]
    Paren(ParenExpr),

    #[tag("JSXMemberExpression")]
    JSXMember(JSXMemberExpr),

    #[tag("JSXNamespacedName")]
    JSXNamespacedName(JSXNamespacedName),

    #[tag("JSXEmptyExpression")]
    JSXEmpty(JSXEmptyExpr),

    #[tag("JSXElement")]
    JSXElement(Box<JSXElement>),

    #[tag("JSXFragment")]
    JSXFragment(JSXFragment),

    #[tag("TsTypeAssertion")]
    TsTypeAssertion(TsTypeAssertion),

    #[tag("TsConstAssertion")]
    TsConstAssertion(TsConstAssertion),

    #[tag("TsNonNullExpression")]
    TsNonNull(TsNonNullExpr),

    #[tag("TsAsExpression")]
    TsAs(TsAsExpr),

    #[tag("TsInstantiation")]
    TsInstantiation(TsInstantiation),

    #[tag("TsSatisfiesExpression")]
    TsSatisfies(TsSatisfiesExpr),

    #[tag("PrivateName")]
    PrivateName(PrivateName),

    #[tag("OptionalChainingExpression")]
    OptChain(OptChainExpr),

    #[tag("Invalid")]
    Invalid(Invalid),
}

bridge_from!(Box<Expr>, Box<JSXElement>, JSXElement);

// Memory layout depends on the version of rustc.
// #[cfg(target_pointer_width = "64")]
// assert_eq_size!(Expr, [u8; 80]);

impl Expr {
    /// Creates `void 0`.
    #[inline]
    pub fn undefined(span: Span) -> Box<Expr> {
        UnaryExpr {
            span,
            op: op!("void"),
            arg: Lit::Num(Number {
                span,
                value: 0.0,
                raw: None,
            })
            .into(),
        }
        .into()
    }

    pub fn is_null(&self) -> bool {
        matches!(self, Expr::Lit(Lit::Null(_)))
    }

    pub fn leftmost(&self) -> Option<&Ident> {
        match self {
            Expr::Ident(i) => Some(i),
            Expr::Member(MemberExpr { obj, .. }) => obj.leftmost(),
            Expr::OptChain(opt) => opt.base.as_member()?.obj.leftmost(),
            _ => None,
        }
    }

    pub fn is_ident_ref_to<S>(&self, ident: &S) -> bool
    where
        S: ?Sized,
        Atom: PartialEq<S>,
    {
        match self {
            Expr::Ident(i) => i.sym == *ident,
            _ => false,
        }
    }

    /// Unwraps an expression with a given function.
    ///
    /// If the provided function returns [Some], the function is called again
    /// with the returned value. If the provided functions returns [None],
    /// the last expression is returned.
    pub fn unwrap_with<'a, F>(&'a self, mut op: F) -> &'a Expr
    where
        F: FnMut(&'a Expr) -> Option<&'a Expr>,
    {
        let mut cur = self;
        loop {
            match op(cur) {
                Some(next) => cur = next,
                None => return cur,
            }
        }
    }

    /// Unwraps an expression with a given function.
    ///
    /// If the provided function returns [Some], the function is called again
    /// with the returned value. If the provided functions returns [None],
    /// the last expression is returned.
    pub fn unwrap_mut_with<'a, F>(&'a mut self, mut op: F) -> &'a mut Expr
    where
        F: FnMut(&'a mut Expr) -> Option<&'a mut Expr>,
    {
        let mut cur = self;
        loop {
            match unsafe {
                // Safety: Polonius is not yet stable
                op(transmute::<&mut _, &mut _>(cur))
            } {
                Some(next) => cur = next,
                None => {
                    return cur;
                }
            }
        }
    }

    /// Normalize parenthesized expressions.
    ///
    /// This will normalize `(foo)`, `((foo))`, ... to `foo`.
    ///
    /// If `self` is not a parenthesized expression, it will be returned as is.
    pub fn unwrap_parens(&self) -> &Expr {
        self.unwrap_with(|e| {
            if let Expr::Paren(expr) = e {
                Some(&expr.expr)
            } else {
                None
            }
        })
    }

    /// Normalize parenthesized expressions.
    ///
    /// This will normalize `(foo)`, `((foo))`, ... to `foo`.
    ///
    /// If `self` is not a parenthesized expression, it will be returned as is.
    pub fn unwrap_parens_mut(&mut self) -> &mut Expr {
        self.unwrap_mut_with(|e| {
            if let Expr::Paren(expr) = e {
                Some(&mut expr.expr)
            } else {
                None
            }
        })
    }

    /// Normalize sequences and parenthesized expressions.
    ///
    /// This returns the last expression of a sequence expression or the
    /// expression of a parenthesized expression.
    pub fn unwrap_seqs_and_parens(&self) -> &Self {
        self.unwrap_with(|expr| match expr {
            Expr::Seq(SeqExpr { exprs, .. }) => exprs.last().map(|v| &**v),
            Expr::Paren(ParenExpr { expr, .. }) => Some(expr),
            _ => None,
        })
    }

    /// Creates an expression from `exprs`. This will return first element if
    /// the length is 1 and a sequential expression otherwise.
    ///
    /// # Panics
    ///
    /// Panics if `exprs` is empty.
    pub fn from_exprs(mut exprs: Vec<Box<Expr>>) -> Box<Expr> {
        debug_assert!(!exprs.is_empty(), "`exprs` must not be empty");

        if exprs.len() == 1 {
            exprs.remove(0)
        } else {
            SeqExpr {
                span: DUMMY_SP,
                exprs,
            }
            .into()
        }
    }

    #[deprecated(note = "Use `directness_matters` instead")]
    pub fn directness_maters(&self) -> bool {
        self.directness_matters()
    }

    /// Returns true for `eval` and member expressions.
    pub fn directness_matters(&self) -> bool {
        self.is_ident_ref_to("eval") || matches!(self, Expr::Member(..))
    }

    pub fn with_span(mut self, span: Span) -> Expr {
        self.set_span(span);
        self
    }

    pub fn set_span(&mut self, span: Span) {
        match self {
            Expr::Ident(i) => {
                i.span = span;
            }
            Expr::This(e) => e.span = span,
            Expr::Array(e) => e.span = span,
            Expr::Object(e) => e.span = span,
            Expr::Fn(e) => e.function.span = span,
            Expr::Unary(e) => e.span = span,
            Expr::Update(e) => e.span = span,
            Expr::Bin(e) => e.span = span,
            Expr::Assign(e) => e.span = span,
            Expr::Member(e) => e.span = span,
            Expr::SuperProp(e) => e.span = span,
            Expr::Cond(e) => e.span = span,
            Expr::Call(e) => e.span = span,
            Expr::New(e) => e.span = span,
            Expr::Seq(e) => e.span = span,
            Expr::Tpl(e) => e.span = span,
            Expr::TaggedTpl(e) => e.span = span,
            Expr::Arrow(e) => e.span = span,
            Expr::Class(e) => e.class.span = span,
            Expr::Yield(e) => e.span = span,
            Expr::Invalid(e) => e.span = span,
            Expr::TsAs(e) => e.span = span,
            Expr::TsTypeAssertion(e) => e.span = span,
            Expr::TsConstAssertion(e) => e.span = span,
            Expr::TsSatisfies(e) => e.span = span,
            Expr::TsNonNull(e) => e.span = span,
            Expr::TsInstantiation(e) => e.span = span,
            Expr::MetaProp(e) => e.span = span,
            Expr::Await(e) => e.span = span,
            Expr::Paren(e) => e.span = span,
            Expr::JSXMember(e) => e.span = span,
            Expr::JSXNamespacedName(e) => e.span = span,
            Expr::JSXEmpty(e) => e.span = span,
            Expr::JSXElement(e) => e.span = span,
            Expr::JSXFragment(e) => e.span = span,
            Expr::PrivateName(e) => e.span = span,
            Expr::OptChain(e) => e.span = span,
            Expr::Lit(e) => e.set_span(span),
            #[cfg(all(swc_ast_unknown, feature = "encoding-impl"))]
            _ => swc_common::unknown!(),
        }
    }
}

// Implement Clone without inline to avoid multiple copies of the
// implementation.
impl Clone for Expr {
    fn clone(&self) -> Self {
        use Expr::*;
        match self {
            #[cfg(all(swc_ast_unknown, feature = "encoding-impl"))]
            Unknown(tag, v) => Unknown(*tag, v.clone()),
            This(e) => This(e.clone()),
            Array(e) => Array(e.clone()),
            Object(e) => Object(e.clone()),
            Fn(e) => Fn(e.clone()),
            Unary(e) => Unary(e.clone()),
            Update(e) => Update(e.clone()),
            Bin(e) => Bin(e.clone()),
            Assign(e) => Assign(e.clone()),
            Member(e) => Member(e.clone()),
            SuperProp(e) => SuperProp(e.clone()),
            Cond(e) => Cond(e.clone()),
            Call(e) => Call(e.clone()),
            New(e) => New(e.clone()),
            Seq(e) => Seq(e.clone()),
            Ident(e) => Ident(e.clone()),
            Lit(e) => Lit(e.clone()),
            Tpl(e) => Tpl(e.clone()),
            TaggedTpl(e) => TaggedTpl(e.clone()),
            Arrow(e) => Arrow(e.clone()),
            Class(e) => Class(e.clone()),
            Yield(e) => Yield(e.clone()),
            MetaProp(e) => MetaProp(e.clone()),
            Await(e) => Await(e.clone()),
            Paren(e) => Paren(e.clone()),
            JSXMember(e) => JSXMember(e.clone()),
            JSXNamespacedName(e) => JSXNamespacedName(e.clone()),
            JSXEmpty(e) => JSXEmpty(e.clone()),
            JSXElement(e) => JSXElement(e.clone()),
            JSXFragment(e) => JSXFragment(e.clone()),
            TsTypeAssertion(e) => TsTypeAssertion(e.clone()),
            TsConstAssertion(e) => TsConstAssertion(e.clone()),
            TsNonNull(e) => TsNonNull(e.clone()),
            TsAs(e) => TsAs(e.clone()),
            TsInstantiation(e) => TsInstantiation(e.clone()),
            PrivateName(e) => PrivateName(e.clone()),
            OptChain(e) => OptChain(e.clone()),
            Invalid(e) => Invalid(e.clone()),
            TsSatisfies(e) => TsSatisfies(e.clone()),
        }
    }
}

impl Take for Expr {
    fn dummy() -> Self {
        Invalid { span: DUMMY_SP }.into()
    }
}

impl Default for Expr {
    fn default() -> Self {
        Expr::Invalid(Default::default())
    }
}

bridge_expr_from!(Ident, IdentName);
bridge_expr_from!(Ident, Id);
bridge_expr_from!(FnExpr, Function);
bridge_expr_from!(ClassExpr, Class);

macro_rules! boxed_expr {
    ($T:ty) => {
        bridge_from!(Box<Expr>, Expr, $T);
    };
}

boxed_expr!(ThisExpr);
boxed_expr!(ArrayLit);
boxed_expr!(ObjectLit);
boxed_expr!(FnExpr);
boxed_expr!(UnaryExpr);
boxed_expr!(UpdateExpr);
boxed_expr!(BinExpr);
boxed_expr!(AssignExpr);
boxed_expr!(MemberExpr);
boxed_expr!(SuperPropExpr);
boxed_expr!(CondExpr);
boxed_expr!(CallExpr);
boxed_expr!(NewExpr);
boxed_expr!(SeqExpr);
bridge_from!(Box<Expr>, Expr, Ident);
boxed_expr!(Lit);
boxed_expr!(Tpl);
boxed_expr!(TaggedTpl);
boxed_expr!(ArrowExpr);
boxed_expr!(ClassExpr);
boxed_expr!(YieldExpr);
boxed_expr!(MetaPropExpr);
boxed_expr!(AwaitExpr);
boxed_expr!(ParenExpr);
boxed_expr!(JSXMemberExpr);
boxed_expr!(JSXNamespacedName);
boxed_expr!(JSXEmptyExpr);
boxed_expr!(Box<JSXElement>);
boxed_expr!(JSXFragment);
boxed_expr!(TsTypeAssertion);
boxed_expr!(TsSatisfiesExpr);
boxed_expr!(TsConstAssertion);
boxed_expr!(TsNonNullExpr);
boxed_expr!(TsAsExpr);
boxed_expr!(TsInstantiation);
boxed_expr!(PrivateName);
boxed_expr!(OptChainExpr);
boxed_expr!(Invalid);

#[ast_node("ThisExpression")]
#[derive(Eq, Hash, Copy, EqIgnoreSpan)]
#[cfg_attr(feature = "arbitrary", derive(arbitrary::Arbitrary))]
#[cfg_attr(feature = "shrink-to-fit", derive(shrink_to_fit::ShrinkToFit))]
pub struct ThisExpr {
    pub span: Span,
}

impl Take for ThisExpr {
    fn dummy() -> Self {
        ThisExpr { span: DUMMY_SP }
    }
}

/// Array literal.
#[ast_node("ArrayExpression")]
#[derive(Eq, Hash, EqIgnoreSpan, Default)]
#[cfg_attr(feature = "arbitrary", derive(arbitrary::Arbitrary))]
#[cfg_attr(feature = "shrink-to-fit", derive(shrink_to_fit::ShrinkToFit))]
pub struct ArrayLit {
    pub span: Span,

    #[cfg_attr(feature = "serde-impl", serde(default, rename = "elements"))]
    #[cfg_attr(
        feature = "encoding-impl",
        encoding(with = "swc_common::serializer::ArrayOption")
    )]
    pub elems: Vec<Option<ExprOrSpread>>,
}

impl Take for ArrayLit {
    fn dummy() -> Self {
        ArrayLit {
            span: DUMMY_SP,
            elems: Default::default(),
        }
    }
}

/// Object literal.
#[ast_node("ObjectExpression")]
#[derive(Eq, Hash, EqIgnoreSpan, Default)]
#[cfg_attr(feature = "arbitrary", derive(arbitrary::Arbitrary))]
#[cfg_attr(feature = "shrink-to-fit", derive(shrink_to_fit::ShrinkToFit))]
pub struct ObjectLit {
    pub span: Span,

    #[cfg_attr(feature = "serde-impl", serde(default, rename = "properties"))]
    pub props: Vec<PropOrSpread>,
}

impl ObjectLit {
    /// See [ImportWith] for details.
    ///
    /// Returns [None] if this is not a valid for `with` of [crate::ImportDecl].
    pub fn as_import_with(&self) -> Option<ImportWith> {
        let mut values = Vec::new();
        for prop in &self.props {
            match prop {
                PropOrSpread::Spread(..) => return None,
                PropOrSpread::Prop(prop) => match &**prop {
                    Prop::KeyValue(kv) => {
                        let key = match &kv.key {
                            PropName::Ident(i) => i.clone(),
                            PropName::Str(s) => {
                                let name = s.value.as_str()?;
                                IdentName::new(name.to_string().into(), s.span)
                            }
                            _ => return None,
                        };

                        values.push(ImportWithItem {
                            key,
                            value: match &*kv.value {
                                Expr::Lit(Lit::Str(s)) => s.clone(),
                                _ => return None,
                            },
                        });
                    }
                    _ => return None,
                },
                #[cfg(all(swc_ast_unknown, feature = "encoding-impl"))]
                _ => swc_common::unknown!(),
            }
        }

        Some(ImportWith {
            span: self.span,
            values,
        })
    }
}

impl From<ImportWith> for ObjectLit {
    fn from(v: ImportWith) -> Self {
        ObjectLit {
            span: v.span,
            props: v
                .values
                .into_iter()
                .map(|item| {
                    PropOrSpread::Prop(Box::new(Prop::KeyValue(KeyValueProp {
                        key: PropName::Ident(item.key),
                        value: Lit::Str(item.value).into(),
                    })))
                })
                .collect(),
        }
    }
}

/// According to the current spec `with` of [crate::ImportDecl] can only have
/// strings or idents as keys, can't be nested, can only have string literals as
/// values:

#[derive(Debug, Clone, PartialEq, Eq, Hash, EqIgnoreSpan)]
pub struct ImportWith {
    pub span: Span,
    pub values: Vec<ImportWithItem>,
}

impl ImportWith {
    pub fn get(&self, key: &str) -> Option<&Str> {
        self.values.iter().find_map(|item| {
            if item.key.sym == key {
                Some(&item.value)
            } else {
                None
            }
        })
    }
}

#[derive(Debug, Clone, PartialEq, Eq, Hash, EqIgnoreSpan)]
pub struct ImportWithItem {
    pub key: IdentName,
    pub value: Str,
}

impl Take for ObjectLit {
    fn dummy() -> Self {
        ObjectLit {
            span: DUMMY_SP,
            props: Default::default(),
        }
    }
}

#[ast_node]
#[derive(Eq, Hash, Is, EqIgnoreSpan)]
#[cfg_attr(feature = "arbitrary", derive(arbitrary::Arbitrary))]
#[cfg_attr(feature = "shrink-to-fit", derive(shrink_to_fit::ShrinkToFit))]
pub enum PropOrSpread {
    /// Spread properties, e.g., `{a: 1, ...obj, b: 2}`.
    #[tag("SpreadElement")]
    Spread(SpreadElement),

    #[tag("*")]
    Prop(Box<Prop>),
}

bridge_from!(PropOrSpread, Box<Prop>, Prop);

impl Take for PropOrSpread {
    fn dummy() -> Self {
        PropOrSpread::Spread(SpreadElement {
            dot3_token: DUMMY_SP,
            expr: Take::dummy(),
        })
    }
}

#[ast_node("SpreadElement")]
#[derive(Eq, Hash, EqIgnoreSpan, Default)]
#[cfg_attr(feature = "arbitrary", derive(arbitrary::Arbitrary))]
#[cfg_attr(feature = "shrink-to-fit", derive(shrink_to_fit::ShrinkToFit))]
pub struct SpreadElement {
    #[cfg_attr(feature = "serde-impl", serde(rename = "spread"))]
    #[span(lo)]
    pub dot3_token: Span,

    #[cfg_attr(feature = "serde-impl", serde(rename = "arguments"))]
    #[span(hi)]
    pub expr: Box<Expr>,
}

impl Take for SpreadElement {
    fn dummy() -> Self {
        SpreadElement {
            dot3_token: DUMMY_SP,
            expr: Take::dummy(),
        }
    }
}

#[ast_node("UnaryExpression")]
#[derive(Eq, Hash, EqIgnoreSpan, Default)]
#[cfg_attr(feature = "arbitrary", derive(arbitrary::Arbitrary))]
#[cfg_attr(feature = "shrink-to-fit", derive(shrink_to_fit::ShrinkToFit))]
pub struct UnaryExpr {
    pub span: Span,

    #[cfg_attr(feature = "serde-impl", serde(rename = "operator"))]
    pub op: UnaryOp,

    #[cfg_attr(feature = "serde-impl", serde(rename = "argument"))]
    pub arg: Box<Expr>,
}

impl Take for UnaryExpr {
    fn dummy() -> Self {
        UnaryExpr {
            span: DUMMY_SP,
            op: op!("!"),
            arg: Take::dummy(),
        }
    }
}

#[ast_node("UpdateExpression")]
#[derive(Eq, Hash, EqIgnoreSpan, Default)]
#[cfg_attr(feature = "arbitrary", derive(arbitrary::Arbitrary))]
#[cfg_attr(feature = "shrink-to-fit", derive(shrink_to_fit::ShrinkToFit))]
pub struct UpdateExpr {
    pub span: Span,

    #[cfg_attr(feature = "serde-impl", serde(rename = "operator"))]
    pub op: UpdateOp,

    pub prefix: bool,

    #[cfg_attr(feature = "serde-impl", serde(rename = "argument"))]
    pub arg: Box<Expr>,
}

impl Take for UpdateExpr {
    fn dummy() -> Self {
        UpdateExpr {
            span: DUMMY_SP,
            op: op!("++"),
            prefix: false,
            arg: Take::dummy(),
        }
    }
}

#[ast_node("BinaryExpression")]
#[derive(Eq, Hash, EqIgnoreSpan, Default)]
#[cfg_attr(feature = "arbitrary", derive(arbitrary::Arbitrary))]
#[cfg_attr(feature = "shrink-to-fit", derive(shrink_to_fit::ShrinkToFit))]
pub struct BinExpr {
    pub span: Span,

    #[cfg_attr(feature = "serde-impl", serde(rename = "operator"))]
    pub op: BinaryOp,

    pub left: Box<Expr>,

    pub right: Box<Expr>,
}

impl Take for BinExpr {
    fn dummy() -> Self {
        BinExpr {
            span: DUMMY_SP,
            op: op!("*"),
            left: Take::dummy(),
            right: Take::dummy(),
        }
    }
}

/// Function expression.
#[ast_node("FunctionExpression")]
#[derive(Eq, Hash, EqIgnoreSpan, Default)]
#[cfg_attr(feature = "arbitrary", derive(arbitrary::Arbitrary))]
#[cfg_attr(feature = "shrink-to-fit", derive(shrink_to_fit::ShrinkToFit))]
pub struct FnExpr {
    #[cfg_attr(feature = "serde-impl", serde(default, rename = "identifier"))]
    #[cfg_attr(
        feature = "encoding-impl",
        encoding(with = "cbor4ii::core::types::Maybe")
    )]
    pub ident: Option<Ident>,

    #[cfg_attr(feature = "serde-impl", serde(flatten))]
    #[span]
    pub function: Box<Function>,
}

impl Take for FnExpr {
    fn dummy() -> Self {
        FnExpr {
            ident: None,
            function: Take::dummy(),
        }
    }
}

impl From<Box<Function>> for FnExpr {
    fn from(function: Box<Function>) -> Self {
        Self {
            ident: None,
            function,
        }
    }
}

bridge_from!(FnExpr, Box<Function>, Function);
bridge_expr_from!(FnExpr, Box<Function>);

/// Class expression.
#[ast_node("ClassExpression")]
#[derive(Eq, Hash, EqIgnoreSpan, Default)]
#[cfg_attr(feature = "arbitrary", derive(arbitrary::Arbitrary))]
#[cfg_attr(feature = "shrink-to-fit", derive(shrink_to_fit::ShrinkToFit))]
pub struct ClassExpr {
    #[cfg_attr(feature = "serde-impl", serde(default, rename = "identifier"))]
    #[cfg_attr(
        feature = "encoding-impl",
        encoding(with = "cbor4ii::core::types::Maybe")
    )]
    pub ident: Option<Ident>,

    #[cfg_attr(feature = "serde-impl", serde(flatten))]
    #[span]
    pub class: Box<Class>,
}

impl Take for ClassExpr {
    fn dummy() -> Self {
        ClassExpr {
            ident: None,
            class: Take::dummy(),
        }
    }
}

impl From<Box<Class>> for ClassExpr {
    fn from(class: Box<Class>) -> Self {
        Self { ident: None, class }
    }
}

bridge_from!(ClassExpr, Box<Class>, Class);
bridge_expr_from!(ClassExpr, Box<Class>);

#[ast_node("AssignmentExpression")]
#[derive(Eq, Hash, EqIgnoreSpan, Default)]
#[cfg_attr(feature = "arbitrary", derive(arbitrary::Arbitrary))]
#[cfg_attr(feature = "shrink-to-fit", derive(shrink_to_fit::ShrinkToFit))]
pub struct AssignExpr {
    pub span: Span,

    #[cfg_attr(feature = "serde-impl", serde(rename = "operator"))]
    pub op: AssignOp,

    pub left: AssignTarget,

    pub right: Box<Expr>,
}

impl Take for AssignExpr {
    fn dummy() -> Self {
        AssignExpr {
            span: DUMMY_SP,
            op: op!("="),
            left: Take::dummy(),
            right: Take::dummy(),
        }
    }
}

impl AssignExpr {
    pub fn is_simple_assign(&self) -> bool {
        self.op == op!("=") && self.left.as_ident().is_some()
    }
}

#[ast_node("MemberExpression")]
#[derive(Eq, Hash, EqIgnoreSpan, Default)]
#[cfg_attr(feature = "arbitrary", derive(arbitrary::Arbitrary))]
#[cfg_attr(feature = "shrink-to-fit", derive(shrink_to_fit::ShrinkToFit))]
pub struct MemberExpr {
    pub span: Span,

    #[cfg_attr(feature = "serde-impl", serde(rename = "object"))]
    pub obj: Box<Expr>,

    #[cfg_attr(feature = "serde-impl", serde(rename = "property"))]
    pub prop: MemberProp,
}

#[ast_node]
#[derive(Eq, Hash, Is, EqIgnoreSpan)]
#[cfg_attr(feature = "arbitrary", derive(arbitrary::Arbitrary))]
#[cfg_attr(feature = "shrink-to-fit", derive(shrink_to_fit::ShrinkToFit))]
pub enum MemberProp {
    #[tag("Identifier")]
    Ident(IdentName),
    #[tag("PrivateName")]
    PrivateName(PrivateName),
    #[tag("Computed")]
    Computed(ComputedPropName),
}

impl MemberProp {
    pub fn is_ident_with(&self, sym: &str) -> bool {
        matches!(self, MemberProp::Ident(i) if i.sym == sym)
    }
}

#[ast_node("SuperPropExpression")]
#[derive(Eq, Hash, EqIgnoreSpan, Default)]
#[cfg_attr(feature = "arbitrary", derive(arbitrary::Arbitrary))]
#[cfg_attr(feature = "shrink-to-fit", derive(shrink_to_fit::ShrinkToFit))]
pub struct SuperPropExpr {
    pub span: Span,

    pub obj: Super,

    #[cfg_attr(feature = "serde-impl", serde(rename = "property"))]
    pub prop: SuperProp,
}

#[ast_node]
#[derive(Eq, Hash, Is, EqIgnoreSpan)]
#[cfg_attr(feature = "arbitrary", derive(arbitrary::Arbitrary))]
#[cfg_attr(feature = "shrink-to-fit", derive(shrink_to_fit::ShrinkToFit))]
pub enum SuperProp {
    #[tag("Identifier")]
    Ident(IdentName),
    #[tag("Computed")]
    Computed(ComputedPropName),
}

impl Take for MemberExpr {
    fn dummy() -> Self {
        MemberExpr {
            span: DUMMY_SP,
            obj: Take::dummy(),
            prop: Take::dummy(),
        }
    }
}

impl Take for MemberProp {
    fn dummy() -> Self {
        Default::default()
    }
}

impl Default for MemberProp {
    fn default() -> Self {
        MemberProp::Ident(Default::default())
    }
}

impl Take for SuperProp {
    fn dummy() -> Self {
        SuperProp::Ident(Default::default())
    }
}

impl Default for SuperProp {
    fn default() -> Self {
        SuperProp::Ident(Default::default())
    }
}

#[ast_node("ConditionalExpression")]
#[derive(Eq, Hash, EqIgnoreSpan, Default)]
#[cfg_attr(feature = "arbitrary", derive(arbitrary::Arbitrary))]
#[cfg_attr(feature = "shrink-to-fit", derive(shrink_to_fit::ShrinkToFit))]
pub struct CondExpr {
    pub span: Span,

    pub test: Box<Expr>,

    #[cfg_attr(feature = "serde-impl", serde(rename = "consequent"))]
    pub cons: Box<Expr>,

    #[cfg_attr(feature = "serde-impl", serde(rename = "alternate"))]
    pub alt: Box<Expr>,
}

impl Take for CondExpr {
    fn dummy() -> Self {
        CondExpr {
            span: DUMMY_SP,
            test: Take::dummy(),
            cons: Take::dummy(),
            alt: Take::dummy(),
        }
    }
}

#[ast_node("CallExpression")]
#[derive(Eq, Hash, EqIgnoreSpan, Default)]
#[cfg_attr(feature = "arbitrary", derive(arbitrary::Arbitrary))]
#[cfg_attr(feature = "shrink-to-fit", derive(shrink_to_fit::ShrinkToFit))]
pub struct CallExpr {
    pub span: Span,
    pub ctxt: SyntaxContext,

    pub callee: Callee,

    #[cfg_attr(feature = "serde-impl", serde(default, rename = "arguments"))]
    pub args: Vec<ExprOrSpread>,

    #[cfg_attr(feature = "serde-impl", serde(default, rename = "typeArguments"))]
    #[cfg_attr(
        feature = "encoding-impl",
        encoding(with = "cbor4ii::core::types::Maybe")
    )]
    pub type_args: Option<Box<TsTypeParamInstantiation>>,
    // pub type_params: Option<TsTypeParamInstantiation>,
}

impl Take for CallExpr {
    fn dummy() -> Self {
        Default::default()
    }
}

#[ast_node("NewExpression")]
#[derive(Eq, Hash, EqIgnoreSpan, Default)]
#[cfg_attr(feature = "arbitrary", derive(arbitrary::Arbitrary))]
#[cfg_attr(feature = "shrink-to-fit", derive(shrink_to_fit::ShrinkToFit))]
pub struct NewExpr {
    pub span: Span,

    pub ctxt: SyntaxContext,

    pub callee: Box<Expr>,

    #[cfg_attr(feature = "serde-impl", serde(default, rename = "arguments"))]
    #[cfg_attr(
        feature = "encoding-impl",
        encoding(with = "::cbor4ii::core::types::Maybe")
    )]
    pub args: Option<Vec<ExprOrSpread>>,

    #[cfg_attr(feature = "serde-impl", serde(default, rename = "typeArguments"))]
    #[cfg_attr(
        feature = "encoding-impl",
        encoding(with = "::cbor4ii::core::types::Maybe")
    )]
    pub type_args: Option<Box<TsTypeParamInstantiation>>,
    // pub type_params: Option<TsTypeParamInstantiation>,
}

impl Take for NewExpr {
    fn dummy() -> Self {
        Default::default()
    }
}

#[ast_node("SequenceExpression")]
#[derive(Eq, Hash, EqIgnoreSpan, Default)]
#[cfg_attr(feature = "arbitrary", derive(arbitrary::Arbitrary))]
#[cfg_attr(feature = "shrink-to-fit", derive(shrink_to_fit::ShrinkToFit))]
pub struct SeqExpr {
    pub span: Span,

    #[cfg_attr(feature = "serde-impl", serde(rename = "expressions"))]
    pub exprs: Vec<Box<Expr>>,
}

impl Take for SeqExpr {
    fn dummy() -> Self {
        SeqExpr {
            span: DUMMY_SP,
            exprs: Take::dummy(),
        }
    }
}

#[ast_node("ArrowFunctionExpression")]
#[derive(Eq, Hash, EqIgnoreSpan, Default)]
#[cfg_attr(feature = "arbitrary", derive(arbitrary::Arbitrary))]
#[cfg_attr(feature = "shrink-to-fit", derive(shrink_to_fit::ShrinkToFit))]
pub struct ArrowExpr {
    pub span: Span,

    pub ctxt: SyntaxContext,

    pub params: Vec<Pat>,

    /// This is boxed to reduce the type size of [Expr].
    pub body: Box<BlockStmtOrExpr>,

    #[cfg_attr(feature = "serde-impl", serde(default, rename = "async"))]
    pub is_async: bool,

    #[cfg_attr(feature = "serde-impl", serde(default, rename = "generator"))]
    pub is_generator: bool,

    #[cfg_attr(feature = "serde-impl", serde(default, rename = "typeParameters"))]
    #[cfg_attr(
        feature = "encoding-impl",
        encoding(with = "cbor4ii::core::types::Maybe")
    )]
    pub type_params: Option<Box<TsTypeParamDecl>>,

    #[cfg_attr(feature = "serde-impl", serde(default))]
    #[cfg_attr(
        feature = "encoding-impl",
        encoding(with = "cbor4ii::core::types::Maybe")
    )]
    pub return_type: Option<Box<TsTypeAnn>>,
}

impl Take for ArrowExpr {
    fn dummy() -> Self {
        ArrowExpr {
            ..Default::default()
        }
    }
}

#[ast_node("YieldExpression")]
#[derive(Eq, Hash, EqIgnoreSpan, Default)]
#[cfg_attr(feature = "arbitrary", derive(arbitrary::Arbitrary))]
#[cfg_attr(feature = "shrink-to-fit", derive(shrink_to_fit::ShrinkToFit))]
pub struct YieldExpr {
    pub span: Span,

    #[cfg_attr(feature = "serde-impl", serde(default, rename = "argument"))]
    #[cfg_attr(
        feature = "encoding-impl",
        encoding(with = "cbor4ii::core::types::Maybe")
    )]
    pub arg: Option<Box<Expr>>,

    #[cfg_attr(feature = "serde-impl", serde(default))]
    pub delegate: bool,
}

impl Take for YieldExpr {
    fn dummy() -> Self {
        YieldExpr {
            span: DUMMY_SP,
            arg: Take::dummy(),
            delegate: false,
        }
    }
}

#[ast_node("MetaProperty")]
#[derive(Eq, Hash, EqIgnoreSpan, Copy)]
#[cfg_attr(feature = "arbitrary", derive(arbitrary::Arbitrary))]
#[cfg_attr(feature = "shrink-to-fit", derive(shrink_to_fit::ShrinkToFit))]
pub struct MetaPropExpr {
    pub span: Span,
    pub kind: MetaPropKind,
}

#[derive(StringEnum, Clone, Copy, Eq, PartialEq, PartialOrd, Ord, Hash, EqIgnoreSpan)]
#[cfg_attr(feature = "arbitrary", derive(arbitrary::Arbitrary))]
#[cfg_attr(
    any(feature = "rkyv-impl"),
    derive(rkyv::Archive, rkyv::Serialize, rkyv::Deserialize)
)]
#[cfg_attr(feature = "rkyv-impl", derive(bytecheck::CheckBytes))]
#[cfg_attr(feature = "rkyv-impl", repr(u32))]
#[cfg_attr(feature = "shrink-to-fit", derive(shrink_to_fit::ShrinkToFit))]
#[cfg_attr(
    feature = "encoding-impl",
    derive(::swc_common::Encode, ::swc_common::Decode)
)]
#[cfg_attr(swc_ast_unknown, non_exhaustive)]
pub enum MetaPropKind {
    /// `new.target`
    NewTarget,
    /// `import.meta`
    ImportMeta,
}

#[ast_node("AwaitExpression")]
#[derive(Eq, Hash, EqIgnoreSpan, Default)]
#[cfg_attr(feature = "arbitrary", derive(arbitrary::Arbitrary))]
#[cfg_attr(feature = "shrink-to-fit", derive(shrink_to_fit::ShrinkToFit))]
pub struct AwaitExpr {
    pub span: Span,

    #[cfg_attr(feature = "serde-impl", serde(rename = "argument"))]
    pub arg: Box<Expr>,
}

#[ast_node("TemplateLiteral")]
#[derive(Eq, Hash, EqIgnoreSpan, Default)]
#[cfg_attr(feature = "arbitrary", derive(arbitrary::Arbitrary))]
#[cfg_attr(feature = "shrink-to-fit", derive(shrink_to_fit::ShrinkToFit))]
pub struct Tpl {
    pub span: Span,

    #[cfg_attr(feature = "serde-impl", serde(rename = "expressions"))]
    pub exprs: Vec<Box<Expr>>,

    pub quasis: Vec<TplElement>,
}

impl Take for Tpl {
    fn dummy() -> Self {
        Tpl {
            span: DUMMY_SP,
            exprs: Take::dummy(),
            quasis: Take::dummy(),
        }
    }
}

#[ast_node("TaggedTemplateExpression")]
#[derive(Eq, Hash, EqIgnoreSpan, Default)]
#[cfg_attr(feature = "arbitrary", derive(arbitrary::Arbitrary))]
#[cfg_attr(feature = "shrink-to-fit", derive(shrink_to_fit::ShrinkToFit))]
pub struct TaggedTpl {
    pub span: Span,

    pub ctxt: SyntaxContext,

    pub tag: Box<Expr>,

    #[cfg_attr(feature = "serde-impl", serde(default, rename = "typeParameters"))]
    #[cfg_attr(
        feature = "encoding-impl",
        encoding(with = "cbor4ii::core::types::Maybe")
    )]
    pub type_params: Option<Box<TsTypeParamInstantiation>>,

    /// This is boxed to reduce the type size of [Expr].
    #[cfg_attr(feature = "serde-impl", serde(rename = "template"))]
    pub tpl: Box<Tpl>,
}

impl Take for TaggedTpl {
    fn dummy() -> Self {
        Default::default()
    }
}

#[ast_node("TemplateElement")]
#[derive(Eq, Hash, EqIgnoreSpan, Default)]
#[cfg_attr(feature = "shrink-to-fit", derive(shrink_to_fit::ShrinkToFit))]
pub struct TplElement {
    pub span: Span,
    pub tail: bool,

    /// This value is never used by `swc_ecma_codegen`, and this fact is
    /// considered as a public API.
    ///
    /// If you are going to use codegen right after creating a [TplElement], you
    /// don't have to worry about this value.
    #[cfg_attr(
        feature = "encoding-impl",
        encoding(with = "cbor4ii::core::types::Maybe")
    )]
    pub cooked: Option<Wtf8Atom>,

    /// You may need to perform. `.replace("\r\n", "\n").replace('\r', "\n")` on
    /// this value.
    pub raw: Atom,
}

impl Take for TplElement {
    fn dummy() -> Self {
        TplElement {
            span: DUMMY_SP,
            tail: Default::default(),
            cooked: None,
            raw: Default::default(),
        }
    }
}

#[cfg(feature = "arbitrary")]
#[cfg_attr(docsrs, doc(cfg(feature = "arbitrary")))]
impl<'a> arbitrary::Arbitrary<'a> for TplElement {
    fn arbitrary(u: &mut arbitrary::Unstructured<'_>) -> arbitrary::Result<Self> {
        let span = u.arbitrary()?;
        let cooked = Some(u.arbitrary::<Wtf8Atom>()?.into());
        let raw = u.arbitrary::<String>()?.into();

        Ok(Self {
            span,
            tail: false,
            cooked,
            raw,
        })
    }
}

#[ast_node("ParenthesisExpression")]
#[derive(Eq, Hash, EqIgnoreSpan, Default)]
#[cfg_attr(feature = "arbitrary", derive(arbitrary::Arbitrary))]
#[cfg_attr(feature = "shrink-to-fit", derive(shrink_to_fit::ShrinkToFit))]
pub struct ParenExpr {
    pub span: Span,

    #[cfg_attr(feature = "serde-impl", serde(rename = "expression"))]
    pub expr: Box<Expr>,
}
impl Take for ParenExpr {
    fn dummy() -> Self {
        ParenExpr {
            span: DUMMY_SP,
            expr: Take::dummy(),
        }
    }
}

#[ast_node]
#[derive(Eq, Hash, Is, EqIgnoreSpan)]
#[cfg_attr(feature = "arbitrary", derive(arbitrary::Arbitrary))]
#[cfg_attr(feature = "shrink-to-fit", derive(shrink_to_fit::ShrinkToFit))]
pub enum Callee {
    #[tag("Super")]
    #[is(name = "super_")]
    Super(Super),

    #[tag("Import")]
    Import(Import),

    #[tag("*")]
    Expr(Box<Expr>),
}

impl Default for Callee {
    fn default() -> Self {
        Callee::Super(Default::default())
    }
}

impl Take for Callee {
    fn dummy() -> Self {
        Callee::Super(Take::dummy())
    }
}

#[ast_node("Super")]
#[derive(Eq, Hash, Copy, EqIgnoreSpan, Default)]
#[cfg_attr(feature = "arbitrary", derive(arbitrary::Arbitrary))]
#[cfg_attr(feature = "shrink-to-fit", derive(shrink_to_fit::ShrinkToFit))]
pub struct Super {
    pub span: Span,
}

impl Take for Super {
    fn dummy() -> Self {
        Super { span: DUMMY_SP }
    }
}

#[ast_node("Import")]
#[derive(Eq, Hash, Copy, EqIgnoreSpan)]
#[cfg_attr(feature = "arbitrary", derive(arbitrary::Arbitrary))]
#[cfg_attr(feature = "shrink-to-fit", derive(shrink_to_fit::ShrinkToFit))]
pub struct Import {
    pub span: Span,
    pub phase: ImportPhase,
}

impl Take for Import {
    fn dummy() -> Self {
        Import {
            span: DUMMY_SP,
            phase: ImportPhase::default(),
        }
    }
}

#[derive(Clone, Debug, PartialEq, Eq, Hash, EqIgnoreSpan)]
#[cfg_attr(feature = "arbitrary", derive(arbitrary::Arbitrary))]
#[cfg_attr(feature = "shrink-to-fit", derive(shrink_to_fit::ShrinkToFit))]
#[cfg_attr(
    any(feature = "rkyv-impl"),
    derive(rkyv::Archive, rkyv::Serialize, rkyv::Deserialize)
)]
#[cfg_attr(
    feature = "rkyv",
    rkyv(serialize_bounds(__S: rkyv::ser::Writer + rkyv::ser::Allocator,
        __S::Error: rkyv::rancor::Source))
)]
#[cfg_attr(
    feature = "rkyv-impl",
    rkyv(deserialize_bounds(__D::Error: rkyv::rancor::Source))
)]
#[cfg_attr(
                    feature = "rkyv-impl",
                    rkyv(bytecheck(bounds(
                        __C: rkyv::validation::ArchiveContext,
                        __C::Error: rkyv::rancor::Source
                    )))
                )]
#[cfg_attr(feature = "rkyv-impl", repr(C))]
#[cfg_attr(feature = "serde-impl", derive(serde::Serialize, serde::Deserialize))]
#[cfg_attr(
    feature = "encoding-impl",
    derive(::swc_common::Encode, ::swc_common::Decode)
)]
pub struct ExprOrSpread {
    #[cfg_attr(feature = "serde-impl", serde(default))]
    #[cfg_attr(feature = "__rkyv", rkyv(omit_bounds))]
    #[cfg_attr(
        feature = "encoding-impl",
        encoding(with = "cbor4ii::core::types::Maybe")
    )]
    pub spread: Option<Span>,

    #[cfg_attr(feature = "serde-impl", serde(rename = "expression"))]
    #[cfg_attr(feature = "__rkyv", rkyv(omit_bounds))]
    pub expr: Box<Expr>,
}

impl Spanned for ExprOrSpread {
    #[inline]
    fn span(&self) -> Span {
        let expr = self.expr.span();
        match self.spread {
            Some(spread) => expr.with_lo(spread.lo()),
            None => expr,
        }
    }

    #[inline]
    fn span_lo(&self) -> BytePos {
        match self.spread {
            Some(s) => s.lo,
            None => self.expr.span_lo(),
        }
    }

    #[inline]
    fn span_hi(&self) -> BytePos {
        self.expr.span_hi()
    }
}

impl From<Box<Expr>> for ExprOrSpread {
    fn from(expr: Box<Expr>) -> Self {
        Self { expr, spread: None }
    }
}

bridge_from!(ExprOrSpread, Box<Expr>, Expr);

#[ast_node]
#[derive(Eq, Hash, Is, EqIgnoreSpan)]
#[allow(variant_size_differences)]
#[cfg_attr(feature = "arbitrary", derive(arbitrary::Arbitrary))]
#[cfg_attr(feature = "shrink-to-fit", derive(shrink_to_fit::ShrinkToFit))]
pub enum BlockStmtOrExpr {
    #[tag("BlockStatement")]
    BlockStmt(BlockStmt),
    #[tag("*")]
    Expr(Box<Expr>),
}

impl Default for BlockStmtOrExpr {
    fn default() -> Self {
        BlockStmtOrExpr::BlockStmt(Default::default())
    }
}

impl<T> From<T> for BlockStmtOrExpr
where
    T: Into<Expr>,
{
    fn from(e: T) -> Self {
        Self::Expr(Box::new(e.into()))
    }
}

impl Take for BlockStmtOrExpr {
    fn dummy() -> Self {
        BlockStmtOrExpr::Expr(Take::dummy())
    }
}

#[ast_node]
#[derive(Is, Eq, Hash, EqIgnoreSpan)]
#[cfg_attr(feature = "arbitrary", derive(arbitrary::Arbitrary))]
#[cfg_attr(feature = "shrink-to-fit", derive(shrink_to_fit::ShrinkToFit))]
pub enum AssignTarget {
    #[tag("Identifier")]
    #[tag("MemberExpression")]
    #[tag("SuperPropExpression")]
    #[tag("OptionalChainingExpression")]
    #[tag("ParenthesisExpression")]
    #[tag("TsAsExpression")]
    #[tag("TsSatisfiesExpression")]
    #[tag("TsNonNullExpression")]
    #[tag("TsTypeAssertion")]
    #[tag("TsInstantiation")]
    Simple(SimpleAssignTarget),
    #[tag("ArrayPattern")]
    #[tag("ObjectPattern")]
    Pat(AssignTargetPat),
}

impl TryFrom<Pat> for AssignTarget {
    type Error = Pat;

    fn try_from(p: Pat) -> Result<Self, Self::Error> {
        Ok(match p {
            Pat::Array(a) => AssignTargetPat::Array(a).into(),
            Pat::Object(o) => AssignTargetPat::Object(o).into(),

            Pat::Ident(i) => SimpleAssignTarget::Ident(i).into(),
            Pat::Invalid(i) => SimpleAssignTarget::Invalid(i).into(),

            Pat::Expr(e) => match Self::try_from(e) {
                Ok(v) => v,
                Err(e) => return Err(e.into()),
            },

            _ => return Err(p),
        })
    }
}
impl TryFrom<Box<Pat>> for AssignTarget {
    type Error = Box<Pat>;

    fn try_from(p: Box<Pat>) -> Result<Self, Self::Error> {
        (*p).try_into().map_err(Box::new)
    }
}

impl TryFrom<Box<Expr>> for AssignTarget {
    type Error = Box<Expr>;

    fn try_from(e: Box<Expr>) -> Result<Self, Self::Error> {
        Ok(Self::Simple(SimpleAssignTarget::try_from(e)?))
    }
}

#[ast_node]
#[derive(Is, Eq, Hash, EqIgnoreSpan)]
#[cfg_attr(feature = "arbitrary", derive(arbitrary::Arbitrary))]
#[cfg_attr(feature = "shrink-to-fit", derive(shrink_to_fit::ShrinkToFit))]
pub enum AssignTargetPat {
    #[tag("ArrayPattern")]
    Array(ArrayPat),
    #[tag("ObjectPattern")]
    Object(ObjectPat),
    #[tag("Invalid")]
    Invalid(Invalid),
}

impl Take for AssignTargetPat {
    fn dummy() -> Self {
        Default::default()
    }
}

impl Default for AssignTargetPat {
    fn default() -> Self {
        AssignTargetPat::Invalid(Take::dummy())
    }
}

impl From<AssignTargetPat> for Pat {
    fn from(pat: AssignTargetPat) -> Self {
        match pat {
            AssignTargetPat::Array(a) => a.into(),
            AssignTargetPat::Object(o) => o.into(),
            AssignTargetPat::Invalid(i) => i.into(),
            #[cfg(all(swc_ast_unknown, feature = "encoding-impl"))]
            _ => swc_common::unknown!(),
        }
    }
}

impl From<AssignTargetPat> for Box<Pat> {
    fn from(pat: AssignTargetPat) -> Self {
        Box::new(pat.into())
    }
}

impl TryFrom<Pat> for AssignTargetPat {
    type Error = Pat;

    fn try_from(p: Pat) -> Result<Self, Self::Error> {
        Ok(match p {
            Pat::Array(a) => AssignTargetPat::Array(a),
            Pat::Object(o) => AssignTargetPat::Object(o),
            Pat::Invalid(i) => AssignTargetPat::Invalid(i),

            _ => return Err(p),
        })
    }
}

#[ast_node]
#[derive(Is, Eq, Hash, EqIgnoreSpan)]
#[cfg_attr(feature = "arbitrary", derive(arbitrary::Arbitrary))]
#[cfg_attr(feature = "shrink-to-fit", derive(shrink_to_fit::ShrinkToFit))]
pub enum SimpleAssignTarget {
    /// Note: This type is to help implementing visitor and the field `type_ann`
    /// is always [None].
    #[tag("Identifier")]
    Ident(BindingIdent),
    #[tag("MemberExpression")]
    Member(MemberExpr),
    #[tag("SuperPropExpression")]
    SuperProp(SuperPropExpr),
    #[tag("ParenthesisExpression")]
    Paren(ParenExpr),
    #[tag("OptionalChainingExpression")]
    OptChain(OptChainExpr),
    #[tag("TsAsExpression")]
    TsAs(TsAsExpr),
    #[tag("TsSatisfiesExpression")]
    TsSatisfies(TsSatisfiesExpr),
    #[tag("TsNonNullExpression")]
    TsNonNull(TsNonNullExpr),
    #[tag("TsTypeAssertion")]
    TsTypeAssertion(TsTypeAssertion),
    #[tag("TsInstantiation")]
    TsInstantiation(TsInstantiation),

    #[tag("Invaliid")]
    Invalid(Invalid),
}

impl TryFrom<Box<Expr>> for SimpleAssignTarget {
    type Error = Box<Expr>;

    fn try_from(e: Box<Expr>) -> Result<Self, Self::Error> {
        Ok(match *e {
            Expr::Ident(i) => SimpleAssignTarget::Ident(i.into()),
            Expr::Member(m) => SimpleAssignTarget::Member(m),
            Expr::SuperProp(s) => SimpleAssignTarget::SuperProp(s),
            Expr::OptChain(s) => SimpleAssignTarget::OptChain(s),
            Expr::Paren(s) => SimpleAssignTarget::Paren(s),
            Expr::TsAs(a) => SimpleAssignTarget::TsAs(a),
            Expr::TsSatisfies(s) => SimpleAssignTarget::TsSatisfies(s),
            Expr::TsNonNull(n) => SimpleAssignTarget::TsNonNull(n),
            Expr::TsTypeAssertion(a) => SimpleAssignTarget::TsTypeAssertion(a),
            Expr::TsInstantiation(a) => SimpleAssignTarget::TsInstantiation(a),
            _ => return Err(e),
        })
    }
}

bridge_from!(SimpleAssignTarget, BindingIdent, Ident);

impl SimpleAssignTarget {
    pub fn leftmost(&self) -> Option<&Ident> {
        match self {
            SimpleAssignTarget::Ident(i) => Some(&i.id),
            SimpleAssignTarget::Member(MemberExpr { obj, .. }) => obj.leftmost(),
            _ => None,
        }
    }
}

impl Take for SimpleAssignTarget {
    fn dummy() -> Self {
        SimpleAssignTarget::Invalid(Take::dummy())
    }
}

bridge_from!(AssignTarget, BindingIdent, Ident);
bridge_from!(AssignTarget, SimpleAssignTarget, BindingIdent);
bridge_from!(AssignTarget, SimpleAssignTarget, MemberExpr);
bridge_from!(AssignTarget, SimpleAssignTarget, SuperPropExpr);
bridge_from!(AssignTarget, SimpleAssignTarget, ParenExpr);
bridge_from!(AssignTarget, SimpleAssignTarget, TsAsExpr);
bridge_from!(AssignTarget, SimpleAssignTarget, TsSatisfiesExpr);
bridge_from!(AssignTarget, SimpleAssignTarget, TsNonNullExpr);
bridge_from!(AssignTarget, SimpleAssignTarget, TsTypeAssertion);

bridge_from!(AssignTarget, AssignTargetPat, ArrayPat);
bridge_from!(AssignTarget, AssignTargetPat, ObjectPat);

impl From<SimpleAssignTarget> for Box<Expr> {
    fn from(s: SimpleAssignTarget) -> Self {
        match s {
            SimpleAssignTarget::Ident(i) => i.into(),
            SimpleAssignTarget::Member(m) => m.into(),
            SimpleAssignTarget::SuperProp(s) => s.into(),
            SimpleAssignTarget::Paren(s) => s.into(),
            SimpleAssignTarget::OptChain(s) => s.into(),
            SimpleAssignTarget::TsAs(a) => a.into(),
            SimpleAssignTarget::TsSatisfies(s) => s.into(),
            SimpleAssignTarget::TsNonNull(n) => n.into(),
            SimpleAssignTarget::TsTypeAssertion(a) => a.into(),
            SimpleAssignTarget::TsInstantiation(a) => a.into(),
            SimpleAssignTarget::Invalid(i) => i.into(),
            #[cfg(all(swc_ast_unknown, feature = "encoding-impl"))]
            _ => swc_common::unknown!(),
        }
    }
}

impl AssignTarget {
    pub fn as_ident(&self) -> Option<&BindingIdent> {
        self.as_simple()?.as_ident()
    }

    pub fn as_ident_mut(&mut self) -> Option<&mut BindingIdent> {
        self.as_mut_simple()?.as_mut_ident()
    }
}

impl Default for AssignTarget {
    fn default() -> Self {
        SimpleAssignTarget::dummy().into()
    }
}

impl Take for AssignTarget {
    fn dummy() -> Self {
        Default::default()
    }
}

#[ast_node("OptionalChainingExpression")]
#[derive(Eq, Hash, EqIgnoreSpan, Default)]
#[cfg_attr(feature = "arbitrary", derive(arbitrary::Arbitrary))]
#[cfg_attr(feature = "shrink-to-fit", derive(shrink_to_fit::ShrinkToFit))]
pub struct OptChainExpr {
    pub span: Span,
    pub optional: bool,
    /// This is boxed to reduce the type size of [Expr].
    pub base: Box<OptChainBase>,
}

#[ast_node]
#[derive(Eq, Hash, Is, EqIgnoreSpan)]
#[cfg_attr(feature = "arbitrary", derive(arbitrary::Arbitrary))]
#[cfg_attr(feature = "shrink-to-fit", derive(shrink_to_fit::ShrinkToFit))]
pub enum OptChainBase {
    #[tag("MemberExpression")]
    Member(MemberExpr),
    #[tag("CallExpression")]
    Call(OptCall),
}

impl Default for OptChainBase {
    fn default() -> Self {
        OptChainBase::Member(Default::default())
    }
}

#[ast_node("CallExpression")]
#[derive(Eq, Hash, EqIgnoreSpan, Default)]
#[cfg_attr(feature = "arbitrary", derive(arbitrary::Arbitrary))]
#[cfg_attr(feature = "shrink-to-fit", derive(shrink_to_fit::ShrinkToFit))]
pub struct OptCall {
    pub span: Span,

    pub ctxt: SyntaxContext,

    pub callee: Box<Expr>,

    #[cfg_attr(feature = "serde-impl", serde(default, rename = "arguments"))]
    pub args: Vec<ExprOrSpread>,

    #[cfg_attr(feature = "serde-impl", serde(default, rename = "typeArguments"))]
    #[cfg_attr(
        feature = "encoding-impl",
        encoding(with = "cbor4ii::core::types::Maybe")
    )]
    pub type_args: Option<Box<TsTypeParamInstantiation>>,
    // pub type_params: Option<TsTypeParamInstantiation>,
}

impl Take for OptChainExpr {
    fn dummy() -> Self {
        Self {
            span: DUMMY_SP,
            optional: false,
            base: Box::new(OptChainBase::Member(Take::dummy())),
        }
    }
}

impl From<OptChainBase> for Expr {
    fn from(opt: OptChainBase) -> Self {
        match opt {
            OptChainBase::Call(OptCall {
                span,
                ctxt,
                callee,
                args,
                type_args,
            }) => Self::Call(CallExpr {
                callee: Callee::Expr(callee),
                args,
                span,
                type_args,
                ctxt,
            }),
            OptChainBase::Member(member) => Self::Member(member),
            #[cfg(all(swc_ast_unknown, feature = "encoding-impl"))]
            _ => swc_common::unknown!(),
        }
    }
}

impl Take for OptCall {
    fn dummy() -> Self {
        Self {
            ..Default::default()
        }
    }
}

impl From<OptCall> for CallExpr {
    fn from(
        OptCall {
            span,
            ctxt,
            callee,
            args,
            type_args,
        }: OptCall,
    ) -> Self {
        Self {
            span,
            callee: Callee::Expr(callee),
            args,
            type_args,
            ctxt,
        }
    }
}

bridge_expr_from!(CallExpr, OptCall);

test_de!(
    jsx_element,
    JSXElement,
    r#"{
      "type": "JSXElement",
      "span": {
        "start": 0,
        "end": 5,
        "ctxt": 0
      },
      "opening": {
        "type": "JSXOpeningElement",
        "name": {
          "type": "Identifier",
          "span": {
            "start": 1,
            "end": 2,
            "ctxt": 0
          },
          "value": "a",
          "optional": false
        },
        "span": {
          "start": 1,
          "end": 5,
          "ctxt": 0
        },
        "selfClosing": true
      },
      "children": [],
      "closing": null
    }"#
);
