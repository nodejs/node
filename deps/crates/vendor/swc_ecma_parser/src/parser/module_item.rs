use swc_atoms::atom;
use swc_common::Span;
use swc_ecma_ast::*;

use crate::{error::SyntaxError, input::Tokens, lexer::Token, Context, PResult, Parser};

impl<I: Tokens> Parser<I> {
    pub fn parse_module_item(&mut self) -> PResult<ModuleItem> {
        self.do_inside_of_context(Context::TopLevel, |p| {
            p.parse_stmt_like(true, handle_import_export)
        })
    }

    pub(crate) fn parse_module_item_block_body(
        &mut self,
        allow_directives: bool,
        end: Option<Token>,
    ) -> PResult<Vec<ModuleItem>> {
        self.parse_block_body(allow_directives, end, handle_import_export)
    }

    /// Parses `from 'foo.js' with {};` or `from 'foo.js' assert {};`
    fn parse_from_clause_and_semi(&mut self) -> PResult<(Box<Str>, Option<Box<ObjectLit>>)> {
        expect!(self, Token::From);

        let cur = self.input().cur();
        let src = if cur == Token::Str {
            Box::new(self.parse_str_lit())
        } else {
            unexpected!(self, "a string literal")
        };
        let with = if self.input().syntax().import_attributes()
            && !self.input().had_line_break_before_cur()
            && (self.input_mut().eat(Token::Assert) || self.input_mut().eat(Token::With))
        {
            match self.parse_object_expr()? {
                Expr::Object(v) => Some(Box::new(v)),
                _ => unreachable!(),
            }
        } else {
            None
        };
        self.expect_general_semi()?;
        Ok((src, with))
    }

    fn parse_named_export_specifier(&mut self, type_only: bool) -> PResult<ExportNamedSpecifier> {
        let start = self.cur_pos();

        let mut is_type_only = false;

        let orig = match self.parse_module_export_name()? {
            ModuleExportName::Ident(orig_ident) => {
                // Handle:
                // `export { type xx }`
                // `export { type xx as yy }`
                // `export { type as }`
                // `export { type as as }`
                // `export { type as as as }`
                if self.syntax().typescript()
                    && orig_ident.sym == "type"
                    && self.input().cur().is_word()
                {
                    let possibly_orig = self.parse_ident_name().map(Ident::from)?;
                    if possibly_orig.sym == "as" {
                        // `export { type as }`
                        if !self.input().cur().is_word() {
                            if type_only {
                                self.emit_err(orig_ident.span, SyntaxError::TS2207);
                            }

                            return Ok(ExportNamedSpecifier {
                                span: self.span(start),
                                orig: ModuleExportName::Ident(possibly_orig),
                                exported: None,
                                is_type_only: true,
                            });
                        }

                        let maybe_as = self.parse_ident_name().map(Ident::from)?;
                        if maybe_as.sym == "as" {
                            if self.input().cur().is_word() {
                                // `export { type as as as }`
                                // `export { type as as foo }`
                                let exported = self.parse_ident_name().map(Ident::from)?;

                                if type_only {
                                    self.emit_err(orig_ident.span, SyntaxError::TS2207);
                                }

                                debug_assert!(start <= orig_ident.span.hi());
                                return Ok(ExportNamedSpecifier {
                                    span: Span::new_with_checked(start, orig_ident.span.hi()),
                                    orig: ModuleExportName::Ident(possibly_orig),
                                    exported: Some(ModuleExportName::Ident(exported)),
                                    is_type_only: true,
                                });
                            } else {
                                // `export { type as as }`
                                return Ok(ExportNamedSpecifier {
                                    span: Span::new_with_checked(start, orig_ident.span.hi()),
                                    orig: ModuleExportName::Ident(orig_ident),
                                    exported: Some(ModuleExportName::Ident(maybe_as)),
                                    is_type_only: false,
                                });
                            }
                        } else {
                            // `export { type as xxx }`
                            return Ok(ExportNamedSpecifier {
                                span: Span::new_with_checked(start, orig_ident.span.hi()),
                                orig: ModuleExportName::Ident(orig_ident),
                                exported: Some(ModuleExportName::Ident(maybe_as)),
                                is_type_only: false,
                            });
                        }
                    } else {
                        // `export { type xx }`
                        // `export { type xx as yy }`
                        if type_only {
                            self.emit_err(orig_ident.span, SyntaxError::TS2207);
                        }

                        is_type_only = true;
                        ModuleExportName::Ident(possibly_orig)
                    }
                } else {
                    ModuleExportName::Ident(orig_ident)
                }
            }
            module_export_name => module_export_name,
        };

        let exported = if self.input_mut().eat(Token::As) {
            Some(self.parse_module_export_name()?)
        } else {
            None
        };

        Ok(ExportNamedSpecifier {
            span: self.span(start),
            orig,
            exported,
            is_type_only,
        })
    }

    fn parse_imported_binding(&mut self) -> PResult<Ident> {
        Ok(self
            .do_outside_of_context(Context::InAsync.union(Context::InGenerator), |p| {
                p.parse_binding_ident(false)
            })?
            .into())
    }

    fn parse_imported_default_binding(&mut self) -> PResult<Ident> {
        self.parse_imported_binding()
    }

    /// Parse `foo`, `foo2 as bar` in `import { foo, foo2 as bar }`
    fn parse_import_specifier(&mut self, type_only: bool) -> PResult<ImportSpecifier> {
        let start = self.cur_pos();
        match self.parse_module_export_name()? {
            ModuleExportName::Ident(mut orig_name) => {
                let mut is_type_only = false;
                // Handle:
                // `import { type xx } from 'mod'`
                // `import { type xx as yy } from 'mod'`
                // `import { type "xx" as yy } from 'mod'`
                // `import { type as } from 'mod'`
                // `import { type as as } from 'mod'`
                // `import { type as as as } from 'mod'`
                if self.syntax().typescript() && orig_name.sym == "type" {
                    // Handle `import { type "string" as foo }` case
                    if self.input().cur() == Token::Str {
                        let imported = self.parse_module_export_name()?;
                        expect!(self, Token::As);
                        let local: Ident = self.parse_binding_ident(false)?.into();

                        if type_only {
                            self.emit_err(orig_name.span, SyntaxError::TS2206);
                        }

                        return Ok(ImportSpecifier::Named(ImportNamedSpecifier {
                            span: Span::new_with_checked(start, local.span.hi()),
                            local,
                            imported: Some(imported),
                            is_type_only: true,
                        }));
                    }

                    if self.input().cur().is_word() {
                        let possibly_orig_name = self.parse_ident_name().map(Ident::from)?;
                        if possibly_orig_name.sym == "as" {
                            // `import { type as } from 'mod'`
                            if !self.input().cur().is_word() {
                                if self.ctx().is_reserved_word(&possibly_orig_name.sym) {
                                    syntax_error!(
                                        self,
                                        possibly_orig_name.span,
                                        SyntaxError::ReservedWordInImport
                                    )
                                }

                                if type_only {
                                    self.emit_err(orig_name.span, SyntaxError::TS2206);
                                }

                                return Ok(ImportSpecifier::Named(ImportNamedSpecifier {
                                    span: self.span(start),
                                    local: possibly_orig_name,
                                    imported: None,
                                    is_type_only: true,
                                }));
                            }

                            let maybe_as: Ident = self.parse_binding_ident(false)?.into();
                            if maybe_as.sym == "as" {
                                if self.input().cur().is_word() {
                                    // `import { type as as as } from 'mod'`
                                    // `import { type as as foo } from 'mod'`
                                    let local: Ident = self.parse_binding_ident(false)?.into();

                                    if type_only {
                                        self.emit_err(orig_name.span, SyntaxError::TS2206);
                                    }

                                    return Ok(ImportSpecifier::Named(ImportNamedSpecifier {
                                        span: Span::new_with_checked(start, local.span.hi()),
                                        local,
                                        imported: Some(ModuleExportName::Ident(possibly_orig_name)),
                                        is_type_only: true,
                                    }));
                                } else {
                                    // `import { type as as } from 'mod'`
                                    return Ok(ImportSpecifier::Named(ImportNamedSpecifier {
                                        span: Span::new_with_checked(start, maybe_as.span.hi()),
                                        local: maybe_as,
                                        imported: Some(ModuleExportName::Ident(orig_name)),
                                        is_type_only: false,
                                    }));
                                }
                            } else {
                                // `import { type as xxx } from 'mod'`
                                return Ok(ImportSpecifier::Named(ImportNamedSpecifier {
                                    span: Span::new_with_checked(start, maybe_as.span.hi()),
                                    local: maybe_as,
                                    imported: Some(ModuleExportName::Ident(orig_name)),
                                    is_type_only: false,
                                }));
                            }
                        } else {
                            // `import { type xx } from 'mod'`
                            // `import { type xx as yy } from 'mod'`
                            if type_only {
                                self.emit_err(orig_name.span, SyntaxError::TS2206);
                            }

                            orig_name = possibly_orig_name;
                            is_type_only = true;
                        }
                    }
                }

                if self.input_mut().eat(Token::As) {
                    let local: Ident = self.parse_binding_ident(false)?.into();
                    return Ok(ImportSpecifier::Named(ImportNamedSpecifier {
                        span: Span::new_with_checked(start, local.span.hi()),
                        local,
                        imported: Some(ModuleExportName::Ident(orig_name)),
                        is_type_only,
                    }));
                }

                // Handle difference between
                //
                // 'ImportedBinding'
                // 'IdentifierName' as 'ImportedBinding'
                if self.ctx().is_reserved_word(&orig_name.sym) {
                    syntax_error!(self, orig_name.span, SyntaxError::ReservedWordInImport)
                }

                let local = orig_name;
                Ok(ImportSpecifier::Named(ImportNamedSpecifier {
                    span: self.span(start),
                    local,
                    imported: None,
                    is_type_only,
                }))
            }
            ModuleExportName::Str(orig_str) => {
                if self.input_mut().eat(Token::As) {
                    let local: Ident = self.parse_binding_ident(false)?.into();
                    Ok(ImportSpecifier::Named(ImportNamedSpecifier {
                        span: Span::new_with_checked(start, local.span.hi()),
                        local,
                        imported: Some(ModuleExportName::Str(orig_str)),
                        is_type_only: false,
                    }))
                } else {
                    syntax_error!(
                        self,
                        orig_str.span,
                        SyntaxError::ImportBindingIsString(orig_str.value.to_string_lossy().into())
                    )
                }
            }
            #[cfg(swc_ast_unknown)]
            _ => unreachable!(),
        }
    }

    pub(crate) fn parse_export(&mut self, mut decorators: Vec<Decorator>) -> PResult<ModuleDecl> {
        if !self.ctx().contains(Context::Module) && self.ctx().contains(Context::TopLevel) {
            // Switch to module mode
            let ctx = self.ctx() | Context::Module | Context::Strict;
            self.set_ctx(ctx);
        }

        let start = self.cur_pos();
        self.assert_and_bump(Token::Export);

        let cur = self.input().cur();
        if cur == Token::Eof {
            return Err(self.eof_error());
        }

        let after_export_start = self.cur_pos();

        // "export declare" is equivalent to just "export".
        let declare = self.input().syntax().typescript() && self.input_mut().eat(Token::Declare);

        if declare {
            // TODO: Remove
            if let Some(decl) = self.try_parse_ts_declare(after_export_start, decorators.clone())? {
                return Ok(ExportDecl {
                    span: self.span(start),
                    decl,
                }
                .into());
            }
        }

        if self.input().syntax().typescript() {
            let cur = self.input().cur();
            if cur.is_word() {
                let sym = cur.take_word(&self.input);
                // TODO: remove clone
                if let Some(decl) = self.try_parse_ts_export_decl(decorators.clone(), sym) {
                    return Ok(ExportDecl {
                        span: self.span(start),
                        decl,
                    }
                    .into());
                }
            }

            if self.input_mut().eat(Token::Import) {
                let is_type_only =
                    self.input().is(Token::Type) && peek!(self).is_some_and(|p| p.is_word());

                if is_type_only {
                    self.assert_and_bump(Token::Type);
                }

                let id = self.parse_ident_name()?;

                // export import A = B
                return self
                    .parse_ts_import_equals_decl(
                        start,
                        id.into(),
                        /* is_export */ true,
                        is_type_only,
                    )
                    .map(From::from);
            }

            if self.input_mut().eat(Token::Eq) {
                // `export = x;`
                let expr = self.parse_expr()?;
                self.expect_general_semi()?;
                return Ok(TsExportAssignment {
                    span: self.span(start),
                    expr,
                }
                .into());
            }

            if self.input_mut().eat(Token::As) {
                // `export as namespace A;`
                // See `parseNamespaceExportDeclaration` in TypeScript's own parser
                expect!(self, Token::Namespace);
                let id = self.parse_ident(false, false)?;
                self.expect_general_semi()?;
                return Ok(TsNamespaceExportDecl {
                    span: self.span(start),
                    id,
                }
                .into());
            }
        }

        let ns_export_specifier_start = self.cur_pos();

        let type_only = self.input().syntax().typescript() && self.input_mut().eat(Token::Type);

        // Some("default") if default is exported from 'src'
        let mut export_default = None;

        if !type_only && self.input_mut().eat(Token::Default) {
            if self.input().is(Token::At) {
                let start = self.cur_pos();
                let after_decorators = self.parse_decorators(false)?;

                if !decorators.is_empty() {
                    syntax_error!(self, self.span(start), SyntaxError::TS8038);
                }

                decorators = after_decorators;
            }

            if self.input().syntax().typescript() {
                if self.input().is(Token::Abstract)
                    && peek!(self).is_some_and(|cur| cur == Token::Class)
                    && !self.input_mut().has_linebreak_between_cur_and_peeked()
                {
                    let class_start = self.cur_pos();
                    self.assert_and_bump(Token::Abstract);
                    let cur = self.input().cur();
                    if cur == Token::Error {
                        let err = self.input_mut().expect_error_token_and_bump();
                        return Err(err);
                    }

                    return self
                        .parse_default_class(start, class_start, decorators, true)
                        .map(ModuleDecl::ExportDefaultDecl);
                }
                if self.input().is(Token::Abstract)
                    && peek!(self).is_some_and(|cur| cur == Token::Interface)
                {
                    self.emit_err(self.input().cur_span(), SyntaxError::TS1242);
                    self.assert_and_bump(Token::Abstract);
                }

                if self.input().is(Token::Interface) {
                    let interface_start = self.cur_pos();
                    self.assert_and_bump(Token::Interface);
                    let decl = self
                        .parse_ts_interface_decl(interface_start)
                        .map(DefaultDecl::from)?;
                    return Ok(ExportDefaultDecl {
                        span: self.span(start),
                        decl,
                    }
                    .into());
                }
            }

            if self.input().is(Token::Class) {
                let class_start = self.cur_pos();
                let decl = self.parse_default_class(start, class_start, decorators, false)?;
                return Ok(decl.into());
            } else if self.input().is(Token::Async)
                && peek!(self).is_some_and(|cur| cur == Token::Function)
                && !self.input_mut().has_linebreak_between_cur_and_peeked()
            {
                let decl = self.parse_default_async_fn(start, decorators)?;
                return Ok(decl.into());
            } else if self.input().is(Token::Function) {
                let decl = self.parse_default_fn(start, decorators)?;
                return Ok(decl.into());
            } else if self.input().syntax().export_default_from()
                && ((self.input().is(Token::From)
                    && peek!(self).is_some_and(|peek| peek == Token::Str))
                    || (self.input().is(Token::Comma)
                        && (peek!(self)
                            .is_some_and(|peek| matches!(peek, Token::Asterisk | Token::LBrace)))))
            {
                export_default = Some(Ident::new_no_ctxt(
                    atom!("default"),
                    self.input().prev_span(),
                ))
            } else {
                let expr = self.allow_in_expr(Self::parse_assignment_expr)?;
                self.expect_general_semi()?;
                return Ok(ExportDefaultExpr {
                    span: self.span(start),
                    expr,
                }
                .into());
            }
        }

        if self.input().is(Token::At) {
            let start = self.cur_pos();
            let after_decorators = self.parse_decorators(false)?;

            if !decorators.is_empty() {
                syntax_error!(self, self.span(start), SyntaxError::TS8038);
            }

            decorators = after_decorators;
        }

        let decl = if !type_only && self.input().is(Token::Class) {
            let class_start = self.cur_pos();
            self.parse_class_decl(start, class_start, decorators, false)?
        } else if !type_only
            && self.input().is(Token::Async)
            && peek!(self).is_some_and(|cur| cur == Token::Function)
            && !self.input_mut().has_linebreak_between_cur_and_peeked()
        {
            self.parse_async_fn_decl(decorators)?
        } else if !type_only && self.input().is(Token::Function) {
            self.parse_fn_decl(decorators)?
        } else if !type_only
            && self.input().syntax().typescript()
            && self.input().is(Token::Const)
            && peek!(self).is_some_and(|cur| cur == Token::Enum)
        {
            let enum_start = self.cur_pos();
            self.assert_and_bump(Token::Const);
            self.assert_and_bump(Token::Enum);
            return self
                .parse_ts_enum_decl(enum_start, /* is_const */ true)
                .map(Decl::from)
                .map(|decl| {
                    ExportDecl {
                        span: self.span(start),
                        decl,
                    }
                    .into()
                });
        } else if !type_only
            && (self.input().is(Token::Var)
                || self.input().is(Token::Const)
                || (self.input().is(Token::Let))
                    && peek!(self)
                        .map(|t| t.follows_keyword_let())
                        .unwrap_or(false))
        {
            self.parse_var_stmt(false).map(Decl::Var)?
        } else {
            // ```javascript
            // export foo, * as bar, { baz } from "mod"; // *
            // export      * as bar, { baz } from "mod"; // *
            // export foo,           { baz } from "mod"; // *
            // export foo, * as bar          from "mod"; // *
            // export foo                    from "mod"; // *
            // export      * as bar          from "mod"; //
            // export                { baz } from "mod"; //
            // export                { baz }           ; //
            // export      *                 from "mod"; //
            // ```

            // export default
            // export foo
            let default = match export_default {
                Some(default) => Some(default),
                None => {
                    if self.input().syntax().export_default_from() && self.input().cur().is_word() {
                        Some(self.parse_ident(false, false)?)
                    } else {
                        None
                    }
                }
            };

            if default.is_none()
                && self.input().is(Token::Asterisk)
                && !peek!(self).is_some_and(|cur| cur == Token::As)
            {
                self.assert_and_bump(Token::Asterisk);

                // improve error message for `export * from foo`
                let (src, with) = self.parse_from_clause_and_semi()?;
                return Ok(ExportAll {
                    span: self.span(start),
                    src,
                    type_only,
                    with,
                }
                .into());
            }

            let mut specifiers = Vec::new();

            let mut has_default = false;
            let mut has_ns = false;

            if let Some(default) = default {
                has_default = true;
                specifiers.push(ExportSpecifier::Default(ExportDefaultSpecifier {
                    exported: default,
                }))
            }

            // export foo, * as bar
            //           ^
            if !specifiers.is_empty()
                && self.input().is(Token::Comma)
                && peek!(self).is_some_and(|cur| cur == Token::Asterisk)
            {
                self.assert_and_bump(Token::Comma);

                has_ns = true;
            }
            // export     * as bar
            //            ^
            else if specifiers.is_empty() && self.input().is(Token::Asterisk) {
                has_ns = true;
            }

            if has_ns {
                self.assert_and_bump(Token::Asterisk);
                expect!(self, Token::As);
                let name = self.parse_module_export_name()?;
                specifiers.push(ExportSpecifier::Namespace(ExportNamespaceSpecifier {
                    span: self.span(ns_export_specifier_start),
                    name,
                }));
            }

            if has_default || has_ns {
                if self.input().is(Token::From) {
                    let (src, with) = self.parse_from_clause_and_semi()?;
                    return Ok(NamedExport {
                        span: self.span(start),
                        specifiers,
                        src: Some(src),
                        type_only,
                        with,
                    }
                    .into());
                } else if !self.input().syntax().export_default_from() {
                    // emit error
                    expect!(self, Token::From);
                }

                expect!(self, Token::Comma);
            }

            expect!(self, Token::LBrace);

            while !self.input().is(Token::RBrace) {
                let specifier = self.parse_named_export_specifier(type_only)?;
                specifiers.push(ExportSpecifier::Named(specifier));

                if self.input().is(Token::RBrace) {
                    break;
                } else {
                    expect!(self, Token::Comma);
                }
            }
            expect!(self, Token::RBrace);

            let opt = if self.input().is(Token::From) {
                Some(self.parse_from_clause_and_semi()?)
            } else {
                for s in &specifiers {
                    match s {
                        ExportSpecifier::Default(default) => {
                            self.emit_err(
                                default.exported.span,
                                SyntaxError::ExportExpectFrom(default.exported.sym.clone()),
                            );
                        }
                        ExportSpecifier::Namespace(namespace) => {
                            let export_name = match &namespace.name {
                                ModuleExportName::Ident(i) => i.sym.clone(),
                                ModuleExportName::Str(s) => s.value.to_string_lossy().into(),
                                #[cfg(swc_ast_unknown)]
                                _ => unreachable!(),
                            };
                            self.emit_err(
                                namespace.span,
                                SyntaxError::ExportExpectFrom(export_name),
                            );
                        }
                        ExportSpecifier::Named(named) => match &named.orig {
                            ModuleExportName::Ident(id) if id.is_reserved() => {
                                self.emit_err(
                                    id.span,
                                    SyntaxError::ExportExpectFrom(id.sym.clone()),
                                );
                            }
                            ModuleExportName::Str(s) => {
                                self.emit_err(s.span, SyntaxError::ExportBindingIsString);
                            }
                            _ => {}
                        },
                        #[cfg(swc_ast_unknown)]
                        _ => unreachable!(),
                    }
                }

                self.eat_general_semi();

                None
            };
            let (src, with) = match opt {
                Some(v) => (Some(v.0), v.1),
                None => (None, None),
            };
            return Ok(NamedExport {
                span: self.span(start),
                specifiers,
                src,
                type_only,
                with,
            }
            .into());
        };

        Ok(ExportDecl {
            span: self.span(start),
            decl,
        }
        .into())
    }

    pub(crate) fn parse_import(&mut self) -> PResult<ModuleItem> {
        let start = self.cur_pos();

        if peek!(self).is_some_and(|cur| cur == Token::Dot) {
            let expr = self.parse_expr()?;

            self.eat_general_semi();

            return Ok(ExprStmt {
                span: self.span(start),
                expr,
            }
            .into());
        }

        if peek!(self).is_some_and(|cur| cur == Token::LParen) {
            let expr = self.parse_expr()?;

            self.eat_general_semi();

            return Ok(ExprStmt {
                span: self.span(start),
                expr,
            }
            .into());
        }

        // It's now import statement

        if !self.ctx().contains(Context::Module) {
            // Switch to module mode
            let ctx = self.ctx() | Context::Module | Context::Strict;
            self.set_ctx(ctx);
        }

        expect!(self, Token::Import);

        // Handle import 'mod.js'
        if self.input().cur() == Token::Str {
            let src = Box::new(self.parse_str_lit());
            let with = if self.input().syntax().import_attributes()
                && !self.input().had_line_break_before_cur()
                && (self.input_mut().eat(Token::Assert) || self.input_mut().eat(Token::With))
            {
                match self.parse_object_expr()? {
                    Expr::Object(v) => Some(Box::new(v)),
                    _ => unreachable!(),
                }
            } else {
                None
            };
            self.eat_general_semi();
            return Ok(ImportDecl {
                span: self.span(start),
                src,
                specifiers: Vec::new(),
                type_only: false,
                with,
                phase: Default::default(),
            }
            .into());
        }

        let mut type_only = false;
        let mut phase = ImportPhase::Evaluation;
        let mut specifiers = Vec::with_capacity(4);

        'import_maybe_ident: {
            if self.is_ident_ref() {
                let mut local = self.parse_imported_default_binding()?;

                if self.input().syntax().typescript() && local.sym == "type" {
                    let cur = self.input().cur();
                    if cur == Token::LBrace || cur == Token::Asterisk {
                        type_only = true;
                        break 'import_maybe_ident;
                    }

                    if self.is_ident_ref() {
                        if !self.input().is(Token::From)
                            || peek!(self).is_some_and(|cur| cur == Token::From)
                        {
                            type_only = true;
                            local = self.parse_imported_default_binding()?;
                        } else if peek!(self).is_some_and(|cur| cur == Token::Eq) {
                            type_only = true;
                            local = self.parse_ident_name().map(From::from)?;
                        }
                    }
                }

                if self.input().syntax().typescript() && self.input().is(Token::Eq) {
                    return self
                        .parse_ts_import_equals_decl(start, local, false, type_only)
                        .map(ModuleDecl::from)
                        .map(ModuleItem::from);
                }

                if matches!(&*local.sym, "source" | "defer") {
                    let new_phase = match &*local.sym {
                        "source" => ImportPhase::Source,
                        "defer" => ImportPhase::Defer,
                        _ => unreachable!(),
                    };

                    let cur = self.input().cur();
                    if cur == Token::LBrace || cur == Token::Asterisk {
                        phase = new_phase;
                        break 'import_maybe_ident;
                    }

                    if self.is_ident_ref() && !self.input().is(Token::From)
                        || peek!(self).is_some_and(|cur| cur == Token::From)
                    {
                        // For defer phase, we expect only namespace imports, so break here
                        // and let the subsequent code handle validation
                        if new_phase == ImportPhase::Defer {
                            break 'import_maybe_ident;
                        }
                        phase = new_phase;
                        local = self.parse_imported_default_binding()?;
                    }
                }

                //TODO: Better error reporting
                if !self.input().is(Token::From) {
                    expect!(self, Token::Comma);
                }
                specifiers.push(ImportSpecifier::Default(ImportDefaultSpecifier {
                    span: local.span,
                    local,
                }));
            }
        }

        {
            let import_spec_start = self.cur_pos();
            // Namespace imports are not allowed in source phase.
            if phase != ImportPhase::Source && self.input_mut().eat(Token::Asterisk) {
                expect!(self, Token::As);
                let local = self.parse_imported_binding()?;
                specifiers.push(ImportSpecifier::Namespace(ImportStarAsSpecifier {
                    span: self.span(import_spec_start),
                    local,
                }));
                // Named imports are only allowed in evaluation phase.
            } else if phase == ImportPhase::Evaluation && self.input_mut().eat(Token::LBrace) {
                while !self.input().is(Token::RBrace) {
                    specifiers.push(self.parse_import_specifier(type_only)?);

                    if self.input().is(Token::RBrace) {
                        break;
                    } else {
                        expect!(self, Token::Comma);
                    }
                }
                expect!(self, Token::RBrace);
            }
        }

        let src = {
            expect!(self, Token::From);
            if self.input().cur() == Token::Str {
                Box::new(self.parse_str_lit())
            } else {
                unexpected!(self, "a string literal")
            }
        };

        let with = if self.input().syntax().import_attributes()
            && !self.input().had_line_break_before_cur()
            && (self.input_mut().eat(Token::Assert) || self.input_mut().eat(Token::With))
        {
            match self.parse_object_expr()? {
                Expr::Object(v) => Some(Box::new(v)),
                _ => unreachable!(),
            }
        } else {
            None
        };

        self.expect_general_semi()?;

        Ok(ImportDecl {
            span: self.span(start),
            specifiers,
            src,
            type_only,
            with,
            phase,
        }
        .into())
    }
}

fn handle_import_export<I: Tokens>(
    p: &mut Parser<I>,
    decorators: Vec<Decorator>,
) -> PResult<ModuleItem> {
    if !p
        .ctx()
        .intersects(Context::TopLevel.union(Context::TsModuleBlock))
    {
        syntax_error!(p, SyntaxError::NonTopLevelImportExport);
    }

    let decl = if p.input().is(Token::Import) {
        p.parse_import()?
    } else if p.input().is(Token::Export) {
        p.parse_export(decorators).map(ModuleItem::from)?
    } else {
        unreachable!(
            "handle_import_export should not be called if current token isn't import nor export"
        )
    };

    Ok(decl)
}

#[cfg(test)]
mod tests {
    use crate::{EsSyntax, Syntax};

    #[test]
    fn test_legacy_decorator() {
        crate::test_parser(
            "@foo
export default class Foo {
  bar() {
    class Baz {}
  }
}",
            Syntax::Es(EsSyntax {
                decorators: true,
                decorators_before_export: true,
                ..Default::default()
            }),
            |p| p.parse_module(),
        );
    }
}
