use crate::attr::{self, Then};
use crate::error::{Error, Result};
use crate::{constfn, expr, iter, token};
use proc_macro::{Delimiter, Group, Ident, Punct, Spacing, Span, TokenStream, TokenTree};
use std::iter::FromIterator;

pub fn cfg(introducer: &str, args: TokenStream, input: TokenStream) -> TokenStream {
    try_cfg(introducer, args, input).unwrap_or_else(Error::into_compile_error)
}

fn try_cfg(introducer: &str, args: TokenStream, input: TokenStream) -> Result<TokenStream> {
    let introducer = Ident::new(introducer, Span::call_site());

    let mut full_args = TokenStream::from(TokenTree::Ident(introducer));
    if !args.is_empty() {
        full_args.extend(std::iter::once(TokenTree::Group(Group::new(
            Delimiter::Parenthesis,
            args,
        ))));
    }

    let ref mut full_args = iter::new(full_args);
    let expr = expr::parse(full_args)?;
    token::parse_end(full_args)?;

    if expr.eval(crate::RUSTVERSION) {
        Ok(allow_incompatible_msrv(input))
    } else {
        Ok(TokenStream::new())
    }
}

pub fn try_attr(args: attr::Args, input: TokenStream) -> Result<TokenStream> {
    if !args.condition.eval(crate::RUSTVERSION) {
        return Ok(input);
    }

    let output = match args.then {
        Then::Const(const_token) => constfn::insert_const(input, const_token)?,
        Then::Attribute(then) => {
            TokenStream::from_iter(
                // #[cfg_attr(all(), #then)]
                vec![
                    TokenTree::Punct(Punct::new('#', Spacing::Alone)),
                    TokenTree::Group(Group::new(
                        Delimiter::Bracket,
                        TokenStream::from_iter(vec![
                            TokenTree::Ident(Ident::new("cfg_attr", Span::call_site())),
                            TokenTree::Group(Group::new(
                                Delimiter::Parenthesis,
                                TokenStream::from_iter(
                                    vec![
                                        TokenTree::Ident(Ident::new("all", Span::call_site())),
                                        TokenTree::Group(Group::new(
                                            Delimiter::Parenthesis,
                                            TokenStream::new(),
                                        )),
                                        TokenTree::Punct(Punct::new(',', Spacing::Alone)),
                                    ]
                                    .into_iter()
                                    .chain(then),
                                ),
                            )),
                        ]),
                    )),
                ]
                .into_iter()
                .chain(input),
            )
        }
    };

    Ok(allow_incompatible_msrv(output))
}

fn allow_incompatible_msrv(input: TokenStream) -> TokenStream {
    TokenStream::from_iter(
        // #[allow(clippy::incompatible_msrv)]
        vec![
            TokenTree::Punct(Punct::new('#', Spacing::Alone)),
            TokenTree::Group(Group::new(
                Delimiter::Bracket,
                TokenStream::from_iter(vec![
                    TokenTree::Ident(Ident::new("allow", Span::call_site())),
                    TokenTree::Group(Group::new(
                        Delimiter::Parenthesis,
                        TokenStream::from_iter(vec![
                            TokenTree::Ident(Ident::new("clippy", Span::call_site())),
                            TokenTree::Punct(Punct::new(':', Spacing::Joint)),
                            TokenTree::Punct(Punct::new(':', Spacing::Alone)),
                            TokenTree::Ident(Ident::new("incompatible_msrv", Span::call_site())),
                        ]),
                    )),
                ]),
            )),
        ]
        .into_iter()
        .chain(input),
    )
}
