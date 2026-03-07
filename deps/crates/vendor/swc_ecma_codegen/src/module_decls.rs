use swc_common::Spanned;
use swc_ecma_ast::*;
use swc_ecma_codegen_macros::node_impl;

#[cfg(swc_ast_unknown)]
use crate::unknown_error;
use crate::{util::StartsWithAlphaNum, ListFormat};

#[node_impl]
impl MacroNode for ModuleDecl {
    fn emit(&mut self, emitter: &mut Macro) -> Result {
        emitter.emit_leading_comments_of_span(self.span(), false)?;

        match self {
            ModuleDecl::Import(ref d) => emit!(d),
            ModuleDecl::ExportDecl(ref d) => emit!(d),
            ModuleDecl::ExportNamed(ref d) => emit!(d),
            ModuleDecl::ExportDefaultDecl(ref d) => emit!(d),
            ModuleDecl::ExportDefaultExpr(ref n) => emit!(n),
            ModuleDecl::ExportAll(ref d) => emit!(d),
            ModuleDecl::TsExportAssignment(ref n) => emit!(n),
            ModuleDecl::TsImportEquals(ref n) => emit!(n),
            ModuleDecl::TsNamespaceExport(ref n) => emit!(n),
            #[cfg(swc_ast_unknown)]
            _ => return Err(unknown_error()),
        }

        emitter.emit_trailing_comments_of_pos(self.span().hi, true, true)?;

        if !emitter.cfg.minify {
            emitter.wr.write_line()?;
        }

        Ok(())
    }
}

#[node_impl]
impl MacroNode for ExportDecl {
    fn emit(&mut self, emitter: &mut Macro) -> Result {
        srcmap!(emitter, self, true);

        match &self.decl {
            Decl::Class(decl) => {
                for dec in &decl.class.decorators {
                    emit!(dec);
                }

                keyword!(emitter, "export");

                space!(emitter);
                emitter.emit_class_decl_inner(decl, true)?;
            }
            _ => {
                keyword!(emitter, "export");

                space!(emitter);
                emit!(self.decl);
            }
        }

        Ok(())
    }
}

#[node_impl]
impl MacroNode for ExportDefaultExpr {
    fn emit(&mut self, emitter: &mut Macro) -> Result {
        srcmap!(emitter, self, true);

        keyword!(emitter, "export");

        space!(emitter);
        keyword!(emitter, "default");
        {
            let starts_with_alpha_num = self.expr.starts_with_alpha_num();
            if starts_with_alpha_num {
                space!(emitter);
            } else {
                formatting_space!(emitter);
            }
            emit!(self.expr);
        }
        semi!(emitter);

        srcmap!(emitter, self, false);

        Ok(())
    }
}

#[node_impl]
impl MacroNode for ExportDefaultDecl {
    fn emit(&mut self, emitter: &mut Macro) -> Result {
        emitter.emit_leading_comments_of_span(self.span(), false)?;

        srcmap!(emitter, self, true);

        keyword!(emitter, "export");

        space!(emitter);
        keyword!(emitter, "default");
        space!(emitter);
        match self.decl {
            DefaultDecl::Class(ref n) => emit!(n),
            DefaultDecl::Fn(ref n) => emit!(n),
            DefaultDecl::TsInterfaceDecl(ref n) => emit!(n),
            #[cfg(swc_ast_unknown)]
            _ => return Err(unknown_error()),
        }

        Ok(())
    }
}

#[node_impl]
impl MacroNode for ImportDecl {
    fn emit(&mut self, emitter: &mut Macro) -> Result {
        emitter.emit_leading_comments_of_span(self.span(), false)?;

        srcmap!(emitter, self, true);

        keyword!(emitter, "import");

        if self.type_only {
            space!(emitter);
            keyword!(emitter, "type");
        }

        match self.phase {
            ImportPhase::Evaluation => {}
            ImportPhase::Source => {
                space!(emitter);
                keyword!(emitter, "source");
            }
            ImportPhase::Defer => {
                space!(emitter);
                keyword!(emitter, "defer");
            }
            #[cfg(swc_ast_unknown)]
            _ => return Err(unknown_error()),
        }

        let starts_with_ident = !self.specifiers.is_empty()
            && match &self.specifiers[0] {
                ImportSpecifier::Default(_) => true,
                _ => false,
            };
        if starts_with_ident {
            space!(emitter);
        } else {
            formatting_space!(emitter);
        }

        let mut specifiers = Vec::new();
        let mut emitted_default = false;
        let mut emitted_ns = false;
        for specifier in &self.specifiers {
            match specifier {
                ImportSpecifier::Named(ref s) => {
                    specifiers.push(s);
                }
                ImportSpecifier::Default(ref s) => {
                    emit!(s.local);
                    emitted_default = true;
                }
                ImportSpecifier::Namespace(ref ns) => {
                    if emitted_default {
                        punct!(emitter, ",");
                        formatting_space!(emitter);
                    }

                    emitted_ns = true;

                    assert!(self.specifiers.len() <= 2);
                    punct!(emitter, "*");
                    formatting_space!(emitter);
                    keyword!(emitter, "as");
                    space!(emitter);
                    emit!(ns.local);
                }
                #[cfg(swc_ast_unknown)]
                _ => return Err(unknown_error()),
            }
        }

        if specifiers.is_empty() {
            if emitted_ns || emitted_default {
                space!(emitter);
                keyword!(emitter, "from");
                formatting_space!(emitter);
            }
        } else {
            if emitted_default {
                punct!(emitter, ",");
                formatting_space!(emitter);
            }

            punct!(emitter, "{");
            emitter.emit_list(
                self.span(),
                Some(&specifiers),
                ListFormat::NamedImportsOrExportsElements,
            )?;
            punct!(emitter, "}");
            formatting_space!(emitter);

            keyword!(emitter, "from");
            formatting_space!(emitter);
        }

        emit!(self.src);

        if let Some(with) = &self.with {
            formatting_space!(emitter);
            if emitter.cfg.emit_assert_for_import_attributes {
                keyword!(emitter, "assert");
            } else {
                keyword!(emitter, "with")
            };
            formatting_space!(emitter);
            emit!(with);
        }

        semi!(emitter);

        srcmap!(emitter, self, false);

        Ok(())
    }
}

#[node_impl]
impl MacroNode for ImportNamedSpecifier {
    fn emit(&mut self, emitter: &mut Macro) -> Result {
        srcmap!(emitter, self, true);

        if self.is_type_only {
            keyword!(emitter, "type");
            space!(emitter);
        }

        if let Some(ref imported) = self.imported {
            emit!(imported);
            space!(emitter);
            keyword!(emitter, "as");
            space!(emitter);
        }

        emit!(self.local);

        srcmap!(emitter, self, false);

        Ok(())
    }
}

#[node_impl]
impl MacroNode for ExportSpecifier {
    fn emit(&mut self, emitter: &mut Macro) -> Result {
        match self {
            ExportSpecifier::Default(..) => {
                unimplemented!("codegen of `export default from 'foo';`")
            }
            ExportSpecifier::Namespace(ref node) => emit!(node),
            ExportSpecifier::Named(ref node) => emit!(node),
            #[cfg(swc_ast_unknown)]
            _ => return Err(unknown_error()),
        }

        Ok(())
    }
}

#[node_impl]
impl MacroNode for ExportNamespaceSpecifier {
    fn emit(&mut self, emitter: &mut Macro) -> Result {
        emitter.emit_leading_comments_of_span(self.span(), false)?;

        srcmap!(emitter, self, true);

        punct!(emitter, "*");
        formatting_space!(emitter);
        keyword!(emitter, "as");
        space!(emitter);
        emit!(self.name);

        srcmap!(emitter, self, false);

        Ok(())
    }
}

#[node_impl]
impl MacroNode for ExportNamedSpecifier {
    fn emit(&mut self, emitter: &mut Macro) -> Result {
        emitter.emit_leading_comments_of_span(self.span(), false)?;

        srcmap!(emitter, self, true);

        if self.is_type_only {
            keyword!(emitter, "type");
            space!(emitter);
        }

        if let Some(exported) = &self.exported {
            emit!(self.orig);
            space!(emitter);
            keyword!(emitter, "as");
            space!(emitter);
            emit!(exported);
        } else {
            emit!(self.orig);
        }
        srcmap!(emitter, self, false);

        Ok(())
    }
}

#[node_impl]
impl MacroNode for NamedExport {
    fn emit(&mut self, emitter: &mut Macro) -> Result {
        emitter.emit_leading_comments_of_span(self.span(), false)?;

        srcmap!(emitter, self, true);

        struct Specifiers<'a> {
            has_namespace_spec: bool,
            namespace_spec: Option<&'a ExportNamespaceSpecifier>,
            has_named_specs: bool,
            named_specs: Vec<&'a ExportSpecifier>,
        }
        let Specifiers {
            has_namespace_spec,
            namespace_spec,
            has_named_specs,
            named_specs,
        } = self.specifiers.iter().fold(
            Specifiers {
                has_namespace_spec: false,
                namespace_spec: None,
                has_named_specs: false,
                named_specs: Vec::new(),
            },
            |mut result, s| match s {
                ExportSpecifier::Namespace(spec) => {
                    result.has_namespace_spec = true;
                    // There can only be one namespace export specifier.
                    if result.namespace_spec.is_none() {
                        result.namespace_spec = Some(spec)
                    }
                    result
                }
                spec => {
                    result.has_named_specs = true;
                    result.named_specs.push(spec);
                    result
                }
            },
        );

        keyword!(emitter, "export");

        if self.type_only {
            space!(emitter);
            keyword!(emitter, "type");
        }
        formatting_space!(emitter);

        if let Some(spec) = namespace_spec {
            emit!(spec);
            if has_named_specs {
                punct!(emitter, ",");
                formatting_space!(emitter);
            }
        }
        if has_named_specs || !has_namespace_spec {
            punct!(emitter, "{");
            emitter.emit_list(
                self.span,
                Some(&named_specs),
                ListFormat::NamedImportsOrExportsElements,
            )?;
            punct!(emitter, "}");
        }

        if let Some(ref src) = self.src {
            if has_named_specs || !has_namespace_spec {
                formatting_space!(emitter);
            } else if has_namespace_spec {
                space!(emitter);
            }
            keyword!(emitter, "from");
            formatting_space!(emitter);
            emit!(src);

            if let Some(with) = &self.with {
                formatting_space!(emitter);
                if emitter.cfg.emit_assert_for_import_attributes {
                    keyword!(emitter, "assert");
                } else {
                    keyword!(emitter, "with")
                };
                formatting_space!(emitter);
                emit!(with);
            }
        }
        semi!(emitter);

        srcmap!(emitter, self, false);

        Ok(())
    }
}

#[node_impl]
impl MacroNode for ExportAll {
    fn emit(&mut self, emitter: &mut Macro) -> Result {
        emitter.emit_leading_comments_of_span(self.span(), false)?;

        srcmap!(emitter, self, true);

        keyword!(emitter, "export");

        if self.type_only {
            space!(emitter);
            keyword!(emitter, "type");
            space!(emitter);
        } else {
            formatting_space!(emitter);
        }

        punct!(emitter, "*");
        formatting_space!(emitter);
        keyword!(emitter, "from");
        formatting_space!(emitter);
        emit!(self.src);

        if let Some(with) = &self.with {
            formatting_space!(emitter);
            if emitter.cfg.emit_assert_for_import_attributes {
                keyword!(emitter, "assert");
            } else {
                keyword!(emitter, "with")
            };
            formatting_space!(emitter);
            emit!(with);
        }

        semi!(emitter);

        srcmap!(emitter, self, false);

        Ok(())
    }
}
