use std::{
    collections::{BTreeMap, HashMap},
    fmt::Debug,
};

use proc_macro2::{TokenStream, TokenTree};
use quote::{ToTokens, TokenStreamExt};
use syn::{
    braced, bracketed,
    buffer::{Cursor, TokenBuffer},
    parenthesized,
    parse::{self, Parse, ParseStream},
    token, Error, Ident, ImplItem, ImplItemMacro, Item, ItemMacro, Token,
};

#[derive(Default)]
pub struct Macros {
    defs: BTreeMap<Ident, MacroDef>,
}

impl Macros {
    pub fn new() -> Macros {
        Macros {
            defs: BTreeMap::new(),
        }
    }

    pub fn add_item_macro(&mut self, input: &ItemMacro) {
        assert!(
            input.ident.is_some(),
            "Expected macro_rules! def. Got {input:?}"
        );
        let m = input.mac.parse_body::<MacroDef>();
        if let Ok(mac) = m {
            let ident = input.ident.clone().unwrap();
            self.defs.insert(ident, mac);
        }
    }

    pub fn evaluate_item_macro(&self, input: &ItemMacro) -> Vec<Item> {
        assert!(input.ident.is_none(), "Expected macro usage. Got {input:?}");
        let m = input.mac.parse_body::<TokenStream>();
        if let Ok(mac) = m {
            // FIXME: Extremely hacky. In the future for importing macros, we'll want to do something else.
            let ident = input.mac.path.segments.last().unwrap().ident.clone();

            if let Some(def) = self.defs.get(&ident) {
                def.evaluate(mac)
            } else {
                panic!("Could not find definition for {ident}. Have you tried creating a #[diplomat::macro_rules] macro_rules! {ident} definition?");
            }
        } else {
            // We handle errors automatically in `diplomat/macro`
            Vec::new()
        }
    }

    pub fn evaluate_impl_item_macro(&self, input: &ImplItemMacro) -> Vec<ImplItem> {
        let m: syn::Result<TokenStream> = input.mac.parse_body();
        // FIXME: Extremely hacky. In the future for importing macros, we'll want to do something else.
        let path_ident = input.mac.path.segments.last().unwrap().ident.clone();

        if let Ok(matched) = m {
            if let Some(def) = self.defs.get(&path_ident) {
                def.evaluate(matched)
            } else {
                panic!("Could not find definition for {path_ident}. Have you tried creating a #[diplomat::macro_rules] macro_rules! {path_ident} definition?");
            }
        } else {
            // We handle errors automatically in `diplomat/macro`
            Vec::new()
        }
    }
}

macro_rules! define_macro_fragments {
    ($($i:ident($p:path)),*) => {
        #[derive(Debug)]
        pub enum MacroFrag {
            $(
                $i($p),
            )*
        }

        impl ToTokens for MacroFrag {
            fn to_token_stream(&self) -> TokenStream {
                let mut tokens = TokenStream::new();
                self.to_tokens(&mut tokens);
                tokens
            }

            fn to_tokens(&self, tokens : &mut TokenStream) {
                match self {
                    $(
                        MacroFrag::$i(item) => item.to_tokens(tokens),
                    )*
                }
            }

            fn into_token_stream(self) -> TokenStream
            where
                Self: Sized,
            {
                match self {
                    $(
                        MacroFrag::$i(item) => item.to_token_stream(),
                    )*
                }
            }
        }

        $(
            impl From<$p> for MacroFrag {
                fn from(value : $p) -> MacroFrag {
                    MacroFrag::$i(value)
                }
            }
        )*
    }
}

define_macro_fragments! {
    Block(syn::Block),
    Expr(syn::Expr),
    Ident(syn::Ident),
    Item(syn::Item),
    Lifetime(syn::Lifetime),
    Literal(syn::Lit),
    Meta(syn::Meta),
    // TODO:
    Pat(syn::Pat),
    // TODO:
    // PatParam()
    Path(syn::Path),
    Stmt(syn::Stmt),
    TokenTree(proc_macro2::TokenTree),
    Ty(syn::Type),
    Vis(syn::Visibility)
}

#[derive(Debug, Clone)]
/// Represents $Identifier:MacroFragSpec (see https://doc.rust-lang.org/reference/macros-by-example.html#railroad-MacroMatch)
pub struct MacroIdent {
    /// represents Identifier.
    pub ident: Ident,
    /// Represents MacroFragSpec. Parsed in construction of [`MacroDef`]
    pub ty: Ident,
}

impl Parse for MacroIdent {
    fn parse(input: parse::ParseStream) -> syn::Result<Self> {
        input.parse::<Token![$]>()?;
        let ident: Ident = input.parse()?;
        input.parse::<Token![:]>()?;
        let ty: Ident = input.parse()?;
        Ok(Self { ident, ty })
    }
}

/// Hack to read information from the TokenTree while saving the rest to be later copied into a buffer.
/// Will attempt to read `T`, and on a success all of the unread tokens after the stream will be written to `remaining`.
struct ParsePartial<T: Parse> {
    item: T,
    remaining: TokenStream,
}

impl<T: Parse> Parse for ParsePartial<T> {
    fn parse(input: ParseStream) -> syn::Result<Self> {
        let item = input.parse::<T>()?;
        let remaining = input.parse()?;
        Ok(Self { item, remaining })
    }
}

impl<T: Parse> ParsePartial<T>
where
    MacroFrag: From<T>,
{
    fn try_parse(
        i: &MacroIdent,
        cursor: &Cursor,
        args: &mut HashMap<Ident, MacroFrag>,
    ) -> syn::Result<TokenBuffer> {
        let out = syn::parse2::<ParsePartial<T>>(cursor.token_stream())?;

        args.insert(i.ident.clone(), MacroFrag::from(out.item));

        let buf = TokenBuffer::new2(out.remaining);
        Ok(buf)
    }
}

#[derive(Debug)]
#[non_exhaustive]
/// A site where a macro is used (i.e., `example!(...)`)
/// Constructed in [`MacroUse::parse`] (called by [`MacroDef::evaluate`]) when a previously defined macro is used. We use the definition of [`MacroDef`] to then construct the args in the used macro.
pub struct MacroUse {
    /// The arguments $argname:MacroFragSpec passed to the macro. Indexed by `argname`. Used for substitution during [`MacroDef::evaluate`].
    args: HashMap<Ident, MacroFrag>,
}

impl MacroUse {
    /// Not an official implementation of [`syn::parse::Parse`], since we need to know the [`MacroDef`] to know how to extract [`MacroUse::args`].
    fn parse(def: &MacroDef, stream: TokenStream) -> syn::Result<Self> {
        let mut args = HashMap::new();

        Self::parse_macro_matcher(&def.matcher.matches, stream, &mut args)?;

        Ok(Self { args })
    }

    fn parse_macro_matcher(
        matches: &[MacroMatch],
        stream: TokenStream,
        args: &mut HashMap<Ident, MacroFrag>,
    ) -> syn::Result<()> {
        let mut buf = TokenBuffer::new2(stream);
        let mut c = buf.begin();

        let mut match_iter = matches.iter();
        while let Some((tt, next)) = c.token_tree() {
            let curr_match = match_iter.next();
            match curr_match {
                Some(MacroMatch::Ident(i)) => match i.ty.to_string().as_str() {
                    "block" => {
                        if let TokenTree::Group(..) = &tt {
                            args.insert(
                                i.ident.clone(),
                                MacroFrag::Block(syn::parse2::<syn::Block>(tt.into())?),
                            );
                            c = next;
                        } else {
                            return Err(Error::new(
                                c.span(),
                                format!("${}:block expected. Got {tt:?}", i.ident),
                            ));
                        }
                    }
                    "expr" => {
                        buf = ParsePartial::<syn::Expr>::try_parse(i, &c, args)?;
                        c = buf.begin();
                    }
                    "ident" => {
                        buf = ParsePartial::<syn::Ident>::try_parse(i, &c, args)?;
                        c = buf.begin();
                    }
                    "item" => {
                        buf = ParsePartial::<syn::Item>::try_parse(i, &c, args)?;
                        c = buf.begin();
                    }
                    "lifetime" => {
                        buf = ParsePartial::<syn::Lifetime>::try_parse(i, &c, args)?;
                        c = buf.begin();
                    }
                    "literal" => {
                        buf = ParsePartial::<syn::Lit>::try_parse(i, &c, args)?;
                        c = buf.begin();
                    }
                    "meta" => {
                        buf = ParsePartial::<syn::Meta>::try_parse(i, &c, args)?;
                        c = buf.begin();
                    }
                    "pat" => {
                        return Err(Error::new(
                            c.span(),
                            format!("${}:pat MacroFragSpec currently unsupported.", i.ident),
                        ));
                    }
                    "path" => {
                        buf = ParsePartial::<syn::Path>::try_parse(i, &c, args)?;
                        c = buf.begin();
                    }
                    "stmt" => {
                        // Syn expects a semicolon, Rust does not. This constitutes a parsing problem.
                        return Err(Error::new(
                            c.span(),
                            format!("${}:stmt MacroFragSpec currently unsupported.", i.ident),
                        ));
                        // buf = MaybeParse::<syn::Stmt>::try_parse(i, &c, args)?;

                        // c = buf.begin();
                    }
                    "tt" => {
                        args.insert(i.ident.clone(), MacroFrag::TokenTree(tt));
                        c = next;
                    }
                    "ty" => {
                        buf = ParsePartial::<syn::Type>::try_parse(i, &c, args)?;
                        c = buf.begin();
                    }
                    "vis" => {
                        buf = ParsePartial::<syn::Visibility>::try_parse(i, &c, args)?;
                        c = buf.begin();
                    }
                    _ => {
                        return Err(Error::new(
                            c.span(),
                            format!("${}, unsupported MacroFragSpec :{}", i.ident, i.ty),
                        ));
                    }
                },
                Some(MacroMatch::Tokens(t)) => {
                    Self::get_tokens_match(&mut c, t)?;
                }
                Some(MacroMatch::MacroMatcher(matcher)) => {
                    if let TokenTree::Group(g) = &tt {
                        Self::parse_macro_matcher(&matcher.matches, g.stream(), args)?;
                        c = next;
                    } else {
                        return Err(Error::new(
                            c.span(),
                            format!("Macro use error: expected {:?}", matcher.delim),
                        ));
                    }
                }
                None => {
                    return Err(Error::new(
                        c.span(),
                        format!("Macro use error, expected no more tokens. Got {tt:?}"),
                    ))
                }
            }
        }
        Ok(())
    }

    fn get_tokens_match(cursor: &mut Cursor, t: &TokenStream) -> syn::Result<()> {
        let other_iter = t.clone().into_iter();
        for other_tt in other_iter {
            let maybe_tt = cursor.token_tree();

            if let Some((tt, next)) = maybe_tt {
                let matches = match &tt {
                    TokenTree::Group(..) => unreachable!(
                        "Unexpected group token found in MacroMatch, this should not be possible."
                    ),
                    TokenTree::Ident(i) => {
                        if let TokenTree::Ident(other_i) = &other_tt {
                            i.clone() == other_i.clone()
                        } else {
                            false
                        }
                    }
                    TokenTree::Literal(l) => {
                        if let TokenTree::Literal(other_l) = &other_tt {
                            // TODO: Is this okay? Does this work?
                            l.to_string() == other_l.to_string()
                        } else {
                            false
                        }
                    }
                    TokenTree::Punct(p) => {
                        if let TokenTree::Punct(other_p) = &other_tt {
                            other_p.as_char() == p.as_char()
                        } else {
                            false
                        }
                    }
                };

                if !matches {
                    return Err(Error::new(
                        cursor.span(),
                        format!("Error reading macro use: expected {other_tt:?}, got {tt:?}"),
                    ));
                }

                *cursor = next;
            } else {
                return Err(Error::new(
                    cursor.span(),
                    format!(
                        "Error reading macro use: Unexpected end of tokens, expected {other_tt:?}"
                    ),
                ));
            }
        }
        Ok(())
    }
}

#[derive(Debug)]
/// A token representing part of a macro's arguments.
/// Used for determining how to parse [`MacroUse`].
/// First constructed with [`MacroMatcher::parse`] inside of [`MacroDef::parse`]. Then, we compare with [`MacroUse::parse_macro_matcher`].
pub enum MacroMatch {
    /// A token, excluding $ or delimeters. See https://doc.rust-lang.org/reference/tokens.html#grammar-Token
    Tokens(TokenStream),
    /// A delimeter-separated vector of [`MacroMatch`].
    MacroMatcher(MacroMatcher),
    /// A $ident:MacroFragSpec pairing.
    Ident(MacroIdent),
    // TODO: $(MacroMatch+) MacroRepSep? MacroRepOp
}

macro_rules! accepted_tokens {
    ($lookahead:ident, $tokens:ident, $input:ident, [$($i:path),+], [$($p:ident),+]) => {
        $(
            if $lookahead.peek($i) {
                $input.parse::<$i>()?.to_tokens(&mut $tokens);
            }
        )*
        $(
            if $lookahead.peek(token::$p) {

                $input.parse::<token::$p>()?.to_tokens(&mut $tokens);
            }
        )*
    };
}

impl Parse for MacroMatch {
    fn parse(input: parse::ParseStream) -> syn::Result<Self> {
        let lookahead = input.lookahead1();
        let mut tokens = TokenStream::new();

        if lookahead.peek(Token![$]) {
            return Ok(MacroMatch::Ident(input.parse()?));
        } else if lookahead.peek(token::Brace)
            || lookahead.peek(token::Bracket)
            || lookahead.peek(token::Paren)
        {
            return Ok(MacroMatch::MacroMatcher(input.parse()?));
        }

        accepted_tokens!(
            lookahead,
            tokens,
            input,
            [syn::Ident, syn::Lit, syn::Lifetime],
            // Not including the Eq symbols (OrEq, AndEq), since they confuse the parser.
            [
                Eq, Lt, Ne, Ge, Gt, Not, Tilde, Plus, Minus, Star, Slash, Percent, Caret, And, Or,
                Shl, Shr, At, Dot, Comma, Semi, Colon, Pound, Question, Underscore
            ]
        );

        if !tokens.is_empty() {
            Ok(MacroMatch::Tokens(tokens))
        } else {
            Err(Error::new(
                input.span(),
                format!("Did not recognize token. {input:?}"),
            ))
        }
    }
}

#[derive(Debug)]
/// Represents any given macro definition's arguments, and information on how to parse them for use.
/// A MacroMatcher is a delimited list of [`MacroMatch`]es. When you call the macro `example!(...)`, the macro matcher is the parentheses delimited tokens: `(...)`.
/// Used to compare a [`MacroDef`] against a [`MacroUse`] when parsing arguments.
pub struct MacroMatcher {
    pub delim: proc_macro2::Delimiter,
    pub matches: Vec<MacroMatch>,
}

impl Parse for MacroMatcher {
    fn parse(input: syn::parse::ParseStream) -> syn::Result<Self> {
        let lookahead = input.lookahead1();

        let mut matches = Vec::new();

        let delim;
        let arm;

        if lookahead.peek(token::Paren) {
            delim = proc_macro2::Delimiter::Parenthesis;
            parenthesized!(arm in input);
        } else if lookahead.peek(token::Brace) {
            delim = proc_macro2::Delimiter::Brace;
            braced!(arm in input);
        } else if lookahead.peek(token::Bracket) {
            delim = proc_macro2::Delimiter::Bracket;
            bracketed!(arm in input);
        } else {
            return Err(Error::new(input.span(), "Expected {}, (), or []"));
        }

        while !arm.is_empty() {
            matches.push(arm.parse::<MacroMatch>()?);
        }

        Ok(Self { delim, matches })
    }
}

#[derive(Debug)]
#[non_exhaustive]
/// Struct for defining a macro (i.e., `macro_rules! example`)
pub struct MacroDef {
    pub matcher: MacroMatcher,
    pub body: TokenStream,
}

impl Parse for MacroDef {
    fn parse(input: syn::parse::ParseStream) -> syn::Result<Self> {
        // Read the matcher:
        let matcher = input.parse::<MacroMatcher>()?;

        let _arrow = input.parse::<Token![=>]>()?;

        // Now the expansion:
        let arm_body;
        braced!(arm_body in input);

        let body = arm_body.parse::<TokenStream>()?;

        let _semicolon = input.parse::<Token![;]>()?;

        if !input.is_empty() {
            return Err(syn::Error::new(
                input.span(),
                "Diplomat does not support macros of more than one arm.",
            ));
        }

        // We don't support any other rules, so we ignore them.

        Ok(Self { matcher, body })
    }
}

impl MacroDef {
    pub fn validate(input: ItemMacro) -> TokenStream {
        let r = input.mac.parse_body::<Self>();

        if let Err(e) = r {
            e.to_compile_error()
        } else {
            TokenStream::default()
        }
    }

    fn parse_group(matched: &MacroUse, inner: Cursor) -> TokenStream {
        let mut stream = TokenStream::new();

        let mut c = inner;
        while let Some((tt, next)) = c.token_tree() {
            match &tt {
                TokenTree::Punct(p) if p.as_char() == '$' => {
                    if let Some((tt, next)) = next.token_tree() {
                        if let TokenTree::Ident(i) = tt {
                            matched.args[&i].to_tokens(&mut stream);
                            c = next;
                        } else {
                            panic!("Expected ident next to $, got {tt:?}");
                        }
                    } else {
                        panic!("Expected token tree.");
                    }
                }
                TokenTree::Group(g) => {
                    let (inner, _, next) = c.group(g.delimiter()).unwrap();
                    let group =
                        proc_macro2::Group::new(g.delimiter(), Self::parse_group(matched, inner));
                    // Once we detect a group, we push it to the array for syn to evaluate.
                    stream.append(group);
                    c = next;
                }
                _ => {
                    stream.append(tt);
                    c = next
                }
            }
        }

        stream
    }

    fn evaluate_buf(&self, matched: MacroUse) -> TokenStream {
        let mut stream = TokenStream::new();

        let buf = TokenBuffer::new2(self.body.clone());
        let mut c = buf.begin();
        // Search until we find a token to replace:
        while let Some((tt, next)) = c.token_tree() {
            match &tt {
                TokenTree::Punct(punct) if punct.as_char() == '$' => {
                    if let Some((tt, next)) = next.token_tree() {
                        if let TokenTree::Ident(i) = tt {
                            matched.args[&i].to_tokens(&mut stream);
                            c = next;
                        } else {
                            panic!("Expected ident next to $, got {tt:?}");
                        }
                    } else {
                        panic!("Expected token tree.");
                    }
                }
                TokenTree::Group(g) => {
                    let (inner, _, next) = c.group(g.delimiter()).unwrap();
                    // We need to read inside of any groups to find and replace `$` idents.
                    let group =
                        proc_macro2::Group::new(g.delimiter(), Self::parse_group(&matched, inner));
                    stream.append(group);
                    c = next;
                }
                _ => {
                    stream.append(tt);
                    c = next
                }
            }
        }

        stream
    }

    fn evaluate<T: Parse + Debug>(&self, matched: TokenStream) -> Vec<T> {
        let macro_use = MacroUse::parse(self, matched).unwrap_or_else(|e| panic!("{}", e));
        let stream = self.evaluate_buf(macro_use);

        // Now we have a stream to read through. We read through the whole thing and assume each thing we read is a top level item.
        let maybe_list = syn::parse_str::<ItemList<T>>(&stream.to_string());
        if let Ok(i) = maybe_list {
            i.items
        } else {
            panic!("Macro expansion error: {:?}", maybe_list.unwrap_err());
        }
    }
}

#[derive(Debug)]
struct ItemList<T: Parse> {
    items: Vec<T>,
}

impl<T: Parse> Parse for ItemList<T> {
    fn parse(input: parse::ParseStream) -> syn::Result<Self> {
        let mut items = Vec::new();
        while !input.is_empty() {
            items.push(input.parse::<T>()?);
        }

        Ok(Self { items })
    }
}
