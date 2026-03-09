// This is not a public api.
#![cfg_attr(docsrs, feature(doc_cfg))]
#![deny(clippy::all)]
#![allow(clippy::ptr_arg)]

#[doc(hidden)]
pub extern crate swc_ecma_ast;

use std::{borrow::Cow, fmt::Debug};

use swc_common::{pass::CompilerPass, util::take::Take, Span, DUMMY_SP};
use swc_ecma_ast::*;
use swc_visit::{Repeat, Repeated};

pub use crate::generated::*;
mod generated;

pub fn fold_pass<V>(pass: V) -> FoldPass<V>
where
    V: Fold,
{
    FoldPass { pass }
}

pub struct FoldPass<V> {
    pass: V,
}

impl<V> Pass for FoldPass<V>
where
    V: Fold,
{
    #[inline(always)]
    fn process(&mut self, program: &mut Program) {
        program.map_with_mut(|p| p.fold_with(&mut self.pass));
    }
}

impl<V> Fold for FoldPass<V>
where
    V: Fold,
{
    #[inline(always)]
    fn fold_program(&mut self, node: Program) -> Program {
        self.pass.fold_program(node)
    }

    #[inline(always)]
    fn fold_module(&mut self, node: Module) -> Module {
        self.pass.fold_module(node)
    }

    #[inline(always)]
    fn fold_script(&mut self, node: Script) -> Script {
        self.pass.fold_script(node)
    }

    #[inline(always)]
    fn fold_stmt(&mut self, node: Stmt) -> Stmt {
        self.pass.fold_stmt(node)
    }

    #[inline(always)]
    fn fold_module_item(&mut self, item: ModuleItem) -> ModuleItem {
        self.pass.fold_module_item(item)
    }

    #[inline(always)]
    fn fold_expr(&mut self, expr: Expr) -> Expr {
        self.pass.fold_expr(expr)
    }

    #[inline(always)]
    fn fold_pat(&mut self, pat: Pat) -> Pat {
        self.pass.fold_pat(pat)
    }

    #[inline(always)]
    fn fold_assign_target(&mut self, target: AssignTarget) -> AssignTarget {
        self.pass.fold_assign_target(target)
    }

    #[inline(always)]
    fn fold_ident(&mut self, ident: Ident) -> Ident {
        self.pass.fold_ident(ident)
    }
}

impl<V> Repeated for FoldPass<V>
where
    V: Fold + Repeated,
{
    fn changed(&self) -> bool {
        self.pass.changed()
    }

    fn reset(&mut self) {
        self.pass.reset();
    }
}

impl<V> CompilerPass for FoldPass<V>
where
    V: Fold + CompilerPass,
{
    fn name(&self) -> Cow<'static, str> {
        self.pass.name()
    }
}

pub fn visit_mut_pass<V>(pass: V) -> VisitMutPass<V>
where
    V: VisitMut,
{
    VisitMutPass { pass }
}

pub struct VisitMutPass<V> {
    pass: V,
}

impl<V> Pass for VisitMutPass<V>
where
    V: VisitMut,
{
    #[inline(always)]
    fn process(&mut self, program: &mut Program) {
        program.visit_mut_with(&mut self.pass);
    }
}

impl<V> VisitMut for VisitMutPass<V>
where
    V: VisitMut,
{
    #[inline(always)]
    fn visit_mut_program(&mut self, program: &mut Program) {
        self.pass.visit_mut_program(program);
    }

    #[inline(always)]
    fn visit_mut_module(&mut self, module: &mut Module) {
        self.pass.visit_mut_module(module);
    }

    #[inline(always)]
    fn visit_mut_script(&mut self, script: &mut Script) {
        self.pass.visit_mut_script(script);
    }

    #[inline(always)]
    fn visit_mut_module_item(&mut self, item: &mut ModuleItem) {
        self.pass.visit_mut_module_item(item);
    }

    #[inline(always)]
    fn visit_mut_stmt(&mut self, stmt: &mut Stmt) {
        self.pass.visit_mut_stmt(stmt);
    }

    #[inline(always)]
    fn visit_mut_expr(&mut self, expr: &mut Expr) {
        self.pass.visit_mut_expr(expr);
    }

    #[inline(always)]
    fn visit_mut_pat(&mut self, pat: &mut Pat) {
        self.pass.visit_mut_pat(pat);
    }

    #[inline(always)]
    fn visit_mut_assign_target(&mut self, target: &mut AssignTarget) {
        self.pass.visit_mut_assign_target(target);
    }

    #[inline(always)]
    fn visit_mut_ident(&mut self, ident: &mut Ident) {
        self.pass.visit_mut_ident(ident);
    }
}

impl<V> Repeated for VisitMutPass<V>
where
    V: VisitMut + Repeated,
{
    fn changed(&self) -> bool {
        self.pass.changed()
    }

    fn reset(&mut self) {
        self.pass.reset();
    }
}

impl<V> CompilerPass for VisitMutPass<V>
where
    V: VisitMut + CompilerPass,
{
    fn name(&self) -> Cow<'static, str> {
        self.pass.name()
    }
}

pub fn visit_pass<V>(pass: V) -> VisitPass<V>
where
    V: Visit,
{
    VisitPass { pass }
}

pub struct VisitPass<V> {
    pass: V,
}

impl<V> Pass for VisitPass<V>
where
    V: Visit,
{
    #[inline(always)]
    fn process(&mut self, program: &mut Program) {
        program.visit_with(&mut self.pass);
    }
}

impl<V> Repeated for VisitPass<V>
where
    V: Visit + Repeated,
{
    fn changed(&self) -> bool {
        self.pass.changed()
    }

    fn reset(&mut self) {
        self.pass.reset();
    }
}

impl<V> CompilerPass for VisitPass<V>
where
    V: Visit + CompilerPass,
{
    fn name(&self) -> Cow<'static, str> {
        self.pass.name()
    }
}

impl<V> Fold for Repeat<V>
where
    V: Fold + Repeated,
{
    fn fold_program(&mut self, mut node: Program) -> Program {
        loop {
            self.pass.reset();
            node = node.fold_with(&mut self.pass);

            if !self.pass.changed() {
                break;
            }
        }

        node
    }

    fn fold_module(&mut self, mut node: Module) -> Module {
        loop {
            self.pass.reset();
            node = node.fold_with(&mut self.pass);

            if !self.pass.changed() {
                break;
            }
        }

        node
    }

    fn fold_script(&mut self, mut node: Script) -> Script {
        loop {
            self.pass.reset();
            node = node.fold_with(&mut self.pass);

            if !self.pass.changed() {
                break;
            }
        }

        node
    }
}

impl<V> VisitMut for Repeat<V>
where
    V: VisitMut + Repeated,
{
    fn visit_mut_program(&mut self, node: &mut Program) {
        loop {
            self.pass.reset();
            node.visit_mut_with(&mut self.pass);

            if !self.pass.changed() {
                break;
            }
        }
    }

    fn visit_mut_module(&mut self, node: &mut Module) {
        loop {
            self.pass.reset();
            node.visit_mut_with(&mut self.pass);

            if !self.pass.changed() {
                break;
            }
        }
    }

    fn visit_mut_script(&mut self, node: &mut Script) {
        loop {
            self.pass.reset();
            node.visit_mut_with(&mut self.pass);

            if !self.pass.changed() {
                break;
            }
        }
    }
}

/// Not a public api.
#[derive(Debug, Clone, Copy, PartialEq, Eq, Default)]
struct SpanRemover;

/// Returns a `Fold` which changes all span into `DUMMY_SP`.
pub fn span_remover() -> impl Debug + Fold + Copy + Eq + Default + 'static {
    SpanRemover
}

impl Fold for SpanRemover {
    fn fold_span(&mut self, _: Span) -> Span {
        DUMMY_SP
    }
}

#[macro_export]
macro_rules! assert_eq_ignore_span {
    ($l:expr, $r:expr) => {{
        use $crate::FoldWith;
        let l = $l.fold_with(&mut $crate::span_remover());
        let r = $r.fold_with(&mut $crate::span_remover());

        assert_eq!(l, r);
    }};

    ($l:expr, $r:expr, $($tts:tt)*) => {{
        use $crate::FoldWith;
        let l = $l.fold_with(&mut $crate::span_remover());
        let r = $r.fold_with(&mut $crate::span_remover());

        assert_eq!(l, r, $($tts)*);
    }};
}

/// Implemented for passes which inject variables.
///
/// If a pass depends on other pass which injects variables, this trait can be
/// used to keep the variables.
pub trait InjectVars {
    fn take_vars(&mut self) -> Vec<VarDeclarator>;
}

impl<V> InjectVars for FoldPass<V>
where
    V: Fold + InjectVars,
{
    fn take_vars(&mut self) -> Vec<VarDeclarator> {
        self.pass.take_vars()
    }
}

impl<V> InjectVars for VisitMutPass<V>
where
    V: VisitMut + InjectVars,
{
    fn take_vars(&mut self) -> Vec<VarDeclarator> {
        self.pass.take_vars()
    }
}

impl<V> InjectVars for VisitPass<V>
where
    V: Visit + InjectVars,
{
    fn take_vars(&mut self) -> Vec<VarDeclarator> {
        self.pass.take_vars()
    }
}

/// Note: Ignoring more types is not considered as a breaking change.
#[macro_export]
macro_rules! noop_fold_type {
    ($name:ident, $N:tt) => {
        fn $name(&mut self, node: $crate::swc_ecma_ast::$N) -> $crate::swc_ecma_ast::$N {
            node
        }
    };
    () => {
        noop_fold_type!(fold_accessibility, Accessibility);
        noop_fold_type!(fold_true_plus_minus, TruePlusMinus);
        noop_fold_type!(fold_ts_array_type, TsArrayType);
        noop_fold_type!(fold_ts_call_signature_decl, TsCallSignatureDecl);
        noop_fold_type!(fold_ts_conditional_type, TsConditionalType);
        noop_fold_type!(fold_ts_construct_signature_decl, TsConstructSignatureDecl);
        noop_fold_type!(fold_ts_constructor_type, TsConstructorType);
        noop_fold_type!(fold_ts_entity_name, TsEntityName);
        noop_fold_type!(fold_ts_enum_decl, TsEnumDecl);
        noop_fold_type!(fold_ts_enum_member, TsEnumMember);
        noop_fold_type!(fold_ts_enum_member_id, TsEnumMemberId);
        noop_fold_type!(fold_ts_expr_with_type_args, TsExprWithTypeArgs);
        noop_fold_type!(fold_ts_fn_or_constructor_type, TsFnOrConstructorType);
        noop_fold_type!(fold_ts_fn_param, TsFnParam);
        noop_fold_type!(fold_ts_fn_type, TsFnType);
        noop_fold_type!(fold_ts_import_equals_decl, TsImportEqualsDecl);
        noop_fold_type!(fold_ts_import_type, TsImportType);
        noop_fold_type!(fold_ts_index_signature, TsIndexSignature);
        noop_fold_type!(fold_ts_indexed_access_type, TsIndexedAccessType);
        noop_fold_type!(fold_ts_infer_type, TsInferType);
        noop_fold_type!(fold_ts_interface_body, TsInterfaceBody);
        noop_fold_type!(fold_ts_interface_decl, TsInterfaceDecl);
        noop_fold_type!(fold_ts_intersection_type, TsIntersectionType);
        noop_fold_type!(fold_ts_keyword_type, TsKeywordType);
        noop_fold_type!(fold_ts_keyword_type_kind, TsKeywordTypeKind);
        noop_fold_type!(fold_ts_mapped_type, TsMappedType);
        noop_fold_type!(fold_ts_method_signature, TsMethodSignature);
        noop_fold_type!(fold_ts_module_block, TsModuleBlock);
        noop_fold_type!(fold_ts_module_decl, TsModuleDecl);
        noop_fold_type!(fold_ts_module_name, TsModuleName);
        noop_fold_type!(fold_ts_namespace_body, TsNamespaceBody);
        noop_fold_type!(fold_ts_namespace_decl, TsNamespaceDecl);
        noop_fold_type!(fold_ts_namespace_export_decl, TsNamespaceExportDecl);
        noop_fold_type!(fold_ts_optional_type, TsOptionalType);
        noop_fold_type!(fold_ts_param_prop, TsParamProp);
        noop_fold_type!(fold_ts_param_prop_param, TsParamPropParam);
        noop_fold_type!(fold_ts_parenthesized_type, TsParenthesizedType);
        noop_fold_type!(fold_ts_property_signature, TsPropertySignature);
        noop_fold_type!(fold_ts_qualified_name, TsQualifiedName);
        noop_fold_type!(fold_ts_rest_type, TsRestType);
        noop_fold_type!(fold_ts_this_type, TsThisType);
        noop_fold_type!(fold_ts_this_type_or_ident, TsThisTypeOrIdent);
        noop_fold_type!(fold_ts_tuple_type, TsTupleType);
        noop_fold_type!(fold_ts_type, TsType);
        noop_fold_type!(fold_ts_type_alias_decl, TsTypeAliasDecl);
        noop_fold_type!(fold_ts_type_ann, TsTypeAnn);
        noop_fold_type!(fold_ts_type_assertion, TsTypeAssertion);
        noop_fold_type!(fold_ts_type_element, TsTypeElement);
        noop_fold_type!(fold_ts_type_lit, TsTypeLit);
        noop_fold_type!(fold_ts_type_operator, TsTypeOperator);
        noop_fold_type!(fold_ts_type_operator_op, TsTypeOperatorOp);
        noop_fold_type!(fold_ts_type_param, TsTypeParam);
        noop_fold_type!(fold_ts_type_param_decl, TsTypeParamDecl);
        noop_fold_type!(fold_ts_type_param_instantiation, TsTypeParamInstantiation);
        noop_fold_type!(fold_ts_type_predicate, TsTypePredicate);
        noop_fold_type!(fold_ts_type_query, TsTypeQuery);
        noop_fold_type!(fold_ts_type_query_expr, TsTypeQueryExpr);
        noop_fold_type!(fold_ts_type_ref, TsTypeRef);
        noop_fold_type!(
            fold_ts_union_or_intersection_type,
            TsUnionOrIntersectionType
        );
        noop_fold_type!(fold_ts_union_type, TsUnionType);
    };
}

/// Note: Ignoring more types is not considered as a breaking change.
#[macro_export]
macro_rules! noop_visit_type {
    (fail) => {
        noop_visit_type!(visit_accessibility, Accessibility, fail);
        noop_visit_type!(visit_true_plus_minus, TruePlusMinus, fail);
        noop_visit_type!(visit_ts_array_type, TsArrayType, fail);
        noop_visit_type!(visit_ts_call_signature_decl, TsCallSignatureDecl, fail);
        noop_visit_type!(visit_ts_conditional_type, TsConditionalType, fail);
        noop_visit_type!(
            visit_ts_construct_signature_decl,
            TsConstructSignatureDecl,
            fail
        );
        noop_visit_type!(visit_ts_constructor_type, TsConstructorType, fail);
        noop_visit_type!(visit_ts_entity_name, TsEntityName, fail);
        noop_visit_type!(visit_ts_expr_with_type_args, TsExprWithTypeArgs, fail);
        noop_visit_type!(visit_ts_fn_or_constructor_type, TsFnOrConstructorType, fail);
        noop_visit_type!(visit_ts_fn_param, TsFnParam, fail);
        noop_visit_type!(visit_ts_fn_type, TsFnType, fail);
        noop_visit_type!(visit_ts_import_type, TsImportType, fail);
        noop_visit_type!(visit_ts_index_signature, TsIndexSignature, fail);
        noop_visit_type!(visit_ts_indexed_access_type, TsIndexedAccessType, fail);
        noop_visit_type!(visit_ts_infer_type, TsInferType, fail);
        noop_visit_type!(visit_ts_interface_body, TsInterfaceBody, fail);
        noop_visit_type!(visit_ts_interface_decl, TsInterfaceDecl, fail);
        noop_visit_type!(visit_ts_intersection_type, TsIntersectionType, fail);
        noop_visit_type!(visit_ts_keyword_type, TsKeywordType, fail);
        noop_visit_type!(visit_ts_keyword_type_kind, TsKeywordTypeKind, fail);
        noop_visit_type!(visit_ts_mapped_type, TsMappedType, fail);
        noop_visit_type!(visit_ts_method_signature, TsMethodSignature, fail);
        noop_visit_type!(visit_ts_optional_type, TsOptionalType, fail);
        noop_visit_type!(visit_ts_parenthesized_type, TsParenthesizedType, fail);
        noop_visit_type!(visit_ts_property_signature, TsPropertySignature, fail);
        noop_visit_type!(visit_ts_qualified_name, TsQualifiedName, fail);
        noop_visit_type!(visit_ts_rest_type, TsRestType, fail);
        noop_visit_type!(visit_ts_this_type, TsThisType, fail);
        noop_visit_type!(visit_ts_this_type_or_ident, TsThisTypeOrIdent, fail);
        noop_visit_type!(visit_ts_tuple_type, TsTupleType, fail);
        noop_visit_type!(visit_ts_type, TsType, fail);
        noop_visit_type!(visit_ts_type_alias_decl, TsTypeAliasDecl, fail);
        noop_visit_type!(visit_ts_type_ann, TsTypeAnn, fail);
        noop_visit_type!(visit_ts_type_element, TsTypeElement, fail);
        noop_visit_type!(visit_ts_type_lit, TsTypeLit, fail);
        noop_visit_type!(visit_ts_type_operator, TsTypeOperator, fail);
        noop_visit_type!(visit_ts_type_operator_op, TsTypeOperatorOp, fail);
        noop_visit_type!(visit_ts_type_param, TsTypeParam, fail);
        noop_visit_type!(visit_ts_type_param_decl, TsTypeParamDecl, fail);
        noop_visit_type!(
            visit_ts_type_param_instantiation,
            TsTypeParamInstantiation,
            fail
        );
        noop_visit_type!(visit_ts_type_predicate, TsTypePredicate, fail);
        noop_visit_type!(visit_ts_type_query, TsTypeQuery, fail);
        noop_visit_type!(visit_ts_type_query_expr, TsTypeQueryExpr, fail);
        noop_visit_type!(visit_ts_type_ref, TsTypeRef, fail);
        noop_visit_type!(
            visit_ts_union_or_intersection_type,
            TsUnionOrIntersectionType,
            fail
        );
        noop_visit_type!(visit_ts_union_type, TsUnionType, fail);
    };
    () => {
        noop_visit_type!(visit_accessibility, Accessibility);
        noop_visit_type!(visit_true_plus_minus, TruePlusMinus);
        noop_visit_type!(visit_ts_array_type, TsArrayType);
        noop_visit_type!(visit_ts_call_signature_decl, TsCallSignatureDecl);
        noop_visit_type!(visit_ts_conditional_type, TsConditionalType);
        noop_visit_type!(visit_ts_construct_signature_decl, TsConstructSignatureDecl);
        noop_visit_type!(visit_ts_constructor_type, TsConstructorType);
        noop_visit_type!(visit_ts_entity_name, TsEntityName);
        noop_visit_type!(visit_ts_expr_with_type_args, TsExprWithTypeArgs);
        noop_visit_type!(visit_ts_fn_or_constructor_type, TsFnOrConstructorType);
        noop_visit_type!(visit_ts_fn_param, TsFnParam);
        noop_visit_type!(visit_ts_fn_type, TsFnType);
        noop_visit_type!(visit_ts_import_type, TsImportType);
        noop_visit_type!(visit_ts_index_signature, TsIndexSignature);
        noop_visit_type!(visit_ts_indexed_access_type, TsIndexedAccessType);
        noop_visit_type!(visit_ts_infer_type, TsInferType);
        noop_visit_type!(visit_ts_interface_body, TsInterfaceBody);
        noop_visit_type!(visit_ts_interface_decl, TsInterfaceDecl);
        noop_visit_type!(visit_ts_intersection_type, TsIntersectionType);
        noop_visit_type!(visit_ts_keyword_type, TsKeywordType);
        noop_visit_type!(visit_ts_keyword_type_kind, TsKeywordTypeKind);
        noop_visit_type!(visit_ts_mapped_type, TsMappedType);
        noop_visit_type!(visit_ts_method_signature, TsMethodSignature);
        noop_visit_type!(visit_ts_optional_type, TsOptionalType);
        noop_visit_type!(visit_ts_parenthesized_type, TsParenthesizedType);
        noop_visit_type!(visit_ts_property_signature, TsPropertySignature);
        noop_visit_type!(visit_ts_qualified_name, TsQualifiedName);
        noop_visit_type!(visit_ts_rest_type, TsRestType);
        noop_visit_type!(visit_ts_this_type, TsThisType);
        noop_visit_type!(visit_ts_this_type_or_ident, TsThisTypeOrIdent);
        noop_visit_type!(visit_ts_tuple_type, TsTupleType);
        noop_visit_type!(visit_ts_type, TsType);
        noop_visit_type!(visit_ts_type_alias_decl, TsTypeAliasDecl);
        noop_visit_type!(visit_ts_type_ann, TsTypeAnn);
        noop_visit_type!(visit_ts_type_element, TsTypeElement);
        noop_visit_type!(visit_ts_type_lit, TsTypeLit);
        noop_visit_type!(visit_ts_type_operator, TsTypeOperator);
        noop_visit_type!(visit_ts_type_operator_op, TsTypeOperatorOp);
        noop_visit_type!(visit_ts_type_param, TsTypeParam);
        noop_visit_type!(visit_ts_type_param_decl, TsTypeParamDecl);
        noop_visit_type!(visit_ts_type_param_instantiation, TsTypeParamInstantiation);
        noop_visit_type!(visit_ts_type_predicate, TsTypePredicate);
        noop_visit_type!(visit_ts_type_query, TsTypeQuery);
        noop_visit_type!(visit_ts_type_query_expr, TsTypeQueryExpr);
        noop_visit_type!(visit_ts_type_ref, TsTypeRef);
        noop_visit_type!(
            visit_ts_union_or_intersection_type,
            TsUnionOrIntersectionType
        );
        noop_visit_type!(visit_ts_union_type, TsUnionType);
    };

    ($name:ident, $N:tt, fail) => {
        #[cfg_attr(not(debug_assertions), inline(always))]
        fn $name(&mut self, _: &$crate::swc_ecma_ast::$N) {
            $crate::fail_no_typescript(stringify!($name));
        }
    };
    ($name:ident, $N:tt) => {
        fn $name(&mut self, _: &$crate::swc_ecma_ast::$N) {}
    };
}

/// NOT A PUBLIC API
#[doc(hidden)]
#[cfg_attr(not(debug_assertions), inline(always))]
pub fn fail_not_standard() {
    unsafe {
        debug_unreachable::debug_unreachable!(
            "This visitor supports only standard ECMAScript types. This method fails for \
             optimization purpose."
        )
    }
}

/// NOT A PUBLIC API
#[doc(hidden)]
#[cfg_attr(not(debug_assertions), inline(always))]
pub fn fail_no_typescript(visitor_name: &str) {
    unsafe {
        debug_unreachable::debug_unreachable!(
            "This visitor does not support TypeScript. This method fails for optimization \
             purposes. Encountered in unreachable visitor: {visitor_name}"
        )
    }
}

/// Mark visitor as ECMAScript standard only and mark other types as
/// unreachable.
///
/// Used to reduce the binary size.
#[macro_export]
macro_rules! standard_only_fold {
    ($name:ident, $N:ident) => {
        fn $name(&mut self, n: $crate::swc_ecma_ast::$N) -> $crate::swc_ecma_ast::$N {
            $crate::fail_not_standard();
            n
        }
    };
    () => {
        standard_only_fold!(fold_accessibility, Accessibility);
        standard_only_fold!(fold_true_plus_minus, TruePlusMinus);
        standard_only_fold!(fold_ts_array_type, TsArrayType);
        standard_only_fold!(fold_ts_call_signature_decl, TsCallSignatureDecl);
        standard_only_fold!(fold_ts_conditional_type, TsConditionalType);
        standard_only_fold!(fold_ts_construct_signature_decl, TsConstructSignatureDecl);
        standard_only_fold!(fold_ts_constructor_type, TsConstructorType);
        standard_only_fold!(fold_ts_entity_name, TsEntityName);
        standard_only_fold!(fold_ts_expr_with_type_args, TsExprWithTypeArgs);
        standard_only_fold!(fold_ts_fn_or_constructor_type, TsFnOrConstructorType);
        standard_only_fold!(fold_ts_fn_param, TsFnParam);
        standard_only_fold!(fold_ts_fn_type, TsFnType);
        standard_only_fold!(fold_ts_import_type, TsImportType);
        standard_only_fold!(fold_ts_index_signature, TsIndexSignature);
        standard_only_fold!(fold_ts_indexed_access_type, TsIndexedAccessType);
        standard_only_fold!(fold_ts_infer_type, TsInferType);
        standard_only_fold!(fold_ts_interface_body, TsInterfaceBody);
        standard_only_fold!(fold_ts_interface_decl, TsInterfaceDecl);
        standard_only_fold!(fold_ts_intersection_type, TsIntersectionType);
        standard_only_fold!(fold_ts_keyword_type, TsKeywordType);
        standard_only_fold!(fold_ts_keyword_type_kind, TsKeywordTypeKind);
        standard_only_fold!(fold_ts_mapped_type, TsMappedType);
        standard_only_fold!(fold_ts_method_signature, TsMethodSignature);
        standard_only_fold!(fold_ts_optional_type, TsOptionalType);
        standard_only_fold!(fold_ts_parenthesized_type, TsParenthesizedType);
        standard_only_fold!(fold_ts_property_signature, TsPropertySignature);
        standard_only_fold!(fold_ts_qualified_name, TsQualifiedName);
        standard_only_fold!(fold_ts_rest_type, TsRestType);
        standard_only_fold!(fold_ts_this_type, TsThisType);
        standard_only_fold!(fold_ts_this_type_or_ident, TsThisTypeOrIdent);
        standard_only_fold!(fold_ts_tuple_type, TsTupleType);
        standard_only_fold!(fold_ts_type, TsType);
        standard_only_fold!(fold_ts_type_alias_decl, TsTypeAliasDecl);
        standard_only_fold!(fold_ts_type_ann, TsTypeAnn);
        standard_only_fold!(fold_ts_type_element, TsTypeElement);
        standard_only_fold!(fold_ts_type_lit, TsTypeLit);
        standard_only_fold!(fold_ts_type_operator, TsTypeOperator);
        standard_only_fold!(fold_ts_type_operator_op, TsTypeOperatorOp);
        standard_only_fold!(fold_ts_type_param, TsTypeParam);
        standard_only_fold!(fold_ts_type_param_decl, TsTypeParamDecl);
        standard_only_fold!(fold_ts_type_param_instantiation, TsTypeParamInstantiation);
        standard_only_fold!(fold_ts_type_predicate, TsTypePredicate);
        standard_only_fold!(fold_ts_type_query, TsTypeQuery);
        standard_only_fold!(fold_ts_type_query_expr, TsTypeQueryExpr);
        standard_only_fold!(fold_ts_type_ref, TsTypeRef);
        standard_only_fold!(
            fold_ts_union_or_intersection_type,
            TsUnionOrIntersectionType
        );
        standard_only_fold!(fold_ts_union_type, TsUnionType);

        standard_only_fold!(fold_jsx_element, JSXElement);
        standard_only_fold!(fold_jsx_fragment, JSXFragment);
        standard_only_fold!(fold_jsx_empty_expr, JSXEmptyExpr);
        standard_only_fold!(fold_jsx_member_expr, JSXMemberExpr);
        standard_only_fold!(fold_jsx_namespaced_name, JSXNamespacedName);
    };
}

/// Mark visitor as ECMAScript standard only and mark other types as
/// unreachable.
///
/// Used to reduce the binary size.
#[macro_export]
macro_rules! standard_only_visit {
    ($name:ident, $N:ident) => {
        fn $name(&mut self, _: &$crate::swc_ecma_ast::$N) {
            $crate::fail_not_standard()
        }
    };
    () => {
        standard_only_visit!(visit_accessibility, Accessibility);
        standard_only_visit!(visit_true_plus_minus, TruePlusMinus);
        standard_only_visit!(visit_ts_array_type, TsArrayType);
        standard_only_visit!(visit_ts_call_signature_decl, TsCallSignatureDecl);
        standard_only_visit!(visit_ts_conditional_type, TsConditionalType);
        standard_only_visit!(visit_ts_construct_signature_decl, TsConstructSignatureDecl);
        standard_only_visit!(visit_ts_constructor_type, TsConstructorType);
        standard_only_visit!(visit_ts_entity_name, TsEntityName);
        standard_only_visit!(visit_ts_expr_with_type_args, TsExprWithTypeArgs);
        standard_only_visit!(visit_ts_fn_or_constructor_type, TsFnOrConstructorType);
        standard_only_visit!(visit_ts_fn_param, TsFnParam);
        standard_only_visit!(visit_ts_fn_type, TsFnType);
        standard_only_visit!(visit_ts_import_type, TsImportType);
        standard_only_visit!(visit_ts_index_signature, TsIndexSignature);
        standard_only_visit!(visit_ts_indexed_access_type, TsIndexedAccessType);
        standard_only_visit!(visit_ts_infer_type, TsInferType);
        standard_only_visit!(visit_ts_interface_body, TsInterfaceBody);
        standard_only_visit!(visit_ts_interface_decl, TsInterfaceDecl);
        standard_only_visit!(visit_ts_intersection_type, TsIntersectionType);
        standard_only_visit!(visit_ts_keyword_type, TsKeywordType);
        standard_only_visit!(visit_ts_keyword_type_kind, TsKeywordTypeKind);
        standard_only_visit!(visit_ts_mapped_type, TsMappedType);
        standard_only_visit!(visit_ts_method_signature, TsMethodSignature);
        standard_only_visit!(visit_ts_optional_type, TsOptionalType);
        standard_only_visit!(visit_ts_parenthesized_type, TsParenthesizedType);
        standard_only_visit!(visit_ts_property_signature, TsPropertySignature);
        standard_only_visit!(visit_ts_qualified_name, TsQualifiedName);
        standard_only_visit!(visit_ts_rest_type, TsRestType);
        standard_only_visit!(visit_ts_this_type, TsThisType);
        standard_only_visit!(visit_ts_this_type_or_ident, TsThisTypeOrIdent);
        standard_only_visit!(visit_ts_tuple_type, TsTupleType);
        standard_only_visit!(visit_ts_type, TsType);
        standard_only_visit!(visit_ts_type_alias_decl, TsTypeAliasDecl);
        standard_only_visit!(visit_ts_type_ann, TsTypeAnn);
        standard_only_visit!(visit_ts_type_element, TsTypeElement);
        standard_only_visit!(visit_ts_type_lit, TsTypeLit);
        standard_only_visit!(visit_ts_type_operator, TsTypeOperator);
        standard_only_visit!(visit_ts_type_operator_op, TsTypeOperatorOp);
        standard_only_visit!(visit_ts_type_param, TsTypeParam);
        standard_only_visit!(visit_ts_type_param_decl, TsTypeParamDecl);
        standard_only_visit!(visit_ts_type_param_instantiation, TsTypeParamInstantiation);
        standard_only_visit!(visit_ts_type_predicate, TsTypePredicate);
        standard_only_visit!(visit_ts_type_query, TsTypeQuery);
        standard_only_visit!(visit_ts_type_query_expr, TsTypeQueryExpr);
        standard_only_visit!(visit_ts_type_ref, TsTypeRef);
        standard_only_visit!(
            visit_ts_union_or_intersection_type,
            TsUnionOrIntersectionType
        );
        standard_only_visit!(visit_ts_union_type, TsUnionType);

        standard_only_visit!(visit_jsx_element, JSXElement);
        standard_only_visit!(visit_jsx_fragment, JSXFragment);
        standard_only_visit!(visit_jsx_empty_expr, JSXEmptyExpr);
        standard_only_visit!(visit_jsx_member_expr, JSXMemberExpr);
        standard_only_visit!(visit_jsx_namespaced_name, JSXNamespacedName);
    };
}

/// Mark visitor as ECMAScript standard only and mark other types as
/// unreachable.
///
/// Used to reduce the binary size.
#[macro_export]
macro_rules! standard_only_visit_mut {
    ($name:ident, $N:ident) => {
        fn $name(&mut self, _: &mut $crate::swc_ecma_ast::$N) {
            $crate::fail_not_standard()
        }
    };
    () => {
        standard_only_visit_mut!(visit_mut_accessibility, Accessibility);
        standard_only_visit_mut!(visit_mut_true_plus_minus, TruePlusMinus);
        standard_only_visit_mut!(visit_mut_ts_array_type, TsArrayType);
        standard_only_visit_mut!(visit_mut_ts_call_signature_decl, TsCallSignatureDecl);
        standard_only_visit_mut!(visit_mut_ts_conditional_type, TsConditionalType);
        standard_only_visit_mut!(
            visit_mut_ts_construct_signature_decl,
            TsConstructSignatureDecl
        );
        standard_only_visit_mut!(visit_mut_ts_constructor_type, TsConstructorType);
        standard_only_visit_mut!(visit_mut_ts_entity_name, TsEntityName);
        standard_only_visit_mut!(visit_mut_ts_expr_with_type_args, TsExprWithTypeArgs);
        standard_only_visit_mut!(visit_mut_ts_fn_or_constructor_type, TsFnOrConstructorType);
        standard_only_visit_mut!(visit_mut_ts_fn_param, TsFnParam);
        standard_only_visit_mut!(visit_mut_ts_fn_type, TsFnType);
        standard_only_visit_mut!(visit_mut_ts_import_type, TsImportType);
        standard_only_visit_mut!(visit_mut_ts_index_signature, TsIndexSignature);
        standard_only_visit_mut!(visit_mut_ts_indexed_access_type, TsIndexedAccessType);
        standard_only_visit_mut!(visit_mut_ts_infer_type, TsInferType);
        standard_only_visit_mut!(visit_mut_ts_interface_body, TsInterfaceBody);
        standard_only_visit_mut!(visit_mut_ts_interface_decl, TsInterfaceDecl);
        standard_only_visit_mut!(visit_mut_ts_intersection_type, TsIntersectionType);
        standard_only_visit_mut!(visit_mut_ts_keyword_type, TsKeywordType);
        standard_only_visit_mut!(visit_mut_ts_keyword_type_kind, TsKeywordTypeKind);
        standard_only_visit_mut!(visit_mut_ts_mapped_type, TsMappedType);
        standard_only_visit_mut!(visit_mut_ts_method_signature, TsMethodSignature);
        standard_only_visit_mut!(visit_mut_ts_optional_type, TsOptionalType);
        standard_only_visit_mut!(visit_mut_ts_parenthesized_type, TsParenthesizedType);
        standard_only_visit_mut!(visit_mut_ts_property_signature, TsPropertySignature);
        standard_only_visit_mut!(visit_mut_ts_qualified_name, TsQualifiedName);
        standard_only_visit_mut!(visit_mut_ts_rest_type, TsRestType);
        standard_only_visit_mut!(visit_mut_ts_this_type, TsThisType);
        standard_only_visit_mut!(visit_mut_ts_this_type_or_ident, TsThisTypeOrIdent);
        standard_only_visit_mut!(visit_mut_ts_tuple_type, TsTupleType);
        standard_only_visit_mut!(visit_mut_ts_type, TsType);
        standard_only_visit_mut!(visit_mut_ts_type_alias_decl, TsTypeAliasDecl);
        standard_only_visit_mut!(visit_mut_ts_type_ann, TsTypeAnn);
        standard_only_visit_mut!(visit_mut_ts_type_element, TsTypeElement);
        standard_only_visit_mut!(visit_mut_ts_type_lit, TsTypeLit);
        standard_only_visit_mut!(visit_mut_ts_type_operator, TsTypeOperator);
        standard_only_visit_mut!(visit_mut_ts_type_operator_op, TsTypeOperatorOp);
        standard_only_visit_mut!(visit_mut_ts_type_param, TsTypeParam);
        standard_only_visit_mut!(visit_mut_ts_type_param_decl, TsTypeParamDecl);
        standard_only_visit_mut!(
            visit_mut_ts_type_param_instantiation,
            TsTypeParamInstantiation
        );
        standard_only_visit_mut!(visit_mut_ts_type_predicate, TsTypePredicate);
        standard_only_visit_mut!(visit_mut_ts_type_query, TsTypeQuery);
        standard_only_visit_mut!(visit_mut_ts_type_query_expr, TsTypeQueryExpr);
        standard_only_visit_mut!(visit_mut_ts_type_ref, TsTypeRef);
        standard_only_visit_mut!(
            visit_mut_ts_union_or_intersection_type,
            TsUnionOrIntersectionType
        );
        standard_only_visit_mut!(visit_mut_ts_union_type, TsUnionType);

        standard_only_visit_mut!(visit_mut_jsx_element, JSXElement);
        standard_only_visit_mut!(visit_mut_jsx_fragment, JSXFragment);
        standard_only_visit_mut!(visit_mut_jsx_empty_expr, JSXEmptyExpr);
        standard_only_visit_mut!(visit_mut_jsx_member_expr, JSXMemberExpr);
        standard_only_visit_mut!(visit_mut_jsx_namespaced_name, JSXNamespacedName);
    };
}

/// Note: Ignoring more types is not considered as a breaking change.
#[macro_export]
macro_rules! noop_visit_mut_type {
    (fail) => {
        noop_visit_mut_type!(visit_mut_accessibility, Accessibility, fail);
        noop_visit_mut_type!(visit_mut_true_plus_minus, TruePlusMinus, fail);
        noop_visit_mut_type!(visit_mut_ts_array_type, TsArrayType, fail);
        noop_visit_mut_type!(visit_mut_ts_call_signature_decl, TsCallSignatureDecl, fail);
        noop_visit_mut_type!(visit_mut_ts_conditional_type, TsConditionalType, fail);
        noop_visit_mut_type!(
            visit_mut_ts_construct_signature_decl,
            TsConstructSignatureDecl,
            fail
        );
        noop_visit_mut_type!(visit_mut_ts_constructor_type, TsConstructorType, fail);
        noop_visit_mut_type!(visit_mut_ts_entity_name, TsEntityName, fail);
        noop_visit_mut_type!(visit_mut_ts_expr_with_type_args, TsExprWithTypeArgs, fail);
        noop_visit_mut_type!(
            visit_mut_ts_fn_or_constructor_type,
            TsFnOrConstructorType,
            fail
        );
        noop_visit_mut_type!(visit_mut_ts_fn_param, TsFnParam, fail);
        noop_visit_mut_type!(visit_mut_ts_fn_type, TsFnType, fail);
        noop_visit_mut_type!(visit_mut_ts_import_type, TsImportType, fail);
        noop_visit_mut_type!(visit_mut_ts_index_signature, TsIndexSignature, fail);
        noop_visit_mut_type!(visit_mut_ts_indexed_access_type, TsIndexedAccessType, fail);
        noop_visit_mut_type!(visit_mut_ts_infer_type, TsInferType, fail);
        noop_visit_mut_type!(visit_mut_ts_interface_body, TsInterfaceBody, fail);
        noop_visit_mut_type!(visit_mut_ts_interface_decl, TsInterfaceDecl, fail);
        noop_visit_mut_type!(visit_mut_ts_intersection_type, TsIntersectionType, fail);
        noop_visit_mut_type!(visit_mut_ts_keyword_type, TsKeywordType, fail);
        noop_visit_mut_type!(visit_mut_ts_keyword_type_kind, TsKeywordTypeKind, fail);
        noop_visit_mut_type!(visit_mut_ts_mapped_type, TsMappedType, fail);
        noop_visit_mut_type!(visit_mut_ts_method_signature, TsMethodSignature, fail);
        noop_visit_mut_type!(visit_mut_ts_optional_type, TsOptionalType, fail);
        noop_visit_mut_type!(visit_mut_ts_parenthesized_type, TsParenthesizedType, fail);
        noop_visit_mut_type!(visit_mut_ts_property_signature, TsPropertySignature, fail);
        noop_visit_mut_type!(visit_mut_ts_qualified_name, TsQualifiedName, fail);
        noop_visit_mut_type!(visit_mut_ts_rest_type, TsRestType, fail);
        noop_visit_mut_type!(visit_mut_ts_this_type, TsThisType, fail);
        noop_visit_mut_type!(visit_mut_ts_this_type_or_ident, TsThisTypeOrIdent, fail);
        noop_visit_mut_type!(visit_mut_ts_tuple_type, TsTupleType, fail);
        noop_visit_mut_type!(visit_mut_ts_type, TsType, fail);
        noop_visit_mut_type!(visit_mut_ts_type_alias_decl, TsTypeAliasDecl, fail);
        noop_visit_mut_type!(visit_mut_ts_type_ann, TsTypeAnn, fail);
        noop_visit_mut_type!(visit_mut_ts_type_element, TsTypeElement, fail);
        noop_visit_mut_type!(visit_mut_ts_type_lit, TsTypeLit, fail);
        noop_visit_mut_type!(visit_mut_ts_type_operator, TsTypeOperator, fail);
        noop_visit_mut_type!(visit_mut_ts_type_operator_op, TsTypeOperatorOp, fail);
        noop_visit_mut_type!(visit_mut_ts_type_param, TsTypeParam, fail);
        noop_visit_mut_type!(visit_mut_ts_type_param_decl, TsTypeParamDecl, fail);
        noop_visit_mut_type!(
            visit_mut_ts_type_param_instantiation,
            TsTypeParamInstantiation,
            fail
        );
        noop_visit_mut_type!(visit_mut_ts_type_predicate, TsTypePredicate, fail);
        noop_visit_mut_type!(visit_mut_ts_type_query, TsTypeQuery, fail);
        noop_visit_mut_type!(visit_mut_ts_type_query_expr, TsTypeQueryExpr, fail);
        noop_visit_mut_type!(visit_mut_ts_type_ref, TsTypeRef, fail);
        noop_visit_mut_type!(
            visit_mut_ts_union_or_intersection_type,
            TsUnionOrIntersectionType,
            fail
        );
        noop_visit_mut_type!(visit_mut_ts_union_type, TsUnionType, fail);
    };
    () => {
        noop_visit_mut_type!(visit_mut_accessibility, Accessibility);
        noop_visit_mut_type!(visit_mut_true_plus_minus, TruePlusMinus);
        noop_visit_mut_type!(visit_mut_ts_array_type, TsArrayType);
        noop_visit_mut_type!(visit_mut_ts_call_signature_decl, TsCallSignatureDecl);
        noop_visit_mut_type!(visit_mut_ts_conditional_type, TsConditionalType);
        noop_visit_mut_type!(
            visit_mut_ts_construct_signature_decl,
            TsConstructSignatureDecl
        );
        noop_visit_mut_type!(visit_mut_ts_constructor_type, TsConstructorType);
        noop_visit_mut_type!(visit_mut_ts_entity_name, TsEntityName);
        noop_visit_mut_type!(visit_mut_ts_expr_with_type_args, TsExprWithTypeArgs);
        noop_visit_mut_type!(visit_mut_ts_fn_or_constructor_type, TsFnOrConstructorType);
        noop_visit_mut_type!(visit_mut_ts_fn_param, TsFnParam);
        noop_visit_mut_type!(visit_mut_ts_fn_type, TsFnType);
        noop_visit_mut_type!(visit_mut_ts_import_type, TsImportType);
        noop_visit_mut_type!(visit_mut_ts_index_signature, TsIndexSignature);
        noop_visit_mut_type!(visit_mut_ts_indexed_access_type, TsIndexedAccessType);
        noop_visit_mut_type!(visit_mut_ts_infer_type, TsInferType);
        noop_visit_mut_type!(visit_mut_ts_interface_body, TsInterfaceBody);
        noop_visit_mut_type!(visit_mut_ts_interface_decl, TsInterfaceDecl);
        noop_visit_mut_type!(visit_mut_ts_intersection_type, TsIntersectionType);
        noop_visit_mut_type!(visit_mut_ts_keyword_type, TsKeywordType);
        noop_visit_mut_type!(visit_mut_ts_keyword_type_kind, TsKeywordTypeKind);
        noop_visit_mut_type!(visit_mut_ts_mapped_type, TsMappedType);
        noop_visit_mut_type!(visit_mut_ts_method_signature, TsMethodSignature);
        noop_visit_mut_type!(visit_mut_ts_optional_type, TsOptionalType);
        noop_visit_mut_type!(visit_mut_ts_parenthesized_type, TsParenthesizedType);
        noop_visit_mut_type!(visit_mut_ts_property_signature, TsPropertySignature);
        noop_visit_mut_type!(visit_mut_ts_qualified_name, TsQualifiedName);
        noop_visit_mut_type!(visit_mut_ts_rest_type, TsRestType);
        noop_visit_mut_type!(visit_mut_ts_this_type, TsThisType);
        noop_visit_mut_type!(visit_mut_ts_this_type_or_ident, TsThisTypeOrIdent);
        noop_visit_mut_type!(visit_mut_ts_tuple_type, TsTupleType);
        noop_visit_mut_type!(visit_mut_ts_type, TsType);
        noop_visit_mut_type!(visit_mut_ts_type_alias_decl, TsTypeAliasDecl);
        noop_visit_mut_type!(visit_mut_ts_type_ann, TsTypeAnn);
        noop_visit_mut_type!(visit_mut_ts_type_element, TsTypeElement);
        noop_visit_mut_type!(visit_mut_ts_type_lit, TsTypeLit);
        noop_visit_mut_type!(visit_mut_ts_type_operator, TsTypeOperator);
        noop_visit_mut_type!(visit_mut_ts_type_operator_op, TsTypeOperatorOp);
        noop_visit_mut_type!(visit_mut_ts_type_param, TsTypeParam);
        noop_visit_mut_type!(visit_mut_ts_type_param_decl, TsTypeParamDecl);
        noop_visit_mut_type!(
            visit_mut_ts_type_param_instantiation,
            TsTypeParamInstantiation
        );
        noop_visit_mut_type!(visit_mut_ts_type_predicate, TsTypePredicate);
        noop_visit_mut_type!(visit_mut_ts_type_query, TsTypeQuery);
        noop_visit_mut_type!(visit_mut_ts_type_query_expr, TsTypeQueryExpr);
        noop_visit_mut_type!(visit_mut_ts_type_ref, TsTypeRef);
        noop_visit_mut_type!(
            visit_mut_ts_union_or_intersection_type,
            TsUnionOrIntersectionType
        );
        noop_visit_mut_type!(visit_mut_ts_union_type, TsUnionType);
    };

    ($name:ident, $N:ident, fail) => {
        #[cfg_attr(not(debug_assertions), inline(always))]
        fn $name(&mut self, _: &mut $crate::swc_ecma_ast::$N) {
            $crate::fail_no_typescript(stringify!($name));
        }
    };
    ($name:ident, $N:ident) => {
        fn $name(&mut self, _: &mut $crate::swc_ecma_ast::$N) {}
    };
}

#[macro_export]
macro_rules! visit_obj_and_computed {
    () => {
        fn visit_member_prop(&mut self, n: &$crate::swc_ecma_ast::MemberProp) {
            if let $crate::swc_ecma_ast::MemberProp::Computed(c) = n {
                c.visit_with(self);
            }
        }

        fn visit_jsx_member_expr(&mut self, n: &$crate::swc_ecma_ast::JSXMemberExpr) {
            n.obj.visit_with(self);
        }

        fn visit_super_prop(&mut self, n: &$crate::swc_ecma_ast::SuperProp) {
            if let $crate::swc_ecma_ast::SuperProp::Computed(c) = n {
                c.visit_with(self);
            }
        }
    };
}

#[macro_export]
macro_rules! visit_mut_obj_and_computed {
    () => {
        fn visit_mut_member_prop(&mut self, n: &mut $crate::swc_ecma_ast::MemberProp) {
            if let $crate::swc_ecma_ast::MemberProp::Computed(c) = n {
                c.visit_mut_with(self);
            }
        }

        fn visit_mut_jsx_member_expr(&mut self, n: &mut $crate::swc_ecma_ast::JSXMemberExpr) {
            n.obj.visit_mut_with(self);
        }

        fn visit_mut_super_prop(&mut self, n: &mut $crate::swc_ecma_ast::SuperProp) {
            if let $crate::swc_ecma_ast::SuperProp::Computed(c) = n {
                c.visit_mut_with(self);
            }
        }
    };
}

macro_rules! impl_traits_for_tuple {
    (
        [$idx:tt, $name:ident], $([$idx_rest:tt, $name_rest:ident]),*
    ) => {
        impl<$name, $($name_rest),*> VisitMut for ($name, $($name_rest),*)
        where
            $name: VisitMut,
            $($name_rest: VisitMut),*
        {

            fn visit_mut_program(&mut self, program: &mut Program) {
                self.$idx.visit_mut_program(program);

                $(
                    self.$idx_rest.visit_mut_program(program);
                )*
            }

            fn visit_mut_module(&mut self, module: &mut Module) {
                self.$idx.visit_mut_module(module);

                $(
                    self.$idx_rest.visit_mut_module(module);
                )*
            }

            fn visit_mut_script(&mut self, script: &mut Script) {
                self.$idx.visit_mut_script(script);

                $(
                    self.$idx_rest.visit_mut_script(script);
                )*
            }

            fn visit_mut_stmt(&mut self, stmt: &mut Stmt) {
                self.$idx.visit_mut_stmt(stmt);

                $(
                    self.$idx_rest.visit_mut_stmt(stmt);
                )*
            }

            fn visit_mut_expr(&mut self, expr: &mut Expr) {
                self.$idx.visit_mut_expr(expr);

                $(
                    self.$idx_rest.visit_mut_expr(expr);
                )*
            }

            fn visit_mut_pat(&mut self, pat: &mut Pat) {
                self.$idx.visit_mut_pat(pat);

                $(
                    self.$idx_rest.visit_mut_pat(pat);
                )*
            }

            fn visit_mut_assign_target(&mut self, target: &mut AssignTarget) {
                self.$idx.visit_mut_assign_target(target);

                $(
                    self.$idx_rest.visit_mut_assign_target(target);
                )*
            }

            fn visit_mut_ident(&mut self, ident: &mut Ident) {
                self.$idx.visit_mut_ident(ident);

                $(
                    self.$idx_rest.visit_mut_ident(ident);
                )*
            }
        }
    };
}

impl_traits_for_tuple!([0, A], [1, B]);
impl_traits_for_tuple!([0, A], [1, B], [2, C]);
impl_traits_for_tuple!([0, A], [1, B], [2, C], [3, D]);
impl_traits_for_tuple!([0, A], [1, B], [2, C], [3, D], [4, E]);
impl_traits_for_tuple!([0, A], [1, B], [2, C], [3, D], [4, E], [5, F]);
impl_traits_for_tuple!([0, A], [1, B], [2, C], [3, D], [4, E], [5, F], [6, G]);
impl_traits_for_tuple!(
    [0, A],
    [1, B],
    [2, C],
    [3, D],
    [4, E],
    [5, F],
    [6, G],
    [7, H]
);
impl_traits_for_tuple!(
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
impl_traits_for_tuple!(
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
impl_traits_for_tuple!(
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
impl_traits_for_tuple!(
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
impl_traits_for_tuple!(
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
