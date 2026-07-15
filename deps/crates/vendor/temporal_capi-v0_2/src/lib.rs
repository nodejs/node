#![allow(unused)] // Until we add all the APIs
#![warn(unused_imports)] // But we want to clean up imports
#![allow(clippy::needless_lifetimes)] // Diplomat requires explicit lifetimes at times
#![allow(clippy::too_many_arguments)] // We're mapping APIs with the same argument size
#![allow(clippy::wrong_self_convention)] // Diplomat forces self conventions that may not always be ideal

//! This crate contains the original definitions of [`temporal_rs`] APIs consumed by [Diplomat](https://github.com/rust-diplomat/diplomat)
//! to generate FFI bindings. We currently generate bindings for C++ and `extern "C"` APIs.
//!
//! The APIs exposed by this crate are *not intended to be used from Rust* and as such this crate may change its Rust API
//! across compatible semver versions. In contrast, the `extern "C"` APIs and all the language bindings are stable within
//! the same major semver version.
//!
//! [`temporal_rs`]: http://crates.io/crates/temporal_rs

#![no_std]
#![cfg_attr(
    not(test),
    forbid(clippy::unwrap_used, clippy::expect_used, clippy::indexing_slicing)
)]

extern crate alloc;

#[macro_use]
pub mod provider;

pub mod calendar;
pub mod duration;
pub mod error;
pub mod instant;
pub mod options;

pub mod plain_date;
pub mod plain_date_time;
pub mod plain_month_day;
pub mod plain_time;
pub mod plain_year_month;
pub mod zoned_date_time;

pub mod time_zone;
