# `bitvec` API Documentation

Rust release `1.54` stabilized the use of `#[doc = include_str!()]`, which
allows documentation to be sourced from external files. This directory contains
the Rustdoc API documentation for items whose text is larger than a comment
block warrants.

The files here use Rustdoc’s ability to resolve symbol paths as link references,
and so will not render correctly in other Markdown viewers. The target renderer
is Rustdoc, not CommonMark.

Module and type documentation should generally be moved to this directory;
function, struct field, and enum variant documentation should generally stay
in source.
