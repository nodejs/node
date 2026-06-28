// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

use icu_locale_core::Locale;

const DEFAULT_FOREIGN_SPACE_REPLACEMENT: &str = " ";

/// <https://www.unicode.org/reports/tr35/tr35-personNames.html#setting-the-spacereplacement>
pub fn space_replacement<'lt>(
    formatting_locale: &Locale,
    person_name_locale: &Locale,
    foreign_space_replacement: Option<&'lt str>,
) -> &'lt str {
    let mut native_space_replacement = DEFAULT_FOREIGN_SPACE_REPLACEMENT;
    if let Some(script) = formatting_locale.id.script {
        native_space_replacement = match script.as_str() {
            "Jpan" | "Hant" | "Hans" => "",
            _ => DEFAULT_FOREIGN_SPACE_REPLACEMENT,
        };
    }
    if formatting_locale.id.language == person_name_locale.id.language {
        return native_space_replacement;
    }
    foreign_space_replacement.unwrap_or(DEFAULT_FOREIGN_SPACE_REPLACEMENT)
}

#[cfg(test)]
mod tests {
    use icu_locale::LocaleExpander;
    use icu_locale_core::locale;

    #[test]
    fn test_formatter_and_name_language_match() {
        let mut formatting_locale = locale!("es");
        let mut person_name_locale = locale!("es");
        let foreign_space_replacement = Some("not used");

        // locales are maximized earlier during the formatting process.
        let lc = LocaleExpander::new_extended();
        lc.maximize(&mut formatting_locale.id);
        lc.maximize(&mut person_name_locale.id);

        let result = super::space_replacement(
            &formatting_locale,
            &person_name_locale,
            foreign_space_replacement,
        );

        assert_eq!(result, " ")
    }

    #[test]
    fn test_formatter_and_name_language_match_ja() {
        let mut formatting_locale = locale!("ja");
        let mut person_name_locale = locale!("ja");
        let foreign_space_replacement = Some("not used");

        // locales are maximized earlier during the formatting process.
        let lc = LocaleExpander::new_extended();
        lc.maximize(&mut formatting_locale.id);
        lc.maximize(&mut person_name_locale.id);

        let result = super::space_replacement(
            &formatting_locale,
            &person_name_locale,
            foreign_space_replacement,
        );

        assert_eq!(result, "")
    }

    #[test]
    fn test_formatter_and_name_language_not_match_en() {
        let mut formatting_locale = locale!("en");
        let mut person_name_locale = locale!("ja");
        let foreign_space_replacement = Some(" "); // en space replacement is a normal space

        // locales are maximized earlier during the formatting process.
        let lc = LocaleExpander::new_extended();
        lc.maximize(&mut formatting_locale.id);
        lc.maximize(&mut person_name_locale.id);

        let result = super::space_replacement(
            &formatting_locale,
            &person_name_locale,
            foreign_space_replacement,
        );

        assert_eq!(result, " ")
    }

    #[test]
    fn test_formatter_and_name_language_not_match_ja() {
        let mut formatting_locale = locale!("ja");
        let mut person_name_locale = locale!("en");
        let foreign_space_replacement = Some("・"); // ja space replacement is a normal space

        // locales are maximized earlier during the formatting process.
        let lc = LocaleExpander::new_extended();
        lc.maximize(&mut formatting_locale.id);
        lc.maximize(&mut person_name_locale.id);

        let result = super::space_replacement(
            &formatting_locale,
            &person_name_locale,
            foreign_space_replacement,
        );

        assert_eq!(result, "・")
    }

    #[test]
    fn test_formatter_and_name_language_match_thai() {
        let mut formatting_locale = locale!("th");
        let mut person_name_locale = locale!("th");
        let foreign_space_replacement = Some("should not be used");

        // locales are maximized earlier during the formatting process.
        let lc = LocaleExpander::new_extended();
        lc.maximize(&mut formatting_locale.id);
        lc.maximize(&mut person_name_locale.id);

        let result = super::space_replacement(
            &formatting_locale,
            &person_name_locale,
            foreign_space_replacement,
        );

        assert_eq!(result, " ")
    }
}
