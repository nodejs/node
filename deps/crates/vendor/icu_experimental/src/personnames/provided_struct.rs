// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

use crate::personnames::api::{
    NameField, NameFieldKind, PersonName, PersonNamesFormatterError, PreferredOrder,
};
use alloc::string::String;
use alloc::vec::Vec;
use icu_locale_core::Locale;
use litemap::LiteMap;

/// [`DefaultPersonName`], default implementation provided for [`PersonNameFormatter`](super::PersonNamesFormatter).
#[derive(Debug)]
pub struct DefaultPersonName {
    person_data: LiteMap<NameField, String>,
    locale: Option<Locale>,
    preferred_order: Option<PreferredOrder>,
}

impl PersonName for DefaultPersonName {
    fn name_locale(&self) -> Option<&Locale> {
        self.locale.as_ref()
    }

    fn preferred_order(&self) -> Option<&PreferredOrder> {
        self.preferred_order.as_ref()
    }

    fn get(&self, field: NameField) -> &str {
        self.person_data
            .get(&field)
            .map(String::as_ref)
            .unwrap_or("")
    }

    fn available_name_fields(&self) -> Vec<NameField> {
        self.person_data.keys().copied().collect()
    }

    fn has_name_field_kind(&self, lookup_name_field: NameFieldKind) -> bool {
        self.available_name_fields()
            .into_iter()
            .any(|field| field.kind == lookup_name_field)
    }

    fn has_name_field(&self, lookup_name_field: NameField) -> bool {
        self.available_name_fields()
            .into_iter()
            .any(|field| field == lookup_name_field)
    }
}

/// Default person name functions.
impl DefaultPersonName {
    ///
    /// Returns a new person name structure.
    pub fn new(
        person_data: LiteMap<NameField, String>,
        locale: Option<Locale>,
        preferred_order: Option<PreferredOrder>,
    ) -> Result<DefaultPersonName, PersonNamesFormatterError> {
        let result = DefaultPersonName {
            person_data,
            locale,
            preferred_order,
        };
        if !crate::personnames::formatter::validate_person_name(&result.available_name_fields()) {
            return Err(PersonNamesFormatterError::InvalidPersonName);
        }
        Ok(result)
    }
}
