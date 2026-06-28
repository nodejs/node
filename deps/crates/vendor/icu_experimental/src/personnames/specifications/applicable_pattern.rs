// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

use alloc::string::String;

use crate::personnames::api::{NameField, PersonNamesFormatterError};
use crate::personnames::specifications::PersonNamePattern;

pub fn find_best_applicable_pattern<'lt>(
    applicable_pattern: &'lt [PersonNamePattern<'lt>],
    available_name_fields: &'lt [NameField],
) -> Result<&'lt PersonNamePattern<'lt>, PersonNamesFormatterError> {
    let (_, _, max_applicable_pattern) =
        applicable_pattern
            .iter()
            .fold((0, u8::MAX as usize, None), |current_max, element| {
                let (max_used_field_count, max_missing_field_count, _) = &current_max;
                let (used_field_count, missing_field_count) =
                    &element.match_info(available_name_fields);
                if used_field_count < max_used_field_count {
                    return current_max;
                }
                if used_field_count > max_used_field_count {
                    return (*used_field_count, *missing_field_count, Some(element));
                }
                // check if used_field_count == max_used_field_count
                if max_missing_field_count < missing_field_count {
                    return current_max;
                }
                (*used_field_count, *missing_field_count, Some(element))
            });
    max_applicable_pattern.map(Ok).unwrap_or_else(|| {
        Err(PersonNamesFormatterError::ParseError(String::from(
            "Invalid Person name pattern",
        )))
    })
}

#[cfg(test)]
mod tests {
    use crate::personnames::api::{
        FieldLength, FieldModifierSet, NameField, NameFieldKind, PersonNamesFormatterError,
    };
    use crate::personnames::specifications::pattern_regex_selector::PersonNamePattern;
    use crate::personnames::specifications::to_person_name_pattern;

    #[test]
    fn test_simple_match() -> Result<(), PersonNamesFormatterError> {
        let tested_patterns: &[PersonNamePattern; 2] = &[
            "{surname}, {title} {given} {given2}",
            "{surname}, {given-initial} {given2-initial}",
        ]
        .map(to_person_name_pattern)
        .map(|v| v.unwrap());

        let title = NameField {
            kind: NameFieldKind::Title,
            modifier: Default::default(),
        };
        let surname = NameField {
            kind: NameFieldKind::Surname,
            modifier: Default::default(),
        };
        let given2 = NameField {
            kind: NameFieldKind::Given2,
            modifier: Default::default(),
        };
        let given = NameField {
            kind: NameFieldKind::Given,
            modifier: Default::default(),
        };
        let name_fields = vec![title, surname, given, given2];
        let result = super::find_best_applicable_pattern(tested_patterns, &name_fields)?;
        let expect = &tested_patterns[0];
        assert_eq!(result, expect);

        Ok(())
    }

    #[test]
    fn test_complex_match() -> Result<(), PersonNamesFormatterError> {
        let tested_patterns: &[PersonNamePattern; 2] = &[
            "{surname}, {title} {given} {given2}",
            "{surname}, {given-initial} {given2-initial}",
        ]
        .map(to_person_name_pattern)
        .map(|v| v.unwrap());

        let surname = NameField {
            kind: NameFieldKind::Surname,
            modifier: Default::default(),
        };
        let given = NameField {
            kind: NameFieldKind::Given,
            modifier: Default::default(),
        };
        let given_initial = NameField {
            kind: NameFieldKind::Given,
            modifier: FieldModifierSet::length(FieldLength::Initial),
        };
        let given2 = NameField {
            kind: NameFieldKind::Given2,
            modifier: Default::default(),
        };
        let given2_initial = NameField {
            kind: NameFieldKind::Given2,
            modifier: FieldModifierSet::length(FieldLength::Initial),
        };
        let name_fields = vec![surname, given, given_initial, given2, given2_initial];
        let result = super::find_best_applicable_pattern(tested_patterns, &name_fields)?;
        let expect = &tested_patterns[1];
        assert_eq!(result, expect);

        Ok(())
    }
}
