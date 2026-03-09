use swc_atoms::Atom;
use swc_common::{BytePos, Span, Spanned};
use swc_ecma_ast::*;

use super::{input::Tokens, Parser};
use crate::{
    error::SyntaxError,
    lexer::{Token, TokenFlags},
    Context, PResult,
};

impl<I: Tokens> Parser<I> {
    /// Parses JSX expression enclosed into curly brackets.
    fn parse_jsx_expr_container(&mut self) -> PResult<JSXExprContainer> {
        debug_assert!(self.input().syntax().jsx());
        debug_assert!(self.input().is(Token::LBrace));

        let start = self.input().cur_pos();
        self.bump(); // bump "{"
        let expr = if self.input().is(Token::RBrace) {
            JSXExpr::JSXEmptyExpr(self.parse_jsx_empty_expr())
        } else {
            self.parse_expr().map(JSXExpr::Expr)?
        };
        expect!(self, Token::RBrace);
        Ok(JSXExprContainer {
            span: self.span(start),
            expr,
        })
    }

    /// JSXEmptyExpression is unique type since it doesn't actually parse
    /// anything, and so it should start at the end of last read token (left
    /// brace) and finish at the beginning of the next one (right brace).
    fn parse_jsx_empty_expr(&mut self) -> JSXEmptyExpr {
        debug_assert!(self.input().syntax().jsx());
        let start = self.input().cur_pos();
        JSXEmptyExpr {
            span: Span::new_with_checked(start, start),
        }
    }

    fn jsx_expr_container_to_jsx_attr_value(
        &mut self,
        start: BytePos,
        node: JSXExprContainer,
    ) -> PResult<JSXAttrValue> {
        match node.expr {
            JSXExpr::JSXEmptyExpr(..) => {
                syntax_error!(self, self.span(start), SyntaxError::EmptyJSXAttr)
            }
            JSXExpr::Expr(..) => Ok(node.into()),
            #[cfg(swc_ast_unknown)]
            _ => unreachable!(),
        }
    }

    fn parse_jsx_text(&mut self) -> JSXText {
        debug_assert!(self.input().syntax().jsx());
        let cur = self.input.cur();
        debug_assert!(cur == Token::JSXText);

        let value = self.input.expect_jsx_text_token_value();
        let raw = Atom::new(self.input.cur_string());

        self.input.scan_jsx_token();
        let span = self.input().prev_span();
        JSXText { span, value, raw }
    }

    fn parse_jsx_ident(&mut self) -> PResult<Ident> {
        debug_assert!(self.input().syntax().jsx());
        trace_cur!(self, parse_jsx_ident);
        let cur = self.input().cur();
        if cur == Token::JSXName || cur == Token::Ident {
            if self.input().token_flags().contains(TokenFlags::UNICODE) {
                syntax_error!(
                    self,
                    self.input().cur_span(),
                    SyntaxError::InvalidUnicodeEscape
                );
            }
            let name = cur.take_jsx_name(self.input_mut());
            self.bump();
            let span = self.input().prev_span();
            Ok(Ident::new_no_ctxt(name, span))
        } else {
            unexpected!(self, "jsx identifier")
        }
    }

    fn parse_jsx_tag_name(&mut self) -> PResult<JSXAttrName> {
        debug_assert!(self.input().syntax().jsx());
        trace_cur!(self, parse_jsx_tag_name);
        let start = self.input().cur_pos();
        self.input_mut().scan_jsx_identifier();

        let ns = self.parse_jsx_ident()?.into();
        Ok(if self.input_mut().eat(Token::Colon) {
            self.input_mut().scan_jsx_identifier();
            let name: IdentName = self.parse_jsx_ident()?.into();
            JSXAttrName::JSXNamespacedName(JSXNamespacedName {
                span: Span::new_with_checked(start, name.span.hi),
                ns,
                name,
            })
        } else {
            JSXAttrName::Ident(ns)
        })
    }

    fn parse_jsx_element_name(&mut self) -> PResult<JSXElementName> {
        debug_assert!(self.input().syntax().jsx());
        trace_cur!(self, parse_jsx_element_name);
        let start = self.input().cur_pos();
        let mut node = match self.parse_jsx_tag_name()? {
            JSXAttrName::Ident(i) => JSXElementName::Ident(i.into()),
            JSXAttrName::JSXNamespacedName(i) => JSXElementName::JSXNamespacedName(i),
            #[cfg(swc_ast_unknown)]
            _ => unreachable!(),
        };
        while self.input_mut().eat(Token::Dot) {
            self.input_mut().scan_jsx_identifier();
            let prop: IdentName = self.parse_jsx_ident()?.into();
            let new_node = JSXElementName::JSXMemberExpr(JSXMemberExpr {
                span: self.span(start),
                obj: match node {
                    JSXElementName::Ident(i) => JSXObject::Ident(i),
                    JSXElementName::JSXMemberExpr(i) => JSXObject::JSXMemberExpr(Box::new(i)),
                    _ => unreachable!("JSXNamespacedName -> JSXObject"),
                },
                prop,
            });
            node = new_node;
        }
        Ok(node)
    }

    fn parse_jsx_closing_element(
        &mut self,
        in_expr_context: bool,
        open_name: &JSXElementName,
    ) -> PResult<JSXClosingElement> {
        let start = self.cur_pos();
        self.expect(Token::LessSlash)?;
        let tagname = self.parse_jsx_element_name()?;

        // Handle JSX closing tag followed by '=': '</tag>='
        // When lexer sees '>=' it combines into GtEq, but JSX only needs '>'
        // Use rescan_jsx_open_el_terminal_token to split >= back into >
        self.input_mut().rescan_jsx_open_el_terminal_token();
        self.expect_without_advance(Token::Gt)?;

        if in_expr_context {
            self.bump();
        } else {
            self.input_mut().scan_jsx_token();
        }

        if get_qualified_jsx_name(open_name) != get_qualified_jsx_name(&tagname) {
            syntax_error!(
                self,
                tagname.span(),
                SyntaxError::JSXExpectedClosingTag {
                    tag: get_qualified_jsx_name(open_name),
                }
            )
        }

        let span = self.span(start);
        Ok(JSXClosingElement {
            span,
            name: tagname,
        })
    }

    fn parse_jsx_closing_fragment(&mut self, in_expr_context: bool) -> PResult<JSXClosingFragment> {
        let start = self.cur_pos();
        self.expect(Token::LessSlash)?;

        // Handle JSX closing fragment followed by '=': '</>=
        // When lexer sees '>=' it combines into GtEq, but JSX only needs '>'
        // Use rescan_jsx_open_el_terminal_token to split >= back into >
        self.input_mut().rescan_jsx_open_el_terminal_token();
        self.expect_without_advance(Token::Gt)?;

        if in_expr_context {
            self.bump();
        } else {
            self.input_mut().scan_jsx_token();
        }
        let span = self.span(start);
        Ok(JSXClosingFragment { span })
    }

    fn parse_jsx_children(&mut self) -> Vec<JSXElementChild> {
        let mut list = Vec::with_capacity(8);
        loop {
            self.input_mut().rescan_jsx_token();
            let Ok(Some(child)) = self.parse_jsx_child(self.input().get_cur().token) else {
                break;
            };
            list.push(child);
        }
        list
    }

    fn parse_jsx_child(&mut self, t: Token) -> PResult<Option<JSXElementChild>> {
        debug_assert!(self.input().syntax().jsx());

        match t {
            Token::LessSlash => Ok(None),
            Token::LBrace => Ok(Some({
                self.do_outside_of_context(
                    Context::InCondExpr.union(Context::WillExpectColonForCond),
                    |p| {
                        let start = p.cur_pos();
                        p.bump(); // bump "{"
                        let ret = if p.input().cur() == Token::DotDotDot {
                            p.bump(); // bump "..."
                            let expr = p.parse_expr()?;
                            p.expect_without_advance(Token::RBrace)?;
                            p.input_mut().scan_jsx_token();
                            JSXElementChild::JSXSpreadChild(JSXSpreadChild {
                                span: p.span(start),
                                expr,
                            })
                        } else {
                            let expr = if p.input().cur() == Token::RBrace {
                                JSXExpr::JSXEmptyExpr(p.parse_jsx_empty_expr())
                            } else {
                                p.parse_expr().map(JSXExpr::Expr)?
                            };
                            p.expect_without_advance(Token::RBrace)?;
                            p.input_mut().scan_jsx_token();
                            JSXElementChild::JSXExprContainer(JSXExprContainer {
                                span: p.span(start),
                                expr,
                            })
                        };
                        Ok(ret)
                    },
                )?
            })),
            Token::Lt => {
                let ele = self.parse_jsx_element(false)?;
                match ele {
                    either::Either::Left(frag) => Ok(Some(JSXElementChild::JSXFragment(frag))),
                    either::Either::Right(ele) => {
                        Ok(Some(JSXElementChild::JSXElement(Box::new(ele))))
                    }
                }
            }
            Token::JSXText => Ok(Some(JSXElementChild::JSXText(self.parse_jsx_text()))),
            Token::Eof => {
                unexpected!(self, "< (jsx tag start), jsx text or {")
            }
            _ => unreachable!(),
        }
    }

    fn parse_jsx_attr_name(&mut self) -> PResult<JSXAttrName> {
        debug_assert!(self.input().syntax().jsx());
        trace_cur!(self, parse_jsx_attr_name);
        let start = self.input().cur_pos();
        self.input_mut().scan_jsx_identifier();

        let attr_name = self.parse_jsx_ident()?;
        if self.input_mut().eat(Token::Colon) {
            self.input_mut().scan_jsx_identifier();
            let name = self.parse_jsx_ident()?;
            Ok(JSXAttrName::JSXNamespacedName(JSXNamespacedName {
                span: Span::new_with_checked(start, name.span.hi),
                ns: attr_name.into(),
                name: name.into(),
            }))
        } else {
            Ok(JSXAttrName::Ident(attr_name.into()))
        }
    }

    fn parse_jsx_attr_value(&mut self) -> PResult<Option<JSXAttrValue>> {
        debug_assert!(self.input().syntax().jsx());
        trace_cur!(self, parse_jsx_attr_value);
        if self.input().is(Token::Eq) {
            self.input_mut().scan_jsx_attribute_value();
            let cur = self.input().get_cur();
            match cur.token {
                Token::Str => {
                    let value = self.parse_str_lit();
                    Ok(Some(JSXAttrValue::Str(value)))
                }
                Token::LBrace => {
                    let start = self.cur_pos();
                    let node = self.parse_jsx_expr_container()?;
                    self.jsx_expr_container_to_jsx_attr_value(start, node)
                        .map(Some)
                }
                Token::Lt => match self.parse_jsx_element(true)? {
                    either::Either::Left(frag) => Ok(Some(JSXAttrValue::JSXFragment(frag))),
                    either::Either::Right(ele) => Ok(Some(JSXAttrValue::JSXElement(Box::new(ele)))),
                },
                _ => {
                    let span = self.input().cur_span();
                    syntax_error!(self, span, SyntaxError::InvalidJSXValue)
                }
            }
        } else {
            Ok(None)
        }
    }

    fn parse_jsx_attr(&mut self) -> PResult<JSXAttrOrSpread> {
        debug_assert!(self.input().syntax().jsx());
        trace_cur!(self, parse_jsx_attr);
        if self.input_mut().eat(Token::LBrace) {
            let dot3_start = self.input().cur_pos();
            self.expect(Token::DotDotDot)?;
            let dot3_token = self.span(dot3_start);
            let expr = self.parse_assignment_expr()?;
            self.expect(Token::RBrace)?;
            Ok(JSXAttrOrSpread::SpreadElement(SpreadElement {
                dot3_token,
                expr,
            }))
        } else {
            let start = self.input().cur_pos();
            let name = self.parse_jsx_attr_name()?;
            let value = self.do_outside_of_context(
                Context::InCondExpr.union(Context::WillExpectColonForCond),
                |p| p.parse_jsx_attr_value(),
            )?;
            Ok(JSXAttrOrSpread::JSXAttr(JSXAttr {
                span: self.span(start),
                name,
                value,
            }))
        }
    }

    fn parse_jsx_attrs(&mut self) -> PResult<Vec<JSXAttrOrSpread>> {
        let mut attrs = Vec::with_capacity(8);

        loop {
            trace_cur!(self, parse_jsx_opening__attrs_loop);
            self.input_mut().rescan_jsx_open_el_terminal_token();
            let cur = self.input().get_cur();
            if matches!(cur.token, Token::Gt | Token::Slash) {
                break;
            }
            let attr = self.parse_jsx_attr()?;
            attrs.push(attr);
        }

        Ok(attrs)
    }

    pub(crate) fn parse_jsx_element(
        &mut self,
        in_expr_context: bool,
    ) -> PResult<either::Either<JSXFragment, JSXElement>> {
        debug_assert!(self.input().syntax().jsx());
        trace_cur!(self, parse_jsx_element);

        let start = self.cur_pos();

        self.do_outside_of_context(Context::ShouldNotLexLtOrGtAsType, |p| {
            p.expect(Token::Lt)?;

            // Handle JSX fragment opening followed by '=': '<>='
            // When lexer sees '>=' it combines into GtEq, but JSX fragment only needs '>'
            // Use rescan_jsx_open_el_terminal_token to split >= back into >
            p.input_mut().rescan_jsx_open_el_terminal_token();

            if p.input().cur() == Token::Gt {
                // <>xxxxxx</>
                p.input_mut().scan_jsx_token();
                let opening = JSXOpeningFragment {
                    span: p.span(start),
                };
                let children = p.parse_jsx_children();
                let closing = p.parse_jsx_closing_fragment(in_expr_context)?;
                let span = p.span(start);
                Ok(either::Either::Left(JSXFragment {
                    span,
                    opening,
                    children,
                    closing,
                }))
            } else {
                let name = p.do_outside_of_context(Context::ShouldNotLexLtOrGtAsType, |p| {
                    p.parse_jsx_element_name()
                })?;
                let type_args = if p.input().syntax().typescript() && p.input().is(Token::Lt) {
                    p.try_parse_ts(|p| {
                        let ret = p.parse_ts_type_args()?;
                        p.assert_and_bump(Token::Gt);
                        Ok(Some(ret))
                    })
                } else {
                    None
                };
                let attrs = p.parse_jsx_attrs()?;
                if p.input().cur() == Token::Gt {
                    // <xxxxx>xxxxx</xxxxx>
                    p.input_mut().scan_jsx_token();
                    let span = Span::new_with_checked(start, p.input.get_cur().span.lo);
                    let opening = JSXOpeningElement {
                        span,
                        name,
                        type_args,
                        attrs,
                        self_closing: false,
                    };
                    let children = p.parse_jsx_children();
                    let closing = p.parse_jsx_closing_element(in_expr_context, &opening.name)?;
                    let span = if in_expr_context {
                        Span::new_with_checked(start, p.last_pos())
                    } else {
                        Span::new_with_checked(start, p.cur_pos())
                    };
                    Ok(either::Either::Right(JSXElement {
                        span,
                        opening,
                        children,
                        closing: Some(closing),
                    }))
                } else {
                    // <xxxxx/>
                    p.expect(Token::Slash)?;

                    // Handle JSX self-closing tag followed by '=': '<tag/>='
                    // When lexer sees '>=' it combines into GtEq, but JSX only needs '>'
                    // Use rescan_jsx_open_el_terminal_token to split >= back into >
                    p.input_mut().rescan_jsx_open_el_terminal_token();
                    p.expect_without_advance(Token::Gt)?;

                    if in_expr_context {
                        p.bump();
                    } else {
                        p.input_mut().scan_jsx_token();
                    }
                    let span = if in_expr_context {
                        p.span(start)
                    } else {
                        Span::new_with_checked(start, p.cur_pos())
                    };
                    Ok(either::Either::Right(JSXElement {
                        span,
                        opening: JSXOpeningElement {
                            span,
                            name,
                            type_args,
                            attrs,
                            self_closing: true,
                        },
                        children: Vec::new(),
                        closing: None,
                    }))
                }
            }
        })
    }
}

fn get_qualified_jsx_name(name: &JSXElementName) -> Atom {
    fn get_qualified_obj_name(obj: &JSXObject) -> Atom {
        match *obj {
            JSXObject::Ident(ref i) => i.sym.clone(),
            JSXObject::JSXMemberExpr(ref member) => format!(
                "{}.{}",
                get_qualified_obj_name(&member.obj),
                member.prop.sym
            )
            .into(),
            #[cfg(swc_ast_unknown)]
            _ => unreachable!(),
        }
    }
    match *name {
        JSXElementName::Ident(ref i) => i.sym.clone(),
        JSXElementName::JSXNamespacedName(JSXNamespacedName {
            ref ns, ref name, ..
        }) => format!("{}:{}", ns.sym, name.sym).into(),
        JSXElementName::JSXMemberExpr(JSXMemberExpr {
            ref obj, ref prop, ..
        }) => format!("{}.{}", get_qualified_obj_name(obj), prop.sym).into(),
        #[cfg(swc_ast_unknown)]
        _ => unreachable!(),
    }
}

#[cfg(test)]
mod tests {
    use swc_atoms::atom;
    use swc_common::DUMMY_SP as span;
    use swc_ecma_visit::assert_eq_ignore_span;

    use super::super::*;

    fn jsx(src: &'static str) -> Box<Expr> {
        test_parser(
            src,
            crate::Syntax::Es(crate::EsSyntax {
                jsx: true,
                ..Default::default()
            }),
            |p| p.parse_expr(),
        )
    }

    #[test]
    fn self_closing_01() {
        assert_eq_ignore_span!(
            jsx("<a />"),
            Box::new(Expr::JSXElement(Box::new(JSXElement {
                span,
                opening: JSXOpeningElement {
                    span,
                    name: JSXElementName::Ident(Ident::new_no_ctxt(atom!("a"), span)),
                    self_closing: true,
                    attrs: Vec::new(),
                    type_args: None,
                },
                children: Vec::new(),
                closing: None,
            })))
        );
    }

    #[test]
    fn normal_01() {
        assert_eq_ignore_span!(
            jsx("<a>foo</a>"),
            Box::new(Expr::JSXElement(Box::new(JSXElement {
                span,
                opening: JSXOpeningElement {
                    span,
                    name: JSXElementName::Ident(Ident::new_no_ctxt(atom!("a"), span)),
                    self_closing: false,
                    attrs: Vec::new(),
                    type_args: None,
                },
                children: vec![JSXElementChild::JSXText(JSXText {
                    span,
                    raw: atom!("foo"),
                    value: atom!("foo"),
                })],
                closing: Some(JSXClosingElement {
                    span,
                    name: JSXElementName::Ident(Ident::new_no_ctxt(atom!("a"), span)),
                })
            })))
        );
    }

    #[test]
    fn escape_in_attr() {
        assert_eq_ignore_span!(
            jsx(r#"<div id="w &lt; w" />;"#),
            Box::new(Expr::JSXElement(Box::new(JSXElement {
                span,
                opening: JSXOpeningElement {
                    span,
                    attrs: vec![JSXAttrOrSpread::JSXAttr(JSXAttr {
                        span,
                        name: JSXAttrName::Ident(IdentName::new(atom!("id"), span)),
                        value: Some(JSXAttrValue::Str(Str {
                            span,
                            value: atom!("w < w").into(),
                            raw: Some(atom!("\"w &lt; w\"")),
                        })),
                    })],
                    name: JSXElementName::Ident(Ident::new_no_ctxt(atom!("div"), span)),
                    self_closing: true,
                    type_args: None,
                },
                children: Vec::new(),
                closing: None
            })))
        );
    }

    #[test]
    fn issue_584() {
        assert_eq_ignore_span!(
            jsx(r#"<test other={4} />;"#),
            Box::new(Expr::JSXElement(Box::new(JSXElement {
                span,
                opening: JSXOpeningElement {
                    span,
                    name: JSXElementName::Ident(Ident::new_no_ctxt(atom!("test"), span)),
                    attrs: vec![JSXAttrOrSpread::JSXAttr(JSXAttr {
                        span,
                        name: JSXAttrName::Ident(IdentName::new(atom!("other"), span)),
                        value: Some(JSXAttrValue::JSXExprContainer(JSXExprContainer {
                            span,
                            expr: JSXExpr::Expr(Box::new(Expr::Lit(Lit::Num(Number {
                                span,
                                value: 4.0,
                                raw: Some(atom!("4"))
                            }))))
                        })),
                    })],
                    self_closing: true,
                    type_args: None,
                },
                children: Vec::new(),
                closing: None
            })))
        );
    }
}
