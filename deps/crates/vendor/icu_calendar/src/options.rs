// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

//! Options used by types in this crate

#[cfg(feature = "unstable")]
pub use unstable::{
    DateAddOptions, DateDifferenceOptions, DateFromFieldsOptions, MissingFieldsStrategy, Overflow,
};
#[cfg(not(feature = "unstable"))]
pub(crate) use unstable::{
    DateAddOptions, DateDifferenceOptions, DateFromFieldsOptions, MissingFieldsStrategy, Overflow,
};

mod unstable {
    /// Options bag for [`Date::try_from_fields`](crate::Date::try_from_fields).
    ///
    /// <div class="stab unstable">
    /// 🚧 This code is considered unstable; it may change at any time, in breaking or non-breaking ways,
    /// including in SemVer minor releases. Do not use this type unless you are prepared for things to occasionally break.
    ///
    /// Graduation tracking issue: [issue #7161](https://github.com/unicode-org/icu4x/issues/7161).
    /// </div>
    ///
    /// ✨ *Enabled with the `unstable` Cargo feature.*
    #[derive(Copy, Clone, Debug, PartialEq, Default)]
    #[non_exhaustive]
    pub struct DateFromFieldsOptions {
        /// How to behave with out-of-bounds fields.
        ///
        /// Defaults to [`Overflow::Reject`].
        ///
        /// # Examples
        ///
        /// ```
        /// use icu::calendar::options::DateFromFieldsOptions;
        /// use icu::calendar::options::Overflow;
        /// use icu::calendar::types::DateFields;
        /// use icu::calendar::Date;
        /// use icu::calendar::Iso;
        ///
        /// // There is no day 31 in September.
        /// let mut fields = DateFields::default();
        /// fields.extended_year = Some(2025);
        /// fields.ordinal_month = Some(9);
        /// fields.day = Some(31);
        ///
        /// let options_default = DateFromFieldsOptions::default();
        /// assert!(Date::try_from_fields(fields, options_default, Iso).is_err());
        ///
        /// let mut options_reject = options_default;
        /// options_reject.overflow = Some(Overflow::Reject);
        /// assert!(Date::try_from_fields(fields, options_reject, Iso).is_err());
        ///
        /// let mut options_constrain = options_default;
        /// options_constrain.overflow = Some(Overflow::Constrain);
        /// assert_eq!(
        ///     Date::try_from_fields(fields, options_constrain, Iso).unwrap(),
        ///     Date::try_new_iso(2025, 9, 30).unwrap()
        /// );
        /// ```
        pub overflow: Option<Overflow>,
        /// How to behave when the fields that are present do not fully constitute a Date.
        ///
        /// This option can be used to fill in a missing year given a month and a day according to
        /// the ECMAScript Temporal specification.
        ///
        /// # Examples
        ///
        /// ```
        /// use icu::calendar::options::DateFromFieldsOptions;
        /// use icu::calendar::options::MissingFieldsStrategy;
        /// use icu::calendar::types::DateFields;
        /// use icu::calendar::Date;
        /// use icu::calendar::Iso;
        ///
        /// // These options are missing a year.
        /// let mut fields = DateFields::default();
        /// fields.month_code = Some(b"M02");
        /// fields.day = Some(1);
        ///
        /// let options_default = DateFromFieldsOptions::default();
        /// assert!(Date::try_from_fields(fields, options_default, Iso).is_err());
        ///
        /// let mut options_reject = options_default;
        /// options_reject.missing_fields_strategy =
        ///     Some(MissingFieldsStrategy::Reject);
        /// assert!(Date::try_from_fields(fields, options_reject, Iso).is_err());
        ///
        /// let mut options_ecma = options_default;
        /// options_ecma.missing_fields_strategy = Some(MissingFieldsStrategy::Ecma);
        /// assert_eq!(
        ///     Date::try_from_fields(fields, options_ecma, Iso).unwrap(),
        ///     Date::try_new_iso(1972, 2, 1).unwrap()
        /// );
        /// ```
        pub missing_fields_strategy: Option<MissingFieldsStrategy>,
    }

    impl DateFromFieldsOptions {
        pub(crate) fn from_add_options(options: DateAddOptions) -> Self {
            Self {
                overflow: options.overflow,
                missing_fields_strategy: Default::default(),
            }
        }
    }

    /// Options for adding a duration to a date.
    ///
    /// <div class="stab unstable">
    /// 🚧 This code is considered unstable; it may change at any time, in breaking or non-breaking ways,
    /// including in SemVer minor releases. Do not use this type unless you are prepared for things to occasionally break.
    ///
    /// Graduation tracking issue: [issue #3964](https://github.com/unicode-org/icu4x/issues/3964).
    /// </div>
    ///
    /// ✨ *Enabled with the `unstable` Cargo feature.*
    #[derive(Copy, Clone, PartialEq, Debug, Default)]
    #[non_exhaustive]
    pub struct DateAddOptions {
        /// How to behave with out-of-bounds fields during arithmetic.
        ///
        /// Defaults to [`Overflow::Constrain`].
        ///
        /// # Examples
        ///
        /// ```
        /// use icu::calendar::options::DateAddOptions;
        /// use icu::calendar::options::Overflow;
        /// use icu::calendar::types::DateDuration;
        /// use icu::calendar::Date;
        ///
        /// // There is a day 31 in October but not in November.
        /// let d1 = Date::try_new_iso(2025, 10, 31).unwrap();
        /// let duration = DateDuration::for_months(1);
        ///
        /// let options_default = DateAddOptions::default();
        /// assert_eq!(
        ///     d1.try_added_with_options(duration, options_default)
        ///         .unwrap(),
        ///     Date::try_new_iso(2025, 11, 30).unwrap()
        /// );
        ///
        /// let mut options_reject = options_default;
        /// options_reject.overflow = Some(Overflow::Reject);
        /// assert!(d1.try_added_with_options(duration, options_reject).is_err());
        ///
        /// let mut options_constrain = options_default;
        /// options_constrain.overflow = Some(Overflow::Constrain);
        /// assert_eq!(
        ///     d1.try_added_with_options(duration, options_constrain)
        ///         .unwrap(),
        ///     Date::try_new_iso(2025, 11, 30).unwrap()
        /// );
        /// ```
        pub overflow: Option<Overflow>,
    }

    /// Options for taking the difference between two dates.
    ///
    /// <div class="stab unstable">
    /// 🚧 This code is considered unstable; it may change at any time, in breaking or non-breaking ways,
    /// including in SemVer minor releases. Do not use this type unless you are prepared for things to occasionally break.
    ///
    /// Graduation tracking issue: [issue #3964](https://github.com/unicode-org/icu4x/issues/3964).
    /// </div>
    ///
    /// ✨ *Enabled with the `unstable` Cargo feature.*
    #[derive(Copy, Clone, PartialEq, Debug, Default)]
    #[non_exhaustive]
    pub struct DateDifferenceOptions {
        /// Which date field to allow as the largest in a duration when taking the difference.
        ///
        /// When choosing [`Months`] or [`Years`], the resulting [`DateDuration`] might not be
        /// associative or commutative in subsequent arithmetic operations, and it might require
        /// [`Overflow::Constrain`] in addition.
        ///
        /// # Examples
        ///
        /// ```
        /// use icu::calendar::options::DateDifferenceOptions;
        /// use icu::calendar::types::DateDuration;
        /// use icu::calendar::types::DateDurationUnit;
        /// use icu::calendar::Date;
        ///
        /// let d1 = Date::try_new_iso(2025, 3, 31).unwrap();
        /// let d2 = Date::try_new_iso(2026, 5, 15).unwrap();
        ///
        /// let options_default = DateDifferenceOptions::default();
        /// assert_eq!(
        ///     d1.try_until_with_options(&d2, options_default).unwrap(),
        ///     DateDuration::for_days(410)
        /// );
        ///
        /// let mut options_days = options_default;
        /// options_days.largest_unit = Some(DateDurationUnit::Days);
        /// assert_eq!(
        ///     d1.try_until_with_options(&d2, options_default).unwrap(),
        ///     DateDuration::for_days(410)
        /// );
        ///
        /// let mut options_weeks = options_default;
        /// options_weeks.largest_unit = Some(DateDurationUnit::Weeks);
        /// assert_eq!(
        ///     d1.try_until_with_options(&d2, options_weeks).unwrap(),
        ///     DateDuration {
        ///         weeks: 58,
        ///         days: 4,
        ///         ..Default::default()
        ///     }
        /// );
        ///
        /// let mut options_months = options_default;
        /// options_months.largest_unit = Some(DateDurationUnit::Months);
        /// assert_eq!(
        ///     d1.try_until_with_options(&d2, options_months).unwrap(),
        ///     DateDuration {
        ///         months: 13,
        ///         days: 15,
        ///         ..Default::default()
        ///     }
        /// );
        ///
        /// let mut options_years = options_default;
        /// options_years.largest_unit = Some(DateDurationUnit::Years);
        /// assert_eq!(
        ///     d1.try_until_with_options(&d2, options_years).unwrap(),
        ///     DateDuration {
        ///         years: 1,
        ///         months: 1,
        ///         days: 15,
        ///         ..Default::default()
        ///     }
        /// );
        /// ```
        ///
        /// [`Months`]: crate::types::DateDurationUnit::Months
        /// [`Years`]: crate::types::DateDurationUnit::Years
        /// [`DateDuration`]: crate::types::DateDuration
        pub largest_unit: Option<crate::duration::DateDurationUnit>,
    }

    /// Whether to constrain or reject out-of-bounds values when constructing a Date.
    ///
    /// The behavior conforms to the ECMAScript Temporal specification.
    ///
    /// <div class="stab unstable">
    /// 🚧 This code is considered unstable; it may change at any time, in breaking or non-breaking ways,
    /// including in SemVer minor releases. Do not use this type unless you are prepared for things to occasionally break.
    ///
    /// Graduation tracking issue: [issue #7161](https://github.com/unicode-org/icu4x/issues/7161).
    /// </div>
    ///
    /// ✨ *Enabled with the `unstable` Cargo feature.*
    #[derive(Copy, Clone, Debug, PartialEq, Default)]
    #[non_exhaustive]
    pub enum Overflow {
        /// Constrain out-of-bounds fields to the nearest in-bounds value.
        ///
        /// Only the out-of-bounds field is constrained. If the other fields are not themselves
        /// out of bounds, they are not changed.
        ///
        /// This is the [default option](
        /// https://tc39.es/proposal-temporal/#sec-temporal-gettemporaloverflowoption),
        /// following the ECMAScript Temporal specification. See also the [docs on MDN](
        /// https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Temporal/PlainDate#invalid_date_clamping).
        ///
        /// # Examples
        ///
        /// ```
        /// use icu::calendar::cal::Hebrew;
        /// use icu::calendar::options::DateFromFieldsOptions;
        /// use icu::calendar::options::Overflow;
        /// use icu::calendar::types::DateFields;
        /// use icu::calendar::Date;
        /// use icu::calendar::DateError;
        ///
        /// let mut options = DateFromFieldsOptions::default();
        /// options.overflow = Some(Overflow::Constrain);
        ///
        /// // 5784, a leap year, contains M05L, but the day is too big.
        /// let mut fields = DateFields::default();
        /// fields.extended_year = Some(5784);
        /// fields.month_code = Some(b"M05L");
        /// fields.day = Some(50);
        ///
        /// let date = Date::try_from_fields(fields, options, Hebrew).unwrap();
        ///
        /// // Constrained to the 30th day of M05L of year 5784
        /// assert_eq!(date.year().extended_year(), 5784);
        /// assert_eq!(date.month().standard_code.0, "M05L");
        /// assert_eq!(date.day_of_month().0, 30);
        ///
        /// // 5785, a common year, does not contain M05L.
        /// fields.extended_year = Some(5785);
        ///
        /// let date = Date::try_from_fields(fields, options, Hebrew).unwrap();
        ///
        /// // Constrained to the 29th day of M06 of year 5785
        /// assert_eq!(date.year().extended_year(), 5785);
        /// assert_eq!(date.month().standard_code.0, "M06");
        /// assert_eq!(date.day_of_month().0, 29);
        /// ```
        Constrain,
        /// Return an error if any fields are out of bounds.
        ///
        /// # Examples
        ///
        /// ```
        /// use icu::calendar::cal::Hebrew;
        /// use icu::calendar::error::DateFromFieldsError;
        /// use icu::calendar::options::DateFromFieldsOptions;
        /// use icu::calendar::options::Overflow;
        /// use icu::calendar::types::DateFields;
        /// use icu::calendar::Date;
        /// use tinystr::tinystr;
        ///
        /// let mut options = DateFromFieldsOptions::default();
        /// options.overflow = Some(Overflow::Reject);
        ///
        /// // 5784, a leap year, contains M05L, but the day is too big.
        /// let mut fields = DateFields::default();
        /// fields.extended_year = Some(5784);
        /// fields.month_code = Some(b"M05L");
        /// fields.day = Some(50);
        ///
        /// let err = Date::try_from_fields(fields, options, Hebrew)
        ///     .expect_err("Day is out of bounds");
        /// assert!(matches!(err, DateFromFieldsError::Range { .. }));
        ///
        /// // Set the day to one that exists
        /// fields.day = Some(1);
        /// Date::try_from_fields(fields, options, Hebrew)
        ///     .expect("A valid Hebrew date");
        ///
        /// // 5785, a common year, does not contain M05L.
        /// fields.extended_year = Some(5785);
        ///
        /// let err = Date::try_from_fields(fields, options, Hebrew)
        ///     .expect_err("Month is out of bounds");
        /// assert!(matches!(err, DateFromFieldsError::MonthCodeNotInYear));
        /// ```
        #[default]
        Reject,
    }

    /// How to infer missing fields when the fields that are present do not fully constitute a Date.
    ///
    /// In order for the fields to fully constitute a Date, they must identify a year, a month,
    /// and a day. The fields `era`, `era_year`, and `extended_year` identify the year:
    ///
    /// | Era? | Era Year? | Extended Year? | Outcome |
    /// |------|-----------|----------------|---------|
    /// | -    | -         | -              | Error |
    /// | Some | -         | -              | Error |
    /// | -    | Some      | -              | Error |
    /// | -    | -         | Some           | OK |
    /// | Some | Some      | -              | OK |
    /// | Some | -         | Some           | Error (era requires era year) |
    /// | -    | Some      | Some           | Error (era year requires era) |
    /// | Some | Some      | Some           | OK (but error if inconsistent) |
    ///
    /// The fields `month_code` and `ordinal_month` identify the month:
    ///
    /// | Month Code? | Ordinal Month? | Outcome |
    /// |-------------|----------------|---------|
    /// | -           | -              | Error |
    /// | Some        | -              | OK |
    /// | -           | Some           | OK |
    /// | Some        | Some           | OK (but error if inconsistent) |
    ///
    /// The field `day` identifies the day.
    ///
    /// If the fields identify a year, a month, and a day, then there are no missing fields, so
    /// the strategy chosen here has no effect (fields that are out-of-bounds or inconsistent
    /// are handled by other errors).
    ///
    /// <div class="stab unstable">
    /// 🚧 This code is considered unstable; it may change at any time, in breaking or non-breaking ways,
    /// including in SemVer minor releases. Do not use this type unless you are prepared for things to occasionally break.
    ///
    /// Graduation tracking issue: [issue #7161](https://github.com/unicode-org/icu4x/issues/7161).
    /// </div>
    ///
    /// ✨ *Enabled with the `unstable` Cargo feature.*
    #[derive(Copy, Clone, Debug, PartialEq, Default)]
    #[non_exhaustive]
    pub enum MissingFieldsStrategy {
        /// If the fields that are present do not fully constitute a Date,
        /// return [`DateFromFieldsError::NotEnoughFields`].
        ///
        /// [`DateFromFieldsError::NotEnoughFields`]: crate::error::DateFromFieldsError::NotEnoughFields
        #[default]
        Reject,
        /// If the fields that are present do not fully constitute a Date,
        /// follow the ECMAScript specification when possible.
        ///
        /// ⚠️ This option causes a year or day to be implicitly added to the Date!
        ///
        /// This strategy makes the following changes:
        ///
        /// 1. If the fields identify a year and a month, but not a day, then set `day` to 1.
        /// 2. If `month_code` and `day` are set but nothing else, then set the year to a
        ///    _reference year_: some year in the calendar that contains the specified month
        ///    and day, according to the ECMAScript specification.
        ///
        /// Note that the reference year is _not_ added if an ordinal month is present, since
        /// the identity of an ordinal month changes from year to year.
        Ecma,
    }
}
#[cfg(test)]
mod tests {
    use crate::{error::DateFromFieldsError, types::DateFields, Date, Gregorian};
    use itertools::Itertools;
    use std::collections::{BTreeMap, BTreeSet};

    use super::*;

    #[test]
    #[allow(clippy::field_reassign_with_default)] // want out-of-crate code style
    fn test_missing_fields_strategy() {
        // The sets of fields that identify a year, according to the table in the docs
        let valid_year_field_sets = [
            &["era", "era_year"][..],
            &["extended_year"][..],
            &["era", "era_year", "extended_year"][..],
        ]
        .into_iter()
        .map(|field_names| field_names.iter().copied().collect())
        .collect::<Vec<BTreeSet<&str>>>();

        // The sets of fields that identify a month, according to the table in the docs
        let valid_month_field_sets = [
            &["month_code"][..],
            &["ordinal_month"][..],
            &["month_code", "ordinal_month"][..],
        ]
        .into_iter()
        .map(|field_names| field_names.iter().copied().collect())
        .collect::<Vec<BTreeSet<&str>>>();

        // The sets of fields that identify a day, according to the table in the docs
        let valid_day_field_sets = [&["day"][..]]
            .into_iter()
            .map(|field_names| field_names.iter().copied().collect())
            .collect::<Vec<BTreeSet<&str>>>();

        // All possible valid sets of fields
        let all_valid_field_sets = valid_year_field_sets
            .iter()
            .cartesian_product(valid_month_field_sets.iter())
            .cartesian_product(valid_day_field_sets.iter())
            .map(|((year_fields, month_fields), day_fields)| {
                year_fields
                    .iter()
                    .chain(month_fields.iter())
                    .chain(day_fields.iter())
                    .copied()
                    .collect::<BTreeSet<&str>>()
            })
            .collect::<BTreeSet<BTreeSet<&str>>>();

        // Field sets with year and month but without day that ECMA accepts
        let field_sets_without_day = valid_year_field_sets
            .iter()
            .cartesian_product(valid_month_field_sets.iter())
            .map(|(year_fields, month_fields)| {
                year_fields
                    .iter()
                    .chain(month_fields.iter())
                    .copied()
                    .collect::<BTreeSet<&str>>()
            })
            .collect::<BTreeSet<BTreeSet<&str>>>();

        // Field sets with month and day but without year that ECMA accepts
        let field_sets_without_year = [&["month_code", "day"][..]]
            .into_iter()
            .map(|field_names| field_names.iter().copied().collect())
            .collect::<Vec<BTreeSet<&str>>>();

        // A map from field names to a function that sets that field
        let mut field_fns = BTreeMap::<&str, &dyn Fn(&mut DateFields)>::new();
        field_fns.insert("era", &|fields| fields.era = Some(b"ad"));
        field_fns.insert("era_year", &|fields| fields.era_year = Some(2000));
        field_fns.insert("extended_year", &|fields| fields.extended_year = Some(2000));
        field_fns.insert("month_code", &|fields| fields.month_code = Some(b"M04"));
        field_fns.insert("ordinal_month", &|fields| fields.ordinal_month = Some(4));
        field_fns.insert("day", &|fields| fields.day = Some(20));

        for field_set in field_fns.keys().copied().powerset() {
            let field_set = field_set.into_iter().collect::<BTreeSet<&str>>();

            // Check whether this case should succeed: whether it identifies a year,
            // a month, and a day.
            let should_succeed_rejecting = all_valid_field_sets.contains(&field_set);

            // Check whether it should succeed in ECMA mode.
            let should_succeed_ecma = should_succeed_rejecting
                || field_sets_without_day.contains(&field_set)
                || field_sets_without_year.contains(&field_set);

            // Populate the fields in the field set
            let mut fields = Default::default();
            for field_name in field_set {
                field_fns.get(field_name).unwrap()(&mut fields);
            }

            // Check whether we were able to successfully construct the date
            let mut options = DateFromFieldsOptions::default();
            options.missing_fields_strategy = Some(MissingFieldsStrategy::Reject);
            match Date::try_from_fields(fields, options, Gregorian) {
                Ok(_) => assert!(
                    should_succeed_rejecting,
                    "Succeeded, but should have rejected: {fields:?}"
                ),
                Err(DateFromFieldsError::NotEnoughFields) => assert!(
                    !should_succeed_rejecting,
                    "Rejected, but should have succeeded: {fields:?}"
                ),
                Err(e) => panic!("Unexpected error: {e} for {fields:?}"),
            }

            // Check ECMA mode
            let mut options = DateFromFieldsOptions::default();
            options.missing_fields_strategy = Some(MissingFieldsStrategy::Ecma);
            match Date::try_from_fields(fields, options, Gregorian) {
                Ok(_) => assert!(
                    should_succeed_ecma,
                    "Succeeded, but should have rejected (ECMA): {fields:?}"
                ),
                Err(DateFromFieldsError::NotEnoughFields) => assert!(
                    !should_succeed_ecma,
                    "Rejected, but should have succeeded (ECMA): {fields:?}"
                ),
                Err(e) => panic!("Unexpected error: {e} for {fields:?}"),
            }
        }
    }

    #[test]
    fn test_constrain_large_months() {
        let fields = DateFields {
            extended_year: Some(2004),
            ordinal_month: Some(15),
            day: Some(1),
            ..Default::default()
        };
        let options = DateFromFieldsOptions {
            overflow: Some(Overflow::Constrain),
            ..Default::default()
        };

        let _ = Date::try_from_fields(fields, options, crate::cal::Persian).unwrap();
    }
}
