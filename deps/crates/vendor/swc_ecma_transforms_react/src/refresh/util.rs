use rustc_hash::FxHashSet;
use swc_common::{Spanned, SyntaxContext, DUMMY_SP};
use swc_ecma_ast::*;
use swc_ecma_utils::ExprFactory;
use swc_ecma_visit::{noop_visit_type, Visit, VisitWith};

pub fn is_builtin_hook(name: &str) -> bool {
    matches!(
        name,
        "useState"
            | "useReducer"
            | "useEffect"
            | "useLayoutEffect"
            | "useMemo"
            | "useCallback"
            | "useRef"
            | "useContext"
            | "useImperativeHandle"
            | "useDebugValue"
    )
}

pub fn is_body_arrow_fn(body: &BlockStmtOrExpr) -> bool {
    if let BlockStmtOrExpr::Expr(body) = body {
        body.is_arrow()
    } else {
        false
    }
}

fn assert_hygiene(e: &Expr) {
    if !cfg!(debug_assertions) {
        return;
    }

    if let Expr::Ident(i) = e {
        if i.ctxt == SyntaxContext::empty() {
            panic!("`{i}` should be resolved")
        }
    }
}

pub fn make_assign_stmt(handle: Ident, expr: Box<Expr>) -> Expr {
    assert_hygiene(&expr);

    AssignExpr {
        span: expr.span(),
        op: op!("="),
        left: handle.into(),
        right: expr,
    }
    .into()
}

pub fn make_call_stmt(handle: Ident) -> Stmt {
    ExprStmt {
        span: DUMMY_SP,
        expr: Box::new(make_call_expr(handle)),
    }
    .into()
}

pub fn make_call_expr(handle: Ident) -> Expr {
    CallExpr {
        span: DUMMY_SP,
        callee: handle.as_callee(),
        args: Vec::new(),
        ..Default::default()
    }
    .into()
}

pub fn is_import_or_require(expr: &Expr) -> bool {
    match expr {
        Expr::Call(CallExpr {
            callee: Callee::Expr(expr),
            ..
        }) => {
            if let Expr::Ident(ident) = expr.as_ref() {
                ident.sym.contains("require")
            } else {
                false
            }
        }
        Expr::Call(CallExpr {
            callee: Callee::Import(_),
            ..
        }) => true,
        _ => false,
    }
}

pub struct UsedInJsx(FxHashSet<Id>);

impl Visit for UsedInJsx {
    noop_visit_type!();

    fn visit_call_expr(&mut self, n: &CallExpr) {
        n.visit_children_with(self);

        if let Callee::Expr(expr) = &n.callee {
            let ident = match expr.as_ref() {
                Expr::Ident(ident) => ident.to_id(),
                Expr::Member(MemberExpr {
                    prop: MemberProp::Ident(ident),
                    ..
                }) => (ident.sym.clone(), SyntaxContext::empty()),
                _ => return,
            };
            if matches!(
                ident.0.as_ref(),
                "createElement" | "jsx" | "jsxDEV" | "jsxs"
            ) {
                if let Some(ExprOrSpread { expr, .. }) = n.args.first() {
                    if let Expr::Ident(ident) = expr.as_ref() {
                        self.0.insert(ident.to_id());
                    }
                }
            }
        }
    }

    fn visit_jsx_opening_element(&mut self, n: &JSXOpeningElement) {
        if let JSXElementName::Ident(ident) = &n.name {
            self.0.insert(ident.to_id());
        }
    }
}

pub fn collect_ident_in_jsx<V: VisitWith<UsedInJsx>>(item: &V) -> FxHashSet<Id> {
    let mut visitor = UsedInJsx(FxHashSet::default());
    item.visit_with(&mut visitor);
    visitor.0
}
