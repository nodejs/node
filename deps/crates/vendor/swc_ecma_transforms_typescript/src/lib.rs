#![deny(clippy::all)]
#![allow(clippy::vec_box)]
#![allow(clippy::mutable_key_type)]

pub use self::{strip_type::*, typescript::*};
mod config;
mod macros;
mod strip_import_export;
mod strip_type;
mod transform;
mod ts_enum;
pub mod typescript;
mod utils;
