use swc_common::Span;
use swc_ecma_ast::*;

pub trait IsSimpleParameterList {
    fn is_simple_parameter_list(&self) -> bool;
}

impl IsSimpleParameterList for Vec<Param> {
    fn is_simple_parameter_list(&self) -> bool {
        self.iter().all(|param| matches!(param.pat, Pat::Ident(_)))
    }
}

impl IsSimpleParameterList for Vec<Pat> {
    fn is_simple_parameter_list(&self) -> bool {
        self.iter().all(|pat| matches!(pat, Pat::Ident(_)))
    }
}

impl IsSimpleParameterList for Vec<ParamOrTsParamProp> {
    fn is_simple_parameter_list(&self) -> bool {
        self.iter().all(|param| {
            matches!(
                param,
                ParamOrTsParamProp::TsParamProp(..)
                    | ParamOrTsParamProp::Param(Param {
                        pat: Pat::Ident(_),
                        ..
                    })
            )
        })
    }
}

pub trait IsInvalidClassName {
    fn invalid_class_name(&self) -> Option<Span>;
}

impl IsInvalidClassName for Ident {
    fn invalid_class_name(&self) -> Option<Span> {
        match &*self.sym {
            "string" | "null" | "number" | "object" | "any" | "unknown" | "boolean" | "bigint"
            | "symbol" | "void" | "never" | "intrinsic" => Some(self.span),
            _ => None,
        }
    }
}
impl IsInvalidClassName for Option<Ident> {
    fn invalid_class_name(&self) -> Option<Span> {
        self.as_ref().and_then(|i| i.invalid_class_name())
    }
}

pub trait ExprExt {
    fn as_expr(&self) -> &Expr;

    /// "IsValidSimpleAssignmentTarget" from spec.
    fn is_valid_simple_assignment_target(&self, strict: bool) -> bool {
        match self.as_expr() {
            Expr::Ident(ident) => {
                if strict && ident.is_reserved_in_strict_bind() {
                    return false;
                }
                true
            }

            Expr::This(..)
            | Expr::Lit(..)
            | Expr::Array(..)
            | Expr::Object(..)
            | Expr::Fn(..)
            | Expr::Class(..)
            | Expr::Tpl(..)
            | Expr::TaggedTpl(..) => false,
            Expr::Paren(ParenExpr { expr, .. }) => expr.is_valid_simple_assignment_target(strict),

            Expr::Member(MemberExpr { obj, .. }) => match obj.as_ref() {
                Expr::Member(..) => obj.is_valid_simple_assignment_target(strict),
                Expr::OptChain(..) => false,
                _ => true,
            },

            Expr::SuperProp(..) => true,

            Expr::New(..) | Expr::Call(..) => false,
            // TODO: Spec only mentions `new.target`
            Expr::MetaProp(..) => false,

            Expr::Update(..) => false,

            Expr::Unary(..) | Expr::Await(..) => false,

            Expr::Bin(..) => false,

            Expr::Cond(..) => false,

            Expr::Yield(..) | Expr::Arrow(..) | Expr::Assign(..) => false,

            Expr::Seq(..) => false,

            Expr::OptChain(..) => false,

            // MemberExpression is valid assignment target
            Expr::PrivateName(..) => false,

            // jsx
            Expr::JSXMember(..)
            | Expr::JSXNamespacedName(..)
            | Expr::JSXEmpty(..)
            | Expr::JSXElement(..)
            | Expr::JSXFragment(..) => false,

            // typescript
            Expr::TsNonNull(TsNonNullExpr { ref expr, .. })
            | Expr::TsTypeAssertion(TsTypeAssertion { ref expr, .. })
            | Expr::TsAs(TsAsExpr { ref expr, .. })
            | Expr::TsInstantiation(TsInstantiation { ref expr, .. })
            | Expr::TsSatisfies(TsSatisfiesExpr { ref expr, .. }) => {
                expr.is_valid_simple_assignment_target(strict)
            }

            Expr::TsConstAssertion(..) => false,

            Expr::Invalid(..) => false,
            #[cfg(swc_ast_unknown)]
            _ => unreachable!(),
        }
    }
}

impl ExprExt for Box<Expr> {
    #[inline(always)]
    fn as_expr(&self) -> &Expr {
        self
    }
}
impl ExprExt for Expr {
    #[inline(always)]
    fn as_expr(&self) -> &Expr {
        self
    }
}
