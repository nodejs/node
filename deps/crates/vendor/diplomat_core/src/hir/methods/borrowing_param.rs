//! Tools for traversing all borrows in method parameters and return types, shallowly
//!
//! This is useful for backends which wish to figure out the borrowing relationships between parameters
//! and return values,
//! and then delegate how lifetimes get mapped to fields to the codegen for those types respectively.
//!
//!
//! # Typical usage
//!
//! In managed languages, to ensure the GC doesn't prematurely clean up the borrowed-from object,
//! we use the technique of stashing a borrowed-from object as a field of a borrowed-to object.
//! This is called a "lifetime edge" (since it's an edge in the GC graph). An "edge array" is typically
//! a stash of all the edges on an object, or all the edges corresponding to a particular lifetime.
//!
//! This stashing is primarily driven by method codegen, which is where actual borrow relationships can be set up.
//!
//! Typically, codegen for a method should instantiate a [`BorrowedParamVisitor`] and uses it to [`BorrowedParamVisitor::visit_param()`] each input parameter to determine if it needs to be linked into an edge.
//! Whilst doing so, it may return useful [`ParamBorrowInfo`] which provides additional information on handling structs and slices. More on structs later.
//! At the end of the visiting, [`BorrowedLifetimeInfo::borrow_map()`] can be called to get a list of relevant input edges: each lifetime that participates in borrowing,
//! and a list of parameter names that it borrows from.
//!
//! This visitor will automatically handle transitive lifetime relationships as well.
//!
//! These edges should be put together into "edge arrays" which then get passed down to output types, handling transitive lifetimes as necessary.
//!
//! ## Method codegen for slices
//!
//! Slices are strange: in managed languages a host slice will not be a Rust slice, unlike other borrowed host values (a borrowed host opaque is just a borrowed Rust
//! opaque). We need to convert these across the boundary by allocating a temporary arena or something.
//!
//! If the slice doesn't participate in borrowing, this is easy: just allocate a temporary arena. However, if not, we need to allocate an arena with a finalizer (or something similar)
//! and add that arena to the edge instead. [`LifetimeEdgeKind`] has special handling for this.
//!
//! Whether or not the slice participates in borrowing is found out from the [`ParamBorrowInfo`] returned by [`BorrowedParamVisitor::visit_param()`].
//!
//! # Method codegen for struct params
//!
//! Structs can contain many lifetimes and have relationships between them. Generally, a Diplomat struct is a "bag of stuff"; it is converted to a host value that is structlike, with fields
//! individually converted.
//!
//! Diplomat enforces that struct lifetime bounds never imply additional bounds on methods during HIR validation, so the relationships between struct
//! lifetimes are not relevant for code here (and as such you should hopefully never need to call [`LifetimeEnv::all_longer_lifetimes()`])
//! for a struct env).
//!
//! The borrowing technique in this module allows a lot of things to be delegated to structs. As will be explained below, structs will have:
//!
//! - A `fields_for_lifetime_foo()` set of methods that returns all non-slice fields corresponding to a lifetime
//! - "append array" outparams for stashing edges when converting from host-to-Rust
//! - Some way of passing down edge arrays to individual borrowed fields when constructing Rust-to-host
//!
//! In methods, when constructing the edge arrays, `fields_for_lifetime_foo()` for every borrowed param can be appended to each
//! one whenever [`LifetimeEdgeKind::StructLifetime`] asks you to do so. The code needs to handle lifetime
//! transitivity, since the struct will not be doing so. Fortunately the edges produced by [`BorrowedParamVisitor`] already do so.
//!
//! Then, when converting structs host-to-Rust, every edge array relevant to a struct lifetime should be passed in for an append array. Append arrays
//! are nested arrays: they are an array of edge arrays. The struct will append further things to the edge array.
//!
//! Finally, when converting Rust-to-host, if producing a struct, make sure to pass in all the edge arrays.
//!
//! # Struct codegen
//!
//! At a high level, struct codegen needs to deal with lifetimes in three places:
//!
//! - A `fields_for_lifetime_foo()` set of methods that returns all non-slice fields corresponding to a lifetime
//! - "append array" outparams for stashing edges when converting from host-to-Rust
//! - Some way of passing down edge arrays to individual borrowed fields when constructing Rust-to-host
//!
//! The backend may choose to handle just slices via append arrays, or handle all borrowing there, in which case `fields_for_lifetime_foo()` isn't necessary.
//!
//! `fields_for_lifetime_foo()` should return an iterator/list/etc corresponding to each field that *directly* borrows from lifetime foo.
//! This may include calling `fields_for_lifetime_bar()` on fields that are themselves structs. As mentioned previously, lifetime relationships
//! are handled by methods and don't need to be dealt with here. There are no helpers for doing this but it's just a matter of
//! filtering for fields using the lifetime, except for handling nested structs ([`StructBorrowInfo::compute_for_struct_field()`])
//!
//! Append arrays are similarly straightforward: for each lifetime on the struct, the host-to-Rust constructor should accept a list of edge arrays. For slice fields,
//! if the appropriate list is nonempty, the slice's arena edge should be appended to all of the edge arrays in it. These arrays should also be passed down to struct fields
//! appropriately: [`StructBorrowInfo::compute_for_struct_field()`] is super helpful for getting a list edges something should get passed down to.
//!
//! Finally, when converting Rust-to-host, the relevant lifetime edges should be proxied down to the final host type constructors who can stash them wherever needed.
//! This is one again a matter of filtering fields by the lifetimes they use, and for nested structs you can use [`StructBorrowInfo::compute_for_struct_field()`].

use std::collections::{BTreeMap, BTreeSet};

use crate::hir::{self, Method, StructDef, StructPath, TyPosition, TypeContext};

use crate::hir::lifetimes::{Lifetime, LifetimeEnv, MaybeStatic};
use crate::hir::ty_position::StructPathLike;

/// A visitor for processing method parameters/returns and understanding their borrowing relationships, shallowly.
///
/// This produces a list of lifetime "edges" per lifetime in the output producing a borrow.
///
/// Each `BorrowingFieldsVisitor` corresponds to the type of an input of a method.
///
/// Obtain from [`Method::borrowing_param_visitor()`].
pub struct BorrowingParamVisitor<'tcx> {
    tcx: &'tcx TypeContext,
    used_method_lifetimes: BTreeSet<Lifetime>,
    borrow_map: BTreeMap<Lifetime, BorrowedLifetimeInfo<'tcx>>,
}

/// A single lifetime "edge" from a parameter to a value
#[non_exhaustive]
#[derive(Clone, Debug)]
pub struct LifetimeEdge<'tcx> {
    pub param_name: String,
    pub kind: LifetimeEdgeKind<'tcx>,
}

#[non_exhaustive]
#[derive(Copy, Clone, Debug)]
pub enum LifetimeEdgeKind<'tcx> {
    /// Just an opaque parameter directly being borrowed.
    OpaqueParam,
    /// A slice being converted and then borrowed. These often need to be handled differently
    /// when they are borrowed as the borrow will need to create an edge
    SliceParam,
    /// A lifetime parameter of a struct, given the lifetime context and the struct-def lifetime for that struct.
    ///
    /// The boolean is whether or not the struct is optional.
    ///
    /// Using this, you can generate code that "asks" the struct for the lifetime-relevant field edges
    StructLifetime(&'tcx LifetimeEnv, Lifetime, bool),
}

#[non_exhaustive]
#[derive(Clone, Debug)]
pub struct BorrowedLifetimeInfo<'tcx> {
    // Initializers for all inputs to the edge array from parameters, except for slices (slices get handled
    // differently)
    pub incoming_edges: Vec<LifetimeEdge<'tcx>>,
    // All lifetimes longer than this. When this lifetime is borrowed from, data corresponding to
    // the other lifetimes may also be borrowed from.
    pub all_longer_lifetimes: BTreeSet<Lifetime>,
}

impl<'tcx> BorrowingParamVisitor<'tcx> {
    pub(crate) fn new(
        method: &'tcx Method,
        tcx: &'tcx TypeContext,
        force_include_slices: bool,
    ) -> Self {
        let mut used_method_lifetimes = method.output.used_method_lifetimes();

        // If we need to track the lifetime of slices, we just need to make sure to add a slice's
        // lifetime to the used_method_lifetimes set.
        //
        // Currently this is done recursively, but as mentioned in
        // https://github.com/rust-diplomat/diplomat/pull/839#discussion_r2031764206,
        // a long term solution would involve re-designing the lifetimes map to track lifetimes
        // that involve slices.
        if force_include_slices {
            if let Some(s) = &method.param_self {
                if let hir::SelfType::Struct(s) = &s.ty {
                    let st = s.resolve(tcx);
                    for f in &st.fields {
                        BorrowingParamVisitor::add_slices_to_used_lifetimes(
                            &mut used_method_lifetimes,
                            method,
                            tcx,
                            &f.ty,
                        );
                    }
                }
            }

            for p in &method.params {
                BorrowingParamVisitor::add_slices_to_used_lifetimes(
                    &mut used_method_lifetimes,
                    method,
                    tcx,
                    &p.ty,
                );
            }
        }

        let borrow_map = used_method_lifetimes
            .iter()
            .map(|lt| {
                (
                    *lt,
                    BorrowedLifetimeInfo {
                        incoming_edges: Vec::new(),
                        all_longer_lifetimes: method
                            .lifetime_env
                            .all_longer_lifetimes(lt)
                            .collect(),
                    },
                )
            })
            .collect();
        BorrowingParamVisitor {
            tcx,
            used_method_lifetimes,
            borrow_map,
        }
    }

    /// Given a specific [hir::Type] `ty`, find the lifetimes of slices associated with `ty` and add them to `set`.
    ///
    /// We're only interested in non-static, bounded lifetimes (since those are ones we can explicitly de-allocate).
    fn add_slices_to_used_lifetimes<P: TyPosition<StructPath = StructPath>>(
        set: &mut BTreeSet<Lifetime>,
        method: &'tcx Method,
        tcx: &'tcx TypeContext,
        ty: &hir::Type<P>,
    ) {
        match ty {
            hir::Type::Struct(s) => {
                let st = s.resolve(tcx);
                for f in &st.fields {
                    BorrowingParamVisitor::add_slices_to_used_lifetimes(set, method, tcx, &f.ty);
                }
            }
            hir::Type::Slice(s) => {
                if let Some(MaybeStatic::NonStatic(lt)) = s.lifetime() {
                    if method.lifetime_env.get_bounds(*lt).is_some() {
                        set.insert(*lt);
                    }
                }
            }
            _ => {}
        }
    }

    /// Get the cached list of used method lifetimes. Same as calling `.used_method_lifetimes()` on `method.output`
    pub fn used_method_lifetimes(&self) -> &BTreeSet<Lifetime> {
        &self.used_method_lifetimes
    }

    /// Get the final borrow map, listing lifetime edges for each output lfietime
    pub fn borrow_map(self) -> BTreeMap<Lifetime, BorrowedLifetimeInfo<'tcx>> {
        self.borrow_map
    }

    /// Processes a parameter, adding it to the borrow_map for any lifetimes it references. Returns further information about the type of borrow.
    ///
    /// This basically boils down to: For each lifetime that is actually relevant to borrowing in this method, check if that
    /// lifetime or lifetimes longer than it are used by this parameter. In other words, check if
    /// it is possible for data in the return type with this lifetime to have been borrowed from this parameter.
    /// If so, add code that will yield the ownership-relevant parts of this object to incoming_edges for that lifetime.
    pub fn visit_param<P: TyPosition<StructPath = StructPath>>(
        &mut self,
        ty: &hir::Type<P>,
        param_name: &str,
    ) -> ParamBorrowInfo<'tcx> {
        let mut is_borrowed = false;
        if self.used_method_lifetimes.is_empty() {
            if let hir::Type::Slice(..) = *ty {
                return ParamBorrowInfo::TemporarySlice;
            } else {
                return ParamBorrowInfo::NotBorrowed;
            }
        }

        // Structs have special handling: structs are purely Dart-side, so if you borrow
        // from a struct, you really are borrowing from the internal fields.
        if let hir::Type::Struct(s) = ty {
            let mut borrowed_struct_lifetime_map = BTreeMap::<Lifetime, BTreeSet<Lifetime>>::new();
            let link = s.link_lifetimes(self.tcx);
            for (method_lifetime, method_lifetime_info) in &mut self.borrow_map {
                // Note that ty.lifetimes()/s.lifetimes() is lifetimes
                // in the *use* context, i.e. lifetimes on the Type that reference the
                // indices of the method's lifetime arrays. Their *order* references
                // the indices of the underlying struct def. We need to link the two,
                // since the _fields_for_lifetime_foo() methods are named after
                // the *def* context lifetime.
                //
                // Concretely, if we have struct `Foo<'a, 'b>` and our method
                // accepts `Foo<'x, 'y>`, we need to output _fields_for_lifetime_a()/b not x/y.
                //
                // This is a struct so lifetimes_def_only() is fine to call
                for (use_lt, def_lt) in link.lifetimes_def_only() {
                    if let MaybeStatic::NonStatic(use_lt) = use_lt {
                        if method_lifetime_info.all_longer_lifetimes.contains(&use_lt) {
                            let edge = LifetimeEdge {
                                param_name: param_name.into(),
                                kind: LifetimeEdgeKind::StructLifetime(
                                    link.def_env(),
                                    def_lt,
                                    ty.is_option(),
                                ),
                            };
                            method_lifetime_info.incoming_edges.push(edge);

                            is_borrowed = true;

                            borrowed_struct_lifetime_map
                                .entry(def_lt)
                                .or_default()
                                .insert(*method_lifetime);
                            // Do *not* break the inner loop here: even if we found *one* matching lifetime
                            // in this struct that may not be all of them, there may be some other fields that are borrowed
                        }
                    }
                }
            }
            if is_borrowed {
                ParamBorrowInfo::Struct(StructBorrowInfo {
                    env: link.def_env(),
                    borrowed_struct_lifetime_map,
                })
            } else {
                ParamBorrowInfo::NotBorrowed
            }
        } else {
            for method_lifetime in self.borrow_map.values_mut() {
                for lt in ty.lifetimes() {
                    if let MaybeStatic::NonStatic(lt) = lt {
                        if method_lifetime.all_longer_lifetimes.contains(&lt) {
                            let kind = match ty {
                                hir::Type::Slice(..) => LifetimeEdgeKind::SliceParam,
                                hir::Type::Opaque(..) => LifetimeEdgeKind::OpaqueParam,
                                _ => unreachable!("Types other than slices, opaques, and structs cannot have lifetimes")
                            };

                            let edge = LifetimeEdge {
                                param_name: param_name.into(),
                                kind,
                            };

                            method_lifetime.incoming_edges.push(edge);
                            is_borrowed = true;
                            // Break the inner loop: we've already determined this
                            break;
                        }
                    }
                }
            }
            match (is_borrowed, ty) {
                (true, &hir::Type::Slice(..)) => ParamBorrowInfo::BorrowedSlice,
                (false, &hir::Type::Slice(..)) => ParamBorrowInfo::TemporarySlice,
                (false, _) => ParamBorrowInfo::NotBorrowed,
                (true, _) => ParamBorrowInfo::BorrowedOpaque,
            }
        }
    }
}

/// Information relevant to borrowing for producing conversions
#[derive(Clone, Debug)]
#[non_exhaustive]
pub enum ParamBorrowInfo<'tcx> {
    /// No borrowing constraints. This means the parameter
    /// is not borrowed by the output and also does not need temporary borrows
    NotBorrowed,
    /// A slice that is not borrowed by the output (but will still need temporary allocation)
    TemporarySlice,
    /// A slice that is borrowed by the output
    BorrowedSlice,
    /// A struct parameter that is borrowed by the output
    Struct(StructBorrowInfo<'tcx>),
    /// An opaque type that is borrowed
    BorrowedOpaque,
}

/// Information about the lifetimes of a struct parameter that are borrowed by a method output or by a wrapping struct
#[derive(Clone, Debug)]
#[non_exhaustive]
pub struct StructBorrowInfo<'tcx> {
    /// This is the struct's lifetime environment
    pub env: &'tcx LifetimeEnv,
    /// A map from (borrow-relevant) struct lifetimes to lifetimes in the method (or wrapping struct) that may flow from it
    pub borrowed_struct_lifetime_map: BTreeMap<Lifetime, BTreeSet<Lifetime>>,
}

impl<'tcx> StructBorrowInfo<'tcx> {
    /// Get borrowing info for a struct field, if it does indeed borrow
    ///
    /// The lifetime map produced here does not handle lifetime dependencies: the expectation is that the struct
    /// machinery generated by this will be called by method code that handles these dependencies. We try to handle
    /// lifetime dependencies in ONE place.
    pub fn compute_for_struct_field<P: TyPosition>(
        struc: &StructDef<P>,
        field: &P::StructPath,
        tcx: &'tcx TypeContext,
    ) -> Option<Self> {
        if field.lifetimes().as_slice().is_empty() {
            return None;
        }

        let mut borrowed_struct_lifetime_map = BTreeMap::<Lifetime, BTreeSet<Lifetime>>::new();

        let link = field.link_lifetimes(tcx);

        for outer_lt in struc.lifetimes.all_lifetimes() {
            // Note that field.lifetimes()
            // in the *use* context, i.e. lifetimes on the Type that reference the
            // indices of the outer struct's lifetime arrays. Their *order* references
            // the indices of the underlying struct def. We need to link the two,
            // since the _fields_for_lifetime_foo() methods are named after
            // the *def* context lifetime.
            //
            // This is a struct so lifetimes_def_only() is fine to call
            for (use_lt, def_lt) in link.lifetimes_def_only() {
                if let MaybeStatic::NonStatic(use_lt) = use_lt {
                    // We do *not* need to transitively check for longer lifetimes here:
                    //
                    if outer_lt == use_lt {
                        borrowed_struct_lifetime_map
                            .entry(def_lt)
                            .or_default()
                            .insert(outer_lt);
                    }
                }
            }
        }
        if borrowed_struct_lifetime_map.is_empty() {
            // if the inner struct is only statics
            None
        } else {
            Some(StructBorrowInfo {
                env: link.def_env(),
                borrowed_struct_lifetime_map,
            })
        }
    }
}
