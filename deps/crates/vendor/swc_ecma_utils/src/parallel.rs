//! Module for parallel processing

use once_cell::sync::Lazy;
use par_core::join;
use swc_common::GLOBALS;
use swc_ecma_ast::*;

static CPU_COUNT: Lazy<usize> = Lazy::new(num_cpus::get);

pub fn cpu_count() -> usize {
    *CPU_COUNT
}

pub trait Parallel: swc_common::sync::Send + swc_common::sync::Sync {
    /// Used to create visitor.
    fn create(&self) -> Self;

    /// This can be called in anytime.
    fn merge(&mut self, other: Self);

    /// Invoked after visiting all [Stmt]s, possibly in parallel.
    ///
    ///
    /// Note: `visit_*_par` never calls this.
    fn after_stmts(&mut self, _stmts: &mut Vec<Stmt>) {}

    /// Invoked after visiting all [ModuleItem]s, possibly in parallel.
    ///
    ///
    /// Note: `visit_*_par` never calls this.
    fn after_module_items(&mut self, _stmts: &mut Vec<ModuleItem>) {}
}

pub trait ParallelExt: Parallel {
    /// Invoke `op` in parallel, if `swc_ecma_utils` is compiled with
    /// concurrent feature enabled and `nodes.len()` is bigger than threshold.
    ///
    ///
    /// This configures [GLOBALS], while not configuring [HANDLER] nor [HELPERS]
    fn maybe_par<I, F>(&mut self, threshold: usize, nodes: I, op: F)
    where
        I: IntoItems,
        F: Send + Sync + Fn(&mut Self, I::Elem),
    {
        self.maybe_par_idx(threshold, nodes, |v, _, n| op(v, n))
    }

    /// Invoke `op` in parallel, if `swc_ecma_utils` is compiled with
    /// concurrent feature enabled and `nodes.len()` is bigger than threshold.
    ///
    ///
    /// This configures [GLOBALS], while not configuring [HANDLER] nor [HELPERS]
    fn maybe_par_idx<I, F>(&mut self, threshold: usize, nodes: I, op: F)
    where
        I: IntoItems,
        F: Send + Sync + Fn(&mut Self, usize, I::Elem),
    {
        self.maybe_par_idx_raw(threshold, nodes.into_items(), &op)
    }

    /// If you don't have a special reason, use [`ParallelExt::maybe_par`] or
    /// [`ParallelExt::maybe_par_idx`] instead.
    fn maybe_par_idx_raw<I, F>(&mut self, threshold: usize, nodes: I, op: &F)
    where
        I: Items,
        F: Send + Sync + Fn(&mut Self, usize, I::Elem);
}

#[cfg(feature = "concurrent")]
impl<T> ParallelExt for T
where
    T: Parallel,
{
    fn maybe_par_idx_raw<I, F>(&mut self, threshold: usize, nodes: I, op: &F)
    where
        I: Items,
        F: Send + Sync + Fn(&mut Self, usize, I::Elem),
    {
        if nodes.len() >= threshold {
            GLOBALS.with(|globals| {
                let len = nodes.len();
                if len == 0 {
                    return;
                }

                if len == 1 {
                    op(self, 0, nodes.into_iter().next().unwrap());
                    return;
                }

                let (na, nb) = nodes.split_at(len / 2);
                let mut vb = Parallel::create(&*self);

                let (_, vb) = join(
                    || {
                        GLOBALS.set(globals, || {
                            self.maybe_par_idx_raw(threshold, na, op);
                        })
                    },
                    || {
                        GLOBALS.set(globals, || {
                            vb.maybe_par_idx_raw(threshold, nb, op);

                            vb
                        })
                    },
                );

                Parallel::merge(self, vb);
            });

            return;
        }

        for (idx, n) in nodes.into_iter().enumerate() {
            op(self, idx, n);
        }
    }
}

#[cfg(not(feature = "concurrent"))]
impl<T> ParallelExt for T
where
    T: Parallel,
{
    fn maybe_par_idx_raw<I, F>(&mut self, _threshold: usize, nodes: I, op: &F)
    where
        I: Items,
        F: Send + Sync + Fn(&mut Self, usize, I::Elem),
    {
        for (idx, n) in nodes.into_iter().enumerate() {
            op(self, idx, n);
        }
    }
}

use self::private::Sealed;

mod private {
    pub trait Sealed {}

    impl<T> Sealed for Vec<T> {}
    impl<T> Sealed for &mut Vec<T> {}
    impl<T> Sealed for &mut [T] {}
    impl<T> Sealed for &[T] {}
}
pub trait IntoItems: Sealed {
    type Elem;
    type Items: Items<Elem = Self::Elem>;

    fn into_items(self) -> Self::Items;
}

impl<T, I> IntoItems for I
where
    I: Items<Elem = T>,
{
    type Elem = T;
    type Items = I;

    fn into_items(self) -> Self::Items {
        self
    }
}

impl<'a, T> IntoItems for &'a mut Vec<T>
where
    T: Send + Sync,
{
    type Elem = &'a mut T;
    type Items = &'a mut [T];

    fn into_items(self) -> Self::Items {
        self
    }
}

/// This is considered as a private type and it's NOT A PUBLIC API.
#[allow(clippy::len_without_is_empty)]
pub trait Items: Sized + IntoIterator<Item = Self::Elem> + Send + Sealed {
    type Elem: Send + Sync;

    fn len(&self) -> usize;

    fn split_at(self, idx: usize) -> (Self, Self);
}

impl<T> Items for Vec<T>
where
    T: Send + Sync,
{
    type Elem = T;

    fn len(&self) -> usize {
        Vec::len(self)
    }

    fn split_at(mut self, at: usize) -> (Self, Self) {
        let b = self.split_off(at);

        (self, b)
    }
}

impl<'a, T> Items for &'a mut [T]
where
    T: Send + Sync,
{
    type Elem = &'a mut T;

    fn len(&self) -> usize {
        <[T]>::len(self)
    }

    fn split_at(self, at: usize) -> (Self, Self) {
        let (a, b) = self.split_at_mut(at);

        (a, b)
    }
}

impl<'a, T> Items for &'a [T]
where
    T: Send + Sync,
{
    type Elem = &'a T;

    fn len(&self) -> usize {
        <[T]>::len(self)
    }

    fn split_at(self, at: usize) -> (Self, Self) {
        let (a, b) = self.split_at(at);

        (a, b)
    }
}
//
