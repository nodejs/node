//! Lifetime information for types.
#![allow(dead_code)]

use super::IdentBuf;
use crate::ast;
use core::fmt::Debug;
use core::hash::Hash;

use smallvec::{smallvec, SmallVec};
use std::borrow::{Borrow, Cow};

/// Convenience const representing the number of lifetimes a [`LifetimeEnv`]
/// can hold inline before needing to dynamically allocate.
pub(crate) const INLINE_NUM_LIFETIMES: usize = 4;

/// The lifetimes and bounds found on a method or type definition
#[derive(Debug)]
pub struct LifetimeEnv {
    /// List of named lifetimes in scope of the method, and their bounds
    nodes: SmallVec<[BoundedLifetime; INLINE_NUM_LIFETIMES]>,

    /// Only relevant for method LifetimeEnvs (otherwise this is nodes.len())
    ///
    /// The number of named _and_ anonymous lifetimes in the method.
    /// We store the sum since it represents the upper bound on what indices
    /// are in range of the graph. If we make a [`Lifetimes`] with
    /// `num_lifetimes` entries, then `Lifetime`s that convert into
    /// `Lifetime`s will fall into this range, and we'll know that it's
    /// a named lifetime if it's < `nodes.len()`, or that it's an anonymous
    /// lifetime if it's < `num_lifetimes`. Otherwise, we'd have to make a
    /// distinction in `Lifetime` about which context it's in.
    num_lifetimes: usize,
}

impl LifetimeEnv {
    /// Format a lifetime indexing this env for use in code
    pub fn fmt_lifetime(&self, lt: impl Borrow<Lifetime>) -> Cow<'_, str> {
        // we use Borrow here so that this can be used in templates where there's autoborrowing
        let lt = *lt.borrow();
        if let Some(lt) = self.nodes.get(lt.0) {
            Cow::from(lt.ident.as_str())
        } else if lt.0 < self.num_lifetimes {
            format!("anon_{}", lt.0 - self.nodes.len()).into()
        } else {
            panic!("Found out of range lifetime: Got {lt:?} for env with {} nodes and {} total lifetimes", self.nodes.len(), self.num_lifetimes);
        }
    }

    /// Get an iterator of all lifetimes that this must live as long as (including itself)
    /// with the first lifetime always being returned first
    pub fn all_shorter_lifetimes(
        &self,
        lt: impl Borrow<Lifetime>,
    ) -> impl Iterator<Item = Lifetime> + '_ {
        // we use Borrow here so that this can be used in templates where there's autoborrowing
        let lt = *lt.borrow();
        // longer = true, since we are looking for lifetimes this is longer than
        LifetimeTransitivityIterator::new(self, lt.0, false)
    }

    /// Same as all_shorter_lifetimes but the other way
    pub fn all_longer_lifetimes(
        &self,
        lt: impl Borrow<Lifetime>,
    ) -> impl Iterator<Item = Lifetime> + '_ {
        // we use Borrow here so that this can be used in templates where there's autoborrowing
        let lt = *lt.borrow();
        LifetimeTransitivityIterator::new(self, lt.0, true)
    }

    // List all named and unnamed lifetimes
    pub fn num_lifetimes(&self) -> usize {
        self.num_lifetimes
    }

    pub fn all_lifetimes(&self) -> impl ExactSizeIterator<Item = Lifetime> {
        (0..self.num_lifetimes()).map(Lifetime::new)
    }

    /// Get the bounds for a named lifetime (none for unnamed lifetimes)
    pub(super) fn get_bounds(&self, named_lifetime: Lifetime) -> Option<&BoundedLifetime> {
        self.nodes.get(named_lifetime.0)
    }

    /// Returns a new [`LifetimeEnv`].
    pub(super) fn new(
        nodes: SmallVec<[BoundedLifetime; INLINE_NUM_LIFETIMES]>,
        num_lifetimes: usize,
    ) -> Self {
        Self {
            nodes,
            num_lifetimes,
        }
    }

    /// Returns a fresh [`Lifetimes`] corresponding to `self`.
    pub fn lifetimes(&self) -> Lifetimes {
        let indices = (0..self.num_lifetimes)
            .map(|index| MaybeStatic::NonStatic(Lifetime::new(index)))
            .collect();

        Lifetimes { indices }
    }

    /// Returns a new [`SubtypeLifetimeVisitor`], which can visit all reachable
    /// lifetimes
    pub fn subtype_lifetimes_visitor<F>(&self, visit_fn: F) -> SubtypeLifetimeVisitor<'_, F>
    where
        F: FnMut(Lifetime),
    {
        SubtypeLifetimeVisitor::new(self, visit_fn)
    }
}

/// A lifetime in a [`LifetimeEnv`], which keeps track of which lifetimes it's
/// longer and shorter than.
///
/// Invariant: for a BoundedLifetime found inside a LifetimeEnv, all short/long connections
/// should be bidirectional.
#[derive(Debug)]
pub(super) struct BoundedLifetime {
    pub(super) ident: IdentBuf,
    /// Lifetimes longer than this (not transitive)
    ///
    /// These are the inverse graph edges compared to `shorter`
    pub(super) longer: SmallVec<[Lifetime; 2]>,
    /// Lifetimes this is shorter than (not transitive)
    ///
    /// These match `'a: 'b + 'c` bounds
    pub(super) shorter: SmallVec<[Lifetime; 2]>,
}

impl BoundedLifetime {
    /// Returns a new [`BoundedLifetime`].
    pub(super) fn new(
        ident: IdentBuf,
        longer: SmallVec<[Lifetime; 2]>,
        shorter: SmallVec<[Lifetime; 2]>,
    ) -> Self {
        Self {
            ident,
            longer,
            shorter,
        }
    }
}

/// Visit subtype lifetimes recursively, keeping track of which have already
/// been visited.
pub struct SubtypeLifetimeVisitor<'lt, F> {
    lifetime_env: &'lt LifetimeEnv,
    visited: SmallVec<[bool; INLINE_NUM_LIFETIMES]>,
    visit_fn: F,
}

impl<'lt, F> SubtypeLifetimeVisitor<'lt, F>
where
    F: FnMut(Lifetime),
{
    fn new(lifetime_env: &'lt LifetimeEnv, visit_fn: F) -> Self {
        Self {
            lifetime_env,
            visited: smallvec![false; lifetime_env.nodes.len()],
            visit_fn,
        }
    }

    /// Visit more sublifetimes. This method tracks which lifetimes have already
    /// been visited, and uses this to not visit the same lifetime twice.
    pub fn visit_subtypes(&mut self, method_lifetime: Lifetime) {
        if let Some(visited @ false) = self.visited.get_mut(method_lifetime.0) {
            *visited = true;

            (self.visit_fn)(method_lifetime);

            for longer in self.lifetime_env.nodes[method_lifetime.0].longer.iter() {
                self.visit_subtypes(*longer)
            }
        } else {
            debug_assert!(
                method_lifetime.0 > self.lifetime_env.num_lifetimes,
                "method lifetime has an internal index that's not in range of the lifetime env"
            );
        }
    }
}

/// Wrapper type for `Lifetime` and `Lifetime`, indicating that it may
/// be the `'static` lifetime.
#[derive(Copy, Clone, Debug, Hash, PartialEq, Eq, PartialOrd, Ord)]
#[allow(clippy::exhaustive_enums)] // this will only ever have two variants
pub enum MaybeStatic<T> {
    Static,
    NonStatic(T),
}

impl<T> MaybeStatic<T> {
    /// Maps the lifetime, if it's not the `'static` lifetime, to another
    /// non-static lifetime.
    pub(super) fn map_nonstatic<F, R>(self, f: F) -> MaybeStatic<R>
    where
        F: FnOnce(T) -> R,
    {
        match self {
            MaybeStatic::Static => MaybeStatic::Static,
            MaybeStatic::NonStatic(lifetime) => MaybeStatic::NonStatic(f(lifetime)),
        }
    }

    /// Maps the lifetime, if it's not the `'static` lifetime, to a potentially
    /// static lifetime.
    pub(super) fn flat_map_nonstatic<R, F>(self, f: F) -> MaybeStatic<R>
    where
        F: FnOnce(T) -> MaybeStatic<R>,
    {
        match self {
            MaybeStatic::Static => MaybeStatic::Static,
            MaybeStatic::NonStatic(lifetime) => f(lifetime),
        }
    }
}

/// A lifetime that exists as part of a type name, struct signature, or method signature.
///
/// This index only makes sense in the context of a surrounding type or method; since
/// this is essentially an index into that type/method's lifetime list.
#[derive(Copy, Clone, Hash, PartialEq, Eq, PartialOrd, Ord, Debug)]
pub struct Lifetime(usize);

/// A set of lifetimes found on a type name, struct signature, or method signature
#[derive(Clone, Debug)]
pub struct Lifetimes {
    indices: SmallVec<[MaybeStatic<Lifetime>; 2]>,
}

impl Lifetime {
    pub(super) const fn new(index: usize) -> Self {
        Self(index)
    }
}

impl Lifetimes {
    /// Returns an iterator over the contained [`Lifetime`]s.
    pub fn lifetimes(&self) -> impl ExactSizeIterator<Item = MaybeStatic<Lifetime>> + '_ {
        self.indices.iter().copied()
    }

    pub(super) fn as_slice(&self) -> &[MaybeStatic<Lifetime>] {
        self.indices.as_slice()
    }
}

impl Lifetime {
    /// Returns a [`Lifetime`] from its AST counterparts.
    pub(super) fn from_ast(named: &ast::NamedLifetime, lifetime_env: &ast::LifetimeEnv) -> Self {
        let index = lifetime_env
            .id(named)
            .unwrap_or_else(|| panic!("lifetime `{named}` not found in lifetime env"));
        Self::new(index)
    }

    /// Returns a new [`MaybeStatic<Lifetime>`] representing `self` in the
    /// scope of the method that it appears in.
    ///
    /// For example, if we have some `Foo<'a>` type with a field `&'a Bar`, then
    /// we can call this on the `'a` on the field. If `Foo` was `Foo<'static>`
    /// in the method, then this will return `MaybeStatic::Static`. But if it
    /// was `Foo<'b>`, then this will return `MaybeStatic::NonStatic` containing
    /// the `Lifetime` corresponding to `'b`.
    pub fn as_method_lifetime(self, method_lifetimes: &Lifetimes) -> MaybeStatic<Lifetime> {
        method_lifetimes.indices[self.0]
    }
}

impl Lifetimes {
    pub(super) fn from_fn<F>(lifetimes: &[ast::Lifetime], lower_fn: F) -> Self
    where
        F: FnMut(&ast::Lifetime) -> MaybeStatic<Lifetime>,
    {
        Self {
            indices: lifetimes.iter().map(lower_fn).collect(),
        }
    }

    /// Append an additional lifetime. Used to tack on anon lifetimes
    pub(super) fn append_lifetime(&mut self, lifetime: MaybeStatic<Lifetime>) {
        self.indices.push(lifetime)
    }

    /// Returns a new [`Lifetimes`] representing the lifetimes in the scope
    /// of the method this type appears in.
    ///
    /// # Examples
    ///
    /// ```rust
    /// # struct Alice<'a>(&'a ());
    /// # struct Bob<'b>(&'b ());
    /// struct Foo<'a, 'b> {
    ///     alice: Alice<'a>,
    ///     bob: Bob<'b>,
    /// }
    ///
    /// fn bar<'x, 'y>(arg: Foo<'x, 'y>) {}
    /// ```
    /// Here, `Foo` will have a [`Lifetimes`] containing `['a, 'b]`,
    /// and `bar` will have a [`Lifetimes`] containing `{'x: 'x, 'y: 'y}`.
    /// When we enter the scope of `Foo` as a type, we use this method to combine
    /// the two to get a new [`Lifetimes`] representing the mapping from
    /// lifetimes in `Foo`'s scope to lifetimes in `bar`s scope: `{'a: 'x, 'b: 'y}`.
    ///
    /// This tells us that `arg.alice` has lifetime `'x` in the method, and
    /// that `arg.bob` has lifetime `'y`.
    pub fn as_method_lifetimes(&self, method_lifetimes: &Lifetimes) -> Lifetimes {
        let indices = self
            .indices
            .iter()
            .map(|maybe_static_lt| {
                maybe_static_lt.flat_map_nonstatic(|lt| lt.as_method_lifetime(method_lifetimes))
            })
            .collect();

        Lifetimes { indices }
    }
}

struct LifetimeTransitivityIterator<'env> {
    env: &'env LifetimeEnv,
    visited: Vec<bool>,
    queue: Vec<usize>,
    longer: bool,
}

impl<'env> LifetimeTransitivityIterator<'env> {
    // Longer is whether we are looking for lifetimes longer or shorter than this
    fn new(env: &'env LifetimeEnv, starting: usize, longer: bool) -> Self {
        Self {
            env,
            visited: vec![false; env.num_lifetimes()],
            queue: vec![starting],
            longer,
        }
    }
}

impl<'env> Iterator for LifetimeTransitivityIterator<'env> {
    type Item = Lifetime;

    fn next(&mut self) -> Option<Lifetime> {
        while let Some(next) = self.queue.pop() {
            if self.visited[next] {
                continue;
            }
            self.visited[next] = true;

            if let Some(named) = self.env.nodes.get(next) {
                let edge_dir = if self.longer {
                    &named.longer
                } else {
                    &named.shorter
                };
                self.queue.extend(edge_dir.iter().map(|i| i.0));
            }

            return Some(Lifetime::new(next));
        }
        None
    }
}

/// Convenience type for linking the lifetimes found at a type *use* site (e.g. `&'c Foo<'a, 'b>`)
/// with the lifetimes found at its *def* site (e.g. `struct Foo<'x, 'y>`).
///
/// Construct this by calling `.linked_lifetimes()` on a StructPath or OpaquePath
pub struct LinkedLifetimes<'def, 'tcx> {
    env: &'tcx LifetimeEnv,
    self_lt: Option<MaybeStatic<Lifetime>>,
    lifetimes: &'def Lifetimes,
}

impl<'def, 'tcx> LinkedLifetimes<'def, 'tcx> {
    pub(crate) fn new(
        env: &'tcx LifetimeEnv,
        self_lt: Option<MaybeStatic<Lifetime>>,
        lifetimes: &'def Lifetimes,
    ) -> Self {
        debug_assert_eq!(
            lifetimes.lifetimes().len(),
            env.all_lifetimes().len(),
            "Should only link lifetimes between a type and its def"
        );
        Self {
            env,
            self_lt,
            lifetimes,
        }
    }

    /// Takes a lifetime at the def site and produces one at the use site
    pub fn def_to_use(&self, def_lt: Lifetime) -> MaybeStatic<Lifetime> {
        *self
            .lifetimes
            .as_slice()
            .get(def_lt.0)
            .expect("All def site lifetimes must be used!")
    }

    /// The lifetime env at the def site. Def lifetimes should be resolved
    /// against this.
    pub fn def_env(&self) -> &'tcx LifetimeEnv {
        self.env
    }

    /// Link lifetimes from the use site to lifetimes from the def site, only including
    /// lifetimes found at the def site.
    ///
    /// This will *not* include the self-lifetime, i.e. for an opaque use site `&'c Foo<'a, 'b>`
    /// this will not include `'c` (but you can obtain it from [`Self::self_lifetime()`]))
    ///
    /// The return iterator returns pairs of (use_lt, def_lt), in order.
    ///
    /// This behaves identically to [`Self::lifetimes_all()`] for `LinkedLifetimes` constructed
    /// from anything other than a borrowing opaque.
    pub fn lifetimes_def_only(
        &self,
    ) -> impl Iterator<Item = (MaybeStatic<Lifetime>, Lifetime)> + '_ {
        self.lifetimes.lifetimes().zip(self.env.all_lifetimes())
    }

    /// If there is a self-lifetime (e.g. `'c` on `&'c Foo<'a, 'b>`), return it. This lifetime
    /// isn't found at the def site.
    pub fn self_lifetime(&self) -> Option<MaybeStatic<Lifetime>> {
        self.self_lt
    }

    /// Link lifetimes from the use site to lifetimes from the def site, including self lifetimes.
    ///
    /// This returns Options since self-lifetimes do not map to anything at the def site.
    ///
    /// The return iterator returns pairs of (use_lt, def_lt), in order, with the first entry potentially being
    /// the self lifetime (which has a def_lt of None).
    ///
    /// This behaves identically to [`Self::lifetimes_all()`] for `LinkedLifetimes` constructed
    /// from anything other than a borrowing opaque.
    pub fn lifetimes_all(
        &self,
    ) -> impl Iterator<Item = (MaybeStatic<Lifetime>, Option<Lifetime>)> + '_ {
        self.self_lt.iter().map(|i| (*i, None)).chain(
            self.lifetimes
                .lifetimes()
                .zip(self.env.all_lifetimes().map(Some)),
        )
    }
}
