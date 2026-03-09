//! Composable hooks for SWC ECMAScript visitors.
//!
//! This crate provides a hook-based API for composing AST visitors,
//! allowing you to build complex visitors by combining simple hooks.

#![cfg_attr(docsrs, feature(doc_cfg))]
#![deny(clippy::all)]
#![allow(clippy::ptr_arg)]

pub use swc_ecma_ast;
pub use swc_ecma_visit;

pub use crate::generated::*;
mod generated;

pub fn noop_hook() -> NoopHook {
    NoopHook
}

pub struct NoopHook;

impl<C> VisitHook<C> for NoopHook {}

impl<C> VisitMutHook<C> for NoopHook {}
