use swc_ecma_ast::*;

pub trait EndsWithAlphaNum {
    fn ends_with_alpha_num(&self) -> bool;
}

impl EndsWithAlphaNum for ForHead {
    fn ends_with_alpha_num(&self) -> bool {
        match self {
            ForHead::VarDecl(n) => n.ends_with_alpha_num(),
            ForHead::Pat(n) => n.ends_with_alpha_num(),
            ForHead::UsingDecl(n) => n.ends_with_alpha_num(),
            #[cfg(swc_ast_unknown)]
            _ => false,
        }
    }
}

impl EndsWithAlphaNum for Pat {
    fn ends_with_alpha_num(&self) -> bool {
        match self {
            Pat::Object(_) | Pat::Array(_) => false,
            Pat::Rest(p) => p.arg.ends_with_alpha_num(),
            Pat::Assign(p) => p.right.ends_with_alpha_num(),
            Pat::Expr(p) => p.ends_with_alpha_num(),
            _ => true,
        }
    }
}

impl EndsWithAlphaNum for VarDecl {
    fn ends_with_alpha_num(&self) -> bool {
        match self.decls.last() {
            None => true,
            Some(d) => match d.init.as_deref() {
                Some(e) => e.ends_with_alpha_num(),
                None => d.name.ends_with_alpha_num(),
            },
        }
    }
}

impl EndsWithAlphaNum for UsingDecl {
    fn ends_with_alpha_num(&self) -> bool {
        match self.decls.last() {
            None => true,
            Some(d) => match d.init.as_deref() {
                Some(e) => e.ends_with_alpha_num(),
                None => d.name.ends_with_alpha_num(),
            },
        }
    }
}

impl EndsWithAlphaNum for Expr {
    fn ends_with_alpha_num(&self) -> bool {
        match self {
            Expr::Array(..)
            | Expr::Object(..)
            | Expr::Lit(Lit::Str(..))
            | Expr::Paren(..)
            | Expr::Member(MemberExpr {
                prop: MemberProp::Computed(..),
                ..
            })
            | Expr::Tpl(..)
            | Expr::TaggedTpl(..)
            | Expr::Call(..) => false,

            Expr::Unary(n) => n.arg.ends_with_alpha_num(),

            Expr::Update(n) => n.prefix && n.arg.ends_with_alpha_num(),

            Expr::OptChain(n) => match n.base.as_ref() {
                OptChainBase::Member(base) => !base.prop.is_computed(),
                OptChainBase::Call(_) => false,
                #[cfg(swc_ast_unknown)]
                _ => false,
            },

            Expr::Bin(n) => n.right.ends_with_alpha_num(),

            Expr::New(NewExpr {
                args: Some(args), ..
            }) => args.is_empty(),

            _ => true,
        }
    }
}

/// Leftmost recursion
pub trait StartsWithAlphaNum {
    fn starts_with_alpha_num(&self) -> bool;
}

macro_rules! alpha_num_enum {
    ($T:ty, [$($name:ident),*]) => {
        impl StartsWithAlphaNum for $T {
            #[inline]
            fn starts_with_alpha_num(&self) -> bool {
                match *self {
                    $(
                        Self::$name(ref n) => n.starts_with_alpha_num(),
                    )*
                    #[cfg(swc_ast_unknown)]
                    _ => false,
                }
            }
        }
    };
}

macro_rules! alpha_num_const {
    ($value:tt, $($ty:ident),*) => {
        $(
            impl StartsWithAlphaNum for $ty {
                #[inline]
                fn starts_with_alpha_num(&self) -> bool {
                    $value
                }
            }
        )*
    };
}

alpha_num_const!(true, BindingIdent, Ident, SuperPropExpr, TsTypeAssertion);
alpha_num_const!(false, ArrayPat, ObjectPat, Invalid, ParenExpr);

impl StartsWithAlphaNum for MemberExpr {
    #[inline]
    fn starts_with_alpha_num(&self) -> bool {
        self.obj.starts_with_alpha_num()
    }
}

impl StartsWithAlphaNum for TsAsExpr {
    #[inline]
    fn starts_with_alpha_num(&self) -> bool {
        self.expr.starts_with_alpha_num()
    }
}

impl StartsWithAlphaNum for TsSatisfiesExpr {
    #[inline]
    fn starts_with_alpha_num(&self) -> bool {
        self.expr.starts_with_alpha_num()
    }
}

impl StartsWithAlphaNum for TsNonNullExpr {
    #[inline]
    fn starts_with_alpha_num(&self) -> bool {
        self.expr.starts_with_alpha_num()
    }
}

impl StartsWithAlphaNum for TsInstantiation {
    #[inline]
    fn starts_with_alpha_num(&self) -> bool {
        self.expr.starts_with_alpha_num()
    }
}

impl StartsWithAlphaNum for PropName {
    #[inline]
    fn starts_with_alpha_num(&self) -> bool {
        match self {
            PropName::Str(_) | PropName::Computed(_) => false,
            PropName::Ident(_) | PropName::Num(_) | PropName::BigInt(_) => true,
            #[cfg(swc_ast_unknown)]
            _ => false,
        }
    }
}

impl StartsWithAlphaNum for Expr {
    fn starts_with_alpha_num(&self) -> bool {
        match self {
            Expr::Ident(_)
            | Expr::Lit(Lit::Bool(_))
            | Expr::Lit(Lit::Num(_))
            | Expr::Lit(Lit::Null(_))
            | Expr::Lit(Lit::BigInt(_))
            | Expr::Await(_)
            | Expr::Fn(_)
            | Expr::Class(_)
            | Expr::This(_)
            | Expr::Yield(_)
            | Expr::New(_)
            | Expr::MetaProp(_)
            | Expr::SuperProp(_) => true,

            Expr::PrivateName(_) => false,

            // Handle other literals.
            Expr::Lit(_) => false,

            Expr::Seq(SeqExpr { ref exprs, .. }) => exprs
                .first()
                .map(|e| e.starts_with_alpha_num())
                .unwrap_or(false),

            //
            Expr::Assign(AssignExpr { ref left, .. }) => left.starts_with_alpha_num(),

            Expr::Bin(BinExpr { ref left, .. }) | Expr::Cond(CondExpr { test: ref left, .. }) => {
                left.starts_with_alpha_num()
            }
            Expr::Call(CallExpr { callee: left, .. }) => left.starts_with_alpha_num(),
            Expr::Member(MemberExpr { obj: ref left, .. }) => left.starts_with_alpha_num(),

            Expr::Unary(UnaryExpr { op, .. }) => {
                matches!(op, op!("void") | op!("delete") | op!("typeof"))
            }

            Expr::Arrow(ref expr) => {
                if expr.is_async {
                    true
                } else {
                    match expr.params.as_slice() {
                        [p] => p.starts_with_alpha_num(),
                        _ => false,
                    }
                }
            }

            Expr::Update(ref expr) => {
                if expr.prefix {
                    false
                } else {
                    expr.arg.starts_with_alpha_num()
                }
            }

            Expr::Tpl(_) | Expr::Array(_) | Expr::Object(_) | Expr::Paren(_) => false,

            Expr::TaggedTpl(TaggedTpl { ref tag, .. }) => tag.starts_with_alpha_num(),

            // it's empty
            Expr::JSXEmpty(..) => false,
            // start with `<`
            Expr::JSXFragment(..) | Expr::JSXElement(..) => false,
            Expr::JSXNamespacedName(..) => true,
            Expr::JSXMember(..) => true,

            Expr::TsTypeAssertion(..) => false,
            Expr::TsNonNull(TsNonNullExpr { ref expr, .. })
            | Expr::TsAs(TsAsExpr { ref expr, .. })
            | Expr::TsConstAssertion(TsConstAssertion { ref expr, .. })
            | Expr::TsInstantiation(TsInstantiation { ref expr, .. })
            | Expr::TsSatisfies(TsSatisfiesExpr { ref expr, .. }) => expr.starts_with_alpha_num(),

            Expr::OptChain(e) => e.starts_with_alpha_num(),

            Expr::Invalid(..) => true,
            #[cfg(swc_ast_unknown)]
            _ => false,
        }
    }
}

impl StartsWithAlphaNum for OptChainExpr {
    fn starts_with_alpha_num(&self) -> bool {
        match &*self.base {
            OptChainBase::Member(base) => base.obj.starts_with_alpha_num(),
            OptChainBase::Call(base) => base.callee.starts_with_alpha_num(),
            #[cfg(swc_ast_unknown)]
            _ => false,
        }
    }
}

impl StartsWithAlphaNum for Pat {
    fn starts_with_alpha_num(&self) -> bool {
        match *self {
            Pat::Ident(..) => true,
            Pat::Assign(AssignPat { ref left, .. }) => left.starts_with_alpha_num(),
            Pat::Object(..) | Pat::Array(..) | Pat::Rest(..) => false,
            Pat::Expr(ref expr) => expr.starts_with_alpha_num(),
            Pat::Invalid(..) => true,
            #[cfg(swc_ast_unknown)]
            _ => false,
        }
    }
}

alpha_num_enum!(AssignTarget, [Pat, Simple]);
alpha_num_enum!(
    SimpleAssignTarget,
    [
        Ident,
        Member,
        SuperProp,
        OptChain,
        Paren,
        TsAs,
        TsSatisfies,
        TsNonNull,
        TsTypeAssertion,
        TsInstantiation,
        Invalid
    ]
);
alpha_num_enum!(AssignTargetPat, [Array, Object, Invalid]);

impl StartsWithAlphaNum for ExprOrSpread {
    fn starts_with_alpha_num(&self) -> bool {
        match *self {
            ExprOrSpread {
                spread: Some(_), ..
            } => false,
            ExprOrSpread {
                spread: None,
                ref expr,
            } => expr.starts_with_alpha_num(),
        }
    }
}
impl StartsWithAlphaNum for Callee {
    fn starts_with_alpha_num(&self) -> bool {
        match *self {
            Callee::Super(_) | Callee::Import(_) => true,
            Callee::Expr(ref e) => e.starts_with_alpha_num(),
            #[cfg(swc_ast_unknown)]
            _ => false,
        }
    }
}
impl StartsWithAlphaNum for Stmt {
    fn starts_with_alpha_num(&self) -> bool {
        match *self {
            Stmt::Expr(ref expr) => expr.expr.starts_with_alpha_num(),
            Stmt::Decl(ref decl) => decl.starts_with_alpha_num(),
            Stmt::Debugger(..)
            | Stmt::With(..)
            | Stmt::While(..)
            | Stmt::DoWhile(..)
            | Stmt::Return(..)
            | Stmt::Labeled(..)
            | Stmt::Break(..)
            | Stmt::Continue(..)
            | Stmt::Switch(..)
            | Stmt::Throw(..)
            | Stmt::Try(..)
            | Stmt::For(..)
            | Stmt::ForIn(..)
            | Stmt::ForOf(..)
            | Stmt::If(..) => true,
            Stmt::Block(..) | Stmt::Empty(..) => false,
            #[cfg(swc_ast_unknown)]
            _ => false,
        }
    }
}

impl StartsWithAlphaNum for Decl {
    fn starts_with_alpha_num(&self) -> bool {
        match *self {
            Decl::Class(..)
            | Decl::Fn(..)
            | Decl::Var(..)
            | Decl::TsEnum(..)
            | Decl::TsInterface(..)
            | Decl::TsModule(..)
            | Decl::TsTypeAlias(..)
            | Decl::Using(..) => true,
            #[cfg(swc_ast_unknown)]
            _ => false,
        }
    }
}
