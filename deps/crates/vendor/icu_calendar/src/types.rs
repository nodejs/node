// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

//! This module contains various types used by `icu::calendar` and `icu::datetime`

#[doc(no_inline)]
pub use calendrical_calculations::rata_die::RataDie;
use core::fmt;
use tinystr::TinyAsciiStr;
use zerovec::ule::AsULE;

// Export the duration types from here
#[cfg(feature = "unstable")]
pub use crate::duration::{DateDuration, DateDurationUnit};
use crate::error::MonthCodeParseError;

#[cfg(feature = "unstable")]
pub use unstable::DateFields;
#[cfg(not(feature = "unstable"))]
pub(crate) use unstable::DateFields;

mod unstable {
    /// A bag of various ways of expressing the year, month, and/or day.
    ///
    /// Pass this into [`Date::try_from_fields`](crate::Date::try_from_fields).
    ///
    /// <div class="stab unstable">
    /// 🚧 This code is considered unstable; it may change at any time, in breaking or non-breaking ways,
    /// including in SemVer minor releases. Do not use this type unless you are prepared for things to occasionally break.
    ///
    /// Graduation tracking issue: [issue #7161](https://github.com/unicode-org/icu4x/issues/7161).
    /// </div>
    ///
    /// ✨ *Enabled with the `unstable` Cargo feature.*
    #[derive(Copy, Clone, PartialEq, Default)]
    #[non_exhaustive]
    pub struct DateFields<'a> {
        /// The era code as a UTF-8 string.
        ///
        /// The acceptable codes are defined by CLDR and documented on each calendar.
        ///
        /// If set, [`Self::era_year`] must also be set.
        ///
        /// # Examples
        ///
        /// To set the era field, use a byte string:
        ///
        /// ```
        /// use icu::calendar::types::DateFields;
        ///
        /// let mut fields = DateFields::default();
        ///
        /// // As a byte string literal:
        /// fields.era = Some(b"reiwa");
        ///
        /// // Using str::as_bytes:
        /// fields.era = Some("reiwa".as_bytes());
        /// ```
        ///
        /// For a full example, see [`Self::extended_year`].
        pub era: Option<&'a [u8]>,
        /// The numeric year in [`Self::era`].
        ///
        /// If set, [`Self::era`] must also be set.
        ///
        /// For an example, see [`Self::extended_year`].
        pub era_year: Option<i32>,
        /// See [`Date::extended_year()`](crate::Date::extended_year).
        ///
        /// If both this and [`Self::era`]/[`Self::era_year`] are set, they must
        /// refer to the same year.
        ///
        /// # Examples
        ///
        /// Either `extended_year` or `era` + `era_year` can be used in DateFields:
        ///
        /// ```
        /// use icu::calendar::cal::Japanese;
        /// use icu::calendar::types::DateFields;
        /// use icu::calendar::Date;
        ///
        /// let mut fields1 = DateFields::default();
        /// fields1.era = Some(b"reiwa");
        /// fields1.era_year = Some(7);
        /// fields1.ordinal_month = Some(1);
        /// fields1.day = Some(1);
        ///
        /// let date1 =
        ///     Date::try_from_fields(fields1, Default::default(), Japanese::new())
        ///         .expect("a well-defined Japanese date from era year");
        ///
        /// let mut fields2 = DateFields::default();
        /// fields2.extended_year = Some(2025);
        /// fields2.ordinal_month = Some(1);
        /// fields2.day = Some(1);
        ///
        /// let date2 =
        ///     Date::try_from_fields(fields2, Default::default(), Japanese::new())
        ///         .expect("a well-defined Japanese date from extended year");
        ///
        /// assert_eq!(date1, date2);
        ///
        /// let year_info = date1.year().era().unwrap();
        /// assert_eq!(year_info.year, 7);
        /// assert_eq!(year_info.era.as_str(), "reiwa");
        /// assert_eq!(year_info.extended_year, 2025);
        /// ```
        pub extended_year: Option<i32>,
        /// The month code representing a valid month in this calendar year,
        /// as a UTF-8 string.
        ///
        /// See [`MonthCode`](crate::types::MonthCode) for information on the syntax.
        ///
        /// # Examples
        ///
        /// To set the month code field, use a byte string:
        ///
        /// ```
        /// use icu::calendar::types::DateFields;
        ///
        /// let mut fields = DateFields::default();
        ///
        /// // As a byte string literal:
        /// fields.era = Some(b"M02L");
        ///
        /// // Using str::as_bytes:
        /// fields.era = Some("M02L".as_bytes());
        /// ```
        ///
        /// For a full example, see [`Self::ordinal_month`].
        pub month_code: Option<&'a [u8]>,
        /// See [`MonthInfo::ordinal`](crate::types::MonthInfo::ordinal).
        ///
        /// If both this and [`Self::month_code`] are set, they must refer to
        /// the same month.
        ///
        /// Note: using [`Self::month_code`] is recommended, because the ordinal month numbers
        /// can vary from year to year, as illustrated in the following example.
        ///
        /// # Examples
        ///
        /// Either `month_code` or `ordinal_month` can be used in DateFields, but they
        /// might not resolve to the same month number:
        ///
        /// ```
        /// use icu::calendar::cal::ChineseTraditional;
        /// use icu::calendar::types::DateFields;
        /// use icu::calendar::Date;
        ///
        /// // The 2023 Year of the Rabbit had a leap month after the 2nd month.
        /// let mut fields1 = DateFields::default();
        /// fields1.extended_year = Some(2023);
        /// fields1.month_code = Some(b"M02L");
        /// fields1.day = Some(1);
        ///
        /// let date1 = Date::try_from_fields(
        ///     fields1,
        ///     Default::default(),
        ///     ChineseTraditional::new(),
        /// )
        /// .expect("a well-defined Chinese date from month code");
        ///
        /// let mut fields2 = DateFields::default();
        /// fields2.extended_year = Some(2023);
        /// fields2.ordinal_month = Some(3);
        /// fields2.day = Some(1);
        ///
        /// let date2 = Date::try_from_fields(
        ///     fields2,
        ///     Default::default(),
        ///     ChineseTraditional::new(),
        /// )
        /// .expect("a well-defined Chinese date from ordinal month");
        ///
        /// assert_eq!(date1, date2);
        ///
        /// let month_info = date1.month();
        /// assert_eq!(month_info.ordinal, 3);
        /// assert_eq!(month_info.standard_code.0, "M02L");
        /// ```
        pub ordinal_month: Option<u8>,
        /// See [`DayOfMonth`](crate::types::DayOfMonth).
        pub day: Option<u8>,
    }
}

// Custom impl to stringify era and month_code where possible.
impl fmt::Debug for DateFields<'_> {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        // Ensures we catch future fields
        let Self {
            era,
            era_year,
            extended_year,
            month_code,
            ordinal_month,
            day,
        } = *self;
        let mut builder = f.debug_struct("DateFields");
        if let Some(s) = era.and_then(|s| core::str::from_utf8(s).ok()) {
            builder.field("era", &Some(s));
        } else {
            builder.field("era", &era);
        }
        builder.field("era_year", &era_year);
        builder.field("extended_year", &extended_year);
        if let Some(s) = month_code.and_then(|s| core::str::from_utf8(s).ok()) {
            builder.field("month_code", &Some(s));
        } else {
            builder.field("month_code", &month_code);
        }
        builder.field("ordinal_month", &ordinal_month);
        builder.field("day", &day);
        builder.finish()
    }
}

/// The type of year: Calendars like Chinese don't have an era and instead format with cyclic years.
#[derive(Copy, Clone, Debug, PartialEq)]
#[non_exhaustive]
pub enum YearInfo {
    /// An era and a year in that era
    Era(EraYear),
    /// A cyclic year, and the related ISO year
    ///
    /// Knowing the cyclic year is typically not enough to pinpoint a date, however cyclic calendars
    /// don't typically use eras, so disambiguation can be done by saying things like "Year 甲辰 (2024)"
    Cyclic(CyclicYear),
}

impl From<EraYear> for YearInfo {
    fn from(value: EraYear) -> Self {
        Self::Era(value)
    }
}

impl From<CyclicYear> for YearInfo {
    fn from(value: CyclicYear) -> Self {
        Self::Cyclic(value)
    }
}

impl YearInfo {
    /// Get *some* year number that can be displayed
    ///
    /// Gets the era year for era calendars, and the related ISO year for cyclic calendars.
    pub fn era_year_or_related_iso(self) -> i32 {
        match self {
            YearInfo::Era(e) => e.year,
            YearInfo::Cyclic(c) => c.related_iso,
        }
    }

    /// Get the extended year (See [`Date::extended_year`](crate::Date::extended_year))
    /// for more information
    pub fn extended_year(self) -> i32 {
        match self {
            YearInfo::Era(e) => e.extended_year,
            YearInfo::Cyclic(c) => c.related_iso,
        }
    }

    /// Get the era year information, if available
    pub fn era(self) -> Option<EraYear> {
        match self {
            Self::Era(e) => Some(e),
            Self::Cyclic(_) => None,
        }
    }

    /// Get the cyclic year informat, if available
    pub fn cyclic(self) -> Option<CyclicYear> {
        match self {
            Self::Era(_) => None,
            Self::Cyclic(c) => Some(c),
        }
    }
}

/// Defines whether the era or century is required to interpret the year.
///
/// For example 2024 AD can be formatted as `2024`, or even `24`, but 1931 AD
/// should not be formatted as `31`, and 2024 BC should not be formatted as `2024`.
#[derive(Copy, Clone, Debug, PartialEq)]
#[allow(clippy::exhaustive_enums)] // logically complete
pub enum YearAmbiguity {
    /// The year is unambiguous without a century or era.
    Unambiguous,
    /// The century is required, the era may be included.
    CenturyRequired,
    /// The era is required, the century may be included.
    EraRequired,
    /// The century and era are required.
    EraAndCenturyRequired,
}

/// Year information for a year that is specified with an era
#[derive(Copy, Clone, Debug, PartialEq)]
#[non_exhaustive]
pub struct EraYear {
    /// The numeric year in that era
    pub year: i32,
    /// See [`YearInfo::extended_year()`]
    pub extended_year: i32,
    /// The era code as defined by CLDR, expect for cases where CLDR does not define a code.
    pub era: TinyAsciiStr<16>,
    /// An era index, for calendars with a small set of eras.
    ///
    /// The only guarantee we make is that these values are stable. These do *not*
    /// match the indices produced by ICU4C or CLDR.
    ///
    /// These are used by ICU4X datetime formatting for efficiently storing data.
    pub era_index: Option<u8>,
    /// The ambiguity of the era/year combination
    pub ambiguity: YearAmbiguity,
}

/// Year information for a year that is specified as a cyclic year
#[derive(Copy, Clone, Debug, PartialEq)]
#[non_exhaustive]
pub struct CyclicYear {
    /// The year in the cycle, 1-based
    pub year: u8,
    /// The ISO year corresponding to this year
    pub related_iso: i32,
}

/// Representation of a month in a year
///
/// Month codes typically look like `M01`, `M02`, etc, but can handle leap months
/// (`M03L`) in lunar calendars. Solar calendars will have codes between `M01` and `M12`
/// potentially with an `M13` for epagomenal months. Check the docs for a particular calendar
/// for details on what its month codes are.
///
/// Month codes are shared with Temporal, [see Temporal proposal][era-proposal].
///
/// [era-proposal]: https://tc39.es/proposal-intl-era-monthcode/
#[derive(Clone, Copy, Debug, PartialEq, Eq, PartialOrd, Ord, Hash)]
#[allow(clippy::exhaustive_structs)] // this is a newtype
#[cfg_attr(feature = "datagen", derive(serde::Serialize, databake::Bake))]
#[cfg_attr(feature = "datagen", databake(path = icu_calendar::types))]
#[cfg_attr(feature = "serde", derive(serde::Deserialize))]
pub struct MonthCode(pub TinyAsciiStr<4>);

impl MonthCode {
    /// Returns an option which is `Some` containing the non-month version of a leap month
    /// if the [`MonthCode`] this method is called upon is a leap month, and `None` otherwise.
    /// This method assumes the [`MonthCode`] is valid.
    #[deprecated(since = "2.1.0")]
    pub fn get_normal_if_leap(self) -> Option<MonthCode> {
        let bytes = self.0.all_bytes();
        if bytes[3] == b'L' {
            Some(MonthCode(TinyAsciiStr::try_from_utf8(&bytes[0..3]).ok()?))
        } else {
            None
        }
    }

    #[deprecated(since = "2.1.0")]
    /// Get the month number and whether or not it is leap from the month code
    pub fn parsed(self) -> Option<(u8, bool)> {
        ValidMonthCode::try_from_utf8(self.0.as_bytes())
            .ok()
            .map(ValidMonthCode::to_tuple)
    }

    /// Construct a "normal" month code given a number ("Mxx").
    ///
    /// Returns an error for months greater than 99
    pub fn new_normal(number: u8) -> Option<Self> {
        (1..=99)
            .contains(&number)
            .then(|| ValidMonthCode::new_unchecked(number, false).to_month_code())
    }

    /// Construct a "leap" month code given a number ("MxxL").
    ///
    /// Returns an error for months greater than 99
    pub fn new_leap(number: u8) -> Option<Self> {
        (1..=99)
            .contains(&number)
            .then(|| ValidMonthCode::new_unchecked(number, true).to_month_code())
    }
}

#[test]
fn test_get_normal_month_code_if_leap() {
    #![allow(deprecated)]
    assert_eq!(
        MonthCode::new_leap(1).unwrap().get_normal_if_leap(),
        MonthCode::new_normal(1)
    );

    assert_eq!(
        MonthCode::new_leap(11).unwrap().get_normal_if_leap(),
        MonthCode::new_normal(11)
    );

    assert_eq!(
        MonthCode::new_normal(10).unwrap().get_normal_if_leap(),
        None
    );
}

impl AsULE for MonthCode {
    type ULE = TinyAsciiStr<4>;
    fn to_unaligned(self) -> TinyAsciiStr<4> {
        self.0
    }
    fn from_unaligned(u: TinyAsciiStr<4>) -> Self {
        Self(u)
    }
}

#[cfg(feature = "alloc")]
impl<'a> zerovec::maps::ZeroMapKV<'a> for MonthCode {
    type Container = zerovec::ZeroVec<'a, MonthCode>;
    type Slice = zerovec::ZeroSlice<MonthCode>;
    type GetType = <MonthCode as AsULE>::ULE;
    type OwnedType = MonthCode;
}

impl fmt::Display for MonthCode {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        write!(f, "{}", self.0)
    }
}

/// A [`MonthCode`] that has been parsed into its internal representation.
#[derive(Copy, Clone, Debug, PartialEq)]
pub(crate) struct ValidMonthCode {
    /// Month number between 0 and 99
    number: u8,
    is_leap: bool,
}

impl ValidMonthCode {
    #[inline]
    pub(crate) fn try_from_utf8(bytes: &[u8]) -> Result<Self, MonthCodeParseError> {
        match *bytes {
            [b'M', tens, ones] => Ok(Self {
                number: (tens - b'0') * 10 + ones - b'0',
                is_leap: false,
            }),
            [b'M', tens, ones, b'L'] => Ok(Self {
                number: (tens - b'0') * 10 + ones - b'0',
                is_leap: true,
            }),
            _ => Err(MonthCodeParseError::InvalidSyntax),
        }
    }

    /// Create a new ValidMonthCode without checking that the number is between 1 and 99
    #[inline]
    pub(crate) const fn new_unchecked(number: u8, is_leap: bool) -> Self {
        debug_assert!(1 <= number && number <= 99);
        Self { number, is_leap }
    }

    /// Returns the month number according to the month code.
    ///
    /// This is NOT the same as the ordinal month!
    ///
    /// # Examples
    ///
    /// ```ignore
    /// use icu::calendar::Date;
    /// use icu::calendar::cal::Hebrew;
    ///
    /// let hebrew_date = Date::try_new_iso(2024, 7, 1).unwrap().to_calendar(Hebrew);
    /// let month_info = hebrew_date.month();
    ///
    /// // Hebrew year 5784 was a leap year, so the ordinal month and month number diverge.
    /// assert_eq!(month_info.ordinal, 10);
    /// assert_eq!(month_info.valid_month_code.number(), 9);
    /// ```
    #[inline]
    pub fn number(self) -> u8 {
        self.number
    }

    /// Returns whether the month is a leap month.
    ///
    /// This is true for intercalary months in [`Hebrew`] and [`LunarChinese`].
    ///
    /// [`Hebrew`]: crate::cal::Hebrew
    /// [`LunarChinese`]: crate::cal::LunarChinese
    #[inline]
    pub fn is_leap(self) -> bool {
        self.is_leap
    }

    #[inline]
    pub(crate) fn to_tuple(self) -> (u8, bool) {
        (self.number, self.is_leap)
    }

    pub(crate) fn to_month_code(self) -> MonthCode {
        #[allow(clippy::unwrap_used)] // by construction
        MonthCode(
            TinyAsciiStr::try_from_raw([
                b'M',
                b'0' + self.number / 10,
                b'0' + self.number % 10,
                if self.is_leap { b'L' } else { 0 },
            ])
            .unwrap(),
        )
    }
}

/// Representation of a formattable month.
#[derive(Copy, Clone, Debug, PartialEq)]
#[non_exhaustive]
pub struct MonthInfo {
    /// The month number in this given year. For calendars with leap months, all months after
    /// the leap month will end up with an incremented number.
    ///
    /// In general, prefer using the month code in generic code.
    pub ordinal: u8,

    /// The month code, used to distinguish months during leap years.
    ///
    /// Round-trips through `Date` constructors like [`Date::try_new_from_codes`] and [`Date::try_from_fields`].
    ///
    /// This follows [Temporal's specification](https://tc39.es/proposal-intl-era-monthcode/#table-additional-month-codes).
    /// Months considered the "same" have the same code: This means that the Hebrew months "Adar" and "Adar II" ("Adar, but during a leap year")
    /// are considered the same month and have the code M05.
    ///
    /// [`Date::try_new_from_codes`]: crate::Date::try_new_from_codes
    /// [`Date::try_from_fields`]: crate::Date::try_from_fields
    pub standard_code: MonthCode,

    /// Same as [`Self::standard_code`] but with invariants validated.
    pub(crate) valid_standard_code: ValidMonthCode,

    /// A month code, useable for formatting.
    ///
    /// Does NOT necessarily round-trip through `Date` constructors like [`Date::try_new_from_codes`] and [`Date::try_from_fields`].
    ///
    /// This may not necessarily be the canonical month code for a month in cases where a month has different
    /// formatting in a leap year, for example Adar/Adar II in the Hebrew calendar in a leap year has
    /// the standard code M06, but for formatting specifically the Hebrew calendar will return M06L since it is formatted
    /// differently.
    ///
    /// [`Date::try_new_from_codes`]: crate::Date::try_new_from_codes
    /// [`Date::try_from_fields`]: crate::Date::try_from_fields
    pub formatting_code: MonthCode,

    /// Same as [`Self::formatting_code`] but with invariants validated.
    pub(crate) valid_formatting_code: ValidMonthCode,
}

impl MonthInfo {
    pub(crate) fn non_lunisolar(number: u8) -> Self {
        Self::for_code_and_ordinal(ValidMonthCode::new_unchecked(number, false), number)
    }

    pub(crate) fn for_code_and_ordinal(code: ValidMonthCode, ordinal: u8) -> Self {
        Self {
            ordinal,
            standard_code: code.to_month_code(),
            valid_standard_code: code,
            formatting_code: code.to_month_code(),
            valid_formatting_code: code,
        }
    }

    /// Gets the month number. A month number N is not necessarily the Nth month in the year
    /// if there are leap months in the year, rather it is associated with the Nth month of a "regular"
    /// year. There may be multiple month Ns in a year
    pub fn month_number(self) -> u8 {
        self.valid_standard_code.number()
    }

    /// Get whether the month is a leap month
    pub fn is_leap(self) -> bool {
        self.valid_standard_code.is_leap()
    }

    #[doc(hidden)]
    pub fn formatting_month_number(self) -> u8 {
        self.valid_formatting_code.number()
    }

    #[doc(hidden)]
    pub fn formatting_is_leap(self) -> bool {
        self.valid_formatting_code.is_leap()
    }
}

/// The current day of the year, 1-based.
#[derive(Copy, Clone, Debug, PartialEq)]
#[allow(clippy::exhaustive_structs)] // this is a newtype
pub struct DayOfYear(pub u16);

/// A 1-based day number in a month.
#[allow(clippy::exhaustive_structs)] // this is a newtype
#[derive(Clone, Copy, Debug, PartialEq)]
pub struct DayOfMonth(pub u8);

/// A week number in a year
#[derive(Clone, Copy, Debug, PartialEq)]
#[allow(clippy::exhaustive_structs)] // this is a newtype
pub struct IsoWeekOfYear {
    /// The 1-based ISO week number
    pub week_number: u8,
    /// The ISO year
    pub iso_year: i32,
}

/// A day of week in month. 1-based.
#[derive(Clone, Copy, Debug, PartialEq)]
#[allow(clippy::exhaustive_structs)] // this is a newtype
pub struct DayOfWeekInMonth(pub u8);

impl From<DayOfMonth> for DayOfWeekInMonth {
    fn from(day_of_month: DayOfMonth) -> Self {
        DayOfWeekInMonth(1 + ((day_of_month.0 - 1) / 7))
    }
}

#[test]
fn test_day_of_week_in_month() {
    assert_eq!(DayOfWeekInMonth::from(DayOfMonth(1)).0, 1);
    assert_eq!(DayOfWeekInMonth::from(DayOfMonth(7)).0, 1);
    assert_eq!(DayOfWeekInMonth::from(DayOfMonth(8)).0, 2);
}

/// A weekday in a 7-day week, according to ISO-8601.
///
/// The discriminant values correspond to ISO-8601 weekday numbers (Monday = 1, Sunday = 7).
///
/// # Examples
///
/// ```
/// use icu::calendar::types::Weekday;
///
/// assert_eq!(1, Weekday::Monday as usize);
/// assert_eq!(7, Weekday::Sunday as usize);
/// ```
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
#[allow(missing_docs)] // The weekday variants should be self-obvious.
#[repr(i8)]
#[cfg_attr(feature = "datagen", derive(serde::Serialize, databake::Bake))]
#[cfg_attr(feature = "datagen", databake(path = icu_calendar::types))]
#[cfg_attr(feature = "serde", derive(serde::Deserialize))]
#[allow(clippy::exhaustive_enums)] // This is stable
pub enum Weekday {
    Monday = 1,
    Tuesday,
    Wednesday,
    Thursday,
    Friday,
    Saturday,
    Sunday,
}

// RD 0 is a Sunday
const SUNDAY: RataDie = RataDie::new(0);

impl From<RataDie> for Weekday {
    fn from(value: RataDie) -> Self {
        use Weekday::*;
        match (value - SUNDAY).rem_euclid(7) {
            0 => Sunday,
            1 => Monday,
            2 => Tuesday,
            3 => Wednesday,
            4 => Thursday,
            5 => Friday,
            6 => Saturday,
            _ => unreachable!(),
        }
    }
}

impl Weekday {
    /// Convert from an ISO-8601 weekday number to an [`Weekday`] enum. 0 is automatically converted
    /// to 7 (Sunday). If the number is out of range, it is interpreted modulo 7.
    ///
    /// # Examples
    ///
    /// ```
    /// use icu::calendar::types::Weekday;
    ///
    /// assert_eq!(Weekday::Sunday, Weekday::from_days_since_sunday(0));
    /// assert_eq!(Weekday::Monday, Weekday::from_days_since_sunday(1));
    /// assert_eq!(Weekday::Sunday, Weekday::from_days_since_sunday(7));
    /// assert_eq!(Weekday::Monday, Weekday::from_days_since_sunday(8));
    /// ```
    pub fn from_days_since_sunday(input: isize) -> Self {
        (SUNDAY + input as i64).into()
    }

    /// Returns the day after the current day.
    pub(crate) fn next_day(self) -> Weekday {
        use Weekday::*;
        match self {
            Monday => Tuesday,
            Tuesday => Wednesday,
            Wednesday => Thursday,
            Thursday => Friday,
            Friday => Saturday,
            Saturday => Sunday,
            Sunday => Monday,
        }
    }
}
