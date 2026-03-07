use swc_common::{Spanned, DUMMY_SP};
use swc_ecma_ast::*;
use swc_ecma_codegen_macros::node_impl;

#[cfg(swc_ast_unknown)]
use crate::unknown_error;
use crate::{is_empty_comments, ListFormat};

#[node_impl]
impl MacroNode for ObjectLit {
    fn emit(&mut self, emitter: &mut Macro) -> Result {
        emitter.emit_leading_comments_of_span(self.span(), false)?;

        srcmap!(emitter, self, true);

        punct!(emitter, "{");

        let emit_new_line = !emitter.cfg.minify
            && !(self.props.is_empty() && is_empty_comments(&self.span(), &emitter.comments));

        if emit_new_line {
            emitter.wr.write_line()?;
        }

        let mut list_format =
            ListFormat::ObjectLiteralExpressionProperties | ListFormat::CanSkipTrailingComma;

        if !emit_new_line {
            list_format -= ListFormat::MultiLine | ListFormat::Indented;
        }

        emitter.emit_list(self.span(), Some(&self.props), list_format)?;

        if emit_new_line {
            emitter.wr.write_line()?;
        }

        srcmap!(emitter, self, false, true);
        punct!(emitter, "}");

        Ok(())
    }
}

#[node_impl]
impl MacroNode for Prop {
    fn emit(&mut self, emitter: &mut Macro) -> Result {
        match self {
            Prop::Shorthand(ref n) => emit!(n),
            Prop::KeyValue(ref n) => emit!(n),
            Prop::Assign(ref n) => emit!(n),
            Prop::Getter(ref n) => emit!(n),
            Prop::Setter(ref n) => emit!(n),
            Prop::Method(ref n) => emit!(n),
            #[cfg(swc_ast_unknown)]
            _ => return Err(unknown_error()),
        }

        Ok(())
    }
}

#[node_impl]
impl MacroNode for KeyValueProp {
    fn emit(&mut self, emitter: &mut Macro) -> Result {
        emitter.emit_leading_comments_of_span(self.span(), false)?;
        let key_span = self.key.span();
        let value_span = self.value.span();
        if !key_span.is_dummy() {
            emitter.wr.add_srcmap(key_span.lo)?;
        }
        emit!(self.key);
        if !key_span.is_dummy() && value_span.is_dummy() {
            emitter.wr.add_srcmap(key_span.hi)?;
        }
        punct!(emitter, ":");
        formatting_space!(emitter);
        if key_span.is_dummy() && !value_span.is_dummy() {
            emitter.wr.add_srcmap(value_span.lo)?;
        }
        emit!(self.value);

        Ok(())
    }
}

#[node_impl]
impl MacroNode for AssignProp {
    fn emit(&mut self, emitter: &mut Macro) -> Result {
        emitter.emit_leading_comments_of_span(self.span(), false)?;

        srcmap!(emitter, self, true);

        emit!(self.key);
        punct!(emitter, "=");
        emit!(self.value);

        Ok(())
    }
}

#[node_impl]
impl MacroNode for GetterProp {
    fn emit(&mut self, emitter: &mut Macro) -> Result {
        emitter.emit_leading_comments_of_span(self.span(), false)?;

        srcmap!(emitter, self, true);

        keyword!(emitter, "get");

        let starts_with_alpha_num = match self.key {
            PropName::Str(_) | PropName::Computed(_) => false,
            _ => true,
        };
        if starts_with_alpha_num {
            space!(emitter);
        } else {
            formatting_space!(emitter);
        }
        emit!(self.key);
        formatting_space!(emitter);
        punct!(emitter, "(");
        punct!(emitter, ")");
        formatting_space!(emitter);
        emit!(self.body);

        Ok(())
    }
}

#[node_impl]
impl MacroNode for SetterProp {
    fn emit(&mut self, emitter: &mut Macro) -> Result {
        emitter.emit_leading_comments_of_span(self.span(), false)?;

        srcmap!(emitter, self, true);

        keyword!(emitter, "set");

        let starts_with_alpha_num = match self.key {
            PropName::Str(_) | PropName::Computed(_) => false,
            _ => true,
        };

        if starts_with_alpha_num {
            space!(emitter);
        } else {
            formatting_space!(emitter);
        }

        emit!(self.key);
        formatting_space!(emitter);

        punct!(emitter, "(");
        if let Some(this) = &self.this_param {
            emit!(this);
            punct!(emitter, ",");

            formatting_space!(emitter);
        }

        emit!(self.param);

        punct!(emitter, ")");

        emit!(self.body);

        Ok(())
    }
}

#[node_impl]
impl MacroNode for MethodProp {
    fn emit(&mut self, emitter: &mut Macro) -> Result {
        emitter.emit_leading_comments_of_span(self.span(), false)?;

        srcmap!(emitter, self, true);

        if self.function.is_async {
            keyword!(emitter, "async");
            space!(emitter);
        }

        if self.function.is_generator {
            punct!(emitter, "*");
        }

        emit!(self.key);
        formatting_space!(emitter);
        // TODO
        emitter.emit_fn_trailing(&self.function)?;

        Ok(())
    }
}

#[node_impl]
impl MacroNode for PropName {
    fn emit(&mut self, emitter: &mut Macro) -> Result {
        match self {
            PropName::Ident(ident) => {
                // TODO: Use write_symbol when ident is a symbol.
                emitter.emit_leading_comments_of_span(ident.span, false)?;

                // Source map
                emitter.wr.commit_pending_semi()?;

                srcmap!(emitter, ident, true);

                if emitter.cfg.ascii_only {
                    if emitter.wr.can_ignore_invalid_unicodes() {
                        emitter.wr.write_symbol(
                            DUMMY_SP,
                            &crate::get_ascii_only_ident(&ident.sym, true, emitter.cfg.target),
                        )?;
                    } else {
                        emitter.wr.write_symbol(
                            DUMMY_SP,
                            &crate::get_ascii_only_ident(
                                &crate::handle_invalid_unicodes(&ident.sym),
                                true,
                                emitter.cfg.target,
                            ),
                        )?;
                    }
                } else {
                    emit!(ident);
                }
            }
            PropName::Str(ref n) => emit!(n),
            PropName::Num(ref n) => emit!(n),
            PropName::BigInt(ref n) => emit!(n),
            PropName::Computed(ref n) => emit!(n),
            #[cfg(swc_ast_unknown)]
            _ => return Err(unknown_error()),
        }

        Ok(())
    }
}

#[node_impl]
impl MacroNode for ComputedPropName {
    fn emit(&mut self, emitter: &mut Macro) -> Result {
        srcmap!(emitter, self, true);

        punct!(emitter, "[");
        emit!(self.expr);
        punct!(emitter, "]");

        srcmap!(emitter, self, false);

        Ok(())
    }
}
