#![allow(clippy::vec_box)]
#![allow(missing_copy_implementations)]

#[cfg(feature = "serde-impl")]
use std::fmt;

use is_macro::Is;
#[cfg(feature = "serde-impl")]
use serde::{
    de::{self, Unexpected, Visitor},
    Deserialize, Deserializer, Serialize,
};
use string_enum::StringEnum;
use swc_common::{ast_node, EqIgnoreSpan, Span};

use crate::{
    class::Decorator,
    expr::{Expr, ObjectLit},
    ident::Ident,
    lit::{Bool, Number, Str},
    module::ModuleItem,
    pat::{ArrayPat, AssignPat, ObjectPat, Pat, RestPat},
    BigInt, BindingIdent, IdentName, TplElement,
};

#[ast_node("TsTypeAnnotation")]
#[derive(Eq, Hash, EqIgnoreSpan)]
#[cfg_attr(feature = "arbitrary", derive(arbitrary::Arbitrary))]
#[cfg_attr(feature = "shrink-to-fit", derive(shrink_to_fit::ShrinkToFit))]
pub struct TsTypeAnn {
    pub span: Span,
    #[cfg_attr(feature = "serde-impl", serde(rename = "typeAnnotation"))]
    pub type_ann: Box<TsType>,
}

#[ast_node("TsTypeParameterDeclaration")]
#[derive(Eq, Hash, EqIgnoreSpan)]
#[cfg_attr(feature = "arbitrary", derive(arbitrary::Arbitrary))]
#[cfg_attr(feature = "shrink-to-fit", derive(shrink_to_fit::ShrinkToFit))]
pub struct TsTypeParamDecl {
    pub span: Span,
    #[cfg_attr(feature = "serde-impl", serde(rename = "parameters"))]
    pub params: Vec<TsTypeParam>,
}

#[ast_node("TsTypeParameter")]
#[derive(Eq, Hash, EqIgnoreSpan)]
#[cfg_attr(feature = "arbitrary", derive(arbitrary::Arbitrary))]
#[cfg_attr(feature = "shrink-to-fit", derive(shrink_to_fit::ShrinkToFit))]
pub struct TsTypeParam {
    pub span: Span,
    pub name: Ident,

    #[cfg_attr(feature = "serde-impl", serde(default, rename = "in"))]
    pub is_in: bool,

    #[cfg_attr(feature = "serde-impl", serde(default, rename = "out"))]
    pub is_out: bool,

    #[cfg_attr(feature = "serde-impl", serde(default, rename = "const"))]
    pub is_const: bool,

    #[cfg_attr(feature = "serde-impl", serde(default))]
    #[cfg_attr(
        feature = "encoding-impl",
        encoding(with = "cbor4ii::core::types::Maybe")
    )]
    pub constraint: Option<Box<TsType>>,

    #[cfg_attr(feature = "serde-impl", serde(default))]
    #[cfg_attr(
        feature = "encoding-impl",
        encoding(with = "cbor4ii::core::types::Maybe")
    )]
    pub default: Option<Box<TsType>>,
}

#[ast_node("TsTypeParameterInstantiation")]
#[derive(Eq, Hash, EqIgnoreSpan)]
#[cfg_attr(feature = "arbitrary", derive(arbitrary::Arbitrary))]
#[cfg_attr(feature = "shrink-to-fit", derive(shrink_to_fit::ShrinkToFit))]
pub struct TsTypeParamInstantiation {
    pub span: Span,
    pub params: Vec<Box<TsType>>,
}

#[ast_node("TsParameterProperty")]
#[derive(Eq, Hash, EqIgnoreSpan)]
#[cfg_attr(feature = "arbitrary", derive(arbitrary::Arbitrary))]
#[cfg_attr(feature = "shrink-to-fit", derive(shrink_to_fit::ShrinkToFit))]
pub struct TsParamProp {
    pub span: Span,
    #[cfg_attr(feature = "serde-impl", serde(default))]
    pub decorators: Vec<Decorator>,
    /// At least one of `accessibility` or `readonly` must be set.
    #[cfg_attr(feature = "serde-impl", serde(default))]
    #[cfg_attr(
        feature = "encoding-impl",
        encoding(with = "cbor4ii::core::types::Maybe")
    )]
    pub accessibility: Option<Accessibility>,
    #[cfg_attr(feature = "serde-impl", serde(rename = "override"))]
    pub is_override: bool,
    pub readonly: bool,
    pub param: TsParamPropParam,
}

#[ast_node]
#[derive(Eq, Hash, Is, EqIgnoreSpan)]
#[cfg_attr(feature = "arbitrary", derive(arbitrary::Arbitrary))]
#[cfg_attr(feature = "shrink-to-fit", derive(shrink_to_fit::ShrinkToFit))]
pub enum TsParamPropParam {
    #[tag("Identifier")]
    Ident(BindingIdent),

    #[tag("AssignmentPattern")]
    Assign(AssignPat),
}

#[ast_node("TsQualifiedName")]
#[derive(Eq, Hash, EqIgnoreSpan)]
#[cfg_attr(feature = "arbitrary", derive(arbitrary::Arbitrary))]
#[cfg_attr(feature = "shrink-to-fit", derive(shrink_to_fit::ShrinkToFit))]
pub struct TsQualifiedName {
    pub span: Span,
    pub left: TsEntityName,
    pub right: IdentName,
}

#[ast_node]
#[derive(Eq, Hash, Is, EqIgnoreSpan)]
#[allow(variant_size_differences)]
#[cfg_attr(feature = "arbitrary", derive(arbitrary::Arbitrary))]
#[cfg_attr(feature = "shrink-to-fit", derive(shrink_to_fit::ShrinkToFit))]
pub enum TsEntityName {
    #[tag("TsQualifiedName")]
    TsQualifiedName(Box<TsQualifiedName>),

    #[tag("Identifier")]
    Ident(Ident),
}

// ================
// TypeScript type members (for type literal / interface / class)
// ================

#[ast_node]
#[derive(Eq, Hash, Is, EqIgnoreSpan)]
#[cfg_attr(feature = "arbitrary", derive(arbitrary::Arbitrary))]
#[cfg_attr(feature = "shrink-to-fit", derive(shrink_to_fit::ShrinkToFit))]
pub enum TsTypeElement {
    #[tag("TsCallSignatureDeclaration")]
    TsCallSignatureDecl(TsCallSignatureDecl),

    #[tag("TsConstructSignatureDeclaration")]
    TsConstructSignatureDecl(TsConstructSignatureDecl),

    #[tag("TsPropertySignature")]
    TsPropertySignature(TsPropertySignature),

    #[tag("TsGetterSignature")]
    TsGetterSignature(TsGetterSignature),

    #[tag("TsSetterSignature")]
    TsSetterSignature(TsSetterSignature),

    #[tag("TsMethodSignature")]
    TsMethodSignature(TsMethodSignature),

    #[tag("TsIndexSignature")]
    TsIndexSignature(TsIndexSignature),
}

#[ast_node("TsCallSignatureDeclaration")]
#[derive(Eq, Hash, EqIgnoreSpan)]
#[cfg_attr(feature = "arbitrary", derive(arbitrary::Arbitrary))]
#[cfg_attr(feature = "shrink-to-fit", derive(shrink_to_fit::ShrinkToFit))]
pub struct TsCallSignatureDecl {
    pub span: Span,
    pub params: Vec<TsFnParam>,
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
    pub type_params: Option<Box<TsTypeParamDecl>>,
}

#[ast_node("TsConstructSignatureDeclaration")]
#[derive(Eq, Hash, EqIgnoreSpan)]
#[cfg_attr(feature = "arbitrary", derive(arbitrary::Arbitrary))]
#[cfg_attr(feature = "shrink-to-fit", derive(shrink_to_fit::ShrinkToFit))]
pub struct TsConstructSignatureDecl {
    pub span: Span,
    pub params: Vec<TsFnParam>,
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
    pub type_params: Option<Box<TsTypeParamDecl>>,
}

#[ast_node("TsPropertySignature")]
#[derive(Eq, Hash, EqIgnoreSpan)]
#[cfg_attr(feature = "arbitrary", derive(arbitrary::Arbitrary))]
#[cfg_attr(feature = "shrink-to-fit", derive(shrink_to_fit::ShrinkToFit))]
pub struct TsPropertySignature {
    pub span: Span,
    pub readonly: bool,
    pub key: Box<Expr>,
    pub computed: bool,
    pub optional: bool,
    #[cfg_attr(feature = "serde-impl", serde(default, rename = "typeAnnotation"))]
    #[cfg_attr(
        feature = "encoding-impl",
        encoding(with = "cbor4ii::core::types::Maybe")
    )]
    pub type_ann: Option<Box<TsTypeAnn>>,
}

#[ast_node("TsGetterSignature")]
#[derive(Eq, Hash, EqIgnoreSpan)]
#[cfg_attr(feature = "arbitrary", derive(arbitrary::Arbitrary))]
#[cfg_attr(feature = "shrink-to-fit", derive(shrink_to_fit::ShrinkToFit))]
pub struct TsGetterSignature {
    pub span: Span,
    pub key: Box<Expr>,
    pub computed: bool,
    #[cfg_attr(feature = "serde-impl", serde(default, rename = "typeAnnotation"))]
    #[cfg_attr(
        feature = "encoding-impl",
        encoding(with = "cbor4ii::core::types::Maybe")
    )]
    pub type_ann: Option<Box<TsTypeAnn>>,
}

#[ast_node("TsSetterSignature")]
#[derive(Eq, Hash, EqIgnoreSpan)]
#[cfg_attr(feature = "arbitrary", derive(arbitrary::Arbitrary))]
#[cfg_attr(feature = "shrink-to-fit", derive(shrink_to_fit::ShrinkToFit))]
pub struct TsSetterSignature {
    pub span: Span,
    pub key: Box<Expr>,
    pub computed: bool,
    pub param: TsFnParam,
}

#[ast_node("TsMethodSignature")]
#[derive(Eq, Hash, EqIgnoreSpan)]
#[cfg_attr(feature = "arbitrary", derive(arbitrary::Arbitrary))]
#[cfg_attr(feature = "shrink-to-fit", derive(shrink_to_fit::ShrinkToFit))]
pub struct TsMethodSignature {
    pub span: Span,
    pub key: Box<Expr>,
    pub computed: bool,
    pub optional: bool,
    pub params: Vec<TsFnParam>,
    #[cfg_attr(feature = "serde-impl", serde(default))]
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
    pub type_params: Option<Box<TsTypeParamDecl>>,
}

#[ast_node("TsIndexSignature")]
#[derive(Eq, Hash, EqIgnoreSpan)]
#[cfg_attr(feature = "arbitrary", derive(arbitrary::Arbitrary))]
#[cfg_attr(feature = "shrink-to-fit", derive(shrink_to_fit::ShrinkToFit))]
pub struct TsIndexSignature {
    pub params: Vec<TsFnParam>,
    #[cfg_attr(feature = "serde-impl", serde(default, rename = "typeAnnotation"))]
    #[cfg_attr(
        feature = "encoding-impl",
        encoding(with = "cbor4ii::core::types::Maybe")
    )]
    pub type_ann: Option<Box<TsTypeAnn>>,

    pub readonly: bool,
    #[cfg_attr(feature = "serde-impl", serde(rename = "static"))]
    pub is_static: bool,
    pub span: Span,
}

// ================
// TypeScript types
// ================

#[ast_node(no_clone)]
#[derive(Eq, Hash, Is, EqIgnoreSpan)]
#[cfg_attr(feature = "arbitrary", derive(arbitrary::Arbitrary))]
#[cfg_attr(feature = "shrink-to-fit", derive(shrink_to_fit::ShrinkToFit))]
pub enum TsType {
    #[tag("TsKeywordType")]
    TsKeywordType(TsKeywordType),

    #[tag("TsThisType")]
    TsThisType(TsThisType),

    #[tag("TsFunctionType")]
    #[tag("TsConstructorType")]
    TsFnOrConstructorType(TsFnOrConstructorType),

    #[tag("TsTypeReference")]
    TsTypeRef(TsTypeRef),

    #[tag("TsTypeQuery")]
    TsTypeQuery(TsTypeQuery),

    #[tag("TsTypeLiteral")]
    TsTypeLit(TsTypeLit),

    #[tag("TsArrayType")]
    TsArrayType(TsArrayType),

    #[tag("TsTupleType")]
    TsTupleType(TsTupleType),

    #[tag("TsOptionalType")]
    TsOptionalType(TsOptionalType),

    #[tag("TsRestType")]
    TsRestType(TsRestType),

    #[tag("TsUnionType")]
    #[tag("TsIntersectionType")]
    TsUnionOrIntersectionType(TsUnionOrIntersectionType),

    #[tag("TsConditionalType")]
    TsConditionalType(TsConditionalType),

    #[tag("TsInferType")]
    TsInferType(TsInferType),

    #[tag("TsParenthesizedType")]
    TsParenthesizedType(TsParenthesizedType),

    #[tag("TsTypeOperator")]
    TsTypeOperator(TsTypeOperator),

    #[tag("TsIndexedAccessType")]
    TsIndexedAccessType(TsIndexedAccessType),

    #[tag("TsMappedType")]
    TsMappedType(TsMappedType),

    #[tag("TsLiteralType")]
    TsLitType(TsLitType),

    #[tag("TsTypePredicate")]
    TsTypePredicate(TsTypePredicate),

    #[tag("TsImportType")]
    TsImportType(TsImportType),
}

// Implement Clone without inline to avoid multiple copies of the
// implementation.
impl Clone for TsType {
    fn clone(&self) -> Self {
        use TsType::*;
        match self {
            #[cfg(all(swc_ast_unknown, feature = "encoding-impl"))]
            Unknown(tag, v) => Unknown(*tag, v.clone()),
            TsKeywordType(t) => TsKeywordType(t.clone()),
            TsThisType(t) => TsThisType(t.clone()),
            TsFnOrConstructorType(t) => TsFnOrConstructorType(t.clone()),
            TsTypeRef(t) => TsTypeRef(t.clone()),
            TsTypeQuery(t) => TsTypeQuery(t.clone()),
            TsTypeLit(t) => TsTypeLit(t.clone()),
            TsArrayType(t) => TsArrayType(t.clone()),
            TsTupleType(t) => TsTupleType(t.clone()),
            TsOptionalType(t) => TsOptionalType(t.clone()),
            TsRestType(t) => TsRestType(t.clone()),
            TsUnionOrIntersectionType(t) => TsUnionOrIntersectionType(t.clone()),
            TsConditionalType(t) => TsConditionalType(t.clone()),
            TsInferType(t) => TsInferType(t.clone()),
            TsParenthesizedType(t) => TsParenthesizedType(t.clone()),
            TsTypeOperator(t) => TsTypeOperator(t.clone()),
            TsIndexedAccessType(t) => TsIndexedAccessType(t.clone()),
            TsMappedType(t) => TsMappedType(t.clone()),
            TsLitType(t) => TsLitType(t.clone()),
            TsTypePredicate(t) => TsTypePredicate(t.clone()),
            TsImportType(t) => TsImportType(t.clone()),
        }
    }
}

#[ast_node]
#[derive(Eq, Hash, Is, EqIgnoreSpan)]
#[cfg_attr(feature = "arbitrary", derive(arbitrary::Arbitrary))]
#[cfg_attr(feature = "shrink-to-fit", derive(shrink_to_fit::ShrinkToFit))]
pub enum TsFnOrConstructorType {
    #[tag("TsFunctionType")]
    TsFnType(TsFnType),
    #[tag("TsConstructorType")]
    TsConstructorType(TsConstructorType),
}

impl From<TsFnType> for TsType {
    fn from(t: TsFnType) -> Self {
        TsFnOrConstructorType::TsFnType(t).into()
    }
}

impl From<TsConstructorType> for TsType {
    fn from(t: TsConstructorType) -> Self {
        TsFnOrConstructorType::TsConstructorType(t).into()
    }
}

impl From<TsUnionType> for TsType {
    fn from(t: TsUnionType) -> Self {
        TsUnionOrIntersectionType::TsUnionType(t).into()
    }
}

impl From<TsIntersectionType> for TsType {
    fn from(t: TsIntersectionType) -> Self {
        TsUnionOrIntersectionType::TsIntersectionType(t).into()
    }
}

#[ast_node("TsKeywordType")]
#[derive(Eq, Hash, EqIgnoreSpan)]
#[cfg_attr(feature = "arbitrary", derive(arbitrary::Arbitrary))]
#[cfg_attr(feature = "shrink-to-fit", derive(shrink_to_fit::ShrinkToFit))]
pub struct TsKeywordType {
    pub span: Span,
    pub kind: TsKeywordTypeKind,
}

#[derive(Debug, Copy, Clone, PartialEq, Eq, Hash, EqIgnoreSpan)]
#[cfg_attr(feature = "arbitrary", derive(arbitrary::Arbitrary))]
#[cfg_attr(feature = "shrink-to-fit", derive(shrink_to_fit::ShrinkToFit))]
#[cfg_attr(
    any(feature = "rkyv-impl"),
    derive(rkyv::Archive, rkyv::Serialize, rkyv::Deserialize)
)]
#[cfg_attr(feature = "rkyv-impl", derive(bytecheck::CheckBytes))]
#[cfg_attr(feature = "rkyv-impl", repr(u32))]
#[cfg_attr(feature = "serde-impl", derive(serde::Serialize, serde::Deserialize))]
#[cfg_attr(
    feature = "encoding-impl",
    derive(::swc_common::Encode, ::swc_common::Decode)
)]
#[cfg_attr(swc_ast_unknown, non_exhaustive)]
pub enum TsKeywordTypeKind {
    #[cfg_attr(feature = "serde-impl", serde(rename = "any"))]
    TsAnyKeyword,

    #[cfg_attr(feature = "serde-impl", serde(rename = "unknown"))]
    TsUnknownKeyword,

    #[cfg_attr(feature = "serde-impl", serde(rename = "number"))]
    TsNumberKeyword,

    #[cfg_attr(feature = "serde-impl", serde(rename = "object"))]
    TsObjectKeyword,

    #[cfg_attr(feature = "serde-impl", serde(rename = "boolean"))]
    TsBooleanKeyword,

    #[cfg_attr(feature = "serde-impl", serde(rename = "bigint"))]
    TsBigIntKeyword,

    #[cfg_attr(feature = "serde-impl", serde(rename = "string"))]
    TsStringKeyword,

    #[cfg_attr(feature = "serde-impl", serde(rename = "symbol"))]
    TsSymbolKeyword,

    #[cfg_attr(feature = "serde-impl", serde(rename = "void"))]
    TsVoidKeyword,

    #[cfg_attr(feature = "serde-impl", serde(rename = "undefined"))]
    TsUndefinedKeyword,

    #[cfg_attr(feature = "serde-impl", serde(rename = "null"))]
    TsNullKeyword,

    #[cfg_attr(feature = "serde-impl", serde(rename = "never"))]
    TsNeverKeyword,

    #[cfg_attr(feature = "serde-impl", serde(rename = "intrinsic"))]
    TsIntrinsicKeyword,
}

#[ast_node("TsThisType")]
#[derive(Copy, Eq, Hash, EqIgnoreSpan)]
#[cfg_attr(feature = "arbitrary", derive(arbitrary::Arbitrary))]
#[cfg_attr(feature = "shrink-to-fit", derive(shrink_to_fit::ShrinkToFit))]
pub struct TsThisType {
    pub span: Span,
}

#[ast_node]
#[derive(Eq, Hash, Is, EqIgnoreSpan)]
#[cfg_attr(feature = "arbitrary", derive(arbitrary::Arbitrary))]
#[cfg_attr(feature = "shrink-to-fit", derive(shrink_to_fit::ShrinkToFit))]
pub enum TsFnParam {
    #[tag("Identifier")]
    Ident(BindingIdent),

    #[tag("ArrayPattern")]
    Array(ArrayPat),

    #[tag("RestElement")]
    Rest(RestPat),

    #[tag("ObjectPattern")]
    Object(ObjectPat),
}

#[ast_node("TsFunctionType")]
#[derive(Eq, Hash, EqIgnoreSpan)]
#[cfg_attr(feature = "arbitrary", derive(arbitrary::Arbitrary))]
#[cfg_attr(feature = "shrink-to-fit", derive(shrink_to_fit::ShrinkToFit))]
pub struct TsFnType {
    pub span: Span,
    pub params: Vec<TsFnParam>,

    #[cfg_attr(feature = "serde-impl", serde(default))]
    #[cfg_attr(
        feature = "encoding-impl",
        encoding(with = "cbor4ii::core::types::Maybe")
    )]
    pub type_params: Option<Box<TsTypeParamDecl>>,
    #[cfg_attr(feature = "serde-impl", serde(rename = "typeAnnotation"))]
    pub type_ann: Box<TsTypeAnn>,
}

#[ast_node("TsConstructorType")]
#[derive(Eq, Hash, EqIgnoreSpan)]
#[cfg_attr(feature = "arbitrary", derive(arbitrary::Arbitrary))]
#[cfg_attr(feature = "shrink-to-fit", derive(shrink_to_fit::ShrinkToFit))]
pub struct TsConstructorType {
    pub span: Span,
    pub params: Vec<TsFnParam>,
    #[cfg_attr(feature = "serde-impl", serde(default))]
    #[cfg_attr(
        feature = "encoding-impl",
        encoding(with = "cbor4ii::core::types::Maybe")
    )]
    pub type_params: Option<Box<TsTypeParamDecl>>,
    #[cfg_attr(feature = "serde-impl", serde(rename = "typeAnnotation"))]
    pub type_ann: Box<TsTypeAnn>,
    pub is_abstract: bool,
}

#[ast_node("TsTypeReference")]
#[derive(Eq, Hash, EqIgnoreSpan)]
#[cfg_attr(feature = "arbitrary", derive(arbitrary::Arbitrary))]
#[cfg_attr(feature = "shrink-to-fit", derive(shrink_to_fit::ShrinkToFit))]
pub struct TsTypeRef {
    pub span: Span,
    pub type_name: TsEntityName,
    #[cfg_attr(feature = "serde-impl", serde(default))]
    #[cfg_attr(
        feature = "encoding-impl",
        encoding(with = "cbor4ii::core::types::Maybe")
    )]
    pub type_params: Option<Box<TsTypeParamInstantiation>>,
}

#[ast_node("TsTypePredicate")]
#[derive(Eq, Hash, EqIgnoreSpan)]
#[cfg_attr(feature = "arbitrary", derive(arbitrary::Arbitrary))]
#[cfg_attr(feature = "shrink-to-fit", derive(shrink_to_fit::ShrinkToFit))]
pub struct TsTypePredicate {
    pub span: Span,
    pub asserts: bool,
    pub param_name: TsThisTypeOrIdent,
    #[cfg_attr(feature = "serde-impl", serde(rename = "typeAnnotation"))]
    #[cfg_attr(
        feature = "encoding-impl",
        encoding(with = "cbor4ii::core::types::Maybe")
    )]
    pub type_ann: Option<Box<TsTypeAnn>>,
}

#[ast_node]
#[derive(Eq, Hash, Is, EqIgnoreSpan)]
#[allow(variant_size_differences)]
#[cfg_attr(feature = "arbitrary", derive(arbitrary::Arbitrary))]
#[cfg_attr(feature = "shrink-to-fit", derive(shrink_to_fit::ShrinkToFit))]
pub enum TsThisTypeOrIdent {
    #[tag("TsThisType")]
    TsThisType(TsThisType),

    #[tag("Identifier")]
    Ident(Ident),
}

/// `typeof` operator
#[ast_node("TsTypeQuery")]
#[derive(Eq, Hash, EqIgnoreSpan)]
#[cfg_attr(feature = "arbitrary", derive(arbitrary::Arbitrary))]
#[cfg_attr(feature = "shrink-to-fit", derive(shrink_to_fit::ShrinkToFit))]
pub struct TsTypeQuery {
    pub span: Span,
    pub expr_name: TsTypeQueryExpr,
    #[cfg_attr(feature = "serde-impl", serde(default, rename = "typeArguments"))]
    #[cfg_attr(
        feature = "encoding-impl",
        encoding(with = "cbor4ii::core::types::Maybe")
    )]
    pub type_args: Option<Box<TsTypeParamInstantiation>>,
}

#[ast_node]
#[derive(Eq, Hash, Is, EqIgnoreSpan)]
#[cfg_attr(feature = "arbitrary", derive(arbitrary::Arbitrary))]
#[cfg_attr(feature = "shrink-to-fit", derive(shrink_to_fit::ShrinkToFit))]
pub enum TsTypeQueryExpr {
    #[tag("TsQualifiedName")]
    #[tag("Identifier")]
    TsEntityName(TsEntityName),
    #[tag("TsImportType")]
    Import(TsImportType),
}

#[ast_node("TsImportCallOptions")]
#[derive(Eq, Hash, EqIgnoreSpan)]
#[cfg_attr(feature = "arbitrary", derive(arbitrary::Arbitrary))]
#[cfg_attr(feature = "shrink-to-fit", derive(shrink_to_fit::ShrinkToFit))]
pub struct TsImportCallOptions {
    pub span: Span,
    #[cfg_attr(feature = "serde-impl", serde(default))]
    pub with: Box<ObjectLit>,
}

#[ast_node("TsImportType")]
#[derive(Eq, Hash, EqIgnoreSpan)]
#[cfg_attr(feature = "arbitrary", derive(arbitrary::Arbitrary))]
#[cfg_attr(feature = "shrink-to-fit", derive(shrink_to_fit::ShrinkToFit))]
pub struct TsImportType {
    pub span: Span,
    #[cfg_attr(feature = "serde-impl", serde(rename = "argument"))]
    pub arg: Str,
    #[cfg_attr(
        feature = "encoding-impl",
        encoding(with = "cbor4ii::core::types::Maybe")
    )]
    pub qualifier: Option<TsEntityName>,
    #[cfg_attr(feature = "serde-impl", serde(rename = "typeArguments"))]
    #[cfg_attr(
        feature = "encoding-impl",
        encoding(with = "cbor4ii::core::types::Maybe")
    )]
    pub type_args: Option<Box<TsTypeParamInstantiation>>,
    #[cfg_attr(feature = "serde-impl", serde(default))]
    #[cfg_attr(
        feature = "encoding-impl",
        encoding(with = "cbor4ii::core::types::Maybe")
    )]
    pub attributes: Option<TsImportCallOptions>,
}

#[ast_node("TsTypeLiteral")]
#[derive(Eq, Hash, EqIgnoreSpan)]
#[cfg_attr(feature = "arbitrary", derive(arbitrary::Arbitrary))]
#[cfg_attr(feature = "shrink-to-fit", derive(shrink_to_fit::ShrinkToFit))]
pub struct TsTypeLit {
    pub span: Span,
    pub members: Vec<TsTypeElement>,
}

#[ast_node("TsArrayType")]
#[derive(Eq, Hash, EqIgnoreSpan)]
#[cfg_attr(feature = "arbitrary", derive(arbitrary::Arbitrary))]
#[cfg_attr(feature = "shrink-to-fit", derive(shrink_to_fit::ShrinkToFit))]
pub struct TsArrayType {
    pub span: Span,
    pub elem_type: Box<TsType>,
}

#[ast_node("TsTupleType")]
#[derive(Eq, Hash, EqIgnoreSpan)]
#[cfg_attr(feature = "arbitrary", derive(arbitrary::Arbitrary))]
#[cfg_attr(feature = "shrink-to-fit", derive(shrink_to_fit::ShrinkToFit))]
pub struct TsTupleType {
    pub span: Span,
    pub elem_types: Vec<TsTupleElement>,
}

#[ast_node("TsTupleElement")]
#[derive(Eq, Hash, EqIgnoreSpan)]
#[cfg_attr(feature = "arbitrary", derive(arbitrary::Arbitrary))]
#[cfg_attr(feature = "shrink-to-fit", derive(shrink_to_fit::ShrinkToFit))]
pub struct TsTupleElement {
    pub span: Span,
    /// `Ident` or `RestPat { arg: Ident }`
    #[cfg_attr(
        feature = "encoding-impl",
        encoding(with = "cbor4ii::core::types::Maybe")
    )]
    pub label: Option<Pat>,
    pub ty: Box<TsType>,
}

#[ast_node("TsOptionalType")]
#[derive(Eq, Hash, EqIgnoreSpan)]
#[cfg_attr(feature = "arbitrary", derive(arbitrary::Arbitrary))]
#[cfg_attr(feature = "shrink-to-fit", derive(shrink_to_fit::ShrinkToFit))]
pub struct TsOptionalType {
    pub span: Span,
    #[cfg_attr(feature = "serde-impl", serde(rename = "typeAnnotation"))]
    pub type_ann: Box<TsType>,
}

#[ast_node("TsRestType")]
#[derive(Eq, Hash, EqIgnoreSpan)]
#[cfg_attr(feature = "arbitrary", derive(arbitrary::Arbitrary))]
#[cfg_attr(feature = "shrink-to-fit", derive(shrink_to_fit::ShrinkToFit))]
pub struct TsRestType {
    pub span: Span,
    #[cfg_attr(feature = "serde-impl", serde(rename = "typeAnnotation"))]
    pub type_ann: Box<TsType>,
}

#[ast_node]
#[derive(Eq, Hash, Is, EqIgnoreSpan)]
#[cfg_attr(feature = "arbitrary", derive(arbitrary::Arbitrary))]
#[cfg_attr(feature = "shrink-to-fit", derive(shrink_to_fit::ShrinkToFit))]
pub enum TsUnionOrIntersectionType {
    #[tag("TsUnionType")]
    TsUnionType(TsUnionType),

    #[tag("TsIntersectionType")]
    TsIntersectionType(TsIntersectionType),
}

#[ast_node("TsUnionType")]
#[derive(Eq, Hash, EqIgnoreSpan)]
#[cfg_attr(feature = "arbitrary", derive(arbitrary::Arbitrary))]
#[cfg_attr(feature = "shrink-to-fit", derive(shrink_to_fit::ShrinkToFit))]
pub struct TsUnionType {
    pub span: Span,
    pub types: Vec<Box<TsType>>,
}

#[ast_node("TsIntersectionType")]
#[derive(Eq, Hash, EqIgnoreSpan)]
#[cfg_attr(feature = "arbitrary", derive(arbitrary::Arbitrary))]
#[cfg_attr(feature = "shrink-to-fit", derive(shrink_to_fit::ShrinkToFit))]
pub struct TsIntersectionType {
    pub span: Span,
    pub types: Vec<Box<TsType>>,
}

#[ast_node("TsConditionalType")]
#[derive(Eq, Hash, EqIgnoreSpan)]
#[cfg_attr(feature = "arbitrary", derive(arbitrary::Arbitrary))]
#[cfg_attr(feature = "shrink-to-fit", derive(shrink_to_fit::ShrinkToFit))]
pub struct TsConditionalType {
    pub span: Span,
    pub check_type: Box<TsType>,
    pub extends_type: Box<TsType>,
    pub true_type: Box<TsType>,
    pub false_type: Box<TsType>,
}

#[ast_node("TsInferType")]
#[derive(Eq, Hash, EqIgnoreSpan)]
#[cfg_attr(feature = "arbitrary", derive(arbitrary::Arbitrary))]
#[cfg_attr(feature = "shrink-to-fit", derive(shrink_to_fit::ShrinkToFit))]
pub struct TsInferType {
    pub span: Span,
    pub type_param: TsTypeParam,
}

#[ast_node("TsParenthesizedType")]
#[derive(Eq, Hash, EqIgnoreSpan)]
#[cfg_attr(feature = "arbitrary", derive(arbitrary::Arbitrary))]
#[cfg_attr(feature = "shrink-to-fit", derive(shrink_to_fit::ShrinkToFit))]
pub struct TsParenthesizedType {
    pub span: Span,
    #[cfg_attr(feature = "serde-impl", serde(rename = "typeAnnotation"))]
    pub type_ann: Box<TsType>,
}

#[ast_node("TsTypeOperator")]
#[derive(Eq, Hash, EqIgnoreSpan)]
#[cfg_attr(feature = "arbitrary", derive(arbitrary::Arbitrary))]
#[cfg_attr(feature = "shrink-to-fit", derive(shrink_to_fit::ShrinkToFit))]
pub struct TsTypeOperator {
    pub span: Span,
    pub op: TsTypeOperatorOp,
    #[cfg_attr(feature = "serde-impl", serde(rename = "typeAnnotation"))]
    pub type_ann: Box<TsType>,
}

#[derive(StringEnum, Clone, Copy, PartialEq, Eq, Hash, EqIgnoreSpan)]
#[cfg_attr(feature = "arbitrary", derive(arbitrary::Arbitrary))]
#[cfg_attr(feature = "shrink-to-fit", derive(shrink_to_fit::ShrinkToFit))]
#[cfg_attr(
    any(feature = "rkyv-impl"),
    derive(rkyv::Archive, rkyv::Serialize, rkyv::Deserialize)
)]
#[cfg_attr(feature = "rkyv-impl", derive(bytecheck::CheckBytes))]
#[cfg_attr(feature = "rkyv-impl", repr(u32))]
#[cfg_attr(
    feature = "encoding-impl",
    derive(::swc_common::Encode, ::swc_common::Decode)
)]
#[cfg_attr(swc_ast_unknown, non_exhaustive)]
pub enum TsTypeOperatorOp {
    /// `keyof`
    KeyOf,
    /// `unique`
    Unique,
    /// `readonly`
    ReadOnly,
}

#[ast_node("TsIndexedAccessType")]
#[derive(Eq, Hash, EqIgnoreSpan)]
#[cfg_attr(feature = "arbitrary", derive(arbitrary::Arbitrary))]
#[cfg_attr(feature = "shrink-to-fit", derive(shrink_to_fit::ShrinkToFit))]
pub struct TsIndexedAccessType {
    pub span: Span,
    pub readonly: bool,
    #[cfg_attr(feature = "serde-impl", serde(rename = "objectType"))]
    pub obj_type: Box<TsType>,
    pub index_type: Box<TsType>,
}

#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash, EqIgnoreSpan)]
#[cfg_attr(feature = "arbitrary", derive(arbitrary::Arbitrary))]
#[cfg_attr(feature = "shrink-to-fit", derive(shrink_to_fit::ShrinkToFit))]
#[cfg_attr(
    any(feature = "rkyv-impl"),
    derive(rkyv::Archive, rkyv::Serialize, rkyv::Deserialize)
)]
#[cfg_attr(feature = "rkyv-impl", derive(bytecheck::CheckBytes))]
#[cfg_attr(feature = "rkyv-impl", repr(u32))]
#[cfg_attr(
    feature = "encoding-impl",
    derive(::swc_common::Encode, ::swc_common::Decode)
)]
#[cfg_attr(swc_ast_unknown, non_exhaustive)]
pub enum TruePlusMinus {
    True,
    Plus,
    Minus,
}

#[cfg(feature = "serde-impl")]
impl Serialize for TruePlusMinus {
    fn serialize<S>(&self, serializer: S) -> Result<S::Ok, S::Error>
    where
        S: ::serde::Serializer,
    {
        match *self {
            TruePlusMinus::True => serializer.serialize_bool(true),
            TruePlusMinus::Plus => serializer.serialize_str("+"),
            TruePlusMinus::Minus => serializer.serialize_str("-"),
        }
    }
}

#[cfg(feature = "serde-impl")]
impl<'de> Deserialize<'de> for TruePlusMinus {
    fn deserialize<D>(deserializer: D) -> Result<Self, D::Error>
    where
        D: Deserializer<'de>,
    {
        struct TruePlusMinusVisitor;

        impl Visitor<'_> for TruePlusMinusVisitor {
            type Value = TruePlusMinus;

            fn expecting(&self, formatter: &mut fmt::Formatter<'_>) -> fmt::Result {
                formatter.write_str("one of '+', '-', true")
            }

            fn visit_str<E>(self, value: &str) -> Result<Self::Value, E>
            where
                E: de::Error,
            {
                match value {
                    "+" => Ok(TruePlusMinus::Plus),
                    "-" => Ok(TruePlusMinus::Minus),
                    "true" => Ok(TruePlusMinus::True),
                    _ => Err(de::Error::invalid_value(Unexpected::Str(value), &self)),
                }
            }

            fn visit_bool<E>(self, value: bool) -> Result<Self::Value, E>
            where
                E: de::Error,
            {
                if value {
                    Ok(TruePlusMinus::True)
                } else {
                    Err(de::Error::invalid_value(Unexpected::Bool(value), &self))
                }
            }
        }

        deserializer.deserialize_any(TruePlusMinusVisitor)
    }
}

#[ast_node("TsMappedType")]
#[derive(Eq, Hash, EqIgnoreSpan)]
#[cfg_attr(feature = "arbitrary", derive(arbitrary::Arbitrary))]
#[cfg_attr(feature = "shrink-to-fit", derive(shrink_to_fit::ShrinkToFit))]
pub struct TsMappedType {
    pub span: Span,
    #[cfg_attr(feature = "serde-impl", serde(default))]
    #[cfg_attr(
        feature = "encoding-impl",
        encoding(with = "cbor4ii::core::types::Maybe")
    )]
    pub readonly: Option<TruePlusMinus>,
    pub type_param: TsTypeParam,
    #[cfg_attr(feature = "serde-impl", serde(default, rename = "nameType"))]
    #[cfg_attr(
        feature = "encoding-impl",
        encoding(with = "cbor4ii::core::types::Maybe")
    )]
    pub name_type: Option<Box<TsType>>,
    #[cfg_attr(feature = "serde-impl", serde(default))]
    #[cfg_attr(
        feature = "encoding-impl",
        encoding(with = "cbor4ii::core::types::Maybe")
    )]
    pub optional: Option<TruePlusMinus>,
    #[cfg_attr(feature = "serde-impl", serde(default, rename = "typeAnnotation"))]
    #[cfg_attr(
        feature = "encoding-impl",
        encoding(with = "cbor4ii::core::types::Maybe")
    )]
    pub type_ann: Option<Box<TsType>>,
}

#[ast_node("TsLiteralType")]
#[derive(Eq, Hash, EqIgnoreSpan)]
#[cfg_attr(feature = "arbitrary", derive(arbitrary::Arbitrary))]
#[cfg_attr(feature = "shrink-to-fit", derive(shrink_to_fit::ShrinkToFit))]
pub struct TsLitType {
    pub span: Span,
    #[cfg_attr(feature = "serde-impl", serde(rename = "literal"))]
    pub lit: TsLit,
}

#[ast_node]
#[derive(Eq, Hash, Is, EqIgnoreSpan)]
#[cfg_attr(feature = "arbitrary", derive(arbitrary::Arbitrary))]
#[cfg_attr(feature = "shrink-to-fit", derive(shrink_to_fit::ShrinkToFit))]
pub enum TsLit {
    #[tag("NumericLiteral")]
    Number(Number),

    #[tag("StringLiteral")]
    Str(Str),

    #[tag("BooleanLiteral")]
    Bool(Bool),

    #[tag("BigIntLiteral")]
    BigInt(BigInt),

    #[tag("TemplateLiteral")]
    Tpl(TsTplLitType),
}

#[ast_node("TemplateLiteral")]
#[derive(Eq, Hash, EqIgnoreSpan)]
#[cfg_attr(feature = "arbitrary", derive(arbitrary::Arbitrary))]
#[cfg_attr(feature = "shrink-to-fit", derive(shrink_to_fit::ShrinkToFit))]
pub struct TsTplLitType {
    pub span: Span,

    pub types: Vec<Box<TsType>>,

    pub quasis: Vec<TplElement>,
}

// // ================
// // TypeScript declarations
// // ================

#[ast_node("TsInterfaceDeclaration")]
#[derive(Eq, Hash, EqIgnoreSpan)]
#[cfg_attr(feature = "arbitrary", derive(arbitrary::Arbitrary))]
#[cfg_attr(feature = "shrink-to-fit", derive(shrink_to_fit::ShrinkToFit))]
pub struct TsInterfaceDecl {
    pub span: Span,
    pub id: Ident,
    pub declare: bool,
    #[cfg_attr(feature = "serde-impl", serde(default))]
    #[cfg_attr(
        feature = "encoding-impl",
        encoding(with = "cbor4ii::core::types::Maybe")
    )]
    pub type_params: Option<Box<TsTypeParamDecl>>,
    pub extends: Vec<TsExprWithTypeArgs>,
    pub body: TsInterfaceBody,
}

#[ast_node("TsInterfaceBody")]
#[derive(Eq, Hash, EqIgnoreSpan)]
#[cfg_attr(feature = "arbitrary", derive(arbitrary::Arbitrary))]
#[cfg_attr(feature = "shrink-to-fit", derive(shrink_to_fit::ShrinkToFit))]
pub struct TsInterfaceBody {
    pub span: Span,
    pub body: Vec<TsTypeElement>,
}

#[ast_node("TsExpressionWithTypeArguments")]
#[derive(Eq, Hash, EqIgnoreSpan)]
#[cfg_attr(feature = "arbitrary", derive(arbitrary::Arbitrary))]
#[cfg_attr(feature = "shrink-to-fit", derive(shrink_to_fit::ShrinkToFit))]
pub struct TsExprWithTypeArgs {
    pub span: Span,
    #[cfg_attr(feature = "serde-impl", serde(rename = "expression"))]
    pub expr: Box<Expr>,
    #[cfg_attr(feature = "serde-impl", serde(default, rename = "typeArguments"))]
    #[cfg_attr(
        feature = "encoding-impl",
        encoding(with = "cbor4ii::core::types::Maybe")
    )]
    pub type_args: Option<Box<TsTypeParamInstantiation>>,
}

#[ast_node("TsTypeAliasDeclaration")]
#[derive(Eq, Hash, EqIgnoreSpan)]
#[cfg_attr(feature = "arbitrary", derive(arbitrary::Arbitrary))]
#[cfg_attr(feature = "shrink-to-fit", derive(shrink_to_fit::ShrinkToFit))]
pub struct TsTypeAliasDecl {
    pub span: Span,
    pub declare: bool,
    pub id: Ident,
    #[cfg_attr(feature = "serde-impl", serde(default))]
    #[cfg_attr(
        feature = "encoding-impl",
        encoding(with = "cbor4ii::core::types::Maybe")
    )]
    pub type_params: Option<Box<TsTypeParamDecl>>,
    #[cfg_attr(feature = "serde-impl", serde(rename = "typeAnnotation"))]
    pub type_ann: Box<TsType>,
}

#[ast_node("TsEnumDeclaration")]
#[derive(Eq, Hash, EqIgnoreSpan)]
#[cfg_attr(feature = "arbitrary", derive(arbitrary::Arbitrary))]
#[cfg_attr(feature = "shrink-to-fit", derive(shrink_to_fit::ShrinkToFit))]
pub struct TsEnumDecl {
    pub span: Span,
    pub declare: bool,
    pub is_const: bool,
    pub id: Ident,
    pub members: Vec<TsEnumMember>,
}

#[ast_node("TsEnumMember")]
#[derive(Eq, Hash, EqIgnoreSpan)]
#[cfg_attr(feature = "arbitrary", derive(arbitrary::Arbitrary))]
#[cfg_attr(feature = "shrink-to-fit", derive(shrink_to_fit::ShrinkToFit))]
pub struct TsEnumMember {
    pub span: Span,
    pub id: TsEnumMemberId,
    #[cfg_attr(feature = "serde-impl", serde(default))]
    #[cfg_attr(
        feature = "encoding-impl",
        encoding(with = "cbor4ii::core::types::Maybe")
    )]
    pub init: Option<Box<Expr>>,
}

///
/// - Invalid: [Ident] with empty symbol.
#[ast_node]
#[derive(Eq, Hash, Is, EqIgnoreSpan)]
#[cfg_attr(feature = "arbitrary", derive(arbitrary::Arbitrary))]
#[cfg_attr(feature = "shrink-to-fit", derive(shrink_to_fit::ShrinkToFit))]
pub enum TsEnumMemberId {
    #[tag("Identifier")]
    Ident(Ident),

    #[tag("StringLiteral")]
    Str(Str),
}

#[ast_node("TsModuleDeclaration")]
#[derive(Eq, Hash, EqIgnoreSpan)]
#[cfg_attr(feature = "arbitrary", derive(arbitrary::Arbitrary))]
#[cfg_attr(feature = "shrink-to-fit", derive(shrink_to_fit::ShrinkToFit))]
pub struct TsModuleDecl {
    pub span: Span,
    pub declare: bool,
    /// In TypeScript, this is only available through`node.flags`.
    pub global: bool,
    pub namespace: bool,

    pub id: TsModuleName,
    #[cfg_attr(feature = "serde-impl", serde(default))]
    #[cfg_attr(
        feature = "encoding-impl",
        encoding(with = "cbor4ii::core::types::Maybe")
    )]
    pub body: Option<TsNamespaceBody>,
}

/// `namespace A.B { }` is a namespace named `A` with another TsNamespaceDecl as
/// its body.
#[ast_node]
#[derive(Eq, Hash, Is, EqIgnoreSpan)]
#[cfg_attr(feature = "arbitrary", derive(arbitrary::Arbitrary))]
#[cfg_attr(feature = "shrink-to-fit", derive(shrink_to_fit::ShrinkToFit))]
pub enum TsNamespaceBody {
    #[tag("TsModuleBlock")]
    TsModuleBlock(TsModuleBlock),

    #[tag("TsNamespaceDeclaration")]
    TsNamespaceDecl(TsNamespaceDecl),
}

#[ast_node("TsModuleBlock")]
#[derive(Eq, Hash, EqIgnoreSpan)]
#[cfg_attr(feature = "arbitrary", derive(arbitrary::Arbitrary))]
#[cfg_attr(feature = "shrink-to-fit", derive(shrink_to_fit::ShrinkToFit))]
pub struct TsModuleBlock {
    pub span: Span,
    pub body: Vec<ModuleItem>,
}

#[ast_node("TsNamespaceDeclaration")]
#[derive(Eq, Hash, EqIgnoreSpan)]
#[cfg_attr(feature = "arbitrary", derive(arbitrary::Arbitrary))]
#[cfg_attr(feature = "shrink-to-fit", derive(shrink_to_fit::ShrinkToFit))]
pub struct TsNamespaceDecl {
    pub span: Span,
    pub declare: bool,
    /// In TypeScript, this is only available through`node.flags`.
    pub global: bool,
    pub id: Ident,
    pub body: Box<TsNamespaceBody>,
}

#[ast_node]
#[derive(Eq, Hash, Is, EqIgnoreSpan)]
#[cfg_attr(feature = "arbitrary", derive(arbitrary::Arbitrary))]
#[cfg_attr(feature = "shrink-to-fit", derive(shrink_to_fit::ShrinkToFit))]
pub enum TsModuleName {
    #[tag("Identifier")]
    Ident(Ident),

    #[tag("StringLiteral")]
    Str(Str),
}

#[ast_node("TsImportEqualsDeclaration")]
#[derive(Eq, Hash, EqIgnoreSpan)]
#[cfg_attr(feature = "arbitrary", derive(arbitrary::Arbitrary))]
#[cfg_attr(feature = "shrink-to-fit", derive(shrink_to_fit::ShrinkToFit))]
pub struct TsImportEqualsDecl {
    pub span: Span,
    pub is_export: bool,
    pub is_type_only: bool,
    pub id: Ident,
    pub module_ref: TsModuleRef,
}

#[ast_node]
#[derive(Eq, Hash, Is, EqIgnoreSpan)]
#[cfg_attr(feature = "arbitrary", derive(arbitrary::Arbitrary))]
#[cfg_attr(feature = "shrink-to-fit", derive(shrink_to_fit::ShrinkToFit))]
pub enum TsModuleRef {
    #[tag("TsQualifiedName")]
    #[tag("Identifier")]
    TsEntityName(TsEntityName),

    #[tag("TsExternalModuleReference")]
    TsExternalModuleRef(TsExternalModuleRef),
}

#[ast_node("TsExternalModuleReference")]
#[derive(Eq, Hash, EqIgnoreSpan)]
#[cfg_attr(feature = "arbitrary", derive(arbitrary::Arbitrary))]
#[cfg_attr(feature = "shrink-to-fit", derive(shrink_to_fit::ShrinkToFit))]
pub struct TsExternalModuleRef {
    pub span: Span,
    #[cfg_attr(feature = "serde-impl", serde(rename = "expression"))]
    pub expr: Str,
}

/// TypeScript's own parser uses ExportAssignment for both `export default` and
/// `export =`. But for @babel/parser, `export default` is an ExportDefaultDecl,
/// so a TsExportAssignment is always `export =`.
#[ast_node("TsExportAssignment")]
#[derive(Eq, Hash, EqIgnoreSpan)]
#[cfg_attr(feature = "arbitrary", derive(arbitrary::Arbitrary))]
#[cfg_attr(feature = "shrink-to-fit", derive(shrink_to_fit::ShrinkToFit))]
pub struct TsExportAssignment {
    pub span: Span,
    #[cfg_attr(feature = "serde-impl", serde(rename = "expression"))]
    pub expr: Box<Expr>,
}

#[ast_node("TsNamespaceExportDeclaration")]
#[derive(Eq, Hash, EqIgnoreSpan)]
#[cfg_attr(feature = "arbitrary", derive(arbitrary::Arbitrary))]
#[cfg_attr(feature = "shrink-to-fit", derive(shrink_to_fit::ShrinkToFit))]
pub struct TsNamespaceExportDecl {
    pub span: Span,
    pub id: Ident,
}

// // ================
// // TypeScript exprs
// // ================

#[ast_node("TsAsExpression")]
#[derive(Eq, Hash, EqIgnoreSpan)]
#[cfg_attr(feature = "arbitrary", derive(arbitrary::Arbitrary))]
#[cfg_attr(feature = "shrink-to-fit", derive(shrink_to_fit::ShrinkToFit))]
pub struct TsAsExpr {
    pub span: Span,
    #[cfg_attr(feature = "serde-impl", serde(rename = "expression"))]
    pub expr: Box<Expr>,
    #[cfg_attr(feature = "serde-impl", serde(rename = "typeAnnotation"))]
    pub type_ann: Box<TsType>,
}

#[ast_node("TsTypeAssertion")]
#[derive(Eq, Hash, EqIgnoreSpan)]
#[cfg_attr(feature = "arbitrary", derive(arbitrary::Arbitrary))]
#[cfg_attr(feature = "shrink-to-fit", derive(shrink_to_fit::ShrinkToFit))]
pub struct TsTypeAssertion {
    pub span: Span,
    #[cfg_attr(feature = "serde-impl", serde(rename = "expression"))]
    pub expr: Box<Expr>,
    #[cfg_attr(feature = "serde-impl", serde(rename = "typeAnnotation"))]
    pub type_ann: Box<TsType>,
}

#[ast_node("TsNonNullExpression")]
#[derive(Eq, Hash, EqIgnoreSpan)]
#[cfg_attr(feature = "arbitrary", derive(arbitrary::Arbitrary))]
#[cfg_attr(feature = "shrink-to-fit", derive(shrink_to_fit::ShrinkToFit))]
pub struct TsNonNullExpr {
    pub span: Span,
    #[cfg_attr(feature = "serde-impl", serde(rename = "expression"))]
    pub expr: Box<Expr>,
}

#[ast_node("TsSatisfiesExpression")]
#[derive(Eq, Hash, EqIgnoreSpan)]
#[cfg_attr(feature = "arbitrary", derive(arbitrary::Arbitrary))]
#[cfg_attr(feature = "shrink-to-fit", derive(shrink_to_fit::ShrinkToFit))]
pub struct TsSatisfiesExpr {
    pub span: Span,
    #[cfg_attr(feature = "serde-impl", serde(rename = "expression"))]
    pub expr: Box<Expr>,
    #[cfg_attr(feature = "serde-impl", serde(rename = "typeAnnotation"))]
    pub type_ann: Box<TsType>,
}

#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash, EqIgnoreSpan)]
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
pub enum Accessibility {
    #[cfg_attr(feature = "serde-impl", serde(rename = "public"))]
    Public,
    #[cfg_attr(feature = "serde-impl", serde(rename = "protected"))]
    Protected,
    #[cfg_attr(feature = "serde-impl", serde(rename = "private"))]
    Private,
}

#[ast_node("TsConstAssertion")]
#[derive(Eq, Hash, EqIgnoreSpan)]
#[cfg_attr(feature = "arbitrary", derive(arbitrary::Arbitrary))]
#[cfg_attr(feature = "shrink-to-fit", derive(shrink_to_fit::ShrinkToFit))]
pub struct TsConstAssertion {
    pub span: Span,
    #[cfg_attr(feature = "serde-impl", serde(rename = "expression"))]
    pub expr: Box<Expr>,
}

#[ast_node("TsInstantiation")]
#[derive(Eq, Hash, EqIgnoreSpan)]
#[cfg_attr(feature = "arbitrary", derive(arbitrary::Arbitrary))]
#[cfg_attr(feature = "shrink-to-fit", derive(shrink_to_fit::ShrinkToFit))]
pub struct TsInstantiation {
    pub span: Span,
    #[cfg_attr(feature = "serde-impl", serde(rename = "expression"))]
    pub expr: Box<Expr>,
    #[cfg_attr(feature = "serde-impl", serde(rename = "typeArguments"))]
    pub type_args: Box<TsTypeParamInstantiation>,
}
