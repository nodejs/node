use swc_common::Mark;
use swc_ecma_ast::*;

use self::scope::{Scope, ScopeKind};

mod reverse_map;
pub(super) mod scope;

#[derive(Debug, Default)]
pub(super) struct Analyzer {
    /// If `eval` exists for the current scope, we only rename synthesized
    /// identifiers.
    pub(super) has_eval: bool,
    /// The [Mark] which is parent of user-specified identifiers.
    pub(super) top_level_mark: Mark,

    pub(super) is_pat_decl: bool,
    pub(super) var_belong_to_fn_scope: bool,
    pub(super) in_catch_params: bool,
    pub(super) scope: Scope,
    /// If we try add variables declared by `var` to the block scope,
    /// variables will be added to `hoisted_vars` and merged to latest
    /// function scope in the end.
    pub(super) hoisted_vars: Vec<Id>,

    pub(super) skip_first_fn_or_class_decl: bool,
    pub(super) is_first_node: bool,

    /// Whether we're in mangle mode. Some Safari bug workarounds only apply
    /// during mangling, not during normal hygiene.
    pub(super) mangle: bool,
}

impl Analyzer {
    pub(super) fn new(
        has_eval: bool,
        top_level_mark: Mark,
        skip_first_fn_or_class_decl: bool,
        mangle: bool,
    ) -> Self {
        Self {
            has_eval,
            top_level_mark,
            skip_first_fn_or_class_decl,
            is_first_node: true,
            hoisted_vars: Vec::with_capacity(32),
            mangle,
            ..Default::default()
        }
    }

    pub(super) fn add_decl(&mut self, id: Id, belong_to_fn_scope: bool) {
        if belong_to_fn_scope {
            match self.scope.kind {
                ScopeKind::Fn => {
                    self.scope.add_decl(&id, self.has_eval, self.top_level_mark);
                }
                ScopeKind::Block => self.hoisted_vars.push(id),
            }
        } else {
            self.scope.add_decl(&id, self.has_eval, self.top_level_mark);
        }
    }

    fn reserve_decl(&mut self, len: usize, belong_to_fn_scope: bool) {
        if belong_to_fn_scope {
            match self.scope.kind {
                ScopeKind::Fn => {
                    self.scope.reserve_decl(len);
                }
                ScopeKind::Block => {
                    self.hoisted_vars.reserve(len);
                }
            }
        } else {
            self.scope.reserve_decl(len);
        }
    }

    pub(super) fn add_usage(&mut self, id: Id) {
        self.scope.add_usage(id);
    }

    fn reserve_usage(&mut self, len: usize) {
        self.scope.reserve_usage(len);
    }

    fn enter_scope(&mut self, kind: ScopeKind) -> Self {
        let mut analyer = Analyzer {
            has_eval: self.has_eval,
            top_level_mark: self.top_level_mark,
            is_pat_decl: self.is_pat_decl,
            var_belong_to_fn_scope: false,
            in_catch_params: false,
            scope: Scope {
                kind,
                ..Default::default()
            },
            skip_first_fn_or_class_decl: false,
            is_first_node: false,
            hoisted_vars: Default::default(),
            mangle: self.mangle,
        };
        std::mem::swap(self, &mut analyer);
        analyer // old analyzer
    }

    pub(super) fn enter_fn_scope(&mut self) -> Self {
        self.enter_scope(ScopeKind::Fn)
    }

    pub(super) fn enter_block_scope(&mut self) -> Self {
        self.enter_scope(ScopeKind::Block)
    }

    pub(super) fn exit_scope(&mut self, mut v: Self) {
        std::mem::swap(self, &mut v);
        if !v.hoisted_vars.is_empty() {
            debug_assert!(matches!(v.scope.kind, ScopeKind::Block));
            self.reserve_usage(v.hoisted_vars.len());
            v.hoisted_vars.clone().into_iter().for_each(|id| {
                // For variables declared in block scope using `var` and `function`,
                // We should create a fake usage in the block to prevent conflicted
                // renaming.
                v.add_usage(id);
            });
            match self.scope.kind {
                ScopeKind::Fn => {
                    self.reserve_decl(v.hoisted_vars.len(), true);
                    v.hoisted_vars
                        .into_iter()
                        .for_each(|id| self.add_decl(id, true));
                }
                ScopeKind::Block => {
                    self.hoisted_vars.extend(v.hoisted_vars);
                }
            }
        }
        self.scope.children.push(v.scope);
    }

    pub(super) fn handle_binding_ident(&mut self, node: &BindingIdent) {
        if self.is_pat_decl {
            self.add_decl(node.to_id(), self.var_belong_to_fn_scope)
        } else {
            self.add_usage(node.to_id())
        }
    }

    pub(super) fn handle_class_decl(&mut self, node: &ClassDecl) {
        if self.is_first_node && self.skip_first_fn_or_class_decl {
            self.is_first_node = false;
            return;
        }
        self.is_first_node = false;
        self.add_decl(node.ident.to_id(), false);
    }

    pub(super) fn handle_class_expr(&mut self, node: &ClassExpr) {
        if let Some(id) = &node.ident {
            self.add_decl(id.to_id(), false);
        }
    }

    pub(super) fn handle_fn_expr(&mut self, node: &FnExpr) {
        if let Some(id) = &node.ident {
            self.add_decl(id.to_id(), true);
        }
    }

    pub(super) fn handle_export_named_specifier(&mut self, n: &ExportNamedSpecifier) {
        match &n.orig {
            ModuleExportName::Ident(orig) => {
                self.add_usage(orig.to_id());
            }
            ModuleExportName::Str(..) => {}
            #[cfg(swc_ast_unknown)]
            _ => {}
        };
    }

    pub(super) fn handle_expr(&mut self, e: &Expr) {
        if let Expr::Ident(i) = e {
            self.add_usage(i.to_id());
        }
    }

    pub(super) fn handle_prop(&mut self, node: &Prop) {
        if let Prop::Shorthand(i) = node {
            self.add_usage(i.to_id());
        }
    }
}
