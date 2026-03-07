use swc_common::DUMMY_SP;
use swc_ecma_ast::*;
use swc_ecma_utils::ExprFactory;

///
/// this.prop = value
pub(crate) fn assign_value_to_this_prop(prop_name: PropName, value: Expr) -> Box<Expr> {
    let target = MemberExpr {
        obj: ThisExpr { span: DUMMY_SP }.into(),
        span: DUMMY_SP,
        prop: prop_name.into(),
    };

    let expr = value.make_assign_to(op!("="), target.into());

    Box::new(expr)
}

/// this.#prop = value
pub(crate) fn assign_value_to_this_private_prop(
    private_name: PrivateName,
    value: Expr,
) -> Box<Expr> {
    let target = MemberExpr {
        obj: ThisExpr { span: DUMMY_SP }.into(),
        span: DUMMY_SP,
        prop: MemberProp::PrivateName(private_name),
    };

    let expr = value.make_assign_to(op!("="), target.into());

    Box::new(expr)
}

pub(crate) struct Factory;

impl Factory {
    pub(crate) fn function(params: Vec<Param>, body: BlockStmt) -> Function {
        Function {
            params,
            decorators: Default::default(),
            span: DUMMY_SP,
            body: Some(body),
            is_generator: false,
            is_async: false,
            ..Default::default()
        }
    }
}
