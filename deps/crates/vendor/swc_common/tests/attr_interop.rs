//! Test that `#[span]` and `#[fold]` can be used at same time.
#![allow(unexpected_cfgs)]

use serde::{Deserialize, Serialize};
use swc_common::{ast_node, Span, Spanned};

#[ast_node("Class")]
// See https://github.com/rust-lang/rust/issues/44925
pub struct Class {
    #[span]
    pub has_span: HasSpan,
    pub s: String,
}

#[ast_node("Tuple")]
pub struct Tuple(#[span] HasSpan, u64, u64);

#[derive(Debug, Clone, PartialEq, Eq, Spanned, Serialize, Deserialize)]
#[cfg_attr(
    any(feature = "rkyv-impl"),
    derive(rkyv::Archive, rkyv::Serialize, rkyv::Deserialize)
)]
#[cfg_attr(
    feature = "rkyv-impl",
    rkyv(serialize_bounds(__S: rkyv::ser::Writer + rkyv::ser::Allocator,
        __S::Error: rkyv::rancor::Source))
)]
#[cfg_attr(feature = "rkyv-impl", derive(bytecheck::CheckBytes))]
#[cfg_attr(
    feature = "rkyv-impl",
    rkyv(deserialize_bounds(__D::Error: rkyv::rancor::Source))
)]
#[cfg_attr(
    feature = "rkyv-impl",
    bytecheck(bounds(
        __C: rkyv::validation::ArchiveContext
    ))
)]
#[cfg_attr(feature = "rkyv-impl", repr(C))]
#[cfg_attr(
    feature = "encoding-impl",
    derive(::swc_common::Encode, ::swc_common::Decode)
)]
pub struct HasSpan {
    #[cfg_attr(feature = "__rkyv", rkyv(omit_bounds))]
    pub span: Span,
}

#[ast_node]
pub enum Node {
    #[tag("Class")]
    Class(Class),
    #[tag("Tuple")]
    Tuple(Tuple),
}
