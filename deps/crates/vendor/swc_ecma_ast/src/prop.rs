use is_macro::Is;
use swc_common::{ast_node, util::take::Take, EqIgnoreSpan, Span, DUMMY_SP};

use crate::{
    expr::Expr,
    function::Function,
    ident::Ident,
    lit::{BigInt, Number, Str},
    stmt::BlockStmt,
    typescript::TsTypeAnn,
    Id, IdentName, MemberProp, Pat,
};

#[ast_node]
#[derive(Eq, Hash, Is, EqIgnoreSpan)]
#[cfg_attr(feature = "arbitrary", derive(arbitrary::Arbitrary))]
#[cfg_attr(feature = "shrink-to-fit", derive(shrink_to_fit::ShrinkToFit))]
pub enum Prop {
    /// `a` in `{ a, }`
    #[tag("Identifier")]
    Shorthand(Ident),

    /// `key: value` in `{ key: value, }`
    #[tag("KeyValueProperty")]
    KeyValue(KeyValueProp),

    /// This is **invalid** for object literal.
    #[tag("AssignmentProperty")]
    Assign(AssignProp),

    #[tag("GetterProperty")]
    Getter(GetterProp),

    #[tag("SetterProperty")]
    Setter(SetterProp),

    #[tag("MethodProperty")]
    Method(MethodProp),
}

bridge_from!(Prop, Ident, IdentName);

#[ast_node("KeyValueProperty")]
#[derive(Eq, Hash, EqIgnoreSpan)]
#[cfg_attr(feature = "arbitrary", derive(arbitrary::Arbitrary))]
#[cfg_attr(feature = "shrink-to-fit", derive(shrink_to_fit::ShrinkToFit))]
pub struct KeyValueProp {
    #[span(lo)]
    pub key: PropName,

    #[span(hi)]
    pub value: Box<Expr>,
}

#[ast_node("AssignmentProperty")]
#[derive(Eq, Hash, EqIgnoreSpan)]
#[cfg_attr(feature = "arbitrary", derive(arbitrary::Arbitrary))]
#[cfg_attr(feature = "shrink-to-fit", derive(shrink_to_fit::ShrinkToFit))]
pub struct AssignProp {
    pub span: Span,
    pub key: Ident,
    pub value: Box<Expr>,
}

#[ast_node("GetterProperty")]
#[derive(Eq, Hash, EqIgnoreSpan, Default)]
#[cfg_attr(feature = "arbitrary", derive(arbitrary::Arbitrary))]
#[cfg_attr(feature = "shrink-to-fit", derive(shrink_to_fit::ShrinkToFit))]
pub struct GetterProp {
    pub span: Span,
    pub key: PropName,
    #[cfg_attr(feature = "serde-impl", serde(default, rename = "typeAnnotation"))]
    #[cfg_attr(
        feature = "encoding-impl",
        encoding(with = "cbor4ii::core::types::Maybe")
    )]
    pub type_ann: Option<Box<TsTypeAnn>>,
    #[cfg_attr(feature = "serde-impl", serde(default))]
    #[cfg_attr(
        feature = "encoding-impl",
        encoding(with = "cbor4ii::core::types::Maybe")
    )]
    pub body: Option<BlockStmt>,
}
#[ast_node("SetterProperty")]
#[derive(Eq, Hash, EqIgnoreSpan, Default)]
#[cfg_attr(feature = "arbitrary", derive(arbitrary::Arbitrary))]
#[cfg_attr(feature = "shrink-to-fit", derive(shrink_to_fit::ShrinkToFit))]
pub struct SetterProp {
    pub span: Span,
    pub key: PropName,
    #[cfg_attr(
        feature = "encoding-impl",
        encoding(with = "cbor4ii::core::types::Maybe")
    )]
    pub this_param: Option<Pat>,
    pub param: Box<Pat>,
    #[cfg_attr(feature = "serde-impl", serde(default))]
    #[cfg_attr(
        feature = "encoding-impl",
        encoding(with = "cbor4ii::core::types::Maybe")
    )]
    pub body: Option<BlockStmt>,
}
#[ast_node("MethodProperty")]
#[derive(Eq, Hash, EqIgnoreSpan)]
#[cfg_attr(feature = "arbitrary", derive(arbitrary::Arbitrary))]
#[cfg_attr(feature = "shrink-to-fit", derive(shrink_to_fit::ShrinkToFit))]
pub struct MethodProp {
    pub key: PropName,

    #[cfg_attr(feature = "serde-impl", serde(flatten))]
    #[span]
    pub function: Box<Function>,
}

#[ast_node]
#[derive(Eq, Hash, Is, EqIgnoreSpan)]
#[cfg_attr(feature = "arbitrary", derive(arbitrary::Arbitrary))]
#[cfg_attr(feature = "shrink-to-fit", derive(shrink_to_fit::ShrinkToFit))]
pub enum PropName {
    #[tag("Identifier")]
    Ident(IdentName),
    /// String literal.
    #[tag("StringLiteral")]
    Str(Str),
    /// Numeric literal.
    #[tag("NumericLiteral")]
    Num(Number),
    #[tag("Computed")]
    Computed(ComputedPropName),
    #[tag("BigIntLiteral")]
    BigInt(BigInt),
}

bridge_from!(PropName, IdentName, Ident);
bridge_from!(PropName, Ident, Id);

impl Default for PropName {
    fn default() -> Self {
        PropName::Ident(Default::default())
    }
}

impl Take for PropName {
    fn dummy() -> Self {
        PropName::Ident(Take::dummy())
    }
}

impl From<PropName> for MemberProp {
    fn from(p: PropName) -> Self {
        match p {
            PropName::Ident(p) => MemberProp::Ident(p),
            PropName::Computed(p) => MemberProp::Computed(p),
            PropName::Str(p) => MemberProp::Computed(ComputedPropName {
                span: DUMMY_SP,
                expr: p.into(),
            }),
            PropName::Num(p) => MemberProp::Computed(ComputedPropName {
                span: DUMMY_SP,
                expr: p.into(),
            }),
            PropName::BigInt(p) => MemberProp::Computed(ComputedPropName {
                span: DUMMY_SP,
                expr: p.into(),
            }),
            #[cfg(all(swc_ast_unknown, feature = "encoding-impl"))]
            _ => swc_common::unknown!(),
        }
    }
}

#[ast_node("Computed")]
#[derive(Eq, Hash, EqIgnoreSpan)]
#[cfg_attr(feature = "arbitrary", derive(arbitrary::Arbitrary))]
#[cfg_attr(feature = "shrink-to-fit", derive(shrink_to_fit::ShrinkToFit))]
pub struct ComputedPropName {
    /// Span including `[` and `]`.
    pub span: Span,
    #[cfg_attr(feature = "serde-impl", serde(rename = "expression"))]
    pub expr: Box<Expr>,
}

impl Take for ComputedPropName {
    fn dummy() -> Self {
        Self {
            span: DUMMY_SP,
            expr: Take::dummy(),
        }
    }
}
