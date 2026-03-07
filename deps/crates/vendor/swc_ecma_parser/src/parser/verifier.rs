use swc_common::{Span, Spanned};
use swc_ecma_ast::{AssignProp, Expr};
use swc_ecma_visit::{noop_visit_type, Visit, VisitWith};

use crate::error::SyntaxError;

pub struct Verifier {
    pub errors: Vec<(Span, SyntaxError)>,
}

impl Visit for Verifier {
    noop_visit_type!();

    fn visit_assign_prop(&mut self, p: &AssignProp) {
        self.errors.push((p.span(), SyntaxError::AssignProperty));
    }

    fn visit_expr(&mut self, e: &Expr) {
        match *e {
            Expr::Fn(..) | Expr::Arrow(..) => {}
            _ => e.visit_children_with(self),
        }
    }
}
