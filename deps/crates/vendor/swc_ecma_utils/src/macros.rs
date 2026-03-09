/// Shortcut for `quote_ident!(span.apply_mark(Mark::fresh(Mark::root())), s)`
#[macro_export]
macro_rules! private_ident {
    ($s:literal) => {
        private_ident!($crate::swc_atoms::atom!($s))
    };
    ($s:expr) => {
        private_ident!($crate::swc_common::DUMMY_SP, $s)
    };
    ($span:expr, $s:expr) => {{
        let mark = $crate::swc_common::Mark::new();
        let ctxt = $crate::swc_common::SyntaxContext::empty().apply_mark(mark);
        $crate::swc_ecma_ast::Ident::new($s.into(), $span, ctxt)
    }};
}

/// As we have multiple identifier types, the expected usage is
/// `quote_ident!("foo").into()`.
#[macro_export]
macro_rules! quote_ident {
    ($s:literal) => {{
        quote_ident!($crate::swc_atoms::atom!($s))
    }};
    ($s:expr) => {{
        let sym: $crate::swc_atoms::Atom = $s.into();
        let id: $crate::swc_ecma_ast::IdentName = sym.into();

        id
    }};
    ($ctxt:expr, $s:literal) => {{
        quote_ident!($ctxt, $crate::swc_atoms::atom!($s))
    }};
    ($ctxt:expr, $s:expr) => {{
        let sym: $crate::swc_atoms::Atom = $s.into();
        let id: $crate::swc_ecma_ast::Ident =
            $crate::swc_ecma_ast::Ident::new(sym, $crate::swc_common::DUMMY_SP, $ctxt);

        id
    }};

    ($ctxt:expr, $span:expr, $s:expr) => {{
        $crate::swc_ecma_ast::Ident::new($s.into(), $span, $ctxt)
    }};
}

#[macro_export]
macro_rules! quote_str {
    ($s:literal) => {
        quote_str!($crate::swc_atoms::atom!($s))
    };
    ($s:expr) => {
        quote_str!($crate::swc_common::DUMMY_SP, $s)
    };
    ($span:expr, $s:expr) => {{
        $crate::swc_ecma_ast::Str {
            span: $span,
            raw: None,
            value: $s.into(),
        }
    }};
}

#[macro_export]
macro_rules! quote_expr {
    ($span:expr, null) => {{
        use $crate::swc_ecma_ast::*;
        Expr::Lit(Lit::Null(Null { span: $span }))
    }};

    ($span:expr, undefined) => {{
        box Expr::Ident(Ident::new("undefined", $span))
    }};
}

/// Creates a member expression.
///
/// # Usage
/// ```rust,ignore
/// member_expr!(span, Function.bind.apply);
/// ```
///
/// Returns Box<[Expr](swc_ecma_ast::Expr)>.
#[macro_export]
macro_rules! member_expr {
    ($ctxt:expr, $span:expr, $first:ident) => {{
        use $crate::swc_ecma_ast::Expr;
        Box::new(Expr::Ident($crate::quote_ident!($ctxt, $span, stringify!($first))))
    }};

    ($ctxt:expr, $span:expr, $first:ident . $($rest:tt)+) => {{
        let obj = member_expr!($ctxt, $span, $first);

        member_expr!(@EXT, $span, obj, $($rest)* )
    }};

    (@EXT, $span:expr, $obj:expr, $first:ident . $($rest:tt)* ) => {{
        use $crate::swc_ecma_ast::MemberProp;
        use $crate::swc_ecma_ast::IdentName;
        let prop = MemberProp::Ident(IdentName::new(stringify!($first).into(), $span));

        member_expr!(@EXT, $span, Box::new(Expr::Member(MemberExpr{
            span: $crate::swc_common::DUMMY_SP,
            obj: $obj,
            prop,
        })), $($rest)*)
    }};

    (@EXT, $span:expr, $obj:expr,  $first:ident) => {{
        use $crate::swc_ecma_ast::*;
        let prop = MemberProp::Ident(IdentName::new(stringify!($first).into(), $span));

        MemberExpr{
            span: $crate::swc_common::DUMMY_SP,
            obj: $obj,
            prop,
        }
    }};
}

#[cfg(test)]
mod tests {
    use swc_atoms::atom;
    use swc_common::DUMMY_SP as span;
    use swc_ecma_ast::*;

    use crate::drop_span;

    #[test]
    fn quote_member_expr() {
        let expr: Box<Expr> = drop_span(member_expr!(
            Default::default(),
            Default::default(),
            Function.prototype.bind
        ))
        .into();

        assert_eq!(
            expr,
            Box::new(Expr::Member(MemberExpr {
                span,
                obj: Box::new(Expr::Member(MemberExpr {
                    span,
                    obj: member_expr!(Default::default(), Default::default(), Function),
                    prop: MemberProp::Ident(atom!("prototype").into()),
                })),
                prop: MemberProp::Ident(atom!("bind").into()),
            }))
        );
    }
}
