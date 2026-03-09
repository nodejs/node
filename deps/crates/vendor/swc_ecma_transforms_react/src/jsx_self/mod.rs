use swc_common::DUMMY_SP;
use swc_ecma_ast::*;
use swc_ecma_hooks::VisitMutHook;
use swc_ecma_utils::quote_ident;

#[cfg(test)]
mod tests;

/// `@babel/plugin-transform-react-jsx-self`
///
/// Add a __self prop to all JSX Elements
pub fn hook(dev: bool) -> impl VisitMutHook<()> {
    JsxSelf {
        dev,
        ctx_stack: vec![Context::default()],
    }
}

/// See <https://github.com/babel/babel/blob/1bdb1a4175ed1fc40751fb84dc4ad1900260f28f/packages/babel-plugin-transform-react-jsx-self/src/index.ts#L27>
#[derive(Clone, Copy, Default)]
struct Context {
    in_constructor: bool,
    in_derived_class: bool,
}

struct JsxSelf {
    dev: bool,
    ctx_stack: Vec<Context>,
}

impl JsxSelf {
    fn current_ctx(&self) -> Context {
        *self
            .ctx_stack
            .last()
            .expect("context stack should never be empty")
    }

    fn push_ctx(&mut self, ctx: Context) {
        self.ctx_stack.push(ctx);
    }

    fn pop_ctx(&mut self) {
        if self.ctx_stack.len() > 1 {
            self.ctx_stack.pop();
        }
    }
}

// For tests
#[cfg(test)]
fn jsx_self(dev: bool) -> impl Pass {
    use swc_ecma_hooks::VisitMutWithHook;
    use swc_ecma_visit::visit_mut_pass;

    visit_mut_pass(VisitMutWithHook {
        hook: hook(dev),
        context: (),
    })
}

impl VisitMutHook<()> for JsxSelf {
    fn enter_class(&mut self, n: &mut Class, _ctx: &mut ()) {
        let mut new_ctx = self.current_ctx();
        new_ctx.in_derived_class = n.super_class.is_some();
        self.push_ctx(new_ctx);
    }

    fn exit_class(&mut self, _n: &mut Class, _ctx: &mut ()) {
        self.pop_ctx();
    }

    fn enter_fn_decl(&mut self, _n: &mut FnDecl, _ctx: &mut ()) {
        let mut new_ctx = self.current_ctx();
        new_ctx.in_constructor = false;
        self.push_ctx(new_ctx);
    }

    fn exit_fn_decl(&mut self, _n: &mut FnDecl, _ctx: &mut ()) {
        self.pop_ctx();
    }

    fn enter_fn_expr(&mut self, _n: &mut FnExpr, _ctx: &mut ()) {
        let mut new_ctx = self.current_ctx();
        new_ctx.in_constructor = false;
        self.push_ctx(new_ctx);
    }

    fn exit_fn_expr(&mut self, _n: &mut FnExpr, _ctx: &mut ()) {
        self.pop_ctx();
    }

    fn enter_getter_prop(&mut self, _n: &mut GetterProp, _ctx: &mut ()) {
        let mut new_ctx = self.current_ctx();
        new_ctx.in_constructor = false;
        self.push_ctx(new_ctx);
    }

    fn exit_getter_prop(&mut self, _n: &mut GetterProp, _ctx: &mut ()) {
        self.pop_ctx();
    }

    fn enter_setter_prop(&mut self, _n: &mut SetterProp, _ctx: &mut ()) {
        let mut new_ctx = self.current_ctx();
        new_ctx.in_constructor = false;
        self.push_ctx(new_ctx);
    }

    fn exit_setter_prop(&mut self, _n: &mut SetterProp, _ctx: &mut ()) {
        self.pop_ctx();
    }

    fn enter_method_prop(&mut self, _n: &mut MethodProp, _ctx: &mut ()) {
        let mut new_ctx = self.current_ctx();
        new_ctx.in_constructor = false;
        self.push_ctx(new_ctx);
    }

    fn exit_method_prop(&mut self, _n: &mut MethodProp, _ctx: &mut ()) {
        self.pop_ctx();
    }

    fn enter_constructor(&mut self, _n: &mut Constructor, _ctx: &mut ()) {
        let mut new_ctx = self.current_ctx();
        new_ctx.in_constructor = true;
        self.push_ctx(new_ctx);
    }

    fn exit_constructor(&mut self, _n: &mut Constructor, _ctx: &mut ()) {
        self.pop_ctx();
    }

    fn enter_class_method(&mut self, _n: &mut ClassMethod, _ctx: &mut ()) {
        let mut new_ctx = self.current_ctx();
        new_ctx.in_constructor = false;
        self.push_ctx(new_ctx);
    }

    fn exit_class_method(&mut self, _n: &mut ClassMethod, _ctx: &mut ()) {
        self.pop_ctx();
    }

    fn enter_private_method(&mut self, _n: &mut PrivateMethod, _ctx: &mut ()) {
        let mut new_ctx = self.current_ctx();
        new_ctx.in_constructor = false;
        self.push_ctx(new_ctx);
    }

    fn exit_private_method(&mut self, _n: &mut PrivateMethod, _ctx: &mut ()) {
        self.pop_ctx();
    }

    fn enter_static_block(&mut self, _n: &mut StaticBlock, _ctx: &mut ()) {
        let mut new_ctx = self.current_ctx();
        new_ctx.in_constructor = false;
        self.push_ctx(new_ctx);
    }

    fn exit_static_block(&mut self, _n: &mut StaticBlock, _ctx: &mut ()) {
        self.pop_ctx();
    }

    fn enter_jsx_opening_element(&mut self, n: &mut JSXOpeningElement, _ctx: &mut ()) {
        if !self.dev {
            return;
        }

        let ctx = self.current_ctx();

        // https://github.com/babel/babel/blob/1bdb1a4175ed1fc40751fb84dc4ad1900260f28f/packages/babel-plugin-transform-react-jsx-self/src/index.ts#L50
        if ctx.in_constructor && ctx.in_derived_class {
            return;
        }

        n.attrs.push(JSXAttrOrSpread::JSXAttr(JSXAttr {
            span: DUMMY_SP,
            name: JSXAttrName::Ident(quote_ident!("__self")),
            value: Some(JSXAttrValue::JSXExprContainer(JSXExprContainer {
                span: DUMMY_SP,
                expr: JSXExpr::Expr(Box::new(ThisExpr { span: DUMMY_SP }.into())),
            })),
        }));
    }
}
