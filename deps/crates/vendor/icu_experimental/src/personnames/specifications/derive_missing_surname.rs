// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

use crate::personnames::api::{NameField, NameFieldKind};

/// Returns a remapped field for missing surname.
/// <https://www.unicode.org/reports/tr35/tr35-personNames.html#handle-missing-surname>
pub fn derive_missing_surname(
    available_name_field: &[NameField],
    requested_name_field: NameField,
    requires_given_name: bool,
) -> Option<NameField> {
    match requested_name_field.kind {
        NameFieldKind::Surname | NameFieldKind::Given => {
            let has_surname = available_name_field
                .iter()
                .any(|&field| field.kind == NameFieldKind::Surname);
            if !has_surname && !requires_given_name {
                if requested_name_field.kind == NameFieldKind::Surname {
                    return Some(NameField {
                        kind: NameFieldKind::Given,
                        modifier: requested_name_field.modifier,
                    });
                }
                return None;
            }
            Some(requested_name_field)
        }
        _ => Some(requested_name_field),
    }
}

#[cfg(test)]
mod tests {
    use NameFieldKind::{Given, Surname};

    use crate::personnames::api::{NameField, NameFieldKind};

    #[test]
    fn test_given_name_unused_no_surname_available_should_be_none_for_given() {
        let requested_name_field = NameField {
            kind: Given,
            modifier: Default::default(),
        };
        let available_name_field = &[NameField {
            kind: Given,
            modifier: Default::default(),
        }];
        let result =
            super::derive_missing_surname(available_name_field, requested_name_field, false);

        assert_eq!(result, None)
    }

    #[test]
    fn test_given_name_unused_no_surname_available_should_be_some_for_surname_and_swapped() {
        let requested_name_field = NameField {
            kind: Surname,
            modifier: Default::default(),
        };
        let available_name_field = &[NameField {
            kind: Given,
            modifier: Default::default(),
        }];
        let result =
            super::derive_missing_surname(available_name_field, requested_name_field, false);

        assert_eq!(
            result,
            Some(NameField {
                kind: Given,
                modifier: Default::default(),
            })
        )
    }

    #[test]
    fn test_given_name_used_no_surname_available_should_be_some() {
        let requested_name_field = NameField {
            kind: Given,
            modifier: Default::default(),
        };
        let available_name_field = &[NameField {
            kind: Given,
            modifier: Default::default(),
        }];
        let result =
            super::derive_missing_surname(available_name_field, requested_name_field, true);

        assert_eq!(
            result,
            Some(NameField {
                kind: Given,
                modifier: Default::default(),
            })
        )
    }

    #[test]
    fn test_given_name_used_surname_available_should_be_some() {
        let requested_name_field = NameField {
            kind: Given,
            modifier: Default::default(),
        };
        let available_name_field = &[
            NameField {
                kind: Given,
                modifier: Default::default(),
            },
            NameField {
                kind: Surname,
                modifier: Default::default(),
            },
        ];
        let result =
            super::derive_missing_surname(available_name_field, requested_name_field, true);

        assert_eq!(
            result,
            Some(NameField {
                kind: Given,
                modifier: Default::default(),
            })
        )
    }

    #[test]
    fn test_given_name_unused_surname_available_should_be_some() {
        let requested_name_field = NameField {
            kind: Given,
            modifier: Default::default(),
        };
        let available_name_field = &[
            NameField {
                kind: Given,
                modifier: Default::default(),
            },
            NameField {
                kind: Surname,
                modifier: Default::default(),
            },
        ];
        let result =
            super::derive_missing_surname(available_name_field, requested_name_field, false);

        assert_eq!(
            result,
            Some(NameField {
                kind: Given,
                modifier: Default::default(),
            })
        )
    }
}
