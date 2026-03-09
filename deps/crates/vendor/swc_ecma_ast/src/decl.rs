use is_macro::Is;
use string_enum::StringEnum;
use swc_common::{ast_node, util::take::Take, EqIgnoreSpan, Span, SyntaxContext, DUMMY_SP};

use crate::{
    class::Class,
    expr::Expr,
    function::Function,
    ident::Ident,
    pat::Pat,
    typescript::{TsEnumDecl, TsInterfaceDecl, TsModuleDecl, TsTypeAliasDecl},
};

#[ast_node]
#[derive(Eq, Hash, Is, EqIgnoreSpan)]
#[cfg_attr(feature = "arbitrary", derive(arbitrary::Arbitrary))]
#[cfg_attr(feature = "shrink-to-fit", derive(shrink_to_fit::ShrinkToFit))]
pub enum Decl {
    #[tag("ClassDeclaration")]
    Class(ClassDecl),
    #[tag("FunctionDeclaration")]
    #[is(name = "fn_decl")]
    Fn(FnDecl),
    #[tag("VariableDeclaration")]
    Var(Box<VarDecl>),
    #[tag("UsingDeclaration")]
    Using(Box<UsingDecl>),

    #[tag("TsInterfaceDeclaration")]
    TsInterface(Box<TsInterfaceDecl>),
    #[tag("TsTypeAliasDeclaration")]
    TsTypeAlias(Box<TsTypeAliasDecl>),
    #[tag("TsEnumDeclaration")]
    TsEnum(Box<TsEnumDecl>),
    #[tag("TsModuleDeclaration")]
    TsModule(Box<TsModuleDecl>),
}

boxed!(
    Decl,
    [
        VarDecl,
        UsingDecl,
        TsInterfaceDecl,
        TsTypeAliasDecl,
        TsEnumDecl,
        TsModuleDecl
    ]
);

macro_rules! decl_from {
    ($($variant_ty:ty),*) => {
        $(
            bridge_from!(crate::Stmt, Decl, $variant_ty);
            bridge_from!(crate::ModuleItem, crate::Stmt, $variant_ty);
        )*
    };
}

decl_from!(
    ClassDecl,
    FnDecl,
    VarDecl,
    UsingDecl,
    TsInterfaceDecl,
    TsTypeAliasDecl,
    TsEnumDecl,
    TsModuleDecl
);

macro_rules! decl_from_boxed {
    ($($variant_ty:ty),*) => {
        $(
            bridge_from!(Box<crate::Stmt>, Decl, $variant_ty);
            bridge_from!(Box<crate::Stmt>, Decl, Box<$variant_ty>);
            bridge_from!(crate::Stmt, Decl, Box<$variant_ty>);
            bridge_from!(crate::ModuleItem, crate::Stmt, Box<$variant_ty>);
        )*
    };
}

decl_from_boxed!(
    VarDecl,
    UsingDecl,
    TsInterfaceDecl,
    TsTypeAliasDecl,
    TsEnumDecl,
    TsModuleDecl
);

impl Take for Decl {
    fn dummy() -> Self {
        Decl::Var(Default::default())
    }
}

#[ast_node("FunctionDeclaration")]
#[derive(Eq, Hash, EqIgnoreSpan)]
#[cfg_attr(feature = "arbitrary", derive(arbitrary::Arbitrary))]
#[cfg_attr(feature = "shrink-to-fit", derive(shrink_to_fit::ShrinkToFit))]
pub struct FnDecl {
    #[cfg_attr(feature = "serde-impl", serde(rename = "identifier"))]
    pub ident: Ident,

    #[cfg_attr(feature = "serde-impl", serde(default))]
    pub declare: bool,

    #[cfg_attr(feature = "serde-impl", serde(flatten))]
    #[span]
    pub function: Box<Function>,
}

impl Take for FnDecl {
    fn dummy() -> Self {
        FnDecl {
            ident: Take::dummy(),
            declare: Default::default(),
            function: Take::dummy(),
        }
    }
}

#[ast_node("ClassDeclaration")]
#[derive(Eq, Hash, EqIgnoreSpan)]
#[cfg_attr(feature = "arbitrary", derive(arbitrary::Arbitrary))]
#[cfg_attr(feature = "shrink-to-fit", derive(shrink_to_fit::ShrinkToFit))]
pub struct ClassDecl {
    #[cfg_attr(feature = "serde-impl", serde(rename = "identifier"))]
    pub ident: Ident,

    #[cfg_attr(feature = "serde-impl", serde(default))]
    pub declare: bool,

    #[cfg_attr(feature = "serde-impl", serde(flatten))]
    #[span]
    pub class: Box<Class>,
}

impl Take for ClassDecl {
    fn dummy() -> Self {
        ClassDecl {
            ident: Take::dummy(),
            declare: Default::default(),
            class: Take::dummy(),
        }
    }
}

#[ast_node("VariableDeclaration")]
#[derive(Eq, Hash, EqIgnoreSpan, Default)]
#[cfg_attr(feature = "arbitrary", derive(arbitrary::Arbitrary))]
#[cfg_attr(feature = "shrink-to-fit", derive(shrink_to_fit::ShrinkToFit))]
pub struct VarDecl {
    pub span: Span,

    pub ctxt: SyntaxContext,

    pub kind: VarDeclKind,

    #[cfg_attr(feature = "serde-impl", serde(default))]
    pub declare: bool,

    #[cfg_attr(feature = "serde-impl", serde(rename = "declarations"))]
    pub decls: Vec<VarDeclarator>,
}

impl Take for VarDecl {
    fn dummy() -> Self {
        Default::default()
    }
}

#[derive(StringEnum, Clone, Copy, Eq, PartialEq, PartialOrd, Ord, Hash, EqIgnoreSpan, Default)]
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
pub enum VarDeclKind {
    /// `var`
    #[default]
    Var,
    /// `let`
    Let,
    /// `const`
    Const,
}

#[ast_node("VariableDeclarator")]
#[derive(Eq, Hash, EqIgnoreSpan)]
#[cfg_attr(feature = "arbitrary", derive(arbitrary::Arbitrary))]
#[cfg_attr(feature = "shrink-to-fit", derive(shrink_to_fit::ShrinkToFit))]
pub struct VarDeclarator {
    pub span: Span,
    #[cfg_attr(feature = "serde-impl", serde(rename = "id"))]
    pub name: Pat,

    /// Initialization expression.
    #[cfg_attr(feature = "serde-impl", serde(default))]
    #[cfg_attr(
        feature = "encoding-impl",
        encoding(with = "cbor4ii::core::types::Maybe")
    )]
    pub init: Option<Box<Expr>>,

    /// Typescript only
    #[cfg_attr(feature = "serde-impl", serde(default))]
    pub definite: bool,
}

impl Take for VarDeclarator {
    fn dummy() -> Self {
        VarDeclarator {
            span: DUMMY_SP,
            name: Take::dummy(),
            init: Take::dummy(),
            definite: Default::default(),
        }
    }
}

#[ast_node("UsingDeclaration")]
#[derive(Eq, Hash, EqIgnoreSpan)]
#[cfg_attr(feature = "arbitrary", derive(arbitrary::Arbitrary))]
#[cfg_attr(feature = "shrink-to-fit", derive(shrink_to_fit::ShrinkToFit))]
pub struct UsingDecl {
    #[cfg_attr(feature = "serde-impl", serde(default))]
    pub span: Span,

    #[cfg_attr(feature = "serde-impl", serde(default))]
    pub is_await: bool,

    #[cfg_attr(feature = "serde-impl", serde(default))]
    pub decls: Vec<VarDeclarator>,
}

impl Take for UsingDecl {
    fn dummy() -> Self {
        Self {
            span: DUMMY_SP,
            is_await: Default::default(),
            decls: Take::dummy(),
        }
    }
}
