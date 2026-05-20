//! Types that can be exposed in Diplomat APIs.

use super::lifetimes::{Lifetime, MaybeStatic};
use super::{
    EnumPath, Everywhere, NonOptional, OpaqueOwner, OpaquePath, Optional, OutputOnly,
    PrimitiveType, StructPath, StructPathLike, TyPosition, TypeContext, TypeId,
};
use crate::ast;
use crate::hir::MaybeOwn;
pub use ast::Mutability;
pub use ast::StringEncoding;
use either::Either;

/// Type that can only be used as an output.
pub type OutType = Type<OutputOnly>;

/// Type that may be used as input or output.
#[derive(Debug, Clone)]
#[non_exhaustive]
pub enum Type<P: TyPosition = Everywhere> {
    Primitive(PrimitiveType),
    Opaque(OpaquePath<Optional, P::OpaqueOwnership>),
    Struct(P::StructPath),
    ImplTrait(P::TraitPath),
    Enum(EnumPath),
    Slice(Slice<P>),
    Callback(P::CallbackInstantiation), // only a Callback if P == InputOnly
    /// `DiplomatOption<T>`, for  a primitive, struct, or enum `T`.
    ///
    /// In some cases this can be specified as `Option<T>`, but under the hood it gets translated to
    /// the DiplomatOption FFI-compatible union type.
    ///
    /// This is *different* from `Option<&T>` and `Option<Box<T>` for an opaque `T`: That is represented as
    /// a nullable pointer.
    ///
    /// This does not get used when the user writes `-> Option<T>` (for non-opaque T):
    /// that will always use [`ReturnType::Nullable`](crate::hir::ReturnType::Nullable).
    DiplomatOption(Box<Type<P>>),
}

/// Type that can appear in the `self` position.
#[derive(Debug, Clone)]
#[non_exhaustive]
pub enum SelfType {
    Opaque(OpaquePath<NonOptional, Borrow>),
    Struct(StructPath),
    Enum(EnumPath),
}

#[derive(Copy, Clone, Debug)]
#[non_exhaustive]
pub enum Slice<P: TyPosition> {
    /// A string slice, e.g. `&DiplomatStr` or `Box<DiplomatStr>`.
    ///
    /// Owned slices are useful for garbage-collected languages that have to
    /// reallocate into non-gc memory anyway. For example for Dart it's more
    /// efficient to accept `Box<str>` than to accept `&str` and then
    /// allocate in Rust, as Dart will have to create the `Box<str`> to
    /// pass `&str` anyway.
    Str(Option<MaybeStatic<Lifetime>>, StringEncoding),

    /// A primitive slice, e.g. `&mut [u8]` or `Box<[usize]>`.
    ///
    /// Owned slices are useful for garbage-collected languages that have to
    /// reallocate into non-gc memory anyway. For example for Dart it's more
    /// efficient to accept `Box<[bool]>` than to accept `&[bool]` and then
    /// allocate in Rust, as Dart will have to create the `Box<[bool]`> to
    /// pass `&[bool]` anyway.
    Primitive(MaybeOwn, PrimitiveType),

    /// A `&[DiplomatStrSlice]`. This type of slice always needs to be
    /// allocated before passing it into Rust, as it has to conform to the
    /// Rust ABI. In other languages this is the idiomatic list of string
    /// views, i.e. `std::span<std::string_view>` or `core.List<core.String>`.
    Strs(StringEncoding),

    /// A `&[Struct]`, where `Struct` is a structure that is only comprised of primitive types and
    /// structures that only contain primitive types. Must be marked with `#[diplomat::attr(auto, allowed_in_slices)]`.
    /// Currently assumes that `&[Struct]` is provided as an input only for function parameters.
    /// Validated in [`super::type_context::TypeContext::validate_primitive_slice_struct`]
    Struct(MaybeOwn, P::StructPath),
}

// For now, the lifetime in not optional. This is because when you have references
// as fields of structs, the lifetime must always be present, and we want to uphold
// this invariant at the type level within the HIR.
//
// The only time when a lifetime is optional in Rust code is in function signatures,
// where implicit lifetimes are allowed. Getting this to all fit together will
// involve getting the implicit lifetime thing to be understood by Diplomat, but
// should be doable.
#[derive(Copy, Clone, Debug)]
#[non_exhaustive]
pub struct Borrow {
    pub lifetime: MaybeStatic<Lifetime>,
    pub mutability: Mutability,
}

// This is implemented on InputOnly and Everywhere types. Could be extended
// if we genericize .resolve() later.
impl<P: TyPosition<StructPath = StructPath>> Type<P> {
    /// Return the number of fields and leaves that will show up in the [`LifetimeTree`]
    /// returned by [`Param::lifetime_tree`] and [`ParamSelf::lifetime_tree`].
    ///
    /// This method is used to calculate how much space to allocate upfront.
    pub(super) fn field_leaf_lifetime_counts(&self, tcx: &TypeContext) -> (usize, usize) {
        match self {
            Type::Struct(ty) => ty.resolve(tcx).fields.iter().fold((1, 0), |acc, field| {
                let inner = field.ty.field_leaf_lifetime_counts(tcx);
                (acc.0 + inner.0, acc.1 + inner.1)
            }),
            Type::Opaque(_) | Type::Slice(_) | Type::Callback(_) | Type::ImplTrait(_) => (1, 1),
            Type::Primitive(_) | Type::Enum(_) => (0, 0),
            Type::DiplomatOption(ty) => ty.field_leaf_lifetime_counts(tcx),
        }
    }
}

impl<P: TyPosition> Type<P> {
    /// Get all lifetimes "contained" in this type
    pub fn lifetimes(&self) -> impl Iterator<Item = MaybeStatic<Lifetime>> + '_ {
        match self {
            Type::Opaque(opaque) => Either::Right(
                opaque
                    .lifetimes
                    .as_slice()
                    .iter()
                    .copied()
                    .chain(opaque.owner.lifetime()),
            ),
            Type::Struct(struct_) => Either::Left(struct_.lifetimes().as_slice().iter().copied()),
            Type::Slice(slice) => Either::Left(
                slice
                    .lifetime()
                    .map(|lt| std::slice::from_ref(lt).iter().copied())
                    .unwrap_or([].iter().copied()),
            ),
            Type::DiplomatOption(ty) => ty.lifetimes(),
            // TODO the Callback case
            _ => Either::Left([].iter().copied()),
        }
    }

    // For custom types, get the type id
    pub fn id(&self) -> Option<TypeId> {
        Some(match self {
            Self::Opaque(p) => TypeId::Opaque(p.tcx_id),
            Self::Enum(p) => TypeId::Enum(p.tcx_id),
            Self::Struct(p) => p.id(),
            _ => return None,
        })
    }

    /// Unwrap to the inner type if `self` is `DiplomatOption`
    pub fn unwrap_option(&self) -> &Type<P> {
        match self {
            Self::DiplomatOption(ref o) => o,
            _ => self,
        }
    }

    /// Whether this type is a `DiplomatOption` or optional Opaque
    pub fn is_option(&self) -> bool {
        match self {
            Self::DiplomatOption(..) => true,
            Self::Opaque(ref o) if o.is_optional() => true,
            _ => false,
        }
    }
    /// Returns whether the self parameter is borrowed immutably.
    ///
    /// Curently this can only happen with opaque types.
    pub fn is_immutably_borrowed(&self) -> bool {
        matches!(self, Self::Opaque(opaque_path) if opaque_path.owner.mutability() == Some(Mutability::Immutable))
            || matches!(self, Self::Struct(st) if st.owner().mutability().is_immutable())
    }
    /// Returns whether the self parameter is borrowed mutably.
    ///
    /// Curently this can only happen with opaque types.
    pub fn is_mutably_borrowed(&self) -> bool {
        matches!(self, Self::Opaque(opaque_path) if opaque_path.owner.mutability() == Some(Mutability::Mutable))
            || matches!(self, Self::Struct(st) if st.owner().mutability().is_immutable())
    }
}

impl SelfType {
    /// Returns whether the self parameter is borrowed immutably.
    ///
    /// Curently this can only happen with opaque types.
    pub fn is_immutably_borrowed(&self) -> bool {
        matches!(self, SelfType::Opaque(opaque_path) if opaque_path.owner.mutability == Mutability::Immutable)
            || matches!(self, SelfType::Struct(st) if st.owner().mutability().is_immutable())
    }
    /// Returns whether the self parameter is borrowed mutably.
    ///
    /// Curently this can only happen with opaque types.
    pub fn is_mutably_borrowed(&self) -> bool {
        matches!(self, SelfType::Opaque(opaque_path) if opaque_path.owner.mutability == Mutability::Mutable)
            || matches!(self, SelfType::Struct(st) if st.owner().mutability().is_immutable())
    }
    /// Returns whether the self parameter is consuming.
    ///
    /// Currently this can only (and must) only happen for non-opaque types.
    pub fn is_consuming(&self) -> bool {
        matches!(self, SelfType::Enum(_))
            || matches!(self, SelfType::Struct(st) if st.owner().is_owned())
    }
}

impl<P: TyPosition> Slice<P> {
    /// Returns the [`Lifetime`] contained in either the `Str` or `Primitive`
    /// variant.
    pub fn lifetime(&self) -> Option<&MaybeStatic<Lifetime>> {
        match self {
            Slice::Str(lifetime, ..) => lifetime.as_ref(),
            Slice::Primitive(MaybeOwn::Borrow(reference), ..)
            | Slice::Struct(MaybeOwn::Borrow(reference), ..) => Some(&reference.lifetime),
            Slice::Primitive(..) | Slice::Struct(..) => None,
            Slice::Strs(..) => Some({
                const X: MaybeStatic<Lifetime> = MaybeStatic::NonStatic(Lifetime::new(usize::MAX));
                &X
            }),
        }
    }
}

impl Borrow {
    pub(super) fn new(lifetime: MaybeStatic<Lifetime>, mutability: Mutability) -> Self {
        Self {
            lifetime,
            mutability,
        }
    }
}

impl From<SelfType> for Type {
    fn from(s: SelfType) -> Type {
        match s {
            SelfType::Opaque(o) => Type::Opaque(o.wrap_optional()),
            SelfType::Struct(s) => Type::Struct(s),
            SelfType::Enum(e) => Type::Enum(e),
        }
    }
}
