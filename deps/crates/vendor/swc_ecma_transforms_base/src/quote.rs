/// Not a public api.
#[doc(hidden)]
#[macro_export]
macro_rules! helper_expr {
    (ts, $field_name:ident) => {{
        $crate::helper_expr!(ts, ::swc_common::DUMMY_SP, $field_name)
    }};

    (ts, $span:expr, $field_name:ident) => {{
        use swc_ecma_utils::{quote_ident, ExprFactory};

        let mark = $crate::enable_helper!($field_name);
        let ctxt = swc_common::SyntaxContext::empty().apply_mark(mark);

        Expr::from(swc_ecma_utils::quote_ident!(
            ctxt,
            $span,
            concat!("_", stringify!($field_name))
        ))
    }};

    ($field_name:ident) => {{
        $crate::helper_expr!(::swc_common::DUMMY_SP, $field_name)
    }};

    ($span:expr, $field_name:ident) => {{
        use swc_ecma_utils::{quote_ident, ExprFactory};

        let mark = $crate::enable_helper!($field_name);
        let ctxt = swc_common::SyntaxContext::empty().apply_mark(mark);

        Expr::from(swc_ecma_utils::quote_ident!(
            ctxt,
            $span,
            concat!("_", stringify!($field_name))
        ))
    }};
}

/// Not a public api.
#[doc(hidden)]
#[macro_export]
macro_rules! helper {
    ($($t:tt)*) => {{
        use swc_ecma_utils::ExprFactory;
        $crate::helper_expr!($($t)*).as_callee()
    }};
}
