use is_macro::Is;
use swc_common::{ast_node, util::take::Take, EqIgnoreSpan, Span, DUMMY_SP};

use crate::{
    expr::Expr,
    ident::{BindingIdent, Ident},
    prop::PropName,
    typescript::TsTypeAnn,
    Id, IdentName, Invalid,
};

#[ast_node(no_clone)]
#[derive(Eq, Hash, Is, EqIgnoreSpan)]
#[cfg_attr(feature = "arbitrary", derive(arbitrary::Arbitrary))]
#[cfg_attr(feature = "shrink-to-fit", derive(shrink_to_fit::ShrinkToFit))]
pub enum Pat {
    #[tag("Identifier")]
    Ident(BindingIdent),

    #[tag("ArrayPattern")]
    Array(ArrayPat),

    #[tag("RestElement")]
    Rest(RestPat),

    #[tag("ObjectPattern")]
    Object(ObjectPat),

    #[tag("AssignmentPattern")]
    Assign(AssignPat),

    #[tag("Invalid")]
    Invalid(Invalid),

    /// Only for for-in / for-of loops. This is *syntactically* valid.
    #[tag("*")]
    Expr(Box<Expr>),
}

// Implement Clone without inline to avoid multiple copies of the
// implementation.
impl Clone for Pat {
    fn clone(&self) -> Self {
        use Pat::*;
        match self {
            #[cfg(all(swc_ast_unknown, feature = "encoding-impl"))]
            Unknown(tag, v) => Unknown(*tag, v.clone()),
            Ident(p) => Ident(p.clone()),
            Array(p) => Array(p.clone()),
            Rest(p) => Rest(p.clone()),
            Object(p) => Object(p.clone()),
            Assign(p) => Assign(p.clone()),
            Invalid(p) => Invalid(p.clone()),
            Expr(p) => Expr(p.clone()),
        }
    }
}

impl Default for Pat {
    fn default() -> Self {
        Invalid { span: DUMMY_SP }.into()
    }
}
impl Take for Pat {
    fn dummy() -> Self {
        Default::default()
    }
}

bridge_pat_from!(BindingIdent, Ident);
bridge_pat_from!(BindingIdent, IdentName);
bridge_pat_from!(BindingIdent, Id);

macro_rules! pat_to_other {
    ($T:ty) => {
        bridge_from!(crate::Param, crate::Pat, $T);
        bridge_from!(Box<crate::Pat>, crate::Pat, $T);
    };
}

pat_to_other!(BindingIdent);
pat_to_other!(ArrayPat);
pat_to_other!(ObjectPat);
pat_to_other!(AssignPat);
pat_to_other!(RestPat);
pat_to_other!(Box<Expr>);

#[ast_node("ArrayPattern")]
#[derive(Eq, Hash, EqIgnoreSpan)]
#[cfg_attr(feature = "arbitrary", derive(arbitrary::Arbitrary))]
#[cfg_attr(feature = "shrink-to-fit", derive(shrink_to_fit::ShrinkToFit))]
pub struct ArrayPat {
    pub span: Span,

    #[cfg_attr(feature = "serde-impl", serde(rename = "elements"))]
    #[cfg_attr(
        feature = "encoding-impl",
        encoding(with = "swc_common::serializer::ArrayOption")
    )]
    pub elems: Vec<Option<Pat>>,

    /// Only in an ambient context
    #[cfg_attr(feature = "serde-impl", serde(rename = "optional"))]
    pub optional: bool,

    #[cfg_attr(feature = "serde-impl", serde(default, rename = "typeAnnotation"))]
    #[cfg_attr(
        feature = "encoding-impl",
        encoding(with = "cbor4ii::core::types::Maybe")
    )]
    pub type_ann: Option<Box<TsTypeAnn>>,
}

#[ast_node("ObjectPattern")]
#[derive(Eq, Hash, EqIgnoreSpan)]
#[cfg_attr(feature = "arbitrary", derive(arbitrary::Arbitrary))]
#[cfg_attr(feature = "shrink-to-fit", derive(shrink_to_fit::ShrinkToFit))]
pub struct ObjectPat {
    pub span: Span,

    #[cfg_attr(feature = "serde-impl", serde(rename = "properties"))]
    pub props: Vec<ObjectPatProp>,

    /// Only in an ambient context
    #[cfg_attr(feature = "serde-impl", serde(rename = "optional"))]
    pub optional: bool,

    #[cfg_attr(feature = "serde-impl", serde(default, rename = "typeAnnotation"))]
    #[cfg_attr(
        feature = "encoding-impl",
        encoding(with = "cbor4ii::core::types::Maybe")
    )]
    pub type_ann: Option<Box<TsTypeAnn>>,
}

#[ast_node("AssignmentPattern")]
#[derive(Eq, Hash, EqIgnoreSpan)]
#[cfg_attr(feature = "arbitrary", derive(arbitrary::Arbitrary))]
#[cfg_attr(feature = "shrink-to-fit", derive(shrink_to_fit::ShrinkToFit))]
pub struct AssignPat {
    pub span: Span,

    pub left: Box<Pat>,

    pub right: Box<Expr>,
}

/// EsTree `RestElement`
#[ast_node("RestElement")]
#[derive(Eq, Hash, EqIgnoreSpan)]
#[cfg_attr(feature = "arbitrary", derive(arbitrary::Arbitrary))]
#[cfg_attr(feature = "shrink-to-fit", derive(shrink_to_fit::ShrinkToFit))]
pub struct RestPat {
    pub span: Span,

    #[cfg_attr(feature = "serde-impl", serde(rename = "rest"))]
    pub dot3_token: Span,

    #[cfg_attr(feature = "serde-impl", serde(rename = "argument"))]
    pub arg: Box<Pat>,

    #[cfg_attr(feature = "serde-impl", serde(default, rename = "typeAnnotation"))]
    #[cfg_attr(
        feature = "encoding-impl",
        encoding(with = "cbor4ii::core::types::Maybe")
    )]
    pub type_ann: Option<Box<TsTypeAnn>>,
}

#[ast_node]
#[derive(Eq, Hash, Is, EqIgnoreSpan)]
#[cfg_attr(feature = "arbitrary", derive(arbitrary::Arbitrary))]
#[cfg_attr(feature = "shrink-to-fit", derive(shrink_to_fit::ShrinkToFit))]
pub enum ObjectPatProp {
    #[tag("KeyValuePatternProperty")]
    KeyValue(KeyValuePatProp),

    #[tag("AssignmentPatternProperty")]
    Assign(AssignPatProp),

    #[tag("RestElement")]
    Rest(RestPat),
}

/// `{key: value}`
#[ast_node("KeyValuePatternProperty")]
#[derive(Eq, Hash, EqIgnoreSpan)]
#[cfg_attr(feature = "arbitrary", derive(arbitrary::Arbitrary))]
#[cfg_attr(feature = "shrink-to-fit", derive(shrink_to_fit::ShrinkToFit))]
pub struct KeyValuePatProp {
    #[span(lo)]
    pub key: PropName,

    #[span(hi)]
    pub value: Box<Pat>,
}
/// `{key}` or `{key = value}`
#[ast_node("AssignmentPatternProperty")]
#[derive(Eq, Hash, EqIgnoreSpan)]
#[cfg_attr(feature = "arbitrary", derive(arbitrary::Arbitrary))]
#[cfg_attr(feature = "shrink-to-fit", derive(shrink_to_fit::ShrinkToFit))]
pub struct AssignPatProp {
    pub span: Span,
    /// Note: This type is to help implementing visitor and the field `type_ann`
    /// is always [None].
    pub key: BindingIdent,

    #[cfg_attr(feature = "serde-impl", serde(default))]
    #[cfg_attr(
        feature = "encoding-impl",
        encoding(with = "cbor4ii::core::types::Maybe")
    )]
    pub value: Option<Box<Expr>>,
}
