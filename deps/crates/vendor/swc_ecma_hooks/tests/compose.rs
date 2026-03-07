use swc_atoms::atom;
use swc_common::{Span, DUMMY_SP};
use swc_ecma_ast::*;
use swc_ecma_hooks::{CompositeHook, VisitMutHook, VisitMutWithHook};
use swc_ecma_visit::VisitMutWith;

#[derive(Debug, Default)]
struct Hook {
    name: String,
    depth: usize,
    span_count: usize,
}

impl<C> VisitMutHook<C> for Hook {
    fn enter_span(&mut self, _span: &mut Span, _ctx: &mut C) {
        println!(
            "enter_span: name={}, depth={}, count={}",
            self.name, self.depth, self.span_count
        );
        self.depth += 1;
        self.span_count += 1;
    }

    fn exit_span(&mut self, _span: &mut Span, _ctx: &mut C) {
        self.depth -= 1;
        println!(
            "exit_span: name={}, depth={}, count={}",
            self.name, self.depth, self.span_count
        );
    }

    fn enter_expr(&mut self, _expr: &mut Expr, _ctx: &mut C) {
        println!(
            "enter_expr: name={}, depth={}, count={}",
            self.name, self.depth, self.span_count
        );
        self.depth += 1;
    }

    fn exit_expr(&mut self, _expr: &mut Expr, _ctx: &mut C) {
        self.depth -= 1;
        println!(
            "exit_expr: name={}, depth={}, count={}",
            self.name, self.depth, self.span_count
        );
    }
}

#[test]
fn compose_visit() {
    let hook1 = Hook {
        name: "hook1".to_string(),
        ..Default::default()
    };
    let hook2 = Hook {
        name: "hook2".to_string(),
        ..Default::default()
    };

    let hook = CompositeHook {
        first: hook1,
        second: hook2,
    };

    let mut node = Expr::Call(CallExpr {
        span: DUMMY_SP,
        callee: Callee::Expr(
            AwaitExpr {
                span: DUMMY_SP,
                arg: Ident::new_no_ctxt(atom!("foo"), DUMMY_SP).into(),
            }
            .into(),
        ),
        ..Default::default()
    });

    let mut visitor = VisitMutWithHook { hook, context: () };

    node.visit_mut_with(&mut visitor);

    assert_eq!(visitor.hook.first.depth, 0);
    assert_eq!(visitor.hook.first.span_count, 3);
    assert_eq!(visitor.hook.second.depth, 0);
    assert_eq!(visitor.hook.second.span_count, 3);
}
