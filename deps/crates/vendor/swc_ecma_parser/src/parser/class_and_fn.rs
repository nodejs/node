use swc_atoms::atom;
use swc_common::{BytePos, Span, Spanned};
use swc_ecma_ast::*;

use crate::{
    error::SyntaxError,
    input::Tokens,
    lexer::Token,
    parser::{
        state::State,
        util::{IsInvalidClassName, IsSimpleParameterList},
    },
    Context, PResult, Parser,
};

struct MakeMethodArgs {
    start: BytePos,
    accessibility: Option<Accessibility>,
    is_abstract: bool,
    static_token: Option<Span>,
    decorators: Vec<Decorator>,
    is_optional: bool,
    is_override: bool,
    key: Key,
    kind: MethodKind,
    is_async: bool,
    is_generator: bool,
}

impl<I: Tokens> Parser<I> {
    /// If `required` is `true`, this never returns `None`.
    fn parse_maybe_opt_binding_ident(
        &mut self,
        required: bool,
        disallow_let: bool,
    ) -> PResult<Option<Ident>> {
        if required {
            self.parse_binding_ident(disallow_let)
                .map(|v| v.id)
                .map(Some)
        } else {
            self.parse_opt_binding_ident(disallow_let)
                .map(|v| v.map(|v| v.id))
        }
    }

    fn parse_maybe_decorator_args(&mut self, expr: Box<Expr>) -> PResult<Box<Expr>> {
        let type_args = if self.input().syntax().typescript() && self.input().is(Token::Lt) {
            let ret = self.parse_ts_type_args()?;
            self.assert_and_bump(Token::Gt);
            Some(ret)
        } else {
            None
        };

        if type_args.is_none() && !self.input().is(Token::LParen) {
            return Ok(expr);
        }

        let args = self.parse_args(false)?;
        Ok(CallExpr {
            span: self.span(expr.span_lo()),
            callee: Callee::Expr(expr),
            args,
            ..Default::default()
        }
        .into())
    }

    pub(crate) fn parse_decorators(&mut self, allow_export: bool) -> PResult<Vec<Decorator>> {
        if !self.syntax().decorators() {
            return Ok(Vec::new());
        }
        trace_cur!(self, parse_decorators);

        let mut decorators = Vec::new();
        let start = self.cur_pos();

        while self.input().is(Token::At) {
            decorators.push(self.parse_decorator()?);
        }
        if decorators.is_empty() {
            return Ok(decorators);
        }

        if self.input().is(Token::Export) {
            if !self.ctx().contains(Context::InClass)
                && !self.ctx().contains(Context::InFunction)
                && !allow_export
            {
                syntax_error!(self, self.input().cur_span(), SyntaxError::ExportNotAllowed);
            }

            if !self.ctx().contains(Context::InClass)
                && !self.ctx().contains(Context::InFunction)
                && !self.syntax().decorators_before_export()
            {
                syntax_error!(self, self.span(start), SyntaxError::DecoratorOnExport);
            }
        } else if !self.input().is(Token::Class) {
            // syntax_error!(p, self.span(start),
            // SyntaxError::InvalidLeadingDecorator)
        }

        Ok(decorators)
    }

    fn parse_decorator(&mut self) -> PResult<Decorator> {
        let start = self.cur_pos();
        trace_cur!(self, parse_decorator);

        self.assert_and_bump(Token::At);

        let expr = if self.input_mut().eat(Token::LParen) {
            let expr = self.parse_expr()?;
            expect!(self, Token::RParen);
            expr
        } else {
            let expr = self
                .parse_ident(false, false)
                .map(Expr::from)
                .map(Box::new)?;

            self.parse_subscripts(Callee::Expr(expr), false, true)?
        };

        let expr = self.parse_maybe_decorator_args(expr)?;

        Ok(Decorator {
            span: self.span(start),
            expr,
        })
    }

    pub(crate) fn parse_access_modifier(&mut self) -> PResult<Option<Accessibility>> {
        Ok(self
            .parse_ts_modifier(&["public", "protected", "private", "in", "out"], false)?
            .and_then(|s| match s {
                "public" => Some(Accessibility::Public),
                "protected" => Some(Accessibility::Protected),
                "private" => Some(Accessibility::Private),
                other => {
                    self.emit_err(self.input().prev_span(), SyntaxError::TS1274(other.into()));
                    None
                }
            }))
    }

    fn parse_super_class(&mut self) -> PResult<(Box<Expr>, Option<Box<TsTypeParamInstantiation>>)> {
        let super_class = self.parse_lhs_expr()?;
        match *super_class {
            Expr::TsInstantiation(TsInstantiation {
                expr, type_args, ..
            }) => Ok((expr, Some(type_args))),
            _ => {
                // We still need to parse TS type arguments,
                // because in some cases "super class" returned by `parse_lhs_expr`
                // may not include `TsExprWithTypeArgs`
                // but it's a super class with type params, for example, in JSX.
                if self.syntax().typescript() && self.input().is(Token::Lt) {
                    let ret = self.parse_ts_type_args()?;
                    self.assert_and_bump(Token::Gt);
                    Ok((super_class, Some(ret)))
                } else {
                    Ok((super_class, None))
                }
            }
        }
    }

    fn is_class_method(&mut self) -> bool {
        let cur = self.input().cur();
        cur == Token::LParen
            || (self.input().syntax().typescript()
                && (cur == Token::Lt || cur == Token::JSXTagStart))
    }

    fn is_class_property(&mut self, asi: bool) -> bool {
        let cur = self.input().cur();
        (self.input().syntax().typescript() && (cur == Token::Bang || cur == Token::Colon))
            || (cur == Token::Eq || cur == Token::RBrace)
            || if asi {
                self.is_general_semi()
            } else {
                self.input().is(Token::Semi)
            }
    }

    fn parse_class_prop_name(&mut self) -> PResult<Key> {
        if self.input().is(Token::Hash) {
            let name = self.parse_private_name()?;
            if name.name == "constructor" {
                self.emit_err(name.span, SyntaxError::PrivateConstructor);
            }
            Ok(Key::Private(name))
        } else {
            self.parse_prop_name().map(Key::Public)
        }
    }

    /// `parse_args` closure should not eat '(' or ')'.
    pub(crate) fn parse_fn_args_body<F>(
        &mut self,
        decorators: Vec<Decorator>,
        start: BytePos,
        parse_args: F,
        is_async: bool,
        is_generator: bool,
    ) -> PResult<Box<Function>>
    where
        F: FnOnce(&mut Self) -> PResult<Vec<Param>>,
    {
        trace_cur!(self, parse_fn_args_body);
        let f = |p: &mut Self| {
            let type_params = if p.syntax().typescript() {
                p.in_type(|p| {
                    trace_cur!(p, parse_fn_args_body__type_params);

                    Ok(
                        if p.input().is(Token::Lt) || p.input().is(Token::JSXTagStart) {
                            Some(p.parse_ts_type_params(false, true)?)
                        } else {
                            None
                        },
                    )
                })?
            } else {
                None
            };

            expect!(p, Token::LParen);

            let parse_args_with_generator_ctx = |p: &mut Self| {
                if is_generator {
                    p.do_inside_of_context(Context::InGenerator, parse_args)
                } else {
                    p.do_outside_of_context(Context::InGenerator, parse_args)
                }
            };

            let params = p.do_inside_of_context(Context::InParameters, |p| {
                p.do_outside_of_context(Context::InFunction, |p| {
                    if is_async {
                        p.do_inside_of_context(Context::InAsync, parse_args_with_generator_ctx)
                    } else {
                        p.do_outside_of_context(Context::InAsync, parse_args_with_generator_ctx)
                    }
                })
            })?;

            expect!(p, Token::RParen);

            // typescript extension
            let return_type = if p.syntax().typescript() && p.input().is(Token::Colon) {
                p.parse_ts_type_or_type_predicate_ann(Token::Colon)
                    .map(Some)?
            } else {
                None
            };

            let body: Option<_> = p.parse_fn_block_body(
                is_async,
                is_generator,
                false,
                params.is_simple_parameter_list(),
            )?;

            if p.syntax().typescript() && body.is_none() {
                // Declare functions cannot have assignment pattern in parameters
                for param in &params {
                    // TODO: Search deeply for assignment pattern using a Visitor

                    let span = match &param.pat {
                        Pat::Assign(ref p) => Some(p.span()),
                        _ => None,
                    };

                    if let Some(span) = span {
                        p.emit_err(span, SyntaxError::TS2371)
                    }
                }
            }

            Ok(Box::new(Function {
                span: p.span(start),
                decorators,
                type_params,
                params,
                body,
                is_async,
                is_generator,
                return_type,
                ctxt: Default::default(),
            }))
        };

        let f_with_generator_ctx = |p: &mut Self| {
            if is_generator {
                p.do_inside_of_context(Context::InGenerator, f)
            } else {
                p.do_outside_of_context(Context::InGenerator, f)
            }
        };

        if is_async {
            self.do_inside_of_context(Context::InAsync, f_with_generator_ctx)
        } else {
            self.do_outside_of_context(Context::InAsync, f_with_generator_ctx)
        }
    }

    pub(crate) fn parse_async_fn_expr(&mut self) -> PResult<Box<Expr>> {
        let start = self.cur_pos();
        expect!(self, Token::Async);
        self.parse_fn(None, Some(start), Vec::new())
    }

    /// Parse function expression
    pub(crate) fn parse_fn_expr(&mut self) -> PResult<Box<Expr>> {
        self.parse_fn(None, None, Vec::new())
    }

    pub(crate) fn parse_async_fn_decl(&mut self, decorators: Vec<Decorator>) -> PResult<Decl> {
        let start = self.cur_pos();
        expect!(self, Token::Async);
        self.parse_fn(None, Some(start), decorators)
    }

    pub(crate) fn parse_fn_decl(&mut self, decorators: Vec<Decorator>) -> PResult<Decl> {
        self.parse_fn(None, None, decorators)
    }

    pub(crate) fn parse_default_async_fn(
        &mut self,
        start: BytePos,
        decorators: Vec<Decorator>,
    ) -> PResult<ExportDefaultDecl> {
        let start_of_async = self.cur_pos();
        expect!(self, Token::Async);
        self.parse_fn(Some(start), Some(start_of_async), decorators)
    }

    pub(crate) fn parse_default_fn(
        &mut self,
        start: BytePos,
        decorators: Vec<Decorator>,
    ) -> PResult<ExportDefaultDecl> {
        self.parse_fn(Some(start), None, decorators)
    }

    fn parse_fn_inner(
        &mut self,
        _start_of_output_type: Option<BytePos>,
        start_of_async: Option<BytePos>,
        decorators: Vec<Decorator>,
        is_fn_expr: bool,
        is_ident_required: bool,
    ) -> PResult<(Option<Ident>, Box<Function>)> {
        let start = start_of_async.unwrap_or_else(|| self.cur_pos());
        self.assert_and_bump(Token::Function);
        let is_async = start_of_async.is_some();

        let is_generator = self.input_mut().eat(Token::Asterisk);

        let ident = if is_fn_expr {
            let f_with_generator_context = |p: &mut Self| {
                if is_generator {
                    p.do_inside_of_context(Context::InGenerator, |p| {
                        p.parse_maybe_opt_binding_ident(is_ident_required, false)
                    })
                } else {
                    p.do_outside_of_context(Context::InGenerator, |p| {
                        p.parse_maybe_opt_binding_ident(is_ident_required, false)
                    })
                }
            };

            self.do_outside_of_context(
                Context::AllowDirectSuper.union(Context::InClassField),
                |p| {
                    if is_async {
                        p.do_inside_of_context(Context::InAsync, f_with_generator_context)
                    } else {
                        p.do_outside_of_context(Context::InAsync, f_with_generator_context)
                    }
                },
            )?
        } else {
            // function declaration does not change context for `BindingIdentifier`.
            self.do_outside_of_context(
                Context::AllowDirectSuper.union(Context::InClassField),
                |p| p.parse_maybe_opt_binding_ident(is_ident_required, false),
            )?
        };

        self.do_outside_of_context(
            Context::AllowDirectSuper
                .union(Context::InClassField)
                .union(Context::WillExpectColonForCond),
            |p| {
                let f = p.parse_fn_args_body(
                    decorators,
                    start,
                    Self::parse_formal_params,
                    is_async,
                    is_generator,
                )?;
                if is_fn_expr && f.body.is_none() {
                    unexpected!(p, "{");
                }
                Ok((ident, f))
            },
        )
    }

    fn parse_fn<T>(
        &mut self,
        start_of_output_type: Option<BytePos>,
        start_of_async: Option<BytePos>,
        decorators: Vec<Decorator>,
    ) -> PResult<T>
    where
        T: OutputType,
    {
        let start = start_of_async.unwrap_or_else(|| self.cur_pos());
        let (ident, f) = self.parse_fn_inner(
            start_of_output_type,
            start_of_async,
            decorators,
            T::is_fn_expr(),
            T::IS_IDENT_REQUIRED,
        )?;

        match T::finish_fn(self.span(start_of_output_type.unwrap_or(start)), ident, f) {
            Ok(v) => Ok(v),
            Err(kind) => syntax_error!(self, kind),
        }
    }

    pub(crate) fn parse_class_decl(
        &mut self,
        start: BytePos,
        class_start: BytePos,
        decorators: Vec<Decorator>,
        is_abstract: bool,
    ) -> PResult<Decl> {
        self.parse_class(start, class_start, decorators, is_abstract)
    }

    pub(crate) fn parse_class_expr(
        &mut self,
        start: BytePos,
        decorators: Vec<Decorator>,
    ) -> PResult<Box<Expr>> {
        self.parse_class(start, start, decorators, false)
    }

    pub(crate) fn parse_default_class(
        &mut self,
        start: BytePos,
        class_start: BytePos,
        decorators: Vec<Decorator>,
        is_abstract: bool,
    ) -> PResult<ExportDefaultDecl> {
        self.parse_class(start, class_start, decorators, is_abstract)
    }

    fn make_method<F>(
        &mut self,
        parse_args: F,
        MakeMethodArgs {
            start,
            accessibility,
            is_abstract,
            static_token,
            decorators,
            is_optional,
            is_override,
            key,
            kind,
            is_async,
            is_generator,
        }: MakeMethodArgs,
    ) -> PResult<ClassMember>
    where
        F: FnOnce(&mut Self) -> PResult<Vec<Param>>,
    {
        trace_cur!(self, make_method);

        let is_static = static_token.is_some();
        let function = self.do_inside_of_context(Context::AllowDirectSuper, |p| {
            p.do_outside_of_context(Context::InClassField, |p| {
                p.parse_fn_args_body(decorators, start, parse_args, is_async, is_generator)
            })
        })?;

        match kind {
            MethodKind::Getter | MethodKind::Setter
                if self.input().syntax().typescript()
                    && self.input().target() == EsVersion::Es3 =>
            {
                self.emit_err(key.span(), SyntaxError::TS1056);
            }
            _ => {}
        }

        match key {
            Key::Private(key) => {
                let span = self.span(start);
                if accessibility.is_some() {
                    self.emit_err(span.with_hi(key.span_hi()), SyntaxError::TS18010);
                }

                Ok(PrivateMethod {
                    span,

                    accessibility,
                    is_abstract,
                    is_optional,
                    is_override,

                    is_static,
                    key,
                    function,
                    kind,
                }
                .into())
            }
            Key::Public(key) => {
                let span = self.span(start);
                if is_abstract && function.body.is_some() {
                    self.emit_err(span, SyntaxError::TS1245)
                }
                Ok(ClassMethod {
                    span,

                    accessibility,
                    is_abstract,
                    is_optional,
                    is_override,

                    is_static,
                    key,
                    function,
                    kind,
                }
                .into())
            }
            #[cfg(swc_ast_unknown)]
            _ => unreachable!(),
        }
    }

    pub(crate) fn parse_fn_block_or_expr_body(
        &mut self,
        is_async: bool,
        is_generator: bool,
        is_arrow_function: bool,
        is_simple_parameter_list: bool,
    ) -> PResult<Box<BlockStmtOrExpr>> {
        self.parse_fn_body(
            is_async,
            is_generator,
            is_arrow_function,
            is_simple_parameter_list,
            |p, is_simple_parameter_list| {
                if p.input().is(Token::LBrace) {
                    p.parse_block(false)
                        .map(|block_stmt| {
                            if !is_simple_parameter_list {
                                if let Some(span) = has_use_strict(&block_stmt) {
                                    p.emit_err(span, SyntaxError::IllegalLanguageModeDirective);
                                }
                            }
                            BlockStmtOrExpr::BlockStmt(block_stmt)
                        })
                        .map(Box::new)
                } else {
                    p.parse_assignment_expr()
                        .map(BlockStmtOrExpr::Expr)
                        .map(Box::new)
                }
            },
        )
    }

    fn parse_fn_body<T>(
        &mut self,
        is_async: bool,
        is_generator: bool,
        is_arrow_function: bool,
        is_simple_parameter_list: bool,
        f: impl FnOnce(&mut Self, bool) -> PResult<T>,
    ) -> PResult<T> {
        if self.ctx().contains(Context::InDeclare)
            && self.syntax().typescript()
            && self.input().is(Token::LBrace)
        {
            //            self.emit_err(
            //                self.ctx().span_of_fn_name.expect("we are not in function"),
            //                SyntaxError::TS1183,
            //            );
            self.emit_err(self.input().cur_span(), SyntaxError::TS1183);
        }

        let f_with_generator_context = |p: &mut Self| {
            let f_with_inside_non_arrow_fn_scope = |p: &mut Self| {
                let f_with_new_state = |p: &mut Self| {
                    let mut p = p.with_state(State::default());
                    f(&mut p, is_simple_parameter_list)
                };

                if is_arrow_function && !p.ctx().contains(Context::InsideNonArrowFunctionScope) {
                    p.do_outside_of_context(Context::InsideNonArrowFunctionScope, f_with_new_state)
                } else {
                    p.do_inside_of_context(Context::InsideNonArrowFunctionScope, f_with_new_state)
                }
            };

            if is_generator {
                p.do_inside_of_context(Context::InGenerator, f_with_inside_non_arrow_fn_scope)
            } else {
                p.do_outside_of_context(Context::InGenerator, f_with_inside_non_arrow_fn_scope)
            }
        };

        self.do_inside_of_context(Context::InFunction, |p| {
            p.do_outside_of_context(
                Context::InStaticBlock
                    .union(Context::IsBreakAllowed)
                    .union(Context::IsContinueAllowed)
                    .union(Context::TopLevel),
                |p| {
                    if is_async {
                        p.do_inside_of_context(Context::InAsync, f_with_generator_context)
                    } else {
                        p.do_outside_of_context(Context::InAsync, f_with_generator_context)
                    }
                },
            )
        })
    }

    pub(super) fn parse_fn_block_body(
        &mut self,
        is_async: bool,
        is_generator: bool,
        is_arrow_function: bool,
        is_simple_parameter_list: bool,
    ) -> PResult<Option<BlockStmt>> {
        self.parse_fn_body(
            is_async,
            is_generator,
            is_arrow_function,
            is_simple_parameter_list,
            |p, is_simple_parameter_list| {
                // allow omitting body and allow placing `{` on next line
                if p.input().syntax().typescript()
                    && !p.input().is(Token::LBrace)
                    && p.eat_general_semi()
                {
                    return Ok(None);
                }
                p.allow_in_expr(|p| p.parse_block(true)).map(|block_stmt| {
                    if !is_simple_parameter_list {
                        if let Some(span) = has_use_strict(&block_stmt) {
                            p.emit_err(span, SyntaxError::IllegalLanguageModeDirective);
                        }
                    }
                    Some(block_stmt)
                })
            },
        )
    }

    fn make_property(
        &mut self,
        start: BytePos,
        decorators: Vec<Decorator>,
        accessibility: Option<Accessibility>,
        key: Key,
        is_static: bool,
        accessor_token: Option<Span>,
        is_optional: bool,
        readonly: bool,
        declare: bool,
        is_abstract: bool,
        is_override: bool,
    ) -> PResult<ClassMember> {
        if is_constructor(&key) {
            syntax_error!(self, key.span(), SyntaxError::PropertyNamedConstructor);
        }
        if key.is_private() {
            if declare {
                self.emit_err(
                    key.span(),
                    SyntaxError::PrivateNameModifier(atom!("declare")),
                )
            }
            if is_abstract {
                self.emit_err(
                    key.span(),
                    SyntaxError::PrivateNameModifier(atom!("abstract")),
                )
            }
        }
        let definite =
            self.input().syntax().typescript() && !is_optional && self.input_mut().eat(Token::Bang);

        let type_ann = self.try_parse_ts_type_ann()?;

        self.do_inside_of_context(Context::IncludeInExpr.union(Context::InClassField), |p| {
            let value = if p.input().is(Token::Eq) {
                p.assert_and_bump(Token::Eq);
                Some(p.parse_assignment_expr()?)
            } else {
                None
            };

            if !p.eat_general_semi() {
                p.emit_err(p.input().cur_span(), SyntaxError::TS1005);
            }

            if accessor_token.is_some() {
                return Ok(ClassMember::AutoAccessor(AutoAccessor {
                    span: p.span(start),
                    key,
                    value,
                    type_ann,
                    is_static,
                    decorators,
                    accessibility,
                    is_abstract,
                    is_override,
                    definite,
                }));
            }

            Ok(match key {
                Key::Private(key) => {
                    let span = p.span(start);
                    if accessibility.is_some() {
                        p.emit_err(span.with_hi(key.span_hi()), SyntaxError::TS18010);
                    }

                    PrivateProp {
                        span: p.span(start),
                        key,
                        value,
                        is_static,
                        decorators,
                        accessibility,
                        is_optional,
                        is_override,
                        readonly,
                        type_ann,
                        definite,
                        ctxt: Default::default(),
                    }
                    .into()
                }
                Key::Public(key) => {
                    let span = p.span(start);
                    if is_abstract && value.is_some() {
                        p.emit_err(span, SyntaxError::TS1267)
                    }
                    ClassProp {
                        span,
                        key,
                        value,
                        is_static,
                        decorators,
                        accessibility,
                        is_abstract,
                        is_optional,
                        is_override,
                        readonly,
                        declare,
                        definite,
                        type_ann,
                    }
                    .into()
                }
                #[cfg(swc_ast_unknown)]
                _ => unreachable!(),
            })
        })
    }

    fn parse_static_block(&mut self, start: BytePos) -> PResult<ClassMember> {
        let body = self.do_inside_of_context(
            Context::InStaticBlock
                .union(Context::InClassField)
                .union(Context::AllowUsingDecl),
            |p| p.parse_block(false),
        )?;

        let span = self.span(start);
        Ok(StaticBlock { span, body }.into())
    }

    fn parse_class_member_with_is_static(
        &mut self,
        start: BytePos,
        declare_token: Option<Span>,
        accessibility: Option<Accessibility>,
        static_token: Option<Span>,
        accessor_token: Option<Span>,
        decorators: Vec<Decorator>,
    ) -> PResult<ClassMember> {
        let mut is_static = static_token.is_some();

        let mut is_abstract = false;
        let mut is_override = false;
        let mut readonly = None;
        let mut modifier_span = None;
        let declare = declare_token.is_some();
        while let Some(modifier) = if self.input().syntax().typescript() {
            self.parse_ts_modifier(&["abstract", "readonly", "override", "static"], true)?
        } else {
            None
        } {
            modifier_span = Some(self.input().prev_span());
            match modifier {
                "abstract" => {
                    if is_abstract {
                        self.emit_err(
                            self.input().prev_span(),
                            SyntaxError::TS1030(atom!("abstract")),
                        );
                    } else if is_override {
                        self.emit_err(
                            self.input().prev_span(),
                            SyntaxError::TS1029(atom!("abstract"), atom!("override")),
                        );
                    }
                    is_abstract = true;
                }
                "override" => {
                    if is_override {
                        self.emit_err(
                            self.input().prev_span(),
                            SyntaxError::TS1030(atom!("override")),
                        );
                    } else if readonly.is_some() {
                        self.emit_err(
                            self.input().prev_span(),
                            SyntaxError::TS1029(atom!("override"), atom!("readonly")),
                        );
                    } else if declare {
                        self.emit_err(
                            self.input().prev_span(),
                            SyntaxError::TS1243(atom!("override"), atom!("declare")),
                        );
                    } else if !self.ctx().contains(Context::HasSuperClass) {
                        self.emit_err(self.input().prev_span(), SyntaxError::TS4112);
                    }
                    is_override = true;
                }
                "readonly" => {
                    let readonly_span = self.input().prev_span();
                    if readonly.is_some() {
                        self.emit_err(readonly_span, SyntaxError::TS1030(atom!("readonly")));
                    } else {
                        readonly = Some(readonly_span);
                    }
                }
                "static" => {
                    if is_override {
                        self.emit_err(
                            self.input().prev_span(),
                            SyntaxError::TS1029(atom!("static"), atom!("override")),
                        );
                    }

                    is_static = true;
                }
                _ => {}
            }
        }

        let accessor_token = accessor_token.or_else(|| {
            if self.syntax().auto_accessors() && readonly.is_none() {
                let start = self.cur_pos();
                if !peek!(self).is_some_and(|cur| cur == Token::LParen)
                    && self.input_mut().eat(Token::Accessor)
                {
                    Some(self.span(start))
                } else {
                    None
                }
            } else {
                None
            }
        });

        if is_static && self.input().is(Token::LBrace) {
            if let Some(span) = declare_token {
                self.emit_err(span, SyntaxError::TS1184);
            }
            if accessibility.is_some() {
                self.emit_err(self.input().cur_span(), SyntaxError::TS1184);
            }
            return self.parse_static_block(start);
        }
        if self.input().is(Token::Static) && peek!(self).is_some_and(|cur| cur == Token::LBrace) {
            // For "readonly", "abstract" and "override"
            if let Some(span) = modifier_span {
                self.emit_err(span, SyntaxError::TS1184);
            }
            if let Some(span) = static_token {
                self.emit_err(span, SyntaxError::TS1184);
            }
            self.bump(); // consume "static"
            return self.parse_static_block(start);
        }

        if self.input().syntax().typescript()
            && !is_abstract
            && !is_override
            && accessibility.is_none()
        {
            let idx = self.try_parse_ts_index_signature(start, readonly.is_some(), is_static)?;
            if let Some(idx) = idx {
                return Ok(idx.into());
            }
        }

        if self.input_mut().eat(Token::Asterisk) {
            // generator method
            let key = self.parse_class_prop_name()?;
            if readonly.is_some() {
                self.emit_err(self.span(start), SyntaxError::ReadOnlyMethod);
            }
            if is_constructor(&key) {
                self.emit_err(self.span(start), SyntaxError::GeneratorConstructor);
            }

            return self.make_method(
                Self::parse_unique_formal_params,
                MakeMethodArgs {
                    start,
                    decorators,
                    is_async: false,
                    is_generator: true,
                    accessibility,
                    is_abstract,
                    is_override,
                    is_optional: false,
                    static_token,
                    key,
                    kind: MethodKind::Method,
                },
            );
        }

        trace_cur!(self, parse_class_member_with_is_static__normal_class_member);
        let key = if readonly.is_some() && matches!(self.input().cur(), Token::Bang | Token::Colon)
        {
            Key::Public(PropName::Ident(IdentName::new(
                atom!("readonly"),
                readonly.unwrap(),
            )))
        } else {
            self.parse_class_prop_name()?
        };
        let is_optional =
            self.input().syntax().typescript() && self.input_mut().eat(Token::QuestionMark);

        if self.is_class_method() {
            // handle a(){} / get(){} / set(){} / async(){}

            trace_cur!(self, parse_class_member_with_is_static__normal_class_method);

            if let Some(token) = declare_token {
                self.emit_err(token, SyntaxError::TS1031)
            }

            if readonly.is_some() {
                syntax_error!(self, self.span(start), SyntaxError::ReadOnlyMethod);
            }
            let is_constructor = is_constructor(&key);

            if is_constructor {
                if self.syntax().typescript() && is_override {
                    self.emit_err(self.span(start), SyntaxError::TS1089(atom!("override")));
                }

                if self.syntax().typescript() && self.input().is(Token::Lt) {
                    let start = self.cur_pos();
                    if peek!(self).is_some_and(|cur| cur == Token::Lt) {
                        self.assert_and_bump(Token::Lt);
                        let start2 = self.cur_pos();
                        self.assert_and_bump(Token::Gt);

                        self.emit_err(self.span(start), SyntaxError::TS1098);
                        self.emit_err(self.span(start2), SyntaxError::TS1092);
                    } else {
                        let type_params = self.try_parse_ts_type_params(false, true)?;

                        if let Some(type_params) = type_params {
                            for param in type_params.params {
                                self.emit_err(param.span(), SyntaxError::TS1092);
                            }
                        }
                    }
                }

                expect!(self, Token::LParen);
                let params = self.parse_constructor_params()?;
                expect!(self, Token::RParen);

                if self.syntax().typescript() && self.input().is(Token::Colon) {
                    let start = self.cur_pos();
                    let type_ann = self.parse_ts_type_ann(true, start)?;

                    self.emit_err(type_ann.type_ann.span(), SyntaxError::TS1093);
                }

                let body: Option<_> = self.parse_fn_block_body(
                    false,
                    false,
                    false,
                    params.is_simple_parameter_list(),
                )?;

                if body.is_none() {
                    for param in params.iter() {
                        if param.is_ts_param_prop() {
                            self.emit_err(param.span(), SyntaxError::TS2369)
                        }
                    }
                }

                if self.syntax().typescript() && body.is_none() {
                    // Declare constructors cannot have assignment pattern in parameters
                    for param in &params {
                        // TODO: Search deeply for assignment pattern using a Visitor

                        let span = match *param {
                            ParamOrTsParamProp::Param(ref param) => match param.pat {
                                Pat::Assign(ref p) => Some(p.span()),
                                _ => None,
                            },
                            ParamOrTsParamProp::TsParamProp(TsParamProp {
                                param: TsParamPropParam::Assign(ref p),
                                ..
                            }) => Some(p.span()),
                            _ => None,
                        };

                        if let Some(span) = span {
                            self.emit_err(span, SyntaxError::TS2371)
                        }
                    }
                }

                if let Some(static_token) = static_token {
                    self.emit_err(static_token, SyntaxError::TS1089(atom!("static")))
                }

                if let Some(span) = modifier_span {
                    if is_abstract {
                        self.emit_err(span, SyntaxError::TS1242);
                    }
                }

                return Ok(ClassMember::Constructor(Constructor {
                    span: self.span(start),
                    accessibility,
                    key: match key {
                        Key::Public(key) => key,
                        _ => unreachable!("is_constructor() returns false for PrivateName"),
                    },
                    is_optional,
                    params,
                    body,
                    ..Default::default()
                }));
            } else {
                return self.make_method(
                    Self::parse_formal_params,
                    MakeMethodArgs {
                        start,
                        is_optional,
                        accessibility,
                        decorators,
                        is_abstract,
                        is_override,
                        static_token,
                        kind: MethodKind::Method,
                        key,
                        is_async: false,
                        is_generator: false,
                    },
                );
            }
        }

        let is_next_line_generator =
            self.input_mut().had_line_break_before_cur() && self.input().is(Token::Asterisk);
        let getter_or_setter_ident = match key {
            // `get\n*` is an uninitialized property named 'get' followed by a generator.
            Key::Public(PropName::Ident(ref i))
                if (i.sym == "get" || i.sym == "set")
                    && !self.is_class_property(/* asi */ false)
                    && !is_next_line_generator =>
            {
                Some(i)
            }
            _ => None,
        };

        if getter_or_setter_ident.is_none() && self.is_class_property(/* asi */ true) {
            return self.make_property(
                start,
                decorators,
                accessibility,
                key,
                is_static,
                accessor_token,
                is_optional,
                readonly.is_some(),
                declare,
                is_abstract,
                is_override,
            );
        }

        if match key {
            Key::Public(PropName::Ident(ref i)) => i.sym == "async",
            _ => false,
        } && !self.input_mut().had_line_break_before_cur()
        {
            // handle async foo(){}

            if self.input().syntax().typescript()
                && self.parse_ts_modifier(&["override"], false)?.is_some()
            {
                is_override = true;
                self.emit_err(
                    self.input().prev_span(),
                    SyntaxError::TS1029(atom!("override"), atom!("async")),
                );
            }

            let is_generator = self.input_mut().eat(Token::Asterisk);
            let key = self.parse_class_prop_name()?;
            if is_constructor(&key) {
                syntax_error!(self, key.span(), SyntaxError::AsyncConstructor)
            }
            if readonly.is_some() {
                syntax_error!(self, self.span(start), SyntaxError::ReadOnlyMethod);
            }

            // handle async foo(){}
            let is_optional = is_optional
                || self.input().syntax().typescript() && self.input_mut().eat(Token::QuestionMark);
            return self.make_method(
                Self::parse_unique_formal_params,
                MakeMethodArgs {
                    start,
                    static_token,
                    key,
                    is_abstract,
                    accessibility,
                    is_optional,
                    is_override,
                    decorators,
                    kind: MethodKind::Method,
                    is_async: true,
                    is_generator,
                },
            );
        }

        if let Some(i) = getter_or_setter_ident {
            let key_span = key.span();

            // handle get foo(){} / set foo(v){}
            let key = self.parse_class_prop_name()?;

            if readonly.is_some() {
                self.emit_err(key_span, SyntaxError::GetterSetterCannotBeReadonly);
            }

            if is_constructor(&key) {
                self.emit_err(key_span, SyntaxError::ConstructorAccessor);
            }

            return match &*i.sym {
                "get" => self.make_method(
                    |p| {
                        let params = p.parse_formal_params()?;

                        if params.iter().any(is_not_this) {
                            p.emit_err(key_span, SyntaxError::GetterParam);
                        }

                        Ok(params)
                    },
                    MakeMethodArgs {
                        decorators,
                        start,
                        is_abstract,
                        is_async: false,
                        is_generator: false,
                        is_optional,
                        is_override,
                        accessibility,
                        static_token,
                        key,
                        kind: MethodKind::Getter,
                    },
                ),
                "set" => self.make_method(
                    |p| {
                        let params = p.parse_formal_params()?;

                        if params.iter().filter(|p| is_not_this(p)).count() != 1 {
                            p.emit_err(key_span, SyntaxError::SetterParam);
                        }

                        if !params.is_empty() {
                            if let Pat::Rest(..) = params[0].pat {
                                p.emit_err(params[0].pat.span(), SyntaxError::RestPatInSetter);
                            }
                        }

                        Ok(params)
                    },
                    MakeMethodArgs {
                        decorators,
                        start,
                        is_optional,
                        is_abstract,
                        is_override,
                        is_async: false,
                        is_generator: false,
                        accessibility,
                        static_token,
                        key,
                        kind: MethodKind::Setter,
                    },
                ),
                _ => unreachable!(),
            };
        }

        unexpected!(self, "* for generator, private key, identifier or async")
    }

    fn parse_class_member(&mut self) -> PResult<ClassMember> {
        trace_cur!(self, parse_class_member);

        let start = self.cur_pos();
        let decorators = self.parse_decorators(false)?;
        let declare = self.syntax().typescript() && self.input_mut().eat(Token::Declare);
        let accessibility = if self.input().syntax().typescript() {
            self.parse_access_modifier()?
        } else {
            None
        };
        // Allow `private declare`.
        let declare = declare || self.syntax().typescript() && self.input_mut().eat(Token::Declare);

        let declare_token = if declare {
            // Handle declare(){}
            if self.is_class_method() {
                let key = Key::Public(PropName::Ident(IdentName::new(
                    atom!("declare"),
                    self.span(start),
                )));
                let is_optional =
                    self.input().syntax().typescript() && self.input_mut().eat(Token::QuestionMark);
                return self.make_method(
                    Self::parse_unique_formal_params,
                    MakeMethodArgs {
                        start,
                        accessibility,
                        decorators,
                        is_abstract: false,
                        is_optional,
                        is_override: false,
                        is_async: false,
                        is_generator: false,
                        static_token: None,
                        key,
                        kind: MethodKind::Method,
                    },
                );
            } else if self.is_class_property(/* asi */ true)
                || (self.syntax().typescript() && self.input().is(Token::QuestionMark))
            {
                // Property named `declare`

                let key = Key::Public(PropName::Ident(IdentName::new(
                    atom!("declare"),
                    self.span(start),
                )));
                let is_optional =
                    self.input().syntax().typescript() && self.input_mut().eat(Token::QuestionMark);
                return self.make_property(
                    start,
                    decorators,
                    accessibility,
                    key,
                    false,
                    None,
                    is_optional,
                    false,
                    false,
                    false,
                    false,
                );
            } else {
                Some(self.span(start))
            }
        } else {
            None
        };

        let static_token = {
            let start = self.cur_pos();
            if self.input_mut().eat(Token::Static) {
                Some(self.span(start))
            } else {
                None
            }
        };

        let accessor_token = if self.syntax().auto_accessors() {
            let start = self.cur_pos();
            if self.input_mut().eat(Token::Accessor) {
                Some(self.span(start))
            } else {
                None
            }
        } else {
            None
        };

        if let Some(accessor_token) = accessor_token {
            // Handle accessor(){}
            if self.is_class_method() {
                let key = Key::Public(PropName::Ident(IdentName::new(
                    atom!("accessor"),
                    accessor_token,
                )));
                let is_optional =
                    self.input().syntax().typescript() && self.input_mut().eat(Token::QuestionMark);
                return self.make_method(
                    Self::parse_unique_formal_params,
                    MakeMethodArgs {
                        start,
                        accessibility,
                        decorators,
                        is_abstract: false,
                        is_optional,
                        is_override: false,
                        is_async: false,
                        is_generator: false,
                        static_token,
                        key,
                        kind: MethodKind::Method,
                    },
                );
            } else if self.is_class_property(/* asi */ true)
                || (self.syntax().typescript() && self.input().is(Token::QuestionMark))
            {
                // Property named `accessor`

                let key = Key::Public(PropName::Ident(IdentName::new(
                    atom!("accessor"),
                    accessor_token,
                )));
                let is_optional =
                    self.input().syntax().typescript() && self.input_mut().eat(Token::QuestionMark);
                let is_static = static_token.is_some();
                return self.make_property(
                    start,
                    decorators,
                    accessibility,
                    key,
                    is_static,
                    None,
                    is_optional,
                    false,
                    declare,
                    false,
                    false,
                );
            }
        }

        if let Some(static_token) = static_token {
            // Handle static(){}
            if self.is_class_method() {
                let key = Key::Public(PropName::Ident(IdentName::new(
                    atom!("static"),
                    static_token,
                )));
                let is_optional =
                    self.input().syntax().typescript() && self.input_mut().eat(Token::QuestionMark);
                return self.make_method(
                    Self::parse_unique_formal_params,
                    MakeMethodArgs {
                        start,
                        accessibility,
                        decorators,
                        is_abstract: false,
                        is_optional,
                        is_override: false,
                        is_async: false,
                        is_generator: false,
                        static_token: None,
                        key,
                        kind: MethodKind::Method,
                    },
                );
            } else if self.is_class_property(/* asi */ false)
                || (self.syntax().typescript() && self.input().is(Token::QuestionMark))
            {
                // Property named `static`

                // Avoid to parse
                //   static
                //   {}
                let is_parsing_static_blocks = self.input().is(Token::LBrace);
                if !is_parsing_static_blocks {
                    let key = Key::Public(PropName::Ident(IdentName::new(
                        atom!("static"),
                        static_token,
                    )));
                    let is_optional = self.input().syntax().typescript()
                        && self.input_mut().eat(Token::QuestionMark);
                    return self.make_property(
                        start,
                        decorators,
                        accessibility,
                        key,
                        false,
                        accessor_token,
                        is_optional,
                        false,
                        declare,
                        false,
                        false,
                    );
                }
            } else {
                // TODO: error if static contains escape
            }
        }

        self.parse_class_member_with_is_static(
            start,
            declare_token,
            accessibility,
            static_token,
            accessor_token,
            decorators,
        )
    }

    fn parse_class_body(&mut self) -> PResult<Vec<ClassMember>> {
        let mut elems = Vec::with_capacity(32);
        let mut has_constructor_with_body = false;
        while !self.input().is(Token::RBrace) {
            if self.input_mut().eat(Token::Semi) {
                let span = self.input().prev_span();
                debug_assert!(span.lo <= span.hi);
                elems.push(ClassMember::Empty(EmptyStmt { span }));
                continue;
            }
            let elem =
                self.do_inside_of_context(Context::AllowDirectSuper, Self::parse_class_member)?;

            if !self.ctx().contains(Context::InDeclare) {
                if let ClassMember::Constructor(Constructor {
                    body: Some(..),
                    span,
                    ..
                }) = elem
                {
                    if has_constructor_with_body {
                        self.emit_err(span, SyntaxError::DuplicateConstructor);
                    }
                    has_constructor_with_body = true;
                }
            }
            elems.push(elem);
        }
        Ok(elems)
    }

    fn parse_class<T>(
        &mut self,
        start: BytePos,
        class_start: BytePos,
        decorators: Vec<Decorator>,
        is_abstract: bool,
    ) -> PResult<T>
    where
        T: OutputType,
    {
        let (ident, mut class) = self.do_inside_of_context(Context::InClass, |p| {
            p.parse_class_inner(start, class_start, decorators, T::IS_IDENT_REQUIRED)
        })?;

        if is_abstract {
            class.is_abstract = true
        } else {
            for member in class.body.iter() {
                match member {
                    ClassMember::ClassProp(ClassProp {
                        is_abstract: true,
                        span,
                        ..
                    })
                    | ClassMember::Method(ClassMethod {
                        span,
                        is_abstract: true,
                        ..
                    }) => self.emit_err(*span, SyntaxError::TS1244),
                    _ => (),
                }
            }
        }

        match T::finish_class(self.span(start), ident, class) {
            Ok(v) => Ok(v),
            Err(kind) => syntax_error!(self, kind),
        }
    }

    /// Not generic
    fn parse_class_inner(
        &mut self,
        _start: BytePos,
        class_start: BytePos,
        decorators: Vec<Decorator>,
        is_ident_required: bool,
    ) -> PResult<(Option<Ident>, Box<Class>)> {
        self.strict_mode(|p| {
            expect!(p, Token::Class);

            let ident = p.parse_maybe_opt_binding_ident(is_ident_required, true)?;
            if p.input().syntax().typescript() {
                if let Some(span) = ident.invalid_class_name() {
                    p.emit_err(span, SyntaxError::TS2414);
                }
            }

            let type_params = if p.input().syntax().typescript() {
                p.try_parse_ts_type_params(true, true)?
            } else {
                None
            };

            let (mut super_class, mut super_type_params) = if p.input_mut().eat(Token::Extends) {
                let (super_class, super_type_params) = p.parse_super_class()?;

                if p.syntax().typescript() && p.input_mut().eat(Token::Comma) {
                    let exprs = p.parse_ts_heritage_clause()?;

                    for e in &exprs {
                        p.emit_err(e.span(), SyntaxError::TS1174);
                    }
                }

                (Some(super_class), super_type_params)
            } else {
                (None, None)
            };

            // Handle TS1172
            if p.input_mut().eat(Token::Extends) {
                p.emit_err(p.input().prev_span(), SyntaxError::TS1172);

                p.parse_super_class()?;
            };

            let implements =
                if p.input().syntax().typescript() && p.input_mut().eat(Token::Implements) {
                    p.parse_ts_heritage_clause()?
                } else {
                    Vec::with_capacity(4)
                };

            {
                // Handle TS1175
                if p.input().syntax().typescript() && p.input_mut().eat(Token::Implements) {
                    p.emit_err(p.input().prev_span(), SyntaxError::TS1175);

                    p.parse_ts_heritage_clause()?;
                }
            }

            // Handle TS1173
            if p.input().syntax().typescript() && p.input_mut().eat(Token::Extends) {
                p.emit_err(p.input().prev_span(), SyntaxError::TS1173);

                let (sc, type_params) = p.parse_super_class()?;

                if super_class.is_none() {
                    super_class = Some(sc);
                    if type_params.is_some() {
                        super_type_params = type_params;
                    }
                }
            }

            expect!(p, Token::LBrace);

            let body = if super_class.is_some() {
                p.do_inside_of_context(Context::HasSuperClass, Self::parse_class_body)?
            } else {
                p.do_outside_of_context(Context::HasSuperClass, Self::parse_class_body)?
            };

            if p.input().cur() == Token::Eof {
                let eof_text = p.input_mut().dump_cur();
                p.emit_err(
                    p.input().cur_span(),
                    SyntaxError::Expected(format!("{:?}", Token::RBrace), eof_text),
                );
            } else {
                expect!(p, Token::RBrace);
            }

            let span = p.span(class_start);
            Ok((
                ident,
                Box::new(Class {
                    span,
                    decorators,
                    is_abstract: false,
                    type_params,
                    super_class,
                    super_type_params,
                    body,
                    implements,
                    ..Default::default()
                }),
            ))
        })
    }
}

trait OutputType: Sized {
    const IS_IDENT_REQUIRED: bool;

    /// From babel..
    ///
    /// When parsing function expression, the binding identifier is parsed
    /// according to the rules inside the function.
    /// e.g. (function* yield() {}) is invalid because "yield" is disallowed in
    /// generators.
    /// This isn't the case with function declarations: function* yield() {} is
    /// valid because yield is parsed as if it was outside the generator.
    /// Therefore, this.state.inGenerator is set before or after parsing the
    /// function id according to the "isStatement" parameter.
    fn is_fn_expr() -> bool {
        false
    }

    fn finish_fn(span: Span, ident: Option<Ident>, f: Box<Function>) -> Result<Self, SyntaxError>;

    fn finish_class(
        span: Span,
        ident: Option<Ident>,
        class: Box<Class>,
    ) -> Result<Self, SyntaxError>;
}

impl OutputType for Box<Expr> {
    const IS_IDENT_REQUIRED: bool = false;

    fn is_fn_expr() -> bool {
        true
    }

    fn finish_fn(
        _span: Span,
        ident: Option<Ident>,
        function: Box<Function>,
    ) -> Result<Self, SyntaxError> {
        Ok(FnExpr { ident, function }.into())
    }

    fn finish_class(
        _span: Span,
        ident: Option<Ident>,
        class: Box<Class>,
    ) -> Result<Self, SyntaxError> {
        Ok(ClassExpr { ident, class }.into())
    }
}

impl OutputType for ExportDefaultDecl {
    const IS_IDENT_REQUIRED: bool = false;

    fn finish_fn(
        span: Span,
        ident: Option<Ident>,
        function: Box<Function>,
    ) -> Result<Self, SyntaxError> {
        Ok(ExportDefaultDecl {
            span,
            decl: DefaultDecl::Fn(FnExpr { ident, function }),
        })
    }

    fn finish_class(
        span: Span,
        ident: Option<Ident>,
        class: Box<Class>,
    ) -> Result<Self, SyntaxError> {
        Ok(ExportDefaultDecl {
            span,
            decl: DefaultDecl::Class(ClassExpr { ident, class }),
        })
    }
}

impl OutputType for Decl {
    const IS_IDENT_REQUIRED: bool = true;

    fn finish_fn(
        _span: Span,
        ident: Option<Ident>,
        function: Box<Function>,
    ) -> Result<Self, SyntaxError> {
        let ident = ident.ok_or(SyntaxError::ExpectedIdent)?;

        Ok(FnDecl {
            declare: false,
            ident,
            function,
        }
        .into())
    }

    fn finish_class(_: Span, ident: Option<Ident>, class: Box<Class>) -> Result<Self, SyntaxError> {
        let ident = ident.ok_or(SyntaxError::ExpectedIdent)?;

        Ok(ClassDecl {
            declare: false,
            ident,
            class,
        }
        .into())
    }
}

fn has_use_strict(block: &BlockStmt) -> Option<Span> {
    block
        .stmts
        .iter()
        .take_while(|s| s.can_precede_directive())
        .find_map(|s| {
            if s.is_use_strict() {
                Some(s.span())
            } else {
                None
            }
        })
}

fn is_constructor(key: &Key) -> bool {
    if let Key::Public(PropName::Ident(IdentName { sym, .. })) = key {
        sym.eq("constructor")
    } else if let Key::Public(PropName::Str(Str { value, .. })) = key {
        value.eq("constructor")
    } else {
        false
    }
}

pub(crate) fn is_not_this(p: &Param) -> bool {
    !matches!(
        &p.pat,
        Pat::Ident(BindingIdent {
            id: Ident{ sym: this, .. },
            ..
        })if &**this == "this"
    )
}

#[cfg(test)]
mod tests {

    use swc_common::DUMMY_SP as span;
    use swc_ecma_ast::*;
    use swc_ecma_visit::assert_eq_ignore_span;

    use crate::{test_parser, Syntax};

    fn expr(s: &'static str) -> Box<Expr> {
        test_parser(s, Syntax::default(), |p| p.parse_expr())
    }

    #[test]
    fn class_expr() {
        assert_eq_ignore_span!(
            expr("(class extends a {})"),
            Box::new(Expr::Paren(ParenExpr {
                span,
                expr: Box::new(Expr::Class(ClassExpr {
                    ident: None,
                    class: Box::new(Class {
                        decorators: Vec::new(),
                        span,
                        body: Vec::new(),
                        super_class: Some(expr("a")),
                        implements: Vec::new(),
                        is_abstract: false,
                        ..Default::default()
                    }),
                })),
            }))
        );
    }
}
