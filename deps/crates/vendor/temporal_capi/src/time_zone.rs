use temporal_rs::provider::TimeZoneId;
use temporal_rs::UtcOffset;
use timezone_provider::provider::{NormalizedId, ResolvedId};

#[diplomat::bridge]
#[diplomat::abi_rename = "temporal_rs_{0}"]
pub mod ffi {
    use crate::error::ffi::TemporalError;
    use crate::provider::ffi::Provider;
    use core::fmt::Write;
    use diplomat_runtime::DiplomatWrite;

    /// A type representing a time zone over FFI.
    ///
    /// It is not recommended to directly manipulate the fields of this type.
    #[derive(Copy, Clone)]
    pub struct TimeZone {
        /// The UTC offset in minutes (is_iana_id = false)
        pub offset_minutes: i16,
        /// An id which can be used with the resolver (is_iana_id = true)
        pub resolved_id: usize,
        /// An id which can be used with the normalizer (is_iana_id = true)
        pub normalized_id: usize,
        /// Whether this is an IANA id or an offset
        pub is_iana_id: bool,
    }

    impl TimeZone {
        #[cfg(feature = "compiled_data")]
        pub fn try_from_identifier_str(ident: &DiplomatStr) -> Result<Self, TemporalError> {
            Self::try_from_identifier_str_with_provider(ident, &Provider::compiled())
        }
        pub fn try_from_identifier_str_with_provider<'p>(
            ident: &DiplomatStr,
            p: &Provider<'p>,
        ) -> Result<Self, TemporalError> {
            let Ok(ident) = core::str::from_utf8(ident) else {
                return Err(temporal_rs::TemporalError::range().into());
            };
            with_provider!(p, |p| {
                temporal_rs::TimeZone::try_from_identifier_str_with_provider(ident, p)
            })
            .map(Into::into)
            .map_err(Into::into)
        }
        pub fn try_from_offset_str(ident: &DiplomatStr) -> Result<Self, TemporalError> {
            temporal_rs::UtcOffset::from_utf8(ident)
                .map(|x| temporal_rs::TimeZone::UtcOffset(x).into())
                .map_err(Into::into)
        }
        #[cfg(feature = "compiled_data")]
        pub fn try_from_str(ident: &DiplomatStr) -> Result<Self, TemporalError> {
            Self::try_from_str_with_provider(ident, &Provider::compiled())
        }
        pub fn try_from_str_with_provider<'p>(
            ident: &DiplomatStr,
            p: &Provider<'p>,
        ) -> Result<Self, TemporalError> {
            let Ok(ident) = core::str::from_utf8(ident) else {
                return Err(temporal_rs::TemporalError::range().into());
            };
            with_provider!(p, |p| temporal_rs::TimeZone::try_from_str_with_provider(
                ident, p
            ))
            .map(Into::into)
            .map_err(Into::into)
        }

        #[cfg(feature = "compiled_data")]
        pub fn identifier(self, write: &mut DiplomatWrite) {
            let _ = self.identifier_with_provider(&Provider::compiled(), write);
        }

        pub fn identifier_with_provider<'p>(
            self,
            p: &Provider<'p>,
            write: &mut DiplomatWrite,
        ) -> Result<(), TemporalError> {
            // TODO ideally this would use Writeable instead of allocating
            let s =
                with_provider!(p, |p| self.tz().identifier_with_provider(p)).unwrap_or_default();

            // This can only fail in cases where the DiplomatWriteable is capped, we
            // don't care about that.
            let _ = write.write_str(&s);
            Ok(())
        }

        #[cfg(feature = "compiled_data")]
        pub fn utc() -> Self {
            temporal_rs::TimeZone::utc().into()
        }

        pub fn utc_with_provider<'p>(p: &Provider<'p>) -> Result<Self, TemporalError> {
            Ok(with_provider!(p, |p| { temporal_rs::TimeZone::utc_with_provider(p) }).into())
        }

        /// Create a TimeZone that represents +00:00
        ///
        /// This is the only way to infallibly make a TimeZone without compiled_data,
        /// and can be used as a fallback.
        pub fn zero() -> Self {
            temporal_rs::TimeZone::UtcOffset(Default::default()).into()
        }

        /// Get the primary time zone identifier corresponding to this time zone
        #[cfg(feature = "compiled_data")]
        pub fn primary_identifier(self) -> Result<Self, TemporalError> {
            self.primary_identifier_with_provider(&Provider::compiled())
        }
        pub fn primary_identifier_with_provider<'p>(
            self,
            p: &Provider<'p>,
        ) -> Result<Self, TemporalError> {
            with_provider!(p, |p| self.tz().primary_identifier_with_provider(p))
                .map(Into::into)
                .map_err(Into::into)
        }

        pub(crate) fn tz(self) -> temporal_rs::TimeZone {
            self.into()
        }
    }
}

impl From<ffi::TimeZone> for temporal_rs::TimeZone {
    fn from(other: ffi::TimeZone) -> Self {
        if other.is_iana_id {
            Self::IanaIdentifier(TimeZoneId {
                normalized: NormalizedId(other.normalized_id),
                resolved: ResolvedId(other.resolved_id),
            })
        } else {
            Self::UtcOffset(UtcOffset::from_minutes(other.offset_minutes))
        }
    }
}

impl From<temporal_rs::TimeZone> for ffi::TimeZone {
    fn from(other: temporal_rs::TimeZone) -> Self {
        match other {
            temporal_rs::TimeZone::UtcOffset(offset) => Self {
                offset_minutes: offset.minutes(),
                is_iana_id: false,
                normalized_id: 0,
                resolved_id: 0,
            },
            temporal_rs::TimeZone::IanaIdentifier(id) => Self {
                normalized_id: id.normalized.0,
                resolved_id: id.resolved.0,
                is_iana_id: true,
                offset_minutes: 0,
            },
        }
    }
}
