//! A compact, zero copy TZif file.
//!
//! NOTE: This representation does not follow the TZif specification
//! to full detail, but instead attempts to compress TZif data into
//! a functional, data driven equivalent.

#[cfg(feature = "datagen")]
mod datagen;
pub mod posix;
pub mod provider;

use zerotrie::ZeroAsciiIgnoreCaseTrie;
use zerovec::{vecs::Index32, VarZeroVec, ZeroVec};

use posix::PosixZone;
use provider::ZeroCompiledZoneInfo;

use crate::{self as timezone_provider, provider::NormalizerAndResolver, CompiledNormalizer};
compiled_zoneinfo_provider!(COMPILED_ZONEINFO_PROVIDER);

/// `ZeroCompiledTzdbProvider` is zero-copy compiled time zone database provider.
pub type ZeroCompiledTzdbProvider<'a> =
    NormalizerAndResolver<CompiledNormalizer, ZeroCompiledZoneInfo>;

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

/// A zero-copy TZif data struct for time zone data provided by IANA's tzdb (also known
/// as the Olsen database).
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
    /// The time in UTC epoch seconds where a transition occurs.
    pub transitions: ZeroVec<'data, i64>,
    /// An index identify the local time type for the corresponding transition.
    pub transition_types: ZeroVec<'data, u8>,
    /// The available local time types
    pub types: ZeroVec<'data, LocalTimeRecord>,
    /// The POSIX time zone data for this TZif
    pub posix: PosixZone,
    /// The available time zone designations.
    pub designations: ZeroVec<'data, char>,
}

#[zerovec::make_ule(LocalTimeRecordULE)]
#[derive(PartialEq, Eq, Default, Debug, Clone, Copy, PartialOrd, Ord)]
#[cfg_attr(
    feature = "datagen",
    derive(yoke::Yokeable, serde::Serialize, databake::Bake)
)]
#[cfg_attr(feature = "datagen", databake(path = timezone_provider::experimental_tzif))]
pub struct LocalTimeRecord {
    /// The offset from UTC in seconds
    pub offset: i64,
    /// Whether the current local time type is considered DST or not
    pub(crate) is_dst: bool,
    /// The index into the designations array.
    pub index: u8,
}
