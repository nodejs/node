// #![cfg_attr(test, deny(warnings))]
#![allow(clippy::mutable_key_type)]
#![cfg_attr(not(test), allow(unused))]

pub use self::resolver::resolver;

#[doc(hidden)]
pub mod ext;
pub mod fixer;
#[macro_use]
pub mod hygiene;
pub mod assumptions;
pub mod helpers;
#[doc(hidden)]
pub mod native;
pub mod perf;
pub mod quote;
pub mod rename;
mod resolver;
pub mod scope;
#[cfg(test)]
mod tests;
