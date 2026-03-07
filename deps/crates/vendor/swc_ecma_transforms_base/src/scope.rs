use swc_ecma_ast::VarDeclKind;

#[derive(Debug, Clone, Copy, PartialEq, Eq, Default)]
pub enum ScopeKind {
    Block,
    #[default]
    Fn,
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum IdentType {
    Binding,
    Ref,
    Label,
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum DeclKind {
    Lexical,
    Param,
    Var,
    Function,
    /// don't actually get stored
    Type,
}

impl From<VarDeclKind> for DeclKind {
    fn from(kind: VarDeclKind) -> Self {
        match kind {
            VarDeclKind::Const | VarDeclKind::Let => Self::Lexical,
            VarDeclKind::Var => Self::Var,
            #[cfg(swc_ast_unknown)]
            _ => panic!("unable to access unknown nodes"),
        }
    }
}
