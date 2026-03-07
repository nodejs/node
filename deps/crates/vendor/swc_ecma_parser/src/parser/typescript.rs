use std::{fmt::Write, mem};

use either::Either;
use swc_atoms::{atom, Atom, Wtf8Atom};
use swc_common::{BytePos, Span, Spanned};
use swc_ecma_ast::*;

use crate::{
    error::SyntaxError, input::Tokens, lexer::Token, parser::util::IsSimpleParameterList, Context,
    PResult, Parser,
};

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
enum ParsingContext {
    EnumMembers,
    HeritageClauseElement,
    TupleElementTypes,
    TypeMembers,
    TypeParametersOrArguments,
}

#[derive(Clone, Copy, PartialEq, Eq)]
enum UnionOrIntersection {
    Union,
    Intersection,
}

#[derive(Clone, Copy, PartialEq, Eq)]
enum SignatureParsingMode {
    TSCallSignatureDeclaration,
    TSConstructSignatureDeclaration,
}

/// Mark as declare
fn make_decl_declare(mut decl: Decl) -> Decl {
    match decl {
        Decl::Class(ref mut c) => c.declare = true,
        Decl::Fn(ref mut f) => f.declare = true,
        Decl::Var(ref mut v) => v.declare = true,
        Decl::TsInterface(ref mut i) => i.declare = true,
        Decl::TsTypeAlias(ref mut a) => a.declare = true,
        Decl::TsEnum(ref mut e) => e.declare = true,
        Decl::TsModule(ref mut m) => m.declare = true,
        Decl::Using(..) => unreachable!("Using is not a valid declaration for `declare` keyword"),
        #[cfg(swc_ast_unknown)]
        _ => unreachable!(),
    }

    decl
}

impl<I: Tokens> Parser<I> {
    /// `tsParseList`
    fn parse_ts_list<T, F>(&mut self, kind: ParsingContext, mut parse_element: F) -> PResult<Vec<T>>
    where
        F: FnMut(&mut Self) -> PResult<T>,
    {
        debug_assert!(self.input().syntax().typescript());
        let mut buf = Vec::with_capacity(8);
        while !self.is_ts_list_terminator(kind) {
            // Skipping "parseListElement" from the TS source since that's just for error
            // handling.
            buf.push(parse_element(self)?);
        }
        Ok(buf)
    }

    /// `tsTryParse`
    pub(super) fn try_parse_ts_bool<F>(&mut self, op: F) -> PResult<bool>
    where
        F: FnOnce(&mut Self) -> PResult<Option<bool>>,
    {
        if !self.input().syntax().typescript() {
            return Ok(false);
        }

        let prev_ignore_error = self.input().get_ctx().contains(Context::IgnoreError);
        let checkpoint = self.checkpoint_save();
        self.set_ctx(self.ctx() | Context::IgnoreError);
        let res = op(self);
        match res {
            Ok(Some(res)) if res => {
                let mut ctx = self.ctx();
                ctx.set(Context::IgnoreError, prev_ignore_error);
                self.input_mut().set_ctx(ctx);
                Ok(res)
            }
            _ => {
                self.checkpoint_load(checkpoint);
                Ok(false)
            }
        }
    }

    /// `tsParseDelimitedList`
    fn parse_ts_delimited_list_inner<T, F>(
        &mut self,
        kind: ParsingContext,
        mut parse_element: F,
    ) -> PResult<Vec<T>>
    where
        F: FnMut(&mut Self) -> PResult<(BytePos, T)>,
    {
        debug_assert!(self.input().syntax().typescript());
        let mut buf = Vec::new();
        loop {
            trace_cur!(self, parse_ts_delimited_list_inner__element);

            if self.is_ts_list_terminator(kind) {
                break;
            }

            let (_, element) = parse_element(self)?;
            buf.push(element);

            if self.input_mut().eat(Token::Comma) {
                continue;
            }

            if self.is_ts_list_terminator(kind) {
                break;
            }

            if kind == ParsingContext::EnumMembers {
                let expect = Token::Comma;
                let cur = self.input().cur();
                let cur = cur.to_string();
                self.emit_err(
                    self.input().cur_span(),
                    SyntaxError::Expected(format!("{expect:?}"), cur),
                );
                continue;
            }
            // This will fail with an error about a missing comma
            expect!(self, Token::Comma);
        }

        Ok(buf)
    }

    /// In no lexer context
    pub(crate) fn ts_in_no_context<T, F>(&mut self, op: F) -> PResult<T>
    where
        F: FnOnce(&mut Self) -> PResult<T>,
    {
        debug_assert!(self.input().syntax().typescript());
        trace_cur!(self, ts_in_no_context__before);
        let res = op(self);
        trace_cur!(self, ts_in_no_context__after);
        res
    }

    /// `tsIsListTerminator`
    fn is_ts_list_terminator(&mut self, kind: ParsingContext) -> bool {
        debug_assert!(self.input().syntax().typescript());
        let cur = self.input().cur();
        match kind {
            ParsingContext::EnumMembers | ParsingContext::TypeMembers => cur == Token::RBrace,
            ParsingContext::HeritageClauseElement => {
                matches!(cur, Token::LBrace | Token::Implements | Token::Extends)
            }
            ParsingContext::TupleElementTypes => cur == Token::RBracket,
            ParsingContext::TypeParametersOrArguments => cur == Token::Gt,
        }
    }

    /// `tsNextTokenCanFollowModifier`
    pub(super) fn ts_next_token_can_follow_modifier(&mut self) -> bool {
        debug_assert!(self.input().syntax().typescript());
        // Note: TypeScript's implementation is much more complicated because
        // more things are considered modifiers there.
        // This implementation only handles modifiers not handled by @babel/parser
        // itself. And "static". TODO: Would be nice to avoid lookahead. Want a
        // hasLineBreakUpNext() method...
        self.bump();

        let cur = self.input().cur();
        !self.input().had_line_break_before_cur()
            && matches!(
                cur,
                Token::LBracket
                    | Token::LBrace
                    | Token::Asterisk
                    | Token::DotDotDot
                    | Token::Hash
                    | Token::Str
                    | Token::Num
                    | Token::BigInt
            )
            || cur.is_word()
    }

    /// `tsTryParse`
    pub(crate) fn try_parse_ts<T, F>(&mut self, op: F) -> Option<T>
    where
        F: FnOnce(&mut Self) -> PResult<Option<T>>,
    {
        if !self.input().syntax().typescript() {
            return None;
        }
        debug_tracing!(self, "try_parse_ts");

        trace_cur!(self, try_parse_ts);

        let prev_ignore_error = self.input().get_ctx().contains(Context::IgnoreError);
        let checkpoint = self.checkpoint_save();
        self.set_ctx(self.ctx() | Context::IgnoreError);
        let res = op(self);
        match res {
            Ok(Some(res)) => {
                trace_cur!(self, try_parse_ts__success_value);
                let mut ctx = self.ctx();
                ctx.set(Context::IgnoreError, prev_ignore_error);
                self.input_mut().set_ctx(ctx);
                Some(res)
            }
            Ok(None) => {
                trace_cur!(self, try_parse_ts__success_no_value);
                self.checkpoint_load(checkpoint);
                None
            }
            Err(..) => {
                trace_cur!(self, try_parse_ts__fail);
                self.checkpoint_load(checkpoint);
                None
            }
        }
    }

    /// `tsParseTypeMemberSemicolon`
    fn parse_ts_type_member_semicolon(&mut self) -> PResult<()> {
        debug_assert!(self.input().syntax().typescript());

        if !self.input_mut().eat(Token::Comma) {
            self.expect_general_semi()
        } else {
            Ok(())
        }
    }

    /// `tsIsStartOfConstructSignature`
    fn is_ts_start_of_construct_signature(&mut self) -> bool {
        debug_assert!(self.input().syntax().typescript());

        self.bump();
        let cur = self.input().cur();
        matches!(cur, Token::LParen | Token::Lt)
    }

    /// `tsParseDelimitedList`
    fn parse_ts_delimited_list<T, F>(
        &mut self,
        kind: ParsingContext,
        mut parse_element: F,
    ) -> PResult<Vec<T>>
    where
        F: FnMut(&mut Self) -> PResult<T>,
    {
        self.parse_ts_delimited_list_inner(kind, |p| {
            let start = p.input().cur_pos();
            Ok((start, parse_element(p)?))
        })
    }

    /// `tsParseUnionOrIntersectionType`
    fn parse_ts_union_or_intersection_type<F>(
        &mut self,
        kind: UnionOrIntersection,
        mut parse_constituent_type: F,
        operator: Token,
    ) -> PResult<Box<TsType>>
    where
        F: FnMut(&mut Self) -> PResult<Box<TsType>>,
    {
        trace_cur!(self, parse_ts_union_or_intersection_type);

        debug_assert!(self.input().syntax().typescript());

        let start = self.input().cur_pos(); // include the leading operator in the start
        self.input_mut().eat(operator);
        trace_cur!(self, parse_ts_union_or_intersection_type__first_type);

        let ty = parse_constituent_type(self)?;
        trace_cur!(self, parse_ts_union_or_intersection_type__after_first);

        if self.input().is(operator) {
            let mut types = vec![ty];

            while self.input_mut().eat(operator) {
                trace_cur!(self, parse_ts_union_or_intersection_type__constituent);

                types.push(parse_constituent_type(self)?);
            }

            return Ok(Box::new(TsType::TsUnionOrIntersectionType(match kind {
                UnionOrIntersection::Union => TsUnionOrIntersectionType::TsUnionType(TsUnionType {
                    span: self.span(start),
                    types,
                }),
                UnionOrIntersection::Intersection => {
                    TsUnionOrIntersectionType::TsIntersectionType(TsIntersectionType {
                        span: self.span(start),
                        types,
                    })
                }
            })));
        }
        Ok(ty)
    }

    pub(crate) fn eat_any_ts_modifier(&mut self) -> PResult<bool> {
        if self.syntax().typescript()
            && {
                let cur = self.input().cur();
                matches!(
                    cur,
                    Token::Public | Token::Protected | Token::Private | Token::Readonly
                )
            }
            && peek!(self)
                .is_some_and(|t| t.is_word() || matches!(t, Token::LBrace | Token::LBracket))
        {
            let _ = self.parse_ts_modifier(&["public", "protected", "private", "readonly"], false);
            Ok(true)
        } else {
            Ok(false)
        }
    }

    /// Parses a modifier matching one the given modifier names.
    ///
    /// `tsParseModifier`
    pub(crate) fn parse_ts_modifier(
        &mut self,
        allowed_modifiers: &[&'static str],
        stop_on_start_of_class_static_blocks: bool,
    ) -> PResult<Option<&'static str>> {
        debug_assert!(self.input().syntax().typescript());
        let pos = {
            let cur = self.input().cur();
            let modifier = if cur == Token::Ident {
                cur.clone().take_unknown_ident_ref(self.input()).clone()
            } else if cur.is_known_ident() {
                cur.take_known_ident(&self.input)
            } else if cur == Token::In {
                atom!("in")
            } else if cur == Token::Const {
                atom!("const")
            } else if cur == Token::Error {
                let err = self.input_mut().expect_error_token_and_bump();
                return Err(err);
            } else if cur == Token::Eof {
                return Err(self.eof_error());
            } else {
                return Ok(None);
            };
            // TODO: compare atom rather than string.
            allowed_modifiers
                .iter()
                .position(|s| **s == *modifier.as_str())
        };
        if let Some(pos) = pos {
            if stop_on_start_of_class_static_blocks
                && self.input().is(Token::Static)
                && peek!(self).is_some_and(|peek| peek == Token::LBrace)
            {
                return Ok(None);
            }
            if self.try_parse_ts_bool(|p| Ok(Some(p.ts_next_token_can_follow_modifier())))? {
                return Ok(Some(allowed_modifiers[pos]));
            }
        }
        Ok(None)
    }

    fn parse_ts_bracketed_list<T, F>(
        &mut self,
        kind: ParsingContext,
        parse_element: F,
        bracket: bool,
        skip_first_token: bool,
    ) -> PResult<Vec<T>>
    where
        F: FnMut(&mut Self) -> PResult<T>,
    {
        debug_assert!(self.input().syntax().typescript());
        if !skip_first_token {
            if bracket {
                expect!(self, Token::LBracket);
            } else {
                expect!(self, Token::Lt);
            }
        }
        let result = self.parse_ts_delimited_list(kind, parse_element)?;
        if bracket {
            expect!(self, Token::RBracket);
        } else {
            expect!(self, Token::Gt);
        }
        Ok(result)
    }

    /// `tsParseThisTypeNode`
    fn parse_ts_this_type_node(&mut self) -> PResult<TsThisType> {
        debug_assert!(self.input().syntax().typescript());
        expect!(self, Token::This);
        Ok(TsThisType {
            span: self.input().prev_span(),
        })
    }

    /// `tsParseEntityName`
    fn parse_ts_entity_name(&mut self, allow_reserved_words: bool) -> PResult<TsEntityName> {
        debug_assert!(self.input().syntax().typescript());
        trace_cur!(self, parse_ts_entity_name);
        let start = self.input().cur_pos();
        let init = self.parse_ident_name()?;
        if &*init.sym == "void" {
            let dot_start = self.input().cur_pos();
            let dot_span = self.span(dot_start);
            self.emit_err(dot_span, SyntaxError::TS1005)
        }
        let mut entity = TsEntityName::Ident(init.into());
        while self.input_mut().eat(Token::Dot) {
            let dot_start = self.input().cur_pos();
            let cur = self.input().cur();
            if cur != Token::Hash && !cur.is_word() {
                self.emit_err(
                    Span::new_with_checked(dot_start, dot_start),
                    SyntaxError::TS1003,
                );
                return Ok(entity);
            }
            let left = entity;
            let right = if allow_reserved_words {
                self.parse_ident_name()?
            } else {
                self.parse_ident(false, false)?.into()
            };
            let span = self.span(start);
            entity = TsEntityName::TsQualifiedName(Box::new(TsQualifiedName { span, left, right }));
        }
        Ok(entity)
    }

    pub(crate) fn ts_look_ahead<T, F>(&mut self, op: F) -> T
    where
        F: FnOnce(&mut Self) -> T,
    {
        debug_assert!(self.input().syntax().typescript());
        let checkpoint = self.checkpoint_save();
        self.set_ctx(self.ctx() | Context::IgnoreError);
        let ret = op(self);
        self.checkpoint_load(checkpoint);
        ret
    }

    /// `tsParseTypeArguments`
    pub(crate) fn parse_ts_type_args(&mut self) -> PResult<Box<TsTypeParamInstantiation>> {
        trace_cur!(self, parse_ts_type_args);
        debug_assert!(self.input().syntax().typescript());

        let start = self.input().cur_pos();
        let params = self.in_type(|p| {
            // Temporarily remove a JSX parsing context, which makes us scan different
            // tokens.
            p.ts_in_no_context(|p| {
                if p.input().is(Token::LShift) {
                    p.input_mut().cut_lshift();
                } else {
                    expect!(p, Token::Lt);
                }
                p.parse_ts_delimited_list(ParsingContext::TypeParametersOrArguments, |p| {
                    trace_cur!(p, parse_ts_type_args__arg);

                    p.parse_ts_type()
                })
            })
        })?;
        // This reads the next token after the `>` too, so do this in the enclosing
        // context. But be sure not to parse a regex in the jsx expression
        // `<C<number> />`, so set exprAllowed = false
        self.input_mut().set_expr_allowed(false);
        self.expect_without_advance(Token::Gt)?;
        let span = Span::new_with_checked(start, self.input().cur_span().hi);

        // Report grammar error for empty type argument list like `I<>`.
        if params.is_empty() {
            self.emit_err(span, SyntaxError::EmptyTypeArgumentList);
        }

        Ok(Box::new(TsTypeParamInstantiation { span, params }))
    }

    /// `tsParseTypeReference`
    fn parse_ts_type_ref(&mut self) -> PResult<TsTypeRef> {
        trace_cur!(self, parse_ts_type_ref);
        debug_assert!(self.input().syntax().typescript());

        let start = self.input().cur_pos();

        let has_modifier = self.eat_any_ts_modifier()?;

        let type_name = self.parse_ts_entity_name(/* allow_reserved_words */ true)?;
        trace_cur!(self, parse_ts_type_ref__type_args);
        let type_params = if !self.input().had_line_break_before_cur()
            && (self.input().is(Token::Lt) || self.input().is(Token::LShift))
        {
            let ret = self.do_outside_of_context(
                Context::ShouldNotLexLtOrGtAsType,
                Self::parse_ts_type_args,
            )?;
            self.assert_and_bump(Token::Gt);
            Some(ret)
        } else {
            None
        };

        if has_modifier {
            self.emit_err(self.span(start), SyntaxError::TS2369);
        }

        Ok(TsTypeRef {
            span: self.span(start),
            type_name,
            type_params,
        })
    }

    #[cfg_attr(
        feature = "tracing-spans",
        tracing::instrument(level = "debug", skip_all)
    )]
    pub(crate) fn parse_ts_type_ann(
        &mut self,
        eat_colon: bool,
        start: BytePos,
    ) -> PResult<Box<TsTypeAnn>> {
        trace_cur!(self, parse_ts_type_ann);

        debug_assert!(self.input().syntax().typescript());

        self.in_type(|p| {
            if eat_colon {
                p.assert_and_bump(Token::Colon);
            }

            trace_cur!(p, parse_ts_type_ann__after_colon);

            let type_ann = p.parse_ts_type()?;

            Ok(Box::new(TsTypeAnn {
                span: p.span(start),
                type_ann,
            }))
        })
    }

    /// `tsParseThisTypePredicate`
    fn parse_ts_this_type_predicate(
        &mut self,
        start: BytePos,
        has_asserts_keyword: bool,
        lhs: TsThisType,
    ) -> PResult<TsTypePredicate> {
        debug_assert!(self.input().syntax().typescript());

        let param_name = TsThisTypeOrIdent::TsThisType(lhs);
        let type_ann = if self.input_mut().eat(Token::Is) {
            let cur_pos = self.input().cur_pos();
            Some(self.parse_ts_type_ann(false, cur_pos)?)
        } else {
            None
        };

        Ok(TsTypePredicate {
            span: self.span(start),
            asserts: has_asserts_keyword,
            param_name,
            type_ann,
        })
    }

    /// `tsEatThenParseType`
    fn eat_then_parse_ts_type(&mut self, token_to_eat: Token) -> PResult<Option<Box<TsType>>> {
        if !cfg!(feature = "typescript") {
            return Ok(Default::default());
        }

        self.in_type(|p| {
            if !p.input_mut().eat(token_to_eat) {
                return Ok(None);
            }

            p.parse_ts_type().map(Some)
        })
    }

    /// `tsExpectThenParseType`
    fn expect_then_parse_ts_type(
        &mut self,
        token: Token,
        token_str: &'static str,
    ) -> PResult<Box<TsType>> {
        debug_assert!(self.input().syntax().typescript());

        self.in_type(|p| {
            if !p.input_mut().eat(token) {
                let got = format!("{:?}", p.input().cur());
                syntax_error!(
                    p,
                    p.input().cur_span(),
                    SyntaxError::Unexpected {
                        got,
                        expected: token_str
                    }
                );
            }

            p.parse_ts_type()
        })
    }

    /// `tsParseMappedTypeParameter`
    fn parse_ts_mapped_type_param(&mut self) -> PResult<TsTypeParam> {
        debug_assert!(self.input().syntax().typescript());

        let start = self.input().cur_pos();
        let name = self.parse_ident_name()?;
        let constraint = Some(self.expect_then_parse_ts_type(Token::In, "in")?);

        Ok(TsTypeParam {
            span: self.span(start),
            name: name.into(),
            is_in: false,
            is_out: false,
            is_const: false,
            constraint,
            default: None,
        })
    }

    /// `tsParseTypeParameter`
    fn parse_ts_type_param(
        &mut self,
        permit_in_out: bool,
        permit_const: bool,
    ) -> PResult<TsTypeParam> {
        debug_assert!(self.input().syntax().typescript());

        let mut is_in = false;
        let mut is_out = false;
        let mut is_const = false;

        let start = self.input().cur_pos();

        while let Some(modifer) = self.parse_ts_modifier(
            &[
                "public",
                "private",
                "protected",
                "readonly",
                "abstract",
                "const",
                "override",
                "in",
                "out",
            ],
            false,
        )? {
            match modifer {
                "const" => {
                    is_const = true;
                    if !permit_const {
                        self.emit_err(
                            self.input().prev_span(),
                            SyntaxError::TS1277(atom!("const")),
                        );
                    }
                }
                "in" => {
                    if !permit_in_out {
                        self.emit_err(self.input().prev_span(), SyntaxError::TS1274(atom!("in")));
                    } else if is_in {
                        self.emit_err(self.input().prev_span(), SyntaxError::TS1030(atom!("in")));
                    } else if is_out {
                        self.emit_err(
                            self.input().prev_span(),
                            SyntaxError::TS1029(atom!("in"), atom!("out")),
                        );
                    }
                    is_in = true;
                }
                "out" => {
                    if !permit_in_out {
                        self.emit_err(self.input().prev_span(), SyntaxError::TS1274(atom!("out")));
                    } else if is_out {
                        self.emit_err(self.input().prev_span(), SyntaxError::TS1030(atom!("out")));
                    }
                    is_out = true;
                }
                other => self.emit_err(self.input().prev_span(), SyntaxError::TS1273(other.into())),
            };
        }

        let name = self.in_type(Self::parse_ident_name)?.into();
        let constraint = self.eat_then_parse_ts_type(Token::Extends)?;
        let default = self.eat_then_parse_ts_type(Token::Eq)?;

        Ok(TsTypeParam {
            span: self.span(start),
            name,
            is_in,
            is_out,
            is_const,
            constraint,
            default,
        })
    }

    /// `tsParseTypeParameter`
    pub(crate) fn parse_ts_type_params(
        &mut self,
        permit_in_out: bool,
        permit_const: bool,
    ) -> PResult<Box<TsTypeParamDecl>> {
        self.in_type(|p| {
            p.ts_in_no_context(|p| {
                let start = p.input().cur_pos();
                let cur = p.input().cur();
                if cur != Token::Lt && cur != Token::JSXTagStart {
                    unexpected!(p, "< (jsx tag start)")
                }
                p.bump();

                let params = p.parse_ts_bracketed_list(
                    ParsingContext::TypeParametersOrArguments,
                    |p| p.parse_ts_type_param(permit_in_out, permit_const), // bracket
                    false,
                    // skip_first_token
                    true,
                )?;

                Ok(Box::new(TsTypeParamDecl {
                    span: p.span(start),
                    params,
                }))
            })
        })
    }

    /// `tsTryParseTypeParameters`
    pub(crate) fn try_parse_ts_type_params(
        &mut self,
        permit_in_out: bool,
        permit_const: bool,
    ) -> PResult<Option<Box<TsTypeParamDecl>>> {
        if !cfg!(feature = "typescript") {
            return Ok(None);
        }

        if self.input().cur() == Token::Lt {
            return self
                .parse_ts_type_params(permit_in_out, permit_const)
                .map(Some);
        }

        Ok(None)
    }

    /// `tsParseTypeOrTypePredicateAnnotation`
    pub(crate) fn parse_ts_type_or_type_predicate_ann(
        &mut self,
        return_token: Token,
    ) -> PResult<Box<TsTypeAnn>> {
        debug_assert!(self.input().syntax().typescript());

        self.in_type(|p| {
            let return_token_start = p.input().cur_pos();
            if !p.input_mut().eat(return_token) {
                let cur = format!("{:?}", p.input().cur());
                let span = p.input().cur_span();
                syntax_error!(
                    p,
                    span,
                    SyntaxError::Expected(format!("{return_token:?}"), cur)
                )
            }

            let type_pred_start = p.input().cur_pos();
            let has_type_pred_asserts = p.input().cur() == Token::Asserts && {
                let ctx = p.ctx();
                peek!(p).is_some_and(|peek| {
                    if peek.is_word() {
                        !peek.is_reserved(ctx)
                    } else {
                        false
                    }
                })
            };

            if has_type_pred_asserts {
                p.assert_and_bump(Token::Asserts);
            }

            let has_type_pred_is = p.is_ident_ref()
                && peek!(p).is_some_and(|peek| peek == Token::Is)
                && !p.input_mut().has_linebreak_between_cur_and_peeked();
            let is_type_predicate = has_type_pred_asserts || has_type_pred_is;
            if !is_type_predicate {
                return p.parse_ts_type_ann(
                    // eat_colon
                    false,
                    return_token_start,
                );
            }

            let type_pred_var = p.parse_ident_name()?;
            let type_ann = if has_type_pred_is {
                p.assert_and_bump(Token::Is);
                let pos = p.input().cur_pos();
                Some(p.parse_ts_type_ann(false, pos)?)
            } else {
                None
            };

            let node = Box::new(TsType::TsTypePredicate(TsTypePredicate {
                span: p.span(type_pred_start),
                asserts: has_type_pred_asserts,
                param_name: TsThisTypeOrIdent::Ident(type_pred_var.into()),
                type_ann,
            }));

            Ok(Box::new(TsTypeAnn {
                span: p.span(return_token_start),
                type_ann: node,
            }))
        })
    }

    fn is_start_of_expr(&mut self) -> bool {
        self.is_start_of_left_hand_side_expr() || {
            let cur = self.input().cur();
            matches!(
                cur,
                Token::Plus
                    | Token::Minus
                    | Token::Tilde
                    | Token::Bang
                    | Token::Delete
                    | Token::TypeOf
                    | Token::Void
                    | Token::PlusPlus
                    | Token::MinusMinus
                    | Token::Lt
                    | Token::Await
                    | Token::Yield
            ) || (cur == Token::Hash && peek!(self).is_some_and(|peek| peek.is_word()))
        }
    }

    #[cfg_attr(
        feature = "tracing-spans",
        tracing::instrument(level = "debug", skip_all)
    )]
    pub(super) fn try_parse_ts_type_args(&mut self) -> Option<Box<TsTypeParamInstantiation>> {
        trace_cur!(self, try_parse_ts_type_args);
        debug_assert!(self.input().syntax().typescript());

        self.try_parse_ts(|p| {
            let type_args = p.parse_ts_type_args()?;
            p.assert_and_bump(Token::Gt);
            let cur = p.input().cur();
            if matches!(
                cur,
                Token::Lt
                    | Token::Gt
                    | Token::Eq
                    | Token::RShift
                    | Token::GtEq
                    | Token::Plus
                    | Token::Minus
                    | Token::LParen
                    | Token::NoSubstitutionTemplateLiteral
                    | Token::TemplateHead
                    | Token::BackQuote
            )
            // these should be type
            // arguments in function
            // call or template, not
            // instantiation
            // expression
            {
                Ok(None)
            } else if p.input().had_line_break_before_cur()
                || p.input().cur().is_bin_op()
                || !p.is_start_of_expr()
            {
                Ok(Some(type_args))
            } else {
                Ok(None)
            }
        })
    }

    /// `tsTryParseType`
    fn try_parse_ts_type(&mut self) -> PResult<Option<Box<TsType>>> {
        if !cfg!(feature = "typescript") {
            return Ok(None);
        }

        self.eat_then_parse_ts_type(Token::Colon)
    }

    /// `tsTryParseTypeAnnotation`
    #[cfg_attr(
        feature = "tracing-spans",
        tracing::instrument(level = "debug", skip_all)
    )]
    pub(crate) fn try_parse_ts_type_ann(&mut self) -> PResult<Option<Box<TsTypeAnn>>> {
        if !cfg!(feature = "typescript") {
            return Ok(None);
        }

        if self.input().is(Token::Colon) {
            let pos = self.cur_pos();
            return self.parse_ts_type_ann(/* eat_colon */ true, pos).map(Some);
        }

        Ok(None)
    }

    /// `tsNextThenParseType`
    pub(super) fn next_then_parse_ts_type(&mut self) -> PResult<Box<TsType>> {
        debug_assert!(self.input().syntax().typescript());

        let result = self.in_type(|p| {
            p.bump();
            p.parse_ts_type()
        });

        if !self.ctx().contains(Context::InType) && {
            let cur = self.input().cur();
            cur == Token::Lt || cur == Token::Gt
        } {
            self.input_mut().merge_lt_gt();
        }

        result
    }

    /// `tsParseEnumMember`
    fn parse_ts_enum_member(&mut self) -> PResult<TsEnumMember> {
        debug_assert!(self.input().syntax().typescript());

        let start = self.cur_pos();
        // TypeScript allows computed property names with literal expressions in enums.
        // Non-literal computed properties (like ["a" + "b"]) are rejected.
        // We normalize literal computed properties (["\t"] → "\t") to keep AST simple.
        // See https://github.com/swc-project/swc/issues/11160
        let cur = self.input().cur();
        let id = if cur == Token::Str {
            TsEnumMemberId::Str(self.parse_str_lit())
        } else if cur == Token::Num {
            let token_span = self.input.cur_span();
            let value = self.input_mut().expect_number_token_value();
            self.bump();

            let raw = self.input.iter.read_string(token_span);
            let mut new_raw = String::with_capacity(raw.len() + 2);

            new_raw.push('"');
            new_raw.push_str(raw);
            new_raw.push('"');

            let span = self.span(start);
            // Recover from error
            self.emit_err(span, SyntaxError::TS2452);

            TsEnumMemberId::Str(Str {
                span,
                value: value.to_string().into(),
                raw: Some(new_raw.into()),
            })
        } else if cur == Token::LBracket {
            self.assert_and_bump(Token::LBracket);
            let expr = self.parse_expr()?;
            self.assert_and_bump(Token::RBracket);
            let bracket_span = self.span(start);

            match *expr {
                Expr::Lit(Lit::Str(str_lit)) => {
                    // String literal: ["\t"] → "\t"
                    TsEnumMemberId::Str(str_lit)
                }
                Expr::Tpl(mut tpl) if tpl.exprs.is_empty() => {
                    // Template literal without substitution: [`hello`] → "hello"

                    let tpl = mem::take(tpl.quasis.first_mut().unwrap());

                    let span = tpl.span;
                    let value = tpl.cooked.unwrap();

                    TsEnumMemberId::Str(Str {
                        span,
                        value,
                        raw: None,
                    })
                }
                _ => {
                    // Non-literal expression: report error
                    self.emit_err(bracket_span, SyntaxError::TS1164);
                    TsEnumMemberId::Ident(Ident::new_no_ctxt(atom!(""), bracket_span))
                }
            }
        } else if cur == Token::Error {
            let err = self.input_mut().expect_error_token_and_bump();
            return Err(err);
        } else {
            self.parse_ident_name()
                .map(Ident::from)
                .map(TsEnumMemberId::from)?
        };

        let init = if self.input_mut().eat(Token::Eq) {
            Some(self.parse_assignment_expr()?)
        } else if self.input().cur() == Token::Comma || self.input().cur() == Token::RBrace {
            None
        } else {
            let start = self.cur_pos();
            self.bump();
            self.input_mut().store(Token::Comma);
            self.emit_err(Span::new_with_checked(start, start), SyntaxError::TS1005);
            None
        };

        Ok(TsEnumMember {
            span: self.span(start),
            id,
            init,
        })
    }

    /// `tsParseEnumDeclaration`
    pub(crate) fn parse_ts_enum_decl(
        &mut self,
        start: BytePos,
        is_const: bool,
    ) -> PResult<Box<TsEnumDecl>> {
        debug_assert!(self.input().syntax().typescript());

        let id = self.parse_ident_name()?;
        expect!(self, Token::LBrace);
        let members =
            self.parse_ts_delimited_list(ParsingContext::EnumMembers, Self::parse_ts_enum_member)?;
        expect!(self, Token::RBrace);

        Ok(Box::new(TsEnumDecl {
            span: self.span(start),
            declare: false,
            is_const,
            id: id.into(),
            members,
        }))
    }

    /// `tsTryParseTypeOrTypePredicateAnnotation`
    ///
    /// Used for parsing return types.
    fn try_parse_ts_type_or_type_predicate_ann(&mut self) -> PResult<Option<Box<TsTypeAnn>>> {
        if !cfg!(feature = "typescript") {
            return Ok(None);
        }

        if self.input().is(Token::Colon) {
            self.parse_ts_type_or_type_predicate_ann(Token::Colon)
                .map(Some)
        } else {
            Ok(None)
        }
    }

    /// `tsParseTemplateLiteralType`
    fn parse_ts_tpl_lit_type(&mut self) -> PResult<TsTplLitType> {
        debug_assert!(self.input().syntax().typescript());

        let start = self.cur_pos();

        self.assert_and_bump(Token::BackQuote);

        let (types, quasis) = self.parse_ts_tpl_type_elements()?;

        expect!(self, Token::BackQuote);

        Ok(TsTplLitType {
            span: self.span(start),
            types,
            quasis,
        })
    }

    fn parse_ts_tpl_type_elements(&mut self) -> PResult<(Vec<Box<TsType>>, Vec<TplElement>)> {
        if !cfg!(feature = "typescript") {
            return Ok(Default::default());
        }

        trace_cur!(self, parse_tpl_elements);

        let mut types = Vec::new();

        let cur_elem = self.parse_tpl_element(false)?;
        let mut is_tail = cur_elem.tail;
        let mut quasis = vec![cur_elem];

        while !is_tail {
            expect!(self, Token::DollarLBrace);
            types.push(self.parse_ts_type()?);
            expect!(self, Token::RBrace);
            let elem = self.parse_tpl_element(false)?;
            is_tail = elem.tail;
            quasis.push(elem);
        }

        Ok((types, quasis))
    }

    /// `tsParseLiteralTypeNode`
    fn parse_ts_lit_type_node(&mut self) -> PResult<TsLitType> {
        debug_assert!(self.input().syntax().typescript());

        let start = self.cur_pos();

        let lit = if self.input().is(Token::BackQuote) {
            let tpl = self.parse_ts_tpl_lit_type()?;
            TsLit::Tpl(tpl)
        } else {
            match self.parse_lit()? {
                Lit::BigInt(n) => TsLit::BigInt(n),
                Lit::Bool(n) => TsLit::Bool(n),
                Lit::Num(n) => TsLit::Number(n),
                Lit::Str(n) => TsLit::Str(n),
                _ => unreachable!(),
            }
        };

        Ok(TsLitType {
            span: self.span(start),
            lit,
        })
    }

    /// `tsParseHeritageClause`
    pub(crate) fn parse_ts_heritage_clause(&mut self) -> PResult<Vec<TsExprWithTypeArgs>> {
        debug_assert!(self.input().syntax().typescript());

        self.parse_ts_delimited_list(
            ParsingContext::HeritageClauseElement,
            Self::parse_ts_heritage_clause_element,
        )
    }

    fn parse_ts_heritage_clause_element(&mut self) -> PResult<TsExprWithTypeArgs> {
        debug_assert!(self.input().syntax().typescript());

        let start = self.cur_pos();
        // Note: TS uses parseLeftHandSideExpressionOrHigher,
        // then has grammar errors later if it's not an EntityName.

        let ident = self.parse_ident_name()?.into();
        let expr = self.parse_subscripts(Callee::Expr(ident), true, true)?;
        if !matches!(
            &*expr,
            Expr::Ident(..) | Expr::Member(..) | Expr::TsInstantiation(..)
        ) {
            self.emit_err(self.span(start), SyntaxError::TS2499);
        }

        match *expr {
            Expr::TsInstantiation(v) => Ok(TsExprWithTypeArgs {
                span: v.span,
                expr: v.expr,
                type_args: Some(v.type_args),
            }),
            _ => {
                let type_args = if self.input().is(Token::Lt) {
                    let ret = self.parse_ts_type_args()?;
                    self.assert_and_bump(Token::Gt);
                    Some(ret)
                } else {
                    None
                };

                Ok(TsExprWithTypeArgs {
                    span: self.span(start),
                    expr,
                    type_args,
                })
            }
        }
    }

    /// `tsSkipParameterStart`
    fn skip_ts_parameter_start(&mut self) -> PResult<bool> {
        debug_assert!(self.input().syntax().typescript());

        let _ = self.eat_any_ts_modifier()?;

        let cur = self.input().cur();

        if cur == Token::Void {
            Ok(false)
        } else if cur.is_word() || cur == Token::This {
            self.bump();
            Ok(true)
        } else if (cur == Token::LBrace || cur == Token::LBracket)
            && self.parse_binding_pat_or_ident(false).is_ok()
        {
            Ok(true)
        } else {
            Ok(false)
        }
    }

    /// `tsIsUnambiguouslyStartOfFunctionType`
    fn is_ts_unambiguously_start_of_fn_type(&mut self) -> PResult<bool> {
        debug_assert!(self.input().syntax().typescript());

        self.assert_and_bump(Token::LParen);

        let cur = self.input().cur();
        if cur == Token::RParen || cur == Token::DotDotDot {
            // ( )
            // ( ...
            return Ok(true);
        }
        if self.skip_ts_parameter_start()? {
            let cur = self.input().cur();
            if matches!(
                cur,
                Token::Colon | Token::Comma | Token::Eq | Token::QuestionMark
            ) {
                // ( xxx :
                // ( xxx ,
                // ( xxx ?
                // ( xxx =
                return Ok(true);
            }
            if self.input_mut().eat(Token::RParen) && self.input().cur() == Token::Arrow {
                // ( xxx ) =>
                return Ok(true);
            }
        }
        Ok(false)
    }

    fn is_ts_start_of_fn_type(&mut self) -> bool {
        debug_assert!(self.input().syntax().typescript());

        if self.input().cur() == Token::Lt {
            return true;
        }

        self.input().cur() == Token::LParen
            && self
                .ts_look_ahead(Self::is_ts_unambiguously_start_of_fn_type)
                .unwrap_or_default()
    }

    /// `tsIsUnambiguouslyIndexSignature`
    fn is_ts_unambiguously_index_signature(&mut self) -> bool {
        debug_assert!(self.input().syntax().typescript());

        // Note: babel's comment is wrong
        self.assert_and_bump(Token::LBracket); // Skip '['

        // ',' is for error recovery
        self.eat_ident_ref() && {
            let cur = self.input().cur();
            cur == Token::Comma || cur == Token::Colon
        }
    }

    /// `tsTryParseIndexSignature`
    pub(crate) fn try_parse_ts_index_signature(
        &mut self,
        index_signature_start: BytePos,
        readonly: bool,
        is_static: bool,
    ) -> PResult<Option<TsIndexSignature>> {
        if !cfg!(feature = "typescript") {
            return Ok(Default::default());
        }

        if !(self.input().cur() == Token::LBracket
            && self.ts_look_ahead(Self::is_ts_unambiguously_index_signature))
        {
            return Ok(None);
        }

        expect!(self, Token::LBracket);

        let ident_start = self.cur_pos();
        let mut id = self.parse_ident_name().map(BindingIdent::from)?;
        let type_ann_start = self.cur_pos();

        if self.input_mut().eat(Token::Comma) {
            self.emit_err(id.span, SyntaxError::TS1096);
        } else {
            expect!(self, Token::Colon);
        }

        let type_ann = self.parse_ts_type_ann(/* eat_colon */ false, type_ann_start)?;
        id.span = self.span(ident_start);
        id.type_ann = Some(type_ann);

        expect!(self, Token::RBracket);

        let params = vec![TsFnParam::Ident(id)];

        let ty = self.try_parse_ts_type_ann()?;
        let type_ann = ty;

        self.parse_ts_type_member_semicolon()?;

        Ok(Some(TsIndexSignature {
            span: self.span(index_signature_start),
            readonly,
            is_static,
            params,
            type_ann,
        }))
    }

    /// `tsIsExternalModuleReference`
    fn is_ts_external_module_ref(&mut self) -> bool {
        debug_assert!(self.input().syntax().typescript());
        self.input().is(Token::Require) && peek!(self).is_some_and(|t| t == Token::LParen)
    }

    /// `tsParseModuleReference`
    fn parse_ts_module_ref(&mut self) -> PResult<TsModuleRef> {
        debug_assert!(self.input().syntax().typescript());

        if self.is_ts_external_module_ref() {
            self.parse_ts_external_module_ref().map(From::from)
        } else {
            self.parse_ts_entity_name(/* allow_reserved_words */ false)
                .map(From::from)
        }
    }

    /// `tsParseExternalModuleReference`
    fn parse_ts_external_module_ref(&mut self) -> PResult<TsExternalModuleRef> {
        debug_assert!(self.input().syntax().typescript());

        let start = self.cur_pos();
        expect!(self, Token::Require);
        expect!(self, Token::LParen);
        let cur = self.input().cur();
        if cur == Token::Error {
            let err = self.input_mut().expect_error_token_and_bump();
            return Err(err);
        } else if cur != Token::Str {
            unexpected!(self, "a string literal")
        }
        let expr = self.parse_str_lit();
        expect!(self, Token::RParen);
        Ok(TsExternalModuleRef {
            span: self.span(start),
            expr,
        })
    }

    /// `tsParseImportEqualsDeclaration`
    pub(crate) fn parse_ts_import_equals_decl(
        &mut self,
        start: BytePos,
        id: Ident,
        is_export: bool,
        is_type_only: bool,
    ) -> PResult<Box<TsImportEqualsDecl>> {
        debug_assert!(self.input().syntax().typescript());

        expect!(self, Token::Eq);
        let module_ref = self.parse_ts_module_ref()?;
        self.expect_general_semi()?;

        Ok(Box::new(TsImportEqualsDecl {
            span: self.span(start),
            id,
            is_export,
            is_type_only,
            module_ref,
        }))
    }

    /// `tsParseBindingListForSignature`
    ///
    /// Eats ')` at the end but does not eat `(` at start.
    fn parse_ts_binding_list_for_signature(&mut self) -> PResult<Vec<TsFnParam>> {
        if !cfg!(feature = "typescript") {
            return Ok(Default::default());
        }

        debug_assert!(self.input().syntax().typescript());

        let params = self.parse_formal_params()?;
        let mut list = Vec::with_capacity(4);

        for param in params {
            let item = match param.pat {
                Pat::Ident(pat) => TsFnParam::Ident(pat),
                Pat::Array(pat) => TsFnParam::Array(pat),
                Pat::Object(pat) => TsFnParam::Object(pat),
                Pat::Rest(pat) => TsFnParam::Rest(pat),
                _ => unexpected!(
                    self,
                    "an identifier, [ for an array pattern, { for an object patter or ... for a \
                     rest pattern"
                ),
            };
            list.push(item);
        }
        expect!(self, Token::RParen);
        Ok(list)
    }

    /// `tsIsStartOfMappedType`
    fn is_ts_start_of_mapped_type(&mut self) -> bool {
        debug_assert!(self.input().syntax().typescript());

        self.bump();
        if self.input_mut().eat(Token::Plus) || self.input_mut().eat(Token::Minus) {
            return self.input().is(Token::Readonly);
        }

        self.input_mut().eat(Token::Readonly);

        if !self.input().is(Token::LBracket) {
            return false;
        }
        self.bump();
        if !self.is_ident_ref() {
            return false;
        }
        self.bump();

        self.input().is(Token::In)
    }

    /// `tsParseSignatureMember`
    fn parse_ts_signature_member(
        &mut self,
        kind: SignatureParsingMode,
    ) -> PResult<Either<TsCallSignatureDecl, TsConstructSignatureDecl>> {
        debug_assert!(self.input().syntax().typescript());

        let start = self.cur_pos();

        if kind == SignatureParsingMode::TSConstructSignatureDeclaration {
            expect!(self, Token::New);
        }

        // ----- inlined self.tsFillSignature(tt.colon, node);
        let type_params = self.try_parse_ts_type_params(false, true)?;
        expect!(self, Token::LParen);
        let params = self.parse_ts_binding_list_for_signature()?;
        let type_ann = if self.input().is(Token::Colon) {
            Some(self.parse_ts_type_or_type_predicate_ann(Token::Colon)?)
        } else {
            None
        };
        // -----

        self.parse_ts_type_member_semicolon()?;

        match kind {
            SignatureParsingMode::TSCallSignatureDeclaration => {
                Ok(Either::Left(TsCallSignatureDecl {
                    span: self.span(start),
                    params,
                    type_ann,
                    type_params,
                }))
            }
            SignatureParsingMode::TSConstructSignatureDeclaration => {
                Ok(Either::Right(TsConstructSignatureDecl {
                    span: self.span(start),
                    params,
                    type_ann,
                    type_params,
                }))
            }
        }
    }

    fn try_parse_ts_tuple_element_name(&mut self) -> Option<Pat> {
        if !cfg!(feature = "typescript") {
            return Default::default();
        }

        self.try_parse_ts(|p| {
            let start = p.cur_pos();

            let rest = if p.input_mut().eat(Token::DotDotDot) {
                Some(p.input().prev_span())
            } else {
                None
            };

            let mut ident = p.parse_ident_name().map(Ident::from)?;
            if p.input_mut().eat(Token::QuestionMark) {
                ident.optional = true;
                ident.span = ident.span.with_hi(p.input().prev_span().hi);
            }
            expect!(p, Token::Colon);

            Ok(Some(if let Some(dot3_token) = rest {
                RestPat {
                    span: p.span(start),
                    dot3_token,
                    arg: ident.into(),
                    type_ann: None,
                }
                .into()
            } else {
                ident.into()
            }))
        })
    }

    /// `tsParseTupleElementType`
    fn parse_ts_tuple_element_type(&mut self) -> PResult<TsTupleElement> {
        debug_assert!(self.input().syntax().typescript());

        // parses `...TsType[]`
        let start = self.cur_pos();

        let label = self.try_parse_ts_tuple_element_name();

        if self.input_mut().eat(Token::DotDotDot) {
            let type_ann = self.parse_ts_type()?;
            return Ok(TsTupleElement {
                span: self.span(start),
                label,
                ty: Box::new(TsType::TsRestType(TsRestType {
                    span: self.span(start),
                    type_ann,
                })),
            });
        }

        let ty = self.parse_ts_type()?;
        // parses `TsType?`
        if self.input_mut().eat(Token::QuestionMark) {
            let type_ann = ty;
            return Ok(TsTupleElement {
                span: self.span(start),
                label,
                ty: Box::new(TsType::TsOptionalType(TsOptionalType {
                    span: self.span(start),
                    type_ann,
                })),
            });
        }

        Ok(TsTupleElement {
            span: self.span(start),
            label,
            ty,
        })
    }

    /// `tsParseTupleType`
    fn parse_ts_tuple_type(&mut self) -> PResult<TsTupleType> {
        debug_assert!(self.input().syntax().typescript());

        let start = self.cur_pos();
        let elems = self.parse_ts_bracketed_list(
            ParsingContext::TupleElementTypes,
            Self::parse_ts_tuple_element_type,
            /* bracket */ true,
            /* skipFirstToken */ false,
        )?;

        // Validate the elementTypes to ensure:
        //   No mandatory elements may follow optional elements
        //   If there's a rest element, it must be at the end of the tuple

        let mut seen_optional_element = false;

        for elem in elems.iter() {
            match *elem.ty {
                TsType::TsRestType(..) => {}
                TsType::TsOptionalType(..) => {
                    seen_optional_element = true;
                }
                _ if seen_optional_element => {
                    syntax_error!(self, self.span(start), SyntaxError::TsRequiredAfterOptional)
                }
                _ => {}
            }
        }

        Ok(TsTupleType {
            span: self.span(start),
            elem_types: elems,
        })
    }

    /// `tsParseMappedType`
    fn parse_ts_mapped_type(&mut self) -> PResult<TsMappedType> {
        debug_assert!(self.input().syntax().typescript());

        let start = self.cur_pos();
        expect!(self, Token::LBrace);
        let mut readonly = None;
        let cur = self.input().cur();
        if cur == Token::Plus || cur == Token::Minus {
            readonly = Some(if cur == Token::Plus {
                TruePlusMinus::Plus
            } else {
                TruePlusMinus::Minus
            });
            self.bump();
            expect!(self, Token::Readonly)
        } else if self.input_mut().eat(Token::Readonly) {
            readonly = Some(TruePlusMinus::True);
        }

        expect!(self, Token::LBracket);
        let type_param = self.parse_ts_mapped_type_param()?;
        let name_type = if self.input_mut().eat(Token::As) {
            Some(self.parse_ts_type()?)
        } else {
            None
        };
        expect!(self, Token::RBracket);

        let mut optional = None;
        let cur = self.input().cur();
        if cur == Token::Plus || cur == Token::Minus {
            optional = Some(if cur == Token::Plus {
                TruePlusMinus::Plus
            } else {
                TruePlusMinus::Minus
            });
            self.bump(); // +, -
            expect!(self, Token::QuestionMark);
        } else if self.input_mut().eat(Token::QuestionMark) {
            optional = Some(TruePlusMinus::True);
        }

        let type_ann = self.try_parse_ts_type()?;
        self.expect_general_semi()?;
        expect!(self, Token::RBrace);

        Ok(TsMappedType {
            span: self.span(start),
            readonly,
            optional,
            type_param,
            name_type,
            type_ann,
        })
    }

    /// `tsParseParenthesizedType`
    fn parse_ts_parenthesized_type(&mut self) -> PResult<TsParenthesizedType> {
        debug_assert!(self.input().syntax().typescript());
        trace_cur!(self, parse_ts_parenthesized_type);

        let start = self.cur_pos();
        expect!(self, Token::LParen);
        let type_ann = self.parse_ts_type()?;
        expect!(self, Token::RParen);
        Ok(TsParenthesizedType {
            span: self.span(start),
            type_ann,
        })
    }

    /// `tsParseTypeAliasDeclaration`
    pub(crate) fn parse_ts_type_alias_decl(
        &mut self,
        start: BytePos,
    ) -> PResult<Box<TsTypeAliasDecl>> {
        debug_assert!(self.input().syntax().typescript());

        let id = self.parse_ident_name()?;
        let type_params = self.try_parse_ts_type_params(true, false)?;
        let type_ann = self.expect_then_parse_ts_type(Token::Eq, "=")?;
        self.expect_general_semi()?;
        Ok(Box::new(TsTypeAliasDecl {
            declare: false,
            span: self.span(start),
            id: id.into(),
            type_params,
            type_ann,
        }))
    }

    /// `tsParseFunctionOrConstructorType`
    fn parse_ts_fn_or_constructor_type(
        &mut self,
        is_fn_type: bool,
    ) -> PResult<TsFnOrConstructorType> {
        trace_cur!(self, parse_ts_fn_or_constructor_type);

        debug_assert!(self.input().syntax().typescript());

        let start = self.cur_pos();
        let is_abstract = if !is_fn_type {
            self.input_mut().eat(Token::Abstract)
        } else {
            false
        };
        if !is_fn_type {
            expect!(self, Token::New);
        }

        // ----- inlined `self.tsFillSignature(tt.arrow, node)`
        let type_params = self.try_parse_ts_type_params(false, true)?;
        expect!(self, Token::LParen);
        let params = self.parse_ts_binding_list_for_signature()?;
        let type_ann = self.parse_ts_type_or_type_predicate_ann(Token::Arrow)?;
        // ----- end

        Ok(if is_fn_type {
            TsFnOrConstructorType::TsFnType(TsFnType {
                span: self.span(start),
                type_params,
                params,
                type_ann,
            })
        } else {
            TsFnOrConstructorType::TsConstructorType(TsConstructorType {
                span: self.span(start),
                type_params,
                params,
                type_ann,
                is_abstract,
            })
        })
    }

    /// `tsParseUnionTypeOrHigher`
    fn parse_ts_union_type_or_higher(&mut self) -> PResult<Box<TsType>> {
        trace_cur!(self, parse_ts_union_type_or_higher);
        debug_assert!(self.input().syntax().typescript());

        self.parse_ts_union_or_intersection_type(
            UnionOrIntersection::Union,
            Self::parse_ts_intersection_type_or_higher,
            Token::Pipe,
        )
    }

    /// `tsParseIntersectionTypeOrHigher`
    fn parse_ts_intersection_type_or_higher(&mut self) -> PResult<Box<TsType>> {
        trace_cur!(self, parse_ts_intersection_type_or_higher);

        debug_assert!(self.input().syntax().typescript());

        self.parse_ts_union_or_intersection_type(
            UnionOrIntersection::Intersection,
            Self::parse_ts_type_operator_or_higher,
            Token::Ampersand,
        )
    }

    /// `tsParseTypeOperatorOrHigher`
    fn parse_ts_type_operator_or_higher(&mut self) -> PResult<Box<TsType>> {
        trace_cur!(self, parse_ts_type_operator_or_higher);
        debug_assert!(self.input().syntax().typescript());

        let operator = if self.input().is(Token::Keyof) {
            Some(TsTypeOperatorOp::KeyOf)
        } else if self.input().is(Token::Unique) {
            Some(TsTypeOperatorOp::Unique)
        } else if self.input().is(Token::Readonly) {
            Some(TsTypeOperatorOp::ReadOnly)
        } else {
            None
        };

        match operator {
            Some(operator) => self
                .parse_ts_type_operator(operator)
                .map(TsType::from)
                .map(Box::new),
            None => {
                trace_cur!(self, parse_ts_type_operator_or_higher__not_operator);

                if self.input().is(Token::Infer) {
                    self.parse_ts_infer_type().map(TsType::from).map(Box::new)
                } else {
                    let readonly = self.parse_ts_modifier(&["readonly"], false)?.is_some();
                    self.parse_ts_array_type_or_higher(readonly)
                }
            }
        }
    }

    /// `tsParseTypeOperator`
    fn parse_ts_type_operator(&mut self, op: TsTypeOperatorOp) -> PResult<TsTypeOperator> {
        debug_assert!(self.input().syntax().typescript());

        let start = self.cur_pos();
        match op {
            TsTypeOperatorOp::Unique => expect!(self, Token::Unique),
            TsTypeOperatorOp::KeyOf => expect!(self, Token::Keyof),
            TsTypeOperatorOp::ReadOnly => expect!(self, Token::Readonly),
            #[cfg(swc_ast_unknown)]
            _ => unreachable!(),
        }

        let type_ann = self.parse_ts_type_operator_or_higher()?;
        Ok(TsTypeOperator {
            span: self.span(start),
            op,
            type_ann,
        })
    }

    /// `tsParseInferType`
    fn parse_ts_infer_type(&mut self) -> PResult<TsInferType> {
        debug_assert!(self.input().syntax().typescript());

        let start = self.cur_pos();
        expect!(self, Token::Infer);
        let type_param_name = self.parse_ident_name()?;
        let constraint = self.try_parse_ts(|p| {
            expect!(p, Token::Extends);
            let constraint = p.parse_ts_non_conditional_type();
            if p.ctx().contains(Context::DisallowConditionalTypes)
                || !p.input().is(Token::QuestionMark)
            {
                constraint.map(Some)
            } else {
                Ok(None)
            }
        });
        let type_param = TsTypeParam {
            span: type_param_name.span(),
            name: type_param_name.into(),
            is_in: false,
            is_out: false,
            is_const: false,
            constraint,
            default: None,
        };
        Ok(TsInferType {
            span: self.span(start),
            type_param,
        })
    }

    /// `tsParseNonConditionalType`
    fn parse_ts_non_conditional_type(&mut self) -> PResult<Box<TsType>> {
        trace_cur!(self, parse_ts_non_conditional_type);

        debug_assert!(self.input().syntax().typescript());

        if self.is_ts_start_of_fn_type() {
            return self
                .parse_ts_fn_or_constructor_type(true)
                .map(TsType::from)
                .map(Box::new);
        }
        if (self.input().is(Token::Abstract) && peek!(self).is_some_and(|cur| cur == Token::New))
            || self.input().is(Token::New)
        {
            // As in `new () => Date`
            return self
                .parse_ts_fn_or_constructor_type(false)
                .map(TsType::from)
                .map(Box::new);
        }

        self.parse_ts_union_type_or_higher()
    }

    /// `tsParseArrayTypeOrHigher`
    fn parse_ts_array_type_or_higher(&mut self, readonly: bool) -> PResult<Box<TsType>> {
        trace_cur!(self, parse_ts_array_type_or_higher);
        debug_assert!(self.input().syntax().typescript());

        let mut ty = self.parse_ts_non_array_type()?;

        while !self.input().had_line_break_before_cur() && self.input_mut().eat(Token::LBracket) {
            if self.input_mut().eat(Token::RBracket) {
                ty = Box::new(TsType::TsArrayType(TsArrayType {
                    span: self.span(ty.span_lo()),
                    elem_type: ty,
                }));
            } else {
                let index_type = self.parse_ts_type()?;
                expect!(self, Token::RBracket);
                ty = Box::new(TsType::TsIndexedAccessType(TsIndexedAccessType {
                    span: self.span(ty.span_lo()),
                    readonly,
                    obj_type: ty,
                    index_type,
                }))
            }
        }

        Ok(ty)
    }

    /// Be sure to be in a type context before calling self.
    ///
    /// `tsParseType`
    pub(crate) fn parse_ts_type(&mut self) -> PResult<Box<TsType>> {
        trace_cur!(self, parse_ts_type);

        debug_assert!(self.input().syntax().typescript());

        // Need to set `state.inType` so that we don't parse JSX in a type context.
        debug_assert!(self.ctx().contains(Context::InType));

        let start = self.cur_pos();

        self.do_outside_of_context(Context::DisallowConditionalTypes, |p| {
            let ty = p.parse_ts_non_conditional_type()?;
            if p.input().had_line_break_before_cur() || !p.input_mut().eat(Token::Extends) {
                return Ok(ty);
            }

            let check_type = ty;
            let extends_type = p.do_inside_of_context(
                Context::DisallowConditionalTypes,
                Self::parse_ts_non_conditional_type,
            )?;

            expect!(p, Token::QuestionMark);

            let true_type = p.parse_ts_type()?;

            expect!(p, Token::Colon);

            let false_type = p.parse_ts_type()?;

            Ok(Box::new(TsType::TsConditionalType(TsConditionalType {
                span: p.span(start),
                check_type,
                extends_type,
                true_type,
                false_type,
            })))
        })
    }

    /// `parsePropertyName` in babel.
    ///
    /// Returns `(computed, key)`.
    fn parse_ts_property_name(&mut self) -> PResult<(bool, Box<Expr>)> {
        let (computed, key) = if self.input_mut().eat(Token::LBracket) {
            let key = self.parse_assignment_expr()?;
            expect!(self, Token::RBracket);
            (true, key)
        } else {
            self.do_inside_of_context(Context::InPropertyName, |p| {
                // We check if it's valid for it to be a private name when we push it.
                let cur = p.input().cur();

                let key = if cur == Token::Num || cur == Token::Str {
                    p.parse_new_expr()
                } else if cur == Token::Error {
                    let err = p.input_mut().expect_error_token_and_bump();
                    return Err(err);
                } else {
                    p.parse_maybe_private_name().map(|e| match e {
                        Either::Left(e) => {
                            p.emit_err(e.span(), SyntaxError::PrivateNameInInterface);

                            e.into()
                        }
                        Either::Right(e) => e.into(),
                    })
                };
                key.map(|key| (false, key))
            })?
        };

        Ok((computed, key))
    }

    /// `tsParsePropertyOrMethodSignature`
    fn parse_ts_property_or_method_signature(
        &mut self,
        start: BytePos,
        readonly: bool,
    ) -> PResult<Either<TsPropertySignature, TsMethodSignature>> {
        debug_assert!(self.input().syntax().typescript());

        let (computed, key) = self.parse_ts_property_name()?;

        let optional = self.input_mut().eat(Token::QuestionMark);

        let cur = self.input().cur();
        if matches!(cur, Token::LParen | Token::Lt) {
            if readonly {
                syntax_error!(self, SyntaxError::ReadOnlyMethod);
            }

            let type_params = self.try_parse_ts_type_params(false, true)?;
            expect!(self, Token::LParen);
            let params = self.parse_ts_binding_list_for_signature()?;
            let type_ann = if self.input().is(Token::Colon) {
                self.parse_ts_type_or_type_predicate_ann(Token::Colon)
                    .map(Some)?
            } else {
                None
            };
            // -----

            self.parse_ts_type_member_semicolon()?;
            Ok(Either::Right(TsMethodSignature {
                span: self.span(start),
                computed,
                key,
                optional,
                type_params,
                params,
                type_ann,
            }))
        } else {
            let type_ann = self.try_parse_ts_type_ann()?;

            self.parse_ts_type_member_semicolon()?;
            Ok(Either::Left(TsPropertySignature {
                span: self.span(start),
                computed,
                readonly,
                key,
                optional,
                type_ann,
            }))
        }
    }

    /// `tsParseTypeMember`
    fn parse_ts_type_member(&mut self) -> PResult<TsTypeElement> {
        debug_assert!(self.input().syntax().typescript());

        fn into_type_elem(
            e: Either<TsCallSignatureDecl, TsConstructSignatureDecl>,
        ) -> TsTypeElement {
            match e {
                Either::Left(e) => e.into(),
                Either::Right(e) => e.into(),
            }
        }
        let cur = self.input().cur();
        if cur == Token::LParen || cur == Token::Lt {
            return self
                .parse_ts_signature_member(SignatureParsingMode::TSCallSignatureDeclaration)
                .map(into_type_elem);
        }
        if self.input().is(Token::New)
            && self.ts_look_ahead(Self::is_ts_start_of_construct_signature)
        {
            return self
                .parse_ts_signature_member(SignatureParsingMode::TSConstructSignatureDeclaration)
                .map(into_type_elem);
        }
        // Instead of fullStart, we create a node here.
        let start = self.cur_pos();
        let readonly = self.parse_ts_modifier(&["readonly"], false)?.is_some();

        let idx = self.try_parse_ts_index_signature(start, readonly, false)?;
        if let Some(idx) = idx {
            return Ok(idx.into());
        }

        if let Some(v) = self.try_parse_ts(|p| {
            let start = p.input().cur_pos();

            if readonly {
                syntax_error!(p, SyntaxError::GetterSetterCannotBeReadonly)
            }

            let is_get = if p.input_mut().eat(Token::Get) {
                true
            } else {
                expect!(p, Token::Set);
                false
            };

            let (computed, key) = p.parse_ts_property_name()?;

            if is_get {
                expect!(p, Token::LParen);
                expect!(p, Token::RParen);
                let type_ann = p.try_parse_ts_type_ann()?;

                p.parse_ts_type_member_semicolon()?;

                Ok(Some(TsTypeElement::TsGetterSignature(TsGetterSignature {
                    span: p.span(start),
                    key,
                    computed,
                    type_ann,
                })))
            } else {
                expect!(p, Token::LParen);
                let params = p.parse_ts_binding_list_for_signature()?;
                if params.is_empty() {
                    syntax_error!(p, SyntaxError::SetterParamRequired)
                }
                let param = params.into_iter().next().unwrap();

                p.parse_ts_type_member_semicolon()?;

                Ok(Some(TsTypeElement::TsSetterSignature(TsSetterSignature {
                    span: p.span(start),
                    key,
                    computed,
                    param,
                })))
            }
        }) {
            return Ok(v);
        }

        self.parse_ts_property_or_method_signature(start, readonly)
            .map(|e| match e {
                Either::Left(e) => e.into(),
                Either::Right(e) => e.into(),
            })
    }

    /// `tsParseObjectTypeMembers`
    fn parse_ts_object_type_members(&mut self) -> PResult<Vec<TsTypeElement>> {
        debug_assert!(self.input().syntax().typescript());

        expect!(self, Token::LBrace);
        let members =
            self.parse_ts_list(ParsingContext::TypeMembers, |p| p.parse_ts_type_member())?;
        expect!(self, Token::RBrace);
        Ok(members)
    }

    /// `tsParseTypeLiteral`
    fn parse_ts_type_lit(&mut self) -> PResult<TsTypeLit> {
        debug_assert!(self.input().syntax().typescript());

        let start = self.cur_pos();
        let members = self.parse_ts_object_type_members()?;
        Ok(TsTypeLit {
            span: self.span(start),
            members,
        })
    }

    /// `tsParseInterfaceDeclaration`
    pub(crate) fn parse_ts_interface_decl(
        &mut self,
        start: BytePos,
    ) -> PResult<Box<TsInterfaceDecl>> {
        debug_assert!(self.input().syntax().typescript());

        let id = self.parse_ident_name()?;
        match &*id.sym {
            "string" | "null" | "number" | "object" | "any" | "unknown" | "boolean" | "bigint"
            | "symbol" | "void" | "never" | "intrinsic" => {
                self.emit_err(id.span, SyntaxError::TS2427);
            }
            _ => {}
        }

        let type_params = self.try_parse_ts_type_params(true, false)?;

        let extends = if self.input_mut().eat(Token::Extends) {
            self.parse_ts_heritage_clause()?
        } else {
            Vec::new()
        };

        // Recover from
        //
        //     interface I extends A extends B {}
        if self.input().is(Token::Extends) {
            self.emit_err(self.input().cur_span(), SyntaxError::TS1172);

            while self.input().cur() != Token::Eof && !self.input().is(Token::LBrace) {
                self.bump();
            }
        }

        let body_start = self.cur_pos();
        let body = self.in_type(Self::parse_ts_object_type_members)?;
        let body = TsInterfaceBody {
            span: self.span(body_start),
            body,
        };
        Ok(Box::new(TsInterfaceDecl {
            span: self.span(start),
            declare: false,
            id: id.into(),
            type_params,
            extends,
            body,
        }))
    }

    /// `tsParseTypeAssertion`
    pub(crate) fn parse_ts_type_assertion(&mut self, start: BytePos) -> PResult<TsTypeAssertion> {
        debug_assert!(self.input().syntax().typescript());

        if self.input().syntax().disallow_ambiguous_jsx_like() {
            self.emit_err(self.span(start), SyntaxError::ReservedTypeAssertion);
        }

        // Not actually necessary to set state.inType because we never reach here if JSX
        // plugin is enabled, but need `tsInType` to satisfy the assertion in
        // `tsParseType`.
        let type_ann = self.in_type(Self::parse_ts_type)?;
        expect!(self, Token::Gt);
        let expr = self.parse_unary_expr()?;
        Ok(TsTypeAssertion {
            span: self.span(start),
            type_ann,
            expr,
        })
    }

    /// `tsParseImportType`
    fn parse_ts_import_type(&mut self) -> PResult<TsImportType> {
        let start = self.cur_pos();
        self.assert_and_bump(Token::Import);

        expect!(self, Token::LParen);

        let cur = self.input().cur();

        let arg = if cur == Token::Str {
            self.parse_str_lit()
        } else if cur == Token::Error {
            let err = self.input_mut().expect_error_token_and_bump();
            return Err(err);
        } else {
            let arg_span = self.input().cur_span();
            self.bump();
            self.emit_err(arg_span, SyntaxError::TS1141);
            Str {
                span: arg_span,
                value: Wtf8Atom::default(),
                raw: Some(atom!("\"\"")),
            }
        };

        // the "assert" keyword is deprecated and this syntax is niche, so
        // don't support it
        let attributes = if self.input_mut().eat(Token::Comma)
            && self.input().syntax().import_attributes()
            && self.input().is(Token::LBrace)
        {
            Some(self.parse_ts_call_options()?)
        } else {
            None
        };

        expect!(self, Token::RParen);

        let qualifier = if self.input_mut().eat(Token::Dot) {
            self.parse_ts_entity_name(false).map(Some)?
        } else {
            None
        };

        let type_args = if self.input().is(Token::Lt) {
            let ret = self.do_outside_of_context(
                Context::ShouldNotLexLtOrGtAsType,
                Self::parse_ts_type_args,
            )?;
            self.assert_and_bump(Token::Gt);
            Some(ret)
        } else {
            None
        };

        Ok(TsImportType {
            span: self.span(start),
            arg,
            qualifier,
            type_args,
            attributes,
        })
    }

    fn parse_ts_call_options(&mut self) -> PResult<TsImportCallOptions> {
        debug_assert!(self.input().syntax().typescript());
        let start = self.cur_pos();
        self.assert_and_bump(Token::LBrace);

        expect!(self, Token::With);
        expect!(self, Token::Colon);

        let value = match self.parse_object_expr()? {
            Expr::Object(v) => v,
            _ => unreachable!(),
        };
        self.input_mut().eat(Token::Comma);
        expect!(self, Token::RBrace);
        Ok(TsImportCallOptions {
            span: self.span(start),
            with: Box::new(value),
        })
    }

    /// `tsParseTypeQuery`
    fn parse_ts_type_query(&mut self) -> PResult<TsTypeQuery> {
        debug_assert!(self.input().syntax().typescript());

        let start = self.cur_pos();
        expect!(self, Token::TypeOf);
        let expr_name = if self.input().is(Token::Import) {
            self.parse_ts_import_type().map(From::from)?
        } else {
            self.parse_ts_entity_name(true).map(From::from)?
        };

        let type_args = if !self.input().had_line_break_before_cur() && self.input().is(Token::Lt) {
            let ret = self.do_outside_of_context(
                Context::ShouldNotLexLtOrGtAsType,
                Self::parse_ts_type_args,
            )?;
            self.assert_and_bump(Token::Gt);
            Some(ret)
        } else {
            None
        };

        Ok(TsTypeQuery {
            span: self.span(start),
            expr_name,
            type_args,
        })
    }

    /// `tsParseModuleBlock`
    fn parse_ts_module_block(&mut self) -> PResult<TsModuleBlock> {
        trace_cur!(self, parse_ts_module_block);

        debug_assert!(self.input().syntax().typescript());

        let start = self.cur_pos();
        expect!(self, Token::LBrace);
        let body = self.do_inside_of_context(Context::TsModuleBlock, |p| {
            p.do_outside_of_context(Context::TopLevel, |p| {
                p.parse_module_item_block_body(false, Some(Token::RBrace))
            })
        })?;

        Ok(TsModuleBlock {
            span: self.span(start),
            body,
        })
    }

    /// `tsParseModuleOrNamespaceDeclaration`
    fn parse_ts_module_or_ns_decl(
        &mut self,
        start: BytePos,
        namespace: bool,
    ) -> PResult<Box<TsModuleDecl>> {
        debug_assert!(self.input().syntax().typescript());

        let id = self.parse_ident_name()?;
        let body: TsNamespaceBody = if self.input_mut().eat(Token::Dot) {
            let inner_start = self.cur_pos();
            let inner = self.parse_ts_module_or_ns_decl(inner_start, namespace)?;
            let inner = TsNamespaceDecl {
                span: inner.span,
                id: match inner.id {
                    TsModuleName::Ident(i) => i,
                    _ => unreachable!(),
                },
                body: Box::new(inner.body.unwrap()),
                declare: inner.declare,
                global: inner.global,
            };
            inner.into()
        } else {
            self.parse_ts_module_block().map(From::from)?
        };

        Ok(Box::new(TsModuleDecl {
            span: self.span(start),
            declare: false,
            id: TsModuleName::Ident(id.into()),
            body: Some(body),
            global: false,
            namespace,
        }))
    }

    /// `tsParseAmbientExternalModuleDeclaration`
    fn parse_ts_ambient_external_module_decl(
        &mut self,
        start: BytePos,
    ) -> PResult<Box<TsModuleDecl>> {
        debug_assert!(self.input().syntax().typescript());

        let (global, id) = if self.input().is(Token::Global) {
            let id = self.parse_ident_name()?;
            (true, TsModuleName::Ident(id.into()))
        } else if self.input().cur() == Token::Str {
            let id = TsModuleName::Str(self.parse_str_lit());
            (false, id)
        } else {
            unexpected!(self, "global or a string literal");
        };

        let body = if self.input().is(Token::LBrace) {
            Some(self.parse_ts_module_block().map(TsNamespaceBody::from)?)
        } else {
            self.expect_general_semi()?;
            None
        };

        Ok(Box::new(TsModuleDecl {
            span: self.span(start),
            declare: false,
            id,
            global,
            body,
            namespace: false,
        }))
    }

    /// `tsParseNonArrayType`
    fn parse_ts_non_array_type(&mut self) -> PResult<Box<TsType>> {
        if !cfg!(feature = "typescript") {
            unreachable!()
        }
        trace_cur!(self, parse_ts_non_array_type);
        debug_assert!(self.input().syntax().typescript());

        let start = self.cur_pos();

        let cur = self.input().cur();
        if cur.is_known_ident()
            || matches!(
                cur,
                Token::Ident
                    | Token::Void
                    | Token::Yield
                    | Token::Null
                    | Token::Await
                    | Token::Break
            )
        {
            if self.input().is(Token::Asserts)
                && peek!(self).is_some_and(|peek| peek == Token::This)
            {
                self.bump();
                let this_keyword = self.parse_ts_this_type_node()?;
                return self
                    .parse_ts_this_type_predicate(start, true, this_keyword)
                    .map(TsType::from)
                    .map(Box::new);
            }
            let kind = if self.input().is(Token::Void) {
                Some(TsKeywordTypeKind::TsVoidKeyword)
            } else if self.input().is(Token::Null) {
                Some(TsKeywordTypeKind::TsNullKeyword)
            } else if self.input().is(Token::Any) {
                Some(TsKeywordTypeKind::TsAnyKeyword)
            } else if self.input().is(Token::Boolean) {
                Some(TsKeywordTypeKind::TsBooleanKeyword)
            } else if self.input().is(Token::Bigint) {
                Some(TsKeywordTypeKind::TsBigIntKeyword)
            } else if self.input().is(Token::Never) {
                Some(TsKeywordTypeKind::TsNeverKeyword)
            } else if self.input().is(Token::Number) {
                Some(TsKeywordTypeKind::TsNumberKeyword)
            } else if self.input().is(Token::Object) {
                Some(TsKeywordTypeKind::TsObjectKeyword)
            } else if self.input().is(Token::String) {
                Some(TsKeywordTypeKind::TsStringKeyword)
            } else if self.input().is(Token::Symbol) {
                Some(TsKeywordTypeKind::TsSymbolKeyword)
            } else if self.input().is(Token::Unknown) {
                Some(TsKeywordTypeKind::TsUnknownKeyword)
            } else if self.input().is(Token::Undefined) {
                Some(TsKeywordTypeKind::TsUndefinedKeyword)
            } else if self.input().is(Token::Intrinsic) {
                Some(TsKeywordTypeKind::TsIntrinsicKeyword)
            } else {
                None
            };

            let peeked_is_dot = peek!(self).is_some_and(|cur| cur == Token::Dot);

            match kind {
                Some(kind) if !peeked_is_dot => {
                    self.bump();
                    return Ok(Box::new(TsType::TsKeywordType(TsKeywordType {
                        span: self.span(start),
                        kind,
                    })));
                }
                _ => {
                    return self.parse_ts_type_ref().map(TsType::from).map(Box::new);
                }
            }
        } else if matches!(
            cur,
            Token::BigInt | Token::Str | Token::Num | Token::True | Token::False | Token::BackQuote
        ) {
            return self
                .parse_ts_lit_type_node()
                .map(TsType::from)
                .map(Box::new);
        } else if cur == Token::NoSubstitutionTemplateLiteral || cur == Token::TemplateHead {
            return self.parse_tagged_tpl_ty().map(TsType::from).map(Box::new);
        } else if cur == Token::Minus {
            let start = self.cur_pos();

            self.bump();

            let cur = self.input().cur();
            if !(cur == Token::Num || cur == Token::BigInt) {
                unexpected!(self, "numeric literal or bigint literal")
            }

            let lit = self.parse_lit()?;
            let lit = match lit {
                Lit::Num(Number { span, value, raw }) => {
                    let mut new_raw = String::from("-");

                    match raw {
                        Some(raw) => {
                            new_raw.push_str(&raw);
                        }
                        _ => {
                            write!(new_raw, "{value}").unwrap();
                        }
                    };

                    TsLit::Number(Number {
                        span,
                        value: -value,
                        raw: Some(new_raw.into()),
                    })
                }
                Lit::BigInt(BigInt { span, value, raw }) => {
                    let mut new_raw = String::from("-");

                    match raw {
                        Some(raw) => {
                            new_raw.push_str(&raw);
                        }
                        _ => {
                            write!(new_raw, "{value}").unwrap();
                        }
                    };

                    TsLit::BigInt(BigInt {
                        span,
                        value: Box::new(-*value),
                        raw: Some(new_raw.into()),
                    })
                }
                _ => unreachable!(),
            };

            return Ok(Box::new(TsType::TsLitType(TsLitType {
                span: self.span(start),
                lit,
            })));
        } else if cur == Token::Import {
            return self.parse_ts_import_type().map(TsType::from).map(Box::new);
        } else if cur == Token::This {
            let start = self.cur_pos();
            let this_keyword = self.parse_ts_this_type_node()?;
            return if !self.input().had_line_break_before_cur() && self.input().is(Token::Is) {
                self.parse_ts_this_type_predicate(start, false, this_keyword)
                    .map(TsType::from)
                    .map(Box::new)
            } else {
                Ok(Box::new(TsType::TsThisType(this_keyword)))
            };
        } else if cur == Token::TypeOf {
            return self.parse_ts_type_query().map(TsType::from).map(Box::new);
        } else if cur == Token::LBrace {
            return if self.ts_look_ahead(Self::is_ts_start_of_mapped_type) {
                self.parse_ts_mapped_type().map(TsType::from).map(Box::new)
            } else {
                self.parse_ts_type_lit().map(TsType::from).map(Box::new)
            };
        } else if cur == Token::LBracket {
            return self.parse_ts_tuple_type().map(TsType::from).map(Box::new);
        } else if cur == Token::LParen {
            return self
                .parse_ts_parenthesized_type()
                .map(TsType::from)
                .map(Box::new);
        }

        //   switch (self.state.type) {
        //   }

        unexpected!(
            self,
            "an identifier, void, yield, null, await, break, a string literal, a numeric literal, \
             true, false, `, -, import, this, typeof, {, [, ("
        )
    }

    /// `tsParseExpressionStatement`
    pub(crate) fn parse_ts_expr_stmt(
        &mut self,
        decorators: Vec<Decorator>,
        expr: Ident,
    ) -> PResult<Option<Decl>> {
        if !cfg!(feature = "typescript") {
            return Ok(Default::default());
        }

        let start = expr.span_lo();

        match &*expr.sym {
            "declare" => {
                let decl = self.try_parse_ts_declare(start, decorators)?;
                if let Some(decl) = decl {
                    Ok(Some(make_decl_declare(decl)))
                } else {
                    Ok(None)
                }
            }
            "global" => {
                // `global { }` (with no `declare`) may appear inside an ambient module
                // declaration.
                // Would like to use tsParseAmbientExternalModuleDeclaration here, but already
                // ran past "global".
                if self.input().is(Token::LBrace) {
                    let global = true;
                    let id = TsModuleName::Ident(expr);
                    let body = self
                        .parse_ts_module_block()
                        .map(TsNamespaceBody::from)
                        .map(Some)?;
                    Ok(Some(
                        TsModuleDecl {
                            span: self.span(start),
                            global,
                            declare: false,
                            namespace: false,
                            id,
                            body,
                        }
                        .into(),
                    ))
                } else {
                    Ok(None)
                }
            }
            _ => self.parse_ts_decl(start, decorators, expr.sym, /* next */ false),
        }
    }

    /// `tsTryParseDeclare`
    pub(crate) fn try_parse_ts_declare(
        &mut self,
        start: BytePos,
        decorators: Vec<Decorator>,
    ) -> PResult<Option<Decl>> {
        if !self.syntax().typescript() {
            return Ok(None);
        }

        if self
            .ctx()
            .contains(Context::InDeclare | Context::TsModuleBlock)
        {
            let span_of_declare = self.span(start);
            self.emit_err(span_of_declare, SyntaxError::TS1038);
        }

        let declare_start = start;
        self.do_inside_of_context(Context::InDeclare, |p| {
            if p.input().is(Token::Function) {
                return p
                    .parse_fn_decl(decorators)
                    .map(|decl| match decl {
                        Decl::Fn(f) => FnDecl {
                            declare: true,
                            function: Box::new(Function {
                                span: Span {
                                    lo: declare_start,
                                    ..f.function.span
                                },
                                ..*f.function
                            }),
                            ..f
                        }
                        .into(),
                        _ => decl,
                    })
                    .map(Some);
            }

            if p.input().is(Token::Class) {
                return p
                    .parse_class_decl(start, start, decorators, false)
                    .map(|decl| match decl {
                        Decl::Class(c) => ClassDecl {
                            declare: true,
                            class: Box::new(Class {
                                span: Span {
                                    lo: declare_start,
                                    ..c.class.span
                                },
                                ..*c.class
                            }),
                            ..c
                        }
                        .into(),
                        _ => decl,
                    })
                    .map(Some);
            }

            if p.input().is(Token::Const) && peek!(p).is_some_and(|peek| peek == Token::Enum) {
                p.assert_and_bump(Token::Const);
                p.assert_and_bump(Token::Enum);

                return p
                    .parse_ts_enum_decl(start, /* is_const */ true)
                    .map(|decl| TsEnumDecl {
                        declare: true,
                        span: Span {
                            lo: declare_start,
                            ..decl.span
                        },
                        ..*decl
                    })
                    .map(Box::new)
                    .map(From::from)
                    .map(Some);
            }

            let cur = p.input().cur();
            if matches!(cur, Token::Const | Token::Var | Token::Let) {
                return p
                    .parse_var_stmt(false)
                    .map(|decl| VarDecl {
                        declare: true,
                        span: Span {
                            lo: declare_start,
                            ..decl.span
                        },
                        ..*decl
                    })
                    .map(Box::new)
                    .map(From::from)
                    .map(Some);
            }

            if p.input().is(Token::Global) {
                return p
                    .parse_ts_ambient_external_module_decl(start)
                    .map(Decl::from)
                    .map(make_decl_declare)
                    .map(Some);
            } else if p.input().cur().is_word() {
                let value = p.input().cur().take_word(&p.input);
                return p
                    .parse_ts_decl(start, decorators, value, /* next */ true)
                    .map(|v| v.map(make_decl_declare));
            }

            Ok(None)
        })
    }

    /// `tsTryParseExportDeclaration`
    ///
    /// Note: this won't be called unless the keyword is allowed in
    /// `shouldParseExportDeclaration`.
    pub(crate) fn try_parse_ts_export_decl(
        &mut self,
        decorators: Vec<Decorator>,
        value: Atom,
    ) -> Option<Decl> {
        if !cfg!(feature = "typescript") {
            return None;
        }

        self.try_parse_ts(|p| {
            let start = p.cur_pos();
            let opt = p.parse_ts_decl(start, decorators, value, true)?;
            Ok(opt)
        })
    }

    /// Common to tsTryParseDeclare, tsTryParseExportDeclaration, and
    /// tsParseExpressionStatement.
    ///
    /// `tsParseDeclaration`
    fn parse_ts_decl(
        &mut self,
        start: BytePos,
        decorators: Vec<Decorator>,
        value: Atom,
        next: bool,
    ) -> PResult<Option<Decl>> {
        if !cfg!(feature = "typescript") {
            return Ok(Default::default());
        }

        match &*value {
            "abstract" => {
                if next
                    || (self.input().is(Token::Class) && !self.input().had_line_break_before_cur())
                {
                    if next {
                        self.bump();
                    }
                    return Ok(Some(self.parse_class_decl(start, start, decorators, true)?));
                }
            }

            "enum" => {
                if next || self.is_ident_ref() {
                    if next {
                        self.bump();
                    }
                    return self
                        .parse_ts_enum_decl(start, /* is_const */ false)
                        .map(From::from)
                        .map(Some);
                }
            }

            "interface" => {
                if next || (self.is_ident_ref()) {
                    if next {
                        self.bump();
                    }

                    return self
                        .parse_ts_interface_decl(start)
                        .map(From::from)
                        .map(Some);
                }
            }

            "module" if !self.input().had_line_break_before_cur() => {
                if next {
                    self.bump();
                }

                let cur = self.input().cur();
                if cur == Token::Str {
                    return self
                        .parse_ts_ambient_external_module_decl(start)
                        .map(From::from)
                        .map(Some);
                } else if cur == Token::Error {
                    let err = self.input_mut().expect_error_token_and_bump();
                    return Err(err);
                } else if cur == Token::Eof {
                    return Err(self.eof_error());
                } else if next || self.is_ident_ref() {
                    return self
                        .parse_ts_module_or_ns_decl(start, false)
                        .map(From::from)
                        .map(Some);
                }
            }

            "namespace" => {
                if next || self.is_ident_ref() {
                    if next {
                        self.bump();
                    }
                    return self
                        .parse_ts_module_or_ns_decl(start, true)
                        .map(From::from)
                        .map(Some);
                }
            }

            "type" => {
                if next || (!self.input().had_line_break_before_cur() && self.is_ident_ref()) {
                    if next {
                        self.bump();
                    }
                    return self
                        .parse_ts_type_alias_decl(start)
                        .map(From::from)
                        .map(Some);
                }
            }

            _ => {}
        }

        Ok(None)
    }

    /// `tsTryParseGenericAsyncArrowFunction`
    pub(crate) fn try_parse_ts_generic_async_arrow_fn(
        &mut self,
        start: BytePos,
    ) -> PResult<Option<ArrowExpr>> {
        if !cfg!(feature = "typescript") {
            return Ok(Default::default());
        }

        let cur = self.input().cur();
        let res = if cur == Token::Lt || cur == Token::JSXTagStart {
            self.try_parse_ts(|p| {
                let type_params = p.parse_ts_type_params(false, false)?;

                // In TSX mode, type parameters that could be mistaken for JSX
                // (single param without constraint and no trailing comma) are not
                // allowed.
                if p.input().syntax().jsx() && type_params.params.len() == 1 {
                    let single_param = &type_params.params[0];
                    let has_trailing_comma = type_params.span.hi.0 - single_param.span.hi.0 > 1;
                    let dominated_by_jsx = single_param.constraint.is_none() && !has_trailing_comma;

                    if dominated_by_jsx {
                        return Ok(None);
                    }
                }

                // Don't use overloaded parseFunctionParams which would look for "<" again.
                expect!(p, Token::LParen);
                let params: Vec<Pat> = p
                    .parse_formal_params()?
                    .into_iter()
                    .map(|p| p.pat)
                    .collect();
                expect!(p, Token::RParen);
                let return_type = p.try_parse_ts_type_or_type_predicate_ann()?;
                expect!(p, Token::Arrow);

                Ok(Some((type_params, params, return_type)))
            })
        } else {
            None
        };

        let (type_params, params, return_type) = match res {
            Some(v) => v,
            None => return Ok(None),
        };

        self.do_inside_of_context(Context::InAsync, |p| {
            p.do_outside_of_context(Context::InGenerator, |p| {
                let is_generator = false;
                let is_async = true;
                let body = p.parse_fn_block_or_expr_body(
                    true,
                    false,
                    true,
                    params.is_simple_parameter_list(),
                )?;
                Ok(Some(ArrowExpr {
                    span: p.span(start),
                    body,
                    is_async,
                    is_generator,
                    type_params: Some(type_params),
                    params,
                    return_type,
                    ..Default::default()
                }))
            })
        })
    }
}

#[cfg(test)]
mod tests {
    use swc_atoms::atom;
    use swc_common::DUMMY_SP;
    use swc_ecma_ast::*;
    use swc_ecma_visit::assert_eq_ignore_span;

    use crate::{test_parser, Syntax};

    #[test]
    fn issue_708_1() {
        let actual = test_parser(
            "type test = -1;",
            Syntax::Typescript(Default::default()),
            |p| p.parse_module(),
        );

        let expected = Module {
            span: DUMMY_SP,
            shebang: None,
            body: {
                let first = TsTypeAliasDecl {
                    span: DUMMY_SP,
                    declare: false,
                    id: Ident::new_no_ctxt(atom!("test"), DUMMY_SP),
                    type_params: None,
                    type_ann: Box::new(TsType::TsLitType(TsLitType {
                        span: DUMMY_SP,
                        lit: TsLit::Number(Number {
                            span: DUMMY_SP,
                            value: -1.0,
                            raw: Some(atom!("-1")),
                        }),
                    })),
                }
                .into();
                vec![first]
            },
        };

        assert_eq_ignore_span!(actual, expected);
    }

    #[test]
    fn issue_708_2() {
        let actual = test_parser(
            "const t = -1;",
            Syntax::Typescript(Default::default()),
            |p| p.parse_module(),
        );

        let expected = Module {
            span: DUMMY_SP,
            shebang: None,
            body: {
                let second = VarDecl {
                    span: DUMMY_SP,
                    kind: VarDeclKind::Const,
                    declare: false,
                    decls: vec![VarDeclarator {
                        span: DUMMY_SP,
                        name: Pat::Ident(Ident::new_no_ctxt(atom!("t"), DUMMY_SP).into()),
                        init: Some(Box::new(Expr::Unary(UnaryExpr {
                            span: DUMMY_SP,
                            op: op!(unary, "-"),
                            arg: Box::new(Expr::Lit(Lit::Num(Number {
                                span: DUMMY_SP,
                                value: 1.0,
                                raw: Some(atom!("1")),
                            }))),
                        }))),
                        definite: false,
                    }],
                    ..Default::default()
                }
                .into();
                vec![second]
            },
        };

        assert_eq_ignore_span!(actual, expected);
    }
}
