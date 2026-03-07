use std::iter;

use swc_atoms::atom;
use swc_common::{util::take::Take, Span, Spanned, DUMMY_SP};
use swc_ecma_ast::*;

/// Extension methods for [Expr].
///
/// Note that many types implements `Into<Expr>` and you can do like
///
/// ```rust
/// use swc_ecma_utils::ExprFactory;
///
/// let _args = vec![0f64.as_arg()];
/// ```
///
/// to create literals. Almost all rust core types implements `Into<Expr>`.
#[allow(clippy::wrong_self_convention)]
pub trait ExprFactory: Into<Box<Expr>> {
    /// Creates an [ExprOrSpread] using the given [Expr].
    ///
    /// This is recommended way to create [ExprOrSpread].
    ///
    /// # Example
    ///
    /// ```rust
    /// use swc_common::DUMMY_SP;
    /// use swc_ecma_ast::*;
    /// use swc_ecma_utils::ExprFactory;
    ///
    /// let first = Lit::Num(Number {
    ///     span: DUMMY_SP,
    ///     value: 0.0,
    ///     raw: None,
    /// });
    /// let _args = vec![first.as_arg()];
    /// ```
    #[cfg_attr(not(debug_assertions), inline(always))]
    fn as_arg(self) -> ExprOrSpread {
        ExprOrSpread {
            expr: self.into(),
            spread: None,
        }
    }

    /// Creates an expression statement with `self`.
    #[cfg_attr(not(debug_assertions), inline(always))]
    fn into_stmt(self) -> Stmt {
        let expr = self.into();
        ExprStmt {
            span: expr.span(),
            expr,
        }
        .into()
    }

    /// Creates a statement whcih return `self`.
    #[cfg_attr(not(debug_assertions), inline(always))]
    fn into_return_stmt(self) -> ReturnStmt {
        ReturnStmt {
            span: DUMMY_SP,
            arg: Some(self.into()),
        }
    }

    #[cfg_attr(not(debug_assertions), inline(always))]
    fn as_callee(self) -> Callee {
        Callee::Expr(self.into())
    }

    #[cfg_attr(not(debug_assertions), inline(always))]
    fn as_iife(self) -> CallExpr {
        CallExpr {
            span: DUMMY_SP,
            callee: self.as_callee(),
            ..Default::default()
        }
    }

    /// create a ArrowExpr which return self
    /// - `(params) => $self`
    #[cfg_attr(not(debug_assertions), inline(always))]
    fn into_lazy_arrow(self, params: Vec<Pat>) -> ArrowExpr {
        ArrowExpr {
            params,
            body: Box::new(BlockStmtOrExpr::Expr(self.into())),
            ..Default::default()
        }
    }

    /// create a Function which return self
    /// - `function(params) { return $self; }`
    #[cfg_attr(not(debug_assertions), inline(always))]
    fn into_lazy_fn(self, params: Vec<Param>) -> Function {
        Function {
            params,
            decorators: Default::default(),
            span: DUMMY_SP,
            body: Some(BlockStmt {
                span: DUMMY_SP,
                stmts: vec![self.into_return_stmt().into()],
                ..Default::default()
            }),
            ..Default::default()
        }
    }

    #[cfg_attr(not(debug_assertions), inline(always))]
    fn into_lazy_auto(self, params: Vec<Pat>, support_arrow: bool) -> Expr {
        if support_arrow {
            self.into_lazy_arrow(params).into()
        } else {
            self.into_lazy_fn(params.into_iter().map(From::from).collect())
                .into_fn_expr(None)
                .into()
        }
    }

    /// create a var declartor using self as init
    /// - `var name = expr`
    #[cfg_attr(not(debug_assertions), inline(always))]
    fn into_var_decl(self, kind: VarDeclKind, name: Pat) -> VarDecl {
        let var_declarator = VarDeclarator {
            span: DUMMY_SP,
            name,
            init: Some(self.into()),
            definite: false,
        };

        VarDecl {
            kind,
            decls: vec![var_declarator],
            ..Default::default()
        }
    }

    #[cfg_attr(not(debug_assertions), inline(always))]
    fn into_new_expr(self, span: Span, args: Option<Vec<ExprOrSpread>>) -> NewExpr {
        NewExpr {
            span,
            callee: self.into(),
            args,
            ..Default::default()
        }
    }

    #[cfg_attr(not(debug_assertions), inline(always))]
    fn apply(self, span: Span, this: Box<Expr>, args: Vec<ExprOrSpread>) -> Expr {
        let apply = self.make_member(IdentName::new(atom!("apply"), span));

        CallExpr {
            span,
            callee: apply.as_callee(),
            args: iter::once(this.as_arg()).chain(args).collect(),

            ..Default::default()
        }
        .into()
    }

    #[cfg_attr(not(debug_assertions), inline(always))]
    fn call_fn(self, span: Span, args: Vec<ExprOrSpread>) -> Expr {
        CallExpr {
            span,
            args,
            callee: self
                .make_member(IdentName::new(atom!("call"), span))
                .as_callee(),
            ..Default::default()
        }
        .into()
    }

    #[cfg_attr(not(debug_assertions), inline(always))]
    fn as_call(self, span: Span, args: Vec<ExprOrSpread>) -> Expr {
        CallExpr {
            span,
            args,
            callee: self.as_callee(),
            ..Default::default()
        }
        .into()
    }

    #[cfg_attr(not(debug_assertions), inline(always))]
    fn as_fn_decl(self) -> Option<FnDecl> {
        match *self.into() {
            Expr::Fn(FnExpr {
                ident: Some(ident),
                function,
            }) => Some(FnDecl {
                ident,
                declare: false,
                function,
            }),
            _ => None,
        }
    }

    #[cfg_attr(not(debug_assertions), inline(always))]
    fn as_class_decl(self) -> Option<ClassDecl> {
        match *self.into() {
            Expr::Class(ClassExpr {
                ident: Some(ident),
                class,
            }) => Some(ClassDecl {
                ident,
                declare: false,
                class,
            }),
            _ => None,
        }
    }

    #[cfg_attr(not(debug_assertions), inline(always))]
    fn wrap_with_paren(self) -> Expr {
        let expr = self.into();
        let span = expr.span();
        ParenExpr { expr, span }.into()
    }

    /// Creates a binary expr `$self === `
    #[cfg_attr(not(debug_assertions), inline(always))]
    fn make_eq<T>(self, right: T) -> Expr
    where
        T: Into<Expr>,
    {
        self.make_bin(op!("==="), right)
    }

    /// Creates a binary expr `$self $op $rhs`
    #[cfg_attr(not(debug_assertions), inline(always))]
    fn make_bin<T>(self, op: BinaryOp, right: T) -> Expr
    where
        T: Into<Expr>,
    {
        let right = Box::new(right.into());

        BinExpr {
            span: DUMMY_SP,
            left: self.into(),
            op,
            right,
        }
        .into()
    }

    /// Creates a assign expr `$lhs $op $self`
    #[cfg_attr(not(debug_assertions), inline(always))]
    fn make_assign_to(self, op: AssignOp, left: AssignTarget) -> Expr {
        let right = self.into();

        AssignExpr {
            span: DUMMY_SP,
            left,
            op,
            right,
        }
        .into()
    }

    #[cfg_attr(not(debug_assertions), inline(always))]
    fn make_member(self, prop: IdentName) -> MemberExpr {
        MemberExpr {
            obj: self.into(),
            span: DUMMY_SP,
            prop: MemberProp::Ident(prop),
        }
    }

    #[cfg_attr(not(debug_assertions), inline(always))]
    fn computed_member<T>(self, prop: T) -> MemberExpr
    where
        T: Into<Box<Expr>>,
    {
        MemberExpr {
            obj: self.into(),
            span: DUMMY_SP,
            prop: MemberProp::Computed(ComputedPropName {
                span: DUMMY_SP,
                expr: prop.into(),
            }),
        }
    }
}

impl<T: Into<Box<Expr>>> ExprFactory for T {}

pub trait IntoIndirectCall
where
    Self: std::marker::Sized,
{
    type Item;
    fn into_indirect(self) -> Self::Item;
}

impl IntoIndirectCall for CallExpr {
    type Item = CallExpr;

    #[cfg_attr(not(debug_assertions), inline(always))]
    fn into_indirect(self) -> CallExpr {
        let callee = self.callee.into_indirect();

        CallExpr { callee, ..self }
    }
}

impl IntoIndirectCall for Callee {
    type Item = Callee;

    #[cfg_attr(not(debug_assertions), inline(always))]
    fn into_indirect(self) -> Callee {
        SeqExpr {
            span: DUMMY_SP,
            exprs: vec![0f64.into(), self.expect_expr()],
        }
        .as_callee()
    }
}

impl IntoIndirectCall for TaggedTpl {
    type Item = TaggedTpl;

    #[cfg_attr(not(debug_assertions), inline(always))]
    fn into_indirect(mut self) -> Self {
        Self {
            tag: Box::new(
                SeqExpr {
                    span: DUMMY_SP,
                    exprs: vec![0f64.into(), self.tag.take()],
                }
                .into(),
            ),
            ..self
        }
    }
}

pub trait FunctionFactory: Into<Box<Function>> {
    #[cfg_attr(not(debug_assertions), inline(always))]
    fn into_fn_expr(self, ident: Option<Ident>) -> FnExpr {
        FnExpr {
            ident,
            function: self.into(),
        }
    }

    #[cfg_attr(not(debug_assertions), inline(always))]
    fn into_fn_decl(self, ident: Ident) -> FnDecl {
        FnDecl {
            ident,
            declare: false,
            function: self.into(),
        }
    }

    #[cfg_attr(not(debug_assertions), inline(always))]
    fn into_method_prop(self, key: PropName) -> MethodProp {
        MethodProp {
            key,
            function: self.into(),
        }
    }
}

impl<T: Into<Box<Function>>> FunctionFactory for T {}
