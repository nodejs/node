//! Experimental high-level representation (HIR) for Diplomat.
//!
//! Enabled with the `"hir"` Cargo feature

mod attrs;
mod defs;
mod elision;
mod lifetimes;
mod lowering;
mod methods;
mod paths;
mod primitives;
mod ty_position;
mod type_context;
mod types;
pub use attrs::*;
pub use defs::*;
pub(super) use elision::*;
pub use lifetimes::*;
pub(super) use lowering::*;
pub use methods::*;
pub use paths::*;
pub use primitives::*;
pub use ty_position::*;
pub use type_context::*;
pub use types::*;

pub use lowering::{ErrorAndContext, ErrorContext, LoweringError};

pub use crate::ast::{Docs, DocsTypeReferenceSyntax, DocsUrlGenerator};
pub use strck::ident::rust::{Ident, IdentBuf};
