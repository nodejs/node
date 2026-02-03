//! Type definitions for structs, output structs, opaque structs, and enums.

use super::lifetimes::LifetimeEnv;
use super::{
    Attrs, Callback, Everywhere, IdentBuf, Method, OutputOnly, SpecialMethodPresence, TyPosition,
    Type,
};
use crate::ast::Docs;

#[non_exhaustive]
pub enum ReturnableStructDef<'tcx> {
    Struct(&'tcx StructDef),
    OutStruct(&'tcx OutStructDef),
}

#[derive(Copy, Clone, Debug)]
#[non_exhaustive]
pub enum TypeDef<'tcx> {
    Struct(&'tcx StructDef),
    OutStruct(&'tcx OutStructDef),
    Opaque(&'tcx OpaqueDef),
    Enum(&'tcx EnumDef),
}

#[derive(Debug)]
#[non_exhaustive]
pub struct TraitDef {
    // TyPosition: InputOnly
    pub docs: Docs,
    pub name: IdentBuf,
    pub methods: Vec<Callback>,
    pub attrs: Attrs,
    pub lifetimes: LifetimeEnv,
}

/// Structs that can only be returned from methods.
pub type OutStructDef = StructDef<OutputOnly>;

/// Structs that can be either inputs or outputs in methods.
#[derive(Debug)]
#[non_exhaustive]
pub struct StructDef<P: TyPosition = Everywhere> {
    pub docs: Docs,
    pub name: IdentBuf,
    pub fields: Vec<StructField<P>>,
    pub methods: Vec<Method>,
    pub attrs: Attrs,
    pub lifetimes: LifetimeEnv,
    pub special_method_presence: SpecialMethodPresence,
}

/// A struct whose contents are opaque across the FFI boundary, and can only
/// cross when behind a pointer.
///
/// All opaques can be inputs or outputs when behind a reference, but owned
/// opaques can only be returned since there isn't a general way for most languages
/// to give up ownership.
///
/// A struct marked with `#[diplomat::opaque]`.
#[derive(Debug)]
#[non_exhaustive]
pub struct OpaqueDef {
    pub docs: Docs,
    pub name: IdentBuf,
    pub methods: Vec<Method>,
    pub attrs: Attrs,
    pub lifetimes: LifetimeEnv,
    pub special_method_presence: SpecialMethodPresence,

    /// The ABI name of the generated destructor
    pub dtor_abi_name: IdentBuf,
}

/// The enum type.
#[derive(Debug)]
#[non_exhaustive]
pub struct EnumDef {
    pub docs: Docs,
    pub name: IdentBuf,
    pub variants: Vec<EnumVariant>,
    pub methods: Vec<Method>,
    pub attrs: Attrs,
    pub special_method_presence: SpecialMethodPresence,
}

/// A field on a [`OutStruct`]s.
pub type OutStructField = StructField<OutputOnly>;

/// A field on a [`Struct`]s.
#[derive(Debug)]
#[non_exhaustive]
pub struct StructField<P: TyPosition = Everywhere> {
    pub docs: Docs,
    pub name: IdentBuf,
    pub ty: Type<P>,
    pub attrs: Attrs,
}

/// A variant of an [`Enum`].
#[derive(Debug)]
#[non_exhaustive]
pub struct EnumVariant {
    pub docs: Docs,
    pub name: IdentBuf,
    pub discriminant: isize,
    pub attrs: Attrs,
}

impl TraitDef {
    pub(super) fn new(
        docs: Docs,
        name: IdentBuf,
        methods: Vec<Callback>,
        attrs: Attrs,
        lifetimes: LifetimeEnv,
    ) -> Self {
        Self {
            docs,
            name,
            methods,
            attrs,
            lifetimes,
        }
    }
}

impl<P: TyPosition> StructDef<P> {
    pub(super) fn new(
        docs: Docs,
        name: IdentBuf,
        fields: Vec<StructField<P>>,
        methods: Vec<Method>,
        attrs: Attrs,
        lifetimes: LifetimeEnv,
        special_method_presence: SpecialMethodPresence,
    ) -> Self {
        Self {
            docs,
            name,
            fields,
            methods,
            attrs,
            lifetimes,
            special_method_presence,
        }
    }
}

impl OpaqueDef {
    pub(super) fn new(
        docs: Docs,
        name: IdentBuf,
        methods: Vec<Method>,
        attrs: Attrs,
        lifetimes: LifetimeEnv,
        special_method_presence: SpecialMethodPresence,
        dtor_abi_name: IdentBuf,
    ) -> Self {
        Self {
            docs,
            name,
            methods,
            attrs,
            lifetimes,
            special_method_presence,
            dtor_abi_name,
        }
    }
}

impl EnumDef {
    pub(super) fn new(
        docs: Docs,
        name: IdentBuf,
        variants: Vec<EnumVariant>,
        methods: Vec<Method>,
        attrs: Attrs,
        special_method_presence: SpecialMethodPresence,
    ) -> Self {
        Self {
            docs,
            name,
            variants,
            methods,
            attrs,
            special_method_presence,
        }
    }
}

impl<'a, P: TyPosition> From<&'a StructDef<P>> for TypeDef<'a> {
    fn from(x: &'a StructDef<P>) -> Self {
        P::wrap_struct_def(x)
    }
}

impl<'a> From<&'a OpaqueDef> for TypeDef<'a> {
    fn from(x: &'a OpaqueDef) -> Self {
        TypeDef::Opaque(x)
    }
}

impl<'a> From<&'a EnumDef> for TypeDef<'a> {
    fn from(x: &'a EnumDef) -> Self {
        TypeDef::Enum(x)
    }
}

impl<'tcx> TypeDef<'tcx> {
    pub fn name(&self) -> &'tcx IdentBuf {
        match *self {
            Self::Struct(ty) => &ty.name,
            Self::OutStruct(ty) => &ty.name,
            Self::Opaque(ty) => &ty.name,
            Self::Enum(ty) => &ty.name,
        }
    }

    pub fn docs(&self) -> &'tcx Docs {
        match *self {
            Self::Struct(ty) => &ty.docs,
            Self::OutStruct(ty) => &ty.docs,
            Self::Opaque(ty) => &ty.docs,
            Self::Enum(ty) => &ty.docs,
        }
    }
    pub fn methods(&self) -> &'tcx [Method] {
        match *self {
            Self::Struct(ty) => &ty.methods,
            Self::OutStruct(ty) => &ty.methods,
            Self::Opaque(ty) => &ty.methods,
            Self::Enum(ty) => &ty.methods,
        }
    }

    pub fn attrs(&self) -> &'tcx Attrs {
        match *self {
            Self::Struct(ty) => &ty.attrs,
            Self::OutStruct(ty) => &ty.attrs,
            Self::Opaque(ty) => &ty.attrs,
            Self::Enum(ty) => &ty.attrs,
        }
    }

    pub fn special_method_presence(&self) -> &'tcx SpecialMethodPresence {
        match *self {
            Self::Struct(ty) => &ty.special_method_presence,
            Self::OutStruct(ty) => &ty.special_method_presence,
            Self::Opaque(ty) => &ty.special_method_presence,
            Self::Enum(ty) => &ty.special_method_presence,
        }
    }
}
