use swc_common::{Span, Spanned, DUMMY_SP};
use swc_ecma_ast::*;

use crate::{
    error::SyntaxError, input::Tokens, lexer::Token, parser::class_and_fn::is_not_this, Context,
    PResult, Parser,
};

impl<I: Tokens> Parser<I> {
    pub(crate) fn parse_object<Object, ObjectProp>(
        &mut self,
        parse_prop: impl Fn(&mut Self) -> PResult<ObjectProp>,
        make_object: impl Fn(&mut Self, Span, Vec<ObjectProp>, Option<Span>) -> PResult<Object>,
    ) -> PResult<Object> {
        self.do_outside_of_context(Context::WillExpectColonForCond, |p| {
            trace_cur!(p, parse_object);

            let start = p.cur_pos();
            let mut trailing_comma = None;
            p.assert_and_bump(Token::LBrace);

            let mut props = Vec::with_capacity(8);

            while !p.input_mut().eat(Token::RBrace) {
                props.push(parse_prop(p)?);

                if !p.input().is(Token::RBrace) {
                    expect!(p, Token::Comma);
                    if p.input().is(Token::RBrace) {
                        trailing_comma = Some(p.input().prev_span());
                    }
                }
            }

            let span = p.span(start);
            make_object(p, span, props, trailing_comma)
        })
    }

    /// Production 'BindingProperty'
    pub(crate) fn parse_binding_object_prop(&mut self) -> PResult<ObjectPatProp> {
        let start = self.cur_pos();

        if self.input_mut().eat(Token::DotDotDot) {
            // spread element
            let dot3_token = self.span(start);

            let arg = Box::new(self.parse_binding_pat_or_ident(false)?);

            return Ok(ObjectPatProp::Rest(RestPat {
                span: self.span(start),
                dot3_token,
                arg,
                type_ann: None,
            }));
        }

        let key = self.parse_prop_name()?;
        if self.input_mut().eat(Token::Colon) {
            let value = Box::new(self.parse_binding_element()?);

            return Ok(ObjectPatProp::KeyValue(KeyValuePatProp { key, value }));
        }
        let key = match key {
            PropName::Ident(ident) => ident,
            _ => unexpected!(self, "an identifier"),
        };

        let value = if self.input_mut().eat(Token::Eq) {
            self.allow_in_expr(Self::parse_assignment_expr).map(Some)?
        } else {
            if self.ctx().is_reserved_word(&key.sym) {
                self.emit_err(key.span, SyntaxError::ReservedWordInObjShorthandOrPat);
            }

            None
        };

        Ok(ObjectPatProp::Assign(AssignPatProp {
            span: self.span(start),
            key: key.into(),
            value,
        }))
    }

    fn make_binding_object(
        &mut self,
        span: Span,
        props: Vec<ObjectPatProp>,
        trailing_comma: Option<Span>,
    ) -> PResult<Pat> {
        let len = props.len();
        for (i, prop) in props.iter().enumerate() {
            if i == len - 1 {
                if let ObjectPatProp::Rest(ref rest) = prop {
                    match *rest.arg {
                        Pat::Ident(..) => {
                            if let Some(trailing_comma) = trailing_comma {
                                self.emit_err(trailing_comma, SyntaxError::CommaAfterRestElement);
                            }
                        }
                        _ => syntax_error!(self, prop.span(), SyntaxError::DotsWithoutIdentifier),
                    }
                }
                continue;
            }

            if let ObjectPatProp::Rest(..) = prop {
                self.emit_err(prop.span(), SyntaxError::NonLastRestParam)
            }
        }

        let optional = (self.input().syntax().dts() || self.ctx().contains(Context::InDeclare))
            && self.input_mut().eat(Token::QuestionMark);

        Ok(ObjectPat {
            span,
            props,
            optional,
            type_ann: None,
        }
        .into())
    }

    pub(super) fn parse_object_pat(&mut self) -> PResult<Pat> {
        self.parse_object(Self::parse_binding_object_prop, Self::make_binding_object)
    }

    fn make_expr_object(
        &mut self,
        span: Span,
        props: Vec<PropOrSpread>,
        trailing_comma: Option<Span>,
    ) -> PResult<Expr> {
        if let Some(trailing_comma) = trailing_comma {
            self.state_mut()
                .trailing_commas
                .insert(span.lo, trailing_comma);
        }
        Ok(ObjectLit { span, props }.into())
    }

    fn parse_expr_object_prop(&mut self) -> PResult<PropOrSpread> {
        trace_cur!(self, parse_object_prop);

        let start = self.cur_pos();
        // Parse as 'MethodDefinition'

        if self.input_mut().eat(Token::DotDotDot) {
            // spread element
            let dot3_token = self.span(start);

            let expr = self.allow_in_expr(Self::parse_assignment_expr)?;

            return Ok(PropOrSpread::Spread(SpreadElement { dot3_token, expr }));
        }

        if self.input_mut().eat(Token::Asterisk) {
            let name = self.parse_prop_name()?;
            return self
                .do_inside_of_context(Context::AllowDirectSuper, |p| {
                    p.do_outside_of_context(Context::InClassField, |p| {
                        p.parse_fn_args_body(
                            // no decorator in an object literal
                            Vec::new(),
                            start,
                            Self::parse_unique_formal_params,
                            false,
                            true,
                        )
                    })
                })
                .map(|function| {
                    PropOrSpread::Prop(Box::new(Prop::Method(MethodProp {
                        key: name,
                        function,
                    })))
                });
        }

        let has_modifiers = self.eat_any_ts_modifier()?;
        let modifiers_span = self.input().prev_span();

        let key = self.parse_prop_name()?;

        let cur = self.input().cur();
        if self.input().syntax().typescript()
            && !(matches!(
                cur,
                Token::LParen
                    | Token::LBracket
                    | Token::Colon
                    | Token::Comma
                    | Token::QuestionMark
                    | Token::Eq
                    | Token::Asterisk
            ) || cur == Token::Str
                || cur == Token::Num
                || cur.is_word())
            && !(self.input().syntax().typescript() && self.input().is(Token::Lt))
            && !(self.input().is(Token::RBrace) && matches!(key, PropName::Ident(..)))
        {
            trace_cur!(self, parse_object_prop_error);

            self.emit_err(self.input().cur_span(), SyntaxError::TS1005);
            return Ok(PropOrSpread::Prop(Box::new(Prop::KeyValue(KeyValueProp {
                key,
                value: Invalid {
                    span: self.span(start),
                }
                .into(),
            }))));
        }
        //
        // {[computed()]: a,}
        // { 'a': a, }
        // { 0: 1, }
        // { a: expr, }
        if self.input_mut().eat(Token::Colon) {
            let value = self.allow_in_expr(Self::parse_assignment_expr)?;
            return Ok(PropOrSpread::Prop(Box::new(Prop::KeyValue(KeyValueProp {
                key,
                value,
            }))));
        }

        // Handle `a(){}` (and async(){} / get(){} / set(){})
        if (self.input().syntax().typescript() && self.input().is(Token::Lt))
            || self.input().is(Token::LParen)
        {
            return self
                .do_inside_of_context(Context::AllowDirectSuper, |p| {
                    p.do_outside_of_context(Context::InClassField, |p| {
                        p.parse_fn_args_body(
                            // no decorator in an object literal
                            Vec::new(),
                            start,
                            Self::parse_unique_formal_params,
                            false,
                            false,
                        )
                    })
                })
                .map(|function| Box::new(Prop::Method(MethodProp { key, function })))
                .map(PropOrSpread::Prop);
        }

        let ident = match key {
            PropName::Ident(ident) => ident,
            // TODO
            _ => unexpected!(self, "identifier"),
        };

        if self.input_mut().eat(Token::QuestionMark) {
            self.emit_err(self.input().prev_span(), SyntaxError::TS1162);
        }

        // `ident` from parse_prop_name is parsed as 'IdentifierName'
        // It means we should check for invalid expressions like { for, }
        let cur = self.input().cur();
        if matches!(cur, Token::Eq | Token::Comma | Token::RBrace) {
            if self.ctx().is_reserved_word(&ident.sym) {
                self.emit_err(ident.span, SyntaxError::ReservedWordInObjShorthandOrPat);
            }

            if self.input_mut().eat(Token::Eq) {
                let value = self.allow_in_expr(Self::parse_assignment_expr)?;
                let span = self.span(start);
                return Ok(PropOrSpread::Prop(Box::new(Prop::Assign(AssignProp {
                    span,
                    key: ident.into(),
                    value,
                }))));
            }

            return Ok(PropOrSpread::Prop(Box::new(Prop::from(ident))));
        }

        // get a(){}
        // set a(v){}
        // async a(){}

        match &*ident.sym {
            "get" | "set" | "async" => {
                trace_cur!(self, parse_object_prop__after_accessor);

                if has_modifiers {
                    self.emit_err(modifiers_span, SyntaxError::TS1042);
                }

                let is_generator = ident.sym == "async" && self.input_mut().eat(Token::Asterisk);
                let key = self.parse_prop_name()?;
                let key_span = key.span();
                self.do_inside_of_context(Context::AllowDirectSuper, |p| {
                    p.do_outside_of_context(Context::InClassField, |p| {
                        match &*ident.sym {
                            "get" => p
                                .parse_fn_args_body(
                                    // no decorator in an object literal
                                    Vec::new(),
                                    start,
                                    |p| {
                                        let params = p.parse_formal_params()?;

                                        if params.iter().any(is_not_this) {
                                            p.emit_err(key_span, SyntaxError::GetterParam);
                                        }

                                        Ok(params)
                                    },
                                    false,
                                    false,
                                )
                                .map(|v| *v)
                                .map(
                                    |Function {
                                         body, return_type, ..
                                     }| {
                                        if p.input().syntax().typescript()
                                            && p.input().target() == EsVersion::Es3
                                        {
                                            p.emit_err(key_span, SyntaxError::TS1056);
                                        }

                                        PropOrSpread::Prop(Box::new(Prop::Getter(GetterProp {
                                            span: p.span(start),
                                            key,
                                            type_ann: return_type,
                                            body,
                                        })))
                                    },
                                ),
                            "set" => {
                                p.parse_fn_args_body(
                                    // no decorator in an object literal
                                    Vec::new(),
                                    start,
                                    |p| {
                                        let params = p.parse_formal_params()?;

                                        if params.iter().filter(|p| is_not_this(p)).count() != 1 {
                                            p.emit_err(key_span, SyntaxError::SetterParam);
                                        }

                                        if !params.is_empty() {
                                            if let Pat::Rest(..) = params[0].pat {
                                                p.emit_err(
                                                    params[0].span(),
                                                    SyntaxError::RestPatInSetter,
                                                );
                                            }
                                        }

                                        if p.input().syntax().typescript()
                                            && p.input().target() == EsVersion::Es3
                                        {
                                            p.emit_err(key_span, SyntaxError::TS1056);
                                        }

                                        Ok(params)
                                    },
                                    false,
                                    false,
                                )
                                .map(|v| *v)
                                .map(
                                    |Function {
                                         mut params, body, ..
                                     }| {
                                        let mut this = None;
                                        if params.len() >= 2 {
                                            this = Some(params.remove(0).pat);
                                        }

                                        let param = Box::new(
                                            params
                                                .into_iter()
                                                .next()
                                                .map(|v| v.pat)
                                                .unwrap_or_else(|| {
                                                    p.emit_err(key_span, SyntaxError::SetterParam);

                                                    Invalid { span: DUMMY_SP }.into()
                                                }),
                                        );

                                        // debug_assert_eq!(params.len(), 1);
                                        PropOrSpread::Prop(Box::new(Prop::Setter(SetterProp {
                                            span: p.span(start),
                                            key,
                                            body,
                                            param,
                                            this_param: this,
                                        })))
                                    },
                                )
                            }
                            "async" => p
                                .parse_fn_args_body(
                                    // no decorator in an object literal
                                    Vec::new(),
                                    start,
                                    Self::parse_unique_formal_params,
                                    true,
                                    is_generator,
                                )
                                .map(|function| {
                                    PropOrSpread::Prop(Box::new(Prop::Method(MethodProp {
                                        key,
                                        function,
                                    })))
                                }),
                            _ => unreachable!(),
                        }
                    })
                })
            }
            _ => {
                if self.input().syntax().typescript() {
                    unexpected!(
                        self,
                        "... , *,  (, [, :, , ?, =, an identifier, public, protected, private, \
                         readonly, <."
                    )
                } else {
                    unexpected!(self, "... , *,  (, [, :, , ?, = or an identifier")
                }
            }
        }
    }

    pub(crate) fn parse_object_expr(&mut self) -> PResult<Expr> {
        self.parse_object(Self::parse_expr_object_prop, Self::make_expr_object)
    }
}
