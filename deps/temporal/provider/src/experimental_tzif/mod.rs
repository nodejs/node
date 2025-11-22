//! A compact, zero copy TZif file.
//!
//! NOTE: This representation does not follow the TZif specification
//! to full detail, but instead attempts to compress TZif data into
//! a functional, data driven equivalent.

#[cfg(feature = "datagen")]
mod datagen;
pub mod posix;

use zerotrie::ZeroAsciiIgnoreCaseTrie;
use zerovec::{vecs::Index32, VarZeroVec, ZeroVec};

use posix::PosixZone;

use crate as timezone_provider;
compiled_zoneinfo_provider!(COMPILED_ZONEINFO_PROVIDER);

#[derive(Debug, Clone)]
#[cfg_attr(
    feature = "datagen",
    derive(yoke::Yokeable, serde::Serialize, databake::Bake)
)]
#[cfg_attr(feature = "datagen", databake(path = timezone_provider::experimental_tzif))]
pub struct ZoneInfoProvider<'data> {
    // IANA identifier map to TZif index.
    pub ids: ZeroAsciiIgnoreCaseTrie<ZeroVec<'data, u8>>,
    // Vector of TZif data
    pub tzifs: VarZeroVec<'data, ZeroTzifULE, Index32>,
}

impl ZoneInfoProvider<'_> {
    pub fn get(&self, identifier: &str) -> Option<&ZeroTzifULE> {
        let idx = self.ids.get(identifier)?;
        self.tzifs.get(idx)
    }
}

#[zerovec::make_varule(ZeroTzifULE)]
#[derive(PartialEq, Debug, Clone)]
#[zerovec::skip_derive(Ord)]
#[zerovec::derive(Debug)]
#[cfg_attr(
    feature = "datagen",
    derive(yoke::Yokeable, serde::Serialize, databake::Bake)
)]
#[cfg_attr(feature = "datagen", zerovec::derive(Serialize))]
#[cfg_attr(feature = "datagen", databake(path = timezone_provider::experimental_tzif))]
pub struct ZeroTzif<'data> {
    pub transitions: ZeroVec<'data, i64>,
    pub transition_types: ZeroVec<'data, u8>,
    // NOTE: zoneinfo64 does a fun little bitmap str
    pub types: ZeroVec<'data, LocalTimeRecord>,
    pub posix: PosixZone,
}

#[zerovec::make_ule(LocalTimeRecordULE)]
#[derive(PartialEq, Eq, Debug, Clone, Copy, PartialOrd, Ord)]
#[cfg_attr(
    feature = "datagen",
    derive(yoke::Yokeable, serde::Serialize, databake::Bake)
)]
#[cfg_attr(feature = "datagen", databake(path = timezone_provider::experimental_tzif))]
pub struct LocalTimeRecord {
    pub offset: i64,
}
