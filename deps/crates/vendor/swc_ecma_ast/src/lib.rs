#![cfg_attr(docsrs, feature(doc_cfg))]
#![deny(unreachable_patterns)]
#![deny(missing_copy_implementations)]
#![deny(trivial_casts)]
#![deny(trivial_numeric_casts)]
#![deny(unreachable_pub)]
#![deny(clippy::all)]
#![allow(clippy::enum_variant_names)]
#![allow(clippy::clone_on_copy)]
#![recursion_limit = "1024"]

pub use num_bigint::BigInt as BigIntValue;
#[cfg(feature = "serde")]
use serde::{Deserialize, Serialize};
use swc_common::{ast_node, pass::Either, util::take::Take, EqIgnoreSpan, Span};

pub use self::{
    class::{
        AutoAccessor, Class, ClassMember, ClassMethod, ClassProp, Constructor, Decorator, Key,
        MethodKind, PrivateMethod, PrivateProp, StaticBlock,
    },
    decl::{ClassDecl, Decl, FnDecl, UsingDecl, VarDecl, VarDeclKind, VarDeclarator},
    expr::*,
    function::{Function, Param, ParamOrTsParamProp},
    ident::{
        unsafe_id, unsafe_id_from_ident, BindingIdent, EsReserved, Id, Ident, IdentName,
        PrivateName, UnsafeId,
    },
    jsx::{
        JSXAttr, JSXAttrName, JSXAttrOrSpread, JSXAttrValue, JSXClosingElement, JSXClosingFragment,
        JSXElement, JSXElementChild, JSXElementName, JSXEmptyExpr, JSXExpr, JSXExprContainer,
        JSXFragment, JSXMemberExpr, JSXNamespacedName, JSXObject, JSXOpeningElement,
        JSXOpeningFragment, JSXSpreadChild, JSXText,
    },
    list::ListFormat,
    lit::{BigInt, Bool, Lit, Null, Number, Regex, Str},
    module::{Module, ModuleItem, Program, Script},
    module_decl::{
        DefaultDecl, ExportAll, ExportDecl, ExportDefaultDecl, ExportDefaultExpr,
        ExportDefaultSpecifier, ExportNamedSpecifier, ExportNamespaceSpecifier, ExportSpecifier,
        ImportDecl, ImportDefaultSpecifier, ImportNamedSpecifier, ImportPhase, ImportSpecifier,
        ImportStarAsSpecifier, ModuleDecl, ModuleExportName, NamedExport,
    },
    operators::{AssignOp, BinaryOp, UnaryOp, UpdateOp},
    pat::{
        ArrayPat, AssignPat, AssignPatProp, KeyValuePatProp, ObjectPat, ObjectPatProp, Pat, RestPat,
    },
    prop::{
        AssignProp, ComputedPropName, GetterProp, KeyValueProp, MethodProp, Prop, PropName,
        SetterProp,
    },
    source_map::{SourceMapperExt, SpanExt},
    stmt::{
        BlockStmt, BreakStmt, CatchClause, ContinueStmt, DebuggerStmt, DoWhileStmt, EmptyStmt,
        ExprStmt, ForHead, ForInStmt, ForOfStmt, ForStmt, IfStmt, LabeledStmt, ReturnStmt, Stmt,
        SwitchCase, SwitchStmt, ThrowStmt, TryStmt, VarDeclOrExpr, WhileStmt, WithStmt,
    },
    typescript::{
        Accessibility, TruePlusMinus, TsArrayType, TsAsExpr, TsCallSignatureDecl,
        TsConditionalType, TsConstAssertion, TsConstructSignatureDecl, TsConstructorType,
        TsEntityName, TsEnumDecl, TsEnumMember, TsEnumMemberId, TsExportAssignment,
        TsExprWithTypeArgs, TsExternalModuleRef, TsFnOrConstructorType, TsFnParam, TsFnType,
        TsGetterSignature, TsImportCallOptions, TsImportEqualsDecl, TsImportType, TsIndexSignature,
        TsIndexedAccessType, TsInferType, TsInstantiation, TsInterfaceBody, TsInterfaceDecl,
        TsIntersectionType, TsKeywordType, TsKeywordTypeKind, TsLit, TsLitType, TsMappedType,
        TsMethodSignature, TsModuleBlock, TsModuleDecl, TsModuleName, TsModuleRef, TsNamespaceBody,
        TsNamespaceDecl, TsNamespaceExportDecl, TsNonNullExpr, TsOptionalType, TsParamProp,
        TsParamPropParam, TsParenthesizedType, TsPropertySignature, TsQualifiedName, TsRestType,
        TsSatisfiesExpr, TsSetterSignature, TsThisType, TsThisTypeOrIdent, TsTplLitType,
        TsTupleElement, TsTupleType, TsType, TsTypeAliasDecl, TsTypeAnn, TsTypeAssertion,
        TsTypeElement, TsTypeLit, TsTypeOperator, TsTypeOperatorOp, TsTypeParam, TsTypeParamDecl,
        TsTypeParamInstantiation, TsTypePredicate, TsTypeQuery, TsTypeQueryExpr, TsTypeRef,
        TsUnionOrIntersectionType, TsUnionType,
    },
};

#[macro_use]
mod macros;
mod class;
mod decl;
mod expr;
mod function;
mod ident;
mod jsx;
mod list;
mod lit;
mod module;
mod module_decl;
mod operators;
mod pat;
mod prop;
mod source_map;
mod stmt;
mod typescript;

/// A map from the [Program] to the [Program].
///
/// This trait is used to implement transformations. The implementor may decide
/// to implement [Fold] or [VisitMut] if the transform is fine to start from an
/// arbitrary node.
///
/// Tuple of [Pass] implementations also implements [Pass], but it's limited to
/// 12 items for fast compile time. If you have more passes, nest it like `(a,
/// (b, c), (d, e))`
pub trait Pass {
    fn process(&mut self, program: &mut Program);
}

/// Optional pass implementation.
impl<P> Pass for Option<P>
where
    P: Pass,
{
    #[inline(always)]
    fn process(&mut self, program: &mut Program) {
        if let Some(pass) = self {
            pass.process(program);
        }
    }
}

impl<P: ?Sized> Pass for Box<P>
where
    P: Pass,
{
    #[inline(always)]
    fn process(&mut self, program: &mut Program) {
        (**self).process(program);
    }
}

impl<P: ?Sized> Pass for &'_ mut P
where
    P: Pass,
{
    #[inline(always)]
    fn process(&mut self, program: &mut Program) {
        (**self).process(program);
    }
}

impl<L, R> Pass for Either<L, R>
where
    L: Pass,
    R: Pass,
{
    #[inline]
    fn process(&mut self, program: &mut Program) {
        match self {
            Either::Left(l) => l.process(program),
            Either::Right(r) => r.process(program),
        }
    }
}

impl<P> Pass for swc_visit::Optional<P>
where
    P: Pass,
{
    #[inline]
    fn process(&mut self, program: &mut Program) {
        if self.enabled {
            self.visitor.process(program);
        }
    }
}

impl<P> Pass for swc_visit::Repeat<P>
where
    P: Pass + swc_visit::Repeated,
{
    #[inline]
    fn process(&mut self, program: &mut Program) {
        loop {
            self.pass.reset();
            self.pass.process(program);

            if !self.pass.changed() {
                break;
            }
        }
    }
}

impl Program {
    #[inline(always)]
    pub fn mutate<P>(&mut self, mut pass: P)
    where
        P: Pass,
    {
        pass.process(self);
    }

    #[inline(always)]
    pub fn apply<P>(mut self, mut pass: P) -> Self
    where
        P: Pass,
    {
        pass.process(&mut self);
        self
    }
}

macro_rules! impl_pass_for_tuple {
    (
        [$idx:tt, $name:ident], $([$idx_rest:tt, $name_rest:ident]),*
    ) => {
        impl<$name, $($name_rest),*> Pass for ($name, $($name_rest),*)
        where
            $name: Pass,
            $($name_rest: Pass),*
        {
            #[inline]
            fn process(&mut self, program: &mut Program) {
                self.$idx.process(program);

                $(
                    self.$idx_rest.process(program);
                )*

            }
        }
    };
}

impl_pass_for_tuple!([0, A], [1, B]);
impl_pass_for_tuple!([0, A], [1, B], [2, C]);
impl_pass_for_tuple!([0, A], [1, B], [2, C], [3, D]);
impl_pass_for_tuple!([0, A], [1, B], [2, C], [3, D], [4, E]);
impl_pass_for_tuple!([0, A], [1, B], [2, C], [3, D], [4, E], [5, F]);
impl_pass_for_tuple!([0, A], [1, B], [2, C], [3, D], [4, E], [5, F], [6, G]);
impl_pass_for_tuple!(
    [0, A],
    [1, B],
    [2, C],
    [3, D],
    [4, E],
    [5, F],
    [6, G],
    [7, H]
);
impl_pass_for_tuple!(
    [0, A],
    [1, B],
    [2, C],
    [3, D],
    [4, E],
    [5, F],
    [6, G],
    [7, H],
    [8, I]
);
impl_pass_for_tuple!(
    [0, A],
    [1, B],
    [2, C],
    [3, D],
    [4, E],
    [5, F],
    [6, G],
    [7, H],
    [8, I],
    [9, J]
);
impl_pass_for_tuple!(
    [0, A],
    [1, B],
    [2, C],
    [3, D],
    [4, E],
    [5, F],
    [6, G],
    [7, H],
    [8, I],
    [9, J],
    [10, K]
);
impl_pass_for_tuple!(
    [0, A],
    [1, B],
    [2, C],
    [3, D],
    [4, E],
    [5, F],
    [6, G],
    [7, H],
    [8, I],
    [9, J],
    [10, K],
    [11, L]
);
impl_pass_for_tuple!(
    [0, A],
    [1, B],
    [2, C],
    [3, D],
    [4, E],
    [5, F],
    [6, G],
    [7, H],
    [8, I],
    [9, J],
    [10, K],
    [11, L],
    [12, M]
);

#[inline(always)]
pub fn noop_pass() -> impl Pass {
    fn noop(_: &mut Program) {}

    fn_pass(noop)
}

#[inline(always)]
pub fn fn_pass(f: impl FnMut(&mut Program)) -> impl Pass {
    FnPass { f }
}

struct FnPass<F> {
    f: F,
}

impl<F> Pass for FnPass<F>
where
    F: FnMut(&mut Program),
{
    fn process(&mut self, program: &mut Program) {
        (self.f)(program);
    }
}

/// Represents a invalid node.
#[ast_node("Invalid")]
#[derive(Eq, Default, Hash, Copy, EqIgnoreSpan)]
#[cfg_attr(feature = "arbitrary", derive(arbitrary::Arbitrary))]
#[cfg_attr(feature = "shrink-to-fit", derive(shrink_to_fit::ShrinkToFit))]
pub struct Invalid {
    pub span: Span,
}

impl Take for Invalid {
    fn dummy() -> Self {
        Invalid::default()
    }
}

/// Note: This type implements `Serailize` and `Deserialize` if `serde` is
/// enabled, instead of requiring `serde-impl` feature.
#[derive(Debug, Default, Clone, Copy, PartialOrd, Ord, PartialEq, Eq, Hash)]
#[cfg_attr(feature = "serde", derive(Serialize))]
#[cfg_attr(feature = "serde", serde(rename_all = "lowercase"))]
pub enum EsVersion {
    Es3,
    #[default]
    Es5,
    Es2015,
    Es2016,
    Es2017,
    Es2018,
    Es2019,
    Es2020,
    Es2021,
    Es2022,
    Es2023,
    Es2024,
    EsNext,
}

#[cfg(feature = "serde")]
impl<'de> Deserialize<'de> for EsVersion {
    fn deserialize<D>(deserializer: D) -> Result<Self, D::Error>
    where
        D: serde::Deserializer<'de>,
    {
        use serde::de::Error;

        let s = String::deserialize(deserializer)?;
        match s.to_lowercase().as_str() {
            "es3" => Ok(EsVersion::Es3),
            "es5" => Ok(EsVersion::Es5),
            "es2015" | "es6" => Ok(EsVersion::Es2015),
            "es2016" => Ok(EsVersion::Es2016),
            "es2017" => Ok(EsVersion::Es2017),
            "es2018" => Ok(EsVersion::Es2018),
            "es2019" => Ok(EsVersion::Es2019),
            "es2020" => Ok(EsVersion::Es2020),
            "es2021" => Ok(EsVersion::Es2021),
            "es2022" => Ok(EsVersion::Es2022),
            "es2023" => Ok(EsVersion::Es2023),
            "es2024" => Ok(EsVersion::Es2024),
            "esnext" => Ok(EsVersion::EsNext),
            _ => Err(D::Error::custom(format!("Unknown ES version: {s}"))),
        }
    }
}

impl EsVersion {
    pub const fn latest() -> Self {
        EsVersion::EsNext
    }
}

/// Warning: The particular implementation of serialization and deserialization
/// of the ast nodes may change in the future, and so these types would be
/// removed. It's safe to say they will be serializable in some form or another,
/// but not necessarily with these specific types underlying the implementation.
/// As such, *use these types at your own risk*.
#[cfg(feature = "rkyv-impl")]
#[doc(hidden)]
pub use self::{
    class::{
        ArchivedAutoAccessor, ArchivedClass, ArchivedClassMember, ArchivedClassMethod,
        ArchivedClassProp, ArchivedConstructor, ArchivedDecorator, ArchivedKey, ArchivedMethodKind,
        ArchivedPrivateMethod, ArchivedPrivateProp, ArchivedStaticBlock,
    },
    decl::{
        ArchivedClassDecl, ArchivedDecl, ArchivedFnDecl, ArchivedUsingDecl, ArchivedVarDecl,
        ArchivedVarDeclKind, ArchivedVarDeclarator,
    },
    expr::{
        ArchivedArrayLit, ArchivedArrowExpr, ArchivedAssignExpr, ArchivedAssignTarget,
        ArchivedAwaitExpr, ArchivedBinExpr, ArchivedBlockStmtOrExpr, ArchivedCallExpr,
        ArchivedCallee, ArchivedClassExpr, ArchivedCondExpr, ArchivedExpr, ArchivedExprOrSpread,
        ArchivedFnExpr, ArchivedImport, ArchivedMemberExpr, ArchivedMemberProp,
        ArchivedMetaPropExpr, ArchivedMetaPropKind, ArchivedNewExpr, ArchivedObjectLit,
        ArchivedOptCall, ArchivedOptChainBase, ArchivedOptChainExpr, ArchivedParenExpr,
        ArchivedPropOrSpread, ArchivedSeqExpr, ArchivedSpreadElement, ArchivedSuper,
        ArchivedSuperProp, ArchivedSuperPropExpr, ArchivedTaggedTpl, ArchivedThisExpr, ArchivedTpl,
        ArchivedTplElement, ArchivedUnaryExpr, ArchivedUpdateExpr, ArchivedYieldExpr,
    },
    function::{ArchivedFunction, ArchivedParam, ArchivedParamOrTsParamProp},
    ident::{ArchivedBindingIdent, ArchivedIdent, ArchivedIdentName, ArchivedPrivateName},
    jsx::{
        ArchivedJSXAttr, ArchivedJSXAttrName, ArchivedJSXAttrOrSpread, ArchivedJSXAttrValue,
        ArchivedJSXClosingElement, ArchivedJSXClosingFragment, ArchivedJSXElement,
        ArchivedJSXElementChild, ArchivedJSXElementName, ArchivedJSXEmptyExpr, ArchivedJSXExpr,
        ArchivedJSXExprContainer, ArchivedJSXFragment, ArchivedJSXMemberExpr,
        ArchivedJSXNamespacedName, ArchivedJSXObject, ArchivedJSXOpeningElement,
        ArchivedJSXOpeningFragment, ArchivedJSXSpreadChild, ArchivedJSXText,
    },
    lit::{
        ArchivedBigInt, ArchivedBool, ArchivedLit, ArchivedNull, ArchivedNumber, ArchivedRegex,
        ArchivedStr,
    },
    module::{ArchivedModule, ArchivedModuleItem, ArchivedProgram, ArchivedScript},
    module_decl::{
        ArchivedDefaultDecl, ArchivedExportAll, ArchivedExportDecl, ArchivedExportDefaultDecl,
        ArchivedExportDefaultExpr, ArchivedExportDefaultSpecifier, ArchivedExportNamedSpecifier,
        ArchivedExportNamespaceSpecifier, ArchivedExportSpecifier, ArchivedImportDecl,
        ArchivedImportDefaultSpecifier, ArchivedImportNamedSpecifier, ArchivedImportSpecifier,
        ArchivedImportStarAsSpecifier, ArchivedModuleDecl, ArchivedModuleExportName,
        ArchivedNamedExport,
    },
    operators::{ArchivedAssignOp, ArchivedBinaryOp, ArchivedUnaryOp, ArchivedUpdateOp},
    pat::{
        ArchivedArrayPat, ArchivedAssignPat, ArchivedAssignPatProp, ArchivedKeyValuePatProp,
        ArchivedObjectPat, ArchivedObjectPatProp, ArchivedPat, ArchivedRestPat,
    },
    prop::{
        ArchivedAssignProp, ArchivedComputedPropName, ArchivedGetterProp, ArchivedKeyValueProp,
        ArchivedMethodProp, ArchivedProp, ArchivedPropName, ArchivedSetterProp,
    },
    stmt::{
        ArchivedBlockStmt, ArchivedBreakStmt, ArchivedCatchClause, ArchivedContinueStmt,
        ArchivedDebuggerStmt, ArchivedDoWhileStmt, ArchivedEmptyStmt, ArchivedExprStmt,
        ArchivedForHead, ArchivedForInStmt, ArchivedForOfStmt, ArchivedForStmt, ArchivedIfStmt,
        ArchivedLabeledStmt, ArchivedReturnStmt, ArchivedStmt, ArchivedSwitchCase,
        ArchivedSwitchStmt, ArchivedThrowStmt, ArchivedTryStmt, ArchivedVarDeclOrExpr,
        ArchivedWhileStmt, ArchivedWithStmt,
    },
    typescript::{
        ArchivedAccessibility, ArchivedTruePlusMinus, ArchivedTsArrayType, ArchivedTsAsExpr,
        ArchivedTsCallSignatureDecl, ArchivedTsConditionalType, ArchivedTsConstAssertion,
        ArchivedTsConstructSignatureDecl, ArchivedTsConstructorType, ArchivedTsEntityName,
        ArchivedTsEnumDecl, ArchivedTsEnumMember, ArchivedTsEnumMemberId,
        ArchivedTsExportAssignment, ArchivedTsExprWithTypeArgs, ArchivedTsExternalModuleRef,
        ArchivedTsFnOrConstructorType, ArchivedTsFnParam, ArchivedTsFnType,
        ArchivedTsGetterSignature, ArchivedTsImportEqualsDecl, ArchivedTsImportType,
        ArchivedTsIndexSignature, ArchivedTsIndexedAccessType, ArchivedTsInferType,
        ArchivedTsInstantiation, ArchivedTsInterfaceBody, ArchivedTsInterfaceDecl,
        ArchivedTsIntersectionType, ArchivedTsKeywordType, ArchivedTsKeywordTypeKind,
        ArchivedTsLit, ArchivedTsLitType, ArchivedTsMappedType, ArchivedTsMethodSignature,
        ArchivedTsModuleBlock, ArchivedTsModuleDecl, ArchivedTsModuleName, ArchivedTsModuleRef,
        ArchivedTsNamespaceBody, ArchivedTsNamespaceDecl, ArchivedTsNamespaceExportDecl,
        ArchivedTsNonNullExpr, ArchivedTsOptionalType, ArchivedTsParamProp,
        ArchivedTsParamPropParam, ArchivedTsParenthesizedType, ArchivedTsPropertySignature,
        ArchivedTsQualifiedName, ArchivedTsRestType, ArchivedTsSatisfiesExpr,
        ArchivedTsSetterSignature, ArchivedTsThisType, ArchivedTsThisTypeOrIdent,
        ArchivedTsTplLitType, ArchivedTsTupleElement, ArchivedTsTupleType, ArchivedTsType,
        ArchivedTsTypeAliasDecl, ArchivedTsTypeAnn, ArchivedTsTypeAssertion, ArchivedTsTypeElement,
        ArchivedTsTypeLit, ArchivedTsTypeOperator, ArchivedTsTypeOperatorOp, ArchivedTsTypeParam,
        ArchivedTsTypeParamDecl, ArchivedTsTypeParamInstantiation, ArchivedTsTypePredicate,
        ArchivedTsTypeQuery, ArchivedTsTypeQueryExpr, ArchivedTsTypeRef,
        ArchivedTsUnionOrIntersectionType, ArchivedTsUnionType,
    },
};

#[cfg(feature = "rkyv-impl")]
mod rkyv_layout_assert {
    use crate::*;

    macro_rules! assert_size {
        ($T:ty, $size:expr) => {
            const _: fn() = || {
                let _ = std::mem::transmute::<$T, [u8; $size]>;
            };
        };
    }

    assert_size!(ArchivedProgram, 32);
    assert_size!(ArchivedModule, 28);
    assert_size!(ArchivedScript, 28);
    assert_size!(ArchivedModuleItem, 52);

    // Class types
    assert_size!(ArchivedAutoAccessor, 96);
    assert_size!(ArchivedClass, 64);
    assert_size!(ArchivedClassMember, 104);
    assert_size!(ArchivedClassMethod, 64);
    assert_size!(ArchivedClassProp, 88);
    assert_size!(ArchivedConstructor, 96);
    assert_size!(ArchivedDecorator, 12);
    assert_size!(ArchivedKey, 48);
    assert_size!(ArchivedMethodKind, 1);
    assert_size!(ArchivedPrivateMethod, 36);
    assert_size!(ArchivedPrivateProp, 64);
    assert_size!(ArchivedStaticBlock, 28);

    // Declaration types
    assert_size!(ArchivedClassDecl, 32);
    assert_size!(ArchivedDecl, 36);
    assert_size!(ArchivedFnDecl, 32);
    assert_size!(ArchivedUsingDecl, 20);
    assert_size!(ArchivedVarDecl, 24);
    assert_size!(ArchivedVarDeclKind, 1);
    assert_size!(ArchivedVarDeclarator, 56);

    // Expression types
    assert_size!(ArchivedArrayLit, 16);
    assert_size!(ArchivedArrowExpr, 44);
    assert_size!(ArchivedAssignExpr, 60);
    assert_size!(ArchivedAssignTarget, 44);
    assert_size!(ArchivedAwaitExpr, 12);
    assert_size!(ArchivedBinExpr, 20);
    assert_size!(ArchivedBlockStmtOrExpr, 24);
    assert_size!(ArchivedCallExpr, 44);
    assert_size!(ArchivedCallee, 16);
    assert_size!(ArchivedClassExpr, 32);
    assert_size!(ArchivedCondExpr, 20);
    assert_size!(ArchivedExpr, 64);
    assert_size!(ArchivedExprOrSpread, 16);
    assert_size!(ArchivedFnExpr, 32);
    assert_size!(ArchivedImport, 12);
    assert_size!(ArchivedMemberExpr, 32);
    assert_size!(ArchivedMemberProp, 20);
    assert_size!(ArchivedMetaPropExpr, 12);
    assert_size!(ArchivedMetaPropKind, 1);
    assert_size!(ArchivedNewExpr, 36);
    assert_size!(ArchivedObjectLit, 16);
    assert_size!(ArchivedOptCall, 32);
    assert_size!(ArchivedOptChainBase, 36);
    assert_size!(ArchivedOptChainExpr, 16);
    assert_size!(ArchivedParenExpr, 12);
    assert_size!(ArchivedPropOrSpread, 16);
    assert_size!(ArchivedSeqExpr, 16);
    assert_size!(ArchivedSpreadElement, 12);
    assert_size!(ArchivedSuper, 8);
    assert_size!(ArchivedSuperProp, 20);
    assert_size!(ArchivedSuperPropExpr, 36);
    assert_size!(ArchivedTaggedTpl, 28);
    assert_size!(ArchivedThisExpr, 8);
    assert_size!(ArchivedTpl, 24);
    assert_size!(ArchivedTplElement, 32);
    assert_size!(ArchivedUnaryExpr, 16);
    assert_size!(ArchivedUpdateExpr, 16);
    assert_size!(ArchivedYieldExpr, 20);

    // Function types
    assert_size!(ArchivedFunction, 72);
    assert_size!(ArchivedParam, 52);
    assert_size!(ArchivedParamOrTsParamProp, 60);

    // Identifier types
    assert_size!(ArchivedBindingIdent, 32);
    assert_size!(ArchivedIdent, 24);
    assert_size!(ArchivedIdentName, 16);
    assert_size!(ArchivedPrivateName, 16);

    // JSX types
    assert_size!(ArchivedJSXAttr, 92);
    assert_size!(ArchivedJSXAttrName, 44);
    assert_size!(ArchivedJSXAttrOrSpread, 96);
    assert_size!(ArchivedJSXAttrValue, 36);
    assert_size!(ArchivedJSXClosingElement, 64);
    assert_size!(ArchivedJSXClosingFragment, 8);
    assert_size!(ArchivedJSXElement, 168);
    assert_size!(ArchivedJSXElementChild, 36);
    assert_size!(ArchivedJSXElementName, 56);
    assert_size!(ArchivedJSXEmptyExpr, 8);
    assert_size!(ArchivedJSXExpr, 12);
    assert_size!(ArchivedJSXExprContainer, 20);
    assert_size!(ArchivedJSXFragment, 32);
    assert_size!(ArchivedJSXMemberExpr, 52);
    assert_size!(ArchivedJSXNamespacedName, 40);
    assert_size!(ArchivedJSXObject, 28);
    assert_size!(ArchivedJSXOpeningElement, 84);
    assert_size!(ArchivedJSXOpeningFragment, 8);
    assert_size!(ArchivedJSXSpreadChild, 12);
    assert_size!(ArchivedJSXText, 24);

    // Literal types
    assert_size!(ArchivedBigInt, 28);
    assert_size!(ArchivedBool, 12);
    assert_size!(ArchivedLit, 40);
    assert_size!(ArchivedNull, 8);
    assert_size!(ArchivedNumber, 32);
    assert_size!(ArchivedRegex, 24);
    assert_size!(ArchivedStr, 28);

    // Module declaration types
    assert_size!(ArchivedDefaultDecl, 36);
    assert_size!(ArchivedExportAll, 24);
    assert_size!(ArchivedExportDecl, 44);
    assert_size!(ArchivedExportDefaultDecl, 44);
    assert_size!(ArchivedExportDefaultExpr, 12);
    assert_size!(ArchivedExportDefaultSpecifier, 24);
    assert_size!(ArchivedExportNamedSpecifier, 80);
    assert_size!(ArchivedExportNamespaceSpecifier, 40);
    assert_size!(ArchivedExportSpecifier, 84);
    assert_size!(ArchivedImportDecl, 36);
    assert_size!(ArchivedImportDefaultSpecifier, 32);
    assert_size!(ArchivedImportNamedSpecifier, 72);
    assert_size!(ArchivedImportSpecifier, 76);
    assert_size!(ArchivedImportStarAsSpecifier, 32);
    assert_size!(ArchivedModuleDecl, 48);
    assert_size!(ArchivedModuleExportName, 32);
    assert_size!(ArchivedNamedExport, 36);

    // Operator types
    assert_size!(ArchivedAssignOp, 1);
    assert_size!(ArchivedBinaryOp, 1);
    assert_size!(ArchivedUnaryOp, 1);
    assert_size!(ArchivedUpdateOp, 1);

    // Pattern types
    assert_size!(ArchivedArrayPat, 28);
    assert_size!(ArchivedAssignPat, 16);
    assert_size!(ArchivedAssignPatProp, 48);
    assert_size!(ArchivedKeyValuePatProp, 48);
    assert_size!(ArchivedObjectPat, 28);
    assert_size!(ArchivedObjectPatProp, 56);
    assert_size!(ArchivedPat, 36);
    assert_size!(ArchivedRestPat, 28);

    // Property types
    assert_size!(ArchivedAssignProp, 36);
    assert_size!(ArchivedComputedPropName, 12);
    assert_size!(ArchivedGetterProp, 80);
    assert_size!(ArchivedKeyValueProp, 48);
    assert_size!(ArchivedMethodProp, 48);
    assert_size!(ArchivedProp, 128);
    assert_size!(ArchivedPropName, 40);
    assert_size!(ArchivedSetterProp, 120);

    // Statement types
    assert_size!(ArchivedBlockStmt, 20);
    assert_size!(ArchivedBreakStmt, 36);
    assert_size!(ArchivedCatchClause, 68);
    assert_size!(ArchivedContinueStmt, 36);
    assert_size!(ArchivedDebuggerStmt, 8);
    assert_size!(ArchivedDoWhileStmt, 16);
    assert_size!(ArchivedEmptyStmt, 8);
    assert_size!(ArchivedExprStmt, 12);
    assert_size!(ArchivedForHead, 8);
    assert_size!(ArchivedForInStmt, 24);
    assert_size!(ArchivedForOfStmt, 28);
    assert_size!(ArchivedForStmt, 40);
    assert_size!(ArchivedIfStmt, 24);
    assert_size!(ArchivedLabeledStmt, 36);
    assert_size!(ArchivedReturnStmt, 16);
    assert_size!(ArchivedStmt, 44);
    assert_size!(ArchivedSwitchCase, 24);
    assert_size!(ArchivedSwitchStmt, 20);
    assert_size!(ArchivedThrowStmt, 12);
    assert_size!(ArchivedTryStmt, 124);
    assert_size!(ArchivedVarDeclOrExpr, 8);
    assert_size!(ArchivedWhileStmt, 16);
    assert_size!(ArchivedWithStmt, 16);

    // TypeScript types
    assert_size!(ArchivedAccessibility, 1);
    assert_size!(ArchivedTruePlusMinus, 1);
    assert_size!(ArchivedTsArrayType, 12);
    assert_size!(ArchivedTsAsExpr, 16);
    assert_size!(ArchivedTsCallSignatureDecl, 32);
    assert_size!(ArchivedTsConditionalType, 24);
    assert_size!(ArchivedTsConstAssertion, 12);
    assert_size!(ArchivedTsConstructSignatureDecl, 32);
    assert_size!(ArchivedTsConstructorType, 32);
    assert_size!(ArchivedTsEntityName, 28);
    assert_size!(ArchivedTsEnumDecl, 44);
    assert_size!(ArchivedTsEnumMember, 48);
    assert_size!(ArchivedTsEnumMemberId, 32);
    assert_size!(ArchivedTsExportAssignment, 12);
    assert_size!(ArchivedTsExprWithTypeArgs, 20);
    assert_size!(ArchivedTsExternalModuleRef, 36);
    assert_size!(ArchivedTsFnOrConstructorType, 36);
    assert_size!(ArchivedTsFnParam, 36);
    assert_size!(ArchivedTsFnType, 28);
    assert_size!(ArchivedTsGetterSignature, 24);
    assert_size!(ArchivedTsImportEqualsDecl, 76);
    assert_size!(ArchivedTsImportType, 92);
    assert_size!(ArchivedTsIndexSignature, 28);
    assert_size!(ArchivedTsIndexedAccessType, 20);
    assert_size!(ArchivedTsInferType, 60);
    assert_size!(ArchivedTsInstantiation, 16);
    assert_size!(ArchivedTsInterfaceBody, 16);
    assert_size!(ArchivedTsInterfaceDecl, 68);
    assert_size!(ArchivedTsIntersectionType, 16);
    assert_size!(ArchivedTsKeywordType, 12);
    assert_size!(ArchivedTsKeywordTypeKind, 1);
    assert_size!(ArchivedTsLit, 40);
    assert_size!(ArchivedTsLitType, 48);
    assert_size!(ArchivedTsMappedType, 84);
    assert_size!(ArchivedTsMethodSignature, 40);
    assert_size!(ArchivedTsModuleBlock, 16);
    assert_size!(ArchivedTsModuleDecl, 92);
    assert_size!(ArchivedTsModuleName, 32);
    assert_size!(ArchivedTsModuleRef, 40);
    assert_size!(ArchivedTsNamespaceBody, 44);
    assert_size!(ArchivedTsNamespaceDecl, 40);
    assert_size!(ArchivedTsNamespaceExportDecl, 32);
    assert_size!(ArchivedTsNonNullExpr, 12);
    assert_size!(ArchivedTsOptionalType, 12);
    assert_size!(ArchivedTsParamProp, 56);
    assert_size!(ArchivedTsParamPropParam, 36);
    assert_size!(ArchivedTsParenthesizedType, 12);
    assert_size!(ArchivedTsPropertySignature, 28);
    assert_size!(ArchivedTsQualifiedName, 52);
    assert_size!(ArchivedTsRestType, 12);
    assert_size!(ArchivedTsSatisfiesExpr, 16);
    assert_size!(ArchivedTsSetterSignature, 52);
    assert_size!(ArchivedTsThisType, 8);
    assert_size!(ArchivedTsThisTypeOrIdent, 28);
    assert_size!(ArchivedTsTplLitType, 24);
    assert_size!(ArchivedTsTupleElement, 52);
    assert_size!(ArchivedTsTupleType, 16);
    assert_size!(ArchivedTsType, 120);
    assert_size!(ArchivedTsTypeAliasDecl, 48);
    assert_size!(ArchivedTsTypeAnn, 12);
    assert_size!(ArchivedTsTypeAssertion, 16);
    assert_size!(ArchivedTsTypeElement, 56);
    assert_size!(ArchivedTsTypeLit, 16);
    assert_size!(ArchivedTsTypeOperator, 16);
    assert_size!(ArchivedTsTypeOperatorOp, 1);
    assert_size!(ArchivedTsTypeParam, 52);
    assert_size!(ArchivedTsTypeParamDecl, 16);
    assert_size!(ArchivedTsTypeParamInstantiation, 16);
    assert_size!(ArchivedTsTypePredicate, 48);
    assert_size!(ArchivedTsTypeQuery, 112);
    assert_size!(ArchivedTsTypeQueryExpr, 96);
    assert_size!(ArchivedTsTypeRef, 44);
    assert_size!(ArchivedTsUnionOrIntersectionType, 20);
    assert_size!(ArchivedTsUnionType, 16);
}
