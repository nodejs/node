use is_macro::Is;
use swc_common::{ast_node, util::take::Take, EqIgnoreSpan, Span, SyntaxContext, DUMMY_SP};

use crate::{
    expr::Expr,
    function::{Function, ParamOrTsParamProp},
    ident::PrivateName,
    prop::PropName,
    stmt::BlockStmt,
    typescript::{
        Accessibility, TsExprWithTypeArgs, TsIndexSignature, TsTypeAnn, TsTypeParamDecl,
        TsTypeParamInstantiation,
    },
    BigInt, ComputedPropName, EmptyStmt, Id, Ident, IdentName, Number,
};

#[ast_node]
#[derive(Eq, Hash, EqIgnoreSpan, Default)]
#[cfg_attr(feature = "arbitrary", derive(arbitrary::Arbitrary))]
#[cfg_attr(feature = "shrink-to-fit", derive(shrink_to_fit::ShrinkToFit))]
pub struct Class {
    pub span: Span,

    pub ctxt: SyntaxContext,

    #[cfg_attr(feature = "serde-impl", serde(default))]
    pub decorators: Vec<Decorator>,

    #[cfg_attr(feature = "serde-impl", serde(default))]
    pub body: Vec<ClassMember>,

    #[cfg_attr(feature = "serde-impl", serde(default))]
    #[cfg_attr(
        feature = "encoding-impl",
        encoding(with = "cbor4ii::core::types::Maybe")
    )]
    pub super_class: Option<Box<Expr>>,

    #[cfg_attr(feature = "serde-impl", serde(default))]
    pub is_abstract: bool,

    #[cfg_attr(feature = "serde-impl", serde(default))]
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
    pub super_type_params: Option<Box<TsTypeParamInstantiation>>,

    /// Typescript extension.
    #[cfg_attr(feature = "serde-impl", serde(default))]
    pub implements: Vec<TsExprWithTypeArgs>,
}

impl Take for Class {
    fn dummy() -> Self {
        Class {
            ..Default::default()
        }
    }
}

#[ast_node]
#[derive(Eq, Hash, Is, EqIgnoreSpan)]
#[cfg_attr(feature = "arbitrary", derive(arbitrary::Arbitrary))]
#[cfg_attr(feature = "shrink-to-fit", derive(shrink_to_fit::ShrinkToFit))]
pub enum ClassMember {
    #[tag("Constructor")]
    Constructor(Constructor),
    /// `es2015`
    #[tag("ClassMethod")]
    Method(ClassMethod),
    #[tag("PrivateMethod")]
    PrivateMethod(PrivateMethod),
    /// stage 0 / Typescript
    #[tag("ClassProperty")]
    ClassProp(ClassProp),
    #[tag("PrivateProperty")]
    PrivateProp(PrivateProp),
    #[tag("TsIndexSignature")]
    TsIndexSignature(TsIndexSignature),
    #[tag("EmptyStatement")]
    Empty(EmptyStmt),

    /// Stage 3
    #[tag("StaticBlock")]
    StaticBlock(StaticBlock),

    /// Stage 3
    #[tag("AutoAccessor")]
    AutoAccessor(AutoAccessor),
}

impl Take for ClassMember {
    fn dummy() -> Self {
        ClassMember::Empty(EmptyStmt { span: DUMMY_SP })
    }
}

#[ast_node("ClassProperty")]
#[derive(Eq, Hash, EqIgnoreSpan, Default)]
#[cfg_attr(feature = "arbitrary", derive(arbitrary::Arbitrary))]
#[cfg_attr(feature = "shrink-to-fit", derive(shrink_to_fit::ShrinkToFit))]
pub struct ClassProp {
    #[cfg_attr(feature = "serde-impl", serde(default))]
    pub span: Span,

    pub key: PropName,

    #[cfg_attr(feature = "serde-impl", serde(default))]
    #[cfg_attr(
        feature = "encoding-impl",
        encoding(with = "cbor4ii::core::types::Maybe")
    )]
    pub value: Option<Box<Expr>>,

    #[cfg_attr(feature = "serde-impl", serde(default, rename = "typeAnnotation"))]
    #[cfg_attr(
        feature = "encoding-impl",
        encoding(with = "cbor4ii::core::types::Maybe")
    )]
    pub type_ann: Option<Box<TsTypeAnn>>,

    #[cfg_attr(feature = "serde-impl", serde(default))]
    pub is_static: bool,

    #[cfg_attr(feature = "serde-impl", serde(default))]
    pub decorators: Vec<Decorator>,

    /// Typescript extension.
    #[cfg_attr(feature = "serde-impl", serde(default))]
    #[cfg_attr(
        feature = "encoding-impl",
        encoding(with = "cbor4ii::core::types::Maybe")
    )]
    pub accessibility: Option<Accessibility>,

    /// Typescript extension.
    #[cfg_attr(feature = "serde-impl", serde(default))]
    pub is_abstract: bool,

    #[cfg_attr(feature = "serde-impl", serde(default))]
    pub is_optional: bool,

    #[cfg_attr(feature = "serde-impl", serde(default))]
    pub is_override: bool,

    #[cfg_attr(feature = "serde-impl", serde(default))]
    pub readonly: bool,

    #[cfg_attr(feature = "serde-impl", serde(default))]
    pub declare: bool,

    #[cfg_attr(feature = "serde-impl", serde(default))]
    pub definite: bool,
}

#[ast_node("PrivateProperty")]
#[derive(Eq, Hash, EqIgnoreSpan, Default)]
#[cfg_attr(feature = "arbitrary", derive(arbitrary::Arbitrary))]
#[cfg_attr(feature = "shrink-to-fit", derive(shrink_to_fit::ShrinkToFit))]
pub struct PrivateProp {
    #[cfg_attr(feature = "serde-impl", serde(default))]
    pub span: Span,

    #[cfg_attr(feature = "serde-impl", serde(default))]
    pub ctxt: SyntaxContext,

    pub key: PrivateName,

    #[cfg_attr(feature = "serde-impl", serde(default))]
    #[cfg_attr(
        feature = "encoding-impl",
        encoding(with = "cbor4ii::core::types::Maybe")
    )]
    pub value: Option<Box<Expr>>,

    #[cfg_attr(feature = "serde-impl", serde(default, rename = "typeAnnotation"))]
    #[cfg_attr(
        feature = "encoding-impl",
        encoding(with = "cbor4ii::core::types::Maybe")
    )]
    pub type_ann: Option<Box<TsTypeAnn>>,

    #[cfg_attr(feature = "serde-impl", serde(default))]
    pub is_static: bool,

    #[cfg_attr(feature = "serde-impl", serde(default))]
    pub decorators: Vec<Decorator>,

    /// Typescript extension.
    #[cfg_attr(feature = "serde-impl", serde(default))]
    #[cfg_attr(
        feature = "encoding-impl",
        encoding(with = "cbor4ii::core::types::Maybe")
    )]
    pub accessibility: Option<Accessibility>,

    #[cfg_attr(feature = "serde-impl", serde(default))]
    pub is_optional: bool,

    #[cfg_attr(feature = "serde-impl", serde(default))]
    pub is_override: bool,

    #[cfg_attr(feature = "serde-impl", serde(default))]
    pub readonly: bool,

    #[cfg_attr(feature = "serde-impl", serde(default))]
    pub definite: bool,
}

#[ast_node("ClassMethod")]
#[derive(Eq, Hash, EqIgnoreSpan, Default)]
#[cfg_attr(feature = "arbitrary", derive(arbitrary::Arbitrary))]
#[cfg_attr(feature = "shrink-to-fit", derive(shrink_to_fit::ShrinkToFit))]
pub struct ClassMethod {
    #[cfg_attr(feature = "serde-impl", serde(default))]
    pub span: Span,
    pub key: PropName,
    pub function: Box<Function>,
    pub kind: MethodKind,
    #[cfg_attr(feature = "serde-impl", serde(default))]
    pub is_static: bool,
    #[doc = r" Typescript extension."]
    #[cfg_attr(feature = "serde-impl", serde(default))]
    #[cfg_attr(
        feature = "encoding-impl",
        encoding(with = "cbor4ii::core::types::Maybe")
    )]
    pub accessibility: Option<Accessibility>,
    #[doc = r" Typescript extension."]
    #[cfg_attr(feature = "serde-impl", serde(default))]
    pub is_abstract: bool,
    #[cfg_attr(feature = "serde-impl", serde(default))]
    pub is_optional: bool,
    #[cfg_attr(feature = "serde-impl", serde(default))]
    pub is_override: bool,
}

#[ast_node("PrivateMethod")]
#[derive(Eq, Hash, EqIgnoreSpan, Default)]
#[cfg_attr(feature = "arbitrary", derive(arbitrary::Arbitrary))]
#[cfg_attr(feature = "shrink-to-fit", derive(shrink_to_fit::ShrinkToFit))]
pub struct PrivateMethod {
    #[cfg_attr(feature = "serde-impl", serde(default))]
    pub span: Span,
    pub key: PrivateName,
    pub function: Box<Function>,
    pub kind: MethodKind,
    #[cfg_attr(feature = "serde-impl", serde(default))]
    pub is_static: bool,
    #[doc = r" Typescript extension."]
    #[cfg_attr(feature = "serde-impl", serde(default))]
    #[cfg_attr(
        feature = "encoding-impl",
        encoding(with = "cbor4ii::core::types::Maybe")
    )]
    pub accessibility: Option<Accessibility>,
    #[doc = r" Typescript extension."]
    #[cfg_attr(feature = "serde-impl", serde(default))]
    pub is_abstract: bool,
    #[cfg_attr(feature = "serde-impl", serde(default))]
    pub is_optional: bool,
    #[cfg_attr(feature = "serde-impl", serde(default))]
    pub is_override: bool,
}

#[ast_node("Constructor")]
#[derive(Eq, Hash, EqIgnoreSpan, Default)]
#[cfg_attr(feature = "arbitrary", derive(arbitrary::Arbitrary))]
#[cfg_attr(feature = "shrink-to-fit", derive(shrink_to_fit::ShrinkToFit))]
pub struct Constructor {
    pub span: Span,

    pub ctxt: SyntaxContext,

    pub key: PropName,

    pub params: Vec<ParamOrTsParamProp>,

    #[cfg_attr(feature = "serde-impl", serde(default))]
    #[cfg_attr(
        feature = "encoding-impl",
        encoding(with = "cbor4ii::core::types::Maybe")
    )]
    pub body: Option<BlockStmt>,

    #[cfg_attr(feature = "serde-impl", serde(default))]
    #[cfg_attr(
        feature = "encoding-impl",
        encoding(with = "cbor4ii::core::types::Maybe")
    )]
    pub accessibility: Option<Accessibility>,

    #[cfg_attr(feature = "serde-impl", serde(default))]
    pub is_optional: bool,
}

#[ast_node("Decorator")]
#[derive(Eq, Hash, EqIgnoreSpan, Default)]
#[cfg_attr(feature = "arbitrary", derive(arbitrary::Arbitrary))]
#[cfg_attr(feature = "shrink-to-fit", derive(shrink_to_fit::ShrinkToFit))]
pub struct Decorator {
    pub span: Span,

    #[cfg_attr(feature = "serde-impl", serde(rename = "expression"))]
    pub expr: Box<Expr>,
}

#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash, EqIgnoreSpan, Default)]
#[cfg_attr(feature = "arbitrary", derive(arbitrary::Arbitrary))]
#[cfg_attr(
    any(feature = "rkyv-impl"),
    derive(rkyv::Archive, rkyv::Serialize, rkyv::Deserialize)
)]
#[cfg_attr(feature = "rkyv-impl", derive(bytecheck::CheckBytes))]
#[cfg_attr(feature = "rkyv-impl", repr(u32))]
#[cfg_attr(feature = "serde-impl", derive(serde::Serialize, serde::Deserialize))]
#[cfg_attr(feature = "shrink-to-fit", derive(shrink_to_fit::ShrinkToFit))]
#[cfg_attr(
    feature = "encoding-impl",
    derive(::swc_common::Encode, ::swc_common::Decode)
)]
#[cfg_attr(swc_ast_unknown, non_exhaustive)]
pub enum MethodKind {
    #[default]
    #[cfg_attr(feature = "serde-impl", serde(rename = "method"))]
    Method,
    #[cfg_attr(feature = "serde-impl", serde(rename = "getter"))]
    Getter,
    #[cfg_attr(feature = "serde-impl", serde(rename = "setter"))]
    Setter,
}

#[ast_node("StaticBlock")]
#[derive(Eq, Hash, EqIgnoreSpan, Default)]
#[cfg_attr(feature = "arbitrary", derive(arbitrary::Arbitrary))]
#[cfg_attr(feature = "shrink-to-fit", derive(shrink_to_fit::ShrinkToFit))]
pub struct StaticBlock {
    pub span: Span,
    pub body: BlockStmt,
}

impl Take for StaticBlock {
    fn dummy() -> Self {
        StaticBlock {
            span: DUMMY_SP,
            body: Take::dummy(),
        }
    }
}

/// Either a private name or a public name.
#[ast_node]
#[derive(Is, Eq, Hash, EqIgnoreSpan)]
#[cfg_attr(feature = "arbitrary", derive(arbitrary::Arbitrary))]
#[cfg_attr(feature = "shrink-to-fit", derive(shrink_to_fit::ShrinkToFit))]
pub enum Key {
    #[tag("PrivateName")]
    Private(PrivateName),
    #[tag("Identifier")]
    #[tag("StringLiteral")]
    #[tag("NumericLiteral")]
    #[tag("Computed")]
    #[tag("BigIntLiteral")]
    Public(PropName),
}

bridge_from!(Key, IdentName, Ident);
bridge_from!(Key, PropName, IdentName);
bridge_from!(Key, PropName, Id);
bridge_from!(Key, PropName, Number);
bridge_from!(Key, PropName, ComputedPropName);
bridge_from!(Key, PropName, BigInt);

impl Take for Key {
    fn dummy() -> Self {
        Default::default()
    }
}

impl Default for Key {
    fn default() -> Self {
        Key::Public(Default::default())
    }
}

#[ast_node("AutoAccessor")]
#[derive(Eq, Hash, EqIgnoreSpan, Default)]
#[cfg_attr(feature = "arbitrary", derive(arbitrary::Arbitrary))]
#[cfg_attr(feature = "shrink-to-fit", derive(shrink_to_fit::ShrinkToFit))]
pub struct AutoAccessor {
    #[cfg_attr(feature = "serde-impl", serde(default))]
    pub span: Span,

    pub key: Key,

    #[cfg_attr(feature = "serde-impl", serde(default))]
    #[cfg_attr(
        feature = "encoding-impl",
        encoding(with = "cbor4ii::core::types::Maybe")
    )]
    pub value: Option<Box<Expr>>,

    #[cfg_attr(feature = "serde-impl", serde(default, rename = "typeAnnotation"))]
    #[cfg_attr(
        feature = "encoding-impl",
        encoding(with = "cbor4ii::core::types::Maybe")
    )]
    pub type_ann: Option<Box<TsTypeAnn>>,

    #[cfg_attr(feature = "serde-impl", serde(default))]
    pub is_static: bool,

    #[cfg_attr(feature = "serde-impl", serde(default))]
    pub decorators: Vec<Decorator>,

    /// Typescript extension.
    #[cfg_attr(feature = "serde-impl", serde(default))]
    #[cfg_attr(
        feature = "encoding-impl",
        encoding(with = "cbor4ii::core::types::Maybe")
    )]
    pub accessibility: Option<Accessibility>,

    #[cfg_attr(feature = "serde-impl", serde(default))]
    pub is_abstract: bool,

    #[cfg_attr(feature = "serde-impl", serde(default))]
    pub is_override: bool,

    #[cfg_attr(feature = "serde-impl", serde(default))]
    pub definite: bool,
}

impl Take for AutoAccessor {
    fn dummy() -> AutoAccessor {
        AutoAccessor {
            span: Take::dummy(),
            key: Take::dummy(),
            value: Take::dummy(),
            type_ann: None,
            is_static: false,
            decorators: Take::dummy(),
            accessibility: None,
            is_abstract: false,
            is_override: false,
            definite: false,
        }
    }
}
