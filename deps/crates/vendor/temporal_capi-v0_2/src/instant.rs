#[diplomat::bridge]
#[diplomat::abi_rename = "temporal_rs_{0}"]
pub mod ffi {
    use crate::duration::ffi::Duration;
    use crate::error::ffi::TemporalError;
    use crate::options::ffi::{DifferenceSettings, RoundingOptions};
    use crate::zoned_date_time::ffi::ZonedDateTime;
    use alloc::boxed::Box;
    use alloc::string::String;
    use core::str::FromStr;
    use diplomat_runtime::{DiplomatStr, DiplomatStr16};

    use crate::options::ffi::ToStringRoundingOptions;
    use crate::provider::ffi::Provider;
    use crate::time_zone::ffi::TimeZone;

    #[diplomat::opaque]
    pub struct Instant(pub temporal_rs::Instant);

    /// For portability, we use two u64s instead of an i128.
    /// The high bit of the u64 is the sign.
    /// This cannot represent i128::MIN, and has a -0, but those are largely
    /// irrelevant for this purpose.
    ///
    /// This could potentially instead be a bit-by-bit split, or something else
    #[derive(Debug, Copy, Clone)]
    pub struct I128Nanoseconds {
        pub high: u64,
        pub low: u64,
    }

    impl I128Nanoseconds {
        pub fn is_valid(self) -> bool {
            let ns = i128::from(self);
            temporal_rs::unix_time::EpochNanoseconds::from(ns)
                .check_validity()
                .is_ok()
        }
    }

    impl Instant {
        pub fn try_new(ns: I128Nanoseconds) -> Result<Box<Self>, TemporalError> {
            temporal_rs::Instant::try_new(ns.into())
                .map(|c| Box::new(Self(c)))
                .map_err(Into::into)
        }

        pub fn from_epoch_milliseconds(
            epoch_milliseconds: i64,
        ) -> Result<Box<Self>, TemporalError> {
            temporal_rs::Instant::from_epoch_milliseconds(epoch_milliseconds)
                .map(|c| Box::new(Self(c)))
                .map_err(Into::into)
        }

        pub fn from_utf8(s: &DiplomatStr) -> Result<Box<Self>, TemporalError> {
            temporal_rs::Instant::from_utf8(s)
                .map(|c| Box::new(Self(c)))
                .map_err(Into::into)
        }

        pub fn from_utf16(s: &DiplomatStr16) -> Result<Box<Self>, TemporalError> {
            // TODO(#275) This should not need to convert
            let s = String::from_utf16(s).map_err(|_| temporal_rs::TemporalError::range())?;
            temporal_rs::Instant::from_str(&s)
                .map(|c| Box::new(Self(c)))
                .map_err(Into::into)
        }

        pub fn add(&self, duration: &Duration) -> Result<Box<Self>, TemporalError> {
            self.0
                .add(&duration.0)
                .map(|c| Box::new(Self(c)))
                .map_err(Into::into)
        }
        pub fn subtract(&self, duration: &Duration) -> Result<Box<Self>, TemporalError> {
            self.0
                .subtract(&duration.0)
                .map(|c| Box::new(Self(c)))
                .map_err(Into::into)
        }
        pub fn since(
            &self,
            other: &Self,
            settings: DifferenceSettings,
        ) -> Result<Box<Duration>, TemporalError> {
            self.0
                .since(&other.0, settings.try_into()?)
                .map(|c| Box::new(Duration(c)))
                .map_err(Into::into)
        }
        pub fn until(
            &self,
            other: &Self,
            settings: DifferenceSettings,
        ) -> Result<Box<Duration>, TemporalError> {
            self.0
                .until(&other.0, settings.try_into()?)
                .map(|c| Box::new(Duration(c)))
                .map_err(Into::into)
        }
        pub fn round(&self, options: RoundingOptions) -> Result<Box<Self>, TemporalError> {
            self.0
                .round(options.try_into()?)
                .map(|c| Box::new(Self(c)))
                .map_err(Into::into)
        }

        pub fn compare(&self, other: &Self) -> core::cmp::Ordering {
            self.0.cmp(&other.0)
        }

        pub fn equals(&self, other: &Self) -> bool {
            self.0 == other.0
        }

        pub fn epoch_milliseconds(&self) -> i64 {
            self.0.epoch_milliseconds()
        }

        pub fn epoch_nanoseconds(&self) -> I128Nanoseconds {
            let ns = self.0.epoch_nanoseconds().as_i128();
            ns.into()
        }

        // TODO: rename after 0.15/0.1.0 to remove the with_compiled_data
        #[cfg(feature = "compiled_data")]
        pub fn to_ixdtf_string_with_compiled_data(
            &self,
            zone: Option<TimeZone>,
            options: ToStringRoundingOptions,
            write: &mut DiplomatWrite,
        ) -> Result<(), TemporalError> {
            self.to_ixdtf_string_with_provider(zone, options, &Provider::compiled(), write)
        }
        pub fn to_ixdtf_string_with_provider<'p>(
            &self,
            zone: Option<TimeZone>,
            options: ToStringRoundingOptions,
            p: &Provider<'p>,
            write: &mut DiplomatWrite,
        ) -> Result<(), TemporalError> {
            use writeable::Writeable;
            with_provider!(p, |p| {
                let writeable = self.0.to_ixdtf_writeable_with_provider(
                    zone.map(Into::into),
                    options.into(),
                    p,
                )?;
                // This can only fail in cases where the DiplomatWriteable is capped, we
                // don't care about that.
                let _ = writeable.write_to(write);
            });
            Ok(())
        }

        #[cfg(feature = "compiled_data")]
        pub fn to_zoned_date_time_iso(
            &self,
            zone: TimeZone,
        ) -> Result<Box<ZonedDateTime>, TemporalError> {
            self.to_zoned_date_time_iso_with_provider(zone, &Provider::compiled())
        }
        pub fn to_zoned_date_time_iso_with_provider<'p>(
            &self,
            zone: TimeZone,
            p: &Provider<'p>,
        ) -> Result<Box<ZonedDateTime>, TemporalError> {
            with_provider!(p, |p| self
                .0
                .to_zoned_date_time_iso_with_provider(zone.into(), p))
            .map(|c| Box::new(ZonedDateTime(c)))
            .map_err(Into::into)
        }

        #[allow(clippy::should_implement_trait)]
        pub fn clone(&self) -> Box<Self> {
            Box::new(Self(self.0))
        }
    }
}

const U64_HIGH_BIT_MASK: u64 = 1 << 63;

impl From<ffi::I128Nanoseconds> for i128 {
    fn from(ns: ffi::I128Nanoseconds) -> Self {
        let is_neg = (ns.high & U64_HIGH_BIT_MASK) != 0;
        // Remove the bitflag
        let ns_high = (ns.high & !U64_HIGH_BIT_MASK) as u128;
        // Stick them together
        let total = ((ns_high << 64) + ns.low as u128) as i128;
        // Reintroduce the sign
        if is_neg {
            -total
        } else {
            total
        }
    }
}

impl From<i128> for ffi::I128Nanoseconds {
    fn from(ns: i128) -> Self {
        debug_assert!(
            ns != i128::MIN,
            "temporal_rs should never produce i128::MIN, it is out of valid range"
        );
        let is_neg = ns < 0;
        let ns = ns.unsigned_abs();

        let high = (ns >> 64) as u64;
        let low = (ns & u64::MAX as u128) as u64;
        let high = if (is_neg) {
            high | U64_HIGH_BIT_MASK
        } else {
            high
        };

        ffi::I128Nanoseconds { high, low }
    }
}

#[test]
fn test_i128_roundtrip() {
    #[track_caller]
    fn roundtrip(x: i128) {
        let ns = ffi::I128Nanoseconds::from(x);
        let round = i128::from(ns);
        assert_eq!(x, round, "{x} does not roundtrip via {ns:?}");
    }

    roundtrip(0);
    roundtrip(-1);
    roundtrip(1);
    roundtrip(100);
    roundtrip(1000);
    roundtrip(-1000);
    roundtrip(1000000000);
    roundtrip(-100000000);
    roundtrip(u64::MAX as i128);
    roundtrip(-(u64::MIN as i128));
    roundtrip(100 * (u64::MAX as i128));
    roundtrip(-100 * (u64::MAX as i128));
    roundtrip(i128::MIN + 1);
    roundtrip(i128::MAX);
    roundtrip(i128::MAX - 10);
}
