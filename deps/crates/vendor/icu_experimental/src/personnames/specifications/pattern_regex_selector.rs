// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

use core::str::FromStr;

use alloc::borrow::Cow;
use alloc::string::String;
use alloc::vec::Vec;
use icu_pattern::{MultiNamedPlaceholderPattern, PatternItem};

use crate::personnames::api::{
    FieldModifier, FieldModifierSet, NameField, NameFieldKind, PersonName,
    PersonNamesFormatterError,
};
use crate::personnames::specifications;

/// Contains meta information about the person name pattern.
#[derive(PartialEq, Debug)]
pub struct PersonNamePattern<'lt> {
    // TODO: Make this use a regular icu_pattern::Pattern as its data store
    pub name_fields: Vec<(NameField, Cow<'lt, str>)>,
}

impl PersonNamePattern<'_> {}

impl PersonNamePattern<'_> {
    #[cfg(test)]
    fn get_field(&self, lookup_name_field: NameField) -> Option<Cow<'_, str>> {
        self.name_fields
            .iter()
            .find(|&&(k, _)| k == lookup_name_field)
            .map(|(_, v)| v.clone())
    }

    fn pattern_requires_given_name(&self) -> bool {
        self.name_fields.iter().any(|(field, _)| {
            // the pattern either doesn't include the given name or only shows an initial for the given name
            field.kind == NameFieldKind::Given && !field.modifier.has_field(FieldModifier::Initial)
        })
    }

    fn contains_key(&self, lookup_name_field: NameField) -> bool {
        self.name_fields
            .iter()
            .any(|&(k, _)| k == lookup_name_field)
    }

    /// Returns the how many fields can be matched using the current build pattern.
    pub fn match_info(&self, available_name_fields: &[NameField]) -> (usize, usize) {
        let available_fields = available_name_fields.iter().fold(0, |count, &name_field| {
            if self.contains_key(name_field) {
                count + 1
            } else {
                count
            }
        });
        let missing_fields = self.name_fields.iter().fold(0, |count, &(name_field, _)| {
            if available_name_fields.contains(&name_field) {
                count
            } else {
                count + 1
            }
        });
        (available_fields, missing_fields)
    }

    fn fetch_pattern_data_replacement<'lt>(
        &'lt self,
        person_name: &'lt dyn PersonName,
        requested_name_field: NameField,
        initial_pattern: &'lt str,
        initial_sequence_pattern: &'lt str,
    ) -> Vec<String> {
        let available_name_field = person_name.available_name_fields();
        let effective_name_field = specifications::handle_field_modifier_core_prefix(
            &available_name_field,
            requested_name_field,
        );

        effective_name_field
            .iter()
            .flat_map(|&field| {
                specifications::derive_missing_surname(
                    &available_name_field,
                    field,
                    self.pattern_requires_given_name(),
                )
            })
            .map(|field| {
                specifications::derive_missing_initials(
                    person_name,
                    field,
                    initial_pattern,
                    initial_sequence_pattern,
                )
            })
            .collect()
    }

    pub fn format_person_name(
        &self,
        person_name: &dyn PersonName,
        initial_pattern: &str,
        initial_sequence_pattern: &str,
    ) -> String {
        self.name_fields
            .iter()
            .flat_map(|&(k, ref v)| {
                let p_name = self
                    .fetch_pattern_data_replacement(
                        person_name,
                        k,
                        initial_pattern,
                        initial_sequence_pattern,
                    )
                    .join(" ");
                [p_name, String::from(v.as_ref())]
            })
            .collect()
    }
}

impl FromStr for NameFieldKind {
    type Err = PersonNamesFormatterError;

    fn from_str(value: &str) -> Result<Self, Self::Err> {
        match value {
            "title" => Ok(NameFieldKind::Title),
            "given" => Ok(NameFieldKind::Given),
            "given2" => Ok(NameFieldKind::Given2),
            "surname" => Ok(NameFieldKind::Surname),
            "surname2" => Ok(NameFieldKind::Surname2),
            "generation" => Ok(NameFieldKind::Generation),
            "credentials" => Ok(NameFieldKind::Credentials),

            _ => {
                icu_provider::log::warn!("Invalid NameFieldKind value matched [{value}]");
                Err(PersonNamesFormatterError::InvalidCldrData)
            }
        }
    }
}

impl FromStr for FieldModifier {
    type Err = PersonNamesFormatterError;

    fn from_str(value: &str) -> Result<Self, Self::Err> {
        match value {
            "informal" => Ok(FieldModifier::Informal),
            "prefix" => Ok(FieldModifier::Prefix),
            "core" => Ok(FieldModifier::Core),
            "allCaps" => Ok(FieldModifier::AllCaps),
            "initialCap" => Ok(FieldModifier::InitialCap),
            "initial" => Ok(FieldModifier::Initial),
            "monogram" => Ok(FieldModifier::Monogram),
            _ => {
                icu_provider::log::warn!("Invalid FieldModifier value matched [{value}]");
                Err(PersonNamesFormatterError::InvalidCldrData)
            }
        }
    }
}

impl FromStr for NameField {
    type Err = PersonNamesFormatterError;

    fn from_str(value: &str) -> Result<Self, Self::Err> {
        let mut value_iter = value.split('-');
        let name_field_kind = value_iter
            .next()
            .map(NameFieldKind::from_str)
            .unwrap_or_else(|| {
                icu_provider::log::warn!("unable to match");
                Err(PersonNamesFormatterError::InvalidCldrData)
            })?;

        let field_modifier_1 = value_iter
            .next()
            .map(FieldModifier::from_str)
            .unwrap_or(Ok(FieldModifier::None))?
            .bit_value();
        let field_modifier_2 = value_iter
            .next()
            .map(FieldModifier::from_str)
            .unwrap_or(Ok(FieldModifier::None))?
            .bit_value();
        let field_modifier_3 = value_iter
            .next()
            .map(FieldModifier::from_str)
            .unwrap_or(Ok(FieldModifier::None))?
            .bit_value();
        let field_modifier_4 = value_iter
            .next()
            .map(FieldModifier::from_str)
            .unwrap_or(Ok(FieldModifier::None))?
            .bit_value();

        Ok(Self {
            kind: name_field_kind,
            modifier: FieldModifierSet {
                value: field_modifier_1 | field_modifier_2 | field_modifier_3 | field_modifier_4,
            },
        })
    }
}

pub fn to_person_name_pattern(
    value: &str,
) -> Result<PersonNamePattern<'_>, PersonNamesFormatterError> {
    let mut name_fields_map: Vec<(NameField, Cow<str>)> = Vec::new();

    let parsed_pattern = MultiNamedPlaceholderPattern::try_from_str(value, Default::default())?;

    let mut current_name_field = None;
    let mut current_literal = None;
    for item in parsed_pattern.iter() {
        match item {
            PatternItem::Literal(s) => {
                debug_assert!(current_literal.is_none());
                current_literal = Some(s);
            }
            PatternItem::Placeholder(key) => {
                if let Some(name_field) = current_name_field.take() {
                    // TODO: This takes ownership because we can't borrow from the parsed pattern.
                    // This should be fixed by using a Pattern in the data model.
                    let trailing = Cow::Owned(String::from(current_literal.take().unwrap_or("")));
                    name_fields_map.push((name_field, trailing));
                }
                current_name_field = Some(NameField::from_str(key.0)?);
            }
        }
    }
    if let Some(name_field) = current_name_field.take() {
        // TODO: This takes ownership because we can't borrow from the parsed pattern.
        // This should be fixed by using a Pattern in the data model.
        let trailing = Cow::Owned(String::from(current_literal.take().unwrap_or("")));
        name_fields_map.push((name_field, trailing));
    }

    Ok(PersonNamePattern {
        name_fields: name_fields_map,
    })
}

#[cfg(test)]
mod tests {
    use litemap::LiteMap;

    use crate::personnames::api::{
        FieldCapsStyle, FieldFormality, FieldLength, FieldModifierSet, FieldPart, NameField,
        NameFieldKind, PersonNamesFormatterError,
    };
    use crate::personnames::provided_struct::DefaultPersonName;

    use super::*;

    #[test]
    fn should_match_no_field_modifier() -> Result<(), PersonNamesFormatterError> {
        let pattern = "{title} {given} {given2} {surname} {generation}, {credentials}";
        let person_name_pattern = to_person_name_pattern(pattern)?;
        let nb_captures = person_name_pattern.name_fields.len();
        let trailing_end = person_name_pattern
            .get_field(NameField {
                kind: NameFieldKind::Credentials,
                modifier: Default::default(),
            })
            .unwrap();
        assert_eq!(nb_captures, 6, "{nb_captures}");
        assert_eq!(
            trailing_end, "",
            "should be a empty at the end space matched"
        );
        let trailing_comma = person_name_pattern
            .get_field(NameField {
                kind: NameFieldKind::Generation,
                modifier: Default::default(),
            })
            .unwrap();
        assert_eq!(trailing_comma, ", ", "should be a comma after generation");
        let trailing_space = person_name_pattern
            .get_field(NameField {
                kind: NameFieldKind::Title,
                modifier: Default::default(),
            })
            .unwrap();
        assert_eq!(trailing_space, " ", "should be a space after title");
        Ok(())
    }

    #[test]
    fn should_match_field_modifier() -> Result<(), PersonNamesFormatterError> {
        let pattern = "{surname-monogram-allCaps}{given-monogram-allCaps}{given2-monogram-allCaps}";
        let person_name_pattern = to_person_name_pattern(pattern)?;
        let captures = person_name_pattern.name_fields.len();

        assert_eq!(captures, 3, "{captures}");
        assert!(
            person_name_pattern.contains_key(NameField {
                kind: NameFieldKind::Surname,
                modifier: FieldModifierSet::new(
                    FieldCapsStyle::AllCaps,
                    FieldPart::Auto,
                    FieldLength::Monogram,
                    FieldFormality::Auto,
                ),
            }),
            "didn't properly match surname-monogram-allCaps"
        );
        assert!(
            person_name_pattern.contains_key(NameField {
                kind: NameFieldKind::Given,
                modifier: FieldModifierSet::new(
                    FieldCapsStyle::AllCaps,
                    FieldPart::Auto,
                    FieldLength::Monogram,
                    FieldFormality::Auto,
                ),
            }),
            "didn't properly match given-monogram-allCaps"
        );
        assert!(
            person_name_pattern.contains_key(NameField {
                kind: NameFieldKind::Given2,
                modifier: FieldModifierSet::new(
                    FieldCapsStyle::AllCaps,
                    FieldPart::Auto,
                    FieldLength::Monogram,
                    FieldFormality::Auto,
                ),
            }),
            "didn't properly match given2-monogram-allCaps"
        );

        let trailing = person_name_pattern
            .get_field(NameField {
                kind: NameFieldKind::Given2,
                modifier: FieldModifierSet::new(
                    FieldCapsStyle::AllCaps,
                    FieldPart::Auto,
                    FieldLength::Monogram,
                    FieldFormality::Auto,
                ),
            })
            .unwrap();
        assert_eq!(trailing, "", "should be a trailing space matched");
        Ok(())
    }

    #[test]
    fn should_format() -> Result<(), PersonNamesFormatterError> {
        let pattern = "{given} {surname} is {given-informal} {surname-informal}";
        let person_name_pattern = to_person_name_pattern(pattern)?;
        let person_name = simple_default_person_name_object()?;
        assert_eq!(
            person_name_pattern.format_person_name(
                &person_name as &dyn PersonName,
                "{0}.",
                "{0} {1}",
            ),
            "Henry Jekyll is Edward Hide"
        );
        Ok(())
    }

    #[test]
    fn should_format_reversed() -> Result<(), PersonNamesFormatterError> {
        let pattern = "{given-informal} {surname-informal} is {given} {surname}";
        let person_name_pattern = to_person_name_pattern(pattern)?;
        let person_name = simple_default_person_name_object()?;
        assert_eq!(
            person_name_pattern.format_person_name(
                &person_name as &dyn PersonName,
                "{0}.",
                "{0} {1}",
            ),
            "Edward Hide is Henry Jekyll"
        );
        Ok(())
    }

    #[test]
    fn should_format_missing_initials() -> Result<(), PersonNamesFormatterError> {
        let pattern = "{given-initial-informal} {surname-informal} is {given-initial} {surname}";
        let person_name_pattern = to_person_name_pattern(pattern)?;
        let person_name = simple_default_person_name_object()?;
        assert_eq!(
            person_name_pattern.format_person_name(
                &person_name as &dyn PersonName,
                "{0}.",
                "{0} {1}",
            ),
            "E. Hide is H. Jekyll"
        );
        Ok(())
    }

    fn simple_default_person_name_object() -> Result<DefaultPersonName, PersonNamesFormatterError> {
        let mut person_data: LiteMap<NameField, String> = LiteMap::new();
        person_data.insert(
            NameField {
                kind: NameFieldKind::Given,
                modifier: FieldModifierSet::default(),
            },
            String::from("Henry"),
        );
        person_data.insert(
            NameField {
                kind: NameFieldKind::Given,
                modifier: FieldModifierSet::formality(FieldFormality::Informal),
            },
            String::from("Edward"),
        );
        person_data.insert(
            NameField {
                kind: NameFieldKind::Surname,
                modifier: FieldModifierSet::default(),
            },
            String::from("Jekyll"),
        );
        person_data.insert(
            NameField {
                kind: NameFieldKind::Surname,
                modifier: FieldModifierSet::new(
                    FieldCapsStyle::Auto,
                    FieldPart::Auto,
                    FieldLength::Auto,
                    FieldFormality::Informal,
                ),
            },
            String::from("Hide"),
        );
        // locale and preferred order are irrelevant here.
        let person_name = DefaultPersonName::new(person_data, None, None)?;
        Ok(person_name)
    }

    #[test]
    fn should_format_all_missing_initials() -> Result<(), PersonNamesFormatterError> {
        let pattern = "{given-initial-informal} {surname-initial-informal} is {given-initial} {surname-initial}";
        let person_name_pattern = to_person_name_pattern(pattern)?;
        let mut person_data: LiteMap<NameField, String> = LiteMap::new();
        person_data.insert(
            NameField {
                kind: NameFieldKind::Given,
                modifier: FieldModifierSet::default(),
            },
            String::from("Henry"),
        );
        person_data.insert(
            NameField {
                kind: NameFieldKind::Given,
                modifier: FieldModifierSet::formality(FieldFormality::Informal),
            },
            String::from("Edward"),
        );
        person_data.insert(
            NameField {
                kind: NameFieldKind::Surname,
                modifier: FieldModifierSet::default(),
            },
            String::from("Jekyll"),
        );
        person_data.insert(
            NameField {
                kind: NameFieldKind::Surname,
                modifier: FieldModifierSet::new(
                    FieldCapsStyle::Auto,
                    FieldPart::Auto,
                    FieldLength::Auto,
                    FieldFormality::Informal,
                ),
            },
            String::from("Hide"),
        );
        // locale and preferred order are irrelevant here.
        let person_name = DefaultPersonName::new(person_data, None, None)?;
        assert_eq!(
            person_name_pattern.format_person_name(
                &person_name as &dyn PersonName,
                "{0}.",
                "{0} {1}",
            ),
            "E. H. is H. J."
        );
        Ok(())
    }
}
