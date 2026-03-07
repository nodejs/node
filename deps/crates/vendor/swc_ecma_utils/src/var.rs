use swc_ecma_ast::*;
use swc_ecma_visit::{noop_visit_type, Visit, VisitWith};

use crate::ident::IdentLike;

/// This collects variables bindings while ignoring if it's nested in
/// expression.
pub struct VarCollector<'a, I: IdentLike> {
    pub to: &'a mut Vec<I>,
}

impl<I: IdentLike> Visit for VarCollector<'_, I> {
    noop_visit_type!(fail);

    fn visit_arrow_expr(&mut self, _: &ArrowExpr) {}

    fn visit_constructor(&mut self, _: &Constructor) {}

    fn visit_expr(&mut self, _: &Expr) {}

    fn visit_function(&mut self, _: &Function) {}

    fn visit_key_value_pat_prop(&mut self, node: &KeyValuePatProp) {
        node.value.visit_with(self);
    }

    fn visit_ident(&mut self, i: &Ident) {
        self.to.push(I::from_ident(i))
    }

    fn visit_var_declarator(&mut self, node: &VarDeclarator) {
        node.name.visit_with(self);
    }
}
