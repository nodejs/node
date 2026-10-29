//! Tools for traversing all borrows in method parameters and return types, transitively
//!
//! This is useful for backends which wish to "splat out" lifetime edge codegen for methods,
//! linking each borrowed input param/field (however deep it may be in a struct) to a borrowed output param/field.

use std::fmt::{self, Write};

use smallvec::SmallVec;

use crate::hir::{
    paths, Borrow, Ident, Method, SelfType, Slice, StructPath, TyPosition, Type, TypeContext,
};

use crate::hir::lifetimes::{Lifetime, Lifetimes, MaybeStatic};

/// An id for indexing into a [`BorrowingFieldsVisitor`].
#[derive(Copy, Clone, Debug)]
struct ParentId(usize);

impl ParentId {
    /// Pushes a new parent to the vec, returning the corresponding [`ParentId`].
    fn new<'m>(
        parent: Option<ParentId>,
        name: &'m Ident,
        parents: &mut SmallVec<[(Option<ParentId>, &'m Ident); 4]>,
    ) -> Self {
        let this = ParentId(parents.len());
        parents.push((parent, name));
        this
    }
}

/// Convenience const representing the number of nested structs a [`BorrowingFieldVisitor`]
/// can hold inline before needing to dynamically allocate.
const INLINE_NUM_PARENTS: usize = 4;

/// Convenience const representing the number of borrowed fields a [`BorrowingFieldVisitor`]
/// can hold inline before needing to dynamically allocate.
const INLINE_NUM_LEAVES: usize = 8;

/// A tree of lifetimes mapping onto a specific instantiation of a type tree.
///
/// Each `BorrowingFieldsVisitor` corresponds to the type of an input of a method.
///
/// Obtain from [`Method::borrowing_field_visitor()`].
pub struct BorrowingFieldVisitor<'m> {
    parents: SmallVec<[(Option<ParentId>, &'m Ident); INLINE_NUM_PARENTS]>,
    leaves: SmallVec<[BorrowingFieldVisitorLeaf; INLINE_NUM_LEAVES]>,
}

/// A leaf of a lifetime tree capable of tracking its parents.
#[derive(Copy, Clone)]
pub struct BorrowingField<'m> {
    /// All inner nodes in the tree. When tracing from the root, we jump around
    /// this slice based on indices, but don't necessarily use all of them.
    parents: &'m [(Option<ParentId>, &'m Ident)],

    /// The unpacked field that is a leaf on the tree.
    leaf: &'m BorrowingFieldVisitorLeaf,
}

/// Non-recursive input-output types that contain lifetimes
enum BorrowingFieldVisitorLeaf {
    Opaque(ParentId, MaybeStatic<Lifetime>, Lifetimes),
    Slice(ParentId, MaybeStatic<Lifetime>),
}

impl<'m> BorrowingFieldVisitor<'m> {
    /// Visits every borrowing field and method lifetime that it uses.
    ///
    /// The idea is that you could use this to construct a mapping from
    /// `Lifetime`s to `BorrowingField`s. We choose to use a visitor
    /// pattern to avoid having to
    ///
    /// This would be convenient in the JavaScript backend where if you're
    /// returning an `NonOpaque<'a, 'b>` from Rust, you need to pass a
    /// `[[all input borrowing fields with 'a], [all input borrowing fields with 'b]]`
    /// array into `NonOpaque`'s constructor.
    ///
    /// Alternatively, you could use such a map in the C++ backend by recursing
    /// down the return type and keeping track of which fields you've recursed
    /// into so far, and when you hit some lifetime 'a, generate docs saying
    /// "path.to.current.field must be outlived by {borrowing fields of input that
    /// contain 'a}".
    pub fn visit_borrowing_fields<'a, F>(&'a self, mut visit: F)
    where
        F: FnMut(MaybeStatic<Lifetime>, BorrowingField<'a>),
    {
        for leaf in self.leaves.iter() {
            let borrowing_field = BorrowingField {
                parents: self.parents.as_slice(),
                leaf,
            };

            match leaf {
                BorrowingFieldVisitorLeaf::Opaque(_, lt, method_lifetimes) => {
                    visit(*lt, borrowing_field);
                    for lt in method_lifetimes.lifetimes() {
                        visit(lt, borrowing_field);
                    }
                }
                BorrowingFieldVisitorLeaf::Slice(_, lt) => {
                    visit(*lt, borrowing_field);
                }
            }
        }
    }

    /// Returns a new `BorrowingFieldsVisitor` containing all the lifetime trees of the arguments
    /// in only two allocations.
    pub(crate) fn new(method: &'m Method, tcx: &'m TypeContext, self_name: &'m Ident) -> Self {
        let (parents, leaves) = method
            .param_self
            .as_ref()
            .map(|param_self| param_self.field_leaf_lifetime_counts(tcx))
            .into_iter()
            .chain(
                method
                    .params
                    .iter()
                    .map(|param| param.ty.field_leaf_lifetime_counts(tcx)),
            )
            .reduce(|acc, x| (acc.0 + x.0, acc.1 + x.1))
            .map(|(num_fields, num_leaves)| {
                let num_params = method.params.len() + usize::from(method.param_self.is_some());
                let mut parents = SmallVec::with_capacity(num_fields + num_params);
                let mut leaves = SmallVec::with_capacity(num_leaves);
                let method_lifetimes = method.method_lifetimes();

                if let Some(param_self) = method.param_self.as_ref() {
                    let parent = ParentId::new(None, self_name, &mut parents);
                    match &param_self.ty {
                        SelfType::Opaque(ty) => {
                            Self::visit_opaque(
                                &ty.lifetimes,
                                &ty.borrowed().lifetime,
                                parent,
                                &method_lifetimes,
                                &mut leaves,
                            );
                        }
                        SelfType::Struct(ty) => {
                            Self::visit_struct(
                                ty,
                                tcx,
                                parent,
                                &method_lifetimes,
                                &mut parents,
                                &mut leaves,
                            );
                        }
                        SelfType::Enum(_) => {}
                    }
                }

                for param in method.params.iter() {
                    let parent = ParentId::new(None, param.name.as_ref(), &mut parents);
                    Self::from_type(
                        &param.ty,
                        tcx,
                        parent,
                        &method_lifetimes,
                        &mut parents,
                        &mut leaves,
                    );
                }

                // sanity check that the preallocations were correct
                debug_assert_eq!(
                    parents.capacity(),
                    std::cmp::max(INLINE_NUM_PARENTS, num_fields + num_params)
                );
                debug_assert_eq!(
                    leaves.capacity(),
                    std::cmp::max(INLINE_NUM_LEAVES, num_leaves)
                );
                (parents, leaves)
            })
            .unwrap_or_default();

        Self { parents, leaves }
    }

    /// Returns a new [`BorrowingFieldsVisitor`] corresponding to a type.
    fn from_type<P: TyPosition<StructPath = StructPath, OpaqueOwnership = Borrow>>(
        ty: &'m Type<P>,
        tcx: &'m TypeContext,
        parent: ParentId,
        method_lifetimes: &Lifetimes,
        parents: &mut SmallVec<[(Option<ParentId>, &'m Ident); 4]>,
        leaves: &mut SmallVec<[BorrowingFieldVisitorLeaf; 8]>,
    ) {
        match ty {
            Type::Opaque(path) => {
                Self::visit_opaque(
                    &path.lifetimes,
                    &path.borrowed().lifetime,
                    parent,
                    method_lifetimes,
                    leaves,
                );
            }
            Type::Slice(slice) => {
                Self::visit_slice(slice, parent, method_lifetimes, leaves);
            }
            Type::Struct(path) => {
                Self::visit_struct(path, tcx, parent, method_lifetimes, parents, leaves);
            }
            _ => {}
        }
    }

    /// Add an opaque as a leaf during construction of a [`BorrowingFieldsVisitor`].
    fn visit_opaque(
        lifetimes: &'m Lifetimes,
        borrow: &'m MaybeStatic<Lifetime>,
        parent: ParentId,
        method_lifetimes: &Lifetimes,
        leaves: &mut SmallVec<[BorrowingFieldVisitorLeaf; 8]>,
    ) {
        let method_borrow_lifetime =
            borrow.flat_map_nonstatic(|lt| lt.as_method_lifetime(method_lifetimes));
        let method_type_lifetimes = lifetimes.as_method_lifetimes(method_lifetimes);
        leaves.push(BorrowingFieldVisitorLeaf::Opaque(
            parent,
            method_borrow_lifetime,
            method_type_lifetimes,
        ));
    }

    /// Add a slice as a leaf during construction of a [`BorrowingFieldsVisitor`].
    fn visit_slice<P: TyPosition>(
        slice: &Slice<P>,
        parent: ParentId,
        method_lifetimes: &Lifetimes,
        leaves: &mut SmallVec<[BorrowingFieldVisitorLeaf; 8]>,
    ) {
        if let Some(lifetime) = slice.lifetime() {
            let method_lifetime =
                lifetime.flat_map_nonstatic(|lt| lt.as_method_lifetime(method_lifetimes));
            leaves.push(BorrowingFieldVisitorLeaf::Slice(parent, method_lifetime));
        }
    }

    /// Add a struct as a parent and recurse down leaves during construction of a
    /// [`BorrowingFieldsVisitor`].
    fn visit_struct(
        ty: &paths::StructPath,
        tcx: &'m TypeContext,
        parent: ParentId,
        method_lifetimes: &Lifetimes,
        parents: &mut SmallVec<[(Option<ParentId>, &'m Ident); 4]>,
        leaves: &mut SmallVec<[BorrowingFieldVisitorLeaf; 8]>,
    ) {
        let method_type_lifetimes = ty.lifetimes.as_method_lifetimes(method_lifetimes);
        for field in ty.resolve(tcx).fields.iter() {
            Self::from_type(
                &field.ty,
                tcx,
                ParentId::new(Some(parent), field.name.as_ref(), parents),
                &method_type_lifetimes,
                parents,
                leaves,
            );
        }
    }
}

impl<'m> BorrowingField<'m> {
    /// Visit fields in order.
    ///
    /// If `self` represents the field `param.first.second`, then calling [`BorrowingField::trace`]
    /// will visit the following in order: `"param"`, `"first"`, `"second"`.
    pub fn backtrace<F>(&self, mut visit: F)
    where
        F: FnMut(usize, &'m Ident),
    {
        let (parent, ident) = match self.leaf {
            BorrowingFieldVisitorLeaf::Opaque(id, ..) | BorrowingFieldVisitorLeaf::Slice(id, _) => {
                self.parents[id.0]
            }
        };

        self.backtrace_rec(parent, ident, &mut visit);
    }

    /// Recursively visits fields in order from root to leaf by building up the
    /// stack, and then visiting fields as it unwinds.
    fn backtrace_rec<F>(&self, parent: Option<ParentId>, ident: &'m Ident, visit: &mut F) -> usize
    where
        F: FnMut(usize, &'m Ident),
    {
        let from_end = if let Some(id) = parent {
            let (parent, ident) = self.parents[id.0];
            self.backtrace_rec(parent, ident, visit)
        } else {
            0
        };

        visit(from_end, ident);

        from_end + 1
    }

    /// Fallibly visits fields in order.
    ///
    /// This method is similar to [`BorrowinfField::backtrace`], but short-circuits
    /// when an `Err` is returned.
    pub fn try_backtrace<F, E>(&self, mut visit: F) -> Result<(), E>
    where
        F: FnMut(usize, &'m Ident) -> Result<(), E>,
    {
        let (parent, ident) = match self.leaf {
            BorrowingFieldVisitorLeaf::Opaque(id, ..) | BorrowingFieldVisitorLeaf::Slice(id, _) => {
                self.parents[id.0]
            }
        };

        self.try_backtrace_rec(parent, ident, &mut visit)?;

        Ok(())
    }

    /// Recursively visits fields in order from root to leaf by building up the
    /// stack, and then visiting fields as it unwinds.
    fn try_backtrace_rec<F, E>(
        &self,
        parent: Option<ParentId>,
        ident: &'m Ident,
        visit: &mut F,
    ) -> Result<usize, E>
    where
        F: FnMut(usize, &'m Ident) -> Result<(), E>,
    {
        let from_end = if let Some(id) = parent {
            let (parent, ident) = self.parents[id.0];
            self.try_backtrace_rec(parent, ident, visit)?
        } else {
            0
        };

        visit(from_end, ident)?;

        Ok(from_end + 1)
    }
}

impl<'m> fmt::Display for BorrowingField<'m> {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        self.try_backtrace(|i, ident| {
            if i != 0 {
                f.write_char('.')?;
            }
            f.write_str(ident.as_str())
        })
    }
}
