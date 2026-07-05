// The subset of Span's API stabilized in Rust 1.88.

#![cfg_attr(procmacro2_build_probe, no_std)]

extern crate alloc;
extern crate proc_macro;
extern crate std;

use alloc::string::String;
use proc_macro::Span;
use std::path::PathBuf;

pub fn file(this: &Span) -> String {
    this.file()
}

pub fn local_file(this: &Span) -> Option<PathBuf> {
    this.local_file()
}
