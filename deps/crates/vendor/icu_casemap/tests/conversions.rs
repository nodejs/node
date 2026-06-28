// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

use icu_casemap::CaseMapper;
use icu_locale_core::langid;

#[test]
fn test_simple_mappings() {
    let case_mapping = CaseMapper::new();

    // Basic case mapping
    assert_eq!(case_mapping.simple_uppercase('a'), 'A');
    assert_eq!(case_mapping.simple_lowercase('a'), 'a');
    assert_eq!(case_mapping.simple_titlecase('a'), 'A');
    assert_eq!(case_mapping.simple_fold('a'), 'a');
    assert_eq!(case_mapping.simple_uppercase('A'), 'A');
    assert_eq!(case_mapping.simple_lowercase('A'), 'a');
    assert_eq!(case_mapping.simple_titlecase('A'), 'A');
    assert_eq!(case_mapping.simple_fold('A'), 'a');

    // Case mapping of titlecase character
    assert_eq!(case_mapping.simple_uppercase('\u{1c4}'), '\u{1c4}');
    assert_eq!(case_mapping.simple_titlecase('\u{1c4}'), '\u{1c5}');
    assert_eq!(case_mapping.simple_lowercase('\u{1c4}'), '\u{1c6}');
    assert_eq!(case_mapping.simple_uppercase('\u{1c5}'), '\u{1c4}');
    assert_eq!(case_mapping.simple_titlecase('\u{1c5}'), '\u{1c5}');
    assert_eq!(case_mapping.simple_lowercase('\u{1c5}'), '\u{1c6}');
    assert_eq!(case_mapping.simple_uppercase('\u{1c6}'), '\u{1c4}');
    assert_eq!(case_mapping.simple_titlecase('\u{1c6}'), '\u{1c5}');
    assert_eq!(case_mapping.simple_lowercase('\u{1c6}'), '\u{1c6}');

    // Turkic case folding
    assert_eq!(case_mapping.simple_fold('I'), 'i');
    assert_eq!(case_mapping.simple_fold_turkic('I'), 'ı');
    assert_eq!(case_mapping.simple_fold('İ'), 'İ');
    assert_eq!(case_mapping.simple_fold_turkic('İ'), 'i');

    // Supplementary code points (Deseret)
    assert_eq!(case_mapping.simple_uppercase('\u{1043c}'), '\u{10414}');
    assert_eq!(case_mapping.simple_lowercase('\u{1043c}'), '\u{1043c}');
    assert_eq!(case_mapping.simple_titlecase('\u{1043c}'), '\u{10414}');
    assert_eq!(case_mapping.simple_fold('\u{1043c}'), '\u{1043c}');
    assert_eq!(case_mapping.simple_uppercase('\u{10414}'), '\u{10414}');
    assert_eq!(case_mapping.simple_lowercase('\u{10414}'), '\u{1043c}');
    assert_eq!(case_mapping.simple_titlecase('\u{10414}'), '\u{10414}');
    assert_eq!(case_mapping.simple_fold('\u{10414}'), '\u{1043c}');
}

// These and the below tests are taken from StringCaseTest::TestCaseConversion in ICU4C.
#[test]
fn test_full_mappings() {
    let case_mapping = CaseMapper::new();
    let root = langid!("und");
    let tr = langid!("tr");
    let lt = langid!("lt");

    let uppercase_greek = "ΙΕΣΥΣ ΧΡΙΣΤΟΣ"; // "IESUS CHRISTOS"
    let lowercase_greek = "ιεσυς χριστος"; // "IESUS CHRISTOS"
    assert_eq!(
        case_mapping.uppercase_to_string(lowercase_greek, &root),
        uppercase_greek
    );
    assert_eq!(
        case_mapping.lowercase_to_string(uppercase_greek, &root),
        lowercase_greek
    );
    assert_eq!(
        case_mapping.fold_string(uppercase_greek),
        case_mapping.fold_string(lowercase_greek)
    );

    let lowercase_turkish_1 = "istanbul, not constantınople";
    let uppercase_turkish_1 = "İSTANBUL, NOT CONSTANTINOPLE";
    assert_eq!(
        case_mapping.lowercase_to_string(uppercase_turkish_1, &root),
        "i\u{307}stanbul, not constantinople"
    );
    assert_eq!(
        case_mapping.lowercase_to_string(uppercase_turkish_1, &tr),
        lowercase_turkish_1
    );

    let lowercase_turkish_2 = "topkapı palace, istanbul";
    let uppercase_turkish_2 = "TOPKAPI PALACE, İSTANBUL";
    assert_eq!(
        case_mapping.uppercase_to_string(lowercase_turkish_2, &root),
        "TOPKAPI PALACE, ISTANBUL"
    );
    assert_eq!(
        case_mapping.uppercase_to_string(lowercase_turkish_2, &tr),
        uppercase_turkish_2
    );

    let initial_german = "Süßmayrstraße";
    let uppercase_german = "SÜSSMAYRSTRASSE";
    assert_eq!(
        case_mapping.uppercase_to_string(initial_german, &root),
        uppercase_german
    );

    let before = "aBIΣßΣ/\u{5ffff}";
    let after = "abiσßς/\u{5ffff}";
    let after_turkish = "abıσßς/\u{5ffff}";
    assert_eq!(case_mapping.lowercase_to_string(before, &root), after);
    assert_eq!(case_mapping.lowercase_to_string(before, &tr), after_turkish);

    let before = "aBiςßσ/\u{fb03}\u{fb03}\u{fb03}\u{5ffff}";
    let after = "ABIΣSSΣ/FFIFFIFFI\u{5ffff}";
    let after_turkish = "ABİΣSSΣ/FFIFFIFFI\u{5ffff}";
    assert_eq!(case_mapping.uppercase_to_string(before, &root), after);
    assert_eq!(case_mapping.uppercase_to_string(before, &tr), after_turkish);

    let before = "ßa";
    let after = "SSA";
    assert_eq!(case_mapping.uppercase_to_string(before, &root), after);

    let initial_deseret = "\u{1043c}\u{10414}";
    let upper_deseret = "\u{10414}\u{10414}";
    let lower_deseret = "\u{1043c}\u{1043c}";
    assert_eq!(
        case_mapping.uppercase_to_string(initial_deseret, &root),
        upper_deseret
    );
    assert_eq!(
        case_mapping.lowercase_to_string(initial_deseret, &root),
        lower_deseret
    );

    // lj ligature
    let initial_ligature = "\u{1c7}\u{1c8}\u{1c9}";
    let lower_ligature = "\u{1c9}\u{1c9}\u{1c9}";
    let upper_ligature = "\u{1c7}\u{1c7}\u{1c7}";
    assert_eq!(
        case_mapping.uppercase_to_string(initial_ligature, &root),
        upper_ligature
    );
    assert_eq!(
        case_mapping.lowercase_to_string(initial_ligature, &root),
        lower_ligature
    );

    // Sigmas preceded and/or followed by cased letters
    let initial_sigmas = "i\u{307}\u{3a3}\u{308}j \u{307}\u{3a3}\u{308}j i\u{ad}\u{3a3}\u{308} \u{307}\u{3a3}\u{308}";
    let lower_sigmas = "i\u{307}\u{3c3}\u{308}j \u{307}\u{3c3}\u{308}j i\u{ad}\u{3c2}\u{308} \u{307}\u{3c3}\u{308}";
    let upper_sigmas = "I\u{307}\u{3a3}\u{308}J \u{307}\u{3a3}\u{308}J I\u{ad}\u{3a3}\u{308} \u{307}\u{3a3}\u{308}";
    assert_eq!(
        case_mapping.uppercase_to_string(initial_sigmas, &root),
        upper_sigmas
    );
    assert_eq!(
        case_mapping.lowercase_to_string(initial_sigmas, &root),
        lower_sigmas
    );

    // Turkish & Azerbaijani dotless i & dotted I:
    // Remove dot above if there was a capital I before and there are no more accents above.
    let initial_dots = "I İ I\u{307} I\u{327}\u{307} I\u{301}\u{307} I\u{327}\u{307}\u{301}";
    let after = "i i\u{307} i\u{307} i\u{327}\u{307} i\u{301}\u{307} i\u{327}\u{307}\u{301}";
    let after_turkish = "ı i i i\u{327} ı\u{301}\u{307} i\u{327}\u{301}";
    assert_eq!(case_mapping.lowercase_to_string(initial_dots, &root), after);
    assert_eq!(
        case_mapping.lowercase_to_string(initial_dots, &tr),
        after_turkish
    );

    // Lithuanian dot above in uppercasing
    let initial_dots = "a\u{307} \u{307} i\u{307} j\u{327}\u{307} j\u{301}\u{307}";
    let after = "A\u{307} \u{307} I\u{307} J\u{327}\u{307} J\u{301}\u{307}";
    let after_lithuanian = "A\u{307} \u{307} I J\u{327} J\u{301}\u{307}";
    assert_eq!(case_mapping.uppercase_to_string(initial_dots, &root), after);
    assert_eq!(
        case_mapping.uppercase_to_string(initial_dots, &lt),
        after_lithuanian
    );

    // Lithuanian adds dot above to i in lowercasing if there are more above accents
    let initial_dots = "I I\u{301} J J\u{301} \u{12e} \u{12e}\u{301} \u{cc}\u{cd}\u{128}";
    let after = "i i\u{301} j j\u{301} \u{12f} \u{12f}\u{301} \u{ec}\u{ed}\u{129}";
    let after_lithuanian = "i i\u{307}\u{301} j j\u{307}\u{301} \u{12f} \u{12f}\u{307}\u{301} i\u{307}\u{300}i\u{307}\u{301}i\u{307}\u{303}";
    assert_eq!(case_mapping.lowercase_to_string(initial_dots, &root), after);
    assert_eq!(
        case_mapping.lowercase_to_string(initial_dots, &lt),
        after_lithuanian
    );

    // Test case folding
    let initial = "Aßµ\u{fb03}\u{1040c}İı";
    let simple = "assμffi\u{10434}i\u{307}ı";
    let turkic = "assμffi\u{10434}iı";
    assert_eq!(case_mapping.fold_string(initial), simple);
    assert_eq!(case_mapping.fold_turkic_string(initial), turkic);
}

#[test]
fn test_armenian() {
    let cm = CaseMapper::new();
    let root = langid!("und");
    let east = langid!("hy");
    let west = langid!("hyw");
    let default_options = Default::default();

    let s = "և Երևանի";

    assert_eq!(cm.uppercase_to_string(s, &root), "ԵՒ ԵՐԵՒԱՆԻ");
    assert_eq!(cm.uppercase_to_string(s, &east), "ԵՎ ԵՐԵՎԱՆԻ");
    assert_eq!(cm.uppercase_to_string(s, &west), "ԵՒ ԵՐԵՒԱՆԻ");

    let ew = "և";
    let yerevan = "Երևանի";
    assert_eq!(
        cm.titlecase_segment_with_only_case_data_to_string(ew, &root, default_options),
        "Եւ"
    );
    assert_eq!(
        cm.titlecase_segment_with_only_case_data_to_string(yerevan, &root, default_options),
        "Երևանի"
    );
    assert_eq!(
        cm.titlecase_segment_with_only_case_data_to_string(ew, &east, default_options),
        "Եվ"
    );
    assert_eq!(
        cm.titlecase_segment_with_only_case_data_to_string(yerevan, &east, default_options),
        "Երևանի"
    );
    assert_eq!(
        cm.titlecase_segment_with_only_case_data_to_string(ew, &west, default_options),
        "Եւ"
    );
    assert_eq!(
        cm.titlecase_segment_with_only_case_data_to_string(yerevan, &west, default_options),
        "Երևանի"
    );
}

#[test]
fn test_dutch() {
    let cm = CaseMapper::new();
    let nl = langid!("nl");
    let default_options = Default::default();

    assert_eq!(
        cm.titlecase_segment_with_only_case_data_to_string("ijssel", &nl, default_options),
        "IJssel"
    );
    assert_eq!(
        cm.titlecase_segment_with_only_case_data_to_string("igloo", &nl, default_options),
        "Igloo"
    );
    assert_eq!(
        cm.titlecase_segment_with_only_case_data_to_string("IJMUIDEN", &nl, default_options),
        "IJmuiden"
    );

    assert_eq!(
        cm.titlecase_segment_with_only_case_data_to_string("ij", &nl, default_options),
        "IJ"
    );
    assert_eq!(
        cm.titlecase_segment_with_only_case_data_to_string("IJ", &nl, default_options),
        "IJ"
    );
    assert_eq!(
        cm.titlecase_segment_with_only_case_data_to_string("íj́", &nl, default_options),
        "ÍJ́"
    );
    assert_eq!(
        cm.titlecase_segment_with_only_case_data_to_string("ÍJ́", &nl, default_options),
        "ÍJ́"
    );
    assert_eq!(
        cm.titlecase_segment_with_only_case_data_to_string("íJ́", &nl, default_options),
        "ÍJ́"
    );
    assert_eq!(
        cm.titlecase_segment_with_only_case_data_to_string("Ij́", &nl, default_options),
        "Ij́"
    );
    assert_eq!(
        cm.titlecase_segment_with_only_case_data_to_string("ij́", &nl, default_options),
        "Ij́"
    );
    assert_eq!(
        cm.titlecase_segment_with_only_case_data_to_string("ïj́", &nl, default_options),
        "Ïj́"
    );
    assert_eq!(
        cm.titlecase_segment_with_only_case_data_to_string("íj\u{0308}", &nl, default_options),
        "Íj\u{0308}"
    );
    assert_eq!(
        cm.titlecase_segment_with_only_case_data_to_string("íj́\u{1D16E}", &nl, default_options),
        "Íj́\u{1D16E}"
    );
    assert_eq!(
        cm.titlecase_segment_with_only_case_data_to_string("íj\u{1ABE}", &nl, default_options),
        "Íj\u{1ABE}"
    );

    assert_eq!(
        cm.titlecase_segment_with_only_case_data_to_string("ijabc", &nl, default_options),
        "IJabc"
    );
    assert_eq!(
        cm.titlecase_segment_with_only_case_data_to_string("IJabc", &nl, default_options),
        "IJabc"
    );
    assert_eq!(
        cm.titlecase_segment_with_only_case_data_to_string("íj́abc", &nl, default_options),
        "ÍJ́abc"
    );
    assert_eq!(
        cm.titlecase_segment_with_only_case_data_to_string("ÍJ́abc", &nl, default_options),
        "ÍJ́abc"
    );
    assert_eq!(
        cm.titlecase_segment_with_only_case_data_to_string("íJ́abc", &nl, default_options),
        "ÍJ́abc"
    );
    assert_eq!(
        cm.titlecase_segment_with_only_case_data_to_string("Ij́abc", &nl, default_options),
        "Ij́abc"
    );
    assert_eq!(
        cm.titlecase_segment_with_only_case_data_to_string("ij́abc", &nl, default_options),
        "Ij́abc"
    );
    assert_eq!(
        cm.titlecase_segment_with_only_case_data_to_string("ïj́abc", &nl, default_options),
        "Ïj́abc"
    );
    assert_eq!(
        cm.titlecase_segment_with_only_case_data_to_string("íjabc\u{0308}", &nl, default_options),
        "Íjabc\u{0308}"
    );
    assert_eq!(
        cm.titlecase_segment_with_only_case_data_to_string("íj́abc\u{1D16E}", &nl, default_options),
        "ÍJ́abc\u{1D16E}"
    );
    assert_eq!(
        cm.titlecase_segment_with_only_case_data_to_string("íjabc\u{1ABE}", &nl, default_options),
        "Íjabc\u{1ABE}"
    );
}

#[test]
fn test_greek_upper() {
    let nfc = icu_normalizer::ComposingNormalizerBorrowed::new_nfc();
    let nfd = icu_normalizer::DecomposingNormalizerBorrowed::new_nfd();

    let cm = CaseMapper::new();
    let modern_greek = &langid!("el");

    let assert_greek_uppercase = |input: &str, expected: &str| {
        assert_eq!(
            cm.uppercase_to_string(nfc.normalize(input).as_ref(), modern_greek),
            nfc.normalize(expected)
        );
        assert_eq!(
            cm.uppercase_to_string(nfd.normalize(input).as_ref(), modern_greek),
            nfd.normalize(expected)
        );
    };

    // https://unicode-org.atlassian.net/browse/ICU-5456
    assert_greek_uppercase("άδικος, κείμενο, ίριδα", "ΑΔΙΚΟΣ, ΚΕΙΜΕΝΟ, ΙΡΙΔΑ");
    // https://bugzilla.mozilla.org/show_bug.cgi?id=307039
    // https://bug307039.bmoattachments.org/attachment.cgi?id=194893
    assert_greek_uppercase("Πατάτα", "ΠΑΤΑΤΑ");
    assert_greek_uppercase("Αέρας, Μυστήριο, Ωραίο", "ΑΕΡΑΣ, ΜΥΣΤΗΡΙΟ, ΩΡΑΙΟ");
    assert_greek_uppercase("Μαΐου, Πόρος, Ρύθμιση", "ΜΑΪΟΥ, ΠΟΡΟΣ, ΡΥΘΜΙΣΗ");
    assert_greek_uppercase("ΰ, Τηρώ, Μάιος", "Ϋ, ΤΗΡΩ, ΜΑΪΟΣ");
    assert_greek_uppercase("άυλος", "ΑΫΛΟΣ");
    assert_greek_uppercase("ΑΫΛΟΣ", "ΑΫΛΟΣ");
    assert_greek_uppercase(
        "Άκλιτα ρήματα ή άκλιτες μετοχές",
        "ΑΚΛΙΤΑ ΡΗΜΑΤΑ Ή ΑΚΛΙΤΕΣ ΜΕΤΟΧΕΣ",
    );
    // http://www.unicode.org/udhr/d/udhr_ell_monotonic.html
    assert_greek_uppercase(
        "Επειδή η αναγνώριση της αξιοπρέπειας",
        "ΕΠΕΙΔΗ Η ΑΝΑΓΝΩΡΙΣΗ ΤΗΣ ΑΞΙΟΠΡΕΠΕΙΑΣ",
    );
    assert_greek_uppercase("νομικού ή διεθνούς", "ΝΟΜΙΚΟΥ Ή ΔΙΕΘΝΟΥΣ");
    // http://unicode.org/udhr/d/udhr_ell_polytonic.html
    assert_greek_uppercase("Ἐπειδὴ ἡ ἀναγνώριση", "ΕΠΕΙΔΗ Η ΑΝΑΓΝΩΡΙΣΗ");
    assert_greek_uppercase("νομικοῦ ἢ διεθνοῦς", "ΝΟΜΙΚΟΥ Ή ΔΙΕΘΝΟΥΣ");
    // From Google bug report
    assert_greek_uppercase("Νέο, Δημιουργία", "ΝΕΟ, ΔΗΜΙΟΥΡΓΙΑ");
    // http://crbug.com/234797
    assert_greek_uppercase(
        "Ελάτε να φάτε τα καλύτερα παϊδάκια!",
        "ΕΛΑΤΕ ΝΑ ΦΑΤΕ ΤΑ ΚΑΛΥΤΕΡΑ ΠΑΪΔΑΚΙΑ!",
    );
    assert_greek_uppercase("Μαΐου, τρόλεϊ", "ΜΑΪΟΥ, ΤΡΟΛΕΪ");
    assert_greek_uppercase("Το ένα ή το άλλο.", "ΤΟ ΕΝΑ Ή ΤΟ ΑΛΛΟ.");
    // http://multilingualtypesetting.co.uk/blog/greek-typesetting-tips/
    assert_greek_uppercase("ρωμέικα", "ΡΩΜΕΪΚΑ");
    assert_greek_uppercase("ή.", "Ή.");

    // The ὑπογεγραμμέναι become Ι as in default case conversion, but they are
    // specially handled by the implementation.
    assert_greek_uppercase("ᾠδή, -ήν, -ῆς, -ῇ", "ΩΙΔΗ, -ΗΝ, -ΗΣ, -ΗΙ");
    assert_greek_uppercase("ᾍδης", "ΑΙΔΗΣ");

    // Handle breathing marks on rho
    assert_greek_uppercase("ῥήματα ῤήματα", "ΡΗΜΑΤΑ ΡΗΜΑΤΑ");
}
