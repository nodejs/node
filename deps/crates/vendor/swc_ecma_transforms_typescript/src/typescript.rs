use std::mem;

use rustc_hash::FxHashSet;
use swc_atoms::atom;
use swc_common::{comments::Comments, sync::Lrc, util::take::Take, Mark, SourceMap, Span, Spanned};
use swc_ecma_ast::*;
use swc_ecma_transforms_react::{parse_expr_for_jsx, JsxDirectives};
use swc_ecma_visit::{visit_mut_pass, VisitMut, VisitMutWith};

pub use crate::config::*;
use crate::{strip_import_export::StripImportExport, strip_type::StripType, transform::transform};

macro_rules! static_str {
    ($s:expr) => {
        $s.into()
    };
}

pub fn typescript(config: Config, unresolved_mark: Mark, top_level_mark: Mark) -> impl Pass {
    debug_assert_ne!(unresolved_mark, top_level_mark);

    TypeScript {
        config,
        unresolved_mark,
        top_level_mark,
        id_usage: Default::default(),
    }
}

pub fn strip(unresolved_mark: Mark, top_level_mark: Mark) -> impl Pass {
    typescript(Config::default(), unresolved_mark, top_level_mark)
}

pub(crate) struct TypeScript {
    pub config: Config,
    pub unresolved_mark: Mark,
    pub top_level_mark: Mark,

    id_usage: FxHashSet<Id>,
}

impl Pass for TypeScript {
    fn process(&mut self, n: &mut Program) {
        let was_module = n.as_module().and_then(|m| self.get_last_module_span(m));

        if !self.config.verbatim_module_syntax {
            n.visit_mut_with(&mut StripImportExport {
                import_not_used_as_values: self.config.import_not_used_as_values,
                usage_info: mem::take(&mut self.id_usage).into(),
                ..Default::default()
            });
        }

        n.visit_mut_with(&mut StripType::default());

        n.mutate(transform(
            self.unresolved_mark,
            self.top_level_mark,
            self.config.import_export_assign_config,
            self.config.ts_enum_is_mutable,
            self.config.verbatim_module_syntax,
            self.config.native_class_properties,
        ));

        if let Some(span) = was_module {
            let module = n.as_mut_module().unwrap();
            Self::restore_esm_ctx(module, span);
        }
    }
}

impl TypeScript {
    fn get_last_module_span(&self, n: &Module) -> Option<Span> {
        if self.config.no_empty_export {
            return None;
        }

        n.body
            .iter()
            .rev()
            .find(|m| m.is_es_module_decl())
            .map(Spanned::span)
    }

    fn restore_esm_ctx(n: &mut Module, span: Span) {
        if n.body.iter().any(ModuleItem::is_es_module_decl) {
            return;
        }

        n.body.push(
            NamedExport {
                span,
                ..NamedExport::dummy()
            }
            .into(),
        );
    }
}

trait EsModuleDecl {
    fn is_es_module_decl(&self) -> bool;
}

impl EsModuleDecl for ModuleDecl {
    fn is_es_module_decl(&self) -> bool {
        // Do not use `matches!`
        // We should cover all cases explicitly.
        match self {
            ModuleDecl::Import(..)
            | ModuleDecl::ExportDecl(..)
            | ModuleDecl::ExportNamed(..)
            | ModuleDecl::ExportDefaultDecl(..)
            | ModuleDecl::ExportDefaultExpr(..)
            | ModuleDecl::ExportAll(..) => true,

            ModuleDecl::TsImportEquals(..)
            | ModuleDecl::TsExportAssignment(..)
            | ModuleDecl::TsNamespaceExport(..) => false,
            #[cfg(swc_ast_unknown)]
            _ => panic!("unable to access unknown nodes"),
        }
    }
}

impl EsModuleDecl for ModuleItem {
    fn is_es_module_decl(&self) -> bool {
        self.as_module_decl()
            .is_some_and(ModuleDecl::is_es_module_decl)
    }
}

pub fn tsx<C>(
    cm: Lrc<SourceMap>,
    config: Config,
    tsx_config: TsxConfig,
    comments: C,
    unresolved_mark: Mark,
    top_level_mark: Mark,
) -> impl Pass
where
    C: Comments,
{
    visit_mut_pass(TypeScriptReact {
        config,
        tsx_config,
        id_usage: Default::default(),
        comments,
        cm,
        top_level_mark,
        unresolved_mark,
    })
}

/// Get an [Id] which will used by expression.
///
/// For `React#1.createElement`, this returns `React#1`.
fn id_for_jsx(e: &Expr) -> Option<Id> {
    match e {
        Expr::Ident(i) => Some(i.to_id()),
        Expr::Member(MemberExpr { obj, .. }) => Some(id_for_jsx(obj)).flatten(),
        Expr::Lit(Lit::Null(..)) => Some((atom!("null"), Default::default())),
        _ => None,
    }
}

struct TypeScriptReact<C>
where
    C: Comments,
{
    config: Config,
    tsx_config: TsxConfig,
    id_usage: FxHashSet<Id>,
    comments: C,
    cm: Lrc<SourceMap>,
    top_level_mark: Mark,
    unresolved_mark: Mark,
}

impl<C> VisitMut for TypeScriptReact<C>
where
    C: Comments,
{
    fn visit_mut_module(&mut self, n: &mut Module) {
        // We count `React` or pragma from config as ident usage and do not strip it
        // from import statement.
        // But in `verbatim_module_syntax` mode, we do not remove any unused imports.
        // So we do not need to collect usage info.
        if !self.config.verbatim_module_syntax {
            let pragma = parse_expr_for_jsx(
                &self.cm,
                "pragma",
                self.tsx_config
                    .pragma
                    .clone()
                    .unwrap_or_else(|| static_str!("React.createElement")),
                self.top_level_mark,
            );

            let pragma_frag = parse_expr_for_jsx(
                &self.cm,
                "pragma",
                self.tsx_config
                    .pragma_frag
                    .clone()
                    .unwrap_or_else(|| static_str!("React.Fragment")),
                self.top_level_mark,
            );

            let pragma_id = id_for_jsx(&pragma).unwrap();
            let pragma_frag_id = id_for_jsx(&pragma_frag).unwrap();

            self.id_usage.insert(pragma_id);
            self.id_usage.insert(pragma_frag_id);
        }

        if !self.config.verbatim_module_syntax {
            let span = if n.shebang.is_some() {
                n.span
                    .with_lo(n.body.first().map(|s| s.span_lo()).unwrap_or(n.span.lo))
            } else {
                n.span
            };

            let JsxDirectives {
                pragma,
                pragma_frag,
                ..
            } = self.comments.with_leading(span.lo, |comments| {
                JsxDirectives::from_comments(&self.cm, span, comments, self.top_level_mark)
            });

            if let Some(pragma) = pragma {
                if let Some(pragma_id) = id_for_jsx(&pragma) {
                    self.id_usage.insert(pragma_id);
                }
            }

            if let Some(pragma_frag) = pragma_frag {
                if let Some(pragma_frag_id) = id_for_jsx(&pragma_frag) {
                    self.id_usage.insert(pragma_frag_id);
                }
            }
        }
    }

    fn visit_mut_script(&mut self, _: &mut Script) {
        // skip script
    }

    fn visit_mut_program(&mut self, n: &mut Program) {
        n.visit_mut_children_with(self);

        n.mutate(&mut TypeScript {
            config: mem::take(&mut self.config),
            unresolved_mark: self.unresolved_mark,
            top_level_mark: self.top_level_mark,
            id_usage: mem::take(&mut self.id_usage),
        });
    }
}
