use rustc_hash::FxHashMap;
use swc_common::{
    util::{move_map::MoveMap, take::Take},
    Spanned, SyntaxContext, DUMMY_SP,
};
use swc_ecma_ast::*;
use swc_ecma_utils::stack_size::maybe_grow_default;
use swc_ecma_visit::{noop_visit_mut_type, VisitMut, VisitMutWith};

use crate::{
    hygiene::Config,
    perf::{cpu_count, ParExplode, Parallel, ParallelExt},
    rename::RenamedVariable,
};

pub(super) struct Operator<'a, V>
where
    V: RenamedVariable,
{
    pub rename: &'a FxHashMap<Id, V>,
    pub config: Config,

    pub extra: Vec<ModuleItem>,
}

impl<V> Operator<'_, V>
where
    V: RenamedVariable,
{
    fn keep_class_name(&mut self, ident: &mut Ident, class: &mut Class) -> Option<ClassExpr> {
        if !self.config.keep_class_names {
            return None;
        }

        let mut orig_name = ident.clone();
        orig_name.ctxt = SyntaxContext::empty();

        {
            // Remove span hygiene of the class.
            let mut rename = FxHashMap::<Id, V>::default();

            rename.insert(ident.to_id(), V::new_private(ident.sym.clone()));

            let mut operator = Operator {
                rename: &rename,
                config: self.config.clone(),
                extra: Default::default(),
            };

            class.visit_mut_with(&mut operator);
        }

        let _ = self.rename_ident(ident);
        class.visit_mut_with(self);

        let class_expr = ClassExpr {
            ident: Some(orig_name),
            class: Box::new(class.take()),
        };

        Some(class_expr)
    }
}

impl<V> Parallel for Operator<'_, V>
where
    V: RenamedVariable,
{
    fn create(&self) -> Self {
        Self {
            rename: self.rename,
            config: self.config.clone(),
            extra: Default::default(),
        }
    }

    fn merge(&mut self, other: Self) {
        self.extra.extend(other.extra);
    }

    fn after_module_items(&mut self, stmts: &mut Vec<ModuleItem>) {
        stmts.append(&mut self.extra);
    }
}

impl<V> ParExplode for Operator<'_, V>
where
    V: RenamedVariable,
{
    fn after_one_stmt(&mut self, _: &mut Vec<Stmt>) {}

    fn after_one_module_item(&mut self, stmts: &mut Vec<ModuleItem>) {
        stmts.append(&mut self.extra);
    }
}

impl<V> VisitMut for Operator<'_, V>
where
    V: RenamedVariable,
{
    noop_visit_mut_type!();

    /// Preserve key of properties.
    fn visit_mut_assign_pat_prop(&mut self, p: &mut AssignPatProp) {
        if let Some(value) = &mut p.value {
            value.visit_mut_children_with(self);
        }
    }

    fn visit_mut_class_expr(&mut self, n: &mut ClassExpr) {
        if let Some(ident) = &mut n.ident {
            if let Some(expr) = self.keep_class_name(ident, &mut n.class) {
                *n = expr;
                return;
            }
        }

        n.ident.visit_mut_with(self);

        n.class.visit_mut_with(self);
    }

    fn visit_mut_decl(&mut self, decl: &mut Decl) {
        match decl {
            Decl::Class(cls) if self.config.keep_class_names => {
                let span = cls.class.span;

                let expr = self.keep_class_name(&mut cls.ident, &mut cls.class);
                if let Some(expr) = expr {
                    let var = VarDeclarator {
                        span,
                        name: cls.ident.clone().into(),
                        init: Some(expr.into()),
                        definite: false,
                    };
                    *decl = VarDecl {
                        span,
                        kind: VarDeclKind::Let,
                        decls: vec![var],
                        ..Default::default()
                    }
                    .into();
                    return;
                }

                return;
            }
            _ => {}
        }

        decl.visit_mut_children_with(self);
    }

    fn visit_mut_export_named_specifier(&mut self, s: &mut ExportNamedSpecifier) {
        if s.exported.is_some() {
            s.orig.visit_mut_with(self);
            return;
        }

        let exported = s.orig.clone();

        if let ModuleExportName::Ident(orig) = &mut s.orig {
            if self.rename_ident(orig).is_ok() {
                match &exported {
                    ModuleExportName::Ident(exported) => {
                        if orig.sym == exported.sym {
                            return;
                        }
                    }
                    ModuleExportName::Str(_) => {}
                    #[cfg(swc_ast_unknown)]
                    _ => {}
                }

                s.exported = Some(exported);
            }
        }
    }

    fn visit_mut_expr(&mut self, n: &mut Expr) {
        maybe_grow_default(|| n.visit_mut_children_with(self))
    }

    fn visit_mut_expr_or_spreads(&mut self, n: &mut Vec<ExprOrSpread>) {
        self.maybe_par(cpu_count() * 100, n, |v, n| {
            n.visit_mut_with(v);
        })
    }

    fn visit_mut_exprs(&mut self, n: &mut Vec<Box<Expr>>) {
        self.maybe_par(cpu_count() * 100, n, |v, n| {
            n.visit_mut_with(v);
        })
    }

    fn visit_mut_ident(&mut self, ident: &mut Ident) {
        match self.rename_ident(ident) {
            Ok(i) | Err(i) => i,
        }
    }

    fn visit_mut_import_named_specifier(&mut self, s: &mut ImportNamedSpecifier) {
        if s.imported.is_some() {
            s.local.visit_mut_with(self);
            return;
        }

        let imported = s.local.clone();
        let local = self.rename_ident(&mut s.local);

        if local.is_ok() {
            if s.local.sym == imported.sym {
                return;
            }

            s.imported = Some(ModuleExportName::Ident(imported));
        }
    }

    /// Preserve key of properties.
    fn visit_mut_key_value_pat_prop(&mut self, p: &mut KeyValuePatProp) {
        p.key.visit_mut_with(self);
        p.value.visit_mut_with(self);
    }

    fn visit_mut_key_value_prop(&mut self, p: &mut KeyValueProp) {
        p.key.visit_mut_with(self);
        p.value.visit_mut_with(self);
    }

    fn visit_mut_member_expr(&mut self, expr: &mut MemberExpr) {
        expr.span.visit_mut_with(self);
        expr.obj.visit_mut_with(self);

        if let MemberProp::Computed(c) = &mut expr.prop {
            c.visit_mut_with(self)
        }
    }

    fn visit_mut_module_item(&mut self, item: &mut ModuleItem) {
        let span = item.span();

        macro_rules! export {
            ($orig:expr, $ident:expr) => {
                self.extra
                    .push(ModuleItem::ModuleDecl(ModuleDecl::ExportNamed(
                        NamedExport {
                            span,
                            specifiers: vec![ExportSpecifier::Named(ExportNamedSpecifier {
                                span: DUMMY_SP,
                                orig: $ident,
                                exported: Some($orig),
                                is_type_only: false,
                            })],
                            src: None,
                            type_only: false,
                            with: None,
                        },
                    )));
            };
        }

        match item {
            ModuleItem::ModuleDecl(ModuleDecl::ExportDecl(ExportDecl {
                span,
                decl:
                    Decl::Class(ClassDecl {
                        ident,
                        class,
                        declare,
                    }),
            })) => {
                let mut ident = ident.take();
                let mut class = class.take();

                class.visit_mut_with(self);
                let orig_ident = ident.clone();
                match self.rename_ident(&mut ident) {
                    Ok(..) => {
                        *item = ClassDecl {
                            ident: ident.clone(),
                            class: class.take(),
                            declare: *declare,
                        }
                        .into();
                        export!(
                            ModuleExportName::Ident(orig_ident),
                            ModuleExportName::Ident(ident.take())
                        );
                    }
                    Err(..) => {
                        *item = ExportDecl {
                            span: *span,
                            decl: ClassDecl {
                                ident: ident.take(),
                                class: class.take(),
                                declare: *declare,
                            }
                            .into(),
                        }
                        .into()
                    }
                }
            }
            ModuleItem::ModuleDecl(ModuleDecl::ExportDecl(ExportDecl {
                span,
                decl:
                    Decl::Fn(FnDecl {
                        ident,
                        function,
                        declare,
                    }),
            })) => {
                let mut ident = ident.take();
                let mut function = function.take();

                function.visit_mut_with(self);
                let orig_ident = ident.clone();
                match self.rename_ident(&mut ident) {
                    Ok(..) => {
                        *item = FnDecl {
                            ident: ident.clone(),
                            function,
                            declare: *declare,
                        }
                        .into();
                        export!(
                            ModuleExportName::Ident(orig_ident),
                            ModuleExportName::Ident(ident)
                        );
                    }
                    Err(..) => {
                        *item = ExportDecl {
                            span: *span,
                            decl: FnDecl {
                                ident,
                                function,
                                declare: *declare,
                            }
                            .into(),
                        }
                        .into()
                    }
                }
            }
            ModuleItem::ModuleDecl(ModuleDecl::ExportDecl(ExportDecl {
                decl: Decl::Var(var),
                ..
            })) => {
                let decls = var.decls.take();

                let mut renamed: Vec<ExportSpecifier> = Vec::new();
                let decls = decls.move_map(|mut decl| {
                    decl.name.visit_mut_with(&mut VarFolder {
                        orig: self,
                        renamed: &mut renamed,
                    });
                    decl.init.visit_mut_with(self);
                    decl
                });

                if renamed.is_empty() {
                    *item = ExportDecl {
                        span,
                        decl: VarDecl {
                            decls,
                            ..*var.take()
                        }
                        .into(),
                    }
                    .into();
                    return;
                }
                *item = VarDecl {
                    decls,
                    ..*var.take()
                }
                .into();
                self.extra.push(
                    NamedExport {
                        span,
                        specifiers: renamed,
                        src: None,
                        type_only: false,
                        with: None,
                    }
                    .into(),
                );
            }
            _ => {
                item.visit_mut_children_with(self);
            }
        }
    }

    fn visit_mut_module_items(&mut self, nodes: &mut Vec<ModuleItem>) {
        use std::mem::take;

        #[cfg(feature = "concurrent")]
        if nodes.len() >= 8 * cpu_count() {
            ::swc_common::GLOBALS.with(|globals| {
                use par_iter::prelude::*;

                let (visitor, new_nodes) = take(nodes)
                    .into_par_iter()
                    .map(|mut node| {
                        ::swc_common::GLOBALS.set(globals, || {
                            let mut visitor = Parallel::create(&*self);
                            node.visit_mut_with(&mut visitor);

                            let mut nodes = Vec::with_capacity(4);

                            ParExplode::after_one_module_item(&mut visitor, &mut nodes);

                            nodes.push(node);

                            (visitor, nodes)
                        })
                    })
                    .reduce(
                        || (Parallel::create(&*self), Vec::new()),
                        |mut a, b| {
                            Parallel::merge(&mut a.0, b.0);

                            a.1.extend(b.1);

                            a
                        },
                    );

                Parallel::merge(self, visitor);

                {
                    self.after_module_items(nodes);
                }

                *nodes = new_nodes;
            });

            return;
        }

        let mut buf = Vec::with_capacity(nodes.len());

        for mut node in take(nodes) {
            let mut visitor = Parallel::create(&*self);
            node.visit_mut_with(&mut visitor);
            buf.push(node);
            visitor.after_one_module_item(&mut buf);
        }

        self.after_module_items(&mut buf);

        *nodes = buf;
    }

    fn visit_mut_named_export(&mut self, e: &mut NamedExport) {
        if e.src.is_some() {
            return;
        }

        e.visit_mut_children_with(self);
    }

    fn visit_mut_object_pat_prop(&mut self, n: &mut ObjectPatProp) {
        n.visit_mut_children_with(self);

        if let ObjectPatProp::Assign(p) = n {
            let mut renamed = Ident::from(&p.key);
            if self.rename_ident(&mut renamed).is_ok() {
                if renamed.sym == p.key.sym {
                    return;
                }

                *n = KeyValuePatProp {
                    key: PropName::Ident(p.key.take().into()),
                    value: match p.value.take() {
                        Some(default_expr) => AssignPat {
                            span: p.span,
                            left: renamed.into(),
                            right: default_expr,
                        }
                        .into(),
                        None => renamed.into(),
                    },
                }
                .into();
            }
        }
    }

    fn visit_mut_opt_vec_expr_or_spreads(&mut self, n: &mut Vec<Option<ExprOrSpread>>) {
        self.maybe_par(cpu_count() * 100, n, |v, n| {
            n.visit_mut_with(v);
        })
    }

    fn visit_mut_prop(&mut self, prop: &mut Prop) {
        match prop {
            Prop::Shorthand(i) => {
                let mut renamed = i.clone();
                if self.rename_ident(&mut renamed).is_ok() {
                    if renamed.sym == i.sym {
                        return;
                    }

                    *prop = Prop::KeyValue(KeyValueProp {
                        key: PropName::Ident(IdentName {
                            // clear mark
                            span: i.span,
                            sym: i.sym.clone(),
                        }),
                        value: renamed.into(),
                    })
                }
            }
            _ => prop.visit_mut_children_with(self),
        }
    }

    fn visit_mut_prop_name(&mut self, n: &mut PropName) {
        if let PropName::Computed(c) = n {
            c.visit_mut_with(self)
        }
    }

    fn visit_mut_class_members(&mut self, members: &mut Vec<ClassMember>) {
        self.maybe_par(cpu_count(), members, |v, member| {
            member.visit_mut_with(v);
        });
    }

    fn visit_mut_prop_or_spreads(&mut self, n: &mut Vec<PropOrSpread>) {
        self.maybe_par(cpu_count() * 100, n, |v, n| {
            n.visit_mut_with(v);
        })
    }

    fn visit_mut_stmts(&mut self, nodes: &mut Vec<Stmt>) {
        use std::mem::take;

        #[cfg(feature = "concurrent")]
        if nodes.len() >= 100 * cpu_count() {
            ::swc_common::GLOBALS.with(|globals| {
                use par_iter::prelude::*;

                let (visitor, new_nodes) = take(nodes)
                    .into_par_iter()
                    .map(|mut node| {
                        ::swc_common::GLOBALS.set(globals, || {
                            let mut visitor = Parallel::create(&*self);
                            node.visit_mut_with(&mut visitor);

                            let mut nodes = Vec::with_capacity(4);

                            ParExplode::after_one_stmt(&mut visitor, &mut nodes);

                            nodes.push(node);

                            (visitor, nodes)
                        })
                    })
                    .reduce(
                        || (Parallel::create(&*self), Vec::new()),
                        |mut a, b| {
                            Parallel::merge(&mut a.0, b.0);

                            a.1.extend(b.1);

                            a
                        },
                    );

                Parallel::merge(self, visitor);

                {
                    self.after_stmts(nodes);
                }

                *nodes = new_nodes;
            });

            return;
        }

        let mut buf = Vec::with_capacity(nodes.len());

        for mut node in take(nodes) {
            let mut visitor = Parallel::create(&*self);
            node.visit_mut_with(&mut visitor);
            buf.push(node);
            visitor.after_one_stmt(&mut buf);
        }

        self.after_stmts(&mut buf);

        *nodes = buf;
    }

    fn visit_mut_super_prop_expr(&mut self, expr: &mut SuperPropExpr) {
        expr.span.visit_mut_with(self);
        if let SuperProp::Computed(c) = &mut expr.prop {
            c.visit_mut_with(self);
        }
    }

    fn visit_mut_var_declarators(&mut self, n: &mut Vec<VarDeclarator>) {
        self.maybe_par(cpu_count() * 100, n, |v, n| {
            n.visit_mut_with(v);
        })
    }
}

struct VarFolder<'a, 'b, V>
where
    V: RenamedVariable,
{
    orig: &'a mut Operator<'b, V>,
    renamed: &'a mut Vec<ExportSpecifier>,
}

impl<V> VisitMut for VarFolder<'_, '_, V>
where
    V: RenamedVariable,
{
    noop_visit_mut_type!();

    #[inline]
    fn visit_mut_expr(&mut self, _: &mut Expr) {}

    #[inline]
    fn visit_mut_simple_assign_target(&mut self, _: &mut SimpleAssignTarget) {}

    fn visit_mut_ident(&mut self, i: &mut Ident) {
        let orig = i.clone();
        if self.orig.rename_ident(i).is_ok() {
            self.renamed
                .push(ExportSpecifier::Named(ExportNamedSpecifier {
                    span: i.span,
                    exported: Some(ModuleExportName::Ident(orig)),
                    orig: ModuleExportName::Ident(i.clone()),
                    is_type_only: false,
                }));
        }
    }
}

impl<V> Operator<'_, V>
where
    V: RenamedVariable,
{
    /// Returns `Ok(renamed_ident)` if ident should be renamed.
    fn rename_ident(&mut self, ident: &mut Ident) -> Result<(), ()> {
        if let Some(new_id) = self.rename.get(&ident.to_id()) {
            let new_sym = new_id.atom();

            if ident.sym.eq(new_sym) {
                return Err(());
            }

            ident.ctxt = new_id.ctxt();
            ident.sym = new_sym.clone();
            return Ok(());
        }

        Err(())
    }
}
