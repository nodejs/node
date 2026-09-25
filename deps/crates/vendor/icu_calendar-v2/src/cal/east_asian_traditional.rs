// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

use crate::calendar_arithmetic::{ArithmeticDate, DateFieldsResolver, PackWithMD};
use crate::error::{
    DateAddError, DateError, DateFromFieldsError, DateNewError, EcmaReferenceYearError,
    LunisolarDateError, MonthError, UnknownEraError,
};
use crate::options::{DateAddOptions, DateDifferenceOptions};
use crate::options::{DateFromFieldsOptions, Overflow};
use crate::AsCalendar;
use crate::{types, Calendar, Date};
use calendrical_calculations::chinese_based;
use calendrical_calculations::rata_die::RataDie;
use core::cmp::Ordering;
use icu_locale_core::preferences::extensions::unicode::keywords::CalendarAlgorithm;
use icu_provider::prelude::*;

#[path = "east_asian_traditional/china_data.rs"]
mod china_data;
#[path = "east_asian_traditional/korea_data.rs"]
mod korea_data;
#[path = "east_asian_traditional/qing_data.rs"]
mod qing_data;
#[path = "east_asian_traditional/simple.rs"]
mod simple;

#[derive(PartialEq)]
enum EastAsianCalendarKind {
    Chinese,
    Korean,
}

/// Implements <https://tc39.es/proposal-intl-era-monthcode/#chinese-dangi-iso-reference-years>
///
/// `generate_reference_years` is helpful for generating this data if the spec needs to be updated.
///
/// Note that the spec is written in terms of ISO years, and this code is in terms of extended years.
/// This distinction only matters for month 11 and 12.
fn ecma_reference_year_common(
    month: types::Month,
    day: u8,
    cal: EastAsianCalendarKind,
) -> Result<i32, EcmaReferenceYearError> {
    let extended_year = match (month.number(), month.is_leap(), day > 29) {
        (1, false, false) => 1972,
        (1, false, true) => 1970,
        (1, true, _) => return Err(EcmaReferenceYearError::UseRegularIfConstrain),
        (2, false, _) => 1972,
        (2, true, false) => 1947,
        (2, true, true) => return Err(EcmaReferenceYearError::UseRegularIfConstrain),
        (3, false, false) => 1972,
        (3, false, true) if cal == EastAsianCalendarKind::Chinese => 1966,
        (3, false, true) => 1968, // Korean
        (3, true, false) => 1966,
        (3, true, true) => 1955,
        (4, false, false) => 1972,
        (4, false, true) => 1970,
        (4, true, false) => 1963,
        (4, true, true) => 1944,
        (5, false, _) => 1972,
        (5, true, false) => 1971,
        (5, true, true) => 1952,
        (6, false, false) => 1972,
        (6, false, true) => 1971,
        (6, true, false) => 1960,
        (6, true, true) => 1941,
        (7, false, _) => 1972,
        (7, true, false) => 1968,
        (7, true, true) => 1938,
        (8, false, false) => 1972,
        (8, false, true) => 1971,
        (8, true, false) => 1957,
        (8, true, true) => return Err(EcmaReferenceYearError::UseRegularIfConstrain),
        (9, false, _) => 1972,
        (9, true, false) => 2014,
        (9, true, true) => return Err(EcmaReferenceYearError::UseRegularIfConstrain),
        (10, false, _) => 1972,
        (10, true, false) => 1984,
        (10, true, true) => return Err(EcmaReferenceYearError::UseRegularIfConstrain),
        // Dec 31, 1972 is 1972-M11-26, dates after that
        // are in the next year
        (11, false, false) if day > 26 => 1971,
        (11, false, false) => 1972,
        (11, false, true) => 1969,
        // Spec has two years that map to the same extended year
        (11, true, false) => 2033,
        (11, true, true) => return Err(EcmaReferenceYearError::UseRegularIfConstrain),
        // Spec says 1972, but that is extended year 1971
        (12, false, _) => 1971,
        (12, true, _) => return Err(EcmaReferenceYearError::UseRegularIfConstrain),

        (0 | 13.., _, _) => return Err(EcmaReferenceYearError::MonthNotInCalendar),
    };

    Ok(extended_year)
}

/// The traditional East-Asian lunisolar calendar.
///
/// This calendar used traditionally in China as well as in other countries in East Asia is
/// often used today to track important cultural events and holidays like the Lunar New Year.
///
/// The type parameter specifies a particular set of calculation rules and local
/// time information, which differs by country and over time.
/// It must implement the currently-unstable `Rules` trait, at the moment this crate exports two stable
/// implementors of `Rules`: [`China`] and [`Korea`]. Please comment on [this issue](https://github.com/unicode-org/icu4x/issues/6962)
/// if you would like to see this trait stabilized.
///
/// This corresponds to the `"chinese"` and `"dangi"` [CLDR calendars](https://unicode.org/reports/tr35/#UnicodeCalendarIdentifier)
/// respectively, when used with the [`China`] and [`Korea`] [`Rules`] types.
///
/// # Year and Era codes
///
/// Unlike most calendars, the traditional East-Asian calendar does not traditionally count years in an infinitely
/// increasing sequence. Instead, 10 "celestial stems" and 12 "terrestrial branches" are combined to form a
/// cycle of year names which repeats every 60 years. However, for the purposes of calendar calculations and
/// conversions, this calendar also counts years based on the [`Gregorian`](crate::cal::Gregorian) (ISO) calendar.
/// This "related ISO year" marks the Gregorian year in which a traditional East-Asian year begins.
///
/// Because the traditional East-Asian calendar does not traditionally count years, era codes are not used in this calendar.
///
/// For more information, suggested reading materials include:
/// * _Calendrical Calculations_ by Reingold & Dershowitz
/// * _The Mathematics of the Chinese Calendar_ by Helmer Aslaksen <https://citeseerx.ist.psu.edu/viewdoc/download?doi=10.1.1.139.9311&rep=rep1&type=pdf>
/// * Wikipedia: <https://en.wikipedia.org/wiki/Chinese_calendar>
///
/// # Months and days
///
/// The 12 months (`M01`-`M12`) don't use names in modern usage, instead they are referred to as
/// e.g. 三月 (third month) using Chinese characters.
///
/// As a lunar calendar, the lengths of the months depend on the lunar cycle (a month starts on the day of
/// local new moon), and will be either 29 or 30 days. As 12 such months fall short of a solar year, a leap
/// month is inserted roughly every 3 years; this can be after any month (e.g. `M02L`).
///
/// Both the lengths of the months and the occurence of leap months are determined by the
/// concrete [`Rules`] implementation.
///
/// The length of the year is 353-355 days, and the length of the leap year 383-385 days.
///
/// # Calendar drift
///
/// As leap months are determined with respect to the solar year, this calendar stays anchored
/// to the seasons.
#[derive(Clone, Debug, Default, Copy, PartialEq, Eq, PartialOrd, Ord)]
#[allow(clippy::exhaustive_structs)] // newtype
pub struct EastAsianTraditional<R>(pub R);

/// The rules for the [`EastAsianTraditional`] calendar.
///
/// The calendar depends on both astronomical calculations and local time.
/// The rules for how to perform these calculations, as well as how local
/// time is determined differ between countries and have changed over time.
///
/// Implementations of this trait must produce calendars that have a leap month
/// at least once every three years.
///
/// This crate currently provides [`Rules`] for [`China`] and [`Korea`].
///
/// <div class="stab unstable">
/// 🚫 This trait is sealed; it should not be implemented by user code. If an API requests an item that implements this
/// trait, please consider using a type from the implementors listed below.
///
/// It is still possible to implement this trait in userland (since `UnstableSealed` is public),
/// do not do so unless you are prepared for things to occasionally break.
/// </div>
pub trait Rules: Clone + core::fmt::Debug + crate::cal::scaffold::UnstableSealed {
    /// Returns data about the given year.
    fn year(&self, related_iso: i32) -> EastAsianTraditionalYear;

    /// Returns data for the year containing the given [`RataDie`].
    fn year_containing_rd(&self, rd: RataDie) -> EastAsianTraditionalYear {
        let related_iso = calendrical_calculations::gregorian::year_from_fixed(rd)
            .unwrap_or_else(|e| e.saturate());

        let mut year = self.year(related_iso);

        if rd < year.new_year() {
            year = self.year(related_iso - 1)
        }

        year
    }

    /// Returns an ECMA reference year (represented as an extended year)
    /// that contains the given month-day combination.
    ///
    /// If the day is out of range, it will return a year that contains the given month
    /// and the maximum day possible for that month. See [the spec][spec] for the
    /// precise algorithm used.
    ///
    /// This API only matters when using [`MissingFieldsStrategy::Ecma`] to compute
    /// a date without providing a year in [`Date::try_from_fields()`]. The default impl
    /// will just error, and custom calendars who do not care about ECMA/Temporal
    /// reference years do not need to override this.
    ///
    /// [spec]: https://tc39.es/proposal-temporal/#sec-temporal-nonisomonthdaytoisoreferencedate
    /// [`MissingFieldsStrategy::Ecma`]: crate::options::MissingFieldsStrategy::Ecma
    fn ecma_reference_year(
        &self,
        _month: types::Month,
        _day: u8,
    ) -> Result<i32, EcmaReferenceYearError> {
        Err(EcmaReferenceYearError::Unimplemented)
    }

    /// The error that is returned by [`Self::check_date_compatibility`].
    ///
    /// Set this to [`core::convert::Infallible`] if the type is a singleton or
    /// the parameterization does not affect calendar semantics.
    type DateCompatibilityError: core::fmt::Debug;

    /// Checks whether two [`Rules`] values are equal for the purpose of [`Date`] interaction.
    fn check_date_compatibility(&self, other: &Self) -> Result<(), Self::DateCompatibilityError>;

    /// The debug name for the calendar defined by these [`Rules`].
    fn debug_name(&self) -> &'static str {
        "EastAsianTraditional (custom)"
    }

    /// The BCP-47 [`CalendarAlgorithm`] for the calendar defined by these [`Rules`], if defined.
    fn calendar_algorithm(&self) -> Option<CalendarAlgorithm> {
        None
    }
}

/// The [Chinese](https://en.wikipedia.org/wiki/Chinese_calendar) variant of the [`EastAsianTraditional`] calendar.
///
/// This type agrees with the official data published by the
/// [Purple Mountain Observatory for the years 1900-2025], as well as with
/// the data published by the [Hong Kong Observatory for the years 1901-2100].
///
/// For years since 1912, this uses the [GB/T 33661-2017] rules.
/// As accurate computation is computationally expensive, years until
/// 2100 are precomputed, and after that this type regresses to a simplified
/// calculation. If accuracy beyond 2100 is required, clients
/// can implement their own [`Rules`] type containing more precomputed data.
/// We note that the calendar is inherently uncertain for some future dates.
///
/// Before 1912 [different rules](https://ytliu.epizy.com/Shixian/Shixian_summary.html)
/// were used. This type produces correct data for the years 1900-1912, and
/// falls back to a simplified calculation before 1900. If accuracy is
/// required before 1900, clients can implement their own [`Rules`] type
/// using data such as from the excellent compilation by [Yuk Tung Liu].
///
/// The precise behavior of this calendar may change in the future if:
/// - New ground truth is established by published government sources
/// - We decide to tweak the simplified calculation
/// - We decide to expand or reduce the range where we are correctly handling past dates.
///
/// [Purple Mountain Observatory for the years 1900-2025]: http://www.pmo.cas.cn/xwdt2019/kpdt2019/202203/P020250414456381274062.pdf
/// [Hong Kong Observatory for the years 1901-2100]: https://www.hko.gov.hk/en/gts/time/conversion.htm
/// [GB/T 33661-2017]: China::gb_t_33661_2017
/// [Yuk Tung Liu]: https://ytliu0.github.io/ChineseCalendar/table.html
pub type ChineseTraditional = EastAsianTraditional<China>;

/// The [`Rules`] used in China.
///
/// See [`ChineseTraditional`] for more information.
#[derive(Copy, Clone, Debug, Default)]
#[non_exhaustive]
pub struct China;

impl China {
    /// Computes [`EastAsianTraditionalYear`] according to [GB/T 33661-2017],
    /// as implemented by [`calendrical_calculations::chinese_based::Chinese`].
    ///
    /// The rules specified in [GB/T 33661-2017] have only been used
    /// since 1912, applying them proleptically to years before 1912 will not
    /// necessarily match historical calendars.
    ///
    /// Note that for future years there is a small degree of uncertainty, as
    /// [GB/T 33661-2017] depends on the uncertain future [difference between UT1
    /// and UTC](https://en.wikipedia.org/wiki/Leap_second#Future).
    /// As noted by
    /// [Yuk Tung Liu](https://ytliu0.github.io/ChineseCalendar/computation.html#modern),
    /// years as early as 2057, 2089, and 2097 have lunar events very close to
    /// local midnight, which might affect the start of a (single) month if additional
    /// leap seconds are introduced.
    ///
    /// [GB/T 33661-2017]: https://openstd.samr.gov.cn/bzgk/gb/newGbInfo?hcno=E107EA4DE9725EDF819F33C60A44B296
    pub fn gb_t_33661_2017(related_iso: i32) -> EastAsianTraditionalYear {
        EastAsianTraditionalYear::calendrical_calculations::<chinese_based::Chinese>(related_iso)
    }
}

impl crate::cal::scaffold::UnstableSealed for China {}
impl Rules for China {
    fn year(&self, related_iso: i32) -> EastAsianTraditionalYear {
        if let Some(year) = EastAsianTraditionalYear::lookup(
            related_iso,
            china_data::STARTING_YEAR,
            china_data::DATA,
        ) {
            year
        } else if related_iso > china_data::STARTING_YEAR {
            EastAsianTraditionalYear::simple(simple::UTC_PLUS_8, related_iso)
        } else if let Some(year) =
            EastAsianTraditionalYear::lookup(related_iso, qing_data::STARTING_YEAR, qing_data::DATA)
        {
            year
        } else {
            EastAsianTraditionalYear::simple(simple::BEIJING_UTC_OFFSET, related_iso)
        }
    }

    fn ecma_reference_year(
        &self,
        month: types::Month,
        day: u8,
    ) -> Result<i32, EcmaReferenceYearError> {
        ecma_reference_year_common(month, day, EastAsianCalendarKind::Chinese)
    }

    type DateCompatibilityError = core::convert::Infallible;

    fn check_date_compatibility(&self, &Self: &Self) -> Result<(), Self::DateCompatibilityError> {
        Ok(())
    }

    fn calendar_algorithm(&self) -> Option<CalendarAlgorithm> {
        Some(CalendarAlgorithm::Chinese)
    }

    fn debug_name(&self) -> &'static str {
        "Chinese"
    }
}

/// The [Korean](https://en.wikipedia.org/wiki/Korean_calendar) variant of the [`EastAsianTraditional`] calendar.
///
/// This type agrees with the official data published by the
/// [Korea Astronomy and Space Science Institute for the years 1900-2050].
///
/// For years since 1912, this uses [adapted GB/T 33661-2017] rules,
/// using Korea time instead of Beijing Time.
/// As accurate computation is computationally expensive, years until
/// 2100 are precomputed, and after that this type regresses to a simplified
/// calculation. If accuracy beyond 2100 is required, clients
/// can implement their own [`Rules`] type containing more precomputed data.
/// We note that the calendar is inherently uncertain for some future dates.
///
/// Before 1912 [different rules](https://ytliu.epizy.com/Shixian/Shixian_summary.html)
/// were used (those of Qing-dynasty China). This type produces correct data
/// for the years 1900-1912, and falls back to a simplified calculation
/// before 1900. If accuracy is required before 1900, clients can implement
/// their own [`Rules`] type using data such as from the excellent compilation
/// by [Yuk Tung Liu].
///
/// The precise behavior of this calendar may change in the future if:
/// - New ground truth is established by published government sources
/// - We decide to tweak the simplified calculation
/// - We decide to expand or reduce the range where we are correctly handling past dates.
///
/// [Korea Astronomy and Space Science Institute for the years 1900-2050]: https://astro.kasi.re.kr/life/pageView/5
/// [adapted GB/T 33661-2017]: Korea::adapted_gb_t_33661_2017
/// [GB/T 33661-2017]: China::gb_t_33661_2017
/// [Yuk Tung Liu]: https://ytliu0.github.io/ChineseCalendar/table.html
///
/// ```rust
/// use icu::calendar::cal::{ChineseTraditional, KoreanTraditional};
/// use icu::calendar::Date;
///
/// let iso_a = Date::try_new_iso(2012, 4, 23).unwrap();
/// let korean_a = iso_a.to_calendar(KoreanTraditional::new());
/// let chinese_a = iso_a.to_calendar(ChineseTraditional::new());
///
/// assert_eq!(korean_a.month().number(), 3);
/// assert_eq!(korean_a.month().to_input().is_leap(), true);
/// assert_eq!(chinese_a.month().number(), 4);
/// assert_eq!(chinese_a.month().to_input().is_leap(), false);
///
/// let iso_b = Date::try_new_iso(2012, 5, 23).unwrap();
/// let korean_b = iso_b.to_calendar(KoreanTraditional::new());
/// let chinese_b = iso_b.to_calendar(ChineseTraditional::new());
///
/// assert_eq!(korean_b.month().number(), 4);
/// assert_eq!(korean_b.month().to_input().is_leap(), false);
/// assert_eq!(chinese_b.month().number(), 4);
/// assert_eq!(chinese_b.month().to_input().is_leap(), true);
/// ```
pub type KoreanTraditional = EastAsianTraditional<Korea>;

/// The [`Rules`] used in Korea.
///
/// See [`KoreanTraditional`] for more information.
#[derive(Copy, Clone, Debug, Default, PartialEq, Eq, PartialOrd, Ord)]
#[non_exhaustive]
pub struct Korea;

impl Korea {
    /// A version of [`China::gb_t_33661_2017`] adapted for Korean time.
    ///
    /// See [`China::gb_t_33661_2017`] for caveats.
    pub fn adapted_gb_t_33661_2017(related_iso: i32) -> EastAsianTraditionalYear {
        EastAsianTraditionalYear::calendrical_calculations::<chinese_based::Dangi>(related_iso)
    }
}

impl KoreanTraditional {
    /// Creates a new [`KoreanTraditional`] calendar.
    pub const fn new() -> Self {
        Self(Korea)
    }

    /// Use [`Self::new`].
    #[cfg(feature = "serde")]
    #[doc = icu_provider::gen_buffer_unstable_docs!(BUFFER,Self::new)]
    #[deprecated(since = "2.1.0", note = "use `Self::new()`")]
    pub fn try_new_with_buffer_provider(
        _provider: &(impl BufferProvider + ?Sized),
    ) -> Result<Self, DataError> {
        Ok(Self::new())
    }

    /// Use [`Self::new`].
    #[doc = icu_provider::gen_buffer_unstable_docs!(UNSTABLE, Self::new)]
    #[deprecated(since = "2.1.0", note = "use `Self::new()`")]
    pub fn try_new_unstable<D: ?Sized>(_provider: &D) -> Result<Self, DataError> {
        Ok(Self::new())
    }

    /// Use [`Self::new`].
    #[deprecated(since = "2.1.0", note = "use `Self::new()`")]
    pub fn new_always_calculating() -> Self {
        Self::new()
    }
}

impl crate::cal::scaffold::UnstableSealed for Korea {}
impl Rules for Korea {
    fn year(&self, related_iso: i32) -> EastAsianTraditionalYear {
        if let Some(year) = EastAsianTraditionalYear::lookup(
            related_iso,
            korea_data::STARTING_YEAR,
            korea_data::DATA,
        ) {
            year
        } else if related_iso > korea_data::STARTING_YEAR {
            EastAsianTraditionalYear::simple(simple::UTC_PLUS_9, related_iso)
        } else if let Some(year) =
            EastAsianTraditionalYear::lookup(related_iso, qing_data::STARTING_YEAR, qing_data::DATA)
        {
            // Korea used Qing-dynasty rules before 1912
            // https://github.com/unicode-org/icu4x/issues/6455#issuecomment-3282175550
            year
        } else {
            EastAsianTraditionalYear::simple(simple::BEIJING_UTC_OFFSET, related_iso)
        }
    }

    fn ecma_reference_year(
        &self,
        month: types::Month,
        day: u8,
    ) -> Result<i32, EcmaReferenceYearError> {
        ecma_reference_year_common(month, day, EastAsianCalendarKind::Korean)
    }

    type DateCompatibilityError = core::convert::Infallible;

    fn check_date_compatibility(&self, &Self: &Self) -> Result<(), Self::DateCompatibilityError> {
        Ok(())
    }

    fn calendar_algorithm(&self) -> Option<CalendarAlgorithm> {
        Some(CalendarAlgorithm::Dangi)
    }
    fn debug_name(&self) -> &'static str {
        "Korean"
    }
}

impl Date<KoreanTraditional> {
    /// Construct a new traditional Korean [`Date`].
    ///
    /// Years are arithmetic, meaning there is a year 0 preceded by negative years, with a
    /// valid range of `-9999..=9999`.
    ///
    /// ```rust
    /// use icu::calendar::types::Month;
    /// use icu::calendar::Date;
    ///
    /// let date = Date::try_new_korean_traditional(2025, Month::new(5), 25)
    ///     .expect("Failed to initialize Date instance.");
    ///
    /// assert_eq!(date.cyclic_year().related_iso, 2025);
    /// assert_eq!(date.month().to_input(), Month::new(5));
    /// assert_eq!(date.day_of_month().0, 25);
    /// ```
    pub fn try_new_korean_traditional(
        related_iso_year: i32,
        month: types::Month,
        day: u8,
    ) -> Result<Date<KoreanTraditional>, LunisolarDateError> {
        let calendar = KoreanTraditional::new();
        ArithmeticDate::try_from_ymd_lunisolar(related_iso_year, month, day, &calendar)
            .map(ChineseDateInner)
            .map(|inner| Date::from_raw(inner, calendar))
    }
}

impl<A: AsCalendar<Calendar = KoreanTraditional>> Date<A> {
    /// This method uses an ordinal month, which is probably not what you want.
    ///
    /// Use [`Date::try_new_korean_traditional`]
    #[deprecated(since = "2.1.0", note = "use `Date::try_new_korean_traditional`")]
    pub fn try_new_dangi_with_calendar(
        related_iso_year: i32,
        ordinal_month: u8,
        day: u8,
        calendar: A,
    ) -> Result<Date<A>, DateError> {
        ArithmeticDate::from_year_month_day(
            related_iso_year,
            ordinal_month,
            day,
            calendar.as_calendar(),
        )
        .map(ChineseDateInner)
        .map(|inner| Date::from_raw(inner, calendar))
        .map_err(Into::into)
    }
}

/// The inner date type used for representing [`Date`]s of [`EastAsianTraditional`].
#[derive(Debug, Clone)]
pub struct ChineseDateInner<R: Rules>(ArithmeticDate<EastAsianTraditional<R>>);

impl<R: Rules> Copy for ChineseDateInner<R> {}
impl<R: Rules> PartialEq for ChineseDateInner<R> {
    fn eq(&self, other: &Self) -> bool {
        self.0 == other.0
    }
}
impl<R: Rules> Eq for ChineseDateInner<R> {}
impl<R: Rules> PartialOrd for ChineseDateInner<R> {
    fn partial_cmp(&self, other: &Self) -> Option<Ordering> {
        Some(self.cmp(other))
    }
}
impl<R: Rules> Ord for ChineseDateInner<R> {
    fn cmp(&self, other: &Self) -> Ordering {
        self.0.cmp(&other.0)
    }
}

impl core::ops::Sub<EastAsianTraditionalYear> for EastAsianTraditionalYear {
    type Output = i32;
    #[inline]
    fn sub(self, rhs: EastAsianTraditionalYear) -> Self::Output {
        self.related_iso - rhs.related_iso
    }
}

impl ChineseTraditional {
    /// Creates a new [`ChineseTraditional`] calendar.
    pub const fn new() -> Self {
        EastAsianTraditional(China)
    }

    #[cfg(feature = "serde")]
    #[doc = icu_provider::gen_buffer_unstable_docs!(BUFFER,Self::new)]
    #[deprecated(since = "2.1.0", note = "use `Self::new()`")]
    pub fn try_new_with_buffer_provider(
        _provider: &(impl BufferProvider + ?Sized),
    ) -> Result<Self, DataError> {
        Ok(Self::new())
    }

    #[doc = icu_provider::gen_buffer_unstable_docs!(UNSTABLE, Self::new)]
    #[deprecated(since = "2.1.0", note = "use `Self::new()`")]
    pub fn try_new_unstable<D: ?Sized>(_provider: &D) -> Result<Self, DataError> {
        Ok(Self::new())
    }

    /// Use [`Self::new()`].
    #[deprecated(since = "2.1.0", note = "use `Self::new()`")]
    pub fn new_always_calculating() -> Self {
        Self::new()
    }
}

impl PackWithMD for EastAsianTraditionalYear {
    /// The first three bytes are the [`PackedEastAsianTraditionalYearData`], the remaining four the YMD as encoded by [`i32::pack`].
    type Packed = [u8; 7];

    fn pack(self, month: u8, day: u8) -> Self::Packed {
        let PackedEastAsianTraditionalYearData(a, b, c) = self.packed;
        let [d, e, f, g] = self.related_iso.pack(month, day);
        [a, b, c, d, e, f, g]
    }

    fn unpack_year([a, b, c, d, e, f, g]: Self::Packed) -> Self {
        let related_iso = i32::unpack_year([d, e, f, g]);
        let packed = PackedEastAsianTraditionalYearData(a, b, c);
        Self {
            packed,
            related_iso,
        }
    }

    fn unpack_month([_, _, _c, d, e, f, g]: Self::Packed) -> u8 {
        i32::unpack_month([d, e, f, g])
    }

    fn unpack_day([_, _, _c, d, e, f, g]: Self::Packed) -> u8 {
        i32::unpack_day([d, e, f, g])
    }
}

impl<R: Rules> DateFieldsResolver for EastAsianTraditional<R> {
    type YearInfo = EastAsianTraditionalYear;

    fn days_in_provided_month(year: EastAsianTraditionalYear, month: u8) -> u8 {
        year.packed.month_len(month)
    }

    /// Returns the number of months in a given year, which is 13 in a leap year, and 12 in a common year.
    fn months_in_provided_year(year: EastAsianTraditionalYear) -> u8 {
        12 + year.packed.leap_month().is_some() as u8
    }

    #[inline]
    fn min_months_from(_start: Self::YearInfo, years: i32) -> i32 {
        // By Rules invariant
        12 * years + (years / 3)
    }

    #[inline]
    fn extended_year_from_era_year_unchecked(
        &self,
        _era: &[u8],
        _era_year: i32,
    ) -> Result<i32, UnknownEraError> {
        // This calendar has no era codes
        Err(UnknownEraError)
    }

    #[inline]
    fn year_info_from_extended(&self, extended_year: i32) -> Self::YearInfo {
        debug_assert!(crate::calendar_arithmetic::SAFE_YEAR_RANGE.contains(&extended_year));
        self.0.year(extended_year)
    }

    #[inline]
    fn extended_from_year_info(&self, year_info: Self::YearInfo) -> i32 {
        year_info.related_iso
    }

    #[inline]
    fn reference_year_from_month_day(
        &self,
        month: types::Month,
        day: u8,
    ) -> Result<Self::YearInfo, EcmaReferenceYearError> {
        self.0
            .ecma_reference_year(month, day)
            .map(|y| self.0.year(y))
    }

    fn ordinal_from_month(
        &self,
        year: Self::YearInfo,
        month: types::Month,
        overflow: Overflow,
    ) -> Result<u8, MonthError> {
        let (number @ 1..=12, leap) = (month.number(), month.is_leap()) else {
            return Err(MonthError::NotInCalendar);
        };

        // 14 is a sentinel value, greater than all other months, for the purpose of computation only;
        // it is impossible to actually have 14 months in a year.
        let leap_month_sentinel = year.packed.leap_month().unwrap_or(14);

        // leap_month identifies the ordinal month number of the leap month,
        // so its month number will be leap_month - 1
        if month == types::Month::leap(leap_month_sentinel - 1) {
            return Ok(leap_month_sentinel);
        }

        if leap {
            // This leap month doesn't exist in the year, reject if needed
            match overflow {
                Overflow::Reject => return Err(MonthError::NotInYear),
                // Written as a match for exhaustiveness
                Overflow::Constrain => (),
            }
        }

        // add one if there was a leap month before
        Ok(number + (number >= leap_month_sentinel) as u8)
    }

    fn month_from_ordinal(&self, year: Self::YearInfo, ordinal_month: u8) -> types::Month {
        // 14 is a sentinel value, greater than all other months, for the purpose of computation only;
        // it is impossible to actually have 14 months in a year.
        let leap_month = year.packed.leap_month().unwrap_or(14);
        types::Month::new_unchecked(
            // subtract one if there was a leap month before
            ordinal_month - (ordinal_month >= leap_month) as u8,
            ordinal_month == leap_month,
        )
    }

    fn to_rata_die_inner(year: Self::YearInfo, month: u8, day: u8) -> RataDie {
        year.new_year() + year.packed.days_before_month(month) as i64 + (day - 1) as i64
    }
}

impl<R: Rules> crate::cal::scaffold::UnstableSealed for EastAsianTraditional<R> {}
impl<R: Rules> Calendar for EastAsianTraditional<R> {
    type DateInner = ChineseDateInner<R>;
    type Year = types::CyclicYear;
    type DateCompatibilityError = R::DateCompatibilityError;

    fn new_date(
        &self,
        year: types::YearInput,
        month: types::Month,
        day: u8,
    ) -> Result<Self::DateInner, DateNewError> {
        ArithmeticDate::from_input_year_month_code_day(year, month, day, self).map(ChineseDateInner)
    }

    fn from_fields(
        &self,
        fields: types::DateFields,
        options: DateFromFieldsOptions,
    ) -> Result<Self::DateInner, DateFromFieldsError> {
        ArithmeticDate::from_fields(fields, options, self).map(ChineseDateInner)
    }

    fn from_rata_die(&self, rd: RataDie) -> Self::DateInner {
        let year = self.0.year_containing_rd(rd);

        // Clamp the RD to our year
        let rd = rd.clamp(
            year.new_year(),
            year.new_year() + year.packed.days_in_year() as i64,
        );

        let (month, day) = year.month_day_for((rd - year.new_year()) as u16 + 1);

        // date is in the valid RD range
        ChineseDateInner(ArithmeticDate::new_unchecked(year, month, day))
    }

    fn to_rata_die(&self, date: &Self::DateInner) -> RataDie {
        date.0.to_rata_die()
    }

    fn has_cheap_iso_conversion(&self) -> bool {
        false
    }

    // Count the number of months in a given year, specified by providing a date
    // from that year
    fn days_in_year(&self, date: &Self::DateInner) -> u16 {
        date.0.year().packed.days_in_year()
    }

    fn days_in_month(&self, date: &Self::DateInner) -> u8 {
        Self::days_in_provided_month(date.0.year(), date.0.month())
    }

    fn add(
        &self,
        date: &Self::DateInner,
        duration: types::DateDuration,
        options: DateAddOptions,
    ) -> Result<Self::DateInner, DateAddError> {
        date.0.added(duration, self, options).map(ChineseDateInner)
    }

    fn until(
        &self,
        date1: &Self::DateInner,
        date2: &Self::DateInner,
        options: DateDifferenceOptions,
    ) -> types::DateDuration {
        date1.0.until(&date2.0, self, options)
    }

    fn check_date_compatibility(&self, other: &Self) -> Result<(), Self::DateCompatibilityError> {
        self.0.check_date_compatibility(&other.0)
    }

    /// Obtain a name for the calendar for debug printing
    fn debug_name(&self) -> &'static str {
        self.0.debug_name()
    }

    fn year_info(&self, date: &Self::DateInner) -> Self::Year {
        let year = date.0.year();
        types::CyclicYear {
            year: (year.related_iso - 4).rem_euclid(60) as u8 + 1,
            related_iso: year.related_iso,
        }
    }

    fn is_in_leap_year(&self, date: &Self::DateInner) -> bool {
        date.0.year().packed.leap_month().is_some()
    }

    /// The calendar-specific month code represented by `date`;
    /// since the Chinese calendar has leap months, an "L" is appended to the month code for
    /// leap months. For example, in a year where an intercalary month is added after the second
    /// month, the month codes for ordinal months 1, 2, 3, 4, 5 would be "M01", "M02", "M02L", "M03", "M04".
    fn month(&self, date: &Self::DateInner) -> types::MonthInfo {
        let mut m = types::MonthInfo::new(self, date.0);
        if date.0.year().packed.leap_month() == Some(m.ordinal + 1) {
            m.leap_status = types::LeapStatus::Base;
        }
        m
    }

    /// The calendar-specific day-of-month represented by `date`
    fn day_of_month(&self, date: &Self::DateInner) -> types::DayOfMonth {
        types::DayOfMonth(date.0.day())
    }

    /// Information of the day of the year
    fn day_of_year(&self, date: &Self::DateInner) -> types::DayOfYear {
        types::DayOfYear(
            date.0.year().packed.days_before_month(date.0.month()) + date.0.day() as u16,
        )
    }

    fn calendar_algorithm(&self) -> Option<CalendarAlgorithm> {
        self.0.calendar_algorithm()
    }

    fn months_in_year(&self, date: &Self::DateInner) -> u8 {
        Self::months_in_provided_year(date.0.year())
    }
}

impl Date<ChineseTraditional> {
    /// Construct a new traditional Chinese [`Date`].
    ///
    /// Years are arithmetic, meaning there is a year 0 preceded by negative years, with a
    /// valid range of `-9999..=9999`.
    ///
    /// ```rust
    /// use icu::calendar::types::Month;
    /// use icu::calendar::Date;
    ///
    /// let date = Date::try_new_chinese_traditional(2025, Month::new(5), 25)
    ///     .expect("Failed to initialize Date instance.");
    ///
    /// assert_eq!(date.cyclic_year().related_iso, 2025);
    /// assert_eq!(date.month().to_input(), Month::new(5));
    /// assert_eq!(date.day_of_month().0, 25);
    /// ```
    pub fn try_new_chinese_traditional(
        related_iso_year: i32,
        month: types::Month,
        day: u8,
    ) -> Result<Date<ChineseTraditional>, LunisolarDateError> {
        let calendar = ChineseTraditional::new();
        ArithmeticDate::try_from_ymd_lunisolar(related_iso_year, month, day, &calendar)
            .map(ChineseDateInner)
            .map(|inner| Date::from_raw(inner, calendar))
    }
}

impl<A: AsCalendar<Calendar = ChineseTraditional>> Date<A> {
    /// This method uses an ordinal month, which is probably not what you want.
    ///
    /// Use [`Date::try_new_chinese_traditional`]
    #[deprecated(since = "2.1.0", note = "use `Date::try_new_chinese_traditional`")]
    pub fn try_new_chinese_with_calendar(
        related_iso_year: i32,
        ordinal_month: u8,
        day: u8,
        calendar: A,
    ) -> Result<Date<A>, DateError> {
        ArithmeticDate::from_year_month_day(
            related_iso_year,
            ordinal_month,
            day,
            calendar.as_calendar(),
        )
        .map(ChineseDateInner)
        .map(|inner| Date::from_raw(inner, calendar))
        .map_err(Into::into)
    }
}

/// Information about a [`EastAsianTraditional`] year.
#[derive(Copy, Clone, Debug)]
// TODO(#3933): potentially make this smaller
pub struct EastAsianTraditionalYear {
    /// Contains:
    /// - length of each month in the year
    /// - whether or not there is a leap month, and which month it is
    /// - the date of Chinese New Year in the related ISO year
    packed: PackedEastAsianTraditionalYearData,
    related_iso: i32,
}

impl PartialEq for EastAsianTraditionalYear {
    fn eq(&self, other: &Self) -> bool {
        self.related_iso == other.related_iso
    }
}
impl Eq for EastAsianTraditionalYear {}
impl core::hash::Hash for EastAsianTraditionalYear {
    fn hash<H: core::hash::Hasher>(&self, state: &mut H) {
        self.related_iso.hash(state);
    }
}
impl PartialOrd for EastAsianTraditionalYear {
    fn partial_cmp(&self, other: &Self) -> Option<Ordering> {
        Some(self.cmp(other))
    }
}
impl Ord for EastAsianTraditionalYear {
    fn cmp(&self, other: &Self) -> Ordering {
        self.related_iso.cmp(&other.related_iso)
    }
}

impl EastAsianTraditionalYear {
    /// Creates [`EastAsianTraditionalYear`] from the given parts.
    ///
    /// `month_starts` contains the first day of the 13 or 14 months from the first
    /// month of the given year to the first month of the following year (inclusive).
    /// See [`Date::to_rata_die`] to obtain a [`RataDie`] from a [`Date`] in an
    /// arbitrary calendar. If non-leap years, the last value is ignored.
    ///
    /// Months need to have either 29 or 30 days.
    ///
    /// `leap_month` is the ordinal number of the leap month, for example if a year
    /// has months 1, 2, 3, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, the `leap_month`
    /// would be `Some(4)`.
    pub const fn try_new(
        related_iso: i32,
        month_starts: [RataDie; 14],
        leap_month: Option<u8>,
    ) -> Option<Self> {
        let mut month_lengths = [false; 13];

        let mut i = 0;
        #[allow(clippy::indexing_slicing)]
        while i < 12 + leap_month.is_some() as usize {
            match month_starts[i + 1].to_i64_date() - month_starts[i].to_i64_date() {
                29 => month_lengths[i] = false,
                30 => month_lengths[i] = true,
                _ => return None,
            }
            i += 1;
        }

        Some(Self {
            packed: PackedEastAsianTraditionalYearData::new(
                related_iso,
                month_lengths,
                leap_month,
                month_starts[0],
            ),
            related_iso,
        })
    }

    #[cfg(test)]
    pub(crate) fn related_iso(self) -> i32 {
        self.related_iso
    }

    fn lookup(
        related_iso: i32,
        starting_year: i32,
        data: &[PackedEastAsianTraditionalYearData],
    ) -> Option<Self> {
        Some(related_iso)
            .and_then(|e| usize::try_from(e.checked_sub(starting_year)?).ok())
            .and_then(|i| data.get(i))
            .map(|&packed| Self {
                related_iso,
                packed,
            })
    }

    fn calendrical_calculations<CB: chinese_based::ChineseBased>(
        related_iso: i32,
    ) -> EastAsianTraditionalYear {
        let mid_year = calendrical_calculations::gregorian::fixed_from_gregorian(related_iso, 7, 1);
        let year_bounds = chinese_based::YearBounds::compute::<CB>(mid_year);

        let chinese_based::YearBounds {
            new_year,
            next_new_year,
            ..
        } = year_bounds;
        let (month_lengths, leap_month) =
            chinese_based::month_structure_for_year::<CB>(new_year, next_new_year);

        EastAsianTraditionalYear {
            packed: PackedEastAsianTraditionalYearData::new(
                related_iso,
                month_lengths,
                leap_month,
                new_year,
            ),
            related_iso,
        }
    }

    // 1-based day of year
    fn month_day_for(self, day_of_year: u16) -> (u8, u8) {
        // We divide by 30, not 29, to account for the case where all months before this
        // were length 30 (possible near the beginning of the year)
        let mut month = ((day_of_year - 1) / 30) as u8 + 1;
        let mut days_before_month = self.packed.days_before_month(month);
        let mut last_day_of_month = self.packed.days_before_month(month + 1);

        while day_of_year > last_day_of_month {
            month += 1;
            days_before_month = last_day_of_month;
            last_day_of_month = self.packed.days_before_month(month + 1);
        }

        (month, (day_of_year - days_before_month) as u8)
    }

    /// Get the new year R.D.    
    fn new_year(self) -> RataDie {
        self.packed.new_year(self.related_iso)
    }
}

/// A packed [`EastAsianTraditionalYear`]
///
/// Bit structure (little endian: note that shifts go in the opposite direction!)
///
/// ```text
/// Bit:             0   1   2   3   4   5   6   7
/// Byte 0:          [  month lengths .............
/// Byte 1:         .. month lengths ] | [ leap month index ..
/// Byte 2:          ] | [   NY offset       ] | unused
/// ```
///
/// Where the New Year Offset is the offset from ISO Jan 19 of that year for Chinese New Year,
/// the month lengths are stored as 1 = 30, 0 = 29 for each month including the leap month.
/// The largest possible offset is 33, which requires 6 bits of storage.
///
/// <div class="stab unstable">
/// 🚧 This code is considered unstable; it may change at any time, in breaking or non-breaking ways,
/// including in SemVer minor releases. While the serde representation of data structs is guaranteed
/// to be stable, their Rust representation might not be. Use with caution.
/// </div>
#[derive(Debug, Copy, Clone)]
#[cfg_attr(test, derive(PartialEq))]
struct PackedEastAsianTraditionalYearData(u8, u8, u8);

impl PackedEastAsianTraditionalYearData {
    /// The first day on which Chinese New Year may occur
    ///
    /// According to Reingold & Dershowitz, ch 19.6, Chinese New Year occurs on Jan 21 - Feb 21 inclusive.
    ///
    /// Our simple approximation sometimes returns Feb 22.
    ///
    /// We allow it to occur as early as January 19 which is the earliest the second new moon
    /// could occur after the Winter Solstice if the solstice is pinned to December 20.
    const fn earliest_ny(related_iso: i32) -> RataDie {
        calendrical_calculations::gregorian::fixed_from_gregorian(related_iso, 1, 19)
    }

    /// It clamps some values to avoid debug assertions on calendrical invariants.
    const fn new(
        related_iso: i32,
        month_lengths: [bool; 13],
        leap_month: Option<u8>,
        new_year: RataDie,
    ) -> Self {
        // These assertions are API correctness assertions and even bad calendar arithmetic
        // should not produce this
        if let Some(l) = leap_month {
            debug_assert!(2 <= l && l <= 13, "Leap month indices must be 2 <= i <= 13");
        } else {
            debug_assert!(
                !month_lengths[12],
                "Last month length should not be set for non-leap years"
            )
        }

        let ny_offset = new_year.since(Self::earliest_ny(related_iso));

        // Assert the offset is in range, but allow it to be out of
        // range when out_of_valid_astronomical_range=true
        debug_assert!(ny_offset >= 0, "Year offset too small to store");

        // The maximum new-year's offset we have found is 34
        debug_assert!(ny_offset < 35, "Year offset too big to store");

        // Just clamp to something we can represent when things get of range.
        //
        // This will typically happen when out_of_valid_astronomical_range
        // is true.
        //
        // We can store up to 6 bytes for ny_offset, even if our
        // maximum asserted value is otherwise 33.
        let ny_offset = ny_offset & (0x40 - 1);

        let mut all = 0u32; // last byte unused

        let mut month = 0;
        while month < month_lengths.len() {
            #[allow(clippy::indexing_slicing)] // const iteration
            if month_lengths[month] {
                all |= 1 << month as u32;
            }
            month += 1;
        }
        let leap_month_idx = if let Some(leap_month_idx) = leap_month {
            leap_month_idx
        } else {
            0
        };
        all |= (leap_month_idx as u32) << (8 + 5);
        all |= (ny_offset as u32) << (16 + 1);
        let le = all.to_le_bytes();
        Self(le[0], le[1], le[2])
    }

    fn new_year(self, related_iso: i32) -> RataDie {
        Self::earliest_ny(related_iso) + (self.2 as i64 >> 1)
    }

    fn leap_month(self) -> Option<u8> {
        let bits = (self.1 >> 5) + ((self.2 & 0b1) << 3);

        (bits != 0).then_some(bits)
    }

    // Whether a particular month has 30 days (month is 1-indexed)
    fn month_len(self, month: u8) -> u8 {
        let months = u16::from_le_bytes([self.0, self.1]);
        29 + (months >> (month - 1) & 1) as u8
    }

    // month is 1-indexed
    fn days_before_month(self, month: u8) -> u16 {
        let months = u16::from_le_bytes([self.0, self.1]);
        // month is 1-indexed, so `29 * month` includes the current month
        let mut prev_month_lengths = 29 * (month - 1) as u16;
        // month is 1-indexed, so `1 << month` is a mask with all zeroes except
        // for a 1 at the bit index at the next month. Subtracting 1 from it gets us
        // a bitmask for all months up to now
        let long_month_bits = months & ((1 << (month - 1) as u16) - 1);
        prev_month_lengths += long_month_bits.count_ones().try_into().unwrap_or(0);
        prev_month_lengths
    }

    fn days_in_year(self) -> u16 {
        self.days_before_month(13 + self.leap_month().is_some() as u8)
    }
}

// Precalculates Chinese years, significant performance improvement for big tests
#[cfg(test)]
#[derive(Debug, Clone, Copy)]
pub(crate) struct EastAsianTraditionalYears<R: Rules>(&'static [EastAsianTraditionalYear], R);

#[cfg(test)]
impl EastAsianTraditionalYears<China> {
    pub fn china() -> Self {
        static R: std::sync::LazyLock<Vec<EastAsianTraditionalYear>> =
            std::sync::LazyLock::new(|| (-1100000..=1100000).map(|i| China.year(i)).collect());
        Self(&R, China)
    }
}

#[cfg(test)]
impl EastAsianTraditionalYears<Korea> {
    pub fn korea() -> Self {
        static R: std::sync::LazyLock<Vec<EastAsianTraditionalYear>> =
            std::sync::LazyLock::new(|| (-1100000..=1100000).map(|i| Korea.year(i)).collect());
        Self(&R, Korea)
    }
}

#[cfg(test)]
impl<R: Rules> crate::cal::scaffold::UnstableSealed for EastAsianTraditionalYears<R> {}
#[cfg(test)]
impl<R: Rules> Rules for EastAsianTraditionalYears<R> {
    fn year(&self, related_iso: i32) -> EastAsianTraditionalYear {
        self.0[(related_iso + 1100000) as usize]
    }

    fn debug_name(&self) -> &'static str {
        self.1.debug_name()
    }

    fn calendar_algorithm(&self) -> Option<CalendarAlgorithm> {
        self.1.calendar_algorithm()
    }

    fn ecma_reference_year(
        &self,
        month: types::Month,
        day: u8,
    ) -> Result<i32, EcmaReferenceYearError> {
        self.1.ecma_reference_year(month, day)
    }

    fn year_containing_rd(&self, rd: RataDie) -> EastAsianTraditionalYear {
        self.1.year_containing_rd(rd)
    }

    type DateCompatibilityError = R::DateCompatibilityError;

    fn check_date_compatibility(&self, other: &Self) -> Result<(), Self::DateCompatibilityError> {
        self.1.check_date_compatibility(&other.1)
    }
}

#[cfg(test)]
mod test {
    use super::*;
    use crate::cal::Iso;
    use crate::options::{DateFromFieldsOptions, Overflow};
    use calendrical_calculations::{gregorian::fixed_from_gregorian, rata_die::RataDie};
    use std::collections::BTreeMap;
    use types::DateFields;
    use types::Month;

    #[test]
    fn test_min_months_invariant() {
        fn inner<R: Rules>(cal: R) {
            let mut num_leap = 0;
            let mut gap = 0;

            let smallest =
                cal.year_containing_rd(*crate::calendar_arithmetic::VALID_RD_RANGE.start());
            let largest = cal.year_containing_rd(*crate::calendar_arithmetic::VALID_RD_RANGE.end());

            for y in smallest.related_iso()..=largest.related_iso() {
                if cal.year(y).packed.leap_month().is_none() {
                    gap += 1;
                } else {
                    num_leap += 1;
                    gap = 0;
                }
                if gap == 3 {
                    panic!("{y}");
                }
            }

            let total = (largest - smallest + 1) * 12 + num_leap;
            let approximated =
                EastAsianTraditional::<R>::min_months_from(smallest, (largest - smallest) + 1);

            println!(
                "absolute error {}: {}",
                cal.debug_name(),
                total - approximated
            );
        }

        inner(EastAsianTraditionalYears::china());
        inner(EastAsianTraditionalYears::korea());
    }

    #[test]
    fn test_chinese_from_rd() {
        #[derive(Debug)]
        struct TestCase {
            rd: i64,
            expected_year: i32,
            expected_month: u8,
            expected_day: u8,
        }

        let cases = [
            TestCase {
                rd: -964192,
                expected_year: -2639,
                expected_month: 1,
                expected_day: 1,
            },
            TestCase {
                rd: -963838,
                expected_year: -2638,
                expected_month: 1,
                expected_day: 1,
            },
            TestCase {
                rd: -963129,
                expected_year: -2637,
                expected_month: 13,
                expected_day: 1,
            },
            TestCase {
                rd: -963100,
                expected_year: -2637,
                expected_month: 13,
                expected_day: 30,
            },
            TestCase {
                rd: -963099,
                expected_year: -2636,
                expected_month: 1,
                expected_day: 1,
            },
            TestCase {
                rd: 738700,
                expected_year: 2023,
                expected_month: 6,
                expected_day: 12,
            },
            TestCase {
                rd: fixed_from_gregorian(2319, 2, 20).to_i64_date(),
                expected_year: 2318,
                expected_month: 13,
                expected_day: 30,
            },
            TestCase {
                rd: fixed_from_gregorian(2319, 2, 21).to_i64_date(),
                expected_year: 2319,
                expected_month: 1,
                expected_day: 1,
            },
            TestCase {
                rd: 738718,
                expected_year: 2023,
                expected_month: 6,
                expected_day: 30,
            },
            TestCase {
                rd: 738747,
                expected_year: 2023,
                expected_month: 7,
                expected_day: 29,
            },
            TestCase {
                rd: 738748,
                expected_year: 2023,
                expected_month: 8,
                expected_day: 1,
            },
            TestCase {
                rd: 738865,
                expected_year: 2023,
                expected_month: 11,
                expected_day: 29,
            },
            TestCase {
                rd: 738895,
                expected_year: 2023,
                expected_month: 12,
                expected_day: 29,
            },
            TestCase {
                rd: 738925,
                expected_year: 2023,
                expected_month: 13,
                expected_day: 30,
            },
            TestCase {
                rd: 0,
                expected_year: 0,
                expected_month: 11,
                expected_day: 19,
            },
            TestCase {
                rd: -1,
                expected_year: 0,
                expected_month: 11,
                expected_day: 18,
            },
            TestCase {
                rd: -365,
                expected_year: -1,
                expected_month: 12,
                expected_day: 9,
            },
            TestCase {
                rd: 100,
                expected_year: 1,
                expected_month: 3,
                expected_day: 1,
            },
        ];

        for case in cases {
            let rata_die = RataDie::new(case.rd);

            let chinese = Date::from_rata_die(rata_die, ChineseTraditional::new());
            assert_eq!(
                case.expected_year,
                chinese.year().extended_year(),
                "Chinese from RD failed, case: {case:?}"
            );
            assert_eq!(
                case.expected_month,
                chinese.month().ordinal,
                "Chinese from RD failed, case: {case:?}"
            );
            assert_eq!(
                case.expected_day,
                chinese.day_of_month().0,
                "Chinese from RD failed, case: {case:?}"
            );
        }
    }

    #[test]
    fn test_rd_from_chinese() {
        #[derive(Debug)]
        struct TestCase {
            year: i32,
            ordinal_month: u8,
            month: Month,
            day: u8,
            expected: i64,
        }

        let cases = [
            TestCase {
                year: 2023,
                ordinal_month: 6,
                month: Month::new(5),
                day: 6,
                // June 23 2023
                expected: 738694,
            },
            TestCase {
                year: -2636,
                ordinal_month: 1,
                month: Month::new(1),
                day: 1,
                expected: -963099,
            },
        ];

        for case in cases {
            let date = Date::try_new_chinese_traditional(case.year, case.month, case.day).unwrap();
            #[allow(deprecated)] // should still test
            {
                assert_eq!(
                    Date::try_new_chinese_with_calendar(
                        case.year,
                        case.ordinal_month,
                        case.day,
                        ChineseTraditional::new()
                    ),
                    Ok(date)
                );
            }
            let rd = date.to_rata_die().to_i64_date();
            let expected = case.expected;
            assert_eq!(rd, expected, "RD from Chinese failed, with expected: {expected} and calculated: {rd}, for test case: {case:?}");
        }
    }

    #[test]
    fn test_rd_chinese_roundtrip() {
        let mut rd = -1963020;
        let max_rd = 1963020;
        let mut iters = 0;
        let max_iters = 560;
        while rd < max_rd && iters < max_iters {
            let rata_die = RataDie::new(rd);

            let chinese = Date::from_rata_die(rata_die, ChineseTraditional::new());
            let result = chinese.to_rata_die();
            assert_eq!(result, rata_die, "Failed roundtrip RD -> Chinese -> RD for RD: {rata_die:?}, with calculated: {result:?} from Chinese date:\n{chinese:?}");

            rd += 7043;
            iters += 1;
        }
    }

    #[test]
    fn test_chinese_epoch() {
        let iso = Date::try_new_iso(-2636, 2, 15).unwrap();

        let chinese = iso.to_calendar(ChineseTraditional::new());

        assert_eq!(chinese.cyclic_year().related_iso, -2636);
        assert_eq!(chinese.month().ordinal, 1);
        assert_eq!(chinese.month().to_input(), Month::new(1));
        assert_eq!(chinese.day_of_month().0, 1);
        assert_eq!(chinese.cyclic_year().year, 1);
        assert_eq!(chinese.cyclic_year().related_iso, -2636);
    }

    #[test]
    fn test_iso_to_chinese_negative_years() {
        #[derive(Debug)]
        struct TestCase {
            iso_year: i32,
            iso_month: u8,
            iso_day: u8,
            expected_year: i32,
            expected_month: u8,
            expected_day: u8,
        }

        let cases = [
            TestCase {
                iso_year: -2636,
                iso_month: 2,
                iso_day: 14,
                expected_year: -2637,
                expected_month: 13,
                expected_day: 30,
            },
            TestCase {
                iso_year: -2636,
                iso_month: 1,
                iso_day: 15,
                expected_year: -2637,
                expected_month: 12,
                expected_day: 29,
            },
        ];

        for case in cases {
            let iso = Date::try_new_iso(case.iso_year, case.iso_month, case.iso_day).unwrap();

            let chinese = iso.to_calendar(ChineseTraditional::new());
            assert_eq!(
                case.expected_year,
                chinese.cyclic_year().related_iso,
                "ISO to Chinese failed for case: {case:?}"
            );
            assert_eq!(
                case.expected_month,
                chinese.month().ordinal,
                "ISO to Chinese failed for case: {case:?}"
            );
            assert_eq!(
                case.expected_day,
                chinese.day_of_month().0,
                "ISO to Chinese failed for case: {case:?}"
            );
        }
    }

    #[test]
    fn test_month_days() {
        let year = ChineseTraditional::new().0.year(2023);
        let cases = [
            (1, 29),
            (2, 30),
            (3, 29),
            (4, 29),
            (5, 30),
            (6, 30),
            (7, 29),
            (8, 30),
            (9, 30),
            (10, 29),
            (11, 30),
            (12, 29),
            (13, 30),
        ];
        for case in cases {
            let days_in_month = EastAsianTraditional::<China>::days_in_provided_month(year, case.0);
            assert_eq!(
                case.1, days_in_month,
                "month_days test failed for case: {case:?}"
            );
        }
    }

    #[test]
    fn test_ordinal_to_month() {
        #[derive(Debug)]
        struct TestCase {
            iso_year: i32,
            iso_month: u8,
            iso_day: u8,
            month: Month,
        }

        let cases = [
            TestCase {
                iso_year: 2023,
                iso_month: 1,
                iso_day: 9,
                month: Month::new(12),
            },
            TestCase {
                iso_year: 2023,
                iso_month: 2,
                iso_day: 9,
                month: Month::new(1),
            },
            TestCase {
                iso_year: 2023,
                iso_month: 3,
                iso_day: 9,
                month: Month::new(2),
            },
            TestCase {
                iso_year: 2023,
                iso_month: 4,
                iso_day: 9,
                month: Month::leap(2),
            },
            TestCase {
                iso_year: 2023,
                iso_month: 5,
                iso_day: 9,
                month: Month::new(3),
            },
            TestCase {
                iso_year: 2023,
                iso_month: 6,
                iso_day: 9,
                month: Month::new(4),
            },
            TestCase {
                iso_year: 2023,
                iso_month: 7,
                iso_day: 9,
                month: Month::new(5),
            },
            TestCase {
                iso_year: 2023,
                iso_month: 8,
                iso_day: 9,
                month: Month::new(6),
            },
            TestCase {
                iso_year: 2023,
                iso_month: 9,
                iso_day: 9,
                month: Month::new(7),
            },
            TestCase {
                iso_year: 2023,
                iso_month: 10,
                iso_day: 9,
                month: Month::new(8),
            },
            TestCase {
                iso_year: 2023,
                iso_month: 11,
                iso_day: 9,
                month: Month::new(9),
            },
            TestCase {
                iso_year: 2023,
                iso_month: 12,
                iso_day: 9,
                month: Month::new(10),
            },
            TestCase {
                iso_year: 2024,
                iso_month: 1,
                iso_day: 9,
                month: Month::new(11),
            },
            TestCase {
                iso_year: 2024,
                iso_month: 2,
                iso_day: 9,
                month: Month::new(12),
            },
            TestCase {
                iso_year: 2024,
                iso_month: 2,
                iso_day: 10,
                month: Month::new(1),
            },
        ];

        for case in cases {
            let iso = Date::try_new_iso(case.iso_year, case.iso_month, case.iso_day).unwrap();
            let chinese = iso.to_calendar(ChineseTraditional::new());
            assert_eq!(
                chinese.month().to_input(),
                case.month,
                "Month codes did not match for test case: {case:?}"
            );
        }
    }

    #[test]
    fn test_month_to_ordinal() {
        let cal = ChineseTraditional::new();
        let year = cal.year_info_from_extended(2023);
        for (ordinal, month) in [
            Month::new(1),
            Month::new(2),
            Month::leap(2),
            Month::new(3),
            Month::new(4),
            Month::new(5),
            Month::new(6),
            Month::new(7),
            Month::new(8),
            Month::new(9),
            Month::new(10),
            Month::new(11),
            Month::new(12),
        ]
        .into_iter()
        .enumerate()
        {
            let ordinal = ordinal as u8 + 1;
            assert_eq!(
                cal.ordinal_from_month(year, month, Overflow::Reject),
                Ok(ordinal),
                "Code to ordinal failed for year: {}, code: {ordinal}",
                year.related_iso
            );
        }
    }

    #[test]
    fn check_invalid_month_to_ordinal() {
        let cal = ChineseTraditional::new();
        for year in [4659, 4660] {
            let year = cal.year_info_from_extended(year);
            for (month, error) in [
                (Month::leap(4), MonthError::NotInYear),
                (Month::new(13), MonthError::NotInCalendar),
            ] {
                assert_eq!(
                    cal.ordinal_from_month(year, month, Overflow::Reject),
                    Err(error),
                    "Invalid month code failed for year: {}, code: {month:?}",
                    year.related_iso,
                );
            }
        }
    }

    #[test]
    fn test_iso_chinese_roundtrip() {
        for i in -1000..=1000 {
            let year = i;
            let month = i as u8 % 12 + 1;
            let day = i as u8 % 28 + 1;
            let iso = Date::try_new_iso(year, month, day).unwrap();
            let chinese = iso.to_calendar(ChineseTraditional::new());
            let result = chinese.to_calendar(Iso);
            assert_eq!(iso, result, "ISO to Chinese roundtrip failed!\nIso: {iso:?}\nChinese: {chinese:?}\nResult: {result:?}");
        }
    }

    fn check_cyclic_and_rel_iso(year: i32) {
        let iso = Date::try_new_iso(year, 6, 6).unwrap();
        let chinese = iso.to_calendar(ChineseTraditional::new());
        let korean = iso.to_calendar(KoreanTraditional::new());
        let chinese_year = chinese.cyclic_year();
        let korean_year = korean.cyclic_year();
        assert_eq!(
            chinese_year, korean_year,
            "Cyclic year failed for year: {year}"
        );
        let chinese_rel_iso = chinese_year.related_iso;
        let korean_rel_iso = korean_year.related_iso;
        assert_eq!(
            chinese_rel_iso, korean_rel_iso,
            "Rel. ISO year equality failed for year: {year}"
        );
        assert_eq!(korean_rel_iso, year, "Korean Rel. ISO failed!");
    }

    #[test]
    fn test_cyclic_same_as_chinese_near_present_day() {
        for year in 1923..=2123 {
            check_cyclic_and_rel_iso(year);
        }
    }

    #[test]
    fn test_cyclic_same_as_chinese_near_rd_zero() {
        for year in -100..=100 {
            check_cyclic_and_rel_iso(year);
        }
    }

    #[test]
    fn test_iso_to_korean_roundtrip() {
        let mut rd = -1963020;
        let max_rd = 1963020;
        let mut iters = 0;
        let max_iters = 560;
        while rd < max_rd && iters < max_iters {
            let rata_die = RataDie::new(rd);
            let iso = Date::from_rata_die(rata_die, Iso);
            let korean = iso.to_calendar(KoreanTraditional::new());
            let result = korean.to_calendar(Iso);
            assert_eq!(
                iso, result,
                "Failed roundtrip ISO -> Korean -> ISO for RD: {rd}"
            );

            rd += 7043;
            iters += 1;
        }
    }

    #[test]
    fn test_from_fields_constrain() {
        let fields = DateFields {
            day: Some(31),
            month: Some(Month::new(1)),
            extended_year: Some(1972),
            ..Default::default()
        };
        let options = DateFromFieldsOptions {
            overflow: Some(Overflow::Constrain),
            ..Default::default()
        };

        let cal = ChineseTraditional::new();
        let date = Date::try_from_fields(fields, options, cal).unwrap();
        assert_eq!(
            date.day_of_month().0,
            29,
            "Day was successfully constrained"
        );

        // 2022 did not have M01L, the month should be constrained back down
        let fields = DateFields {
            day: Some(1),
            month: Some(Month::leap(1)),
            extended_year: Some(2022),
            ..Default::default()
        };
        let date = Date::try_from_fields(fields, options, cal).unwrap();
        assert_eq!(
            date.month().to_input(),
            Month::new(1),
            "Month was successfully constrained"
        );
    }

    #[test]
    fn test_from_fields_regress_7049() {
        // We want to make sure that overly large years do not panic
        // (we just reject them in Date::try_from_fields)
        let fields = DateFields {
            extended_year: Some(889192448),
            ordinal_month: Some(1),
            day: Some(1),
            ..Default::default()
        };
        let options = DateFromFieldsOptions {
            overflow: Some(Overflow::Reject),
            ..Default::default()
        };

        let cal = ChineseTraditional::new();
        assert!(matches!(
            Date::try_from_fields(fields, options, cal).unwrap_err(),
            DateFromFieldsError::Overflow
        ));
    }

    #[test]
    #[ignore] // slow, network
    fn test_against_hong_kong_observatory_data() {
        use crate::{cal::Gregorian, Date};

        let mut related_iso = 1900;
        let mut lunar_month = Month::new(11);

        for year in 1901..=2100 {
            println!("Validating year {year}...");

            for line in ureq::get(&format!(
                "https://www.hko.gov.hk/en/gts/time/calendar/text/files/T{year}e.txt"
            ))
            .call()
            .unwrap()
            .body_mut()
            .read_to_string()
            .unwrap()
            .split('\n')
            {
                if !line.starts_with(['1', '2']) {
                    // comments or blank lines
                    continue;
                }

                let mut fields = line.split_ascii_whitespace();

                let mut gregorian = fields.next().unwrap().split('/');
                let gregorian = Date::try_new_gregorian(
                    gregorian.next().unwrap().parse().unwrap(),
                    gregorian.next().unwrap().parse().unwrap(),
                    gregorian.next().unwrap().parse().unwrap(),
                )
                .unwrap();

                let day_or_lunar_month = fields.next().unwrap();

                let lunar_day = if fields.next().is_some_and(|s| s.contains("Lunar")) {
                    let new_lunar_month = day_or_lunar_month
                        // 1st, 2nd, 3rd, nth
                        .split_once(['s', 'n', 'r', 't'])
                        .unwrap()
                        .0
                        .parse()
                        .unwrap();
                    lunar_month = if new_lunar_month == lunar_month.number() {
                        Month::leap(new_lunar_month)
                    } else {
                        Month::new(new_lunar_month)
                    };
                    if new_lunar_month == 1 {
                        related_iso += 1;
                    }
                    1
                } else {
                    day_or_lunar_month.parse().unwrap()
                };

                let chinese =
                    Date::try_new_chinese_traditional(related_iso, lunar_month, lunar_day).unwrap();

                assert_eq!(
                    gregorian,
                    chinese.to_calendar(Gregorian),
                    "{line}, {chinese:?}"
                );
            }
        }
    }

    #[test]
    #[ignore] // network
    fn test_against_kasi_data() {
        use crate::{cal::Gregorian, Date};

        // TODO: query KASI directly
        let uri = "https://gist.githubusercontent.com/Manishearth/d8c94a7df22a9eacefc4472a5805322e/raw/e1ea3b0aa52428686bb3a9cd0f262878515e16c1/resolved.json";

        #[derive(serde::Deserialize)]
        struct Golden(BTreeMap<i32, BTreeMap<String, MonthData>>);

        #[derive(serde::Deserialize)]
        struct MonthData {
            start_date: String,
        }

        let json = ureq::get(uri)
            .call()
            .unwrap()
            .body_mut()
            .read_to_string()
            .unwrap();

        let golden = serde_json::from_str::<Golden>(&json).unwrap();

        for (year, months) in golden.0 {
            if year == 1899 || year == 2050 {
                continue;
            }
            for (month_code, month_data) in months {
                let mut gregorian = month_data.start_date.split('-');
                let gregorian = Date::try_new_gregorian(
                    gregorian.next().unwrap().parse().unwrap(),
                    gregorian.next().unwrap().parse().unwrap(),
                    gregorian.next().unwrap().parse().unwrap(),
                )
                .unwrap();

                assert_eq!(
                    Date::try_new_korean_traditional(
                        year,
                        Month::try_from_str(&month_code).unwrap(),
                        1
                    )
                    .unwrap()
                    .to_calendar(Gregorian),
                    gregorian
                );
            }
        }
    }

    #[test]
    #[ignore]
    fn generate_reference_years() {
        generate_reference_years_for(ChineseTraditional::new());
        generate_reference_years_for(KoreanTraditional::new());
        fn generate_reference_years_for<R: Rules + Copy>(calendar: EastAsianTraditional<R>) {
            use crate::Date;

            println!("Reference years for {calendar:?}:");
            let reference_year_end = Date::from_rata_die(
                crate::cal::abstract_gregorian::LAST_DAY_OF_REFERENCE_YEAR,
                calendar,
            );
            let year_1900_start = Date::try_new_gregorian(1900, 1, 1)
                .unwrap()
                .to_calendar(calendar);
            let year_2035_end = Date::try_new_gregorian(2035, 12, 31)
                .unwrap()
                .to_calendar(calendar);
            for month in 1..=12 {
                for leap in [false, true] {
                    'outer: for long in [false, true] {
                        for (start_year, start_month, end_year, end_month, by) in [
                            (
                                reference_year_end.year().extended_year(),
                                reference_year_end.month().number(),
                                year_1900_start.year().extended_year(),
                                year_1900_start.month().number(),
                                -1,
                            ),
                            (
                                reference_year_end.year().extended_year(),
                                reference_year_end.month().number(),
                                year_2035_end.year().extended_year(),
                                year_2035_end.month().number(),
                                1,
                            ),
                            (
                                year_1900_start.year().extended_year(),
                                year_1900_start.month().number(),
                                -10000,
                                1,
                                -1,
                            ),
                        ] {
                            let mut year = start_year;
                            while year * by < end_year * by {
                                if year == start_year
                                    && month as i32 * by <= start_month as i32 * by
                                    || year == end_year
                                        && month as i32 * by >= end_month as i32 * by
                                {
                                    year += by;
                                    continue;
                                }
                                let data = calendar.0.year(year);
                                let leap_month = data.packed.leap_month().unwrap_or(15);
                                let ordinal_month = if leap && month + 1 == leap_month {
                                    month + 1
                                } else {
                                    month + (month + 1 > leap_month) as u8
                                };
                                if (!long || data.packed.month_len(ordinal_month) == 30)
                                    && (!leap || month + 1 == leap_month)
                                {
                                    println!("({month}, {leap:?}, {long:?}) => {year},");
                                    continue 'outer;
                                }
                                year += by;
                            }
                        }
                        println!("({month}, {leap:?}, {long:?}) => todo!(),")
                    }
                }
            }
        }
    }

    #[test]
    fn test_roundtrip_packed() {
        fn packed_roundtrip_single(
            month_lengths: [bool; 13],
            leap_month_idx: Option<u8>,
            ny_offset: i64,
        ) {
            let ny = fixed_from_gregorian(1000, 1, 1) + ny_offset;
            let packed =
                PackedEastAsianTraditionalYearData::new(1000, month_lengths, leap_month_idx, ny);

            assert_eq!(
                ny,
                packed.new_year(1000),
                "Roundtrip with {month_lengths:?}, {leap_month_idx:?}, {ny_offset}"
            );
            assert_eq!(
                leap_month_idx,
                packed.leap_month(),
                "Roundtrip with {month_lengths:?}, {leap_month_idx:?}, {ny_offset}"
            );
            assert_eq!(
                month_lengths,
                core::array::from_fn(|i| packed.month_len(i as u8 + 1) == 30),
                "Roundtrip with {month_lengths:?}, {leap_month_idx:?}, {ny_offset}"
            );
        }

        const SHORT: [bool; 13] = [false; 13];
        const LONG: [bool; 13] = [true; 13];
        const ALTERNATING1: [bool; 13] = [
            false, true, false, true, false, true, false, true, false, true, false, true, false,
        ];
        const ALTERNATING2: [bool; 13] = [
            true, false, true, false, true, false, true, false, true, false, true, false, false,
        ];
        const RANDOM1: [bool; 13] = [
            true, true, false, false, true, true, false, true, true, true, true, false, false,
        ];
        const RANDOM2: [bool; 13] = [
            false, true, true, true, true, false, true, true, true, false, false, true, false,
        ];
        packed_roundtrip_single(SHORT, None, 18 + 5);
        packed_roundtrip_single(SHORT, None, 18 + 10);
        packed_roundtrip_single(SHORT, Some(11), 18 + 15);
        packed_roundtrip_single(LONG, Some(12), 18 + 15);
        packed_roundtrip_single(ALTERNATING1, None, 18 + 2);
        packed_roundtrip_single(ALTERNATING1, Some(3), 18 + 5);
        packed_roundtrip_single(ALTERNATING2, None, 18 + 9);
        packed_roundtrip_single(ALTERNATING2, Some(7), 18 + 26);
        packed_roundtrip_single(RANDOM1, None, 18 + 29);
        packed_roundtrip_single(RANDOM1, Some(12), 18 + 29);
        packed_roundtrip_single(RANDOM1, Some(2), 18 + 21);
        packed_roundtrip_single(RANDOM2, None, 18 + 25);
        packed_roundtrip_single(RANDOM2, Some(2), 18 + 19);
        packed_roundtrip_single(RANDOM2, Some(5), 18 + 2);
        packed_roundtrip_single(RANDOM2, Some(12), 18 + 5);
    }
}
