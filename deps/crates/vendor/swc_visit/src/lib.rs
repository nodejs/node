//! Visitor generator for the rust language.
//!
//!
//! There are three variants of visitor in swc. Those are `Fold`, `VisitMut`,
//! `Visit`.
//!
//! # Comparisons
//!
//! ## `Fold` vs `VisitMut`
//!
//! `Fold` and `VisitMut` do almost identical tasks, but `Fold` is easier to use
//! while being slower and weak to stack overflow for very deep asts. `Fold` is
//! fast enough for almost all cases so it would be better to start with `Fold`.
//!
//! By very deep asts, I meant code like thousands of `a + a + a + a + ...`.
//!
//!
//! # `Fold`
//!
//! > WARNING: `Fold` is slow, and it's recommended to use VisitMut if you are
//! > experienced.
//!
//!
//! `Fold` takes ownership of value, which means you have to return the new
//! value. Returning new value means returning ownership of the value. But you
//! don't have to care about ownership or about managing memories while using
//! such visitors. `rustc` handles them automatically and all allocations will
//! be freed when it goes out of the scope.
//!
//! You can invoke your `Fold` implementation like `node.fold_with(&mut
//! visitor)` where `visitor` is your visitor. Note that as it takes ownership
//! of value, you have to call `node.fold_children_with(self)` in e.g. `fn
//! fold_module(&mut self, m: Module) -> Module` if you override the default
//! behavior. Also you have to store return value from `fold_children_with`,
//! like `let node = node.fold_children_with(self)`. Order of execution can be
//! controlled using this. If there is some logic that should be applied to the
//! parent first, you can call `fold_children_with` after such logic.
//!
//! # `VisitMut`
//!
//! `VisitMut` uses a mutable reference to AST nodes (e.g. `&mut Expr`). You can
//! use `Take` from `swc_common::util::take::Take` to get owned value from a
//! mutable reference.
//!
//! You will typically use code like
//!
//! ```ignore
//! *e = return_value.take();
//! ```
//!
//! where `e = &mut Expr` and `return_value` is also `&mut Expr`. `take()` is an
//! extension method defined on `MapWithMut`.  It's almost identical to `Fold`,
//! so I'll skip memory management.
//!
//! You can invoke your `VisitMut` implementation like `node.visit_mut_with(&mut
//! visitor)` where `visitor` is your visitor. Again, you need to call
//! `node.visit_mut_children_with(self)` in visitor implementation if you want
//! to modify children nodes. You don't need to store the return value in this
//! case.
//!
//!
//! # `Visit`
//!
//!`Visit` uses non-mutable references to AST nodes. It can be used to see if
//! an AST node contains a specific node nested deeply in the AST. This is
//! useful for checking if AST node contains `this`. This is useful for lots of
//! cases - `this` in arrow expressions are special and we need to generate
//! different code if a `this` expression is used.
//!
//! You can use your `Visit` implementation like  `node.visit_with(&Invalid{
//! span: DUMMY_SP, }, &mut visitor`. I think API is mis-designed, but it works
//! and there are really lots of code using `Visit` already.
//!
//!
//!
//! # Cargo features
//!
//! You should add
//! ```toml
//! [features]
//! path = []
//! ```
//!
//! If you want to allow using path-aware visitor.
//!
//!
//! # Path-aware visitor
//!
//! Path-aware visitor is a visitor that can be used to visit AST nodes with
//! current path from the entrypoint.
//!
//! `VisitMutAstPath` and `FoldAstPath` can be used to transform AST nodes with
//! the path to the node.

use std::ops::{Deref, DerefMut};

pub use either::Either;

pub mod util;

/// Visit all children nodes. This converts `VisitAll` to `Visit`. The type
/// parameter `V` should implement `VisitAll` and `All<V>` implements `Visit`.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub struct All<V> {
    pub visitor: V,
}

/// A visitor which visits node only if `enabled` is true.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub struct Optional<V> {
    pub enabled: bool,
    pub visitor: V,
}

impl<V> Optional<V> {
    pub const fn new(visitor: V, enabled: bool) -> Self {
        Self { enabled, visitor }
    }
}

/// Trait for a pass which is designed to invoked multiple time to same input.
///
/// See [Repeat].
pub trait Repeated {
    /// Should run again?
    fn changed(&self) -> bool;

    /// Reset.
    fn reset(&mut self);
}

macro_rules! impl_repeated_for_tuple {
    (
        [$idx:tt, $name:ident], $([$idx_rest:tt, $name_rest:ident]),*
    ) => {
        impl<$name, $($name_rest),*> Repeated for ($name, $($name_rest),*)
        where
            $name: Repeated,
            $($name_rest: Repeated),*
        {
            fn changed(&self) -> bool {
                self.$idx.changed() || $(self.$idx_rest.changed() ||)* false
            }

            fn reset(&mut self) {
                self.$idx.reset();
                $(self.$idx_rest.reset();)*
            }
        }
    };
}

impl_repeated_for_tuple!([0, A], [1, B]);
impl_repeated_for_tuple!([0, A], [1, B], [2, C]);
impl_repeated_for_tuple!([0, A], [1, B], [2, C], [3, D]);
impl_repeated_for_tuple!([0, A], [1, B], [2, C], [3, D], [4, E]);
impl_repeated_for_tuple!([0, A], [1, B], [2, C], [3, D], [4, E], [5, F]);
impl_repeated_for_tuple!([0, A], [1, B], [2, C], [3, D], [4, E], [5, F], [6, G]);
impl_repeated_for_tuple!(
    [0, A],
    [1, B],
    [2, C],
    [3, D],
    [4, E],
    [5, F],
    [6, G],
    [7, H]
);
impl_repeated_for_tuple!(
    [0, A],
    [1, B],
    [2, C],
    [3, D],
    [4, E],
    [5, F],
    [6, G],
    [7, H],
    [8, I]
);
impl_repeated_for_tuple!(
    [0, A],
    [1, B],
    [2, C],
    [3, D],
    [4, E],
    [5, F],
    [6, G],
    [7, H],
    [8, I],
    [9, J]
);
impl_repeated_for_tuple!(
    [0, A],
    [1, B],
    [2, C],
    [3, D],
    [4, E],
    [5, F],
    [6, G],
    [7, H],
    [8, I],
    [9, J],
    [10, K]
);
impl_repeated_for_tuple!(
    [0, A],
    [1, B],
    [2, C],
    [3, D],
    [4, E],
    [5, F],
    [6, G],
    [7, H],
    [8, I],
    [9, J],
    [10, K],
    [11, L]
);
impl_repeated_for_tuple!(
    [0, A],
    [1, B],
    [2, C],
    [3, D],
    [4, E],
    [5, F],
    [6, G],
    [7, H],
    [8, I],
    [9, J],
    [10, K],
    [11, L],
    [12, M]
);

/// A visitor which applies `V` again and again if `V` modifies the node.
///
/// # Note
/// `V` should return `true` from `changed()` to make the pass run multiple
/// time.
///
/// See: [Repeated]
#[derive(Debug, Default, Clone, Copy, PartialEq, Eq)]
pub struct Repeat<V>
where
    V: Repeated,
{
    pub pass: V,
}

impl<V> Repeat<V>
where
    V: Repeated,
{
    pub fn new(pass: V) -> Self {
        Self { pass }
    }
}

impl<V> Repeated for Repeat<V>
where
    V: Repeated,
{
    fn changed(&self) -> bool {
        self.pass.changed()
    }

    fn reset(&mut self) {
        self.pass.reset()
    }
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct AstKindPath<K>
where
    K: ParentKind,
{
    path: Vec<K>,
}

impl<K> std::ops::Deref for AstKindPath<K>
where
    K: ParentKind,
{
    type Target = Vec<K>;

    fn deref(&self) -> &Self::Target {
        &self.path
    }
}

impl<K> Default for AstKindPath<K>
where
    K: ParentKind,
{
    fn default() -> Self {
        Self {
            path: Default::default(),
        }
    }
}

impl<K> AstKindPath<K>
where
    K: ParentKind,
{
    pub fn new(path: Vec<K>) -> Self {
        Self { path }
    }

    pub fn with_guard(&mut self, kind: K) -> AstKindPathGuard<K> {
        self.path.push(kind);

        AstKindPathGuard { path: self }
    }

    pub fn with_index_guard(&mut self, index: usize) -> AstKindPathIndexGuard<K> {
        self.path.last_mut().unwrap().set_index(index);

        AstKindPathIndexGuard { path: self }
    }

    #[deprecated = "Use with_guard instead"]
    pub fn with<Ret>(&mut self, path: K, op: impl FnOnce(&mut Self) -> Ret) -> Ret {
        self.path.push(path);
        let ret = op(self);
        self.path.pop();
        ret
    }

    #[deprecated = "Use with_index_guard instead"]
    pub fn with_index<Ret>(&mut self, index: usize, op: impl FnOnce(&mut Self) -> Ret) -> Ret {
        self.path.last_mut().unwrap().set_index(index);
        let res = op(self);
        self.path.last_mut().unwrap().set_index(usize::MAX);
        res
    }
}

pub struct AstKindPathGuard<'a, K>
where
    K: ParentKind,
{
    path: &'a mut AstKindPath<K>,
}

impl<K> Deref for AstKindPathGuard<'_, K>
where
    K: ParentKind,
{
    type Target = AstKindPath<K>;

    #[inline]
    fn deref(&self) -> &Self::Target {
        self.path
    }
}

impl<K> DerefMut for AstKindPathGuard<'_, K>
where
    K: ParentKind,
{
    #[inline]
    fn deref_mut(&mut self) -> &mut Self::Target {
        self.path
    }
}

impl<K> Drop for AstKindPathGuard<'_, K>
where
    K: ParentKind,
{
    fn drop(&mut self) {
        self.path.path.pop();
    }
}

pub struct AstKindPathIndexGuard<'a, K>
where
    K: ParentKind,
{
    path: &'a mut AstKindPath<K>,
}

impl<K> Deref for AstKindPathIndexGuard<'_, K>
where
    K: ParentKind,
{
    type Target = AstKindPath<K>;

    #[inline]
    fn deref(&self) -> &Self::Target {
        self.path
    }
}

impl<K> DerefMut for AstKindPathIndexGuard<'_, K>
where
    K: ParentKind,
{
    #[inline]
    fn deref_mut(&mut self) -> &mut Self::Target {
        self.path
    }
}

impl<K> Drop for AstKindPathIndexGuard<'_, K>
where
    K: ParentKind,
{
    fn drop(&mut self) {
        self.path.path.last_mut().unwrap().set_index(usize::MAX);
    }
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct AstNodePath<N>
where
    N: NodeRef,
{
    kinds: AstKindPath<N::ParentKind>,
    path: Vec<N>,
}

impl<N> std::ops::Deref for AstNodePath<N>
where
    N: NodeRef,
{
    type Target = Vec<N>;

    fn deref(&self) -> &Self::Target {
        &self.path
    }
}

impl<N> Default for AstNodePath<N>
where
    N: NodeRef,
{
    fn default() -> Self {
        Self {
            kinds: Default::default(),
            path: Default::default(),
        }
    }
}

impl<N> AstNodePath<N>
where
    N: NodeRef,
{
    pub fn new(kinds: AstKindPath<N::ParentKind>, path: Vec<N>) -> Self {
        Self { kinds, path }
    }

    pub fn kinds(&self) -> &AstKindPath<N::ParentKind> {
        &self.kinds
    }

    pub fn with_guard(&mut self, node: N) -> AstNodePathGuard<N> {
        self.kinds.path.push(node.kind());
        self.path.push(node);

        AstNodePathGuard { path: self }
    }

    pub fn with_index_guard(&mut self, index: usize) -> AstNodePathIndexGuard<N> {
        self.kinds.path.last_mut().unwrap().set_index(index);
        self.path.last_mut().unwrap().set_index(index);

        AstNodePathIndexGuard { path: self }
    }

    #[deprecated = "Use with_guard instead"]
    pub fn with<F, Ret>(&mut self, node: N, op: F) -> Ret
    where
        F: for<'aa> FnOnce(&'aa mut AstNodePath<N>) -> Ret,
    {
        let kind = node.kind();

        self.kinds.path.push(kind);
        self.path.push(node);
        let ret = op(self);
        self.path.pop();
        self.kinds.path.pop();

        ret
    }

    #[deprecated = "Use with_index_guard instead"]
    pub fn with_index<F, Ret>(&mut self, index: usize, op: F) -> Ret
    where
        F: for<'aa> FnOnce(&'aa mut AstNodePath<N>) -> Ret,
    {
        self.kinds.path.last_mut().unwrap().set_index(index);
        self.path.last_mut().unwrap().set_index(index);

        let res = op(self);

        self.path.last_mut().unwrap().set_index(usize::MAX);
        self.kinds.path.last_mut().unwrap().set_index(usize::MAX);
        res
    }
}

pub trait NodeRef: Copy {
    type ParentKind: ParentKind;

    fn kind(&self) -> Self::ParentKind;

    fn set_index(&mut self, index: usize);
}

pub trait ParentKind: Copy {
    fn set_index(&mut self, index: usize);
}

pub struct AstNodePathGuard<'a, N>
where
    N: NodeRef,
{
    path: &'a mut AstNodePath<N>,
}

impl<N> Deref for AstNodePathGuard<'_, N>
where
    N: NodeRef,
{
    type Target = AstNodePath<N>;

    #[inline]
    fn deref(&self) -> &Self::Target {
        self.path
    }
}

impl<N> DerefMut for AstNodePathGuard<'_, N>
where
    N: NodeRef,
{
    #[inline]
    fn deref_mut(&mut self) -> &mut Self::Target {
        self.path
    }
}

impl<N> Drop for AstNodePathGuard<'_, N>
where
    N: NodeRef,
{
    fn drop(&mut self) {
        self.path.path.pop();
        self.path.kinds.path.pop();
    }
}

pub struct AstNodePathIndexGuard<'a, N>
where
    N: NodeRef,
{
    path: &'a mut AstNodePath<N>,
}

impl<N> Deref for AstNodePathIndexGuard<'_, N>
where
    N: NodeRef,
{
    type Target = AstNodePath<N>;

    #[inline]
    fn deref(&self) -> &Self::Target {
        self.path
    }
}

impl<N> DerefMut for AstNodePathIndexGuard<'_, N>
where
    N: NodeRef,
{
    #[inline]
    fn deref_mut(&mut self) -> &mut Self::Target {
        self.path
    }
}

impl<N> Drop for AstNodePathIndexGuard<'_, N>
where
    N: NodeRef,
{
    fn drop(&mut self) {
        self.path.path.last_mut().unwrap().set_index(usize::MAX);
        self.path
            .kinds
            .path
            .last_mut()
            .unwrap()
            .set_index(usize::MAX);
    }
}

/// NOT A PUBLIC API
#[doc(hidden)]
pub fn wrong_ast_path() {
    unsafe {
        debug_unreachable::debug_unreachable!("Wrong ast path");
    }
}
