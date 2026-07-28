use crate::error::ffi::TemporalError;

#[diplomat::bridge]
#[diplomat::abi_rename = "temporal_rs_{0}"]
pub mod ffi {
    use diplomat_runtime::DiplomatOption;
    use temporal_rs::options;

    #[diplomat::enum_convert(options::Overflow)]
    pub enum ArithmeticOverflow {
        #[diplomat::attr(auto, default)]
        Constrain,
        Reject,
    }
    #[diplomat::enum_convert(options::Disambiguation)]
    pub enum Disambiguation {
        #[diplomat::attr(auto, default)]
        Compatible,
        Earlier,
        Later,
        Reject,
    }

    #[diplomat::enum_convert(options::DisplayCalendar)]
    pub enum DisplayCalendar {
        #[diplomat::attr(auto, default)]
        Auto,
        Always,
        Never,
        Critical,
    }

    #[diplomat::enum_convert(options::DisplayOffset)]
    pub enum DisplayOffset {
        #[diplomat::attr(auto, default)]
        Auto,
        Never,
    }

    #[diplomat::enum_convert(options::DisplayTimeZone)]
    pub enum DisplayTimeZone {
        #[diplomat::attr(auto, default)]
        Auto,
        Never,
        Critical,
    }

    #[diplomat::enum_convert(options::OffsetDisambiguation)]
    pub enum OffsetDisambiguation {
        Use,
        Prefer,
        Ignore,
        Reject,
    }

    #[diplomat::enum_convert(options::RoundingMode)]
    pub enum RoundingMode {
        Ceil,
        Floor,
        Expand,
        Trunc,
        HalfCeil,
        HalfFloor,
        HalfExpand,
        HalfTrunc,
        HalfEven,
    }

    #[diplomat::enum_convert(options::Unit)]
    pub enum Unit {
        #[diplomat::attr(auto, default)]
        Auto = 0,
        Nanosecond = 1,
        Microsecond = 2,
        Millisecond = 3,
        Second = 4,
        Minute = 5,
        Hour = 6,
        Day = 7,
        Week = 8,
        Month = 9,
        Year = 10,
    }

    #[diplomat::enum_convert(options::UnsignedRoundingMode)]
    pub enum UnsignedRoundingMode {
        Infinity,
        Zero,
        HalfInfinity,
        HalfZero,
        HalfEven,
    }

    #[diplomat::enum_convert(temporal_rs::provider::TransitionDirection)]
    pub enum TransitionDirection {
        Next,
        Previous,
    }

    pub struct Precision {
        /// Sets the precision to minute precision.
        pub is_minute: bool,
        /// Sets the number of digits. Auto when None. Has no effect if is_minute is set.
        pub precision: DiplomatOption<u8>,
    }

    pub struct RoundingOptions {
        pub largest_unit: DiplomatOption<Unit>,
        pub smallest_unit: DiplomatOption<Unit>,
        pub rounding_mode: DiplomatOption<RoundingMode>,
        pub increment: DiplomatOption<u32>,
    }

    pub struct ToStringRoundingOptions {
        pub precision: Precision,
        pub smallest_unit: DiplomatOption<Unit>,
        pub rounding_mode: DiplomatOption<RoundingMode>,
    }

    pub struct DifferenceSettings {
        pub largest_unit: DiplomatOption<Unit>,
        pub smallest_unit: DiplomatOption<Unit>,
        pub rounding_mode: DiplomatOption<RoundingMode>,
        pub increment: DiplomatOption<u32>,
    }
}

impl From<ffi::Precision> for temporal_rs::parsers::Precision {
    fn from(other: ffi::Precision) -> Self {
        if other.is_minute {
            Self::Minute
        } else if let Some(digit) = other.precision.into() {
            Self::Digit(digit)
        } else {
            Self::Auto
        }
    }
}

impl From<ffi::ToStringRoundingOptions> for temporal_rs::options::ToStringRoundingOptions {
    fn from(other: ffi::ToStringRoundingOptions) -> Self {
        Self {
            precision: other.precision.into(),
            smallest_unit: other.smallest_unit.into_converted_option(),
            rounding_mode: other.rounding_mode.into_converted_option(),
        }
    }
}

impl TryFrom<ffi::DifferenceSettings> for temporal_rs::options::DifferenceSettings {
    type Error = TemporalError;
    fn try_from(other: ffi::DifferenceSettings) -> Result<Self, TemporalError> {
        let mut ret = Self::default();
        ret.largest_unit = other.largest_unit.into_converted_option();
        ret.smallest_unit = other.smallest_unit.into_converted_option();
        ret.rounding_mode = other.rounding_mode.into_converted_option();
        if let Some(increment) = other.increment.into() {
            ret.increment = Some(temporal_rs::options::RoundingIncrement::try_new(increment)?);
        }
        Ok(ret)
    }
}
impl TryFrom<ffi::RoundingOptions> for temporal_rs::options::RoundingOptions {
    type Error = TemporalError;
    fn try_from(other: ffi::RoundingOptions) -> Result<Self, TemporalError> {
        let mut ret = Self::default();
        ret.largest_unit = other.largest_unit.into_converted_option();
        ret.smallest_unit = other.smallest_unit.into_converted_option();
        ret.rounding_mode = other.rounding_mode.into_converted_option();
        if let Some(increment) = other.increment.into() {
            ret.increment = Some(temporal_rs::options::RoundingIncrement::try_new(increment)?);
        }
        Ok(ret)
    }
}
