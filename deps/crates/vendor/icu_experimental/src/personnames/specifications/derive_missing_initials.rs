// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

use alloc::string::String;
use icu_pattern::{DoublePlaceholderPattern, SinglePlaceholderPattern};
use writeable::Writeable;

use crate::personnames::api::{FieldLength, FieldModifier, NameField, PersonName};

///
/// Derive missing initials from the name
///
/// <https://www.unicode.org/reports/tr35/tr35-personNames.html#derive-initials>
pub fn derive_missing_initials(
    person_name: &dyn PersonName,
    requested_field: NameField,
    initial_pattern_str: &str,
    initial_sequence_pattern_str: &str,
) -> String {
    let initial_pattern =
        SinglePlaceholderPattern::try_from_str(initial_pattern_str, Default::default()).unwrap();
    let initial_sequence_pattern =
        DoublePlaceholderPattern::try_from_str(initial_sequence_pattern_str, Default::default())
            .unwrap();

    if person_name.has_name_field(requested_field) {
        return String::from(person_name.get(requested_field));
    }
    if requested_field.modifier.has_field(FieldModifier::Initial) {
        let initials = person_name
            .get(NameField {
                kind: requested_field.kind,
                modifier: requested_field.modifier.with_length(FieldLength::Auto),
            })
            .split(' ')
            .filter_map(|s| s.trim().chars().next());

        let mut interpolated_initials =
            initials.map(|initial| initial_pattern.interpolate((initial,)));

        let mut output = interpolated_initials
            .next()
            .map(|s| s.write_to_string().into_owned())
            .unwrap_or(String::new());

        // TODO: Interpolate these strings without so many allocations
        for s in interpolated_initials {
            output = initial_sequence_pattern
                .interpolate((output, s.write_to_string().into_owned()))
                .write_to_string()
                .into_owned();
        }

        return output;
    }
    // If it had the field, it would have been returned earlier.
    String::from("")
}

#[cfg(test)]
mod tests {
    use icu_locale_core::locale;
    use litemap::LiteMap;

    use crate::personnames::api::NameFieldKind::Given;
    use crate::personnames::api::{
        FieldLength, FieldModifierSet, NameField, PersonNamesFormatterError,
    };
    use crate::personnames::provided_struct::DefaultPersonName;

    #[test]
    fn test_single_initial() -> Result<(), PersonNamesFormatterError> {
        let mut person_data = LiteMap::new();
        person_data.insert(
            NameField {
                kind: Given,
                modifier: FieldModifierSet::default(),
            },
            String::from("Henry"),
        );
        let person_name = DefaultPersonName::new(person_data, Some(locale!("en")), None)?;
        let requested_field = NameField {
            kind: Given,
            modifier: FieldModifierSet::length(FieldLength::Initial),
        };
        let result =
            super::derive_missing_initials(&person_name, requested_field, "{0}.", "{0} {1}");
        assert_eq!(result, "H.");
        Ok(())
    }

    #[test]
    fn test_multi_initial() -> Result<(), PersonNamesFormatterError> {
        let mut person_data = LiteMap::new();
        person_data.insert(
            NameField {
                kind: Given,
                modifier: FieldModifierSet::default(),
            },
            String::from("Mary Jane"),
        );
        let person_name = DefaultPersonName::new(person_data, Some(locale!("en")), None)?;
        let requested_field = NameField {
            kind: Given,
            modifier: FieldModifierSet::length(FieldLength::Initial),
        };
        let result =
            super::derive_missing_initials(&person_name, requested_field, "{0}.", "{0} {1}");
        assert_eq!(result, "M. J.");
        Ok(())
    }

    #[test]
    fn test_multi_3_initial_should_still_only_be_2() -> Result<(), PersonNamesFormatterError> {
        let mut person_data = LiteMap::new();
        person_data.insert(
            NameField {
                kind: Given,
                modifier: FieldModifierSet::default(),
            },
            String::from("Mary Jane Anne"),
        );
        let person_name = DefaultPersonName::new(person_data, Some(locale!("en")), None)?;
        let requested_field = NameField {
            kind: Given,
            modifier: FieldModifierSet::length(FieldLength::Initial),
        };
        let result =
            super::derive_missing_initials(&person_name, requested_field, "{0}.", "{0} {1}");

        // TODO(#3077): broken, this should be equal
        assert_ne!(result, "M. J.");
        Ok(())
    }
}
