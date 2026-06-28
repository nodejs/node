// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

use crate::personnames::api::{FieldModifier, FieldPart, NameField};
use alloc::vec;
use alloc::vec::Vec;

/// Transform the request field in a list of requested field based on the available fields.
/// a single field can be transformed into 2 fields, or just be removed.
/// <https://www.unicode.org/reports/tr35/tr35-personNames.html#handle-core-and-prefix>
pub fn handle_field_modifier_core_prefix(
    available_fields: &[NameField],
    requested_field: NameField,
) -> Vec<NameField> {
    let is_core = requested_field.modifier.has_field(FieldModifier::Core);
    let is_prefix = requested_field.modifier.has_field(FieldModifier::Prefix);
    let is_plain = !is_core && !is_prefix;
    let mut has_prefix_version_available = false;
    let mut has_core_version_available = false;
    let mut has_plain_version_available = false;
    for &field in available_fields {
        if field.kind == requested_field.kind {
            has_plain_version_available |=
                field.modifier == requested_field.modifier.with_part(FieldPart::Auto);
            has_prefix_version_available |=
                field.modifier == requested_field.modifier.with_part(FieldPart::Prefix);
            has_core_version_available |=
                field.modifier == requested_field.modifier.with_part(FieldPart::Core);
        }
    }
    let mut result = vec![];

    match (
        has_prefix_version_available,
        has_core_version_available,
        has_plain_version_available,
    ) {
        (true, true, true) | (false, false, false) => result.push(requested_field),
        (true, false, true) => {
            if is_core {
                result.push(NameField {
                    kind: requested_field.kind,
                    modifier: requested_field.modifier.with_part(FieldPart::Auto),
                })
            } else if is_plain {
                result.push(requested_field)
            }
        }
        (false, true, true) | (false, false, true) => {
            if is_core {
                result.push(NameField {
                    kind: requested_field.kind,
                    modifier: requested_field.modifier.with_part(FieldPart::Auto),
                })
            } else {
                result.push(requested_field)
            }
        }
        (true, true, false) => {
            if is_plain {
                result.push(NameField {
                    kind: requested_field.kind,
                    modifier: requested_field.modifier.with_part(FieldPart::Prefix),
                });
                result.push(NameField {
                    kind: requested_field.kind,
                    modifier: requested_field.modifier.with_part(FieldPart::Core),
                })
            } else {
                result.push(requested_field)
            }
        }
        (false, true, false) => {
            if is_plain {
                result.push(NameField {
                    kind: requested_field.kind,
                    modifier: requested_field.modifier.with_part(FieldPart::Core),
                })
            } else {
                result.push(requested_field)
            }
        }
        (true, false, false) => {
            if !&is_prefix {
                result.push(requested_field)
            }
        }
    }

    result
}

#[cfg(test)]
mod tests {
    use crate::personnames::api::{FieldModifierSet, FieldPart, NameField, NameFieldKind};

    // Each case is named after the combination if the field is present in spec table.
    // e.g. : cp = core + plain, pc = prefix + core, pp = prefix + plain, pcp = all fields

    #[test]
    fn test_pcp() {
        let asserted_parts = &[FieldPart::Prefix, FieldPart::Core, FieldPart::Auto];
        assert_part(asserted_parts, FieldPart::Prefix, &[FieldPart::Prefix]);
        assert_part(asserted_parts, FieldPart::Core, &[FieldPart::Core]);
        assert_part(asserted_parts, FieldPart::Auto, &[FieldPart::Auto]);
    }

    #[test]
    fn test_pp() {
        let asserted_parts = &[FieldPart::Prefix, FieldPart::Auto];
        assert_part(asserted_parts, FieldPart::Prefix, &[]);
        assert_part(asserted_parts, FieldPart::Core, &[FieldPart::Auto]);
        assert_part(asserted_parts, FieldPart::Auto, &[FieldPart::Auto]);
    }

    #[test]
    fn test_cp() {
        let asserted_parts = &[FieldPart::Core, FieldPart::Auto];
        assert_part(asserted_parts, FieldPart::Prefix, &[FieldPart::Prefix]);
        assert_part(asserted_parts, FieldPart::Core, &[FieldPart::Auto]);
        assert_part(asserted_parts, FieldPart::Auto, &[FieldPart::Auto]);
    }

    #[test]
    fn test_plain() {
        let asserted_parts = &[FieldPart::Auto];
        assert_part(asserted_parts, FieldPart::Prefix, &[FieldPart::Prefix]);
        assert_part(asserted_parts, FieldPart::Core, &[FieldPart::Auto]);
        assert_part(asserted_parts, FieldPart::Auto, &[FieldPart::Auto]);
    }

    #[test]
    fn test_pc() {
        let asserted_parts = &[FieldPart::Prefix, FieldPart::Core];
        assert_part(asserted_parts, FieldPart::Prefix, &[FieldPart::Prefix]);
        assert_part(asserted_parts, FieldPart::Core, &[FieldPart::Core]);
        assert_part(
            asserted_parts,
            FieldPart::Auto,
            &[FieldPart::Prefix, FieldPart::Core],
        );
    }

    #[test]
    fn test_c() {
        let asserted_parts = &[FieldPart::Core];
        assert_part(asserted_parts, FieldPart::Prefix, &[FieldPart::Prefix]);
        assert_part(asserted_parts, FieldPart::Core, &[FieldPart::Core]);
        assert_part(asserted_parts, FieldPart::Auto, &[FieldPart::Core]);
    }

    #[test]
    fn test_prefix() {
        let asserted_parts = &[FieldPart::Prefix];
        assert_part(asserted_parts, FieldPart::Prefix, &[]);
        assert_part(asserted_parts, FieldPart::Core, &[FieldPart::Core]);
        assert_part(asserted_parts, FieldPart::Auto, &[FieldPart::Auto]);
    }

    #[test]
    fn test_nothing() {
        let asserted_parts = &[];
        assert_part(asserted_parts, FieldPart::Prefix, &[FieldPart::Prefix]);
        assert_part(asserted_parts, FieldPart::Core, &[FieldPart::Core]);
        assert_part(asserted_parts, FieldPart::Auto, &[FieldPart::Auto]);
    }

    fn assert_part(
        available_parts: &[FieldPart],
        field_part: FieldPart,
        expected_parts: &[FieldPart],
    ) {
        let available_fields: Vec<NameField> = available_parts
            .iter()
            .map(|part: &FieldPart| NameField {
                kind: NameFieldKind::Given,
                modifier: FieldModifierSet::part(*part),
            })
            .collect::<Vec<NameField>>();
        let expected_fields: Vec<NameField> = expected_parts
            .iter()
            .map(|part| NameField {
                kind: NameFieldKind::Given,
                modifier: FieldModifierSet::part(*part),
            })
            .collect::<Vec<NameField>>();
        assert_eq!(
            super::handle_field_modifier_core_prefix(
                &available_fields,
                NameField {
                    kind: NameFieldKind::Given,
                    modifier: FieldModifierSet::part(field_part),
                }
            ),
            expected_fields
        )
    }
}
