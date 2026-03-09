use rustc_hash::FxHashSet;
use swc_atoms::atom;
use swc_common::{
    comments::Comments, sync::Lrc, util::take::Take, BytePos, Mark, SourceMap, SourceMapper, Span,
    Spanned, SyntaxContext, DUMMY_SP,
};
use swc_ecma_ast::*;
use swc_ecma_hooks::VisitMutHook;
use swc_ecma_utils::{private_ident, quote_ident, quote_str, ExprFactory};
use swc_ecma_visit::{Visit, VisitMutWith, VisitWith};

use self::{
    hook::HookRegister,
    util::{collect_ident_in_jsx, is_body_arrow_fn, is_import_or_require, make_assign_stmt},
};

pub mod options;
use options::RefreshOptions;
mod hook;
mod util;

#[cfg(test)]
mod tests;

struct Hoc {
    insert: bool,
    reg: Vec<(Ident, Id)>,
    hook: Option<HocHook>,
}
struct HocHook {
    callee: Callee,
    rest_arg: Vec<ExprOrSpread>,
}
enum Persist {
    Hoc(Hoc),
    Component(Ident),
    None,
}
fn get_persistent_id(ident: &Ident) -> Persist {
    if ident.sym.starts_with(|c: char| c.is_ascii_uppercase()) {
        if cfg!(debug_assertions) && ident.ctxt == SyntaxContext::empty() {
            panic!("`{ident}` should be resolved")
        }
        Persist::Component(ident.clone())
    } else {
        Persist::None
    }
}

/// `react-refresh/babel`
/// https://github.com/facebook/react/blob/main/packages/react-refresh/src/ReactFreshBabelPlugin.js
pub fn hook<C: Comments>(
    dev: bool,
    options: Option<RefreshOptions>,
    cm: Lrc<SourceMap>,
    comments: Option<C>,
    global_mark: Mark,
) -> impl VisitMutHook<()> {
    Refresh {
        enable: dev && options.is_some(),
        cm,
        comments,
        should_reset: false,
        options: options.unwrap_or_default(),
        global_mark,
    }
}

struct Refresh<C: Comments> {
    enable: bool,
    options: RefreshOptions,
    cm: Lrc<SourceMap>,
    should_reset: bool,
    comments: Option<C>,
    global_mark: Mark,
}

// For tests
#[cfg(test)]
fn refresh<C: Comments>(
    dev: bool,
    options: Option<RefreshOptions>,
    cm: Lrc<SourceMap>,
    comments: Option<C>,
    global_mark: Mark,
) -> impl Pass {
    use swc_ecma_hooks::VisitMutWithHook;
    use swc_ecma_visit::visit_mut_pass;

    visit_mut_pass(VisitMutWithHook {
        hook: hook(dev, options, cm, comments, global_mark),
        context: (),
    })
}

impl<C: Comments> Refresh<C> {
    fn get_persistent_id_from_var_decl(
        &self,
        var_decl: &mut VarDecl,
        used_in_jsx: &FxHashSet<Id>,
        hook_reg: &mut HookRegister,
    ) -> Persist {
        // We only handle the case when a single variable is declared
        if let [VarDeclarator {
            name: Pat::Ident(binding),
            init: Some(init_expr),
            ..
        }] = var_decl.decls.as_mut_slice()
        {
            if used_in_jsx.contains(&binding.to_id()) && !is_import_or_require(init_expr) {
                match init_expr.as_ref() {
                    // TaggedTpl is for something like styled.div`...`
                    Expr::Arrow(_) | Expr::Fn(_) | Expr::TaggedTpl(_) | Expr::Call(_) => {
                        return Persist::Component(Ident::from(&*binding))
                    }
                    _ => (),
                }
            }

            if let Persist::Component(persistent_id) = get_persistent_id(&Ident::from(&*binding)) {
                return match init_expr.as_mut() {
                    Expr::Fn(_) => Persist::Component(persistent_id),
                    Expr::Arrow(ArrowExpr { body, .. }) => {
                        // Ignore complex function expressions like
                        // let Foo = () => () => {}
                        if is_body_arrow_fn(body) {
                            Persist::None
                        } else {
                            Persist::Component(persistent_id)
                        }
                    }
                    // Maybe a HOC.
                    Expr::Call(call_expr) => {
                        let res = self.get_persistent_id_from_possible_hoc(
                            call_expr,
                            vec![(private_ident!("_c"), persistent_id.to_id())],
                            hook_reg,
                        );
                        if let Persist::Hoc(Hoc {
                            insert,
                            reg,
                            hook: Some(hook),
                        }) = res
                        {
                            make_hook_reg(init_expr.as_mut(), hook);
                            Persist::Hoc(Hoc {
                                insert,
                                reg,
                                hook: None,
                            })
                        } else {
                            res
                        }
                    }
                    _ => Persist::None,
                };
            }
        }
        Persist::None
    }

    fn get_persistent_id_from_possible_hoc(
        &self,
        call_expr: &mut CallExpr,
        mut reg: Vec<(Ident, Id)>,
        hook_reg: &mut HookRegister,
    ) -> Persist {
        let first_arg = match call_expr.args.as_mut_slice() {
            [first, ..] => &mut first.expr,
            _ => return Persist::None,
        };
        let callee = if let Callee::Expr(expr) = &call_expr.callee {
            expr
        } else {
            return Persist::None;
        };
        let hoc_name = match callee.as_ref() {
            Expr::Ident(fn_name) => fn_name.sym.to_string(),
            // original react implement use `getSource` so we just follow them
            Expr::Member(member) => self.cm.span_to_snippet(member.span).unwrap_or_default(),
            _ => return Persist::None,
        };
        let reg_str = (
            format!("{}${}", reg.last().unwrap().1 .0, &hoc_name).into(),
            SyntaxContext::empty(),
        );
        match first_arg.as_mut() {
            Expr::Call(expr) => {
                let reg_ident = private_ident!("_c");
                reg.push((reg_ident.clone(), reg_str));
                if let Persist::Hoc(hoc) =
                    self.get_persistent_id_from_possible_hoc(expr, reg, hook_reg)
                {
                    let mut first = first_arg.take();
                    if let Some(HocHook { callee, rest_arg }) = &hoc.hook {
                        let span = first.span();
                        let mut args = vec![first.as_arg()];
                        args.extend(rest_arg.clone());
                        first = CallExpr {
                            span,
                            callee: callee.clone(),
                            args,
                            ..Default::default()
                        }
                        .into()
                    }
                    *first_arg = Box::new(make_assign_stmt(reg_ident, first));

                    Persist::Hoc(hoc)
                } else {
                    Persist::None
                }
            }
            Expr::Fn(_) | Expr::Arrow(_) => {
                let reg_ident = private_ident!("_c");
                let mut first = first_arg.take();
                first.visit_mut_with(hook_reg);
                let hook = if let Expr::Call(call) = first.as_ref() {
                    let res = Some(HocHook {
                        callee: call.callee.clone(),
                        rest_arg: call.args[1..].to_owned(),
                    });
                    *first_arg = Box::new(make_assign_stmt(reg_ident.clone(), first));
                    res
                } else {
                    *first_arg = Box::new(make_assign_stmt(reg_ident.clone(), first));
                    None
                };
                reg.push((reg_ident, reg_str));
                Persist::Hoc(Hoc {
                    reg,
                    insert: true,
                    hook,
                })
            }
            // export default hoc(Foo)
            // const X = hoc(Foo)
            Expr::Ident(ident) => {
                if let Persist::Component(_) = get_persistent_id(ident) {
                    Persist::Hoc(Hoc {
                        reg,
                        insert: true,
                        hook: None,
                    })
                } else {
                    Persist::None
                }
            }
            _ => Persist::None,
        }
    }
}

impl<C: Comments> VisitMutHook<()> for Refresh<C> {
    fn enter_module_items(&mut self, module_items: &mut Vec<ModuleItem>, _ctx: &mut ()) {
        if !self.enable {
            return;
        }

        // First, visit all items to collect @refresh reset comments
        self.visit_module(module_items);

        self.handle_module_items(module_items);
    }

    fn enter_ts_module_decl(&mut self, _: &mut TsModuleDecl, _ctx: &mut ()) {
        // Skip processing TypeScript module declarations
    }
}

impl<C: Comments> Refresh<C> {
    fn visit_module(&mut self, module_items: &[ModuleItem]) {
        struct SpanVisitor<'a, C: Comments> {
            refresh: &'a mut Refresh<C>,
        }

        impl<C: Comments> Visit for SpanVisitor<'_, C> {
            fn visit_span(&mut self, n: &Span) {
                if self.refresh.should_reset {
                    return;
                }

                let mut should_refresh = self.refresh.should_reset;
                if let Some(comments) = &self.refresh.comments {
                    if !n.hi.is_dummy() {
                        comments.with_leading(n.hi - BytePos(1), |comments| {
                            if comments.iter().any(|c| c.text.contains("@refresh reset")) {
                                should_refresh = true
                            }
                        });
                    }

                    comments.with_leading(n.lo, |comments| {
                        if comments.iter().any(|c| c.text.contains("@refresh reset")) {
                            should_refresh = true
                        }
                    });

                    comments.with_trailing(n.lo, |comments| {
                        if comments.iter().any(|c| c.text.contains("@refresh reset")) {
                            should_refresh = true
                        }
                    });
                }

                self.refresh.should_reset = should_refresh;
            }
        }

        let mut visitor = SpanVisitor { refresh: self };
        for item in module_items {
            item.visit_with(&mut visitor);
        }
    }

    fn handle_module_items(&mut self, module_items: &mut Vec<ModuleItem>) {
        let used_in_jsx = collect_ident_in_jsx(module_items);

        let mut items = Vec::with_capacity(module_items.len());
        let mut refresh_regs = Vec::<(Ident, Id)>::new();

        let mut hook_visitor = HookRegister {
            options: &self.options,
            ident: Vec::new(),
            extra_stmt: Vec::new(),
            current_scope: vec![SyntaxContext::empty().apply_mark(self.global_mark)],
            cm: &self.cm,
            should_reset: self.should_reset,
        };

        for mut item in module_items.take() {
            let persistent_id = match &mut item {
                // function Foo() {}
                ModuleItem::Stmt(Stmt::Decl(Decl::Fn(FnDecl { ident, .. }))) => {
                    get_persistent_id(ident)
                }

                // export function Foo() {}
                ModuleItem::ModuleDecl(ModuleDecl::ExportDecl(ExportDecl {
                    decl: Decl::Fn(FnDecl { ident, .. }),
                    ..
                })) => get_persistent_id(ident),

                // export default function Foo() {}
                ModuleItem::ModuleDecl(ModuleDecl::ExportDefaultDecl(ExportDefaultDecl {
                    decl:
                        DefaultDecl::Fn(FnExpr {
                            // We don't currently handle anonymous default exports.
                            ident: Some(ident),
                            ..
                        }),
                    ..
                })) => get_persistent_id(ident),

                // const Foo = () => {}
                // export const Foo = () => {}
                ModuleItem::Stmt(Stmt::Decl(Decl::Var(var_decl)))
                | ModuleItem::ModuleDecl(ModuleDecl::ExportDecl(ExportDecl {
                    decl: Decl::Var(var_decl),
                    ..
                })) => {
                    self.get_persistent_id_from_var_decl(var_decl, &used_in_jsx, &mut hook_visitor)
                }

                // This code path handles nested cases like:
                // export default memo(() => {})
                // In those cases it is more plausible people will omit names
                // so they're worth handling despite possible false positives.
                ModuleItem::ModuleDecl(ModuleDecl::ExportDefaultExpr(ExportDefaultExpr {
                    expr,
                    span,
                })) => {
                    if let Expr::Call(call) = expr.as_mut() {
                        if let Persist::Hoc(Hoc { reg, hook, .. }) = self
                            .get_persistent_id_from_possible_hoc(
                                call,
                                vec![(
                                    private_ident!("_c"),
                                    (atom!("%default%"), SyntaxContext::empty()),
                                )],
                                &mut hook_visitor,
                            )
                        {
                            if let Some(hook) = hook {
                                make_hook_reg(expr.as_mut(), hook)
                            }
                            item = ExportDefaultExpr {
                                expr: Box::new(make_assign_stmt(reg[0].0.clone(), expr.take())),
                                span: *span,
                            }
                            .into();
                            Persist::Hoc(Hoc {
                                insert: false,
                                reg,
                                hook: None,
                            })
                        } else {
                            Persist::None
                        }
                    } else {
                        Persist::None
                    }
                }

                _ => Persist::None,
            };

            if let Persist::Hoc(_) = persistent_id {
                // we need to make hook transform happens after component for
                // HOC
                items.push(item);
            } else {
                item.visit_mut_children_with(&mut hook_visitor);

                items.push(item);
                items.extend(
                    hook_visitor
                        .extra_stmt
                        .take()
                        .into_iter()
                        .map(ModuleItem::Stmt),
                );
            }

            match persistent_id {
                Persist::None => (),
                Persist::Component(persistent_id) => {
                    let registration_handle = private_ident!("_c");

                    refresh_regs.push((registration_handle.clone(), persistent_id.to_id()));

                    items.push(
                        ExprStmt {
                            span: DUMMY_SP,
                            expr: Box::new(make_assign_stmt(
                                registration_handle,
                                persistent_id.into(),
                            )),
                        }
                        .into(),
                    );
                }

                Persist::Hoc(mut hoc) => {
                    hoc.reg = hoc.reg.into_iter().rev().collect();
                    if hoc.insert {
                        let (ident, name) = hoc.reg.last().unwrap();
                        items.push(
                            ExprStmt {
                                span: DUMMY_SP,
                                expr: Box::new(make_assign_stmt(
                                    ident.clone(),
                                    Ident::new(name.0.clone(), DUMMY_SP, name.1).into(),
                                )),
                            }
                            .into(),
                        )
                    }
                    refresh_regs.append(&mut hoc.reg);
                }
            }
        }

        if !hook_visitor.ident.is_empty() {
            items.insert(0, hook_visitor.gen_hook_handle().into());
        }

        // Insert
        // ```
        // var _c, _c1;
        // ```
        if !refresh_regs.is_empty() {
            items.push(
                VarDecl {
                    span: DUMMY_SP,
                    kind: VarDeclKind::Var,
                    declare: false,
                    decls: refresh_regs
                        .iter()
                        .map(|(handle, _)| VarDeclarator {
                            span: DUMMY_SP,
                            name: handle.clone().into(),
                            init: None,
                            definite: false,
                        })
                        .collect(),
                    ..Default::default()
                }
                .into(),
            );
        }

        // Insert
        // ```
        // $RefreshReg$(_c, "Hello");
        // $RefreshReg$(_c1, "Foo");
        // ```
        let refresh_reg = self.options.refresh_reg.as_str();
        for (handle, persistent_id) in refresh_regs {
            items.push(
                ExprStmt {
                    span: DUMMY_SP,
                    expr: CallExpr {
                        callee: quote_ident!(refresh_reg).as_callee(),
                        args: vec![handle.as_arg(), quote_str!(persistent_id.0).as_arg()],
                        ..Default::default()
                    }
                    .into(),
                }
                .into(),
            );
        }

        *module_items = items
    }
}

fn make_hook_reg(expr: &mut Expr, mut hook: HocHook) {
    let span = expr.span();
    let mut args = vec![expr.take().as_arg()];
    args.append(&mut hook.rest_arg);
    *expr = CallExpr {
        span,
        callee: hook.callee,
        args,
        ..Default::default()
    }
    .into();
}
