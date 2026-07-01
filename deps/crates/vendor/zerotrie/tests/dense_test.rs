// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

use itertools::Either;
use itertools::Itertools;
use rand::Rng;
use rand::SeedableRng;
use std::collections::BTreeMap;
use zerotrie::dense::ZeroAsciiDenseSparse2dTrieBorrowed;
use zerotrie::dense::ZeroAsciiDenseSparse2dTrieOwned;
use zerotrie::dense::ZeroAsciiDenseSparse2dTrieVarULE;
use zerotrie::ZeroTrieSimpleAscii;
use zerovec::VarZeroCow;

mod testdata {
    include!("data/data.rs");
}

const BASIC_DATA: &[&str] = &[
    "ar/FR", "ar/IR", "ar/SA", "ar/UK", "ar/US", "en/AU", "en/FR", "en/UK", "en/US", "fr/FR",
    "fr/SA", "fr/UK", "fr/US", "it/IT",
];

const NON_EXISTENT_BASIC_DATA: &[&str] = &["ar/IT", "en/ZA", "it/FR", "zh/UK"];

fn strings_to_2d_btreemap<'a>(
    strings: &[&'a str],
    delimiter: &str,
) -> BTreeMap<&'a str, BTreeMap<&'a str, usize>> {
    let mut map = BTreeMap::<&str, BTreeMap<&str, usize>>::default();
    for (value, (prefix, suffix)) in strings
        .iter()
        .flat_map(|s| s.split_once(delimiter))
        .enumerate()
    {
        map.entry(prefix).or_default().insert(suffix, value);
    }
    map
}

fn check_data(
    data: &BTreeMap<&str, BTreeMap<&str, usize>>,
    trie: ZeroAsciiDenseSparse2dTrieBorrowed,
    polarity: bool,
) {
    for (prefix, values) in data.iter() {
        for (suffix, value) in values.iter() {
            if polarity {
                assert_eq!(trie.get(prefix, suffix), Some(*value), "{prefix}/{suffix}");
            } else {
                assert_eq!(trie.get(prefix, suffix), None, "{prefix}/{suffix}");
            }
        }
    }
}

#[must_use]
fn check_encoding(trie: ZeroAsciiDenseSparse2dTrieBorrowed) -> usize {
    let ule =
        VarZeroCow::<ZeroAsciiDenseSparse2dTrieVarULE>::from_encodeable(&trie.as_encodeable());
    let decoded = ZeroAsciiDenseSparse2dTrieBorrowed::from(&*ule);
    assert_eq!(decoded, trie);
    ule.as_bytes().len()
}

#[must_use]
fn make_simple_ascii_trie(
    data: &BTreeMap<&str, BTreeMap<&str, usize>>,
) -> ZeroTrieSimpleAscii<Vec<u8>> {
    let mut data_for_simple_ascii_trie = BTreeMap::new();
    for (prefix, values) in data.iter() {
        for (suffix, value) in values.iter() {
            data_for_simple_ascii_trie.insert(format!("{prefix}/{suffix}"), *value);
        }
    }
    ZeroTrieSimpleAscii::try_from_btree_map_str(&data_for_simple_ascii_trie).unwrap()
}

#[test]
fn test_basic() {
    let data_as_map = strings_to_2d_btreemap(BASIC_DATA, "/");
    let dense =
        ZeroAsciiDenseSparse2dTrieOwned::try_from_btree_map_str(&data_as_map, b'/').unwrap();
    check_data(&data_as_map, dense.as_borrowed(), true);
    let non_existent_data_as_map = strings_to_2d_btreemap(NON_EXISTENT_BASIC_DATA, "/");
    check_data(&non_existent_data_as_map, dense.as_borrowed(), false);
    let byte_len = check_encoding(dense.as_borrowed());
    assert_eq!(byte_len, 106);
    let simple_trie = make_simple_ascii_trie(&data_as_map);
    assert_eq!(simple_trie.byte_len(), 71);
}

#[test]
fn test_locales_with_aux() {
    let data_as_map = strings_to_2d_btreemap(testdata::locales_with_aux::STRINGS, "-x-");
    let dense =
        ZeroAsciiDenseSparse2dTrieOwned::try_from_btree_map_str(&data_as_map, b'/').unwrap();
    check_data(&data_as_map, dense.as_borrowed(), true);
    let byte_len = check_encoding(dense.as_borrowed());
    assert_eq!(byte_len, 4045);
    let simple_trie = make_simple_ascii_trie(&data_as_map);
    assert_eq!(simple_trie.byte_len(), 4614);
}

#[test]
fn test_short_subtags() {
    let (prefixes, suffixes): (Vec<&str>, Vec<&str>) = testdata::short_subtags::STRINGS
        .iter()
        .copied()
        .partition_map(|s| match s.strip_prefix("und-") {
            Some(suffix) => Either::Right(suffix),
            None => Either::Left(s),
        });
    assert_eq!(prefixes.len(), 1427);
    assert_eq!(suffixes.len(), 450);
    let mut counter = 0;
    let mut rng = rand_pcg::Lcg64Xsh32::seed_from_u64(2025);
    let mut data: BTreeMap<&str, BTreeMap<&str, usize>> = BTreeMap::new();
    for prefix in prefixes.iter() {
        // Two types of keys: big and small. Pick at random with a certain chance of big.
        let is_big = rng.random::<f64>() < 0.3;
        for suffix in suffixes.iter() {
            // Include the suffix with a probability dependent on is_big.
            let include_value = rng.random::<f64>() < if is_big { 0.8 } else { 0.05 };
            if include_value {
                data.entry(prefix).or_default().insert(suffix, counter);
                counter += 1;
            }
        }
    }
    assert_eq!(data.values().map(|m| m.len()).sum::<usize>(), 175306);
    let dense = ZeroAsciiDenseSparse2dTrieOwned::try_from_btree_map_str(&data, b'/').unwrap();
    check_data(&data, dense.as_borrowed(), true);
    let byte_len = check_encoding(dense.as_borrowed());
    assert_eq!(byte_len, 555240);
    let simple_trie = make_simple_ascii_trie(&data);
    assert_eq!(simple_trie.byte_len(), 1099634);
}

#[test]
fn test_dense_sparse_window_selection() {
    //Make initial Btree (not using enumerate)
    let row_width = usize::from(u16::MAX); // Densetype max
    let far_low = 0;
    let cluster_start = 50;
    let far_high = cluster_start + row_width + 100;

    let mut inner = BTreeMap::new();
    inner.insert("low", far_low);
    inner.insert("a", 50); // cluster_start
    inner.insert("c", 53); // cluster_start + 3
    inner.insert("d", cluster_start + row_width - 3);
    inner.insert("e", cluster_start + row_width - 2);
    inner.insert("f", cluster_start + row_width - 1);
    inner.insert("c2", 53); // cluster_start + 3
    inner.insert("g", cluster_start + row_width);
    inner.insert("h", cluster_start + row_width + 1);
    inner.insert("b", 52); // cluster_start + 2
    inner.insert("high", far_high);
    inner.insert("c3", 53); // cluster_start + 3
    inner.insert("low2", far_low);

    let mut data = BTreeMap::new();
    data.insert("p", inner);

    // Build the 2d trie.
    let dense = ZeroAsciiDenseSparse2dTrieOwned::try_from_btree_map_str(&data, b'/').unwrap();
    let trie = dense.as_borrowed();

    assert_eq!(
        format!("{:?}", trie),
        "ZeroAsciiDenseSparse2dTrieBorrowed { primary: ZeroTrieSimpleAscii { store: [112, 128, 47, 144, 36, 195, 97, 104, 108, 2, 8, 144, 34, 105, 103, 104, 147, 241, 5, 111, 119, 128, 50, 128] }, suffixes: ZeroTrieSimpleAscii { store: [201, 97, 98, 99, 100, 101, 102, 103, 104, 108, 1, 2, 9, 10, 11, 12, 13, 18, 128, 129, 130, 194, 50, 51, 1, 131, 132, 133, 134, 135, 136, 137, 105, 103, 104, 138, 111, 119, 139, 50, 140] }, dense: ZeroVec([65535, 0, 1, 1, 1, 65530, 65531, 65532, 65533, 65534, 65535, 65535, 65535]), suffix_count: 13, delimiter: 47 }"
    );

    check_data(&data, trie, true);

    let byte_len = check_encoding(dense.as_borrowed());
    assert_eq!(byte_len, 102);
    let simple_trie = make_simple_ascii_trie(&data);
    assert_eq!(simple_trie.byte_len(), 60);
}

#[test]
fn test_dense_suffix_count_with_low_frequency_suffixes() {
    // Create a dataset where many suffixes appear in only one prefix.
    // This should result in fewer dense suffixes than if we included all.

    let mut data: BTreeMap<&str, BTreeMap<&str, usize>> = BTreeMap::new();

    // Common suffixes that appear in multiple prefixes
    data.entry("en").or_default().insert("US", 1);
    data.entry("en").or_default().insert("GB", 2);
    data.entry("en").or_default().insert("CA", 3);

    data.entry("fr").or_default().insert("FR", 4);
    data.entry("fr").or_default().insert("CA", 5);
    data.entry("fr").or_default().insert("BE", 6);

    data.entry("de").or_default().insert("DE", 7);
    data.entry("de").or_default().insert("AT", 8);
    data.entry("de").or_default().insert("CH", 9);

    // Low-frequency suffixes (appear in only one prefix each)
    data.entry("en").or_default().insert("rare1", 100);
    data.entry("fr").or_default().insert("rare2", 101);
    data.entry("de").or_default().insert("rare3", 102);
    data.entry("es").or_default().insert("rare4", 103);
    data.entry("es").or_default().insert("ES", 104);

    let trie = ZeroAsciiDenseSparse2dTrieOwned::try_from_btree_map_str(&data, b'/').unwrap();

    assert_eq!(
        format!("{:?}", trie.as_borrowed()),
        "ZeroAsciiDenseSparse2dTrieBorrowed { primary: ZeroTrieSimpleAscii { store: [195, 100, 101, 102, 22, 61, 101, 47, 196, 65, 67, 68, 114, 2, 4, 6, 84, 136, 72, 137, 69, 135, 97, 114, 101, 51, 144, 86, 194, 110, 115, 21, 47, 196, 67, 71, 85, 114, 2, 4, 6, 65, 131, 66, 130, 83, 129, 97, 114, 101, 49, 144, 84, 47, 194, 69, 114, 3, 83, 144, 88, 97, 114, 101, 52, 144, 87, 114, 47, 196, 66, 67, 70, 114, 2, 4, 6, 69, 134, 65, 133, 82, 132, 97, 114, 101, 50, 144, 85] }, suffixes: ZeroTrieSimpleAscii { store: [201, 65, 66, 67, 68, 69, 70, 71, 85, 114, 2, 4, 10, 12, 14, 16, 18, 20, 84, 128, 69, 129, 194, 65, 72, 1, 130, 131, 69, 132, 83, 133, 82, 134, 66, 135, 83, 136, 97, 114, 101, 196, 49, 50, 51, 52, 1, 2, 3, 137, 138, 139, 140] }, dense: ZeroVec([]), suffix_count: 13, delimiter: 47 }"
    );

    check_data(&data, trie.as_borrowed(), true);

    // Non-existent lookups
    let mut non_existent: BTreeMap<&str, BTreeMap<&str, usize>> = BTreeMap::new();
    non_existent.entry("en").or_default().insert("FR", 0);
    non_existent
        .entry("nonexistent")
        .or_default()
        .insert("US", 0);
    check_data(&non_existent, trie.as_borrowed(), false);
}

#[test]
fn test_empty_dense_when_no_suffix_meets_threshold() {
    // Create a dataset where NO suffix appears in enough prefixes to meet threshold.
    // Dense matrix should be empty, all lookups use sparse path.

    let mut data: BTreeMap<&str, BTreeMap<&str, usize>> = BTreeMap::new();

    // Each suffix appears in only one prefix (0% overlap)
    data.entry("a").or_default().insert("s1", 1);
    data.entry("b").or_default().insert("s2", 2);
    data.entry("c").or_default().insert("s3", 3);

    let trie = ZeroAsciiDenseSparse2dTrieOwned::try_from_btree_map_str(&data, b'/').unwrap();

    assert_eq!(
        format!("{:?}", trie.as_borrowed()),
        "ZeroAsciiDenseSparse2dTrieBorrowed { primary: ZeroTrieSimpleAscii { store: [195, 97, 98, 99, 4, 8, 47, 115, 49, 129, 47, 115, 50, 130, 47, 115, 51, 131] }, suffixes: ZeroTrieSimpleAscii { store: [115, 195, 49, 50, 51, 1, 2, 128, 129, 130] }, dense: ZeroVec([]), suffix_count: 3, delimiter: 47 }"
    );

    check_data(&data, trie.as_borrowed(), true);

    // Non-existent
    let mut non_existent: BTreeMap<&str, BTreeMap<&str, usize>> = BTreeMap::new();
    non_existent.entry("a").or_default().insert("s2", 0);
    non_existent.entry("b").or_default().insert("s3", 0);
    check_data(&non_existent, trie.as_borrowed(), false);
}

#[test]
fn test_empty_input() {
    let data: BTreeMap<&str, BTreeMap<&str, usize>> = BTreeMap::new();

    let trie = ZeroAsciiDenseSparse2dTrieOwned::try_from_btree_map_str(&data, b'/').unwrap();

    check_data(&data, trie.as_borrowed(), true);
}

#[test]
fn test_lookup_semantics_remain_correct() {
    // Verify that both dense and sparse suffixes return correct values

    let mut data: BTreeMap<&str, BTreeMap<&str, usize>> = BTreeMap::new();

    // High-frequency suffix (will be dense)
    for i in 0..10 {
        let prefix = format!("p{}", i);
        data.entry(prefix.leak() as &str)
            .or_default()
            .insert("common", 100 + i);
    }

    // Low-frequency suffix (will be sparse)
    data.entry("p0").or_default().insert("rare", 999);

    let trie = ZeroAsciiDenseSparse2dTrieOwned::try_from_btree_map_str(&data, b'/').unwrap();

    assert_eq!(
        format!("{:?}", trie.as_borrowed()),
        "ZeroAsciiDenseSparse2dTrieBorrowed { primary: ZeroTrieSimpleAscii { store: [112, 202, 48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 4, 8, 12, 16, 20, 24, 28, 32, 36, 128, 47, 144, 84, 129, 47, 144, 85, 130, 47, 144, 86, 131, 47, 144, 87, 132, 47, 144, 88, 133, 47, 144, 89, 134, 47, 144, 90, 135, 47, 144, 91, 136, 47, 144, 92, 137, 47, 144, 93] }, suffixes: ZeroTrieSimpleAscii { store: [194, 99, 114, 6, 111, 109, 109, 111, 110, 128, 97, 114, 101, 129] }, dense: ZeroVec([0, 899, 0, 65535, 0, 65535, 0, 65535, 0, 65535, 0, 65535, 0, 65535, 0, 65535, 0, 65535, 0, 65535]), suffix_count: 2, delimiter: 47 }"
    );

    check_data(&data, trie.as_borrowed(), true);

    // Non-existent combinations
    let mut non_existent: BTreeMap<&str, BTreeMap<&str, usize>> = BTreeMap::new();
    non_existent.entry("p1").or_default().insert("rare", 0);
    non_existent.entry("p99").or_default().insert("common", 0);
    check_data(&non_existent, trie.as_borrowed(), false);
}

#[test]
fn test_delimiter_validation() {
    let mut data: BTreeMap<&str, BTreeMap<&str, usize>> = BTreeMap::new();

    // Try to use delimiter in suffix
    data.entry("test").or_default().insert("bad/suffix", 1);

    let result = ZeroAsciiDenseSparse2dTrieOwned::try_from_btree_map_str(&data, b'/');
    assert!(result.is_err());

    // Try to use delimiter in prefix
    let mut data2: BTreeMap<&str, BTreeMap<&str, usize>> = BTreeMap::new();
    data2.entry("bad/prefix").or_default().insert("ok", 1);

    let result2 = ZeroAsciiDenseSparse2dTrieOwned::try_from_btree_map_str(&data2, b'/');
    assert!(result2.is_err());
}
