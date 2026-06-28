// Regression test for zero-repetition capture groups,
// which caused a panic when the Vec passed into search_slots
// contained space for the capture group which would never
// have any results.
//
// See: https://github.com/rust-lang/regex/issues/1327
#[test]
fn zero_repetition_capture_group() {
    use regex_automata::{
        dfa::onepass::DFA, util::primitives::NonMaxUsize, Anchored, Input,
    };

    let expr = DFA::new(r"(abc)(ABC){0}").unwrap();
    let s = "abcABC";
    let input = Input::new(s).span(0..s.len()).anchored(Anchored::Yes);

    // Test with 4 slots, so the whole match plus the first capture group.
    let mut cache = expr.create_cache();
    let mut slots: Vec<Option<NonMaxUsize>> = vec![None; 4];
    let pid = expr.try_search_slots(&mut cache, &input, &mut slots).unwrap();
    assert_eq!(pid, Some(regex_automata::PatternID::must(0)));
    assert_eq!(slots[0], Some(NonMaxUsize::new(0).unwrap()));
    assert_eq!(slots[1], Some(NonMaxUsize::new(3).unwrap()));
    assert_eq!(slots[2], Some(NonMaxUsize::new(0).unwrap()));
    assert_eq!(slots[3], Some(NonMaxUsize::new(3).unwrap()));

    // Test with larger slot array, which would fit the
    // zero-repetition capture group.
    slots.resize(6, None);
    let pid = expr.try_search_slots(&mut cache, &input, &mut slots).unwrap();
    assert_eq!(pid, Some(regex_automata::PatternID::must(0)));
    // First capture group should match
    assert_eq!(slots[2], Some(NonMaxUsize::new(0).unwrap()));
    assert_eq!(slots[3], Some(NonMaxUsize::new(3).unwrap()));
    // Second capture group with {0} should be None.
    assert_eq!(slots[4], None);
    assert_eq!(slots[5], None);
}

// Another regression test for the same case as
// `zero_repetition_capture_group`, but uses a simpler pattern. That
// is, a zero-repetition capture group is a red herring. The actual bug
// is simpler: it happens whenever too many slots are provided by the
// caller.
#[test]
fn too_many_slots_normal_pattern() {
    use regex_automata::{
        dfa::onepass::DFA, util::primitives::NonMaxUsize, Anchored, Input,
    };

    let expr = DFA::new(r"abc").unwrap();
    let s = "abc";
    let input = Input::new(s).span(0..s.len()).anchored(Anchored::Yes);

    let mut cache = expr.create_cache();
    let mut slots: Vec<Option<NonMaxUsize>> = vec![None; 4];
    let pid = expr.try_search_slots(&mut cache, &input, &mut slots).unwrap();
    assert_eq!(pid, Some(regex_automata::PatternID::must(0)));
    assert_eq!(slots[0], Some(NonMaxUsize::new(0).unwrap()));
    assert_eq!(slots[1], Some(NonMaxUsize::new(3).unwrap()));
}
