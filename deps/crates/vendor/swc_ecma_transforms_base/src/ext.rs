//! Do not use: This is not a public api and it can be changed without a version
//! bump.

use swc_ecma_ast::*;
use swc_ecma_utils::ExprExt;

pub trait ExprRefExt: ExprExt {
    fn as_ident(&self) -> Option<&Ident> {
        match self.as_expr() {
            Expr::Ident(ref i) => Some(i),
            _ => None,
        }
    }
}

impl<T> ExprRefExt for T where T: ExprExt {}

/// Do not use: This is not a public api and it can be changed without a version
/// bump.
pub trait AsOptExpr {
    fn as_expr(&self) -> Option<&Expr>;
    fn as_expr_mut(&mut self) -> Option<&mut Expr>;
}

impl AsOptExpr for Callee {
    fn as_expr(&self) -> Option<&Expr> {
        match self {
            Callee::Super(_) | Callee::Import(_) => None,
            Callee::Expr(e) => Some(e),
            #[cfg(swc_ast_unknown)]
            _ => None,
        }
    }

    fn as_expr_mut(&mut self) -> Option<&mut Expr> {
        match self {
            Callee::Super(_) | Callee::Import(_) => None,
            Callee::Expr(e) => Some(e),
            #[cfg(swc_ast_unknown)]
            _ => None,
        }
    }
}

impl<N> AsOptExpr for Option<N>
where
    N: AsOptExpr,
{
    fn as_expr(&self) -> Option<&Expr> {
        match self {
            Some(n) => n.as_expr(),
            None => None,
        }
    }

    fn as_expr_mut(&mut self) -> Option<&mut Expr> {
        match self {
            None => None,
            Some(n) => n.as_expr_mut(),
        }
    }
}
