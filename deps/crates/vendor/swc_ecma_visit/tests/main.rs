use swc_atoms::atom;
use swc_common::DUMMY_SP;
use swc_ecma_ast::*;
use swc_ecma_visit::NodeRef;

#[test]
fn traverse_lookup() {
    let node = Expr::Call(CallExpr {
        span: DUMMY_SP,
        callee: Callee::Expr(
            AwaitExpr {
                span: DUMMY_SP,
                arg: Ident::new_no_ctxt(atom!("foo"), DUMMY_SP).into(),
            }
            .into(),
        ),
        args: Vec::new(),
        ..Default::default()
    });

    let node_ref = NodeRef::from(&node);
    let iter = node_ref.experimental_traverse();

    let mut has_await = false;

    for node in iter {
        has_await |= matches!(node, NodeRef::AwaitExpr(..));
    }

    assert!(has_await);
}
