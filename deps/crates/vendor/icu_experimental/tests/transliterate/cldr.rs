// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

#![allow(unused_imports)]

// TODO(#3736): find a way to keep cldr_testData uptodate

use icu_experimental::transliterate::Transliterator;

#[test]
fn test_all_cldr() {
    for (locale, data) in [
        (
            "de-t-de-d0-ascii",
            include_str!("data/fixtures/de-t-de-d0-ascii.txt"),
        ),
        (
            "el-Latn-t-el-m0-bgn",
            include_str!("data/fixtures/el-Latn-t-el-m0-bgn.txt"),
        ),
        (
            "und-Arab-t-und-beng",
            include_str!("data/fixtures/und-Arab-t-und-beng.txt"),
        ),
        (
            "und-t-d0-publish",
            include_str!("data/fixtures/und-t-d0-publish.txt"),
        ),
        (
            "und-t-s0-publish",
            include_str!("data/fixtures/und-t-s0-publish.txt"),
        ),
        (
            "und-t-und-latn-d0-ascii",
            include_str!("data/fixtures/und-t-und-latn-d0-ascii.txt"),
        ),
    ] {
        let t = Transliterator::try_new(&locale.parse().unwrap()).unwrap();
        let test_cases = data
            .lines()
            .filter(|x| !x.starts_with('#'))
            .map(|l| l.split_once('\t').unwrap())
            .collect::<Vec<_>>();

        eprintln!("Testing {:?} with {} test cases", locale, test_cases.len());

        for (idx, (input, output)) in test_cases.into_iter().enumerate() {
            eprintln!(
                "Testing testcase {}! Input: {:?} Output: {:?}",
                idx + 1,
                input,
                output
            );
            let actual = t.transliterate(input.to_string());
            assert_eq!(actual, output, "Transliterator {locale:?} failed");
            eprintln!("Passed testcase {}!", idx + 1);
        }
        eprintln!("Transliterator {locale:?} passed");
    }
}
