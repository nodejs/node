/// Creates a corresponding operator. This macro is used to make code more
/// readable.
///
/// This can used to represent [UnaryOp](crate::UnaryOp),
/// [BinaryOp](crate::BinaryOp), or [AssignOp](crate::AssignOp).
///
/// Actual type is determined by the input.
///
/// # Note about `+` and `-`
///
/// As this macro is intended to be used in anywhere, including pattern
/// bindings, this macro cannot use trick to determine type for `+` and `-`.
///
/// So we have to use some special syntax for `+` and `-`:
///
///  - If you need [UnaryOp](crate::UnaryOp), use `op!(unary, "+")` and
///    `op!(unary, "-")`.
///
///  - If you need [BinaryOp](crate::BinaryOp), use `op!(bin, "+")` and
///    `op!(bin, "-")`.
#[macro_export]
macro_rules! op {
    (unary,"-") => {
        $crate::UnaryOp::Minus
    };
    (unary,"+") => {
        $crate::UnaryOp::Plus
    };
    ("!") => {
        $crate::UnaryOp::Bang
    };
    ("~") => {
        $crate::UnaryOp::Tilde
    };
    ("typeof") => {
        $crate::UnaryOp::TypeOf
    };
    ("void") => {
        $crate::UnaryOp::Void
    };
    ("delete") => {
        $crate::UnaryOp::Delete
    };

    ("++") => {
        $crate::UpdateOp::PlusPlus
    };
    ("--") => {
        $crate::UpdateOp::MinusMinus
    };

    ("==") => {
        $crate::BinaryOp::EqEq
    };
    ("!=") => {
        $crate::BinaryOp::NotEq
    };
    ("===") => {
        $crate::BinaryOp::EqEqEq
    };
    ("!==") => {
        $crate::BinaryOp::NotEqEq
    };
    ("<") => {
        $crate::BinaryOp::Lt
    };
    ("<=") => {
        $crate::BinaryOp::LtEq
    };
    (">") => {
        $crate::BinaryOp::Gt
    };
    (">=") => {
        $crate::BinaryOp::GtEq
    };
    ("<<") => {
        $crate::BinaryOp::LShift
    };
    (">>") => {
        $crate::BinaryOp::RShift
    };
    (">>>") => {
        $crate::BinaryOp::ZeroFillRShift
    };
    (bin,"+") => {
        $crate::BinaryOp::Add
    };
    (bin,"-") => {
        $crate::BinaryOp::Sub
    };
    ("*") => {
        $crate::BinaryOp::Mul
    };
    ("/") => {
        $crate::BinaryOp::Div
    };
    ("%") => {
        $crate::BinaryOp::Mod
    };
    ("|") => {
        $crate::BinaryOp::BitOr
    };
    ("^") => {
        $crate::BinaryOp::BitXor
    };
    ("&") => {
        $crate::BinaryOp::BitAnd
    };
    ("||") => {
        $crate::BinaryOp::LogicalOr
    };
    ("&&") => {
        $crate::BinaryOp::LogicalAnd
    };
    ("in") => {
        $crate::BinaryOp::In
    };
    ("instanceof") => {
        $crate::BinaryOp::InstanceOf
    };
    ("**") => {
        $crate::BinaryOp::Exp
    };
    ("??") => {
        $crate::BinaryOp::NullishCoalescing
    };

    ("=") => {
        $crate::AssignOp::Assign
    };
    ("+=") => {
        $crate::AssignOp::AddAssign
    };
    ("-=") => {
        $crate::AssignOp::SubAssign
    };
    ("*=") => {
        $crate::AssignOp::MulAssign
    };
    ("/=") => {
        $crate::AssignOp::DivAssign
    };
    ("%=") => {
        $crate::AssignOp::ModAssign
    };
    ("<<=") => {
        $crate::AssignOp::LShiftAssign
    };
    (">>=") => {
        $crate::AssignOp::RShiftAssign
    };
    (">>>=") => {
        $crate::AssignOp::ZeroFillRShiftAssign
    };
    ("|=") => {
        $crate::AssignOp::BitOrAssign
    };
    ("^=") => {
        $crate::AssignOp::BitXorAssign
    };
    ("&=") => {
        $crate::AssignOp::BitAndAssign
    };
    ("**=") => {
        $crate::AssignOp::ExpAssign
    };
    ("&&=") => {
        $crate::AssignOp::AndAssign
    };
    ("||=") => {
        $crate::AssignOp::OrAssign
    };
    ("??=") => {
        $crate::AssignOp::NullishAssign
    };
}

macro_rules! test_de {
    ($name:ident, $T:path, $s:literal) => {
        #[test]
        #[cfg(feature = "serde-impl")]
        fn $name() {
            let _var: $T = ::serde_json::from_str(&$s).expect("failed to parse json");
        }
    };
}

macro_rules! boxed {
    ($T:ty, [$($variant_ty:ty),*]) => {
        $(
            bridge_from!($T, Box<$variant_ty>, $variant_ty);
        )*
    };
}

/// Implement `From<$src>` for `$dst`, by using implementation of `From<$src>`
/// for `$bridge`.
///
/// - `$dst` must implement `From<$bridge>`.
/// - `$bridge` must implement `From<$src>`.
///
///
/// e.g. For `&str` -> `Box<Expr>`, we convert it by `&str` -> `Atom` -> `Str`
/// -> `Lit` -> `Expr` -> `Box<Expr>`.
macro_rules! bridge_from {
    ($dst:ty, $bridge:ty, $src:ty) => {
        impl From<$src> for $dst {
            #[cfg_attr(not(debug_assertions), inline(always))]
            fn from(src: $src) -> $dst {
                let src: $bridge = src.into();
                src.into()
            }
        }
    };
}

macro_rules! bridge_expr_from {
    ($bridge:ty, $src:ty) => {
        bridge_from!(crate::Expr, $bridge, $src);
        bridge_from!(Box<crate::Expr>, crate::Expr, $src);
    };
}

macro_rules! bridge_pat_from {
    ($bridge:ty, $src:ty) => {
        bridge_from!(crate::Pat, $bridge, $src);
        bridge_from!(crate::Param, crate::Pat, $src);
        bridge_from!(Box<crate::Pat>, crate::Pat, $src);
    };
}
