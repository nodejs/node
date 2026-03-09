use std::{borrow::Cow, collections::hash_map::Entry};

use analyer_and_collector::AnalyzerAndCollector;
use rustc_hash::{FxHashMap, FxHashSet};
use swc_atoms::Atom;
use swc_common::{Mark, SyntaxContext};
use swc_ecma_ast::*;
use swc_ecma_utils::stack_size::maybe_grow_default;
use swc_ecma_visit::{noop_visit_mut_type, visit_mut_pass, VisitMut, VisitMutWith, VisitWith};

pub use self::eval::contains_eval;
#[cfg(feature = "concurrent-renamer")]
use self::renamer_concurrent::{Send, Sync};
#[cfg(not(feature = "concurrent-renamer"))]
use self::renamer_single::{Send, Sync};
use self::{analyzer::Analyzer, ops::Operator};
use crate::hygiene::Config;

mod analyer_and_collector;
mod analyzer;
mod eval;
mod ops;

pub trait Renamer: Send + Sync {
    /// See the [`RenamedVariable`] documentation, this type determines whether
    /// impls can be used with [`renamer`] or [`renamer_keep_contexts`] .
    type Target: RenamedVariable;

    /// Should reset `n` to 0 for each identifier?
    const RESET_N: bool;

    /// It should be true if you expect lots of collisions
    const MANGLE: bool;

    fn get_cached(&self) -> Option<Cow<FxHashMap<Id, Self::Target>>> {
        None
    }

    fn store_cache(&mut self, _update: &FxHashMap<Id, Self::Target>) {}

    /// Should increment `n`.
    fn new_name_for(&self, orig: &Id, n: &mut usize) -> Atom;

    fn unresolved_symbols(&self) -> Vec<Atom> {
        Default::default()
    }

    /// Return true if the identifier should be preserved.
    #[inline]
    fn preserve_name(&self, _orig: &Id) -> bool {
        false
    }
}

pub type RenameMap = FxHashMap<Id, Atom>;

pub fn rename<V: RenamedVariable>(map: &FxHashMap<Id, V>) -> impl '_ + Pass + VisitMut {
    rename_with_config(map, Default::default())
}

pub fn rename_with_config<V: RenamedVariable>(
    map: &FxHashMap<Id, V>,
    config: Config,
) -> impl '_ + Pass + VisitMut {
    visit_mut_pass(Operator {
        rename: map,
        config,
        extra: Default::default(),
    })
}

pub fn renamer<R>(config: Config, renamer: R) -> impl Pass + VisitMut
where
    R: Renamer<Target = Atom>,
{
    visit_mut_pass(RenamePass {
        config,
        renamer,
        preserved: Default::default(),
        unresolved: Default::default(),
        previous_cache: Default::default(),
        total_map: None,
        marker: std::marker::PhantomData::<Atom>,
    })
}

/// Create correct (unique) syntax contexts. Use this if you need the syntax
/// contexts produces by this pass (unlike the default `hygiene` pass which
/// removes them anyway.)
pub fn renamer_keep_contexts<R>(config: Config, renamer: R) -> impl Pass + VisitMut
where
    R: Renamer<Target = Id>,
{
    visit_mut_pass(RenamePass {
        config,
        renamer,
        preserved: Default::default(),
        unresolved: Default::default(),
        previous_cache: Default::default(),
        total_map: None,
        marker: std::marker::PhantomData::<Id>,
    })
}

mod private {
    use swc_atoms::Atom;
    use swc_ecma_ast::Id;

    pub trait Sealed {}

    impl Sealed for Atom {}
    impl Sealed for Id {}
}

/// A trait that is used to represent a renamed variable. For
/// `renamer_keep_contexts`, the syntax contexts of the replacements should be
/// correct (unique), while for `hygiene` (which calls `renamer`), the resulting
/// syntax contexts are irrelevant. This type is used to handle both cases
/// without code duplication by using `HashMap<Id, impl RenamedVariable>`
/// everywhere:
/// - For `renamer`, `HashMap<Id, Atom>` is used (and `SyntaxContext::empty()`
///   isn't store unnecessarily). All replaced idents have the same
///   SyntaxContext #0.
/// - For `renamer_keep_contexts`, `HashMap<Id, Id>` is used. All replaced
///   idents have a unique SyntaxContext.
pub trait RenamedVariable:
    private::Sealed + Clone + Sized + std::marker::Send + std::marker::Sync + 'static
{
    /// Potentially create a new private variable, depending on whether the
    /// consumer cares about the syntax context after the renaming.
    fn new_private(sym: Atom) -> Self;
    fn to_id(&self) -> Id;
    fn atom(&self) -> &Atom;
    fn ctxt(&self) -> SyntaxContext;
}
impl RenamedVariable for Atom {
    fn new_private(sym: Atom) -> Self {
        sym
    }

    fn to_id(&self) -> Id {
        (self.clone(), Default::default())
    }

    fn atom(&self) -> &Atom {
        self
    }

    fn ctxt(&self) -> SyntaxContext {
        Default::default()
    }
}
impl RenamedVariable for Id {
    fn new_private(sym: Atom) -> Self {
        (sym, SyntaxContext::empty().apply_mark(Mark::new()))
    }

    fn to_id(&self) -> Id {
        self.clone()
    }

    fn atom(&self) -> &Atom {
        &self.0
    }

    fn ctxt(&self) -> SyntaxContext {
        self.1
    }
}

#[derive(Debug, Default)]
struct RenamePass<R, V>
where
    R: Renamer<Target = V>,
    V: RenamedVariable,
{
    config: Config,
    renamer: R,

    preserved: FxHashSet<Id>,
    unresolved: FxHashSet<Atom>,

    previous_cache: FxHashMap<Id, V>,

    /// Used to store cache.
    ///
    /// [Some] if the [`Renamer::get_cached`] returns [Some].
    total_map: Option<FxHashMap<Id, V>>,

    marker: std::marker::PhantomData<V>,
}

impl<R, V> RenamePass<R, V>
where
    R: Renamer<Target = V>,
    V: RenamedVariable,
{
    fn get_map<N>(
        &mut self,
        node: &N,
        skip_one: bool,
        top_level: bool,
        has_eval: bool,
    ) -> FxHashMap<Id, V>
    where
        N: VisitWith<AnalyzerAndCollector>,
    {
        let (mut scope, unresolved) = analyer_and_collector::analyzer_and_collect_unresolved(
            node,
            has_eval,
            self.config.top_level_mark,
            skip_one,
            R::MANGLE,
        );

        scope.prepare_renaming();

        let mut unresolved = if !top_level {
            let mut set = self.unresolved.clone();
            set.extend(unresolved);
            Cow::Owned(set)
        } else {
            self.unresolved = unresolved;
            Cow::Borrowed(&self.unresolved)
        };

        if !self.preserved.is_empty() {
            unresolved
                .to_mut()
                .extend(self.preserved.iter().map(|v| v.0.clone()));
        }

        {
            let extra_unresolved = self.renamer.unresolved_symbols();

            if !extra_unresolved.is_empty() {
                unresolved.to_mut().extend(extra_unresolved);
            }
        }

        let mut map = FxHashMap::<Id, V>::default();

        if R::MANGLE {
            let cost = scope.rename_cost();
            scope.rename_in_mangle_mode(
                &self.renamer,
                &mut map,
                &self.previous_cache,
                &Default::default(),
                &self.preserved,
                &unresolved,
                cost > 1024,
            );
        } else {
            scope.rename_in_normal_mode(
                &self.renamer,
                &mut map,
                &self.previous_cache,
                &mut Default::default(),
                &self.preserved,
                &unresolved,
            );
        }

        if let Some(total_map) = &mut self.total_map {
            total_map.reserve(map.len());

            for (k, v) in &map {
                match total_map.entry(k.clone()) {
                    Entry::Occupied(old) => {
                        let old = old.get().to_id();
                        let new = v.to_id();
                        unreachable!(
                            "{} is already renamed to {}, but it's renamed as {}",
                            k.0, old.0, new.0
                        );
                    }
                    Entry::Vacant(e) => {
                        e.insert(v.clone());
                    }
                }
            }
        }

        map
    }

    fn load_cache(&mut self) {
        if let Some(cache) = self.renamer.get_cached() {
            self.previous_cache = cache.into_owned();
            self.total_map = Some(Default::default());
        }
    }
}

/// Mark a node as a unit of minification.
///
/// This is
macro_rules! unit {
    ($name:ident, $T:ty) => {
        /// Only called if `eval` exists
        fn $name(&mut self, n: &mut $T) {
            if !self.config.ignore_eval && contains_eval(n, true) {
                n.visit_mut_children_with(self);
            } else {
                let map = self.get_map(n, false, false, false);

                if !map.is_empty() {
                    n.visit_mut_with(&mut rename_with_config(&map, self.config.clone()));
                }
            }
        }
    };
}

impl<R, V> VisitMut for RenamePass<R, V>
where
    R: Renamer<Target = V>,
    V: RenamedVariable,
{
    noop_visit_mut_type!();

    unit!(visit_mut_arrow_expr, ArrowExpr);

    unit!(visit_mut_setter_prop, SetterProp);

    unit!(visit_mut_getter_prop, GetterProp);

    unit!(visit_mut_constructor, Constructor);

    unit!(visit_mut_fn_expr, FnExpr);

    unit!(visit_mut_method_prop, MethodProp);

    unit!(visit_mut_class_method, ClassMethod);

    unit!(visit_mut_private_method, PrivateMethod);

    fn visit_mut_fn_decl(&mut self, n: &mut FnDecl) {
        if !self.config.ignore_eval && contains_eval(n, true) {
            n.visit_mut_children_with(self);
        } else {
            let id = n.ident.to_id();
            let inserted = self.preserved.insert(id.clone());
            let map = self.get_map(n, true, false, false);

            if inserted {
                self.preserved.remove(&id);
            }

            if !map.is_empty() {
                n.visit_mut_with(&mut rename_with_config(&map, self.config.clone()));
            }
        }
    }

    fn visit_mut_class_decl(&mut self, n: &mut ClassDecl) {
        if !self.config.ignore_eval && contains_eval(n, true) {
            n.visit_mut_children_with(self);
        } else {
            let id = n.ident.to_id();
            let inserted = self.preserved.insert(id.clone());
            let map = self.get_map(n, true, false, false);

            if inserted {
                self.preserved.remove(&id);
            }

            if !map.is_empty() {
                n.visit_mut_with(&mut rename_with_config(&map, self.config.clone()));
            }
        }
    }

    fn visit_mut_default_decl(&mut self, n: &mut DefaultDecl) {
        match n {
            DefaultDecl::Class(n) => {
                n.visit_mut_children_with(self);
            }
            DefaultDecl::Fn(n) => {
                n.visit_mut_children_with(self);
            }
            DefaultDecl::TsInterfaceDecl(n) => {
                n.visit_mut_children_with(self);
            }
            #[cfg(swc_ast_unknown)]
            _ => (),
        }
    }

    fn visit_mut_expr(&mut self, n: &mut Expr) {
        maybe_grow_default(|| n.visit_mut_children_with(self));
    }

    fn visit_mut_module(&mut self, m: &mut Module) {
        self.load_cache();

        let has_eval = !self.config.ignore_eval && contains_eval(m, true);

        let map = self.get_map(m, false, true, has_eval);

        // If we have eval, we cannot rename a whole program at once.
        //
        // Still, we can, and should rename some identifiers, if the containing scope
        // (function-like nodes) does not have eval. This `eval` check includes
        // `eval` in children.
        //
        // We calculate the top level map first, rename children, and then rename the
        // top level.
        //
        //
        // Order:
        //
        // 1. Top level map calculation
        // 2. Per-unit map calculation
        // 3. Per-unit renaming
        // 4. Top level renaming
        //
        // This is because the top level map may contain a mapping which conflicts
        // with a map from one of the children.
        //
        // See https://github.com/swc-project/swc/pull/7615
        if has_eval {
            m.visit_mut_children_with(self);
        }

        if !map.is_empty() {
            m.visit_mut_with(&mut rename_with_config(&map, self.config.clone()));
        }

        if let Some(total_map) = &self.total_map {
            self.renamer.store_cache(total_map);
        }
    }

    fn visit_mut_script(&mut self, m: &mut Script) {
        self.load_cache();

        let has_eval = !self.config.ignore_eval && contains_eval(m, true);

        let map = self.get_map(m, false, true, has_eval);

        if has_eval {
            m.visit_mut_children_with(self);
        }

        if !map.is_empty() {
            m.visit_mut_with(&mut rename_with_config(&map, self.config.clone()));
        }

        if let Some(total_map) = &self.total_map {
            self.renamer.store_cache(total_map);
        }
    }
}

#[cfg(feature = "concurrent-renamer")]
mod renamer_concurrent {
    pub use std::marker::{Send, Sync};
}

#[cfg(not(feature = "concurrent-renamer"))]
mod renamer_single {
    /// Dummy trait because swc_common is in single thread mode.
    pub trait Send {}
    /// Dummy trait because swc_common is in single thread mode.
    pub trait Sync {}

    impl<T> Send for T where T: ?Sized {}
    impl<T> Sync for T where T: ?Sized {}
}
