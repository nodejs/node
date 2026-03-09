use std::{iter, mem};

use rustc_hash::{FxHashMap, FxHashSet};
use swc_atoms::Atom;
use swc_common::{
    errors::HANDLER, source_map::PURE_SP, util::take::Take, Mark, Span, Spanned, SyntaxContext,
    DUMMY_SP,
};
use swc_ecma_ast::*;
use swc_ecma_utils::{
    alias_ident_for, constructor::inject_after_super, find_pat_ids, ident::IdentLike, is_literal,
    member_expr, private_ident, quote_ident, quote_str, stack_size::maybe_grow_default,
    ExprFactory, QueryRef, RefRewriter, StmtLikeInjector,
};
use swc_ecma_visit::{
    noop_visit_mut_type, noop_visit_type, visit_mut_pass, Visit, VisitMut, VisitMutWith, VisitWith,
};

use crate::{
    config::TsImportExportAssignConfig,
    ts_enum::{EnumValueComputer, TsEnumRecord, TsEnumRecordKey, TsEnumRecordValue},
    utils::{assign_value_to_this_private_prop, assign_value_to_this_prop, Factory},
};

#[inline]
fn enum_member_id_atom(id: &TsEnumMemberId) -> Atom {
    match id {
        TsEnumMemberId::Ident(ident) => ident.sym.clone(),
        TsEnumMemberId::Str(s) => s.value.to_atom_lossy().into_owned(),
        #[cfg(swc_ast_unknown)]
        _ => panic!("unable to access unknown nodes"),
    }
}

/// ## This Module will transform all TypeScript specific synatx
///
/// - ### [namespace]/[modules]/[enums]
/// - ### class constructor [parameter properties]
///
///     ```TypeScript
///     class Foo {
///         constructor(public x: number) {
///             // No body necessary
///         }
///     }
///     ```
/// - ### [export and import require]
///
///     ```TypeScript
///     import foo = require("foo");
///     export = foo;
///     ```
///
/// [namespace]: https://www.typescriptlang.org/docs/handbook/namespaces.html
/// [modules]: https://www.typescriptlang.org/docs/handbook/modules.html
/// [enums]: https://www.typescriptlang.org/docs/handbook/enums.html
/// [parameter properties]: https://www.typescriptlang.org/docs/handbook/2/classes.html#parameter-properties
/// [export and import require]: https://www.typescriptlang.org/docs/handbook/modules.html#export--and-import--require
#[derive(Default)]
pub(crate) struct Transform {
    unresolved_ctxt: SyntaxContext,
    top_level_ctxt: SyntaxContext,

    import_export_assign_config: TsImportExportAssignConfig,
    ts_enum_is_mutable: bool,
    verbatim_module_syntax: bool,
    native_class_properties: bool,

    is_lhs: bool,

    ref_rewriter: Option<RefRewriter<ExportQuery>>,

    decl_id_record: FxHashSet<Id>,
    namespace_id: Option<Id>,
    exported_binding: FxHashMap<Id, Option<Id>>,

    enum_record: TsEnumRecord,
    const_enum: FxHashSet<Id>,

    var_list: Vec<Id>,
    export_var_list: Vec<Id>,

    in_class_prop: Vec<Id>,
    in_class_prop_init: Vec<Box<Expr>>,
}

pub fn transform(
    unresolved_mark: Mark,
    top_level_mark: Mark,
    import_export_assign_config: TsImportExportAssignConfig,
    ts_enum_is_mutable: bool,
    verbatim_module_syntax: bool,
    native_class_properties: bool,
) -> impl Pass {
    visit_mut_pass(Transform {
        unresolved_ctxt: SyntaxContext::empty().apply_mark(unresolved_mark),
        top_level_ctxt: SyntaxContext::empty().apply_mark(top_level_mark),
        import_export_assign_config,
        ts_enum_is_mutable,
        verbatim_module_syntax,
        native_class_properties,
        ..Default::default()
    })
}

impl Visit for Transform {
    noop_visit_type!();

    fn visit_ts_enum_decl(&mut self, node: &TsEnumDecl) {
        node.visit_children_with(self);

        let TsEnumDecl {
            declare,
            is_const,
            id,
            members,
            ..
        } = node;

        if *is_const {
            self.const_enum.insert(id.to_id());
        }

        debug_assert!(!declare);

        let mut default_init = 0.0.into();

        for m in members {
            let value = Self::transform_ts_enum_member(
                m.clone(),
                &id.to_id(),
                &default_init,
                &self.enum_record,
                self.unresolved_ctxt,
            );

            default_init = value.inc();

            let member_name = enum_member_id_atom(&m.id);
            let key = TsEnumRecordKey {
                enum_id: id.to_id(),
                member_name: member_name.clone(),
            };

            self.enum_record.insert(key, value);
        }
    }

    fn visit_ts_namespace_decl(&mut self, n: &TsNamespaceDecl) {
        let id = n.id.to_id();
        let namespace_id = self.namespace_id.replace(id);

        n.body.visit_with(self);

        self.namespace_id = namespace_id;
    }

    fn visit_ts_module_decl(&mut self, n: &TsModuleDecl) {
        let id = n.id.to_id();

        let namespace_id = self.namespace_id.replace(id);

        n.body.visit_with(self);

        self.namespace_id = namespace_id;
    }

    fn visit_export_decl(&mut self, node: &ExportDecl) {
        node.visit_children_with(self);

        match &node.decl {
            Decl::Var(var_decl) => {
                self.exported_binding.extend({
                    find_pat_ids(&var_decl.decls)
                        .into_iter()
                        .zip(iter::repeat(self.namespace_id.clone()))
                });
            }
            Decl::TsEnum(ts_enum_decl) => {
                self.exported_binding
                    .insert(ts_enum_decl.id.to_id(), self.namespace_id.clone());
            }
            Decl::TsModule(ts_module_decl) => {
                self.exported_binding
                    .insert(ts_module_decl.id.to_id(), self.namespace_id.clone());
            }
            _ => {}
        }
    }

    fn visit_export_named_specifier(&mut self, node: &ExportNamedSpecifier) {
        if let ModuleExportName::Ident(ident) = &node.orig {
            self.exported_binding
                .insert(ident.to_id(), self.namespace_id.clone());
        }
    }

    fn visit_export_default_expr(&mut self, node: &ExportDefaultExpr) {
        node.visit_children_with(self);

        if let Expr::Ident(ident) = &*node.expr {
            self.exported_binding
                .insert(ident.to_id(), self.namespace_id.clone());
        }
    }

    fn visit_ts_import_equals_decl(&mut self, node: &TsImportEqualsDecl) {
        node.visit_children_with(self);

        if node.is_export {
            self.exported_binding
                .insert(node.id.to_id(), self.namespace_id.clone());
        }
    }

    fn visit_expr(&mut self, node: &Expr) {
        maybe_grow_default(|| node.visit_children_with(self));
    }
}

impl VisitMut for Transform {
    noop_visit_mut_type!();

    fn visit_mut_program(&mut self, node: &mut Program) {
        node.visit_with(self);

        if !self.exported_binding.is_empty() {
            self.ref_rewriter = Some(RefRewriter {
                query: ExportQuery {
                    export_name: self.exported_binding.clone(),
                },
            });
        }
        node.visit_mut_children_with(self);
    }

    fn visit_mut_module(&mut self, node: &mut Module) {
        self.visit_mut_for_ts_import_export(node);

        node.visit_mut_children_with(self);

        if !self.export_var_list.is_empty() {
            let decls = self
                .export_var_list
                .take()
                .into_iter()
                .map(id_to_var_declarator)
                .collect();

            node.body.push(
                ExportDecl {
                    decl: VarDecl {
                        decls,
                        ..Default::default()
                    }
                    .into(),
                    span: DUMMY_SP,
                }
                .into(),
            )
        }
    }

    fn visit_mut_module_items(&mut self, node: &mut Vec<ModuleItem>) {
        let var_list = self.var_list.take();
        node.retain_mut(|item| {
            let is_empty = item.as_stmt().map(Stmt::is_empty).unwrap_or(false);
            item.visit_mut_with(self);
            // Remove those folded into Empty
            is_empty || !item.as_stmt().map(Stmt::is_empty).unwrap_or(false)
        });
        let var_list = mem::replace(&mut self.var_list, var_list);

        if !var_list.is_empty() {
            let decls = var_list.into_iter().map(id_to_var_declarator).collect();

            node.push(
                VarDecl {
                    decls,
                    ..Default::default()
                }
                .into(),
            )
        }
    }

    fn visit_mut_class_members(&mut self, node: &mut Vec<ClassMember>) {
        let prop_list = self.in_class_prop.take();
        let init_list = self.in_class_prop_init.take();

        node.visit_mut_children_with(self);
        let prop_list = mem::replace(&mut self.in_class_prop, prop_list);
        let init_list = mem::replace(&mut self.in_class_prop_init, init_list);

        if !prop_list.is_empty() {
            if self.native_class_properties {
                self.reorder_class_prop_decls(node, prop_list, init_list);
            } else {
                self.reorder_class_prop_decls_and_inits(node, prop_list, init_list);
            }
        }
    }

    fn visit_mut_constructor(&mut self, node: &mut Constructor) {
        node.params
            .iter_mut()
            .for_each(|param_or_ts_param_prop| match param_or_ts_param_prop {
                ParamOrTsParamProp::TsParamProp(ts_param_prop) => {
                    let TsParamProp {
                        span,
                        decorators,
                        param,
                        ..
                    } = ts_param_prop;

                    let (pat, expr, id) = match param {
                        TsParamPropParam::Ident(binding_ident) => {
                            let id = binding_ident.to_id();
                            let prop_name = PropName::Ident(IdentName::from(&*binding_ident));
                            let value = Ident::from(&*binding_ident).into();

                            (
                                binding_ident.clone().into(),
                                assign_value_to_this_prop(prop_name, value),
                                id,
                            )
                        }
                        TsParamPropParam::Assign(assign_pat) => {
                            let AssignPat { left, .. } = &assign_pat;

                            let Pat::Ident(binding_ident) = &**left else {
                                unreachable!("destructuring pattern inside TsParameterProperty");
                            };

                            let id = binding_ident.id.to_id();
                            let prop_name = PropName::Ident(binding_ident.id.clone().into());
                            let value = binding_ident.id.clone().into();

                            (
                                assign_pat.clone().into(),
                                assign_value_to_this_prop(prop_name, value),
                                id,
                            )
                        }
                        #[cfg(swc_ast_unknown)]
                        _ => panic!("unable to access unknown nodes"),
                    };

                    self.in_class_prop.push(id);
                    self.in_class_prop_init.push(expr);

                    *param_or_ts_param_prop = Param {
                        span: *span,
                        decorators: decorators.take(),
                        pat,
                    }
                    .into();
                }
                ParamOrTsParamProp::Param(..) => {}
                #[cfg(swc_ast_unknown)]
                _ => panic!("unable to access unknown nodes"),
            });

        node.params.visit_mut_children_with(self);
        node.body.visit_mut_children_with(self);
    }

    fn visit_mut_stmts(&mut self, node: &mut Vec<Stmt>) {
        let var_list = self.var_list.take();
        node.retain_mut(|stmt| {
            let is_empty = stmt.is_empty();
            stmt.visit_mut_with(self);
            // Remove those folded into Empty
            is_empty || !stmt.is_empty()
        });
        let var_list = mem::replace(&mut self.var_list, var_list);
        if !var_list.is_empty() {
            let decls = var_list.into_iter().map(id_to_var_declarator).collect();
            node.push(
                VarDecl {
                    decls,
                    ..Default::default()
                }
                .into(),
            )
        }
    }

    fn visit_mut_ts_namespace_decl(&mut self, node: &mut TsNamespaceDecl) {
        let id = node.id.to_id();
        let namespace_id = self.namespace_id.replace(id);

        node.body.visit_mut_with(self);

        self.namespace_id = namespace_id;
    }

    fn visit_mut_ts_module_decl(&mut self, node: &mut TsModuleDecl) {
        let id = node.id.to_id();

        let namespace_id = self.namespace_id.replace(id);

        node.body.visit_mut_with(self);

        self.namespace_id = namespace_id;
    }

    fn visit_mut_stmt(&mut self, node: &mut Stmt) {
        node.visit_mut_children_with(self);

        let Stmt::Decl(decl) = node else {
            return;
        };

        match self.fold_decl(decl.take(), false) {
            FoldedDecl::Decl(var_decl) => *decl = var_decl,
            FoldedDecl::Expr(stmt) => *node = stmt,
            FoldedDecl::Empty => {
                node.take();
            }
        }
    }

    fn visit_mut_module_item(&mut self, node: &mut ModuleItem) {
        node.visit_mut_children_with(self);

        if let Some(ExportDecl { decl, .. }) = node
            .as_mut_module_decl()
            .and_then(ModuleDecl::as_mut_export_decl)
        {
            match self.fold_decl(decl.take(), true) {
                FoldedDecl::Decl(var_decl) => *decl = var_decl,
                FoldedDecl::Expr(stmt) => *node = stmt.into(),
                FoldedDecl::Empty => {
                    node.take();
                }
            }
        }
    }

    fn visit_mut_export_default_decl(&mut self, node: &mut ExportDefaultDecl) {
        node.visit_mut_children_with(self);

        if let DefaultDecl::Class(ClassExpr {
            ident: Some(ref ident),
            ..
        })
        | DefaultDecl::Fn(FnExpr {
            ident: Some(ref ident),
            ..
        }) = node.decl
        {
            self.decl_id_record.insert(ident.to_id());
        }
    }

    fn visit_mut_export_decl(&mut self, node: &mut ExportDecl) {
        if self.ref_rewriter.is_some() {
            if let Decl::Var(var_decl) = &mut node.decl {
                // visit inner directly to bypass visit_mut_var_declarator
                for decl in var_decl.decls.iter_mut() {
                    decl.name.visit_mut_with(self);
                    decl.init.visit_mut_with(self);
                }
                return;
            }
        }
        node.visit_mut_children_with(self);
    }

    fn visit_mut_prop(&mut self, node: &mut Prop) {
        node.visit_mut_children_with(self);

        if let Some(ref_rewriter) = self.ref_rewriter.as_mut() {
            ref_rewriter.exit_prop(node);
        }
    }

    fn visit_mut_var_declarator(&mut self, n: &mut VarDeclarator) {
        let ref_rewriter = self.ref_rewriter.take();
        n.name.visit_mut_with(self);
        self.ref_rewriter = ref_rewriter;
        n.init.visit_mut_with(self);
    }

    fn visit_mut_pat(&mut self, node: &mut Pat) {
        node.visit_mut_children_with(self);

        if let Some(ref_rewriter) = self.ref_rewriter.as_mut() {
            ref_rewriter.exit_pat(node);
        }
    }

    fn visit_mut_expr(&mut self, node: &mut Expr) {
        self.enter_expr_for_inline_enum(node);

        maybe_grow_default(|| node.visit_mut_children_with(self));

        if let Some(ref_rewriter) = self.ref_rewriter.as_mut() {
            ref_rewriter.exit_expr(node);
        }
    }

    fn visit_mut_assign_expr(&mut self, n: &mut AssignExpr) {
        let is_lhs = mem::replace(&mut self.is_lhs, true);
        n.left.visit_mut_with(self);
        self.is_lhs = false;
        n.right.visit_mut_with(self);
        self.is_lhs = is_lhs;
    }

    fn visit_mut_assign_pat(&mut self, n: &mut AssignPat) {
        let is_lhs = mem::replace(&mut self.is_lhs, true);
        n.left.visit_mut_with(self);
        self.is_lhs = false;
        n.right.visit_mut_with(self);
        self.is_lhs = is_lhs;
    }

    fn visit_mut_update_expr(&mut self, n: &mut UpdateExpr) {
        let is_lhs = mem::replace(&mut self.is_lhs, true);
        n.arg.visit_mut_with(self);
        self.is_lhs = is_lhs;
    }

    fn visit_mut_assign_pat_prop(&mut self, n: &mut AssignPatProp) {
        n.key.visit_mut_with(self);
        let is_lhs = mem::replace(&mut self.is_lhs, false);
        n.value.visit_mut_with(self);
        self.is_lhs = is_lhs;
    }

    fn visit_mut_member_expr(&mut self, n: &mut MemberExpr) {
        let is_lhs = mem::replace(&mut self.is_lhs, false);
        n.visit_mut_children_with(self);
        self.is_lhs = is_lhs;
    }

    fn visit_mut_simple_assign_target(&mut self, node: &mut SimpleAssignTarget) {
        node.visit_mut_children_with(self);

        if let Some(ref_rewriter) = self.ref_rewriter.as_mut() {
            ref_rewriter.exit_simple_assign_target(node);
        }
    }

    fn visit_mut_jsx_element_name(&mut self, node: &mut JSXElementName) {
        node.visit_mut_children_with(self);

        if let Some(ref_rewriter) = self.ref_rewriter.as_mut() {
            ref_rewriter.exit_jsx_element_name(node);
        }
    }

    fn visit_mut_jsx_object(&mut self, node: &mut JSXObject) {
        node.visit_mut_children_with(self);

        if let Some(ref_rewriter) = self.ref_rewriter.as_mut() {
            ref_rewriter.exit_jsx_object(node);
        }
    }

    fn visit_mut_object_pat_prop(&mut self, n: &mut ObjectPatProp) {
        n.visit_mut_children_with(self);

        if let Some(ref_rewriter) = self.ref_rewriter.as_mut() {
            ref_rewriter.exit_object_pat_prop(n);
        }
    }
}

enum FoldedDecl {
    Empty,
    Decl(Decl),
    Expr(Stmt),
}

impl Transform {
    fn fold_decl(&mut self, node: Decl, is_export: bool) -> FoldedDecl {
        match node {
            Decl::TsModule(ts_module) => {
                let id = ts_module.id.to_id();

                if self.decl_id_record.insert(id.clone()) {
                    if is_export {
                        if self.namespace_id.is_none() {
                            self.export_var_list.push(id);
                        }
                    } else {
                        self.var_list.push(id);
                    }
                }

                self.transform_ts_module(*ts_module, is_export)
            }
            Decl::TsEnum(ts_enum) => {
                let id = ts_enum.id.to_id();

                let is_first = self.decl_id_record.insert(id);

                self.transform_ts_enum(*ts_enum, is_first, is_export)
            }
            Decl::Fn(FnDecl { ref ident, .. }) | Decl::Class(ClassDecl { ref ident, .. }) => {
                self.decl_id_record.insert(ident.to_id());
                FoldedDecl::Decl(node)
            }
            decl => FoldedDecl::Decl(decl),
        }
    }
}

struct InitArg<'a> {
    id: &'a Ident,
    namespace_id: Option<&'a Id>,
}

impl InitArg<'_> {
    // {}
    fn empty() -> ExprOrSpread {
        ExprOrSpread {
            spread: None,
            expr: ObjectLit::default().into(),
        }
    }

    // N
    fn get(&self) -> ExprOrSpread {
        self.namespace_id
            .cloned()
            .map_or_else(
                || -> Expr { self.id.clone().into() },
                |namespace_id| namespace_id.make_member(self.id.clone().into()).into(),
            )
            .into()
    }

    // N || {}
    fn or_empty(&self) -> ExprOrSpread {
        let expr = self.namespace_id.cloned().map_or_else(
            || -> Expr { self.id.clone().into() },
            |namespace_id| namespace_id.make_member(self.id.clone().into()).into(),
        );

        let bin = BinExpr {
            op: op!("||"),
            left: expr.into(),
            right: ObjectLit::default().into(),
            ..Default::default()
        };

        ExprOrSpread {
            spread: None,
            expr: bin.into(),
        }
    }

    // N || (N = {})
    fn or_assign_empty(&self) -> ExprOrSpread {
        let expr = self.namespace_id.cloned().map_or_else(
            || -> Expr { self.id.clone().into() },
            |namespace_id| namespace_id.make_member(self.id.clone().into()).into(),
        );

        let assign = self.namespace_id.cloned().map_or_else(
            || ObjectLit::default().make_assign_to(op!("="), self.id.clone().into()),
            |namespace_id| {
                ObjectLit::default().make_assign_to(
                    op!("="),
                    namespace_id.make_member(self.id.clone().into()).into(),
                )
            },
        );

        let bin = BinExpr {
            op: op!("||"),
            left: expr.into(),
            right: assign.into(),
            ..Default::default()
        };

        ExprOrSpread {
            spread: None,
            expr: bin.into(),
        }
    }
}

impl Transform {
    fn transform_ts_enum(
        &mut self,
        ts_enum: TsEnumDecl,
        is_first: bool,
        is_export: bool,
    ) -> FoldedDecl {
        let TsEnumDecl {
            span,
            declare,
            is_const,
            id,
            members,
        } = ts_enum;

        debug_assert!(!declare);

        let ts_enum_safe_remove = !self.verbatim_module_syntax
            && is_const
            && !is_export
            && !self.exported_binding.contains_key(&id.to_id());

        let member_list: Vec<_> = members
            .into_iter()
            .map(|m| {
                let span = m.span;
                let name = enum_member_id_atom(&m.id);

                let key = TsEnumRecordKey {
                    enum_id: id.to_id(),
                    member_name: name.clone(),
                };

                let value = self.enum_record.get(&key).unwrap().clone();

                EnumMemberItem { span, name, value }
            })
            .filter(|m| !ts_enum_safe_remove || !m.is_const())
            .collect();

        if member_list.is_empty() && is_const {
            return FoldedDecl::Empty;
        }

        let opaque = member_list
            .iter()
            .any(|item| matches!(item.value, TsEnumRecordValue::Opaque(..)));

        let stmts = member_list
            .into_iter()
            .filter(|item| !ts_enum_safe_remove || !item.is_const())
            .map(|item| item.build_assign(&id.to_id()));

        let namespace_export = self.namespace_id.is_some() && is_export;
        let iife = !is_first || namespace_export;

        let body = if !iife {
            let return_stmt: Stmt = ReturnStmt {
                arg: Some(id.clone().into()),
                ..Default::default()
            }
            .into();

            let stmts = stmts.chain(iter::once(return_stmt)).collect();

            BlockStmt {
                stmts,
                ..Default::default()
            }
        } else {
            BlockStmt {
                stmts: stmts.collect(),
                ..Default::default()
            }
        };

        let var_kind = if is_export || id.ctxt == self.top_level_ctxt {
            VarDeclKind::Var
        } else {
            VarDeclKind::Let
        };

        let init_arg = 'init_arg: {
            let init_arg = InitArg {
                id: &id,
                namespace_id: self.namespace_id.as_ref().filter(|_| is_export),
            };
            if !is_first {
                break 'init_arg init_arg.get();
            }

            if namespace_export {
                break 'init_arg init_arg.or_assign_empty();
            }

            if is_export || var_kind == VarDeclKind::Let {
                InitArg::empty()
            } else {
                init_arg.or_empty()
            }
        };

        let expr = Factory::function(vec![id.clone().into()], body).as_call(
            if iife || opaque { DUMMY_SP } else { PURE_SP },
            vec![init_arg],
        );

        if iife {
            FoldedDecl::Expr(
                ExprStmt {
                    span,
                    expr: expr.into(),
                }
                .into(),
            )
        } else {
            let var_declarator = VarDeclarator {
                span,
                name: id.into(),
                init: Some(expr.into()),
                definite: false,
            };

            FoldedDecl::Decl(
                VarDecl {
                    span,
                    kind: var_kind,
                    decls: vec![var_declarator],
                    ..Default::default()
                }
                .into(),
            )
        }
    }

    fn transform_ts_enum_member(
        member: TsEnumMember,
        enum_id: &Id,
        default_init: &TsEnumRecordValue,
        record: &TsEnumRecord,
        unresolved_ctxt: SyntaxContext,
    ) -> TsEnumRecordValue {
        member
            .init
            .map(|expr| {
                EnumValueComputer {
                    enum_id,
                    unresolved_ctxt,
                    record,
                }
                .compute(expr)
            })
            .filter(TsEnumRecordValue::has_value)
            .unwrap_or_else(|| default_init.clone())
    }
}

impl Transform {
    fn transform_ts_module(&self, ts_module: TsModuleDecl, is_export: bool) -> FoldedDecl {
        debug_assert!(!ts_module.declare);
        debug_assert!(!ts_module.global);

        let TsModuleDecl {
            span,
            id: TsModuleName::Ident(module_ident),
            body: Some(body),
            ..
        } = ts_module
        else {
            unreachable!();
        };

        let body = Self::transform_ts_namespace_body(module_ident.to_id(), body);

        let init_arg = InitArg {
            id: &module_ident,
            namespace_id: self.namespace_id.as_ref().filter(|_| is_export),
        }
        .or_assign_empty();

        let expr = Factory::function(vec![module_ident.clone().into()], body)
            .as_call(DUMMY_SP, vec![init_arg])
            .into();

        FoldedDecl::Expr(ExprStmt { span, expr }.into())
    }

    fn transform_ts_namespace_body(id: Id, body: TsNamespaceBody) -> BlockStmt {
        let TsNamespaceDecl {
            span,
            declare,
            global,
            id: local_name,
            body,
        } = match body {
            TsNamespaceBody::TsModuleBlock(ts_module_block) => {
                return Self::transform_ts_module_block(id, ts_module_block);
            }
            TsNamespaceBody::TsNamespaceDecl(ts_namespace_decl) => ts_namespace_decl,
            #[cfg(swc_ast_unknown)]
            _ => panic!("unable to access unknown nodes"),
        };

        debug_assert!(!declare);
        debug_assert!(!global);

        let body = Self::transform_ts_namespace_body(local_name.to_id(), *body);

        let init_arg = InitArg {
            id: &local_name,
            namespace_id: Some(&id.to_id()),
        }
        .or_assign_empty();

        let expr =
            Factory::function(vec![local_name.into()], body).as_call(DUMMY_SP, vec![init_arg]);

        BlockStmt {
            span,
            stmts: vec![expr.into_stmt()],
            ..Default::default()
        }
    }

    /// Note:
    /// All exported variable declarations are transformed into assignment to
    /// the namespace. All references to the exported binding will be
    /// replaced with qualified access to the namespace property.
    ///
    /// Exported function and class will be treat as const exported which is in
    /// line with how the TypeScript compiler handles exports.
    ///
    /// Inline exported syntax should not be used with function which will lead
    /// to issues with function hoisting.
    ///
    /// Input:
    /// ```TypeScript
    /// export const foo = init, { bar: baz = init } = init;
    ///
    /// export function a() {}
    ///
    /// export let b = init;
    /// ```
    ///
    /// Output:
    /// ```TypeScript
    /// NS.foo = init, { bar: NS.baz = init } = init;
    ///
    /// function a() {}
    /// NS.a = a;
    ///
    /// NS.b = init;
    /// ```
    fn transform_ts_module_block(id: Id, TsModuleBlock { span, body }: TsModuleBlock) -> BlockStmt {
        let mut stmts = Vec::new();

        for module_item in body {
            match module_item {
                ModuleItem::Stmt(stmt) => stmts.push(stmt),
                ModuleItem::ModuleDecl(ModuleDecl::ExportDecl(ExportDecl {
                    decl, span, ..
                })) => match decl {
                    Decl::Class(ClassDecl { ref ident, .. })
                    | Decl::Fn(FnDecl { ref ident, .. }) => {
                        let assign_stmt = Self::assign_prop(&id, ident, span);
                        stmts.push(decl.into());
                        stmts.push(assign_stmt);
                    }
                    Decl::Var(var_decl) => {
                        let mut exprs: Vec<Box<_>> = var_decl
                            .decls
                            .into_iter()
                            .flat_map(
                                |VarDeclarator {
                                     span, name, init, ..
                                 }| {
                                    let right = init?;
                                    let left = name.try_into().unwrap();

                                    Some(
                                        AssignExpr {
                                            span,
                                            left,
                                            op: op!("="),
                                            right,
                                        }
                                        .into(),
                                    )
                                },
                            )
                            .collect();

                        if exprs.is_empty() {
                            continue;
                        }

                        let expr = if exprs.len() == 1 {
                            exprs.pop().unwrap()
                        } else {
                            SeqExpr {
                                span: DUMMY_SP,
                                exprs,
                            }
                            .into()
                        };

                        stmts.push(
                            ExprStmt {
                                span: var_decl.span,
                                expr,
                            }
                            .into(),
                        );
                    }
                    decl => unreachable!("{decl:?}"),
                },
                ModuleItem::ModuleDecl(ModuleDecl::TsImportEquals(decl)) => {
                    match decl.module_ref {
                        TsModuleRef::TsEntityName(ts_entity_name) => {
                            let init = Self::ts_entity_name_to_expr(ts_entity_name);

                            // export impot foo = bar.baz
                            let stmt = if decl.is_export {
                                // Foo.foo = bar.baz
                                let left = id.clone().make_member(decl.id.clone().into());
                                let expr = init.make_assign_to(op!("="), left.into());

                                ExprStmt {
                                    span: decl.span,
                                    expr: expr.into(),
                                }
                                .into()
                            } else {
                                // const foo = bar.baz
                                let mut var_decl =
                                    init.into_var_decl(VarDeclKind::Const, decl.id.clone().into());

                                var_decl.span = decl.span;

                                var_decl.into()
                            };

                            stmts.push(stmt);
                        }
                        TsModuleRef::TsExternalModuleRef(..) => {
                            // TS1147
                            if HANDLER.is_set() {
                                HANDLER.with(|handler| {
                                    handler
                                    .struct_span_err(
                                        decl.span,
                                        r#"Import declarations in a namespace cannot reference a module."#,
                                    )
                                    .emit();
                                });
                            }
                        }
                        #[cfg(swc_ast_unknown)]
                        _ => panic!("unable to access unknown nodes"),
                    }
                }
                item => {
                    if HANDLER.is_set() {
                        HANDLER.with(|handler| {
                            handler
                                .struct_span_err(
                                    item.span(),
                                    r#"ESM-style module declarations are not permitted in a namespace."#,
                                )
                                .emit();
                        });
                    }
                }
            }
        }

        BlockStmt {
            span,
            stmts,
            ..Default::default()
        }
    }
}

impl Transform {
    fn reorder_class_prop_decls_and_inits(
        &mut self,
        class_member_list: &mut Vec<ClassMember>,
        prop_list: Vec<Id>,
        mut init_list: Vec<Box<Expr>>,
    ) {
        let mut constructor = None;
        let mut cons_index = 0;
        for (index, member) in class_member_list.iter_mut().enumerate() {
            match member {
                ClassMember::Constructor(..) => {
                    let empty = EmptyStmt {
                        span: member.span(),
                    }
                    .into();
                    constructor = mem::replace(member, empty).constructor();
                    cons_index = index;
                }
                ClassMember::ClassProp(ClassProp {
                    key,
                    value: value @ Some(..),
                    is_static: false,
                    span,
                    ..
                }) => {
                    let key = match &mut *key {
                        PropName::Computed(ComputedPropName { span, expr })
                            if !is_literal(expr) =>
                        {
                            let ident = alias_ident_for(expr, "_key");

                            self.var_list.push(ident.to_id());

                            **expr = expr.take().make_assign_to(op!("="), ident.clone().into());

                            PropName::Computed(ComputedPropName {
                                span: *span,
                                expr: ident.into(),
                            })
                        }
                        _ => key.clone(),
                    };

                    let mut init = assign_value_to_this_prop(key, *value.take().unwrap());
                    init.set_span(*span);

                    init_list.push(init);
                }
                ClassMember::PrivateProp(PrivateProp {
                    key,
                    value: value @ Some(..),
                    is_static: false,
                    span,
                    ..
                }) => {
                    let mut init =
                        assign_value_to_this_private_prop(key.clone(), *value.take().unwrap());
                    init.set_span(*span);
                    init_list.push(init);
                }
                _ => {}
            }
        }

        if let Some(mut constructor) = constructor {
            inject_after_super(&mut constructor, init_list);

            if let Some(c) = class_member_list
                .get_mut(cons_index)
                .filter(|m| m.is_empty() && m.span() == constructor.span)
            {
                *c = constructor.into();
            } else {
                class_member_list.push(constructor.into());
            }
        }

        class_member_list.splice(
            0..0,
            prop_list
                .into_iter()
                .map(Ident::from)
                .map(PropName::from)
                .map(|key| ClassProp {
                    key,
                    ..Default::default()
                })
                .map(ClassMember::ClassProp),
        );
    }

    fn reorder_class_prop_decls(
        &mut self,
        class_member_list: &mut Vec<ClassMember>,
        prop_list: Vec<Id>,
        init_list: Vec<Box<Expr>>,
    ) {
        if let Some(constructor) = class_member_list
            .iter_mut()
            .find_map(|m| m.as_mut_constructor())
        {
            inject_after_super(constructor, init_list);
        }

        class_member_list.splice(
            0..0,
            prop_list
                .into_iter()
                .map(Ident::from)
                .map(PropName::from)
                .map(|key| ClassProp {
                    key,
                    ..Default::default()
                })
                .map(ClassMember::ClassProp),
        );
    }
}

impl Transform {
    // Foo.x = x;
    fn assign_prop(id: &Id, prop: &Ident, span: Span) -> Stmt {
        let expr = prop
            .clone()
            .make_assign_to(op!("="), id.clone().make_member(prop.clone().into()).into());

        ExprStmt {
            span,
            expr: expr.into(),
        }
        .into()
    }

    fn ts_entity_name_to_expr(n: TsEntityName) -> Expr {
        match n {
            TsEntityName::Ident(i) => i.into(),
            TsEntityName::TsQualifiedName(q) => {
                let TsQualifiedName { span, left, right } = *q;

                MemberExpr {
                    span,
                    obj: Box::new(Self::ts_entity_name_to_expr(left)),
                    prop: MemberProp::Ident(right),
                }
                .into()
            }
            #[cfg(swc_ast_unknown)]
            _ => panic!("unable to access unknown nodes"),
        }
    }
}

impl Transform {
    fn enter_expr_for_inline_enum(&mut self, node: &mut Expr) {
        if self.is_lhs {
            return;
        }

        if let Expr::Member(MemberExpr { obj, prop, .. }) = node {
            let Some(enum_id) = get_enum_id(obj) else {
                return;
            };

            if self.ts_enum_is_mutable && !self.const_enum.contains(&enum_id) {
                return;
            }

            let Some(member_name) = get_member_key(prop) else {
                return;
            };

            let key = TsEnumRecordKey {
                enum_id,
                member_name,
            };

            let Some(value) = self.enum_record.get(&key) else {
                return;
            };

            if value.is_const() {
                *node = value.clone().into();
            }
        }
    }

    fn visit_mut_for_ts_import_export(&mut self, node: &mut Module) {
        let mut should_inject = false;
        let create_require = private_ident!("_createRequire");
        let require = private_ident!("__require");

        // NOTE: This is not correct!
        // However, all unresolved_span are used in TsImportExportAssignConfig::Classic
        // which is deprecated and not used in real world.
        let unresolved_ctxt = self.unresolved_ctxt;
        let cjs_require = quote_ident!(unresolved_ctxt, "require");
        let cjs_exports = quote_ident!(unresolved_ctxt, "exports");

        let mut cjs_export_assign = None;

        for module_item in &mut node.body {
            match module_item {
                ModuleItem::ModuleDecl(ModuleDecl::TsImportEquals(decl)) if !decl.is_type_only => {
                    debug_assert_ne!(
                        decl.id.ctxt, self.unresolved_ctxt,
                        "TsImportEquals has top-level context and it should not be identical to \
                         the unresolved mark"
                    );
                    debug_assert_eq!(decl.id.ctxt, self.top_level_ctxt);

                    match &mut decl.module_ref {
                        // import foo = bar.baz
                        TsModuleRef::TsEntityName(ts_entity_name) => {
                            let init = Self::ts_entity_name_to_expr(ts_entity_name.clone());

                            let mut var_decl =
                                init.into_var_decl(VarDeclKind::Const, decl.id.take().into());

                            *module_item = if decl.is_export {
                                ExportDecl {
                                    span: decl.span,
                                    decl: var_decl.into(),
                                }
                                .into()
                            } else {
                                var_decl.span = decl.span;
                                var_decl.into()
                            };
                        }
                        // import foo = require("foo")
                        TsModuleRef::TsExternalModuleRef(TsExternalModuleRef { expr, .. }) => {
                            match self.import_export_assign_config {
                                TsImportExportAssignConfig::Classic => {
                                    // require("foo");
                                    let mut init = cjs_require
                                        .clone()
                                        .as_call(DUMMY_SP, vec![expr.take().as_arg()]);

                                    // exports.foo = require("foo");
                                    if decl.is_export {
                                        init = init.make_assign_to(
                                            op!("="),
                                            cjs_exports
                                                .clone()
                                                .make_member(decl.id.clone().into())
                                                .into(),
                                        )
                                    }

                                    // const foo = require("foo");
                                    // const foo = exports.foo = require("foo");
                                    let mut var_decl = init
                                        .into_var_decl(VarDeclKind::Const, decl.id.take().into());
                                    var_decl.span = decl.span;

                                    *module_item = var_decl.into();
                                }
                                TsImportExportAssignConfig::Preserve => {}
                                TsImportExportAssignConfig::NodeNext => {
                                    should_inject = true;

                                    let mut var_decl = require
                                        .clone()
                                        .as_call(DUMMY_SP, vec![expr.take().as_arg()])
                                        .into_var_decl(VarDeclKind::Const, decl.id.take().into());

                                    *module_item = if decl.is_export {
                                        ExportDecl {
                                            span: decl.span,
                                            decl: var_decl.into(),
                                        }
                                        .into()
                                    } else {
                                        var_decl.span = decl.span;
                                        var_decl.into()
                                    };
                                }
                                TsImportExportAssignConfig::EsNext => {
                                    // TS1202
                                    if HANDLER.is_set() {
                                        HANDLER.with(|handler| {
                                            handler.struct_span_err(
                                                decl.span,
                                                r#"Import assignment cannot be used when targeting ECMAScript modules. Consider using `import * as ns from "mod"`, `import {a} from "mod"`, `import d from "mod"`, or another module format instead."#,
                                            )
                                            .emit();
                                        });
                                    }
                                }
                            }
                        }
                        #[cfg(swc_ast_unknown)]
                        _ => panic!("unable to access unknown nodes"),
                    }
                }
                ModuleItem::ModuleDecl(ModuleDecl::TsExportAssignment(..)) => {
                    let ts_export_assign = module_item
                        .take()
                        .module_decl()
                        .unwrap()
                        .ts_export_assignment()
                        .unwrap();

                    cjs_export_assign.get_or_insert(ts_export_assign);
                }
                _ => {}
            }
        }

        if should_inject {
            node.body.prepend_stmts([
                // import { createRequire } from "module";
                ImportDecl {
                    span: DUMMY_SP,
                    specifiers: vec![ImportNamedSpecifier {
                        span: DUMMY_SP,
                        local: create_require.clone(),
                        imported: Some(quote_ident!("createRequire").into()),
                        is_type_only: false,
                    }
                    .into()],
                    src: Box::new(quote_str!("module")),
                    type_only: false,
                    with: None,
                    phase: Default::default(),
                }
                .into(),
                // const __require = _createRequire(import.meta.url);
                create_require
                    .as_call(
                        DUMMY_SP,
                        vec![MetaPropExpr {
                            span: DUMMY_SP,
                            kind: MetaPropKind::ImportMeta,
                        }
                        .make_member(quote_ident!("url"))
                        .as_arg()],
                    )
                    .into_var_decl(VarDeclKind::Const, require.clone().into())
                    .into(),
            ]);
        }

        if let Some(cjs_export_assign) = cjs_export_assign {
            match self.import_export_assign_config {
                TsImportExportAssignConfig::Classic => {
                    let TsExportAssignment { expr, span } = cjs_export_assign;

                    let stmt = ExprStmt {
                        span,
                        expr: Box::new(
                            expr.make_assign_to(
                                op!("="),
                                member_expr!(unresolved_ctxt, Default::default(), module.exports)
                                    .into(),
                            ),
                        ),
                    }
                    .into();

                    if let Some(item) = node
                        .body
                        .last_mut()
                        .and_then(ModuleItem::as_mut_stmt)
                        .filter(|stmt| stmt.is_empty())
                    {
                        *item = stmt;
                    } else {
                        node.body.push(stmt.into());
                    }
                }
                TsImportExportAssignConfig::Preserve => {
                    node.body.push(cjs_export_assign.into());
                }
                TsImportExportAssignConfig::NodeNext | TsImportExportAssignConfig::EsNext => {
                    // TS1203
                    if HANDLER.is_set() {
                        HANDLER.with(|handler| {
                            handler
                                .struct_span_err(
                                    cjs_export_assign.span,
                                    r#"Export assignment cannot be used when targeting ECMAScript modules. Consider using `export default` or another module format instead."#,
                                )
                                .emit()
                        });
                    }
                }
            }
        }
    }
}

struct ExportQuery {
    export_name: FxHashMap<Id, Option<Id>>,
}

impl QueryRef for ExportQuery {
    fn query_ref(&self, export_name: &Ident) -> Option<Box<Expr>> {
        self.export_name
            .get(&export_name.to_id())?
            .clone()
            .map(|namespace_id| namespace_id.make_member(export_name.clone().into()).into())
    }

    fn query_lhs(&self, ident: &Ident) -> Option<Box<Expr>> {
        self.query_ref(ident)
    }

    fn query_jsx(&self, ident: &Ident) -> Option<JSXElementName> {
        self.export_name
            .get(&ident.to_id())?
            .clone()
            .map(|namespace_id| {
                JSXMemberExpr {
                    span: DUMMY_SP,
                    obj: JSXObject::Ident(namespace_id.into()),
                    prop: ident.clone().into(),
                }
                .into()
            })
    }
}

struct EnumMemberItem {
    span: Span,
    name: Atom,
    value: TsEnumRecordValue,
}

impl EnumMemberItem {
    fn is_const(&self) -> bool {
        self.value.is_const()
    }

    fn build_assign(self, enum_id: &Id) -> Stmt {
        let is_string = self.value.is_string();
        let value: Expr = self.value.into();

        let inner_assign = value.make_assign_to(
            op!("="),
            Ident::from(enum_id.clone())
                .computed_member(self.name.clone())
                .into(),
        );

        let outer_assign = if is_string {
            inner_assign
        } else {
            let value: Expr = self.name.clone().into();

            value.make_assign_to(
                op!("="),
                Ident::from(enum_id.clone())
                    .computed_member(inner_assign)
                    .into(),
            )
        };

        ExprStmt {
            span: self.span,
            expr: outer_assign.into(),
        }
        .into()
    }
}

trait ModuleId {
    fn to_id(&self) -> Id;
}

impl ModuleId for TsModuleName {
    fn to_id(&self) -> Id {
        self.as_ident()
            .expect("Only ambient modules can use quoted names.")
            .to_id()
    }
}

fn id_to_var_declarator(id: Id) -> VarDeclarator {
    VarDeclarator {
        span: DUMMY_SP,
        name: id.into(),
        init: None,
        definite: false,
    }
}

fn get_enum_id(e: &Expr) -> Option<Id> {
    if let Expr::Ident(ident) = e {
        Some(ident.to_id())
    } else {
        None
    }
}

fn get_member_key(prop: &MemberProp) -> Option<Atom> {
    match prop {
        MemberProp::Ident(ident) => Some(ident.sym.clone()),
        MemberProp::Computed(ComputedPropName { expr, .. }) => match &**expr {
            Expr::Lit(Lit::Str(Str { value, .. })) => Some(value.to_atom_lossy().into_owned()),
            Expr::Tpl(Tpl { exprs, quasis, .. }) => match (exprs.len(), quasis.len()) {
                (0, 1) => quasis[0]
                    .cooked
                    .as_ref()
                    .map(|cooked| cooked.to_atom_lossy().into_owned())
                    .or_else(|| Some(quasis[0].raw.clone())),
                _ => None,
            },
            _ => None,
        },
        MemberProp::PrivateName(_) => None,
        #[cfg(swc_ast_unknown)]
        _ => panic!("unable to access unknown nodes"),
    }
}
