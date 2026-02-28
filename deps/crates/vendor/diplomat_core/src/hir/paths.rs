use super::lifetimes::{Lifetimes, LinkedLifetimes};
use super::{
    Borrow, EnumDef, EnumId, Everywhere, Mutability, OpaqueDef, OpaqueId, OpaqueOwner,
    OutStructDef, OutputOnly, ReturnableStructDef, StructDef, TraitId, TyPosition, TypeContext,
};

/// Path to a struct that may appear as an output.
#[derive(Debug, Clone)]
#[non_exhaustive]
pub enum ReturnableStructPath {
    Struct(StructPath),
    OutStruct(OutStructPath),
}

/// Path to a struct that can only be used as an output.
pub type OutStructPath = StructPath<OutputOnly>;

/// Path to a struct that can be used in inputs and outputs.
#[derive(Debug, Clone)]
#[non_exhaustive]
pub struct StructPath<P: TyPosition = Everywhere> {
    pub lifetimes: Lifetimes,
    pub tcx_id: P::StructId,
    pub owner: MaybeOwn,
}

#[derive(Debug, Clone)]
#[non_exhaustive]
pub struct TraitPath {
    pub lifetimes: Lifetimes,
    pub tcx_id: TraitId,
}

/// Non-instantiable enum to denote the trait path in
/// TyPositions that don't allow traits (anything not InputOnly)
#[derive(Debug, Clone)]
#[non_exhaustive]
pub enum NoTraitPath {}

/// Path to an opaque.
///
/// There are three kinds of opaques that Diplomat uses, so this type has two
/// generic arguments to differentiate between the three, while still showing
/// that the three are all paths to opaques. The monomorphized versions that
/// Diplomat uses are:
///
/// 1. `OpaquePath<Optional, MaybeOwn>`: Opaques in return types,
///    which can be optional and either owned or borrowed.
/// 2. `OpaquePath<Optional, Borrow>`: Opaques in method parameters, which can
///    be optional but must be borrowed, since most languages don't have a way to
///    entirely give up ownership of a value.
/// 3. `OpaquePath<NonOptional, Borrow>`: Opaques in the `&self` position, which
///    cannot be optional and must be borrowed for the same reason as above.
#[derive(Debug, Clone)]
#[non_exhaustive]
pub struct OpaquePath<Opt, Owner> {
    pub lifetimes: Lifetimes,
    pub optional: Opt,
    pub owner: Owner,
    pub tcx_id: OpaqueId,
}

#[derive(Debug, Copy, Clone)]
pub struct Optional(pub(super) bool);

#[derive(Debug, Copy, Clone)]
#[allow(clippy::exhaustive_structs)] // marker type
pub struct NonOptional;

impl<Owner: OpaqueOwner> OpaquePath<Optional, Owner> {
    pub fn is_optional(&self) -> bool {
        self.optional.0
    }

    pub fn is_owned(&self) -> bool {
        self.owner.is_owned()
    }
}

impl<Owner: OpaqueOwner> OpaquePath<NonOptional, Owner> {
    pub fn wrap_optional(self) -> OpaquePath<Optional, Owner> {
        OpaquePath {
            lifetimes: self.lifetimes,
            optional: Optional(false),
            owner: self.owner,
            tcx_id: self.tcx_id,
        }
    }
}

impl<Opt> OpaquePath<Opt, MaybeOwn> {
    pub fn as_borrowed(&self) -> Option<&Borrow> {
        self.owner.as_borrowed()
    }
}

impl<Opt> OpaquePath<Opt, Borrow> {
    pub fn borrowed(&self) -> &Borrow {
        &self.owner
    }
}

/// Path to an enum.
#[derive(Debug, Clone)]
#[non_exhaustive]
pub struct EnumPath {
    pub tcx_id: EnumId,
}

/// Determine whether a type is owned or borrowed.
///
/// Ownership in the case of opaques is `Box<Opaque>`, in the case of structs is
/// `Struct`, and in the case of slices is `Box<[T]>`.
#[derive(Copy, Clone, Debug)]
#[allow(clippy::exhaustive_enums)] // only two answers to this question
pub enum MaybeOwn {
    Own,
    Borrow(Borrow),
}

impl MaybeOwn {
    pub fn as_borrowed(&self) -> Option<&Borrow> {
        match self {
            MaybeOwn::Own => None,
            MaybeOwn::Borrow(borrow) => Some(borrow),
        }
    }

    pub fn is_owned(&self) -> bool {
        matches!(*self, Self::Own)
    }

    /// Returns the mutability of this potential-borrow
    ///
    /// Owned types are mutable
    pub fn mutability(&self) -> Mutability {
        match *self {
            Self::Own => Mutability::Mutable,
            Self::Borrow(b) => b.mutability,
        }
    }
}

impl From<Option<Borrow>> for MaybeOwn {
    fn from(other: Option<Borrow>) -> Self {
        other.map(Self::Borrow).unwrap_or(Self::Own)
    }
}
impl ReturnableStructPath {
    pub fn resolve<'tcx>(&self, tcx: &'tcx TypeContext) -> ReturnableStructDef<'tcx> {
        match self {
            ReturnableStructPath::Struct(path) => ReturnableStructDef::Struct(path.resolve(tcx)),
            ReturnableStructPath::OutStruct(path) => {
                ReturnableStructDef::OutStruct(path.resolve(tcx))
            }
        }
    }

    pub(crate) fn lifetimes(&self) -> &Lifetimes {
        match self {
            Self::Struct(p) => &p.lifetimes,
            Self::OutStruct(p) => &p.lifetimes,
        }
    }
}

impl<P: TyPosition> StructPath<P> {
    /// Returns a new [`EnumPath`].
    pub(super) fn new(lifetimes: Lifetimes, tcx_id: P::StructId, owner: MaybeOwn) -> Self {
        Self {
            lifetimes,
            tcx_id,
            owner,
        }
    }
}
impl StructPath {
    /// Returns the [`StructDef`] that this path references.
    pub fn resolve<'tcx>(&self, tcx: &'tcx TypeContext) -> &'tcx StructDef {
        tcx.resolve_struct(self.tcx_id)
    }
}

impl OutStructPath {
    /// Returns the [`OutStructDef`] that this path references.
    pub fn resolve<'tcx>(&self, tcx: &'tcx TypeContext) -> &'tcx OutStructDef {
        tcx.resolve_out_struct(self.tcx_id)
    }

    /// Get a map of lifetimes used on this path to lifetimes as named in the def site. See [`LinkedLifetimes`]
    /// for more information.
    pub fn link_lifetimes<'def, 'tcx>(
        &'def self,
        tcx: &'tcx TypeContext,
    ) -> LinkedLifetimes<'def, 'tcx> {
        let struc = self.resolve(tcx);
        let env = &struc.lifetimes;
        LinkedLifetimes::new(env, None, &self.lifetimes)
    }
}

impl<Opt, Owner> OpaquePath<Opt, Owner> {
    /// Returns a new [`OpaquePath`].
    pub(super) fn new(lifetimes: Lifetimes, optional: Opt, owner: Owner, tcx_id: OpaqueId) -> Self {
        Self {
            lifetimes,
            optional,
            owner,
            tcx_id,
        }
    }

    /// Returns the [`OpaqueDef`] that this path references.
    pub fn resolve<'tcx>(&self, tcx: &'tcx TypeContext) -> &'tcx OpaqueDef {
        tcx.resolve_opaque(self.tcx_id)
    }
}

impl<Opt, Owner: OpaqueOwner> OpaquePath<Opt, Owner> {
    /// Get a map of lifetimes used on this path to lifetimes as named in the def site. See [`LinkedLifetimes`]
    /// for more information.
    pub fn link_lifetimes<'def, 'tcx>(
        &'def self,
        tcx: &'tcx TypeContext,
    ) -> LinkedLifetimes<'def, 'tcx> {
        let opaque = self.resolve(tcx);
        let env = &opaque.lifetimes;
        LinkedLifetimes::new(env, self.owner.lifetime(), &self.lifetimes)
    }
}

impl EnumPath {
    /// Returns a new [`EnumPath`].
    pub(super) fn new(tcx_id: EnumId) -> Self {
        Self { tcx_id }
    }

    /// Returns the [`EnumDef`] that this path references.
    pub fn resolve<'tcx>(&self, tcx: &'tcx TypeContext) -> &'tcx EnumDef {
        tcx.resolve_enum(self.tcx_id)
    }
}

impl TraitPath {
    /// Returns a new [`TraitPath`].
    pub(super) fn new(lifetimes: Lifetimes, tcx_id: TraitId) -> Self {
        Self { lifetimes, tcx_id }
    }
}
