use super::lifetimes::{Lifetime, Lifetimes, MaybeStatic};
use super::{
    Borrow, Callback, CallbackInstantiationFunctionality, LinkedLifetimes, MaybeOwn, Mutability,
    NoCallback, NoTraitPath, OutStructId, ReturnableStructPath, StructDef, StructId, StructPath,
    TraitId, TraitPath, TypeContext, TypeDef, TypeId,
};
use core::fmt::Debug;

/// Abstraction over where a type can appear in a function signature.
///
/// # "Output only" and "everywhere" types
///
/// While Rust is able to give up ownership of values, languages that Diplomat
/// supports (C++, Javascript, etc.) generally cannot. For example, we can
/// construct a `Box<MyOpaque>` in a Rust function and _return_ it to the other
/// language as a pointer. However, we cannot _accept_ `Box<MyOpaque>` as an input
/// because there's nothing stopping other languages from using that value again.
/// Therefore, we classify boxed opaques as "output only" types, since they can
/// only be returned from Rust but not taken as inputs.
///
/// Furthermore, Diplomat also supports "bag o' stuff" structs where all fields get
/// translated at the boundary. If one contains an "output only" type as a field,
/// then the whole struct must also be "output only". In particular, this means
/// that if a boxed opaque is nested in a bunch of "bag o' stuff" structs, than
/// all of those structs must also be "output only".
///
/// Currently, there are only two classes of structs: those that are "output only",
/// and those that are not. These are represented by the types [`OutputOnly`]
/// and [`Everywhere`] marker types respectively, which are the _only_ two types
/// that implement [`TyPosition`].
///
/// # How does abstraction help?
///
/// The HIR was designed around the idea of making invalid states unrepresentable.
/// Since "output only" types can contain values that "everywhere" types cannot,
/// it doesn't make sense for them to be represented in the same type, even if
/// they're mostly the same. One of these differences is that opaques (which are
/// always behind a pointer) can only be represented as a borrow in "everywhere"
/// types, but can additionally be represented as owned in "output only" types.
/// If we were to use the same type for both, then backends working with "everywhere"
/// types would constantly have unreachable statements for owned opaque cases.
///
/// That being said, "output only" and "everywhere" types are still mostly the
/// same, so this trait allows us to describe the differences. For example, the
/// HIR uses a singular [`Type`](super::Type) type for representing both
/// "output only" types and "everywhere" types, since it takes advantage of this
/// traits associated types to "fill in" the different parts:
/// ```ignore
/// pub enum Type<P: TyPosition = Everywhere> {
///     Primitive(PrimitiveType),
///     Opaque(OpaquePath<Optional, P::OpaqueOwnership>),
///     Struct(P::StructPath),
///     Enum(EnumPath),
///     Slice(Slice),
/// }
/// ```
///
/// When `P` takes on [`Everywhere`], this signature becomes:
/// ```ignore
/// pub enum Type {
///     Primitive(PrimitiveType),
///     Opaque(OpaquePath<Optional, Borrow>),
///     Struct(StructPath),
///     Enum(EnumPath),
///     Slice(Slice),
/// }
/// ```
///
/// This allows us to represent any kind of type that can appear "everywhere"
/// i.e. in inputs or outputs. Notice how the second generic in the `Opaque`
/// variant becomes [`Borrow`]. This describes the semantics of the pointer that
/// the opaque lives behind, and shows that for "everywhere" types, opaques
/// can _only_ be represented as living behind a borrow.
///
/// Contrast this to when `P` takes on [`OutputOnly`]:
/// ```ignore
/// pub enum Type {
///     Primitive(PrimitiveType),
///     Opaque(OpaquePath<Optional, MaybeOwn>),
///     Struct(OutStructPath),
///     Enum(EnumPath),
///     Slice(Slice),
/// }
/// ```
/// Here, the second generic of the `Opaque` variant becomes [`MaybeOwn`], meaning
/// that "output only" types can contain opaques that are either borrowed _or_ owned.
///
/// Therefore, this trait allows be extremely precise about making invalid states
/// unrepresentable, while also reducing duplicated code.
///
pub trait TyPosition: Debug + Copy
where
    for<'tcx> TypeDef<'tcx>: From<&'tcx StructDef<Self>>,
{
    const IN_OUT_STATUS: InputOrOutput;
    type CallbackInstantiation: Debug + CallbackInstantiationFunctionality + Clone;

    /// Type representing how we can point to opaques, which must always be behind a pointer.
    ///
    /// The types represented by [`OutputOnly`] are capable of either owning or
    /// borrowing opaques, and so the associated type for that impl is [`MaybeOwn`].
    ///
    /// On the other hand, types represented by [`Everywhere`] can only contain
    /// borrowes, so the associated type for that impl is [`Borrow`].
    type OpaqueOwnership: Debug + OpaqueOwner + Clone;

    type StructId: Debug + Clone;

    type StructPath: Debug + StructPathLike + Clone;

    type TraitPath: Debug + TraitIdGetter + Clone;

    fn wrap_struct_def<'tcx>(def: &'tcx StructDef<Self>) -> TypeDef<'tcx>;
    fn build_callback(cb: Callback) -> Self::CallbackInstantiation;
    fn build_trait_path(trait_path: TraitPath) -> Self::TraitPath;
}

/// Directionality of the type
#[non_exhaustive]
pub enum InputOrOutput {
    Input,
    Output,
    InputOutput,
}

pub trait TraitIdGetter {
    fn id(&self) -> TraitId;
}

/// One of 3 types implementing [`TyPosition`], representing types that can be
/// used as both input and output to functions.
///
/// The restricted versions of this type are [`OutputOnly`] and [`InputOnly`].
#[derive(Debug, Copy, Clone)]
#[non_exhaustive]
pub struct Everywhere;

/// One of 3 types implementing [`TyPosition`], representing types that can
/// only be used as return types in functions.
///
/// The directional opposite of this type is [`InputOnly`].
#[derive(Debug, Copy, Clone)]
#[non_exhaustive]
pub struct OutputOnly;

/// One of 3 types implementing [`TyPosition`], representing types that can
/// only be used as input types in functions.
///
/// The directional opposite of this type is [`OutputOnly`].
#[derive(Debug, Copy, Clone)]
#[non_exhaustive]
pub struct InputOnly;

impl TyPosition for Everywhere {
    const IN_OUT_STATUS: InputOrOutput = InputOrOutput::InputOutput;
    type OpaqueOwnership = Borrow;
    type StructId = StructId;
    type StructPath = StructPath;
    type CallbackInstantiation = NoCallback;
    type TraitPath = NoTraitPath;

    fn wrap_struct_def<'tcx>(def: &'tcx StructDef<Self>) -> TypeDef<'tcx> {
        TypeDef::Struct(def)
    }
    fn build_callback(_cb: Callback) -> Self::CallbackInstantiation {
        panic!("Callbacks must be input-only");
    }
    fn build_trait_path(_trait_path: TraitPath) -> Self::TraitPath {
        panic!("Traits must be input-only");
    }
}

impl TyPosition for OutputOnly {
    const IN_OUT_STATUS: InputOrOutput = InputOrOutput::Output;
    type OpaqueOwnership = MaybeOwn;
    type StructId = OutStructId;
    type StructPath = ReturnableStructPath;
    type CallbackInstantiation = NoCallback;
    type TraitPath = NoTraitPath;

    fn wrap_struct_def<'tcx>(def: &'tcx StructDef<Self>) -> TypeDef<'tcx> {
        TypeDef::OutStruct(def)
    }
    fn build_callback(_cb: Callback) -> Self::CallbackInstantiation {
        panic!("Callbacks must be input-only");
    }
    fn build_trait_path(_trait_path: TraitPath) -> Self::TraitPath {
        panic!("Traits must be input-only");
    }
}

impl TyPosition for InputOnly {
    const IN_OUT_STATUS: InputOrOutput = InputOrOutput::Input;
    type OpaqueOwnership = Borrow;
    type StructId = StructId;
    type StructPath = StructPath;
    type CallbackInstantiation = Callback;
    type TraitPath = TraitPath;

    fn wrap_struct_def<'tcx>(_def: &'tcx StructDef<Self>) -> TypeDef<'tcx> {
        panic!("Input-only structs are not currently supported");
    }
    fn build_callback(cb: Callback) -> Self::CallbackInstantiation {
        cb
    }
    fn build_trait_path(trait_path: TraitPath) -> Self::TraitPath {
        trait_path
    }
}

pub trait StructPathLike {
    fn lifetimes(&self) -> &Lifetimes;
    fn id(&self) -> TypeId;
    fn owner(&self) -> MaybeOwn;

    /// Get a map of lifetimes used on this path to lifetimes as named in the def site. See [`LinkedLifetimes`]
    /// for more information.
    fn link_lifetimes<'def, 'tcx>(
        &'def self,
        tcx: &'tcx TypeContext,
    ) -> LinkedLifetimes<'def, 'tcx>;
}

impl StructPathLike for StructPath {
    fn lifetimes(&self) -> &Lifetimes {
        &self.lifetimes
    }
    fn id(&self) -> TypeId {
        self.tcx_id.into()
    }

    fn owner(&self) -> MaybeOwn {
        self.owner
    }

    fn link_lifetimes<'def, 'tcx>(
        &'def self,
        tcx: &'tcx TypeContext,
    ) -> LinkedLifetimes<'def, 'tcx> {
        let struc = self.resolve(tcx);
        let env = &struc.lifetimes;
        LinkedLifetimes::new(env, None, &self.lifetimes)
    }
}

impl StructPathLike for ReturnableStructPath {
    fn lifetimes(&self) -> &Lifetimes {
        self.lifetimes()
    }
    fn id(&self) -> TypeId {
        match self {
            ReturnableStructPath::Struct(p) => p.tcx_id.into(),
            ReturnableStructPath::OutStruct(p) => p.tcx_id.into(),
        }
    }

    fn owner(&self) -> MaybeOwn {
        MaybeOwn::Own
    }

    fn link_lifetimes<'def, 'tcx>(
        &'def self,
        tcx: &'tcx TypeContext,
    ) -> LinkedLifetimes<'def, 'tcx> {
        match self {
            Self::Struct(p) => p.link_lifetimes(tcx),
            Self::OutStruct(p) => p.link_lifetimes(tcx),
        }
    }
}

impl TraitIdGetter for TraitPath {
    fn id(&self) -> TraitId {
        self.tcx_id
    }
}

impl TraitIdGetter for NoTraitPath {
    fn id(&self) -> TraitId {
        panic!("Trait path not allowed here, no trait ID valid");
    }
}

/// Abstraction over how a type can hold a pointer to an opaque.
///
/// This trait is designed as a helper abstraction for the `OpaqueOwnership`
/// associated type in the [`TyPosition`] trait. As such, only has two implementing
/// types: [`MaybeOwn`] and [`Borrow`] for the [`OutputOnly`] and [`Everywhere`]
/// implementations of [`TyPosition`] respectively.
pub trait OpaqueOwner {
    /// Return the mutability of this owner
    fn mutability(&self) -> Option<Mutability>;

    fn is_owned(&self) -> bool;

    /// Return the lifetime of the borrow, if any.
    fn lifetime(&self) -> Option<MaybeStatic<Lifetime>>;
}

impl OpaqueOwner for MaybeOwn {
    fn mutability(&self) -> Option<Mutability> {
        match self {
            MaybeOwn::Own => None,
            MaybeOwn::Borrow(b) => b.mutability(),
        }
    }

    fn is_owned(&self) -> bool {
        match self {
            MaybeOwn::Own => true,
            MaybeOwn::Borrow(_) => false,
        }
    }

    fn lifetime(&self) -> Option<MaybeStatic<Lifetime>> {
        match self {
            MaybeOwn::Own => None,
            MaybeOwn::Borrow(b) => b.lifetime(),
        }
    }
}

impl OpaqueOwner for Borrow {
    fn mutability(&self) -> Option<Mutability> {
        Some(self.mutability)
    }

    fn is_owned(&self) -> bool {
        false
    }

    fn lifetime(&self) -> Option<MaybeStatic<Lifetime>> {
        Some(self.lifetime)
    }
}
