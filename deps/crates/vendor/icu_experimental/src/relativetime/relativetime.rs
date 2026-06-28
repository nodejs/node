// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

use fixed_decimal::{Decimal, Sign};
use icu_decimal::{
    options::DecimalFormatterOptions, provider::DecimalDigitsV1, provider::DecimalSymbolsV1,
    DecimalFormatter, DecimalFormatterPreferences,
};
use icu_locale_core::preferences::{define_preferences, prefs_convert};
use icu_plurals::PluralRulesPreferences;
use icu_plurals::{provider::PluralsCardinalV1, PluralRules};
use icu_provider::marker::ErasedMarker;
use icu_provider::prelude::*;

use crate::relativetime::format::FormattedRelativeTime;
use crate::relativetime::options::RelativeTimeFormatterOptions;
use crate::relativetime::provider::*;

define_preferences!(
    /// The preferences for relative time formatting.
    [Copy]
    RelativeTimeFormatterPreferences,
    {
        /// The user's preferred numbering system.
        ///
        /// Corresponds to the `-u-nu` in Unicode Locale Identifier.
        ///
        /// To get the resolved numbering system, you can inspect the data provider.
        /// See the [`provider`] module for an example.
        numbering_system: preferences::NumberingSystem
    }
);
prefs_convert!(
    RelativeTimeFormatterPreferences,
    DecimalFormatterPreferences,
    { numbering_system }
);
prefs_convert!(RelativeTimeFormatterPreferences, PluralRulesPreferences);

/// Locale preferences used by this crate
pub mod preferences {
    /// **This is a reexport of a type in [`icu::locale`](icu_locale_core::preferences::extensions::unicode::keywords)**.
    #[doc = "\n"] // prevent autoformatting
    pub use icu_locale_core::preferences::extensions::unicode::keywords::NumberingSystem;
}

/// A formatter to render locale-sensitive relative time.
///
/// # Example
///
/// ```
/// use fixed_decimal::Decimal;
/// use icu::experimental::relativetime::{
///     RelativeTimeFormatter, RelativeTimeFormatterOptions,
/// };
/// use icu::locale::locale;
/// use writeable::assert_writeable_eq;
///
/// let relative_time_formatter = RelativeTimeFormatter::try_new_long_second(
///     locale!("en").into(),
///     RelativeTimeFormatterOptions::default(),
/// )
/// .expect("locale should be present");
///
/// assert_writeable_eq!(
///     relative_time_formatter.format(Decimal::from(5i8)),
///     "in 5 seconds"
/// );
/// assert_writeable_eq!(
///     relative_time_formatter.format(Decimal::from(-10i8)),
///     "10 seconds ago"
/// );
/// ```
///
/// # Example
///
/// ```
/// use fixed_decimal::Decimal;
/// use icu::experimental::relativetime::options::Numeric;
/// use icu::experimental::relativetime::{
///     RelativeTimeFormatter, RelativeTimeFormatterOptions,
/// };
/// use icu::locale::locale;
/// use writeable::assert_writeable_eq;
///
/// let mut options = RelativeTimeFormatterOptions::default();
/// options.numeric = Numeric::Auto;
///
/// let relative_time_formatter =
///     RelativeTimeFormatter::try_new_short_day(locale!("es").into(), options)
///         .expect("locale should be present");
///
/// assert_writeable_eq!(
///     relative_time_formatter.format(Decimal::from(0u8)),
///     "hoy"
/// );
/// assert_writeable_eq!(
///     relative_time_formatter.format(Decimal::from(-2i8)),
///     "anteayer"
/// );
/// assert_writeable_eq!(
///     relative_time_formatter.format(Decimal::from(2u8)),
///     "pasado mañana"
/// );
/// assert_writeable_eq!(
///     relative_time_formatter.format(Decimal::from(15i8)),
///     "dentro de 15 d"
/// );
/// ```
///
/// # Example
/// ```
/// use fixed_decimal::Decimal;
/// use icu::experimental::relativetime::{
///     RelativeTimeFormatter, RelativeTimeFormatterOptions,
/// };
/// use icu::locale::locale;
/// use writeable::assert_writeable_eq;
///
/// let relative_time_formatter = RelativeTimeFormatter::try_new_narrow_year(
///     locale!("bn").into(),
///     RelativeTimeFormatterOptions::default(),
/// )
/// .expect("locale should be present");
///
/// assert_writeable_eq!(
///     relative_time_formatter.format(Decimal::from(3u8)),
///     "৩ বছরে"
/// );
/// assert_writeable_eq!(
///     relative_time_formatter.format(Decimal::from(-15i8)),
///     "১৫ বছর পূর্বে"
/// );
/// ```
#[derive(Debug)]
pub struct RelativeTimeFormatter {
    pub(crate) plural_rules: PluralRules,
    pub(crate) rt: DataPayload<ErasedMarker<RelativeTimePatternData<'static>>>,
    pub(crate) options: RelativeTimeFormatterOptions,
    pub(crate) decimal_formatter: DecimalFormatter,
}

macro_rules! constructor {
    ($unstable: ident, $baked: ident, $buffer: ident, $marker: ty) => {

        /// Create a new [`RelativeTimeFormatter`] from compiled data.
        ///
        /// ✨ *Enabled with the `compiled_data` Cargo feature.*
        ///
        /// [📚 Help choosing a constructor](icu_provider::constructors)
        #[cfg(feature = "compiled_data")]
        pub fn $baked(
            prefs: RelativeTimeFormatterPreferences,
            options: RelativeTimeFormatterOptions,
        ) -> Result<Self, DataError> {
            let locale = <$marker>::make_locale(prefs.locale_preferences);
            let plural_rules = PluralRules::try_new_cardinal((&prefs).into())?;
            // Initialize DecimalFormatter with default options
            let decimal_formatter = DecimalFormatter::try_new(
                (&prefs).into(),
                DecimalFormatterOptions::default(),
            )?;
            let rt: DataResponse<$marker> = crate::provider::Baked
                .load(DataRequest {
                    id: DataIdentifierBorrowed::for_locale(&locale),
                    ..Default::default()
                })?;
            let rt = rt.payload.cast();
            Ok(RelativeTimeFormatter {
                plural_rules,
                options,
                rt,
                decimal_formatter,
            })
        }

        icu_provider::gen_buffer_data_constructors!(
            (prefs: RelativeTimeFormatterPreferences, options: RelativeTimeFormatterOptions) -> error: DataError,
            functions: [
                $baked: skip,
                $buffer,
                $unstable,
                Self,
            ]
        );


        #[doc = icu_provider::gen_buffer_unstable_docs!(UNSTABLE, Self::$baked)]
        pub fn $unstable<D>(
            provider: &D,
            prefs: RelativeTimeFormatterPreferences,
            options: RelativeTimeFormatterOptions,
        ) -> Result<Self, DataError>
        where
            D: DataProvider<PluralsCardinalV1>
                + DataProvider<$marker>
                + DataProvider<DecimalSymbolsV1> + DataProvider<DecimalDigitsV1>
                + ?Sized,
        {
            let locale = <$marker>::make_locale(prefs.locale_preferences);
            let plural_rules = PluralRules::try_new_cardinal_unstable(provider, (&prefs).into())?;
            // Initialize DecimalFormatter with default options
            let decimal_formatter = DecimalFormatter::try_new_unstable(
                provider,
                (&prefs).into(),
                DecimalFormatterOptions::default(),
            )?;
            let rt: DataResponse<$marker> = provider
                .load(DataRequest {
                id: DataIdentifierBorrowed::for_locale(&locale),
                    ..Default::default()
                })?;
            let rt = rt.payload.cast();
            Ok(RelativeTimeFormatter {
                plural_rules,
                options,
                rt,
                decimal_formatter,
            })
        }
    };
}

impl RelativeTimeFormatter {
    constructor!(
        try_new_long_second_unstable,
        try_new_long_second,
        try_new_long_second_with_buffer_provider,
        LongSecondRelativeV1
    );
    constructor!(
        try_new_long_minute_unstable,
        try_new_long_minute,
        try_new_long_minute_with_buffer_provider,
        LongMinuteRelativeV1
    );
    constructor!(
        try_new_long_hour_unstable,
        try_new_long_hour,
        try_new_long_hour_with_buffer_provider,
        LongHourRelativeV1
    );
    constructor!(
        try_new_long_day_unstable,
        try_new_long_day,
        try_new_long_day_with_buffer_provider,
        LongDayRelativeV1
    );
    constructor!(
        try_new_long_week_unstable,
        try_new_long_week,
        try_new_long_week_with_buffer_provider,
        LongWeekRelativeV1
    );
    constructor!(
        try_new_long_month_unstable,
        try_new_long_month,
        try_new_long_month_with_buffer_provider,
        LongMonthRelativeV1
    );
    constructor!(
        try_new_long_quarter_unstable,
        try_new_long_quarter,
        try_new_long_quarter_with_buffer_provider,
        LongQuarterRelativeV1
    );
    constructor!(
        try_new_long_year_unstable,
        try_new_long_year,
        try_new_long_year_with_buffer_provider,
        LongYearRelativeV1
    );
    constructor!(
        try_new_short_second_unstable,
        try_new_short_second,
        try_new_short_second_with_buffer_provider,
        ShortSecondRelativeV1
    );
    constructor!(
        try_new_short_minute_unstable,
        try_new_short_minute,
        try_new_short_minute_with_buffer_provider,
        ShortMinuteRelativeV1
    );
    constructor!(
        try_new_short_hour_unstable,
        try_new_short_hour,
        try_new_short_hour_with_buffer_provider,
        ShortHourRelativeV1
    );
    constructor!(
        try_new_short_day_unstable,
        try_new_short_day,
        try_new_short_day_with_buffer_provider,
        ShortDayRelativeV1
    );
    constructor!(
        try_new_short_week_unstable,
        try_new_short_week,
        try_new_short_week_with_buffer_provider,
        ShortWeekRelativeV1
    );
    constructor!(
        try_new_short_month_unstable,
        try_new_short_month,
        try_new_short_month_with_buffer_provider,
        ShortMonthRelativeV1
    );
    constructor!(
        try_new_short_quarter_unstable,
        try_new_short_quarter,
        try_new_short_quarter_with_buffer_provider,
        ShortQuarterRelativeV1
    );
    constructor!(
        try_new_short_year_unstable,
        try_new_short_year,
        try_new_short_year_with_buffer_provider,
        ShortYearRelativeV1
    );
    constructor!(
        try_new_narrow_second_unstable,
        try_new_narrow_second,
        try_new_narrow_second_with_buffer_provider,
        NarrowSecondRelativeV1
    );
    constructor!(
        try_new_narrow_minute_unstable,
        try_new_narrow_minute,
        try_new_narrow_minute_with_buffer_provider,
        NarrowMinuteRelativeV1
    );
    constructor!(
        try_new_narrow_hour_unstable,
        try_new_narrow_hour,
        try_new_narrow_hour_with_buffer_provider,
        NarrowHourRelativeV1
    );
    constructor!(
        try_new_narrow_day_unstable,
        try_new_narrow_day,
        try_new_narrow_day_with_buffer_provider,
        NarrowDayRelativeV1
    );
    constructor!(
        try_new_narrow_week_unstable,
        try_new_narrow_week,
        try_new_narrow_week_with_buffer_provider,
        NarrowWeekRelativeV1
    );
    constructor!(
        try_new_narrow_month_unstable,
        try_new_narrow_month,
        try_new_narrow_month_with_buffer_provider,
        NarrowMonthRelativeV1
    );
    constructor!(
        try_new_narrow_quarter_unstable,
        try_new_narrow_quarter,
        try_new_narrow_quarter_with_buffer_provider,
        NarrowQuarterRelativeV1
    );
    constructor!(
        try_new_narrow_year_unstable,
        try_new_narrow_year,
        try_new_narrow_year_with_buffer_provider,
        NarrowYearRelativeV1
    );

    /// Format a `value` according to the locale and formatting options of
    /// [`RelativeTimeFormatter`].
    pub fn format(&self, value: Decimal) -> FormattedRelativeTime<'_> {
        let is_negative = value.sign() == Sign::Negative;
        FormattedRelativeTime {
            options: &self.options,
            formatter: self,
            value: value.with_sign(Sign::None),
            is_negative,
        }
    }
}
