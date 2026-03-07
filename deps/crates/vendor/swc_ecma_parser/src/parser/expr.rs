use either::Either;
use swc_atoms::atom;
use swc_common::{ast_node, util::take::Take, BytePos, Span, Spanned};

use super::*;
use crate::{
    error::SyntaxError,
    parser::{pat::PatType, util::IsSimpleParameterList, Parser},
};

#[ast_node]
pub(crate) enum AssignTargetOrSpread {
    #[tag("ExprOrSpread")]
    ExprOrSpread(ExprOrSpread),
    #[tag("*")]
    Pat(Pat),
}

impl<I: Tokens> Parser<I> {
    pub fn parse_expr(&mut self) -> PResult<Box<Expr>> {
        trace_cur!(self, parse_expr);
        debug_tracing!(self, "parse_expr");
        let expr = self.parse_assignment_expr()?;
        let start = expr.span_lo();

        if self.input_mut().is(Token::Comma) {
            let mut exprs = vec![expr];

            while self.input_mut().eat(Token::Comma) {
                exprs.push(self.parse_assignment_expr()?);
            }

            return Ok(SeqExpr {
                span: self.span(start),
                exprs,
            }
            .into());
        }

        Ok(expr)
    }

    /// AssignmentExpression[+In, ?Yield, ?Await]
    /// ...AssignmentExpression[+In, ?Yield, ?Await]
    fn parse_expr_or_spread(&mut self) -> PResult<ExprOrSpread> {
        trace_cur!(self, parse_expr_or_spread);
        let start = self.input().cur_pos();
        if self.input_mut().eat(Token::DotDotDot) {
            let spread_span = self.span(start);
            let spread = Some(spread_span);
            self.allow_in_expr(Self::parse_assignment_expr)
                .map_err(|err| {
                    Error::new(
                        err.span(),
                        SyntaxError::WithLabel {
                            inner: Box::new(err),
                            span: spread_span,
                            note: "An expression should follow '...'",
                        },
                    )
                })
                .map(|expr| ExprOrSpread { spread, expr })
        } else {
            self.parse_assignment_expr()
                .map(|expr| ExprOrSpread { spread: None, expr })
        }
    }

    ///`parseMaybeAssign` (overridden)
    #[cfg_attr(
        feature = "tracing-spans",
        tracing::instrument(level = "debug", skip_all)
    )]
    pub(crate) fn parse_assignment_expr(&mut self) -> PResult<Box<Expr>> {
        trace_cur!(self, parse_assignment_expr);

        if self.input().syntax().typescript() && self.input().is(Token::JSXTagStart) {
            // Note: When the JSX plugin is on, type assertions (`<T> x`) aren't valid
            // syntax.
            let res = self.try_parse_ts(|p| p.parse_assignment_expr_base().map(Some));
            if let Some(res) = res {
                return Ok(res);
            }
        }

        self.parse_assignment_expr_base()
    }

    /// Parse an assignment expression. This includes applications of
    /// operators like `+=`.
    ///
    /// `parseMaybeAssign`
    #[cfg_attr(feature = "tracing-spans", tracing::instrument(skip_all))]
    fn parse_assignment_expr_base(&mut self) -> PResult<Box<Expr>> {
        trace_cur!(self, parse_assignment_expr_base);
        let start = self.input().cur_span();

        if self.input().syntax().typescript()
            && (self.input().cur() == Token::Lt || self.input().cur() == Token::JSXTagStart)
            && (peek!(self).is_some_and(|peek| peek.is_word() || peek == Token::JSXName))
        {
            let res = self.do_outside_of_context(Context::WillExpectColonForCond, |p| {
                p.try_parse_ts(|p| {
                    let type_parameters = p.parse_ts_type_params(false, true)?;

                    // In TSX mode, type parameters that could be mistaken for JSX
                    // (single param without constraint and no trailing comma) are not
                    // allowed. e.g., `<T>() => {}` is invalid in TSX because `<T>`
                    // looks like JSX.
                    // Valid alternatives: `<T,>() => {}` or `<T extends unknown>() =>
                    // {}`
                    if p.input().syntax().jsx() && type_parameters.params.len() == 1 {
                        let single_param = &type_parameters.params[0];
                        // Check if there was a trailing comma by examining spans.
                        // For `<T>`: decl.span.hi - param.span.hi = 1 (just `>`)
                        // For `<T,>`: decl.span.hi - param.span.hi > 1 (`,` and `>`)
                        let has_trailing_comma =
                            type_parameters.span.hi.0 - single_param.span.hi.0 > 1;
                        let dominated_by_jsx =
                            single_param.constraint.is_none() && !has_trailing_comma;

                        if dominated_by_jsx {
                            return Ok(None);
                        }
                    }

                    let mut arrow = p.parse_assignment_expr_base()?;
                    match *arrow {
                        Expr::Arrow(ArrowExpr {
                            ref mut span,
                            ref mut type_params,
                            ..
                        }) => {
                            *span = Span::new_with_checked(type_parameters.span.lo, span.hi);
                            *type_params = Some(type_parameters);
                        }
                        _ => unexpected!(p, "("),
                    }
                    Ok(Some(arrow))
                })
            });
            if let Some(res) = res {
                if self.input().syntax().disallow_ambiguous_jsx_like() {
                    self.emit_err(start, SyntaxError::ReservedArrowTypeParam);
                }
                return Ok(res);
            }
        }

        if self.ctx().contains(Context::InGenerator) && self.input().is(Token::Yield) {
            return self.parse_yield_expr();
        }

        let cur = self.input().cur();

        if cur == Token::Error {
            let err = self.input_mut().expect_error_token_and_bump();
            return Err(err);
        }

        self.state_mut().potential_arrow_start =
            if cur.is_known_ident() || matches!(cur, Token::Ident | Token::Yield | Token::LParen) {
                Some(self.cur_pos())
            } else {
                None
            };

        let start = self.cur_pos();

        // Try to parse conditional expression.
        let cond = self.parse_cond_expr()?;

        return_if_arrow!(self, cond);

        match *cond {
            // if cond is conditional expression but not left-hand-side expression,
            // just return it.
            Expr::Cond(..) | Expr::Bin(..) | Expr::Unary(..) | Expr::Update(..) => return Ok(cond),
            _ => {}
        }

        self.finish_assignment_expr(start, cond)
    }

    #[allow(dead_code)]
    pub(crate) fn parse_member_expr(&mut self) -> PResult<Box<Expr>> {
        self.parse_member_expr_or_new_expr(false)
    }

    pub(super) fn parse_unary_expr(&mut self) -> PResult<Box<Expr>> {
        trace_cur!(self, parse_unary_expr);

        let token_and_span = self.input().get_cur();
        let start = token_and_span.span.lo;
        let cur = token_and_span.token;

        if cur == Token::Lt && self.input().syntax().typescript() && !self.input().syntax().jsx() {
            self.bump(); // consume `<`
            return if self.input_mut().eat(Token::Const) {
                self.expect(Token::Gt)?;
                let expr = self.parse_unary_expr()?;
                Ok(TsConstAssertion {
                    span: self.span(start),
                    expr,
                }
                .into())
            } else {
                self.parse_ts_type_assertion(start)
                    .map(Expr::from)
                    .map(Box::new)
            };
        } else if cur == Token::Lt
            && self.input().syntax().jsx()
            && self.input_mut().peek().is_some_and(|peek| {
                peek.is_word() || peek == Token::Gt || peek.should_rescan_into_gt_in_jsx()
            })
        {
            fn into_expr(e: Either<JSXFragment, JSXElement>) -> Box<Expr> {
                match e {
                    Either::Left(l) => l.into(),
                    Either::Right(r) => r.into(),
                }
            }
            return self.parse_jsx_element(true).map(into_expr);
        } else if matches!(cur, Token::PlusPlus | Token::MinusMinus) {
            // Parse update expression
            let op = if cur == Token::PlusPlus {
                op!("++")
            } else {
                op!("--")
            };
            self.bump();

            let arg = self.parse_unary_expr()?;
            let span = Span::new_with_checked(start, arg.span_hi());
            self.check_assign_target(&arg, false);

            return Ok(UpdateExpr {
                span,
                prefix: true,
                op,
                arg,
            }
            .into());
        } else if cur == Token::Delete
            || cur == Token::Void
            || cur == Token::TypeOf
            || cur == Token::Plus
            || cur == Token::Minus
            || cur == Token::Tilde
            || cur == Token::Bang
        {
            // Parse unary expression
            let op = if cur == Token::Delete {
                op!("delete")
            } else if cur == Token::Void {
                op!("void")
            } else if cur == Token::TypeOf {
                op!("typeof")
            } else if cur == Token::Plus {
                op!(unary, "+")
            } else if cur == Token::Minus {
                op!(unary, "-")
            } else if cur == Token::Tilde {
                op!("~")
            } else {
                debug_assert!(cur == Token::Bang);
                op!("!")
            };
            self.bump();
            let arg_start = self.cur_pos() - BytePos(1);
            let arg = match self.parse_unary_expr() {
                Ok(expr) => expr,
                Err(err) => {
                    self.emit_error(err);
                    Invalid {
                        span: Span::new_with_checked(arg_start, arg_start),
                    }
                    .into()
                }
            };

            if op == op!("delete") {
                // Skip emitting TS1102 in TypeScript mode because it's a semantic error
                // that should be handled by the type checker, not the parser.
                // See: https://github.com/swc-project/swc/issues/10558
                if !self.input().syntax().typescript() {
                    if let Expr::Ident(ref i) = *arg {
                        self.emit_strict_mode_err(i.span, SyntaxError::TS1102)
                    }
                }
            }

            return Ok(UnaryExpr {
                span: Span::new_with_checked(start, arg.span_hi()),
                op,
                arg,
            }
            .into());
        } else if cur == Token::Await {
            return self.parse_await_expr(None);
        }

        // UpdateExpression
        let expr = self.parse_lhs_expr()?;
        if let Expr::Arrow { .. } = *expr {
            return Ok(expr);
        }

        // Line terminator isn't allowed here.
        if self.input_mut().had_line_break_before_cur() {
            return Ok(expr);
        }

        let cur = self.input().cur();
        if cur == Token::PlusPlus || cur == Token::MinusMinus {
            let op = if cur == Token::PlusPlus {
                op!("++")
            } else {
                op!("--")
            };

            self.check_assign_target(&expr, false);
            self.bump();

            return Ok(UpdateExpr {
                span: self.span(expr.span_lo()),
                prefix: false,
                op,
                arg: expr,
            }
            .into());
        }
        Ok(expr)
    }

    #[inline(always)]
    pub(super) fn parse_primary_expr(&mut self) -> PResult<Box<Expr>> {
        trace_cur!(self, parse_primary_expr);
        let start = self.input().cur_pos();
        let can_be_arrow = self
            .state
            .potential_arrow_start
            .map(|s| s == start)
            .unwrap_or(false);
        let tok = self.input.cur();
        match tok {
            Token::This => return self.parse_this_expr(start),
            Token::Async => {
                if let Some(res) = self.try_parse_async_start(can_be_arrow) {
                    return res;
                }
            }
            Token::LBracket => {
                return self
                    .do_outside_of_context(Context::WillExpectColonForCond, Self::parse_array_lit)
            }
            Token::LBrace => {
                return self.parse_object_expr().map(Box::new);
            }
            // Handle FunctionExpression and GeneratorExpression
            Token::Function => {
                return self.parse_fn_expr();
            }
            // Literals
            Token::Null | Token::True | Token::False | Token::Num | Token::BigInt | Token::Str => {
                return self.parse_lit().map(|lit| lit.into());
            }
            // Regexp
            Token::Slash | Token::DivEq => {
                if let Some(res) = self.try_parse_regexp(start) {
                    return Ok(res);
                }
            }
            Token::LParen => return self.parse_paren_expr_or_arrow_fn(can_be_arrow, None),
            Token::NoSubstitutionTemplateLiteral => {
                return Ok(self.parse_no_substitution_template_literal(false)?.into())
            }
            Token::TemplateHead => {
                // parse template literal
                return Ok(self
                    .do_outside_of_context(Context::WillExpectColonForCond, |p| p.parse_tpl(false))?
                    .into());
            }
            _ => {}
        }

        self.parse_primary_expr_rest(start, can_be_arrow)
    }

    /// Parse call, dot, and `[]`-subscript expressions.
    #[cfg_attr(
        feature = "tracing-spans",
        tracing::instrument(level = "debug", skip_all)
    )]
    pub(crate) fn parse_lhs_expr(&mut self) -> PResult<Box<Expr>> {
        trace_cur!(self, parse_lhs_expr);

        let token_and_span = self.input().get_cur();
        let start = token_and_span.span.lo;
        let cur = token_and_span.token;

        // `super()` can't be handled from parse_new_expr()
        if cur == Token::Super {
            self.bump(); // eat `super`
            let obj = Callee::Super(Super {
                span: self.span(start),
            });
            return self.parse_subscripts(obj, false, false);
        } else if cur == Token::Import {
            self.bump(); // eat `import`
            return self.parse_dynamic_import_or_import_meta(start, false);
        }

        let callee = self.parse_new_expr()?;
        return_if_arrow!(self, callee);

        let type_args = if self.input().syntax().typescript() && {
            let cur = self.input().cur();
            cur == Token::Lt || cur == Token::LShift
        } {
            self.try_parse_ts(|p| {
                let type_args = p.parse_ts_type_args()?;
                p.assert_and_bump(Token::Gt);
                if p.input().is(Token::LParen) {
                    Ok(Some(type_args))
                } else {
                    Ok(None)
                }
            })
        } else {
            None
        };

        if let Expr::New(ne @ NewExpr { args: None, .. }) = *callee {
            // If this is parsed using 'NewExpression' rule, just return it.
            // Because it's not left-recursive.
            if type_args.is_some() {
                // This fails with `expected (`
                expect!(self, Token::LParen);
            }
            debug_assert!(
                self.input().cur() != Token::LParen,
                "parse_new_expr() should eat paren if it exists"
            );
            return Ok(NewExpr { type_args, ..ne }.into());
        }
        // 'CallExpr' rule contains 'MemberExpr (...)',
        // and 'MemberExpr' rule contains 'new MemberExpr (...)'

        if self.input().is(Token::LParen) {
            // This is parsed using production MemberExpression,
            // which is left-recursive.
            let (callee, is_import) = match callee {
                _ if callee.is_ident_ref_to("import") => (
                    Callee::Import(Import {
                        span: callee.span(),
                        phase: Default::default(),
                    }),
                    true,
                ),
                _ => (Callee::Expr(callee), false),
            };
            let args = self.parse_args(is_import)?;

            let call_expr = match callee {
                Callee::Expr(e) if unwrap_ts_non_null(&e).is_opt_chain() => OptChainExpr {
                    span: self.span(start),
                    base: Box::new(OptChainBase::Call(OptCall {
                        span: self.span(start),
                        callee: e,
                        args,
                        type_args,
                        ..Default::default()
                    })),
                    optional: false,
                }
                .into(),
                _ => CallExpr {
                    span: self.span(start),

                    callee,
                    args,
                    type_args,
                    ..Default::default()
                }
                .into(),
            };

            return self.parse_subscripts(Callee::Expr(call_expr), false, false);
        }
        if type_args.is_some() {
            // This fails
            expect!(self, Token::LParen);
        }

        // This is parsed using production 'NewExpression', which contains
        // 'MemberExpression'
        Ok(callee)
    }

    #[cfg_attr(
        feature = "tracing-spans",
        tracing::instrument(level = "debug", skip_all)
    )]
    fn parse_array_lit(&mut self) -> PResult<Box<Expr>> {
        trace_cur!(self, parse_array_lit);

        let start = self.input().cur_pos();

        self.assert_and_bump(Token::LBracket);

        let mut elems = Vec::with_capacity(8);

        while !self.input().is(Token::RBracket) {
            if self.input().is(Token::Comma) {
                expect!(self, Token::Comma);
                elems.push(None);
                continue;
            }

            elems.push(self.allow_in_expr(|p| p.parse_expr_or_spread()).map(Some)?);

            if !self.input().is(Token::RBracket) {
                expect!(self, Token::Comma);
                if self.input().is(Token::RBracket) {
                    let prev_span = self.input().prev_span();
                    self.state_mut().trailing_commas.insert(start, prev_span);
                }
            }
        }

        expect!(self, Token::RBracket);

        let span = self.span(start);
        Ok(ArrayLit { span, elems }.into())
    }

    fn at_possible_async(&mut self, expr: &Expr) -> bool {
        // TODO(kdy1): !this.state.containsEsc &&
        self.state().potential_arrow_start == Some(expr.span_lo()) && expr.is_ident_ref_to("async")
    }

    fn parse_yield_expr(&mut self) -> PResult<Box<Expr>> {
        let start = self.input().cur_pos();
        self.assert_and_bump(Token::Yield);
        debug_assert!(self.ctx().contains(Context::InGenerator));

        // Spec says
        // YieldExpression cannot be used within the FormalParameters of a generator
        // function because any expressions that are part of FormalParameters are
        // evaluated before the resulting generator object is in a resumable state.
        if self.ctx().contains(Context::InParameters) && !self.ctx().contains(Context::InFunction) {
            syntax_error!(self, self.input().prev_span(), SyntaxError::YieldParamInGen)
        }

        let parse_with_arg = |p: &mut Self| {
            let has_star = p.input_mut().eat(Token::Asterisk);
            let err_span = p.span(start);
            let arg = p.parse_assignment_expr().map_err(|err| {
                Error::new(
                    err.span(),
                    SyntaxError::WithLabel {
                        inner: Box::new(err),
                        span: err_span,
                        note: "Tried to parse an argument of yield",
                    },
                )
            })?;
            Ok(YieldExpr {
                span: p.span(start),
                arg: Some(arg),
                delegate: has_star,
            }
            .into())
        };

        if self.is_general_semi() || {
            let cur = self.input().cur();
            cur != Token::Lt
                && cur != Token::Asterisk
                && cur != Token::Slash
                && cur != Token::DivEq
                && !cur.starts_expr()
        } {
            Ok(YieldExpr {
                span: self.span(start),
                arg: None,
                delegate: false,
            }
            .into())
        } else {
            parse_with_arg(self)
        }
    }

    fn parse_tpl_elements(
        &mut self,
        is_tagged_tpl: bool,
    ) -> PResult<(Vec<Box<Expr>>, Vec<TplElement>)> {
        trace_cur!(self, parse_tpl_elements);

        let mut exprs = Vec::new();
        let cur_elem = self.parse_template_head(is_tagged_tpl)?;
        let mut is_tail = cur_elem.tail;
        let mut quasis = vec![cur_elem];

        while !is_tail {
            exprs.push(self.allow_in_expr(|p| p.parse_expr())?);
            let elem = self.parse_tpl_element(is_tagged_tpl)?;
            is_tail = elem.tail;
            quasis.push(elem);
        }
        Ok((exprs, quasis))
    }

    fn parse_tagged_tpl(
        &mut self,
        tag: Box<Expr>,
        type_params: Option<Box<TsTypeParamInstantiation>>,
    ) -> PResult<TaggedTpl> {
        let tagged_tpl_start = tag.span_lo();
        trace_cur!(self, parse_tagged_tpl);

        let tpl = Box::new(
            if self.input_mut().is(Token::NoSubstitutionTemplateLiteral) {
                self.input_mut().rescan_template_token(true);
                self.parse_no_substitution_template_literal(true)?
            } else {
                self.parse_tpl(true)?
            },
        );

        let span = self.span(tagged_tpl_start);

        if tag.is_opt_chain() {
            self.emit_err(span, SyntaxError::TaggedTplInOptChain);
        }

        Ok(TaggedTpl {
            span,
            tag,
            type_params,
            tpl,
            ..Default::default()
        })
    }

    pub(super) fn parse_no_substitution_template_literal(
        &mut self,
        is_tagged_tpl: bool,
    ) -> PResult<Tpl> {
        let start = self.input.cur_pos();
        let cur = self.input.cur();
        let token_span = self.input.cur_span();
        debug_assert!(matches!(cur, Token::NoSubstitutionTemplateLiteral));

        // exclude the leading and the trailing `\``
        let span = Span::new_with_checked(token_span.lo + BytePos(1), token_span.hi - BytePos(1));
        let raw = Atom::new(self.input.iter.read_string(span));
        let cooked = self.input.expect_template_token_value();
        let cooked = match cooked {
            Ok(cooked) => Some(cooked),
            Err(err) => {
                if is_tagged_tpl {
                    None
                } else {
                    return Err(err);
                }
            }
        };
        self.bump();

        Ok(Tpl {
            span: self.span(start),
            exprs: vec![],
            quasis: vec![TplElement {
                span,
                tail: true,
                raw,
                cooked,
            }],
        })
    }

    fn parse_template_head(&mut self, is_tagged_tpl: bool) -> PResult<TplElement> {
        let cur = self.input.cur();
        let token_span = self.input.cur_span();
        debug_assert!(matches!(cur, Token::TemplateHead));

        // exclude the leading `\`` and the trailing `${`
        let span = Span::new_with_checked(token_span.lo + BytePos(1), token_span.hi - BytePos(2));
        let raw = Atom::new(self.input.iter.read_string(span));
        let cooked = self.input.expect_template_token_value();
        let cooked = match cooked {
            Ok(cooked) => Some(cooked),
            Err(err) => {
                if is_tagged_tpl {
                    None
                } else {
                    return Err(err);
                }
            }
        };

        self.bump();

        Ok(TplElement {
            span,
            raw,
            tail: false,
            cooked,
        })
    }

    pub(super) fn parse_tpl(&mut self, is_tagged_tpl: bool) -> PResult<Tpl> {
        trace_cur!(self, parse_tpl);
        debug_assert!(matches!(self.input.cur(), Token::TemplateHead));

        let start = self.cur_pos();

        let (exprs, quasis) = self.parse_tpl_elements(is_tagged_tpl)?;

        Ok(Tpl {
            span: self.span(start),
            exprs,
            quasis,
        })
    }

    pub(crate) fn parse_tpl_element(&mut self, is_tagged_tpl: bool) -> PResult<TplElement> {
        if self.input_mut().is(Token::RBrace) {
            self.input_mut().rescan_template_token(false);
        }
        let cur = self.input_mut().cur();
        let token_span = self.input.cur_span();

        let (cooked, raw, tail, span) = match cur {
            Token::TemplateMiddle => {
                let span = Span::new_with_checked(token_span.lo, token_span.hi - BytePos(2));
                let raw = Atom::new(self.input.iter.read_string(span));
                let cooked = self.input.expect_template_token_value();
                self.bump();

                match cooked {
                    Ok(cooked) => (Some(cooked), raw, false, span),
                    Err(err) => {
                        if is_tagged_tpl {
                            (None, raw, false, span)
                        } else {
                            return Err(err);
                        }
                    }
                }
            }
            Token::TemplateTail => {
                let span = Span::new_with_checked(token_span.lo, token_span.hi - BytePos(1));
                let raw = Atom::new(self.input.iter.read_string(span));
                let cooked = self.input.expect_template_token_value();
                self.bump();

                match cooked {
                    Ok(cooked) => (Some(cooked), raw, true, span),
                    Err(err) => {
                        if is_tagged_tpl {
                            (None, raw, true, span)
                        } else {
                            return Err(err);
                        }
                    }
                }
            }
            Token::Error => {
                let err = cur.take_error(self.input_mut());
                self.input_mut().bump();
                return Err(err);
            }
            _ => {
                unexpected!(self, "`}`")
            }
        };

        Ok(TplElement {
            span,
            raw,
            tail,
            cooked,
        })
    }

    #[cfg(feature = "typescript")]
    fn parse_tpl_ty_elements(&mut self) -> PResult<(Vec<Box<TsType>>, Vec<TplElement>)> {
        trace_cur!(self, parse_tpl_elements);

        let mut tys = Vec::new();
        let cur_elem = self.parse_template_head(false)?;
        let mut is_tail = cur_elem.tail;
        let mut quasis = vec![cur_elem];

        while !is_tail {
            tys.push(self.parse_ts_type()?);
            let elem = self.parse_tpl_element(false)?;
            is_tail = elem.tail;
            quasis.push(elem);
        }
        Ok((tys, quasis))
    }

    #[cfg(feature = "typescript")]
    fn parse_no_substitution_template_ty(&mut self) -> PResult<TsTplLitType> {
        let start = self.input.cur_pos();
        let cur = self.input.cur();
        let token_span = self.input.cur_span();
        debug_assert!(matches!(cur, Token::NoSubstitutionTemplateLiteral));

        // exclude the leading and the trailing `\``
        let span = Span::new_with_checked(token_span.lo + BytePos(1), token_span.hi - BytePos(1));
        let raw = Atom::new(self.input.iter.read_string(span));
        let cooked = self.input.expect_template_token_value().ok();
        self.bump();

        Ok(TsTplLitType {
            span: self.span(start),
            types: vec![],
            quasis: vec![TplElement {
                span,
                tail: true,
                raw,
                cooked,
            }],
        })
    }

    #[cfg(feature = "typescript")]
    fn parse_tpl_ty(&mut self) -> PResult<TsTplLitType> {
        trace_cur!(self, parse_tpl_ty);
        debug_assert!(matches!(self.input.cur(), Token::TemplateHead));

        let start = self.cur_pos();

        let (types, quasis) = self.parse_tpl_ty_elements()?;

        let _ = self.input.cur();

        Ok(TsTplLitType {
            span: self.span(start),
            types,
            quasis,
        })
    }

    #[cfg(feature = "typescript")]
    pub(super) fn parse_tagged_tpl_ty(&mut self) -> PResult<TsLitType> {
        let start = self.cur_pos();
        debug_assert!(self.input().syntax().typescript());
        trace_cur!(self, parse_tagged_tpl);
        let tpl_ty = if self.input_mut().is(Token::NoSubstitutionTemplateLiteral) {
            self.parse_no_substitution_template_ty()
        } else {
            self.parse_tpl_ty()
        };
        tpl_ty.map(|tpl_ty| {
            let lit = TsLit::Tpl(tpl_ty);
            TsLitType {
                span: self.span(start),
                lit,
            }
        })
    }

    pub(crate) fn parse_str_lit(&mut self) -> swc_ecma_ast::Str {
        debug_assert!(self.input().cur() == Token::Str);
        let start = self.input().get_cur().span.lo;
        let raw = Atom::new(self.input.cur_string());
        let value = self.input.expect_string_token_value();
        self.bump();

        swc_ecma_ast::Str {
            span: self.span(start),
            value,
            raw: Some(raw),
        }
    }

    pub(crate) fn parse_lit(&mut self) -> PResult<Lit> {
        let token_and_span = self.input().get_cur();
        let start = token_and_span.span.lo;
        let cur = token_and_span.token;
        let v = if cur == Token::Null {
            self.bump();
            let span = self.span(start);
            Lit::Null(swc_ecma_ast::Null { span })
        } else if cur == Token::True || cur == Token::False {
            let value = cur == Token::True;
            self.bump();
            let span = self.span(start);
            Lit::Bool(swc_ecma_ast::Bool { span, value })
        } else if cur == Token::Str {
            Lit::Str(self.parse_str_lit())
        } else if cur == Token::Num {
            let raw = Atom::new(self.input.cur_string());
            let value = self.input_mut().expect_number_token_value();
            self.bump();

            Lit::Num(swc_ecma_ast::Number {
                span: self.span(start),
                value,
                raw: Some(raw),
            })
        } else if cur == Token::BigInt {
            let raw = Atom::new(self.input.cur_string());
            let value = self.input_mut().expect_bigint_token_value();
            self.bump();

            Lit::BigInt(swc_ecma_ast::BigInt {
                span: self.span(start),
                value,
                raw: Some(raw),
            })
        } else if cur == Token::Error {
            let err = self.input_mut().expect_error_token_and_bump();
            return Err(err);
        } else if cur == Token::Eof {
            return Err(self.eof_error());
        } else {
            unreachable!("parse_lit should not be called for {:?}", cur)
        };
        Ok(v)
    }

    /// Parse `Arguments[Yield, Await]`
    #[cfg_attr(feature = "tracing-spans", tracing::instrument(skip_all))]
    pub(crate) fn parse_args(&mut self, is_dynamic_import: bool) -> PResult<Vec<ExprOrSpread>> {
        trace_cur!(self, parse_args);

        self.do_outside_of_context(Context::WillExpectColonForCond, |p| {
            let start = p.cur_pos();
            expect!(p, Token::LParen);

            let mut first = true;
            let mut expr_or_spreads = Vec::with_capacity(2);

            while !p.input().is(Token::RParen) {
                if first {
                    first = false;
                } else {
                    expect!(p, Token::Comma);
                    // Handle trailing comma.
                    if p.input().is(Token::RParen) {
                        if is_dynamic_import && !p.input().syntax().import_attributes() {
                            syntax_error!(p, p.span(start), SyntaxError::TrailingCommaInsideImport)
                        }

                        break;
                    }
                }

                expr_or_spreads.push(p.allow_in_expr(|p| p.parse_expr_or_spread())?);
            }

            expect!(p, Token::RParen);
            Ok(expr_or_spreads)
        })
    }

    fn finish_assignment_expr(&mut self, start: BytePos, cond: Box<Expr>) -> PResult<Box<Expr>> {
        trace_cur!(self, finish_assignment_expr);

        if let Some(op) = self.input().cur().as_assign_op() {
            let left = if op == AssignOp::Assign {
                match AssignTarget::try_from(self.reparse_expr_as_pat(PatType::AssignPat, cond)?) {
                    Ok(pat) => pat,
                    Err(expr) => {
                        syntax_error!(self, expr.span(), SyntaxError::InvalidAssignTarget)
                    }
                }
            } else {
                // It is an early Reference Error if IsValidSimpleAssignmentTarget of
                // LeftHandSideExpression is false.
                if !cond.is_valid_simple_assignment_target(self.ctx().contains(Context::Strict)) {
                    if self.input().syntax().typescript() {
                        self.emit_err(cond.span(), SyntaxError::TS2406);
                    } else {
                        self.emit_err(cond.span(), SyntaxError::NotSimpleAssign)
                    }
                }
                if self.input().syntax().typescript()
                    && cond
                        .as_ident()
                        .map(|i| i.is_reserved_in_strict_bind())
                        .unwrap_or(false)
                {
                    self.emit_strict_mode_err(cond.span(), SyntaxError::TS1100);
                }

                // TODO
                match AssignTarget::try_from(cond) {
                    Ok(v) => v,
                    Err(v) => {
                        syntax_error!(self, v.span(), SyntaxError::InvalidAssignTarget);
                    }
                }
            };

            self.bump();
            let right = self.parse_assignment_expr()?;
            Ok(AssignExpr {
                span: self.span(start),
                op,
                // TODO:
                left,
                right,
            }
            .into())
        } else {
            Ok(cond)
        }
    }

    /// Spec: 'ConditionalExpression'
    #[cfg_attr(
        feature = "tracing-spans",
        tracing::instrument(level = "debug", skip_all)
    )]
    fn parse_cond_expr(&mut self) -> PResult<Box<Expr>> {
        trace_cur!(self, parse_cond_expr);

        let start = self.cur_pos();

        let test = self.parse_bin_expr()?;
        return_if_arrow!(self, test);

        if self.input_mut().eat(Token::QuestionMark) {
            let cons = self.do_inside_of_context(
                Context::InCondExpr
                    .union(Context::WillExpectColonForCond)
                    .union(Context::IncludeInExpr),
                Self::parse_assignment_expr,
            )?;

            expect!(self, Token::Colon);

            let alt = self.do_inside_of_context(Context::InCondExpr, |p| {
                p.do_outside_of_context(
                    Context::WillExpectColonForCond,
                    Self::parse_assignment_expr,
                )
            })?;

            let span = Span::new_with_checked(start, alt.span_hi());
            Ok(CondExpr {
                span,
                test,
                cons,
                alt,
            }
            .into())
        } else {
            Ok(test)
        }
    }

    #[cfg_attr(feature = "tracing-spans", tracing::instrument(skip_all))]
    pub(crate) fn parse_subscripts(
        &mut self,
        obj: Callee,
        no_call: bool,
        no_computed_member: bool,
    ) -> PResult<Box<Expr>> {
        let start = obj.span().lo;
        let mut expr = match obj {
            Callee::Import(import) => self.parse_subscript_import_call(start, import)?,
            Callee::Super(s) => self.parse_subscript_super(start, s, no_call)?,
            Callee::Expr(expr) => expr,
            #[cfg(swc_ast_unknown)]
            _ => unreachable!(),
        };

        loop {
            expr = match self.parse_subscript(start, expr, no_call, no_computed_member)? {
                (expr, false) => return Ok(expr),
                (expr, true) => expr,
            }
        }
    }

    /// returned bool is true if this method should be called again.
    #[cfg_attr(feature = "tracing-spans", tracing::instrument(skip_all))]
    fn parse_subscript(
        &mut self,
        start: BytePos,
        mut callee: Box<Expr>,
        no_call: bool,
        no_computed_member: bool,
    ) -> PResult<(Box<Expr>, bool)> {
        trace_cur!(self, parse_subscript);

        if self.input().syntax().typescript() {
            if !self.input().had_line_break_before_cur() && self.input().is(Token::Bang) {
                self.input_mut().set_expr_allowed(false);
                self.assert_and_bump(Token::Bang);

                let expr = Box::new(Expr::TsNonNull(TsNonNullExpr {
                    span: self.span(start),
                    expr: callee,
                }));

                return Ok((expr, true));
            }

            if self.input().is(Token::Lt) {
                // tsTryParseAndCatch is expensive, so avoid if not necessary.
                // There are number of things we are going to "maybe" parse, like type arguments
                // on tagged template expressions. If any of them fail, walk it back and
                // continue.

                let result = self.do_inside_of_context(Context::ShouldNotLexLtOrGtAsType, |p| {
                    p.try_parse_ts(|p| {
                        if !no_call && p.at_possible_async(&callee) {
                            // Almost certainly this is a generic async function `async <T>() =>
                            // ... But it might be a call with a
                            // type argument `async<T>();`
                            let async_arrow_fn = p.try_parse_ts_generic_async_arrow_fn(start)?;
                            if let Some(async_arrow_fn) = async_arrow_fn {
                                return Ok(Some((async_arrow_fn.into(), true)));
                            }
                        }

                        let type_args = p.parse_ts_type_args()?;
                        p.assert_and_bump(Token::Gt);
                        let cur = p.input().cur();

                        if !no_call && cur == Token::LParen {
                            // possibleAsync always false here, because we would have handled it
                            // above. (won't be any undefined arguments)
                            let args = p.parse_args(false)?;

                            let expr = if callee.is_opt_chain() {
                                Expr::OptChain(OptChainExpr {
                                    span: p.span(start),
                                    base: Box::new(OptChainBase::Call(OptCall {
                                        span: p.span(start),
                                        callee: callee.take(),
                                        type_args: Some(type_args),
                                        args,
                                        ..Default::default()
                                    })),
                                    optional: false,
                                })
                            } else {
                                Expr::Call(CallExpr {
                                    span: p.span(start),
                                    callee: Callee::Expr(callee.take()),
                                    type_args: Some(type_args),
                                    args,
                                    ..Default::default()
                                })
                            };

                            Ok(Some((Box::new(expr), true)))
                        } else if matches!(
                            cur,
                            Token::NoSubstitutionTemplateLiteral
                                | Token::TemplateHead
                                | Token::BackQuote
                        ) {
                            p.parse_tagged_tpl(callee.take(), Some(type_args))
                                .map(|expr| (expr.into(), true))
                                .map(Some)
                        } else if matches!(cur, Token::Eq | Token::As | Token::Satisfies) {
                            let expr = Expr::TsInstantiation(TsInstantiation {
                                span: p.span(start),
                                expr: callee.take(),
                                type_args,
                            });
                            Ok(Some((Box::new(expr), false)))
                        } else if no_call {
                            unexpected!(p, "`")
                        } else {
                            unexpected!(p, "( or `")
                        }
                    })
                });

                if let Some(expr) = result {
                    return Ok(expr);
                }
            }
        }

        let ts_instantiation = if self.syntax().typescript() && self.input().is(Token::Lt) {
            self.try_parse_ts_type_args()
        } else {
            None
        };

        let question_dot_token = if self.input().is(Token::QuestionMark)
            && peek!(self).is_some_and(|peek| peek == Token::Dot)
        {
            let start = self.cur_pos();
            self.bump();

            let span = Some(self.span(start));
            self.bump();

            span
        } else {
            None
        };

        // If question_dot_token is Some, then `self.cur == Token::Dot`
        let question_dot = question_dot_token.is_some();

        // $obj[name()]
        if !no_computed_member && self.input_mut().eat(Token::LBracket) {
            let bracket_lo = self.input().prev_span().lo;
            let prop = self.allow_in_expr(|p| p.parse_expr())?;
            expect!(self, Token::RBracket);
            let span = Span::new_with_checked(callee.span_lo(), self.input().last_pos());
            debug_assert_eq!(callee.span_lo(), span.lo());
            let prop = ComputedPropName {
                span: Span::new_with_checked(bracket_lo, self.input().last_pos()),
                expr: prop,
            };

            let type_args = if self.syntax().typescript() && self.input().is(Token::Lt) {
                self.try_parse_ts_type_args()
            } else {
                None
            };

            let is_opt_chain = unwrap_ts_non_null(&callee).is_opt_chain();
            let expr = MemberExpr {
                span,
                obj: callee,
                prop: MemberProp::Computed(prop),
            };
            let expr = if is_opt_chain || question_dot {
                OptChainExpr {
                    span,
                    optional: question_dot,
                    base: Box::new(OptChainBase::Member(expr)),
                }
                .into()
            } else {
                expr.into()
            };

            let expr = if let Some(type_args) = type_args {
                Expr::TsInstantiation(TsInstantiation {
                    expr: Box::new(expr),
                    type_args,
                    span: self.span(start),
                })
            } else {
                expr
            };
            return Ok((Box::new(expr), true));
        }

        let type_args = if self.syntax().typescript() && self.input().is(Token::Lt) && question_dot
        {
            let ret = self.parse_ts_type_args()?;
            self.assert_and_bump(Token::Gt);
            Some(ret)
        } else {
            None
        };

        if (self.input.is(Token::LParen) && (!no_call || question_dot)) || type_args.is_some() {
            let args = self.parse_args(false)?;
            let span = self.span(start);
            return if question_dot || unwrap_ts_non_null(&callee).is_opt_chain() {
                let expr = OptChainExpr {
                    span,
                    optional: question_dot,
                    base: Box::new(OptChainBase::Call(OptCall {
                        span: self.span(start),
                        callee,
                        args,
                        type_args,
                        ..Default::default()
                    })),
                };
                Ok((Box::new(Expr::OptChain(expr)), true))
            } else {
                let expr = CallExpr {
                    span: self.span(start),
                    callee: Callee::Expr(callee),
                    args,
                    ..Default::default()
                };
                Ok((Box::new(Expr::Call(expr)), true))
            };
        }

        // member expression
        // $obj.name
        if question_dot || self.input_mut().eat(Token::Dot) {
            let prop = self.parse_maybe_private_name().map(|e| match e {
                Either::Left(p) => MemberProp::PrivateName(p),
                Either::Right(i) => MemberProp::Ident(i),
            })?;
            let span = self.span(callee.span_lo());
            debug_assert_eq!(callee.span_lo(), span.lo());
            debug_assert_eq!(prop.span_hi(), span.hi());

            let type_args = if self.syntax().typescript() && self.input().is(Token::Lt) {
                self.try_parse_ts_type_args()
            } else {
                None
            };

            let expr = MemberExpr {
                span,
                obj: callee,
                prop,
            };
            let expr = if unwrap_ts_non_null(&expr.obj).is_opt_chain() || question_dot {
                OptChainExpr {
                    span: self.span(start),
                    optional: question_dot,
                    base: Box::new(OptChainBase::Member(expr)),
                }
                .into()
            } else {
                expr.into()
            };

            let expr = if let Some(type_args) = type_args {
                Expr::TsInstantiation(TsInstantiation {
                    expr: Box::new(expr),
                    type_args,
                    span: self.span(start),
                })
            } else {
                expr
            };

            return Ok((Box::new(expr), true));
        }

        let expr = if let Some(type_args) = ts_instantiation {
            TsInstantiation {
                expr: callee,
                type_args,
                span: self.span(start),
            }
            .into()
        } else {
            callee
        };

        // MemberExpression[?Yield, ?Await] TemplateLiteral[?Yield, ?Await, +Tagged]
        let cur = self.input().cur();
        if matches!(
            cur,
            Token::TemplateHead | Token::NoSubstitutionTemplateLiteral | Token::BackQuote
        ) {
            let tpl = self.do_outside_of_context(Context::WillExpectColonForCond, |p| {
                p.parse_tagged_tpl(expr, None)
            })?;
            return Ok((tpl.into(), true));
        }

        Ok((expr, false))
    }

    /// Section 13.3 ImportCall
    #[cfg_attr(feature = "tracing-spans", tracing::instrument(skip_all))]
    fn parse_subscript_super(
        &mut self,
        start: BytePos,
        lhs: Super,
        no_call: bool,
    ) -> PResult<Box<Expr>> {
        trace_cur!(self, parse_subscript_super);
        match self.input().cur() {
            Token::LBracket => {
                self.bump();
                let bracket_lo = self.input().prev_span().lo;
                let prop = self.allow_in_expr(|p| p.parse_expr())?;
                expect!(self, Token::RBracket);
                let span = Span::new_with_checked(lhs.span_lo(), self.input().last_pos());
                debug_assert_eq!(lhs.span_lo(), span.lo());
                let prop = ComputedPropName {
                    span: Span::new_with_checked(bracket_lo, self.input().last_pos()),
                    expr: prop,
                };

                if !self.ctx().contains(Context::AllowDirectSuper)
                    && !self.input().syntax().allow_super_outside_method()
                {
                    syntax_error!(self, lhs.span, SyntaxError::InvalidSuper)
                } else {
                    Ok(Box::new(Expr::SuperProp(SuperPropExpr {
                        span,
                        obj: lhs,
                        prop: SuperProp::Computed(prop),
                    })))
                }
            }
            Token::LParen if !no_call => {
                let args = self.parse_args(false)?;
                Ok(Box::new(Expr::Call(CallExpr {
                    span: self.span(start),
                    callee: Callee::Super(lhs),
                    args,
                    ..Default::default()
                })))
            }
            Token::Dot => {
                self.bump();
                let prop = self.parse_maybe_private_name().map(|e| match e {
                    Either::Left(p) => MemberProp::PrivateName(p),
                    Either::Right(i) => MemberProp::Ident(i),
                })?;
                let span = self.span(lhs.span_lo());
                debug_assert_eq!(lhs.span_lo(), span.lo());
                debug_assert_eq!(prop.span_hi(), span.hi());

                if !self.ctx().contains(Context::AllowDirectSuper)
                    && !self.input().syntax().allow_super_outside_method()
                {
                    syntax_error!(self, lhs.span, SyntaxError::InvalidSuper);
                } else {
                    let expr = match prop {
                        MemberProp::Ident(ident) => SuperPropExpr {
                            span,
                            obj: lhs,
                            prop: SuperProp::Ident(ident),
                        },
                        MemberProp::PrivateName(..) => {
                            syntax_error!(
                                self,
                                self.input().cur_span(),
                                SyntaxError::InvalidSuperCall
                            )
                        }
                        MemberProp::Computed(..) => unreachable!(),
                        #[cfg(swc_ast_unknown)]
                        _ => unreachable!(),
                    };

                    Ok(Box::new(Expr::SuperProp(expr)))
                }
            }
            _ => {
                if no_call {
                    syntax_error!(self, self.input().cur_span(), SyntaxError::InvalidSuperCall)
                } else {
                    syntax_error!(self, self.input().cur_span(), SyntaxError::InvalidSuper)
                }
            }
        }
    }

    /// Section 13.3 ImportCall
    #[cfg_attr(feature = "tracing-spans", tracing::instrument(skip_all))]
    fn parse_subscript_import_call(&mut self, start: BytePos, lhs: Import) -> PResult<Box<Expr>> {
        trace_cur!(self, parse_subscript_import);

        if self.input().is(Token::LParen) {
            let args = self.parse_args(true)?;

            // Dynamic import requires exactly one or two arguments
            if args.is_empty() || args.len() > 2 {
                syntax_error!(
                    self,
                    self.span(start),
                    SyntaxError::ImportRequiresOneOrTwoArgs
                );
            }

            let expr = Box::new(Expr::Call(CallExpr {
                span: self.span(start),
                callee: Callee::Import(lhs),
                args,
                ..Default::default()
            }));
            return Ok(expr);
        }

        syntax_error!(self, self.input().cur_span(), SyntaxError::InvalidImport);
    }

    fn parse_dynamic_import_or_import_meta(
        &mut self,
        start: BytePos,
        no_call: bool,
    ) -> PResult<Box<Expr>> {
        if self.input_mut().eat(Token::Dot) {
            self.mark_found_module_item();

            let ident = self.parse_ident_name()?;

            match &*ident.sym {
                "meta" => {
                    let span = self.span(start);
                    if !self.ctx().contains(Context::CanBeModule) {
                        self.emit_err(span, SyntaxError::ImportMetaInScript);
                    }
                    let expr = MetaPropExpr {
                        span,
                        kind: MetaPropKind::ImportMeta,
                    };
                    self.parse_subscripts(Callee::Expr(expr.into()), no_call, false)
                }
                "defer" => self.parse_dynamic_import_call(start, ImportPhase::Defer),
                "source" => self.parse_dynamic_import_call(start, ImportPhase::Source),
                _ => unexpected!(self, "meta"),
            }
        } else {
            self.parse_dynamic_import_call(start, ImportPhase::Evaluation)
        }
    }

    fn parse_dynamic_import_call(
        &mut self,
        start: BytePos,
        phase: ImportPhase,
    ) -> PResult<Box<Expr>> {
        let import = Callee::Import(Import {
            span: self.span(start),
            phase,
        });
        self.parse_subscripts(import, false, false)
    }

    /// `is_new_expr`: true iff we are parsing production 'NewExpression'.
    #[cfg_attr(
        feature = "tracing-spans",
        tracing::instrument(level = "debug", skip_all)
    )]
    fn parse_member_expr_or_new_expr(&mut self, is_new_expr: bool) -> PResult<Box<Expr>> {
        self.do_inside_of_context(Context::ShouldNotLexLtOrGtAsType, |p| {
            p.parse_member_expr_or_new_expr_inner(is_new_expr)
        })
    }

    fn parse_member_expr_or_new_expr_inner(&mut self, is_new_expr: bool) -> PResult<Box<Expr>> {
        trace_cur!(self, parse_member_expr_or_new_expr);

        let start = self.cur_pos();
        if self.input_mut().eat(Token::New) {
            if self.input_mut().eat(Token::Dot) {
                if self.input_mut().eat(Token::Target) {
                    let span = self.span(start);
                    let expr = MetaPropExpr {
                        span,
                        kind: MetaPropKind::NewTarget,
                    }
                    .into();

                    let ctx = self.ctx();
                    if !ctx.contains(Context::InsideNonArrowFunctionScope)
                        && !ctx.contains(Context::InParameters)
                        && !ctx.contains(Context::InClass)
                    {
                        self.emit_err(span, SyntaxError::InvalidNewTarget);
                    }

                    return self.parse_subscripts(Callee::Expr(expr), true, false);
                }

                unexpected!(self, "target")
            }

            // 'NewExpression' allows new call without paren.
            let callee = self.parse_member_expr_or_new_expr(is_new_expr)?;
            return_if_arrow!(self, callee);

            if is_new_expr {
                match *callee {
                    Expr::OptChain(OptChainExpr {
                        span,
                        optional: true,
                        ..
                    }) => {
                        syntax_error!(self, span, SyntaxError::OptChainCannotFollowConstructorCall)
                    }
                    Expr::Member(MemberExpr { ref obj, .. }) => {
                        if let Expr::OptChain(OptChainExpr {
                            span,
                            optional: true,
                            ..
                        }) = **obj
                        {
                            syntax_error!(
                                self,
                                span,
                                SyntaxError::OptChainCannotFollowConstructorCall
                            )
                        }
                    }
                    _ => {}
                }
            }

            let type_args = if self.input().syntax().typescript() && {
                let cur = self.input().cur();
                cur == Token::Lt || cur == Token::LShift
            } {
                self.try_parse_ts(|p| {
                    let args = p.do_outside_of_context(
                        Context::ShouldNotLexLtOrGtAsType,
                        Self::parse_ts_type_args,
                    )?;
                    p.assert_and_bump(Token::Gt);
                    if !p.input().is(Token::LParen) {
                        let span = p.input().cur_span();
                        let cur = p.input_mut().dump_cur();
                        syntax_error!(p, span, SyntaxError::Expected('('.to_string(), cur))
                    }
                    Ok(Some(args))
                })
            } else {
                None
            };

            if !is_new_expr || self.input().is(Token::LParen) {
                // Parsed with 'MemberExpression' production.
                let args = self.parse_args(false).map(Some)?;

                let new_expr = Callee::Expr(
                    NewExpr {
                        span: self.span(start),
                        callee,
                        args,
                        type_args,
                        ..Default::default()
                    }
                    .into(),
                );

                // We should parse subscripts for MemberExpression.
                // Because it's left recursive.
                return self.parse_subscripts(new_expr, true, false);
            }

            // Parsed with 'NewExpression' production.

            return Ok(NewExpr {
                span: self.span(start),
                callee,
                args: None,
                type_args,
                ..Default::default()
            }
            .into());
        }

        if self.input_mut().eat(Token::Super) {
            let base = Callee::Super(Super {
                span: self.span(start),
            });
            return self.parse_subscripts(base, true, false);
        } else if self.input_mut().eat(Token::Import) {
            return self.parse_dynamic_import_or_import_meta(start, true);
        }
        let obj = self.parse_primary_expr()?;
        return_if_arrow!(self, obj);

        let type_args = if self.syntax().typescript() && self.input().is(Token::Lt) {
            self.try_parse_ts_type_args()
        } else {
            None
        };
        let obj = if let Some(type_args) = type_args {
            trace_cur!(self, parse_member_expr_or_new_expr__with_type_args);
            TsInstantiation {
                expr: obj,
                type_args,
                span: self.span(start),
            }
            .into()
        } else {
            obj
        };

        self.parse_subscripts(Callee::Expr(obj), true, false)
    }

    /// Parse `NewExpression`.
    /// This includes `MemberExpression`.
    #[cfg_attr(feature = "tracing-spans", tracing::instrument(skip_all))]
    pub(crate) fn parse_new_expr(&mut self) -> PResult<Box<Expr>> {
        trace_cur!(self, parse_new_expr);
        self.parse_member_expr_or_new_expr(true)
    }

    /// Name from spec: 'LogicalORExpression'
    pub(crate) fn parse_bin_expr(&mut self) -> PResult<Box<Expr>> {
        trace_cur!(self, parse_bin_expr);

        let left = match self.parse_unary_expr() {
            Ok(v) => v,
            Err(err) => {
                trace_cur!(self, parse_bin_expr__recovery_unary_err);

                let cur = self.input().cur();
                if cur == Token::Error {
                    let err = self.input_mut().expect_error_token_and_bump();
                    return Err(err);
                } else if (cur == Token::In && self.ctx().contains(Context::IncludeInExpr))
                    || cur == Token::InstanceOf
                    || cur.is_bin_op()
                {
                    self.emit_err(self.input().cur_span(), SyntaxError::TS1109);
                    Invalid { span: err.span() }.into()
                } else {
                    return Err(err);
                }
            }
        };

        return_if_arrow!(self, left);
        self.parse_bin_op_recursively(left, 0)
    }

    /// Parse binary operators with the operator precedence parsing
    /// algorithm. `left` is the left-hand side of the operator.
    /// `minPrec` provides context that allows the function to stop and
    /// defer further parser to one of its callers when it encounters an
    /// operator that has a lower precedence than the set it is parsing.
    ///
    /// `parseExprOp`
    pub(crate) fn parse_bin_op_recursively(
        &mut self,
        mut left: Box<Expr>,
        mut min_prec: u8,
    ) -> PResult<Box<Expr>> {
        loop {
            let (next_left, next_prec) = self.parse_bin_op_recursively_inner(left, min_prec)?;

            match &*next_left {
                Expr::Bin(BinExpr {
                    span,
                    left,
                    op: op!("&&"),
                    ..
                })
                | Expr::Bin(BinExpr {
                    span,
                    left,
                    op: op!("||"),
                    ..
                }) => {
                    if let Expr::Bin(BinExpr { op: op!("??"), .. }) = &**left {
                        self.emit_err(*span, SyntaxError::NullishCoalescingWithLogicalOp);
                    }
                }
                _ => {}
            }

            min_prec = match next_prec {
                Some(v) => v,
                None => return Ok(next_left),
            };

            left = next_left;
        }
    }

    /// Returns `(left, Some(next_prec))` or `(expr, None)`.
    fn parse_bin_op_recursively_inner(
        &mut self,
        left: Box<Expr>,
        min_prec: u8,
    ) -> PResult<(Box<Expr>, Option<u8>)> {
        const PREC_OF_IN: u8 = 7;

        if self.input().syntax().typescript() && !self.input().had_line_break_before_cur() {
            if PREC_OF_IN > min_prec && self.input().is(Token::As) {
                let start = left.span_lo();
                let expr = left;
                let node = if peek!(self).is_some_and(|cur| cur == Token::Const) {
                    self.bump(); // as
                    self.bump(); // const
                    TsConstAssertion {
                        span: self.span(start),
                        expr,
                    }
                    .into()
                } else {
                    let type_ann = self.next_then_parse_ts_type()?;
                    TsAsExpr {
                        span: self.span(start),
                        expr,
                        type_ann,
                    }
                    .into()
                };

                return self.parse_bin_op_recursively_inner(node, min_prec);
            } else if self.input().is(Token::Satisfies) {
                let start = left.span_lo();
                let expr = left;
                let node = {
                    let type_ann = self.next_then_parse_ts_type()?;
                    TsSatisfiesExpr {
                        span: self.span(start),
                        expr,
                        type_ann,
                    }
                    .into()
                };

                return self.parse_bin_op_recursively_inner(node, min_prec);
            }
        }

        // Return left on eof
        let cur = self.input().cur();
        let op = if cur == Token::In && self.ctx().contains(Context::IncludeInExpr) {
            op!("in")
        } else if cur == Token::InstanceOf {
            op!("instanceof")
        } else if let Some(op) = cur.as_bin_op() {
            op
        } else {
            return Ok((left, None));
        };

        if op.precedence() <= min_prec {
            if cfg!(feature = "debug") {
                tracing::trace!(
                    "returning {:?} without parsing {:?} because min_prec={}, prec={}",
                    left,
                    op,
                    min_prec,
                    op.precedence()
                );
            }

            return Ok((left, None));
        }
        self.bump();
        if cfg!(feature = "debug") {
            tracing::trace!(
                "parsing binary op {:?} min_prec={}, prec={}",
                op,
                min_prec,
                op.precedence()
            );
        }
        match *left {
            // This is invalid syntax.
            Expr::Unary { .. } | Expr::Await(..) if op == op!("**") => {
                // Correct implementation would be returning Ok(left) and
                // returning "unexpected token '**'" on next.
                // But it's not useful error message.

                syntax_error!(
                    self,
                    SyntaxError::UnaryInExp {
                        // FIXME: Use display
                        left: format!("{left:?}"),
                        left_span: left.span(),
                    }
                )
            }
            _ => {}
        }

        let right = {
            let left_of_right = self.parse_unary_expr()?;
            self.parse_bin_op_recursively(
                left_of_right,
                if op == op!("**") {
                    // exponential operator is right associative
                    op.precedence() - 1
                } else {
                    op.precedence()
                },
            )?
        };
        /* this check is for all ?? operators
         * a ?? b && c for this example
         * b && c => This is considered as a logical expression in the ast tree
         * a => Identifier
         * so for ?? operator we need to check in this case the right expression to
         * have parenthesis second case a && b ?? c
         * here a && b => This is considered as a logical expression in the ast tree
         * c => identifier
         * so now here for ?? operator we need to check the left expression to have
         * parenthesis if the parenthesis is missing we raise an error and
         * throw it
         */
        if op == op!("??") {
            match *left {
                Expr::Bin(BinExpr { span, op, .. }) if op == op!("&&") || op == op!("||") => {
                    self.emit_err(span, SyntaxError::NullishCoalescingWithLogicalOp);
                }
                _ => {}
            }

            match *right {
                Expr::Bin(BinExpr { span, op, .. }) if op == op!("&&") || op == op!("||") => {
                    self.emit_err(span, SyntaxError::NullishCoalescingWithLogicalOp);
                }
                _ => {}
            }
        }

        let node = BinExpr {
            span: Span::new_with_checked(left.span_lo(), right.span_hi()),
            op,
            left,
            right,
        }
        .into();

        Ok((node, Some(min_prec)))
    }

    pub(crate) fn parse_await_expr(
        &mut self,
        start_of_await_token: Option<BytePos>,
    ) -> PResult<Box<Expr>> {
        let start = start_of_await_token.unwrap_or_else(|| self.cur_pos());

        if start_of_await_token.is_none() {
            self.assert_and_bump(Token::Await);
        }

        let await_token = self.span(start);

        if self.input().is(Token::Asterisk) {
            syntax_error!(self, SyntaxError::AwaitStar);
        }

        let ctx = self.ctx();

        let span = self.span(start);

        if !ctx.contains(Context::InAsync)
            && (self.is_general_semi() || {
                let cur = self.input().cur();
                matches!(cur, Token::RParen | Token::RBracket | Token::Comma)
            })
        {
            if ctx.contains(Context::Module) {
                self.emit_err(span, SyntaxError::InvalidIdentInAsync);
            }

            return Ok(Ident::new_no_ctxt(atom!("await"), span).into());
        }

        // This has been checked if start_of_await_token == true,
        if start_of_await_token.is_none() && ctx.contains(Context::TopLevel) {
            self.mark_found_module_item();
            if !ctx.contains(Context::CanBeModule) {
                self.emit_err(await_token, SyntaxError::TopLevelAwaitInScript);
            }
        }

        if ctx.contains(Context::InFunction) && !ctx.contains(Context::InAsync) {
            self.emit_err(await_token, SyntaxError::AwaitInFunction);
        }

        if ctx.contains(Context::InParameters) && !ctx.contains(Context::InFunction) {
            self.emit_err(span, SyntaxError::AwaitParamInAsync);
        }

        let arg = self.parse_unary_expr()?;
        Ok(AwaitExpr {
            span: self.span(start),
            arg,
        }
        .into())
    }

    pub(crate) fn parse_for_head_prefix(&mut self) -> PResult<Box<Expr>> {
        self.parse_expr()
    }

    // Returns (args_or_pats, trailing_comma)
    #[cfg_attr(
        feature = "tracing-spans",
        tracing::instrument(level = "debug", skip_all)
    )]
    fn parse_args_or_pats(&mut self) -> PResult<(Vec<AssignTargetOrSpread>, Option<Span>)> {
        self.do_outside_of_context(
            Context::WillExpectColonForCond,
            Self::parse_args_or_pats_inner,
        )
    }

    fn parse_args_or_pats_inner(&mut self) -> PResult<(Vec<AssignTargetOrSpread>, Option<Span>)> {
        trace_cur!(self, parse_args_or_pats);

        expect!(self, Token::LParen);

        let mut items = Vec::new();
        let mut trailing_comma = None;

        // TODO(kdy1): optimize (once we parsed a pattern, we can parse everything else
        // as a pattern instead of reparsing)
        while !self.input().is(Token::RParen) {
            // https://github.com/swc-project/swc/issues/410
            let is_async = self.input().is(Token::Async)
                && peek!(self)
                    .is_some_and(|t| t == Token::LParen || t == Token::Function || t.is_word());

            let start = self.cur_pos();
            self.state_mut().potential_arrow_start = Some(start);
            let modifier_start = start;

            let has_modifier = self.eat_any_ts_modifier()?;
            let pat_start = self.cur_pos();

            let mut arg = {
                if self.input().syntax().typescript()
                    && (self.is_ident_ref()
                        || (self.input().is(Token::DotDotDot) && self.peek_is_ident_ref()))
                {
                    let spread = if self.input_mut().eat(Token::DotDotDot) {
                        Some(self.input().prev_span())
                    } else {
                        None
                    };

                    // At here, we use parse_bin_expr() instead of parse_assignment_expr()
                    // because `x?: number` should not be parsed as a conditional expression
                    let expr = if spread.is_some() {
                        self.parse_bin_expr()?
                    } else {
                        let mut expr = self.parse_bin_expr()?;

                        if self.input().cur().is_assign_op() {
                            expr = self.finish_assignment_expr(start, expr)?
                        }

                        expr
                    };

                    ExprOrSpread { spread, expr }
                } else {
                    self.allow_in_expr(|p| p.parse_expr_or_spread())?
                }
            };

            let optional = if self.input().syntax().typescript() {
                if self.input().is(Token::QuestionMark) {
                    if peek!(self).is_some_and(|peek| {
                        matches!(
                            peek,
                            Token::Comma | Token::Eq | Token::RParen | Token::Colon
                        )
                    }) {
                        self.assert_and_bump(Token::QuestionMark);
                        if arg.spread.is_some() {
                            self.emit_err(self.input().prev_span(), SyntaxError::TS1047);
                        }
                        match *arg.expr {
                            Expr::Ident(..) => {}
                            _ => {
                                syntax_error!(
                                    self,
                                    arg.span(),
                                    SyntaxError::TsBindingPatCannotBeOptional
                                )
                            }
                        }
                        true
                    } else if matches!(arg, ExprOrSpread { spread: None, .. }) {
                        expect!(self, Token::QuestionMark);
                        let test = arg.expr;

                        let cons = self.do_inside_of_context(
                            Context::InCondExpr
                                .union(Context::WillExpectColonForCond)
                                .union(Context::IncludeInExpr),
                            Self::parse_assignment_expr,
                        )?;
                        expect!(self, Token::Colon);

                        let alt = self.do_inside_of_context(Context::InCondExpr, |p| {
                            p.do_outside_of_context(
                                Context::WillExpectColonForCond,
                                Self::parse_assignment_expr,
                            )
                        })?;

                        arg = ExprOrSpread {
                            spread: None,
                            expr: CondExpr {
                                span: Span::new_with_checked(start, alt.span_hi()),
                                test,
                                cons,
                                alt,
                            }
                            .into(),
                        };

                        false
                    } else {
                        false
                    }
                } else {
                    false
                }
            } else {
                false
            };

            if optional || (self.input().syntax().typescript() && self.input().is(Token::Colon)) {
                // TODO: `async(...args?: any[]) : any => {}`
                //
                // if self.input().syntax().typescript() && optional && arg.spread.is_some() {
                //     self.emit_err(self.input().prev_span(), SyntaxError::TS1047)
                // }

                let mut pat = self.reparse_expr_as_pat(PatType::BindingPat, arg.expr)?;
                if optional {
                    match pat {
                        Pat::Ident(ref mut i) => i.optional = true,
                        _ => unreachable!(),
                    }
                }
                if let Some(span) = arg.spread {
                    pat = RestPat {
                        span: self.span(pat_start),
                        dot3_token: span,
                        arg: Box::new(pat),
                        type_ann: None,
                    }
                    .into();
                }
                match pat {
                    Pat::Ident(BindingIdent {
                        id: Ident { ref mut span, .. },
                        ref mut type_ann,
                        ..
                    })
                    | Pat::Array(ArrayPat {
                        ref mut type_ann,
                        ref mut span,
                        ..
                    })
                    | Pat::Object(ObjectPat {
                        ref mut type_ann,
                        ref mut span,
                        ..
                    })
                    | Pat::Rest(RestPat {
                        ref mut type_ann,
                        ref mut span,
                        ..
                    }) => {
                        let new_type_ann = self.try_parse_ts_type_ann()?;
                        if new_type_ann.is_some() {
                            *span = Span::new_with_checked(pat_start, self.input().prev_span().hi);
                        }
                        *type_ann = new_type_ann;
                    }
                    Pat::Expr(ref expr) => unreachable!("invalid pattern: Expr({:?})", expr),
                    Pat::Assign(..) | Pat::Invalid(..) => {
                        // We don't have to panic here.
                        // See: https://github.com/swc-project/swc/issues/1170
                        //
                        // Also, as an exact error is added to the errors while
                        // creating `Invalid`, we don't have to emit a new
                        // error.
                    }
                    #[cfg(swc_ast_unknown)]
                    _ => unreachable!(),
                }

                if self.input_mut().eat(Token::Eq) {
                    let right = self.parse_assignment_expr()?;
                    pat = AssignPat {
                        span: self.span(pat_start),
                        left: Box::new(pat),
                        right,
                    }
                    .into();
                }

                if has_modifier {
                    self.emit_err(self.span(modifier_start), SyntaxError::TS2369);
                }

                items.push(AssignTargetOrSpread::Pat(pat))
            } else {
                if has_modifier {
                    self.emit_err(self.span(modifier_start), SyntaxError::TS2369);
                }

                items.push(AssignTargetOrSpread::ExprOrSpread(arg));
            }

            // https://github.com/swc-project/swc/issues/433
            if self.input_mut().eat(Token::Arrow) && {
                debug_assert_eq!(items.len(), 1);
                match items[0] {
                    AssignTargetOrSpread::ExprOrSpread(ExprOrSpread { ref expr, .. })
                    | AssignTargetOrSpread::Pat(Pat::Expr(ref expr)) => {
                        matches!(**expr, Expr::Ident(..))
                    }
                    AssignTargetOrSpread::Pat(Pat::Ident(..)) => true,
                    _ => false,
                }
            } {
                let params: Vec<Pat> = self.parse_paren_items_as_params(items.clone(), None)?;

                let body: Box<BlockStmtOrExpr> = self.parse_fn_block_or_expr_body(
                    false,
                    false,
                    true,
                    params.is_simple_parameter_list(),
                )?;
                let span = self.span(start);

                items.push(AssignTargetOrSpread::ExprOrSpread(ExprOrSpread {
                    expr: Box::new(
                        ArrowExpr {
                            span,
                            body,
                            is_async,
                            is_generator: false,
                            params,
                            ..Default::default()
                        }
                        .into(),
                    ),
                    spread: None,
                }));
            }

            if !self.input().is(Token::RParen) {
                expect!(self, Token::Comma);
                if self.input().is(Token::RParen) {
                    trailing_comma = Some(self.input().prev_span());
                }
            }
        }

        expect!(self, Token::RParen);
        Ok((items, trailing_comma))
    }

    #[cfg_attr(
        feature = "tracing-spans",
        tracing::instrument(level = "debug", skip_all)
    )]
    fn parse_paren_expr_or_arrow_fn(
        &mut self,
        can_be_arrow: bool,
        async_span: Option<Span>,
    ) -> PResult<Box<Expr>> {
        trace_cur!(self, parse_paren_expr_or_arrow_fn);

        let expr_start = async_span.map(|x| x.lo()).unwrap_or_else(|| self.cur_pos());

        // At this point, we can't know if it's parenthesized
        // expression or head of arrow function.
        // But as all patterns of javascript is subset of
        // expressions, we can parse both as expression.

        let (paren_items, trailing_comma) = self
            .do_outside_of_context(Context::WillExpectColonForCond, |p| {
                p.allow_in_expr(Self::parse_args_or_pats)
            })?;

        let has_pattern = paren_items
            .iter()
            .any(|item| matches!(item, AssignTargetOrSpread::Pat(..)));

        let will_expect_colon_for_cond = self.ctx().contains(Context::WillExpectColonForCond);
        // This is slow path. We handle arrow in conditional expression.
        if self.syntax().typescript()
            && self.ctx().contains(Context::InCondExpr)
            && self.input().is(Token::Colon)
        {
            // TODO: Remove clone
            let items_ref = &paren_items;
            if let Some(expr) = self.try_parse_ts(|p| {
                let return_type = p.parse_ts_type_or_type_predicate_ann(Token::Colon)?;

                expect!(p, Token::Arrow);

                let params: Vec<Pat> =
                    p.parse_paren_items_as_params(items_ref.clone(), trailing_comma)?;

                let body: Box<BlockStmtOrExpr> = p.parse_fn_block_or_expr_body(
                    async_span.is_some(),
                    false,
                    true,
                    params.is_simple_parameter_list(),
                )?;

                if will_expect_colon_for_cond && !p.input().is(Token::Colon) {
                    trace_cur!(p, parse_arrow_in_cond__fail);
                    unexpected!(p, "fail")
                }

                Ok(Some(
                    ArrowExpr {
                        span: p.span(expr_start),
                        is_async: async_span.is_some(),
                        is_generator: false,
                        params,
                        body,
                        return_type: Some(return_type),
                        ..Default::default()
                    }
                    .into(),
                ))
            }) {
                return Ok(expr);
            }
        }

        let return_type = if !self.ctx().contains(Context::WillExpectColonForCond)
            && self.input().syntax().typescript()
            && self.input().is(Token::Colon)
        {
            self.try_parse_ts(|p| {
                let return_type = p.parse_ts_type_or_type_predicate_ann(Token::Colon)?;

                if !p.input().is(Token::Arrow) {
                    unexpected!(p, "fail")
                }

                Ok(Some(return_type))
            })
        } else {
            None
        };

        // we parse arrow function at here, to handle it efficiently.
        if has_pattern || return_type.is_some() || self.input().is(Token::Arrow) {
            if self.input().had_line_break_before_cur() {
                syntax_error!(
                    self,
                    self.span(expr_start),
                    SyntaxError::LineBreakBeforeArrow
                );
            }

            if !can_be_arrow {
                syntax_error!(self, self.span(expr_start), SyntaxError::ArrowNotAllowed);
            }
            expect!(self, Token::Arrow);

            let params: Vec<Pat> = self.parse_paren_items_as_params(paren_items, trailing_comma)?;

            let body: Box<BlockStmtOrExpr> = self.parse_fn_block_or_expr_body(
                async_span.is_some(),
                false,
                true,
                params.is_simple_parameter_list(),
            )?;
            let arrow_expr = ArrowExpr {
                span: self.span(expr_start),
                is_async: async_span.is_some(),
                is_generator: false,
                params,
                body,
                return_type,
                ..Default::default()
            };
            if let BlockStmtOrExpr::BlockStmt(..) = &*arrow_expr.body {
                if self.input().cur().is_bin_op() {
                    // ) is required
                    self.emit_err(self.input().cur_span(), SyntaxError::TS1005);
                    let errorred_expr =
                        self.parse_bin_op_recursively(Box::new(arrow_expr.into()), 0)?;

                    if !self.is_general_semi() {
                        // ; is required
                        self.emit_err(self.input().cur_span(), SyntaxError::TS1005);
                    }

                    return Ok(errorred_expr);
                }
            }
            return Ok(arrow_expr.into());
        } else {
            // If there's no arrow function, we have to check there's no
            // AssignProp in lhs to check against assignment in object literals
            // like (a, {b = 1});
            for expr_or_spread in paren_items.iter() {
                if let AssignTargetOrSpread::ExprOrSpread(e) = expr_or_spread {
                    if let Expr::Object(o) = &*e.expr {
                        for prop in o.props.iter() {
                            if let PropOrSpread::Prop(prop) = prop {
                                if let Prop::Assign(..) = **prop {
                                    self.emit_err(prop.span(), SyntaxError::AssignProperty);
                                }
                            }
                        }
                    }
                }
            }
        }

        let expr_or_spreads = paren_items
            .into_iter()
            .map(|item| -> PResult<_> {
                match item {
                    AssignTargetOrSpread::ExprOrSpread(e) => Ok(e),
                    _ => syntax_error!(self, item.span(), SyntaxError::InvalidExpr),
                }
            })
            .collect::<Result<Vec<_>, _>>()?;
        if let Some(async_span) = async_span {
            // It's a call expression
            return Ok(CallExpr {
                span: self.span(async_span.lo()),
                callee: Callee::Expr(Box::new(
                    Ident::new_no_ctxt(atom!("async"), async_span).into(),
                )),
                args: expr_or_spreads,
                ..Default::default()
            }
            .into());
        }

        // It was not head of arrow function.

        if expr_or_spreads.is_empty() {
            syntax_error!(
                self,
                Span::new_with_checked(expr_start, self.last_pos()),
                SyntaxError::EmptyParenExpr
            );
        }

        // TODO: Verify that invalid expression like {a = 1} does not exists.

        // ParenthesizedExpression cannot contain spread.
        if expr_or_spreads.len() == 1 {
            let expr = match expr_or_spreads.into_iter().next().unwrap() {
                ExprOrSpread {
                    spread: Some(..),
                    ref expr,
                } => syntax_error!(self, expr.span(), SyntaxError::SpreadInParenExpr),
                ExprOrSpread { expr, .. } => expr,
            };
            Ok(ParenExpr {
                span: self.span(expr_start),
                expr,
            }
            .into())
        } else {
            debug_assert!(expr_or_spreads.len() >= 2);

            let mut exprs = Vec::with_capacity(expr_or_spreads.len());
            for expr in expr_or_spreads {
                match expr {
                    ExprOrSpread {
                        spread: Some(..),
                        ref expr,
                    } => syntax_error!(self, expr.span(), SyntaxError::SpreadInParenExpr),
                    ExprOrSpread { expr, .. } => exprs.push(expr),
                }
            }
            debug_assert!(exprs.len() >= 2);

            // span of sequence expression should not include '(', ')'
            let seq_expr = SeqExpr {
                span: Span::new_with_checked(
                    exprs.first().unwrap().span_lo(),
                    exprs.last().unwrap().span_hi(),
                ),
                exprs,
            }
            .into();
            Ok(ParenExpr {
                span: self.span(expr_start),
                expr: seq_expr,
            }
            .into())
        }
    }

    fn parse_primary_expr_rest(
        &mut self,
        start: BytePos,
        can_be_arrow: bool,
    ) -> PResult<Box<Expr>> {
        let decorators = if self.input().is(Token::At) {
            Some(self.parse_decorators(false)?)
        } else {
            None
        };

        let token_and_span = self.input().get_cur();
        let cur = token_and_span.token;

        if cur == Token::Class {
            return self.parse_class_expr(start, decorators.unwrap_or_default());
        }

        let try_parse_arrow_expr = |p: &mut Self, id: Ident, id_is_async| -> PResult<Box<Expr>> {
            if can_be_arrow && !p.input().had_line_break_before_cur() {
                if id_is_async && p.is_ident_ref() {
                    // see https://github.com/tc39/ecma262/issues/2034
                    // ```js
                    // for(async of
                    // for(async of x);
                    // for(async of =>{};;);
                    // ```
                    let ctx = p.ctx();
                    if ctx.contains(Context::ForLoopInit)
                        && p.input().is(Token::Of)
                        && !peek!(p).is_some_and(|peek| peek == Token::Arrow)
                    {
                        // ```spec https://tc39.es/ecma262/#prod-ForInOfStatement
                        // for ( [lookahead ∉ { let, async of }] LeftHandSideExpression[?Yield, ?Await] of AssignmentExpression[+In, ?Yield, ?Await] ) Statement[?Yield, ?Await, ?Return]
                        // [+Await] for await ( [lookahead ≠ let] LeftHandSideExpression[?Yield, ?Await] of AssignmentExpression[+In, ?Yield, ?Await] ) Statement[?Yield, ?Await, ?Return]
                        // ```

                        if !ctx.contains(Context::ForAwaitLoopInit) {
                            p.emit_err(p.input().prev_span(), SyntaxError::TS1106);
                        }

                        return Ok(id.into());
                    }

                    let ident = p.parse_binding_ident(false)?;
                    if p.input().syntax().typescript()
                        && ident.sym == "as"
                        && !p.input().is(Token::Arrow)
                    {
                        // async as type
                        let type_ann = p.in_type(Self::parse_ts_type)?;
                        return Ok(TsAsExpr {
                            span: p.span(start),
                            expr: Box::new(id.into()),
                            type_ann,
                        }
                        .into());
                    }

                    // async a => body
                    let arg = ident.into();
                    let params = vec![arg];
                    expect!(p, Token::Arrow);
                    let body = p.parse_fn_block_or_expr_body(
                        true,
                        false,
                        true,
                        params.is_simple_parameter_list(),
                    )?;

                    return Ok(ArrowExpr {
                        span: p.span(start),
                        body,
                        params,
                        is_async: true,
                        is_generator: false,
                        ..Default::default()
                    }
                    .into());
                } else if p.input_mut().eat(Token::Arrow) {
                    if p.ctx().contains(Context::Strict) && id.is_reserved_in_strict_bind() {
                        p.emit_strict_mode_err(id.span, SyntaxError::EvalAndArgumentsInStrict)
                    }
                    let params = vec![id.into()];
                    let body = p.parse_fn_block_or_expr_body(
                        false,
                        false,
                        true,
                        params.is_simple_parameter_list(),
                    )?;

                    return Ok(ArrowExpr {
                        span: p.span(start),
                        body,
                        params,
                        is_async: false,
                        is_generator: false,
                        ..Default::default()
                    }
                    .into());
                }
            }

            Ok(id.into())
        };

        let token_start = token_and_span.span.lo;
        if cur == Token::Let || (self.input().syntax().typescript() && cur == Token::Await) {
            let ctx = self.ctx();
            let id = self.parse_ident(
                !ctx.contains(Context::InGenerator),
                !ctx.contains(Context::InAsync),
            )?;
            try_parse_arrow_expr(self, id, false)
        } else if cur == Token::Hash {
            self.bump(); // consume `#`
            let id = self.parse_ident_name()?;
            Ok(PrivateName {
                span: self.span(start),
                name: id.sym,
            }
            .into())
        } else if cur == Token::Ident {
            let word = self.input_mut().expect_word_token_and_bump();
            if self.ctx().contains(Context::InClassField) && word == atom!("arguments") {
                self.emit_err(self.input().prev_span(), SyntaxError::ArgumentsInClassField)
            };
            let id = Ident::new_no_ctxt(word, self.span(token_start));
            try_parse_arrow_expr(self, id, false)
        } else if self.is_ident_ref() {
            let id_is_async = self.input().cur() == Token::Async;
            let word = self.input_mut().expect_word_token_and_bump();
            let id = Ident::new_no_ctxt(word, self.span(token_start));
            try_parse_arrow_expr(self, id, id_is_async)
        } else {
            syntax_error!(self, self.input().cur_span(), SyntaxError::TS1109)
        }
    }

    fn try_parse_regexp(&mut self, start: BytePos) -> Option<Box<Expr>> {
        // Regexp
        debug_assert!(self.input().cur() == Token::Slash || self.input().cur() == Token::DivEq);

        self.input_mut().set_next_regexp(Some(start));

        self.bump(); // `/` or `/=`

        let cur = self.input().cur();
        if cur == Token::Regex {
            self.input_mut().set_next_regexp(None);
            let token_span = self.input.cur_span();
            let exp_end = self.input_mut().expect_regex_token_value();

            let exp_start = token_span.lo + BytePos(1); // +1 to exclude left `/`
            let exp = Atom::new(
                self.input
                    .iter
                    .read_string(Span::new_with_checked(exp_start, exp_end)),
            );
            let flags_start = exp_end + BytePos(1); // +1 to exclude right `/`
            let flags = if flags_start <= token_span.hi {
                Atom::new(
                    self.input
                        .iter
                        .read_string(Span::new_with_checked(flags_start, token_span.hi)),
                )
            } else {
                Atom::default()
            };

            self.bump();
            let span = self.span(start);
            Some(Lit::Regex(Regex { span, exp, flags }).into())
        } else {
            None
        }
    }

    fn try_parse_async_start(&mut self, can_be_arrow: bool) -> Option<PResult<Box<Expr>>> {
        if peek!(self).is_some_and(|peek| peek == Token::Function)
            && !self.input_mut().has_linebreak_between_cur_and_peeked()
        {
            // handle `async function` expression
            return Some(self.parse_async_fn_expr());
        }

        if can_be_arrow
            && self.input().syntax().typescript()
            && peek!(self).is_some_and(|peek| peek == Token::Lt)
        {
            // try parsing `async<T>() => {}`
            if let Some(res) = self.try_parse_ts(|p| {
                let start = p.cur_pos();
                p.assert_and_bump(Token::Async);
                p.try_parse_ts_generic_async_arrow_fn(start)
            }) {
                return Some(Ok(res.into()));
            }
        }

        if can_be_arrow
            && peek!(self).is_some_and(|peek| peek == Token::LParen)
            && !self.input_mut().has_linebreak_between_cur_and_peeked()
        {
            if let Err(e) = self.expect(Token::Async) {
                return Some(Err(e));
            }
            let async_span = self.input().prev_span();
            return Some(self.parse_paren_expr_or_arrow_fn(can_be_arrow, Some(async_span)));
        }

        None
    }

    fn parse_this_expr(&mut self, start: BytePos) -> PResult<Box<Expr>> {
        debug_assert!(self.input().cur() == Token::This);
        self.input_mut().bump();
        Ok(ThisExpr {
            span: self.span(start),
        }
        .into())
    }

    #[cfg(feature = "typescript")]
    pub(crate) fn is_start_of_left_hand_side_expr(&mut self) -> bool {
        let cur = self.input().cur();
        matches!(
            cur,
            Token::This
                | Token::Null
                | Token::Super
                | Token::True
                | Token::False
                | Token::Num
                | Token::BigInt
                | Token::Str
                | Token::NoSubstitutionTemplateLiteral
                | Token::TemplateHead
                | Token::LParen
                | Token::LBracket
                | Token::Function
                | Token::Class
                | Token::New
                | Token::Regex
                | Token::Import
        ) || cur.is_ident_ref(self.ctx())
            || cur == Token::BackQuote && {
                peek!(self)
                    .is_some_and(|peek| matches!(peek, Token::LParen | Token::Lt | Token::Dot))
            }
    }
}

fn unwrap_ts_non_null(mut expr: &Expr) -> &Expr {
    while let Expr::TsNonNull(ts_non_null) = expr {
        expr = &ts_non_null.expr;
    }

    expr
}
