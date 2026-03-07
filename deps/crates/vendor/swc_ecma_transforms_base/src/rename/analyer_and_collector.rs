use std::hash::Hash;

use rustc_hash::FxHashSet;
use swc_atoms::Atom;
use swc_common::{Mark, SyntaxContext};
use swc_ecma_ast::*;
use swc_ecma_utils::{ident::IdentLike, stack_size::maybe_grow_default};
use swc_ecma_visit::{noop_visit_type, Visit, VisitWith};

use super::{analyzer::scope::Scope, Analyzer};

struct IdCollector {
    ids: FxHashSet<Id>,
    stopped: bool,
}

impl IdCollector {
    fn handle_ident(&mut self, n: &Ident) {
        if !self.stopped && n.ctxt != SyntaxContext::empty() {
            self.ids.insert(n.to_id());
        }
    }
}

struct CustomBindingCollector<I>
where
    I: IdentLike + Eq + Hash + Send + Sync,
{
    bindings: FxHashSet<I>,
    preserved: FxHashSet<I>,
    is_pat_decl: bool,

    /// [None] if there's no `eval`.
    pub top_level_for_eval: Option<SyntaxContext>,
}

impl<I> CustomBindingCollector<I>
where
    I: IdentLike + Eq + Hash + Send + Sync,
{
    fn add(&mut self, i: &Ident) {
        if let Some(top_level_ctxt) = self.top_level_for_eval {
            if i.ctxt == top_level_ctxt {
                self.preserved.insert(I::from_ident(i));
                return;
            }
        }

        self.bindings.insert(I::from_ident(i));
    }

    fn handle_assign_pat_prop(&mut self, node: &AssignPatProp) {
        if self.is_pat_decl {
            self.add(&Ident::from(&node.key));
        }
    }

    fn handle_binding_ident(&mut self, node: &BindingIdent) {
        if self.is_pat_decl {
            self.add(&Ident::from(node));
        }
    }

    fn handle_class_expr(&mut self, node: &ClassExpr) {
        if let Some(id) = &node.ident {
            self.add(id);
        }
    }

    fn handle_fn_expr(&mut self, node: &FnExpr) {
        if let Some(id) = &node.ident {
            self.add(id);
        }
    }
}

pub(super) struct AnalyzerAndCollector {
    analyzer: Analyzer,
    id_collector: IdCollector,
    decl_collector: CustomBindingCollector<Id>,
}

impl Visit for AnalyzerAndCollector {
    noop_visit_type!();

    fn visit_arrow_expr(&mut self, node: &ArrowExpr) {
        let old_decl_collector_is_pat_decl = self.decl_collector.is_pat_decl;
        self.decl_collector.is_pat_decl = true;

        let old_analyzer = self.analyzer.enter_fn_scope();
        let old_analyzer_is_pat_decl = self.analyzer.is_pat_decl;
        self.analyzer.is_pat_decl = true;

        node.params.visit_with(self);

        // FIXME: Tests failing after this change - needs investigation
        // self.decl_collector.is_pat_decl = false;
        self.analyzer.is_pat_decl = false;

        match node.body.as_ref() {
            BlockStmtOrExpr::BlockStmt(n) => n.visit_children_with(self),
            BlockStmtOrExpr::Expr(n) => n.visit_with(self),
            #[cfg(swc_ast_unknown)]
            _ => (),
        }

        self.analyzer.is_pat_decl = old_analyzer_is_pat_decl;
        self.analyzer.exit_scope(old_analyzer);

        self.decl_collector.is_pat_decl = old_decl_collector_is_pat_decl;
    }

    fn visit_assign_target(&mut self, node: &AssignTarget) {
        let old_analyzer_is_pat_decl = self.analyzer.is_pat_decl;
        self.analyzer.is_pat_decl = false;

        node.visit_children_with(self);

        self.analyzer.is_pat_decl = old_analyzer_is_pat_decl;
    }

    fn visit_assign_pat_prop(&mut self, node: &AssignPatProp) {
        node.visit_children_with(self);

        self.decl_collector.handle_assign_pat_prop(node);
    }

    fn visit_binding_ident(&mut self, node: &BindingIdent) {
        node.visit_children_with(self);

        self.decl_collector.handle_binding_ident(node);
        self.analyzer.handle_binding_ident(node);
    }

    fn visit_block_stmt(&mut self, node: &BlockStmt) {
        let old_analyzer = self.analyzer.enter_block_scope();

        node.visit_children_with(self);

        self.analyzer.exit_scope(old_analyzer);
    }

    fn visit_bin_expr(&mut self, node: &BinExpr) {
        maybe_grow_default(|| node.visit_children_with(self));
    }

    fn visit_catch_clause(&mut self, node: &CatchClause) {
        let old_decl_collector_is_pat_decl = self.decl_collector.is_pat_decl;

        let old_analyzer = self.analyzer.enter_block_scope();
        let old_analyzer_is_pat_decl = self.analyzer.is_pat_decl;
        let old_analyzer_in_catch_params = self.analyzer.in_catch_params;

        self.decl_collector.is_pat_decl = false;
        self.analyzer.in_catch_params = false;
        self.analyzer.is_pat_decl = false;

        node.body.visit_children_with(self);

        self.decl_collector.is_pat_decl = true;
        self.analyzer.is_pat_decl = true;
        self.analyzer.in_catch_params = true;

        node.param.visit_with(self);

        self.decl_collector.is_pat_decl = old_decl_collector_is_pat_decl;

        self.analyzer.in_catch_params = old_analyzer_in_catch_params;
        self.analyzer.is_pat_decl = old_analyzer_is_pat_decl;
        self.analyzer.exit_scope(old_analyzer);
    }

    fn visit_class_decl(&mut self, node: &ClassDecl) {
        self.analyzer.handle_class_decl(node);

        node.visit_children_with(self);

        self.decl_collector.add(&node.ident);
    }

    fn visit_class_expr(&mut self, node: &ClassExpr) {
        let old_analyzer = self.analyzer.enter_block_scope();

        self.analyzer.handle_class_expr(node);

        node.visit_children_with(self);

        self.decl_collector.handle_class_expr(node);

        self.analyzer.exit_scope(old_analyzer);
    }

    fn visit_class_method(&mut self, node: &ClassMethod) {
        node.key.visit_with(self);

        let old_analyzer = self.analyzer.enter_fn_scope();

        node.function.decorators.visit_with(self);
        node.function.params.visit_with(self);
        if let Some(body) = &node.function.body {
            body.visit_children_with(self);
        }

        self.analyzer.exit_scope(old_analyzer);
    }

    fn visit_constructor(&mut self, node: &Constructor) {
        let old_analyzer = self.analyzer.enter_fn_scope();
        node.key.visit_with(self);
        node.params.visit_with(self);
        if let Some(body) = &node.body {
            body.visit_children_with(self);
        }

        self.analyzer.exit_scope(old_analyzer);
    }

    fn visit_default_decl(&mut self, node: &DefaultDecl) {
        match node {
            DefaultDecl::Class(c) => {
                self.analyzer.handle_class_expr(c);
                self.decl_collector.handle_class_expr(c);

                let old_analyzer = self.analyzer.enter_fn_scope();
                c.visit_children_with(self);
                self.analyzer.exit_scope(old_analyzer);
            }
            DefaultDecl::Fn(f) => {
                self.analyzer.handle_fn_expr(f);

                f.visit_with(self);
            }
            DefaultDecl::TsInterfaceDecl(_) => {}
            #[cfg(swc_ast_unknown)]
            _ => {}
        }
    }

    fn visit_export_named_specifier(&mut self, n: &ExportNamedSpecifier) {
        let old_id_collector_stopped = self.id_collector.stopped;
        self.id_collector.stopped = true;

        self.analyzer.handle_export_named_specifier(n);

        n.visit_children_with(self);

        self.id_collector.stopped = old_id_collector_stopped;
    }

    fn visit_expr(&mut self, node: &Expr) {
        let old_decl_collector_is_pat_decl = self.decl_collector.is_pat_decl;
        self.decl_collector.is_pat_decl = false;

        let old_analyzer_is_pat_decl = self.analyzer.is_pat_decl;
        self.analyzer.is_pat_decl = false;

        self.analyzer.handle_expr(node);

        node.visit_children_with(self);

        self.analyzer.is_pat_decl = old_analyzer_is_pat_decl;
        self.decl_collector.is_pat_decl = old_decl_collector_is_pat_decl;
    }

    fn visit_fn_decl(&mut self, node: &FnDecl) {
        // https://github.com/swc-project/swc/issues/6819
        //
        // We need to check for assign pattern because safari has a bug.
        // https://github.com/swc-project/swc/issues/9015
        //
        // Safari also has a bug with duplicate parameter names in destructuring.
        // https://github.com/swc-project/swc/issues/11083
        //
        // Note: The destructuring pattern check (is_object/is_array) only applies
        // in mangle mode to prevent the minifier from creating collisions. In normal
        // hygiene mode, existing collisions in source code should be preserved.
        let has_rest = node.function.params.iter().any(|p| {
            p.pat.is_rest()
                || p.pat.is_assign()
                || (self.analyzer.mangle && (p.pat.is_object() || p.pat.is_array()))
        });
        let need_skip_analyzer_record =
            self.analyzer.is_first_node && self.analyzer.skip_first_fn_or_class_decl;
        self.analyzer.is_first_node = false;

        if !need_skip_analyzer_record {
            self.analyzer.add_decl(node.ident.to_id(), true);

            if has_rest {
                self.analyzer.add_usage(node.ident.to_id());
            }
        }

        node.ident.visit_with(self);

        let old_analyzer = self.analyzer.enter_fn_scope();

        if !need_skip_analyzer_record && has_rest {
            // self.analyer is different from above because we are in a function scope
            self.analyzer.add_usage(node.ident.to_id());
        }

        node.function.decorators.visit_with(self);
        node.function.params.visit_with(self);
        if let Some(body) = &node.function.body {
            body.visit_children_with(self);
        }

        self.analyzer.exit_scope(old_analyzer);
        self.decl_collector.add(&node.ident);
    }

    fn visit_fn_expr(&mut self, node: &FnExpr) {
        if let Some(id) = &node.ident {
            let old_analyzer0 = self.analyzer.enter_fn_scope();
            self.analyzer.add_decl(id.to_id(), true);
            let old_analyzer1 = self.analyzer.enter_fn_scope();
            // https://github.com/swc-project/swc/issues/6819
            //
            // We need to check for assign pattern because safari has a bug.
            // https://github.com/swc-project/swc/issues/9015
            //
            // Safari also has a bug with duplicate parameter names in destructuring.
            // https://github.com/swc-project/swc/issues/11083
            //
            // Note: The destructuring pattern check (is_object/is_array) only applies
            // in mangle mode to prevent the minifier from creating collisions. In normal
            // hygiene mode, existing collisions in source code should be preserved.
            if node.function.params.iter().any(|p| {
                p.pat.is_rest()
                    || p.pat.is_assign()
                    || (self.analyzer.mangle && (p.pat.is_object() || p.pat.is_array()))
            }) {
                self.analyzer.add_usage(id.to_id());
            }
            node.function.decorators.visit_with(self);
            node.function.params.visit_with(self);
            if let Some(body) = &node.function.body {
                body.visit_children_with(self);
            }
            self.analyzer.exit_scope(old_analyzer1);
            self.analyzer.exit_scope(old_analyzer0);
        } else {
            node.visit_children_with(self)
        }

        self.decl_collector.handle_fn_expr(node);
    }

    fn visit_for_in_stmt(&mut self, node: &ForInStmt) {
        let old_analyzer0 = self.analyzer.enter_block_scope();
        node.left.visit_with(self);
        node.right.visit_with(self);
        let old_analyzer1 = self.analyzer.enter_block_scope();
        match node.body.as_ref() {
            Stmt::Block(n) => n.visit_children_with(self),
            _ => node.body.visit_with(self),
        }
        self.analyzer.exit_scope(old_analyzer1);
        self.analyzer.exit_scope(old_analyzer0);
    }

    fn visit_for_of_stmt(&mut self, node: &ForOfStmt) {
        let old_analyzer0 = self.analyzer.enter_block_scope();
        node.left.visit_with(self);
        node.right.visit_with(self);
        let old_analyzer1 = self.analyzer.enter_block_scope();
        match node.body.as_ref() {
            Stmt::Block(n) => n.visit_children_with(self),
            _ => node.body.visit_with(self),
        }
        self.analyzer.exit_scope(old_analyzer1);
        self.analyzer.exit_scope(old_analyzer0);
    }

    fn visit_for_stmt(&mut self, node: &ForStmt) {
        let old_analyzer0 = self.analyzer.enter_block_scope();
        node.init.visit_with(self);
        node.test.visit_with(self);
        node.update.visit_with(self);
        let old_analyzer1 = self.analyzer.enter_block_scope();
        match node.body.as_ref() {
            Stmt::Block(n) => n.visit_children_with(self),
            _ => node.body.visit_with(self),
        }
        self.analyzer.exit_scope(old_analyzer1);
        self.analyzer.exit_scope(old_analyzer0);
    }

    fn visit_function(&mut self, node: &Function) {
        let old_analyzer = self.analyzer.enter_fn_scope();

        node.decorators.visit_with(self);
        node.params.visit_with(self);
        if let Some(body) = &node.body {
            body.visit_children_with(self);
        }

        self.analyzer.exit_scope(old_analyzer);
    }

    fn visit_import_default_specifier(&mut self, node: &ImportDefaultSpecifier) {
        node.visit_children_with(self);

        self.analyzer.add_decl(node.local.to_id(), true);
        self.decl_collector.add(&node.local);
    }

    fn visit_import_named_specifier(&mut self, node: &ImportNamedSpecifier) {
        node.visit_children_with(self);

        self.analyzer.add_decl(node.local.to_id(), true);
        self.decl_collector.add(&node.local);
    }

    fn visit_import_star_as_specifier(&mut self, node: &ImportStarAsSpecifier) {
        node.visit_children_with(self);

        self.analyzer.add_decl(node.local.to_id(), true);
        self.decl_collector.add(&node.local);
    }

    fn visit_param(&mut self, node: &Param) {
        let old_decl_collector_is_pat_decl = self.decl_collector.is_pat_decl;
        self.decl_collector.is_pat_decl = true;

        let old_analyzer_is_pat_decl = self.analyzer.is_pat_decl;
        let old_analyzer_need_hoist = self.analyzer.var_belong_to_fn_scope;

        // Params belong to function scope.
        // Params in catch clause belong to block scope
        self.analyzer.var_belong_to_fn_scope = !self.analyzer.in_catch_params;
        self.analyzer.is_pat_decl = false;
        node.decorators.visit_with(self);

        self.analyzer.is_pat_decl = true;
        node.pat.visit_with(self);

        self.analyzer.var_belong_to_fn_scope = old_analyzer_need_hoist;
        self.analyzer.is_pat_decl = old_analyzer_is_pat_decl;
        self.decl_collector.is_pat_decl = old_decl_collector_is_pat_decl;
    }

    fn visit_prop(&mut self, node: &Prop) {
        node.visit_children_with(self);
        self.analyzer.handle_prop(node);
    }

    fn visit_static_block(&mut self, node: &StaticBlock) {
        let old_analyzer = self.analyzer.enter_fn_scope();

        node.body.visit_children_with(self);

        self.analyzer.exit_scope(old_analyzer);
    }

    fn visit_stmt(&mut self, node: &Stmt) {
        self.analyzer.is_first_node = false;
        node.visit_children_with(self);
    }

    fn visit_ts_param_prop(&mut self, p: &TsParamProp) {
        let old_decl_collector_is_pat_decl = self.decl_collector.is_pat_decl;
        self.decl_collector.is_pat_decl = true;

        p.visit_children_with(self);

        self.decl_collector.is_pat_decl = old_decl_collector_is_pat_decl;
    }

    fn visit_var_decl(&mut self, node: &VarDecl) {
        let old_analyzer_need_hoisted = self.analyzer.var_belong_to_fn_scope;
        self.analyzer.var_belong_to_fn_scope = node.kind == VarDeclKind::Var;

        node.visit_children_with(self);

        self.analyzer.var_belong_to_fn_scope = old_analyzer_need_hoisted;
    }

    fn visit_var_declarator(&mut self, node: &VarDeclarator) {
        let old_decl_collector_is_pat_decl = self.decl_collector.is_pat_decl;
        self.decl_collector.is_pat_decl = true;

        let old_analyzer_is_pat_decl = self.analyzer.is_pat_decl;
        self.analyzer.is_pat_decl = true;

        node.name.visit_with(self);

        self.decl_collector.is_pat_decl = false;
        self.analyzer.is_pat_decl = false;

        node.init.visit_with(self);

        self.analyzer.is_pat_decl = old_analyzer_is_pat_decl;
        self.decl_collector.is_pat_decl = old_decl_collector_is_pat_decl;
    }

    fn visit_super_prop(&mut self, node: &SuperProp) {
        let old_id_collector_stopped = self.id_collector.stopped;
        if node.is_ident() {
            self.id_collector.stopped = true;
        }

        node.visit_children_with(self);

        if node.is_ident() {
            self.id_collector.stopped = old_id_collector_stopped;
        }
    }

    fn visit_export_default_specifier(&mut self, n: &ExportDefaultSpecifier) {
        let old_id_collector_stopped = self.id_collector.stopped;
        self.id_collector.stopped = true;

        n.visit_children_with(self);

        self.id_collector.stopped = old_id_collector_stopped;
    }

    fn visit_export_namespace_specifier(&mut self, n: &ExportNamespaceSpecifier) {
        let old_id_collector_stopped = self.id_collector.stopped;
        self.id_collector.stopped = true;

        n.visit_children_with(self);

        self.id_collector.stopped = old_id_collector_stopped;
    }

    fn visit_ident(&mut self, id: &Ident) {
        self.id_collector.handle_ident(id);
    }

    fn visit_named_export(&mut self, node: &NamedExport) {
        let old_id_collector_stopped = self.id_collector.stopped;
        if node.src.is_some() {
            self.id_collector.stopped = true;
        }

        node.visit_children_with(self);

        if node.src.is_some() {
            self.id_collector.stopped = old_id_collector_stopped;
        }
    }
}

pub(super) fn analyzer_and_collect_unresolved<N>(
    n: &N,
    has_eval: bool,
    top_level_mark: Mark,
    skip_first_fn_or_class_decl: bool,
    mangle: bool,
) -> (Scope, FxHashSet<Atom>)
where
    N: VisitWith<AnalyzerAndCollector>,
{
    let analyzer = Analyzer::new(
        has_eval,
        top_level_mark,
        skip_first_fn_or_class_decl,
        mangle,
    );
    let id_collector = IdCollector {
        ids: Default::default(),
        stopped: false,
    };
    let decl_collector = CustomBindingCollector {
        bindings: Default::default(),
        preserved: Default::default(),
        is_pat_decl: false,
        top_level_for_eval: has_eval
            .then_some(top_level_mark)
            .map(|m| SyntaxContext::empty().apply_mark(m)),
    };

    let mut v = AnalyzerAndCollector {
        analyzer,
        id_collector,
        decl_collector,
    };

    n.visit_with(&mut v);

    let (usages, decls, preserved) = (
        v.id_collector.ids,
        v.decl_collector.bindings,
        v.decl_collector.preserved,
    );

    let unresolved = usages
        .into_iter()
        .filter(|used_id| !decls.contains(used_id))
        .map(|v| v.0)
        .chain(preserved.into_iter().map(|v| v.0))
        .collect::<FxHashSet<_>>();

    (v.analyzer.scope, unresolved)
}
