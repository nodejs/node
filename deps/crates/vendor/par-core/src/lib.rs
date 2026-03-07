//! # par-core
//!
//! A wrapper for various parallelization library for Rust.
//! This crate currently supports
//!
//! - [`chili`](https://github.com/dragostis/chili)
//! - [`rayon`](https://github.com/rayon-rs/rayon)
//! - Disable parallelization.
//!
//! # Usage
//!
//! If you are developing a library, you should not force the parallelization
//! library, and let the users choose the parallelization library.
//!
//! ## Final application
//!
//! If you are developing a final application, you can use cargo feature to
//! select the parallelization library.
//!
//! ### `chili`
//!
//! ```toml
//! [dependencies]
//! par-core = { version = "1.0.1", features = ["chili"] }
//! ```
//!
//! ### `rayon`
//!
//! ```toml
//! [dependencies]
//! par-core = { version = "1.0.1", features = ["rayon"] }
//! ```
//!
//! ### Disable parallelization
//!
//! ```toml
//! [dependencies]
//! par-core = { version = "1.0.1", default-features = false }
//! ```
//!
//! ## Library developers
//!
//! If you are developing a library, you can simply depend on `par-core` without
//! any features. **Note**: To prevent a small mistake of end-user making the
//! appplication slower, `par-core` emits a error message using a default
//! feature. So if you are a library developer, you should specify
//! `default-features = false`.
//!
//! ```toml
//! [dependencies]
//! par-core = { version = "1.0.1", default-features = false }
//! ```

#[cfg(all(not(feature = "chili"), not(feature = "rayon"), feature = "parallel"))]
compile_error!("You must enable `chili` or `rayon` feature if you want to use `parallel` feature");

#[cfg(all(feature = "chili", feature = "rayon"))]
compile_error!("You must enable `chili` or `rayon` feature, not both");

#[cfg(feature = "chili")]
mod par_chili {
    use std::{cell::RefCell, mem::transmute};

    use chili::Scope;

    thread_local! {
        static SCOPE: RefCell<Option<&'static mut Scope<'static>>> = Default::default();
    }

    #[inline]
    fn join_scoped<A, B, RA, RB>(scope: &mut Scope<'_>, oper_a: A, oper_b: B) -> (RA, RB)
    where
        A: Send + FnOnce() -> RA,
        B: Send + FnOnce() -> RB,
        RA: Send,
        RB: Send,
    {
        scope.join(
            |scope| {
                let old_scope = SCOPE.take();
                // SATETY: it will be only accessed during `oper_a`
                SCOPE.set(Some(unsafe { transmute::<&mut Scope, &mut Scope>(scope) }));

                let ra = oper_a();
                SCOPE.set(old_scope);

                ra
            },
            |scope| {
                let old_scope = SCOPE.take();
                // SATETY: it will be only accessed during `oper_b`
                SCOPE.set(Some(unsafe { transmute::<&mut Scope, &mut Scope>(scope) }));

                let rb = oper_b();
                SCOPE.set(old_scope);

                rb
            },
        )
    }

    #[inline]
    pub fn join<A, B, RA, RB>(oper_a: A, oper_b: B) -> (RA, RB)
    where
        A: Send + FnOnce() -> RA,
        B: Send + FnOnce() -> RB,
        RA: Send,
        RB: Send,
    {
        let old_scope: Option<&mut Scope<'_>> = SCOPE.take();
        match old_scope {
            Some(scope) => {
                let (ra, rb) = join_scoped(scope, oper_a, oper_b);
                SCOPE.set(Some(scope));
                (ra, rb)
            }
            None => join_scoped(&mut Scope::global(), oper_a, oper_b),
        }
    }
}

pub fn join<A, B, RA, RB>(oper_a: A, oper_b: B) -> (RA, RB)
where
    A: Send + FnOnce() -> RA,
    B: Send + FnOnce() -> RB,
    RA: Send,
    RB: Send,
{
    #[cfg(feature = "chili")]
    let (ra, rb) = par_chili::join(oper_a, oper_b);

    #[cfg(feature = "rayon")]
    let (ra, rb) = rayon::join(oper_a, oper_b);

    #[cfg(not(feature = "parallel"))]
    let (ra, rb) = (oper_a(), oper_b());

    (ra, rb)
}
