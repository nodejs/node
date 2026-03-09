use std::mem;

use swc_common::util::take::Take;
use swc_ecma_ast::*;
use swc_ecma_utils::stack_size::maybe_grow_default;
use swc_ecma_visit::{noop_visit_mut_type, VisitMut, VisitMutWith};

use crate::type_to_none;

pub fn strip_type() -> impl VisitMut {
    StripType::default()
}

/// This Module will strip all types/generics/interface/declares
/// and type import/export.
///
/// `export declare var` in a namespace will be retained.
#[derive(Default)]
pub(crate) struct StripType {
    in_namespace: bool,
}

impl VisitMut for StripType {
    noop_visit_mut_type!(fail);

    type_to_none!(visit_mut_opt_ts_type, Box<TsType>);

    type_to_none!(visit_mut_opt_ts_type_ann, Box<TsTypeAnn>);

    type_to_none!(visit_mut_opt_ts_type_param_decl, Box<TsTypeParamDecl>);

    type_to_none!(
        visit_mut_opt_ts_type_param_instantiation,
        Box<TsTypeParamInstantiation>
    );

    fn visit_mut_array_pat(&mut self, n: &mut ArrayPat) {
        n.visit_mut_children_with(self);
        n.optional = false;
    }

    fn visit_mut_auto_accessor(&mut self, n: &mut AutoAccessor) {
        n.type_ann = None;
        n.accessibility = None;
        n.definite = false;
        n.is_override = false;
        n.is_abstract = false;
        n.visit_mut_children_with(self);
    }

    fn visit_mut_class(&mut self, n: &mut Class) {
        n.is_abstract = false;
        n.implements.clear();
        n.visit_mut_children_with(self);
    }

    fn visit_mut_class_members(&mut self, n: &mut Vec<ClassMember>) {
        n.retain(|member| match member {
            ClassMember::TsIndexSignature(..) => false,
            ClassMember::Constructor(Constructor { body: None, .. }) => false,

            ClassMember::Method(ClassMethod {
                is_abstract,
                function,
                ..
            })
            | ClassMember::PrivateMethod(PrivateMethod {
                is_abstract,
                function,
                ..
            }) => !is_abstract && function.body.is_some(),

            ClassMember::ClassProp(
                ClassProp { declare: true, .. }
                | ClassProp {
                    is_abstract: true, ..
                },
            )
            | ClassMember::AutoAccessor(AutoAccessor {
                is_abstract: true, ..
            }) => false,

            _ => true,
        });

        n.visit_mut_children_with(self);
    }

    fn visit_mut_class_method(&mut self, n: &mut ClassMethod) {
        n.accessibility = None;
        n.is_override = false;
        n.is_abstract = false;
        n.is_optional = false;
        n.visit_mut_children_with(self);
    }

    fn visit_mut_class_prop(&mut self, prop: &mut ClassProp) {
        prop.declare = false;
        prop.readonly = false;
        prop.is_override = false;
        prop.is_optional = false;
        prop.is_abstract = false;
        prop.definite = false;
        prop.accessibility = None;
        prop.visit_mut_children_with(self);
    }

    fn visit_mut_private_method(&mut self, n: &mut PrivateMethod) {
        n.accessibility = None;
        n.is_abstract = false;
        n.is_optional = false;
        n.is_override = false;
        n.visit_mut_children_with(self);
    }

    fn visit_mut_constructor(&mut self, n: &mut Constructor) {
        n.accessibility = None;
        n.visit_mut_children_with(self);
    }

    fn visit_mut_export_specifiers(&mut self, n: &mut Vec<ExportSpecifier>) {
        n.retain(|s| match s {
            ExportSpecifier::Named(ExportNamedSpecifier { is_type_only, .. }) => !is_type_only,
            _ => true,
        })
    }

    fn visit_mut_expr(&mut self, n: &mut Expr) {
        // https://github.com/tc39/proposal-type-annotations#type-assertions
        // https://github.com/tc39/proposal-type-annotations#non-nullable-assertions
        while let Expr::TsAs(TsAsExpr { expr, .. })
        | Expr::TsNonNull(TsNonNullExpr { expr, .. })
        | Expr::TsTypeAssertion(TsTypeAssertion { expr, .. })
        | Expr::TsConstAssertion(TsConstAssertion { expr, .. })
        | Expr::TsInstantiation(TsInstantiation { expr, .. })
        | Expr::TsSatisfies(TsSatisfiesExpr { expr, .. }) = n
        {
            *n = *expr.take();
        }

        maybe_grow_default(|| n.visit_mut_children_with(self));
    }

    // https://github.com/tc39/proposal-type-annotations#parameter-optionality
    fn visit_mut_ident(&mut self, n: &mut Ident) {
        n.optional = false;
    }

    fn visit_mut_import_specifiers(&mut self, n: &mut Vec<ImportSpecifier>) {
        n.retain(|s| !matches!(s, ImportSpecifier::Named(named) if named.is_type_only));
    }

    fn visit_mut_ts_module_block(&mut self, node: &mut TsModuleBlock) {
        let in_namespace = mem::replace(&mut self.in_namespace, true);
        node.visit_mut_children_with(self);
        self.in_namespace = in_namespace;
    }

    fn visit_mut_module_items(&mut self, n: &mut Vec<ModuleItem>) {
        n.retain(|item| should_retain_module_item(item, self.in_namespace));
        n.visit_mut_children_with(self);
    }

    fn visit_mut_object_pat(&mut self, pat: &mut ObjectPat) {
        pat.visit_mut_children_with(self);
        pat.optional = false;
    }

    // https://github.com/tc39/proposal-type-annotations#this-parameters
    fn visit_mut_params(&mut self, n: &mut Vec<Param>) {
        if n.first()
            .filter(|param| {
                matches!(
                    &param.pat,
                    Pat::Ident(BindingIdent {
                        id: Ident { sym, .. },
                        ..
                    }) if &**sym == "this"
                )
            })
            .is_some()
        {
            n.drain(0..1);
        }

        n.visit_mut_children_with(self);
    }

    fn visit_mut_private_prop(&mut self, prop: &mut PrivateProp) {
        prop.readonly = false;
        prop.is_override = false;
        prop.is_optional = false;
        prop.definite = false;
        prop.accessibility = None;
        prop.visit_mut_children_with(self);
    }

    fn visit_mut_setter_prop(&mut self, n: &mut SetterProp) {
        n.this_param = None;

        n.visit_mut_children_with(self);
    }

    fn visit_mut_simple_assign_target(&mut self, n: &mut SimpleAssignTarget) {
        // https://github.com/tc39/proposal-type-annotations#type-assertions
        // https://github.com/tc39/proposal-type-annotations#non-nullable-assertions
        while let SimpleAssignTarget::TsAs(TsAsExpr { expr, .. })
        | SimpleAssignTarget::TsNonNull(TsNonNullExpr { expr, .. })
        | SimpleAssignTarget::TsTypeAssertion(TsTypeAssertion { expr, .. })
        | SimpleAssignTarget::TsInstantiation(TsInstantiation { expr, .. })
        | SimpleAssignTarget::TsSatisfies(TsSatisfiesExpr { expr, .. }) = n
        {
            *n = expr.take().try_into().unwrap();
        }

        n.visit_mut_children_with(self);
    }

    fn visit_mut_stmts(&mut self, n: &mut Vec<Stmt>) {
        n.visit_mut_children_with(self);
        n.retain(|s| !matches!(s, Stmt::Empty(e) if e.span.is_dummy()));
    }

    fn visit_mut_stmt(&mut self, n: &mut Stmt) {
        if should_retain_stmt(n) {
            n.visit_mut_children_with(self);
        } else if !n.is_empty() {
            n.take();
        }
    }

    fn visit_mut_ts_import_equals_decl(&mut self, _: &mut TsImportEqualsDecl) {
        // n.id.visit_mut_with(self);
    }

    fn visit_mut_ts_param_prop(&mut self, n: &mut TsParamProp) {
        // skip accessibility
        n.decorators.visit_mut_with(self);
        n.param.visit_mut_with(self);
    }
}

fn should_retain_module_item(module_item: &ModuleItem, in_namespace: bool) -> bool {
    match module_item {
        ModuleItem::ModuleDecl(ModuleDecl::ExportDecl(export_decl)) => {
            // https://github.com/swc-project/swc/issues/9821
            // `export declare var` in namespace should be retained
            // This will help the following transforms to work correctly
            if in_namespace && export_decl.decl.is_var() {
                return true;
            }

            should_retain_decl(&export_decl.decl)
        }
        ModuleItem::Stmt(stmt) => should_retain_stmt(stmt),
        _ => module_item.is_concrete(),
    }
}

fn should_retain_stmt(stmt: &Stmt) -> bool {
    match stmt {
        Stmt::Decl(decl) => should_retain_decl(decl),
        _ => stmt.is_concrete(),
    }
}

fn should_retain_decl(decl: &Decl) -> bool {
    if decl.is_declare() {
        return false;
    }

    decl.is_concrete()
}

pub(crate) trait IsConcrete {
    fn is_concrete(&self) -> bool;
}

impl IsConcrete for TsModuleDecl {
    fn is_concrete(&self) -> bool {
        self.body
            .as_ref()
            .map(|body| body.is_concrete())
            .unwrap_or_default()
    }
}

impl IsConcrete for TsNamespaceBody {
    fn is_concrete(&self) -> bool {
        match self {
            Self::TsModuleBlock(ts_module_block) => {
                ts_module_block.body.iter().any(|item| item.is_concrete())
            }
            Self::TsNamespaceDecl(ts_namespace_decl) => ts_namespace_decl.body.is_concrete(),
            #[cfg(swc_ast_unknown)]
            _ => panic!("unable to access unknown nodes"),
        }
    }
}

impl IsConcrete for ModuleItem {
    fn is_concrete(&self) -> bool {
        match self {
            Self::ModuleDecl(module_decl) => module_decl.is_concrete(),
            Self::Stmt(stmt) => stmt.is_concrete(),
            #[cfg(swc_ast_unknown)]
            _ => panic!("unable to access unknown nodes"),
        }
    }
}

impl IsConcrete for ModuleDecl {
    fn is_concrete(&self) -> bool {
        match self {
            Self::Import(import_decl) => !import_decl.type_only,
            Self::ExportDecl(export_decl) => export_decl.decl.is_concrete(),
            Self::ExportNamed(named_export) => !named_export.type_only,
            Self::ExportDefaultDecl(export_default_decl) => export_default_decl.decl.is_concrete(),
            Self::ExportDefaultExpr(..) => true,
            Self::ExportAll(export_all) => !export_all.type_only,
            Self::TsImportEquals(ts_import_equals) => !ts_import_equals.is_type_only,
            Self::TsExportAssignment(..) => true,
            Self::TsNamespaceExport(..) => false,
            #[cfg(swc_ast_unknown)]
            _ => panic!("unable to access unknown nodes"),
        }
    }
}

impl IsConcrete for Decl {
    fn is_concrete(&self) -> bool {
        match self {
            Self::TsInterface(..) | Self::TsTypeAlias(..) => false,
            Self::Fn(r#fn) => r#fn.function.body.is_some(),
            Self::Class(..) | Self::Var(..) | Self::Using(..) | Self::TsEnum(..) => true,
            Self::TsModule(ts_module) => ts_module.is_concrete(),
            #[cfg(swc_ast_unknown)]
            _ => panic!("unable to access unknown nodes"),
        }
    }
}

impl IsConcrete for DefaultDecl {
    fn is_concrete(&self) -> bool {
        match self {
            Self::Class(_) => true,
            Self::Fn(r#fn) => r#fn.function.body.is_some(),
            Self::TsInterfaceDecl(..) => false,
            #[cfg(swc_ast_unknown)]
            _ => panic!("unable to access unknown nodes"),
        }
    }
}

impl IsConcrete for Stmt {
    fn is_concrete(&self) -> bool {
        match self {
            Self::Empty(..) => false,
            Self::Decl(decl) => decl.is_concrete(),
            _ => true,
        }
    }
}

trait IsDeclare {
    fn is_declare(&self) -> bool;
}

impl IsDeclare for Decl {
    fn is_declare(&self) -> bool {
        match self {
            Decl::Class(class) => class.declare,
            Decl::Fn(r#fn) => r#fn.declare,
            Decl::Var(var) => var.declare,
            Decl::Using(_) => false,
            // NOTE: TsInterface/TsTypeAlias is not relevant
            Decl::TsInterface(_) | Decl::TsTypeAlias(_) => true,
            Decl::TsEnum(ts_enum) => ts_enum.declare,
            Decl::TsModule(ts_module) => ts_module.declare || ts_module.global,
            #[cfg(swc_ast_unknown)]
            _ => panic!("unable to access unknown nodes"),
        }
    }
}
