use swc_common::{BytePos, SourceMapper, Spanned};
use swc_ecma_ast::*;
use swc_ecma_codegen_macros::node_impl;

use super::Emitter;
use crate::text_writer::WriteJs;
#[cfg(swc_ast_unknown)]
use crate::unknown_error;

impl<W, S: SourceMapper> Emitter<'_, W, S>
where
    W: WriteJs,
    S: SourceMapperExt,
{
}

#[node_impl]
impl MacroNode for JSXElement {
    fn emit(&mut self, emitter: &mut Macro) -> Result {
        emitter.emit_leading_comments_of_span(self.span(), false)?;
        emit!(self.opening);
        emitter.emit_list(
            self.span(),
            Some(&self.children),
            ListFormat::JsxElementOrFragmentChildren,
        )?;
        if let Some(ref closing) = self.closing {
            emit!(closing)
        }
        Ok(())
    }
}

#[node_impl]
impl MacroNode for JSXOpeningElement {
    fn emit(&mut self, emitter: &mut Macro) -> Result {
        punct!(emitter, "<");
        emit!(self.name);

        if let Some(type_args) = &self.type_args {
            emit!(type_args);
        }

        if !self.attrs.is_empty() {
            space!(emitter);

            emitter.emit_list(
                self.span(),
                Some(&self.attrs),
                ListFormat::JsxElementAttributes,
            )?;
        }

        if self.self_closing {
            punct!(emitter, "/");
        }
        punct!(emitter, ">");
        Ok(())
    }
}

#[node_impl]
impl MacroNode for JSXElementName {
    fn emit(&mut self, emitter: &mut Macro) -> Result {
        match *self {
            JSXElementName::Ident(ref n) => emit!(n),
            JSXElementName::JSXMemberExpr(ref n) => emit!(n),
            JSXElementName::JSXNamespacedName(ref n) => emit!(n),
            #[cfg(swc_ast_unknown)]
            _ => return Err(unknown_error()),
        }
        Ok(())
    }
}

#[node_impl]
impl MacroNode for JSXAttr {
    fn emit(&mut self, emitter: &mut Macro) -> Result {
        emit!(self.name);

        if let Some(ref value) = self.value {
            punct!(emitter, "=");
            emit!(value);
        }
        Ok(())
    }
}

#[node_impl]
impl MacroNode for JSXAttrValue {
    fn emit(&mut self, emitter: &mut Macro) -> Result {
        match *self {
            JSXAttrValue::Str(ref n) => emit!(n),
            JSXAttrValue::JSXExprContainer(ref n) => emit!(n),
            JSXAttrValue::JSXElement(ref n) => emit!(n),
            JSXAttrValue::JSXFragment(ref n) => emit!(n),
            #[cfg(swc_ast_unknown)]
            _ => return Err(unknown_error()),
        }
        Ok(())
    }
}

#[node_impl]
impl MacroNode for JSXAttrName {
    fn emit(&mut self, emitter: &mut Macro) -> Result {
        match *self {
            JSXAttrName::Ident(ref n) => emit!(n),
            JSXAttrName::JSXNamespacedName(ref n) => emit!(n),
            #[cfg(swc_ast_unknown)]
            _ => return Err(unknown_error()),
        }
        Ok(())
    }
}

#[node_impl]
impl MacroNode for JSXAttrOrSpread {
    fn emit(&mut self, emitter: &mut Macro) -> Result {
        match *self {
            JSXAttrOrSpread::JSXAttr(ref n) => emit!(n),
            JSXAttrOrSpread::SpreadElement(ref n) => {
                punct!(emitter, "{");
                emit!(n);
                punct!(emitter, "}");
            }
            #[cfg(swc_ast_unknown)]
            _ => return Err(unknown_error()),
        }
        Ok(())
    }
}

#[node_impl]
impl MacroNode for JSXElementChild {
    fn emit(&mut self, emitter: &mut Macro) -> Result {
        match *self {
            JSXElementChild::JSXElement(ref n) => emit!(n),
            JSXElementChild::JSXExprContainer(ref n) => emit!(n),
            JSXElementChild::JSXFragment(ref n) => emit!(n),
            JSXElementChild::JSXSpreadChild(ref n) => emit!(n),
            JSXElementChild::JSXText(ref n) => emit!(n),
            #[cfg(swc_ast_unknown)]
            _ => return Err(unknown_error()),
        }
        Ok(())
    }
}

#[node_impl]
impl MacroNode for JSXSpreadChild {
    fn emit(&mut self, emitter: &mut Macro) -> Result {
        punct!(emitter, "{");
        punct!(emitter, "...");
        emit!(self.expr);
        punct!(emitter, "}");
        Ok(())
    }
}

#[node_impl]
impl MacroNode for JSXExprContainer {
    fn emit(&mut self, emitter: &mut Macro) -> Result {
        punct!(emitter, "{");
        emitter.emit_trailing_comments_of_pos(self.span.lo + BytePos(1), true, false)?;
        emit!(self.expr);
        punct!(emitter, "}");
        Ok(())
    }
}

#[node_impl]
impl MacroNode for JSXExpr {
    fn emit(&mut self, emitter: &mut Macro) -> Result {
        match *self {
            JSXExpr::Expr(ref n) => emit!(n),
            JSXExpr::JSXEmptyExpr(ref n) => emit!(n),
            #[cfg(swc_ast_unknown)]
            _ => return Err(unknown_error()),
        }
        Ok(())
    }
}

#[node_impl]
impl MacroNode for JSXClosingElement {
    fn emit(&mut self, emitter: &mut Macro) -> Result {
        punct!(emitter, "</");
        emit!(self.name);
        punct!(emitter, ">");
        Ok(())
    }
}

#[node_impl]
impl MacroNode for JSXFragment {
    fn emit(&mut self, emitter: &mut Macro) -> Result {
        emitter.emit_leading_comments_of_span(self.span(), false)?;
        emit!(self.opening);

        emitter.emit_list(
            self.span(),
            Some(&self.children),
            ListFormat::JsxElementOrFragmentChildren,
        )?;

        emit!(self.closing);
        Ok(())
    }
}

#[node_impl]
impl MacroNode for JSXOpeningFragment {
    fn emit(&mut self, emitter: &mut Macro) -> Result {
        punct!(emitter, "<>");
        Ok(())
    }
}

#[node_impl]
impl MacroNode for JSXClosingFragment {
    fn emit(&mut self, emitter: &mut Macro) -> Result {
        punct!(emitter, "</>");
        Ok(())
    }
}

#[node_impl]
impl MacroNode for JSXNamespacedName {
    fn emit(&mut self, emitter: &mut Macro) -> Result {
        emit!(self.ns);
        punct!(emitter, ":");
        emit!(self.name);
        Ok(())
    }
}

#[node_impl]
impl MacroNode for JSXEmptyExpr {
    fn emit(&mut self, emitter: &mut Macro) -> Result {
        emitter.emit_leading_comments_of_span(self.span, false)?;
        Ok(())
    }
}

#[node_impl]
impl MacroNode for JSXText {
    fn emit(&mut self, emitter: &mut Macro) -> Result {
        emitter.emit_atom(self.span(), &self.raw)?;
        Ok(())
    }
}

#[node_impl]
impl MacroNode for JSXMemberExpr {
    fn emit(&mut self, emitter: &mut Macro) -> Result {
        emit!(self.obj);
        punct!(emitter, ".");
        emit!(self.prop);
        Ok(())
    }
}

#[node_impl]
impl MacroNode for JSXObject {
    fn emit(&mut self, emitter: &mut Macro) -> Result {
        match *self {
            JSXObject::Ident(ref n) => emit!(n),
            JSXObject::JSXMemberExpr(ref n) => emit!(n),
            #[cfg(swc_ast_unknown)]
            _ => return Err(unknown_error()),
        }
        Ok(())
    }
}
