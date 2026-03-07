#![deny(clippy::all)]
#![allow(clippy::boxed_local)]
#![allow(clippy::mutable_key_type)]
#![allow(clippy::match_like_matches_macro)]
#![allow(clippy::vec_box)]
#![cfg_attr(not(feature = "concurrent"), allow(unused))]

#[doc(hidden)]
pub extern crate swc_atoms;
#[doc(hidden)]
pub extern crate swc_common;
#[doc(hidden)]
pub extern crate swc_ecma_ast;

use std::{borrow::Cow, hash::Hash, num::FpCategory, ops::Add};

use number::ToJsString;
use once_cell::sync::Lazy;
use parallel::{Parallel, ParallelExt};
use rustc_hash::{FxHashMap, FxHashSet};
use swc_atoms::{
    atom,
    wtf8::{Wtf8, Wtf8Buf},
};
use swc_common::{util::take::Take, Mark, Span, Spanned, SyntaxContext, DUMMY_SP};
use swc_ecma_ast::*;
use swc_ecma_visit::{
    noop_visit_mut_type, noop_visit_type, visit_mut_obj_and_computed, visit_obj_and_computed,
    Visit, VisitMut, VisitMutWith, VisitWith,
};
use tracing::trace;

#[allow(deprecated)]
pub use self::{
    factory::{ExprFactory, FunctionFactory, IntoIndirectCall},
    value::{
        Merge,
        Type::{
            self, Bool as BoolType, Null as NullType, Num as NumberType, Obj as ObjectType,
            Str as StringType, Symbol as SymbolType, Undefined as UndefinedType,
        },
        Value::{self, Known, Unknown},
    },
    Purity::{MayBeImpure, Pure},
};
use crate::ident::IdentLike;

#[macro_use]
mod macros;
pub mod constructor;
mod factory;
pub mod function;
pub mod ident;
pub mod parallel;
mod value;
pub mod var;

pub mod unicode;

mod node_ignore_span;
pub mod number;
pub mod stack_size;
pub mod str;
pub use node_ignore_span::NodeIgnoringSpan;

// TODO: remove
pub struct ThisVisitor {
    found: bool,
}

pub(crate) static CPU_COUNT: Lazy<usize> = Lazy::new(num_cpus::get);
pub(crate) static LIGHT_TASK_PARALLELS: Lazy<usize> = Lazy::new(|| *CPU_COUNT * 100);

impl Visit for ThisVisitor {
    noop_visit_type!();

    /// Don't recurse into constructor
    fn visit_constructor(&mut self, _: &Constructor) {}

    /// Don't recurse into fn
    fn visit_fn_decl(&mut self, _: &FnDecl) {}

    /// Don't recurse into fn
    fn visit_fn_expr(&mut self, _: &FnExpr) {}

    /// Don't recurse into fn
    fn visit_function(&mut self, _: &Function) {}

    /// Don't recurse into fn
    fn visit_getter_prop(&mut self, n: &GetterProp) {
        n.key.visit_with(self);
    }

    /// Don't recurse into fn
    fn visit_method_prop(&mut self, n: &MethodProp) {
        n.key.visit_with(self);
        n.function.visit_with(self);
    }

    /// Don't recurse into fn
    fn visit_setter_prop(&mut self, n: &SetterProp) {
        n.key.visit_with(self);
        n.param.visit_with(self);
    }

    fn visit_this_expr(&mut self, _: &ThisExpr) {
        self.found = true;
    }
}

/// This does not recurse into a function if `this` is changed by it.
///
/// e.g.
///
///   - The body of an arrow expression is visited.
///   - The body of a function expression is **not** visited.
pub fn contains_this_expr<N>(body: &N) -> bool
where
    N: VisitWith<ThisVisitor>,
{
    let mut visitor = ThisVisitor { found: false };
    body.visit_with(&mut visitor);
    visitor.found
}

pub fn contains_ident_ref<'a, N>(body: &N, ident: &'a Ident) -> bool
where
    N: VisitWith<IdentRefFinder<'a>>,
{
    let mut visitor = IdentRefFinder {
        found: false,
        ident,
    };
    body.visit_with(&mut visitor);
    visitor.found
}

pub struct IdentRefFinder<'a> {
    ident: &'a Ident,
    found: bool,
}

impl Visit for IdentRefFinder<'_> {
    noop_visit_type!();

    fn visit_expr(&mut self, e: &Expr) {
        e.visit_children_with(self);

        match *e {
            Expr::Ident(ref i) if i.ctxt == self.ident.ctxt && i.sym == self.ident.sym => {
                self.found = true;
            }
            _ => {}
        }
    }
}

// TODO: remove
pub fn contains_arguments<N>(body: &N) -> bool
where
    N: VisitWith<ArgumentsFinder>,
{
    let mut visitor = ArgumentsFinder { found: false };
    body.visit_with(&mut visitor);
    visitor.found
}

pub struct ArgumentsFinder {
    found: bool,
}

impl Visit for ArgumentsFinder {
    noop_visit_type!();

    /// Don't recurse into constructor
    fn visit_constructor(&mut self, _: &Constructor) {}

    fn visit_expr(&mut self, e: &Expr) {
        e.visit_children_with(self);

        if e.is_ident_ref_to("arguments") {
            self.found = true;
        }
    }

    /// Don't recurse into fn
    fn visit_function(&mut self, _: &Function) {}

    fn visit_prop(&mut self, n: &Prop) {
        n.visit_children_with(self);

        if let Prop::Shorthand(i) = n {
            if &*i.sym == "arguments" {
                self.found = true;
            }
        }
    }
}

pub trait StmtOrModuleItem: Send + Sync + Sized {
    fn into_stmt(self) -> Result<Stmt, ModuleDecl>;

    fn as_stmt(&self) -> Result<&Stmt, &ModuleDecl>;

    fn as_stmt_mut(&mut self) -> Result<&mut Stmt, &mut ModuleDecl>;

    fn from_stmt(stmt: Stmt) -> Self;

    fn try_from_module_decl(decl: ModuleDecl) -> Result<Self, ModuleDecl>;
}

impl StmtOrModuleItem for Stmt {
    #[inline]
    fn into_stmt(self) -> Result<Stmt, ModuleDecl> {
        Ok(self)
    }

    #[inline]
    fn as_stmt(&self) -> Result<&Stmt, &ModuleDecl> {
        Ok(self)
    }

    #[inline]
    fn as_stmt_mut(&mut self) -> Result<&mut Stmt, &mut ModuleDecl> {
        Ok(self)
    }

    #[inline]
    fn from_stmt(stmt: Stmt) -> Self {
        stmt
    }

    #[inline]
    fn try_from_module_decl(decl: ModuleDecl) -> Result<Self, ModuleDecl> {
        Err(decl)
    }
}

impl StmtOrModuleItem for ModuleItem {
    #[inline]
    fn into_stmt(self) -> Result<Stmt, ModuleDecl> {
        match self {
            ModuleItem::ModuleDecl(v) => Err(v),
            ModuleItem::Stmt(v) => Ok(v),
            #[cfg(swc_ast_unknown)]
            _ => panic!("unable to access unknown nodes"),
        }
    }

    #[inline]
    fn as_stmt(&self) -> Result<&Stmt, &ModuleDecl> {
        match self {
            ModuleItem::ModuleDecl(v) => Err(v),
            ModuleItem::Stmt(v) => Ok(v),
            #[cfg(swc_ast_unknown)]
            _ => panic!("unable to access unknown nodes"),
        }
    }

    #[inline]
    fn as_stmt_mut(&mut self) -> Result<&mut Stmt, &mut ModuleDecl> {
        match self {
            ModuleItem::ModuleDecl(v) => Err(v),
            ModuleItem::Stmt(v) => Ok(v),
            #[cfg(swc_ast_unknown)]
            _ => panic!("unable to access unknown nodes"),
        }
    }

    #[inline]
    fn from_stmt(stmt: Stmt) -> Self {
        stmt.into()
    }

    #[inline]
    fn try_from_module_decl(decl: ModuleDecl) -> Result<Self, ModuleDecl> {
        Ok(decl.into())
    }
}

pub trait ModuleItemLike: StmtLike {
    fn try_into_module_decl(self) -> Result<ModuleDecl, Self> {
        Err(self)
    }
    fn try_from_module_decl(decl: ModuleDecl) -> Result<Self, ModuleDecl> {
        Err(decl)
    }
}

pub trait StmtLike: Sized + 'static + Send + Sync + From<Stmt> {
    fn try_into_stmt(self) -> Result<Stmt, Self>;
    fn as_stmt(&self) -> Option<&Stmt>;
    fn as_stmt_mut(&mut self) -> Option<&mut Stmt>;
}

impl ModuleItemLike for Stmt {}

impl StmtLike for Stmt {
    #[inline]
    fn try_into_stmt(self) -> Result<Stmt, Self> {
        Ok(self)
    }

    #[inline]
    fn as_stmt(&self) -> Option<&Stmt> {
        Some(self)
    }

    #[inline]
    fn as_stmt_mut(&mut self) -> Option<&mut Stmt> {
        Some(self)
    }
}

impl ModuleItemLike for ModuleItem {
    #[inline]
    fn try_into_module_decl(self) -> Result<ModuleDecl, Self> {
        match self {
            ModuleItem::ModuleDecl(decl) => Ok(decl),
            _ => Err(self),
        }
    }

    #[inline]
    fn try_from_module_decl(decl: ModuleDecl) -> Result<Self, ModuleDecl> {
        Ok(decl.into())
    }
}
impl StmtLike for ModuleItem {
    #[inline]
    fn try_into_stmt(self) -> Result<Stmt, Self> {
        match self {
            ModuleItem::Stmt(stmt) => Ok(stmt),
            _ => Err(self),
        }
    }

    #[inline]
    fn as_stmt(&self) -> Option<&Stmt> {
        match self {
            ModuleItem::Stmt(stmt) => Some(stmt),
            _ => None,
        }
    }

    #[inline]
    fn as_stmt_mut(&mut self) -> Option<&mut Stmt> {
        match &mut *self {
            ModuleItem::Stmt(stmt) => Some(stmt),
            _ => None,
        }
    }
}

/// Prepends statements after directive statements.
pub trait StmtLikeInjector<S>
where
    S: StmtLike,
{
    fn prepend_stmt(&mut self, insert_with: S);
    fn prepend_stmts<I>(&mut self, insert_with: I)
    where
        I: IntoIterator<Item = S>;
}

impl<S> StmtLikeInjector<S> for Vec<S>
where
    S: StmtLike,
{
    /// Note: If there is no directive, use `insert` instead.
    fn prepend_stmt(&mut self, insert_with: S) {
        let directive_pos = self
            .iter()
            .position(|stmt| !stmt.as_stmt().is_some_and(is_maybe_branch_directive))
            .unwrap_or(self.len());

        self.insert(directive_pos, insert_with);
    }

    /// Note: If there is no directive, use `splice` instead.
    fn prepend_stmts<I>(&mut self, insert_with: I)
    where
        I: IntoIterator<Item = S>,
    {
        let directive_pos = self
            .iter()
            .position(|stmt| !stmt.as_stmt().is_some_and(is_maybe_branch_directive))
            .unwrap_or(self.len());

        self.splice(directive_pos..directive_pos, insert_with);
    }
}

pub type BoolValue = Value<bool>;

pub trait IsEmpty {
    fn is_empty(&self) -> bool;
}

impl IsEmpty for BlockStmt {
    fn is_empty(&self) -> bool {
        self.stmts.is_empty()
    }
}
impl IsEmpty for CatchClause {
    fn is_empty(&self) -> bool {
        self.body.stmts.is_empty()
    }
}
impl IsEmpty for Stmt {
    fn is_empty(&self) -> bool {
        match *self {
            Stmt::Empty(_) => true,
            Stmt::Block(ref b) => b.is_empty(),
            _ => false,
        }
    }
}

impl<T: IsEmpty> IsEmpty for Option<T> {
    #[inline]
    fn is_empty(&self) -> bool {
        match *self {
            Some(ref node) => node.is_empty(),
            None => true,
        }
    }
}

impl<T: IsEmpty> IsEmpty for Box<T> {
    #[inline]
    fn is_empty(&self) -> bool {
        <T as IsEmpty>::is_empty(self)
    }
}

impl<T> IsEmpty for Vec<T> {
    #[inline]
    fn is_empty(&self) -> bool {
        self.is_empty()
    }
}

/// Extracts hoisted variables
pub fn extract_var_ids<T: VisitWith<Hoister>>(node: &T) -> Vec<Ident> {
    let mut v = Hoister { vars: Vec::new() };
    node.visit_with(&mut v);
    v.vars
}

pub trait StmtExt {
    fn as_stmt(&self) -> &Stmt;

    /// Extracts hoisted variables
    fn extract_var_ids(&self) -> Vec<Ident> {
        extract_var_ids(self.as_stmt())
    }

    fn extract_var_ids_as_var(&self) -> Option<VarDecl> {
        let ids = self.extract_var_ids();
        if ids.is_empty() {
            return None;
        }

        Some(VarDecl {
            kind: VarDeclKind::Var,
            decls: ids
                .into_iter()
                .map(|i| VarDeclarator {
                    span: i.span,
                    name: i.into(),
                    init: None,
                    definite: false,
                })
                .collect(),
            ..Default::default()
        })
    }

    /// stmts contain top level return/break/continue/throw
    fn terminates(&self) -> bool {
        fn terminates_many(
            stmts: &[Stmt],
            in_switch: bool,
            allow_break: bool,
            allow_throw: bool,
        ) -> Result<bool, ()> {
            stmts
                .iter()
                .rev()
                .map(|s| terminates(s, in_switch, allow_break, allow_throw))
                .try_fold(false, |acc, x| x.map(|v| acc || v))
        }

        fn terminates(
            stmt: &Stmt,
            in_switch: bool,
            allow_break: bool,
            allow_throw: bool,
        ) -> Result<bool, ()> {
            Ok(match stmt {
                Stmt::Break(_) => {
                    if in_switch {
                        // In case of `break` in switch, we should stop the analysis because the
                        // statements after `if (foo) break;` may not execute.
                        //
                        // So the `return 1` in
                        //
                        // ```js
                        // switch (foo) {
                        //   case 1:
                        //     if (bar) break;
                        //     return 1;
                        //   default:
                        //     return 0;
                        // }
                        // ```
                        //
                        // may not execute and we should return `false`.
                        return Err(());
                    } else {
                        allow_break
                    }
                }
                Stmt::Throw(_) => allow_throw,
                Stmt::Continue(_) | Stmt::Return(_) => true,
                Stmt::Block(block) => {
                    terminates_many(&block.stmts, in_switch, allow_break, allow_throw)?
                }
                Stmt::If(IfStmt { cons, alt, .. }) => {
                    if let Some(alt) = alt {
                        terminates(cons, in_switch, allow_break, allow_throw)?
                            && terminates(alt, in_switch, allow_break, allow_throw)?
                    } else {
                        terminates(cons, in_switch, allow_break, allow_throw)?;

                        false
                    }
                }
                Stmt::Switch(s) => {
                    let mut has_default = false;
                    let mut has_non_empty_terminates = false;

                    for case in &s.cases {
                        if case.test.is_none() {
                            has_default = true
                        }

                        if !case.cons.is_empty() {
                            let t = terminates_many(&case.cons, true, false, allow_throw)
                                .unwrap_or(false);

                            if t {
                                has_non_empty_terminates = true
                            } else {
                                return Ok(false);
                            }
                        }
                    }

                    has_default && has_non_empty_terminates
                }
                Stmt::Try(t) => {
                    if let Some(h) = &t.handler {
                        terminates_many(&t.block.stmts, in_switch, allow_break, false)?
                            && terminates_many(&h.body.stmts, in_switch, allow_break, allow_throw)?
                    } else {
                        terminates_many(&t.block.stmts, in_switch, allow_break, allow_throw)?
                    }
                }
                _ => false,
            })
        }

        terminates(self.as_stmt(), false, true, true) == Ok(true)
    }

    fn may_have_side_effects(&self, ctx: ExprCtx) -> bool {
        fn may_have_side_effects(stmt: &Stmt, ctx: ExprCtx) -> bool {
            match stmt {
                Stmt::Block(block_stmt) => block_stmt
                    .stmts
                    .iter()
                    .any(|stmt| stmt.may_have_side_effects(ctx)),
                Stmt::Empty(_) => false,
                Stmt::Labeled(labeled_stmt) => labeled_stmt.body.may_have_side_effects(ctx),
                Stmt::If(if_stmt) => {
                    if_stmt.test.may_have_side_effects(ctx)
                        || if_stmt.cons.may_have_side_effects(ctx)
                        || if_stmt
                            .alt
                            .as_ref()
                            .is_some_and(|stmt| stmt.may_have_side_effects(ctx))
                }
                Stmt::Switch(switch_stmt) => {
                    switch_stmt.discriminant.may_have_side_effects(ctx)
                        || switch_stmt.cases.iter().any(|case| {
                            case.test
                                .as_ref()
                                .is_some_and(|expr| expr.may_have_side_effects(ctx))
                                || case.cons.iter().any(|con| con.may_have_side_effects(ctx))
                        })
                }
                Stmt::Try(try_stmt) => {
                    try_stmt
                        .block
                        .stmts
                        .iter()
                        .any(|stmt| stmt.may_have_side_effects(ctx))
                        || try_stmt.handler.as_ref().is_some_and(|handler| {
                            handler
                                .body
                                .stmts
                                .iter()
                                .any(|stmt| stmt.may_have_side_effects(ctx))
                        })
                        || try_stmt.finalizer.as_ref().is_some_and(|finalizer| {
                            finalizer
                                .stmts
                                .iter()
                                .any(|stmt| stmt.may_have_side_effects(ctx))
                        })
                }
                Stmt::Decl(decl) => match decl {
                    Decl::Class(class_decl) => class_has_side_effect(ctx, &class_decl.class),
                    Decl::Fn(_) => !ctx.in_strict,
                    Decl::Var(var_decl) => var_decl.kind == VarDeclKind::Var,
                    _ => false,
                },
                Stmt::Expr(expr_stmt) => expr_stmt.expr.may_have_side_effects(ctx),
                _ => true,
            }
        }

        may_have_side_effects(self.as_stmt(), ctx)
    }
}

impl StmtExt for Stmt {
    fn as_stmt(&self) -> &Stmt {
        self
    }
}

impl StmtExt for Box<Stmt> {
    fn as_stmt(&self) -> &Stmt {
        self
    }
}

pub struct Hoister {
    vars: Vec<Ident>,
}

impl Visit for Hoister {
    noop_visit_type!();

    fn visit_arrow_expr(&mut self, _: &ArrowExpr) {}

    fn visit_assign_expr(&mut self, node: &AssignExpr) {
        node.right.visit_children_with(self);
    }

    fn visit_assign_pat_prop(&mut self, node: &AssignPatProp) {
        node.value.visit_with(self);

        self.vars.push(node.key.clone().into());
    }

    fn visit_constructor(&mut self, _: &Constructor) {}

    fn visit_fn_decl(&mut self, f: &FnDecl) {
        self.vars.push(f.ident.clone());
    }

    fn visit_function(&mut self, _: &Function) {}

    fn visit_getter_prop(&mut self, _: &GetterProp) {}

    fn visit_pat(&mut self, p: &Pat) {
        p.visit_children_with(self);

        if let Pat::Ident(ref i) = *p {
            self.vars.push(i.clone().into())
        }
    }

    fn visit_setter_prop(&mut self, _: &SetterProp) {}

    fn visit_var_decl(&mut self, v: &VarDecl) {
        if v.kind != VarDeclKind::Var {
            return;
        }

        v.visit_children_with(self)
    }
}

#[derive(Debug, Clone, Copy)]

pub struct ExprCtx {
    /// This [SyntaxContext] should be applied only to unresolved references.
    ///
    /// In other words, this should be applied to identifier references to
    /// global objects like `Object` or `Math`, and when those are not shadowed
    /// by a local declaration.
    pub unresolved_ctxt: SyntaxContext,

    /// True for argument of `typeof`.
    pub is_unresolved_ref_safe: bool,

    /// True if we are in the strict mode. This will be set to `true` for
    /// statements **after** `'use strict'`
    pub in_strict: bool,

    /// Remaining depth of the current expression. If this is 0, it means the
    /// function should not operate and return the safe value.
    ///
    /// Default value is `4`
    pub remaining_depth: u8,
}

/// Extension methods for [Expr].
pub trait ExprExt {
    fn as_expr(&self) -> &Expr;

    /// Returns true if this is an immutable value.
    #[inline(always)]
    fn is_immutable_value(&self) -> bool {
        is_immutable_value(self.as_expr())
    }

    #[inline(always)]
    fn is_number(&self) -> bool {
        is_number(self.as_expr())
    }

    // TODO: remove this after a proper evaluator
    #[inline(always)]
    fn is_str(&self) -> bool {
        is_str(self.as_expr())
    }

    #[inline(always)]
    fn is_array_lit(&self) -> bool {
        is_array_lit(self.as_expr())
    }

    /// Checks if `self` is `NaN`.
    #[inline(always)]
    fn is_nan(&self) -> bool {
        is_nan(self.as_expr())
    }

    #[inline(always)]
    fn is_undefined(&self, ctx: ExprCtx) -> bool {
        is_undefined(self.as_expr(), ctx)
    }

    #[inline(always)]
    fn is_void(&self) -> bool {
        is_void(self.as_expr())
    }

    /// Returns `true` if `id` references a global object.
    #[inline(always)]
    fn is_global_ref_to(&self, ctx: ExprCtx, id: &str) -> bool {
        is_global_ref_to(self.as_expr(), ctx, id)
    }

    /// Returns `true` if `id` references a global object.
    #[inline(always)]
    fn is_one_of_global_ref_to(&self, ctx: ExprCtx, ids: &[&str]) -> bool {
        is_one_of_global_ref_to(self.as_expr(), ctx, ids)
    }

    #[inline(always)]
    fn is_pure(&self, ctx: ExprCtx) -> bool {
        self.as_pure_bool(ctx).is_known()
    }

    /// Get bool value of `self` if it does not have any side effects.
    #[inline(always)]
    fn as_pure_bool(&self, ctx: ExprCtx) -> BoolValue {
        as_pure_bool(self.as_expr(), ctx)
    }

    ///
    /// This method emulates the `Boolean()` JavaScript cast function.
    ///Note: unlike getPureBooleanValue this function does not return `None`
    ///for expressions with side-effects.
    #[inline(always)]
    fn cast_to_bool(&self, ctx: ExprCtx) -> (Purity, BoolValue) {
        cast_to_bool(self.as_expr(), ctx)
    }

    #[inline(always)]
    fn cast_to_number(&self, ctx: ExprCtx) -> (Purity, Value<f64>) {
        cast_to_number(self.as_expr(), ctx)
    }

    /// Emulates javascript Number() cast function.
    ///
    /// Note: This method returns [Known] only if it's pure.
    #[inline(always)]
    fn as_pure_number(&self, ctx: ExprCtx) -> Value<f64> {
        as_pure_number(self.as_expr(), ctx)
    }

    /// Returns Known only if it's pure.
    #[inline(always)]
    fn as_pure_string(&self, ctx: ExprCtx) -> Value<Cow<'_, str>> {
        as_pure_string(self.as_expr(), ctx)
    }

    /// Returns Known only if it's pure.
    #[inline(always)]
    fn as_pure_wtf8(&self, ctx: ExprCtx) -> Value<Cow<'_, Wtf8>> {
        as_pure_wtf8(self.as_expr(), ctx)
    }

    /// Apply the supplied predicate against all possible result Nodes of the
    /// expression.
    #[inline(always)]
    fn get_type(&self, ctx: ExprCtx) -> Value<Type> {
        get_type(self.as_expr(), ctx)
    }

    #[inline(always)]
    fn is_pure_callee(&self, ctx: ExprCtx) -> bool {
        is_pure_callee(self.as_expr(), ctx)
    }

    #[inline(always)]
    fn may_have_side_effects(&self, ctx: ExprCtx) -> bool {
        may_have_side_effects(self.as_expr(), ctx)
    }
}

pub fn class_has_side_effect(expr_ctx: ExprCtx, c: &Class) -> bool {
    if let Some(e) = &c.super_class {
        if e.may_have_side_effects(expr_ctx) {
            return true;
        }
    }

    for m in &c.body {
        match m {
            ClassMember::Method(p) => {
                if let PropName::Computed(key) = &p.key {
                    if key.expr.may_have_side_effects(expr_ctx) {
                        return true;
                    }
                }
            }

            ClassMember::ClassProp(p) => {
                if let PropName::Computed(key) = &p.key {
                    if key.expr.may_have_side_effects(expr_ctx) {
                        return true;
                    }
                }

                if let Some(v) = &p.value {
                    if v.may_have_side_effects(expr_ctx) {
                        return true;
                    }
                }
            }
            ClassMember::PrivateProp(p) => {
                if let Some(v) = &p.value {
                    if v.may_have_side_effects(expr_ctx) {
                        return true;
                    }
                }
            }
            ClassMember::StaticBlock(s) => {
                if s.body
                    .stmts
                    .iter()
                    .any(|stmt| stmt.may_have_side_effects(expr_ctx))
                {
                    return true;
                }
            }
            _ => {}
        }
    }

    false
}

fn and(lt: Value<Type>, rt: Value<Type>) -> Value<Type> {
    if lt == rt {
        return lt;
    }
    Unknown
}

/// Return if the node is possibly a string.
fn may_be_str(ty: Value<Type>) -> bool {
    match ty {
        Known(BoolType) | Known(NullType) | Known(NumberType) | Known(UndefinedType) => false,
        Known(ObjectType) | Known(StringType) | Unknown => true,
        // TODO: Check if this is correct
        Known(SymbolType) => true,
    }
}

pub fn num_from_str(s: &str) -> Value<f64> {
    if s.contains('\u{000b}') {
        return Unknown;
    }

    let s = s.trim();

    if s.is_empty() {
        return Known(0.0);
    }

    if s.len() >= 2 {
        match &s.as_bytes()[..2] {
            b"0x" | b"0X" => {
                return match u64::from_str_radix(&s[2..], 16) {
                    Ok(n) => Known(n as f64),
                    Err(_) => Known(f64::NAN),
                }
            }
            b"0o" | b"0O" => {
                return match u64::from_str_radix(&s[2..], 8) {
                    Ok(n) => Known(n as f64),
                    Err(_) => Known(f64::NAN),
                };
            }
            b"0b" | b"0B" => {
                return match u64::from_str_radix(&s[2..], 2) {
                    Ok(n) => Known(n as f64),
                    Err(_) => Known(f64::NAN),
                };
            }
            _ => {}
        }
    }

    if (s.starts_with('-') || s.starts_with('+'))
        && (s[1..].starts_with("0x") || s[1..].starts_with("0X"))
    {
        // hex numbers with explicit signs vary between browsers.
        return Unknown;
    }

    // Firefox and IE treat the "Infinity" differently. Firefox is case
    // insensitive, but IE treats "infinity" as NaN.  So leave it alone.
    match s {
        "infinity" | "+infinity" | "-infinity" => return Unknown,
        _ => {}
    }

    Known(s.parse().ok().unwrap_or(f64::NAN))
}

impl ExprExt for Box<Expr> {
    #[inline(always)]
    fn as_expr(&self) -> &Expr {
        self
    }
}

impl ExprExt for Expr {
    #[inline(always)]
    fn as_expr(&self) -> &Expr {
        self
    }
}

#[derive(Debug, Clone, Copy, PartialEq, Eq, PartialOrd, Ord, Hash)]
pub enum Purity {
    /// May have some side effects.
    MayBeImpure,
    /// Does not have any side effect.
    Pure,
}
impl Purity {
    /// Returns true if it's pure.
    pub fn is_pure(self) -> bool {
        self == Pure
    }
}

impl Add for Purity {
    type Output = Self;

    fn add(self, rhs: Self) -> Self {
        match (self, rhs) {
            (Pure, Pure) => Pure,
            _ => MayBeImpure,
        }
    }
}

/// Cast to javascript's int32
pub fn to_int32(d: f64) -> i32 {
    let id = d as i32;
    if id as f64 == d {
        // This covers -0.0 as well
        return id;
    }

    if d.is_nan() || d.is_infinite() {
        return 0;
    }

    let d = if d >= 0.0 { d.floor() } else { d.ceil() };

    const TWO32: f64 = 4_294_967_296.0;
    let d = d % TWO32;
    // (double)(long)d == d should hold here

    let l = d as i64;
    // returning (int)d does not work as d can be outside int range
    // but the result must always be 32 lower bits of l
    l as i32
}

// pub fn to_u32(_d: f64) -> u32 {
//     //   if (Double.isNaN(d) || Double.isInfinite(d) || d == 0) {
//     //   return 0;
//     // }

//     // d = Math.signum(d) * Math.floor(Math.abs(d));

//     // double two32 = 4294967296.0;
//     // // this ensures that d is positive
//     // d = ((d % two32) + two32) % two32;
//     // // (double)(long)d == d should hold here

//     // long l = (long) d;
//     // // returning (int)d does not work as d can be outside int range
//     // // but the result must always be 32 lower bits of l
//     // return (int) l;
//     unimplemented!("to_u32")
// }

pub fn has_rest_pat<T: VisitWith<RestPatVisitor>>(node: &T) -> bool {
    let mut v = RestPatVisitor { found: false };
    node.visit_with(&mut v);
    v.found
}

pub struct RestPatVisitor {
    found: bool,
}

impl Visit for RestPatVisitor {
    noop_visit_type!();

    fn visit_rest_pat(&mut self, _: &RestPat) {
        self.found = true;
    }
}

pub fn is_literal<T>(node: &T) -> bool
where
    T: VisitWith<LiteralVisitor>,
{
    let (v, _) = calc_literal_cost(node, true);
    v
}

#[inline(never)]
pub fn calc_literal_cost<T>(e: &T, allow_non_json_value: bool) -> (bool, usize)
where
    T: VisitWith<LiteralVisitor>,
{
    let mut v = LiteralVisitor {
        is_lit: true,
        cost: 0,
        allow_non_json_value,
    };
    e.visit_with(&mut v);

    (v.is_lit, v.cost)
}

pub struct LiteralVisitor {
    is_lit: bool,
    cost: usize,
    allow_non_json_value: bool,
}

impl Visit for LiteralVisitor {
    noop_visit_type!();

    fn visit_array_lit(&mut self, e: &ArrayLit) {
        if !self.is_lit {
            return;
        }

        self.cost += 2 + e.elems.len();

        e.visit_children_with(self);

        for elem in &e.elems {
            if !self.allow_non_json_value && elem.is_none() {
                self.is_lit = false;
            }
        }
    }

    fn visit_arrow_expr(&mut self, _: &ArrowExpr) {
        self.is_lit = false
    }

    fn visit_assign_expr(&mut self, _: &AssignExpr) {
        self.is_lit = false;
    }

    fn visit_await_expr(&mut self, _: &AwaitExpr) {
        self.is_lit = false
    }

    fn visit_bin_expr(&mut self, _: &BinExpr) {
        self.is_lit = false
    }

    fn visit_call_expr(&mut self, _: &CallExpr) {
        self.is_lit = false
    }

    fn visit_class_expr(&mut self, _: &ClassExpr) {
        self.is_lit = false
    }

    fn visit_cond_expr(&mut self, _: &CondExpr) {
        self.is_lit = false;
    }

    fn visit_expr(&mut self, e: &Expr) {
        if !self.is_lit {
            return;
        }

        match *e {
            Expr::Ident(..) | Expr::Lit(Lit::Regex(..)) => self.is_lit = false,
            Expr::Tpl(ref tpl) if !tpl.exprs.is_empty() => self.is_lit = false,
            _ => e.visit_children_with(self),
        }
    }

    fn visit_fn_expr(&mut self, _: &FnExpr) {
        self.is_lit = false;
    }

    fn visit_invalid(&mut self, _: &Invalid) {
        self.is_lit = false;
    }

    fn visit_member_expr(&mut self, m: &MemberExpr) {
        if m.obj.is_ident_ref_to("Symbol") {
            return;
        }

        self.is_lit = false;
    }

    fn visit_meta_prop_expr(&mut self, _: &MetaPropExpr) {
        self.is_lit = false
    }

    fn visit_new_expr(&mut self, _: &NewExpr) {
        self.is_lit = false
    }

    fn visit_number(&mut self, node: &Number) {
        if !self.allow_non_json_value && node.value.is_infinite() {
            self.is_lit = false;
        }
    }

    fn visit_opt_chain_expr(&mut self, _: &OptChainExpr) {
        self.is_lit = false
    }

    fn visit_private_name(&mut self, _: &PrivateName) {
        self.is_lit = false
    }

    fn visit_prop(&mut self, p: &Prop) {
        if !self.is_lit {
            return;
        }

        p.visit_children_with(self);

        match p {
            Prop::KeyValue(..) => {
                self.cost += 1;
            }
            _ => self.is_lit = false,
        }
    }

    fn visit_prop_name(&mut self, node: &PropName) {
        if !self.is_lit {
            return;
        }

        node.visit_children_with(self);

        match node {
            PropName::Str(ref s) => self.cost += 2 + s.value.len(),
            PropName::Ident(ref id) => self.cost += 2 + id.sym.len(),
            PropName::Num(..) => {
                // TODO: Count digits
                self.cost += 5;
            }
            PropName::BigInt(_) => self.is_lit = false,
            PropName::Computed(..) => self.is_lit = false,
            #[cfg(swc_ast_unknown)]
            _ => (),
        }
    }

    fn visit_seq_expr(&mut self, _: &SeqExpr) {
        self.is_lit = false
    }

    fn visit_spread_element(&mut self, _: &SpreadElement) {
        self.is_lit = false;
    }

    fn visit_tagged_tpl(&mut self, _: &TaggedTpl) {
        self.is_lit = false
    }

    fn visit_this_expr(&mut self, _: &ThisExpr) {
        self.is_lit = false;
    }

    fn visit_ts_const_assertion(&mut self, _: &TsConstAssertion) {
        self.is_lit = false
    }

    fn visit_ts_non_null_expr(&mut self, _: &TsNonNullExpr) {
        self.is_lit = false
    }

    fn visit_unary_expr(&mut self, _: &UnaryExpr) {
        self.is_lit = false;
    }

    fn visit_update_expr(&mut self, _: &UpdateExpr) {
        self.is_lit = false;
    }

    fn visit_yield_expr(&mut self, _: &YieldExpr) {
        self.is_lit = false
    }
}

pub fn is_simple_pure_expr(expr: &Expr, pure_getters: bool) -> bool {
    match expr {
        Expr::Ident(..) | Expr::This(..) | Expr::Lit(..) => true,
        Expr::Unary(UnaryExpr {
            op: op!("void") | op!("!"),
            arg,
            ..
        }) => is_simple_pure_expr(arg, pure_getters),
        Expr::Member(m) if pure_getters => is_simple_pure_member_expr(m, pure_getters),
        _ => false,
    }
}

pub fn is_simple_pure_member_expr(m: &MemberExpr, pure_getters: bool) -> bool {
    match &m.prop {
        MemberProp::Ident(..) | MemberProp::PrivateName(..) => {
            is_simple_pure_expr(&m.obj, pure_getters)
        }
        MemberProp::Computed(c) => {
            is_simple_pure_expr(&c.expr, pure_getters) && is_simple_pure_expr(&m.obj, pure_getters)
        }
        #[cfg(swc_ast_unknown)]
        _ => false,
    }
}

fn sym_for_expr(expr: &Expr) -> Option<String> {
    match expr {
        Expr::Lit(Lit::Str(s)) => s.value.as_str().map(ToString::to_string),
        Expr::This(_) => Some("this".to_string()),

        Expr::Ident(ident)
        | Expr::Fn(FnExpr {
            ident: Some(ident), ..
        })
        | Expr::Class(ClassExpr {
            ident: Some(ident), ..
        }) => Some(ident.sym.to_string()),

        Expr::OptChain(OptChainExpr { base, .. }) => match &**base {
            OptChainBase::Call(OptCall { callee: expr, .. }) => sym_for_expr(expr),
            OptChainBase::Member(MemberExpr {
                prop: MemberProp::Ident(ident),
                obj,
                ..
            }) => Some(format!(
                "{}_{}",
                sym_for_expr(obj).unwrap_or_default(),
                ident.sym
            )),

            OptChainBase::Member(MemberExpr {
                prop: MemberProp::Computed(ComputedPropName { expr, .. }),
                obj,
                ..
            }) => Some(format!(
                "{}_{}",
                sym_for_expr(obj).unwrap_or_default(),
                sym_for_expr(expr).unwrap_or_default()
            )),
            _ => None,
        },
        Expr::Call(CallExpr {
            callee: Callee::Expr(expr),
            ..
        }) => sym_for_expr(expr),

        Expr::SuperProp(SuperPropExpr {
            prop: SuperProp::Ident(ident),
            ..
        }) => Some(format!("super_{}", ident.sym)),

        Expr::SuperProp(SuperPropExpr {
            prop: SuperProp::Computed(ComputedPropName { expr, .. }),
            ..
        }) => Some(format!("super_{}", sym_for_expr(expr).unwrap_or_default())),

        Expr::Member(MemberExpr {
            prop: MemberProp::Ident(ident),
            obj,
            ..
        }) => Some(format!(
            "{}_{}",
            sym_for_expr(obj).unwrap_or_default(),
            ident.sym
        )),

        Expr::Member(MemberExpr {
            prop: MemberProp::Computed(ComputedPropName { expr, .. }),
            obj,
            ..
        }) => Some(format!(
            "{}_{}",
            sym_for_expr(obj).unwrap_or_default(),
            sym_for_expr(expr).unwrap_or_default()
        )),

        _ => None,
    }
}

/// Used to determine super_class_ident
pub fn alias_ident_for(expr: &Expr, default: &str) -> Ident {
    let ctxt = SyntaxContext::empty().apply_mark(Mark::fresh(Mark::root()));
    let span = expr.span();

    let mut sym = sym_for_expr(expr).unwrap_or_else(|| default.to_string());

    if let Err(s) = Ident::verify_symbol(&sym) {
        sym = s;
    }

    if !sym.starts_with('_') {
        sym = format!("_{sym}")
    }
    quote_ident!(ctxt, span, sym)
}

/// Used to determine super_class_ident
pub fn alias_ident_for_simple_assign_tatget(expr: &SimpleAssignTarget, default: &str) -> Ident {
    let ctxt = SyntaxContext::empty().apply_mark(Mark::fresh(Mark::root()));

    let span = expr.span();

    let mut sym = match expr {
        SimpleAssignTarget::Ident(i) => Some(i.sym.to_string()),

        SimpleAssignTarget::SuperProp(SuperPropExpr {
            prop: SuperProp::Ident(ident),
            ..
        }) => Some(format!("super_{}", ident.sym)),

        SimpleAssignTarget::SuperProp(SuperPropExpr {
            prop: SuperProp::Computed(ComputedPropName { expr, .. }),
            ..
        }) => Some(format!("super_{}", sym_for_expr(expr).unwrap_or_default())),

        SimpleAssignTarget::Member(MemberExpr {
            prop: MemberProp::Ident(ident),
            obj,
            ..
        }) => Some(format!(
            "{}_{}",
            sym_for_expr(obj).unwrap_or_default(),
            ident.sym
        )),

        SimpleAssignTarget::Member(MemberExpr {
            prop: MemberProp::Computed(ComputedPropName { expr, .. }),
            obj,
            ..
        }) => Some(format!(
            "{}_{}",
            sym_for_expr(obj).unwrap_or_default(),
            sym_for_expr(expr).unwrap_or_default()
        )),
        _ => None,
    }
    .unwrap_or_else(|| default.to_string());

    if let Err(s) = Ident::verify_symbol(&sym) {
        sym = s;
    }

    if !sym.starts_with('_') {
        sym = format!("_{sym}")
    }
    quote_ident!(ctxt, span, sym)
}

/// Returns `(ident, aliased)`
pub fn alias_if_required(expr: &Expr, default: &str) -> (Ident, bool) {
    if let Expr::Ident(ref i) = *expr {
        return (Ident::new(i.sym.clone(), i.span, i.ctxt), false);
    }

    (alias_ident_for(expr, default), true)
}

pub fn prop_name_to_expr(p: PropName) -> Expr {
    match p {
        PropName::Ident(i) => i.into(),
        PropName::Str(s) => Lit::Str(s).into(),
        PropName::Num(n) => Lit::Num(n).into(),
        PropName::BigInt(b) => Lit::BigInt(b).into(),
        PropName::Computed(c) => *c.expr,
        #[cfg(swc_ast_unknown)]
        _ => panic!("unable to access unknown nodes"),
    }
}
/// Similar to `prop_name_to_expr`, but used for value position.
///
/// e.g. value from `{ key: value }`
pub fn prop_name_to_expr_value(p: PropName) -> Expr {
    match p {
        PropName::Ident(i) => Lit::Str(Str {
            span: i.span,
            raw: None,
            value: i.sym.into(),
        })
        .into(),
        PropName::Str(s) => Lit::Str(s).into(),
        PropName::Num(n) => Lit::Num(n).into(),
        PropName::BigInt(b) => Lit::BigInt(b).into(),
        PropName::Computed(c) => *c.expr,
        #[cfg(swc_ast_unknown)]
        _ => panic!("unable to access unknown nodes"),
    }
}

pub fn prop_name_to_member_prop(prop_name: PropName) -> MemberProp {
    match prop_name {
        PropName::Ident(i) => MemberProp::Ident(i),
        PropName::Str(s) => MemberProp::Computed(ComputedPropName {
            span: DUMMY_SP,
            expr: s.into(),
        }),
        PropName::Num(n) => MemberProp::Computed(ComputedPropName {
            span: DUMMY_SP,
            expr: n.into(),
        }),
        PropName::Computed(c) => MemberProp::Computed(c),
        PropName::BigInt(b) => MemberProp::Computed(ComputedPropName {
            span: DUMMY_SP,
            expr: b.into(),
        }),
        #[cfg(swc_ast_unknown)]
        _ => panic!("unable to access unknown nodes"),
    }
}

/// `super_call_span` should be the span of the class definition
/// Use value of [`Class::span`].
pub fn default_constructor_with_span(has_super: bool, super_call_span: Span) -> Constructor {
    trace!(has_super = has_super, "Creating a default constructor");
    let super_call_span = super_call_span.with_hi(super_call_span.lo);

    Constructor {
        span: DUMMY_SP,
        key: PropName::Ident(atom!("constructor").into()),
        is_optional: false,
        params: if has_super {
            vec![ParamOrTsParamProp::Param(Param {
                span: DUMMY_SP,
                decorators: Vec::new(),
                pat: Pat::Rest(RestPat {
                    span: DUMMY_SP,
                    dot3_token: DUMMY_SP,
                    arg: Box::new(Pat::Ident(quote_ident!("args").into())),
                    type_ann: Default::default(),
                }),
            })]
        } else {
            Vec::new()
        },
        body: Some(BlockStmt {
            stmts: if has_super {
                vec![CallExpr {
                    span: super_call_span,
                    callee: Callee::Super(Super { span: DUMMY_SP }),
                    args: vec![ExprOrSpread {
                        spread: Some(DUMMY_SP),
                        expr: Box::new(Expr::Ident(quote_ident!("args").into())),
                    }],
                    ..Default::default()
                }
                .into_stmt()]
            } else {
                Vec::new()
            },
            ..Default::default()
        }),
        ..Default::default()
    }
}

/// Check if `e` is `...arguments`
pub fn is_rest_arguments(e: &ExprOrSpread) -> bool {
    if e.spread.is_none() {
        return false;
    }

    e.expr.is_ident_ref_to("arguments")
}

pub fn opt_chain_test(
    left: Box<Expr>,
    right: Box<Expr>,
    span: Span,
    no_document_all: bool,
) -> Expr {
    if no_document_all {
        BinExpr {
            span,
            left,
            op: op!("=="),
            right: Lit::Null(Null { span: DUMMY_SP }).into(),
        }
        .into()
    } else {
        BinExpr {
            span,
            left: BinExpr {
                span: DUMMY_SP,
                left,
                op: op!("==="),
                right: Box::new(Expr::Lit(Lit::Null(Null { span: DUMMY_SP }))),
            }
            .into(),
            op: op!("||"),
            right: BinExpr {
                span: DUMMY_SP,
                left: right,
                op: op!("==="),
                right: Expr::undefined(DUMMY_SP),
            }
            .into(),
        }
        .into()
    }
}

/// inject `branch` after directives
#[inline]
pub fn prepend_stmt<T: StmtLike>(stmts: &mut Vec<T>, stmt: T) {
    stmts.prepend_stmt(stmt);
}

/// If the stmt is maybe a directive like `"use strict";`
pub fn is_maybe_branch_directive(stmt: &Stmt) -> bool {
    match stmt {
        Stmt::Expr(ExprStmt { ref expr, .. }) if matches!(&**expr, Expr::Lit(Lit::Str(..))) => true,
        _ => false,
    }
}

/// inject `stmts` after directives
#[inline]
pub fn prepend_stmts<T: StmtLike>(to: &mut Vec<T>, stmts: impl ExactSizeIterator<Item = T>) {
    to.prepend_stmts(stmts);
}

pub trait IsDirective {
    fn as_ref(&self) -> Option<&Stmt>;

    fn directive_continue(&self) -> bool {
        self.as_ref().is_some_and(Stmt::can_precede_directive)
    }
    fn is_use_strict(&self) -> bool {
        self.as_ref().is_some_and(Stmt::is_use_strict)
    }
}

impl IsDirective for Stmt {
    fn as_ref(&self) -> Option<&Stmt> {
        Some(self)
    }
}

impl IsDirective for ModuleItem {
    fn as_ref(&self) -> Option<&Stmt> {
        self.as_stmt()
    }
}

impl IsDirective for &ModuleItem {
    fn as_ref(&self) -> Option<&Stmt> {
        self.as_stmt()
    }
}

/// Finds all **binding** idents of variables.
pub struct DestructuringFinder<I: IdentLike> {
    pub found: Vec<I>,
}

/// Finds all **binding** idents of `node`.
///
/// If you want to avoid allocation, use [`for_each_binding_ident`] instead.
pub fn find_pat_ids<T, I: IdentLike>(node: &T) -> Vec<I>
where
    T: VisitWith<DestructuringFinder<I>>,
{
    let mut v = DestructuringFinder { found: Vec::new() };
    node.visit_with(&mut v);

    v.found
}

impl<I: IdentLike> Visit for DestructuringFinder<I> {
    /// No-op (we don't care about expressions)
    fn visit_expr(&mut self, _: &Expr) {}

    fn visit_ident(&mut self, i: &Ident) {
        self.found.push(I::from_ident(i));
    }

    fn visit_jsx_member_expr(&mut self, n: &JSXMemberExpr) {
        n.obj.visit_with(self);
    }

    /// No-op (we don't care about expressions)
    fn visit_prop_name(&mut self, _: &PropName) {}

    fn visit_ts_type(&mut self, _: &TsType) {}
}

/// Finds all **binding** idents of variables.
pub struct BindingIdentifierVisitor<F>
where
    F: for<'a> FnMut(&'a BindingIdent),
{
    op: F,
}

/// Finds all **binding** idents of `node`. **Any nested identifiers in
/// expressions are ignored**.
pub fn for_each_binding_ident<T, F>(node: &T, op: F)
where
    T: VisitWith<BindingIdentifierVisitor<F>>,
    F: for<'a> FnMut(&'a BindingIdent),
{
    let mut v = BindingIdentifierVisitor { op };
    node.visit_with(&mut v);
}

impl<F> Visit for BindingIdentifierVisitor<F>
where
    F: for<'a> FnMut(&'a BindingIdent),
{
    noop_visit_type!();

    /// No-op (we don't care about expressions)
    fn visit_expr(&mut self, _: &Expr) {}

    fn visit_binding_ident(&mut self, i: &BindingIdent) {
        (self.op)(i);
    }
}

pub fn is_valid_ident(s: &str) -> bool {
    if s.is_empty() {
        return false;
    }

    Ident::verify_symbol(s).is_ok()
}

pub fn is_valid_prop_ident(s: &str) -> bool {
    s.starts_with(Ident::is_valid_start) && s.chars().all(Ident::is_valid_continue)
}

pub fn drop_span<T>(mut t: T) -> T
where
    T: VisitMutWith<DropSpan>,
{
    t.visit_mut_with(&mut DropSpan {});
    t
}

pub struct DropSpan;

impl Pass for DropSpan {
    fn process(&mut self, program: &mut Program) {
        program.visit_mut_with(self);
    }
}

impl VisitMut for DropSpan {
    fn visit_mut_span(&mut self, span: &mut Span) {
        *span = DUMMY_SP;
    }
}

/// Finds usage of `ident`
pub struct IdentUsageFinder<'a> {
    ident: &'a Ident,
    found: bool,
}

impl Parallel for IdentUsageFinder<'_> {
    fn create(&self) -> Self {
        Self {
            ident: self.ident,
            found: self.found,
        }
    }

    fn merge(&mut self, other: Self) {
        self.found = self.found || other.found;
    }
}

impl Visit for IdentUsageFinder<'_> {
    noop_visit_type!();

    visit_obj_and_computed!();

    fn visit_ident(&mut self, i: &Ident) {
        if i.ctxt == self.ident.ctxt && i.sym == self.ident.sym {
            self.found = true;
        }
    }

    fn visit_class_members(&mut self, n: &[ClassMember]) {
        self.maybe_par(*LIGHT_TASK_PARALLELS, n, |v, item| {
            item.visit_with(v);
        });
    }

    fn visit_expr_or_spreads(&mut self, n: &[ExprOrSpread]) {
        self.maybe_par(*LIGHT_TASK_PARALLELS, n, |v, item| {
            item.visit_with(v);
        });
    }

    fn visit_exprs(&mut self, exprs: &[Box<Expr>]) {
        self.maybe_par(*LIGHT_TASK_PARALLELS, exprs, |v, expr| {
            expr.visit_with(v);
        });
    }

    fn visit_module_items(&mut self, n: &[ModuleItem]) {
        self.maybe_par(*LIGHT_TASK_PARALLELS, n, |v, item| {
            item.visit_with(v);
        });
    }

    fn visit_opt_vec_expr_or_spreads(&mut self, n: &[Option<ExprOrSpread>]) {
        self.maybe_par(*LIGHT_TASK_PARALLELS, n, |v, item| {
            if let Some(e) = item {
                e.visit_with(v);
            }
        });
    }

    fn visit_stmts(&mut self, stmts: &[Stmt]) {
        self.maybe_par(*LIGHT_TASK_PARALLELS, stmts, |v, stmt| {
            stmt.visit_with(v);
        });
    }

    fn visit_var_declarators(&mut self, n: &[VarDeclarator]) {
        self.maybe_par(*LIGHT_TASK_PARALLELS, n, |v, item| {
            item.visit_with(v);
        });
    }
}

impl<'a> IdentUsageFinder<'a> {
    pub fn find<N>(ident: &'a Ident, node: &N) -> bool
    where
        N: VisitWith<Self>,
    {
        let mut v = IdentUsageFinder {
            ident,
            found: false,
        };
        node.visit_with(&mut v);
        v.found
    }
}

impl ExprCtx {
    pub fn consume_depth(self) -> Option<Self> {
        if self.remaining_depth == 0 {
            return None;
        }

        Some(Self {
            remaining_depth: self.remaining_depth - 1,
            ..self
        })
    }

    /// make a new expression which evaluates `val` preserving side effects, if
    /// any.
    pub fn preserve_effects<I>(self, span: Span, val: Box<Expr>, exprs: I) -> Box<Expr>
    where
        I: IntoIterator<Item = Box<Expr>>,
    {
        let mut exprs = exprs.into_iter().fold(Vec::new(), |mut v, e| {
            self.extract_side_effects_to(&mut v, *e);
            v
        });

        if exprs.is_empty() {
            val
        } else {
            exprs.push(val);

            SeqExpr { exprs, span }.into()
        }
    }

    /// Add side effects of `expr` to `to`.
    //
    /// This function preserves order and conditions. (think a() ? yield b() :
    /// c())
    #[allow(clippy::vec_box)]
    pub fn extract_side_effects_to(self, to: &mut Vec<Box<Expr>>, expr: Expr) {
        match expr {
            Expr::Lit(..)
            | Expr::This(..)
            | Expr::Fn(..)
            | Expr::Arrow(..)
            | Expr::PrivateName(..) => {}

            Expr::Ident(..) => {
                if expr.may_have_side_effects(self) {
                    to.push(Box::new(expr));
                }
            }

            // In most case, we can do nothing for this.
            Expr::Update(_) | Expr::Assign(_) | Expr::Yield(_) | Expr::Await(_) => {
                to.push(Box::new(expr))
            }

            // TODO
            Expr::MetaProp(_) => to.push(Box::new(expr)),

            Expr::Call(_) => to.push(Box::new(expr)),
            Expr::New(e) => {
                // Known constructors
                if let Expr::Ident(Ident { ref sym, .. }) = *e.callee {
                    if *sym == "Date" && e.args.is_empty() {
                        return;
                    }
                }

                to.push(e.into())
            }
            Expr::Member(_) | Expr::SuperProp(_) => to.push(Box::new(expr)),

            // We are at here because we could not determine value of test.
            //TODO: Drop values if it does not have side effects.
            Expr::Cond(_) => to.push(Box::new(expr)),

            Expr::Unary(UnaryExpr {
                op: op!("typeof"),
                arg,
                ..
            }) => {
                // We should ignore side effect of `__dirname` in
                //
                // typeof __dirname != void 0
                //
                // https://github.com/swc-project/swc/pull/7763
                if arg.is_ident() {
                    return;
                }
                self.extract_side_effects_to(to, *arg)
            }

            Expr::Unary(UnaryExpr { arg, .. }) => self.extract_side_effects_to(to, *arg),

            Expr::Bin(BinExpr { op, .. }) if op.may_short_circuit() => {
                to.push(Box::new(expr));
            }
            Expr::Bin(BinExpr { left, right, .. }) => {
                self.extract_side_effects_to(to, *left);
                self.extract_side_effects_to(to, *right);
            }
            Expr::Seq(SeqExpr { exprs, .. }) => exprs
                .into_iter()
                .for_each(|e| self.extract_side_effects_to(to, *e)),

            Expr::Paren(e) => self.extract_side_effects_to(to, *e.expr),

            Expr::Object(ObjectLit {
                span, mut props, ..
            }) => {
                //
                let mut has_spread = false;
                props.retain(|node| match node {
                    PropOrSpread::Prop(node) => match &**node {
                        Prop::Shorthand(..) => false,
                        Prop::KeyValue(KeyValueProp { key, value }) => {
                            if let PropName::Computed(e) = key {
                                if e.expr.may_have_side_effects(self) {
                                    return true;
                                }
                            }

                            value.may_have_side_effects(self)
                        }
                        Prop::Getter(GetterProp { key, .. })
                        | Prop::Setter(SetterProp { key, .. })
                        | Prop::Method(MethodProp { key, .. }) => {
                            if let PropName::Computed(e) = key {
                                e.expr.may_have_side_effects(self)
                            } else {
                                false
                            }
                        }
                        Prop::Assign(..) => {
                            unreachable!("assign property in object literal is not a valid syntax")
                        }
                        #[cfg(swc_ast_unknown)]
                        _ => true,
                    },
                    PropOrSpread::Spread(SpreadElement { .. }) => {
                        has_spread = true;
                        true
                    }
                    #[cfg(swc_ast_unknown)]
                    _ => true,
                });

                if has_spread {
                    to.push(ObjectLit { span, props }.into())
                } else {
                    props.into_iter().for_each(|prop| match prop {
                        PropOrSpread::Prop(node) => match *node {
                            Prop::Shorthand(..) => {}
                            Prop::KeyValue(KeyValueProp { key, value }) => {
                                if let PropName::Computed(e) = key {
                                    self.extract_side_effects_to(to, *e.expr);
                                }

                                self.extract_side_effects_to(to, *value)
                            }
                            Prop::Getter(GetterProp { key, .. })
                            | Prop::Setter(SetterProp { key, .. })
                            | Prop::Method(MethodProp { key, .. }) => {
                                if let PropName::Computed(e) = key {
                                    self.extract_side_effects_to(to, *e.expr)
                                }
                            }
                            Prop::Assign(..) => {
                                unreachable!(
                                    "assign property in object literal is not a valid syntax"
                                )
                            }
                            #[cfg(swc_ast_unknown)]
                            _ => panic!("unable to access unknown nodes"),
                        },
                        _ => unreachable!(),
                    })
                }
            }

            Expr::Array(ArrayLit { elems, .. }) => {
                elems.into_iter().flatten().fold(to, |v, e| {
                    self.extract_side_effects_to(v, *e.expr);

                    v
                });
            }

            Expr::TaggedTpl(TaggedTpl { tag, tpl, .. }) => {
                self.extract_side_effects_to(to, *tag);

                tpl.exprs
                    .into_iter()
                    .for_each(|e| self.extract_side_effects_to(to, *e));
            }
            Expr::Tpl(Tpl { exprs, .. }) => {
                exprs
                    .into_iter()
                    .for_each(|e| self.extract_side_effects_to(to, *e));
            }
            Expr::Class(ClassExpr { .. }) => unimplemented!("add_effects for class expression"),

            Expr::JSXMember(..)
            | Expr::JSXNamespacedName(..)
            | Expr::JSXEmpty(..)
            | Expr::JSXElement(..)
            | Expr::JSXFragment(..) => to.push(Box::new(expr)),

            Expr::TsTypeAssertion(TsTypeAssertion { expr, .. })
            | Expr::TsNonNull(TsNonNullExpr { expr, .. })
            | Expr::TsAs(TsAsExpr { expr, .. })
            | Expr::TsConstAssertion(TsConstAssertion { expr, .. })
            | Expr::TsInstantiation(TsInstantiation { expr, .. })
            | Expr::TsSatisfies(TsSatisfiesExpr { expr, .. }) => {
                self.extract_side_effects_to(to, *expr)
            }
            Expr::OptChain(..) => to.push(Box::new(expr)),

            Expr::Invalid(..) => unreachable!(),
            #[cfg(swc_ast_unknown)]
            _ => to.push(Box::new(expr)),
        }
    }
}

pub fn prop_name_eq(p: &PropName, key: &str) -> bool {
    match p {
        PropName::Ident(i) => i.sym == *key,
        PropName::Str(s) => s.value == *key,
        PropName::Num(n) => n.value.to_string() == *key,
        PropName::BigInt(_) => false,
        PropName::Computed(e) => match &*e.expr {
            Expr::Lit(Lit::Str(Str { value, .. })) => *value == *key,
            _ => false,
        },
        #[cfg(swc_ast_unknown)]
        _ => false,
    }
}

/// Replace all `from` in `expr` with `to`.
///
/// # Usage
///
/// ```ignore
/// replace_ident(&mut dec.expr, cls_name.to_id(), alias);
/// ```
pub fn replace_ident<T>(node: &mut T, from: Id, to: &Ident)
where
    T: for<'any> VisitMutWith<IdentReplacer<'any>>,
{
    node.visit_mut_with(&mut IdentReplacer { from, to })
}

pub struct IdentReplacer<'a> {
    from: Id,
    to: &'a Ident,
}

impl VisitMut for IdentReplacer<'_> {
    noop_visit_mut_type!();

    visit_mut_obj_and_computed!();

    fn visit_mut_prop(&mut self, node: &mut Prop) {
        match node {
            Prop::Shorthand(i) => {
                let cloned = i.clone();
                i.visit_mut_with(self);
                if i.sym != cloned.sym || i.ctxt != cloned.ctxt {
                    *node = Prop::KeyValue(KeyValueProp {
                        key: PropName::Ident(IdentName::new(cloned.sym, cloned.span)),
                        value: i.clone().into(),
                    });
                }
            }
            _ => {
                node.visit_mut_children_with(self);
            }
        }
    }

    fn visit_mut_ident(&mut self, node: &mut Ident) {
        if node.sym == self.from.0 && node.ctxt == self.from.1 {
            *node = self.to.clone();
        }
    }
}

pub struct BindingCollector<I>
where
    I: IdentLike + Eq + Hash + Send + Sync,
{
    only: Option<SyntaxContext>,
    bindings: FxHashSet<I>,
    is_pat_decl: bool,
}

impl<I> BindingCollector<I>
where
    I: IdentLike + Eq + Hash + Send + Sync,
{
    fn add(&mut self, i: &Ident) {
        if let Some(only) = self.only {
            if only != i.ctxt {
                return;
            }
        }

        self.bindings.insert(I::from_ident(i));
    }
}

impl<I> Visit for BindingCollector<I>
where
    I: IdentLike + Eq + Hash + Send + Sync,
{
    noop_visit_type!();

    fn visit_arrow_expr(&mut self, n: &ArrowExpr) {
        let old = self.is_pat_decl;

        for p in &n.params {
            self.is_pat_decl = true;
            p.visit_with(self);
        }

        n.body.visit_with(self);
        self.is_pat_decl = old;
    }

    fn visit_assign_pat_prop(&mut self, node: &AssignPatProp) {
        node.value.visit_with(self);

        if self.is_pat_decl {
            self.add(&node.key.clone().into());
        }
    }

    fn visit_class_decl(&mut self, node: &ClassDecl) {
        node.visit_children_with(self);

        self.add(&node.ident);
    }

    fn visit_expr(&mut self, node: &Expr) {
        let old = self.is_pat_decl;
        self.is_pat_decl = false;
        node.visit_children_with(self);
        self.is_pat_decl = old;
    }

    fn visit_export_default_decl(&mut self, e: &ExportDefaultDecl) {
        match &e.decl {
            DefaultDecl::Class(ClassExpr {
                ident: Some(ident), ..
            }) => {
                self.add(ident);
            }
            DefaultDecl::Fn(FnExpr {
                ident: Some(ident),
                function: f,
            }) if f.body.is_some() => {
                self.add(ident);
            }
            _ => {}
        }
        e.visit_children_with(self);
    }

    fn visit_fn_decl(&mut self, node: &FnDecl) {
        node.visit_children_with(self);

        self.add(&node.ident);
    }

    fn visit_import_default_specifier(&mut self, node: &ImportDefaultSpecifier) {
        self.add(&node.local);
    }

    fn visit_import_named_specifier(&mut self, node: &ImportNamedSpecifier) {
        self.add(&node.local);
    }

    fn visit_import_star_as_specifier(&mut self, node: &ImportStarAsSpecifier) {
        self.add(&node.local);
    }

    fn visit_param(&mut self, node: &Param) {
        let old = self.is_pat_decl;
        self.is_pat_decl = true;
        node.visit_children_with(self);
        self.is_pat_decl = old;
    }

    fn visit_pat(&mut self, node: &Pat) {
        node.visit_children_with(self);

        if self.is_pat_decl {
            if let Pat::Ident(i) = node {
                self.add(&i.clone().into())
            }
        }
    }

    fn visit_var_declarator(&mut self, node: &VarDeclarator) {
        let old = self.is_pat_decl;
        self.is_pat_decl = true;
        node.name.visit_with(self);

        self.is_pat_decl = false;
        node.init.visit_with(self);
        self.is_pat_decl = old;
    }
}

/// Collects binding identifiers.
pub fn collect_decls<I, N>(n: &N) -> FxHashSet<I>
where
    I: IdentLike + Eq + Hash + Send + Sync,
    N: VisitWith<BindingCollector<I>>,
{
    let mut v = BindingCollector {
        only: None,
        bindings: Default::default(),
        is_pat_decl: false,
    };
    n.visit_with(&mut v);
    v.bindings
}

/// Collects binding identifiers, but only if it has a context which is
/// identical to `ctxt`.
pub fn collect_decls_with_ctxt<I, N>(n: &N, ctxt: SyntaxContext) -> FxHashSet<I>
where
    I: IdentLike + Eq + Hash + Send + Sync,
    N: VisitWith<BindingCollector<I>>,
{
    let mut v = BindingCollector {
        only: Some(ctxt),
        bindings: Default::default(),
        is_pat_decl: false,
    };
    n.visit_with(&mut v);
    v.bindings
}

pub struct TopLevelAwait {
    found: bool,
}

impl Visit for TopLevelAwait {
    noop_visit_type!();

    fn visit_stmt(&mut self, n: &Stmt) {
        if !self.found {
            n.visit_children_with(self);
        }
    }

    fn visit_param(&mut self, _: &Param) {}

    fn visit_function(&mut self, _: &Function) {}

    fn visit_arrow_expr(&mut self, _: &ArrowExpr) {}

    fn visit_class_member(&mut self, prop: &ClassMember) {
        match prop {
            ClassMember::ClassProp(ClassProp {
                key: PropName::Computed(computed),
                ..
            })
            | ClassMember::Method(ClassMethod {
                key: PropName::Computed(computed),
                ..
            }) => computed.visit_children_with(self),
            _ => (),
        };
    }

    fn visit_prop(&mut self, prop: &Prop) {
        match prop {
            Prop::KeyValue(KeyValueProp {
                key: PropName::Computed(computed),
                ..
            })
            | Prop::Getter(GetterProp {
                key: PropName::Computed(computed),
                ..
            })
            | Prop::Setter(SetterProp {
                key: PropName::Computed(computed),
                ..
            })
            | Prop::Method(MethodProp {
                key: PropName::Computed(computed),
                ..
            }) => computed.visit_children_with(self),
            _ => {}
        }
    }

    fn visit_for_of_stmt(&mut self, for_of_stmt: &ForOfStmt) {
        if for_of_stmt.is_await {
            self.found = true;
            return;
        }

        for_of_stmt.visit_children_with(self);
    }

    fn visit_await_expr(&mut self, _: &AwaitExpr) {
        self.found = true;
    }
}

pub fn contains_top_level_await<V: VisitWith<TopLevelAwait>>(t: &V) -> bool {
    let mut finder = TopLevelAwait { found: false };

    t.visit_with(&mut finder);

    finder.found
}

/// Variable remapper
///
/// This visitor modifies [SyntaxContext] while preserving the symbol of
/// [Ident]s.
pub struct Remapper<'a> {
    vars: &'a FxHashMap<Id, SyntaxContext>,
}

impl<'a> Remapper<'a> {
    pub fn new(vars: &'a FxHashMap<Id, SyntaxContext>) -> Self {
        Self { vars }
    }
}

impl VisitMut for Remapper<'_> {
    noop_visit_mut_type!(fail);

    fn visit_mut_ident(&mut self, i: &mut Ident) {
        if let Some(new_ctxt) = self.vars.get(&i.to_id()).copied() {
            i.ctxt = new_ctxt;
        }
    }
}

/// Replacer for [Id] => ]Id]
pub struct IdentRenamer<'a> {
    map: &'a FxHashMap<Id, Id>,
}

impl<'a> IdentRenamer<'a> {
    pub fn new(map: &'a FxHashMap<Id, Id>) -> Self {
        Self { map }
    }
}

impl VisitMut for IdentRenamer<'_> {
    noop_visit_mut_type!();

    visit_mut_obj_and_computed!();

    fn visit_mut_export_named_specifier(&mut self, node: &mut ExportNamedSpecifier) {
        if node.exported.is_some() {
            node.orig.visit_mut_children_with(self);
            return;
        }

        match &mut node.orig {
            ModuleExportName::Ident(orig) => {
                if let Some(new) = self.map.get(&orig.to_id()) {
                    node.exported = Some(ModuleExportName::Ident(orig.clone()));

                    orig.sym = new.0.clone();
                    orig.ctxt = new.1;
                }
            }
            ModuleExportName::Str(_) => {}
            #[cfg(swc_ast_unknown)]
            _ => {}
        }
    }

    fn visit_mut_ident(&mut self, node: &mut Ident) {
        if let Some(new) = self.map.get(&node.to_id()) {
            node.sym = new.0.clone();
            node.ctxt = new.1;
        }
    }

    fn visit_mut_object_pat_prop(&mut self, i: &mut ObjectPatProp) {
        match i {
            ObjectPatProp::Assign(p) => {
                p.value.visit_mut_with(self);

                let orig = p.key.clone();
                p.key.visit_mut_with(self);

                if orig.ctxt == p.key.ctxt && orig.sym == p.key.sym {
                    return;
                }

                match p.value.take() {
                    Some(default) => {
                        *i = ObjectPatProp::KeyValue(KeyValuePatProp {
                            key: PropName::Ident(orig.clone().into()),
                            value: AssignPat {
                                span: DUMMY_SP,
                                left: p.key.clone().into(),
                                right: default,
                            }
                            .into(),
                        });
                    }
                    None => {
                        *i = ObjectPatProp::KeyValue(KeyValuePatProp {
                            key: PropName::Ident(orig.clone().into()),
                            value: p.key.clone().into(),
                        });
                    }
                }
            }

            _ => {
                i.visit_mut_children_with(self);
            }
        }
    }

    fn visit_mut_prop(&mut self, node: &mut Prop) {
        match node {
            Prop::Shorthand(i) => {
                let cloned = i.clone();
                i.visit_mut_with(self);
                if i.sym != cloned.sym || i.ctxt != cloned.ctxt {
                    *node = Prop::KeyValue(KeyValueProp {
                        key: PropName::Ident(IdentName::new(cloned.sym, cloned.span)),
                        value: i.clone().into(),
                    });
                }
            }
            _ => {
                node.visit_mut_children_with(self);
            }
        }
    }
}

pub trait QueryRef {
    fn query_ref(&self, _ident: &Ident) -> Option<Box<Expr>> {
        None
    }
    fn query_lhs(&self, _ident: &Ident) -> Option<Box<Expr>> {
        None
    }

    /// ref used in JSX
    fn query_jsx(&self, _ident: &Ident) -> Option<JSXElementName> {
        None
    }

    /// when `foo()` is replaced with `bar.baz()`,
    /// should `bar.baz` be indirect call?
    fn should_fix_this(&self, _ident: &Ident) -> bool {
        false
    }
}

/// Replace `foo` with `bar` or `bar.baz`
pub struct RefRewriter<T>
where
    T: QueryRef,
{
    pub query: T,
}

impl<T> RefRewriter<T>
where
    T: QueryRef,
{
    pub fn exit_prop(&mut self, n: &mut Prop) {
        if let Prop::Shorthand(shorthand) = n {
            if let Some(expr) = self.query.query_ref(shorthand) {
                *n = KeyValueProp {
                    key: shorthand.take().into(),
                    value: expr,
                }
                .into()
            }
        }
    }

    pub fn exit_pat(&mut self, n: &mut Pat) {
        if let Pat::Ident(id) = n {
            if let Some(expr) = self.query.query_lhs(&id.clone().into()) {
                *n = expr.into();
            }
        }
    }

    pub fn exit_expr(&mut self, n: &mut Expr) {
        if let Expr::Ident(ref_ident) = n {
            if let Some(expr) = self.query.query_ref(ref_ident) {
                *n = *expr;
            }
        };
    }

    pub fn exit_simple_assign_target(&mut self, n: &mut SimpleAssignTarget) {
        if let SimpleAssignTarget::Ident(ref_ident) = n {
            if let Some(expr) = self.query.query_lhs(&ref_ident.clone().into()) {
                *n = expr.try_into().unwrap();
            }
        };
    }

    pub fn exit_jsx_element_name(&mut self, n: &mut JSXElementName) {
        if let JSXElementName::Ident(ident) = n {
            if let Some(expr) = self.query.query_jsx(ident) {
                *n = expr;
            }
        }
    }

    pub fn exit_jsx_object(&mut self, n: &mut JSXObject) {
        if let JSXObject::Ident(ident) = n {
            if let Some(expr) = self.query.query_jsx(ident) {
                *n = match expr {
                    JSXElementName::Ident(ident) => ident.into(),
                    JSXElementName::JSXMemberExpr(expr) => Box::new(expr).into(),
                    JSXElementName::JSXNamespacedName(..) => unimplemented!(),
                    #[cfg(swc_ast_unknown)]
                    _ => return,
                }
            }
        }
    }

    pub fn exit_object_pat_prop(&mut self, n: &mut ObjectPatProp) {
        if let ObjectPatProp::Assign(AssignPatProp { key, value, .. }) = n {
            if let Some(expr) = self.query.query_lhs(&key.id) {
                let value = value
                    .take()
                    .map(|default_value| {
                        let left = expr.clone().try_into().unwrap();
                        Box::new(default_value.make_assign_to(op!("="), left))
                    })
                    .unwrap_or(expr);

                *n = ObjectPatProp::KeyValue(KeyValuePatProp {
                    key: PropName::Ident(key.take().into()),
                    value: value.into(),
                });
            }
        }
    }
}

impl<T> VisitMut for RefRewriter<T>
where
    T: QueryRef,
{
    /// replace bar in binding pattern
    /// input:
    /// ```JavaScript
    /// const foo = { bar }
    /// ```
    /// output:
    /// ```JavaScript
    /// cobst foo = { bar: baz }
    /// ```
    fn visit_mut_prop(&mut self, n: &mut Prop) {
        n.visit_mut_children_with(self);
        self.exit_prop(n);
    }

    fn visit_mut_var_declarator(&mut self, n: &mut VarDeclarator) {
        if !n.name.is_ident() {
            n.name.visit_mut_with(self);
        }

        // skip var declarator name
        n.init.visit_mut_with(self);
    }

    fn visit_mut_pat(&mut self, n: &mut Pat) {
        n.visit_mut_children_with(self);
        self.exit_pat(n);
    }

    fn visit_mut_expr(&mut self, n: &mut Expr) {
        n.visit_mut_children_with(self);
        self.exit_expr(n);
    }

    fn visit_mut_simple_assign_target(&mut self, n: &mut SimpleAssignTarget) {
        n.visit_mut_children_with(self);
        self.exit_simple_assign_target(n);
    }

    fn visit_mut_callee(&mut self, n: &mut Callee) {
        match n {
            Callee::Expr(e)
                if e.as_ident()
                    .map(|ident| self.query.should_fix_this(ident))
                    .unwrap_or_default() =>
            {
                e.visit_mut_with(self);

                if e.is_member() {
                    *n = n.take().into_indirect()
                }
            }

            _ => n.visit_mut_children_with(self),
        }
    }

    fn visit_mut_tagged_tpl(&mut self, n: &mut TaggedTpl) {
        let should_fix_this = n
            .tag
            .as_ident()
            .map(|ident| self.query.should_fix_this(ident))
            .unwrap_or_default();

        n.visit_mut_children_with(self);

        if should_fix_this && n.tag.is_member() {
            *n = n.take().into_indirect()
        }
    }

    fn visit_mut_jsx_element_name(&mut self, n: &mut JSXElementName) {
        n.visit_mut_children_with(self);

        self.exit_jsx_element_name(n);
    }

    fn visit_mut_jsx_object(&mut self, n: &mut JSXObject) {
        n.visit_mut_children_with(self);

        self.exit_jsx_object(n);
    }
}

fn is_immutable_value(expr: &Expr) -> bool {
    // TODO(johnlenz): rename this function.  It is currently being used
    // in two disjoint cases:
    // 1) We only care about the result of the expression (in which case NOT here
    //    should return true)
    // 2) We care that expression is a side-effect free and can't be side-effected
    //    by other expressions.
    // This should only be used to say the value is immutable and
    // hasSideEffects and canBeSideEffected should be used for the other case.
    match *expr {
        Expr::Lit(Lit::Bool(..))
        | Expr::Lit(Lit::Str(..))
        | Expr::Lit(Lit::Num(..))
        | Expr::Lit(Lit::Null(..)) => true,

        Expr::Unary(UnaryExpr {
            op: op!("!"),
            ref arg,
            ..
        })
        | Expr::Unary(UnaryExpr {
            op: op!("~"),
            ref arg,
            ..
        })
        | Expr::Unary(UnaryExpr {
            op: op!("void"),
            ref arg,
            ..
        }) => arg.is_immutable_value(),

        Expr::Ident(ref i) => i.sym == "undefined" || i.sym == "Infinity" || i.sym == "NaN",

        Expr::Tpl(Tpl { ref exprs, .. }) => exprs.iter().all(|e| e.is_immutable_value()),

        _ => false,
    }
}

fn is_number(expr: &Expr) -> bool {
    matches!(*expr, Expr::Lit(Lit::Num(..)))
}

fn is_str(expr: &Expr) -> bool {
    match expr {
        Expr::Lit(Lit::Str(..)) | Expr::Tpl(_) => true,
        Expr::Unary(UnaryExpr {
            op: op!("typeof"), ..
        }) => true,
        Expr::Bin(BinExpr {
            op: op!(bin, "+"),
            left,
            right,
            ..
        }) => left.is_str() || right.is_str(),
        Expr::Assign(AssignExpr {
            op: op!("=") | op!("+="),
            right,
            ..
        }) => right.is_str(),
        Expr::Seq(s) => s.exprs.last().unwrap().is_str(),
        Expr::Cond(CondExpr { cons, alt, .. }) => cons.is_str() && alt.is_str(),
        _ => false,
    }
}

fn is_array_lit(expr: &Expr) -> bool {
    matches!(*expr, Expr::Array(..))
}

fn is_nan(expr: &Expr) -> bool {
    // NaN is special
    expr.is_ident_ref_to("NaN")
}

fn is_undefined(expr: &Expr, ctx: ExprCtx) -> bool {
    expr.is_global_ref_to(ctx, "undefined")
}

fn is_void(expr: &Expr) -> bool {
    matches!(
        *expr,
        Expr::Unary(UnaryExpr {
            op: op!("void"),
            ..
        })
    )
}

fn is_global_ref_to(expr: &Expr, ctx: ExprCtx, id: &str) -> bool {
    match expr {
        Expr::Ident(i) => i.ctxt == ctx.unresolved_ctxt && &*i.sym == id,
        _ => false,
    }
}

fn is_one_of_global_ref_to(expr: &Expr, ctx: ExprCtx, ids: &[&str]) -> bool {
    match expr {
        Expr::Ident(i) => i.ctxt == ctx.unresolved_ctxt && ids.contains(&&*i.sym),
        _ => false,
    }
}

fn as_pure_bool(expr: &Expr, ctx: ExprCtx) -> BoolValue {
    match expr.cast_to_bool(ctx) {
        (Pure, Known(b)) => Known(b),
        _ => Unknown,
    }
}

fn cast_to_bool(expr: &Expr, ctx: ExprCtx) -> (Purity, BoolValue) {
    let Some(ctx) = ctx.consume_depth() else {
        return (MayBeImpure, Unknown);
    };

    if expr.is_global_ref_to(ctx, "undefined") {
        return (Pure, Known(false));
    }
    if expr.is_nan() {
        return (Pure, Known(false));
    }

    let val = match expr {
        Expr::Paren(ref e) => return e.expr.cast_to_bool(ctx),

        Expr::Assign(AssignExpr {
            ref right,
            op: op!("="),
            ..
        }) => {
            let (_, v) = right.cast_to_bool(ctx);
            return (MayBeImpure, v);
        }

        Expr::Unary(UnaryExpr {
            op: op!(unary, "-"),
            arg,
            ..
        }) => {
            let v = arg.as_pure_number(ctx);
            match v {
                Known(n) => Known(!matches!(n.classify(), FpCategory::Nan | FpCategory::Zero)),
                Unknown => return (MayBeImpure, Unknown),
            }
        }

        Expr::Unary(UnaryExpr {
            op: op!("!"),
            ref arg,
            ..
        }) => {
            let (p, v) = arg.cast_to_bool(ctx);
            return (p, !v);
        }
        Expr::Seq(SeqExpr { exprs, .. }) => exprs.last().unwrap().cast_to_bool(ctx).1,

        Expr::Bin(BinExpr {
            left,
            op: op!(bin, "-"),
            right,
            ..
        }) => {
            let (lp, ln) = left.cast_to_number(ctx);
            let (rp, rn) = right.cast_to_number(ctx);

            return (
                lp + rp,
                match (ln, rn) {
                    (Known(ln), Known(rn)) => {
                        if ln == rn {
                            Known(false)
                        } else {
                            Known(true)
                        }
                    }
                    _ => Unknown,
                },
            );
        }

        Expr::Bin(BinExpr {
            left,
            op: op!("/"),
            right,
            ..
        }) => {
            let lv = left.as_pure_number(ctx);
            let rv = right.as_pure_number(ctx);

            match (lv, rv) {
                (Known(lv), Known(rv)) => {
                    // NaN is false
                    if lv == 0.0 && rv == 0.0 {
                        return (Pure, Known(false));
                    }
                    // Infinity is true.
                    if rv == 0.0 {
                        return (Pure, Known(true));
                    }
                    let v = lv / rv;

                    return (Pure, Known(v != 0.0));
                }
                _ => Unknown,
            }
        }

        Expr::Bin(BinExpr {
            ref left,
            op: op @ op!("&"),
            ref right,
            ..
        })
        | Expr::Bin(BinExpr {
            ref left,
            op: op @ op!("|"),
            ref right,
            ..
        }) => {
            if left.get_type(ctx) != Known(BoolType) || right.get_type(ctx) != Known(BoolType) {
                return (MayBeImpure, Unknown);
            }

            // TODO: Ignore purity if value cannot be reached.

            let (lp, lv) = left.cast_to_bool(ctx);
            let (rp, rv) = right.cast_to_bool(ctx);

            let v = if *op == op!("&") {
                lv.and(rv)
            } else {
                lv.or(rv)
            };

            if lp + rp == Pure {
                return (Pure, v);
            }

            v
        }

        Expr::Bin(BinExpr {
            ref left,
            op: op!("||"),
            ref right,
            ..
        }) => {
            let (lp, lv) = left.cast_to_bool(ctx);
            if let Known(true) = lv {
                return (lp, lv);
            }

            let (rp, rv) = right.cast_to_bool(ctx);
            if let Known(true) = rv {
                return (lp + rp, rv);
            }

            Unknown
        }

        Expr::Bin(BinExpr {
            ref left,
            op: op!("&&"),
            ref right,
            ..
        }) => {
            let (lp, lv) = left.cast_to_bool(ctx);
            if let Known(false) = lv {
                return (lp, lv);
            }

            let (rp, rv) = right.cast_to_bool(ctx);
            if let Known(false) = rv {
                return (lp + rp, rv);
            }

            Unknown
        }

        Expr::Bin(BinExpr {
            left,
            op: op!(bin, "+"),
            right,
            ..
        }) => {
            match &**left {
                Expr::Lit(Lit::Str(s)) if !s.value.is_empty() => return (MayBeImpure, Known(true)),
                _ => {}
            }

            match &**right {
                Expr::Lit(Lit::Str(s)) if !s.value.is_empty() => return (MayBeImpure, Known(true)),
                _ => {}
            }

            Unknown
        }

        Expr::Fn(..) | Expr::Class(..) | Expr::New(..) | Expr::Array(..) | Expr::Object(..) => {
            Known(true)
        }

        Expr::Unary(UnaryExpr {
            op: op!("void"), ..
        }) => Known(false),

        Expr::Lit(ref lit) => {
            return (
                Pure,
                Known(match *lit {
                    Lit::Num(Number { value: n, .. }) => {
                        !matches!(n.classify(), FpCategory::Nan | FpCategory::Zero)
                    }
                    Lit::BigInt(ref v) => v
                        .value
                        .to_string()
                        .contains(|c: char| matches!(c, '1'..='9')),
                    Lit::Bool(b) => b.value,
                    Lit::Str(Str { ref value, .. }) => !value.is_empty(),
                    Lit::Null(..) => false,
                    Lit::Regex(..) => true,
                    Lit::JSXText(..) => unreachable!("as_bool() for JSXText"),
                    #[cfg(swc_ast_unknown)]
                    _ => return (Pure, Unknown),
                }),
            );
        }

        //TODO?
        _ => Unknown,
    };

    if expr.may_have_side_effects(ctx) {
        (MayBeImpure, val)
    } else {
        (Pure, val)
    }
}

fn cast_to_number(expr: &Expr, ctx: ExprCtx) -> (Purity, Value<f64>) {
    let Some(ctx) = ctx.consume_depth() else {
        return (MayBeImpure, Unknown);
    };

    let v = match expr {
        Expr::Lit(l) => match l {
            Lit::Bool(Bool { value: true, .. }) => 1.0,
            Lit::Bool(Bool { value: false, .. }) | Lit::Null(..) => 0.0,
            Lit::Num(Number { value: n, .. }) => *n,
            Lit::Str(Str { value, .. }) => {
                if let Some(value) = value.as_str() {
                    return (Pure, num_from_str(value));
                }
                return (Pure, Unknown);
            }
            _ => return (Pure, Unknown),
        },
        Expr::Array(..) => {
            let Known(s) = expr.as_pure_string(ctx) else {
                return (Pure, Unknown);
            };

            return (Pure, num_from_str(&s));
        }
        Expr::Ident(Ident { sym, ctxt, .. }) => match &**sym {
            "undefined" | "NaN" if *ctxt == ctx.unresolved_ctxt => f64::NAN,
            "Infinity" if *ctxt == ctx.unresolved_ctxt => f64::INFINITY,
            _ => return (Pure, Unknown),
        },
        Expr::Unary(UnaryExpr {
            op: op!(unary, "-"),
            arg,
            ..
        }) => match arg.cast_to_number(ctx) {
            (Pure, Known(v)) => -v,
            _ => return (MayBeImpure, Unknown),
        },
        Expr::Unary(UnaryExpr {
            op: op!("!"),
            ref arg,
            ..
        }) => match arg.cast_to_bool(ctx) {
            (Pure, Known(v)) => {
                if v {
                    0.0
                } else {
                    1.0
                }
            }
            _ => return (MayBeImpure, Unknown),
        },
        Expr::Unary(UnaryExpr {
            op: op!("void"),
            ref arg,
            ..
        }) => {
            if arg.may_have_side_effects(ctx) {
                return (MayBeImpure, Known(f64::NAN));
            } else {
                f64::NAN
            }
        }

        Expr::Tpl(..) => {
            return (
                Pure,
                num_from_str(&match expr.as_pure_string(ctx) {
                    Known(v) => v,
                    Unknown => return (MayBeImpure, Unknown),
                }),
            );
        }

        Expr::Seq(seq) => {
            if let Some(last) = seq.exprs.last() {
                let (_, v) = last.cast_to_number(ctx);

                // TODO: Purity
                return (MayBeImpure, v);
            }

            return (MayBeImpure, Unknown);
        }

        _ => return (MayBeImpure, Unknown),
    };

    (Purity::Pure, Known(v))
}

fn as_pure_number(expr: &Expr, ctx: ExprCtx) -> Value<f64> {
    let (purity, v) = expr.cast_to_number(ctx);
    if !purity.is_pure() {
        return Unknown;
    }

    v
}

fn as_pure_string(expr: &Expr, ctx: ExprCtx) -> Value<Cow<'_, str>> {
    match as_pure_wtf8(expr, ctx) {
        Known(v) => match v {
            Cow::Borrowed(v) => {
                if let Some(v) = v.as_str() {
                    Known(Cow::Borrowed(v))
                } else {
                    Unknown
                }
            }
            Cow::Owned(v) => {
                if let Ok(v) = v.into_string() {
                    Known(Cow::Owned(v))
                } else {
                    Unknown
                }
            }
        },
        Unknown => Unknown,
    }
}

fn as_pure_wtf8(expr: &Expr, ctx: ExprCtx) -> Value<Cow<'_, Wtf8>> {
    let Some(ctx) = ctx.consume_depth() else {
        return Unknown;
    };

    match *expr {
        Expr::Lit(ref l) => match *l {
            Lit::Str(Str { ref value, .. }) => Known(Cow::Borrowed(&**value)),
            Lit::Num(ref n) => {
                if n.value == -0.0 {
                    return Known(Cow::Borrowed("0".into()));
                }

                Known(Cow::Owned(Wtf8Buf::from_string(n.value.to_js_string())))
            }
            Lit::Bool(Bool { value: true, .. }) => Known(Cow::Borrowed("true".into())),
            Lit::Bool(Bool { value: false, .. }) => Known(Cow::Borrowed("false".into())),
            Lit::Null(..) => Known(Cow::Borrowed("null".into())),
            _ => Unknown,
        },
        Expr::Tpl(_) => {
            Value::Unknown
            // TODO:
            // Only convert a template literal if all its expressions
            // can be converted.
            // unimplemented!("TplLit. as_string()")
        }
        Expr::Ident(Ident { ref sym, ctxt, .. }) => match &**sym {
            "undefined" | "Infinity" | "NaN" if ctxt == ctx.unresolved_ctxt => {
                Known(Cow::Borrowed(Wtf8::from_str(sym)))
            }
            _ => Unknown,
        },
        Expr::Unary(UnaryExpr {
            op: op!("void"), ..
        }) => Known(Cow::Borrowed("undefined".into())),
        Expr::Unary(UnaryExpr {
            op: op!("!"),
            ref arg,
            ..
        }) => Known(Cow::Borrowed(match arg.as_pure_bool(ctx) {
            Known(v) => {
                if v {
                    "false".into()
                } else {
                    "true".into()
                }
            }
            Unknown => return Value::Unknown,
        })),
        Expr::Array(ArrayLit { ref elems, .. }) => {
            let mut buf = Wtf8Buf::new();
            let len = elems.len();
            // null, undefined is "" in array literal.
            for (idx, elem) in elems.iter().enumerate() {
                let last = idx == len - 1;
                let e = match *elem {
                    Some(ref elem) => {
                        let ExprOrSpread { ref expr, .. } = *elem;
                        match &**expr {
                            Expr::Lit(Lit::Null(..)) => Cow::Borrowed("".into()),
                            Expr::Unary(UnaryExpr {
                                op: op!("void"),
                                arg,
                                ..
                            }) => {
                                if arg.may_have_side_effects(ctx) {
                                    return Value::Unknown;
                                }
                                Cow::Borrowed("".into())
                            }
                            Expr::Ident(Ident { sym: undefined, .. })
                                if &**undefined == "undefined" =>
                            {
                                Cow::Borrowed("".into())
                            }
                            _ => match expr.as_pure_wtf8(ctx) {
                                Known(v) => v,
                                Unknown => return Value::Unknown,
                            },
                        }
                    }
                    None => Cow::Borrowed("".into()),
                };
                buf.push_wtf8(&e);

                if !last {
                    buf.push_char(',');
                }
            }
            Known(buf.into())
        }
        _ => Unknown,
    }
}

fn get_type(expr: &Expr, ctx: ExprCtx) -> Value<Type> {
    let Some(ctx) = ctx.consume_depth() else {
        return Unknown;
    };

    match expr {
        Expr::Assign(AssignExpr {
            ref right,
            op: op!("="),
            ..
        }) => right.get_type(ctx),

        Expr::Member(MemberExpr {
            obj,
            prop: MemberProp::Ident(IdentName { sym: length, .. }),
            ..
        }) if &**length == "length" => match &**obj {
            Expr::Array(ArrayLit { .. }) | Expr::Lit(Lit::Str(..)) => Known(Type::Num),
            Expr::Ident(Ident { sym: arguments, .. }) if &**arguments == "arguments" => {
                Known(Type::Num)
            }
            _ => Unknown,
        },

        Expr::Seq(SeqExpr { ref exprs, .. }) => exprs
            .last()
            .expect("sequence expression should not be empty")
            .get_type(ctx),

        Expr::Bin(BinExpr {
            ref left,
            op: op!("&&"),
            ref right,
            ..
        })
        | Expr::Bin(BinExpr {
            ref left,
            op: op!("||"),
            ref right,
            ..
        })
        | Expr::Cond(CondExpr {
            cons: ref left,
            alt: ref right,
            ..
        }) => and(left.get_type(ctx), right.get_type(ctx)),

        Expr::Bin(BinExpr {
            ref left,
            op: op!(bin, "+"),
            ref right,
            ..
        }) => {
            let rt = right.get_type(ctx);
            if rt == Known(StringType) {
                return Known(StringType);
            }

            let lt = left.get_type(ctx);
            if lt == Known(StringType) {
                return Known(StringType);
            }

            // There are some pretty weird cases for object types:
            //   {} + [] === "0"
            //   [] + {} ==== "[object Object]"
            if lt == Known(ObjectType) || rt == Known(ObjectType) {
                return Unknown;
            }

            if !may_be_str(lt) && !may_be_str(rt) {
                // ADD used with compilations of null, boolean and number always
                // result in numbers.
                return Known(NumberType);
            }

            // There are some pretty weird cases for object types:
            //   {} + [] === "0"
            //   [] + {} ==== "[object Object]"
            Unknown
        }

        Expr::Assign(AssignExpr {
            op: op!("+="),
            ref right,
            ..
        }) => {
            if right.get_type(ctx) == Known(StringType) {
                return Known(StringType);
            }
            Unknown
        }

        Expr::Ident(Ident { ref sym, .. }) => Known(match &**sym {
            "undefined" => UndefinedType,
            "NaN" | "Infinity" => NumberType,
            _ => return Unknown,
        }),

        Expr::Lit(Lit::Num(..))
        | Expr::Assign(AssignExpr { op: op!("&="), .. })
        | Expr::Assign(AssignExpr { op: op!("^="), .. })
        | Expr::Assign(AssignExpr { op: op!("|="), .. })
        | Expr::Assign(AssignExpr { op: op!("<<="), .. })
        | Expr::Assign(AssignExpr { op: op!(">>="), .. })
        | Expr::Assign(AssignExpr {
            op: op!(">>>="), ..
        })
        | Expr::Assign(AssignExpr { op: op!("-="), .. })
        | Expr::Assign(AssignExpr { op: op!("*="), .. })
        | Expr::Assign(AssignExpr { op: op!("**="), .. })
        | Expr::Assign(AssignExpr { op: op!("/="), .. })
        | Expr::Assign(AssignExpr { op: op!("%="), .. })
        | Expr::Unary(UnaryExpr { op: op!("~"), .. })
        | Expr::Bin(BinExpr { op: op!("|"), .. })
        | Expr::Bin(BinExpr { op: op!("^"), .. })
        | Expr::Bin(BinExpr { op: op!("&"), .. })
        | Expr::Bin(BinExpr { op: op!("<<"), .. })
        | Expr::Bin(BinExpr { op: op!(">>"), .. })
        | Expr::Bin(BinExpr { op: op!(">>>"), .. })
        | Expr::Bin(BinExpr {
            op: op!(bin, "-"), ..
        })
        | Expr::Bin(BinExpr { op: op!("*"), .. })
        | Expr::Bin(BinExpr { op: op!("%"), .. })
        | Expr::Bin(BinExpr { op: op!("/"), .. })
        | Expr::Bin(BinExpr { op: op!("**"), .. })
        | Expr::Update(UpdateExpr { op: op!("++"), .. })
        | Expr::Update(UpdateExpr { op: op!("--"), .. })
        | Expr::Unary(UnaryExpr {
            op: op!(unary, "+"),
            ..
        })
        | Expr::Unary(UnaryExpr {
            op: op!(unary, "-"),
            ..
        }) => Known(NumberType),

        // Primitives
        Expr::Lit(Lit::Bool(..))
        | Expr::Bin(BinExpr { op: op!("=="), .. })
        | Expr::Bin(BinExpr { op: op!("!="), .. })
        | Expr::Bin(BinExpr { op: op!("==="), .. })
        | Expr::Bin(BinExpr { op: op!("!=="), .. })
        | Expr::Bin(BinExpr { op: op!("<"), .. })
        | Expr::Bin(BinExpr { op: op!("<="), .. })
        | Expr::Bin(BinExpr { op: op!(">"), .. })
        | Expr::Bin(BinExpr { op: op!(">="), .. })
        | Expr::Bin(BinExpr { op: op!("in"), .. })
        | Expr::Bin(BinExpr {
            op: op!("instanceof"),
            ..
        })
        | Expr::Unary(UnaryExpr { op: op!("!"), .. })
        | Expr::Unary(UnaryExpr {
            op: op!("delete"), ..
        }) => Known(BoolType),

        Expr::Unary(UnaryExpr {
            op: op!("typeof"), ..
        })
        | Expr::Lit(Lit::Str { .. })
        | Expr::Tpl(..) => Known(StringType),

        Expr::Lit(Lit::Null(..)) => Known(NullType),

        Expr::Unary(UnaryExpr {
            op: op!("void"), ..
        }) => Known(UndefinedType),

        Expr::Fn(..)
        | Expr::New(NewExpr { .. })
        | Expr::Array(ArrayLit { .. })
        | Expr::Object(ObjectLit { .. })
        | Expr::Lit(Lit::Regex(..)) => Known(ObjectType),

        _ => Unknown,
    }
}

fn is_pure_callee(expr: &Expr, ctx: ExprCtx) -> bool {
    if expr.is_global_ref_to(ctx, "Date") {
        return true;
    }

    match expr {
        Expr::Member(MemberExpr {
            obj,
            prop: MemberProp::Ident(prop),
            ..
        }) => {
            // Some methods of string are pure
            fn is_pure_str_method(method: &str) -> bool {
                matches!(
                    method,
                    "charAt"
                        | "charCodeAt"
                        | "concat"
                        | "endsWith"
                        | "includes"
                        | "indexOf"
                        | "lastIndexOf"
                        | "localeCompare"
                        | "slice"
                        | "split"
                        | "startsWith"
                        | "substr"
                        | "substring"
                        | "toLocaleLowerCase"
                        | "toLocaleUpperCase"
                        | "toLowerCase"
                        | "toString"
                        | "toUpperCase"
                        | "trim"
                        | "trimEnd"
                        | "trimStart"
                )
            }

            obj.is_global_ref_to(ctx, "Math")
                || match &**obj {
                    // Allow dummy span
                    Expr::Ident(Ident {
                        ctxt, sym: math, ..
                    }) => &**math == "Math" && *ctxt == SyntaxContext::empty(),

                    Expr::Lit(Lit::Str(..)) => is_pure_str_method(&prop.sym),
                    Expr::Tpl(Tpl { exprs, .. }) if exprs.is_empty() => {
                        is_pure_str_method(&prop.sym)
                    }

                    _ => false,
                }
        }

        Expr::Fn(FnExpr { function: f, .. })
            if f.params.iter().all(|p| p.pat.is_ident())
                && f.body.is_some()
                && f.body.as_ref().unwrap().stmts.is_empty() =>
        {
            true
        }

        _ => false,
    }
}

/// Check if a class expression is pure when used with `new`.
/// This is different from `is_pure_callee` because:
/// - Calling a class as a function (`(class {})()`) throws TypeError
/// - But `new (class {})()` can be pure if the class has no side effects
fn is_pure_new_callee(expr: &Expr, ctx: ExprCtx) -> bool {
    match expr {
        // An empty function expression is also pure for `new`
        Expr::Fn(FnExpr { function: f, .. })
            if f.params.iter().all(|p| p.pat.is_ident())
                && f.body.is_some()
                && f.body.as_ref().unwrap().stmts.is_empty() =>
        {
            true
        }

        // A class expression is pure for `new` if:
        // 1. It has no side effects from definition (computed keys, property initializers, static
        //    blocks)
        // 2. It has no super class (calling super() may have side effects)
        // 3. Either has no constructor, or constructor body is empty
        // 4. Has no instance properties (they are initialized in the constructor)
        Expr::Class(c) => {
            let class = &c.class;

            // Check for super class - calling super() may have side effects
            if class.super_class.is_some() {
                return false;
            }

            // Check for side effects from class definition
            if class_has_side_effect(ctx, class) {
                return false;
            }

            // Check for instance properties (non-static) - they run during construction
            for member in &class.body {
                match member {
                    ClassMember::ClassProp(p) if !p.is_static => return false,
                    ClassMember::PrivateProp(p) if !p.is_static => return false,
                    _ => {}
                }
            }

            // Check constructor - must be empty or not present
            for member in &class.body {
                if let ClassMember::Constructor(ctor) = member {
                    if let Some(body) = &ctor.body {
                        if !body.stmts.is_empty() {
                            return false;
                        }
                    }
                }
            }

            true
        }

        _ => false,
    }
}

fn may_have_side_effects(expr: &Expr, ctx: ExprCtx) -> bool {
    let Some(ctx) = ctx.consume_depth() else {
        return true;
    };

    if expr.is_pure_callee(ctx) {
        return false;
    }

    match expr {
        Expr::Ident(i) => {
            if ctx.is_unresolved_ref_safe {
                return false;
            }

            if i.ctxt == ctx.unresolved_ctxt {
                !matches!(
                    &*i.sym,
                    "Infinity"
                        | "NaN"
                        | "Math"
                        | "undefined"
                        | "Object"
                        | "Array"
                        | "Promise"
                        | "Boolean"
                        | "Number"
                        | "String"
                        | "BigInt"
                        | "Error"
                        | "RegExp"
                        | "Function"
                        | "document"
                )
            } else {
                false
            }
        }

        Expr::Lit(..) | Expr::This(..) | Expr::PrivateName(..) | Expr::TsConstAssertion(..) => {
            false
        }

        Expr::Paren(e) => e.expr.may_have_side_effects(ctx),

        // Function expression does not have any side effect if it's not used.
        Expr::Fn(..) | Expr::Arrow(..) => false,

        // It's annoying to pass in_strict
        Expr::Class(c) => class_has_side_effect(ctx, &c.class),
        Expr::Array(ArrayLit { elems, .. }) => elems
            .iter()
            .filter_map(|e| e.as_ref())
            .any(|e| e.spread.is_some() || e.expr.may_have_side_effects(ctx)),
        Expr::Unary(UnaryExpr {
            op: op!("delete"), ..
        }) => true,
        Expr::Unary(UnaryExpr { arg, .. }) => arg.may_have_side_effects(ctx),
        Expr::Bin(BinExpr { left, right, .. }) => {
            left.may_have_side_effects(ctx) || right.may_have_side_effects(ctx)
        }

        Expr::Member(MemberExpr { obj, prop, .. })
            if obj.is_object() || obj.is_fn_expr() || obj.is_arrow() || obj.is_class() =>
        {
            if obj.may_have_side_effects(ctx) {
                return true;
            }
            match &**obj {
                Expr::Class(c) => {
                    let is_static_accessor = |member: &ClassMember| {
                        if let ClassMember::Method(ClassMethod {
                            kind: MethodKind::Getter | MethodKind::Setter,
                            is_static: true,
                            ..
                        }) = member
                        {
                            true
                        } else {
                            false
                        }
                    };
                    if c.class.body.iter().any(is_static_accessor) {
                        return true;
                    }
                }
                Expr::Object(obj) => {
                    let can_have_side_effect = |prop: &PropOrSpread| match prop {
                        PropOrSpread::Spread(_) => true,
                        PropOrSpread::Prop(prop) => match prop.as_ref() {
                            Prop::Getter(_) | Prop::Setter(_) | Prop::Method(_) => true,
                            Prop::Shorthand(Ident { sym, .. })
                            | Prop::KeyValue(KeyValueProp {
                                key: PropName::Ident(IdentName { sym, .. }),
                                ..
                            }) => &**sym == "__proto__",
                            Prop::KeyValue(KeyValueProp {
                                key: PropName::Str(Str { value: sym, .. }),
                                ..
                            }) => &**sym == "__proto__",
                            Prop::KeyValue(KeyValueProp {
                                key: PropName::Computed(_),
                                ..
                            }) => true,
                            _ => false,
                        },
                        #[cfg(swc_ast_unknown)]
                        _ => true,
                    };
                    if obj.props.iter().any(can_have_side_effect) {
                        return true;
                    }
                }
                _ => {}
            };

            match prop {
                MemberProp::Computed(c) => c.expr.may_have_side_effects(ctx),
                MemberProp::Ident(_) | MemberProp::PrivateName(_) => false,
                #[cfg(swc_ast_unknown)]
                _ => true,
            }
        }

        //TODO
        Expr::Tpl(_) => true,
        Expr::TaggedTpl(_) => true,
        Expr::MetaProp(_) => true,

        Expr::Await(_)
        | Expr::Yield(_)
        | Expr::Member(_)
        | Expr::SuperProp(_)
        | Expr::Update(_)
        | Expr::Assign(_) => true,

        Expr::OptChain(OptChainExpr { base, .. }) if matches!(&**base, OptChainBase::Member(_)) => {
            true
        }

        // A new expression is side-effect free if callee is pure for `new` and args are
        // side-effect free. Note: we use is_pure_new_callee instead of is_pure_callee because
        // class expressions are valid for `new` but calling them throws TypeError.
        Expr::New(NewExpr { callee, args, .. }) if is_pure_new_callee(callee, ctx) => args
            .iter()
            .flatten()
            .any(|arg| arg.expr.may_have_side_effects(ctx)),

        Expr::New(_) => true,

        Expr::Call(CallExpr {
            callee: Callee::Expr(callee),
            ref args,
            ..
        }) if callee.is_pure_callee(ctx) => {
            args.iter().any(|arg| arg.expr.may_have_side_effects(ctx))
        }
        Expr::OptChain(OptChainExpr { base, .. })
            if matches!(&**base, OptChainBase::Call(..))
                && OptChainBase::as_call(base)
                    .unwrap()
                    .callee
                    .is_pure_callee(ctx) =>
        {
            OptChainBase::as_call(base)
                .unwrap()
                .args
                .iter()
                .any(|arg| arg.expr.may_have_side_effects(ctx))
        }

        Expr::Call(_) | Expr::OptChain(..) => true,

        Expr::Seq(SeqExpr { exprs, .. }) => exprs.iter().any(|e| e.may_have_side_effects(ctx)),

        Expr::Cond(CondExpr {
            test, cons, alt, ..
        }) => {
            test.may_have_side_effects(ctx)
                || cons.may_have_side_effects(ctx)
                || alt.may_have_side_effects(ctx)
        }

        Expr::Object(ObjectLit { props, .. }) => props.iter().any(|node| match node {
            PropOrSpread::Prop(node) => match &**node {
                Prop::Shorthand(..) => false,
                Prop::KeyValue(KeyValueProp { key, value }) => {
                    let k = match key {
                        PropName::Computed(e) => e.expr.may_have_side_effects(ctx),
                        _ => false,
                    };

                    k || value.may_have_side_effects(ctx)
                }
                Prop::Getter(GetterProp { key, .. })
                | Prop::Setter(SetterProp { key, .. })
                | Prop::Method(MethodProp { key, .. }) => match key {
                    PropName::Computed(e) => e.expr.may_have_side_effects(ctx),
                    _ => false,
                },
                Prop::Assign(_) => true,
                #[cfg(swc_ast_unknown)]
                _ => true,
            },
            // may trigger getter
            PropOrSpread::Spread(_) => true,
            #[cfg(swc_ast_unknown)]
            _ => true,
        }),

        Expr::JSXMember(..)
        | Expr::JSXNamespacedName(..)
        | Expr::JSXEmpty(..)
        | Expr::JSXElement(..)
        | Expr::JSXFragment(..) => true,

        Expr::TsAs(TsAsExpr { ref expr, .. })
        | Expr::TsNonNull(TsNonNullExpr { ref expr, .. })
        | Expr::TsTypeAssertion(TsTypeAssertion { ref expr, .. })
        | Expr::TsInstantiation(TsInstantiation { ref expr, .. })
        | Expr::TsSatisfies(TsSatisfiesExpr { ref expr, .. }) => expr.may_have_side_effects(ctx),

        Expr::Invalid(..) => true,
        #[cfg(swc_ast_unknown)]
        _ => true,
    }
}

#[cfg(test)]
mod tests {
    use swc_common::{input::StringInput, BytePos};
    use swc_ecma_parser::{Parser, Syntax};

    use super::*;

    #[test]
    fn test_collect_decls() {
        run_collect_decls(
            "const { a, b = 1, inner: { c }, ...d } = {};",
            &["a", "b", "c", "d"],
        );
        run_collect_decls("const [ a, b = 1, [c], ...d ] = [];", &["a", "b", "c", "d"]);
    }

    #[test]
    fn test_collect_export_default_expr() {
        run_collect_decls("export default function foo(){}", &["foo"]);
        run_collect_decls("export default class Foo{}", &["Foo"]);
    }

    fn run_collect_decls(text: &str, expected_names: &[&str]) {
        let module = parse_module(text);
        let decls: FxHashSet<Id> = collect_decls(&module);
        let mut names = decls.iter().map(|d| d.0.to_string()).collect::<Vec<_>>();
        names.sort();
        assert_eq!(names, expected_names);
    }

    #[test]
    fn test_extract_var_ids() {
        run_extract_var_ids(
            "var { a, b = 1, inner: { c }, ...d } = {};",
            &["a", "b", "c", "d"],
        );
        run_extract_var_ids("var [ a, b = 1, [c], ...d ] = [];", &["a", "b", "c", "d"]);
    }

    fn run_extract_var_ids(text: &str, expected_names: &[&str]) {
        let module = parse_module(text);
        let decls = extract_var_ids(&module);
        let mut names = decls.iter().map(|d| d.sym.to_string()).collect::<Vec<_>>();
        names.sort();
        assert_eq!(names, expected_names);
    }

    fn parse_module(text: &str) -> Module {
        let syntax = Syntax::Es(Default::default());
        let mut p = Parser::new(
            syntax,
            StringInput::new(text, BytePos(0), BytePos(text.len() as u32)),
            None,
        );
        p.parse_module().unwrap()
    }

    fn has_top_level_await(text: &str) -> bool {
        let module = parse_module(text);
        contains_top_level_await(&module)
    }

    #[test]
    fn top_level_await_block() {
        assert!(has_top_level_await("if (maybe) { await test; }"))
    }

    #[test]
    fn top_level_await_for_of() {
        assert!(has_top_level_await("for await (let iter of []){}"))
    }

    #[test]
    fn top_level_export_await() {
        assert!(has_top_level_await("export const foo = await 1;"));
        assert!(has_top_level_await("export default await 1;"));
    }
}

#[cfg(test)]
mod ident_usage_finder_parallel_tests {
    use swc_atoms::Atom;
    use swc_common::SyntaxContext;
    use swc_ecma_ast::*;

    use super::*;

    fn make_id(name: &str) -> Ident {
        Ident::new(Atom::from(name), Span::dummy(), SyntaxContext::empty())
    }

    #[test]
    fn test_visit_class_members() {
        let id = make_id("foo");
        let member = ClassMember::ClassProp(ClassProp {
            key: PropName::Ident(quote_ident!("foo")),
            value: Some(Box::new(Expr::Ident(quote_ident!("foo").into()))),
            ..Default::default()
        });
        let found = IdentUsageFinder::find(&id, &vec![member.clone()]);
        assert!(found);
        let not_found = IdentUsageFinder::find(&make_id("bar"), &vec![member]);
        assert!(!not_found);
    }

    #[test]
    fn test_visit_expr_or_spreads() {
        let id = make_id("foo");
        let expr = ExprOrSpread {
            spread: None,
            expr: Box::new(Expr::Ident(quote_ident!("foo").into())),
        };
        let found = IdentUsageFinder::find(&id, &vec![expr.clone()]);
        assert!(found);
        let not_found = IdentUsageFinder::find(&make_id("bar"), &vec![expr]);
        assert!(!not_found);
    }

    #[test]
    fn test_visit_module_items() {
        let id = make_id("foo");
        let item = ModuleItem::Stmt(Stmt::Expr(ExprStmt {
            span: DUMMY_SP,
            expr: Box::new(Expr::Ident(quote_ident!("foo").into())),
        }));
        let found = IdentUsageFinder::find(&id, &vec![item.clone()]);
        assert!(found);
        let not_found = IdentUsageFinder::find(&make_id("bar"), &vec![item]);
        assert!(!not_found);
    }

    #[test]
    fn test_visit_stmts() {
        let id = make_id("foo");
        let stmt = Stmt::Expr(ExprStmt {
            span: DUMMY_SP,
            expr: Box::new(Expr::Ident(quote_ident!("foo").into())),
        });
        let found = IdentUsageFinder::find(&id, &vec![stmt.clone()]);
        assert!(found);
        let not_found = IdentUsageFinder::find(&make_id("bar"), &vec![stmt]);
        assert!(!not_found);
    }

    #[test]
    fn test_visit_opt_vec_expr_or_spreads() {
        let id = make_id("foo");
        let expr = Some(ExprOrSpread {
            spread: None,
            expr: Box::new(Expr::Ident(quote_ident!("foo").into())),
        });
        let found = IdentUsageFinder::find(&id, &vec![expr.clone()]);
        assert!(found);
        let not_found = IdentUsageFinder::find(&make_id("bar"), &vec![expr]);
        assert!(!not_found);
    }

    #[test]
    fn test_visit_var_declarators() {
        let id = make_id("foo");
        let decl = VarDeclarator {
            span: DUMMY_SP,
            name: Pat::Ident(quote_ident!("foo").into()),
            init: None,
            definite: false,
        };
        let found = IdentUsageFinder::find(&id, &vec![decl.clone()]);
        assert!(found);
        let not_found = IdentUsageFinder::find(&make_id("bar"), &vec![decl]);
        assert!(!not_found);
    }

    #[test]
    fn test_visit_exprs() {
        let id = make_id("foo");
        let expr = Box::new(Expr::Ident(quote_ident!("foo").into()));
        let found = IdentUsageFinder::find(&id, &vec![expr.clone()]);
        assert!(found);
        let not_found = IdentUsageFinder::find(&make_id("bar"), &vec![expr]);
        assert!(!not_found);
    }
}
