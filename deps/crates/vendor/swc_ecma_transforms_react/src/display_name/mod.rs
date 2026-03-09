use std::ops::DerefMut;

use swc_atoms::atom;
use swc_common::DUMMY_SP;
use swc_ecma_ast::*;
use swc_ecma_hooks::VisitMutHook;
use swc_ecma_visit::{noop_visit_mut_type, VisitMut, VisitMutWith};

#[cfg(test)]
mod tests;

/// `@babel/plugin-transform-react-display-name`
///
/// Add displayName to React.createClass calls
pub fn hook() -> impl VisitMutHook<()> {
    DisplayName
}

struct DisplayName;

// For tests
#[cfg(test)]
fn display_name() -> impl Pass {
    use swc_ecma_hooks::VisitMutWithHook;
    use swc_ecma_visit::visit_mut_pass;

    visit_mut_pass(VisitMutWithHook {
        hook: hook(),
        context: (),
    })
}

impl VisitMutHook<()> for DisplayName {
    fn enter_assign_expr(&mut self, expr: &mut AssignExpr, _ctx: &mut ()) {
        if expr.op != op!("=") {
            return;
        }

        if let AssignTarget::Simple(
            SimpleAssignTarget::Member(MemberExpr {
                prop: MemberProp::Ident(prop),
                ..
            })
            | SimpleAssignTarget::SuperProp(SuperPropExpr {
                prop: SuperProp::Ident(prop),
                ..
            }),
        ) = &expr.left
        {
            return expr.right.visit_mut_with(&mut Folder {
                name: Some(
                    Lit::Str(Str {
                        span: prop.span,
                        raw: None,
                        value: prop.sym.clone().into(),
                    })
                    .into(),
                ),
            });
        };

        if let Some(ident) = expr.left.as_ident() {
            expr.right.visit_mut_with(&mut Folder {
                name: Some(
                    Lit::Str(Str {
                        span: ident.span,
                        raw: None,
                        value: ident.sym.clone().into(),
                    })
                    .into(),
                ),
            });
        }
    }

    fn exit_module_decl(&mut self, decl: &mut ModuleDecl, _ctx: &mut ()) {
        if let ModuleDecl::ExportDefaultExpr(e) = decl {
            e.visit_mut_with(&mut Folder {
                name: Some(
                    Lit::Str(Str {
                        span: DUMMY_SP,
                        raw: None,
                        value: atom!("input").into(),
                    })
                    .into(),
                ),
            });
        }
    }

    fn exit_prop(&mut self, prop: &mut Prop, _ctx: &mut ()) {
        if let Prop::KeyValue(KeyValueProp { key, value }) = prop {
            let name = match key {
                PropName::Ident(ref i) => Lit::Str(Str {
                    span: i.span,
                    raw: None,
                    value: i.sym.clone().into(),
                })
                .into(),
                PropName::Str(ref s) => Lit::Str(s.clone()).into(),
                PropName::Num(ref n) => Lit::Num(n.clone()).into(),
                PropName::BigInt(ref b) => Lit::BigInt(b.clone()).into(),
                PropName::Computed(ref c) => c.expr.clone(),
                #[cfg(swc_ast_unknown)]
                _ => panic!("unable to access unknown nodes"),
            };

            value.visit_mut_with(&mut Folder { name: Some(name) });
        }
    }

    fn enter_var_declarator(&mut self, decl: &mut VarDeclarator, _ctx: &mut ()) {
        if let Pat::Ident(ref ident) = decl.name {
            decl.init.visit_mut_with(&mut Folder {
                name: Some(
                    Lit::Str(Str {
                        span: ident.span,
                        value: ident.sym.clone().into(),
                        raw: None,
                    })
                    .into(),
                ),
            });
        }
    }
}

struct Folder {
    name: Option<Box<Expr>>,
}

impl VisitMut for Folder {
    noop_visit_mut_type!();

    /// Don't recurse into array.
    fn visit_mut_array_lit(&mut self, _: &mut ArrayLit) {}

    fn visit_mut_call_expr(&mut self, expr: &mut CallExpr) {
        expr.visit_mut_children_with(self);

        if is_create_class_call(expr) {
            let name = match self.name.take() {
                Some(name) => name,
                None => return,
            };
            add_display_name(expr, name)
        }
    }

    /// Don't recurse into object.
    fn visit_mut_object_lit(&mut self, _: &mut ObjectLit) {}
}

fn is_create_class_call(call: &CallExpr) -> bool {
    let callee = match &call.callee {
        Callee::Super(_) | Callee::Import(_) => return false,
        Callee::Expr(callee) => &**callee,
        #[cfg(swc_ast_unknown)]
        _ => panic!("unable to access unknown nodes"),
    };

    match callee {
        Expr::Member(MemberExpr { obj, prop, .. }) if prop.is_ident_with("createClass") => {
            if obj.is_ident_ref_to("React") {
                return true;
            }
        }

        Expr::Ident(Ident { sym, .. }) if &**sym == "createReactClass" => return true,
        _ => {}
    }

    false
}

fn add_display_name(call: &mut CallExpr, name: Box<Expr>) {
    let props = match call.args.first_mut() {
        Some(&mut ExprOrSpread { ref mut expr, .. }) => match expr.deref_mut() {
            Expr::Object(ObjectLit { ref mut props, .. }) => props,
            _ => return,
        },
        _ => return,
    };

    for prop in &*props {
        if is_key_display_name(prop) {
            return;
        }
    }

    props.push(PropOrSpread::Prop(Box::new(Prop::KeyValue(KeyValueProp {
        key: PropName::Ident(atom!("displayName").into()),
        value: name,
    }))));
}

fn is_key_display_name(prop: &PropOrSpread) -> bool {
    match *prop {
        PropOrSpread::Prop(ref prop) => match **prop {
            Prop::Shorthand(ref i) => i.sym == "displayName",
            Prop::Method(MethodProp { ref key, .. })
            | Prop::Getter(GetterProp { ref key, .. })
            | Prop::Setter(SetterProp { ref key, .. })
            | Prop::KeyValue(KeyValueProp { ref key, .. }) => match *key {
                PropName::Ident(ref i) => i.sym == "displayName",
                PropName::Str(ref s) => {
                    matches!(s.value.as_str(), Some(value) if value == "displayName")
                }
                PropName::Num(..) => false,
                PropName::BigInt(..) => false,
                PropName::Computed(..) => false,
                #[cfg(swc_ast_unknown)]
                _ => panic!("unable to access unknown nodes"),
            },
            Prop::Assign(..) => unreachable!("invalid syntax"),
            #[cfg(swc_ast_unknown)]
            _ => panic!("unable to access unknown nodes"),
        },
        _ => false,
        // TODO(kdy1): maybe.. handle spread
    }
}
