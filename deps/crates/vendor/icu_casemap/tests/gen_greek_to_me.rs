// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

use icu_casemap::greek_to_me::{
    self, GreekDiacritics, GreekPrecomposedLetterData, GreekVowel, PackedGreekPrecomposedLetterData,
};
use icu_casemap::CaseMapper;
use icu_locale_core::LanguageIdentifier;
use icu_normalizer::DecomposingNormalizerBorrowed;
use icu_properties::{
    props::{GeneralCategory, GeneralCategoryGroup, Script},
    CodePointMapData,
};
use std::collections::BTreeMap;
use std::fmt::Write;

fn main() {
    let decomposer = DecomposingNormalizerBorrowed::new_nfd();
    let script = CodePointMapData::<Script>::new();
    let gc = CodePointMapData::<GeneralCategory>::new();
    let cm = CaseMapper::new();

    let mut vec_370 = vec![0; 0x400 - 0x370];
    let mut vec_1f00 = vec![0; 0x100];
    let mut extras = BTreeMap::new();

    // Loop over all codepoints
    for range in script.iter_ranges_for_value(Script::Greek) {
        for ch in range {
            if let Ok(ch) = char::try_from(ch) {
                if !GeneralCategoryGroup::Letter.contains(gc.get(ch)) {
                    continue;
                }
                let mut buf = [0u8; 4];
                let nfd = decomposer.normalize_utf8(ch.encode_utf8(&mut buf).as_bytes());
                let mut data: Option<GreekPrecomposedLetterData> = None;

                for nfd_ch in nfd.chars() {
                    match nfd_ch {
                        // accented: [:toNFD=/[\u0300\u0301\u0342\u0302\u0303\u0311]/:]&[:Grek:]&[:L:] (from the JSPs: toNFD is an extension).
                        greek_to_me::diacritics!(ACCENTS) => {
                            if let Some(GreekPrecomposedLetterData::Vowel(_, ref mut diacritics)) =
                                data
                            {
                                diacritics.accented = true;
                            } else {
                                panic!("Found character {ch} that has diacritics but is not a Greek vowel");
                            }
                        }
                        // dialytika: [:toNFD=/[\u0308]/:]&[:Grek:]&[:L:] (from the JSPs: toNFD is an extension).
                        greek_to_me::diacritics!(DIALYTIKA) => {
                            if let Some(GreekPrecomposedLetterData::Vowel(_, ref mut diacritics)) =
                                data
                            {
                                diacritics.dialytika = true;
                            } else {
                                panic!("Found character {ch} that has diacritics but is not a Greek vowel");
                            }
                        }
                        // precomposed_ypogegrammeni [:toNFD=/[\u0345]/:]&[:Grek:]&[:L:] (from the JSPs: toNFD is an extension).
                        greek_to_me::diacritics!(YPOGEGRAMMENI) => {
                            if let Some(GreekPrecomposedLetterData::Vowel(_, ref mut diacritics)) =
                                data
                            {
                                diacritics.ypogegrammeni = true;
                            } else {
                                panic!("Found character {ch} that has diacritics but is not a Greek vowel");
                            }
                        }
                        greek_to_me::diacritics!(BREATHING_AND_LENGTH)
                        | greek_to_me::diacritics!(ACCENTS) => {
                            // Rho takes breathing marks but other consonants should not
                            if let Some(GreekPrecomposedLetterData::Consonant(false)) = data {
                                panic!("Found character {ch} that has diacritics but is not a Greek vowel");
                            }
                        }
                        // Ignore all small letters
                        '\u{1D00}'..='\u{1DBF}' | '\u{AB65}' => (),
                        // caps: [[:Grek:]&[:L:]-[\u1D00-\u1DBF\uAB65]] . NFD, remove non-letters, uppercase
                        letter if GeneralCategoryGroup::Letter.contains(gc.get(letter)) => {
                            let uppercased = cm
                                .uppercase_to_string(
                                    letter.encode_utf8(&mut [0; 4]),
                                    &LanguageIdentifier::UNKNOWN,
                                )
                                .into_owned();
                            let mut iter = uppercased.chars();
                            let uppercased = iter.next().unwrap();
                            assert!(
                                iter.next().is_none(),
                                "{letter} Should uppercase to a single letter char, instead uppercased to {uppercased:?}"
                            );

                            if let Ok(vowel) = GreekVowel::try_from(uppercased) {
                                data = Some(GreekPrecomposedLetterData::Vowel(
                                    vowel,
                                    GreekDiacritics::default(),
                                ))
                            } else {
                                let is_rho = uppercased == greek_to_me::CAPITAL_RHO;
                                data = Some(GreekPrecomposedLetterData::Consonant(is_rho))
                            };
                        }
                        _ => (),
                    }
                }

                if let Some(data) = data {
                    let packed = PackedGreekPrecomposedLetterData::from(data);
                    assert_eq!(
                        Ok(data),
                        packed.try_into(),
                        "packed data for {ch} ({packed:?}) must roundtrip!"
                    );
                    let ch_i = ch as usize;
                    if (0x370..=0x3FF).contains(&ch_i) {
                        vec_370[ch_i - 0x370] = packed.0;
                    } else if (0x1f00..0x1fff).contains(&ch_i) {
                        vec_1f00[ch_i - 0x1f00] = packed.0;
                    } else {
                        extras.insert(ch, packed.0);
                    }
                }
            }
        }
    }

    vec_370.truncate(
        vec_370
            .iter()
            .rposition(|x| *x != 0)
            .expect("must have some greek data")
            + 1,
    );
    vec_1f00.truncate(
        vec_1f00
            .iter()
            .rposition(|x| *x != 0)
            .expect("must have some greek data")
            + 1,
    );

    let mut others = String::new();
    for (ch, data) in extras {
        writeln!(&mut others, "        '{ch}' => {data},").unwrap();
    }

    let output = format!(
        r#"// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

// This file is generated by running `cargo test --test gen_greek_to_me --features compiled_data,datagen
//
// Do not edit manually

// All u8s in this file are PackedGreekPrecomposedLetterDatas, see parent module

/// Data for characters in U+370-U+3FF
pub(crate) const DATA_370: [u8; 0x{:x}] = {vec_370:?};
/// Data for characters in U+1F00-U+1FFF
pub(crate) const DATA_1F00: [u8; 0x{:x}] = {vec_1f00:?};

/// Characters like the ohm sign that do not belong in the two blocks above
pub(crate) fn match_extras(ch: char) -> Option<u8> {{
    Some(match ch {{
{others}
        _ => return None
    }})
}}
"#,
        vec_370.len(),
        vec_1f00.len(),
    );

    let local = include_str!("../src/greek_to_me/data.rs");

    // We cannot just check if the two are unequal because on Windows core.autocrlf
    // may have messed with the line endings on the file, or it may have not (either
    // due to a changed setting, or due to the code being in a cargo cache/vendor. We also
    // cannot fix this by passing `--config newline_style=unix` to rustfmt. We must
    // perform a `\r`-agnostic comparison.
    //
    // (technically this should only catch `\r\n` and not just `\r` but for a golden
    // test on rustfmt output it does not matter)
    if local
        .trim()
        .chars()
        .filter(|&x| x != '\r')
        .ne(output.trim().chars().filter(|&x| x != '\r'))
    {
        println!(
            r#"Please copy the following file to src/greek_to_me/data.rs:
========================================================
{output}
========================================================"#
        );

        panic!("Found mismatch between generated Greek specialcasing data and checked-in data. Please check in the updated file shown above.");
    }
}
