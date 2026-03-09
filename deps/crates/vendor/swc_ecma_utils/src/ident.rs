use swc_atoms::Atom;
use swc_common::SyntaxContext;
use swc_ecma_ast::{unsafe_id_from_ident, BindingIdent, Id, Ident, UnsafeId};

pub trait IdentLike: Sized + Send + Sync + 'static {
    fn from_ident(i: &Ident) -> Self;
    fn to_id(&self) -> Id;
    fn into_id(self) -> Id;
}

impl IdentLike for Atom {
    fn from_ident(i: &Ident) -> Self {
        i.sym.clone()
    }

    fn to_id(&self) -> Id {
        (self.clone(), Default::default())
    }

    fn into_id(self) -> Id {
        (self, Default::default())
    }
}

impl IdentLike for BindingIdent {
    fn from_ident(i: &Ident) -> Self {
        i.clone().into()
    }

    fn to_id(&self) -> Id {
        (self.sym.clone(), self.ctxt)
    }

    fn into_id(self) -> Id {
        self.id.into_id()
    }
}

impl IdentLike for (Atom, SyntaxContext) {
    #[inline]
    fn from_ident(i: &Ident) -> Self {
        (i.sym.clone(), i.ctxt)
    }

    #[inline]
    fn to_id(&self) -> Id {
        (self.0.clone(), self.1)
    }

    #[inline]
    fn into_id(self) -> Id {
        self
    }
}

impl IdentLike for Ident {
    #[inline]
    fn from_ident(i: &Ident) -> Self {
        Ident::new(i.sym.clone(), i.span, i.ctxt)
    }

    #[inline]
    fn to_id(&self) -> Id {
        (self.sym.clone(), self.ctxt)
    }

    #[inline]
    fn into_id(self) -> Id {
        (self.sym, self.ctxt)
    }
}

impl IdentLike for UnsafeId {
    fn from_ident(i: &Ident) -> Self {
        unsafe { unsafe_id_from_ident(i) }
    }

    fn to_id(&self) -> Id {
        unreachable!("UnsafeId.to_id() is not allowed because it is very likely to be unsafe")
    }

    fn into_id(self) -> Id {
        unreachable!("UnsafeId.into_id() is not allowed because it is very likely to be unsafe")
    }
}
