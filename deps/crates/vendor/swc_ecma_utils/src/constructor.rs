use std::{iter, mem};

use swc_common::{util::take::Take, DUMMY_SP};
use swc_ecma_ast::*;
use swc_ecma_visit::{noop_visit_mut_type, VisitMut, VisitMutWith};

use crate::ExprFactory;

pub fn inject_after_super(c: &mut Constructor, exprs: Vec<Box<Expr>>) {
    if exprs.is_empty() {
        return;
    }

    let body = c.body.as_mut().expect("constructor should have a body");

    let mut injector = Injector {
        exprs,
        ..Default::default()
    };

    body.visit_mut_with(&mut injector);

    if !injector.injected {
        let exprs = injector.exprs.take();
        body.stmts
            .splice(0..0, exprs.into_iter().map(|e| e.into_stmt()));
    }
}

#[derive(Default)]
struct Injector {
    exprs: Vec<Box<Expr>>,
    ignore_return_value: bool,

    injected: bool,
}

impl VisitMut for Injector {
    noop_visit_mut_type!();

    fn visit_mut_constructor(&mut self, _: &mut Constructor) {
        // skip
    }

    fn visit_mut_function(&mut self, _: &mut Function) {
        // skip
    }

    fn visit_mut_getter_prop(&mut self, _: &mut GetterProp) {
        // skip
    }

    fn visit_mut_setter_prop(&mut self, _: &mut SetterProp) {
        // skip
    }

    fn visit_mut_expr_stmt(&mut self, node: &mut ExprStmt) {
        let ignore_return_value = mem::replace(&mut self.ignore_return_value, true);
        node.visit_mut_children_with(self);
        self.ignore_return_value = ignore_return_value;
    }

    fn visit_mut_seq_expr(&mut self, node: &mut SeqExpr) {
        if let Some(mut tail) = node.exprs.pop() {
            let ignore_return_value = mem::replace(&mut self.ignore_return_value, true);
            node.visit_mut_children_with(self);
            self.ignore_return_value = ignore_return_value;
            tail.visit_mut_with(self);
            node.exprs.push(tail);
        }
    }

    fn visit_mut_expr(&mut self, node: &mut Expr) {
        let ignore_return_value = self.ignore_return_value;
        if !matches!(node, Expr::Paren(..) | Expr::Seq(..)) {
            self.ignore_return_value = false;
        }
        node.visit_mut_children_with(self);
        self.ignore_return_value = ignore_return_value;

        if let Expr::Call(CallExpr {
            callee: Callee::Super(..),
            ..
        }) = node
        {
            self.injected = true;

            let super_call = node.take();
            let exprs = self.exprs.clone();

            let exprs = iter::once(Box::new(super_call)).chain(exprs);

            *node = if ignore_return_value {
                SeqExpr {
                    span: DUMMY_SP,
                    exprs: exprs.collect(),
                }
                .into()
            } else {
                let array = ArrayLit {
                    span: DUMMY_SP,
                    elems: exprs.map(ExprOrSpread::from).map(Some).collect(),
                };

                MemberExpr {
                    span: DUMMY_SP,
                    obj: array.into(),
                    prop: ComputedPropName {
                        span: DUMMY_SP,
                        expr: 0.into(),
                    }
                    .into(),
                }
                .into()
            }
        }
    }
}
