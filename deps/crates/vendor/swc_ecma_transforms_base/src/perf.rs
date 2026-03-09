use swc_common::util::move_map::MoveMap;
#[cfg(feature = "concurrent")]
use swc_common::{errors::HANDLER, GLOBALS};
use swc_ecma_ast::*;
pub use swc_ecma_utils::parallel::*;
use swc_ecma_visit::{Fold, FoldWith, Visit, VisitMut, VisitMutWith, VisitWith};

#[cfg(feature = "concurrent")]
use crate::helpers::Helpers;
#[cfg(feature = "concurrent")]
use crate::helpers::HELPERS;

pub trait Check: Visit + Default {
    fn should_handle(&self) -> bool;
}

pub fn should_work<C, T>(n: &T) -> bool
where
    C: Check,
    T: VisitWith<C>,
{
    let mut checker = C::default();
    n.visit_with(&mut checker);
    checker.should_handle()
}

pub trait ParExplode: Parallel {
    /// Invoked after visiting each statements.
    ///
    /// Implementor should not delete/prepend to `stmts`.
    fn after_one_stmt(&mut self, stmts: &mut Vec<Stmt>);

    /// Invoked after visiting each statements.
    ///
    /// Implementor should not delete/prepend to `stmts`.
    fn after_one_module_item(&mut self, stmts: &mut Vec<ModuleItem>);
}

pub trait ParVisit: Visit + Parallel {
    fn visit_par<N>(&mut self, threshold: usize, nodes: &[N])
    where
        N: Send + Sync + VisitWith<Self>;
}

#[cfg(feature = "concurrent")]
impl<T> ParVisit for T
where
    T: Visit + Parallel,
{
    fn visit_par<N>(&mut self, threshold: usize, nodes: &[N])
    where
        N: Send + Sync + VisitWith<Self>,
    {
        if nodes.len() >= threshold {
            HELPERS.with(|helpers| {
                let helpers = helpers.data();

                HANDLER.with(|handler| {
                    self.maybe_par(threshold, nodes, |visitor, node| {
                        let helpers = Helpers::from_data(helpers);
                        HELPERS.set(&helpers, || {
                            HANDLER.set(handler, || {
                                node.visit_with(visitor);
                            });
                        });
                    });
                })
            });
            return;
        }

        for n in nodes {
            n.visit_with(self);
        }
    }
}

pub trait ParVisitMut: VisitMut + Parallel {
    fn visit_mut_par<N>(&mut self, threshold: usize, nodes: &mut [N])
    where
        N: Send + Sync + VisitMutWith<Self>;
}

#[cfg(feature = "concurrent")]
impl<T> ParVisitMut for T
where
    T: VisitMut + Parallel,
{
    fn visit_mut_par<N>(&mut self, threshold: usize, nodes: &mut [N])
    where
        N: Send + Sync + VisitMutWith<Self>,
    {
        if nodes.len() >= threshold {
            HELPERS.with(|helpers| {
                let helpers = helpers.data();

                HANDLER.with(|handler| {
                    self.maybe_par(threshold, nodes, |visitor, node| {
                        let helpers = Helpers::from_data(helpers);
                        HELPERS.set(&helpers, || {
                            HANDLER.set(handler, || {
                                node.visit_mut_with(visitor);
                            });
                        });
                    });
                })
            });

            return;
        }

        for n in nodes {
            n.visit_mut_with(self);
        }
    }
}

pub trait ParFold: Fold + Parallel {
    fn fold_par<N>(&mut self, threshold: usize, nodes: Vec<N>) -> Vec<N>
    where
        N: Send + Sync + FoldWith<Self>;
}

#[cfg(feature = "concurrent")]
impl<T> ParFold for T
where
    T: Fold + Parallel,
{
    fn fold_par<N>(&mut self, threshold: usize, nodes: Vec<N>) -> Vec<N>
    where
        N: Send + Sync + FoldWith<Self>,
    {
        if nodes.len() >= threshold {
            use par_iter::prelude::*;

            let (visitor, nodes) = GLOBALS.with(|globals| {
                HELPERS.with(|helpers| {
                    let helpers = helpers.data();
                    HANDLER.with(|handler| {
                        nodes
                            .into_par_iter()
                            .map(|node| {
                                let helpers = Helpers::from_data(helpers);
                                GLOBALS.set(globals, || {
                                    HELPERS.set(&helpers, || {
                                        HANDLER.set(handler, || {
                                            let mut visitor = Parallel::create(&*self);
                                            let node = node.fold_with(&mut visitor);

                                            (visitor, node)
                                        })
                                    })
                                })
                            })
                            .fold(
                                || (Parallel::create(&*self), Vec::new()),
                                |mut a, b| {
                                    Parallel::merge(&mut a.0, b.0);

                                    a.1.push(b.1);

                                    a
                                },
                            )
                            .reduce(
                                || (Parallel::create(&*self), Vec::new()),
                                |mut a, b| {
                                    Parallel::merge(&mut a.0, b.0);

                                    a.1.extend(b.1);

                                    a
                                },
                            )
                    })
                })
            });

            Parallel::merge(self, visitor);

            return nodes;
        }

        nodes.move_map(|n| n.fold_with(self))
    }
}

#[cfg(not(feature = "concurrent"))]
impl<T> ParVisit for T
where
    T: Visit + Parallel,
{
    fn visit_par<N>(&mut self, _: usize, nodes: &[N])
    where
        N: Send + Sync + VisitWith<Self>,
    {
        for n in nodes {
            n.visit_with(self);
        }
    }
}

#[cfg(not(feature = "concurrent"))]
impl<T> ParVisitMut for T
where
    T: VisitMut + Parallel,
{
    fn visit_mut_par<N>(&mut self, _: usize, nodes: &mut [N])
    where
        N: Send + Sync + VisitMutWith<Self>,
    {
        for n in nodes {
            n.visit_mut_with(self);
        }
    }
}

#[cfg(not(feature = "concurrent"))]
impl<T> ParFold for T
where
    T: Fold + Parallel,
{
    fn fold_par<N>(&mut self, _: usize, nodes: Vec<N>) -> Vec<N>
    where
        N: Send + Sync + FoldWith<Self>,
    {
        nodes.move_map(|n| n.fold_with(self))
    }
}
