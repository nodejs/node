//! 13.3.3 Destructuring Binding Patterns

use swc_common::Spanned;

use super::*;
use crate::parser::{expr::AssignTargetOrSpread, Parser};

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub(crate) enum PatType {
    BindingPat,
    BindingElement,
    /// AssignmentPattern
    AssignPat,
    AssignElement,
}

impl PatType {
    fn element(self) -> Self {
        match self {
            PatType::BindingPat | PatType::BindingElement => PatType::BindingElement,
            PatType::AssignPat | PatType::AssignElement => PatType::AssignElement,
        }
    }
}

impl<I: Tokens> Parser<I> {
    pub fn parse_pat(&mut self) -> PResult<Pat> {
        self.parse_binding_pat_or_ident(false)
    }

    /// argument of arrow is pattern, although idents in pattern is already
    /// checked if is a keyword, it should also be checked if is arguments or
    /// eval
    fn pat_is_valid_argument_in_strict(&mut self, pat: &Pat) {
        debug_assert!(self.ctx().contains(Context::Strict));
        match pat {
            Pat::Ident(i) => {
                if i.is_reserved_in_strict_bind() {
                    self.emit_strict_mode_err(i.span, SyntaxError::EvalAndArgumentsInStrict)
                }
            }
            Pat::Array(arr) => {
                for pat in arr.elems.iter().flatten() {
                    self.pat_is_valid_argument_in_strict(pat)
                }
            }
            Pat::Rest(r) => self.pat_is_valid_argument_in_strict(&r.arg),
            Pat::Object(obj) => {
                for prop in obj.props.iter() {
                    match prop {
                        ObjectPatProp::KeyValue(KeyValuePatProp { value, .. })
                        | ObjectPatProp::Rest(RestPat { arg: value, .. }) => {
                            self.pat_is_valid_argument_in_strict(value)
                        }
                        ObjectPatProp::Assign(AssignPatProp { key, .. }) => {
                            if key.is_reserved_in_strict_bind() {
                                self.emit_strict_mode_err(
                                    key.span,
                                    SyntaxError::EvalAndArgumentsInStrict,
                                )
                            }
                        }
                        #[cfg(swc_ast_unknown)]
                        _ => unreachable!(),
                    }
                }
            }
            Pat::Assign(a) => self.pat_is_valid_argument_in_strict(&a.left),
            Pat::Invalid(_) | Pat::Expr(_) => (),
            #[cfg(swc_ast_unknown)]
            _ => unreachable!(),
        }
    }

    /// This does not return 'rest' pattern because non-last parameter cannot be
    /// rest.
    pub(super) fn reparse_expr_as_pat(&mut self, pat_ty: PatType, expr: Box<Expr>) -> PResult<Pat> {
        if let Expr::Invalid(i) = *expr {
            return Ok(i.into());
        }
        if pat_ty == PatType::AssignPat {
            match *expr {
                Expr::Object(..) | Expr::Array(..) => {
                    // It is a Syntax Error if LeftHandSideExpression is either
                    // an ObjectLiteral or an ArrayLiteral
                    // and LeftHandSideExpression cannot
                    // be reparsed as an AssignmentPattern.
                }
                _ => {
                    self.check_assign_target(&expr, true);
                }
            }
        }
        self.reparse_expr_as_pat_inner(pat_ty, expr)
    }

    fn reparse_expr_as_pat_inner(&mut self, pat_ty: PatType, expr: Box<Expr>) -> PResult<Pat> {
        // In dts, we do not reparse.
        debug_assert!(!self.input().syntax().dts());
        let span = expr.span();
        if pat_ty == PatType::AssignPat {
            match *expr {
                Expr::Object(..) | Expr::Array(..) => {
                    // It is a Syntax Error if LeftHandSideExpression is either
                    // an ObjectLiteral or an ArrayLiteral
                    // and LeftHandSideExpression cannot
                    // be reparsed as an AssignmentPattern.
                }

                _ => match *expr {
                    // It is a Syntax Error if the LeftHandSideExpression is
                    // CoverParenthesizedExpressionAndArrowParameterList:(Expression) and
                    // Expression derives a phrase that would produce a Syntax Error according
                    // to these rules if that phrase were substituted for
                    // LeftHandSideExpression. This rule is recursively applied.
                    Expr::Paren(..) => {
                        return Ok(expr.into());
                    }
                    Expr::Ident(i) => return Ok(i.into()),
                    _ => {
                        return Ok(expr.into());
                    }
                },
            }
        }

        // AssignmentElement:
        //      DestructuringAssignmentTarget Initializer[+In]?
        //
        // DestructuringAssignmentTarget:
        //      LeftHandSideExpression
        if pat_ty == PatType::AssignElement {
            match *expr {
                Expr::Array(..) | Expr::Object(..) => {}
                Expr::Member(..)
                | Expr::SuperProp(..)
                | Expr::Call(..)
                | Expr::New(..)
                | Expr::Lit(..)
                | Expr::Ident(..)
                | Expr::Fn(..)
                | Expr::Class(..)
                | Expr::Paren(..)
                | Expr::Tpl(..)
                | Expr::TsAs(..)
                | Expr::TsNonNull(..)
                | Expr::TsTypeAssertion(..)
                | Expr::TsInstantiation(..)
                | Expr::TsSatisfies(..) => {
                    if !expr.is_valid_simple_assignment_target(self.ctx().contains(Context::Strict))
                    {
                        self.emit_err(span, SyntaxError::NotSimpleAssign)
                    }
                    match *expr {
                        Expr::Ident(i) => return Ok(i.into()),
                        _ => {
                            return Ok(expr.into());
                        }
                    }
                }
                // It's special because of optional initializer
                Expr::Assign(..) => {}
                _ => self.emit_err(span, SyntaxError::InvalidPat),
            }
        }

        match *expr {
            Expr::Paren(..) => {
                self.emit_err(span, SyntaxError::InvalidPat);
                Ok(Invalid { span }.into())
            }
            Expr::Assign(
                assign_expr @ AssignExpr {
                    op: AssignOp::Assign,
                    ..
                },
            ) => {
                let AssignExpr {
                    span, left, right, ..
                } = assign_expr;
                Ok(AssignPat {
                    span,
                    left: match left {
                        AssignTarget::Simple(left) => {
                            Box::new(self.reparse_expr_as_pat(pat_ty, left.into())?)
                        }
                        AssignTarget::Pat(pat) => pat.into(),
                        #[cfg(swc_ast_unknown)]
                        _ => unreachable!(),
                    },
                    right,
                }
                .into())
            }
            Expr::Object(ObjectLit {
                span: object_span,
                props,
            }) => {
                // {}
                let len = props.len();
                Ok(ObjectPat {
                    span: object_span,
                    props: props
                        .into_iter()
                        .enumerate()
                        .map(|(idx, prop)| {
                            let span = prop.span();
                            match prop {
                                PropOrSpread::Prop(prop) => match *prop {
                                    Prop::Shorthand(id) => {
                                        Ok(ObjectPatProp::Assign(AssignPatProp {
                                            span: id.span(),
                                            key: id.into(),
                                            value: None,
                                        }))
                                    }
                                    Prop::KeyValue(kv_prop) => {
                                        Ok(ObjectPatProp::KeyValue(KeyValuePatProp {
                                            key: kv_prop.key,
                                            value: Box::new(self.reparse_expr_as_pat(
                                                pat_ty.element(),
                                                kv_prop.value,
                                            )?),
                                        }))
                                    }
                                    Prop::Assign(assign_prop) => {
                                        Ok(ObjectPatProp::Assign(AssignPatProp {
                                            span,
                                            key: assign_prop.key.into(),
                                            value: Some(assign_prop.value),
                                        }))
                                    }
                                    _ => syntax_error!(self, prop.span(), SyntaxError::InvalidPat),
                                },

                                PropOrSpread::Spread(SpreadElement { dot3_token, expr }) => {
                                    if idx != len - 1 {
                                        self.emit_err(span, SyntaxError::NonLastRestParam)
                                    } else if let Some(trailing_comma) =
                                        self.state().trailing_commas.get(&object_span.lo)
                                    {
                                        self.emit_err(
                                            *trailing_comma,
                                            SyntaxError::CommaAfterRestElement,
                                        );
                                    };

                                    let element_pat_ty = pat_ty.element();
                                    let pat = if let PatType::BindingElement = element_pat_ty {
                                        if let Expr::Ident(i) = *expr {
                                            i.into()
                                        } else {
                                            self.emit_err(span, SyntaxError::DotsWithoutIdentifier);
                                            Pat::Invalid(Invalid { span })
                                        }
                                    } else {
                                        self.reparse_expr_as_pat(element_pat_ty, expr)?
                                    };
                                    if let Pat::Assign(_) = pat {
                                        self.emit_err(span, SyntaxError::TS1048)
                                    };
                                    Ok(ObjectPatProp::Rest(RestPat {
                                        span,
                                        dot3_token,
                                        arg: Box::new(pat),
                                        type_ann: None,
                                    }))
                                }
                                #[cfg(swc_ast_unknown)]
                                _ => unreachable!(),
                            }
                        })
                        .collect::<PResult<_>>()?,
                    optional: false,
                    type_ann: None,
                }
                .into())
            }
            Expr::Ident(ident) => Ok(ident.into()),
            Expr::Array(ArrayLit {
                elems: mut exprs, ..
            }) => {
                if exprs.is_empty() {
                    return Ok(ArrayPat {
                        span,
                        elems: Vec::new(),
                        optional: false,
                        type_ann: None,
                    }
                    .into());
                }
                // Trailing comma may exist. We should remove those commas.
                let count_of_trailing_comma =
                    exprs.iter().rev().take_while(|e| e.is_none()).count();
                let len = exprs.len();
                let mut params = Vec::with_capacity(exprs.len() - count_of_trailing_comma);
                // Comma or other pattern cannot follow a rest pattern.
                let idx_of_rest_not_allowed = if count_of_trailing_comma == 0 {
                    len - 1
                } else {
                    // last element is comma, so rest is not allowed for every pattern element.
                    len - count_of_trailing_comma
                };
                for expr in exprs.drain(..idx_of_rest_not_allowed) {
                    match expr {
                        Some(
                            expr @ ExprOrSpread {
                                spread: Some(..), ..
                            },
                        ) => self.emit_err(expr.span(), SyntaxError::NonLastRestParam),
                        Some(ExprOrSpread { expr, .. }) => {
                            params.push(self.reparse_expr_as_pat(pat_ty.element(), expr).map(Some)?)
                        }
                        None => params.push(None),
                    }
                }
                if count_of_trailing_comma == 0 {
                    let expr = exprs.into_iter().next().unwrap();
                    let outer_expr_span = expr.span();
                    let last = match expr {
                        // Rest
                        Some(ExprOrSpread {
                            spread: Some(dot3_token),
                            expr,
                        }) => {
                            // TODO: is BindingPat correct?
                            if let Expr::Assign(_) = *expr {
                                self.emit_err(outer_expr_span, SyntaxError::TS1048);
                            };
                            if let Some(trailing_comma) = self.state().trailing_commas.get(&span.lo)
                            {
                                self.emit_err(*trailing_comma, SyntaxError::CommaAfterRestElement);
                            }
                            let expr_span = expr.span();
                            self.reparse_expr_as_pat(pat_ty.element(), expr)
                                .map(|pat| {
                                    RestPat {
                                        span: expr_span,
                                        dot3_token,
                                        arg: Box::new(pat),
                                        type_ann: None,
                                    }
                                    .into()
                                })
                                .map(Some)?
                        }
                        Some(ExprOrSpread { expr, .. }) => {
                            // TODO: is BindingPat correct?
                            self.reparse_expr_as_pat(pat_ty.element(), expr).map(Some)?
                        }
                        // TODO: syntax error if last element is ellison and ...rest exists.
                        None => None,
                    };
                    params.push(last);
                }
                Ok(ArrayPat {
                    span,
                    elems: params,
                    optional: false,
                    type_ann: None,
                }
                .into())
            }

            // Invalid patterns.
            // Note that assignment expression with '=' is valid, and handled above.
            Expr::Lit(..) | Expr::Assign(..) => {
                self.emit_err(span, SyntaxError::InvalidPat);
                Ok(Invalid { span }.into())
            }

            Expr::Yield(..) if self.ctx().contains(Context::InGenerator) => {
                self.emit_err(span, SyntaxError::InvalidPat);
                Ok(Invalid { span }.into())
            }

            _ => {
                self.emit_err(span, SyntaxError::InvalidPat);

                Ok(Invalid { span }.into())
            }
        }
    }

    pub(super) fn parse_binding_element(&mut self) -> PResult<Pat> {
        trace_cur!(self, parse_binding_element);

        let start = self.cur_pos();
        let left = self.parse_binding_pat_or_ident(false)?;

        if self.input_mut().eat(Token::Eq) {
            let right = self.allow_in_expr(Self::parse_assignment_expr)?;

            if self.ctx().contains(Context::InDeclare) {
                self.emit_err(self.span(start), SyntaxError::TS2371);
            }

            return Ok(AssignPat {
                span: self.span(start),
                left: Box::new(left),
                right,
            }
            .into());
        }

        Ok(left)
    }

    pub(crate) fn parse_binding_pat_or_ident(&mut self, disallow_let: bool) -> PResult<Pat> {
        trace_cur!(self, parse_binding_pat_or_ident);

        let cur = self.input().cur();
        if cur.is_word() {
            self.parse_binding_ident(disallow_let).map(Pat::from)
        } else if cur == Token::LBracket {
            self.parse_array_binding_pat()
        } else if cur == Token::LBrace {
            self.parse_object_pat()
        } else if cur == Token::Error {
            let err = self.input_mut().expect_error_token_and_bump();
            Err(err)
        } else {
            unexpected!(self, "yield, an identifier, [ or {")
        }
    }

    fn parse_array_binding_pat(&mut self) -> PResult<Pat> {
        let start = self.cur_pos();

        self.assert_and_bump(Token::LBracket);

        let mut elems = Vec::new();

        let mut rest_span = Span::default();

        while !self.input().is(Token::RBracket) {
            if self.input_mut().eat(Token::Comma) {
                elems.push(None);
                continue;
            }

            if !rest_span.is_dummy() {
                self.emit_err(rest_span, SyntaxError::NonLastRestParam);
            }

            let start = self.cur_pos();

            let mut is_rest = false;
            if self.input_mut().eat(Token::DotDotDot) {
                is_rest = true;
                let dot3_token = self.span(start);

                let pat = self.parse_binding_pat_or_ident(false)?;
                rest_span = self.span(start);
                let pat = RestPat {
                    span: rest_span,
                    dot3_token,
                    arg: Box::new(pat),
                    type_ann: None,
                }
                .into();
                elems.push(Some(pat));
            } else {
                elems.push(self.parse_binding_element().map(Some)?);
            }

            if !self.input().is(Token::RBracket) {
                expect!(self, Token::Comma);
                if is_rest && self.input().is(Token::RBracket) {
                    self.emit_err(self.input().prev_span(), SyntaxError::CommaAfterRestElement);
                }
            }
        }

        expect!(self, Token::RBracket);
        let optional = (self.input().syntax().dts() || self.ctx().contains(Context::InDeclare))
            && self.input_mut().eat(Token::QuestionMark);

        Ok(ArrayPat {
            span: self.span(start),
            elems,
            optional,
            type_ann: None,
        }
        .into())
    }

    /// spec: 'FormalParameter'
    ///
    /// babel: `parseAssignableListItem`
    fn parse_formal_param_pat(&mut self) -> PResult<Pat> {
        let start = self.cur_pos();

        let has_modifier = self.eat_any_ts_modifier()?;

        let pat_start = self.cur_pos();
        let mut pat = self.parse_binding_element()?;
        let mut opt = false;

        if self.input().syntax().typescript() {
            if self.input_mut().eat(Token::QuestionMark) {
                match pat {
                    Pat::Ident(BindingIdent {
                        id:
                            Ident {
                                ref mut optional, ..
                            },
                        ..
                    })
                    | Pat::Array(ArrayPat {
                        ref mut optional, ..
                    })
                    | Pat::Object(ObjectPat {
                        ref mut optional, ..
                    }) => {
                        *optional = true;
                        opt = true;
                    }
                    _ if self.input().syntax().dts() || self.ctx().contains(Context::InDeclare) => {
                    }
                    _ => {
                        syntax_error!(
                            self,
                            self.input().prev_span(),
                            SyntaxError::TsBindingPatCannotBeOptional
                        );
                    }
                }
            }

            match pat {
                Pat::Array(ArrayPat {
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

                Pat::Ident(BindingIdent {
                    ref mut type_ann, ..
                }) => {
                    let new_type_ann = self.try_parse_ts_type_ann()?;
                    *type_ann = new_type_ann;
                }

                Pat::Assign(AssignPat { ref mut span, .. }) => {
                    if (self.try_parse_ts_type_ann()?).is_some() {
                        *span = Span::new_with_checked(pat_start, self.input().prev_span().hi);
                        self.emit_err(*span, SyntaxError::TSTypeAnnotationAfterAssign);
                    }
                }
                Pat::Invalid(..) => {}
                _ => unreachable!("invalid syntax: Pat: {:?}", pat),
            }
        }

        let pat = if self.input_mut().eat(Token::Eq) {
            // `=` cannot follow optional parameter.
            if opt {
                self.emit_err(pat.span(), SyntaxError::TS1015);
            }

            let right = self.parse_assignment_expr()?;
            if self.ctx().contains(Context::InDeclare) {
                self.emit_err(self.span(start), SyntaxError::TS2371);
            }

            AssignPat {
                span: self.span(start),
                left: Box::new(pat),
                right,
            }
            .into()
        } else {
            pat
        };

        if has_modifier {
            self.emit_err(self.span(start), SyntaxError::TS2369);
            return Ok(pat);
        }

        Ok(pat)
    }

    fn parse_constructor_param(
        &mut self,
        param_start: BytePos,
        decorators: Vec<Decorator>,
    ) -> PResult<ParamOrTsParamProp> {
        let (accessibility, is_override, readonly) = if self.input().syntax().typescript() {
            let accessibility = self.parse_access_modifier()?;
            (
                accessibility,
                self.parse_ts_modifier(&["override"], false)?.is_some(),
                self.parse_ts_modifier(&["readonly"], false)?.is_some(),
            )
        } else {
            (None, false, false)
        };
        if accessibility.is_none() && !is_override && !readonly {
            let pat = self.parse_formal_param_pat()?;
            Ok(ParamOrTsParamProp::Param(Param {
                span: self.span(param_start),
                decorators,
                pat,
            }))
        } else {
            let param = match self.parse_formal_param_pat()? {
                Pat::Ident(i) => TsParamPropParam::Ident(i),
                Pat::Assign(a) => TsParamPropParam::Assign(a),
                node => syntax_error!(self, node.span(), SyntaxError::TsInvalidParamPropPat),
            };
            Ok(ParamOrTsParamProp::TsParamProp(TsParamProp {
                span: self.span(param_start),
                accessibility,
                is_override,
                readonly,
                decorators,
                param,
            }))
        }
    }

    pub(crate) fn parse_constructor_params(&mut self) -> PResult<Vec<ParamOrTsParamProp>> {
        let mut params = Vec::new();
        let mut rest_span = Span::default();

        while !self.input().is(Token::RParen) {
            if !rest_span.is_dummy() {
                self.emit_err(rest_span, SyntaxError::TS1014);
            }

            let param_start = self.cur_pos();
            let decorators = self.parse_decorators(false)?;
            let pat_start = self.cur_pos();

            let mut is_rest = false;
            if self.input_mut().eat(Token::DotDotDot) {
                is_rest = true;
                let dot3_token = self.span(pat_start);

                let pat = self.parse_binding_pat_or_ident(false)?;
                let type_ann =
                    if self.input().syntax().typescript() && self.input().is(Token::Colon) {
                        let cur_pos = self.cur_pos();
                        Some(self.parse_ts_type_ann(/* eat_colon */ true, cur_pos)?)
                    } else {
                        None
                    };

                rest_span = self.span(pat_start);
                let pat = RestPat {
                    span: rest_span,
                    dot3_token,
                    arg: Box::new(pat),
                    type_ann,
                }
                .into();
                params.push(ParamOrTsParamProp::Param(Param {
                    span: self.span(param_start),
                    decorators,
                    pat,
                }));
            } else {
                params.push(self.parse_constructor_param(param_start, decorators)?);
            }

            if !self.input().is(Token::RParen) {
                expect!(self, Token::Comma);
                if self.input().is(Token::RParen) && is_rest {
                    self.emit_err(self.input().prev_span(), SyntaxError::CommaAfterRestElement);
                }
            }
        }

        Ok(params)
    }

    pub(crate) fn parse_formal_params(&mut self) -> PResult<Vec<Param>> {
        let mut params = Vec::new();
        let mut rest_span = Span::default();

        while !self.input().is(Token::RParen) {
            if !rest_span.is_dummy() {
                self.emit_err(rest_span, SyntaxError::TS1014);
            }

            let param_start = self.cur_pos();
            let decorators = self.parse_decorators(false)?;
            let pat_start = self.cur_pos();

            let pat = if self.input_mut().eat(Token::DotDotDot) {
                let dot3_token = self.span(pat_start);

                let mut pat = self.parse_binding_pat_or_ident(false)?;

                if self.input_mut().eat(Token::Eq) {
                    let right = self.parse_assignment_expr()?;
                    self.emit_err(pat.span(), SyntaxError::TS1048);
                    pat = AssignPat {
                        span: self.span(pat_start),
                        left: Box::new(pat),
                        right,
                    }
                    .into();
                }

                let type_ann =
                    if self.input().syntax().typescript() && self.input().is(Token::Colon) {
                        let cur_pos = self.cur_pos();
                        let ty = self.parse_ts_type_ann(/* eat_colon */ true, cur_pos)?;
                        Some(ty)
                    } else {
                        None
                    };

                rest_span = self.span(pat_start);
                let pat = RestPat {
                    span: rest_span,
                    dot3_token,
                    arg: Box::new(pat),
                    type_ann,
                }
                .into();

                if self.syntax().typescript() && self.input_mut().eat(Token::QuestionMark) {
                    self.emit_err(self.input().prev_span(), SyntaxError::TS1047);
                    //
                }

                pat
            } else {
                self.parse_formal_param_pat()?
            };
            let is_rest = matches!(pat, Pat::Rest(_));

            params.push(Param {
                span: self.span(param_start),
                decorators,
                pat,
            });

            if !self.input().is(Token::RParen) {
                expect!(self, Token::Comma);
                if is_rest && self.input().is(Token::RParen) {
                    self.emit_err(self.input().prev_span(), SyntaxError::CommaAfterRestElement);
                }
            }
        }

        Ok(params)
    }

    pub(crate) fn parse_unique_formal_params(&mut self) -> PResult<Vec<Param>> {
        // FIXME: This is wrong
        self.parse_formal_params()
    }

    pub(super) fn parse_paren_items_as_params(
        &mut self,
        mut exprs: Vec<AssignTargetOrSpread>,
        trailing_comma: Option<Span>,
    ) -> PResult<Vec<Pat>> {
        let pat_ty = PatType::BindingPat;

        let len = exprs.len();
        if len == 0 {
            return Ok(Vec::new());
        }

        let mut params = Vec::with_capacity(len);

        for expr in exprs.drain(..len - 1) {
            match expr {
                AssignTargetOrSpread::ExprOrSpread(ExprOrSpread {
                    spread: Some(..), ..
                })
                | AssignTargetOrSpread::Pat(Pat::Rest(..)) => {
                    self.emit_err(expr.span(), SyntaxError::TS1014)
                }
                AssignTargetOrSpread::ExprOrSpread(ExprOrSpread {
                    spread: None, expr, ..
                }) => params.push(self.reparse_expr_as_pat(pat_ty, expr)?),
                AssignTargetOrSpread::Pat(pat) => params.push(pat),
            }
        }

        debug_assert_eq!(exprs.len(), 1);
        let expr = exprs.pop().unwrap();
        let outer_expr_span = expr.span();
        let last = match expr {
            // Rest
            AssignTargetOrSpread::ExprOrSpread(ExprOrSpread {
                spread: Some(dot3_token),
                expr,
            }) => {
                if let Expr::Assign(_) = *expr {
                    self.emit_err(outer_expr_span, SyntaxError::TS1048)
                };
                if let Some(trailing_comma) = trailing_comma {
                    self.emit_err(trailing_comma, SyntaxError::CommaAfterRestElement);
                }
                let expr_span = expr.span();
                self.reparse_expr_as_pat(pat_ty, expr).map(|pat| {
                    RestPat {
                        span: expr_span,
                        dot3_token,
                        arg: Box::new(pat),
                        type_ann: None,
                    }
                    .into()
                })?
            }
            AssignTargetOrSpread::ExprOrSpread(ExprOrSpread { expr, .. }) => {
                self.reparse_expr_as_pat(pat_ty, expr)?
            }
            AssignTargetOrSpread::Pat(pat) => {
                if let Some(trailing_comma) = trailing_comma {
                    if let Pat::Rest(..) = pat {
                        self.emit_err(trailing_comma, SyntaxError::CommaAfterRestElement);
                    }
                }
                pat
            }
        };
        params.push(last);

        if self.ctx().contains(Context::Strict) {
            for param in params.iter() {
                self.pat_is_valid_argument_in_strict(param)
            }
        }
        Ok(params)
    }
}

#[cfg(test)]
mod tests {
    use swc_atoms::atom;
    use swc_common::DUMMY_SP as span;
    use swc_ecma_visit::assert_eq_ignore_span;

    use super::*;

    fn array_pat(s: &'static str) -> Pat {
        test_parser(s, Syntax::default(), |p| p.parse_array_binding_pat())
    }

    fn object_pat(s: &'static str) -> Pat {
        test_parser(s, Syntax::default(), |p| {
            p.parse_binding_pat_or_ident(false)
        })
    }

    fn ident(s: &str) -> Ident {
        Ident::new_no_ctxt(s.into(), span)
    }

    fn ident_name(s: &str) -> IdentName {
        IdentName::new(s.into(), span)
    }

    fn rest() -> Option<Pat> {
        Some(
            RestPat {
                span,
                dot3_token: span,
                type_ann: None,
                arg: ident("tail").into(),
            }
            .into(),
        )
    }

    #[test]
    fn array_pat_simple() {
        assert_eq_ignore_span!(
            array_pat("[a, [b], [c]]"),
            Pat::Array(ArrayPat {
                span,
                optional: false,
                elems: vec![
                    Some(Pat::Ident(ident("a").into())),
                    Some(Pat::Array(ArrayPat {
                        span,
                        optional: false,
                        elems: vec![Some(Pat::Ident(ident("b").into()))],
                        type_ann: None
                    })),
                    Some(Pat::Array(ArrayPat {
                        span,
                        optional: false,
                        elems: vec![Some(Pat::Ident(ident("c").into()))],
                        type_ann: None
                    }))
                ],
                type_ann: None
            })
        );
    }

    #[test]
    fn array_pat_empty_start() {
        assert_eq_ignore_span!(
            array_pat("[, a, [b], [c]]"),
            Pat::Array(ArrayPat {
                span,
                optional: false,
                elems: vec![
                    None,
                    Some(Pat::Ident(ident("a").into())),
                    Some(Pat::Array(ArrayPat {
                        span,
                        optional: false,
                        elems: vec![Some(Pat::Ident(ident("b").into()))],
                        type_ann: None
                    })),
                    Some(Pat::Array(ArrayPat {
                        span,
                        optional: false,
                        elems: vec![Some(Pat::Ident(ident("c").into()))],
                        type_ann: None
                    }))
                ],
                type_ann: None
            })
        );
    }

    #[test]
    fn array_pat_empty() {
        assert_eq_ignore_span!(
            array_pat("[a, , [b], [c]]"),
            Pat::Array(ArrayPat {
                span,
                optional: false,
                elems: vec![
                    Some(Pat::Ident(ident("a").into())),
                    None,
                    Some(Pat::Array(ArrayPat {
                        span,
                        optional: false,
                        elems: vec![Some(Pat::Ident(ident("b").into()))],
                        type_ann: None
                    })),
                    Some(Pat::Array(ArrayPat {
                        span,
                        optional: false,
                        elems: vec![Some(Pat::Ident(ident("c").into()))],
                        type_ann: None
                    }))
                ],
                type_ann: None
            })
        );
    }

    #[test]
    fn array_pat_empty_end() {
        assert_eq_ignore_span!(
            array_pat("[a, ,]"),
            Pat::Array(ArrayPat {
                span,
                optional: false,
                elems: vec![Some(Pat::Ident(ident("a").into())), None,],
                type_ann: None
            })
        );
    }

    #[test]
    fn array_binding_pattern_tail() {
        assert_eq_ignore_span!(
            array_pat("[...tail]"),
            Pat::Array(ArrayPat {
                span,
                optional: false,
                elems: vec![rest()],
                type_ann: None
            })
        );
    }

    #[test]
    fn array_binding_pattern_assign() {
        assert_eq_ignore_span!(
            array_pat("[,a=1,]"),
            Pat::Array(ArrayPat {
                span,
                optional: false,
                elems: vec![
                    None,
                    Some(Pat::Assign(AssignPat {
                        span,
                        left: Box::new(Pat::Ident(ident("a").into())),
                        right: Box::new(Expr::Lit(Lit::Num(Number {
                            span,
                            value: 1.0,
                            raw: Some(atom!("1"))
                        })))
                    }))
                ],
                type_ann: None
            })
        );
    }

    #[test]
    fn array_binding_pattern_tail_with_elems() {
        assert_eq_ignore_span!(
            array_pat("[,,,...tail]"),
            Pat::Array(ArrayPat {
                span,
                optional: false,
                elems: vec![None, None, None, rest()],
                type_ann: None
            })
        );
    }

    #[test]
    fn array_binding_pattern_tail_inside_tail() {
        assert_eq_ignore_span!(
            array_pat("[,,,...[...tail]]"),
            Pat::Array(ArrayPat {
                span,
                optional: false,
                elems: vec![
                    None,
                    None,
                    None,
                    Some(Pat::Rest(RestPat {
                        span,
                        dot3_token: span,
                        type_ann: None,
                        arg: Box::new(Pat::Array(ArrayPat {
                            span,
                            optional: false,
                            elems: vec![rest()],
                            type_ann: None
                        }))
                    }))
                ],
                type_ann: None
            })
        );
    }

    #[test]
    fn object_binding_pattern_tail() {
        assert_eq_ignore_span!(
            object_pat("{...obj}"),
            Pat::Object(ObjectPat {
                span,
                type_ann: None,
                optional: false,
                props: vec![ObjectPatProp::Rest(RestPat {
                    span,
                    dot3_token: span,
                    type_ann: None,
                    arg: Box::new(Pat::Ident(ident("obj").into()))
                })]
            })
        );
    }

    #[test]
    fn object_binding_pattern_with_prop() {
        assert_eq_ignore_span!(
            object_pat("{prop = 10 }"),
            Pat::Object(ObjectPat {
                span,
                type_ann: None,
                optional: false,
                props: vec![ObjectPatProp::Assign(AssignPatProp {
                    span,
                    key: ident("prop").into(),
                    value: Some(Box::new(Expr::Lit(Lit::Num(Number {
                        span,
                        value: 10.0,
                        raw: Some(atom!("10"))
                    }))))
                })]
            })
        );
    }

    #[test]
    fn object_binding_pattern_with_prop_and_label() {
        fn prop(key: PropName, assign_name: &str, expr: Expr) -> PropOrSpread {
            PropOrSpread::Prop(Box::new(Prop::KeyValue(KeyValueProp {
                key,
                value: AssignExpr {
                    span,
                    op: AssignOp::Assign,
                    left: ident(assign_name).into(),
                    right: Box::new(expr),
                }
                .into(),
            })))
        }

        assert_eq_ignore_span!(
            object_pat(
                "{obj = {$: num = 10, '': sym = '', \" \": quote = \" \", _: under = [...tail],}}"
            ),
            Pat::Object(ObjectPat {
                span,
                type_ann: None,
                optional: false,
                props: vec![ObjectPatProp::Assign(AssignPatProp {
                    span,
                    key: ident("obj").into(),
                    value: Some(Box::new(Expr::Object(ObjectLit {
                        span,
                        props: vec![
                            prop(
                                PropName::Ident(ident_name("$")),
                                "num",
                                Expr::Lit(Lit::Num(Number {
                                    span,
                                    value: 10.0,
                                    raw: Some(atom!("10"))
                                }))
                            ),
                            prop(
                                PropName::Str(Str {
                                    span,
                                    value: atom!("").into(),
                                    raw: Some(atom!("''")),
                                }),
                                "sym",
                                Expr::Lit(Lit::Str(Str {
                                    span,
                                    value: atom!("").into(),
                                    raw: Some(atom!("''")),
                                }))
                            ),
                            prop(
                                PropName::Str(Str {
                                    span,
                                    value: atom!(" ").into(),
                                    raw: Some(atom!("\" \"")),
                                }),
                                "quote",
                                Expr::Lit(Lit::Str(Str {
                                    span,
                                    value: atom!(" ").into(),
                                    raw: Some(atom!("\" \"")),
                                }))
                            ),
                            prop(
                                PropName::Ident(ident_name("_")),
                                "under",
                                Expr::Array(ArrayLit {
                                    span,
                                    elems: vec![Some(ExprOrSpread {
                                        spread: Some(span),
                                        expr: Box::new(Expr::Ident(ident("tail")))
                                    })]
                                })
                            ),
                        ]
                    })))
                })]
            })
        );
    }
}
