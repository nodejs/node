//! Various flavors of allocators

pub use self::{arena::Arena, global::Global, scoped::Scoped};

mod arena;
mod global;
mod scoped;
