use rustc_hash::{FxHashMap, FxHashSet};
use swc_ecma_ast::*;
use swc_ecma_utils::stack_size::maybe_grow_default;
use swc_ecma_visit::{noop_visit_type, Visit, VisitMut, VisitMutWith, VisitWith};

use crate::{strip_type::IsConcrete, ImportsNotUsedAsValues};

#[derive(Debug, Default)]
pub(crate) struct UsageCollect {
    id_usage: FxHashSet<Id>,
    import_chain: FxHashMap<Id, Id>,
}

impl From<FxHashSet<Id>> for UsageCollect {
    fn from(id_usage: FxHashSet<Id>) -> Self {
        Self {
            id_usage,
            import_chain: Default::default(),
        }
    }
}

impl Visit for UsageCollect {
    noop_visit_type!();

    fn visit_ident(&mut self, n: &Ident) {
        self.id_usage.insert(n.to_id());
    }

    fn visit_expr(&mut self, n: &Expr) {
        maybe_grow_default(|| n.visit_children_with(self))
    }

    fn visit_binding_ident(&mut self, _: &BindingIdent) {
        // skip
    }

    fn visit_fn_decl(&mut self, n: &FnDecl) {
        // skip function ident
        n.function.visit_with(self);
    }

    fn visit_fn_expr(&mut self, n: &FnExpr) {
        // skip function ident
        n.function.visit_with(self);
    }

    fn visit_class_decl(&mut self, n: &ClassDecl) {
        // skip class ident
        n.class.visit_with(self);
    }

    fn visit_class_expr(&mut self, n: &ClassExpr) {
        // skip class ident
        n.class.visit_with(self);
    }

    fn visit_import_decl(&mut self, _: &ImportDecl) {
        // skip
    }

    fn visit_ts_import_equals_decl(&mut self, n: &TsImportEqualsDecl) {
        if n.is_type_only {
            return;
        }

        // skip id visit

        let TsModuleRef::TsEntityName(ts_entity_name) = &n.module_ref else {
            return;
        };

        let id = get_module_ident(ts_entity_name);

        if n.is_export {
            id.visit_with(self);
            n.id.visit_with(self);
            return;
        }

        self.import_chain.insert(n.id.to_id(), id.to_id());
    }

    fn visit_export_named_specifier(&mut self, n: &ExportNamedSpecifier) {
        if n.is_type_only {
            return;
        }

        n.orig.visit_with(self);
    }

    fn visit_named_export(&mut self, n: &NamedExport) {
        if n.type_only || n.src.is_some() {
            return;
        }

        n.visit_children_with(self);
    }

    fn visit_jsx_element_name(&mut self, n: &JSXElementName) {
        if matches!(n, JSXElementName::Ident(i) if i.sym.starts_with(|c: char| c.is_ascii_lowercase()) )
        {
            return;
        }

        n.visit_children_with(self);
    }
}

impl UsageCollect {
    fn has_usage(&self, id: &Id) -> bool {
        self.id_usage.contains(id)
    }

    fn analyze_import_chain(&mut self) {
        if self.import_chain.is_empty() {
            return;
        }

        let mut new_usage = FxHashSet::default();
        for id in &self.id_usage {
            let mut next = self.import_chain.remove(id);
            while let Some(id) = next {
                next = self.import_chain.remove(&id);
                new_usage.insert(id);
            }
            if self.import_chain.is_empty() {
                break;
            }
        }
        self.import_chain.clear();
        self.id_usage.extend(new_usage);
    }
}

fn get_module_ident(ts_entity_name: &TsEntityName) -> &Ident {
    match ts_entity_name {
        TsEntityName::TsQualifiedName(ts_qualified_name) => {
            get_module_ident(&ts_qualified_name.left)
        }
        TsEntityName::Ident(ident) => ident,
        #[cfg(swc_ast_unknown)]
        _ => panic!("unable to access unknown nodes"),
    }
}

#[derive(Debug, Default)]
pub(crate) struct DeclareCollect {
    id_type: FxHashSet<Id>,
    id_value: FxHashSet<Id>,
}

/// Only scan the top level of the module.
/// Any inner declaration is ignored.
impl Visit for DeclareCollect {
    fn visit_binding_ident(&mut self, n: &BindingIdent) {
        self.id_value.insert(n.to_id());
    }

    fn visit_assign_pat_prop(&mut self, n: &AssignPatProp) {
        self.id_value.insert(n.key.to_id());
    }

    fn visit_var_declarator(&mut self, n: &VarDeclarator) {
        n.name.visit_with(self);
    }

    fn visit_fn_decl(&mut self, n: &FnDecl) {
        self.id_value.insert(n.ident.to_id());
    }

    fn visit_class_decl(&mut self, n: &ClassDecl) {
        self.id_value.insert(n.ident.to_id());
    }

    fn visit_ts_enum_decl(&mut self, n: &TsEnumDecl) {
        self.id_value.insert(n.id.to_id());
    }

    fn visit_export_default_decl(&mut self, n: &ExportDefaultDecl) {
        match &n.decl {
            DefaultDecl::Class(ClassExpr {
                ident: Some(ident), ..
            }) => {
                self.id_value.insert(ident.to_id());
            }
            DefaultDecl::Fn(FnExpr {
                ident: Some(ident), ..
            }) => {
                self.id_value.insert(ident.to_id());
            }
            _ => {}
        };
    }

    fn visit_ts_import_equals_decl(&mut self, n: &TsImportEqualsDecl) {
        if n.is_type_only {
            self.id_type.insert(n.id.to_id());
        } else {
            self.id_value.insert(n.id.to_id());
        }
    }

    fn visit_ts_module_decl(&mut self, n: &TsModuleDecl) {
        if n.global {
            return;
        }

        if let TsModuleName::Ident(ident) = &n.id {
            if n.is_concrete() {
                self.id_value.insert(ident.to_id());
            } else {
                self.id_type.insert(ident.to_id());
            }
        }
    }

    fn visit_ts_interface_decl(&mut self, n: &TsInterfaceDecl) {
        self.id_type.insert(n.id.to_id());
    }

    fn visit_ts_type_alias_decl(&mut self, n: &TsTypeAliasDecl) {
        self.id_type.insert(n.id.to_id());
    }

    fn visit_import_decl(&mut self, n: &ImportDecl) {
        n.specifiers
            .iter()
            .for_each(|import_specifier| match import_specifier {
                ImportSpecifier::Named(named) => {
                    if n.type_only || named.is_type_only {
                        self.id_type.insert(named.local.to_id());
                    }
                }
                ImportSpecifier::Default(default) => {
                    if n.type_only {
                        self.id_type.insert(default.local.to_id());
                    }
                }
                ImportSpecifier::Namespace(namespace) => {
                    if n.type_only {
                        self.id_type.insert(namespace.local.to_id());
                    }
                }
                #[cfg(swc_ast_unknown)]
                _ => (),
            });
    }

    fn visit_stmt(&mut self, n: &Stmt) {
        if !n.is_decl() {
            return;
        }

        n.visit_children_with(self);
    }

    fn visit_stmts(&mut self, _: &[Stmt]) {
        // skip
    }

    fn visit_block_stmt(&mut self, _: &BlockStmt) {
        // skip
    }
}

impl DeclareCollect {
    fn has_pure_type(&self, id: &Id) -> bool {
        self.id_type.contains(id) && !self.id_value.contains(id)
    }

    fn has_value(&self, id: &Id) -> bool {
        self.id_value.contains(id)
    }
}

/// https://www.typescriptlang.org/tsconfig#importsNotUsedAsValues
///
/// These steps match tsc behavior:
/// 1. Remove all import bindings that are not used.
/// 2. Remove all export bindings that are defined as types.
/// 3. If a local value declaration shares a name with an imported binding,
///    treat the imported binding as a type import.
/// 4. If a local type declaration shares a name with an imported binding and
///    it's used in a value position, treat the imported binding as a value
///    import.
#[derive(Default)]
pub(crate) struct StripImportExport {
    pub import_not_used_as_values: ImportsNotUsedAsValues,
    pub usage_info: UsageCollect,
    pub declare_info: DeclareCollect,
}

impl VisitMut for StripImportExport {
    fn visit_mut_module(&mut self, n: &mut Module) {
        n.visit_with(&mut self.usage_info);
        n.visit_with(&mut self.declare_info);

        self.usage_info.analyze_import_chain();

        n.visit_mut_children_with(self);
    }

    fn visit_mut_module_items(&mut self, n: &mut Vec<ModuleItem>) {
        let mut strip_ts_import_equals = StripTsImportEquals;

        n.retain_mut(|module_item| match module_item {
            ModuleItem::ModuleDecl(ModuleDecl::Import(ImportDecl {
                specifiers,
                type_only: false,
                ..
            })) if !specifiers.is_empty() => {
                // Note: If import specifiers is originally empty, then we leave it alone.
                // This is weird but it matches TS.

                specifiers.retain(|import_specifier| match import_specifier {
                    ImportSpecifier::Named(named) => {
                        if named.is_type_only {
                            return false;
                        }

                        let id = named.local.to_id();

                        if self.declare_info.has_value(&id) {
                            return false;
                        }

                        self.usage_info.has_usage(&id)
                    }
                    ImportSpecifier::Default(default) => {
                        let id = default.local.to_id();

                        if self.declare_info.has_value(&id) {
                            return false;
                        }

                        self.usage_info.has_usage(&id)
                    }
                    ImportSpecifier::Namespace(namespace) => {
                        let id = namespace.local.to_id();

                        if self.declare_info.has_value(&id) {
                            return false;
                        }

                        self.usage_info.has_usage(&id)
                    }
                    #[cfg(swc_ast_unknown)]
                    _ => panic!("unable to access unknown nodes"),
                });

                self.import_not_used_as_values == ImportsNotUsedAsValues::Preserve
                    || !specifiers.is_empty()
            }
            ModuleItem::ModuleDecl(ModuleDecl::ExportNamed(NamedExport {
                specifiers,
                src,
                type_only: false,
                ..
            })) => {
                specifiers.retain(|export_specifier| match export_specifier {
                    ExportSpecifier::Namespace(..) => true,
                    ExportSpecifier::Default(..) => true,

                    ExportSpecifier::Named(ExportNamedSpecifier {
                        orig: ModuleExportName::Ident(ident),
                        is_type_only: false,
                        ..
                    }) if src.is_none() => {
                        let id = ident.to_id();

                        !self.declare_info.has_pure_type(&id)
                    }
                    ExportSpecifier::Named(ExportNamedSpecifier { is_type_only, .. }) => {
                        !is_type_only
                    }
                    #[cfg(swc_ast_unknown)]
                    _ => panic!("unable to access unknown nodes"),
                });

                !specifiers.is_empty()
            }
            ModuleItem::ModuleDecl(ModuleDecl::ExportNamed(NamedExport {
                ref type_only, ..
            })) => !type_only,
            ModuleItem::ModuleDecl(ModuleDecl::ExportDefaultExpr(ExportDefaultExpr {
                ref expr,
                ..
            })) => expr
                .as_ident()
                .map(|ident| {
                    let id = ident.to_id();

                    !self.declare_info.has_pure_type(&id)
                })
                .unwrap_or(true),
            ModuleItem::ModuleDecl(ModuleDecl::TsImportEquals(ts_import_equals_decl)) => {
                if ts_import_equals_decl.is_type_only {
                    return false;
                }

                if ts_import_equals_decl.is_export {
                    return true;
                }

                self.usage_info.has_usage(&ts_import_equals_decl.id.to_id())
            }
            ModuleItem::Stmt(Stmt::Decl(Decl::TsModule(ref ts_module)))
                if ts_module.body.is_some() =>
            {
                module_item.visit_mut_with(&mut strip_ts_import_equals);

                true
            }
            _ => true,
        });
    }

    fn visit_mut_script(&mut self, n: &mut Script) {
        let mut visitor = StripTsImportEquals;
        for stmt in n.body.iter_mut() {
            if let Stmt::Decl(Decl::TsModule(..)) = stmt {
                stmt.visit_mut_with(&mut visitor);
            }
        }
    }
}

#[derive(Debug, Default)]
struct StripTsImportEquals;

impl VisitMut for StripTsImportEquals {
    fn visit_mut_module_items(&mut self, n: &mut Vec<ModuleItem>) {
        let has_ts_import_equals = n.iter().any(|module_item| {
            matches!(
                module_item,
                ModuleItem::ModuleDecl(ModuleDecl::TsImportEquals(..))
            )
        });

        // TS1235: A namespace declaration is only allowed at the top level of a
        // namespace or module.
        let has_ts_module = n.iter().any(|module_item| {
            matches!(
                module_item,
                ModuleItem::Stmt(Stmt::Decl(Decl::TsModule(..)))
            )
        });

        if has_ts_import_equals {
            let mut usage_info = UsageCollect::default();

            n.visit_with(&mut usage_info);

            n.retain(|module_item| match module_item {
                ModuleItem::ModuleDecl(ModuleDecl::TsImportEquals(ts_import_equals_decl)) => {
                    if ts_import_equals_decl.is_type_only {
                        return false;
                    }

                    if ts_import_equals_decl.is_export {
                        return true;
                    }

                    usage_info.has_usage(&ts_import_equals_decl.id.to_id())
                }
                _ => true,
            })
        }

        if has_ts_module {
            n.visit_mut_children_with(self);
        }
    }

    fn visit_mut_module_item(&mut self, n: &mut ModuleItem) {
        if let ModuleItem::Stmt(Stmt::Decl(Decl::TsModule(ref ts_module))) = n {
            if ts_module.body.is_some() {
                n.visit_mut_children_with(self)
            }
        }
    }
}
