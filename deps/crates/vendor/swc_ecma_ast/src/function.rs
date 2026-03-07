use is_macro::Is;
use swc_common::{ast_node, util::take::Take, EqIgnoreSpan, Span, SyntaxContext, DUMMY_SP};

use crate::{
    class::Decorator,
    pat::Pat,
    stmt::BlockStmt,
    typescript::{TsParamProp, TsTypeAnn, TsTypeParamDecl},
};

/// Common parts of function and method.
#[ast_node]
#[derive(Eq, Hash, EqIgnoreSpan, Default)]
#[cfg_attr(feature = "arbitrary", derive(arbitrary::Arbitrary))]
#[cfg_attr(feature = "shrink-to-fit", derive(shrink_to_fit::ShrinkToFit))]
pub struct Function {
    pub params: Vec<Param>,

    #[cfg_attr(feature = "serde-impl", serde(default))]
    pub decorators: Vec<Decorator>,

    pub span: Span,

    pub ctxt: SyntaxContext,

    #[cfg_attr(feature = "serde-impl", serde(default))]
    #[cfg_attr(
        feature = "encoding-impl",
        encoding(with = "cbor4ii::core::types::Maybe")
    )]
    pub body: Option<BlockStmt>,

    /// if it's a generator.
    #[cfg_attr(feature = "serde-impl", serde(default, rename = "generator"))]
    pub is_generator: bool,

    /// if it's an async function.
    #[cfg_attr(feature = "serde-impl", serde(default, rename = "async"))]
    pub is_async: bool,

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

impl Take for Function {
    fn dummy() -> Self {
        Function {
            ..Default::default()
        }
    }
}

#[ast_node("Parameter")]
#[derive(Eq, Hash, EqIgnoreSpan)]
#[cfg_attr(feature = "arbitrary", derive(arbitrary::Arbitrary))]
#[cfg_attr(feature = "shrink-to-fit", derive(shrink_to_fit::ShrinkToFit))]
pub struct Param {
    pub span: Span,
    #[cfg_attr(feature = "serde-impl", serde(default))]
    pub decorators: Vec<Decorator>,
    pub pat: Pat,
}

impl From<Pat> for Param {
    fn from(pat: Pat) -> Self {
        Self {
            span: DUMMY_SP,
            decorators: Default::default(),
            pat,
        }
    }
}

#[ast_node]
#[derive(Eq, Hash, Is, EqIgnoreSpan)]
#[cfg_attr(feature = "arbitrary", derive(arbitrary::Arbitrary))]
#[cfg_attr(feature = "shrink-to-fit", derive(shrink_to_fit::ShrinkToFit))]
pub enum ParamOrTsParamProp {
    #[tag("TsParameterProperty")]
    TsParamProp(TsParamProp),
    #[tag("Parameter")]
    Param(Param),
}
