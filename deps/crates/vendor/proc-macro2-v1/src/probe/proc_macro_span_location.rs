// The subset of Span's API stabilized in Rust 1.88.

#![cfg_attr(procmacro2_build_probe, no_std)]

extern crate alloc;
extern crate proc_macro;
extern crate std;

use proc_macro::Span;

pub fn start(this: &Span) -> Span {
    this.start()
}

pub fn end(this: &Span) -> Span {
    this.end()
}

pub fn line(this: &Span) -> usize {
    this.line()
}

pub fn column(this: &Span) -> usize {
    this.column()
}
