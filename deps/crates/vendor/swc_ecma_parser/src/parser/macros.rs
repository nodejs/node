macro_rules! trace_cur {
    ($p:expr, $name:ident) => {{
        if cfg!(feature = "debug") {
            tracing::debug!("{}: {:?}", stringify!($name), $p.input.cur());
        }
    }};
}

macro_rules! syntax_error {
    ($p:expr, $err:expr) => {
        syntax_error!($p, $p.input().cur_span(), $err)
    };
    ($p:expr, $span:expr, $err:expr) => {{
        let err = $crate::error::Error::new($span, $err);
        {
            let cur = $p.input().cur();
            if cur == Token::Error {
                let error = $p.input_mut().expect_error_token_and_bump();
                $p.emit_error(error);
            }
        }
        if cfg!(feature = "debug") {
            tracing::error!(
                "Syntax error called from {}:{}:{}\nCurrent token = {:?}",
                file!(),
                line!(),
                column!(),
                $p.input().cur()
            );
        }
        return Err(err.into());
    }};
}

macro_rules! expect {
    ($p:expr, $t:expr) => {{
        if !$p.input_mut().eat($t) {
            let span = $p.input().cur_span();
            let cur = $p.input_mut().dump_cur();
            syntax_error!(
                $p,
                span,
                $crate::error::SyntaxError::Expected(format!("{:?}", $t), cur)
            )
        }
    }};
}

macro_rules! unexpected {
    ($p:expr, $expected:literal) => {{
        let got = $p.input_mut().dump_cur();
        syntax_error!(
            $p,
            $p.input().cur_span(),
            $crate::error::SyntaxError::Unexpected {
                got,
                expected: $expected
            }
        )
    }};
}

macro_rules! debug_tracing {
    ($p:expr, $name:tt) => {{
        #[cfg(feature = "debug")]
        {
            let _ = tracing::span!(
                tracing::Level::ERROR,
                $name,
                cur = tracing::field::debug(&$p.input.cur())
            )
            .entered();
        }
    }};
}

macro_rules! peek {
    ($p:expr) => {{
        debug_assert!(
            $p.input().cur() != Token::Eof,
            "parser should not call peek() without knowing current token.
Current token is {:?}",
            $p.input().cur(),
        );
        $p.input_mut().peek()
    }};
}

macro_rules! return_if_arrow {
    ($p:expr, $expr:expr) => {{
        // FIXME:
        //
        //

        // let is_cur = match $p.state.potential_arrow_start {
        //     Some(start) => $expr.span.lo() == start,
        //     None => false
        // };
        // if is_cur {
        if let Expr::Arrow { .. } = *$expr {
            return Ok($expr);
        }
        // }
    }};
}
