use std::{fmt::Write, mem};

use base64::prelude::{Engine, BASE64_STANDARD};
use sha1::{Digest, Sha1};
use swc_common::{util::take::Take, SourceMap, SourceMapper, Spanned, SyntaxContext, DUMMY_SP};
use swc_ecma_ast::*;
use swc_ecma_utils::{private_ident, quote_ident, ExprFactory};
use swc_ecma_visit::{
    noop_visit_mut_type, noop_visit_type, Visit, VisitMut, VisitMutWith, VisitWith,
};

use super::util::{is_builtin_hook, make_call_expr, make_call_stmt};
use crate::RefreshOptions;

// function that use hooks
struct HookSig {
    handle: Ident,
    // need to add an extra register, or already inlined
    hooks: Vec<Hook>,
}

impl HookSig {
    fn new(hooks: Vec<Hook>) -> Self {
        HookSig {
            handle: private_ident!("_s"),
            hooks,
        }
    }
}

struct Hook {
    callee: HookCall,
    key: String,
}

// we only consider two kinds of callee as hook call
#[allow(clippy::large_enum_variant)]
enum HookCall {
    Ident(Ident),
    Member(Expr, IdentName), // for obj and prop
}
pub struct HookRegister<'a> {
    pub options: &'a RefreshOptions,
    pub ident: Vec<Ident>,
    pub extra_stmt: Vec<Stmt>,
    pub current_scope: Vec<SyntaxContext>,
    pub cm: &'a SourceMap,
    pub should_reset: bool,
}

impl HookRegister<'_> {
    pub fn gen_hook_handle(&mut self) -> Stmt {
        VarDecl {
            span: DUMMY_SP,
            kind: VarDeclKind::Var,
            decls: self
                .ident
                .take()
                .into_iter()
                .map(|id| VarDeclarator {
                    span: DUMMY_SP,
                    name: id.into(),
                    init: Some(Box::new(make_call_expr(
                        quote_ident!(self.options.refresh_sig.clone()).into(),
                    ))),
                    definite: false,
                })
                .collect(),
            declare: false,
            ..Default::default()
        }
        .into()
    }

    // The second call is around the function itself. This is used to associate a
    // type with a signature.
    // Unlike with $RefreshReg$, this needs to work for nested declarations too.
    fn wrap_with_register(&self, handle: Ident, func: Expr, hooks: Vec<Hook>) -> Expr {
        let mut args = vec![func.as_arg()];
        let mut sign = Vec::new();
        let mut custom_hook = Vec::new();

        for hook in hooks {
            let name = match &hook.callee {
                HookCall::Ident(i) => i.clone(),
                HookCall::Member(_, i) => i.clone().into(),
            };
            sign.push(format!("{}{{{}}}", name.sym, hook.key));
            match &hook.callee {
                HookCall::Ident(ident) if !is_builtin_hook(&ident.sym) => {
                    custom_hook.push(hook.callee);
                }
                HookCall::Member(Expr::Ident(obj_ident), prop) if !is_builtin_hook(&prop.sym) => {
                    if obj_ident.sym.as_ref() != "React" {
                        custom_hook.push(hook.callee);
                    }
                }
                _ => (),
            };
        }

        let sign = sign.join("\n");
        let sign = if self.options.emit_full_signatures {
            sign
        } else {
            let mut hasher = Sha1::new();
            hasher.update(sign);
            BASE64_STANDARD.encode(hasher.finalize())
        };

        args.push(
            Lit::Str(Str {
                span: DUMMY_SP,
                raw: None,
                value: sign.into(),
            })
            .as_arg(),
        );

        let mut should_reset = self.should_reset;

        let mut custom_hook_in_scope = Vec::new();

        for hook in custom_hook {
            let ident = match &hook {
                HookCall::Ident(ident) => Some(ident),
                HookCall::Member(Expr::Ident(ident), _) => Some(ident),
                _ => None,
            };
            if !ident
                .map(|id| self.current_scope.contains(&id.ctxt))
                .unwrap_or(false)
            {
                // We don't have anything to put in the array because Hook is out of scope.
                // Since it could potentially have been edited, remount the component.
                should_reset = true;
            } else {
                custom_hook_in_scope.push(hook);
            }
        }

        if should_reset || !custom_hook_in_scope.is_empty() {
            args.push(should_reset.as_arg());
        }

        if !custom_hook_in_scope.is_empty() {
            let elems = custom_hook_in_scope
                .into_iter()
                .map(|hook| {
                    Some(
                        match hook {
                            HookCall::Ident(ident) => Expr::from(ident),
                            HookCall::Member(obj, prop) => MemberExpr {
                                span: DUMMY_SP,
                                obj: Box::new(obj),
                                prop: MemberProp::Ident(prop),
                            }
                            .into(),
                        }
                        .as_arg(),
                    )
                })
                .collect();
            args.push(
                Function {
                    is_generator: false,
                    is_async: false,
                    params: Vec::new(),
                    decorators: Vec::new(),
                    span: DUMMY_SP,
                    body: Some(BlockStmt {
                        span: DUMMY_SP,
                        stmts: vec![Stmt::Return(ReturnStmt {
                            span: DUMMY_SP,
                            arg: Some(Box::new(Expr::Array(ArrayLit {
                                span: DUMMY_SP,
                                elems,
                            }))),
                        })],
                        ..Default::default()
                    }),
                    ..Default::default()
                }
                .as_arg(),
            );
        }

        CallExpr {
            span: DUMMY_SP,
            callee: handle.as_callee(),
            args,
            ..Default::default()
        }
        .into()
    }

    fn gen_hook_register_stmt(&mut self, ident: Ident, sig: HookSig) {
        self.ident.push(sig.handle.clone());
        self.extra_stmt.push(
            ExprStmt {
                span: DUMMY_SP,
                expr: Box::new(self.wrap_with_register(sig.handle, ident.into(), sig.hooks)),
            }
            .into(),
        )
    }
}

impl VisitMut for HookRegister<'_> {
    noop_visit_mut_type!();

    fn visit_mut_block_stmt(&mut self, b: &mut BlockStmt) {
        let old_ident = self.ident.take();
        let old_stmts = self.extra_stmt.take();

        self.current_scope.push(b.ctxt);

        let stmt_count = b.stmts.len();
        let stmts = mem::replace(&mut b.stmts, Vec::with_capacity(stmt_count));

        for mut stmt in stmts {
            stmt.visit_mut_children_with(self);

            b.stmts.push(stmt);
            b.stmts.append(&mut self.extra_stmt);
        }

        if !self.ident.is_empty() {
            b.stmts.insert(0, self.gen_hook_handle())
        }

        self.current_scope.pop();
        self.ident = old_ident;
        self.extra_stmt = old_stmts;
    }

    fn visit_mut_expr(&mut self, e: &mut Expr) {
        e.visit_mut_children_with(self);

        match e {
            Expr::Fn(FnExpr { function: f, .. }) if f.body.is_some() => {
                let sig = collect_hooks(&mut f.body.as_mut().unwrap().stmts, self.cm);

                if let Some(HookSig { handle, hooks }) = sig {
                    self.ident.push(handle.clone());
                    *e = self.wrap_with_register(handle, e.take(), hooks);
                }
            }
            Expr::Arrow(ArrowExpr { body, .. }) => {
                let sig = collect_hooks_arrow(body, self.cm);

                if let Some(HookSig { handle, hooks }) = sig {
                    self.ident.push(handle.clone());
                    *e = self.wrap_with_register(handle, e.take(), hooks);
                }
            }
            _ => (),
        }
    }

    fn visit_mut_var_decl(&mut self, n: &mut VarDecl) {
        // we don't want visit_mut_expr to mess up with function name inference
        // so intercept it here

        for decl in n.decls.iter_mut() {
            if let VarDeclarator {
                // it doesn't quite make sense for other Pat to appear here
                name: Pat::Ident(id),
                init: Some(init),
                ..
            } = decl
            {
                match init.as_mut() {
                    Expr::Fn(FnExpr { function: f, .. }) if f.body.is_some() => {
                        f.body.visit_mut_with(self);
                        if let Some(sig) =
                            collect_hooks(&mut f.body.as_mut().unwrap().stmts, self.cm)
                        {
                            self.gen_hook_register_stmt(Ident::from(&*id), sig);
                        }
                    }
                    Expr::Arrow(ArrowExpr { body, .. }) => {
                        body.visit_mut_with(self);
                        if let Some(sig) = collect_hooks_arrow(body, self.cm) {
                            self.gen_hook_register_stmt(Ident::from(&*id), sig);
                        }
                    }
                    _ => self.visit_mut_expr(init),
                }
            } else {
                decl.visit_mut_children_with(self)
            }
        }
    }

    fn visit_mut_default_decl(&mut self, d: &mut DefaultDecl) {
        d.visit_mut_children_with(self);

        // only when expr has ident
        match d {
            DefaultDecl::Fn(FnExpr {
                ident: Some(ident),
                function: f,
            }) if f.body.is_some() => {
                if let Some(sig) = collect_hooks(&mut f.body.as_mut().unwrap().stmts, self.cm) {
                    self.gen_hook_register_stmt(ident.clone(), sig);
                }
            }
            _ => {}
        }
    }

    fn visit_mut_fn_decl(&mut self, f: &mut FnDecl) {
        f.visit_mut_children_with(self);

        if let Some(body) = &mut f.function.body {
            if let Some(sig) = collect_hooks(&mut body.stmts, self.cm) {
                self.gen_hook_register_stmt(f.ident.clone(), sig);
            }
        }
    }
}

fn collect_hooks(stmts: &mut Vec<Stmt>, cm: &SourceMap) -> Option<HookSig> {
    let mut hook = HookCollector {
        state: Vec::new(),
        cm,
    };

    stmts.visit_with(&mut hook);

    if !hook.state.is_empty() {
        let sig = HookSig::new(hook.state);
        stmts.insert(0, make_call_stmt(sig.handle.clone()));

        Some(sig)
    } else {
        None
    }
}

fn collect_hooks_arrow(body: &mut BlockStmtOrExpr, cm: &SourceMap) -> Option<HookSig> {
    match body {
        BlockStmtOrExpr::BlockStmt(block) => collect_hooks(&mut block.stmts, cm),
        BlockStmtOrExpr::Expr(expr) => {
            let mut hook = HookCollector {
                state: Vec::new(),
                cm,
            };

            expr.visit_with(&mut hook);

            if !hook.state.is_empty() {
                let sig = HookSig::new(hook.state);
                *body = BlockStmtOrExpr::BlockStmt(BlockStmt {
                    span: expr.span(),
                    stmts: vec![
                        make_call_stmt(sig.handle.clone()),
                        Stmt::Return(ReturnStmt {
                            span: expr.span(),
                            arg: Some(Box::new(expr.as_mut().take())),
                        }),
                    ],
                    ..Default::default()
                });
                Some(sig)
            } else {
                None
            }
        }
        #[cfg(swc_ast_unknown)]
        _ => None,
    }
}

struct HookCollector<'a> {
    state: Vec<Hook>,
    cm: &'a SourceMap,
}

fn is_hook_like(s: &str) -> bool {
    if let Some(s) = s.strip_prefix("use") {
        s.chars().next().map(|c| c.is_uppercase()).unwrap_or(false)
    } else {
        false
    }
}

impl HookCollector<'_> {
    fn get_hook_from_call_expr(&self, expr: &CallExpr, lhs: Option<&Pat>) -> Option<Hook> {
        let callee = if let Callee::Expr(callee) = &expr.callee {
            Some(callee.as_ref())
        } else {
            None
        }?;
        let mut hook_call = None;
        let ident = match callee {
            Expr::Ident(ident) => {
                hook_call = Some(HookCall::Ident(ident.clone()));
                Some(&ident.sym)
            }
            // hook cannot be used in class, so we're fine without SuperProp
            Expr::Member(MemberExpr {
                obj,
                prop: MemberProp::Ident(ident),
                ..
            }) => {
                hook_call = Some(HookCall::Member(*obj.clone(), ident.clone()));
                Some(&ident.sym)
            }
            _ => None,
        }?;
        let name = if is_hook_like(ident) {
            Some(ident)
        } else {
            None
        }?;
        let mut key = if let Some(name) = lhs {
            self.cm.span_to_snippet(name.span()).unwrap_or_default()
        } else {
            String::new()
        };
        // Some built-in Hooks reset on edits to arguments.
        if *name == "useState" && !expr.args.is_empty() {
            // useState first argument is initial state.
            let _ = write!(
                key,
                "({})",
                self.cm
                    .span_to_snippet(expr.args[0].span())
                    .unwrap_or_default()
            );
        } else if name == "useReducer" && expr.args.len() > 1 {
            // useReducer second argument is initial state.
            let _ = write!(
                key,
                "({})",
                self.cm
                    .span_to_snippet(expr.args[1].span())
                    .unwrap_or_default()
            );
        }

        let callee = hook_call?;
        Some(Hook { callee, key })
    }

    fn get_hook_from_expr(&self, expr: &Expr, lhs: Option<&Pat>) -> Option<Hook> {
        if let Expr::Call(call) = expr {
            self.get_hook_from_call_expr(call, lhs)
        } else {
            None
        }
    }
}

impl Visit for HookCollector<'_> {
    noop_visit_type!();

    fn visit_block_stmt_or_expr(&mut self, _: &BlockStmtOrExpr) {}

    fn visit_block_stmt(&mut self, _: &BlockStmt) {}

    fn visit_expr(&mut self, expr: &Expr) {
        expr.visit_children_with(self);

        if let Expr::Call(call) = expr {
            if let Some(hook) = self.get_hook_from_call_expr(call, None) {
                self.state.push(hook)
            }
        }
    }

    fn visit_stmt(&mut self, stmt: &Stmt) {
        match stmt {
            Stmt::Expr(ExprStmt { expr, .. }) => {
                if let Some(hook) = self.get_hook_from_expr(expr, None) {
                    self.state.push(hook)
                } else {
                    stmt.visit_children_with(self)
                }
            }
            Stmt::Decl(Decl::Var(var_decl)) => {
                for decl in &var_decl.decls {
                    if let Some(init) = &decl.init {
                        if let Some(hook) = self.get_hook_from_expr(init, Some(&decl.name)) {
                            self.state.push(hook)
                        } else {
                            stmt.visit_children_with(self)
                        }
                    } else {
                        stmt.visit_children_with(self)
                    }
                }
            }
            Stmt::Return(ReturnStmt { arg: Some(arg), .. }) => {
                if let Some(hook) = self.get_hook_from_expr(arg.as_ref(), None) {
                    self.state.push(hook)
                } else {
                    stmt.visit_children_with(self)
                }
            }
            _ => stmt.visit_children_with(self),
        }
    }
}
