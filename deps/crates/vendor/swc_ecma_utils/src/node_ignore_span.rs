use std::{borrow::Cow, fmt::Debug, hash::Hash};

use swc_common::EqIgnoreSpan;
use swc_ecma_ast::{Expr, MemberProp};

/// A newtype that will ignore Span while doing `eq` or `hash`.
pub struct NodeIgnoringSpan<'a, Node: ToOwned + Debug>(Cow<'a, Node>);

impl<Node: ToOwned + Debug> Debug for NodeIgnoringSpan<'_, Node> {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        f.debug_tuple("NodeIgnoringSpan").field(&*self.0).finish()
    }
}

impl<'a, Node: ToOwned + Debug> NodeIgnoringSpan<'a, Node> {
    #[inline]
    pub fn borrowed(expr: &'a Node) -> Self {
        Self(Cow::Borrowed(expr))
    }

    #[inline]
    pub fn owned(expr: <Node as ToOwned>::Owned) -> Self {
        Self(Cow::Owned(expr))
    }
}

impl<Node: EqIgnoreSpan + ToOwned + Debug> PartialEq for NodeIgnoringSpan<'_, Node> {
    fn eq(&self, other: &Self) -> bool {
        self.0.eq_ignore_span(&other.0)
    }
}

impl<Node: EqIgnoreSpan + ToOwned + Debug> Eq for NodeIgnoringSpan<'_, Node> {}

// TODO: This is only a workaround for Expr. we need something like
// `hash_ignore_span` for each node in the end.
impl Hash for NodeIgnoringSpan<'_, Expr> {
    fn hash<H: std::hash::Hasher>(&self, state: &mut H) {
        // In pratice, most of cases/input we are dealing with are Expr::Member or
        // Expr::Ident.
        match &*self.0 {
            Expr::Ident(i) => {
                i.sym.hash(state);
            }
            Expr::Member(i) => {
                {
                    NodeIgnoringSpan::borrowed(i.obj.as_ref()).hash(state);
                }
                if let MemberProp::Ident(prop) = &i.prop {
                    prop.sym.hash(state);
                }
            }
            _ => {
                // Other expression kinds would fallback to the same empty hash.
                // So, they will spend linear time to do comparisons.
            }
        }
    }
}
