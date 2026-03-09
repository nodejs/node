use rustc_hash::FxHashMap;
use swc_atoms::{atom, Atom, Wtf8Atom};
use swc_common::{comments::Comments, Span};
use swc_ecma_ast::*;
use swc_ecma_hooks::VisitMutHook;

#[cfg(test)]
mod tests;

/// A pass to add a /*#__PURE__#/ annotation to calls to known pure calls.
///
/// This pass adds a /*#__PURE__#/ annotation to calls to known pure top-level
/// React methods, so that terser and other minifiers can safely remove them
/// during dead code elimination.
/// See https://reactjs.org/docs/react-api.html
pub fn hook<C>(comments: Option<C>) -> impl VisitMutHook<()>
where
    C: Comments,
{
    PureAnnotations {
        imports: Default::default(),
        comments,
    }
}

struct PureAnnotations<C>
where
    C: Comments,
{
    imports: FxHashMap<Id, (Wtf8Atom, Atom)>,
    comments: Option<C>,
}

impl<C> VisitMutHook<()> for PureAnnotations<C>
where
    C: Comments,
{
    fn enter_module(&mut self, module: &mut Module, _ctx: &mut ()) {
        // Pass 1: collect imports
        for item in &module.body {
            if let ModuleItem::ModuleDecl(ModuleDecl::Import(import)) = item {
                let Some(src_str) = import.src.value.as_str() else {
                    continue;
                };
                if src_str != "react" && src_str != "react-dom" {
                    continue;
                }

                for specifier in &import.specifiers {
                    let src = import.src.value.clone();
                    match specifier {
                        ImportSpecifier::Named(named) => {
                            let imported: Atom = match &named.imported {
                                Some(ModuleExportName::Ident(imported)) => imported.sym.clone(),
                                Some(ModuleExportName::Str(s)) => {
                                    s.value.to_atom_lossy().into_owned()
                                }
                                None => named.local.sym.clone(),
                                #[cfg(swc_ast_unknown)]
                                Some(_) => continue,
                            };
                            self.imports.insert(named.local.to_id(), (src, imported));
                        }
                        ImportSpecifier::Default(default) => {
                            self.imports
                                .insert(default.local.to_id(), (src, atom!("default")));
                        }
                        ImportSpecifier::Namespace(ns) => {
                            self.imports.insert(ns.local.to_id(), (src, atom!("*")));
                        }
                        #[cfg(swc_ast_unknown)]
                        _ => (),
                    }
                }
            }
        }
    }

    fn enter_call_expr(&mut self, call: &mut CallExpr, _ctx: &mut ()) {
        if self.imports.is_empty() {
            return;
        }

        let is_react_call = match &call.callee {
            Callee::Expr(expr) => match &**expr {
                Expr::Ident(ident) => {
                    if let Some((src, specifier)) = self.imports.get(&ident.to_id()) {
                        is_pure(src, specifier)
                    } else {
                        false
                    }
                }
                Expr::Member(member) => match &*member.obj {
                    Expr::Ident(ident) => {
                        if let Some((src, specifier)) = self.imports.get(&ident.to_id()) {
                            if &**specifier == "default" || &**specifier == "*" {
                                match &member.prop {
                                    MemberProp::Ident(ident) => is_pure(src, &ident.sym),
                                    _ => false,
                                }
                            } else {
                                false
                            }
                        } else {
                            false
                        }
                    }
                    _ => false,
                },
                _ => false,
            },
            _ => false,
        };

        if is_react_call {
            if let Some(comments) = &self.comments {
                if call.span.lo.is_dummy() {
                    call.span.lo = Span::dummy_with_cmt().lo;
                }

                comments.add_pure_comment(call.span.lo);
            }
        }
    }
}

fn is_pure(src: &Wtf8Atom, specifier: &Atom) -> bool {
    let Some(src) = src.as_str() else {
        return false;
    };
    let specifier = specifier.as_str();

    match src {
        "react" => matches!(
            specifier,
            "cloneElement"
                | "createContext"
                | "createElement"
                | "createFactory"
                | "createRef"
                | "forwardRef"
                | "isValidElement"
                | "memo"
                | "lazy"
        ),
        "react-dom" => specifier == "createPortal",
        _ => false,
    }
}
