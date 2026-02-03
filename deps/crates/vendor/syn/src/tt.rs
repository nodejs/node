use proc_macro2::{Delimiter, Spacing, TokenStream, TokenTree};
use std::hash::{Hash, Hasher};

pub(crate) struct TokenTreeHelper<'a>(pub &'a TokenTree);

impl<'a> PartialEq for TokenTreeHelper<'a> {
    fn eq(&self, other: &Self) -> bool {
        match (self.0, other.0) {
            (TokenTree::Group(g1), TokenTree::Group(g2)) => {
                match (g1.delimiter(), g2.delimiter()) {
                    (Delimiter::Parenthesis, Delimiter::Parenthesis)
                    | (Delimiter::Brace, Delimiter::Brace)
                    | (Delimiter::Bracket, Delimiter::Bracket)
                    | (Delimiter::None, Delimiter::None) => {}
                    _ => return false,
                }

                TokenStreamHelper(&g1.stream()) == TokenStreamHelper(&g2.stream())
            }
            (TokenTree::Punct(o1), TokenTree::Punct(o2)) => {
                o1.as_char() == o2.as_char()
                    && match (o1.spacing(), o2.spacing()) {
                        (Spacing::Alone, Spacing::Alone) | (Spacing::Joint, Spacing::Joint) => true,
                        _ => false,
                    }
            }
            (TokenTree::Literal(l1), TokenTree::Literal(l2)) => l1.to_string() == l2.to_string(),
            (TokenTree::Ident(s1), TokenTree::Ident(s2)) => s1 == s2,
            _ => false,
        }
    }
}

impl<'a> Hash for TokenTreeHelper<'a> {
    fn hash<H: Hasher>(&self, h: &mut H) {
        match self.0 {
            TokenTree::Group(g) => {
                0u8.hash(h);
                match g.delimiter() {
                    Delimiter::Parenthesis => 0u8.hash(h),
                    Delimiter::Brace => 1u8.hash(h),
                    Delimiter::Bracket => 2u8.hash(h),
                    Delimiter::None => 3u8.hash(h),
                }

                for item in g.stream() {
                    TokenTreeHelper(&item).hash(h);
                }
                0xFFu8.hash(h); // terminator w/ a variant we don't normally hash
            }
            TokenTree::Punct(op) => {
                1u8.hash(h);
                op.as_char().hash(h);
                match op.spacing() {
                    Spacing::Alone => 0u8.hash(h),
                    Spacing::Joint => 1u8.hash(h),
                }
            }
            TokenTree::Literal(lit) => (2u8, lit.to_string()).hash(h),
            TokenTree::Ident(word) => (3u8, word).hash(h),
        }
    }
}

pub(crate) struct TokenStreamHelper<'a>(pub &'a TokenStream);

impl<'a> PartialEq for TokenStreamHelper<'a> {
    fn eq(&self, other: &Self) -> bool {
        let left = self.0.clone().into_iter();
        let mut right = other.0.clone().into_iter();

        for item1 in left {
            let item2 = match right.next() {
                Some(item) => item,
                None => return false,
            };
            if TokenTreeHelper(&item1) != TokenTreeHelper(&item2) {
                return false;
            }
        }

        right.next().is_none()
    }
}

impl<'a> Hash for TokenStreamHelper<'a> {
    fn hash<H: Hasher>(&self, state: &mut H) {
        let tokens = self.0.clone().into_iter();

        tokens.clone().count().hash(state);

        for tt in tokens {
            TokenTreeHelper(&tt).hash(state);
        }
    }
}
