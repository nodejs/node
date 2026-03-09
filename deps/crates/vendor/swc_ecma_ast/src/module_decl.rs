use std::borrow::Cow;

use is_macro::Is;
use swc_atoms::Atom;
use swc_common::{ast_node, util::take::Take, EqIgnoreSpan, Span, DUMMY_SP};

use crate::{
    decl::Decl,
    expr::{ClassExpr, Expr, FnExpr},
    ident::Ident,
    lit::Str,
    typescript::{TsExportAssignment, TsImportEqualsDecl, TsInterfaceDecl, TsNamespaceExportDecl},
    BindingIdent, IdentName, ObjectLit,
};

#[ast_node]
#[derive(Eq, Hash, Is, EqIgnoreSpan)]
#[cfg_attr(feature = "arbitrary", derive(arbitrary::Arbitrary))]
#[cfg_attr(feature = "shrink-to-fit", derive(shrink_to_fit::ShrinkToFit))]
pub enum ModuleDecl {
    #[tag("ImportDeclaration")]
    Import(ImportDecl),

    #[tag("ExportDeclaration")]
    ExportDecl(ExportDecl),

    #[tag("ExportNamedDeclaration")]
    ExportNamed(NamedExport),

    #[tag("ExportDefaultDeclaration")]
    ExportDefaultDecl(ExportDefaultDecl),

    #[tag("ExportDefaultExpression")]
    ExportDefaultExpr(ExportDefaultExpr),

    #[tag("ExportAllDeclaration")]
    ExportAll(ExportAll),

    #[tag("TsImportEqualsDeclaration")]
    TsImportEquals(Box<TsImportEqualsDecl>),

    #[tag("TsExportAssignment")]
    TsExportAssignment(TsExportAssignment),

    #[tag("TsNamespaceExportDeclaration")]
    TsNamespaceExport(TsNamespaceExportDecl),
}

boxed!(ModuleDecl, [TsImportEqualsDecl]);

macro_rules! module_decl {
    ([$($variant:ty),*]) => {
        $(
            bridge_from!(crate::ModuleItem, crate::ModuleDecl, $variant);
        )*
    };
}

module_decl!([
    ImportDecl,
    ExportDecl,
    NamedExport,
    ExportDefaultDecl,
    ExportDefaultExpr,
    ExportAll,
    TsImportEqualsDecl,
    TsExportAssignment,
    TsNamespaceExportDecl
]);

impl Take for ModuleDecl {
    fn dummy() -> Self {
        ImportDecl::dummy().into()
    }
}

/// Default exports other than **direct** function expression or class
/// expression.
///
///
/// # Note
///
/// ```ts
/// export default function Foo() {
/// }
/// ```
///
/// is [`ExportDefaultDecl`] and it's hoisted.
///
/// ```ts
/// export default (function Foo() {
/// })
/// ```
///
/// is [`ExportDefaultExpr`] and it's not hoisted.
#[ast_node("ExportDefaultExpression")]
#[derive(Eq, Hash, EqIgnoreSpan)]
#[cfg_attr(feature = "arbitrary", derive(arbitrary::Arbitrary))]
#[cfg_attr(feature = "shrink-to-fit", derive(shrink_to_fit::ShrinkToFit))]
pub struct ExportDefaultExpr {
    pub span: Span,

    #[cfg_attr(feature = "serde-impl", serde(rename = "expression"))]
    pub expr: Box<Expr>,
}

#[ast_node("ExportDeclaration")]
#[derive(Eq, Hash, EqIgnoreSpan)]
#[cfg_attr(feature = "arbitrary", derive(arbitrary::Arbitrary))]
#[cfg_attr(feature = "shrink-to-fit", derive(shrink_to_fit::ShrinkToFit))]
pub struct ExportDecl {
    pub span: Span,

    #[cfg_attr(feature = "serde-impl", serde(rename = "declaration"))]
    pub decl: Decl,
}

#[ast_node("ImportDeclaration")]
#[derive(Eq, Hash, EqIgnoreSpan)]
#[cfg_attr(feature = "arbitrary", derive(arbitrary::Arbitrary))]
#[cfg_attr(feature = "shrink-to-fit", derive(shrink_to_fit::ShrinkToFit))]
pub struct ImportDecl {
    pub span: Span,

    #[cfg_attr(feature = "serde-impl", serde(default))]
    pub specifiers: Vec<ImportSpecifier>,

    #[cfg_attr(feature = "serde-impl", serde(rename = "source"))]
    pub src: Box<Str>,

    #[cfg_attr(feature = "serde-impl", serde(default, rename = "typeOnly"))]
    pub type_only: bool,

    #[cfg_attr(feature = "serde-impl", serde(default))]
    #[cfg_attr(
        feature = "encoding-impl",
        encoding(with = "cbor4ii::core::types::Maybe")
    )]
    pub with: Option<Box<ObjectLit>>,

    #[cfg_attr(feature = "serde-impl", serde(default))]
    pub phase: ImportPhase,
}

#[derive(Debug, Copy, Clone, PartialEq, Eq, Hash, Default, EqIgnoreSpan)]
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
pub enum ImportPhase {
    #[default]
    #[cfg_attr(feature = "serde-impl", serde(rename = "evaluation"))]
    Evaluation,
    #[cfg_attr(feature = "serde-impl", serde(rename = "source"))]
    Source,
    #[cfg_attr(feature = "serde-impl", serde(rename = "defer"))]
    Defer,
}

impl Take for ImportDecl {
    fn dummy() -> Self {
        ImportDecl {
            span: DUMMY_SP,
            specifiers: Take::dummy(),
            src: Take::dummy(),
            type_only: Default::default(),
            with: Take::dummy(),
            phase: Default::default(),
        }
    }
}

/// `export * from 'mod'`
#[ast_node("ExportAllDeclaration")]
#[derive(Eq, Hash, EqIgnoreSpan)]
#[cfg_attr(feature = "arbitrary", derive(arbitrary::Arbitrary))]
#[cfg_attr(feature = "shrink-to-fit", derive(shrink_to_fit::ShrinkToFit))]
pub struct ExportAll {
    pub span: Span,

    #[cfg_attr(feature = "serde-impl", serde(rename = "source"))]
    pub src: Box<Str>,

    #[cfg_attr(feature = "serde-impl", serde(rename = "typeOnly"))]
    pub type_only: bool,

    #[cfg_attr(feature = "serde-impl", serde(default))]
    #[cfg_attr(
        feature = "encoding-impl",
        encoding(with = "cbor4ii::core::types::Maybe")
    )]
    pub with: Option<Box<ObjectLit>>,
}

impl Take for ExportAll {
    fn dummy() -> Self {
        Self {
            span: DUMMY_SP,
            src: Take::dummy(),
            type_only: Default::default(),
            with: Take::dummy(),
        }
    }
}

/// `export { foo } from 'mod'`
/// `export { foo as bar } from 'mod'`
#[ast_node("ExportNamedDeclaration")]
#[derive(Eq, Hash, EqIgnoreSpan)]
#[cfg_attr(feature = "arbitrary", derive(arbitrary::Arbitrary))]
#[cfg_attr(feature = "shrink-to-fit", derive(shrink_to_fit::ShrinkToFit))]
pub struct NamedExport {
    pub span: Span,

    pub specifiers: Vec<ExportSpecifier>,

    #[cfg_attr(feature = "serde-impl", serde(rename = "source"))]
    #[cfg_attr(
        feature = "encoding-impl",
        encoding(with = "cbor4ii::core::types::Maybe")
    )]
    pub src: Option<Box<Str>>,

    #[cfg_attr(feature = "serde-impl", serde(rename = "typeOnly"))]
    pub type_only: bool,

    #[cfg_attr(feature = "serde-impl", serde(default))]
    #[cfg_attr(
        feature = "encoding-impl",
        encoding(with = "cbor4ii::core::types::Maybe")
    )]
    pub with: Option<Box<ObjectLit>>,
}

impl Take for NamedExport {
    fn dummy() -> Self {
        Self {
            span: DUMMY_SP,
            specifiers: Take::dummy(),
            src: Take::dummy(),
            type_only: Default::default(),
            with: Take::dummy(),
        }
    }
}

#[ast_node("ExportDefaultDeclaration")]
#[derive(Eq, Hash, EqIgnoreSpan)]
#[cfg_attr(feature = "arbitrary", derive(arbitrary::Arbitrary))]
#[cfg_attr(feature = "shrink-to-fit", derive(shrink_to_fit::ShrinkToFit))]
pub struct ExportDefaultDecl {
    pub span: Span,

    pub decl: DefaultDecl,
}

#[ast_node]
#[derive(Eq, Hash, Is, EqIgnoreSpan)]
#[cfg_attr(feature = "arbitrary", derive(arbitrary::Arbitrary))]
#[cfg_attr(feature = "shrink-to-fit", derive(shrink_to_fit::ShrinkToFit))]
pub enum DefaultDecl {
    #[tag("ClassExpression")]
    Class(ClassExpr),

    #[tag("FunctionExpression")]
    #[is(name = "fn_expr")]
    Fn(FnExpr),

    #[tag("TsInterfaceDeclaration")]
    TsInterfaceDecl(Box<TsInterfaceDecl>),
}

#[ast_node]
#[derive(Eq, Hash, Is, EqIgnoreSpan)]
#[cfg_attr(feature = "arbitrary", derive(arbitrary::Arbitrary))]
#[cfg_attr(feature = "shrink-to-fit", derive(shrink_to_fit::ShrinkToFit))]
pub enum ImportSpecifier {
    #[tag("ImportSpecifier")]
    Named(ImportNamedSpecifier),
    #[tag("ImportDefaultSpecifier")]
    Default(ImportDefaultSpecifier),
    #[tag("ImportNamespaceSpecifier")]
    Namespace(ImportStarAsSpecifier),
}

impl ImportSpecifier {
    pub fn is_type_only(&self) -> bool {
        match self {
            ImportSpecifier::Named(named) => named.is_type_only,
            ImportSpecifier::Default(..) | ImportSpecifier::Namespace(..) => false,
            #[cfg(all(swc_ast_unknown, feature = "encoding-impl"))]
            _ => swc_common::unknown!(),
        }
    }

    pub fn local(&self) -> &Ident {
        match self {
            ImportSpecifier::Named(named) => &named.local,
            ImportSpecifier::Default(default) => &default.local,
            ImportSpecifier::Namespace(ns) => &ns.local,
            #[cfg(all(swc_ast_unknown, feature = "encoding-impl"))]
            _ => swc_common::unknown!(),
        }
    }

    pub fn local_mut(&mut self) -> &mut Ident {
        match self {
            ImportSpecifier::Named(named) => &mut named.local,
            ImportSpecifier::Default(default) => &mut default.local,
            ImportSpecifier::Namespace(ns) => &mut ns.local,
            #[cfg(all(swc_ast_unknown, feature = "encoding-impl"))]
            _ => swc_common::unknown!(),
        }
    }
}

/// e.g. `import foo from 'mod.js'`
#[ast_node("ImportDefaultSpecifier")]
#[derive(Eq, Hash, EqIgnoreSpan)]
#[cfg_attr(feature = "arbitrary", derive(arbitrary::Arbitrary))]
#[cfg_attr(feature = "shrink-to-fit", derive(shrink_to_fit::ShrinkToFit))]
pub struct ImportDefaultSpecifier {
    pub span: Span,

    pub local: Ident,
}
/// e.g. `import * as foo from 'mod.js'`.
#[ast_node("ImportNamespaceSpecifier")]
#[derive(Eq, Hash, EqIgnoreSpan)]
#[cfg_attr(feature = "arbitrary", derive(arbitrary::Arbitrary))]
#[cfg_attr(feature = "shrink-to-fit", derive(shrink_to_fit::ShrinkToFit))]
pub struct ImportStarAsSpecifier {
    pub span: Span,

    pub local: Ident,
}
/// e.g. local = foo, imported = None `import { foo } from 'mod.js'`
/// e.g. local = bar, imported = Some(foo) for `import { foo as bar } from
/// 'mod.js'`
#[ast_node("ImportSpecifier")]
#[derive(Eq, Hash, EqIgnoreSpan)]
#[cfg_attr(feature = "arbitrary", derive(arbitrary::Arbitrary))]
#[cfg_attr(feature = "shrink-to-fit", derive(shrink_to_fit::ShrinkToFit))]
pub struct ImportNamedSpecifier {
    pub span: Span,

    pub local: Ident,

    #[cfg_attr(feature = "serde-impl", serde(default))]
    #[cfg_attr(
        feature = "encoding-impl",
        encoding(with = "cbor4ii::core::types::Maybe")
    )]
    pub imported: Option<ModuleExportName>,

    #[cfg_attr(feature = "serde-impl", serde(default))]
    pub is_type_only: bool,
}

#[ast_node]
#[derive(Eq, Hash, Is, EqIgnoreSpan)]
#[cfg_attr(feature = "arbitrary", derive(arbitrary::Arbitrary))]
#[cfg_attr(feature = "shrink-to-fit", derive(shrink_to_fit::ShrinkToFit))]
pub enum ExportSpecifier {
    #[tag("ExportNamespaceSpecifier")]
    Namespace(ExportNamespaceSpecifier),

    #[tag("ExportDefaultSpecifier")]
    Default(ExportDefaultSpecifier),

    #[tag("ExportSpecifier")]
    Named(ExportNamedSpecifier),
}

/// `export * as foo from 'src';`
#[ast_node("ExportNamespaceSpecifier")]
#[derive(Eq, Hash, EqIgnoreSpan)]
#[cfg_attr(feature = "arbitrary", derive(arbitrary::Arbitrary))]
#[cfg_attr(feature = "shrink-to-fit", derive(shrink_to_fit::ShrinkToFit))]
pub struct ExportNamespaceSpecifier {
    pub span: Span,

    pub name: ModuleExportName,
}

// export v from 'mod';
#[ast_node("ExportDefaultSpecifier")]
#[derive(Eq, Hash, EqIgnoreSpan)]
#[cfg_attr(feature = "arbitrary", derive(arbitrary::Arbitrary))]
#[cfg_attr(feature = "shrink-to-fit", derive(shrink_to_fit::ShrinkToFit))]
pub struct ExportDefaultSpecifier {
    #[span]
    pub exported: Ident,
}

#[ast_node("ExportSpecifier")]
#[derive(Eq, Hash, EqIgnoreSpan)]
#[cfg_attr(feature = "arbitrary", derive(arbitrary::Arbitrary))]
#[cfg_attr(feature = "shrink-to-fit", derive(shrink_to_fit::ShrinkToFit))]
pub struct ExportNamedSpecifier {
    pub span: Span,
    /// `foo` in `export { foo as bar }`
    pub orig: ModuleExportName,
    /// `Some(bar)` in `export { foo as bar }`
    #[cfg_attr(feature = "serde-impl", serde(default))]
    #[cfg_attr(
        feature = "encoding-impl",
        encoding(with = "cbor4ii::core::types::Maybe")
    )]
    pub exported: Option<ModuleExportName>,
    /// `type` in `export { type foo as bar }`
    #[cfg_attr(feature = "serde-impl", serde(default))]
    pub is_type_only: bool,
}

#[ast_node]
#[derive(Eq, Hash, EqIgnoreSpan)]
#[cfg_attr(feature = "arbitrary", derive(arbitrary::Arbitrary))]
#[cfg_attr(feature = "shrink-to-fit", derive(shrink_to_fit::ShrinkToFit))]
// https://tc39.es/ecma262/#prod-ModuleExportName
pub enum ModuleExportName {
    #[tag("Identifier")]
    Ident(Ident),

    #[tag("StringLiteral")]
    Str(Str),
}

bridge_from!(ModuleExportName, Ident, BindingIdent);
bridge_from!(ModuleExportName, Ident, IdentName);

impl ModuleExportName {
    /// Get the atom of the export name.
    ///
    /// If the name is a string literal that has ill-formed UTF16, it will be
    /// converted to a UTF-8 valid string using lossy conversion (Unpaired
    /// surrogates are replaced with Replacement Character).  This is
    /// a SyntaxError if it's fully complied to the spec.
    /// See: https://tc39.es/ecma262/#sec-module-semantics-static-semantics-early-errors
    pub fn atom(&self) -> Cow<'_, Atom> {
        match self {
            ModuleExportName::Ident(i) => Cow::Borrowed(&i.sym),
            ModuleExportName::Str(s) => match s.value.clone().try_into_atom() {
                Ok(atom) => Cow::Owned(atom),
                Err(original) => Cow::Owned(Atom::from(&*original.to_string_lossy())),
            },
            #[cfg(all(swc_ast_unknown, feature = "encoding-impl"))]
            _ => swc_common::unknown!(),
        }
    }
}
