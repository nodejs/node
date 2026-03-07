use is_macro::Is;
use swc_atoms::Atom;
use swc_common::{ast_node, util::take::Take, EqIgnoreSpan, Span, DUMMY_SP};

use crate::{module_decl::ModuleDecl, stmt::Stmt};

#[ast_node]
#[derive(Eq, Hash, Is, EqIgnoreSpan)]
#[cfg_attr(feature = "arbitrary", derive(arbitrary::Arbitrary))]
#[cfg_attr(feature = "shrink-to-fit", derive(shrink_to_fit::ShrinkToFit))]
pub enum Program {
    #[tag("Module")]
    Module(Module),
    #[tag("Script")]
    Script(Script),
}

impl Take for Program {
    fn dummy() -> Self {
        Program::Script(Script::default())
    }
}

#[ast_node("Module")]
#[derive(Eq, Hash, EqIgnoreSpan, Default)]
#[cfg_attr(feature = "shrink-to-fit", derive(shrink_to_fit::ShrinkToFit))]
pub struct Module {
    pub span: Span,

    pub body: Vec<ModuleItem>,

    #[cfg_attr(feature = "serde-impl", serde(default, rename = "interpreter"))]
    #[cfg_attr(
        feature = "encoding-impl",
        encoding(with = "cbor4ii::core::types::Maybe")
    )]
    pub shebang: Option<Atom>,
}

#[cfg(feature = "arbitrary")]
#[cfg_attr(docsrs, doc(cfg(feature = "arbitrary")))]
impl<'a> arbitrary::Arbitrary<'a> for Module {
    fn arbitrary(u: &mut arbitrary::Unstructured<'_>) -> arbitrary::Result<Self> {
        let span = u.arbitrary()?;
        let body = u.arbitrary()?;
        Ok(Self {
            span,
            body,
            shebang: None,
        })
    }
}

impl Take for Module {
    fn dummy() -> Self {
        Module {
            span: DUMMY_SP,
            body: Take::dummy(),
            shebang: Take::dummy(),
        }
    }
}

#[ast_node("Script")]
#[derive(Eq, Hash, EqIgnoreSpan, Default)]
#[cfg_attr(feature = "shrink-to-fit", derive(shrink_to_fit::ShrinkToFit))]
pub struct Script {
    pub span: Span,

    pub body: Vec<Stmt>,

    #[cfg_attr(feature = "serde-impl", serde(default, rename = "interpreter"))]
    #[cfg_attr(
        feature = "encoding-impl",
        encoding(with = "cbor4ii::core::types::Maybe")
    )]
    pub shebang: Option<Atom>,
}

#[cfg(feature = "arbitrary")]
#[cfg_attr(docsrs, doc(cfg(feature = "arbitrary")))]
impl<'a> arbitrary::Arbitrary<'a> for Script {
    fn arbitrary(u: &mut arbitrary::Unstructured<'_>) -> arbitrary::Result<Self> {
        let span = u.arbitrary()?;
        let body = u.arbitrary()?;
        Ok(Self {
            span,
            body,
            shebang: None,
        })
    }
}

impl Take for Script {
    fn dummy() -> Self {
        Script {
            span: DUMMY_SP,
            body: Take::dummy(),
            shebang: Take::dummy(),
        }
    }
}

#[ast_node]
#[derive(Eq, Hash, Is, EqIgnoreSpan)]
#[cfg_attr(feature = "arbitrary", derive(arbitrary::Arbitrary))]
#[cfg_attr(feature = "shrink-to-fit", derive(shrink_to_fit::ShrinkToFit))]
pub enum ModuleItem {
    #[tag("ImportDeclaration")]
    #[tag("ExportDeclaration")]
    #[tag("ExportNamedDeclaration")]
    #[tag("ExportDefaultDeclaration")]
    #[tag("ExportDefaultExpression")]
    #[tag("ExportAllDeclaration")]
    #[tag("TsImportEqualsDeclaration")]
    #[tag("TsExportAssignment")]
    #[tag("TsNamespaceExportDeclaration")]
    ModuleDecl(ModuleDecl),
    #[tag("*")]
    Stmt(Stmt),
}

impl Take for ModuleItem {
    fn dummy() -> Self {
        Self::Stmt(Default::default())
    }
}
