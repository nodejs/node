use swc_ecma_ast::*;
use swc_ecma_utils::stack_size::maybe_grow_default;
use swc_ecma_visit::{noop_visit_type, visit_obj_and_computed, Visit, VisitWith};

pub fn contains_eval<N>(node: &N, include_with: bool) -> bool
where
    N: VisitWith<EvalFinder>,
{
    let mut v = EvalFinder {
        found: false,
        include_with,
    };

    node.visit_with(&mut v);
    v.found
}

pub struct EvalFinder {
    found: bool,
    include_with: bool,
}

impl Visit for EvalFinder {
    noop_visit_type!();

    visit_obj_and_computed!();

    fn visit_callee(&mut self, c: &Callee) {
        if c.as_expr().is_some_and(|e| e.is_ident_ref_to("eval")) {
            self.found = true;
        } else {
            c.visit_children_with(self);
        }
    }

    fn visit_export_default_specifier(&mut self, _: &ExportDefaultSpecifier) {}

    fn visit_export_named_specifier(&mut self, _: &ExportNamedSpecifier) {}

    fn visit_export_namespace_specifier(&mut self, _: &ExportNamespaceSpecifier) {}

    fn visit_expr(&mut self, n: &Expr) {
        if self.found {
            return;
        }
        maybe_grow_default(|| n.visit_children_with(self));
    }

    fn visit_stmt(&mut self, n: &Stmt) {
        if self.found {
            return;
        }
        n.visit_children_with(self)
    }

    fn visit_named_export(&mut self, e: &NamedExport) {
        if e.src.is_some() {
            return;
        }

        e.visit_children_with(self);
    }

    fn visit_prop_name(&mut self, p: &PropName) {
        if let PropName::Computed(n) = p {
            n.visit_with(self);
        }
    }

    fn visit_with_stmt(&mut self, s: &WithStmt) {
        if self.include_with {
            self.found = true;
        } else {
            s.visit_children_with(self);
        }
    }
}
