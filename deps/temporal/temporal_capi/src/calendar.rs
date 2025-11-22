#[diplomat::bridge]
#[diplomat::abi_rename = "temporal_rs_{0}"]
pub mod ffi {
    use crate::error::ffi::TemporalError;
    use alloc::boxed::Box;
    use diplomat_runtime::DiplomatStr;
    use icu_calendar::preferences::CalendarAlgorithm;

    #[diplomat::enum_convert(icu_calendar::AnyCalendarKind, needs_wildcard)]
    pub enum AnyCalendarKind {
        Buddhist,
        Chinese,
        Coptic,
        Dangi,
        Ethiopian,
        EthiopianAmeteAlem,
        Gregorian,
        Hebrew,
        Indian,
        HijriTabularTypeIIFriday,
        HijriSimulatedMecca,
        HijriTabularTypeIIThursday,
        HijriUmmAlQura,
        Iso,
        Japanese,
        JapaneseExtended,
        Persian,
        Roc,
    }

    impl AnyCalendarKind {
        pub fn get_for_str(s: &DiplomatStr) -> Option<Self> {
            let value = icu_locale::extensions::unicode::Value::try_from_utf8(s).ok()?;
            let algorithm = CalendarAlgorithm::try_from(&value).ok()?;
            match icu_calendar::AnyCalendarKind::try_from(algorithm) {
                Ok(c) => Some(c.into()),
                Err(()) if algorithm == CalendarAlgorithm::Hijri(None) => {
                    Some(Self::HijriTabularTypeIIFriday)
                }
                Err(()) => None,
            }
        }

        // https://tc39.es/proposal-temporal/#sec-temporal-parsetemporalcalendarstring
        pub fn parse_temporal_calendar_string(s: &DiplomatStr) -> Option<Self> {
            match temporal_rs::parsers::parse_allowed_calendar_formats(s) {
                Some([]) => Some(AnyCalendarKind::Iso),
                Some(result) => Self::get_for_str(result),
                None => Self::get_for_str(s),
            }
        }
    }

    #[diplomat::opaque]
    #[diplomat::transparent_convert]
    /// By and large one should not need to construct this type; it mostly exists
    /// to represent calendar data found on other Temporal types, obtained via `.calendar()`.
    pub struct Calendar(pub temporal_rs::Calendar);

    impl Calendar {
        pub fn try_new_constrain(kind: AnyCalendarKind) -> Box<Self> {
            Box::new(Calendar(temporal_rs::Calendar::new(kind.into())))
        }

        pub fn from_utf8(s: &DiplomatStr) -> Result<Box<Self>, TemporalError> {
            temporal_rs::Calendar::try_from_utf8(s)
                .map(|c| Box::new(Calendar(c)))
                .map_err(Into::into)
        }

        pub fn is_iso(&self) -> bool {
            self.0.is_iso()
        }

        pub fn identifier(&self) -> &'static str {
            self.0.identifier()
        }
        /// Returns the kind of this calendar
        #[inline]
        pub fn kind(&self) -> AnyCalendarKind {
            self.0.kind().into()
        }
    }
}
